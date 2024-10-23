# ESP32-Portapack
An addon module for portapack to add GPS and extra sensors to it for more fun.

**This module is in early development. Suggest features via Issues, even for this module, even for the PortaPack (HackRf) part.**
## How to build one for yourself, or other questions? Check WIKI!


### Features:

- **Web interface** with remote control
- **GPS**
- **Compass**
- **Temperature + humidity + pressure + light**
- Portapack H4 I2C interface support! Can run unique apps, while plugged in!

For detailed info check Wiki

### Features not yet ready: 

- **Bluetooth connection**
  - Remote controller from a phone.
  - File exchange with phone.
  - Share sensor data with the phone too, for more advanced usage.
  - May not fit in the firmware, so it's not determined yet.

- **IR blaster**
  - To add IR remote functions to PP. May be available only for H4 + I2C

- **LoRa**
  - Some kind of LoRa support, needs to determinde function (based on availeable FW space, and computing power). Not soon!

### Standalone apps
If you connect this module to H4 via I2c, the module sends the following apps:
- Utilities / SatTrack.  With this, you can select one satellite, and check it's Elevation / Azimuth. For this, ESP needs internet connection, to sync the time, and download the latest TLE data.

More coming. Suggest in Issues if you get any ides.


### Screenshots:
**ADS-B with location**

![](https://github.com/htotoo/ESP32-Portapack/blob/main/ScreenShots/ADSB_mycoords.png?raw=true)  ![](https://github.com/htotoo/ESP32-Portapack/blob/main/ScreenShots/ADSB_mycoords_with_orientation.png?raw=true)

**ExtSensor app**

![](https://github.com/htotoo/ESP32-Portapack/blob/main/ScreenShots/ExtSensorTester.png?raw=true)


**Fox hunt app**

![](https://github.com/htotoo/ESP32-Portapack/blob/main/ScreenShots/foxhunt.png?raw=true)

**Web app**

![](https://github.com/htotoo/ESP32-Portapack/blob/main/ScreenShots/esp32pp_web.png?raw=true)




### More info
You can get more info in the [Wiki page](https://github.com/htotoo/ESP32-Portapack/wiki). Like, what modules are supported, functions with details. How to build and wire the module. How to flash it. How to start using it.
 

### Support
- Links are affiliate links! If you don't want to use them, feel free to just search for the modules yourself. Using the affiliate links gives me some credits, so I can buy and integrate more modules to this (or other) projects.
- If you want, you can buy me a coffee (fuel of programmers): https://www.buymeacoffee.com/htotoo
- This FW will be always open source and free to use all functions!
