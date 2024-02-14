idf_component_register(SRCS "i2cdev_esp32.c" "i2c_drv.c" "hmc5883l.c" "nmea_parser.c" "main.c"  INCLUDE_DIRS ".")

spiffs_create_partition_image(storage ../data FLASH_IN_PROJECT)
