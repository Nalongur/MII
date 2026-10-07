#include "mii/ble_rx_session.hpp"

#include <limits>

namespace mii::ble {
namespace {

void increment_saturated(std::uint32_t* value) noexcept {
    if (*value != std::numeric_limits<std::uint32_t>::max()) {
        ++(*value);
    }
}

}  // namespace

void BleRxSession::on_connected(std::uint16_t connection_handle) noexcept {
    if (connection_handle == kInvalidConnectionHandle) {
        return;
    }
    if (connected()) {
        increment_saturated(&counters_.connections_replaced);
    }
    ingress_.reset_session();
    connection_handle_ = connection_handle;
    increment_saturated(&counters_.connections_started);
}

void BleRxSession::on_disconnected(std::uint16_t connection_handle) noexcept {
    if (!connected() || connection_handle != connection_handle_) {
        increment_saturated(&counters_.stale_disconnects);
        return;
    }
    ingress_.reset_session();
    connection_handle_ = kInvalidConnectionHandle;
    increment_saturated(&counters_.disconnects);
}

BleRxResult BleRxSession::consume_fragment(
    std::uint16_t connection_handle,
    const std::uint8_t* bytes,
    std::size_t size,
    std::uint32_t monotonic_ms,
    mii::protocol::FrameHandler handler,
    void* context) noexcept {
    if (bytes == nullptr || size == 0 || handler == nullptr) {
        increment_saturated(&counters_.fragments_rejected);
        return {BleRxError::kInvalidArgument, {}};
    }
    if (!connected()) {
        increment_saturated(&counters_.fragments_rejected);
        return {BleRxError::kDisconnected, {}};
    }
    if (connection_handle != connection_handle_) {
        increment_saturated(&counters_.fragments_rejected);
        increment_saturated(&counters_.stale_fragments);
        return {BleRxError::kStaleConnection, {}};
    }
    if (size > kBleRxMaxFragmentSize) {
        increment_saturated(&counters_.fragments_rejected);
        increment_saturated(&counters_.oversize_fragments);
        return {BleRxError::kFragmentTooLarge, {}};
    }

    mii::protocol::IngressResult ingress =
        ingress_.consume(bytes, size, monotonic_ms, handler, context);
    if (ingress.error == mii::protocol::IngressError::kHandlerRejected) {
        increment_saturated(&counters_.fragments_rejected);
        return {BleRxError::kHandlerRejected, ingress};
    }
    if (!ingress.ok()) {
        increment_saturated(&counters_.fragments_rejected);
        return {BleRxError::kInvalidArgument, ingress};
    }
    increment_saturated(&counters_.fragments_accepted);
    return {BleRxError::kOk, ingress};
}

bool BleRxSession::poll_timeout(std::uint32_t monotonic_ms) noexcept {
    return connected() && ingress_.poll_timeout(monotonic_ms);
}

void BleRxSession::clear_counters() noexcept {
    counters_ = {};
    ingress_.clear_counters();
}

bool BleRxSession::connected() const noexcept {
    return connection_handle_ != kInvalidConnectionHandle;
}

std::uint16_t BleRxSession::connection_handle() const noexcept {
    return connection_handle_;
}

const BleRxCounters& BleRxSession::counters() const noexcept {
    return counters_;
}

const mii::protocol::IngressCounters& BleRxSession::ingress_counters()
    const noexcept {
    return ingress_.counters();
}

}  // namespace mii::ble
