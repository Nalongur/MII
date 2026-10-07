#pragma once

#include <cstddef>
#include <cstdint>

#include "mii/frame_ingress.hpp"

namespace mii::ble {

inline constexpr std::size_t kBleRxMaxFragmentSize = 244;
inline constexpr std::uint16_t kInvalidConnectionHandle = 0xffffU;

enum class BleRxError : std::uint8_t {
    kOk = 0,
    kInvalidArgument,
    kDisconnected,
    kStaleConnection,
    kFragmentTooLarge,
    kHandlerRejected,
};

struct BleRxCounters {
    std::uint32_t connections_started{0};
    std::uint32_t connections_replaced{0};
    std::uint32_t disconnects{0};
    std::uint32_t stale_disconnects{0};
    std::uint32_t fragments_accepted{0};
    std::uint32_t fragments_rejected{0};
    std::uint32_t stale_fragments{0};
    std::uint32_t oversize_fragments{0};
};

struct BleRxResult {
    BleRxError error{BleRxError::kOk};
    mii::protocol::IngressResult ingress{};

    [[nodiscard]] constexpr bool ok() const noexcept {
        return error == BleRxError::kOk;
    }
};

// Protocol-task-owned connection gate around FrameIngress. BLE callbacks must
// copy writes into a bounded queue; they must never call this object directly.
class BleRxSession final {
public:
    BleRxSession() = default;
    BleRxSession(const BleRxSession&) = delete;
    BleRxSession& operator=(const BleRxSession&) = delete;
    BleRxSession(BleRxSession&&) = delete;
    BleRxSession& operator=(BleRxSession&&) = delete;

    void on_connected(std::uint16_t connection_handle) noexcept;
    void on_disconnected(std::uint16_t connection_handle) noexcept;

    [[nodiscard]] BleRxResult consume_fragment(
        std::uint16_t connection_handle,
        const std::uint8_t* bytes,
        std::size_t size,
        std::uint32_t monotonic_ms,
        mii::protocol::FrameHandler handler,
        void* context) noexcept;

    [[nodiscard]] bool poll_timeout(std::uint32_t monotonic_ms) noexcept;
    void clear_counters() noexcept;

    [[nodiscard]] bool connected() const noexcept;
    [[nodiscard]] std::uint16_t connection_handle() const noexcept;
    [[nodiscard]] const BleRxCounters& counters() const noexcept;
    [[nodiscard]] const mii::protocol::IngressCounters& ingress_counters()
        const noexcept;

private:
    std::uint16_t connection_handle_{kInvalidConnectionHandle};
    mii::protocol::FrameIngress ingress_{};
    BleRxCounters counters_{};
};

}  // namespace mii::ble
