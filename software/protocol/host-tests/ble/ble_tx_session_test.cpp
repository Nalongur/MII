#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "mii/ble_tx_session.hpp"

namespace {

using mii::ble::BleTxError;
using mii::ble::BleTxFragment;
using mii::ble::BleTxSession;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "BLE_TX_SESSION_TEST_FAIL: " << message << '\n';
    std::exit(1);
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
}

std::vector<std::uint8_t> pattern(std::size_t size, std::uint8_t seed) {
    std::vector<std::uint8_t> bytes(size);
    for (std::size_t index = 0; index < size; ++index) {
        bytes[index] = static_cast<std::uint8_t>(seed + index * 37U);
    }
    return bytes;
}

std::vector<std::size_t> drain(
    BleTxSession* session,
    std::uint16_t connection,
    std::vector<std::uint8_t>* bytes) {
    std::vector<std::size_t> fragment_sizes;
    while (session->pending_frames() != 0) {
        BleTxFragment fragment;
        require(
            session->peek_fragment(connection, &fragment) == BleTxError::kOk,
            "could not peek queued fragment");
        require(fragment.bytes != nullptr && fragment.size != 0,
                "empty fragment offered");
        fragment_sizes.push_back(fragment.size);
        bytes->insert(bytes->end(), fragment.bytes, fragment.bytes + fragment.size);
        require(
            session->mark_fragment_sent(connection) == BleTxError::kOk,
            "could not mark fragment sent");
    }
    return fragment_sizes;
}

}  // namespace

int main() {
    constexpr std::uint16_t kFirstConnection = 4;
    constexpr std::uint16_t kSecondConnection = 9;
    const std::vector<std::uint8_t> maximum_frame =
        pattern(mii::protocol::kMaxEncodedFrameSize, 0x31);

    BleTxSession guards;
    require(
        guards.enqueue_frame(
            kFirstConnection, maximum_frame.data(), maximum_frame.size()) ==
            BleTxError::kDisconnected,
        "disconnected enqueue accepted");
    guards.on_connected(kFirstConnection);
    require(
        guards.enqueue_frame(
            kFirstConnection, maximum_frame.data(), maximum_frame.size()) ==
            BleTxError::kNotSubscribed,
        "unsubscribed enqueue accepted");
    require(
        guards.set_subscribed(kSecondConnection, true) ==
            BleTxError::kStaleConnection,
        "stale subscription accepted");
    require(
        guards.set_mtu(kFirstConnection, 22) == BleTxError::kInvalidMtu &&
            guards.set_mtu(kFirstConnection, 248) == BleTxError::kInvalidMtu,
        "invalid ATT MTU accepted");
    require(
        guards.set_subscribed(kFirstConnection, true) == BleTxError::kOk,
        "subscription rejected");
    require(
        guards.enqueue_frame(kFirstConnection, nullptr, 1) ==
            BleTxError::kInvalidArgument,
        "null frame accepted");
    std::array<std::uint8_t, mii::protocol::kMaxEncodedFrameSize + 1> too_large{};
    require(
        guards.enqueue_frame(
            kFirstConnection, too_large.data(), too_large.size()) ==
            BleTxError::kFrameTooLarge,
        "oversized frame accepted");

    BleTxSession mtu23;
    mtu23.on_connected(kFirstConnection);
    require(mtu23.att_mtu() == 23, "new connection did not start at MTU 23");
    static_cast<void>(mtu23.set_subscribed(kFirstConnection, true));
    require(
        mtu23.enqueue_frame(
            kFirstConnection, maximum_frame.data(), maximum_frame.size()) ==
            BleTxError::kOk,
        "maximum frame enqueue failed at MTU 23");
    std::vector<std::uint8_t> reconstructed;
    const auto mtu23_sizes = drain(&mtu23, kFirstConnection, &reconstructed);
    require(
        mtu23_sizes.size() == 31 && mtu23_sizes.front() == 20 &&
            mtu23_sizes[29] == 20 && mtu23_sizes.back() == 16,
        "MTU 23 fragmentation was not 30x20 + 16");
    require(reconstructed == maximum_frame, "MTU 23 reconstruction mismatch");

    BleTxSession mtu247;
    mtu247.on_connected(kFirstConnection);
    static_cast<void>(mtu247.set_subscribed(kFirstConnection, true));
    require(
        mtu247.set_mtu(kFirstConnection, 247) == BleTxError::kOk,
        "MTU 247 rejected");
    static_cast<void>(mtu247.enqueue_frame(
        kFirstConnection, maximum_frame.data(), maximum_frame.size()));
    reconstructed.clear();
    const auto mtu247_sizes = drain(&mtu247, kFirstConnection, &reconstructed);
    require(
        mtu247_sizes == std::vector<std::size_t>({244, 244, 128}),
        "MTU 247 fragmentation mismatch");
    require(reconstructed == maximum_frame, "MTU 247 reconstruction mismatch");

    BleTxSession fifo;
    fifo.on_connected(kFirstConnection);
    static_cast<void>(fifo.set_subscribed(kFirstConnection, true));
    std::array<std::vector<std::uint8_t>, mii::ble::kBleTxQueueCapacity> queued;
    for (std::size_t index = 0; index < queued.size(); ++index) {
        queued[index] = pattern(index + 1, static_cast<std::uint8_t>(0x40 + index));
        require(
            fifo.enqueue_frame(
                kFirstConnection, queued[index].data(), queued[index].size()) ==
                BleTxError::kOk,
            "bounded FIFO rejected in-capacity frame");
    }
    const std::uint8_t extra = 0xaa;
    require(
        fifo.enqueue_frame(kFirstConnection, &extra, 1) == BleTxError::kQueueFull,
        "bounded FIFO accepted fifth frame");
    for (const auto& expected : queued) {
        BleTxFragment fragment;
        require(
            fifo.peek_fragment(kFirstConnection, &fragment) == BleTxError::kOk &&
                fragment.size == expected.size() &&
                std::memcmp(fragment.bytes, expected.data(), expected.size()) == 0,
            "bounded FIFO order mismatch");
        static_cast<void>(fifo.mark_fragment_sent(kFirstConnection));
    }

    BleTxSession retry;
    retry.on_connected(kFirstConnection);
    static_cast<void>(retry.set_subscribed(kFirstConnection, true));
    const std::vector<std::uint8_t> retry_frame = pattern(21, 0x72);
    static_cast<void>(retry.enqueue_frame(
        kFirstConnection, retry_frame.data(), retry_frame.size()));
    BleTxFragment first_attempt;
    require(
        retry.peek_fragment(kFirstConnection, &first_attempt) == BleTxError::kOk &&
            first_attempt.size == 20,
        "retry first fragment wrong");
    require(
        retry.record_retryable_failure(kFirstConnection) == BleTxError::kOk,
        "first retryable failure exhausted frame");
    BleTxFragment second_attempt;
    require(
        retry.peek_fragment(kFirstConnection, &second_attempt) == BleTxError::kOk &&
            second_attempt.bytes == first_attempt.bytes &&
            second_attempt.size == first_attempt.size,
        "retry did not preserve current fragment");
    require(
        retry.record_retryable_failure(kFirstConnection) == BleTxError::kOk,
        "second retryable failure exhausted frame");
    static_cast<void>(retry.peek_fragment(kFirstConnection, &second_attempt));
    require(
        retry.record_retryable_failure(kFirstConnection) ==
            BleTxError::kRetryExhausted &&
            retry.pending_frames() == 0,
        "third retryable failure did not drop frame");

    BleTxSession lifecycle;
    lifecycle.on_connected(kFirstConnection);
    static_cast<void>(lifecycle.set_subscribed(kFirstConnection, true));
    static_cast<void>(lifecycle.set_mtu(kFirstConnection, 247));
    static_cast<void>(lifecycle.enqueue_frame(
        kFirstConnection, maximum_frame.data(), maximum_frame.size()));
    require(
        lifecycle.set_subscribed(kFirstConnection, false) == BleTxError::kOk &&
            lifecycle.pending_frames() == 0,
        "unsubscribe did not flush pending frames");
    static_cast<void>(lifecycle.set_subscribed(kFirstConnection, true));
    static_cast<void>(lifecycle.enqueue_frame(
        kFirstConnection, maximum_frame.data(), maximum_frame.size()));
    lifecycle.on_connected(kSecondConnection);
    require(
        lifecycle.pending_frames() == 0 && !lifecycle.subscribed() &&
            lifecycle.att_mtu() == 23 &&
            lifecycle.counters().connections_replaced == 1,
        "connection replacement did not reset TX state");
    lifecycle.on_disconnected(kFirstConnection);
    require(lifecycle.connected(), "stale disconnect closed replacement");
    lifecycle.on_disconnected(kSecondConnection);
    require(!lifecycle.connected(), "active disconnect did not close TX session");
    require(
        lifecycle.counters().frames_flushed == 2,
        "unsubscribe/replacement flush counters wrong");

    require(
        mtu23.counters().frames_sent == 1 &&
            mtu23.counters().fragments_sent == 31 &&
            mtu247.counters().frames_sent == 1 &&
            mtu247.counters().fragments_sent == 3 &&
            fifo.counters().queue_full_rejections == 1 &&
            retry.counters().retryable_failures == 3,
        "TX counters wrong");

    std::cout << "BLE_TX_SESSION_TEST_PASS: MTU 23/247 fragmentation, FIFO "
                 "bound, retry exhaustion, unsubscribe, replacement, and disconnect\n";
    return 0;
}
