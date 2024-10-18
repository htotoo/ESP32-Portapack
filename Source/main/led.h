#include <nvs_flash.h>
#include <led_strip.h>

led_strip_handle_t led_strip;
uint8_t rgb_brightness = 50; // 0-100 %

void rgb_set(uint8_t r, uint8_t g, uint8_t b)
{
    // remap based on brightness
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

void rgb_set_by_status()
{
    bool usb = getUsbConnected();
    bool wifiSta = getWifiStaStatus();
    bool wifiAp = getWifiApClientNum() > 0;
    bool gps = gpsdata.latitude != 0 && gpsdata.longitude != 0 && gpsdata.sats_in_use > 2;
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t tmp = 0;
    const int max_options = 16;

    if (wifiSta)
        tmp |= 1;
    if (wifiAp)
        tmp |= 2;
    if (usb)
        tmp |= 4;
    if (gps)
        tmp |= 8;

    // calc the color
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

void init_rgb()
{
    if (RGB_LED_PIN < 0)
        return;

    led_strip_config_t strip_config = {
        .strip_gpio_num = RGB_LED_PIN,            // The GPIO that connected to the LED strip's data line
        .max_leds = 1,                            // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812,            // LED strip model
        .flags.invert_out = false,                // whether to invert the output signal (useful when your hardware has a level inverter)
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,    // different clock source can lead to different power consumption
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,           // whether to enable the DMA feature
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
}