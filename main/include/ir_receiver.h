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


#define BUTTON_0               0xE916
#define BUTTON_1               0xBA45
#define BUTTON_1_LONG	       0x8000BA45
#define BUTTON_2               0xBF40
#define BUTTON_3               0xBB44
#define BUTTON_4               0xB946
#define BUTTON_5               0xBC43
#define BUTTON_6               0xEA15
#define BUTTON_7               0xB847
#define BUTTON_7_LONG		   0x8000B847
#define BUTTON_8               0xA55A
#define BUTTON_8_LONG		   0x8000A55A
#define BUTTON_9               0xE619
#define BUTTON_9_LONG          0x8000E619 //0x7F80 ( for test )
#define BUTTON_10              0xF609
#define BUTTON_10_LONG		   0x8000F609
#define BUTTON_11              0xF807
#define BUTTON_11_LONG         0x8000F807


int ir_receiver_init();
uint32_t ir_receiver_take_button(void);

#endif /* MAIN_INCLUDE_IR_RECEIVER_H_ */
