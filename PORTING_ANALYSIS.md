# Porting analysis

Upstream HashID iterates its ordered Python `re.IGNORECASE` prototypes and yields every candidate attached to every matching prototype. Normal mode filters candidates marked `extended`; extended mode includes them. Candidate metadata contains a name plus optional Hashcat and John the Ripper identifiers.

HashID FZ preserves that model. The generator parses all upstream expressions into a compact syntax tree containing literals, negated literals, classes, ranges, anchors, branches, subsequences, and bounded/unbounded repeats. The native matcher evaluates those nodes case-insensitively, in original prototype and candidate order. It does not choose a single “best” answer because many encoded formats are inherently ambiguous.

Python, its regex runtime, CLI argument parser, and desktop file APIs are not compatible with the Cortex-M4 firmware environment. They were replaced with a bounded native matcher, official Storage APIs, and Flipper GUI modules. No signature family was intentionally removed.

Manual input is bounded to 256 bytes by the device editor. File lines are streamed and bounded to 4,096 bytes, covering the largest pinned upstream signatures. Overlong lines are rejected rather than truncated. Batch processing runs on a worker thread and can be cancelled.

Saved NFC/RFID/Sub-GHz files are treated only as text containers. Short identifiers such as UIDs are deliberately excluded from automatic extraction; a value must be at least 32 characters or carry a recognized structured-hash prefix before matching. The application does not read cards, bypass authentication, crack hashes, or claim that identifiers are passwords.

The optional HID1 companion does not reimplement identification. It launches the installed genuine `hashid` executable with fixed argument arrays and `shell=False`. Input and transactional report paths are fixed under `/var/lib/hashid-fz`. The Flipper sends only normal/extended run, status, handshake, and cancellation commands.
