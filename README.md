# ESP32-Portapack
An addon module for portapack to add extra sensors to it for more fun.

**This module is in early development. Suggest features via Issues, even for this module, even for the PortaPack (HackRf) part.**

### Features:

- **Web**
  - Can be Wifi AP or can connect to a Wifi router (or phone share).
  - Built in web server to remote control the Portapack.
  - Configure ESP from the Wifi AP. (If you use AP mode, the STA mode will stop, till you disconnect from AP)
  - Default url in AP mode: http://192.168.4.1/ (in AP mode, you should turn off mobile data, to be able to connect to it). Default AP SSID: ESP32PP, Pass: 12345678
  - OTA update the ESP32S3 (from settings / ota)

- **GPS**
  - See yourself in the maps, to know the relative positions. Great while hunting weather balloons.
  - Mark the location of the signals you saw.
  - Help with the Foxhunt (for the Foxhunt app, that is under development)



### Features not yet ready: 

- **Web**
  - OTA update the Portapack (not yet ready).
  - Download / upload files to Portapack from mobile. (not yet ready).

- **Temperature + humidity**
  - Depending on the environmental conditions, a temperature and humidity sensor can be useful for adapting the game strategy or adding complexity to the foxhunt.
  - Also cool.

- **Bluetooth connection**
  - Remote controller from a phone.
  - File exchange with phone.
  - Share sensor data with the phone too, for more advanced usage.

- **IR blaster**
  - To add IR remote functions to PP.
- **Compass**
  - To see where the signal comes from while using a directional antenna. (eg for Foxhunt)
  - Helps find satellites.

### Compatible modules:
- ESP32S3 (recommended: https://s.click.aliexpress.com/e/_DeaSKvJ - select the S3, not the C3!)
- USB cable that fits your needs. (to wire with PP, and supply enough power to it!)
- GPS, any NMEA over UART. For example the NEO 7M: https://s.click.aliexpress.com/e/_DkDZHaV . Select NEO 7M or 8M for much better precision, 6M is GPS only!
- Compass, HMC5883L supported for now (can suggest better in the Issues)
- Temperature+humidity: SHT30 I2C (not yet ready). https://s.click.aliexpress.com/e/_DFU9Ra9 

### Screenshots:
**ADS-B with location**

![](https://github.com/htotoo/ESP32-Portapack/blob/main/ScreenShots/ADSB_mycoords.png?raw=true)  ![](https://github.com/htotoo/ESP32-Portapack/blob/main/ScreenShots/ADSB_mycoords_with_orientation.png?raw=true)

### Support
- Links are affiliate links! If you don't want to use them, feel free to just search for the modules yourself. Using the affiliate links gives me some credits, so I can buy and integrate more modules to this (or other) projects.
- If you want, you can buy me a coffee (fuel of programmers): https://www.buymeacoffee.com/htotoo