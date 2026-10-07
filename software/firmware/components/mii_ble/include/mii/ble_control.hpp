#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

namespace mii::ble {

// Starts the dormant-by-default NimBLE control service. The current baseline
// requires link encryption but does not yet provide authenticated pairing.
[[nodiscard]] esp_err_t start_control_service() noexcept;

// Frames one protobuf Envelope payload and submits it to the bounded TX
// ingress. The call is nonblocking and fails when disconnected, unsubscribed,
// stale, or congested. It never executes a NimBLE API in the caller context.
[[nodiscard]] esp_err_t queue_control_payload(
    const std::uint8_t* payload,
    std::size_t payload_size) noexcept;

}  // namespace mii::ble
