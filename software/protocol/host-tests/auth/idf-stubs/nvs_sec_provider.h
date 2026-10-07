#pragma once
#include "nvs_flash.h"
enum hmac_key_id_t { HMAC_KEY0 = 0, HMAC_KEY_MAX = 6 };
struct nvs_sec_config_hmac_t { hmac_key_id_t hmac_key_id; };
esp_err_t nvs_sec_provider_register_hmac(const nvs_sec_config_hmac_t*, nvs_sec_scheme_t**);
esp_err_t nvs_sec_provider_deregister(nvs_sec_scheme_t*);
