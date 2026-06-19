/*
 * xpt2046.c
 *
 * Created on: 7 maj 2026
 * Author: macmac
 */

#include "xpt2046.h"
#include "ili9341.h"
#include "servo.h"
#include "stepper.h"

/*
 * XPT2046 / ADS7843 compatible touch controller.
 *
 * Assumptions:
 * - shared SPI bus with TFT and SD
 * - separate TOUCH_CS
 * - T_IRQ active low
 * - reading performed outside the ISR
 */

#define XPT2046_CMD_READ_X   0xD0
#define XPT2046_CMD_READ_Y   0x90

#define XPT2046_READ_PERIOD_MS  20

/*
 * Initial calibration.
 * These are approximate values ​​for now.
 * First, use a debugger to view g_touch_raw_x / g_touch_raw_y
 * for the screen corners and then adjust them precisely.
 */
#define TOUCH_RAW_X_MIN   200
#define TOUCH_RAW_X_MAX   3900
#define TOUCH_RAW_Y_MIN   200
#define TOUCH_RAW_Y_MAX   3900

#define TOUCH_SCREEN_W    320
#define TOUCH_SCREEN_H    240

/*
 * If the axes are swapped or inverted, change them here.
 */
#define TOUCH_SWAP_XY     0
#define TOUCH_INVERT_X    0
#define TOUCH_INVERT_Y    0

static SPI_HandleTypeDef *touch_spi = NULL;

volatile uint8_t g_touch_irq_flag = 0;
volatile uint8_t g_touch_pressed = 0;

volatile uint16_t g_touch_raw_x = 0;
volatile uint16_t g_touch_raw_y = 0;

volatile uint16_t g_touch_x = 0;
volatile uint16_t g_touch_y = 0;

static uint32_t last_touch_read_ms = 0;

/* =========================
 * GPIO / CS
 * ========================= */

static void XPT2046_Select(void) {
#ifdef TFT_CS_Pin
	HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
#endif

#ifdef SD_CS_Pin
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
#endif

	HAL_GPIO_WritePin(Touch_CS_GPIO_Port, Touch_CS_Pin, GPIO_PIN_RESET);
}

static void XPT2046_Unselect(void) {
	HAL_GPIO_WritePin(Touch_CS_GPIO_Port, Touch_CS_Pin, GPIO_PIN_SET);
}

static uint8_t XPT2046_IsPressed(void) {
	/*
	 * T_IRQ in the XPT2046 is normally active low.
	 */
	return HAL_GPIO_ReadPin(TOUCH_IRQ_GPIO_Port, TOUCH_IRQ_Pin)
			== GPIO_PIN_RESET;
}

/* =========================
 * Reading ADC XPT2046
 * ========================= */

static uint16_t XPT2046_ReadADC(uint8_t command) {
	uint8_t tx[3];
	uint8_t rx[3];

	tx[0] = command;
	tx[1] = 0x00;
	tx[2] = 0x00;

	rx[0] = 0;
	rx[1] = 0;
	rx[2] = 0;

	XPT2046_Select();

	if (HAL_SPI_TransmitReceive(touch_spi, tx, rx, 3, 100) != HAL_OK) {
		XPT2046_Unselect();
		return 0;
	}

	XPT2046_Unselect();

	/*
	 * The 12-bit result is aligned so that it needs to be shifted by 3 bits.
	 */
	return (uint16_t) ((((uint16_t) rx[1] << 8) | rx[2]) >> 3);
}

static uint16_t XPT2046_ReadADC_Avg(uint8_t command) {
	uint32_t sum = 0;

	/*
	 * 5 samples. Simple, readable, enough for first-time debugging.
	 */
	for (uint8_t i = 0; i < 5; i++) {
		sum += XPT2046_ReadADC(command);
	}

	return (uint16_t) (sum / 5);
}

/* =========================
 * RAW Mapping → Screen
 * ========================= */

static uint16_t map_u16_clamped(uint16_t value, uint16_t in_min,
		uint16_t in_max, uint16_t out_min, uint16_t out_max) {
	if (in_max == in_min)
		return out_min;

	if (value < in_min)
		value = in_min;

	if (value > in_max)
		value = in_max;

	uint32_t numerator = (uint32_t) (value - in_min)
			* (uint32_t) (out_max - out_min);
	uint32_t denominator = (uint32_t) (in_max - in_min);

	return (uint16_t) (out_min + numerator / denominator);
}

static void XPT2046_MapRawToScreen(uint16_t raw_x, uint16_t raw_y) {
	uint16_t tx;
	uint16_t ty;

#if TOUCH_SWAP_XY
    uint16_t tmp = raw_x;
    raw_x = raw_y;
    raw_y = tmp;
#endif

	tx = map_u16_clamped(raw_x,
	TOUCH_RAW_X_MIN,
	TOUCH_RAW_X_MAX, 0,
	TOUCH_SCREEN_W - 1);

	ty = map_u16_clamped(raw_y,
	TOUCH_RAW_Y_MIN,
	TOUCH_RAW_Y_MAX, 0,
	TOUCH_SCREEN_H - 1);

#if TOUCH_INVERT_X
    tx = (TOUCH_SCREEN_W - 1) - tx;
#endif

#if TOUCH_INVERT_Y
    ty = (TOUCH_SCREEN_H - 1) - ty;
#endif

	g_touch_x = tx;
	g_touch_y = ty;
}

/* =========================
 * API
 * ========================= */

void XPT2046_Init(SPI_HandleTypeDef *hspi) {
	touch_spi = hspi;

	g_touch_irq_flag = 0;
	g_touch_pressed = 0;

	g_touch_raw_x = 0;
	g_touch_raw_y = 0;
	g_touch_x = 0;
	g_touch_y = 0;

	HAL_GPIO_WritePin(Touch_CS_GPIO_Port, Touch_CS_Pin, GPIO_PIN_SET);
}

void XPT2046_EXTI_Callback(uint16_t GPIO_Pin) {
	/*
	 * In the ISR, only a flag.
	 * We don't do SPI, printf, HAL_Delay, or drawing here.
	 */
	if (GPIO_Pin == TOUCH_IRQ_Pin) {
		g_touch_irq_flag = 1;
	}
}

void XPT2046_Task(void) {
	uint32_t now = HAL_GetTick();

	/*
	 * If there is no touch and there was no interrupt, we do nothing.
	 */
	if (!g_touch_irq_flag && !XPT2046_IsPressed()) {
		g_touch_pressed = 0;
		return;
	}

	/*
	 * We limit the reading frequency.
	 * While holding your finger, we refresh the position every 20 ms.
	 */
	if ((now - last_touch_read_ms) < XPT2046_READ_PERIOD_MS) {
		return;
	}

	last_touch_read_ms = now;
	g_touch_irq_flag = 0;

	if (!XPT2046_IsPressed()) {
		g_touch_pressed = 0;
		return;
	}

	/*
	 * Reading position.
	 * Note: On some modules, X and Y may be reversed.
	 */
	uint16_t raw_x = XPT2046_ReadADC_Avg(XPT2046_CMD_READ_X);
	uint16_t raw_y = XPT2046_ReadADC_Avg(XPT2046_CMD_READ_Y);

	g_touch_raw_x = raw_x;
	g_touch_raw_y = raw_y;

	XPT2046_MapRawToScreen(raw_x, raw_y);

	g_touch_pressed = 1;

}