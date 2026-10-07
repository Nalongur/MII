"""Deliberately regenerate reviewed test fixtures using independent stdlib encoders.

Not run by tests. No generated protobuf, firmware serializer or target crypto.
All keys/nonces are public deterministic test values.
"""
import hashlib
import hmac
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / "protocol/test-vectors/authentication_v1.json"
v = json.loads(path.read_text(encoding="utf-8"))


def varint(value):
    out = bytearray()
    while value > 127:
        out.append((value & 127) | 128)
        value >>= 7
    out.append(value)
    return bytes(out)


def integer(tag, value):
    return varint(tag << 3) + varint(value) if value else b""


def data(tag, value):
    return varint((tag << 3) | 2) + varint(len(value)) + value if value else b""


def packet(tag, request, body):
    return integer(1, 1) + integer(2, request) + data(tag, body)


owner = bytes.fromhex(v["owner_id_hex"])
key = bytes.fromhex(v["owner_key_hex"])
client = bytes.fromhex(v["client_nonce_hex"])
nonce = bytes.fromhex(v["device_nonce_hex"])
generation = v["binding_generation"]
session_id = bytes.fromhex(v["session_id_mac_hex"])[:16]
v["session_id_hex"] = session_id.hex()
device_id = v["device_id_hex"].encode("ascii")
envelope = (data(1, integer(1, 1)) + integer(2, 1) + data(3, device_id)
            + integer(4, 1) + integer(6, 1) + data(7, integer(1, 1)))
mac_input = b"MIICTL1C" + session_id + len(envelope).to_bytes(2, "little") + envelope
tag = hmac.new(bytes.fromhex(v["session_key_hex"]), mac_input, hashlib.sha256).digest()
v["authenticated_envelope_hex"] = envelope.hex()
v["control_mac_hex"] = tag.hex()
bodies = [
    ("binding_proposal", 10, data(1, owner) + data(2, key) + data(3, bytes(range(65, 97))) + data(4, bytes(range(97, 129)))),
    ("binding_accepted", 11, data(1, owner) + integer(2, generation)),
    ("session_hello", 12, data(1, owner) + data(2, client)),
    ("session_challenge", 13, data(1, nonce) + data(2, bytes.fromhex(v["device_proof_hex"])) + integer(3, generation)),
    ("session_proof", 14, data(1, bytes.fromhex(v["client_proof_hex"]))),
    ("session_ready", 15, data(1, session_id) + integer(2, generation)),
    ("authenticated", 16, data(1, session_id) + data(2, envelope) + data(3, tag)),
    ("failure", 17, integer(1, 1)),
]
for request, (name, body_tag, body) in enumerate(bodies, start=1):
    v["packet_" + name + "_hex"] = packet(body_tag, request, body).hex()
path.write_text(json.dumps(v, indent=2) + "\n", encoding="utf-8")
print("AUTH_WIRE_FIXTURES_GENERATED: eight independent bounded packets")
