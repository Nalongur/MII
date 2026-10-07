#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "mii/session_state.hpp"

namespace {

using mii::protocol::CounterError;
using mii::protocol::InboundDecision;
using mii::protocol::InboundSessionTracker;
using mii::protocol::OutboundSessionCounters;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void verify_outbound_counters() {
    OutboundSessionCounters counters;
    require(
        counters.allocate_message().error == CounterError::kNotStarted,
        "unstarted counter allocated identifiers");
    require(
        counters.allocate_retry(1).error == CounterError::kNotStarted,
        "unstarted counter allocated retry identifiers");
    require(!counters.begin(0, 1), "zero message ID was accepted");
    require(!counters.begin(1, 0), "zero sequence number was accepted");
    require(counters.begin(), "default session start failed");

    require(
        counters.allocate_retry(0).error == CounterError::kInvalidMessageId,
        "zero retry message ID was accepted");
    const auto first = counters.allocate_message();
    require(
        first.ok() && first.numbers.message_id == 1 &&
            first.numbers.sequence_number == 1,
        "first allocation mismatch");
    const auto exact_retry = first.numbers;
    require(
        exact_retry.message_id == first.numbers.message_id &&
            exact_retry.sequence_number == first.numbers.sequence_number,
        "exact retry did not preserve identifiers");
    const auto rewrapped_retry = counters.allocate_retry(first.numbers.message_id);
    require(
        rewrapped_retry.ok() && rewrapped_retry.numbers.message_id == 1 &&
            rewrapped_retry.numbers.sequence_number == 2,
        "rewrapped retry did not preserve message ID");
    const auto second = counters.allocate_message();
    require(
        second.ok() && second.numbers.message_id == 2 &&
            second.numbers.sequence_number == 3,
        "second new-message allocation mismatch");

    constexpr std::uint64_t maximum =
        std::numeric_limits<std::uint64_t>::max();
    require(counters.begin(maximum, maximum), "maximum counter start failed");
    const auto final = counters.allocate_message();
    require(
        final.ok() && final.numbers.message_id == maximum &&
            final.numbers.sequence_number == maximum,
        "final counter allocation mismatch");
    require(
        counters.allocate_message().error == CounterError::kExhausted &&
            counters.allocate_retry(maximum).error == CounterError::kExhausted,
        "counter wraparound was not rejected");

    require(counters.begin(maximum, 1), "split exhaustion start failed");
    require(counters.allocate_message().ok(), "maximum message ID was rejected");
    require(
        counters.allocate_retry(maximum).ok(),
        "retry was blocked by exhausted message IDs");
    require(
        counters.allocate_message().error == CounterError::kExhausted,
        "new message allocated after message ID exhaustion");
    counters.reset();
    require(!counters.started(), "counter reset did not end session");
}

void verify_inbound_window() {
    InboundSessionTracker tracker;
    require(
        tracker.observe(0, 1) == InboundDecision::kInvalidArgument,
        "zero message ID was accepted");
    require(
        tracker.observe(1, 0) == InboundDecision::kInvalidArgument,
        "zero sequence number was accepted");
    require(
        tracker.observe(10, 10) == InboundDecision::kAcceptNew,
        "first inbound message was rejected");
    require(
        tracker.observe(11, 12) == InboundDecision::kAcceptNew,
        "new high sequence was rejected");
    require(
        tracker.observe(12, 11) == InboundDecision::kAcceptReordered,
        "bounded reordering was rejected");
    require(
        tracker.observe(10, 13) == InboundDecision::kDuplicateMessage,
        "logical retry was not recognized");
    require(
        tracker.observe(13, 13) == InboundDecision::kDuplicateSequence,
        "sequence reuse by another message was not rejected");
    require(
        tracker.observe(14, 76) == InboundDecision::kAcceptNew,
        "large forward jump was rejected");
    require(
        tracker.observe(15, 12) == InboundDecision::kTooOld,
        "sequence outside the replay window was accepted");
    require(
        tracker.highest_sequence_number() == 76,
        "highest sequence tracking mismatch");
}

void verify_message_id_eviction_and_reset() {
    InboundSessionTracker tracker;
    for (std::uint64_t value = 1;
         value <= mii::protocol::kRecentMessageIdCapacity + 1;
         ++value) {
        require(
            mii::protocol::should_process(tracker.observe(value, value)),
            "sequential message was rejected");
    }
    require(
        tracker.recent_message_count() ==
            mii::protocol::kRecentMessageIdCapacity,
        "message ID cache exceeded its fixed capacity");
    require(
        tracker.observe(1, 66) == InboundDecision::kAcceptNew,
        "oldest evicted message ID was not reusable");

    tracker.reset();
    require(
        tracker.highest_sequence_number() == 0 &&
            tracker.recent_message_count() == 0,
        "session reset retained replay state");
    require(
        tracker.observe(1, 1) == InboundDecision::kAcceptNew,
        "fresh session did not accept initial identifiers");
}

}  // namespace

int main() {
    try {
        verify_outbound_counters();
        verify_inbound_window();
        verify_message_id_eviction_and_reset();
        std::cout
            << "firmware session state PASS: counters, retry, replay window, reset\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "firmware session state FAIL: " << error.what() << '\n';
        return 1;
    }
}
