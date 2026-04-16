idf_component_register(SRCS "EspHal.cpp" "LoraRadio.cpp"
                       REQUIRES esp_driver_spi esp_driver_gpio
                       PRIV_REQUIRES spi_flash esp_timer radiolib log freertos
                       INCLUDE_DIRS ".")
