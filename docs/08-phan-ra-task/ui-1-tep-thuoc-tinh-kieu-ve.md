# UI 1 — Tệp, thuộc tính, layer, kiểu và thiết lập, vẽ và hiệu chỉnh

> Task giao diện cho màn hình tệp và tiện ích bản vẽ (UI-B01–B14), thuộc tính và layer (UI-C01–C11), kiểu và thiết lập (UI-D01–D14), vẽ, hiệu chỉnh, prompt (UI-E01–E10) và tab Tham số (UI-PAR, sau v1.0). Tiền tố `UIB`, `UIC`, `UID`, `UIE`.
> Thư mục mã: `app/dialogs/`, `app/palettes/`. Mẫu task, DoD, ranh giới sở hữu và task neo: [README](README.md).
> Căn cứ: [07.1](../07-dac-ta-ui-1-khung-ung-dung.md) §1, §2, §5, §7.5, §9.3, §12, §13 · [07.2](../07-dac-ta-ui-2-bang-va-hop-thoai.md) §0–§5, §11 · [07.3](../07-dac-ta-ui-3-tinh-nang-vn-va-he-thong.md) Phụ lục A–C · prototype `design/prototype/index.html#UI-xxx` (hàm mở từng mã trong `js/catalog.js`).

## 0. Ghi chú thiết kế

- **Không có task neo, không có dòng Lệnh.** Module khác phụ thuộc vào màn hình bằng mã `UI-xxx` (README §5.14). Màn hình gọi lệnh của CMD/INT (`CMD:TÊN`, anchor INT); lệnh mở màn hình gọi móc UI do task ở đây đăng ký.
- **Sửa bản vẽ chỉ qua lệnh** (DoD-U 5, 07.2 §0 "Quy ước áp dụng"). Palette áp dụng ngay: mỗi lần sửa là một lệnh, một mark undo. Hộp thoại modal gửi một lệnh (hoặc một nhóm lệnh chung một mark undo) khi bấm nút xác nhận, qua UI0-015 → INT-002. Chỗ prototype ghi "OK", bản Qt ghi tên hành động theo 07.2 §0 ("Áp dụng", "Lưu thiết lập") **[đề xuất]**, nêu trong PR theo DoD-U 1.
- **Đọc qua snapshot, tính ở nền:** dữ liệu bản vẽ đọc qua DOC-009 trên luồng nền NT-005, áp vào model Qt theo lô (UI0-014). Danh sách hơn 200 dòng luôn ảo hóa (UI0-006). Việc nền bị hủy khi vùng chọn đổi hoặc màn hình đóng, trong ≤ 50 ms (DoD-H 5).
- **Một nguồn metadata:** tên, nhóm, kiểu giá trị, mã DXF, biến hệ thống của thuộc tính lấy từ DOC-017. Thuộc tính, Thuộc tính nhanh, QSELECT, FILTER, so sánh kiểu kích thước không tự ánh xạ trường (README §4.2). Số trong vùng vẽ định dạng bằng RGN-013.
- **Thành phần dùng chung một bản** (07.1 §1 mục 7): ô số UI-C09 là UI0-007; bộ chọn màu UIC-017, linetype UIC-018, lineweight UIC-019 dùng cho palette, hộp thoại và ribbon.
- **Xem trước bằng regen thật [đề xuất]:** kiểu chữ, kiểu kích thước, multileader, bảng, đa tuyến song song, linetype, kiểu điểm vẽ bằng RGN + RND-021 trên tài liệu mẫu tạm (UID-001), không vẽ phỏng bằng QPainter.
- **Cổng phiên bản** qua UI0-011 (07.1 §2): Viewer dùng chế độ chỉ đọc hoặc tạm thời, không lưu vào tệp; tính năng thiếu hiện huy hiệu khóa, bấm mở UI-L04.
- **Prompt song ngữ [đề xuất]:** lệnh khai báo prompt và từ khóa bằng chuỗi nguồn tiếng Anh qua INT-003. Bản dịch tiếng Việt và nhãn chip "Tiếng Việt · TẮT" (07.2 UI-E10) thuộc UIE-015…UIE-019.
- **Phụ thuộc ngầm của mọi task, không ghi lại từng task:** QA-011 (ảnh chụp, đo độ trễ), UI0-002 (theme), UI0-003 (font), UI0-004 (biểu tượng), UI0-005 (dịch), UI0-013 (tiếp cận) theo DoD-U; dữ liệu QA-001 và QA-012 (kho bản vẽ, BENCH-STD). Bộ tham chiếu QA-007 ghi rõ ở task dùng nó.
- **Vị trí mã [đề xuất]:** `app/dialogs/<nhóm>/` (file, util, pickers, style, settings, options, view, draw, select, prompts, param) và `app/palettes/<tên>/` (properties, layers, params). Kiểm thử trong `tests/` cạnh mã, kịch bản QA-011 trong `tests/ui/`, benchmark trong `bench/`. Tab ngữ cảnh (Hatch, Mảng, Tham số) là định nghĩa JSON cho khung ribbon UIA-005 kèm controller, đặt trong `app/dialogs/draw/` và `app/dialogs/param/`.

## 1. Tệp và trạng thái tệp (UI-B01–B07, B10, B13)

### UIB-001 · Đọc nhanh thông tin xem trước của tệp bản vẽ
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** DWG-004, DWG-026, DWG-027, NT-005, NT-011
- **Màn hình:** UI-B01, UI-B07
- **Đầu vào:** 07.1 UI-B01 (khung xem trước: thumbnail, phiên bản DWG, "lưu lần cuối bởi", kích thước); API nhận diện phiên bản DWG-004, đọc ảnh xem trước DWG-026, section SummaryInfo và AppInfo DWG-027; đọc bất đồng bộ NT-011; prototype `index.html#UI-B01`, `js/screens1.js` `TC.files.open` (hàm `info`, `drawRecentThumb`).
- **Đầu ra:** `app/dialogs/file/FilePreviewService.{h,cpp}`: `requestPreview(path) → future<FilePreviewInfo>` (mã AC10xx và tên "DWG 2018", ứng dụng lưu cuối, ngày sửa, dung lượng, `QImage` thumbnail, mã lỗi); cache LRU theo (đường dẫn, mtime, kích thước) 64 mục **[đề xuất]**; DWG, DXF, DWT chung một API; kiểm thử `app/dialogs/file/tests/file_preview_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Chỉ đọc header, ảnh xem trước, SummaryInfo/AppInfo; không giải mã đối tượng. ≤ 30 ms p99 mỗi tệp **[đề xuất]** với tệp 100 MB của BENCH-STD, máy tầm trung, ổ SSD.
  2. Phiên bản và "lưu lần cuối bởi" đúng 100% trên bộ 1.000 tệp của QA-001 (so với metadata của kho).
  3. Tệp hỏng, không phải bản vẽ, không có quyền đọc: trả mã lỗi, không crash (200 tệp lỗi của QA-001).
  4. Chạy ở nền; yêu cầu mới hủy yêu cầu cũ ≤ 50 ms; luồng UI không bị chặn quá 2 ms.
  5. Đạt DoD-U, DoD-L.

### UIB-002 · Hộp thoại Mở tệp
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIB-001, CMD:OPEN, DOC-018, UIA-010, UI0-008, UI0-015, NT-010
- **Màn hình:** UI-B01
- **Đầu vào:** 07.1 UI-B01; 07.1 §13.6 (hộp thoại tệp của hệ điều hành, dạng sheet trên macOS); prototype `index.html#UI-B01`, `js/screens1.js` `TC.files.open`.
- **Đầu ra:** `app/dialogs/file/OpenFileDialog.{h,cpp}`. Windows: `IFileDialog` gốc với `IFileDialogCustomize`: nút Mở thả xuống "Mở chỉ đọc" (`EnableOpenDropDown`), nhóm nhãn thông tin cập nhật theo `OnSelectionChange` **[đề xuất]**. macOS: `NSOpenPanel` có accessory view xem trước. Linux: `QFileDialog` kèm widget xem trước. Bộ lọc DWG, DXF, DWT, "Mọi tệp CAD"; chọn nhiều tệp; nhớ thư mục cuối (NT-010); kịch bản `app/dialogs/file/tests/ui/open_dialog.scenario`.
- **Tiêu chí nghiệm thu:**
  1. Chọn 3 tệp rồi Mở: 3 tab mới (UIA-010), tab cuối hiện hành; mỗi tệp đi qua một lệnh OPEN (INT-002), lịch sử dòng lệnh ghi đủ 3 lệnh.
  2. "Mở chỉ đọc": tài liệu mở với cờ chỉ đọc của DOC-018, tab có biểu tượng khóa, tiêu đề cửa sổ ghi "[Chỉ đọc]".
  3. Khung xem trước (thumbnail, phiên bản, lưu lần cuối bởi, kích thước) cập nhật ≤ 50 ms p99 **[đề xuất]** sau khi đổi lựa chọn; tệp không có thumbnail hiện trạng thái rỗng có chữ.
  4. Bộ lọc đúng phần mở rộng, không phân biệt hoa thường; tên tệp tiếng Việt có dấu và đường dẫn UNC mở được.
  5. Viewer mở được DWG, DXF, DWT (chỉ xem); mở hộp thoại đến khung hình đầu ≤ 150 ms; Esc đóng và trả focus về vùng vẽ.
  6. Đạt DoD-U, DoD-L.
- **Ngoài phạm vi:** thumbnail trong Explorer (PLF-115); tệp bị khóa (UIB-011); tệp lỗi (UIB-007).

### UIB-003 · Mở tệp lớn: hiển thị dần, tiến trình, nút hủy
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** DOC-016, NT-013, INT-001, INT-020, UIA-008, UIA-009, UI0-009
- **Màn hình:** UI-B13
- **Đầu vào:** 07.1 UI-B13; 07.1 §12 (thao tác > 2 s có % và nút hủy); trạng thái và sự kiện tiến độ nạp dần của DOC-016; đăng ký tác vụ nền NT-013; cổng lệnh INT-020 (chỉ lệnh xem và đo chạy khi tài liệu chưa nạp xong); prototype `index.html#UI-B13`, `js/screens1.js` `F.openLarge`.
- **Đầu ra:** `app/dialogs/file/ProgressiveLoadPresenter.{h,cpp}`: mục trên thanh trạng thái "Đang nạp 63% · 1,2 triệu đối tượng" kèm nút hủy; toast khi xong ("Đã mở … trong 3,9 giây"); thông điệp dòng lệnh khi gọi lệnh sửa lúc đang nạp; kịch bản `app/dialogs/file/tests/ui/large_open.scenario`.
- **Tiêu chí nghiệm thu:**
  1. Tệp 100 MB của BENCH-STD: khung hình đầu có nội dung hiện trước khi nạp xong; trong lúc nạp, pan/zoom ≥ 60 fps trên máy tầm trung (05 §1.1).
  2. Tiến trình cập nhật 2–10 lần/giây **[đề xuất]**, số đối tượng theo định dạng Việt Nam (1.240.000); không timer nào chạy khi không có tác vụ (DoD-H 6).
  3. Hủy: dừng nạp ≤ 50 ms, giữ phần đã nạp, toast cảnh báo nêu % đã nạp.
  4. Gọi một lệnh có cờ sửa của INT-001 (lệnh thử đăng ký trong kiểm thử) trong lúc nạp: lệnh không chạy, dòng lệnh báo "Lệnh sửa chờ đến khi nạp xong"; lệnh xem và đo vẫn chạy.
  5. Đạt DoD-U, DoD-L.

### UIB-004 · Hộp thoại Lưu thành: chọn tệp, định dạng, gợi ý tương thích
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-021, DWG-022, DWG-024, RND-021, NT-010, UI0-008
- **Màn hình:** UI-B02
- **Đầu vào:** 07.1 UI-B02 (định dạng, gợi ý tương thích, "Đặt làm định dạng mặc định"; VP chỉ dùng để lưu bản sao sau chuyển font); 03 §Tệp (lưu 2018 và 2004 P0; 2000, 2007, 2010, 2013 P1; R12/R14 P2); bộ ghi DWG-021 (AC1018), DWG-022 (AC1032), DXF DWG-024; prototype `index.html#UI-B02`, `js/editor.js` `ED.saveAs`, `FORMATS`.
- **Đầu ra:** `app/dialogs/file/SaveAsDialog.{h,cpp}`: hộp thoại tệp của hệ điều hành cộng phần TrionCAD; trả về (đường dẫn, định dạng) cho nơi gọi. Ô định dạng: DWG 2018 (khuyến nghị), 2013, 2010, 2007, 2004, 2000; DXF cùng các phiên bản; DWT; "R14 · R12" hiện vô hiệu kèm "sau v1.0". Định dạng chưa có bộ ghi ở mốc hiện tại hiện vô hiệu kèm lý do. Dòng gợi ý tương thích ("Mở được bằng AutoCAD 2018 trở lên và TrionCAD"); ô "Đặt làm định dạng mặc định" (NT-010); thumbnail của bản vẽ sẽ lưu kèm (RND-021); kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Đổi định dạng: gợi ý tương thích đổi trong cùng khung hình; phần mở rộng tên tệp đổi theo (.dwg, .dxf, .dwt).
  2. Tên trống hoặc chứa `\ / : * ? " < > |`: viền lỗi và câu giải thích, không trả kết quả; trùng tệp có sẵn: hỏi "Thay thế" nêu tên tệp.
  3. Định dạng mặc định nhớ theo hồ sơ; mở lại hộp thoại chọn sẵn định dạng đó.
  4. Viewer Pro: luồng lưu bản sao sau chuyển font (UI-J02, UI-J03) gọi được hộp thoại; tệp ghi ra mở lại đúng phiên bản (kiểm bằng DWG-004). Viewer không mở được hộp thoại (UI0-011).
  5. Đạt DoD-U, DoD-L.

### UIB-005 · Lưu thành bản Pro: lệnh SAVEAS, cảnh báo giảm tính năng, mẫu DWT
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** UIB-004, CMD:SAVEAS, CMD:QSAVE, UI0-015
- **Màn hình:** UI-B02
- **Đầu vào:** 07.1 UI-B02 (cảnh báo khi lưu phiên bản cũ làm giảm tính năng; thông báo một lần khi lưu DWG lần đầu; phần mẫu DWT); danh sách tính năng bị giảm theo phiên bản do lệnh SAVEAS trả về (annotative, độ trong suốt, bảng, DIMASSOC…); prototype `js/editor.js` `lossy`, `doSave`.
- **Đầu ra:** `app/dialogs/file/SaveAsProFlow.{h,cpp}`: gắn UIB-004 vào lệnh SAVEAS và lần lưu đầu của QSAVE; hộp cảnh báo liệt kê ảnh hưởng; phần DWT (mô tả, đơn vị đo Mét/Anh); thông báo lần đầu lưu DWG có [Tìm hiểu] và "Không hiện lại"; kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Mỗi lần xác nhận gửi đúng một lệnh SAVEAS (định dạng, đường dẫn) qua INT-002; tệp ghi ra mở lại đúng phiên bản; bản vẽ hết dấu chưa lưu.
  2. Bản vẽ mẫu có annotative, độ trong suốt, bảng: chọn DWG 2004 hiện đủ danh sách ảnh hưởng SAVEAS trả về, nút xác nhận đổi thành "Vẫn lưu DWG 2004"; không có ảnh hưởng thì không hiện cảnh báo.
  3. Thông báo lần đầu lưu DWG hiện đúng một lần mỗi hồ sơ; "Không hiện lại" khôi phục được ở UI-D10 (UI0-008).
  4. Lưu DWT: mở lại tệp `.dwt` đọc đúng mô tả và MEASUREMENT đã chọn.
  5. Hành động "Lưu thành" đăng ký ở UI0-015: nút [Lưu thành…] của banner phục hồi (UIB-007) và báo cáo phục hồi (UIB-008) mở hộp này ở bản Pro.
  6. Đạt DoD-U, DoD-L.

### UIB-006 · Đóng khi chưa lưu: một tệp và nhiều tệp
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD:CLOSE, CMD:QSAVE, DOC-018, UIB-005, UI0-008
- **Màn hình:** UI-B03
- **Đầu vào:** 07.1 UI-B03; 07.1 §1 mục 4 (không làm mất dữ liệu); trạng thái chưa lưu theo tài liệu DOC-018; prototype `index.html#UI-B03`, `js/editor.js` `TC.app.closeDoc`, `TC.app.closeDocs`.
- **Đầu ra:** `app/dialogs/file/UnsavedChangesDialog.{h,cpp}` (một tệp: Lưu / Không lưu / Hủy, kèm số thay đổi và lần lưu trước; nhiều tệp: bảng có ô chọn, "Lưu các tệp đã chọn" / "Không lưu" / "Hủy"); móc vào CLOSE, CLOSEALL, đóng tab, thoát ứng dụng; kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Một tệp: Lưu chạy QSAVE và chỉ đóng khi lưu thành công; lưu lỗi (ổ đầy giả lập) thì tab vẫn mở và hiện hộp thoại lỗi.
  2. Đóng nhiều tệp hoặc thoát khi có 10 tệp chưa lưu: một hộp duy nhất; chỉ tệp được chọn được lưu; Hủy không đóng tệp nào.
  3. Tệp chưa từng lưu hoặc chỉ đọc: Lưu mở Lưu thành (UIB-005).
  4. Hộp hiện ≤ 150 ms; Enter là nút chính, Esc là Hủy; Viewer không bao giờ hỏi.
  5. Đạt DoD-U, DoD-L.

### UIB-007 · Phục hồi khi mở tệp lỗi
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD:RECOVER, UIB-008, NT-013, UIA-010, UIA-015
- **Màn hình:** UI-B04
- **Đầu vào:** 07.1 UI-B04 (hỏi "Tệp có lỗi cấu trúc. Chạy Phục hồi?" → tiến trình → báo cáo → mở bản phục hồi gắn nhãn "[Đã phục hồi]", gợi ý lưu thành tệp mới); 07.1 §12; lệnh RECOVER (dùng DWG-020); prototype `index.html#UI-B04`, `js/screens1.js` `F.openBroken`.
- **Đầu ra:** `app/dialogs/file/RecoverFlow.{h,cpp}`: hộp hỏi, hộp tiến trình có nhật ký từng bước và nút Hủy, gọi báo cáo UIB-008, mở tab "[Đã phục hồi]" và banner "Bản vẽ đã được phục hồi…" có [Lưu thành…] (khóa ở Viewer) và [Xem báo cáo]; kịch bản `app/dialogs/file/tests/ui/recover.scenario`.
- **Tiêu chí nghiệm thu:**
  1. 50 tệp nhóm "cần phục hồi" của QA-001: đi đủ luồng hỏi → tiến trình → báo cáo → tab "[Đã phục hồi]"; tệp gốc không đổi (so hash trước và sau).
  2. Tiến trình hiện ≤ 0,5 s sau khi bắt đầu; quá 2 s có % và nút Hủy; Hủy dừng ≤ 50 ms và không mở tab.
  3. Viewer: [Lưu thành…] hiện huy hiệu Pro, bấm mở UI-L04; nút gọi hành động "Lưu thành" của UI0-015 (bản Pro gắn ở UIB-005).
  4. Đạt DoD-U, DoD-L.

### UIB-008 · Báo cáo phục hồi
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD:RECOVER, DOC-015, DWG-020, UI0-006, UI0-008
- **Màn hình:** UI-B10
- **Đầu vào:** 07.2 UI-B10 (tóm tắt số lỗi tìm thấy / đã sửa; nhật ký chi tiết; Lưu báo cáo; hành động tiếp theo); nhật ký sửa của DWG-020 và DOC-015; prototype `js/screens1.js` hàm `report`.
- **Đầu ra:** `app/dialogs/file/RecoveryReportDialog.{h,cpp}` (ba số tóm tắt, nhật ký thu gọn được dạng danh sách ảo hóa, "Lưu báo cáo…", "Sao chép chi tiết", "Lưu thành…", "Mở bản đã phục hồi"; ghi chú Viewer); dùng chung cho AUDIT ở UIB-009.
- **Tiêu chí nghiệm thu:**
  1. Ba số tóm tắt bằng đúng số mục trong nhật ký DWG-020/DOC-015 (kiểm trên 50 tệp của UIB-007).
  2. Nhật ký 100.000 dòng cuộn 60 fps; tìm trong nhật ký mỗi phím ≤ 16 ms.
  3. "Lưu báo cáo" ghi tệp `.txt` UTF-8 trùng từng dòng nhật ký; "Sao chép chi tiết" đưa cùng nội dung vào clipboard.
  4. Đạt DoD-U, DoD-L.

### UIB-009 · Báo cáo kiểm tra lỗi AUDIT
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** UIB-008, UIB-005, UIB-018, CMD:AUDIT
- **Màn hình:** UI-B10
- **Đầu vào:** 07.2 UI-B10; 07.2 UI-E10 dòng `AUDIT` (hỏi "Sửa các lỗi tìm thấy?" Yes/No, báo cáo UI-B10); nhật ký kiểm tra của lệnh AUDIT (DOC-015); prototype `index.html#UI-B10`, `js/screens8.js` `IMPL.AUDIT`.
- **Đầu ra:** `app/dialogs/file/AuditReportPresenter.{h,cpp}`: dùng hộp của UIB-008 với số "Đã kiểm tra / Lỗi tìm thấy / Đã sửa", ghi chú khi chọn không sửa, nút "Mở Dọn dẹp" (UIB-018), "Lưu thành…", "Lưu báo cáo…" (`.adt`).
- **Tiêu chí nghiệm thu:**
  1. AUDIT Yes và No trên 30 tệp có lỗi của QA-001: số lỗi trong báo cáo bằng số mục của nhật ký DOC-015; chọn No thì "Đã sửa" bằng 0 và bản vẽ không bị đánh dấu chưa lưu.
  2. Chạy `AUDIT` từ `.scr` và `(command "_.AUDIT" "_Y")`: không mở hộp báo cáo, chỉ ghi dòng lệnh (giữ script chạy được).
  3. "Lưu báo cáo…" ghi `.adt` cùng nội dung nhật ký.
  4. Đạt DoD-U, DoD-L.

### UIB-010 · Phục hồi cả xref lồng nhau (RECOVERALL)
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UIB-007, UIB-008, CMD:RECOVERALL
- **Màn hình:** UI-B04, UI-B10
- **Đầu vào:** 07.1 UI-B04 ("Phục hồi luôn các xref lồng nhau", báo cáo theo từng tệp); 03 §Tệp (`RECOVERALL` P1); kết quả theo tệp của lệnh RECOVERALL; prototype chưa có màn hình riêng, dùng luồng và báo cáo của `js/screens1.js` `F.openBroken`, `report`.
- **Đầu ra:** ô chọn "Phục hồi luôn các xref lồng nhau" trong hộp hỏi của UIB-007; báo cáo nhiều tệp (cây: tệp chủ → xref, mỗi nút có số lỗi/đã sửa và nhật ký riêng) trong `app/dialogs/file/RecoveryReportDialog`; kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Bản vẽ có 3 cấp xref, 5 tệp lỗi: báo cáo liệt kê đủ 6 tệp theo cấp, số lỗi từng tệp khớp nhật ký RECOVERALL.
  2. Tiến trình theo tệp ("Tệp 3/6"), Hủy dừng sau tệp đang xử lý, các tệp chưa chạy ghi "Chưa phục hồi".
  3. Đạt DoD-U, DoD-L.

### UIB-011 · Tệp đang được người khác mở
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** DOC-019, CMD:OPEN, CMD:QSAVE, UIB-002, UIB-005, UI0-009
- **Màn hình:** UI-B05
- **Đầu vào:** 07.1 UI-B05 (người mở, tên máy, thời điểm; "Mở chỉ đọc" / "Báo khi tệp được giải phóng" (P2) / "Hủy"); khóa `.dwl`/`.dwl2` của DOC-019; prototype `index.html#UI-B05`, `js/screens8.js` `TC.files.openRecent`.
- **Đầu ra:** `app/dialogs/file/FileLockedDialog.{h,cpp}`; banner sau khi mở chỉ đọc "Chỉ đọc: <người> (<máy>) đang mở tệp này" có [Lưu thành…]; nút "Báo khi tệp được giải phóng" vô hiệu kèm "sau v1.0"; kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Tệp có khóa do DOC-019 ghi (và khóa mẫu do AutoCAD tạo trong QA-001): hộp hiện đúng người, máy, thời điểm, ứng dụng; ngày giờ dạng dd/mm/yyyy hh:mm.
  2. "Mở chỉ đọc" không ghi hay xóa tệp khóa (so thư mục trước và sau); QSAVE trên tài liệu này mở Lưu thành.
  3. Thư mục mạng UNC: hộp hiện ≤ 300 ms **[đề xuất]** sau khi bấm Mở (đọc khóa ở nền).
  4. Đạt DoD-U, DoD-L.
- **Ngoài phạm vi:** báo khi tệp được giải phóng (P2, chưa có thiết kế đợt 5).

### UIB-012 · Quét tham chiếu thiếu khi mở và banner không chặn
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** DOC-013, RGN-006, RGN-021, PLT-002, NT-011, NT-005, UIA-015
- **Màn hình:** UI-B06
- **Đầu vào:** 07.1 UI-B06 (banner "Thiếu 2 xref, 3 font, 1 ảnh — [Xem chi tiết] [Bỏ qua]"; nhóm Font, Xref, Ảnh, PDF underlay, Plot style, Shape); trạng thái phân giải xref DOC-013, font thiếu và font thay thế RGN-006, trạng thái nạp ảnh RGN-021, tìm CTB/STB PLT-002, đường dẫn tìm kiếm NT-011; prototype `index.html#UI-B06`, `js/screens1.js` `SC.missingRefs`, `TC.missingRefs.onOpen` trong `js/palettes.js`.
- **Đầu ra:** `app/dialogs/file/MissingReferenceScanner.{h,cpp}` (quét ở nền sau khi mở, trả danh sách theo nhóm: tên, đường dẫn đã lưu, trạng thái, gợi ý thay thế; theo dõi thay đổi tệp NT-011 để cập nhật); banner qua UIA-015; kiểm thử `app/dialogs/file/tests/missing_refs_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. 100 tệp của QA-001 có danh sách tham chiếu thiếu đã biết: số lượng và tên theo từng nhóm đúng 100%.
  2. Banner hiện ≤ 1 s **[đề xuất]** sau khung hình đầu; không chặn pan/zoom và lệnh; câu chữ theo 07.1 UI-B06, số lượng định dạng Việt Nam.
  3. Chép tệp xref còn thiếu vào thư mục tìm kiếm: banner tự giảm số đếm ≤ 2 s **[đề xuất]**, không cần mở lại.
  4. "Bỏ qua" ẩn banner cho tài liệu này trong phiên; không còn mục thiếu thì không hiện banner.
  5. Đạt DoD-U, DoD-L.

### UIB-013 · Bảng chi tiết tham chiếu thiếu và tìm tệp hàng loạt
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIB-012, UI-J02, DOC-013, NT-011, NT-010, NT-013, UI0-006, UI0-008
- **Màn hình:** UI-B06
- **Đầu vào:** 07.1 UI-B06 (mỗi mục: đường dẫn đã lưu, trạng thái, Tìm tệp…, Dùng font thay thế, Bỏ qua; "Tìm trong thư mục…" xử lý hàng loạt; "Nhớ lựa chọn cho các lần sau"; V: tìm để hiển thị); nạp xref chỉ đọc vào phiên DOC-013; prototype `js/screens1.js` `SC.missingRefs`.
- **Đầu ra:** `app/dialogs/file/MissingReferencesDialog.{h,cpp}` (bảng nhóm theo loại, ẩn nhóm rỗng; trạng thái có biểu tượng và chữ, không chỉ màu; "Dùng font thay thế" mở UI-J02; "Tìm trong thư mục…" quét đệ quy ở nền có tiến trình và hủy); lựa chọn nhớ theo hồ sơ (NT-010); ghi chú Viewer "Viewer tìm tệp để hiển thị; sửa đường dẫn và lưu vào bản vẽ cần TrionCAD Pro".
- **Tiêu chí nghiệm thu:**
  1. "Tìm trong thư mục…" trên cây 50.000 tệp tìm đủ mọi tệp thiếu có trong cây ≤ 2 s **[đề xuất]** (duyệt song song, máy tầm trung, SSD); có tiến trình và hủy ≤ 50 ms.
  2. Tìm thấy xref: xref nạp và hiện trên vùng vẽ trong phiên; banner UIB-012 giảm số đếm; tệp bản vẽ không bị đánh dấu chưa lưu.
  3. "Nhớ lựa chọn": mở lại cùng bản vẽ, font thay thế và mục bỏ qua áp dụng tự động, không hỏi lại.
  4. Bàn phím đủ: ↑/↓ chọn mục, Enter chạy hành động chính của mục, Esc đóng.
  5. Đạt DoD-U, DoD-L, DoD-H.

### UIB-014 · Sửa đường dẫn tham chiếu và lưu vào bản vẽ
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** UIB-013, CMD:XREF, UI0-015
- **Màn hình:** UI-B06
- **Đầu vào:** 07.1 UI-B06 ("EA: thêm sửa đường dẫn và lưu"); lệnh đổi đường dẫn của nhóm tham chiếu ngoài (xref; ảnh và underlay dùng lệnh cùng nhóm); đường dẫn tương đối NT-011; prototype `js/screens1.js` `SC.missingRefs` (nút `data-rf` find, folder).
- **Đầu ra:** trong `MissingReferencesDialog`: lựa chọn "Lưu đường dẫn mới vào bản vẽ" (tuyệt đối / tương đối / không lưu); áp dụng cả lô bằng một nhóm lệnh có một mark undo; kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Sửa 3 xref, 2 ảnh: một mark undo; Ctrl+Z trả cả 5 đường dẫn về cũ; lưu rồi mở lại không còn mục thiếu.
  2. Đường dẫn tương đối tính đúng so với thư mục bản vẽ (kể cả khác ổ đĩa: giữ tuyệt đối và báo lý do).
  3. Đạt DoD-U, DoD-L.

### UIB-015 · Trình phục hồi bản vẽ sau sự cố
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** DOC-019, PLF-007, UIB-001, UIB-005, CMD:DRAWINGRECOVERY, UI0-010
- **Màn hình:** UI-B07
- **Đầu vào:** 07.1 UI-B07 (tự hiện khi khởi động lại sau crash; lệnh `DRAWINGRECOVERY`; mỗi bản vẽ có tệp gốc, `.bak`, bản tự lưu kèm thời điểm và thumbnail; Mở, Lưu thành, Xóa bản sao lưu); autosave `.sv$` và `.bak` của DOC-019; dấu hiệu phiên trước crash của PLF-007; prototype `index.html#UI-B07`, `js/screens8.js` `SC.recovery`.
- **Đầu ra:** `app/dialogs/file/DrawingRecoveryPanel.{h,cpp}` (không modal: cây bản vẽ → phiên bản; chi tiết tệp, thư mục, giờ lưu, dung lượng, thumbnail qua UIB-001; Mở bản tự lưu kèm banner "Đang mở bản tự lưu lúc … Lưu để giữ lại; tệp gốc chưa thay đổi" có [Lưu] [Lưu thành…]; Xóa bản sao lưu có xác nhận); kịch bản `app/dialogs/file/tests/ui/crash_recovery.scenario`.
- **Tiêu chí nghiệm thu:**
  1. Kịch bản giả lập crash (giết tiến trình khi có 2 bản vẽ đã tự lưu): khởi động lại, bảng tự mở, liệt kê 2 bản vẽ với đủ phiên bản và giờ lưu.
  2. Mở bản tự lưu: nội dung trùng bản tự lưu (so DOC-017 từng đối tượng), tài liệu ở trạng thái chưa lưu, tệp gốc không đổi.
  3. Xóa bản sao lưu: hộp xác nhận nêu tên tệp và "Không khôi phục lại được"; sau khi xóa, danh sách cập nhật.
  4. Thumbnail tải ở nền ≤ 100 ms mỗi tệp **[đề xuất]**; mở bảng ≤ 150 ms.
  5. Đạt DoD-U, DoD-L.

## 2. Tiện ích bản vẽ (UI-B08, B09, B11, B12, B14)

### UIB-016 · Thuộc tính bản vẽ: chế độ xem
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** DWG-027, DOC-010, CMD:DWGPROPS, UI0-008
- **Màn hình:** UI-B08
- **Đầu vào:** 07.2 UI-B08 (tab Chung, Tóm tắt, Thống kê, Tùy biến; V: xem); SummaryInfo DWG-027 (gồm người lưu cuối); biến header TDCREATE, TDUPDATE, TDINDWG (DOC-010); prototype `index.html#UI-B08`, `js/screens1.js` `SC.dwgProps`, `js/screens8.js` `SC.dwgProps`.
- **Đầu ra:** `app/dialogs/util/DrawingPropertiesDialog.{h,cpp}` ở chế độ chỉ đọc: 4 tab, trường sao chép được; kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. 50 tệp của QA-001 có SummaryInfo (kể cả chuỗi tiếng Việt, tệp trước 2007 theo DWGCODEPAGE): mọi trường hiện đúng giá trị DWG-027 đọc được.
  2. Ngày dạng dd/mm/yyyy hh:mm; tổng thời gian sửa dạng "38 giờ 12 phút"; dung lượng định dạng Việt Nam.
  3. Tệp không có SummaryInfo: tab Tóm tắt, Tùy biến hiện trạng thái rỗng có giải thích, không lỗi.
  4. Đạt DoD-U, DoD-L.

### UIB-017 · Thuộc tính bản vẽ: sửa tóm tắt và thuộc tính tùy biến
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UIB-016, CMD:DWGPROPS, UI0-015
- **Màn hình:** UI-B08
- **Đầu vào:** 07.2 UI-B08 (tiêu đề, chủ đề, tác giả, từ khóa, ghi chú, gốc hyperlink; danh sách tên/giá trị dùng cho field trong khung tên); prototype `js/screens8.js` `SC.dwgProps` (bản Pro).
- **Đầu ra:** chế độ sửa của `DrawingPropertiesDialog`: ô nhập tab Tóm tắt, bảng Tùy biến (Thêm…, Xóa, sửa giá trị tại chỗ); nút xác nhận gửi một lệnh DWGPROPS (một mark undo).
- **Tiêu chí nghiệm thu:**
  1. Sửa 6 trường và 4 thuộc tính tùy biến rồi lưu DWG 2018 và DWG 2004: đọc lại bằng DWG-027 trùng 100%, kể cả chuỗi tiếng Việt.
  2. Tên thuộc tính tùy biến trống hoặc trùng (không phân biệt hoa thường): báo lỗi tại ô, không gửi lệnh.
  3. Hủy không gửi lệnh nào; xác nhận đánh dấu bản vẽ chưa lưu; Ctrl+Z trả mọi trường về cũ.
  4. Đạt DoD-U, DoD-L.

### UIB-018 · Dọn dẹp PURGE: phân tích, lý do không dọn được, ước tính dung lượng
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** CMD:PURGE, DOC-014, DOC-009, RND-021, NT-005, UI0-006, UI0-008, UI0-015
- **Màn hình:** UI-B09
- **Đầu vào:** 07.2 UI-B09 (hai chế độ: mục dọn được / không dọn được kèm lý do; cây 12 nhóm kể cả ứng dụng đăng ký (regapps); xác nhận từng mục; dọn mục lồng nhau; đối tượng không tên: hình học độ dài 0, text rỗng, dữ liệu mồ côi; Dọn mục đã chọn / Dọn tất cả; xem trước block; ước tính dung lượng giảm được); 03 §Tệp (`PURGE` P0, gồm regapps); phân tích tham chiếu DOC-014; prototype `index.html#UI-B09`, `js/screens8.js` `IMPL.PURGE`.
- **Đầu ra:** `app/dialogs/util/PurgeDialog.{h,cpp}`, `PurgeAnalysisModel` (chạy phân tích ở nền trên snapshot, cây ảo hóa có ô chọn theo nhóm và từng mục, lý do cho mục không dọn được); xem trước block bằng RND-021; dòng ước tính dung lượng; hộp xác nhận nêu số mục; toast có [Hoàn tác]; kịch bản và benchmark `app/dialogs/util/bench/purge_analysis_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. 200 bản vẽ của QA-001: tập mục dọn được trong hộp thoại trùng 100% với kết quả `-PURGE` của CMD:PURGE; mỗi mục không dọn được có lý do cụ thể ("Layer đang có 25 đối tượng", "Block lồng trong block khác", "Kiểu chữ hiện hành").
  2. Tệp 100 MB của BENCH-STD có 1.240 regapps: phân tích ≤ 1 s p99 **[đề xuất]**, 8 luồng, chạy nền, hủy ≤ 50 ms; luồng UI không bị chặn quá 2 ms.
  3. Tùy chọn xác nhận từng mục, dọn lồng nhau, đối tượng không tên chuyển đúng sang tùy chọn của PURGE; dọn lồng nhau lặp đến khi không còn mục.
  4. "Dọn 1.204 mục?" nêu số mục và vài tên; mọi mục dọn trong một lệnh, một mark undo; sau khi dọn, cây làm mới ≤ 200 ms **[đề xuất]**.
  5. Rê lên block: xem trước ≤ 100 ms **[đề xuất]**. Ước tính "giảm khoảng 1,8 MB" lệch ≤ 15% **[đề xuất]** so với dung lượng thật sau khi lưu, trên 50 tệp mẫu.
  6. Đạt DoD-U, DoD-L, DoD-H.

### UIB-019 · Đổi tên RENAME có wildcard
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD:RENAME, DOC-004, NT-012, UI0-006, UI0-015
- **Màn hình:** UI-B12
- **Đầu vào:** 07.2 UI-B12 (loại đối tượng có tên: Block, Kiểu kích thước, Layer, Linetype, Kiểu multileader, Kiểu bảng, Kiểu chữ, UCS, View, Viewport; tên cũ → tên mới có wildcard, ví dụ `KT-*` → `A-*`); bảng ký hiệu DOC-004; so sánh không phân biệt hoa thường NT-012; prototype `index.html#UI-B12`, `js/screens8.js` `IMPL.RENAME` (`mapName`).
- **Đầu ra:** `app/dialogs/util/RenameDialog.{h,cpp}`, `WildcardRenameMapper` (khớp `*`, `?`, thay theo thứ tự nhóm khớp); dòng xem trước "Sẽ đổi 12 mục, ví dụ KT-TUONG → A-TUONG"; tên cố định (0, Defpoints, Standard, Continuous, ByLayer, ByBlock) và tên phụ thuộc xref ghi "không đổi được"; kiểm thử `app/dialogs/util/tests/wildcard_rename_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. 10.000 ca ngẫu nhiên (property-based): ánh xạ trùng hàm tham chiếu; tên mới trùng tên có sẵn thì báo "Đã có …", không đổi mục nào.
  2. Một lần đổi = một lệnh RENAME cho cả lô, một mark undo; dòng lệnh ghi "Đã đổi tên 12 layer: …".
  3. Danh sách 10.000 layer ảo hóa; gõ tên cũ cập nhật tô sáng mục khớp ≤ 16 ms mỗi phím.
  4. Đạt DoD-U, DoD-L.

### UIB-020 · Đóng gói gửi đi ETRANSMIT: cây tệp, bảng tệp, thiết lập
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD:ETRANSMIT, UIB-013, NT-010, UI0-006, UI0-008
- **Màn hình:** UI-B11
- **Đầu vào:** 07.2 UI-B11 (tab Cây tệp / Bảng tệp: bản vẽ, xref, ảnh, font kể cả SHX tiếng Việt, plot style, PC3, liên kết dữ liệu; Thêm tệp; thiết lập Mới/Sửa: kiểu gói, định dạng, thư mục đích, đặt tên, cấu trúc đường dẫn, hành động, kèm theo; ghi chú); danh sách phụ thuộc do lệnh ETRANSMIT trả về; prototype `index.html#UI-B11`, `js/screens12.js` `IMPL.ETRANSMIT`, `txFiles`, `txEdit`.
- **Đầu ra:** `app/dialogs/util/TransmitDialog.{h,cpp}`, `TransmitSetupDialog`: cây và bảng cùng model, ô chọn từng tệp, trạng thái từng tệp (đưa vào gói, thiếu, không cần vì đã chuyển Unicode…), cảnh báo tệp thiếu có [Tìm tệp…] (UIB-013) hoặc [Bật] chuyển Unicode; thiết lập lưu theo hồ sơ (NT-010); kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. 30 bản vẽ mẫu có danh sách phụ thuộc đã biết: cây và bảng khớp 100% với ETRANSMIT.
  2. Bản vẽ có 50 xref, 200 ảnh: danh sách hiện ≤ 500 ms **[đề xuất]**, thu thập chạy nền, hộp thoại không bị chặn.
  3. Đổi thiết lập (bind xref, chuyển font Unicode, ZIP): trạng thái từng tệp và dòng tổng "N tệp · khoảng X MB sau khi nén" cập nhật ≤ 100 ms.
  4. Thiết lập mới: tên trống hoặc trùng, thư mục đích trống báo lỗi tại ô; thiết lập lưu lại và đọc lại đúng 100% trường.
  5. Đạt DoD-U, DoD-L.

### UIB-021 · Đóng gói gửi đi: chạy nền, tiến trình, báo cáo
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UIB-020, CMD:ETRANSMIT, NT-013, UI0-009
- **Màn hình:** UI-B11
- **Đầu vào:** 07.2 UI-B11 (báo cáo đóng gói xem và lưu; ô ghi chú); 07.1 §12; quy ước tác vụ nền của design/README (đóng hộp thoại vẫn chạy, tiến trình và nút hủy ở thanh trạng thái, toast kèm hành động); prototype `js/screens12.js` `start`, `txReport`.
- **Đầu ra:** chạy lệnh ETRANSMIT qua NT-013: tiến trình theo bước trong hộp thoại và trên thanh trạng thái, nút "Chạy ở nền", Hủy; toast khi xong có [Mở thư mục]; cửa sổ "Xem báo cáo" (nội dung, ghi chú, tệp thiếu, tệp không đưa vào gói).
- **Tiêu chí nghiệm thu:**
  1. Đóng hộp thoại khi đang chạy: tác vụ chạy tiếp, tiến trình còn ở thanh trạng thái; xong có toast.
  2. Hủy: dừng ≤ 1 s **[đề xuất]**, thư mục đích không có tệp nào của gói (so danh sách tệp trước và sau).
  3. Báo cáo trùng nội dung tệp báo cáo do ETRANSMIT ghi vào gói; ghi chú người dùng có trong báo cáo.
  4. Đạt DoD-U, DoD-L.

### UIB-022 · Xử lý tệp hàng loạt TCBATCH
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UI-J03, CMD:TCBATCH, NT-013, UI0-011
- **Màn hình:** UI-B14
- **Đầu vào:** 07.2 UI-B14 (dùng chung màn hình với UI-J03; thao tác theo thứ tự: kiểm tra và sửa lỗi, dọn dẹp kể cả regapps, chuyển font sang Unicode, chuyển phiên bản DWG 2018…2000 và DXF, đặt máy in của layout về "Không", chạy `.scr` (P2); đầu ra ghi đè có `.bak` / thư mục khác giữ cấu trúc / thêm hậu tố; chạy nền đa luồng, tạm dừng, hủy, báo cáo theo tệp, lưu thiết lập thành mẫu); prototype `index.html#UI-B14`, `js/screens4.js` `SC.fontBatch('batch')`, `js/screens12.js` `OPEN['UI-B14']`.
- **Đầu ra:** chế độ "batch" của màn hình UI-J03 trong `app/dialogs/util/BatchProcessPanel.{h,cpp}`: nhóm thao tác có thứ tự, ô chọn phiên bản, cột Phiên bản trong bảng tệp, mẫu thiết lập; ở Viewer Pro chỉ bật thao tác chuyển font, các thao tác khác có huy hiệu Pro; mục "Chạy script .scr" vô hiệu kèm "sau v1.0"; kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. 500 tệp của QA-001 (gồm thư mục con): bảng tiến trình cập nhật theo lô ≤ 10 lần/giây **[đề xuất]**, luồng UI không bị chặn quá 2 ms.
  2. Tạm dừng và tiếp tục giữ đúng vị trí; Hủy dừng sau tệp đang xử lý; tệp đã xong giữ nguyên kết quả.
  3. Báo cáo từng tệp (xong, cảnh báo, lỗi, bỏ qua) trùng nhật ký của TCBATCH; "Xem báo cáo" lọc được theo trạng thái.
  4. Mẫu thiết lập lưu và nạp lại đúng 100% lựa chọn.
  5. Đạt DoD-U, DoD-L.
- **Ngoài phạm vi:** chạy `.scr` trên nhiều bản vẽ (P2, chưa có thiết kế đợt 5); giao diện chuyển font (UI-J03).

## 3. Palette Thuộc tính (UI-C01, C02, C09)

### UIC-001 · Mô hình vùng chọn của palette Thuộc tính trên snapshot
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-009, DOC-017, INT-008, INT-005, RGN-013, NT-005, UI0-014
- **Màn hình:** UI-C01
- **Đầu vào:** 07.2 UI-C01 ("Hành vi": nhiều loại chỉ hiện thuộc tính chung, `*Khác nhau*` khi khác giá trị, trường tính toán chỉ đọc, quá 25.000 đối tượng hiện "Nhiều đối tượng — [Vẫn hiển thị]"; diện tích hatch cộng dồn); 07.1 §9.3 (palette hiện số lượng theo loại); README DoD-L dòng "Palette cập nhật khi vùng chọn đổi"; sự kiện vùng chọn INT-008; sysvar PROPOBJLIMIT (INT-005); metadata DOC-017; prototype `js/palettes.js` `P.props.render` (`types`, `vary`).
- **Đầu ra:** `app/palettes/properties/PropertySelectionModel.{h,cpp}`: vào (snapshot, tập ObjectId); ra: số lượng theo loại theo thứ tự cố định, danh sách mã thuộc tính chung cho "Tất cả" và cho từng loại, mỗi mã một giá trị hoặc cờ KHÁC NHAU, giá trị tổng hợp chỉ đọc (tổng độ dài line/polyline, tổng diện tích hatch). Cập nhật gia tăng theo sự kiện DOC-007 của đối tượng đang chọn; hủy khi vùng chọn đổi. Kiểm thử `app/palettes/properties/tests/property_model_test.cpp`, benchmark `app/palettes/properties/bench/property_model_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Vùng chọn ≤ 10.000 đối tượng: phần đồng bộ (dropdown loại có số lượng, trạng thái "đang tính") ≤ 16 ms; đủ giá trị ≤ 50 ms p99 **[đề xuất]**, máy tầm trung.
  2. 100.000 đối tượng 12 loại trong cảnh 1 triệu entity của BENCH-STD, sau "Vẫn hiển thị": tính đủ ≤ 150 ms p99 **[đề xuất]** với 8 luồng; hiệu suất song song ≥ 60% đến 8 luồng; luồng UI không bị chặn quá 2 ms.
  3. Vượt PROPOBJLIMIT (mặc định 25.000): chỉ tính số lượng theo loại, hiện "Nhiều đối tượng — [Vẫn hiển thị]".
  4. 10.000 vùng chọn ngẫu nhiên trên 50 bản vẽ của QA-001: kết quả trùng bộ tính tham chiếu một luồng; trùng từng bit với 1, 2, 8, 32 luồng.
  5. Đối tượng đang chọn bị sửa (sự kiện DOC-007): chỉ tính lại thuộc tính bị ảnh hưởng, ≤ 16 ms với vùng chọn ≤ 10.000.
  6. Đạt DoD-U, DoD-L, DoD-H.

### UIC-002 · Khung palette Thuộc tính và chế độ chỉ đọc của Viewer
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIC-001, CMD:PROPERTIES, UIA-017, UI0-010, UI0-006, UI0-011
- **Màn hình:** UI-C01
- **Đầu vào:** 07.2 UI-C01 (mở từ Ctrl+1, `PROPERTIES` (`PR`, `CH`), chuột phải › Thuộc tính, ribbon; dock phải, tự ẩn, nổi; đầu palette; nhóm thu gọn; ô mô tả ở đáy; ô lọc tên thuộc tính (P1); khi không chọn gì: thiết lập hiện hành Chung, View, Khác; V chỉ đọc); 07.3 UI-M03 (chỉ đọc nhưng sao chép được giá trị); design/README (palette Thuộc tính mở sẵn ở ≥ 1600 px); prototype `index.html#UI-C01`, `js/palettes.js` `P.props`, `groupHTML`, bảng mô tả `PD`.
- **Đầu ra:** `app/palettes/properties/PropertiesPalette.{h,cpp}`, `PropertyGridView` (cây ảo hóa hai cột, nhóm thu gọn nhớ theo loại); dropdown loại "Không chọn" / "Tất cả (125)" / "Line (80)"…; nút PICKADD (bật/tắt sysvar), Chọn đối tượng, Chọn nhanh (mở UIE-011, khóa Pro); ô lọc tên thuộc tính; ô mô tả (tên, giải thích, mã DXF hoặc sysvar từ DOC-017); trạng thái không chọn (CECOLOR, CLAYER, CELTYPE, CELTSCALE, CELWEIGHT, CETRANSPARENCY, THICKNESS; View: tâm, cao, rộng; Khác: CANNOSCALE, biểu tượng UCS, kiểu hiển thị); chế độ chỉ đọc; kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Ctrl+1 và `PROPERTIES` mở, đóng palette (alias `PR`, `CH` có khi PGP sẵn sàng); mở đến khung hình có nội dung ≤ 150 ms.
  2. Dropdown liệt kê đúng số lượng theo loại (định dạng 1.250); chọn một loại đổi nhóm thuộc tính ≤ 16 ms.
  3. Không chọn gì: hiện thiết lập hiện hành; pan/zoom cập nhật nhóm View tối đa 10 lần/giây **[đề xuất]**; đứng yên không tốn CPU (DoD-H 6).
  4. Viewer: không có ô sửa, ghi chú "Chỉ đọc trong Viewer. Bấm một giá trị để sao chép"; bấm giá trị đưa đúng chuỗi hiển thị vào clipboard. Trường chỉ đọc ở bản Pro cũng sao chép được.
  5. Ô mô tả hiện mã DXF và sysvar đúng metadata DOC-017 cho 100% thuộc tính của 20 loại entity lõi (DWG-011).
  6. Ô lọc tên lọc ≤ 16 ms mỗi phím; ↑/↓ di chuyển hàng, nhóm mở/đóng bằng ←/→.
  7. Đạt DoD-U, DoD-L.

### UIC-003 · Bố cục thuộc tính theo loại: đường cong, chữ, điểm
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIC-002, DOC-017, VN-002, RND-012, UI-J02
- **Màn hình:** UI-C01
- **Đầu vào:** 07.2 UI-C01 nhóm "Chung" và các dòng Line, Polyline, Arc, Circle, Ellipse, Spline, Point, Text, MText, Attribute definition, Xline/Ray (RO = chỉ đọc); bảng mã phát hiện theo đối tượng VN-002; overlay RND-012 để tô sáng đỉnh; prototype `js/palettes.js` hàm `geom`, `common`.
- **Đầu ra:** `app/palettes/properties/layouts/curves_text.json` (loại → nhóm → mã DOC-017 theo thứ tự hiển thị); widget `VertexNavigator` (◀ ▶ duyệt đỉnh polyline, điểm điều khiển/điểm đi qua của spline, tô sáng trên vùng vẽ); hàng `EncodingRow` (thẻ TCVN3/VNI/Unicode/Không rõ, nút "Chuyển sang Unicode"); công cụ kiểm CI: mọi mã có trong DOC-017, mọi thuộc tính hiển thị của loại có mặt đúng một lần.
- **Tiêu chí nghiệm thu:**
  1. Ảnh chụp từng loại khớp prototype; thứ tự, nhóm và cờ chỉ đọc đúng bảng 07.2 UI-C01.
  2. Giá trị hiển thị theo RGN-013 và UNITS của bản vẽ: 100% đúng trên 20 bản vẽ có LUNITS, AUNITS, LUPREC khác nhau.
  3. Polyline: ◀ ▶ duyệt đỉnh vòng tròn, đỉnh hiện tại tô sáng ≤ 16 ms; đổi vùng chọn thì về đỉnh 1.
  4. Text, MText mã TCVN3/VNI: thẻ bảng mã trùng 100% kết quả VN-002 trên bộ mẫu VN-003; nút chuyển ở Viewer mở UI-L04, từ Viewer Pro mở UI-J02.
  5. Đạt DoD-U, DoD-L.
- **Ngoài phạm vi:** sửa giá trị (UIC-005); sửa hyperlink (UI-F15, sau v1.0).

### UIC-004 · Bố cục thuộc tính theo loại: kích thước, đường dẫn, hatch, block, tham chiếu, viewport
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIC-002, DOC-017, DOC-011, DOC-012, DOC-013, DOC-020, GEO-021
- **Màn hình:** UI-C01
- **Đầu vào:** 07.2 UI-C01 các dòng Kích thước (Khác, Đường & mũi tên, Chữ, Fit, Đơn vị chính, Đơn vị phụ, Dung sai), Multileader, Leader cũ, Dung sai hình học, Hatch (diện tích cộng dồn), Block (attribute tag → giá trị), Xref, Ảnh raster, Viewport, Region/Wipeout/Solid 2D, Đa tuyến song song, Nhóm; diện tích vùng GEO-021; prototype `js/palettes.js` `geom` (`dim`, `hatch`, `insert`, `vpframe`, `image`), `attrRows`.
- **Đầu ra:** `app/palettes/properties/layouts/annot_blocks_refs.json`; hàng danh sách attribute của block; nhóm "Tổng hợp" khi chọn nhiều hatch; kiểm CI như UIC-003.
- **Tiêu chí nghiệm thu:**
  1. Ảnh chụp từng loại khớp prototype; nhóm thuộc tính kích thước dùng cùng tên nhóm với hộp Sửa kiểu kích thước (UID-004).
  2. Chọn nhiều hatch: diện tích cộng dồn bằng tổng GEO-021 (sai số tương đối ≤ 1e-9), có island.
  3. Block: attribute hiện theo thứ tự định nghĩa, giá trị đúng ATTRIB của từng tham chiếu; xref hiện "Tìm thấy tại" theo DOC-013.
  4. Viewport: tỉ lệ chuẩn hiện tên tỉ lệ khớp danh sách tỉ lệ của bản vẽ, tỉ lệ lạ hiện dạng số.
  5. Đạt DoD-U, DoD-L.

### UIC-005 · Trình sửa thuộc tính theo kiểu giá trị
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** UIC-003, UIC-004, UIC-017, UIC-018, UIC-019, UIE-002, CMD:PROPERTIES, UI-F01, UI0-007, UI0-015
- **Màn hình:** UI-C01, UI-C09
- **Đầu vào:** 07.2 UI-C09 (đơn vị, biểu thức, nút lấy từ bản vẽ, kiểm tra, `*Khác nhau*` gõ giá trị mới áp cho tất cả); 07.2 UI-C01 (sửa chu vi hoặc diện tích của circle đổi bán kính; nút "…" mở trình soạn MTEXT, gallery mẫu hatch; Block: dropdown để thay block; attribute sửa trực tiếp); 07.2 §0 (palette áp dụng ngay, mỗi lần sửa một bước hoàn tác); lệnh ghi thuộc tính chung của CMD:PROPERTIES (mã DOC-017 + giá trị); prototype `index.html#UI-C01` (bản Pro), `js/palettes.js` `TC.propEdit`, `propMenu`, `MENUK`, `NUMK`, `TXTK`.
- **Đầu ra:** `app/palettes/properties/PropertyEditorFactory.{h,cpp}`: chọn trình sửa theo kiểu metadata: số, độ dài, góc (UI0-007); chuỗi; liệt kê; Có/Không; màu (ByLayer, ByBlock, 1–9, "Chọn màu…" mở UIC-017); layer (ô màu, dấu khóa, tìm khi > 20 layer); linetype (UIC-018); lineweight (UIC-019); kiểu chữ, kiểu kích thước, kiểu multileader, kiểu bảng (kèm "Quản lý…"); tên block; giá trị attribute; nút "…" mở UI-F01 và UIE-002. Mỗi lần xác nhận gửi một lệnh qua INT-002. Kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Mỗi lần sửa gửi đúng một lệnh ghi thuộc tính: một mark undo tên "Sửa <thuộc tính>"; Ctrl+Z trả đúng giá trị cũ của mọi đối tượng.
  2. Đổi màu 10.000 đối tượng: từ Enter đến khung hình có màu mới ≤ 150 ms p99 **[đề xuất]** (lệnh + regen gia tăng); một đối tượng ≤ 30 ms **[đề xuất]**.
  3. Ô số nhận `=1200/2+50`, `12.5m`, `3'6"` theo UNITS; giá trị sai (bán kính âm) có viền lỗi và câu "Bán kính phải lớn hơn 0", không gửi lệnh. Nút ⌖ chuyển focus vào vùng vẽ, lấy điểm/khoảng cách/góc rồi trả giá trị.
  4. Vùng chọn có đối tượng trên layer khóa: các đối tượng đó không đổi, dòng lệnh báo "N đối tượng nằm trên layer bị khóa", đối tượng khác vẫn đổi.
  5. Circle: sửa chu vi hoặc diện tích đổi bán kính (sai số tương đối ≤ 1e-12); polyline: sửa X/Y của đỉnh hiện tại; block: đổi tên block qua dropdown; attribute: sửa giá trị tại chỗ.
  6. Nhật ký lệnh của 50 thao tác palette phát lại bằng `.scr` cho cùng kết quả (so DOC-017 đọc lại).
  7. F2/Enter mở sửa, Esc hủy, Tab sang hàng kế; tooltip có tên thuộc tính tiếng Anh và mã DXF.
  8. Đạt DoD-U, DoD-L.

### UIC-006 · Thuộc tính đủ loại ở bản 1.0
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIC-005, DOC-023, RGN-019, RGN-022, DWG-016, DWG-017, UI-F07, RGN-160
- **Màn hình:** UI-C01
- **Đầu vào:** 07.2 UI-C01 các dòng Bảng (khi chọn ô: kiểu ô, cao hàng, rộng cột, canh lề, nền, màu nội dung, viền, kiểu chữ, chiều cao chữ, kiểu dữ liệu và định dạng), Block "Tùy biến" của dynamic block (khoảng cách, lật, trạng thái hiển thị, tra bảng), Multileader có nội dung block, PDF underlay (Layer PDF "…"), đối tượng proxy và chưa hỗ trợ, 3D solid/surface/mesh; 07.2 UI-C01 "Phát hành: 1.0 (đủ loại)"; prototype `js/screens16.js` (nhóm "Tùy biến" trong palette Thuộc tính), `js/screens10.js` `TC.propsExt.pdfu`, `js/screens9.js` (thuộc tính ô bảng).
- **Đầu ra:** `app/palettes/properties/layouts/full_1_0.json`; hàng dropdown trạng thái hiển thị và tra bảng của dynamic block; hàng chỉ đọc cho proxy ("Chỉ xem. Dữ liệu được giữ nguyên khi lưu") và 3D (loại, hộp bao); nhóm thuộc tính ô khi đang chọn ô bảng (UI-F07).
- **Tiêu chí nghiệm thu:**
  1. Dynamic block mẫu (visibility, lookup, flip, linear): đổi giá trị trong palette cho hình đúng như EvalGraph của RGN-019, một lệnh, một mark undo.
  2. Chọn 1 ô và nhiều ô của bảng: thuộc tính ô hiện đúng, khác giá trị hiện `*Khác nhau*`, sửa áp cho mọi ô đã chọn.
  3. Proxy, 3D solid: chỉ đọc, không có trình sửa; lưu rồi mở lại dữ liệu trùng từng bit (DWG-016, DWG-017).
  4. Đạt DoD-U, DoD-L.

### UIC-007 · Thuộc tính nhanh
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIC-001, UIC-005, CMD:QUICKPROPERTIES, INT-018, UIA-009, UIA-014, NT-010
- **Màn hình:** UI-C02
- **Đầu vào:** 07.2 UI-C02 (popup 5–8 trường; trường mặc định theo loại; vị trí theo góc phần tư của con trỏ hoặc cố định; tự thu gọn; nút "…" mở palette đầy đủ; chọn trường theo loại là P2); 07.1 §9.10 (nhấp đúp đối tượng hình học cơ bản); sysvar QPMODE, QPLOCATION; prototype `index.html#UI-C02`, `js/screens14.js` `QP.show`, `QF`, `place`, `armCollapse`.
- **Đầu ra:** `app/palettes/properties/QuickPropertiesPopup.{h,cpp}` (dùng model UIC-001 và trình sửa UIC-005; dropdown loại; nút "…", thiết lập, đóng); nút bật/tắt trên thanh trạng thái (Ctrl+Shift+P); vị trí nhớ theo hồ sơ; kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. QPMODE bật: chọn đối tượng hiện popup đúng trường mặc định theo bảng 07.2 UI-C02 ≤ 50 ms **[đề xuất]**; nhiều loại thì chỉ Màu, Layer, Linetype kèm ghi chú chọn loại.
  2. Theo con trỏ: đặt ở góc phần tư trên phải, lật khi sát mép, không ra ngoài vùng vẽ; cố định: kéo thanh tiêu đề, vị trí nhớ sau khi khởi động lại.
  3. Không dùng 3 s thì thu gọn (prototype), rê vào mở lại; Esc đóng; "…" mở palette Thuộc tính và đóng popup.
  4. Nhấp đúp line, cung, tròn, ellipse, point, xline, ray mở popup; đối tượng trên layer khóa thì không.
  5. Sửa trong popup: một lệnh, một mark undo (như UIC-005).
  6. Đạt DoD-U, DoD-L.
- **Ngoài phạm vi:** chọn trường cho từng loại đối tượng (P2, chưa có thiết kế đợt 5).

## 4. Layer (UI-C03, C04, C05, C11)

### UIC-008 · Model danh sách layer ảo hóa cho 10.000 layer
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-004, DOC-009, DOC-013, NT-005, NT-012, UI0-006, UI0-014
- **Màn hình:** UI-C03
- **Đầu vào:** 07.2 UI-C03 (các cột; sắp xếp theo cột; kéo, ẩn, hiện cột; cột "Số đối tượng" tính ở nền; ô tìm kiếm có wildcard `KT-*`; chân "Hiện 48/312 layer · 3 đang chọn"); 07.2 §0 (danh sách dài có tìm kiếm và sắp xếp); bảng LAYER DOC-004; layer phụ thuộc xref DOC-013; so sánh có dấu NT-012; prototype `js/palettes.js` `layerRows`, `layerTable`, `lstate`.
- **Đầu ra:** `app/palettes/layers/LayerTableModel.{h,cpp}` (model Qt trên snapshot; cột theo 07.2; trạng thái hiện hành / đang dùng / không dùng; số đối tượng mỗi layer đếm song song ở nền và cập nhật gia tăng theo DOC-007); `LayerFilterProxy` (tìm: `*`, `?` và chứa chuỗi không phân biệt dấu; bộ lọc gốc Tất cả, Đang dùng, từng xref); quản lý cột (ẩn, hiện, kéo đổi thứ tự, độ rộng, nhớ theo hồ sơ); kiểm thử và benchmark `app/palettes/layers/bench/layer_model_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Bản vẽ 10.000 layer: mở palette đến khung hình có nội dung ≤ 150 ms; cuộn 60 fps, không khựng quá 1 khung; sắp xếp theo cột bất kỳ ≤ 30 ms **[đề xuất]**; mỗi phím trong ô tìm ≤ 16 ms.
  2. Cột Số đối tượng với cảnh 1 triệu entity của BENCH-STD: ≤ 200 ms p99 **[đề xuất]**, 8 luồng; lô 1.000 sự kiện thêm/xóa/đổi layer cập nhật ≤ 16 ms.
  3. Tìm `KT-*` khớp đúng tập layer theo wildcard; "tuong" khớp "TƯỜNG".
  4. Thứ tự sắp xếp ổn định, như nhau giữa các lần chạy và mọi số luồng.
  5. Đạt DoD-U, DoD-L, DoD-H.

### UIC-009 · Palette Quản lý layer: khung, cây bộ lọc gốc, chế độ xem của Viewer
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIC-008, CMD:LAYER, UIA-017, UI0-010, UI0-011
- **Màn hình:** UI-C03
- **Đầu vào:** 07.2 UI-C03 (palette không modal, dock được; thanh công cụ; cây bộ lọc ẩn được; V: chỉ bật/tắt và đóng băng tạm thời, ghi chú "Thay đổi không lưu vào tệp"); 07.3 UI-M04 ("Khôi phục như tệp gốc"); kho trạng thái layer tạm thời của Viewer (UIA-017); prototype `index.html#UI-C03`, `js/screens1.js` `SC.layerManager`, `js/palettes.js` `bindLayerTable`, `TC.layers`.
- **Đầu ra:** `app/palettes/layers/LayerManagerPalette.{h,cpp}`: thanh công cụ (nút thiếu quyền có huy hiệu Pro, bấm mở UI-L04), ô tìm, cây (Tất cả, Đang dùng, mỗi xref một nút có số layer), bảng UIC-008, chân palette; Viewer: chỉ cột Bật/Tắt, Đóng băng thao tác được, dòng đổi tạm có thẻ "tạm", nút "Khôi phục như tệp gốc"; menu chuột phải bản Viewer (Chọn đối tượng trên layer này, Cô lập layer này, Khôi phục như tệp gốc); kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. `LAYER` mở palette ≤ 150 ms; dock, nổi, tự ẩn theo UI0-010.
  2. Viewer, cảnh 1 triệu entity của BENCH-STD: bật/tắt hoặc đóng băng tạm thời cập nhật vùng vẽ ≤ 100 ms; không phát sự kiện thay đổi nào của DOC-007; bản vẽ không bị đánh dấu chưa lưu.
  3. "Khôi phục như tệp gốc" trả mọi layer về trạng thái trong tệp; nút chỉ hiện khi có thay đổi tạm.
  4. Nhấp đúp ở Viewer không đổi layer hiện hành, toast giải thích; mọi nút sửa khác mở UI-L04.
  5. Đạt DoD-U, DoD-L.

### UIC-010 · Sửa layer trực tiếp trong lưới
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIC-009, UIC-017, UIC-018, UIC-019, CMD:LAYER, CMD:RENAME, UI0-007, UI0-015
- **Màn hình:** UI-C03
- **Đầu vào:** 07.2 UI-C03 (sửa tên trực tiếp F2; Bật/Tắt, Đóng băng, Khóa, In; Màu mở UI-C06; Linetype, Độ dày nét mở bộ chọn; Độ trong suốt; Plot style chỉ với bản vẽ dùng STB; Đóng băng ở VP mới; Mô tả; chọn nhiều layer rồi sửa một ô là áp dụng cho tất cả); prototype `js/palettes.js` `TC.layerEdit`, `TC.layerProp`, `TC.layerRename`.
- **Đầu ra:** trình sửa ô trong `LayerManagerPalette`; kiểm tên (ký tự cấm `< > / \ " : ; ? * | , =` và dấu backtick, trùng tên, layer 0 và Defpoints không đổi tên); nhấp đúp đặt hiện hành; mỗi thao tác gửi một lệnh (-LAYER hoặc RENAME) qua INT-002; kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Mỗi thao tác là một lệnh, một mark undo; Ctrl+Z trả đúng; nhật ký lệnh phát lại bằng `.scr` cho cùng bảng layer.
  2. Chọn 500 layer rồi đổi màu: một lệnh; vùng vẽ cảnh 1 triệu entity cập nhật ≤ 300 ms **[đề xuất]**.
  3. Đóng băng layer hiện hành bị từ chối kèm thông báo; tên sai hoặc trùng báo lỗi tại ô, không gửi lệnh.
  4. Bàn phím: F2 đổi tên, Space bật/tắt ô toggle đang focus, Enter mở bộ chọn của ô màu/linetype/độ dày, Esc hủy.
  5. Đạt DoD-U, DoD-L.

### UIC-011 · Thanh công cụ layer, menu chuột phải, xóa có quy tắc, nạp bộ layer chuẩn, thiết lập cô lập
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIC-010, CMD:LAYER, CMD:PURGE, CMD:LAYISO, VN-008, INT-008, UI0-008
- **Màn hình:** UI-C03
- **Đầu vào:** 07.2 UI-C03 (Layer mới; Layer mới đóng băng ở mọi viewport; Xóa; Đặt hiện hành; Làm mới; Nạp bộ layer chuẩn (TCVN, theo bộ môn, từ DWG/DWT/`.las`; trùng tên thì chọn giữ hay ghi đè); không xóa được layer `0`, `Defpoints`, layer hiện hành, layer có đối tượng, layer của xref: giải thích lý do và gợi ý gộp; menu chuột phải; thiết lập cô lập `LAYISO`); bộ layer chuẩn VN-008; prototype `js/palettes.js` `TC.layerTool`.
- **Đầu ra:** hành động thanh công cụ và menu chuột phải trong `LayerManagerPalette`; hộp "Nạp bộ layer chuẩn" (`app/palettes/layers/LoadStandardLayersDialog.{h,cpp}`: nguồn, danh sách có ô chọn, xử lý trùng tên); hộp Thiết lập layer (khóa và làm mờ theo %, hoặc tắt; trong layout: đóng băng ở viewport hiện tại hoặc tắt); kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Xóa layer với 5 trường hợp cấm: mỗi trường hợp hiện đúng lý do; layer có đối tượng gợi ý "Gộp vào layer khác…" (bật ở UIC-013); layer rỗng xóa bằng một lệnh, một mark undo.
  2. Nạp bộ layer kiến trúc TCVN: thêm đúng số layer và thuộc tính như VN-008; trùng tên hỏi giữ hay ghi đè, áp theo lựa chọn; một mark undo.
  3. "Chọn đối tượng trên layer này" tạo vùng chọn đúng số đối tượng hiển thị của layer (so với đếm DOC); "Cô lập các layer đã chọn" chạy LAYISO theo thiết lập.
  4. Layer mới lấy thuộc tính của layer đang chọn và vào chế độ đổi tên ngay; Chọn tất cả / Bỏ chọn / Đảo chọn đúng trên danh sách đang lọc.
  5. Đạt DoD-U, DoD-L.

### UIC-012 · Cột viewport khi ở layout: Đóng băng VP, Đóng băng ở VP mới
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** UIC-010, CMD:VPLAYER, CMD:MVIEW, DOC-011
- **Màn hình:** UI-C03
- **Đầu vào:** 07.2 UI-C03 (khi đang ở layout: cột Đóng băng VP; EA có Đóng băng VP và Đóng băng ở VP mới, P0); 03 §10 (VP Freeze P0); viewport hiện hành DOC-011; prototype `js/palettes.js` `vpTgl`.
- **Đầu ra:** hai cột chỉ hiện ở tab layout; cột vô hiệu kèm tooltip "Kích hoạt một viewport để dùng cột này" khi chưa có viewport kích hoạt; thao tác gửi VPLAYER; kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Đóng băng VP chỉ đổi viewport hiện hành (kiểm danh sách layer đóng băng của từng viewport ở DOC-011); vùng vẽ cập nhật ≤ 100 ms.
  2. Đóng băng ở VP mới: viewport tạo sau đó (MVIEW) có layer đóng băng đúng.
  3. Về Model: hai cột ẩn ngay, không đổi độ rộng các cột khác.
  4. Đạt DoD-U, DoD-L.

### UIC-013 · Ghi đè thuộc tính layer theo viewport và gộp layer
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIC-011, UIC-012, CMD:VPLAYER, CMD:LAYMRG, RGN-002
- **Màn hình:** UI-C03
- **Đầu vào:** 07.2 UI-C03 (cột Màu VP, Linetype VP, Độ dày VP, Trong suốt VP, Plot style VP, hiện bằng màu nền khác; nút "Viewport có ghi đè" trên cây; menu Gỡ ghi đè viewport, Gộp các layer đã chọn vào…; 1.0: ghi đè theo viewport); 03 §10 (ghi đè theo viewport P1); phân giải ghi đè RGN-002; prototype chỉ có cột Đóng băng VP (`js/palettes.js` `layerTable`, biến `vpc`), phần ghi đè còn lại theo 07.2.
- **Đầu ra:** năm cột ghi đè trong layout (ô có ghi đè: nền token riêng và biểu tượng, không chỉ dựa vào màu); nút cây "Viewport có ghi đè"; "Gỡ ghi đè viewport"; hộp gộp layer (layer đích, nêu số đối tượng sẽ chuyển); kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Đặt màu VP cho một layer: chỉ viewport hiện hành vẽ màu mới (kiểm pixel bằng QA-011), viewport khác giữ màu cũ; giá trị ô khớp RGN-002.
  2. Gỡ ghi đè trả về thuộc tính gốc; nút cây "Viewport có ghi đè" lọc đúng các layer có ghi đè.
  3. Gộp 3 layer có 10.000 đối tượng: hộp xác nhận nêu số đối tượng; một lệnh LAYMRG ≤ 300 ms **[đề xuất]**; Ctrl+Z trả cả 3 layer và đối tượng.
  4. Đạt DoD-U, DoD-L.

### UIC-014 · Trạng thái layer LAYERSTATE
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIC-009, CMD:LAYERSTATE, NT-011, QA-007, UI0-008
- **Màn hình:** UI-C04
- **Đầu vào:** 07.2 UI-C04 (danh sách tên, không gian, mô tả; Mới, Cập nhật, Sửa, Đổi tên, Xóa, Nhập `.las`/`.dwg`, Xuất `.las`, Khôi phục; tùy chọn không liệt kê trạng thái của xref, tắt layer không có trong trạng thái, áp dụng như ghi đè viewport; 11 ô chọn thuộc tính được khôi phục); 03 §10 (`LAYERSTATE` P1, `.las`); prototype `index.html#UI-C04`, `js/screens14.js` `SC.layerStates`.
- **Đầu ra:** `app/palettes/layers/LayerStatesDialog.{h,cpp}`, hộp Trạng thái mới, hộp Sửa trạng thái (lưới layer của trạng thái); mục "Lưu trạng thái layer…", "Khôi phục trạng thái" ở menu chuột phải của UIC-009; kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Khôi phục một trạng thái: một lệnh, một mark undo; chỉ các thuộc tính đã chọn bị đổi (kiểm 11 thuộc tính × 2 giá trị).
  2. Xuất `.las` rồi nhập lại cho cùng trạng thái 100%; nhập 20 tệp `.las` mẫu do AutoCAD tạo (QA-007) đúng mọi thuộc tính.
  3. 500 trạng thái: danh sách ảo hóa, tìm ≤ 16 ms mỗi phím; Xóa hỏi xác nhận nêu tên.
  4. Đạt DoD-U, DoD-L.

### UIC-015 · Bộ lọc layer theo thuộc tính và theo nhóm
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIC-009, CMD:LAYER, INT-009, UIA-006
- **Màn hình:** UI-C05, UI-C03
- **Đầu vào:** 07.2 UI-C05 (bộ lọc thuộc tính: tên, lưới điều kiện mỗi dòng một điều kiện, cột giống cột layer, tên có wildcard, xem trước layer khớp; bộ lọc nhóm: kéo layer vào nhóm, "Chọn layer từ bản vẽ"); 07.2 UI-C03 (Bộ lọc mới, Bộ lọc nhóm mới, Đảo bộ lọc; áp bộ lọc đang chọn lên dropdown layer trên ribbon); bộ lọc lưu trong bản vẽ; prototype `index.html#UI-C05`, `js/screens14.js` `SC.layerFilterEdit`, `filters`, `inFilter`.
- **Đầu ra:** `app/palettes/layers/LayerFilterDialog.{h,cpp}`; nút bộ lọc người dùng trên cây (lồng nhau, đổi tên, xóa); kéo-thả layer vào bộ lọc nhóm; "Đảo bộ lọc"; tùy chọn áp lên dropdown layer của ribbon (UIA-006); kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. 1.000 điều kiện ngẫu nhiên: tập layer khớp trùng hàm tham chiếu; xem trước cập nhật ≤ 16 ms mỗi lần sửa với 10.000 layer.
  2. 20 DWG có bộ lọc do AutoCAD tạo: hiện đủ trên cây, áp đúng; sửa rồi lưu, mở lại vẫn đúng.
  3. "Chọn layer từ bản vẽ": ẩn hộp thoại, chọn đối tượng, layer của chúng vào nhóm.
  4. Tạo, sửa, xóa bộ lọc đi qua lệnh, một mark undo mỗi lần.
  5. Đạt DoD-U, DoD-L.
- **Ngoài phạm vi:** nút "Chưa đối chiếu" và LAYEREVAL (P2, chưa có thiết kế đợt 5).

### UIC-016 · Duyệt layer LAYWALK
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIC-008, UIC-015, CMD:LAYWALK, CMD:PURGE, RND-013, UI0-010
- **Màn hình:** UI-C11
- **Đầu vào:** 07.2 UI-C11 (chọn một hay nhiều layer thì vùng vẽ chỉ hiện đối tượng của các layer đó; ô lọc wildcard; bộ lọc có sẵn; nhấp đúp để giữ một layer luôn hiện; nút Dọn xóa layer rỗng đang chọn; "Khôi phục trạng thái khi đóng" mặc định bật); prototype `index.html#UI-C11`, `js/screens14.js` `SC.layWalk`.
- **Đầu ra:** `app/palettes/layers/LayerWalkDialog.{h,cpp}` (không modal, danh sách ảo hóa đa chọn Ctrl/Shift, ↑/↓, Ctrl+A; dấu `*` cho layer luôn hiện; chân "Đang hiện N layer"); hiển thị lọc tạm qua cờ hiển thị của renderer, không đổi bản vẽ; kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Đổi lựa chọn layer: vùng vẽ cảnh 1 triệu entity cập nhật ≤ 100 ms; không phát sự kiện DOC-007 nào.
  2. Gõ wildcard tự chọn mọi layer khớp; nhấp đúp bật/tắt "luôn hiện".
  3. Dọn: xác nhận nêu số và tên layer rỗng; xóa bằng một lệnh, một mark undo.
  4. Đóng khi bật "Khôi phục trạng thái khi đóng": bản vẽ không đổi. Khi tắt: một lệnh đặt bật/tắt layer theo lựa chọn, một mark undo; layer hiện hành luôn bật.
  5. Đạt DoD-U, DoD-L.

## 5. Bộ chọn dùng chung (UI-C06, C07, C08, C10)

### UIC-017 · Bộ chọn màu
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-002, INT-009, NT-010, UI0-007, UI0-008
- **Màn hình:** UI-C06
- **Đầu vào:** 07.2 UI-C06 (tab Màu chỉ số: bảng 255 màu gồm dải 1–9, dải xám 250–255, toàn bảng 10–249, ByLayer, ByBlock, ô số màu, rê hiện số và RGB; tab True Color: HSL hoặc RGB, ô số, mã hex; tab Color Book: P2; màu dùng gần đây; ô so sánh cũ/mới; lấy màu từ đối tượng (P1)); 03 §10 (`COLOR` P0 ACI + True Color, Color Books P2); bảng ACI → RGB của RGN-002; prototype `index.html#UI-C06`, `js/screens5.js` `SC.colorPicker`.
- **Đầu ra:** `app/dialogs/pickers/ColorPickerDialog.{h,cpp}` dùng chung (trả ByLayer, ByBlock, ACI hoặc true color); chế độ chỉ True Color cho màu giao diện vùng vẽ (UID-020, mốc V); nút lấy màu từ bản vẽ (ẩn hộp thoại, bấm đối tượng qua INT-009, quay lại); hàng 8 màu gần đây theo hồ sơ (NT-010); móc UI để lệnh COLOR mở hộp và nhận kết quả; kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Bảng ACI: 255/255 ô đúng RGB của RGN-002; rê hiện "Chỉ số màu N (tên) · RGB r, g, b"; nhấp đúp chọn và đóng.
  2. True Color: đổi qua lại HSL ↔ RGB ↔ hex không lệch quá 1 đơn vị mỗi kênh.
  3. Ô "Màu" nhận số 1–255, tên tiếng Việt và tiếng Anh (đỏ, red…), ByLayer, ByBlock, `#RRGGBB`; giá trị sai có viền lỗi.
  4. Lấy màu từ đối tượng: màu ByLayer trả về ByLayer, màu cụ thể trả đúng màu; Esc quay lại không đổi.
  5. Tab Color Book hiện trạng thái "Có sau v1.0"; đối tượng dùng màu Color Book vẫn hiện tên màu ở ô.
  6. Mở hộp ≤ 150 ms; bàn phím: mũi tên di chuyển trong bảng màu, Enter chọn.
  7. Đạt DoD-U, DoD-L.

### UIC-018 · Quản lý linetype, hộp Tải linetype, bộ chọn linetype dùng chung
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UID-001, RGN-011, CMD:LINETYPE, VN-008, NT-011, INT-005, UI0-007
- **Màn hình:** UI-C07
- **Đầu vào:** 07.2 UI-C07 (bộ lọc Tất cả / Đang dùng / Phụ thuộc xref, Đảo bộ lọc; Tải…, Xóa, Hiện hành, Ẩn/Hiện chi tiết; danh sách tên, hình mẫu, mô tả; chi tiết LTSCALE, CELTSCALE, bề rộng nét ISO, PSLTSCALE, MSLTSCALE; hộp Tải: `trion.lin`, `trionISO.lin`, `TCVN.lin` hoặc duyệt, có ô chọn và tìm; bộ chọn dùng chung là dropdown có hình mẫu và mục "Khác…"); 03 §10 (`LINETYPE` P0, linetype phức có shape/text); `TCVN.lin` của VN-008; prototype `index.html#UI-C07`, `js/screens5.js` `SC.linetype`, `SC.loadLinetypes`.
- **Đầu ra:** `app/dialogs/pickers/LinetypeManagerDialog.{h,cpp}`, `LoadLinetypeDialog`, `LinetypeCombo` (dùng ở Thuộc tính, Layer, ribbon); hình mẫu vẽ bằng RGN-011 qua UID-001, tạo ở nền và cache; kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Nút Xóa vô hiệu với ByLayer, ByBlock, Continuous và linetype đang dùng, tooltip nêu lý do.
  2. Hình mẫu linetype phức (shape, text) trùng hình RGN-011 vẽ trong bản vẽ thật; 500 linetype cuộn 60 fps.
  3. Tải N linetype từ một tệp: một lệnh, một mark undo; linetype đã có hiện sẵn dấu chọn và khóa; tệp `.lin` lỗi báo số dòng lỗi, không crash.
  4. Chi tiết LTSCALE, CELTSCALE, PSLTSCALE, MSLTSCALE ghi bằng một lệnh khi xác nhận; vùng vẽ cập nhật theo.
  5. `LinetypeCombo`: ByLayer, ByBlock, linetype đã tải có hình mẫu, "Khác…" mở hộp quản lý; mở dropdown ≤ 100 ms.
  6. Đạt DoD-U, DoD-L.

### UIC-019 · Thiết lập lineweight và bộ chọn lineweight dùng chung
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD:LWEIGHT, INT-005, UI0-008
- **Màn hình:** UI-C08
- **Đầu vào:** 07.2 UI-C08 (ByLayer, ByBlock, Mặc định, 0,00–2,11 mm; đơn vị mm hoặc inch; hiển thị lineweight; độ dày mặc định; thanh trượt tỉ lệ hiển thị); sysvar LWDISPLAY, LWDEFAULT, LWUNITS, CELWEIGHT; prototype `index.html#UI-C08`, `js/screens5.js` `SC.lweight`, `SC.LWS`.
- **Đầu ra:** `app/dialogs/pickers/LineweightDialog.{h,cpp}`, `LineweightCombo` (dùng ở Thuộc tính, Layer, ribbon); ô xem trước độ dày theo tỉ lệ hiển thị.
- **Tiêu chí nghiệm thu:**
  1. Danh sách đúng 24 giá trị chuẩn cộng ByLayer, ByBlock, Mặc định; inch hiện 3 chữ số thập phân, mm theo định dạng Việt Nam (0,25 mm).
  2. Xác nhận gửi một lệnh đặt LWDISPLAY, LWDEFAULT, LWUNITS, CELWEIGHT; bật hiển thị lineweight đổi vùng vẽ trong một khung hình.
  3. Đạt DoD-U, DoD-L.

### UIC-020 · Thiết lập Khớp thuộc tính
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD:MATCHPROP, NT-010, UI0-008
- **Màn hình:** UI-C10
- **Đầu vào:** 07.2 UI-C10 (mở từ tùy chọn *Thiết lập* của `MATCHPROP`; thuộc tính cơ bản: Màu, Layer, Linetype, Tỉ lệ linetype, Độ dày nét, Độ trong suốt, Độ dày đùn, Plot style; đặc biệt: Kích thước, Chữ, Hatch, Polyline, Viewport, Bảng, Multileader, Đường tâm); prototype `index.html#UI-C10`, `js/screens5.js` `SC.matchSettings`.
- **Đầu ra:** `app/dialogs/pickers/MatchPropSettingsDialog.{h,cpp}` (16 ô chọn, giá trị của đối tượng nguồn cạnh từng thuộc tính cơ bản, Chọn tất cả / Bỏ tất cả); lưu theo hồ sơ; trả quyền về prompt của MATCHPROP.
- **Tiêu chí nghiệm thu:**
  1. 16 ca (mỗi ô một ca): MATCHPROP chỉ chép đúng thuộc tính được chọn (so DOC-017 trước và sau).
  2. Thiết lập còn sau khi khởi động lại; Esc quay về prompt "Chọn đối tượng đích" không đổi thiết lập.
  3. Đạt DoD-U, DoD-L.

## 6. Kiểu chú thích và kiểu in (UI-D01–D07)

### UID-001 · Xem trước kiểu bằng regen thật trên tài liệu mẫu tạm
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-007, RGN-001, RGN-003, RND-021, DWG-024, NT-005
- **Màn hình:** UI-D01, UI-D02, UI-D03, UI-D04, UI-D06, UI-D07, UI-C07
- **Đầu vào:** 07.2 §0 ("Xem trước trực tiếp trên vùng vẽ khi có thể: hatch, mảng, kiểu chữ, kích thước"); 07.2 UI-D02 ("mỗi tab đều có xem trước"); API regen thuần RGN-001, bộ điều phối RGN-003; render ngoài màn hình RND-021; prototype `js/screens5.js` `dimPreview`, `ltPaint`, hàm `pv` của từng hộp kiểu.
- **Đầu ra:** `app/dialogs/style/StylePreviewRenderer.{h,cpp}`: tài liệu mẫu tạm dựng từ mẫu DXF trong `app/dialogs/style/preview_templates/` (câu chữ "Bản vẽ kỹ thuật ÂĂÊÔƠƯĐ 0123"; kích thước thẳng 1200, bán kính và đường kính Ø520, góc, tọa độ điểm, cung; multileader; bảng; đa tuyến song song; đoạn thẳng cho linetype; lưới điểm); `render(kind, styleValues, size, theme) → future<QImage>`, chỉ giữ yêu cầu mới nhất; nền theo token vùng vẽ Tối/Sáng; kiểm thử `app/dialogs/style/tests/style_preview_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Ảnh xem trước trùng từng pixel ảnh RND-021 vẽ cùng entity trong một bản vẽ thật dùng cùng kiểu (30 cặp kiểu mẫu): chứng minh dùng regen thật.
  2. Đổi một trường → ảnh mới ≤ 33 ms p99 **[đề xuất]** máy tầm trung, ≤ 66 ms máy thấp; gõ liên tục chỉ render bản mới nhất; luồng UI không bị chặn quá 2 ms.
  3. Tài liệu mẫu không xuất hiện trong danh sách bản vẽ (DOC-018), không phát sự kiện lên bản vẽ người dùng; đóng hộp thoại giải phóng hết (bộ nhớ trước và sau mở/đóng 100 lần lệch ≤ 1 MB).
  4. DPI 100%, 150%, 200%: ảnh sắc nét theo DPI thật của màn hình.
  5. Đạt DoD-U, DoD-L.

### UID-002 · Kiểu chữ STYLE
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UID-001, CMD:STYLE, RGN-006, VN-002, VN-007, UI-J02, UI0-007
- **Màn hình:** UI-D01
- **Đầu vào:** 07.2 UI-D01 (danh sách kiểu lọc Tất cả / Đang dùng; xem trước câu mẫu tiếng Việt; nhóm Font: tên font có biểu tượng SHX/TTF và tìm kiếm, kiểu font, Big Font; Kích thước: chú thích theo tỉ lệ, khớp hướng với layout, chiều cao hoặc chiều cao trên giấy; Hiệu ứng: lộn ngược, viết ngược, viết dọc (chỉ SHX), hệ số rộng, góc nghiêng; nhóm Bảng mã và gợi ý font Unicode tương đương; nút Đặt hiện hành, Mới…, Xóa, Áp dụng, Đóng); 03 §Chú thích (`STYLE` P0); FontResolver RGN-006; bộ font SHX tiếng Việt VN-007; prototype `index.html#UI-D01`, `js/screens5.js` `SC.textStyle`.
- **Đầu ra:** `app/dialogs/style/TextStyleDialog.{h,cpp}`; dropdown font nạp ở nền từ đường dẫn hỗ trợ và font hệ thống; nhóm Bảng mã (thẻ TCVN3/VNI/Unicode/Không rõ, cảnh báo font mã cũ, nút "Dùng font Unicode tương đương", liên kết UI-J02); kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Xem trước câu mẫu bằng UID-001 đúng font SHX, TTF, bigfont và hiệu ứng; đổi trường cập nhật ≤ 33 ms.
  2. Danh sách 2.000 font nạp ở nền ≤ 300 ms **[đề xuất]**; tìm theo tên ≤ 16 ms mỗi phím; SHX và TTF phân biệt bằng nhãn chữ, không chỉ bằng màu.
  3. Big Font và Viết dọc chỉ bật với SHX; chọn Chú thích theo tỉ lệ đổi nhãn sang "Chiều cao trên giấy".
  4. Font `.VnTime`, `VNI-Times`: thẻ bảng mã trùng VN-002, gợi ý font Unicode theo bảng của VN-007; nút đổi font của kiểu hoạt động.
  5. Áp dụng: một lệnh STYLE, một mark undo; 10.000 text dùng kiểu cập nhật hình ≤ 200 ms **[đề xuất]**; Xóa vô hiệu với kiểu đang dùng, Standard, kiểu hiện hành (tooltip nêu số đối tượng dùng).
  6. Đạt DoD-U, DoD-L.

### UID-003 · Quản lý kiểu kích thước: cây kiểu, xem trước, so sánh
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UID-001, CMD:DIMSTYLE, DOC-017, VN-008, UI0-008
- **Màn hình:** UI-D02
- **Đầu vào:** 07.2 UI-D02 (hộp quản lý: danh sách lọc Tất cả / Đang dùng, không liệt kê kiểu của xref; xem trước, mô tả; Đặt hiện hành; Mới… (tên, bắt đầu từ, chú thích theo tỉ lệ, dùng cho: Tất cả, Thẳng, Góc, Bán kính, Đường kính, Tọa độ, Đường dẫn); Sửa…; Ghi đè…; So sánh; kiểu mẫu theo TCVN); 03 §Kích thước (`DIMSTYLE`, `DIMOVERRIDE` P0 lõi); prototype `index.html#UI-D02`, `js/screens5.js` `SC.dimStyle`, `SC.dimStyleNew`, `SC.dimStyleCompare`.
- **Đầu ra:** `app/dialogs/style/DimStyleManagerDialog.{h,cpp}` (cây: kiểu cha và kiểu con theo "Dùng cho", nút `<ghi đè>` dưới kiểu hiện hành), hộp Tạo kiểu mới, hộp So sánh (thuộc tính, tên DIMVAR, giá trị hai kiểu, sao chép bảng); kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Cây đúng quan hệ cha con của bảng DIMSTYLE (50 bản vẽ của QA-001); kiểu của xref không hiện; kiểu TCVN (TCVN-1-100, TCVN-1-50…) có sẵn khi bản vẽ tạo từ mẫu VN-008.
  2. So sánh: danh sách khác biệt trùng 100% phép so từng DIMVAR (DOC-017) trên 50 cặp kiểu; hai kiểu giống nhau báo "Hai kiểu giống nhau".
  3. Mới…: tên trống, trùng hoặc có ký tự cấm báo lỗi tại ô; tạo xong mở hộp Sửa (UID-004).
  4. Đặt hiện hành: một lệnh, một mark undo; dropdown kiểu kích thước trên ribbon đổi theo.
  5. Đạt DoD-U, DoD-L.

### UID-004 · Hộp Sửa kiểu kích thước: khung 7 tab, ghi đè, tab Đường và Ký hiệu & mũi tên
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UID-003, UID-001, RGN-014, RGN-015, DOC-017, UI0-007
- **Màn hình:** UI-D02
- **Đầu vào:** 07.2 UI-D02 bảng 7 tab, dòng **Đường** (đường dim: màu, linetype, độ dày, vượt đường gióng, khoảng cách cùng gốc, ẩn dim 1/2; đường gióng: màu, linetype gióng 1/2, độ dày, ẩn gióng 1/2, vượt đường dim, khoảng cách từ gốc, độ dài cố định) và **Ký hiệu & mũi tên** (mũi tên thứ nhất, thứ hai, đường dẫn gồm gạch nghiêng kiến trúc và block người dùng; cỡ mũi tên; dấu tâm; cỡ ngắt dim; ký hiệu độ dài cung; góc gãy bán kính; hệ số chiều cao gãy dim thẳng); "Phát hành: EA (Đường, Ký hiệu, Chữ, Fit, Đơn vị chính) · 1.0 (đủ 7 tab)"; prototype `js/screens5.js` `SC.dimStyleEdit`.
- **Đầu ra:** `app/dialogs/style/DimStyleEditDialog.{h,cpp}` (7 tab, xem trước UID-001 cạnh tab; tab Đơn vị phụ và Dung sai hiện vô hiệu kèm "Có từ v1.0" đến khi có UID-007); bảng ánh xạ trường → DIMVAR sinh từ metadata DOC-017 của đối tượng DIMSTYLE; chế độ Ghi đè (lưu thành ghi đè của kiểu hiện hành); hai tab Đường, Ký hiệu & mũi tên; kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Mỗi trường ánh xạ đúng một DIMVAR (kiểm tự động trên toàn bộ trường của hai tab); ghi rồi đọc lại qua DOC-017 trùng giá trị.
  2. Đổi trường → xem trước ≤ 33 ms (UID-001); 30 tổ hợp mũi tên và đường gióng cho hình trùng RGN-014/RGN-015 trên bản vẽ thật.
  3. Xác nhận: một lệnh DIMSTYLE (hoặc ghi đè), một mark undo; kích thước đang dùng kiểu cập nhật; Hủy không gửi lệnh.
  4. Mũi tên "Block người dùng…": chọn block trong bản vẽ, block không tồn tại báo lỗi tại ô.
  5. Đạt DoD-U, DoD-L.

### UID-005 · Kiểu kích thước: tab Chữ và Fit
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UID-004
- **Màn hình:** UI-D02
- **Đầu vào:** 07.2 UI-D02 dòng **Chữ** (kiểu chữ, màu chữ, màu nền, chiều cao, tỉ lệ chiều cao phân số, khung chữ; vị trí dọc giữa / trên / ngoài / JIS / dưới; ngang giữa / tại gióng 1/2 / trên gióng 1/2; hướng nhìn; khoảng cách tới đường dim; canh nằm ngang / theo đường dim / ISO) và **Fit** (ưu tiên chữ hay mũi tên, luôn giữ chữ giữa đường gióng, bỏ mũi tên nếu không vừa; vị trí chữ khi bị dời; tỉ lệ chú thích theo tỉ lệ / theo layout / tỉ lệ tổng thể; đặt chữ thủ công, luôn vẽ đường dim giữa đường gióng); prototype `js/screens5.js` `SC.dimStyleEdit` (pane `text`, `fit`).
- **Đầu ra:** hai tab Chữ và Fit trong `DimStyleEditDialog`, ánh xạ DIMTXSTY, DIMCLRT, DIMTFILL, DIMTFILLCLR, DIMTXT, DIMTFAC, DIMTAD, DIMJUST, DIMTXTDIRECTION, DIMGAP, DIMTIH, DIMTOH, DIMATFIT, DIMTIX, DIMSOXD, DIMTMOVE, DIMSCALE, DIMUPT, DIMTOFL.
- **Tiêu chí nghiệm thu:**
  1. Kiểm ánh xạ tự động cho mọi trường của hai tab; tổ hợp không hợp lệ (ví dụ chú thích theo tỉ lệ cùng tỉ lệ tổng thể) vô hiệu đúng như 07.2.
  2. 40 tổ hợp Fit và vị trí chữ: xem trước trùng hình RGN-014 trên bản vẽ thật.
  3. Đạt DoD-U, DoD-L.

### UID-006 · Kiểu kích thước: tab Đơn vị chính
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** UID-004, RGN-013
- **Màn hình:** UI-D02
- **Đầu vào:** 07.2 UI-D02 dòng **Đơn vị chính** (định dạng; độ chính xác; định dạng phân số; dấu thập phân; làm tròn; tiền tố; hậu tố; hệ số tỉ lệ đo, chỉ áp dụng cho layout; bỏ số 0 đầu/cuối; đơn vị phụ của số 0 (hệ số, hậu tố); góc: định dạng, độ chính xác, bỏ số 0); 05 §1.2 (chuỗi text kích thước 100% trùng bộ tham chiếu); thư viện định dạng RGN-013; prototype `js/screens5.js` `SC.dimStyleEdit` (pane `unit`).
- **Đầu ra:** tab Đơn vị chính trong `DimStyleEditDialog`, ánh xạ DIMLUNIT, DIMDEC, DIMFRAC, DIMDSEP, DIMRND, DIMPOST, DIMLFAC, DIMZIN, DIMMZF, DIMMZS, DIMAUNIT, DIMADEC, DIMAZIN; dòng ví dụ giá trị trong xem trước.
- **Tiêu chí nghiệm thu:**
  1. 200 tổ hợp định dạng, độ chính xác, làm tròn, bỏ số 0: chuỗi trong xem trước trùng từng ký tự chuỗi RGN-013 sinh cho kích thước thật; khi có QA-007, trùng 100% bộ tham chiếu.
  2. Đạt DoD-U, DoD-L.

### UID-007 · Kiểu kích thước: tab Đơn vị phụ và Dung sai
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UID-004, RGN-013, RGN-014
- **Màn hình:** UI-D02
- **Đầu vào:** 07.2 UI-D02 dòng **Đơn vị phụ** (hiện đơn vị phụ; định dạng; độ chính xác; hệ số nhân; làm tròn; tiền tố/hậu tố; bỏ số 0; vị trí sau hoặc dưới giá trị chính) và **Dung sai** (phương pháp không / đối xứng / sai lệch / giới hạn / cơ bản; độ chính xác; giá trị trên/dưới; tỉ lệ chiều cao; vị trí dọc; canh; bỏ số 0; dung sai cho đơn vị phụ); 03 §Kích thước (P1 đầy đủ: dung sai, đơn vị phụ); prototype `js/screens5.js` `SC.dimStyleEdit` (pane `alt`, `tol`, đang ghi "có từ v1.0").
- **Đầu ra:** hai tab trong `DimStyleEditDialog` (bỏ trạng thái vô hiệu của UID-004), ánh xạ DIMALT, DIMALTU, DIMALTD, DIMALTF, DIMALTRND, DIMAPOST, DIMALTZ, DIMTOL, DIMLIM, DIMTDEC, DIMTP, DIMTM, DIMTFAC, DIMTOLJ, DIMTALN, DIMTZIN, DIMALTTD, DIMALTTZ; trường phụ thuộc phương pháp dung sai bật/tắt đúng.
- **Tiêu chí nghiệm thu:**
  1. 200 tổ hợp đơn vị phụ và dung sai: chuỗi xem trước trùng từng ký tự RGN-013/RGN-014 trên kích thước thật; có QA-007 thì trùng 100%.
  2. Kiểm ánh xạ tự động mọi trường; đổi phương pháp dung sai chỉ bật các trường hợp lệ.
  3. Đạt DoD-U, DoD-L.

### UID-008 · Kiểu multileader MLEADERSTYLE
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UID-001, CMD:MLEADERSTYLE, RGN-016, VN-008, UI0-007
- **Màn hình:** UI-D03
- **Đầu vào:** 07.2 UI-D03 (hộp quản lý: danh sách, xem trước, Hiện hành, Mới, Sửa, Xóa; hộp Sửa 3 tab: Định dạng đường dẫn (kiểu thẳng / spline / không, màu, linetype, độ dày, mũi tên và cỡ, cỡ ngắt), Cấu trúc (số điểm tối đa, góc đoạn 1/2, đoạn ngang tự thêm và độ dài, tỉ lệ), Nội dung (MText / Block / Không; với chữ và với block, có block TCVN)); 03 §Chú thích (multileader P0 tạo và sửa); block TCVN của VN-008; prototype `index.html#UI-D03`, `js/screens5.js` `SC.mleaderStyle`.
- **Đầu ra:** `app/dialogs/style/MLeaderStyleDialog.{h,cpp}`, `MLeaderStyleEditDialog` (đổi loại nội dung thì đổi nhóm trường hiện), xem trước UID-001; kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Mỗi trường ghi đúng thuộc tính của MLEADERSTYLE (kiểm tự động qua DOC-017); xem trước trùng hình RGN-016 trên bản vẽ thật ở 20 tổ hợp.
  2. Nội dung Block: chọn được block TCVN (ký hiệu chi tiết, số hiệu) và block của bản vẽ; cách gắn, màu, tỉ lệ ghi đúng.
  3. Xóa vô hiệu với Standard và kiểu đang dùng; xác nhận một lệnh, một mark undo; multileader dùng kiểu cập nhật.
  4. Đạt DoD-U, DoD-L.

### UID-009 · Kiểu bảng TABLESTYLE
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UID-001, CMD:TABLESTYLE, RGN-017, VN-008, INT-009, UI0-007, RGN-166
- **Màn hình:** UI-D04
- **Đầu vào:** 07.2 UI-D04 (bảng khởi đầu; hướng xuống / lên; kiểu ô Tiêu đề / Đầu cột / Dữ liệu và kiểu tùy biến; mỗi kiểu ô có tab Chung (màu nền, canh lề, định dạng, loại, lề ngang/dọc, gộp ô), Chữ (kiểu, chiều cao, màu, góc), Viền (độ dày, linetype, màu, đường đôi, khoảng cách, nút chọn cạnh); xem trước; kiểu bảng mẫu theo TCVN); 03 §Chú thích (`TABLESTYLE` P1); prototype `index.html#UI-D04`, `js/screens9.js` hộp `tbstyle`, `tbsedit`.
- **Đầu ra:** `app/dialogs/style/TableStyleDialog.{h,cpp}`, `TableStyleEditDialog` (chọn bảng khởi đầu trên bản vẽ qua INT-009; quản lý kiểu ô: tạo, đổi tên, xóa); xem trước UID-001; kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Kiểu TCVN "thống kê thép" và "diện tích" có sẵn khi bản vẽ tạo từ mẫu VN-008; xem trước trùng hình RGN-017.
  2. Nút cạnh viền đổi đúng cạnh (trên, dưới, trái, phải, trong, ngoài, tất cả) ở cả ba kiểu ô; kiểm ánh xạ tự động mọi trường.
  3. Xác nhận: một lệnh, một mark undo; bảng dùng kiểu cập nhật.
  4. Đạt DoD-U, DoD-L.

### UID-010 · Quản lý plot style
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** PLT-002, CMD:STYLESMANAGER, NT-011, VN-008, UI0-008
- **Màn hình:** UI-D05
- **Đầu vào:** 07.2 UI-D05 (thư mục các tệp CTB/STB, trình hướng dẫn thêm mới, mở để sửa; CTB mẫu theo TCVN: màu → độ dày nét theo TCVN 8-20; V: chọn CTB khi in, EA: sửa); 03 §Layout (`STYLESMANAGER` P0); đọc/ghi CTB, STB PLT-002; prototype `index.html#UI-D05`, `js/screens5.js` `SC.plotStyles`.
- **Đầu ra:** `app/dialogs/style/PlotStyleManagerDialog.{h,cpp}` (danh sách tệp: tên, loại theo màu / theo tên, mô tả; cập nhật khi thư mục đổi qua NT-011; Thêm plot style…: bắt đầu từ đầu hoặc từ tệp có sẵn; Mở để sửa; Mở thư mục); kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Chép một tệp CTB vào thư mục Plot Styles: danh sách cập nhật ≤ 1 s **[đề xuất]**, không cần mở lại.
  2. Thêm mới từ `TCVN.ctb`: tệp mới trùng nội dung nguồn trừ tên và mô tả.
  3. Tệp CTB hỏng: hiện dòng lỗi có lý do, không chặn các tệp khác.
  4. Đạt DoD-U, DoD-L.
- **Ngoài phạm vi:** chọn CTB trong hộp In (UI-H02).

### UID-011 · Trình sửa plot style CTB và STB
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UID-010, PLT-002, QA-007, UI0-006, UI0-007
- **Màn hình:** UI-D05
- **Đầu vào:** 07.2 UI-D05 (tab Chung: mô tả, thông tin tệp, áp hệ số tỉ lệ cho linetype không phải ISO; tab Bảng và tab Biểu mẫu là hai cách xem cùng dữ liệu; thuộc tính mỗi kiểu: màu in, dither, thang xám, số bút, bút ảo, screening 0–100%, linetype, thích ứng, độ dày nét, đầu nét, mối nối, kiểu tô, mô tả; Thêm/Xóa kiểu (STB); Sửa danh sách độ dày; Lưu thành; chọn nhiều màu để sửa một lần); prototype `js/screens5.js` `SC.ctbEdit`.
- **Đầu ra:** `app/dialogs/style/PlotStyleEditor.{h,cpp}` (model chung cho hai tab; danh sách 255 màu cho CTB, danh sách tên cho STB với kiểu Normal không xóa được; chọn nhiều bằng Ctrl/Shift, giá trị khác nhau hiện `*Khác nhau*`); hộp danh sách độ dày; ghi tệp qua PLT-002 (tệp ngoài, không phải bản vẽ).
- **Tiêu chí nghiệm thu:**
  1. Chọn màu 1–7, đặt độ dày 0,25 mm: cả 7 kiểu đổi; tab Bảng và tab Biểu mẫu luôn cùng giá trị.
  2. Mở rồi lưu không sửa: tệp trùng từng byte với 20 CTB/STB mẫu do AutoCAD tạo (QA-007); sửa rồi lưu, đọc lại bằng PLT-002 trùng 100% trường.
  3. Screening ngoài 0–100, số bút ngoài 1–255: viền lỗi, không lưu.
  4. Đạt DoD-U, DoD-L.

### UID-012 · Chuyển đổi CONVERTCTB và CONVERTPSTYLES
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UID-010, PLT-013, PLT-003, QA-005, UI0-008
- **Màn hình:** UI-D05
- **Đầu vào:** 07.2 UI-D05 ("Chuyển đổi (1.0): tạo STB từ CTB (`CONVERTCTB`); chuyển bản vẽ giữa kiểu in theo màu và theo tên (`CONVERTPSTYLES`), có cảnh báo trước"); engine và lệnh PLT-013; prototype chưa có hộp riêng, đặt cạnh `js/screens5.js` `SC.plotStyles`.
- **Đầu ra:** mục "Chuyển đổi" trong `PlotStyleManagerDialog`: hộp chọn CTB nguồn và tên STB đích; hộp cảnh báo trước CONVERTPSTYLES nêu hậu quả (gán plot style theo tên cho layer và đối tượng, chọn tệp STB) và số layer bị ảnh hưởng.
- **Tiêu chí nghiệm thu:**
  1. CONVERTCTB từ `TCVN.ctb` rồi CONVERTPSTYLES bản vẽ mẫu sang STB vừa tạo: bản in trước và sau chuyển trùng nhau, SSIM ≥ 0,999 **[đề xuất]** (QA-005).
  2. CONVERTPSTYLES: cảnh báo nêu đúng số layer; xác nhận chạy một lệnh, một mark undo; Hủy không đổi gì.
  3. Đạt DoD-U, DoD-L.

### UID-013 · Kiểu điểm DDPTYPE
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** UID-001, CMD:DDPTYPE, RGN-023, UI0-007
- **Màn hình:** UI-D06
- **Đầu vào:** 07.2 UI-D06 (lưới 20 kiểu điểm; cỡ điểm; theo % màn hình hoặc đơn vị tuyệt đối); 03 §Vẽ (`DDPTYPE` P0); vẽ POINT theo PDMODE/PDSIZE của RGN-023; prototype `index.html#UI-D06`, `js/screens5.js` `SC.pointStyle`.
- **Đầu ra:** `app/dialogs/style/PointStyleDialog.{h,cpp}`: 20 ô (PDMODE 0, 1, 2, 3, 4, 32–36, 64–68, 96–100) vẽ từ primitive RGN-023 qua UID-001, nhóm radio có tên đọc được; ô cỡ (UI0-007) và lựa chọn tương đối/tuyệt đối (dấu của PDSIZE).
- **Tiêu chí nghiệm thu:**
  1. Hình 20 ô trùng hình POINT vẽ trên vùng vẽ với PDMODE tương ứng.
  2. Xác nhận: một lệnh đặt PDMODE, PDSIZE, một mark undo; điểm trên bản vẽ cập nhật.
  3. Mũi tên di chuyển trong lưới, Space/Enter chọn.
  4. Đạt DoD-U, DoD-L.

### UID-014 · Kiểu đa tuyến song song MLSTYLE
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UID-001, CMD:MLSTYLE, RGN-023, VN-008, NT-011, UI0-007
- **Màn hình:** UI-D07
- **Đầu vào:** 07.2 UI-D07 (danh sách: Hiện hành, Mới, Sửa, Đổi tên, Xóa, Tải, Lưu, xem trước; hộp Sửa: mô tả, nắp đầu/cuối (đường thẳng, cung ngoài, cung trong, góc), màu tô, hiện mối nối, thành phần (độ lệch, màu, linetype), Thêm/Xóa; mẫu tường 110, 220 theo TCVN); 03 §Vẽ (`MLSTYLE` P1); prototype `index.html#UI-D07`, `js/screens5.js` `SC.mlStyle`.
- **Đầu ra:** `app/dialogs/style/MLineStyleDialog.{h,cpp}`, `MLineStyleEditDialog` (thành phần sắp theo độ lệch); Tải/Lưu tệp `.mln`; xem trước UID-001.
- **Tiêu chí nghiệm thu:**
  1. Tải `acad.mln` mẫu do AutoCAD tạo và `.mln` TCVN của VN-008: đủ kiểu, đủ thành phần; lưu rồi tải lại trùng 100%.
  2. Xem trước trùng hình MLINE của RGN-023 ở 10 kiểu mẫu.
  3. Sửa kiểu đang dùng bị chặn kèm lý do (giống AutoCAD); kiểu chưa dùng: một lệnh, một mark undo.
  4. Đạt DoD-U, DoD-L.

## 7. Thiết lập bản vẽ và môi trường (UI-D08–D14)

### UID-015 · Đơn vị bản vẽ UNITS
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD:UNITS, RGN-013, INT-009, UI0-007, UI0-008
- **Màn hình:** UI-D08
- **Đầu vào:** 07.2 UI-D08 (độ dài: Kiến trúc, Thập phân, Kỹ thuật, Phân số, Khoa học, độ chính xác; góc: Độ thập phân, Độ/Phút/Giây, Grad, Radian, Trắc địa, độ chính xác, chiều kim đồng hồ; tỉ lệ chèn; dòng ví dụ kết quả; nút "Hướng…" với Đông / Bắc / Tây / Nam / Khác, chỉ trên bản vẽ được; hệ đo MEASUREMENT); 03 §Bản vẽ (`UNITS` P0, `MEASUREMENT` P1); prototype `index.html#UI-D08`, `js/screens5.js` `SC.units`.
- **Đầu ra:** `app/dialogs/settings/UnitsDialog.{h,cpp}`, hộp Hướng góc gốc (chỉ góc trên bản vẽ qua INT-009); xác nhận gửi một lệnh đặt LUNITS, LUPREC, AUNITS, AUPREC, ANGDIR, ANGBASE, INSUNITS, MEASUREMENT.
- **Tiêu chí nghiệm thu:**
  1. Dòng ví dụ trùng từng ký tự chuỗi RGN-013 cho mọi tổ hợp loại × độ chính xác (độ dài 5 × 9, góc 5 × 9).
  2. Danh sách độ chính xác đổi theo loại; chọn lại giữ độ chính xác gần nhất hợp lệ.
  3. Xác nhận: một lệnh, một mark undo; thanh trạng thái, Thuộc tính, DYN hiện số theo đơn vị mới ngay sau đó.
  4. Đạt DoD-U, DoD-L.

### UID-016 · Thiết lập hỗ trợ vẽ: khung 6 tab, Snap & Lưới, Theo dõi polar
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-011, INT-005, UIA-009, UI0-007, UI0-008
- **Màn hình:** UI-D09
- **Đầu vào:** 07.2 UI-D09 bảng tab **Snap & Lưới** (bật snap; khoảng snap X/Y, bằng nhau; khoảng polar; kiểu snap lưới chữ nhật / isometric / polar; bật lưới; kiểu lưới chấm hay đường cho Model / Block Editor / Paper; khoảng lưới; đường chính sau N ô; lưới thích ứng; cho chia nhỏ; hiện ngoài giới hạn; theo UCS động) và **Theo dõi polar** (bật; góc tăng; góc bổ sung; theo dõi chỉ trực giao hay mọi góc polar; đo góc tuyệt đối hay tương đối); "Phát hành: EA (Snap & Lưới, Polar, Bắt điểm, Nhập động) · 1.0"; lệnh DSETTINGS thuộc INT (README §4.2) gọi hộp này qua móc UI với tham số tab; prototype `index.html#UI-D09`, `js/screens5.js` `SC.dsettings`.
- **Đầu ra:** `app/dialogs/settings/DraftingSettingsDialog.{h,cpp}` (6 tab; tab Thuộc tính nhanh, Chọn luân phiên vô hiệu kèm "Có từ v1.0" đến UID-018; nút "Tùy chọn…" mở tab Vẽ của UI-D10); tab Snap & Lưới, Theo dõi polar ghi SNAPMODE, SNAPUNIT, SNAPTYPE, SNAPSTYL, POLARDIST, GRIDMODE, GRIDUNIT, GRIDMAJOR, GRIDDISPLAY, GRIDSTYLE, POLARMODE, POLARANG, POLARADDANG.
- **Tiêu chí nghiệm thu:**
  1. Kiểm ánh xạ tự động: mỗi trường ↔ một sysvar (hoặc bit của sysvar mã bit) của INT-005, giá trị đọc lại trùng.
  2. Xác nhận: mọi sysvar đổi trong một nhóm lệnh SETVAR, một mark undo; biến theo viewport chỉ đổi viewport hiện hành; Hủy không đổi gì.
  3. Góc bổ sung: thêm, xóa, tối đa 10 góc như AutoCAD; giá trị ngoài 0–360 báo lỗi tại ô.
  4. Nút trạng thái F9, F10 phản ánh giá trị mới trong cùng khung hình sau khi đóng hộp.
  5. Đạt DoD-U, DoD-L.

### UID-017 · Thiết lập hỗ trợ vẽ: tab Bắt điểm và Nhập động
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UID-016, INT-010, INT-014, RND-012, CMD:LINE
- **Màn hình:** UI-D09
- **Đầu vào:** 07.2 UI-D09 tab **Bắt điểm** (bật bắt điểm; bật theo dõi bắt điểm; 14 chế độ của 07.1 §9.6; Chọn tất cả; Bỏ tất cả; Tùy chọn…) và **Nhập động** (nhập con trỏ: cực hay Đề-các, tương đối hay tuyệt đối, khi nào hiện; nhập kích thước: 1 hay 2 ô, các ô hiện đồng thời; prompt động; giao diện tooltip màu, cỡ, độ trong suốt); 07.1 §9.7; prototype `js/screens5.js` `SC.dsettings` (`K`, `mk`).
- **Đầu ra:** hai tab trong `DraftingSettingsDialog`; biểu tượng 14 marker vẽ bằng cùng hình marker trên vùng vẽ (RND-012); ô xem trước tooltip DYN; ghi OSMODE, AUTOSNAP, DYNMODE, DYNPICOORDS, DYNPIFORMAT, DYNPIVIS, DYNDIVIS, DYNPROMPT, DYNTOOLTIPS, DYNINFOTIPS.
- **Tiêu chí nghiệm thu:**
  1. 1.000 tổ hợp ô chế độ bắt điểm ngẫu nhiên: OSMODE ghi ra trùng bảng bit tham chiếu; mở lại hộp hiện đúng tổ hợp.
  2. Biểu tượng marker trong hộp trùng pixel với marker vẽ trên vùng vẽ ở 100% và 200%.
  3. Đổi thiết lập DYN: ô nhập động trên vùng vẽ đổi đúng chế độ ngay khi đóng hộp (kịch bản QA-011 với LINE).
  4. Đạt DoD-U, DoD-L.

### UID-018 · Thiết lập hỗ trợ vẽ: tab Thuộc tính nhanh và Chọn luân phiên
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UID-016, UIC-007, INT-021
- **Màn hình:** UI-D09
- **Đầu vào:** 07.2 UI-D09 tab **Thuộc tính nhanh** (hiện khi chọn; áp dụng cho mọi loại hay chỉ loại đã tùy biến; vị trí theo con trỏ: góc phần tư + khoảng cách / cố định; tự thu gọn; số dòng tối thiểu) và **Chọn luân phiên** (cho phép; hiện danh sách; vị trí; hiện thanh tiêu đề); chọn luân phiên INT-021; prototype `js/screens5.js` `SC.dsettings` (pane `qp`, `cyc`), `js/screens14.js` `qpMenu`.
- **Đầu ra:** hai tab trong `DraftingSettingsDialog` (bỏ trạng thái vô hiệu), ghi QPMODE, QPLOCATION, SELECTIONCYCLING và thiết lập vị trí của UIC-007.
- **Tiêu chí nghiệm thu:**
  1. Mỗi lựa chọn đổi đúng hành vi của popup UIC-007 và danh sách chọn luân phiên (kịch bản QA-011 cho từng lựa chọn).
  2. Đạt DoD-U, DoD-L.

### UID-019 · Khung hộp thoại Tùy chọn và tìm thiết lập
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD:OPTIONS, NT-010, INT-005, UI0-008, UI0-011, UI0-015
- **Màn hình:** UI-D10
- **Đầu vào:** 07.2 UI-D10 (mở từ `OPTIONS` (`OP`), menu ứng dụng, chuột phải trên vùng vẽ; hộp thoại có tab và ô tìm kiếm thiết lập, gõ "autosave" nhảy tới đúng ô; 12 tab; "Phát hành: V (Hiển thị, Hệ thống, Hồ sơ, Tài khoản) · EA (gần đủ) · 1.0"); kho cài đặt NT-010 (phạm vi máy / người dùng / hồ sơ); sysvar theo phạm vi INT-005; prototype `index.html#UI-D10`, `js/screens2.js` `SC.options`, `TC.SETTINGS_INDEX`, `OPT_TABS`.
- **Đầu ra:** `app/dialogs/options/OptionsDialog.{h,cpp}`: danh sách tab dọc có biểu tượng khóa cho tab bị cổng; `SettingsIndex` (mỗi hàng thiết lập có khóa tìm tiếng Việt, tiếng Anh, tên sysvar; tìm không phân biệt dấu); đầu hộp: hồ sơ hiện hành và bản vẽ hiện hành; thiết lập lưu trong bản vẽ có biểu tượng bản vẽ **[đề xuất]**; nút Đặt lại mặc định (xác nhận), Hủy, Áp dụng, Lưu và đóng; mở theo tham số tab và hàng.
- **Tiêu chí nghiệm thu:**
  1. Tìm "autosave", "tự lưu", "SAVETIME": nhảy đúng tab Mở & Lưu và tô sáng hàng ≤ 16 ms mỗi phím; kiểm CI: 100% hàng có mục chỉ mục.
  2. Hủy trả mọi thay đổi đã xem trước (kể cả chủ đề) về như trước khi mở; Áp dụng ghi NT-010 nguyên tử (ngắt tiến trình giữa chừng không hỏng hồ sơ).
  3. Thiết lập trong bản vẽ ghi bằng một nhóm lệnh SETVAR, một mark undo; thiết lập hồ sơ không tạo bước undo.
  4. Viewer: tab Hiển thị, Hệ thống, Dòng lệnh, Hồ sơ, Tài khoản dùng được; tab khác hiện trạng thái khóa có nút "Dùng thử Pro 30 ngày".
  5. Mở hộp ≤ 150 ms; Ctrl+Tab/Ctrl+Shift+Tab đổi tab; Enter là Lưu và đóng, Esc là Hủy.
  6. Đạt DoD-U, DoD-L.

### UID-020 · Tùy chọn: tab Hiển thị và hộp Màu vùng vẽ
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UID-019, UIC-017, UI0-002, UI0-012, RND-020, INT-005
- **Màn hình:** UI-D10
- **Đầu vào:** 07.2 UI-D10 dòng **Hiển thị** (chủ đề Tối/Sáng; Giao diện nhẹ; cỡ biểu tượng ribbon; tooltip; hiện tab tệp; Màu… theo ngữ cảnh Model, Layout, Block Editor, dòng lệnh, xem trước và theo thành phần nền, chữ thập, lưới chính/phụ/trục, đường theo dõi, marker bắt điểm, tooltip; font dòng lệnh; thành phần layout; độ mịn cung tròn; số đoạn của polyline cong; hiệu năng; cỡ chữ thập; độ mờ xref và đối tượng đang sửa tại chỗ); 07.1 §13.2 (màu ngữ nghĩa người dùng chỉnh được ở UI-D10); prototype `js/screens2.js` nhánh `Hiển thị`, `SC.colors`.
- **Đầu ra:** tab Hiển thị trong `OptionsDialog`; `app/dialogs/options/CanvasColorsDialog.{h,cpp}` (ngữ cảnh × thành phần, ô xem trước, khôi phục mặc định); ghi VIEWRES, SPLINESEGS, FILLMODE, QTEXTMODE, IMAGEFRAME, CURSORSIZE, XDWGFADECTL, XFADECTL và token màu người dùng (hồ sơ).
- **Tiêu chí nghiệm thu:**
  1. Đổi chủ đề, mật độ, Giao diện nhẹ: giao diện đổi ≤ 100 ms, không khởi động lại.
  2. Đổi màu nền Model, chữ thập, marker: vùng vẽ đổi trong khung hình kế tiếp; màu chọn có tương phản dưới 3:1 với nền hiện cảnh báo **[đề xuất]**.
  3. Kiểm ánh xạ tự động mọi hàng ↔ sysvar hoặc khóa hồ sơ; giá trị ngoài miền (VIEWRES 1–20000, CURSORSIZE 1–100) báo lỗi tại ô.
  4. Đạt DoD-U, DoD-L.

### UID-021 · Tùy chọn: tab Hệ thống, Dòng lệnh, Hồ sơ, Tài khoản
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UID-019, NT-010, INT-017, PLF-006, PLF-009, UI-L09, UI-L10, UI-L18
- **Màn hình:** UI-D10
- **Đầu vào:** 07.2 UI-D10 dòng **Hệ thống** (Đồ họa mở UI-L10; thiết bị trỏ; khi chuyển layout; khôi phục các "không hỏi lại"; tiếng bíp; cập nhật kênh ổn định/beta và tự kiểm tra; liên kết tệp `.dwg`/`.dxf`/`.dwt` (UI-L18); quyền riêng tư (UI-L09); ngôn ngữ giao diện và ngôn ngữ prompt), **Hồ sơ** (danh sách, đặt hiện hành, thêm, đổi tên, xóa, xuất/nhập, đặt lại), **Tài khoản**; 07.1 §8 thiết lập dòng lệnh (số dòng lịch sử, độ trong suốt khi nổi, độ trễ tự hoàn thành, tìm sysvar, tìm theo tiếng Việt, tìm giữa chuỗi, bỏ qua bộ gõ khi chờ tên lệnh, AutoCorrect); prototype `js/screens2.js` nhánh `Hệ thống`, `Dòng lệnh`, `Hồ sơ`, `Tài khoản`.
- **Đầu ra:** bốn tab trong `OptionsDialog`; ghi LAYOUTREGENCTL, INPUTSEARCHOPTIONS và các khóa hồ sơ; danh sách AutoCorrect (xem, xóa) đọc từ INT-017; xuất/nhập hồ sơ dạng của NT-010; nút "Trạng thái giấy phép…" gọi hành động đã đăng ký qua UI0-015 (UI-L03 đăng ký ở VP).
- **Tiêu chí nghiệm thu:**
  1. Đổi ngôn ngữ giao diện vi ↔ en: mọi cửa sổ đang mở đổi chữ ≤ 1 s **[đề xuất]**, không khởi động lại; đổi ngôn ngữ prompt: prompt và chip dòng lệnh đổi ở prompt kế tiếp.
  2. Khôi phục "không hỏi lại": nút nêu số hộp đã ẩn; sau khi khôi phục, hộp đó hiện lại.
  3. Hồ sơ: xuất rồi nhập trên máy sạch trùng 100% khóa; xóa hồ sơ hiện hành bị chặn kèm lý do; đặt hiện hành áp ngay.
  4. Kênh cập nhật và liên kết tệp gọi đúng API PLF-006, PLF-009; thiết lập dòng lệnh đổi hành vi tự hoàn thành ngay ở lần gõ kế tiếp.
  5. Đạt DoD-U, DoD-L.

### UID-022 · Tùy chọn: tab Tệp, Mở & Lưu, In & Xuất
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** UID-019, UIB-004, NT-011, DOC-019, RGN-006, PLT-001, PLT-002, UI-J04, UI-K03
- **Màn hình:** UI-D10
- **Đầu vào:** 07.2 UI-D10 dòng **Tệp** (đường dẫn tìm tệp hỗ trợ sắp xếp, thêm, xóa; thư mục trusted (UI-K03); đường dẫn dự án; tệp tùy biến; font thay thế mặc định và bảng ánh xạ font (liên kết Bác sĩ Font); tệp từ điển; đường dẫn máy in, plot style, PC3/PMP; thư mục tự lưu; mẫu mặc định cho `QNEW`; thư mục tool palette; thư mục tạm; nhật ký), **Mở & Lưu** (định dạng lưu mặc định; lưu thumbnail; tự lưu mỗi N phút, bản `.bak`, nhật ký; số tệp gần đây; mở lại bản vẽ của phiên trước; đường dẫn đầy đủ trên tiêu đề; xref; đối tượng proxy), **In & Xuất** (máy in mặc định; dùng thiết lập in thành công gần nhất; thư mục in ra tệp; in và xuất bản ở nền; nhật ký in; giữ khổ giấy; ẩn máy in hệ thống; điểm gốc offset; dấu in; kiểu plot style mặc định và CTB mặc định); prototype `js/screens8.js` `SC.optTab`.
- **Đầu ra:** ba tab trong `OptionsDialog`; trình sửa danh sách đường dẫn dùng chung (thêm, xóa, lên, xuống, duyệt, biến môi trường `%USERPROFILE%`, đường dẫn không tồn tại có thẻ cảnh báo bằng chữ); ghi SAVETIME, ISAVEBAK, SAVEFILEPATH, XLOADCTL, VISRETAIN, XEDIT, PROXYSHOW, PROXYNOTICE, BACKGROUNDPLOT, PSTYLEPOLICY, FONTALT và khóa hồ sơ tương ứng; nút ánh xạ font mở UI-J04.
- **Tiêu chí nghiệm thu:**
  1. Thêm đường dẫn hỗ trợ rồi Áp dụng: font, `.lin`, `.pat` trong thư mục đó tìm được ngay (NT-011), không khởi động lại.
  2. Tự lưu mỗi N phút (0–600, ô UI0-007): DOC-019 dùng giá trị mới ở chu kỳ kế tiếp; tắt `.bak` thì lần lưu sau không tạo `.bak`.
  3. Kiểm ánh xạ tự động mọi hàng của ba tab ↔ sysvar hoặc khóa hồ sơ; định dạng lưu mặc định dùng chung giá trị với UIB-004.
  4. Đạt DoD-U, DoD-L.

### UID-023 · Tùy chọn: tab Người dùng và Vẽ
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UID-019, UID-016, UID-027, UIC-019, INT-004, INT-010, INT-011, INT-018, CMD:DIST
- **Màn hình:** UI-D10
- **Đầu vào:** 07.2 UI-D10 dòng **Người dùng** (nhấp đúp để sửa; menu chuột phải theo ngữ cảnh, chuột phải theo thời gian; tỉ lệ chèn mặc định nguồn/đích; kích thước liên kết khi tạo (`DIMASSOC`); hoàn tác gộp zoom/pan và gộp thay đổi layer; lineweight; danh sách tỉ lệ (UI-D11); ưu tiên nhập tọa độ) và **Vẽ** (AutoSnap: marker, nam châm, tooltip, ô bắt điểm, màu; cỡ marker; bỏ qua hatch, bỏ qua đường gióng kích thước, thay Z bằng cao độ hiện hành; AutoTrack: vector polar, vector toàn màn hình, tooltip; lấy điểm theo dõi tự động hoặc giữ Shift; cỡ ô bắt điểm; giao diện tooltip); prototype `js/screens8.js` `SC.optTab` nhánh `Người dùng`, `Vẽ`.
- **Đầu ra:** hai tab trong `OptionsDialog`; ghi DBLCLKEDIT, SHORTCUTMENU, INSUNITSDEFSOURCE, INSUNITSDEFTARGET, DIMASSOC, OSNAPCOORD, AUTOSNAP, APERTURE, OSOPTIONS, TRACKPATH, POLARMODE (bit lấy điểm) và khóa hồ sơ; nút mở UIC-019, UID-027, UID-016.
- **Tiêu chí nghiệm thu:**
  1. Kiểm ánh xạ tự động mọi hàng; mỗi lựa chọn AutoSnap đổi đúng hành vi marker trên vùng vẽ (kịch bản QA-011 với DIST).
  2. Ưu tiên nhập tọa độ "Bàn phím trừ script": tọa độ gõ thắng bắt điểm khi gõ tay, script vẫn theo bắt điểm như AutoCAD (OSNAPCOORD = 2).
  3. Chuột phải theo thời gian: nhả trước ngưỡng (mặc định 250 ms như AutoCAD) = Enter, giữ quá ngưỡng = menu; kiểm bằng sự kiện chuột giả lập ở 100, 249, 251, 400 ms.
  4. Đạt DoD-U, DoD-L.

### UID-024 · Tùy chọn: tab Chọn và Grip
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UID-019, UIC-001, UIC-017, INT-008, INT-013
- **Màn hình:** UI-D10
- **Đầu vào:** 07.2 UI-D10 dòng **Chọn** (cỡ ô chọn; chọn trước rồi gọi lệnh; dùng Shift để thêm; chọn theo nhóm; hatch liên kết; tự tạo khung chọn; nhấn-kéo đối tượng; nhấn-kéo để lasso; kiểu khung; giới hạn số đối tượng cho palette Thuộc tính; xem trước vùng chọn; hiệu ứng tô sáng; Grip: cỡ, màu nguội / rê / nóng / viền, hiện grip, grip trong block, tooltip grip, menu grip động, Ctrl để luân phiên, giới hạn số đối tượng hiện grip); prototype `js/screens8.js` `SC.optTab` nhánh `Chọn`.
- **Đầu ra:** tab Chọn trong `OptionsDialog`; ghi PICKBOX, PICKFIRST, PICKADD, PICKSTYLE, PICKAUTO, PICKDRAG, SELECTIONPREVIEW, PROPOBJLIMIT, GRIPSIZE, GRIPCOLOR, GRIPHOVER, GRIPHOT, GRIPCONTOUR, GRIPS, GRIPBLOCK, GRIPTIPS, GRIPMULTIFUNCTIONAL, GRIPOBJLIMIT; ô xem trước cỡ ô chọn và cỡ grip.
- **Tiêu chí nghiệm thu:**
  1. Kiểm ánh xạ tự động mọi hàng; PROPOBJLIMIT đổi ngưỡng "Nhiều đối tượng" của UIC-001 ngay.
  2. Đổi cỡ và màu grip: grip trên vùng vẽ đổi trong khung hình kế tiếp; màu grip dùng bộ chọn màu UIC-017.
  3. Đạt DoD-U, DoD-L.

### UID-025 · Tùy chọn: nhập và xuất hồ sơ `.arg` tương thích AutoCAD
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UID-021, CUS-003, QA-007
- **Màn hình:** UI-D10
- **Đầu vào:** 07.2 UI-D10 dòng **Hồ sơ** ("Xuất/Nhập (.arg, tương thích AutoCAD)"); 03 §Giao diện (`OPTIONS` > Profiles, P1); engine đọc profile AutoCAD của CUS-003; prototype `js/screens2.js` nhánh `Hồ sơ` (nút Xuất (.arg), Nhập (.arg)).
- **Đầu ra:** nút "Nhập (.arg)…", "Xuất (.arg)…" trong tab Hồ sơ; hộp báo cáo nhập (khóa đã nhập, khóa bỏ qua kèm lý do).
- **Tiêu chí nghiệm thu:**
  1. Nhập 10 tệp `.arg` mẫu xuất từ AutoCAD (QA-007): đường dẫn hỗ trợ, thư mục trusted, màu vùng vẽ, AutoSnap nhập đúng; khóa không hỗ trợ liệt kê trong báo cáo.
  2. Xuất `.arg` rồi nhập lại trùng 100% khóa đã hỗ trợ.
  3. Đạt DoD-U, DoD-L.

### UID-026 · Tùy chọn: phần của bản 1.0 (Mô hình 3D, Block Editor, field)
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UID-023, RND-023, DOC-022, UI-G04
- **Màn hình:** UI-D10
- **Đầu vào:** 07.2 UI-D10 dòng **Mô hình 3D** ("Tối thiểu ở v1.0: hiển thị ViewCube; kiểu hiển thị khi xoay 3D; hiện chữ thập 3D") và dòng **Người dùng** (Block Editor; field: hiện nền, khi nào cập nhật); prototype `js/screens8.js` nhánh `Mô hình 3D`.
- **Đầu ra:** tab Mô hình 3D; hàng Block Editor (mở thiết lập của UI-G04) và hàng field trong tab Người dùng; ghi NAVVCUBEDISPLAY, FIELDDISPLAY, FIELDEVAL và khóa hồ sơ.
- **Tiêu chí nghiệm thu:**
  1. Kiểm ánh xạ tự động mọi hàng; FIELDEVAL đổi đúng thời điểm cập nhật field (mở, lưu, in) theo kịch bản kiểm thử.
  2. ViewCube tự hiện khi bản vẽ có đối tượng 3D và tắt được.
  3. Đạt DoD-U, DoD-L.

### UID-027 · Danh sách tỉ lệ SCALELISTEDIT
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD:SCALELISTEDIT, DOC-021, UI0-008
- **Màn hình:** UI-D11
- **Đầu vào:** 07.2 UI-D11 (Thêm, Sửa, Lên/Xuống, Xóa, Đặt lại; danh sách mặc định theo TCVN 1:1 … 1:10000, 2:1, 5:1, 10:1); 03 §Chú thích (`SCALELISTEDIT` P1); prototype `index.html#UI-D11`, `js/screens5.js` `SC.scaleList`.
- **Đầu ra:** `app/dialogs/settings/ScaleListDialog.{h,cpp}` (bảng tỉ lệ, đơn vị giấy, đơn vị bản vẽ; hộp nhập "giấy:bản vẽ" kiểm ngay khi gõ); xác nhận gửi một lệnh.
- **Tiêu chí nghiệm thu:**
  1. Tỉ lệ sai dạng hoặc trùng báo lỗi tại ô; không xóa được tỉ lệ đang dùng (CANNOSCALE, viewport, đối tượng annotative), lý do nêu rõ.
  2. Đặt lại hỏi xác nhận và trả về đúng 20 tỉ lệ mặc định TCVN theo thứ tự.
  3. Xác nhận: một lệnh, một mark undo; dropdown tỉ lệ của viewport và thanh trạng thái đổi theo.
  4. Đạt DoD-U, DoD-L.

### UID-028 · Quản lý view ở Viewer: duyệt và đặt hiện hành
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD:VIEW, INT-016, RND-021, UI0-008
- **Màn hình:** UI-D12
- **Đầu vào:** 07.2 UI-D12 ("1.0 (V: chỉ Đặt hiện hành)"; cây view hiện hành, view Model, view Layout, view chuẩn); 07.1 §7.6 (V: chỉ khôi phục view); lịch sử view INT-016; prototype `index.html#UI-D12`, `js/screens14.js` `SC.viewManager`.
- **Đầu ra:** `app/dialogs/view/ViewManagerDialog.{h,cpp}` ở chế độ xem: cây view, thuộc tính chỉ đọc, ảnh xem trước view bằng RND-021, "Đặt hiện hành" (nhấp đúp cũng được); nút sửa có huy hiệu Pro.
- **Tiêu chí nghiệm thu:**
  1. Đặt hiện hành một view Model: vùng vẽ về đúng tâm và chiều cao đã lưu (sai số ≤ 1e-9 tương đối), ghi vào lịch sử view (ZOOM Previous quay lại được).
  2. Ảnh xem trước tạo ở nền ≤ 100 ms mỗi view **[đề xuất]**; 200 view cuộn 60 fps.
  3. Đạt DoD-U, DoD-L.

### UID-029 · Quản lý view đầy đủ
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UID-028, CMD:VIEW, DOC-011, INT-015, UI0-007
- **Màn hình:** UI-D12
- **Đầu vào:** 07.2 UI-D12 (thuộc tính view: tên, nhóm, UCS, ảnh chụp trạng thái layer, tỉ lệ chú thích, kiểu hiển thị, nền, khung bao; nút Đặt hiện hành, Mới…, Cập nhật layer, Sửa khung bao, Xóa); 03 §Hiển thị (`VIEW`, `-VIEW` P1); prototype `js/screens14.js` `SC.viewManager`, hộp `viewnew`.
- **Đầu ra:** chế độ đầy đủ của `ViewManagerDialog`; hộp View mới (tên, nhóm, khung bao màn hình hiện tại hoặc chọn cửa sổ, lưu trạng thái layer, UCS, kiểu hiển thị, nền); sửa tên và nhóm tại chỗ; mọi thao tác gửi lệnh VIEW.
- **Tiêu chí nghiệm thu:**
  1. Tạo view có ảnh chụp layer, tắt vài layer, đặt hiện hành view: layer trở lại đúng ảnh chụp; một mark undo.
  2. Sửa khung bao: ẩn hộp, chỉ cửa sổ trên vùng vẽ, quay lại với khung mới.
  3. Tên trùng hoặc rỗng báo lỗi tại ô; Xóa hỏi xác nhận nêu tên.
  4. Đạt DoD-U, DoD-L.

### UID-030 · Quản lý UCS và biểu tượng UCS
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** INT-015, UI0-007, UI0-008
- **Màn hình:** UI-D13
- **Đầu vào:** 07.2 UI-D13 (tab UCS đã đặt tên; UCS trực giao; tab Thiết lập: bật biểu tượng, hiện tại gốc, áp dụng cho mọi viewport, cho chọn biểu tượng UCS, lưu UCS theo viewport, chuyển về mặt bằng khi đổi UCS); 03 §Bản vẽ (`UCSMAN` P1 đầy đủ); UCS và lệnh UCS, UCSMAN, UCSICON của INT-015 (README §4.2); prototype `index.html#UI-D13`, `js/screens14.js` `SC.ucsMan`, hộp `ucsicon`.
- **Đầu ra:** `app/dialogs/view/UcsManagerDialog.{h,cpp}` (đặt hiện hành, chi tiết gốc và trục, đổi tên, xóa; UCS trực giao Trên/Dưới/Trước/Sau/Trái/Phải theo UCS gốc); hộp Biểu tượng UCS (kiểu 2D/3D, cỡ, màu Model và Layout); ghi UCSICON, UCSVP, UCSFOLLOW, UCSSELECTMODE.
- **Tiêu chí nghiệm thu:**
  1. Đặt hiện hành UCS đã đặt tên: tọa độ hiện trên thanh trạng thái đổi theo UCS mới (so INT-015); một mark undo.
  2. Chi tiết gốc và trục hiện đúng 4 chữ số thập phân; đổi tên trùng báo lỗi.
  3. Thiết lập biểu tượng đổi hình biểu tượng UCS trên vùng vẽ trong khung hình kế tiếp.
  4. Đạt DoD-U, DoD-L.

### UID-031 · Cấu hình viewport VPORTS
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD:VPORTS, CMD:MVIEW, DOC-011, UI0-008
- **Màn hình:** UI-D14
- **Đầu vào:** 07.2 UI-D14 (tab Viewport mới: tên, danh sách cấu hình chuẩn, xem trước, áp dụng cho màn hình / viewport hiện hành, kiểu hiển thị; tab Viewport đã đặt tên); 03 §Layout (`VPORTS` P0); prototype `index.html#UI-D14`, `js/screens5.js` `SC.vports`, `CONFIGS`.
- **Đầu ra:** `app/dialogs/view/ViewportConfigDialog.{h,cpp}` (12 cấu hình chuẩn có ô xem trước; ở layout tạo viewport theo cấu hình qua MVIEW; ở Model chia vùng vẽ qua VPORTS; lưu và khôi phục cấu hình đã đặt tên).
- **Tiêu chí nghiệm thu:**
  1. Mỗi cấu hình trong 12 cấu hình tạo đúng số viewport và tỉ lệ khung như xem trước (so DOC-011).
  2. Cấu hình đã đặt tên: lưu rồi khôi phục cho cùng bố cục; trong layout một lần tạo là một mark undo.
  3. Đạt DoD-U, DoD-L.

### UID-032 · Thiết lập bị khóa bởi cấu hình doanh nghiệp
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** PLF-114, UI-D10, UI0-007
- **Màn hình:** UI-D10
- **Đầu vào:** 07.3 UI-L18 (triển khai doanh nghiệp, cấu hình công ty); API mục khóa của PLF-114 (`OrgConfig`); hộp Tùy chọn UI-D10.
- **Đầu ra:** trong hộp Tùy chọn: mục bị khóa hiện biểu tượng khóa và chú thích "Do quản trị viên công ty đặt", không sửa được; Đặt lại và nhập hồ sơ bỏ qua mục khóa. Kiểm thử cạnh mã.
- **Tiêu chí nghiệm thu:**
  1. Cấu hình công ty khóa N mục: đúng N điều khiển bị vô hiệu, có biểu tượng khóa và chú thích; 0 mục khác bị ảnh hưởng.
  2. Nhập hồ sơ hoặc Đặt lại không đổi giá trị mục khóa (so trước và sau).
  3. Cấu hình công ty tải lại khi hộp thoại đang mở: trạng thái khóa cập nhật ≤ 1 s.
  4. Đạt DoD-U.

## 8. Vẽ và hiệu chỉnh (UI-E01–E09)

### UIE-001 · Tab ngữ cảnh Hatch: panel Biên, Thuộc tính, Gốc, Tùy chọn, Đóng
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** UIA-005, CMD:HATCH, CMD:HATCHEDIT, UIC-017, UI0-007, UI0-015
- **Màn hình:** UI-E01
- **Đầu vào:** 07.2 UI-E01 (mở từ `HATCH` (`H`) và khi chọn hatch có sẵn: tab Sửa hatch; panel Biên: Chọn điểm, Chọn đối tượng, Bớt biên, Tạo lại biên, Hiện đối tượng biên, Giữ đường biên polyline/region, Tập biên; panel Thuộc tính: loại, màu hatch, màu nền, độ trong suốt, góc, tỉ lệ, layer ghi đè, tương đối với Paper space, đan chéo, bề rộng nét ISO; panel Gốc; panel Tùy chọn: liên kết, chú thích theo tỉ lệ, khớp thuộc tính, dung sai khe hở, tạo các hatch riêng, nhận diện island, thứ tự hiển thị; panel Đóng; vùng không kín chỉ chỗ hở và gợi ý tăng dung sai khe hở); 07.1 §7.11 (tab ngữ cảnh tự hiện, tự đóng); 03 §Vẽ (`HATCH` P0 cơ bản); prototype `index.html#UI-E01`, `js/screens6.js` `hatchTab`, `setHS`, `IMPL.HATCH`, `ED.ctxRefresh`.
- **Đầu ra:** `app/dialogs/draw/hatch_tab.json` (định nghĩa tab cho UIA-005), `app/dialogs/draw/HatchTabController.{h,cpp}`: hai chế độ Tạo hatch (trong lệnh HATCH: giá trị panel ↔ tùy chọn của lệnh hai chiều) và Sửa hatch (vùng chọn chỉ gồm hatch, ≤ 50 đối tượng: mỗi lần đổi là một lệnh HATCHEDIT); thiết lập mặc định nhớ theo hồ sơ; khi lệnh báo vùng hở, ô "Dung sai khe hở" nhận focus kèm gợi ý; kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Gõ `HATCH`: tab Tạo hatch hiện ≤ 100 ms; Enter/Esc hoặc nút Đóng kết thúc lệnh và tab đóng; chọn 1–50 hatch hiện tab Sửa hatch, bỏ chọn thì tab đóng.
  2. Đổi giá trị trên tab khi đang tạo: lệnh HATCH nhận đúng tùy chọn (kiểm bằng nhật ký lệnh), giá trị gõ trên dòng lệnh phản ánh lại trên tab trong cùng khung hình.
  3. Sửa hatch: đổi tỉ lệ, góc, màu trên 50 hatch là một lệnh, một mark undo; vùng vẽ cập nhật ≤ 100 ms **[đề xuất]** với hatch ≤ 10.000 nét mẫu.
  4. Mọi điều khiển có tooltip tên lệnh hoặc sysvar tiếng Anh (HPSCALE, HPANG, HPGAPTOL, HPISLANDDETECTION, HPASSOC…); ô số theo UI-C09; panel co theo ưu tiên ở 1366×768 (UIA-005).
  5. Đạt DoD-U, DoD-L.

### UIE-002 · Gallery mẫu hatch có xem trước khi rê chuột
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIE-001, RGN-012, RND-021, INT-012, VN-008, NT-011
- **Màn hình:** UI-E01
- **Đầu vào:** 07.2 UI-E01 (panel Mẫu: gallery có xem trước khi rê chuột; nhóm ANSI, ISO, Khác, TCVN (bê tông, BTCT, gạch, đất, cát, gỗ, kim loại, cách nhiệt…), Người dùng, Tùy biến; tìm theo tên); 07.1 §7.1 (gallery xem trước khi rê, P1); 07.1 §9.8 (xem trước không ghi vào bản vẽ); parser `.pat` và sinh nét RGN-012; xem trước trong lệnh INT-012; `.pat` TCVN của VN-008; prototype `js/screens6.js` `SC.patThumb`, `SC.hatchGallery`, hàm `hover` của gallery `h-gal`.
- **Đầu ra:** `app/dialogs/draw/HatchPatternGallery.{h,cpp}`: ô gallery trong ribbon và hộp gallery mở rộng (nhóm, tìm kiếm, ô to); thumbnail vẽ bằng RGN-012 + RND-021, cache đĩa theo hash nội dung `.pat` **[đề xuất]**; rê chuột: hatch đang sửa hoặc vùng đang rê trong lệnh HATCH đổi sang mẫu được rê trên overlay, rời chuột thì trở lại; bấm thì áp dụng.
- **Tiêu chí nghiệm thu:**
  1. Rê lên một mẫu: vùng vẽ hiện mẫu mới ≤ 50 ms p99 **[đề xuất]** với vùng ≤ 10.000 nét mẫu; dày hơn thì dùng LOD của RGN-012, vẫn trong ngân sách; rời chuột trở lại trong 1 khung hình.
  2. Trong suốt lúc rê không có sự kiện DOC-007 nào và không có bước undo; chỉ bấm mới tạo một lệnh.
  3. 300 mẫu: thumbnail tạo ở nền, lần mở sau lấy từ cache ≤ 1 ms mỗi ô **[đề xuất]**; tìm "bê tông" và "AR-CONC" đều ra mẫu bê tông.
  4. Nhóm TCVN đủ mẫu vật liệu của VN-008; tệp `.pat` lỗi bỏ qua mẫu đó và ghi lý do, không crash.
  5. Đạt DoD-U, DoD-L.

### UIE-003 · Gradient trong tab Hatch
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UIE-002, CMD:GRADIENT, RND-008
- **Màn hình:** UI-E01
- **Đầu vào:** 07.2 UI-E01 (loại Gradient; "Phát hành: 1.0 (gradient, cổ điển)"); 07.2 UI-C01 dòng Hatch (gradient: màu 1/2, tên, căn giữa, góc, sắc độ); 03 §Vẽ (`GRADIENT` P1); tô gradient RND-008; prototype `js/screens13.js` (tab Gradient, `swapCmd('GRADIENT')`).
- **Đầu ra:** nhóm gradient trong tab Hatch (một màu / hai màu, màu 1, màu 2 hoặc thanh sắc độ, 9 kiểu gradient trong gallery, căn giữa, góc); lệnh GRADIENT mở tab ở chế độ gradient.
- **Tiêu chí nghiệm thu:**
  1. 9 kiểu × căn giữa × 4 góc: hình trên vùng vẽ trùng thumbnail gallery (cùng RND-008); rê xem trước như UIE-002.
  2. Đổi màu bằng UIC-017: một lệnh, một mark undo khi đang sửa hatch.
  3. Đạt DoD-U, DoD-L.

### UIE-004 · Hộp thoại Hatch cổ điển
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIE-001, UIE-003, CMD:HATCH, UI0-008
- **Màn hình:** UI-E01
- **Đầu vào:** 07.2 UI-E01 (tùy chọn "Hộp thoại cổ điển" cho người thích dạng hộp thoại, P1); 07.2 UI-E10 dòng `HATCH` (tùy chọn seTtings); prototype `js/screens6.js` nút `h-classic`.
- **Đầu ra:** `app/dialogs/draw/HatchClassicDialog.{h,cpp}` (tab Hatch và Gradient, nhóm Biên, Tùy chọn, phần mở rộng island, giữ biên, tập biên, dung sai khe hở, kế thừa thuộc tính; nút Xem trước); cùng model thiết lập với UIE-001; mở từ tùy chọn `T` của HATCH và nút trên tab; thiết lập "luôn dùng hộp cổ điển" theo hồ sơ.
- **Tiêu chí nghiệm thu:**
  1. Mọi thiết lập của hộp cổ điển và tab ngữ cảnh luôn cùng giá trị (đổi bên này, mở bên kia thấy ngay).
  2. Xem trước: ẩn hộp, hiện hatch tạm, Enter chấp nhận, Esc quay lại hộp; xác nhận là một lệnh HATCH, một mark undo.
  3. Đạt DoD-U, DoD-L.

### UIE-005 · Tab ngữ cảnh Mảng chữ nhật và mảng tròn
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIA-005, CMD:ARRAYRECT, CMD:ARRAYPOLAR, UI0-007, UI0-015
- **Màn hình:** UI-E02
- **Đầu vào:** 07.2 UI-E02 (mảng chữ nhật: Cột số, khoảng cách, tổng; Hàng số, khoảng cách, tổng, cao độ tăng dần; Cấp (Sau); Thuộc tính liên kết, điểm gốc; Đóng; mảng tròn: Phần tử số, góc giữa, góc phủ; Hàng; Thuộc tính điểm gốc, xoay phần tử, chiều; grip kéo đổi khoảng cách hoặc số lượng; "Phát hành: EA (chữ nhật, tròn, không liên kết, hộp thoại cổ điển)"); 03 §Sửa (`ARRAYRECT`, `ARRAYPOLAR` P0 không liên kết); prototype `index.html#UI-E02`, `js/screens6.js` `arrTab`, `arrGrips`, `IMPL.ARRAYRECT`, `IMPL.ARRAYPOLAR`.
- **Đầu ra:** `app/dialogs/draw/array_tab.json`, `app/dialogs/draw/ArrayTabController.{h,cpp}`: ô số hai chiều với tùy chọn của lệnh (COUnt, Spacing, COLumns, Rows, Base point…); tổng = (số − 1) × khoảng cách tự tính; nút Liên kết vô hiệu kèm "Có từ v1.0"; panel Cấp vô hiệu kèm "sau v1.0"; kịch bản QA-011.
- **Tiêu chí nghiệm thu:**
  1. Gõ số cột trên tab: xem trước mảng với 1.000 phần tử cập nhật ≤ 8 ms/khung (DoD-L, xem trước trong lệnh); gõ cùng giá trị trên dòng lệnh cho cùng kết quả.
  2. Kéo grip số cột, khoảng cách: ô trên tab đổi theo trong cùng khung hình.
  3. Nhập số âm hoặc 0 cho số lượng báo lỗi tại ô; khoảng cách âm đổi hướng như AutoCAD.
  4. Đóng: một lệnh tạo mảng không liên kết, một mark undo.
  5. Đạt DoD-U, DoD-L.

### UIE-006 · Hộp thoại mảng cổ điển ARRAYCLASSIC
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD:ARRAYCLASSIC, CMD:ARRAYRECT, CMD:ARRAYPOLAR, INT-008, INT-012, UI0-007, UI0-008
- **Màn hình:** UI-E02
- **Đầu vào:** 07.2 UI-E02 (chọn Chữ nhật: số hàng, số cột, khoảng cách hàng/cột, góc mảng; hoặc Tròn: tâm, số phần tử, góc phủ, góc giữa các phần tử, xoay phần tử; nút Chọn đối tượng, Xem trước; luôn tạo mảng không liên kết); prototype `js/screens6.js` `IMPL.ARRAYCLASSIC`.
- **Đầu ra:** `app/dialogs/draw/ArrayClassicDialog.{h,cpp}` (ô số có nút lấy từ bản vẽ; ô xem trước sơ đồ; Xem trước <: ẩn hộp, hiện mảng tạm, hỏi Chấp nhận / Sửa / Hủy).
- **Tiêu chí nghiệm thu:**
  1. 20 bộ thông số mẫu: mảng tạo ra trùng hình ARRAYRECT/ARRAYPOLAR với cùng thông số (so tọa độ, sai số ≤ 1e-9 tương đối).
  2. Chọn đối tượng với PICKFIRST: vùng chọn có sẵn được dùng ngay; không có đối tượng thì nút xác nhận báo "Chọn đối tượng trước".
  3. Xác nhận là một lệnh, một mark undo; Hủy ở bước xem trước không để lại đối tượng tạm.
  4. Đạt DoD-U, DoD-L.

### UIE-007 · Mảng liên kết và mảng theo đường
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIE-005, CMD:ARRAYPATH, CMD:ARRAYEDIT, INT-013
- **Màn hình:** UI-E02
- **Đầu vào:** 07.2 UI-E02 (mảng theo đường: Phần tử số, khoảng cách, tổng; Hàng; Thuộc tính liên kết, điểm gốc, hướng tiếp tuyến, chia đều hay theo khoảng, canh phần tử; sửa mảng liên kết: nhấp vào mảng thì tab hiện lại, có Sửa nguồn, Thay phần tử, Đặt lại mảng; "1.0 (liên kết, theo đường)"); 03 §Sửa (`ARRAYPATH`, `ARRAYEDIT` P1); grip mảng qua INT-013; prototype `js/screens13.js` `IMPL.ARRAYPATH`.
- **Đầu ra:** panel mảng theo đường trong `array_tab.json`; chế độ Sửa mảng (chọn một mảng liên kết hiện tab; mỗi lần đổi là một lệnh ARRAYEDIT); bật nút Liên kết.
- **Tiêu chí nghiệm thu:**
  1. Chọn mảng liên kết: tab hiện ≤ 100 ms với giá trị đúng của mảng; đổi số phần tử là một lệnh, một mark undo.
  2. Sửa nguồn, Thay phần tử, Đặt lại mảng gọi đúng tùy chọn của ARRAYEDIT; Esc thoát chế độ sửa nguồn không đổi mảng.
  3. Mảng theo đường: đổi chia đều / theo khoảng, canh phần tử cập nhật xem trước ≤ 8 ms/khung với 500 phần tử (DoD-L).
  4. Đạt DoD-U, DoD-L.

### UIE-008 · Hộp thoại Đường biên BOUNDARY
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD:BOUNDARY, UI0-008
- **Màn hình:** UI-E03
- **Đầu vào:** 07.2 UI-E03 (chọn điểm; nhận diện island; loại đối tượng tạo ra polyline / region; tập biên viewport hiện hành / chọn mới; sau khi tạo hiện diện tích của đường biên vừa tạo trên dòng lệnh); 03 §Vẽ (`BOUNDARY` P0); prototype `index.html#UI-E03`, `js/screens6.js` `IMPL.BOUNDARY`.
- **Đầu ra:** `app/dialogs/draw/BoundaryDialog.{h,cpp}`: nút Chọn điểm ẩn hộp và chuyển sang prompt của BOUNDARY; tùy chọn chuyển thành tùy chọn của lệnh.
- **Tiêu chí nghiệm thu:**
  1. Đường biên tạo từ hộp thoại trùng kết quả `-BOUNDARY` cùng thiết lập trên 30 vùng mẫu (so tô-pô và tọa độ).
  2. Region vô hiệu kèm lý do nếu REGION chưa có ở mốc hiện tại; dòng lệnh hiện diện tích theo UNITS.
  3. Đạt DoD-U, DoD-L.

### UIE-009 · Xóa trùng lặp OVERKILL
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD:OVERKILL, UI0-007, UI0-008
- **Màn hình:** UI-E04
- **Đầu vào:** 07.2 UI-E04 (dung sai; bỏ qua thuộc tính màu, layer, linetype, tỉ lệ linetype, độ dày, độ trong suốt, plot style, độ dày đùn, vật liệu; tối ưu đoạn trong polyline, bỏ qua bề rộng polyline, không phá polyline; gộp đoạn thẳng hàng chồng một phần, gộp đoạn thẳng hàng nối tiếp; giữ đối tượng liên kết; kết quả "Đã xóa 1.240 đối tượng trùng, gộp 85 đoạn", hoàn tác được); prototype `index.html#UI-E04`, `js/screens6.js` `IMPL.OVERKILL`.
- **Đầu ra:** `app/dialogs/draw/OverkillDialog.{h,cpp}`; thông số chuyển thành tùy chọn của OVERKILL; toast kết quả có [Hoàn tác].
- **Tiêu chí nghiệm thu:**
  1. Kết quả từ hộp thoại trùng `-OVERKILL` cùng tùy chọn trên 20 bản vẽ mẫu; số đếm trong thông báo bằng số đối tượng xóa và gộp thật.
  2. Một lần chạy là một lệnh, một mark undo; dung sai ≤ 0 báo lỗi tại ô.
  3. Đạt DoD-U, DoD-L.

### UIE-010 · Quản lý nhóm CLASSICGROUP
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD:CLASSICGROUP, CMD:GROUP, DOC-020, RND-013, INT-008, UI0-006
- **Màn hình:** UI-E05
- **Đầu vào:** 07.2 UI-E05 (danh sách nhóm: tên, chọn được; tạo nhóm: tên, mô tả, chọn được, không tên; sửa nhóm: bớt/thêm đối tượng, đổi tên, sắp lại thứ tự, mô tả, rã nhóm, bật/tắt chọn được); 03 §Chọn (`GROUP`, `CLASSICGROUP` P1); nhóm DOC-020; prototype `index.html#UI-E05`, `js/screens6.js` `IMPL.CLASSICGROUP`.
- **Đầu ra:** `app/dialogs/draw/GroupManagerDialog.{h,cpp}` (bảng tên, chọn được, số đối tượng; hiện nhóm không tên; tô sáng thành viên nhóm đang chọn trên vùng vẽ qua RND-013; các nút ẩn hộp khi cần chọn đối tượng); mỗi thao tác một lệnh.
- **Tiêu chí nghiệm thu:**
  1. Tạo, đổi tên, thêm/bớt, sắp thứ tự, rã nhóm: mỗi thao tác một lệnh, một mark undo; kết quả trùng `-GROUP` cùng thao tác (so DOC-020).
  2. Chọn một nhóm có 10.000 đối tượng: tô sáng ≤ 100 ms, không dựng lại batch.
  3. Tên trùng báo lỗi; nhóm không tên tên dạng `*A1` như AutoCAD.
  4. Đạt DoD-U, DoD-L.

### UIE-011 · Chọn nhanh QSELECT
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD:QSELECT, DOC-017, DOC-009, INT-008, NT-005, UI0-007, UI0-008
- **Màn hình:** UI-E06
- **Đầu vào:** 07.2 UI-E06 (áp dụng cho toàn bản vẽ / vùng chọn hiện tại kèm nút chọn; loại đối tượng: Nhiều loại / từng loại có trong bản vẽ; thuộc tính; toán tử =, ≠, >, <, * khớp wildcard, Chọn tất cả; giá trị dropdown hoặc ô nhập; đưa vào / loại khỏi tập chọn mới; cộng vào tập chọn hiện tại); 03 §Chọn (`QSELECT` P0); prototype `index.html#UI-E06`, `js/screens6.js` `IMPL.QSELECT`, `qval`.
- **Đầu ra:** `app/dialogs/select/QuickSelectDialog.{h,cpp}`: danh sách loại có số lượng, danh sách thuộc tính theo DOC-017 (thuộc tính chung khi "Nhiều loại"), dropdown giá trị gồm các giá trị có trong bản vẽ (tính ở nền) với thuộc tính liệt kê (layer, màu, linetype, kiểu…), ô số UI0-007 với thuộc tính số; nút Chọn nhanh của palette Thuộc tính (UIC-002) mở hộp này.
- **Tiêu chí nghiệm thu:**
  1. Cảnh 1 triệu entity của BENCH-STD: từ bấm nút xác nhận đến vùng chọn hiện trên vùng vẽ ≤ 300 ms p99 **[đề xuất]**; dropdown giá trị đầy ≤ 200 ms, chạy nền.
  2. 1.000 truy vấn ngẫu nhiên (loại × thuộc tính × toán tử): tập chọn trùng bộ lọc tham chiếu trên snapshot.
  3. Loại khỏi tập chọn và cộng vào tập chọn hiện tại đúng nghĩa; vùng chọn mới đi qua INT-008 nên dùng được với PICKFIRST và lệnh kế tiếp.
  4. Đạt DoD-U, DoD-L, DoD-H.
- **Ngoài phạm vi:** truy vấn đã lưu (P2, chưa có thiết kế đợt 5).

### UIE-012 · Bộ lọc chọn FILTER
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD:FILTER, CMD:MOVE, INT-008, DOC-017, UI0-007, UI0-008
- **Màn hình:** UI-E07
- **Đầu vào:** 07.2 UI-E07 (danh sách điều kiện: loại đối tượng, thuộc tính, toán tử, nhóm logic AND/OR/XOR/NOT; lưu và nạp bộ lọc có tên; áp dụng vào lệnh "Chọn đối tượng" đang chạy); 03 §Chọn (`FILTER` P1, nền của `ssget` có bộ lọc); bộ lọc kiểu `ssget` của INT-008; prototype `index.html#UI-E07`, `js/screens6.js` `IMPL.FILTER`.
- **Đầu ra:** `app/dialogs/select/SelectionFilterDialog.{h,cpp}` (bảng điều kiện có thụt lề theo nhóm logic; Sửa dòng, Xóa dòng, Xóa danh sách; Thêm từ đối tượng; Lưu/Nạp/Xóa bộ lọc có tên; dùng trong suốt `'FILTER` giữa prompt chọn).
- **Tiêu chí nghiệm thu:**
  1. Danh sách điều kiện dịch sang bộ lọc `ssget` (mã `-4`) trùng 100% biểu thức tham chiếu trên 500 ca; nhóm logic không cân bằng báo lỗi và không áp dụng.
  2. Dùng `'FILTER` giữa lệnh MOVE: vùng chọn trả về prompt đang chờ, lệnh chạy tiếp.
  3. Bộ lọc có tên lưu và nạp lại trùng 100%; tệp bộ lọc hỏng báo lỗi, không crash.
  4. Đạt DoD-U, DoD-L.

### UIE-013 · Thiết lập Chọn tương tự
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD:SELECTSIMILAR, INT-005, UI0-008
- **Màn hình:** UI-E08
- **Đầu vào:** 07.2 UI-E08 (ô chọn Màu, Layer, Linetype, Tỉ lệ linetype, Độ dày nét, Plot style, Kiểu đối tượng, Tên); 03 §Chọn (`SELECTSIMILAR` P0); sysvar SELECTSIMILARMODE; prototype `index.html#UI-E08`, `js/screens6.js` `SC.similarSettings`.
- **Đầu ra:** `app/dialogs/select/SelectSimilarSettingsDialog.{h,cpp}`; mở từ tùy chọn `SE` của SELECTSIMILAR và menu chuột phải; ghi SELECTSIMILARMODE.
- **Tiêu chí nghiệm thu:**
  1. 256 tổ hợp ô chọn: SELECTSIMILARMODE ghi ra trùng bảng bit tham chiếu; SELECTSIMILAR sau đó chọn đúng tập trên bản vẽ mẫu.
  2. Đạt DoD-U, DoD-L.

### UIE-014 · Công cụ sửa đa tuyến song song MLEDIT
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD:MLEDIT, UI0-008
- **Màn hình:** UI-E09
- **Đầu vào:** 07.2 UI-E09 (lưới 12 công cụ: giao chữ thập kín / hở / hợp nhất, giao chữ T kín / hở / hợp nhất, nối góc, thêm đỉnh, xóa đỉnh, cắt một, cắt tất cả, hàn tất cả); 03 §Vẽ (`MLEDIT` P1); prototype `index.html#UI-E09`, `js/screens6.js` `IMPL.MLEDIT`, `mlIcon`.
- **Đầu ra:** `app/dialogs/draw/MLineEditDialog.{h,cpp}`: 12 nút biểu tượng tự vẽ có nhãn; bấm một nút ẩn hộp và chạy công cụ tương ứng của MLEDIT.
- **Tiêu chí nghiệm thu:**
  1. Mỗi công cụ cho kết quả trùng `-MLEDIT` cùng công cụ trên bản vẽ mẫu (so đỉnh và mối nối).
  2. Mũi tên di chuyển trong lưới, Enter chạy công cụ; mỗi lần sửa một mark undo.
  3. Đạt DoD-U, DoD-L.

## 9. Prompt và chip tùy chọn của lệnh lõi (UI-E10)

### UIE-015 · Bộ kiểm thử chip tùy chọn sinh từ metadata prompt
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-001, INT-003, UIA-007, UIA-013, QA-011
- **Màn hình:** UI-E10
- **Đầu vào:** 07.2 UI-E10 (nhãn chip tiếng Việt kèm chữ cái tắt tiếng Anh "Bán kính · R"; gõ chữ tắt tiếng Anh luôn có tác dụng; người dùng chọn ngôn ngữ prompt ở UI-D10); 07.1 §8 (chip bấm được, chữ tắt gạch chân, giá trị mặc định `<10.0000>`); 07.1 §9.7 (prompt động, ↓ mở danh sách tùy chọn); design/README (từ khóa ẩn của grip MO, RO, SC, MI vẫn gõ được); mô tả prompt INT-003 (thông điệp, từ khóa toàn cục và địa phương, chữ tắt, mặc định, cờ ẩn); registry lệnh INT-001; prototype `index.html#UI-E10`, `js/cmd.js`, `js/draw.js`.
- **Đầu ra:** `app/dialogs/prompts/PromptConformance.{h,cpp}`: đọc tệp kỳ vọng `app/dialogs/prompts/expect/*.json` (lệnh, chuỗi nhập dẫn tới prompt, tập chip kỳ vọng theo thứ tự, nhãn vi và en, mặc định), chạy lệnh trong `trion` qua QA-011, đọc chip trên dòng lệnh và danh sách ↓ của DYN qua cây accessibility, so với kỳ vọng và với metadata INT-003; chế độ giả bản địa hóa +40% (UI0-005); báo cáo HTML trong CI.
- **Tiêu chí nghiệm thu:**
  1. Phát hiện đủ 5 lỗi cố ý cài vào lệnh mẫu: thiếu chip, sai chữ tắt, nhãn chưa dịch, danh sách ↓ của DYN khác chip, thiếu giá trị mặc định.
  2. Kiểm cả hai ngôn ngữ prompt; chip hiện trong cùng khung hình với thông điệp prompt (≤ 16 ms từ sự kiện prompt).
  3. Toàn bộ tệp kỳ vọng của UIE-016…UIE-019 chạy xong ≤ 8 phút trong CI.
  4. Đạt DoD-U, DoD-L.

### UIE-016 · Prompt và chip của nhóm lệnh vẽ
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIE-015, QA-007, CMD:LINE, CMD:PLINE, CMD:CIRCLE, CMD:ARC, CMD:RECTANG, CMD:POLYGON, CMD:ELLIPSE, CMD:SPLINE, CMD:XLINE, CMD:HATCH
- **Màn hình:** UI-E10
- **Đầu vào:** 07.2 UI-E10 các dòng `LINE`, `PLINE` (kể cả chế độ cung: Angle, CEnter, Direction, Line, Radius, Second pt), `CIRCLE`, `ARC`, `RECTANG`, `POLYGON`, `ELLIPSE`, `SPLINE`, `XLINE`, `HATCH`; prompt và từ khóa thật của từng lệnh trong `docs/specs/commands/<TÊN>.md`; bảng thuật ngữ 07.1 §13.4; prototype `index.html#UI-E10`, `js/draw.js` (LINE, PLINE, CIRCLE…), `js/screens6.js` `IMPL.HATCH`.
- **Đầu ra:** bản dịch vi/en đã duyệt của thông điệp prompt và nhãn từ khóa nhóm lệnh trong tệp dịch `app/dialogs/prompts/i18n/prompts_draw.{vi,en}.ts` (định dạng UI0-005); tệp kỳ vọng `app/dialogs/prompts/expect/draw.json`; ảnh chụp baseline QA-011 của prompt chính từng lệnh.
- **Tiêu chí nghiệm thu:**
  1. 100% prompt của 10 lệnh qua UIE-015: đủ chip, đúng thứ tự như AutoCAD, nhãn dạng "Tiếng Việt · TẮT" (ví dụ "Cung · A", "Nửa bề rộng · H").
  2. Gõ chữ tắt (hoa, thường, có tiền tố `_`) cho cùng kết quả như bấm chip ở cả hai ngôn ngữ prompt; kịch bản `.scr` của nhóm lệnh trong QA-007 chạy không lỗi prompt.
  3. PLINE vào chế độ cung: chip đổi trong cùng khung hình; Undo · U chỉ lùi bước con.
  4. Giả bản địa hóa +40% ở 1366×768: không chip nào bị cắt chữ; chip thừa vào menu "…".
  5. Đạt DoD-U, DoD-L.

### UIE-017 · Prompt và chip của nhóm lệnh sửa
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIE-015, QA-007, CMD:MOVE, CMD:COPY, CMD:ROTATE, CMD:SCALE, CMD:MIRROR, CMD:STRETCH, CMD:OFFSET, CMD:TRIM, CMD:EXTEND, CMD:FILLET, CMD:CHAMFER, CMD:ARRAYRECT, CMD:ARRAYPOLAR, CMD:BREAK, CMD:BREAKATPOINT, CMD:JOIN, CMD:LENGTHEN, CMD:ERASE, CMD:EXPLODE, CMD:MATCHPROP, CMD:DIVIDE, CMD:MEASURE, CMD:PEDIT, CMD:DRAWORDER
- **Màn hình:** UI-E10
- **Đầu vào:** 07.2 UI-E10 các dòng `MOVE`/`COPY` (Displacement; COPY thêm Mode, Array), `ROTATE`, `SCALE` (Copy, Reference), `MIRROR` (Yes/No), `STRETCH`, `OFFSET` (Through, Erase, Layer; Multiple, Undo, Exit), `TRIM`/`EXTEND` (cuTting edges, Crossing, mOde, Project, Edge, eRase, Undo), `FILLET`/`CHAMFER`, `ARRAY*`, `BREAK`, `JOIN`, `LENGTHEN`, `ERASE`, `EXPLODE`, `MATCHPROP` (Settings), `DIVIDE`/`MEASURE` (Block), `PEDIT` (Close/Open, Join, Width, Edit vertex, Fit, Spline, Decurve, Ltype gen, Reverse, Undo; Multiple), `BREAKATPOINT`, `DRAWORDER`; `docs/specs/commands/<TÊN>.md`; prototype `js/draw.js`, `js/editor.js` (grip MO, RO, SC, MI), `js/screens6.js` (mảng, DRAWORDER).
- **Đầu ra:** `app/dialogs/prompts/i18n/prompts_modify.{vi,en}.ts`; `app/dialogs/prompts/expect/modify.json`; ảnh baseline QA-011.
- **Tiêu chí nghiệm thu:**
  1. 100% prompt của 25 lệnh qua UIE-015 như UIE-016 tiêu chí 1–2.
  2. Từ khóa ẩn của grip (MO, RO, SC, MI) gõ được khi grip nóng, không hiện thành chip (đúng cờ ẩn của INT-003).
  3. PEDIT với line/arc hỏi chuyển thành polyline: chip Yes/No đúng; Multiple đổi tập prompt đúng như AutoCAD.
  4. Giả bản địa hóa +40% không cắt chữ ở 1366×768.
  5. Đạt DoD-U, DoD-L.

### UIE-018 · Prompt và chip của nhóm chú thích, block, layout, truy vấn, hệ thống
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIE-015, QA-007, CMD:TEXT, CMD:MTEXT, CMD:DIMLINEAR, CMD:DIMALIGNED, CMD:DIMANGULAR, CMD:DIMRADIUS, CMD:DIMDIAMETER, CMD:DIMCONTINUE, CMD:DIMBASELINE, CMD:DIMEDIT, CMD:DIMTEDIT, CMD:MLEADER, CMD:INSERT, CMD:BLOCK, CMD:LAYOUT, CMD:MVIEW, CMD:XCLIP, CMD:LAYISO, CMD:ATTSYNC, CMD:ATTDISP, CMD:MEASUREGEOM, CMD:AREA, CMD:LIST, CMD:ID, CMD:UNDO, CMD:AUDIT, INT-005, INT-008, INT-016
- **Màn hình:** UI-E10
- **Đầu vào:** 07.2 UI-E10 các dòng `TEXT` (Justify, Style), `MTEXT` (Height, Justify, Line spacing, Rotation, Style, Width, Columns), `DIMLINEAR` và các dim khác (Mtext, Text, Angle, Horizontal, Vertical, Rotated), `DIMEDIT`, `DIMTEDIT`, `MLEADER`, `INSERT` (Basepoint, Scale, X/Y/Z, Rotate, Explode, Repeat), `BLOCK`, `ZOOM` (INT-016), `MEASUREGEOM`, `AREA`, `LIST`/`ID`, `UNDO` (Auto, Control, BEgin, End, Mark, Back), `SELECT` (W, L, C, BOX, ALL, F, WP, CP, G, A, R, M, P, U, AU, SI; INT-008), `XCLIP`/`CLIP`, `MVIEW`, `LAYOUT`, `LAYISO`, `ATTSYNC`/`ATTDISP`, `AUDIT`, `SETVAR` (INT-005); `docs/specs/commands/<TÊN>.md`; prototype `js/annot.js` (TEXT, MTEXT, DIM…), `js/screens7.js` (block, layout, viewport), `js/cmd.js`.
- **Đầu ra:** `app/dialogs/prompts/i18n/prompts_annot_sys.{vi,en}.ts`; `app/dialogs/prompts/expect/annot_sys.json`; ảnh baseline QA-011.
- **Tiêu chí nghiệm thu:**
  1. 100% prompt của các lệnh trong nhóm qua UIE-015 như UIE-016 tiêu chí 1–2.
  2. Prompt "Chọn đối tượng" của mọi lệnh dùng chung một bộ 16 chip của SELECT; dòng lệnh đếm "Đã chọn N" theo định dạng Việt Nam.
  3. Lệnh trong suốt `'ZOOM` giữa LINE: prompt lồng ">> …" và chip của ZOOM hiện, thoát ra trả đúng chip của LINE.
  4. Giả bản địa hóa +40% không cắt chữ ở 1366×768.
  5. Đạt DoD-U, DoD-L.

### UIE-019 · Prompt và chip của lệnh P1 và tab Mở rộng
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIE-015, QA-007, CMD:REVCLOUD, CMD:SPLINEDIT, CMD:DIM, CMD:QDIM, CMD:QLEADER, CMD:CHANGE, CMD:CHPROP, CMD:ARRAYPATH, CMD:BURST, CMD:XPLODE, CMD:NCOPY, CMD:GATTE, CMD:TXTEXP, CMD:FLATTEN, CMD:EXTRIM, CMD:MPEDIT, INT-022
- **Màn hình:** UI-E10
- **Đầu vào:** 07.2 UI-E10 các dòng `REVCLOUD`, `SPLINEDIT`, `DIM` (thông minh), `QDIM`, `QLEADER`, `CHANGE`/`CHPROP`, `ARRAY*` (theo đường), `'CAL` (INT-022), `BURST`/`XPLODE`, `NCOPY`, `GATTE`, `TXTEXP`, `FLATTEN`, `EXTRIM`, `MPEDIT`; 07.1 §7.10 ("Prompt của các lệnh P1 trong tab này nằm ở UI-E10"); `docs/specs/commands/<TÊN>.md`; prototype `js/draw.js` `IMPL.REVCLOUD`, `js/annot.js` `IMPL.DIM`, `js/screens13.js` `IMPL.SPLINEDIT`, `IMPL.QDIM`, `js/screens9.js` `IMPL.QLEADER`, `js/screens10.js` `IMPL.BURST`, `IMPL.GATTE`.
- **Đầu ra:** `app/dialogs/prompts/i18n/prompts_p1.{vi,en}.ts`; `app/dialogs/prompts/expect/p1.json`; ảnh baseline QA-011.
- **Tiêu chí nghiệm thu:**
  1. 100% prompt của các lệnh trong nhóm qua UIE-015 như UIE-016 tiêu chí 1–2.
  2. `DIM` thông minh: chip đổi theo loại đối tượng đang rê (Angular, Baseline, Continue, Ordinate, aliGn, Distribute, Layer, Undo).
  3. GATTE hiện hộp xác nhận nêu số block sẽ đổi trước khi chạy (07.2 UI-E10).
  4. Đạt DoD-U, DoD-L.

## 10. Tab Tham số (UI-PAR, sau v1.0)

### UIE-020 · Tab ribbon Tham số, menu Tham số, nút suy luận ràng buộc
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** S · **Phụ thuộc:** UIA-005, UIA-009, CMD:GEOMCONSTRAINT, CMD:DIMCONSTRAINT, CMD:AUTOCONSTRAIN, CMD:DELCONSTRAINT, CMD:CONSTRAINTBAR, CMD:DCDISPLAY, CMD:PARAMETERS, INT-005, UI0-011
- **Màn hình:** UI-PAR
- **Đầu vào:** 07.1 §7.5 (ràng buộc hình học: trùng điểm, thẳng hàng, đồng tâm, cố định, song song, vuông góc, nằm ngang, thẳng đứng, tiếp xúc, trơn (G2), đối xứng, bằng nhau; tự ràng buộc; hiện/ẩn; ràng buộc kích thước: thẳng, ngang, đứng, nghiêng, bán kính, đường kính, góc, chuyển dim thành ràng buộc; quản lý: hiện/ẩn ràng buộc động, xóa ràng buộc, trình quản lý tham số, thiết lập; bật/tắt tự suy luận trên thanh trạng thái); 07.1 §7.12 (menu Tham số); 03 §Ràng buộc (P2); prototype `index.html#UI-PAR`, `js/ribbons.js` tab "Tham số", `js/screens15.js` danh mục `GT`, `DT`.
- **Đầu ra:** `app/dialogs/param/parametric_tab.json` (cho UIA-005), `app/dialogs/param/ParametricTabController.{h,cpp}` (nút bật/tắt theo vùng chọn, trạng thái hiện/ẩn của thanh ràng buộc và ràng buộc động); mục menu Tham số của workspace Cổ điển; nút suy luận trên thanh trạng thái (CONSTRAINTINFER, Ctrl+Shift+I theo prototype); 19 biểu tượng ràng buộc tự vẽ (UI0-004).
- **Tiêu chí nghiệm thu:**
  1. Mỗi nút chạy đúng lệnh bằng tên toàn cục (`_GCHORIZONTAL`, `_DCLINEAR`…) qua UI0-015; tooltip có tên lệnh tiếng Anh và F1.
  2. Split button nhớ lựa chọn gần nhất; nút suy luận phản ánh CONSTRAINTINFER trong một khung hình khi đổi bằng SETVAR.
  3. Phiên bản không có khả năng ràng buộc: tab và menu theo quy tắc cổng của UI0-011 (huy hiệu khóa trên ribbon Pro, không hiện trên ribbon Viewer).
  4. Đạt DoD-U, DoD-L.

### UIE-021 · Thanh ràng buộc và ràng buộc động trên vùng vẽ
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** UIE-020, CMD:CONSTRAINTBAR, CMD:DCDISPLAY, CMD:DIMCONSTRAINT, CMD:PARAMETERS, RND-012, RND-013, UIA-008
- **Màn hình:** UI-PAR
- **Đầu vào:** 07.1 §7.5; prototype `js/screens15.js` mục "hiển thị: ràng buộc động, thanh ràng buộc" (`labelOf`, `dimShape`, `drawShape`, `barAt`, `hot`): thanh biểu tượng đặt cạnh đối tượng, đi theo pan/zoom; rê biểu tượng tô sáng đối tượng liên quan; nhãn ràng buộc động dạng tên / giá trị / tên = biểu thức, ràng buộc tham chiếu trong ngoặc; nhấp đúp nhãn để sửa biểu thức.
- **Đầu ra:** `app/dialogs/param/ConstraintBarOverlay.{h,cpp}`, `DynConstraintOverlay.{h,cpp}` vẽ trên overlay RND-012 (không dựng lại cảnh); tô sáng qua RND-013; menu chuột phải (Xóa, Ẩn, Ẩn tất cả, Thiết lập…); sửa nhãn tại chỗ gửi lệnh của DIMCONSTRAINT/PARAMETERS.
- **Tiêu chí nghiệm thu:**
  1. 1.000 ràng buộc hiển thị: dựng và vẽ overlay ≤ 4 ms mỗi khung **[đề xuất]**; pan/zoom cảnh 1 triệu entity vẫn ≥ 60 fps trên máy tầm trung.
  2. Rê biểu tượng: đối tượng liên quan và biểu tượng cùng ràng buộc tô sáng ≤ 16 ms.
  3. Nhấp đúp nhãn `d1=600`, gõ `d1=650`, Enter: một lệnh, một mark undo; hình giải lại và vẽ xong ≤ 50 ms **[đề xuất]** với bản vẽ 200 ràng buộc (gồm thời gian giải của lệnh); biểu thức lỗi viền đỏ kèm câu giải thích, không gửi lệnh.
  4. Ràng buộc lỗi hoặc thừa có biểu tượng cảnh báo và chữ, không chỉ đổi màu.
  5. Đạt DoD-U, DoD-L.

### UIE-022 · Palette Trình quản lý tham số
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD:PARAMETERS, CMD:DIMCONSTRAINT, UIE-021, UI0-010, UI0-006
- **Màn hình:** UI-PAR
- **Đầu vào:** 07.1 §7.5 (trình quản lý tham số `PARAMETERS`); 03 §Ràng buộc (quản lý tham số, biểu thức P2); prototype `js/screens15.js` `TC.PALETTES.params` (nhóm Ràng buộc kích thước và Biến người dùng; cột Tên, Biểu thức, Giá trị, Kiểu; lọc Tất cả / Ràng buộc kích thước / Biến người dùng / Được dùng trong biểu thức / Đang lỗi; tìm; sắp xếp; sửa tại chỗ; xóa; rê dòng tô sáng ràng buộc trên vùng vẽ; chỉ xem khi không có Pro).
- **Đầu ra:** `app/palettes/params/ParametersPalette.{h,cpp}` (bảng ảo hóa có nhóm; sửa tên và biểu thức tại chỗ, Tab đổi ô, Enter áp dụng, Esc hủy; thông báo lỗi biểu thức dưới dòng; Biến người dùng mới; Xóa có cảnh báo khi tham số được biểu thức khác dùng); mỗi lần sửa một lệnh.
- **Tiêu chí nghiệm thu:**
  1. 2.000 tham số: cuộn 60 fps; lọc và tìm ≤ 16 ms mỗi phím; sắp xếp theo tên dùng thứ tự số tự nhiên (d2 trước d10).
  2. Sửa biểu thức hợp lệ: một lệnh, một mark undo, giá trị và hình cập nhật; biểu thức lỗi (vòng tham chiếu, tên chưa có, chia cho 0) hiện câu lỗi do lệnh trả về dưới dòng, không đổi bản vẽ.
  3. Xóa `d3` đang được `d5` dùng: hộp xác nhận nêu "d3 được dùng trong biểu thức của d5".
  4. Bàn phím đủ: ↑/↓, Enter/F2 sửa, Delete xóa, Esc bỏ chọn; rê dòng tô sáng ràng buộc tương ứng.
  5. Đạt DoD-U, DoD-L.

### UIE-023 · Hộp Thiết lập ràng buộc và hộp ràng buộc mâu thuẫn
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** UIE-020, CMD:GEOMCONSTRAINT, CMD:DIMCONSTRAINT, CMD:AUTOCONSTRAIN, INT-005, UI0-008
- **Màn hình:** UI-PAR
- **Đầu vào:** 07.1 §7.5 (thiết lập `CONSTRAINTSETTINGS`); prototype `js/screens15.js` `settingsDlg` (tab Hình học: loại ràng buộc hiện trên thanh, độ trong suốt thanh, hiện thanh sau khi áp ràng buộc, hiện cho đối tượng đang chọn; tab Kích thước: định dạng tên, biểu tượng khóa của ràng buộc chú thích, hiện ràng buộc động bị ẩn khi chọn; tab Tự ràng buộc: thứ tự ưu tiên, dung sai khoảng cách và góc) và hộp `pcf` (ràng buộc mâu thuẫn / thừa ràng buộc: danh sách ràng buộc liên quan, lựa chọn xử lý).
- **Đầu ra:** `app/dialogs/param/ConstraintSettingsDialog.{h,cpp}`, `ConstraintConflictDialog.{h,cpp}` (danh sách ràng buộc xung đột do lệnh trả về; Xóa ràng buộc đã chọn, Chuyển thành ràng buộc tham chiếu (với ràng buộc kích thước), Hủy; F1); ghi CONSTRAINTBARMODE, CONSTRAINTBARDISPLAY, CONSTRAINTNAMEFORMAT, DYNCONSTRAINTMODE, CONSTRAINTINFER và thiết lập tự ràng buộc.
- **Tiêu chí nghiệm thu:**
  1. Kiểm ánh xạ tự động mọi trường ↔ sysvar; đổi thiết lập thay hình thanh ràng buộc và nhãn ở khung hình kế tiếp.
  2. 20 ca thừa hoặc mâu thuẫn ràng buộc: hộp liệt kê đúng tập ràng buộc lệnh trả về; mỗi lựa chọn gửi đúng một lệnh, một mark undo; Hủy không đổi bản vẽ.
  3. Thứ tự ưu tiên tự ràng buộc đổi bằng nút Lên/Xuống và kéo-thả; AUTOCONSTRAIN dùng thứ tự mới.
  4. Đạt DoD-U, DoD-L.

## Tóm tắt theo mốc

| Mốc | Task | Số task |
|---|---|---|
| V | UIB-001, UIB-002, UIB-003, UIB-007, UIB-008, UIB-012, UIB-013, UIB-016, UIC-001, UIC-002, UIC-003, UIC-004, UIC-008, UIC-009, UIC-017, UID-019, UID-020, UID-021, UID-028 | 19 |
| VP | UIB-004 | 1 |
| EA | UIB-005, UIB-006, UIB-009, UIB-011, UIB-014, UIB-015, UIB-017, UIB-018, UIB-019, UIC-005, UIC-010, UIC-011, UIC-012, UIC-018, UIC-019, UIC-020, UID-001, UID-002, UID-003, UID-004, UID-005, UID-006, UID-008, UID-010, UID-011, UID-013, UID-015, UID-016, UID-017, UID-022, UID-023, UID-024, UID-025, UID-027, UID-031, UIE-001, UIE-002, UIE-005, UIE-006, UIE-008, UIE-011, UIE-013, UIE-015, UIE-016, UIE-017, UIE-018 | 46 |
| 1.0 | UIB-010, UIB-020, UIB-021, UIB-022, UIC-006, UIC-007, UIC-013, UIC-014, UIC-015, UIC-016, UID-007, UID-009, UID-012, UID-014, UID-018, UID-026, UID-029, UID-030, UID-032, UIE-003, UIE-004, UIE-007, UIE-009, UIE-010, UIE-012, UIE-014, UIE-019 | 27 |
| Sau | UIE-020, UIE-021, UIE-022, UIE-023 | 4 |
| **Tổng** | | **97** |
