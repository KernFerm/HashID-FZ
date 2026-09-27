# HashID FZ v1.0.0

HashID FZ is a native Flipper Zero port of the genuine psypanda HashID identification database. It preserves all 145 ordered signatures and 272 candidate records from pinned HashID 3.2.0-dev commit `7e8473a823060e56d4b6090a98591e252bd9505e`.

## Features

- Native identification directly on Flipper Zero
- Manual input and streaming multi-line microSD file processing
- Normal and extended candidate modes
- Hashcat mode and John the Ripper format metadata
- Measured progress, cancellation, optional bounded history, and transactional reports
- Conservative inspection of saved Flipper data without treating short NFC/RFID identifiers as password hashes
- Optional genuine upstream HashID mode on Raspberry Pi or another Linux computer through 3.3 V UART
- Bounded HID1 protocol, heartbeat timeout, startup-safe cancellation, and visible protocol errors

HashID results are possible formats, not certainty. The application does not crack hashes or bypass authentication.

## Install

Download `hashid_fz.fap` from Assets and copy it to:

```text
/ext/apps/Tools/hashid_fz.fap
```

The build targets official Flipper firmware 1.4.3, target f7, API 87.1.

## Validation

- Native/database tests: 3/3 passed
- Linux companion tests: 4/4 passed
- Clean f7/API 87.1 build and APPCHK passed
- Authenticated Snyk Code scan: zero issues

Artifact SHA-256:

```text
4FC2219E7546F2377D3BEBC6DD97DABF3D1CDCD5127A3CD9C0F31F7AF08538C7
```

See `README.md` for native usage and `EXTERNAL_HASHID.md` for Raspberry Pi/Linux setup.
