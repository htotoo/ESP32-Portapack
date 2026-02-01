#pragma once
#include "esp_err.h"
#include "pinconfig.h"
#include "MtCompact.hpp"
// todo do something with txco and ldo

Radio_PINS radio_pins = {
    /* sck*/ 35,
    /* miso*/ 33,
    /* mosi*/ 34,
    /* cs*/ 36,
    /* irq*/ 14,
    /* rst*/ 47,
    /* gpio*/ 48};  // T_pager.
LoraConfig lora_config = {
    /*.frequency = */ 433.125,    // config
    /*.bandwidth = */ 250,        // config
    /*.spreading_factor = */ 11,  // config
    /*.coding_rate = */ 5,        // config
    /*.sync_word = */ 0x2b,
    /*.preamble_length = */ 16,
    /*.output_power = */ 12,  // config
    /*.tcxo_voltage = */ 3.0,
    /*.use_regulator_ldo = */ true,
};  // default LoRa configuration for EU MFFAST 868
MtCompact mtCompact;

bool initLora(PinConfig& pinConfig) {
    pinConfig.debugPrint();
    radio_pins.cs = pinConfig.LoraNssPin();
    radio_pins.rst = 39;   // pinConfig.LoraResetPin();
    radio_pins.irq = 40;   // pinConfig.LoraDio0Pin();
    radio_pins.gpio = 41;  // pinConfig.LoraDio1Pin();
    radio_pins.sck = pinConfig.SpiSckPin();
    radio_pins.miso = pinConfig.SpiMisoPin();
    radio_pins.mosi = pinConfig.SpiMosiPin();
    // todo load settings from mtCompact, like lora config
    if (!mtCompact.RadioInit((RadioType)5, radio_pins, lora_config)) return false;
    mtCompact.setMyNames("PP32", "PortaPack32");
    mtCompact.setDebugMode(true);
    mtCompact.loadNodeDb();
    mtCompact.setOkToMqtt(true);
    mtCompact.setAutoFullNode(true);
    mtCompact.setSendEnabled(true);
    mtCompact.setSendHopLimit(7);
    mtCompact.sendMyNodeInfo();
    return true;
}