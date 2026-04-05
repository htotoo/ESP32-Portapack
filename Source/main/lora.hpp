#pragma once
#include "esp_err.h"
#include "pinconfig.h"
#include "MtCompact.hpp"
#include "MtMessageStore.hpp"
#include <nlohmann/json.hpp>
using json = nlohmann::json;
// todo do something with txco and ldo
bool loraInited = false;
uint32_t loraLastLoopMillis = 0;
uint32_t loraSecs = 0;
uint16_t loraNodeInfoMins = 30;

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
    /*.use_regulator_ldo = */ false,
};  // default LoRa configuration for EU MFFAST 868
MtCompact mtCompact;
MtMessageStore mtMessageStore;

using OnLoraMessageCallback = void (*)(std::string& sender, std::string& chan, std::string& message);
OnLoraMessageCallback onLoraMessageCallback = nullptr;

void lora_name_from_nodeinfo(const MCT_NodeInfo& nodeinfo, std::string& out) {
    out = nodeinfo.short_name;
    out += " (";
    out += std::string_view(nodeinfo.long_name).substr(0, 20);
    out += ")";
}

void on_message_pre() {
}

void on_message_int(MCT_Header& header, MCT_TextMessage& message) {
    MCT_NodeInfo* nodeinfo = mtCompact.nodeinfo_db.get(header.srcnode);
    std::string sender;
    if (nodeinfo) {
        lora_name_from_nodeinfo(*nodeinfo, sender);
    } else {
        char hexbuf[11];
        snprintf(hexbuf, sizeof(hexbuf), "0x%08" PRIx32, header.srcnode);
        sender = hexbuf;
    }

    MtMessageStore::MessageEntry ent{sender.c_str(), message.chan, header.dstnode != 0xffffffff, message.text.c_str(), header.srcnode == mtCompact.getMyNodeInfo()->node_id, std::time(nullptr)};
    mtMessageStore.addMessage(ent);
}

void on_nodeinfo_int(MCT_Header& header, MCT_NodeInfo& nodeinfo, bool needReply, bool newNode) {
    std::string peers_json = "#$##$$#GOTLORAPEERS[";
    char buf[200];
    std::string name;
    lora_name_from_nodeinfo(nodeinfo, name);
    snprintf(buf, sizeof(buf), "{\"name\":\"%s\",\"id\":\"0x%08" PRIx32 "\",\"hop\":%d,\"rssi\":%.1f,\"snr\":%.1f}", name.c_str(), nodeinfo.node_id, 0, 0.0, 0.0);
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
    if (!mtCompact.RadioInit((RadioType)pinConfig.LoraChipType(), radio_pins, lora_config)) return false;
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
    mtMessageStore.addListener([](const MtMessageStore::MessageEntry& entry) {
        if (onLoraMessageCallback) {
            std::string sender = entry.sender.c_str();
            std::string chan = std::to_string(entry.channel);
            std::string message = entry.message.c_str();
            onLoraMessageCallback(sender, chan, message);
        }
    });
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
        std::string name;
        lora_name_from_nodeinfo(e, name);
        snprintf(buf, sizeof(buf), "{\"name\":\"%s\",\"id\":\"0x%08" PRIx32 "\",\"hop\":%d,\"rssi\":%.1f,\"snr\":%.1f}", name.c_str(), e.node_id, 0, 0.0, 0.0);
        if (!first) {
            peers_json += ",";
        }
        peers_json += buf;
        first = false;
    }
    peers_json += "]\r\n";
    ws_sendall((uint8_t*)peers_json.c_str(), peers_json.length(), true);
    peers_json = "";
    // #$##$$#GOTLORAHISTORY[{"sender":"BaseStation","message":"Node online. Awaiting data."},{"sender":"MobileNode_1","message":"Checking in from the field!"}]
    std::string history_json = "#$##$$#GOTLORAHISTORY[";
    first = true;
    for (const auto& entry : mtMessageStore) {
        char buf[300];
        snprintf(buf, sizeof(buf), "{\"sender\":\"%s\",\"message\":\"%s\"}", entry.sender.c_str(), entry.message.c_str());
        if (!first) {
            history_json += ",";
        }
        history_json += buf;
        first = false;
    }
    history_json += "]\r\n";
    ws_sendall((uint8_t*)history_json.c_str(), history_json.length(), true);
}

void lora_send_message_to_mesh(const char* msg, size_t len) {
    if (loraInited == false || len <= 3) return;
    //{"message":"teszt","totype":"private","dest":"0x050d5990"}
    //{"message":"sadfas","totype":"chan","dest":"8"}
    std::string msg_str(msg, len);
    ESP_LOGI("LORA", "Got message to send: %s", msg_str.c_str());
    json data = json::parse(msg_str, nullptr, false);
    ESP_LOGI("LORA", "Parsed JSON message");
    std::string message = data["message"];
    std::string to_type = data["totype"];
    std::string dest = data["dest"];
    std::string deststr = "TO: ";
    uint8_t chani = 0;
    if (to_type == "private") {
        uint32_t dest_id = std::stoul(dest, nullptr, 16);
        MCT_NodeInfo* nodeinfo = mtCompact.nodeinfo_db.get(dest_id);
        std::string s;
        if (nodeinfo) {
            lora_name_from_nodeinfo(*nodeinfo, s);
        } else {
            char hexbuf[11];
            snprintf(hexbuf, sizeof(hexbuf), "0x%08" PRIx32, dest_id);
            s = hexbuf;
        }
        deststr += s;
        mtCompact.sendTextMessage(message, dest_id);
    } else if (to_type == "chan") {
        chani = std::stoi(dest);
        deststr += "CH: " + dest;
        uint8_t chan = std::stoi(dest);
        mtCompact.sendTextMessage(message, 0xffffffff, chan);
    }
    MtMessageStore::MessageEntry ent{deststr.c_str(), chani, to_type != "chan", message.c_str(), true, std::time(nullptr)};
    mtMessageStore.addMessage(ent);
}

void lora_loop(uint32_t currentMillis) {
    if (!loraInited) return;
    if (currentMillis - loraLastLoopMillis >= 1000) {
        loraLastLoopMillis = currentMillis;
        loraSecs++;
        if (loraSecs % (60 * loraNodeInfoMins) == 0) {
            mtCompact.sendMyNodeInfo();  // send nodeinfo every n mins to keep the network updated
        }
        if (mtCompact.nodeinfo_db.needsSave()) {
            mtCompact.saveNodeDb();
        }
    }
}

// add loop stuff
// add debug info (telemetry, .. to web)
// add web interface stuff