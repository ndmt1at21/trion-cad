import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import check_flags  # noqa: E402


def run(entries):
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
        json.dump(entries, f)
    try:
        return check_flags.main(["check_flags.py", f.name])
    finally:
        os.unlink(f.name)


class CheckFlags(unittest.TestCase):
    def test_every_forbidden_flag_is_caught(self):
        for flag in check_flags.FORBIDDEN:
            with self.subTest(flag=flag):
                self.assertEqual(run([{"file": "a.cpp", "command": f"clang++ -O2 {flag} -c a.cpp"}]), 1)
                self.assertEqual(run([{"file": "a.cpp", "arguments": ["cl", flag, "a.cpp"]}]), 1)

    def test_clean_and_lookalike_flags_pass(self):
        ok = "clang++ -O2 -ffp-contract=off -fno-fast-math -c a.cpp"
        self.assertEqual(run([{"file": "a.cpp", "command": ok}]), 0)


if __name__ == "__main__":
    unittest.main()
