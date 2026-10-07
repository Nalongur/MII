#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "mii/ble_rx_session.hpp"

namespace {

using mii::ble::BleRxError;
using mii::ble::BleRxSession;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "BLE_RX_SESSION_TEST_FAIL: " << message << '\n';
    std::exit(1);
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
}

struct Capture {
    std::array<std::uint8_t, mii::protocol::kMaxFramePayloadSize> payload{};
    std::size_t size{0};
    std::size_t count{0};
    bool reject{false};
};

bool capture_handler(
    const std::uint8_t* payload,
    std::size_t payload_size,
    void* context) noexcept {
    auto* capture = static_cast<Capture*>(context);
    if (capture == nullptr || capture->reject ||
        payload_size > capture->payload.size()) {
        return false;
    }
    std::memcpy(capture->payload.data(), payload, payload_size);
    capture->size = payload_size;
    ++capture->count;
    return true;
}

std::vector<std::uint8_t> make_frame(const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> frame(
        mii::protocol::kFrameHeaderSize + payload.size());
    const auto result = mii::protocol::encode_frame(
        payload.data(), payload.size(), frame.data(), frame.size());
    require(result.ok() && result.bytes_written == frame.size(), "frame encode failed");
    return frame;
}

}  // namespace

int main() {
    constexpr std::uint16_t kFirstConnection = 7;
    constexpr std::uint16_t kSecondConnection = 11;
    const std::vector<std::uint8_t> payload{0x08, 0x01, 0x4d, 0x49, 0xff};
    const std::vector<std::uint8_t> frame = make_frame(payload);
    Capture capture;

    BleRxSession session;
    require(!session.connected(), "new session reported connected");
    require(
        session.consume_fragment(
            kFirstConnection,
            frame.data(),
            frame.size(),
            0,
            capture_handler,
            &capture)
                .error == BleRxError::kDisconnected,
        "disconnected fragment accepted");
    session.on_connected(mii::ble::kInvalidConnectionHandle);
    require(!session.connected(), "invalid connection handle accepted");
    session.on_connected(kFirstConnection);
    require(
        session.connected() && session.connection_handle() == kFirstConnection,
        "valid connection not installed");

    require(
        session.consume_fragment(
            kFirstConnection, nullptr, 1, 1, capture_handler, &capture)
                .error == BleRxError::kInvalidArgument,
        "null fragment accepted");
    require(
        session.consume_fragment(
            kFirstConnection, frame.data(), frame.size(), 1, nullptr, &capture)
                .error == BleRxError::kInvalidArgument,
        "null handler accepted");
    require(
        session.consume_fragment(
            kSecondConnection,
            frame.data(),
            frame.size(),
            2,
            capture_handler,
            &capture)
                .error == BleRxError::kStaleConnection,
        "stale connection fragment accepted");

    std::array<std::uint8_t, mii::ble::kBleRxMaxFragmentSize + 1> oversized{};
    require(
        session.consume_fragment(
            kFirstConnection,
            oversized.data(),
            oversized.size(),
            3,
            capture_handler,
            &capture)
                .error == BleRxError::kFragmentTooLarge,
        "oversized BLE fragment accepted");

    const std::size_t split = 7;
    auto result = session.consume_fragment(
        kFirstConnection,
        frame.data(),
        split,
        100,
        capture_handler,
        &capture);
    require(result.ok() && result.ingress.frames_delivered == 0,
            "first fragment failed");
    result = session.consume_fragment(
        kFirstConnection,
        frame.data() + split,
        frame.size() - split,
        101,
        capture_handler,
        &capture);
    require(result.ok() && result.ingress.frames_delivered == 1,
            "fragmented frame was not delivered");
    require(
        capture.count == 1 && capture.size == payload.size() &&
            std::memcmp(capture.payload.data(), payload.data(), payload.size()) == 0,
        "delivered payload mismatch");

    static_cast<void>(session.consume_fragment(
        kFirstConnection,
        frame.data(),
        split,
        200,
        capture_handler,
        &capture));
    session.on_connected(kSecondConnection);
    require(
        session.connection_handle() == kSecondConnection &&
            session.counters().connections_replaced == 1,
        "connection replacement not recorded");
    require(
        session.consume_fragment(
            kFirstConnection,
            frame.data() + split,
            frame.size() - split,
            201,
            capture_handler,
            &capture)
                .error == BleRxError::kStaleConnection,
        "old connection completed a replaced partial frame");
    result = session.consume_fragment(
        kSecondConnection,
        frame.data(),
        frame.size(),
        202,
        capture_handler,
        &capture);
    require(result.ok() && result.ingress.frames_delivered == 1,
            "replacement connection could not deliver a clean frame");

    session.on_disconnected(kFirstConnection);
    require(session.connected(), "stale disconnect closed active connection");
    static_cast<void>(session.consume_fragment(
        kSecondConnection,
        frame.data(),
        split,
        300,
        capture_handler,
        &capture));
    require(!session.poll_timeout(2299), "partial frame timed out early");
    require(session.poll_timeout(2300), "partial frame timeout did not fire");

    capture.reject = true;
    result = session.consume_fragment(
        kSecondConnection,
        frame.data(),
        frame.size(),
        2301,
        capture_handler,
        &capture);
    require(result.error == BleRxError::kHandlerRejected,
            "handler rejection not propagated");
    capture.reject = false;

    session.on_disconnected(kSecondConnection);
    require(!session.connected(), "active disconnect did not close session");
    require(
        session.consume_fragment(
            kSecondConnection,
            frame.data(),
            frame.size(),
            2302,
            capture_handler,
            &capture)
                .error == BleRxError::kDisconnected,
        "post-disconnect fragment accepted");

    const auto counters = session.counters();
    require(
        counters.connections_started == 2 &&
            counters.connections_replaced == 1 && counters.disconnects == 1 &&
            counters.stale_disconnects == 1 && counters.stale_fragments == 2 &&
            counters.oversize_fragments == 1,
        "BLE session counters wrong");
    require(
        session.ingress_counters().frames_delivered == 2 &&
            session.ingress_counters().partial_timeouts == 1 &&
            session.ingress_counters().handler_rejections == 1,
        "frame ingress counters wrong through BLE session");
    session.clear_counters();
    require(
        session.counters().fragments_accepted == 0 &&
            session.ingress_counters().frames_delivered == 0,
        "counter clear failed");

    std::cout << "BLE_RX_SESSION_TEST_PASS: connection gate, fragmentation, "
                 "replacement, timeout, rejection, disconnect, and counters\n";
    return 0;
}
