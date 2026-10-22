# INPUTSEARCHOPTIONS — thiết lập dòng lệnh

Nguồn: 07.1 §8 ("Thiết lập dòng lệnh (INPUTSEARCHOPTIONS): số dòng lịch sử, cỡ chữ, độ trong suốt khi nổi, độ trễ tự
hoàn thành, có tìm biến hệ thống không, có tìm theo tiếng Việt không"; AutoCorrect xem và xóa trong thiết lập), prototype
tab "Dòng lệnh" của `SC.options` (`design/prototype/js/screens2.js`), tài liệu công khai AutoCAD (INPUTSEARCHOPTIONS).
Thực hiện: UIA-007 (`app/shell/cmdline/InputSearchOptions`), bước integration (đăng ký hộp thoại `inputsearchoptions`,
lệnh vào CommandRouter).

## Gọi

| Cách | Ghi chú |
|---|---|
| Gõ `INPUTSEARCHOPTIONS` | |
| Chuột phải trên dòng lệnh › Tùy chọn dòng lệnh… | Macro `^C^C_INPUTSEARCHOPTIONS` |
| Script, LISP | `(command "_.INPUTSEARCHOPTIONS")` mở hộp thoại; không có bản dòng lệnh |

Phiên bản: mọi phiên bản (V), kể cả Viewer. Không trong suốt: hủy lệnh đang chạy như lệnh thường.

## Hành vi

Hộp thoại modal "Thiết lập dòng lệnh":

| Nhóm | Thiết lập | Giá trị | Mặc định |
|---|---|---|---|
| Dòng lệnh | Số dòng lịch sử dòng lệnh | 0–10 (ngoài khoảng thì kẹp) | 3 |
| | Cỡ chữ | Theo giao diện, 14, 16, 18, 20 px (chữ, cao dòng, ô nhập, chip cùng tỉ lệ) | Theo giao diện |
| | Độ trong suốt khi nổi | 0–90 % | 20 % |
| Tự hoàn thành | Độ trễ | 0, 200, 500 ms | 0 ms |
| | Tìm biến hệ thống | bật/tắt | bật |
| | Tìm lệnh theo tiếng Việt | bật/tắt ("cắt" → TRIM) | bật |
| Bộ gõ tiếng Việt | Khi đang chờ tên lệnh, bỏ qua bộ gõ | bật/tắt | bật |
| Tự sửa lỗi gõ (AutoCorrect) | Bảng Gõ sai, Sửa thành, Số lần chọn; xóa từng dòng (thùng rác, phím Delete) | | |

1. "Lưu" (Enter) ghi mọi giá trị vào thiết lập người dùng và xóa các dòng AutoCorrect đã bỏ; dòng lệnh, danh sách tự
   hoàn thành, ô nhập áp ngay, không khởi động lại. "Hủy", Esc: không đổi gì (dòng AutoCorrect đã bỏ trở lại).
2. Độ trễ chỉ đổi lúc danh sách tự hoàn thành hiện; Enter, Space, Tab, ↑↓ trước khi hết trễ vẫn dùng danh sách tính ngay,
   nên lệnh được gửi như độ trễ 0.
3. Không đổi bản vẽ, không tạo bước hoàn tác.

## Trường hợp biên

- AutoCorrect chưa có mục nào: bảng thay bằng câu hướng dẫn (chọn cùng một gợi ý "Ý bạn là" 3 lần).
- Xóa một dòng AutoCorrect: lỗi gõ ấy học lại từ đầu (số lần chọn về 0).

## Khác AutoCAD

- AutoCAD có thêm tìm nội dung (block, layer, hatch), tìm giữa chuỗi, tự nối gợi ý; TrionCAD có tìm theo tiếng Việt, bỏ
  qua bộ gõ khi chờ tên lệnh, cỡ chữ và độ trong suốt riêng của dòng lệnh. Tìm giữa chuỗi luôn bật (chưa có thiết lập).
