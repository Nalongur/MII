#include "mii/frame_ingress.hpp"

#include <limits>

namespace mii::protocol {
namespace {

void increment_saturated(std::uint32_t* value) noexcept {
    if (*value != std::numeric_limits<std::uint32_t>::max()) {
        ++(*value);
    }
}

void add_saturated(std::uint32_t* value, std::size_t amount) noexcept {
    const std::uint32_t maximum = std::numeric_limits<std::uint32_t>::max();
    if (amount >= maximum || *value > maximum - static_cast<std::uint32_t>(amount)) {
        *value = maximum;
        return;
    }
    *value += static_cast<std::uint32_t>(amount);
}

}  // namespace

IngressResult FrameIngress::consume(
    const std::uint8_t* input,
    std::size_t input_size,
    std::uint32_t monotonic_ms,
    FrameHandler handler,
    void* context) noexcept {
    if ((input_size != 0 && input == nullptr) || handler == nullptr) {
        return {IngressError::kInvalidArgument, 0, 0, 0};
    }

    static_cast<void>(poll_timeout(monotonic_ms));

    IngressResult ingress_result{};
    while (ingress_result.bytes_consumed < input_size) {
        const std::size_t remaining = input_size - ingress_result.bytes_consumed;
        const FrameDecodeResult frame_result = decoder_.push(
            input + ingress_result.bytes_consumed,
            remaining);
        ingress_result.bytes_consumed += frame_result.bytes_consumed;
        add_saturated(&counters_.discarded_bytes, frame_result.discarded_bytes);

        if (frame_result.status == FrameDecodeStatus::kError) {
            ++ingress_result.frame_errors;
            count_frame_error(frame_result.error);
            if (frame_result.bytes_consumed == 0) {
                return ingress_result;
            }
        } else if (frame_result.status == FrameDecodeStatus::kFrameReady) {
            const bool accepted =
                handler(decoder_.frame_data(), decoder_.frame_size(), context);
            static_cast<void>(decoder_.release_frame());
            partial_timer_active_ = false;
            if (!accepted) {
                increment_saturated(&counters_.handler_rejections);
                ingress_result.error = IngressError::kHandlerRejected;
                return ingress_result;
            }
            increment_saturated(&counters_.frames_delivered);
            ++ingress_result.frames_delivered;
        }

        if (decoder_.has_partial_frame()) {
            last_progress_ms_ = monotonic_ms;
            partial_timer_active_ = true;
        } else if (!decoder_.frame_ready()) {
            partial_timer_active_ = false;
        }
    }
    return ingress_result;
}

bool FrameIngress::poll_timeout(std::uint32_t monotonic_ms) noexcept {
    if (!partial_timer_active_ ||
        static_cast<std::uint32_t>(monotonic_ms - last_progress_ms_) <
            kPartialFrameTimeoutMs) {
        return false;
    }
    if (decoder_.expire_partial_frame() != FrameError::kTimeout) {
        partial_timer_active_ = false;
        return false;
    }
    partial_timer_active_ = false;
    increment_saturated(&counters_.partial_timeouts);
    return true;
}

void FrameIngress::reset_session() noexcept {
    decoder_.reset();
    last_progress_ms_ = 0;
    partial_timer_active_ = false;
}

void FrameIngress::clear_counters() noexcept {
    counters_ = {};
}

bool FrameIngress::has_partial_frame() const noexcept {
    return decoder_.has_partial_frame();
}

const IngressCounters& FrameIngress::counters() const noexcept {
    return counters_;
}

void FrameIngress::count_frame_error(FrameError error) noexcept {
    increment_saturated(&counters_.framing_errors);
    switch (error) {
        case FrameError::kHeaderChecksumMismatch:
        case FrameError::kUnsupportedVersion:
        case FrameError::kUnsupportedFlags:
        case FrameError::kInvalidPayloadLength:
            increment_saturated(&counters_.header_errors);
            break;
        case FrameError::kPayloadChecksumMismatch:
            increment_saturated(&counters_.payload_crc_errors);
            break;
        default:
            break;
    }
}

}  // namespace mii::protocol
