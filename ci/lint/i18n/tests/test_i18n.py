import contextlib
import io
import os
import re
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import check_ts  # noqa: E402
import lint  # noqa: E402

SAMPLES = os.path.join(HERE, "samples")
LUPDATE = os.environ.get("LUPDATE", "")  # ctest đặt


def run(main, argv):
    """(mã thoát, stdout) của main(argv)."""
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        code = main(argv)
    return code, out.getvalue()


def read(name):
    path = os.path.join(SAMPLES, name)
    with open(path, encoding="utf-8") as f:
        return path, f.read()


class Lint(unittest.TestCase):
    def test_bad_sample_reports_exactly_the_marked_lines(self):
        path, src = read("bad.cpp")
        want = {i: m.group(1) for i, ln in enumerate(src.split("\n"), 1) if (m := re.search(r"//\s*lỗi \(([ab])\)", ln))}
        got = {int(re.search(r":(\d+): ", e).group(1)): "b" if "ghép" in e else "a" for e in lint.lint(path, src)}
        self.assertEqual(got, want)
        self.assertGreaterEqual(len(want), 19)  # dấu đánh trong mẫu vẫn được đọc

    def test_good_sample_is_clean(self):
        path, src = read("good.cpp")
        self.assertEqual(lint.lint(path, src), [])

    def test_exit_code_and_list_file(self):
        good, bad = os.path.join(SAMPLES, "good.cpp"), os.path.join(SAMPLES, "bad.cpp")
        with tempfile.NamedTemporaryFile("w", suffix=".lst", delete=False) as f:
            f.write(f"{good}\n{bad}\n")
        try:
            self.assertEqual(run(lint.main, ["lint.py", good])[0], 0)
            code, out = run(lint.main, ["lint.py", f"@{f.name}"])
            self.assertEqual((code, "bad.cpp:" in out, "good.cpp" in out), (1, True, False))
        finally:
            os.unlink(f.name)


SRC = """#include <QCoreApplication>
void f(int n)
{
    QCoreApplication::translate("Mau", "Lưu %1");
    QCoreApplication::translate("Mau", "%n tệp", nullptr, n);
}
"""


def msg(src, tr, kind=""):
    """<message>: tr là chuỗi, hoặc danh sách các dạng số nhiều."""
    t = f' type="{kind}"' if kind else ""
    if isinstance(tr, list):
        forms = "".join(f"<numerusform>{f}</numerusform>" for f in tr)
        return f'<message numerus="yes"><source>{src}</source><translation{t}>{forms}</translation></message>'
    return f"<message><source>{src}</source><translation{t}>{tr}</translation></message>"


def ts(lang, *msgs):
    return (f'<?xml version="1.0" encoding="utf-8"?><!DOCTYPE TS><TS version="2.1" language="{lang}" '
            f'sourcelanguage="vi"><context><name>Mau</name>{"".join(msgs)}</context></TS>')


SAVE, FILES = msg("Lưu %1", "Save %1"), msg("%n tệp", ["%n file", "%n files"])
VI = ts("vi", msg("%n tệp", ["%n tệp"]))


@unittest.skipUnless(os.path.isfile(LUPDATE), "cần biến môi trường LUPDATE (ctest đặt)")
class CheckTs(unittest.TestCase):
    def check(self, en, vi=VI, src=SRC):
        with tempfile.TemporaryDirectory() as d:
            files = {"mau.cpp": src, "trion_vi.ts": vi, "trion_en.ts": en}
            for name, text in files.items():
                with open(os.path.join(d, name), "w", encoding="utf-8") as f:
                    f.write(text)
            src, vi_ts, en_ts = (os.path.join(d, n) for n in files)
            code, out = run(check_ts.main, ["check_ts.py", LUPDATE, src, "-ts", vi_ts, en_ts])
            return [] if code == 0 else out.splitlines()

    def test_complete_translations_pass(self):
        self.assertEqual(self.check(ts("en", SAVE, FILES)), [])

    def test_each_defect_is_reported(self):
        cases = {
            "chưa dịch": ts("en", msg("Lưu %1", "", "unfinished"), FILES),
            "placeholder": ts("en", msg("Lưu %1", "Save"), FILES),
            "khoảng trắng": ts("en", msg("Lưu %1", "Save %1 "), FILES),
            "chưa có trong .ts": ts("en", SAVE),  # chuỗi mới, chưa chạy update_translations
            "update_translations": ts("en", msg("Lưu %1", "Save %1", "vanished"), FILES),  # .qm sẽ thiếu nó
        }
        for why, en in cases.items():
            with self.subTest(why):
                bad = self.check(en)
                self.assertEqual(len(bad), 1, bad)
                self.assertIn(why, bad[0])
                self.assertTrue(bad[0].startswith("trion_en.ts: Mau: "), bad[0])

    def test_tr_in_class_without_q_object_is_reported(self):
        src = SRC + "class NoQ : public QObject {\npublic:\n    void f() { tr(\"Lưu %1\"); }\n};\n"
        bad = self.check(ts("en", SAVE, FILES), src=src)
        self.assertTrue(any("lupdate:" in b and "Q_OBJECT" in b for b in bad), bad)

    def test_source_language_file_needs_its_plural_forms(self):
        bad = self.check(ts("en", SAVE, FILES), vi=ts("vi", msg("%n tệp", [""], "unfinished")))
        self.assertEqual(bad, ["trion_vi.ts: Mau: chưa dịch: '%n tệp'"])


if __name__ == "__main__":
    unittest.main()
