/*
 * gui_screens.c
 *
 *  Created on: 7 maj 2026
 *      Author: macmac
 */


#include "gui_screens.h"
#include "ili9341.h"
#include "xpt2046.h"
#include <string.h>

/*
* Simple GUI screen:
* - Start screen: "How are you?"
* - Buttons: "great", "bad"
* - Great screen: "Then go on a motorcycle" + "back"
* - Bad screen: "buy a motorcycle" + "back"
*
* Assumption:
* - Screen logically works as 320x240
* - Touch gives g_touch_x: 0..319, g_touch_y: 0..239
 */

typedef enum
{
    GUI_SCREEN_HOME = 0,
    GUI_SCREEN_GREAT,
    GUI_SCREEN_BAD
} GUI_Screen_t;

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
} GUI_Button_t;

static GUI_Screen_t gui_current_screen = GUI_SCREEN_HOME;
static uint8_t gui_last_touch_pressed = 0;

/* Buttons for 320x240 */
static const GUI_Button_t btn_great = { 35,  125, 110, 55 };
static const GUI_Button_t btn_bad   = { 175, 125, 110, 55 };
static const GUI_Button_t btn_back  = { 105, 175, 110, 45 };

/* Colors RGB565 */
#define GUI_COLOR_BG          ILI9341_BLACK
#define GUI_COLOR_TEXT        ILI9341_WHITE
#define GUI_COLOR_BORDER      ILI9341_WHITE
#define GUI_COLOR_GREAT       ILI9341_GREEN
#define GUI_COLOR_BAD         ILI9341_RED
#define GUI_COLOR_BACK        ILI9341_BLUE
#define GUI_COLOR_PANEL       0x2104

/* =========================
 * Minimum font 5x7
 * ========================= */

static const uint8_t GLYPH_SPACE[5] = {0x00,0x00,0x00,0x00,0x00};
static const uint8_t GLYPH_UNKNOWN[5] = {0x7F,0x41,0x5D,0x41,0x7F};

static const uint8_t GLYPH_H[5] = {0x7F,0x08,0x08,0x08,0x7F};
static const uint8_t GLYPH_T[5] = {0x01,0x01,0x7F,0x01,0x01};
static const uint8_t GLYPH_Q[5] = {0x02,0x01,0x51,0x09,0x06};

static const uint8_t GLYPH_a[5] = {0x20,0x54,0x54,0x54,0x78};
static const uint8_t GLYPH_b[5] = {0x7F,0x48,0x44,0x44,0x38};
static const uint8_t GLYPH_c[5] = {0x38,0x44,0x44,0x44,0x20};
static const uint8_t GLYPH_d[5] = {0x38,0x44,0x44,0x48,0x7F};
static const uint8_t GLYPH_e[5] = {0x38,0x54,0x54,0x54,0x18};
static const uint8_t GLYPH_g[5] = {0x08,0x54,0x54,0x54,0x3C};
static const uint8_t GLYPH_h[5] = {0x7F,0x08,0x04,0x04,0x78};
static const uint8_t GLYPH_k[5] = {0x7F,0x10,0x28,0x44,0x00};
static const uint8_t GLYPH_l[5] = {0x00,0x41,0x7F,0x40,0x00};
static const uint8_t GLYPH_m[5] = {0x7C,0x04,0x18,0x04,0x78};
static const uint8_t GLYPH_n[5] = {0x7C,0x08,0x04,0x04,0x78};
static const uint8_t GLYPH_o[5] = {0x38,0x44,0x44,0x44,0x38};
static const uint8_t GLYPH_r[5] = {0x7C,0x08,0x04,0x04,0x08};
static const uint8_t GLYPH_t[5] = {0x04,0x3F,0x44,0x40,0x20};
static const uint8_t GLYPH_u[5] = {0x3C,0x40,0x40,0x20,0x7C};
static const uint8_t GLYPH_w[5] = {0x3C,0x40,0x30,0x40,0x3C};
static const uint8_t GLYPH_y[5] = {0x0C,0x50,0x50,0x50,0x3C};

static const uint8_t *GUI_GetGlyph(char c)
{
    switch (c)
    {
        case ' ': return GLYPH_SPACE;
        case '?': return GLYPH_Q;

        case 'H': return GLYPH_H;
        case 'T': return GLYPH_T;

        case 'a': return GLYPH_a;
        case 'b': return GLYPH_b;
        case 'c': return GLYPH_c;
        case 'd': return GLYPH_d;
        case 'e': return GLYPH_e;
        case 'g': return GLYPH_g;
        case 'h': return GLYPH_h;
        case 'k': return GLYPH_k;
        case 'l': return GLYPH_l;
        case 'm': return GLYPH_m;
        case 'n': return GLYPH_n;
        case 'o': return GLYPH_o;
        case 'r': return GLYPH_r;
        case 't': return GLYPH_t;
        case 'u': return GLYPH_u;
        case 'w': return GLYPH_w;
        case 'y': return GLYPH_y;

        default: return GLYPH_UNKNOWN;
    }
}

static uint16_t GUI_TextWidth(const char *text, uint8_t scale)
{
    uint16_t len = (uint16_t)strlen(text);

    if (len == 0)
        return 0;

    return (uint16_t)(len * 6 * scale - scale);
}

static void GUI_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color, uint8_t scale)
{
    const uint8_t *glyph = GUI_GetGlyph(c);

    for (uint8_t col = 0; col < 5; col++)
    {
        uint8_t line = glyph[col];

        for (uint8_t row = 0; row < 7; row++)
        {
            if (line & (1 << row))
            {
                ILI9341_FillRect(
                    x + col * scale,
                    y + row * scale,
                    scale,
                    scale,
                    color
                );
            }
        }
    }
}

static void GUI_DrawText(uint16_t x, uint16_t y, const char *text, uint16_t color, uint8_t scale)
{
    while (*text)
    {
        GUI_DrawChar(x, y, *text, color, scale);
        x += 6 * scale;
        text++;
    }
}

static void GUI_DrawTextCentered(uint16_t y, const char *text, uint16_t color, uint8_t scale)
{
    uint16_t text_w = GUI_TextWidth(text, scale);
    uint16_t x = 0;

    if (ILI9341_WIDTH > text_w)
        x = (ILI9341_WIDTH - text_w) / 2;

    GUI_DrawText(x, y, text, color, scale);
}

/* =========================
 * Drawing simple objects
 * ========================= */

static void GUI_DrawRectBorder(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    ILI9341_FillRect(x, y, w, 2, color);
    ILI9341_FillRect(x, y + h - 2, w, 2, color);
    ILI9341_FillRect(x, y, 2, h, color);
    ILI9341_FillRect(x + w - 2, y, 2, h, color);
}

static void GUI_DrawButton(const GUI_Button_t *btn,
                           const char *label,
                           uint16_t fill_color,
                           uint16_t text_color)
{
    ILI9341_FillRect(btn->x, btn->y, btn->w, btn->h, fill_color);
    GUI_DrawRectBorder(btn->x, btn->y, btn->w, btn->h, GUI_COLOR_BORDER);

    uint8_t scale = 2;
    uint16_t text_w = GUI_TextWidth(label, scale);
    uint16_t text_x = btn->x + (btn->w - text_w) / 2;
    uint16_t text_y = btn->y + (btn->h - 7 * scale) / 2;

    GUI_DrawText(text_x, text_y, label, text_color, scale);
}

static uint8_t GUI_PointInButton(uint16_t x, uint16_t y, const GUI_Button_t *btn)
{
    if (x < btn->x)
        return 0;

    if (y < btn->y)
        return 0;

    if (x >= (btn->x + btn->w))
        return 0;

    if (y >= (btn->y + btn->h))
        return 0;

    return 1;
}

/* =========================
 * Screens
 * ========================= */

static void GUI_DrawHomeScreen(void)
{
    ILI9341_FillScreen(GUI_COLOR_BG);

    GUI_DrawTextCentered(35, "How are you?", GUI_COLOR_TEXT, 3);

    GUI_DrawButton(&btn_great, "great", GUI_COLOR_GREAT, GUI_COLOR_TEXT);
    GUI_DrawButton(&btn_bad,   "bad",   GUI_COLOR_BAD,   GUI_COLOR_TEXT);
}

static void GUI_DrawGreatScreen(void)
{
    ILI9341_FillScreen(GUI_COLOR_PANEL);

    GUI_DrawTextCentered(70, "buy a motorcycle", GUI_COLOR_TEXT, 2);

    GUI_DrawButton(&btn_back, "back", GUI_COLOR_BACK, GUI_COLOR_TEXT);
}

static void GUI_DrawBadScreen(void)
{
    ILI9341_FillScreen(GUI_COLOR_PANEL);

    GUI_DrawTextCentered(80, "Then go on a motorcycle", GUI_COLOR_TEXT, 2);

    GUI_DrawButton(&btn_back, "back", GUI_COLOR_BACK, GUI_COLOR_TEXT);
}

static void GUI_DrawCurrentScreen(void)
{
    switch (gui_current_screen)
    {
        case GUI_SCREEN_HOME:
            GUI_DrawHomeScreen();
            break;

        case GUI_SCREEN_GREAT:
            GUI_DrawGreatScreen();
            break;

        case GUI_SCREEN_BAD:
            GUI_DrawBadScreen();
            break;

        default:
            gui_current_screen = GUI_SCREEN_HOME;
            GUI_DrawHomeScreen();
            break;
    }
}

static void GUI_GotoScreen(GUI_Screen_t screen)
{
    gui_current_screen = screen;
    GUI_DrawCurrentScreen();
}

/* =========================
 * GUI support
 * ========================= */

void GUI_Init(void)
{
    gui_current_screen = GUI_SCREEN_HOME;
    gui_last_touch_pressed = 0;

    GUI_DrawCurrentScreen();
}

void GUI_Task(void)
{
    uint8_t pressed_now = g_touch_pressed;

    /*
     * Reacts only to new touches.
     * This prevents multiple clicks when holding your finger down.
     */
    if (pressed_now && !gui_last_touch_pressed)
    {
        uint16_t x = g_touch_x;
        uint16_t y = g_touch_y;

        switch (gui_current_screen)
        {
            case GUI_SCREEN_HOME:
                if (GUI_PointInButton(x, y, &btn_great))
                {
                    GUI_GotoScreen(GUI_SCREEN_GREAT);
                }
                else if (GUI_PointInButton(x, y, &btn_bad))
                {
                    GUI_GotoScreen(GUI_SCREEN_BAD);
                }
                break;

            case GUI_SCREEN_GREAT:
            case GUI_SCREEN_BAD:
                if (GUI_PointInButton(x, y, &btn_back))
                {
                    GUI_GotoScreen(GUI_SCREEN_HOME);
                }
                break;

            default:
                GUI_GotoScreen(GUI_SCREEN_HOME);
                break;
        }
    }

    gui_last_touch_pressed = pressed_now;
}
