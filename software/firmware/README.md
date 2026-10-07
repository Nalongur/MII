# MII V7 firmware baseline

This directory is the first lightweight ESP-IDF/C++ skeleton for the MII V7
board (`ESP32-S3-MINI-1-N8`, 8 MB flash, no PSRAM).

The project is pinned to ESP-IDF 5.5.5 in `versions.json`. The GPIO component
dependency uses the corresponding `esp_driver_gpio` component name.

## What is implemented

- The build target and N8 defaults are explicit in `CMakeLists.txt` and
  `sdkconfig.defaults.v7_n8`.
- Debug and Release optimization choices live in separate defaults files, so
  build policy is not mixed into the V7 hardware definition.
- Each build directory owns its generated `sdkconfig`; Debug and Release do not
  reuse a project-root configuration.
- `board_v7` is the single owner of raw V7 GPIO numbers and active levels.
- `app_main` first asserts `GPIO21 PSHOLD`, before any log or peripheral work.
- The remaining safe outputs keep GNSS disabled (`GPIO15` low), the active-low
  status LED off (`GPIO48` high), and EPD chip-select inactive (`GPIO10` high).
- `partitions_v7.csv` provides two OTA slots and a provisional combined MII data
  area within the 8 MB device boundary.
- `mii_protocol` exposes a bounded C++17 codec for `DeviceStatusRequest`
  envelopes. It uses fixed-capacity buffers and performs no heap allocation.
- The protocol component also owns fixed-memory outbound counters and an inbound
  64-packet replay window with a 64-entry recent-message-ID cache.
- Debug builds run the protocol's known-vector and session-state checks after
  safe GPIO initialization. Release builds omit the startup self-test.
- `mii_ble` compiles a one-connection NimBLE GATT transport. RX uses a static
  eight-item callback queue and a single protocol-task owner. TX frames enter a
  static four-item producer queue, then a four-frame host-context FIFO fragments
  notifications to `ATT_MTU - 3` with a three-attempt congestion bound. The
  service is disabled in both profiles, and received commands remain
  quarantined until an authenticated application session exists.

All other pins remain at their reset defaults. No display, sensor, GNSS,
display, sensor, GNSS, storage-format, or shutdown behavior is implemented in
this baseline. The BLE radio path has not been exercised on hardware.

The `mii_protocol` ESP-IDF component compiles committed nanopb 0.4.9.1 types,
the vendored minimal runtime, and the same codec sources exercised by the host
tests. The host build regenerates the types from `protocol/mii.proto`, rejects
any drift from the committed component sources, and checks fixed-vector parity,
invalid inputs, truncated input, unsupported versions, and unknown fields.
Debug and Release ESP32-S3 images were both successfully rebuilt with ESP-IDF
5.5.5 through the twelfth development round. The session state is deliberately RAM
only and must be reset whenever the future authentication layer opens a new
session; it is not a substitute for authentication or message integrity.

## Intended build command

The repository build wrapper locates the pinned default installation, verifies
the exact ESP-IDF version, verifies the dormant BLE gate, and builds both profiles:

```powershell
powershell -ExecutionPolicy Bypass -File tools\build_firmware.ps1
```

Use `-Profile Debug` or `-Profile Release` to build only one profile, and use
`-IdfPath` if the pinned framework is installed elsewhere. Add `-Fresh` after
changing any `sdkconfig.defaults.*` file so stale generated configuration is
discarded. The wrapper rejects a Debug/Release self-test configuration mismatch.
From an already initialized ESP-IDF shell, the equivalent direct commands are:

```powershell
# Debug (also the CMake default when -D is omitted)
idf.py -B build-debug '-DSDKCONFIG_DEFAULTS=sdkconfig.defaults.v7_n8;sdkconfig.defaults.debug' build

# Release
idf.py -B build-release '-DSDKCONFIG_DEFAULTS=sdkconfig.defaults.v7_n8;sdkconfig.defaults.release' build
```

The build output directories are deliberately ignored by Git. A successful
image build is not a substitute for checking PSHOLD, EPD CS, LED, and GNSS_EN
levels on the physical board; no device has been flashed in this workspace.

## Draft partition boundary

The partition CSV ends at `0x800000`. The single `mii_data` entry is only a W1
capacity placeholder; W6 must split/freeze template, temporary-download, ring
record, and diagnostic storage after measuring real firmware images and data
retention needs.
