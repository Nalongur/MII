idf_component_register(
    SRCS "app_main.cpp"
    REQUIRES board_v7 mii_ble mii_protocol mii_auth
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
