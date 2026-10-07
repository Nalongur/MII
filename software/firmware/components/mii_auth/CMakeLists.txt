idf_component_register(
    SRCS "owner_session.cpp" "owner_record_codec.cpp" "idf_auth_crypto.cpp" "idf_owner_store.cpp" "auth_wire.cpp" "numeric_comparison.cpp"
    INCLUDE_DIRS "include"
    REQUIRES mii_protocol mbedtls nvs_flash nvs_sec_provider bt
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage)
