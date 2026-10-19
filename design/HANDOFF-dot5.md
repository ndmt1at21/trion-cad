# Bàn giao · Đợt 5 (sau v1.0) · thiết kế UI TrionCAD

Viết 19/10/2026. Người nhận: phiên Claude kế tiếp nếu phiên này bị ngắt. Xóa tệp này khi Đợt 5 xong.

## Phạm vi
07.3 Phụ lục B, Đợt 5: 12 mã F10, F15, G05, H08, H10, I08, I09, I10, J14, K07, K09, K10; tab Tham số (07.1 §7.5); tab Mở rộng phần P2 (07.1 §7.10). Không làm các nhóm ở 07.3 §8.

## Lead đã làm (nền chung)
- `cmd.js`: RAW thêm 82 lệnh Đợt 5 (wave 5, need PRO).
- `ribbons.js`:
  - tab Tham số mới;
  - bỏ `p2:1` của các lệnh Đợt 5;
  - Hiển thị › Palette thêm Tool Palettes, DesignCenter, Máy tính nhanh;
  - Quản lý: Chuẩn .dws (menu), Làm sạch hình học, Sức khỏe bản vẽ;
  - tab Mở rộng đủ 8 panel;
  - menu cổ điển Tham số, Mở rộng, Công cụ.
- `shell.js`: nút `infer` trên thanh trạng thái. `screens14.js`: phím Ctrl+4 SHEETSET, Ctrl+8 QUICKCALC, Ctrl+Shift+I infer.
- `editor.js`: `TC.gripsExt` (grip có `drag`, `click`, `menu`, `draw`). `draw.js`: `TC.explodeExt`.
- `index.html` nạp `screens15–21.js`; `app.css`, `icons.js` có dòng đánh dấu cho từng tệp.
- `catalog.js`:
  - gộp `SC.open5`;
  - mục ngoài mã `UI-PAR`, `UI-MR2` (st rỗng, đổi `d` khi xong);
  - nhóm Đợt 5 hiện mục ngoài mã.
- Giao việc chung: `scratchpad/BRIEF-dot5.md` của phiên này (bản sao nội dung chính ở bảng dưới).

## Chia việc (agent nền, chạy từ 19/10/2026)
| Nhóm | Tệp | Phạm vi | Agent |
|---|---|---|---|
| A | screens15.js | tab Tham số, `UI-PAR` | a018e927c32bc0d50 |
| B | screens16.js (+ screens10.js, screens7.js) | G05, phần P2 của G04 và G01 | ad03647bf09b81dd9 |
| C | screens17.js (+ screens11.js, screens2.js hộp In) | H08 (mở rộng palette TCSHEETS, không làm palette thứ hai), H10, J14 | aca4d79804aee1c37 |
| D | screens18.js | I08 (+ Sức khỏe bản vẽ), I09 (+ STANDARDS, CHECKSTANDARDS, LAYTRANS) | aa78b9ed7a5a328dc |
| E | screens19.js (+ ui.js numField, annot.js, screens2.js Xuất PDF) | I10, F10, F15 | afe5ca04d98ab92a5 |
| F | screens20.js | K07, K09, K10 | a7d998d9459422b4f |
| G | screens21.js | tab Mở rộng P2 (29 lệnh), `UI-MR2` | a0d01bee903f5b1ea |

Phiên khác vẫn gửi `SendMessage` tới các ID trên được (chạy tiếp từ transcript `~/.claude/projects/-home-ndmt1at21-trion-cad/39787da2-…/subagents/agent-<id>.jsonl`).

## Việc còn lại của lead
1. Nhận báo cáo, xem ảnh (`scratchpad/shots/5A…5G`), áp yêu cầu sửa tệp lead.
2. Kiểm biểu tượng ribbon đã vẽ đủ:
   - A: gcoin … infer (26 tên);
   - C: rebar;
   - D: geoclean;
   - E: calc, link;
   - F: lisp;
   - G: breakline.
3. Ghép, kiểm cú pháp mọi tệp, `catrun.js` phải in `no errors`.
4. `catalog.js`: 12 mã Đợt 5 và `UI-PAR`, `UI-MR2` sang `d`.
5. README:
   - bảng Tiến độ (Đợt 5: 12/12);
   - Cấu trúc (screens15–21);
   - quy ước grip/explode mở rộng.
6. Styleguide: thêm mục Đợt 5 (thanh ràng buộc, ký hiệu tham số dynamic block, lưới vấn đề làm sạch…), chụp kiểm Tối/Sáng.
7. Đăng lại artifact `https://claude.ai/artifact/FzQZASumgPHYRMhRGKEfyd`:
   - dựng trang đăng bằng `tools/strip.sh`;
   - thêm screens15–21.js vào `files`.
8. Báo nhà sáng lập các mặc định tự chọn. Không commit nếu chưa được yêu cầu.
