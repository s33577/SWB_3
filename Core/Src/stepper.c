/*
 * stepper.c
 *
 *  Created on: 14 maj 2026
 *      Author: macmac
 */

#include "stepper.h"
#include "main.h"

static uint32_t stepper_delay_ms = STEPPER_DEFAULT_DELAY_MS;
static uint8_t stepper_index = 0;

static const uint8_t stepper_sequence[8][4] = { { 1, 0, 0, 0 }, { 1, 1, 0, 0 },
		{ 0, 1, 0, 0 }, { 0, 1, 1, 0 }, { 0, 0, 1, 0 }, { 0, 0, 1, 1 }, { 0, 0,
				0, 1 }, { 1, 0, 0, 1 } };

static void Stepper_Write(uint8_t index) {
	HAL_GPIO_WritePin(STEP_IN1_GPIO_Port, STEP_IN1_Pin,
			stepper_sequence[index][0] ? GPIO_PIN_SET : GPIO_PIN_RESET);

	HAL_GPIO_WritePin(STEP_IN2_GPIO_Port, STEP_IN2_Pin,
			stepper_sequence[index][1] ? GPIO_PIN_SET : GPIO_PIN_RESET);

	HAL_GPIO_WritePin(STEP_IN3_GPIO_Port, STEP_IN3_Pin,
			stepper_sequence[index][2] ? GPIO_PIN_SET : GPIO_PIN_RESET);

	HAL_GPIO_WritePin(STEP_IN4_GPIO_Port, STEP_IN4_Pin,
			stepper_sequence[index][3] ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void Stepper_Init(void) {
	Stepper_Off();
}

void Stepper_SetDelayMs(uint32_t delay_ms) {
	if (delay_ms == 0U) {
		delay_ms = 1U;
	}

	stepper_delay_ms = delay_ms;
}

void Stepper_Move(int8_t direction, uint32_t steps) {
	if (direction == 0 || steps == 0U) {
		return;
	}

	for (uint32_t i = 0; i < steps; i++) {
		if (direction > 0) {
			stepper_index = (stepper_index + 1U) & 0x07U;
		} else {
			stepper_index = (stepper_index + 7U) & 0x07U;
		}

		Stepper_Write(stepper_index);
		HAL_Delay(stepper_delay_ms);
	}

	Stepper_Off();
}

void Stepper_Off(void) {
	HAL_GPIO_WritePin(STEP_IN1_GPIO_Port, STEP_IN1_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(STEP_IN2_GPIO_Port, STEP_IN2_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(STEP_IN3_GPIO_Port, STEP_IN3_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(STEP_IN4_GPIO_Port, STEP_IN4_Pin, GPIO_PIN_RESET);
}
