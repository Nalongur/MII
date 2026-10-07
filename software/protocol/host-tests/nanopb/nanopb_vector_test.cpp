#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

#include "mii/protocol_codec.hpp"
#include "mii/protocol_self_test.hpp"

namespace {

using Bytes = std::vector<std::uint8_t>;

std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open vector: " + path);
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string json_string(const std::string& json, const std::string& name) {
    const std::regex pattern{"\\\"" + name + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\""};
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        throw std::runtime_error("missing JSON string: " + name);
    }
    return match[1].str();
}

std::uint64_t json_uint(const std::string& json, const std::string& name) {
    const std::regex pattern{"\\\"" + name + "\\\"\\s*:\\s*([0-9]+)"};
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        throw std::runtime_error("missing JSON integer: " + name);
    }
    return std::stoull(match[1].str());
}

bool json_bool(const std::string& json, const std::string& name) {
    const std::regex pattern{"\\\"" + name + "\\\"\\s*:\\s*(true|false)"};
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        throw std::runtime_error("missing JSON boolean: " + name);
    }
    return match[1].str() == "true";
}

std::string normalized_hex(std::string value) {
    value.erase(
        std::remove_if(value.begin(), value.end(), [](unsigned char character) {
            return std::isspace(character) != 0 || character == ':' || character == '_' ||
                   character == '-';
        }),
        value.end());
    return value;
}

Bytes hex_bytes(const std::string& value) {
    const std::string normalized = normalized_hex(value);
    if (normalized.empty() || normalized.size() % 2U != 0U) {
        throw std::runtime_error("invalid expected hex length");
    }
    Bytes result;
    result.reserve(normalized.size() / 2U);
    for (std::size_t index = 0; index < normalized.size(); index += 2U) {
        result.push_back(static_cast<std::uint8_t>(
            std::stoul(normalized.substr(index, 2U), nullptr, 16)));
    }
    return result;
}

std::string to_hex(const Bytes& bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2U);
    for (const std::uint8_t byte : bytes) {
        result.push_back(digits[byte >> 4U]);
        result.push_back(digits[byte & 0x0fU]);
    }
    return result;
}

Bytes encode_envelope(const std::string& json) {
    const std::string device_id = json_string(json, "device_id");
    const mii::protocol::EnvelopeMetadata metadata{
        json_uint(json, "message_id"),
        device_id.c_str(),
        json_uint(json, "sequence_number"),
        static_cast<std::int64_t>(json_uint(json, "sent_at_unix_ms")),
    };
    std::array<std::uint8_t, mii::protocol::kMaxEncodedEnvelopeSize> buffer{};
    const auto result = mii::protocol::encode_device_status_request(
        metadata,
        json_bool(json, "include_runtime_counters"),
        buffer.data(),
        buffer.size());
    if (!result.ok()) {
        throw std::runtime_error(
            std::string{"codec encode failed: "} +
            mii::protocol::codec_error_name(result.error));
    }
    return {buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(result.bytes_written)};
}

void verify_decoded(const Bytes& wire, const std::string& json) {
    mii::protocol::DecodedDeviceStatusRequest decoded{};
    const auto result = mii::protocol::decode_device_status_request(
        wire.data(),
        wire.size(),
        &decoded);
    if (result != mii::protocol::CodecError::kOk) {
        throw std::runtime_error(
            std::string{"codec decode failed: "} +
            mii::protocol::codec_error_name(result));
    }
    if (decoded.protocol_major != json_uint(json, "major") ||
        decoded.protocol_minor != json_uint(json, "minor") ||
        decoded.protocol_patch != json_uint(json, "patch") ||
        decoded.message_id != json_uint(json, "message_id") ||
        std::string{decoded.device_id.data()} != json_string(json, "device_id") ||
        decoded.sequence_number != json_uint(json, "sequence_number") ||
        decoded.sent_at_unix_ms != static_cast<std::int64_t>(
                                         json_uint(json, "sent_at_unix_ms")) ||
        decoded.include_runtime_counters != json_bool(json, "include_runtime_counters")) {
        throw std::runtime_error("decoded codec fields do not match vector");
    }
}

void verify_guard_paths(const std::string& json, const Bytes& expected_envelope) {
    const std::string device_id = json_string(json, "device_id");
    mii::protocol::EnvelopeMetadata metadata{
        json_uint(json, "message_id"),
        device_id.c_str(),
        json_uint(json, "sequence_number"),
        static_cast<std::int64_t>(json_uint(json, "sent_at_unix_ms")),
    };

    std::array<std::uint8_t, 1> tiny_buffer{};
    const auto tiny_result = mii::protocol::encode_device_status_request(
        metadata,
        true,
        tiny_buffer.data(),
        tiny_buffer.size());
    if (tiny_result.error != mii::protocol::CodecError::kOutputTooSmall) {
        throw std::runtime_error("tiny output buffer was not rejected");
    }

    const std::string oversized_id(mii::protocol::kMaxDeviceIdLength + 1, 'X');
    metadata.device_id = oversized_id.c_str();
    std::array<std::uint8_t, mii::protocol::kMaxEncodedEnvelopeSize> buffer{};
    const auto oversized_result = mii::protocol::encode_device_status_request(
        metadata,
        true,
        buffer.data(),
        buffer.size());
    if (oversized_result.error != mii::protocol::CodecError::kDeviceIdTooLong) {
        throw std::runtime_error("oversized device ID was not rejected");
    }

    mii::protocol::DecodedDeviceStatusRequest decoded{};
    const std::array<std::uint8_t, 1> truncated{0x0a};
    if (mii::protocol::decode_device_status_request(
            truncated.data(),
            truncated.size(),
            &decoded) != mii::protocol::CodecError::kDecodeFailed) {
        throw std::runtime_error("truncated envelope was not rejected");
    }

    Bytes unsupported_version = expected_envelope;
    unsupported_version.at(3) = 0x02;
    if (mii::protocol::decode_device_status_request(
            unsupported_version.data(),
            unsupported_version.size(),
            &decoded) != mii::protocol::CodecError::kUnsupportedProtocolVersion) {
        throw std::runtime_error("unsupported protocol version was not rejected");
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            throw std::runtime_error("usage: nanopb_vector_test <vector.json>");
        }
        const std::string json = read_file(argv[1]);
        const Bytes expected_payload = hex_bytes(json_string(json, "expected_payload_wire_hex"));
        const Bytes expected_envelope = hex_bytes(json_string(json, "expected_wire_hex"));
        if (expected_payload != Bytes{0x08U, 0x01U}) {
            throw std::runtime_error("fixed payload vector changed unexpectedly");
        }

        const Bytes envelope = encode_envelope(json);
        if (envelope != expected_envelope ||
            envelope.size() != json_uint(json, "expected_wire_length")) {
            throw std::runtime_error("envelope mismatch: expected " + to_hex(expected_envelope) +
                                     ", got " + to_hex(envelope));
        }
        verify_decoded(expected_envelope, json);

        Bytes future_wire = expected_envelope;
        future_wire.insert(future_wire.end(), {0xf8U, 0x07U, 0x01U});
        verify_decoded(future_wire, json);
        verify_guard_paths(json, expected_envelope);

        if (!mii::protocol::run_startup_self_test()) {
            throw std::runtime_error("firmware startup self-tests failed on host");
        }

        std::cout << "firmware protocol codec PASS: " << envelope.size() << " bytes\n";
        std::cout << "codec guards PASS: capacity, device ID, truncation, version\n";
        std::cout << "nanopb unknown-field skip PASS: field 127\n";
        std::cout << to_hex(envelope) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "firmware protocol codec FAIL: " << error.what() << '\n';
        return 1;
    }
}
