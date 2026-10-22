#!/usr/bin/env python3
"""Lint chuỗi giao diện trong app/ (UI0-104, DoD-U 6). ctest i18n_lint; PR lỗi nếu có vi phạm.

(a) Chuỗi hiển thị viết thẳng, không qua tr() / QCoreApplication::translate(), làm đối số trực tiếp của
    setText, setToolTip, setWindowTitle, setPlaceholderText, setStatusTip…, QAction(…), QLabel(…),
    QMessageBox::warning(…)… Chuỗi rỗng hoặc không có chữ cái ("%1", "…", ":") được phép.
(b) Ghép chuỗi hiển thị bằng + (hoặc %): tr("Đã chọn ") + n → tr("Đã chọn %1").arg(n), số nhiều %n.
Ngoại lệ có lý do: chú thích `// i18n-ok: <lý do>` trên dòng bị báo.
Dùng: lint.py <tệp.cpp|tệp.h|@danh sách>...  → in "tệp:dòng: ...", mã thoát 1 nếu có vi phạm.
"""
import bisect
import re
import sys

# Lệnh nhận chuỗi hiển thị. ponytail: danh sách tay, khớp theo tên; thêm khi gặp API mới.
CALL = re.compile(r"""
    \b(?P<name>set(?:Text|ToolTip|WindowTitle|PlaceholderText|StatusTip|WhatsThis|Title|LabelText|TabText
                     |TabToolTip|ItemText|AccessibleName|AccessibleDescription|InformativeText|DetailedText
                     |HeaderLabels?|Confirm)
              |add(?:Action|Menu|Tab|Items?)|insert(?:Items?|Tab)|showMessage
              |QMessageBox\s*::\s*(?:information|warning|critical|question|about))\s*\(
  | \b(?P<cls>QAction|QLabel|QPushButton|QCheckBox|QRadioButton|QGroupBox|QMenu|QDockWidget|QMessageBox
             |QCommandLinkButton|QListWidgetItem|QTreeWidgetItem|QStandardItem)(?:\s+\w+)?\s*\(
""", re.X)
TR = re.compile(r"(?:\b\w+\s*::\s*)*\b(?:tr|translate|qtTrId)\s*\(")  # kết quả là chuỗi đã dịch
CHAIN = re.compile(r"\s*\.\s*\w+\s*\(")  # .arg(…) nối sau tr(…)
# Bọc không đổi bản chất chuỗi viết thẳng (QT_TR_NOOP chỉ đánh dấu, không dịch)
WRAP = {"QStringLiteral", "QString", "QLatin1String", "QLatin1StringView", "QStringView", "QStringList",
        "fromUtf8", "fromLatin1", "QT_TR_NOOP", "QT_TRANSLATE_NOOP"}
OK = re.compile(r"i18n-ok:\s*\S")


def scan(src):
    """Trả (code, lits): code là src với chú thích và nội dung chuỗi/ký tự thay bằng dấu cách (giữ vị trí và
    dòng), lits[vị trí dấu " mở] = nội dung chuỗi."""
    code, lits, i, n = list(src), {}, 0, len(src)

    def blank(a, b):
        for k in range(a, b):
            if code[k] != "\n":
                code[k] = " "

    while i < n:
        c = src[i]
        if src.startswith("//", i) or src.startswith("/*", i):
            j = src.find("\n", i) if src[i + 1] == "/" else src.find("*/", i + 2) + 2
            j = n if j < 2 else j  # không thấy dấu kết thúc
            blank(i, j)
        elif c == '"' and i and src[i - 1] == "R":  # R"tách(…)tách"
            k = src.find("(", i)
            end = src.find(")" + src[i + 1:k] + '"', k) if k > 0 else -1
            j = n if end < 0 else end + k - i  # dấu " đóng
            lits[i] = src[k + 1:end] if end > 0 else ""
            blank(i + 1, j)
            j += 1
        elif c == '"' or (c == "'" and not src[i - 1:i].isdigit()):  # 1'000: dấu tách chữ số
            j = i + 1
            while j < n and src[j] not in (c, "\n"):
                j += 2 if src[j] == "\\" else 1
            if c == '"':
                lits[i] = src[i + 1:j]
            blank(i + 1, j)
            j += 1
        else:
            j = i + 1
        i = j
    return "".join(code), lits


def close(code, k):
    """Vị trí ngay sau dấu đóng khớp với dấu mở ở code[k]."""
    depth = 0
    for j in range(k, len(code)):
        if code[j] in "([{":
            depth += 1
        elif code[j] in ")]}":
            depth -= 1
            if depth == 0:
                return j + 1
    return len(code)


def before(code, k, n=1):
    """n ký tự không phải dấu cách ngay trước code[k]."""
    return code[max(0, k - 200):k].rstrip()[-n:]


def raw_args(code, lits, paren):
    """Chuỗi viết thẳng là đối số trực tiếp của lệnh có dấu mở ở paren: kể cả qua bọc WRAP, ngoặc nhóm, {…};
    bỏ chuỗi nằm trong lời gọi khác (tr, arg, QIcon…), trong [] và thân lambda."""
    stack = []  # mỗi dấu mở bên trong: True nếu trong suốt
    for j in range(paren + 1, close(code, paren) - 1):
        ch = code[j]
        if ch in "([{":
            name = re.search(r"\w*$", before(code, j, 80)).group(0)
            if ch == "[":
                stack.append(False)
            elif name:
                stack.append(name in WRAP)
            else:  # ngoặc nhóm, ép kiểu, {…}; nhưng {…} sau ) hoặc ] là thân lambda
                stack.append(not (ch == "{" and before(code, j) in (")", "]")))
        elif ch in ")]}":
            if stack:
                stack.pop()
        elif ch == '"' and j in lits and all(stack) and any(c.isalpha() for c in re.sub(r"\\.", "", lits[j])):
            yield j, lits[j]


def lint(path, src):
    code, lits = scan(src)
    lines = src.split("\n")
    starts = [0] + [m.end() for m in re.finditer("\n", src)]
    found = {}  # dòng → lỗi đầu tiên
    for m in CALL.finditer(code):
        what = re.sub(r"\s+", "", m.group("name") or m.group("cls"))
        for pos, text in raw_args(code, lits, m.end() - 1):
            found.setdefault(bisect.bisect_right(starts, pos),
                             f'chuỗi "{text}" viết thẳng trong {what}(…): bọc bằng tr() (ngoại lệ: // i18n-ok: <lý do>)')
    for m in TR.finditer(code):
        end = close(code, m.end() - 1)
        while c := CHAIN.match(code, end):
            end = close(code, c.end() - 1)
        after = code[end:end + 200].lstrip()[:2]
        prev = before(code, m.start(), 2)
        if (after[:1] in ("+", "%") and after not in ("++", "+=")) or prev[-1:] in ("+", "%") or prev == "+=":
            found.setdefault(bisect.bisect_right(starts, m.start()),
                             "ghép chuỗi hiển thị bằng +: dùng một chuỗi tr() với %1, số nhiều %n")
    return [f"{path}:{ln}: {msg}" for ln, msg in sorted(found.items()) if not OK.search(lines[ln - 1])]


def paths(args):
    for a in args:
        if a.startswith("@"):
            with open(a[1:], encoding="utf-8") as f:
                yield from (ln.strip() for ln in f if ln.strip())
        else:
            yield a


def main(argv):
    bad = []
    for p in paths(argv[1:]):
        if p.endswith((".cpp", ".h", ".hpp", ".cc", ".cxx")):
            with open(p, encoding="utf-8") as f:
                bad += lint(p, f.read())
    print("\n".join(bad) or "i18n lint: sạch")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
