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


void Sonar_StartReading(void);
uint8_t Sonar_IsReady(void);
float Sonar_GetDist(void);

void Sonar_ResetFlag(void);

#endif
