import contextlib
import io
import os
import re
import sys
import tempfile
import unicodedata
import unittest
from xml.sax.saxutils import escape

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import glossary  # noqa: E402

TSV = os.path.join(HERE, "..", "..", "..", "..", "app", "i18n", "glossary.tsv")


def msg(ctx, src, tr, extra="", translator="", kind=""):
    """Một <context> một <message>; tr là chuỗi hoặc danh sách dạng số nhiều."""
    t = f' type="{kind}"' if kind else ""
    body = ("".join(f"<numerusform>{escape(f)}</numerusform>" for f in tr) if isinstance(tr, list) else escape(tr))
    num = ' numerus="yes"' if isinstance(tr, list) else ""
    return (f"<context><name>{ctx}</name><message{num}><source>{escape(src)}</source>"
            + (f"<extracomment>{escape(extra)}</extracomment>" if extra else "")
            + (f"<translatorcomment>{escape(translator)}</translatorcomment>" if translator else "")
            + f"<translation{t}>{body}</translation></message></context>")


def ts(lang, src_lang, *msgs):
    return (f'<?xml version="1.0" encoding="utf-8"?><!DOCTYPE TS><TS version="2.1" language="{lang}" '
            f'sourcelanguage="{src_lang}">{"".join(msgs)}</TS>')


# Ngữ cảnh "loi-<thuật ngữ>-…" phải bị báo đúng thuật ngữ đó, "sach-…" không được báo.
EN = ts("en", "vi",
        msg("loi-Layer-a", "Ẩn lớp đang chọn", "Hide selected layer"),
        msg("loi-Layer-nfd", unicodedata.normalize("NFD", "Khóa Lớp"), "Lock layer"),  # gõ bằng bảng mã tổ hợp
        msg("loi-Lineweight-a", "Độ rộng nét", "Lineweight"),
        msg("loi-Lineweight-raw", "Lineweight mặc định", "Default lineweight"),
        msg("loi-Lineweight-en", "Độ dày nét", "Line width"),
        msg("loi-Block-mnemonic", "&Khối", "&Block"),
        msg("loi-Block-plural", "%n khối", ["%n block", "%n blocks"]),
        msg("loi-Layer-plural-en", "%n layer", ["%n layer", "%n levels"]),
        msg("loi-Viewport-a", "Khung nhìn", "Viewport"),
        msg("loi-Properties-en", "Thuộc tính nhanh", "Quick info"),
        msg("sach-paren", "Độ dày nét (Lineweight)", "Lineweight"),
        msg("sach-command", "Thuộc tính (PROPERTIES)", "Properties (PROPERTIES)"),
        msg("sach-attribute", "Attribute (thuộc tính block)", "Attribute (block attribute)"),
        msg("sach-ok-extra", "Lớp phủ", "Overlay", extra="glossary-ok: lớp phủ (overlay), không phải Layer"),
        msg("sach-ok-translator", "Độ dày nét", "Pen width", translator="glossary-ok: thử ngoại lệ của người dịch"),
        msg("sach-vanished", "Lớp", "Layer", kind="vanished"),
        msg("sach-unfinished", "Độ dày nét", "", kind="unfinished"),
        msg("sach-size", "Kích thước tệp", "File size"),
        msg("sach-path", "Đường dẫn tệp", "File path"),
        msg("sach-linetype", "Kiểu đường (Linetype)", "Linetype"),
        msg("sach-osnap", "Bật/tắt bắt điểm (OSNAP)", "Toggle object snap (OSNAP)"),
        msg("sach-history", "Lịch sử dòng lệnh", "Command history"))
# Bản dịch tiếng Việt của chuỗi nguồn tiếng Anh (như qtbase_vi.ts): chỉ kiểm bản dịch.
VI = ts("vi", "en",
        msg("loi-Viewport-qt", "Viewport", "Khung nhìn"),
        msg("sach-qt", "Cancel", "Hủy"),
        msg("sach-qt-source", "Layer", "Layer"))


def run(*files):
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        code = glossary.main(["glossary.py", TSV, *files])
    return code, out.getvalue().splitlines()


class Glossary(unittest.TestCase):
    def test_planted_errors_reported_exactly(self):
        with tempfile.TemporaryDirectory() as d:
            files = {"mau_en.ts": EN, "mau_vi.ts": VI}
            for name, text in files.items():
                with open(os.path.join(d, name), "w", encoding="utf-8") as f:
                    f.write(text)
            code, lines = run(*(os.path.join(d, n) for n in files))
        got = {(ln.split(": ")[1], re.search(r": ([A-Za-z ]+) phải là", ln).group(1)) for ln in lines}
        want = {(c, c.split("-")[1]) for c in re.findall(r"<name>(loi-[^<]+)</name>", EN + VI)}
        self.assertEqual(code, 1)
        self.assertEqual(got, want)
        self.assertEqual(len(want), 11)
        self.assertEqual(len(lines), len(want), lines)  # mỗi ca một lỗi, không báo trùng

    def test_table_compiles_and_names_every_term_of_section_13_4(self):
        rows = glossary.load(TSV)
        terms = {r["term"] for r in rows}
        for t in ("Layer", "Properties", "Block", "Attribute", "Layout", "Model space", "Paper space", "Viewport",
                  "Linetype", "Lineweight", "Hatch", "Dimension", "Leader", "Multileader", "Text style", "Dim style",
                  "Plot", "Plot style", "Page setup", "Xref", "Snap", "Object snap", "Tracking", "Grip", "Purge",
                  "Audit", "Recover", "Annotative", "Annotation scale", "Template", "Workspace", "Command line"):
            self.assertIn(t, terms)
        for r in rows:  # nhãn đúng không tự khớp cột nhãn sai của chính nó
            if r["wrong_vi"]:
                label = glossary.COMMAND.sub(" ", glossary.plain(r["label"]))
                self.assertIsNone(r["wrong_vi"].search(label), r["term"])


if __name__ == "__main__":
    unittest.main()
