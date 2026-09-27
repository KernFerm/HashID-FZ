# HashID FZ

HashID FZ is a native Flipper Zero port of the genuine [psypanda/hashID](https://github.com/psypanda/hashID) identification database. Version 1.0.0 contains all 145 ordered upstream signatures and all 272 candidate records from the pinned HashID 3.2.0-dev revision.

Hash identification is inherently ambiguous. The application returns every matching candidate in upstream order and never claims certainty.

The complete pinned database runs natively on Flipper. An optional Raspberry Pi/Linux mode can run the genuine upstream HashID executable while the Flipper acts as its 3.3 V UART controller and status display. A Raspberry Pi is not required; a Linux laptop, desktop, mini PC, or VM can serve the same role.

## Install

The release targets official Flipper firmware 1.4.3, target f7, API 87.1.

1. Download `hashid_fz.fap` from the latest GitHub release.
2. In qFlipper, open the microSD browser and copy it into `/ext/apps/Tools/`.
3. On the Flipper, open **Apps → Tools → HashID FZ**.

## Manual identification

Select **Manual input**, enter the encoded value, and save the input. The result shows its measured length and character form followed by every legitimate candidate. Settings control extended candidates, Hashcat modes, John formats, and manual-input history.

History is disabled by default because entered values may be sensitive. If enabled, it is stored at `/ext/apps_data/hashid_fz/history.txt`, capped at 16 KiB, and restarted when the next entry would exceed that bound. Write or synchronization failures are shown in the manual result.

## Batch files

Select **Analyze text/file** and choose a microSD file. Each non-empty text line is processed independently on a worker thread. Progress shows actual processed, identified, and invalid/overlong counts. Press Back to request cancellation.

A completed detailed report is saved transactionally at `/ext/apps_data/hashid_fz/report.txt`. Cancellation, SD failure, and write failure do not promote the partial report.

For `.nfc`, `.rfid`, `.sub`, and `.ibtn` saves, the app considers only value fields that resemble substantial encoded digests. Short UIDs and radio identifiers are excluded. HashID FZ does not read cards, bypass authentication, or crack hashes.

## Raspberry Pi/Linux mode

Follow [EXTERNAL_HASHID.md](EXTERNAL_HASHID.md), put one value per line in `/var/lib/hashid-fz/input.txt`, connect a 3.3 V UART adapter, and open **External HashID**. The Linux companion invokes genuine HashID with fixed arguments and saves its output transactionally. Normal or extended operation follows the Flipper's **Extended candidates** setting.

The Flipper expires a silent companion after five seconds and automatically resumes handshaking. Rejected or overlong protocol lines are displayed as errors. Cancellation requested during companion startup is remembered and applied before identification proceeds.

## Build

```powershell
python tools/generate_db.py
python tests/run_tests.py
python tests/test_companion.py
python -m ufbt
```

The generated database is reproducible from upstream commit `7e8473a823060e56d4b6090a98591e252bd9505e`. See `UPSTREAM_VERSION.md`, `PORTING_ANALYSIS.md`, and `FEATURE_MATRIX.md`.

## License

HashID FZ is licensed under GNU GPL version 3 or later, preserving upstream HashID licensing and attribution. See `LICENSE`.
