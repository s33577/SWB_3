/*
 * xpt2046.h
 *
 *  Created on: 7 maj 2026
 *      Author: macmac
 */

#ifndef XPT2046_H
#define XPT2046_H

#include "main.h"
#include <stdint.h>

extern volatile uint8_t  g_touch_irq_flag;
extern volatile uint8_t  g_touch_pressed;

extern volatile uint16_t g_touch_raw_x;
extern volatile uint16_t g_touch_raw_y;

extern volatile uint16_t g_touch_x;
extern volatile uint16_t g_touch_y;

void XPT2046_Init(SPI_HandleTypeDef *hspi);
void XPT2046_Task(void);
void XPT2046_EXTI_Callback(uint16_t GPIO_Pin);

#endif
