#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "mii/protocol_codec.hpp"

namespace mii::protocol {

inline constexpr std::uint8_t kFrameMagicFirst = 0x4d;
inline constexpr std::uint8_t kFrameMagicSecond = 0x49;
inline constexpr std::uint8_t kFrameVersion = 1;
inline constexpr std::uint8_t kFrameFlagsNone = 0;
inline constexpr std::size_t kFrameHeaderSize = 12;
inline constexpr std::size_t kMaxFramePayloadSize = kMaxEncodedEnvelopeSize;
inline constexpr std::size_t kMaxEncodedFrameSize =
    kFrameHeaderSize + kMaxFramePayloadSize;

enum class FrameError : std::uint8_t {
    kOk = 0,
    kInvalidArgument,
    kPayloadTooLarge,
    kOutputTooSmall,
    kHeaderChecksumMismatch,
    kUnsupportedVersion,
    kUnsupportedFlags,
    kInvalidPayloadLength,
    kPayloadChecksumMismatch,
    kFramePending,
    kTimeout,
};

struct FrameEncodeResult {
    FrameError error{FrameError::kInvalidArgument};
    std::size_t bytes_written{0};

    [[nodiscard]] constexpr bool ok() const noexcept {
        return error == FrameError::kOk;
    }
};

// Wraps one protobuf Envelope in the transport-independent v1 frame. The
// payload may overlap output; output must still have room for the full frame.
[[nodiscard]] FrameEncodeResult encode_frame(
    const std::uint8_t* payload,
    std::size_t payload_size,
    std::uint8_t* output,
    std::size_t output_capacity) noexcept;

enum class FrameDecodeStatus : std::uint8_t {
    kNeedMore = 0,
    kFrameReady,
    kError,
};

struct FrameDecodeResult {
    FrameDecodeStatus status{FrameDecodeStatus::kNeedMore};
    FrameError error{FrameError::kOk};
    std::size_t bytes_consumed{0};
    std::size_t discarded_bytes{0};
    std::size_t frame_size{0};
};

// Allocation-free streaming decoder. One transport task must own an instance,
// or callers must serialize access. A ready frame remains valid until release.
class FrameStreamDecoder final {
public:
    [[nodiscard]] FrameDecodeResult push(
        const std::uint8_t* input,
        std::size_t input_size) noexcept;

    [[nodiscard]] bool frame_ready() const noexcept;
    [[nodiscard]] const std::uint8_t* frame_data() const noexcept;
    [[nodiscard]] std::size_t frame_size() const noexcept;
    [[nodiscard]] bool has_partial_frame() const noexcept;

    [[nodiscard]] FrameError release_frame() noexcept;
    [[nodiscard]] FrameError expire_partial_frame() noexcept;
    void reset() noexcept;

private:
    enum class State : std::uint8_t {
        kSeekMagicFirst = 0,
        kSeekMagicSecond,
        kReadHeader,
        kReadPayload,
        kFrameReady,
    };

    void clear_storage() noexcept;
    std::size_t recover_header_suffix() noexcept;

    State state_{State::kSeekMagicFirst};
    std::array<std::uint8_t, kFrameHeaderSize> header_{};
    std::array<std::uint8_t, kMaxFramePayloadSize> payload_{};
    std::size_t header_size_{0};
    std::size_t expected_payload_size_{0};
    std::size_t payload_size_{0};
    std::uint32_t expected_payload_crc_{0};
    std::uint32_t running_payload_crc_{0xffffffffU};
};

}  // namespace mii::protocol
