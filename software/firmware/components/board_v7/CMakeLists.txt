idf_component_register(
    SRCS "board_v7.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
