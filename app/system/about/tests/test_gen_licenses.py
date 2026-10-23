"""UIL-011 tiêu chí 2: gen_licenses.py — danh sách thật đạt (Qt LGPL-3.0 kèm GPL-3.0, liên kết động), CI báo lỗi khi
một phụ thuộc thiếu văn bản giấy phép (không khai báo, mã lạ, tệp không có, tệp rỗng), thiếu trường, id trùng; đầu ra
tất định và chỉ ghi khi đổi."""
import contextlib
import copy
import io
import json
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import gen_licenses as gl  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
SPEC = os.path.join(os.path.dirname(HERE), "third_party.json")


def load_spec():
    with open(SPEC, encoding="utf-8") as f:
        return json.load(f)


def run(argv):
    """(mã thoát, stderr) của main()."""
    err = io.StringIO()
    with contextlib.redirect_stderr(err):
        code = gl.main(argv)
    return code, err.getvalue()


class RealList(unittest.TestCase):
    def test_passes_and_covers_05_s2(self):
        out, errors = gl.build(load_spec(), ROOT)
        self.assertEqual(errors, [])
        ids = [c["id"] for c in out["components"]]
        for need in ("qt", "freetype", "harfbuzz", "zlib", "libdeflate", "pdfium", "libsodium", "crashpad", "plex"):
            self.assertIn(need, ids)
        for c in out["components"]:
            for t in c["texts"]:
                self.assertGreater(len(out["texts"][t].strip()), 100, f"{c['id']}: {t}")

    def test_qt_lgpl_dynamic(self):
        out, _ = gl.build(load_spec(), ROOT)
        qt = next(c for c in out["components"] if c["id"] == "qt")
        self.assertEqual(qt["spdx"], "LGPL-3.0-only")
        self.assertEqual(qt["link"], "dynamic")
        self.assertEqual(qt["texts"], ["LGPL-3.0-only", "GPL-3.0-only"])
        self.assertIn("GNU LESSER GENERAL PUBLIC LICENSE", out["texts"]["LGPL-3.0-only"])
        self.assertIn("GNU GENERAL PUBLIC LICENSE", out["texts"]["GPL-3.0-only"])

    def test_check_mode(self):
        self.assertEqual(run([SPEC, "--check", "--root", ROOT]), (0, ""))


class MissingText(unittest.TestCase):
    """CI báo lỗi khi một phụ thuộc thiếu văn bản giấy phép."""

    def broken(self, edit):
        spec = copy.deepcopy(load_spec())
        edit(spec)
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "third_party.json")
            with open(path, "w", encoding="utf-8") as f:
                json.dump(spec, f, ensure_ascii=False)
            out = os.path.join(d, "licenses.json")
            code, err = run([path, out, "--root", ROOT])
            self.assertFalse(os.path.exists(out), "lỗi thì không ghi đầu ra")
        self.assertEqual(code, 1)
        return err

    def test_no_texts(self):
        err = self.broken(lambda s: s["components"][2].pop("texts"))
        self.assertIn("harfbuzz: thiếu văn bản giấy phép", err)

    def test_unknown_text_id(self):
        err = self.broken(lambda s: s["components"][1].update(texts=["FTL-2"]))
        self.assertIn('freetype: thiếu văn bản giấy phép "FTL-2"', err)

    def test_missing_file(self):
        err = self.broken(lambda s: s["texts"].update({"ISC": "app/system/about/licenses/khong-co.txt"}))
        self.assertIn('libsodium: thiếu văn bản giấy phép "ISC": không đọc được', err)

    def test_empty_file(self):
        with tempfile.NamedTemporaryFile("w", suffix=".txt", dir=os.path.dirname(SPEC), delete=False) as f:
            f.write("  \n")
        try:
            rel = os.path.relpath(f.name, ROOT)
            err = self.broken(lambda s: s["texts"].update({"MIT": rel}))
        finally:
            os.unlink(f.name)
        self.assertIn('libdeflate: văn bản giấy phép "MIT" rỗng', err)

    def test_fields_and_duplicates(self):
        err = self.broken(lambda s: (s["components"][0].pop("notice"), s["components"].append(s["components"][3])))
        self.assertIn('qt: thiếu trường "notice"', err)
        self.assertIn("zlib: id trùng", err)

    def test_bad_link(self):
        err = self.broken(lambda s: s["components"][0].update(link="plugin"))
        self.assertIn('qt: link "plugin" không hợp lệ', err)


class Output(unittest.TestCase):
    def test_deterministic_and_written_once(self):
        with tempfile.TemporaryDirectory() as d:
            out = os.path.join(d, "sub", "licenses.json")
            self.assertEqual(run([SPEC, out, "--root", ROOT])[0], 0)
            with open(out, encoding="utf-8") as f:
                first = f.read()
            data = json.loads(first)
            self.assertEqual(data["format"], "trion-licenses/1")
            self.assertEqual(list(data["texts"]), sorted(data["texts"]))
            os.utime(out, (1, 1))
            self.assertEqual(run([SPEC, out, "--root", ROOT])[0], 0)
            self.assertEqual(os.stat(out).st_mtime, 1, "nội dung không đổi thì không ghi lại")
            with open(out, encoding="utf-8") as f:
                self.assertEqual(f.read(), first)


if __name__ == "__main__":
    unittest.main()
