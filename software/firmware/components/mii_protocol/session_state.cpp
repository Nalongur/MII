#include "mii/session_state.hpp"

#include <algorithm>
#include <limits>

namespace mii::protocol {

bool OutboundSessionCounters::begin(
    std::uint64_t first_message_id,
    std::uint64_t first_sequence_number) noexcept {
    if (first_message_id == 0 || first_sequence_number == 0) {
        reset();
        return false;
    }

    next_message_id_ = first_message_id;
    next_sequence_number_ = first_sequence_number;
    started_ = true;
    message_ids_exhausted_ = false;
    sequence_numbers_exhausted_ = false;
    return true;
}

void OutboundSessionCounters::reset() noexcept {
    next_message_id_ = 0;
    next_sequence_number_ = 0;
    started_ = false;
    message_ids_exhausted_ = false;
    sequence_numbers_exhausted_ = false;
}

CounterResult OutboundSessionCounters::allocate_message() noexcept {
    if (!started_) {
        return {CounterError::kNotStarted, {}};
    }
    if (message_ids_exhausted_ || sequence_numbers_exhausted_) {
        return {CounterError::kExhausted, {}};
    }

    const MessageNumbers allocated{
        next_message_id_,
        next_sequence_number_,
    };
    constexpr std::uint64_t kMaximum =
        std::numeric_limits<std::uint64_t>::max();
    if (next_message_id_ == kMaximum) {
        message_ids_exhausted_ = true;
    } else {
        ++next_message_id_;
    }
    if (next_sequence_number_ == kMaximum) {
        sequence_numbers_exhausted_ = true;
    } else {
        ++next_sequence_number_;
    }
    return {CounterError::kOk, allocated};
}

CounterResult OutboundSessionCounters::allocate_retry(
    std::uint64_t message_id) noexcept {
    if (!started_) {
        return {CounterError::kNotStarted, {}};
    }
    if (message_id == 0) {
        return {CounterError::kInvalidMessageId, {}};
    }
    if (sequence_numbers_exhausted_) {
        return {CounterError::kExhausted, {}};
    }

    const MessageNumbers allocated{message_id, next_sequence_number_};
    constexpr std::uint64_t kMaximum =
        std::numeric_limits<std::uint64_t>::max();
    if (next_sequence_number_ == kMaximum) {
        sequence_numbers_exhausted_ = true;
    } else {
        ++next_sequence_number_;
    }
    return {CounterError::kOk, allocated};
}

bool OutboundSessionCounters::started() const noexcept {
    return started_;
}

void InboundSessionTracker::reset() noexcept {
    highest_sequence_number_ = 0;
    seen_sequence_mask_ = 0;
    recent_message_ids_.fill(0);
    recent_message_count_ = 0;
    next_message_slot_ = 0;
}

InboundDecision InboundSessionTracker::observe(
    std::uint64_t message_id,
    std::uint64_t sequence_number) noexcept {
    if (message_id == 0 || sequence_number == 0) {
        return InboundDecision::kInvalidArgument;
    }

    InboundDecision accepted_decision = InboundDecision::kAcceptNew;
    if (highest_sequence_number_ == 0) {
        highest_sequence_number_ = sequence_number;
        seen_sequence_mask_ = 1;
    } else if (sequence_number > highest_sequence_number_) {
        const std::uint64_t distance =
            sequence_number - highest_sequence_number_;
        if (distance >= kReplayWindowSize) {
            seen_sequence_mask_ = 1;
        } else {
            seen_sequence_mask_ =
                (seen_sequence_mask_ << static_cast<unsigned int>(distance)) | 1;
        }
        highest_sequence_number_ = sequence_number;
    } else {
        const std::uint64_t distance =
            highest_sequence_number_ - sequence_number;
        if (distance >= kReplayWindowSize) {
            return InboundDecision::kTooOld;
        }
        const std::uint64_t sequence_bit =
            std::uint64_t{1} << static_cast<unsigned int>(distance);
        if ((seen_sequence_mask_ & sequence_bit) != 0) {
            return has_message_id(message_id)
                       ? InboundDecision::kDuplicateMessage
                       : InboundDecision::kDuplicateSequence;
        }
        seen_sequence_mask_ |= sequence_bit;
        accepted_decision = InboundDecision::kAcceptReordered;
    }

    if (has_message_id(message_id)) {
        return InboundDecision::kDuplicateMessage;
    }
    remember_message_id(message_id);
    return accepted_decision;
}

std::uint64_t InboundSessionTracker::highest_sequence_number() const noexcept {
    return highest_sequence_number_;
}

std::size_t InboundSessionTracker::recent_message_count() const noexcept {
    return recent_message_count_;
}

bool InboundSessionTracker::has_message_id(std::uint64_t message_id) const noexcept {
    const auto active_end =
        recent_message_ids_.begin() +
        static_cast<std::ptrdiff_t>(recent_message_count_);
    return std::find(recent_message_ids_.begin(), active_end, message_id) !=
           active_end;
}

void InboundSessionTracker::remember_message_id(std::uint64_t message_id) noexcept {
    recent_message_ids_[next_message_slot_] = message_id;
    next_message_slot_ =
        (next_message_slot_ + 1) % recent_message_ids_.size();
    if (recent_message_count_ < recent_message_ids_.size()) {
        ++recent_message_count_;
    }
}

}  // namespace mii::protocol
