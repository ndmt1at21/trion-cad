#!/usr/bin/env python3
"""Kiểm thuật ngữ trên các tệp .ts (UI0-005, 07.1 §13.4). ctest i18n_lint_glossary; CI lỗi nếu có vi phạm.

Bảng: app/i18n/glossary.tsv (cột mô tả ở đầu tệp). Với mỗi chuỗi còn dùng (bỏ "vanished"/"obsolete"):
- chuỗi tiếng Việt (nguồn của .ts có sourcelanguage="vi", bản dịch của .ts có language="vi") không được khớp
  cột wrong_vi, vd. "Lớp" thay "Layer", "Độ rộng nét" thay "Độ dày nét";
- bản English (bản dịch của .ts có language="en") phải khớp cột en khi chuỗi nguồn tiếng Việt khớp cột vi,
  vd. nguồn có "Độ dày nét" thì bản en phải có "Lineweight".
So không phân biệt hoa/thường, đã chuẩn hóa NFC, bỏ dấu & phím tắt; trong chuỗi tiếng Việt bỏ trước các từ viết hoa
toàn bộ chữ ASCII (tên lệnh trong tooltip: LAYER, PROPERTIES, OSNAP — DoD-U 3).
Ngoại lệ có lý do: "glossary-ok: <lý do>" trong chú thích cho người dịch (`//:` trước tr() trong mã → <extracomment>)
hoặc chú thích của người dịch trong .ts (<translatorcomment>).
Dùng: glossary.py <glossary.tsv> <tệp .ts>...  → in "tệp: ngữ cảnh: ...", mã thoát 1 nếu có vi phạm.
"""
import csv
import os
import re
import sys
import unicodedata
import xml.etree.ElementTree as ET

COMMAND = re.compile(r"\b[A-Z][A-Z0-9_]+\b")
OK = re.compile(r"glossary-ok:\s*\S")


def nfc(s):
    return unicodedata.normalize("NFC", s)


def word(pattern):
    """Regex của bảng → khớp nguyên từ (ranh giới Unicode hai đầu), không phân biệt hoa/thường."""
    return re.compile(rf"(?<!\w)(?:{nfc(pattern)})(?!\w)", re.I) if pattern else None


def load(path):
    with open(path, encoding="utf-8") as f:
        rows = csv.DictReader((ln for ln in f if not ln.startswith("#")), delimiter="\t", quoting=csv.QUOTE_NONE)
        return [dict(r, vi=word(r["vi"]), wrong_vi=word(r["wrong_vi"]), en=word(r["en"])) for r in rows]


def plain(s):
    return nfc(re.sub(r"&(&?)", r"\1", s or ""))  # "&&" là dấu & thật


def check(rows, ts):
    root = ET.parse(ts).getroot()
    vi_src = root.get("sourcelanguage") == "vi"
    vi_tr, en_tr = root.get("language") == "vi", root.get("language") == "en"
    name, bad = os.path.basename(ts), []
    for ctx in root.iter("context"):
        for m in ctx.iter("message"):
            tr = m.find("translation")
            if tr.get("type") in ("vanished", "obsolete") or any(
                    OK.search(m.findtext(tag) or "") for tag in ("extracomment", "translatorcomment")):
                continue
            src = plain(m.findtext("source"))
            forms = [plain(f.text) for f in tr.iter("numerusform")] or [plain(tr.text)]
            where = f"{name}: {ctx.findtext('name')}: "
            for text in ([src] if vi_src else []) + (forms if vi_tr else []):
                for r in rows:
                    if r["wrong_vi"] and (hit := r["wrong_vi"].search(COMMAND.sub(" ", text))):
                        bad.append(f'{where}"{hit.group(0)}" trong "{text}": {r["term"]} phải là "{r["label"]}"')
            if en_tr and vi_src:
                for r in rows:
                    if r["vi"] and r["en"] and r["vi"].search(src):
                        bad += [f'{where}bản English "{f}" của "{src}": {r["term"]} phải là "{r["term"]}"'
                                for f in forms if f and not r["en"].search(f)]
    return bad


def main(argv):
    if len(argv) < 3:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    rows = load(argv[1])
    bad = [b for ts in argv[2:] for b in check(rows, ts)]
    print("\n".join(bad) or f"thuật ngữ: {len(argv) - 2} tệp .ts đúng bảng ({len(rows)} thuật ngữ)")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
