# VN · Gói Việt Nam: Bác sĩ Font, font SHX, nội dung TCVN, công cụ VN, công cụ miễn phí

Phạm vi: Bác sĩ Font (bảng mã, nhận diện, chế độ hiển thị, chế độ chuyển đổi, chuyển hàng loạt), bộ font SHX tự thiết kế, nội dung TCVN, lệnh TC…, cầu nối Excel, công cụ miễn phí CC chạy trên AutoCAD, ZWCAD, BricsCAD.
Thư mục mã: `vn/`, `content/tcvn/`, `tools/cc/`. Giao diện của các tính năng này thuộc UIJ, UIN ([ui-3](ui-3-vn-tudong-hethong.md)).
Căn cứ: [05](../05-kien-truc-ky-thuat.md) §13, §15, §21; [04](../04-chien-luoc-san-pham.md) §7; [03](../03-chuc-nang-autocad.md) §7; [07.3](../07-dac-ta-ui-3-tinh-nang-vn-va-he-thong.md) §1, §7.

## 0. Ghi chú thiết kế

- **Thư mục [đề xuất]:** `vn/encoding/` (bảng mã, chuỗi CAD), `vn/detect/` (nhận diện, mô hình âm tiết, bộ mẫu), `vn/fontdoctor/` (quét, hiển thị, chuyển đổi, hàng loạt), `vn/fonts/` (nguồn glyph, toolchain, font đóng gói), `tools/cc/` (công cụ miễn phí). Lệnh TC… và Excel dùng thư mục con riêng trong `vn/`; nội dung TCVN ở `content/tcvn/`.
- **Ranh giới** (README §4.2): VN làm engine, lệnh và dữ liệu; UIJ, UIN chỉ làm giao diện. Tệp `.dcl` và hợp đồng khóa tile của CC nằm ở `app/vn/cc/` (UI-N01, UI-N02); mã LISP, lệnh LISP và gói cài đặt CC thuộc VN.
- **Một nguồn dữ liệu:** bảng mã, bảng tên font, mô hình âm tiết, ca vàng là tệp dữ liệu trong `vn/`. Bảng C++ và bảng LISP đều sinh từ đó lúc build, không chép tay ở hai nơi **[đề xuất]**.
- **Unicode ghi ra luôn ở dạng NFC** (NT-012). So sánh tên font, tên kiểu chữ không phân biệt hoa thường.
- **Không đổi dữ liệu ngầm:** chế độ hiển thị chỉ giải mã khi vẽ. Mở rồi lưu mà không chạy lệnh chuyển thì mọi chuỗi giữ nguyên từng byte. Mọi thay đổi bản vẽ đi qua lệnh và DOC-007.
- **Nhận diện bảo thủ:** thà trả "Không rõ" còn hơn chuyển nhầm chữ Unicode đúng. Tỉ lệ chuyển nhầm chữ Unicode ≤ 0,5% **[đề xuất]**. Ngưỡng tin cậy mặc định 80/100 (prototype `js/palettes.js`, `FD.settings`).
- **Cổng phiên bản** (07.3 UI-J02, qua PLF-002 và INT-020): quét và hiển thị ở V; chuyển đổi lưu bản sao và chuyển hàng loạt ở VP; chuyển trực tiếp trong bản vẽ ở EA.
- **Pháp lý** (05 §15): font SHX, `.pat`, `.lin` tự thiết kế, giữ tên chuẩn. Không đọc, chép, đóng gói font của Autodesk hay font SHX tiếng Việt của bên thứ ba. Metric tham chiếu chỉ đo hộp đen trên ứng dụng có cài font gốc, chỉ lưu số đo.
- **Đầu vào không tin cậy** (DoD-C mục 7): SHX, PAT, LIN, bảng thay thế font, gói thư viện, XLSX, CSV do người dùng đưa vào.
- **Công cụ CC** chạy trong AutoCAD (đầy đủ và LT 2024+), ZWCAD, BricsCAD: chỉ AutoLISP thuần và DCL, không `vla-*`, `vlax-*`, `vlr-*`, không ARX, để một mã chạy trên mọi ứng dụng chủ **[đề xuất]**. Với mã LISP, DoD-C áp dụng mục 2, 4, 7, 10. Tên lệnh trùng TrionCAD nhưng dòng **Lệnh:** chỉ ghi ở task TrionCAD; task CC ghi "bản LISP".

## 1. Bảng mã và chuỗi CAD (S4)

### VN-101 · Codec TCVN3, VNI-Windows, VISCII sinh từ bảng dữ liệu
- **Mốc:** S4 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-001, NT-012
- **Đầu vào:** 05 §13; TCVN 5712:1993 (bảng mã VN3); RFC 1456 (VISCII); bảng mã VNI-Windows do VNI công bố.
- **Đầu ra:**
  - `vn/encoding/data/{tcvn3,vni,viscii}.tsv`: byte hoặc chuỗi byte ↔ code point, có cột nguồn.
  - Script sinh bảng C++ lúc build `vn/encoding/gen/`; `vn/encoding/codecs.{h,cpp}`: giải mã và mã hóa từng bảng mã; `vn/encoding/tests/`.
- **Tiêu chí nghiệm thu:**
  1. Khớp 100% bảng vàng độc lập: VISCII và VNI đủ 134 chữ cái có dấu (kể cả đ, Đ); TCVN3 đủ chữ thường có dấu và Ă Â Ê Ô Ơ Ư Đ.
  2. VNI: giải đúng chuỗi 2 byte (nguyên âm + byte dấu); byte dấu đứng lẻ, đứng đầu chuỗi hay sau phụ âm được giữ và gắn cờ, không mất byte.
  3. VISCII: 6 chữ hoa ở vị trí điều khiển C0 (0x02, 0x05, 0x06, 0x14, 0x19, 0x1E) giải đúng; byte ngoài bảng trả cờ lỗi, không ném ngoại lệ.
  4. Mã hóa rồi giải mã cho lại chuỗi gốc trên mọi ký tự trong bảng và 10⁶ chuỗi ngẫu nhiên (seed cố định).
  5. Đạt DoD-C.

### VN-001 · Bảng mã TCVN3, VNI-Windows, VISCII, Unicode dựng sẵn và tổ hợp; chuyển hai chiều
- **Mốc:** S4 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-101, NT-012, NT-006, NT-003, DWG-028
- **Đầu vào:** 05 §13; API VN-101; NT-012 (code page, NFC); DWG-028 (giải mã chuỗi theo DWGCODEPAGE).
- **Đầu ra:** `vn/encoding/encoding.h`: `enum class Encoding`, `toUnicode(bytes, enc, CaseHint)`, `fromUnicode(text, enc)`, `recoverLegacyBytes(text, codepage)` (lấy lại byte gốc từ chuỗi đã giải theo code page ANSI, gồm chuỗi DWG 2007+); đường nhanh SIMD cho đoạn ASCII; benchmark `vn/encoding/bench/`.
- **Tiêu chí nghiệm thu:**
  1. Chữ thường TCVN3 dưới font `.Vn*H` (CaseHint hoa) ra chữ hoa Unicode, khớp 100% bảng vàng.
  2. `recoverLegacyBytes` khôi phục đúng 100% byte 0x80–0xFF qua code page 1252, 1258 và mọi code page DWG-028 hỗ trợ, kể cả vị trí không định nghĩa; `\U+xxxx`, `\M+` giữ nguyên.
  3. Unicode tổ hợp (dấu U+0300, U+0301, U+0303, U+0309, U+0323 tách rời) ra NFC khớp NT-012 trên 10⁶ chuỗi ngẫu nhiên.
  4. 1 luồng, máy tầm trung: ≥ 1 GB/s với ASCII thuần (SIMD, trùng kết quả đường scalar), ≥ 200 MB/s với chữ có dấu **[đề xuất]**; 0 cấp phát trong vòng lặp.
  5. Đạt DoD-C, DoD-H.

### VN-102 · Chuỗi CAD: mã `%%`, `\U+`, `\M+`, mã định dạng MTEXT; chuyển đoạn chữ, giữ nguyên mã
- **Mốc:** S4 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-001
- **Đầu vào:** 05 §13 (xử lý mã `%%`, mã định dạng MTEXT); 03 §7 (`%%c %%d %%p %%u %%o`, `\U+xxxx`); danh mục mã MTEXT ở README §5.7 (RGN-008).
- **Đầu ra:**
  - `vn/encoding/cadtext.{h,cpp}`: tách token (chữ, `%%x`, `%%nnn`, `\U+`, `\M+`, mã định dạng, `{}`, xếp chồng `\S…;`, đổi font `\f…;` `\F…;`); `splitRuns()` theo font; `convertCadString()` chỉ đổi token chữ và tên font trong `\f`.
  - Ca vàng `vn/encoding/testdata/cad-strings.tsv` (≥ 500 ca), dùng chung với bản LISP.
- **Tiêu chí nghiệm thu:**
  1. Ghép lại token khi không chuyển cho đúng chuỗi gốc từng byte trên 10⁶ chuỗi ngẫu nhiên có mã xen kẽ.
  2. Khớp 100% ca vàng: token không phải chữ giữ nguyên; chữ trong xếp chồng được chuyển, ký tự `^ / #` giữ nguyên; `\f` mang font cũ đổi sang font đích, giữ `|b|i|c|p`.
  3. Mã sai cú pháp (`%%` cụt, `\` cuối chuỗi, ngoặc lệch) giữ nguyên và gắn cờ, không ném ngoại lệ.
  4. Tách token ≥ 300 MB/s 1 luồng, máy tầm trung **[đề xuất]**.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** cây định dạng và dàn trang MTEXT (RGN-008, RGN-009).

## 2. Nhận diện bảng mã và bộ mẫu

### VN-103 · Heuristic tên font: `.Vn*`, `VNI-*`, SHX tiếng Việt đã biết
- **Mốc:** S4 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** NT-012, QA-001, DWG-030
- **Đầu vào:** 05 §13 mục 1; bảng STYLE của kho QA-001 qua `dwgdump` (tên tệp font, bigfont, typeface TTF trong xdata).
- **Đầu ra:** `vn/detect/data/font_names.tsv` (mẫu tên → bảng mã, cờ hoa, họ font gợi ý); `vn/detect/font_hint.{h,cpp}`: `classifyFont(file, bigFont, typeface)`; công cụ khai thác tên font từ kho `vn/detect/tools/mine_fonts`.
- **Tiêu chí nghiệm thu:**
  1. `.VnTime`, `.VnArial`, `.VnTimeH`… → TCVN3 (hậu tố `H` → cờ hoa); `VNI-Times`, `VNI-Helve`… → VNI; khớp 100% bảng vàng ≥ 200 tên.
  2. Nhận đúng tên tệp TTF, typeface trong xdata, SHX có hoặc không đuôi, đường dẫn đầy đủ, khác hoa thường.
  3. Mọi tên SHX xuất hiện ở ≥ 5 tệp của kho QA-001 được gán bảng mã hoặc ghi "Không rõ" kèm lý do **[đề xuất]**.
  4. Tên không có trong bảng trả "Không rõ", độ tin cậy 0, không đoán.
  5. Đạt DoD-C.

### VN-104 · Mô hình âm tiết tiếng Việt và n-gram từ thuật ngữ xây dựng
- **Mốc:** S4 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-001, VN-102, QA-001, DWG-030
- **Đầu vào:** 05 §13 mục 3; ngữ liệu Unicode có quyền dùng: chữ trong kho QA-001 (ghi chú, khung tên, thuyết minh), danh mục thuật ngữ xây dựng; quy tắc cấu tạo âm tiết (âm đầu, âm đệm, âm chính, âm cuối, thanh).
- **Đầu ra:** `vn/detect/syllable.{h,cpp}` (tách và kiểm âm tiết hợp lệ); `vn/detect/ngram.{h,cpp}` (log-xác suất unigram/bigram âm tiết, có làm trơn); bộ dựng `vn/detect/tools/build_model`; `vn/detect/model/vn-syllable.bin` ≤ 2 MB và bản rút gọn cho LISP ≤ 150 KB **[đề xuất]**.
- **Tiêu chí nghiệm thu:**
  1. Nhận ≥ 99,9% âm tiết của từ điển chuẩn (≥ 6.000 âm tiết); loại ≥ 99% âm tiết sinh ra khi giải chéo bảng mã (TCVN3 đọc như VNI và ngược lại) **[đề xuất]**.
  2. Đúng quy tắc thanh với âm cuối p, t, c, ch (chỉ sắc, nặng); nhận cả kiểu bỏ dấu cũ và mới (`hoà`, `hòa`).
  3. Dựng mô hình tất định: cùng ngữ liệu cho cùng tệp `.bin` từng bit.
  4. Chấm một âm tiết p50 ≤ 50 ns, máy tầm trung, 0 cấp phát **[đề xuất]**.
  5. Đạt DoD-C.

### VN-003 · Bộ mẫu gán nhãn (1.000 mẫu cho S4) và công cụ đo độ chính xác
- **Mốc:** S4 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-001, VN-102, VN-103, QA-001, DWG-030
- **Đầu vào:** 05 §21 (S4: ≥ 95% trên 1.000 mẫu, C++ và LISP); kho QA-001 bộ 1.000 tệp; `dwgdump`.
- **Đầu ra:**
  - `vn/detect/samples/`: mỗi mẫu có byte gốc (hex), font, bigfont, loại đối tượng, vị trí đoạn MTEXT, nhãn bảng mã, cờ hoa, chuỗi Unicode đáp án, tệp nguồn, người duyệt.
  - `vn/detect/tools/extract_samples` (trích, đề xuất nhãn theo tên font, xuất CSV để duyệt); `vn/detect/tools/eval` (nhận JSON kết quả của bản C++ hoặc LISP; ma trận nhầm lẫn, độ chính xác từng lớp, tỉ lệ chuyển nhầm Unicode; JSON cho QA-010).
- **Tiêu chí nghiệm thu:**
  1. 1.000 mẫu từ ≥ 200 tệp; TCVN3, VNI, Unicode mỗi lớp ≥ 200 mẫu; VISCII và "Không rõ" ≥ 50 mẫu, kho thiếu thì bổ sung mẫu tự tạo có gắn cờ **[đề xuất]**.
  2. Đủ loại: TEXT, đoạn MTEXT, ATTRIB, ghi đè text của dim, ô bảng, chữ hoa `.Vn*H`.
  3. Mỗi nhãn có 2 người duyệt khác nhau (kiểm trong CI); 10% mẫu gán lại độc lập, đồng thuận ≥ 99% **[đề xuất]**.
  4. Tập đánh giá tách hẳn: không mẫu nào thuộc tệp đã dùng làm ngữ liệu VN-104 (kiểm bằng hash tệp).
  5. Công cụ đo chạy lại cho cùng số liệu.
  6. Đạt DoD-C.

### VN-002 · Nhận diện bảng mã theo đối tượng và theo đoạn MTEXT
- **Mốc:** S4 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-001, VN-102, VN-103, VN-104, VN-003, NT-006, NT-003
- **Đầu vào:** 05 §13 (4 bước nhận diện); 07.3 UI-J02 (lớp "Không rõ", độ tin cậy), UI-J04 (ngưỡng).
- **Đầu ra:** `vn/detect/detector.{h,cpp}`: `detect(const TextRun&, const FontPrior*)` trả bảng mã, độ tin cậy 0–100, tín hiệu đã dùng, ứng viên thứ hai; đoạn MTEXT tách bằng `splitRuns` (VN-102); benchmark `vn/detect/bench/`.
- **Tiêu chí nghiệm thu:**
  1. Độ chính xác ≥ 95% trên 1.000 mẫu VN-003 (cổng S4); tỉ lệ chuyển nhầm chữ Unicode ≤ 0,5% **[đề xuất]**.
  2. Kết hợp đủ 4 tín hiệu của 05 §13; dưới ngưỡng (tham số, mặc định 80) trả "Không rõ" kèm ứng viên.
  3. MTEXT nhiều font: mỗi đoạn một kết quả; đoạn ngắn (< 3 âm tiết) dùng thống kê theo font (`FontPrior`) do bên gọi đưa, không có thì hạ độ tin cậy.
  4. ASCII thuần trả Unicode ngay; đối tượng ≤ 256 byte: p50 ≤ 1 µs, p99 ≤ 10 µs, máy tầm trung, 0 cấp phát **[đề xuất]**.
  5. Kết quả tất định, không phụ thuộc thứ tự gọi.
  6. Đạt DoD-C, DoD-H.

### VN-105 · Mở rộng bộ mẫu lên 5.000 mẫu, đo trong CI
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** VN-002, VN-003, QA-001, QA-010
- **Đầu vào:** VN-003 (định dạng mẫu, quy trình duyệt); kho QA-001 bộ 10.000 tệp; danh sách nhận diện sai của bản S4.
- **Đầu ra:** thêm 4.000 mẫu vào `vn/detect/samples/` (thêm block lồng, bảng, mleader, field, VISCII, Unicode tổ hợp, chữ hoa `.Vn*H`); job CI đo bản C++ và bản LISP, đăng kết quả lên QA-010.
- **Tiêu chí nghiệm thu:**
  1. 5.000 mẫu từ ≥ 1.000 tệp; mỗi lớp bảng mã và mỗi loại đối tượng ≥ 100 mẫu **[đề xuất]**.
  2. Duyệt như VN-003, đồng thuận ≥ 99%; 1.000 mẫu cũ giữ nguyên, không trùng mẫu (hash chuỗi + tệp).
  3. PR chạm `vn/detect/` hay `tools/cc/` tự chạy đo; độ chính xác giảm > 0,5 điểm phần trăm thì CI chặn merge **[đề xuất]**.
  4. Đạt DoD-C.

### VN-106 · Đạt độ chính xác nhận diện ≥ 98% trên 5.000 mẫu
- **Mốc:** 1.0 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-002, VN-104, VN-105
- **Đầu vào:** 05 §13 (mục tiêu ≥ 98% trên 5.000 mẫu); ma trận nhầm lẫn của VN-105; ngữ liệu thuật ngữ mở rộng.
- **Đầu ra:** mô hình VN-104 dựng lại với ngữ liệu mở rộng; trọng số kết hợp tín hiệu học tất định (hồi quy logistic, seed cố định) trên tập huấn luyện tách khỏi 5.000 mẫu đánh giá **[đề xuất]**; hiệu chỉnh độ tin cậy; cập nhật bản rút gọn cho LISP.
- **Tiêu chí nghiệm thu:**
  1. ≥ 98% trên 5.000 mẫu (bản C++); không lớp bảng mã nào dưới 95% **[đề xuất]**.
  2. Tỉ lệ chuyển nhầm chữ Unicode ≤ 0,2% **[đề xuất]**.
  3. Độ tin cậy được hiệu chỉnh: mẫu có độ tin cậy ≥ 80 đúng ≥ 99% **[đề xuất]**.
  4. Giữ ngân sách thời gian của VN-002; mô hình vẫn ≤ 2 MB.
  5. Đạt DoD-C.

## 3. Công cụ miễn phí CC (bản LISP)

### VN-107 · Harness chạy LISP trên AutoCAD, AutoCAD LT, ZWCAD, BricsCAD trong CI
- **Mốc:** S4 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** NT-002
- **Đầu vào:** 07.3 §7 (kiểm trên AutoCAD đầy đủ và LT 2024+, ZWCAD, BricsCAD); runner Windows tự host của NT-002 có bản quyền các ứng dụng chủ.
- **Đầu ra:** `tools/cc/ci/`: chạy một tệp `.lsp` trên bản vẽ đầu vào trong từng ứng dụng chủ, không cần người bấm (AutoCAD Core Console nếu có, còn lại khởi động kèm script); hồ sơ sạch mỗi lần chạy; thu JSON kết quả do LISP ghi, nhật ký, thời gian; job CI theo yêu cầu và hằng đêm.
- **Tiêu chí nghiệm thu:**
  1. Một ca kiểm `tools/cc/tests/*.lsp` chạy được trên 4 ứng dụng chủ, JSON cùng lược đồ.
  2. Ca treo hoặc hộp thoại bất ngờ bị dừng sau 120 s **[đề xuất]**, có ảnh chụp và nhật ký; runner không treo.
  3. Chạy lại 10 lần cùng ca cho cùng kết quả.
  4. Đạt DoD-C.

### VN-108 · Lõi nhận diện và chuyển mã bản AutoLISP
- **Mốc:** S4 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-002, VN-101, VN-102, VN-103, VN-104, VN-107
- **Đầu vào:** 05 §21 (S4: nhận diện và chuyển mã cả C++ và LISP); dữ liệu `vn/encoding/data/`, `vn/detect/data/`, mô hình rút gọn VN-104; ca vàng `cad-strings.tsv` (VN-102); công cụ đo VN-003.
- **Đầu ra:**
  - Script sinh `tools/cc/lisp/tc-tables.lsp` từ dữ liệu `vn/` lúc build.
  - `tools/cc/lisp/tc-core.lsp`: giải mã, mã hóa, tách token chuỗi CAD, nhận diện (tên font, byte, âm tiết) trả bảng mã và độ tin cậy.
  - Tự dò chế độ chuỗi của ứng dụng chủ (Unicode hay code page ANSI); ở chế độ ANSI ghi chữ Việt bằng `\U+xxxx` **[đề xuất]**.
- **Tiêu chí nghiệm thu:**
  1. ≥ 95% trên 1.000 mẫu VN-003, đo bằng công cụ của VN-003 trên AutoCAD 2024+ (cổng S4); trùng kết quả bản C++ ở ≥ 99% mẫu **[đề xuất]**.
  2. Khớp 100% ca vàng chuỗi CAD trên cả 4 ứng dụng chủ.
  3. Kiểm tĩnh: không gọi `vla-*`, `vlax-*`, `vlr-*`.
  4. Nhận diện 10.000 đối tượng chữ ≤ 10 s trên AutoCAD, máy tầm trung; `tc-tables.lsp` ≤ 300 KB **[đề xuất]**.
  5. Đạt DoD-C.

### VN-109 · Bác sĩ Font bản LISP: quét, chuyển, báo cáo trên AutoCAD, ZWCAD, BricsCAD
- **Mốc:** CC · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-108, UI-N01
- **Đầu vào:** 07.3 UI-N01; `app/vn/cc/fixfont.dcl` và hợp đồng khóa `app/vn/cc/fixfont-keys.md` (UI-N01); API `tc-core.lsp`.
- **Đầu ra:**
  - `tools/cc/lisp/fixfont.lsp`: lệnh FIXFONT bản LISP. Quét cả bản vẽ hoặc vùng chọn; đếm font thiếu (`findfile`) và đối tượng theo TCVN3/VNI/Unicode. Chuyển TEXT, MTEXT, ATTRIB, ATTDEF, ghi đè text của dim, block lồng; ép bảng mã nguồn; font đích; cập nhật kiểu chữ; báo cáo trên dòng lệnh.
  - `tools/cc/docs/fixfont.md`: prompt, tùy chọn, giới hạn.
- **Tiêu chí nghiệm thu:**
  1. 100 bản vẽ thử (TCVN3, VNI, trộn, block lồng 3 cấp): chuỗi sau chuyển trùng 100% kết quả bản C++ (VN-001, VN-102) với cùng bảng mã nguồn, trên 4 ứng dụng chủ.
  2. Một lần chuyển là một nhóm UNDO; `U` trả mọi `entget` về như cũ. Esc giữa chừng: `*error*` đóng nhóm và hoàn tác, không để bản vẽ dở.
  3. Bảng, field, mleader, xref không bị sửa; số đối tượng bỏ qua ghi trên dòng lệnh **[đề xuất]**.
  4. 10.000 đối tượng chữ chuyển ≤ 30 s trên AutoCAD, máy tầm trung **[đề xuất]**; báo tiến trình mỗi 1.000 đối tượng.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** tệp `.dcl` (UI-N01); lệnh FIXFONT của TrionCAD (VN-005).

### VN-110 · Thư viện TCVN bản LISP: chỉ mục, tìm kiếm, chèn theo tỉ lệ, gói nội dung CC
- **Mốc:** CC · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-107, UI-N02
- **Đầu vào:** 07.3 UI-N02 (danh mục, slide, tìm kiếm, chèn theo tỉ lệ; mẫu, CTB, layer, hatch đi kèm); `app/vn/cc/tclib.dcl`, `app/vn/cc/tclib-keys.md` (UI-N02); tệp nội dung do kỹ sư nội dung soạn trong ứng dụng chủ.
- **Đầu ra:**
  - `tools/cc/lisp/tclib.lsp`: lệnh TCLIB bản LISP. Duyệt danh mục, tìm không dấu, slide trong tile `image`, chèn theo tỉ lệ bản vẽ.
  - `tools/cc/content/`: block `.dwg`, mẫu `.dwt` A3 và A1, `TCVN.ctb`, bộ layer, `.pat` và `.lin` tự viết giữ tên chuẩn; chỉ mục `tclib-index.lsp` và slide `.sld`/`.slb` sinh tự động (MSLIDE qua harness VN-107).
  - `tools/cc/docs/tclib.md`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 50 block **[đề xuất]**; chỉ mục khớp 100% tệp trong `tools/cc/content/`; block nào cũng có slide, tên tiếng Việt, từ khóa không dấu.
  2. Gõ "mat cat" tìm ra "Mặt cắt"; 500 mục trả kết quả ≤ 200 ms trên AutoCAD **[đề xuất]**.
  3. Block vẽ theo mm giấy chèn ra đúng kích thước ở tỉ lệ 1:50 và 1:100 (kiểm hộp bao), tôn trọng `INSUNITS`.
  4. `.pat`, `.lin`, `TCVN.ctb`, `.dwt` nạp không lỗi trên 4 ứng dụng chủ.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** lệnh TCLIB của TrionCAD và nội dung TCVN đầy đủ (VN-008, VN-009).

### VN-111 · Lệnh cài đặt TCINSTALL, TCUNINSTALL bản LISP và gói ZIP công cụ miễn phí
- **Mốc:** CC · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** VN-109, VN-110
- **Đầu vào:** 07.3 UI-N03 (gói ZIP, lệnh cài thêm đường dẫn hỗ trợ và Startup Suite); prototype `cong-cu-mien-phi.html` mục UI-N03 (cấu trúc gói, các bước của TCINSTALL, TCUNINSTALL).
- **Đầu ra:**
  - `tools/cc/lisp/tcinstall.lsp`: TCINSTALL hỏi thư mục; thêm vào đường dẫn hỗ trợ, `TRUSTEDPATHS`, danh sách nạp khi khởi động theo cơ chế của từng ứng dụng chủ; chép `TCVN.ctb` vào thư mục plot style. TCUNINSTALL gỡ sạch.
  - `tools/cc/build/`: dựng ZIP tái lập (LISP, DCL lấy từ `app/vn/cc/`, nội dung, hướng dẫn tiếng Việt, tệp phiên bản, SHA-256); `tools/cc/docs/tcinstall.md`.
- **Tiêu chí nghiệm thu:**
  1. Hồ sơ sạch trên 4 ứng dụng chủ: TCINSTALL, khởi động lại, FIXFONT và TCLIB chạy được, không hỏi về tệp chưa ký số.
  2. Chạy TCINSTALL 2 lần không nhân đôi mục; TCUNINSTALL trả đường dẫn hỗ trợ, `TRUSTEDPATHS`, danh sách nạp về đúng trạng thái trước khi cài (so ảnh chụp cấu hình).
  3. Thư mục cài có dấu tiếng Việt và khoảng trắng chạy đúng.
  4. ZIP dựng tái lập (cùng nguồn, cùng hash), ≤ 20 MB **[đề xuất]**.
  5. Đạt DoD-C.

### VN-017 · Công cụ miễn phí CC: FIXFONT và TCLIB bản LISP + DCL chạy trên AutoCAD, ZWCAD, BricsCAD; gói cài đặt
- **Mốc:** CC · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** VN-111, UI-N01, UI-N02
- **Đầu vào:** 07.3 §7 (UI-N01 đến UI-N03); ma trận ứng dụng chủ: AutoCAD 2024+ đầy đủ và LT, ZWCAD, BricsCAD (bản cụ thể chốt khi phát hành).
- **Đầu ra:** kịch bản đầu-cuối `tools/cc/tests/e2e/`; bản phát hành CC 1.0 (ZIP, ghi chú phát hành tiếng Việt, ma trận tương thích); quy trình phát hành bản sửa lỗi `tools/cc/RELEASE.md`.
- **Tiêu chí nghiệm thu:**
  1. Kịch bản đầu-cuối trên 4 ứng dụng chủ qua VN-107: cài → FIXFONT quét và chuyển 50 bản vẽ thử → TCLIB chèn 20 block → gỡ; đạt 100%.
  2. Mọi khóa tile mà LISP gọi đều có trong tệp `.dcl` của UI-N01, UI-N02, đúng kiểu giá trị theo hợp đồng khóa (kiểm tĩnh).
  3. Bản LISP giữ ≥ 95% trên 1.000 mẫu VN-003.
  4. 50 bản vẽ đặc biệt từ QA-001 (thiếu font, kiểu chữ rỗng, chuỗi rỗng, block lồng sâu) không gây lỗi LISP chưa bắt; thông báo lỗi bằng tiếng Việt.
  5. Đạt DoD-C.

## 4. Bộ font SHX và font thay thế (V)

### VN-112 · Toolchain SHX: nguồn glyph của người thiết kế → mã shape → tệp `.shx`
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** RGN-004, DWG-024
- **Đầu vào:** 05 §13 (font đi kèm), §15; tài liệu công khai của AutoCAD về định nghĩa shape và font shape (Customization Guide); parser SHX của RGN-004 để đọc lại kết quả.
- **Đầu ra:**
  - Định dạng nguồn **[đề xuất]**: mỗi font một tệp DXF, mỗi glyph một block tên `U+XXXX`, nét LINE, ARC, LWPOLYLINE trên lưới; dấu là block thành phần dùng chung. Người thiết kế vẽ glyph, task không vẽ glyph.
  - `vn/fonts/toolchain/`: chuyển nét → mã vector (độ dài và hướng, cung bát phân, cung bulge, nhấc/hạ bút, push/pop, subshape); `shxc` biên dịch ra shape font, bigfont, unifont; xuất `.shp` trung gian đọc được.
- **Tiêu chí nghiệm thu:**
  1. Mọi `.shx` sinh ra được RGN-004 đọc lại; nét giải ra khớp nét nguồn, sai số ≤ 1/2 đơn vị lưới **[đề xuất]**.
  2. Chữ có dấu dựng bằng subshape (chữ gốc + dấu); sửa một dấu thì mọi glyph dùng nó đổi theo.
  3. Biên dịch tất định (cùng nguồn, cùng byte); cả bộ font ≤ 10 s **[đề xuất]**.
  4. Lỗi nguồn (nét vượt giới hạn mã vector, code point trùng, thiếu thành phần) báo tên glyph và vị trí, không sinh tệp.
  5. Đạt DoD-C.

### VN-113 · Kiểm phủ glyph tiếng Việt và kiểm metric của bộ font
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-001, VN-107, VN-112, RGN-004
- **Đầu vào:** 05 §13 (tương thích tên và kích thước), §15; ứng dụng chủ trên runner VN-107 có cài font gốc, chỉ dùng để đo.
- **Đầu ra:**
  - `vn/fonts/data/required-chars.tsv`: ký tự bắt buộc theo loại font (ASCII in được; 134 chữ có dấu kể cả đ, Đ; dấu tổ hợp; glyph cho `%%c`, `%%d`, `%%p`; ký hiệu GD&T cho `gdt`; bố cục byte TCVN3 cho font SHX tiếng Việt mã TCVN3).
  - `vn/fonts/tests/coverage`, `vn/fonts/tests/metrics`; bảng metric tham chiếu đo hộp đen bằng `textbox` của AutoLISP (bề rộng ký tự c = rộng("x"+c+"x") − rộng("xx")).
- **Tiêu chí nghiệm thu:**
  1. 100% ký tự bắt buộc có glyph ở mọi font của bộ; thiếu thì CI chặn merge, báo font và code point.
  2. Font tương thích tên giữ bố cục mã của font gốc: với ≥ 50 chuỗi vàng mỗi font, nhãn code point của glyph tại từng byte trùng kết quả giải mã VN-001.
  3. Bề rộng từng ký tự lệch ≤ 2% chiều cao chữ; bề rộng 200 chuỗi mẫu lệch ≤ 1% **[đề xuất]**.
  4. Bảng tham chiếu chỉ chứa số đo; kiểm tự động không có tệp `.shx`, `.shp`, `.ttf` của bên thứ ba trong kho.
  5. Đạt DoD-C.

### VN-114 · Font TTF thay thế TrionVN-Times, TrionVN-Arial từ font SIL OFL
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** VN-113, RGN-005
- **Đầu vào:** 04 §7.1 (font Unicode SIL OFL làm font thay thế); 07.3 UI-J02 (font đích TrionVN-Times, TrionVN-Arial); SIL Open Font License 1.1.
- **Đầu ra:** `vn/fonts/ttf/`: script dựng tái lập từ bản phát hành OFL ghim phiên bản, chọn họ font có metric tương thích Times New Roman và Arial, đủ tiếng Việt (ví dụ Liberation 2.x) **[đề xuất]**; đổi tên họ font, kèm tệp OFL và ghi nguồn; mục SBOM.
- **Tiêu chí nghiệm thu:**
  1. Phủ 100% ký tự bắt buộc của VN-113 (kiểm cmap qua RGN-005).
  2. Bề rộng 200 chuỗi tiếng Việt mẫu lệch ≤ 1% so với Times New Roman, Arial **[đề xuất]**; chỉ lưu số đo.
  3. Tên họ mới không dùng Reserved Font Name của bản gốc; build tái lập (cùng hash).
  4. Đạt DoD-C.

### VN-007 · Bộ font SHX tự thiết kế, tương thích tên: biên dịch, kiểm phủ glyph tiếng Việt, kiểm metric
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-112, VN-113, RGN-006
- **Đầu vào:** 05 §13 (vntime, vnarial, romansvn, txt, simplex, romans, isocp, gdt…), §15; glyph do người thiết kế vẽ theo định dạng nguồn VN-112 trong `vn/fonts/src/`.
- **Đầu ra:** `vn/fonts/shx/` sinh từ nguồn khi build: các font trên và TrionVN-Times, TrionVN-Arial dạng unifont **[đề xuất]**; bảng tương thích tên cho RGN-006 (tên font thiếu → font cùng tên của bộ); thư mục font và manifest cho trình cài đặt (PLF-009), tệp giấy phép, mục SBOM.
- **Tiêu chí nghiệm thu:**
  1. Mọi font trong danh sách đạt kiểm phủ và kiểm metric VN-113.
  2. 300 bản vẽ QA-001 dùng các font này, mở trên máy không có font gốc: 0 cảnh báo thiếu font với tên trong danh sách; hộp bao mọi đối tượng chữ lệch ≤ 1% so với hộp bao đo bằng công cụ metric VN-113 trên ứng dụng chủ có font gốc **[đề xuất]**.
  3. Mọi `.shx` trong gói sinh từ `vn/fonts/src/` (CI kiểm hash nguồn).
  4. Cả bộ ≤ 5 MB; nạp một font ≤ 5 ms qua RGN-004 **[đề xuất]**.
  5. Đạt DoD-C.

## 5. Chế độ hiển thị (V)

### VN-115 · Quét bản vẽ song song `vn::scanEncodings`
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-002, DOC-009, DOC-012, DOC-017, NT-005, NT-013
- **Đầu vào:** 05 §13; 04 §7.1 (text, mtext, attribute, dim, bảng, block lồng); 07.3 UI-J02 khu vực 1 và 3; BENCH-STD 100 MB.
- **Đầu ra:** `vn/fontdoctor/scan.{h,cpp}`: `scanEncodings(snapshot, options, cancel)` hai lượt (nhận diện song song, rồi xét lại đoạn ngắn theo `FontPrior`) → bảng mã và độ tin cậy theo entity và theo đoạn, thống kê theo kiểu chữ và font, danh sách font thiếu; quét lại gia tăng theo sự kiện DOC-007; benchmark.
- **Tiêu chí nghiệm thu:**
  1. Duyệt đủ TEXT, MTEXT, ATTRIB, ATTDEF, ghi đè text của dim, MLEADER, ô TABLE, giá trị cache của FIELD, định nghĩa block lồng (mỗi định nghĩa một lần); xref chỉ thống kê riêng.
  2. BENCH-STD 100 MB đã nạp: ≤ 1 s máy tầm trung, ≤ 3 s máy thấp **[đề xuất]**; hiệu suất song song ≥ 60% đến 8 luồng.
  3. Chạy nền qua NT-013, hủy ≤ 50 ms, không chặn luồng UI.
  4. Sửa 1 đối tượng: kết quả cập nhật ≤ 1 ms, chỉ quét phần đổi **[đề xuất]**.
  5. Kết quả giống từng bit ở 1 và 32 luồng.
  6. Đạt DoD-C, DoD-H.

### VN-116 · Bảng thay thế font: phát hiện font thiếu, gợi ý, lưu hồ sơ, nhập/xuất
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-103, VN-115, RGN-006, NT-010, NT-011
- **Màn hình:** UI-J02
- **Đầu vào:** 07.3 UI-J02 khu vực 2 (cột, Phiên này / Mọi bản vẽ, nhập/xuất bảng), UI-J04 (thư mục font tin cậy); 04 §7.1 (báo cáo font thiếu, font đã thay); API FontResolver RGN-006.
- **Đầu ra:** `vn/fontdoctor/substitution.{h,cpp}`: dòng bảng (font trong bản vẽ, loại SHX/TTF/Big Font, có/thiếu, bảng mã, font thay thế, phạm vi); gợi ý tự động theo bảng mã và họ font; lưu ở hồ sơ người dùng (NT-010); nhập/xuất JSON, CSV; đẩy bảng vào RGN-006.
- **Tiêu chí nghiệm thu:**
  1. Phát hiện font thiếu đúng 100% trên 50 bản vẽ có font thiếu biết trước **[đề xuất]**, theo đường dẫn tìm kiếm NT-011 và thư mục font tin cậy.
  2. Gợi ý khớp 100% bảng vàng: `.Vn*` → TrionVN-Times hoặc TrionVN-Arial theo họ; `VNI-*` tương tự; SHX tiếng Việt → font cùng tên của VN-007.
  3. "Phiên này" chỉ đổi phiên; "Mọi bản vẽ" lưu hồ sơ; đổi một dòng chỉ regen đối tượng dùng font đó.
  4. Tệp nhập hỏng hay độc hại không làm hỏng bảng đang dùng, báo dòng lỗi; xuất rồi nhập lại giống hệt.
  5. Đạt DoD-C.

### VN-117 · Tóm tắt Bác sĩ Font, quyết định banner và chỉ báo, thiết lập
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** VN-115, NT-010, DOC-010
- **Màn hình:** UI-J01, UI-J04
- **Đầu vào:** 07.3 UI-J01 (nội dung, nút, "Không nhắc lại với tệp này", chỉ báo "Font: 3 thay thế"), UI-J04 (5 thiết lập); prototype `js/palettes.js` `FD.summary`, `FD.onOpen`, `FD.settings`.
- **Đầu ra:**
  - `vn/fontdoctor/summary.{h,cpp}`: số font thiếu, số kiểu chữ, số đối tượng theo Unicode/TCVN3/VNI/Không rõ, mức tin cậy, cờ có vấn đề; quyết định hiện banner, chỉ báo hay không; sự kiện `summaryChanged` theo lô.
  - `vn/fontdoctor/settings.{h,cpp}` trên NT-010: tự quét khi mở, ngưỡng, bảng thay thế mặc định, thư mục font tin cậy, banner hay chỉ báo; danh sách "không nhắc lại" theo `FINGERPRINTGUID` của bản vẽ.
- **Tiêu chí nghiệm thu:**
  1. Số liệu khớp 100% với đếm độc lập trên 20 bản vẽ mẫu.
  2. Quyết định hiển thị đúng bảng chân trị (font thiếu, chữ mã cũ, đã tắt nhắc, thiết lập) trong kiểm thử.
  3. "Không nhắc lại" vẫn đúng khi tệp đổi tên hoặc chuyển thư mục.
  4. Thiết lập có giới hạn (ngưỡng 50–99 **[đề xuất]**), di trú phiên bản, xuất/nhập theo profile.
  5. Khi đang quét, sự kiện tới UI không quá 10 lần/giây **[đề xuất]**.
  6. Đạt DoD-C.

### VN-004 · Chế độ hiển thị: quét khi mở (`vn::scanEncodings`), thay font và giải mã khi vẽ, không đổi dữ liệu
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-007, VN-114, VN-115, VN-116, VN-117, RGN-006, RGN-003
- **Lệnh:** FIXFONT
- **Đầu vào:** 05 §13 (chế độ hiển thị); 04 §7.1 (là mặc định); 07.3 UI-J01, UI-J02 khu vực 4 ("Hiển thị gốc", "Hiển thị đúng").
- **Đầu ra:** `vn/fontdoctor/display.{h,cpp}`: chạy `scanEncodings` khi mở theo thiết lập; cài bộ giải mã theo entity và theo đoạn vào móc của RGN-006 (byte cũ → Unicode, cờ hoa `.Vn*H`, font thay thế); chế độ "Hiển thị gốc" theo bản vẽ; kiểm thử ảnh `vn/fontdoctor/tests/display/`.
- **Tiêu chí nghiệm thu:**
  1. 200 bản vẽ TCVN3/VNI có đáp án (quy trình VN-003): chuỗi Unicode đưa vào regen khớp đáp án ở ≥ 98% đối tượng; đối tượng còn lại hiện dạng gốc, không hiện chữ sai bảng mã.
  2. Mở, quét, lưu mà không chạy lệnh chuyển: mọi chuỗi giữ nguyên từng byte (so `dwgdump`).
  3. Bản vẽ hiện ngay ở dạng gốc, đổi sang dạng đúng khi quét xong; khung pan/zoom vẫn ≤ 16,6 ms trong lúc quét.
  4. Giải mã khi vẽ làm regen cảnh "text dày" BENCH-STD chậm thêm ≤ 5% **[đề xuất]**; không cấp phát thêm mỗi khung.
  5. Đổi "Hiển thị gốc"/"Hiển thị đúng" chỉ regen đối tượng chữ mã cũ: ≤ 100 ms trên BENCH-STD 20 MB **[đề xuất]**.
  6. Lệnh FIXFONT mở palette Bác sĩ Font (UI-J02) ở chế độ hiển thị; ở Viewer nút chuyển đổi bị khóa, chuyển đổi do VN-005 thêm ở VP.
  7. Đạt DoD-C, DoD-H, DoD-L.

## 6. Chế độ chuyển đổi và chuyển hàng loạt (VP)

### VN-118 · Chuyển đối tượng chữ và kiểu chữ sang Unicode NFC
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-102, VN-115, DOC-004, DOC-007, NT-005
- **Đầu vào:** 05 §13 (chế độ chuyển đổi); 07.3 UI-J02 khu vực 5 (phạm vi, bảng mã nguồn, font đích theo nhóm, cập nhật kiểu chữ, chữ hoa `.VnTimeH`, giữ định dạng MTEXT, đối tượng tin cậy thấp).
- **Đầu ra:** `vn/fontdoctor/convert.{h,cpp}`: `plan(scan, options)` chạy nền trên snapshot; `apply(plan, Transaction&)` cho TEXT, ATTRIB, ATTDEF (gồm prompt và giá trị mặc định), MTEXT theo đoạn; đổi font của STYLE sang font đích, tách kiểu mới `<tên>_UNI` khi kiểu dùng chung với chữ Unicode **[đề xuất]**; phạm vi cả bản vẽ / vùng chọn / layout hiện hành; tin cậy thấp: bỏ qua / hỏi từng cái (callback) / vẫn chuyển; ép bảng mã nguồn.
- **Tiêu chí nghiệm thu:**
  1. 200 bản vẽ có đáp án: chuỗi sau chuyển khớp đáp án ở ≥ 98% đối tượng; 50 bản vẽ thuần Unicode: 0 chuỗi bị đổi.
  2. Chuyển lần hai không đổi gì (idempotent).
  3. Kiểu dùng chung: chữ Unicode sẵn có giữ nguyên chuỗi, kiểu và font hiệu lực (so primitive regen trước và sau).
  4. 100.000 đối tượng chữ: lập kế hoạch ≤ 1 s ở nền, áp trong một transaction ≤ 0,5 s, máy tầm trung **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

### VN-119 · Chuyển chữ trong dim, mleader, bảng, field, block lồng
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-118, DOC-012, DOC-017, DWG-014, QA-004
- **Đầu vào:** 05 §13 (attribute, ghi đè text của dim, block lồng, bảng, field); 04 §7.1.
- **Đầu ra:** `vn/fontdoctor/convert_composite.cpp`: ghi đè text của DIMENSION (giữ `<>`, `[]`, `\X`); nội dung MTEXT của MLEADER; chuỗi ô TABLE; giá trị cache của FIELD (không đụng mã `%<\AcVar…>%`); định nghĩa block lồng và block `*U` của dynamic block (mỗi định nghĩa một lần, attribute theo từng insert); chuyển luôn chữ trong block đại diện `*D`, `*T` để phần mềm khác hiện đúng mà không cần sinh lại; xref chỉ báo cáo.
- **Tiêu chí nghiệm thu:**
  1. 100 bản vẽ có dim, mleader, bảng, field, block lồng 3 cấp, có đáp án: khớp 100% đối tượng có độ tin cậy ≥ ngưỡng.
  2. Bản đã chuyển in qua DWG TrueView (QA-004): dim, bảng, mleader ra đúng chữ Unicode, không cần regen.
  3. Mã `<>`, `[]`, `\X`, `%<…>%` giữ nguyên từng byte.
  4. Block dùng ở nhiều insert chỉ chuyển một lần; block của xref không đổi.
  5. Đạt DoD-C.

### VN-005 · Chế độ chuyển đổi: lệnh FIXFONT, xem trước, một mark undo, báo cáo, lưu bản sao
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-119, DOC-008, INT-003, INT-008, INT-020, DWG-021, DWG-022, QA-004
- **Lệnh:** -FIXFONT
- **Đầu vào:** 05 §13; 07.3 UI-J02 (khu vực 5; VP lưu bản sao, EA sửa trực tiếp); 07.1 §2 (cổng phiên bản), §12 (thao tác dài).
- **Đầu ra:**
  - `vn/fontdoctor/command.cpp`: FIXFONT mở palette UI-J02; chạy từ script, LISP hoặc không có UI thì như -FIXFONT. -FIXFONT: `[Quét/Chuyển/Báo cáo]` và prompt phạm vi, bảng mã nguồn, font đích, kiểu chữ, nhóm đối tượng, tin cậy thấp; từ khóa toàn cục `_`.
  - Xem trước (chạy khô: trước → sau, bảng mã, độ tin cậy); báo cáo CSV (tệp, handle, loại, layer, kiểu, bảng mã, độ tin cậy, trước, sau, trạng thái).
  - VP: áp trong transaction, ghi bản sao (DWG-021, DWG-022), rollback, bản vẽ đang mở không đổi. EA: giữ transaction, một mark undo.
  - `docs/specs/commands/FIXFONT.md`.
- **Tiêu chí nghiệm thu:**
  1. Mọi tùy chọn của 07.3 UI-J02 khu vực 5 cho cùng kết quả (hash) khi gọi từ palette, bàn phím, `.scr` và `(command "-FIXFONT" …)`; phạm vi vùng chọn dùng PICKFIRST.
  2. Một mark undo; UNDO trả bản vẽ về giống từng bit (so `dwgdump`).
  3. Xem trước khớp 100% kết quả chuyển thật; 10.000 đối tượng ra danh sách ≤ 300 ms ở nền **[đề xuất]**.
  4. Bản sao VP mở trong DWG TrueView (QA-004) không thiếu font với kiểu đã đổi; tệp gốc và bản vẽ đang mở không đổi.
  5. Viewer: tùy chọn Chuyển báo cần nâng cấp theo 07.1 §2 (INT-020); thao tác > 2 s có tiến trình và nút hủy.
  6. Đạt DoD-C, DoD-H, DoD-L.

### VN-006 · Chuyển font hàng loạt nhiều tệp (FIXFONTBATCH): chạy nền, lưu bản sao
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-005, INT-019, NT-011, NT-013, DOC-005, DWG-024
- **Lệnh:** FIXFONTBATCH
- **Màn hình:** UI-J03
- **Đầu vào:** 07.3 UI-J03 (tệp hoặc thư mục có thư mục con, lọc `*.dwg` `*.dxf`, 3 cách ghi, định dạng lưu, dọn regapps, bảng tiến trình, Tạm dừng, Hủy, báo cáo tổng); API `trion-console` (INT-019).
- **Đầu ra:** `vn/fontdoctor/batch.{h,cpp}`: hàng đợi tệp dùng lại được (API chung, CMD-264 TCBATCH chạy trên nó); mỗi tệp mở không giao diện qua INT-019 → quét → chuyển → ghi; số tệp song song theo nhân và RAM; dòng tiến trình (tệp, trạng thái, số đối tượng đã chuyển, cảnh báo); tùy chọn dọn APPID không còn xdata tham chiếu; báo cáo tổng CSV; lệnh con `trion-console fixfont`; `docs/specs/commands/FIXFONTBATCH.md` (gồm prompt dòng lệnh khi chạy từ script).
- **Tiêu chí nghiệm thu:**
  1. Ghi đè có `.bak`, thư mục khác giữ cấu trúc, hậu tố `_unicode`: tệp gốc chỉ bị thay sau khi tệp mới ghi xong (tệp tạm + đổi tên, NT-011).
  2. Định dạng lưu: giữ nguyên, 2018, 2004, DXF; phiên bản chưa có bộ ghi (2007, 2010, 2013 trước DWG-023) lưu 2018 và ghi cảnh báo ở dòng tệp.
  3. 100 tệp × 20 MB: hiệu suất song song ≥ 60% đến 8 luồng; bộ nhớ đỉnh ≤ 70% RAM khả dụng **[đề xuất]**.
  4. Tệp hỏng: ghi lỗi, chạy tiếp; Hủy dừng ≤ 50 ms, không để tệp ghi dở; Tạm dừng và Tiếp tục đúng.
  5. Kết quả từng tệp giống hệt FIXFONT trên tệp đó (hash), không phụ thuộc số luồng.
  6. Đạt DoD-C, DoD-H.

## 7. Excel: bộ ghi XLSX/CSV, dán từ Excel, xuất bảng

### VN-151 · Bộ ghi XLSX và CSV tự viết, dùng chung cho mọi lệnh xuất
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** NT-011, NT-012, NT-114
- **Đầu vào:** 07.3 UI-J12 (giữ ô gộp, định dạng số), UI-J15; 07.2 UI-I04, UI-I05; ECMA-376 Office Open XML phần 1 (SpreadsheetML); RFC 4180 (CSV); API NT-114 (ghi ZIP dạng luồng), NT-011 (ghi an toàn).
- **Đầu ra:** `vn/excel/cell_grid.h` (`CellGrid`: ô chuỗi/số/rỗng, đậm, căn lề, mã định dạng số, ô gộp, độ rộng cột, nhiều sheet); `vn/excel/xlsx_writer.{h,cpp}`, `csv_writer.{h,cpp}`; tests, `vn/bench/xlsx_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. 50 tệp mẫu (tiếng Việt, ô gộp, số, ngày, 3 sheet) đọc bằng openpyxl và LibreOffice headless trong CI: giá trị, ô gộp, độ rộng cột khớp 100%; XML hợp lệ theo lược đồ ECMA-376 (xmllint). Job đêm trên runner Windows có Excel: mở không hiện hộp sửa tệp.
  2. Số ghi dạng số; chuỗi giống số (handle `1E5`, `007`) ghi dạng chuỗi, Excel không tự đổi.
  3. CSV: UTF-8 có BOM; dấu phân cách `,` / `;` / tab và dấu thập phân theo tham số; escape đúng RFC 4180 (nháy kép, xuống dòng trong ô).
  4. Tất định: cùng `CellGrid` cho cùng byte (thời điểm trong ZIP cố định).
  5. 10.000 dòng × 10 cột ghi XLSX ≤ 300 ms, bộ nhớ đỉnh ≤ 40 MB trên máy tầm trung **[đề xuất]**.
  6. Đạt DoD-C.

### VN-152 · Đọc vùng ô Excel từ clipboard: HTML và TSV
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** VN-151, NT-012, QA-006
- **Đầu vào:** 07.3 UI-J12 (dán thành bảng giữ định dạng cơ bản, độ rộng cột, ô gộp); định dạng clipboard "HTML Format" của Windows (offset StartFragment/EndFragment), `text/html` của macOS; HTML do Excel, LibreOffice Calc, Google Sheets sinh; TSV.
- **Đầu ra:** `vn/excel/clip_html_reader.{h,cpp}` (bộ tách thẻ tự viết, chỉ hiểu `table/tr/td/col`, `colspan/rowspan`, `width`, căn lề, đậm, `mso-number-format`), `clip_tsv_reader.{h,cpp}` → `CellGrid`; fuzz target `vn/fuzz/clip_html_fuzz.cpp`; 60 mẫu clipboard thật trong `vn/tests/data/clipboard/`.
- **Tiêu chí nghiệm thu:**
  1. 60 mẫu (Excel 2016/365 Windows và macOS, LibreOffice, Google Sheets; ô gộp, ô trống, số định dạng, xuống dòng trong ô): lưới, ô gộp, chữ hiển thị khớp 100% đáp án.
  2. Độ rộng cột đổi ra mm sai ≤ 1 mm so với đáp án đo trên Excel **[đề xuất]**.
  3. Có HTML thì dùng HTML, không thì TSV; nội dung không phải bảng trả lỗi rõ, không tạo lưới rỗng.
  4. Giới hạn 16 MB và 100.000 ô **[đề xuất]**, vượt thì báo lỗi; fuzz 1 giờ không lỗi.
  5. 10.000 ô ≤ 50 ms trên máy tầm trung **[đề xuất]**.
  6. Đạt DoD-C.

### VN-013 · Lệnh TCEXCELPASTE, TCEXCELOUT: dán từ Excel thành bảng, xuất bảng ra XLSX/CSV
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** VN-151, VN-152, CMD:TABLE, CMD:PASTECLIP, DOC-007, DOC-008, INT-003, INT-007, LSP-006
- **Lệnh:** TCEXCELPASTE, TCEXCELOUT
- **Đầu vào:** 07.3 §0, UI-J12; README §4.2 (dán từ Excel thành bảng thuộc VN-013); API tạo bảng của CMD:TABLE, đăng ký định dạng clipboard của CMD:PASTECLIP.
- **Đầu ra:** `vn/excel/tcexcelpaste_cmd.{h,cpp}`, `tcexcelout_cmd.{h,cpp}`; móc vào PASTECLIP; `docs/specs/commands/TCEXCELPASTE.md`, `TCEXCELOUT.md`; tests.
- **Tiêu chí nghiệm thu:**
  1. TCEXCELPASTE hỏi `[Bảng/Text]` (mặc định Bảng), kiểu bảng, điểm chèn. Bảng giữ ô gộp, độ rộng cột, đậm, căn lề; chiều cao chữ theo kiểu bảng nhân tỉ lệ chú thích hiện hành. Text: một TEXT mỗi ô, đúng lưới.
  2. Ctrl+V khi clipboard có dữ liệu Excel chạy TCEXCELPASTE; dữ liệu khác giữ nguyên hành vi PASTECLIP.
  3. TCEXCELOUT: một hoặc nhiều bảng → XLSX (mỗi bảng một sheet) hoặc CSV (mỗi bảng một tệp); ô gộp và định dạng số giữ nguyên; 30 bảng của QA-001 đọc lại bằng openpyxl khớp 100%.
  4. Bảng 500 ô hiện trên vùng vẽ ≤ 100 ms sau khi chỉ điểm **[đề xuất]**; một mark undo mỗi lần dán.
  5. PICKFIRST với TCEXCELOUT; bàn phím, `.scr`, `(command ...)` cho cùng kết quả.
  6. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** dán thành đối tượng OLE (P2); hộp thoại (UI-J12).

## 8. Attribute với Excel, tổng chiều dài, diện tích

### VN-014 · Lệnh TCATTOUT: xuất attribute ra XLSX/CSV
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** VN-151, VN-002, DOC-009, DOC-012, INT-003, INT-008, NT-005, LSP-006
- **Lệnh:** TCATTOUT
- **Đầu vào:** 07.3 UI-J15 (Xuất); 03 §29 (xuất/nhập attribute với Excel, P1); API DOC-012 (ATTRIB, tên hiệu dụng của dynamic block), DOC-009 (snapshot).
- **Đầu ra:** `vn/attexcel/att_export.{h,cpp}` (quét song song trên snapshot), `tcattout_cmd.{h,cpp}`; `docs/specs/commands/TCATTOUT.md` (định dạng cột, dùng lại cho TCATTIN); tests, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Chọn block theo tên (wildcard) hoặc vùng chọn; phạm vi cả bản vẽ hoặc layout hiện hành; chọn tag. Cột: HANDLE (ẩn được), tên block hiệu dụng (không phải `*U…`), layout, mỗi tag một cột.
  2. Attribute nhiều dòng: `\P` thành xuống dòng trong ô; attribute hằng không xuất; attribute có field xuất chữ hiển thị.
  3. Text mã TCVN3/VNI (VN-002) xuất nguyên bản, kèm cảnh báo nêu số đối tượng và gợi ý FIXFONT.
  4. 10.000 block × 10 tag ra XLSX ≤ 500 ms trên máy tầm trung **[đề xuất]**; tệp từng byte như nhau ở mọi số luồng.
  5. Không sửa bản vẽ (chạy ở Viewer Pro); PICKFIRST; bàn phím và `(command ...)` cho cùng tệp.
  6. Đạt DoD-C, DoD-H, DoD-L.

### VN-015 · Lệnh TCLENGTH, TCAREA: engine tổng song song tất định
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** VN-151, GEO-005, GEO-006, GEO-007, GEO-021, DOC-009, INT-003, INT-008, NT-005, RGN-013, LSP-006
- **Lệnh:** TCLENGTH, TCAREA
- **Đầu vào:** 07.2 UI-I04; 03 §16, §29 (tổng chiều dài, tổng diện tích, xuất Excel); API GEO-005/006/007 (độ dài), GEO-021 (diện tích), RGN-013 (định dạng số).
- **Đầu ra:** `vn/measure/totals.{h,cpp}` (`MeasureResult`: số đối tượng, tổng, theo layer, theo loại, giá trị từng đối tượng, danh sách bỏ qua kèm lý do; dùng cho bảng UI-I04 và tô sáng); `tclength_cmd.{h,cpp}`, `tcarea_cmd.{h,cpp}`; `docs/specs/commands/TCLENGTH.md`, `TCAREA.md`; tests, `vn/bench/totals_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Chiều dài: line, polyline (LW, 2D, 3D, bulge), cung, tròn, ellipse, spline. Diện tích: đối tượng kín, hatch (trừ island). Polyline hở, polyline tự cắt, đối tượng trong block vào danh sách bỏ qua hoặc gắn cờ, không tính sai im lặng.
  2. Sai số tương đối ≤ 1e-9 với đường giải tích, ≤ 1e-6 với spline so lời giải tham chiếu **[đề xuất]**; quy đổi theo INSUNITS (mm → m, mm² → m²), số chữ số tùy chọn.
  3. Cộng theo khối cố định rồi cây cố định (Neumaier): kết quả từng bit như nhau từ 1 đến 32 luồng.
  4. 100.000 đối tượng BENCH-STD (10% spline) ≤ 50 ms p99, 1 triệu ≤ 400 ms trên máy tầm trung **[đề xuất]**; hiệu suất song song ≥ 60% đến 8 luồng.
  5. Từ khóa `Loại`, `Đơn vị`, `Xuất` (XLSX/CSV qua VN-151); không sửa bản vẽ; PICKFIRST; bàn phím và `(command ...)` cho cùng kết quả.
  6. Đạt DoD-C, DoD-H, DoD-L.
- **Ngoài phạm vi:** chèn kết quả thành text hoặc bảng (UI-I04, mốc EA, phía UI).

### VN-153 · Lệnh TCATTIN và bộ đọc XLSX/CSV
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** VN-014, NT-015, NT-012, DOC-007, DOC-008, INT-007, QA-006
- **Lệnh:** TCATTIN
- **Đầu vào:** 07.3 UI-J15 (Nhập: xem trước thay đổi, một bước hoàn tác); `docs/specs/commands/TCATTOUT.md`; ECMA-376 (SpreadsheetML); RFC 4180.
- **Đầu ra:** `vn/excel/xlsx_reader.{h,cpp}` (sheet, sharedStrings, inlineStr, số), `csv_reader.{h,cpp}` (UTF-8 có/không BOM, UTF-16 LE, cp1258 qua NT-012, tự nhận dấu phân cách); `vn/attexcel/att_import.{h,cpp}` (khớp theo handle, bản khác biệt cho xem trước), `tcattin_cmd.{h,cpp}`; `docs/specs/commands/TCATTIN.md`; fuzz target cho hai bộ đọc.
- **Tiêu chí nghiệm thu:**
  1. Xuất bằng TCATTOUT, sửa bằng openpyxl và LibreOffice, nhập lại trên 30 bản vẽ QA-001: mọi giá trị đổi đúng; ô không đổi không tạo thay đổi.
  2. Xem trước liệt kê: cũ → mới, dòng không tìm thấy đối tượng, tag không tồn tại, tên block khác, đối tượng trên layer khóa hoặc trong xref (bỏ qua).
  3. CSV do Excel tiếng Việt lưu (cp1258, `;`) và "Unicode text" (UTF-16, tab) đọc đúng 100% chữ có dấu.
  4. Cả lần nhập một mark undo; 10.000 dòng áp dụng ≤ 1 s **[đề xuất]**.
  5. Giới hạn kích thước giải nén và số ô **[đề xuất]**; fuzz mỗi bộ đọc 1 giờ không lỗi.
  6. Đạt DoD-C, DoD-L.

### VN-154 · Lệnh TCROOMAREA: bảng diện tích phòng
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** VN-015, VN-151, VN-156, GEO-014, CMD:TABLE, DOC-008, INT-007
- **Lệnh:** TCROOMAREA
- **Đầu vào:** 07.2 UI-I05; 03 §16, §29; API GEO-014 (vùng kín theo điểm chỉ, island), VN-015, kiểu bảng "Diện tích phòng" của VN-156; prototype `js/screens12.js` (`IMPL.TCROOMAREA`).
- **Đầu ra:** `vn/measure/room_area.{h,cpp}`, `tcroomarea_cmd.{h,cpp}`; `docs/specs/commands/TCROOMAREA.md`; tests.
- **Tiêu chí nghiệm thu:**
  1. Lấy phòng: chỉ điểm bên trong (trừ island như HATCH) hoặc chọn polyline kín / hatch. Tên phòng: chữ nằm trong vùng (nhiều chữ thì lấy chữ cao nhất), attribute theo tag, hoặc nhập tay.
  2. Bảng STT, Tên phòng, Diện tích (m²), Chu vi (m), Ghi chú, dòng tổng; sắp theo cột; làm tròn 2 chữ số; tổng bằng tổng các giá trị đã làm tròn **[đề xuất]**.
  3. Bảng chèn theo kiểu TCVN và tệp XLSX/CSV cùng số liệu; 20 mặt bằng mẫu khớp đáp án tính tay.
  4. Chỉ điểm một phòng ≤ 100 ms trên mặt bằng 50.000 đối tượng **[đề xuất]**; cả lệnh một mark undo.
  5. Bàn phím, `.scr`, `(command ...)` cho cùng kết quả.
  6. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** tự cập nhật khi đường biên đổi qua field (P2).

## 9. Nội dung TCVN

### VN-155 · Kho nguồn nội dung TCVN, bộ layer theo bộ môn, CTB
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLT-002, DOC-004, NT-011, INT-019
- **Đầu vào:** 04 §7.2 (TCVN 8-20:2002, 5570:2012, 5672:2012: dãy bề rộng nét 0,13…2,0 mm, đậm:mảnh ≥ 2:1); 05 §15 (không dùng tệp của Autodesk); 07.3 UI-J13 (bộ layer KT, KC, ĐN, CTN, M&E…); API PLT-002 (ghi CTB).
- **Đầu ra:** `content/tcvn/src/` (nguồn JSON UTF-8, lược đồ ở `content/tcvn/schema/`, mỗi mục có trường nguồn: số hiệu TCVN, tác giả, ngày); CLI `vn/content/tcvn_build` chạy trên `trion-console` (`validate`, `build`); bộ layer Chung, KT, KC, ĐN, CTN, ĐHKK, GT, CK **[đề xuất]** (dạng mà CMD:TCLAYERSET đọc); `TCVN.ctb` (đen trắng), `TCVN-mau.ctb`; tests.
- **Tiêu chí nghiệm thu:**
  1. `tcvn_build validate` chạy trong CI: đúng lược đồ, tên layer ASCII không trùng (không phân biệt hoa thường) **[đề xuất]**, mọi mục có nguồn.
  2. Mọi lineweight thuộc dãy TCVN; mỗi bộ có cặp đậm:mảnh ≥ 2:1; bề rộng bút trong CTB theo màu bằng lineweight của layer cùng màu ở mọi bộ (kiểm tự động).
  3. Quét chặn: tệp chứa chuỗi bản quyền Autodesk hoặc tiêu đề của `acad.pat`/`acad.lin` làm CI đỏ.
  4. CTB đọc lại bằng PLT-002 trùng 100% nguồn; dựng hai lần cho cùng byte.
  5. Đạt DoD-C.

### VN-156 · Kiểu chữ, kiểu kích thước, kiểu bảng, mleader, MLINE TCVN
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-155, VN-007, DOC-007, RGN-014, RGN-016, RND-021
- **Đầu vào:** 04 §7.2 (TCVN 7284-0:2003 / ISO 3098, TCVN 4608:2012 cho chữ; ISO 129 và TCVN tương ứng cho kích thước **[xác minh số hiệu]**); 07.2 UI-D07 (tường 110, 220); font SHX tiếng Việt của VN-007.
- **Đầu ra:** `content/tcvn/src/styles/*.json`; `vn/content/apply_styles.{h,cpp}` (`applyStyles(Transaction&, const StyleSet&, ConflictPolicy)` giữ / ghi đè; dùng khi dựng mẫu và khi nạp từ TCLIB); `content/tcvn/TCVN.mln`; ảnh vàng; tests.
- **Tiêu chí nghiệm thu:**
  1. Kiểu chữ theo dãy chiều cao 2,5 / 3,5 / 5 / 7 / 10 / 14 / 20 mm, bản thường và annotative, font của VN-007.
  2. Kiểu kích thước `TCVN-1-1`, `TCVN-1-5`, `TCVN-1-10`, `TCVN-1-20`, `TCVN-1-25`, `TCVN-1-50`, `TCVN-1-100`, `TCVN-1-200`, `TCVN-1-500`, `TCVN-1-1000`, `TCVN-ANNO` **[đề xuất]**: chữ in ra cao 2,5 mm ở mọi tỉ lệ; ảnh vàng qua RGN-014 khớp.
  3. Kiểu bảng "Thống kê thép", "Diện tích phòng", "Danh mục bản vẽ"; kiểu mleader ghi chú và số hiệu chi tiết; kiểu MLINE tường 110, 220.
  4. `applyStyles` trong một transaction, trùng tên theo chính sách; áp lần hai không đổi gì.
  5. Đạt DoD-C.

### VN-157 · Tệp `.pat`, `.lin` tự viết giữ tên chuẩn, mẫu TCVN, `ltypeshp.shx`
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-155, VN-007, RGN-011, RGN-012, RND-021, QA-005
- **Đầu vào:** 05 §15 (tự viết tệp mẫu, giữ tên chuẩn, định nghĩa riêng); 04 §7.2 (TCVN 8-50:2005 ký hiệu vật liệu, TCVN 8-20:2002 loại nét); 07.2 UI-C07 (`trion.lin`, `trionISO.lin`, `TCVN.lin`), UI-E01 (nhóm TCVN: bê tông, BTCT, gạch, đất, cát, gỗ, kim loại, cách nhiệt); công cụ biên dịch SHX của VN-007.
- **Đầu ra:** `content/tcvn/pat/trion.pat`, `trionISO.pat`, `TCVN.pat`; `content/tcvn/lin/trion.lin`, `trionISO.lin`, `TCVN.lin`; `content/tcvn/shx/ltypeshp.shp` biên dịch ra `ltypeshp.shx` (tên shape giữ chuẩn, hình tự vẽ); danh sách tên chuẩn `docs/specs/content/pat-lin-names.md`; ảnh vàng từng mẫu; tests.
- **Tiêu chí nghiệm thu:**
  1. Đủ 100% tên trong danh sách (hatch ANSI, AR-, ISO và mẫu thường; linetype đơn giản, ACAD_ISO, phức có shape và text); mỗi định nghĩa ghi nguồn "tự viết".
  2. RGN-012 và RGN-011 đọc 100% tệp không cảnh báo; mọi shape mà linetype phức tham chiếu có trong `ltypeshp.shx`.
  3. Ảnh vàng từng mẫu qua RND-021 khớp (QA-005, SSIM ≥ 0,99).
  4. `TCVN.pat` có ≥ 20 vật liệu theo TCVN 8-50 **[đề xuất]**; `TCVN.lin` đủ loại nét TCVN 8-20 mà bộ layer VN-155 dùng.
  5. Đạt DoD-C.

### VN-158 · Khung bản vẽ A0–A4 và khung tên TCVN 5571
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** VN-155, VN-156, DOC-012, DWG-024, PLT-009
- **Đầu vào:** 04 §7.2 (TCVN 7285:2003 khổ giấy và khung; TCVN 5571:2012 khung tên); 07.3 UI-J05 (các ô); 07.2 UI-H07 (ký hiệu "KT 1/18"); API PLT-009 (nhận diện khung tên).
- **Đầu ra:** `content/tcvn/src/frames/` (nguồn DXF ASCII + manifest JSON các ô: tag, nhãn, nguồn giá trị dự án / tờ / bản vẽ, bắt buộc, độ rộng tối đa); khung A0–A4 ngang/dọc; khung tên ngang và dọc; tag cố định `TC_DONVI`, `TC_CONGTRINH`, `TC_HANGMUC`, `TC_TENBV`, `TC_KYHIEU`, `TC_GIAIDOAN`, `TC_TILE`, `TC_NGUOIVE`, `TC_KIEM`, `TC_CHUTRI`, `TC_CHUNHIEM`, `TC_DUYET`, `TC_NGAY`, `TC_SOTO`, `TC_TONGSOTO` **[đề xuất]**; tests.
- **Tiêu chí nghiệm thu:**
  1. Khổ và lề khớp bảng số của TCVN 7285:2003 (sai số 0); khung tên ở góc phải dưới, đủ ô của TCVN 5571:2012.
  2. Mọi ATTDEF có prompt tiếng Việt, kiểu chữ của VN-156; chuỗi dài +40% co theo ô, không tràn.
  3. PLT-009 nhận 100% khung chèn từ các block này ở Model (tỉ lệ 1:50…1:500) và layout, trên 20 bản vẽ thử.
  4. Đọc qua DWG-024 không cảnh báo; manifest và ATTDEF lệch nhau (tag thừa hoặc thiếu) làm CI đỏ.
  5. Đạt DoD-C.

### VN-159 · Thư viện block TCVN: Chung và Kiến trúc
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** VN-155, VN-156, DOC-012, DOC-021, DWG-024, RND-021
- **Đầu vào:** 07.3 UI-J13 (Chung: cao độ, trục, hướng Bắc, mặt cắt, chi tiết, mũi tên; Kiến trúc: cửa đi, cửa sổ, thiết bị vệ sinh, bếp, nội thất, cầu thang, cây xanh, người, xe); 04 §7.2 (TCVN 4614:2012, 5686:2012, 6003-1/-2:2012, 6077:2012, 4607:2012).
- **Đầu ra:** `content/tcvn/src/blocks/chung/`, `kien-truc/` (DXF ASCII + metadata JSON: tên Việt/Anh, danh mục, từ khóa, tiêu chuẩn tham chiếu, kích thước, điểm gốc, annotative hay kích thước thật, layer gợi ý); ảnh xem trước sinh bằng RND-021; `docs/specs/content/symbol-tags.md` (tag của ký hiệu, TCSYMBOL dùng); tests.
- **Tiêu chí nghiệm thu:**
  1. ≥ 60 ký hiệu Chung (annotative) và ≥ 150 block Kiến trúc vẽ kích thước thật theo mm **[đề xuất]**.
  2. Kiểm tự động: hình trên layer 0, màu/nét/độ dày ByBlock; INSUNITS = mm; không xref, proxy, ảnh; không kiểu thừa; ≤ 200 KB mỗi block **[đề xuất]**.
  3. Ký hiệu cao độ, trục, mặt cắt, chi tiết có attribute đúng tag trong `symbol-tags.md`.
  4. Cửa đi, cửa sổ có đủ dãy kích thước phổ biến dạng block tĩnh **[đề xuất]**.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** dynamic block (sau DOC-023); Kết cấu, Điện, Cấp thoát nước (VN-162).

### VN-008 · Mẫu `.dwt` TCVN và nghiệm thu gói nội dung
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** VN-155, VN-156, VN-157, VN-158, VN-159, DWG-022, DWG-026, PLT-001, PLT-004, DOC-011, DOC-015, QA-003, QA-004, QA-005, VN-110
- **Đầu vào:** 04 §7.2; 07.3 UI-J11 (lọc theo khổ, hướng, bộ môn, tỉ lệ chính, kiểu khung tên); API DWG-022 (ghi AC1032), PLT-001 (page setup), DOC-011 (layout).
- **Đầu ra:** `tcvn_build build` sinh `content/tcvn/templates/*.dwt` cho A0–A4 × ngang/dọc × KT, KC, M&E, GT, CK, mỗi mẫu có bộ layer, kiểu, CTB, page setup, khung tên trong layout, viewport theo tỉ lệ chính, INSUNITS = mm, PSLTSCALE/MSLTSCALE, danh sách tỉ lệ chú thích; `templates/index.json` cho UI-J11; `content/tcvn/REVIEW.md` (biên bản thẩm định, 04 §7.2).
- **Tiêu chí nghiệm thu:**
  1. Mọi `.dwt` qua AUDIT (DOC-015) 0 lỗi, round-trip QA-003 khớp từng trường, DWG TrueView mở không hỏi recover (QA-004).
  2. Kiểm chéo: mọi linetype, kiểu, CTB được tham chiếu có trong gói; không mục thừa (PURGE không xóa được gì).
  3. Dựng hai lần cho cùng byte; cả gói nội dung ≤ 40 MB **[đề xuất]**.
  4. Bản vẽ mới từ mẫu A3 KT 1:100 in qua PLT-004 đúng khổ, đúng bề rộng nét theo CTB (QA-005 so ảnh vàng).
  5. Mọi mục có trạng thái thẩm định trong `REVIEW.md`; thiếu thì CI đỏ.
  6. Đạt DoD-C.

## 10. Thư viện TCVN

### VN-160 · Gói thư viện `.tcpkg` và chỉ mục tìm không dấu
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** NT-015, NT-012, NT-011, NT-005, QA-006
- **Đầu vào:** 07.3 UI-J13 (tìm không phân biệt dấu, xem trước, thông tin tiêu chuẩn, thư mục máy hoặc thư mục mạng dùng chung); API NT-015 (ZIP), NT-012 (so sánh không phân biệt hoa thường có dấu), NT-011 (theo dõi thư mục).
- **Đầu ra:** `vn/library/package.{h,cpp}`: `.tcpkg` **[đề xuất]** là ZIP gồm `manifest.json` (phiên bản định dạng, mục, danh mục, từ khóa), `items/<id>.dwg`, `thumbs/<id>.png`, `files/` (`.pat`, `.lin`, `.ctb`, `.dwt`, bộ layer, bộ kiểu); thư mục cùng cấu trúc cũng là gói hợp lệ. `vn/library/index.{h,cpp}` (gộp nhiều nguồn, tìm, cây danh mục); `docs/specs/formats/tcpkg.md`; fuzz target.
- **Tiêu chí nghiệm thu:**
  1. Ghi rồi đọc lại gói trùng 100%. Gói hỏng (đường dẫn `../`, bom nén, JSON sai, mục thiếu tệp) bị từ chối theo từng mục, các mục khác vẫn dùng được; fuzz 1 giờ không lỗi.
  2. "cua di", "CUA DI", "cửa đi" cho cùng kết quả; đ ↔ d; xếp hạng: trùng tên > tiền tố > chứa; yêu thích lên trước.
  3. 20.000 mục: dựng chỉ mục ≤ 200 ms, cập nhật kết quả mỗi phím ≤ 16 ms p99 trên máy tầm trung **[đề xuất]**.
  4. Thư mục mạng thêm/xóa gói: chỉ mục cập nhật ≤ 2 s qua theo dõi tệp NT-011.
  5. Mở gói chỉ đọc `manifest.json`; DWG và ảnh nạp lười, cache LRU.
  6. Đạt DoD-C, DoD-H, DoD-L.

### VN-009 · Lệnh TCLIB: chèn từ Thư viện TCVN, nạp bộ layer, kiểu, mẫu
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** VN-160, VN-008, VN-156, CMD:INSERT, CMD:TCLAYERSET, CMD:LINETYPE, DOC-014, DOC-021, INT-003, INT-007, INT-012, LSP-006
- **Lệnh:** TCLIB
- **Đầu vào:** 07.3 §0, UI-J13 (chèn bằng chỉ điểm hoặc kéo-thả, tự áp tỉ lệ chú thích; Nạp vào bản vẽ: bộ layer, kiểu, hatch, linetype, CTB, mẫu); API VN-160, DOC-014 (sao chép sâu giữa hai bản vẽ), API chèn của CMD:INSERT, CMD:TCLAYERSET, `applyStyles` của VN-156.
- **Đầu ra:** `vn/library/tclib_cmd.{h,cpp}`; `vn/library/library_service.{h,cpp}` (API cho palette UI-J13 và BLOCKSPALETTE: duyệt, tìm, chèn, nạp); bước `tcvn_build pack` đóng nội dung VN-008 thành `tcvn.tcpkg`; `docs/specs/commands/TCLIB.md`; tests.
- **Tiêu chí nghiệm thu:**
  1. Có giao diện: TCLIB mở palette. Từ `.scr`/LISP: dạng dòng lệnh `[Chèn/Nạp/Tìm]`, chèn theo mã mục, điểm, góc; kết quả trùng với chèn từ palette.
  2. Ký hiệu annotative chèn theo tỉ lệ chú thích hiện hành; đồ vật kích thước thật đổi đơn vị theo INSUNITS của bản vẽ đích; ATTREQ, ATTDIA được tôn trọng.
  3. Block trùng tên khác định nghĩa: hỏi Dùng có sẵn / Định nghĩa lại / Đổi tên; không ghi đè im lặng.
  4. Chèn một mục ≤ 50 ms p99 từ lúc chỉ điểm (gói trên ổ cục bộ) **[đề xuất]**; jig ≤ 8 ms/khung.
  5. Nạp bộ layer, kiểu, linetype, hatch: một mark undo, báo số mục thêm / ghi đè / giữ.
  6. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** bản LISP chạy trên CAD khác (VN-017); Thư viện của tôi (VN-161).

### VN-161 · Thư viện của tôi, yêu thích, thư mục thư viện dùng chung
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** VN-009, CMD:WBLOCK, NT-010, NT-011, RND-021
- **Đầu vào:** 07.3 UI-J13 (Khác: yêu thích; Thư viện của tôi với tên, danh mục, điểm gốc, mô tả; thư mục mạng dùng chung cho cả công ty).
- **Đầu ra:** `vn/library/user_library.{h,cpp}` (thêm, sửa, xóa mục; ghi gói dạng thư mục nguyên tử); vị trí thư viện và yêu thích trong kho cài đặt NT-010; tests.
- **Tiêu chí nghiệm thu:**
  1. Đưa đối tượng đang chọn vào thư viện: DWG của mục (qua API của CMD:WBLOCK), ảnh xem trước, metadata; tìm thấy ngay trong chỉ mục.
  2. Hai tiến trình cùng ghi vào một thư mục mạng, 1.000 lần ghi xen kẽ: không mất mục, `manifest.json` không hỏng (tệp tạm + đổi tên, kiểm phiên bản manifest trước khi ghi).
  3. Thư mục chỉ đọc: thao tác ghi bị chặn kèm thông báo rõ, đọc vẫn bình thường.
  4. Yêu thích và vị trí thư viện còn sau khi khởi động lại, theo profile.
  5. Đạt DoD-C.

### VN-162 · Thư viện block TCVN: Kết cấu, Điện, Cấp thoát nước
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** VN-159, VN-009
- **Đầu vào:** 07.3 UI-J13 (Kết cấu: ký hiệu thép, cọc, cột, dầm, móng, mặt cắt thép hình; Điện: thiết bị, chiếu sáng, ổ cắm, công tắc; Cấp thoát nước: thiết bị, van); 04 §7.2 (TCVN 5572:2012, 6084:2012, 4613:2012, 4610:2012; 5422:2012, 5673:2012, 3989:2012, 4318:2012, 5681:2012).
- **Đầu ra:** `content/tcvn/src/blocks/ket-cau/`, `dien/`, `cap-thoat-nuoc/`; mặt cắt thép hình sinh từ bảng quy cách bằng script trong `tcvn_build`; block thanh thép có attribute (số hiệu, đường kính, chiều dài, số lượng, hình dạng) cho TCREBAR; tests.
- **Tiêu chí nghiệm thu:**
  1. ≥ 200 block cho ba bộ môn **[đề xuất]**, qua cùng bộ kiểm tự động của VN-159.
  2. Mặt cắt thép hình khớp 100% bảng quy cách nguồn (so tự động).
  3. Mọi block chèn được qua TCLIB và tìm được bằng tên không dấu.
  4. Đạt DoD-C.

## 11. Khung tên và khung in

### VN-163 · Mô hình khung tên: nhận diện, đọc/ghi ô, áp cho mọi khung cùng loại, mẫu công ty
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** VN-158, VN-160, DOC-005, DOC-007, DOC-012, DWG-027
- **Đầu vào:** 07.3 UI-J05; manifest ô của VN-158; API DOC-005 (xdata), DOC-012, DWG-027 (DWGPROPS).
- **Đầu ra:** `vn/titleblock/template.{h,cpp}` (mẫu = block + manifest); `detect.{h,cpp}` (xdata `TRION_TB` ghi mã mẫu và phiên bản **[đề xuất]**, dự phòng theo tên block); `cells.{h,cpp}` (đọc/ghi ô, áp cho mọi khung cùng mẫu, điền từ DWGPROPS và giá trị dùng gần nhất); lưu mẫu công ty vào gói VN-160; tests.
- **Tiêu chí nghiệm thu:**
  1. Nhận 100% khung tên trên 30 bản vẽ thử, kể cả block bị đổi tên, khung trong layout và trong Model ở mọi tỉ lệ.
  2. Giá trị ô ghi dạng chữ thường, không field, để AutoCAD mở thấy đúng **[đề xuất]**; mỗi thao tác một transaction.
  3. "Áp dụng cho tất cả cùng loại" trên 500 khung ≤ 200 ms trên máy tầm trung **[đề xuất]**, chỉ đổi các ô được chọn.
  4. Mẫu công ty lưu từ block có attribute chèn lại được ở bản vẽ khác, đủ ô.
  5. Đạt DoD-C.

### VN-010 · Lệnh TCTITLE và TCFRAME
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** VN-163, VN-008, PLT-009, DOC-008, DOC-021, INT-003, INT-007, INT-012, LSP-006
- **Lệnh:** TCTITLE, TCFRAME
- **Đầu vào:** 07.3 §0, UI-J05, UI-J06; prototype `js/screens11.js` (`IMPL.TCTITLE`, `IMPL.TCFRAME`, `ensureFrame`); API VN-163, PLT-009.
- **Đầu ra:** `vn/titleblock/tctitle_cmd.{h,cpp}`, `tcframe_cmd.{h,cpp}`; `docs/specs/commands/TCTITLE.md`, `TCFRAME.md`; tests.
- **Tiêu chí nghiệm thu:**
  1. TCTITLE `[Chèn/Sửa/Áp dụng/Lưu mẫu]`: Chèn hỏi mẫu (TCVN 5571 ngang/dọc, mẫu công ty), khổ, tỉ lệ (Model) hoặc 1:1 (layout), điểm chèn có jig; ô tự điền theo VN-163. Sửa nhận khung bằng một lần chọn.
  2. TCFRAME: khổ, hướng, tỉ lệ, mẫu; đặt từng khung hoặc mảng n cột × m hàng có khoảng cách; số tờ tự đánh theo hàng rồi cột.
  3. Khung do TCFRAME tạo được PLT-009 nhận 100% và xếp đúng thứ tự số tờ.
  4. Mảng 10 × 10 khung ≤ 100 ms **[đề xuất]**; jig ≤ 8 ms/khung; mỗi lệnh một mark undo.
  5. Bàn phím, `.scr`, `(command ...)` cho cùng kết quả.
  6. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** logo và ánh xạ field (VN-169); hộp thoại (UI-J05, UI-J06).

## 12. Đánh số, công cụ text, ký hiệu kỹ thuật

### VN-164 · Engine số trong chuỗi và lệnh TCINCCOPY
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** RGN-008, DOC-007, DOC-008, INT-003, INT-007, INT-012, LSP-006
- **Lệnh:** TCINCCOPY
- **Đầu vào:** 07.3 UI-J08 (`C1` → `C2`, `1.05` → `1.10`, `A` → `B`; phần số cuối / đầu / thứ n), UI-J09 (đổi số: cộng, nhân); prototype `js/screens11.js` (`IMPL.TCINCCOPY`: Bước · B, Phần số · P với Cuối · L, Đầu · F, Thứ 2, Thứ 3, Hoàn tác · U).
- **Đầu ra:** `vn/text/number_runs.{h,cpp}` (tách phần số và chữ; tăng theo bước, cộng, nhân; giữ số 0 đầu, số chữ số thập phân, dấu `+`/`±`, dấu thập phân `.` hoặc `,`; dãy chữ A…Z → AA), `vn/text/tcinccopy_cmd.{h,cpp}`; `docs/specs/commands/TCINCCOPY.md`; tests.
- **Tiêu chí nghiệm thu:**
  1. ≥ 300 ca vàng **[đề xuất]** (`09` → `10`, `001` → `002`, `1.05` + 0,05 → `1.10`, `Z` → `AA`, `+1.200` + 0,45 → `+1.650`, bước âm); tăng n rồi giảm n trả về chuỗi gốc trên 1 triệu ca ngẫu nhiên.
  2. Mã định dạng MTEXT giữ nguyên, chỉ đổi đoạn chữ (so cây RGN-008).
  3. TCINCCOPY: nguồn là TEXT, MTEXT hoặc attribute (sao chép cả block); jig hiện giá trị kế tiếp ≤ 8 ms/khung; tùy chọn B, P, U như prototype; Enter kết thúc; cả lần chạy một mark undo.
  4. Bàn phím, `.scr`, `(command ...)` cho cùng kết quả.
  5. Đạt DoD-C, DoD-L.

### VN-011 · Lệnh TCNUMBER và TCOUNT
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** VN-164, INT-007, INT-008, INT-012, NT-010
- **Lệnh:** TCNUMBER, TCOUNT
- **Đầu vào:** 07.3 UI-J07; prototype `js/screens11.js` (`IMPL.TCNUMBER`: chỉ điểm từng cái với Hoàn tác · U, Xong · X; lưu mẫu đánh số); hành vi TCOUNT của Express Tools (sắp theo X, Y hoặc thứ tự chọn; số đầu và bước; Overwrite, Prefix, Suffix, Find&replace).
- **Đầu ra:** `vn/text/tcnumber_cmd.{h,cpp}`; `vn/text/tcount_cmd.{h,cpp}` (ánh xạ tùy chọn TCOUNT vào engine của TCNUMBER); preset lưu trong NT-010; `docs/specs/commands/TCNUMBER.md`, `TCOUNT.md`; tests.
- **Tiêu chí nghiệm thu:**
  1. Đích: text có sẵn, attribute theo tag, text mới tại điểm chỉ. Định dạng: tiền tố, số đầu, bước, số chữ số, hậu tố, hoặc chữ cái.
  2. Thứ tự: thứ tự chọn, X tăng, Y giảm, hàng–cột (dung sai hàng 0,5 × chiều cao chữ **[đề xuất]**); 50 ca vàng khớp.
  3. Giá trị xem trước trên overlay INT-012 trước khi áp dụng; 5.000 đối tượng cập nhật ≤ 16 ms **[đề xuất]**.
  4. TCOUNT: 20 kịch bản `.scr` viết theo cú pháp Express Tools cho cùng kết quả như tự đánh tay (ca vàng).
  5. Có sẵn preset trục, phòng, cọc, thiết bị; preset người dùng còn sau khi khởi động lại; mỗi lệnh một mark undo.
  6. Đạt DoD-C, DoD-L.

### VN-165 · Đổi hoa/thường tiếng Việt có dấu
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** NT-012, RGN-008, VN-001, VN-002
- **Đầu vào:** 07.3 UI-J09 (HOA TOÀN BỘ / thường / Hoa đầu câu / Hoa Mỗi Từ); 03 §29; bảng chữ tiếng Việt Unicode dựng sẵn và tổ hợp; API VN-001, VN-002.
- **Đầu ra:** `vn/text/vn_case.{h,cpp}` (`changeCase(text, Mode, Encoding)`; Upper, Lower, Sentence, Title, Toggle); tests.
- **Tiêu chí nghiệm thu:**
  1. Đủ 134 chữ cái có dấu (67 cặp, kể cả đ/Đ) đổi đúng ở NFC và NFD; giữ dạng chuẩn hóa của đầu vào.
  2. Mã MTEXT (`\P`, `{\f…;}`, `\S…;`), `%%c`/`%%d`/`%%p`, `\U+xxxx`, field `%<…>%` không đổi: 1 triệu chuỗi ngẫu nhiên chỉ đổi phần chữ (so cây RGN-008).
  3. Hoa đầu câu sau `.`, `!`, `?`, `\P` và đầu chuỗi; Hoa Mỗi Từ theo từng âm tiết.
  4. Text mã TCVN3/VNI: giải mã (VN-001), đổi, mã hóa lại đúng bảng mã; ký tự không có trong bảng mã (chữ hoa có dấu của TCVN3) thì báo, giữ nguyên chữ đó, gợi ý FIXFONT.
  5. Đạt DoD-C.

### VN-166 · Lệnh TCSYMBOL: cao độ, mặt cắt, chi tiết, hướng Bắc, độ dốc, vật liệu
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** VN-159, VN-009, CMD:HATCH, RGN-013, DOC-005, DOC-008, INT-003, INT-007, INT-012
- **Lệnh:** TCSYMBOL
- **Đầu vào:** 07.3 §0, UI-J10; 03 §29 (cao độ, trục, mặt cắt, vật liệu theo TCVN); prototype `js/screens11.js` (`IMPL.TCSYMBOL`); block ký hiệu và tag của VN-159 (`symbol-tags.md`).
- **Đầu ra:** `vn/symbol/tcsymbol_cmd.{h,cpp}`, `elevation.{h,cpp}`, `section_mark.{h,cpp}`; gốc ±0.000 lưu trong xrecord của bản vẽ **[đề xuất]**; `docs/specs/commands/TCSYMBOL.md`; tests.
- **Tiêu chí nghiệm thu:**
  1. Từ khóa `[Cao độ/Trục/Mặt cắt/Chi tiết/Bắc/Dốc/Vật liệu]`; nhánh Trục do VN-167 hiện thực.
  2. Cao độ: mẫu mặt bằng và mặt cắt; giá trị (Y − Y gốc) đổi ra m theo INSUNITS, định dạng `+1.200`, `-0.450`, `±0.000`, hoặc nhập tay; 40 ca vàng khớp.
  3. Mặt cắt, chi tiết: số hiệu, mũi tên hướng nhìn, tham chiếu tờ `1-1 / KT 05`; độ dốc `i = 2%` theo hướng chỉ; Vật liệu mở HATCH với mẫu TCVN đã chọn.
  4. Ký hiệu annotative theo tỉ lệ chú thích hiện hành; jig ≤ 8 ms/khung; mỗi lần đặt một mark undo.
  5. Bàn phím, `.scr`, `(command ...)` cho cùng kết quả.
  6. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** cao độ tự cập nhật khi di chuyển (P2).

### VN-167 · TCSYMBOL: lưới trục định vị và kích thước trục
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** VN-166, VN-164, VN-156, RGN-014, DOC-020
- **Đầu vào:** 07.3 UI-J10 (Trục định vị: bảng tên trục và khoảng cách, hoặc lấy từ đối tượng có sẵn; đường trục nét tâm, bóng trục, kích thước khoảng cách và kích thước tổng).
- **Đầu ra:** `vn/symbol/axis_grid.{h,cpp}` (cú pháp khoảng cách `3x6000,4500` **[đề xuất]**; tên 1, 2, 3… và A, B, C… qua engine VN-164); nhánh Trục của TCSYMBOL; tests.
- **Tiêu chí nghiệm thu:**
  1. Lưới 12 × 8 trục: đường trục trên layer trục với linetype tâm, bóng trục hai đầu, chuỗi kích thước khoảng cách và kích thước tổng theo kiểu kích thước TCVN của tỉ lệ hiện hành; ≤ 150 ms **[đề xuất]**.
  2. Lấy lưới từ line có sẵn: nhận trục song song, sắp theo vị trí, dung sai góc 0,01°.
  3. Cả lưới là một GROUP và một mark undo; giá trị kích thước bằng khoảng cách nhập (so tự động).
  4. Tùy chọn bỏ qua I, O, Z khi đặt tên trục chữ **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** sửa lưới trục sau khi tạo (P2).

### VN-012 · Lệnh TCTEXT và TCASE: hoa/thường, nối, tách, canh hàng, tiền tố, đổi số
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** VN-165, VN-164, VN-167, RGN-009, DOC-008, INT-007, INT-008, LSP-006
- **Lệnh:** TCTEXT, TCASE
- **Đầu vào:** 07.3 §0, UI-J09; prototype `js/screens11.js` (`IMPL.TCTEXT`); hành vi TCASE của Express Tools (Sentence, lower, UPPER, Title, tOGGLE); API VN-165, VN-164, RGN-009 (dàn dòng MTEXT).
- **Đầu ra:** `vn/text/tctext_cmd.{h,cpp}`, `text_ops.{h,cpp}` (nối, tách, canh hàng, chia đều, tiền tố/hậu tố, đổi số), `tcase_cmd.{h,cpp}`; `docs/specs/commands/TCTEXT.md`, `TCASE.md`; tests.
- **Tiêu chí nghiệm thu:**
  1. TCTEXT `[Hoa thường/Nối/Tách/Canh/Tiền tố/Đổi số]` áp cho TEXT, MTEXT, ATTRIB, ATTDEF, chữ MLEADER, chữ ghi đè của kích thước, ô bảng; Nối và Tách chỉ TEXT, MTEXT.
  2. Nối nhiều TEXT thành TEXT hoặc MTEXT theo thứ tự chọn / X / Y, ký tự nối tùy chọn, giữ kiểu và chiều cao của chữ đầu. Tách MTEXT thành TEXT theo dòng hiển thị (RGN-009) hoặc theo ký tự; vị trí từng dòng lệch ≤ 1e-6 đơn vị.
  3. Canh trái / phải / giữa theo một đường, chia đều khoảng cách; Đổi số cộng/nhân giữ định dạng (cao độ +0,450).
  4. TCASE nhận đủ tùy chọn kiểu Express Tools, cho cùng kết quả với TCTEXT Hoa thường.
  5. 10.000 text đổi hoa/thường ≤ 200 ms **[đề xuất]**; mỗi lệnh một mark undo; PICKFIRST; bàn phím, `.scr`, `(command ...)` như nhau.
  6. Nghiệm thu nhóm: 30 ca vàng tự soạn (bản vẽ trước/sau) cho TCTEXT và TCSYMBOL, kể cả Trục, chạy trong CI.
  7. Đạt DoD-C, DoD-L.

## 13. Quản lý bộ bản vẽ kiểu VN

### VN-168 · Mô hình bộ bản vẽ: tệp `.tcsheets`, quét tờ từ nhiều tệp, số hiệu theo bộ môn
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** VN-163, DOC-016, DOC-018, DWG-019, NT-011, NT-013, QA-006
- **Đầu vào:** 07.2 UI-H07 (thông tin dự án; tờ từ layout hoặc khung trong Model của nhiều tệp; cột số hiệu, tên, tỉ lệ, khổ, ngày, trạng thái; kéo để sắp xếp; ánh xạ khung tên); 03 §14 (P1 khác biệt VN).
- **Đầu ra:** `vn/sheets/sheet_set.{h,cpp}`; `.tcsheets` **[đề xuất]** là JSON UTF-8 có số phiên bản, đường dẫn tương đối; `vn/sheets/scan.{h,cpp}` (đọc lười, chỉ layout và khung tên); `numbering.{h,cpp}` (`KT 01` hoặc `KT 1/18` theo bộ môn); `docs/specs/formats/tcsheets.md`; fuzz target.
- **Tiêu chí nghiệm thu:**
  1. Quét 50 tệp 20 MB song song ≤ 5 s trên máy tầm trung **[đề xuất]**; chạy nền có tiến trình, hủy ≤ 50 ms.
  2. Đánh lại số hiệu sau khi kéo sắp xếp cho kết quả tất định; `KT 1/18` đếm riêng từng bộ môn.
  3. Trạng thái "sửa sau khi in" so hash nội dung tệp lúc in **[đề xuất]**, không dựa thời gian sửa.
  4. `.tcsheets` ghi an toàn, đọc lại trùng; dời cả thư mục dự án vẫn mở được; fuzz 1 giờ không lỗi.
  5. Đạt DoD-C, DoD-H.

### VN-169 · Cập nhật khung tên trên nhiều tệp, logo, field
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** VN-168, DWG-018, DWG-023, DOC-019, DOC-022, RGN-025, NT-011, NT-013, QA-003, QA-004
- **Đầu vào:** 07.2 UI-H07 ("Cập nhật tất cả khung tên"); 07.3 UI-J05 (logo, ánh xạ attribute ↔ field, áp cho cả bộ bản vẽ); API VN-163, VN-168.
- **Đầu ra:** `vn/sheets/update_titles.{h,cpp}` (mở từng tệp, ghi ô theo ánh xạ, lưu đúng phiên bản gốc); `vn/titleblock/logo.{h,cpp}` (ảnh logo theo đường dẫn tương đối); field cho giá trị nội tại của bản vẽ (ngày, tên tệp, tỉ lệ); tests.
- **Tiêu chí nghiệm thu:**
  1. Bộ 18 tờ trong 6 tệp: mọi khung tên có `KT 1/18`…`KT 18/18` và thông tin dự án đúng; QA-003 chỉ thấy các attribute dự kiến đổi, mọi dữ liệu khác giữ nguyên.
  2. DWG TrueView mở mọi tệp đã lưu không hỏi recover (QA-004).
  3. Tệp đang mở trong phiên: sửa qua document đó, một mark undo. Tệp người khác đang khóa (`.dwl`) hoặc chỉ đọc: bỏ qua và báo. Luôn có `.bak`.
  4. Chạy nền có tiến trình; hủy giữa hai tệp, không để tệp ghi dở.
  5. Đạt DoD-C, DoD-H.

### VN-018 · Lệnh TCSHEETS: quản lý bộ bản vẽ kiểu VN
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** VN-169, VN-010, VN-151, VN-156, PLT-008, PLT-009, CMD:TABLE, CMD:ETRANSMIT, INT-007, INT-019
- **Lệnh:** TCSHEETS
- **Đầu vào:** 07.2 UI-H07; 03 §14; 07.3 §0; prototype `js/screens11.js`, `js/screens17.js` (`IMPL.TCSHEETS`, `newSheet`, `moveSheet`); API VN-168, VN-169, PLT-008 (PUBLISH), PLT-009.
- **Đầu ra:** `vn/sheets/tcsheets_cmd.{h,cpp}` (mở palette; dạng dòng lệnh cho `.scr`: `[Mở/Quét/Đánh số/Cập nhật/In/Xuất/Đóng gói]`), `sheet_list_export.{h,cpp}`; `docs/specs/commands/TCSHEETS.md`; tests.
- **Tiêu chí nghiệm thu:**
  1. In cả bộ: tờ layout qua PLT-008, khung trong Model qua PLT-009; 18 tờ ra PDF đúng thứ tự số hiệu.
  2. Xuất danh mục bản vẽ thành bảng (kiểu "Danh mục bản vẽ" của VN-156) hoặc XLSX/CSV, cùng số liệu.
  3. Đóng gói gọi ETRANSMIT với mọi tệp của bộ, kèm `.tcsheets` và logo.
  4. Kịch bản đầu cuối 18 tờ bằng `.scr` trên `trion-console` cho cùng kết quả mỗi lần chạy.
  5. Đạt DoD-C.

## 14. Sau v1.0: thống kê thép, kiểm tra theo TCVN

### VN-170 · Lệnh TCREBAR: thống kê thép
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** VN-162, VN-156, VN-151, CMD:TABLE, RGN-008, GEO-006, NT-005
- **Lệnh:** TCREBAR
- **Đầu vào:** 07.3 UI-J14; 03 §29 (thống kê thép P1–P2); 04 §7.2 (TCVN 5572:2012); prototype `js/screens17.js` (`IMPL.TCREBAR`, `rbExcel`); block thanh thép của VN-162.
- **Đầu ra:** `vn/rebar/notation.{h,cpp}` (nhận dạng `2Ø16`, `Ø8a200`, `%%c16`, `ø`/`Φ`, số hiệu thanh), `schedule.{h,cpp}` (số hiệu, hình dạng, đường kính, chiều dài một thanh, số lượng, tổng chiều dài, khối lượng; tổng hợp theo đường kính), `tcrebar_cmd.{h,cpp}`; bảng trọng lượng đơn vị có ghi nguồn; `docs/specs/commands/TCREBAR.md`; tests.
- **Tiêu chí nghiệm thu:**
  1. 20 bản vẽ kết cấu có bảng thống kê tính tay: nhận dạng ≥ 98% ký hiệu **[đề xuất]**; phần còn lại vào danh sách "thiếu thông tin", không ký hiệu nào bị tính sai im lặng.
  2. Tổng khối lượng khớp đáp án, sai ≤ 0,01 kg.
  3. Bảng chèn theo mẫu TCVN và tệp XLSX cùng số liệu.
  4. 50.000 ký hiệu ≤ 1 s, song song, kết quả tất định **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

### VN-016 · Lệnh TCCHECK: kiểm tra bản vẽ theo TCVN; nghiệm thu nhóm Sau
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** VN-170, VN-155, VN-156, VN-163, VN-151, CMD:CHECKSTANDARDS, NT-005
- **Lệnh:** TCCHECK
- **Đầu vào:** 07.2 UI-I09 (bộ quy tắc TCVN kiến trúc, kết cấu, công ty; lỗi / cảnh báo / gợi ý; trích dẫn điều khoản; Sửa tự động / Bỏ qua / Ghi chú; xuất báo cáo; chung khung với STANDARDS, CHECKSTANDARDS); nội dung VN-155, VN-156; mô hình kết quả và luồng sửa của CMD:CHECKSTANDARDS.
- **Đầu ra:** `vn/check/rules.{h,cpp}` (quy tắc JSON: tên layer, chiều cao chữ theo tỉ lệ, độ dày nét theo dãy TCVN và tỉ lệ đậm:mảnh, kiểu kích thước, ô khung tên bắt buộc); `content/tcvn/rules/`; `vn/check/tccheck_cmd.{h,cpp}`; `docs/specs/commands/TCCHECK.md`; tests.
- **Tiêu chí nghiệm thu:**
  1. Mỗi quy tắc có ca dương và ca âm; 30 bản vẽ cấy lỗi: phát hiện 100%; 30 bản vẽ sạch tạo từ mẫu VN-008: 0 báo nhầm.
  2. Mỗi lỗi có mức độ và trích dẫn điều khoản lấy từ tệp quy tắc (không tự sinh); Sửa tự động đi qua lệnh, mỗi lần sửa một mark undo.
  3. Bản vẽ 1 triệu entity kiểm ≤ 2 s trên máy tầm trung **[đề xuất]**, song song, kết quả tất định.
  4. Xuất báo cáo XLSX; nạp bộ quy tắc công ty từ tệp.
  5. Nghiệm thu nhóm: TCREBAR và TCCHECK chạy trên cùng 20 bản vẽ kết cấu không lỗi.
  6. Đạt DoD-C, DoD-H.

## Tóm tắt theo mốc

| Mốc | Task | Số task |
|---|---|---|
| S4 | VN-101, VN-001, VN-102, VN-103, VN-104, VN-003, VN-002, VN-107, VN-108 | 9 |
| CC | VN-109, VN-110, VN-111, VN-017 | 4 |
| V | VN-105, VN-112, VN-113, VN-114, VN-007, VN-115, VN-116, VN-117, VN-004 | 9 |
| VP | VN-118, VN-119, VN-005, VN-006, VN-151, VN-014, VN-015 | 7 |
| EA | VN-152, VN-013, VN-155, VN-156, VN-157, VN-158, VN-159, VN-008, VN-160, VN-009, VN-163, VN-010 | 12 |
| 1.0 | VN-106, VN-153, VN-154, VN-161, VN-162, VN-164, VN-011, VN-165, VN-166, VN-167, VN-012, VN-168, VN-169, VN-018 | 14 |
| Sau | VN-170, VN-016 | 2 |
| **Tổng** | | **57** |
