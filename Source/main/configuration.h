/*
 * Copyright (C) 2024 HTotoo
 *
 * This file is part of ESP32-Portapack.
 *
 * For additional license information, see the LICENSE file.
 */

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "nvs_flash.h"
#include "string.h"
#include "esp_log.h"

// orientation variables
extern int16_t orientationXMin;
extern int16_t orientationYMin;
extern int16_t orientationZMin;
extern int16_t orientationXMax;
extern int16_t orientationYMax;
extern int16_t orientationZMax;

extern float declinationAngle;

// misc
extern uint8_t rgb_brightness;
extern uint32_t gps_baud;

#if __cplusplus
extern "C"
{
#endif

    void load_config_orientation();
    void save_config_orientation();
    void reset_orientation_calibration();
    void load_config_misc(); // led brightness
    void save_config_misc();

#if __cplusplus
}
#endif

#endif