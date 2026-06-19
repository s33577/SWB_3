/*
* gui_screens.h
 *
 * Created on: 7 maj 2026
 * Author: macmac
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

typedef struct {
    uint16_t min_angle;
    uint16_t max_angle;
    uint16_t sweep_time_s;
} GUI_ScanConfig_t;

void GUI_Init(void);
void GUI_Task(void);
GUI_State_t GUI_GetState(void);
const GUI_ScanConfig_t *GUI_GetScanConfig(void);
void GUI_SetScanConfig(const GUI_ScanConfig_t *config);
uint8_t GUI_IsPaused(void);
uint8_t GUI_GetCalibrationIndex(void);
uint8_t GUI_TakeCalibrationChanged(void);
int16_t GUI_TakeStepperDelta(void);

// Global flags
extern volatile uint8_t gui_save_requested;

#endif /* GUI_SCREENS_H */
