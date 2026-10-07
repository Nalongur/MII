# Owner binding and session core

`owner_session.cpp` is an allocation-free, single-task state machine. It owns
one persistent owner, one pending candidate and one active challenge/session.
Crypto and protected persistence must be supplied by trusted platform ports.
Missing ports, bad record versions and failed/ambiguous commits fail closed.

The Windows host test uses Windows CNG HMAC-SHA-256 and deterministic **test-only**
nonces. Python hashlib independently establishes the fixed vector; RFC 4231
case 1 verifies the CNG primitive. These are host code/crypto evidence only.

The ESP-IDF component is compiled as a dependency of `main`; it does not change
boot behavior or activate BLE. The mbedTLS port reuses one checked bounded context
and replaces old HMAC key pads with zero-key pads after each operation. The RNG
adapter requires an enabled BLE controller; host RNG stubs are plumbing only.

`IdfOwnerStore` exclusively owns the dedicated `mii_auth` NVS partition at
`0x12000`/`0x3000`, using formerly unused alignment space before OTA. Existing
partition offsets/capacities are unchanged. It requires a pre-provisioned HMAC_UP
key ID supplied by local configuration, reads its security configuration, removes
any preexisting plaintext initialization, and calls encrypted initialization.
No key generation, eFuse programming, plaintext fallback or erase is performed.
Records use a fixed 132-byte CRC blob; commit requires a matching reopen/readback.

`tools/test_auth_platform.ps1` compiles real mbedTLS 3.6.6 from pinned ESP-IDF
5.5.5 with a minimal host config. NVS/RNG APIs are explicitly stubbed; the storage
test verifies call order and fault handling, not encrypted flash or power cuts.

Numeric Comparison, actual protected NVS, physical confirmation and protocol
dispatcher integration remain unfinished gates. No boolean supplied by a remote
caller may establish link authentication or physical confirmation.

`AuthWireGateway` verifies the exact opaque Envelope bytes before parsing them,
derives the expected 32-hex-character device identity from the same immutable
binary session identity, then applies decoded IDs to the replay window.
AuthenticationPacket uses existing nanopb generation and a streaming canonical
comparison, without a second packet-sized scratch buffer. The experimental BLE
RX path still rejects all business frames; gateway tests do not release it.

Contract: [`authentication_v1.md`](../../../protocol/authentication_v1.md).

`NumericComparisonGate` is a fixed single-task preparation core. A physical
60-second window permits one NimBLE-supplied six-digit prompt. Only a trusted
display-completion event followed by a fresh debounced release/press can accept
that exact connection generation and prompt ID. Timeout, disconnect and local
cancel reject; a decision is consumed once and expires before host injection.
The adapter must compare the returned prompt ID with its current pending SMP
request before calling `ble_sm_inject_io`. No NimBLE/display/button bridge has
been enabled yet; the existing experimental pairing configuration is unchanged.
