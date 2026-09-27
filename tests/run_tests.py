#!/usr/bin/env python3
from __future__ import annotations
import importlib.util
import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).parents[1]
sys.path.insert(0, str(ROOT / "upstream-hashid"))
import hashid  # type: ignore


class CompatibilityTests(unittest.TestCase):
    def test_upstream_examples_and_families(self):
        cases = {
            "098f6bcd4621d373cade4e832627b4f6": ["MD5", "MD4", "Double MD5", "LM"],
            "a9993e364706816aba3e25717850c26c9cd0d89d": ["SHA-1", "Double SHA-1"],
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad": ["SHA-256", "Haval-256"],
            "$P$8ohUJ.1sdFw09/bMaAQPTGDNi2BIUt1": ["Wordpress >= v2.6.2", "Joomla >= v2.5.18"],
            "$racf$*AAAAAAAA*3c44ee7f409c9a9b": ["RACF"],
            "$2y$10$92IXUNpkjO0rOQ5byMi.Ye4oKoEa3Ro9llC/.og/at2uheWG/igi.": ["Blowfish(OpenBSD)", "bcrypt"],
            "{SHA}qUqP5cyxm6YcTAhz05Hph5gvu9M=": ["SHA-1(Base64)", "Netscape LDAP SHA"],
        }
        for value, required in cases.items():
            names = [mode.name.replace("≥", ">=").replace("≤", "<=") for mode in hashid.HashID().identifyHash(value) if not mode.extended]
            with self.subTest(value=value):
                for name in required:
                    self.assertIn(name, names)
        self.assertEqual(list(hashid.HashID().identifyHash("not-a-hash")), [])

    def test_database_counts_and_generation(self):
        source = (ROOT / "hashid_db.c").read_text(encoding="utf-8")
        self.assertIn("hid_prototype_count = 145U", source)
        self.assertIn("hid_candidate_count = 272U", source)
        before = source
        import subprocess
        subprocess.run([sys.executable, str(ROOT / "tools" / "generate_db.py")], check=True)
        self.assertEqual((ROOT / "hashid_db.c").read_text(encoding="utf-8"), before)

    def test_repository_hygiene(self):
        ignore = (ROOT / ".gitignore").read_text(encoding="utf-8")
        self.assertIn("/to-do.md", ignore)
        for path in list(ROOT.glob("*.c")) + list(ROOT.glob("*.h")):
            text = path.read_text(encoding="utf-8").lower()
            self.assertNotIn("todo", text, path.name)
            self.assertNotIn("placeholder", text, path.name)


if __name__ == "__main__":
    unittest.main(verbosity=2)
