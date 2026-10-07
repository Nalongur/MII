#pragma once

#include "esp_err.h"
#include "mbedtls/md.h"
#include "mii/owner_session.hpp"

namespace mii::auth {
// Single task. One checked mbedTLS context allocation at initialization;
// subsequent HMACs reuse its bounded SHA-256/HMAC workspace.
class IdfAuthCrypto {
public:
    IdfAuthCrypto() noexcept;
    ~IdfAuthCrypto();
    IdfAuthCrypto(const IdfAuthCrypto&) = delete;
    IdfAuthCrypto& operator=(const IdfAuthCrypto&) = delete;
    [[nodiscard]] esp_err_t initialize() noexcept;
    [[nodiscard]] CryptoPort port() noexcept;
private:
    static bool random(void*, std::uint8_t*, std::size_t) noexcept;
    static bool hmac(void*, const Secret&, const std::uint8_t*, std::size_t, Secret&) noexcept;
    mbedtls_md_context_t context_{};
    bool ready_{false};
};
}  // namespace mii::auth
