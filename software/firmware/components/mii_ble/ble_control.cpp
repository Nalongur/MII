#include "mii/ble_control.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_hs_mbuf.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "mii/ble_rx_session.hpp"
#include "mii/ble_tx_session.hpp"
#include "nimble/nimble_npl.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "esp_log.h"

namespace mii::ble {
namespace {

constexpr char kTag[] = "mii_ble";
constexpr char kDeviceName[] = "MII-V7-DEV";
constexpr std::uint16_t kPreferredAttMtu = 247;
constexpr UBaseType_t kRxQueueCapacity = 8;
constexpr UBaseType_t kTxIngressQueueCapacity = 4;
constexpr std::uint32_t kTxRetryDelayMs = 20;
constexpr std::uint32_t kProtocolTaskStackDepth = 3072;
constexpr UBaseType_t kProtocolTaskPriority = 5;
constexpr TickType_t kProtocolPollPeriod = pdMS_TO_TICKS(50);

// Canonical UUIDs:
//   service 4d494900-7a91-4c11-9a5e-4d4949563701
//   RX      4d494901-7a91-4c11-9a5e-4d4949563701
//   TX      4d494902-7a91-4c11-9a5e-4d4949563701
const ble_uuid128_t kControlServiceUuid = BLE_UUID128_INIT(
    0x01, 0x37, 0x56, 0x49, 0x49, 0x4d, 0x5e, 0x9a,
    0x11, 0x4c, 0x91, 0x7a, 0x00, 0x49, 0x49, 0x4d);
const ble_uuid128_t kRxCharacteristicUuid = BLE_UUID128_INIT(
    0x01, 0x37, 0x56, 0x49, 0x49, 0x4d, 0x5e, 0x9a,
    0x11, 0x4c, 0x91, 0x7a, 0x01, 0x49, 0x49, 0x4d);
const ble_uuid128_t kTxCharacteristicUuid = BLE_UUID128_INIT(
    0x01, 0x37, 0x56, 0x49, 0x49, 0x4d, 0x5e, 0x9a,
    0x11, 0x4c, 0x91, 0x7a, 0x02, 0x49, 0x49, 0x4d);

struct RxQueueItem {
    std::uint32_t connection_state{0};
    std::uint16_t length{0};
    std::array<std::uint8_t, kBleRxMaxFragmentSize> bytes{};
};

struct TxIngressItem {
    std::uint32_t connection_state{0};
    std::uint32_t subscription_state{0};
    std::uint16_t length{0};
    std::array<std::uint8_t, mii::protocol::kMaxEncodedFrameSize> bytes{};
};

std::array<std::uint8_t, kRxQueueCapacity * sizeof(RxQueueItem)> g_rx_queue_storage{};
StaticQueue_t g_rx_queue_control{};
QueueHandle_t g_rx_queue{nullptr};

std::array<std::uint8_t,
           kTxIngressQueueCapacity * sizeof(TxIngressItem)>
    g_tx_ingress_storage{};
StaticQueue_t g_tx_ingress_control{};
QueueHandle_t g_tx_ingress_queue{nullptr};

std::array<StackType_t, kProtocolTaskStackDepth> g_protocol_task_stack{};
StaticTask_t g_protocol_task_control{};
TaskHandle_t g_protocol_task{nullptr};

// High 16 bits are a generation and low 16 bits are a NimBLE handle. A
// generation change invalidates queued writes, including handle reuse.
std::atomic<std::uint32_t> g_connection_state{kInvalidConnectionHandle};
std::atomic<std::uint32_t> g_queue_drops{0};
std::atomic<std::uint32_t> g_tx_queue_drops{0};
// High 31 bits are a subscription generation and bit 0 is the enabled flag.
// Every transition changes the full value, preventing an unsubscribe/resubscribe
// race from reviving a producer item queued after a reset.
std::atomic<std::uint32_t> g_tx_subscription_state{0};
std::atomic<bool> g_tx_events_ready{false};
std::atomic<bool> g_started{false};
std::uint8_t g_own_address_type{0};
std::uint16_t g_tx_characteristic_handle{0};

BleTxSession g_tx_session{};
ble_npl_event g_tx_event{};
ble_npl_callout g_tx_retry_callout{};

ble_gatt_chr_def g_characteristics[3]{};
ble_gatt_svc_def g_services[2]{};

constexpr std::uint16_t connection_handle(std::uint32_t state) noexcept {
    return static_cast<std::uint16_t>(state & 0xffffU);
}

constexpr std::uint16_t connection_generation(std::uint32_t state) noexcept {
    return static_cast<std::uint16_t>(state >> 16U);
}

constexpr std::uint32_t make_connection_state(
    std::uint16_t generation,
    std::uint16_t handle) noexcept {
    return (static_cast<std::uint32_t>(generation) << 16U) | handle;
}

constexpr bool tx_is_subscribed(std::uint32_t state) noexcept {
    return (state & 1U) != 0;
}

void publish_tx_subscription(bool subscribed) noexcept {
    const std::uint32_t old_state =
        g_tx_subscription_state.load(std::memory_order_relaxed);
    const std::uint32_t generation = (old_state >> 1U) + 1U;
    g_tx_subscription_state.store(
        (generation << 1U) | (subscribed ? 1U : 0U),
        std::memory_order_release);
}

void publish_connection(std::uint16_t handle) noexcept {
    const std::uint32_t old_state =
        g_connection_state.load(std::memory_order_relaxed);
    const auto generation = static_cast<std::uint16_t>(
        connection_generation(old_state) + 1U);
    g_connection_state.store(
        make_connection_state(generation, handle), std::memory_order_release);
}

void publish_disconnect(std::uint16_t handle) noexcept {
    std::uint32_t old_state = g_connection_state.load(std::memory_order_acquire);
    while (connection_handle(old_state) == handle) {
        const auto generation = static_cast<std::uint16_t>(
            connection_generation(old_state) + 1U);
        const std::uint32_t new_state =
            make_connection_state(generation, kInvalidConnectionHandle);
        if (g_connection_state.compare_exchange_weak(
                old_state,
                new_state,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            return;
        }
    }
}

bool reject_unimplemented_frame(
    const std::uint8_t*,
    std::size_t payload_size,
    void*) noexcept {
    // Round 11 deliberately stops at the transport boundary. No received
    // control command is executed until application authorization exists.
    ESP_LOGW(kTag, "quarantined unauthorised control frame (%u bytes)",
             static_cast<unsigned>(payload_size));
    return false;
}

void synchronize_session(
    BleRxSession* session,
    std::uint32_t* observed_state,
    std::uint32_t new_state) noexcept {
    if (*observed_state == new_state) {
        return;
    }
    const std::uint16_t new_handle = connection_handle(new_state);
    const std::uint16_t old_handle = connection_handle(*observed_state);
    if (new_handle == kInvalidConnectionHandle) {
        if (old_handle != kInvalidConnectionHandle) {
            session->on_disconnected(old_handle);
        }
    } else {
        // on_connected() resets a partial frame even when NimBLE reuses the
        // same 16-bit connection handle after a disconnect.
        session->on_connected(new_handle);
    }
    *observed_state = new_state;
}

void schedule_tx_event() noexcept;

void protocol_task(void*) {
    BleRxSession session;
    std::uint32_t observed_state =
        make_connection_state(0, kInvalidConnectionHandle);
    RxQueueItem item;

    for (;;) {
        std::uint32_t current_state =
            g_connection_state.load(std::memory_order_acquire);
        synchronize_session(&session, &observed_state, current_state);

        if (xQueueReceive(g_rx_queue, &item, kProtocolPollPeriod) == pdTRUE) {
            current_state = g_connection_state.load(std::memory_order_acquire);
            synchronize_session(&session, &observed_state, current_state);
            if (item.connection_state != current_state) {
                continue;
            }
            const BleRxResult result = session.consume_fragment(
                connection_handle(item.connection_state),
                item.bytes.data(),
                item.length,
                esp_log_timestamp(),
                reject_unimplemented_frame,
                nullptr);
            if (!result.ok() && result.error != BleRxError::kHandlerRejected) {
                ESP_LOGW(kTag, "RX fragment rejected: error=%u",
                         static_cast<unsigned>(result.error));
            }
        }
        static_cast<void>(session.poll_timeout(esp_log_timestamp()));
        // Keep the public producer API nonblocking. This existing protocol task
        // bridges queued work into the NimBLE host queue within at most one
        // 50 ms poll period; subsequent fragments are driven in host context.
        if (g_tx_ingress_queue != nullptr &&
            uxQueueMessagesWaiting(g_tx_ingress_queue) != 0) {
            schedule_tx_event();
        }
    }
}

int rx_access(
    std::uint16_t conn_handle,
    std::uint16_t,
    ble_gatt_access_ctxt* context,
    void*) {
    if (context == nullptr || context->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    }

    const std::uint16_t length = OS_MBUF_PKTLEN(context->om);
    if (length == 0 || length > kBleRxMaxFragmentSize) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    RxQueueItem item;
    item.connection_state = g_connection_state.load(std::memory_order_acquire);
    if (connection_handle(item.connection_state) != conn_handle) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    item.length = length;
    if (ble_hs_mbuf_to_flat(
            context->om, item.bytes.data(), item.bytes.size(), nullptr) != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    if (item.connection_state !=
        g_connection_state.load(std::memory_order_acquire)) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    if (xQueueSend(g_rx_queue, &item, 0) != pdTRUE) {
        g_queue_drops.fetch_add(1, std::memory_order_relaxed);
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return 0;
}

int tx_access(
    std::uint16_t,
    std::uint16_t,
    ble_gatt_access_ctxt*,
    void*) {
    return BLE_ATT_ERR_READ_NOT_PERMITTED;
}

void schedule_tx_event() noexcept {
    if (!g_tx_events_ready.load(std::memory_order_acquire)) {
        return;
    }
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &g_tx_event);
}

void schedule_tx_retry() noexcept {
    const ble_npl_time_t ticks =
        ble_npl_time_ms_to_ticks32(kTxRetryDelayMs);
    if (ble_npl_callout_reset(&g_tx_retry_callout, ticks) != 0) {
        ESP_LOGE(kTag, "could not schedule TX retry");
    }
}

void drain_tx_ingress(
    std::uint32_t current_state,
    std::uint32_t subscription_state) noexcept {
    TxIngressItem item;
    while (g_tx_session.pending_frames() < kBleTxQueueCapacity &&
           xQueueReceive(g_tx_ingress_queue, &item, 0) == pdTRUE) {
        if (item.connection_state != current_state ||
            item.subscription_state != subscription_state) {
            continue;
        }
        const BleTxError result = g_tx_session.enqueue_frame(
            connection_handle(current_state),
            item.bytes.data(),
            item.length);
        if (result != BleTxError::kOk) {
            ESP_LOGW(kTag, "TX ingress frame rejected: error=%u",
                     static_cast<unsigned>(result));
        }
    }
}

bool tx_work_pending() noexcept {
    return g_tx_session.pending_frames() != 0 ||
           uxQueueMessagesWaiting(g_tx_ingress_queue) != 0;
}

void tx_event_callback(ble_npl_event*) {
    const std::uint32_t state =
        g_connection_state.load(std::memory_order_acquire);
    const std::uint32_t subscription_state =
        g_tx_subscription_state.load(std::memory_order_acquire);
    const std::uint16_t handle = connection_handle(state);
    if (handle == kInvalidConnectionHandle ||
        !tx_is_subscribed(subscription_state) ||
        !g_tx_session.connected() ||
        g_tx_session.connection_handle() != handle) {
        return;
    }

    drain_tx_ingress(state, subscription_state);
    BleTxFragment fragment;
    const BleTxError peek = g_tx_session.peek_fragment(handle, &fragment);
    if (peek == BleTxError::kEmpty) {
        return;
    }
    if (peek != BleTxError::kOk) {
        ESP_LOGW(kTag, "TX fragment unavailable: error=%u",
                 static_cast<unsigned>(peek));
        return;
    }

    os_mbuf* notification =
        ble_hs_mbuf_from_flat(fragment.bytes, fragment.size);
    int result = BLE_HS_ENOMEM;
    if (notification != nullptr) {
        // NimBLE consumes the mbuf on every return path.
        result = ble_gatts_notify_custom(
            handle, g_tx_characteristic_handle, notification);
    }

    if (result == 0) {
        static_cast<void>(g_tx_session.mark_fragment_sent(handle));
        if (tx_work_pending()) {
            schedule_tx_event();
        }
        return;
    }

    if (result == BLE_HS_ENOMEM || result == BLE_HS_EBUSY) {
        const BleTxError retry =
            g_tx_session.record_retryable_failure(handle);
        if (retry == BleTxError::kRetryExhausted) {
            ESP_LOGW(kTag, "TX frame dropped after %u congestion retries",
                     kBleTxRetryLimit);
            if (tx_work_pending()) {
                schedule_tx_event();
            }
        } else {
            schedule_tx_retry();
        }
        return;
    }

    ESP_LOGW(kTag, "TX frame dropped after notify error: rc=%d", result);
    static_cast<void>(g_tx_session.drop_current_frame(handle));
    if (tx_work_pending()) {
        schedule_tx_event();
    }
}

void reset_tx_ingress() noexcept {
    publish_tx_subscription(false);
    ble_npl_callout_stop(&g_tx_retry_callout);
    if (g_tx_ingress_queue != nullptr) {
        xQueueReset(g_tx_ingress_queue);
    }
}

void configure_gatt_definitions() noexcept {
    std::memset(g_characteristics, 0, sizeof(g_characteristics));
    std::memset(g_services, 0, sizeof(g_services));

    g_characteristics[0].uuid = &kRxCharacteristicUuid.u;
    g_characteristics[0].access_cb = rx_access;
    g_characteristics[0].flags = BLE_GATT_CHR_F_WRITE |
                                 BLE_GATT_CHR_F_WRITE_NO_RSP |
                                 BLE_GATT_CHR_F_WRITE_ENC;

    g_characteristics[1].uuid = &kTxCharacteristicUuid.u;
    g_characteristics[1].access_cb = tx_access;
    g_characteristics[1].flags = BLE_GATT_CHR_F_NOTIFY |
                                 BLE_GATT_CHR_F_NOTIFY_INDICATE_ENC;
    g_characteristics[1].val_handle = &g_tx_characteristic_handle;

    g_services[0].type = BLE_GATT_SVC_TYPE_PRIMARY;
    g_services[0].uuid = &kControlServiceUuid.u;
    g_services[0].characteristics = g_characteristics;
}

int gap_event(ble_gap_event* event, void*);

int start_advertising() noexcept {
    ble_hs_adv_fields advertisement{};
    advertisement.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    advertisement.uuids128 = &kControlServiceUuid;
    advertisement.num_uuids128 = 1;
    advertisement.uuids128_is_complete = 1;
    int result = ble_gap_adv_set_fields(&advertisement);
    if (result != 0) {
        return result;
    }

    ble_hs_adv_fields scan_response{};
    scan_response.name = reinterpret_cast<const std::uint8_t*>(kDeviceName);
    scan_response.name_len = sizeof(kDeviceName) - 1;
    scan_response.name_is_complete = 1;
    result = ble_gap_adv_rsp_set_fields(&scan_response);
    if (result != 0) {
        return result;
    }

    ble_gap_adv_params parameters{};
    parameters.conn_mode = BLE_GAP_CONN_MODE_UND;
    parameters.disc_mode = BLE_GAP_DISC_MODE_GEN;
    return ble_gap_adv_start(
        g_own_address_type,
        nullptr,
        BLE_HS_FOREVER,
        &parameters,
        gap_event,
        nullptr);
}

int gap_event(ble_gap_event* event, void*) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status != 0) {
                ESP_LOGW(kTag, "connection attempt failed: status=%d",
                         event->connect.status);
                static_cast<void>(start_advertising());
                return 0;
            }
            publish_connection(event->connect.conn_handle);
            g_tx_session.on_connected(event->connect.conn_handle);
            reset_tx_ingress();
            ESP_LOGI(kTag, "connected: handle=%u; requesting encryption",
                     event->connect.conn_handle);
            if (ble_gap_security_initiate(event->connect.conn_handle) != 0) {
                ESP_LOGW(kTag, "could not initiate link encryption");
            }
            return 0;

        case BLE_GAP_EVENT_DISCONNECT:
            publish_disconnect(event->disconnect.conn.conn_handle);
            g_tx_session.on_disconnected(event->disconnect.conn.conn_handle);
            reset_tx_ingress();
            ESP_LOGI(kTag, "disconnected: handle=%u reason=%d",
                     event->disconnect.conn.conn_handle,
                     event->disconnect.reason);
            static_cast<void>(start_advertising());
            return 0;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            static_cast<void>(start_advertising());
            return 0;

        case BLE_GAP_EVENT_ENC_CHANGE:
            ESP_LOGI(kTag, "encryption change: handle=%u status=%d",
                     event->enc_change.conn_handle,
                     event->enc_change.status);
            return 0;

        case BLE_GAP_EVENT_MTU:
            if (g_tx_session.set_mtu(
                    event->mtu.conn_handle, event->mtu.value) !=
                BleTxError::kOk) {
                ESP_LOGW(kTag, "rejected ATT MTU: handle=%u mtu=%u",
                         event->mtu.conn_handle,
                         event->mtu.value);
            }
            ESP_LOGI(kTag, "ATT MTU: handle=%u mtu=%u",
                     event->mtu.conn_handle,
                     event->mtu.value);
            return 0;

        case BLE_GAP_EVENT_SUBSCRIBE:
            if (event->subscribe.attr_handle != g_tx_characteristic_handle) {
                return 0;
            }
            if (g_tx_session.set_subscribed(
                    event->subscribe.conn_handle,
                    event->subscribe.cur_notify != 0) != BleTxError::kOk) {
                return 0;
            }
            if (event->subscribe.cur_notify != 0) {
                publish_tx_subscription(true);
                schedule_tx_event();
            } else {
                reset_tx_ingress();
            }
            ESP_LOGI(kTag, "TX notifications %s: handle=%u",
                     event->subscribe.cur_notify != 0 ? "enabled" : "disabled",
                     event->subscribe.conn_handle);
            return 0;

        default:
            return 0;
    }
}

void on_reset(int reason) {
    const std::uint16_t handle = connection_handle(
        g_connection_state.load(std::memory_order_acquire));
    if (handle != kInvalidConnectionHandle) {
        publish_disconnect(handle);
        g_tx_session.on_disconnected(handle);
    }
    reset_tx_ingress();
    ESP_LOGE(kTag, "NimBLE host reset: reason=%d", reason);
}

void on_sync() {
    int result = ble_hs_util_ensure_addr(0);
    if (result == 0) {
        result = ble_hs_id_infer_auto(0, &g_own_address_type);
    }
    if (result != 0) {
        ESP_LOGE(kTag, "could not select BLE identity address: rc=%d", result);
        return;
    }
    result = start_advertising();
    if (result != 0) {
        ESP_LOGE(kTag, "could not start advertising: rc=%d", result);
    }
}

void host_task(void*) {
    ESP_LOGI(kTag, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

}  // namespace

esp_err_t start_control_service() noexcept {
    bool expected = false;
    if (!g_started.compare_exchange_strong(expected, true)) {
        return ESP_ERR_INVALID_STATE;
    }

    // Never erase NVS automatically: a storage migration/recovery decision is
    // outside this transport component's authority.
    esp_err_t result = nvs_flash_init();
    if (result != ESP_OK) {
        g_started.store(false);
        ESP_LOGE(kTag, "NVS initialization failed: %s", esp_err_to_name(result));
        return result;
    }

    result = nimble_port_init();
    if (result != ESP_OK) {
        g_started.store(false);
        ESP_LOGE(kTag, "NimBLE initialization failed: %s", esp_err_to_name(result));
        return result;
    }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 0;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    configure_gatt_definitions();
    int host_result = ble_gatts_count_cfg(g_services);
    if (host_result == 0) {
        host_result = ble_gatts_add_svcs(g_services);
    }
    if (host_result == 0) {
        host_result = ble_svc_gap_device_name_set(kDeviceName);
    }
    if (host_result == 0) {
        host_result = ble_att_set_preferred_mtu(kPreferredAttMtu);
    }
    if (host_result != 0) {
        g_started.store(false);
        ESP_LOGE(kTag, "NimBLE GATT configuration failed: rc=%d", host_result);
        return ESP_FAIL;
    }

    g_rx_queue = xQueueCreateStatic(
        kRxQueueCapacity,
        sizeof(RxQueueItem),
        g_rx_queue_storage.data(),
        &g_rx_queue_control);
    if (g_rx_queue == nullptr) {
        g_started.store(false);
        return ESP_ERR_NO_MEM;
    }
    g_tx_ingress_queue = xQueueCreateStatic(
        kTxIngressQueueCapacity,
        sizeof(TxIngressItem),
        g_tx_ingress_storage.data(),
        &g_tx_ingress_control);
    if (g_tx_ingress_queue == nullptr) {
        g_started.store(false);
        return ESP_ERR_NO_MEM;
    }
    ble_npl_event_init(&g_tx_event, tx_event_callback, nullptr);
    if (ble_npl_callout_init(
            &g_tx_retry_callout,
            nimble_port_get_dflt_eventq(),
            tx_event_callback,
            nullptr) != 0) {
        g_started.store(false);
        return ESP_ERR_NO_MEM;
    }
    g_tx_events_ready.store(true, std::memory_order_release);

    g_protocol_task = xTaskCreateStatic(
        protocol_task,
        "mii_ble_rx",
        g_protocol_task_stack.size(),
        nullptr,
        kProtocolTaskPriority,
        g_protocol_task_stack.data(),
        &g_protocol_task_control);
    if (g_protocol_task == nullptr) {
        g_started.store(false);
        return ESP_ERR_NO_MEM;
    }

    nimble_port_freertos_init(host_task);
    ESP_LOGW(kTag,
             "experimental BLE control transport enabled; commands remain quarantined");
    return ESP_OK;
}

esp_err_t queue_control_payload(
    const std::uint8_t* payload,
    std::size_t payload_size) noexcept {
    if (payload == nullptr || payload_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!g_started.load(std::memory_order_acquire) ||
        g_tx_ingress_queue == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    TxIngressItem item;
    item.connection_state =
        g_connection_state.load(std::memory_order_acquire);
    item.subscription_state =
        g_tx_subscription_state.load(std::memory_order_acquire);
    if (connection_handle(item.connection_state) == kInvalidConnectionHandle ||
        !tx_is_subscribed(item.subscription_state)) {
        return ESP_ERR_INVALID_STATE;
    }
    const mii::protocol::FrameEncodeResult encoded =
        mii::protocol::encode_frame(
            payload,
            payload_size,
            item.bytes.data(),
            item.bytes.size());
    if (!encoded.ok()) {
        return ESP_ERR_INVALID_ARG;
    }
    item.length = static_cast<std::uint16_t>(encoded.bytes_written);

    if (item.connection_state !=
            g_connection_state.load(std::memory_order_acquire) ||
        item.subscription_state !=
            g_tx_subscription_state.load(std::memory_order_acquire)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xQueueSend(g_tx_ingress_queue, &item, 0) != pdTRUE) {
        g_tx_queue_drops.fetch_add(1, std::memory_order_relaxed);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

}  // namespace mii::ble
