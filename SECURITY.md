# Security

HashID FZ identifies string formats; it does not validate passwords, crack hashes, or authenticate to devices.

- Manual input is bounded to 256 bytes and file input to 4,096 bytes per line.
- Empty and overlong inputs are rejected.
- Generated pattern, class, branch, sequence, prototype, and candidate indices use fixed generated bounds.
- Native matching has recursion-depth and operation-step limits.
- File processing streams bytes and never loads a complete input file into RAM.
- Reports use a synchronized temporary file and are promoted only after successful completion.
- Native identification has no UART, shell-command, network, or dynamic-executable dependency.
- External UART accepts a bounded fixed HID1 grammar; the Linux bridge uses `shell=False`, fixed executable locations, and fixed data paths.
- External output is synchronized and atomically promoted only after a successful HashID exit.
- The Flipper rejects malformed or overlong UART replies and expires stale companion state after five seconds.
- External cancellation remains effective during the process-startup window.
- History is disabled by default, capped at 16 KiB when enabled, and reports failed writes or synchronization because entered hashes may be sensitive.
- NFC/RFID identifiers are not automatically characterized as password hashes.

Native mode has no UART, shell, or network dependency. External mode is optional.

Report vulnerabilities privately with the application version, firmware version, reproduction input, and expected upstream behavior. Do not include real credentials or private evidence.
