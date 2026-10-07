idf_component_register(
    SRCS
        "ble_control.cpp"
        "ble_rx_session.cpp"
        "ble_tx_session.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        bt
        mii_protocol
        nvs_flash
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
