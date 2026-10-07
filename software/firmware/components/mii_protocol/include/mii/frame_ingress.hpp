#pragma once

#include <cstddef>
#include <cstdint>

#include "mii/frame_codec.hpp"

namespace mii::protocol {

inline constexpr std::uint32_t kPartialFrameTimeoutMs = 2000;

using FrameHandler = bool (*)(
    const std::uint8_t* payload,
    std::size_t payload_size,
    void* context) noexcept;

enum class IngressError : std::uint8_t {
    kOk = 0,
    kInvalidArgument,
    kHandlerRejected,
};

struct IngressCounters {
    std::uint32_t frames_delivered{0};
    std::uint32_t framing_errors{0};
    std::uint32_t header_errors{0};
    std::uint32_t payload_crc_errors{0};
    std::uint32_t partial_timeouts{0};
    std::uint32_t handler_rejections{0};
    std::uint32_t discarded_bytes{0};
};

struct IngressResult {
    IngressError error{IngressError::kOk};
    std::size_t bytes_consumed{0};
    std::size_t frames_delivered{0};
    std::size_t frame_errors{0};

    [[nodiscard]] constexpr bool ok() const noexcept {
        return error == IngressError::kOk;
    }
};

// Single-owner adapter between a byte-stream transport and FrameStreamDecoder.
// The handler is called synchronously; its payload view is invalid on return.
class FrameIngress final {
public:
    FrameIngress() = default;
    FrameIngress(const FrameIngress&) = delete;
    FrameIngress& operator=(const FrameIngress&) = delete;
    FrameIngress(FrameIngress&&) = delete;
    FrameIngress& operator=(FrameIngress&&) = delete;

    [[nodiscard]] IngressResult consume(
        const std::uint8_t* input,
        std::size_t input_size,
        std::uint32_t monotonic_ms,
        FrameHandler handler,
        void* context) noexcept;

    // Expires an incomplete frame after 2 seconds without input progress.
    // Unsigned subtraction keeps normal uint32_t tick wraparound valid.
    [[nodiscard]] bool poll_timeout(std::uint32_t monotonic_ms) noexcept;

    // Clears decoder/timer state for disconnect or new-session handling while
    // retaining diagnostic counters.
    void reset_session() noexcept;
    void clear_counters() noexcept;

    [[nodiscard]] bool has_partial_frame() const noexcept;
    [[nodiscard]] const IngressCounters& counters() const noexcept;

private:
    void count_frame_error(FrameError error) noexcept;

    FrameStreamDecoder decoder_{};
    IngressCounters counters_{};
    std::uint32_t last_progress_ms_{0};
    bool partial_timer_active_{false};
};

}  // namespace mii::protocol
