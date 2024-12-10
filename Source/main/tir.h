#ifndef __TIR_H
#define __TIR_H

#include <stdint.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"

enum irproto {
    UNK,
    NEC,
    NECEXT,
    SONY,
    SAM,
    RC5,
    PROTO_COUNT
};

typedef struct ir_data {
    irproto protocol;
    uint64_t data;
    uint8_t repeat;
} ir_data_t;

typedef struct
{
    uint16_t header_high;
    uint16_t header_low;
    uint16_t one_high;
    uint16_t one_low;
    uint16_t zero_high;
    uint16_t zero_low;
    uint16_t footer_high;
    uint8_t footer_low;
    uint16_t frequency;
    const char* name;
    uint8_t bits;
} ir_protocol_t;

typedef struct
{
    rmt_encoder_t base;
    rmt_encoder_t* copy_encoder;
    uint8_t bit_index;
    int state;
} rmt_ir_encoder_t;

class TIR {
   public:
    TIR();
    ~TIR();
    void init(gpio_num_t tx, gpio_num_t rx);  // initializes ir rx,tx

    void send(irproto protocol, uint32_t addr, uint32_t cmd);  // add data to a send queut, that is parsed from a thread that manages sending. queue max size is 5
    void send(irproto protocol, uint64_t data);                // add data to a send queut, that is parsed from a thread that manages sending. queue max size is 5
    void send(ir_data_t data);                                 // add data to a send queut, that is parsed from a thread that manages sending. queue max size is 5
    void send_from_irq(ir_data_t data);

   private:
    static void create_symbol(rmt_symbol_word_t& item, uint16_t high, uint16_t low, bool bit);
    static size_t rmt_encode_ir(rmt_encoder_t* encoder, rmt_channel_handle_t channel, const void* primary_data, size_t data_size, rmt_encode_state_t* ret_state);
    static void processSendTask(void* pvParameters);
    static const ir_protocol_t proto[PROTO_COUNT];
    static QueueHandle_t sendQueue;
    static gpio_num_t tx_pin;
};

#endif