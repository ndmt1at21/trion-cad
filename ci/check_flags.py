#!/usr/bin/env python3
"""Quét compile_commands.json tìm cờ làm hỏng tính tất định số học (05 ADR-004, DoD-C 5).

Dùng: check_flags.py <build>/compile_commands.json   → mã thoát 1 nếu có vi phạm.
"""
import json
import shlex
import sys

FORBIDDEN = ("-ffast-math", "-Ofast", "/fp:fast", "-ffp-contract=fast", "-funsafe-math-optimizations")


def violations(entries):
    for e in entries:
        args = e.get("arguments") or shlex.split(e.get("command", ""))
        bad = [a for a in args if a in FORBIDDEN]
        if bad:
            yield e.get("file", "?"), bad


def main(argv):
    if len(argv) != 2:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    with open(argv[1], encoding="utf-8") as f:
        found = list(violations(json.load(f)))
    for path, bad in found:
        print(f"{path}: cờ cấm {' '.join(bad)}")
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
