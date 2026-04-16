idf_component_register(
    SRCS
        "test_main.cpp"
        "test_config.cpp"
        "test_packet.cpp"
        "test_lora_radio.cpp"
    PRIV_REQUIRES
        lora
        unity
    INCLUDE_DIRS "."
)
