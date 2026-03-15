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
    /* irq*/ 40,
    /* rst*/ 39,
    /* gpio*/ 41};  // T_pager.
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

using OnLoraMessageCallback = void (*)(std::string& sender, std::string& chan, std::string& message);
OnLoraMessageCallback onLoraMessageCallback = nullptr;

void on_message_int(MCT_Header& header, MCT_TextMessage& message) {
    MCT_NodeInfo* nodeinfo = mtCompact.nodeinfo_db.get(header.srcnode);
    std::string sender;
    if (nodeinfo) {
        sender = nodeinfo->short_name;
    } else {
        char hexbuf[11];
        snprintf(hexbuf, sizeof(hexbuf), "0x%08" PRIx32, header.srcnode);
        sender = hexbuf;
    }

    std::string chan = (header.dstnode == 0xffffffff) ? std::to_string(message.chan) : "PRIV";
    if (onLoraMessageCallback) {
        onLoraMessageCallback(sender, chan, message.text);
    }
}

bool initLora(PinConfig& pinConfig) {
    pinConfig.debugPrint();
    radio_pins.cs = pinConfig.LoraNssPin();
    radio_pins.rst = pinConfig.LoraResetPin();
    radio_pins.irq = pinConfig.LoraDio0Pin();
    radio_pins.gpio = pinConfig.LoraDio1Pin();
    radio_pins.sck = pinConfig.SpiSckPin();
    radio_pins.miso = pinConfig.SpiMisoPin();
    radio_pins.mosi = pinConfig.SpiMosiPin();
    // todo load settings from mtCompact, like lora config
    if (!mtCompact.RadioInit((RadioType)5, radio_pins, lora_config)) return false;
    mtCompact.setMyNames("PP32", "PortaPack32");
    uint8_t my_p_key[32] = {0xf0, 0x69, 0x3d, 0xfd, 0x3e, 0x2c, 0x68, 0xcb, 0xb3, 0xcc, 0x09, 0xe6, 0xdb, 0x6c, 0xe0, 0x6a, 0xf7, 0xea, 0x33, 0xaa, 0x3c, 0xdf, 0xde, 0xaf, 0xd3, 0xaa, 0xe5, 0x0c, 0x22, 0xba, 0x0b, 0x74};
    memcpy(mtCompact.getMyNodeInfo()->private_key, my_p_key, 32);
    MtCompactHelpers::RegenerateOrGeneratePrivateKey(*mtCompact.getMyNodeInfo());
    // mtCompact.setPrimaryChanByHash(8);
    mtCompact.setDebugMode(true);
    mtCompact.loadNodeDb();
    mtCompact.setOkToMqtt(true);
    mtCompact.setAutoFullNode(true);
    mtCompact.setSendEnabled(true);
    mtCompact.setSendHopLimit(7);
    mtCompact.sendMyNodeInfo();
    mtCompact.setOnMessage(on_message_int);
    return true;
}

void lora_set_onmessage(OnLoraMessageCallback cb) {
    onLoraMessageCallback = cb;
}