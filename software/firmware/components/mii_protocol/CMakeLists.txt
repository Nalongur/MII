set(NANOPB_RUNTIME_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../third_party/nanopb")

set(MII_PROTOCOL_SRCS
    "generated/mii.pb.c"
    "frame_codec.cpp"
    "frame_ingress.cpp"
    "protocol_codec.cpp"
    "session_state.cpp"
    "${NANOPB_RUNTIME_DIR}/pb_common.c"
    "${NANOPB_RUNTIME_DIR}/pb_encode.c"
    "${NANOPB_RUNTIME_DIR}/pb_decode.c"
)

if(CONFIG_MII_PROTOCOL_STARTUP_SELF_TEST)
    list(APPEND MII_PROTOCOL_SRCS "protocol_self_test.cpp")
endif()

idf_component_register(
    SRCS ${MII_PROTOCOL_SRCS}
    INCLUDE_DIRS
        "include"
        "generated"
        "${NANOPB_RUNTIME_DIR}"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
