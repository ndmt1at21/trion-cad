#!/usr/bin/env python3
"""Lint quy cách SVG biểu tượng (UI0-103 tiêu chí 1, 07.1 §13.5).

  python3 tools/iconlint/iconlint.py <tệp.svg | thư mục> ...

In "tệp: lỗi" cho từng vi phạm, thoát 1 nếu có lỗi. Quy cách:
- gốc <svg> có đúng các thuộc tính ROOT: viewBox 0 0 16 16 (không width/height), nét currentColor 1,25 đầu vuông nối miter;
- con trực tiếp chỉ là path/circle/rect, không lồng, chỉ thuộc tính hình học của thẻ và thuộc tính nét/tô trong PAINT
  (không class, style, transform, opacity…);
- fill/stroke chỉ currentColor hoặc none: không màu viết thẳng, để bộ biểu tượng lúc chạy tô lại theo chủ đề;
- mọi nét đầu vuông (phần tử không được đổi stroke-linecap/linejoin); phần tử được đặt độ dày nét riêng (bảng độ dày
  nét lw, viền đậm) và nét đứt stroke-dasharray. Đường chấm của prototype đã thành chấm tròn tô đặc (iconconv).
"""
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

NS = "{http://www.w3.org/2000/svg}"
ROOT = {"viewBox": "0 0 16 16", "fill": "none", "stroke": "currentColor", "stroke-width": "1.25",
        "stroke-linecap": "square", "stroke-linejoin": "miter"}
GEOMETRY = {"path": {"d"}, "circle": {"cx", "cy", "r"}, "rect": {"x", "y", "width", "height"}}
PAINT = {"fill", "stroke", "stroke-width", "stroke-dasharray"}
COLORS = {"currentColor", "none"}
NUMBER = re.compile(r"^\d*\.?\d+$")


def lint(path):
    """Danh sách vi phạm của một tệp SVG (rỗng = đạt)."""
    try:
        root = ET.parse(str(path)).getroot()
    except (ET.ParseError, OSError) as e:
        return ["không đọc được: %s" % e]
    if root.tag != NS + "svg":
        return ["gốc phải là <svg> với xmlns SVG, gặp <%s>" % root.tag]
    errs = ['gốc: %s phải là "%s"' % (k, v) for k, v in ROOT.items() if root.get(k) != v]
    errs += ["gốc: thuộc tính không cho phép %s" % k for k in root.attrib if k not in ROOT]
    for i, el in enumerate(root, 1):
        tag = el.tag[len(NS):] if el.tag.startswith(NS) else el.tag
        where = "phần tử %d <%s>" % (i, tag)
        if tag not in GEOMETRY:
            errs.append("%s: thẻ không cho phép (chỉ path/circle/rect)" % where)
            continue
        if len(el):
            errs.append("%s: không được có phần tử con" % where)
        errs += ["%s: thiếu %s" % (where, k) for k in sorted(GEOMETRY[tag] - el.attrib.keys())]
        errs += ["%s: thuộc tính không cho phép %s" % (where, k) for k in el.keys() if k not in GEOMETRY[tag] | PAINT]
        errs += ['%s: %s="%s" là màu viết thẳng, chỉ dùng currentColor hoặc none' % (where, k, el.get(k))
                 for k in ("fill", "stroke") if el.get(k, "none") not in COLORS]
        width, dash = el.get("stroke-width"), el.get("stroke-dasharray")
        if width is not None and (not NUMBER.match(width) or float(width) == 0):
            errs.append('%s: stroke-width="%s" phải là số dương' % (where, width))
        if dash is not None and not all(NUMBER.match(x) for x in dash.replace(",", " ").split()):
            errs.append('%s: stroke-dasharray="%s" phải là dãy số' % (where, dash))
    return errs


def main(argv):
    files = []
    for arg in argv:
        p = Path(arg)
        files += sorted(p.glob("*.svg")) if p.is_dir() else [p]
    if not files:
        print("iconlint: không có tệp SVG nào", file=sys.stderr)
        return 1
    n = 0
    for f in files:
        for e in lint(f):
            print("%s: %s" % (f, e))
            n += 1
    print("iconlint: %d tệp, %d lỗi" % (len(files), n))
    return 1 if n else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
