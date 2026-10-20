# PLT · In và xuất

> Thiết lập trang, plot style CTB/STB, pipeline in vector, bộ ghi PDF tự viết, in qua QPrinter, xem trước, xuất ảnh, PUBLISH, in hàng loạt theo khung tên, PC3/PMP, nhập PDF qua PDFium, Sheet Set.
> Thư mục mã: `plot/` (README §4.1). Căn cứ: [03](../03-chuc-nang-autocad.md) §12–14, §24; [05](../05-kien-truc-ky-thuat.md) §1.1, §1.2, §9.2; [06](../06-lo-trinh-trien-khai.md) §3–4; [07.2](../07-dac-ta-ui-2-bang-va-hop-thoai.md) UI-D05, UI-G14, UI-H01…UI-H10.

## 0. Ghi chú thiết kế

- **Một pipeline cho mọi đầu ra.** PLT-003 sinh `PlotStream` theo tờ: primitive theo tọa độ giấy (mm, `double`), bút đã phân giải theo plot style. PDF (PLT-004), máy in (PLT-005), xem trước (PLT-006), ảnh (PLT-007) chỉ đọc `PlotStream`, không tự regen.
- In và xuất đọc snapshot DOC-009 ở luồng nền, đăng ký tác vụ NT-013 (tiến trình, hủy ≤ 50 ms theo DoD-H). Người dùng vẫn sửa bản vẽ trong lúc in.
- **Song song** theo tờ, theo viewport, theo stream nén (05 §9.2). Bộ ghép ghi tuần tự theo thứ tự tờ nên tệp ra trùng từng bit với mọi số luồng. Ngày giờ ghi vào tệp lấy từ tham số (CI cố định).
- Bộ ghi PDF tự viết (03 §24). Chỉ dùng thư viện đã có trong 05 §2: zlib/libdeflate qua NT-014; FreeType, HarfBuzz (tách tập con font bằng `hb-subset` **[đề xuất]**); libsodium (BLAKE2b cho `/ID` **[đề xuất]**). PDFium (BSD) chỉ dùng để đọc PDF (PLT-011).
- **Raster đi một đường:** bộ vẽ QPainter trên CPU của PLT-005, chia dải song song, dùng cho máy in raster, đích PNG và xuất ảnh **[đề xuất]**. Kết quả lặp lại được, không phụ thuộc GPU. Xem trước tương tác dùng GPU qua RND-014.
- CTB, STB, PC3, PMP (vỏ PIA), `.dsd`, `.dst`, PDF là đầu vào không tin cậy: mỗi parser có fuzz target (DoD-C 7). Định dạng tự khảo sát ghi đặc tả ở `docs/specs/formats/`.
- **Chất lượng in** đo bằng QA-005 so với PDF do DWG TrueView in (QA-004): SSIM ≥ 0,97 trên ≥ 95% tờ ở V (cổng G1, 06 §3); ≥ 0,98 trên ≥ 97% tờ ở 1.0 (05 §1.2).
- Xuất PDF: hiệu suất song song ≥ 70% đến 16 luồng (05 §1.1). Các ngưỡng thông lượng là **[đề xuất]**, chốt khi đo trên máy tham chiếu.
- Lệnh chạy giống nhau từ bàn phím, `.scr`, `(command ...)`; FILEDIA = 0 thì hỏi trên dòng lệnh. Cổng phiên bản qua INT-020: Viewer có PLOT, PREVIEW, EXPORTPDF, PNGOUT…; PUBLISH, TCBATCHPLOT từ Viewer Pro.
- Thư mục con **[đề xuất]**: `plot/setup`, `plot/style`, `plot/pipeline`, `plot/pdf`, `plot/device`, `plot/raster`, `plot/preview`, `plot/batch`, `plot/pdfium`, `plot/pdfimport`, `plot/sheetset`, `plot/commands`; kiểm thử `plot/tests`, benchmark `plot/bench`, fuzz `plot/fuzz`.

## 1. Thiết lập trang và plot style

### PLT-001 · Mô hình thiết lập trang, danh mục khổ giấy, PLOTSETTINGS và LAYOUT
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DOC-004, DOC-007, DOC-011, DOC-018, DWG-013, NT-010, QA-004
- **Màn hình:** UI-H01, UI-H02
- **Đầu vào:** 03 §13; 07.2 UI-H01, UI-H02; schema PLOTSETTINGS, LAYOUT (DWG-013); khổ ISO 216 (A0–A4, khổ dài), ANSI; tên khổ trong layout của kho QA-001.
- **Đầu ra:**
  - `plot/setup/page_setup.h`: `PageSetup` có kiểu (thiết bị, khổ, đơn vị, vùng in, tỉ lệ, offset, xoay, in ngược, plot style, cờ tùy chọn); đọc/ghi LAYOUT và page setup có tên (`ACAD_PLOTSETTINGS`).
  - `plot/setup/media_catalog.h`: thiết bị dựng sẵn (PDF của TrionCAD, PNG), phân tích tên khổ kiểu `ISO_full_bleed_A1_(841.00_x_594.00_MM)`, tra khổ theo kích thước.
  - `importPageSetups()`: nhập từ DWG/DWT/DXF mở chỉ đọc.
- **Tiêu chí nghiệm thu:**
  1. Đọc → ghi → đọc trùng 100% trường PLOTSETTINGS/LAYOUT trên kho 1.000 tệp (QA-001, so qua DOC-017).
  2. ≥ 99,5% layout của kho 10.000 tệp phân giải đúng kích thước khổ và vùng in được (ca vàng lấy từ PDF do DWG TrueView in, QA-004).
  3. Nhập page setup chép đủ mọi page setup có tên của 100 tệp; trùng tên trả danh sách xung đột, không tự ghi đè.
  4. Mọi thay đổi qua transaction DOC-007.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** khổ đọc từ PC3/PMP (PLT-010), khổ của driver máy in (PLT-005).

### PLT-101 · Biến đổi in: vùng in, tỉ lệ, offset, xoay
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLT-001, GEO-001, GEO-002, DOC-009, DOC-010, QA-009
- **Màn hình:** UI-H02
- **Đầu vào:** 07.2 UI-H02 (Vùng in, Offset, Tỉ lệ, Hướng giấy); sysvar PLOTOFFSET, PLOTROTMODE; EXTMIN/EXTMAX, LIMMIN/LIMMAX; bảng VIEW.
- **Đầu ra:** `plot/setup/plot_transform.h`: `computePlotTransform(PageSetup, Snapshot, ViewState) → PlotTransform` (ma trận bản vẽ → giấy mm, hình chữ nhật clip, tỉ lệ thực khi Vừa khổ giấy); 6 vùng in (Layout, Toàn bộ đối tượng, Màn hình hiện tại, Giới hạn, View có tên, Cửa sổ); canh giữa, offset X/Y, xoay 0/90/180/270, in ngược, mm/inch.
- **Tiêu chí nghiệm thu:**
  1. 300 ca vàng (6 vùng in × 4 góc × in ngược × canh giữa/offset × mm/inch × PLOTOFFSET 0/1) khớp ma trận kỳ vọng, sai số ≤ 1e-9 mm.
  2. Tỉ lệ Vừa khổ giấy khớp tỉ lệ AutoCAD đã lưu trong ≥ 99,9% layout dùng Fit của kho 10.000 tệp, sai số tương đối ≤ 1e-6.
  3. Bất biến (QA-009, 1 triệu ca): xoay 4 lần 90° về đồng nhất; canh giữa cho hai lề bằng nhau ±1e-9 mm; Cửa sổ chọn ngược chiều cho cùng kết quả.
  4. Toàn bộ đối tượng lấy hộp bao từ chỉ mục snapshot: ≤ 2 ms với 1 triệu entity (BENCH-STD) **[đề xuất]**.
  5. Đạt DoD-C.

### PLT-102 · Vỏ tệp PIA, đọc/ghi CTB và STB
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-011, NT-012, NT-014, QA-004, QA-006
- **Đầu vào:** 07.2 UI-D05 (thuộc tính của kiểu); tệp CTB/STB do doanh nghiệp pilot cung cấp; định dạng tự khảo sát (dòng `PIAFILEVERSION_2.0`, thân nén zlib, cây khóa–giá trị).
- **Đầu ra:**
  - `plot/style/pia_file.h`: vỏ PIA dùng chung cho CTB, STB, PC3, PMP (tiêu đề, checksum, giải nén có giới hạn, cây khóa–giá trị giữ thứ tự).
  - `plot/style/plot_style_table.h`: `PlotStyleTable` (255 kiểu theo màu hoặc kiểu có tên, bảng độ dày tùy chỉnh, hệ số linetype, `aci_table`).
  - `docs/specs/formats/pia-ctb-stb.md`; `plot/fuzz/pia_fuzz.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Đọc 100% bộ 300 tệp CTB/STB mẫu **[đề xuất]**; đọc → ghi → đọc trùng 100% trường; văn bản sau giải nén trùng từng byte bản gốc khi không sửa.
  2. DWG TrueView in 50 tờ thử bằng tệp do TrionCAD ghi và bằng tệp gốc: content stream của hai PDF trùng từng byte (QA-004).
  3. Tệp hỏng trả lỗi có vị trí; giới hạn giải nén 16 MB **[đề xuất]**; fuzz ≥ 1 giờ không lỗi.
  4. Đọc một tệp ≤ 1 ms trên máy tầm trung **[đề xuất]**.
  5. Đạt DoD-C.

### PLT-002 · Áp dụng plot style CTB/STB, bộ CTB mẫu
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLT-102, RGN-002, DOC-004, NT-006, QA-004
- **Màn hình:** UI-D05
- **Đầu vào:** 03 §13; 07.2 UI-D05; sysvar PSTYLEMODE, LWDEFAULT; màu ACI/true color và độ dày đã phân giải (RGN-002).
- **Đầu ra:**
  - `plot/style/pen_resolver.h`: `PenResolver::resolve(attrs) → Pen` (màu in, thang xám, screening, linetype ghi đè, độ dày, đầu nét, mối nối, 9 kiểu tô); cache theo khóa thuộc tính; kiểu không tìm thấy → `Normal` kèm cảnh báo.
  - Bộ mẫu tự soạn, giữ tên chuẩn: `acad.ctb`, `monochrome.ctb`, `grayscale.ctb`, `Screening 25%/50%/75%/100%.ctb`, `acad.stb`, `monochrome.stb` ở `plot/data/plotstyles/`.
- **Tiêu chí nghiệm thu:**
  1. 255 ACI × 7 CTB mẫu: màu in khớp 100% màu trong PDF do DWG TrueView in cùng CTB (QA-004); thang xám, screening lệch ≤ 1/255.
  2. 80 ca vàng độ dày (theo đối tượng, theo kiểu, ByLayer/ByBlock, LWDEFAULT) và tên kiểu STB (ByLayer, ByBlock, Normal, tên riêng) đúng 100%.
  3. 9 kiểu tô của plot style sinh nét mẫu đúng khoảng cách, góc và clip theo vùng tô trên 30 ca vàng.
  4. Phân giải 1 triệu primitive ≤ 20 ms trên máy tầm trung **[đề xuất]**, 0 cấp phát trong vòng lặp.
  5. Đạt DoD-C, DoD-H.

### PLT-103 · Lệnh PAGESETUP
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLT-001, PLT-101, INT-001, INT-002, INT-020
- **Lệnh:** PAGESETUP
- **Màn hình:** UI-H01
- **Đầu vào:** 07.2 UI-H01, UI-H02 (chế độ thiết lập trang); sysvar PAPERUPDATE; API PLT-001.
- **Đầu ra:** `plot/commands/pagesetup.cpp`: Đặt hiện hành, Mới (từ `<Không>`, `<Mặc định>` hoặc page setup có sẵn), Sửa, Đổi tên, Xóa, Nhập (DWG/DWT/DXF), cờ "Hiện khi tạo layout mới" (NT-010); `docs/specs/commands/PAGESETUP.md`.
- **Tiêu chí nghiệm thu:**
  1. Đặt hiện hành chép 100% trường page setup có tên vào LAYOUT (so DOC-017), cho cả Model và layout.
  2. Khổ không có trên thiết bị → cảnh báo theo PAPERUPDATE; tỉ lệ ≤ 0, Cửa sổ rỗng, View không tồn tại → lỗi rõ ràng, dữ liệu không đổi.
  3. Mỗi thao tác là một transaction DOC-007; Viewer chỉ đổi trong phiên, không ghi tệp (INT-020).
  4. Mỗi thao tác có ca kiểm theo đặc tả lệnh, gồm nhánh lỗi.
  5. Đạt DoD-C.

### PLT-104 · Lệnh STYLESMANAGER và backend trình sửa plot style
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLT-002, PLT-102, NT-010, NT-011, INT-001, INT-020, QA-004, QA-005
- **Lệnh:** STYLESMANAGER
- **Màn hình:** UI-D05
- **Đầu vào:** 03 §13 (P0 dùng và sửa); 07.2 UI-D05; thư mục plot style trong cài đặt (NT-010).
- **Đầu ra:** `plot/style/style_editor.h`: liệt kê thư mục; tạo mới (từ đầu, từ tệp có sẵn); sửa nhiều kiểu một lần; sửa bảng độ dày; thêm/xóa/đổi tên kiểu STB; hệ số linetype không ISO; Lưu/Lưu thành (ghi nguyên tử); hoàn tác trong trình sửa; `plot/commands/stylesmanager.cpp`; `docs/specs/commands/STYLESMANAGER.md`.
- **Tiêu chí nghiệm thu:**
  1. Mọi thuộc tính của UI-D05 sửa được qua API; sửa màu 1–7 cùng lúc cho kết quả bằng sửa từng màu.
  2. Đổi một giá trị trong bảng độ dày cập nhật mọi kiểu dùng giá trị đó; không xóa được kiểu `Normal`.
  3. DWG TrueView in bằng tệp đã sửa cho kết quả trùng TrionCAD (QA-004, QA-005: SSIM ≥ 0,99 trên 20 tờ thử).
  4. Sửa 255 kiểu một lần ≤ 16 ms trên máy tầm trung; lỗi khi ghi không làm hỏng tệp cũ (NT-011).
  5. Đạt DoD-C.

### PLT-013 · CONVERTCTB, CONVERTPSTYLES
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** PLT-002, PLT-004, DOC-007, DOC-008, INT-002, INT-005
- **Lệnh:** CONVERTCTB, CONVERTPSTYLES
- **Đầu vào:** 03 §13 (P1); 07.2 UI-D05 (Chuyển đổi); sysvar PSTYLEMODE, PSTYLEPOLICY, DEFPLSTYLE, DEFLPLSTYLE.
- **Đầu ra:** `plot/style/convert.h`: CTB → STB kèm `aci_table` (màu → tên kiểu); chuyển bản vẽ theo màu ↔ theo tên (gán tên kiểu cho layer và đối tượng theo màu; chiều ngược xóa tên kiểu, gắn CTB cho layout); `plot/commands/convertctb.cpp`, `convertpstyles.cpp`; `docs/specs/commands/CONVERTCTB.md`, `CONVERTPSTYLES.md`.
- **Tiêu chí nghiệm thu:**
  1. Bất biến: 100 bản vẽ (QA-001) in bằng CTB và in sau CONVERTCTB + CONVERTPSTYLES cho content stream PDF (PLT-004) trùng từng byte; chuyển ngược về CTB cũng vậy.
  2. Cảnh báo trước khi chạy nêu số layer và đối tượng bị đổi; PSTYLEMODE đổi đúng.
  3. Mỗi lệnh một mark undo; undo khôi phục 100% trường (so DOC-017).
  4. Bàn phím, `.scr`, `(command ...)` cho cùng kết quả.
  5. Đạt DoD-C.

## 2. Pipeline in vector

### PLT-003 · Pipeline in vector: từ snapshot đến PlotStream theo tờ
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** PLT-101, PLT-002, RGN-003, RGN-006, RGN-024, RGN-027, DOC-005, NT-005
- **Màn hình:** UI-H02
- **Đầu vào:** 05 §8, §9.2; 07.2 UI-H02 (Tùy chọn in); regen song song (RGN-003), viewport (RGN-024), thứ tự vẽ (RGN-027), font thay thế (RGN-006), hyperlink trong xdata (DOC-005).
- **Đầu ra:**
  - `plot/pipeline/plot_stream.h`: `PlotStream` (primitive theo giấy mm, bút đã phân giải, đường clip, ảnh, text kèm font, nhãn layer, vùng hyperlink).
  - `plot/pipeline/sheet_builder.h`: gom paper space và từng viewport (chữ nhật, đa giác, theo đối tượng), clip; bỏ đối tượng không in (layer không in, Defpoints, tắt/đóng băng, VP freeze, viewport tắt); tùy chọn in độ dày nét, co giãn độ dày theo tỉ lệ, in độ trong suốt, theo plot style, paper space sau cùng, ẩn đối tượng paper space (thuật toán họa sĩ cho 3DFACE và mesh **[đề xuất]**).
  - `PlotReport`: font bị thay thế, đối tượng ngoài vùng in, plot style không tìm thấy, nét < 0,1 mm (dùng cho kiểm tra trước khi in).
- **Tiêu chí nghiệm thu:**
  1. 80 ca vàng về loại trừ đối tượng và tùy chọn in đúng 100% (so hash PlotStream kỳ vọng).
  2. Không primitive nào vượt biên viewport quá 1e-9 × cạnh tờ, kiểm trên 1.000 tờ (QA-001).
  3. Tờ 1 triệu entity (BENCH-STD) → PlotStream ≤ 0,8 s trên máy tầm trung **[đề xuất]**; hiệu suất song song ≥ 70% đến 16 luồng (05 §1.1).
  4. Hash PlotStream giống nhau với 1–32 luồng và trên 3 hệ điều hành.
  5. PlotReport đủ 4 loại cảnh báo trên ca vàng, 0 cảnh báo thừa trên 100 tờ sạch.
  6. Đạt DoD-C, DoD-H.

## 3. Bộ ghi PDF

### PLT-105 · PDF: cấu trúc tệp, layer OCG, bookmark, hyperlink, ghi song song theo tờ
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** PLT-003, NT-005, NT-011, NT-012, NT-014
- **Đầu vào:** "ISO 32000-1:2008, Document management — Portable document format — Part 1: PDF 1.7"; 03 §24; 05 §9.2 (xuất PDF theo tờ); 07.2 UI-H05 (Dữ liệu kèm theo).
- **Đầu ra:**
  - `plot/pdf/pdf_writer.h`: đối tượng gián tiếp, xref stream, object stream, trailer, `/Info`, `/ID`, cây trang; ghi ra tệp tạm rồi đổi tên (NT-011).
  - `plot/pdf/sheet_job.h`: mỗi tờ dựng khối độc lập song song (NT-005), nén từng stream song song (NT-014); bộ ghép đánh số đối tượng theo thứ tự tờ, giữ tối đa 2 × số luồng tờ trong bộ nhớ **[đề xuất]**.
  - `/OCProperties` theo layer, `/Outlines` theo tờ (lồng theo bản vẽ khi nhiều bản vẽ), annotation `/Link` (URI, tệp, view có tên); chuỗi UTF-16BE.
- **Tiêu chí nghiệm thu:**
  1. `qpdf --check` sạch trên 100% tệp kiểm thử (công cụ chỉ chạy trong CI, không liên kết vào sản phẩm) **[đề xuất]**.
  2. Tệp trùng từng byte với 1–32 luồng và trên 3 hệ điều hành.
  3. Bộ 64 tờ: hiệu suất song song ≥ 70% đến 16 luồng; bộ 500 tờ: bộ nhớ đỉnh ≤ 512 MB **[đề xuất]**.
  4. Layer, bookmark, link (đọc bằng `qpdf --json`) khớp 100% đầu vào, kể cả tên tiếng Việt.
  5. Hủy ≤ 50 ms, không để lại tệp dở.
  6. Đạt DoD-C, DoD-H.

### PLT-106 · PDF: content stream từ PlotStream
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLT-105, GEO-005, GEO-007, NT-006, QA-009
- **Đầu vào:** ISO 32000-1 (toán tử path, clip, ExtGState, marked content); `PlotStream` (PLT-003); 07.2 UI-H05 (độ phân giải vector, gộp các nét chồng nhau).
- **Đầu ra:** `plot/pdf/content_builder.h`: path (đường, polyline; cung, tròn, ellipse, đoạn spline → Bézier bậc 3, spline bậc ≤ 3 không hữu tỉ tách chính xác), stroke/fill (nonzero, even-odd), độ dày, đầu nét, mối nối, clip viewport, độ trong suốt, màu RGB/xám, marked content `/OC` theo layer; tọa độ làm tròn theo lưới DPI vector; định dạng số tất định, không theo locale; bỏ toán tử trạng thái thừa; tùy chọn gộp đoạn thẳng trùng hoặc chồng cùng bút.
- **Tiêu chí nghiệm thu:**
  1. Bézier lệch ≤ 0,01 mm trên giấy so với phép đánh giá đường cong của GEO-005 (cung, tròn, ellipse) và GEO-007 (spline) **[đề xuất]**, kiểm 1 triệu ca (QA-009).
  2. Thông lượng ≥ 10 triệu đoạn thẳng/giây/luồng trước nén **[đề xuất]**; 0 cấp phát trong vòng lặp (NT-004).
  3. Gộp nét chồng bỏ 100% đoạn trùng/chồng cùng bút trên ca vàng; tập điểm được phủ trước và sau gộp bằng nhau (so hợp khoảng trên từng đường thẳng, dung sai 1e-9 mm).
  4. Tọa độ lệch ≤ nửa ô lưới DPI vector; cùng PlotStream cho content stream trùng từng byte trên 3 hệ điều hành.
  5. Đạt DoD-C, DoD-H.

### PLT-107 · PDF: nhúng tập con font TrueType, text tìm và copy được
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLT-106, RGN-005, RGN-006, NT-012
- **Đầu vào:** ISO 32000-1 (CIDFontType2, Identity-H, ToUnicode); đặc tả OpenType (bảng `OS/2`, cờ `fsType`); glyph và vị trí sau shaping (RGN-005).
- **Đầu ra:** `plot/pdf/font_embed.h`: tách tập con bằng `hb-subset` **[đề xuất]**, gộp glyph dùng trên mọi tờ, tách song song theo font; `/W`, FontDescriptor, ToUnicode (gồm dấu tổ hợp tiếng Việt); ma trận text cho width factor, oblique, ngược/lộn ngược; font cấm nhúng hoặc tùy chọn "chuyển thành nét" → path từ đường viền glyph.
- **Tiêu chí nghiệm thu:**
  1. Trích text bằng pdf.js trong CI **[đề xuất]** trả đúng 100% chuỗi NFC của bộ 2.000 chuỗi tiếng Việt dựng sẵn và tổ hợp **[đề xuất]**.
  2. Vị trí glyph lệch ≤ 0,01 mm so với RGN-005; ca vàng căn lề, width factor, oblique của RGN-007 đúng 100%.
  3. Font 20 MB dùng 100 glyph nhúng ≤ 40 KB **[đề xuất]**; mỗi font nhúng một lần trong tệp.
  4. Font có `fsType` cấm nhúng → tự chuyển nét, ghi cảnh báo vào PlotReport.
  5. Đạt DoD-C, DoD-H.

### PLT-108 · PDF: chữ SHX dạng nét kèm text ẩn tìm được
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** PLT-107, RGN-004, RGN-007, RGN-009, VN-004
- **Đầu vào:** 03 §13 (PDF có text tìm được); 07.2 UI-H05 (chữ SHX tìm được); nét glyph SHX (RGN-004); vị trí ký tự (RGN-007, RGN-009); chuỗi TCVN3/VNI đã giải mã (VN-004).
- **Đầu ra:** `plot/pdf/shx_text.h`: nét SHX vẽ như hình; lớp text ẩn (render mode 3) dùng font không nét kèm ToUnicode **[đề xuất]**, mỗi ký tự đặt theo ô glyph (vị trí, chiều cao, góc, width factor, oblique); áp cho TEXT, MTEXT, ATTRIB, chữ kích thước, chữ trong block.
- **Tiêu chí nghiệm thu:**
  1. 100% chữ SHX của bộ 200 tờ (QA-001) tìm được bằng chuỗi Unicode đúng (trích bằng pdf.js), kể cả chữ TCVN3/VNI.
  2. Hộp chọn từng ký tự so với hộp bao nét glyph: IoU ≥ 0,8 **[đề xuất]**.
  3. Bật/tắt tùy chọn không đổi phần nhìn thấy của content stream (so từng byte); text ẩn chỉ dùng render mode 3.
  4. Dung lượng tăng ≤ 10% trên bộ 200 tờ **[đề xuất]**.
  5. Đạt DoD-C.

### PLT-109 · PDF: ảnh raster và gradient
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLT-106, RGN-012, RGN-021, NT-005, NT-014
- **Đầu vào:** ISO 32000-1 (Image XObject, SMask, DCTDecode, FlateDecode có predictor); ảnh đã giải mã và chỉnh (RGN-021); hatch gradient (RGN-012); 07.2 UI-H05 (DPI gradient, preset).
- **Đầu ra:** `plot/pdf/image_writer.h`: JPEG nguồn không biến đổi nhúng nguyên byte; ảnh khác nén Flate có predictor; alpha → SMask; clip ảnh (kể cả clip đảo); giảm mẫu theo DPI tối đa của preset; khử trùng ảnh theo băm nội dung; gradient raster hóa theo DPI gradient, clip theo biên hatch; xử lý song song theo ảnh.
- **Tiêu chí nghiệm thu:**
  1. JPEG nguồn: stream trong PDF trùng từng byte tệp gốc.
  2. Ảnh không mất dữ liệu: giải mã lại trùng từng pixel ảnh sau chỉnh của RGN-021.
  3. Ảnh dùng ở nhiều tờ chỉ nhúng một lần.
  4. 50 ca gradient: ảnh nhúng trùng từng pixel ảnh gradient của RGN-012 ở cùng DPI, clip đúng biên hatch.
  5. Bộ nhớ đỉnh ≤ 2 × ảnh lớn nhất + 64 MB **[đề xuất]**.
  6. Đạt DoD-C, DoD-H.

### PLT-004 · Bộ xuất PDF vector: tích hợp, preset, nghiệm thu
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLT-105, PLT-106, PLT-107, PLT-108, PLT-109, NT-010, NT-013, QA-004, QA-005, QA-010
- **Màn hình:** UI-H05
- **Đầu vào:** 03 §13, §24; 05 §1.1, §1.2; 06 §3 (KPI G1); 07.2 UI-H05; kho QA-001, PDF đối chứng QA-004.
- **Đầu ra:** `plot/pdf/pdf_exporter.h`: `exportPdf(sheets, PdfOptions, sink)`; `PdfOptions` với 3 preset (Chất lượng cao, Dung lượng nhỏ, Xem trên web/điện thoại) và Tùy chỉnh, lưu ở NT-010; chế độ màu (theo plot style, màu, xám, đen trắng); một tệp nhiều trang hoặc mỗi tờ một tệp; ước tính dung lượng; `plot/bench/pdf_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. SSIM ≥ 0,97 trên ≥ 95% tờ của kho 10.000 tệp so với PDF do DWG TrueView in (QA-005; cổng G1, 06 §3); kết quả lên QA-010.
  2. Dung lượng PDF trung vị ≤ 100% PDF do DWG TrueView in cùng tờ trên kho 1.000 tệp (QA-004) **[đề xuất]**.
  3. Tờ A1 1 triệu entity (BENCH-STD) ≤ 3 s; 100 tờ của tệp 20 MB ≤ 20 s trên máy tầm trung **[đề xuất]**; hiệu suất song song ≥ 70% đến 16 luồng (05 §1.1).
  4. Preset cho đúng tham số trong đặc tả; Dung lượng nhỏ ≤ 50% Chất lượng cao (trung vị, kho 1.000 tệp) **[đề xuất]**; ước tính dung lượng lệch ≤ 25% **[đề xuất]**.
  5. PDFium và pdf.js mở 100% tệp xuất từ kho 10.000 tệp không lỗi (job đêm).
  6. Đạt DoD-C, DoD-H.

### PLT-110 · Lệnh EXPORTPDF
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** PLT-004, INT-002, INT-003, INT-005, INT-020
- **Lệnh:** EXPORTPDF
- **Màn hình:** UI-H05
- **Đầu vào:** 07.1 §7.8 (PDF: layout hiện tại, tất cả layout, vùng đang xem, cửa sổ); 07.2 UI-H05; sysvar FILEDIA.
- **Đầu ra:** `plot/commands/exportpdf.cpp`: 4 vùng xuất, chọn cửa sổ bằng 2 điểm, tên và thư mục tệp, mở PDF khi xong; chạy nền qua NT-013; `docs/specs/commands/EXPORTPDF.md`.
- **Tiêu chí nghiệm thu:**
  1. 4 vùng xuất cho đúng số trang và vùng in trên 40 ca vàng.
  2. Cùng thiết lập → tệp trùng từng byte với gọi thẳng `exportPdf` (PLT-004).
  3. FILEDIA = 0 hỏi đường dẫn trên dòng lệnh; bàn phím, `.scr`, `(command "EXPORTPDF" ...)` cho cùng tệp.
  4. Phần chạy trên luồng UI ≤ 2 ms; hủy không để lại tệp dở; Viewer dùng được (INT-020).
  5. Đạt DoD-C, DoD-L.

## 4. Máy in, xem trước, xuất ảnh, lệnh in

### PLT-005 · In qua QPrinter và bộ vẽ raster dùng chung
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** PLT-001, PLT-003, NT-005, NT-013, QA-005
- **Màn hình:** UI-H02
- **Đầu vào:** 07.2 UI-H02 (Máy in, Khổ giấy, Số bản, Hướng giấy); Qt 6 `QPrinter`, `QPrinterInfo`, `QPageSize`; `PlotStream` (PLT-003).
- **Đầu ra:**
  - `plot/device/system_printer.h`: liệt kê máy in và khổ của driver (chạy nền, có cache) vào danh mục PLT-001; vùng in được, số bản, hướng, in ngược.
  - `plot/raster/qpainter_renderer.h`: vẽ `PlotStream` lên `QPaintDevice`: vector cho máy in; raster theo dải song song khi cần (độ trong suốt, driver không hỗ trợ). PLT-007 dùng lại.
- **Tiêu chí nghiệm thu:**
  1. In ra máy in PDF ảo của hệ điều hành (Microsoft Print to PDF, CUPS-PDF, PDF của macOS) so với raster của chính bộ vẽ ở cùng DPI: SSIM ≥ 0,98 trên 100 tờ (QA-005) **[đề xuất]**.
  2. Raster theo dải trùng từng bit với 1–32 luồng; giữa 3 hệ điều hành SSIM ≥ 0,999 **[đề xuất]**.
  3. Liệt kê máy in không chặn luồng UI; driver lỗi hoặc treo → báo lỗi trong ≤ 5 s **[đề xuất]**, ứng dụng không treo.
  4. Tờ A1 raster 300 DPI ≤ 3 s trên máy tầm trung **[đề xuất]**; hiệu suất song song ≥ 60% đến 8 luồng.
  5. Đạt DoD-C, DoD-H.

### PLT-006 · Engine xem trước bản in
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLT-003, PLT-004, RGN-001, RND-004, RND-014, QA-005
- **Màn hình:** UI-H02, UI-H03
- **Đầu vào:** 07.2 UI-H02 (xem trước nhỏ), UI-H03; `PlotStream`; mô hình primitive RGN-001; paper space trên RND-014.
- **Đầu ra:** `plot/preview/plot_preview.h`: `PlotStream` → cảnh primitive cho RND (giấy, vùng in được, độ dày nét theo mm giấy quy ra pixel theo zoom); xem trước nhỏ (giấy, vùng in được, khung vùng in); thông tin trang; dựng tăng dần, hủy khi đổi thiết lập.
- **Tiêu chí nghiệm thu:**
  1. Xem trước nhỏ cập nhật ≤ 16 ms sau mỗi lần đổi thiết lập.
  2. Khung đầu có nội dung ≤ 1 s với tờ thường, ≤ 3 s với tờ 1 triệu entity (BENCH-STD) **[đề xuất]**; pan/zoom theo ngân sách DoD-L.
  3. Ảnh xem trước so với raster PDF cùng tờ (QA-005): SSIM ≥ 0,98 **[đề xuất]**; độ dày nét đúng ±0,5 px ở mọi mức zoom.
  4. Đổi thiết lập giữa chừng hủy lần dựng cũ ≤ 50 ms.
  5. Đạt DoD-C, DoD-L.

### PLT-007 · Xuất ảnh PNGOUT, JPGOUT, BMPOUT, TIFOUT
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** PLT-005, PLT-101, PLT-004, QA-005, INT-002, INT-003, INT-005, INT-008, INT-020
- **Lệnh:** PNGOUT, JPGOUT, BMPOUT, TIFOUT
- **Màn hình:** UI-H09
- **Đầu vào:** 03 §13, §24; 07.2 UI-H09; sysvar FILEDIA; `QImageWriter` (PNG, JPEG, TIFF, BMP).
- **Đầu ra:** `plot/raster/image_export.h`: vùng (màn hình, toàn bộ, cửa sổ, layout), DPI hoặc kích thước pixel, nền (trong suốt với PNG, trắng, màu nền vùng vẽ), độ dày nét, khử răng cưa; vẽ theo ô song song bằng bộ vẽ PLT-005; ghi DPI vào metadata; `plot/commands/imageout.cpp`; `docs/specs/commands/PNGOUT.md`, `JPGOUT.md`, `BMPOUT.md`, `TIFOUT.md`.
- **Tiêu chí nghiệm thu:**
  1. Kích thước ảnh đúng từng pixel theo vùng × DPI hoặc kích thước đặt; PNG nền trong suốt có alpha = 0 đúng vùng nền.
  2. So với raster PDF cùng vùng: SSIM ≥ 0,99 **[đề xuất]**.
  3. Ảnh A1 300 DPI ≤ 3 s trên máy tầm trung **[đề xuất]**; tối đa 16.384 × 16.384 px, bộ nhớ ≤ 1,2 GB **[đề xuất]**.
  4. Lời nhắc chọn đối tượng như AutoCAD; FILEDIA = 0; bàn phím, `.scr`, `(command ...)` cho cùng tệp.
  5. Đạt DoD-C, DoD-H.

### PLT-111 · Lệnh PLOT và PREVIEW
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLT-001, PLT-004, PLT-005, PLT-006, PLT-007, INT-002, INT-005, INT-020, NT-013
- **Lệnh:** PLOT, PREVIEW
- **Màn hình:** UI-H02, UI-H03
- **Đầu vào:** 03 §13; 07.1 §7.8; 07.2 UI-H02, UI-H03; sysvar BACKGROUNDPLOT, PLOTROTMODE.
- **Đầu ra:** `plot/commands/plot.cpp`, `preview.cpp`: PLOT layout hoặc Model hiện hành theo page setup cùng ghi đè từ hộp thoại; thiết bị PDF của TrionCAD (PLT-004), PNG (PLT-007), máy in hệ thống (PLT-005); số bản 1–999; Áp dụng cho layout; in nền theo BACKGROUNDPLOT; nhật ký in CSV **[đề xuất]**; PREVIEW theo thiết lập hiện hành, nút In trong xem trước; `docs/specs/commands/PLOT.md`, `PREVIEW.md`.
- **Tiêu chí nghiệm thu:**
  1. PLOT ra PDF trùng từng byte `exportPdf` (PLT-004), ra PNG trùng PLT-007 vùng Layout, cùng thiết lập.
  2. 100 ca vàng trường UI-H02 (vùng in, tỉ lệ, offset, xoay, số bản, plot style, tùy chọn) cho đúng PlotStream kỳ vọng.
  3. In nền: phần đồng bộ trên luồng UI ≤ 2 ms, hủy ≤ 50 ms; BACKGROUNDPLOT tắt thì chạy tiền cảnh có tiến trình.
  4. Áp dụng cho layout ghi LAYOUT qua DOC-007; Viewer dùng được (INT-020).
  5. Đạt DoD-C, DoD-L.

## 5. In hàng loạt: PUBLISH, TCBATCHPLOT, dấu in

### PLT-112 · Danh sách tờ, tệp `.dsd`, mẫu tên tệp
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** PLT-001, DOC-012, NT-011, NT-012, QA-006
- **Màn hình:** UI-H04
- **Đầu vào:** 07.2 UI-H04 (lưu/nạp `.dsd`, mẫu đặt tên tệp), UI-H06 bước 3 (`{SoHieu}_{TenBanVe}.pdf`); tệp `.dsd` mẫu do doanh nghiệp pilot cung cấp.
- **Đầu ra:** `plot/batch/sheet_list.h` (tờ: tệp, layout hoặc khung, page setup, trạng thái); `plot/batch/dsd.h` đọc/ghi `.dsd` (đường dẫn tương đối và tuyệt đối); `plot/batch/name_template.h`: token tên bản vẽ, layout, page setup, ngày, STT, attribute khung tên; bỏ dấu (đ → d); thay ký tự cấm; trùng tên thêm `_2`, `_3` tất định; `docs/specs/formats/dsd.md`; fuzz target.
- **Tiêu chí nghiệm thu:**
  1. Đọc 100% bộ 50 tệp `.dsd` mẫu **[đề xuất]**; đọc → ghi → đọc trùng 100% trường.
  2. 1.000 ca vàng mẫu tên (tiếng Việt có dấu, ký tự cấm của Windows, tên trùng, attribute thiếu) đúng 100%.
  3. Fuzz `.dsd` ≥ 1 giờ không lỗi.
  4. Đạt DoD-C.

### PLT-008 · PUBLISH: in nhiều layout, nhiều bản vẽ
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** PLT-112, PLT-004, PLT-005, PLT-007, DOC-016, DOC-018, NT-013, INT-020
- **Lệnh:** PUBLISH
- **Màn hình:** UI-H04
- **Đầu vào:** 03 §13; 06 §4 (Viewer Pro); 07.2 UI-H04; sysvar BACKGROUNDPLOT.
- **Đầu ra:** `plot/batch/publish_job.h`: chạy danh sách tờ từ bản vẽ đang mở hoặc từ tệp (mở chỉ đọc, nạp lười, tối đa 2 tệp cùng lúc **[đề xuất]**); page setup riêng từng tờ; đích máy in, PDF (một tệp nhiều trang hoặc mỗi tờ một tệp), PNG; số bản; tiến trình theo tờ, hủy, nhật ký lỗi, chạy tiếp khi một tờ lỗi; `plot/commands/publish.cpp`; `docs/specs/commands/PUBLISH.md`.
- **Tiêu chí nghiệm thu:**
  1. 100 tờ từ 20 tệp (QA-001): mỗi tờ trùng từng byte với xuất riêng tờ đó qua PLT-004; tệp nhiều trang đúng thứ tự, có bookmark theo tờ.
  2. Tệp thiếu hoặc hỏng → ghi nhật ký, các tờ khác vẫn xong.
  3. Bộ nhớ đỉnh ≤ 2 GB với 20 tệp 20 MB **[đề xuất]**; hiệu suất song song ≥ 70% đến 16 luồng.
  4. Hủy ≤ 50 ms; phần đồng bộ trên luồng UI ≤ 2 ms.
  5. Viewer miễn phí bị chặn, Viewer Pro dùng được (INT-020).
  6. Đạt DoD-C, DoD-H.

### PLT-113 · Nhận diện khung tên, suy khổ giấy, sắp thứ tự tờ
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** PLT-001, DOC-009, DOC-012, GEO-002, NT-005
- **Màn hình:** UI-H06
- **Đầu vào:** 03 §13 (in hàng loạt theo khung tên, P1 khác biệt VN); 07.2 UI-H06 bước 1–2; prototype `#UI-H06` (`screens4.js`: `findFrames`, `paperOf`); bộ 300 bản vẽ VN có khung tên gán nhãn, thuộc QA-001 **[đề xuất]**.
- **Đầu ra:** `plot/batch/frame_finder.h`:
  - nhận diện theo tên block (block động quy về tên gốc), theo layer (polyline kín 4 đỉnh vuông góc hoặc INSERT trên layer), theo đối tượng mẫu (cùng block; polyline cùng layer, tỉ lệ cạnh ±1% **[đề xuất]**); đề xuất block khung (có attribute số hiệu/tên bản vẽ, có hình chữ nhật bao ≥ 90% hộp block);
  - phạm vi Model, layout; khung nằm trong khung khác bị loại kèm cảnh báo **[đề xuất]**;
  - khổ = kích thước khung / tỉ lệ chèn, khớp danh mục PLT-001 trong ±2%, không khớp → khổ tùy chỉnh; hướng và góc 0/90/180/270;
  - thứ tự: theo hàng (chồng lấn dọc ≥ 50% chiều cao khung nhỏ hơn là cùng hàng **[đề xuất]**), theo cột, theo attribute (sắp tự nhiên, "KT-2" trước "KT-10"); tên tờ từ attribute.
- **Tiêu chí nghiệm thu:**
  1. Bộ 300 bản vẽ: recall ≥ 99%, precision ≥ 99% với cả 3 cách nhận diện **[đề xuất]**.
  2. Khổ suy đúng ≥ 98%; hướng và góc đúng 100%; khung xoay góc khác → cảnh báo, không in sai.
  3. 3 cách sắp thứ tự đúng 100% ca vàng.
  4. Quét 1 triệu entity ≤ 150 ms trên máy tầm trung **[đề xuất]**; kết quả tất định với mọi số luồng.
  5. Đạt DoD-C, DoD-H.

### PLT-009 · TCBATCHPLOT: in hàng loạt theo khung tên
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** PLT-113, PLT-008, PLT-101, NT-010, INT-020
- **Lệnh:** TCBATCHPLOT
- **Màn hình:** UI-H06
- **Đầu vào:** 06 §4 (Viewer Pro); 07.2 UI-H06 bước 3–4 và Mẫu in; prototype `screens4.js` (`SC.batchPlot`, `bpName`).
- **Đầu ra:** `plot/batch/batch_plot.h`: khung → danh sách tờ (vùng Cửa sổ = hộp khung, xoay, khổ, tỉ lệ hoặc vừa khổ, offset/canh giữa, CTB, độ dày nét) → chạy bằng PLT-008; PDF một tệp nhiều trang hoặc mỗi khung một tệp, tên theo mẫu (PLT-112); tạm dừng, tiếp tục, hủy; báo cáo (thành công, lỗi, font thay thế từ PlotReport); Mẫu in lưu ở NT-010, xuất/nhập JSON; nhiều tệp (engine có từ VP, UI mở ở 1.0); `plot/commands/tcbatchplot.cpp`; `docs/specs/commands/TCBATCHPLOT.md`.
- **Tiêu chí nghiệm thu:**
  1. 50 bản vẽ VN (10–60 khung mỗi bản): mỗi trang trùng từng byte với xuất riêng vùng Cửa sổ của khung đó qua PLT-004.
  2. 50 khung A3 ra PDF ≤ 15 s trên máy tầm trung **[đề xuất]**.
  3. Tạm dừng/tiếp tục giữ thứ tự; hủy ≤ 50 ms; báo cáo đủ 3 loại.
  4. Nhiều tệp: thứ tự theo tệp rồi theo khung; tệp lỗi không dừng cả bộ.
  5. Gọi từ `.scr`/`(command ...)` với tên Mẫu in và thư mục đích cho cùng kết quả **[đề xuất]**; Viewer miễn phí bị chặn (INT-020).
  6. Đạt DoD-C, DoD-H.

### PLT-114 · Lệnh dòng lệnh -PLOT, -PUBLISH cho script và LISP
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLT-111, PLT-008, INT-003, INT-007, LSP-006, QA-007, QA-008
- **Lệnh:** -PLOT, -PUBLISH
- **Đầu vào:** chuỗi prompt của AutoCAD (bản ghi dòng lệnh trong QA-007); các LISP in ấn trong bộ QA-008.
- **Đầu ra:** `plot/commands/plot_cli.cpp`: -PLOT đủ prompt (cấu hình chi tiết, layout, thiết bị, khổ, đơn vị, hướng, in ngược, vùng in, tỉ lệ, offset, plot style, độ dày nét, paper space trước, ẩn đối tượng paper space, ghi tệp, lưu thay đổi, tiến hành), `?` để liệt kê; PLOT gọi từ `.scr`/`(command)` chạy dạng dòng lệnh; -PUBLISH nhận `.dsd`; bổ sung `docs/specs/commands/PLOT.md`, `PUBLISH.md`.
- **Tiêu chí nghiệm thu:**
  1. Trình tự prompt, từ khóa, mặc định khớp 100% bản ghi AutoCAD (QA-007); trước khi có đáp án dùng ca vàng tự soạn.
  2. Kết quả trùng từng byte với PLOT/PUBLISH qua hộp thoại cùng thiết lập.
  3. 100% LISP in ấn trong QA-008 chạy đúng, không phải sửa.
  4. Đạt DoD-C.

### PLT-115 · PLOTSTAMP: dấu in
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** PLT-003, PLT-111, RGN-005, NT-010
- **Lệnh:** PLOTSTAMP
- **Đầu vào:** 03 §13 (P2); 07.2 UI-H02 (Dấu in).
- **Đầu ra:** `plot/pipeline/plot_stamp.h`: trường (tên bản vẽ, layout, ngày giờ, người in, máy in, khổ giấy, tỉ lệ) và trường tự đặt; 4 góc, ngang hoặc dọc, offset theo vùng in được hoặc mép giấy; font, cỡ chữ, đơn vị; thêm vào PlotStream sau cùng; tệp thiết lập JSON **[đề xuất]**; dùng trong PLOT, PUBLISH, TCBATCHPLOT; `plot/commands/plotstamp.cpp`; `docs/specs/commands/PLOTSTAMP.md`.
- **Tiêu chí nghiệm thu:**
  1. Giá trị trường đúng 100%; vị trí 4 góc × 2 hướng × 3 khổ lệch ≤ 0,1 mm.
  2. Bật dấu in không đổi phần còn lại của content stream (so từng byte).
  3. Dấu in là text tìm được trong PDF.
  4. Đạt DoD-C.

## 6. Cấu hình máy in PC3, PMP

### PLT-010 · Đọc PC3, PMP
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** PLT-102, PLT-001, PLT-005, NT-010, QA-006
- **Đầu vào:** 03 §13 (PLOTTERMANAGER: P1 đọc); 07.2 UI-H10 (trước Sau chỉ đọc khổ từ PC3); tệp PC3/PMP do doanh nghiệp pilot cung cấp.
- **Đầu ra:** `plot/device/pc3.h`: đọc PC3 (driver, máy in hệ thống đích, khổ, vùng in được) và PMP (khổ tùy chỉnh, hiệu chỉnh tỉ lệ) trên vỏ PIA (PLT-102); tìm theo đường dẫn cấu hình máy in (NT-010); ánh xạ vào danh mục PLT-001 và máy in PLT-005; `docs/specs/formats/pc3-pmp.md`.
- **Tiêu chí nghiệm thu:**
  1. Đọc 100% bộ 200 tệp PC3/PMP mẫu **[đề xuất]**; khổ tùy chỉnh và vùng in được khớp ca vàng ≤ 0,01 mm.
  2. Layout tham chiếu PC3 có trong đường dẫn: khổ phân giải đúng 100% (QA-001); thiếu PC3 → dùng tên khổ chuẩn (PLT-001) và cảnh báo.
  3. Fuzz ≥ 1 giờ không lỗi; đọc ≤ 2 ms/tệp **[đề xuất]**.
  4. Đạt DoD-C.

### PLT-116 · PLOTTERMANAGER: trình sửa PC3, PMP
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** PLT-010, NT-011, INT-001, QA-004
- **Lệnh:** PLOTTERMANAGER
- **Màn hình:** UI-H10
- **Đầu vào:** 03 §13 (P2 soạn); 07.2 UI-H10; prototype `#UI-H10` (`screens17.js`, `SC.pc3`).
- **Đầu ra:** `plot/device/pc3_editor.h`: ghi PC3/PMP; thêm/sửa/xóa khổ tùy chỉnh và vùng in được; hiệu chỉnh tỉ lệ (PMP); cổng; Lưu thành; `plot/commands/plottermanager.cpp`; `docs/specs/commands/PLOTTERMANAGER.md`.
- **Tiêu chí nghiệm thu:**
  1. Ghi → đọc trùng 100% trường trên bộ mẫu của PLT-010.
  2. DWG TrueView dùng PC3/PMP do TrionCAD ghi để in ra đúng khổ tùy chỉnh (QA-004).
  3. Ghi nguyên tử; lỗi giữa chừng không hỏng tệp cũ.
  4. Đạt DoD-C.

## 7. Nhập PDF

### PLT-011 · Lớp bọc PDFium
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** NT-001, NT-005, NT-011, QA-006, PLT-004
- **Đầu vào:** 03 §12, §24 (đọc PDF bằng PDFium, BSD); API công khai của PDFium.
- **Đầu ra:** `plot/pdfium/pdf_document.h`: mở tệp hoặc bộ nhớ (mật khẩu qua callback); số trang, kích thước, xoay; render ô RGBA theo vùng và tỉ lệ (cho RGN-022, RND-015); trích path (đã nhân CTM, stroke/fill, độ dày, nét đứt, màu, luật tô, clip), text (Unicode, font, cỡ, ma trận, hộp ký tự), ảnh, marked content `/OC` (layer); mọi lời gọi PDFium qua một hàng đợi tuần tự **[đề xuất]**; giới hạn kích thước ô, số đối tượng, bộ nhớ.
- **Tiêu chí nghiệm thu:**
  1. Round-trip: PDF do PLT-004 ghi → trích lại khớp PlotStream gốc: 100% primitive, tọa độ ≤ 0,001 mm, 100% chuỗi text.
  2. Render ô 1.024 × 1.024 trang A1 vector ≤ 60 ms trên máy tầm trung **[đề xuất]**.
  3. Tệp hỏng, có mật khẩu, vượt giới hạn → lỗi rõ ràng; fuzz ≥ 1 giờ không lỗi; TSan sạch khi gọi từ nhiều luồng.
  4. Đạt DoD-C.

### PLT-117 · Chuyển path PDF thành hình học bản vẽ
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** PLT-011, PLT-004, GEO-006, GEO-007, GEO-014, DOC-007, NT-005
- **Màn hình:** UI-G14
- **Đầu vào:** 07.2 UI-G14 (Dữ liệu nhập, Tùy chọn); path từ PLT-011; tệp `.lin` đang nạp.
- **Đầu ra:** `plot/pdfimport/path_converter.h`: đoạn thẳng → LINE hoặc LWPOLYLINE; Bézier khớp cung, tròn, ellipse trong dung sai → ARC, CIRCLE, ELLIPSE, còn lại → SPLINE chính xác (NURBS bậc 3); `re` → polyline kín; vùng tô đặc → HATCH SOLID (luật nonzero có vòng lồng cùng chiều xử lý qua GEO-014); RGB → true color (ACI khi trùng); độ dày → lineweight chuẩn gần nhất; nối đoạn và cung có đầu mút trùng; suy linetype từ nét đứt thẳng hàng; xử lý song song theo lô.
- **Tiêu chí nghiệm thu:**
  1. Round-trip DWG → PDF (PLT-004) → hình học: ≥ 99% LINE/ARC/CIRCLE về đúng loại, sai số ≤ 1e-6 × cạnh trang **[đề xuất]**.
  2. Diện tích HATCH lệch ≤ 1e-6 tương đối; island đúng 100% ca vàng.
  3. Suy linetype đúng ≥ 95% ca vàng **[đề xuất]**; không nối nhầm đoạn khác thuộc tính.
  4. Trang 1 triệu đoạn ≤ 5 s trên máy tầm trung **[đề xuất]**; hiệu suất song song ≥ 60% đến 8 luồng; tất định.
  5. Đạt DoD-C, DoD-H.

### PLT-118 · PDFSHXTEXT: nhận dạng chữ SHX
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** PLT-117, PLT-004, RGN-004, VN-001, VN-007, GEO-017, INT-008, DOC-007
- **Lệnh:** PDFSHXTEXT
- **Màn hình:** UI-G14
- **Đầu vào:** 03 §12 (P1, giá trị cao ở VN); 07.2 UI-G14 (Nhận dạng chữ SHX); nét glyph SHX (RGN-004); bộ font SHX tiếng Việt (VN-007).
- **Đầu ra:** `plot/pdfimport/shx_recognizer.h`: gom nét thành cụm ký tự theo đường chân chữ, chuẩn hóa, so với mẫu glyph của font chọn ở nhiều chiều cao, góc, width factor; điểm tin cậy và ngưỡng; ghép từ, dòng → TEXT (style, chiều cao, góc, width factor, oblique), tùy chọn gộp thành MTEXT; mã hóa chuỗi theo bảng mã của font (VN-001); báo cáo; `plot/commands/pdfshxtext.cpp`; `docs/specs/commands/PDFSHXTEXT.md`.
- **Tiêu chí nghiệm thu:**
  1. Round-trip (DWG có chữ SHX → PDF không có text ẩn → hình học qua PLT-117): ≥ 98% ký tự đúng với txt, simplex, romans, isocp; ≥ 95% với font SHX tiếng Việt **[đề xuất]**.
  2. Nhận nhầm ≤ 0,5% trên bản vẽ có hatch và kích thước dày **[đề xuất]**.
  3. 100.000 nét ≤ 2 s trên máy tầm trung **[đề xuất]**; hiệu suất song song ≥ 60% đến 8 luồng; tất định.
  4. Một mark undo; bàn phím, `.scr`, `(command ...)` cho cùng kết quả.
  5. Đạt DoD-C, DoD-H.

### PLT-012 · PDFIMPORT: nhập PDF thành đối tượng
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** PLT-011, PLT-117, PLT-118, INT-002, INT-003, INT-005, DOC-008
- **Lệnh:** PDFIMPORT
- **Màn hình:** UI-G14
- **Đầu vào:** 03 §12, §24; 07.2 UI-G14; sysvar PDFIMPORTMODE, PDFIMPORTLAYERS, PDFIMPORTFILTER, PDFIMPORTIMAGEPATH.
- **Đầu ra:** `plot/commands/pdfimport.cpp`: nguồn là tệp hoặc PDF underlay đang chọn; trang; vị trí, tỉ lệ, góc; dữ liệu nhập (vector, tô đặc, chữ TrueType → TEXT với style theo tên font, ảnh → tệp PNG + IMAGE); layer (theo PDF, theo loại đối tượng, hiện hành); nhập thành block; gọi PLT-117, PLT-118; báo cáo số đối tượng; `docs/specs/commands/PDFIMPORT.md`.
- **Tiêu chí nghiệm thu:**
  1. Mỗi tùy chọn của UI-G14 có ca vàng, đúng 100%.
  2. Chữ TrueType → TEXT đúng chuỗi NFC, vị trí lệch ≤ 0,01 mm × tỉ lệ; ảnh trích ra trùng từng pixel.
  3. Một mark undo, undo khôi phục 100% (so DOC-017); bàn phím, `.scr`, `(command ...)` cho cùng kết quả.
  4. Báo cáo đếm đúng số đối tượng tạo ra và số chữ nhận dạng.
  5. Đạt DoD-C.

## 8. Sheet Set

### PLT-119 · Tệp `.dst` và mô hình bộ bản vẽ
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** NT-011, NT-012, QA-006, QA-007
- **Màn hình:** UI-H08
- **Đầu vào:** 03 §14 (Sheet Set, P2, đọc/ghi `.dst`); 07.2 UI-H08; tệp `.dst` do AutoCAD tạo, nhận qua QA-007.
- **Đầu ra:** `plot/sheetset/sheet_set.h`: bộ (tên, mô tả, thuộc tính tùy biến), subset lồng nhau, tờ (DWG, layout, số hiệu, tiêu đề, cờ publish), sheet view, DWT cho tờ mới, DWT ghi đè page setup, tài nguyên; `plot/sheetset/dst_io.h` đọc/ghi; `docs/specs/formats/dst.md`; fuzz target.
- **Tiêu chí nghiệm thu:**
  1. Đọc 100% bộ 30 tệp `.dst` mẫu **[đề xuất]**; đọc → ghi → đọc trùng 100% mô hình.
  2. Doanh nghiệp pilot mở tệp do TrionCAD ghi trong AutoCAD không lỗi (QA-007).
  3. Fuzz ≥ 1 giờ không lỗi.
  4. Đạt DoD-C.

### PLT-120 · SHEETSET: thao tác bộ bản vẽ
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** PLT-119, PLT-008, DOC-014, RGN-025, NT-013, NT-015
- **Lệnh:** SHEETSET
- **Màn hình:** UI-H08
- **Đầu vào:** 03 §14; 07.2 UI-H08; prototype `#UI-H08` (`screens17.js`).
- **Đầu ra:** `plot/sheetset/sheet_set_service.h`: mở/tạo bộ; thêm, bớt, sắp tờ và subset; tờ mới từ DWT (DOC-014); thuộc tính bộ và tờ; field sheet set cho khung tên (cấp giá trị cho RGN-025); publish (PLT-008); lưu trữ ZIP (NT-015); `plot/commands/sheetset.cpp`; `docs/specs/commands/SHEETSET.md`.
- **Tiêu chí nghiệm thu:**
  1. Tờ mới từ mẫu: DWG và layout tạo đúng; số hiệu, tiêu đề hiện trong khung tên.
  2. Field sheet set đánh giá đúng 100% ca vàng.
  3. Publish bộ: danh sách tờ đúng thứ tự subset, bỏ tờ không đánh dấu publish.
  4. Gói lưu trữ chứa đủ `.dst`, DWG, DWT, xref, ảnh, font tham chiếu; giải nén ra mở lại được bộ.
  5. Đạt DoD-C.

## 9. Cổng chất lượng

### PLT-121 · Cổng chất lượng in v1.0
- **Mốc:** 1.0 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** PLT-004, QA-005, QA-010, RGN-010, RGN-022, RGN-025
- **Đầu vào:** 05 §1.2 (SSIM ≥ 0,98 trên ≥ 97% tờ); kho 10.000 tệp (QA-001); PDF đối chứng (QA-004).
- **Đầu ra:** `plot/tests/quality/`: job đêm in toàn kho và so QA-005; phân loại tờ dưới ngưỡng theo nguyên nhân (font, hatch, linetype, ảnh, viewport, field, MTEXT); bảng xu hướng trên QA-010.
- **Tiêu chí nghiệm thu:**
  1. SSIM ≥ 0,98 trên ≥ 97% tờ của kho 10.000 tệp (05 §1.2).
  2. 100% tờ dưới ngưỡng có nhãn nguyên nhân; lỗi thuộc module khác được mở issue gắn module.
  3. Job chạy ≤ 4 giờ trên runner tự host **[đề xuất]**; dưới ngưỡng thì chặn phát hành 1.0.
  4. Đạt DoD-C.

## Tóm tắt theo mốc

| Mốc | Task | Số task |
|---|---|---|
| V | PLT-001, PLT-101, PLT-102, PLT-002, PLT-103, PLT-003, PLT-105, PLT-106, PLT-107, PLT-108, PLT-109, PLT-004, PLT-110, PLT-005, PLT-006, PLT-007, PLT-111 | 17 |
| VP | PLT-112, PLT-008, PLT-113, PLT-009 | 4 |
| EA | PLT-104, PLT-114 | 2 |
| 1.0 | PLT-013, PLT-010, PLT-011, PLT-117, PLT-118, PLT-012, PLT-121 | 7 |
| Sau | PLT-115, PLT-116, PLT-119, PLT-120 | 4 |
| **Tổng** | | **34** |
