
#include "led.h"

led_strip_handle_t LedFeedback::led_strip;
uint8_t LedFeedback::rgb_brightness = 50; // 0-100 %

void LedFeedback::init(int pin)
{
    if (pin < 0)
        return;

    led_strip_config_t strip_config = {
        .strip_gpio_num = pin,
        .max_leds = 1,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags{false}};

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 0,
        .flags{false}};
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
}

void LedFeedback::set_brightness(uint8_t brightness)
{
    rgb_brightness = brightness;
}

uint8_t LedFeedback::get_brightness()
{
    return rgb_brightness;
}

void LedFeedback::rgb_set(uint8_t r, uint8_t g, uint8_t b)
{
    r = r * rgb_brightness / 100;
    g = g * rgb_brightness / 100;
    b = b * rgb_brightness / 100;

    if (r > 0 || b > 0 || g > 0)
    {
        led_strip_set_pixel(led_strip, 0, r, g, b);
        led_strip_refresh(led_strip);
    }
    else
    {
        led_strip_clear(led_strip);
    }
}

void LedFeedback::rgb_set_by_status(bool pp_connection, bool wifiSta, bool wifiAp, bool gps)
{
    // bool usb = getUsbConnected() | i2p_pp_conn_state;
    // bool wifiSta = getWifiStaStatus();
    // bool wifiAp = getWifiApClientNum() > 0;
    // bool gps = gpsdata.latitude != 0 && gpsdata.longitude != 0 && gpsdata.sats_in_use > 2;
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t tmp = 0;
    const int max_options = 16;

    if (wifiSta)
        tmp |= 1;
    if (wifiAp)
        tmp |= 2;
    if (pp_connection)
        tmp |= 4;
    if (gps)
        tmp |= 8;

    int hue = ((int)tmp * 360) / max_options;
    if (hue >= 0 && hue < 60)
    {
        r = 255;
        g = hue * 255 / 60;
        b = 0;
    }
    else if (hue >= 60 && hue < 120)
    {
        r = 255 - (hue - 60) * 255 / 60;
        g = 255;
        b = 0;
    }
    else if (hue >= 120 && hue < 180)
    {
        r = 0;
        g = 255;
        b = (hue - 120) * 255 / 60;
    }
    else if (hue >= 180 && hue < 240)
    {
        r = 0;
        g = 255 - (hue - 180) * 255 / 60;
        b = 255;
    }
    else if (hue >= 240 && hue < 300)
    {
        r = (hue - 240) * 255 / 60;
        g = 0;
        b = 255;
    }
    else
    {
        r = 255;
        g = 0;
        b = 255 - (hue - 300) * 255 / 60;
    }

    if (tmp == max_options - 1)
    {
        r = 255;
        g = 255;
        b = 255;
    }
    rgb_set(r, g, b);
}
