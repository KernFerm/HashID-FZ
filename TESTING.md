# Testing

Run from the project root:

```powershell
python tests/run_tests.py
python tests/test_companion.py
python -m ufbt
```

The host suite checks upstream examples and representative MD5, SHA-1, SHA-256, phpass, RACF, bcrypt, and LDAP SHA inputs; unknown input; exact database counts; deterministic regeneration; source hygiene; and exclusion of `to-do.md`. The firmware build compiles the native matcher and validates the FAP against target f7/API 87.1.

Device checklist:

1. Confirm About shows version 1.0.0, 145 signatures, and 272 candidates.
2. Enter `098f6bcd4621d373cade4e832627b4f6`; confirm several candidates appear and no certainty claim is made.
3. Enable Extended candidates and confirm additional candidates appear for the same value.
4. Confirm Hashcat and John metadata can each be hidden and shown.
5. Analyze a multi-line UTF-8 text file containing known, unknown, blank, and malformed lines.
6. Cancel a large batch and confirm no completed report replaces the previous report.
7. Test an over-4,096-byte line and an SD read/write failure.
8. Inspect an NFC/RFID save containing a short UID and confirm it is not automatically submitted as a password hash.
9. Repeat batch operations and verify resources are released.
10. With a Linux companion, verify HID1 handshake, normal/extended runs, cancellation, and preservation of an old report after failure.

## Current automated evidence

- Version: 1.0.0
- Upstream compatibility/regeneration tests: 3/3 passed
- Linux companion protocol/transaction tests: 4/4 passed
- Clean official firmware 1.4.3 build: passed
- APPCHK: target f7, API 87.1 passed
- Snyk Code: zero issues at low-or-higher severity
- Artifact: `dist/hashid_fz.fap`, 64,080 bytes
- SHA-256: `4FC2219E7546F2377D3BEBC6DD97DABF3D1CDCD5127A3CD9C0F31F7AF08538C7`
- Raspberry Pi/Linux hardware and physical-device checklist: pending
