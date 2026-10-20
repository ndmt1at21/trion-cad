#!/usr/bin/env python3
"""Lint DoD-U 2 (UI0-002): màu, cỡ chữ, khoảng cách viết thẳng trong mã giao diện app/. ctest theme_no_literal_style.

Báo "tệp:dòng: …" (mã thoát 1) khi gặp, ngoài chú thích:
  màu        #RGB/#RRGGBB/#AARRGGBB, rgb()/rgba()/hsl() với số, qRgb(số), QColor hằng (QColor(số | "tên" | Qt::…),
             QColor::fromRgb(số)…, Qt::red…, QColorConstants::…), tên màu trong thuộc tính QSS (color: red).
  cỡ chữ     setPointSize(F)/setPixelSize(số), QFont("…", số), font-size:/font: số+đơn vị trong QSS.
  khoảng cách setSpacing/setHorizontalSpacing/setVerticalSpacing/addSpacing(số ≠ 0), setContentsMargins/QMargins có
             số ≠ 0, padding/margin/spacing: số ≠ 0 kèm đơn vị trong QSS.
Thay bằng token: ThemeManager::instance().color(tok::c_accent), .px(tok::sp_4), .font(tok::fs_s); QSS mẫu: var(--token).
Bỏ qua thư mục tests/ (giá trị mong đợi của test) và generated/. Ngoại lệ có lý do: `style-ok: <lý do>` trên dòng.
Dùng: lint.py <thư mục|tệp>...
"""
import bisect
import os
import re
import sys

EXTS = (".cpp", ".h", ".hpp", ".cc", ".cxx", ".mm", ".qss", ".in")
SKIP_DIRS = {"tests", "generated"}
OK = re.compile(r"style-ok:\s*\S")

# Chú thích → dấu cách (giữ dòng); chuỗi giữ nguyên vì QSS nằm trong chuỗi. ponytail: không hiểu raw string chứa
# dấu ", chữ số có dấu tách 1'000; gặp thì có thể bỏ sót, không báo nhầm.
LEXEME = re.compile(r"//[^\n]*|/\*.*?(?:\*/|\Z)|\"(?:\\.|[^\"\\\n])*\"|'(?:\\.|[^'\\\n])*'", re.S)

LEN = r"(?<![\w.#-])(?:0*[1-9]\d*(?:\.\d*)?|0*\.\d*[1-9]\d*)\s*(?:px|pt|em|ex)\b"  # độ dài ≠ 0 có đơn vị
NAMED = ("white|black|red|darkred|green|darkgreen|blue|darkblue|cyan|magenta|yellow|orange|purple|pink|brown|gray|grey|"
         "darkgray|darkgrey|lightgray|lightgrey|silver|navy|maroon|lime|olive|teal|aqua|fuchsia")
COLOR_MSG = "màu viết thẳng: dùng token (ThemeManager::color(tok::c_…), var(--c-…) trong QSS mẫu)"
RULES = [(re.compile(p, f), m) for p, f, m in [
    (r"(?<![\w&\])])#(?:[0-9A-Fa-f]{8}|[0-9A-Fa-f]{6}|[0-9A-Fa-f]{3})\b", 0, COLOR_MSG),
    (r"(?<![\w.>:])(?:rgba?|hsla?|hsva?)\s*\(\s*[-+.\d]", 0, COLOR_MSG),
    (r"\bqRgba?\s*\(\s*[-+.\d]", 0, COLOR_MSG),
    (r"\bQColor\s*(?:[A-Za-z_]\w*\s*)?[({]\s*(?:[-+.\d]|\"|u\"|L\"|QString|QLatin1|Qt\s*::)", 0, COLOR_MSG),
    (r"\bQColor\s*::\s*from(?:Rgb|Hsv|Hsl|Cmyk|String)\w*\s*\(\s*(?:[-+.\d]|\"|u\")", 0, COLOR_MSG),
    (r"\bQt\s*::\s*(?:white|black|red|darkRed|green|darkGreen|blue|darkBlue|cyan|darkCyan|magenta|darkMagenta|yellow|"
     r"darkYellow|gray|darkGray|lightGray|color0|color1)\b", 0, COLOR_MSG),
    (r"\bQColorConstants\s*::", 0, COLOR_MSG),
    (r"(?<![\w-])(?:(?:alternate-|selection-)?background(?:-color)?|(?:selection-)?color|gridline-color|"
     r"border(?:-top|-right|-bottom|-left)?(?:-color)?)\s*:[^;{}\"]*?\b(?:" + NAMED + r")\b", re.I, COLOR_MSG),
    (r"\bset(?:PointSizeF?|PixelSize)\s*\(\s*[-+.\d]", 0, "cỡ chữ viết thẳng: dùng ThemeManager::font(tok::fs_…)"),
    (r"\bQFont\s*(?:[A-Za-z_]\w*\s*)?[({][^;(){}]*,\s*[-+.\d]", 0, "cỡ chữ viết thẳng: dùng ThemeManager::font(tok::fs_…)"),
    (r"(?<![\w-])font(?:-size)?\s*:[^;{}\"]*?" + LEN, 0, "cỡ chữ viết thẳng trong QSS: dùng var(--fs-…)"),
    (r"\b(?:setSpacing|setHorizontalSpacing|setVerticalSpacing|addSpacing)\s*\(\s*(?:0*[1-9]|0*\.\d*[1-9])", 0,
     "khoảng cách viết thẳng: dùng ThemeManager::px(tok::sp_…)"),
    (r"(?<![\w-])(?:padding|margin)(?:-top|-right|-bottom|-left)?\s*:[^;{}\"]*?" + LEN, 0,
     "khoảng cách viết thẳng trong QSS: dùng var(--sp-…)"),
    (r"(?<![\w-])spacing\s*:[^;{}\"]*?" + LEN, 0, "khoảng cách viết thẳng trong QSS: dùng var(--sp-…)"),
]]
MARGINS = re.compile(r"\b(?:setContentsMargins|QMarginsF?)\s*(?:[A-Za-z_]\w*\s*)?[({]([^;(){}]*)[)}]")
NUMBER = re.compile(r"[-+]?(?:\d+\.?\d*|\.\d+)[fF]?")


def strip_comments(src):
    return LEXEME.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)) if m.group(0)[:2] in ("//", "/*") else m.group(0),
                      src)


def lint(path, src):
    code = strip_comments(src)
    lines = src.split("\n")
    starts = [0] + [m.end() for m in re.finditer("\n", src)]
    found = {}  # dòng → lỗi đầu tiên
    hits = [(m.start(), msg) for rx, msg in RULES for m in rx.finditer(code)]
    for m in MARGINS.finditer(code):
        if any(NUMBER.fullmatch(a.strip()) and float(a.strip().rstrip("fF")) != 0 for a in m.group(1).split(",")):
            hits.append((m.start(), "khoảng cách viết thẳng: dùng ThemeManager::px(tok::sp_…)"))
    for pos, msg in sorted(hits):
        found.setdefault(bisect.bisect_right(starts, pos), msg)
    return [f"{path}:{ln}: {msg}" for ln, msg in sorted(found.items()) if not OK.search(lines[ln - 1])]


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
    print("\n".join(bad) or "no-literal-style: sạch")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
