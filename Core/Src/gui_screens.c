#include "gui_screens.h"
#include "ili9341.h"
#include "xpt2046.h"
#include "stepper.h"
#include <string.h>

typedef struct {
    uint16_t x; uint16_t y; uint16_t w; uint16_t h;
} GUI_Button_t;

static GUI_State_t current_state = GUI_STATE_CONFIG;
static uint8_t gui_last_touch_pressed = 0;
volatile uint8_t gui_save_requested = 0;

/* Buttons */
static const GUI_Button_t btn_start = { 60,  80, 200, 50 };
static const GUI_Button_t btn_calib = { 60, 150, 200, 50 };
static const GUI_Button_t btn_stop  = { 260, 10,  50, 40 }; // Small stop button in Run mode
static const GUI_Button_t btn_ccw   = { 20,  100, 80, 60 };
static const GUI_Button_t btn_cw    = { 220, 100, 80, 60 };
static const GUI_Button_t btn_save  = { 110, 180, 100, 40 };

/* Font Data */
static const uint8_t GLYPH_SPACE[5] = {0x00,0x00,0x00,0x00,0x00};
static const uint8_t GLYPH_UNKNOWN[5] = {0x7F,0x41,0x5D,0x41,0x7F};


// Minimal character set for Demo
static const uint8_t GLYPH_A[5] = {0x7E,0x11,0x11,0x11,0x7E};
static const uint8_t GLYPH_C[5] = {0x3E,0x41,0x41,0x41,0x22};
static const uint8_t GLYPH_E[5] = {0x7F,0x49,0x49,0x49,0x41};
static const uint8_t GLYPH_L[5] = {0x7F,0x40,0x40,0x40,0x40};
static const uint8_t GLYPH_O[5] = {0x3E,0x41,0x41,0x41,0x3E};
static const uint8_t GLYPH_P[5] = {0x7F,0x09,0x09,0x09,0x06};
static const uint8_t GLYPH_R[5] = {0x7F,0x09,0x19,0x29,0x46};
static const uint8_t GLYPH_S[5] = {0x46,0x49,0x49,0x49,0x31};
static const uint8_t GLYPH_T[5] = {0x01,0x01,0x7F,0x01,0x01};
static const uint8_t GLYPH_W[5] = {0x3F,0x40,0x38,0x40,0x3F};
static const uint8_t GLYPH_I[5] = {0x00,0x41,0x7F,0x41,0x00};
static const uint8_t GLYPH_B[5] = {0x7F,0x49,0x49,0x49,0x36};
static const uint8_t GLYPH_V[5] = {0x1F,0x20,0x40,0x20,0x1F};

static const uint8_t *GUI_GetGlyph(char c) {
    if(c>='a' && c<='z') c -= 32; // Uppercase it
    switch (c) {
        case ' ': return GLYPH_SPACE; case 'A': return GLYPH_A;
        case 'C': return GLYPH_C; case 'E': return GLYPH_E;
        case 'L': return GLYPH_L; case 'O': return GLYPH_O;
        case 'P': return GLYPH_P; case 'R': return GLYPH_R;
        case 'S': return GLYPH_S; case 'T': return GLYPH_T;
        case 'W': return GLYPH_W; case 'I': return GLYPH_I;
        case 'B': return GLYPH_B; case 'V': return GLYPH_V;
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

void GUI_DrawCurrentScreen(void) {
    ILI9341_FillScreen(ILI9341_BLACK);
    if (current_state == GUI_STATE_CONFIG) {
        GUI_DrawText(40, 20, "SONAR DEMONSTRATOR", ILI9341_YELLOW, 2);
        GUI_DrawButton(&btn_start, "START SCAN", ILI9341_GREEN);
        GUI_DrawButton(&btn_calib, "CALIBRATE", ILI9341_BLUE);
    } else if (current_state == GUI_STATE_RUN) {
        GUI_DrawButton(&btn_stop, "STOP", ILI9341_RED);
    } else if (current_state == GUI_STATE_CALIBRATE) {
        GUI_DrawText(20, 20, "POINTER CALIBRATION", ILI9341_YELLOW, 2);
        GUI_DrawText(40, 60, "ALIGN TO CENTER", ILI9341_WHITE, 2);
        GUI_DrawButton(&btn_ccw, "CCW", ILI9341_BLUE);
        GUI_DrawButton(&btn_cw, "CW", ILI9341_BLUE);
        GUI_DrawButton(&btn_save, "SAVE", ILI9341_GREEN);
    }
}

void GUI_Init(void) {
    current_state = GUI_STATE_CONFIG;
    gui_last_touch_pressed = 0;
    GUI_DrawCurrentScreen();
}

GUI_State_t GUI_GetState(void) {
    return current_state;
}

void GUI_Task(void) {
    uint8_t pressed_now = g_touch_pressed;
    if (pressed_now && !gui_last_touch_pressed) {
        uint16_t x = g_touch_x;
        uint16_t y = g_touch_y;

        HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
        if (current_state == GUI_STATE_CONFIG) {
            if (GUI_IsPressed(x, y, &btn_start)) {
                current_state = GUI_STATE_RUN;
                GUI_DrawCurrentScreen();
            } else if (GUI_IsPressed(x, y, &btn_calib)) {
                current_state = GUI_STATE_CALIBRATE;
                GUI_DrawCurrentScreen();
            }
        } else if (current_state == GUI_STATE_RUN) {
            if (GUI_IsPressed(x, y, &btn_stop)) {
                current_state = GUI_STATE_CONFIG;
                GUI_DrawCurrentScreen();
            }
        } else if (current_state == GUI_STATE_CALIBRATE) {
            if (GUI_IsPressed(x, y, &btn_ccw)) {
                Stepper_Move(-1, 10);
            } else if (GUI_IsPressed(x, y, &btn_cw)) {
                Stepper_Move(1, 10);
            } else if (GUI_IsPressed(x, y, &btn_save)) {
                gui_save_requested = 1; // Trigger flash write in main
                current_state = GUI_STATE_CONFIG;
                GUI_DrawCurrentScreen();
            }
        }
        HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
    }
    gui_last_touch_pressed = pressed_now;
}