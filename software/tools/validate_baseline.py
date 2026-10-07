#!/usr/bin/env python3
"""Validate the lightweight MII V7 W1 repository baseline.

This script intentionally uses only the Python standard library.  It checks
repository structure and static configuration; it does not claim that the
firmware, Android application, or physical V7 board has been built or tested.
"""

from __future__ import annotations

import binascii
import csv
import hashlib
import hmac
import json
import re
import sys
import zlib
from pathlib import Path
from typing import Any, Iterable, Mapping, Sequence


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
FLASH_CAPACITY_BYTES = 8 * 1024 * 1024

VERSIONS_FILE = REPOSITORY_ROOT / "versions.json"
SDKCONFIG_FILE = REPOSITORY_ROOT / "firmware" / "sdkconfig.defaults.v7_n8"
DEBUG_SDKCONFIG_FILE = REPOSITORY_ROOT / "firmware" / "sdkconfig.defaults.debug"
RELEASE_SDKCONFIG_FILE = REPOSITORY_ROOT / "firmware" / "sdkconfig.defaults.release"
PARTITIONS_FILE = REPOSITORY_ROOT / "firmware" / "partitions_v7.csv"
FIRMWARE_CMAKE_FILE = REPOSITORY_ROOT / "firmware" / "CMakeLists.txt"
APP_CMAKE_FILE = REPOSITORY_ROOT / "firmware" / "main" / "CMakeLists.txt"
BOARD_CMAKE_FILE = REPOSITORY_ROOT / "firmware" / "components" / "board_v7" / "CMakeLists.txt"
PROTOCOL_COMPONENT_DIRECTORY = (
    REPOSITORY_ROOT / "firmware" / "components" / "mii_protocol"
)
PROTOCOL_COMPONENT_CMAKE = PROTOCOL_COMPONENT_DIRECTORY / "CMakeLists.txt"
PROTOCOL_COMPONENT_KCONFIG = PROTOCOL_COMPONENT_DIRECTORY / "Kconfig"
BLE_COMPONENT_DIRECTORY = REPOSITORY_ROOT / "firmware" / "components" / "mii_ble"
BLE_COMPONENT_CMAKE = BLE_COMPONENT_DIRECTORY / "CMakeLists.txt"
BLE_COMPONENT_KCONFIG = BLE_COMPONENT_DIRECTORY / "Kconfig"
BLE_CONTROL_SOURCE = BLE_COMPONENT_DIRECTORY / "ble_control.cpp"
BLE_RX_SESSION_SOURCE = BLE_COMPONENT_DIRECTORY / "ble_rx_session.cpp"
BLE_TX_SESSION_SOURCE = BLE_COMPONENT_DIRECTORY / "ble_tx_session.cpp"
BLE_CONTROL_HEADER = BLE_COMPONENT_DIRECTORY / "include" / "mii" / "ble_control.hpp"
BLE_RX_SESSION_HEADER = (
    BLE_COMPONENT_DIRECTORY / "include" / "mii" / "ble_rx_session.hpp"
)
BLE_TX_SESSION_HEADER = (
    BLE_COMPONENT_DIRECTORY / "include" / "mii" / "ble_tx_session.hpp"
)
PROTOCOL_CODEC_SOURCE = PROTOCOL_COMPONENT_DIRECTORY / "protocol_codec.cpp"
PROTOCOL_SESSION_STATE_SOURCE = PROTOCOL_COMPONENT_DIRECTORY / "session_state.cpp"
PROTOCOL_FRAME_CODEC_SOURCE = PROTOCOL_COMPONENT_DIRECTORY / "frame_codec.cpp"
PROTOCOL_FRAME_INGRESS_SOURCE = PROTOCOL_COMPONENT_DIRECTORY / "frame_ingress.cpp"
PROTOCOL_SELF_TEST_SOURCE = PROTOCOL_COMPONENT_DIRECTORY / "protocol_self_test.cpp"
PROTOCOL_CODEC_HEADER = (
    PROTOCOL_COMPONENT_DIRECTORY / "include" / "mii" / "protocol_codec.hpp"
)
PROTOCOL_SELF_TEST_HEADER = (
    PROTOCOL_COMPONENT_DIRECTORY / "include" / "mii" / "protocol_self_test.hpp"
)
PROTOCOL_SESSION_STATE_HEADER = (
    PROTOCOL_COMPONENT_DIRECTORY / "include" / "mii" / "session_state.hpp"
)
PROTOCOL_FRAME_CODEC_HEADER = (
    PROTOCOL_COMPONENT_DIRECTORY / "include" / "mii" / "frame_codec.hpp"
)
PROTOCOL_FRAME_INGRESS_HEADER = (
    PROTOCOL_COMPONENT_DIRECTORY / "include" / "mii" / "frame_ingress.hpp"
)
COMMITTED_NANOPB_SOURCE = PROTOCOL_COMPONENT_DIRECTORY / "generated" / "mii.pb.c"
COMMITTED_NANOPB_HEADER = PROTOCOL_COMPONENT_DIRECTORY / "generated" / "mii.pb.h"
BOARD_SOURCE_FILE = REPOSITORY_ROOT / "firmware" / "components" / "board_v7" / "board_v7.cpp"
BOARD_HEADER_FILE = (
    REPOSITORY_ROOT
    / "firmware"
    / "components"
    / "board_v7"
    / "include"
    / "mii"
    / "board_v7.hpp"
)
APP_MAIN_FILE = REPOSITORY_ROOT / "firmware" / "main" / "app_main.cpp"
PROTO_FILE = REPOSITORY_ROOT / "protocol" / "mii.proto"
NANOPB_OPTIONS_FILE = REPOSITORY_ROOT / "protocol" / "mii.options"
VECTOR_FILE = (
    REPOSITORY_ROOT
    / "protocol"
    / "test-vectors"
    / "device_status_request_v1.json"
)
FRAME_VECTOR_FILE = (
    REPOSITORY_ROOT
    / "protocol"
    / "test-vectors"
    / "framed_device_status_request_v1.json"
)
FRAMING_SPEC_FILE = REPOSITORY_ROOT / "protocol" / "framing_v1.md"
HOST_BUILD_SCRIPT = REPOSITORY_ROOT / "tools" / "build_host.ps1"
FIRMWARE_BUILD_SCRIPT = REPOSITORY_ROOT / "tools" / "build_firmware.ps1"
NANOPB_GENERATOR_SCRIPT = REPOSITORY_ROOT / "tools" / "generate_nanopb.ps1"
CPP_VECTOR_TEST = (
    REPOSITORY_ROOT / "protocol" / "host-tests" / "cpp" / "protocol_vector_test.cpp"
)
NANOPB_TEST_DIRECTORY = REPOSITORY_ROOT / "protocol" / "host-tests" / "nanopb"
NANOPB_VECTOR_TEST = NANOPB_TEST_DIRECTORY / "nanopb_vector_test.cpp"
NANOPB_REQUIREMENTS = NANOPB_TEST_DIRECTORY / "requirements.txt"
SESSION_STATE_TEST = (
    REPOSITORY_ROOT / "protocol" / "host-tests" / "session" / "session_state_test.cpp"
)
FRAME_CODEC_TEST = (
    REPOSITORY_ROOT / "protocol" / "host-tests" / "frame" / "frame_codec_test.cpp"
)
FRAME_INGRESS_TEST = (
    REPOSITORY_ROOT / "protocol" / "host-tests" / "frame" / "frame_ingress_test.cpp"
)
BLE_RX_SESSION_TEST = (
    REPOSITORY_ROOT / "protocol" / "host-tests" / "ble" / "ble_rx_session_test.cpp"
)
BLE_TX_SESSION_TEST = (
    REPOSITORY_ROOT / "protocol" / "host-tests" / "ble" / "ble_tx_session_test.cpp"
)
NANOPB_RUNTIME_DIRECTORY = REPOSITORY_ROOT / "third_party" / "nanopb"
NANOPB_UPSTREAM_MANIFEST = NANOPB_RUNTIME_DIRECTORY / "UPSTREAM.json"
KOTLIN_PROJECT = REPOSITORY_ROOT / "protocol" / "host-tests" / "kotlin"
KOTLIN_BUILD_FILE = KOTLIN_PROJECT / "build.gradle.kts"
KOTLIN_VECTOR_TEST = (
    KOTLIN_PROJECT
    / "src"
    / "main"
    / "kotlin"
    / "com"
    / "mii"
    / "protocol"
    / "ProtocolVectorCheck.kt"
)
GRADLE_WRAPPER_PROPERTIES = KOTLIN_PROJECT / "gradle" / "wrapper" / "gradle-wrapper.properties"
GRADLE_WRAPPER_JAR = KOTLIN_PROJECT / "gradle" / "wrapper" / "gradle-wrapper.jar"


class Validation:
    """Collect readable pass/fail results without stopping at the first error."""

    def __init__(self) -> None:
        self.passed = 0
        self.errors: list[str] = []

    def check(self, condition: bool, label: str, detail: str = "") -> bool:
        if condition:
            self.passed += 1
            print(f"[PASS] {label}")
            return True

        message = label if not detail else f"{label}: {detail}"
        self.errors.append(message)
        print(f"[FAIL] {message}")
        return False

    def summary(self) -> int:
        print()
        if self.errors:
            print(
                f"Baseline validation failed: {len(self.errors)} failure(s), "
                f"{self.passed} check(s) passed."
            )
            for index, message in enumerate(self.errors, start=1):
                print(f"  {index}. {message}")
            return 1

        print(f"Baseline validation passed: {self.passed} check(s).")
        print("Boundary: this is static host validation, not a build, flash, or V7-board test.")
        return 0


def load_text(path: Path, validation: Validation, label: str) -> str | None:
    if not validation.check(path.is_file(), label, str(path)):
        return None
    try:
        return path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exc:
        validation.check(False, f"read {path.name}", str(exc))
        return None


def load_json(path: Path, validation: Validation, label: str) -> Any | None:
    text = load_text(path, validation, label)
    if text is None:
        return None
    try:
        return json.loads(text)
    except (json.JSONDecodeError, TypeError) as exc:
        validation.check(False, f"parse {path.name} as JSON", str(exc))
        return None


def require_mapping(value: Any, name: str) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        raise ValueError(f"{name} must be a JSON object")
    return value


def pick(mapping: Mapping[str, Any], *names: str) -> Any:
    for name in names:
        if name in mapping:
            return mapping[name]
    raise ValueError(f"missing required field; expected one of: {', '.join(names)}")


def parse_int(value: Any, name: str) -> int:
    if isinstance(value, bool):
        raise ValueError(f"{name} must be an integer, not a boolean")
    if isinstance(value, int):
        result = value
    elif isinstance(value, str):
        try:
            result = int(value, 0)
        except ValueError as exc:
            raise ValueError(f"{name} is not an integer: {value!r}") from exc
    else:
        raise ValueError(f"{name} must be an integer")
    if result < 0:
        raise ValueError(f"{name} must not be negative")
    return result


def parse_uint(value: Any, name: str, bits: int) -> int:
    result = parse_int(value, name)
    maximum = (1 << bits) - 1
    if result > maximum:
        raise ValueError(f"{name} exceeds uint{bits}")
    return result


def parse_partition_number(value: str) -> int:
    normalized = value.strip().lower().replace("_", "")
    multiplier = 1
    if normalized.endswith("k"):
        normalized = normalized[:-1]
        multiplier = 1024
    elif normalized.endswith("m"):
        normalized = normalized[:-1]
        multiplier = 1024 * 1024
    if not normalized:
        raise ValueError("empty numeric field")
    return int(normalized, 0) * multiplier


def encode_varint(value: int) -> bytes:
    if value < 0:
        raise ValueError("this baseline encoder accepts only non-negative varints")
    encoded = bytearray()
    while value > 0x7F:
        encoded.append((value & 0x7F) | 0x80)
        value >>= 7
    encoded.append(value)
    return bytes(encoded)


def encode_varint_field(field_number: int, value: int) -> bytes:
    if value == 0:
        return b""
    return encode_varint(field_number << 3) + encode_varint(value)


def encode_bytes_field(field_number: int, value: bytes) -> bytes:
    if not value:
        return b""
    tag = encode_varint((field_number << 3) | 2)
    return tag + encode_varint(len(value)) + value


def protocol_version_parts(value: Any) -> tuple[int, int, int]:
    if isinstance(value, str):
        match = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)", value.strip())
        if match is None:
            raise ValueError("version must use MAJOR.MINOR.PATCH")
        parts = tuple(int(part) for part in match.groups())
        if any(part > 0xFFFFFFFF for part in parts):
            raise ValueError("version component exceeds uint32")
        return parts  # type: ignore[return-value]

    version = require_mapping(value, "version")
    return (
        parse_uint(pick(version, "major"), "version.major", 32),
        parse_uint(pick(version, "minor"), "version.minor", 32),
        parse_uint(pick(version, "patch"), "version.patch", 32),
    )


def encode_payload(value: Any) -> bytes:
    if isinstance(value, str):
        payload_wire = bytes.fromhex(value)
    else:
        payload = require_mapping(value, "payload")
        include_counters = pick(
            payload,
            "include_runtime_counters",
            "includeRuntimeCounters",
        )
        if not isinstance(include_counters, bool):
            raise ValueError("payload.include_runtime_counters must be a boolean")
        payload_wire = encode_varint_field(1, int(include_counters))
    if len(payload_wire) > 512:
        raise ValueError("encoded payload exceeds nanopb max_size 512")
    return payload_wire


def encode_vector(vector_document: Mapping[str, Any]) -> bytes:
    envelope_value = vector_document.get("envelope", vector_document)
    envelope = require_mapping(envelope_value, "envelope")

    version_value = pick(envelope, "version", "protocol_version", "protocolVersion")
    major, minor, patch = protocol_version_parts(version_value)
    version_wire = b"".join(
        (
            encode_varint_field(1, major),
            encode_varint_field(2, minor),
            encode_varint_field(3, patch),
        )
    )

    payload_wire = encode_payload(pick(envelope, "payload"))
    device_id = pick(envelope, "device_id", "deviceId")
    if not isinstance(device_id, str):
        raise ValueError("device_id must be a string")
    device_id_wire = device_id.encode("utf-8")
    if len(device_id_wire) > 32:
        raise ValueError("device_id exceeds nanopb max_length 32 UTF-8 bytes")

    return b"".join(
        (
            encode_bytes_field(1, version_wire),
            encode_varint_field(
                2,
                parse_uint(
                    pick(envelope, "message_id", "messageId"),
                    "message_id",
                    64,
                ),
            ),
            encode_bytes_field(3, device_id_wire),
            encode_varint_field(
                4,
                parse_uint(
                    pick(envelope, "sequence_number", "sequenceNumber"),
                    "sequence_number",
                    64,
                ),
            ),
            encode_varint_field(
                5,
                parse_uint(
                    pick(envelope, "sent_at_unix_ms", "sentAtUnixMs", "sent_at"),
                    "sent_at_unix_ms",
                    63,
                ),
            ),
            encode_varint_field(
                6,
                parse_uint(
                    pick(envelope, "payload_type", "payloadType"),
                    "payload_type",
                    31,
                ),
            ),
            encode_bytes_field(7, payload_wire),
        )
    )


def expected_vector_hex(document: Mapping[str, Any]) -> str:
    try:
        value = pick(
            document,
            "expected_wire_hex",
            "expectedWireHex",
            "expected_hex",
            "expectedHex",
        )
    except ValueError:
        expected = require_mapping(document.get("expected"), "expected")
        value = pick(expected, "wire_hex", "wireHex", "hex")
    if not isinstance(value, str):
        raise ValueError("expected wire hex must be a string")
    normalized = re.sub(r"[\s:_-]", "", value).lower()
    if not normalized or len(normalized) % 2 or re.search(r"[^0-9a-f]", normalized):
        raise ValueError("expected wire hex is not valid hexadecimal")
    return normalized


def check_repository_structure(validation: Validation) -> None:
    for directory_name in ("firmware", "android", "protocol", "tools", "verification"):
        path = REPOSITORY_ROOT / directory_name
        validation.check(path.is_dir(), f"directory {directory_name}/ exists", str(path))

    required_files = (
        REPOSITORY_ROOT / "README.md",
        REPOSITORY_ROOT / "docs" / "MII_整体软件设计说明_v1.0.md",
        REPOSITORY_ROOT / "workflow" / "MII_软件开发与验证工作流_v1.0.md",
        VERSIONS_FILE,
        SDKCONFIG_FILE,
        DEBUG_SDKCONFIG_FILE,
        RELEASE_SDKCONFIG_FILE,
        PARTITIONS_FILE,
        FIRMWARE_CMAKE_FILE,
        APP_CMAKE_FILE,
        BOARD_CMAKE_FILE,
        PROTOCOL_COMPONENT_CMAKE,
        PROTOCOL_COMPONENT_KCONFIG,
        BLE_COMPONENT_CMAKE,
        BLE_COMPONENT_KCONFIG,
        BLE_CONTROL_SOURCE,
        BLE_RX_SESSION_SOURCE,
        BLE_TX_SESSION_SOURCE,
        BLE_CONTROL_HEADER,
        BLE_RX_SESSION_HEADER,
        BLE_TX_SESSION_HEADER,
        PROTOCOL_CODEC_SOURCE,
        PROTOCOL_SESSION_STATE_SOURCE,
        PROTOCOL_FRAME_CODEC_SOURCE,
        PROTOCOL_FRAME_INGRESS_SOURCE,
        PROTOCOL_SELF_TEST_SOURCE,
        PROTOCOL_CODEC_HEADER,
        PROTOCOL_SESSION_STATE_HEADER,
        PROTOCOL_FRAME_CODEC_HEADER,
        PROTOCOL_FRAME_INGRESS_HEADER,
        PROTOCOL_SELF_TEST_HEADER,
        COMMITTED_NANOPB_SOURCE,
        COMMITTED_NANOPB_HEADER,
        BOARD_SOURCE_FILE,
        BOARD_HEADER_FILE,
        APP_MAIN_FILE,
        PROTO_FILE,
        NANOPB_OPTIONS_FILE,
        VECTOR_FILE,
        FRAME_VECTOR_FILE,
        FRAMING_SPEC_FILE,
        HOST_BUILD_SCRIPT,
        FIRMWARE_BUILD_SCRIPT,
        NANOPB_GENERATOR_SCRIPT,
        CPP_VECTOR_TEST,
        NANOPB_VECTOR_TEST,
        NANOPB_REQUIREMENTS,
        SESSION_STATE_TEST,
        FRAME_CODEC_TEST,
        FRAME_INGRESS_TEST,
        BLE_RX_SESSION_TEST,
        BLE_TX_SESSION_TEST,
        NANOPB_UPSTREAM_MANIFEST,
        NANOPB_RUNTIME_DIRECTORY / "LICENSE.txt",
        NANOPB_RUNTIME_DIRECTORY / "pb.h",
        NANOPB_RUNTIME_DIRECTORY / "pb_common.c",
        NANOPB_RUNTIME_DIRECTORY / "pb_common.h",
        NANOPB_RUNTIME_DIRECTORY / "pb_decode.c",
        NANOPB_RUNTIME_DIRECTORY / "pb_decode.h",
        NANOPB_RUNTIME_DIRECTORY / "pb_encode.c",
        NANOPB_RUNTIME_DIRECTORY / "pb_encode.h",
        KOTLIN_BUILD_FILE,
        KOTLIN_VECTOR_TEST,
        GRADLE_WRAPPER_PROPERTIES,
        GRADLE_WRAPPER_JAR,
    )
    for path in required_files:
        validation.check(path.is_file(), f"required file {path.relative_to(REPOSITORY_ROOT)}", str(path))


def check_versions(validation: Validation) -> None:
    document = load_json(VERSIONS_FILE, validation, "versions.json is readable")
    if document is None:
        return
    if not isinstance(document, Mapping):
        validation.check(False, "versions.json root is an object")
        return

    expected_fields = (
        "hardwareRevision",
        "espIdfVersion",
        "firmwareVersion",
        "protocolVersion",
        "templateFormatVersion",
        "templateRevision",
        "storageFormatVersion",
        "androidAppVersion",
    )
    missing = [name for name in expected_fields if name not in document]
    validation.check(
        not missing,
        "versions.json contains independent version fields",
        "missing " + ", ".join(missing) if missing else "",
    )
    validation.check(
        document.get("hardwareRevision") == "V7",
        "hardwareRevision is V7",
        repr(document.get("hardwareRevision")),
    )
    validation.check(
        document.get("espIdfVersion") == "5.5.5",
        "ESP-IDF toolchain is pinned to 5.5.5",
        repr(document.get("espIdfVersion")),
    )
    validation.check(
        document.get("protocolVersion") == "1.0.0",
        "protocolVersion is 1.0.0",
        repr(document.get("protocolVersion")),
    )
    version_fields = (
        "firmwareVersion",
        "protocolVersion",
        "templateFormatVersion",
        "storageFormatVersion",
        "androidAppVersion",
    )
    invalid_versions = [
        name
        for name in version_fields
        if not isinstance(document.get(name), str)
        or re.fullmatch(
            r"\d+\.\d+\.\d+(?:-[0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*)?",
            document[name],
        )
        is None
    ]
    validation.check(
        not invalid_versions,
        "version strings use semantic-version syntax",
        "invalid " + ", ".join(invalid_versions) if invalid_versions else "",
    )
    template_revision = document.get("templateRevision")
    validation.check(
        isinstance(template_revision, int)
        and not isinstance(template_revision, bool)
        and template_revision >= 0,
        "templateRevision is a non-negative integer",
        repr(template_revision),
    )


def active_sdkconfig_values(text: str) -> dict[str, str]:
    values: dict[str, str] = {}
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        name, value = line.split("=", 1)
        values[name.strip()] = value.strip().strip('"')
    return values


def check_sdkconfig(validation: Validation) -> None:
    text = load_text(SDKCONFIG_FILE, validation, "sdkconfig defaults is readable")
    if text is None:
        return
    values = active_sdkconfig_values(text)
    flash_is_8mb = (
        values.get("CONFIG_ESPTOOLPY_FLASHSIZE_8MB") == "y"
        and values.get("CONFIG_ESPTOOLPY_FLASHSIZE") == "8MB"
    )
    validation.check(
        flash_is_8mb,
        "sdkconfig fixes flash capacity at 8 MB",
        "expected CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y and FLASHSIZE=\"8MB\"",
    )
    no_spiram = re.search(r"(?m)^# CONFIG_SPIRAM is not set\s*$", text) is not None
    active_spiram = any(
        name.startswith("CONFIG_SPIRAM") and value == "y"
        for name, value in values.items()
    )
    validation.check(
        no_spiram and not active_spiram,
        "sdkconfig explicitly disables PSRAM",
        "expected '# CONFIG_SPIRAM is not set' and no active CONFIG_SPIRAM*=y",
    )
    nimble_expectations = {
        "CONFIG_BT_ENABLED": "y",
        "CONFIG_BT_NIMBLE_ENABLED": "y",
        "CONFIG_BT_NIMBLE_ROLE_PERIPHERAL": "y",
        "CONFIG_BT_NIMBLE_ROLE_BROADCASTER": "y",
        "CONFIG_BT_NIMBLE_GATT_SERVER": "y",
        "CONFIG_BT_NIMBLE_SECURITY_ENABLE": "y",
        "CONFIG_BT_NIMBLE_MAX_CONNECTIONS": "1",
        "CONFIG_BT_NIMBLE_MAX_CCCDS": "1",
        "CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU": "247",
    }
    bad_nimble_values = [
        f"{name}={values.get(name)!r}"
        for name, expected in nimble_expectations.items()
        if values.get(name) != expected
    ]
    disabled_roles = (
        "CONFIG_BT_NIMBLE_ROLE_CENTRAL",
        "CONFIG_BT_NIMBLE_ROLE_OBSERVER",
        "CONFIG_BT_NIMBLE_GATT_CLIENT",
    )
    active_disabled_roles = [
        name for name in disabled_roles if values.get(name) == "y"
    ]
    validation.check(
        not bad_nimble_values and not active_disabled_roles,
        "sdkconfig bounds NimBLE to one peripheral GATT-server connection",
        "; ".join(bad_nimble_values + active_disabled_roles),
    )

    debug_text = load_text(DEBUG_SDKCONFIG_FILE, validation, "Debug sdkconfig is readable")
    release_text = load_text(
        RELEASE_SDKCONFIG_FILE,
        validation,
        "Release sdkconfig is readable",
    )
    if debug_text is not None:
        debug_values = active_sdkconfig_values(debug_text)
        validation.check(
            debug_values.get("CONFIG_COMPILER_OPTIMIZATION_DEBUG") == "y",
            "Debug profile selects -Og optimization",
            str(DEBUG_SDKCONFIG_FILE),
        )
        validation.check(
            debug_values.get("CONFIG_MII_PROTOCOL_STARTUP_SELF_TEST") == "y",
            "Debug profile enables the protocol startup self-test",
            str(DEBUG_SDKCONFIG_FILE),
        )
        validation.check(
            re.search(
                r"(?m)^# CONFIG_MII_BLE_CONTROL_SERVICE is not set\s*$",
                debug_text,
            )
            is not None,
            "Debug profile keeps the experimental BLE service dormant",
            str(DEBUG_SDKCONFIG_FILE),
        )
    if release_text is not None:
        release_values = active_sdkconfig_values(release_text)
        validation.check(
            release_values.get("CONFIG_COMPILER_OPTIMIZATION_SIZE") == "y",
            "Release profile selects size optimization",
            str(RELEASE_SDKCONFIG_FILE),
        )
        validation.check(
            re.search(
                r"(?m)^# CONFIG_MII_BLE_CONTROL_SERVICE is not set\s*$",
                release_text,
            )
            is not None,
            "Release profile keeps the experimental BLE service dormant",
            str(RELEASE_SDKCONFIG_FILE),
        )
        validation.check(
            re.search(
                r"(?m)^# CONFIG_MII_PROTOCOL_STARTUP_SELF_TEST is not set\s*$",
                release_text,
            )
            is not None
            and "CONFIG_MII_PROTOCOL_STARTUP_SELF_TEST=y" not in release_text,
            "Release profile disables the protocol startup self-test",
            str(RELEASE_SDKCONFIG_FILE),
        )


def check_board_pins(validation: Validation) -> None:
    text = load_text(BOARD_HEADER_FILE, validation, "board_v7 header is readable")
    if text is None:
        return

    pin_expectations: Sequence[tuple[str, int, Sequence[str]]] = (
        ("PSHOLD", 21, (r"power_?hold", r"pshold")),
        ("GNSS_EN", 15, (r"gnss.*enable", r"gnss_en")),
        ("status LED", 48, (r"status.*led", r"led.*status")),
        ("EPD CS", 10, (r"epd.*(?:chip_?select|cs)",)),
    )
    for label, gpio, name_patterns in pin_expectations:
        number_pattern = rf"(?:GPIO_NUM_{gpio}|(?<![A-Za-z0-9_]){gpio}(?![A-Za-z0-9_]))"
        found = any(
            re.search(rf"(?im)^.*{name_pattern}.*{number_pattern}.*$", text)
            for name_pattern in name_patterns
        )
        validation.check(
            found,
            f"board_v7 declares {label} on GPIO{gpio}",
            str(BOARD_HEADER_FILE),
        )

    level_expectations = (
        (r"kPowerHoldAsserted\s*=\s*1", "PSHOLD asserted level is high"),
        (r"kGnssDisabled\s*=\s*0", "GNSS safe default is low/off"),
        (r"kStatusLedOff\s*=\s*1", "active-low status LED safe default is high/off"),
        (
            r"kEpdChipSelectInactive\s*=\s*1",
            "EPD chip-select safe default is high/inactive",
        ),
    )
    missing_levels = [
        label for pattern, label in level_expectations if re.search(pattern, text) is None
    ]
    validation.check(
        not missing_levels,
        "board_v7 safe output levels match the V7 baseline",
        "; ".join(missing_levels),
    )


def check_firmware_boot_safety(validation: Validation) -> None:
    app_text = load_text(APP_MAIN_FILE, validation, "app_main is readable")
    board_source = load_text(BOARD_SOURCE_FILE, validation, "board_v7 source is readable")
    firmware_cmake = load_text(
        FIRMWARE_CMAKE_FILE,
        validation,
        "firmware CMake is readable",
    )
    cmake_text = load_text(BOARD_CMAKE_FILE, validation, "board_v7 CMake is readable")
    app_cmake = load_text(APP_CMAKE_FILE, validation, "main CMake is readable")
    protocol_cmake = load_text(
        PROTOCOL_COMPONENT_CMAKE,
        validation,
        "mii_protocol CMake is readable",
    )
    if app_text is not None:
        power_hold_position = app_text.find("assert_power_hold()")
        safe_outputs_position = app_text.find("initialize_safe_outputs()")
        protocol_self_test_position = app_text.find("run_startup_self_test()")
        log_position = app_text.find("ESP_LOGI")
        validation.check(
            0 <= power_hold_position
            < safe_outputs_position
            < protocol_self_test_position
            < log_position,
            "app_main establishes safe outputs before protocol self-test and logging",
            str(APP_MAIN_FILE),
        )
    if board_source is not None:
        required_defaults = (
            "pin::kGnssEnable, level::kGnssDisabled",
            "pin::kStatusLed, level::kStatusLedOff",
            "pin::kEpdChipSelect, level::kEpdChipSelectInactive",
        )
        missing_defaults = [item for item in required_defaults if item not in board_source]
        validation.check(
            not missing_defaults,
            "board_v7 applies all conservative output defaults",
            "; missing " + ", ".join(missing_defaults) if missing_defaults else "",
        )
    if cmake_text is not None:
        validation.check(
            re.search(r"\bREQUIRES\s+esp_driver_gpio\b", cmake_text) is not None,
            "board_v7 declares the current ESP-IDF GPIO component dependency",
            str(BOARD_CMAKE_FILE),
        )
    if firmware_cmake is not None:
        validation.check(
            'set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")' in firmware_cmake,
            "Debug and Release builds isolate their generated sdkconfig",
            str(FIRMWARE_CMAKE_FILE),
        )
    if app_cmake is not None:
        validation.check(
            re.search(
                r"\bREQUIRES\s+board_v7\s+mii_ble\s+mii_protocol\b",
                app_cmake,
            )
            is not None,
            "main component requires board_v7, mii_ble, and mii_protocol",
            str(APP_CMAKE_FILE),
        )
    if protocol_cmake is not None:
        required_sources = (
            "generated/mii.pb.c",
            "frame_codec.cpp",
            "frame_ingress.cpp",
            "protocol_codec.cpp",
            "session_state.cpp",
            "protocol_self_test.cpp",
            "pb_common.c",
            "pb_encode.c",
            "pb_decode.c",
            "NANOPB_RUNTIME_DIR",
        )
        missing_sources = [item for item in required_sources if item not in protocol_cmake]
        validation.check(
            not missing_sources,
            "mii_protocol component compiles generated types and nanopb runtime",
            "missing " + ", ".join(missing_sources) if missing_sources else "",
        )


def check_protocol_codec(validation: Validation) -> None:
    codec_header = load_text(
        PROTOCOL_CODEC_HEADER,
        validation,
        "firmware protocol codec header is readable",
    )
    codec_source = load_text(
        PROTOCOL_CODEC_SOURCE,
        validation,
        "firmware protocol codec source is readable",
    )
    self_test_source = load_text(
        PROTOCOL_SELF_TEST_SOURCE,
        validation,
        "firmware protocol self-test source is readable",
    )
    kconfig = load_text(
        PROTOCOL_COMPONENT_KCONFIG,
        validation,
        "firmware protocol Kconfig is readable",
    )
    if codec_header is not None and codec_source is not None:
        markers = (
            "encode_device_status_request",
            "decode_device_status_request",
            "kMaxEncodedEnvelopeSize",
            "kUnsupportedProtocolVersion",
            "kUnexpectedPayloadType",
            "pb_get_encoded_size",
        )
        combined = codec_header + codec_source
        missing = [marker for marker in markers if marker not in combined]
        validation.check(
            not missing and "std::vector" not in combined and "new " not in combined,
            "firmware protocol codec is bounded and allocation-free",
            "missing " + ", ".join(missing) if missing else "dynamic allocation marker found",
        )
    if self_test_source is not None:
        markers = (
            "kExpectedEnvelope",
            "kExpectedFrame",
            "run_known_vector_self_test",
            "run_session_state_self_test",
            "run_frame_codec_self_test",
            "run_startup_self_test",
            "future_wire",
            "decode_device_status_request",
        )
        missing = [marker for marker in markers if marker not in self_test_source]
        validation.check(
            not missing,
            "firmware self-test covers fixed encoding, decoding, and future fields",
            "missing " + ", ".join(missing) if missing else "",
        )
    if kconfig is not None:
        validation.check(
            "config MII_PROTOCOL_STARTUP_SELF_TEST" in kconfig
            and "default n" in kconfig,
            "protocol startup self-test is opt-in by default",
            str(PROTOCOL_COMPONENT_KCONFIG),
        )


def check_protocol_session_state(validation: Validation) -> None:
    header = load_text(
        PROTOCOL_SESSION_STATE_HEADER,
        validation,
        "firmware session state header is readable",
    )
    source = load_text(
        PROTOCOL_SESSION_STATE_SOURCE,
        validation,
        "firmware session state source is readable",
    )
    test_source = load_text(
        SESSION_STATE_TEST,
        validation,
        "firmware session state host test is readable",
    )
    if header is not None and source is not None:
        markers = (
            "kReplayWindowSize = 64",
            "kRecentMessageIdCapacity = 64",
            "OutboundSessionCounters",
            "allocate_message",
            "allocate_retry",
            "InboundSessionTracker",
            "CounterError::kExhausted",
            "InboundDecision::kDuplicateMessage",
            "InboundDecision::kDuplicateSequence",
            "InboundDecision::kTooOld",
            "seen_sequence_mask_",
            "distance >= kReplayWindowSize",
        )
        combined = header + source
        missing = [marker for marker in markers if marker not in combined]
        dynamic_markers = (
            "std::vector",
            "std::string",
            "std::make_unique",
            "std::make_shared",
            "operator new",
            "malloc(",
        )
        found_dynamic = [marker for marker in dynamic_markers if marker in combined]
        validation.check(
            not missing and not found_dynamic,
            "firmware session state is bounded and allocation-free",
            "missing " + ", ".join(missing)
            if missing
            else "dynamic markers " + ", ".join(found_dynamic),
        )
    if test_source is not None:
        markers = (
            "kExhausted",
            "allocate_retry",
            "kAcceptReordered",
            "kDuplicateMessage",
            "kDuplicateSequence",
            "kTooOld",
            "kRecentMessageIdCapacity",
            "tracker.reset()",
        )
        missing = [marker for marker in markers if marker not in test_source]
        validation.check(
            not missing,
            "session state host test covers retry, replay, eviction, and reset",
            "missing " + ", ".join(missing) if missing else "",
        )


def check_protocol_framing(validation: Validation) -> None:
    header = load_text(
        PROTOCOL_FRAME_CODEC_HEADER,
        validation,
        "firmware frame codec header is readable",
    )
    source = load_text(
        PROTOCOL_FRAME_CODEC_SOURCE,
        validation,
        "firmware frame codec source is readable",
    )
    test_source = load_text(
        FRAME_CODEC_TEST,
        validation,
        "firmware frame codec host test is readable",
    )
    ingress_header = load_text(
        PROTOCOL_FRAME_INGRESS_HEADER,
        validation,
        "firmware transport ingress header is readable",
    )
    ingress_source = load_text(
        PROTOCOL_FRAME_INGRESS_SOURCE,
        validation,
        "firmware transport ingress source is readable",
    )
    ingress_test = load_text(
        FRAME_INGRESS_TEST,
        validation,
        "firmware transport ingress host test is readable",
    )
    spec = load_text(
        FRAMING_SPEC_FILE,
        validation,
        "stream framing specification is readable",
    )
    if header is not None and source is not None:
        combined = header + source
        markers = (
            "kFrameHeaderSize = 12",
            "kMaxFramePayloadSize = kMaxEncodedEnvelopeSize",
            "FrameStreamDecoder",
            "encode_frame",
            "kHeaderChecksumMismatch",
            "kPayloadChecksumMismatch",
            "expire_partial_frame",
            "recover_header_suffix",
            "payload_.fill(0)",
        )
        missing = [marker for marker in markers if marker not in combined]
        dynamic_markers = (
            "std::vector",
            "std::string",
            "std::make_unique",
            "std::make_shared",
            "operator new",
            "malloc(",
        )
        found_dynamic = [marker for marker in dynamic_markers if marker in combined]
        validation.check(
            not missing and not found_dynamic,
            "firmware stream framing is bounded and allocation-free",
            "missing " + ", ".join(missing)
            if missing
            else "dynamic markers " + ", ".join(found_dynamic),
        )
    if test_source is not None:
        markers = (
            "fragment <= 17",
            "kFramePending",
            "kPayloadChecksumMismatch",
            "kHeaderChecksumMismatch",
            "kUnsupportedVersion",
            "kUnsupportedFlags",
            "kInvalidPayloadLength",
            "suffix_recovery",
            "expire_partial_frame",
            "overlapping encode",
        )
        missing = [marker for marker in markers if marker not in test_source]
        validation.check(
            not missing,
            "frame host test covers fragments, guards, resync, CRC, and timeout",
            "missing " + ", ".join(missing) if missing else "",
        )
    if ingress_header is not None and ingress_source is not None:
        combined = ingress_header + ingress_source
        markers = (
            "kPartialFrameTimeoutMs = 2000",
            "FrameIngress",
            "FrameHandler",
            "poll_timeout",
            "reset_session",
            "IngressCounters",
            "increment_saturated",
            "monotonic_ms - last_progress_ms_",
        )
        missing = [marker for marker in markers if marker not in combined]
        dynamic_markers = (
            "std::vector",
            "std::string",
            "std::make_unique",
            "std::make_shared",
            "operator new",
            "malloc(",
        )
        found_dynamic = [marker for marker in dynamic_markers if marker in combined]
        validation.check(
            not missing and not found_dynamic,
            "transport ingress is bounded, allocation-free, and wrap-safe",
            "missing " + ", ".join(missing)
            if missing
            else "dynamic markers " + ", ".join(found_dynamic),
        )
    if ingress_test is not None:
        markers = (
            "consecutive frame delivery",
            "timeout did not fire at 2 seconds",
            "late input did not expire stale partial frame first",
            "near_wrap",
            "header-error recovery",
            "payload-error recovery",
            "handler rejection boundary",
            "reset did not preserve counters",
        )
        missing = [marker for marker in markers if marker not in ingress_test]
        validation.check(
            not missing,
            "transport ingress test covers delivery, timeout, recovery, and counters",
            "missing " + ", ".join(missing) if missing else "",
        )
    if spec is not None:
        markers = (
            "CRC-16/CCITT-FALSE",
            "CRC-32/IEEE",
            "1..604",
            "bytes_consumed",
            "expire_partial_frame()",
            "not authentication",
        )
        missing = [marker for marker in markers if marker not in spec]
        validation.check(
            not missing,
            "framing specification defines bounds, recovery, and security boundary",
            "missing " + ", ".join(missing) if missing else "",
        )


def check_ble_transport(validation: Validation) -> None:
    rx_header = load_text(
        BLE_RX_SESSION_HEADER,
        validation,
        "BLE receive session header is readable",
    )
    rx_source = load_text(
        BLE_RX_SESSION_SOURCE,
        validation,
        "BLE receive session source is readable",
    )
    rx_test = load_text(
        BLE_RX_SESSION_TEST,
        validation,
        "BLE receive session host test is readable",
    )
    tx_header = load_text(
        BLE_TX_SESSION_HEADER,
        validation,
        "BLE transmit session header is readable",
    )
    tx_source = load_text(
        BLE_TX_SESSION_SOURCE,
        validation,
        "BLE transmit session source is readable",
    )
    tx_test = load_text(
        BLE_TX_SESSION_TEST,
        validation,
        "BLE transmit session host test is readable",
    )
    control_source = load_text(
        BLE_CONTROL_SOURCE,
        validation,
        "NimBLE control transport source is readable",
    )
    component_cmake = load_text(
        BLE_COMPONENT_CMAKE,
        validation,
        "mii_ble CMake is readable",
    )
    component_kconfig = load_text(
        BLE_COMPONENT_KCONFIG,
        validation,
        "mii_ble Kconfig is readable",
    )
    app_source = load_text(
        APP_MAIN_FILE,
        validation,
        "app_main is readable for BLE gate",
    )

    if rx_header is not None and rx_source is not None:
        combined = rx_header + rx_source
        markers = (
            "kBleRxMaxFragmentSize = 244",
            "kInvalidConnectionHandle = 0xffffU",
            "class BleRxSession final",
            "kStaleConnection",
            "kFragmentTooLarge",
            "connections_replaced",
            "ingress_.reset_session()",
            "increment_saturated",
        )
        missing = [marker for marker in markers if marker not in combined]
        dynamic_markers = (
            "std::vector",
            "std::string",
            "std::make_unique",
            "std::make_shared",
            "operator new",
            "malloc(",
        )
        found_dynamic = [marker for marker in dynamic_markers if marker in combined]
        validation.check(
            not missing and not found_dynamic,
            "BLE receive session is bounded, allocation-free, and connection-scoped",
            "missing " + ", ".join(missing)
            if missing
            else "dynamic markers " + ", ".join(found_dynamic),
        )
    if rx_test is not None:
        markers = (
            "disconnected fragment accepted",
            "stale connection fragment accepted",
            "oversized BLE fragment accepted",
            "connection replacement not recorded",
            "old connection completed a replaced partial frame",
            "partial frame timeout did not fire",
            "handler rejection not propagated",
            "post-disconnect fragment accepted",
        )
        missing = [marker for marker in markers if marker not in rx_test]
        validation.check(
            not missing,
            "BLE receive host test covers connection, bound, timeout, and reset guards",
            "missing " + ", ".join(missing) if missing else "",
        )
    if tx_header is not None and tx_source is not None:
        combined = tx_header + tx_source
        markers = (
            "kBleAttMinimumMtu = 23",
            "kBleAttPreferredMtu = 247",
            "kBleTxQueueCapacity = 4",
            "kBleTxRetryLimit = 3",
            "class BleTxSession final",
            "std::array<FrameSlot, kBleTxQueueCapacity>",
            "att_mtu_ - 3U",
            "kRetryExhausted",
            "flush_frames()",
            "increment_saturated",
        )
        missing = [marker for marker in markers if marker not in combined]
        dynamic_markers = (
            "std::vector",
            "std::string",
            "std::make_unique",
            "std::make_shared",
            "operator new",
            "malloc(",
        )
        found_dynamic = [marker for marker in dynamic_markers if marker in combined]
        validation.check(
            not missing and not found_dynamic,
            "BLE transmit session is bounded, allocation-free, and MTU-aware",
            "missing " + ", ".join(missing)
            if missing
            else "dynamic markers " + ", ".join(found_dynamic),
        )
    if tx_test is not None:
        markers = (
            "MTU 23 fragmentation was not 30x20 + 16",
            "MTU 247 fragmentation mismatch",
            "bounded FIFO accepted fifth frame",
            "third retryable failure did not drop frame",
            "unsubscribe did not flush pending frames",
            "connection replacement did not reset TX state",
            "active disconnect did not close TX session",
        )
        missing = [marker for marker in markers if marker not in tx_test]
        validation.check(
            not missing,
            "BLE transmit host test covers fragmentation, bounds, retry, and lifecycle",
            "missing " + ", ".join(missing) if missing else "",
        )
    if control_source is not None:
        markers = (
            "kRxQueueCapacity = 8",
            "kTxIngressQueueCapacity = 4",
            "xQueueCreateStatic",
            "xTaskCreateStatic",
            "BLE_GATT_CHR_F_WRITE_ENC",
            "BLE_GATT_CHR_F_NOTIFY_INDICATE_ENC",
            "BLE_ATT_ERR_INSUFFICIENT_RES",
            "ble_gatts_notify_custom",
            "nimble_port_get_dflt_eventq",
            "ble_npl_callout_reset",
            "queue_control_payload",
            "connection_state",
            "subscription_state",
            "item.connection_state != current_state",
            "item.subscription_state != subscription_state",
            "reject_unimplemented_frame",
            "sm_mitm = 0",
            "sm_sc = 1",
            "Never erase NVS automatically",
        )
        missing = [marker for marker in markers if marker not in control_source]
        dynamic_markers = (
            "xQueueCreate(",
            "xTaskCreate(",
            "std::vector",
            "std::string",
            "malloc(",
            "nvs_flash_erase(",
        )
        found_dynamic = [marker for marker in dynamic_markers if marker in control_source]
        validation.check(
            not missing and not found_dynamic,
            "NimBLE paths use static bounded handoff and quarantine inbound commands",
            "missing " + ", ".join(missing)
            if missing
            else "unsafe markers " + ", ".join(found_dynamic),
        )
    if component_cmake is not None:
        markers = (
            '"ble_control.cpp"',
            '"ble_rx_session.cpp"',
            '"ble_tx_session.cpp"',
            "REQUIRES",
            "bt",
            "mii_protocol",
            "nvs_flash",
            "cxx_std_17",
        )
        missing = [marker for marker in markers if marker not in component_cmake]
        validation.check(
            not missing,
            "mii_ble declares sources, dependencies, and C++17",
            "missing " + ", ".join(missing) if missing else "",
        )
    if component_kconfig is not None:
        validation.check(
            "config MII_BLE_CONTROL_SERVICE" in component_kconfig
            and "depends on BT_NIMBLE_ENABLED" in component_kconfig
            and "default n" in component_kconfig
            and "does not authenticate the peer" in component_kconfig,
            "BLE service is opt-in and documents the unauthenticated pairing boundary",
            str(BLE_COMPONENT_KCONFIG),
        )
    if app_source is not None:
        validation.check(
            "#if CONFIG_MII_BLE_CONTROL_SERVICE" in app_source
            and "mii::ble::start_control_service()" in app_source
            and "command execution is quarantined" in app_source,
            "app_main starts BLE only behind the explicit dormant feature gate",
            str(APP_MAIN_FILE),
        )


def check_frame_vector(validation: Validation) -> None:
    document = load_json(
        FRAME_VECTOR_FILE,
        validation,
        "framed protocol vector is readable",
    )
    payload_document = load_json(
        VECTOR_FILE,
        validation,
        "payload protocol vector is readable for framing",
    )
    if not isinstance(document, Mapping) or not isinstance(payload_document, Mapping):
        return
    try:
        payload_hex = str(document["payload_wire_hex"]).lower()
        frame_hex = str(document["expected_frame_wire_hex"]).lower()
        canonical_payload_hex = expected_vector_hex(payload_document)
        payload = bytes.fromhex(payload_hex)
        frame = bytes.fromhex(frame_hex)
        version = parse_int(document["framing_version"], "framing_version")
        flags = parse_int(document["flags"], "flags")
        header = b"MI" + bytes((version, flags)) + len(payload).to_bytes(2, "little")
        header_crc = binascii.crc_hqx(header, 0xFFFF)
        payload_crc = zlib.crc32(payload) & 0xFFFFFFFF
        expected_frame = (
            header
            + header_crc.to_bytes(2, "little")
            + payload_crc.to_bytes(4, "little")
            + payload
        )
    except (KeyError, TypeError, ValueError) as exc:
        validation.check(False, "framed protocol vector fields are valid", str(exc))
        return
    validation.check(
        payload_hex == canonical_payload_hex,
        "framed vector embeds the canonical Envelope bytes",
        f"framed {payload_hex}, canonical {canonical_payload_hex}",
    )
    validation.check(
        frame == expected_frame,
        "independent CRC implementation reproduces the fixed frame vector",
        f"expected {expected_frame.hex()}, declared {frame_hex}",
    )
    validation.check(
        document.get("payload_length") == len(payload)
        and document.get("expected_frame_length") == len(frame)
        and len(payload) <= 604
        and len(frame) <= 616,
        "framed vector lengths match the bounded v1 wire format",
        repr(
            {
                "payload": document.get("payload_length"),
                "frame": document.get("expected_frame_length"),
            }
        ),
    )
    validation.check(
        document.get("header_crc16_ccitt_false") == f"0x{header_crc:04x}"
        and document.get("payload_crc32_ieee") == f"0x{payload_crc:08x}",
        "framed vector declares the independently calculated CRC values",
        repr(
            {
                "header": document.get("header_crc16_ccitt_false"),
                "payload": document.get("payload_crc32_ieee"),
            }
        ),
    )


def partition_rows(text: str) -> Iterable[tuple[str, str, str, int, int]]:
    reader = csv.reader(
        line for line in text.splitlines() if line.strip() and not line.lstrip().startswith("#")
    )
    for row_number, row in enumerate(reader, start=1):
        if len(row) < 5:
            raise ValueError(f"partition row {row_number} has fewer than five columns")
        name, partition_type, subtype, offset_text, size_text = (
            cell.strip() for cell in row[:5]
        )
        if not name:
            raise ValueError(f"partition row {row_number} has an empty name")
        if not offset_text:
            raise ValueError(
                f"partition {name!r} has an implicit offset; this draft must be auditable"
            )
        offset = parse_partition_number(offset_text)
        size = parse_partition_number(size_text)
        if size <= 0:
            raise ValueError(f"partition {name!r} has a non-positive size")
        yield name, partition_type, subtype, offset, size


def check_partitions(validation: Validation) -> None:
    text = load_text(PARTITIONS_FILE, validation, "partition table is readable")
    if text is None:
        return
    try:
        rows = list(partition_rows(text))
    except (ValueError, csv.Error) as exc:
        validation.check(False, "partition table parses", str(exc))
        return

    validation.check(bool(rows), "partition table has entries")
    names = {row[0] for row in rows}
    required_names = {"nvs", "otadata", "phy_init", "mii_auth", "ota_0", "ota_1", "mii_data"}
    missing_names = sorted(required_names - names)
    validation.check(
        not missing_names,
        "partition table contains V7 baseline regions",
        "missing " + ", ".join(missing_names) if missing_names else "",
    )
    validation.check(("mii_auth", "data", "nvs", 0x12000, 0x3000) in rows,
                     "owner NVS uses reserved pre-OTA space without moving existing regions")

    ordered = sorted(rows, key=lambda row: row[3])
    overlap_details: list[str] = []
    for previous, current in zip(ordered, ordered[1:]):
        previous_end = previous[3] + previous[4]
        if current[3] < previous_end:
            overlap_details.append(f"{previous[0]} overlaps {current[0]}")
    validation.check(
        not overlap_details,
        "partition regions do not overlap",
        "; ".join(overlap_details),
    )

    highest_end = max(offset + size for _, _, _, offset, size in rows)
    validation.check(
        highest_end == FLASH_CAPACITY_BYTES,
        "partition regions end exactly at the 8 MiB flash boundary",
        f"highest end is 0x{highest_end:X}; expected 0x{FLASH_CAPACITY_BYTES:X}",
    )


def check_proto_schema(validation: Validation) -> None:
    text = load_text(PROTO_FILE, validation, "mii.proto is readable")
    if text is None:
        return
    schema = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    schema = re.sub(r"//.*", "", schema)
    field_expectations = (
        (r"\buint32\s+major\s*=\s*1\s*;", "ProtocolVersion.major = 1"),
        (r"\buint32\s+minor\s*=\s*2\s*;", "ProtocolVersion.minor = 2"),
        (r"\buint32\s+patch\s*=\s*3\s*;", "ProtocolVersion.patch = 3"),
        (
            r"\bPAYLOAD_TYPE_DEVICE_STATUS_REQUEST\s*=\s*1\s*;",
            "DEVICE_STATUS_REQUEST enum = 1",
        ),
        (r"\bProtocolVersion\s+protocol_version\s*=\s*1\s*;", "protocol_version = 1"),
        (r"\buint64\s+message_id\s*=\s*2\s*;", "message_id = 2"),
        (r"\bstring\s+device_id\s*=\s*3\s*;", "device_id = 3"),
        (r"\buint64\s+sequence_number\s*=\s*4\s*;", "sequence_number = 4"),
        (r"\bint64\s+sent_at_unix_ms\s*=\s*5\s*;", "sent_at_unix_ms = 5"),
        (r"\bPayloadType\s+payload_type\s*=\s*6\s*;", "payload_type = 6"),
        (r"\bbytes\s+payload\s*=\s*7\s*;", "payload = 7"),
        (
            r"\bbool\s+include_runtime_counters\s*=\s*1\s*;",
            "DeviceStatusRequest.include_runtime_counters = 1",
        ),
    )
    missing = [
        label for pattern, label in field_expectations if re.search(pattern, schema) is None
    ]
    validation.check(
        not missing,
        "Envelope field numbers match protocol v1 baseline",
        "missing/mismatched " + ", ".join(missing) if missing else "",
    )

    options_text = load_text(
        NANOPB_OPTIONS_FILE,
        validation,
        "nanopb options are readable",
    )
    if options_text is not None:
        required_options = (
            "mii.protocol.v1.Envelope.device_id max_length:32",
            "mii.protocol.v1.Envelope.payload max_size:512",
        )
        missing_options = [item for item in required_options if item not in options_text]
        validation.check(
            not missing_options,
            "nanopb options bound device ID and control payload",
            "; missing " + ", ".join(missing_options) if missing_options else "",
        )


def check_protocol_vector(validation: Validation) -> None:
    document = load_json(VECTOR_FILE, validation, "protocol vector is readable")
    if document is None:
        return
    if not isinstance(document, Mapping):
        validation.check(False, "protocol vector root is an object")
        return
    try:
        actual = encode_vector(document).hex()
        expected = expected_vector_hex(document)
        envelope = require_mapping(document.get("envelope", document), "envelope")
        payload_actual = encode_payload(pick(envelope, "payload")).hex()
        payload_expected_value = document.get("expected_payload_wire_hex")
        if not isinstance(payload_expected_value, str):
            raise ValueError("expected_payload_wire_hex must be a string")
        payload_expected = re.sub(r"[\s:_-]", "", payload_expected_value).lower()
        major, minor, patch = protocol_version_parts(
            pick(envelope, "version", "protocol_version", "protocolVersion")
        )
        vector_version = f"{major}.{minor}.{patch}"
    except (TypeError, ValueError) as exc:
        validation.check(False, "protocol vector fields are valid", str(exc))
        return
    validation.check(
        actual == expected,
        "independent minimal Protobuf encoder reproduces fixed vector",
        f"expected {expected}, encoded {actual}",
    )
    validation.check(
        payload_actual == payload_expected,
        "independent encoder reproduces fixed payload vector",
        f"expected {payload_expected}, encoded {payload_actual}",
    )
    expected_length = document.get("expected_wire_length")
    validation.check(
        expected_length == len(bytes.fromhex(expected)),
        "fixed vector byte length matches expected_wire_length",
        f"declared {expected_length!r}, actual {len(bytes.fromhex(expected))}",
    )
    versions = load_json(VERSIONS_FILE, validation, "versions.json is readable for vector")
    if isinstance(versions, Mapping):
        validation.check(
            vector_version == versions.get("protocolVersion"),
            "fixed vector protocol version matches versions.json",
            f"vector {vector_version}, manifest {versions.get('protocolVersion')!r}",
        )


def check_host_build_scaffold(validation: Validation) -> None:
    build_script = load_text(HOST_BUILD_SCRIPT, validation, "host build script is readable")
    firmware_build_script = load_text(
        FIRMWARE_BUILD_SCRIPT,
        validation,
        "firmware build script is readable",
    )
    kotlin_build = load_text(KOTLIN_BUILD_FILE, validation, "Kotlin build file is readable")
    kotlin_test = load_text(KOTLIN_VECTOR_TEST, validation, "Kotlin vector test is readable")
    wrapper = load_text(
        GRADLE_WRAPPER_PROPERTIES,
        validation,
        "Gradle wrapper properties are readable",
    )
    if build_script is not None:
        required_steps = (
            "validate_baseline.py",
            "zigExecutable c++",
            "generate_nanopb.ps1",
            "nanopbTestSource",
            "protocolCppSources",
            "protocolIncludeDirectory",
            "sessionTestSource",
            "session_state.cpp",
            "frameTestSource",
            "frame_codec.cpp",
            "Firmware streaming frame build",
            "ingressTestSource",
            "frame_ingress.cpp",
            "Firmware transport ingress build",
            "bleRxTestSource",
            "ble_rx_session.cpp",
            "BLE receive session build",
            "bleTxTestSource",
            "ble_tx_session.cpp",
            "BLE notification transmit session build",
            "gradleExecutable",
        )
        missing_steps = [step for step in required_steps if step not in build_script]
        validation.check(
            not missing_steps,
            "host build script includes protocol, BLE RX/TX, nanopb, and Kotlin stages",
            "missing " + ", ".join(missing_steps) if missing_steps else "",
        )
    if firmware_build_script is not None:
        required_steps = (
            "espIdfVersion",
            "export.ps1",
            "idf.py --version",
            "sdkconfig.defaults.v7_n8;sdkconfig.defaults.$profileName",
            "fullclean",
            "CONFIG_MII_PROTOCOL_STARTUP_SELF_TEST",
            "mii_firmware.bin",
            "bootloader\\bootloader.bin",
        )
        missing_steps = [step for step in required_steps if step not in firmware_build_script]
        validation.check(
            not missing_steps,
            "firmware build script pins ESP-IDF and verifies both image types",
            "missing " + ", ".join(missing_steps) if missing_steps else "",
        )
    if kotlin_build is not None:
        required_dependencies = (
            'kotlin("jvm") version "2.1.10"',
            'id("com.google.protobuf") version "0.9.4"',
            'implementation("com.google.protobuf:protobuf-java:3.24.4")',
            'artifact = "com.google.protobuf:protoc:3.24.4"',
        )
        missing_dependencies = [
            dependency for dependency in required_dependencies if dependency not in kotlin_build
        ]
        validation.check(
            not missing_dependencies,
            "Kotlin host build pins compiler, protobuf plugin, runtime, and protoc",
            "missing " + ", ".join(missing_dependencies) if missing_dependencies else "",
        )
        validation.check(
            "stageCanonicalProto" in kotlin_build
            and 'rootProject.file("../../mii.proto")' in kotlin_build
            and 'layout.buildDirectory.dir("canonical-proto")' in kotlin_build,
            "Kotlin generation stages the canonical mii.proto without a checked-in copy",
            str(KOTLIN_BUILD_FILE),
        )
    if kotlin_test is not None:
        generated_api_markers = (
            "import com.mii.protocol.v1.Envelope",
            "DeviceStatusRequest.newBuilder()",
            "Envelope.parseFrom(envelope)",
            "unknownFields.asMap().containsKey(127)",
        )
        missing_markers = [
            marker for marker in generated_api_markers if marker not in kotlin_test
        ]
        validation.check(
            not missing_markers,
            "Kotlin test uses generated types for encode, decode, and unknown fields",
            "missing " + ", ".join(missing_markers) if missing_markers else "",
        )
    if wrapper is not None:
        validation.check(
            "gradle-8.11.1-bin.zip" in wrapper,
            "Gradle wrapper pins version 8.11.1",
            str(GRADLE_WRAPPER_PROPERTIES),
        )
        validation.check(
            "distributionSha256Sum=f397b287023acdba1e9f6fc5ea72d22dd63669d59ed4a289a29b1a76eee151c6"
            in wrapper,
            "Gradle distribution has a pinned SHA-256",
            str(GRADLE_WRAPPER_PROPERTIES),
        )


def check_nanopb_scaffold(validation: Validation) -> None:
    requirements = load_text(
        NANOPB_REQUIREMENTS,
        validation,
        "nanopb host requirements are readable",
    )
    generator = load_text(
        NANOPB_GENERATOR_SCRIPT,
        validation,
        "nanopb generator script is readable",
    )
    test_source = load_text(
        NANOPB_VECTOR_TEST,
        validation,
        "nanopb vector test is readable",
    )
    manifest = load_json(
        NANOPB_UPSTREAM_MANIFEST,
        validation,
        "nanopb upstream manifest is readable",
    )

    if requirements is not None:
        expected_requirements = {
            "nanopb==0.4.9.1",
            "grpcio-tools==1.62.3",
            "protobuf==4.25.6",
        }
        actual_requirements = {
            line.strip()
            for line in requirements.splitlines()
            if line.strip() and not line.lstrip().startswith("#")
        }
        validation.check(
            actual_requirements == expected_requirements,
            "nanopb Python generator dependencies are exactly pinned",
            repr(sorted(actual_requirements)),
        )
    if generator is not None:
        markers = (
            "--error-on-unmatched",
            "protocol/mii.options",
            "NANOPB_GENERATION_PASS",
            "NANOPB_COMMITTED_SOURCES_PASS",
            "Get-FileHash -Algorithm SHA256",
        )
        missing = [marker for marker in markers if marker not in generator]
        validation.check(
            not missing,
            "nanopb generator uses strict options and verifies outputs",
            "missing " + ", ".join(missing) if missing else "",
        )
    if test_source is not None:
        markers = (
            "encode_device_status_request(",
            "decode_device_status_request(",
            "run_startup_self_test()",
            "kOutputTooSmall",
            "kDeviceIdTooLong",
            "kUnsupportedProtocolVersion",
            "future_wire.insert",
            "nanopb unknown-field skip PASS",
        )
        missing = [marker for marker in markers if marker not in test_source]
        validation.check(
            not missing,
            "firmware codec host test covers round-trip and guard paths",
            "missing " + ", ".join(missing) if missing else "",
        )

    if isinstance(manifest, Mapping):
        validation.check(
            manifest.get("version") == "0.4.9.1"
            and manifest.get("commit") == "cad3c18ef15a663e30e3e43e3a752b66378adec1",
            "vendored nanopb runtime records the audited upstream revision",
            repr({"version": manifest.get("version"), "commit": manifest.get("commit")}),
        )
        hashes = manifest.get("sha256")
        hash_errors: list[str] = []
        if not isinstance(hashes, Mapping):
            hash_errors.append("sha256 is not an object")
        else:
            for name, expected_hash in hashes.items():
                path = NANOPB_RUNTIME_DIRECTORY / str(name)
                if not path.is_file():
                    hash_errors.append(f"missing {name}")
                    continue
                actual_hash = hashlib.sha256(path.read_bytes()).hexdigest()
                if actual_hash != expected_hash:
                    hash_errors.append(f"hash mismatch {name}")
        validation.check(
            not hash_errors,
            "vendored nanopb runtime matches recorded SHA-256 hashes",
            "; ".join(hash_errors),
        )
def check_authentication_vector(validation: Validation) -> None:
    vector_path = REPOSITORY_ROOT / "protocol/test-vectors/authentication_v1.json"
    try:
        vector = json.loads(vector_path.read_text(encoding="utf-8"))
        fields = {name: bytes.fromhex(vector[name + "_hex"]) for name in (
            "device_id", "owner_id", "owner_key", "client_nonce", "device_nonce"
        )}
        sizes = {"device_id": 16, "owner_id": 16, "owner_key": 32,
                 "client_nonce": 32, "device_nonce": 32}
        validation.check(all(len(fields[k]) == n for k, n in sizes.items()),
                         "authentication fixed vector field sizes")
        transcript = (b"MIIAUTH1C" + fields["device_id"] + fields["owner_id"]
                      + vector["binding_generation"].to_bytes(8, "little")
                      + fields["client_nonce"] + fields["device_nonce"])
        validation.check(len(transcript) == 113 and transcript.hex() == vector["client_transcript_hex"],
                         "independent authentication transcript matches vector")
        for name, role in (("device_proof", b"D"), ("client_proof", b"C"),
                           ("session_key", b"K"), ("session_id_mac", b"I")):
            result = hmac.new(fields["owner_key"], transcript[:8] + role + transcript[9:],
                              hashlib.sha256).hexdigest()
            validation.check(result == vector[name + "_hex"], "independent HMAC vector " + name)
        blob = bytes.fromhex(vector["owner_record_blob_hex"])
        validation.check(len(blob) == 132 and blob[:8] == b"MIIO\x01\x01\x00\x00"
                         and int.from_bytes(blob[-4:], "little") == zlib.crc32(blob[:-4]),
                         "owner record independent fixed-size CRC vector")
        session_id = bytes.fromhex(vector["session_id_hex"])
        validation.check(session_id == bytes.fromhex(vector["session_id_mac_hex"])[:16],
                         "authentication session ID uses domain-separated MAC prefix")
        envelope = bytes.fromhex(vector["authenticated_envelope_hex"])
        control = b"MIICTL1C" + session_id + len(envelope).to_bytes(2, "little") + envelope
        validation.check(hmac.new(bytes.fromhex(vector["session_key_hex"]), control,
                                 hashlib.sha256).hexdigest() == vector["control_mac_hex"],
                         "independent exact-envelope control MAC vector")
        names = ("binding_proposal", "binding_accepted", "session_hello", "session_challenge",
                 "session_proof", "session_ready", "authenticated", "failure")
        validation.check(all(0 < len(bytes.fromhex(vector["packet_" + name + "_hex"])) <= 604
                             for name in names) and len(envelope) <= 512,
                         "authentication packet/inner fixtures respect frame bounds")
    except (OSError, ValueError, KeyError, OverflowError, TypeError) as error:
        validation.check(False, "authentication vector is readable and bounded", str(error))


def check_protected_owner_store(validation: Validation) -> None:
    source = load_text(REPOSITORY_ROOT / "firmware/components/mii_auth/idf_owner_store.cpp",
                       validation, "protected owner store adapter is readable")
    if source is not None:
        code = re.sub(r"//[^\n]*", "", source)
        validation.check(not re.search(r"\bnvs_flash_(?:init|generate_keys(?:_v2)?|erase\w*)\s*\(", code),
                         "owner adapter has no auto-provisioning or erase API calls")
        validation.check('constexpr char kPartition[] = "mii_auth";' in code
                         and "nvs_flash_read_security_cfg_v2" in code
                         and "nvs_flash_secure_init_partition" in code,
                         "owner adapter requires derived protection on a dedicated partition")


def main() -> int:
    print(f"MII W1 baseline validation: {REPOSITORY_ROOT}")
    print()
    validation = Validation()
    check_repository_structure(validation)
    check_versions(validation)
    check_sdkconfig(validation)
    check_board_pins(validation)
    check_firmware_boot_safety(validation)
    check_protocol_codec(validation)
    check_protocol_session_state(validation)
    check_protocol_framing(validation)
    check_ble_transport(validation)
    check_partitions(validation)
    check_proto_schema(validation)
    check_protocol_vector(validation)
    check_frame_vector(validation)
    check_host_build_scaffold(validation)
    check_nanopb_scaffold(validation)
    check_authentication_vector(validation)
    check_protected_owner_store(validation)
    return validation.summary()


if __name__ == "__main__":
    sys.exit(main())
