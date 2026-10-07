#include "mii/owner_record_codec.hpp"

#include <cstring>

namespace mii::auth {
namespace {
std::uint32_t crc32(const std::uint8_t* data, std::size_t size) noexcept {
    std::uint32_t crc = 0xffffffffU;
    for (std::size_t index = 0; index < size; ++index) {
        crc ^= data[index];
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
        }
    }
    return crc ^ 0xffffffffU;
}
void store32(std::uint8_t* out, std::uint32_t value) noexcept {
    for (unsigned i = 0; i < 4; ++i) { out[i] = static_cast<std::uint8_t>(value >> (8 * i)); }
}
std::uint32_t load32(const std::uint8_t* in) noexcept {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) { value |= static_cast<std::uint32_t>(in[i]) << (8 * i); }
    return value;
}
}  // namespace

bool encode_owner_record(const OwnerRecord& record, OwnerRecordBlob& out) noexcept {
    wipe(out.data(), out.size());
    if (!valid_record(record)) { return false; }
    std::memcpy(out.data(), "MIIO", 4);
    out[4] = 1;
    out[5] = record.bound ? 1 : 0;
    for (unsigned i = 0; i < 8; ++i) { out[8 + i] = static_cast<std::uint8_t>(record.generation >> (8 * i)); }
    std::memcpy(out.data() + 16, record.credential.owner_id.data(), 16);
    std::memcpy(out.data() + 32, record.credential.owner_key.data(), 32);
    std::memcpy(out.data() + 64, record.credential.https_token.data(), 32);
    std::memcpy(out.data() + 96, record.credential.certificate_sha256.data(), 32);
    store32(out.data() + 128, crc32(out.data(), 128));
    return true;
}

bool decode_owner_record(const std::uint8_t* data, std::size_t size,
                         OwnerRecord& out) noexcept {
    wipe(&out, sizeof(out));
    if (data == nullptr || size != kOwnerRecordBlobSize ||
        std::memcmp(data, "MIIO", 4) != 0 || data[4] != 1 || data[5] > 1 ||
        data[6] != 0 || data[7] != 0 || load32(data + 128) != crc32(data, 128)) {
        return false;
    }
    out.format_version = data[4]; out.bound = data[5] == 1;
    for (unsigned i = 0; i < 8; ++i) { out.generation |= static_cast<std::uint64_t>(data[8 + i]) << (8 * i); }
    std::memcpy(out.credential.owner_id.data(), data + 16, 16);
    std::memcpy(out.credential.owner_key.data(), data + 32, 32);
    std::memcpy(out.credential.https_token.data(), data + 64, 32);
    std::memcpy(out.credential.certificate_sha256.data(), data + 96, 32);
    if (!valid_record(out)) { wipe(&out, sizeof(out)); return false; }
    return true;
}
}  // namespace mii::auth
