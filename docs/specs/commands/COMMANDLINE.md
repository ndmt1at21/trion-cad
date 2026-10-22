# COMMANDLINE — hiện dòng lệnh

Nguồn: 07.1 §8 (UI-A06: "luôn có; bật/tắt bằng Ctrl+9"), §11 (Ctrl+9), tài liệu công khai AutoCAD (COMMANDLINE,
COMMANDLINEHIDE, biến CLISTATE). Thực hiện: UIA-107 (widget `app/shell/cmdline/CommandLine`), bước integration nhóm 5
(ẩn/hiện vùng dòng lệnh của `MainWindow`, UIA-001), UIA-007 (dạng nổi). Lệnh ngược: [COMMANDLINEHIDE](COMMANDLINEHIDE.md).

## Gọi

| Cách | Ghi chú |
|---|---|
| Gõ `COMMANDLINE` | Khi dòng lệnh đang ẩn, gõ được qua ô nhập động (DYN) |
| Ctrl+9 | Keymap id `cmdl`, phạm vi toàn cục: dòng lệnh đang ẩn → COMMANDLINE, đang hiện → COMMANDLINEHIDE |
| Ribbon Hiển thị › Palette, menu Công cụ › Dòng lệnh (§7.12) | Nút bật/tắt, trạng thái theo CLISTATE |
| Script, LISP | `(command "_.COMMANDLINE")` |

Phiên bản: mọi phiên bản (V), kể cả Viewer. Không có prompt, không có tùy chọn.

## Hành vi

1. Dòng lệnh đang ẩn: hiện lại ở dạng và vị trí trước khi ẩn (dock dưới mặc định, hoặc nổi bán trong suốt, UIA-007);
   lịch sử, prompt đang hỏi, chữ đang gõ dở giữ nguyên (ẩn chỉ là ẩn widget, không xóa gì). Focus về ô nhập.
2. Dòng lệnh đang hiện: không đổi gì, chỉ đưa focus về ô nhập.
3. **Trong suốt:** chạy được giữa lệnh khác (Ctrl+9 khi đang LINE không hủy LINE); macro của phím là `'_COMMANDLINE`.
   Không tạo bước hoàn tác, không đổi bản vẽ.
4. CLISTATE (chỉ đọc, không lưu trong bản vẽ) thành 1.
5. Trạng thái hiện/ẩn lưu theo không gian làm việc của người dùng (NT-010), khởi động lại giữ nguyên.

## Trường hợp biên

- Màn hẹp (1366–1600 px): dòng lệnh hiện ở dạng gọn, lịch sử 1 dòng (07.1 §3.2; `compact` của UIA-107).
- Hộp thoại modal đang mở: Ctrl+9 không chạy (phím tắt toàn cục bị chặn khi có modal, UI0-015); gõ lệnh cũng không.
- Clean Screen (Ctrl+0, CLEANSCREENON): hai trạng thái độc lập; tắt Clean Screen trả dòng lệnh về đúng trạng thái
  COMMANDLINE/COMMANDLINEHIDE đã chọn.

## Khác AutoCAD

- Không có. Tên lệnh, phím, CLISTATE giữ như AutoCAD.
