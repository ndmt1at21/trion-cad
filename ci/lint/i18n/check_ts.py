#!/usr/bin/env python3
"""ctest i18n_ts (UI0-104): các .ts có bản dịch cho mọi chuỗi của nguồn; bản phát hành lỗi nếu thiếu.

Chạy lupdate (như target update_translations) trên bản sao các .ts, rồi với mỗi chuỗi nguồn đang dùng:
- .ts gốc phải có nó và không ở trạng thái "vanished" (nếu không: chạy update_translations);
- đã dịch đủ mọi dạng số nhiều; placeholder %1 %L1 %n và khoảng trắng hai đầu khớp chuỗi nguồn.
Cảnh báo của lupdate (lớp gọi tr() thiếu Q_OBJECT…) cũng là lỗi.
Tệp có language = sourcelanguage (trion_vi.ts) là tệp ngôn ngữ nguồn: chỉ chứa dạng số nhiều (-pluralonly).
Theo ~/snapone/ui/i18n/check.py (cùng tác giả).
Dùng: check_ts.py <lupdate> <nguồn|@danh sách>... -ts <tệp .ts>...  → mã thoát 1 kèm danh sách chuỗi cần dịch.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def marks(s):
    return sorted(re.findall(r"%L?(?:\d+|n)", s))


def edges(s):
    return s[:1].isspace(), s[-1:].isspace()


def messages(ts):
    """{(ngữ cảnh, nguồn, chú thích): (message, còn dùng?)}"""
    out = {}
    for ctx in ET.parse(ts).getroot().iter("context"):
        for m in ctx.iter("message"):
            live = m.find("translation").get("type") not in ("vanished", "obsolete")
            out[(ctx.findtext("name"), m.findtext("source"), m.findtext("comment") or "")] = (m, live)
    return out


def problems(lupdate, sources, ts_files):
    bad = []
    with tempfile.TemporaryDirectory() as tmp:
        for ts in ts_files:
            root = ET.parse(ts).getroot()
            copy = shutil.copy(ts, tmp)
            plural = ["-pluralonly"] if root.get("language") == root.get("sourcelanguage") else []
            r = subprocess.run([lupdate, "-silent", "-locations", "none", *plural, *sources, "-ts", copy],
                               capture_output=True, encoding="utf-8", errors="replace")
            # -silent: còn in ra là cảnh báo, vd. lớp gọi tr() thiếu Q_OBJECT thì lúc chạy tra sai ngữ cảnh
            bad += [f"lupdate: {ln}" for ln in (r.stdout + r.stderr).splitlines() if ln.strip()
                    and f"lupdate: {ln}" not in bad]
            if r.returncode:
                bad.append(f"lupdate thoát mã {r.returncode} với {os.path.basename(ts)}")
                continue
            old = messages(ts)
            for key, (m, live) in messages(copy).items():
                if not live:
                    continue
                tr, src = m.find("translation"), key[1]
                forms = [f.text or "" for f in tr.iter("numerusform")] if m.get("numerus") == "yes" else [tr.text or ""]
                why = ("chưa có trong .ts, chạy target update_translations" if not old.get(key, (0, False))[1]
                       else "chưa dịch" if tr.get("type") == "unfinished" or not all(forms)
                       else "placeholder khác chuỗi nguồn" if any(marks(f) != marks(src) for f in forms)
                       else "khoảng trắng hai đầu khác chuỗi nguồn" if any(edges(f) != edges(src) for f in forms)
                       else "")
                if why:
                    bad.append(f"{os.path.basename(ts)}: {key[0]}: {why}: {src!r}")
    return bad


def main(argv):
    if "-ts" not in argv[2:]:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    k = argv.index("-ts", 2)
    ts_files = argv[k + 1:]
    bad = problems(argv[1], argv[2:k], ts_files)
    print("\n".join(bad) or f"{len(ts_files)} tệp .ts đủ bản dịch")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
