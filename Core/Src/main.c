/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ili9341.h"
#include "xpt2046.h"
#include "gui_screens.h"
#include "sonar.h"
#include "servo.h"
#include "stepper.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
  uint16_t angle;
  int32_t steps;
} CalibrationPoint_t;

typedef struct {
  uint32_t magic;
  uint32_t version;
  GUI_ScanConfig_t config;
  CalibrationPoint_t calibration[3];
  uint32_t checksum;
  uint32_t reserved;
} AppFlashData_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define FLASH_CALIB_ADDR       0x0807F800U
#define FLASH_CALIB_PAGE       255U
#define APP_FLASH_MAGIC        0x33505753U
#define APP_FLASH_VERSION      1U
#define GRAPH_TOP              60U
#define GRAPH_HEIGHT           170U
#define GRAPH_WIDTH            320U
#define BAR_WIDTH              10U
#define SCAN_STEP_DEG          5U
#define SONAR_TIMEOUT_MS       45U
#define SONAR_MAX_CM           200.0f
#define SONAR_INVALID_CM       999.0f
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_tx;
DMA_HandleTypeDef hdma_spi1_rx;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

/* USER CODE BEGIN PV */
static CalibrationPoint_t calibration[3] = {
  { 0, 0 },
  { 90, 1024 },
  { 180, 2048 }
};
static int32_t pointer_step_position = 1024;

static uint8_t scan_active = 0;
static uint16_t scan_angle = 0;
static uint16_t sample_angle = 0;
static int8_t scan_direction = 1;
static uint32_t last_sample_tick = 0;
static float filtered_dist = 0.0f;
static uint8_t filter_has_value = 0;
static float closest_dist = SONAR_INVALID_CM;
static uint16_t closest_angle = 90;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */
static void App_LoadSettingsFromFlash(void);
static HAL_StatusTypeDef App_SaveSettingsToFlash(void);
static void App_StartScan(void);
static void App_RunScanTask(uint32_t now);
static void App_StopScan(void);
static void App_HandleCalibration(void);
static void App_MovePointerToAngle(uint16_t angle);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// generate simple checksum for flash data validation
static uint32_t App_Checksum(const AppFlashData_t *data) {
  uint32_t checksum = data->magic ^ data->version;
  checksum ^= data->config.min_angle;
  checksum ^= ((uint32_t)data->config.max_angle << 16);
  checksum ^= data->config.sweep_time_s;

  for (uint8_t i = 0; i < 3U; i++) {
    checksum ^= data->calibration[i].angle;
    checksum ^= (uint32_t)data->calibration[i].steps;
  }
  return checksum;
}

// package up current settings for flash storage
static void App_CopyFlashData(AppFlashData_t *data) {
  const GUI_ScanConfig_t *config = GUI_GetScanConfig();

  data->magic = APP_FLASH_MAGIC;
  data->version = APP_FLASH_VERSION;
  data->config = *config;
  data->calibration[0] = calibration[0];
  data->calibration[1] = calibration[1];
  data->calibration[2] = calibration[2];
  data->reserved = 0U;
  data->checksum = App_Checksum(data);
}

// verify flash data hasnt been corrupted
static uint8_t App_FlashDataIsValid(const AppFlashData_t *data) {
  if (data->magic != APP_FLASH_MAGIC || data->version != APP_FLASH_VERSION) return 0;
  if (data->config.max_angle > SERVO_MAX_POSITION ||
      data->config.min_angle >= data->config.max_angle) return 0;
  return (data->checksum == App_Checksum(data));
}

// try loading existing settings from memory
static void App_LoadSettingsFromFlash(void) {
  const AppFlashData_t *stored = (const AppFlashData_t *)FLASH_CALIB_ADDR;
  if (App_FlashDataIsValid(stored)) {
    GUI_SetScanConfig(&stored->config);
    calibration[0] = stored->calibration[0];
    calibration[1] = stored->calibration[1];
    calibration[2] = stored->calibration[2];
    pointer_step_position = calibration[1].steps;
  }
}

// write settings to flash
static HAL_StatusTypeDef App_SaveSettingsToFlash(void) {
  FLASH_EraseInitTypeDef erase = {0};
  AppFlashData_t data = {0};
  uint32_t page_error = 0;
  HAL_StatusTypeDef status;

  App_CopyFlashData(&data);
  HAL_FLASH_Unlock();

  erase.TypeErase = FLASH_TYPEERASE_PAGES;
  erase.Banks = FLASH_BANK_1;
  erase.Page = FLASH_CALIB_PAGE;
  erase.NbPages = 1;

  status = HAL_FLASHEx_Erase(&erase, &page_error);
  if (status == HAL_OK) {
    const uint64_t *words = (const uint64_t *)&data;
    for (uint32_t i = 0; i < (sizeof(AppFlashData_t) / 8U); i++) {
      status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, FLASH_CALIB_ADDR + (i * 8U), words[i]);
      if (status != HAL_OK) break;
    }
  }
  HAL_FLASH_Lock();
  return status;
}

static float App_AbsFloat(float value) {
  return (value < 0.0f) ? -value : value;
}

static uint16_t App_ClampAngle(uint16_t angle) {
  return (angle > SERVO_MAX_POSITION) ? SERVO_MAX_POSITION : angle;
}

static void App_SetCalibrationAngles(void) {
  const GUI_ScanConfig_t *config = GUI_GetScanConfig();
  calibration[0].angle = App_ClampAngle(config->min_angle);
  calibration[1].angle = App_ClampAngle((uint16_t)((config->min_angle + config->max_angle) / 2U));
  calibration[2].angle = App_ClampAngle(config->max_angle);
}

// map servo degrees to stepper motor steps
static int32_t App_InterpolateSteps(uint16_t angle) {
  CalibrationPoint_t left, right;
  if (angle <= calibration[1].angle) { left = calibration[0]; right = calibration[1]; } 
  else { left = calibration[1]; right = calibration[2]; }

  if (right.angle == left.angle) return left.steps;
  return left.steps + (((int32_t)(angle - left.angle) * (right.steps - left.steps)) / (int32_t)(right.angle - left.angle));
}

static void App_MovePointerToSteps(int32_t target_steps) {
  int32_t delta = target_steps - pointer_step_position;
  if (delta > 0) Stepper_Move(1, (uint32_t)delta);
  else if (delta < 0) Stepper_Move(-1, (uint32_t)(-delta));
  pointer_step_position = target_steps;
}

static void App_MovePointerToAngle(uint16_t angle) {
  App_MovePointerToSteps(App_InterpolateSteps(angle));
}

// draw a single sonar bar on screen
static void App_DrawSample(uint16_t angle, float distance_cm, uint8_t is_closest) {


  const GUI_ScanConfig_t *config = GUI_GetScanConfig();
  uint16_t range = config->max_angle - config->min_angle;
  uint16_t x = 0;
  if (range != 0U) x = (uint16_t)(((uint32_t)(angle - config->min_angle) * (GRAPH_WIDTH - BAR_WIDTH)) / range);


  if (distance_cm > SONAR_MAX_CM) distance_cm = SONAR_MAX_CM;

  uint16_t bar_height = (uint16_t)(((SONAR_MAX_CM - distance_cm) * (float)GRAPH_HEIGHT) / SONAR_MAX_CM);


  if (bar_height > GRAPH_HEIGHT) bar_height = GRAPH_HEIGHT;

  uint16_t color = is_closest ? ILI9341_RED : ILI9341_GREEN;


  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);

  ILI9341_FillRect(x, GRAPH_TOP, BAR_WIDTH, GRAPH_HEIGHT, ILI9341_BLACK);
  ILI9341_FillRect(x, (uint16_t)(GRAPH_TOP + GRAPH_HEIGHT - bar_height), BAR_WIDTH, bar_height, color);



  ILI9341_FillRect(260, 10, 50, 40, ILI9341_RED); // keep stop button visible
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
}

static uint32_t App_SamplePeriodMs(void) {
  const GUI_ScanConfig_t *config = GUI_GetScanConfig();
  uint16_t range = config->max_angle - config->min_angle;
  uint16_t samples = (range / SCAN_STEP_DEG) < 1U ? 1U : (range / SCAN_STEP_DEG);
  return ((uint32_t)config->sweep_time_s * 1000U) / samples;
}

static void App_StartScan(void) {
  App_SetCalibrationAngles();
  scan_active = 1;


  scan_angle = App_ClampAngle(GUI_GetScanConfig()->min_angle);
  sample_angle = scan_angle;

  scan_direction = 1;
  last_sample_tick = 0;
  filtered_dist = 0.0f; filter_has_value = 0;
  closest_dist = SONAR_INVALID_CM; closest_angle = scan_angle;

  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
  ILI9341_FillRect(0, GRAPH_TOP, GRAPH_WIDTH, GRAPH_HEIGHT, ILI9341_BLACK);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
}

static void App_StopScan(void) {
  scan_active = 0;
  Sonar_ResetFlag();
  Servo_SetPosition(90);
}

static void App_AdvanceScanAngle(void) {
  const GUI_ScanConfig_t *config = GUI_GetScanConfig();
  if (scan_direction > 0) {
    if ((scan_angle + SCAN_STEP_DEG) >= config->max_angle) {


      scan_angle = config->max_angle; scan_direction = -1;
      App_MovePointerToAngle(closest_angle);
      closest_dist = SONAR_INVALID_CM;
    } else scan_angle += SCAN_STEP_DEG;
  } else {
    if (scan_angle <= (config->min_angle + SCAN_STEP_DEG)) {
      scan_angle = config->min_angle; scan_direction = 1;

      App_MovePointerToAngle(closest_angle);

      closest_dist = SONAR_INVALID_CM;
    } else scan_angle -= SCAN_STEP_DEG;
  }
}

static void App_ProcessSonarSample(void) {
  float distance = Sonar_GetDist();
  uint8_t is_closest = 0;
  Sonar_ResetFlag();

  if (distance < 2.0f || distance > 400.0f) return;
  // ema filter
  if (filter_has_value != 0U && App_AbsFloat(distance - filtered_dist) > 80.0f) return;

  if (filter_has_value == 0U) { filtered_dist = distance; filter_has_value = 1; }
  else { filtered_dist = (0.7f * filtered_dist) + (0.3f * distance); }

  if (filtered_dist < closest_dist) {
    closest_dist = filtered_dist; closest_angle = sample_angle; is_closest = 1;
  }
  App_DrawSample(sample_angle, filtered_dist, is_closest);
}

static void App_RunScanTask(uint32_t now) {
  if (Sonar_IsReady()) App_ProcessSonarSample();
  else if (Sonar_HasTimedOut()) Sonar_ResetFlag();

  if (Sonar_IsBusy() != 0U) return;

  if (last_sample_tick == 0U || (now - last_sample_tick) >= App_SamplePeriodMs()) {
    Servo_SetPosition(scan_angle);
    if (Sonar_StartReading(SONAR_TIMEOUT_MS) == HAL_OK) {
      sample_angle = scan_angle; last_sample_tick = now;
      App_AdvanceScanAngle();
    }
  }
}

static void App_HandleCalibration(void) {
  uint8_t index = GUI_GetCalibrationIndex();
  int16_t delta = GUI_TakeStepperDelta();

  if (GUI_TakeCalibrationChanged() != 0U) {
    App_SetCalibrationAngles();
    Servo_SetPosition(calibration[index].angle);
    App_MovePointerToSteps(calibration[index].steps);
  }

  if (delta != 0) {
    App_MovePointerToSteps(pointer_step_position + delta);
    calibration[index].steps = pointer_step_position;
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  GUI_State_t previous_state = GUI_STATE_CONFIG;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(TOUCH_CS_GPIO_Port, TOUCH_CS_Pin, GPIO_PIN_SET);

  ILI9341_Init(&hspi1);
  XPT2046_Init(&hspi1);
  Sonar_Init(&htim3, TIM_CHANNEL_1);
  Servo_Init(&htim2, TIM_CHANNEL_3);
  Stepper_Init();
  Stepper_SetDelayMs(2);

  App_LoadSettingsFromFlash();
  GUI_Init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint32_t now = HAL_GetTick();
    GUI_State_t state;

    XPT2046_Task();
    GUI_Task();
    state = GUI_GetState();

    // handle state switch
    if (state != previous_state) {
      if (state == GUI_STATE_RUN) App_StartScan();
      else if (previous_state == GUI_STATE_RUN) App_StopScan();
      else if (state == GUI_STATE_CALIBRATE) {
        App_SetCalibrationAngles();
        Servo_SetPosition(calibration[GUI_GetCalibrationIndex()].angle);
        App_MovePointerToSteps(calibration[GUI_GetCalibrationIndex()].steps);
      }
      previous_state = state;
    }

    if (gui_save_requested != 0U) {
      App_SaveSettingsToFlash();
      gui_save_requested = 0;
    }

    // run tasks
    if (state == GUI_STATE_RUN && GUI_IsPaused() == 0U) App_RunScanTask(now);
    else if (state == GUI_STATE_CALIBRATE) App_HandleCalibration();
    else if (scan_active != 0U) App_StopScan();
  }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 169;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 19999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 169;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_IC_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_BOTHEDGE;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim3, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, TFT_CS_Pin|Touch_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, TFT_DC_Pin|STEP_IN1_Pin|STEP_IN2_Pin|STEP_IN3_Pin
                          |STEP_IN4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, TFT_RST_Pin|SONAR_TRIG_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : TFT_CS_Pin Touch_CS_Pin */
  GPIO_InitStruct.Pin = TFT_CS_Pin|Touch_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : LPUART1_TX_Pin LPUART1_RX_Pin */
  GPIO_InitStruct.Pin = LPUART1_TX_Pin|LPUART1_RX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF12_LPUART1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : TFT_DC_Pin STEP_IN1_Pin STEP_IN2_Pin STEP_IN3_Pin
                           STEP_IN4_Pin */
  GPIO_InitStruct.Pin = TFT_DC_Pin|STEP_IN1_Pin|STEP_IN2_Pin|STEP_IN3_Pin
                          |STEP_IN4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : TFT_RST_Pin SONAR_TRIG_Pin */
  GPIO_InitStruct.Pin = TFT_RST_Pin|SONAR_TRIG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : TOUCH_IRQ_Pin */
  GPIO_InitStruct.Pin = TOUCH_IRQ_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(TOUCH_IRQ_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  if (GPIO_Pin == TOUCH_IRQ_Pin) {
    XPT2046_EXTI_Callback(GPIO_Pin);
  }
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
  Sonar_IC_CaptureCallback(htim);
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  (void)file;
  (void)line;
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
