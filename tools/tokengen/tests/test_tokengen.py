import contextlib
import io
import json
import os
import re
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import tokengen as tg  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
CSS = os.path.join(ROOT, "design", "prototype", "css", "tokens.css")
PAIRS = os.path.join(ROOT, "app", "theme", "contrast-pairs.txt")


def read(path, mode="r"):
    with open(path, mode, **({} if "b" in mode else {"encoding": "utf-8"})) as f:
        return f.read()


def output(css):
    """tokens.json sinh từ chuỗi CSS, đọc lại như phía dùng đầu ra."""
    return json.loads(tg.render_json(tg.build(tg.parse_css(css, "mẫu.css"))))["tokens"]


def value(out, name, *active):
    t = out[name]
    return t["values"][sum(1 << j for j, a in enumerate(t["axes"]) if a in active)]


def declared(css):
    """{biến thể: {tên: giá trị thô}} đọc bằng regex riêng, độc lập với parse_css."""
    css = re.sub(r"/\*.*?\*/", "", css, flags=re.S)
    blocks = {
        "dark": r"^\s*:root\s*\{([^{}]*)\}",
        "light": r':root:not\(\[data-theme="dark"\]\)\s*\{([^{}]*)\}',
        "light-attr": r':root\[data-theme="light"\]\s*\{([^{}]*)\}',
        "comfortable": r':root\[data-density="comfortable"\]\s*\{([^{}]*)\}',
        "lite": r":root\[data-lite\]\s*\{([^{}]*)\}",
        "reduced-motion": r"prefers-reduced-motion:\s*reduce\)\s*\{\s*:root\s*\{([^{}]*)\}",
    }
    return {v: {n: " ".join(r.split()) for n, r in re.findall(r"(--[\w-]+)\s*:\s*([^;]+)", re.search(p, css)[1])}
            for v, p in blocks.items()}


class Coverage(unittest.TestCase):
    """Tiêu chí 1: mọi thuộc tính --* của từng biến thể có trong đầu ra, tên giữ nguyên."""

    def test_every_property_of_every_variant_is_in_output(self):
        css = read(CSS)
        out, decl = output(css), declared(css)
        self.assertEqual(decl["light"], decl.pop("light-attr"), "hai khối Sáng phải trùng nhau")
        base = decl.pop("dark")
        self.assertEqual((sorted(set(base) - set(out)), sorted(set(out) - set(base))), ([], []), ":root thiếu, thừa")
        for v, names in decl.items():
            # Khai báo của biến thể v (khác :root) phải là đúng những token đổi giá trị khi bật v.
            want = {n for n, raw in names.items() if raw != base[n]}
            got = {n for n in out if value(out, n, v) != value(out, n)}
            with self.subTest(variant=v, declared=len(names)):
                self.assertTrue(names)
                self.assertEqual((sorted(want - got), sorted(got - want)), ([], []), "thiếu, thừa")


SAMPLE = """\
:root{
  --c-a: #3CC8C0; --c-b: rgba(255,255,255,.055); --c-c: #abc; --c-d: rgb(1, 2, 3); --c-e: #11223380;
  --s: 10.5px; --t: 80ms; --n: 500; --m: 1.35; --o: .5; --neg: -2px;
  --c-alias: var(--c-a); --sh: 0 4px 14px rgba(0,0,0,.45);
  --f: "IBM Plex Sans", system-ui, -apple-system, sans-serif; --cap: square;
  color-scheme: dark;
}
:root[data-theme="light"]{ --c-a: #06706A; --sh: 0 4px 14px rgba(20,30,40,.18); }
:root[data-density="comfortable"]{ --s: 11.5px }
:root[data-lite]{ --sh: 0 0 0 1px var(--c-a); --t: 0ms; }
@media (prefers-reduced-motion: reduce){ :root{ --t: 0ms; } }
"""


class Conversion(unittest.TestCase):
    """Tiêu chí 2: hex, rgba(), px, ms, số trần, var() lồng chuyển đúng; cú pháp lạ lỗi kèm số dòng."""

    def test_values_convert_exactly(self):
        out = output(SAMPLE)
        self.assertEqual({n: t["kind"] for n, t in out.items()}, {
            "--c-a": "color", "--c-b": "color", "--c-c": "color", "--c-d": "color", "--c-e": "color",
            "--s": "px", "--t": "ms", "--n": "num", "--m": "num", "--o": "num", "--neg": "px",
            "--c-alias": "color", "--sh": "shadow", "--f": "str", "--cap": "str"})
        lite_sh = {"x": 0, "y": 0, "blur": 0, "spread": 1}
        cases = [
            (("--c-a",), [0x3C, 0xC8, 0xC0, 255]),
            (("--c-a", "light"), [0x06, 0x70, 0x6A, 255]),
            (("--c-b",), [255, 255, 255, 14]),  # .055 × 255 = 14,025
            (("--c-c",), [0xAA, 0xBB, 0xCC, 255]),
            (("--c-d",), [1, 2, 3, 255]),
            (("--c-e",), [0x11, 0x22, 0x33, 0x80]),
            (("--c-alias", "light"), [0x06, 0x70, 0x6A, 255]),
            (("--s",), 10.5), (("--s", "comfortable"), 11.5), (("--neg",), -2),
            (("--t",), 80), (("--t", "lite"), 0), (("--t", "reduced-motion"), 0),
            (("--n",), 500), (("--m",), 1.35), (("--o",), 0.5),
            (("--sh",), {"x": 0, "y": 4, "blur": 14, "spread": 0, "color": [0, 0, 0, 115]}),
            (("--sh", "light"), {"x": 0, "y": 4, "blur": 14, "spread": 0, "color": [20, 30, 40, 46]}),
            (("--sh", "lite"), dict(lite_sh, color=[0x3C, 0xC8, 0xC0, 255])),
            (("--sh", "lite", "light"), dict(lite_sh, color=[0x06, 0x70, 0x6A, 255])),  # nhẹ thắng Sáng; var() lồng theo chủ đề
            (("--f",), '"IBM Plex Sans", system-ui, -apple-system, sans-serif'),
            (("--cap",), "square"),
        ]
        self.assertEqual(value(output(":root{ --a: 1px; }\n:root{ --b: 2px; }"), "--b"), 2)  # khối cùng selector gộp lại
        for (name, *active), want in cases:
            with self.subTest(name=name, active=active):
                self.assertEqual(value(out, name, *active), want)

    def test_unknown_syntax_fails_with_line(self):
        cases = [
            (":root{\n --a: 12pt;\n}", 2),  # đơn vị lạ
            (":root{\n --c: #12345;\n}", 2),
            (":root{\n\n --c: rgb(1 2 3);\n}", 3),
            (":root{\n --c: rgba(300,0,0,1);\n}", 2),
            (":root{\n --a: calc(1px + 2px);\n}", 2),
            (":root{\n --t: 1.5ms;\n}", 2),
            (":root{\n --sh: 0 4px 14px;\n}", 2),  # bóng thiếu màu
            (":root{\n --a 1px;\n}", 2),
            (":root{\n --a: 1px;\n --a: 2px;\n}", 3),
            (":root{\n --a: 1px;\n}\n:root{\n --a: 2px;\n}", 5),  # lặp qua hai khối cùng selector
            (':root{\n --a: 1px;\n}\n:root[data-theme="hc"]{\n --a: 2px;\n}', 4),  # khối lạ
            (":root{\n --a: 1px;\n}\n}", 4),
            (":root{\n --a: 1px;\n", 3),
            ("/* x\n:root{}", 1),
            (":root{\n --a: var(--b);\n}", 2),
            (":root{\n --a: var(--b);\n --b: var(--a);\n}", 3),
            (':root{\n --a: 1px;\n}\n:root[data-lite]{\n --b: 1px;\n}', 5),  # không có trong :root
            (':root{\n --a: 1px;\n}\n:root[data-density="comfortable"]{\n --a: #fff;\n}', 5),  # đổi loại
            (':root{\n --c: #fff;\n}\n:root[data-density="comfortable"]{\n --c: #000;\n}', 5),  # màu theo mật độ
            (':root{\n --c: #fff;\n}\n@media (prefers-color-scheme: light){ :root:not([data-theme="dark"]){ --c: #000; } }\n'
             ':root[data-theme="light"]{\n --c: #111;\n}', 5),  # hai khối Sáng khác nhau
        ]
        for css, line in cases:
            with self.subTest(css=css):
                with self.assertRaisesRegex(tg.TokenError, rf"^mẫu\.css:{line}: "):
                    tg.build(tg.parse_css(css, "mẫu.css"))


class Contrast(unittest.TestCase):
    """Tiêu chí 3: tỉ số WCAG 2.x đúng, trộn alpha trước khi tính, cặp không đạt làm build lỗi và in tỉ số."""

    def test_ratio_known_cases(self):
        white, black = (255, 255, 255, 255), (0, 0, 0, 255)
        self.assertAlmostEqual(tg.ratio(black, white), 21.0)
        self.assertAlmostEqual(tg.ratio(white, white), 1.0)
        self.assertAlmostEqual(tg.ratio((0x76, 0x76, 0x76, 255), white), 4.54, places=2)  # xám tối nhất đạt 4,5:1
        self.assertLess(tg.ratio((0x77, 0x77, 0x77, 255), white), 4.5)
        self.assertAlmostEqual(tg.ratio(tg.over((0, 0, 0, 128), white), white), tg.ratio((127, 127, 127, 255), white))

    def test_pairs_checked_in_both_themes(self):
        toks = tg.build(tg.parse_css(':root{ --c-bg: #FFFFFF; --c-fg: #767676; --c-half: rgba(0,0,0,.5); --n: 1; }\n'
                                     ':root[data-theme="light"]{ --c-fg: #777777; }', "mẫu.css"))
        self.assertEqual(len(tg.check_contrast(toks, "--c-fg --c-bg 4.4 # đạt cả hai\n", "c.txt")), 2)
        fails = [
            ("# chú thích\n--c-fg --c-bg 4.5", r"^c\.txt:2: --c-fg trên --c-bg \(Sáng\) = 4\.47:1 < 4\.5:1$"),
            ("--c-half --c-bg 4.5", r"--c-half trên --c-bg \(Tối\) = 4\.00:1 < 4\.5:1"),  # đen 50% trộn lên trắng
            ("--c-fg --c-half 3", r"--c-half có alpha"),
            ("--c-fg --n 3", r"^c\.txt:1: dòng lạ"),
            ("--c-fg 3", r"^c\.txt:1: dòng lạ"),
        ]
        for text, err in fails:
            with self.subTest(text=text):
                with self.assertRaisesRegex(tg.TokenError, err):
                    tg.check_contrast(toks, text, "c.txt")
        tg.check_contrast(toks, "--c-fg --c-half --c-bg 1", "c.txt")

    def test_failing_pair_fails_build_and_writes_nothing(self):
        with tempfile.TemporaryDirectory() as d:
            pairs, out = os.path.join(d, "pairs.txt"), os.path.join(d, "out")
            with open(pairs, "w", encoding="utf-8") as f:
                f.write("--c-text-dis --c-base 4.5\n")
            err = io.StringIO()
            with contextlib.redirect_stderr(err):
                self.assertEqual(tg.main(["tokengen.py", CSS, pairs, out]), 1)
            self.assertRegex(err.getvalue(), r"pairs\.txt:1: --c-text-dis trên --c-base \(Tối\) = 3\.\d\d:1 < 4\.5:1")
            self.assertFalse(os.path.exists(out))

    def test_project_pairs_cover_required_and_pass(self):
        active = {tuple(line.split("#")[0].split()[:-1]) for line in read(PAIRS).splitlines() if line.split("#")[0].strip()}
        texts = ["--c-text", "--c-text-2", "--c-text-3", "--c-accent", "--c-on", "--c-off", "--c-ok", "--c-warn",
                 "--c-danger", "--c-info"]
        badges = ["viewer", "vp", "pro", "ent", "edu", "trial"]
        required = ([(t, "--c-base") for t in texts] + [("--c-accent-t", "--c-accent"), ("--c-border-ctl", "--c-base")]
                    + [("--c-text", s) for s in ("--c-chrome", "--c-raised", "--c-sunken")]
                    + [(f"--c-ed-{b}-t", f"--c-ed-{b}") for b in badges]
                    + [("--c-dyn-t", "--c-dyn-bg"), ("--c-snap-tip-t", "--c-snap-tip-bg")])
        self.assertEqual(sorted(set(required) - active), [])
        tg.check_contrast(tg.build(tg.parse_css(read(CSS), CSS)), read(PAIRS), PAIRS)


class Determinism(unittest.TestCase):
    """Tiêu chí 4: cùng đầu vào cùng byte; nội dung không đổi thì không ghi lại tệp."""

    def test_same_input_same_bytes_and_no_rewrite(self):
        with tempfile.TemporaryDirectory() as d:
            outs = []
            for sub in ("a", "b"):
                self.assertEqual(tg.main(["tokengen.py", CSS, PAIRS, os.path.join(d, sub)]), 0)
                outs.append(b"".join(read(os.path.join(d, sub, f), "rb") for f in ("Tokens.h", "tokens.json")))
            self.assertEqual(outs[0], outs[1])
            self.assertNotIn(ROOT.encode(), outs[0])  # không nhúng đường dẫn của máy build
            self.assertNotIn(b"\r", outs[0])
            header = os.path.join(d, "a", "Tokens.h")
            os.utime(header, (0, 0))
            self.assertEqual(tg.main(["tokengen.py", CSS, PAIRS, os.path.join(d, "a")]), 0)
            self.assertEqual(os.stat(header).st_mtime, 0)


if __name__ == "__main__":
    unittest.main()
