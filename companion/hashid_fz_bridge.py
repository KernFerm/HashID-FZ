#!/usr/bin/env python3
"""Bounded UART bridge to the genuine upstream HashID executable."""
from __future__ import annotations
import argparse
import os
import pathlib
import signal
import subprocess
import sys
import threading
from dataclasses import dataclass
import serial

PROTOCOL = 1
BRIDGE_VERSION = "1.0.0"
ROOT = pathlib.Path("/var/lib/hashid-fz")
INPUT = ROOT / "input.txt"
OUTPUT = ROOT / "output"
REPORT = OUTPUT / "hashid-report.txt"
EXECUTABLES = ("/usr/bin/hashid", "/usr/local/bin/hashid")


@dataclass
class State:
    name: str = "IDLE"
    processed: int = 0
    bytes: int = 0
    identified: int = 0
    unknown: int = 0
    invalid: int = 0
    exit_code: int = 0
    target: str = "none"
    error: str = ""


def safe_token(value: str, limit: int = 63) -> str:
    cleaned = "".join(c if c.isalnum() or c in "._-" else "_" for c in value)
    return (cleaned or "none")[:limit]


def find_hashid() -> str:
    for candidate in EXECUTABLES:
        if pathlib.Path(candidate).is_file() and os.access(candidate, os.X_OK):
            return candidate
    raise FileNotFoundError("install_hashid_in_usr_bin_or_usr_local_bin")


def version_of(executable: str) -> str:
    result = subprocess.run([executable, "--version"], capture_output=True, text=True, timeout=5, check=False)
    lines = (result.stdout or result.stderr).splitlines()
    return safe_token(lines[0] if lines else "unknown", 31)


def measure_input(path: pathlib.Path) -> tuple[int, int]:
    size = path.stat().st_size
    processed = 0
    with path.open("r", encoding="utf-8", errors="strict") as source:
        for line in source:
            if line.strip():
                processed += 1
    return processed, size


class Bridge:
    def __init__(self, port: str, baud: int, executable: str) -> None:
        self.executable = executable
        self.version = version_of(executable)
        self.state = State()
        self.lock = threading.RLock()
        self.serial_lock = threading.Lock()
        self.process: subprocess.Popen[str] | None = None
        self.thread: threading.Thread | None = None
        self.cancel_requested = False
        self.stop = False
        self.serial = serial.Serial(port, baud, timeout=0.25, write_timeout=1)

    def send(self, line: str) -> bool:
        try:
            with self.serial_lock:
                self.serial.write((line.rstrip("\r\n") + "\n").encode("ascii", "strict"))
                self.serial.flush()
            return True
        except (OSError, serial.SerialException):
            with self.lock:
                self.state.name = "ERROR"
                self.state.error = "serial_io_failed"
            return False

    def send_info(self) -> None:
        self.send(f"HID1 INFO {PROTOCOL} {self.version}")

    def send_status(self) -> None:
        with self.lock:
            s = self.state
            self.send(
                f"HID1 STATUS {s.name} {s.processed} {s.bytes} {s.identified} "
                f"{s.unknown} {s.invalid} 0 {s.exit_code} {safe_token(s.target)}"
            )

    def set_error(self, message: str) -> None:
        with self.lock:
            self.state.name = "ERROR"
            self.state.error = safe_token(message)
            self.process = None
        self.send(f"HID1 ERROR {safe_token(message)}")

    def run_operation(self, operation: str) -> None:
        temporary = REPORT.with_suffix(".txt.partial")
        promoted = False
        try:
            if not INPUT.is_file():
                raise FileNotFoundError("input.txt_missing")
            OUTPUT.mkdir(parents=True, exist_ok=True)
            processed, size = measure_input(INPUT)
            args = [self.executable, "-m", "-j"]
            if operation == "EXTENDED":
                args.append("-e")
            args.append(str(INPUT))
            temporary.unlink(missing_ok=True)
            with self.lock:
                self.state = State(name="STARTING", processed=processed, bytes=size, target=REPORT.name)
                if self.cancel_requested:
                    self.state.name = "CANCELLED"
                    self.send_status()
                    return
            self.send_status()
            with temporary.open("w", encoding="utf-8", newline="\n") as output:
                process = subprocess.Popen(
                    args, stdin=subprocess.DEVNULL, stdout=output, stderr=subprocess.STDOUT,
                    text=True, shell=False, cwd=str(ROOT), start_new_session=True)
                with self.lock:
                    self.process = process
                    cancel_pending = self.cancel_requested
                    self.state.name = "STOPPING" if cancel_pending else "RUNNING"
                self.send_status()
                if cancel_pending:
                    try:
                        os.killpg(process.pid, signal.SIGTERM)
                    except OSError:
                        pass
                return_code = process.wait()
                output.flush()
                os.fsync(output.fileno())
            with self.lock:
                cancelled = self.cancel_requested or self.state.name == "STOPPING"
                self.process = None
                self.state.exit_code = return_code
            if not cancelled and return_code == 0:
                text = temporary.read_text(encoding="utf-8", errors="strict")
                unknown = text.count("[+] Unknown hash")
                analyzed = text.count("Analyzing '")
                os.replace(temporary, REPORT)
                promoted = True
                directory = os.open(REPORT.parent, os.O_RDONLY | getattr(os, "O_DIRECTORY", 0))
                try:
                    os.fsync(directory)
                finally:
                    os.close(directory)
                with self.lock:
                    self.state.processed = analyzed
                    self.state.unknown = unknown
                    self.state.identified = max(0, analyzed - unknown)
            with self.lock:
                self.state.name = "CANCELLED" if cancelled else ("COMPLETE" if promoted else "FAILED")
            temporary.unlink(missing_ok=True)
            self.send_status()
        except (OSError, UnicodeError, ValueError, subprocess.SubprocessError) as error:
            if not promoted:
                try:
                    temporary.unlink(missing_ok=True)
                except OSError:
                    pass
            self.set_error(str(error))

    def start_operation(self, operation: str) -> None:
        with self.lock:
            if self.process is not None or (self.thread and self.thread.is_alive()):
                self.send("HID1 ERROR busy")
                return
            self.cancel_requested = False
            self.thread = threading.Thread(target=self.run_operation, args=(operation,), daemon=True)
            self.thread.start()

    def cancel(self) -> None:
        with self.lock:
            process = self.process
            thread_running = self.thread is not None and self.thread.is_alive()
            if process is None:
                if thread_running:
                    self.cancel_requested = True
                    self.state.name = "STOPPING"
                else:
                    self.send("HID1 ERROR not_running")
                    return
            else:
                self.cancel_requested = True
                self.state.name = "STOPPING"
        self.send_status()
        if process is None:
            return
        try:
            os.killpg(process.pid, signal.SIGTERM)
            process.wait(timeout=5)
        except (OSError, subprocess.TimeoutExpired):
            try:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait(timeout=5)
            except (OSError, subprocess.TimeoutExpired):
                self.set_error("process_would_not_stop")

    def handle(self, line: str) -> None:
        parts = line.strip().split()
        if parts == ["HID1", "HELLO"]:
            self.send_info(); self.send_status()
        elif parts == ["HID1", "STATUS"]:
            self.send_status()
        elif len(parts) == 3 and parts[:2] == ["HID1", "RUN"] and parts[2] in {"IDENTIFY", "EXTENDED"}:
            self.start_operation(parts[2])
        elif parts == ["HID1", "CANCEL"]:
            self.cancel()
        else:
            self.send("HID1 ERROR invalid_command")

    def serve(self) -> None:
        self.send_info()
        while not self.stop:
            raw = self.serial.readline(257)
            if len(raw) > 256:
                self.send("HID1 ERROR line_too_long"); self.serial.reset_input_buffer(); continue
            if raw:
                try:
                    self.handle(raw.decode("ascii", "strict"))
                except UnicodeDecodeError:
                    self.send("HID1 ERROR non_ascii_command")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="UART device such as /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, choices=(115200, 230400, 460800), default=115200)
    args = parser.parse_args()
    bridge = None
    try:
        bridge = Bridge(args.port, args.baud, find_hashid())
        bridge.serve()
    except KeyboardInterrupt:
        if bridge and bridge.process:
            bridge.cancel()
    except (OSError, UnicodeError, subprocess.SubprocessError, serial.SerialException) as error:
        print(f"HashID FZ bridge error: {safe_token(str(error), 160)}", file=sys.stderr)
        return 1
    finally:
        if bridge:
            try:
                bridge.serial.close()
            except (OSError, serial.SerialException):
                pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
