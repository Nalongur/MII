#include "mii/frame_codec.hpp"

#include <algorithm>
#include <cstring>

namespace mii::protocol {
namespace {

constexpr std::uint16_t kCrc16Polynomial = 0x1021U;
constexpr std::uint32_t kCrc32Polynomial = 0xedb88320U;

std::uint16_t crc16_ccitt_false(
    const std::uint8_t* data,
    std::size_t size) noexcept {
    std::uint16_t crc = 0xffffU;
    for (std::size_t index = 0; index < size; ++index) {
        crc ^= static_cast<std::uint16_t>(data[index]) << 8U;
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000U) != 0U
                      ? static_cast<std::uint16_t>((crc << 1U) ^ kCrc16Polynomial)
                      : static_cast<std::uint16_t>(crc << 1U);
        }
    }
    return crc;
}

std::uint32_t crc32_update(
    std::uint32_t crc,
    const std::uint8_t* data,
    std::size_t size) noexcept {
    for (std::size_t index = 0; index < size; ++index) {
        crc ^= data[index];
        for (unsigned bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (kCrc32Polynomial & mask);
        }
    }
    return crc;
}

std::uint16_t read_u16_le(const std::uint8_t* data) noexcept {
    return static_cast<std::uint16_t>(data[0]) |
           static_cast<std::uint16_t>(data[1]) << 8U;
}

std::uint32_t read_u32_le(const std::uint8_t* data) noexcept {
    return static_cast<std::uint32_t>(data[0]) |
           static_cast<std::uint32_t>(data[1]) << 8U |
           static_cast<std::uint32_t>(data[2]) << 16U |
           static_cast<std::uint32_t>(data[3]) << 24U;
}

void write_u16_le(std::uint8_t* data, std::uint16_t value) noexcept {
    data[0] = static_cast<std::uint8_t>(value & 0xffU);
    data[1] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
}

void write_u32_le(std::uint8_t* data, std::uint32_t value) noexcept {
    data[0] = static_cast<std::uint8_t>(value & 0xffU);
    data[1] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
    data[2] = static_cast<std::uint8_t>((value >> 16U) & 0xffU);
    data[3] = static_cast<std::uint8_t>((value >> 24U) & 0xffU);
}

}  // namespace

FrameEncodeResult encode_frame(
    const std::uint8_t* payload,
    std::size_t payload_size,
    std::uint8_t* output,
    std::size_t output_capacity) noexcept {
    if (payload == nullptr || output == nullptr || payload_size == 0) {
        return {FrameError::kInvalidArgument, 0};
    }
    if (payload_size > kMaxFramePayloadSize) {
        return {FrameError::kPayloadTooLarge, 0};
    }
    const std::size_t frame_size = kFrameHeaderSize + payload_size;
    if (output_capacity < frame_size) {
        return {FrameError::kOutputTooSmall, 0};
    }

    const std::uint32_t payload_crc =
        crc32_update(0xffffffffU, payload, payload_size) ^ 0xffffffffU;
    std::memmove(output + kFrameHeaderSize, payload, payload_size);
    output[0] = kFrameMagicFirst;
    output[1] = kFrameMagicSecond;
    output[2] = kFrameVersion;
    output[3] = kFrameFlagsNone;
    write_u16_le(output + 4, static_cast<std::uint16_t>(payload_size));
    write_u16_le(output + 6, crc16_ccitt_false(output, 6));
    write_u32_le(output + 8, payload_crc);
    return {FrameError::kOk, frame_size};
}

FrameDecodeResult FrameStreamDecoder::push(
    const std::uint8_t* input,
    std::size_t input_size) noexcept {
    if (state_ == State::kFrameReady) {
        return {FrameDecodeStatus::kError, FrameError::kFramePending, 0, 0,
                payload_size_};
    }
    if (input_size != 0 && input == nullptr) {
        return {FrameDecodeStatus::kError, FrameError::kInvalidArgument, 0, 0, 0};
    }

    std::size_t consumed = 0;
    std::size_t discarded = 0;
    while (consumed < input_size) {
        if (state_ == State::kSeekMagicFirst) {
            if (input[consumed++] == kFrameMagicFirst) {
                header_[0] = kFrameMagicFirst;
                header_size_ = 1;
                state_ = State::kSeekMagicSecond;
            } else {
                ++discarded;
            }
            continue;
        }

        if (state_ == State::kSeekMagicSecond) {
            const std::uint8_t byte = input[consumed++];
            if (byte == kFrameMagicSecond) {
                header_[1] = byte;
                header_size_ = 2;
                state_ = State::kReadHeader;
            } else if (byte == kFrameMagicFirst) {
                ++discarded;
                header_[0] = kFrameMagicFirst;
                header_size_ = 1;
            } else {
                discarded += 2;
                header_size_ = 0;
                state_ = State::kSeekMagicFirst;
            }
            continue;
        }

        if (state_ == State::kReadHeader) {
            const std::size_t copied = std::min(
                kFrameHeaderSize - header_size_, input_size - consumed);
            std::memcpy(header_.data() + header_size_, input + consumed, copied);
            header_size_ += copied;
            consumed += copied;
            if (header_size_ != kFrameHeaderSize) {
                continue;
            }

            FrameError error = FrameError::kOk;
            if (read_u16_le(header_.data() + 6) !=
                crc16_ccitt_false(header_.data(), 6)) {
                error = FrameError::kHeaderChecksumMismatch;
            } else if (header_[2] != kFrameVersion) {
                error = FrameError::kUnsupportedVersion;
            } else if (header_[3] != kFrameFlagsNone) {
                error = FrameError::kUnsupportedFlags;
            } else {
                expected_payload_size_ = read_u16_le(header_.data() + 4);
                if (expected_payload_size_ == 0 ||
                    expected_payload_size_ > kMaxFramePayloadSize) {
                    error = FrameError::kInvalidPayloadLength;
                }
            }
            if (error != FrameError::kOk) {
                discarded += recover_header_suffix();
                return {FrameDecodeStatus::kError, error, consumed, discarded, 0};
            }

            expected_payload_crc_ = read_u32_le(header_.data() + 8);
            payload_size_ = 0;
            running_payload_crc_ = 0xffffffffU;
            state_ = State::kReadPayload;
            continue;
        }

        if (state_ == State::kReadPayload) {
            const std::size_t copied = std::min(
                expected_payload_size_ - payload_size_, input_size - consumed);
            std::memcpy(payload_.data() + payload_size_, input + consumed, copied);
            running_payload_crc_ =
                crc32_update(running_payload_crc_, input + consumed, copied);
            payload_size_ += copied;
            consumed += copied;
            if (payload_size_ != expected_payload_size_) {
                continue;
            }

            const std::uint32_t actual_crc = running_payload_crc_ ^ 0xffffffffU;
            if (actual_crc != expected_payload_crc_) {
                clear_storage();
                return {FrameDecodeStatus::kError,
                        FrameError::kPayloadChecksumMismatch, consumed, discarded, 0};
            }
            state_ = State::kFrameReady;
            return {FrameDecodeStatus::kFrameReady, FrameError::kOk, consumed,
                    discarded, payload_size_};
        }
    }
    return {FrameDecodeStatus::kNeedMore, FrameError::kOk, consumed, discarded, 0};
}

bool FrameStreamDecoder::frame_ready() const noexcept {
    return state_ == State::kFrameReady;
}

const std::uint8_t* FrameStreamDecoder::frame_data() const noexcept {
    return frame_ready() ? payload_.data() : nullptr;
}

std::size_t FrameStreamDecoder::frame_size() const noexcept {
    return frame_ready() ? payload_size_ : 0;
}

bool FrameStreamDecoder::has_partial_frame() const noexcept {
    return state_ != State::kSeekMagicFirst && state_ != State::kFrameReady;
}

FrameError FrameStreamDecoder::release_frame() noexcept {
    if (!frame_ready()) {
        return FrameError::kInvalidArgument;
    }
    clear_storage();
    return FrameError::kOk;
}

FrameError FrameStreamDecoder::expire_partial_frame() noexcept {
    if (frame_ready()) {
        return FrameError::kFramePending;
    }
    if (!has_partial_frame()) {
        return FrameError::kOk;
    }
    clear_storage();
    return FrameError::kTimeout;
}

void FrameStreamDecoder::reset() noexcept {
    clear_storage();
}

void FrameStreamDecoder::clear_storage() noexcept {
    header_.fill(0);
    payload_.fill(0);
    state_ = State::kSeekMagicFirst;
    header_size_ = 0;
    expected_payload_size_ = 0;
    payload_size_ = 0;
    expected_payload_crc_ = 0;
    running_payload_crc_ = 0xffffffffU;
}

std::size_t FrameStreamDecoder::recover_header_suffix() noexcept {
    std::size_t suffix_start = kFrameHeaderSize;
    for (std::size_t index = 1; index + 1 < kFrameHeaderSize; ++index) {
        if (header_[index] == kFrameMagicFirst &&
            header_[index + 1] == kFrameMagicSecond) {
            suffix_start = index;
        }
    }

    if (suffix_start < kFrameHeaderSize) {
        const std::size_t suffix_size = kFrameHeaderSize - suffix_start;
        std::memmove(header_.data(), header_.data() + suffix_start, suffix_size);
        std::fill(header_.begin() + static_cast<std::ptrdiff_t>(suffix_size),
                  header_.end(), 0);
        header_size_ = suffix_size;
        state_ = suffix_size == 1 ? State::kSeekMagicSecond : State::kReadHeader;
        expected_payload_size_ = 0;
        payload_size_ = 0;
        expected_payload_crc_ = 0;
        running_payload_crc_ = 0xffffffffU;
        return suffix_start;
    }

    if (header_.back() == kFrameMagicFirst) {
        clear_storage();
        header_[0] = kFrameMagicFirst;
        header_size_ = 1;
        state_ = State::kSeekMagicSecond;
        return kFrameHeaderSize - 1;
    }

    clear_storage();
    return kFrameHeaderSize;
}

}  // namespace mii::protocol
