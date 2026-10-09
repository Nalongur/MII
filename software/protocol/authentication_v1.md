# MII authentication and binding v1

Status: implementation contract for host-verifiable software preparation. The
radio, physical confirmation, protected storage and Android interoperability
gates remain pending. Business RX remains quarantined.

## Trust boundary

One device has one owner data space. BLE encryption alone does not establish
ownership. Initial credential delivery requires **LE Secure Connections Numeric
Comparison**, both displays showing the same six digits, an explicit phone
confirmation and a fresh device button confirmation. Legacy pairing, Just Works,
debug SC keys, keys shorter than 16 bytes, unattended confirmation and remote
confirmation are rejected. The adapter must obtain security evidence from
NimBLE, never from a protocol field. SC-only is a compile-time requirement;
NimBLE `authenticated`, `encrypted` and `key_size` are runtime requirements.

The owner credential consists of a random 16-byte owner ID, a random 32-byte
owner key, a separate random 32-byte local HTTPS token and a 32-byte phone
certificate SHA-256 fingerprint. Android generates these with its CSPRNG,
persists them using a Keystore wrapping key before transmission, and retains a
pending record if the final device reply is lost. No cloud account is involved.
All four fields are confirmed together; replacing any field requires revocation
and a new physical binding. A copied phone backup cannot silently restore keys.

Threats covered: an unbound nearby phone, passive capture, replay/reflection,
stale queued input, repeated/late confirmation, stolen BLE bond without the
owner key, disconnect/reboot during binding, and failed credential persistence.
Not covered: compromised phone/firmware, extracted owner keys, invasive attacks,
flash rollback by a physical attacker, radio denial of service, or a user
confirming mismatching codes. Numeric comparison does not prove a factory device
identity. Device ID must be stable and locally checked by the user.

## Lifecycle

The single connectivity task owns the state machine. Calls from the button,
NimBLE and storage adapters use bounded events carrying the **full connection
generation**, not a reused BLE handle. A local binding window lasts 60 seconds.
Only an unbound device with healthy protected storage accepts a candidate.
Physical approval applies to exactly that candidate and connection generation;
it cannot be reused after disconnect, timeout or credential replacement.

The candidate remains in RAM until physical approval. Persist one versioned
owner record atomically; acknowledge bound only after commit and readback.
Failure is ambiguous: immediately invalidate all volatile authority and enter
storage fault until a successful reload. Never auto-erase corrupt NVS. Reboot
loads the committed owner or tombstone, with no live authenticated session.
Disconnect wipes candidates, challenges, session keys and replay state.

Revocation requires an authenticated owner request followed by a separate local
confirmation, or a deliberate local recovery action when the phone is lost.
Revoke live authority first, then commit an unbound tombstone with an incremented
generation. Do not erase the generation. Only a confirmed successful commit
permits rebind. Reset of Wi-Fi, bonds and data is coordinated later by recovery;
failure leaves binding closed. Unbinding does not delete unsynchronized records.

## Session and proof transcript

Every connection starts unauthenticated, including bonded reconnects. On a
protected SC link the owner sends its owner ID and a fresh 32-byte client nonce.
The device generates a fresh 32-byte nonce using its CSPRNG. The challenge is
single-use and expires after 30 seconds; session authority expires after 15
minutes and requires another handshake. Wall-clock timestamps are not trusted.

The canonical 113-byte transcript is:

```
ASCII "MIIAUTH1" (8) || role (1) || deviceId (16) || ownerId (16)
|| bindingGeneration (u64 little endian) || clientNonce (32) || deviceNonce (32)
```

Use HMAC-SHA-256 under the owner key, with distinct ASCII role bytes:
`D` device proof, `C` client proof, `K` session key, `I` session ID (first 16
bytes). Verify proofs in constant time. A fresh nonce pair separates reconnects
and reboots; generation separates revocation/rebinding. A device proof cannot
be reflected as a client proof. Derivations are a labeled HMAC PRF; no custom
cipher, PIN-derived key or home-made elliptic curve exchange is used.

Authenticate exact canonical Envelope bytes before decoding/dispatch:

```
HMAC-SHA-256(sessionKey,
  ASCII "MIICTL1" (7) || direction ('C' or 'D') || sessionId (16)
  || byteLength (u16 little endian) || encodedEnvelope)
```

The existing Envelope sequence and 64-entry replay window apply only after MAC
verification. Wrong session/MAC must not advance replay state. Duplicates are
reported to the future idempotent dispatcher, never executed twice. Protocol
framing CRC remains an accidental-corruption check. Authenticated transport,
business commit and Android durable acknowledgment are separate outcomes.

`AuthenticationPacket` is the separate canonical outer wire format in the same
`mii.proto`, with authentication version 1 and nonzero request correlation ID.
It carries binding/challenge/proof/ready/failure messages or an authenticated
Envelope with fixed 16-byte session ID and 32-byte MAC. Outer duplicate fields,
unknown fields and nonminimal/noncanonical encodings are rejected by a strict
decode/reencode comparison. Future security-format changes require negotiated
authentication version; legacy business Envelope fields/vectors are unchanged.
The opaque inner Envelope is capped at 512 bytes so wrapper overhead still fits
the existing 604-byte frame payload. The gateway verifies MAC on exact inner
bytes **before** decoding, checks the expected device ID and supported request
type, then advances replay state with the decoded IDs. Raw legacy frames remain
quarantined. Correlation IDs are not durable business ACKs.

Three failed proofs/MACs impose a 60-second device-wide volatile backoff; a
disconnect cannot clear it. Storage and identity faults never downgrade to
unbound. Nonces, transcripts and MAC fields have fixed sizes; one candidate and
one challenge exist at a time. Secret buffers are wiped on invalidation.

## Storage and integration gates

Production binding requires encrypted NVS backed by pre-provisioned protection,
and Android Keystore-backed credential persistence. HMAC-based NVS protection
uses an eFuse key; its default initialization can generate/burn a missing key.
This project must instead read an existing security configuration and fail
closed if it is absent. No automatic eFuse provisioning or flash encryption
activation is performed by the component. Host storage fault tests
do not prove physical flash protection or resistance to flash rollback.

Before business RX is enabled: implement and verify Numeric Comparison display
and device-button delivery, protected persistence, both endpoint proofs, MAC
envelopes, lifecycle generation checks, response/ACK/idempotency, and controlled
target tests. An isolated core or a successful build does not release this gate.

## Primary references

- [ESP-IDF 5.5.5 NVS encryption](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/storage/nvs_encryption.html)
- ESP-IDF 5.5.5 local sources: `components/bt/host/nimble/Kconfig.in`,
  `nimble/host/include/host/ble_gap.h`, and
  `examples/bluetooth/nimble/bleprph/main/main.c` (Numeric Comparison handling).
- [RFC 2104 HMAC](https://www.rfc-editor.org/rfc/rfc2104)
- [RFC 4231 HMAC-SHA-256 test cases](https://www.rfc-editor.org/rfc/rfc4231)
