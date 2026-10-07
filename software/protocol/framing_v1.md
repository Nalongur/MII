# MII stream framing v1

The protocol v1 protobuf `Envelope` is carried in a transport-independent,
fixed-header frame. The same bytes can be fragmented across BLE writes or other
byte streams without exposing the decoder to unbounded allocation.

## Wire format

All multibyte integers are little-endian.

| Offset | Size | Field | v1 value |
| ---: | ---: | --- | --- |
| 0 | 2 | magic | ASCII `MI` (`4d 49`) |
| 2 | 1 | framing version | `1` |
| 3 | 1 | flags | `0`; other values are rejected |
| 4 | 2 | payload length | `1..604` bytes |
| 6 | 2 | header CRC | CRC-16/CCITT-FALSE over bytes 0..5 |
| 8 | 4 | payload CRC | CRC-32/IEEE over the payload |
| 12 | N | payload | one serialized protobuf `Envelope` |

CRC-16/CCITT-FALSE uses polynomial `0x1021`, initial value `0xffff`, no
reflection and no final XOR. CRC-32/IEEE uses reflected polynomial
`0xedb88320`, initial value `0xffffffff`, and final XOR `0xffffffff`.

The largest frame is 616 bytes: a 12-byte header plus the protocol codec's
604-byte maximum Envelope. Zero-length payloads are invalid. Header CRC is
checked before version, flags, or length are trusted.

## Streaming contract

- A decoder accepts arbitrary fragments and stops immediately after one frame.
- `bytes_consumed` tells the caller where the next frame begins in the same
  transport read. The caller releases the current frame before resuming.
- Bytes before magic are discarded. A malformed header retains a possible `MI`
  suffix so the next valid frame can resynchronize without resetting the link.
- The transport owns the timer. On its incomplete-frame deadline it calls
  `expire_partial_frame()`, which wipes the partial buffer and reports timeout.
- A ready payload is valid until `release_frame()` or `reset()` and is wiped on
  release. One decoder instance is owned by one task, or access is serialized.

The CRC fields detect accidental corruption only. They are not authentication,
authorization, encryption, replay protection, or a substitute for BLE link and
application security.

## Firmware ingress policy

`FrameIngress` is the allocation-free single-task adapter used before a real
BLE or Wi-Fi transport is connected. Each transport read is consumed until all
complete frames have been delivered or the synchronous business handler rejects
a frame. An incomplete frame expires after 2,000 ms without byte progress; the
caller supplies a monotonic 32-bit millisecond tick, whose normal wraparound is
handled by unsigned subtraction. Disconnect and new-session paths reset decoder
and timer state while diagnostic counters remain available.
