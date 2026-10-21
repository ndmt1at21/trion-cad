#!/usr/bin/env python3
"""Lint DoD-U 4 (UI0-014): cấm QListWidget/QTableWidget/QTreeWidget (kể cả …Item, …ItemIterator) trong mã giao diện
app/. ctest model_no_item_widgets.

Danh sách dữ liệu bản vẽ (layer, block, đối tượng, kết quả tìm…) có thể hàng nghìn dòng: dùng model/view
(trion::ui::BackgroundModel + QListView/QTreeView/QTableView) để ảo hóa và dựng ở nền.
Ngoại lệ (danh sách nhỏ cố định, không phải dữ liệu bản vẽ): `item-widget-ok: <lý do>` trên dòng bị báo.
Bỏ qua chú thích, dòng #include, thư mục tests/ và generated/. Quét cả tệp .ui (Qt Designer).
Dùng: lint.py <thư mục|tệp>...  → in "tệp:dòng: …", mã thoát 1 nếu có vi phạm.
"""
import bisect
import os
import re
import sys

EXTS = (".cpp", ".h", ".hpp", ".cc", ".cxx", ".mm", ".ui")
SKIP_DIRS = {"tests", "generated"}
OK = re.compile(r"item-widget-ok:\s*\S")
WIDGET = re.compile(r"\bQ(?:List|Table|Tree)Widget\w*")
INCLUDE = re.compile(r"^\s*#\s*include\b")
# Chú thích → dấu cách (giữ dòng); chuỗi giữ nguyên (tên lớp trong .ui nằm trong chuỗi XML).
LEXEME = re.compile(r"//[^\n]*|/\*.*?(?:\*/|\Z)|\"(?:\\.|[^\"\\\n])*\"|'(?:\\.|[^'\\\n])*'", re.S)


def strip_comments(src):
    return LEXEME.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)) if m.group(0)[:2] in ("//", "/*") else m.group(0),
                      src)


def lint(path, src):
    code = strip_comments(src)
    lines = src.split("\n")
    starts = [0] + [m.end() for m in re.finditer("\n", src)]
    found = {}  # dòng → tên lớp đầu tiên
    for m in WIDGET.finditer(code):
        found.setdefault(bisect.bisect_right(starts, m.start()), m.group(0))
    return [f"{path}:{ln}: {name}: danh sách dữ liệu dùng model/view (BackgroundModel + QListView/QTreeView); "
            f"ngoại lệ ghi `item-widget-ok: <lý do>`"
            for ln, name in sorted(found.items())
            if not OK.search(lines[ln - 1]) and not INCLUDE.match(lines[ln - 1])]


def files(args):
    for a in args:
        if os.path.isfile(a):
            yield a
        for root, dirs, names in os.walk(a):
            dirs[:] = sorted(d for d in dirs if d not in SKIP_DIRS)
            yield from (os.path.join(root, n) for n in sorted(names) if n.endswith(EXTS))


def main(argv):
    bad = []
    for p in files(argv[1:]):
        with open(p, encoding="utf-8") as f:
            bad += lint(p, f.read())
    print("\n".join(bad) or "no-item-widgets: sạch")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
