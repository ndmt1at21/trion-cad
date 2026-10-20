#!/usr/bin/env python3
"""Chụp ảnh tham chiếu biểu tượng bằng trình duyệt thật (UI0-103 tiêu chí 2). Chạy tay mỗi khi prototype đổi, không
chạy trong CI. Trang lưới dùng nguyên tokens.css và các quy tắc .ic của app.css; markup nhúng thẳng vào <svg class="ic">
để quy tắc lớp (.dot, .f, .dash, .dots) áp như thiết kế (qua <use> thì CSS của trang không chạm tới, xem cuối icons.js).

  python3 tools/iconconv/capture_ref.py --browser <chrome|msedge> [--tmp <thư mục trình duyệt ghi được>]

Từ WSL: --browser "/mnt/c/Program Files (x86)/Microsoft/Edge/Application/msedge.exe"
        --tmp /mnt/c/Users/<tên>/AppData/Local/Temp/trion-iconref  (đường dẫn đổi sang Windows bằng wslpath -w)
Ghi app/theme/icons/ref/icons16.png, icons32.png (nền trong suốt, nét đen #000) và ref/layout.json.
"""
import argparse
import json
import math
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import iconconv

COLUMNS = 18
# (cỡ px, lớp CSS, độ dày nét gốc tương ứng trong SVG): .ic.lg = 32 px, nét calc(var(--sw) * .62) (app.css).
SHEETS = ((16, "ic", "1.25"), (32, "ic lg", "0.775"))
GAP = 8  # khoảng trống quanh ô: .ic có overflow:visible, nét tràn khung không lấn sang ô bên


def page(icons, css, size, cls):
    pitch = size + GAP
    cells = "".join('<svg class="%s" viewBox="0 0 16 16" style="left:%dpx;top:%dpx">%s</svg>'
                    % (cls, (i % COLUMNS) * pitch + GAP // 2, (i // COLUMNS) * pitch + GAP // 2, m)
                    for i, m in enumerate(icons.values()))
    return ('<!doctype html><html><head><meta charset="utf-8"><style>%s\nhtml,body{margin:0;background:transparent;'
            'color:#000}svg.ic{position:absolute}</style></head><body>%s</body></html>' % (css, cells))


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--browser", required=True, help="chrome/msedge (Chromium ≥ 112 cho --headless=new)")
    ap.add_argument("--tmp", help="thư mục tạm trình duyệt đọc/ghi được (mặc định: thư mục tạm hệ thống)")
    args = ap.parse_args(argv)
    proto_css = iconconv.ROOT / "design/prototype/css"
    css = (proto_css / "tokens.css").read_text(encoding="utf-8") + "\n" + "\n".join(
        l for l in (proto_css / "app.css").read_text(encoding="utf-8").splitlines() if l.startswith(".ic"))
    icons = iconconv.load_proto()["icons"]
    tmp = Path(args.tmp or tempfile.mkdtemp())
    tmp.mkdir(parents=True, exist_ok=True)
    win = args.browser.endswith(".exe") and shutil.which("wslpath")  # trình duyệt Windows chạy từ WSL
    native = lambda p: subprocess.check_output(["wslpath", "-w", str(p)], text=True).strip() if win else str(p)
    ref = iconconv.ICONS_DIR / "ref"
    ref.mkdir(exist_ok=True)
    rows = math.ceil(len(icons) / COLUMNS)
    sheets = []
    for size, cls, sw in SHEETS:
        pitch = size + GAP
        html, shot = tmp / ("icons%d.html" % size), tmp / ("icons%d.png" % size)
        html.write_text(page(icons, css, size, cls), encoding="utf-8")
        if shot.exists():
            shot.unlink()
        subprocess.run([args.browser, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--force-color-profile=srgb",
                        "--force-device-scale-factor=1", "--default-background-color=00000000",
                        "--window-size=%d,%d" % (COLUMNS * pitch, rows * pitch), "--screenshot=" + native(shot),
                        "file:///" + native(html).replace("\\", "/").lstrip("/")], check=True, timeout=120,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        shutil.copyfile(shot, ref / shot.name)
        sheets.append({"size": size, "file": shot.name, "pitch": pitch, "offset": GAP // 2, "rootStrokeWidth": sw})
    layout = {"columns": COLUMNS, "sheets": sheets, "names": list(icons)}
    (ref / "layout.json").write_bytes((json.dumps(layout, ensure_ascii=False, indent=1) + "\n").encode("utf-8"))
    print("capture_ref: %d biểu tượng × %d cỡ → %s" % (len(icons), len(SHEETS), ref))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
