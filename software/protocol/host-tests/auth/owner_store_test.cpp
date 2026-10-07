#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "mii/idf_owner_store.hpp"
#include "nvs.h"

namespace {
using namespace mii::auth;
unsigned checks = 0;
void require(bool ok, const char* description) {
    ++checks;
    if (!ok) { std::cerr << "OWNER_STORE_TEST_FAIL: " << description << '\n'; std::exit(1); }
}
struct NvsMock {
    bool protection_present{true};
    bool initialized{false};
    bool encrypted{false};
    bool namespace_exists{false};
    bool corrupt_readback{false};
    bool wrong_readback_size{false};
    bool fail_commit_after_durable_write{false};
    std::string fail_at{};
    std::vector<std::uint8_t> staged{};
    std::vector<std::uint8_t> durable{};
    std::vector<std::string> calls{};
    unsigned handles{0};
    bool step(const char* name) { calls.emplace_back(name); return fail_at != name; }
} mock;
nvs_sec_scheme_t scheme;
template<std::size_t N> std::array<std::uint8_t, N> bytes(unsigned start) {
    std::array<std::uint8_t, N> value{};
    for (std::size_t i = 0; i < N; ++i) { value[i] = static_cast<std::uint8_t>(start + i); }
    return value;
}
const OwnerRecord bound{1, UINT64_C(0x0102030405060708), true,
    {bytes<16>(17), bytes<32>(33), bytes<32>(65), bytes<32>(97)}};
std::string hex(const OwnerRecordBlob& blob) {
    std::string output; const char* digits = "0123456789abcdef";
    for (const auto byte : blob) { output += digits[byte >> 4]; output += digits[byte & 15]; }
    return output;
}
void codec_test(const char* path) {
    OwnerRecordBlob blob{};
    require(encode_owner_record(bound, blob), "encode owner record");
    std::ifstream file(path); require(file.good(), "fixture input");
    const std::string json((std::istreambuf_iterator<char>(file)), {});
    const auto field = json.find("\"owner_record_blob_hex\"");
    require(field != std::string::npos, "fixture property");
    const auto start = json.find('"', json.find(':', field)) + 1;
    require(hex(blob) == json.substr(start, json.find('"', start) - start), "Python zlib independent blob");
    OwnerRecord decoded{};
    require(decode_owner_record(blob.data(), blob.size(), decoded), "decode record");
    OwnerRecordBlob reencoded{};
    require(encode_owner_record(decoded, reencoded) && reencoded == blob, "canonical roundtrip");
    for (std::size_t size = 0; size < blob.size(); ++size) {
        decoded = bound;
        require(!decode_owner_record(blob.data(), size, decoded), "every truncated size rejected");
        require(decoded.credential.owner_key == Secret{}, "failed decode wipes key");
    }
    for (std::size_t bit = 0; bit < blob.size() * 8; ++bit) {
        auto corrupt = blob; corrupt[bit / 8] ^= static_cast<std::uint8_t>(1U << (bit % 8));
        require(!decode_owner_record(corrupt.data(), corrupt.size(), decoded), "every single-bit corruption rejected");
    }
    require(!decode_owner_record(nullptr, blob.size(), decoded), "null buffer");
    require(!decode_owner_record(blob.data(), blob.size() + 1, decoded), "oversized record");
    auto invalid = bound; invalid.generation = 0;
    require(!encode_owner_record(invalid, reencoded), "bound zero generation invalid");
    require(reencoded == OwnerRecordBlob{}, "failed encode wipes key output");
    invalid = {}; invalid.credential.https_token[0] = 1;
    require(!encode_owner_record(invalid, reencoded), "tombstone cannot contain credentials");
    const OwnerRecord tombstone{1, bound.generation + 1, false, {}};
    require(encode_owner_record(tombstone, blob) && decode_owner_record(blob.data(), blob.size(), decoded)
        && !decoded.bound && decoded.generation == tombstone.generation, "tombstone roundtrip");
}
}  // namespace

esp_err_t nvs_sec_provider_register_hmac(const nvs_sec_config_hmac_t* config, nvs_sec_scheme_t** out) {
    require(config->hmac_key_id >= 0 && config->hmac_key_id < HMAC_KEY_MAX, "key ID range");
    if (!mock.step("register")) { return ESP_FAIL; }
    *out = &scheme; return ESP_OK;
}
esp_err_t nvs_sec_provider_deregister(nvs_sec_scheme_t*) { mock.step("deregister"); return ESP_OK; }
esp_err_t nvs_flash_read_security_cfg_v2(nvs_sec_scheme_t*, nvs_sec_cfg_t* out) {
    if (!mock.step("read_security")) { return ESP_FAIL; }
    if (!mock.protection_present) { return ESP_ERR_NVS_SEC_HMAC_KEY_NOT_FOUND; }
    std::fill_n(out->eky, 32, 0x5a); std::fill_n(out->tky, 32, 0x6b); return ESP_OK;
}
esp_err_t nvs_flash_deinit_partition(const char* partition) {
    require(std::strcmp(partition, "mii_auth") == 0, "deinit exclusive partition");
    if (!mock.step("deinit")) { return ESP_FAIL; }
    const bool was_initialized = mock.initialized;
    mock.initialized = false; mock.encrypted = false;
    return was_initialized ? ESP_OK : ESP_ERR_NVS_NOT_INITIALIZED;
}
esp_err_t nvs_flash_secure_init_partition(const char* partition, nvs_sec_cfg_t* config) {
    require(std::strcmp(partition, "mii_auth") == 0, "secure init exclusive partition");
    require(config != nullptr && config->eky[0] == 0x5a && config->tky[0] == 0x6b, "non-null derived configuration");
    require(!mock.initialized, "plaintext preinitialization must be removed");
    if (!mock.step("secure_init")) { return ESP_FAIL; }
    mock.initialized = true; mock.encrypted = true; return ESP_OK;
}
esp_err_t nvs_open_from_partition(const char* partition, const char* space,
                                 nvs_open_mode_t mode, nvs_handle_t* out) {
    require(std::strcmp(partition, "mii_auth") == 0 && std::strcmp(space, "owner") == 0,
            "only dedicated owner namespace");
    require(mock.initialized && mock.encrypted, "no plaintext open");
    if (!mock.step(mode == NVS_READONLY ? "open_read" : "open_write")) { return ESP_FAIL; }
    if (mode == NVS_READONLY && !mock.namespace_exists) { return ESP_ERR_NVS_NOT_FOUND; }
    mock.namespace_exists = true; *out = ++mock.handles; return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t, const char* key, void* output, std::size_t* size) {
    require(std::strcmp(key, "record_v1") == 0, "one owner blob key");
    if (!mock.step("get")) { return ESP_FAIL; }
    if (mock.durable.empty()) { return ESP_ERR_NVS_NOT_FOUND; }
    if (*size < mock.durable.size()) { *size = mock.durable.size(); return ESP_ERR_NVS_INVALID_LENGTH; }
    std::memcpy(output, mock.durable.data(), mock.durable.size());
    *size = mock.wrong_readback_size ? mock.durable.size() - 1 : mock.durable.size();
    if (mock.corrupt_readback) { static_cast<std::uint8_t*>(output)[0] ^= 1; }
    return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t, const char* key, const void* data, std::size_t size) {
    require(std::strcmp(key, "record_v1") == 0 && size == kOwnerRecordBlobSize, "bounded atomic blob");
    if (!mock.step("set")) { return ESP_FAIL; }
    mock.staged.assign(static_cast<const std::uint8_t*>(data), static_cast<const std::uint8_t*>(data) + size);
    return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t) {
    if (!mock.step("commit")) { return ESP_FAIL; }
    mock.durable = mock.staged;
    return mock.fail_commit_after_durable_write ? ESP_FAIL : ESP_OK;
}
void nvs_close(nvs_handle_t) { mock.step("close"); }

int main(int argc, char** argv) {
    require(argc == 2, "fixture path"); codec_test(argv[1]);
    {
        mock = {}; mock.protection_present = false; IdfOwnerStore store;
        require(store.initialize(-1) == ESP_ERR_INVALID_ARG && mock.calls.empty(), "invalid key cannot invoke platform");
        require(store.initialize(6) == ESP_ERR_INVALID_ARG && mock.calls.empty(), "out of range key");
        require(store.initialize(0) == ESP_ERR_NVS_SEC_HMAC_KEY_NOT_FOUND, "missing provisioned key refuses initialization");
        require(mock.calls == std::vector<std::string>{"register", "read_security"}, "no init/writes when protection absent");
        OwnerRecord record{}; require(store.load(record) == StorageLoad::kFault, "missing protection cannot load");
        const auto port = store.port(); require(!port.commit(port.context, bound), "missing protection cannot commit");
    }
    {
        mock = {}; mock.initialized = true; mock.encrypted = false; IdfOwnerStore store;
        require(store.initialize(0) == ESP_OK, "remove existing plaintext init");
        require(mock.calls == std::vector<std::string>{"register", "read_security", "deinit", "secure_init"},
                "read, deinit, secure init order");
        OwnerRecord record{}; require(store.load(record) == StorageLoad::kAbsent, "healthy empty partition");
        const auto port = store.port(); require(port.commit(port.context, bound), "durable commit and readback");
        require(store.load(record) == StorageLoad::kOk && record.bound && record.generation == bound.generation,
                "load committed owner");
        require(store.initialize(0) == ESP_ERR_INVALID_STATE, "no overlapping initialization");
        require(port.commit(port.context, {1, bound.generation + 1, false, {}}), "commit revocation tombstone");
        require(store.load(record) == StorageLoad::kOk && !record.bound, "load tombstone");
    }
    for (const char* operation : {"register", "read_security", "deinit", "secure_init"}) {
        mock = {}; mock.fail_at = operation; IdfOwnerStore store;
        require(store.initialize(0) != ESP_OK, "initialization failure closed");
        const auto port = store.port(); require(!port.commit(port.context, bound), "failed init cannot write");
    }
    for (const char* operation : {"open_write", "set", "commit", "open_read", "get"}) {
        mock = {}; IdfOwnerStore store; require(store.initialize(0) == ESP_OK, "write fault init");
        mock.fail_at = operation; const auto port = store.port();
        require(!port.commit(port.context, bound), "write/readback failure closed");
        mock.fail_at.clear(); const auto calls = mock.calls.size();
        require(!port.commit(port.context, bound) && mock.calls.size() == calls, "no writes after ambiguous fault");
        require(store.initialize(0) == ESP_OK, "explicit reinitialize after fault");
        OwnerRecord record{}; const auto status = store.load(record);
        require(status == (mock.durable.empty() ? StorageLoad::kAbsent : StorageLoad::kOk), "reload actual durable outcome");
    }
    for (const int fault : {0, 1, 2}) {
        mock = {}; IdfOwnerStore store; require(store.initialize(0) == ESP_OK, "readback fault init");
        mock.corrupt_readback = fault == 0; mock.wrong_readback_size = fault == 1;
        mock.fail_commit_after_durable_write = fault == 2;
        const auto port = store.port(); require(!port.commit(port.context, bound), "corrupt/short/ambiguous durable result");
        OwnerRecord record{}; require(store.load(record) == StorageLoad::kFault, "fault adapter remains closed");
    }
    {
        mock = {}; IdfOwnerStore store; require(store.initialize(0) == ESP_OK, "corrupt stored blob init");
        mock.namespace_exists = true; mock.durable.assign(133, 0x55);
        OwnerRecord record{}; require(store.load(record) == StorageLoad::kFault, "oversized stored blob closed");
        require(record.credential.owner_key == Secret{}, "load fault clears output");
    }
    std::cout << "OWNER_STORE_TEST_PASS: " << checks
              << " assertions; independent blob, all truncations/bit faults, protected-init order and commit/readback failures\n";
}
