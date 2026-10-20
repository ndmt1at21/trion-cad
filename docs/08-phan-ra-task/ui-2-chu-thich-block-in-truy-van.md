# UI 2 · Chú thích, block, in và truy vấn (UIF, UIG, UIH, UII)

> Task giao diện cho 55 mã màn hình: chú thích UI-F01–F15, block và tham chiếu ngoài UI-G01–G19, layout, in và xuất UI-H01–H11, truy vấn và dữ liệu UI-I01–I10.
> - Đặc tả: [07.2](../07-dac-ta-ui-2-bang-va-hop-thoai.md) §6–§10; khung chung ở [07.1](../07-dac-ta-ui-1-khung-ung-dung.md) §1, §7.3, §7.4, §7.8, §7.11, §9.10–§9.12, §12, §13; danh mục và đợt thiết kế ở [07.3](../07-dac-ta-ui-3-tinh-nang-vn-va-he-thong.md) Phụ lục A–C. Prototype: `design/prototype/index.html#UI-xxx`.
> - Thư mục mã: `app/dialogs/`, `app/palettes/`, `app/contextual/`. Mẫu task, DoD, task neo: [README](README.md) §2–§5.

## 0. Ghi chú thiết kế

- **Ranh giới** (README §4.2): lệnh, engine và quy tắc hình học thuộc CMD, PLT, VN, INT, RGN. Task ở đây chỉ làm giao diện: hộp thoại, palette, tab ngữ cảnh, lớp phủ trên vùng vẽ, tương tác. Lệnh mở giao diện qua điểm nối trình bày đăng ký theo tên lệnh ở registry INT-001. Khi không có giao diện (`-LỆNH`, `.scr`, `(command ...)`, `trion-console`, CMDDIA/FILEDIA = 0), lệnh vẫn chạy đủ bằng prompt **[đề xuất]**.
- **Ghi bản vẽ chỉ qua lệnh** (DoD-U 5). Nút Áp dụng, Tạo, Chèn gửi lệnh có đủ tham số qua INT-002, đúng một mark undo mỗi lần. Palette áp dụng ngay, mỗi lần sửa là một bước hoàn tác (07.2 §0). Tiêu chí chung: chuỗi lệnh do giao diện gửi (ghi trong nhật ký lệnh) chạy lại bằng `.scr` cho DXF trùng 100% **[đề xuất]**.
- **Đọc bản vẽ** qua snapshot DOC-009 và metadata thuộc tính DOC-017. Model của palette dựng ở nền, áp diff theo lô (UI0-014). Danh sách hơn 200 dòng luôn ảo hóa.
- **Tab ngữ cảnh** của các màn hình trong tệp này (Soạn văn bản, Ô bảng, Block Editor, Ảnh / PDF / Xref, So sánh bản vẽ, Layout, Sửa tham chiếu) do task ở đây định nghĩa: JSON trên khung UIA-005 kèm binding, đặt ở `app/contextual/<tab>/`. UIA-006 chỉ gắn chúng vào bộ ribbon. Tab So sánh phải chạy trên ribbon Viewer (UIA-017) từ mốc VP **[đề xuất]**.
- **Thumbnail và xem trước** chỉ hiển thị (README §4.2): ảnh lưu trong tệp qua DWG-026, ảnh render ngoài màn hình qua RND-021. Tạo ở nền ưu tiên thấp (NT-005), hủy khi dòng ra khỏi màn hình, cache theo (handle, thế hệ thay đổi, cỡ, DPI, chủ đề) **[đề xuất]**.
- **Tác vụ dài** (in hàng loạt, PUBLISH, so sánh, quét, trích xuất nhiều tệp) đăng ký NT-013. Đóng hộp thoại không dừng tác vụ. Tiến trình và nút hủy hiện ở thanh trạng thái (UIA-009); xong thì có toast kèm hành động (07.1 §12).
- **Cổng phiên bản** (07.1 §2, UI0-011): Viewer xem, đo, in theo thiết lập có sẵn; Viewer Pro thêm đếm, so sánh, in hàng loạt, xuất bảng, markup, nhưng không ghi DWG; Pro sửa và chèn vào bản vẽ. Mỗi task ghi phần bị khóa và kiểm ở cả ba phiên bản.
- **Nhập chữ trên vùng vẽ** (MTEXT, TEXT, ô bảng, attribute nhiều dòng) dùng chung thành phần UIF-002, để bộ gõ tiếng Việt và IME chạy như nhau ở mọi nơi.
- **Kiểm thử:** Qt Test đặt cạnh mã ở `<thư mục>/tests/`; kịch bản QA-011 (ảnh chụp Tối/Sáng ở 100/150/200%, đo độ trễ) ở `<thư mục>/tests/ui/`. Số đo hiệu năng dùng BENCH-STD (QA-001, QA-012). Ca chức năng và ảnh chụp dùng bộ bản vẽ nhỏ dựng lại từ cảnh mẫu của prototype (`design/prototype/js/scene.js`: nhà 2 tầng, mặt bằng kết cấu) **[đề xuất]**.
- Ngưỡng độ trễ theo DoD-L. Số mới do tệp này đặt ghi **[đề xuất]**.

## 1. Đo và truy vấn

### UII-001 · Nhãn đo, kích thước tạm và thẻ kết quả trên vùng vẽ
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD:MEASUREGEOM, CMD:DIST, CMD:AREA, UIA-008, UIA-013, RGN-013, UI0-011
- **Màn hình:** UI-I01, UI-I02
- **Đầu vào:** 07.2 UI-I01, UI-I02; 07.1 §9.14, §12. Prototype `#UI-I01`, `#UI-I02`: `cmd.js` › `IMPL.MEASUREGEOM`, `IMPL.DIST`, `IMPL.AREA`, `resultCard`; `cad.js` › `cad.quick`, `quickMove`, `quickClick`, `cad.pin`, `cad.card`; `canvas.js` › `View.prototype.quickRays`, `TC.roomAt`. API kết quả đo và jig của các lệnh MEASUREGEOM, DIST, AREA; định dạng số RGN-013.
- **Đầu ra:** `app/palettes/measure/`: `MeasureLabelLayer` (nhãn số đo neo vào điểm thế giới, vẽ trên lớp overlay qua UIA-013, không dùng widget cho nhãn), `MeasureResultCard` (thẻ nổi: các hàng giá trị, nút mm/m, Sao chép, Chèn thành text, Đóng), `MeasureFormatter` (RGN-013, quy đổi mm ↔ m, mm² ↔ m²). Kiểm thử `app/palettes/measure/tests/`, kịch bản `tests/ui/measure_*.json`.
- **Tiêu chí nghiệm thu:**
  1. Đo nhanh trên BENCH-STD 1 triệu entity: phần vẽ nhãn và kích thước tạm ≤ 2 ms/khung **[đề xuất]**; cả khung khi rê chuột ≤ 8 ms p99 (tầm trung), ≤ 16 ms (máy thấp).
  2. Nhãn và thẻ bám điểm thế giới khi pan, zoom, đổi DPI: lệch ≤ 1 px ở 100/150/200%. Thẻ luôn nằm trong vùng vẽ, cách mép ≥ 8 px.
  3. Chuỗi số trên thẻ trùng 100% dòng ghi trên dòng lệnh ở 50 ca (độ dài, diện tích, góc, delta X/Y). Nút mm/m đổi mọi hàng; Sao chép đặt vào clipboard đúng giá trị số theo đơn vị đang chọn.
  4. AREA cộng/trừ và Shift + bấm trong đo nhanh: tổng cộng dồn và số vùng cập nhật sau mỗi lần bấm, khớp tổng tính tay ở 10 ca.
  5. Bấm ngoài vùng kín: chú thích cạnh con trỏ "Không có vùng kín tại đây", tự ẩn sau 2 giây.
  6. Thoát lệnh (Esc, tùy chọn Thoát, gọi lệnh khác): không còn nhãn, thẻ hay primitive overlay nào (đếm bằng 0).
  7. Viewer và Viewer Pro đo đủ; nút Chèn thành text hiện huy hiệu Pro, bấm mở UI-L04.
  8. Đạt DoD-U, DoD-L.

### UII-002 · Cửa sổ văn bản (LIST, TEXTSCR) và lịch sử lệnh
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD:LIST, INT-017, NT-010, NT-012, UI0-006, UI0-008, UI0-014
- **Màn hình:** UI-I03
- **Đầu vào:** 07.2 UI-I03; 07.1 §8, §11 (F2). Prototype `#UI-I03`: `screens1.js` › `SC.textWindow`; `cmd.js` › `IMPL.LIST`, `IMPL.TEXTSCR`. Mô hình lịch sử dòng lệnh INT-017; văn bản kết quả của LIST, MASSPROP, STATUS, TIME, DBLIST.
- **Đầu ra:** `app/dialogs/textwindow/TextWindow.{h,cpp}` (cửa sổ không modal, view văn bản ảo hóa nhận dòng theo luồng), `TextSearchModel` (tìm ở nền, tô sáng, F3 / Shift+F3), lưu `.txt`. Kiểm thử và kịch bản QA-011 cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. F2 và TEXTSCR mở cửa sổ lịch sử lệnh. LIST, MASSPROP, STATUS, TIME, DBLIST mở cửa sổ kết quả có tiêu đề nêu lệnh và số đối tượng ("LIST · 12 đối tượng"). Khung đầu có nội dung ≤ 150 ms.
  2. DBLIST trên BENCH-STD 1 triệu entity: dòng đầu hiện ≤ 150 ms, phần còn lại đổ vào ở nền theo lô; cuộn 60 fps, không khựng quá 1 khung; bộ nhớ của view ≤ 1,5 lần dung lượng văn bản **[đề xuất]**.
  3. Tìm không phân biệt hoa thường, tùy chọn bỏ qua dấu tiếng Việt (NT-012): tô sáng mọi lần khớp, hiện "k/n". Trên 1 triệu dòng, lần khớp đầu ≤ 100 ms, luồng UI không bị chặn quá 16 ms **[đề xuất]**.
  4. Sao chép (vùng chọn hoặc toàn bộ) và Lưu ra tệp cho nội dung trùng từng ký tự với văn bản hiển thị; tệp UTF-8, tên mặc định `list.txt` hoặc `lich-su-lenh.txt`.
  5. Vị trí và kích thước cửa sổ được nhớ (NT-010). Esc đóng. Gọi lại khi đang mở thì đưa cửa sổ lên trước, không mở bản thứ hai.
  6. Đạt DoD-U, DoD-L.

### UII-003 · Bảng tổng chiều dài, tổng diện tích (TCLENGTH, TCAREA)
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** VN-015, RND-013, UI0-006, UI0-008, UI0-011, VN-151
- **Màn hình:** UI-I04
- **Đầu vào:** 07.2 UI-I04; 07.1 §7.9 (panel Đo và thống kê). Prototype `#UI-I04`: `screens3.js` › `totalsCmd`, `SC.totals`, `lenOf`, `areaOf`. API kết quả của VN-015 (tổng, nhóm theo layer và theo loại, số đối tượng bị bỏ qua); bộ ghi XLSX/CSV của VN-013.
- **Đầu ra:** `app/dialogs/totals/TotalsDialog.{h,cpp}` (không modal, đặt sát mép phải vùng vẽ), model nhóm theo layer/loại, menu Xuất. Kiểm thử và kịch bản `tests/ui/totals_*.json`.
- **Tiêu chí nghiệm thu:**
  1. Bố cục như prototype: tổng cỡ lớn; nút đơn vị (mm/m, mm²/m²); độ chính xác 0 đến 0.0000; bộ lọc loại kèm số đếm (loại không có đối tượng thì vô hiệu); tab Theo layer / Theo loại với cột Số, Tổng, Tỉ lệ; dòng nêu số đối tượng bị bỏ qua và lý do.
  2. Số liệu trùng 100% kết quả của VN-015 trên 20 vùng chọn mẫu, kể cả sau khi đổi bộ lọc loại.
  3. Bấm một nhóm: chỉ tô sáng nhóm đó (RND-013, không dựng lại batch), tổng đổi theo nhóm; bấm lại thì bỏ. Tắt ô "Tô sáng trên bản vẽ" hoặc đóng hộp thoại thì bỏ tô sáng.
  4. 100.000 đối tượng: hộp thoại có nội dung ≤ 150 ms sau khi lệnh trả kết quả; đổi bộ lọc, đơn vị, tab ≤ 16 ms.
  5. Sao chép đặt tổng đang hiện kèm đơn vị vào clipboard. Xuất Excel tạo tệp có 3 trang (tổng, theo layer, theo loại); CSV UTF-8; xong có toast "Mở tệp".
  6. Viewer Pro xem, sao chép, xuất đủ; nút "Chèn vào bản vẽ" có huy hiệu Pro. Nút Chọn lại chạy lại lệnh với hộp thoại mới.
  7. Đạt DoD-U, DoD-L.

### UII-004 · Chèn kết quả đo, tổng chiều dài, tổng diện tích vào bản vẽ
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UII-001, UII-003, CMD:MTEXT, CMD:TABLE, INT-012
- **Màn hình:** UI-I02, UI-I04
- **Đầu vào:** 07.2 UI-I02 (Chèn thành text), UI-I04 (chèn thành text hoặc bảng). Prototype: nút `data-totext` trong `cmd.js` › `resultCard`; nút `data-tt="insert"` trong `screens3.js` › `SC.totals`. API jig INT-012; lệnh MTEXT, TABLE.
- **Đầu ra:** `app/palettes/measure/InsertResult.{h,cpp}`: dựng nội dung chữ hoặc lưới bảng từ kết quả, xem trước theo con trỏ, gửi lệnh. Dùng lại ở UII-005. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Chèn thành text: chữ xem trước bám con trỏ ≤ 8 ms/khung; bấm điểm tạo 1 MTEXT đúng giá trị và đơn vị đang hiện, dấu chấm thập phân theo UNITS, kiểu chữ và chiều cao hiện hành (theo tỉ lệ chú thích nếu kiểu là annotative).
  2. Từ UI-I04 chọn Chữ (tổng) hoặc Bảng (Layer hoặc Loại, Số, Tổng, dòng Tổng cộng). Số trong bảng trùng số trong hộp thoại ở cùng độ chính xác.
  3. Mỗi lần chèn là 1 mark undo; Ctrl+Z xóa đúng các đối tượng vừa chèn.
  4. Chuỗi lệnh ghi trong nhật ký chạy lại bằng `.scr` cho DXF trùng 100%.
  5. Viewer và Viewer Pro: nút khóa, bấm mở UI-L04.
  6. Đạt DoD-U, DoD-L.

### UII-005 · Bảng diện tích phòng (TCROOMAREA)
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** VN-015, VN-013, UII-004, UIA-013, UI0-006, UI0-008, VN-154
- **Màn hình:** UI-I05
- **Đầu vào:** 07.2 UI-I05. Prototype `#UI-I05`: `screens12.js` › `IMPL.TCROOMAREA`, `raDialog`, `raOverlay`, `roomAdd`, `roomName`, `raTable`. API của VN-015 (dò vùng kín tại điểm, tên phòng từ chữ hoặc attribute); kiểu bảng `TCVN-dien-tich` (nội dung VN-008).
- **Đầu ra:** `app/dialogs/roomarea/RoomAreaDialog.{h,cpp}` (không modal), `RoomAreaOverlay` (tô vùng đã lấy, số thứ tự, dòng đang chọn đậm hơn), model bảng sắp xếp được. Kiểm thử và kịch bản QA-011 cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Chỉ điểm trong 7 phòng của bản vẽ mẫu: mỗi phòng thêm 1 dòng (STT, Tên phòng, Diện tích m², Chu vi m, Ghi chú). Chỉ lại phòng đã có: chú thích cạnh con trỏ "Phòng này đã có trong bảng", không thêm dòng. Không có vùng kín: gợi ý kiểm tra khe hở hoặc chọn polyline kín.
  2. Nguồn tên: Chữ trong vùng (chọn layer hoặc Mọi layer, bỏ qua chữ dạng số diện tích), Attribute (chọn tag, tự chọn tag chứa TEN, NAME hoặc PHONG), Nhập tay (dòng lệnh hỏi tên). Đổi nguồn thì đặt lại tên các dòng chưa sửa tay. Phòng chưa có tên được báo kèm số lượng.
  3. Làm tròn 2 chữ số; dòng Tổng cộng bằng tổng các số đã làm tròn, khớp từng dòng giữa hộp thoại, bảng chèn và tệp Excel.
  4. Sắp xếp theo STT, Tên (thứ tự tiếng Việt), Diện tích, Chu vi bằng chuột và bàn phím. Sửa tên, ghi chú rồi Tab sang ô kế không mất focus. Nút × bỏ một dòng; tùy chọn Hoàn tác bỏ phòng vừa thêm.
  5. Lớp phủ: phòng đã lấy tô nhạt có số thứ tự, dòng đang chọn đậm hơn; vẽ ≤ 2 ms/khung với 200 phòng **[đề xuất]**.
  6. Chèn bảng qua UII-004 (xem trước theo con trỏ, kiểu bảng `TCVN-dien-tich`), 1 mark undo. Ô "Tự cập nhật theo đường biên" vô hiệu, ghi "Sau v1.0".
  7. Đạt DoD-U, DoD-L.

## 2. So sánh bản vẽ

### UII-006 · Hộp thoại thiết lập so sánh và chạy so sánh ở nền
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD:COMPARE, DWG-026, RND-021, NT-010, NT-013, UI0-007, UI0-008, UI-B01
- **Màn hình:** UI-I07
- **Đầu vào:** 07.2 UI-I07 (Chọn, Thiết lập); 07.1 §12, §13.2 (token `compare.*`). Prototype `#UI-I07`: `screens3.js` › `SC.compare`, `cmpStart`; `REVS` là dữ liệu mẫu cho danh sách B. API engine COMPARE: đầu vào là snapshot A, đường dẫn B hoặc bản `.bak`, nhóm đối tượng, dung sai, cờ so định dạng chữ; đầu ra là danh sách thay đổi (thêm/xóa/sửa, đối tượng A/B, hộp bao, thuộc tính đổi).
- **Đầu ra:** `app/dialogs/compare/CompareSetupDialog.{h,cpp}`, `CompareSession` (giữ kết quả, phát sự kiện cho UII-007, UII-008). Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. A là bản vẽ hiện hành (thumbnail, tên, đường dẫn, ngày sửa). B là Tệp khác (danh sách gần đây, nút Duyệt… mở UI-B01) hoặc Bản lưu trước của tệp này (`.bak`; vô hiệu khi không có, hiện ngày lưu). Thumbnail B cập nhật ≤ 300 ms sau khi đổi lựa chọn **[đề xuất]**.
  2. Thiết lập: 5 nhóm đối tượng (Hình học, Chữ, Kích thước, Hatch, Block); dung sai bằng ô số UI-C09 (mặc định 0.01, phải lớn hơn 0); ô "So cả định dạng chữ". Thiết lập được nhớ theo người dùng.
  3. Bấm So sánh: chạy ở nền qua NT-013, chân hộp thoại hiện "Đang so sánh N đối tượng…" kèm Hủy. So BENCH-STD 20 MB với bản sửa của nó: luồng UI không bị chặn quá 16 ms; Hủy dừng ≤ 50 ms và không để lại trạng thái so sánh.
  4. B không đọc được (hỏng, phiên bản chưa hỗ trợ, đang bị khóa): hộp thoại lỗi có lời giải thích và chi tiết thu gọn (UI0-008), không vào chế độ so sánh.
  5. So sánh chỉ đọc: hash snapshot A và hash tệp B trước và sau khi so sánh trùng nhau.
  6. Viewer: lệnh khóa, huy hiệu Viewer Pro; Viewer Pro và Pro chạy đủ.
  7. Đạt DoD-U, DoD-L.

### UII-007 · Lớp phủ so sánh: chồng hình, hai khung cạnh nhau, đánh dấu thay đổi
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** UII-006, RND-012, RND-013, INT-016, UIA-008, UI0-002
- **Màn hình:** UI-I07
- **Đầu vào:** 07.2 UI-I07 (Chế độ xem); 07.1 §13.2, §13.3. Prototype `#UI-I07`: `screens3.js` › `TC.cmpDraw`, `TC.cmpClick`, `legend`, `pill`, `cloud`, `rectS`, overlay trong `TC.overlays`; token `--c-cmp-a`, `--c-cmp-b`, `--c-cmp-common` ở `css/tokens.css`.
- **Đầu ra:** `app/palettes/compare/CompareOverlay.{h,cpp}` (cờ tô theo entity qua RND-013, batch riêng cho đối tượng chỉ có ở B, nhãn ký hiệu và số thứ tự, khung đám mây/chữ nhật), `CompareSplitView` (hai nửa vùng vẽ dùng chung ma trận view), chú giải. Kịch bản QA-011 ảnh chụp và fps.
- **Tiêu chí nghiệm thu:**
  1. Chồng hình: đối tượng chỉ có ở A tô `compare.onlyA` nét 2 px, chỉ có ở B tô `compare.onlyB`, giống nhau tô `compare.common`. Mỗi thay đổi có nhãn ký hiệu (−, +, ~) và số thứ tự, nên vẫn đọc được khi không phân biệt màu.
  2. Hai khung cạnh nhau: A bên trái, B bên phải, có tiêu đề hai nửa. Pan hoặc zoom ở một khung thì khung kia theo cùng ma trận, lệch 0 px.
  3. Bật/tắt 3 nhóm (chỉ A, chỉ B, giống nhau) ≤ 16 ms; số lần dựng lại batch bằng 0.
  4. Pan/zoom BENCH-STD 1 triệu entity ở chế độ chồng hình ≥ 60 fps (tầm trung), ≥ 30 fps (máy thấp); hai khung cạnh nhau ≥ 30 fps (tầm trung) **[đề xuất]**.
  5. Khung đánh dấu: đám mây hoặc chữ nhật quanh hộp bao thay đổi cộng lề (0, 150, 300, 500, 1000 mm), cạnh tối thiểu 18 px trên màn hình; thay đổi đang chọn viền màu nhấn, đậm hơn.
  6. Bấm trên vùng vẽ: trúng hộp bao (cộng lề) thì chọn thay đổi đó; trượt thì chú thích "Không có thay đổi tại đây".
  7. Ảnh chụp QA-011 khớp baseline ở Tối và Sáng.
  8. Đạt DoD-U, DoD-L.

### UII-008 · Tab ngữ cảnh So sánh bản vẽ và palette danh sách thay đổi
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UII-007, UIA-005, UIA-017, UI0-006, UI0-010
- **Màn hình:** UI-I07
- **Đầu vào:** 07.2 UI-I07 (Danh sách thay đổi, Xuất); 07.1 §7.11. Prototype `#UI-I07`: `screens3.js` › `cmpTab` (7 panel: Bản vẽ, Chế độ xem, Hiện nhóm, Thay đổi, Đánh dấu, Xuất, Đóng), `P.cmp` (palette So sánh), `goTo`, `step`, `shown`, `cmpEnd`.
- **Đầu ra:** `app/contextual/compare/compare.ribbon.json` và binding; `app/palettes/compare/ChangeListPalette.{h,cpp}` (bộ lọc, điều hướng, bảng ảo hóa, dòng chi tiết). Kiểm thử và kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Vào chế độ so sánh: tab ngữ cảnh hiện, palette So sánh mở ở dock phải, vùng vẽ zoom vừa mọi thay đổi, dòng lệnh ghi "So sánh xong: N thay đổi (a thêm, b xóa, c sửa)". Đóng so sánh: tab, palette, lớp phủ, chú giải đều biến mất, view giữ nguyên.
  2. Trước/Sau và ◀ ▶ duyệt vòng trong các thay đổi đang hiện (theo bộ lọc và nhóm đang bật). Mỗi bước zoom tới thay đổi (khung tối thiểu 5 m) ≤ 100 ms và cập nhật "n / N" ở tab, palette và dòng trạng thái lệnh.
  3. Bộ lọc Tất cả / Thêm / Xóa / Sửa kèm số đếm. Với 5.000 thay đổi: lọc ≤ 16 ms, cuộn 60 fps.
  4. Dòng đang chọn có dòng chi tiết: đối tượng và thuộc tính đổi dạng "trước → sau" (ví dụ "Vị trí X: 5000.00 → 4700.00"), số theo UNITS.
  5. Danh sách sắp theo vị trí trên bản vẽ (trên xuống, trái sang); hai lần chạy cùng đầu vào cho cùng thứ tự.
  6. Tab chạy trên ribbon Viewer Pro (UIA-017) và ribbon Pro. Mọi nút có tooltip tên lệnh và dùng được bằng bàn phím; ↑/↓ trong bảng chuyển và zoom tới thay đổi.
  7. Đạt DoD-U, DoD-L.

### UII-009 · Đám mây đánh dấu và báo cáo so sánh
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UII-008, UI-M02, PLT-004, RND-021, NT-013
- **Màn hình:** UI-I07
- **Đầu vào:** 07.2 UI-I07 (Đánh dấu, Xuất); 07.3 UI-M02 (markup lưu tệp riêng). Prototype: `screens3.js` › `makeClouds`, nút `cmp-shot`, `cmp-pdf`, `cmp-csv` trong `cmpTab`. API markup của UI-M02; bộ ghi PDF PLT-004; render ngoài màn hình RND-021.
- **Đầu ra:** `app/palettes/compare/CompareMarkup.{h,cpp}`, `CompareReport.{h,cpp}` (PDF, CSV, ảnh chụp). Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Viewer Pro: Tạo đám mây thêm đúng N markup (mỗi thay đổi một, màu theo loại, chữ mô tả thay đổi) vào tệp markup cạnh bản vẽ; hash tệp DWG không đổi; toast có nút "Mở danh sách".
  2. Báo cáo PDF: trang tổng hợp (A, B, thời điểm, số thay đổi theo loại) và mỗi thay đổi một trang có ảnh vùng thay đổi. 500 thay đổi xuất ở nền ≤ 30 s (tầm trung), có tiến trình và Hủy **[đề xuất]**.
  3. CSV UTF-8: 1 dòng tiêu đề và đúng N dòng dữ liệu (#, Thay đổi, Đối tượng, Layer, Thuộc tính), số theo UNITS.
  4. Ảnh chụp: vùng đang xem kèm lớp phủ vào clipboard; "Lưu PNG" ghi ảnh đúng kích thước vùng vẽ nhân hệ số DPI.
  5. Đạt DoD-U, DoD-L.
- **Ngoài phạm vi:** đám mây REVCLOUD thật trong bản vẽ (Pro): UII-012.

### UII-010 · Nhập thay đổi từ bản vẽ B sang A
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** S · **Phụ thuộc:** UII-008, CMD:COMPARE, DOC-014
- **Màn hình:** UI-I07
- **Đầu vào:** 07.2 UI-I07 (Nhập đối tượng đã thay đổi từ B sang A, P2). API chép sâu giữa hai bản vẽ DOC-014.
- **Đầu ra:** hành động "Nhập vào A" trong `ChangeListPalette` và tab So sánh (chọn nhiều dòng, menu chuột phải); hộp xác nhận. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Chọn k thay đổi rồi Nhập vào A: đối tượng chỉ có ở B được chép sang A (layer, kiểu còn thiếu được tạo qua DOC-014); đối tượng bị sửa được thay bằng bản của B; đối tượng chỉ có ở A chỉ bị xóa khi người dùng chọn rõ "Xóa ở A" **[đề xuất]**.
  2. Hộp xác nhận nêu số đối tượng sẽ chép, thay, xóa; toàn bộ là 1 mark undo, Ctrl+Z trả A về hash cũ.
  3. Sau khi nhập, danh sách thay đổi cập nhật (mục đã nhập biến mất) ≤ 500 ms với 5.000 thay đổi.
  4. Viewer Pro: nút khóa, huy hiệu Pro.
  5. Đạt DoD-U, DoD-L.

### UII-012 · Tạo đám mây REVCLOUD từ kết quả so sánh (Pro)
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UII-009, CMD:REVCLOUD, DOC-007
- **Màn hình:** UI-I07
- **Đầu vào:** 07.2 UI-I07 (Đánh dấu: đám mây trong bản vẽ ở bản Pro). Prototype: `screens3.js` › `makeClouds`. Hình đám mây của lệnh REVCLOUD; danh sách thay đổi của UII-008.
- **Đầu ra:** hành động "Tạo đám mây trong bản vẽ" trong `app/palettes/compare/CompareMarkup.{h,cpp}`: mỗi thay đổi một REVCLOUD theo khung bao, hình và lề đang chọn. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Pro: tạo đúng N đám mây trên layer `TC-REVCLOUD` (tự tạo nếu chưa có) trong đúng 1 mark undo, theo hình và lề đang chọn.
  2. Chuỗi lệnh chạy lại bằng `.scr` cho DXF trùng 100%.
  3. 500 thay đổi tạo xong ≤ 1 s trên máy tầm trung **[đề xuất]**.
  4. Viewer Pro: hành động bị khóa, có huy hiệu Pro; đám mây ở Viewer Pro là markup của UII-009.
  5. Đạt DoD-U, DoD-L.

## 3. In và xuất

### UIH-001 · Hộp thoại In: thiết bị, khổ giấy, vùng in, tỉ lệ, tùy chọn in
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** PLT-001, PLT-002, PLT-003, PLT-005, CMD:PLOT, NT-013, QA-005, UI0-007, UI0-008, UI0-011
- **Màn hình:** UI-H02
- **Đầu vào:** 07.2 UI-H02; 07.1 §7.8, §11 (Ctrl+P), §12; 03 §13. Prototype `#UI-H02`: `screens2.js` › `SC.plot`, `sync`, `runPlot`; danh sách mẫu `PAPERS`, `DEVICES`, `CTBS`, `SCALES`. Prototype chưa giữ CTB, hướng giấy, số bản, offset, canh giữa, đặt cứng khổ A1 cho layout và dựng lại hộp khi mở rộng; task giữ đủ trạng thái. Mô hình thiết lập trang PLT-001, CTB PLT-002, pipeline in PLT-003, máy in hệ thống PLT-005.
- **Đầu ra:** `app/dialogs/plot/PlotDialog.{h,cpp}`, `PlotSettingsModel` (một nguồn trạng thái cho mọi trường, đọc/ghi cấu trúc thiết lập trang của PLT-001). Kiểm thử và kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Đủ nhóm của 07.2: Thiết lập trang (`<Không>`, `<Lần in trước>`, thiết lập có tên); Máy in (máy in hệ thống, PDF của TrionCAD, PNG; dòng thông tin vị trí; In ra tệp bắt buộc với PDF, PNG); Khổ giấy theo thiết bị; Số bản 1–999; Vùng in (Layout, Toàn bộ đối tượng, Màn hình hiện tại, Giới hạn bản vẽ, View đã đặt tên kèm danh sách view, Cửa sổ kèm nút chọn); Offset (Canh giữa, X/Y mm); Tỉ lệ (Vừa khổ giấy, tỉ lệ chuẩn, tùy chỉnh mm = đơn vị, co giãn độ dày nét). Phần mở rộng: Plot style, Viewport có shade, 8 tùy chọn in, Hướng giấy (Dọc, Ngang, In ngược).
  2. Giá trị ban đầu lấy từ thiết lập trang của layout (thiết bị, khổ, CTB, vùng in, tỉ lệ). Model chưa có thiết lập thì dùng Màn hình hiện tại và Vừa khổ giấy.
  3. Chọn thiết lập có tên chép mọi trường của thiết lập đó. "Thêm tùy chọn ▸" mở rộng mà không dựng lại hộp, giữ mọi giá trị đang sửa; trạng thái mở rộng nhớ theo người dùng.
  4. Cửa sổ: hộp ẩn, chỉ 2 góc trên vùng vẽ (có bắt điểm), hộp hiện lại kèm tọa độ; Esc giữ cửa sổ cũ.
  5. Bấm In: hộp đóng ngay, in chạy nền qua NT-013, thanh trạng thái hiện "Đang in 1/1 · p%" kèm nút hủy. Xong: toast "Đã in ra <tệp>" kèm Mở thư mục (thiết bị TrionCAD) hoặc "Đã gửi 1 tờ tới <máy in>". Hủy dừng ≤ 50 ms, không để lại tệp dở.
  6. PDF in qua hộp thoại trùng PDF của `-PLOT` cùng tham số chạy bằng `.scr`: so bằng QA-005, sai khác 0 pixel ở 300 DPI.
  7. Viewer, Viewer Pro: in, chọn máy in, khổ, vùng in, tỉ lệ, CTB đủ; nút Thêm…, Áp dụng cho layout, Lưu thay đổi vào layout, Sửa plot style, Thuộc tính máy in có huy hiệu Pro.
  8. Mở hộp ≤ 150 ms; đổi một trường ≤ 16 ms.
  9. Đạt DoD-U, DoD-L.

### UIH-002 · Xem trước nhỏ trực tiếp trong hộp thoại In
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIH-001, PLT-006, RND-021
- **Màn hình:** UI-H02
- **Đầu vào:** 07.2 UI-H02 (xem trước nhỏ có vùng in được). Prototype: phần vẽ canvas 400×260 trong `screens2.js` › `sync`; prototype dùng lề cố định 5 mm và không vẽ hướng giấy, offset. Engine xem trước PLT-006; render ngoài màn hình RND-021.
- **Đầu ra:** `app/dialogs/plot/PlotMiniPreview.{h,cpp}`: tờ giấy có bóng, vùng in được theo lề của thiết bị, vùng nội dung, ảnh nội dung thu nhỏ render ở nền và cache, nhãn kích thước. Kiểm thử và ảnh chụp QA-011.
- **Tiêu chí nghiệm thu:**
  1. Khung giấy, vùng in được, vùng nội dung theo vùng in, tỉ lệ, offset, canh giữa, hướng giấy cập nhật ≤ 16 ms sau mỗi thay đổi; ảnh nội dung thu nhỏ có trong ≤ 300 ms **[đề xuất]**.
  2. Phần nội dung vượt vùng in được tô cảnh báo kèm dòng chữ, không chỉ bằng màu.
  3. Vị trí vùng nội dung khớp tờ PDF in ra ở 20 tổ hợp, lệch ≤ 0,5 mm theo khổ giấy.
  4. Ảnh chụp khớp baseline ở Tối và Sáng, 100/150/200%.
  5. Đạt DoD-U, DoD-L.

### UIH-003 · Quản lý thiết lập trang (PAGESETUP): xem và đặt hiện hành
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIH-001, PLT-001, CMD:PAGESETUP, UI0-006, UI0-008, UI0-011
- **Màn hình:** UI-H01
- **Đầu vào:** 07.2 UI-H01; 03 §13. Prototype `#UI-H01`: `screens2.js` › `SC.pageSetup`; `cmd.js` › `IMPL.PAGESETUP`. Prototype lỗi khi bản vẽ có dưới 2 thiết lập và nút "In với thiết lập này…" không truyền dòng đang chọn; task sửa cả hai.
- **Đầu ra:** `app/dialogs/plot/PageSetupManager.{h,cpp}`. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Danh sách: dòng đầu `*Tên layout*` hoặc `*Model*` (hiện hành), sau đó các thiết lập trang có tên. Khung chi tiết chỉ đọc: thiết bị, khổ giấy và hướng, nơi in, plot style, mô tả.
  2. Bản vẽ không có thiết lập có tên: trạng thái rỗng có hướng dẫn, không lỗi.
  3. Đặt hiện hành: ở Pro ghi vào layout (1 mark undo); ở Viewer chỉ trong phiên, toast ghi rõ "không lưu vào tệp".
  4. "In với thiết lập này…" mở UIH-001 với đúng thiết lập đang chọn.
  5. Mới, Sửa, Nhập hiện huy hiệu Pro ở Viewer, Viewer Pro (phần Pro làm ở UIH-004).
  6. Đạt DoD-U, DoD-L.

### UIH-004 · Thiết lập trang có tên và phần Pro của hộp thoại In
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIH-003, PLT-001, CMD:PAGESETUP, DOC-016, NT-010, UI-D05
- **Màn hình:** UI-H01, UI-H02
- **Đầu vào:** 07.2 UI-H01 (Mới, Sửa, Nhập, Hiện khi tạo layout mới), UI-H02 (Thêm…, Áp dụng cho layout, Lưu thay đổi vào layout, Sửa plot style). Prototype: chế độ thiết lập trang `SC.plot(true)` trong `screens2.js` (chưa nơi nào gọi). Nhập thiết lập từ bản vẽ khác PLT-001; nạp lười DOC-016.
- **Đầu ra:** chế độ thiết lập trang của `PlotDialog` (tiêu đề "Thiết lập trang · <layout>", nút "Lưu thiết lập", không có Số bản và In); các nút Mới, Sửa, Nhập của `PageSetupManager`; hộp Nhập thiết lập trang. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Mới (tên, bắt đầu từ `<Không>` hoặc thiết lập có sẵn), Sửa, Nhập, Thêm…, Áp dụng cho layout, Lưu thay đổi vào layout: mỗi lần 1 mark undo; kết quả trùng lệnh dòng lệnh tương ứng chạy bằng `.scr` (so DXF).
  2. Nhập từ DWG/DWT/DXF: đọc tệp nguồn ở nền bằng nạp lười, không mở thành tab; danh sách thiết lập có trong ≤ 500 ms với tệp 20 MB **[đề xuất]**; chọn nhiều; trùng tên hỏi "Ghi đè thiết lập X?".
  3. Lưu thiết lập ghi đủ mọi trường: đọc lại PLOTSETTINGS qua DOC-017 trùng giá trị trong hộp ở 15 tổ hợp.
  4. Sửa… plot style mở UI-D05 với CTB đang chọn; đóng UI-D05 thì danh sách CTB cập nhật.
  5. Ô "Hiện khi tạo layout mới" nhớ trong Tùy chọn; tạo layout mới thì mở trình quản lý khi ô bật.
  6. Đạt DoD-U, DoD-L.

### UIH-005 · Xem trước bản in toàn màn hình (PREVIEW)
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIH-001, PLT-006, RND-014, CMD:PREVIEW, QA-005
- **Màn hình:** UI-H03
- **Đầu vào:** 07.2 UI-H03; 07.1 §12. Prototype `#UI-H03`: `screens2.js` › `SC.preview`. Prototype ép màu đen, không áp CTB, nút Pan và Zoom cửa sổ chưa có xử lý, nút In bỏ mất thiết lập; task làm theo đặc tả (độ dày nét theo CTB). Engine xem trước PLT-006.
- **Đầu ra:** `app/dialogs/plot/PlotPreviewWindow.{h,cpp}`: lớp phủ toàn màn hình, thanh công cụ (In, Pan, Zoom, Zoom cửa sổ, Zoom về ban đầu, Đóng), thanh trạng thái (khổ giấy, CTB, tỉ lệ, gợi ý phím). Kiểm thử và kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Ảnh xem trước trùng tờ in thật: so với PDF cùng thiết lập bằng QA-005, SSIM ≥ 0,99 ở 20 tờ mẫu (màu, độ dày nét, screening theo CTB) **[đề xuất]**.
  2. Tờ A1 dày của BENCH-STD (nhiều viewport): khung đầu có nội dung ≤ 1 s, sau đó tinh chỉnh dần; pan/zoom 60 fps.
  3. Lăn chuột zoom tại con trỏ, kéo để pan, Zoom cửa sổ bằng 2 góc, Zoom về ban đầu, Esc đóng. Nút In in đúng thiết lập đang xem trước.
  4. Có ở mọi phiên bản.
  5. Đạt DoD-U, DoD-L.

### UIH-006 · Tùy chọn xuất PDF (EXPORTPDF)
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLT-004, CMD:EXPORTPDF, NT-013, UI0-008
- **Màn hình:** UI-H05
- **Đầu vào:** 07.2 UI-H05; 07.1 §7.8 (nút PDF tách: layout hiện tại, tất cả layout, vùng đang xem, cửa sổ). Prototype `#UI-H05`: `screens2.js` › `SC.exportPdf`, `PRESETS`. Prototype không đổi tên tệp theo vùng xuất, không chọn cửa sổ, không có nút hủy; task làm đủ. Bộ ghi PDF PLT-004.
- **Đầu ra:** `app/dialogs/plot/ExportPdfDialog.{h,cpp}`, bảng preset. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. 4 preset (Chất lượng cao: vector 2400 DPI, giữ layer, chữ SHX tìm được; Dung lượng nhỏ: 600 DPI, gộp nét chồng, nén ảnh; Xem trên web, điện thoại: 400 DPI, nhúng font, bookmark; Tùy chỉnh) đặt đúng các trường; đổi một trường thì preset chuyển Tùy chỉnh.
  2. Mọi tùy chọn truyền đúng sang PLT-004: PDF đầu ra có hoặc không có layer OCG, bookmark, chữ SHX tìm được, font nhúng, nét chuyển đường viền đúng theo lựa chọn ở 12 tổ hợp.
  3. Vùng Cửa sổ chọn 2 góc như UIH-001; tên tệp tự đổi theo vùng xuất (`<bản vẽ>-<layout>.pdf`, tất cả layout là `<bản vẽ>.pdf`).
  4. Xuất chạy nền, tiến trình ở thanh trạng thái có nút hủy (≤ 50 ms, không để tệp dở); xong toast "Đã xuất <tệp> · N trang" kèm Mở thư mục; mở PDF khi xong nếu chọn.
  5. Ước tính dung lượng lệch ≤ 30% so với tệp thật ở 10 bản vẽ mẫu **[đề xuất]**.
  6. Đạt DoD-U, DoD-L.

### UIH-007 · Xuất ảnh (PNGOUT, JPGOUT, TIFOUT, BMPOUT)
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** PLT-007, CMD:PNGOUT, NT-013, UI0-008
- **Màn hình:** UI-H09
- **Đầu vào:** 07.2 UI-H09; 03 §13. Prototype `#UI-H09`: `screens2.js` › `SC.exportImage`. Prototype ghi kích thước ảnh cố định và chưa nối Vùng; task tính thật. Engine xuất ảnh PLT-007.
- **Đầu ra:** `app/dialogs/plot/ExportImageDialog.{h,cpp}`. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Ảnh xuất đúng kích thước pixel đã chọn (DPI hoặc pixel) ở 8 tổ hợp; nền trong suốt chỉ bật với PNG, chọn định dạng khác thì tắt kèm ghi chú.
  2. Xem trước và dòng ước tính (kích thước pixel, dung lượng) cập nhật theo mọi tùy chọn ≤ 100 ms.
  3. Ảnh 8K từ BENCH-STD 1 triệu entity xuất ở nền có tiến trình và hủy; trùng pixel với ảnh do lệnh dòng lệnh tạo cùng tham số.
  4. Đạt DoD-U, DoD-L.

### UIH-008 · Kiểm tra trước khi in
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UIH-001, PLT-003, RGN-006, UI-J02
- **Màn hình:** UI-H02
- **Đầu vào:** 07.2 UI-H02 (Kiểm tra trước khi in, TrionCAD, P1). Prototype: danh sách cảnh báo trong `screens2.js` › `sync`. Phân tích của pipeline in PLT-003; danh sách font bị thay của RGN-006.
- **Đầu ra:** khối "Kiểm tra trước khi in" trong `PlotDialog`, tính ở nền khi thiết lập đổi (gom thay đổi trong 150 ms) **[đề xuất]**. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Đủ 4 loại cảnh báo của 07.2 (font bị thay, đối tượng ngoài vùng in, plot style không tìm thấy, nét mảnh hơn 0,1 mm sau CTB): mỗi loại báo đúng trên 1 bản vẽ dựng sẵn; 20 bản vẽ sạch không có cảnh báo sai.
  2. Tính ở nền, cập nhật ≤ 300 ms sau khi đổi thiết lập với tờ A1 dày; hộp thoại không khựng.
  3. Cảnh báo không chặn nút In. Mỗi cảnh báo có hành động: font mở Bác sĩ Font (UI-J02), đối tượng ngoài vùng in zoom tới và tô sáng.
  4. Đạt DoD-U, DoD-L.

### UIH-009 · In nhiều layout, nhiều bản vẽ (PUBLISH): danh sách tờ và tệp .dsd
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** PLT-008, CMD:PUBLISH, DOC-018, UI0-006, UI0-008, UI0-011, UI-B01
- **Màn hình:** UI-H04
- **Đầu vào:** 07.2 UI-H04 (Danh sách tờ); 03 §13. Prototype `#UI-H04`: `screens4.js` › `SC.publish`, `IMPL.PUBLISH`. Prototype chỉ có toast cho `.dsd` và "Từ tệp…"; task làm thật. Đọc/ghi `.dsd` và danh sách tờ của PLT-008.
- **Đầu ra:** `app/dialogs/plot/PublishDialog.{h,cpp}` phần danh sách tờ: Thêm tờ ▾ (từ bản vẽ đang mở, từ tệp), Gồm (Chỉ layout, Chỉ Model, Model và layout), Bỏ, Lên, Xuống, Nạp/Lưu danh sách; bảng (chọn, Tên tờ, Thiết lập trang theo từng tờ, Khổ/thiết bị, Trạng thái). Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Thêm từ 3 bản vẽ đang mở và 20 tệp chưa mở: tệp chưa mở được đọc danh sách layout và thiết lập ở nền, ≤ 300 ms mỗi tệp 20 MB, không mở thành tab **[đề xuất]**.
  2. Thiết lập trang đổi riêng từng tờ. Trạng thái kiểm trước (Sẵn sàng; Chưa có thiết lập, in vùng đang xem; Không tìm thấy thiết lập trang; Thiết lập in ra PDF) đúng ở 10 ca.
  3. Lưu rồi nạp `.dsd` cho cùng danh sách và thiết lập; `.dsd` do AutoCAD lưu (bộ mẫu của doanh nghiệp pilot) nạp đúng tờ; tệp hỏng báo lỗi, không crash.
  4. Sắp xếp bằng nút, kéo-thả, Alt+↑/↓; 500 tờ cuộn 60 fps. Danh sách rỗng có hướng dẫn.
  5. Viewer: lệnh khóa, huy hiệu Viewer Pro.
  6. Đạt DoD-U, DoD-L.

### UIH-010 · PUBLISH: đích, mẫu tên tệp, chạy nền
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIH-009, UIH-005, UII-002, PLT-008, NT-013, UIA-009
- **Màn hình:** UI-H04
- **Đầu vào:** 07.2 UI-H04 (Đích, Tùy chọn PDF, Khác). Prototype: `screens4.js` › `SC.publish` (đích, mẫu tên, chạy), bộ mô phỏng tác vụ `simulate`, `stCell`. Prototype chưa nối "In ở nền", "Mở thư mục khi xong"; task làm đủ.
- **Đầu ra:** phần đích và chạy của `PublishDialog`: Đích (Máy in theo thiết lập trang, PDF, PNG), PDF một tệp nhiều trang hoặc mỗi tờ một tệp, mẫu tên tệp có token (TenBanVe, Layout, KYHIEU từ khung tên, STT), thư mục, số bản; trạng thái từng tờ; nhật ký. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Mẫu tên: chèn token tại con trỏ; xem trước 3 tên đầu cập nhật ≤ 16 ms mỗi phím; tên trùng tự thêm hậu tố `-2`, ký tự cấm trong tên tệp được thay và báo trước khi chạy **[đề xuất]**.
  2. Xuất 50 tờ PDF ở nền: trạng thái từng tờ (Chờ, %, Xong, Lỗi, Xong có cảnh báo, Đã hủy); Tạm dừng dừng sau tờ đang in; Hủy dừng ≤ 50 ms; đóng hộp vẫn chạy, thanh trạng thái hiện "In nhiều layout k/n".
  3. Tờ lỗi (thiếu thiết lập, máy in lỗi) ghi nhật ký, tờ khác vẫn chạy; xong toast "In nhiều layout: ok/n tờ xong" kèm Mở thư mục (khi bật).
  4. Một tệp nhiều trang: đủ N trang đúng thứ tự danh sách, bookmark theo tên tờ.
  5. Xem trước mở UIH-005 cho tờ đang chọn; Nhật ký mở cửa sổ văn bản UII-002.
  6. Đạt DoD-U, DoD-L.

### UIH-011 · Cấu hình máy in PC3 trong hộp thoại In
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UIH-001, UIH-002, PLT-010
- **Màn hình:** UI-H02, UI-H10
- **Đầu vào:** 07.2 UI-H10 (trước P2 chỉ đọc khổ giấy từ PC3 có sẵn, 1.0); 03 §13. Bộ đọc PC3/PMP của PLT-010.
- **Đầu ra:** danh sách máy in của `PlotDialog` gồm các cấu hình PC3 trong thư mục Plotters; khổ giấy và lề in được lấy từ PC3/PMP; nút "Thuộc tính…" hiện thông tin chỉ đọc. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Thư mục Plotters có 10 tệp PC3 mẫu (AutoCAD 2018–2025): mọi cấu hình có trong danh sách máy in; khổ giấy và lề in được trùng giá trị đọc bằng PLT-010.
  2. PC3 thiếu driver: dòng có trạng thái "Thiếu driver"; nút In vô hiệu kèm lý do.
  3. Xem trước nhỏ (UIH-002) dùng lề của PC3.
  4. Đạt DoD-U, DoD-L.

### UIH-012 · Dấu in (PLOTSTAMP)
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** S · **Phụ thuộc:** UIH-001, UIH-010, CMD:PLOTSTAMP, QA-005
- **Màn hình:** UI-H02, UI-H04
- **Đầu vào:** 07.2 UI-H02 (Dấu in, P2); 03 §13 (PLOTSTAMP P2). Prototype: ô "Bật dấu in" (chưa có hộp thiết lập), "Thiết lập dấu in…" ở PUBLISH (toast P2).
- **Đầu ra:** `app/dialogs/plot/PlotStampDialog.{h,cpp}`: chọn trường (tên bản vẽ, layout, ngày giờ, người in, máy in, khổ giấy, tỉ lệ), trường tự đặt, vị trí (4 góc, ngang hoặc dọc), offset, font và cỡ chữ, xem trước, lưu và nạp tệp thiết lập. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Dấu in hiện trên PDF đúng vị trí, hướng, offset, font, cỡ ở 8 tổ hợp (so ảnh bằng QA-005).
  2. Ngày theo dd/mm/yyyy; trường tự đặt có trong dấu in; lưu rồi nạp tệp thiết lập cho cùng kết quả.
  3. Ô "In dấu in" ở PUBLISH (UIH-010) dùng cùng thiết lập.
  4. Đạt DoD-U, DoD-L.

### UIH-013 · Quản lý máy in và trình sửa PC3 (PLOTTERMANAGER)
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** UIH-011, PLT-010, UI0-006, UI0-008, CMD:PLOTTERMANAGER
- **Màn hình:** UI-H10
- **Đầu vào:** 07.2 UI-H10; 03 §13 (PC3, PMP: P2 soạn). Prototype `#UI-H10`: `screens17.js` › `SC.pc3`, dữ liệu `PCS`. Lưu ý thứ tự lề PC3 là [trên, dưới, trái, phải], lề layout là [trái, trên, phải, dưới]. Bộ đọc/ghi PC3 của PLT-010.
- **Đầu ra:** `app/dialogs/plot/PlotterManagerDialog.{h,cpp}` (1010×620): danh sách cấu hình (tìm, nhãn Đã sửa, Lỗi, Mặc định; Thêm máy in ▾), tab Chung, Cổng, Thiết lập thiết bị (Giấy, Đồ họa, Khổ giấy và hiệu chỉnh, Lọc khổ giấy); Lưu thành…, Bỏ sửa, Lưu n cấu hình. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Sửa rồi lưu PC3: đọc lại bằng PLT-010 trùng mọi trường đã sửa; tệp lưu ra mở được bằng AutoCAD (kiểm qua doanh nghiệp pilot, cùng kênh QA-007) **[đề xuất]**.
  2. Lưu nhiều cấu hình một lần; đóng khi chưa lưu hỏi "Bỏ thay đổi ở n cấu hình máy in?".
  3. Tên cấu hình kiểm trùng và ký tự cấm; để trống khi rời ô thì trả tên cũ.
  4. Khổ bị lọc không còn trong danh sách khổ của hộp In (UIH-001).
  5. Cấu hình thiếu driver: thông báo lỗi kèm "Chọn driver khác" và "Xóa cấu hình…" (có xác nhận).
  6. Đạt DoD-U, DoD-L.

### UIH-014 · Khổ giấy tùy chỉnh và hiệu chỉnh tỉ lệ máy in (PMP)
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** UIH-013, PLT-010, CMD:PLOTTERMANAGER
- **Màn hình:** UI-H10
- **Đầu vào:** 07.2 UI-H10 (khổ giấy tùy chỉnh, hiệu chỉnh PMP). Prototype: `screens17.js` › `sizeWizard`, `pmpWizard`, mục "Sửa vùng in được".
- **Đầu ra:** trình hướng dẫn khổ tùy chỉnh (Bắt đầu, Kích thước, Vùng in được, Tên khổ, Xong), trình hướng dẫn PMP (khung hiệu chỉnh 400 × 250 mm, nhập kích thước đo được, hệ số hiệu chỉnh), sửa lề in được (0–50 mm, về lề của driver). Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Khổ tạo qua trình hướng dẫn có trong hộp In và in đúng kích thước (đo trên PDF, lệch ≤ 0,1 mm).
  2. Kiểm hợp lệ: rộng, cao lớn hơn 0; khổ cuộn rộng ≤ khổ cuộn của máy; dài ≤ 15.000 mm; lề nhỏ hơn khổ; tên không trùng. Thông báo ở đúng bước.
  3. PMP: hệ số = kích thước đo / kích thước chuẩn (5 chữ số lẻ); lệch quá 5% bị từ chối; tên tệp khớp `^[\w.-]+\.pmp$`.
  4. Đạt DoD-U, DoD-L.

## 4. In hàng loạt theo khung tên và bộ bản vẽ

### UIH-015 · In hàng loạt theo khung tên: khung 4 bước và bước Nhận diện khung
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** PLT-009, CMD:TCBATCHPLOT, RND-013, UI0-006, UI0-008, UI0-011
- **Màn hình:** UI-H06
- **Đầu vào:** 07.2 UI-H06 (bước 1); 03 §13 (khác biệt VN); 07.1 §7.8, §7.9. Prototype `#UI-H06`: `screens4.js` › `SC.batchPlot` (bước `s1`), `frameBlocks`, `findFrames`, trạng thái `BP`. Engine nhận diện khung, sắp thứ tự, suy khổ của PLT-009.
- **Đầu ra:** `app/dialogs/batchplot/BatchPlotDialog.{h,cpp}` (không modal, 820×640, đặt sát mép trái vùng vẽ; dải 4 bước, thanh Mẫu in và tóm tắt "N khung sẽ in"), `FrameDetectStep`. Kiểm thử và kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Ba cách nhận diện: theo tên block khung tên (bảng block có attribute kèm số lần dùng ở Model, layout và nhãn "đề xuất"; chọn nhiều), theo layer của hình chữ nhật viền khung, theo đối tượng mẫu. Kết quả trùng PLT-009 trên bộ 20 bản vẽ VN có 1–60 khung trong Model (chọn từ QA-001) **[đề xuất]**.
  2. Quét ở nền, kết quả hiện dần; bản vẽ 100 MB với 200 khung: kết quả đầu ≤ 500 ms, luồng UI không bị chặn quá 16 ms **[đề xuất]**.
  3. Khung tìm được tô viền và đánh số trên vùng vẽ (RND-013); đổi cách nhận diện cập nhật ≤ 100 ms sau khi engine trả kết quả.
  4. Chọn khung mẫu: hộp ẩn, bấm viền khung, quay lại với "Khung mẫu: block X. Tìm thấy N khung giống."; bấm vào đối tượng không phải khung thì cảnh báo, kết quả không đổi.
  5. Phạm vi Model, Các layout (N); "Nhiều tệp" vô hiệu kèm nhãn "1.0" (làm ở UIH-019). Không tìm thấy khung: trạng thái rỗng gợi ý cách khác, nút Tiếp vô hiệu.
  6. Hộp không modal: vẫn pan, zoom, chọn trên vùng vẽ khi hộp mở. Dải bước chỉ cho tới bước đã đủ điều kiện.
  7. Viewer: lệnh khóa, huy hiệu Viewer Pro.
  8. Đạt DoD-U, DoD-L.

### UIH-016 · In hàng loạt theo khung tên: bước Danh sách khung
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** UIH-015, PLT-009, RND-021, INT-016, UI0-006
- **Màn hình:** UI-H06
- **Đầu vào:** 07.2 UI-H06 (bước 2). Prototype: `screens4.js` (bước `s2`, `paperOf`, `bpName`, `zoomBeside`). Prototype giả định polyline có tỉ lệ 100 và không dùng khổ, hướng, tỉ lệ sửa tay khi in; task dùng đủ.
- **Đầu ra:** `FrameListStep`: Lưới thumbnail và Bảng (chọn, #, Tên tờ, Khổ giấy, Hướng, Tỉ lệ, Ở), Thứ tự, Chọn tất cả, Bỏ chọn, bộ đếm "n/N khung sẽ in". Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Khổ giấy, hướng, tỉ lệ tự suy ra trùng PLT-009 trên bộ 20 bản vẽ; giá trị sửa tay của một khung được dùng khi in (kiểm kích thước trang PDF).
  2. 200 khung: lưới cuộn 60 fps; thumbnail render ở nền theo thứ tự hiển thị, ≤ 100 ms mỗi khung **[đề xuất]**, dừng khi rời bước.
  3. Ba kiểu thứ tự (Trái → phải, trên → dưới; Trên → dưới, trái → phải; Theo số hiệu attribute) cho đúng thứ tự ở 10 ca, kể cả khung lệch hàng ±5% chiều cao; hai lần chạy cùng thứ tự.
  4. Bấm một khung: vùng vẽ zoom vừa khung (×1,12) trong phần không bị hộp che ≤ 100 ms, khung được tô sáng, có lịch sử zoom; đóng hộp thì bỏ tô sáng.
  5. Tên tờ lấy từ attribute theo mẫu, mặc định `{KYHIEU} {TENBV}`; khung thiếu attribute dùng "Khung i". Khung bỏ chọn hiện mờ.
  6. Đạt DoD-U, DoD-L.

### UIH-017 · In hàng loạt theo khung tên: bước Thiết lập in, mẫu tên tệp, Mẫu in
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIH-016, PLT-002, PLT-009, NT-010
- **Màn hình:** UI-H06
- **Đầu vào:** 07.2 UI-H06 (bước 3, Lưu cấu hình thành Mẫu in). Prototype: `screens4.js` (bước `s3`, `BP_TPL`). Mẫu tên mặc định của prototype là `{KYHIEU}_{TENBV}` (07.2 ghi ví dụ `{SoHieu}_{TenBanVe}.pdf`); prototype chưa lưu mẫu in; task lưu thật.
- **Đầu ra:** `PlotSetupStep`: thiết bị (máy in hoặc PDF; PDF một tệp nhiều trang hoặc mỗi khung một tệp), CTB, In độ dày nét, Canh giữa hoặc offset, mẫu tên tệp có token (KYHIEU, TENBV, TILE, CONGTRINH, TenBanVe, STT, Ngay), "Bỏ dấu tiếng Việt, thay khoảng trắng bằng “-”", thư mục đích, Mở thư mục khi xong; quản lý Mẫu in (lưu, chọn, đổi tên, xóa). Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Tên tệp thay token bằng giá trị attribute: KT-01 + MẶT BẰNG TẦNG 1 cho `KT-01_MAT-BANG-TANG-1.pdf`. Bỏ dấu đúng mọi chữ tiếng Việt kể cả đ → d và chữ hoa; ký tự cấm của Windows được thay; tên trùng tự thêm hậu tố. Xem trước 4 tên đầu cập nhật ≤ 16 ms mỗi phím.
  2. Lưu Mẫu in ghi đủ cách nhận diện, thứ tự, thiết bị, kiểu PDF, CTB, mẫu tên, bỏ dấu, thư mục, offset. Chọn mẫu ở bản vẽ khác cho đúng mọi trường (so từng trường).
  3. CTB không có trên máy: cảnh báo, phải chọn CTB khác trước khi chạy.
  4. Đạt DoD-U, DoD-L.

### UIH-018 · In hàng loạt theo khung tên: chạy nền, tạm dừng, hủy, báo cáo
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIH-017, PLT-009, NT-013, UIA-009, UII-002
- **Màn hình:** UI-H06
- **Đầu vào:** 07.2 UI-H06 (bước 4); 07.1 §1 (thao tác dài chạy nền), §12. Prototype: `screens4.js` (bước `s4`, `start`, `simulate`, `stCell`). Tác vụ nền NT-013.
- **Đầu ra:** `RunStep`: ô tóm tắt, tiến trình tổng, bảng từng tờ (#, Tên tờ, Tệp, Trạng thái), ghi chú, nút Chạy ở nền, Tạm dừng/Tiếp tục, Hủy, Xem báo cáo, Mở thư mục, Thiết lập lại, Đóng. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. 60 khung, mỗi khung một PDF: chạy nền qua NT-013, luồng UI không bị chặn quá 16 ms; tiến trình từng tờ và tổng cập nhật ≥ 4 lần mỗi giây.
  2. Tạm dừng dừng sau tờ đang in, Tiếp tục chạy tiếp; Hủy dừng ≤ 50 ms, tờ dở bị xóa, tờ đã xong giữ nguyên, báo cáo nêu số tờ xong trước khi hủy.
  3. Đóng hộp khi đang chạy: tác vụ vẫn chạy, thanh trạng thái hiện "In hàng loạt k/n" (có biểu tượng tạm dừng khi dừng); mở lại hộp thì vào bước 4 với tiến trình hiện tại.
  4. Báo cáo mỗi tờ một dòng (thành công, lỗi kèm lý do, cảnh báo font thay thế). Xong: toast "In hàng loạt xong N tờ…" kèm Mở thư mục; dòng lệnh ghi "TCBATCHPLOT: ok/n tờ".
  5. Đạt DoD-U, DoD-L.

### UIH-019 · In hàng loạt theo khung tên từ nhiều tệp
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIH-018, PLT-009, DOC-016, UI-B01
- **Màn hình:** UI-H06
- **Đầu vào:** 07.2 UI-H06 (Phạm vi nhiều tệp, P1; phát hành 1.0 nhiều tệp). Prototype: ô "Nhiều tệp [bản 1.0]" vô hiệu trong bước 1.
- **Đầu ra:** phạm vi Nhiều tệp trong `FrameDetectStep` (thêm tệp, thư mục, gồm thư mục con; danh sách tệp kèm số khung và trạng thái), danh sách khung nhóm theo tệp. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. 30 tệp DWG 20 MB: đọc ở nền song song bằng nạp lười, số khung từng tệp hiện dần; Hủy ≤ 50 ms; tệp lỗi (hỏng, đang bị khóa) liệt kê riêng, không dừng cả lượt.
  2. Danh sách khung nhóm theo tệp, theo thứ tự tệp rồi quy tắc thứ tự của bước 2; token `{TenBanVe}` lấy đúng tên tệp nguồn.
  3. In 300 khung từ 30 tệp: bộ nhớ đỉnh ≤ 1,5 GB, tệp in xong được giải phóng **[đề xuất]**.
  4. Tệp nguồn không mở thành tab và không đổi (hash).
  5. Đạt DoD-U, DoD-L.

### UIH-020 · Palette Quản lý bộ bản vẽ: thông tin dự án, danh sách tờ, số hiệu theo bộ môn
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** VN-018, DOC-018, UI0-006, UI0-010, UI0-011, UI-J05
- **Màn hình:** UI-H07
- **Đầu vào:** 07.2 UI-H07; 03 §14 (Quản lý bộ bản vẽ kiểu VN). Prototype `#UI-H07`: `screens11.js` › `P.sheets`, `sheets`, `nav`, `IMPL.TCSHEETS`, dữ liệu `PJ`, `STAGES`, `BMS`, `SS`; phần nằm sau `SC.ssm &&` thuộc UI-H08. Engine bộ bản vẽ VN-018 (danh mục tờ, số hiệu, lưu bộ).
- **Đầu ra:** `app/palettes/sheets/SheetSetPalette.{h,cpp}` (dock trái): đầu palette (tên bộ, số tờ, số tệp, nút +, menu ⋯), Thông tin dự án (công trình, hạng mục, chủ đầu tư, địa điểm, đơn vị thiết kế, logo, giai đoạn, chủ trì, chủ nhiệm, kiểm tra), Danh sách tờ nhóm theo bộ môn, khung chi tiết tờ. Kiểm thử và kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Tờ lấy từ các layout và khung tên trong Model của mọi tệp trong bộ (tệp chưa mở đọc ở nền). Khổ giấy, tỉ lệ, ngày, bộ môn suy ra đúng quy tắc của VN-018 trên bộ mẫu 3 dự án, tổng ≥ 60 tờ.
  2. Số hiệu theo bộ môn KT, KC, ĐN, CN, ME, dạng "KT 01" hoặc "KT 1/18". Kéo-thả hoặc Alt+↑/↓ đổi thứ tự trong cùng bộ môn và đánh số lại ≤ 16 ms; số đặt tay được giữ.
  3. Cột trạng thái: Thiếu tệp, Đã in (kèm ngày), Sửa sau khi in, Chưa in. Sửa đối tượng trong khung (Model) hoặc trong layout đã in thì tờ đó chuyển "Sửa sau khi in" ≤ 100 ms sau lệnh.
  4. 500 tờ: palette có nội dung ≤ 150 ms, cuộn 60 fps; tìm không phân biệt dấu ≤ 16 ms mỗi phím.
  5. Mở tờ (Enter, nhấp đúp): chuyển bản vẽ, không gian, zoom tới khung, tô sáng 2,5 s. Menu chuột phải: Mở tờ, Sửa khung tên… (UI-J05), Bộ môn ▸, Lên, Xuống, đánh dấu đã in, Bỏ khỏi bộ (hoàn tác được).
  6. Lưu bộ rồi mở lại cho cùng dữ liệu: thông tin dự án, thứ tự, số hiệu tay, trạng thái in.
  7. Viewer, Viewer Pro: palette khóa, trạng thái rỗng nêu tính năng kèm nút dùng thử.
  8. Đạt DoD-U, DoD-L.

### UIH-021 · Ánh xạ khung tên và "Cập nhật tất cả khung tên"
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIH-020, VN-018, VN-010, NT-013
- **Màn hình:** UI-H07
- **Đầu vào:** 07.2 UI-H07 (Ánh xạ khung tên, nút Cập nhật tất cả khung tên). Prototype: `screens11.js` › `updateAll`, bảng ánh xạ 15 tag (DONVI, CONGTRINH, HANGMUC, GIAIDOAN, TENBV, KYHIEU, TILE, NGAY, SOTO, CHUNHIEM, CHUTRI, THIETKE, VE, KIEMTRA, DUYET). Engine khung tên VN-010.
- **Đầu ra:** mục "Ánh xạ khung tên" (mỗi tag chọn nguồn: nhập tay, trường dự án, trường của tờ), dòng đồng bộ "N khung tên chưa khớp", chấm cảnh báo trên từng tờ, lệnh cập nhật. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Cập nhật 60 khung ở 5 bản vẽ đang mở và 10 tệp chưa mở: mỗi bản vẽ đang mở 1 mark undo; tệp chưa mở được ghi ở nền qua VN-018 (ghi an toàn), có tiến trình và hủy; toast nêu số khung đã cập nhật, số tệp chưa mở, số tờ bỏ qua và lý do (thiếu tệp, khung cũ thiếu ô).
  2. Chỉ ghi ô có tag và có giá trị khác; chạy lần hai không đổi ô nào (bất biến).
  3. Dòng đồng bộ và chấm cảnh báo cập nhật ≤ 100 ms sau khi sửa thông tin dự án hoặc ánh xạ.
  4. Ánh xạ lưu cùng bộ bản vẽ; số tờ "1/18" tính đúng theo bộ môn.
  5. Đạt DoD-U, DoD-L.

### UIH-022 · In cả bộ, đóng gói gửi đi, xuất danh mục bản vẽ
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIH-020, UIH-009, UIH-015, VN-018, VN-013, CMD:TABLE, UI-B11
- **Màn hình:** UI-H07
- **Đầu vào:** 07.2 UI-H07 (Hành động). Prototype: `screens11.js` › `printSet`, nút "In cả bộ ▾", "Đóng gói gửi đi", "Xuất danh mục ▾".
- **Đầu ra:** chân palette: In cả bộ ▾ (Xuất PDF cả bộ, In theo khung tên…, In nhiều layout…), Đóng gói gửi đi, Xuất danh mục ▾ (Bảng trong bản vẽ…, Excel). Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Xuất PDF cả bộ: một tệp nhiều trang theo thứ tự danh sách, bookmark "Số hiệu – Tên"; tờ đặt "không đưa vào khi in" bị bỏ; tệp thiếu ghi lỗi trong báo cáo; chạy nền có tiến trình ở palette và thanh trạng thái, Hủy ≤ 50 ms; tờ in xong chuyển "Đã in" kèm ngày.
  2. In theo khung tên và In nhiều layout mở UIH-015 hoặc UIH-009 đã điền sẵn đúng các tờ của bộ.
  3. Đóng gói gửi đi mở UI-B11 với mọi tệp của bộ.
  4. Xuất danh mục: bảng trong bản vẽ (STT, Số hiệu, Tên bản vẽ, Tỉ lệ, Khổ giấy, Ghi chú) theo kiểu bảng TCVN, 1 mark undo; tệp Excel cùng nội dung, so từng ô.
  5. Đạt DoD-U, DoD-L.

### UIH-023 · Sheet Set Manager: tab View của tờ, View Model, đặt view lên tờ
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** UIH-020, CMD:SHEETSET, CMD:MVIEW, RND-021
- **Màn hình:** UI-H08
- **Đầu vào:** 07.2 UI-H08; 03 §14 (Sheet Set Manager P2). Prototype `#UI-H08`: `screens17.js` (thêm tab vào palette của UI-H07 qua `SC.ssm`, không làm palette thứ hai; tab View của tờ, View Model, kéo view lên layout). Prototype chỉ ghi "Sau" ở danh mục; task thêm nhãn "Sau v1.0" vào giao diện khi chạy bản trước v1.x.
- **Đầu ra:** tab "Danh sách tờ | View của tờ | View Model" trong `SheetSetPalette`; danh sách view theo tờ (số, tên, tỉ lệ, nhãn view, callout), cây View Model (thư mục, tệp, view có tên), khung chi tiết, kéo-thả view lên layout. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Kéo view lên layout đang mở: viền đứt nét "tên · 1:tỉ lệ · rộng × cao mm" theo con trỏ; thả tạo viewport đúng tỉ lệ, đúng vùng view, khóa hiển thị, kèm nhãn view; 1 mark undo.
  2. Nhãn view và callout lỗi thời (số tờ đã đổi) được báo; "Cập nhật nhãn, callout" sửa hết, chạy lại không đổi gì.
  3. Đổi tên view cập nhật nhãn cùng tờ trong cùng 1 mark undo.
  4. 200 view: cuộn 60 fps; thumbnail view render ở nền ≤ 300 ms mỗi view.
  5. Bàn phím: ←/→ đổi tab, ↑/↓ chọn, Enter đi tới view, F2 đổi tên.
  6. Đạt DoD-U, DoD-L.

### UIH-024 · Sheet Set Manager: thuộc tính bộ, tờ mới từ mẫu, .dst, lưu trữ
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** UIH-023, CMD:SHEETSET, NT-013, NT-015
- **Màn hình:** UI-H08
- **Đầu vào:** 07.2 UI-H08 (thuộc tính bộ, tờ mới từ mẫu, callout, publish, lưu trữ, `.dst`). Prototype: `screens17.js` (hộp "Thuộc tính bộ bản vẽ" 780×560, "Thuộc tính tờ" 600, "Tờ mới từ mẫu" 820, "Mở bộ bản vẽ .dst" 1000×640, "Lưu bộ bản vẽ thành .dst" 640, "Lưu trữ bộ bản vẽ" 980×620). ZIP của NT-015.
- **Đầu ra:** `app/dialogs/sheetset/`: các hộp thoại trên. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Mở `.dst` do AutoCAD tạo (≥ 5 bộ mẫu của doanh nghiệp pilot) cho đúng số tờ, tên, số hiệu, thuộc tính tùy biến; lưu `.dst` rồi mở lại trùng dữ liệu; `.dst` hỏng báo lỗi, không crash.
  2. Tờ mới từ mẫu: tạo tệp hoặc layout từ `.dwt`, số hiệu tự động, khung tên điền sẵn; kiểm trùng tên tệp, tên layout, ký tự cấm.
  3. Thuộc tính tùy biến (bộ, tờ) dùng được làm nguồn ánh xạ khung tên (UIH-021); tên trống hoặc trùng bị chặn có thông báo.
  4. Lưu trữ ra ZIP hoặc thư mục gồm mọi tệp tờ, xref, font, CTB của bộ; chạy nền có tiến trình, Hủy ≤ 50 ms; giải nén lại mở được cả bộ.
  5. Đạt DoD-U, DoD-L.

## 5. Tham chiếu ngoài, ảnh, PDF underlay

### UIG-001 · Palette Tham chiếu ngoài: danh sách, cây lồng nhau, trạng thái, chi tiết
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DOC-013, NT-011, DWG-026, UI0-006, UI0-010, UI0-011, UI0-014, UI-B06
- **Màn hình:** UI-G10
- **Đầu vào:** 07.2 UI-G10; 07.1 §7.3 (panel Tham chiếu), §12; 03 §12. Prototype `#UI-G10`: `palettes.js` › `P.xref` (bản Viewer), `screens7.js` › `P.xref.render` (bản Pro); `cmd.js` › `IMPL.EXTERNALREFERENCES`. Prototype chỉ có 5 trạng thái và chưa có dạng cây; task làm theo đặc tả 07.2. Mô hình xref DOC-013 (block record ngoài, trạng thái phân giải, lồng nhau, attach/overlay, nạp tài liệu xref chỉ đọc); đường dẫn tìm kiếm NT-011.
- **Đầu ra:** `app/palettes/xref/XrefPalette.{h,cpp}`, `XrefModel` (bản vẽ chính, xref DWG, ảnh, PDF; dạng danh sách và dạng cây), `XrefDetailsPane` (chi tiết, xem trước). Kiểm thử và kịch bản QA-011 cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Hai kiểu xem: Danh sách và Cây lồng nhau (xref dùng ở nhiều nhánh hiện ở mọi nhánh). Dòng đầu là bản vẽ chính, trạng thái "Đã mở".
  2. Cột Tên (biểu tượng theo loại DWG, ảnh, PDF), Trạng thái, Kích thước, Kiểu (Attach/Overlay), Ngày, Đường dẫn đã lưu, Tìm thấy tại; sắp xếp theo mọi cột, ẩn/hiện cột.
  3. Đủ 6 trạng thái của 07.2: Đã nạp, Chưa nạp, Không tìm thấy, Chưa phân giải, Mồ côi, Cần nạp lại. Mỗi trạng thái có biểu tượng và chữ, không chỉ màu. Trạng thái khớp DOC-013 ở 30 ca dựng sẵn (thiếu tệp, xref vòng, xref cha bị gỡ, tệp đổi trên đĩa…).
  4. Khung chi tiết: tên, kiểu, đường dẫn đã lưu, tìm thấy tại, kích thước, ngày, thumbnail đọc ở nền. Tham chiếu không tìm thấy có nút "Tìm tệp…" mở UI-B06.
  5. Làm mới và Nạp lại (cả ở Viewer) nạp lại tài liệu xref ở nền qua DOC-013 và cập nhật trạng thái. Nạp lại 20 xref 20 MB: luồng UI không bị chặn quá 16 ms.
  6. 2.000 tham chiếu: palette có nội dung ≤ 150 ms, cuộn 60 fps, cập nhật trạng thái theo lô ≤ 16 ms.
  7. Viewer chỉ xem và nạp lại; Gắn, sửa đường dẫn hiện huy hiệu Pro, bấm mở UI-L04.
  8. Đạt DoD-U, DoD-L.

### UIG-002 · Thông báo tham chiếu đã đổi trên đĩa
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** UIG-001, NT-011, INT-005, UIA-009, UI0-009
- **Màn hình:** UI-G10
- **Đầu vào:** 07.2 UI-G10 (Thông báo); 07.1 §10, §12. Prototype chưa có phần này (XREFNOTIFY chỉ có trong danh sách biến). Theo dõi thay đổi tệp NT-011; sysvar XREFNOTIFY qua INT-005.
- **Đầu ra:** `app/palettes/xref/XrefChangeNotifier.{h,cpp}`: theo dõi tệp xref, ảnh, PDF, gom sự kiện, biểu tượng và thông báo ở thanh trạng thái. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Ghi đè KT-01.dwg từ tiến trình khác: trong ≤ 2 s thanh trạng thái hiện "Xref KT-01 đã thay đổi — [Nạp lại]", dòng tương ứng trong palette chuyển "Cần nạp lại" **[đề xuất]**.
  2. Nạp lại chỉ nạp các tham chiếu đã đổi; thông báo biến mất khi không còn tham chiếu nào cần nạp lại.
  3. 50 tệp đổi cùng lúc (chép đè cả thư mục) gom thành 1 thông báo nêu số lượng; tối đa 1 thông báo mỗi giây.
  4. Tôn trọng XREFNOTIFY: 0 không báo, 1 chỉ biểu tượng, 2 biểu tượng và thông báo.
  5. Theo dõi bằng API của hệ điều hành, không polling; khi rảnh CPU ~0%.
  6. Đạt DoD-U, DoD-L, DoD-H.

### UIG-003 · Thao tác sửa trong palette Tham chiếu ngoài: gỡ nạp, gỡ bỏ, bind, đường dẫn
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIG-001, CMD:XREF, CMD:XOPEN, UI0-008
- **Màn hình:** UI-G10
- **Đầu vào:** 07.2 UI-G10 (thanh công cụ, chuột phải); 03 §12. Prototype: `screens7.js` › `P.xref.render` (menu Gắn, Đổi đường dẫn, chuột phải, ô "Đường dẫn lưu" sửa được). Các mục đường dẫn và bind trong prototype chỉ hiện toast; task làm thật. Tùy chọn của lệnh `-XREF` (Attach, Detach, Reload, Unload, Bind, Path) và XOPEN.
- **Đầu ra:** hành động sửa trong `XrefPalette`: Gắn ▾ (DWG, Ảnh, PDF; DGN vô hiệu "Sau v1.0"), Gỡ nạp, Nạp lại, Gỡ bỏ, Bind ▸ (Bind, Insert, Từng đối tượng phụ thuộc), Đường dẫn ▸ (tuyệt đối, tương đối, bỏ đường dẫn, chọn đường dẫn mới, tìm và thay đường dẫn), Mở; hộp Tìm và thay đường dẫn. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Mỗi thao tác gửi đúng tùy chọn của `-XREF` hoặc XOPEN, 1 mark undo mỗi lần; chuỗi lệnh chạy lại bằng `.scr` cho DXF trùng 100%.
  2. Chọn nhiều dòng thì thao tác áp cho mọi dòng. Gỡ bỏ hỏi xác nhận nêu số tham chiếu và số lần chèn sẽ xóa ("Gỡ 3 tham chiếu, xóa 12 lần chèn?"), hoàn tác được.
  3. Đường dẫn tuyệt đối, tương đối, bỏ đường dẫn tính đúng theo vị trí bản vẽ chủ ở 20 ca (cùng ổ, khác ổ, UNC, thư mục cha). Khác ổ thì không đặt được tương đối và báo lý do.
  4. Tìm và thay đường dẫn: xem trước "trước → sau" từng dòng, chỉ áp cho dòng được tích; toast nêu số tham chiếu đã đổi.
  5. Gỡ nạp ẩn hình của xref trên vùng vẽ, Nạp lại hiện lại; trạng thái cập nhật ≤ 100 ms sau khi lệnh xong.
  6. Viewer và Viewer Pro: các thao tác này khóa, huy hiệu Pro; Nạp lại vẫn dùng được.
  7. Đạt DoD-U, DoD-L.

### UIG-004 · Bind từng đối tượng phụ thuộc (XBIND)
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** UIG-003, CMD:XBIND, UI0-006, UI0-008
- **Màn hình:** UI-G10
- **Đầu vào:** 07.2 UI-G10 (Bind ▸ Từng đối tượng phụ thuộc); 03 §12. Prototype: hộp XBIND trong `screens7.js` (640 px, cây theo loại, Thêm → / ← Bỏ, dòng "Tên sau khi bind").
- **Đầu ra:** `app/dialogs/xref/XbindDialog.{h,cpp}`. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Cây liệt kê đúng định nghĩa phụ thuộc của từng xref (tên dạng `XREF|TÊN`) theo 5 nhóm: Block, Kiểu kích thước, Layer, Linetype, Kiểu chữ; ảo hóa, 10.000 định nghĩa cuộn 60 fps.
  2. Thêm/Bỏ bằng nút, nhấp đúp và bàn phím. Định nghĩa kéo theo (linetype của layer, block lồng) được thêm tự động kèm ghi chú.
  3. Bind đặt tên `XREF$0$TÊN` (số tăng khi trùng), 1 mark undo; kết quả trùng `-XBIND` qua `.scr`.
  4. Đạt DoD-U, DoD-L.

### UIG-005 · Gắn DWG làm xref (XATTACH)
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD:XATTACH, DWG-026, RND-021, INT-012, UI0-007, UI0-008, UI-B01
- **Màn hình:** UI-G11
- **Đầu vào:** 07.2 UI-G11; 03 §12. Prototype `#UI-G11`: hộp XATTACH trong `screens7.js` (680 px). Prototype không áp góc, ô X/Y và kiểu đường dẫn; task áp đủ. Ảnh xem trước trong tệp DWG-026; jig INT-012.
- **Đầu ra:** `app/dialogs/xref/XattachDialog.{h,cpp}`. Kiểm thử và kịch bản QA-011 cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Mọi trường có hiệu lực: Tên (gần đây, Duyệt… mở UI-B01), Kiểu (Attach/Overlay), Đường dẫn (đầy đủ, tương đối, không), Điểm chèn, Tỉ lệ (X/Y/Z, đều), Góc (trên màn hình hoặc nhập). Kết quả trùng `-XATTACH` cùng tham số qua `.scr` ở 12 tổ hợp.
  2. Xem trước có trong ≤ 300 ms với tệp 20 MB nhờ ảnh lưu trong tệp; tệp không có ảnh thì render ở nền, hiện "Đang tạo xem trước" rồi ảnh **[đề xuất]**.
  3. Tệp không tìm thấy, đọc lỗi, hoặc gắn tạo vòng (B đã gắn A): báo dưới ô Tên, nút Gắn vô hiệu.
  4. Đơn vị block và hệ số tỉ lệ theo INSUNITS của hai bản vẽ (chỉ đọc), đúng ở 6 cặp đơn vị.
  5. Bóng xref bám con trỏ ≤ 8 ms/khung với xref 20 MB.
  6. Đạt DoD-U, DoD-L.

### UIG-006 · Gắn ảnh (IMAGEATTACH)
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD:IMAGEATTACH, RGN-021, INT-012, UI0-007, UI0-008, UI-B01
- **Màn hình:** UI-G12
- **Đầu vào:** 07.2 UI-G12; 03 §12 (PNG, JPG, TIFF, BMP, GIF). Prototype `#UI-G12`: hộp IMAGEATTACH trong `screens7.js` (620 px). Nạp ảnh bất đồng bộ RGN-021.
- **Đầu ra:** `app/dialogs/xref/ImageAttachDialog.{h,cpp}`. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Ảnh 100 megapixel: ảnh xem trước thu nhỏ có trong ≤ 500 ms, giải mã ở nền, hộp thoại không khựng **[đề xuất]**. Ảnh hỏng báo lỗi rõ, nút Gắn vô hiệu.
  2. Chi tiết: độ phân giải (px/inch), đơn vị, kích thước pixel, kích thước theo bản vẽ = pixel / DPI × hệ số đơn vị, đúng ở 10 ca (có và không có DPI trong tệp).
  3. Mọi tùy chọn (đường dẫn, điểm chèn, tỉ lệ, góc) có hiệu lực, kết quả trùng `-IMAGEATTACH` qua `.scr`. Gắn xong thì ảnh được chọn và tab ngữ cảnh Ảnh hiện.
  4. Đạt DoD-U, DoD-L.

### UIG-007 · Tab ngữ cảnh Tham chiếu ngoài: mở xref, clip, tùy chọn
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** UIG-001, CMD:XCLIP, CMD:XOPEN, INT-005, UIA-005
- **Màn hình:** UI-G15
- **Đầu vào:** 07.2 UI-G15 (Xref); 07.1 §7.11. Prototype `#UI-G15`: tab xref trong `screens7.js` (chuỗi chọn `ED.ctxRefresh`), CLIP `screens10.js`. Prototype đặt độ mờ xref cố định 40%; task dùng sysvar XDWGFADECTL. Nút Sửa tại chỗ do UIG-033 thêm ở 1.0.
- **Đầu ra:** `app/contextual/xref/xref.ribbon.json` và binding: Mở xref, Clip (chữ nhật, đa giác, theo polyline, đảo clip, bỏ clip), Tùy chọn (độ mờ xref, khung clip theo XCLIPFRAME), mở palette Tham chiếu ngoài. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Tab hiện khi chọn đúng 1 tham chiếu xref, ẩn khi bỏ chọn hoặc khi chạy lệnh; không có ở Viewer. Hiện/ẩn tab ≤ 100 ms.
  2. Clip chữ nhật, đa giác, theo polyline, đảo clip, bỏ clip: kết quả trùng XCLIP gõ tay (so DXF) ở 8 ca, 1 mark undo mỗi lần.
  3. Thanh trượt độ mờ (XDWGFADECTL 0–90) đổi qua SETVAR, vùng vẽ cập nhật ≤ 16 ms; khung clip hiện, ẩn, hiện không in theo XCLIPFRAME.
  4. Mở xref mở tệp ở tab mới; tệp bị khóa thì mở chỉ đọc kèm banner (UI-B05).
  5. Đạt DoD-U, DoD-L.


### UIG-008 · Tab ngữ cảnh Ảnh và hộp Điều chỉnh ảnh
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIG-006, CMD:IMAGEADJUST, CMD:IMAGECLIP, RGN-021, UIA-005, UI0-007
- **Màn hình:** UI-G15
- **Đầu vào:** 07.2 UI-G15 (Ảnh); 03 §12 (IMAGECLIP, IMAGEADJUST). Prototype: `screens7.js` › `SC.imageTab`, ADJUST (hộp "Điều chỉnh ảnh" 460 px không modal), CLIP.
- **Đầu ra:** `app/contextual/image/image.ribbon.json` và binding (Điều chỉnh: Độ sáng, Tương phản, Mờ 0–100; Clip: Tạo, Bỏ; Tùy chọn: Hiện ảnh, Nền trong suốt, Khung ảnh; mở palette Tham chiếu ngoài); `app/dialogs/xref/ImageAdjustDialog.{h,cpp}`. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Kéo thanh trượt: ảnh trên vùng vẽ cập nhật ≤ 16 ms/khung với ảnh 50 megapixel.
  2. Hủy trả về giá trị cũ, không để lại bước hoàn tác; Áp dụng tạo 1 mark undo; giá trị trùng IMAGEADJUST gõ tay. Đặt lại về 50 / 50 / 0.
  3. Nền trong suốt chỉ bật được với ảnh có kênh alpha hoặc ảnh 1 bit; khi vô hiệu có ghi chú lý do.
  4. Clip chữ nhật, đa giác, bỏ clip: trùng IMAGECLIP ở 6 ca.
  5. Đạt DoD-U, DoD-L.

### UIG-009 · Nhập PDF thành đối tượng (PDFIMPORT): hộp thoại, tiến trình, báo cáo
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** PLT-012, NT-013, UI0-007, UI0-008, UI0-009, UI-B01
- **Màn hình:** UI-G14
- **Đầu vào:** 07.2 UI-G14; 03 §12 (PDFIMPORT, PDFSHXTEXT). Prototype `#UI-G14`: `screens10.js` (hộp 880 px ba cột, hộp tiến trình "Đang nhập PDF", hộp "Kết quả nhập PDF"). Prototype chỉ kiểm danh sách font SHX khác rỗng; task truyền danh sách font cho engine. Engine và lệnh PLT-012.
- **Đầu ra:** `app/dialogs/pdf/PdfImportDialog.{h,cpp}`, `PdfImportReport.{h,cpp}`. Kiểm thử và kịch bản QA-011 cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Nguồn là PDF underlay đang chọn (dùng vị trí, tỉ lệ, góc của underlay; tùy chọn gỡ underlay sau khi nhập) hoặc tệp PDF và trang. Mọi tùy chọn (dữ liệu nhập, layer, nhập thành block, nối đoạn, tô đặc thành hatch, độ dày nét, suy ra linetype, nhận dạng SHX với danh sách font, ngưỡng tin cậy 50–100 mặc định 75, gộp chữ thành đoạn) truyền đúng sang PDFIMPORT; kết quả trùng `-PDFIMPORT` cùng tham số qua `.scr` ở 10 PDF mẫu.
  2. Nhập chạy ở nền: tiến trình theo bước kèm thời gian còn lại; Hủy dừng ≤ 50 ms và bản vẽ không đổi (hash); xong là đúng 1 mark undo.
  3. Lỗi chặn nút Nhập: không chọn loại dữ liệu nào, bật nhận dạng SHX mà không chọn font. Trang ảnh quét mà tắt ảnh raster: cảnh báo.
  4. Báo cáo: số đối tượng theo loại khớp số đối tượng thật đã tạo (đếm qua DOC-017), số chữ SHX nhận dạng được trên tổng, cảnh báo số chữ dưới ngưỡng, layer mới; nút "Chọn N đối tượng" chọn và zoom đúng tập đó.
  5. Thiết lập được nhớ theo người dùng.
  6. Đạt DoD-U, DoD-L.

### UIG-010 · Gắn PDF (PDFATTACH): danh sách trang có thumbnail
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIG-009, CMD:PDFATTACH, PLT-011, INT-012, UI0-007, UI0-008, UI-B01
- **Màn hình:** UI-G13
- **Đầu vào:** 07.2 UI-G13; 03 §12. Prototype `#UI-G13`: `screens10.js` › hộp PDFATTACH (720 px), `pdfThumb`. Lớp bọc PDFium PLT-011 (mở PDF, render ô).
- **Đầu ra:** `app/dialogs/pdf/PdfAttachDialog.{h,cpp}`, `PdfPageGrid` (lưới trang ảo hóa, thumbnail render nền). Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. PDF 200 trang: thumbnail trang đầu có trong ≤ 300 ms, các trang khác render ở nền theo thứ tự cuộn và dừng khi đóng hộp; bộ nhớ thumbnail ≤ 64 MB **[đề xuất]**.
  2. Chọn nhiều trang bằng chuột (Ctrl, Shift) và bàn phím (mũi tên, Space), Chọn tất cả, bộ đếm "N trang · đã chọn M". Chưa chọn trang: "Chọn ít nhất một trang.", nút Gắn vô hiệu.
  3. Tệp có mật khẩu: ô mật khẩu và nút Mở khóa; sai mật khẩu báo ngay, không đóng hộp; mật khẩu không ghi ra đĩa.
  4. Gắn k trang tạo k PDF underlay, trang sau đặt bên phải trang trước; 1 mark undo; trùng `-PDFATTACH` từng trang qua `.scr`.
  5. Liên kết "Muốn chuyển thành đối tượng sửa được? → Nhập PDF" mở UIG-009 với tệp và trang đang chọn. Tệp hỏng báo lỗi có chi tiết, không crash.
  6. Đạt DoD-U, DoD-L.

### UIG-011 · Tab ngữ cảnh PDF underlay và hộp Layer của underlay
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIG-010, CMD:PDFADJUST, CMD:PDFCLIP, CMD:PDFLAYERS, RGN-022, INT-005, UIA-005, UI0-006
- **Màn hình:** UI-G15
- **Đầu vào:** 07.2 UI-G15 (PDF underlay); 03 §12. Prototype: tab PDF trong `screens10.js`, hộp "Layer của underlay" (460 px, xem trước trực tiếp), menu chuột phải trên underlay. Hiển thị underlay RGN-022.
- **Đầu ra:** `app/contextual/pdf/pdfunderlay.ribbon.json` và binding (Điều chỉnh: Tương phản mặc định 20, Mờ mặc định 25 trong 0–80, Đơn sắc; Clip; Tùy chọn: Hiện, Bắt điểm vào PDF theo UOSNAP, Chỉnh màu theo nền; Layer PDF; Nhập thành đối tượng; mở palette); `app/dialogs/pdf/UnderlayLayersDialog.{h,cpp}`. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Tương phản, mờ, đơn sắc cập nhật underlay ≤ 16 ms/khung trên PDF bản vẽ A0 vector.
  2. Hộp Layer của underlay: 500 layer PDF cuộn 60 fps; tìm, Bật tất cả, Tắt tất cả; bật/tắt xem trước ngay; Hủy khôi phục; Áp dụng 1 mark undo, trùng PDFLAYERS dòng lệnh.
  3. UOSNAP tắt thì bắt điểm bỏ qua hình học của underlay (kiểm 10 điểm).
  4. Clip chữ nhật, đa giác, bỏ clip: trùng PDFCLIP ở 6 ca. Nhập thành đối tượng mở UIG-009 với nguồn là underlay đang chọn.
  5. Đạt DoD-U, DoD-L.

### UIG-012 · So sánh xref với bản trước (XCOMPARE)
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** S · **Phụ thuộc:** UIG-003, UII-008, CMD:XCOMPARE
- **Màn hình:** UI-G10
- **Đầu vào:** 07.2 UI-G10 (So sánh với bản trước, P2); 07.1 §7.3; 03 §12 (XCOMPARE P2). Prototype: mục chuột phải vô hiệu "So sánh với bản trước (sau v1.0)".
- **Đầu ra:** mục "So sánh với bản trước" trong palette và trong thông báo của UIG-002; chế độ so sánh dùng lại lớp phủ, tab và danh sách của UII-007, UII-008, giới hạn trong phạm vi xref.
- **Tiêu chí nghiệm thu:**
  1. Xref ở trạng thái Cần nạp lại: lệnh vào chế độ so sánh bản đang hiện với tệp mới trên đĩa, chỉ trong phạm vi xref; đối tượng của bản vẽ chủ hiện mờ.
  2. Danh sách, điều hướng, đánh dấu chạy đúng bằng chính bộ kịch bản kiểm thử của UII-007, UII-008.
  3. Nút "Nạp lại và đóng" nạp bản mới rồi thoát so sánh; "Đóng" giữ bản cũ.
  4. Đạt DoD-U, DoD-L.

## 6. Văn bản: tìm, soạn MTEXT, sửa TEXT, chính tả

### UIF-001 · Tìm chữ (FIND): hộp thoại, tùy chọn tìm, danh sách kết quả
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD:FIND, RND-013, INT-016, UI0-006, UI0-008, UI0-011
- **Màn hình:** UI-F09
- **Đầu vào:** 07.2 UI-F09; 07.1 §7.4 (panel Văn bản); 03 §7. Prototype `#UI-F09`: bản Viewer `screens1.js` › `SC.find`, bản Pro `screens8.js` (hộp "Tìm và thay thế"). Prototype chưa dùng Phạm vi, Tìm trong xref, chữ mã TCVN3/VNI; task nối đủ. API tìm của lệnh FIND (gồm so khớp bỏ dấu tiếng Việt và tìm chữ mã cũ).
- **Đầu ra:** `app/dialogs/find/FindDialog.{h,cpp}` (không modal): ô Tìm; Phạm vi (Toàn bản vẽ, Không gian hiện tại, Vùng chọn kèm nút chọn); tùy chọn (Phân biệt hoa thường, Nguyên từ, Wildcard, Phân biệt dấu tiếng Việt, Tìm trong xref, Tìm trong block, Bỏ qua đối tượng ẩn, Tìm cả chữ mã TCVN3/VNI, các loại chữ); bảng kết quả ảo hóa (vị trí, loại đối tượng, đoạn chữ có tô đoạn khớp, nhãn chữ mã cũ); nút Tìm, Tạo tập chọn. Kiểm thử và kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Kết quả trùng API tìm của FIND ở bộ 40 ca (có và không dấu, hoa thường, nguyên từ, wildcard, TCVN3/VNI, trong block, xref, bảng, chữ kích thước) trên bản vẽ mẫu VN. Tắt "Phân biệt dấu" thì "duong" khớp "đường".
  2. BENCH-STD text dày (≥ 200.000 chữ): kết quả đầu ≤ 200 ms, phần còn lại đổ vào theo lô ở nền, số kết quả cập nhật; đổi chuỗi tìm thì lượt cũ hủy ≤ 50 ms **[đề xuất]**.
  3. Bấm một dòng hoặc ↑/↓: zoom tới đối tượng ≤ 100 ms (tự chuyển layout), tô sáng đối tượng.
  4. Tạo tập chọn chọn đúng đối tượng của mọi dòng hoặc các dòng đang chọn.
  5. Chưa gõ: trạng thái rỗng giải thích tùy chọn dấu; không có kết quả: "Không tìm thấy “…”".
  6. Viewer, Viewer Pro: ô Thay bằng, nút Thay, Thay tất cả có huy hiệu Pro.
  7. Đạt DoD-U, DoD-L.

### UIF-002 · Thay thế trong FIND: Thay, Thay tất cả
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** UIF-001, CMD:FIND, CMD-304
- **Màn hình:** UI-F09
- **Đầu vào:** 07.2 UI-F09 (Thay bằng, Thay, Thay tất cả). Prototype: `screens8.js` (toast "Đã thay n chỗ (một bước hoàn tác)" kèm Hoàn tác; thay trong kích thước ghi vào chữ ghi đè).
- **Đầu ra:** phần thay thế của `FindDialog`. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Thay tất cả 10.000 chỗ: 1 mark undo, ≤ 1 s tầm trung **[đề xuất]**; toast nêu số chỗ đã thay kèm Hoàn tác; Ctrl+Z trả bản vẽ về hash cũ.
  2. Đối tượng trên layer khóa và trong xref không bị thay; toast nêu số chỗ bỏ qua.
  3. Thay trong chữ kích thước ghi vào chữ ghi đè và giữ `<>` khi chuỗi có giá trị đo; thay trong attribute, ô bảng, MTEXT giữ định dạng của đoạn quanh chỗ thay.
  4. Thay bằng chuỗi rỗng xóa đoạn khớp. Kết quả trùng API thay của FIND ở 40 ca.
  5. Đạt DoD-U, DoD-L.

### UIF-003 · Thành phần nhập chữ trên vùng vẽ: phím, IME, bộ gõ tiếng Việt, caret, vùng chọn
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** CMD:MTEXT, RGN-009, UIA-008, NT-012
- **Màn hình:** UI-F01, UI-F07, UI-F12
- **Đầu vào:** 07.2 UI-F01 (bộ gõ tiếng Việt hoạt động đầy đủ), UI-F12; 07.1 §13.6 (Unikey, EVKey, bộ gõ Windows; macOS Telex, VNI, EVKey, OpenKey; ibus, fcitx5). README §4.2: mô hình soạn (cây định dạng, caret, vùng chọn, trạng thái IME) thuộc task MTEXT của CMD. Prototype dùng ô nhập của trình duyệt, không có xử lý IME (`annot.js` › `ED.mtextEdit`, `ED.textEdit`), nên không có mẫu để chép. Hình học dàn trang RGN-009.
- **Đầu ra:** `app/contextual/textedit/InCanvasTextInput.{h,cpp}`: widget phủ trên vùng vẽ nhận `QKeyEvent`, `QInputMethodEvent` và chuyển cho mô hình soạn; trả lời `inputMethodQuery` (ImCursorRectangle, ImSurroundingText, ImCursorPosition, ImAnchorPosition, ImCurrentSelection, ImHints); vẽ caret, vùng chọn, chữ đang gõ dở có gạch chân; chuột (bấm, kéo chọn, nhấp đúp chọn từ, nhấp ba chọn đoạn); phím di chuyển (mũi tên, Home/End, Ctrl + mũi tên theo từ); bộ phát lại chuỗi sự kiện của bộ gõ cho kiểm thử.
- **Tiêu chí nghiệm thu:**
  1. Bộ kịch bản gõ ghi lại từ Unikey, EVKey (Telex, VNI; kiểu xóa rồi gửi lại và kiểu preedit), bộ gõ Windows, macOS Telex/VNI, ibus-bamboo, fcitx5-unikey, tổng ≥ 300 câu tiếng Việt: chuỗi kết quả trùng 100% câu đích, ghi vào mô hình ở dạng NFC.
  2. Cửa sổ ứng viên của IME nằm ngay dưới caret ở 100/150/200% DPI, cả khi vùng vẽ zoom và chữ xoay; lệch ≤ 2 px.
  3. Ký tự gõ hiện ≤ 16 ms (tầm trung), ≤ 33 ms (máy thấp) trong MTEXT 5.000 ký tự, đo từ sự kiện phím tới khung hình bằng QA-011.
  4. Caret, vùng chọn và gạch chân chữ đang gõ dở đúng vị trí ký tự theo dàn trang RGN-009 (lệch ≤ 1 px), kể cả chữ nhiều dấu như "ặ", "ỗ".
  5. Khi thành phần có focus, phím tắt của vùng vẽ và dòng lệnh không nhận phím; Esc, Ctrl+Enter, Tab, Enter được chuyển lên trình soạn đang dùng (MTEXT, TEXT, ô bảng).
  6. Trình đọc màn hình đọc được nội dung và vị trí caret (giao diện văn bản của QAccessible).
  7. Đạt DoD-U, DoD-L.

### UIF-004 · Trình soạn MTEXT tại chỗ: khung, hiển thị WYSIWYG, kết thúc và hủy
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** UIF-003, CMD:MTEXT, RGN-008, RGN-009, UIA-008, UIA-014, UI0-008
- **Màn hình:** UI-F01
- **Đầu vào:** 07.2 UI-F01 (Trình soạn tại chỗ); 07.1 §9.10 (nhấp đúp MTEXT); 03 §7. Prototype `#UI-F01`: `annot.js` › `IMPL.MTEXT`, `ED.mtextEdit`, `editMtext`, `applyF`. Prototype chỉ có cờ định dạng cấp đối tượng, không xoay khung và chỉ so chữ khi hỏi hủy; task làm theo cây định dạng của RGN-008. Parser và bộ ghi MTEXT RGN-008, dàn trang RGN-009.
- **Đầu ra:** `app/contextual/mtext/MTextEditor.{h,cpp}`: khung soạn neo vào MTEXT (bám pan/zoom, đúng góc xoay), chữ hiển thị bằng dàn trang thật, grip chỉnh rộng và cao, nền đục tùy chọn, ẩn đối tượng gốc khi soạn, lưu và hủy. Kiểm thử và kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Chữ trong trình soạn trùng chữ trên bản vẽ sau khi lưu: so ảnh chụp ở cùng zoom, SSIM ≥ 0,99 trên 30 MTEXT mẫu có định dạng hỗn hợp, xếp chồng, bullet, tab **[đề xuất]**.
  2. Mở trình soạn với MTEXT 5.000 ký tự ≤ 150 ms; pan/zoom khi đang soạn giữ khung đúng chỗ, 60 fps.
  3. Kéo grip rộng hoặc cao: dàn trang cập nhật ≤ 16 ms/khung.
  4. Ctrl+Enter hoặc bấm ra ngoài khung thì lưu; bấm ribbon không đóng trình soạn. Esc khi đã sửa (chữ hoặc định dạng) hỏi Lưu và đóng / Bỏ thay đổi / Tiếp tục sửa; chưa sửa thì đóng ngay. Mỗi phiên soạn là 1 mark undo.
  5. Sửa MTEXT có sẵn rồi xóa hết chữ: đối tượng bị xóa trong 1 mark undo.
  6. Chuỗi MTEXT ghi ra (mã `\f \H \W \Q \T \S \P \L \O \K \A \C`) trùng chuỗi lệnh MTEXT tạo khi nhận cùng nội dung qua `.scr` (so DXF).
  7. Chỉ một trình soạn mở cùng lúc; mở trình soạn khác thì lưu trình soạn đang mở.
  8. Đạt DoD-U, DoD-L.

### UIF-005 · Thước của trình soạn MTEXT: thụt lề, tab dừng
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIF-004, CMD:MTEXT, RGN-009
- **Màn hình:** UI-F01
- **Đầu vào:** 07.2 UI-F01 (thước có thụt lề và tab, kéo để chỉnh); 03 §7 (tab là phần P0 của MTEXT). Prototype: thước 16 px chỉ để hiển thị (`annot.js`, các marker `.i1`, `.i2`, `.tbs`); task làm thước kéo được.
- **Đầu ra:** `app/contextual/mtext/MTextRuler.{h,cpp}`: thước theo đơn vị bản vẽ, marker thụt lề dòng đầu, thụt lề treo, thụt lề phải, tab dừng 4 loại (trái, giữa, phải, thập phân), nút đổi loại tab. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Kéo 3 loại thụt lề; bấm thước thêm tab theo loại đang chọn, kéo để dời, kéo ra ngoài để xóa. Mã đoạn ghi ra (`\pi…,l…,r…,t…;`) đúng giá trị và dàn trang RGN-009 khớp vị trí marker (lệch ≤ 1 px).
  2. Kéo cập nhật dàn trang ≤ 16 ms/khung với đoạn 2.000 ký tự; nhãn giá trị theo UNITS hiện khi kéo.
  3. Áp cho mọi đoạn đang chọn; khi các đoạn khác giá trị, thước hiện giá trị của đoạn chứa caret.
  4. Dùng được bằng bàn phím: focus vào thước, mũi tên dời marker đang chọn, Delete xóa tab.
  5. Đạt DoD-U, DoD-L.

### UIF-006 · Tab ngữ cảnh Soạn văn bản: panel Kiểu và Định dạng
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIF-004, CMD:MTEXT, RGN-006, UIA-005, UI0-007, UI-C06, UI-D01
- **Màn hình:** UI-F01
- **Đầu vào:** 07.2 UI-F01 (panel Kiểu, Định dạng); 07.1 §7.11. Prototype: `annot.js` › `tab()` (id `mte`, panel Kiểu, Định dạng). Prototype chỉ gợi ý phím tắt, đổi font cả đối tượng, không lưu giãn ký tự và hệ số rộng; task áp theo vùng chọn và lưu vào chuỗi MTEXT. Danh sách font của RGN-006.
- **Đầu ra:** `app/contextual/mtext/mtext.ribbon.json` (panel Kiểu, Định dạng) và binding: kiểu chữ (vẽ bằng chính font, mục "Quản lý kiểu chữ…" mở UI-D01 trong suốt), chú thích theo tỉ lệ, chiều cao; đậm, nghiêng, gạch chân, gạch trên, gạch ngang; font; màu (ByLayer, ByBlock, ACI, "Chọn màu khác…" mở UI-C06); khớp định dạng; xóa định dạng (ký tự, đoạn, tất cả); góc nghiêng, giãn ký tự, hệ số rộng; chữ hoa, chữ thường; che nền. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Mỗi điều khiển áp cho vùng chọn (không có vùng chọn thì cho chữ gõ tiếp) qua API của lệnh MTEXT; trạng thái nút phản ánh vùng chọn (bật, tắt, khác nhau) ≤ 16 ms khi caret đổi chỗ.
  2. Đổi hoa/thường (CHỮ HOA, chữ thường, Viết Hoa Chữ Đầu, Viết hoa đầu câu) giữ đúng dấu tiếng Việt ở 200 từ mẫu ("đường" → "ĐƯỜNG"); chữ tổ hợp NFD được chuẩn hóa NFC.
  3. Danh sách ≥ 500 font TTF và SHX: mở ≤ 150 ms, tìm theo tên, xem trước bằng chính font (render ở nền).
  4. Phím tắt Ctrl+B, Ctrl+I, Ctrl+U, Ctrl+Shift+U, Ctrl+Shift+L hoạt động trong trình soạn.
  5. Giãn ký tự 0,75–4, hệ số rộng 0,1–10, góc nghiêng −85° đến 85° được kiểm phạm vi và ghi vào chuỗi (`\T`, `\W`, `\Q`).
  6. Đạt DoD-U, DoD-L.

### UIF-007 · Tab ngữ cảnh Soạn văn bản: Đoạn, Chèn, Công cụ, Tùy chọn, Đóng
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIF-005, UIF-006, CMD:MTEXT, NT-010
- **Màn hình:** UI-F01
- **Đầu vào:** 07.2 UI-F01 (panel Đoạn, Chèn, Công cụ, Tùy chọn, Đóng). Prototype: `annot.js` › `tab()`; hộp "Tìm và thay thế trong MText". Prototype chưa dùng ô hoa thường và dấu khi thay, mục "Khác…" chèn ký tự giữ chỗ, thiết lập trình soạn không có tác dụng; task làm đủ.
- **Đầu ra:** phần còn lại của `mtext.ribbon.json` và binding: điểm canh 9 vị trí, canh đoạn (trái, giữa, phải, đều, phân bố), bullet và đánh số (số, chữ cái, ký hiệu, tự đánh số khi gõ), giãn dòng; ký hiệu (°, ±, ⌀, ≈, ∠, Δ, Ω, ², ³, ≠, ≤, ≥, ‰, khoảng trắng không ngắt, m², m³) và bảng ký tự "Khác…"; Tìm và thay thế trong MTEXT, Tự viết hoa; Hoàn tác, Làm lại, Thước, thiết lập trình soạn (luôn WYSIWYG, hiện thanh công cụ, nền đục, màu tô chữ); Đóng. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Ký hiệu chèn đúng mã (° là `%%d`, ± là `%%p`, ⌀ là `%%c`, ký hiệu khác bằng `\U+xxxx` khi font không có glyph) và hiện đúng với font SHX và TTF mẫu.
  2. Bullet và đánh số: tạo, bỏ, đổi kiểu; gõ "1." và khoảng trắng thì tự đánh số; dàn trang trùng RGN-009.
  3. Hoàn tác, Làm lại trong trình soạn lùi từng thao tác (gõ, định dạng, dán) về lúc mở, không tạo mark undo của bản vẽ.
  4. Tìm và thay thế trong MTEXT: tùy chọn hoa thường và dấu tiếng Việt có hiệu lực; Thay tất cả báo số chỗ.
  5. Thiết lập trình soạn nhớ theo người dùng và có hiệu lực ngay.
  6. Ở 1366×768, panel thu gọn theo ưu tiên vẫn dùng được mọi chức năng.
  7. Đạt DoD-U, DoD-L.

### UIF-008 · Dán vào trình soạn MTEXT giữ định dạng cơ bản
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIF-004, RGN-008, NT-012
- **Màn hình:** UI-F01
- **Đầu vào:** 07.2 UI-F01 (dán từ Word giữ định dạng cơ bản, tùy chọn "chỉ dán chữ"). Prototype dán chữ thuần của trình duyệt; task làm bộ chuyển. Định dạng HTML và RTF của clipboard; bộ ghi cây định dạng RGN-008.
- **Đầu ra:** `app/contextual/mtext/RichPasteConverter.{h,cpp}` (HTML, RTF → cây định dạng MTEXT: đậm, nghiêng, gạch chân, font, cỡ theo tỉ lệ, màu, đoạn, bullet), lệnh "Chỉ dán chữ" (menu chuột phải, Ctrl+Shift+V), thiết lập mặc định. Kiểm thử và fuzz target.
- **Tiêu chí nghiệm thu:**
  1. 50 đoạn mẫu chép từ Word, LibreOffice, Excel, trình duyệt: định dạng cơ bản giữ đúng (so chuỗi MTEXT kỳ vọng); định dạng không hỗ trợ bị bỏ mà không mất chữ.
  2. "Chỉ dán chữ" dán chữ thuần, giữ xuống dòng.
  3. Dán 1 MB HTML ≤ 300 ms, luồng UI không bị chặn quá 16 ms. Dữ liệu clipboard là đầu vào không tin cậy: giới hạn kích thước, fuzz bộ chuyển ≥ 1 giờ không lỗi.
  4. Đạt DoD-U, DoD-L, DoD-C (mục 7).

### UIF-009 · Sửa text một dòng tại chỗ (TEXT, TEXTEDIT) và sửa chữ kích thước
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIF-003, UIF-004, CMD:TEXT, CMD:TEXTEDIT, RGN-007, UIA-014
- **Màn hình:** UI-F12
- **Đầu vào:** 07.2 UI-F12; 07.1 §9.10 (nhấp đúp chữ); 03 §7. Prototype `#UI-F12`: `annot.js` › `ED.textEdit`, `IMPL.TEXT`, `editText`, `IMPL.TEXTEDIT`, `editDim`, `hook.dbl`. Prototype chỉ có 4 kiểu canh lề và chế độ Một/Nhiều không có tác dụng; task làm đủ. TEXT một dòng RGN-007.
- **Đầu ra:** `app/contextual/textedit/SingleLineTextEditor.{h,cpp}` (khung soạn neo vào TEXT, đúng font, cỡ, góc, canh lề), luồng soạn của TEXT, TEXTEDIT, chữ kích thước, attribute definition (Tag, Prompt, Mặc định); MTEXT và chữ của multileader chuyển sang UIF-004. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. TEXT nhiều dòng: mỗi Enter tạo một TEXT mới đúng khoảng cách dòng và góc; Enter ở dòng trống kết thúc; Esc chỉ hủy dòng đang gõ; cả phiên là 1 mark undo; trùng TEXT gõ qua `.scr` (so DXF).
  2. Chữ trong ô soạn trùng chữ vẽ ở 15 kiểu canh lề, góc xoay, hệ số rộng, góc nghiêng, font SHX và TTF: SSIM ≥ 0,99 ở 30 ca **[đề xuất]**.
  3. Chữ kích thước: bắt đầu từ chữ ghi đè hiện có hoặc `<>`; để trống, `<>` hoặc giá trị đo thì bỏ ghi đè.
  4. TEXTEDIT chế độ Nhiều lặp đến khi Esc, chế độ Một thoát sau một lần sửa; chọn sai loại báo "Chọn chữ, MText, kích thước hoặc multileader".
  5. Bộ gõ tiếng Việt chạy đúng với cùng bộ kịch bản gõ của UIF-003.
  6. Đạt DoD-U, DoD-L.

### UIF-010 · Hộp thoại Đoạn văn của MTEXT
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIF-005, UIF-007, CMD:MTEXT, UI0-007
- **Màn hình:** UI-F03
- **Đầu vào:** 07.2 UI-F03. Prototype `#UI-F03`: `screens9.js` › `SC.mtPara`, `paraCss`, `mtLayout`. Prototype không dùng tab dừng khi dàn trang; task dùng.
- **Đầu ra:** `app/dialogs/mtext/ParagraphDialog.{h,cpp}` (780 px, 3 cột): tab dừng (loại L, C, R, D; vị trí; Thêm, Xóa), thụt lề dòng đầu, treo, phải; canh đoạn; khoảng cách trước, sau; giãn dòng (bội số, chính xác, tối thiểu); xem trước. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Áp dụng ghi đúng mã đoạn vào chuỗi MTEXT; dàn trang RGN-009 dùng tab dừng; khớp bộ tham chiếu MTEXT của RGN-009 ở 20 đoạn có tab.
  2. Kiểm hợp lệ tab: "Vị trí tab phải lớn hơn 0.", "Đã có tab dừng tại X."; ô số theo UI-C09 (đơn vị, biểu thức).
  3. Xem trước cập nhật ≤ 16 ms sau mỗi thay đổi.
  4. Mở được từ thước (nhấp đúp), từ Giãn dòng › Khác…, từ panel Đoạn.
  5. Đạt DoD-U, DoD-L.

### UIF-011 · Cột MTEXT: thiết lập cột và tay nắm cột
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIF-004, CMD:MTEXT, RGN-010, UI0-007, CMD-316
- **Màn hình:** UI-F04
- **Đầu vào:** 07.2 UI-F04, UI-F01 (tay nắm cột); 03 §7 (cột là P1). Prototype `#UI-F04`: `screens9.js` › `SC.mtColMenu`, `colDlg` (`SC.mtCols`). Prototype chưa có tay nắm cột và trình soạn chỉ hiện một cột; task làm cả hai. Dàn trang cột RGN-010.
- **Đầu ra:** menu Cột (Không cột; Cột động ▸ Tự cao, Cao thủ công; Cột tĩnh ▸ 2–6 cột; Thiết lập cột…), `app/dialogs/mtext/ColumnsDialog.{h,cpp}` (kiểu cột, số cột, chiều cao, rộng cột, khoảng giữa, tổng rộng, sơ đồ xem trước), tay nắm cột trong `MTextEditor`. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Ba kiểu cột cho dàn trang trùng RGN-010 ở 15 ca; dữ liệu cột ghi trong MTEXT trùng lệnh MTEXT tùy chọn Cột chạy qua `.scr` (so DXF).
  2. Trình soạn hiện đủ các cột khi đang soạn; kéo tay nắm rộng cột, khoảng giữa, chiều cao từng cột (cột động thủ công) cập nhật ≤ 16 ms.
  3. Ràng buộc: số cột 2–20, rộng cột không nhỏ hơn tối thiểu theo chiều cao chữ, chiều cao lớn hơn 0; báo dưới ô.
  4. Đạt DoD-U, DoD-L.

### UIF-012 · Xếp chồng (Stack) và AutoStack
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UIF-004, CMD:MTEXT, RGN-009, NT-010
- **Màn hình:** UI-F02
- **Đầu vào:** 07.2 UI-F02. Prototype `#UI-F02`: `screens9.js` › `SC.mtStack`, `stackDlg`, `asDlg`, `autoStack`. Prototype thay xếp chồng bằng ký tự Unicode và không lưu vị trí, cỡ; task ghi mã `\S` thật.
- **Đầu ra:** nút Xếp chồng (xếp, bỏ xếp), `app/dialogs/mtext/StackDialog.{h,cpp}` (chữ trên, dưới; kiểu phân số ngang, chéo, dung sai, thập phân; vị trí trên, giữa, dưới; cỡ 50–100%; xem trước), hộp Thiết lập AutoStack. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Ghi mã `\S…;` đúng kiểu (`/`, `#`, `^`) kèm vị trí và cỡ; trùng chuỗi trong bộ tham chiếu MTEXT của RGN-009 ở 20 ca; không dùng ký tự Unicode thay thế.
  2. AutoStack: gõ "1/2" và khoảng trắng thì xếp chồng theo thiết lập; "1 3/4" bỏ khoảng trắng khi bật; hộp hỏi ở lần đầu, "Không hiện hộp thoại này nữa" nhớ theo người dùng.
  3. Bỏ xếp chồng trả về chuỗi gốc.
  4. Đạt DoD-U, DoD-L.

### UIF-013 · Nhập văn bản .txt, .rtf vào MTEXT
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UIF-008, VN-001, VN-002, UI-B01, CMD-317
- **Màn hình:** UI-F01
- **Đầu vào:** 07.2 UI-F01 (Nhập văn bản .txt, .rtf; phát hành 1.0). Prototype: hộp "Nhập văn bản vào MText" (`screens9.js`, 640 px) có cảnh báo tệp TCVN3 và ô "Chuyển sang Unicode khi chèn". Chuyển bảng mã VN-001, nhận diện VN-002.
- **Đầu ra:** `app/dialogs/mtext/ImportTextDialog.{h,cpp}` (tệp gần đây, Duyệt…, xem trước, bảng mã nhận diện được, chèn tại caret). Kiểm thử và fuzz target bộ đọc.
- **Tiêu chí nghiệm thu:**
  1. 30 tệp mẫu (UTF-8, UTF-16, CP1258, TCVN3, VNI, RTF từ Word): chữ chèn đúng 100% sau khi chuyển Unicode NFC; RTF giữ định dạng cơ bản như UIF-008.
  2. Bảng mã cũ được nhận diện và cảnh báo; tắt "Chuyển sang Unicode" thì chèn nguyên bản theo font cũ.
  3. Tệp hơn 1 MB báo vượt giới hạn **[đề xuất]**; tệp hỏng báo lỗi, không crash; fuzz bộ đọc ≥ 1 giờ không lỗi.
  4. Đạt DoD-U, DoD-L, DoD-C (mục 7).

### UIF-014 · Kiểm tra chính tả (SPELL): hộp thoại và phạm vi
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD:SPELL, RND-013, INT-016, NT-013, UI0-008
- **Màn hình:** UI-F10
- **Đầu vào:** 07.2 UI-F10; 03 §7 (SPELL P2, có từ điển tiếng Việt). Prototype `#UI-F10`: `screens19.js` › `SC.spell`, `IMPL.SPELL`; thuật toán `spCheck` (âm tiết tiếng Việt theo luật, dấu thanh sai vị trí, sót phím Telex, thiếu dấu, gợi ý Levenshtein) thuộc engine của SPELL.
- **Đầu ra:** `app/dialogs/spell/SpellDialog.{h,cpp}` (không modal, 720 px, góc trên phải vùng vẽ): phạm vi, từ điển, Kiểm tra lại; tiến trình quét; mục đang xem (trạng thái, loại lỗi, ngữ cảnh, vị trí, Phóng tới), Sửa thành, Gợi ý, các nút Sửa, Sửa tất cả, Bỏ qua, Bỏ qua tất cả, Thêm vào từ điển, Hoàn tác; bảng lỗi. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Danh sách lỗi trùng engine SPELL trên bộ 300 câu mẫu có gán nhãn (dấu thanh sai vị trí, sót phím Telex, thiếu dấu, từ lạ).
  2. Quét 50.000 đối tượng chữ ở nền có tiến trình và Dừng (≤ 50 ms); luồng UI không bị chặn quá 16 ms.
  3. Sửa giữ kiểu hoa thường của từ gốc; Sửa tất cả là 1 mark undo; Hoàn tác trong hộp lùi đúng thao tác SPELL cuối.
  4. Đối tượng trên layer khóa có nhãn và không sửa được; Phóng tới zoom cạnh hộp, gạch sóng dưới từ.
  5. Bàn phím: Enter ở Sửa thành hoặc gợi ý thì sửa, ↑/↓ chọn gợi ý.
  6. Đạt DoD-U, DoD-L.

### UIF-015 · Chính tả: thiết lập, từ điển người dùng, gạch chân khi gõ MTEXT
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** UIF-014, UIF-004, CMD:SPELL, NT-010
- **Màn hình:** UI-F10
- **Đầu vào:** 07.2 UI-F10 (từ điển, thiết lập). Prototype: `screens19.js` (hộp "Thiết lập chính tả" 540 px, "Từ điển chính tả" 480 px, `SC.mtSpell`, panel Chính tả trong tab Soạn văn bản).
- **Đầu ra:** hộp Thiết lập (kiểm cả chữ kích thước, attribute, chữ trong bảng, block của xref; bỏ qua chữ hoa toàn bộ, có số, địa chỉ web, chữ mã TCVN3/VNI; kiểu đặt dấu cũ, mới, cả hai), hộp Từ điển (thêm, xóa, tìm, nhập/xuất `.dic`), gạch chân khi gõ trong `MTextEditor` và menu gợi ý chuột phải. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Gạch chân khi gõ cập nhật ≤ 16 ms sau mỗi phím trong MTEXT 5.000 ký tự (kiểm ở nền theo đoạn); không gạch từ đang gõ dở.
  2. Từ điển người dùng: thêm (báo "Nhập một từ.", "Mỗi lần thêm một từ, không có dấu cách."), xóa có hoàn tác, tìm; nhập/xuất `.dic` UTF-8 một từ mỗi dòng; tệp hỏng báo lỗi, không crash.
  3. Mỗi thiết lập có hiệu lực ngay ở lần kiểm sau (kiểm 10 ca).
  4. Đạt DoD-U, DoD-L.

## 7. Kích thước, leader, dung sai

### UIF-016 · Menu grip đa năng của kích thước
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD:DIMLINEAR, CMD:DIMCONTINUE, CMD:DIMBASELINE, CMD:DIMTEDIT, INT-013, UIA-013, CMD:AIDIMFLIPARROW
- **Màn hình:** UI-F05
- **Đầu vào:** 07.2 UI-F05 (Menu grip); 07.1 §9.5. README §4.2: grip của kích thước do task CMD/RGN của kích thước đăng ký qua INT-013, task này làm nội dung menu và tương tác. Prototype `#UI-F05`: `editor.js` (menu grip, `stretchGrip`), `annot.js` (grip p1, p2, d1, d2, tx của `dim`). Prototype không vẽ cờ lật mũi tên và đường dẫn chữ; engine kích thước phải vẽ.
- **Đầu ra:** `app/contextual/dim/DimGripMenu.{h,cpp}`: mục Kéo giãn, Nối tiếp dim, Dim cùng gốc, Lật mũi tên, Vị trí chữ ▸ (Đi cùng đường dim, Chỉ di chuyển chữ, Di chuyển kèm đường dẫn, Trên đường dim giữa, Đặt lại vị trí chữ), lọc mục theo loại grip. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Rê lên grip 420 ms thì menu mở với đúng mục của loại grip (5 grip của kích thước thẳng; grip của kích thước bán kính, đường kính, góc); rời xa hơn 70 px hoặc Esc thì đóng.
  2. Mỗi mục chạy đúng hành động của lệnh kích thước: kết quả trùng lệnh gõ tay (DIMCONTINUE, DIMBASELINE từ kích thước đó, lật mũi tên, 5 mục vị trí chữ) ở 12 ca, so DXF; 1 mark undo mỗi lần.
  3. Menu mở ≤ 100 ms, dùng được bằng bàn phím (mũi tên, Enter); không mở khi đang chạy lệnh hoặc khi số đối tượng chọn vượt GRIPOBJLIMIT.
  4. Đạt DoD-U, DoD-L.

### UIF-017 · Phản hồi của kích thước thông minh (DIM)
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD:DIM, UIA-013, UI-E10
- **Màn hình:** UI-F05
- **Đầu vào:** 07.2 UI-F05 (Dim thông minh); 03 §8 (DIM P1). Prototype: `annot.js` › `IMPL.DIM` (xem trước loại kích thước khi rê, tùy chọn Góc, Cùng gốc, Nối tiếp, Layer). Jig của lệnh DIM.
- **Đầu ra:** nhãn loại kích thước cạnh con trỏ (Thẳng, Nghiêng, Bán kính, Đường kính, Góc, Cung), popup chọn khi đặt trùng kích thước cũ (Thay thế, Giữ cả hai, Tách) nối với từ khóa của lệnh. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Nhãn loại kích thước đổi đúng theo đối tượng dưới con trỏ ở 6 loại (đoạn ngang, đứng, nghiêng, cung, đường tròn, hai đường) và cập nhật cùng khung với jig (≤ 8 ms/khung).
  2. Đặt trùng kích thước cũ: popup 3 lựa chọn cạnh con trỏ, cùng từ khóa với chip trên dòng lệnh (UI-E10); chọn được bằng chuột, chữ tắt, Enter cho mặc định.
  3. Đạt DoD-U, DoD-L.

### UIF-018 · Thiết lập leader nhanh (QLEADER)
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD:QLEADER, UI0-008
- **Màn hình:** UI-F13
- **Đầu vào:** 07.2 UI-F13 (người dùng VN gọi bằng `LE`); 03 §9 (QLEADER P1). Prototype `#UI-F13`: `screens9.js` (QLEADER và hộp "Thiết lập leader nhanh" 820 px, 3 tab, xem trước, tóm tắt). Danh sách mũi tên dùng chung với kiểu kích thước.
- **Đầu ra:** `app/dialogs/leader/QLeaderSettingsDialog.{h,cpp}`: tab Chú thích (MText, Sao chép một đối tượng, Dung sai, Tham chiếu block, Không; tùy chọn MText; dùng lại chú thích), Đường dẫn và mũi tên (thẳng, spline; mũi tên; số điểm; ràng buộc góc đoạn 1 và 2), Gắn chữ (5 vị trí trái, phải; gạch chân dòng cuối); xem trước. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Mọi thiết lập có hiệu lực ở lần QLEADER kế: số điểm (không giới hạn hoặc 2–50), ràng buộc góc (Tự do, Ngang, 90°, 45°, 30°, 15°), 5 loại chú thích, tùy chọn MText, dùng lại chú thích, gắn chữ, gạch chân; so DXF với QLEADER chạy bằng `.scr` cùng thiết lập ở 15 ca.
  2. Tùy chọn MText vô hiệu khi loại không phải MText; số điểm nhỏ hơn 2 bị chặn kèm thông báo.
  3. Xem trước cập nhật ≤ 16 ms sau mỗi thay đổi.
  4. Mở từ tùy chọn Thiết lập (S) hoặc Enter ở prompt đầu của QLEADER.
  5. Đạt DoD-U, DoD-L.

### UIF-019 · Khung dung sai hình học (TOLERANCE)
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD:TOLERANCE, INT-012, UIA-014, UI0-004
- **Màn hình:** UI-F14
- **Đầu vào:** 07.2 UI-F14 (14 ký hiệu theo ISO 1101, ký hiệu tự vẽ); 03 §8 (TOLERANCE P1). Prototype `#UI-F14`: `screens9.js` › `SC.tolDlg`, `IMPL.TOLERANCE`, nhấp đúp sửa (`hook.dbl`).
- **Đầu ra:** `app/dialogs/leader/ToleranceDialog.{h,cpp}` (800 px): lưới 2 hàng (ký hiệu đặc tính; dung sai 1, 2 có ⌀, giá trị, điều kiện vật liệu M, L, S; chuẩn 1–3), chiều cao và ký hiệu vùng dung sai chiếu, ký hiệu chuẩn, xem trước; chế độ Chèn và Sửa. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Chuỗi TOLERANCE ghi ra trùng chuỗi lệnh TOLERANCE tạo cho cùng nội dung qua `.scr` (so DXF) ở 20 ca; vẽ đúng bằng font gdt.
  2. Kiểm hợp lệ: hàng 1 cần ký hiệu hoặc dung sai ("Chọn ký hiệu đặc tính hoặc nhập dung sai cho hàng 1."); ký hiệu cần Dung sai 1; ô chuẩn chỉ nhận A–Z, tối đa 3 ký tự, tự viết hoa.
  3. 14 ký hiệu là biểu tượng tự vẽ (UI0-004), có tên cho trình đọc màn hình và tooltip tiếng Việt kèm tên tiếng Anh.
  4. Nhấp đúp khung có sẵn mở hộp ở chế độ Sửa với đúng giá trị; Cập nhật dung sai là 1 mark undo. Chèn có xem trước theo con trỏ ≤ 8 ms/khung.
  5. Đạt DoD-U, DoD-L.

## 8. Block, attribute, đếm, thư viện nội dung

### UIG-013 · Palette Đếm và chế độ đếm trên vùng vẽ (COUNT)
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD:COUNT, RND-013, RND-021, INT-016, UI0-006, UI0-010, UI0-011, VN-151
- **Màn hình:** UI-G09
- **Đầu vào:** 07.2 UI-G09; 07.1 §7.9 (Đếm block); 03 §11 (COUNT P1). Prototype `#UI-G09`: `screens3.js` › `P.count`, `countGroups`, `overlaps`, `countEnter`, `showItem`, `countBar`, `IMPL.COUNT`. Prototype chỉ có toast cho Xuất Excel và vẽ thumbnail đồng bộ; task làm thật.
- **Đầu ra:** `app/palettes/count/CountPalette.{h,cpp}` (dock phải: phạm vi Model, Layout, Toàn bộ; Làm mới; menu ⋯; tìm; sắp xếp; ghi chú block chồng; dòng block có thumbnail, tên, mô tả, số lượng; Xuất Excel; Tạo bảng), `CountModeBar` (thanh nổi dưới banner: ◀ i / N ▶, Thoát), `app/palettes/common/BlockThumbnailCache.{h,cpp}` (render nền qua RND-021, cache theo handle, thế hệ, cỡ, DPI, chủ đề; dùng lại ở palette Blocks, DesignCenter) **[đề xuất]**. Kiểm thử và kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Số lượng trùng kết quả lệnh COUNT cùng phạm vi trên cảnh block lồng của BENCH-STD và 10 bản vẽ QA-001.
  2. 50.000 tham chiếu block: palette có số ≤ 300 ms sau khi COUNT trả kết quả; tìm, sắp xếp (Số lượng, Tên A–Z), đổi phạm vi ≤ 16 ms **[đề xuất]**.
  3. Bấm một dòng thì vào chế độ đếm: tô sáng mọi tham chiếu (RND-013), zoom tới tham chiếu hiện tại (khung tối thiểu 6 m) kèm khung đứt nét; ◀ ▶ duyệt vòng ≤ 100 ms mỗi bước, tự chuyển layout; thanh trạng thái hiện "Đếm X: N block · đang xem i / N".
  4. Bấm lại dòng, Thoát, đổi bản vẽ hoặc bỏ chọn thì thoát chế độ đếm và bỏ hết tô sáng.
  5. Block chồng nhau (cùng tên, vị trí, góc, tỉ lệ): ghi chú nêu số lượng, nhãn "N chồng" trên dòng, nút Xem tô sáng và zoom các block đó.
  6. Xuất Excel có cột Tên, Mô tả, Số lượng (và cột theo layout khi phạm vi Toàn bộ), trùng số trong palette; toast kèm Mở tệp.
  7. Mở palette không thêm gì vào bản vẽ (hash không đổi). Viewer: palette khóa, huy hiệu Viewer Pro; Viewer Pro dùng đủ trừ Tạo bảng (khóa Pro).
  8. Đạt DoD-U, DoD-L, DoD-H.

### UIG-014 · Tạo bảng đếm từ palette Đếm
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UIG-013, UII-004
- **Màn hình:** UI-G09
- **Đầu vào:** 07.2 UI-G09 (Tạo bảng đếm; EA vì cần quyền sửa bản vẽ). Prototype: nút "Tạo bảng" (`screens3.js`, chỉ toast). Chèn bảng có xem trước của UII-004.
- **Đầu ra:** hành động Tạo bảng của `CountPalette`. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Tạo bảng chèn bảng STT, Tên block, Mô tả, Số lượng và dòng Tổng cộng theo phạm vi, bộ lọc tìm và thứ tự đang hiện; xem trước theo con trỏ ≤ 8 ms/khung; 1 mark undo.
  2. Số trong bảng trùng palette. Bảng là bảng tĩnh; field đếm tự cập nhật đi cùng hộp Field ở 1.0 (UIF-026) **[đề xuất]**.
  3. Viewer Pro: nút khóa, huy hiệu Pro.
  4. Đạt DoD-U, DoD-L.

### UIG-015 · Palette Blocks: khung, tab Bản vẽ hiện hành, thumbnail nền
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** UIG-013, DOC-012, UI0-006, UI0-010, UI0-011, UI0-014, UI-J13
- **Màn hình:** UI-G01
- **Đầu vào:** 07.2 UI-G01; 07.1 §7.2 (panel Block, gallery Chèn), §7.3, §12; 03 §11. Prototype `#UI-G01`: `screens7.js` › `P.blocks.render`, `SC.blockThumb`, `IMPL.BLOCKSPALETTE`; `ribbons.js` › `insertGal`. Prototype vẽ thumbnail đồng bộ và lặng lẽ thêm định nghĩa thư viện vào bản vẽ khi mở tab Gần đây, Yêu thích; task không làm vậy. Định nghĩa block DOC-012.
- **Đầu ra:** `app/palettes/blocks/BlocksPalette.{h,cpp}` (4 tab Bản vẽ hiện hành, Gần đây, Yêu thích, Thư viện; ô tìm; kiểu xem biểu tượng lớn, nhỏ, danh sách; thanh trượt cỡ xem trước), model tab Bản vẽ hiện hành, gallery Chèn trên ribbon. Kiểm thử và kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Tab Bản vẽ hiện hành liệt kê mọi định nghĩa có tên (ẩn `*…`, `A$…`, block của xref) kèm số lần dùng và mô tả; tìm theo tên và mô tả không phân biệt hoa thường và dấu; sắp theo tên hoặc số lần dùng.
  2. 5.000 định nghĩa: palette có nội dung ≤ 150 ms (ô giữ chỗ), thumbnail của màn hình đầu xong ≤ 500 ms, phần còn lại render ở nền ưu tiên thấp và dừng khi cuộn qua; cuộn 60 fps **[đề xuất]**.
  3. Thumbnail vừa hộp bao block (lề 5 px), theo chủ đề Tối/Sáng; định nghĩa đổi (BEDIT, REFEDIT, định nghĩa lại) thì thumbnail tạo lại ≤ 1 s; mở palette, đổi tab không thêm gì vào bản vẽ (hash không đổi).
  4. Kiểu xem và cỡ xem trước nhớ theo người dùng.
  5. Trạng thái rỗng "Bản vẽ chưa có block" kèm nút Mở Thư viện TCVN (UI-J13); tìm không thấy "Không có block khớp"; Viewer, Viewer Pro: palette khóa, "Chèn block có trong TrionCAD Pro" kèm nút dùng thử.
  6. Gallery Chèn trên ribbon (tối đa 14 block, Block gần đây…, Block yêu thích…, Mở thư viện…) dùng chung cache.
  7. Đạt DoD-U, DoD-L, DoD-H.

### UIG-016 · Chèn từ palette Blocks: tùy chọn chèn, bấm để chèn, kéo-thả
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIG-015, CMD:INSERT, INT-012, UIA-014, UI0-007
- **Màn hình:** UI-G01
- **Đầu vào:** 07.2 UI-G01 (Tùy chọn chèn, Hành vi); 07.1 §9.11 (kéo block vào vùng vẽ); 03 §11. Prototype: `screens7.js` › `IMPL.INSERT`, `askAttrs`, kéo-thả `text/x-tc-block`. Prototype lưu mà không dùng cờ điểm chèn và tỉ lệ trên màn hình, Phá khối, Điểm gốc; task áp đủ.
- **Đầu ra:** mục "Tùy chọn chèn" của `BlocksPalette` (điểm chèn trên màn hình hoặc X/Y/Z; tỉ lệ đều hoặc X/Y/Z, trên màn hình; góc trên màn hình hoặc nhập; Lặp chèn liên tục; Phá khối khi chèn), bấm để chèn, kéo-thả, hộp nhập attribute khi chèn. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Mọi tùy chọn có hiệu lực: kết quả trùng `-INSERT` cùng tham số qua `.scr` ở 16 tổ hợp (điểm, tỉ lệ đều và không đều, góc, phá khối), so DXF.
  2. Bấm block: INSERT chạy với bóng block bám con trỏ ≤ 8 ms/khung (block 10.000 đối tượng); Lặp chèn liên tục quay lại prompt điểm chèn đến khi Esc.
  3. Kéo-thả: chèn tại điểm thả (có bắt điểm), block annotative nhân tỉ lệ chú thích hiện hành; mỗi lần thả 1 mark undo.
  4. Block có attribute: theo ATTREQ và ATTDIA (ATTDIA = 1 mở hộp nhập attribute có nút Dùng mặc định).
  5. Tùy chọn chèn nhớ theo người dùng.
  6. Đạt DoD-U, DoD-L.

### UIG-017 · Tab Thư viện của palette Blocks
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIG-015, DOC-016, DWG-026, NT-010, NT-011, NT-013, UI-J13
- **Màn hình:** UI-G01
- **Đầu vào:** 07.2 UI-G01 (tab Thư viện: thư mục hoặc tệp block, liên kết Thư viện TCVN); 03 §11. Prototype: tab Thư viện trong `screens7.js` (dữ liệu `LIBDEF`, `LIB`; cây có Thư viện TCVN, thư mục máy, thư mục mạng; nút "Thêm thư mục hoặc tệp…"). Ảnh xem trước trong tệp DWG-026; nạp lười DOC-016.
- **Đầu ra:** tab Thư viện: cây thư viện (mục Thư viện TCVN mở UI-J13; thư mục và tệp của người dùng; thư mục mạng), lưới block của mục đang chọn, chèn từ thư viện. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Thư mục 2.000 tệp DWG: danh sách tệp ≤ 300 ms; thumbnail đọc từ ảnh lưu trong tệp ở nền song song; tệp không có ảnh thì render ở nền; cuộn 60 fps **[đề xuất]**.
  2. Tệp DWG chứa nhiều block: mở rộng thành danh sách block bên trong, đọc bảng block bằng nạp lười, không giải mã cả tệp.
  3. Chèn block từ thư viện: định nghĩa (kèm block lồng, layer, kiểu) được chép vào bản vẽ ở lần chèn đầu, chung 1 mark undo với lần chèn; chèn lại không chép nữa.
  4. Thư mục mạng mất kết nối: dòng ghi "Không kết nối được", palette không treo (chờ tối đa 3 s, ở nền) **[đề xuất]**.
  5. Danh sách thư mục thư viện nhớ theo người dùng.
  6. Đạt DoD-U, DoD-L, DoD-H.

### UIG-018 · Tab Gần đây, Yêu thích và menu chuột phải của palette Blocks
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UIG-017, CMD:INSERT, CMD:BEDIT, NT-010
- **Màn hình:** UI-G01
- **Đầu vào:** 07.2 UI-G01 (tab Gần đây, Yêu thích; chuột phải: Chèn, Block Editor, Định nghĩa lại từ tệp, Thêm vào Yêu thích, Mở tệp nguồn). Prototype: `screens7.js` (danh sách gần đây tối đa 12, mục chuột phải "Chọn các block này trên bản vẽ"); "Định nghĩa lại từ tệp…" chỉ là toast.
- **Đầu ra:** tab Gần đây, tab Yêu thích, menu chuột phải đầy đủ của ô block. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Gần đây: 12 block chèn gần nhất trên mọi bản vẽ, mới nhất trước, nhớ qua các phiên; mở tab không thêm định nghĩa vào bản vẽ (hash không đổi).
  2. Yêu thích: thêm, bỏ từ menu chuột phải; rỗng thì "Chưa có block yêu thích" kèm hướng dẫn.
  3. Định nghĩa lại từ tệp: xác nhận nêu số tham chiếu sẽ cập nhật; 1 mark undo; trùng `-INSERT tên=tệp` qua `.scr`.
  4. "Chọn các block này trên bản vẽ" chọn đúng mọi tham chiếu trong không gian hiện hành; Block Editor mở BEDIT với block đó.
  5. Đạt DoD-U, DoD-L.

### UIG-019 · Định nghĩa block (BLOCK) và ghi block ra tệp (WBLOCK)
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD:BLOCK, CMD:WBLOCK, INT-008, NT-013, UI0-007, UI0-008, UI-B01, UI-E06
- **Màn hình:** UI-G02, UI-G03
- **Đầu vào:** 07.2 UI-G02, UI-G03; 03 §11. Prototype `#UI-G02`, `#UI-G03`: `screens7.js` › `blockDlg` (dùng chung cho hai lệnh). Prototype chỉ hiện toast khi WBLOCK; task ghi tệp thật qua lệnh. Ô "Mở trong Block Editor sau khi tạo" do UIG-031 bật ở 1.0; nút Hyperlink… do UIF-028 bật.
- **Đầu ra:** `app/dialogs/block/BlockDefineDialog.{h,cpp}`: Tên (combo, kiểm hợp lệ), Điểm gốc (Chọn điểm hoặc X/Y/Z), Đối tượng (Chọn đối tượng, Chọn nhanh UI-E06; Giữ nguyên, Chuyển thành block, Xóa; số đối tượng đã chọn), Hành vi (chú thích theo tỉ lệ, khớp hướng layout, co giãn đều, cho phép phá khối), Đơn vị block, Mô tả; chế độ WBLOCK (Nguồn: block, cả bản vẽ, đối tượng; Đích: tệp, đơn vị chèn, phiên bản DWG). Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Kiểm tên tức thời (≤ 16 ms mỗi phím): "Nhập tên block.", ký tự cấm `< > / \ " : ; ? * | , = \``, tối đa 255 ký tự; tên đã có thì cảnh báo "Định nghĩa lại block X sẽ cập nhật N tham chiếu" và hỏi xác nhận trước khi ghi.
  2. Chọn điểm và Chọn đối tượng ẩn hộp rồi quay lại kèm giá trị; vùng chọn có sẵn (PICKFIRST) là đối tượng ban đầu; chưa chọn đối tượng thì cảnh báo.
  3. BLOCK: kết quả trùng `-BLOCK` cùng tham số qua `.scr` (so DXF) ở 9 tổ hợp (3 cách xử lý đối tượng × có hoặc không annotative × đơn vị); 1 mark undo.
  4. WBLOCK: tệp ghi ra trùng tệp do `-WBLOCK` tạo (so DXF) ở 3 nguồn × 2 phiên bản DWG; ghi lâu hơn 2 s thì có tiến trình và Hủy; bản vẽ hiện hành không đổi, không có mark undo.
  5. Đạt DoD-U, DoD-L.

### UIG-020 · Định nghĩa attribute (ATTDEF)
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD:ATTDEF, INT-012, UI0-007, UI0-008, UI-D01
- **Màn hình:** UI-G06
- **Đầu vào:** 07.2 UI-G06; 03 §11. Prototype `#UI-G06`: `screens7.js` (hộp ATTDEF 640 px). Prototype chỉ lưu chế độ Ẩn và có 4 kiểu canh lề; task lưu đủ 6 chế độ, đủ 15 kiểu canh lề của TEXT. Nút "Chèn field" do UIF-026 thêm ở 1.0.
- **Đầu ra:** `app/dialogs/block/AttdefDialog.{h,cpp}`: Chế độ (Ẩn, Hằng số, Xác nhận, Đặt sẵn, Khóa vị trí, Nhiều dòng), Attribute (Tag, Prompt, Mặc định), Điểm chèn, Thiết lập chữ (canh lề, kiểu chữ, chú thích theo tỉ lệ, chiều cao, góc, bề rộng khung), "Canh dưới attribute trước". Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Cờ chế độ ghi vào ATTDEF trùng `-ATTDEF` qua `.scr` ở 12 tổ hợp chế độ (so DXF).
  2. Tag tự viết hoa; báo "Nhập tag cho attribute.", "Tag không chứa khoảng trắng."; Hằng số thì ô Prompt vô hiệu; Nhiều dòng thì bật Bề rộng khung.
  3. "Canh dưới attribute trước" đặt ngay dưới attdef vừa tạo theo chiều cao chữ và giãn dòng; vô hiệu khi phiên chưa tạo attdef nào.
  4. Điểm chèn trên màn hình: chữ xem trước bám con trỏ ≤ 8 ms/khung.
  5. Đạt DoD-U, DoD-L.

### UIG-021 · Trình sửa attribute (EATTEDIT, ATTEDIT)
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD:EATTEDIT, UIF-004, UIA-014, UI0-007, UI0-008, UI-C06, UI-C07, UI-C08
- **Màn hình:** UI-G07
- **Đầu vào:** 07.2 UI-G07; 07.1 §9.10 (nhấp đúp block có attribute); 03 §11. Prototype `#UI-G07`: `screens7.js` (hộp EATTEDIT 600 px và bản ATTEDIT cổ điển); `annot.js` › `hook.dbl`. Prototype không áp tab Tùy chọn chữ và chỉ áp Layer ở tab Thuộc tính; task áp đủ. "Block kế tiếp" là P2.
- **Đầu ra:** `app/dialogs/block/AttributeEditorDialog.{h,cpp}`: đầu hộp (Block, Tag, Chọn block), tab Attribute (bảng Tag, Prompt, Giá trị; ô sửa giá trị), tab Tùy chọn chữ, tab Thuộc tính (layer, linetype, màu, plot style, độ dày nét), nút Áp dụng, OK, Hủy; khung đứt nét quanh block trên vùng vẽ khi hộp mở; bản ATTEDIT không tab. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Mọi tab có hiệu lực: giá trị và thuộc tính sau OK trùng thao tác dòng lệnh tương ứng qua `.scr` ở 15 ca (so DXF).
  2. Enter ở ô giá trị sang tag kế (vòng lại) và chọn sẵn chữ; bảng cập nhật khi gõ ≤ 16 ms.
  3. Attribute nhiều dòng mở trình soạn MTEXT tại chỗ (UIF-004), bộ gõ tiếng Việt chạy như trong MTEXT.
  4. Mỗi lần Áp dụng là 1 mark undo; Hủy sau Áp dụng giữ phần đã áp dụng.
  5. Block không có attribute: báo "Block X không có attribute.", không mở hộp. Viewer, Viewer Pro: lệnh khóa (nhấp đúp mở Thuộc tính chỉ đọc theo 07.1 §9.10).
  6. Nút "Block kế tiếp" vô hiệu, ghi "Sau v1.0".
  7. Đạt DoD-U, DoD-L.

### UIG-022 · Quản lý attribute block (BATTMAN)
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIG-021, CMD:BATTMAN, CMD:ATTSYNC, UI0-006, UI0-008
- **Màn hình:** UI-G08
- **Đầu vào:** 07.2 UI-G08; 03 §11 (BATTMAN P1, ATTSYNC P0). Prototype `#UI-G08`: `screens10.js` (hộp BATTMAN 760 px, "Sửa attribute: TAG" 560 px, "Thiết lập quản lý attribute" 420 px, `syncRefs`).
- **Đầu ra:** `app/dialogs/block/BattmanDialog.{h,cpp}`: chọn block (chỉ block có attribute, nút Chọn block, số tham chiếu theo Model và layout), danh sách attribute (cột cấu hình được), nút Đồng bộ, Lên, Xuống, Sửa…, Gỡ, ô "Áp thay đổi cho tham chiếu có sẵn", Thiết lập. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Đổi thứ tự, sửa, gỡ attribute rồi Áp dụng: định nghĩa và (khi bật ô) mọi tham chiếu cập nhật; giá trị đã nhập giữ theo tag kể cả khi tag đổi tên; attribute hằng số lấy mặc định; trùng ATTSYNC ở 10 ca (so DXF).
  2. Đổi block khi còn thay đổi chưa áp dụng: hỏi "Áp dụng rồi đổi" hoặc "Bỏ thay đổi". Gỡ: xác nhận nêu số tham chiếu bị ảnh hưởng.
  3. Toàn bộ một lần Áp dụng là 1 mark undo; block có 500 tham chiếu áp dụng ≤ 200 ms tầm trung **[đề xuất]**.
  4. Tag trùng có nhãn "trùng" và ghi chú cảnh báo; 1.000 attribute cuộn 60 fps.
  5. Đạt DoD-U, DoD-L.

### UIG-023 · Thay block (BLOCKREPLACE)
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIG-015, UIG-017, CMD:BLOCKREPLACE, RND-012, RND-013, UI0-008
- **Màn hình:** UI-G16
- **Đầu vào:** 07.2 UI-G16 (Thay block); 07.1 §7.10 (tab Mở rộng); 03 §11. Prototype `#UI-G16`: `screens10.js` (hộp không modal 760 px, tab "Thay block" và tab "Tìm và chuyển đổi" có nhãn sau v1.0, xem trước trên vùng vẽ).
- **Đầu ra:** `app/dialogs/block/BlockReplaceDialog.{h,cpp}`: Block cần thay (mọi block cùng tên, hoặc các block đã chọn), Tùy chọn (giữ tỉ lệ và góc, ánh xạ attribute theo tag, dọn định nghĩa cũ), Block thay thế (trong bản vẽ hoặc thư viện, tìm, danh sách có thumbnail), xem trước trên vùng vẽ. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Xem trước trên vùng vẽ (block sẽ bị thay tô sáng, bóng block mới) cập nhật ≤ 100 ms khi đổi block thay thế, với 5.000 tham chiếu.
  2. Thay: kết quả trùng lệnh BLOCKREPLACE dòng lệnh qua `.scr` ở 8 tổ hợp tùy chọn (so DXF); attribute ánh xạ theo tag, tag thiếu ở block mới lấy mặc định; 1 mark undo.
  3. Thông báo chặn: chưa có block cần thay, chưa chọn block thay thế, block thay thế trùng block cần thay. "Dọn định nghĩa cũ" chỉ bật khi không còn tham chiếu khác ("Còn N tham chiếu X khác: chưa dọn được.").
  4. Đạt DoD-U, DoD-L.

### UIG-024 · Tìm và chuyển đổi block (TCBLOCKMATCH)
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** UIG-023, CMD:TCBLOCKMATCH, GEO-025, RND-013, NT-013
- **Màn hình:** UI-G16
- **Đầu vào:** 07.2 UI-G16 (Tìm và chuyển đổi, P2, kiểu Smart Match); 03 §11 (Smart Blocks P2). Prototype: tab "Tìm và chuyển đổi" chỉ có trạng thái rỗng "có ở sau v1.0". Chữ ký hình học bất biến GEO-025.
- **Đầu ra:** tab Tìm và chuyển đổi của `BlockReplaceDialog`: chọn hình mẫu, dung sai (UI-C09), bất biến khi xoay, khi đổi tỉ lệ; chạy tìm ở nền; danh sách cụm tìm được (số lượng, zoom tới); hành động Chuyển thành block (tên, điểm gốc), Đếm, Chọn. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Tìm trên BENCH-STD 1 triệu entity ở nền: tiến trình, Hủy ≤ 50 ms, kết quả hiện dần theo lô.
  2. Bản vẽ mẫu có 40 cụm cửa vẽ rời (xoay 0/90/180/270°, tỉ lệ 0,8–1,2): danh sách hiện đúng tập engine trả về (40 cụm, không cụm sai).
  3. Chuyển thành block: mọi cụm thay bằng tham chiếu đúng góc, tỉ lệ; 1 mark undo.
  4. Đạt DoD-U, DoD-L.

### UIG-025 · DesignCenter: cây nguồn, nội dung bản vẽ, xem trước
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** UIG-013, CMD:ADCENTER, DOC-009, DOC-016, DWG-026, NT-011, UI0-006, UI0-010, UI0-011
- **Màn hình:** UI-G17
- **Đầu vào:** 07.2 UI-G17; 07.1 §11 (Ctrl+2); 03 §11 (ADCENTER P1). Prototype `#UI-G17`: `screens10.js` › `P.adc`, `dcGo`. Nạp lười DOC-016 cho tệp chưa mở; snapshot DOC-009 cho tệp đang mở.
- **Đầu ra:** `app/palettes/designcenter/DesignCenterPalette.{h,cpp}`: thanh công cụ (Tải tệp…, Lùi, Lên trên, Tìm kiếm, Yêu thích, Trang chủ, bật/tắt Cây thư mục, Xem trước, Mô tả, Kiểu xem), tab Thư mục, Bản vẽ đang mở, Lịch sử; cây tệp → 9 nhóm (Block, Kiểu kích thước, Layer, Layout, Linetype, Kiểu multileader, Kiểu bảng, Kiểu chữ, Xref) kèm số lượng; vùng nội dung; khung xem trước và mô tả. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Tệp DWG chưa mở 100 MB: cây 9 nhóm kèm số lượng có trong ≤ 500 ms nhờ chỉ đọc bảng ký hiệu và dictionary **[đề xuất]**.
  2. Thư mục 2.000 tệp: liệt kê ≤ 300 ms; thumbnail tệp đọc ở nền từ ảnh lưu trong tệp; thumbnail block dùng `BlockThumbnailCache`; cuộn 60 fps.
  3. Lùi, Lên trên, Trang chủ, Lịch sử hoạt động như trình duyệt tệp; ổ mạng mất kết nối báo rõ, không treo (chờ tối đa 3 s, ở nền).
  4. Tệp đang mở trong phiên đọc từ snapshot, không đọc lại đĩa.
  5. Viewer, Viewer Pro: palette khóa, huy hiệu Pro.
  6. Đạt DoD-U, DoD-L, DoD-H.

### UIG-026 · DesignCenter: thêm vào bản vẽ, kéo-thả, tìm kiếm, yêu thích
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIG-025, UIG-005, CMD:ADCENTER, CMD:INSERT, DOC-014, NT-013
- **Màn hình:** UI-G17
- **Đầu vào:** 07.2 UI-G17 (kéo-thả sang bản vẽ hiện hành, tìm kiếm theo tên, loại, ngày, yêu thích). Prototype: `screens10.js` › `dcAdd`, kéo-thả `text/x-tc-adc`. Prototype chỉ hiện toast với linetype, kiểu multileader, kiểu bảng; task thêm thật. Chép sâu giữa hai bản vẽ DOC-014.
- **Đầu ra:** thêm nội dung (nhấp đúp, Enter, kéo-thả, menu chuột phải: Chèn block…, Gắn xref…, Thêm vào bản vẽ, Thêm/Bỏ Yêu thích, Mở tệp), hộp Tìm kiếm (tìm gì, trong đâu, tên chứa, sửa trong 7 hoặc 30 ngày), danh sách Yêu thích. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Thêm đối tượng có tên của 8 nhóm (block, kiểu kích thước, layer, layout, linetype, kiểu multileader, kiểu bảng, kiểu chữ) từ bản vẽ khác: kết quả trùng thao tác tương ứng bằng lệnh qua `.scr` (so DXF); block kèm block lồng và layer, linetype phụ thuộc; mỗi lần thêm 1 mark undo. Xref mở UIG-005.
  2. Tên đã có trong bản vẽ: giữ bản hiện có, không ghi đè, dòng lệnh báo rõ; không sinh tên lạ.
  3. Kéo block vào vùng vẽ: chèn tại điểm thả, bóng block ≤ 8 ms/khung.
  4. Tìm trong thư mục 2.000 tệp theo tên, loại, ngày sửa: chạy nền, kết quả hiện dần, Hủy ≤ 50 ms; không có kết quả thì gợi ý từ khóa ngắn hơn.
  5. Yêu thích nhớ qua các phiên; rỗng thì có hướng dẫn.
  6. Đạt DoD-U, DoD-L.

### UIG-027 · Tool Palettes: cửa sổ, nhóm palette, dùng công cụ
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** CMD:TOOLPALETTES, CMD:INSERT, CMD:HATCH, CUS-001, INT-002, NT-010, UI0-010, UI0-011, UI-K01
- **Màn hình:** UI-G18
- **Đầu vào:** 07.2 UI-G18; 07.1 §11 (Ctrl+3); 03 §11. Prototype `#UI-G18`: `screens10.js` (palette Tool Palettes, `tpRun`, nhóm palette, menu ⋯, menu chuột phải). Thực thi macro CUS-001.
- **Đầu ra:** `app/palettes/toolpalettes/ToolPalettesWindow.{h,cpp}`: dải tab dọc các palette, menu nhóm (Tất cả, các nhóm, Nhóm mới…), danh sách công cụ (thumbnail hoặc biểu tượng, tên, nhãn loại Block, Hatch, Lệnh, LISP), menu ⋯ (Palette mới…, Khôi phục palette mặc định có xác nhận), menu chuột phải của công cụ và của tab; tự ẩn, trong suốt, dock hoặc nổi. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Công cụ block chèn theo layer, màu, tỉ lệ, góc, hỏi góc, phá khối của công cụ; sau lệnh, layer, màu và tùy chọn chèn hiện hành trở về như cũ (kiểm CLAYER, CECOLOR).
  2. Công cụ hatch tạo hatch theo mẫu, tỉ lệ, góc của công cụ, trùng HATCH gõ tay cùng tham số (so DXF).
  3. Công cụ lệnh và LISP chạy qua input pipeline, kết quả trùng khi gõ macro tay; lệnh không có báo "Không có lệnh “X”"; hàm LISP chưa nạp báo kèm nút "Nạp ứng dụng…" (UI-K01); layer của công cụ chưa có thì dùng layer hiện hành và báo.
  4. 50 palette × 200 công cụ: mở ≤ 150 ms, đổi tab ≤ 16 ms, cuộn 60 fps; cấu hình lưu nguyên tử, mở lại giữ nguyên.
  5. Xóa công cụ, xóa palette có xác nhận; không xóa được palette cuối cùng.
  6. Viewer, Viewer Pro: palette khóa.
  7. Đạt DoD-U, DoD-L.

### UIG-028 · Tool Palettes: tạo công cụ từ bản vẽ, thuộc tính công cụ
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIG-027, UIG-026, DOC-017, RND-021
- **Màn hình:** UI-G18
- **Đầu vào:** 07.2 UI-G18 (kéo đối tượng từ bản vẽ vào để tạo công cụ, thuộc tính công cụ). Prototype: `screens10.js` (kéo vùng chọn vào palette, nút "+", hộp "Thuộc tính công cụ" 480 px). Metadata thuộc tính DOC-017.
- **Đầu ra:** tạo công cụ bằng kéo-thả vùng chọn, nút "+" và mục "Tạo công cụ trên Tool Palette" trong menu chuột phải của DesignCenter (UIG-026); `app/dialogs/toolpalettes/ToolPropertiesDialog.{h,cpp}` (trường theo loại công cụ). Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Kéo một tham chiếu block vào palette: công cụ block có đúng tên, tệp nguồn, layer, màu, tỉ lệ, góc của đối tượng (đọc qua DOC-017); dùng công cụ ở bản vẽ khác cho cùng kết quả.
  2. Kéo hatch tạo công cụ hatch (mẫu, tỉ lệ, góc); kéo đối tượng khác tạo công cụ lệnh vẽ tương ứng kèm thuộc tính **[đề xuất]**.
  3. Hộp thuộc tính kiểm từng trường (tên không rỗng, tỉ lệ lớn hơn 0, macro không rỗng), báo dưới ô; Lưu công cụ cập nhật ngay.
  4. Thumbnail công cụ render ở nền ≤ 500 ms mỗi công cụ.
  5. Đạt DoD-U, DoD-L.

### UIG-029 · Tool Palettes: nhập, xuất .xtp
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** S · **Phụ thuộc:** UIG-028
- **Màn hình:** UI-G18
- **Đầu vào:** 07.2 UI-G18 (Nhập/xuất tương thích `.xtp`, P2). Prototype: mục "Nhập, xuất .xtp (sau v1.0)" vô hiệu.
- **Đầu ra:** `app/palettes/toolpalettes/XtpIO.{h,cpp}` (đọc, ghi `.xtp`), mục Nhập, Xuất trong menu ⋯, báo cáo nhập. Kiểm thử và fuzz target bộ đọc.
- **Tiêu chí nghiệm thu:**
  1. Xuất k palette ra `.xtp` rồi nhập vào profile trống: công cụ trùng 100% (tên, loại, thuộc tính, ảnh).
  2. `.xtp` do AutoCAD xuất (bộ mẫu ≥ 20 tệp của doanh nghiệp pilot): công cụ block, hatch, lệnh nhập đúng; loại chưa hỗ trợ được liệt kê trong báo cáo, không dừng cả lượt.
  3. Tệp hỏng báo lỗi rõ, không crash; fuzz bộ đọc ≥ 1 giờ không lỗi.
  4. Đạt DoD-U, DoD-L, DoD-C (mục 7).

## 9. Layout và viewport

### UIH-025 · Tab ngữ cảnh Layout: layout mới, viewport chữ nhật, tỉ lệ khi tạo, khóa, cấu hình có tên
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD:LAYOUT, CMD:MVIEW, CMD:VPORTS, INT-013, UIA-005, UIA-009, UIA-011, UIH-003, UI-D14
- **Màn hình:** UI-H11
- **Đầu vào:** 07.2 UI-H11 (phần EA); 07.1 §7.11, §9.12; 03 §13. Prototype `#UI-H11`: `screens7.js` › `SC.layoutTab`, `addVp`, MVIEW, cấu hình có tên, lệnh LAYOUT; grip viewport trong `editor.js`; tỉ lệ ở thanh trạng thái `shell.js`. README §4.2: grip của viewport do task lệnh viewport đăng ký qua INT-013; task này làm menu tỉ lệ và biểu tượng khóa. Prototype mở menu tỉ lệ khi rê grip tam giác và bỏ qua khóa; task theo khóa.
- **Đầu ra:** `app/contextual/layout/layout.ribbon.json` và binding: panel Layout (Layout mới, Thiết lập trang mở UIH-003), panel Viewport (nút tách Chữ nhật ▾ gồm Chữ nhật, Vừa khổ giấy, Theo cấu hình có tên mở UI-D14; dropdown "Tỉ lệ khi tạo"; Khóa / Mở khóa hiển thị); menu tỉ lệ của grip tam giác; biểu tượng khóa ở góc viewport. Kiểm thử và kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Tab hiện khi đang ở layout hoặc chọn một viewport; ẩn ở Model, ở Viewer và khi đang chạy lệnh; hiện/ẩn ≤ 100 ms.
  2. Viewport chữ nhật tạo với "Tỉ lệ khi tạo" (mặc định là tỉ lệ dùng gần nhất, nhớ theo bản vẽ); kết quả trùng MVIEW rồi đặt tỉ lệ qua `.scr` (so DXF).
  3. Menu tỉ lệ của grip tam giác và dropdown tỉ lệ ở thanh trạng thái dùng cùng danh sách tỉ lệ (SCALELISTEDIT). Viewport đang khóa thì cả hai báo "Viewport đang khóa hiển thị. Mở khóa trước khi đổi tỉ lệ." và không đổi gì.
  4. Khóa, Mở khóa: 1 mark undo; viewport khóa có biểu tượng khóa ở góc (không in ra); chưa chọn viewport thì báo "Chọn khung viewport hoặc kích hoạt viewport trước."
  5. Theo cấu hình có tên: mở UI-D14, tạo đủ viewport của cấu hình trong 1 mark undo.
  6. Đạt DoD-U, DoD-L.

### UIH-026 · Tab Layout phần 1.0: viewport đa giác và theo đối tượng, clip, phóng to, layer theo viewport, chuyển không gian
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIH-025, CMD:MVIEW, CMD:VPCLIP, CMD:VPMAX, CMD:VPMIN, CMD:LAYVPI, CMD:CHSPACE, CMD:EXPORTLAYOUT, UI-J11
- **Màn hình:** UI-H11
- **Đầu vào:** 07.2 UI-H11 (phần còn lại); 03 §13 (viewport đa giác, theo đối tượng, clip, VPMAX, VPMIN, CHSPACE: P1). Prototype: `screens11.js` (clip đa giác, MVIEW Đa giác và Đối tượng, VPCLIP, LAYVPI, CHSPACE, EXPORTLAYOUT), `screens7.js` (VPMAX, thu nhỏ viewport). Prototype xóa đối tượng nguồn khi tạo viewport theo đối tượng; task làm như lệnh MVIEW quy định.
- **Đầu ra:** phần 1.0 của `layout.ribbon.json`: Viewport ▾ thêm Đa giác, Theo đối tượng (polyline kín, đường tròn, ellipse, region, spline kín); Clip; Phóng to, Thu nhỏ viewport; panel Layer (Đóng băng ở viewport hiện tại, Cô lập layer trong viewport); panel Không gian (Chuyển không gian); Layout ▾ thêm Từ mẫu… (UI-J11), Xuất thành Model; banner khi phóng to viewport. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Mỗi nút cho kết quả trùng lệnh gõ tay cùng tham số (so DXF) ở 3 ca mỗi lệnh; 1 mark undo mỗi lần.
  2. Viewport đa giác cần ít nhất 3 điểm ("Cần ít nhất 3 điểm."); theo đối tượng chỉ nhận đối tượng kín ("Đối tượng chưa kín…").
  3. VPMAX: banner "Đang phóng to viewport của layout X · tỉ lệ 1:s" kèm nút Thu nhỏ viewport; VPMIN giữ tỉ lệ, viewport khóa thì giữ view.
  4. Cô lập layer trong viewport khi chưa có viewport hiện hành: nhắc kích hoạt viewport; Chuyển không gian đổi kích thước theo tỉ lệ viewport đúng ở 5 ca.
  5. Đạt DoD-U, DoD-L.

## 10. Bảng, field, tỉ lệ chú thích, hyperlink

### UIF-020 · Sửa nội dung ô bảng tại chỗ
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIF-003, UIF-004, CMD:TABLE, RGN-017, UIA-014, RGN-166
- **Màn hình:** UI-F07
- **Đầu vào:** 07.2 UI-F07 (Trên lưới bảng); 07.1 §9.10 (nhấp đúp ô bảng: sửa nội dung ô, EA); 03 §9 (sửa text trong ô là P0). Prototype `#UI-F07`: `screens9.js` (trình soạn ô `textarea.tbce`, phím Enter, Tab, Alt+Enter, Ctrl+Enter, Esc; ô khóa nội dung). Sinh lại bảng từ dữ liệu ô RGN-017.
- **Đầu ra:** `app/contextual/table/CellEditor.{h,cpp}`: trình soạn đặt đúng khung ô (vùng gộp nếu ô gộp), dùng `MTextEditor` cho định dạng chữ trong ô, dòng trạng thái gợi ý phím. Kiểm thử và kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Nhấp đúp một ô mở trình soạn đúng khung ô ≤ 150 ms; chữ trong trình soạn trùng hiển thị ô sau khi lưu (SSIM ≥ 0,99 ở 20 ô mẫu).
  2. Enter và Shift+Enter lưu rồi sang ô dưới, ô trên; Tab và Shift+Tab sang phải, trái (vòng qua hàng); Alt+Enter xuống dòng; Ctrl+Enter hoặc bấm ra ngoài thì lưu; Esc hủy ô đang sửa. Mỗi ô sửa là 1 mark undo.
  3. Ô khóa nội dung: báo "Ô B3 đang khóa nội dung. Mở khóa ở Định dạng ô › Khóa.", không mở trình soạn.
  4. Bảng 50 × 20 ô sinh lại sau khi sửa ≤ 50 ms; chiều cao hàng giãn theo nội dung.
  5. Bộ gõ tiếng Việt chạy đúng với bộ kịch bản gõ của UIF-003.
  6. Đạt DoD-U, DoD-L.

### UIF-021 · Chèn bảng (TABLE)
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIF-020, UII-011, CMD:TABLE, INT-012, UI0-007, UI0-008, UI-D04
- **Màn hình:** UI-F06
- **Đầu vào:** 07.2 UI-F06; 03 §9 (TABLE P1). Prototype `#UI-F06`: `screens9.js` › `IMPL.TABLE`, `makeTable`. Prototype kiểm hộp kiểu bảng bằng polling 250 ms; task dùng sự kiện.
- **Đầu ra:** `app/dialogs/table/InsertTableDialog.{h,cpp}` (800 px): kiểu bảng (kèm nút mở UI-D04), cách bắt đầu (bảng trống, từ liên kết dữ liệu vô hiệu "Sau v1.0", từ dữ liệu đối tượng mở trình hướng dẫn trích xuất dữ liệu UII-011), xem trước, cách chèn (điểm chèn, khung), cột và hàng, kiểu ô cho hàng đầu, hàng hai, các hàng còn lại. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Bảng tạo ra trùng `-TABLE` cùng tham số qua `.scr` (so DXF) ở 8 tổ hợp (điểm chèn hoặc khung; kiểu ô 3 hàng).
  2. Kiểm hợp lệ: "Số cột từ 1 đến 100.", "Số hàng dữ liệu từ 1 đến 500.", "Rộng cột phải lớn hơn 0.", "Cao hàng từ 1 đến 20 dòng."; nút Chèn bảng vô hiệu khi có lỗi.
  3. Kiểu bảng có bảng khởi đầu: khóa các ô số và kiểu ô, có ghi chú. Đóng UI-D04 thì danh sách kiểu cập nhật ngay.
  4. Xem trước trong hộp ≤ 16 ms sau mỗi thay đổi; bảng xem trước theo con trỏ ≤ 8 ms/khung; chèn xong vào sửa ô A1 ngay (UIF-020).
  5. Đạt DoD-U, DoD-L.

### UIF-022 · Chọn vùng ô bảng, chỉ mục hàng cột, grip đổi kích thước
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIF-020, CMD:TABLE, RGN-017, INT-013, UIA-013, RGN-166, CMD-335
- **Màn hình:** UI-F07
- **Đầu vào:** 07.2 UI-F07 (nhấp chọn ô, kéo chọn nhiều ô, Tab/Enter di chuyển, grip đổi rộng cột và cao hàng). Prototype: `screens9.js` (`cellDown`, lớp phủ chữ cột và số hàng, grip mép vùng chọn, phím mũi tên, Shift, F2, Delete, Ctrl+A; bảng hướng lên đảo chiều dọc).
- **Đầu ra:** `app/contextual/table/CellSelection.{h,cpp}`: chọn ô, vùng ô; lớp phủ (chữ cột, số hàng, viền vùng chọn, biểu tượng khóa); grip đổi rộng cột, cao hàng kèm nhãn giá trị; phím điều hướng; gõ ký tự để bắt đầu sửa. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Chọn ô và vùng ô bằng chuột (bấm, kéo, Shift) và bàn phím (mũi tên, Shift + mũi tên, Tab, Enter, Ctrl+A) đúng quy tắc trên bảng 100 × 30 có ô gộp; vẽ lớp phủ ≤ 2 ms/khung **[đề xuất]**.
  2. Kéo grip đổi rộng cột, cao hàng: nhãn "Rộng cột B: …" theo UNITS, tối thiểu theo chiều cao chữ; mỗi lần kéo 1 mark undo; trùng thao tác dòng lệnh tương ứng qua `.scr` (so DXF).
  3. Gõ một ký tự khi đang chọn ô thì bắt đầu sửa với ký tự đó; Delete xóa nội dung vùng chọn trong 1 mark undo; F2 sửa ô.
  4. Bảng hướng lên đảo chiều di chuyển dọc; bảng trên layer khóa báo "Bảng nằm trên layer bị khóa".
  5. Đạt DoD-U, DoD-L.

### UIF-023 · Tab ngữ cảnh Ô bảng và menu chuột phải của ô
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** UIF-022, CMD:TABLE, CMD:INSERT, UIA-005, UI-C06, UI-D04, CMD-335, CMD-336
- **Màn hình:** UI-F07
- **Đầu vào:** 07.2 UI-F07 (Hàng, Cột, Gộp, Kiểu ô, Định dạng ô, Chèn, Dữ liệu); 07.1 §7.11. Prototype: `screens9.js` (tab `cell` 7 panel, menu chuột phải, hộp "Viền ô" 470 px, "Chèn block vào ô" 520 px, các hộp xác nhận xóa và gộp).
- **Đầu ra:** `app/contextual/table/cell.ribbon.json` và binding: Hàng (chèn trên, dưới, xóa), Cột (chèn trái, phải, xóa), Gộp (tất cả, theo hàng, theo cột; tách), Kiểu ô (khớp ô, kiểu ô, màu nền, sửa viền), Định dạng ô (canh lề 9 vị trí và Theo kiểu ô; khóa; kiểu dữ liệu và độ chính xác), Chèn (block, field, công thức), Dữ liệu (P2, vô hiệu "Sau v1.0"); menu chuột phải cùng nội dung; hộp Viền ô, Chèn block vào ô. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Mỗi lệnh của tab và của menu chuột phải cho kết quả trùng lệnh dòng lệnh tương ứng (so DXF) ở 3 ca mỗi lệnh; 1 mark undo mỗi lần.
  2. Xóa hàng hoặc cột có nội dung: xác nhận nêu số hàng và số ô có nội dung; không cho xóa hết hàng hoặc cột; gộp làm mất nội dung: xác nhận nêu số ô; ô khóa định dạng được bỏ qua kèm thông báo.
  3. 9 kiểu dữ liệu với độ chính xác 0 đến 0.0000; tiền tệ có nhóm nghìn và " đ"; ngày hiện dd/mm/yyyy; kiểm chuỗi hiển thị ở 30 ca.
  4. Trạng thái nút phản ánh vùng chọn (khóa, canh lề, kiểu dữ liệu chung hoặc khác nhau) ≤ 16 ms khi vùng chọn đổi.
  5. Viền ô: độ dày 0,13–0,70 mm, màu, đường đôi, khoảng cách, 8 kiểu cạnh; áp cho n ô đang chọn.
  6. Đạt DoD-U, DoD-L.

### UIF-024 · Sao chép, dán giữa các ô và dán vùng ô từ Excel
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIF-022, CMD:TABLE, VN-013, CMD-335
- **Màn hình:** UI-F07
- **Đầu vào:** 07.2 UI-F07 (Ctrl+C/V giữa các ô, dán vùng ô từ Excel); README §4.2 (dán từ Excel thành bảng thuộc VN-013). Prototype: `screens9.js` (Ctrl+C/X chép TSV giá trị hiển thị và bản nội bộ giữ công thức, định dạng; Ctrl+V thêm hàng cột khi cần, bỏ qua ô khóa nội dung). Bộ đọc HTML/TSV của VN-013.
- **Đầu ra:** `app/contextual/table/CellClipboard.{h,cpp}`. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Dán vùng 50 × 10 ô chép từ Excel, LibreOffice Calc, Google Sheets (bộ mẫu HTML/TSV của VN-013): giá trị trùng 100%, số giữ kiểu số; ô gộp ở nguồn được tách và báo **[đề xuất]**.
  2. Chép trong một bảng và giữa hai bảng giữ công thức (tham chiếu dịch theo vị trí) và định dạng; dán sang Excel cho giá trị hiển thị.
  3. Thiếu hàng, cột thì thêm; ô khóa nội dung bỏ qua và báo số lượng; cả lần dán là 1 mark undo.
  4. Dán 5.000 ô ≤ 300 ms.
  5. Đạt DoD-U, DoD-L.

### UIF-025 · Công thức trong ô bảng: chọn vùng, gợi ý, báo lỗi
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UIF-023, RGN-025, DOC-022, CMD-337
- **Màn hình:** UI-F07
- **Đầu vào:** 07.2 UI-F07 (Chèn › Công thức: Tổng, Trung bình, Đếm, Ô, Biểu thức); 03 §9 (công thức trong bảng P1). Prototype: `screens9.js` (gợi ý vùng số liền kề, viền đứt nét khi chọn vùng, `evalF` và thông báo lỗi, dời tham chiếu khi chèn/xóa hàng cột). Đánh giá công thức RGN-025.
- **Đầu ra:** luồng chèn công thức trong `CellSelection` và tab Ô bảng. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Tổng, Trung bình, Đếm gợi ý đúng khối số liền kề phía trên (hoặc bên trái) ở 10 ca; Enter nhận gợi ý, kéo để đổi vùng; vùng chứa chính ô đích thì cảnh báo.
  2. Giá trị trùng engine RGN-025 ở 30 ca; lỗi hiện đúng loại (tham chiếu vòng, thiếu toán hạng, thiếu dấu ngoặc, vùng không có số, hàm không có, chia cho 0) kèm cách sửa (F2).
  3. Chèn, xóa hàng cột cập nhật tham chiếu; tham chiếu bị xóa thành `#REF`.
  4. Đạt DoD-U, DoD-L.

### UIF-026 · Hộp thoại Field và các điểm chèn field
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** UIF-007, UIF-023, UIG-013, UIG-020, CMD:FIELD, CMD:UPDATEFIELD, DOC-022, RGN-025, VN-018
- **Màn hình:** UI-F08
- **Đầu vào:** 07.2 UI-F08; 03 §9 (FIELD P1, dùng cho khung tên). Prototype `#UI-F08`: `screens9.js` › `SC.fieldDlg`, `IMPL.FIELD`, UPDATEFIELD (28 field, tùy chọn theo loại, xem trước, biểu thức chỉ đọc, ô FIELDDISPLAY). Mô hình field DOC-022, đánh giá RGN-025; Số tờ, Tổng số tờ lấy từ bộ bản vẽ VN-018.
- **Đầu ra:** `app/dialogs/field/FieldDialog.{h,cpp}` (880 px): nhóm field, ô tìm, danh sách; tùy chọn theo loại (định dạng ngày có mẫu kiểu Việt Nam; chọn đối tượng rồi thuộc tính, đơn vị, độ chính xác, tiền tố, hậu tố; thuộc tính tùy biến; đối tượng có tên; biến hệ thống; công thức; tên tệp; dung lượng; hyperlink); định dạng hoa thường; xem trước; biểu thức field. Gắn điểm chèn field vào tab Soạn văn bản (UIF-007), hộp ATTDEF (UIG-020), tab Ô bảng (UIF-023), palette Đếm (UIG-013, field đếm). Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Biểu thức field sinh ra đúng mã AutoCAD, ví dụ `%<\AcVar Date \f "dd/MM/yyyy">%`, `%<\AcObjProp Object(%<\_ObjId n>%).Area \f "%lu2%pr2%ct8[1E-06]">%`, `%<\AcSm Sheet.Number>%`, ở 28 field với các định dạng chính (đối chiếu bộ tham chiếu QA-007 khi có).
  2. Xem trước bằng giá trị engine trả về sau khi chèn; thiếu lựa chọn bắt buộc thì nút Chèn field vô hiệu và nêu thiếu gì.
  3. Đủ 4 điểm chèn (MTEXT, ATTDEF, ô bảng, palette Đếm) và lệnh FIELD (xem trước theo con trỏ); mỗi lần chèn 1 mark undo. UPDATEFIELD và menu chuột phải "Cập nhật field" cập nhật đúng số field báo trên dòng lệnh.
  4. Ngày kiểu Việt Nam và hoa/thường giữ dấu tiếng Việt đúng ở 20 ca. Danh sách field tìm không phân biệt dấu, sắp theo thứ tự tiếng Việt; nhóm Bộ bản vẽ có nhãn VN.
  5. Ô "Hiện nền xám cho field" đổi FIELDDISPLAY, vùng vẽ cập nhật ngay.
  6. Đạt DoD-U, DoD-L.

### UIF-027 · Tỉ lệ chú thích của đối tượng (OBJECTSCALE)
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD:OBJECTSCALE, DOC-021, UI0-008, UI-D11
- **Màn hình:** UI-F11
- **Đầu vào:** 07.2 UI-F11; 07.1 §7.4 (Tỉ lệ chú thích); 03 §7, §13. Prototype `#UI-F11`: `screens9.js` › `SC.objScale`. Prototype đặt cứng tỉ lệ hiện hành 1:100; task đọc CANNOSCALE. Mô hình tỉ lệ chú thích DOC-021.
- **Đầu ra:** `app/dialogs/annoscale/ObjectScaleDialog.{h,cpp}` (600 px): bảng tỉ lệ (tỉ lệ, đơn vị giấy, đơn vị bản vẽ, gắn với k/n đối tượng), lọc Mọi tỉ lệ / Chỉ tỉ lệ chung, Thêm…, Xóa, hộp Thêm tỉ lệ. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Bảng tỉ lệ gắn với vùng chọn hiện "k/n đối tượng"; tỉ lệ hiện hành lấy từ CANNOSCALE và có nhãn "hiện hành".
  2. Thêm nhiều tỉ lệ một lần từ danh sách SCALELISTEDIT; Xóa bị chặn khi đối tượng chỉ còn 1 tỉ lệ (báo lý do); OK là 1 mark undo; trùng `-OBJECTSCALE` qua `.scr` (so DXF).
  3. Vùng chọn không có đối tượng annotative: trạng thái rỗng có nút "Bật chú thích theo tỉ lệ cho n đối tượng" (1 mark undo).
  4. 10.000 đối tượng: hộp có nội dung ≤ 150 ms.
  5. Đạt DoD-U, DoD-L.

### UIF-028 · Hyperlink: gắn, sửa, gỡ, mở bằng Ctrl + bấm
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD:HYPERLINK, UIA-008, UIH-006, UIG-019, UI0-008
- **Màn hình:** UI-F15
- **Đầu vào:** 07.2 UI-F15; 03 §17 (HYPERLINK P2). Prototype `#UI-F15`: `screens19.js` › `SC.hyperlink`, `IMPL.HYPERLINK`, huy hiệu con trỏ, Ctrl + bấm; ô "Giữ hyperlink" ở hộp xuất PDF (`screens2.js`); nút Hyperlink… trong hộp BLOCK.
- **Đầu ra:** `app/dialogs/hyperlink/HyperlinkDialog.{h,cpp}` (660 px: chữ hiển thị; liên kết tới tệp hoặc trang web với gợi ý, danh sách gần đây, đường dẫn tương đối; hoặc view, layout trong bản vẽ; gỡ liên kết), huy hiệu và tooltip khi rê, Ctrl + bấm để mở; bật nút Hyperlink… của hộp BLOCK (UIG-019) và ô "Giữ hyperlink" khi xuất PDF (UIH-006). Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Gắn liên kết tệp, web, view, layout cho 1 và cho n đối tượng: dữ liệu lưu trùng `-HYPERLINK` qua `.scr` (so DXF); 1 mark undo; gỡ có xác nhận nêu số đối tượng.
  2. Rê lên đối tượng có liên kết: huy hiệu con trỏ và tooltip "Ctrl + bấm để mở" ≤ 100 ms; Ctrl + bấm mở đúng đích ở 4 loại.
  3. Tệp chạy được (exe, bat, cmd, com, msi, vbs, js, lsp, fas, vlx, scr, ps1) không mở thẳng mà hỏi và chỉ đề nghị mở thư mục chứa.
  4. Đường dẫn tương đối tính theo thư mục bản vẽ; tệp không tìm thấy vẫn gắn được, có cảnh báo. PDF xuất ra có annotation liên kết khi chọn giữ hyperlink.
  5. Viewer: mở và sao chép liên kết; Gắn, Sửa, Gỡ khóa Pro.
  6. Đạt DoD-U, DoD-L.

## 11. Block Editor, sửa tại chỗ, trích xuất dữ liệu, kiểm tra chuẩn, làm sạch, máy tính nhanh

### UIG-030 · Hộp chọn block để sửa (BEDIT): danh sách, tìm, xem trước, block mới
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD:BEDIT, UIG-015, UI0-006, UI0-008
- **Màn hình:** UI-G04
- **Đầu vào:** 07.2 UI-G04 (Vào Block Editor); 07.1 §9.10 (nhấp đúp block không có attribute). Prototype `#UI-G04`: `screens10.js` › `beDialog`, `IMPL.BEDIT`. API phiên sửa của CMD-555; thumbnail nền của UIG-015.
- **Đầu ra:** `app/dialogs/bedit/EditBlockDialog.{h,cpp}` (660 px): ô "Tìm hoặc gõ tên mới", sắp xếp, danh sách block kèm số tham chiếu (bỏ xref, block phụ thuộc xref, block của layout, block vô danh), xem trước 300×200 và chi tiết (mô tả, số đối tượng, attribute, block động), nút "Sửa tại chỗ (REFEDIT)" khi mở từ một block tham chiếu. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. PICKFIRST là block tham chiếu hoặc mở từ nhấp đúp: block đó chọn sẵn và cuộn tới. OK gửi `BEDIT`, trùng `-BEDIT <tên>` qua `.scr` (cùng trạng thái phiên).
  2. Gõ tên chưa có: dòng "Tạo block mới «tên»". Tên sai quy tắc của core báo lỗi tại ô, OK vô hiệu.
  3. 5.000 định nghĩa block **[đề xuất]**: hộp có nội dung ≤ 150 ms; lọc sau mỗi phím ≤ 16 ms; danh sách ảo hóa; thumbnail đến dần ở nền.
  4. Mũi tên chọn, Enter = OK, Esc = Hủy; trình đọc màn hình đọc tên và số tham chiếu. Viewer, Viewer Pro: khóa Pro.
  5. Đạt DoD-U, DoD-L.

### UIG-031 · Môi trường Block Editor: vào, ra, nền vùng vẽ, banner, hỏi lưu, thứ tự attribute
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** UIG-030, CMD:BEDIT, CMD:BATTORDER, UIG-022, UIA-006, UIA-008, INT-016, UI0-008, UI0-009, UIA-005
- **Màn hình:** UI-G04
- **Đầu vào:** 07.2 UI-G04 (Môi trường, Mở/Lưu, Đóng). Prototype `#UI-G04`: `screens10.js` › `beEnter`, `beChrome`, `fitView`, `beSave`, `beSaveAs`, `beClose`, `beExit`. Sysvar BLOCKEDITOR (thông báo đổi qua INT-005). Thứ tự attribute chưa có prototype riêng: dùng bố cục danh sách ATTDEF của `#UI-G08` (UIG-022).
- **Đầu ra:** `app/contextual/bedit/BlockEditorMode.{h,cpp}`: chế độ vùng vẽ theo BLOCKEDITOR (nền theo token chế độ Block Editor, prototype `data-bedit`), banner "Đang sửa block X · n tham chiếu sẽ cập nhật khi lưu", lưu view và lịch sử view khi vào, zoom vừa định nghĩa chừa chỗ banner, khôi phục khi ra; hộp hỏi lưu của BCLOSE (Lưu / Không lưu / Hủy), xác nhận lưu block rỗng, hộp tên BSAVEAS; `app/dialogs/bedit/AttributeOrderDialog.{h,cpp}` (BATTORDER: Lên, Xuống, kéo-thả). Kiểm thử cạnh mã. Nội dung tab ngữ cảnh Block Editor (panel, nút, trạng thái bật/tắt) khai báo trên khung tab ngữ cảnh của UIA-005.
- **Tiêu chí nghiệm thu:**
  1. Vào và ra: nền, banner, tab ngữ cảnh Block Editor đổi trong 1 khung (≤ 16,6 ms). Ra xong, view và lịch sử view trùng lúc vào (so ma trận view).
  2. BCLOSE khi có thay đổi: hộp nêu số tham chiếu sẽ cập nhật; Hủy giữ phiên. Đóng bản vẽ, thoát ứng dụng, BEDIT block khác khi đang sửa cũng qua hộp này. Lỗi lưu do core trả (block chứa chính nó): toast lỗi, phiên giữ nguyên.
  3. BSAVEAS: tên trùng hoặc sai quy tắc báo lỗi tại ô; lưu xong banner đổi tên ngay.
  4. BATTORDER: OK là 1 bước hoàn tác trong phiên; sau BSAVE, thứ tự nhắc của INSERT trùng danh sách.
  5. Mọi thao tác trùng chuỗi lệnh qua `.scr` (BEDIT, BSAVE, BSAVEAS, BCLOSE, BATTORDER; so DXF sau BCLOSE).
  6. Đạt DoD-U, DoD-L.
- **Ngoài phạm vi:** phần dynamic block (UIG-034, UIG-035).

### UIG-032 · Hộp Sửa tham chiếu tại chỗ (REFEDIT): nhận dạng, cây lồng nhau, thiết lập
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD:REFEDIT, UIG-015, RND-012, NT-010, UI0-006, UI0-008, UI-B05
- **Màn hình:** UI-G19
- **Đầu vào:** 07.2 UI-G19 (Mở từ, Bước chọn, Thiết lập); 07.1 §9.10. Prototype `#UI-G19`: `screens10.js` › `IMPL.REFEDIT`, `refDialog`; `screens16.js` › `IMPL.REFEDIT` (block động chuyển sang BEDIT). API chuỗi tham chiếu tại điểm chọn của CMD-556; trạng thái khóa tệp (UI-B05).
- **Đầu ra:** `app/dialogs/refedit/RefEditDialog.{h,cpp}` (640 px): tab Nhận dạng tham chiếu (cây từ tham chiếu ngoài cùng vào trong, xem trước, chi tiết tệp xref và số lần chèn, ghi chú khi xref đang bị người khác mở, Tự chọn mọi đối tượng lồng / Nhắc chọn), tab Thiết lập (tên duy nhất cho layer và kiểu trùng, hiện ATTDEF để sửa, khóa đối tượng ngoài tập làm việc; nhớ theo người dùng qua NT-010); tô sáng tham chiếu đang chọn trên vùng vẽ. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Cây khớp chuỗi lồng nhau do CMD-556 trả cho 30 ca vàng **[đề xuất]** (block trong block, block trong xref, sâu 8 mức). Chọn nút: tô sáng ≤ 16 ms, xem trước từ cache thumbnail.
  2. "Hiện ATTDEF" vô hiệu khi block không có ATTDEF. Block động: hộp không mở, dòng lệnh gợi ý BEDIT.
  3. OK trùng `-REFEDIT` cùng tùy chọn qua `.scr` (cùng tập làm việc). "Nhắc chọn": hộp ẩn, chọn trên vùng vẽ, Enter kết thúc.
  4. Xref đang bị người khác mở: ghi chú nêu người mở; vẫn sửa được, không lưu được (UIG-033).
  5. Hộp có nội dung ≤ 150 ms. Viewer, Viewer Pro: khóa Pro.
  6. Đạt DoD-U, DoD-L.

### UIG-033 · Phiên sửa tại chỗ: làm mờ, banner, tập làm việc (REFSET), lưu và hủy (REFCLOSE)
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIG-032, CMD:REFSET, CMD:REFCLOSE, CMD-557, RND-013, UIA-006, UI0-008, UI0-009, UI-B05, UIA-005
- **Màn hình:** UI-G19
- **Đầu vào:** 07.2 UI-G19 (Trong khi sửa, Kiểm tra). Prototype `#UI-G19`: `screens10.js` › `refStart`, `reChrome`, `reTab`, `IMPL.REFSET`, `IMPL.REFCLOSE`, `refSave`. Sysvar REFEDITNAME, XFADECTL; ghi tệp xref và kiểm khóa ở CMD-557.
- **Đầu ra:** `app/contextual/refedit/RefEditMode.{h,cpp}`: làm mờ đối tượng ngoài tập làm việc theo XFADECTL qua bộ đệm cờ RND-013; banner cho block, block lồng, xref ("thay đổi sẽ ghi vào KT-01.dwg") kèm Lưu thay đổi, Hủy thay đổi; trạng thái cho panel Tập làm việc của tab Sửa tham chiếu (số đối tượng trong tập, Lưu vô hiệu khi tệp khóa); thông báo khi Thêm, Bớt, khi đối tượng ngoài tập bị khóa bị bỏ khỏi vùng chọn; xác nhận Lưu, Hủy nêu số lượng. Kiểm thử cạnh mã. Nội dung tab ngữ cảnh Sửa tham chiếu (panel, nút, trạng thái bật/tắt) khai báo trên khung tab ngữ cảnh của UIA-005.
- **Tiêu chí nghiệm thu:**
  1. Cảnh 1 triệu entity (BENCH-STD): bật, tắt làm mờ trong 1 khung (≤ 16,6 ms), không dựng lại batch; đổi XFADECTL áp ngay.
  2. Lưu block: xác nhận "cập nhật n tham chiếu"; lưu xref: nêu tên tệp; Hủy: nêu số thay đổi. Lưu là 1 mark undo; Hủy trả bản vẽ trùng từng bit trước phiên.
  3. Xref bị khóa (UI-B05): banner cảnh báo nêu người mở, Lưu vô hiệu, Hủy vẫn được; khóa nhả thì Lưu bật lại, không cần thoát phiên.
  4. Thêm, Bớt qua giao diện trùng `REFSET` qua `.scr`. Đóng bản vẽ khi đang sửa hỏi Lưu / Hủy / Quay lại. BEDIT khi đang sửa tại chỗ: thông báo của lệnh, phiên giữ nguyên.
  5. Đạt DoD-U, DoD-L.
- **Ngoài phạm vi:** ghi tệp xref (CMD-557).

### UIG-034 · Palette tạo dynamic block (BAUTHORPALETTE) và cửa sổ thử block (BTESTBLOCK)
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** UIG-031, CMD:BAUTHORPALETTE, CMD:BTESTBLOCK, CMD:BPARAMETER, CMD:BACTION, CMD:BTABLE, CMD:GEOMCONSTRAINT, CMD:DIMCONSTRAINT, UI0-010, UIA-005
- **Màn hình:** UI-G05, UI-G04
- **Đầu vào:** 07.2 UI-G05, UI-G04 (Panel Thử). Prototype `#UI-G05`: `screens16.js` › `P.bauthor`, `IMPL.BAUTHORPALETTE`, `showAuthor`, `hideAuthor`, `IMPL.BTESTBLOCK`, `testOut`, `testTab`; biểu tượng ràng buộc dùng chung với tab Tham số (`screens15.js`).
- **Đầu ra:** `app/palettes/bauthor/BlockAuthoringPalette.{h,cpp}`: tab Tham số, Action, Bộ tham số, Ràng buộc; đầu palette tên block; dòng "n tham số · m action" và tối đa 3 cảnh báo bấm để chọn đối tượng; tự hiện khi vào Block Editor nếu lần trước đang mở, đóng khi BCLOSE và trả bố cục palette cũ. `app/contextual/bedit/BlockTestMode.{h,cpp}`: banner, chế độ thử lồng trong Block Editor, thoát thử trước BSAVE, BCLOSE, BEDIT hoặc khi mở màn hình khác. Kiểm thử cạnh mã. Nội dung tab ngữ cảnh Thử block (panel, nút, trạng thái bật/tắt) khai báo trên khung tab ngữ cảnh của UIA-005.
- **Tiêu chí nghiệm thu:**
  1. Mỗi công cụ gửi đúng lệnh và tham số (BPARAMETER, BACTION, BTABLE, GC…, DC…); trùng chuỗi lệnh qua `.scr` (so DXF sau BSAVE).
  2. Ngoài Block Editor: trạng thái rỗng có nút Mở Block Editor. Viewer: khóa Pro. Tab đang chọn nhớ theo người dùng.
  3. Dòng trạng thái và cảnh báo cập nhật ≤ 16 ms sau mỗi lệnh, block 2.000 đối tượng **[đề xuất]**.
  4. Đóng cửa sổ thử: định nghĩa, undo, view của phiên Block Editor trùng từng bit trước khi thử.
  5. Đạt DoD-U, DoD-L.
- **Ngoài phạm vi:** grip của tham số khi thử (CMD, RGN đăng ký qua INT-013).

### UIG-035 · Block Editor phần dynamic block: trạng thái hiển thị, bảng giá trị, menu trên tham số và action
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** UIG-031, CMD:BVSTATE, CMD:BVHIDE, CMD:BPARAMETER, CMD:BACTION, UIA-012, UIA-014, UI0-007, UI0-008
- **Màn hình:** UI-G04
- **Đầu vào:** 07.2 UI-G04 (Panel Tham số & Action, Panel Visibility, P2). Prototype `#UI-G04`: `screens16.js` › hộp `bvstate`, `bvnew`, `vsDialog`, `SC.dynPanels`, mục thêm của `TC.cad.ctxExt` và `TC.cad.hook.dbl` trong Block Editor.
- **Đầu ra:** `app/dialogs/bedit/VisibilityStatesDialog.{h,cpp}` (540 px: danh sách trạng thái, đặt hiện hành, Mới với 3 cách khởi tạo: ẩn hết, hiện hết, giữ như hiện hành; Đổi tên, Xóa, Lên, Xuống); `ValueSetDialog.{h,cpp}` (bảng giá trị của tham số thẳng, cực, xoay); mục chuột phải trong Block Editor (Sửa tập đối tượng của action…, Thêm action cho tham số…, Bảng giá trị…); nhấp đúp action mở BACTION, nhấp đúp tham số visibility mở BVSTATE. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Mọi thao tác trong hai hộp gửi BVSTATE, BPARAMETER đủ tham số, trùng chuỗi lệnh qua `.scr` (so DXF sau BSAVE); OK là 1 bước hoàn tác trong phiên.
  2. Tên trạng thái trùng hoặc rỗng báo lỗi tại ô; xóa trạng thái cuối bị chặn, có lý do.
  3. Bảng giá trị: giá trị ngoài min–max hoặc lệch bước báo lỗi tại ô.
  4. Mục chuột phải và nhấp đúp chỉ có khi BLOCKEDITOR = 1; mở menu ≤ 100 ms.
  5. Đạt DoD-U, DoD-L.

### UII-011 · Trình hướng dẫn trích xuất dữ liệu (DATAEXTRACTION): bước 1–5 rút gọn
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** CMD:DATAEXTRACTION, DOC-017, NT-013, UI0-006, UI0-008, UI0-014
- **Màn hình:** UI-I06
- **Đầu vào:** 07.2 UI-I06 (bước 1–5, bản 1.0 rút gọn). Prototype `#UI-I06`: `screens12.js` › `IMPL.DATAEXTRACTION`, trạng thái `SC.DX`, lưới gộp dòng, `dxTable`. Engine trích xuất và xuất XLSX, CSV của CMD-620; tên và nhóm thuộc tính từ DOC-017.
- **Đầu ra:** `app/dialogs/dataextract/DataExtractionWizard.{h,cpp}` (900×600, danh sách bước, Quay lại / Tiếp / Hoàn tất): Nguồn (bản vẽ hiện hành, chọn trên vùng vẽ); Loại đối tượng (chỉ block, không phải block, tất cả; chỉ block có attribute; tìm, chọn hết); Thuộc tính theo nhóm (attribute, chung, hình học…); Tinh chỉnh (lưới xem trước ảo hóa, gộp dòng giống nhau, cột Số lượng, cột Tên, sắp xếp, lọc); Đầu ra (chèn bảng, XLSX hoặc CSV, mở tệp sau khi xuất). Mở được từ hộp Chèn bảng (UIF-021) với đầu ra Chèn bảng chọn sẵn. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Bảng chèn và tệp XLSX, CSV trùng kết quả gọi engine CMD-620 trực tiếp với cùng tùy chọn (so DXF của bảng, so từng ô). Chèn bảng là 1 mark undo.
  2. Gộp dòng: tổng cột Số lượng bằng số đối tượng được trích ở mọi ca kiểm thử.
  3. 100.000 block, mỗi block 10 attribute **[đề xuất]**: lưới có dòng đầu ≤ 300 ms, sắp xếp hoặc lọc ≤ 100 ms **[đề xuất]**; chạy nền, hủy được, cuộn 60 fps.
  4. Tiếp vô hiệu khi bước chưa chọn gì, có lý do; đổi lựa chọn ở bước trước giữ lựa chọn còn hợp lệ ở bước sau.
  5. Viewer Pro: chỉ xuất XLSX/CSV (07.1 §2, xuất bảng thống kê), nút Chèn bảng vào bản vẽ khóa Pro; Viewer: khóa.
  6. Đạt DoD-U, DoD-L.
- **Ngoài phạm vi:** nhiều bản vẽ, kiểu bảng, lưu thiết lập `.dxe` (UII-013).

### UII-013 · Trích xuất dữ liệu đầy đủ: nhiều bản vẽ, kiểu bảng, lưu thiết lập
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** UII-011, CMD:DATAEXTRACTION, NT-013, CMD-622
- **Màn hình:** UI-I06
- **Đầu vào:** 07.2 UI-I06 (bước 1 nhiều bản vẽ, bước 6 Kiểu bảng, bước 7 Hoàn tất; bản đầy đủ ở mốc Sau). Prototype `#UI-I06`: `screens12.js` (ghi chú phạm vi sau v1.0). Phần core cho nhiều bản vẽ và tệp `.dxe` nằm ngoài CMD-620.
- **Đầu ra:** bổ sung vào `DataExtractionWizard`: nguồn nhiều bản vẽ (thêm tệp, thư mục, gồm thư mục con; danh sách tệp kèm trạng thái đọc); bước Kiểu bảng (chọn TABLESTYLE, tiêu đề, mở hộp Kiểu bảng); bước Hoàn tất (lưu `.dxe`; mở `.dxe` để chạy lại, nhảy tới bước bất kỳ). Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. 200 tệp của kho QA-001 **[đề xuất]**: đọc nền qua NT-013, có tiến trình và nút hủy; tệp lỗi hoặc bị khóa liệt kê kèm lý do, không dừng cả lượt.
  2. Tổng Số lượng nhiều bản vẽ bằng tổng của từng bản vẽ trích riêng.
  3. Lưu `.dxe` rồi mở lại: đủ lựa chọn 7 bước; chạy lại trên cùng bản vẽ cho bảng trùng DXF.
  4. `.dxe` hỏng hoặc khác phiên bản: hộp lỗi có chi tiết, không treo.
  5. Đạt DoD-U, DoD-L.

### UII-014 · Khung kiểm tra bản vẽ dùng chung và kiểm tra theo TCVN (TCCHECK)
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** VN-016, UII-009, NT-013, RND-012, INT-016, UI0-006, UI0-008
- **Màn hình:** UI-I09
- **Đầu vào:** 07.2 UI-I09 (Bộ quy tắc, Kết quả, Xuất báo cáo). Prototype `#UI-I09`: `screens18.js` › `ckOpen`, `IMPL.TCCHECK`, hộp `ckpanel`, `ckrules`, `report`. Engine và nội dung quy tắc ở VN-016; trích dẫn chỉ ghi số hiệu tiêu chuẩn có thật, điều khoản do chuyên gia đối chiếu.
- **Đầu ra:** `app/dialogs/check/CheckPanel.{h,cpp}` (hộp không chặn, một khung cho TCVN và chuẩn `.dws`): chọn bộ quy tắc (TCVN kiến trúc, TCVN kết cấu, quy tắc công ty); chạy nền; danh sách ảo hóa theo mức lỗi / cảnh báo / gợi ý kèm số đếm; chi tiết có giải thích và trích dẫn điều khoản; Sửa tự động (nếu có) / Bỏ qua / Ghi chú; Trước, Sau, zoom tới; lớp phủ (mục chọn đậm, mục rê nhạt). `RuleSetDialog.{h,cpp}` (880 px: bật tắt bộ và từng quy tắc, mức độ, tham số quy tắc công ty). Báo cáo xem trước, PDF, CSV dùng lại bộ xuất của UII-009. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Danh sách, số đếm theo mức và báo cáo trùng kết quả gọi VN-016 trực tiếp trên bộ bản vẽ mẫu (so từng mục).
  2. Sửa là 1 mark undo; Sửa tất cả xác nhận nêu số lượng. Sau sửa hoặc hoàn tác, danh sách cập nhật và giữ dòng đang chọn.
  3. Cảnh 1 triệu entity (BENCH-STD): kiểm tra chạy nền, có tiến trình, hủy được; vùng vẽ vẫn pan/zoom 60 fps; dòng rê tô sáng ≤ 16 ms.
  4. Bỏ qua và Ghi chú giữ theo bản vẽ; trạng thái quy tắc lưu theo người dùng.
  5. Viewer, Viewer Pro: khóa Pro.
  6. Đạt DoD-U, DoD-L.

### UII-015 · Chuẩn `.dws`: Cấu hình chuẩn (STANDARDS), kiểm tra chuẩn (CHECKSTANDARDS)
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** UII-014, CMD:STANDARDS, CMD:CHECKSTANDARDS, UIA-009, UI0-008
- **Màn hình:** UI-I09
- **Đầu vào:** 07.2 UI-I09 (kiểm tra chuẩn `.dws`). Prototype `#UI-I09`: `screens18.js` › `stdConfig` (`IMPL.STANDARDS`, hộp `stdcfg`), `IMPL.CHECKSTANDARDS`, bảng xem trước "thuộc tính | hiện tại | sau khi sửa", chỉ báo chuẩn trên thanh trạng thái.
- **Đầu ra:** `app/dialogs/check/StandardsConfigDialog.{h,cpp}` (720 px: tệp `.dws` đã gắn với Thêm, Gỡ, Lên, Xuống; tạo `.dws` từ bản vẽ; mục kiểm layer, kiểu chữ, linetype, kiểu kích thước; thiết lập); chế độ `.dws` của `CheckPanel` (đi qua từng vấn đề, chọn mục chuẩn thay thế, bảng xem trước, Sửa, Bỏ qua); chỉ báo chuẩn trên thanh trạng thái khi bản vẽ có gắn `.dws`, báo khi có vi phạm mới. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Gắn, gỡ, đổi thứ tự `.dws` qua hộp trùng `STANDARDS` qua `.scr` (so DXF). Tệp `.dws` không đọc được: lỗi có chi tiết.
  2. Danh sách vấn đề trùng CMD-533 chạy trực tiếp trên bộ bản vẽ mẫu; mỗi lần Sửa 1 mark undo; bảng xem trước tô nhấn đúng các ô khác nhau.
  3. Chỉ báo hiện hoặc ẩn ≤ 100 ms sau khi gắn, gỡ; bấm chỉ báo mở CHECKSTANDARDS.
  4. Viewer, Viewer Pro: khóa Pro.
  5. Đạt DoD-U, DoD-L.

### UII-016 · Dịch layer (LAYTRANS): ánh xạ layer nguồn sang layer chuẩn
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD:LAYTRANS, INT-021, UI0-006, UI0-008
- **Màn hình:** UI-I09
- **Đầu vào:** 07.2 UI-I09 (dịch layer: ánh xạ, lưu và nạp ánh xạ). Prototype `#UI-I09`: `screens18.js` › `SC.layTrans` (`IMPL.LAYTRANS`, hộp `laytrans`, `ltedit`, `ltset`).
- **Đầu ra:** `app/dialogs/check/LayerTranslatorDialog.{h,cpp}` (900 px): layer nguồn kèm số đối tượng; layer đích nạp từ `.dws`, `.dwg`, `.dwt`, Layer mới, Sửa layer đích; Ánh xạ, Ánh xạ cùng tên; bảng ánh xạ sửa, gỡ được; Lưu ánh xạ; hộp Thiết lập (ép màu, linetype về ByLayer, dịch cả trong block, ghi nhật ký, xem nhanh layer đang chọn); Dịch gửi LAYTRANS kèm bảng ánh xạ. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Dịch qua hộp trùng `LAYTRANS` cùng ánh xạ qua `.scr` (so DXF); 1 mark undo; xác nhận nêu số layer và số đối tượng bị đổi.
  2. Ánh xạ cùng tên không phân biệt hoa thường; layer chưa ánh xạ có biểu tượng và chữ, không chỉ dùng màu.
  3. Xem nhanh: chỉ hiện đối tượng của layer đang chọn, vừa khung ≤ 100 ms; đóng hộp trả hiển thị cũ, hash bản vẽ không đổi.
  4. 2.000 layer nguồn **[đề xuất]**: hộp có nội dung ≤ 150 ms, danh sách ảo hóa. Viewer, Viewer Pro: khóa Pro.
  5. Đạt DoD-U, DoD-L.

### UII-017 · Panel Làm sạch hình học (TCCLEANUP): quét, danh sách vấn đề, xem trước cách sửa
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** CMD:TCCLEANUP, UII-014, RND-012, INT-016, NT-013, UI0-007
- **Màn hình:** UI-I08
- **Đầu vào:** 07.2 UI-I08. Prototype `#UI-I08`: `screens18.js` › `IMPL.TCCLEANUP`, hộp `gclean` (tab Hình học), lớp phủ dấu và hình xem trước cách sửa. Thứ tự quét, dung sai, quy tắc sửa ở CMD-075.
- **Đầu ra:** `app/dialogs/cleanup/GeometryCleanupPanel.{h,cpp}` (440 px, không chặn): phạm vi (Model, layer đang hiện, lọc layer, chọn trên vùng vẽ); bật tắt và dung sai từng loại (khe hở, đoạn thừa, đoạn hụt, góc gần 0°/90° bị lệch, trùng, độ dài 0) bằng ô UI-C09 có lấy khoảng cách từ bản vẽ; Quét có tiến trình và Dừng; danh sách theo nhóm dùng lại danh sách và báo cáo của `CheckPanel` (UII-014); Sửa / Sửa tất cả / Bỏ qua, zoom tới; lớp phủ dấu theo loại kèm số thứ tự, cách sửa (phần thêm, phần bỏ, nét đứt). Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Danh sách trùng `-TCCLEANUP` Quét cùng thiết lập qua `.scr`; Sửa, Sửa tất cả trùng `-TCCLEANUP` Sửa (so DXF), mỗi lần 1 mark undo; Sửa tất cả xác nhận nêu số lượng.
  2. Rê hoặc chọn dòng: tô sáng đối tượng và hiện cách sửa ≤ 16 ms. Sau sửa, hoàn tác, đổi bản vẽ: quét lại không hiện tiến trình, giữ dòng đang chọn; dòng vừa sửa mất thì chọn dòng kế.
  3. Cảnh 1 triệu entity (BENCH-STD): quét nền, vùng vẽ vẫn 60 fps; Dừng giữ kết quả cũ.
  4. Dung sai lưu theo người dùng; Bỏ qua giữ theo từng bản vẽ đang mở. Viewer, Viewer Pro: khóa Pro.
  5. Đạt DoD-U, DoD-L.

### UII-018 · Trang Sức khỏe bản vẽ: tóm tắt một trang và sửa nhanh
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** UII-017, CMD:TCCLEANUP, UI-B09, UI-J02, UI-G01
- **Màn hình:** UI-I08
- **Đầu vào:** 07.2 UI-I08 (trang Sức khỏe bản vẽ). Prototype `#UI-I08`: `screens18.js` (tab Sức khỏe bản vẽ, nút sửa nhanh, báo cáo `Suc-khoe_<tên>`); tùy chọn Sức khỏe của `-TCCLEANUP` (CMD-075).
- **Đầu ra:** tab Sức khỏe bản vẽ của `GeometryCleanupPanel`: điểm /100 và mức; mục regapp, block không dùng, đối tượng độ dài 0, proxy, xref thiếu, font bị thay, dung lượng có thể giảm; mỗi mục có nút sửa nhanh (mở UI-B09 Dọn dẹp, UI-J02 Bác sĩ Font, UI-G01 Tham chiếu ngoài; xóa đối tượng độ dài 0); xuất báo cáo. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Số liệu từng mục và điểm trùng `-TCCLEANUP` Sức khỏe trên bộ bản vẽ mẫu; mở trang không đổi hash bản vẽ.
  2. Sửa nhanh mở đúng màn hình với phạm vi lọc sẵn (ví dụ Dọn dẹp chỉ regapp). Xóa đối tượng độ dài 0: xác nhận nêu số lượng, 1 mark undo.
  3. Sau sửa nhanh hoặc hoàn tác, trang cập nhật ở nền ≤ 500 ms **[đề xuất]**, không chặn luồng UI.
  4. Bản vẽ không có vấn đề: trạng thái rỗng; mục không áp dụng (không có xref) ẩn.
  5. Đạt DoD-U, DoD-L.

### UII-019 · Palette Máy tính nhanh (QUICKCALC) và tính cho ô số
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** CMD:QUICKCALC, INT-022, UI0-007, UI0-010, UIA-007, UI-C01
- **Màn hình:** UI-I10
- **Đầu vào:** 07.2 UI-I10; 07.2 UI-C09 (nút máy tính trong ô số). Prototype `#UI-I10`: `screens19.js` › `IMPL.QUICKCALC`, hộp `qcf` (chế độ palette và chế độ tính cho một ô), `qcvar`; `ui.js` (nút máy tính, Ctrl+8). Tính biểu thức, hàm, biến, lịch sử, quy đổi đơn vị qua engine QUICKCALC của INT (task khai báo QUICKCALC, trên INT-022).
- **Đầu ra:** `app/palettes/quickcalc/QuickCalcPalette.{h,cpp}`: ô biểu thức, lịch sử 40 dòng; bàn phím số, bộ nhớ, hàm khoa học; biến người dùng (hằng hoặc hàm có `cur`, `rad`; hộp Biến mới/Sửa biến 470 px); 4 nút lấy từ bản vẽ (tọa độ, khoảng cách, góc, giao hai đường qua 4 điểm); quy đổi độ dài, diện tích, thể tích, góc; dán vào dòng lệnh (`[x,y]` thành `x,y`), áp vào ô đang sửa của palette Thuộc tính; chế độ tính cho một ô mở từ nút máy tính hoặc Ctrl+8 trong ô số UI0-007. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Kết quả trùng engine INT-022 trên bộ biểu thức kiểm thử (số có đơn vị, `^`, ngoặc, tọa độ, hàm, biến); số theo quy ước bản vẽ (dấu chấm, tối đa 6 chữ số lẻ).
  2. Lấy từ bản vẽ khi một lệnh đang chạy: `'QUICKCALC` trong suốt, có OSNAP; Esc quay lại máy tính, lệnh bên ngoài không bị hủy.
  3. Áp vào Thuộc tính là 1 bước hoàn tác, trùng sửa tay giá trị đó; giá trị dán vào dòng lệnh dùng được ngay khi lệnh hỏi điểm.
  4. Ký tự gõ hiện ≤ 16 ms; biểu thức 200 ký tự có kết quả ≤ 16 ms sau Enter **[đề xuất]**. Viewer, Viewer Pro: khóa Pro.
  5. Đạt DoD-U, DoD-L.
- **Ngoài phạm vi:** evaluator, hàm, biến, lịch sử, bảng quy đổi đơn vị (engine QUICKCALC của INT).

## Tóm tắt theo mốc

| Mốc | Task | Số task |
|---|---|---|
| V | UII-001, UII-002, UIH-001, UIH-002, UIH-003, UIH-005, UIH-006, UIH-007, UIG-001, UIG-002, UIF-001 | 11 |
| VP | UII-003, UII-006, UII-007, UII-008, UII-009, UIH-009, UIH-010, UIH-015, UIH-016, UIH-017, UIH-018, UIG-013 | 12 |
| EA | UII-004, UIH-004, UIG-003, UIG-004, UIG-005, UIG-006, UIG-007, UIG-008, UIF-002, UIF-003, UIF-004, UIF-005, UIF-006, UIF-007, UIF-008, UIF-009, UIF-016, UIG-014, UIG-015, UIG-016, UIG-017, UIG-019, UIG-020, UIG-021, UIH-025, UIF-020 | 26 |
| 1.0 | UII-005, UII-012, UIH-008, UIH-011, UIH-019, UIH-020, UIH-021, UIH-022, UIG-009, UIG-010, UIG-011, UIF-010, UIF-011, UIF-012, UIF-013, UIF-017, UIF-018, UIF-019, UIG-018, UIG-022, UIG-023, UIG-025, UIG-026, UIG-027, UIG-028, UIH-026, UIF-021, UIF-022, UIF-023, UIF-024, UIF-025, UIF-026, UIF-027, UIG-030, UIG-031, UIG-032, UIG-033, UII-011 | 38 |
| Sau | UII-010, UIH-012, UIH-013, UIH-014, UIH-023, UIH-024, UIG-012, UIF-014, UIF-015, UIG-024, UIG-029, UIF-028, UIG-034, UIG-035, UII-013, UII-014, UII-015, UII-016, UII-017, UII-018, UII-019 | 21 |
| **Tổng** | | **108** |
