#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "mii/frame_codec.hpp"
#include "mii/ble_rx_session.hpp"

namespace mii::ble {

inline constexpr std::uint16_t kBleAttMinimumMtu = 23;
inline constexpr std::uint16_t kBleAttPreferredMtu = 247;
inline constexpr std::size_t kBleTxQueueCapacity = 4;
inline constexpr std::uint8_t kBleTxRetryLimit = 3;

enum class BleTxError : std::uint8_t {
    kOk = 0,
    kInvalidArgument,
    kDisconnected,
    kStaleConnection,
    kNotSubscribed,
    kInvalidMtu,
    kFrameTooLarge,
    kQueueFull,
    kEmpty,
    kNoFragmentOffered,
    kRetryExhausted,
};

struct BleTxFragment {
    const std::uint8_t* bytes{nullptr};
    std::size_t size{0};
    bool completes_frame{false};
};

struct BleTxCounters {
    std::uint32_t connections_started{0};
    std::uint32_t connections_replaced{0};
    std::uint32_t disconnects{0};
    std::uint32_t stale_operations{0};
    std::uint32_t frames_queued{0};
    std::uint32_t frames_sent{0};
    std::uint32_t fragments_sent{0};
    std::uint32_t retryable_failures{0};
    std::uint32_t frames_flushed{0};
    std::uint32_t queue_full_rejections{0};
};

// NimBLE-host-context-owned queue and MTU fragmenter. A producer must cross a
// separate bounded synchronization primitive before calling enqueue_frame().
class BleTxSession final {
public:
    BleTxSession() = default;
    BleTxSession(const BleTxSession&) = delete;
    BleTxSession& operator=(const BleTxSession&) = delete;
    BleTxSession(BleTxSession&&) = delete;
    BleTxSession& operator=(BleTxSession&&) = delete;

    void on_connected(std::uint16_t connection_handle) noexcept;
    void on_disconnected(std::uint16_t connection_handle) noexcept;
    [[nodiscard]] BleTxError set_subscribed(
        std::uint16_t connection_handle,
        bool subscribed) noexcept;
    [[nodiscard]] BleTxError set_mtu(
        std::uint16_t connection_handle,
        std::uint16_t att_mtu) noexcept;

    [[nodiscard]] BleTxError enqueue_frame(
        std::uint16_t connection_handle,
        const std::uint8_t* frame,
        std::size_t frame_size) noexcept;
    [[nodiscard]] BleTxError peek_fragment(
        std::uint16_t connection_handle,
        BleTxFragment* fragment) noexcept;
    [[nodiscard]] BleTxError mark_fragment_sent(
        std::uint16_t connection_handle) noexcept;
    [[nodiscard]] BleTxError record_retryable_failure(
        std::uint16_t connection_handle) noexcept;
    [[nodiscard]] BleTxError drop_current_frame(
        std::uint16_t connection_handle) noexcept;

    void clear_counters() noexcept;
    [[nodiscard]] bool connected() const noexcept;
    [[nodiscard]] bool subscribed() const noexcept;
    [[nodiscard]] std::uint16_t connection_handle() const noexcept;
    [[nodiscard]] std::uint16_t att_mtu() const noexcept;
    [[nodiscard]] std::size_t pending_frames() const noexcept;
    [[nodiscard]] const BleTxCounters& counters() const noexcept;

private:
    struct FrameSlot {
        std::array<std::uint8_t, mii::protocol::kMaxEncodedFrameSize> bytes{};
        std::size_t size{0};
        std::size_t offset{0};
        std::uint8_t retry_count{0};
    };

    [[nodiscard]] BleTxError validate_connection(
        std::uint16_t connection_handle) noexcept;
    void flush_frames() noexcept;
    void pop_current(bool sent) noexcept;

    std::array<FrameSlot, kBleTxQueueCapacity> frames_{};
    std::size_t head_{0};
    std::size_t count_{0};
    std::size_t offered_size_{0};
    std::uint16_t connection_handle_{kInvalidConnectionHandle};
    std::uint16_t att_mtu_{kBleAttMinimumMtu};
    bool subscribed_{false};
    BleTxCounters counters_{};
};

}  // namespace mii::ble
