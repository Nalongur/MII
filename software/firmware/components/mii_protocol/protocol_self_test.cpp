#include "mii/protocol_self_test.hpp"

#include <array>
#include <cstdint>
#include <cstring>

#include "mii/frame_codec.hpp"
#include "mii/protocol_codec.hpp"
#include "mii/session_state.hpp"

namespace mii::protocol {
namespace {

constexpr std::array<std::uint8_t, 31> kExpectedEnvelope{
    0x0a, 0x02, 0x08, 0x01, 0x10, 0x01, 0x1a, 0x0d,
    0x4d, 0x49, 0x49, 0x2d, 0x54, 0x45, 0x53, 0x54,
    0x2d, 0x30, 0x30, 0x30, 0x31, 0x20, 0x01, 0x28,
    0x01, 0x30, 0x01, 0x3a, 0x02, 0x08, 0x01,
};

constexpr std::array<std::uint8_t, 43> kExpectedFrame{
    0x4d, 0x49, 0x01, 0x00, 0x1f, 0x00, 0xae, 0xf6,
    0xc8, 0xee, 0x7c, 0xac, 0x0a, 0x02, 0x08, 0x01,
    0x10, 0x01, 0x1a, 0x0d, 0x4d, 0x49, 0x49, 0x2d,
    0x54, 0x45, 0x53, 0x54, 0x2d, 0x30, 0x30, 0x30,
    0x31, 0x20, 0x01, 0x28, 0x01, 0x30, 0x01, 0x3a,
    0x02, 0x08, 0x01,
};

bool matches_vector(const DecodedDeviceStatusRequest& decoded) noexcept {
    return decoded.protocol_major == kProtocolMajor &&
           decoded.protocol_minor == kProtocolMinor &&
           decoded.protocol_patch == kProtocolPatch &&
           decoded.message_id == 1 &&
           std::strcmp(decoded.device_id.data(), "MII-TEST-0001") == 0 &&
           decoded.sequence_number == 1 &&
           decoded.sent_at_unix_ms == 1 &&
           decoded.include_runtime_counters;
}

}  // namespace

bool run_known_vector_self_test() noexcept {
    const EnvelopeMetadata metadata{
        1,
        "MII-TEST-0001",
        1,
        1,
    };
    std::array<std::uint8_t, kMaxEncodedEnvelopeSize> encoded{};
    const EncodeResult encode_result = encode_device_status_request(
        metadata,
        true,
        encoded.data(),
        encoded.size());
    if (!encode_result.ok() || encode_result.bytes_written != kExpectedEnvelope.size() ||
        std::memcmp(
            encoded.data(),
            kExpectedEnvelope.data(),
            kExpectedEnvelope.size()) != 0) {
        return false;
    }

    DecodedDeviceStatusRequest decoded{};
    if (decode_device_status_request(
            kExpectedEnvelope.data(),
            kExpectedEnvelope.size(),
            &decoded) != CodecError::kOk ||
        !matches_vector(decoded)) {
        return false;
    }

    std::array<std::uint8_t, kExpectedEnvelope.size() + 3> future_wire{};
    std::memcpy(
        future_wire.data(),
        kExpectedEnvelope.data(),
        kExpectedEnvelope.size());
    future_wire[kExpectedEnvelope.size()] = 0xf8;
    future_wire[kExpectedEnvelope.size() + 1] = 0x07;
    future_wire[kExpectedEnvelope.size() + 2] = 0x01;

    decoded = {};
    return decode_device_status_request(
               future_wire.data(),
               future_wire.size(),
               &decoded) == CodecError::kOk &&
           matches_vector(decoded);
}

bool run_session_state_self_test() noexcept {
    OutboundSessionCounters outbound;
    if (!outbound.begin()) {
        return false;
    }
    const CounterResult first = outbound.allocate_message();
    const CounterResult retry = outbound.allocate_retry(first.numbers.message_id);
    const CounterResult second = outbound.allocate_message();
    if (!first.ok() || first.numbers.message_id != 1 ||
        first.numbers.sequence_number != 1 || !retry.ok() ||
        retry.numbers.message_id != 1 || retry.numbers.sequence_number != 2 ||
        !second.ok() || second.numbers.message_id != 2 ||
        second.numbers.sequence_number != 3) {
        return false;
    }

    InboundSessionTracker inbound;
    if (inbound.observe(
            first.numbers.message_id,
            first.numbers.sequence_number) != InboundDecision::kAcceptNew ||
        inbound.observe(
            second.numbers.message_id,
            second.numbers.sequence_number + 1) !=
            InboundDecision::kAcceptNew ||
        inbound.observe(3, 2) != InboundDecision::kAcceptReordered ||
        inbound.observe(
            first.numbers.message_id,
            second.numbers.sequence_number + 2) !=
            InboundDecision::kDuplicateMessage ||
        inbound.observe(4, second.numbers.sequence_number + 2) !=
            InboundDecision::kDuplicateSequence) {
        return false;
    }

    inbound.reset();
    return inbound.observe(1, 1) == InboundDecision::kAcceptNew;
}

bool run_frame_codec_self_test() noexcept {
    std::array<std::uint8_t, kExpectedFrame.size()> encoded{};
    const FrameEncodeResult encode_result = encode_frame(
        kExpectedEnvelope.data(),
        kExpectedEnvelope.size(),
        encoded.data(),
        encoded.size());
    if (!encode_result.ok() || encode_result.bytes_written != encoded.size() ||
        std::memcmp(encoded.data(), kExpectedFrame.data(), encoded.size()) != 0) {
        return false;
    }

    FrameStreamDecoder decoder;
    constexpr std::size_t kFirstFragmentSize = 7;
    const FrameDecodeResult first =
        decoder.push(encoded.data(), kFirstFragmentSize);
    const FrameDecodeResult second = decoder.push(
        encoded.data() + kFirstFragmentSize,
        encoded.size() - kFirstFragmentSize);
    if (first.status != FrameDecodeStatus::kNeedMore ||
        second.status != FrameDecodeStatus::kFrameReady ||
        second.frame_size != kExpectedEnvelope.size() ||
        decoder.frame_size() != kExpectedEnvelope.size() ||
        std::memcmp(
            decoder.frame_data(),
            kExpectedEnvelope.data(),
            kExpectedEnvelope.size()) != 0) {
        return false;
    }
    return decoder.release_frame() == FrameError::kOk &&
           !decoder.frame_ready() && !decoder.has_partial_frame();
}

bool run_startup_self_test() noexcept {
    return run_known_vector_self_test() && run_session_state_self_test() &&
           run_frame_codec_self_test();
}

}  // namespace mii::protocol
