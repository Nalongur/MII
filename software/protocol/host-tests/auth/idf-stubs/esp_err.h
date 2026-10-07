#pragma once
using esp_err_t = int;
inline constexpr esp_err_t ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_INVALID_ARG = -2,
    ESP_ERR_INVALID_STATE = -3, ESP_ERR_NO_MEM = -4, ESP_ERR_NOT_SUPPORTED = -5;
