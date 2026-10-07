cmake_minimum_required(VERSION 3.16)

# MII V7 is fixed to ESP32-S3. Keep the project defaults explicit so a clean
# build does not silently inherit settings from a different ESP-IDF project.
set(IDF_TARGET "esp32s3")
if(NOT DEFINED SDKCONFIG_DEFAULTS)
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults.v7_n8;sdkconfig.defaults.debug")
endif()

# Keep generated configuration with its build directory so Debug and Release
# profiles never reuse a project-root sdkconfig from another build.
set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(mii_firmware)
