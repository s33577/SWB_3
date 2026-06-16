#include "ili9341.h"

/*
 * ILI9341 test driver for STM32 HAL.
 * Assumptions:
 * - SPI in 8-bit mode
 * - CPOL = LOW
 * - CPHA = 1 EDGE
 * - MSB FIRST
 * - CS manually controlled GPIO
 *
 * Required pins for CubeMX:
 * - TFT_CS
 * - TFT_DC
 * - TFT_RST
 *
 * Optional:
 * - TOUCH_CS
 * - SD_CS
 */

uint16_t ILI9341_WIDTH = ILI9341_TFTWIDTH;
uint16_t ILI9341_HEIGHT = ILI9341_TFTHEIGHT;

static SPI_HandleTypeDef *lcd_spi = NULL;

/* =========================
 * Low-level functions
 * ========================= */

static void ILI9341_Select(void) {
	HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
}

static void ILI9341_Unselect(void) {
	HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
}

static void ILI9341_CommandMode(void) {
	HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_RESET);
}

static void ILI9341_DataMode(void) {
	HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
}

static void ILI9341_Reset(void) {
	HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_RESET);
	HAL_Delay(20);

	HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_SET);
	HAL_Delay(120);
}

static HAL_StatusTypeDef ILI9341_WriteCommand(uint8_t cmd) {
	HAL_StatusTypeDef status;

	ILI9341_CommandMode();
	ILI9341_Select();

	status = HAL_SPI_Transmit(lcd_spi, &cmd, 1, 100);

	ILI9341_Unselect();

	return status;
}

static HAL_StatusTypeDef ILI9341_WriteData(uint8_t *data, uint16_t size) {
	HAL_StatusTypeDef status;

	ILI9341_DataMode();
	ILI9341_Select();

	status = HAL_SPI_Transmit(lcd_spi, data, size, 1000);

	ILI9341_Unselect();

	return status;
}

static HAL_StatusTypeDef ILI9341_WriteData8(uint8_t data) {
	return ILI9341_WriteData(&data, 1);
}

static void ILI9341_DeactivateOtherDevices(void) {
#ifdef TOUCH_CS_Pin
	HAL_GPIO_WritePin(TOUCH_CS_GPIO_Port, TOUCH_CS_Pin, GPIO_PIN_SET);
#endif

#ifdef SD_CS_Pin
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
#endif

	HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
}

/* =========================
 * Addressing window
 * ========================= */

static void ILI9341_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1,
		uint16_t y1) {
	uint8_t data[4];

	/*
	 * 0x2A: CASET - Column Address Set
	 * Zakres X.
	 */
	ILI9341_WriteCommand(0x2A);

	data[0] = (uint8_t) (x0 >> 8);
	data[1] = (uint8_t) (x0 & 0xFF);
	data[2] = (uint8_t) (x1 >> 8);
	data[3] = (uint8_t) (x1 & 0xFF);

	ILI9341_WriteData(data, 4);

	/*
	 * 0x2B: PASET - Page Address Set
	 * Zakres Y.
	 */
	ILI9341_WriteCommand(0x2B);

	data[0] = (uint8_t) (y0 >> 8);
	data[1] = (uint8_t) (y0 & 0xFF);
	data[2] = (uint8_t) (y1 >> 8);
	data[3] = (uint8_t) (y1 & 0xFF);

	ILI9341_WriteData(data, 4);

	/*
	 * 0x2C: RAMWR - Memory Write
	 * After this command we send the pixel data.
	 */
	ILI9341_WriteCommand(0x2C);
}

/* =========================
 * Screen rotation
 * ========================= */

void ILI9341_SetRotation(uint8_t rotation) {
	ILI9341_WriteCommand(0x36); // MADCTL

	switch (rotation) {
	case 0:
		/*
		 * Mode known from your test:
		 * Logical 320x240, MADCTL as before.
		 */
		ILI9341_WriteData8(0x48);
		ILI9341_WIDTH = ILI9341_TFTWIDTH;
		ILI9341_HEIGHT = ILI9341_TFTHEIGHT;
		break;

	case 1:
		/*
		 * Standard landscape for many libraries.
		 * Yours looks suspicious so far.
		 */
		ILI9341_WriteData8(0x28);
		ILI9341_WIDTH = ILI9341_TFTWIDTH;
		ILI9341_HEIGHT = ILI9341_TFTHEIGHT;
		break;

	case 2:
		ILI9341_WriteData8(0x88);
		ILI9341_WIDTH = ILI9341_TFTWIDTH;
		ILI9341_HEIGHT = ILI9341_TFTHEIGHT;
		break;

	case 3:
		ILI9341_WriteData8(0xE8);
		ILI9341_WIDTH = ILI9341_TFTWIDTH;
		ILI9341_HEIGHT = ILI9341_TFTHEIGHT;
		break;

	default:
		ILI9341_WriteData8(0x48);
		ILI9341_WIDTH = ILI9341_TFTWIDTH;
		ILI9341_HEIGHT = ILI9341_TFTHEIGHT;
		break;
	}
}

/* =========================
 * Initialization
 * ========================= */

void ILI9341_Init(SPI_HandleTypeDef *hspi) {
	lcd_spi = hspi;

	ILI9341_DeactivateOtherDevices();

	HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_SET);

	ILI9341_Reset();

	/*
	 * Software reset
	 */
	ILI9341_WriteCommand(0x01);
	HAL_Delay(120);

	/*
	 * Display OFF
	 */
	ILI9341_WriteCommand(0x28);

	/*
	 * Initialization sequence typical of ILI9341.
	 */

	ILI9341_WriteCommand(0xCF);
	{
		uint8_t data[] = { 0x00, 0xC1, 0x30 };
		ILI9341_WriteData(data, sizeof(data));
	}

	ILI9341_WriteCommand(0xED);
	{
		uint8_t data[] = { 0x64, 0x03, 0x12, 0x81 };
		ILI9341_WriteData(data, sizeof(data));
	}

	ILI9341_WriteCommand(0xE8);
	{
		uint8_t data[] = { 0x85, 0x00, 0x78 };
		ILI9341_WriteData(data, sizeof(data));
	}

	ILI9341_WriteCommand(0xCB);
	{
		uint8_t data[] = { 0x39, 0x2C, 0x00, 0x34, 0x02 };
		ILI9341_WriteData(data, sizeof(data));
	}

	ILI9341_WriteCommand(0xF7);
	ILI9341_WriteData8(0x20);

	ILI9341_WriteCommand(0xEA);
	{
		uint8_t data[] = { 0x00, 0x00 };
		ILI9341_WriteData(data, sizeof(data));
	}

	/*
	 * Power Control 1
	 */
	ILI9341_WriteCommand(0xC0);
	ILI9341_WriteData8(0x23);

	/*
	 * Power Control 2
	 */
	ILI9341_WriteCommand(0xC1);
	ILI9341_WriteData8(0x10);

	/*
	 * VCOM Control 1
	 */
	ILI9341_WriteCommand(0xC5);
	{
		uint8_t data[] = { 0x3E, 0x28 };
		ILI9341_WriteData(data, sizeof(data));
	}

	/*
	 * VCOM Control 2
	 */
	ILI9341_WriteCommand(0xC7);
	ILI9341_WriteData8(0x86);

	/*
	 * Pixel Format Set
	 * 0x55 = 16 bit/pixel, RGB565
	 */
	ILI9341_WriteCommand(0x3A);
	ILI9341_WriteData8(0x55);

	/*
	 * Frame Rate Control
	 */
	ILI9341_WriteCommand(0xB1);
	{
		uint8_t data[] = { 0x00, 0x18 };
		ILI9341_WriteData(data, sizeof(data));
	}

	/*
	 * Display Function Control
	 */
	ILI9341_WriteCommand(0xB6);
	{
		uint8_t data[] = { 0x08, 0x82, 0x27 };
		ILI9341_WriteData(data, sizeof(data));
	}

	/*
	 * Gamma Function Disable
	 */
	ILI9341_WriteCommand(0xF2);
	ILI9341_WriteData8(0x00);

	/*
	 * Gamma Curve Selected
	 */
	ILI9341_WriteCommand(0x26);
	ILI9341_WriteData8(0x01);

	/*
	 * Positive Gamma Correction
	 */
	ILI9341_WriteCommand(0xE0);
	{
		uint8_t data[] = { 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37,
				0x07, 0x10, 0x03, 0x0E, 0x09, 0x00 };
		ILI9341_WriteData(data, sizeof(data));
	}

	/*
	 * Negative Gamma Correction
	 */
	ILI9341_WriteCommand(0xE1);
	{
		uint8_t data[] = { 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48,
				0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F };
		ILI9341_WriteData(data, sizeof(data));
	}

	/*
	 * Rotation setting.
	 * You're working with landscape, i.e. 320x240.
	 */
	ILI9341_SetRotation(0);

	/*
	 * Sleep OUT
	 */
	ILI9341_WriteCommand(0x11);
	HAL_Delay(120);

	/*
	 * Display ON
	 */
	ILI9341_WriteCommand(0x29);
	HAL_Delay(20);

	ILI9341_FillScreen(ILI9341_BLACK);
}

/* =========================
 * Basic drawing
 * ========================= */

void ILI9341_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
		uint16_t color) {
	if (lcd_spi == NULL)
		return;

	if (x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT)
		return;

	if ((x + w) > ILI9341_WIDTH)
		w = ILI9341_WIDTH - x;

	if ((y + h) > ILI9341_HEIGHT)
		h = ILI9341_HEIGHT - y;

	if (w == 0 || h == 0)
		return;

	ILI9341_SetAddressWindow(x, y, x + w - 1, y + h - 1);

	uint8_t hi = (uint8_t) (color >> 8);
	uint8_t lo = (uint8_t) (color & 0xFF);

	/*
	 * 128-byte buffer = 64 RGB565 pixels.
	 * Sending in blocks is much faster than sending pixel-by-pixel.
	 */
	uint8_t buffer[128];

	for (uint16_t i = 0; i < sizeof(buffer); i += 2) {
		buffer[i] = hi;
		buffer[i + 1] = lo;
	}

	uint32_t pixels = (uint32_t) w * (uint32_t) h;

	ILI9341_DataMode();

	/*
	 * With a common bus, we make sure that touch and SD are not active.
	 */
#ifdef TOUCH_CS_Pin
	HAL_GPIO_WritePin(TOUCH_CS_GPIO_Port, TOUCH_CS_Pin, GPIO_PIN_SET);
#endif

#ifdef SD_CS_Pin
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
#endif

	ILI9341_Select();

	while (pixels > 0) {
		uint16_t chunk_pixels;
		uint16_t chunk_bytes;

		if (pixels > 64)
			chunk_pixels = 64;
		else
			chunk_pixels = (uint16_t) pixels;

		chunk_bytes = chunk_pixels * 2;

		if (HAL_SPI_Transmit(lcd_spi, buffer, chunk_bytes, 1000) != HAL_OK) {
			ILI9341_Unselect();
			return;
		}

		pixels -= chunk_pixels;
	}

	ILI9341_Unselect();
}

void ILI9341_FillScreen(uint16_t color) {
	ILI9341_FillRect(0, 0, ILI9341_WIDTH, ILI9341_HEIGHT, color);
}

void ILI9341_FillScreenFromSonar(float var) {
	uint16_t fill_var;
	if (var >= 100) {
		fill_var = ILI9341_HEIGHT;
	} else {
		fill_var = (var * ILI9341_HEIGHT) / 100;
	}

	ILI9341_FillRect(0, 0, ILI9341_WIDTH, fill_var, ILI9341_WHITE);
	ILI9341_FillRect(0, fill_var, ILI9341_WIDTH, ILI9341_HEIGHT - fill_var, ILI9341_BLACK);
}

void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
	if (x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT)
		return;

	ILI9341_SetAddressWindow(x, y, x, y);

	uint8_t data[2];
	data[0] = (uint8_t) (color >> 8);
	data[1] = (uint8_t) (color & 0xFF);

	ILI9341_WriteData(data, 2);
}

/* =========================
 * Screen tests
 * ========================= */

void ILI9341_DrawTestScreen(void) {
	uint16_t h = ILI9341_HEIGHT / 6;

	ILI9341_FillScreen(ILI9341_BLACK);

	ILI9341_FillRect(0, 0 * h, ILI9341_WIDTH, h, ILI9341_RED);
	ILI9341_FillRect(0, 1 * h, ILI9341_WIDTH, h, ILI9341_GREEN);
	ILI9341_FillRect(0, 2 * h, ILI9341_WIDTH, h, ILI9341_BLUE);
	ILI9341_FillRect(0, 3 * h, ILI9341_WIDTH, h, ILI9341_YELLOW);
	ILI9341_FillRect(0, 4 * h, ILI9341_WIDTH, h, ILI9341_CYAN);
	ILI9341_FillRect(0, 5 * h, ILI9341_WIDTH, ILI9341_HEIGHT - 5 * h,
	ILI9341_MAGENTA);

	// horn test
	ILI9341_FillRect(0, 0, 30, 30, ILI9341_WHITE);
	ILI9341_FillRect(ILI9341_WIDTH - 30, 0, 30, 30, ILI9341_BLACK);
	ILI9341_FillRect(0, ILI9341_HEIGHT - 30, 30, 30, ILI9341_RED);
	ILI9341_FillRect(ILI9341_WIDTH - 30, ILI9341_HEIGHT - 30, 30, 30,
	ILI9341_GREEN);
}
