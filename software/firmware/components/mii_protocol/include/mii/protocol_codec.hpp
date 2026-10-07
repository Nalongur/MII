#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace mii::protocol {

inline constexpr std::uint32_t kProtocolMajor = 1;
inline constexpr std::uint32_t kProtocolMinor = 0;
inline constexpr std::uint32_t kProtocolPatch = 0;
inline constexpr std::size_t kMaxDeviceIdLength = 32;
inline constexpr std::size_t kMaxEncodedEnvelopeSize = 604;

enum class CodecError : std::uint8_t {
    kOk = 0,
    kInvalidArgument,
    kDeviceIdTooLong,
    kOutputTooSmall,
    kEncodeFailed,
    kDecodeFailed,
    kUnsupportedProtocolVersion,
    kUnexpectedPayloadType,
};

struct EnvelopeMetadata {
    std::uint64_t message_id = 0;
    const char* device_id = nullptr;
    std::uint64_t sequence_number = 0;
    std::int64_t sent_at_unix_ms = 0;
};

struct EncodeResult {
    CodecError error = CodecError::kInvalidArgument;
    std::size_t bytes_written = 0;

    [[nodiscard]] constexpr bool ok() const noexcept {
        return error == CodecError::kOk;
    }
};

struct DecodedDeviceStatusRequest {
    std::uint32_t protocol_major = 0;
    std::uint32_t protocol_minor = 0;
    std::uint32_t protocol_patch = 0;
    std::uint64_t message_id = 0;
    std::array<char, kMaxDeviceIdLength + 1> device_id{};
    std::uint64_t sequence_number = 0;
    std::int64_t sent_at_unix_ms = 0;
    bool include_runtime_counters = false;
};

[[nodiscard]] EncodeResult encode_device_status_request(
    const EnvelopeMetadata& metadata,
    bool include_runtime_counters,
    std::uint8_t* output,
    std::size_t output_capacity) noexcept;

[[nodiscard]] CodecError decode_device_status_request(
    const std::uint8_t* input,
    std::size_t input_size,
    DecodedDeviceStatusRequest* output) noexcept;

[[nodiscard]] const char* codec_error_name(CodecError error) noexcept;

}  // namespace mii::protocol
