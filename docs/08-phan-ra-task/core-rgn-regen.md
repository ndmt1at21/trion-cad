# RGN · Regen (sinh hình hiển thị)

> Module regen sinh primitive hiển thị từ entity trên snapshot bất biến, dùng cho renderer (RND), in (PLT), đo và chọn. Tệp chính (§0–§7): primitive và thuộc tính hiển thị, font SHX và TrueType, TEXT, MTEXT, định dạng số, bộ điều phối song song, thứ tự vẽ, ngân sách. Từ §8: linetype, hatch, block và dynamic block, kích thước, leader và bảng, proxy, ảnh, PDF underlay, entity còn lại, viewport và xref, field, chú thích theo tỉ lệ.
> - Thư mục mã: `core/regen/` (`prim/`, `props/`, `curves/`, `font/`, `text/`, `mtext/`, `numfmt/`, `driver/`, `tests/`, `bench/`, `fuzz/`).
> - Căn cứ: [05](../05-kien-truc-ky-thuat.md) §1, §8, §9.1, §13, §21; [03](../03-chuc-nang-autocad.md) §7; [07.2](../07-dac-ta-ui-2-bang-va-hop-thoai.md) UI-D01, UI-F01–F04.

## 0. Ghi chú thiết kế

- **Regen là hàm thuần trên snapshot bất biến** (DOC-009): `regen(entity, RegenContext) → primitive`. Không biến toàn cục, không ghi DB, không I/O trong vòng lặp entity (font, linetype, kiểu đã phân giải sẵn trong `RegenContext`). Nhờ vậy chạy song song tự do, cache được, tất định (DoD-C 4).
- Primitive giữ tọa độ `double` theo WCS; renderer trừ gốc khung nhìn (RND-011). Cung, đường tròn, ellipse xuất dạng giải tích cho RND-005, không tessellate. Chỉ spline và đường cong không giải tích tessellate theo sai số pixel (GEO-016).
- Primitive phụ thuộc view (spline, linetype phức, LOD chữ) gom theo **bậc view**: mỗi bậc gấp đôi tỉ lệ **[đề xuất]**. Pan/zoom trong cùng bậc không regen lại.
- Kết quả song song hợp nhất theo khóa cố định (không gian, thứ tự vẽ, chỉ số entity), không theo thứ tự luồng xong. Chạy 1→32 luồng cho cùng hash.
- Bộ nhớ: arena theo lượt regen, cache theo entity sống theo document (NT-007). Không cấp phát trong vòng lặp entity (NT-004 đếm).
- Tệp SHX, TTF, chuỗi MTEXT, mã `%%` là đầu vào không tin cậy: kiểm biên, giới hạn đệ quy, fuzz ≥ 1 giờ (DoD-C 7). SHX viết theo tài liệu công khai của AutoCAD.
- RGN-013 là **nơi duy nhất** định dạng số, đơn vị (README §4.2). Kích thước, `rtos`/`angtos`, DYN, thanh trạng thái, Properties đều gọi thư viện này. Chiều chuỗi → số thuộc INT-105; RGN-013 không phụ thuộc task INT nào.
- Bác sĩ Font chỉ cắm móc vào FontResolver (RGN-006): thay font và giải mã khi vẽ, không đổi dữ liệu bản vẽ (05 §13).
- Đối chiếu với AutoCAD qua: bộ tham chiếu ngắt dòng MTEXT QA-109 và chuỗi kích thước, định dạng số QA-110 (đều ở mốc V); giá trị AutoCAD tính sẵn trong tệp do AutoCAD lưu (điểm căn của TEXT, kích thước thực của MTEXT, chữ trong block `*D`); so vector với PDF DWG TrueView (QA-005). Ca vàng soát một lần với DWG TrueView rồi kiểm tự động.
- Font dùng trong CI: font OFL (Noto, DejaVu, IBM Plex) và font hệ thống của runner. Font bên thứ ba chỉ lấy từ kho QA-001 có thỏa thuận sử dụng, không đóng gói (05 §13).

## 1. Primitive hiển thị và thuộc tính

### RGN-001 · Mô hình primitive, khóa vật liệu và API regen thuần
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-001, NT-007, DOC-002
- **Đầu vào:** 05 §8, §9.1 (bộ đệm theo vật liệu, instancing, RTC); header entity của DOC-002; arena NT-007.
- **Đầu ra:** `core/regen/prim/` (`Primitive.h`, `MaterialKey.h`, `PrimitiveBuffer`, `RegenContext.h` gồm tham số khung nhìn: cỡ pixel theo đơn vị bản vẽ, bậc view, không gian, chế độ màn hình/in; `Regen.h` với `regen(EntityRef, const RegenContext&, PrimitiveSink&)`); dump chuẩn kèm hash cho CI; `core/regen/tests/prim_*`, `core/regen/bench/bench_prim.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Đủ loại primitive: đoạn/polyline (SoA), cung/tròn/ellipse giải tích, vùng tô nhiều contour, glyph run TTF, nét SHX, ảnh, tham chiếu instance (định nghĩa + ma trận). Mỗi entity có dải primitive và hộp bao hiển thị.
  2. `MaterialKey` (màu, linetype, lineweight, transparency, cờ layer) so sánh và băm theo giá trị, cùng hash trên 3 hệ điều hành.
  3. Đoạn thẳng ≤ 40 byte, cung ≤ 64 byte kể cả chỉ số entity và vật liệu **[đề xuất]**. RND-003 đọc bộ đệm qua `span`, không sao chép.
  4. Hàm thuần: 32 luồng regen cùng tập entity cho dump trùng từng byte, TSan sạch; 0 cấp phát/entity khi sink đã reserve (NT-004).
  5. Đạt DoD-C, DoD-H.

### RGN-101 · Regen đường cong cơ bản: LINE, ARC, CIRCLE, ELLIPSE, LWPOLYLINE
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-001, GEO-005, GEO-006, NT-005, NT-006, QA-009, QA-012
- **Đầu vào:** API RGN-001; đường cong GEO-005, polyline bulge GEO-006; cảnh 1 triệu entity QA-012 cho spike S2 (05 §21).
- **Đầu ra:** `core/regen/curves/BasicCurves.cpp`; `regenRange()` song song qua NT-005 (RGN-003 mở rộng); `core/regen/tests/curves_basic_test.cpp`, `core/regen/bench/bench_curves.cpp`.
- **Tiêu chí nghiệm thu:**
  1. LINE, ARC, CIRCLE, ELLIPSE (đủ và cung), LWPOLYLINE (bulge, kín/hở, elevation) ra primitive giải tích, không tessellate. OCS với extrusion bất kỳ, kể cả (0,0,−1) của đối tượng lật, đúng theo thuật toán trục tùy ý.
  2. Điểm đầu/cuối, tâm, bán kính khớp GEO-005/GEO-006 trong `Tolerance` trên 1 triệu ca QA-009 (cung 0°, bulge 1e-12, tọa độ VN-2000 ≈ 10⁹); không NaN, không primitive suy biến.
  3. Cảnh S2 1 triệu entity: ≤ 60 ms với 8 luồng máy tầm trung; hiệu suất ≥ 70% đến 16 luồng **[đề xuất]**.
  4. Biến đổi điểm dùng SIMD NT-006; đường scalar cho cùng bytes.
  5. Đạt DoD-C, DoD-H.

### RGN-102 · Polyline có độ rộng và SPLINE
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-101, GEO-007, GEO-016, QA-005
- **Đầu vào:** NURBS GEO-007; tessellation theo sai số pixel GEO-016; bậc view (§0); so vector với PDF DWG TrueView (QA-005).
- **Đầu ra:** `core/regen/curves/WidePolyline.cpp` (hàm công khai `wideSegmentBand()` cho RGN-023 dùng với POLYLINE 2D, TRACE), `core/regen/curves/SplineRegen.cpp`; kiểm thử, benchmark.
- **Tiêu chí nghiệm thu:**
  1. LWPOLYLINE độ rộng hằng và đổi theo đỉnh, đoạn cung có độ rộng, mối nối giữa hai đoạn rộng như AutoCAD; FILLMODE = 0 chỉ vẽ biên.
  2. SPLINE theo điểm điều khiển, điểm fit, hữu tỉ, tuần hoàn: sai số dây cung ≤ 0,5 pixel ở mọi mức zoom; SPLFRAME hiện khung điều khiển.
  3. Zoom trong cùng bậc view không sinh lại spline; sang bậc mới chỉ sinh lại spline đang thấy.
  4. 200 tệp QA-001 có polyline rộng và spline: biên vùng tô và nét lệch ≤ 0,05 mm ở tỉ lệ in 1:1 so với PDF DWG TrueView **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

### RGN-002 · Phân giải thuộc tính hiển thị
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** RGN-001, DOC-004, DOC-005, DOC-010, DOC-011, DOC-012
- **Đầu vào:** bảng LAYER, LTYPE (DOC-004); block lồng (DOC-012); viewport (DOC-011); ghi đè layer theo viewport lưu trong extension dictionary (DOC-005); biến header LWDEFAULT, LWDISPLAY, TRANSPARENCYDISPLAY (DOC-010).
- **Đầu ra:** `core/regen/props/` (`DisplayProps.h`, `AciPalette.h`, `resolveMaterial()`, memo theo cặp (layer, chuỗi insert)); `core/regen/tests/props_*`, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Màu ACI 1–255, ByLayer, ByBlock, true color, color book (dùng RGB lưu kèm); layer tắt (màu âm), đóng băng, khóa (cờ cho RND làm mờ); ACI 7 gắn cờ đảo theo nền.
  2. ByLayer/ByBlock qua block lồng ≤ 32 mức **[đề xuất]**, quy tắc layer 0 trong block, ByBlock ở cấp ngoài cùng, layer phụ thuộc xref; linetype ByLayer/ByBlock/CONTINUOUS; lineweight −1/−2/−3 theo LWDEFAULT; transparency ByLayer/ByBlock/0–90%.
  3. Ghi đè layer theo viewport (P1): màu, linetype, lineweight, transparency chỉ áp trong viewport đó.
  4. Bảng ca vàng ≥ 600 tổ hợp khớp 100% **[đề xuất]**.
  5. ≤ 25 ns/entity đơn luồng nhờ memo, 0 cấp phát/entity **[đề xuất]**.
  6. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** VP Freeze và làm mờ xref (RGN-024); sinh nét linetype (RGN-011).

## 2. Font SHX

### RGN-103 · Parser shape font SHX
- **Mốc:** S6 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-001, QA-001, QA-006
- **Đầu vào:** tài liệu AutoCAD Customization Guide, phần mô tả shape và font shape (SHP/SHX); bộ SHX trong kho QA-001; 05 §8.
- **Đầu ra:** `core/regen/font/shx/ShxShapeFile.{h,cpp}` (header, chỉ mục, font info, giải mã glyph lười thành nét: đoạn + cung bulge, vector tiến); `core/regen/fuzz/fuzz_shx.cpp`; `core/regen/tests/shx_shape_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Đọc header `AutoCAD-86 shapes 1.0` và `1.1`, bảng chỉ mục, font info (above, below, chế độ ngang/dọc).
  2. Thông dịch đủ mã vector 16 hướng và mã đặc biệt 0–14: bút lên/xuống, chia/nhân độ dài, push/pop, subshape, dịch X-Y đơn và chuỗi, cung bát phân, cung phân đoạn, cung bulge đơn và chuỗi, mã chỉ cho text dọc.
  3. Ca vàng ≥ 300 glyph phủ mọi mã lệnh: tọa độ nét khớp giá trị tính tay theo đặc tả, sai số ≤ 1e-12 đơn vị font **[đề xuất]**.
  4. Tệp hỏng (subshape vòng lặp, push/pop tràn, độ dài vượt khai báo) trả lỗi có mã, không crash; fuzz ≥ 1 giờ; 100% shape font của QA-001 đọc không lỗi.
  5. Đọc chỉ mục tệp 1 MB ≤ 2 ms **[đề xuất]**.
  6. Đạt DoD-C.

### RGN-104 · Bigfont SHX
- **Mốc:** S6 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** RGN-103, NT-012
- **Đầu vào:** tài liệu AutoCAD phần big font; code page DBCS 932, 936, 949, 950 (NT-012); bigfont trong QA-001.
- **Đầu ra:** `core/regen/font/shx/ShxBigFont.{h,cpp}`; kiểm thử; mở rộng `fuzz_shx.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Header `AutoCAD-86 bigfont 1.0`, dải lead byte, mã 2 byte; extended big font (subshape mở rộng có gốc và kích thước ô).
  2. Tra glyph theo mã 2 byte; mã ngoài dải trả "không có", không đọc ngoài biên.
  3. 100% bigfont của QA-001 đọc không lỗi; ca vàng 200 ký tự CJK khớp tọa độ tính tay.
  4. Fuzz ≥ 1 giờ.
  5. Đạt DoD-C.

### RGN-105 · Unifont SHX
- **Mốc:** S6 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** RGN-103
- **Đầu vào:** tài liệu AutoCAD phần unicode font; unifont trong QA-001.
- **Đầu ra:** `core/regen/font/shx/ShxUniFont.{h,cpp}`; công cụ `shxcoverage` (báo glyph tiếng Việt có/thiếu); kiểm thử, fuzz.
- **Tiêu chí nghiệm thu:**
  1. Header `AutoCAD-86 unifont 1.0`, font info, mã shape 16-bit theo Unicode, header riêng của từng shape.
  2. Tra glyph trực tiếp theo code point; ca vàng 200 glyph khớp tọa độ tính tay.
  3. `shxcoverage` liệt kê đúng 134 chữ có dấu tiếng Việt có/thiếu trên mọi unifont của QA-001; VN-007 dùng để kiểm bộ font tự thiết kế.
  4. 100% unifont của QA-001 đọc không lỗi; fuzz ≥ 1 giờ.
  5. Đạt DoD-C.

### RGN-004 · Font SHX: shape, bigfont, unifont và cache glyph dạng nét
- **Mốc:** S6 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-001, RGN-103, RGN-104, RGN-105, NT-012
- **Đầu vào:** RGN-103–RGN-105; DWGCODEPAGE và bảng code page (NT-012); 05 §8.
- **Đầu ra:** `core/regen/font/shx/ShxFont.h` (một giao diện cho 3 dạng, ghép font chính + bigfont của STYLE), `ShxGlyphCache`; sinh primitive nét cho RND-007; kiểm thử, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Tra ký tự: unifont theo code point; font không Unicode đổi code point → byte theo DWGCODEPAGE rồi tra shape; byte có lead byte tra bigfont; `%%nnn` tra thẳng mã shape.
  2. `%%d`, `%%p`, `%%c` → shape 127, 128, 129 ở font không Unicode; U+00B0, U+00B1, U+2205 ở unifont. Ký tự thiếu hiển thị như DWG TrueView (ca vàng) và vào báo cáo font (RGN-006).
  3. Cache glyph dùng chung giữa luồng và bản vẽ, đọc không khóa sau khi công bố; trúng cache ≤ 20 ns **[đề xuất]**; 32 luồng TSan sạch.
  4. 100.000 TEXT SHX (BENCH-STD text dày): phần sinh nét ≤ 25 ms với 8 luồng **[đề xuất]**; 0 cấp phát/ký tự khi cache ấm.
  5. Đạt DoD-C, DoD-H.

## 3. Font TrueType và FontResolver

### RGN-106 · Đường viền và metric TrueType/OpenType qua FreeType
- **Mốc:** S6 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-001, NT-011, QA-006
- **Đầu vào:** FreeType (05 §8); tệp font qua mmap (NT-011); font OFL Noto Sans, Noto Serif, DejaVu Sans, IBM Plex Sans.
- **Đầu ra:** `core/regen/font/ttf/FtFace.{h,cpp}`: nạp TTF, OTF (CFF), TTC theo chỉ số face; đường viền bậc 2/bậc 3 theo đơn vị font, không hinting; metric advance, hộp bao, ascender, descender, units per em, cap height, x-height; `flattenOutline()`. Kiểm thử, fuzz, benchmark.
- **Tiêu chí nghiệm thu:**
  1. 100% glyph của 4 font OFL trích đường viền không lỗi; contour kín, chiều đúng quy tắc non-zero.
  2. `flattenOutline()` lệch khỏi đường cong gốc không quá dung sai yêu cầu trên 100% glyph.
  3. Face pool theo luồng, glyph đã trích bất biến và dùng chung; 32 luồng TSan sạch.
  4. Tệp font hỏng hoặc cắt cụt trả lỗi, không crash; fuzz lớp bọc ≥ 1 giờ.
  5. ≥ 200.000 glyph/s đơn luồng máy tầm trung **[đề xuất]**.
  6. Đạt DoD-C, DoD-H.

### RGN-107 · Shaping HarfBuzz và dấu tổ hợp tiếng Việt
- **Mốc:** S6 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-106, NT-012
- **Đầu vào:** HarfBuzz (05 §8); chuẩn hóa NFC/NFD (NT-012); 05 §21 S6.
- **Đầu ra:** `core/regen/font/ttf/Shaper.{h,cpp}`: run UTF-16 → glyph id, advance, offset, ánh xạ cluster ↔ chỉ số ký tự; cache run đã shape. Kiểm thử, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Chuỗi dựng sẵn (NFC) và tổ hợp (NFD, kiểu "Unicode tổ hợp" của bộ gõ) cho cùng glyph và vị trí (± 1/64 đơn vị) trên font có glyph dựng sẵn.
  2. Font thiếu glyph dựng sẵn hoặc thiếu GPOS mark: dấu đặt bằng fallback, hộp bao dấu không chồng chữ cơ sở và không chồng nhau ở cả 60 chữ hai dấu (ă â ê ô ơ ư kèm thanh, hoa và thường).
  3. Ánh xạ cluster đúng 100% trên ca vàng ≥ 200 chuỗi **[đề xuất]** (caret của trình soạn MTEXT dùng).
  4. Kerning, ligature bật/tắt được; mặc định chốt ở RGN-111.
  5. ≥ 2 triệu glyph/s đơn luồng; trúng cache run ≥ 95% trên BENCH-STD text dày **[đề xuất]**.
  6. Đạt DoD-C, DoD-H.

### RGN-005 · TrueType: đường viền, shaping, cache glyph, cấp glyph cho atlas SDF
- **Mốc:** S6 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-001, RGN-106, RGN-107
- **Đầu vào:** RGN-106, RGN-107; nhu cầu atlas SDF và tam giác hóa chữ lớn của RND-007 (05 §8, §9.1).
- **Đầu ra:** `core/regen/font/ttf/TtfFont.h` (facade), `GlyphCache` có ngân sách bộ nhớ, `GlyphProvider` cho RND-007; sinh primitive `GlyphRun` (font, glyph id, vị trí, ma trận gồm hệ số rộng, góc nghiêng, viết ngược, lộn ngược). Kiểm thử, benchmark.
- **Tiêu chí nghiệm thu:**
  1. ASCII và 134 chữ có dấu tiếng Việt trên 4 font OFL và Arial, Times New Roman (runner Windows): không ra `.notdef`, đúng glyph theo bảng kỳ vọng.
  2. `GlyphProvider` trả đường viền theo glyph id: cache miss ≤ 50 µs p99, hit ≤ 100 ns **[đề xuất]**.
  3. Cache glyph mặc định 64 MB, loại bỏ LRU **[đề xuất]**; đổi ngân sách hay tắt cache không đổi bytes primitive.
  4. 32 luồng shape và lấy glyph đồng thời: TSan sạch, kết quả trùng chạy đơn luồng.
  5. Đạt DoD-C, DoD-H.

### RGN-006 · FontResolver: tìm font, thay font thiếu, móc Bác sĩ Font
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-004, RGN-005, NT-005, NT-011, DOC-004, INT-005, QA-001
- **Màn hình:** UI-D01
- **Đầu vào:** bảng STYLE (DOC-004: tệp font, bigfont, họ TTF, cờ đậm/nghiêng); đường dẫn tìm kiếm NT-011; sysvar FONTALT, FONTMAP (INT-005); 05 §13 (chế độ hiển thị của Bác sĩ Font).
- **Đầu ra:** `core/regen/font/FontResolver.{h,cpp}`, `FontReport` (font thiếu, font thay), `DisplayDecodeHook` để VN-004 đăng ký; danh sách font (tên, SHX/TTF, bigfont) cho UI-D01. Kiểm thử, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Thứ tự tìm cố định: thư mục bản vẽ, support path, font đi kèm (VN-007), thư mục font hệ thống; tên không phân biệt hoa thường, có hoặc không đuôi; TTF theo tên tệp hoặc tên họ (bảng `name`).
  2. Font thiếu: FONTMAP (`.fmp`), FONTALT, ánh xạ SHX ↔ TTF; ca vàng ≥ 200 ca khớp 100% **[đề xuất]**. Mở bộ 1.000 tệp QA-001: 100% style có font hiển thị, báo cáo đúng font thiếu.
  3. Móc hiển thị: trả font thay và chuỗi giải mã theo style hoặc entity; hash document không đổi; đổi móc chỉ làm mất hiệu lực cache của entity liên quan.
  4. Quét 1.000 font hệ thống ≤ 300 ms ở luồng nền, có cache trên đĩa; phân giải 1.000 style ≤ 2 ms **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** nhận diện bảng mã và giải mã TCVN3/VNI (VN-002, VN-004).

## 4. TEXT một dòng và spike S6

### RGN-108 · Giải mã nội dung text khi hiển thị: `%%`, `\U+`, `\M+`
- **Mốc:** S6 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** VN-102, NT-012, QA-006
- **Đầu vào:** 03 §7 (ký tự đặc biệt); bộ tách token chuỗi CAD của VN-102 (`vn/encoding/cadtext.h`), không viết bộ tách thứ hai; code page NT-012 cho `\M+`.
- **Đầu ra:** `core/regen/text/TextDecode.{h,cpp}`: token VN-102 → dãy code point, đoạn gạch dưới/gạch trên, mã shape trực tiếp (`%%nnn`); dùng chung cho TEXT, ATTRIB, MTEXT. Kiểm thử, fuzz.
- **Tiêu chí nghiệm thu:**
  1. `%%c %%d %%p %%u %%o %%%` và `%%nnn`; `\U+xxxx`, `\M+nxxxx`; `%%` theo sau ký tự lạ xử lý như DWG TrueView (ca vàng).
  2. Ca vàng ≥ 300 chuỗi, gồm chuỗi hỏng và cắt cụt **[đề xuất]**; chuỗi gốc không bị đổi (round-trip DWG giữ nguyên).
  3. Fuzz ≥ 1 giờ.
  4. 1 triệu chuỗi BENCH-STD text dày ≤ 40 ms đơn luồng, 0 cấp phát/chuỗi **[đề xuất]**.
  5. Đạt DoD-C.

### RGN-007 · TEXT, ATTRIB, ATTDEF một dòng
- **Mốc:** S6 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** RGN-001, RGN-004, RGN-005, RGN-108
- **Đầu vào:** 03 §7; 07.2 UI-D01 (hiệu ứng kiểu chữ); RGN-004, RGN-005, RGN-108.
- **Đầu ra:** `core/regen/text/TextRegen.{h,cpp}`; `textExtents()` (hộp bao cho `textbox`, LOD, zoom extents); `core/regen/tests/text_*`, `core/regen/bench/bench_text.cpp`.
- **Tiêu chí nghiệm thu:**
  1. 15 kiểu căn lề (Left, Center, Right, Aligned, Middle, Fit, TL, TC, TR, ML, MC, MR, BL, BC, BR) đúng điểm chèn và điểm căn; Aligned tính lại chiều cao, Fit tính lại hệ số rộng như AutoCAD.
  2. Hệ số rộng, góc nghiêng, viết ngược, lộn ngược, viết dọc (SHX), extrusion OCS; gạch dưới/gạch trên từ `%%u`/`%%o`; ATTDEF hiện tag, ATTRIB hiện giá trị; QTEXT (P1) vẽ khung; TEXTFILL cho TTF.
  3. Ma trận ca vàng 15 căn lề × 3 loại font (SHX không Unicode, unifont, TTF) × 4 hiệu ứng: SHX lệch ≤ 1e-9 × chiều cao chữ, TTF ≤ 1% chiều cao chữ (siết ở RGN-111) **[đề xuất]**.
  4. 100.000 TEXT (BENCH-STD text dày) ≤ 40 ms với 8 luồng máy tầm trung; 0 cấp phát/entity khi cache ấm **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** attribute nhiều dòng (dàn bằng RGN-009); ATTDISP và block (RGN-018).

### RGN-109 · Spike S6 phần hiển thị: chữ tiếng Việt SHX và TTF đúng 100%
- **Mốc:** S6 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** RGN-007, RND-007, QA-001
- **Đầu vào:** 05 §21 S6; mẫu QA-001: TCVN3 với `.Vn*` và SHX mã cũ, VNI với `VNI-*`, Unicode với Arial, Times New Roman và unifont; RND-007 (atlas SDF, tam giác hóa, SHX dạng nét).
- **Đầu ra:** bộ kiểm `core/regen/tests/s6_vietnamese/` (bảng glyph kỳ vọng, ảnh vàng); báo cáo `docs/spikes/S6-hien-thi-chu-viet.md` cho cổng G0.
- **Tiêu chí nghiệm thu:**
  1. 134 chữ có dấu, dạng NFC và NFD, trên 4 nhóm font: TTF Unicode, SHX unifont, SHX mã cũ (TCVN3 qua DWGCODEPAGE), TTF mã cũ (.VnTime, VNI-Times) khi có tệp font hợp lệ.
  2. 100% ký tự ra đúng glyph hoặc shape theo bảng kỳ vọng (05 §21).
  3. Ảnh qua RND-007 ở 3 cỡ (10 px SDF, 32 px, 300 px tam giác hóa) khớp ảnh vàng, SSIM ≥ 0,99 **[đề xuất]**.
  4. Báo cáo ghi kết quả, giới hạn còn lại, việc chuyển sang mốc V.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** bộ gõ (IME) của S6 thuộc UI và INT.

## 5. MTEXT và metric chữ

### RGN-008 · Parser MTEXT → cây định dạng và ghi ngược thành chuỗi
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** RGN-108, VN-102, NT-012, QA-001, QA-006
- **Đầu vào:** 05 §8 (mã định dạng MTEXT); tài liệu AutoCAD DXF Reference, mục MTEXT; README §4.2 (ranh giới trình soạn MTEXT); bộ tách token VN-102 (dựng cây trên token đó, mã còn thiếu bổ sung ngay trong `vn/encoding/cadtext`); chuỗi MTEXT của kho QA-001.
- **Đầu ra:** `core/regen/mtext/MTextTree.h`, `MTextParser.cpp` (token → cây), `MTextWriter.cpp`; API duyệt run và ánh xạ vị trí ký tự ↔ nút cho task MTEXT của CMD; đặc tả `docs/specs/regen/mtext-codes.md`. Kiểm thử, fuzz, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Đủ mã `\f \F \H \W \Q \T \S \P \L \l \O \o \K \k \A \C \c \p \N \X \~ \\ \{ \}`, nhóm `{}`, `%%`, `\U+`, `\M+`, ký tự điều khiển dạng `^I`; mã lạ giữ nguyên trong cây.
  2. Parse rồi ghi lại trùng từng byte trên 100% chuỗi MTEXT của kho QA-001.
  3. Property-based 1 triệu cây ngẫu nhiên: parse(write(T)) = T; chuỗi ghi ra không có mã thừa.
  4. Fuzz ≥ 1 giờ; parse ≥ 150 MB/s đơn luồng **[đề xuất]**.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** thao tác soạn, IME, caret (task MTEXT của CMD); dàn trang (RGN-009).

### RGN-111 · Metric chữ tương thích AutoCAD
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-007, QA-001, QA-109
- **Đầu vào:** RGN-004, RGN-005, RGN-007; tệp do AutoCAD lưu (AppInfo) trong QA-001: điểm căn thứ nhất của TEXT (mã nhóm 10) do AutoCAD tính từ điểm căn thứ hai (mã nhóm 11), kích thước thực của MTEXT (mã nhóm 42, 43); ca một dòng của QA-109.
- **Đầu ra:** `core/regen/text/TextMetrics.{h,cpp}`: chiều cao chữ → em cho TTF và SHX (above/below), advance có hệ số rộng và tracking `\T`, khoảng trắng, góc nghiêng, khoảng cách dòng; công cụ `textmetric-check` chạy trên kho; đặc tả `docs/specs/regen/text-metrics.md`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 5.000 TEXT căn khác Left: điểm căn thứ nhất tính lại lệch ≤ 0,5% chiều cao chữ so với giá trị AutoCAD lưu, trên ≥ 99% mẫu **[đề xuất]**.
  2. MTEXT một dòng không ngắt: chiều rộng thực khớp mã nhóm 42 trong 0,5% trên ≥ 99% mẫu **[đề xuất]**.
  3. Ca một dòng của QA-109: ≥ 99% trùng theo định nghĩa của QA-109.
  4. Chốt bật/tắt kerning, ligature theo số đo, ghi vào cấu hình mặc định của RGN-107; `textExtents()` (RGN-007) và `textbox` của LISP dùng cùng metric.
  5. Đạt DoD-C.

### RGN-112 · MTEXT: run định dạng và ngắt dòng
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-006, RGN-008, RGN-111, QA-109
- **Đầu vào:** cây định dạng RGN-008; font theo run qua RGN-006; metric RGN-111; API chấm điểm của QA-109.
- **Đầu ra:** `core/regen/mtext/Runs.cpp`, `LineBreaker.{h,cpp}` (có phần tử nội dòng không ngắt cho khối xếp chồng); kiểm thử, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Run có kiểu: font, chiều cao, hệ số rộng, góc nghiêng, tracking, màu; shaping theo run (RGN-004, RGN-005).
  2. Ngắt tại khoảng trắng và giữa hai ký tự CJK; không ngắt ở `\~`; từ dài hơn bề rộng, bề rộng 0 (không ngắt), khoảng trắng cuối dòng xử lý như AutoCAD.
  3. Ca QA-109 không có tab, bullet, xếp chồng: ≥ 99% đoạn trùng (cùng số dòng, cùng chuỗi mỗi dòng).
  4. Dàn lại đoạn 2.000 ký tự ≤ 1 ms p99; 100.000 đoạn BENCH-STD ≤ 60 ms với 8 luồng **[đề xuất]**.
  5. Đạt DoD-C, DoD-H, DoD-L.

### RGN-113 · MTEXT: đoạn, tab, thụt lề, giãn dòng, bullet và đánh số
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** RGN-112, QA-109
- **Màn hình:** UI-F03
- **Đầu vào:** 07.2 UI-F01 (panel Đoạn), UI-F03; mã `\p` trong cây RGN-008.
- **Đầu ra:** `core/regen/mtext/Paragraph.{h,cpp}`, `ListNumbering.cpp`; kiểm thử, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Canh đoạn trái, giữa, phải, đều, phân bố; thụt lề dòng đầu, treo, phải; tab dừng trái/giữa/phải/thập phân và tab mặc định; khoảng cách trước/sau đoạn; giãn dòng chính xác/tối thiểu/bội số.
  2. Bullet và đánh số: ký hiệu, số, chữ cái hoa/thường, tiếp tục và bắt đầu lại, tự động; đọc và giữ đúng cách AutoCAD lưu trong chuỗi.
  3. Điểm canh 9 vị trí; hướng viết theo kiểu chữ.
  4. Ca QA-109 có tab, thụt lề, bullet, giãn dòng: ≥ 99% đoạn trùng, kể cả đường chân chữ theo ngưỡng của QA-109.
  5. Đổi tab, thụt lề từ thước (UI-F03): dàn lại đoạn 2.000 ký tự ≤ 1 ms p99 **[đề xuất]**.
  6. Đạt DoD-C, DoD-L.

### RGN-114 · MTEXT: xếp chồng phân số và dung sai
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-112, QA-109
- **Màn hình:** UI-F02
- **Đầu vào:** 07.2 UI-F02; mã `\S` trong cây RGN-008; phần tử nội dòng của RGN-112.
- **Đầu ra:** `core/regen/mtext/Stack.{h,cpp}`, API dùng lại cho phân số và dung sai của kích thước (RGN-014); kiểm thử.
- **Tiêu chí nghiệm thu:**
  1. `\S` với `/` (phân số ngang có gạch), `#` (phân số chéo), `^` (dung sai, không gạch); kiểu thập phân căn theo dấu thập phân; vị trí trên/giữa/dưới; cỡ %.
  2. Khối xếp chồng không bị ngắt dòng, nâng chiều cao dòng như AutoCAD.
  3. Ca `\S` của QA-109: ≥ 99% đoạn trùng; chữ trên, chữ dưới lệch ≤ 1% chiều cao chữ so với đáp án **[đề xuất]**.
  4. Đạt DoD-C.

### RGN-009 · Dàn trang MTEXT
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-002, RGN-113, RGN-114, QA-001, QA-109
- **Màn hình:** UI-F01
- **Đầu vào:** 05 §1.2, §8; 03 §7; 07.2 UI-F01; RGN-112–RGN-114; bộ tham chiếu ngắt dòng QA-109.
- **Đầu ra:** `core/regen/mtext/MTextLayout.{h,cpp}` (`layoutMText()`, sinh primitive); gạch dưới/trên/ngang liền qua các run; nền che. Kiểm thử, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Ghép run, đoạn, xếp chồng; góc xoay, OCS; màu `\C`/`\c` theo quy tắc ByLayer/ByBlock của RGN-002; nền che (màu riêng hoặc màu nền bản vẽ, hệ số biên) là primitive tô nằm ngay dưới chữ.
  2. ≥ 99% đoạn của QA-109 trùng qua API chấm điểm của QA-109 (05 §1.2); MTEXT trong tệp do AutoCAD lưu (QA-001): chiều rộng/cao thực khớp mã nhóm 42/43 trong 0,5% trên ≥ 99% **[đề xuất]**.
  3. Soạn tại chỗ (UI-F01): dàn lại MTEXT 2.000 ký tự sau mỗi phím ≤ 2 ms p99 máy tầm trung **[đề xuất]**.
  4. 100.000 MTEXT (BENCH-STD text dày) ≤ 150 ms với 8 luồng; hiệu suất ≥ 70% đến 16 luồng **[đề xuất]**.
  5. Đạt DoD-C, DoD-H, DoD-L.

### RGN-010 · MTEXT nâng cao: cột, cột động, chiều cao tự động
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** RGN-009, QA-109
- **Màn hình:** UI-F04
- **Đầu vào:** 03 §7 (P1 cột và nâng cao); 07.2 UI-F01 (panel Chèn, cột), UI-F04; ca có cột của QA-109.
- **Đầu ra:** `core/regen/mtext/Columns.{h,cpp}`; kiểm thử, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Cột tĩnh (số cột, rộng cột, khoảng giữa, chiều cao); cột động tự cao và cao thủ công từng cột; ngắt cột `\N`; nội dung chảy qua cột.
  2. Chiều cao tự động (P1) cập nhật khi đổi bề rộng hoặc nội dung.
  3. Ca có cột tĩnh và động của QA-109: ≥ 99% đoạn trùng; vị trí cột lệch ≤ 0,5% chiều cao chữ **[đề xuất]**.
  4. Đổi số cột, khoảng giữa trong UI-F04: dàn lại MTEXT 2.000 ký tự ≤ 2 ms p99 **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.

## 6. Định dạng số và đơn vị

### RGN-013 · Thư viện định dạng số và đơn vị dùng chung, khớp từng ký tự
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DOC-010, QA-110
- **Đầu vào:** 05 §1.2 (chuỗi text của kích thước 100% trùng), §8; README §4.2; biến header LUNITS, LUPREC, AUNITS, AUPREC, UNITMODE, ANGBASE, ANGDIR, DIMZIN (DOC-010); bộ tham chiếu QA-110.
- **Đầu ra:** `core/regen/numfmt/NumFormat.h` (API công khai cho RGN-014, LSP-011, DYN, tọa độ thanh trạng thái, Properties), `UnitContext.h`, `LinearFormat.cpp`, `AngularFormat.cpp`; đặc tả `docs/specs/regen/number-format.md`; kiểm thử, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Độ dài LUNITS 1–5 (khoa học, thập phân, kỹ thuật, kiến trúc, phân số) và DIMLUNIT 6 (theo thiết lập vùng truyền vào ngữ cảnh), LUPREC 0–8, UNITMODE; DIMZIN 0–15, DIMRND, DIMDSEP, DIMFRAC; số âm, số ≈ 10⁹, số rất nhỏ, làm tròn ở biên .5.
  2. Góc AUNITS 0–4 (độ thập phân, độ/phút/giây, grad, radian, góc khảo sát `N 45d0' E`), AUPREC, DIMAZIN. Hai dạng đầu ra: chuỗi thường (`rtos`, `angtos`, tọa độ) và chuỗi MTEXT có phân số xếp chồng `\S` (kích thước).
  3. Làm tròn tất định từ biểu diễn thập phân chính xác, không dựa `printf` của nền tảng; 100% ca số thuần của QA-110 (`rtos`, `angtos`, kích thước không DIMPOST, đơn vị phụ, dung sai, text ghi đè) khớp từng ký tự (05 §1.2).
  4. Ngữ cảnh lấy từ biến header hoặc ghi đè của người gọi (dimstyle); không trạng thái toàn cục; 32 luồng gọi đồng thời TSan sạch.
  5. ≤ 150 ns/giá trị p99, 0 cấp phát (ghi vào bộ đệm của người gọi) **[đề xuất]**.
  6. Đạt DoD-C.
- **Ngoài phạm vi:** phân tích chuỗi → số (`distof`, `angtof`, nhập liệu) thuộc INT-105; ghép chuỗi kích thước (tiền tố, hậu tố, đơn vị phụ, dung sai) thuộc RGN-014.

## 7. Bộ điều phối regen, cache, thứ tự vẽ, ngân sách

### RGN-117 · Chỉ mục phụ thuộc và tập bẩn cho regen gia tăng
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-001, RGN-101, DOC-004, DOC-007, DOC-012, QA-009
- **Đầu vào:** sự kiện gom lô của DOC-007; quan hệ block lồng DOC-012; bảng ký hiệu DOC-004.
- **Đầu ra:** `core/regen/driver/DependencyIndex.{h,cpp}`, `DirtySet` (sinh lại hình, chỉ đổi vật liệu, thêm, xóa); kiểm thử, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Chỉ mục ngược cập nhật gia tăng: layer → entity; kiểu chữ → TEXT, MTEXT, ATTRIB, kích thước; dimstyle → kích thước; linetype → entity; định nghĩa block → insert (bắc cầu qua lồng nhau); xref → insert; biến header (LTSCALE, PDMODE, PDSIZE, FILLMODE, QTEXTMODE, TEXTFILL) → entity bị ảnh hưởng.
  2. 10.000 chuỗi sửa ngẫu nhiên (seed QA-009): mọi entity có primitive đổi (so regen toàn bộ trước/sau) đều nằm trong `DirtySet`; đổi màu layer → 0 entity sinh lại hình.
  3. Dựng chỉ mục 1 triệu entity ≤ 60 ms với 8 luồng; ≤ 16 byte/entity; ≤ 1 µs/sự kiện trung bình **[đề xuất]**.
  4. Đạt DoD-C, DoD-H.

### RGN-118 · Cache primitive theo entity
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-001, DOC-001, NT-007
- **Đầu vào:** `ObjectId` có thế hệ (DOC-001); arena và pool NT-007; bậc view (§0).
- **Đầu ra:** `core/regen/driver/EntityCache.{h,cpp}`: khóa (ObjectId + thế hệ; epoch ngữ cảnh gồm kiểu chữ, font, linetype, móc Bác sĩ Font; bậc view nếu phụ thuộc view) → dải primitive trong arena của document. Kiểm thử, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Bật hay tắt cache cho cùng bytes đầu ra (hash) trên toàn bộ BENCH-STD.
  2. Pan/zoom trong cùng bậc view: trúng ≥ 99%; phần quản lý ≤ 8% kích thước primitive; biến thể theo view có ngân sách, loại bỏ LRU **[đề xuất]**.
  3. Đọc không khóa trong lượt regen; ghi gộp cuối lượt theo thứ tự tất định; 32 luồng TSan sạch.
  4. Nén (compaction) chạy nền, hủy được ≤ 50 ms.
  5. Đạt DoD-C, DoD-H.

### RGN-003 · Bộ điều phối regen song song trên snapshot, regen gia tăng, cache theo entity
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** RGN-002, RGN-006, RGN-101, RGN-117, RGN-118, DOC-009, NT-005, NT-013
- **Đầu vào:** snapshot DOC-009; bộ lập lịch NT-005 (ưu tiên, hủy, QoS nhân P/E); `regenRange()` của RGN-101; RGN-117, RGN-118; RND-003 nhận diff theo vật liệu.
- **Đầu ra:** `core/regen/driver/RegenDriver.{h,cpp}`: regen toàn bộ, regen gia tăng, diff theo vật liệu, nhận lô entity mới khi nạp dần (DOC-016); kiểm thử, `core/regen/bench/bench_driver.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Regen toàn bộ: chia khối theo không gian và chủ sở hữu, `parallel_for` NT-005, arena theo luồng; hợp nhất theo khóa (không gian, thứ tự vẽ do RGN-027 điền, chỉ số entity).
  2. Tất định: hash đầu ra bằng nhau với 1, 2, 4, 8, 16, 32 luồng; regen gia tăng bằng regen toàn bộ trên 10.000 chuỗi sửa ngẫu nhiên.
  3. Sửa 1 entity trong cảnh 1 triệu: từ sự kiện đến diff sẵn sàng ≤ 2 ms p99 máy tầm trung **[đề xuất]**.
  4. Lượt mới hủy lượt cũ ≤ 50 ms; regen do tương tác chạy ưu tiên cao, việc nền (bậc view mới, nén cache) ưu tiên thấp; lượt > 2 s báo tiến trình qua NT-013.
  5. Đạt DoD-C, DoD-H, DoD-L.

### RGN-027 · Thứ tự vẽ (SORTENTS, draw order) trong luồng primitive
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** RGN-003, DOC-020
- **Đầu vào:** bảng SORTENTS (DOC-020); 05 §9.1 (tuân thủ draw order); sysvar DRAWORDERCTL, SORTENTS.
- **Đầu ra:** `core/regen/driver/DrawOrder.{h,cpp}`: khóa thứ tự theo block record và không gian, đưa vào khóa hợp nhất của RGN-003; kiểm thử, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Entity trong block vẽ tại vị trí của insert, theo thứ tự trong block; DRAWORDERCTL 0–3 và bit regen, in của SORTENTS được tôn trọng.
  2. Ca vàng ≥ 150 cảnh chồng nhau (hatch đặc, wipeout, ảnh, nền che MTEXT) khớp 100% thứ tự che **[đề xuất]**.
  3. Đổi thứ tự 1 entity (DRAWORDER) chỉ cập nhật khóa, không sinh lại hình: ≤ 1 ms p99 trong cảnh 1 triệu **[đề xuất]**.
  4. Đạt DoD-C, DoD-L.

### RGN-028 · Ngân sách regen: 1 triệu entity ≤ 0,5 s, hiệu suất song song ≥ 70%
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-003, RGN-007, RGN-009, RGN-011, RGN-012, RGN-014, RGN-018, RGN-027, RGN-102, NT-003, NT-004, QA-012
- **Đầu vào:** 05 §1.1 (regen 1 triệu entity, hiệu suất song song); cảnh BENCH-STD (QA-012); cổng hiệu năng NT-003; Tracy và bộ đếm cấp phát NT-004.
- **Đầu ra:** `core/regen/bench/bench_regen_full.cpp` (cảnh hỗn hợp, đường scaling 1→32 luồng); tối ưu điểm nóng (SoA, SIMD NT-006) trong các phần regen; số đo lưu trong CI.
- **Tiêu chí nghiệm thu:**
  1. Regen toàn bộ cảnh 1 triệu entity hỗn hợp (đường cong, TEXT, MTEXT, hatch, kích thước, block lồng, linetype): ≤ 0,5 s p99 với 8 nhân máy tầm trung, ≤ 2 s với 4 nhân máy thấp (05 §1.1).
  2. Hiệu suất song song ≥ 70% so với tuyến tính đến 16 luồng (máy mạnh); đường 1→32 luồng lưu trong CI.
  3. Cảnh 10 triệu entity ≤ 4 s trên máy mạnh 16 luồng; bộ nhớ đỉnh của lượt regen 1 triệu entity ≤ 400 MB ngoài dữ liệu bản vẽ **[đề xuất]**.
  4. 0 cấp phát/entity ở trạng thái ổn định; NT-003 chặn merge khi vượt ngưỡng hoặc chậm hơn baseline 5%.
  5. Đạt DoD-C, DoD-H.

## 8. Linetype

### RGN-151 · Sinh nét linetype theo chiều dài cung thật
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-001, RGN-002, GEO-005, GEO-006, GEO-007, GEO-016, NT-005, NT-006
- **Đầu vào:** 05 §8 (Linetype: sinh theo chiều dài cung thật), §9.1 (linetype tính trong fragment shader theo tham số chiều dài cung); mẫu nét của bản ghi LTYPE qua RGN-002; độ dài cung GEO-005, GEO-006, GEO-007.
- **Đầu ra:**
  - `core/regen/linetype/ArcLength.{h,cpp}`: chiều dài tích lũy tại đỉnh cho line, cung, tròn, ellipse, spline, polyline có bulge; tham số mẫu cho RND-006.
  - `core/regen/linetype/DashGenerator.{h,cpp}`: sinh tường minh gạch, chấm, khoảng hở (cung giữ là cung) cho đường dự phòng CPU, PLT-003, OSNAP; chế độ theo từng đoạn hoặc liên tục qua đỉnh (cờ PLINEGEN).
  - Kiểm thử `core/regen/tests/linetype_dash_test.cpp`; benchmark `core/regen/bench/linetype_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Đường shader và đường sinh tường minh cho cùng vị trí mép nét, lệch ≤ 1e-9 × chiều dài đường (GEO-002).
  2. Căn mẫu ở hai đầu đường hở, điểm bắt đầu trên đường kín, đường ngắn hơn một chu kỳ: khớp ảnh DWG TrueView (QA-004) trên ≥ 200 ca vàng **[đề xuất]**.
  3. Ellipse, spline: sai số chiều dài cung ≤ 1e-9 tương đối so với tích phân tham chiếu.
  4. 1 triệu entity có linetype: tính tham số mẫu ≤ 60 ms trên 8 nhân, máy tầm trung **[đề xuất]**; 0 cấp phát trong vòng lặp.
  5. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** parser `.lin` (CMD-513); shape và text của linetype phức (RGN-152).

### RGN-152 · Phần tử shape và text của linetype phức
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-151, RGN-004, RGN-005, RGN-007
- **Đầu vào:** 05 §8 (mẫu nét phức có shape, text); phần tử `[tên,tệp,S,R|A|U,X,Y]` và `["chuỗi",kiểu,…]` lưu trong LTYPE; glyph SHX RGN-004, TrueType RGN-005, căn chữ RGN-007.
- **Đầu ra:** `core/regen/linetype/ComplexElements.{h,cpp}`: đặt shape/text theo chiều dài cung với xoay tương đối (R), tuyệt đối (A), luôn đọc được (U), offset X/Y, tỉ lệ S; primitive glyph riêng (RND-006 không vẽ phần này); kiểm thử với linetype phức tên chuẩn (GAS_LINE, HOT_WATER_SUPPLY, FENCELINE1, BATTING, ZIGZAG).
- **Tiêu chí nghiệm thu:**
  1. Vị trí và góc từng shape/text khớp ảnh TrueView (QA-004) ≤ 0,5 px ở 1:1 trên ≥ 100 ca vàng **[đề xuất]**.
  2. Đoạn không đủ chỗ cho phần tử: xử lý như AutoCAD, có ca kiểm riêng; shape hoặc font thiếu: bỏ phần tử, ghi log một lần mỗi tệp.
  3. 100.000 entity linetype phức regen ≤ 300 ms trên 8 nhân **[đề xuất]**.
  4. Đạt DoD-C, DoD-H.

### RGN-011 · Linetype đơn giản và phức theo chiều dài cung thật; LTSCALE, PSLTSCALE, CELTSCALE, MSLTSCALE, PLINEGEN
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-151, RGN-152, RGN-003, DOC-010, DOC-011, QA-005
- **Đầu vào:** 05 §8; 03 §10 (`LINETYPE`, `LTSCALE`, `CELTSCALE`, `PSLTSCALE`, `MSLTSCALE` P0); biến header (DOC-010); layout và viewport (DOC-011).
- **Đầu ra:** `core/regen/linetype/LinetypeScale.{h,cpp}`: tỉ lệ hiệu dụng = LTSCALE × tỉ lệ riêng của entity (CELTSCALE chỉ là giá trị mặc định khi tạo) × hệ số PSLTSCALE theo viewport × hệ số MSLTSCALE theo tỉ lệ chú thích; cờ PLINEGEN của từng polyline; điền mẫu nét vào khóa vật liệu RGN-001 cho RND-006; kiểm thử `core/regen/tests/linetype_integration_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Ma trận LTSCALE {0,1; 1; 100} × PSLTSCALE {0; 1} × MSLTSCALE {0; 1} × tỉ lệ chú thích {1:1; 1:100} × model/viewport khớp ảnh in TrueView (QA-005, SSIM ≥ 0,99) **[đề xuất]**.
  2. Đổi LTSCALE: linetype đơn giản chỉ đổi tham số vật liệu, cập nhật ≤ 1 khung, không sinh lại hình học; linetype phức sinh lại ở nền (NT-005), hủy được.
  3. ByLayer, ByBlock, linetype của xref (`XREF|TÊN`) phân giải đúng trên ≥ 500 tệp QA-001 có linetype phức.
  4. Chu kỳ mẫu < 1 px trên màn hình: đánh dấu vẽ liền cho RND-006, không sinh nét vô ích.
  5. Đạt DoD-C, DoD-H.

## 9. Hatch

### RGN-153 · Parser `.pat` và mô hình họ đường mẫu
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** DOC-003, NT-011, NT-012, QA-006
- **Đầu vào:** 05 §8 (Hatch: parser `.pat`); định dạng `.pat` trong tài liệu tùy biến công khai của AutoCAD; `.pat` TCVN của VN-008 làm dữ liệu kiểm thử khi có; họ đường lưu sẵn trong entity HATCH (DOC-003).
- **Đầu ra:**
  - `core/regen/hatch/PatParser.{h,cpp}`: `parsePat(bytes)` trả định nghĩa (tên, mô tả, họ đường: góc, gốc, dịch, khoảng cách, dash) và chẩn đoán (dòng, cột); mã hóa ANSI và UTF-8 (NT-012).
  - `core/regen/hatch/HatchPattern.{h,cpp}`: áp tỉ lệ, góc, double, bề rộng bút ISO; tra mẫu theo tên trên đường dẫn hỗ trợ (NT-011).
  - Fuzz `core/regen/fuzz/pat_fuzz.cpp`; kiểm thử `core/regen/tests/pat_parser_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Đọc đúng 100% bộ ≥ 300 định nghĩa tự soạn (dòng dài, chú thích, khoảng trắng lạ, ký tự `*` trong mô tả) **[đề xuất]**.
  2. Họ đường dựng từ `.pat` + tỉ lệ + góc khớp họ đường AutoCAD lưu trong ≥ 1.000 HATCH của QA-001, lệch ≤ 1e-12 tương đối.
  3. Tệp hỏng: báo dòng lỗi, bỏ định nghĩa lỗi, giữ phần còn lại; giới hạn ≤ 1.000 họ mỗi mẫu, ≤ 64 dash mỗi họ **[đề xuất]**.
  4. Fuzz ≥ 1 giờ không lỗi.
  5. Đạt DoD-C.

### RGN-154 · Nét mẫu hatch cắt chính xác theo biên cong, island
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** RGN-153, GEO-010, GEO-014, NT-005, NT-006
- **Đầu vào:** 05 §8 (sinh nét mẫu cắt chính xác theo biên đường cong), §7; vòng biên lưu trong HATCH (line, cung, cung ellipse, spline, polyline có bulge) và cờ vòng (ngoài, ngoài cùng, textbox); kiểu island Normal/Outer/Ignore; giao điểm GEO-010; lồng vòng GEO-014.
- **Đầu ra:** `core/regen/hatch/PatternLines.{h,cpp}`: mỗi họ quét các đường song song phủ hộp bao, giao chính xác với cạnh cong, sắp theo tham số, lọc chẵn-lẻ theo kiểu island, áp dash theo pha tính từ gốc mẫu; chỉ mục cạnh theo phương vuông góc (không duyệt số đường × số cạnh); song song theo lô đường quét (NT-005), SIMD (NT-006); đoạn kết quả cho đường dự phòng CPU của RND-016, PLT-003, OSNAP lên hatch.
- **Tiêu chí nghiệm thu:**
  1. Mép mọi đoạn nằm trên biên, lệch ≤ 1e-9 × kích thước hatch; 0 đoạn ra ngoài vùng hoặc lọt vào island trên 1 triệu ca QA-009 (đường quét qua đỉnh, tiếp xúc cung, cạnh trùng đường quét).
  2. Kết quả trùng từng bit với 1, 4, 16 luồng.
  3. Hatch 1 vòng 1.000 cạnh cong sinh 1 triệu đoạn: ≤ 60 ms trên 8 nhân, ≤ 250 ms 1 luồng, máy tầm trung **[đề xuất]**; hiệu suất song song ≥ 60% đến 8 luồng.
  4. Khớp ảnh TrueView (QA-004, SSIM ≥ 0,99) trên ≥ 500 hatch mẫu của QA-001 **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

### RGN-012 · Hatch: parser `.pat`, nét mẫu cắt theo biên cong, island, tô đặc, gradient, LOD
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** RGN-154, RGN-001, RGN-003, GEO-003, GEO-016, GEO-021, QA-012
- **Đầu vào:** 05 §8 (Hatch), §9.1 (LOD: hatch quá dày vẽ thành mảng tô); primitive vùng tô mẫu của RGN-001 (họ đường cho RND-016); 9 kiểu gradient (LINEAR, CYLINDER, INVCYLINDER, SPHERICAL, INVSPHERICAL, HEMISPHERICAL, INVHEMISPHERICAL, CURVED, INVCURVED), một hoặc hai màu, tint, shift, góc; cảnh hatch dày QA-012.
- **Đầu ra:** `core/regen/hatch/HatchRegen.{h,cpp}`: điền primitive vùng tô mẫu (họ đường + vòng biên tessellate theo GEO-016); tô đặc bằng tam giác hóa tất định có lỗ (predicate GEO-003); gradient (tham số cho shader + hàm CPU tham chiếu cho PLT); chọn LOD theo khoảng cách nét trên màn hình; cache theo entity; benchmark `core/regen/bench/hatch_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Khoảng cách nét trên màn hình < 2 px chuyển sang mảng tô (màu theo mật độ nét), trễ ngưỡng 20% để không nhấp nháy **[đề xuất]**.
  2. Tam giác hóa: tổng diện tích khớp GEO-021 lệch ≤ 1e-9 tương đối, không tam giác chồng, trùng bit mọi số luồng.
  3. Gradient 9 kiểu × một/hai màu × shift/góc so ảnh TrueView (QA-005): ΔE trung bình ≤ 2 **[đề xuất]**.
  4. Cảnh hatch dày BENCH-STD: regen ≤ 1 s trên 8 nhân, ≤ 4 s máy thấp 4 nhân **[đề xuất]**.
  5. Kho QA-001 10.000 tệp: 0 crash, 0 hatch bỏ qua im lặng (biên hỏng: vẽ biên, ghi log).
  6. Đạt DoD-C, DoD-H.

### RGN-155 · Móc liên kết hatch: dựng lại biên từ đối tượng nguồn
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** RGN-012, GEO-014, DOC-005, DOC-007
- **Đầu vào:** handle đối tượng nguồn lưu trong vòng biên HATCH; reactor lưu trong bản vẽ (DOC-005); sự kiện thay đổi DOC-007; BOUNDARY và vùng kín GEO-014.
- **Đầu ra:** `core/regen/hatch/HatchAssoc.{h,cpp}`: chỉ mục nguồn → hatch; hàm thuần `rebuildLoops(hatch, snapshot)`; bộ nghe sự kiện xếp hatch bị ảnh hưởng vào transaction của lệnh đang chạy (DOC-007); biên không còn kín thì giữ hình cũ, bỏ cờ liên kết như AutoCAD. Lệnh HATCH và lệnh sửa (MOVE, STRETCH, TRIM…) dùng qua sự kiện.
- **Tiêu chí nghiệm thu:**
  1. Dời, kéo giãn, xóa đối tượng biên: hatch cập nhật trong cùng mark undo; UNDO trả cả hai về như cũ.
  2. Sửa 1 đối tượng biên của hatch 10.000 cạnh: cập nhật ≤ 20 ms tầm trung.
  3. 10.000 kịch bản sửa ngẫu nhiên (QA-009): không vòng lặp sự kiện, không hatch mồ côi.
  4. Đạt DoD-C, DoD-L.

## 10. Block và dynamic block

### RGN-156 · Dữ liệu instancing block và làm phẳng block lồng
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-001, RGN-002, RGN-003, DOC-012, GEO-022, NT-007
- **Đầu vào:** 05 §9.1 (hình học định nghĩa upload một lần, mỗi tham chiếu là một ma trận); ByBlock và layer 0 qua block lồng (RGN-002); biến đổi không đều GEO-022 (tròn → ellipse); định nghĩa và tham chiếu block DOC-012.
- **Đầu ra:** `core/regen/block/BlockInstancing.{h,cpp}`: cache primitive theo định nghĩa (tọa độ block, dựng một lần, dùng chung); bản ghi instance (ma trận, ngữ cảnh ByBlock/layer 0, đường dẫn insert lồng cho chọn và `nentsel`); làm phẳng song song; chống chu trình định nghĩa; dữ liệu instancing theo mô hình RGN-001 cho RND-009.
- **Tiêu chí nghiệm thu:**
  1. Cảnh block lồng BENCH-STD (QA-012): dựng 1 triệu instance ≤ 200 ms trên 8 nhân; bản ghi instance ≤ 64 byte **[đề xuất]**.
  2. Sửa một định nghĩa: chỉ cache của định nghĩa đó và instance trỏ tới nó bị vô hiệu.
  3. Tỉ lệ âm (gương), không đều, extrusion: ảnh khớp TrueView (QA-004) trên ≥ 300 tệp QA-001 có block lồng ≥ 5 cấp **[đề xuất]**.
  4. Định nghĩa tự tham chiếu (tệp hỏng): phát hiện, bỏ nhánh, ghi log, không tràn ngăn xếp.
  5. Đạt DoD-C, DoD-H.

### RGN-018 · Block: insert lồng nhau, attribute, ATTDISP, MINSERT; dữ liệu instancing
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-156, RGN-007, RGN-009, DOC-010
- **Đầu vào:** 03 §11 (`ATTDEF`, `ATTSYNC`, `ATTDISP` P0; `MINSERT` P1); ATTDEF/ATTRIB, MINSERT (DOC-012); TEXT một dòng RGN-007; MTEXT RGN-009; biến ATTMODE (DOC-010).
- **Đầu ra:** `core/regen/block/InsertRegen.{h,cpp}`: INSERT và MINSERT (hàng, cột, khoảng cách → lưới instance, không nhân bản hình học); ATTRIB một dòng và nhiều dòng; ATTDEF hằng trong định nghĩa hiện như AutoCAD, ATTDEF thường không hiện trong insert; ATTMODE 0/1/2 kết hợp cờ ẩn của attribute; kiểm thử `core/regen/tests/insert_regen_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Ma trận ATTMODE × cờ ẩn × attribute nhiều dòng × MINSERT: ca vàng khớp TrueView (QA-004).
  2. Đổi ATTMODE: chỉ primitive attribute đổi trạng thái, không dựng lại instancing; ≤ 50 ms với 100.000 attribute trên 8 nhân **[đề xuất]**.
  3. MINSERT 1.000 × 1.000: bộ nhớ thêm ≤ 64 MB **[đề xuất]**.
  4. Kho QA-001: 100% insert hiển thị, 0 crash.
  5. Đạt DoD-C, DoD-H.

### RGN-019 · Dynamic block: hiển thị qua block đại diện `*U`
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** RGN-018, DWG-015, QA-004, QA-005
- **Đầu vào:** 03 §11 (dynamic block hiển thị đúng P0, DWG lưu sẵn `*U…`); 05 §8 (khi xem dùng `*U`); block đại diện `*U` (DOC-012); đối tượng dynamic block đọc và giữ nguyên (DWG-015).
- **Đầu ra:** `core/regen/dynblock/DynBlockView.{h,cpp}`: insert trỏ `*U` vẽ như block thường; insert trỏ thẳng định nghĩa động: hiện theo trạng thái visibility đang lưu trong định nghĩa; `*U` thiếu hoặc hỏng: dùng định nghĩa, ghi log; tra tên hiệu dụng (effective name) cho Properties và bộ lọc chọn.
- **Tiêu chí nghiệm thu:**
  1. 100% insert động trong ≥ 1.000 tệp QA-001 có dynamic block khớp ảnh TrueView (SSIM ≥ 0,99) **[đề xuất]**.
  2. Tên hiệu dụng khớp 100% dữ liệu ODA File Converter xuất (QA-004).
  3. Chi phí regen so với block thường chênh ≤ 5%.
  4. Đạt DoD-C.
- **Ngoài phạm vi:** tính lại parameter và action (RGN-157…RGN-160, mốc 1.0).

### RGN-157 · Lõi engine EvalGraph: đồ thị, thứ tự đánh giá, tập giá trị
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** RGN-019, DOC-023, DOC-014
- **Đầu vào:** 05 §8 (engine EvalGraph tự viết cho parameter và action); 03 §11 (grip tương tác P1); mô hình EvalGraph, parameter, action DOC-023; sao chép sâu có ánh xạ ID DOC-014.
- **Đầu ra:** `core/regen/dynblock/EvalGraph.{h,cpp}`: dựng đồ thị từ DOC-023, thứ tự topo tất định, lan truyền cờ bẩn khi đổi giá trị, action xích; tập giá trị (danh sách, bước, min/max, bắt về giá trị hợp lệ gần nhất); parameter điểm và điểm gốc (base point); hàm thuần `evaluate(def, values)` → nội dung `*U` mới + giá trị thuộc tính động; bảng đăng ký loại parameter/action cho RGN-158, RGN-159.
- **Tiêu chí nghiệm thu:**
  1. Cùng định nghĩa và giá trị cho cùng nội dung `*U` từng bit, mọi số luồng.
  2. Đồ thị có chu trình hoặc tham chiếu treo: báo lỗi, giữ `*U` cũ, không crash.
  3. Đánh giá lại một parameter trên block 1.000 entity, 20 action ≤ 2 ms tầm trung **[đề xuất]**.
  4. Đạt DoD-C, DoD-L.

### RGN-158 · Parameter linear, polar, XY; action move, scale, stretch, polar stretch, array
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** RGN-157, GEO-022
- **Đầu vào:** 03 §11 (linear, polar, XY P1); 07.2 UI-G04 (danh sách parameter và action); DOC-023.
- **Đầu ra:** `core/regen/dynblock/params/{Linear,Polar,XY}Param.cpp`, `actions/{Move,Scale,Stretch,PolarStretch,Array}Action.cpp`: hệ số khoảng cách và góc; gốc phụ thuộc/độc lập cho scale; khung stretch (đỉnh trong khung dời, entity nằm hẳn trong khung dời cả); array theo khoảng cách hàng/cột; kiểm thử `core/regen/tests/dynblock_linear_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Insert động của QA-001 dùng các loại này: đánh giá từ giá trị lưu trong insert tái tạo `*U` đã lưu, lệch ≤ 1e-9 × kích thước block, trên ≥ 99% insert **[đề xuất]**; ca lệch được phân loại.
  2. Stretch trên line, cung, polyline, hatch, kích thước bị khung cắt: khớp ≥ 100 ca vàng **[đề xuất]**.
  3. Action xích chạy đúng thứ tự của RGN-157.
  4. Đạt DoD-C.

### RGN-159 · Parameter rotation, flip, alignment, visibility, lookup; action rotate, flip, lookup, bảng thuộc tính block
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** RGN-157, GEO-022
- **Đầu vào:** 03 §11 (rotation, flip, visibility, lookup P1; `BTABLE`); 07.2 UI-G04; DOC-023.
- **Đầu ra:** `core/regen/dynblock/params/{Rotation,Flip,Alignment,Visibility,Lookup}Param.cpp`, `actions/{Rotate,Flip,Lookup}Action.cpp`, `BlockPropertiesTable.cpp`: trạng thái visibility (tập entity theo trạng thái), lookup xuôi và ngược, hàng bảng thuộc tính block đặt nhiều parameter một lần, alignment xoay theo hướng đối tượng đích.
- **Tiêu chí nghiệm thu:**
  1. Như RGN-158 mục 1 cho các loại này: ≥ 99% insert tái tạo đúng `*U` đã lưu **[đề xuất]**.
  2. Đổi trạng thái visibility chỉ đổi tập entity hiện, không sao chép hình học; ≤ 1 ms trên block 1.000 entity **[đề xuất]**.
  3. Lookup ngược với giá trị không có trong bảng trả "Tùy chỉnh" như AutoCAD.
  4. Đạt DoD-C.

### RGN-160 · Dynamic block: API đánh giá cho jig, ghi `*U`, nghiệm thu EvalGraph
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** RGN-158, RGN-159, DOC-007
- **Đầu vào:** 03 §11 (grip tương tác P1); README §4.2 (grip dynamic block do CMD-558 đăng ký qua INT-013); DOC-023.
- **Đầu ra:** `core/regen/dynblock/DynBlockEval.{h,cpp}`: API đánh giá cho jig (giá trị parameter mới → hình tạm, không ghi DB); vị trí và loại grip theo parameter (tam giác, mũi tên, tròn, lookup) cho CMD-558; khi thả: transaction DOC-007 ghi `*U` mới và giá trị thuộc tính động; Đặt lại block; API thuộc tính động cho Properties và lệnh block của CMD.
- **Tiêu chí nghiệm thu:**
  1. Đánh giá lại block 1.000 entity khi đổi một parameter ≤ 4 ms p99 tầm trung **[đề xuất]**, để jig của CMD-558 giữ ≤ 8 ms/khung.
  2. Một thao tác grip = một mark undo; undo/redo trả `*U` và giá trị cũ.
  3. Lưu rồi mở bằng ODA File Converter/TrueView (QA-004): hình và thuộc tính động khớp 100% trên 200 kịch bản **[đề xuất]**.
  4. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** đăng ký grip, jig, mark undo của thao tác grip trên bản vẽ (CMD-558).

## 11. Kích thước

### RGN-161 · Bộ phân giải DIMVAR: kiểu, ghi đè, tỉ lệ hiệu dụng
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-004, DOC-005, DOC-010, DOC-011
- **Đầu vào:** 05 §8 (Kích thước); 03 §8 (`DIMSTYLE`, `DIMOVERRIDE`); 07.2 UI-D02 (7 tab); bảng DIMSTYLE (DOC-004); ghi đè trong xdata `ACAD`/`DSTYLE` và xdata mở rộng của kiểu (DOC-005); biến header (DOC-010).
- **Đầu ra:** `core/regen/dim/DimVars.{h,cpp}`: struct có kiểu `ResolvedDimStyle`; thứ tự header → kiểu → kiểu con theo loại dim → ghi đè của entity; DIMSCALE hiệu dụng (0 = theo tỉ lệ viewport; annotative = 1/tỉ lệ chú thích); DIMLFAC âm chỉ áp trong layout. Danh sách phủ bắt buộc:
  - Đường: DIMCLRD, DIMLTYPE, DIMLWD, DIMDLE, DIMDLI, DIMSD1, DIMSD2, DIMCLRE, DIMLTEX1, DIMLTEX2, DIMLWE, DIMSE1, DIMSE2, DIMEXE, DIMEXO, DIMFXL, DIMFXLON.
  - Ký hiệu: DIMBLK, DIMBLK1, DIMBLK2, DIMLDRBLK, DIMSAH, DIMASZ, DIMTSZ, DIMCEN, DIMARCSYM, DIMJOGANG, cỡ ngắt dim, hệ số chiều cao gãy dim thẳng.
  - Chữ, fit: DIMTXSTY, DIMCLRT, DIMTXT, DIMTFILL, DIMTFILLCLR, DIMGAP, DIMTAD, DIMJUST, DIMTVP, DIMTIH, DIMTOH, DIMTXTDIRECTION, DIMATFIT, DIMTIX, DIMSOXD, DIMTOFL, DIMTMOVE, DIMUPT, DIMSCALE, cờ annotative.
  - Đơn vị: DIMLUNIT, DIMDEC, DIMFRAC, DIMDSEP, DIMRND, DIMPOST, DIMLFAC, DIMZIN, DIMMZF, DIMMZS, DIMAUNIT, DIMADEC, DIMAZIN, DIMALT, DIMALTU, DIMALTD, DIMALTF, DIMALTRND, DIMAPOST, DIMALTZ, DIMALTMZF, DIMALTMZS.
  - Dung sai: DIMTOL, DIMLIM, DIMTP, DIMTM, DIMTDEC, DIMTZIN, DIMTFAC, DIMTOLJ, DIMALTTD, DIMALTTZ.
- **Tiêu chí nghiệm thu:**
  1. Mỗi biến trong danh sách có ca kiểm: đọc từ kiểu, từ ghi đè, giá trị biên; DWG và DXF cho cùng kết quả.
  2. ≥ 2.000 kích thước QA-001: giá trị phân giải khớp 100% dữ liệu ODA File Converter xuất (QA-004).
  3. ≤ 2 µs/kích thước trung vị, cache theo cặp (kiểu, ghi đè) **[đề xuất]**.
  4. Đạt DoD-C.

### RGN-162 · Chuỗi chữ kích thước khớp từng ký tự
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-161, RGN-013, RGN-008, NT-007, QA-110
- **Đầu vào:** 05 §1.2 (chữ kích thước khớp 100% từng ký tự), §8; định dạng số RGN-013; parser MTEXT RGN-008.
- **Đầu ra:** `core/regen/dim/DimText.{h,cpp}`: giá trị đo × DIMLFAC, làm tròn DIMRND, định dạng DIMLUNIT/DIMDEC/DIMFRAC/DIMDSEP/DIMZIN, đơn vị con DIMMZF/DIMMZS, góc DIMAUNIT/DIMADEC/DIMAZIN; đơn vị phụ `[]` theo DIMALT*, đặt sau hoặc dưới (`\X`); dung sai đối xứng, sai lệch (xếp chồng `\S`, DIMTFAC, DIMTOLJ), giới hạn DIMLIM, cơ bản (khung); DIMPOST với `<>`; ghi đè text (rỗng, `<>`, một dấu cách = ẩn); tiền tố `R`, `%%c`. Kết quả: chuỗi MTEXT và cây định dạng.
- **Tiêu chí nghiệm thu:**
  1. Khớp 100% từng ký tự với text trong block `*D` của ≥ 20.000 kích thước QA-001 do AutoCAD tạo; ca lệch phải quy được về block cũ hoặc ghi đè ngoài **[đề xuất]**; khớp 100% QA-007 khi có đáp án.
  2. Ma trận 6 DIMLUNIT × DIMDEC 0–8 × DIMZIN 0–15 × DIMRND {0; 0,25; 5} × giá trị biên (0, âm, ≈ 10⁹, đúng nửa đơn vị làm tròn) có ca vàng.
  3. ≤ 1 µs/chuỗi trung vị, 0 cấp phát nhờ arena NT-007 **[đề xuất]**.
  4. Đạt DoD-C.

### RGN-163 · Mũi tên, ký hiệu và quy tắc fit dùng chung cho kích thước
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-161, RGN-162, RGN-009, RGN-018
- **Đầu vào:** 07.2 UI-D02 (tab Ký hiệu & mũi tên, Fit); 05 §8 (quy tắc fit DIMATFIT/DIMTMOVE); hộp chữ đo bằng RGN-009; block người dùng làm mũi tên (RGN-018).
- **Đầu ra:**
  - `core/regen/dim/Arrowheads.{h,cpp}`: 20 mũi tên dựng sẵn (_ClosedFilled, _ClosedBlank, _Closed, _Dot, _ArchTick, _Oblique, _Open, _Origin, _Origin2, _Open90, _Open30, _DotSmall, _DotBlank, _Small, _BoxBlank, _BoxFilled, _DatumBlank, _DatumFilled, _Integral, _None) tự dựng hình; block DIMBLK*; gạch DIMTSZ; dấu tâm DIMCEN; nền chữ DIMTFILL; lật mũi tên.
  - `core/regen/dim/Fit.{h,cpp}`: hàm thuần quyết định chữ và mũi tên trong/ngoài theo DIMATFIT 0–3, DIMTIX, DIMSOXD, DIMTOFL, DIMTMOVE 0–2, DIMUPT.
- **Tiêu chí nghiệm thu:**
  1. Mỗi mũi tên khớp hình block `_TÊN` AutoCAD lưu trong QA-001, lệch ≤ 1e-9 × DIMASZ.
  2. Mọi tổ hợp DIMATFIT × DIMTIX × DIMSOXD × DIMTOFL × DIMTMOVE với khoảng trống quanh ngưỡng (±1e-6) có ca kiểm; quyết định khớp block `*D` của QA-001 ≥ 99,5% **[đề xuất]**.
  3. Đạt DoD-C.

### RGN-014 · Kích thước thẳng, nghiêng, xoay theo đủ DIMVAR; xem dùng block lưu sẵn, sửa thì sinh lại
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** RGN-163, RGN-003, QA-001
- **Đầu vào:** 05 §8; 03 §8 (`DIMLINEAR`, `DIMALIGNED`, `DIMCONTINUE`, `DIMBASELINE` P0; `DIMBREAK`, `DIMJOGLINE` P1); điểm định nghĩa, góc xoay, góc nghiêng, góc chữ của DIMENSION; block `*D` lưu sẵn (DOC-012).
- **Đầu ra:**
  - `core/regen/dim/LinearDim.{h,cpp}`: đường gióng (DIMEXO, DIMEXE, DIMFXL, ẩn 1/2, nghiêng), đường dim (DIMDLE, ẩn nửa), vị trí chữ (DIMTAD 0–4, DIMJUST 0–4, DIMTVP, hướng chữ), gãy DIMJOGLINE và ngắt DIMBREAK lưu kèm entity, kích thước xuyên không gian (đo qua tỉ lệ viewport).
  - `core/regen/dim/DimBlockPolicy.{h,cpp}`: khi xem dùng `*D` nếu hợp lệ; sinh lại khi thiếu block, entity đã sửa hoặc kiểu đổi; công cụ so `*D` với kết quả sinh lại trên kho.
  - Benchmark `core/regen/bench/dim_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Sinh lại so với `*D` của ≥ 20.000 kích thước thẳng QA-001: hình học lệch ≤ 1e-9 × kích thước trên ≥ 99% **[đề xuất]**; khớp 100% QA-007 khi có.
  2. Jig: sinh một kích thước ≤ 0,2 ms p99 tầm trung **[đề xuất]**.
  3. 100.000 kích thước sinh lại ≤ 0,5 s trên 8 nhân, ≤ 2 s máy thấp 4 nhân **[đề xuất]**.
  4. DXF của phần mềm khác không có `*D`: 100% kích thước vẫn hiển thị.
  5. Đạt DoD-C, DoD-H, DoD-L.

### RGN-164 · Kích thước góc và độ dài cung
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-163, GEO-005
- **Đầu vào:** 03 §8 (`DIMANGULAR` P0; `DIMARC` P1); 07.2 UI-D02 (ký hiệu độ dài cung trước / trên / không).
- **Đầu ra:** `core/regen/dim/AngularDim.{h,cpp}`: góc 2 đường, 3 điểm, theo cung và đường tròn; góc > 180°; cung gióng kéo dài khi điểm đặt ngoài đoạn; chữ theo cung, fit theo chiều dài cung. `core/regen/dim/ArcLengthDim.{h,cpp}`: DIMARCSYM, cung bộ phận, đường dẫn khi cung > 90°. Định dạng góc qua RGN-162.
- **Tiêu chí nghiệm thu:**
  1. Sinh lại so với `*D` của ≥ 5.000 kích thước góc và cung QA-001: lệch ≤ 1e-9 × kích thước trên ≥ 99% **[đề xuất]**.
  2. Ca suy biến (hai đường song song, góc 0°/180°/360°, cung rất nhỏ): không crash, hiển thị như AutoCAD trên ca vàng.
  3. Đạt DoD-C.

### RGN-015 · Kích thước góc, bán kính, đường kính, cung, gãy khúc, tọa độ điểm
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** RGN-014, RGN-164
- **Đầu vào:** 03 §8 (`DIMRADIUS`, `DIMDIAMETER` P0; `DIMJOGGED`, `DIMORDINATE`, `DIMCENTER` P1); 07.2 UI-D02 (góc gãy bán kính, dấu tâm).
- **Đầu ra:** `core/regen/dim/RadialDim.{h,cpp}`: bán kính, đường kính (chữ trong/ngoài, đoạn ngang khi DIMTOH, cung gióng khi điểm đặt ngoài cung, dấu tâm), gãy khúc (DIMJOGANG, tâm thay thế); `core/regen/dim/OrdinateDim.{h,cpp}`: X/Y theo gốc của kích thước, đường dẫn có gãy; tích hợp 6 loại vào bộ điều phối RGN-003 cùng chính sách `*D` của RGN-014.
- **Tiêu chí nghiệm thu:**
  1. Sinh lại so với `*D` của ≥ 10.000 kích thước các loại này trong QA-001: hình học lệch ≤ 1e-9 trên ≥ 99% **[đề xuất]**; chữ khớp 100% (RGN-162).
  2. Mỗi loại có ca vàng cho từng giá trị DIMTAD, DIMATFIT, DIMTOFL, DIMCEN (dương, âm, 0).
  3. Ngân sách như RGN-014 mục 2–3, đo trên hỗn hợp 6 loại.
  4. Đạt DoD-C, DoD-H, DoD-L.

### RGN-165 · Khung dung sai hình học (TOLERANCE)
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** RGN-161, RGN-004, RGN-007, VN-007, QA-006
- **Đầu vào:** 03 §8 (`TOLERANCE` P1); 07.2 UI-F14 (14 ký hiệu ISO 1101, M/L/S, ⌀, vùng dung sai chiếu, ký hiệu chuẩn); chuỗi mã của TOLERANCE (`%%v`, `^J`, `{\Fgdt;…}`); font gdt tự thiết kế (VN-007).
- **Đầu ra:** `core/regen/dim/ToleranceFrame.{h,cpp}`: tách hàng và ô; khung theo DIMTXT × DIMSCALE và DIMGAP, màu DIMCLRD/DIMCLRT; chữ qua RGN-007 với font gdt; dùng cho entity TOLERANCE và chú thích của LEADER; fuzz `core/regen/fuzz/tolerance_fuzz.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Ảnh khớp TrueView (QA-004) trên ≥ 200 khung của QA-001 và ca vàng phủ 14 ký hiệu, 2 hàng, vùng chiếu, ký hiệu chuẩn **[đề xuất]**.
  2. Chuỗi mã lạ hoặc hỏng: vẽ phần đọc được, không crash; fuzz ≥ 1 giờ.
  3. Đạt DoD-C.

## 12. Leader, MLeader và bảng

### RGN-016 · Leader và MLeader (nội dung MTEXT/block, landing, dogleg, mũi tên)
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** RGN-163, RGN-018, RGN-009, DWG-014, GEO-112
- **Đầu vào:** 05 §8 (Leader/MLeader sinh lại từ dữ liệu ngữ cảnh); 03 §9 (`MLEADER` P0; `LEADER`, `QLEADER` P1); 07.2 UI-D03 (đường thẳng/spline/không, mũi tên, đoạn ngang, nội dung MText/Block, cách gắn chữ); MLEADER, MLEADERSTYLE (DWG-014); nội suy spline qua đỉnh (hàm của GEO-023).
- **Đầu ra:**
  - `core/regen/leader/LeaderRegen.{h,cpp}`: LEADER thẳng/spline, mũi tên DIMLDRBLK, bỏ mũi tên khi đoạn đầu quá ngắn như AutoCAD, hookline, gắn chú thích MTEXT/TOLERANCE/block.
  - `core/regen/leader/MLeaderRegen.{h,cpp}`: dữ liệu ngữ cảnh (nhiều gốc, nhiều nhánh, điểm ngắt), landing gap, dogleg, gắn chữ trái/phải và trên/dưới, khung chữ, nội dung block có attribute, cờ ghi đè thuộc tính so với kiểu, mũi tên riêng từng nhánh.
- **Tiêu chí nghiệm thu:**
  1. Ảnh khớp TrueView (QA-004, SSIM ≥ 0,99) trên ≥ 2.000 MLEADER và ≥ 1.000 LEADER của QA-001 **[đề xuất]**.
  2. Ma trận ca vàng: 3 kiểu đường × nội dung MText/Block/Không × mọi kiểu gắn chữ × 1–5 nhánh.
  3. Jig MLEADER ≤ 0,3 ms/đối tượng p99; 100.000 MLEADER regen ≤ 0,6 s trên 8 nhân **[đề xuất]**.
  4. Đạt DoD-C, DoD-H, DoD-L.

### RGN-017 · Bảng: dùng block đại diện khi xem (V); sinh lại từ dữ liệu ô khi sửa (1.0)
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** RGN-018, DWG-014
- **Đầu vào:** 05 §8 (bảng dùng block đại diện khi chỉ xem); 03 §9 (hiển thị bảng P0); TABLE (DWG-014); block đại diện `*T` (DOC-012).
- **Đầu ra:** `core/regen/table/TableView.{h,cpp}`: vẽ `*T` như block (RGN-018); `*T` thiếu: vẽ lưới hàng/cột và text ô từ giá trị cache, ghi log; hộp bao từng ô cho chọn ô và sửa text trong ô.
- **Tiêu chí nghiệm thu:**
  1. 100% bảng trong ≥ 500 tệp QA-001 có bảng khớp ảnh TrueView (SSIM ≥ 0,99) **[đề xuất]**.
  2. Hộp bao ô trùng lưới hàng/cột lưu trong entity, lệch ≤ 1e-9 × kích thước bảng.
  3. Đạt DoD-C.
- **Ngoài phạm vi:** sinh lại từ dữ liệu ô (RGN-166, mốc 1.0).

### RGN-166 · Sinh lại bảng từ dữ liệu ô và kiểu ô
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** RGN-017, RGN-009, RGN-013, DOC-007, VN-008
- **Đầu vào:** 03 §9 (`TABLE`, `TABLESTYLE`, `TABLEDIT` P1: công thức, gộp ô, block trong ô); 07.2 UI-D04 (kiểu ô Tiêu đề/Đầu cột/Dữ liệu và tùy biến: nền, canh lề, định dạng, lề, chữ, viền đôi); dữ liệu ô TABLE (DWG-014); kiểu bảng TCVN (VN-008).
- **Đầu ra:** `core/regen/table/TableLayout.{h,cpp}`: phân giải kiểu ô (kiểu bảng → hàng/cột → ô); cao hàng tự động theo nội dung (RGN-009); ô gộp; nhiều nội dung trong ô; block trong ô (vừa ô hoặc theo tỉ lệ); định dạng số và ngày (RGN-013); 9 kiểu canh; xoay nội dung; viền theo cạnh (đôi, linetype, lineweight, màu); hướng lên/xuống; chia bảng. Hàm thuần trả nội dung `*T` mới; lệnh ghi qua DOC-007.
- **Tiêu chí nghiệm thu:**
  1. Sinh lại so với `*T` của ≥ 2.000 bảng QA-001: hình học lệch ≤ 1e-9, text khớp 100%, trên ≥ 99% bảng **[đề xuất]**.
  2. Sửa text một ô của bảng 100 × 20: sinh lại ≤ 20 ms tầm trung.
  3. Bảng thống kê thép và bảng diện tích TCVN mẫu khớp ảnh tham chiếu.
  4. Đạt DoD-C, DoD-L.

## 13. Proxy graphics, ảnh raster, PDF underlay

### RGN-020 · Proxy graphics: parser và regen (P1)
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** RGN-001, RGN-002, RGN-007, DWG-016, QA-006
- **Đầu vào:** 05 §8 (Proxy graphics); Đặc tả DWG của ODA (Open Design Specification for .dwg files) v5.4.1, phần proxy entity graphics; khối proxy graphics của proxy entity và entity kèm graphics (DWG-016).
- **Đầu ra:** `core/regen/proxy/ProxyGraphics.{h,cpp}`: parser opcode kiểm biên: hình học (extents, circle, circle 3 điểm, cung, cung 3 điểm, polyline, polygon, mesh, shell, text, text Unicode, xline, ray, lwpolyline, polyline có pháp tuyến); thuộc tính (màu ACI, true color, layer, linetype, lineweight, ltscale, thickness, plot style, marker, fill); push/pop biến đổi; push/pop clip; opcode lạ bỏ qua theo độ dài khai báo. Fuzz `core/regen/fuzz/proxy_fuzz.cpp`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 500 tệp Civil 3D, Architecture, MEP trong QA-001: 100% proxy có graphics hiển thị; ảnh khớp TrueView (SSIM ≥ 0,98) **[đề xuất]**.
  2. Khối hỏng (độ dài sai, push/pop lệch, số đỉnh khổng lồ): dừng an toàn, cấp phát ≤ 64 MB/entity **[đề xuất]**; fuzz ≥ 1 giờ.
  3. 100.000 proxy entity regen ≤ 0,5 s trên 8 nhân **[đề xuất]**.
  4. Đạt DoD-C, DoD-H.

### RGN-021 · Ảnh raster (PNG, JPG, TIFF, BMP, GIF qua QImage): nạp bất đồng bộ, clip, sáng/tương phản, trong suốt
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** RGN-001, RND-015, DWG-013, NT-005, NT-006, NT-011, NT-013
- **Đầu vào:** 03 §12 (`IMAGEATTACH` P0; `IMAGECLIP`, `IMAGEADJUST`, `IMAGEQUALITY`, `IMAGEFRAME` P1); IMAGE, IMAGEDEF (DWG-013); giao diện nguồn ô `render/raster/TileSource.h` (RND-015).
- **Đầu ra:**
  - `core/regen/image/ImageSource.{h,cpp}`: tìm tệp (đường dẫn lưu, tương đối, support path qua NT-011), giải mã nền ưu tiên thấp (NT-005), kim tự tháp mức chia ô 512 px **[đề xuất]**, cài `TileSource`, tiến trình qua NT-013.
  - `core/regen/image/ImageRegen.{h,cpp}`: đặt ảnh theo vector u/v; clip chữ nhật/đa giác/đảo; sáng, tương phản, mờ; ảnh bitonal trong suốt; IMAGEFRAME 0/1/2; IMAGEQUALITY; ảnh thiếu: khung và tên tệp.
- **Tiêu chí nghiệm thu:**
  1. Bản vẽ có 50 ảnh 100 MP: vùng vẽ tương tác ngay, khung ảnh hiện trong 1 khung, ô ảnh đầu ≤ 300 ms tầm trung **[đề xuất]**; không chặn luồng UI.
  2. Pan/zoom không đợi giải mã (dùng mức thô sẵn có); việc giải mã ra khỏi màn hình hủy trong ≤ 50 ms.
  3. Ảnh đã giải mã ≤ ngân sách cấu hình (mặc định 1 GB **[đề xuất]**), loại bỏ LRU.
  4. Hàm CPU tham chiếu sáng/tương phản/mờ (NT-006) trùng shader RND-015 trong ±1/255.
  5. Ảnh hỏng hoặc khổng lồ: báo lỗi, không crash; fuzz đường giải mã ≥ 1 giờ.
  6. Đạt DoD-C, DoD-H.

### RGN-022 · PDF underlay (qua PLT-011): ô render theo mức zoom, layer PDF, clip
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** RGN-021, PLT-011, RND-015
- **Đầu vào:** 03 §12 (`PDFATTACH`, `PDFCLIP`, `PDFLAYERS`, `PDFADJUST` P1; bắt điểm underlay); render ô và trích hình học PDF (PLT-011); `TileSource` (RND-015); tìm tệp và clip của RGN-021.
- **Đầu ra:** `core/regen/underlay/PdfUnderlay.{h,cpp}`: cài `TileSource` theo mức zoom (render nền, hủy khi đổi view, cache LRU RAM và đĩa); bật/tắt layer PDF (OCG) theo entity; clip (cả đảo); tương phản, mờ, đơn sắc; khung; hình học vector của trang đưa vào chỉ mục cho OSNAP underlay (INT-010).
- **Tiêu chí nghiệm thu:**
  1. Zoom vào PDF A0 dày: ô rõ nét ≤ 300 ms sau khi dừng zoom; trong lúc chờ vẫn ≥ 60 fps **[đề xuất]**.
  2. Ảnh khớp PDFium render trực tiếp cùng độ phân giải (SSIM ≥ 0,99).
  3. Tắt một layer PDF: chỉ ô bị ảnh hưởng render lại.
  4. Đạt DoD-C, DoD-H.

## 14. Entity còn lại

### RGN-167 · POINT, SOLID, TRACE, WIPEOUT, XLINE, RAY, 3DFACE
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-001, RGN-002, RGN-027, DOC-003, DOC-010
- **Đầu vào:** README §5.7 (RGN-023); 07.2 UI-D06 (20 kiểu điểm, cỡ theo % màn hình hoặc tuyệt đối); biến PDMODE, PDSIZE, FILLMODE, WIPEOUTFRAME, SPLFRAME (DOC-010); thứ tự vẽ RGN-027.
- **Đầu ra:** `core/regen/entity/`:
  - `PointRegen.cpp`: 20 kiểu PDMODE; PDSIZE dương tuyệt đối, 0 = 5% chiều cao vùng vẽ, âm = % vùng vẽ; cỡ theo màn hình cập nhật liên tục, không đợi REGEN **[đề xuất]**.
  - `SolidTraceRegen.cpp` (thứ tự đỉnh 1-2-4-3, FILLMODE); `Face3dRegen.cpp` (cạnh ẩn theo cờ, SPLFRAME).
  - `WipeoutRegen.cpp`: vùng che bằng màu nền theo ngữ cảnh (model, giấy, in), khung WIPEOUTFRAME 0/1/2.
  - `XlineRayRegen.cpp`: primitive điểm + hướng; cắt theo khung nhìn bằng `double` khi view đổi, không regen.
- **Tiêu chí nghiệm thu:**
  1. Ca vàng mỗi loại × biến liên quan khớp TrueView (QA-004).
  2. XLINE/RAY ở tọa độ VN-2000 (≈ 10⁹): không rung, không mất đoạn khi zoom từ 1e-6 đến 1e6.
  3. 1 triệu POINT regen ≤ 80 ms trên 8 nhân **[đề xuất]**.
  4. Đạt DoD-C, DoD-H.

### RGN-168 · POLYLINE 2D/3D, lưới đa giác, MLINE
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-001, RGN-002, GEO-006, GEO-007, DWG-013
- **Đầu vào:** 07.2 UI-D07 (MLSTYLE: nắp, màu tô, mối nối, thành phần); POLYLINE/VERTEX (DOC-003); MLINESTYLE (DWG-013); bulge và độ rộng GEO-006.
- **Đầu ra:**
  - `core/regen/entity/Polyline2d3dRegen.cpp`: POLYLINE 2D (bulge, độ rộng đầu/cuối từng đoạn tô theo FILLMODE, đỉnh fit và spline-fit, SPLFRAME hiện khung điều khiển); POLYLINE 3D (đỉnh spline-fit); polyface mesh và polygon mesh M×N (cạnh ẩn, kín theo M/N).
  - `core/regen/entity/MlineRegen.cpp`: thành phần theo offset; tham số đoạn lưu trong đỉnh (giữ chỗ cắt của MLEDIT); nắp đường, cung ngoài, cung trong, góc; màu tô; mối nối; căn top/zero/bottom; tỉ lệ.
- **Tiêu chí nghiệm thu:**
  1. Ảnh khớp TrueView (SSIM ≥ 0,99) trên ≥ 300 tệp QA-001 có các loại này **[đề xuất]**.
  2. Polyline rộng có cung: cạnh ngoài là cung giải tích, mối nối khớp AutoCAD trên ca vàng.
  3. 1 triệu đỉnh POLYLINE 2D regen ≤ 100 ms trên 8 nhân **[đề xuất]**.
  4. Đạt DoD-C, DoD-H.

### RGN-023 · Entity còn lại: POINT, SOLID, TRACE, WIPEOUT, XLINE/RAY, 3DFACE, POLYLINE 2D/3D, MLINE, REGION/3DSOLID (khung bao đến P3)
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-167, RGN-168, RGN-003, DOC-003, QA-001, QA-005, QA-006, DWG-168
- **Đầu vào:** README §5.7; khối dữ liệu ACIS SAT/SAB thô của REGION, 3DSOLID, BODY (trường của DOC-003; DWG-017 lo giữ nguyên khi lưu).
- **Đầu ra:** `core/regen/entity/AcisBounds.{h,cpp}`: đọc tối thiểu SAT (văn bản, cả dạng mã hóa của DWG cũ) và SAB (nhị phân): bản ghi điểm và đường cong cơ bản → hộp bao; vẽ khung bao màu entity. Bộ điều phối mọi loại còn lại vào RGN-003; loại chưa hỗ trợ: khung bao nếu có, đếm theo loại, ghi log. Fuzz `core/regen/fuzz/acis_bounds_fuzz.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Kho QA-001 10.000 tệp: 0 entity bị bỏ qua im lặng; báo cáo JSON số lượng theo loại chưa hỗ trợ.
  2. Hộp bao chứa trọn hình 3DSOLID/REGION trong ảnh in TrueView (QA-005) trên ≥ 200 tệp, rộng hơn không quá 5% **[đề xuất]**.
  3. SAT/SAB hỏng: bỏ entity, ghi log; fuzz ≥ 1 giờ.
  4. Đạt DoD-C.

## 15. Viewport và xref

### RGN-169 · Xref trong regen: attach/overlay, làm mờ, XCLIP
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** RGN-156, DOC-013, DOC-005, GEO-003
- **Đầu vào:** 03 §12 (`XREF`, `XATTACH`, `XCLIP` P0; xref lồng); tài liệu xref chỉ đọc nạp vào phiên (DOC-013); bộ lọc clip trong extension dictionary của INSERT (DOC-005); biến XDWGFADECTL, XCLIPFRAME, VISRETAIN.
- **Đầu ra:**
  - `core/regen/xref/XrefRegen.{h,cpp}`: regen model space của xref bằng bảng ký hiệu của xref (layer `XREF|TÊN`, ghi đè VISRETAIN của bản chủ); cache dùng chung cho mọi insert cùng xref; overlay lồng không hiện; chống vòng tham chiếu; xref chưa phân giải hiện tên/đường dẫn tại điểm chèn; làm mờ theo XDWGFADECTL.
  - `core/regen/xref/ClipBoundary.{h,cpp}`: XCLIP cho block và xref (chữ nhật, đa giác, đảo), XCLIPFRAME 0/1/2, mã vùng clip trong primitive (RND-014 dùng stencil), hàm `isPointVisible` cho chọn và OSNAP.
- **Tiêu chí nghiệm thu:**
  1. Ảnh khớp TrueView (SSIM ≥ 0,99) trên ≥ 300 tệp QA-001 có xref lồng ≥ 3 cấp và XCLIP **[đề xuất]**.
  2. Nạp lại một xref: chỉ cache của xref đó vô hiệu; thời gian ≤ regen riêng xref đó + 10% **[đề xuất]**.
  3. `isPointVisible` ≤ 1 µs với biên 1.000 đỉnh **[đề xuất]**; đúng 100% trên 1 triệu điểm ngẫu nhiên so với predicate GEO-003.
  4. Đạt DoD-C, DoD-H.

### RGN-024 · Viewport và xref: tham số view theo viewport, VP Freeze, clip; xref attach/overlay, làm mờ, XCLIP
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** RGN-169, RGN-011, RGN-002, DOC-011, QA-012, QA-005
- **Đầu vào:** 03 §10 (VP Freeze P0; ghi đè thuộc tính layer theo viewport P1); entity VIEWPORT (tâm, cỡ, tâm và chiều cao view, góc xoay, layer đóng băng, biên clip, bật/tắt, tỉ lệ chú thích) (DOC-011); ghi đè theo viewport trong extension dictionary của layer (DOC-005).
- **Đầu ra:** `core/regen/viewport/ViewportRegen.{h,cpp}`: `ViewParams` theo viewport (RGN-001); tập layer ẩn theo viewport; bảng ghi đè màu, linetype, lineweight, transparency, plot style theo viewport cấp cho RGN-002; biên clip đa giác hoặc theo đối tượng; primitive độc lập view dùng chung giữa các viewport, chỉ phần phụ thuộc view (PSLTSCALE, annotative, XLINE/RAY, PDSIZE) sinh theo viewport; giới hạn MAXACTVP; tích hợp xref của RGN-169.
- **Tiêu chí nghiệm thu:**
  1. Cảnh nhiều viewport BENCH-STD (QA-012): regen 16 viewport ≤ 1,3 × thời gian 1 viewport **[đề xuất]**.
  2. Ảnh in layout khớp TrueView (QA-005, SSIM ≥ 0,99) trên ≥ 300 layout QA-001 có VP Freeze, ghi đè viewport, viewport đa giác, xref **[đề xuất]**.
  3. Đổi VP Freeze hoặc ghi đè một layer: cập nhật ≤ 1 khung, không sinh lại hình học.
  4. Đạt DoD-C, DoD-H.

## 16. Field

### RGN-170 · Parser mã field và bộ đánh giá
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** DOC-022, DOC-017, RGN-013, INT-022, QA-006
- **Đầu vào:** 03 §9 (`FIELD`, `UPDATEFIELD` P1, dùng cho khung tên); 07.2 UI-F08 (nhóm và danh sách field, hoa/thường, ngày dd/MM/yyyy, Số tờ / Tổng số tờ); mô hình field DOC-022; metadata thuộc tính DOC-017; DIESEL INT-022.
- **Đầu ra:** `core/regen/field/FieldCode.{h,cpp}`: parser `%<\AcVar …>%`, `%<\AcObjProp …>%`, `%<\AcExpr …>%`, `%<\AcDiesel …>%`, `%<\AcSm …>%`, field lồng, chuỗi định dạng (`%lu`, `%pr`, `%ps`, `%ct`, `%ds`, `%th`, `%zs`, `%au`, `%tc`, mẫu ngày); bộ đánh giá AcVar (ngày, tên tệp, dung lượng, thuộc tính bản vẽ, thông số in), AcObjProp (qua DOC-017), AcExpr (số học, hàm, ô bảng `A1:B3`, Sum/Average/Count), AcDiesel; điểm đăng ký nguồn ngoài cho AcSm (VN-018 cài). Fuzz `core/regen/fuzz/field_fuzz.cpp`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 300 mã tự soạn và ≥ 2.000 field của QA-001: giá trị đánh giá khớp giá trị cache AutoCAD đã lưu (trừ ngày giờ) ≥ 99% **[đề xuất]**.
  2. Định dạng số qua RGN-013 khớp từng ký tự.
  3. Mã hỏng hoặc vòng tham chiếu: hiện `####` như AutoCAD, không crash; fuzz ≥ 1 giờ.
  4. Đạt DoD-C.

### RGN-025 · Field: đánh giá và cập nhật (ngày, tên tệp, thuộc tính đối tượng, công thức)
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** RGN-170, RGN-166, RGN-009, DOC-007
- **Đầu vào:** 03 §9; biến FIELDEVAL (mở, lưu, in, etransmit, regen), FIELDDISPLAY; field trong TEXT, MTEXT, ATTRIB, ô bảng.
- **Đầu ra:** `core/regen/field/FieldUpdate.{h,cpp}`: chỉ mục field theo đối tượng chứa và nguồn phụ thuộc; cập nhật theo FIELDEVAL; ghi giá trị cache qua DOC-007 gom một lô, không tạo mark undo riêng khi tự kích hoạt **[đề xuất]**; dàn lại text chứa field (RGN-009), sinh lại bảng (RGN-166); nền xám FIELDDISPLAY là cờ hiển thị, không in; API `updateFields(selection)` cho lệnh UPDATEFIELD.
- **Tiêu chí nghiệm thu:**
  1. Mở tệp 10.000 field: cập nhật ở nền ≤ 200 ms trên 8 nhân, không chặn luồng UI **[đề xuất]**.
  2. Đổi thuộc tính đối tượng nguồn: field phụ thuộc cập nhật ở lần kích hoạt kế tiếp theo FIELDEVAL; UPDATEFIELD cập nhật ngay, một mark undo.
  3. In hoặc xuất PDF với bit in của FIELDEVAL: ngày in đúng trên từng tờ.
  4. Đạt DoD-C, DoD-H.

## 17. Chú thích theo tỉ lệ

### RGN-026 · Chú thích theo tỉ lệ (annotative): chọn biểu diễn theo tỉ lệ hiện hành
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-021, RGN-009, RGN-012, RGN-015, RGN-016, RGN-018, RGN-024, QA-005
- **Đầu vào:** 03 §8 (kích thước annotative P1; hiển thị đúng bản vẽ có sẵn là P0 **[đề xuất]**); dữ liệu ngữ cảnh theo tỉ lệ DOC-021; tỉ lệ chú thích của model (CANNOSCALE) và của từng viewport; ANNOALLVISIBLE.
- **Đầu ra:** `core/regen/annot/AnnoSelect.{h,cpp}`: chọn ngữ cảnh theo tỉ lệ cho TEXT, MTEXT, kích thước, LEADER/MLEADER, hatch (tỉ lệ mẫu), block và attribute; ANNOALLVISIBLE 0/1; đổi tỉ lệ chỉ sinh lại đối tượng annotative; biểu diễn theo viewport cấp qua RGN-024.
- **Tiêu chí nghiệm thu:**
  1. Đổi tỉ lệ chú thích với 100.000 đối tượng annotative: cập nhật ≤ 100 ms trên 8 nhân **[đề xuất]**; đối tượng khác không regen.
  2. Ảnh khớp TrueView (QA-005) trên ≥ 200 tệp có nhiều tỉ lệ, cả model và layout nhiều viewport **[đề xuất]**.
  3. Đối tượng không hỗ trợ tỉ lệ hiện hành ẩn/hiện đúng theo ANNOALLVISIBLE.
  4. Đạt DoD-C, DoD-H.

## Tóm tắt theo mốc

| Mốc | Task | Số task |
|---|---|---|
| S2 | RGN-001, RGN-101 | 2 |
| S6 | RGN-103, RGN-104, RGN-105, RGN-004, RGN-106, RGN-107, RGN-005, RGN-108, RGN-007, RGN-109 | 10 |
| V | RGN-102, RGN-002, RGN-006, RGN-008, RGN-111, RGN-112, RGN-113, RGN-114, RGN-009, RGN-013, RGN-117, RGN-118, RGN-003, RGN-027, RGN-028, RGN-151, RGN-152, RGN-011, RGN-153, RGN-154, RGN-012, RGN-156, RGN-018, RGN-019, RGN-161, RGN-162, RGN-163, RGN-014, RGN-164, RGN-015, RGN-165, RGN-016, RGN-017, RGN-020, RGN-021, RGN-167, RGN-168, RGN-023, RGN-169, RGN-024, RGN-026 | 41 |
| EA | RGN-155, RGN-166 | 2 |
| 1.0 | RGN-010, RGN-157, RGN-158, RGN-159, RGN-160, RGN-022, RGN-170, RGN-025 | 8 |
| **Tổng** | | **63** |
