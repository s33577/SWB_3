/*
 * sonar.h
 *
 *  Created on: 14 maj 2026
 *      Author: macmac
 */

#ifndef SONAR_H
#define SONAR_H

#include "stm32g4xx_hal.h"
#include <stdint.h>

HAL_StatusTypeDef Sonar_Init(TIM_HandleTypeDef *htim, uint32_t channel);
void Sonar_IC_CaptureCallback(TIM_HandleTypeDef *htim);

HAL_StatusTypeDef Sonar_ReadOnce(float *distance_cm, uint32_t timeout_ms);

HAL_StatusTypeDef Sonar_StartReading(uint32_t timeout_ms);
uint8_t Sonar_IsReady(void);
uint8_t Sonar_IsBusy(void);
uint8_t Sonar_HasTimedOut(void);
float Sonar_GetDist(void);

void Sonar_ResetFlag(void);

#endif
