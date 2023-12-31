/*
 * ota.c
 *
 *  Created on: 10 oct. 2023
 *      Author: youcef.benakmoume
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include <string.h>
#include <inttypes.h>

#include "blufi.h"
#include "blufi_internal.h"
#include "storage.h"

#include "esp_event.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"


//const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
//const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

static int validate_image_header(esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL) {
        return -1;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
    	printf("Running firmware version: %s", running_app_info.version);
    }

#ifndef CONFIG_EXAMPLE_SKIP_VERSION_CHECK
    if (memcmp(new_app_info->version, running_app_info.version, sizeof(new_app_info->version)) == 0) {
    	printf("Current running version is the same as a new. We will not continue the update.");
        return -1;
    }
#endif

#ifdef CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK
    /**
     * Secure version check from firmware image header prevents subsequent download and flash write of
     * entire firmware image. However this is optional because it is also taken care in API
     * esp_https_ota_finish at the end of OTA update procedure.
     */
    const uint32_t hw_sec_version = esp_efuse_read_secure_version();
    if (new_app_info->secure_version < hw_sec_version) {
    	printf("New firmware security version is less than eFuse programmed, %"PRIu32" < %"PRIu32, new_app_info->secure_version, hw_sec_version);
        return ESP_FAIL;
    }
#endif

    return 0;
}

static esp_err_t _http_client_init_cb(esp_http_client_handle_t http_client)
{
    esp_err_t err = ESP_OK;
    /* Uncomment to add custom headers to HTTP request */
    // err = esp_http_client_set_header(http_client, "Custom-Header", "Value");
    return err;
}

void ota_task(void *pvParameter)
{
	printf("Starting OTA ");

	uint8_t ota_url[OTA_URL_SIZE + 1] = { 0 };  // Assuming you have a define for OTA_URL_MAX_SIZE
	get_ota_url(ota_url);

    esp_err_t ota_finish_err = ESP_OK;
    esp_http_client_config_t config = {
        .url = (char *)ota_url,
		.cert_pem = NULL,
       // .cert_pem = (char *)"",  // add certificate here!
        .timeout_ms = 5000,
        .keep_alive_enable = true,
		.skip_cert_common_name_check = true,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
        .http_client_init_cb = _http_client_init_cb, // Register a callback to be invoked after esp_http_client is initialized
#ifdef CONFIG_EXAMPLE_ENABLE_PARTIAL_HTTP_DOWNLOAD
        .partial_http_download = true,
        .max_http_request_size = CONFIG_EXAMPLE_HTTP_REQUEST_SIZE,
#endif
    };

    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        printf("ESP HTTPS OTA Begin failed");
        vTaskDelete(NULL);
    }

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK) {
    	printf("esp_https_ota_read_img_desc failed");
        goto ota_end;
    }
    err = validate_image_header(&app_desc);
    if (err != ESP_OK) {
    	printf("image header verification failed");
        goto ota_end;
    }

    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        // esp_https_ota_perform returns after every read operation which gives user the ability to
        // monitor the status of OTA upgrade by calling esp_https_ota_get_image_len_read, which gives length of image
        // data read so far.
        printf("Image bytes read: %d\n", esp_https_ota_get_image_len_read(https_ota_handle));
    }

    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true) {
        // the OTA image was not completely received and user can customise the response to this situation.
    	printf("Complete data was not received.");
    } else {
        ota_finish_err = esp_https_ota_finish(https_ota_handle);
        if ((err == ESP_OK) && (ota_finish_err == ESP_OK)) {
        	printf("ESP_HTTPS_OTA upgrade successful. Rebooting ...");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            esp_restart();
        } else {
            if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
            	printf("Image validation failed, image is corrupted");
            }
            printf("ESP_HTTPS_OTA upgrade failed 0x%x", ota_finish_err);
            vTaskDelete(NULL);
        }
    }

ota_end:
    esp_https_ota_abort(https_ota_handle);
    printf("ESP_HTTPS_OTA upgrade failed");
    vTaskDelete(NULL);
}

int blufi_ota_start(void){
    BaseType_t task_created = xTaskCreate(ota_task, "OTA task ", OTA_TASK_STACK_SIZE, NULL, OTA_TASK_PRIORITY, NULL);

    return task_created == pdPASS ? 0 : -1;
}

