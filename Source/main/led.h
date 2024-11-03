/*
 * Copyright (C) 2024 HTotoo
 *
 * This file is part of ESP32-Portapack.
 *
 * For additional license information, see the LICENSE file.
 */

#ifndef LED_FEEDBACK_H
#define LED_FEEDBACK_H

#include <nvs_flash.h>
#include <led_strip.h>

class LedFeedback
{
public:
    static void init(int pin);

    static void set_brightness(uint8_t brightness);
    static uint8_t get_brightness();

    static void rgb_set(uint8_t r, uint8_t g, uint8_t b);
    static void rgb_set_by_status(bool pp_connection, bool wifiSta, bool wifiAp, bool gps);

private:
    static led_strip_handle_t led_strip;
    static uint8_t rgb_brightness;
};

#endif