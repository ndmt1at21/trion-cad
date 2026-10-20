# UI 0 — Nền tảng giao diện và khung ứng dụng

> Nền giao diện Qt (ứng dụng `trion`, luồng UI, theme, font, biểu tượng, bản địa hóa, thành phần, ô nhập số, hộp thoại, thông điệp, palette, cổng phiên bản, DPI, tiếp cận, hiệu năng UI, hành động và phím tắt) và khung ứng dụng (UI-A01–A15, M01, M03, M04, bản macOS). Tiền tố `UI0` (mục 1–7), `UIA` (mục 8 trở đi).
> Thư mục mã: `app/core/`, `app/theme/`, `app/widgets/`, `app/shell/`. Mẫu task, DoD, ranh giới sở hữu và task neo: [README](README.md).
> Căn cứ: [07.1](../07-dac-ta-ui-1-khung-ung-dung.md) §1, §2, §12, §13 · [07.2](../07-dac-ta-ui-2-bang-va-hop-thoai.md) UI-C09, quy ước áp dụng · [07.3](../07-dac-ta-ui-3-tinh-nang-vn-va-he-thong.md) Phụ lục C · [05](../05-kien-truc-ky-thuat.md) ADR-002 · `design/README.md`, `design/prototype/css/tokens.css`, `design/prototype/styleguide.html`.

## 0. Ghi chú thiết kế

- **Nền tảng:** Qt 6 Widgets, LGPLv3 link động; vùng vẽ là widget QRhi (05 ADR-002). Docking, tự ẩn, tooltip, toast tự viết trên Qt; không thêm thư viện UI ngoài 05 §2.
- **Luồng:** luồng UI sở hữu document và mọi widget. Việc > 2 ms chạy nền qua NT-005, kết quả về luồng UI theo lô (UI0-101). UI đọc bản vẽ qua snapshot DOC-009; sửa bản vẽ chỉ qua chuỗi lệnh INT-002 (UI0-015).
- **Token là hợp đồng:** tên và kích thước giữ đúng `tokens.css` (UI0-102 sinh mã). Lint chặn màu, cỡ chữ, khoảng cách viết thẳng (UI0-002).
- **Hướng A "Than chì":** khung xám than trung tính để màu ACI không lẫn; một màu nhấn xanh ngọc chỉ cho trạng thái bật, focus, hành động chính; bo góc 2 px (`--r`). Không làm mờ, không bóng lớn: bóng chỉ cho menu và hộp thoại; Giao diện nhẹ thay bằng viền 1 px.
- **Kích thước (gọn mặc định / thoáng):** dòng palette 22/26 px, thân ribbon 87/94 px, tab ribbon 26 px, điều khiển hộp thoại 24/26 px, thanh trạng thái 26/28 px, palette 272 px. Ở ≥ 1600 px palette Thuộc tính dock mở sẵn; hẹp hơn thì tự ẩn thành dải 22 px và dòng lệnh còn 1 dòng lịch sử.
- **Ngân sách (DoD-L):** đổi trạng thái, mở menu ≤ 100 ms; hộp thoại, palette đến khung đầu ≤ 150 ms; cuộn, kéo 60 fps; ký tự gõ ≤ 16 ms; khởi động ấm ≤ 1,5 s; rảnh 0% CPU (không timer định kỳ, không polling). Animation ≤ 150 ms (`--t-fast` 80 ms, `--t` 120 ms), về 0 khi Giao diện nhẹ hoặc hệ điều hành bật giảm chuyển động.
- **Một nguồn:** phím tắt (UI0-114, chuyển từ bảng `KDEF` của prototype), hành động → chuỗi lệnh (UI0-015), định dạng số bản vẽ (RGN-013), cổng phiên bản (PLF-002 → UI0-011), tiến trình (NT-013 → UI0-112), ô nhập số (UI0-007).
- **Mã màn hình:** task nền ghi mã gần nhất: UI-A01 (toàn khung), UI-C09 (ô số), UI-A14 (thông điệp), UI-MAC (phần macOS). Task UI0 không phụ thuộc mã `UI-xxx`, để màn hình dùng chúng không tạo vòng phụ thuộc.
- **macOS** (alpha T26–27, GA cùng 1.0): cùng bố cục; khác ở menu hệ thống, nút cửa sổ bên trái, ⌘ cho phím hệ thống và ⌃ cho phím CAD, hộp thoại dạng sheet (07.1 §13.6). Phần riêng macOS có Mốc 1.0.
- **Kiểm thử:** mỗi task có bảng mẫu hoặc kịch bản chạy trong CI; ảnh chụp và độ trễ đầu-cuối do QA-011 thu, số đo trong ứng dụng do UI0-105 cung cấp.

## 1. Ứng dụng `trion` và luồng UI

### UI0-101 · Executor luồng UI: nhận kết quả nền theo lô, kiểm sở hữu luồng
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-005, NT-009
- **Màn hình:** UI-A01
- **Đầu vào:** API "đưa kết quả về luồng UI" của NT-005; DoD-U mục 4; DoD-H mục 6 (rảnh không polling).
- **Đầu ra:** `app/core/UiExecutor.{h,cpp}`: executor luồng UI của NT-005 trên vòng sự kiện Qt (một sự kiện đánh thức cho cả lô), gộp theo khóa "mới nhất thắng", bỏ kết quả khi receiver đã hủy; macro `TRION_ASSERT_UI_THREAD`; test `app/core/tests/ui_executor_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. 100.000 kết quả gửi về trong 1 s: xử lý theo lô, mỗi lô ≤ 4 ms trên luồng UI **[đề xuất]**, không lô nào làm trễ khung > 1 khung.
  2. 1.000 kết quả cùng khóa trong một khung: receiver nhận đúng 1 lần, giá trị cuối.
  3. Receiver hủy trước khi kết quả về: kết quả bị bỏ; 10.000 vòng ngẫu nhiên sạch ASan, TSan.
  4. Rảnh 10 s: 0 lần đánh thức luồng UI do executor (đếm), không timer.
  5. Gọi API ghi document ngoài luồng UI: assert ở bản debug, trả lỗi ở bản release (test cả hai).
  6. Đạt DoD-C, DoD-H, DoD-L.

### UI0-001 · Ứng dụng Qt `trion`: nối lõi, luồng UI sở hữu document, khởi động ấm
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** NT-001, NT-008, NT-010, DOC-018, INT-002, UI0-101
- **Màn hình:** UI-A01
- **Đầu vào:** 05 ADR-002; DoD-L (khởi động ấm ≤ 1,5 s, máy thấp ≤ 3 s); 07.1 §13.6 (tab tệp trong một cửa sổ); API DOC-018 (phiên nhiều bản vẽ), INT-002 (input pipeline trên luồng UI), NT-010, NT-008.
- **Đầu ra:**
  - `app/core/main.cpp`, `app/core/Application.{h,cpp}`: QApplication, chính sách High-DPI, vòng đời, thoát có hủy tác vụ nền.
  - `app/core/CoreSession.{h,cpp}`: sở hữu phiên DOC-018 và INT-002 trên luồng UI.
  - `app/core/SingleInstance.{h,cpp}`: lần chạy thứ hai chuyển đường dẫn cho phiên đang chạy.
  - `app/core/StartupTrace.{h,cpp}`: pha khởi động, `--startup-trace <tệp.json>`.
- **Tiêu chí nghiệm thu:**
  1. `trion a.dwg b.dxf` mở hai bản vẽ; `trion c.dwg` khi đã có phiên chạy: phiên cũ mở thêm bản vẽ ≤ 300 ms **[đề xuất]**, tiến trình mới thoát mã 0.
  2. Pha nền của khởi động ấm (tiến trình bắt đầu → sẵn sàng dựng cửa sổ chính: Qt, cài đặt, theme, font, biểu tượng hay dùng) ≤ 400 ms tầm trung, ≤ 800 ms máy thấp **[đề xuất]**; trace ghi từng pha. Tổng ≤ 1,5 s nghiệm thu ở UI0-115.
  3. TSan: 1.000 lần mở/đóng bản vẽ xen tác vụ nền, không race; truy cập document ngoài luồng UI bị chặn.
  4. Thoát khi còn tác vụ nền: hủy hợp tác, tiến trình kết thúc ≤ 500 ms **[đề xuất]**, LeakSanitizer sạch.
  5. Rảnh 30 s sau khởi động: CPU trung bình ≤ 0,5% **[đề xuất]**, 0 timer định kỳ.
  6. Đạt DoD-C, DoD-H, DoD-L, DoD-U.

## 2. Theme, font, biểu tượng

### UI0-102 · Bộ sinh token từ `tokens.css` và kiểm tương phản lúc build
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-001
- **Màn hình:** UI-A01
- **Đầu vào:** `design/prototype/css/tokens.css` (Tối, Sáng, `data-density="comfortable"`, `data-lite`, `prefers-reduced-motion`); design/README mục Quy ước (chữ ≥ 4,5:1, viền điều khiển ≥ 3:1 ở cả hai chủ đề).
- **Đầu ra:** `tools/tokengen/` chạy lúc build, sinh `app/theme/generated/Tokens.h` (hằng theo tên token, `--c-accent` → `tok::c_accent`) và `tokens.json`; danh sách cặp tương phản `app/theme/contrast-pairs.txt`; test `tools/tokengen/tests/`.
- **Tiêu chí nghiệm thu:**
  1. 100% thuộc tính `--*` của từng biến thể (Tối, Sáng, thoáng, nhẹ, giảm chuyển động) có trong đầu ra, tên giữ nguyên; test báo thiếu, thừa.
  2. Hex, `rgba()`, px, ms, số trần và `var()` lồng chuyển đúng (màu đúng từng kênh 8 bit); cú pháp lạ làm build lỗi kèm số dòng.
  3. Mọi cặp chữ/nền trong danh sách ≥ 4,5:1, viền điều khiển ≥ 3:1, ở Tối và Sáng (WCAG 2.x; màu có alpha trộn lên nền trước khi tính); cặp không đạt làm build lỗi và in tỉ số. Chữ vô hiệu được miễn.
  4. Sinh tất định (cùng đầu vào cùng byte); sửa tokens.css chỉ build lại phần phụ thuộc.
  5. Đạt DoD-C.

### UI0-002 · Theme sinh từ token: Tối/Sáng, mật độ, Giao diện nhẹ, màu ngữ nghĩa
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UI0-102, NT-010
- **Màn hình:** UI-A01
- **Đầu vào:** 07.1 §13.2 (Tối mặc định, Sáng; nền vùng vẽ đặt riêng: tối cho Model, trắng cho Layout; bảng token ngữ nghĩa); styleguide.html mục Màu; mã sinh của UI0-102.
- **Đầu ra:** `app/theme/ThemeManager.{h,cpp}` (Tối, Sáng, Theo hệ thống; gọn/thoáng; Giao diện nhẹ; tín hiệu đổi), QPalette và QSS sinh từ `app/theme/qss/app.qss.in`, `app/theme/SemanticColors.{h,cpp}` (tên 07.1 §13.2 → token; ghi đè của người dùng lưu NT-010, UI-D10 dùng), lint `ci/lint/no-literal-style`.
- **Tiêu chí nghiệm thu:**
  1. Đổi chủ đề, mật độ hoặc Giao diện nhẹ khi đang chạy: ≤ 100 ms đến khung đầu đã đổi với 2.000 widget **[đề xuất]**, không khởi động lại, không widget nào giữ màu cũ (quét cây widget).
  2. Đủ mọi token ở 07.1 §13.2, kể cả `edition.badge.*` của 6 phiên bản, ở cả hai chủ đề; token vùng vẽ không đổi khi đổi chủ đề giao diện.
  3. Ghi đè màu ngữ nghĩa sống qua khởi động lại; "Đặt lại" trả đúng giá trị tokens.css.
  4. Lint: `#RRGGBB`, `rgb(`, `QColor` hằng, cỡ chữ và khoảng cách viết thẳng trong `app/` (trừ `app/theme/generated/`) làm PR lỗi.
  5. Giao diện nhẹ: bóng thành viền 1 px `--c-border-ctl`, chuyển động 0 ms; không chế độ nào dùng làm mờ.
  6. Đạt DoD-C, DoD-U, DoD-L.

### UI0-003 · Font giao diện IBM Plex Sans/Mono đủ tiếng Việt
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** UI0-102
- **Màn hình:** UI-A01
- **Đầu vào:** styleguide.html mục Chữ (Sans cho giao diện; Mono cho tọa độ, giá trị số, tên lệnh, dòng lệnh; chuỗi thử dấu chồng tầng); token `--f-*`, `--fs-*`, `--fw-*`, `--lh`; giấy phép SIL OFL 1.1.
- **Đầu ra:** `app/theme/fonts/` (Plex Sans 400/500/600, Plex Mono 400/500, `OFL.txt`) nhúng resource; `app/theme/FontRegistry.{h,cpp}` (nạp, vai trò chữ theo thang token, font dự phòng); test.
- **Tiêu chí nghiệm thu:**
  1. Đủ 134 chữ cái tiếng Việt có dấu (hoa, thường, Đ/đ) dựng bằng Plex Sans và Plex Mono, 0 glyph rơi về font dự phòng (kiểm từng ký tự).
  2. Dấu chồng tầng (Ấ, Ẫ, Ự…) nằm trọn trong dòng 22 px ở 100/150/200% (đo hộp bao glyph); ảnh chụp khớp baseline.
  3. Mọi vai trò chữ lấy cỡ, độ đậm từ token: giao diện 12 px, nhãn phụ 11,5 px.
  4. Nạp font chiếm ≤ 30 ms luồng UI khi khởi động **[đề xuất]**; `OFL.txt` có trong gói cài và hộp Giới thiệu.
  5. Đạt DoD-C, DoD-U.

### UI0-103 · Chuyển biểu tượng prototype sang SVG, lint quy cách, báo cáo phủ
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UI0-102, INT-001
- **Màn hình:** UI-A01
- **Đầu vào:** `design/prototype/js/icons.js` (323 biểu tượng lưới 16 px; lớp `.f` tô nhạt, `.dot` tô đặc; nét `--sw` 1,25, đầu vuông, nối miter); 07.1 §13.5; metadata biểu tượng và vị trí ribbon của lệnh (INT-001).
- **Đầu ra:** `tools/iconconv/` (icons.js → `app/theme/icons/src/<tên>.svg`, chạy lại được khi prototype đổi), `tools/iconlint/`, `app/theme/icons/manifest.json` (tên, nhóm, cờ bộ Viewer), báo cáo phủ trong CI.
- **Tiêu chí nghiệm thu:**
  1. 323/323 biểu tượng chuyển xong; SVG chỉ có `viewBox="0 0 16 16"`, path/circle/rect, không màu viết thẳng, nét 1,25 đầu vuông; lint báo lỗi kèm tên tệp.
  2. Raster 16 px của SVG khớp ảnh tham chiếu chụp một lần từ styleguide.html, sai khác ≤ 2% pixel **[đề xuất]**.
  3. Báo cáo phủ: lệnh có vị trí ribbon/menu/toolbar trong INT-001 mà thiếu biểu tượng 16 hoặc 32 px → CI lỗi nếu lệnh thuộc mốc hiện hành, cảnh báo nếu thuộc mốc sau.
  4. Bộ Viewer (~80 biểu tượng: ribbon Viewer, khung, thanh trạng thái) đánh dấu trong manifest và đủ 100%.
  5. Đạt DoD-C.

### UI0-004 · Bộ biểu tượng lúc chạy: biến thể chủ đề, vô hiệu, cache raster theo DPI
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UI0-103, UI0-002, NT-005
- **Màn hình:** UI-A01
- **Đầu vào:** 07.1 §13.5 (16 và 32 px, biến thể Tối/Sáng, trạng thái vô hiệu); SVG nguồn và manifest (UI0-103); màu token (UI0-002).
- **Đầu ra:** `app/theme/IconProvider.{h,cpp}` (`QIconEngine` riêng: tô màu theo chủ đề và trạng thái thường, rê, bật, vô hiệu, khóa), cache raster LRU theo (tên, cỡ, DPR, chủ đề, trạng thái), raster trước bộ hay dùng ở nền qua NT-005; test, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Raster đúng cỡ pixel thiết bị (16 px → 16/20/24/28/32 px ở 100–200%), không phóng bitmap; ảnh chụp khớp baseline ở 3 DPI × 2 chủ đề × 5 trạng thái.
  2. Đổi chủ đề: 550 biểu tượng đang hiện tô lại ≤ 30 ms luồng UI **[đề xuất]**, phần còn lại raster ở nền.
  3. Lần vẽ thứ hai không raster lại (đếm); cache ≤ 16 MB **[đề xuất]**, loại LRU.
  4. Vô hiệu và khóa phân biệt không chỉ bằng màu (khóa có huy hiệu); biểu tượng thường tương phản ≥ 3:1 với nền.
  5. Đạt DoD-C, DoD-U, DoD-L.

### UI0-116 · Bộ biểu tượng cho lệnh đến mốc EA
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UI0-004, UIA-006
- **Màn hình:** UI-A01
- **Đầu vào:** 07.1 §13.5 (tự thiết kế, cùng quy cách); báo cáo phủ UI0-103; nội dung ribbon, menu, toolbar Cổ điển (UIA-006) của các lệnh mốc VP và EA.
- **Đầu ra:** SVG bổ sung trong `app/theme/icons/src/`, manifest cập nhật, baseline ảnh bảng biểu tượng.
- **Tiêu chí nghiệm thu:**
  1. Báo cáo phủ UI0-103 đạt 100% cho mọi lệnh mốc VP và EA có vị trí ribbon, menu, toolbar (16 và 32 px).
  2. Mọi SVG mới qua lint UI0-103; ảnh bảng biểu tượng (Tối/Sáng × 100/200%) lưu baseline.
  3. Không trùng: hai lệnh khác nhau không có raster 16 px giống nhau > 95% pixel **[đề xuất]** (kiểm tự động), trừ cặp khai báo dùng chung.
  4. Đạt DoD-U.

### UI0-117 · Hoàn thiện bộ biểu tượng đến v1.0
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UI0-116
- **Màn hình:** UI-A01
- **Đầu vào:** 07.1 §13.5 (khoảng 450–550 biểu tượng đến v1.0, kể cả marker bắt điểm, grip, huy hiệu con trỏ, biểu tượng trạng thái); báo cáo phủ UI0-103.
- **Đầu ra:** SVG bổ sung trong `app/theme/icons/src/`, manifest cập nhật, baseline ảnh bảng biểu tượng.
- **Tiêu chí nghiệm thu:**
  1. Báo cáo phủ UI0-103 đạt 100% cho mọi lệnh mốc 1.0 có vị trí ribbon, menu, toolbar; tổng số biểu tượng trong khoảng 450–550.
  2. Mọi SVG mới qua lint và kiểm trùng của UI0-116; baseline ảnh bảng biểu tượng cập nhật.
  3. Đạt DoD-U.

## 3. Bản địa hóa

### UI0-104 · Hạ tầng chuỗi dịch và đổi ngôn ngữ khi đang chạy
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-001, NT-010
- **Màn hình:** UI-A01
- **Đầu vào:** 07.1 §13.4 (tiếng Việt mặc định, English; chuyển không cần cài lại; mọi chuỗi trong tệp dịch); DoD-U mục 6.
- **Đầu ra:** `app/core/i18n/Translator.{h,cpp}` (nạp `.qm`, đổi ngôn ngữ lúc chạy, phát `LanguageChange`), quy ước `retranslate()` cho widget, `app/i18n/trion_vi.ts`, `trion_en.ts`, bước CMake chạy lupdate/lrelease, kiểm `ci/lint/i18n`.
- **Tiêu chí nghiệm thu:**
  1. Đổi vi ↔ en khi đang chạy: 2.000 widget đang hiện đổi chuỗi ≤ 300 ms **[đề xuất]**, không khởi động lại, giữ trạng thái (chọn, cuộn, nội dung ô nhập).
  2. Chuỗi hiện cho người dùng viết thẳng không qua `tr()` trong `app/` (setText, setToolTip, setWindowTitle, QAction…) làm PR lỗi; thiếu bản dịch vi hoặc en làm bản phát hành lỗi.
  3. Chuỗi có số dùng placeholder và dạng số nhiều của Qt; lint cấm ghép chuỗi hiển thị bằng `+`.
  4. Lần chạy đầu dùng tiếng Việt; lựa chọn lưu qua NT-010.
  5. Đạt DoD-C, DoD-U.

### UI0-005 · Bản địa hóa vi/en: định dạng, bảng thuật ngữ, kiểm chuỗi dài +40%
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UI0-104, NT-012
- **Màn hình:** UI-A01
- **Đầu vào:** 07.1 §13.4 (bảng thuật ngữ, quy tắc dịch, định dạng số và ngày); 07.3 Phụ lục C (chuỗi dài hơn 40% không bị cắt); API UI0-104, NT-012.
- **Đầu ra:**
  - `app/core/i18n/UiFormat.{h,cpp}`: số đếm, dung lượng, ngày, thời lượng theo ngôn ngữ giao diện (số trong vùng vẽ không qua đây mà qua RGN-013).
  - `app/i18n/glossary.tsv` và kiểm thuật ngữ trong CI.
  - Ngôn ngữ giả `vi-XL` (dài +40%, dấu chồng tầng) và `app/core/i18n/TruncationProbe.{h,cpp}` quét cây widget tìm chữ bị cắt.
- **Tiêu chí nghiệm thu:**
  1. `UiFormat` vi: `1.250`, `1.250.000`, `12,5 MB`, `05/03/2026`; en: `1,250`, `12.5 MB`; ≥ 50 ca vàng.
  2. Chuỗi vi chứa thuật ngữ 07.1 §13.4 phải dùng đúng nhãn ("Layer", không "Lớp"; "Độ dày nét" cho Lineweight); vi phạm làm CI lỗi, ngoại lệ có lý do.
  3. `TruncationProbe` với `vi-XL` trên bộ widget thử (nhãn, nút, tab, cột bảng, mục menu) phát hiện 100% ca cắt cài sẵn, 0 báo nhầm; QA-011 gọi được probe trên mọi màn hình.
  4. Đổi ngôn ngữ không đổi số trong vùng vẽ và dòng lệnh (luôn dấu chấm thập phân theo `UNITS`).
  5. Đạt DoD-C, DoD-U.

## 4. Hạ tầng hiệu năng và thành phần cơ bản

### UI0-105 · Đo độ trễ và thời gian khung trong ứng dụng, watchdog luồng UI
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-004, NT-009
- **Màn hình:** UI-A01
- **Đầu vào:** README §3 DoD-L (bảng ngưỡng UI); 07.1 §1 mục 3; API NT-004 (Tracy), NT-009.
- **Đầu ra:** `app/core/perf/UiProbe.{h,cpp}`: input → khung hình hiển thị, mở menu, khung đầu của hộp thoại/palette, fps khi cuộn/kéo; watchdog báo luồng UI khựng kèm tên handler; xuất JSON p50/p99 cho QA-011 và vùng Tracy; test.
- **Tiêu chí nghiệm thu:**
  1. Sai số đo ≤ 1 ms so với dấu thời gian Tracy trên ca có độ trễ cài sẵn 5/20/100 ms.
  2. Tắt (mặc định ở bản phát hành): chi phí ≤ 1% thời gian khung **[đề xuất]**, không cấp phát; bật bằng `--ui-probe`.
  3. Watchdog bắt 100% lần khựng cài sẵn ≥ 50 ms **[đề xuất]**; 0 báo nhầm khi rảnh 60 s.
  4. JSON có p50/p99 cho từng loại thao tác UI của bảng DoD-L; QA-011 đọc được để chặn merge.
  5. Đạt DoD-C, DoD-L.

### UI0-014 · Hạ tầng hiệu năng UI: model nền, áp diff theo lô, ảo hóa
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** UI0-101, UI0-105, DOC-009
- **Màn hình:** UI-A01
- **Đầu vào:** DoD-U mục 4 (việc > 2 ms chạy nền, áp theo lô; danh sách > 200 dòng ảo hóa); API UI0-101, UI0-105, DOC-009 (snapshot).
- **Đầu ra:** `app/widgets/model/BackgroundModel.{h,cpp}` (dựng dữ liệu từ snapshot ở nền, diff chèn/xóa/di chuyển/đổi, áp theo lô có hạn mức thời gian mỗi khung, giữ chọn và vị trí cuộn, hủy yêu cầu cũ); lint cấm `QListWidget`/`QTableWidget`/`QTreeWidget` cho dữ liệu bản vẽ; benchmark `app/widgets/bench/`.
- **Tiêu chí nghiệm thu:**
  1. Danh sách 100.000 dòng: khung đầu ≤ 150 ms, phần đồng bộ trên luồng UI ≤ 16 ms.
  2. 10.000 thay đổi đến cùng lúc: áp theo lô ≤ 4 ms mỗi khung **[đề xuất]**, không khựng > 1 khung; chọn và vị trí cuộn giữ với dòng còn tồn tại.
  3. Cuộn 100.000 dòng 60 fps tầm trung, 30 fps máy thấp (UI0-105 đo).
  4. Yêu cầu mới hủy yêu cầu nền cũ ≤ 50 ms; kết quả cũ không bao giờ đè kết quả mới (10.000 vòng ngẫu nhiên, TSan sạch).
  5. Bản vẽ không đổi: 0 cập nhật model.
  6. Đạt DoD-C, DoD-H, DoD-L, DoD-U.

### UI0-106 · Nút và điều khiển nhập: button, split button, toggle, dropdown, combo, ô tìm, tab, slider
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** UI0-002, UI0-004, UI0-104, NT-012
- **Màn hình:** UI-A01
- **Đầu vào:** styleguide.html mục Thành phần; prototype `css/app.css`; token `--h-btn`, `--h-ctl`, `--r`, `--c-*`; 07.3 Phụ lục C (đủ trạng thái).
- **Đầu ra:** `app/widgets/controls/`: `Button` (thường, chính, nguy hiểm, chỉ biểu tượng), `SplitButton`, `ToggleButton`, `SegmentedGroup`, `DropDown`, `ComboBox` có ô lọc, `SearchField`, `TabBar` (gạch chân 2 px màu nhấn, tràn thì cuộn), `Slider`; QSS theo token; test.
- **Tiêu chí nghiệm thu:**
  1. Mỗi điều khiển có trạng thái mặc định, rê, nhấn, focus (viền 2 px `--c-focus`), vô hiệu, bật, khóa theo phiên bản, lỗi; ảnh chụp khớp baseline Tối/Sáng × 100/150/200%.
  2. Combo có lọc với 10.000 mục: mỗi phím cập nhật danh sách ≤ 16 ms; lọc không phân biệt dấu, hoa/thường ("lop" khớp "Lớp") qua NT-012.
  3. Bàn phím: Space/Enter kích hoạt, mũi tên trong nhóm, Alt+↓ mở dropdown, Esc đóng không đổi giá trị.
  4. Chiều cao theo token ở gọn/thoáng; nhãn `vi-XL` (+40%) không bị cắt trong bố cục mẫu.
  5. Đạt DoD-C, DoD-U, DoD-L.

### UI0-107 · Menu và tooltip 2 mức
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UI0-106, UI0-114
- **Màn hình:** UI-A01
- **Đầu vào:** 07.1 §7.1 (tooltip 2 mức); prototype `TC.menu`, `showTip` trong `js/ui.js` (mức 1: tên, huy hiệu phiên bản, tên lệnh, phím; mức 2 sau 1,5 s: mô tả, "Nhấn F1 để xem trợ giúp"); nhãn phím UI0-114.
- **Đầu ra:** `app/widgets/menu/Menu.{h,cpp}` (QMenu theo token, menu con, phím, huy hiệu, nhóm), `app/widgets/tooltip/RichTooltip.{h,cpp}` (2 mức; F1 khi đang rê phát yêu cầu trợ giúp theo tên lệnh); test.
- **Tiêu chí nghiệm thu:**
  1. Mở menu (kể cả menu con 30 mục) đến khung đầu ≤ 100 ms p99, mục tiêu trong 1 khung.
  2. Mức 2 hiện sau 1,5 s rê liên tục; rời phần tử thì ẩn ngay; tooltip không che con trỏ; phím hiện theo nền tảng.
  3. Bàn phím: mũi tên, Enter, Esc đóng từng cấp, mnemonic theo ngôn ngữ; trình đọc màn hình đọc tên mục, phím, trạng thái khóa.
  4. F1 khi rê nút có lệnh phát yêu cầu trợ giúp kèm tên lệnh tiếng Anh (kiểm bằng tín hiệu).
  5. Đạt DoD-C, DoD-U, DoD-L.

### UI0-108 · Tree và table ảo hóa
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UI0-014, UI0-106, NT-010
- **Màn hình:** UI-A01
- **Đầu vào:** DoD-U mục 4; 07.2 quy ước áp dụng (danh sách dài luôn có tìm kiếm, sắp xếp được); token `--h-row` 22/26 px; API UI0-014.
- **Đầu ra:** `app/widgets/views/VirtualTreeView.{h,cpp}`, `VirtualTableView.{h,cpp}` (dòng cố định theo token, sắp xếp ở nền, cột ẩn/hiện/kéo rộng lưu NT-010, gõ để nhảy, chọn nhiều, sửa tại chỗ qua delegate, header dính); test, benchmark.
- **Tiêu chí nghiệm thu:**
  1. 1.000.000 dòng: khung đầu ≤ 150 ms, cuộn 60 fps tầm trung; bộ nhớ của view không tăng theo số dòng.
  2. Sắp xếp 100.000 dòng chạy nền, giữ chọn; luồng UI không bận > 16 ms liên tục.
  3. Bàn phím: mũi tên, PgUp/PgDn, Home/End, Shift/Ctrl chọn nhiều, F2 sửa, gõ chữ nhảy tới dòng (không phân biệt dấu).
  4. Bố cục cột lưu theo khóa view; đổi mật độ 22 ↔ 26 px giữ dòng đầu đang hiện.
  5. Đạt DoD-C, DoD-U, DoD-L.

### UI0-006 · Thành phần cơ bản: bảng mẫu tích hợp và nghiệm thu
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UI0-003, UI0-106, UI0-107, UI0-108
- **Màn hình:** UI-A01
- **Đầu vào:** styleguide.html (bản HTML của bảng mẫu); 07.3 Phụ lục C; API UI0-106, UI0-107, UI0-108, UI0-003.
- **Đầu ra:** `app/widgets/gallery/` (bảng mẫu mọi thành phần ở mọi trạng thái, mở bằng `trion --gallery`, QA-011 chụp); `docs/ui/thanh-phan.md` (API, khi nào dùng thành phần nào, khác biệt so với styleguide).
- **Tiêu chí nghiệm thu:**
  1. Bảng mẫu đủ: nút, split button, toggle, nhóm phân đoạn, dropdown, combo, ô tìm kiếm, tab, tree/table ảo hóa, slider, menu, tooltip 2 mức; mỗi thành phần ở 8 trạng thái của 07.3 Phụ lục C.
  2. Ảnh chụp khớp baseline: Tối/Sáng × 100/150/200% × gọn/thoáng; mọi sai khác với styleguide.html có ghi lý do trong tài liệu.
  3. Tab duyệt toàn bảng mẫu: đúng thứ tự, viền focus luôn nhìn thấy, 0 bẫy focus.
  4. Bảng mẫu mở và rảnh 60 s: 0 lần vẽ lại, CPU ~0%.
  5. Đạt DoD-C, DoD-U, DoD-L.

## 5. Ô nhập số, hộp thoại, thông điệp

### UI0-109 · Bộ phân tích giá trị của ô số: đơn vị, biểu thức, định dạng lại
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-004, RGN-013
- **Màn hình:** UI-C09
- **Đầu vào:** 07.2 UI-C09 (số thường; có đơn vị `12.5m`, `250cm`, `3'6"`; biểu thức `=1200/2+50` có ngoặc; dấu thập phân luôn "."); prototype `TC.parseNum` trong `js/ui.js`; API INT-004 (đơn vị, feet-inch, góc khảo sát), RGN-013 (LUNITS, LUPREC, AUNITS).
- **Đầu ra:** `app/widgets/numeric/ValueParser.{h,cpp}` (hàm thuần, không widget): loại độ dài, góc, tỉ lệ, số nguyên, không đơn vị; biểu thức số học (+ − × ÷, ngoặc, số có đơn vị) quy về đơn vị bản vẽ; lỗi có mã và câu giải thích; fuzz target.
- **Tiêu chí nghiệm thu:**
  1. ≥ 300 ca vàng **[đề xuất]**: mọi dạng của 07.2 UI-C09, kiến trúc, kỹ thuật, phân số, góc độ/grad/radian/khảo sát; giá trị khớp INT-004, chuỗi định dạng lại khớp RGN-013 từng ký tự.
  2. "12,5" → lỗi mã `comma` kèm gợi ý "12.5"; chia 0, tràn, ngoặc lệch → lỗi có câu giải thích, không ném ngoại lệ.
  3. Phân tích ≤ 20 µs mỗi giá trị **[đề xuất]**, không cấp phát.
  4. Fuzz ≥ 1 giờ sạch ASan/UBSan.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** biến, hàm, vector của `'CAL` (INT-022).

### UI0-007 · Ô nhập số chuẩn UI-C09
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UI0-106, UI0-109, UI0-110, INT-003
- **Màn hình:** UI-C09
- **Đầu vào:** 07.2 UI-C09 (hiển thị theo `UNITS`, nút ⌖ lấy từ bản vẽ, viền lỗi kèm câu, ↑/↓ theo bước, `*Khác nhau*`); prototype `TC.numField`, `numCommit`, `TC.pickValue` trong `js/ui.js`; API UI0-109, UI0-110 (ẩn/hiện hộp thoại), INT-003 (prompt lấy điểm, khoảng cách, góc).
- **Đầu ra:** `app/widgets/numeric/NumericField.{h,cpp}` (chế độ độ dài, góc, tỉ lệ, số nguyên; nút ⌖; nút bước; dòng lỗi dưới ô; tín hiệu `valueCommitted`); test.
- **Tiêu chí nghiệm thu:**
  1. Hiển thị theo `UNITS` (`1250.00`, `4'-1 1/2"`); đổi `UNITS` thì ô đang mở hiển thị lại trong ≤ 1 khung, giá trị không đổi.
  2. Enter hoặc rời ô: sai → viền lỗi và câu dưới ô (đọc được bởi trình đọc màn hình), không phát tín hiệu; đúng → định dạng lại, phát đúng 1 lần.
  3. ↑/↓ theo bước (Shift ×10 **[đề xuất]**); `*Khác nhau*` khi nhiều giá trị, gõ giá trị mới áp cho tất cả; Esc trả giá trị trước khi sửa.
  4. Nút ⌖: ẩn hộp thoại chứa ô ≤ 100 ms, lấy khoảng cách (2 điểm) hoặc góc qua INT-003, quay lại với giá trị; Esc quay lại không đổi.
  5. Ký tự gõ hiện ≤ 16 ms; tên đọc gồm nhãn, giá trị, đơn vị, lỗi.
  6. Đạt DoD-C, DoD-U, DoD-L.
- **Ngoài phạm vi:** nút máy tính nhanh trong ô (UI-I10, P2).

### UI0-110 · Khung hộp thoại cơ sở: modal/modeless, focus, Enter/Esc, khung đầu
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UI0-104, UI0-105, UI0-106, NT-010
- **Màn hình:** UI-A01
- **Đầu vào:** 07.2 quy ước áp dụng (modal áp dụng khi bấm nút xác nhận mang tên hành động, không "OK"); 07.1 §1 mục 2 (hạn chế modal); prototype `TC.dialog` trong `js/ui.js` (tiêu đề kèm tên lệnh, nút Trợ giúp F1, Đóng Esc, kéo, xếp chồng); DoD-L (khung đầu ≤ 150 ms).
- **Đầu ra:** `app/widgets/dialog/DialogHost.{h,cpp}`, `DialogFrame.{h,cpp}` (đầu: tiêu đề, tên lệnh tiếng Anh, F1, đóng; chân: nút xác nhận mang tên hành động); modal/modeless; nhớ vị trí, cỡ theo mã hộp thoại (NT-010); ẩn/hiện tạm cho thao tác lấy từ bản vẽ; dựng nội dung trễ có trạng thái đang tải; test.
- **Tiêu chí nghiệm thu:**
  1. Mở hộp thoại đến khung đầu có nội dung ≤ 150 ms tầm trung, ≤ 300 ms máy thấp (UI0-105 đo); dữ liệu chậm hiện đang tải, không chặn luồng UI.
  2. Enter = nút xác nhận (trừ ô nhiều dòng), Esc hủy, F1 mở trợ giúp theo tên lệnh; Tab vòng trong hộp thoại; đóng thì focus về chỗ cũ.
  3. Modeless không chặn vùng vẽ và dòng lệnh; mở lại cùng mã thì đưa bản đang mở lên, không tạo bản thứ hai.
  4. Vị trí, cỡ khôi phục theo mã; màn hình cũ đã rút thì đặt vào màn hình của cửa sổ chính.
  5. Kiểm CI: nút xác nhận của mọi hộp thoại đăng ký không mang nhãn "OK".
  6. Đạt DoD-C, DoD-U, DoD-L.

### UI0-008 · Khung hộp thoại: xác nhận phá hủy, "Không hỏi lại", hộp thoại lỗi
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UI0-005, UI0-110, NT-009, PLF-010
- **Màn hình:** UI-A14
- **Đầu vào:** 07.1 §12 (xác nhận: nút chính nêu hành động, "Không hỏi lại" khôi phục được trong Tùy chọn; lỗi: lời giải thích, chi tiết thu gọn, "Sao chép chi tiết", "Gửi báo lỗi"; văn phong); 07.1 §1 mục 4 ("Xóa 1.250 đối tượng?"); prototype `TC.confirm`; API UI0-110, UI0-005, NT-009, PLF-010.
- **Đầu ra:** `app/widgets/dialog/ConfirmDialog.{h,cpp}` (builder bắt buộc động từ và số lượng), `SuppressedPrompts.{h,cpp}` (lưu NT-010, model cho trang Tùy chọn), `ErrorDialog.{h,cpp}` (mã lỗi, chi tiết, trích log NT-009, sao chép, gửi qua PLF-010); test.
- **Tiêu chí nghiệm thu:**
  1. Không tạo được xác nhận phá hủy thiếu số lượng hoặc nút chính chung chung (assert + test); nhãn ví dụ "Xóa 3 layout", số dạng `1.250`.
  2. "Không hỏi lại": lần sau trả kết quả đã nhớ, không hiện; danh sách khôi phục từng mục hoặc tất cả, sống qua khởi động lại.
  3. Lỗi: chi tiết thu gọn mặc định; "Sao chép chi tiết" chép mã lỗi, phiên bản, hệ điều hành, 200 dòng log cuối **[đề xuất]**; "Gửi báo lỗi" mở luồng PLF-010, không gửi gì khi chưa đồng ý.
  4. Xác nhận cho hành động không hoàn tác được: focus mặc định ở Hủy **[đề xuất]**; Esc = Hủy; mở ≤ 150 ms.
  5. Đạt DoD-C, DoD-U, DoD-L.
- **Ngoài phạm vi:** sheet trên macOS (UI0-111).

### UI0-111 · macOS: hộp thoại dạng sheet và quy ước nút
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** UI0-008
- **Màn hình:** UI-MAC
- **Đầu vào:** 07.1 §13.6 (hộp thoại tệp, in của hệ điều hành dạng sheet gắn vào cửa sổ); prototype chế độ Nền tảng macOS; API UI0-110, UI0-008.
- **Đầu ra:** chế độ trình bày `Sheet` trong `DialogHost` trên macOS (gắn vào cửa sổ bản vẽ); hộp thoại tệp và in mở dạng sheet; thứ tự nút theo macOS; test chạy trên runner macOS.
- **Tiêu chí nghiệm thu:**
  1. Trên macOS, xác nhận, lỗi, tệp, in mở dạng sheet của cửa sổ đang dùng; cửa sổ khác vẫn thao tác được.
  2. Esc và ⌘. hủy, Enter = nút chính, nút chính ở mép phải; Windows và Linux không đổi (ảnh chụp không đổi).
  3. Sheet đến khung đầu ≤ 150 ms; ảnh chụp khớp prototype chế độ macOS ở Tối/Sáng.
  4. Đạt DoD-C, DoD-U.

### UI0-112 · Tiến trình gắn NT-013: con trỏ bận, thanh %, nút hủy
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UI0-101, UI0-106, NT-013
- **Màn hình:** UI-A14
- **Đầu vào:** 07.1 §12 (> 0,5 s con trỏ bận; > 2 s thanh % và nút hủy ở thanh trạng thái hoặc trong hộp thoại; "Đang mở · 63%"); design/README (đóng hộp thoại tác vụ vẫn chạy, xong thì toast); API NT-013.
- **Đầu ra:** `app/widgets/progress/ProgressPresenter.{h,cpp}` (theo dõi tác vụ NT-013, quy tắc 0,5 s/2 s, gộp nhiều tác vụ), `ProgressItemModel` cho thanh trạng thái (UIA-009 dùng), `InlineProgress` cho hộp thoại; test.
- **Tiêu chí nghiệm thu:**
  1. Tác vụ < 0,5 s: không hiện gì; 0,5–2 s: con trỏ bận, vùng vẽ không bị chặn; > 2 s: thanh % và nút hủy (lệch ≤ 1 khung).
  2. Bấm hủy: nút thành "Đang hủy…" trong khung kế; NT-013 báo đã hủy ≤ 50 ms với tác vụ hợp tác.
  3. Tác vụ báo 10.000 lần/s: UI cập nhật tối đa 1 lần mỗi khung **[đề xuất]**; rảnh 0 cập nhật.
  4. Nhiều tác vụ: hiện tác vụ chính và số tác vụ còn lại; trình đọc màn hình đọc % theo bước 10%.
  5. Đạt DoD-C, DoD-U, DoD-L.

### UI0-009 · Thông điệp: banner, toast, trung tâm thông báo, chú thích cạnh con trỏ, trạng thái rỗng
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** UI0-106, UI0-112
- **Màn hình:** UI-A14
- **Đầu vào:** 07.1 §12 (bảng loại thông báo, văn phong); prototype `TC.toast`, `TC.ctip`, `TC.notes` trong `js/ui.js`; API UI0-112.
- **Đầu ra:** `app/widgets/message/`: `CanvasBanner` (ở lại đến khi đóng, có hành động), `Toast` và `NotificationCenter` (model, chưa đọc, lưu theo phiên), `CursorTip` (tự ẩn 2 s), `EmptyState` (giải thích và hành động gợi ý), `FieldMessage` (dòng dưới ô nhập); test.
- **Tiêu chí nghiệm thu:**
  1. Toast tự ẩn sau 5 s (rê chuột thì dừng đếm), toast lỗi ở lại; tối đa 3 toast cùng lúc **[đề xuất]**; mọi toast có mục trong trung tâm thông báo.
  2. Chú thích cạnh con trỏ ẩn sau 2 s, lệch ≥ 16 px khỏi điểm đang chỉ; hiện/ẩn không làm vùng vẽ dựng lại cảnh (0 khung RND).
  3. Banner ở lại đến khi đóng, nút gọi hành động đăng ký; tối đa 2 banner cùng lúc, có đếm phần còn lại.
  4. Toast lỗi là alert, toast thường là status cho trình đọc màn hình; mỗi loại có biểu tượng, không chỉ màu.
  5. Hiện ≤ 1 khung sau sự kiện, không lấy focus của vùng vẽ và dòng lệnh.
  6. Đạt DoD-C, DoD-U, DoD-L.

## 6. Hành động, phím tắt, cổng phiên bản

### UI0-114 · Bảng phím tắt một nguồn và nhãn phím theo nền tảng
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-010, INT-001
- **Màn hình:** UI-A01
- **Đầu vào:** `KDEF`, `FIXED`, `RESERVED`, `MACSYS`, `TC.keyLabel` trong `design/prototype/js/screens14.js` (id, nhóm, tên, lệnh, phím mặc định, phạm vi Toàn cục/Vùng vẽ, phím thay thế macOS); 07.1 §13.6; README §4.2 (UI-K06 sửa nguồn này).
- **Đầu ra:** `app/core/keymap/keymap.json` (chuyển từ KDEF), `app/core/keymap/Keymap.{h,cpp}` (nạp, ghi đè của người dùng qua NT-010, phát hiện trùng, phím dành riêng của hệ điều hành và bộ gõ như Ctrl+Space, phạm vi), `keyLabel(nền tảng)`; test.
- **Tiêu chí nghiệm thu:**
  1. 100% dòng KDEF có trong keymap.json (so từng trường); mọi lệnh trong keymap có trong registry INT-001 (kiểm CI).
  2. Gán phím trùng hoặc phím dành riêng bị từ chối kèm tên chủ cũ; ghi đè sống qua khởi động lại, "Đặt lại" trả mặc định.
  3. `keyLabel` macOS: phím hệ thống (Ctrl+N/O/S/P/Z/X/C/V/A/Q, phím số) → ⌘, phím CAD giữ ⌃, bảng MACSYS (Ctrl+Y → ⇧⌘Z, Delete → ⌫…); khớp 100% ca vàng sinh từ prototype.
  4. Mỗi toggle F3–F12 có phím thay thế trên macOS theo KDEF.
  5. Tra phím → hành động ≤ 10 µs **[đề xuất]**; đổi keymap phát tín hiệu cho tooltip, menu, hộp Phím tắt.
  6. Đạt DoD-C.

### UI0-015 · Hành động UI → chuỗi lệnh qua INT-002, định tuyến phím tắt
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UI0-107, UI0-114, INT-001, INT-002, INT-005
- **Màn hình:** UI-A01
- **Đầu vào:** README §4.2 (phím tắt một nguồn); DoD-U mục 5; 07.1 §1 mục 1 (bàn phím trước; Enter/Space lặp lệnh); API INT-001 (metadata lệnh), INT-002, INT-005 (sysvar cho toggle), UI0-114.
- **Đầu ra:** `app/core/actions/ActionRegistry.{h,cpp}`: một `QAction` cho mỗi lệnh (nhãn vi, tên lệnh tiếng Anh, biểu tượng, tooltip, phím từ UI0-114); kích hoạt → chuỗi lệnh gửi INT-002 (hủy lệnh đang chạy, lệnh trong suốt `'` thì không); toggle gắn sysvar; định tuyến phím theo phạm vi Toàn cục/Vùng vẽ; test.
- **Tiêu chí nghiệm thu:**
  1. Mọi hành động ribbon, menu, toolbar, phím tắt tạo đúng chuỗi lệnh trên dòng lệnh và lịch sử; phát lại 100 hành động mẫu bằng `.scr` cho cùng bản vẽ.
  2. Toggle (OSNAP F3, ORTHO F8…) đồng bộ hai chiều với sysvar: đổi qua SETVAR hoặc LISP thì nút cập nhật ≤ 1 khung, không polling.
  3. Phím phạm vi Vùng vẽ không chạy khi focus ở ô văn bản; Ctrl+C trong ô là chép chữ, trong vùng vẽ là COPYCLIP.
  4. Từ phím hoặc bấm đến khi lệnh nhận input ≤ 16 ms **[đề xuất]**; trạng thái bật/tắt của hành động theo sự kiện INT-002, không timer.
  5. Đạt DoD-C, DoD-U, DoD-L.

### UI0-011 · Cổng phiên bản trong UI: huy hiệu khóa, trang nâng cấp, giao diện Viewer
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UI0-015, UI0-106, PLF-002
- **Màn hình:** UI-A01
- **Đầu vào:** 07.1 §2 (bảng khả năng; huy hiệu "Pro"/"Viewer Pro"; bấm mở trang nâng cấp UI-L04; Viewer dùng ribbon giản lược UI-M01; dùng thử hết hạn về Viewer, không khóa dữ liệu); styleguide.html (trang nâng cấp mở tối đa một lần cho mỗi tính năng mỗi phiên); API PLF-002.
- **Đầu ra:** `app/core/edition/EditionGate.{h,cpp}` (bọc PLF-002, tín hiệu khi giấy phép đổi), `LockBadge` cho nút, mục menu, tooltip, hàng palette; cổng gắn vào `ActionRegistry`; `UpgradeRouter` (UI-L04 đăng ký xử lý, chưa có thì mở trang web); `UiProfile` Viewer/đầy đủ để khung chọn ribbon và workspace; test.
- **Tiêu chí nghiệm thu:**
  1. Với 6 phiên bản (Viewer, Viewer Pro, Pro, Dùng thử, Giáo dục, Doanh nghiệp), trạng thái khóa của mọi hành động có cờ phiên bản khớp 100% bảng 07.1 §2 (bảng kiểm sinh từ PLF-002).
  2. Bấm mục khóa không chạy lệnh mà gửi yêu cầu nâng cấp kèm mã tính năng; tự mở trang tối đa 1 lần cho mỗi tính năng mỗi phiên, lần sau hiện chú thích có liên kết.
  3. Giấy phép đổi khi đang chạy (dùng thử hết hạn, kích hoạt Pro): UI đổi hồ sơ ≤ 500 ms **[đề xuất]**, không khởi động lại; bản vẽ đang mở giữ nguyên, chuyển chỉ xem khi về Viewer.
  4. Khóa không chỉ dùng màu: huy hiệu có chữ; trình đọc màn hình đọc "Có trong TrionCAD Pro".
  5. Đạt DoD-C, DoD-U.

## 7. DPI, palette, tiếp cận, khởi động

### UI0-012 · High-DPI, đổi DPI giữa màn hình, 1366×768, mật độ, Giao diện nhẹ
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UI0-002, UI0-004, NT-008
- **Màn hình:** UI-A01
- **Đầu vào:** 07.1 §13.1 (từ 1366×768; 100–200%; đổi DPI khi kéo giữa màn hình; biểu tượng 16/32 px; hai mật độ); 07.1 §1 mục 6, 9; styleguide.html (bảng kích thước gọn/thoáng, ngưỡng 1600 px); API NT-008 (DPI, tần số màn hình).
- **Đầu ra:** `app/core/display/DisplayContext.{h,cpp}` (DPI theo màn hình, hệ số lẻ không làm tròn, tín hiệu đổi DPI và đổi màn hình), `ScreenClass` (≥ 1600 px, < 1600 px, 1366×768) cho khung đọc; áp mật độ và Giao diện nhẹ toàn ứng dụng; test.
- **Tiêu chí nghiệm thu:**
  1. Kéo cửa sổ hoặc palette nổi giữa màn hình 100% và 150%/200%: bố cục, chữ, biểu tượng theo DPI mới ≤ 100 ms, biểu tượng raster lại đúng cỡ pixel (không mờ), vùng vẽ nhận tín hiệu DPI mới.
  2. Hệ số 125% và 175% không làm tròn; ảnh chụp bảng mẫu ở 100/125/150/175/200% khớp baseline.
  3. `ScreenClass` đổi đúng ở bề rộng 1366, 1599, 1600, 1920 px; khung đọc qua API, không tự đo.
  4. Gọn ↔ thoáng đổi chiều cao theo token (dòng 22 ↔ 26 px); Giao diện nhẹ: 0 animation, 0 bóng (viền 1 px), không trong suốt; tôn trọng giảm chuyển động của hệ điều hành.
  5. Đạt DoD-C, DoD-U, DoD-L.

### UI0-113 · Palette dock cơ sở: trái/phải/dưới, nổi, nhóm tab, lưu bố cục
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UI0-106, NT-010
- **Màn hình:** UI-A01
- **Đầu vào:** README §5.13 (UI0-010); prototype `index.html#UI-A01` (đầu palette: tiêu đề, menu, ghim, đóng; palette Thuộc tính, Layer); token `--pal-w`, `--strip`.
- **Đầu ra:** `app/widgets/dock/PaletteHost.{h,cpp}` (trên dock của QMainWindow: trái, phải, dưới, nổi, nhóm tab), `PaletteHeader.{h,cpp}`; lưu và khôi phục bố cục qua NT-010 có kiểm màn hình còn tồn tại; palette tạo trễ ở lần hiện đầu; test.
- **Tiêu chí nghiệm thu:**
  1. Dock trái/phải/dưới, nổi, gộp tab, tách tab bằng kéo; kéo palette 60 fps, không khựng > 1 khung.
  2. Bố cục khôi phục y hệt sau khởi động lại; palette nổi trên màn hình đã rút về màn hình chính, nằm trọn trong vùng làm việc.
  3. Không palette nào được dựng trước lần hiện đầu; khung đầu ≤ 150 ms.
  4. Phím đưa focus vào palette, Esc trả focus về vùng vẽ; mỗi palette có tên cho trình đọc màn hình.
  5. Đạt DoD-C, DoD-U, DoD-L.

### UI0-010 · Khung palette và docking: tự ẩn thành dải, quy tắc theo màn hình, đổi DPI
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UI0-012, UI0-113
- **Màn hình:** UI-A01
- **Đầu vào:** design/README Quy ước (palette Thuộc tính mở sẵn ở ≥ 1600 px; hẹp hơn tự ẩn thành dải 22 px; ở 1920×1080 vùng vẽ ≈ 60% khi mở, ≥ 75% khi tự ẩn hoặc Clean Screen); 07.1 §1 mục 6; API UI0-113, UI0-012.
- **Đầu ra:** `app/widgets/dock/AutoHideStrip.{h,cpp}` (dải 22 px ở cạnh, rê hoặc bấm thì trượt ra phủ lên vùng vẽ, ghim lại thành dock); bố cục mặc định theo `ScreenClass`; xử lý đổi DPI và màn hình cho palette dock, nổi, đang trượt ra; test.
- **Tiêu chí nghiệm thu:**
  1. ≥ 1600 px: Thuộc tính dock mở sẵn rộng 272 px; < 1600 px: tự ẩn thành dải 22 px. Ở 1920×1080 vùng vẽ 58–62% khi mở, ≥ 75% khi tự ẩn (đo diện tích).
  2. Rê dải 300 ms **[đề xuất]** hoặc bấm: trượt ra ≤ 120 ms (`--t`), không đẩy vùng vẽ; rời chuột hoặc Esc thì thu lại; Giao diện nhẹ hiện tức thì.
  3. Đổi DPI hoặc rút màn hình khi palette nổi hay đang trượt ra: đặt lại đúng, giữ nội dung và trạng thái.
  4. Palette trượt ra không lấy focus của dòng lệnh trừ khi bấm vào.
  5. Đạt DoD-C, DoD-U, DoD-L.

### UI0-013 · Khả năng tiếp cận: tên cho trình đọc màn hình, focus nhìn thấy, giảm chuyển động
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UI0-006, UI0-008
- **Màn hình:** UI-A01
- **Đầu vào:** 07.1 §13.3; DoD-U mục 7; styleguide.html (viền focus 2 px `--c-focus`); API UI0-006, UI0-008.
- **Đầu ra:** `app/core/a11y/AccessibilityAudit.{h,cpp}` (quét cây widget: tên, vai trò, trạng thái, thứ tự focus, bẫy focus; xuất cây QAccessible ra JSON), vẽ viền focus chung, đọc thiết lập giảm chuyển động của Windows và macOS và gắn vào token chuyển động, API thông báo cho trình đọc màn hình (toast, tiến trình); test.
- **Tiêu chí nghiệm thu:**
  1. Audit trên bảng mẫu và mọi hộp thoại đăng ký: 100% phần tử nhận focus có tên và vai trò, 0 bẫy focus; CI chặn PR thêm vi phạm.
  2. Viền focus 2 px `--c-focus` tương phản ≥ 3:1 với nền ở cả hai chủ đề, luôn hiện khi điều khiển bằng bàn phím.
  3. Bật giảm chuyển động của hệ điều hành khi đang chạy: mọi animation về 0 ms trong ≤ 1 khung.
  4. Cây QAccessible của bảng mẫu (tên, vai trò, trạng thái bật, khóa, lỗi) khớp baseline JSON ở vi và en.
  5. Đạt DoD-C, DoD-U.

### UI0-115 · Khởi động ấm ≤ 1,5 s toàn ứng dụng: nạp trễ, cache khởi động, cổng CI
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UI0-001, UI0-010, UIA-001, UIA-004, UIA-005, UIA-007, UIA-009, QA-011
- **Màn hình:** UI-A01
- **Đầu vào:** 05 §1.1 và DoD-L (khởi động ấm ≤ 1,5 s, máy thấp ≤ 3 s); trace của UI0-001; cửa sổ chính, Start, ribbon, dòng lệnh, thanh trạng thái (UIA-001, UIA-004, UIA-005, UIA-007, UIA-009).
- **Đầu ra:** `app/core/StartupPlan.{h,cpp}` (việc dời sau khung đầu: palette, tab ribbon chưa mở, hộp thoại, chỉ mục tìm lệnh), cache khởi động trên đĩa (bố cục ribbon đã tính, raster biểu tượng hay dùng; tự bỏ khi phiên bản, DPI, chủ đề đổi), benchmark `ci/perf/startup`.
- **Tiêu chí nghiệm thu:**
  1. Khởi động ấm đến khung đầu tương tác (Start, ribbon, dòng lệnh nhận phím): ≤ 1,5 s tầm trung, ≤ 3 s máy thấp, p99 của 20 lần; khởi động lạnh ghi lại để theo dõi.
  2. Gõ "LINE" 50 ms sau khung đầu: dòng lệnh nhận đủ ký tự (kịch bản QA-011).
  3. Cache hỏng hoặc khác phiên bản: bỏ qua và dựng lại, không lỗi; không cache vẫn ≤ 2 s tầm trung **[đề xuất]**.
  4. Việc dời chạy nền ưu tiên thấp, xong ≤ 2 s sau khung đầu **[đề xuất]**; sau đó CPU ~0%.
  5. Đạt DoD-C, DoD-L, DoD-U.

## 8. Cửa sổ chính, thanh tiêu đề, Quick Access Toolbar

### UIA-001 · Cửa sổ chính, workspace, hành vi theo kích thước màn hình, Clean Screen
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** UI0-001, UI0-010, UI0-011, UI0-012, UI0-015, NT-010
- **Lệnh:** CLEANSCREENON, CLEANSCREENOFF
- **Màn hình:** UI-A01
- **Đầu vào:** 07.1 §3 (bố cục, bảng vùng), §3.1, §3.2; `design/README.md` §Quy ước (tỉ lệ vùng vẽ); prototype `#UI-A01`, `js/shell.js` (`A.mount`, `A.setWs`, `renderDocks`, `A.cleanScreen`).
- **Đầu ra:**
  - `app/shell/MainWindow.{h,cpp}`: các vùng thanh tiêu đề, ribbon, tab tệp, vùng vẽ, dock palette, tab layout, dòng lệnh, thanh trạng thái; lưu và khôi phục hình học cửa sổ, dock qua NT-010.
  - `app/shell/Workspace.{h,cpp}`: workspace dựng sẵn Soạn thảo 2D và Viewer (mô tả JSON: vùng hiện, palette mở, vị trí dock); API `apply(id)` cho UIA-105, UIA-017.
  - `app/shell/CleanScreen.{h,cpp}`, Ctrl+0; `docs/specs/commands/CLEANSCREENON.md`, `CLEANSCREENOFF.md`.
- **Tiêu chí nghiệm thu:**
  1. 1920×1080: ribbon đầy đủ, palette Thuộc tính mở sẵn, vùng vẽ ≈ 60% diện tích (±3%). 1366×768: palette tự ẩn, dòng lệnh 1 dòng, vùng vẽ ≥ 70% **[đề xuất]**.
  2. Clean Screen ẩn ribbon, palette, toolbar; vùng vẽ ≥ 75% ở 1920×1080; tắt lại thì bố cục khôi phục đúng từng pixel. Chạy từ phím, `.scr`, `(command ...)` cho cùng kết quả.
  3. Đổi workspace hoặc Clean Screen ≤ 100 ms, mục tiêu 1 khung; 100 lần khởi động liên tiếp giữ đúng workspace, dock, kích thước cửa sổ.
  4. Kéo cửa sổ giữa màn hình 100% ↔ 200%: bố cục dựng lại trong 1 khung, không nhòe chữ (UI0-012). Đổi kích thước liên tục 10 s: ≥ 60 fps, không khung nào > 33 ms (QA-011).
  5. Đạt DoD-U, DoD-L.

### UIA-002 · Thanh tiêu đề, Quick Access Toolbar, ô Tìm lệnh, menu tài khoản
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** UIA-001, INT-001, INT-017, PLF-002, UI0-006
- **Màn hình:** UI-A01, UI-A04
- **Đầu vào:** 07.1 §3 (dòng Thanh tiêu đề), UI-A04; chỉ mục INT-017, metadata vị trí ribbon INT-001; prototype `#UI-A01`, `#UI-A04`, `js/shell.js` (`renderTitle`, `qatMenu`, `userMenu`, `helpMenu`, `edBadge`).
- **Đầu ra:**
  - `app/shell/TitleBar.{h,cpp}`: nút menu ứng dụng, QAT, tên tệp kèm `*`, "[Chỉ đọc]", "[Đã phục hồi]", huy hiệu phiên bản, tài khoản, trợ giúp.
  - `app/shell/CommandSearch.{h,cpp}`: tìm theo tên tiếng Anh, alias, từ khóa tiếng Việt; kết quả ghi vị trí ribbon ("Trang chủ › Vẽ"), chạy ngay qua INT-002.
  - `app/shell/Qat.{h,cpp}`: QAT bản V (Mở, In). Menu tài khoản: tên, email, phiên bản, Trạng thái giấy phép (lệnh mở UI-L03), Quản lý tài khoản (web), Đăng xuất.
- **Tiêu chí nghiệm thu:**
  1. 50 truy vấn mẫu (tiếng Việt có/không dấu, alias, tên tiếng Anh) **[đề xuất]**: lệnh đúng nằm trong top 3 ở ≥ 48 ca; "đường tròn" → `CIRCLE` đứng đầu.
  2. Kết quả cập nhật ≤ 16 ms p99 sau mỗi phím trên chỉ mục đầy đủ (QA-011).
  3. Lệnh bị khóa theo phiên bản hiện huy hiệu; Enter mở UI-L04 thay vì chạy (UI0-011).
  4. Dấu `*` hiện trong 1 khung sau thay đổi đầu tiên; tên dài cắt giữa, tooltip có đủ đường dẫn. Mọi nút dùng được bằng Tab, Enter, Esc.
  5. Đạt DoD-U, DoD-L.

### UIA-101 · QAT đầy đủ: danh sách hoàn tác, tùy biến, "Thêm vào QAT"
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** UIA-002, UIA-005, DOC-008, NT-010
- **Màn hình:** UI-A04
- **Đầu vào:** 07.1 UI-A04; `design/README.md` §Quy ước (nút tách hoàn tác 12 bước); API undo DOC-008; prototype `#UI-A04`, `js/shell.js` `qatMenu`.
- **Đầu ra:** mở rộng `app/shell/Qat.{h,cpp}`: Mới, Mở, Lưu, Lưu thành, Hoàn tác (split, 12 bước), Làm lại, In; menu tùy biến (Khớp thuộc tính, In hàng loạt, Xem trước bản in, Thuộc tính, Chuyển workspace, Bác sĩ Font, Hiện thanh menu); "Hiện bên dưới ribbon"; nhận "Thêm vào QAT" từ chuột phải ribbon; kiểm thử `app/shell/tests/qat_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Chọn bước thứ k trong danh sách phát một lệnh `UNDO k`; hash bản vẽ bằng k lần Ctrl+Z (100 kịch bản ngẫu nhiên).
  2. Danh sách mở ≤ 100 ms (mục tiêu 1 khung) khi undo stack có 10.000 bước.
  3. Thêm, bớt, đổi vị trí trên/dưới ribbon giữ sau khởi động lại; "Đặt lại" khôi phục đúng 7 mục mặc định.
  4. Đạt DoD-U, DoD-L.

## 9. Tab tệp, tab Start, menu ứng dụng

### UIA-010 · Tab tệp
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIA-001, DOC-018, RND-021, UI0-006
- **Màn hình:** UI-A09
- **Đầu vào:** 07.1 §5.6, §9.11 (thả DWG/DXF vào dải tab); phiên DOC-018, render ngoài màn hình RND-021; prototype `#UI-A09`, `js/shell.js` (`A.renderFileTabs`, `fitTabs`, `TC.thumb`).
- **Đầu ra:** `app/shell/FileTabBar.{h,cpp}`: chỗ cho tab Start, mỗi bản vẽ một tab (tên, chấm chưa lưu, biểu tượng chỉ đọc), thumbnail Model và layout khi rê (bấm để nhảy tới), menu chuột phải (Mới, Mở, Lưu, Lưu tất cả, Đóng, Đóng tất cả, Đóng các tab khác, Sao chép đường dẫn, Mở thư mục chứa), nút "+", mũi tên tràn, kéo sắp xếp, chuột giữa để đóng, nhận thả tệp; kiểm thử `app/shell/tests/file_tabs_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Thumbnail khi rê hiện ≤ 150 ms nếu có trong cache; nếu chưa, render nền ≤ 500 ms **[đề xuất]** với BENCH-STD 1 triệu entity, khung UI vẫn ≤ 16,6 ms trong lúc chờ.
  2. Mọi mục menu và thao tác đóng phát lệnh qua INT-002 (CLOSE, CLOSEALL, QSAVE…); tệp chưa lưu do lệnh CLOSE xử lý.
  3. 50 tab: mũi tên tràn liệt kê đủ; chuyển tab ≤ 100 ms, không tính regen.
  4. Thả 5 tệp DWG/DXF cùng lúc mở 5 tab theo thứ tự; tệp khác định dạng bị từ chối kèm chú thích.
  5. Đạt DoD-U, DoD-L.

### UIA-004 · Tab Start và dịch vụ tệp gần đây
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** UIA-010, NT-005, NT-011, NT-012, DWG-026, RND-021, PLF-006, UI0-014
- **Lệnh:** NEWTAB
- **Màn hình:** UI-A02
- **Đầu vào:** 07.1 UI-A02, §9.11; ảnh xem trước DWG-026, render RND-021; feed thông báo PLF-006; prototype `#UI-A02`, `js/shell.js` (`A.activate('start')`, `TC.thumb`, xử lý thả ở `bindCanvas`).
- **Đầu ra:**
  - `app/shell/RecentFiles.{h,cpp}`: tên, đường dẫn, ngày sửa, kích thước, phiên bản DWG; ghim, tìm không phân biệt dấu (NT-012), sắp xếp; dùng chung cho UIA-003.
  - `app/shell/ThumbnailQueue.{h,cpp}`: lấy ảnh nhúng (DWG-026), thiếu thì xin RND-021 render ở mức ưu tiên nền; chỉ giữ cache ảnh đã giải mã và hàng đợi.
  - `app/shell/StartPage.{h,cpp}`: Tạo mới (mẫu TCVN A0–A4, Trống mm/inch, Mẫu của tôi, Duyệt mẫu…; khóa ở Viewer), Mở, tệp gần đây dạng lưới/danh sách và menu chuột phải, Học tập, Thông báo, thẻ phiên bản, trạng thái rỗng, nhận thả tệp; `docs/specs/commands/NEWTAB.md`.
- **Tiêu chí nghiệm thu:**
  1. Tab Start có nội dung ≤ 150 ms sau khởi động ấm với 200 tệp gần đây; thumbnail điền dần, không khung UI nào > 16,6 ms.
  2. Thumbnail từ cache ≤ 5 ms/ảnh **[đề xuất]**; tệp không có ảnh nhúng render nền ≤ 1 s/tệp **[đề xuất]**, hủy ≤ 50 ms khi rời tab.
  3. Tìm trên 200 mục ≤ 16 ms/phím; "ket cau" khớp "Kết cấu".
  4. Tệp đã xóa hoặc đổi tên hiện "Không tìm thấy"; 100 tệp hỏng từ corpus QA-006 hiện ảnh giữ chỗ, không lỗi trang.
  5. Viewer không có Tạo mới; thẻ phiên bản đúng phiên bản và số ngày dùng thử (PLF-002).
  6. Đạt DoD-U, DoD-L.

### UIA-003 · Menu ứng dụng
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIA-002, UIA-004, UI0-011, UI0-015
- **Màn hình:** UI-A03
- **Đầu vào:** 07.1 UI-A03; `RecentFiles` (UIA-004), ô tìm của UIA-002; prototype `#UI-A03`, `SC.appMenu` (theo `js/catalog.js`).
- **Đầu ra:** `app/shell/AppMenu.{h,cpp}`: Mới, Mở, Lưu, Lưu thành (menu con DWG/DXF/DWT theo phiên bản), Xuất, In, Tiện ích bản vẽ, Đóng, Tùy chọn, Thoát; cột tệp gần đây (ghim; sắp theo ngày, tên, kích thước; xem trước khi rê); ô tìm cả lệnh lẫn tệp; bản rút gọn ở Viewer qua UI0-011; kiểm thử `app/shell/tests/app_menu_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Mở menu đến khung có nội dung ≤ 100 ms, mục tiêu 1 khung, với 50 tệp gần đây.
  2. Bảng đối chiếu phủ 100% mục: mỗi mục phát đúng chuỗi lệnh qua UI0-015.
  3. Ô tìm trả kết quả lệnh và tệp ≤ 16 ms/phím; Enter chạy kết quả đầu.
  4. Viewer chỉ hiện mục 07.1 §2 cho phép, mục Pro có huy hiệu khóa mở UI-L04; Esc đóng menu và trả focus về vùng vẽ.
  5. Đạt DoD-U, DoD-L.

## 10. Ribbon: khung

### UIA-102 · Mô hình ribbon JSON và các loại điều khiển
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** UIA-001, INT-001, UI0-004, UI0-006, UI0-007, UI0-011, UI0-015
- **Màn hình:** UI-A05
- **Đầu vào:** 07.1 §7.1 (loại điều khiển); metadata lệnh INT-001 (biểu tượng, tên, trợ giúp, cờ phiên bản); prototype `#UI-A05`, `js/shell.js` (`ribbonData`, `ctlHTML`, `indexCtls`, `paintGal`), `js/ribbons.js` (`MK`, `D_`, `styleDD`).
- **Đầu ra:**
  - `app/shell/ribbon/RibbonModel.{h,cpp}`, `app/shell/resources/ribbon/schema.json`: tab → panel → hàng → điều khiển; mỗi điều khiển trỏ tới lệnh INT-001 hoặc hành động UI0-015, có ưu tiên co và cờ phiên bản.
  - `app/shell/ribbon/controls/`: nút lớn, nút nhỏ, split button (nhớ lựa chọn gần nhất), dropdown, gallery (ảo hóa > 200 mục), toggle, ô số UI-C09, thanh trượt.
  - Kiểm thử lint `app/shell/tests/ribbon_lint_test.cpp` chạy trong CI.
- **Tiêu chí nghiệm thu:**
  1. Lint chặn merge nếu điều khiển trỏ tới lệnh không có trong registry INT-001, trỏ tới tên biểu tượng không có trong báo cáo phủ UI0-103, hoặc thiếu chuỗi vi/en. Ribbon chỉ tham chiếu tên biểu tượng, không vẽ biểu tượng.
  2. Nạp JSON của 8 tab ≤ 20 ms **[đề xuất]**; widget tạo lười theo tab.
  3. Split button chạy lựa chọn gần nhất, giữ sau khởi động lại; mũi tên mở danh sách bằng chuột và ↓.
  4. Gallery 2.000 mục cuộn 60 fps; toggle phản ánh sysvar đổi từ dòng lệnh hoặc LISP trong 1 khung.
  5. Đủ các trạng thái DoD-U mục 3 cho từng loại điều khiển (ảnh chụp QA-011).
  6. Đạt DoD-U, DoD-L.

### UIA-005 · Khung ribbon
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** UIA-102, UI0-012, UI0-107, NT-010
- **Lệnh:** RIBBON, RIBBONCLOSE
- **Màn hình:** UI-A05
- **Đầu vào:** 07.1 §7.1, §3 (3 mức thu gọn), §3.2 (co theo ưu tiên); prototype `js/shell.js` (`renderRibbon`, `fitRibbon`, `ctlRun`), `js/ribbons.js` `TC.panelMore`.
- **Đầu ra:** `app/shell/ribbon/RibbonBar.{h,cpp}`: dải tab; co panel theo ưu tiên (panel ít dùng thành nút thả xuống trước); thu gọn 3 mức (đầy đủ → tên panel → tên tab); panel mở rộng có ghim; nội dung tooltip 2 mức cho UI0-107 (mức 1: tên, tên lệnh tiếng Anh, phím tắt; mức 2 sau 1,5 s: mô tả, ảnh, "Nhấn F1 để xem trợ giúp"); chuột phải (hiện/ẩn tab và panel, thu gọn, Thêm vào QAT); `docs/specs/commands/RIBBON.md`, `RIBBONCLOSE.md`; benchmark `app/shell/bench/ribbon_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Đổi tab ≤ 16,6 ms p99 (1 khung) trên máy tầm trung, ≤ 100 ms trên máy thấp, kể cả lần đầu mở tab (QA-011).
  2. Tính lại bố cục khi đổi kích thước ≤ 2 ms **[đề xuất]**; kéo rộng/hẹp cửa sổ liên tục giữ 60 fps.
  3. Ở 1366 px mọi lệnh của tab Trang chủ tới được trong ≤ 2 lần bấm; chuỗi tiếng Việt dài hơn 40% không bị cắt.
  4. Tooltip mức 2 hiện sau 1,5 s ±100 ms; F1 mở trợ giúp đúng lệnh của điều khiển đang rê.
  5. Trạng thái thu gọn, panel ghim, tab ẩn giữ sau khởi động lại; ribbon điều hướng đủ bằng bàn phím.
  6. Đạt DoD-U, DoD-L.

### UIA-103 · Khung tab ngữ cảnh và xem trước gallery khi rê
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIA-005, INT-002, INT-008, INT-012
- **Màn hình:** UI-A05
- **Đầu vào:** 07.1 §7.11, §7.1 (gallery xem trước, P1); sự kiện vòng đời lệnh INT-002, sự kiện vùng chọn INT-008, jig INT-012; prototype `js/shell.js` `A.setCtxTab`.
- **Đầu ra:** `app/shell/ribbon/ContextualTabs.{h,cpp}`: registry `add(id, predicate(vùng chọn, lệnh đang chạy, không gian), json)`; tự hiện, tự kích hoạt, tự đóng và trả về tab trước; nhiều tab cùng lúc theo ưu tiên. `GalleryPreview.{h,cpp}`: rê lên mục gallery đặt giá trị tạm cho lệnh đang chạy hoặc vùng chọn qua jig, rời chuột thì trả lại.
- **Tiêu chí nghiệm thu:**
  1. Tab ngữ cảnh hiện trong 1 khung sau sự kiện vùng chọn hoặc lệnh; đóng khi lệnh xong hoặc bỏ chọn, focus về tab trước (9 tab giả lập × 4 kịch bản).
  2. Vùng chọn 100.000 đối tượng: đánh giá predicate ≤ 2 ms **[đề xuất]**, dùng thống kê loại có sẵn của INT-008.
  3. Xem trước gallery ≤ 8 ms/khung; rời chuột trả trạng thái cũ trong 1 khung; không ghi DB, không tạo mark undo (revision không đổi).
  4. Đạt DoD-U, DoD-L.
- **Ngoài phạm vi:** nội dung panel của từng tab ngữ cảnh (UI-F01, UI-E01, UI-E02, UI-F07, UI-G04, UI-G15, UI-I07, UI-H11, UI-G19 tự khai báo).

## 11. Ribbon: nội dung và workspace Cổ điển

### UIA-104 · Tab Trang chủ và dropdown sống (layer, thuộc tính, block)
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIA-005, DOC-009, INT-005, INT-008, RND-021
- **Màn hình:** UI-A05
- **Đầu vào:** 07.1 §7.2; snapshot DOC-009; prototype `#UI-A05` (bản Pro), `js/ribbons.js` (`curLayer`, `curLayerHTML`, `mkProp`, `styleDD`).
- **Đầu ra:** `app/shell/resources/ribbon/home.json` (panel Vẽ, Sửa, Chú thích, Layer, Block, Thuộc tính, Nhóm, Tiện ích, Clipboard); `app/shell/ribbon/live/`: dropdown layer (bật/tắt, đóng băng, khóa ngay trên dòng; ảo hóa), dropdown màu, linetype, lineweight của vùng chọn ("ByLayer", "*Khác nhau*"), gallery block trong bản vẽ có thumbnail. Widget dùng lại cho toolbar Cổ điển. Mọi thay đổi phát lệnh qua INT-002.
- **Tiêu chí nghiệm thu:**
  1. Vùng chọn đổi: dropdown cập nhật ≤ 16 ms phần đồng bộ với ≤ 10.000 đối tượng, phần còn lại ở nền.
  2. Dropdown 5.000 layer mở ≤ 150 ms, cuộn 60 fps, gõ lọc ≤ 16 ms/phím.
  3. Mỗi thay đổi là một lệnh, một mark undo; phát lại bản ghi bằng `.scr` cho cùng hash bản vẽ.
  4. Tắt layer từ dropdown: vùng vẽ đổi ở khung kế tiếp sau regen gia tăng.
  5. Đạt DoD-U, DoD-L.

### UIA-105 · Thanh menu, toolbar và workspace Cổ điển
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIA-001, UIA-102, UIA-104, UI0-114
- **Lệnh:** TOOLBAR
- **Màn hình:** UI-A05, UI-A01
- **Đầu vào:** 07.1 §7.12, §3.1 (14 toolbar); prototype `js/shell.js` (`renderClassic`, `tbHTML`, `tbClick`, `tbMenu`), `js/ribbons.js` `TC.classicMenu`.
- **Đầu ra:** `app/shell/resources/ribbon/classic_menu.json`, `classic_toolbars.json` (dùng khai báo điều khiển của UIA-102); `app/shell/classic/MenuBar.{h,cpp}`, `Toolbars.{h,cpp}`: 14 menu (Tham số ẩn đến khi có P2), 14 toolbar bật/tắt, dock hoặc nổi; cột phải ghi tên lệnh tiếng Anh và phím tắt; workspace Cổ điển đăng ký vào `Workspace` (UIA-001); `docs/specs/commands/TOOLBAR.md`.
- **Tiêu chí nghiệm thu:**
  1. Kiểm tự động: tập lệnh của ribbon Pro và của thanh menu chênh nhau 0 lệnh, trừ danh sách ngoại lệ có ghi lý do.
  2. Đổi Soạn thảo 2D ↔ Cổ điển ≤ 100 ms (mục tiêu 1 khung); vị trí toolbar, palette giữ sau khởi động lại.
  3. Mở menu ≤ 1 khung; đổi phím tắt ở UI-K06 thì cột phím tắt đổi theo ngay (bảng KDEF một nguồn của UI0-114).
  4. Toolbar Layers, Properties, Styles dùng widget của UIA-104, cùng ngưỡng cập nhật.
  5. Đạt DoD-U, DoD-L.

### UIA-006 · Nội dung ribbon: các tab §7.3–§7.10, kiểm phủ lệnh
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** UIA-103, UIA-104, UIA-105
- **Màn hình:** UI-A05
- **Đầu vào:** 07.1 §7.3–§7.10 (bảng panel, cột Phát hành), §7.11; 03 Phụ lục A (lệnh P0); prototype `#UI-A05` (bản Pro), `js/ribbons.js`.
- **Đầu ra:** `app/shell/resources/ribbon/{insert,annotate,view,manage,output,vntools,express}.json` đúng panel 07.1; dropdown kiểu chữ, kiểu kích thước, kiểu multileader, kiểu bảng, ô chiều cao chữ dùng widget UIA-104; đăng ký mã và điều kiện hiện của 9 tab ngữ cảnh §7.11 qua UIA-103; kiểm thử `app/shell/tests/ribbon_coverage_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. 100% lệnh P0 của 03 Phụ lục A có vị trí ribbon theo 07.1 tới được từ ribbon và thanh menu Cổ điển (kiểm trên registry INT-001).
  2. Mỗi điều khiển theo cột Phát hành: chưa tới mốc thì ẩn, thuộc phiên bản cao hơn thì hiện huy hiệu khóa.
  3. Đổi giữa 8 tab ≤ 16,6 ms p99 mỗi lần; ảnh chụp 8 tab khớp prototype ở Tối/Sáng, 100/150/200%.
  4. Đạt DoD-U, DoD-L.
- **Ngoài phạm vi:** tab Tham số (UI-PAR, P2); hành vi riêng của tab Mở rộng (UI-MR, UI-MR2); nội dung panel của tab ngữ cảnh.

## 12. Dòng lệnh

### UIA-106 · Spike S6: bộ gõ tiếng Việt trong ô dòng lệnh
- **Mốc:** S6 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-001, NT-002, NT-012
- **Màn hình:** UI-A06
- **Đầu vào:** 05 §21 (S6); 07.1 §8 (Gõ tiếng Việt), §13.6 (bộ gõ theo hệ điều hành); Qt `QInputMethodEvent`, `inputMethodQuery`.
- **Đầu ra:**
  - `app/widgets/CommandInput.{h,cpp}`: preedit gạch chân tại chỗ, commit, chuẩn hóa NFC (NT-012).
  - `app/core/ime/ImeBypass_{win,mac,linux}.cpp`: khi chờ tên lệnh, bỏ qua bộ gõ. Windows: bỏ sự kiện do Unikey/EVKey chèn (VK_PACKET, phím có mã quét 0) và lấy ký tự theo phím vật lý. macOS, Linux: tắt input method của ô (`Qt::WA_InputMethodEnabled`); bộ gõ kiểu chèn phím (EVKey, OpenKey) lọc theo nguồn sự kiện. Cách lọc chốt sau spike **[đề xuất]**.
  - Harness `app/shell/tests/ime/` bơm phím (SendInput, CGEvent, XTest) trên runner tự host có cài bộ gõ; báo cáo `docs/spikes/S6-ime.md`.
- **Tiêu chí nghiệm thu:**
  1. Gõ đủ 134 ký tự có dấu (67 thường, 67 hoa) bằng Telex và VNI ra NFC đúng 100% với: Unikey, EVKey, bộ gõ Windows; bộ gõ macOS (Telex, VNI), EVKey, OpenKey; ibus và fcitx5 (Bamboo, Unikey).
  2. Khi chờ tên lệnh, "lm", "dd", "aa", "ow" ra đúng LM, DD, AA, OW với mọi bộ gõ trên; tắt tùy chọn thì gõ có dấu bình thường.
  3. 10.000 chuỗi phím ngẫu nhiên ở 30 phím/s: không mất, không lặp ký tự.
  4. Ký tự hiện trong ô ≤ 16 ms p99 sau phím (máy tầm trung).
  5. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** IME trong trình soạn MTEXT (task MTEXT của CMD dùng lại `CommandInput` và `ImeBypass`).

### UIA-107 · Ô nhập, prompt, chip tùy chọn, lịch sử dòng lệnh
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIA-001, UIA-106, INT-002, INT-003, UI0-006
- **Lệnh:** COMMANDLINE, COMMANDLINEHIDE
- **Màn hình:** UI-A06
- **Đầu vào:** 07.1 §8 (Thành phần 1–3, Hành vi); `design/README.md` §Quy ước (Prompt); dữ liệu prompt và chip INT-003; prototype `#UI-A06`, `js/cmd.js` (`CL.mount`, `renderPrompt`, `CL.options`, `CL.hist`, `browse`, `CL.submit`).
- **Đầu ra:** `app/shell/cmdline/CommandLine.{h,cpp}`: ô nhập (UIA-106); prompt có chip bấm được (nhãn tiếng Việt kèm chữ tắt gạch chân, "Bán kính · R"), giá trị mặc định `<10.0000>`, prompt lồng ">>" của lệnh trong suốt; lịch sử 0–10 dòng (mặc định 3) kèm thông báo của lệnh; Enter/Space, Esc (hai lần bỏ vùng chọn), ↑/↓; gợi ý một lần khi gõ ","; menu chuột phải (Lệnh gần đây, Giá trị đã nhập gần đây, Sao chép lịch sử, Tùy chọn); Ctrl+9; `docs/specs/commands/COMMANDLINE.md`, `COMMANDLINEHIDE.md`.
- **Tiêu chí nghiệm thu:**
  1. Ký tự hiện ≤ 16 ms p99; prompt mới hiện trong 1 khung sau khi lệnh hỏi (QA-011).
  2. Bấm chip và gõ chữ tắt cho cùng đầu vào INT-002 trên mọi prompt đã đăng ký; từ khóa ẩn (MO, RO, SC, MI) vẫn chạy.
  3. Bản ghi INT-002 của 1.000 phiên gõ trùng với khi chạy cùng chuỗi bằng `.scr`; Enter/Space khi rảnh lặp lệnh vừa dùng.
  4. Lịch sử 10.000 dòng cuộn 60 fps (ảo hóa); thêm dòng không làm khung > 16,6 ms.
  5. Đạt DoD-U, DoD-L.

### UIA-108 · Tự hoàn thành, gợi ý khi gõ sai, AutoCorrect
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIA-107, INT-017, UI0-004, UI0-014
- **Màn hình:** UI-A06
- **Đầu vào:** 07.1 §8 (Thành phần 4, Gõ sai tên lệnh); chỉ mục INT-017; prototype `js/cmd.js` (`matchCmds`, `updateAC`, `paintAC`, `lev`, thông điệp "Ý bạn là").
- **Đầu ra:** `app/shell/cmdline/Autocomplete.{h,cpp}`: lệnh, alias, sysvar (biểu tượng riêng), lệnh LISP/plugin, từ khóa tiếng Việt ("cắt" → TRIM, "bo góc" → FILLET); mỗi dòng có tên, alias, mô tả ngắn, biểu tượng; xếp theo tần suất của người dùng; tìm giữa chuỗi; dòng "Không có lệnh 'LNE'. Ý bạn là: LINE, LEN?" có gợi ý bấm được; AutoCorrect sau 3 lần chọn cùng gợi ý (P1), xem và xóa trong thiết lập.
- **Tiêu chí nghiệm thu:**
  1. Danh sách cập nhật ≤ 16 ms p99 sau mỗi phím (≤ 33 ms máy thấp) trên chỉ mục ≥ 3.000 mục (lệnh, alias, 250 sysvar, lệnh LISP).
  2. 100 lỗi gõ mẫu (sai, thiếu, thừa, đảo một ký tự) **[đề xuất]**: lệnh đúng có trong gợi ý ở ≥ 95 ca.
  3. Sau 20 lần dùng `LEN`, gõ "le" đưa `LEN` lên đầu; tần suất giữ sau khởi động lại.
  4. Tắt "tìm sysvar" hoặc "tìm theo tiếng Việt" có hiệu lực ngay.
  5. Đạt DoD-U, DoD-L.

### UIA-007 · Dòng lệnh: lịch sử F2, dạng nổi, thiết lập, cổng phiên bản
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIA-107, UIA-108, CMD:TEXTSCR, INT-020, UI0-110, NT-010, CMD:INPUTSEARCHOPTIONS
- **Màn hình:** UI-A06
- **Đầu vào:** 07.1 §8 (Dạng, Thiết lập dòng lệnh), §2 (thông báo cổng phiên bản); prototype `js/cmd.js` (`CL.msg`, `CL.status`, `CL.busy`).
- **Đầu ra:** `app/shell/cmdline/HistoryWindow.{h,cpp}` (F2/TEXTSCR: lịch sử đầy đủ, tìm, sao chép); dạng nổi bán trong suốt trên vùng vẽ (kéo, bám lại dock, chỉnh độ trong suốt); hộp INPUTSEARCHOPTIONS trên khung UI0-110 (số dòng lịch sử, cỡ chữ, độ trong suốt, độ trễ tự hoàn thành, tìm sysvar, tìm tiếng Việt); thông báo cổng phiên bản; kiểm thử tích hợp `app/shell/tests/cmdline_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. F2 mở lịch sử 100.000 dòng ≤ 150 ms; tìm ≤ 50 ms **[đề xuất]**; sao chép giữ nguyên văn bản.
  2. Dạng nổi không chặn chuột ngoài ô nhập; vùng vẽ phía dưới vẫn ≥ 60 fps khi pan.
  3. Viewer: 100% lệnh sửa của 03 Phụ lục A cho thông báo nâng cấp đúng mẫu 07.1 §2, bản vẽ không đổi.
  4. Thiết lập lưu qua NT-010, có hiệu lực không cần khởi động lại.
  5. Đạt DoD-U, DoD-L.

## 13. Vùng vẽ

### UIA-109 · Widget vùng vẽ: QRhi, con trỏ, định tuyến phím, khung chọn, tô sáng
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** UIA-001, UIA-007, UI0-105, UI0-114, RND-001, RND-002, RND-012, RND-013, RND-020, INT-008, INT-009
- **Màn hình:** UI-A07
- **Đầu vào:** 07.1 §9.1, §9.3, §9.4; overlay RND-012, cờ tô sáng RND-013, chữ thập RND-020; prototype `#UI-A07`, `js/shell.js` (`bindCanvas`, `bindKeys`), `js/cad.js` (`cad.move`, `cad.down`, `cad.up`, `cad.boxDone`, `cad.mode`).
- **Đầu ra:** `app/shell/canvas/CanvasWidget.{h,cpp}`: chứa cửa sổ QRhi của RND-001; 6 trạng thái con trỏ (§9.1), huy hiệu con trỏ (P1), cỡ chữ thập, pickbox, aperture theo sysvar; định tuyến phím (ký tự vào dòng lệnh hoặc ô DYN qua `KeySink`, phím tắt qua bảng UI0-114); khung Window (viền liền, màu A), Crossing (viền đứt, màu B), Lasso (P1, Space đổi W/C/F), Fence/WP/CP; tô sáng khi rê và trong khung đang kéo; tooltip rê lên đối tượng (P1).
- **Tiêu chí nghiệm thu:**
  1. Input → photon (di chuột đến khung có chữ thập mới) ≤ 25 ms p99 ở 60 Hz, ≤ 15 ms ở 144 Hz **[đề xuất]**, đo bằng UI0-105 và QA-011 qua dấu thời gian present.
  2. BENCH-STD 1 triệu entity: hit test ≤ 1 ms, tô sáng rê hiện ở khung kế tiếp; khung Crossing đang kéo cập nhật tô sáng ≤ 8 ms/khung.
  3. Rảnh: 0 khung vẽ, CPU ≈ 0% trong 60 s.
  4. Window và Crossing phân biệt được ở ảnh xám (kiểu viền); 10.000 phím gõ ở 30 phím/s khi focus ở vùng vẽ không mất ký tự nào.
  5. Đạt DoD-U, DoD-L.

### UIA-008 · Vùng vẽ: điều hướng, hiển thị dần khi mở tệp lớn
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIA-109, INT-016, DOC-016, NT-013, QA-011
- **Màn hình:** UI-A07, UI-B13
- **Đầu vào:** 07.1 §9.2, UI-B13; điều hướng INT-016, nạp dần DOC-016; prototype `js/canvas.js` (`View.prototype.zoomAt`, `panBy`, `zoomExtents`, `pushHist`).
- **Đầu ra:** `app/shell/canvas/Navigation.{h,cpp}`: chuyển sự kiện chuột và touchpad thành lời gọi INT-016 (lăn zoom quanh con trỏ, tốc độ, đảo chiều; giữ chuột giữa pan; nhấp đúp chuột giữa zoom toàn bộ; chụm và kéo hai ngón, P1, qua `QNativeGestureEvent`, `pixelDelta`); gộp zoom/pan liên tiếp thành một bước hoàn tác (tùy chọn). Hiển thị dần: tác vụ nạp đăng ký NT-013, lệnh sửa chờ nạp xong. Kịch bản QA-011 `app/shell/tests/canvas_nav_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Pan/zoom BENCH-STD 1 triệu entity ≤ 16,6 ms/khung p99 (tầm trung), ≤ 33 ms (máy thấp).
  2. Điểm dưới con trỏ lệch ≤ 0,5 px sau 100 nấc lăn vào/ra liên tiếp.
  3. Tệp BENCH-STD 100 MB: khung hình đầu ≤ 2 s **[đề xuất]** từ lúc bấm Mở trong khi nạp tiếp; pan/zoom trong lúc nạp ≥ 30 fps; hủy nạp phản hồi ≤ 50 ms.
  4. Gõ lệnh sửa khi đang nạp: thông báo chờ, lệnh chạy khi nạp xong, Esc hủy được.
  5. Đạt DoD-U, DoD-L.

## 14. Bắt điểm, grip, theo dõi, nhập động, xem trước

### UIA-013 · Marker bắt điểm, menu ghi đè, kiểu xem trước trong lệnh
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIA-008, INT-010, INT-012, RND-012
- **Màn hình:** UI-A12
- **Đầu vào:** 07.1 §9.6 (bảng 14 marker, Tab, Shift + chuột phải), §9.8; prototype `#UI-A12`, `js/cad.js` (`snapTip`, `cad.cycleSnap`, `snapOverride`).
- **Đầu ra:** `app/shell/canvas/SnapMarkers.{h,cpp}`: 14 marker tự vẽ dạng vector trên overlay, tooltip tên chế độ, Tab luân phiên; menu ghi đè Shift + chuột phải (điểm theo dõi tạm, From, M2P, bộ lọc `.X .Y .Z .XY .XZ .YZ`, các chế độ, "Thiết lập bắt điểm…"). `PreviewStyle.{h,cpp}`: kiểu rubber-band, đối tượng tạm, phần sẽ bị xóa (mờ hoặc đứt nét) dùng chung cho mọi lệnh qua INT-012.
- **Tiêu chí nghiệm thu:**
  1. Marker hiện ở khung kế tiếp sau truy vấn OSNAP (≤ 1 ms); 14 marker phân biệt được bằng hình ở ảnh xám.
  2. Marker và tooltip đạt tương phản ≥ 4,5:1 trên nền vùng vẽ tối và sáng.
  3. 100 phiên lệnh ngẫu nhiên kết thúc bằng Esc: revision bản vẽ không đổi, overlay sạch trong 1 khung.
  4. Rubber-band ≤ 8 ms/khung trên BENCH-STD 1 triệu entity.
  5. Đạt DoD-U, DoD-L.

### UIA-110 · Grip: hình, trạng thái, menu grip đa năng, grip nóng
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIA-013, INT-013
- **Màn hình:** UI-A12
- **Đầu vào:** 07.1 §9.5; khung grip INT-013 (grip do module khác đăng ký); prototype `#UI-A12` (bản Pro).
- **Đầu ra:** `app/shell/canvas/GripView.{h,cpp}`: 4 hình (vuông, chữ nhật, tam giác/mũi tên, tròn); 3 trạng thái nguội/rê/nóng khác nhau cả màu và độ đậm; menu nhỏ khi rê grip đa năng (nội dung từ bên đăng ký grip: polyline, kích thước, dynamic block); grip nóng: Space luân phiên Kéo giãn → Di chuyển → Xoay → Tỉ lệ → Đối xứng, prompt trên dòng lệnh; ẩn grip khi vùng chọn vượt GRIPOBJLIMIT (mặc định 1.000).
- **Tiêu chí nghiệm thu:**
  1. Chọn 1.000 đối tượng: grip hiện ≤ 16,6 ms; kéo grip nóng ≤ 8 ms/khung.
  2. Ba trạng thái phân biệt được ở ảnh xám.
  3. Menu grip đa năng mở trong 1 khung sau thời gian chờ rê; nội dung khớp 100% mục đã đăng ký.
  4. Vùng chọn 100.000 đối tượng (vượt giới hạn): không tính grip, chi phí ≤ 1 ms.
  5. Đạt DoD-U, DoD-L.

### UIA-111 · Đường theo dõi và nhập động (DYN)
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** UIA-013, INT-011, INT-014, UI0-007
- **Màn hình:** UI-A12
- **Đầu vào:** 07.1 §9.6 (Tracking), §9.7; INT-011, INT-014; prototype `js/cad.js` (`acquire`, `track`, `cad.getPoint`).
- **Đầu ra:** `app/shell/canvas/TrackingView.{h,cpp}`: dấu cộng điểm đã lấy, đường chấm vô hạn, tooltip "Kéo dài: 1250.00 < 0°", "Polar: 3000.00 < 45°", "Giao điểm: Polar và Kéo dài". `app/shell/canvas/DynInput.{h,cpp}`: nhập con trỏ (X/Y hoặc khoảng cách/góc, Tab chuyển ô, biểu tượng khóa), nhập kích thước gắn trên đối tượng đang tạo, prompt động (↓ mở danh sách tùy chọn), `#` và `@`, viền đỏ khi sai; nhận phím qua `KeySink` của UIA-109.
- **Tiêu chí nghiệm thu:**
  1. Ký tự vào ô DYN hiện ≤ 16 ms p99; ô và tooltip theo con trỏ ≤ 8 ms/khung.
  2. 500 ca (tuyệt đối, tương đối, cực, khóa ô): nhập qua DYN và qua dòng lệnh cho cùng bản ghi INT-002.
  3. Tooltip đạt tương phản ≥ 4,5:1 trên nền vùng vẽ tối và sáng; số theo `UNITS` hiện hành, dấu chấm thập phân (RGN-013).
  4. Đạt DoD-U, DoD-L.

## 15. Thanh trạng thái

### UIA-112 · Nút chế độ hỗ trợ vẽ trên thanh trạng thái
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIA-001, INT-005, UI0-006, UI0-011, UI0-015, UI0-114
- **Màn hình:** UI-A08
- **Đầu vào:** 07.1 §10 (dòng Lưới … Hiển thị độ trong suốt); registry sysvar INT-005; prototype `#UI-A08`, `js/shell.js` (`renderStatus`, `toggle`, `osnapMenu`).
- **Đầu ra:** `app/shell/status/ModeToggles.{h,cpp}`: GRID (F7), SNAP (F9), DYN (F12), ORTHO (F8), POLAR (F10; 90, 45, 30, 22.5, 18, 15, 10, 5 + góc người dùng), Isometric (P1, F5 đổi mặt), OTRACK (F11), OSNAP (F3; danh sách chế độ có ô chọn), hiển thị lineweight, hiển thị độ trong suốt; mỗi nút đổi sysvar bằng lệnh trong suốt; "Thiết lập…" phát lệnh mở hộp thoại tương ứng.
- **Tiêu chí nghiệm thu:**
  1. Bấm nút hoặc phím F: trạng thái đổi trong 1 khung; sysvar đổi từ LISP hoặc `.scr` hiện lên nút trong 1 khung.
  2. F8 khi đang LINE không gián đoạn lệnh; bản ghi INT-002 giống gõ `'ORTHOMODE`.
  3. Bật/tắt phân biệt được không cần màu; tooltip có tên, phím tắt, tên sysvar.
  4. Viewer chỉ có OSNAP (khi đo), lineweight, độ trong suốt (UI0-011).
  5. Đạt DoD-U, DoD-L.

### UIA-009 · Thanh trạng thái: bố cục, tọa độ, chỉ báo, tùy biến
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** UIA-008, UIA-112, RGN-013, NT-013, UI0-009, UI0-112, VN-004, PLF-002, CMD:GRAPHICSCONFIG
- **Màn hình:** UI-A08
- **Đầu vào:** 07.1 §10 (Tọa độ, MODEL/PAPER, chỉ báo đồ họa, chỉ báo Bác sĩ Font, Tác vụ nền, Thông báo, Huy hiệu phiên bản, Clean Screen, Tùy biến); `design/README.md` §Quy ước (Tác vụ nền); prototype `js/shell.js` (`renderStatus`, `updateCoord`, `A.setTask`).
- **Đầu ra:** `app/shell/status/StatusBar.{h,cpp}`: tọa độ (Ctrl+I: tuyệt đối/tương đối/tắt; định dạng RGN-013), MODEL/PAPER, chỉ báo đồ họa (rê: backend, GPU, FPS; bấm: GRAPHICSCONFIG), chỉ báo Bác sĩ Font (rê: "3 font đã thay thế · 250 đối tượng TCVN3"; bấm mở UI-J02 qua lệnh), tác vụ nền NT-013 (tiến trình, hủy qua thành phần UI0-112), thông báo, huy hiệu phiên bản, nút Clean Screen (UIA-001), menu Tùy biến; mục tràn theo ưu tiên ở màn hẹp.
- **Tiêu chí nghiệm thu:**
  1. Tọa độ cập nhật tối đa 1 lần mỗi khung theo tần số màn hình (60/120/144 Hz), ≤ 0,2 ms/lần **[đề xuất]**; chuột 1.000 Hz không tạo thêm lần vẽ.
  2. Rảnh: không timer, không vẽ lại; CPU ≈ 0% trong 60 s (bộ đếm hệ điều hành).
  3. Hủy tác vụ nền từ danh sách: tác vụ dừng ≤ 50 ms, mục biến mất trong 1 khung.
  4. 1366 px: không mục nào bị cắt chữ, mục thừa vào menu tràn; lựa chọn Tùy biến giữ sau khởi động lại.
  5. Đạt DoD-U, DoD-L.

### UIA-113 · Thanh trạng thái EA: tỉ lệ, khóa viewport, workspace, đơn vị, tọa độ theo UCS
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIA-009, UIA-011, UIA-105, INT-015, DOC-021
- **Màn hình:** UI-A08, UI-A10
- **Đầu vào:** 07.1 §10 (Tỉ lệ chú thích/viewport, Khóa viewport, Chuyển workspace, Đơn vị, Thuộc tính nhanh, Hiển thị chú thích); UCS INT-015, danh sách tỉ lệ DOC-021.
- **Đầu ra:** `app/shell/status/ScaleControls.{h,cpp}`: dropdown tỉ lệ chú thích (Model) và tỉ lệ viewport (Layout) 1:1 … 1:1000, tỉ lệ người dùng, "Tùy chỉnh…"; khóa viewport; menu workspace (Soạn thảo 2D, Cổ điển); Đơn vị, Thuộc tính nhanh, Hiển thị chú thích / Tự thêm tỉ lệ (P1); tọa độ theo UCS hiện hành.
- **Tiêu chí nghiệm thu:**
  1. Đổi tỉ lệ viewport là một lệnh, một mark undo; viewport khóa thì dropdown vô hiệu kèm lý do.
  2. Dropdown phản ánh viewport đang kích hoạt trong 1 khung sau khi đổi viewport.
  3. Tọa độ hiển thị khớp `trans` của LISP cho 1.000 điểm ở UCS xoay và dời gốc.
  4. Đạt DoD-U, DoD-L.

### UIA-114 · Tính năng 1.0: cô lập, chọn luân phiên, MODEMACRO, theo dõi biến, workspace người dùng
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIA-109, UIA-113, INT-021, INT-022, CUS-004
- **Màn hình:** UI-A08, UI-A07
- **Đầu vào:** 07.1 §10 (Cô lập, Chọn luân phiên, Theo dõi biến hệ thống, Ô tùy biến DIESEL), §9.3, §3.1 (Workspace người dùng); INT-021, INT-022, CUS-004.
- **Đầu ra:** `app/shell/status/` mở rộng: Cô lập (Cô lập / Ẩn / Kết thúc cô lập); Chọn luân phiên (Ctrl+W) và danh sách nhỏ các đối tượng chồng nhau trên vùng vẽ; ô MODEMACRO; chỉ báo theo dõi biến (chỉ hiện khi biến trong danh sách bị đổi, bấm mở UI-K13 qua lệnh); "Lưu workspace hiện tại…" và chuyển workspace người dùng.
- **Tiêu chí nghiệm thu:**
  1. Ô MODEMACRO cập nhật trong 1 khung sau khi sysvar liên quan đổi; biểu thức DIESEL lỗi hiện thông báo, thanh trạng thái không treo.
  2. Danh sách chọn luân phiên hiện trong 1 khung sau khi dừng rê, khớp INT-009 trên 1.000 điểm ngẫu nhiên.
  3. Lưu workspace rồi khởi động lại: ribbon, palette, toolbar khôi phục đúng 100%.
  4. Đạt DoD-U, DoD-L.

## 16. Tab layout, menu chuột phải, nhấp đúp, kéo-thả, thành phần nổi

### UIA-011 · Tab layout và viewport
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIA-008, UIA-009, DOC-011, RND-014, RND-021, INT-016
- **Màn hình:** UI-A10
- **Đầu vào:** 07.1 §9.12, §9.10 (dòng Viewport); DOC-011, RND-014; prototype `#UI-A10`, `js/shell.js` (`renderLayoutTabs`, `setSpace`), `js/canvas.js` (`vpRect`, `fitPaper`).
- **Đầu ra:** `app/shell/LayoutTabBar.{h,cpp}`: tab Model và layout, thumbnail khi rê, nút "+", menu chuột phải (Layout mới, Từ mẫu…, Xóa, Đổi tên, Di chuyển hoặc Sao chép, Chọn tất cả layout, Kích hoạt layout trước, Quản lý thiết lập trang, In…, Xuất layout thành Model (P1), Ẩn tab); viền viewport, viền đậm cho viewport kích hoạt, biểu tượng khóa ở góc; nhấp đúp trong viewport vào Model, ra ngoài về Paper; tỉ lệ viewport chỉ xem trên thanh trạng thái.
- **Tiêu chí nghiệm thu:**
  1. Chuyển Model ↔ layout có 20 viewport: khung đầu ≤ 100 ms khi đã có cache regen của layout; không chặn luồng UI khi chưa có.
  2. Nhấp đúp vào/ra viewport phát MSPACE/PSPACE, viền đổi trong 1 khung; viewport khóa: zoom bên trong zoom cả layout khi bật tùy chọn.
  3. Mục menu phát lệnh qua INT-002; mục chưa có quyền (Viewer) hiện khóa; thumbnail layout ≤ 150 ms từ cache.
  4. Đạt DoD-U, DoD-L.

### UIA-012 · Menu chuột phải theo ngữ cảnh
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIA-008, INT-003, INT-018, UI0-006
- **Màn hình:** UI-A11
- **Đầu vào:** 07.1 §9.9 (bảng 5 ngữ cảnh, chuột phải theo thời gian); mô hình menu INT-018, tùy chọn lệnh INT-003; prototype `#UI-A11`, `js/cad.js` `cad.context`.
- **Đầu ra:** `app/shell/canvas/ContextMenu.{h,cpp}`: dựng menu từ mô hình INT-018 cho các ngữ cảnh: không lệnh không chọn, có vùng chọn, mục thêm theo loại, trong lệnh (tùy chọn sinh động từ INT-003), grip nóng; bản rút gọn Viewer (Pan, Zoom, Đo, Thuộc tính); chuột phải theo thời gian (P1, SHORTCUTMENUDURATION).
- **Tiêu chí nghiệm thu:**
  1. Menu mở ≤ 16,6 ms (1 khung) với vùng chọn 1 triệu đối tượng; mục theo loại lấy từ thống kê có sẵn, không quét lại vùng chọn.
  2. Mục tùy chọn trong lệnh khớp 100% từ khóa của prompt hiện tại trên mọi prompt đã đăng ký.
  3. Chuột phải theo thời gian: nhả trước 250 ms (giá trị sysvar) là Enter, giữ lâu hơn mở menu.
  4. Đạt DoD-U, DoD-L.

### UIA-014 · Nhấp đúp theo loại đối tượng, thả tệp, biểu tượng UCS
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIA-008, UIA-010, INT-018, RND-020
- **Màn hình:** UI-A13
- **Đầu vào:** 07.1 §9.10, §9.11 (dòng tệp DWG/DXF), §9.13 (biểu tượng UCS); bảng DBLCLKEDIT INT-018; prototype `#UI-A13`, `js/cad.js` `cad.dbl`, xử lý thả ở `js/shell.js` `bindCanvas`.
- **Đầu ra:** `app/shell/canvas/DoubleClick.{h,cpp}`: tra bảng §9.10 ra chuỗi lệnh (sửa chữ tại chỗ, sửa attribute, hỏi mở Block Editor, REFEDIT có xác nhận, chọn hatch và mở tab ngữ cảnh, PEDIT, SPLINEDIT, MLEDIT, ô bảng, sửa ATTDEF, kích hoạt viewport, Thuộc tính nhanh); Viewer mở Thuộc tính chỉ đọc. `DropTarget.{h,cpp}`: tệp thả vào vùng vẽ (V: mở tab mới; chèn và gắn ở UIA-115). Bật/tắt biểu tượng UCS (sysvar UCSICON).
- **Tiêu chí nghiệm thu:**
  1. 12 dòng của bảng §9.10: nhấp đúp phát đúng lệnh (bản ghi INT-002), phản hồi trong 1 khung; vùng trống không phát lệnh.
  2. Ngưỡng thời gian nhấp đúp lấy theo hệ điều hành.
  3. Viewer: thả DWG mở tab mới; tệp không hỗ trợ bị từ chối kèm chú thích cạnh con trỏ.
  4. Đạt DoD-U, DoD-L.

### UIA-115 · Kéo-thả nội dung và đối tượng
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIA-014, INT-008, INT-012, INT-018
- **Màn hình:** UI-A13
- **Đầu vào:** 07.1 §9.11 (bảng 7 dòng); hành động kéo-thả INT-018; prototype `js/cad.js` (`cad.quick`, `quickMove`, `cad.quickEnd`).
- **Đầu ra:** `app/shell/canvas/DragDrop.{h,cpp}`: DWG → chèn block (hỏi điểm chèn) hoặc gắn xref khi giữ Ctrl; ảnh, PDF → gắn ảnh hoặc PDF underlay; block từ palette, Thư viện TCVN, DesignCenter → chèn tại điểm thả theo tỉ lệ chú thích; đối tượng đã chọn nhấn giữ rồi kéo → di chuyển; kéo chuột phải → menu Di chuyển tới đây / Sao chép tới đây / Dán thành block / Hủy; đối tượng → ô Tool Palette; văn bản, bảng từ ứng dụng khác → MText hoặc bảng (P1).
- **Tiêu chí nghiệm thu:**
  1. Mỗi dòng của bảng §9.11 phát đúng chuỗi lệnh so với kịch bản mẫu (bản ghi INT-002); mỗi lần thả là một mark undo.
  2. Bóng xem trước khi kéo ≤ 8 ms/khung với vùng chọn 10.000 đối tượng.
  3. Esc khi đang kéo: revision bản vẽ không đổi, overlay sạch trong 1 khung.
  4. Đạt DoD-U, DoD-L.

### UIA-116 · Điều khiển viewport trên vùng vẽ và ViewCube
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIA-014, INT-016, CMD:NAVVCUBE
- **Màn hình:** UI-A13
- **Đầu vào:** 07.1 §9.13; ViewCube và lệnh NAVVCUBE (CMD:NAVVCUBE); prototype `#UI-A01` (dải `[-][Trên][2D Wireframe]`).
- **Đầu ra:** `app/shell/canvas/ViewportControls.{h,cpp}`: menu `[-]` (phóng to/thu nhỏ viewport, cấu hình viewport, bật ViewCube), menu hướng nhìn (Trên, Dưới, Trái, Phải, Trước, Sau, 4 góc isometric), menu kiểu hiển thị (VSCURRENT); ViewCube ẩn ở bản vẽ 2D, tự hiện khi bản vẽ có đối tượng 3D.
- **Tiêu chí nghiệm thu:**
  1. Chọn hướng nhìn hoặc kiểu hiển thị phát lệnh qua INT-002; nhãn menu phản ánh trạng thái thật trong 1 khung.
  2. 1.000 tệp QA-001: ViewCube hiện đúng với tệp có 3DFACE/3DSOLID (trong 1 khung sau khi có thống kê loại), không hiện với tệp chỉ 2D.
  3. Đạt DoD-U, DoD-L.
- **Ngoài phạm vi:** Navigation bar (P2).

## 17. Thông điệp và lần chạy đầu

### UIA-015 · Thông điệp gắn sự kiện ứng dụng
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIA-008, UIA-009, UI0-008, UI0-009, UI0-112, NT-013, INT-002, DOC-018
- **Màn hình:** UI-A14
- **Đầu vào:** 07.1 §9.14, §12; thành phần UI0-009, UI0-112, UI0-008; prototype `#UI-A14`, `js/shell.js` (`A.banner`, `A.unbanner`, `renderBanners`, `A.setTask`).
- **Đầu ra:** `app/shell/messages/EventRouter.{h,cpp}` và bảng `app/shell/resources/messages/event_map.json`: sự kiện → hình thức. Banner (thiếu tham chiếu, đã thay font, tệp chỉ đọc, đã phục hồi); toast (kết quả tác vụ nền, tự ẩn 5 s, có hành động); chú thích cạnh con trỏ (lỗi tức thời của lệnh); con trỏ bận > 0,5 s, tiến trình và nút hủy > 2 s; hộp thoại lỗi có chi tiết; thông báo quan trọng của lệnh hiện thêm ở góc vùng vẽ.
- **Tiêu chí nghiệm thu:**
  1. Mỗi sự kiện trong bảng có kiểm thử: phát sự kiện → đúng hình thức, đúng chuỗi, hiện trong 1 khung.
  2. Con trỏ bận ở 0,5 s ±50 ms; tiến trình và nút hủy ở 2 s ±50 ms; hủy phản hồi ≤ 50 ms.
  3. 50 toast liên tiếp: tối đa 3 hiện cùng lúc **[đề xuất]**, phần còn lại vào trung tâm thông báo; không khung nào > 16,6 ms.
  4. Banner không chặn: vùng vẽ vẫn nhận chuột và phím; đóng banner được bằng bàn phím.
  5. Đạt DoD-U, DoD-L.

### UIA-016 · Lần chạy đầu (onboarding)
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIA-005, UIA-008, UIA-009, UI0-002, UI0-005, PLF-002, PLF-008, NT-010
- **Màn hình:** UI-A15
- **Đầu vào:** 07.1 UI-A15 (bản V bỏ bước 3, 4, 5); đồng ý telemetry PLF-008; prototype `#UI-A15`, `SC.onboarding` (theo `js/catalog.js`).
- **Đầu ra:** `app/shell/onboarding/Onboarding.{h,cpp}`: bước Ngôn ngữ, Chủ đề, Quyền riêng tư (telemetry mặc định tắt), Phiên bản (Viewer miễn phí; dùng thử và key dẫn sang UIA-117), hướng dẫn nhanh 5 bước trên bản vẽ mẫu (tô sáng ribbon, dòng lệnh, vùng vẽ, thanh trạng thái); bỏ qua được, xem lại từ Tùy chọn; khung bước mở rộng được; bản vẽ mẫu `app/shell/resources/samples/`.
- **Tiêu chí nghiệm thu:**
  1. Đổi ngôn ngữ và chủ đề ở bước 1–2 áp ngay ≤ 100 ms, không khởi động lại.
  2. Bỏ qua mọi bước: telemetry tắt, không gói nào rời máy qua PLF-008 (kiểm bằng proxy mạng của test).
  3. Bỏ qua ở bất kỳ bước nào vào thẳng ứng dụng; lần chạy sau không hiện lại; "Xem lại" chạy từ đầu.
  4. Hướng dẫn 5 bước đi hết bằng bàn phím; vùng đang tô sáng có tên cho trình đọc màn hình.
  5. Đạt DoD-U, DoD-L.

### UIA-117 · Lần chạy đầu bản đầy đủ: kiểu giao diện, chuyển cài đặt AutoCAD, đơn vị và mẫu
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** UIA-016, UIA-105, CUS-003, PLF-003, PLF-004
- **Màn hình:** UI-A15
- **Đầu vào:** 07.1 UI-A15 (bước 3, 4, 5, 7); engine chuyển cài đặt CUS-003; kích hoạt PLF-003, dùng thử PLF-004.
- **Đầu ra:** thêm vào `app/shell/onboarding/`: Kiểu giao diện (Ribbon / Cổ điển, áp workspace UIA-105); Chuyển cài đặt từ AutoCAD (chỉ hiện khi CUS-003 phát hiện AutoCAD, mở UI-K12 qua lệnh TCMIGRATE); Đơn vị và mẫu mặc định (mm, TCVN A3/A1…); bước Phiên bản đủ: dùng thử Pro 30 ngày, kích hoạt bằng key.
- **Tiêu chí nghiệm thu:**
  1. Máy sạch: bước 4 không hiện; máy giả lập có AutoCAD (profile, registry): bước 4 hiện và mở đúng UI-K12.
  2. Chọn Cổ điển: lần mở đầu dùng workspace Cổ điển; chọn mẫu TCVN A3: NEW dùng đúng mẫu đó.
  3. Bắt đầu dùng thử: trạng thái giấy phép đổi ≤ 2 s khi mạng tốt; lỗi mạng có thông báo và nút thử lại.
  4. Đạt DoD-U, DoD-L.

## 18. Viewer

### UIA-118 · Layer trong Viewer và lưu trạng thái hiển thị
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIA-005, DOC-004, INT-002, NT-011, PLF-002
- **Màn hình:** UI-M04, UI-M03
- **Đầu vào:** 07.3 UI-M04, UI-M03 ("Lưu trạng thái hiển thị", VP); prototype `#UI-M04`, `js/cmd.js` (`tempLayer`, `IMPL.LAYOFF`, `IMPL.LAYFRZ`, `IMPL.LAYISO`).
- **Đầu ra:** `app/shell/viewer/ViewerLayers.{h,cpp}`: bật/tắt, đóng băng tạm thời, cô lập, tìm layer qua lệnh layer được INT-020 cho phép ở Viewer; "Khôi phục như tệp gốc" (trạng thái gốc chụp lúc mở); ghi chú "Thay đổi không lưu vào bản vẽ". `ViewState.{h,cpp}` (VP): lưu và nạp view, layout, trạng thái layer vào tệp `.tcview` cạnh bản vẽ.
- **Tiêu chí nghiệm thu:**
  1. Bản vẽ 5.000 layer: lọc ≤ 16 ms/phím; bật/tắt hiện trên vùng vẽ ở khung kế tiếp sau regen gia tăng.
  2. "Khôi phục như tệp gốc" trả đúng trạng thái lúc mở (so từng layer); tệp DWG trên đĩa giữ nguyên hash sau phiên.
  3. `.tcview` ghi rồi đọc lại cho cùng view và trạng thái layer; tệp hỏng hoặc > 1 MB **[đề xuất]** bị bỏ qua kèm banner; fuzz bộ đọc 1 giờ không lỗi.
  4. Đạt DoD-U, DoD-L.

### UIA-017 · Viewer: ribbon Viewer, thanh trạng thái giản lược, chế độ chỉ xem
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** UIA-005, UIA-007, UIA-009, UIA-118, INT-020, PLF-002
- **Màn hình:** UI-M01, UI-M03, UI-M04
- **Đầu vào:** 07.3 UI-M01, UI-M03; 07.1 §2, §3.1 (workspace Viewer); prototype `#UI-M01`, `#UI-M03`, `js/shell.js` (`ribbonData`, `edBadge`).
- **Đầu ra:** `app/shell/resources/ribbon/viewer.json`: tab Xem, Đo, Ghi chú (VP), In & Xuất, Công cụ (VP); panel cố định cuối ribbon "Sửa bản vẽ? Dùng thử Pro 30 ngày" (mở UI-L04). Workspace Viewer dùng ribbon này và thanh trạng thái giản lược (tọa độ, MODEL/PAPER, bắt điểm khi đo, lineweight, tỉ lệ viewport chỉ xem, chỉ báo font, tác vụ nền, thông báo, huy hiệu); thanh tiêu đề "[Chỉ xem]". Kiểm thử `app/shell/tests/viewer_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. 1.000 tệp QA-001 mở ở Viewer, chạy kịch bản xem, đo, in: không tệp nào bị ghi.
  2. 100% lệnh sửa của 03 Phụ lục A bị chặn kèm thông báo nâng cấp; 0 thay đổi bản vẽ ngoài trạng thái layer tạm.
  3. Mục VP hiện huy hiệu khóa ở Viewer và mở ngay khi giấy phép thành VP (PLF-002), không cần khởi động lại.
  4. Ảnh chụp ribbon và thanh trạng thái khớp prototype `#UI-M01`.
  5. Đạt DoD-U, DoD-L.
- **Ngoài phạm vi:** palette Thuộc tính chỉ đọc có sao chép giá trị (task palette Thuộc tính ở ui-1); markup UI-M02.

## 19. macOS và nhiều cửa sổ

### UIA-018 · Bản macOS
- **Mốc:** 1.0 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** UIA-002, UIA-003, UIA-008, UIA-105, UIA-106, UI0-111, UI0-114
- **Màn hình:** UI-MAC
- **Đầu vào:** 07.1 §13.6; `design/README.md` §Quy ước (macOS, `TC.keyLabel`); hộp thoại sheet UI0-111; bảng KDEF UI0-114; prototype `#UI-MAC`.
- **Đầu ra:** `app/shell/mac/`: thanh menu hệ thống (TrionCAD: Giới thiệu, Cài đặt… ⌘,, Thoát ⌘Q; Tệp, Sửa, Hiển thị…, Cửa sổ, Trợ giúp) dựng từ định nghĩa menu Cổ điển và menu ứng dụng; thanh tiêu đề chừa chỗ ba nút cửa sổ bên trái, QAT dời phải; ⌘ cho Mới, Mở, Lưu, In, Hoàn tác, Sao chép, Dán, Ctrl giữ nghĩa CAD; tổ hợp thay thế F3–F12; Ctrl + bấm hoặc bấm hai ngón là chuột phải; chụm zoom, kéo hai ngón pan, Magic Mouse; toàn màn hình, Split View; hộp thoại tệp và in dạng sheet qua UI0-111.
- **Tiêu chí nghiệm thu:**
  1. 100% mục thanh menu hệ thống phát lệnh qua UI0-015; "Cài đặt…" thay "Tùy chọn" ở mọi chỗ.
  2. Mọi toggle F3–F12 dùng được không cần Fn; không tổ hợp ⌘ nào trùng phím hệ thống macOS (kiểm tự động trên bảng KDEF).
  3. Cử chỉ touchpad và Magic Mouse: khung pan/zoom ≤ 16,6 ms p99 trên Mac Apple Silicon với BENCH-STD 1 triệu entity.
  4. Bộ gõ macOS, EVKey, OpenKey đạt lại tiêu chí 1–2 của UIA-106 trong bản phát hành.
  5. Ảnh chụp khớp prototype `#UI-MAC` ở Tối/Sáng, Retina 200%.
  6. Đạt DoD-U, DoD-L.

### UIA-019 · Nhiều cửa sổ bản vẽ: xếp lớp, chia ô; Clean Screen với nhiều cửa sổ
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** UIA-001, UIA-008, UIA-010, DOC-018
- **Lệnh:** SYSWINDOWS
- **Màn hình:** UI-A01
- **Đầu vào:** 07.1 §9.15, §3 (Clean Screen); phiên nhiều bản vẽ DOC-018; prototype `js/shell.js` `A.cleanScreen`.
- **Đầu ra:** `app/shell/DocumentArea.{h,cpp}`: chế độ tab (mặc định), xếp lớp, chia ô ngang/dọc (lệnh SYSWINDOWS, nút ở panel Giao diện tab Hiển thị); mỗi cửa sổ một vùng vẽ giữ riêng view, layout đang mở, vùng chọn; các vùng vẽ dùng chung một QRhi (nhiều swapchain) **[đề xuất]**; Clean Screen (UIA-001) áp cho mọi cửa sổ; `docs/specs/commands/SYSWINDOWS.md`.
- **Tiêu chí nghiệm thu:**
  1. 4 bản vẽ chia ô, mỗi bản BENCH-STD 1 triệu entity: pan cửa sổ kích hoạt ≤ 16,6 ms/khung; cửa sổ khác không vẽ lại khi không đổi.
  2. Chuyển cửa sổ kích hoạt ≤ 100 ms; sau 100 lần chuyển, view, layout, vùng chọn từng bản vẽ giữ nguyên.
  3. Rảnh với 4 cửa sổ: CPU và GPU ≈ 0%; Clean Screen bật/tắt khôi phục đúng bố cục chia ô.
  4. Đạt DoD-U, DoD-L.

### UIA-119 · Cửa sổ bản vẽ nổi, kéo sang màn hình khác
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** UIA-019, UI0-012
- **Màn hình:** UI-A01
- **Đầu vào:** 07.1 §9.15 (P2); UI0-012 (đổi DPI khi kéo giữa màn hình).
- **Đầu ra:** mở rộng `app/shell/DocumentArea.{h,cpp}`: kéo tab tệp ra thành cửa sổ nổi có vùng vẽ riêng, kéo trở lại để gộp; nhớ màn hình và vị trí khi mở lại.
- **Tiêu chí nghiệm thu:**
  1. Kéo tab ra cửa sổ nổi ≤ 150 ms đến khung có nội dung, không regen lại (dùng chung bộ đệm GPU của bản vẽ).
  2. Kéo cửa sổ nổi giữa màn hình 100% và 200%: dựng lại trong 1 khung, không nhòe.
  3. Đạt DoD-U, DoD-L.

## Tóm tắt theo mốc

| Mốc | Task | Số task |
|---|---|---|
| S6 | UIA-106 | 1 |
| V | UI0-101, UI0-001, UI0-102, UI0-002, UI0-003, UI0-103, UI0-004, UI0-104, UI0-005, UI0-105, UI0-014, UI0-106, UI0-107, UI0-108, UI0-006, UI0-109, UI0-007, UI0-110, UI0-008, UI0-112, UI0-009, UI0-114, UI0-015, UI0-011, UI0-012, UI0-113, UI0-010, UI0-013, UI0-115, UIA-001, UIA-002, UIA-010, UIA-004, UIA-003, UIA-102, UIA-005, UIA-107, UIA-108, UIA-007, UIA-109, UIA-008, UIA-013, UIA-112, UIA-009, UIA-011, UIA-012, UIA-014, UIA-015, UIA-016, UIA-118, UIA-017 | 51 |
| VP | UIA-103 | 1 |
| EA | UI0-116, UIA-101, UIA-104, UIA-105, UIA-006, UIA-110, UIA-111, UIA-113, UIA-115, UIA-117 | 10 |
| 1.0 | UI0-117, UI0-111, UIA-114, UIA-116, UIA-018, UIA-019 | 6 |
| Sau | UIA-119 | 1 |
| **Tổng** | | **70** |
