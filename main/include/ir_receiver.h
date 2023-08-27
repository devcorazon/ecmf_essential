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


int ir_receiver_init();

#endif /* MAIN_INCLUDE_IR_RECEIVER_H_ */
