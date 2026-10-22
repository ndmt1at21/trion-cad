# CLEANSCREENOFF — tắt Clean Screen

Mã: `app/shell/CleanScreen.{h,cpp}` (UIA-001). Lệnh bật và nguồn tài liệu: [CLEANSCREENON](CLEANSCREENON.md).

## 1. Tóm tắt

Khôi phục giao diện về đúng trạng thái trước lần CLEANSCREENON gần nhất (AutoCAD: "Restores the state of the display
before CLEANSCREENON was used"): hiện lại đúng những gì CLEANSCREENON đã ẩn (vùng, palette ở dock và nổi, dải tự ẩn,
toolbar) ở chỗ và cỡ cũ, đưa tab đang chọn của từng nhóm palette lên lại. Bố cục về đúng từng pixel.

## 2. Gọi

| Cách | Chuỗi gửi tới bộ định tuyến lệnh |
|---|---|
| Gõ `CLEANSCREENOFF` (hoặc `_CLEANSCREENOFF`) | `CLEANSCREENOFF` |
| Ctrl+0, nút Clean Screen của thanh trạng thái khi CLEANSCREENSTATE = 1 | `'_CLEANSCREENOFF` |
| Trong suốt giữa lệnh khác | `'CLEANSCREENOFF` |

## 3. Prompt, tùy chọn

Không có prompt, không có tùy chọn. Dòng lệnh ghi **[đề xuất]**: `<Clean Screen tắt>` / `<Clean Screen off>` (bước
integration nối `CleanScreen::toggled`).

## 4. Sysvar

CLEANSCREENSTATE thành 0 (chỉ đọc, không lưu; xem [CLEANSCREENON](CLEANSCREENON.md) §4).

## 5. Enter, Esc, tập chọn, phiên bản

Như CLEANSCREENON: không prompt; tập chọn giữ nguyên; trong suốt được; mọi phiên bản. Ngoài danh mục lệnh của prototype
(`cmd.js`) nên không có nút riêng: Ctrl+0 và nút thanh trạng thái gửi nó khi Clean Screen đang bật.

## 6. Hành vi

1. Đang tắt: không làm gì; CLEANSCREENSTATE giữ 0.
2. Hiện lại đúng các vùng CLEANSCREENON đã ẩn (vùng người dùng tắt từ trước vẫn tắt).
3. Hiện lại palette ở dock và nổi, dải tự ẩn, toolbar đã ẩn (cửa sổ giữ chỗ và cỡ của chúng), rồi đưa tab đang chọn
   lúc bật lên lại; khung trượt không mở lại.
4. Kích thước, trạng thái phóng to, màn hình của cửa sổ không đổi.

## 7. Trường hợp biên

- Đổi workspace khi đang bật: Clean Screen đã tắt theo workspace mới; CLEANSCREENOFF sau đó không làm gì.
- Thoát ứng dụng khi đang bật: tự tắt trước khi lưu bố cục.
- Palette mở trong lúc đang bật: giữ nguyên **[cần xác minh]** (CLEANSCREENON §9).

## 8. Test

`app/shell/tests/shell_test.cpp` (cleanScreen: hình học từng vùng, từng palette và ảnh chụp cửa sổ giống từng pixel sau
khi tắt, qua Ctrl+0 và qua `^C^C_CLEANSCREENOFF`; tắt khi đang tắt không làm gì).
