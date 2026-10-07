# MII BLE control transport

This ESP-IDF component is the bounded NimBLE control transport for MII V7. The
implementation is compiled in Debug and Release builds, while
`CONFIG_MII_BLE_CONTROL_SERVICE` remains disabled by default.

## GATT layout

| Item | UUID | Access |
|---|---|---|
| Control service | `4d494900-7a91-4c11-9a5e-4d4949563701` | primary service |
| RX characteristic | `4d494901-7a91-4c11-9a5e-4d4949563701` | write / write without response, encrypted link required |
| TX characteristic | `4d494902-7a91-4c11-9a5e-4d4949563701` | notify, encrypted CCCD write required |

The preferred ATT MTU is 247, so an RX write is bounded to 244 bytes. A NimBLE
access callback only copies the write into an eight-entry static queue. One
protocol task owns `BleRxSession` and `FrameIngress`; it rejects stale queue
items by connection generation, clears partial frames on disconnect or handle
reuse, and polls the two-second partial-frame deadline.

`queue_control_payload()` wraps one encoded protobuf Envelope in the canonical
transport frame and copies it into a four-item static producer queue. The
NimBLE host context owns a second four-frame static FIFO and splits each frame
into `ATT_MTU - 3` notification values: 20 bytes at the mandatory MTU 23 and up
to 244 bytes at the preferred MTU 247. `BLE_HS_ENOMEM` and `BLE_HS_EBUSY` retry
after 20 ms, with the current frame dropped after the third failed attempt.
Queue submission is nonblocking; the protocol task wakes the NimBLE host within
its 50 ms poll period. Queue items carry both connection and subscription
generations. Unsubscribe, disconnect, host reset and connection replacement
flush all pending TX state, so bytes cannot cross either lifecycle boundary.

## Deliberate security boundary

The current Secure Connections Just Works configuration encrypts the link but
does not authenticate the peer against man-in-the-middle attacks. Complete
frames are therefore quarantined and rejected instead of being executed. Keep
the feature disabled until an application pairing/authorization policy binds
the protocol replay state to an authenticated session.

NVS initialization errors are returned to the caller. This component never
erases NVS as an automatic recovery action.

## Validation boundary

`protocol/host-tests/ble/ble_rx_session_test.cpp` exercises the portable RX
connection gate, size bound, timeout and reset rules. The adjacent TX test
exercises MTU 23/247 fragmentation, FIFO capacity, retry exhaustion and
lifecycle flushes. ESP-IDF builds compile the real NimBLE integration, and an
opt-in build has linked the active service path. None of these proves radio
behavior; advertising, pairing, ATT traffic, real controller congestion and
disconnect recovery still require controlled V7 hardware tests.
