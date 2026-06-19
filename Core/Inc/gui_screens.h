/*
 * gui_screens.h
 *
 *  Created on: 7 maj 2026
 *      Author: macmac
 */

#ifndef GUI_SCREENS_H
#define GUI_SCREENS_H

#include "main.h"
#include <stdint.h>

typedef enum {
    GUI_STATE_CONFIG = 0,
    GUI_STATE_RUN,
    GUI_STATE_CALIBRATE
} GUI_State_t;

void GUI_Init(void);
void GUI_Task(void);
GUI_State_t GUI_GetState(void);

// Global flags
extern volatile uint8_t gui_save_requested;

#endif
