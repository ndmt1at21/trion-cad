#!/usr/bin/env python3
"""Báo cáo phủ biểu tượng (UI0-103 tiêu chí 3, 4): lệnh có vị trí trên ribbon/thanh công cụ/menu mà thiếu SVG.

  python3 tools/iconconv/coverage_report.py [--icons app/theme/icons]

Lỗi (thoát 1): lệnh trên ribbon Viewer, và biểu tượng của bộ Viewer (ribbon Viewer, khung, thanh trạng thái) chưa có
SVG — mốc V đang làm. Cảnh báo: vị trí trên ribbon Pro, thanh công cụ, menu cổ điển (mốc sau).
Một SVG phục vụ cả 16 và 32 px, nên thiếu ở cỡ nào cũng là thiếu tệp.
ponytail: nguồn vị trí lệnh tạm là dữ liệu ribbon/menu của prototype (proto_dump.js); phân loại lỗi/cảnh báo theo
mốc của từng lệnh hoãn tới khi có metadata INT-001, khi đó thay load_proto bằng registry lệnh.
"""
import argparse
import sys
from pathlib import Path

import iconconv


def report(proto, have):
    """(lỗi, cảnh báo): danh sách dòng báo cáo; have = tập tên SVG đang có."""
    errors, warnings = [], []
    for p in proto["positions"]:
        if p["icon"] not in have:
            line = "%s: %s thiếu biểu tượng%s" % (p["where"], p["cmd"], " %s.svg" % p["icon"] if p["icon"] else "")
            (errors if p["viewer"] else warnings).append(line)
    errors += ["bộ Viewer: thiếu %s.svg" % n for n in sorted(set(proto["viewerIcons"]) | set(proto["frameIcons"]))
               if n not in have]
    return errors, warnings


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--icons", default=str(iconconv.ICONS_DIR), help="thư mục chứa src/*.svg")
    args = ap.parse_args(argv)
    proto = iconconv.load_proto()
    errors, warnings = report(proto, {p.stem for p in (Path(args.icons) / "src").glob("*.svg")})
    for line in warnings:
        print("cảnh báo: " + line)
    for line in errors:
        print("lỗi: " + line)
    print("coverage: %d vị trí lệnh, %d lỗi, %d cảnh báo" % (len(proto["positions"]), len(errors), len(warnings)))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
