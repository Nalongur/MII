#include "mii/auth_wire.hpp"

#include <cstring>

#include "pb_decode.h"
#include "pb_encode.h"

namespace mii::auth {
namespace {
static_assert(mii_protocol_v1_AuthenticationPacket_size <= kMaxAuthenticationPacketSize);
static_assert(sizeof(mii_protocol_v1_AuthenticationPacket) <= 640);
struct CanonicalComparison {
    const std::uint8_t* input{nullptr};
    std::size_t size{0};
    std::size_t offset{0};
    bool matches{true};
};
bool compare_encoded_bytes(pb_ostream_t* stream, const pb_byte_t* bytes,
                           std::size_t size) noexcept {
    auto* comparison = static_cast<CanonicalComparison*>(stream->state);
    if (size > comparison->size - comparison->offset || (size != 0 && bytes == nullptr)) { return false; }
    // Reuse nanopb's streaming encoder: compare chunks without allocating a
    // second 604-byte packet buffer. Continue on mismatch to consume all bytes.
    if (!constant_time_equal(comparison->input + comparison->offset, bytes, size)) {
        comparison->matches = false;
    }
    comparison->offset += size;
    return true;
}
bool valid(const mii_protocol_v1_AuthenticationPacket& packet) noexcept {
    if (packet.authentication_version != 1 || packet.request_id == 0) { return false; }
    switch (packet.which_body) {
    case mii_protocol_v1_AuthenticationPacket_binding_proposal_tag: {
        const auto& p = packet.body.binding_proposal;
        return p.owner_id.size == 16 && p.owner_key.size == 32 && p.https_token.size == 32 && p.certificate_sha256.size == 32;
    }
    case mii_protocol_v1_AuthenticationPacket_binding_accepted_tag:
        return packet.body.binding_accepted.owner_id.size == 16 && packet.body.binding_accepted.binding_generation != 0;
    case mii_protocol_v1_AuthenticationPacket_session_hello_tag:
        return packet.body.session_hello.owner_id.size == 16 && packet.body.session_hello.client_nonce.size == 32;
    case mii_protocol_v1_AuthenticationPacket_session_challenge_tag:
        return packet.body.session_challenge.device_nonce.size == 32 && packet.body.session_challenge.device_proof.size == 32
            && packet.body.session_challenge.binding_generation != 0;
    case mii_protocol_v1_AuthenticationPacket_session_proof_tag:
        return packet.body.session_proof.client_proof.size == 32;
    case mii_protocol_v1_AuthenticationPacket_session_ready_tag:
        return packet.body.session_ready.session_id.size == 16 && packet.body.session_ready.binding_generation != 0;
    case mii_protocol_v1_AuthenticationPacket_authenticated_envelope_tag:
        return packet.body.authenticated_envelope.session_id.size == 16 && packet.body.authenticated_envelope.authentication_tag.size == 32
            && packet.body.authenticated_envelope.canonical_envelope.size > 0
            && packet.body.authenticated_envelope.canonical_envelope.size <= kMaxAuthenticatedEnvelopeSize;
    case mii_protocol_v1_AuthenticationPacket_failure_tag:
        return packet.body.failure.code >= mii_protocol_v1_AuthenticationFailureCode_AUTHENTICATION_FAILURE_REJECTED
            && packet.body.failure.code <= mii_protocol_v1_AuthenticationFailureCode_AUTHENTICATION_FAILURE_STORAGE_UNAVAILABLE;
    default: return false;
    }
}
}  // namespace

WireResult encode_authentication_packet(const mii_protocol_v1_AuthenticationPacket& packet,
    std::uint8_t* output, std::size_t capacity) noexcept {
    if (output == nullptr || capacity == 0) { return {}; }
    if (!valid(packet)) { return {WireError::kInvalidFields, 0}; }
    std::size_t size = 0;
    if (!pb_get_encoded_size(&size, &mii_protocol_v1_AuthenticationPacket_msg, &packet)) {
        return {WireError::kInvalidFields, 0};
    }
    if (size > capacity) { return {WireError::kOutputTooSmall, 0}; }
    auto stream = pb_ostream_from_buffer(output, capacity);
    if (!pb_encode(&stream, &mii_protocol_v1_AuthenticationPacket_msg, &packet)) {
        return {WireError::kInvalidFields, 0};
    }
    return {WireError::kOk, stream.bytes_written};
}

WireError decode_authentication_packet(const std::uint8_t* input, std::size_t size,
    mii_protocol_v1_AuthenticationPacket& output) noexcept {
    wipe(&output, sizeof(output));
    if (input == nullptr || size == 0 || size > kMaxAuthenticationPacketSize) { return WireError::kInvalidArgument; }
    auto stream = pb_istream_from_buffer(input, size);
    if (!pb_decode(&stream, &mii_protocol_v1_AuthenticationPacket_msg, &output)) {
        wipe(&output, sizeof(output)); return WireError::kDecodeFailed;
    }
    if (!valid(output)) { wipe(&output, sizeof(output)); return WireError::kInvalidFields; }
    CanonicalComparison comparison{input, size, 0, true};
    pb_ostream_t canonical{};
    canonical.callback = compare_encoded_bytes;
    canonical.state = &comparison;
    canonical.max_size = size;
    const bool matches = pb_encode(&canonical, &mii_protocol_v1_AuthenticationPacket_msg, &output)
        && canonical.bytes_written == size && comparison.matches;
    if (!matches) { wipe(&output, sizeof(output)); return WireError::kNonCanonical; }
    return WireError::kOk;
}

AuthStatusResult AuthWireGateway::authorize_status(OwnerSession& session,
    std::uint64_t connection, const std::uint8_t* bytes,
    std::size_t size, std::uint64_t now, mii::protocol::DecodedDeviceStatusRequest& output) noexcept {
    output = {};
    // The expected identity belongs to the same immutable device/session
    // transcript. It cannot be supplied by a transport caller or packet field.
    std::array<char, 33> expected_device_id{};
    constexpr char kHex[] = "0123456789abcdef";
    for (std::size_t i = 0; i < session.device_id_.size(); ++i) {
        expected_device_id[i * 2] = kHex[session.device_id_[i] >> 4];
        expected_device_id[i * 2 + 1] = kHex[session.device_id_[i] & 15];
    }
    mii_protocol_v1_AuthenticationPacket packet = mii_protocol_v1_AuthenticationPacket_init_zero;
    const WireError decoded = decode_authentication_packet(bytes, size, packet);
    if (decoded != WireError::kOk) { return {decoded, {}}; }
    if (packet.which_body != mii_protocol_v1_AuthenticationPacket_authenticated_envelope_tag) {
        wipe(&packet, sizeof(packet)); return {WireError::kWrongBody, {}};
    }
    const auto& p = packet.body.authenticated_envelope;
    Id id{}; Secret tag{};
    std::memcpy(id.data(), p.session_id.bytes, id.size());
    std::memcpy(tag.data(), p.authentication_tag.bytes, tag.size());
    const Error verified = session.verify_control_mac(connection, id, p.canonical_envelope.bytes,
                                                       p.canonical_envelope.size, tag, now);
    wipe(tag.data(), tag.size());
    if (verified != Error::kOk) {
        wipe(&packet, sizeof(packet)); return {WireError::kUnauthorized, {verified, {}}};
    }
    mii::protocol::DecodedDeviceStatusRequest command{};
    const auto result = mii::protocol::decode_device_status_request(p.canonical_envelope.bytes,
                                                                   p.canonical_envelope.size, &command);
    wipe(&packet, sizeof(packet));
    if (result != mii::protocol::CodecError::kOk || command.message_id == 0 || command.sequence_number == 0) {
        return {WireError::kInvalidEnvelope, {}};
    }
    if (std::memcmp(command.device_id.data(), expected_device_id.data(), expected_device_id.size()) != 0) {
        return {WireError::kWrongDevice, {}};
    }
    const auto authorization = session.observe_authorized_numbers(command.message_id, command.sequence_number);
    if (authorization.error != Error::kOk) { return {WireError::kUnauthorized, authorization}; }
    output = command;
    return {WireError::kOk, authorization};
}
}  // namespace mii::auth
