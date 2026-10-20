import filecmp
import json
import os
import re
import shutil
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import coverage_report  # noqa: E402
import iconconv  # noqa: E402

DOTS_24 = 'style="stroke-dasharray:0 2.4;stroke-linecap:round;stroke-width:1.5"'


class ToSvg(unittest.TestCase):
    def test_classes_style_ellipse(self):
        svg = iconconv.to_svg('<circle class="dot" cx="3" cy="8" r="1"/><path class="dash" d="M1 1h4"/>'
                              '<path class="f" d="M2 2h3v3z" fill="currentColor"/><path d="M1 3h2" style="stroke-width:2"/>'
                              '<ellipse cx="8" cy="8" rx="6.2" ry="3.9"/>').splitlines()
        self.assertEqual(svg[0], iconconv.SVG_OPEN)
        self.assertEqual(svg[1:], [
            '  <circle cx="3" cy="8" r="1" fill="currentColor" stroke="none"/>',
            '  <path d="M1 1h4" stroke-dasharray="1.6 1.7"/>',
            '  <path d="M2 2h3v3z" fill="none"/>',  # lớp đè thuộc tính trình bày, như CSS
            '  <path d="M1 3h2" stroke-width="2"/>',
            '  <path d="M1.8 8a6.2 3.9 0 1 0 12.4 0a6.2 3.9 0 1 0 -12.4 0z"/>',
            "</svg>"])

    def test_unknown_class_fails(self):
        with self.assertRaises(KeyError):
            iconconv.to_svg('<path class="moi" d="M0 0h1"/>')

    def test_dotted_lines_become_dots_like_chromium(self):
        # 5 × 2,4f vượt 12 nên không có chấm ở điểm cuối; 5 × 2,6f chưa tới 13 nên có (khớp ảnh chụp Edge).
        a = iconconv.to_svg('<path d="M2 2h12" %s/>' % DOTS_24)
        self.assertEqual(re.findall(r'cx="([\d.]+)"', a), ["2", "4.4", "6.8", "9.2", "11.6"])
        self.assertIn('cy="2" r="0.75" fill="currentColor" stroke="none"/>', a)
        b = iconconv.to_svg('<path d="M8 1.5v13" class="dots"/>')
        self.assertEqual(re.findall(r'cy="([\d.]+)"', b), ["1.5", "4.1", "6.7", "9.3", "11.9", "14.5"])
        self.assertIn('r="0.875"', b)
        # đường đóng: chấm chạy tiếp qua góc, không lặp chấm đầu ở cuối vòng
        c = re.findall(r'cx="([\d.]+)" cy="([\d.]+)"', iconconv.to_svg('<path d="M2 2h12v12H2z" %s/>' % DOTS_24))
        self.assertEqual(len(c), 20)
        self.assertEqual(len(set(c)), 20)
        self.assertIn(("14", "14"), c)

    def test_polylines(self):
        self.assertEqual(iconconv.polylines("m1 1h2v2l-1 0zM5 5 6 6"),
                         [[(1, 1), (3, 1), (3, 3), (2, 3), (1, 1)], [(5, 5), (6, 6)]])
        with self.assertRaises(ValueError):
            iconconv.polylines("M0 0a1 1 0 0 1 2 0")


class Groups(unittest.TestCase):
    def test_comment_rules(self):
        js = ("TC.ICONS = {\n  // nhóm một\n  a:'<path d=\"M0 0\"/>', b:'<path/>',\n"
              "  // ==== Đợt 4 · screens9.js · chú thích, bảng ====\n  // mô tả ngay sau tiêu đề, không phải nhóm\n"
              "  c:'<path/>',\n  // nhóm con\n  d:'<path/>',\n  // ==== hết screens9.js ====\n};\n")
        self.assertEqual(iconconv.groups(js), {"a": "nhóm một", "b": "nhóm một", "c": "chú thích, bảng", "d": "nhóm con"})


class Coverage(unittest.TestCase):
    def test_viewer_is_error_rest_is_warning(self):
        proto = {"positions": [
            {"where": "Ribbon Viewer › Xem", "cmd": "PAN", "icon": "pan", "viewer": True},
            {"where": "Ribbon Viewer › Xem", "cmd": "ZOOM", "icon": "zoom", "viewer": True},
            {"where": "Menu › Vẽ", "cmd": "SOLID", "icon": "", "viewer": False}],
            "viewerIcons": ["pan", "zoom"], "frameIcons": ["logo"]}
        errors, warnings = coverage_report.report(proto, {"pan", "logo"})
        self.assertEqual(errors, ["Ribbon Viewer › Xem: ZOOM thiếu biểu tượng zoom.svg", "bộ Viewer: thiếu zoom.svg"])
        self.assertEqual(warnings, ["Menu › Vẽ: SOLID thiếu biểu tượng"])


@unittest.skipUnless(shutil.which("node"), "cần node để đọc JS của prototype")
class Repo(unittest.TestCase):
    """Tệp trong kho đúng bằng kết quả chạy lại converter trên prototype hiện tại (tiêu chí 1, 4)."""

    def test_rerun_matches_repo_bytes(self):
        repo = iconconv.ICONS_DIR
        with tempfile.TemporaryDirectory() as tmp:
            Path(tmp, "src").mkdir()
            Path(tmp, "src", "da-bo.svg").write_text("cũ", encoding="utf-8")
            with redirect_stdout(StringIO()):
                self.assertEqual(iconconv.main(["--out", tmp]), 0)
            names = sorted(p.name for p in Path(tmp, "src").glob("*.svg"))
            self.assertEqual(names, sorted(p.name for p in (repo / "src").glob("*.svg")))  # SVG thừa đã bị xóa
            self.assertEqual(filecmp.cmpfiles(str(Path(tmp, "src")), str(repo / "src"), names, shallow=False)[1:], ([], []))
            self.assertTrue(filecmp.cmp(str(Path(tmp, "manifest.json")), str(repo / "manifest.json"), shallow=False))

    def test_manifest_covers_prototype_and_viewer_set(self):
        proto = iconconv.load_proto()
        icons = json.loads((iconconv.ICONS_DIR / "manifest.json").read_text(encoding="utf-8"))["icons"]
        self.assertEqual([i["name"] for i in icons], list(proto["icons"]))
        self.assertTrue(all(i["group"] for i in icons))
        viewer = set(proto["viewerIcons"]) | set(proto["frameIcons"])
        self.assertEqual({i["name"] for i in icons if i["viewer"]}, viewer)  # mọi biểu tượng Viewer đều có (100%)


if __name__ == "__main__":
    unittest.main()
