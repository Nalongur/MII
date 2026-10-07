#pragma once

#include "mii/owner_session.hpp"
#include "mii/protocol_codec.hpp"
#include "mii.pb.h"

namespace mii::auth {
inline constexpr std::size_t kMaxAuthenticationPacketSize = 604;
inline constexpr std::size_t kMaxAuthenticatedEnvelopeSize = 512;
enum class WireError : std::uint8_t {
    kOk, kInvalidArgument, kDecodeFailed, kInvalidFields, kNonCanonical,
    kOutputTooSmall, kWrongBody, kUnauthorized, kInvalidEnvelope, kWrongDevice,
};
struct WireResult {
    WireError error{WireError::kInvalidArgument};
    std::size_t size{0};
    [[nodiscard]] bool ok() const noexcept { return error == WireError::kOk; }
};
[[nodiscard]] WireResult encode_authentication_packet(
    const mii_protocol_v1_AuthenticationPacket&, std::uint8_t*, std::size_t) noexcept;
[[nodiscard]] WireError decode_authentication_packet(
    const std::uint8_t*, std::size_t, mii_protocol_v1_AuthenticationPacket&) noexcept;
struct AuthStatusResult {
    WireError wire{WireError::kInvalidArgument};
    Authorization authorization{};
};
// This is the only wire-authority gateway. It verifies exact bytes, then decodes
// version/type/device/IDs from those SAME bytes, then updates the replay window.
class AuthWireGateway {
public:
    [[nodiscard]] static AuthStatusResult authorize_status(
        OwnerSession&, std::uint64_t connection_generation,
        const std::uint8_t* packet, std::size_t size, std::uint64_t now_ms,
        mii::protocol::DecodedDeviceStatusRequest&) noexcept;
};
}  // namespace mii::auth
