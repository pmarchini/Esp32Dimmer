idf_component_register(
    SRCS "test_esp32_dimmer.c"
    INCLUDE_DIRS "."
    REQUIRES unity esp32-triac-dimmer-driver
)
