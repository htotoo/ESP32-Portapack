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
  - OTA update the Portapack.
  - Download / upload files to Portapack from a browser via WiFi.

- **GPS**
  - See yourself in the maps, to know the relative positions. Great while hunting weather balloons.
  - Mark the location of the signals you saw.
  - Help with the Foxhunt (for the Foxhunt app, that is under development)

- **Temperature + humidity + pressure + light**
  - Depending on the environmental conditions, it can be useful for adapting the game strategy or adding complexity to the foxhunt.
  - In the future light sensor could set the brightness of the PP, or if it detects it is in a pocket (0 LUX) turn off the screen. (No support on PP side yet).


### Features not yet ready: 

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
- Temperature+humidity: SHT30 I2C. https://s.click.aliexpress.com/e/_DFU9Ra9 or BME280 I2C(or BMP280 I2C) https://s.click.aliexpress.com/e/_DCRJ0ZT
- Light sensor: BH1750 https://s.click.aliexpress.com/e/_DDFf7vr

### Screenshots:
**ADS-B with location**

![](https://github.com/htotoo/ESP32-Portapack/blob/main/ScreenShots/ADSB_mycoords.png?raw=true)  ![](https://github.com/htotoo/ESP32-Portapack/blob/main/ScreenShots/ADSB_mycoords_with_orientation.png?raw=true)

**ExtSensor app**

![](https://github.com/htotoo/ESP32-Portapack/blob/main/ScreenShots/ExtSensorTester.png?raw=true)


### Flash:
If you want to flash the provided binary files, you can use this command:

```esptool.py -p COM5 -b 460800 --before default_reset --after hard_reset --chip esp32s3 write_flash --flash_mode dio --flash_freq 80m --flash_size detect 0x0 bootloader.bin 0x10000 ESP32PP.bin 0x8000 partition-table.bin 0xd000 ota_data_initial.bin```

If you want to flash from GUI, you can use the Flash Download Tool from ESP with these settings:
![](https://github.com/htotoo/ESP32-Portapack/blob/main/ScreenShots/flash.png?raw=true)

### Support
- Links are affiliate links! If you don't want to use them, feel free to just search for the modules yourself. Using the affiliate links gives me some credits, so I can buy and integrate more modules to this (or other) projects.
- If you want, you can buy me a coffee (fuel of programmers): https://www.buymeacoffee.com/htotoo
- This FW will be always open source and free to use all functions!