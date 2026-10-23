#!/usr/bin/env python3
"""UIL-011 · Sinh licenses.json cho hộp Giới thiệu (ABOUT, UI-L13) từ danh sách thư viện bên thứ ba.

Đầu vào: third_party.json — "texts" (mã văn bản → đường dẫn từ gốc kho) và "components" (id, name, license hiện trên
bảng, spdx, texts, url, notice bản quyền; link khi giấy phép đòi, ví dụ Qt "dynamic").
ponytail: danh sách tĩnh theo 05 §2 tới khi có vcpkg manifest (NT-001); khi có, đọc vcpkg.json và văn bản
share/<port>/copyright của vcpkg_installed (đúng phiên bản) thay cho tệp này, giữ nguyên đầu ra.

Đầu ra: licenses.json {"format": "trion-licenses/1", "components": [...], "texts": {mã: văn bản}} (UTF-8, thứ tự
ổn định: cùng nguồn ra cùng byte), chỉ ghi lại khi nội dung đổi. AboutDialog đọc ở :/trion/about/licenses.json.

Lỗi — thành phần thiếu văn bản giấy phép (không khai báo, mã không có trong "texts", tệp không có hoặc rỗng), thiếu
trường, id trùng, link lạ: in từng lỗi "<tệp>: <id>: …" ra stderr, mã thoát 1 (bước build dừng, CI báo).

Dùng: gen_licenses.py <third_party.json> <licenses.json> --root <gốc kho>
      gen_licenses.py <third_party.json> --check --root <gốc kho>      (chỉ kiểm)
"""
import argparse
import json
import os
import sys

FORMAT = "trion-licenses/1"
REQUIRED = ("id", "name", "license", "spdx", "url", "notice")
LINKS = ("dynamic", "static")


def build(spec, root, source="third_party.json"):
    """Trả (licenses, lỗi). licenses là dict đầu ra; lỗi là danh sách chuỗi (rỗng = đạt)."""
    errors = []
    paths = spec.get("texts")
    comps = spec.get("components")
    if not isinstance(paths, dict) or not isinstance(comps, list) or not comps:
        return None, [f"{source}: cần \"texts\" (đối tượng) và \"components\" (danh sách không rỗng)"]
    texts, seen, out = {}, set(), []
    for i, c in enumerate(comps):
        cid = c.get("id") if isinstance(c, dict) else None
        where = f"{source}: {cid or f'components[{i}]'}"
        if not isinstance(c, dict):
            errors.append(f"{where}: phải là đối tượng")
            continue
        for k in REQUIRED:
            if not isinstance(c.get(k), str) or not c[k].strip():
                errors.append(f"{where}: thiếu trường \"{k}\"")
        if cid in seen:
            errors.append(f"{where}: id trùng")
        seen.add(cid)
        link = c.get("link", "")
        if link and link not in LINKS:
            errors.append(f"{where}: link \"{link}\" không hợp lệ (dynamic, static)")
        ids = c.get("texts")
        if not isinstance(ids, list) or not ids:
            errors.append(f"{where}: thiếu văn bản giấy phép (\"texts\" rỗng)")
            ids = []
        for t in ids:
            if t not in paths:
                errors.append(f"{where}: thiếu văn bản giấy phép \"{t}\" (không có trong \"texts\")")
                continue
            if t in texts:
                continue
            path = os.path.join(root, paths[t])
            try:
                with open(path, encoding="utf-8") as f:
                    body = f.read()
            except (OSError, UnicodeDecodeError) as e:
                errors.append(f"{where}: thiếu văn bản giấy phép \"{t}\": không đọc được {paths[t]} ({e.__class__.__name__})")
                continue
            if not body.strip():
                errors.append(f"{where}: văn bản giấy phép \"{t}\" rỗng: {paths[t]}")
                continue
            texts[t] = body.replace("\r\n", "\n")
        item = {k: c.get(k, "") for k in REQUIRED}
        item["texts"] = list(ids)
        if link:
            item["link"] = link
        out.append(item)
    if errors:
        return None, errors
    return {"format": FORMAT, "components": out, "texts": dict(sorted(texts.items()))}, []


def render(licenses):
    return json.dumps(licenses, ensure_ascii=False, indent=1) + "\n"


def write_if_changed(path, content):
    try:
        with open(path, encoding="utf-8") as f:
            if f.read() == content:
                return False
    except OSError:
        pass
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(content)
    return True


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("spec", help="third_party.json")
    ap.add_argument("out", nargs="?", help="licenses.json cần ghi")
    ap.add_argument("--root", required=True, help="gốc kho (đường dẫn văn bản tính từ đây)")
    ap.add_argument("--check", action="store_true", help="chỉ kiểm, không ghi")
    a = ap.parse_args(argv)
    if not a.check and not a.out:
        ap.error("cần đường dẫn licenses.json hoặc --check")
    try:
        with open(a.spec, encoding="utf-8") as f:
            spec = json.load(f)
    except (OSError, ValueError) as e:
        print(f"{a.spec}: không đọc được: {e}", file=sys.stderr)
        return 1
    licenses, errors = build(spec, a.root, os.path.basename(a.spec))
    for e in errors:
        print(e, file=sys.stderr)
    if errors:
        return 1
    if not a.check:
        write_if_changed(a.out, render(licenses))
    return 0


if __name__ == "__main__":
    sys.exit(main())
