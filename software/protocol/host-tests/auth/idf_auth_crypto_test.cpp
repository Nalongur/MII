#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

#include "esp_bt.h"
#include "mbedtls/platform.h"
#include "mii/idf_auth_crypto.hpp"

namespace {
using namespace mii::auth;
bool fail_allocation = false;
std::size_t allocation_calls = 0, live_bytes = 0, peak_bytes = 0;
std::size_t fail_after_call = 0;
unsigned random_calls = 0, checks = 0;
std::unordered_map<void*, std::size_t> allocations;
esp_bt_controller_status_t controller_status = ESP_BT_CONTROLLER_STATUS_IDLE;
void require(bool ok, const char* description) {
    ++checks;
    if (!ok) { std::cerr << "IDF_AUTH_CRYPTO_TEST_FAIL: " << description << '\n'; std::exit(1); }
}
void* checked_calloc(std::size_t count, std::size_t size) {
    ++allocation_calls;
    if (fail_allocation || (fail_after_call != 0 && allocation_calls >= fail_after_call)) { return nullptr; }
    auto* output = std::calloc(count, size);
    if (output != nullptr) {
        allocations[output] = count * size; live_bytes += count * size;
        peak_bytes = std::max(peak_bytes, live_bytes);
    }
    return output;
}
void checked_free(void* data) {
    if (data != nullptr) { live_bytes -= allocations.at(data); allocations.erase(data); }
    std::free(data);
}
std::string hex(const Secret& value) {
    const char* digits = "0123456789abcdef"; std::string output;
    for (const auto byte : value) { output += digits[byte >> 4]; output += digits[byte & 15]; }
    return output;
}
std::string field(const std::string& json, const char* name) {
    const auto key = json.find('"' + std::string(name) + '"');
    require(key != std::string::npos, "vector field");
    const auto start = json.find('"', json.find(':', key)) + 1;
    return json.substr(start, json.find('"', start) - start);
}
std::uint8_t unhex(char byte) { return static_cast<std::uint8_t>(byte <= '9' ? byte - '0' : byte - 'a' + 10); }
}  // namespace

esp_bt_controller_status_t esp_bt_controller_get_status() { return controller_status; }
void esp_fill_random(void* output, std::size_t size) {
    ++random_calls; std::memset(output, 0x5a, size); // Plumbing stub, not entropy evidence.
}

int main(int argc, char** argv) {
    require(argc == 2, "vector path");
    require(mbedtls_platform_set_calloc_free(checked_calloc, checked_free) == 0, "allocator instrumentation");
    {
        IdfAuthCrypto crypto; const auto port = crypto.port(); Secret out{};
        require(!port.hmac(port.context, {}, reinterpret_cast<const std::uint8_t*>("test"), 4, out), "closed before setup");
        fail_allocation = true;
        require(crypto.initialize() == ESP_ERR_NO_MEM && live_bytes == 0, "setup allocation failure cleanup");
        fail_allocation = false;
        fail_after_call = allocation_calls + 2;
        require(crypto.initialize() == ESP_ERR_NO_MEM && live_bytes == 0, "partial setup failure cleanup");
        fail_after_call = 0;
        require(crypto.initialize() == ESP_OK, "retry setup");
        const auto initial_allocations = allocation_calls;
        require(crypto.initialize() == ESP_OK && allocation_calls == initial_allocations, "one-time setup");
        Secret key{}; std::fill_n(key.begin(), 20, 0x0b);
        const std::uint8_t data[]{'H','i',' ','T','h','e','r','e'};
        require(port.hmac(port.context, key, data, sizeof(data), out), "real mbedTLS HMAC");
        require(hex(out) == "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7", "RFC4231 output");
        std::ifstream file(argv[1]); require(file.good(), "vector input");
        const std::string json((std::istreambuf_iterator<char>(file)), {});
        const auto transcript_hex = field(json, "client_transcript_hex");
        const auto key_hex = field(json, "owner_key_hex");
        require(transcript_hex.size() == 226 && key_hex.size() == 64, "vector sizes");
        std::array<std::uint8_t, 113> transcript{};
        for (std::size_t i = 0; i < transcript.size(); ++i) {
            transcript[i] = static_cast<std::uint8_t>((unhex(transcript_hex[i*2]) << 4) | unhex(transcript_hex[i*2+1]));
        }
        for (std::size_t i = 0; i < key.size(); ++i) {
            key[i] = static_cast<std::uint8_t>((unhex(key_hex[i*2]) << 4) | unhex(key_hex[i*2+1]));
        }
        for (const auto& entry : std::array<std::pair<char, const char*>, 4>{{
            {'D', "device_proof_hex"}, {'C', "client_proof_hex"},
            {'K', "session_key_hex"}, {'I', "session_id_mac_hex"}}}) {
            transcript[8] = entry.first;
            require(port.hmac(port.context, key, transcript.data(), transcript.size(), out), "vector HMAC");
            require(hex(out) == field(json, entry.second), "Python/CNG/mbedTLS exact vector");
        }
        for (unsigned i = 0; i < 1000; ++i) {
            require(port.hmac(port.context, key, transcript.data(), transcript.size(), out), "repeated HMAC");
        }
        require(allocation_calls == initial_allocations, "no per-HMAC allocation");
        require(!port.hmac(port.context, key, nullptr, 1, out) && out == Secret{}, "null input clears output");
        require(!port.hmac(port.context, key, transcript.data(), 631, out), "oversized input rejected");
        require(!port.random(port.context, out.data(), out.size()) && random_calls == 0 && out == Secret{},
                "idle controller entropy rejected and output cleared");
        controller_status = ESP_BT_CONTROLLER_STATUS_INITED;
        require(!port.random(port.context, out.data(), out.size()) && random_calls == 0, "initialized but disabled entropy rejected");
        controller_status = ESP_BT_CONTROLLER_STATUS_ENABLED;
        require(!port.random(port.context, out.data(), 31) && random_calls == 0, "random output bound");
        require(port.random(port.context, out.data(), out.size()) && random_calls == 1, "enabled entropy call plumbing");
    }
    require(live_bytes == 0 && allocations.empty(), "destructor frees bounded context");
    std::cout << "IDF_AUTH_CRYPTO_TEST_PASS: " << checks << " assertions; pinned real mbedTLS; peak workspace "
              << peak_bytes << " bytes; no per-HMAC allocations; RNG adapter plumbing only\n";
}
