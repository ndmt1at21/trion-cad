# UI 3 — Tính năng Việt Nam, tự động hóa, hệ thống, trang web

> Giao diện của: công cụ miễn phí CC (UI-N01–N03), Bác sĩ Font và công cụ TCVN (UI-J01–J15), markup Viewer Pro (UI-M02), tự động hóa và tùy biến (UI-K01–K13), hệ thống và tài khoản (UI-L01–L18), tab Mở rộng (UI-MR, UI-MR2).
> Thư mục mã: `app/vn/`, `app/custom/`, `app/system/`, `web/`. Căn cứ: [07.3](../07-dac-ta-ui-3-tinh-nang-vn-va-he-thong.md) (toàn bộ), [07.1](../07-dac-ta-ui-1-khung-ung-dung.md) §1, §2, §7.9, §7.10, §12, §13; prototype `design/prototype/` (`index.html#UI-xxx`, `cong-cu-mien-phi.html`, `thanh-toan.html`, `quan-tri.html`).
> Engine và lệnh thuộc module lõi (VN, LSP/CUS, PLF, RND, INT, CMD). Tệp này chỉ làm giao diện, theo README §4.2.

## 0. Ghi chú thiết kế

- **Ranh giới.** Engine và lệnh của Bác sĩ Font, công cụ TC…, LISP/DCL, giấy phép, cập nhật, TRIONBENCH thuộc module lõi (README §4.2). Task ở đây dựng màn hình, đọc dữ liệu qua snapshot (DOC-009) hoặc API đọc của task lõi. Mọi thay đổi bản vẽ đi qua lệnh (INT-002), mỗi lần áp dụng là một mark undo.
- **Lệnh mở màn hình.** Lệnh do module lõi khai báo; UI gắn "trình mở giao diện" vào lệnh qua móc của INT-001 **[đề xuất]**. `FILEDIA` = 0, tiền tố `-` hoặc `trion-console` thì chạy bản dòng lệnh của lõi. Task ở đây chỉ có dòng **Lệnh:** khi lệnh không làm gì ngoài mở màn hình của chính task (TCMARKUP, HELP, ABOUT, GRAPHICSCONFIG, ALIASEDIT, SECURITYOPTIONS, SYSVDLG, SYSVARMONITOR, TCKEYS, WSSAVE, WSSETTINGS, CUI, LISPCONSOLE, PLUGINS); khi đó task nộp kèm `docs/specs/commands/<TÊN>.md`.
- **Viewer Pro không sửa DWG** (design/README): markup ghi `.tcmk` cạnh bản vẽ, chuyển font lưu bản sao. Ghi đè tệp gốc, chèn vào bản vẽ, nhập attribute là của Pro. Ở mốc VP chỉ lưu được DWG 2004 (DWG-021) và 2018 (DWG-022); 2000, 2007, 2010, 2013 mở từ 1.0 (DWG-023), trước đó hiện mờ kèm lý do.
- **Tác vụ dài** (quét font, chuyển hàng loạt, thống kê, xuất, cài đặt) chạy qua NT-013: đóng hộp thoại vẫn chạy; tiến trình và nút hủy ở thanh trạng thái (UI0-009); xong thì có toast và một mục trong trung tâm thông báo (UIL-001).
- **Cổng phiên bản** qua UI0-011 (huy hiệu khóa, mở trang nâng cấp). Màn hình chỉ khai báo khả năng cần (PLF-002), không tự so phiên bản.
- **Kiểm thử.** Mỗi thư mục tính năng có `tests/` (đơn vị: Qt Test hoặc GoogleTest) và `tests/ui/` (kịch bản QA-011, ảnh chuẩn Tối/Sáng, 100/150/200%) **[đề xuất]**. Độ trễ đo bằng QA-011 trên máy tầm trung, tiêu chí áp cho p99.
- **Tìm kiếm không dấu** ("cua di" khớp "cửa đi") dùng so sánh của NT-012. Danh sách hơn 200 dòng ảo hóa bằng model/view (UI0-006, UI0-014).
- **Web** (`web/`) **[đề xuất]**: HTML, CSS và JavaScript thuần (ES module), không framework, không bước build. Token chép nguyên từ `design/prototype/css/tokens.css`; font IBM Plex tự host; trang tĩnh do máy chủ của PLF đặt tại Việt Nam phục vụ; không script bên thứ ba. Playwright và axe-core chỉ dùng để kiểm thử.
- **Công cụ miễn phí (CC):** hộp thoại do AutoCAD, ZWCAD, BricsCAD vẽ từ tệp `.dcl`. Tệp `.dcl` và hợp đồng khóa tile nằm ở `app/vn/cc/` **[đề xuất]**; mã LISP, lệnh và gói cài đặt là của VN-017. Nhãn tiếng Việt phải kiểm mã hóa trên từng ứng dụng chủ.
- **Định dạng:** số ở giao diện dạng Việt Nam (1.250), ngày dd/mm/yyyy; số và tọa độ trong vùng vẽ theo `UNITS` qua RGN-013. Giá, mã giảm giá, liên kết web đọc từ dữ liệu cấu hình, không viết cứng trong mã.

## 1. Công cụ miễn phí chạy trên AutoCAD, ZWCAD, BricsCAD (CC)

### UIN-001 · Bộ thành phần web dùng chung: token, kiểu, kiểm tra hợp lệ, khung kiểm thử
- **Mốc:** CC · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** —
- **Màn hình:** UI-N03, UI-L15, UI-L16
- **Đầu vào:** design/README.md (Quy ước); `design/prototype/css/tokens.css`, `css/web.css`, `css/page.css`; thành phần chung của `cong-cu-mien-phi.html`, `thanh-toan.html`, `quan-tri.html` (nút `cta`, ô `fld`, `stepper`, nhóm `opt`, bảng `wtbl`, `card`, chân trang); 07.1 §13 (tương phản, bàn phím, bản địa hóa); 07.3 §6 UI-L16 ("giao diện web dùng chung hệ thống thiết kế với ứng dụng").
- **Đầu ra:**
  - `web/shared/tokens.css` (chép bằng `web/tools/sync-tokens.mjs`), `web/shared/web.css`, `web/shared/fonts/` (IBM Plex Sans, Mono; OFL).
  - `web/shared/i18n.js`, `web/shared/i18n/vi.json`, `en.json`.
  - `web/shared/validate.js`: email; số điện thoại Việt Nam (`0xxxxxxxxx`, `+84…`); mã số thuế (10 số hoặc `10 số-3 số`).
  - `web/shared/api.js`: gọi JSON qua HTTPS, hết thời gian 15 s **[đề xuất]**, ánh xạ mã lỗi thành câu tiếng Việt.
  - `web/tests/`: Playwright (Chromium, Firefox, WebKit), ảnh chuẩn 1366×768, 1920×1080, 390×844; kiểm tương phản; axe-core. `web/README.md`: chạy cục bộ bằng máy chủ tĩnh bất kỳ.
- **Tiêu chí nghiệm thu:**
  1. Trang mẫu `web/shared/demo.html` có đủ thành phần ở Tối và Sáng; ảnh chụp so với prototype sai khác ≤ 1% diện tích **[đề xuất]**.
  2. `web/shared/tokens.css` giống từng byte với `design/prototype/css/tokens.css`; CI thất bại khi lệch.
  3. Chữ đạt tương phản ≥ 4,5:1, viền điều khiển ≥ 3:1 ở cả hai chủ đề (kiểm tự động mọi cặp token chữ/nền).
  4. 100% ca kiểm thử `validate.js` đạt: 30 email, 30 số điện thoại, 30 mã số thuế đúng và sai, có khoảng trắng, dấu chấm, gạch nối.
  5. Không tải tài nguyên ngoài tên miền của trang (Playwright chặn mạng ngoài và báo lỗi); CSS và JS dùng chung ≤ 60 KB sau gzip, chưa tính font **[đề xuất]**.
  6. axe-core không báo lỗi mức nghiêm trọng hay nặng; mọi điều khiển dùng được bằng bàn phím, focus nhìn thấy được.
  7. Đổi vi ↔ en không tải lại trang; chuỗi tiếng Việt dài hơn 40% không tràn ở 390 px.
  8. Đạt DoD-U (mục 1–3, 6, 7).
- **Ngoài phạm vi:** máy chủ, API, cổng thanh toán (PLF); trang marketing (07.3 §8).

### UIN-002 · Trang tải công cụ miễn phí và đăng ký danh sách chờ
- **Mốc:** CC · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIN-001
- **Màn hình:** UI-N03
- **Đầu vào:** 07.3 §7 UI-N03; 04 §2.3 (công cụ miễn phí, danh sách người dùng); prototype `cong-cu-mien-phi.html#n03` (trang tải, gói ZIP, ghi chú hành vi); API đăng ký `POST /api/cc/dang-ky` **[đề xuất]** (backend chưa có task neo).
- **Đầu ra:** `web/tai-ve/index.html`, `tai-ve.js`, `tai-ve.css`; hợp đồng API `web/tai-ve/api.md`; máy chủ giả `web/tests/mock/cc-api.mjs`; kịch bản `web/tests/tai-ve.spec.mjs`.
- **Tiêu chí nghiệm thu:**
  1. Bố cục như prototype: giới thiệu, chip ứng dụng chủ (AutoCAD 2024+, AutoCAD LT 2024+, ZWCAD, BricsCAD), thẻ đăng ký, thẻ FIXFONT và TCLIB, "Cài trong 3 bước", bảng "Chạy trên", câu hỏi thường gặp, chân trang ghi chú nhãn hiệu.
  2. Đăng ký bằng Zalo hoặc Email (nút chuyển). Số điện thoại nhận `0xxxxxxxxx` hoặc `+84…`; email kiểm dạng cơ bản; lỗi hiện ngay dưới ô khi rời ô hoặc bấm gửi, gắn `aria-describedby`. Ô "lĩnh vực" không bắt buộc; câu cam kết dùng dữ liệu nằm cạnh nút gửi.
  3. Gửi thành công: "Đã gửi link tải tới …", nút tải trực tiếp kèm dung lượng và mã SHA-256, nút "Đăng ký bằng số khác". Lỗi mạng hoặc bị giới hạn tần suất: câu giải thích, nút thử lại, dữ liệu đã nhập giữ nguyên.
  4. Ở 390 px: cột đăng ký xuống dưới phần giới thiệu, nút tải rộng hết hàng; không cuộn ngang từ 320 đến 1920 px.
  5. Tải lần đầu ≤ 1,5 s trên mạng 4G mô phỏng (1,6 Mbps, 150 ms) **[đề xuất]**; không script theo dõi.
  6. Bản English đầy đủ; mọi kịch bản chạy được với máy chủ giả.
  7. Đạt DoD-U (mục 1–3, 6, 7).
- **Ngoài phạm vi:** lưu đăng ký, gửi tin qua Zalo OA hoặc email (backend); tạo gói ZIP (VN-017).

### UIN-003 · Bác sĩ Font bản LISP: hộp thoại DCL quét và chuyển
- **Mốc:** CC · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** —
- **Màn hình:** UI-N01
- **Đầu vào:** 07.3 §7 UI-N01; 07.3 §1 UI-J02 khu vực 5 (cùng thuật ngữ); prototype `cong-cu-mien-phi.html#n01` (hộp "Bác sĩ Font · TrionCAD (miễn phí)", hộp "Chuyển sang Unicode", bốn trạng thái, khối mã DCL mẫu); đặc tả DCL công khai trong AutoLISP Developer's Guide của Autodesk.
- **Đầu ra:**
  - `app/vn/cc/fixfont.dcl`: dialog `fixfont_scan` và `fixfont_convert`.
  - `app/vn/cc/fixfont-keys.md`: hợp đồng với VN-017 (key, kiểu giá trị, điều kiện bật/tắt, chuỗi `errtile`, màu ACI của thanh tỉ lệ).
  - `app/vn/cc/tests/fixfont-demo.lsp`: LISP điền dữ liệu giả cho từng trạng thái để chụp ảnh; `app/vn/cc/tests/check-keys.py`.
- **Tiêu chí nghiệm thu:**
  1. Hộp quét: Phạm vi (Cả bản vẽ / Vùng chọn; nút "Chọn đối tượng <" chỉ bật với Vùng chọn); Kết quả (số font không có trên máy, số kiểu chữ); danh sách Font và bảng mã; thanh tỉ lệ là tile `image` có chú giải bằng chữ cạnh từng màu; dòng chân dẫn về TrionCAD Viewer và nút "Đăng ký nhận bản dùng thử…"; nút Quét, Chuyển sang Unicode…, Đóng, Trợ giúp.
  2. Hộp chuyển: Bảng mã nguồn (Tự nhận diện / Ép TCVN3 / Ép VNI); Font Unicode đích (Theo font gốc, Times New Roman, Arial, Tahoma, Segoe UI) kèm dòng gợi ý ánh xạ; ô Cập nhật kiểu chữ sang font Unicode, ô Gồm block lồng và attribute; dòng "Sẽ chuyển N đối tượng: a TCVN3, b VNI. Gõ U để hoàn tác."; nút < Quay lại, Chuyển, Hủy.
  3. `mnemonic` không trùng trong cùng hộp; `is_default` là Quét và Chuyển; `is_cancel` là Đóng và Hủy.
  4. `errtile` dùng cho "Vùng chọn không có chữ" và lỗi khác, không đóng hộp; trạng thái sạch dùng đúng câu của UI-J02: "Bản vẽ dùng Unicode và đủ font."
  5. Rộng hộp ≤ 560 px ở 100% **[đề xuất]**; vừa màn hình 1366×768 ở 150% (đo ở UIN-005).
  6. `check-keys.py` so 100% key trong `.dcl` với hợp đồng; CI chặn khi lệch.
  7. Chỉ dùng tile chuẩn của DCL; không biểu tượng hay hình ảnh của Autodesk.
  8. Đạt DoD-U mục 3 (chuỗi tiếng Việt, bàn phím, không dùng màu làm tín hiệu duy nhất).
- **Ngoài phạm vi:** quét, nhận diện, chuyển mã, lệnh FIXFONT bản LISP, đóng gói (VN-017).

### UIN-004 · Thư viện TCVN bản LISP: hộp thoại DCL duyệt và chèn
- **Mốc:** CC · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** —
- **Màn hình:** UI-N02
- **Đầu vào:** 07.3 §7 UI-N02; prototype `cong-cu-mien-phi.html#n02` (hộp "Thư viện TCVN · TrionCAD (miễn phí)", ghi chú: tìm không dấu, danh mục cha/con, nhấp đúp để chèn, hai loại tỉ lệ, DIMSCALE, INSUNITS); đặc tả DCL công khai (tile `image`, `slide_image`).
- **Đầu ra:** `app/vn/cc/tclib.dcl` (dialog `tclib_browse`); `app/vn/cc/tclib-keys.md`; `app/vn/cc/tests/tclib-demo.lsp` (danh mục, slide mẫu).
- **Tiêu chí nghiệm thu:**
  1. Ô Tìm (`edit_box`, `allow_accept`), list_box Danh mục (con thụt vào), list_box Ký hiệu, tile `image` Xem trước tỉ lệ 4:3 nền màu −2, popup_list "Tỉ lệ bản vẽ", dòng chân dẫn về TrionCAD, nút Chèn (mặc định), Đóng, Trợ giúp.
  2. Hợp đồng nêu: lọc khi gõ (`$reason` 1), nhấp đúp ký hiệu (`$reason` 4) là Chèn, xóa ô tìm thì về danh mục đang chọn; danh sách rỗng hiện một dòng hướng dẫn.
  3. Danh sách 2.000 ký hiệu cuộn được; slide 320×240 hiện đủ trong tile ở 100% và 150% (đo ở UIN-005).
  4. `check-keys.py` khớp 100% key với hợp đồng.
  5. Đạt DoD-U mục 3.
- **Ngoài phạm vi:** LISP tìm kiếm, chèn, quy đổi đơn vị; nội dung block và slide (VN-017, VN-008).

### UIN-005 · Kiểm hộp thoại công cụ miễn phí trên AutoCAD, AutoCAD LT, ZWCAD, BricsCAD
- **Mốc:** CC · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIN-003, UIN-004
- **Màn hình:** UI-N01, UI-N02
- **Đầu vào:** 07.3 §7 ("Kiểm tra trên: AutoCAD (bản đầy đủ và LT 2024+), ZWCAD, BricsCAD"); tệp `.dcl` và LISP mẫu của UIN-003, UIN-004; máy Windows tự host của NT-002 có cài bốn ứng dụng chủ, bản quyền hoặc dùng thử của đội **[đề xuất]**.
- **Đầu ra:** `app/vn/cc/tests/host-shots/`: kịch bản `.scr` cho từng ứng dụng chủ, script PowerShell chụp cửa sổ hộp thoại (Win32 `PrintWindow`) và gửi phím, ảnh chuẩn đã duyệt, báo cáo HTML; job CI chạy theo yêu cầu và hằng tuần.
- **Tiêu chí nghiệm thu:**
  1. Mỗi ứng dụng chủ × 8 trạng thái hộp (6 của FIXFONT, 2 của TCLIB) × 100% và 150%: hộp mở được, không lỗi DCL, ảnh so với ảnh chuẩn sai khác ≤ 0,5% pixel **[đề xuất]**.
  2. Nhãn tiếng Việt có dấu hiện đúng trên cả bốn ứng dụng chủ; cách mã hóa chọn được (tệp UTF-8, chuỗi `\U+XXXX`, hay `set_tile` lúc chạy) ghi vào `fixfont-keys.md` kèm ảnh chứng minh.
  3. Không nhãn nào bị cắt; hộp nằm trọn trong 1366×768 ở 150%.
  4. Bàn phím: Tab đi qua mọi điều khiển theo thứ tự; Enter và Esc đúng nút mặc định và nút hủy (gửi phím rồi chụp lại).
  5. Cả job chạy ≤ 10 phút trên một máy.
  6. Đạt DoD-U mục 3.
- **Ngoài phạm vi:** kiểm logic chuyển mã và chèn trên ứng dụng chủ (VN-017).

## 2. Bác sĩ Font (UI-J01–J04)

### UIJ-001 · Banner Bác sĩ Font khi mở bản vẽ và chỉ báo trên thanh trạng thái
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-004, UI0-009, UI0-011, UIA-009
- **Màn hình:** UI-J01
- **Đầu vào:** 07.3 §1 UI-J01; 07.1 §10 (thanh trạng thái), §12 (banner); prototype `index.html#UI-J01`, `js/palettes.js` (`TC.fontDoctor.onOpen`, `FD.summary`), `js/shell.js` (chip `#sb-font`); API VN-004: kết quả quét (font thiếu, số đối tượng theo bảng mã), tiến trình quét, sự kiện quét xong.
- **Đầu ra:** `app/vn/fontdoctor/FontDoctorBanner.{h,cpp}`, `FontStatusChip.{h,cpp}`, `FontDoctorUiSettings.{h,cpp}` (khóa `fontdoctor.showMode`, `fontdoctor.mutedFiles`); `app/vn/fontdoctor/tests/`, `tests/ui/j01-*`.
- **Tiêu chí nghiệm thu:**
  1. Bản vẽ có font thiếu hoặc chữ TCVN3/VNI: banner hiện khi VN-004 báo quét xong, nội dung theo mẫu "Bản vẽ dùng 3 font không có trên máy và 250 đối tượng chữ mã TCVN3. TrionCAD đã tự hiển thị đúng." (có VNI thì thêm ", 46 chữ VNI"); số theo dạng 1.250.
  2. Nút: Xem chi tiết (mở palette UI-J02), Chuyển sang Unicode (ở Viewer mang huy hiệu Viewer Pro, bấm mở trang nâng cấp qua UI0-011), Không nhắc lại với tệp này, Đóng.
  3. Banner không chặn thao tác: khi banner hiện, pan/zoom vẫn ≥ 60 fps trên BENCH-STD 1 triệu entity; phần việc của banner trên luồng UI ≤ 2 ms mỗi lần mở bản vẽ.
  4. Đóng banner, hoặc chọn "chỉ hiện chỉ báo": chip "Font: 3 thay thế" vẫn ở thanh trạng thái, tooltip nêu số font thay thế và số chữ mã cũ; bấm chip mở palette.
  5. "Không nhắc lại với tệp này" ghi theo đường dẫn chuẩn hóa, còn hiệu lực sau khi khởi động lại; tệp đó vẫn có chip. Đổi tên hoặc chuyển tệp thì banner hiện lại.
  6. Bản vẽ không có vấn đề: không banner, không chip. Đang quét bản vẽ lớn: chip "Đang quét font" kèm tiến trình, banner chờ tới khi quét xong.
  7. Mỗi tab tệp có banner riêng; chuyển tab đổi banner và chip ≤ 100 ms.
  8. Trên kho 1.000 tệp spike (QA-001): số font thiếu và số đối tượng theo bảng mã trên banner và chip khớp 100% kết quả của VN-004.
  9. Đạt DoD-U, DoD-L.

### UIJ-002 · Palette Bác sĩ Font: khung, tab Tóm tắt và danh sách kiểu chữ
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIJ-001, VN-004, UI0-010, UI0-006, UIA-008
- **Màn hình:** UI-J02
- **Đầu vào:** 07.3 §1 UI-J02 (khu vực 1, 3; trạng thái); 07.1 §7.9 (tab Công cụ VN › Font); prototype `index.html#UI-J02`, `js/palettes.js` `P.fontdoc.render` (tab `sum`), `encBar`; API VN-004 (tóm tắt; kiểu chữ: tên, font, có/thiếu, bảng mã, số đối tượng; độ tin cậy nhận diện); API tô sáng và zoom của UIA-008.
- **Đầu ra:** `app/vn/fontdoctor/FontDoctorPalette.{h,cpp}` (khung 4 tab Tóm tắt, Thay thế, Xem trước, Chuyển; nút bánh răng), `SummaryTab.{h,cpp}`, `StyleListModel.{h,cpp}`; trình mở palette cho lệnh FIXFONT; tests.
- **Tiêu chí nghiệm thu:**
  1. Tóm tắt: số font thiếu, số kiểu chữ, số chữ mã cũ; thanh tỉ lệ Unicode / TCVN3 / VNI / Không rõ có nhãn chữ và số cho từng phần (không chỉ dựa vào màu); câu độ tin cậy nhận diện lấy từ VN-004.
  2. Danh sách kiểu chữ: Kiểu, Font (kèm "thiếu"), Bảng mã, Số đối tượng; sắp theo cột; bấm một dòng thì tô sáng mọi đối tượng dùng kiểu đó, kể cả trong block lồng, dòng lệnh báo "Tô sáng N đối tượng dùng kiểu chữ X". Tô sáng hiện ≤ 100 ms với ≤ 100.000 đối tượng, ≤ 300 ms với 1 triệu (BENCH-STD text dày) **[đề xuất]**; tập đối tượng tính ở nền trên snapshot.
  3. Không có vấn đề: trạng thái "Bản vẽ dùng Unicode và đủ font" kèm biểu tượng đạt. Đang quét: thanh tiến trình trong palette, các tab đã có dữ liệu vẫn dùng được.
  4. Mở từ banner, chip, ribbon (Công cụ VN › Font), lệnh FIXFONT: tới khung đầu có nội dung ≤ 150 ms.
  5. Đổi bản vẽ hiện hành: nội dung đổi theo, phần đồng bộ ≤ 16 ms.
  6. Dock trái/phải, nổi, tự ẩn theo UI0-010; nhớ tab cuối cùng.
  7. Đạt DoD-U, DoD-L.

### UIJ-003 · Palette Bác sĩ Font: bảng thay thế font, nhập và xuất bảng
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIJ-002, RGN-006, NT-010
- **Màn hình:** UI-J02
- **Đầu vào:** 07.3 §1 UI-J02 khu vực 2; 07.2 UI-D10 tab Tệp (bảng ánh xạ font); prototype `js/palettes.js` `P.fontdoc.render` (tab `map`, menu `data-sub`, `data-io`); API FontResolver của RGN-006 (font thay thế, gợi ý tự động, áp theo phiên hoặc theo hồ sơ); định dạng FONTMAP `.fmp` công khai của AutoCAD.
- **Đầu ra:** `app/vn/fontdoctor/SubstitutionTab.{h,cpp}`; `FontMapFile.{h,cpp}`: đọc/ghi `.tcfm` **[đề xuất]** (UTF-8, mỗi dòng: font gốc, loại, bảng mã, font thay thế, phạm vi), đọc `.fmp` hai cột; tests, fuzz target `fontmapfile_fuzz.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Bảng: Font trong bản vẽ, Loại (SHX / TTF / Big Font), Trạng thái (có / thiếu), Bảng mã, Font thay thế (danh sách thả xuống; mục tự động ghi "gợi ý"), Áp dụng cho (Phiên này / Mọi bản vẽ).
  2. Đổi một dòng: chữ dùng font đó vẽ lại bằng font mới, khung hình đầu sau khi đổi ≤ 0,5 s trên cảnh text dày BENCH-STD; chỉ entity liên quan được regen (RGN-003 gia tăng); UI không khựng quá 1 khung.
  3. "Mọi bản vẽ" lưu vào hồ sơ người dùng (NT-010), áp cho bản vẽ mở sau; "Phiên này" mất khi đóng ứng dụng.
  4. Xuất `.tcfm`; nhập `.tcfm` hoặc `.fmp`; nhập rồi xuất lại cho tệp giống hệt. Dòng lỗi trong tệp nhập bị bỏ qua và liệt kê (số dòng, lý do), không chặn các dòng còn lại; tệp > 1 MB bị từ chối có thông báo.
  5. Fuzz `FontMapFile` 1 giờ không lỗi.
  6. Đạt DoD-U, DoD-L, DoD-C (mục 2, 3, 7 cho `FontMapFile`).

### UIJ-004 · Palette Bác sĩ Font: tab Xem trước chữ gốc và chữ đúng
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** UIJ-002, VN-004, UIA-008
- **Màn hình:** UI-J02
- **Đầu vào:** 07.3 §1 UI-J02 khu vực 4; prototype `js/palettes.js` (tab `prev`, `samples`, `data-pm`, `data-zoom`), `TC.rawText`; API VN-004: mẫu chữ theo bảng mã kèm độ tin cậy, chuỗi "gốc" (như AutoCAD thấy khi thiếu font), chế độ vẽ gốc/đúng cho cả vùng vẽ.
- **Đầu ra:** `app/vn/fontdoctor/PreviewTab.{h,cpp}`; tests.
- **Tiêu chí nghiệm thu:**
  1. Tối đa 10 mẫu khác nhau trong bản vẽ (kể cả trong block lồng, attribute); mỗi dòng: bảng mã, độ tin cậy %, font, chữ gốc, chữ đúng; bấm dòng thì zoom tới đối tượng, đổi Model/layout khi cần.
  2. Nút "Hiển thị đúng / Hiển thị gốc" áp cho cả vùng vẽ, dòng lệnh báo chế độ hiện tại. Nút đổi trạng thái ≤ 100 ms; khung hình đầu ở chế độ mới ≤ 0,5 s trên cảnh text dày BENCH-STD.
  3. Chế độ "gốc" chỉ đổi hiển thị, không đổi dữ liệu (hash snapshot trước và sau như nhau); đóng palette hoặc đổi bản vẽ thì trở về "đúng".
  4. Chuỗi gốc và chuỗi đúng hiện khớp 100% chuỗi VN-004 trả về trên 50 mẫu của bộ VN-003.
  5. Đạt DoD-U, DoD-L.

### UIJ-005 · Thiết lập Bác sĩ Font
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** UIJ-001, VN-004, UI0-008
- **Màn hình:** UI-J04
- **Đầu vào:** 07.3 §1 UI-J04; 07.2 UI-D10 tab Tệp; prototype `index.html#UI-J04`, `js/screens2.js` `SC.fontDoctorSettings` (hộp `fdset` 540); biến `TCFONTDOC` trong `js/screens14.js`; tùy chọn của VN-004 (tự quét, ngưỡng tin cậy, bảng mặc định, thư mục font tin cậy).
- **Đầu ra:** `app/vn/fontdoctor/FontDoctorSettingsPage.{h,cpp}` (dùng trong hộp riêng và nhúng được vào UI-D10 › Tệp); tests.
- **Tiêu chí nghiệm thu:**
  1. Ô "Tự quét khi mở bản vẽ"; thanh trượt ngưỡng tin cậy 50–100% (bước 1, hiện giá trị); bảng thay thế mặc định (Mặc định TrionCAD / bảng của công ty `.tcfm`); thư mục font tin cậy (Duyệt…); "Khi có vấn đề font": Hiện banner trên vùng vẽ / Chỉ hiện chỉ báo trên thanh trạng thái; dòng giải thích ngưỡng.
  2. Lưu áp dụng cho bản vẽ mở sau, không khởi động lại; Hủy không đổi gì.
  3. Tắt tự quét: mở bản vẽ không quét, không banner; palette có nút "Quét bản vẽ này" **[đề xuất]**.
  4. Mở từ bánh răng của palette và từ Tùy chọn › Tệp cho cùng một trang, cùng giá trị.
  5. Đạt DoD-U, DoD-L.

### UIJ-006 · Bác sĩ Font: tab Chuyển và hộp Xem trước thay đổi
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIJ-002, VN-005, CMD:FIXFONT, UI0-011
- **Màn hình:** UI-J02
- **Đầu vào:** 07.3 §1 UI-J02 khu vực 5; 07.1 §2 (Viewer Pro: chuyển đổi và lưu bản sao); prototype `index.html#UI-J02` ở bản Viewer Pro, `js/palettes.js` (tab `conv`), `js/screens4.js` `SC.fontPreview` (hộp `fdprev` 820×560), `legacyRows`; API VN-005: tùy chọn chuyển, danh sách thay đổi dự kiến (trước → sau, độ tin cậy); tùy chọn dòng lệnh của FIXFONT.
- **Đầu ra:** `app/vn/fontdoctor/ConvertTab.{h,cpp}`, `ConvertOptionsWidget.{h,cpp}` (dùng lại ở UIJ-009), `ChangePreviewDialog.{h,cpp}`; tests.
- **Tiêu chí nghiệm thu:**
  1. Phạm vi: Cả bản vẽ / Vùng chọn / Layout này. Ô chọn: block lồng, attribute, chữ kích thước, bảng, field. Bảng mã nguồn: Tự nhận diện / Ép TCVN3 / Ép VNI. Font Unicode đích cho từng nhóm (mặc định TCVN3 → TrionVN-Times, VNI → TrionVN-Arial). Ô chọn: cập nhật kiểu chữ, xử lý chữ hoa (`.VnTimeH`), giữ định dạng MTEXT. Độ tin cậy thấp: Hỏi từng cái / Bỏ qua / Vẫn chuyển.
  2. Viewer: tab hiện khóa kèm câu "Chuyển đổi có trong Viewer Pro (lưu bản sao) và Pro (sửa trực tiếp). Viewer vẫn hiển thị đúng chữ."; nút mang huy hiệu, bấm mở trang nâng cấp. Viewer Pro: dòng "Viewer Pro lưu kết quả thành bản sao, không sửa tệp gốc".
  3. Xem trước thay đổi: tiêu đề "N đối tượng sẽ đổi · k độ tin cậy thấp"; cột Đối tượng (Text, Attribute TAG, Text trong block · tên block), Bảng mã, Trước, Sau, Tin cậy; tìm không dấu; bấm dòng thì zoom, đổi không gian khi cần. Danh sách ảo hóa: 200.000 dòng tới khung đầu ≤ 150 ms, cuộn 60 fps.
  4. Danh sách dự kiến tính ở nền; quá 2 s thì hiện tiến trình và nút hủy; đổi tùy chọn thì tính lại, lần tính cũ hủy ≤ 50 ms.
  5. Danh sách trước → sau khớp 100% kết quả VN-005 trên bộ 5.000 mẫu có nhãn (VN-003).
  6. Mỗi tùy chọn trên tab ánh xạ đúng một tùy chọn dòng lệnh của FIXFONT: chạy lại cùng tùy chọn bằng `.scr` cho cùng kết quả.
  7. Đạt DoD-U, DoD-L.

### UIJ-007 · Bác sĩ Font: lưu bản sao, báo cáo CSV, xác nhận từng chữ độ tin cậy thấp
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIJ-006, VN-005, CMD:FIXFONT, NT-013
- **Màn hình:** UI-J02
- **Đầu vào:** 07.3 §1 UI-J02 khu vực 5 (Chuyển đổi, Xuất báo cáo CSV, độ tin cậy thấp); prototype `js/screens4.js` `SC.fontConvert` (hộp `fdconv` 560), `openCopy`; design/README ("Viewer Pro không sửa DWG"); §0 tệp này (định dạng lưu theo bộ ghi DWG-021, DWG-022, DWG-023).
- **Đầu ra:** `app/vn/fontdoctor/SaveCopyDialog.{h,cpp}`, `LowConfidenceReview.{h,cpp}` **[đề xuất]**, `ConversionReport.{h,cpp}` (CSV UTF-8 có BOM); tests.
- **Tiêu chí nghiệm thu:**
  1. Hộp "Chuyển sang Unicode và lưu bản sao": tóm tắt (N đối tượng, bảng mã, font đích); đường dẫn mặc định `<tên>_unicode.dwg` cạnh tệp gốc, Duyệt…; Định dạng (Giữ nguyên khi có bộ ghi cho phiên bản đó, 2018, 2004; 2013, 2010, 2007 hiện mờ "có từ bản 1.0" ở mốc VP); ô "Mở bản sao sau khi chuyển"; nút Xem trước thay đổi, Hủy, Chuyển và lưu bản sao.
  2. Tệp gốc không đổi một byte (so hash trước và sau); bản sao mở trong TrionCAD không còn chữ mã cũ, Bác sĩ Font báo "Bản vẽ dùng Unicode và đủ font".
  3. Chuyển chạy nền, tiến trình trong hộp và ở thanh trạng thái; hủy phản hồi ≤ 50 ms và không để lại tệp dở (ghi tạm rồi đổi tên). Xong: toast "Đã lưu bản sao X: N đối tượng chữ sang Unicode. Tệp gốc giữ nguyên." kèm "Mở bản sao".
  4. Trùng tên tệp đích: hỏi Ghi đè / Tự đổi tên thành `(2)`; thư mục chỉ đọc hoặc ổ đầy: hộp lỗi nêu lý do, bản vẽ đang mở không bị ảnh hưởng.
  5. "Hỏi từng cái": bảng nổi zoom tới từng chữ, hiện chữ gốc, cách đọc TCVN3, cách đọc VNI; nút Dùng TCVN3, Dùng VNI, Giữ nguyên, Áp dụng cho các chữ cùng kiểu, Dừng; đếm "k/n" **[đề xuất]**.
  6. Báo cáo CSV: mỗi đối tượng một dòng (handle, loại, không gian, bảng mã nguồn, độ tin cậy, chữ trước, chữ sau, font đích, kết quả); Excel mở đúng tiếng Việt.
  7. Đạt DoD-U, DoD-L.

### UIJ-008 · Bác sĩ Font: chuyển trực tiếp trong bản vẽ (Pro)
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** UIJ-007, CMD:FIXFONT, DOC-008
- **Màn hình:** UI-J02
- **Đầu vào:** 07.3 §1 UI-J02 (EA: chuyển trực tiếp trong bản vẽ); prototype `index.html#UI-J02` ở bản Pro, `js/screens8.js` `SC.fontConvert` bản Pro (hộp `fdconvp` 560).
- **Đầu ra:** `app/vn/fontdoctor/InPlaceConvertDialog.{h,cpp}`; tests.
- **Tiêu chí nghiệm thu:**
  1. Hộp liệt kê các kiểu chữ mã cũ: Kiểu chữ, Bảng mã, Font hiện tại, Đổi sang, Số chữ; lựa chọn "Sửa trực tiếp bản vẽ đang mở" (mặc định) / "Lưu thành bản sao, giữ tệp gốc" (mở hộp của UIJ-007).
  2. Chuyển trực tiếp chạy qua lệnh FIXFONT: đúng một mark undo; Ctrl+Z trả bản vẽ về trạng thái trước (hash snapshot như ban đầu); toast có nút Hoàn tác.
  3. Sau khi chuyển: banner Bác sĩ Font tắt, chip trên thanh trạng thái cập nhật ≤ 100 ms, palette về trạng thái "đủ font" nếu không còn vấn đề.
  4. Bản vẽ không còn chữ mã cũ: không mở hộp, chỉ báo "Bản vẽ không còn chữ mã cũ".
  5. Đạt DoD-U, DoD-L.

### UIJ-009 · Chuyển font hàng loạt nhiều tệp (FIXFONTBATCH)
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** UIJ-006, VN-006, NT-013, UI0-009, UI0-006
- **Màn hình:** UI-J03
- **Đầu vào:** 07.3 §1 UI-J03; 07.1 §12 (toast "Chuyển font xong 48/50 tệp — 2 lỗi [Xem]"); prototype `index.html#UI-J03`, `js/screens4.js` `SC.fontBatch` (hộp `fb`, chế độ `font`, cùng khung với UI-B14), `fbReport`; API VN-006: hàng đợi tệp, tiến trình từng tệp, tạm dừng/tiếp tục/hủy, kết quả và cảnh báo từng tệp.
- **Đầu ra:** `app/vn/fontbatch/FontBatchDialog.{h,cpp}`; `BatchFileJobPanel.{h,cpp}` (bảng tệp, chân hộp tiến trình; dùng lại được cho UI-B14); `BatchReportWriter.{h,cpp}`; tests, `tests/ui/j03-*`.
- **Tiêu chí nghiệm thu:**
  1. Thêm tệp…, Thêm thư mục… (ô "Gồm thư mục con"), lọc `*.dwg; *.dxf`, Xóa danh sách; bảng: chọn, Tệp (tên, thư mục), Dung lượng, Chữ mã cũ (TCVN3/VNI, đọc trước ở nền), Trạng thái. Quét thư mục 10.000 tệp không chặn UI, dòng hiện dần.
  2. Tùy chọn chuyển dùng lại UIJ-006; độ tin cậy thấp chỉ có "Bỏ qua, ghi vào báo cáo" / "Vẫn chuyển", kèm câu "Chạy hàng loạt ở nền nên không hỏi từng cái".
  3. Đầu ra: ghi đè tệp gốc và tạo `.bak` (chỉ Pro; Viewer Pro hiện khóa) / lưu sang thư mục khác giữ cấu trúc / thêm hậu tố (mặc định `_unicode`, hiện tên mẫu); định dạng Giữ nguyên / 2018 / 2013 / 2010 / 2007 / 2004 theo bộ ghi có ở mốc (§0; tệp không ghi được ở định dạng gốc thì lưu 2018 và ghi vào báo cáo); ô "Dọn regapps".
  4. Chạy nền nhiều luồng theo số nhân (hiện số luồng); Tạm dừng / Tiếp tục / Hủy phản hồi ≤ 100 ms. Đóng hộp vẫn chạy (toast "Chuyển font vẫn chạy ở nền"), tiến trình và nút hủy ở thanh trạng thái; mở lại hộp thấy đúng trạng thái.
  5. Trạng thái từng tệp: Chờ, Đang chuyển %, Xong (số đối tượng), Cảnh báo (k đối tượng bỏ qua), Bỏ qua (đã là Unicode), Lỗi (lỗi cấu trúc: gợi ý RECOVER; tệp bị khóa `.dwl`; ổ đầy). Một tệp lỗi không dừng các tệp khác.
  6. Xong: dòng "Xong a/n tệp: m đối tượng chữ chuyển sang Unicode. e lỗi, w cảnh báo, s bỏ qua."; nút Báo cáo CSV (mỗi tệp một dòng, kèm đối tượng bỏ qua), Mở thư mục kết quả, Chạy lại; toast lỗi ở lại tới khi đóng, có nút Xem.
  7. Lưu thiết lập thành mẫu (tên không rỗng, không trùng, lỗi dưới ô).
  8. Bảng 10.000 dòng cuộn 60 fps; trạng thái cập nhật theo lô ≤ 10 lần/giây, phần đồng bộ ≤ 16 ms.
  9. Đạt DoD-U, DoD-L.

## 3. Thông báo, cập nhật, sự cố, hỗ trợ, quyền riêng tư (UI-L06–L09, L14, L17)

### UIL-001 · Trung tâm thông báo
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** NT-013, NT-010, UI0-009, UIA-009
- **Màn hình:** UI-L14
- **Đầu vào:** 07.3 §6 UI-L14; 07.1 §12 (toast vào trung tâm thông báo); prototype `index.html#UI-L14`, `js/screens2.js` `SC.notifications` (popup từ chuông `#sb-bell`), `SC.tasks`; sự kiện tác vụ nền xong hoặc lỗi của NT-013.
- **Đầu ra:** `app/system/notifications/NotificationCenter.{h,cpp}` (kho thông báo, lưu qua phiên, API đăng thông báo cho task khác), `NotificationPopup.{h,cpp}`, `NotificationBell.{h,cpp}`; tests.
- **Tiêu chí nghiệm thu:**
  1. Popup từ chuông: tiêu đề "Thông báo", "Đánh dấu đã đọc", bánh răng chọn loại muốn nhận (Cập nhật, Giấy phép, Tác vụ nền xong hoặc lỗi, Mẹo, Tin cộng đồng); lọc Tất cả / Tác vụ / Cập nhật / Giấy phép / Mẹo.
  2. Mục: biểu tượng theo loại, nội dung, thời gian ("5 phút trước" hoặc dd/mm/yyyy), chấm chưa đọc kèm chữ cho trình đọc màn hình, nút xóa; bấm mục chạy hành động gắn kèm (mở hộp, zoom, mở thư mục).
  3. Toast loại tác vụ, cập nhật, giấy phép tự vào trung tâm; chuông hiện số chưa đọc; loại đã tắt nhận thì không vào trung tâm, không toast.
  4. Lưu 200 mục gần nhất qua phiên **[đề xuất]**; trống thì "Chưa có thông báo."
  5. Bàn phím: mũi tên di chuyển, Enter chạy hành động, Delete xóa, Esc đóng và trả focus về chuông.
  6. Mở popup ≤ 100 ms.
  7. Đạt DoD-U, DoD-L.

### UIL-002 · Cập nhật: thông báo, chi tiết, cài đặt, hết quyền cập nhật
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIL-001, PLF-006, UI0-008
- **Màn hình:** UI-L06
- **Đầu vào:** 07.3 §6 UI-L06; 07.2 UI-D10 tab Hệ thống (kênh, tự kiểm tra); prototype `index.html#UI-L06`, `js/screens2.js` `SC.update` (toast, hộp `upd` 600); API PLF-006 (feed ký số, kênh, tải nền, kiểm chữ ký, quyền cập nhật, cài khi đóng ứng dụng).
- **Đầu ra:** `app/system/update/UpdateDialog.{h,cpp}`, `UpdateNotifier.{h,cpp}`, `UpdateChannelSettings.{h,cpp}` (nhúng được vào UI-D10 › Hệ thống); tests.
- **Tiêu chí nghiệm thu:**
  1. Có bản mới: toast ở lại "TrionCAD 1.0.3 đã có: …" kèm "Xem chi tiết", và một mục trong trung tâm thông báo.
  2. Hộp chi tiết: phiên bản, kênh (Ổn định / Beta), bản đang dùng, dung lượng, "đã kiểm tra chữ ký số", ghi chú phát hành tiếng Việt (Markdown đơn giản); nút Cài ngay / Cài khi đóng ứng dụng / Nhắc sau.
  3. Cài ngay: tải ở nền có tiến trình và Hủy; tải xong đóng ứng dụng theo luồng của QUIT (bản vẽ chưa lưu được hỏi), cài, mở lại các bản vẽ đã mở.
  4. Hết quyền cập nhật (vĩnh viễn): vẫn báo bản mới, ghi "Bản 1.3 cần gia hạn cập nhật" kèm ngày hết quyền; nút chính "Gia hạn cập nhật"; bản đang dùng không bị ảnh hưởng.
  5. Kênh và ô tự kiểm tra có hiệu lực ở lần kiểm tra kế tiếp.
  6. Khi rảnh không tốn CPU: không polling dày, lịch kiểm tra theo PLF-006 (đo bằng QA-011 1 giờ ở trạng thái rảnh: ~0% CPU).
  7. Đạt DoD-U, DoD-L.
- **Ngoài phạm vi:** quay về bản trước (P2).

### UIL-003 · Báo lỗi sau sự cố
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** PLF-007, UI0-008
- **Màn hình:** UI-L07
- **Đầu vào:** 07.3 §6 UI-L07; prototype `index.html#UI-L07`, `js/screens2.js` `SC.crash` (hộp `crash` 600); API PLF-007 (phát hiện lần chạy trước bị crash, gói báo lỗi, nội dung sẽ gửi, gửi, tự gửi, hàng đợi khi mất mạng).
- **Đầu ra:** `app/system/crash/CrashReportDialog.{h,cpp}` (có chỗ gắn nút "Mở Trình phục hồi bản vẽ" do UI-B07 cung cấp); tests.
- **Tiêu chí nghiệm thu:**
  1. Lần mở sau crash: tiêu đề "TrionCAD đã đóng bất thường lần trước", lời xin lỗi; ở Viewer ghi rõ không có dữ liệu cần phục hồi; ở bản Pro có chỗ cho nút mở trình phục hồi.
  2. Mô tả, email (không bắt buộc); "Đính kèm bản vẽ đang mở (tên, dung lượng)" mặc định tắt; "Xem nội dung sẽ gửi" hiện đúng nội dung PLF-007 sẽ gửi (phiên bản, hệ điều hành, RAM, GPU và driver, minidump và dung lượng, các lệnh cuối).
  3. Ô "Luôn tự gửi báo lỗi (không kèm bản vẽ)"; Gửi → toast có mã theo dõi; Không gửi → máy chủ giả không nhận gói nào.
  4. Không có mạng: báo lỗi xếp hàng, gửi ở lần có mạng kế tiếp, không hỏi lại.
  5. Đạt DoD-U, DoD-L.

### UIL-004 · Gửi bản vẽ lỗi (TCREPORT)
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLF-010, UI0-008, UIA-008
- **Màn hình:** UI-L08
- **Đầu vào:** 07.3 §6 UI-L08; 05 §14 (kho bản vẽ lớn lên nhờ nút gửi bản vẽ lỗi); prototype `index.html#UI-L08`, `js/screens2.js` `SC.sendDrawing` (hộp `send` 640); API PLF-010 (đóng gói, ẩn danh tùy chọn, tải lên có tiến trình, mã theo dõi; lệnh TCREPORT).
- **Đầu ra:** `app/system/report/SendDrawingDialog.{h,cpp}`, `ScreenRegionGrabber.{h,cpp}`; tests.
- **Tiêu chí nghiệm thu:**
  1. Trường: Loại vấn đề (Hiển thị sai, Chạy chậm, Không mở được, In sai, Lỗi font, Khác), email nhận phản hồi, mô tả; "Chụp một vùng màn hình…" (hộp ẩn tạm, kéo khung, kết quả "1 ảnh · W × H px").
  2. Tệp đính kèm tự gợi ý: bản vẽ đang mở và xref đã nạp (chọn sẵn), font thiếu (chưa chọn); Thêm tệp…; tổng dung lượng.
  3. Ô quyền sử dụng "Cho phép TrionCAD dùng tệp này trong bộ kiểm thử nội bộ (không chia sẻ ra ngoài)" mặc định tắt; dòng chân "Tệp gửi qua kết nối mã hóa, lưu trên máy chủ tại Việt Nam."
  4. Tải lên có tiến trình và Hủy khi quá 2 s; xong hiện mã theo dõi, nút Sao chép mã, câu hẹn trả lời.
  5. Mở từ menu Trợ giúp, banner lỗi hiển thị, chuột phải trên tab tệp (đều chạy TCREPORT).
  6. Đạt DoD-U, DoD-L.

### UIL-005 · Quyền riêng tư
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** PLF-008, UI0-008
- **Màn hình:** UI-L09
- **Đầu vào:** 07.3 §6 UI-L09; 05 §16 (telemetry chỉ khi đồng ý, băm đường dẫn); prototype `index.html#UI-L09`, `js/screens2.js` `SC.privacy` (hộp `priv` 600); API PLF-008 (mức, nhật ký dữ liệu đã gửi, yêu cầu xóa).
- **Đầu ra:** `app/system/privacy/PrivacyPage.{h,cpp}` (hộp riêng và nhúng được vào UI-D10 › Hệ thống); tests.
- **Tiêu chí nghiệm thu:**
  1. Mức: Tắt (mặc định) / Cơ bản (lỗi và hiệu năng, ẩn danh) / Đầy đủ (thêm tần suất dùng lệnh); mỗi mức có câu mô tả dữ liệu cụ thể.
  2. Minh bạch: máy chủ đặt tại Việt Nam, dữ liệu ẩn danh giữ tối đa 12 tháng; "Xem nhật ký dữ liệu đã gửi" hiện đúng bản ghi PLF-008 đã gửi; "Yêu cầu xóa dữ liệu" hỏi xác nhận.
  3. Bật/tắt: tự gửi báo lỗi khi đóng bất thường; tự kiểm tra cập nhật.
  4. Mức Tắt: trong 1 giờ chạy kịch bản tự động, máy chủ giả không nhận gói telemetry nào.
  5. Đạt DoD-U, DoD-L.

### UIL-006 · Liên hệ hỗ trợ, góp ý, khảo sát NPS, huy hiệu Beta
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** PLF-010, PLF-008, UIA-002, UI0-008
- **Màn hình:** UI-L17
- **Đầu vào:** 07.3 §6 UI-L17; prototype `index.html#UI-L17`, `js/screens2.js` `SC.support` (hộp `sup` 680, tab Hỗ trợ, Góp ý), `SC.nps`; gói nhật ký và thông tin hệ thống của PLF-010.
- **Đầu ra:** `app/system/support/SupportDialog.{h,cpp}`, `FeedbackForm.{h,cpp}`, `NpsSurvey.{h,cpp}`, `BetaBadge.{h,cpp}`; tests.
- **Tiêu chí nghiệm thu:**
  1. Tab Hỗ trợ: mã QR Zalo OA (ảnh từ cấu hình), liên kết, giờ trả lời; biểu mẫu mô tả, đính kèm ảnh chụp, ô "Tự đính kèm nhật ký và thông tin hệ thống" (xem trước được); Gửi yêu cầu hỗ trợ → mã hỗ trợ; hướng dẫn hỗ trợ từ xa (UltraViewer, TeamViewer); diễn đàn cộng đồng.
  2. Tab Góp ý: loại (Ý tưởng, Lỗi nhỏ, Khác), nội dung, kèm ảnh chụp, Gửi góp ý.
  3. Khảo sát NPS 0–10 hiện tối đa một lần mỗi quý, Bỏ qua được; không hiện khi đang chạy lệnh hoặc trong 10 phút đầu phiên **[đề xuất]**.
  4. Kênh beta: huy hiệu "Beta" trên thanh tiêu đề, nút Góp ý luôn hiện.
  5. Gửi lỗi mạng: dữ liệu nhập giữ lại, nút thử lại.
  6. Đạt DoD-U, DoD-L.

## 4. Đồ họa, hiệu năng, trợ giúp, giới thiệu (UI-L10–L13)

### UIL-007 · Thiết lập đồ họa (GRAPHICSCONFIG)
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-001, RND-018, NT-008, INT-001, UI0-008, CMD:GRAPHICSCONFIG
- **Màn hình:** UI-L10
- **Đầu vào:** 07.3 §6 UI-L10; 05 §9.5; 03 §21 (GRAPHICSCONFIG P0); prototype `index.html#UI-L10`, `js/screens2.js` `SC.graphics` (hộp `gfx` 640); API RND-001 (backend, GPU, VRAM, driver, lỗi đồ họa), RND-018 (mức chất lượng, MSAA, làm mượt, giới hạn khung khi chạy pin, LOD).
- **Đầu ra:** `app/system/graphics/GraphicsConfigDialog.{h,cpp}`, `FpsOverlay.{h,cpp}`; tests.
- **Tiêu chí nghiệm thu:**
  1. Backend: Tự động (đang dùng X) / Direct3D 12 / Direct3D 11 / Vulkan / Metal (chỉ macOS) / OpenGL; backend máy không hỗ trợ hiện mờ kèm lý do. GPU: danh sách GPU tích hợp và rời kèm VRAM.
  2. Mức chất lượng: Tự động / Cao / Cân bằng / Tiết kiệm. Hiển thị: MSAA, làm mượt nét, giới hạn 30 khung/giây khi chạy pin, mức LOD, hiện FPS ở góc vùng vẽ.
  3. Chẩn đoán: phiên bản và ngày driver, số lỗi đồ họa trong 30 ngày, VRAM đang dùng/tổng; Sao chép báo cáo.
  4. Lưu: chất lượng, MSAA, làm mượt, LOD, FPS áp dụng ở khung kế tiếp (≤ 100 ms). Đổi backend hoặc GPU dựng lại vùng vẽ không khởi động lại ứng dụng, ≤ 2 s **[đề xuất]**; lỗi thì quay về backend cũ và báo.
  5. Nút Chạy kiểm tra hiệu năng (mở UI-L11), Đặt lại mặc định, Hủy, Lưu.
  6. Lớp FPS làm khung chậm thêm ≤ 0,2 ms.
  7. Gọi từ `.scr`, `(command)` hoặc `trion-console`: báo không có giao diện, trả lỗi, không treo.
  8. Đạt DoD-U, DoD-L.

### UIL-008 · Kiểm tra hiệu năng máy (TRIONBENCH)
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIL-007, RND-024, PLF-008
- **Màn hình:** UI-L11
- **Đầu vào:** 07.3 §6 UI-L11; 05 §9.5; prototype `index.html#UI-L11`, `js/screens2.js` `SC.bench` (hộp `bench` 700: 8 bài đo, so với máy tham chiếu, đề xuất, lịch sử); API RND-024 (chạy từng bài, tiến trình, hủy, điểm, mức máy, cấu hình đề xuất, lịch sử, gửi ẩn danh; lệnh TRIONBENCH).
- **Đầu ra:** `app/system/bench/BenchDialog.{h,cpp}`, `BenchCompareBar.{h,cpp}`; tests.
- **Tiêu chí nghiệm thu:**
  1. Bảng: CPU một luồng, CPU đa luồng (số luồng), SIMD (tập lệnh), render 1 triệu và 10 triệu đối tượng, hatch dày, chữ dày, đọc đĩa; cột Điểm (kèm FPS hoặc MB/s), So với máy tham chiếu (văn phòng cũ / tầm trung / máy trạm, có nhãn chữ), Trạng thái (Chờ, Đang đo, Xong).
  2. Xong: mức máy và cấu hình đề xuất; "Áp dụng cấu hình đề xuất" ghi vào RND-018, toast xác nhận.
  3. Hủy giữa chừng phản hồi ≤ 200 ms; đóng hộp thì hủy bài đo.
  4. Lịch sử các lần đo (ngày, máy, điểm chính); ô "Gửi kết quả ẩn danh…" mặc định tắt, chỉ gửi khi được chọn.
  5. Khi bài đo GPU chạy, hộp vẫn vẽ lại ≤ 16 ms/khung.
  6. Đạt DoD-U, DoD-L.

### UIL-009 · Cửa sổ trợ giúp và F1 theo ngữ cảnh (HELP)
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** INT-001, INT-017, UI0-015, UI0-008
- **Lệnh:** HELP
- **Màn hình:** UI-L12
- **Đầu vào:** 07.3 §6 UI-L12; 07.1 §1 (tên lệnh tiếng Anh luôn hiện kèm); README §4.2 (chỉ mục tìm lệnh dùng lại INT-017); prototype `index.html#UI-L12`, `js/screens2.js` `TC.openHelp` (hộp không modal `help` 900×600, `TC.HELP_TOPICS`, `art`).
- **Đầu ra:** `app/system/help/HelpWindow.{h,cpp}`, `HelpIndex.{h,cpp}`, `HelpArticleView.{h,cpp}` (Markdown tập con trên QTextBrowser), `HelpContext.{h,cpp}` (lệnh đang chạy, mã màn hình đang focus → bài); `docs/specs/commands/HELP.md`; tests.
- **Tiêu chí nghiệm thu:**
  1. Ô tìm theo tên lệnh tiếng Anh, viết tắt, từ khóa tiếng Việt không dấu; kết quả ≤ 50 ms với 3.000 bài **[đề xuất]**.
  2. Mục lục: Bắt đầu, Chuyển từ AutoCAD, Lệnh A–Z, Biến hệ thống, LISP, Câu hỏi thường gặp, Phím tắt; dòng "Tài liệu offline <bản> · cập nhật thêm khi có mạng".
  3. Trang lệnh: tên, mô tả tiếng Việt, viết tắt, vị trí trên ribbon ("Trang chủ › Sửa"), phiên bản có lệnh, khác biệt với AutoCAD, nút "Chạy X".
  4. F1 khi đang chạy lệnh mở trang lệnh đó; F1 trong hộp thoại hoặc palette mở trang của mã màn hình; không có trang thì mở trang chủ với ô tìm điền sẵn. `'HELP` chạy trong suốt giữa lệnh.
  5. Mở cửa sổ ≤ 150 ms; chạy hoàn toàn offline.
  6. Đạt DoD-U, DoD-L.

### UIL-010 · Nội dung trợ giúp: trang sinh từ metadata, bài viết, gói offline
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIL-009, INT-001, INT-005, UI0-015
- **Màn hình:** UI-L12
- **Đầu vào:** 07.3 §6 UI-L12 (mục lục, tài liệu offline đóng gói sẵn); 05 §15 (không chép văn bản trợ giúp của Autodesk); metadata lệnh (INT-001), sysvar (INT-005), phím tắt (UI0-015).
- **Đầu ra:** `app/system/help/tools/gen_help.py` (sinh trang Lệnh A–Z, Biến hệ thống, Phím tắt); `app/system/help/content/vi/`, `content/en/` (Bắt đầu, Chuyển từ AutoCAD sang trong 1 ngày, Bác sĩ Font, In và xuất PDF theo CTB, LISP trong TrionCAD, Câu hỏi thường gặp); gói `help-<phiên bản>.pack` kèm chỉ mục; kiểm liên kết trong CI.
- **Tiêu chí nghiệm thu:**
  1. 100% lệnh đã đăng ký và 100% sysvar trong registry có trang sinh tự động; CI báo lỗi khi thiếu.
  2. 0 liên kết hỏng giữa các bài; mọi ảnh có chữ thay thế.
  3. Gói offline ≤ 30 MB **[đề xuất]**, sinh tất định (cùng nguồn cho cùng hash).
  4. Mọi trang sinh tự động có bản English; bài viết tay có bản English từ mốc 1.0.
  5. Đạt DoD-U (mục 6).

### UIL-011 · Giới thiệu (ABOUT)
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** UIL-006, NT-008, NT-001, RND-001, PLF-002, INT-001
- **Lệnh:** ABOUT
- **Màn hình:** UI-L13
- **Đầu vào:** 07.3 §6 UI-L13; 05 §15 (Qt LGPL liên kết động, SBOM); prototype `index.html#UI-L13`, `js/screens2.js` `SC.about` (hộp `about` 600, danh sách thư viện mã nguồn mở); manifest vcpkg (NT-001).
- **Đầu ra:** `app/system/about/AboutDialog.{h,cpp}`; `app/system/about/gen_licenses.py` (sinh `licenses.json` và văn bản giấy phép từ manifest vcpkg lúc build); `docs/specs/commands/ABOUT.md`; tests.
- **Tiêu chí nghiệm thu:**
  1. Phiên bản, bản build, kênh; giấy phép (phiên bản, huy hiệu); thông tin hệ thống (hệ điều hành, CPU, RAM, backend và GPU, độ phân giải và tỉ lệ, ngôn ngữ, hồ sơ) có nút Sao chép.
  2. Danh sách thư viện mã nguồn mở và giấy phép sinh từ manifest; CI báo lỗi khi một phụ thuộc thiếu văn bản giấy phép; mục Qt ghi LGPL-3.0, liên kết động, cách thay thư viện Qt.
  3. Điều khoản sử dụng, chính sách riêng tư, ghi chú "AutoCAD® là nhãn hiệu của Autodesk, Inc.", Liên hệ hỗ trợ (mở UI-L17).
  4. Mở ≤ 150 ms; thông tin GPU đọc ở nền nếu chậm.
  5. Đạt DoD-U, DoD-L.

## 5. Cài đặt và tích hợp hệ điều hành (UI-L18)

### UIL-012 · Trình cài đặt Windows và gỡ cài đặt
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** PLF-009, UI0-002, UI0-006
- **Màn hình:** UI-L18
- **Đầu vào:** 07.3 §6 UI-L18; prototype `index.html#UI-L18`, `js/screens2.js` `SC.installer` (hộp `inst` 640×470, 7 bước); gói MSI và tham số của PLF-009 (thư mục, liên kết tệp, lối tắt, menu chuột phải, jump list, thumbnail); tài liệu Windows Installer API công khai của Microsoft.
- **Đầu ra:** `app/system/setup/`: ứng dụng Qt nhỏ `trion-setup` **[đề xuất]** điều khiển MSI của PLF-009 (cài, sửa, gỡ) qua Windows Installer API, nhận tiến trình qua external UI handler, dùng theme và thành phần UI0; tests trên máy ảo sạch.
- **Tiêu chí nghiệm thu:**
  1. Bảy bước: Ngôn ngữ (Tiếng Việt, English); Điều khoản (ô "Tôi đồng ý" mới mở nút Tiếp); Thư mục (Duyệt…, dung lượng cần và còn trống, thiếu chỗ thì báo và không cho tiếp); Liên kết tệp (`.dwg`, `.dxf`, `.dwt`; "Đặt TrionCAD làm ứng dụng mặc định" bật sẵn khi máy chưa có ứng dụng CAD, tắt sẵn kèm giải thích khi đã có AutoCAD); Lối tắt (Desktop, Start Menu, "Mở bằng TrionCAD", "Mở chỉ đọc", jump list; thumbnail Explorer hiện mờ "từ bản 1.0"); Cài đặt (tiến trình, tệp đang chép, Hủy có hoàn tác); Hoàn tất ("Mở TrionCAD").
  2. Quay lại vô hiệu ở bước Cài đặt và Hoàn tất; mọi bước dùng được bằng bàn phím.
  3. Hủy giữa chừng: máy trở về như trước khi cài (so ảnh chụp registry và thư mục trên máy ảo sạch).
  4. Gỡ cài đặt: hỏi giữ hay xóa thiết lập người dùng; hỏi lý do gỡ (không bắt buộc, gửi khi có mạng).
  5. Chạy đúng trên Windows 10 22H2 và Windows 11, 100/150/200% DPI; ký số qua PLF-009 để SmartScreen không chặn.
  6. Đạt DoD-U.
- **Ngoài phạm vi:** cài đặt im lặng, liên kết tệp, thumbnail Explorer, gói macOS (PLF-009).

### UIL-013 · Tích hợp hệ điều hành trong ứng dụng: ứng dụng mặc định, jump list
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** PLF-009, UI0-001
- **Màn hình:** UI-L18
- **Đầu vào:** 07.3 §6 UI-L18 (Trong ứng dụng; jump list tệp gần đây); 07.2 UI-D10 tab Hệ thống (liên kết tệp); API liên kết tệp của PLF-009.
- **Đầu ra:** `app/system/osintegration/DefaultAppButton.{h,cpp}` (nhúng được vào UI-D10 › Hệ thống), `JumpList.{h,cpp}`; tests.
- **Tiêu chí nghiệm thu:**
  1. Nút "Đặt TrionCAD làm ứng dụng mặc định cho DWG" mở trang ứng dụng mặc định của hệ điều hành ở đúng mục; khi đã là mặc định thì hiện dòng trạng thái thay cho nút.
  2. Mở tệp bằng TrionCAD thêm tệp vào jump list (tối đa 10 tệp **[đề xuất]**); xóa khỏi danh sách tệp gần đây trong ứng dụng thì jump list cũng xóa.
  3. Đạt DoD-U.

## 6. Giấy phép, nâng cấp, dùng thử (UI-L01–L05)

### UIL-014 · Trang nâng cấp
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** PLF-002, UI0-008
- **Màn hình:** UI-L04
- **Đầu vào:** 07.3 §6 UI-L04; 07.1 §2 (hành vi cổng tính năng); prototype `index.html#UI-L04`, `js/screens2.js` `SC.upgrade` (hộp `up` 560, `PRICE`, `BENEFIT`, `shownUp`); khả năng và phiên bản cần có từ PLF-002.
- **Đầu ra:** `app/system/upgrade/UpgradeDialog.{h,cpp}`; `app/system/upgrade/upgrade-content.json` (tính năng → lợi ích, ảnh minh họa, phiên bản cần, giá "từ … đ/năm"); API `showUpgrade(feature)` cho UI0-011 và INT-020; tests.
- **Tiêu chí nghiệm thu:**
  1. Nội dung: "Có trong TrionCAD <phiên bản>", tên tính năng, lợi ích 1–2 câu, ảnh minh họa, giá "Từ … đ/năm" và dòng phụ; nút Mua (mở trang mua UI-L16, chọn sẵn gói), Để sau. "Đã có key? Kích hoạt" có từ mốc VP, Dùng thử 30 ngày có từ mốc EA (task kích hoạt và dùng thử gắn vào).
  2. Mỗi tính năng hiện tối đa một lần trong một phiên; lần sau chỉ toast "X có trong TrionCAD Y" kèm "Xem".
  3. Giá và lợi ích đọc từ `upgrade-content.json` (cập nhật theo bản phát hành); tính năng thiếu mục thì dùng nội dung chung của phiên bản.
  4. Mở ≤ 150 ms.
  5. Đạt DoD-U, DoD-L.

### UIL-015 · Bộ mã hóa QR tự viết
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** NT-001
- **Màn hình:** UI-L02
- **Đầu vào:** 07.3 §6 UI-L02 (mã QR để quét bằng điện thoại, P1); tiêu chuẩn ISO/IEC 18004 (QR Code); 05 §2 (không thêm phụ thuộc vào sản phẩm); prototype `js/screens4.js` `qrSVG` (chỉ minh họa).
- **Đầu ra:** `app/system/license/qr/QrEncoder.{h,cpp}` (chế độ byte, mức sửa lỗi M, phiên bản 1–10, chọn mặt nạ theo điểm phạt), `QrWidget.{h,cpp}` (vẽ vector theo DPI, lề trắng 4 ô); tests.
- **Tiêu chí nghiệm thu:**
  1. 1.000 chuỗi ngẫu nhiên 1–200 byte: ảnh mã giải được 100% bằng bộ giải độc lập chỉ dùng trong CI (ZXing-C++, Apache-2.0) **[đề xuất]**.
  2. Tất định: cùng chuỗi cho cùng ma trận trên 3 hệ điều hành (so hash).
  3. Mã đen trên nền trắng ở cả chủ đề Tối; ở 100% mỗi ô ≥ 3 px.
  4. Đạt DoD-C, DoD-U.

### UIL-016 · Kích hoạt giấy phép: tài khoản, key, giáo dục
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLF-001, PLF-002, PLF-003, UIL-014, UI0-008
- **Màn hình:** UI-L01
- **Đầu vào:** 07.3 §6 UI-L01; 07.1 §2; prototype `index.html#UI-L01`, `js/screens4.js` `SC.activate` (hộp `act` 760×520, `ACT_TABS`, `errBox`, `go`); API PLF-003 (đăng nhập, kích hoạt bằng key, xác minh email trường, mã lỗi), PLF-001 (giấy phép kết quả).
- **Đầu ra:** `app/system/license/ActivationDialog.{h,cpp}`, `KeyInput.{h,cpp}`, `ActivationErrorBox.{h,cpp}`; gắn "Đã có key? Kích hoạt" vào trang nâng cấp; tests với máy chủ kích hoạt giả.
- **Tiêu chí nghiệm thu:**
  1. Cột trái: Tài khoản TrionCAD, Nhập key, Máy chủ giấy phép (UIL-020), Giáo dục, Kích hoạt offline (UIL-017); dưới cùng tên máy, hệ điều hành, phiên bản.
  2. Tài khoản: email, mật khẩu, "Quên mật khẩu?", "Đăng nhập qua trình duyệt" (trình duyệt mặc định; ứng dụng nhận kết quả qua PLF-003), "Chưa có tài khoản? Mua hoặc dùng thử".
  3. Key: tự viết hoa, tự thêm gạch nối mỗi 4 ký tự, dán cả key một lần được; cần 16 ký tự bắt đầu bằng TCVP, TCPR hoặc TCEN, sai thì lỗi dưới ô.
  4. Giáo dục: email đuôi `.edu.vn` hoặc tên miền trường đã đăng ký, hoặc mã do trường cấp; gửi mã 6 số, Xác minh, Gửi lại mã.
  5. Lỗi có hành động: key sai (nhắc 0/O, 1/I); hết lượt kích hoạt "2/2 máy" (Quản lý thiết bị trên web, Thử lại); không kết nối (gợi ý proxy; Thử lại, Kích hoạt offline).
  6. Thành công: phiên bản, thời hạn, thiết bị (k/2 máy), quyền cập nhật; "Bắt đầu dùng" đổi phiên bản không khởi động lại, ribbon và cổng tính năng cập nhật ≤ 1 s.
  7. Gọi mạng không chặn UI; nút chính hiện "Đang kích hoạt…"; hết thời gian 15 s **[đề xuất]**.
  8. Đạt DoD-U, DoD-L.

### UIL-017 · Kích hoạt offline: tệp yêu cầu, mã QR, tệp phản hồi
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** UIL-016, UIL-015, PLF-003, PLF-001
- **Màn hình:** UI-L02
- **Đầu vào:** 07.3 §6 UI-L02; prototype `js/screens4.js` `offHTML` (ba bước); API PLF-003 (tạo `.req`, nhập `.lic`), PLF-001 (kiểm chữ ký Ed25519, ràng buộc máy).
- **Đầu ra:** `app/system/license/OfflineActivationPage.{h,cpp}`; tests.
- **Tiêu chí nghiệm thu:**
  1. Bước 1 "Tạo tệp yêu cầu": `<tên máy>.req` (không chứa dữ liệu bản vẽ), Lưu tệp…, Sao chép mã máy, mã QR mở trang kích hoạt có sẵn mã máy.
  2. Bước 2: hướng dẫn trên máy có mạng (trang kích hoạt, tải `.req` lên hoặc quét QR, tải `.lic` về); nút "Đã có tệp .lic".
  3. Bước 3: chọn hoặc kéo thả `.lic`; lỗi rõ ràng cho tệp của máy khác, chữ ký sai, hết hạn, tệp hỏng.
  4. Trạng thái từng bước (xong, đang làm, chưa tới) đọc được bằng chữ, không chỉ bằng màu.
  5. Đạt DoD-U, DoD-L.

### UIL-018 · Trạng thái giấy phép
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIL-016, PLF-001, PLF-003, UI0-009
- **Màn hình:** UI-L03
- **Đầu vào:** 07.3 §6 UI-L03; 04 §6.3 (hết hạn không khóa dữ liệu); prototype `index.html#UI-L03`, `js/screens4.js` `SC.license` (hộp `lic` 680, `LIC_SIM`, `setLic`, `SC.signOut`).
- **Đầu ra:** `app/system/license/LicenseStatusDialog.{h,cpp}`, `LicenseNotices.{h,cpp}` (nhắc trước 30, 7, 1 ngày); tests.
- **Tiêu chí nghiệm thu:**
  1. Thông tin: phiên bản kèm huy hiệu; loại (thuê bao, vĩnh viễn, dùng thử, giáo dục); key che giữa, có nút sao chép; hạn dùng; quyền cập nhật đến ngày; tài khoản.
  2. Thiết bị đã kích hoạt (k/2): tên máy, hệ điều hành, ngày; Hủy kích hoạt hỏi xác nhận; hủy máy này thì về Viewer ngay, bản vẽ đang mở không bị đóng.
  3. Trạng thái: sắp hết hạn (toast và ghi chú trong hộp trước 30, 7, 1 ngày); đã hết hạn (đang chạy Viewer, dữ liệu an toàn, vẫn mở và in được, nút Gia hạn); lâu chưa xác minh (còn n ngày dùng offline; Kết nối lại, Kích hoạt offline…).
  4. Nút: Đổi key…, Đăng xuất (hỏi xác nhận; giải phóng giấy phép; về Viewer), Gia hạn hoặc nâng cấp (mở trang web UI-L16), Đóng. Viewer miễn phí: "Miễn phí, không cần kích hoạt", Kích hoạt, nhập key…, Mua.
  5. Đổi trạng thái giấy phép áp dụng trong phiên, không khởi động lại; lệnh đang chạy không bị ngắt giữa chừng.
  6. Đạt DoD-U, DoD-L.

### UIL-019 · Dùng thử: bắt đầu, nhắc hạn, hết hạn
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIL-014, PLF-004, UI0-009
- **Màn hình:** UI-L05
- **Đầu vào:** 07.3 §6 UI-L05; 07.1 §2 (huy hiệu "Dùng thử: còn 12 ngày", nhắc 7, 3, 1 ngày, hết hạn về Viewer); prototype `index.html#UI-L05`, `js/screens2.js` `SC.trialStart` (hộp `trial` 460), `js/screens8.js` `SC.trialExpired` (hộp `trialx` 620), nhắc hạn `trialNoted`; API PLF-004 (bắt đầu, số ngày còn, ưu đãi khi hết hạn).
- **Đầu ra:** `app/system/trial/TrialStartDialog.{h,cpp}`, `TrialReminder.{h,cpp}`, `TrialExpiredDialog.{h,cpp}`; gắn nút Dùng thử 30 ngày vào trang nâng cấp; tests.
- **Tiêu chí nghiệm thu:**
  1. Bắt đầu: email (kiểm dạng; lỗi "Email chưa đúng. Ví dụ: ten@congty.vn"), câu "Không cần thẻ thanh toán…", ô "Nhắc tôi trước khi hết hạn 7, 3 và 1 ngày"; thành công thì đổi phiên bản trong phiên, toast nêu ngày hết hạn.
  2. Nhắc: ở các mốc 7, 3, 1 ngày hiện toast ở lại, một lần mỗi mốc, kèm "Mua TrionCAD Pro".
  3. Hết hạn: hộp giải thích chuyển về Viewer; vẫn mở, xem, đo, in; bản vẽ đã lưu an toàn, mở được bằng AutoCAD; giữ thiết lập, viết tắt, LISP đã nạp; ưu đãi (mã, hạn lấy từ PLF-004); nút "Đã có key? Kích hoạt", "Tiếp tục với Viewer", "Mua TrionCAD Pro".
  4. Hết hạn giữa phiên: lệnh Lưu vẫn dùng được cho bản vẽ đang mở tới khi đóng bản vẽ đó, không mất dữ liệu **[đề xuất]**.
  5. Đạt DoD-U, DoD-L.

### UIL-020 · Giấy phép floating: kết nối máy chủ, hết ghế, hàng đợi
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIL-016, UIL-018, PLF-005
- **Màn hình:** UI-L01, UI-L03
- **Đầu vào:** 07.3 §6 UI-L01 (máy chủ giấy phép; "Đã dùng hết 10/10 ghế"), UI-L03 (với floating); prototype `js/screens4.js` (tab `srv`, mô phỏng `seats`, `scan`, `notify`, mục floating của `SC.license`), `quan-tri.html` (giữ ghế 3 phút cho người đầu hàng đợi, thu hồi cho 5 phút để lưu); API PLF-005 (kết nối, tìm máy chủ trong mạng, cấp và trả ghế, hàng đợi, thu hồi, ân hạn).
- **Đầu ra:** `app/system/license/FloatingServerPage.{h,cpp}`, `SeatQueueNotifier.{h,cpp}`, `SeatRevokedBanner.{h,cpp}`; tests với máy chủ floating giả.
- **Tiêu chí nghiệm thu:**
  1. Tab Máy chủ giấy phép: địa chỉ `máy:cổng`, "Tự tìm trong mạng", "Kết nối lại tự động khi mở TrionCAD".
  2. Hết ghế: "Đã dùng hết 10/10 ghế" với Thử lại / Báo khi có ghế trống / Mở ở chế độ Viewer; chọn báo thì vào hàng đợi, có ghế thì toast mời nhận ghế trong 3 phút.
  3. Bị thu hồi ghế: banner đếm ngược 5 phút để lưu, sau đó về Viewer, bản vẽ không bị đóng.
  4. Mất kết nối máy chủ: giữ ghế trong thời gian ân hạn, chip thanh trạng thái đếm ngược.
  5. Hộp Trạng thái giấy phép có mục floating: máy chủ, đã kết nối, ghế đang dùng k/n; "Mượn giấy phép offline" hiện mờ ghi "P2".
  6. Đạt DoD-U, DoD-L.

## 7. Viewer Pro: markup (UI-M02)

### UIM-001 · Markup: mô hình dữ liệu và tệp `.tcmk`
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** NT-011, NT-010
- **Màn hình:** UI-M02
- **Đầu vào:** 07.3 §4 UI-M02 (Lưu trữ: tệp riêng cạnh bản vẽ, không sửa DWG; nhập và gộp); design/README ("Viewer Pro không sửa DWG"); prototype `js/screens3.js` mục "Markup UI-M02" (`mkNew`, `mks`, trường của một markup, `importMk`).
- **Đầu ra:** `app/system/markup/MarkupModel.{h,cpp}` (14 loại; tọa độ theo Model hoặc layout; màu, độ dày, độ mờ, cỡ chữ, nội dung, tác giả, thời điểm, trạng thái, luồng bình luận, ẩn, nguồn); `MarkupFile.{h,cpp}`: `.tcmk` **[đề xuất]** là JSON UTF-8 có số phiên bản, mỗi markup và bình luận có UUID, ảnh chụp vùng là PNG mã base64; `MarkupMerge.{h,cpp}`, `MarkupUndo.{h,cpp}`; `docs/specs/formats/tcmk.md`; tests, fuzz target.
- **Tiêu chí nghiệm thu:**
  1. Tệp `<tên bản vẽ>.tcmk` đặt cạnh DWG; ghi nguyên tử (NT-011) sau mỗi thay đổi, gom trong 1 s **[đề xuất]**; DWG không đổi một byte trong mọi thao tác markup (so hash).
  2. Thư mục bản vẽ chỉ đọc: ghi vào thư mục dữ liệu người dùng, báo một lần bằng banner trong palette, phiên sau vẫn mở lại được.
  3. Gộp theo UUID: hai phiên cùng ghi một `.tcmk` trên thư mục mạng không mất markup hay bình luận nào; một markup sửa ở hai nơi thì giữ bản có thời điểm mới hơn **[đề xuất]**. Nhập tệp của người khác bỏ qua markup trùng vị trí và nội dung.
  4. Tệp ghi dấu nhận dạng bản vẽ (tên, kích thước, GUID của bản vẽ nếu có); mở với bản vẽ khác dấu thì cảnh báo, không tự xóa markup. Markup của layout đã đổi tên được giữ, gắn nhãn "layout không còn".
  5. Hoàn tác và làm lại thao tác markup (thêm, sửa, xóa, đổi trạng thái), ≥ 100 bước, tách khỏi undo của bản vẽ.
  6. Round-trip đọc → ghi → đọc giữ 100% trường trên 1.000 tệp sinh ngẫu nhiên; đọc tệp 10.000 markup ≤ 200 ms.
  7. Fuzz `MarkupFile` 1 giờ không lỗi; giới hạn kích thước tệp và ảnh.
  8. Đạt DoD-C, DoD-U.

### UIM-002 · Markup: lệnh TCMARKUP, công cụ hình và nét, thuộc tính, vẽ trên overlay
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** UIM-001, INT-001, INT-002, INT-003, INT-012, RND-012, UI0-011
- **Lệnh:** TCMARKUP
- **Màn hình:** UI-M02
- **Đầu vào:** 07.3 §4 UI-M02 (Công cụ, Thuộc tính), UI-M01 (tab Ghi chú); prototype `index.html#UI-M02`, `js/screens3.js` (`IMPL.TCMARKUP`, `drawMk`, `polyCloud`, `cloud`, `MCOL`, `MK.props`), `js/ribbons.js` (tab Ghi chú: panel Đánh dấu, Thuộc tính).
- **Đầu ra:** `app/system/markup/MarkupCommand.{h,cpp}` (đăng ký TCMARKUP), `MarkupTools.{h,cpp}` (đám mây, chữ nhật, ellipse, mũi tên, đường thẳng, nét tự do, bút dạ), `MarkupRenderer.{h,cpp}` (vẽ trên overlay RND-012), `MarkupProps.{h,cpp}`; `docs/specs/commands/TCMARKUP.md` (mọi từ khóa, prompt, cách gọi từ `.scr`); tests.
- **Tiêu chí nghiệm thu:**
  1. TCMARKUP không đối số hỏi "Chọn công cụ markup" với chip Đám mây · C, Chữ nhật · R, Mũi tên · A, Chữ · T, Nét tự do · F, Con dấu · S, Danh sách · L; mọi công cụ khác có từ khóa riêng không trùng chữ tắt (đặc tả). Mỗi công cụ lặp tới Enter hoặc Esc; ribbon, bàn phím, `.scr` cho cùng kết quả.
  2. Đám mây, chữ nhật, ellipse: hai góc, xem trước theo con trỏ, sau đó ô ghi chú tại chỗ (Enter xong, Shift+Enter xuống dòng, Esc bỏ ghi chú); mũi tên, đường thẳng: hai điểm; nét tự do, bút dạ: kéo chuột (bút dạ nét 14 px, độ mờ 38%).
  3. Thuộc tính: 8 màu, độ dày 1/2/3/5 px, độ mờ 100/75/50/25%, cỡ chữ 11/14/18/24 px; nhớ qua phiên; cỡ quy đổi ra đơn vị bản vẽ theo mức zoom lúc tạo.
  4. Mỗi markup có huy hiệu số thứ tự (đã xử lý thì là dấu kiểm xám); bấm huy hiệu mở palette ở đúng markup.
  5. Markup vẽ trên overlay, không chạm dữ liệu bản vẽ; 1.000 markup trên màn hình: pan/zoom vẫn ≥ 60 fps trên BENCH-STD 1 triệu entity; xem trước khi vẽ ≤ 8 ms/khung.
  6. Dùng được trong Model và layout (tọa độ giấy); đang chạy COMPARE thì ẩn markup.
  7. Viewer: lệnh báo "có trong TrionCAD Viewer Pro" và mở trang nâng cấp.
  8. Đạt DoD-U, DoD-L.

### UIM-003 · Markup: chữ ghi chú, ghi chú có đường dẫn, dấu tick, con dấu, đo, chụp vùng
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIM-002, INT-010, PLF-003
- **Màn hình:** UI-M02
- **Đầu vào:** 07.3 §4 UI-M02; prototype `js/screens3.js` (`IMPL.TCMARKUP` nhánh `text`, `leader`, `tick`, `stamp`, `dist`, `area`, `snap`; `STAMPS`, `textBox`, `shot`, `editText`), `js/ribbons.js` (nút Con dấu có menu ba loại, Chụp vùng).
- **Đầu ra:** mở rộng `MarkupTools`, `MarkupRenderer`; `RegionCapture.{h,cpp}` (lấy khung hình của vùng vẽ qua UIA-008 rồi cắt vùng); cập nhật `TCMARKUP.md`; tests.
- **Tiêu chí nghiệm thu:**
  1. Chữ ghi chú: chỉ điểm rồi soạn tại chỗ; ghi chú có đường dẫn: điểm cần ghi chú (đầu mũi tên), rồi vị trí chữ; dấu tick: một điểm.
  2. Con dấu ĐÃ DUYỆT / CẦN SỬA / KHÔNG DUYỆT (chọn ở ribbon hoặc từ khóa), khung đôi xoay −8°, dòng phụ "viết tắt tác giả · dd/mm/yyyy"; tác giả là tên tài khoản (PLF-003), chưa đăng nhập thì tên người dùng hệ điều hành **[đề xuất]**.
  3. Đo khoảng cách: hai điểm có bắt điểm (INT-010), nhãn giá trị theo `UNITS` khi kéo, lưu thành markup "Khoảng cách … mm"; đo diện tích: nhiều điểm, Enter để xong (≥ 3 điểm), lưu "Diện tích … m²".
  4. Chụp vùng: kéo khung; ảnh vào danh sách markup và clipboard; toast có "Lưu PNG"; ảnh đúng độ phân giải màn hình theo DPI, không có con trỏ hay marker tạm.
  5. Chữ tiếng Việt gõ bằng Unikey, EVKey, bộ gõ hệ điều hành hiện đúng trong ô soạn.
  6. Đạt DoD-U, DoD-L.

### UIM-004 · Palette Markup: danh sách, lọc, bình luận, trạng thái
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIM-001, UI0-010, UI0-006, UIA-008
- **Màn hình:** UI-M02
- **Đầu vào:** 07.3 §4 UI-M02 (Danh sách markup); prototype `js/screens3.js` `P.markup` (lọc tác giả và trạng thái, chi tiết, trả lời, Zoom, Đã xử lý / Mở lại, Ẩn, Xóa, menu thêm, chân palette).
- **Đầu ra:** `app/system/markup/MarkupPalette.{h,cpp}`, `MarkupListModel.{h,cpp}`, `CommentThreadWidget.{h,cpp}`; tests.
- **Tiêu chí nghiệm thu:**
  1. Dòng tin "Lưu trong X.tcmk cạnh bản vẽ. Không sửa tệp DWG."; lọc Mọi tác giả / từng tác giả (của mình ghi "(tôi)"), Mọi trạng thái / Đang mở / Đã xử lý.
  2. Mỗi mục: số, biểu tượng loại, tiêu đề (dòng đầu nội dung hoặc tên loại), tác giả · thời điểm, "· từ So sánh" nếu do COMPARE tạo, trạng thái, số bình luận.
  3. Chọn mục: zoom mềm tới markup (không zoom khi đã nằm trong khung nhìn); chi tiết gồm ảnh chụp, nội dung nhiều dòng, luồng bình luận (tác giả, thời điểm, nội dung), ô trả lời (Enter để gửi); nút Zoom, Đã xử lý / Mở lại (tự ghi một bình luận), Ẩn/Hiện, Xóa.
  4. Chỉ xóa được markup của mình; hỏi xác nhận nêu số và tiêu đề, nói rõ bình luận bị xóa và DWG không ảnh hưởng; hoàn tác được.
  5. Trạng thái rỗng: "Chưa có markup" (hướng dẫn dùng tab Ghi chú) và "Không có markup khớp bộ lọc".
  6. Chân palette "N markup · M mở", nút Xuất PDF; menu thêm: Nhập markup từ tệp…, Xuất PDF kèm markup…, Hiện tất cả, Ẩn tất cả, Đưa markup vào bản vẽ (Pro).
  7. 10.000 markup: danh sách ảo hóa cuộn 60 fps; lọc ≤ 16 ms.
  8. Đạt DoD-U, DoD-L.

### UIM-005 · Markup: xuất PDF kèm markup, nhập và gộp tệp của người khác
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIM-004, PLT-003, PLT-004
- **Màn hình:** UI-M02
- **Đầu vào:** 07.3 §4 UI-M02 (xuất PDF bản vẽ kèm markup; nhập markup của người khác); prototype `js/screens3.js` (`exportPdf`, `importMk`); API PLT-003, PLT-004 (thêm lớp primitive phụ vào trang in, layer OCG, trang văn bản).
- **Đầu ra:** `app/system/markup/MarkupPdfExport.{h,cpp}`, `MarkupImportDialog.{h,cpp}`; tests.
- **Tiêu chí nghiệm thu:**
  1. Xuất PDF: bản vẽ (Model hoặc các layout có markup) kèm markup ở layer PDF "Markup" bật/tắt được, cùng trang tổng hợp bình luận (số, tác giả, thời điểm, trạng thái, nội dung); tên mặc định `<tên>_markup.pdf`.
  2. Vị trí markup trong PDF lệch so với trên màn hình ≤ 0,1 mm trên giấy (so tọa độ trên 50 ca).
  3. Xuất chạy nền, quá 2 s thì có tiến trình và Hủy; xong có toast kèm "Mở tệp".
  4. Nhập: chọn `.tcmk`; hộp xác nhận "Gộp N markup của <tác giả> gửi <ngày>?" nêu số bị bỏ qua vì trùng; markup của mình giữ nguyên; tệp lỗi thì hộp lỗi có chi tiết.
  5. Đạt DoD-U, DoD-L.

### UIM-006 · Markup: đưa markup vào bản vẽ trên layer TC-MARKUP (Pro)
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UIM-002, DOC-007, DOC-008
- **Màn hình:** UI-M02
- **Đầu vào:** 07.3 §4 UI-M02 ("bản Pro có thêm Đưa markup vào bản vẽ (layer TC-MARKUP)"); prototype `js/ribbons.js` (nút "Đưa vào bản vẽ", khóa Pro), `js/screens3.js` (menu thêm của palette).
- **Đầu ra:** `app/system/markup/MarkupToDrawing.{h,cpp}`: tùy chọn "Đưa vào bản vẽ" của TCMARKUP, tạo entity qua DOC-007; cập nhật `docs/specs/commands/TCMARKUP.md`; tests.
- **Tiêu chí nghiệm thu:**
  1. Chọn tất cả, theo bộ lọc hiện tại, hoặc các markup đang chọn; hộp xác nhận nêu số markup và layer đích.
  2. Ánh xạ: đám mây → polyline có cung; chữ nhật, mũi tên, đường thẳng, nét tự do → polyline (bút dạ: polyline có bề rộng và độ trong suốt); ellipse → ELLIPSE; chữ, con dấu → MTEXT (con dấu kèm khung polyline); ghi chú có đường dẫn → MLEADER; đo → MTEXT kèm đường; ảnh chụp vùng bỏ qua và nêu trong báo cáo.
  3. Layer `TC-MARKUP` tạo khi chưa có; màu true color theo markup; vị trí lệch ≤ 1e-9 × kích thước bản vẽ.
  4. Cả lần đưa vào là một mark undo; Ctrl+Z xóa hết entity vừa tạo; `.tcmk` giữ nguyên.
  5. Viewer Pro: nút mang huy hiệu Pro, mở trang nâng cấp.
  6. Đạt DoD-U, DoD-L.

## 8. Trang web: thanh toán, cổng quản trị, máy chủ giấy phép (UI-L15, L16)

### UIL-021 · Thanh toán trên web: trang mua
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** UIN-001, PLF-012
- **Màn hình:** UI-L16
- **Đầu vào:** 07.3 §6 UI-L16; 04 §6 (giá, cam kết giấy phép công bằng), §7 (bán tự phục vụ); prototype `thanh-toan.html#mua` (bốn trạng thái Điền đơn, Chờ chuyển khoản, Thành công, Không thành công; ghi chú hành vi); API PLF-012 (bảng giá, tạo đơn, tra cứu mã số thuế, kiểm mã giảm giá và mã đại lý, chuyển sang cổng thanh toán, trạng thái đơn).
- **Đầu ra:** `web/mua/index.html`, `mua.js`, `mua.css`; kịch bản `web/tests/mua.spec.mjs` với máy chủ giả `web/tests/mock/pay-api.mjs`.
- **Tiêu chí nghiệm thu:**
  1. Chọn gói, chọn sẵn theo liên kết từ ứng dụng (`#vp`, `#pro`); số ghế (nút − và +, mặc định 1); thuê bao năm, tháng hoặc vĩnh viễn (ghi chú kèm 12 tháng cập nhật, gia hạn cập nhật không bắt buộc).
  2. Thông tin hóa đơn điện tử: công ty hoặc cá nhân; mã số thuế 10 số hoặc `10 số-3 số` (kiểm dạng), nút Tra cứu điền tên và địa chỉ, vẫn sửa tay được; email nhận hóa đơn.
  3. Mã bắt đầu bằng `DL-` là mã đại lý (ghi nhận và hiện tên đại lý, giá không đổi); mã khác là mã giảm giá; mã sai báo dưới ô.
  4. Phương thức: VNPay, MoMo, ZaloPay, chuyển khoản, thẻ quốc tế. Trang không nhận số thẻ (chuyển sang trang của cổng). Chuyển khoản: mã VietQR do PLF-012 cấp, số tài khoản, nội dung chuyển khoản là mã đơn, đơn giữ 24 giờ, tự kích hoạt khi tiền về.
  5. Tổng tiền cập nhật ngay khi đổi gói, kỳ, số ghế, mã (≤ 100 ms; chỉ gọi máy chủ khi kiểm mã); số tiền dạng 2.990.000 đ.
  6. Thành công: mã đơn, key, các bước kích hoạt trong ứng dụng, hóa đơn gửi trong 24 giờ. Thất bại: "Tài khoản của bạn chưa bị trừ tiền.", giữ đơn và thông tin, Thử lại, Chọn phương thức khác, liên hệ Zalo OA kèm mã đơn.
  7. Ở 390 px: tóm tắt đơn xuống cuối trang; cả luồng dùng được bằng bàn phím; CSP chặn script ngoài và script nội tuyến; mọi gọi API qua HTTPS.
  8. Đạt DoD-U (mục 1–3, 6, 7).
- **Ngoài phạm vi:** tích hợp cổng thanh toán, phát hành hóa đơn điện tử, tạo key (PLF-012).

### UIL-022 · Thanh toán trên web: trang tài khoản
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIL-021, PLF-012
- **Màn hình:** UI-L16
- **Đầu vào:** 07.3 §6 UI-L16 (trang tài khoản: hóa đơn, gia hạn tự động, quản lý ghế); prototype `thanh-toan.html#tai-khoan` (thuê bao Đang hoạt động, Còn 7 ngày, Đã hết hạn; ghi chú hành vi); API PLF-012 (thuê bao, gia hạn tự động, ghế, thiết bị, phương thức thanh toán, hóa đơn PDF và XML).
- **Đầu ra:** `web/tai-khoan/index.html`, `tai-khoan.js`; kịch bản Playwright.
- **Tiêu chí nghiệm thu:**
  1. Thuê bao: kiểu, số ghế (đang dùng), hạn; công tắc tự động gia hạn (không dùng được với chuyển khoản: nhắc email trước 30, 7, 1 ngày); đã hết hạn thì ghi rõ ứng dụng về Viewer, bản vẽ vẫn mở và in được, ngay cạnh nút Gia hạn.
  2. Ghế: mời theo email, thu hồi (hỏi xác nhận; máy của người đó về Viewer ở lần kết nối tới); mỗi ghế dùng trên 2 máy; ghi rõ giảm ghế khi gia hạn không bị phạt.
  3. Thiết bị đã kích hoạt: tên máy, người dùng, kết nối cuối, hủy kích hoạt.
  4. Phương thức thanh toán (che số, Đổi); thông tin xuất hóa đơn sửa cho các đơn sau; danh sách hóa đơn điện tử (số, ngày, nội dung, số tiền, tải PDF, XML).
  5. Ở 390 px bảng chuyển thành thẻ, không cuộn ngang.
  6. Đạt DoD-U (mục 1–3, 6, 7).

### UIL-023 · Cổng quản trị doanh nghiệp (web): khung, tổng quan, ghế và người dùng
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** UIN-001, PLF-011
- **Màn hình:** UI-L15
- **Đầu vào:** 07.3 §6 UI-L15 (Cổng web); prototype `quan-tri.html` (Tổng quan, Ghế và người dùng; trạng thái Đang dùng, Công ty mới chưa gán ghế, Hết ghế, Mất kết nối máy chủ floating, Quản trị phụ chỉ xem, Đang tải; ghi chú vai trò, mời hàng loạt, thu hồi); API PLF-011 (đăng nhập quản trị, ghế, người dùng, vai trò, lời mời, thu hồi).
- **Đầu ra:** `web/quan-tri/index.html`, `app.js`, `seats.js`, `quan-tri.css`; kịch bản Playwright với máy chủ giả.
- **Tiêu chí nghiệm thu:**
  1. Khung: điều hướng Tổng quan, Ghế và người dùng, Thiết bị, Lịch sử dùng, Triển khai, Máy chủ floating; mục Ghế hiện "k/n"; ở 390 px điều hướng thu thành menu.
  2. Vai trò: Chủ tài khoản, Quản trị, Quản trị phụ (chỉ xem, xuất báo cáo: nút sửa ẩn, API từ chối), Thành viên; đổi vai trò ngay trong bảng.
  3. Ghế gán theo người và ghế floating; tìm không dấu ("ket cau" khớp "Kết cấu").
  4. Mời hàng loạt: dán email (mỗi dòng một người, họ tên sau dấu phẩy; dán từ Excel, Outlook) hoặc chọn CSV; bảng xem trước báo email sai, trùng, đã có ghế, ngoài tên miền công ty, vượt số ghế trống; lời mời hết hạn sau 14 ngày, gửi lại được.
  5. Thu hồi một hoặc nhiều ghế: hộp xác nhận nêu tên, số ghế, số máy bị ảnh hưởng; hoàn tác ngay trên thông báo.
  6. Trạng thái rỗng (công ty mới), đang tải (khung xương), lỗi kết nối có Thử lại.
  7. Bảng 2.000 người dùng: lọc ≤ 50 ms, cuộn không giật.
  8. Đạt DoD-U (mục 1–3, 6, 7).

### UIL-024 · Cổng quản trị doanh nghiệp (web): thiết bị, lịch sử dùng, triển khai, cấu hình chung
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** UIL-023, PLF-011, PLF-009
- **Màn hình:** UI-L15
- **Đầu vào:** 07.3 §6 UI-L15 (Triển khai: bộ cài MSI; cấu hình mặc định cho cả công ty), UI-L18 (cài đặt im lặng); prototype `quan-tri.html` (Thiết bị, Lịch sử dùng, Triển khai; ghi chú biểu đồ có bảng số liệu thay thế, lệnh cài im lặng đổi theo lựa chọn, cấu hình có phiên bản và khóa, tệp `.tcconf`); API PLF-011; tham số MSI của PLF-009.
- **Đầu ra:** `web/quan-tri/devices.js`, `usage.js`, `deploy.js`; kịch bản Playwright.
- **Tiêu chí nghiệm thu:**
  1. Thiết bị: tên máy, người dùng, phiên bản, kết nối cuối; gỡ kích hoạt trả lại lượt kích hoạt; máy đang tắt nhận lệnh khi kết nối lại.
  2. Lịch sử dùng: lọc 7, 30, 90 ngày và loại ghế áp cho mọi số liệu; biểu đồ xem được bằng chuột và phím mũi tên, mỗi biểu đồ có bảng số liệu thay thế; xuất CSV theo bộ lọc.
  3. Triển khai: tải MSI; dòng lệnh cài im lặng cập nhật theo lựa chọn (thư mục, liên kết tệp, kênh, cấu hình), nút Sao chép.
  4. Cấu hình chung: hồ sơ, đường dẫn mẫu chung, thư viện chung, CTB chung; mỗi lần lưu thành một phiên bản; mục có dấu Khóa thì người dùng không đổi được trong Tùy chọn; xuất `.tcconf` cho máy không ra Internet; đường dẫn sai dạng báo dưới ô.
  5. Đạt DoD-U (mục 1–3, 6, 7).

### UIL-025 · Máy chủ giấy phép floating: giao diện quản lý
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIN-001, PLF-005
- **Màn hình:** UI-L15
- **Đầu vào:** 07.3 §6 UI-L15 (máy chủ floating: trạng thái, ghế đang dùng, nhật ký); prototype `quan-tri.html` mục "Máy chủ giấy phép floating" (Tổng quan, Ghế đang dùng, Hàng đợi, Nhật ký, Cổng mạng, Sao lưu; trạng thái Đang chạy, Hết ghế có hàng đợi, Dịch vụ dừng do lỗi, Đang khởi động); API quản trị của dịch vụ PLF-005.
- **Đầu ra:** `web/may-chu-giay-phep/` (trang tĩnh do dịch vụ PLF-005 phục vụ trong mạng LAN); kịch bản Playwright.
- **Tiêu chí nghiệm thu:**
  1. Đăng nhập bằng mật khẩu quản trị đặt khi cài; ngoài dải địa chỉ cho phép thì API từ chối và trang báo rõ.
  2. Ghế đang dùng: người, máy, từ lúc; thu hồi (máy khách có 5 phút để lưu). Hàng đợi: đưa lên đầu, bỏ khỏi hàng; ghế trống giữ 3 phút cho người đầu hàng.
  3. Dịch vụ dừng: nguyên nhân và cách khắc phục; khởi động có tiến trình từng bước và Hủy.
  4. Nhật ký 90 ngày: lọc theo mức, tìm, xuất `.log`. Sao lưu khóa máy chủ, cấu hình, nhật ký; khôi phục hỏi xác nhận.
  5. Biểu tượng khay hệ thống trên máy chủ mở trang này bằng trình duyệt mặc định **[đề xuất]**.
  6. Đạt DoD-U (mục 1–3, 6, 7).

## 9. Tự động hóa và tùy biến (UI-K01–K13)

### UIK-001 · Nạp ứng dụng (APPLOAD) và Startup Suite
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD:APPLOAD, CMD:SCRIPT, LSP-013, UI0-008, UI0-006
- **Màn hình:** UI-K01
- **Đầu vào:** 07.3 §5 UI-K01; 03 §19 (APPLOAD, Startup Suite P0); prototype `index.html#UI-K01`, `js/screens8.js` `IMPL.APPLOAD` (hộp `appl` 780×580, hộp `suite` 480, `loadApp`); API LSP-013 (nạp, gỡ, danh sách đã nạp, lịch sử, Startup Suite, kiểm thư mục tin cậy).
- **Đầu ra:** `app/custom/appload/AppLoadDialog.{h,cpp}`, `StartupSuiteDialog.{h,cpp}`; tests.
- **Tiêu chí nghiệm thu:**
  1. Thư mục (gần đây và duyệt), bộ lọc loại (LISP, DCL, script `*.lsp; *.dcl; *.scr` / đã biên dịch `*.vlx; *.fas` / plugin `*.tcplugin`); bảng tệp Tên, Ngày sửa (dd/mm/yyyy), Kích thước, nhãn "ngoài thư mục tin cậy"; thư mục trống thì "Không có tệp phù hợp."
  2. Nạp (nút hoặc nhấp đúp): `.lsp` qua LSP-013 (có thể qua cảnh báo UIK-002); `.scr` chạy SCRIPT; `.dcl` lỗi mở hộp lỗi DCL (UIK-007); `.vlx`, `.fas` báo "hỗ trợ sau v1.0. Dùng bản .lsp gốc"; dòng trạng thái "Đã nạp X".
  3. Ứng dụng đã nạp (tệp, đường dẫn) có nút Gỡ; Lịch sử, ô "Thêm vào lịch sử".
  4. Startup Suite: "Nội dung…" mở danh sách tự nạp khi khởi động (Thêm, Bớt); hộp chính hiện "N tệp tự nạp khi khởi động".
  5. Nút Bảo mật… mở SECURITYOPTIONS; chân hộp nêu các thư mục tin cậy.
  6. Mở hộp ≤ 150 ms với thư mục 2.000 tệp (liệt kê ở nền).
  7. Đạt DoD-U, DoD-L.

### UIK-002 · Cảnh báo bảo mật khi nạp mã
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** LSP-013, UI0-008
- **Màn hình:** UI-K02
- **Đầu vào:** 07.3 §5 UI-K02; 05 §16 (SECURELOAD mặc định bật); prototype `index.html#UI-K02`, `js/screens8.js` `SC.secWarn` (hộp `secw` 520, focus mặc định `#sw-no`); móc hỏi người dùng của LSP-013 (tệp, lý do, chữ ký số; kết quả Nạp một lần / Luôn nạp / Không nạp).
- **Đầu ra:** `app/custom/security/LoadWarningDialog.{h,cpp}` (đăng ký vào móc của LSP-013), `WritableFolderCheck.{h,cpp}` (quyền ghi của nhóm Everyone/Users trên Windows, bit quyền trên macOS/Linux); tests.
- **Tiêu chí nghiệm thu:**
  1. Nội dung: "Nạp X?", lý do (ngoài thư mục tin cậy / tệp `acad*.lsp` nằm cạnh bản vẽ, tự chạy khi mở bản vẽ / chưa ký số), đường dẫn, kích thước, ngày sửa dd/mm/yyyy, câu "Chỉ nạp mã từ nguồn bạn tin tưởng."
  2. Nút: Luôn nạp (tin cậy thư mục này), Nạp một lần, Không nạp. "Không nạp" có focus mặc định; Enter, Esc và nút đóng cửa sổ đều là Không nạp.
  3. Luôn nạp: thư mục vào TRUSTEDPATHS; thư mục ai cũng ghi được (Tải về, Desktop, Everyone/Users có quyền ghi) thì cảnh báo thêm trước khi thêm. Kiểm quyền ghi đúng trên 10 thư mục mẫu mỗi hệ điều hành.
  4. Hiện cho mọi đường nạp (APPLOAD, `(load)`, tự nạp `acad.lsp`/`acaddoc.lsp`, Startup Suite, LISP đi kèm tệp tùy biến); LISP đang chạy tạm dừng tới khi trả lời, vùng vẽ vẫn vẽ lại.
  5. Đạt DoD-U, DoD-L.

### UIK-003 · Tùy chọn bảo mật (SECURITYOPTIONS)
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UIK-002, LSP-013, INT-005, INT-001, CMD:SECURITYOPTIONS
- **Màn hình:** UI-K03
- **Đầu vào:** 07.3 §5 UI-K03; 07.2 UI-D10 tab Tệp (thư mục tin cậy) và Hệ thống (bảo mật); prototype `index.html#UI-K03`, `js/screens8.js` `IMPL.SECURITYOPTIONS` (hộp `secopt` 560); biến SECURELOAD, TRUSTEDPATHS (INT-005, LSP-013).
- **Đầu ra:** `app/custom/security/SecurityOptionsDialog.{h,cpp}`; tests.
- **Tiêu chí nghiệm thu:**
  1. Mức SECURELOAD: 0 nạp từ mọi nơi không cảnh báo; 1 cảnh báo khi ngoài thư mục tin cậy (khuyến nghị, mặc định); 2 chỉ nạp từ thư mục tin cậy. Chọn 0 hiện cảnh báo "Chỉ dùng trên máy không nối mạng".
  2. Danh sách thư mục tin cậy: Thêm…, Bớt; thư mục ai cũng ghi được có nhãn và toast giải thích rủi ro (dùng `WritableFolderCheck`).
  3. Ô chọn: cảnh báo khi bản vẽ đi kèm `acad.lsp`, `acaddoc.lsp`; chặn tự nạp LISP từ thư mục chứa bản vẽ.
  4. OK ghi SECURELOAD, TRUSTEDPATHS và hai cờ vào hồ sơ, có hiệu lực ngay ở lần nạp kế tiếp; Hủy không đổi gì.
  5. Gọi từ `.scr`, `(command)` hoặc `trion-console`: báo không có giao diện và trả lỗi, không treo; đổi mức bảo mật bằng `SETVAR SECURELOAD` vẫn dùng được.
  6. Đạt DoD-U, DoD-L.

### UIK-004 · Chạy script: chọn tệp, bảng tiến trình, lỗi theo dòng
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD:SCRIPT, INT-017, UI0-008, UI0-009
- **Màn hình:** UI-K04
- **Đầu vào:** 07.3 §5 UI-K04; prototype `index.html#UI-K04`, `js/screens8.js` `IMPL.SCRIPT` (hộp `scrsel` 520, bảng không modal `scr` 540, hộp lỗi `screrr` 500); sự kiện của INT-007 (dòng đang chạy, tổng số dòng, lỗi kèm số dòng); gợi ý lệnh gõ sai của INT-017.
- **Đầu ra:** `app/custom/script/ScriptPickDialog.{h,cpp}`, `ScriptProgressPanel.{h,cpp}`, `ScriptErrorDialog.{h,cpp}`; tests.
- **Tiêu chí nghiệm thu:**
  1. FILEDIA = 1: hộp chọn `.scr` (thư mục, Tên, Ngày sửa), ghi chú "Chạy một script trên nhiều bản vẽ có sau v1.0 (Xử lý tệp hàng loạt)". FILEDIA = 0: không mở hộp.
  2. Bảng tiến trình không modal: tên script, các dòng có đánh số, dòng đang chạy tô nền, "Dòng k/N", thanh tiến trình, "Esc để dừng script", nút Dừng. Dừng ở ranh giới dòng kế tiếp; dòng lệnh báo "Đã dừng script ở dòng k".
  3. Bảng chỉ hiện khi script chạy quá 0,5 s **[đề xuất]**, cập nhật ≤ 30 lần/giây; với `.scr` 10.000 dòng LINE, script chậm đi ≤ 2% so với khi không có bảng.
  4. Lỗi: hộp "Script dừng vì lỗi" nêu số dòng, nội dung dòng, thông điệp, gợi ý lệnh gần đúng ("Ý bạn là: LAYER?"), số dòng đã chạy; FILEDIA đang là 0 thì có nút "Đặt lại FILEDIA = 1".
  5. Bảng không lấy focus khỏi dòng lệnh; vùng vẽ vẫn vẽ lại khi script chạy.
  6. Đạt DoD-U, DoD-L.

### UIK-005 · Viết tắt lệnh: trình sửa PGP (ALIASEDIT)
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** INT-006, INT-001, UI0-008, UI0-006
- **Lệnh:** ALIASEDIT
- **Màn hình:** UI-K05
- **Đầu vào:** 07.3 §5 UI-K05; 07.3 §0 (viết tắt mặc định chốt sau khảo sát LISP Việt Nam); prototype `index.html#UI-K05`, `js/screens8.js` `IMPL.ALIASEDIT` (hộp `alias` 720×560, `aledit` 440, `alimp` 540); API INT-006 (viết tắt mặc định, người dùng, nhập từ AutoCAD; đọc/ghi `.pgp`; áp dụng như REINIT); registry lệnh INT-001.
- **Đầu ra:** `app/custom/alias/AliasEditorDialog.{h,cpp}`, `AliasEditDialog.{h,cpp}`, `PgpImportDialog.{h,cpp}`; `docs/specs/commands/ALIASEDIT.md`; tests.
- **Tiêu chí nghiệm thu:**
  1. Bảng Viết tắt | Lệnh | Mô tả (tự điền từ INT-001) | Nguồn (mặc định / người dùng / nhập từ AutoCAD); tìm theo viết tắt hoặc lệnh, lọc ≤ 16 ms với 3.000 dòng; dòng đếm "N viết tắt".
  2. Thêm, Sửa (nhấp đúp): kiểm khi gõ: chưa nhập; "Không có lệnh X"; "Trùng: A đang là viết tắt của B. Lưu sẽ thay thế."; "Che mất tên lệnh có sẵn A."; hợp lệ thì "Gõ A sẽ chạy B".
  3. Xóa viết tắt người dùng; viết tắt mặc định không xóa được, toast nêu cách thay.
  4. Nhập `acad.pgp`: tự tìm trong hồ sơ AutoCAD hoặc chọn tệp; bảng xung đột (Viết tắt, TrionCAD, acad.pgp); Gộp (giữ của TrionCAD) / Thay thế; toast nêu số đã nhập.
  5. Xuất `.pgp` (dạng `ALIAS, *COMMAND`) đọc lại được bằng INT-006 và AutoCAD. Đặt lại mặc định hỏi xác nhận nêu số viết tắt bị xóa.
  6. Áp dụng ngay: gõ viết tắt mới trên dòng lệnh chạy được ngay sau khi Lưu, không khởi động lại.
  7. Gọi từ `.scr`, `(command)` hoặc `trion-console`: báo không có giao diện, trả lỗi, không treo.
  8. Đạt DoD-U, DoD-L.

### UIK-006 · Hộp thoại DCL: dựng tile bằng Qt
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** LSP-018, UI0-006, UI0-008, UI0-012
- **Màn hình:** UI-K11
- **Đầu vào:** 07.3 §5 UI-K11; 05 §11 (DCL: layout xấp xỉ kích thước của AutoCAD, render bằng Qt); quy tắc từng loại tile ở `design/prototype/styleguide.html#dcl`; prototype `index.html#UI-K11`, `js/screens8.js` `SC.dclDemo` (hộp `dcl`), `css/app.css` (`.dclbox`, `.dclrow`, `.dcllist`, `.dclimg`); cây tile và bố cục (đơn vị ký tự) của LSP-018; đặc tả DCL công khai trong AutoLISP Developer's Guide.
- **Đầu ra:** `app/custom/dcl/DclDialog.{h,cpp}`, `DclTileFactory.{h,cpp}` (mỗi loại tile một lớp), `DclMetrics.{h,cpp}` (đơn vị ký tự DCL → pixel theo font giao diện và DPI); tests, fuzz target dựng hộp từ cây tile ngẫu nhiên.
- **Tiêu chí nghiệm thu:**
  1. Đủ tile theo styleguide: `dialog` (tiêu đề từ `label`, kéo được, theo chủ đề và DPI); `button` (`is_default` là nút chính); `ok_only`, `ok_cancel`, `ok_cancel_help`, `ok_cancel_help_info`, `ok_cancel_help_errtile` ở chân hộp căn phải (Hủy rồi OK, Trợ giúp bên trái); `edit_box` (nhãn bên trái, `edit_width` theo ký tự, tối thiểu 80 px, `edit_limit`, `password_char`); `popup_list`; `list_box` (`multiple_select`, `tabs`); `toggle`; `radio_button`, `radio_row`, `radio_column`, `boxed_radio_row`, `boxed_radio_column`; `row`, `column`, `boxed_row`, `boxed_column` (tiêu đề nằm trên viền); `text`, `text_part`, `concatenation`, `paragraph`; `slider` (giá trị bên phải); `image`, `image_button` (viền khi rê); `errtile` (ẩn khi trống); `spacer`, `spacer_0`, `spacer_1`.
  2. Thuộc tính bố cục và hành vi: `width`, `height`, `fixed_width`, `fixed_height`, `alignment`, `children_alignment`, `children_fixed_width`, `is_enabled`, `is_tab_stop`, `initial_focus`, `mnemonic`, `is_cancel`, `allow_accept`.
  3. Kích thước hộp lệch ≤ 10% mỗi chiều so với ảnh chụp AutoCAD của cùng tệp `.dcl` ở 100% **[đề xuất]**; ở 150% và 200% co giãn đúng theo DPI.
  4. Bàn phím: Tab theo thứ tự tile, `mnemonic` với Alt, Enter là nút mặc định (và `allow_accept`), Esc là `is_cancel`; tên cho trình đọc màn hình lấy từ `label` hoặc `key`.
  5. Hộp ≤ 200 tile tới khung đầu ≤ 150 ms.
  6. Fuzz dựng hộp từ cây tile ngẫu nhiên 1 giờ không crash, ASan sạch.
  7. Đạt DoD-U, DoD-L, DoD-C (mục 2, 3, 7).

### UIK-007 · Hộp thoại DCL: nối runtime, list, image và slide, hộp lỗi DCL
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIK-006, LSP-019
- **Màn hình:** UI-K11
- **Đầu vào:** 07.3 §5 UI-K11 ("DCL lỗi thì báo số dòng và nội dung lỗi"); prototype `js/screens8.js` `dclError` (hộp `dclerr` 520), `js/screens20.js` (dựng `thongkethep.dcl`); API LSP-019 (`new_dialog`, `start_dialog`, `done_dialog`, `term_dialog`, `action_tile`, `set_tile`, `get_tile`, `get_attr`, `mode_tile`, `client_data_tile`, `start_list`/`add_list`/`end_list`, `start_image`/`fill_image`/`vector_image`/`slide_image`/`end_image`, `dimx_tile`/`dimy_tile`).
- **Đầu ra:** `app/custom/dcl/DclRuntimeBridge.{h,cpp}`, `DclImageCanvas.{h,cpp}` (vẽ fill, vector, slide; màu ACI kể cả −2, −15, −16, −18), `DclErrorDialog.{h,cpp}`; tests.
- **Tiêu chí nghiệm thu:**
  1. `action_tile` nhận `$key`, `$value`, `$reason` (1–4), `$data`, `$x`, `$y` đúng như AutoCAD cho từng loại tile (40 ca vàng).
  2. `mode_tile` 0–4 (bật, tắt, focus, chọn chữ trong ô, đảo sáng ảnh); `start_list` thao tác 1/2/3 (đổi, thêm, tạo mới).
  3. Ảnh: `fill_image`, `vector_image`, `slide_image` (slide `.sld` do LSP-019 đọc) vẽ đúng tỉ lệ khung; màu −2 nền vùng vẽ, −15 nền hộp, −16 chữ hộp, −18 nét hộp theo chủ đề hiện tại.
  4. `done_dialog` trả mã và vị trí hộp; `new_dialog` lồng trong callback chạy đúng; khi hộp mở, LISP tạm dừng (LSP-003) mà vùng vẽ vẫn vẽ lại.
  5. Hộp lỗi DCL: tệp, dòng, cột, thông điệp, đoạn mã ±2 dòng có mũi tên chỉ dòng lỗi, nút "Sao chép lỗi", câu "LISP gọi hộp thoại này sẽ nhận giá trị lỗi từ load_dialog".
  6. Đạt DoD-U, DoD-L.

### UIK-008 · Hộp thoại DCL: nghiệm thu trên bộ LISP và ảnh chụp đối chứng
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIK-007, QA-008, QA-011
- **Màn hình:** UI-K11
- **Đầu vào:** bộ LISP QA-008 (mọi tệp `.dcl` đi kèm); ảnh chụp các hộp DCL trên AutoCAD do doanh nghiệp pilot gửi theo quy trình QA-007 **[đề xuất]**; harness QA-011.
- **Đầu ra:** `app/custom/dcl/tests/corpus/` (kịch bản mở từng dialog với dữ liệu mẫu), báo cáo HTML so sánh kích thước và ảnh; job CI hằng đêm.
- **Tiêu chí nghiệm thu:**
  1. 100% dialog trong bộ QA-008 (100 LISP ở EA, 500 ở 1.0) dựng được không crash; dialog lỗi cú pháp hiện hộp lỗi DCL đúng dòng.
  2. ≥ 95% dialog có kích thước lệch ≤ 10% so với ảnh AutoCAD; các ca lệch hơn vào báo cáo kèm ảnh **[đề xuất]**.
  3. Ảnh chuẩn Tối, Sáng, 100/150/200% cho 30 dialog đại diện; CI so ảnh qua QA-011.
  4. Job chạy ≤ 10 phút.
  5. Đạt DoD-U, DoD-L.

### UIK-009 · Chuyển cài đặt từ AutoCAD (TCMIGRATE): trình 5 bước
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CUS-003, UIL-009, UI0-008, UI0-009
- **Màn hình:** UI-K12
- **Đầu vào:** 07.3 §5 UI-K12; 03 §18 (chuyển cài đặt từ AutoCAD, P1); prototype `index.html#UI-K12`, `js/screens8.js` `IMPL.TCMIGRATE` (hộp `mig` 760×560, `STEPS`, `ITEMS`, `CONF`); API CUS-003 (phát hiện bản AutoCAD và hồ sơ, đếm mục, xung đột, chạy từng mục có tiến trình, báo cáo; lệnh TCMIGRATE).
- **Đầu ra:** `app/custom/migrate/MigrateWizard.{h,cpp}`, `MigrateItemModel`, `MigrateConflictModel`, `MigrateReportView`; tests.
- **Tiêu chí nghiệm thu:**
  1. Bước 1: danh sách bản AutoCAD và hồ sơ tìm thấy (tên, đường dẫn), hoặc "Chọn thư mục hoặc tệp (acad.pgp, .arg, .cuix)…"; không tìm thấy thì chỉ còn chọn tay.
  2. Bước 2: mục kèm số lượng; ở EA có viết tắt, đường dẫn hỗ trợ, mẫu bản vẽ, CTB/STB, LISP khởi động và Startup Suite; mục chưa có ở mốc hiện mờ "có ở bản 1.0".
  3. Bước 3: bảng xung đột (Viết tắt, TrionCAD, AutoCAD, cách xử lý: Giữ của TrionCAD / Ghi đè bằng AutoCAD / Đổi tên thêm hậu tố 2); nút áp dụng cho tất cả.
  4. Bước 4: tiến trình từng mục; Quay lại vô hiệu; Hủy dừng sau mục đang chạy và hoàn tác mục đó, không để cài đặt dở.
  5. Bước 5: tổng Đã chuyển / Bỏ qua / Cảnh báo, chi tiết từng mục, Lưu báo cáo…, "Mở hướng dẫn chuyển đổi" mở trợ giúp ở bài "Chuyển từ AutoCAD".
  6. Đạt DoD-U, DoD-L.

### UIK-010 · TCMIGRATE bản 1.0: các mục còn lại và CUIX một phần
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UIK-009, CUS-002, CUS-003, CUS-105
- **Màn hình:** UI-K12
- **Đầu vào:** 07.3 §5 UI-K12 (1.0: PC3/PMP, font SHX, thư mục tin cậy, màu và thiết lập bắt điểm, PAT/LIN tùy biến, CUIX một phần); 06 §5 (CUIX một phần ở 1.0); prototype `js/screens8.js` (mục `pc3`, `shx`, `trust`, `colors`, `patlin`, `cuix`; báo cáo CUIX).
- **Đầu ra:** mở rộng `MigrateWizard`, `MigrateReportView`; tests.
- **Tiêu chí nghiệm thu:**
  1. Các mục mới bật ở 1.0 kèm số lượng; font SHX đã có bản TrionVN tương đương thì báo dùng bản Unicode.
  2. CUIX: báo cáo số tab ribbon, toolbar, menu, macro đã nhập vào workspace "AutoCAD đã chuyển"; macro DIESEL phức tạp liệt kê riêng là cần sửa tay; ribbon và tool palette ghi "P2".
  3. Toolbar và menu nhập từ CUIX hiện như workspace Cổ điển và bật được trong mọi workspace (kiểm trên 10 tệp CUIX mẫu).
  4. Đạt DoD-U, DoD-L.

### UIK-011 · Biến hệ thống: trình duyệt (SYSVDLG)
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** INT-005, INT-001, CMD:SETVAR, UI0-006, UI0-008
- **Lệnh:** SYSVDLG
- **Màn hình:** UI-K13
- **Đầu vào:** 07.3 §5 UI-K13 (Trình duyệt); 07.1 §7.10 (panel Công cụ: SYSVDLG); prototype `index.html#UI-K13`, `js/screens14.js` `SC.sysvars` (hộp `sysv` 940×600, tab "Tất cả biến"), `DEFS`, `SV.parse`; registry INT-005 (kiểu, miền, mặc định, nơi lưu, mô tả tiếng Việt, chỉ đọc).
- **Đầu ra:** `app/custom/sysvar/SysvarBrowserDialog.{h,cpp}`, `SysvarTableModel`, `SysvarDetailPane`, `SysvarCsv.{h,cpp}`; `docs/specs/commands/SYSVDLG.md`; tests, fuzz target `SysvarCsv`.
- **Tiêu chí nghiệm thu:**
  1. Tìm theo tên hoặc mô tả tiếng Việt không dấu; lọc Tất cả (N) / Khác mặc định (n) / Đang theo dõi (m); không khớp thì gợi ý "Thử tên tiếng Anh (OSMODE) hoặc mô tả tiếng Việt (bắt điểm)".
  2. Bảng Tên | Giá trị (kèm nhãn liệt kê) | Mặc định | Lưu ở (Bản vẽ / Người dùng / Không lưu) | Mô tả; biến khác mặc định có dấu chấm và chữ "đã đổi", không chỉ dựa vào màu.
  3. Sửa tại ô: số và chuỗi trong ô nhập (Enter áp dụng, Esc bỏ); liệt kê và bật/tắt bằng menu; lỗi theo INT-005 ("Cần một số nguyên. Dấu thập phân là dấu chấm.", "Chỉ nhận 0 hoặc 1.", "Phải từ a đến b.", danh sách giá trị hợp lệ); biến chỉ đọc không sửa được.
  4. Khung chi tiết: kiểu, miền, giá trị, mặc định, nơi lưu và giải thích; mã bit (OSMODE…) sửa bằng ô chọn từng bit; Đặt về mặc định; Theo dõi biến này.
  5. Mọi thay đổi đi qua SETVAR; biến lưu trong bản vẽ hoàn tác được bằng U.
  6. Xuất CSV (Tên, Giá trị, Mặc định, Lưu ở, Mô tả); Nhập CSV hỏi xác nhận nêu số biến sẽ đổi; biến lưu trong bản vẽ chỉ áp cho bản vẽ đang mở; biến lạ bỏ qua và ghi nhật ký.
  7. 250 biến: mở ≤ 150 ms, lọc ≤ 16 ms; fuzz `SysvarCsv` 1 giờ không lỗi.
  8. Đạt DoD-U, DoD-L.

### UIK-012 · Theo dõi biến hệ thống (SYSVARMONITOR) và cảnh báo trên thanh trạng thái
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIK-011, INT-005, UIA-009, UI0-009
- **Lệnh:** SYSVARMONITOR
- **Màn hình:** UI-K13
- **Đầu vào:** 07.3 §5 UI-K13 (Theo dõi); 07.1 §10; `design/prototype/styleguide.html` (mục "Theo dõi biến hệ thống UI-K13"); prototype `js/screens14.js` (`MON`, `SV.drift`, chip `#sb-sysv`, `driftPop`, tab "Theo dõi"); nguồn của mỗi lần đổi biến (tệp LISP, SETVAR, script) do INT-005 ghi.
- **Đầu ra:** `app/custom/sysvar/SysvarMonitorPage.{h,cpp}`, `SysvarDriftChip.{h,cpp}`, `SysvarDriftPopup.{h,cpp}`; `docs/specs/commands/SYSVARMONITOR.md`; tests.
- **Tiêu chí nghiệm thu:**
  1. Danh sách mặc định: OSMODE, CMDECHO, FILEDIA, PICKFIRST, ORTHOMODE, ATTREQ, ATTDIA, PEDITACCEPT, MIRRTEXT, PICKADD; cột Biến, Giá trị chuẩn, Hiện tại, Trạng thái (Đúng chuẩn / Đã đổi · do X); Thêm biến…, Lấy giá trị hiện tại làm chuẩn, Khôi phục tất cả (n), Khôi phục từng biến, Bỏ theo dõi.
  2. Ô chọn "Báo trên thanh trạng thái khi một biến bị đổi" và "Hiện thông báo ngay khi đổi".
  3. Chip "N biến đã đổi" chỉ hiện khi có biến lệch (ẩn ở Viewer và tab Start); bấm mở popup: từng biến cũ → mới, nguồn, Khôi phục; Khôi phục tất cả; Theo dõi biến….
  4. Nhiều biến đổi trong 250 ms (cùng một LISP) gom thành một toast có nút Khôi phục; khôi phục không sinh toast mới.
  5. Chip cập nhật ≤ 16 ms sau sự kiện đổi biến; LISP đổi 1.000 biến liên tiếp không làm UI khựng quá 1 khung.
  6. Đạt DoD-U, DoD-L.

### UIK-013 · Phím tắt (TCKEYS)
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UI0-015, INT-001, UI0-008, UI0-006
- **Lệnh:** TCKEYS
- **Màn hình:** UI-K06
- **Đầu vào:** 07.3 §5 UI-K06; 07.1 §11 (phím mặc định), §13.6 (F-key trên laptop macOS); design/README ("Phím tắt một nguồn"); prototype `index.html#UI-K06`, `js/screens14.js` `SC.shortcuts` (hộp `keys` 960×620, `KDEF`, `FIXED`, `RESERVED`, `comboOf`, `capture`), `TC.keyLabel`; nguồn phím của UI0-015.
- **Đầu ra:** `app/custom/shortcuts/ShortcutsDialog.{h,cpp}`, `KeyCaptureWidget.{h,cpp}`, `ShortcutFile.{h,cpp}` (`.tckeys` **[đề xuất]**); `docs/specs/commands/TCKEYS.md`; tests, fuzz target `ShortcutFile`.
- **Tiêu chí nghiệm thu:**
  1. Tab Phím tắt; tab "Phím ghi đè tạm thời · sau v1.0" vô hiệu. Tìm theo tên, lệnh, tổ hợp, nhãn macOS; lọc Thường dùng / Đã gán / Đã đổi (n); bảng nhóm theo Chung, Hỗ trợ vẽ, Tệp, Sửa, Palette và hiển thị, Lệnh khác: Lệnh hoặc hành động | Tên lệnh | Phím tắt | Phạm vi (Toàn cục / Vùng vẽ).
  2. Gán: bấm "Bấm vào đây rồi nhấn tổ hợp phím" rồi nhấn tổ hợp, Esc để thôi. Từ chối tổ hợp của hệ điều hành và bộ gõ (Ctrl+Alt+Delete, Alt+F4, Alt+Tab, Ctrl+Shift+Esc, Ctrl+Space, ⌘⇧3, ⌘⇧4, ⌘H, ⌘M) kèm lý do; phím đơn bị từ chối, có liên kết mở Viết tắt lệnh.
  3. Trùng: "X đang dùng cho Y (LỆNH). Gán lại cho lệnh này / Giữ như cũ"; sau khi Lưu, không có hai hành động cùng tổ hợp trong cùng phạm vi (kiểm toàn bảng).
  4. macOS: nhãn ⌘, ⌃, ⌥, ⇧ theo UI0-015; F3–F12 có tổ hợp thay thế, hiện viền đứt.
  5. Đặt về mặc định từng lệnh; Đặt lại tất cả hỏi xác nhận nêu số phím đã đổi; Nhập `.tckeys` hỏi xác nhận nêu số phím mới và số trùng; Xuất `.tckeys`.
  6. Lưu áp dụng ngay cho menu, tooltip, ribbon và bảng in (một nguồn); Hủy bỏ bản nháp.
  7. Lọc ≤ 16 ms với 1.500 dòng; fuzz `ShortcutFile` 1 giờ không lỗi.
  8. Đạt DoD-U, DoD-L.

### UIK-014 · In bảng phím tắt (PDF một trang)
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UIK-013, INT-006
- **Màn hình:** UI-K06
- **Đầu vào:** 07.3 §5 UI-K06 ("in bảng phím tắt (PDF một trang)"); prototype `js/screens14.js` `cheatSheet` (hộp `keysheet` 1000×720); viết tắt hiện hành từ INT-006.
- **Đầu ra:** `app/custom/shortcuts/CheatSheetDialog.{h,cpp}`, `CheatSheetRenderer.{h,cpp}` (QPdfWriter; in qua hộp in của Qt); tests.
- **Tiêu chí nghiệm thu:**
  1. Xem trước khổ A4 ngang, một trang, đen trắng: tiêu đề kèm nền tảng (Windows / macOS) và ngày in; cột theo nhóm phím; cột Phím cố định; khối "Gõ tắt lệnh hay dùng" (22 viết tắt hiện hành); dòng chân.
  2. Dùng phím hiện hành của người dùng, nhãn theo nền tảng; tới 150 mục vẫn vừa một trang với cỡ chữ ≥ 7 pt **[đề xuất]**.
  3. Lưu PDF… (chữ chọn được, font nhúng tập con); In… mở hộp in của hệ điều hành.
  4. Đạt DoD-U, DoD-L.

### UIK-015 · Workspace: lưu, thiết lập, chuyển
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CUS-004, INT-001, UIA-001, UIA-009, UI0-008
- **Lệnh:** WSSAVE, WSSETTINGS
- **Màn hình:** UI-K08
- **Đầu vào:** 07.3 §5 UI-K08; 07.1 §3.1 (workspace có sẵn); 03 §1 (WORKSPACE, WSSAVE, P1); prototype `index.html#UI-K08`, `js/screens14.js` (`TC.wsMenu`, `SC.wsSave` hộp `wssave` 480, `SC.wsSettings` hộp `wsset` 560, `SC.wsSwitch`, `IMPL.WORKSPACE`); mô hình workspace CUS-004 (chụp bố cục, áp dụng, danh sách, thứ tự, ẩn, lệnh WORKSPACE).
- **Đầu ra:** `app/custom/workspace/WorkspaceMenu.{h,cpp}`, `WorkspaceSaveDialog.{h,cpp}`, `WorkspaceSettingsDialog.{h,cpp}`; `docs/specs/commands/WSSAVE.md`, `WSSETTINGS.md`; tests.
- **Tiêu chí nghiệm thu:**
  1. Menu từ bánh răng thanh trạng thái: "Workspace của tôi: X", danh sách (nhãn "có sẵn", dấu chọn ở workspace hiện hành), Lưu workspace hiện tại…, Thiết lập workspace…, Tùy biến giao diện… (khóa tới khi có CUI).
  2. Lưu: tên (danh sách workspace người dùng); liệt kê mục được lưu (ribbon hoặc thanh menu và toolbar; palette, vị trí dock, ghim/tự ẩn; tab tệp, tab layout, dòng lệnh; mục thanh trạng thái); tên trùng workspace có sẵn bị từ chối; trùng workspace người dùng thì hỏi ghi đè; ô "Chuyển sang workspace này sau khi lưu".
  3. Thiết lập: Workspace của tôi; hiện trong menu (ô chọn) và thứ tự (Lên, Xuống); Đổi tên, Xóa chỉ với workspace người dùng (xóa hỏi xác nhận); khi chuyển: không lưu / tự lưu thay đổi vào workspace đang dùng.
  4. Chuyển workspace: palette, dock, tab, dòng lệnh đổi theo, khung hình đầu đủ bố cục ≤ 150 ms; dòng lệnh báo "<Workspace: X>".
  5. WSSAVE và WSSETTINGS từ `.scr` hoặc `trion-console` báo không có giao diện, trả lỗi, không treo.
  6. Đạt DoD-U, DoD-L.

### UIK-016 · Tùy biến giao diện (CUI): cây tùy biến, thuộc tính, áp dụng
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** CUS-005, INT-001, UI0-015, UIA-005, UI0-008
- **Lệnh:** CUI
- **Màn hình:** UI-K07
- **Đầu vào:** 07.3 §5 UI-K07; 03 §19 (trình sửa CUI P2); prototype `index.html#UI-K07`, `js/screens20.js` mục "Tùy biến giao diện UI-K07" (hộp `cui` 1180×684, cây, khung Thuộc tính, `diff`, đóng khi còn thay đổi); backend CUS-005 (tệp tùy biến chính và phụ, áp dụng, khôi phục mặc định).
- **Đầu ra:** `app/custom/cui/CuiDialog.{h,cpp}`, `CuiTreeModel`, `CuiPropertyPane`, `CuiPreview`; `docs/specs/commands/CUI.md`; tests.
- **Tiêu chí nghiệm thu:**
  1. Cây: workspace, QAT, tab và panel ribbon, toolbar, menu, menu chuột phải, phím tắt (đọc nguồn UI0-015), hành động nhấp đúp, nút chuột, tệp LISP đi kèm, tệp tùy biến phụ (của plugin: chỉ đọc).
  2. Khung thuộc tính sửa nhãn, biểu tượng, macro, tooltip, cỡ nút; kéo-thả trong cây để sắp và thêm; xem trước panel ribbon hoặc toolbar đang chọn.
  3. "Áp dụng N thay đổi" áp ngay, không khởi động lại; đóng khi còn thay đổi hỏi "Bỏ N thay đổi chưa áp dụng?"; Khôi phục mặc định… hỏi xác nhận.
  4. Áp dụng 500 thay đổi ≤ 1 s, ribbon vẽ lại không khựng quá 1 khung **[đề xuất]**.
  5. Đạt DoD-U, DoD-L.

### UIK-017 · Tùy biến giao diện (CUI): danh sách lệnh, lệnh mới, chuyển giữa hai tệp, nhập CUIX
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** UIK-016, CUS-001, CUS-002
- **Màn hình:** UI-K07
- **Đầu vào:** 07.3 §5 UI-K07 (Danh sách lệnh, Khác); prototype `js/screens20.js` (danh sách lệnh, "Lệnh mới", hộp chọn biểu tượng `cuicon` 540, tab "Chuyển giữa hai tệp", "Nhập CUIX…"); thực thi macro của CUS-001; nhập CUIX một phần của CUS-002.
- **Đầu ra:** `app/custom/cui/CommandListPane.{h,cpp}`, `NewCommandEditor.{h,cpp}`, `IconPickerDialog.{h,cpp}`, `CuiTransferPage.{h,cpp}`; tests.
- **Tiêu chí nghiệm thu:**
  1. Danh sách lệnh tìm theo tên, mô tả, từ khóa tiếng Việt; lọc ≤ 16 ms với 3.000 lệnh.
  2. Lệnh mới: tên (không trùng), macro (kiểm cú pháp `^C^C`, `;`, `\`, `$M=` qua CUS-001, báo vị trí lỗi), biểu tượng (bộ của UI0-004 hoặc SVG của người dùng), tooltip; lệnh mới chạy được bằng nút và bằng tên.
  3. Tab chuyển: hai cây cạnh nhau, kéo mục từ tệp này sang tệp kia, lưu tệp đích.
  4. Nhập CUIX của AutoCAD (ribbon, toolbar, menu): báo cáo mục nhập được và mục bỏ qua.
  5. Đạt DoD-U, DoD-L.

### UIK-018 · Bảng điều khiển LISP (LISPCONSOLE)
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** UIK-002, LSP-003, LSP-005, LSP-013, INT-001, UI0-010, LSP-126
- **Lệnh:** LISPCONSOLE
- **Màn hình:** UI-K09
- **Đầu vào:** 07.3 §5 UI-K09; prototype `index.html#UI-K09`, `js/screens20.js` mục "Bảng điều khiển LISP UI-K09" (palette `lisp`: tab `con`, `var`, `file`; hộp `lspfile` 620; vết gọi hàm); API LSP-003 (đánh giá, tạm dừng/tiếp tục, ngắt), LSP-005 (lỗi, vết gọi hàm, biến cục bộ từng khung), LSP-013 (nạp, nạp lại).
- **Đầu ra:** `app/custom/lispconsole/LispConsolePalette.{h,cpp}`, `ReplWidget`, `GlobalsView`, `LoadedFilesView`, `TraceView`; `docs/specs/commands/LISPCONSOLE.md`; tests.
- **Tiêu chí nghiệm thu:**
  1. REPL: ô nhập nhiều dòng (Shift+Enter xuống dòng, Enter chạy), lịch sử ↑ ↓ giữ qua phiên; kết quả in như AutoLISP kèm nhãn kiểu (INT, REAL, STR, SYM, LIST, ENAME, PICKSET…), T và nil ghi riêng; thời gian chạy.
  2. Tab Biến: biến toàn cục, tìm, giá trị rút gọn. Tab Tệp: tệp đã nạp, Nạp lại, Nạp tệp LISP… (qua kiểm thư mục tin cậy và cảnh báo của UIK-002).
  3. Lỗi: dừng kèm vết gọi hàm (khung trong cùng trước; tệp và dòng; biến cục bộ); "Mở trong VS Code" mở tệp ở dòng lỗi bằng ứng dụng ngoài.
  4. Nút Dừng và Esc ngắt vòng lặp dài ≤ 100 ms **[đề xuất]**; khi LISP chạy vùng vẽ vẫn vẽ lại.
  5. `getpoint`, `entsel` gọi từ bảng điều khiển hỏi trên dòng lệnh rồi trả kết quả về REPL.
  6. Đạt DoD-U, DoD-L.

### UIK-019 · Quản lý plugin (PLUGINS)
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** SDK-003, INT-001, UI0-008, UI0-006, SDK-102
- **Lệnh:** PLUGINS
- **Màn hình:** UI-K10
- **Đầu vào:** 07.3 §5 UI-K10; prototype `index.html#UI-K10`, `js/screens20.js` mục "Quản lý plugin UI-K10" (hộp `plug` 980×620, `pltrust`, `plprog`, `plfile`); API SDK-003 (manifest, chữ ký số, quyền, lệnh của plugin, bật/tắt, cài, cập nhật, gỡ, chính sách doanh nghiệp).
- **Đầu ra:** `app/custom/plugins/PluginManagerDialog.{h,cpp}`, `PluginTrustDialog`, `PluginProgressDialog`, `PluginInstallDialog`; `docs/specs/commands/PLUGINS.md`; tests.
- **Tiêu chí nghiệm thu:**
  1. Danh sách: tên, nhà phát triển, phiên bản, đã ký số hay chưa, trạng thái (đang chạy, tắt, lỗi); bật/tắt; chi tiết: quyền yêu cầu, các lệnh plugin thêm vào.
  2. Cài từ tệp gói: plugin chưa ký hiện hộp tin cậy nêu rủi ro, nhà phát hành tự khai, quyền yêu cầu, "Không cài" là nút mặc định; bản Doanh nghiệp có chính sách chặn plugin chưa ký thì hộp chặn kèm lý do.
  3. Tiến trình cài và cập nhật (giải nén, kiểm tra, đăng ký lệnh) có Hủy; Gỡ hỏi xác nhận.
  4. Gõ lệnh của plugin đang tắt hoặc lỗi: dòng lệnh nêu lý do và cách bật.
  5. Đạt DoD-U, DoD-L.

## 10. Nội dung và công cụ TCVN (UI-J11, J13, J05, J06)

### UIJ-010 · Chọn mẫu bản vẽ TCVN
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-008, CMD:NEW, CMD:LAYOUT, DWG-026, UI0-008
- **Màn hình:** UI-J11
- **Đầu vào:** 07.3 §2 UI-J11; 03 Phụ lục A (VN: mẫu TCVN); prototype `index.html#UI-J11`, `js/screens7.js` `SC.newTemplate` (hộp `tpl` 820×560, `TPLS`, `PAPER`); mẫu `.dwt` A0–A4 của VN-008 kèm metadata (khổ, hướng, bộ môn, tỉ lệ, kiểu khung tên); ảnh xem trước trong `.dwt` (DWG-026).
- **Đầu ra:** `app/vn/templates/TemplatePickerDialog.{h,cpp}`, `TemplateCatalog.{h,cpp}` (quét thư mục mẫu của VN-008, của người dùng và thư mục mẫu chung của công ty); trình mở cho NEW và cho "Layout mới từ mẫu"; tests.
- **Tiêu chí nghiệm thu:**
  1. Lọc theo Khổ (A0–A4), Hướng, Bộ môn (kiến trúc, kết cấu, M&E, giao thông, cơ khí), Tỉ lệ, Khung tên (TCVN 5571, TCVN 7284, mẫu công ty, không khung); Bỏ lọc; không khớp thì "Không có mẫu khớp bộ lọc."
  2. Lưới mẫu có tên và dòng mô tả; khung Xem trước (ảnh trong `.dwt`; thiếu ảnh thì vẽ khổ giấy và khung tên theo metadata) và thông tin: khổ, bộ môn, tỉ lệ, khung tên, nội dung có sẵn (layer, kiểu chữ, kiểu kích thước, CTB).
  3. Tạo (nút chính hoặc nhấp đúp) chạy NEW với mẫu đã chọn; ở chế độ layout chạy LAYOUT từ mẫu; tiêu đề hộp đổi theo chế độ.
  4. Nhớ bộ lọc và mẫu chọn gần nhất (NT-010); "Mở mẫu khác…" mở hộp chọn tệp `.dwt`.
  5. Mở hộp ≤ 150 ms đến khung đầu với 200 mẫu; ảnh xem trước nạp ở nền.
  6. FILEDIA = 0 hoặc gọi từ `.scr`: không mở hộp, NEW hỏi tên mẫu trên dòng lệnh như lõi quy định.
  7. Đạt DoD-U, DoD-L.

### UIJ-011 · Palette Thư viện TCVN: duyệt, tìm không dấu, xem trước, chèn
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** VN-009, CMD:INSERT, CMD:HATCH, CMD:LINETYPE, UI0-010, UIA-014
- **Màn hình:** UI-J13
- **Đầu vào:** 07.3 §2 UI-J13; 07.1 §7.9 (panel TCVN), §12 (trạng thái rỗng "[Mở Thư viện TCVN]"); prototype `index.html#UI-J13`, `js/screens7.js` `P.tcvnlib` (cây danh mục, `@hatch`, `@lt`, yêu thích, vị trí thư viện, menu chuột phải, kéo-thả `text/x-tc-block`); API VN-009 (chỉ mục, tìm kiếm, ảnh xem trước, metadata tiêu chuẩn, cách chèn theo tỉ lệ); API thả lên vùng vẽ của UIA-014.
- **Đầu ra:** `app/vn/tcvnlib/TcvnLibraryPalette.{h,cpp}`, `LibraryTreeModel`, `LibraryItemModel`, `ThumbnailCache.{h,cpp}`; trình mở palette cho TCLIB; tests.
- **Tiêu chí nghiệm thu:**
  1. Cây: Kiến trúc, Kết cấu, Điện, Cấp thoát nước, Chung (mỗi nhánh con kèm số mục); nhóm "Vật liệu, nét, kiểu" (Hatch vật liệu TCVN, Linetype TCVN); nhóm "Của tôi" (Yêu thích, Thư viện của tôi).
  2. Tìm không dấu ("cua di" khớp "Cửa đi"), lọc khi gõ ≤ 16 ms trên chỉ mục 5.000 mục; không thấy thì "Không tìm thấy “…”. Tìm không cần dấu…".
  3. Lưới ô có ảnh xem trước nạp ở nền, cache theo DPI; rê hiện tên và thông tin tiêu chuẩn (tham chiếu, kích thước, ghi chú).
  4. Bấm ô chạy INSERT với block của mục: ký hiệu chú thích tự áp tỉ lệ chú thích hiện hành, vật thể thật chèn 1:1 theo đơn vị bản vẽ (theo metadata VN-009). Kéo-thả lên vùng vẽ chèn tại điểm thả, có xem trước theo con trỏ.
  5. Hatch vật liệu: bấm mẫu chạy HATCH với mẫu đó. Linetype: "Nạp" chạy LINETYPE nạp kiểu; kiểu đã có ghi "đã có".
  6. Chuột phải: Chèn, Thêm vào/Bỏ khỏi Yêu thích, Thông tin tiêu chuẩn; yêu thích lưu theo người dùng.
  7. Vị trí thư viện: thư mục máy hoặc thư mục mạng dùng chung; mất kết nối thư mục mạng thì dùng chỉ mục đã cache, palette có banner, mục đã cache vẫn chèn được **[đề xuất]**.
  8. Viewer: palette hiện trạng thái khóa kèm giải thích, nút "Dùng thử Pro 30 ngày" và lối dẫn tới bản LISP miễn phí.
  9. Mở palette ≤ 150 ms đến khung đầu; lưới 5.000 ô cuộn 60 fps.
  10. Đạt DoD-U, DoD-L.

### UIJ-012 · Thư viện TCVN: Thư viện của tôi, bộ layer, kiểu và mẫu
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIJ-011, VN-009, CMD:WBLOCK, VN-161
- **Màn hình:** UI-J13
- **Đầu vào:** 07.3 §2 UI-J13 (bộ layer theo bộ môn, Thư viện của tôi, thư mục mạng dùng chung); prototype `js/screens7.js` `P.tcvnlib` (`@layers` với `SETS`, `@style`, `#lb-add`, `#ls-load`); API VN-009 (ghi mục vào thư viện của người dùng; tùy chọn dòng lệnh để nạp bộ layer, kiểu chữ, kiểu kích thước, CTB, mẫu vào bản vẽ).
- **Đầu ra:** `app/vn/tcvnlib/MyLibraryPage.{h,cpp}`, `LayerSetPage.{h,cpp}`, `StyleSetPage.{h,cpp}`, `AddToLibraryDialog.{h,cpp}`; tests.
- **Tiêu chí nghiệm thu:**
  1. "Thêm vào thư viện" với đối tượng đang chọn: tên (không trùng trong danh mục), danh mục, điểm gốc (chỉ trên bản vẽ), mô tả; ghi block qua WBLOCK vào Thư viện của tôi, ảnh xem trước có ngay. Chưa chọn đối tượng thì báo "Chọn đối tượng trên bản vẽ trước…".
  2. Bộ layer theo bộ môn (KT, KC, ĐN, CTN, M&E): bảng Tên, Màu, Nét, Dày; layer trùng tên có nhãn "trùng tên"; lựa chọn "Giữ của bản vẽ" / "Ghi đè thuộc tính"; "Nạp vào bản vẽ" là một lệnh, một mark undo; toast "thêm a layer, ghi đè b".
  3. Kiểu chữ, kiểu kích thước, CTB, mẫu bản vẽ TCVN: mỗi mục có "Nạp vào bản vẽ"; mục đã có ghi "đã có" và hỏi trước khi ghi đè.
  4. Thư viện của tôi trống: trạng thái rỗng có hướng dẫn; thư mục mạng chỉ đọc: nút thêm vô hiệu kèm lý do.
  5. Đạt DoD-U, DoD-L.

### UIJ-013 · Khung tên TCTITLE: chèn từ mẫu và sửa attribute
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** VN-010, UI0-008, UI0-007, UIA-008
- **Màn hình:** UI-J05
- **Đầu vào:** 07.3 §2 UI-J05 (phần EA: chèn từ thư viện, sửa attribute); prototype `index.html#UI-J05`, `js/screens11.js` `IMPL.TCTITLE` (hộp `tct` 940×660: tab Sửa khung tên, Chèn khung tên), `tbPaint`, `ensureFrame`; API VN-010 (khung tên trong bản vẽ, mẫu, chèn theo khổ và tỉ lệ, ghi attribute; tùy chọn dòng lệnh của TCTITLE).
- **Đầu ra:** `app/vn/titleblock/TitleBlockDialog.{h,cpp}`, `TitleBlockPreview.{h,cpp}` (vẽ mẫu khung có giá trị), `TitleBlockForm.{h,cpp}`; tests.
- **Tiêu chí nghiệm thu:**
  1. Tab "Sửa khung tên (N)": danh sách khung trong Model và các layout (ký hiệu, nơi đặt); chọn khung thì viền khung đó tô sáng trên vùng vẽ; biểu mẫu nhóm "Công trình và tờ" và "Người thực hiện" theo các ô của mẫu (đơn vị thiết kế, công trình, hạng mục, tên bản vẽ, ký hiệu, giai đoạn, tỉ lệ, người vẽ / kiểm / chủ trì / chủ nhiệm / duyệt, ngày, số tờ / tổng số tờ).
  2. Ký hiệu trùng với khung khác: lỗi dưới ô, không áp dụng. Áp dụng chạy TCTITLE, một mark undo; toast "Đã cập nhật N khung tên. Hoàn tác bằng Ctrl+Z."
  3. Tab "Chèn khung tên": mẫu (TCVN 5571 ngang/dọc, mẫu công ty), khổ A0–A4, hướng, tỉ lệ (trong Model) hoặc 1:1 (trong layout, tự chọn theo không gian hiện hành), bộ môn; xem trước cập nhật ≤ 100 ms khi đổi tùy chọn; Chèn đóng hộp, lệnh hỏi điểm chèn có xem trước theo con trỏ.
  4. Bản vẽ chưa có khung tên: trạng thái rỗng kèm nút "Chèn khung tên" chuyển sang tab Chèn.
  5. Ngày dạng dd/mm/yyyy; tỉ lệ dạng 1:100; ô số dùng UI0-007.
  6. Đạt DoD-U, DoD-L.

### UIJ-014 · Khung tên: trình sửa đầy đủ, ánh xạ attribute, logo, mẫu công ty
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIJ-013, VN-010, VN-018
- **Màn hình:** UI-J05
- **Đầu vào:** 07.3 §2 UI-J05 (trình sửa khung tên, 1.0); prototype `js/screens11.js` `IMPL.TCTITLE` (tab "Ánh xạ attribute", `mapPane`, `SS.map`, `logoRow`, phạm vi `tcs`, `#tc-save`, `#tc-fill`, `redefine`); API VN-018 (thông tin dự án, thông tin tờ, ánh xạ nguồn dữ liệu, cập nhật khung tên của cả bộ).
- **Đầu ra:** `app/vn/titleblock/AttributeMappingPage.{h,cpp}`, `CompanyTemplateDialog.{h,cpp}`, mở rộng `TitleBlockDialog`; tests.
- **Tiêu chí nghiệm thu:**
  1. Ánh xạ: mỗi attribute chọn nguồn (Dự án, Tờ, field, nhập tay); cột giá trị xem trước theo tờ đang chọn; ô của mẫu v1.0 chưa có trong khung cũ ghi "mẫu v1.0".
  2. "Tự điền ô từ bộ bản vẽ (n)" điền mọi ô khác giá trị nguồn; n hiện trên nút, bằng 0 thì nút vô hiệu.
  3. Logo: chọn ảnh PNG hoặc JPG, xem trước trong khung, Bỏ logo.
  4. Phạm vi áp dụng: chỉ khung này / các ô chung cho mọi khung cùng loại trong bản vẽ (nêu số khung) / cả bộ bản vẽ (nêu số tờ; chỉ khi bản vẽ thuộc bộ); nút chính đổi chữ theo phạm vi ("Áp dụng cho 6 khung"). Áp dụng trong bản vẽ là một mark undo.
  5. Khung thiếu ô so với mẫu: cảnh báo liệt kê ô thiếu, ô chọn "cập nhật định nghĩa khung theo mẫu v1.0, giữ nội dung cũ".
  6. Lưu thành mẫu công ty: tên không trùng; lưu vào thư mục mẫu chung (cấu hình công ty); mẫu mới xuất hiện ở tab Chèn và ở TCFRAME.
  7. Đạt DoD-U, DoD-L.

### UIJ-015 · Tạo khung in trong Model theo tỉ lệ (TCFRAME)
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIJ-013, VN-010, UI0-007
- **Màn hình:** UI-J06
- **Đầu vào:** 07.3 §2 UI-J06; prototype `index.html#UI-J06`, `js/screens11.js` `IMPL.TCFRAME` (hộp `tcf` 800, `FR`, `read`, `upd`, `noAt`); API VN-010 (TCFRAME: khổ, tỉ lệ, mẫu, mảng n × m, khoảng cách, đánh số tờ).
- **Đầu ra:** `app/vn/titleblock/FrameArrayDialog.{h,cpp}`; tests.
- **Tiêu chí nghiệm thu:**
  1. Nhóm Khổ giấy, tỉ lệ, mẫu (A0–A4; Ngang/Dọc; 1:20, 1:25, 1:50, 1:75, 1:100, 1:200, 1:500; mẫu TCVN 5571 hoặc mẫu công ty); Cách đặt (Từng khung / Mảng n cột × m hàng, cách g mm giấy); Số tờ (tự đánh số, bộ môn, số bắt đầu, tự điền thông tin dự án).
  2. Kiểm hợp lệ: số cột, số hàng 1–20; khoảng cách 0–500; số bắt đầu 1–999; lỗi hiện dưới nhóm, nút chính vô hiệu tới khi sửa.
  3. Thông tin: khung trong Model (khổ × tỉ lệ), vùng vẽ trong khung, tên block, số khung; dòng xem trước số tờ ("KT 01, KT 02 … KT 06"); nút chính "Đặt N khung". Xem trước trong hộp cập nhật ≤ 100 ms.
  4. Gọi khi đang ở layout hoặc trong viewport kích hoạt: thông báo dùng TCTITLE, không mở hộp.
  5. Đặt xong: toast "Đã tạo N khung … có attribute" kèm nút "In theo khung tên" (chạy TCBATCHPLOT).
  6. Đạt DoD-U, DoD-L.

## 11. Công cụ văn bản, ký hiệu, Excel (UI-J07–J10, J12, J14, J15)

### UIJ-016 · Đánh số tự động (TCNUMBER): hộp thoại cạnh vùng vẽ, xem trước giá trị
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD:TCNUMBER, UI0-006, UI0-007, UI0-008, UIA-013
- **Màn hình:** UI-J07
- **Đầu vào:** 07.3 UI-J07; prototype `index.html#UI-J07`, `js/screens11.js` `IMPL.TCNUMBER`; API đích, định dạng, thứ tự, preset và xem trước của CMD:TCNUMBER (VN-011).
- **Đầu ra:** `app/vn/text/NumberDialog.{h,cpp}`, `app/vn/text/NumberPreviewModel.{h,cpp}`; `app/vn/text/tests/`, `app/vn/text/tests/ui/`.
- **Tiêu chí nghiệm thu:**
  1. Hộp không chặn, đặt cạnh vùng vẽ; đủ nhóm như prototype: Đích (Text có sẵn / Attribute kèm chọn tag / Text mới), Định dạng (mẫu có sẵn, tiền tố, Số hoặc Chữ cái, bắt đầu, bước, số chữ số 1/01/001/0001, hậu tố), Thứ tự (thứ tự chọn, X tăng, Y giảm, hàng rồi cột).
  2. Đổi vùng chọn hoặc một trường: bảng xem trước (#, Hiện tại, Mới; ảo hóa) và nhãn "→ giá trị mới" trên vùng vẽ cập nhật ≤ 16 ms phần đồng bộ với 1.000 đối tượng **[đề xuất]**; nhãn đi theo pan/zoom, mất khi đóng hộp.
  3. Nút chính "Đánh số N text|block" vô hiệu khi không giá trị nào đổi; áp dụng chạy TCNUMBER qua INT-002, một mark undo; giá trị sau khi áp dụng khớp từng ký tự bảng xem trước trên 20 ca vàng (01→12, Z→AA, bước âm, có hậu tố).
  4. Mẫu có sẵn (trục số, trục chữ, phòng, cọc, thiết bị) và "Lưu mẫu…" ghi vào kho preset của lệnh, còn sau khi khởi động lại.
  5. Đích Attribute mà vùng chọn không có block chứa tag: trạng thái rỗng kèm nút chọn trên bản vẽ; Esc khi đang chọn quay lại hộp, giữ nguyên cài đặt.
  6. Đạt DoD-U, DoD-L.

### UIJ-017 · TCNUMBER chỉ điểm từng cái và TCINCCOPY: prompt, chip, bóng theo con trỏ
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UIJ-016, CMD:TCINCCOPY, UI-E10, UIA-013, UI0-009
- **Màn hình:** UI-J07, UI-J08
- **Đầu vào:** 07.3 UI-J07 (chế độ chỉ điểm), UI-J08; prototype `js/screens11.js` `numPoints`, `IMPL.TCINCCOPY`; prompt và từ khóa do CMD:TCNUMBER, CMD:TCINCCOPY (VN-164) khai báo qua INT-003.
- **Đầu ra:** `app/vn/text/NumberPointMode.{h,cpp}`; `app/dialogs/prompts/i18n/prompts_vn_number.{vi,en}.ts`, `app/dialogs/prompts/expect/vn_number.json`; ảnh baseline QA-011.
- **Tiêu chí nghiệm thu:**
  1. "Chỉ điểm từng cái" ẩn hộp UI-J07 và chạy chế độ chỉ điểm: prompt ghi giá trị kế tiếp ("Bấm vào text để đặt “C3”"), chip Hoàn tác / Xong; Enter hoặc Esc trả về hộp với số bắt đầu đã tăng.
  2. Bấm vào chỗ không hợp lệ: chú thích cạnh con trỏ "Không có text tại đây" hoặc "Không phải block có attribute TAG" (UI0-009), không tạo bước undo.
  3. TCINCCOPY: dòng tóm tắt "Nguồn · bước · phần số", chip Bước / Phần số (Cuối, Đầu, Thứ n) / Hoàn tác; bóng bản sao kèm giá trị mới theo con trỏ ≤ 8 ms/khung.
  4. Ca vàng `C1`→`C2`, `1.05`→`1.10`, `A`→`B`, `09`→`10`: chuỗi trên prompt và trên bóng khớp kết quả lệnh.
  5. Bộ kiểm chip UI-E10 chạy `vn_number.json`: 100% prompt có bản vi và en, chip khớp từ khóa của lệnh.
  6. Đạt DoD-U, DoD-L.

### UIJ-018 · Công cụ text (TCTEXT): khung hộp thoại, Hoa/thường, Tiền tố và hậu tố, Đổi số
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD:TCTEXT, CMD:TCASE, UI0-006, UI0-007, UI0-008, UIA-013
- **Màn hình:** UI-J09
- **Đầu vào:** 07.3 UI-J09; prototype `index.html#UI-J09`, `js/screens11.js` `IMPL.TCTEXT`, `txCalc`; API xem trước và áp dụng của CMD:TCTEXT, CMD:TCASE (VN-012).
- **Đầu ra:** `app/vn/text/TextToolsDialog.{h,cpp}`, `app/vn/text/panes/{CasePane,AffixPane,NumberOpPane}.{h,cpp}`; `app/vn/text/tests/`.
- **Tiêu chí nghiệm thu:**
  1. Hộp không chặn, tab dọc 6 công cụ (Hoa, thường · Nối text · Tách text · Canh hàng · Tiền tố, hậu tố · Đổi số trong text); TCTEXT kèm tên công cụ mở thẳng tab đó; TCASE mở tab Hoa, thường.
  2. Hoa, thường: HOA TOÀN BỘ, thường toàn bộ, Hoa đầu câu, Hoa Mỗi Từ; có ghi chú khi text mã TCVN3 phải đổi kiểu chữ sang font "H" để giữ dấu.
  3. Tiền tố, hậu tố (tìm ở đầu/cuối, thay bằng) và Đổi số (Cộng/Nhân, giá trị ô UI-C09, số cuối/đầu/mọi số): bảng Hiện tại → Sau khi đổi và nhãn trên vùng vẽ khớp từng ký tự kết quả lệnh trên 30 ca vàng (`±0.000` + 0,450 → `+0.450`; "ư" → "Ư").
  4. Đổi vùng chọn: xem trước cập nhật ≤ 16 ms phần đồng bộ với 1.000 text **[đề xuất]**; chưa chọn thì trạng thái rỗng "Chưa chọn text"; nút "Đổi N text" vô hiệu khi N = 0; áp dụng là một mark undo.
  5. Đạt DoD-U, DoD-L.

### UIJ-019 · Công cụ text (TCTEXT): Nối, Tách, Canh hàng, bóng kết quả trên vùng vẽ
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIJ-018, CMD:TCTEXT, UIA-013
- **Màn hình:** UI-J09
- **Đầu vào:** 07.3 UI-J09 (nối, tách, canh hàng); prototype `js/screens11.js` `txCalc` nhánh `join`, `split`, `align`; API xem trước hình học của CMD:TCTEXT.
- **Đầu ra:** `app/vn/text/panes/{JoinPane,SplitPane,AlignPane}.{h,cpp}`; kịch bản QA-011 trong `app/vn/text/tests/ui/`.
- **Tiêu chí nghiệm thu:**
  1. Nối: thứ tự (trên → dưới, trái → phải, theo thứ tự chọn), ký tự nối (khoảng trắng, dấu phẩy, xuống dòng, tự nhập), kết quả TEXT hoặc MTEXT; dưới 2 text thì ghi chú "Chọn ít nhất 2 text để nối" và nút chính vô hiệu.
  2. Tách: MTEXT theo dòng, hoặc theo ký tự (ô ký tự tách); không text nào tách được thì ghi chú lý do.
  3. Canh hàng: trái/giữa/phải theo text chọn đầu tiên, tùy chọn chia đều khoảng cách; bảng ghi số text đổi vị trí và X của đường canh.
  4. Bóng đối tượng mới và vị trí mới vẽ trên overlay màu nhấn ≤ 8 ms/khung với 500 text **[đề xuất]**; bản vẽ không đổi trước khi áp dụng; áp dụng là một mark undo và chọn lại đối tượng kết quả.
  5. Đạt DoD-U, DoD-L.

### UIJ-020 · Ký hiệu kỹ thuật (TCSYMBOL): khung hộp thoại, cao độ, mặt cắt, chi tiết, hướng Bắc, độ dốc, vật liệu
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD:TCSYMBOL, UI0-006, UI0-007, UI0-008, UIA-013, RND-021
- **Màn hình:** UI-J10
- **Đầu vào:** 07.3 UI-J10; prototype `index.html#UI-J10`, `js/screens11.js` `IMPL.TCSYMBOL`, `symPreview`; API ký hiệu và block TCVN của CMD:TCSYMBOL (VN-166).
- **Đầu ra:** `app/vn/symbol/SymbolDialog.{h,cpp}`, `app/vn/symbol/SymbolPreview.{h,cpp}`, `app/vn/symbol/panes/`; `app/vn/symbol/tests/`.
- **Tiêu chí nghiệm thu:**
  1. Hộp có tab dọc theo loại ký hiệu và ô Tỉ lệ chung; khung xem trước vẽ ký hiệu bằng render ngoài màn hình (RND-021), cập nhật ≤ 50 ms sau mỗi thay đổi **[đề xuất]**.
  2. Cao độ: kiểu mặt bằng hoặc mặt cắt; giá trị theo Y so với gốc ±0.000 (ô UI-C09 có nút lấy từ bản vẽ) hoặc nhập tay; hiển thị dạng `+1.200`.
  3. Mặt cắt và chi tiết: số hiệu, tờ tham chiếu chọn từ danh sách tờ ("1-1 / KT 05"); hướng Bắc: góc so với phương dọc; độ dốc: giá trị và đơn vị % hoặc ‰; vật liệu: chọn mẫu từ gallery hatch TCVN.
  4. Nút chính theo loại ("Đặt cao độ", "Đặt ký hiệu mặt cắt"…) đóng hộp và chạy TCSYMBOL qua INT-002; prompt chỉ điểm có bóng ký hiệu theo con trỏ ≤ 8 ms/khung; mở lại hộp giữ giá trị lần trước.
  5. Đạt DoD-U, DoD-L.
- **Ngoài phạm vi:** cao độ tự cập nhật khi di chuyển (P2).

### UIJ-021 · Ký hiệu kỹ thuật (TCSYMBOL): bảng lưới trục định vị
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIJ-020, VN-012
- **Màn hình:** UI-J10
- **Đầu vào:** 07.3 UI-J10 (trục định vị); prototype `js/screens11.js` tab Trục định vị, `axesEnts`; API lưới trục của VN-012 (gồm VN-167).
- **Đầu ra:** `app/vn/symbol/panes/AxisGridPane.{h,cpp}`, `app/vn/symbol/AxisTableModel.{h,cpp}`; kiểm thử trong `app/vn/symbol/tests/`.
- **Tiêu chí nghiệm thu:**
  1. Hai bảng sửa được cho trục ngang (1, 2, 3…) và trục dọc (A, B, C…): cột tên trục và khoảng cách tới trục trước (mm, ô UI-C09); thêm, chèn, xóa, đổi thứ tự bằng bàn phím; tên tự tăng khi thêm dòng.
  2. Tùy chọn bóng trục (hai đầu / đầu dưới, trái), độ kéo dài, kích thước khoảng cách, kích thước tổng; xem trước cập nhật ≤ 50 ms với lưới 30 × 30 trục **[đề xuất]**.
  3. "Lấy từ bản vẽ": chọn các đường trục có sẵn, bảng điền tên và khoảng cách đo được; đường không song song báo lỗi theo dòng.
  4. Khoảng cách ≤ 0 hoặc rỗng tô lỗi và chặn nút "Vẽ lưới trục"; chỉ điểm gốc có bóng toàn lưới ≤ 8 ms/khung; vẽ xong là một mark undo.
  5. Đạt DoD-U, DoD-L.
- **Ngoài phạm vi:** sửa lưới trục sau khi tạo (P2).

### UIJ-022 · Excel: hộp Dán từ Excel và Xuất bảng ra Excel (TCEXCELPASTE, TCEXCELOUT)
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD:TCEXCELPASTE, CMD:TCEXCELOUT, UI0-006, UI0-008, UI0-009, UIA-013
- **Màn hình:** UI-J12
- **Đầu vào:** 07.3 UI-J12; prototype `index.html#UI-J12`, `js/screens7.js` `IMPL.TCEXCELPASTE`, `IMPL.TCEXCELOUT`; API của VN-013: vùng ô đọc từ clipboard, kiểu bảng, tùy chọn xuất.
- **Đầu ra:** `app/vn/excel/ExcelPasteDialog.{h,cpp}`, `app/vn/excel/ExcelExportDialog.{h,cpp}`; `app/vn/excel/tests/`.
- **Tiêu chí nghiệm thu:**
  1. Dán: dòng nguồn (vùng ô, tên tệp, số cột × hàng, số ô gộp) và bảng xem trước giữ ô gộp; dán thành Bảng TrionCAD hoặc Chữ thường (MText, cột cách bằng tab); Đối tượng OLE hiện mờ kèm lý do "sau v1.0".
  2. Kiểu bảng chọn từ bản vẽ; cỡ chữ hiện dạng "2.5 mm × 1:100 = 250" khi bật theo tỉ lệ chú thích; Dán chuyển sang prompt điểm chèn có bóng bảng ≤ 8 ms/khung; một mark undo.
  3. QA-011: Ctrl+V khi clipboard có vùng ô Excel mở hộp này; clipboard không có vùng ô thì TCEXCELPASTE báo trên dòng lệnh, không mở hộp.
  4. Xuất: tóm tắt số bảng và kích thước; XLSX hoặc CSV (CSV mỗi bảng một tệp, tắt "Giữ ô gộp"); giữ định dạng số; tên tệp mặc định theo tiêu đề bảng; "Mở tệp sau khi xuất"; toast có nút Mở thư mục.
  5. Ghi tệp lỗi (tệp đang mở trong Excel, không có quyền): hộp lỗi có chi tiết (UI0-008), giữ nguyên lựa chọn để thử lại.
  6. Đạt DoD-U, DoD-L.

### UIJ-023 · Attribute với Excel: khung hai tab và tab Xuất (TCATTOUT)
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD:TCATTOUT, UI0-006, UI0-008, UI0-009, UI0-011
- **Màn hình:** UI-J15
- **Đầu vào:** 07.3 UI-J15 (Xuất); prototype `index.html#UI-J15` (phiên bản Viewer Pro), `js/screens4.js` `SC.attOut`, `SC.attTabs`; API của CMD:TCATTOUT (VN-014): block có attribute, tag, phạm vi, ghi tệp.
- **Đầu ra:** `app/vn/excel/AttributeExcelDialog.{h,cpp}` (khung hai tab), `app/vn/excel/AttOutPane.{h,cpp}`; `app/vn/excel/tests/`.
- **Tiêu chí nghiệm thu:**
  1. Hộp hai tab Xuất / Nhập; khi chưa có khả năng nhập (mốc VP, Viewer Pro) tab Nhập mang huy hiệu khóa (UI0-011), bấm mở trang nâng cấp.
  2. Chọn block theo tên (danh sách có ô đánh dấu, số bản, Chọn tất cả / Bỏ chọn) hoặc theo vùng chọn; phạm vi cả bản vẽ hoặc layout hiện tại; danh sách tag cập nhật theo block đã chọn.
  3. Bảng xem trước cột Handle (ẩn được), Tên block, Layout và các tag; ảo hóa, cuộn 60 fps với 10.000 block; số block và tag cập nhật ≤ 16 ms phần đồng bộ **[đề xuất]**.
  4. XLSX hoặc CSV, "Mở tệp sau khi xuất", tên tệp mặc định theo bản vẽ; xuất chạy nền, > 2 s có tiến trình và nút hủy (UI0-009); xong có toast kèm nút Mở.
  5. Xuất không tạo mark undo và không đánh dấu bản vẽ đã sửa (Viewer Pro không sửa DWG).
  6. Đạt DoD-U, DoD-L.

### UIJ-024 · Attribute với Excel: tab Nhập, xem trước thay đổi, áp dụng (TCATTIN)
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIJ-023, CMD:TCATTIN
- **Màn hình:** UI-J15
- **Đầu vào:** 07.3 UI-J15 (Nhập); prototype `js/screens12.js` `SC.attIn`; API đọc tệp và tính chênh lệch của CMD:TCATTIN (VN-153).
- **Đầu ra:** `app/vn/excel/AttInPane.{h,cpp}`, `app/vn/excel/AttDiffModel.{h,cpp}`; kiểm thử trong `app/vn/excel/tests/`.
- **Tiêu chí nghiệm thu:**
  1. Tab Nhập mở được ở bản Pro: chọn tệp XLSX hoặc CSV, gợi ý sẵn tệp vừa xuất; đọc và so khớp chạy nền, > 2 s có tiến trình và nút hủy.
  2. Bảng xem trước cột Dòng, Handle, Block, Layout, Tag, Giá trị cũ, Giá trị mới; bộ lọc Tất cả / Thay đổi / Cảnh báo; cảnh báo cho dòng không tìm thấy đối tượng và tag không tồn tại; ảo hóa với 10.000 dòng.
  3. Trạng thái rỗng và lỗi như prototype: bản vẽ không có block có attribute; chưa chọn tệp; tệp thiếu cột Handle (hướng dẫn xuất lại bằng TCATTOUT).
  4. Nút "Áp dụng N thay đổi" vô hiệu khi N = 0; cả lần nhập là một mark undo; số dòng cảnh báo bị bỏ qua ghi trong toast.
  5. Đạt DoD-U, DoD-L.

### UIJ-025 · Thống kê thép (TCREBAR): bảng thống kê, tổng hợp, kiểm tra, chèn bảng
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** CMD:TCREBAR, UIJ-022, UI0-006, UI0-007, UI0-008, UIA-013
- **Màn hình:** UI-J14
- **Đầu vào:** 07.3 UI-J14; prototype `index.html#UI-J14`, `js/screens17.js` `IMPL.TCREBAR`; API nhận dạng, bảng thống kê và quy tắc của CMD:TCREBAR (VN-170).
- **Đầu ra:** `app/vn/rebar/RebarDialog.{h,cpp}`, `app/vn/rebar/RebarTableModel.{h,cpp}`, `app/vn/rebar/ShapeSketch.{h,cpp}`; `app/vn/rebar/tests/`.
- **Tiêu chí nghiệm thu:**
  1. Hộp không chặn; nguồn Ký hiệu chữ / Block thanh thép / Cả hai; nhận dạng chạy nền, có tiến trình khi > 2 s; không tìm thấy ký hiệu thì trạng thái rỗng có hướng dẫn.
  2. Bảng mẫu TCVN: số hiệu, phác hình và kích thước, đường kính, dài một thanh, số lượng một cấu kiện, tổng số lượng, tổng dài (m), khối lượng (kg); sửa số lượng ngay trong bảng; chọn dòng thì tô sáng ký hiệu nguồn trên vùng vẽ ≤ 100 ms.
  3. Bảng tổng hợp theo đường kính (mác thép, khối lượng đơn vị kg/m, khối lượng) bật/tắt được; tổng khớp bảng chi tiết đến 0,01 kg trên 10 bản vẽ mẫu.
  4. Danh sách ký hiệu thiếu thông tin, bấm một dòng thì phóng tới; đổi thông số nhóm Quy tắc nhận dạng thì tính lại.
  5. Chèn bảng (chọn kiểu bảng, điểm chèn có bóng ≤ 8 ms/khung, một mark undo) và Xuất Excel qua hộp xuất của UIJ-022.
  6. Đạt DoD-U, DoD-L.

## 12. Tab Mở rộng: hộp thoại và prompt (UI-MR, UI-MR2)

### UIK-020 · Tab Mở rộng P1: TCOUNT và TCASE nối với công cụ VN, nghiệm thu màn hình UI-MR
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UIA-006, UI-E10, UIJ-016, UIJ-018, CMD:TCOUNT, CMD:TCASE
- **Màn hình:** UI-MR
- **Đầu vào:** 07.1 §7.10; prototype `index.html#UI-MR`, `js/screens14.js` `OPEN['UI-MR']`, `js/screens10.js` `IMPL.BURST`…`IMPL.SAVEALL`; prompt P1 của tab đã có ở UI-E10 (phần 2); prompt TCOUNT do CMD:TCOUNT (VN-011) khai báo.
- **Đầu ra:** `app/custom/express/ExpressLaunchers.{h,cpp}` (trình mở giao diện của TCASE); `app/dialogs/prompts/i18n/prompts_tcount.{vi,en}.ts`, `app/dialogs/prompts/expect/tcount.json`; kịch bản QA-011 trong `app/custom/express/tests/ui/`.
- **Tiêu chí nghiệm thu:**
  1. Nút Đánh số mở UI-J07 với vùng chọn hiện có; TCOUNT gõ tay, từ `.scr` hoặc `(command ...)` chạy prompt kiểu Express để script cũ vẫn chạy.
  2. Nút Đổi hoa/thường và TCASE khi `FILEDIA` = 1 mở UI-J09 ở tab Hoa, thường; `-TCASE` chạy prompt.
  3. Prompt TCOUNT (sắp theo [X/Y/Thứ tự chọn]; số bắt đầu và bước; vị trí số [Ghi đè/Tiền tố/Hậu tố/Tìm và thay]) có chip vi/en; bộ kiểm chip UI-E10 đạt 100% với `tcount.json`.
  4. QA-011 bấm từng nút P1 của tab (panel Block, Văn bản, Sửa, Tệp, Công cụ): đúng lệnh chạy qua INT-002, prompt hoặc hộp đầu tiên hiện ≤ 100 ms; nút P2 ẩn đến mốc Sau; tooltip có tên lệnh Express gốc và F1.
  5. Đạt DoD-U, DoD-L.

### UIK-021 · Tab Mở rộng P2: hộp thoại block và chữ (XLIST, BLOCKTOXREF, ARCTEXT, TEXTMASK, RTEXT, SUPERHATCH)
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** CMD:XLIST, CMD:BLOCKTOXREF, CMD:ARCTEXT, CMD:RTEXT, CMD:TEXTMASK, CMD:SUPERHATCH, UI0-006, UI0-007, UI0-008, UIA-013
- **Màn hình:** UI-MR2
- **Đầu vào:** 07.1 §7.10 (panel Block, Văn bản, Vẽ); prototype `index.html#UI-MR2`, `js/screens21.js` hộp `xlist`, `b2x`, `arctext`, `tmset`, `rtpick`, `rtedit`, `shatch`; API tham số của từng lệnh.
- **Đầu ra:** `app/custom/express/{XListDialog,BlockToXrefDialog,ArcTextDialog,TextMaskDialog,RTextDialogs,SuperHatchDialog}.{h,cpp}`; `app/custom/express/tests/`.
- **Tiêu chí nghiệm thu:**
  1. Đối tượng lồng (XLIST): hộp không chặn hiện block chứa, loại, layer, màu, linetype của đối tượng lồng vừa chọn; chọn tiếp không cần đóng hộp.
  2. Đổi block thành xref: chọn block và tệp xref đích, xem trước số tham chiếu sẽ thay; xác nhận nêu số lượng.
  3. Chữ theo cung (tạo và sửa): nội dung, kiểu chữ, chiều cao, hệ số rộng, giãn ký tự, khoảng cách tới cung, căn, hướng trong/ngoài; bóng trên cung đã chọn ≤ 8 ms/khung.
  4. Che nền chữ (kiểu che, hệ số lề ô UI-C09), RTEXT (chọn tệp chữ, sửa nội dung hoặc biểu thức DIESEL), SuperHatch (ảnh, block, xref, vùng chọn làm mẫu; sửa lại): đủ trường như prototype, kiểm hợp lệ trước khi chạy lệnh.
  5. Mỗi hộp chạy lệnh qua INT-002 với tham số đã chọn, một mark undo; `FILEDIA` = 0 hoặc tiền tố `-` bỏ qua hộp.
  6. Đạt DoD-U, DoD-L.

### UIK-022 · Tab Mở rộng P2: hộp thoại layout, kích thước, tệp, công cụ (VPSCALE, DIMEX, DIMIM, REDIR, XDATA, EDITTIME)
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD:DIMEX, UI0-006, UI0-007, UI0-008, UI0-009, CMD:VPSCALE, CMD:REDIR, CMD:XDATA, CMD:EDITTIME
- **Màn hình:** UI-MR2
- **Đầu vào:** 07.1 §7.10 (panel Layout, Kích thước, Tệp, Công cụ); prototype `js/screens21.js` hộp `vpscale`, `dimex`, `dimim`, `redir`, `fpick`, `xdata`, `edittime`; API của task lõi sở hữu từng lệnh.
- **Đầu ra:** `app/custom/express/{VpScaleDialog,DimExImDialogs,RedirDialog,XDataDialog,EditTimeDialog}.{h,cpp}`; `app/custom/express/tests/`.
- **Tiêu chí nghiệm thu:**
  1. Tỉ lệ nhiều viewport: danh sách viewport đã chọn kèm tỉ lệ hiện tại; chọn tỉ lệ từ danh sách tỉ lệ của bản vẽ hoặc nhập; viewport bị khóa hiện biểu tượng khóa và được bỏ qua.
  2. Xuất/nhập kiểu kích thước: chọn kiểu và tệp `.dim`; khi nhập liệt kê kiểu trùng tên kèm lựa chọn ghi đè hoặc bỏ qua; xong báo số kiểu đã xuất/nhập.
  3. Đổi đường dẫn hàng loạt: bảng đường dẫn xref, ảnh, font, kiểu chữ theo loại; tìm và thay chuỗi, chọn thư mục; xem trước cũ → mới, đường dẫn không tồn tại có biểu tượng cảnh báo kèm chữ.
  4. Gắn xdata (chọn ứng dụng, thêm giá trị có kiểu, kiểm kiểu từng dòng) và Thời gian sửa bản vẽ (đồng hồ chạy, tạm dừng, đặt lại); đồng hồ chỉ cập nhật khi hộp hiện, ẩn hộp thì 0% CPU.
  5. Mỗi hộp chạy lệnh qua INT-002, một mark undo khi đổi bản vẽ; `FILEDIA` = 0 hoặc tiền tố `-` bỏ qua hộp.
  6. Đạt DoD-U, DoD-L.

### UIK-023 · Tab Mở rộng P2: prompt và chip
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** S · **Phụ thuộc:** UI-E10, CMD:BSCALE, CMD:TEXTFIT, CMD:MOCORO, CMD:BREAKLINE, CMD:MKSHAPE, CMD:MKLTYPE, CMD:SHP2BLK, CMD:ATTOUT, CMD:ALIGNSPACE, CMD:VPSYNC, CMD:QLATTACH, CMD:REVERT, CMD:QQUIT, CMD:MOVEBAK, CMD:XDLIST
- **Màn hình:** UI-MR2
- **Đầu vào:** 07.1 §7.10 (lệnh P2); prototype `js/screens21.js` các `IMPL` của tab Mở rộng phần P2; prompt và từ khóa do task lõi khai báo qua INT-003.
- **Đầu ra:** `app/dialogs/prompts/i18n/prompts_mr2.{vi,en}.ts`, `app/dialogs/prompts/expect/mr2.json` (kể cả ALIGNSPACE, VPSYNC, QLATTACH, REVERT, QQUIT, MOVEBAK, XDLIST); ảnh baseline QA-011.
- **Tiêu chí nghiệm thu:**
  1. Prompt và chip vi/en cho BSCALE, TEXTFIT, TORIENT, TEXTUNMASK, MOCORO, MSTRETCH, BREAKLINE, SHAPE, MKSHAPE, MKLTYPE, SHP2BLK, ATTOUT, ATTIN và phần prompt sau hộp thoại của ARCTEXT, RTEXT, SUPERHATCH.
  2. Bộ kiểm chip UI-E10 chạy `mr2.json`: 100% prompt có chip khớp từ khóa của lệnh; chuỗi dài hơn 40% không bị cắt.
  3. Prompt có bóng (BREAKLINE, MOCORO, MSTRETCH, TEXTFIT, BSCALE) cập nhật ≤ 8 ms/khung.
  4. Đạt DoD-U, DoD-L.

## Tóm tắt theo mốc

| Mốc | Task | Số task |
|---|---|---|
| CC | UIN-001, UIN-002, UIN-003, UIN-004, UIN-005 | 5 |
| V | UIJ-001, UIJ-002, UIJ-003, UIJ-004, UIJ-005, UIL-001, UIL-002, UIL-003, UIL-004, UIL-005, UIL-006, UIL-007, UIL-008, UIL-009, UIL-010, UIL-011, UIL-012, UIL-013, UIL-014 | 19 |
| VP | UIJ-006, UIJ-007, UIJ-009, UIL-015, UIL-016, UIL-017, UIL-018, UIM-001, UIM-002, UIM-003, UIM-004, UIM-005, UIL-021, UIL-022, UIJ-023 | 15 |
| EA | UIJ-008, UIL-019, UIM-006, UIK-001, UIK-002, UIK-003, UIK-004, UIK-005, UIK-006, UIK-007, UIK-008, UIK-009, UIJ-010, UIJ-011, UIJ-013, UIJ-022 | 16 |
| 1.0 | UIL-020, UIL-023, UIL-024, UIL-025, UIK-010, UIK-011, UIK-012, UIK-013, UIK-014, UIK-015, UIJ-012, UIJ-014, UIJ-015, UIJ-016, UIJ-017, UIJ-018, UIJ-019, UIJ-020, UIJ-021, UIJ-024, UIK-020 | 21 |
| Sau | UIK-016, UIK-017, UIK-018, UIK-019, UIJ-025, UIK-021, UIK-022, UIK-023 | 8 |
| **Tổng** | | **84** |
