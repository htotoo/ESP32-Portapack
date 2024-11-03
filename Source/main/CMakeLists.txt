
idf_component_register(SRCS "led.cpp" "configuration.cpp" "sensordb.c" "orientation.c" "environment.c" "ppi2c/pp_handler.cpp" "ppi2c/i2c_slave_driver.c"  "drivers/i2cdev.c" "drivers/hmc5883l.c" "drivers/lsm303.c"  "drivers/mpu925x.c" "drivers/sht3x.c"  "drivers/bh1750.c"  "drivers/bmp280.c"  "drivers/adxl345.c" "nmea_parser.c" "main.cpp" 
"sgp4/brent.cpp"
"sgp4/sgp4coord.cpp"
"sgp4/sgp4ext.cpp"
"sgp4/sgp4io.cpp"
"sgp4/sgp4pred.cpp"
"sgp4/sgp4unit.cpp"
"sgp4/visible.cpp"
INCLUDE_DIRS "." "./sgp4"  EMBED_FILES ../data/index.html ../data/ota.html ../data/setup.html)
