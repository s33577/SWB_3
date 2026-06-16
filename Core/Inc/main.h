/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define RCC_OSC32_IN_Pin GPIO_PIN_14
#define RCC_OSC32_IN_GPIO_Port GPIOC
#define RCC_OSC32_OUT_Pin GPIO_PIN_15
#define RCC_OSC32_OUT_GPIO_Port GPIOC
#define RCC_OSC_IN_Pin GPIO_PIN_0
#define RCC_OSC_IN_GPIO_Port GPIOF
#define RCC_OSC_OUT_Pin GPIO_PIN_1
#define RCC_OSC_OUT_GPIO_Port GPIOF
#define TFT_CS_Pin GPIO_PIN_1
#define TFT_CS_GPIO_Port GPIOA
#define LPUART1_TX_Pin GPIO_PIN_2
#define LPUART1_TX_GPIO_Port GPIOA
#define LPUART1_RX_Pin GPIO_PIN_3
#define LPUART1_RX_GPIO_Port GPIOA
#define TFT_DC_Pin GPIO_PIN_0
#define TFT_DC_GPIO_Port GPIOB
#define SERVO_PWM_Pin GPIO_PIN_10
#define SERVO_PWM_GPIO_Port GPIOB
#define STEP_IN1_Pin GPIO_PIN_11
#define STEP_IN1_GPIO_Port GPIOB
#define STEP_IN2_Pin GPIO_PIN_12
#define STEP_IN2_GPIO_Port GPIOB
#define STEP_IN3_Pin GPIO_PIN_13
#define STEP_IN3_GPIO_Port GPIOB
#define STEP_IN4_Pin GPIO_PIN_14
#define STEP_IN4_GPIO_Port GPIOB
#define SONAR_ECHO_Pin GPIO_PIN_6
#define SONAR_ECHO_GPIO_Port GPIOC
#define TFT_RST_Pin GPIO_PIN_7
#define TFT_RST_GPIO_Port GPIOC
#define SONAR_TRIG_Pin GPIO_PIN_8
#define SONAR_TRIG_GPIO_Port GPIOC
#define Touch_CS_Pin GPIO_PIN_8
#define Touch_CS_GPIO_Port GPIOA
#define TOUCH_IRQ_Pin GPIO_PIN_10
#define TOUCH_IRQ_GPIO_Port GPIOA
#define TOUCH_IRQ_EXTI_IRQn EXTI15_10_IRQn
#define T_SWDIO_Pin GPIO_PIN_13
#define T_SWDIO_GPIO_Port GPIOA
#define T_SWCLK_Pin GPIO_PIN_14
#define T_SWCLK_GPIO_Port GPIOA
#define T_SWO_Pin GPIO_PIN_3
#define T_SWO_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
