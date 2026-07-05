/*
 * pinconfig_html.h
 */
#pragma once

// Part 1: Start -> ledRgbPin
static const char PINCONFIG_HTML_PART1[] = R"EOF(<!DOCTYPE HTML><html><head><title>ESP32PP Pin Config</title><meta name="viewport" content="width=device-width, initial-scale=1"><link rel="icon" href="data:,"><link rel="stylesheet" href="setup.css"></head><body><h1>ESP32 PP Pin Config</h1><form method="post" action="/pinconfig.html"><div class="form-section"><label for="hw_variant">Hardware Variant:</label><select id="hw_variant" name="hw_variant"><option value="custom">Custom</option><option value="esp32pp">ESP32PP</option><option value="mdk">MDK Board</option><option value="prfai">PRFAI</option></select><i>Select a preset or "Custom" to enter pins manually.</i></div><div class="form-section"><p>Core Pins</p><label for="ledRgbPin">RGB LED Pin:</label><input type="number" min="-1" name="ledRgbPin" id="ledRgbPin" value=")EOF";

// Part 2: ledRgbPin -> gpsRxPin
static const char PINCONFIG_HTML_PART2[] = R"EOF(" /><label for="gpsRxPin">GPS RX Pin:</label><input type="number" min="-1" name="gpsRxPin" id="gpsRxPin" value=")EOF";

// Part 3: gpsRxPin -> i2cSdaPin
static const char PINCONFIG_HTML_PART3[] = R"EOF(" /></div><div class="form-section"><p>I2C Master (Sensors)</p><label for="i2cSdaPin">SDA Pin:</label><input type="number" min="-1" name="i2cSdaPin" id="i2cSdaPin" value=")EOF";

// Part 4: i2cSdaPin -> i2cSclPin
static const char PINCONFIG_HTML_PART4[] = R"EOF(" /><label for="i2cSclPin">SCL Pin:</label><input type="number" min="-1" name="i2cSclPin" id="i2cSclPin" value=")EOF";

// Part SPI Start: i2cSclPin -> spiSckPin
static const char PINCONFIG_HTML_PART_SPI_START[] = R"EOF(" /></div><div class="form-section"><p>SPI Master</p><label for="spiSckPin">SCK Pin:</label><input type="number" min="-1" name="spiSckPin" id="spiSckPin" value=")EOF";

// Part SPI 1: spiSckPin -> spiMisoPin
static const char PINCONFIG_HTML_PART_SPI_1[] = R"EOF(" /><label for="spiMisoPin">MISO Pin:</label><input type="number" min="-1" name="spiMisoPin" id="spiMisoPin" value=")EOF";

// Part SPI 2: spiMisoPin -> spiMosiPin
static const char PINCONFIG_HTML_PART_SPI_2[] = R"EOF(" /><label for="spiMosiPin">MOSI Pin:</label><input type="number" min="-1" name="spiMosiPin" id="spiMosiPin" value=")EOF";

// Part LoRa Start: spiMosiPin -> loraChipType (injected into data-val)
static const char PINCONFIG_HTML_PART_LORA_START[] = R"EOF(" /></div><div class="form-section"><p>LoRa Radio</p><label for="loraChipType">Radio Chip:</label><select id="loraChipType" name="loraChipType" data-val=")EOF";

// Part LoRa 1: loraChipType -> loraNssPin
static const char PINCONFIG_HTML_PART_LORA_1[] = R"EOF("><option value="-1">None</option><option value="0">SX1262</option><option value="1">SX1261</option><option value="2">SX1268</option><option value="3">SX1276</option><option value="4">LR1121</option><option value="5">SX1278</option></select><label for="loraNssPin">NSS Pin:</label><input type="number" min="-1" name="loraNssPin" id="loraNssPin" value=")EOF";

// Part LoRa 2: loraNssPin -> loraResetPin
static const char PINCONFIG_HTML_PART_LORA_2[] = R"EOF(" /><label for="loraResetPin">Reset Pin:</label><input type="number" min="-1" name="loraResetPin" id="loraResetPin" value=")EOF";

// Part LoRa 3: loraResetPin -> loraDio0Pin
static const char PINCONFIG_HTML_PART_LORA_3[] = R"EOF(" /><label for="loraDio0Pin">DIO0 Pin:</label><input type="number" min="-1" name="loraDio0Pin" id="loraDio0Pin" value=")EOF";

// Part LoRa 4: loraDio0Pin -> loraDio1Pin
static const char PINCONFIG_HTML_PART_LORA_4[] = R"EOF(" /><label for="loraDio1Pin">DIO1 Pin:</label><input type="number" min="-1" name="loraDio1Pin" id="loraDio1Pin" value=")EOF";

// Part 5: loraDio1Pin -> irRxPin
static const char PINCONFIG_HTML_PART5[] = R"EOF(" /></div><div class="form-section"><p>Infrared (IR)</p><label for="irRxPin">IR RX Pin:</label><input type="number" min="-1" name="irRxPin" id="irRxPin" value=")EOF";

// Part 6: irRxPin -> irTxPin
static const char PINCONFIG_HTML_PART6[] = R"EOF(" /><label for="irTxPin">IR TX Pin:</label><input type="number" min="-1" name="irTxPin" id="irTxPin" value=")EOF";

// Part 7: irTxPin -> i2cSdaSlavePin
static const char PINCONFIG_HTML_PART7[] = R"EOF(" /></div><div class="form-section"><p>I2C Slave (to PortaPack)</p><label for="i2cSdaSlavePin">SDA Pin:</label><input type="number" min="-1" name="i2cSdaSlavePin" id="i2cSdaSlavePin" value=")EOF";

// Part 8: i2cSdaSlavePin -> i2cSclSlavePin
static const char PINCONFIG_HTML_PART8[] = R"EOF(" /><label for="i2cSclSlavePin">SCL Pin:</label><input type="number" min="-1" name="i2cSclSlavePin" id="i2cSclSlavePin" value=")EOF";

// Part 9: Rest of file (JS + Footer)
static const char PINCONFIG_HTML_PART9[] = R"EOF(" /></div><script>
const pinPresets = {
'custom': [-1,256,-1,-1,-1,-1,-1,-1,35,36,37,-1,38,39,40,41],
'esp32pp': [48,6,5,4,12,13,11,10,-1,-1,-1,-1,-1,-1,-1,-1],
'mdk': [-1,4,11,10,12,13,5,6,-1,-1,-1,-1,-1,-1,-1,-1],
'prfai': [-1,256,11,10,12,13,1,2,-1,-1,-1,-1,-1,-1,-1,-1]
};
const pinInputIds = ['ledRgbPin','gpsRxPin','i2cSdaPin','i2cSclPin','irRxPin','irTxPin','i2cSdaSlavePin','i2cSclSlavePin','spiSckPin','spiMisoPin','spiMosiPin','loraChipType','loraNssPin','loraResetPin','loraDio0Pin','loraDio1Pin'];
document.addEventListener('DOMContentLoaded', () => {
const vs = document.getElementById('hw_variant');
const pi = pinInputIds.map(id => document.getElementById(id));
const loraSel = document.getElementById('loraChipType');
if(loraSel && loraSel.hasAttribute('data-val')) loraSel.value = loraSel.getAttribute('data-val');
function updF() {
const sv = vs.value;
const pins = pinPresets[sv];
if (!pins) return;
pi.forEach((inp, i) => { if (inp) inp.value = pins[i]; });
}
function updD() {
const cur = pi.map(inp => parseInt(inp.value, 10) || 0);
let mv = 'custom';
for (const [v, p] of Object.entries(pinPresets)) {
if (p.every((pin, i) => pin === cur[i])) { mv = v; break; }
}
if (vs.value !== mv) vs.value = mv;
}
vs.addEventListener('change', updF);
pi.forEach(inp => { if(inp) { inp.addEventListener('input', updD); inp.addEventListener('change', updD); }});
updD();
});
</script><div class="actions"><input type="submit" name="submit" value="Save" /><a href="/">Cancel</a></div></form><div class="info-text">Pin changes require rebooting the ESP to apply.<br /><a href="/ota.html">OTA Update ESP</a><br /></div></body></html>)EOF";