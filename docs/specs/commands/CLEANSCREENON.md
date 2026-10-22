# CLEANSCREENON — bật Clean Screen

Mã: `app/shell/CleanScreen.{h,cpp}` (UIA-001), phím và macro: `app/core/actions/ActionRegistry.cpp`. Lệnh tắt:
[CLEANSCREENOFF](CLEANSCREENOFF.md).

Nguồn (tài liệu công khai): AutoCAD Help "Clean Screen (Status Bar Button)" (2023, 2025), "CLEANSCREENON (Command)",
"CLEANSCREENOFF (Command)", "CLEANSCREENSTATE (System Variable)"; BricsCAD Help "CLEANSCREENOPTIONS system variable";
07.1 §3 (bảng vùng: ribbon tự ẩn khi bật Clean Screen), §1 (vùng vẽ ≥ 75% ở 1920×1080), §11 (Ctrl+0); prototype
`#app.clean` (css/app.css), nút `sb-clean` ("Thu gọn toàn bộ giao diện, chỉ còn vùng vẽ, dòng lệnh, thanh trạng thái").

## 1. Tóm tắt

Mở rộng vùng vẽ: ẩn thanh tiêu đề của ứng dụng (nút menu, Quick Access Toolbar, ô Tìm lệnh), ribbon (workspace Cổ
điển: thanh menu và toolbar), tab tệp, tab layout, mọi palette (dock, nổi, dải tự ẩn, khung trượt đang mở) và
toolbar. Còn lại vùng vẽ, dòng lệnh, thanh trạng thái. Ở 1920×1080 vùng vẽ chiếm ~89% diện tích cửa sổ (≥ 75%).

## 2. Gọi

| Cách | Chuỗi gửi tới bộ định tuyến lệnh |
|---|---|
| Gõ `CLEANSCREENON` (hoặc `_CLEANSCREENON`) | `CLEANSCREENON` |
| Ctrl+0 (Keymap `clean`, phạm vi Toàn cục), nút Clean Screen của thanh trạng thái, mục menu | `'_CLEANSCREENON` khi CLEANSCREENSTATE = 0, `'_CLEANSCREENOFF` khi = 1 |
| Trong suốt giữa lệnh khác | `'CLEANSCREENON` |

Ctrl+0 đổi vòng bật/tắt như macro CUI của AutoCAD: biểu thức DIESEL `$M=$(if,…$(getvar,cleanscreenstate)…)` chọn
`_cleanscreenoff` khi đang bật, `_cleanscreenon` khi đang tắt.

## 3. Prompt, tùy chọn

Không có prompt, không có tùy chọn. Dòng lệnh ghi một dòng thông báo **[đề xuất]** (prototype `A.cleanScreen`):

| vi | en |
|---|---|
| `<Clean Screen bật · Ctrl+0 để tắt>` | `<Clean Screen on · Ctrl+0 to turn off>` |

Dòng này do bước integration nối `CleanScreen::toggled` vào lịch sử dòng lệnh (UIA-107).

## 4. Sysvar

| Sysvar | Kiểu | Mặc định | Lưu | Ý nghĩa |
|---|---|---|---|---|
| CLEANSCREENSTATE | số nguyên 0/1, chỉ đọc | 0 | không lưu | 1 khi Clean Screen đang bật. Ctrl+0 đọc nó để chọn ON/OFF |

CLEANSCREENOPTIONS không có trong AutoCAD: là sysvar của BricsCAD (bitcode 0–127, mặc định 15 = ẩn tab tài liệu 1, panel
dock 2, toolbar 4, ribbon 8; thêm dòng lệnh 16, thanh trạng thái 32, thanh menu 64). TrionCAD chưa có; phần bị ẩn cố
định như §1 **[đề xuất]**.

## 5. Enter, Esc, tập chọn, phiên bản

- Không có prompt nên không có Enter/Esc riêng. Enter ở dấu nhắc trống ngay sau đó lặp lại CLEANSCREENON (đang bật:
  không làm gì).
- Không dùng tập chọn; PICKFIRST và tập chọn hiện tại giữ nguyên.
- Trong suốt: chạy giữa lệnh khác không ngắt lệnh đó; prompt của lệnh vẫn hiện (dòng lệnh không bị ẩn).
- Mọi phiên bản (Viewer trở lên; danh mục lệnh `need = V`).

## 6. Hành vi

1. Đang bật: không làm gì; CLEANSCREENSTATE giữ 1.
2. Bật: ghi lại những gì đang hiện (vùng, palette ở dock và nổi, dải tự ẩn, toolbar) và tab đang chọn của từng nhóm
   palette, rồi ẩn. Cửa sổ giữ chỗ và cỡ của palette ẩn. Vùng đã ẩn từ trước (ví dụ người dùng tắt tab tệp) không được
   ghi, nên CLEANSCREENOFF không hiện nó ra.
3. Focus nằm trong phần bị ẩn (palette, ô Tìm lệnh): focus về vùng vẽ.
4. Khung trượt của palette tự ẩn đang mở thì thu lại; CLEANSCREENOFF không mở lại (trạng thái tạm như menu).
5. Kích thước, trạng thái phóng to, màn hình của cửa sổ không đổi; khung cửa sổ của hệ điều hành giữ nguyên.
6. CLEANSCREENSTATE thành 1.

## 7. Trường hợp biên

- Thoát ứng dụng hoặc đóng cửa sổ khi đang bật: tắt trước rồi lưu, nên bố cục lưu là bố cục trước khi bật; lần mở sau
  Clean Screen tắt (CLEANSCREENSTATE không lưu).
- Đổi workspace khi đang bật: workspace mới hiện vùng và palette của nó, Clean Screen thành tắt (CLEANSCREENSTATE 0).
- Kéo cửa sổ sang màn hình khác lớp khi đang bật: dòng lệnh theo lớp mới (1 dòng lịch sử ở màn hình hẹp); bố cục
  palette không áp lại quy tắc màn hình.
- Mở palette bằng lệnh khi đang bật (PROPERTIES, Ctrl+1): palette hiện; CLEANSCREENOFF hiện lại các palette đã ẩn,
  palette mở trong lúc bật giữ nguyên **[cần xác minh]**.
- Macro đặt CLEANSCREENSTATE trực tiếp (`'CLEANSCREENSTATE 1`): sysvar chỉ đọc. Bộ định tuyến tạm chưa chặn (INT-005
  chặn); lần Ctrl+0 kế tiếp tự đồng bộ lại.

## 8. Khác AutoCAD (có chủ đích)

- AutoCAD 2015 trở đi trên Windows giữ thanh tiêu đề có Quick Access Toolbar; bản cũ chuyển ứng dụng sang toàn màn
  hình. TrionCAD theo prototype: ẩn cả thanh tiêu đề của ứng dụng, giữ khung cửa sổ của hệ điều hành (thu nhỏ, đóng
  vẫn dùng được), không chuyển toàn màn hình **[đề xuất]**.
- Tài liệu AutoCAD bản cũ giữ tab Model/layout; TrionCAD ẩn tab layout theo prototype (Ctrl+PageUp/PageDown vẫn chuyển
  layout) **[đề xuất]**.
- Ctrl+0 gửi dạng trong suốt (`'_CLEANSCREENON`): không hủy lệnh đang chạy **[đề xuất]**.

## 9. Cần xác minh (QA-007)

- AutoCAD 2025: tab tệp, tab layout có bị ẩn không.
- Palette mở trong lúc Clean Screen bật: CLEANSCREENOFF của AutoCAD giữ hay đóng palette đó.
- Macro Ctrl+0 của AutoCAD có hủy lệnh đang chạy (`^C^C`) hay chạy trong suốt.

## 10. Test

`app/shell/tests/shell_test.cpp` (cleanScreen: Ctrl+0 và bộ định tuyến cho cùng kết quả, ≥ 75%, tắt khôi phục đúng
từng pixel, thời gian; workspaces: đổi workspace khi đang bật), `app/core/actions/tests/actions_test.cpp`
(cleanScreenCycle: Ctrl+0 theo CLEANSCREENSTATE). Chạy từ `.scr`, `(command "CLEANSCREENON")`: Hoãn (INT-002, LSP).
