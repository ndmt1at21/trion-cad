# COMMANDLINEHIDE — ẩn dòng lệnh

Nguồn: 07.1 §8 (UI-A06: "bật/tắt bằng Ctrl+9"), §11, §9.7 (ô nhập động), tài liệu công khai AutoCAD (COMMANDLINEHIDE,
CLISTATE). Thực hiện: bước integration nhóm 5 (ẩn vùng dòng lệnh của `MainWindow`, UIA-001), UIA-107 (widget), UIA-007
(dạng nổi). Lệnh ngược: [COMMANDLINE](COMMANDLINE.md).

## Gọi

| Cách | Ghi chú |
|---|---|
| Gõ `COMMANDLINEHIDE` | |
| Ctrl+9 khi dòng lệnh đang hiện | Keymap id `cmdl` |
| Nút đóng của dòng lệnh ở dạng nổi (UIA-007) | |
| Script, LISP | `(command "_.COMMANDLINEHIDE")` |

Phiên bản: mọi phiên bản (V). Không có prompt, không có tùy chọn.

## Hành vi

1. Ẩn vùng dòng lệnh (dock hoặc nổi); vùng vẽ giãn ra phần đó. Lệnh đang chạy, prompt, lịch sử, chữ đang gõ dở giữ
   nguyên trong widget ẩn; COMMANDLINE hiện lại đúng như cũ.
2. **Trong suốt:** chạy giữa lệnh khác không hủy lệnh đó; macro `'_COMMANDLINEHIDE`. Không tạo bước hoàn tác.
3. CLISTATE thành 0. Trạng thái lưu theo không gian làm việc (NT-010).
4. Khi ẩn, phím gõ ở vùng vẽ đi vào ô nhập động cạnh con trỏ nếu DYN bật (07.1 §8 Thành phần 1); prompt và tùy chọn
   hiện ở đó (INT-014). Thông báo quan trọng của lệnh vẫn hiện ở góc vùng vẽ (07.1 §12).
5. DYN tắt mà người dùng bắt đầu gõ ở vùng vẽ: dòng lệnh tự hiện lại rồi nhận phím đó **[đề xuất]**, để không có lúc gõ
   lệnh mà không thấy prompt ở đâu.
6. Lần đầu ẩn trong phiên: toast "Đã ẩn dòng lệnh. Nhấn Ctrl+9 để hiện lại." (Info, tự ẩn, 07.1 §12) **[đề xuất]**.

## Trường hợp biên

- Dòng lệnh đã ẩn: không làm gì.
- Hộp thoại modal đang mở: Ctrl+9 không chạy.
- F2 (lịch sử đầy đủ, UIA-007) vẫn mở được khi dòng lệnh ẩn.

## Khác AutoCAD

- AutoCAD hỏi xác nhận trước khi đóng cửa sổ dòng lệnh (hộp "Command Line - Close Window" có ô "luôn làm theo lựa chọn
  này"). TrionCAD không hỏi: ẩn không phá hủy gì, Ctrl+9 hiện lại ngay; thay bằng toast một lần (mục 6) **[đề xuất]**.
- Mục 5 là hành vi riêng của TrionCAD **[đề xuất]**; cần đối chiếu cách AutoCAD xử lý phím gõ khi dòng lệnh ẩn và DYN
  tắt (bộ tham chiếu QA-110).
