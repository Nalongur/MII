# mii_protocol ESP-IDF component

This component exposes the nanopb-generated MII protocol types and compiles
the audited nanopb 0.4.9.1 runtime from `third_party/nanopb`.

The public C++17 API in `include/mii/protocol_codec.hpp` currently provides a
bounded `DeviceStatusRequest` envelope encoder and decoder. It uses caller-owned
or fixed-capacity buffers, accepts device IDs up to 32 bytes, rejects unsupported
protocol versions and payload types, and performs no heap allocation. The
largest accepted envelope buffer is 604 bytes; large-object transfer remains a
separate future protocol.

`include/mii/session_state.hpp` adds allocation-free per-session state. The
sender counter allocates non-zero `message_id`/`sequence_number` pairs and
rejects wraparound. The receiver accepts bounded out-of-order delivery through
a 64-packet bitmap, rejects duplicate or stale sequence numbers, and keeps the
64 most recent message IDs to identify a logical retry even when it arrives
with a newer sequence. State is RAM-only, not thread-safe, and must have one
transport-task owner or external serialization. Evicted message IDs are no
longer protected by the idempotency cache.

`include/mii/frame_codec.hpp` adds a transport-independent v1 stream frame for
one encoded Envelope. Its 12-byte header carries `MI` magic, framing version,
flags, a 1..604 byte payload length, CRC-16/CCITT-FALSE for the trusted header,
and CRC-32/IEEE for the payload. `FrameStreamDecoder` uses fixed storage,
accepts arbitrary fragments, stops after one ready frame, preserves a possible
magic suffix after malformed input, and lets the transport expire an incomplete
frame. A ready buffer is wiped on release. CRC detects accidental corruption;
it does not authenticate a peer or replace encryption and replay controls.

`include/mii/frame_ingress.hpp` is the single-owner adapter for a future GATT
or other byte-stream task. It delivers every complete frame synchronously,
continues after malformed frames, expires a partial frame after 2 seconds
without input progress, handles 32-bit monotonic-tick wraparound, and records
saturating delivery/error/timeout/discard counters. Disconnect or a new
authenticated session must call `reset_session()`. This adapter does not start
BLE, define GATT permissions, or authenticate the synchronous handler.

`generated/mii.pb.c` and `generated/mii.pb.h` are committed so a firmware
build does not silently depend on a developer machine's Python environment.
Do not edit them manually. `tools/generate_nanopb.ps1` regenerates the schema
into `build/host/nanopb` and fails unless both committed files match by
SHA-256.

The component is in the ESP-IDF dependency graph through
`firmware/main/CMakeLists.txt`. The exact codec and committed nanopb sources pass
the host conformance build and compile in both ESP32-S3 Debug and Release images.
`CONFIG_MII_PROTOCOL_STARTUP_SELF_TEST` includes fixed-vector, session-state and
stream-framing tests in Debug only; they run after PSHOLD and the remaining safe
outputs are initialized.
The image has not yet been flashed, so successful execution on MII V7 hardware
is not claimed.
