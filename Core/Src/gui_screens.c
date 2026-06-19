/*
 * gui_screens.c
 *
 * Created on: 7 maj 2026
 * Author: macmac
 */

#include "gui_screens.h"
#include "ili9341.h"
#include "xpt2046.h"
#include <string.h>

/*
 * Assumption:
 * - Screen logically works as 320x240
 * - Touch gives g_touch_x: 0..319, g_touch_y: 0..239
 */

typedef struct {
    uint16_t x; uint16_t y; uint16_t w; uint16_t h;
} GUI_Button_t;

static GUI_State_t current_state = GUI_STATE_CONFIG;
static GUI_ScanConfig_t scan_config = { 0, 180, 6 };
static uint8_t gui_last_touch_pressed = 0;
static uint8_t gui_paused = 0;
static uint8_t calibration_index = 0;
static uint8_t calibration_changed = 0;
static int16_t stepper_delta = 0;
volatile uint8_t gui_save_requested = 0;

/* Buttons for 320x240 */
static const GUI_Button_t btn_min_minus = { 20,  68, 45, 34 };
static const GUI_Button_t btn_min_plus  = { 75,  68, 45, 34 };
static const GUI_Button_t btn_max_minus = { 200, 68, 45, 34 };
static const GUI_Button_t btn_max_plus  = { 255, 68, 45, 34 };
static const GUI_Button_t btn_time_minus = { 95, 112, 45, 34 };
static const GUI_Button_t btn_time_plus  = { 180, 112, 45, 34 };
static const GUI_Button_t btn_start = { 60,  162, 95, 45 };
static const GUI_Button_t btn_calib = { 165, 162, 95, 45 };
static const GUI_Button_t btn_stop  = { 250, 8,  60, 36 };
static const GUI_Button_t btn_pause = { 165, 8,  78, 36 };
static const GUI_Button_t btn_ccw   = { 20,  100, 75, 50 };
static const GUI_Button_t btn_next  = { 112, 100, 95, 50 };
static const GUI_Button_t btn_cw    = { 225, 100, 75, 50 };
static const GUI_Button_t btn_save  = { 115, 178, 90, 40 };

/* Font Data */
static const uint8_t GLYPH_SPACE[5] = {0x00,0x00,0x00,0x00,0x00};
static const uint8_t GLYPH_UNKNOWN[5] = {0x7F,0x41,0x5D,0x41,0x7F};

// Minimal character set for Demo
static const uint8_t GLYPH_0[5] = {0x3E,0x45,0x49,0x51,0x3E};
static const uint8_t GLYPH_1[5] = {0x00,0x42,0x7F,0x40,0x00};
static const uint8_t GLYPH_2[5] = {0x62,0x51,0x49,0x49,0x46};
static const uint8_t GLYPH_3[5] = {0x22,0x41,0x49,0x49,0x36};
static const uint8_t GLYPH_4[5] = {0x18,0x14,0x12,0x7F,0x10};
static const uint8_t GLYPH_5[5] = {0x2F,0x49,0x49,0x49,0x31};
static const uint8_t GLYPH_6[5] = {0x3E,0x49,0x49,0x49,0x32};
static const uint8_t GLYPH_7[5] = {0x01,0x71,0x09,0x05,0x03};
static const uint8_t GLYPH_8[5] = {0x36,0x49,0x49,0x49,0x36};
static const uint8_t GLYPH_9[5] = {0x26,0x49,0x49,0x49,0x3E};
static const uint8_t GLYPH_PLUS[5] = {0x08,0x08,0x3E,0x08,0x08};
static const uint8_t GLYPH_MINUS[5] = {0x08,0x08,0x08,0x08,0x08};
static const uint8_t GLYPH_A[5] = {0x7E,0x11,0x11,0x11,0x7E};
static const uint8_t GLYPH_B[5] = {0x7F,0x49,0x49,0x49,0x36};
static const uint8_t GLYPH_C[5] = {0x3E,0x41,0x41,0x41,0x22};
static const uint8_t GLYPH_D[5] = {0x7F,0x41,0x41,0x22,0x1C};
static const uint8_t GLYPH_E[5] = {0x7F,0x49,0x49,0x49,0x41};
static const uint8_t GLYPH_F[5] = {0x7F,0x09,0x09,0x09,0x01};
static const uint8_t GLYPH_G[5] = {0x3E,0x41,0x49,0x49,0x7A};
static const uint8_t GLYPH_H[5] = {0x7F,0x08,0x08,0x08,0x7F};
static const uint8_t GLYPH_I[5] = {0x00,0x41,0x7F,0x41,0x00};
static const uint8_t GLYPH_L[5] = {0x7F,0x40,0x40,0x40,0x40};
static const uint8_t GLYPH_M[5] = {0x7F,0x02,0x0C,0x02,0x7F};
static const uint8_t GLYPH_N[5] = {0x7F,0x04,0x08,0x10,0x7F};
static const uint8_t GLYPH_O[5] = {0x3E,0x41,0x41,0x41,0x3E};
static const uint8_t GLYPH_P[5] = {0x7F,0x09,0x09,0x09,0x06};
static const uint8_t GLYPH_R[5] = {0x7F,0x09,0x19,0x29,0x46};
static const uint8_t GLYPH_S[5] = {0x46,0x49,0x49,0x49,0x31};
static const uint8_t GLYPH_T[5] = {0x01,0x01,0x7F,0x01,0x01};
static const uint8_t GLYPH_U[5] = {0x3F,0x40,0x40,0x40,0x3F};
static const uint8_t GLYPH_V[5] = {0x1F,0x20,0x40,0x20,0x1F};
static const uint8_t GLYPH_W[5] = {0x3F,0x40,0x38,0x40,0x3F};
static const uint8_t GLYPH_X[5] = {0x63,0x14,0x08,0x14,0x63};
static const uint8_t GLYPH_Y[5] = {0x07,0x08,0x70,0x08,0x07};

static const uint8_t *GUI_GetGlyph(char c) {
    if(c>='a' && c<='z') c -= 32; // Uppercase it
    switch (c) {
        case ' ': return GLYPH_SPACE; case '0': return GLYPH_0;
        case '1': return GLYPH_1; case '2': return GLYPH_2;
        case '3': return GLYPH_3; case '4': return GLYPH_4;
        case '5': return GLYPH_5; case '6': return GLYPH_6;
        case '7': return GLYPH_7; case '8': return GLYPH_8;
        case '9': return GLYPH_9; case '+': return GLYPH_PLUS;
        case '-': return GLYPH_MINUS; case 'A': return GLYPH_A;
        case 'B': return GLYPH_B; case 'C': return GLYPH_C;
        case 'D': return GLYPH_D; case 'E': return GLYPH_E;
        case 'F': return GLYPH_F; case 'G': return GLYPH_G;
        case 'H': return GLYPH_H; case 'I': return GLYPH_I;
        case 'L': return GLYPH_L; case 'M': return GLYPH_M;
        case 'N': return GLYPH_N; case 'O': return GLYPH_O;
        case 'P': return GLYPH_P; case 'R': return GLYPH_R;
        case 'S': return GLYPH_S; case 'T': return GLYPH_T;
        case 'U': return GLYPH_U; case 'V': return GLYPH_V;
        case 'W': return GLYPH_W; case 'X': return GLYPH_X;
        case 'Y': return GLYPH_Y;
        default: return GLYPH_UNKNOWN;
    }
}

static void GUI_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color, uint8_t scale) {
    const uint8_t *glyph = GUI_GetGlyph(c);
    for (uint8_t col = 0; col < 5; col++) {
        uint8_t line = glyph[col];
        for (uint8_t row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                ILI9341_FillRect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

static void GUI_DrawText(uint16_t x, uint16_t y, const char *text, uint16_t color, uint8_t scale) {
    while (*text) {
        GUI_DrawChar(x, y, *text, color, scale);
        x += 6 * scale; text++;
    }
}

static void GUI_DrawNumber(uint16_t x, uint16_t y, uint16_t value, uint16_t color, uint8_t scale) {
    char text[6];
    uint8_t pos = 0;

    if (value >= 100U) {
        text[pos++] = (char)('0' + (value / 100U));
    }
    if (value >= 10U || pos != 0U) {
        text[pos++] = (char)('0' + ((value / 10U) % 10U));
    }
    text[pos++] = (char)('0' + (value % 10U));
    text[pos] = '\0';

    GUI_DrawText(x, y, text, color, scale);
}

static void GUI_DrawButton(const GUI_Button_t *btn, const char *label, uint16_t fill_color) {
    ILI9341_FillRect(btn->x, btn->y, btn->w, btn->h, fill_color);
    ILI9341_FillRect(btn->x, btn->y, btn->w, 2, ILI9341_WHITE);
    ILI9341_FillRect(btn->x, btn->y + btn->h - 2, btn->w, 2, ILI9341_WHITE);
    ILI9341_FillRect(btn->x, btn->y, 2, btn->h, ILI9341_WHITE);
    ILI9341_FillRect(btn->x + btn->w - 2, btn->y, 2, btn->h, ILI9341_WHITE);

    uint16_t text_w = strlen(label) * 6 * 2 - 2;
    uint16_t text_x = btn->x + (btn->w - text_w) / 2;
    uint16_t text_y = btn->y + (btn->h - 14) / 2;
    GUI_DrawText(text_x, text_y, label, ILI9341_WHITE, 2);
}

static uint8_t GUI_IsPressed(uint16_t x, uint16_t y, const GUI_Button_t *btn) {
    return (x >= btn->x && x <= (btn->x + btn->w) && y >= btn->y && y <= (btn->y + btn->h));
}

static void GUI_DrawConfigValues(void) {
    ILI9341_FillRect(0, 34, 320, 118, ILI9341_BLACK);

    GUI_DrawText(20, 42, "MIN", ILI9341_YELLOW, 2);
    GUI_DrawNumber(68, 42, scan_config.min_angle, ILI9341_WHITE, 2);
    GUI_DrawText(200, 42, "MAX", ILI9341_YELLOW, 2);
    GUI_DrawNumber(248, 42, scan_config.max_angle, ILI9341_WHITE, 2);
    GUI_DrawText(82, 112, "TIME", ILI9341_YELLOW, 2);
    GUI_DrawNumber(146, 112, scan_config.sweep_time_s, ILI9341_WHITE, 2);

    GUI_DrawButton(&btn_min_minus, "-5", ILI9341_BLUE);
    GUI_DrawButton(&btn_min_plus, "+5", ILI9341_BLUE);
    GUI_DrawButton(&btn_max_minus, "-5", ILI9341_BLUE);
    GUI_DrawButton(&btn_max_plus, "+5", ILI9341_BLUE);
    GUI_DrawButton(&btn_time_minus, "-1", ILI9341_BLUE);
    GUI_DrawButton(&btn_time_plus, "+1", ILI9341_BLUE);
}

void GUI_DrawCurrentScreen(void) {
    ILI9341_FillScreen(ILI9341_BLACK);
    if (current_state == GUI_STATE_CONFIG) {
        GUI_DrawText(54, 16, "SONAR DEMO", ILI9341_YELLOW, 2);
        GUI_DrawConfigValues();
        GUI_DrawButton(&btn_start, "START", ILI9341_GREEN);
        GUI_DrawButton(&btn_calib, "CALIB", ILI9341_BLUE);
    } else if (current_state == GUI_STATE_RUN) {
        GUI_DrawText(8, 18, "RUN", ILI9341_YELLOW, 2);
        GUI_DrawButton(&btn_pause, gui_paused ? "RESUME" : "PAUSE", ILI9341_BLUE);
        GUI_DrawButton(&btn_stop, "STOP", ILI9341_RED);
    } else if (current_state == GUI_STATE_CALIBRATE) {
        GUI_DrawText(42, 18, "CALIBRATION", ILI9341_YELLOW, 2);
        if (calibration_index == 0U) {
            GUI_DrawText(105, 58, "LEFT", ILI9341_WHITE, 2);
        } else if (calibration_index == 1U) {
            GUI_DrawText(92, 58, "CENTER", ILI9341_WHITE, 2);
        } else {
            GUI_DrawText(100, 58, "RIGHT", ILI9341_WHITE, 2);
        }
        GUI_DrawButton(&btn_ccw, "CCW", ILI9341_BLUE);
        GUI_DrawButton(&btn_next, "NEXT", ILI9341_BLUE);
        GUI_DrawButton(&btn_cw, "CW", ILI9341_BLUE);
        GUI_DrawButton(&btn_save, "SAVE", ILI9341_GREEN);
    }
}

/* =========================
 * GUI support
 * ========================= */

void GUI_Init(void) {
    current_state = GUI_STATE_CONFIG;
    gui_last_touch_pressed = 0;
    gui_paused = 0;
    calibration_index = 0;
    calibration_changed = 0;
    stepper_delta = 0;
    GUI_DrawCurrentScreen();
}

GUI_State_t GUI_GetState(void) {
    return current_state;
}

const GUI_ScanConfig_t *GUI_GetScanConfig(void) {
    return &scan_config;
}

void GUI_SetScanConfig(const GUI_ScanConfig_t *config) {
    if (config == NULL) {
        return;
    }

    if (config->min_angle < config->max_angle &&
        config->max_angle <= 180U &&
        config->sweep_time_s >= 2U &&
        config->sweep_time_s <= 20U) {
        scan_config = *config;
        if (current_state == GUI_STATE_CONFIG) {
            GUI_DrawCurrentScreen();
        }
    }
}

uint8_t GUI_IsPaused(void) {
    return gui_paused;
}

uint8_t GUI_GetCalibrationIndex(void) {
    return calibration_index;
}

uint8_t GUI_TakeCalibrationChanged(void) {
    uint8_t changed = calibration_changed;
    calibration_changed = 0;
    return changed;
}

int16_t GUI_TakeStepperDelta(void) {
    int16_t delta = stepper_delta;
    stepper_delta = 0;
    return delta;
}

static void GUI_AdjustMinAngle(int16_t delta) {
    int16_t next = (int16_t)scan_config.min_angle + delta;

    if (next < 0) {
        next = 0;
    }
    if (next > (int16_t)(scan_config.max_angle - 20U)) {
        next = (int16_t)(scan_config.max_angle - 20U);
    }

    scan_config.min_angle = (uint16_t)next;
    GUI_DrawCurrentScreen();
}

static void GUI_AdjustMaxAngle(int16_t delta) {
    int16_t next = (int16_t)scan_config.max_angle + delta;

    if (next > 180) {
        next = 180;
    }
    if (next < (int16_t)(scan_config.min_angle + 20U)) {
        next = (int16_t)(scan_config.min_angle + 20U);
    }

    scan_config.max_angle = (uint16_t)next;
    GUI_DrawCurrentScreen();
}

static void GUI_AdjustTime(int16_t delta) {
    int16_t next = (int16_t)scan_config.sweep_time_s + delta;

    if (next < 2) {
        next = 2;
    }
    if (next > 20) {
        next = 20;
    }

    scan_config.sweep_time_s = (uint16_t)next;
    GUI_DrawCurrentScreen();
}

void GUI_Task(void) {
    uint8_t pressed_now = g_touch_pressed;

    /*
     * Reacts only to new touches.
     * This prevents multiple clicks when holding your finger down.
     */
    if (pressed_now && !gui_last_touch_pressed) {
        uint16_t x = g_touch_x;
        uint16_t y = g_touch_y;

        HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
        if (current_state == GUI_STATE_CONFIG) {
            if (GUI_IsPressed(x, y, &btn_min_minus)) {
                GUI_AdjustMinAngle(-5);
            } else if (GUI_IsPressed(x, y, &btn_min_plus)) {
                GUI_AdjustMinAngle(5);
            } else if (GUI_IsPressed(x, y, &btn_max_minus)) {
                GUI_AdjustMaxAngle(-5);
            } else if (GUI_IsPressed(x, y, &btn_max_plus)) {
                GUI_AdjustMaxAngle(5);
            } else if (GUI_IsPressed(x, y, &btn_time_minus)) {
                GUI_AdjustTime(-1);
            } else if (GUI_IsPressed(x, y, &btn_time_plus)) {
                GUI_AdjustTime(1);
            } else if (GUI_IsPressed(x, y, &btn_start)) {
                gui_paused = 0;
                current_state = GUI_STATE_RUN;
                GUI_DrawCurrentScreen();
            } else if (GUI_IsPressed(x, y, &btn_calib)) {
                calibration_index = 0;
                calibration_changed = 1;
                current_state = GUI_STATE_CALIBRATE;
                GUI_DrawCurrentScreen();
            }
        } else if (current_state == GUI_STATE_RUN) {
            if (GUI_IsPressed(x, y, &btn_stop)) {
                current_state = GUI_STATE_CONFIG;
                GUI_DrawCurrentScreen();
            } else if (GUI_IsPressed(x, y, &btn_pause)) {
                gui_paused = !gui_paused;
                GUI_DrawCurrentScreen();
            }
        } else if (current_state == GUI_STATE_CALIBRATE) {
            if (GUI_IsPressed(x, y, &btn_ccw)) {
                stepper_delta -= 10;
            } else if (GUI_IsPressed(x, y, &btn_cw)) {
                stepper_delta += 10;
            } else if (GUI_IsPressed(x, y, &btn_next)) {
                calibration_index = (uint8_t)((calibration_index + 1U) % 3U);
                calibration_changed = 1;
                GUI_DrawCurrentScreen();
            } else if (GUI_IsPressed(x, y, &btn_save)) {
                gui_save_requested = 1; // main owns the flash write
                current_state = GUI_STATE_CONFIG;
                GUI_DrawCurrentScreen();
            }
        }
        HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
    }
    gui_last_touch_pressed = pressed_now;
}
