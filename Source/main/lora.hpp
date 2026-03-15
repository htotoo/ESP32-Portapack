#pragma once
#include "esp_err.h"
#include "pinconfig.h"
#include "MtCompact.hpp"
// todo do something with txco and ldo
bool loraInited = false;

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

void on_nodeinfo_int(MCT_Header& header, MCT_NodeInfo& nodeinfo, bool needReply, bool newNode) {
    std::string peers_json = "#$##$$#GOTLORAPEERS[";
    char buf[200];
    snprintf(buf, sizeof(buf), "{\"name\":\"%s\",\"id\":\"0x%08" PRIx32 "\",\"hop\":%d,\"rssi\":%.1f,\"snr\":%.1f}", nodeinfo.short_name, nodeinfo.node_id, 0, 0.0, 0.0);
    peers_json += buf;
    peers_json += "]\r\n";
    ws_sendall((uint8_t*)peers_json.c_str(), peers_json.length(), true);
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
    mtCompact.loadPrivKey();
    // mtCompact.setPrimaryChanByHash(8);
    mtCompact.setDebugMode(true);
    mtCompact.loadNodeDb();
    mtCompact.setOkToMqtt(true);
    mtCompact.setAutoFullNode(true);
    mtCompact.setSendEnabled(true);
    mtCompact.setSendHopLimit(7);
    mtCompact.sendMyNodeInfo();
    mtCompact.setOnMessage(on_message_int);
    mtCompact.setOnNodeInfoMessage(on_nodeinfo_int);
    loraInited = true;
    return true;
}

void lora_set_onmessage(OnLoraMessageCallback cb) {
    onLoraMessageCallback = cb;
}

// #$##$$#GOTLORAPEERS[{"name":"Frnk","id":"0xe0f74d14","hop":0,"rssi":0.0,"snr":0.0},{"name":"Tot4","id":"0x050d5990","hop":0,"rssi":0.0,"snr":0.0},]#$##$$#GOTSENS{"gps":{"y":2026,"m":3,"d":15,"h":10,"mi":54,"s":50,"siu":0,"siv":0,"lat":0.000000,"lon":0.000000,"alt":0.00,"speed":0.000000},"ori":{"head":400.0, "tilt":400.0 },"env":{"tempesp":44.9,"temp":0.0,"humi":0.0, "press":0.0, "light":0 }}

void lora_send_init_data_to_web() {
    if (loraInited == false) return;
    // send lora peers and history to web
    // sendinf peers
    // #$##$$#GOTLORAPEERS[{"name":"BaseStation","id":"0x1A2B3C4D","hop":0,"rssi":-85,"snr":6},{"name":"MobileNode_1","id":"0x9F8E7D6C","hop":1,"rssi":0,"snr":0}]
    std::string peers_json = "#$##$$#GOTLORAPEERS[";
    bool first = true;
    for (auto e : mtCompact.nodeinfo_db) {
        char buf[200];
        snprintf(buf, sizeof(buf), "{\"name\":\"%s\",\"id\":\"0x%08" PRIx32 "\",\"hop\":%d,\"rssi\":%.1f,\"snr\":%.1f}", e.short_name, e.node_id, 0, 0.0, 0.0);
        if (!first) {
            peers_json += ",";
        }
        peers_json += buf;
        first = false;
    }
    peers_json += "]\r\n";
    ws_sendall((uint8_t*)peers_json.c_str(), peers_json.length(), true);
    // #$##$$#GOTLORAHISTORY[{"sender":"BaseStation","message":"Node online. Awaiting data."},{"sender":"MobileNode_1","message":"Checking in from the field!"}]
}

void lora_send_message_to_mesh(const char* msg, size_t len) {
    if (loraInited == false || len <= 3) return;
    //{"message":"teszt","totype":"private","dest":"0x050d5990"}
    //{"message":"sadfas","totype":"chan","dest":"8"}
}

// add loop
// add debug info (telemetry, .. to web)
// add web interface stuff