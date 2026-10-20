import contextlib
import io
import os
import re
import shutil
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import lint  # noqa: E402

SAMPLES = os.path.join(HERE, "samples")
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def run(argv):
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        code = lint.main(["lint.py"] + argv)
    return code, out.getvalue()


def lines(errors):
    return {int(re.search(r":(\d+): ", e).group(1)) for e in errors}


class Lint(unittest.TestCase):
    def test_bad_cpp_reports_exactly_the_marked_lines(self):
        path = os.path.join(SAMPLES, "bad.cpp")
        src = read(path)
        want = {i for i, ln in enumerate(src.split("\n"), 1) if re.search(r"//\s*lỗi\s*$", ln)}
        self.assertGreaterEqual(len(want), 25)  # dấu trong mẫu vẫn được đọc
        self.assertEqual(lines(lint.lint(path, src)), want)

    def test_bad_qss_every_rule_line(self):
        path = os.path.join(SAMPLES, "bad.qss")
        self.assertEqual(lines(lint.lint(path, read(path))), set(range(2, 7)))

    def test_good_cpp_is_clean(self):
        path = os.path.join(SAMPLES, "good.cpp")
        self.assertEqual(lint.lint(path, read(path)), [])

    def test_qss_template_uses_only_tokens(self):
        path = os.path.join(ROOT, "app", "theme", "qss", "app.qss.in")
        src = read(path)
        self.assertIn("var(--c-sunken)", src)
        self.assertEqual(lint.lint(path, src), [])
        # sửa một giá trị thành số viết thẳng thì lint bắt
        self.assertEqual(len(lint.lint(path, src.replace("var(--sp-4)", "8px", 1))), 1)
        self.assertEqual(len(lint.lint(path, src.replace("var(--c-sunken)", "#15171A", 1))), 1)

    def test_walk_skips_tests_and_generated_and_sets_exit_code(self):
        tmp = tempfile.mkdtemp()
        try:
            for sub in ("ui", "ui/tests", "theme/generated"):
                os.makedirs(os.path.join(tmp, sub))
                shutil.copy(os.path.join(SAMPLES, "bad.cpp"), os.path.join(tmp, sub, "x.cpp"))
            code, out = run([tmp])
            self.assertEqual(code, 1)
            self.assertEqual({re.sub(r":\d+: .*", "", e) for e in out.splitlines()}, {os.path.join(tmp, "ui", "x.cpp")})
            os.remove(os.path.join(tmp, "ui", "x.cpp"))
            self.assertEqual(run([tmp]), (0, "no-literal-style: sạch\n"))
        finally:
            shutil.rmtree(tmp)


if __name__ == "__main__":
    unittest.main()
