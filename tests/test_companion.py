#!/usr/bin/env python3
import importlib.util
import pathlib
import sys
import tempfile
import threading
import unittest
from unittest import mock

MODULE = pathlib.Path(__file__).parents[1] / "companion" / "hashid_fz_bridge.py"
SPEC = importlib.util.spec_from_file_location("hashid_bridge", MODULE)
bridge = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
sys.modules[SPEC.name] = bridge
SPEC.loader.exec_module(bridge)


class CompanionTests(unittest.TestCase):
    def test_tokens_and_measurement(self):
        self.assertEqual(bridge.safe_token("../../bad name;id"), ".._.._bad_name_id")
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "input.txt"
            path.write_text("abc\n\n  \ndef\n", encoding="utf-8")
            self.assertEqual(bridge.measure_input(path), (2, len(path.read_bytes())))

    def test_fixed_command_grammar(self):
        instance = object.__new__(bridge.Bridge)
        messages = []
        instance.send = messages.append
        instance.send_info = lambda: messages.append("info")
        instance.send_status = lambda: messages.append("status")
        instance.start_operation = messages.append
        instance.cancel = lambda: messages.append("cancel")
        instance.handle("HID1 RUN EXTENDED")
        instance.handle("HID1 RUN IDENTIFY;id")
        self.assertEqual(messages, ["EXTENDED", "HID1 ERROR invalid_command"])

    def test_failed_run_preserves_report(self):
        class FailedProcess:
            pid = 123
            def wait(self, timeout=None): return 2

        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory); input_path = root / "input.txt"; output = root / "output"; report = output / "hashid-report.txt"
            input_path.write_text("098f6bcd4621d373cade4e832627b4f6\n", encoding="utf-8")
            output.mkdir(); report.write_text("old valid report", encoding="utf-8")
            instance = object.__new__(bridge.Bridge); instance.executable = "/usr/bin/hashid"; instance.state = bridge.State(); instance.lock = threading.Lock(); instance.process = None; instance.cancel_requested = False
            instance.send_status = lambda: None; instance.set_error = lambda message: self.fail(message)
            with mock.patch.multiple(bridge, ROOT=root, INPUT=input_path, OUTPUT=output, REPORT=report), mock.patch.object(bridge.subprocess, "Popen", return_value=FailedProcess()):
                instance.run_operation("IDENTIFY")
            self.assertEqual(report.read_text(encoding="utf-8"), "old valid report")
            self.assertEqual(instance.state.name, "FAILED")

    def test_cancel_during_starting_is_remembered(self):
        class RunningThread:
            @staticmethod
            def is_alive(): return True

        instance = object.__new__(bridge.Bridge)
        instance.lock = threading.RLock(); instance.state = bridge.State(name="STARTING")
        instance.process = None; instance.thread = RunningThread(); instance.cancel_requested = False
        instance.send_status = lambda: None; instance.send = lambda line: self.fail(line)
        instance.cancel()
        self.assertTrue(instance.cancel_requested)
        self.assertEqual(instance.state.name, "STOPPING")


if __name__ == "__main__":
    unittest.main()
