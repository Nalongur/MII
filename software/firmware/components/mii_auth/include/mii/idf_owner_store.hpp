#pragma once

#include "esp_err.h"
#include "mii/owner_record_codec.hpp"
#include "nvs_sec_provider.h"

namespace mii::auth {
// Exclusive owner of the dedicated mii_auth partition; no shared default NVS.
// Initialization only reads a pre-provisioned HMAC_UP key. No key generation,
// eFuse writes, partition erase or implicit plaintext fallback exists here.
class IdfOwnerStore {
public:
    IdfOwnerStore() noexcept = default;
    ~IdfOwnerStore();
    IdfOwnerStore(const IdfOwnerStore&) = delete;
    IdfOwnerStore& operator=(const IdfOwnerStore&) = delete;
    [[nodiscard]] esp_err_t initialize(int provisioned_hmac_key_id) noexcept;
    [[nodiscard]] StorageLoad load(OwnerRecord&) noexcept;
    [[nodiscard]] StoragePort port() noexcept;
private:
    static bool commit(void*, const OwnerRecord&) noexcept;
    bool ready_{false};
    bool initialized_{false};
    nvs_sec_scheme_t* scheme_{nullptr};
};
}  // namespace mii::auth
