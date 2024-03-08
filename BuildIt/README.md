# ESP32-Portapack
An addon module for portapack to add extra sensors to it for more fun.


**Pin configuration**

- GPS TX -> ESP pin 6 (baud 9600)
- I2c devices SCL  -> ESP pin 4
- I2c devices SDA  -> ESP pin 5

**Sensor placement**
If you use HMC5883L AND ADXL345 all axes must point to the same direction on both sensors!