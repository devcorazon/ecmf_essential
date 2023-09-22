/*
 * ir_receiver.h
 *
 *  Created on: 30 juin 2023
 *      Author: youcef.benakmoume
 */

#include "system.h"

#ifndef MAIN_INCLUDE_IR_RECEIVER_H_
#define MAIN_INCLUDE_IR_RECEIVER_H_

#define IR_RESOLUTION_HZ     1000000 // 1MHz resolution, 1 tick = 1us
#define IR_RX_GPIO_NUM       2
#define IR_NEC_DECODE_MARGIN 200     // Tolerance for parsing RMT symbols into bit stream

/**
 * @brief NEC timing spec
 */
#define NEC_LEADING_CODE_DURATION_0  9000
#define NEC_LEADING_CODE_DURATION_1  4500
#define NEC_PAYLOAD_ZERO_DURATION_0  560
#define NEC_PAYLOAD_ZERO_DURATION_1  560
#define NEC_PAYLOAD_ONE_DURATION_0   560
#define NEC_PAYLOAD_ONE_DURATION_1   1690
#define NEC_REPEAT_CODE_DURATION_0   9000
#define NEC_REPEAT_CODE_DURATION_1   2250


#define BUTTON_0 0xE916                 // ON/OFF
#define BUTTON_1 0xBA45
#define BUTTON_2 0xBF40                 // Fixed Cycle
#define BUTTON_3 0xBB44                 // Imissione
#define BUTTON_4 0xB946                 // Get mode (Center)
#define BUTTON_5 0xBC43                 // Espulsione
#define BUTTON_6 0xEA15                 // Automatic Cycle
#define BUTTON_7 0xB847                 // Not configured
#define BUTTON_8 0xA55A                 // Low
#define BUTTON_9 0xE619                 // Get speed
#define BUTTON_9_LONG 0x8000E619        // Configurazione sensori
#define BUTTON_10 0xF609                // Mid
#define BUTTON_11 0xF807                // High


int ir_receiver_init();
uint32_t ir_receiver_take_button(void);

#endif /* MAIN_INCLUDE_IR_RECEIVER_H_ */
