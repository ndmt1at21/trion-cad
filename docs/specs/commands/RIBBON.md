# RIBBON — hiện ribbon

Nguồn: 07.1 §3 (bố cục cửa sổ, 3 mức thu gọn), §3.1 (workspace), §7.1 (hành vi ribbon), tài liệu công khai AutoCAD
(RIBBON, RIBBONCLOSE, biến RIBBONSTATE). Thực hiện: UIA-005 (widget `app/shell/ribbon/RibbonBar`), bước integration
nhóm 6 (vùng Ribbon của `MainWindow`, UIA-001). Lệnh ngược: [RIBBONCLOSE](RIBBONCLOSE.md).

## Gọi

| Cách | Ghi chú |
|---|---|
| Gõ `RIBBON` | |
| Menu Công cụ › Palette › Ribbon (workspace Cổ điển, UIA-011) | |
| Script, LISP | `(command "_.RIBBON")` |

Phiên bản: mọi phiên bản (V), kể cả Viewer (ribbon giản lược UI-M01). Không có prompt, không có tùy chọn.

## Hành vi

1. Ribbon đang ẩn: hiện lại ở vùng Ribbon (dưới thanh tiêu đề), đúng mức thu gọn, tab đang mở, tab/panel ẩn và panel
   mở rộng đã ghim như trước khi ẩn (lưu qua NT-010: `ribbon/collapse`, `ribbon/hiddenTabs`, `ribbon/hiddenPanels`,
   `ribbon/pinned`). Vùng vẽ co lại phần đó.
2. Ribbon đang hiện: không đổi gì.
3. **Trong suốt:** chạy được giữa lệnh khác, macro `'_RIBBON`. Không tạo bước hoàn tác, không đổi bản vẽ.
4. RIBBONSTATE (chỉ đọc, không lưu trong bản vẽ) thành 1. Trạng thái hiện/ẩn lưu theo workspace (NT-010), khởi động
   lại giữ nguyên.
5. Lệnh không đổi mức thu gọn: mức thu gọn chỉ đổi bằng nút ⌃ cuối dải tab hoặc menu chuột phải của ribbon (đầy đủ →
   chỉ tên panel → chỉ tên tab).

## Trường hợp biên

- Workspace Cổ điển (không có ribbon, 07.1 §3.1): hiện ribbon trên thanh menu và toolbar như AutoCAD; workspace vẫn
  là Cổ điển cho tới khi người dùng lưu hoặc đổi workspace **[đề xuất]**.
- Clean Screen (Ctrl+0) đang bật: ghi nhận trạng thái hiện, ribbon xuất hiện khi tắt Clean Screen (hai trạng thái độc
  lập, như COMMANDLINE).
- Màn hẹp (1366–1600 px): panel co theo ưu tiên ngay khi hiện (07.1 §3.2), không cần lệnh khác.

## Khác AutoCAD

- AutoCAD có 4 mức thu gọn (thêm "chỉ nút panel"); TrionCAD có 3 mức theo 07.1 §3, các panel tự thu thành nút khi hẹp.
- Ribbon của TrionCAD không tách thành palette nổi (AutoCAD cho kéo ribbon ra khỏi khung) **[đề xuất]**.
