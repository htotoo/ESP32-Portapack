# ESP32-Portapack
An addon module for portapack to add extra sensors to it for more fun.

**This module is in early development. Suggest features via Issues, even for this module, even for the PortaPack (HackRf) part.**

### Features: (will be)Features: (will be)

- **GPS**
  - See yourself in the maps, to know the relative positions. Great while hunting weather balloons.
  - Mark the location of the signals you saw.
  - Help with the Foxhunt.
- **Compass**
  - To see where the signal comes from while using a directional antenna. (eg for Foxhunt)
  - Helps find satellites.
- **Temperature + humidity**
  - Depending on the environmental conditions, a temperature and humidity sensor can be useful for adapting the game strategy or adding complexity to the foxhunt.
  - Also cool.
- **Bluetooth connection**
  - Remote controller from a phone.
  - File exchange with phone.
  - Share sensor data with the phone too, for more advanced usage.


### Compatible modules:
- ESP32S3, ESP32S2 (the ones with OTG)
- USB cable that fits your needs. (Special Y if you want to power PP and ESP)
- GPS, any NMEA uart with baud 9600.
- Compass, to be determined what is the most precise.
- Temperature+humidity: SHT30 I2C

### Screenshots:
**ADS-B with location**
![](https://github.com/htotoo/ESP32-Portapack/blob/main/ScreenShots/ADSB_mycoords.png?raw=true)  ![](https://github.com/htotoo/ESP32-Portapack/blob/main/ScreenShots/ADSB_mycoords_with_orientation.png?raw=true)