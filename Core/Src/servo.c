/*
 * servo.c
 *
 *  Created on: 14 maj 2026
 *      Author: macmac
 */

#include "servo.h"

static TIM_HandleTypeDef *servo_htim = NULL;
static uint32_t servo_channel = 0;

HAL_StatusTypeDef Servo_Init(TIM_HandleTypeDef *htim, uint32_t channel) {
	if (htim == NULL) {
		return HAL_ERROR;
	}

	servo_htim = htim;
	servo_channel = channel;

	HAL_StatusTypeDef status = HAL_TIM_PWM_Start(servo_htim, servo_channel);

	if (status == HAL_OK) {
		Servo_SetPosition(SERVO_MAX_POSITION / 2U);
	}

	return status;
}

HAL_StatusTypeDef Servo_SetPosition(uint16_t position_0_340) {
	if (servo_htim == NULL) {
		return HAL_ERROR;
	}

	if (position_0_340 > SERVO_MAX_POSITION) {
		position_0_340 = SERVO_MAX_POSITION;
	}

	uint32_t pulse_us =
	SERVO_MIN_PULSE_US
			+ ((uint32_t) position_0_340
					* (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US))
					/ SERVO_MAX_POSITION;

	__HAL_TIM_SET_COMPARE(servo_htim, servo_channel, pulse_us);

	return HAL_OK;
}
