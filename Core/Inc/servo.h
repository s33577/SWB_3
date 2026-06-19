/*
 * servo.h
 *
 *  Created on: 14 maj 2026
 *      Author: macmac
 */

#ifndef SERVO_H
#define SERVO_H

#include "stm32g4xx_hal.h"
#include <stdint.h>

#define SERVO_MIN_PULSE_US   500U
#define SERVO_MAX_PULSE_US   2500U
#define SERVO_MAX_POSITION   180U

HAL_StatusTypeDef Servo_Init(TIM_HandleTypeDef *htim, uint32_t channel);

HAL_StatusTypeDef Servo_SetPosition(uint16_t position_0_340);

#endif
