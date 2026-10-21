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

ROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))

BAD = """\
#include <QListWidget>
auto* list = new QListWidget(this);                // lỗi
QTableWidget table;                                 // lỗi
void fill(QTreeWidget* tree);                       // lỗi
auto* item = new QTreeWidgetItem(tree);             // lỗi
list->addItem(new QListWidgetItem(name));           // lỗi
QTreeWidgetItemIterator it(tree);                   // lỗi
table.setItem(0, 0, new QTableWidgetItem);          // lỗi
// QListWidget trong chú thích không tính
/* QTreeWidget
   nhiều dòng */
auto* view = new QListView(this);
QTreeView tree2;
QTableView grid;
auto* modes = new QListWidget(this); // item-widget-ok: 3 chế độ cố định, không phải dữ liệu bản vẽ
auto* bare = new QListWidget(this); /* lỗi: thiếu lý do */ // item-widget-ok:
const char* s = "x // y"; QTreeWidget t2;            // lỗi
"""


def lines(errors):
    return {int(re.search(r":(\d+): ", e).group(1)) for e in errors}


def run(argv):
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        code = lint.main(["lint.py"] + argv)
    return code, out.getvalue()


class Lint(unittest.TestCase):
    def test_reports_exactly_the_marked_lines(self):
        want = {i for i, ln in enumerate(BAD.split("\n"), 1) if "lỗi" in ln}
        self.assertEqual(len(want), 9)
        self.assertEqual(lines(lint.lint("x.cpp", BAD)), want)

    def test_designer_ui_file(self):
        ui = '<ui version="4.0">\n <widget class="QTreeWidget" name="layers"/>\n <widget class="QTreeView"/>\n</ui>\n'
        self.assertEqual(lines(lint.lint("x.ui", ui)), {2})

    def test_walk_skips_tests_and_generated_and_sets_exit_code(self):
        tmp = tempfile.mkdtemp()
        try:
            for sub in ("ui", "ui/tests", "ui/generated"):
                os.makedirs(os.path.join(tmp, sub))
                with open(os.path.join(tmp, sub, "x.cpp"), "w", encoding="utf-8") as f:
                    f.write(BAD)
            code, out = run([tmp])
            self.assertEqual(code, 1)
            self.assertEqual({re.sub(r":\d+: .*", "", e) for e in out.splitlines()}, {os.path.join(tmp, "ui", "x.cpp")})
            os.remove(os.path.join(tmp, "ui", "x.cpp"))
            self.assertEqual(run([tmp]), (0, "no-item-widgets: sạch\n"))
        finally:
            shutil.rmtree(tmp)

    def test_app_is_clean(self):
        self.assertEqual(run([os.path.join(ROOT, "app")])[0], 0)


if __name__ == "__main__":
    unittest.main()
