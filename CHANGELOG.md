# Changelog

## 1.0.2 — 2026-09-27

- Refreshed application and companion version metadata.
- Rebuilt and revalidated the target f7/API 87.1 FAP.

## 1.0.0 — 2026-09-27

- Ported all 145 ordered signatures and 272 candidate records from pinned HashID 3.2.0-dev.
- Added a bounded native matcher for upstream literals, classes, ranges, branches, groups, anchors, and repeats.
- Added manual input, extended mode, candidate details, Hashcat modes, John formats, and optional history.
- Added worker-thread batch file analysis with measured progress and cancellation.
- Added conservative extraction from saved Flipper data while excluding short NFC/RFID identifiers.
- Added synchronized transactional reports with failure and cancellation handling.
- Added reproducible database generation, upstream compatibility tests, documentation, and GPL-3.0-or-later attribution.
- Added optional Raspberry Pi/Linux genuine HashID mode with bounded HID1 UART control, transactional output, cancellation, and service documentation.
- Passed the clean f7/API 87.1 build and authenticated Snyk Code scan with zero issues.
- Guarded worker and history-view allocations before use.
- Bounded optional history to 16 KiB, disabled it by default, and surfaced write/sync failures.
- Made external cancellation reliable during startup.
- Added a five-second companion heartbeat timeout and visible malformed/overlong UART errors.
- Hardened Linux startup and serial failures with clean diagnostics and resource closure.
