/*
 * stepper.h
 *
 *  Created on: 14 maj 2026
 *      Author: macmac
 */

#ifndef STEPPER_H
#define STEPPER_H

#include "stm32g4xx_hal.h"
#include <stdint.h>

#define STEPPER_DEFAULT_DELAY_MS  3U

void Stepper_Init(void);


void Stepper_Move(int8_t direction, uint32_t steps);

void Stepper_SetDelayMs(uint32_t delay_ms);
void Stepper_Off(void);

#endif
