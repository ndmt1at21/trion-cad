# RIBBONCLOSE — ẩn ribbon

Nguồn: 07.1 §3 (bố cục cửa sổ), §7.1 (hành vi ribbon), tài liệu công khai AutoCAD (RIBBONCLOSE, RIBBON, biến
RIBBONSTATE). Thực hiện: UIA-005 (widget `app/shell/ribbon/RibbonBar`), bước integration nhóm 6 (ẩn vùng Ribbon của
`MainWindow`, UIA-001). Lệnh ngược: [RIBBON](RIBBON.md).

## Gọi

| Cách | Ghi chú |
|---|---|
| Gõ `RIBBONCLOSE` | |
| Script, LISP | `(command "_.RIBBONCLOSE")` |

Phiên bản: mọi phiên bản (V). Không có prompt, không có tùy chọn.

## Hành vi

1. Ẩn vùng Ribbon; vùng vẽ giãn ra phần đó. Thân nổi (mức chỉ tên tab) và panel mở rộng đang mở đóng theo; panel đã
   ghim giữ trạng thái ghim và hiện lại khi chạy RIBBON.
2. **Trong suốt:** chạy giữa lệnh khác không hủy lệnh đó; macro `'_RIBBONCLOSE`. Không tạo bước hoàn tác.
3. RIBBONSTATE thành 0. Trạng thái lưu theo workspace (NT-010), khởi động lại giữ nguyên.
4. Lệnh vẫn gõ được ở dòng lệnh, ô Tìm lệnh trên thanh tiêu đề (UIA-002) và QAT; phím Alt không vào ribbon khi ribbon
   ẩn.
5. Lần đầu ẩn trong phiên: toast "Đã ẩn ribbon. Gõ RIBBON để hiện lại." (Info, tự ẩn, 07.1 §12) **[đề xuất]**.

## Trường hợp biên

- Ribbon đã ẩn: không làm gì.
- Hộp thoại modal đang mở: không chạy (như mọi lệnh gõ).
- Clean Screen: hai trạng thái độc lập; tắt Clean Screen trả ribbon về đúng trạng thái RIBBON/RIBBONCLOSE đã chọn.

## Khác AutoCAD

- AutoCAD không có toast nhắc cách hiện lại; TrionCAD thêm (mục 5) vì không có nút nào trên màn hình để mở lại ribbon
  **[đề xuất]**.
