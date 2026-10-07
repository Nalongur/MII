#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "mii/frame_codec.hpp"

namespace {

using mii::protocol::FrameDecodeStatus;
using mii::protocol::FrameError;
using mii::protocol::FrameStreamDecoder;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "FRAME_CODEC_TEST_FAIL: " << message << '\n';
    std::exit(1);
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
}

std::string read_file(const char* path) {
    std::ifstream input(path, std::ios::binary);
    require(input.good(), "cannot open vector file");
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

std::string json_string(const std::string& json, const std::string& field) {
    const std::regex pattern("\\\"" + field + "\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
    std::smatch match;
    require(std::regex_search(json, match, pattern), "missing JSON field: " + field);
    return match[1].str();
}

std::vector<std::uint8_t> from_hex(const std::string& hex) {
    require(hex.size() % 2 == 0, "odd hex length");
    std::vector<std::uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (std::size_t index = 0; index < hex.size(); index += 2) {
        const auto value = std::stoul(hex.substr(index, 2), nullptr, 16);
        bytes.push_back(static_cast<std::uint8_t>(value));
    }
    return bytes;
}

std::uint16_t crc16(const std::uint8_t* data, std::size_t size) {
    std::uint16_t crc = 0xffffU;
    for (std::size_t index = 0; index < size; ++index) {
        crc ^= static_cast<std::uint16_t>(data[index]) << 8U;
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000U) != 0U
                      ? static_cast<std::uint16_t>((crc << 1U) ^ 0x1021U)
                      : static_cast<std::uint16_t>(crc << 1U);
        }
    }
    return crc;
}

void set_header_crc(std::vector<std::uint8_t>* frame) {
    const std::uint16_t value = crc16(frame->data(), 6);
    (*frame)[6] = static_cast<std::uint8_t>(value & 0xffU);
    (*frame)[7] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
}

void expect_frame(
    const FrameStreamDecoder& decoder,
    const std::vector<std::uint8_t>& payload,
    const std::string& context) {
    require(decoder.frame_ready(), context + ": frame not ready");
    require(decoder.frame_size() == payload.size(), context + ": wrong size");
    require(std::memcmp(decoder.frame_data(), payload.data(), payload.size()) == 0,
            context + ": wrong payload");
}

void test_fragment_size(
    const std::vector<std::uint8_t>& frame,
    const std::vector<std::uint8_t>& payload,
    std::size_t fragment_size) {
    FrameStreamDecoder decoder;
    std::size_t offset = 0;
    while (offset < frame.size()) {
        const std::size_t size = std::min(fragment_size, frame.size() - offset);
        const auto result = decoder.push(frame.data() + offset, size);
        require(result.bytes_consumed == size, "fragment not fully consumed");
        offset += result.bytes_consumed;
        if (offset == frame.size()) {
            require(result.status == FrameDecodeStatus::kFrameReady,
                    "final fragment did not complete frame");
        } else {
            require(result.status == FrameDecodeStatus::kNeedMore,
                    "early fragment unexpectedly completed frame");
        }
    }
    expect_frame(decoder, payload, "fragmented decode");
}

void expect_header_error(
    std::vector<std::uint8_t> frame,
    FrameError expected,
    const std::string& context) {
    FrameStreamDecoder decoder;
    const auto result = decoder.push(frame.data(), frame.size());
    require(result.status == FrameDecodeStatus::kError, context + ": no error");
    require(result.error == expected, context + ": wrong error");
    require(result.bytes_consumed == mii::protocol::kFrameHeaderSize,
            context + ": consumed payload after bad header");
}

void test_every_payload_size() {
    std::uint32_t random_state = 0x4d494931U;
    for (std::size_t payload_size = 1;
         payload_size <= mii::protocol::kMaxFramePayloadSize;
         ++payload_size) {
        std::vector<std::uint8_t> payload(payload_size);
        for (std::uint8_t& byte : payload) {
            random_state ^= random_state << 13U;
            random_state ^= random_state >> 17U;
            random_state ^= random_state << 5U;
            byte = static_cast<std::uint8_t>(random_state & 0xffU);
        }

        std::vector<std::uint8_t> frame(
            mii::protocol::kFrameHeaderSize + payload_size);
        const auto encode_result = mii::protocol::encode_frame(
            payload.data(), payload.size(), frame.data(), frame.size());
        require(encode_result.ok() && encode_result.bytes_written == frame.size(),
                "payload-size sweep encode failed");

        FrameStreamDecoder decoder;
        const std::size_t fragment_size = (payload_size % 31U) + 1U;
        std::size_t offset = 0;
        while (offset < frame.size()) {
            const std::size_t size =
                std::min(fragment_size, frame.size() - offset);
            const auto result = decoder.push(frame.data() + offset, size);
            require(result.bytes_consumed == size,
                    "payload-size sweep left input unconsumed");
            offset += size;
            require(
                result.status == (offset == frame.size()
                                      ? FrameDecodeStatus::kFrameReady
                                      : FrameDecodeStatus::kNeedMore),
                "payload-size sweep returned wrong status");
        }
        expect_frame(decoder, payload, "payload-size sweep");
    }
}

}  // namespace

int main(int argc, char** argv) {
    require(argc == 2, "usage: frame_codec_test <vector.json>");
    const std::string json = read_file(argv[1]);
    const std::vector<std::uint8_t> payload =
        from_hex(json_string(json, "payload_wire_hex"));
    const std::vector<std::uint8_t> expected_frame =
        from_hex(json_string(json, "expected_frame_wire_hex"));

    require(payload.size() == 31, "unexpected canonical payload length");
    require(expected_frame.size() == 43, "unexpected canonical frame length");

    std::array<std::uint8_t, mii::protocol::kMaxEncodedFrameSize> encoded{};
    const auto encoded_result = mii::protocol::encode_frame(
        payload.data(), payload.size(), encoded.data(), encoded.size());
    require(encoded_result.ok(), "canonical encode failed");
    require(encoded_result.bytes_written == expected_frame.size(),
            "canonical encode length mismatch");
    require(std::memcmp(encoded.data(), expected_frame.data(), expected_frame.size()) == 0,
            "canonical frame bytes mismatch");

    std::vector<std::uint8_t> overlapping(
        mii::protocol::kFrameHeaderSize + payload.size());
    std::memcpy(overlapping.data(), payload.data(), payload.size());
    const auto overlap_result = mii::protocol::encode_frame(
        overlapping.data(), payload.size(), overlapping.data(), overlapping.size());
    require(overlap_result.ok() && overlapping == expected_frame,
            "overlapping encode failed");

    require(mii::protocol::encode_frame(nullptr, 1, encoded.data(), encoded.size()).error ==
                FrameError::kInvalidArgument,
            "null payload accepted");
    require(mii::protocol::encode_frame(payload.data(), 0, encoded.data(), encoded.size()).error ==
                FrameError::kInvalidArgument,
            "empty payload accepted");
    require(mii::protocol::encode_frame(payload.data(), payload.size(), nullptr, encoded.size()).error ==
                FrameError::kInvalidArgument,
            "null output accepted");
    require(mii::protocol::encode_frame(payload.data(), payload.size(), encoded.data(), 42).error ==
                FrameError::kOutputTooSmall,
            "short output accepted");
    std::array<std::uint8_t, mii::protocol::kMaxFramePayloadSize + 1> oversized{};
    require(mii::protocol::encode_frame(
                oversized.data(), oversized.size(), encoded.data(), encoded.size()).error ==
                FrameError::kPayloadTooLarge,
            "oversized payload accepted");

    for (std::size_t fragment = 1; fragment <= 17; ++fragment) {
        test_fragment_size(expected_frame, payload, fragment);
    }

    FrameStreamDecoder consecutive;
    std::vector<std::uint8_t> doubled = expected_frame;
    doubled.insert(doubled.end(), expected_frame.begin(), expected_frame.end());
    auto result = consecutive.push(doubled.data(), doubled.size());
    require(result.status == FrameDecodeStatus::kFrameReady &&
                result.bytes_consumed == expected_frame.size(),
            "first consecutive frame boundary wrong");
    expect_frame(consecutive, payload, "first consecutive frame");
    const auto pending = consecutive.push(
        doubled.data() + result.bytes_consumed,
        doubled.size() - result.bytes_consumed);
    require(pending.error == FrameError::kFramePending && pending.bytes_consumed == 0,
            "pending frame guard failed");
    require(consecutive.release_frame() == FrameError::kOk, "release failed");
    result = consecutive.push(
        doubled.data() + expected_frame.size(), expected_frame.size());
    require(result.status == FrameDecodeStatus::kFrameReady, "second frame missing");
    expect_frame(consecutive, payload, "second consecutive frame");

    for (std::size_t index = mii::protocol::kFrameHeaderSize;
         index < expected_frame.size(); ++index) {
        std::vector<std::uint8_t> corrupt = expected_frame;
        corrupt[index] ^= 0x01U;
        FrameStreamDecoder decoder;
        const auto corrupt_result = decoder.push(corrupt.data(), corrupt.size());
        require(corrupt_result.error == FrameError::kPayloadChecksumMismatch,
                "payload corruption was not detected");
    }

    std::vector<std::uint8_t> bad_header_crc = expected_frame;
    bad_header_crc[4] ^= 0x01U;
    expect_header_error(bad_header_crc, FrameError::kHeaderChecksumMismatch,
                        "header checksum");
    std::vector<std::uint8_t> bad_version = expected_frame;
    bad_version[2] = 2;
    set_header_crc(&bad_version);
    expect_header_error(bad_version, FrameError::kUnsupportedVersion, "version");
    std::vector<std::uint8_t> bad_flags = expected_frame;
    bad_flags[3] = 1;
    set_header_crc(&bad_flags);
    expect_header_error(bad_flags, FrameError::kUnsupportedFlags, "flags");
    std::vector<std::uint8_t> empty = expected_frame;
    empty[4] = 0;
    empty[5] = 0;
    set_header_crc(&empty);
    expect_header_error(empty, FrameError::kInvalidPayloadLength, "empty length");
    std::vector<std::uint8_t> too_large = expected_frame;
    too_large[4] = 0x5d;
    too_large[5] = 0x02;
    set_header_crc(&too_large);
    expect_header_error(too_large, FrameError::kInvalidPayloadLength, "large length");

    FrameStreamDecoder noisy;
    std::vector<std::uint8_t> noise{0x00, 0x4d, 0x4d, 0x00, 0xff};
    noise.insert(noise.end(), expected_frame.begin(), expected_frame.end());
    result = noisy.push(noise.data(), noise.size());
    require(result.status == FrameDecodeStatus::kFrameReady &&
                result.discarded_bytes == 5,
            "noise resynchronization failed");
    expect_frame(noisy, payload, "noise recovery");

    FrameStreamDecoder suffix_recovery;
    std::array<std::uint8_t, mii::protocol::kFrameHeaderSize> malformed{};
    malformed[0] = mii::protocol::kFrameMagicFirst;
    malformed[1] = mii::protocol::kFrameMagicSecond;
    malformed[10] = mii::protocol::kFrameMagicFirst;
    malformed[11] = mii::protocol::kFrameMagicSecond;
    result = suffix_recovery.push(malformed.data(), malformed.size());
    require(result.error == FrameError::kHeaderChecksumMismatch &&
                result.discarded_bytes == 10,
            "malformed-header suffix was not retained");
    result = suffix_recovery.push(
        expected_frame.data() + 2, expected_frame.size() - 2);
    require(result.status == FrameDecodeStatus::kFrameReady,
            "valid frame after malformed suffix did not recover");
    expect_frame(suffix_recovery, payload, "header suffix recovery");

    FrameStreamDecoder timeout;
    require(timeout.expire_partial_frame() == FrameError::kOk,
            "idle decoder reported timeout");
    result = timeout.push(expected_frame.data(), 8);
    require(result.status == FrameDecodeStatus::kNeedMore && timeout.has_partial_frame(),
            "partial header not retained");
    require(timeout.expire_partial_frame() == FrameError::kTimeout &&
                !timeout.has_partial_frame(),
            "partial frame timeout failed");
    result = timeout.push(expected_frame.data(), expected_frame.size());
    require(result.status == FrameDecodeStatus::kFrameReady, "post-timeout recovery failed");
    require(timeout.expire_partial_frame() == FrameError::kFramePending,
            "ready frame was expired");
    timeout.reset();
    require(!timeout.frame_ready() && timeout.frame_data() == nullptr &&
                timeout.frame_size() == 0,
            "reset did not clear ready frame");
    require(timeout.release_frame() == FrameError::kInvalidArgument,
            "release without frame accepted");
    require(timeout.push(nullptr, 1).error == FrameError::kInvalidArgument,
            "null input accepted");
    require(timeout.push(nullptr, 0).status == FrameDecodeStatus::kNeedMore,
            "empty input rejected");

    test_every_payload_size();

    std::cout << "FRAME_CODEC_TEST_PASS: exact vector, 1..17 byte fragments, "
                 "all 1..604 byte payloads, consecutive frames, corruption, "
                 "resync, and timeout\n";
    return 0;
}
