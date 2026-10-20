#!/usr/bin/env python3
"""Biểu tượng prototype (design/prototype/js/icons.js) → app/theme/icons/src/<tên>.svg + manifest.json (UI0-103).

Chạy lại mỗi khi prototype đổi; kết quả tất định (cùng đầu vào cho cùng từng byte), SVG thừa bị xóa.
  python3 tools/iconconv/iconconv.py [--proto design/prototype/js] [--out app/theme/icons]
Cần node để đọc dữ liệu JS của prototype (proto_dump.js).
"""
import argparse
import html
import json
import re
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROTO_JS = ROOT / "design/prototype/js"
ICONS_DIR = ROOT / "app/theme/icons"

# Gốc mọi SVG: nét theo token --sw/--cap/--join của prototype (1,25, vuông, miter); màu duy nhất là currentColor.
SVG_OPEN = ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" fill="none" stroke="currentColor" '
            'stroke-width="1.25" stroke-linecap="square" stroke-linejoin="miter">')
# Lớp CSS của prototype (app.css, quy tắc .ic …) → thuộc tính. Lớp đè thuộc tính trình bày, style đè lớp (như CSS).
# .f tô bằng --c-icon-fill, hiện là none ở cả hai chủ đề: phần tử .f vẫn có nét kế thừa, đúng như trình duyệt vẽ.
# ponytail: khi --c-icon-fill có giá trị thật, đổi .f thành fill="currentColor" fill-opacity="…" ở đây.
CLASS_ATTRS = {
    "f": {"fill": "none"},
    "dot": {"fill": "currentColor", "stroke": "none"},
    "dash": {"stroke-dasharray": "1.6 1.7"},
    "dots": {"stroke-dasharray": "0 2.6", "stroke-linecap": "round", "stroke-width": "1.75"},  # nét --sw × 1,4
}
ATTR_ORDER = ("d", "cx", "cy", "r", "x", "y", "width", "height", "rx", "ry",
              "fill", "stroke", "stroke-width", "stroke-linecap", "stroke-dasharray")


def load_proto(js_dir=PROTO_JS):
    """Dữ liệu prototype (biểu tượng, vị trí lệnh, bộ Viewer) do proto_dump.js in ra."""
    dump = Path(__file__).with_name("proto_dump.js")
    out = subprocess.run(["node", str(dump), str(js_dir)], check=True, stdout=subprocess.PIPE)
    return json.loads(out.stdout.decode("utf-8"))


def groups(icons_js):
    """Tên biểu tượng → nhóm, theo chú thích trong khối TC.ICONS. '// ==== Đợt n · tệp · nhóm ====' đặt nhóm;
    chú thích thường cũng đặt nhóm, trừ khi đứng ngay sau tiêu đề (khi đó là lời mô tả của tiêu đề)."""
    body = icons_js[icons_js.index("TC.ICONS = {"):icons_js.index("\n};")]
    res, group, after_header = {}, "", False
    for line in body.splitlines()[1:]:
        s = line.strip()
        if s.startswith("//"):
            text = s[2:].strip()
            if text.startswith("===="):
                if "hết" not in text:
                    group, after_header = text.strip("= ").split(" · ")[-1], True
            elif not after_header:
                group = text
            continue
        for name in re.findall(r"(\w+):'", line):
            res[name] = group
            after_header = False
    return res


def num(v):
    return "%g" % round(v, 4)


def f32(v):
    return struct.unpack("f", struct.pack("f", v))[0]


def polylines(d):
    """Các đường gấp khúc [(điểm…), …] của path chỉ gồm đoạn thẳng (M L H V Z, hoa hoặc thường); đường đóng lặp lại
    điểm đầu ở cuối."""
    toks = re.findall(r"[A-Za-z]|-?(?:\d+\.?\d*|\.\d+)", d)
    res, x, y, cmd, i = [], 0.0, 0.0, "", 0
    while i < len(toks):
        if toks[i].isalpha():
            cmd, i = toks[i], i + 1
            if cmd in "Zz":
                res[-1].append(res[-1][0])
                x, y = res[-1][0]
                continue
        c, rel = cmd.upper(), cmd.islower()
        if c not in "MLHV":
            raise ValueError("đường chấm chỉ nhận đoạn thẳng M L H V Z: " + d)
        n = 1 if c in "HV" else 2
        v = [float(t) for t in toks[i:i + n]]
        i += n
        nx = (v[0] + (x if rel else 0)) if c != "V" else x
        ny = (v[-1] + (y if rel else 0)) if c != "H" else y
        if c == "M":
            res.append([])
            cmd = "l" if rel else "L"  # cặp số tiếp sau M là L
        res[-1].append((nx, ny))
        x, y = nx, ny
    return res


def dots(d, period, r):
    """Đường chấm (stroke-dasharray "0 chu kỳ", đầu tròn) → chấm tròn tô đặc, vì QtSvg không vẽ nét dài 0.
    Vị trí như Chromium (Skia) vẽ prototype: chu kỳ là float32, cộng dồn bằng double, chỉ lấy chấm có quãng đường
    < độ dài đường; nên chấm đúng ở điểm cuối có khi bị bỏ (5 × 2,4f > 12), có khi không (5 × 2,6f < 13)."""
    out = []
    for pts in polylines(d):
        segs = [(a, b, f32(((b[0] - a[0]) ** 2 + (b[1] - a[1]) ** 2) ** 0.5)) for a, b in zip(pts, pts[1:])]
        total, start, dist, step = f32(sum(s[2] for s in segs)), 0.0, 0.0, f32(period)
        for a, b, length in segs:
            while dist < total and dist <= start + length:
                t = (dist - start) / length if length else 0.0
                out.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
                dist += step
            start += length
    return ['<circle cx="%s" cy="%s" r="%s" fill="currentColor" stroke="none"/>' % (num(x), num(y), num(r))
            for x, y in out]


def to_svg(markup):
    """Markup một biểu tượng của icons.js → nội dung tệp SVG (chỉ path/circle/rect, không lớp, không style)."""
    lines = [SVG_OPEN]
    for el in ET.fromstring("<g>" + markup + "</g>"):
        tag, a = el.tag, dict(el.attrib)
        if tag == "ellipse":  # thành path hai nửa cung, để SVG chỉ còn path/circle/rect
            cx, cy, rx, ry = (float(a.pop(k)) for k in ("cx", "cy", "rx", "ry"))
            d = "M%s %sa%s %s 0 1 0 %s 0a%s %s 0 1 0 %s 0z" % (
                num(cx - rx), num(cy), num(rx), num(ry), num(2 * rx), num(rx), num(ry), num(-2 * rx))
            tag, a = "path", dict(d=d, **a)
        for c in a.pop("class", "").split():
            a.update(CLASS_ATTRS[c])  # lớp lạ → KeyError: thêm vào CLASS_ATTRS theo app.css
        for decl in filter(str.strip, a.pop("style", "").split(";")):
            k, v = decl.split(":")
            a[k.strip()] = v.strip()
        dash = a.get("stroke-dasharray", "").split()
        if len(dash) == 2 and float(dash[0]) == 0 and a.get("stroke-linecap") == "round":
            lines += ["  " + c for c in dots(a["d"], float(dash[1]), float(a.get("stroke-width", "1.25")) / 2)]
            continue
        keys = [k for k in ATTR_ORDER if k in a] + [k for k in a if k not in ATTR_ORDER]
        lines.append("  <%s %s/>" % (tag, " ".join('%s="%s"' % (k, html.escape(a[k])) for k in keys)))
    lines.append("</svg>")
    return "\n".join(lines) + "\n"


def convert(proto, icons_js, out_dir):
    """Ghi out_dir/src/<tên>.svg (xóa SVG không còn trong prototype) và out_dir/manifest.json."""
    icons, grp = proto["icons"], groups(icons_js)
    if grp.keys() != icons.keys():
        raise SystemExit("iconconv: không đọc được nhóm của %s" % sorted(icons.keys() ^ grp.keys()))
    viewer = set(proto["viewerIcons"]) | set(proto["frameIcons"])
    src = Path(out_dir) / "src"
    src.mkdir(parents=True, exist_ok=True)
    for old in src.glob("*.svg"):
        if old.stem not in icons:
            old.unlink()
    for name, markup in icons.items():
        (src / (name + ".svg")).write_bytes(to_svg(markup).encode("utf-8"))
    rows = [json.dumps({"name": n, "group": grp[n], "viewer": n in viewer}, ensure_ascii=False) for n in icons]
    (Path(out_dir) / "manifest.json").write_bytes(('{"icons": [\n  ' + ",\n  ".join(rows) + "\n]}\n").encode("utf-8"))
    return len(icons), sum(n in viewer for n in icons)


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--proto", default=str(PROTO_JS), help="thư mục js của prototype")
    ap.add_argument("--out", default=str(ICONS_DIR), help="thư mục đích (chứa src/ và manifest.json)")
    args = ap.parse_args(argv)
    icons_js = (Path(args.proto) / "icons.js").read_text(encoding="utf-8")
    n, nv = convert(load_proto(args.proto), icons_js, args.out)
    print("iconconv: %d biểu tượng, %d thuộc bộ Viewer → %s" % (n, nv, args.out))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
