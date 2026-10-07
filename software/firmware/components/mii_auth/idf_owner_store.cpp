#include "mii/idf_owner_store.hpp"

#include "nvs.h"
#include "nvs_flash.h"

namespace mii::auth {
namespace {
constexpr char kPartition[] = "mii_auth";
constexpr char kNamespace[] = "owner";
constexpr char kRecordKey[] = "record_v1";
}
IdfOwnerStore::~IdfOwnerStore() {
    if (initialized_) { static_cast<void>(nvs_flash_deinit_partition(kPartition)); }
    if (scheme_ != nullptr) { static_cast<void>(nvs_sec_provider_deregister(scheme_)); }
}

esp_err_t IdfOwnerStore::initialize(int key_id) noexcept {
    if (ready_) { return ESP_ERR_INVALID_STATE; }
    if (key_id < 0 || key_id >= HMAC_KEY_MAX) { return ESP_ERR_INVALID_ARG; }
    if (scheme_ != nullptr) {
        const esp_err_t result = nvs_sec_provider_deregister(scheme_);
        if (result != ESP_OK) { return result; }
        scheme_ = nullptr;
    }
    nvs_sec_config_hmac_t scheme_config{};
    scheme_config.hmac_key_id = static_cast<hmac_key_id_t>(key_id);
    esp_err_t result = nvs_sec_provider_register_hmac(&scheme_config, &scheme_);
    if (result != ESP_OK) { return result; }
    nvs_sec_cfg_t config{};
    // Read-only derivation rejects a missing/wrong-purpose key. NEVER call
    // nvs_flash_generate_keys_v2 or nvs_flash_init here (they can burn eFuse).
    result = nvs_flash_read_security_cfg_v2(scheme_, &config);
    if (result != ESP_OK) { wipe(&config, sizeof(config)); return result; }
    // IDF secure_init returns success for an already initialized plaintext
    // partition. Exclusively deinitialize ours first, then require non-null cfg.
    result = nvs_flash_deinit_partition(kPartition);
    if (result != ESP_OK && result != ESP_ERR_NVS_NOT_INITIALIZED) {
        wipe(&config, sizeof(config)); return result;
    }
    initialized_ = false;
    result = nvs_flash_secure_init_partition(kPartition, &config);
    wipe(&config, sizeof(config));
    if (result != ESP_OK) { return result; }
    initialized_ = true;
    ready_ = true;
    return ESP_OK;
}

StorageLoad IdfOwnerStore::load(OwnerRecord& record) noexcept {
    wipe(&record, sizeof(record));
    if (!ready_) { return StorageLoad::kFault; }
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open_from_partition(kPartition, kNamespace, NVS_READONLY, &handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) { return StorageLoad::kAbsent; }
    if (result != ESP_OK) { ready_ = false; return StorageLoad::kFault; }
    OwnerRecordBlob blob{};
    std::size_t size = blob.size();
    result = nvs_get_blob(handle, kRecordKey, blob.data(), &size);
    nvs_close(handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        wipe(blob.data(), blob.size());
        return StorageLoad::kAbsent;
    }
    const bool valid = result == ESP_OK && decode_owner_record(blob.data(), size, record);
    wipe(blob.data(), blob.size());
    if (!valid) { ready_ = false; return StorageLoad::kFault; }
    return StorageLoad::kOk;
}

StoragePort IdfOwnerStore::port() noexcept { return {this, commit}; }
bool IdfOwnerStore::commit(void* context, const OwnerRecord& record) noexcept {
    auto* store = static_cast<IdfOwnerStore*>(context);
    if (store == nullptr || !store->ready_) { return false; }
    OwnerRecordBlob blob{};
    if (!encode_owner_record(record, blob)) { return false; }
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open_from_partition(kPartition, kNamespace, NVS_READWRITE, &handle);
    if (result == ESP_OK) { result = nvs_set_blob(handle, kRecordKey, blob.data(), blob.size()); }
    if (result == ESP_OK) { result = nvs_commit(handle); }
    if (handle != 0) { nvs_close(handle); }
    // Reopen read-only after commit. Any ambiguous failure locks this adapter
    // until explicit reinitialization/reload; no optimistic bound acknowledgment.
    OwnerRecordBlob readback{};
    if (result == ESP_OK) {
        handle = 0;
        result = nvs_open_from_partition(kPartition, kNamespace, NVS_READONLY, &handle);
        std::size_t size = readback.size();
        if (result == ESP_OK) { result = nvs_get_blob(handle, kRecordKey, readback.data(), &size); }
        if (handle != 0) { nvs_close(handle); }
        if (result == ESP_OK && (size != blob.size() ||
            !constant_time_equal(blob.data(), readback.data(), blob.size()))) { result = ESP_FAIL; }
    }
    wipe(blob.data(), blob.size()); wipe(readback.data(), readback.size());
    if (result != ESP_OK) { store->ready_ = false; }
    return result == ESP_OK;
}
}  // namespace mii::auth
