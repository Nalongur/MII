#include "mii/ble_tx_session.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace mii::ble {
namespace {

void increment_saturated(std::uint32_t* value) noexcept {
    if (*value != std::numeric_limits<std::uint32_t>::max()) {
        ++(*value);
    }
}

void add_saturated(std::uint32_t* value, std::size_t amount) noexcept {
    const auto maximum = std::numeric_limits<std::uint32_t>::max();
    if (amount >= maximum ||
        *value > maximum - static_cast<std::uint32_t>(amount)) {
        *value = maximum;
        return;
    }
    *value += static_cast<std::uint32_t>(amount);
}

}  // namespace

void BleTxSession::on_connected(std::uint16_t connection_handle) noexcept {
    if (connection_handle == kInvalidConnectionHandle) {
        return;
    }
    if (connected()) {
        increment_saturated(&counters_.connections_replaced);
    }
    flush_frames();
    connection_handle_ = connection_handle;
    att_mtu_ = kBleAttMinimumMtu;
    subscribed_ = false;
    increment_saturated(&counters_.connections_started);
}

void BleTxSession::on_disconnected(std::uint16_t connection_handle) noexcept {
    if (validate_connection(connection_handle) != BleTxError::kOk) {
        return;
    }
    flush_frames();
    connection_handle_ = kInvalidConnectionHandle;
    att_mtu_ = kBleAttMinimumMtu;
    subscribed_ = false;
    increment_saturated(&counters_.disconnects);
}

BleTxError BleTxSession::set_subscribed(
    std::uint16_t connection_handle,
    bool subscribed) noexcept {
    const BleTxError connection = validate_connection(connection_handle);
    if (connection != BleTxError::kOk) {
        return connection;
    }
    if (subscribed_ && !subscribed) {
        flush_frames();
    }
    subscribed_ = subscribed;
    return BleTxError::kOk;
}

BleTxError BleTxSession::set_mtu(
    std::uint16_t connection_handle,
    std::uint16_t att_mtu) noexcept {
    const BleTxError connection = validate_connection(connection_handle);
    if (connection != BleTxError::kOk) {
        return connection;
    }
    if (att_mtu < kBleAttMinimumMtu || att_mtu > kBleAttPreferredMtu) {
        return BleTxError::kInvalidMtu;
    }
    att_mtu_ = att_mtu;
    offered_size_ = 0;
    return BleTxError::kOk;
}

BleTxError BleTxSession::enqueue_frame(
    std::uint16_t connection_handle,
    const std::uint8_t* frame,
    std::size_t frame_size) noexcept {
    if (frame == nullptr || frame_size == 0) {
        return BleTxError::kInvalidArgument;
    }
    const BleTxError connection = validate_connection(connection_handle);
    if (connection != BleTxError::kOk) {
        return connection;
    }
    if (!subscribed_) {
        return BleTxError::kNotSubscribed;
    }
    if (frame_size > mii::protocol::kMaxEncodedFrameSize) {
        return BleTxError::kFrameTooLarge;
    }
    if (count_ == frames_.size()) {
        increment_saturated(&counters_.queue_full_rejections);
        return BleTxError::kQueueFull;
    }

    FrameSlot& slot = frames_[(head_ + count_) % frames_.size()];
    std::memcpy(slot.bytes.data(), frame, frame_size);
    slot.size = frame_size;
    slot.offset = 0;
    slot.retry_count = 0;
    ++count_;
    increment_saturated(&counters_.frames_queued);
    return BleTxError::kOk;
}

BleTxError BleTxSession::peek_fragment(
    std::uint16_t connection_handle,
    BleTxFragment* fragment) noexcept {
    if (fragment == nullptr) {
        return BleTxError::kInvalidArgument;
    }
    *fragment = {};
    const BleTxError connection = validate_connection(connection_handle);
    if (connection != BleTxError::kOk) {
        return connection;
    }
    if (!subscribed_) {
        return BleTxError::kNotSubscribed;
    }
    if (count_ == 0) {
        return BleTxError::kEmpty;
    }

    FrameSlot& slot = frames_[head_];
    const std::size_t remaining = slot.size - slot.offset;
    const std::size_t fragment_capacity = att_mtu_ - 3U;
    offered_size_ = std::min(remaining, fragment_capacity);
    fragment->bytes = slot.bytes.data() + slot.offset;
    fragment->size = offered_size_;
    fragment->completes_frame = offered_size_ == remaining;
    return BleTxError::kOk;
}

BleTxError BleTxSession::mark_fragment_sent(
    std::uint16_t connection_handle) noexcept {
    const BleTxError connection = validate_connection(connection_handle);
    if (connection != BleTxError::kOk) {
        return connection;
    }
    if (count_ == 0 || offered_size_ == 0) {
        return BleTxError::kNoFragmentOffered;
    }

    FrameSlot& slot = frames_[head_];
    slot.offset += offered_size_;
    slot.retry_count = 0;
    offered_size_ = 0;
    increment_saturated(&counters_.fragments_sent);
    if (slot.offset == slot.size) {
        pop_current(true);
    }
    return BleTxError::kOk;
}

BleTxError BleTxSession::record_retryable_failure(
    std::uint16_t connection_handle) noexcept {
    const BleTxError connection = validate_connection(connection_handle);
    if (connection != BleTxError::kOk) {
        return connection;
    }
    if (count_ == 0 || offered_size_ == 0) {
        return BleTxError::kNoFragmentOffered;
    }

    FrameSlot& slot = frames_[head_];
    increment_saturated(&counters_.retryable_failures);
    ++slot.retry_count;
    if (slot.retry_count >= kBleTxRetryLimit) {
        pop_current(false);
        return BleTxError::kRetryExhausted;
    }
    return BleTxError::kOk;
}

BleTxError BleTxSession::drop_current_frame(
    std::uint16_t connection_handle) noexcept {
    const BleTxError connection = validate_connection(connection_handle);
    if (connection != BleTxError::kOk) {
        return connection;
    }
    if (count_ == 0) {
        return BleTxError::kEmpty;
    }
    pop_current(false);
    return BleTxError::kOk;
}

void BleTxSession::clear_counters() noexcept {
    counters_ = {};
}

bool BleTxSession::connected() const noexcept {
    return connection_handle_ != kInvalidConnectionHandle;
}

bool BleTxSession::subscribed() const noexcept {
    return subscribed_;
}

std::uint16_t BleTxSession::connection_handle() const noexcept {
    return connection_handle_;
}

std::uint16_t BleTxSession::att_mtu() const noexcept {
    return att_mtu_;
}

std::size_t BleTxSession::pending_frames() const noexcept {
    return count_;
}

const BleTxCounters& BleTxSession::counters() const noexcept {
    return counters_;
}

BleTxError BleTxSession::validate_connection(
    std::uint16_t connection_handle) noexcept {
    if (!connected()) {
        increment_saturated(&counters_.stale_operations);
        return BleTxError::kDisconnected;
    }
    if (connection_handle != connection_handle_) {
        increment_saturated(&counters_.stale_operations);
        return BleTxError::kStaleConnection;
    }
    return BleTxError::kOk;
}

void BleTxSession::flush_frames() noexcept {
    add_saturated(&counters_.frames_flushed, count_);
    for (FrameSlot& slot : frames_) {
        slot.size = 0;
        slot.offset = 0;
        slot.retry_count = 0;
    }
    head_ = 0;
    count_ = 0;
    offered_size_ = 0;
}

void BleTxSession::pop_current(bool sent) noexcept {
    FrameSlot& slot = frames_[head_];
    slot.size = 0;
    slot.offset = 0;
    slot.retry_count = 0;
    offered_size_ = 0;
    head_ = (head_ + 1) % frames_.size();
    --count_;
    if (sent) {
        increment_saturated(&counters_.frames_sent);
    } else {
        increment_saturated(&counters_.frames_flushed);
    }
}

}  // namespace mii::ble
