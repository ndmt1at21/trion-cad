import io
import os
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import iconlint  # noqa: E402

DATA = Path(__file__).with_name("data")


class IconLint(unittest.TestCase):
    def test_good_file_passes(self):
        self.assertEqual(iconlint.lint(DATA / "good.svg"), [])

    def test_bad_file_reports_each_violation(self):
        errs = iconlint.lint(DATA / "bad.svg")
        for frag in ('gốc: viewBox phải là "0 0 16 16"', 'gốc: stroke-linecap phải là "square"',
                     "gốc: thuộc tính không cho phép width", "phần tử 1 <ellipse>: thẻ không cho phép",
                     'fill="#e33" là màu viết thẳng', 'stroke="rgb(0,0,0)" là màu viết thẳng',
                     "thuộc tính không cho phép class", "thuộc tính không cho phép style",
                     "phần tử 6 <path>: thuộc tính không cho phép stroke-linecap", "phần tử 7 <rect>: thiếu height",
                     'stroke-width="0" phải là số dương', 'stroke-dasharray="1 x" phải là dãy số',
                     "phần tử 10 <path>: không được có phần tử con", "phần tử 11 <g>: thẻ không cho phép"):
            self.assertTrue(any(frag in e for e in errs), frag)
        self.assertEqual(len(errs), 15)

    def test_main_prefixes_file_name_and_fails(self):
        out = io.StringIO()
        with redirect_stdout(out):
            code = iconlint.main([str(DATA)])
        self.assertEqual(code, 1)
        self.assertIn(str(DATA / "bad.svg") + ": gốc:", out.getvalue())
        self.assertNotIn(str(DATA / "good.svg") + ":", out.getvalue())
        with redirect_stdout(io.StringIO()):
            self.assertEqual(iconlint.main([str(DATA / "good.svg")]), 0)

    def test_unreadable_file_and_empty_dir(self):
        with tempfile.TemporaryDirectory() as tmp:
            broken = Path(tmp, "hong.svg")
            broken.write_text("<svg", encoding="utf-8")
            self.assertIn("không đọc được", iconlint.lint(broken)[0])
            Path(tmp, "rong").mkdir()
            with redirect_stderr(io.StringIO()):
                self.assertEqual(iconlint.main([str(Path(tmp, "rong"))]), 1)


if __name__ == "__main__":
    unittest.main()
