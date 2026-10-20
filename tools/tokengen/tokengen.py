#!/usr/bin/env python3
"""Sinh token Qt từ design/prototype/css/tokens.css và kiểm tương phản lúc build (UI0-102).

Dùng: tokengen.py <tokens.css> <contrast-pairs.txt> <thư mục ra>
  → <ra>/Tokens.h, <ra>/tokens.json. Cú pháp lạ hoặc cặp màu không đạt: in "tệp:dòng: lý do", mã thoát 1,
  không ghi tệp nào. Tệp ra chỉ ghi lại khi nội dung đổi, nên sửa tokens.css mà token không đổi thì không
  build lại mã C++.

Mỗi token có một hàng giá trị cho mỗi tổ hợp biến thể trong trục của loại nó (AXES): hàng r có bit j bật
⇔ biến thể axes[j] bật. Biến thể ngoài trục mà làm đổi giá trị là lỗi (Tokens.h chưa có trục đó).
"""
import itertools
import json
import math
import os
import re
import sys
from fractions import Fraction

# Khối của tokens.css → biến thể. Khóa: chuỗi @media và selector lồng nhau, đã bỏ hết khoảng trắng.
VARIANTS = {
    (":root",): "dark",
    ("@media(prefers-color-scheme:light)", ':root:not([data-theme="dark"])'): "light",
    (':root[data-theme="light"]',): "light",
    (':root[data-density="comfortable"]',): "comfortable",
    (":root[data-lite]",): "lite",
    ("@media(prefers-reduced-motion:reduce)", ":root"): "reduced-motion",
}
# Thứ tự cascade, ưu tiên tăng dần: khối giảm chuyển động là ":root" (0,1,0) nên thua các khối thuộc tính (0,2,0),
# các khối (0,2,0) xếp theo thứ tự nguồn. ponytail: cố định theo tokens.css hiện tại, không tính specificity.
ORDER = ("dark", "reduced-motion", "light", "comfortable", "lite")
# Loại token → trục biến thể được phép làm đổi giá trị. Thứ tự trục quyết định chỉ số hàng.
AXES = {
    "color": ("light",),
    "px": ("comfortable",),
    "ms": ("lite", "reduced-motion"),
    "shadow": ("light", "lite"),
    "num": (),
    "str": (),
}
# Loại → (tên Kind trong C++, kiểu giá trị C++); hàm tra trong Tokens.h trùng tên loại (tok::color, tok::px…).
CPP = {
    "color": ("Color", "Rgba"),
    "px": ("Px", "double"),
    "ms": ("Ms", "int"),
    "shadow": ("Shadow", "Shadow"),
    "num": ("Num", "double"),
    "str": ("Str", "std::string_view"),
}
FIELD = {"light": "light", "comfortable": "comfortable", "lite": "lite", "reduced-motion": "reducedMotion"}
RESERVED = {"color", "px", "ms", "shadow", "num", "str", "name", "find", "tokens", "detail"}

NUM = r"[+-]?(?:\d+\.?\d*|\.\d+)"
WORD = r'(?:"[^"]*"|-?[A-Za-z][\w-]*)'  # chuỗi trong ngoặc kép hoặc định danh CSS (font, square, none)
VAR = r"var\((--[\w-]+)\)"


class TokenError(Exception):
    """Lỗi kèm vị trí "tệp:dòng: lý do"."""


def parse_css(text, path):
    """tokens.css → {biến thể: {tên: (giá trị thô, "tệp:dòng")}}; thuộc tính không có "--" bị bỏ qua."""
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group().count("\n"), text, flags=re.S)

    def where(pos):
        return f"{path}:{text.count(chr(10), 0, pos) + 1}"

    if "/*" in text:
        raise TokenError(f"{where(text.index('/*'))}: chú thích không đóng")
    prefixes = {k[:i] for k in VARIANTS for i in range(1, len(k))}
    blocks, stack = {}, []  # selector → ({tên: (thô, nơi)}, nơi mở khối); khối cùng selector gộp làm một
    for m in re.finditer(r"([^{};]*)([{};]|$)", text):
        raw, delim = m.group(1), m.group(2)
        chunk, at = raw.strip(), m.start(1) + len(raw) - len(raw.lstrip())
        if delim == "{":
            stack.append(re.sub(r"\s+", "", chunk))
            if tuple(stack) in VARIANTS:
                blocks.setdefault(tuple(stack), ({}, where(at)))
            elif tuple(stack) not in prefixes:
                raise TokenError(f"{where(at)}: khối lạ '{chunk}'")
            continue
        if chunk:
            name, colon, value = (s.strip() for s in chunk.partition(":"))
            if tuple(stack) not in VARIANTS or not colon or not value:
                raise TokenError(f"{where(at)}: khai báo lạ '{chunk}'")
            if name.startswith("--"):
                decls = blocks[tuple(stack)][0]
                if not re.fullmatch(r"--[A-Za-z][A-Za-z0-9_-]*", name) or name in decls:
                    raise TokenError(f"{where(at)}: tên token lạ hoặc khai báo lặp '{name}'")
                decls[name] = (" ".join(value.split()), where(at))
        if delim == "}":
            if not stack:
                raise TokenError(f"{where(m.start(2))}: thừa '}}'")
            stack.pop()
        elif not delim and stack:
            raise TokenError(f"{where(len(text))}: thiếu '}}'")

    variants = {}
    for key, (decls, at) in blocks.items():  # các selector cùng biến thể (hai khối Sáng) phải trùng nhau
        v = VARIANTS[key]
        a = {n: r for n, (r, _) in variants.setdefault(v, decls).items()}
        b = {n: r for n, (r, _) in decls.items()}
        diff = sorted(n for n in a.keys() | b.keys() if a.get(n) != b.get(n))
        if diff:
            raise TokenError(f"{at}: khối '{v}' khác khối trước cùng biến thể ở {', '.join(diff)}")
    if "dark" not in variants:
        raise TokenError(f"{path}:1: thiếu khối :root")
    for decls in variants.values():
        for n, (_, at) in decls.items():
            if n not in variants["dark"]:
                raise TokenError(f"{at}: {n} không có trong :root")
    return variants


def parse_color(raw):
    """#rgb, #rrggbb, #rrggbbaa, rgb()/rgba() kênh 0–255 → (r, g, b, a) 8 bit; không phải màu → None."""
    m = re.fullmatch(r"#([0-9A-Fa-f]{3}|[0-9A-Fa-f]{6}|[0-9A-Fa-f]{8})", raw)
    if m:
        h = m[1] if len(m[1]) > 3 else "".join(c * 2 for c in m[1])
        return tuple(bytes.fromhex(h + ("ff" if len(h) == 6 else "")))
    m = re.fullmatch(rf"rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*(?:,\s*({NUM})\s*)?\)", raw)
    if m:
        rgb, a = [int(x) for x in m.groups()[:3]], Fraction(m[4] or 1)
        if max(rgb) <= 255 and 0 <= a <= 1:
            return (*rgb, math.floor(a * 255 + Fraction(1, 2)))  # alpha làm tròn nửa lên, tính bằng phân số
    return None


def parse_value(raw, at):
    """Giá trị thô → (loại, giá trị). var(--x) còn để ("var", "--x"); màu bóng có thể là "--x" chờ phân giải."""
    m = re.fullmatch(rf"({NUM})(px|ms)?", raw)
    if m:
        v = float(m[1])
        if m[2] != "ms":
            return ("px" if m[2] else "num"), v
        if v.is_integer():
            return "ms", int(v)
    m = re.fullmatch(VAR, raw)
    if m:
        return "var", m[1]
    color = parse_color(raw)
    if color:
        return "color", color
    m = re.fullmatch(rf"((?:(?:{NUM}px|[+-]?0)\s+){{2,4}})(.+)", raw)  # bóng: x y [blur [spread]] màu
    color = m and (parse_color(m[2]) or (re.fullmatch(VAR, m[2]) and m[2][4:-1]))
    if color:
        lengths = [float(x[:-2] if x.endswith("px") else x) for x in m[1].split()]
        return "shadow", (*(lengths + [0.0, 0.0])[:4], color)
    if re.fullmatch(rf"{WORD}(?:\s*,\s*{WORD})*", raw):
        return "str", raw
    raise TokenError(f"{at}: giá trị lạ '{raw}'")


def resolve(variants, name, active, at="", seen=()):
    """(loại, giá trị, nơi khai báo) của token khi các biến thể `active` bật; var() phân giải cùng tổ hợp."""
    if name not in variants["dark"]:
        raise TokenError(f"{at}: var() trỏ tới token không có {name}")
    if name in seen:
        raise TokenError(f"{at}: var() vòng quanh {' → '.join(seen + (name,))}")
    raw, at = next(variants[v][name] for v in reversed(ORDER)
                   if (v == "dark" or v in active) and name in variants.get(v, {}))
    kind, val = parse_value(raw, at)
    if kind == "var":
        kind, val, _ = resolve(variants, val, active, at, seen + (name,))
    elif kind == "shadow" and isinstance(val[4], str):
        ck, cv, _ = resolve(variants, val[4], active, at, seen + (name,))
        if ck != "color":
            raise TokenError(f"{at}: màu bóng {val[4]} không phải màu")
        val = (*val[:4], cv)
    return kind, val, at


def build(variants):
    """→ [(tên, loại, trục, [giá trị theo hàng])] theo thứ tự nguồn của :root."""
    tokens, idents = [], {}
    for name, (_, at) in variants["dark"].items():
        ident = name[2:].replace("-", "_")
        if ident in RESERVED or idents.setdefault(ident, name) != name:
            raise TokenError(f"{at}: {name} trùng tên C++ '{ident}'")
        kind = resolve(variants, name, ())[0]
        axes, rows = AXES[kind], {}
        for combo in itertools.product((False, True), repeat=len(ORDER) - 1):
            active = {v for v, on in zip(ORDER[1:], combo) if on}
            k, val, at = resolve(variants, name, active)
            if k != kind:
                raise TokenError(f"{at}: {name} là {kind} ở :root nhưng là {k} ở đây")
            if rows.setdefault(sum(1 << j for j, a in enumerate(axes) if a in active), val) != val:
                raise TokenError(f"{at}: {name} ({kind}) đổi theo {', '.join(sorted(active - set(axes)))}, "
                                 f"Tokens.h chưa có trục này cho loại {kind}")
        tokens.append((name, kind, axes, [rows[r] for r in range(1 << len(axes))]))
    return tokens


def luminance(c):
    """Độ chói tương đối WCAG 2.x của màu sRGB (kênh 0–255, có thể lẻ sau khi trộn)."""
    lin = [v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4 for v in (x / 255 for x in c[:3])]
    return 0.2126 * lin[0] + 0.7152 * lin[1] + 0.0722 * lin[2]


def ratio(a, b):
    lo, hi = sorted((luminance(a), luminance(b)))
    return (hi + 0.05) / (lo + 0.05)


def over(fg, bg):
    """Trộn fg (có alpha) lên nền đục bg trong không gian sRGB, như trình duyệt và QPainter."""
    a = fg[3] / 255
    return (*(f * a + b * (1 - a) for f, b in zip(fg[:3], bg[:3])), 255)


def check_contrast(tokens, text, path):
    """Dòng: <chữ hoặc viền> <nền> [<nền dưới>] <ngưỡng>, kiểm ở Tối và Sáng → [(tỉ số, mô tả)].

    Màu có alpha được trộn lên lớp dưới nó trước khi tính; lớp dưới cùng phải đục. Không đạt → TokenError.
    """
    colors = {n: rows for n, kind, _, rows in tokens if kind == "color"}
    results, bad = [], []
    for ln, line in enumerate(text.splitlines(), 1):
        f = line.split("#")[0].split()
        if not f:
            continue
        if len(f) not in (3, 4) or any(n not in colors for n in f[:-1]) or not re.fullmatch(NUM, f[-1]):
            raise TokenError(f"{path}:{ln}: dòng lạ hoặc token không phải màu '{line.strip()}'")
        for theme, row in (("Tối", 0), ("Sáng", 1)):
            layers = [colors[n][row] for n in f[:-1]]
            if layers[-1][3] != 255:
                raise TokenError(f"{path}:{ln}: {f[-2]} có alpha, ghi thêm nền đục bên dưới")
            bg = layers[-1]
            for c in reversed(layers[1:-1]):
                bg = over(c, bg)
            r = ratio(over(layers[0], bg), bg)
            desc = f"{f[0]} trên {' / '.join(f[1:-1])} ({theme}) = {math.floor(r * 100) / 100:.2f}:1"
            results.append((r, desc))
            if r < float(f[-1]):
                bad.append(f"{path}:{ln}: {desc} < {f[-1]}:1")
    if bad:
        raise TokenError("\n".join(bad))
    return results


def cpp_value(kind, v):
    if kind == "color":
        return "{" + ", ".join(f"0x{c:02x}" for c in v) + "}"
    if kind == "shadow":
        return "{" + ", ".join(repr(x) for x in v[:4]) + ", " + cpp_value("color", v[4]) + "}"
    return json.dumps(v) if kind == "str" else repr(v)


def render_header(tokens):
    by_kind = {k: [t for t in tokens if t[1] == k] for k in CPP}
    out = [HEADER]
    for kind, (K, _) in CPP.items():
        out.append(f"struct {K}Id {{\n    static constexpr Kind kind = Kind::{K};\n    std::uint16_t i;\n}};")
    for kind, (K, _) in CPP.items():
        out.append(f"\n// {K}: " + (f"đổi theo {', '.join(AXES[kind])}" if AXES[kind] else "không đổi theo biến thể"))
        out += [f"inline constexpr {K}Id {n[2:].replace('-', '_')}{{{i}}};" for i, (n, *_) in enumerate(by_kind[kind])]
    out.append("\nstruct Token {\n    std::string_view name; // tên CSS, ví dụ \"--c-accent\"\n    Kind kind;\n"
               "    std::uint16_t i; // chỉ số trong loại, như ColorId::i\n};\n\n"
               "// Mọi token, theo loại rồi theo thứ tự trong tokens.css.\ninline constexpr Token tokens[] = {")
    out += [f"    {{{json.dumps(n)}, Kind::{CPP[k][0]}, {i}}}," for k in CPP for i, (n, *_) in enumerate(by_kind[k])]
    first = list(itertools.accumulate([0] + [len(by_kind[k]) for k in CPP]))[:-1]
    out.append("};\n\nnamespace detail {\n// Vị trí token đầu tiên của mỗi Kind trong tokens[].\n"
               f"inline constexpr std::uint16_t first[] = {{{', '.join(map(str, first))}}};")
    for kind, (K, ctype) in CPP.items():
        axes, ts = AXES[kind], by_kind[kind]
        out.append(f"\n// Hàng r: bit j bật ⇔ biến thể thứ j bật, trục: {', '.join(axes) or 'không'}.\n"
                   f"inline constexpr {ctype} {kind}[{1 << len(axes)}][{len(ts)}] = {{")
        for r in range(1 << len(axes)):
            out += ["    {"] + [f"        {cpp_value(kind, rows[r])}, // {n}" for n, _, _, rows in ts] + ["    },"]
        out.append("};")
    out.append("} // namespace detail\n")
    for kind, (K, ctype) in CPP.items():
        idx = " + ".join((f"{1 << j} * " if j else "") + f"int(m.{FIELD[a]})" for j, a in enumerate(AXES[kind]))
        params = f"{K}Id id, Mode m" if AXES[kind] else f"{K}Id id"
        out.append(f"constexpr {ctype} {kind}({params}) {{ return detail::{kind}[{idx or 0}][id.i]; }}")
    out.append(FOOTER)
    return "\n".join(out)


def render_json(tokens):
    def js(kind, v):
        if kind == "shadow":
            return {"x": v[0], "y": v[1], "blur": v[2], "spread": v[3], "color": list(v[4])}
        return list(v) if kind == "color" else v

    data = {"format": "values[r]: bit j của r bật ⇔ biến thể axes[j] bật; màu là [r, g, b, a] 8 bit",
            "tokens": {n: {"kind": k, "axes": list(axes), "values": [js(k, v) for v in rows]}
                       for n, k, axes, rows in tokens}}
    return json.dumps(data, ensure_ascii=False, indent=1) + "\n"


def write(path, text):
    try:
        with open(path, encoding="utf-8", newline="") as f:
            if f.read() == text:
                return
    except FileNotFoundError:
        pass
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


HEADER = """\
// Sinh bởi tools/tokengen/tokengen.py từ design/prototype/css/tokens.css (UI0-102). Không sửa tay.
//
// Mỗi token CSS là một định danh có kiểu: --c-accent → tok::c_accent (ColorId), --h-row → tok::h_row (PxId).
// Tra giá trị theo biến thể đang bật (tra nhầm loại là lỗi biên dịch):
//   tok::color(tok::c_accent, mode)  tok::px(tok::h_row, mode)  tok::ms(tok::t, mode)
//   tok::shadow(tok::sh_pop, mode)   tok::num(tok::fw_m)        tok::str(tok::f_ui)
// Tên CSS: tok::name(tok::c_accent) == "--c-accent"; theo tên: tok::find("--c-accent") (QSS, thông báo lỗi).
#pragma once

#include <cstdint>
#include <string_view>

namespace tok {

struct Rgba {
    std::uint8_t r, g, b, a;
    bool operator==(const Rgba&) const = default;
};

// box-shadow, đơn vị px. Giao diện nhẹ thay bóng bằng viền 1 px (spread 1, màu --c-border-ctl).
struct Shadow {
    double x, y, blur, spread;
    Rgba color;
    bool operator==(const Shadow&) const = default;
};

// Biến thể của tokens.css đang bật; Mode{} là :root (Tối, gọn).
struct Mode {
    bool light = false;         // Sáng: [data-theme="light"] hoặc hệ thống sáng
    bool comfortable = false;   // mật độ thoáng: [data-density="comfortable"]
    bool lite = false;          // Giao diện nhẹ: [data-lite]
    bool reducedMotion = false; // hệ thống bật giảm chuyển động: prefers-reduced-motion: reduce
};

enum class Kind : std::uint8_t { Color, Px, Ms, Shadow, Num, Str };
"""

FOOTER = """
// Tên CSS của token, ví dụ name(c_accent) == "--c-accent".
template <class Id> constexpr std::string_view name(Id id) { return tokens[detail::first[int(Id::kind)] + id.i].name; }

// Tra theo tên CSS; nullptr nếu không có. ponytail: quét tuyến tính (~150 token), đủ cho QSS và thông báo lỗi.
constexpr const Token* find(std::string_view name)
{
    for (const Token& t : tokens)
        if (t.name == name)
            return &t;
    return nullptr;
}

} // namespace tok
"""


def main(argv):
    if len(argv) != 4:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    css, pairs, out = argv[1:]
    try:
        with open(css, encoding="utf-8") as f:
            tokens = build(parse_css(f.read(), css))
        with open(pairs, encoding="utf-8") as f:
            check_contrast(tokens, f.read(), pairs)
    except TokenError as e:
        print(e, file=sys.stderr)
        return 1
    os.makedirs(out, exist_ok=True)
    write(os.path.join(out, "Tokens.h"), render_header(tokens))
    write(os.path.join(out, "tokens.json"), render_json(tokens))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
