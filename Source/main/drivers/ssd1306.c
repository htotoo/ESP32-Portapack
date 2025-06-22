/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Eric Gionet (gionet.c.eric@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file ssd1306.c
 *
 * ESP-IDF driver for SSD1306 display panel
 *
 * Ported from esp-open-rtos
 *
 * Copyright (c) 2024 Eric Gionet (gionet.c.eric@gmail.com)
 *
 * MIT Licensed as described in the file LICENSE
 */

#include "ssd1306.h"
#include "../display/font_latin_8x8.h"
#include <string.h>
#include <esp_log.h>
#include <esp_check.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Following definitions are borrowed from
// http://robotcantalk.blogspot.com/2015/03/interfacing-arduino-with-ssd1306-driven.html
// https://github.com/nopnop2002/esp-idf-ssd1306/tree/master/components/ssd1306

/* Control byte for i2c
Co : bit 8 : Continuation Bit
 * 1 = no-continuation (only one byte to follow)
 * 0 = the controller should expect a stream of bytes.
D/C# : bit 7 : Data/Command Select bit
 * 1 = the next byte or byte stream will be Data.
 * 0 = a Command byte or byte stream will be coming up next.
 Bits 6-0 will be all zeros.
Usage:
0x80 : Single Command byte
0x00 : Command Stream
0xC0 : Single Data byte
0x40 : Data Stream
*/
#define SSD1306_CONTROL_BYTE_CMD_SINGLE 0x80   // Continuation bit=1, D/C=0; 1000 0000
#define SSD1306_CONTROL_BYTE_CMD_STREAM 0x00   // Continuation bit=0, D/C=0; 0000 0000
#define SSD1306_CONTROL_BYTE_DATA_SINGLE 0xC0  // Continuation bit=1, D/C=1; 1100 0000
#define SSD1306_CONTROL_BYTE_DATA_STREAM 0x40  // Continuation bit=0, D/C=1; 0100 0000

// Fundamental commands (pg.28)
#define SSD1306_CMD_SET_CONTRAST 0x81      // Set Contrast Control, Double byte command to select 1 to 256 contrast steps, increases as the value increases
#define SSD1306_CMD_DISPLAY_RAM 0xA4       // Entire Display ON, Output ignores RAM content
#define SSD1306_CMD_DISPLAY_ALLON 0xA5     // Resume to RAM content display, Output follows RAM content
#define SSD1306_CMD_DISPLAY_NORMAL 0xA6    // Normal display, 0 in RAM: OFF in display panel, 1 in RAM: ON in display panel
#define SSD1306_CMD_DISPLAY_INVERTED 0xA7  // Inverse display, 0 in RAM: ON in display panel, 1 in RAM: OFF in display panel
#define SSD1306_CMD_DISPLAY_OFF 0xAE       // Display OFF (sleep mode)
#define SSD1306_CMD_DISPLAY_ON 0xAF        // Display ON in normal mode

// Addressing Command Table (pg.30)
#define SSD1306_CMD_SET_MEMORY_ADDR_MODE 0x20  // Set Memory Addressing Mode
#define SSD1306_CMD_SET_HORI_ADDR_MODE 0x00    // Horizontal Addressing Mode
#define SSD1306_CMD_SET_VERT_ADDR_MODE 0x01    // Vertical Addressing Mode
#define SSD1306_CMD_SET_PAGE_ADDR_MODE 0x02    // Page Addressing Mode
#define SSD1306_CMD_SET_COLUMN_RANGE 0x21      // can be used only in HORZ/VERT mode - follow with 0x00 and 0x7F = COL127
#define SSD1306_CMD_SET_PAGE_RANGE 0x22        // can be used only in HORZ/VERT mode - follow with 0x00 and 0x07 = PAGE7

// Hardware Config (pg.31)
#define SSD1306_CMD_SET_DISPLAY_START_LINE 0x40
#define SSD1306_CMD_SET_SEGMENT_REMAP_0 0xA0  // Set Segment Re-map, X[0]=0b column address 0 is mapped to SEG0
#define SSD1306_CMD_SET_SEGMENT_REMAP_1 0xA1  // Set Segment Re-map, X[0]=1b: column address 127 is mapped to SEG0
#define SSD1306_CMD_SET_MUX_RATIO 0xA8        // Set MUX ratio to N+1 MUX, N=A[5:0] : from 32MUX, 64MUX, and 128MUX
#define SSD1306_CMD_SET_COM_SCAN_MODE 0xC8
#define SSD1306_CMD_SET_DISPLAY_OFFSET 0xD3  // follow with 0x00
#define SSD1306_CMD_SET_COM_PIN_MAP 0xDA     // Set COM Pins Hardware Configuration,
                                             // A[4]=0b, Sequential COM pin configuration, A[4]=1b(RESET), Alternative COM pin configuration
                                             // A[5]=0b(RESET), Disable COM Left/Right remap, A[5]=1b, Enable COM Left/Right remap
#define SSD1306_CMD_NOP 0xE3                 // No operation

// Timing and Driving Scheme (pg.32)
#define SSD1306_CMD_SET_DISPLAY_CLK_DIV 0xD5  // follow with 0x80
#define SSD1306_CMD_SET_PRECHARGE 0xD9        // follow with 0xF1
#define SSD1306_CMD_SET_VCOMH_DESELCT 0xDB    // follow with 0x30

// Charge Pump (pg.62)
#define SSD1306_CMD_SET_CHARGE_PUMP 0x8D  // follow with 0x14

// Scrolling Command
#define SSD1306_CMD_HORIZONTAL_RIGHT 0x26
#define SSD1306_CMD_HORIZONTAL_LEFT 0x27
#define SSD1306_CMD_CONTINUOUS_SCROLL 0x29
#define SSD1306_CMD_DEACTIVE_SCROLL 0x2E
#define SSD1306_CMD_ACTIVE_SCROLL 0x2F
#define SSD1306_CMD_VERTICAL 0xA3

#define SSD1306_TEXTBOX_DISPLAY_MAX_LEN 50
#define SSD1306_TEXT_DISPLAY_MAX_LEN 18
#define SSD1306_TEXT_X2_DISPLAY_MAX_LEN 8
#define SSD1306_TEXT_X3_DISPLAY_MAX_LEN 5

/*
 * macro definitions
 */
#define ESP_ARG_CHECK(VAL)                      \
    do {                                        \
        if (!(VAL)) return ESP_ERR_INVALID_ARG; \
    } while (0)
#define PACK8 __attribute__((aligned(__alignof__(uint8_t)), packed))

/*
 * static constant declarations
 */
static const char* TAG = "ssd1306";

/**
 * @brief SSD1306 panel properties for each display panel size supported.
 */
static const ssd1306_panel_t ssd1306_panel_properties[] = {
    {.panel_size = SSD1306_PANEL_128x32, .width = SSD1306_PANEL_128x32_WIDTH, .height = SSD1306_PANEL_128x32_HEIGHT, .pages = SSD1306_PAGE_128x32_SIZE},
    {.panel_size = SSD1306_PANEL_128x64, .width = SSD1306_PANEL_128x64_WIDTH, .height = SSD1306_PANEL_128x64_HEIGHT, .pages = SSD1306_PAGE_128x64_SIZE},
    {.panel_size = SSD1306_PANEL_128x128, .width = SSD1306_PANEL_128x128_WIDTH, .height = SSD1306_PANEL_128x128_HEIGHT, .pages = SSD1306_PAGE_128x128_SIZE}};

typedef union ssd1306_out_column_t {
    uint32_t u32;
    uint8_t u8[4];
} PACK8 ssd1306_out_column_t;

/**
 * @brief SSD1306 I2C write transaction.
 *
 * @param handle SSD1306 device handle.
 * @param buffer Buffer to write for write transaction.
 * @param size Length of buffer to write for write transaction.
 * @return esp_err_t ESP_OK on success.
 */
static inline esp_err_t ssd1306_i2c_write(ssd1306_handle_t handle, const uint8_t* buffer, const uint8_t size) {
    /* validate arguments */
    ESP_ARG_CHECK(handle);

    /* attempt i2c write transaction */
    i2c_dev_write(&handle->i2c_handle, 0, 0, buffer, size);
    // ESP_RETURN_ON_ERROR(i2c_master_transmit(handle->i2c_handle, buffer, size, I2C_XFR_TIMEOUT_MS), TAG, "i2c_master_transmit, i2c write failed");

    return ESP_OK;
}

esp_err_t ssd1306_load_bitmap_font(const uint8_t* font, int encoding, uint8_t* bitmap, ssd1306_bdf_font_t* const bdf_font) {
    ESP_LOGI(TAG, "encoding=%d", encoding);
    int index = 2;
    while (1) {
        ESP_LOGD(TAG, "font[%d]=%d size=%d", index, font[index], font[index + 6]);
        if (font[index + 6] == 0) break;
        if (font[index] == encoding) {
            bdf_font->encoding = font[index];
            bdf_font->width = font[index + 1];
            bdf_font->bbw = font[index + 2];
            bdf_font->bbh = font[index + 3];
            bdf_font->bbx = font[index + 4];
            bdf_font->bby = font[index + 5];
            bdf_font->num_data = font[index + 6];
            bdf_font->y_start = font[index + 7];
            bdf_font->y_end = font[index + 8];
            for (int i = 0; i < font[index + 6]; i++) {
                ESP_LOGD(TAG, "font[%d]=0x%x", index + 9 + i, font[index + 9 + i]);
                bitmap[i] = font[index + 9 + i];
            }
            return ESP_OK;
        }
        index = index + font[index + 6] + 9;
        ;
    }  // end while
    return ESP_ERR_NOT_FOUND;
}

esp_err_t ssd1306_display_bdf_text(ssd1306_handle_t handle, const uint8_t* font, const char* text, int xpos, int ypos) {
    /* validate parameters */
    ESP_ARG_CHECK(handle);

    if (strnlen(text, SSD1306_TEXT_DISPLAY_MAX_LEN + 1) > SSD1306_TEXT_DISPLAY_MAX_LEN) return ESP_ERR_INVALID_SIZE;

    int fontboundingbox_width = font[0];
    int fontboundingbox_height = font[1];
    size_t bitmap_size = ((fontboundingbox_width + 7) / 8) * fontboundingbox_height;
    ESP_LOGD(TAG, "fontboundingbox_width=%d fontboundingbox_height=%d bitmap_size=%d",
             fontboundingbox_width, fontboundingbox_height, bitmap_size);
    uint8_t* bitmap = malloc(bitmap_size);
    if (bitmap == NULL) {
        ESP_LOGE(TAG, "malloc fail");
        return ESP_ERR_NO_MEM;
    }
    ssd1306_bdf_font_t bdf_font;
    int _xpos = xpos;
    for (int i = 0; i < strnlen(text, SSD1306_TEXT_DISPLAY_MAX_LEN); i++) {
        memset(bitmap, 0, bitmap_size);
        int ch = text[i];
        esp_err_t err = ssd1306_load_bitmap_font(font, ch, bitmap, &bdf_font);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "font not found [%d]", ch);
            continue;
        }
        // ESP_LOG_BUFFER_HEXDUMP(tag, bitmap, bdf_font.num_data, ESP_LOG_INFO);
        ESP_LOGD(TAG, "bdf_font.width=%d", bdf_font.width);
        ESP_LOGD(TAG, "bdf_font.bbx=%d, bby=%d", bdf_font.bbx, bdf_font.bby);
        ESP_LOGD(TAG, "bdf_font.y_start=%d, y_end=%d", bdf_font.y_start, bdf_font.y_end);
        int bitmap_height = bdf_font.y_end - bdf_font.y_start + 1;
        int bitmap_width = bdf_font.num_data / bitmap_height;
        ESP_LOGD(TAG, "bitmap_width=%d bitmap_height=%d", bitmap_width, bitmap_height);
        ssd1306_set_bitmap(handle, _xpos, ypos + bdf_font.y_start, bitmap, bitmap_width * 8, bitmap_height, false);
        _xpos = _xpos + bdf_font.width;
    }
    ESP_RETURN_ON_ERROR(ssd1306_display_pages(handle), TAG, "display pages for display bdf text failed");
    free(bitmap);
    return ESP_OK;
}

esp_err_t ssd1306_display_bdf_code(ssd1306_handle_t handle, const uint8_t* font, int code, int xpos, int ypos) {
    /* validate parameters */
    ESP_ARG_CHECK(handle);

    int fontboundingbox_width = font[0];
    int fontboundingbox_height = font[1];
    size_t bitmap_size = ((fontboundingbox_width + 7) / 8) * fontboundingbox_height;
    ESP_LOGD(TAG, "fontboundingbox_width=%d fontboundingbox_height=%d bitmap_size=%d",
             fontboundingbox_width, fontboundingbox_height, bitmap_size);
    uint8_t* bitmap = malloc(bitmap_size);
    if (bitmap == NULL) {
        ESP_LOGE(TAG, "malloc fail");
        return ESP_ERR_NO_MEM;
    }
    ssd1306_bdf_font_t bdf_font;
    memset(bitmap, 0, bitmap_size);
    esp_err_t err = ssd1306_load_bitmap_font(font, code, bitmap, &bdf_font);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "font not found [%d]", code);
        return err;
    }
    // ESP_LOG_BUFFER_HEXDUMP(tag, bitmap, bdf_font.num_data, ESP_LOG_INFO);
    ESP_LOGD(TAG, "bdf_font.width=%d", bdf_font.width);
    ESP_LOGD(TAG, "bdf_font.bbx=%d, bby=%d", bdf_font.bbx, bdf_font.bby);
    ESP_LOGD(TAG, "bdf_font.y_start=%d, y_end=%d", bdf_font.y_start, bdf_font.y_end);
    int bitmap_height = bdf_font.y_end - bdf_font.y_start + 1;
    int bitmap_width = bdf_font.num_data / bitmap_height;
    ESP_LOGD(TAG, "bitmap_width=%d bitmap_height=%d", bitmap_width, bitmap_height);
    ESP_LOG_BUFFER_HEXDUMP(TAG, bitmap, bdf_font.num_data, ESP_LOG_DEBUG);
    ssd1306_display_bitmap(handle, xpos, ypos + bdf_font.y_start, bitmap, bitmap_width * 8, bitmap_height, false);
    free(bitmap);
    return ESP_OK;
}

esp_err_t ssd1306_set_pixel(ssd1306_handle_t handle, uint8_t xpos, uint8_t ypos, bool invert) {
    /* validate parameters */
    ESP_ARG_CHECK(handle);

    if (
        xpos >= handle->width ||
        ypos >= handle->height) {
        /* Error */
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t _page = (ypos / 8);
    uint8_t _bits = (ypos % 8);
    uint8_t _seg = xpos;
    uint8_t wk0 = handle->page[_page].segment[_seg];
    uint8_t wk1 = 1 << _bits;

    ESP_LOGD(TAG, "ypos=%d _page=%d _bits=%d wk0=0x%02x wk1=0x%02x", ypos, _page, _bits, wk0, wk1);

    if (invert) {
        wk0 = wk0 & ~wk1;
    } else {
        wk0 = wk0 | wk1;
    }

    if (handle->dev_config.flip_enabled) wk0 = ssd1306_rotate_byte(wk0);

    ESP_LOGD(TAG, "wk0=0x%02x wk1=0x%02x", wk0, wk1);

    handle->page[_page].segment[_seg] = wk0;

    return ESP_OK;
}

esp_err_t ssd1306_set_line(ssd1306_handle_t handle, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool invert) {
    int16_t dx, dy, sx, sy, err, e2, i, tmp;

    /* validate parameters */
    ESP_ARG_CHECK(handle);

    /* Check for overflow */
    if (x0 >= handle->width) {
        x0 = handle->width - 1;
    }
    if (x1 >= handle->width) {
        x1 = handle->width - 1;
    }
    if (y0 >= handle->height) {
        y0 = handle->height - 1;
    }
    if (y1 >= handle->height) {
        y1 = handle->height - 1;
    }

    dx = (x0 < x1) ? (x1 - x0) : (x0 - x1);
    dy = (y0 < y1) ? (y1 - y0) : (y0 - y1);
    sx = (x0 < x1) ? 1 : -1;
    sy = (y0 < y1) ? 1 : -1;
    err = ((dx > dy) ? dx : -dy) / 2;

    if (dx == 0) {
        if (y1 < y0) {
            tmp = y1;
            y1 = y0;
            y0 = tmp;
        }

        if (x1 < x0) {
            tmp = x1;
            x1 = x0;
            x0 = tmp;
        }

        /* Vertical line */
        for (i = y0; i <= y1; i++) {
            ssd1306_set_pixel(handle, x0, i, invert);
        }

        /* Return from function */
        return ESP_OK;
    }

    if (dy == 0) {
        if (y1 < y0) {
            tmp = y1;
            y1 = y0;
            y0 = tmp;
        }

        if (x1 < x0) {
            tmp = x1;
            x1 = x0;
            x0 = tmp;
        }

        /* Horizontal line */
        for (i = x0; i <= x1; i++) {
            ssd1306_set_pixel(handle, i, y0, invert);
        }

        /* Return from function */
        return ESP_OK;
    }

    while (1) {
        ssd1306_set_pixel(handle, x0, y0, invert);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        e2 = err;
        if (e2 > -dx) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dy) {
            err += dx;
            y0 += sy;
        }
    }

    return ESP_OK;
}

esp_err_t ssd1306_set_circle(ssd1306_handle_t handle, uint8_t x0, uint8_t y0, uint8_t r, bool invert) {
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    /* validate parameters */
    ESP_ARG_CHECK(handle);

    ssd1306_set_pixel(handle, x0, y0 + r, invert);
    ssd1306_set_pixel(handle, x0, y0 - r, invert);
    ssd1306_set_pixel(handle, x0 + r, y0, invert);
    ssd1306_set_pixel(handle, x0 - r, y0, invert);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        ssd1306_set_pixel(handle, x0 + x, y0 + y, invert);
        ssd1306_set_pixel(handle, x0 - x, y0 + y, invert);
        ssd1306_set_pixel(handle, x0 + x, y0 - y, invert);
        ssd1306_set_pixel(handle, x0 - x, y0 - y, invert);

        ssd1306_set_pixel(handle, x0 + y, y0 + x, invert);
        ssd1306_set_pixel(handle, x0 - y, y0 + x, invert);
        ssd1306_set_pixel(handle, x0 + y, y0 - x, invert);
        ssd1306_set_pixel(handle, x0 - y, y0 - x, invert);
    }

    return ESP_OK;
}

esp_err_t ssd1306_display_circle(ssd1306_handle_t handle, uint8_t x0, uint8_t y0, uint8_t r, bool invert) {
    /* validate parameters */
    ESP_ARG_CHECK(handle);

    ESP_RETURN_ON_ERROR(ssd1306_set_circle(handle, x0, y0, r, invert), TAG, "set circle for display circle failed");

    ESP_RETURN_ON_ERROR(ssd1306_display_pages(handle), TAG, "display pages for circle failed");

    return ESP_OK;
}

esp_err_t ssd1306_display_filled_circle(ssd1306_handle_t handle, uint8_t x0, uint8_t y0, uint8_t r, bool invert) {
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    /* validate parameters */
    ESP_ARG_CHECK(handle);

    ssd1306_set_pixel(handle, x0, y0 + r, invert);
    ssd1306_set_pixel(handle, x0, y0 - r, invert);
    ssd1306_set_pixel(handle, x0 + r, y0, invert);
    ssd1306_set_pixel(handle, x0 - r, y0, invert);
    ssd1306_set_line(handle, x0 - r, y0, x0 + r, y0, invert);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        ssd1306_set_line(handle, x0 - (uint8_t)x, y0 + (uint8_t)y, x0 + (uint8_t)x, y0 + (uint8_t)y, invert);
        ssd1306_set_line(handle, x0 + (uint8_t)x, y0 - (uint8_t)y, x0 - (uint8_t)x, y0 - (uint8_t)y, invert);

        ssd1306_set_line(handle, x0 + (uint8_t)y, y0 + (uint8_t)x, x0 - (uint8_t)y, y0 + (uint8_t)x, invert);
        ssd1306_set_line(handle, x0 + (uint8_t)y, y0 - (uint8_t)x, x0 - (uint8_t)y, y0 - (uint8_t)x, invert);
    }

    ESP_RETURN_ON_ERROR(ssd1306_display_pages(handle), TAG, "display pages for filled circle failed");

    return ESP_OK;
}

esp_err_t ssd1306_set_rectangle(ssd1306_handle_t handle, uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool invert) {
    /* validate parameters */
    ESP_ARG_CHECK(handle);

    /* Check input parameters */
    if (
        x >= handle->width ||
        y >= handle->height) {
        /* Return error */
        return ESP_ERR_INVALID_SIZE;
    }

    /* Check width and height */
    if ((x + w) >= handle->width) {
        w = handle->width - x;
    }
    if ((y + h) >= handle->height) {
        h = handle->height - y;
    }

    /* Set 4 lines */
    ssd1306_set_line(handle, x, y, x + w, y, invert);         /* Top line */
    ssd1306_set_line(handle, x, y + h, x + w, y + h, invert); /* Bottom line */
    ssd1306_set_line(handle, x, y, x, y + h, invert);         /* Left line */
    ssd1306_set_line(handle, x + w, y, x + w, y + h, invert); /* Right line */

    return ESP_OK;
}

esp_err_t ssd1306_display_rectangle(ssd1306_handle_t handle, uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool invert) {
    /* validate parameters */
    ESP_ARG_CHECK(handle);

    ESP_RETURN_ON_ERROR(ssd1306_set_rectangle(handle, x, y, w, h, invert), TAG, "set rectangle for display rectangle failed");

    ESP_RETURN_ON_ERROR(ssd1306_display_pages(handle), TAG, "display pages for rectangle failed");

    return ESP_OK;
}

esp_err_t ssd1306_display_filled_rectangle(ssd1306_handle_t handle, uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool invert) {
    /* validate parameters */
    ESP_ARG_CHECK(handle);

    /* Check input parameters */
    if (
        x >= handle->width ||
        y >= handle->height) {
        /* Return error */
        return ESP_ERR_INVALID_SIZE;
    }

    /* Check width and height */
    if ((x + w) >= handle->width) {
        w = handle->width - x;
    }
    if ((y + h) >= handle->height) {
        h = handle->height - y;
    }

    /* Set lines */
    for (uint8_t i = 0; i <= h; i++) {
        /* Set line */
        ssd1306_set_line(handle, x, y + i, x + w, y + i, invert);
    }

    ESP_RETURN_ON_ERROR(ssd1306_display_pages(handle), TAG, "display pages for rectangle failed");

    return ESP_OK;
}

esp_err_t ssd1306_enable_display(ssd1306_handle_t handle) {
    uint8_t out_buf[2];
    uint8_t out_index = 0;

    /* validate parameters */
    ESP_ARG_CHECK(handle);

    out_buf[out_index++] = SSD1306_CONTROL_BYTE_CMD_STREAM;  // 00
    out_buf[out_index++] = SSD1306_CMD_DISPLAY_ON;           // AF

    ESP_RETURN_ON_ERROR(ssd1306_i2c_write(handle, out_buf, out_index), TAG, "write contrast configuration failed");

    /* set handle parameter */
    handle->dev_config.display_enabled = true;

    return ESP_OK;
}

esp_err_t ssd1306_disable_display(ssd1306_handle_t handle) {
    uint8_t out_buf[2];
    uint8_t out_index = 0;

    /* validate parameters */
    ESP_ARG_CHECK(handle);

    out_buf[out_index++] = SSD1306_CONTROL_BYTE_CMD_STREAM;  // 00
    out_buf[out_index++] = SSD1306_CMD_DISPLAY_OFF;          // AE

    ESP_RETURN_ON_ERROR(ssd1306_i2c_write(handle, out_buf, out_index), TAG, "write contrast configuration failed");

    /* set handle parameter */
    handle->dev_config.display_enabled = false;

    return ESP_OK;
}

esp_err_t ssd1306_display_pages(ssd1306_handle_t handle) {
    /* validate parameters */
    ESP_ARG_CHECK(handle);

    for (uint8_t page = 0; page < handle->pages; page++) {
        ESP_RETURN_ON_ERROR(ssd1306_display_image(handle, page, 0, handle->page[page].segment, handle->width), TAG, "show buffer failed (page %d)", page);
    }

    return ESP_OK;
}

esp_err_t ssd1306_set_pages(ssd1306_handle_t handle, uint8_t* buffer) {
    uint8_t index = 0;

    /* validate parameters */
    ESP_ARG_CHECK(handle);

    for (uint8_t page = 0; page < handle->pages; page++) {
        memcpy(&handle->page[page].segment, &buffer[index], 128);
        index = index + 128;
    }

    return ESP_OK;
}

esp_err_t ssd1306_get_pages(ssd1306_handle_t handle, uint8_t* buffer) {
    uint8_t index = 0;

    /* validate parameters */
    ESP_ARG_CHECK(handle);

    for (uint8_t page = 0; page < handle->pages; page++) {
        memcpy(&buffer[index], &handle->page[page].segment, 128);
        index = index + 128;
    }

    return ESP_OK;
}

esp_err_t ssd1306_set_bitmap(ssd1306_handle_t handle, uint8_t xpos, uint8_t ypos, const uint8_t* bitmap, uint8_t width, uint8_t height, bool invert) {
    uint8_t byte_width = (width + 7) / 8;

    /* validate parameters */
    ESP_ARG_CHECK(handle);

    for (uint8_t j = 0; j < height; j++) {
        for (uint8_t i = 0; i < width; i++) {
            if (*(bitmap + j * byte_width + i / 8) & (128 >> (i & 7))) {
                ssd1306_set_pixel(handle, xpos + i, ypos + j, invert);
            }
        }
    }

    return ESP_OK;
}

esp_err_t ssd1306_display_bitmap(ssd1306_handle_t handle, uint8_t xpos, uint8_t ypos, const uint8_t* bitmap, uint8_t width, uint8_t height, bool invert) {
    /* validate parameters */
    ESP_ARG_CHECK(handle);

    ESP_RETURN_ON_ERROR(ssd1306_set_bitmap(handle, xpos, ypos, bitmap, width, height, invert), TAG, "set bitmap for display bitmap failed");

    ESP_RETURN_ON_ERROR(ssd1306_display_pages(handle), TAG, "display pages for bitmap failed");

    return ESP_OK;
}

/* this works fine for a 128x32 but the pages repeat after page 3, e.g. pages 0-3 and 4-7 are the same */
esp_err_t ssd1306_display_bitmap__(ssd1306_handle_t handle, uint8_t xpos, uint8_t ypos, const uint8_t* bitmap, uint8_t width, uint8_t height, bool invert) {
    /* validate parameters */
    ESP_ARG_CHECK(handle);

    if ((width % 8) != 0) {
        ESP_LOGE(TAG, "width must be a multiple of 8");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t _width = width / 8;
    uint8_t wk0;
    uint8_t wk1;
    uint8_t wk2;
    uint8_t page = (ypos / 8);
    uint8_t _seg = xpos;
    uint8_t dstBits = (ypos % 8);
    uint8_t offset = 0;

    ESP_LOGD(TAG, "ypos=%d page=%d dstBits=%d", ypos, page, dstBits);

    for (uint8_t _height = 0; _height < height; _height++) {
        for (uint8_t index = 0; index < _width; index++) {
            for (int8_t srcBits = 7; srcBits >= 0; srcBits--) {
                wk0 = handle->page[page].segment[_seg];
                if (handle->dev_config.flip_enabled) {
                    wk0 = ssd1306_rotate_byte(wk0);
                }

                wk1 = bitmap[index + offset];
                if (invert) {
                    wk1 = ~wk1;
                }

                wk2 = ssd1306_copy_bit(wk1, srcBits, wk0, dstBits);
                if (handle->dev_config.flip_enabled) {
                    wk2 = ssd1306_rotate_byte(wk2);
                }

                ESP_LOGD(TAG, "index=%d offset=%d page=%d _seg=%d, wk2=%02x", index, offset, page, _seg, wk2);
                handle->page[page].segment[_seg] = wk2;
                _seg++;
            }
        }
        vTaskDelay(1);
        offset = offset + _width;
        dstBits++;
        _seg = xpos;
        if (dstBits == 8) {
            page++;
            dstBits = 0;
        }
    }

    ESP_RETURN_ON_ERROR(ssd1306_display_pages(handle), TAG, "display pages for bitmap failed");

    return ESP_OK;
}

esp_err_t ssd1306_display_image(ssd1306_handle_t handle, uint8_t page, uint8_t segment, const uint8_t* image, uint8_t width) {
    esp_err_t ret = ESP_OK;

    /* validate parameters */
    ESP_ARG_CHECK(handle);

    if (page >= handle->pages) return ESP_ERR_INVALID_SIZE;
    if (segment >= handle->width) return ESP_ERR_INVALID_SIZE;

    uint8_t _seg = segment + handle->dev_config.offset_x;
    uint8_t columLow = _seg & 0x0F;
    uint8_t columHigh = (_seg >> 4) & 0x0F;

    uint8_t _page = page;
    if (handle->dev_config.flip_enabled) {
        _page = (handle->pages - page) - 1;
    }

    uint8_t* out_buf;
    out_buf = malloc(width < 4 ? 4 : width + 1);
    if (out_buf == NULL) {
        ESP_LOGE(TAG, "malloc for image display failed");
        return ESP_ERR_NO_MEM;
    }

    uint8_t out_index = 0;
    out_buf[out_index++] = SSD1306_CONTROL_BYTE_CMD_STREAM;
    // Set Lower Column Start Address for Page Addressing Mode
    out_buf[out_index++] = (0x00 + columLow);
    // Set Higher Column Start Address for Page Addressing Mode
    out_buf[out_index++] = (0x10 + columHigh);
    // Set Page Start Address for Page Addressing Mode
    out_buf[out_index++] = 0xB0 | _page;

    ESP_GOTO_ON_ERROR(ssd1306_i2c_write(handle, out_buf, out_index), err, TAG, "write page addressing mode for image display failed");

    out_buf[0] = SSD1306_CONTROL_BYTE_DATA_STREAM;

    memcpy(&out_buf[1], image, width);

    ESP_GOTO_ON_ERROR(ssd1306_i2c_write(handle, out_buf, width + 1), err, TAG, "write image for image display failed");

    free(out_buf);

    // Set to internal buffer
    memcpy(&handle->page[page].segment[segment], image, width);

    return ESP_OK;

err:
    return ret;
}

esp_err_t ssd1306_display_text(ssd1306_handle_t handle, uint8_t page, char* text, bool invert) {
    /* validate parameters */
    ESP_ARG_CHECK(handle);

    if (page >= handle->pages) return ESP_ERR_INVALID_SIZE;

    if (strnlen(text, SSD1306_TEXT_DISPLAY_MAX_LEN + 1) > SSD1306_TEXT_DISPLAY_MAX_LEN) {
        text[SSD1306_TEXT_DISPLAY_MAX_LEN - 1] = '\0';  // Ensure null termination at end //todo maybe scroll text instead of truncating
        // return ESP_ERR_INVALID_SIZE;
    }

    uint8_t seg = 0;
    uint8_t image[8];

    for (uint8_t i = 0; i < strnlen(text, SSD1306_TEXT_DISPLAY_MAX_LEN); i++) {
        memcpy(image, font_latin_8x8_tr[(uint8_t)text[i]], 8);
        if (invert) ssd1306_invert_buffer(image, 8);
        if (handle->dev_config.flip_enabled) ssd1306_flip_buffer(image, 8);
        ssd1306_display_image(handle, page, seg, image, 8);
        seg = seg + 8;
    }

    return ESP_OK;
}

esp_err_t ssd1306_display_text_x2(ssd1306_handle_t handle, uint8_t page, const char* text, bool invert) {
    /* validate parameters */
    ESP_ARG_CHECK(handle);

    if (page >= handle->pages) return ESP_ERR_INVALID_SIZE;

    if (strnlen(text, SSD1306_TEXT_X2_DISPLAY_MAX_LEN + 1) > SSD1306_TEXT_X2_DISPLAY_MAX_LEN) return ESP_ERR_INVALID_SIZE;

    uint8_t seg = 0;

    for (uint8_t nn = 0; nn < strnlen(text, SSD1306_TEXT_X2_DISPLAY_MAX_LEN); nn++) {
        uint8_t const* const in_columns = font_latin_8x8_tr[(uint8_t)text[nn]];

        // make the character 2x as high
        ssd1306_out_column_t out_columns[8];
        memset(out_columns, 0, sizeof(out_columns));

        for (uint8_t xx = 0; xx < 8; xx++) {  // for each column (x-direction)
            uint32_t in_bitmask = 0b1;
            uint32_t out_bitmask = 0b11;

            for (uint8_t yy = 0; yy < 8; yy++) {  // for pixel (y-direction)
                if (in_columns[xx] & in_bitmask) {
                    out_columns[xx].u32 |= out_bitmask;
                }
                in_bitmask <<= 1;
                out_bitmask <<= 2;
            }
        }

        // render character in 8 column high pieces, making them 2x as wide
        for (uint8_t yy = 0; yy < 2; yy++) {  // for each group of 8 pixels high (y-direction)
            uint8_t image[16];
            for (uint8_t xx = 0; xx < 8; xx++) {  // for each column (x-direction)
                image[xx * 2 + 0] =
                    image[xx * 2 + 1] = out_columns[xx].u8[yy];
            }
            if (invert) ssd1306_invert_buffer(image, 16);
            if (handle->dev_config.flip_enabled) ssd1306_flip_buffer(image, 16);

            ESP_RETURN_ON_ERROR(ssd1306_display_image(handle, page + yy, seg, image, 16), TAG, "display image for display text x2 failed");

            memcpy(&handle->page[page + yy].segment[seg], image, 16);
        }
        seg = seg + 16;
    }

    return ESP_OK;
}

esp_err_t ssd1306_display_text_x3(ssd1306_handle_t handle, uint8_t page, const char* text, bool invert) {
    /* validate parameters */
    ESP_ARG_CHECK(handle);

    if (page >= handle->pages) return ESP_ERR_INVALID_SIZE;

    if (strnlen(text, SSD1306_TEXT_X3_DISPLAY_MAX_LEN + 1) > SSD1306_TEXT_X3_DISPLAY_MAX_LEN) return ESP_ERR_INVALID_SIZE;

    uint8_t seg = 0;

    for (uint8_t nn = 0; nn < strnlen(text, SSD1306_TEXT_X3_DISPLAY_MAX_LEN); nn++) {
        uint8_t const* const in_columns = font_latin_8x8_tr[(uint8_t)text[nn]];

        // make the character 3x as high
        ssd1306_out_column_t out_columns[8];
        memset(out_columns, 0, sizeof(out_columns));

        for (uint8_t xx = 0; xx < 8; xx++) {  // for each column (x-direction)
            uint32_t in_bitmask = 0b1;
            uint32_t out_bitmask = 0b111;

            for (uint8_t yy = 0; yy < 8; yy++) {  // for pixel (y-direction)
                if (in_columns[xx] & in_bitmask) {
                    out_columns[xx].u32 |= out_bitmask;
                }
                in_bitmask <<= 1;
                out_bitmask <<= 3;
            }
        }

        // render character in 8 column high pieces, making them 3x as wide
        for (uint8_t yy = 0; yy < 3; yy++) {  // for each group of 8 pixels high (y-direction)
            uint8_t image[24];
            for (uint8_t xx = 0; xx < 8; xx++) {  // for each column (x-direction)
                image[xx * 3 + 0] =
                    image[xx * 3 + 1] =
                        image[xx * 3 + 2] = out_columns[xx].u8[yy];
            }
            if (invert) ssd1306_invert_buffer(image, 24);
            if (handle->dev_config.flip_enabled) ssd1306_flip_buffer(image, 24);

            ESP_RETURN_ON_ERROR(ssd1306_display_image(handle, page + yy, seg, image, 24), TAG, "display image for display text x3 failed");

            memcpy(&handle->page[page + yy].segment[seg], image, 24);
        }
        seg = seg + 24;
    }

    return ESP_OK;
}

esp_err_t ssd1306_display_textbox_banner(ssd1306_handle_t handle, uint8_t page, uint8_t segment, const char* text, uint8_t box_width, bool invert, uint8_t delay) {
    if (page >= handle->pages) return ESP_ERR_INVALID_SIZE;
    uint8_t text_box_pixel = box_width * 8;
    if (segment + text_box_pixel > handle->width) return ESP_ERR_INVALID_SIZE;
    if (strnlen(text, SSD1306_TEXTBOX_DISPLAY_MAX_LEN + 1) > SSD1306_TEXTBOX_DISPLAY_MAX_LEN) return ESP_ERR_INVALID_SIZE;

    uint8_t _seg = segment;
    uint8_t image[8];

    for (uint8_t i = 0; i < box_width; i++) {
        memcpy(image, font_latin_8x8_tr[(uint8_t)text[i]], 8);
        if (invert) ssd1306_invert_buffer(image, 8);
        if (handle->dev_config.flip_enabled) ssd1306_flip_buffer(image, 8);
        ssd1306_display_image(handle, page, _seg, image, 8);
        _seg = _seg + 8;
    }
    vTaskDelay(delay / portTICK_PERIOD_MS);

    // Horizontally scroll inside the box
    for (uint8_t _text = box_width; _text < strnlen(text, SSD1306_TEXTBOX_DISPLAY_MAX_LEN); _text++) {
        memcpy(image, font_latin_8x8_tr[(uint8_t)text[_text]], 8);
        if (invert) ssd1306_invert_buffer(image, 8);
        if (handle->dev_config.flip_enabled) ssd1306_flip_buffer(image, 8);
        for (uint8_t _bit = 0; _bit < 8; _bit++) {
            for (int _pixel = 0; _pixel < text_box_pixel; _pixel++) {
                // ESP_LOGI(TAG, "_text=%d _bit=%d _pixel=%d", _text, _bit, _pixel);
                handle->page[page].segment[_pixel + segment] = handle->page[page].segment[_pixel + segment + 1];
            }
            handle->page[page].segment[segment + text_box_pixel - 1] = image[_bit];
            ssd1306_display_image(handle, page, segment, &handle->page[page].segment[segment], text_box_pixel);
            vTaskDelay(delay / portTICK_PERIOD_MS);
        }
    }

    return ESP_OK;
}

esp_err_t ssd1306_display_textbox_ticker(ssd1306_handle_t handle, uint8_t page, uint8_t segment, const char* text, uint8_t box_width, bool invert, uint8_t delay) {
    if (page >= handle->pages) return ESP_ERR_INVALID_SIZE;
    uint8_t text_box_pixel = box_width * 8;
    if (segment + text_box_pixel > handle->width) return ESP_ERR_INVALID_SIZE;
    if (strnlen(text, SSD1306_TEXTBOX_DISPLAY_MAX_LEN + 1) > SSD1306_TEXTBOX_DISPLAY_MAX_LEN) return ESP_ERR_INVALID_SIZE;

    uint8_t _seg = segment;
    uint8_t image[8];

    // fill box with spaces
    for (uint8_t i = 0; i < box_width; i++) {
        memcpy(image, font_latin_8x8_tr[21], 8);
        if (invert) ssd1306_invert_buffer(image, 8);
        if (handle->dev_config.flip_enabled) ssd1306_flip_buffer(image, 8);
        ssd1306_display_image(handle, page, _seg, image, 8);
        _seg = _seg + 8;
    }
    vTaskDelay(delay / portTICK_PERIOD_MS);

    // Horizontally scroll inside the box
    for (uint8_t _text = 0; _text < strnlen(text, SSD1306_TEXTBOX_DISPLAY_MAX_LEN); _text++) {
        memcpy(image, font_latin_8x8_tr[(uint8_t)text[_text]], 8);
        if (invert) ssd1306_invert_buffer(image, 8);
        if (handle->dev_config.flip_enabled) ssd1306_flip_buffer(image, 8);
        for (uint8_t _bit = 0; _bit < 8; _bit++) {
            for (uint8_t _pixel = 0; _pixel < text_box_pixel; _pixel++) {
                // ESP_LOGI(TAG, "_text=%d _bit=%d _pixel=%d", _text, _bit, _pixel);
                handle->page[page].segment[_pixel + segment] = handle->page[page].segment[_pixel + segment + 1];
            }
            handle->page[page].segment[segment + text_box_pixel - 1] = image[_bit];
            ssd1306_display_image(handle, page, segment, &handle->page[page].segment[segment], text_box_pixel);
            vTaskDelay(delay / portTICK_PERIOD_MS);
        }
    }

    // Horizontally scroll inside the box
    for (uint8_t _text = 0; _text < box_width; _text++) {
        memcpy(image, font_latin_8x8_tr[21], 8);
        if (invert) ssd1306_invert_buffer(image, 8);
        if (handle->dev_config.flip_enabled) ssd1306_flip_buffer(image, 8);
        for (uint8_t _bit = 0; _bit < 8; _bit++) {
            for (uint8_t _pixel = 0; _pixel < text_box_pixel; _pixel++) {
                // ESP_LOGI(TAG, "_text=%d _bit=%d _pixel=%d", _text, _bit, _pixel);
                handle->page[page].segment[_pixel + segment] = handle->page[page].segment[_pixel + segment + 1];
            }
            handle->page[page].segment[segment + text_box_pixel - 1] = image[_bit];
            ssd1306_display_image(handle, page, segment, &handle->page[page].segment[segment], text_box_pixel);
            vTaskDelay(delay / portTICK_PERIOD_MS);
        }
    }

    return ESP_OK;
}

esp_err_t ssd1306_clear_display_page(ssd1306_handle_t handle, uint8_t page, bool invert) {
    /* validate parameters */
    ESP_ARG_CHECK(handle);

    ESP_RETURN_ON_ERROR(ssd1306_display_text(handle, page, "                ", invert), TAG, "display text for clear line failed");

    return ESP_OK;
}

esp_err_t ssd1306_clear_display(ssd1306_handle_t handle, bool invert) {
    /* validate parameters */
    ESP_ARG_CHECK(handle);

    for (uint8_t page = 0; page < handle->pages; page++) {
        ESP_RETURN_ON_ERROR(ssd1306_clear_display_page(handle, page, invert), TAG, "clear line for clear screen failed");
    }

    return ESP_OK;
}

esp_err_t ssd1306_set_contrast(ssd1306_handle_t handle, uint8_t contrast) {
    uint8_t out_buf[3];
    uint8_t out_index = 0;

    /* validate parameters */
    ESP_ARG_CHECK(handle);

    out_buf[out_index++] = SSD1306_CONTROL_BYTE_CMD_STREAM;  // 00
    out_buf[out_index++] = SSD1306_CMD_SET_CONTRAST;         // 81
    out_buf[out_index++] = contrast;

    ESP_RETURN_ON_ERROR(ssd1306_i2c_write(handle, out_buf, out_index), TAG, "write contrast configuration failed");

    return ESP_OK;
}

esp_err_t ssd1306_set_software_scroll(ssd1306_handle_t handle, uint8_t start, uint8_t end) {
    /* validate parameters */
    ESP_ARG_CHECK(handle);

    ESP_LOGD(TAG, "software_scroll start=%d end=%d _pages=%d", start, end, handle->pages);

    if (start >= handle->pages || end >= handle->pages) {
        handle->scroll_enabled = false;
    } else {
        handle->scroll_enabled = true;
        handle->scroll_start = start;
        handle->scroll_end = end;
        handle->scroll_direction = 1;
        if (start > end) handle->scroll_direction = -1;
    }

    return ESP_OK;
}

esp_err_t ssd1306_display_software_scroll_text(ssd1306_handle_t handle, const char* text, bool invert) {
    /* validate parameters */
    ESP_ARG_CHECK(handle);

    if (strnlen(text, SSD1306_TEXT_DISPLAY_MAX_LEN + 1) > SSD1306_TEXT_DISPLAY_MAX_LEN) return ESP_ERR_INVALID_SIZE;

    ESP_LOGD(TAG, "ssd1306_handle->dev_params->scroll_enabled=%d", handle->scroll_enabled);
    if (handle->scroll_enabled == false) return ESP_ERR_INVALID_ARG;

    uint16_t srcIndex = handle->scroll_end - handle->scroll_direction;
    while (1) {
        uint16_t dstIndex = srcIndex + handle->scroll_direction;
        ESP_LOGD(TAG, "srcIndex=%u dstIndex=%u", srcIndex, dstIndex);
        for (uint16_t seg = 0; seg < handle->width; seg++) {
            handle->page[dstIndex].segment[seg] = handle->page[srcIndex].segment[seg];
        }
        ESP_RETURN_ON_ERROR(ssd1306_display_image(handle, dstIndex, 0, handle->page[dstIndex].segment, sizeof(handle->page[dstIndex].segment)), TAG, "display image for scroll text failed");
        if (srcIndex == handle->scroll_start) break;
        srcIndex = srcIndex - handle->scroll_direction;
    }

    ESP_RETURN_ON_ERROR(ssd1306_display_text(handle, srcIndex, text, invert), TAG, "display text for scroll text failed");

    return ESP_OK;
}

esp_err_t ssd1306_clear_display_software_scroll(ssd1306_handle_t handle) {
    /* validate parameters */
    ESP_ARG_CHECK(handle);

    ESP_LOGD(TAG, "ssd1306_handle->dev_params->scroll_enabled=%d", handle->scroll_enabled);
    ESP_RETURN_ON_FALSE(handle->scroll_enabled, ESP_ERR_INVALID_ARG, TAG, "software scroll not enabled");

    uint16_t srcIndex = handle->scroll_end - handle->scroll_direction;
    while (1) {
        uint16_t dstIndex = srcIndex + handle->scroll_direction;
        ESP_LOGD(TAG, "srcIndex=%u dstIndex=%u", srcIndex, dstIndex);
        ESP_RETURN_ON_ERROR(ssd1306_clear_display_page(handle, dstIndex, false), TAG, "clear display page for scroll clear failed");
        if (dstIndex == handle->scroll_start) break;
        srcIndex = srcIndex - handle->scroll_direction;
    }

    return ESP_OK;
}

esp_err_t ssd1306_set_hardware_scroll(ssd1306_handle_t handle, ssd1306_scroll_types_t scroll, ssd1306_scroll_frames_t frame_frequency) {
    uint8_t out_buf[15];
    uint8_t out_index = 0;

    /* validate parameters */
    ESP_ARG_CHECK(handle);

    if (handle->dev_config.panel_size == SSD1306_PAGE_128x128_SIZE) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    out_buf[out_index++] = SSD1306_CONTROL_BYTE_CMD_STREAM;  // 00

    if (scroll == SSD1306_SCROLL_RIGHT) {
        out_buf[out_index++] = SSD1306_CMD_HORIZONTAL_RIGHT;  // 26
        out_buf[out_index++] = 0x00;                          // Dummy byte
        out_buf[out_index++] = 0x00;                          // Define start page address
        out_buf[out_index++] = (uint8_t)frame_frequency;      // Frame frequency
        out_buf[out_index++] = 0x01;                          // Define end page address
        out_buf[out_index++] = 0x00;                          // Dummy byte 0x00
        out_buf[out_index++] = 0xFF;                          // Dummy byte 0xFF
        out_buf[out_index++] = SSD1306_CMD_ACTIVE_SCROLL;     // 2F
    }

    if (scroll == SSD1306_SCROLL_LEFT) {
        out_buf[out_index++] = SSD1306_CMD_HORIZONTAL_LEFT;  // 27
        out_buf[out_index++] = 0x00;                         // Dummy byte
        out_buf[out_index++] = 0x00;                         // Define start page address
        out_buf[out_index++] = (uint8_t)frame_frequency;     // Frame frequency
        out_buf[out_index++] = 0x01;                         // Define end page address
        out_buf[out_index++] = 0x00;                         //
        out_buf[out_index++] = 0xFF;                         //
        out_buf[out_index++] = SSD1306_CMD_ACTIVE_SCROLL;    // 2F
    }

    if (scroll == SSD1306_SCROLL_DOWN) {
        out_buf[out_index++] = SSD1306_CMD_CONTINUOUS_SCROLL;  // 29
        out_buf[out_index++] = 0x00;                           // Dummy byte
        out_buf[out_index++] = 0x00;                           // Define start page address
        out_buf[out_index++] = (uint8_t)frame_frequency;       // Frame frequency
        out_buf[out_index++] = 0x00;                           // Define end page address
        out_buf[out_index++] = 0x3F;                           // Vertical scrolling offset

        out_buf[out_index++] = SSD1306_CMD_VERTICAL;  // A3
        out_buf[out_index++] = 0x00;
        if (handle->height == 128)
            out_buf[out_index++] = 0x80;
        if (handle->height == 64)
            out_buf[out_index++] = 0x40;
        if (handle->height == 32)
            out_buf[out_index++] = 0x20;
        out_buf[out_index++] = SSD1306_CMD_ACTIVE_SCROLL;  // 2F
    }

    if (scroll == SSD1306_SCROLL_UP) {
        out_buf[out_index++] = SSD1306_CMD_CONTINUOUS_SCROLL;  // 29
        out_buf[out_index++] = 0x00;                           // Dummy byte
        out_buf[out_index++] = 0x00;                           // Define start page address
        out_buf[out_index++] = (uint8_t)frame_frequency;       // Frame frequency
        out_buf[out_index++] = 0x00;                           // Define end page address
        out_buf[out_index++] = 0x01;                           // Vertical scrolling offset

        out_buf[out_index++] = SSD1306_CMD_VERTICAL;  // A3
        out_buf[out_index++] = 0x00;
        if (handle->height == 128)
            out_buf[out_index++] = 0x80;
        if (handle->height == 64)
            out_buf[out_index++] = 0x40;
        if (handle->height == 32)
            out_buf[out_index++] = 0x20;
        out_buf[out_index++] = SSD1306_CMD_ACTIVE_SCROLL;  // 2F
    }

    if (scroll == SSD1306_SCROLL_STOP) {
        out_buf[out_index++] = SSD1306_CMD_DEACTIVE_SCROLL;  // 2E
    }

    ESP_RETURN_ON_ERROR(ssd1306_i2c_write(handle, out_buf, out_index), TAG, "write hardware scroll configuration failed");

    return ESP_OK;
}

// delay = 0 : display with no wait
// delay > 0 : display with wait
// delay < 0 : no display
esp_err_t ssd1306_display_wrap_around(ssd1306_handle_t handle, ssd1306_scroll_types_t scroll, uint8_t start, uint8_t end, int8_t delay) {
    /* validate parameters */
    ESP_ARG_CHECK(handle);

    if (scroll == SSD1306_SCROLL_RIGHT) {
        uint8_t _start = start;  // 0 to 7
        uint8_t _end = end;      // 0 to 7
        if (_end >= handle->pages) _end = handle->pages - 1;
        uint8_t wk;
        for (uint8_t page = _start; page <= _end; page++) {
            wk = handle->page[page].segment[127];
            for (uint8_t seg = 127; seg > 0; seg--) {
                handle->page[page].segment[seg] = handle->page[page].segment[seg - 1];
            }
            handle->page[page].segment[0] = wk;
        }
    } else if (scroll == SSD1306_SCROLL_LEFT) {
        uint8_t _start = start;  // 0 to 7
        uint8_t _end = end;      // 0 to 7
        if (_end >= handle->pages) _end = handle->pages - 1;
        uint8_t wk;
        for (uint8_t page = _start; page <= _end; page++) {
            wk = handle->page[page].segment[0];
            for (uint8_t seg = 0; seg < 127; seg++) {
                handle->page[page].segment[seg] = handle->page[page].segment[seg + 1];
            }
            handle->page[page].segment[127] = wk;
        }

    } else if (scroll == SSD1306_SCROLL_UP) {
        uint8_t _start = start;  // 0 to {width-1}
        uint8_t _end = end;      // 0 to {width-1}
        if (_end >= handle->width) _end = handle->width - 1;
        uint8_t wk0;
        uint8_t wk1;
        uint8_t wk2;
        uint8_t save[128];
        // Save pages 0
        for (uint8_t seg = 0; seg < 128; seg++) {
            save[seg] = handle->page[0].segment[seg];
        }
        // Page0 to Page6
        for (uint8_t page = 0; page < handle->pages - 1; page++) {
            for (uint8_t seg = _start; seg <= _end; seg++) {
                wk0 = handle->page[page].segment[seg];
                wk1 = handle->page[page + 1].segment[seg];
                if (handle->dev_config.flip_enabled) wk0 = ssd1306_rotate_byte(wk0);
                if (handle->dev_config.flip_enabled) wk1 = ssd1306_rotate_byte(wk1);
                if (seg == 0) {
                    ESP_LOGD(TAG, "b page=%d wk0=%02x wk1=%02x", page, wk0, wk1);
                }
                wk0 = wk0 >> 1;
                wk1 = wk1 & 0x01;
                wk1 = wk1 << 7;
                wk2 = wk0 | wk1;
                if (seg == 0) {
                    ESP_LOGD(TAG, "a page=%d wk0=%02x wk1=%02x wk2=%02x", page, wk0, wk1, wk2);
                }
                if (handle->dev_config.flip_enabled) wk2 = ssd1306_rotate_byte(wk2);
                handle->page[page].segment[seg] = wk2;
            }
        }
        // Page7
        uint8_t pages = handle->pages - 1;
        for (uint8_t seg = _start; seg <= _end; seg++) {
            wk0 = handle->page[pages].segment[seg];
            wk1 = save[seg];
            if (handle->dev_config.flip_enabled) wk0 = ssd1306_rotate_byte(wk0);
            if (handle->dev_config.flip_enabled) wk1 = ssd1306_rotate_byte(wk1);
            wk0 = wk0 >> 1;
            wk1 = wk1 & 0x01;
            wk1 = wk1 << 7;
            wk2 = wk0 | wk1;
            if (handle->dev_config.flip_enabled) wk2 = ssd1306_rotate_byte(wk2);
            handle->page[pages].segment[seg] = wk2;
        }

    } else if (scroll == SSD1306_SCROLL_DOWN) {
        uint8_t _start = start;  // 0 to {width-1}
        uint8_t _end = end;      // 0 to {width-1}
        if (_end >= handle->width) _end = handle->width - 1;
        uint8_t wk0;
        uint8_t wk1;
        uint8_t wk2;
        uint8_t save[128];
        // Save pages 7
        uint8_t pages = handle->pages - 1;
        for (uint8_t seg = 0; seg < 128; seg++) {
            save[seg] = handle->page[pages].segment[seg];
        }
        // Page7 to Page1
        for (uint8_t page = pages; page > 0; page--) {
            for (uint8_t seg = _start; seg <= _end; seg++) {
                wk0 = handle->page[page].segment[seg];
                wk1 = handle->page[page - 1].segment[seg];
                if (handle->dev_config.flip_enabled) wk0 = ssd1306_rotate_byte(wk0);
                if (handle->dev_config.flip_enabled) wk1 = ssd1306_rotate_byte(wk1);
                if (seg == 0) {
                    ESP_LOGD(TAG, "b page=%d wk0=%02x wk1=%02x", page, wk0, wk1);
                }
                wk0 = wk0 << 1;
                wk1 = wk1 & 0x80;
                wk1 = wk1 >> 7;
                wk2 = wk0 | wk1;
                if (seg == 0) {
                    ESP_LOGD(TAG, "a page=%d wk0=%02x wk1=%02x wk2=%02x", page, wk0, wk1, wk2);
                }
                if (handle->dev_config.flip_enabled) wk2 = ssd1306_rotate_byte(wk2);
                handle->page[page].segment[seg] = wk2;
            }
        }
        // Page0
        for (uint8_t seg = _start; seg <= _end; seg++) {
            wk0 = handle->page[0].segment[seg];
            wk1 = save[seg];
            if (handle->dev_config.flip_enabled) wk0 = ssd1306_rotate_byte(wk0);
            if (handle->dev_config.flip_enabled) wk1 = ssd1306_rotate_byte(wk1);
            wk0 = wk0 << 1;
            wk1 = wk1 & 0x80;
            wk1 = wk1 >> 7;
            wk2 = wk0 | wk1;
            if (handle->dev_config.flip_enabled) wk2 = ssd1306_rotate_byte(wk2);
            handle->page[0].segment[seg] = wk2;
        }
    }

    if (delay >= 0) {
        for (uint8_t page = 0; page < handle->pages; page++) {
            ESP_RETURN_ON_ERROR(ssd1306_display_image(handle, page, 0, handle->page[page].segment, 128), TAG, "display image for wrap around failed");
            if (delay) vTaskDelay(delay / portTICK_PERIOD_MS);
            ;
        }
    }

    return ESP_OK;
}

void ssd1306_invert_buffer(uint8_t* buf, size_t blen) {
    uint8_t wk;
    for (uint16_t i = 0; i < blen; i++) {
        wk = buf[i];
        buf[i] = ~wk;
    }
}

uint8_t ssd1306_copy_bit(uint8_t src, uint8_t src_bits, uint8_t dst, uint8_t dst_bits) {
    ESP_LOGD(TAG, "src=%02x src_bits=%d dst=%02x dst_bits=%d", src, src_bits, dst, dst_bits);

    uint8_t smask = 0x01 << src_bits;
    uint8_t dmask = 0x01 << dst_bits;
    uint8_t _src = src & smask;
    uint8_t _dst;

    if (_src != 0) {
        _dst = dst | dmask;  // set bit
    } else {
        _dst = dst & ~(dmask);  // clear bit
    }

    return _dst;
}

// Flip upside down
void ssd1306_flip_buffer(uint8_t* buf, size_t blen) {
    for (uint16_t i = 0; i < blen; i++) {
        buf[i] = ssd1306_rotate_byte(buf[i]);
    }
}

uint8_t ssd1306_rotate_byte(uint8_t ch1) {
    uint8_t ch2 = 0;

    for (int8_t j = 0; j < 8; j++) {
        ch2 = (ch2 << 1) + (ch1 & 0x01);
        ch1 = ch1 >> 1;
    }

    return ch2;
}

esp_err_t ssd1306_display_fadeout(ssd1306_handle_t handle) {
    uint8_t image[1];

    /* validate parameters */
    ESP_ARG_CHECK(handle);

    for (uint8_t page = 0; page < handle->pages; page++) {
        image[0] = 0xFF;
        for (uint8_t line = 0; line < 8; line++) {
            if (handle->dev_config.flip_enabled) {
                image[0] = image[0] >> 1;
            } else {
                image[0] = image[0] << 1;
            }
            for (uint8_t seg = 0; seg < 128; seg++) {
                ESP_RETURN_ON_ERROR(ssd1306_display_image(handle, page, seg, image, 1), TAG, "display image for fadeout failed");
                handle->page[page].segment[seg] = image[0];
            }
        }
    }

    return ESP_OK;
}

static inline esp_err_t ssd1306_setup(ssd1306_handle_t handle) {
    uint8_t out_buf[40];
    uint8_t out_index = 0;

    /* validate parameters */
    ESP_ARG_CHECK(handle);

    out_buf[out_index++] = SSD1306_CONTROL_BYTE_CMD_STREAM;
    out_buf[out_index++] = SSD1306_CMD_DISPLAY_OFF;    // AE
    out_buf[out_index++] = SSD1306_CMD_SET_MUX_RATIO;  // A8
    if (handle->height == 128) out_buf[out_index++] = 0x7F;
    if (handle->height == 64) out_buf[out_index++] = 0x3F;
    if (handle->height == 32) out_buf[out_index++] = 0x1F;
    out_buf[out_index++] = SSD1306_CMD_SET_DISPLAY_OFFSET;  // D3
    out_buf[out_index++] = 0x00;
    out_buf[out_index++] = SSD1306_CMD_SET_DISPLAY_START_LINE;  // 40
    if (handle->dev_config.flip_enabled) {
        out_buf[out_index++] = SSD1306_CMD_SET_SEGMENT_REMAP_0;  // A0
    } else {
        out_buf[out_index++] = SSD1306_CMD_SET_SEGMENT_REMAP_1;  // A1
    }
    out_buf[out_index++] = SSD1306_CMD_SET_COM_SCAN_MODE;    // C8
    out_buf[out_index++] = SSD1306_CMD_SET_DISPLAY_CLK_DIV;  // D5
    out_buf[out_index++] = 0x80;
    out_buf[out_index++] = SSD1306_CMD_SET_COM_PIN_MAP;  // DA 0x12 if height > 32 else 0x02
    if (handle->height == 128) out_buf[out_index++] = 0x12;
    if (handle->height == 64) out_buf[out_index++] = 0x12;
    if (handle->height == 32) out_buf[out_index++] = 0x02;
    out_buf[out_index++] = SSD1306_CMD_SET_CONTRAST;  // 81
    out_buf[out_index++] = 0xFF;
    out_buf[out_index++] = SSD1306_CMD_DISPLAY_RAM;        // A4
    out_buf[out_index++] = SSD1306_CMD_SET_VCOMH_DESELCT;  // DB
    out_buf[out_index++] = 0x40;
    out_buf[out_index++] = SSD1306_CMD_SET_MEMORY_ADDR_MODE;  // 20
    out_buf[out_index++] = SSD1306_CMD_SET_PAGE_ADDR_MODE;    // 02
    // Set Lower Column Start Address for Page Addressing Mode
    out_buf[out_index++] = 0x00;
    // Set Higher Column Start Address for Page Addressing Mode
    out_buf[out_index++] = 0x10;
    out_buf[out_index++] = SSD1306_CMD_SET_CHARGE_PUMP;  // 8D
    out_buf[out_index++] = 0x14;
    out_buf[out_index++] = SSD1306_CMD_DEACTIVE_SCROLL;  // 2E
    out_buf[out_index++] = SSD1306_CMD_DISPLAY_NORMAL;   // A6
    out_buf[out_index++] = SSD1306_CMD_DISPLAY_ON;       // AF

    ESP_RETURN_ON_ERROR(ssd1306_i2c_write(handle, out_buf, out_index), TAG, "write setup configuration failed");

    handle->dev_config.display_enabled = true;

    return ESP_OK;
}

esp_err_t ssd1306_init(i2c_dev_t master_handle, const ssd1306_config_t* ssd1306_config, ssd1306_handle_t* ssd1306_handle) {
    /* validate device exists on the master bus */
    // esp_err_t ret = i2c_dev_probe(&master_handle, I2C_DEV_WRITE);
    // ESP_GOTO_ON_ERROR(ret, err, TAG, "device does not exist at address 0x%02x, ssd1306 device handle initialization failed", ssd1306_config->i2c_address);
    esp_err_t ret = ESP_OK;
    /* validate memory availability for handle */
    ssd1306_handle_t out_handle;
    out_handle = (ssd1306_handle_t)calloc(1, sizeof(*out_handle));
    ESP_GOTO_ON_FALSE(out_handle, ESP_ERR_NO_MEM, err, TAG, "no memory for i2c ssd1306 device, init failed");

    /* copy configuration */
    out_handle->dev_config = *ssd1306_config;
    out_handle->i2c_handle = master_handle;

    /* set panel properties */
    out_handle->width = ssd1306_panel_properties[out_handle->dev_config.panel_size].width;
    out_handle->height = ssd1306_panel_properties[out_handle->dev_config.panel_size].height;
    out_handle->pages = ssd1306_panel_properties[out_handle->dev_config.panel_size].pages;

    /* initialize page and segment buffer */
    for (uint8_t i = 0; i < out_handle->pages; i++) {
        memset(out_handle->page[i].segment, 0, SSD1306_PAGE_SEGMENT_SIZE);
    }

    /* attempt to setup display */
    ESP_GOTO_ON_ERROR(ssd1306_setup(out_handle), err_handle, TAG, "panel setup for init failed");

    /* set device handle */
    *ssd1306_handle = out_handle;

    return ESP_OK;

err_handle:

    free(out_handle);
err:
    return ret;
}

esp_err_t ssd1306_remove(ssd1306_handle_t handle) {
    /* validate parameters */
    ESP_ARG_CHECK(handle);

    return ESP_OK;
}

esp_err_t ssd1306_delete(ssd1306_handle_t handle) {
    /* validate arguments */
    ESP_ARG_CHECK(handle);

    /* remove device from master bus */
    ESP_RETURN_ON_ERROR(ssd1306_remove(handle), TAG, "unable to remove device from i2c master bus, delete handle failed");

    /* validate handle instance and free handles */
    // if (handle->i2c_handle) {
    // free(handle->i2c_handle);
    // free(handle);
    //}

    return ESP_OK;
}

esp_err_t ssd1306_init_desc(i2c_dev_t* dev, uint8_t addr, i2c_port_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio) {
    dev->port = port;
    dev->addr = addr;
    dev->cfg.sda_io_num = sda_gpio;
    dev->cfg.scl_io_num = scl_gpio;
#if HELPER_TARGET_IS_ESP32
    dev->cfg.master.clk_speed = I2C_SSD1306_DEV_CLK_SPD;
#endif
    return i2c_dev_create_mutex(dev);
}