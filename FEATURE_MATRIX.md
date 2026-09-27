# Feature matrix

| Capability | HashID FZ | Notes |
|---|---:|---|
| All pinned upstream prototypes | Yes | 145 ordered signatures |
| All pinned candidate records | Yes | 272 records |
| Normal and extended modes | Yes | Extended candidates are explicitly labeled |
| Hashcat metadata | Yes | Optional display |
| John metadata | Yes | Optional display |
| Manual input | Yes | 256-byte editor bound |
| Text-file batch input | Yes | Streaming, 4,096-byte line bound |
| Multiple hashes | Yes | One input per non-empty line |
| History | Yes | Optional manual-input history in app data |
| Transactional report | Yes | Partial output is not promoted after failure/cancellation |
| Progress/cancellation | Yes | Measured processed/identified/invalid counts |
| Saved Flipper data extraction | Bounded | Only digest-like values; short UIDs excluded |
| Cracking or authentication bypass | No | Identification only |
| Exact certainty | No | All matches are presented as candidates |
| Genuine external HashID | Yes | Optional Raspberry Pi/Linux companion over 3.3 V UART |
| External normal/extended modes | Yes | Fixed bounded HID1 commands |
| Linux laptop/desktop/VM | Yes | Raspberry Pi is optional |
