# HashID FZ v1.0.2

HashID FZ is a native Flipper Zero port of the genuine ordered `psypanda/hashID` signature database. It identifies possible hash and encoded-value formats from their structure; it does not crack hashes and never presents a candidate as certainty.

## Native Flipper features

- Contains all 145 ordered upstream signatures and all 272 candidate records from pinned HashID commit `7e8473a823060e56d4b6090a98591e252bd9505e`.
- Identifies manual values entered directly on the Flipper.
- Streams lines from real text files on microSD without loading the complete file into memory.
- Reports every matching candidate in upstream order.
- Optionally displays corresponding Hashcat modes and John the Ripper format names where upstream data provides them.
- Supports the extended-candidate database independently from the default results.
- Rejects empty, unterminated, overlong, and malformed inputs using fixed bounds.
- Produces transactional measured reports for file analysis.
- Provides optional, size-limited manual history that is disabled by default because entered values may be sensitive.
- Treats Flipper NFC, RFID, Sub-GHz, and iButton files only as ordinary saved data; hardware identifiers are not automatically treated as password hashes.

Hash identification is inherently ambiguous. A result means that the input shape matches one or more known formats; it is not proof of the algorithm that created the value.

## Optional Raspberry Pi/Linux mode

The included UART companion can run genuine upstream HashID on a Raspberry Pi, Linux laptop, desktop, mini PC, or VM. The Flipper acts as the 3.3 V UART controller and live status display.

The companion uses fixed local input and output paths, fixed operations, bounded ASCII protocol frames, no shell command construction, cancellation handling, a heartbeat timeout, and transactional report promotion. UART cannot provide an executable path or arbitrary shell command.

## Upstream compatibility

The native database is generated reproducibly from the pinned upstream source rather than being replaced by a smaller hand-written list. Automated compatibility checks cover representative MD5, SHA-1, SHA-256, phpass, RACF, bcrypt, LDAP SHA, unknown inputs, exact record counts, and deterministic database regeneration.

## Changes in v1.0.2

- Updated native application, About page, companion, documentation, and package metadata to v1.0.2.
- Retained all 145 upstream signatures and 272 ordered candidate records.
- Rebuilt and validated the FAP against target f7/API 87.1.
- Re-ran native compatibility and external protocol/transaction regression tests.

## Install

HashID FZ requires official Flipper firmware 1.4.3 or later and a microSD card.

1. Download the attached `hashid_fz.fap`.
2. Connect the Flipper by USB and open qFlipper.
3. Copy the FAP to `/ext/apps/Tools/`.
4. Open **Apps → Tools → HashID FZ**.

## Quick start

1. Select **Manual input** to identify one value, or **Analyze text/file** to process newline-delimited values from microSD.
2. Use **Settings** to enable extended candidates, Hashcat modes, John formats, or manual history.
3. Read every result as a list of structural possibilities, not a confirmed algorithm.
4. Use **External HashID** only after starting the documented Linux companion and connecting 3.3 V UART.

## Verification and license

- Native upstream compatibility/regeneration tests: 3 passed.
- Linux companion protocol/transaction tests: 4 passed.
- Authenticated Snyk Code scan: 0 issues at low-or-higher severity.
- uFBT APPCHK: target f7/API 87.1, no unresolved symbols.
- FAP size: 64,080 bytes.
- SHA-256: `CE7973B06845A669F41571F1B5A70D2DFF3A289D440D0B54CBAA59A30F9633AE`

HashID FZ is licensed under GNU GPL v3 or later and preserves the upstream MIT attribution and pinned revision information included in the repository.
