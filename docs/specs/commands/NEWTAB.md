# NEWTAB — mở tab Start

Mã: `app/shell/start/StartPage.{h,cpp}` (UIA-004, trang UI-A02), `app/shell/filetabs/FileTabBar.{h,cpp}` (UIA-010, tab
Start ở đầu dải, `FileTabBar::kStart`), danh mục lệnh `app/core/actions/commands.cpp` (`NEWTAB`, bản V, "Mở tab Start",
biểu tượng `home`). Bộ định tuyến nối lệnh ở bước integration (INT-002 thay `CommandRouter`).

Nguồn (tài liệu công khai): AutoCAD Help "NEWTAB (Command)", "STARTMODE (System Variable)", "About the Start Tab" (2015,
2022–2025); 07.1 UI-A02 (thành phần, hành vi, trạng thái rỗng, phát hành V/EA), §5.6 (dải tab tệp), §9.11 (kéo-thả); 07.1
§2 (Viewer không có Tạo mới); prototype `A.activate('start')`, `SC.start` (`js/shell.js`, `js/screens1.js`).

## 1. Tóm tắt

Hiện tab Start: trang đầu của ứng dụng với Tạo mới (mẫu TCVN, Trống mm/inch, Mẫu của tôi, Duyệt mẫu…), Mở (Mở tệp…, Mở
thư mục gần đây, Phục hồi bản vẽ lỗi), tệp gần đây (lưới ảnh hoặc danh sách; ghim, tìm, sắp), Học tập, Mẹo, Thông báo và
thẻ phiên bản. Lệnh không tạo, không mở, không đổi bản vẽ nào; bản vẽ đang mở giữ nguyên trong tab của nó.

## 2. Gọi

| Cách | Chuỗi gửi tới bộ định tuyến lệnh |
|---|---|
| Gõ `NEWTAB` (hoặc `_NEWTAB`) | `NEWTAB` |
| Mục menu, nút ribbon dùng QAction `NEWTAB` của ActionRegistry | `^C^C_NEWTAB` |
| Bấm tab Start, Ctrl+Tab / Ctrl+Shift+Tab tới tab Start | không qua lệnh: dải tab tệp phát `activateRequested(kStart)` |

Không có phím tắt mặc định (Keymap không có `NEWTAB`). Không chạy trong suốt (`'NEWTAB` bị từ chối như lệnh thường)
**[cần xác minh]**.

## 3. Prompt, tùy chọn

Không có prompt, không có tùy chọn, không ghi dòng nào vào lịch sử dòng lệnh.

## 4. Sysvar

| Sysvar | Kiểu | Mặc định | Lưu | Ý nghĩa |
|---|---|---|---|---|
| STARTMODE | số nguyên 0/1 | 1 | người dùng (Store) | 1: tab Start hiện ở đầu dải tab tệp. 0: không có tab Start (07.1 UI-A02 "tắt được trong Tùy chọn") |

TrionCAD chưa có STARTMODE: tab Start luôn hiện **[đề xuất]** đặt STARTMODE trong Tùy chọn › Hiển thị (UI-D10) khi có
INT-005. AutoCAD 2015 trở đi: STARTMODE = 0 thì nút "+" của dải tab tạo bản vẽ mới thay vì mở tab Start.

## 5. Enter, Esc, tập chọn, phiên bản

- Không có prompt nên không có Enter/Esc riêng. Enter ở dấu nhắc trống ngay sau đó lặp lại NEWTAB (đang ở tab Start: không
  làm gì).
- Macro `^C^C_NEWTAB` hủy lệnh đang chạy của bản vẽ hiện hành trước (như mọi lệnh menu); tập chọn, PICKFIRST của bản vẽ giữ
  nguyên, quay lại tab bản vẽ thì còn.
- Mọi phiên bản (danh mục `need = V`). Viewer, Viewer Pro (hồ sơ Viewer của UI0-011): trang không có mục Tạo mới.

## 6. Hành vi

1. Tab Start đang mở: không làm gì.
2. Kích hoạt tab Start (`FileTabBar::setCurrent(kStart)`), vùng giữa cửa sổ hiện trang Start thay vùng vẽ; ribbon, tab
   layout, dòng lệnh, palette ẩn như prototype `A.activate('start')` **[đề xuất]**: trang Start không có bản vẽ để lệnh tác
   động. Tiêu đề cửa sổ không có tên bản vẽ.
3. Trang hiện ngay với số liệu đã lưu của tệp gần đây (≤ 150 ms với 200 tệp), rồi kiểm lại ở nền: tệp đã xóa, đổi tên,
   ổ mạng mất hiện "Không tìm thấy"; kích thước, ngày sửa, phiên bản DWG đọc lại. Ảnh xem trước của thẻ đang thấy điền
   dần (ảnh nhúng trong tệp trước, không có thì render ngoài màn hình), luồng UI không chờ.
4. Rời tab Start (chọn tab bản vẽ, mở tệp): mọi việc tạo ảnh đang chạy bị hủy (≤ 50 ms).

## 7. Hành động trên trang → lệnh

Trang không tự chạy lệnh, không tự mở tệp: phát tín hiệu cho bước integration (bộ định tuyến lệnh, phiên tài liệu).

| Phần tử | Gửi |
|---|---|
| Mẫu TCVN A3 ngang, A3 dọc, A1 ngang, A2 ngang, A4 dọc; Trống (mm), Trống (inch) | `^C^C_NEW <tệp mẫu>` (`TCVN-A3-ngang.dwt`…, `Trong-mm.dwt`, `Trong-inch.dwt`) **[đề xuất]**: trả lời prompt tên mẫu của NEW khi FILEDIA = 0 (CMD-241); tên tệp do VN-008 chốt |
| Mẫu của tôi | `^C^C_QNEW` (mẫu mặc định trong Tùy chọn › Tệp) |
| Duyệt mẫu… | `^C^C_NEW` (hộp chọn mẫu) |
| Mở tệp… | `^C^C_OPEN` |
| Mở thư mục gần đây › thư mục | `browseRequested(thư mục)`: hộp thoại Mở bắt đầu ở thư mục đó |
| Phục hồi bản vẽ lỗi | `^C^C_RECOVER` |
| Bấm thẻ/dòng tệp gần đây, Enter, menu Mở; thả tệp DWG/DXF/DWT | `openRequested(đường dẫn…)` (thả: đúng thứ tự thả) |
| Menu Mở chỉ đọc | `openReadOnlyRequested(đường dẫn)` |
| Mở bản vẽ mẫu, Xem video 2 phút, Học tập, Thông báo, Trạng thái giấy phép | `screenRequested(id)`: `sample`, `video-intro`, `migrate`, `video-measure`, `video-fontdoctor`, id của mục thông báo, `license` |
| Dùng thử Pro 30 ngày, Mua TrionCAD Pro | trang nâng cấp qua `EditionGate::upgrade().open()` |

Ghim, sắp, tìm, kiểu xem, Mở thư mục chứa, Sao chép đường dẫn, Xóa khỏi danh sách là việc của giao diện (không phải lệnh,
không vào undo của bản vẽ); xóa khỏi danh sách có Hoàn tác trên toast, tệp trên đĩa không bị xóa.

## 8. Trường hợp biên

- Không còn bản vẽ nào mở (đóng tab cuối): dải tab chọn tab Start, như gọi NEWTAB.
- Tab Start không đóng được, không kéo được, luôn ở đầu dải (UIA-010).
- Đang kéo-thả tệp lên trang: gợi ý "Thả tệp DWG, DXF để mở"; tệp khác định dạng bị từ chối kèm thông báo nêu tên.
- Danh sách tệp gần đây rỗng: trạng thái rỗng "Chưa có tệp gần đây" với "Mở bản vẽ mẫu", "Xem video 2 phút".
- NEWTAB từ script, LISP `(command "NEWTAB")`: chuyển tab giữa script làm lệnh kế tiếp không còn bản vẽ để tác động;
  AutoCAD xử lý thế nào **[cần xác minh]**.

## 9. Khác AutoCAD (có chủ đích)

- AutoCAD 2015 gọi trang này là "New Tab", 2022 trở đi "Start". TrionCAD có đúng một tab Start, NEWTAB chỉ kích hoạt nó
  (không tạo thêm tab).
- Trang có thẻ phiên bản (số ngày dùng thử, nâng cấp) và Mẹo, theo 07.1 UI-A02; AutoCAD có mục tài khoản Autodesk, tệp
  trên đám mây (không có ở TrionCAD).
- Tìm trong tệp gần đây không phân biệt dấu tiếng Việt ("ket cau" khớp "Kết cấu").

## 10. Cần xác minh (QA-007)

- AutoCAD 2025: NEWTAB khi STARTMODE = 0 (mở tab Start tạm thời, tạo bản vẽ mới hay báo lỗi).
- `'NEWTAB` trong suốt có được chấp nhận không; NEWTAB có hủy lệnh đang chạy khi gõ trên dòng lệnh.

## 11. Test

`app/shell/start/tests/start_test.cpp` (commands: chuỗi lệnh của từng nút; startupContent, cachedThumbnails, slowRender,
search, missingFiles, brokenFiles, viewerProfile, editionCard, menuActions, dropFiles, keyboard). Nối NEWTAB tới bộ định
tuyến và dải tab tệp: integration nhóm 6 (Hoãn, cần bước integration).
