# ESP32-Portapack (ESP32PP)
An addon module for Portapack H1 / H2 / H4 / PortaRF to add GPS and extra sensors to it for more fun.

**This module is in early development. Suggest features via Issues, even for this module, even for the PortaPack (HackRf) part.**

## How to build one for yourself, or other questions? Check WIKI!
If you got the MDK hardware, it has a different pinout, so check the corresponding wiki page for that! Also download the right binary!


### Features:

- **Web interface** with remote control
- **GPS**
- **Compass**
- **Temperature + humidity + pressure + light**
- Portapack H4 / PortaRF I2C interface support! Can run unique apps, while plugged in!
- On system apps! The ESP can handle it's own apps built into it. For example Wifi SSID spoofer. (see Wiki for it)

For detailed info check Wiki

### Features not yet ready: 

- **IR blaster**
  - To add IR remote functions to PP. May be available only over I2C connection. Now only experimental RX.

- **LoRa**
  - Meshtastic support. (functions not yet decided for it)

### Standalone apps
If you connect this module to H4 / PortaRF via I2c, the module will supply additional apps for it. Check the list in the [Wiki](https://github.com/htotoo/ESP32-Portapack/wiki/Apps-over-I2C)

Suggest in Issues if you get any ideas.


### Screenshots:
**ADS-B with location**

![](https://github.com/htotoo/ESP32-Portapack/blob/main/ScreenShots/ADSB_mycoords.png?raw=true)  ![](https://github.com/htotoo/ESP32-Portapack/blob/main/ScreenShots/ADSB_mycoords_with_orientation.png?raw=true)

**ExtSensor app**

![](https://github.com/htotoo/ESP32-Portapack/blob/main/ScreenShots/ExtSensorTester.png?raw=true)


**Fox hunt app**

![](https://github.com/htotoo/ESP32-Portapack/blob/main/ScreenShots/foxhunt.png?raw=true)


**SatTrack**

![](https://github.com/htotoo/ESP32-Portapack/blob/main/ScreenShots/sattrack.png?raw=true)


**Web app**

![](https://github.com/htotoo/ESP32-Portapack/blob/main/ScreenShots/esp32pp_web.png?raw=true)



### More info
You can get more info in the [Wiki page](https://github.com/htotoo/ESP32-Portapack/wiki). Like, what modules are supported, functions with details. How to build and wire the module. How to flash it. How to start using it.
 

### Support
- Links are affiliate links! If you don't want to use them, feel free to just search for the modules yourself. Using the affiliate links gives me some credits, so I can buy and integrate more modules to this (or other) projects.
- If you want, you can buy me a coffee (fuel of programmers): https://www.buymeacoffee.com/htotoo
- This FW will be always open source and free to use all functions!
