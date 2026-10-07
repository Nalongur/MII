#pragma once
#include "esp_err.h"
#include <cstdint>
struct nvs_sec_cfg_t { std::uint8_t eky[32]{}; std::uint8_t tky[32]{}; };
struct nvs_sec_scheme_t { int unused{0}; };
inline constexpr esp_err_t ESP_ERR_NVS_NOT_INITIALIZED = -10, ESP_ERR_NVS_NOT_FOUND = -11,
    ESP_ERR_NVS_INVALID_LENGTH = -12, ESP_ERR_NVS_SEC_HMAC_KEY_NOT_FOUND = -13;
esp_err_t nvs_flash_deinit_partition(const char*);
esp_err_t nvs_flash_read_security_cfg_v2(nvs_sec_scheme_t*, nvs_sec_cfg_t*);
esp_err_t nvs_flash_secure_init_partition(const char*, nvs_sec_cfg_t*);
