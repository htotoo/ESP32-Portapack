/*
 * pinconfig_html.h
 *
 * This file is split into chunks to be sent sequentially.
 * The server will fill in the variable data between chunks.
 */
#pragma once

// Part 1: From the start to the first variable (ledRgbPin)
static const char PINCONFIG_HTML_PART1[] = R"EOF(<!DOCTYPE HTML>
<html>
<head>
    <title>ESP32PP Pin Config</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="icon" href="data:,">
    <link rel="stylesheet" href="setup.css">
</head>
<body>
    <h1>ESP32 PP Pin Config</h1>
    <form method="post" action="/pinconfig.html">
        <div class="form-section">
            <label for="hw_variant">Hardware Variant:</label>
            <select id="hw_variant" name="hw_variant">
                <option value="custom">Custom</option>
                <option value="esp32pp">ESP32PP</option>
                <option value="mdk">MDK Board</option>
                <option value="prfai">PRFAI</option>
            </select>
            <i>Select a preset or "Custom" to enter pins manually.</i>
        </div>
        <div class="form-section">
            <p>Core Pins</p>
            <label for="ledRgbPin">RGB LED Pin:</label>
            <input type="number" min="-1" name="ledRgbPin" id="ledRgbPin" value=")EOF";

// Part 2: Between ledRgbPin and gpsRxPin
static const char PINCONFIG_HTML_PART2[] = R"EOF(" />
            <label for="gpsRxPin">GPS RX Pin:</label>
            <input type="number" min="-1" name="gpsRxPin" id="gpsRxPin" value=")EOF";

// Part 3: Between gpsRxPin and i2cSdaPin
static const char PINCONFIG_HTML_PART3[] = R"EOF(" />
        </div>
        <div class="form-section">
            <p>I2C Master (Sensors)</p>
            <label for="i2cSdaPin">SDA Pin:</label>
            <input type="number" min="-1" name="i2cSdaPin" id="i2cSdaPin" value=")EOF";

// Part 4: Between i2cSdaPin and i2cSclPin
static const char PINCONFIG_HTML_PART4[] = R"EOF(" />
            <label for="i2cSclPin">SCL Pin:</label>
            <input type="number" min="-1" name="i2cSclPin" id="i2cSclPin" value=")EOF";

// Part 5: Between i2cSclPin and irRxPin
static const char PINCONFIG_HTML_PART5[] = R"EOF(" />
        </div>
        <div class="form-section">
            <p>Infrared (IR)</p>
            <label for="irRxPin">IR RX Pin:</label>
            <input type="number" min="-1" name="irRxPin" id="irRxPin" value=")EOF";

// Part 6: Between irRxPin and irTxPin
static const char PINCONFIG_HTML_PART6[] = R"EOF(" />
            <label for="irTxPin">IR TX Pin:</label>
            <input type="number" min="-1" name="irTxPin" id="irTxPin" value=")EOF";

// Part 7: Between irTxPin and i2cSdaSlavePin
static const char PINCONFIG_HTML_PART7[] = R"EOF(" />
        </div>
        <div class="form-section">
            <p>I2C Slave (to PortaPack)</p>
            <label for="i2cSdaSlavePin">SDA Pin:</label>
            <input type="number" min="-1" name="i2cSdaSlavePin" id="i2cSdaSlavePin" value=")EOF";

// Part 8: Between i2cSdaSlavePin and i2cSclSlavePin
static const char PINCONFIG_HTML_PART8[] = R"EOF(" />
            <label for="i2cSclSlavePin">SCL Pin:</label>
            <input type="number" min="-1" name="i2cSclSlavePin" id="i2cSclSlavePin" value=")EOF";

// Part 9: The rest of the file (including all JavaScript)
static const char PINCONFIG_HTML_PART9[] = R"EOF(" />
        </div>
        <script>
            const pinPresets = {
                'custom': [-1, 256, -1, -1, -1, -1, -1, -1],
                'esp32pp': [48, 6, 5, 4, 12, 13, 11, 10],
                'mdk': [-1, 4, 11, 10, 12, 13, 5, 6],
                'prfai': [-1, 256, 11, 10, 12, 13, 1, 2]
            };
            const pinInputIds = [
                'ledRgbPin',
                'gpsRxPin',
                'i2cSdaPin',
                'i2cSclPin',
                'irRxPin',
                'irTxPin',
                'i2cSdaSlavePin',
                'i2cSclSlavePin'
            ];
            document.addEventListener('DOMContentLoaded', () => {
                const variantSelect = document.getElementById('hw_variant');
                const pinInputs = pinInputIds.map(id => document.getElementById(id));
                function updateFieldsFromDropdown() {
                    const selectedVariant = variantSelect.value;
                    const pins = pinPresets[selectedVariant];
                    if (!pins) return;
                    pinInputs.forEach((input, index) => {
                        if (input) {
                            input.value = pins[index];
                        }
                    });
                }
                function updateDropdownFromFields() {
                    const currentPins = pinInputs.map(input => parseInt(input.value, 10) || 0);
                    let matchedVariant = 'custom'; 
                    for (const [variant, pins] of Object.entries(pinPresets)) {
                        if (pins.every((pin, index) => pin === currentPins[index])) {
                            matchedVariant = variant;
                            break; 
                        }
                    }
                    if (variantSelect.value !== matchedVariant) {
                        variantSelect.value = matchedVariant;
                    }
                }
                variantSelect.addEventListener('change', updateFieldsFromDropdown);
                pinInputs.forEach(input => {
                    input.addEventListener('input', updateDropdownFromFields);
                });
                updateDropdownFromFields();
            });
        </script>
        <div class="actions">
            <input type="submit" name="submit" value="Save" />
            <a href="/">Cancel</a>
        </div>
    </form>
    <div class="info-text">
        Pin changes require rebooting the ESP to apply.<br />
        <a href="/ota.html">OTA Update ESP</a><br />
    </div>
</body>
</html>)EOF";