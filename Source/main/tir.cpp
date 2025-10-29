#include "tir.h"
#include "esp_log.h"
QueueHandle_t TIR::sendQueue;
gpio_num_t TIR::tx_pin;
gpio_num_t TIR::rx_pin;
bool TIR::irTX;
void (*TIR::ir_callback)(irproto proto, uint64_t rcode, size_t len);

const ir_protocol_t TIR::proto[IR_PROTO_COUNT] = {
    [UNK] = {0, 0, 0, 0, 0, 0, 0, 0, 0, "UNK", 32},
    [NEC] = {9000, 4500, 560, 1690, 560, 560, 560, 0, 38000, "NEC", 32},
    [NECEXT] = {9000, 4500, 560, 1690, 560, 560, 560, 0, 38000, "NECEXT", 32},
    [SONY] = {2400, 600, 1200, 600, 600, 600, 0, 0, 40000, "SONY", 32},
    [SAM] = {4500, 4500, 560, 1690, 560, 560, 560, 0, 38000, "SAM", 32},
    [RC5] = {0, 0, 889, 889, 889, 889, 0, 0, 38000, "RC5", 32},
};

TIR::TIR() {
}

TIR::~TIR() {
}

void TIR::set_on_ir_received(void (*callback)(irproto proto, uint64_t rcode, size_t len)) {
    ir_callback = callback;
}

void TIR::init(gpio_num_t tx, gpio_num_t rx) {
    irTX = false;
    tx_pin = tx;
    rx_pin = rx;
    // init rx
    if (rx > 0) {
        xTaskCreate(recvIRTask, "processIRRxTask", 4096, NULL, 5, NULL);
    }

    if (tx_pin > 0) {
        sendQueue = xQueueCreate(5, sizeof(ir_data_t));
        xTaskCreate(processSendTask, "processIRSendTask", 4096, NULL, 5, NULL);
    }
}

void TIR::recvIRTask(void* param) {
    rmt_rx_done_event_data_t rx_data;
    QueueHandle_t rx_queue = xQueueCreate(1, sizeof(rx_data));

    for (;;) {
        if (irTX) {
            vTaskDelay(300 / portTICK_PERIOD_MS);
            continue;
        }
        rmt_channel_handle_t rx_channel = NULL;
        rmt_symbol_word_t symbols[64];

        rmt_receive_config_t rx_config = {
            .signal_range_min_ns = 1250,
            .signal_range_max_ns = 12000000,
        };

        rmt_rx_channel_config_t rx_ch_conf = {
            .gpio_num = static_cast<gpio_num_t>(rx_pin),
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .resolution_hz = 1000000,
            .mem_block_symbols = 64,
            .intr_priority = 0,
        };

        rmt_new_rx_channel(&rx_ch_conf, &rx_channel);
        rmt_rx_event_callbacks_t cbs = {
            .on_recv_done = irrx_done,
        };

        rmt_rx_register_event_callbacks(rx_channel, &cbs, rx_queue);
        rmt_enable(rx_channel);
        rmt_receive(rx_channel, symbols, sizeof(symbols), &rx_config);
        for (;;) {
            if (irTX) {
                break;
            }
            if (xQueueReceive(rx_queue, &rx_data, pdMS_TO_TICKS(1000)) == pdPASS) {
                size_t len = rx_data.num_symbols;
                rmt_symbol_word_t* rx_items = rx_data.received_symbols;

                if (len > 11) {
                    uint32_t rcode = 0;
                    irproto rproto = UNK;
                    if ((rcode = nec_check(rx_items, len))) {
                        rproto = NEC;
                        // } else if (rcode = necext_check(rx_items, len)) {
                        //     rproto = NECEXT;
                    } else if ((rcode = sony_check(rx_items, len))) {
                        rproto = SONY;
                    } else if ((rcode = sam_check(rx_items, len))) {
                        rproto = SAM;
                    } else if ((rcode = rc5_check(rx_items, len))) {
                        rproto = RC5;
                    }
                    if (ir_callback && rproto) {
                        ir_callback(rproto, rcode, len);
                    }
                    ESP_LOGI("IR", "Ir rx: %zu %" PRIu32 " %zu", rproto, rcode, len);
                }

                rmt_receive(rx_channel, symbols, sizeof(symbols), &rx_config);
            }
        }
        rmt_disable(rx_channel);
        rmt_del_channel(rx_channel);
    }
    vQueueDelete(rx_queue);
    vTaskDelete(NULL);
}

bool TIR::irrx_done(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t* edata, void* udata) {
    BaseType_t h = pdFALSE;
    QueueHandle_t q = (QueueHandle_t)udata;
    xQueueSendFromISR(q, edata, &h);
    return h == pdTRUE;
}

void TIR::send(irproto protocol, uint32_t addr, uint32_t cmd) {
    ir_data_t data;
    data.protocol = protocol;
    data.repeat = 1;
    if (protocol == NEC) {
        int8_t address = (int8_t)addr;
        uint8_t address_inverse = ~address;
        uint8_t command = (uint8_t)cmd;
        uint8_t command_inverse = ~command;
        data.data = address;
        data.data |= address_inverse << 8;
        data.data |= command << 16;
        data.data |= command_inverse << 24;
    } else if (protocol == NECEXT) {
        data.data = (uint16_t)addr;
        data.data |= (cmd & 0xFFFF) << 16;
    } else {
        ESP_LOGI("IR", "NOT SUPPORTED PROTOCOL, ADD DATA DIRECTLY");
        return;
    }
    send(data);
}

void TIR::send(irproto protocol, uint64_t data_) {
    ir_data_t data;
    data.protocol = protocol;
    data.data = data_;
    data.repeat = 1;
    send(data);
}

void TIR::send(ir_data_t data) {
    xQueueSend(sendQueue, &data, portMAX_DELAY);
}

void TIR::send_from_irq(ir_data_t data) {
    auto ttt = pdFALSE;
    xQueueSendFromISR(sendQueue, &data, &ttt);
}

void TIR::create_symbol(rmt_symbol_word_t& item, uint16_t high, uint16_t low, bool bit) {
    item.level0 = !bit;
    item.duration0 = high;
    item.level1 = bit;
    item.duration1 = low;
}

size_t TIR::rmt_encode_ir(rmt_encoder_t* encoder, rmt_channel_handle_t channel, const void* primary_data, size_t data_size, rmt_encode_state_t* ret_state) {
    rmt_ir_encoder_t* ir_encoder = __containerof(encoder, rmt_ir_encoder_t, base);

    rmt_encode_state_t fstate = RMT_ENCODING_RESET;

    size_t encoded_symbols = 0;
    rmt_encoder_handle_t copy_encoder = ir_encoder->copy_encoder;

    ir_data_t* send_data = (ir_data_t*)primary_data;

    ir_protocol_t protocol = proto[send_data->protocol];

    if (ir_encoder->state == 0) {
        if (protocol.header_high > 0) {
            rmt_symbol_word_t header_symbol;
            create_symbol(header_symbol, protocol.header_high, protocol.header_low, 0);
            encoded_symbols += copy_encoder->encode(copy_encoder, channel, &header_symbol, sizeof(rmt_symbol_word_t), &fstate);
        } else {
            fstate = RMT_ENCODING_COMPLETE;
        }

        if (fstate & RMT_ENCODING_COMPLETE) {
            ir_encoder->state = 1;
            ir_encoder->bit_index = 0;
        }
        if (fstate & RMT_ENCODING_MEM_FULL) {
            fstate = RMT_ENCODING_MEM_FULL;
            *ret_state = fstate;
            return encoded_symbols;
        }
    }

    if (ir_encoder->state == 1) {
        uint8_t rcspecial = 0;
        if (send_data->protocol == RC5) {
            rcspecial = 1;
        }

        rmt_symbol_word_t one_symbol;
        create_symbol(one_symbol, protocol.one_high, protocol.one_low, rcspecial);

        rmt_symbol_word_t zero_symbol;
        create_symbol(zero_symbol, protocol.zero_high, protocol.zero_low, 0);

        for (uint8_t i = ir_encoder->bit_index; i < protocol.bits; i++) {
            if ((send_data->data >> i) & 1) {
                encoded_symbols += copy_encoder->encode(copy_encoder, channel, &one_symbol, sizeof(rmt_symbol_word_t), &fstate);
            } else {
                encoded_symbols += copy_encoder->encode(copy_encoder, channel, &zero_symbol, sizeof(rmt_symbol_word_t), &fstate);
            }
            if (fstate & RMT_ENCODING_MEM_FULL) {
                fstate = RMT_ENCODING_MEM_FULL;
                ir_encoder->bit_index = i + 1;
                *ret_state = fstate;
                return encoded_symbols;
            }
        }
        ir_encoder->state = 2;
    }

    if (ir_encoder->state == 2) {
        if (protocol.footer_high > 0) {
            rmt_symbol_word_t end_symbol;
            create_symbol(end_symbol, protocol.footer_high, protocol.footer_low, 0);
            encoded_symbols += copy_encoder->encode(copy_encoder, channel, &end_symbol, sizeof(rmt_symbol_word_t), &fstate);
        } else {
            fstate = (rmt_encode_state_t)((int)fstate | RMT_ENCODING_COMPLETE);
        }
        if (fstate & RMT_ENCODING_COMPLETE) {
            ir_encoder->state = 0;
            *ret_state = RMT_ENCODING_COMPLETE;
        }
    }

    return encoded_symbols;
}

esp_err_t rmt_del_ir_encoder(rmt_encoder_t* encoder) {
    rmt_ir_encoder_t* ir_encoder = __containerof(encoder, rmt_ir_encoder_t, base);
    rmt_del_encoder(ir_encoder->copy_encoder);
    free(ir_encoder);
    return ESP_OK;
}

esp_err_t rmt_ir_encoder_reset(rmt_encoder_t* encoder) {
    rmt_ir_encoder_t* ir_encoder = __containerof(encoder, rmt_ir_encoder_t, base);
    rmt_encoder_reset(ir_encoder->copy_encoder);
    ir_encoder->state = 0;
    ir_encoder->bit_index = 0;
    return ESP_OK;
}

void TIR::processSendTask(void* pvParameters) {
    ir_data_t receivedData;
    while (1) {
        if (xQueueReceive(sendQueue, &receivedData, portMAX_DELAY) == pdTRUE) {
            if (receivedData.protocol >= IR_PROTO_COUNT) {
                ESP_LOGW("IR", "INVALID PROTOCOL");
                continue;
            }
            // wait for rx to finish. todo
            rmt_channel_handle_t tx_channel = NULL;

            rmt_tx_channel_config_t txconf = {
                .gpio_num = static_cast<gpio_num_t>(tx_pin),
                .clk_src = RMT_CLK_SRC_DEFAULT,
                .resolution_hz = 1000000,  // 1MHz resolution, 1 tick = 1us
                .mem_block_symbols = 64,
                .trans_queue_depth = 4,
                .intr_priority = 0,

                .flags = {
                    .invert_out = 0,  // do not invert output signal
                    .with_dma = 0,
                    .io_loop_back = 0,
                    .io_od_mode = 0,
                }  // do not need DMA backend
            };
            if (rmt_new_tx_channel(&txconf, &tx_channel) != ESP_OK) {
                continue;
            }

            // todo overwrite with protocol data
            float duty = 0.50;
            uint8_t rptgap = 100;
            if (receivedData.protocol == SONY) {
                rptgap = 24;
                duty = 0.40;
            }

            rmt_carrier_config_t carrier_cfg = {
                .frequency_hz = proto[receivedData.protocol].frequency,
                .duty_cycle = duty,

                .flags = {
                    .polarity_active_low = 0,
                    .always_on = 0}};

            rmt_apply_carrier(tx_channel, &carrier_cfg);
            rmt_ir_encoder_t* ir_encoder = (rmt_ir_encoder_t*)calloc(1, sizeof(rmt_ir_encoder_t));
            ir_encoder->base.encode = rmt_encode_ir;
            ir_encoder->base.del = rmt_del_ir_encoder;
            ir_encoder->base.reset = rmt_ir_encoder_reset;

            rmt_copy_encoder_config_t copy_encoder_config = {};
            rmt_new_copy_encoder(&copy_encoder_config, &ir_encoder->copy_encoder);

            rmt_encoder_handle_t encoder_handle = &ir_encoder->base;

            rmt_enable(tx_channel);

            rmt_transmit_config_t tx_config = {
                .loop_count = 0,
                .flags = {
                    .eot_level = 0,
                    .queue_nonblocking = 0,
                }};

            uint8_t burst = 1;  // not really needed

            for (uint8_t j = 0; j < receivedData.repeat; j++) {
                for (uint8_t i = 0; i < burst; i++) {
                    rmt_transmit(tx_channel, encoder_handle, &receivedData, sizeof(ir_data_t), &tx_config);
                    rmt_tx_wait_all_done(tx_channel, portMAX_DELAY);
                    vTaskDelay(rptgap / portTICK_PERIOD_MS);
                }
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }

            rmt_disable(tx_channel);
            rmt_del_channel(tx_channel);
            rmt_del_encoder(encoder_handle);
        }
    }
}

uint32_t TIR::nec_check(rmt_symbol_word_t* item, size_t& len) {
    const uint8_t totalData = 34;
    if (len < totalData) {
        return 0;
    }
    const uint32_t m = 0x80000000;
    uint32_t code = 0;
    for (uint8_t i = 0; i < totalData; i++) {
        if (i == 0) {  // header
            if (!checkbit(item[i], proto[NEC].header_high, proto[NEC].header_low)) {
                return 0;
            }
        } else if (i == 33) {  // footer
            if (!checkbit(item[i], proto[NEC].footer_high, proto[NEC].footer_low)) {
                return 0;
            }
        } else if (checkbit(item[i], proto[NEC].one_high, proto[NEC].one_low)) {
            code |= (m >> (i - 1));
        } else if (!checkbit(item[i], proto[NEC].zero_high, proto[NEC].zero_low)) {
            // Serial.printf("BitError i:%d\n",i);
            return 0;
        }
    }
    return code;
}

uint32_t TIR::sam_check(rmt_symbol_word_t* item, size_t& len) {
    const uint8_t totalData = 34;
    if (len < totalData) {
        return 0;
    }
    const uint32_t m = 0x80000000;
    uint32_t code = 0;
    for (uint8_t i = 0; i < totalData; i++) {
        if (i == 0) {  // header
            if (!checkbit(item[i], proto[SAM].header_high, proto[SAM].header_low)) {
                return 0;
            }
        } else if (i == 33) {  // footer
            if (!checkbit(item[i], proto[SAM].footer_high, proto[SAM].footer_low)) {
                return 0;
            }
        } else if (checkbit(item[i], proto[SAM].one_high, proto[SAM].one_low)) {
            code |= (m >> (i - 1));
        } else if (!checkbit(item[i], proto[SAM].zero_high, proto[SAM].zero_low)) {
            // Serial.printf("BitError i:%d\n",i);
            return 0;
        }
    }
    return code;
}

uint32_t TIR::sony_check(rmt_symbol_word_t* item, size_t& len) {
    const uint8_t totalMin = 12;
    uint8_t i = 0;
    if (len < totalMin || !checkbit(item[i], proto[SONY].header_high, proto[SONY].header_low)) {
        return 0;
    }
    i++;
    uint32_t m = 0x80000000;
    uint32_t code = 0;
    uint8_t maxData = 20;
    if (len < maxData) {
        maxData = 15;
    }
    if (len < maxData) {
        maxData = 12;
    }
    for (int j = 0; j < maxData - 1; j++) {
        if (checkbit(item[i], proto[SONY].one_high, proto[SONY].one_low)) {
            code |= (m >> j);
        } else if (checkbit(item[i], proto[SONY].zero_high, proto[SONY].zero_low)) {
            code |= 0 & (m >> j);
        } else if (item[i].duration1 > 15000) {  // space repeats
            // len = i+1;
            break;
        } else {
            // Serial.printf("BitError %d, max:%d, j:%d\n",i, maxData, j); Serial.printf("d0:%d, d1:%d, L0:%d L1:%d\n", item[i].duration0, item[i].duration1, item[i].level0, item[i].level1);
            return 0;
        }
        i++;
    }
    code = code >> (32 - i);
    len = i + 1;
    return code;
}

uint32_t TIR::rc5_check(rmt_symbol_word_t* item, size_t& len) {
    if (len < 13 || len > 30) {
        return 0;
    }
    const uint16_t RC5_High = proto[RC5].one_high + proto[RC5].one_low;
    uint32_t code = 0;
    bool c = false;
    for (uint8_t i = 0; i < len; i++) {
        uint32_t d0 = item[i].duration0;
        uint32_t d1 = item[i].duration1;
        if (rc5_bit(d0, proto[RC5].one_low)) {
            code = (code << 1) | c;
            c = rc5_bit(d1, RC5_High) ? !c : c;
        } else if (rc5_bit(d0, RC5_High)) {
            code = (code << 2) | (item[i].level0 << 1) | !item[i].level0;
            c = rc5_bit(d1, proto[RC5].one_low) ? !c : c;
        } else {
            // Serial.printf("BitError i:%d\n",i);
            return 0;
        }
    }
    return code;
}

bool TIR::rc5_bit(uint32_t d, uint32_t v) {
    return (d < (v + bitMargin)) && (d > (v - bitMargin));
}

bool TIR::checkbit(rmt_symbol_word_t& item, uint16_t high, uint16_t low) {
    return item.level0 == 0 && item.level1 != 0 &&
           item.duration0 < (high + bitMargin) && item.duration0 > (high - bitMargin) &&
           item.duration1 < (low + bitMargin) && item.duration1 > (low - bitMargin);
}