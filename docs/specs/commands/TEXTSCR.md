# TEXTSCR — lịch sử dòng lệnh đầy đủ (F2)

Nguồn: 07.1 §8 (UI-A06: "F2 mở cửa sổ lịch sử đầy đủ, có tìm kiếm và sao chép"), prototype `SC.textWindow`
(`design/prototype/js/screens1.js`, UI-I03), tài liệu công khai AutoCAD (TEXTSCR, F2). Thực hiện: UIA-007
(`app/shell/cmdline/HistoryWindow`), bước integration (đăng ký hộp thoại `textscr` với DialogHost, lệnh TEXTSCR vào
CommandRouter).

## Gọi

| Cách | Ghi chú |
|---|---|
| Gõ `TEXTSCR` | |
| F2 | Keymap id `hist`, phạm vi toàn cục (macOS ⌃⌘H) |
| Nút ^ cuối ô nhập dòng lệnh | Tooltip "Lịch sử đầy đủ · F2" |
| Script, LISP | `(command "_.TEXTSCR")` |

Phiên bản: mọi phiên bản (V), kể cả Viewer. Trong suốt: chạy giữa lệnh khác (`'_TEXTSCR`), không hủy lệnh đang chạy.
Không có prompt, không có tùy chọn.

## Hành vi

1. Mở cửa sổ "Lịch sử dòng lệnh" không chế độ (760×480, đổi cỡ được, nhớ vị trí và cỡ): mọi dòng lịch sử của dòng lệnh
   (tối đa 100.000 dòng cuối), chữ Mono, dòng mới nhất ở cuối. Đang mở thì đưa lên trước, không mở bản thứ hai.
2. Dòng mới của dòng lệnh (echo, thông báo, lỗi) vào cửa sổ ngay; đang xem dòng cuối thì cuộn theo.
3. **Tìm:** ô "Tìm trong nội dung" lọc các dòng chứa chữ gõ, không phân biệt dấu và chữ hoa ("doi tuong" khớp
   "Đã chọn 125 đối tượng"); đoạn khớp được tô nền. Không dòng nào khớp: báo trống kèm cách xem lại toàn bộ. Tìm chạy nền,
   100.000 dòng ≤ 50 ms.
4. **Sao chép:** nút Sao chép (hoặc Ctrl+C trong danh sách) chép các dòng đang chọn, không chọn thì mọi dòng đang hiện;
   chữ nguyên văn, mỗi dòng một dòng.
5. Esc (ô tìm trống) hoặc nút Đóng: đóng cửa sổ, focus về chỗ cũ. Không đổi bản vẽ, không tạo bước hoàn tác.

## Trường hợp biên

- Lịch sử rỗng: chỉ có dòng chào của dòng lệnh.
- Dòng rất dài hiện rút gọn "…"; sao chép vẫn đủ.
- Đang lọc mà có dòng mới khớp: dòng đó vào danh sách lọc ngay.

## Khác AutoCAD

- AutoCAD F2 mở rộng lịch sử ngay trên dòng lệnh hoặc mở Text Window (cửa sổ chữ chọn được); TrionCAD luôn mở cửa sổ lịch
  sử có ô tìm (lọc dòng) và nút Sao chép. Chưa có "Lưu ra tệp…" (sẽ thêm).
