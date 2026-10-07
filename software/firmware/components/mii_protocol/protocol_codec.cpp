#include "mii/protocol_codec.hpp"

#include <algorithm>
#include <array>
#include <cstring>

#include "mii.pb.h"
#include "pb_decode.h"
#include "pb_encode.h"

namespace mii::protocol {
namespace {

static_assert(kMaxEncodedEnvelopeSize == mii_protocol_v1_Envelope_size);
static_assert(
    kMaxDeviceIdLength + 1 ==
    sizeof(((mii_protocol_v1_Envelope*)nullptr)->device_id));

std::size_t bounded_string_length(const char* value, std::size_t maximum) noexcept {
    if (value == nullptr) {
        return 0;
    }
    std::size_t length = 0;
    while (length <= maximum && value[length] != '\0') {
        ++length;
    }
    return length;
}

}  // namespace

EncodeResult encode_device_status_request(
    const EnvelopeMetadata& metadata,
    bool include_runtime_counters,
    std::uint8_t* output,
    std::size_t output_capacity) noexcept {
    if (metadata.device_id == nullptr || output == nullptr || output_capacity == 0) {
        return {CodecError::kInvalidArgument, 0};
    }

    const std::size_t device_id_length =
        bounded_string_length(metadata.device_id, kMaxDeviceIdLength);
    if (device_id_length == 0) {
        return {CodecError::kInvalidArgument, 0};
    }
    if (device_id_length > kMaxDeviceIdLength) {
        return {CodecError::kDeviceIdTooLong, 0};
    }

    mii_protocol_v1_DeviceStatusRequest payload =
        mii_protocol_v1_DeviceStatusRequest_init_zero;
    payload.include_runtime_counters = include_runtime_counters;

    std::array<pb_byte_t, mii_protocol_v1_DeviceStatusRequest_size> payload_buffer{};
    pb_ostream_t payload_stream =
        pb_ostream_from_buffer(payload_buffer.data(), payload_buffer.size());
    if (!pb_encode(
            &payload_stream,
            &mii_protocol_v1_DeviceStatusRequest_msg,
            &payload)) {
        return {CodecError::kEncodeFailed, 0};
    }

    mii_protocol_v1_Envelope envelope = mii_protocol_v1_Envelope_init_zero;
    envelope.has_protocol_version = true;
    envelope.protocol_version.major = kProtocolMajor;
    envelope.protocol_version.minor = kProtocolMinor;
    envelope.protocol_version.patch = kProtocolPatch;
    envelope.message_id = metadata.message_id;
    std::memcpy(
        envelope.device_id,
        metadata.device_id,
        device_id_length + 1);
    envelope.sequence_number = metadata.sequence_number;
    envelope.sent_at_unix_ms = metadata.sent_at_unix_ms;
    envelope.payload_type =
        mii_protocol_v1_PayloadType_PAYLOAD_TYPE_DEVICE_STATUS_REQUEST;
    envelope.payload.size = payload_stream.bytes_written;
    std::copy_n(
        payload_buffer.begin(),
        payload_stream.bytes_written,
        envelope.payload.bytes);

    std::size_t encoded_size = 0;
    if (!pb_get_encoded_size(
            &encoded_size,
            &mii_protocol_v1_Envelope_msg,
            &envelope)) {
        return {CodecError::kEncodeFailed, 0};
    }
    if (encoded_size > output_capacity) {
        return {CodecError::kOutputTooSmall, 0};
    }

    pb_ostream_t envelope_stream =
        pb_ostream_from_buffer(output, output_capacity);
    if (!pb_encode(
            &envelope_stream,
            &mii_protocol_v1_Envelope_msg,
            &envelope)) {
        return {CodecError::kEncodeFailed, 0};
    }

    return {CodecError::kOk, envelope_stream.bytes_written};
}

CodecError decode_device_status_request(
    const std::uint8_t* input,
    std::size_t input_size,
    DecodedDeviceStatusRequest* output) noexcept {
    if (input == nullptr || input_size == 0 || output == nullptr) {
        return CodecError::kInvalidArgument;
    }

    mii_protocol_v1_Envelope envelope = mii_protocol_v1_Envelope_init_zero;
    pb_istream_t envelope_stream = pb_istream_from_buffer(input, input_size);
    if (!pb_decode(
            &envelope_stream,
            &mii_protocol_v1_Envelope_msg,
            &envelope)) {
        return CodecError::kDecodeFailed;
    }

    if (!envelope.has_protocol_version ||
        envelope.protocol_version.major != kProtocolMajor ||
        envelope.protocol_version.minor != kProtocolMinor ||
        envelope.protocol_version.patch != kProtocolPatch) {
        return CodecError::kUnsupportedProtocolVersion;
    }
    if (envelope.payload_type !=
        mii_protocol_v1_PayloadType_PAYLOAD_TYPE_DEVICE_STATUS_REQUEST) {
        return CodecError::kUnexpectedPayloadType;
    }

    mii_protocol_v1_DeviceStatusRequest payload =
        mii_protocol_v1_DeviceStatusRequest_init_zero;
    pb_istream_t payload_stream =
        pb_istream_from_buffer(envelope.payload.bytes, envelope.payload.size);
    if (!pb_decode(
            &payload_stream,
            &mii_protocol_v1_DeviceStatusRequest_msg,
            &payload)) {
        return CodecError::kDecodeFailed;
    }

    DecodedDeviceStatusRequest decoded{};
    decoded.protocol_major = envelope.protocol_version.major;
    decoded.protocol_minor = envelope.protocol_version.minor;
    decoded.protocol_patch = envelope.protocol_version.patch;
    decoded.message_id = envelope.message_id;
    std::copy_n(
        envelope.device_id,
        decoded.device_id.size(),
        decoded.device_id.begin());
    decoded.device_id.back() = '\0';
    decoded.sequence_number = envelope.sequence_number;
    decoded.sent_at_unix_ms = envelope.sent_at_unix_ms;
    decoded.include_runtime_counters = payload.include_runtime_counters;
    *output = decoded;
    return CodecError::kOk;
}

const char* codec_error_name(CodecError error) noexcept {
    switch (error) {
        case CodecError::kOk:
            return "ok";
        case CodecError::kInvalidArgument:
            return "invalid_argument";
        case CodecError::kDeviceIdTooLong:
            return "device_id_too_long";
        case CodecError::kOutputTooSmall:
            return "output_too_small";
        case CodecError::kEncodeFailed:
            return "encode_failed";
        case CodecError::kDecodeFailed:
            return "decode_failed";
        case CodecError::kUnsupportedProtocolVersion:
            return "unsupported_protocol_version";
        case CodecError::kUnexpectedPayloadType:
            return "unexpected_payload_type";
    }
    return "unknown";
}

}  // namespace mii::protocol
