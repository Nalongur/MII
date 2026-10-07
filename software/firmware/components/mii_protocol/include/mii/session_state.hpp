#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace mii::protocol {

inline constexpr std::size_t kReplayWindowSize = 64;
inline constexpr std::size_t kRecentMessageIdCapacity = 64;

struct MessageNumbers {
    std::uint64_t message_id = 0;
    std::uint64_t sequence_number = 0;
};

enum class CounterError : std::uint8_t {
    kOk = 0,
    kNotStarted,
    kExhausted,
    kInvalidMessageId,
};

struct CounterResult {
    CounterError error = CounterError::kNotStarted;
    MessageNumbers numbers{};

    [[nodiscard]] constexpr bool ok() const noexcept {
        return error == CounterError::kOk;
    }
};

// Allocates non-zero identifiers for one authenticated sender session.
// Keep the returned pair with the logical request and reuse it for an exact
// transport retry. allocate_retry() preserves message_id while assigning a new
// sequence number. Start a fresh instance or call begin() for a new session.
class OutboundSessionCounters {
public:
    [[nodiscard]] bool begin(
        std::uint64_t first_message_id = 1,
        std::uint64_t first_sequence_number = 1) noexcept;
    void reset() noexcept;

    [[nodiscard]] CounterResult allocate_message() noexcept;
    [[nodiscard]] CounterResult allocate_retry(
        std::uint64_t message_id) noexcept;
    [[nodiscard]] bool started() const noexcept;

private:
    std::uint64_t next_message_id_ = 0;
    std::uint64_t next_sequence_number_ = 0;
    bool started_ = false;
    bool message_ids_exhausted_ = false;
    bool sequence_numbers_exhausted_ = false;
};

enum class InboundDecision : std::uint8_t {
    kAcceptNew = 0,
    kAcceptReordered,
    kDuplicateMessage,
    kDuplicateSequence,
    kTooOld,
    kInvalidArgument,
};

[[nodiscard]] constexpr bool should_process(InboundDecision decision) noexcept {
    return decision == InboundDecision::kAcceptNew ||
           decision == InboundDecision::kAcceptReordered;
}

// Tracks one authenticated receiver session with fixed memory. The 64-bit
// bitmap accepts bounded reordering while rejecting duplicate or stale
// sequence numbers. A second fixed ring recognizes logical-request retries
// that reuse message_id with a newer transport sequence. This class is not
// thread-safe; one transport task must own it or callers must serialize access.
class InboundSessionTracker {
public:
    void reset() noexcept;

    [[nodiscard]] InboundDecision observe(
        std::uint64_t message_id,
        std::uint64_t sequence_number) noexcept;

    [[nodiscard]] std::uint64_t highest_sequence_number() const noexcept;
    [[nodiscard]] std::size_t recent_message_count() const noexcept;

private:
    [[nodiscard]] bool has_message_id(std::uint64_t message_id) const noexcept;
    void remember_message_id(std::uint64_t message_id) noexcept;

    std::uint64_t highest_sequence_number_ = 0;
    std::uint64_t seen_sequence_mask_ = 0;
    std::array<std::uint64_t, kRecentMessageIdCapacity> recent_message_ids_{};
    std::size_t recent_message_count_ = 0;
    std::size_t next_message_slot_ = 0;
};

}  // namespace mii::protocol
