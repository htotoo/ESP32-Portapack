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
 * @file type_utils.c
 *
 * ESP-IDF type utilities (utils)
 *
 * Ported from esp-open-rtos
 *
 * Copyright (c) 2024 Eric Gionet (gionet.c.eric@gmail.com)
 *
 * MIT Licensed as described in the file LICENSE
 */

#include "type_utils.h"
#include <string.h>
#include <stdio.h>

/*
 * macro definitions
 */

/*
 * static declarations
 */

/*
 * static constant declarations
 */

// static const char* TAG = "type_utils";

/*
 * functions and subroutines
 */

uint32_t get_uint32_chip_id(void) {
    uint32_t chipid = 0L;
    for (int i = 0; i < 17; i = i + 8) {
        chipid |= ((get_efuse_mac() >> (40 - i)) & 0xff) << i;
    }
    return chipid;
}

uint64_t get_uint64_chip_id(void) {
    uint64_t chipid = 0LL;
    for (int i = 0; i < 63; i = i + 8) {
        chipid |= ((get_efuse_mac() >> (56 - i)) & 0xff) << i;
    }
    return chipid;
}

uint64_t get_efuse_mac(void) {
    uint64_t chipmacid = 0LL;
    esp_efuse_mac_get_default((uint8_t*)(&chipmacid));
    return chipmacid;
}

const char* uint8_to_binary(const uint8_t value) {
    static bin8_char_buffer_t buffer;
    buffer[8] = '\0';
    uint8_t n = value;

    for (int i = 7; i >= 0; --i) {
        buffer[i] = '0' + (n & 1);  // '0' or '1'
        n >>= 1;                    // shift to the next bit
    }

    return buffer;
}

const char* int8_to_binary(const int8_t value) {
    static bin8_char_buffer_t buffer;
    buffer[8] = '\0';
    int8_t n = value;

    for (int i = 7; i >= 0; --i) {
        buffer[i] = '0' + (n & 1);  // '0' or '1'
        n >>= 1;                    // shift to the next bit
    }

    return buffer;
}

const char* uint16_to_binary(const uint16_t value) {
    static bin16_char_buffer_t buffer;
    buffer[16] = '\0';
    uint16_t n = value;

    for (int i = 15; i >= 0; --i) {
        buffer[i] = '0' + (n & 1);  // '0' or '1'
        n >>= 1;                    // shift to the next bit
    }

    return buffer;
}

const char* int16_to_binary(const int16_t value) {
    static bin16_char_buffer_t buffer;
    buffer[16] = '\0';
    int16_t n = value;

    for (int i = 15; i >= 0; --i) {
        buffer[i] = '0' + (n & 1);  // '0' or '1'
        n >>= 1;                    // shift to the next bit
    }

    return buffer;
}

const char* uint32_to_binary(const uint32_t value) {
    static bin32_char_buffer_t buffer;
    buffer[32] = '\0';
    uint32_t n = value;

    for (int i = 31; i >= 0; --i) {
        buffer[i] = '0' + (n & 1);  // '0' or '1'
        n >>= 1;                    // shift to the next bit
    }

    return buffer;
}

const char* int32_to_binary(const int32_t value) {
    static bin32_char_buffer_t buffer;
    buffer[32] = '\0';
    int32_t n = value;

    for (int i = 31; i >= 0; --i) {
        buffer[i] = '0' + (n & 1);  // '0' or '1'
        n >>= 1;                    // shift to the next bit
    }

    return buffer;
}

const char* uint64_to_binary(const uint64_t value) {
    static bin64_char_buffer_t buffer;
    buffer[64] = '\0';
    uint64_t n = value;

    for (int i = 63; i >= 0; --i) {
        buffer[i] = '0' + (n & 1);  // '0' or '1'
        n >>= 1;                    // shift to the next bit
    }

    return buffer;
}

const char* int64_to_binary(const int64_t value) {
    static bin64_char_buffer_t buffer;
    buffer[64] = '\0';
    int64_t n = value;

    for (int i = 63; i >= 0; --i) {
        buffer[i] = '0' + (n & 1);  // '0' or '1'
        n >>= 1;                    // shift to the next bit
    }

    return buffer;
}

uint16_t bytes_to_uint16(const uint8_t* bytes, const bool little_endian) {
    if (little_endian == true) {
        return (uint16_t)(bytes[0] |
                          ((uint16_t)bytes[1] << 8));
    } else {
        return (uint16_t)(((uint16_t)bytes[0] << 8) |
                          bytes[1]);
    }
}

uint32_t bytes_to_uint32(const uint8_t* bytes, const bool little_endian) {
    if (little_endian == true) {
        return (uint32_t)bytes[0] |
               ((uint32_t)bytes[1] << 8) |
               ((uint32_t)bytes[2] << 16) |
               ((uint32_t)bytes[3] << 24);
    } else {
        return ((uint32_t)bytes[0] << 24) |
               ((uint32_t)bytes[1] << 16) |
               ((uint32_t)bytes[2] << 8) |
               (uint32_t)bytes[3];
    }
}

uint64_t bytes_to_uint64(const uint8_t* bytes, const bool little_endian) {
    if (little_endian == true) {
        return (uint64_t)bytes[0] |
               ((uint64_t)bytes[1] << 8) |
               ((uint64_t)bytes[2] << 16) |
               ((uint64_t)bytes[3] << 24) |
               ((uint64_t)bytes[4] << 32) |
               ((uint64_t)bytes[5] << 40) |
               ((uint64_t)bytes[6] << 48) |
               ((uint64_t)bytes[7] << 56);
    } else {
        return ((uint64_t)bytes[0] << 56) |
               ((uint64_t)bytes[1] << 48) |
               ((uint64_t)bytes[2] << 40) |
               ((uint64_t)bytes[3] << 32) |
               ((uint64_t)bytes[4] << 24) |
               ((uint64_t)bytes[5] << 16) |
               ((uint64_t)bytes[6] << 8) |
               (uint64_t)bytes[7];
    }
}

int16_t bytes_to_int16(const uint8_t* bytes, const bool little_endian) {
    if (little_endian == true) {
        return (int16_t)(bytes[0] |
                         ((int16_t)bytes[1] << 8));
    } else {
        return (int16_t)(((int16_t)bytes[0] << 8) | bytes[1]);
    }
}

int32_t bytes_to_int32(const uint8_t* bytes, const bool little_endian) {
    if (little_endian == true) {
        return (int32_t)bytes[0] |
               ((int32_t)bytes[1] << 8) |
               ((int32_t)bytes[2] << 16) |
               ((int32_t)bytes[3] << 24);
    } else {
        return ((int32_t)bytes[0] << 24) |
               ((int32_t)bytes[1] << 16) |
               ((int32_t)bytes[2] << 8) |
               (int32_t)bytes[3];
    }
}

int64_t bytes_to_int64(const uint8_t* bytes, const bool little_endian) {
    if (little_endian == true) {
        return (int64_t)bytes[0] |
               ((int64_t)bytes[1] << 8) |
               ((int64_t)bytes[2] << 16) |
               ((int64_t)bytes[3] << 24) |
               ((int64_t)bytes[4] << 32) |
               ((int64_t)bytes[5] << 40) |
               ((int64_t)bytes[6] << 48) |
               ((int64_t)bytes[7] << 56);
    } else {
        return ((int64_t)bytes[0] << 56) |
               ((int64_t)bytes[1] << 48) |
               ((int64_t)bytes[2] << 40) |
               ((int64_t)bytes[3] << 32) |
               ((int64_t)bytes[4] << 24) |
               ((int64_t)bytes[5] << 16) |
               ((int64_t)bytes[6] << 8) |
               (int64_t)bytes[7];
    }
}

void uint16_to_bytes(const uint16_t value, uint8_t* bytes, const bool little_endian) {
    if (little_endian == true) {
        bytes[0] = (uint8_t)(value & 0xff);         // lsb
        bytes[1] = (uint8_t)((value >> 8) & 0xff);  // msb
    } else {
        bytes[0] = (uint8_t)((value >> 8) & 0xff);  // msb
        bytes[1] = (uint8_t)(value & 0xff);         // lsb
    }
}

void uint32_to_bytes(const uint32_t value, uint8_t* bytes, const bool little_endian) {
    if (little_endian == true) {
        bytes[0] = (uint8_t)(value & 0xff);
        bytes[1] = (uint8_t)((value >> 8) & 0xff);
        bytes[2] = (uint8_t)((value >> 16) & 0xff);
        bytes[3] = (uint8_t)((value >> 24) & 0xff);
    } else {
        bytes[0] = (uint8_t)((value >> 24) & 0xff);
        bytes[1] = (uint8_t)((value >> 16) & 0xff);
        bytes[2] = (uint8_t)((value >> 8) & 0xff);
        bytes[3] = (uint8_t)(value & 0xff);
    }
}

void uint64_to_bytes(const uint64_t value, uint8_t* bytes, const bool little_endian) {
    if (little_endian == true) {
        bytes[0] = (uint8_t)(value & 0xff);
        bytes[1] = (uint8_t)((value >> 8) & 0xff);
        bytes[2] = (uint8_t)((value >> 16) & 0xff);
        bytes[3] = (uint8_t)((value >> 24) & 0xff);
        bytes[4] = (uint8_t)((value >> 32) & 0xff);
        bytes[5] = (uint8_t)((value >> 40) & 0xff);
        bytes[6] = (uint8_t)((value >> 48) & 0xff);
        bytes[7] = (uint8_t)((value >> 56) & 0xff);
    } else {
        bytes[0] = (uint8_t)((value >> 56) & 0xff);
        bytes[1] = (uint8_t)((value >> 48) & 0xff);
        bytes[2] = (uint8_t)((value >> 40) & 0xff);
        bytes[3] = (uint8_t)((value >> 32) & 0xff);
        bytes[4] = (uint8_t)((value >> 24) & 0xff);
        bytes[5] = (uint8_t)((value >> 16) & 0xff);
        bytes[6] = (uint8_t)((value >> 8) & 0xff);
        bytes[7] = (uint8_t)(value & 0xff);
    }
}

void int16_to_bytes(const int16_t value, uint8_t* bytes, const bool little_endian) {
    if (little_endian == true) {
        bytes[0] = (uint8_t)(value & 0xff);         // lsb
        bytes[1] = (uint8_t)((value >> 8) & 0xff);  // msb
    } else {
        bytes[0] = (uint8_t)((value >> 8) & 0xff);  // msb
        bytes[1] = (uint8_t)(value & 0xff);         // lsb
    }
}

void int32_to_bytes(const int32_t value, uint8_t* bytes, const bool little_endian) {
    if (little_endian == true) {
        bytes[0] = (uint8_t)(value & 0xff);
        bytes[1] = (uint8_t)((value >> 8) & 0xff);
        bytes[2] = (uint8_t)((value >> 16) & 0xff);
        bytes[3] = (uint8_t)((value >> 24) & 0xff);
    } else {
        bytes[0] = (uint8_t)((value >> 24) & 0xff);
        bytes[1] = (uint8_t)((value >> 16) & 0xff);
        bytes[2] = (uint8_t)((value >> 8) & 0xff);
        bytes[3] = (uint8_t)(value & 0xff);
    }
}

void int64_to_bytes(const int64_t value, uint8_t* bytes, const bool little_endian) {
    if (little_endian == true) {
        bytes[0] = (uint8_t)(value & 0xff);
        bytes[1] = (uint8_t)((value >> 8) & 0xff);
        bytes[2] = (uint8_t)((value >> 16) & 0xff);
        bytes[3] = (uint8_t)((value >> 24) & 0xff);
        bytes[4] = (uint8_t)((value >> 32) & 0xff);
        bytes[5] = (uint8_t)((value >> 40) & 0xff);
        bytes[6] = (uint8_t)((value >> 48) & 0xff);
        bytes[7] = (uint8_t)((value >> 56) & 0xff);
    } else {
        bytes[0] = (uint8_t)((value >> 56) & 0xff);
        bytes[1] = (uint8_t)((value >> 48) & 0xff);
        bytes[2] = (uint8_t)((value >> 40) & 0xff);
        bytes[3] = (uint8_t)((value >> 32) & 0xff);
        bytes[4] = (uint8_t)((value >> 24) & 0xff);
        bytes[5] = (uint8_t)((value >> 16) & 0xff);
        bytes[6] = (uint8_t)((value >> 8) & 0xff);
        bytes[7] = (uint8_t)(value & 0xff);
    }
}

void float_to_bytes(const float value, uint8_t* bytes, const bool little_endian) {
    const union {
        uint32_t u32_value;
        float float32;
    } tmp = {.float32 = value};
    if (little_endian == true) {
        uint32_to_bytes(tmp.u32_value, bytes, true);
    } else {
        uint32_to_bytes(tmp.u32_value, bytes, false);
    }
}

void double_to_bytes(const double value, uint8_t* bytes, const bool little_endian) {
    const union {
        uint64_t u64_value;
        double double64;
    } tmp = {.double64 = value};
    if (little_endian == true) {
        uint64_to_bytes(tmp.u64_value, bytes, true);
    } else {
        uint64_to_bytes(tmp.u64_value, bytes, false);
    }
}

void copy_bytes(const uint8_t* source, uint8_t* destination, const size_t size) {
    memcpy(destination, source, size);
}
