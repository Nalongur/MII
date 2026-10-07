#include "mii/idf_auth_crypto.hpp"

#include "esp_bt.h"
#include "esp_random.h"

namespace mii::auth {
IdfAuthCrypto::IdfAuthCrypto() noexcept { mbedtls_md_init(&context_); }
IdfAuthCrypto::~IdfAuthCrypto() { mbedtls_md_free(&context_); }

esp_err_t IdfAuthCrypto::initialize() noexcept {
    if (ready_) { return ESP_OK; }
    const auto* algorithm = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (algorithm == nullptr) { return ESP_ERR_NOT_SUPPORTED; }
    const int result = mbedtls_md_setup(&context_, algorithm, 1);
    if (result != 0) {
        mbedtls_md_free(&context_);
        mbedtls_md_init(&context_);
        return ESP_ERR_NO_MEM;
    }
    ready_ = true;
    return ESP_OK;
}
CryptoPort IdfAuthCrypto::port() noexcept { return {this, random, hmac}; }

bool IdfAuthCrypto::random(void* context, std::uint8_t* out, std::size_t size) noexcept {
    const auto* crypto = static_cast<IdfAuthCrypto*>(context);
    if (out != nullptr && size == 32) { wipe(out, size); }
    if (crypto == nullptr || !crypto->ready_ || out == nullptr || size != 32 ||
        esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED) {
        return false;
    }
    // Active BLE supplies hardware RNG entropy. Do not treat an idle controller
    // as proof of CSPRNG prerequisites, and never enable ADC entropy implicitly.
    esp_fill_random(out, size);
    return true;
}
bool IdfAuthCrypto::hmac(void* context, const Secret& key, const std::uint8_t* input,
                        std::size_t size, Secret& out) noexcept {
    auto* crypto = static_cast<IdfAuthCrypto*>(context);
    wipe(out.data(), out.size());
    if (crypto == nullptr || !crypto->ready_ || input == nullptr || size == 0 || size > 630) {
        return false;
    }
    if (mbedtls_md_hmac_starts(&crypto->context_, key.data(), key.size()) != 0 ||
        mbedtls_md_hmac_update(&crypto->context_, input, size) != 0 ||
        mbedtls_md_hmac_finish(&crypto->context_, out.data()) != 0) {
        wipe(out.data(), out.size());
        crypto->ready_ = false;
        mbedtls_md_free(&crypto->context_);
        mbedtls_md_init(&crypto->context_);
        return false;
    }
    // Retain the bounded workspace, not the previous owner/session key pads.
    const Secret zero_key{};
    if (mbedtls_md_hmac_starts(&crypto->context_, zero_key.data(), zero_key.size()) != 0) {
        wipe(out.data(), out.size());
        crypto->ready_ = false;
        mbedtls_md_free(&crypto->context_);
        mbedtls_md_init(&crypto->context_);
        return false;
    }
    return true;
}
}  // namespace mii::auth
