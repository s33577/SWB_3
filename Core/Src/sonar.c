/*
 * sonar.c
 *
 *  Created on: 14 maj 2026
 *      Author: macmac
 */

#include "sonar.h"
#include "main.h"

static TIM_HandleTypeDef *sonar_htim = NULL;
static uint32_t sonar_channel = 0;

static volatile uint32_t sonar_rise_us = 0;
static volatile uint32_t sonar_width_us = 0;
static volatile uint8_t sonar_done = 0;
static volatile uint8_t sonar_waiting = 0;

static void Sonar_DWT_Init(void) {
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void Sonar_DelayUs(uint32_t us) {
	uint32_t cycles = (HAL_RCC_GetHCLKFreq() / 1000000U) * us;
	uint32_t start = DWT->CYCCNT;

	while ((DWT->CYCCNT - start) < cycles) {
		;
	}
}

static void Sonar_TriggerPulse(void) {
	HAL_GPIO_WritePin(SONAR_TRIG_GPIO_Port, SONAR_TRIG_Pin, GPIO_PIN_RESET);
	Sonar_DelayUs(2);

	HAL_GPIO_WritePin(SONAR_TRIG_GPIO_Port, SONAR_TRIG_Pin, GPIO_PIN_SET);
	Sonar_DelayUs(10);

	HAL_GPIO_WritePin(SONAR_TRIG_GPIO_Port, SONAR_TRIG_Pin, GPIO_PIN_RESET);
}

HAL_StatusTypeDef Sonar_Init(TIM_HandleTypeDef *htim, uint32_t channel) {
	if (htim == NULL) {
		return HAL_ERROR;
	}

	sonar_htim = htim;
	sonar_channel = channel;

	Sonar_DWT_Init();

	HAL_GPIO_WritePin(SONAR_TRIG_GPIO_Port, SONAR_TRIG_Pin, GPIO_PIN_RESET);

	return HAL_TIM_IC_Start_IT(sonar_htim, sonar_channel);
}

HAL_StatusTypeDef Sonar_ReadOnce(float *distance_cm, uint32_t timeout_ms) {
	if (sonar_htim == NULL || distance_cm == NULL) {
		return HAL_ERROR;
	}

	sonar_done = 0;
	sonar_waiting = 1;
	sonar_width_us = 0;
	sonar_rise_us = 0;

	__HAL_TIM_SET_COUNTER(sonar_htim, 0);

	Sonar_TriggerPulse();

	uint32_t start_tick = HAL_GetTick();

	while (sonar_done == 0U) {
		if ((HAL_GetTick() - start_tick) >= timeout_ms) {
			sonar_waiting = 0;
			return HAL_TIMEOUT;
		}
	}

	sonar_waiting = 0;


	*distance_cm = (float) sonar_width_us / 58.0f;

	return HAL_OK;
}

void Sonar_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
	if (htim == NULL || sonar_htim == NULL) {
		return;
	}

	if (htim->Instance != sonar_htim->Instance) {
		return;
	}

	if (htim->Channel != HAL_TIM_ACTIVE_CHANNEL_1) {
		return;
	}

	if (sonar_waiting == 0U) {
		return;
	}

	uint32_t captured = HAL_TIM_ReadCapturedValue(htim, sonar_channel);

	if (HAL_GPIO_ReadPin(SONAR_ECHO_GPIO_Port, SONAR_ECHO_Pin)
			== GPIO_PIN_SET) {
		sonar_rise_us = captured;
	} else {
		if (captured >= sonar_rise_us) {
			sonar_width_us = captured - sonar_rise_us;
		} else {
			sonar_width_us = (65536U - sonar_rise_us) + captured;
		}

		sonar_done = 1;
	}
}

uint8_t Sonar_IsReady(void) {
    return sonar_done;
}

float Sonar_GetDist(void) {
    return (float)sonar_width_us / 58.0f;
}

void Sonar_StartReading(void) {
    sonar_done = 0;
    sonar_waiting = 1;
    sonar_width_us = 0;
    sonar_rise_us = 0;
    __HAL_TIM_SET_COUNTER(sonar_htim, 0);
    Sonar_TriggerPulse();
}

void Sonar_ResetFlag(void) {
    sonar_done = 0;
}
