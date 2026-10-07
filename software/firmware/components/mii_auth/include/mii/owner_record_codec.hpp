#pragma once

#include "mii/owner_session.hpp"

namespace mii::auth {
inline constexpr std::size_t kOwnerRecordBlobSize = 132;
using OwnerRecordBlob = std::array<std::uint8_t, kOwnerRecordBlobSize>;
// Canonical fixed little-endian record, not compiler struct memory/padding.
// CRC detects interrupted/corrupt data, not malicious modification.
[[nodiscard]] bool encode_owner_record(const OwnerRecord&, OwnerRecordBlob&) noexcept;
[[nodiscard]] bool decode_owner_record(const std::uint8_t*, std::size_t,
                                       OwnerRecord&) noexcept;
}  // namespace mii::auth
