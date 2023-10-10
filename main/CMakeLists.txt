idf_component_register(
					SRCS 
						"main.c"
						"hardware/system.c"
						"hardware/storage.c"
						"hardware/test.c"
						"hardware/sensor.c"
						"hardware/rgb_led.c"
						"hardware/fan.c"
						"hardware/ir_receiver.c"
                    	"driver/sht4x/sht4x.c"
                    	"driver/sgp40/sgp40.c" "driver/sgp40/sensirion_gas_index_algorithm.c"
                    	"driver/ltr303/ltr303.c"
                    	"driver/ktd2027/ktd2027.c"
                    	"blufi/blufi.c"
                    	"blufi/blufi_init.c"
                    	"blufi/blufi_security.c"
                    	"feature/controller.c"
                    	"feature/user_experience.c"
                    	"feature/statistic.c"
     	

                    INCLUDE_DIRS 
                    	"include/"
                    	"driver/sht4x/"
                    	"driver/sgp40/"
                    	"driver/ltr303/"
                    	"driver/ktd2027/"
)
