# DWG · Schema, codegen và codec DWG/DXF

> - Phạm vi §1–§7: DSL schema và codegen, bit-stream, chuỗi, bố cục tệp R13–R2018, section header/class/object map, giải mã song song, API mở tệp, nạp lười, Recover, `dwgdump`, ngân sách mở tệp. Bộ ghi DWG, DXF, schema theo loại, section phụ, thumbnail ở §8 trở đi.
> - Thư mục mã: `core/schema/`, `tools/codegen/`, `core/dwg/`, `core/dxf/`.
> - Căn cứ: [05](../05-kien-truc-ky-thuat.md) §1.1, §2, §6, §9.2–9.4, §14–16, §21; [06](../06-lo-trinh-trien-khai.md) §2–3.

## 0. Ghi chú thiết kế

- **Schema là nguồn sự thật duy nhất** (05 §6.2): mỗi loại đối tượng và biến header mô tả bằng DSL trong `core/schema/`. Bộ đọc/ghi DWG, đọc/ghi DXF, metadata thuộc tính đều sinh mã. Không viết tay bộ đọc/ghi theo loại. Cấu trúc đặc biệt (EED, khối đồ họa, đường biên hatch…) dùng hook `custom` có cặp đọc/ghi viết tay và kiểm round-trip.
- **Codegen là công cụ C++20 chạy lúc build** (`trion-codegen`). Không thêm Python, YAML hay JSON vào build. Mã sinh nằm trong thư mục build, không commit, giống nhau từng byte trên mọi máy **[đề xuất]**.
- **Clean-room** (05 §2, §15): nguồn được dùng là đặc tả DWG của ODA v5.4.1 (dẫn theo tên phần), DXF Reference của Autodesk, ACadSharp (MIT), thực nghiệm trên kho QA-001. Không đọc mã LibreDWG, chỉ chạy CLI của nó trong QA-004. Mọi PR ghi nguồn vào nhật ký DWG-101.
- **Đầu vào không tin cậy:** kiểm biên mọi lần đọc bit. Mọi số lượng, kích thước đọc từ tệp được so với số bit còn lại trước khi cấp phát. Lỗi trả `Status`, không dùng exception trên đường nóng **[đề xuất]**.
- **Fuzz:** mỗi parser có fuzz target riêng `core/dwg/fuzz/fuzz_<tên>.cpp`, đăng ký vào QA-006. Bản build fuzz bỏ kiểm CRC/checksum (cờ `TRION_FUZZ_NO_CRC`) để fuzzer vào được tầng sâu **[đề xuất]**.
- **Một parser, hai chế độ:** mở thường và Recover (DWG-020) dùng chung mã đọc, chỉ khác chính sách xử lý lỗi. Lỗi container dừng mở tệp ở chế độ thường. Lỗi một đối tượng được cô lập: tệp vẫn mở và được đánh dấu "nên RECOVER" **[đề xuất]**.
- **Song song và tất định:** giải nén theo trang, giải mã theo đối tượng qua NT-005; arena theo luồng (NT-007); tệp đọc qua mmap (NT-011). Kết quả ghi vào ô theo chỉ số trong object map, nên giống nhau từng bit ở mọi số luồng.
- **SIMD:** NT-006 có từ S2, nên task S1 viết scalar và chép khối bằng `memcpy` cỡ cố định để trình biên dịch tự vector hóa. Từ V, Reed–Solomon và LZ77 của R2007 dùng NT-006 kèm đường scalar đối chiếu **[đề xuất]**.
- **Chuỗi:** bên trong là UTF-8. Chuỗi TV trước 2007 giải mã theo code page của tệp và khôi phục được byte gốc (DWG-028), để Bác sĩ Font (VN-002) nhận diện TCVN3/VNI.
- **Ngân sách** (05 §1.1): mở 20 MB ≤ 1,5 s, 100 MB ≤ 5 s (tầm trung), ≤ 4 s và ≤ 15 s (máy thấp); bộ nhớ ≤ 1,2 GB cho tệp 100 MB; hiệu suất song song ≥ 70% đến 16 luồng. Mỗi tầng ghi phần ngân sách của mình, DWG-031 nghiệm thu tổng.

## 1. Pháp lý, bit-stream và chuỗi

### DWG-101 · Nhật ký nguồn tham khảo và kiểm clean-room trong CI
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** NT-002
- **Đầu vào:** 05 §2 (nguồn được phép), §15 (pháp lý codec DWG); 06 §2.1 (chính sách clean-room, rà soát của luật sư SHTT).
- **Đầu ra:**
  - `core/dwg/legal/REFERENCES.md`: quy trình clean-room, cam kết không đọc mã LibreDWG, mẫu mục nhật ký (ngày, task, nguồn, phiên bản tài liệu, tên phần, điều rút ra).
  - `core/dwg/legal/check_refs.py` chạy trong job PR của NT-002; mẫu PR có ô "Nguồn tham khảo".
- **Tiêu chí nghiệm thu:**
  1. PR sửa `core/dwg/`, `core/dxf/`, `core/schema/` hoặc `tools/codegen/` mà không thêm mục nhật ký → CI báo lỗi. 4 diff mẫu (thiếu mục, đủ mục, chỉ sửa tài liệu, chỉ đổi tên tệp) cho đúng kết quả kỳ vọng.
  2. Mục thiếu trường bắt buộc (nguồn, phiên bản tài liệu, tên phần, task) → CI báo lỗi.
  3. Chuỗi "libredwg" (không phân biệt hoa thường) hoặc header giấy phép GPL/AGPL trong 4 thư mục trên → CI báo lỗi; `qa/` được miễn.
  4. Script chạy ≤ 5 s trên toàn kho mã **[đề xuất]**, có kiểm thử cho cả 3 luật.
  5. Đạt DoD-C (phần áp dụng cho script).

### DWG-105 · Bit-stream lõi: kiểu số, MC/MS, handle reference
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-001, NT-003, NT-004, DWG-101
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1, phần Bit Codes and Data Definitions (BITSHORT, BITLONG, BITLONGLONG, BITDOUBLE, MODULAR CHARS, MODULAR SHORTS, HANDLE REFERENCES).
- **Đầu ra:**
  - `core/dwg/bitstream/bit_reader.h/.cpp`, `bit_writer.h/.cpp`: B, BB, 3B, BS, BL, BLL, BD, RC, RS, RL, RD, MC có và không dấu, MS, handle reference (mã 2–5 tuyệt đối; 6, 8, 0xA, 0xC tương đối theo handle gốc), bit thô; lỗi dính và vị trí bit.
  - Kiểm thử `core/dwg/tests/bitstream_*`, `core/dwg/bench/bench_bitstream.cpp`, `core/dwg/fuzz/fuzz_bitstream.cpp`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 200 vector vàng, đủ ca biên: MC 1–5 byte, MC âm, 0 và ±(2³¹−1); MS 1 và 2 từ; BLL 0–7 byte; handle có counter 0; handle tương đối tràn 64 bit → lỗi; mã 2 bit không hợp lệ của BL, BD → lỗi.
  2. Bộ ghi chọn mã ngắn nhất (BS 0 → `10`, 256 → `11`, 1–255 → `01`); read(write(x)) == x trên 10⁷ giá trị ngẫu nhiên mỗi kiểu, seed cố định.
  3. Đọc quá cuối bộ đệm trả 0 và bật lỗi dính, không chạm bộ nhớ ngoài (ASan), trên 10⁶ lần cắt cụt ngẫu nhiên.
  4. Đọc ≥ 80 triệu giá trị BS/s mỗi luồng trên máy tầm trung **[đề xuất]**; không cấp phát trong vòng đọc (NT-004).
  5. Fuzz target `fuzz_bitstream`: fuzzer chọn chuỗi lệnh đọc.
  6. Đạt DoD-C, DoD-H.

### DWG-003 · Bit-stream đọc/ghi đầy đủ: kiểu phức hợp, ba luồng của đối tượng, kiểm biên
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-105
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1, phần Bit Codes and Data Definitions (BITDOUBLE WITH DEFAULT, BITEXTRUSION, BITTHICKNESS, CMC, OBJECT TYPE), phần R2007 DWG File Format Organization (luồng chuỗi trong dữ liệu đối tượng); API DWG-105.
- **Đầu ra:**
  - `core/dwg/bitstream/`: TV (byte thô), TU (UTF-16LE), CMC và ENC (màu, transparency, tên màu, sách màu), BE, DD, BT, OT, 2RD, 3BD, 2DD, 3DD; đọc và ghi.
  - `object_streams.h/.cpp`: `ObjectReader`, `ObjectWriter` gom 3 con trỏ data, strings, handles theo phiên bản: R13–R14 một luồng; R2000+ luồng handle theo kích thước bit; R2007+ luồng chuỗi ở cuối dữ liệu; R2010+ kích thước luồng handle dạng MC.
  - `fuzz_object_streams.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Vector vàng mọi kiểu: TV, TU độ dài 0, 1, 32.767; CMC dạng chỉ số (R13–R2000) và RGB có cờ tên (R2004+); ENC có transparency; BE, BT dạng rút gọn; DD đủ 4 mã so với giá trị mặc định; OT đủ 3 dạng.
  2. Định vị luồng chuỗi: ≥ 12 ca vàng (có/không có luồng, kích thước 2 phần, kích thước vượt dữ liệu → lỗi).
  3. read(write(x)) == x cho mọi kiểu phức hợp trên 10⁷ giá trị ngẫu nhiên, đủ 8 phiên bản AC1012–AC1032; đối tượng tổng hợp ghi rồi đọc: 3 con trỏ dừng đúng bit cuối.
  4. Fuzz target `fuzz_object_streams`.
  5. Đạt DoD-C, DoD-H.

### DWG-028 · Mã hóa chuỗi: DWGCODEPAGE cho bản trước 2007, `\U+xxxx`, `\M+`
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-012, DWG-101
- **Đầu vào:** DXF Reference, biến `$DWGCODEPAGE`; đặc tả DWG của ODA v5.4.1, phần R13-R15 DWG File Format Organization (trường code page của header tệp); bảng code page NT-012.
- **Đầu ra:** `core/dwg/strings/dwg_strings.h/.cpp`, `fuzz_dwg_strings.cpp`:
  - số code page của tệp ↔ code page Windows; TV ↔ UTF-8; khi ghi bản trước 2007, ký tự ngoài code page → `\U+XXXX`; `\M+nXXXX` theo bảng code page CJK của đặc tả;
  - TU ↔ UTF-8 không mất mát, surrogate lẻ giữ dạng WTF-8 **[đề xuất]**.
- **Tiêu chí nghiệm thu:**
  1. encode(decode(b)) == b cho 100% chuỗi TV trong kho QA-001 và 10⁶ chuỗi byte ngẫu nhiên trên mỗi code page hỗ trợ, kể cả byte chưa định nghĩa của 1252 và cặp DBCS lỗi.
  2. 1.000 chuỗi TCVN3, VNI-Windows lưu trong tệp code page 1252: khôi phục đúng 100% byte gốc cho VN-002.
  3. ≥ 150 vector `\U+`, `\M+`; chuỗi thoát hỏng (`\U+12`, `\U+ZZZZ`, chỉ số `\M+` lạ) giữ nguyên dạng chữ.
  4. Mọi giá trị code page gặp trong kho QA-001 có ánh xạ; giá trị lạ → ANSI_1252 kèm cảnh báo trong kết quả mở. Đường ASCII thuần ≥ 1 GB/s mỗi luồng **[đề xuất]**.
  5. Fuzz target `fuzz_dwg_strings`.
  6. Đạt DoD-C.

## 2. Schema và codegen

### DWG-001 · DSL schema: trường, kiểu bit, điều kiện theo phiên bản, mã nhóm DXF, tên thuộc tính; parser schema
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-001, DWG-101
- **Đầu vào:** 05 §6.1–6.2; đặc tả DWG của ODA v5.4.1, phần Bit Codes and Data Definitions; DXF Reference (mã nhóm, subclass marker); nhu cầu DOC-003, DOC-017, LSP-017, QA-003.
- **Đầu ra:**
  - `core/schema/DSL.md`: kiểu bit (đủ danh sách DWG-003), mảng có trường đếm, điều kiện theo phiên bản và theo cờ, kế thừa (phần chung entity/object), mã nhóm DXF và subclass, tên thuộc tính, đơn vị (khoảng cách, góc), cờ chỉ đọc, loại handle (sở hữu/trỏ, cứng/mềm), hook `custom`, chú thích `@tên(giá trị)` chuyển nguyên cho backend.
  - `core/schema/parser/`: lexer, parser, kiểm ngữ nghĩa, IR `schema::Module`; kiểm thử `core/schema/tests/`.
- **Tiêu chí nghiệm thu:**
  1. Schema mẫu phủ 100% kiểu bit và mọi dạng điều kiện; IR in ra khớp tệp vàng.
  2. 40 ca lỗi cố ý (trùng tên, mã nhóm sai kiểu giá trị, khoảng phiên bản rỗng, trường đếm không tồn tại, kế thừa vòng) báo đúng tệp:dòng:cột, báo hết lỗi trong một lần chạy.
  3. In IR ra dạng chuẩn rồi parse lại cho IR y hệt trên mọi tệp schema.
  4. Parse 15.000 dòng schema ≤ 200 ms **[đề xuất]**.
  5. Schema là đầu vào tin cậy trong kho mã: `fuzz_schema` chạy 10 phút không crash là đủ **[đề xuất]**.
  6. Đạt DoD-C.

### DWG-102 · Codegen: kiểu C++ và bảng metadata thuộc tính
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-001
- **Đầu vào:** IR DWG-001; nhu cầu DOC-003 (kiểu trong `core/doc/generated`), DOC-017 (đọc/ghi theo mã thuộc tính), QA-003 (so từng trường).
- **Đầu ra:**
  - `tools/codegen/`: công cụ `trion-codegen`; backend `types` (struct theo loại, enum `TypeId`, phần chung kế thừa) và `meta` (bảng loại: tên, tên DXF, entity/object, khoảng phiên bản; bảng trường: tên thuộc tính, kiểu, mã nhóm, đơn vị, cờ, accessor đọc/ghi).
  - Cơ chế đăng ký backend để DOC-003 (lưu trữ) và LSP-017 (`vla-*`) thêm backend riêng; kiểm thử vàng `tools/codegen/tests/`.
- **Tiêu chí nghiệm thu:**
  1. Mã sinh từ schema mẫu biên dịch sạch trên MSVC và Clang, cảnh báo là lỗi.
  2. Đầu ra khớp tệp vàng từng byte; hai lần chạy liên tiếp cho cùng hash.
  3. Get/set qua metadata đúng với 100% trường của schema mẫu (ca kiểm sinh từ chính bảng metadata).
  4. Sinh mã cho 120 loại ≤ 2 s **[đề xuất]**.
  5. Đạt DoD-C.

### DWG-103 · Codegen: bộ đọc/ghi DWG theo phiên bản
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DWG-102, DWG-003
- **Đầu vào:** IR DWG-001; `ObjectReader`, `ObjectWriter` DWG-003; đặc tả DWG của ODA v5.4.1, phần Data Section AcDb:AcDbObjects (thứ tự trường, handle ở cuối đối tượng).
- **Đầu ra:** backend `dwg` trong `tools/codegen/`:
  - `readDwg(T&, ObjectReader&)`, `writeDwg(const T&, ObjectWriter&)` cho từng loại, chuyên biệt theo nhóm phiên bản lúc sinh mã để vòng đọc không rẽ nhánh theo phiên bản **[đề xuất]**; bảng điều phối `TypeId` → hàm.
  - Trường văn bản tự chọn TV/TU và luồng chuỗi R2007+; handle đọc ở luồng handle đúng thứ tự đặc tả (R13–R14 đọc tiếp sau dữ liệu); số phần tử mảng so với số bit còn lại trước khi cấp phát; hook `custom` gọi hàm viết tay.
- **Tiêu chí nghiệm thu:**
  1. Schema mẫu, 10⁶ đối tượng ngẫu nhiên mỗi phiên bản AC1012–AC1032: ghi rồi đọc bằng nhau từng trường; 3 con trỏ dừng đúng bit cuối.
  2. Cắt cụt tại mọi vị trí bit của 10⁴ đối tượng → `Status` lỗi, không đọc ngoài biên (ASan); mảng khai 10⁹ phần tử bị từ chối, không cấp phát.
  3. Bộ đọc sinh cho một loại kiểu LINE và một loại có mảng đỉnh kiểu LWPOLYLINE chậm hơn bản viết tay đối chứng ≤ 5% **[đề xuất]**.
  4. Loại có kích thước cố định đọc không cấp phát heap (bộ đếm NT-004).
  5. Đạt DoD-C, DoD-H.

### DWG-104 · Codegen: đọc/ghi DXF theo mã nhóm
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-102
- **Đầu vào:** IR DWG-001; DXF Reference (mã nhóm, subclass marker, khác biệt theo phiên bản).
- **Đầu ra:**
  - `core/dxf/dxf_group.h`: cặp (mã nhóm, giá trị có kiểu), `DxfGroupCursor`, `DxfGroupSink`; bộ tách token ASCII và nhị phân (DWG-024, DWG-025) cấp dữ liệu qua đây.
  - Backend `dxf` trong `tools/codegen/`: `readDxf`, `writeDxf` theo loại và phiên bản; handle dạng hex; nhóm lặp (đỉnh, điểm điều khiển); nhóm không nhận ra gom theo thứ tự để giữ nguyên (DWG-018).
- **Tiêu chí nghiệm thu:**
  1. 10⁶ đối tượng ngẫu nhiên của schema mẫu, mọi phiên bản: ghi DXF rồi đọc bằng nhau từng trường.
  2. Thứ tự nhóm và subclass marker đúng theo schema và phiên bản (tệp vàng cho 10 loại mẫu).
  3. Nhóm lạ chen giữa nhóm đã biết: giữ nguyên 100% giá trị và vị trí khi ghi lại.
  4. Số thực ghi dạng ngắn nhất, đọc lại đúng từng bit (`std::to_chars`), giống nhau trên 3 hệ điều hành.
  5. Đạt DoD-C.

### DWG-002 · Codegen từ schema: bộ đọc/ghi DWG, đọc/ghi DXF, metadata thuộc tính (tích hợp và nghiệm thu)
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-103, DWG-104
- **Đầu vào:** backend `types`, `meta` (DWG-102), `dwg` (DWG-103), `dxf` (DWG-104); preset CMake NT-001.
- **Đầu ra:**
  - `tools/codegen/TrionSchema.cmake`: hàm `trion_generate_schema()` sinh mã vào thư mục build, phụ thuộc theo từng tệp schema.
  - Schema tổng hợp `core/schema/tests/synthetic/` (≥ 40 loại, mọi kiểu bit, mọi dạng điều kiện) và test đầu-cuối; mục "Thêm một loại đối tượng" trong `core/schema/DSL.md`.
- **Tiêu chí nghiệm thu:**
  1. Test đầu-cuối trên schema tổng hợp: round-trip DWG ở 8 phiên bản, round-trip DXF, get/set qua metadata đều đạt trong một lần chạy CI.
  2. Thêm một trường vào schema là đủ để đọc/ghi DWG, DXF và thấy trong metadata, không sửa mã viết tay (test "trường mới").
  3. Mã sinh giống nhau từng byte trên Windows, macOS, Linux (CI so hash).
  4. Sửa 1 tệp schema chỉ sinh lại tệp của loại đó và loại kế thừa nó; ≤ 5% đơn vị biên dịch phải dịch lại **[đề xuất]**.
  5. Sinh và biên dịch toàn bộ mã của 120 loại ≤ 90 s trên máy CI 8 nhân **[đề xuất]**.
  6. Đạt DoD-C.

## 3. Phiên bản và bố cục tệp R13–R2018

### DWG-004 · Nhận diện phiên bản (AC1012–AC1032), bố cục tệp R13–R2000 (section locator, CRC)
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-003, NT-011
- **Đầu vào:** đặc tả DWG của ODA v5.4.1, phần R13-R15 DWG File Format Organization; mmap NT-011; metadata phiên bản của kho QA-001.
- **Đầu ra:**
  - `core/dwg/container/version.h/.cpp`: `AC10xx` → `DwgVersion`; AC1009 trở về trước và mã lạ → lỗi `UnsupportedVersion` kèm tên phiên bản (R11/R12…).
  - `r13_layout.h/.cpp`: header tệp (bản bảo trì, code page, địa chỉ ảnh xem trước), section locator, sentinel; trả span không sao chép trên mmap.
  - `crc16.h/.cpp`: CRC-16 của DWG (bảng, slicing-by-8) dùng chung mọi phiên bản; `fuzz_r13_layout.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Nhận diện đúng phiên bản 100% tệp kho QA-001; đọc ≤ 4 KB và ≤ 100 µs mỗi tệp **[đề xuất]** (đủ nhanh cho xem trước ở hộp mở tệp).
  2. 100% tệp R13–R2000 của kho: đủ section locator, CRC header và sentinel đúng.
  3. Cắt cụt tại mọi offset trong 4 KB đầu → lỗi đúng loại, không crash; CRC sai → `CrcMismatch` có vị trí (Recover dùng lại).
  4. CRC-16 khớp vector vàng, ≥ 1 GB/s mỗi luồng **[đề xuất]**.
  5. Đạt DoD-C.

### DWG-107 · Giải nén LZ77 biến thể của R2004
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-003, NT-004, DWG-101
- **Đầu vào:** đặc tả DWG của ODA v5.4.1, phần R2004 DWG File Format Organization (mục nén).
- **Đầu ra:** `core/dwg/container/lz77_r2004.h/.cpp`: giải nén vào bộ đệm do người gọi cấp (kích thước biết trước từ header trang); chép match bằng `memcpy` 16 byte, xử lý chồng lấn; đường scalar từng byte để đối chiếu; bộ sinh luồng hợp lệ ngẫu nhiên (chỉ cho kiểm thử); `bench_lz77_r2004.cpp`, `fuzz_lz77_r2004.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Vector vàng từ ví dụ của đặc tả và ≥ 50 luồng tự soạn phủ mọi opcode; đường nhanh và scalar giống từng byte trên 10⁶ luồng hợp lệ ngẫu nhiên.
  2. 10⁶ luồng ác ý (offset trước đầu ra, độ dài tràn, thiếu mã kết thúc, vượt kích thước đích) → lỗi, không ghi ngoài bộ đệm (ASan).
  3. ≥ 600 MB/s đầu ra mỗi luồng trên máy tầm trung **[đề xuất]**; không cấp phát trong hàm (NT-004).
  4. Fuzz target `fuzz_lz77_r2004`.
  5. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** bộ nén LZ77 cho bộ ghi (DWG-021).

### DWG-106 · R2004+: giải mã header tệp, section page map, section info
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-004, DWG-107, NT-014
- **Đầu vào:** đặc tả DWG của ODA v5.4.1, phần R2004 DWG File Format Organization (header tệp mã hóa, section page map, section map, header trang dữ liệu, checksum); CRC32 và Adler-32 có seed của NT-014.
- **Đầu ra:** `core/dwg/container/r2004_header.h/.cpp`: giải mã khối header bằng dãy XOR giả ngẫu nhiên, kiểm CRC32; đọc page map (số trang → offset, kích thước, khoảng trống) và section info (tên, kích thước, số trang, cờ nén, cờ mã hóa, danh sách trang); giải mặt nạ header trang dữ liệu; kiểm checksum header và dữ liệu trang; `fuzz_r2004_container.cpp`.
- **Tiêu chí nghiệm thu:**
  1. 100% tệp AC1018, AC1024, AC1027, AC1032 của kho QA-001: chuỗi nhận diện sau giải mã đúng, CRC32 đúng, mọi trang trong page map có checksum đúng.
  2. Tự nhất quán: mỗi trang thuộc đúng một section, tổng kích thước trang khớp section; trang chồng nhau hoặc vượt cuối tệp → lỗi có vị trí.
  3. Đọc toàn bộ cấu trúc container của tệp 100 MB (chưa giải nén dữ liệu) ≤ 10 ms **[đề xuất]**.
  4. Fuzz target `fuzz_r2004_container`.
  5. Đạt DoD-C.

### DWG-005 · R2004+: lắp section, giải nén LZ77 song song theo trang, checksum trang
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-106, NT-005
- **Đầu vào:** cấu trúc container DWG-106; LZ77 DWG-107; `parallel_for` NT-005; BENCH-STD 20 MB, 100 MB (QA-001).
- **Đầu ra:** `core/dwg/container/r2004_sections.h/.cpp`: `SectionStore` lắp section (AcDb:Header, AcDb:Classes, AcDb:Handles, AcDb:AcDbObjects và section khác) từ trang; mỗi trang giải nén thẳng vào vị trí của nó trong bộ đệm section, song song; giải nén theo yêu cầu từng section, từng trang (cho DWG-019); tệp có mật khẩu → `EncryptedUnsupported`; mở rộng `fuzz_r2004_container` tới bước lắp section.
- **Tiêu chí nghiệm thu:**
  1. 100% section của mọi tệp R2004–R2018 trong kho QA-001 lắp đủ; mọi trang giải nén đúng kích thước khai báo, checksum đúng.
  2. Giải nén toàn bộ section của tệp 100 MB ≤ 0,5 s trên máy tầm trung **[đề xuất]**.
  3. Hiệu suất song song ≥ 70% đến 16 luồng (đo 1→32 luồng, lưu trong CI); kết quả giống từng byte ở mọi số luồng.
  4. Trang hỏng chỉ làm hỏng section chứa nó; lỗi có tên section và số trang.
  5. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** dựng container khi ghi (DWG-021, DWG-022).

## 4. Section header, class, object map và phần chung của đối tượng

### DWG-008 · Section biến header: đọc và ghi
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-002, DWG-004, DWG-005, DWG-028
- **Đầu vào:** đặc tả DWG của ODA v5.4.1, phần Data Section AcDb:Header; DXF Reference, mục HEADER; 100 tệp mẫu kèm DXF do ODA File Converter xuất sẵn (kho QA-001).
- **Đầu ra:**
  - `core/schema/header.schema`: khoảng 300 biến theo phiên bản, kể cả trường chưa tên của DWG; tên `$VAR`, mã nhóm DXF (DOC-010 dùng làm kiểu header).
  - `core/dwg/sections/header_section.h/.cpp`: khung section (sentinel, kích thước, CRC, luồng chuỗi R2007+), gọi bộ đọc/ghi sinh mã; biến tham chiếu (CLAYER, control object) giữ dạng handle; `fuzz_header_section.cpp`.
- **Tiêu chí nghiệm thu:**
  1. 100% tệp kho QA-001 thuộc các phiên bản đã có container (AC1021 kiểm ở DWG-006): CRC đúng, con trỏ đọc dừng đúng kích thước khai báo.
  2. Khớp `$HEADER` trong DXF của ODA ở ≥ 99,5% biến trên 100 tệp mẫu **[đề xuất]**; chênh lệch còn lại có giải thích trong `core/schema/header-notes.md`.
  3. Ghi rồi đọc: 100% biến bằng nhau; header không sửa thì byte section ghi ra giống bản gốc ở ≥ 95% tệp **[đề xuất]**.
  4. Fuzz target `fuzz_header_section`.
  5. Đạt DoD-C.

### DWG-009 · Section class (class động): đọc và ghi
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** DWG-003, DWG-004, DWG-005, DWG-028
- **Đầu vào:** đặc tả DWG của ODA v5.4.1, phần Data Section AcDb:Classes; DXF Reference, mục CLASSES; 100 tệp mẫu kèm DXF của ODA (như DWG-008).
- **Đầu ra:** `core/dwg/sections/classes_section.h/.cpp`: bản ghi class (số class ≥ 500, cờ proxy, tên ứng dụng, tên lớp C++, tên DXF, cờ zombie, entity/object, số instance và phiên bản từ R2004), luồng chuỗi R2007+; ánh xạ số class → `TypeId` theo tên DXF; class chưa có schema → loại "chưa hiểu" (DWG-016, DWG-018 xử lý tiếp); `fuzz_classes_section.cpp`.
- **Tiêu chí nghiệm thu:**
  1. 100% tệp kho QA-001 (AC1021 kiểm ở DWG-006): CRC đúng, đọc vừa hết kích thước khai báo.
  2. Tên DXF, tên ứng dụng, cờ proxy khớp mục CLASSES trong DXF của ODA 100% trên 100 tệp mẫu.
  3. Ghi rồi đọc bằng nhau từng trường; không sửa thì byte ghi ra giống bản gốc.
  4. Fuzz target `fuzz_classes_section`.
  5. Đạt DoD-C.

### DWG-111 · Object map: đọc, kiểm CRC từng khối, chỉ mục handle → offset
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** DWG-003, DWG-004, DWG-005
- **Đầu vào:** đặc tả DWG của ODA v5.4.1, phần Data Section AcDb:Handles (Object Map); section từ DWG-004 (R13–R2000) và DWG-005 (R2004+).
- **Đầu ra:** `core/dwg/sections/object_map.h/.cpp`: đọc các khối (kích thước RS big-endian, cặp delta handle/offset dạng MC, CRC); mảng SoA (handle, offset) sắp theo handle; báo handle trùng, offset ngoài section; `fuzz_object_map.cpp`.
- **Tiêu chí nghiệm thu:**
  1. 100% tệp kho QA-001 (AC1021 kiểm ở DWG-006): CRC mọi khối đúng; mọi offset trỏ tới đầu một đối tượng có kích thước và kiểu đọc được.
  2. 1 triệu mục đọc ≤ 10 ms trên máy tầm trung **[đề xuất]**, một lần cấp phát.
  3. Handle trùng, delta handle âm, offset ngoài section → lỗi có vị trí ở chế độ thường; danh sách bất thường trả cho Recover (DWG-112).
  4. Fuzz target `fuzz_object_map`.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** ghi object map (DWG-021).

### DWG-110 · Phần chung của đối tượng: khung đối tượng, EED, khối đồ họa, dữ liệu entity chung
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DWG-002, DWG-028, DWG-111
- **Đầu vào:** đặc tả DWG của ODA v5.4.1, phần Data Section AcDb:AcDbObjects (định dạng chung của entity và object), phần Extended Entity Data, phần Proxy Entity Graphics; DSL DWG-001.
- **Đầu ra:**
  - `core/schema/common.schema`: `ObjectCommon`, `EntityCommon` theo phiên bản (entity mode, reactor, xdictionary, màu, linetype, plot style, material, shadow, visual style, invisibility, lineweight; handle owner, layer, linetype, prev/next).
  - `core/dwg/objects/object_frame.h/.cpp`, `eed.h/.cpp`: khung đối tượng (kích thước MS, kiểu, kích thước bit dữ liệu R2000+, handle, CRC); EED từng mục, chuỗi EED có code page riêng trước 2007; khối đồ họa giữ thô; đọc và ghi; `fuzz_object_common.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Mọi đối tượng trong tệp R13–R2004 của bộ 1.000 tệp: phần chung đọc xong; owner trỏ tới handle có trong object map ở ≥ 99,9% đối tượng **[đề xuất]**; CRC từng đối tượng đúng 100% trên tệp không hỏng.
  2. EED: 100% mục đọc được, ghi lại giống từng byte khối EED gốc.
  3. 10⁶ đối tượng ngẫu nhiên mỗi phiên bản R13–R2007: ghi rồi đọc bằng nhau từng trường.
  4. Fuzz target `fuzz_object_common`.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** parser proxy graphics (RGN-020); giữ đối tượng chưa hiểu (DWG-018).

### DWG-007 · Đặc thù R2010/R2013/R2018: object map, luồng handle, thay đổi trường theo phiên bản
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-110
- **Đầu vào:** đặc tả DWG của ODA v5.4.1, phần R2010 DWG File Format Organization, phần R2013 DWG File Format Organization, phần R2018 DWG File Format Organization; DXF Reference (trường mới theo phiên bản).
- **Đầu ra:**
  - `core/dwg/objects/object_frame.*`: header đối tượng R2010+ (kích thước MS, kích thước luồng handle MC, kiểu OT), đọc và ghi; định vị luồng chuỗi và luồng handle từ hai kích thước này.
  - `core/schema/common.schema`: điều kiện R2010+, R2013+, R2018 cho phần chung; ca kiểm object map R2010–R2018 (định dạng không đổi so với R2004); `fuzz_object_r2010.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Mọi đối tượng trong tệp AC1024, AC1027, AC1032 của kho QA-001: dữ liệu và luồng handle khớp kích thước khai báo; phần chung đọc xong ở 100% đối tượng.
  2. 10⁶ đối tượng ngẫu nhiên mỗi phiên bản AC1024–AC1032: ghi rồi đọc bằng nhau từng trường.
  3. 50 bản vẽ lưu ở AC1018 và AC1032 bằng ODA File Converter: phần chung của từng đối tượng (so theo handle) giống nhau, trừ trường chỉ có ở bản mới.
  4. Fuzz target `fuzz_object_r2010`.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** section AcDsPrototype_1b chứa dữ liệu ACIS từ R2013 (DWG-017).

## 5. Giải mã song song, API mở tệp, `dwgdump`, nghiệm thu spike S1

### DWG-010 · Object map và giải mã đối tượng song song (work-stealing)
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DWG-111, DWG-007, DWG-009, DOC-001, DOC-003, NT-004, NT-005, NT-007
- **Đầu vào:** object map DWG-111; khung đối tượng DWG-110, DWG-007; bảng điều phối sinh mã DWG-002; handle → `ObjectId` DOC-001; kho kiểu DOC-003; BENCH-STD 20 MB (QA-001).
- **Đầu ra:** `core/dwg/objects/parallel_decode.h/.cpp`, `fuzz_dwg_open.cpp` (cả tệp):
  - chia object map thành lô, giải mã song song vào arena theo luồng; kết quả xếp theo chỉ số object map nên tất định; class ≥ 500 điều phối qua DWG-009;
  - loại chưa có schema → trạng thái "chưa hiểu" (DWG-018 giữ khối thô); lỗi một đối tượng được cô lập và đếm;
  - dựng bảng handle → `ObjectId` song song, nạp hàng loạt vào kho DOC-003; thống kê theo loại cho `dwgdump`.
- **Tiêu chí nghiệm thu:**
  1. Giải mã toàn bộ đối tượng tệp BENCH-STD 20 MB ≤ 0,7 s trên máy tầm trung **[đề xuất]** (phần của ngân sách 1,5 s).
  2. Hiệu suất song song ≥ 70% đến 16 luồng; đường cong 1→32 luồng lưu trong CI.
  3. Hash mô hình sau khi nạp giống nhau ở 1, 2, 4, 8, 16, 32 luồng và trên 3 hệ điều hành.
  4. ≤ 1 lần cấp phát heap trên 1.000 đối tượng (bộ đếm NT-004) **[đề xuất]**.
  5. Tệp có 1% đối tượng bị làm hỏng: 100% đối tượng còn lại vẫn nạp; `fuzz_dwg_open` chạy ≥ 1 giờ không lỗi.
  6. Đạt DoD-C, DoD-H.

### DWG-114 · API mở tệp DWG: điều phối theo phiên bản, tiến trình, hủy
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** DWG-008, DWG-010, NT-005, NT-011
- **Đầu vào:** các tầng DWG-004 đến DWG-010; hủy hợp tác của NT-005; 07.1 §12 (thao tác dài).
- **Đầu ra:** `core/dwg/dwg_reader.h/.cpp`: `openDwg(path, OpenOptions) → OpenResult` (document, cảnh báo, thống kê theo tầng). Thứ tự: phiên bản và code page → container → header → class → object map → đối tượng. Tùy chọn số luồng; callback tiến trình, token hủy; mã lỗi có kiểu (phiên bản chưa hỗ trợ, có mật khẩu, hỏng container, I/O).
- **Tiêu chí nghiệm thu:**
  1. Mỗi mã lỗi có ca kiểm và mã thông điệp vi/en; tệp R12 báo "R12, chưa hỗ trợ", không báo "tệp hỏng".
  2. Tiến trình báo ≥ 10 lần/s, đơn điệu, đạt 100% khi xong **[đề xuất]**.
  3. Hủy giữa chừng dừng trong ≤ 50 ms và trả hết bộ nhớ (bộ đếm NT-004).
  4. Chi phí điều phối ≤ 2% thời gian mở tệp 20 MB **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.

### DWG-030 · Công cụ `dwgdump`: in đối tượng ra JSON cho QA và gỡ lỗi
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-114, DWG-104
- **Đầu vào:** `openDwg` DWG-114; metadata DWG-102; bộ ghi DXF sinh mã DWG-104; nhu cầu QA-002, QA-003, QA-004.
- **Đầu ra:** `core/dwg/tools/dwgdump/` và `FORMAT.md`:
  - `dwgdump <tệp> [--header] [--classes] [--sections] [--objects] [--handle H] [--type T] [--format json|dxfgroups] [--stats] [--threads N]`;
  - JSON thứ tự khóa cố định, số thực dạng ngắn nhất (`std::to_chars`); `--stats`: thời gian từng tầng, bộ nhớ đỉnh, số đối tượng theo loại, lỗi;
  - mã thoát 0 (đủ), 1 (lỗi đối tượng), 2 (không mở được), 3 (phiên bản chưa hỗ trợ).
- **Tiêu chí nghiệm thu:**
  1. JSON giống nhau từng byte trên 3 hệ điều hành và mọi `--threads` cho bộ 1.000 tệp QA-001.
  2. 100% đầu ra là JSON hợp lệ, kiểm bằng một bộ parse độc lập trong CI.
  3. `--format dxfgroups` dùng bộ ghi DXF DWG-104, cùng thứ tự nhóm như DXF để QA-004 so trực tiếp với DXF của ODA.
  4. Dump toàn bộ tệp 20 MB ≤ 3 s trên máy tầm trung **[đề xuất]**; 4 mã thoát có ca kiểm.
  5. Đạt DoD-C.

### DWG-113 · Nghiệm thu spike S1: đọc R2000, R2004, R2018 trên bộ 1.000 tệp
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** DWG-030, DWG-011, DWG-012, QA-002, QA-004
- **Đầu vào:** 05 §21 (tiêu chí S1); 06 §2.2 (cổng G0); bộ 1.000 tệp QA-001; bộ chạy QA-002; DXF của ODA File Converter và thời gian mở của DWG TrueView qua harness QA-004.
- **Đầu ra:** `core/dwg/reports/spike-s1.md` sinh tự động từ JSON của QA-002: tỉ lệ đọc theo phiên bản, thời gian so với DWG TrueView, phân loại lỗi, giới hạn còn lại (05 §17); job đêm chạy lại báo cáo.
- **Tiêu chí nghiệm thu:**
  1. ≥ 90% tệp R2000, R2004, R2018 của bộ 1.000 tệp đọc được ở mức entity lõi: container, header, class, object map không lỗi; ≥ 99% entity thuộc các loại của DWG-011 giải mã không lỗi; số entity theo loại khớp DXF của ODA.
  2. Thời gian mở trung vị bằng hoặc nhanh hơn DWG TrueView trên cùng máy, báo riêng từng phiên bản.
  3. 100% tệp lỗi được phân loại theo tầng (container, header, class, object map, đối tượng) và loại đối tượng.
  4. Chạy lại cho cùng tỉ lệ và cùng danh sách tệp lỗi.
  5. Đạt DoD-C.

## 6. R2007, nạp lười và Recover

### DWG-108 · Reed–Solomon của R2007: kiểm, sửa lỗi, khử xen kẽ
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-003, NT-006, DWG-101
- **Đầu vào:** đặc tả DWG của ODA v5.4.1, phần R2007 DWG File Format Organization (mục Reed–Solomon: tham số mã, hệ số xen kẽ theo loại trang); lớp SIMD NT-006.
- **Đầu ra:** `core/dwg/container/reed_solomon.h/.cpp`: GF(256), khử xen kẽ, syndrome bằng SIMD (đường nhanh khi không lỗi), sửa lỗi Berlekamp–Massey + Chien + Forney, bộ mã hóa (cho kiểm thử và DWG-023); `bench_reed_solomon.cpp`, `fuzz_rs2007.cpp`.
- **Tiêu chí nghiệm thu:**
  1. 10⁶ khối ngẫu nhiên có 0…t byte lỗi: sửa đúng 100%; khối có > t lỗi: báo không sửa được ở ≥ 99,9% ca **[đề xuất]**.
  2. Đường nhanh ≥ 1 GB/s mỗi luồng trên máy tầm trung **[đề xuất]**; SIMD và scalar cho kết quả giống từng byte.
  3. Khối header tệp của 100% tệp AC1021 trong kho QA-001 có syndrome bằng 0 và khử xen kẽ đúng kích thước.
  4. Fuzz target `fuzz_rs2007`.
  5. Đạt DoD-C, DoD-H.

### DWG-109 · Giải nén LZ77 biến thể của R2007
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** NT-003, NT-006, DWG-108
- **Đầu vào:** đặc tả DWG của ODA v5.4.1, phần R2007 DWG File Format Organization (mục nén); lớp SIMD NT-006.
- **Đầu ra:** `core/dwg/container/lz77_r2007.h/.cpp`: giải nén vào bộ đệm biết trước kích thước, chép literal và match qua NT-006, đường scalar đối chiếu; bộ sinh luồng hợp lệ ngẫu nhiên cho kiểm thử; `bench_lz77_r2007.cpp`, `fuzz_lz77_r2007.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Khối header tệp (sau RS) của 100% tệp AC1021 trong kho QA-001 giải nén đúng kích thước khai báo; đường nhanh và scalar giống từng byte trên 10⁶ luồng hợp lệ.
  2. 10⁶ luồng ác ý (offset trước đầu ra, độ dài tràn, vượt kích thước đích) → lỗi, không ghi ngoài bộ đệm (ASan).
  3. ≥ 500 MB/s đầu ra mỗi luồng trên máy tầm trung **[đề xuất]**; không cấp phát trong hàm.
  4. Fuzz target `fuzz_lz77_r2007`.
  5. Đạt DoD-C, DoD-H.

### DWG-006 · R2007: Reed–Solomon, tổ chức trang/section, chuỗi UTF-16, tách luồng data/strings/handles
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-108, DWG-109, DWG-114, NT-005
- **Đầu vào:** đặc tả DWG của ODA v5.4.1, phần R2007 DWG File Format Organization; RS DWG-108; LZ77 DWG-109; TU và `ObjectReader` 3 luồng của DWG-003; `openDwg` DWG-114.
- **Đầu ra:** `core/dwg/container/r2007_container.h/.cpp`, `crc64.h/.cpp`, `fuzz_r2007_container.cpp`:
  - header tệp (RS, nén, CRC-64), pages map, sections map (tên section UTF-16); trang dữ liệu RS → giải nén → lắp, song song theo trang;
  - nối vào `openDwg` thành họ container thứ ba; header, class, đối tượng AC1021 đọc qua luồng chuỗi và luồng handle của DWG-003.
- **Tiêu chí nghiệm thu:**
  1. 100% tệp AC1021 của bộ 10.000 tệp QA-001: header tệp, pages map, sections map, section header, class, object map đọc được; CRC-64 và checksum đúng.
  2. 50 bản vẽ lưu ở AC1021 và AC1018 bằng ODA File Converter: ≥ 99,9% đối tượng thuộc loại đã có schema giống nhau từng trường (so theo handle) **[đề xuất]**; phần khác có giải thích.
  3. Chuỗi UTF-16 có surrogate lẻ giữ không mất (so với byte gốc qua DWG-028).
  4. Giải mã trang đạt hiệu suất song song ≥ 70% đến 16 luồng; kết quả giống từng byte ở mọi số luồng.
  5. Fuzz target `fuzz_r2007_container`.
  6. Đạt DoD-C, DoD-H.

### DWG-019 · Chế độ giải mã lười cho Viewer và tệp lớn
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-114, DOC-016
- **Đầu vào:** 05 §6.2 mục 3; trạng thái "chưa giải mã" và vùng ưu tiên của DOC-016; giải nén theo trang DWG-005; BENCH-STD 100 MB, tệp nhiều layout và block lồng (QA-001).
- **Đầu ra:** `core/dwg/lazy/lazy_source.h/.cpp`; tùy chọn `--lazy` của `dwgdump`:
  - lượt nhẹ: kiểu, owner, layer, entity mode (nhảy thẳng tới luồng handle, không giải mã trường riêng); trang section giải nén khi chạm lần đầu;
  - giải mã đầy đủ theo yêu cầu, an toàn đa luồng, mỗi đối tượng đúng một lần; ưu tiên Model hoặc layout hiện hành, block được tham chiếu, layer bật và không đóng băng; phần còn lại giải mã nền ưu tiên thấp, hủy được.
- **Tiêu chí nghiệm thu:**
  1. Lượt nhẹ của tệp 100 MB ≤ 40% thời gian giải mã đầy đủ **[đề xuất]**.
  2. Tệp 20 layout, 5.000 định nghĩa block: Model sẵn sàng hiển thị trong ≤ 50% thời gian nạp đủ **[đề xuất]**.
  3. Giải mã theo yêu cầu một đối tượng ≤ 50 µs p99, không tính giải nén trang **[đề xuất]**; trang chạm lần đầu ≤ 2 ms **[đề xuất]**.
  4. Sau khi nạp nền xong, hash mô hình giống chế độ nạp đủ ở mọi số luồng; bộ nhớ đỉnh không vượt chế độ nạp đủ; TSan sạch khi UI đọc trong lúc nạp.
  5. Hủy nạp nền dừng trong ≤ 50 ms.
  6. Đạt DoD-C, DoD-H, DoD-L.
- **Ngoài phạm vi:** trạng thái và lịch công bố phía mô hình (DOC-016).

### DWG-112 · Quét khôi phục: tìm trang, dựng lại page map, section map, object map
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-005, DWG-006, DWG-111, NT-005
- **Đầu vào:** 05 §6.2 mục 4; cấu trúc trang DWG-106, DWG-006; RS DWG-108; object map DWG-111.
- **Đầu ra:** `core/dwg/recover/scan.h/.cpp`, `fuzz_recover_scan.cpp`:
  - R2004+: quét tệp tìm header trang theo chữ ký đã giải mặt nạ, dựng lại section theo id section và số trang; R2007: sửa trang bằng RS trước khi quét;
  - mọi phiên bản: dựng lại object map bằng quét đối tượng hợp lệ (kích thước, kiểu, CRC); tìm lại đầu đối tượng khi offset lệch trong cửa sổ ±64 byte **[đề xuất]**; quét song song theo vùng tệp.
- **Tiêu chí nghiệm thu:**
  1. Page map bị xóa trắng (200 tệp tổng hợp từ kho): 100% trang dữ liệu được tìm lại, section lắp đúng từng byte.
  2. Object map bị xóa: ≥ 99% đối tượng được tìm lại **[đề xuất]**; không có đối tượng ma (mỗi đối tượng tìm lại có CRC đúng).
  3. Offset lệch ngẫu nhiên 1–64 byte ở 5% mục: ≥ 99% được chỉnh lại **[đề xuất]**.
  4. Quét tệp 100 MB ≤ 3 s trên máy tầm trung **[đề xuất]**; fuzz target `fuzz_recover_scan`.
  5. Đạt DoD-C, DoD-H.

### DWG-020 · Chế độ Recover: chịu CRC sai, offset lệch, đối tượng thiếu; nhật ký sửa
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-112, DWG-114
- **Đầu vào:** 05 §6.2 mục 4; quét DWG-112; mã lỗi các tầng DWG-004 đến DWG-010; nhu cầu lệnh RECOVER và hộp thoại kết quả Recover.
- **Đầu ra:** `core/dwg/recover/recover.h/.cpp`, `repair_log.h/.cpp`, `fuzz_dwg_recover.cpp`; tùy chọn `--recover` của `dwgdump`:
  - chính sách theo mã lỗi: CRC sai mà cấu trúc đọc được → giữ và ghi log; header hỏng → giá trị mặc định; đối tượng không giải mã được → bỏ, ghi handle vào log;
  - `RepairLog`: mức, tầng, vị trí (section, trang, offset, handle), mã lỗi, hành động; xuất JSON và tóm tắt theo mã thông điệp vi/en.
- **Tiêu chí nghiệm thu:**
  1. Bộ 1.000 tệp hỏng sinh từ kho QA-001 với 6 kiểu hỏng (lật bit trong trang, cắt đuôi, xóa page map, lệch offset, sai CRC, xóa đối tượng) **[đề xuất]**: ≥ 95% mở được; trung bình giữ ≥ 90% đối tượng không bị chạm **[đề xuất]**.
  2. 100% hỏng hóc cấy vào được ghi trong nhật ký đúng tầng và vị trí.
  3. Tệp không hỏng mở bằng Recover cho mô hình cùng hash với mở thường, nhật ký rỗng.
  4. Không treo: mỗi tệp hỏng xong ≤ 10 s **[đề xuất]**; fuzz target `fuzz_dwg_recover`.
  5. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** sửa lỗi cấp mô hình như owner, tham chiếu treo (DOC-015); lệnh RECOVER (CMD).

## 7. Ngân sách mở tệp và nghiệm thu Viewer

### DWG-031 · Ngân sách mở tệp: DWG 20 MB ≤ 1,5 s, 100 MB ≤ 5 s; bộ nhớ ≤ 1,2 GB cho 100 MB; scaling ≥ 70% đến 16 luồng
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-006, DWG-019, DOC-113, QA-002, QA-004, NT-003
- **Đầu vào:** 05 §1.1; BENCH-STD 20 MB và 100 MB ở mọi phiên bản có trong bộ (QA-001); bộ chạy QA-002; số đo bộ nhớ mô hình DOC-113; 4 máy tham chiếu (README §3.1); DWG TrueView qua QA-004.
- **Đầu ra:** `core/dwg/bench/bench_open.cpp` (mở đầu-cuối, thời gian theo tầng, nạp đủ và nạp lười); job đêm trên 4 máy; các bản sửa hiệu năng ở tầng vượt ngân sách, mỗi bản kèm số đo trước/sau; `core/dwg/reports/open-budget.md`.
- **Tiêu chí nghiệm thu:**
  1. Mở tới khi mô hình nạp đủ: 20 MB ≤ 1,5 s và 100 MB ≤ 5 s trên máy tầm trung; ≤ 4 s và ≤ 15 s trên máy thấp; đạt ở mọi phiên bản có trong BENCH-STD.
  2. Bộ nhớ đỉnh ≤ 1,2 GB khi mở tệp 100 MB, gồm cả mô hình (DOC-113).
  3. Hiệu suất song song ≥ 70% đến 16 luồng trên máy mạnh; đường cong 1→32 luồng lưu trong CI.
  4. Nhanh hơn DWG TrueView ở ≥ 70% bài đo mở tệp (06 §3).
  5. CI chặn merge khi chậm hơn baseline quá 5% (NT-003).
  6. Đạt DoD-C, DoD-H.

### DWG-115 · Nghiệm thu Viewer: mở ≥ 98% bộ 10.000 tệp R13–R2018
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** DWG-006, DWG-020, DWG-013, DWG-014, DWG-015, DWG-016, QA-002, QA-004
- **Đầu vào:** 06 §3 (KPI cuối Giai đoạn 1); 05 §1.2 (≥ 99,5% ở v1.0); bộ 10.000 tệp QA-001; DXF đối chứng của ODA File Converter (QA-004).
- **Đầu ra:** job đêm chạy toàn bộ 10.000 tệp qua QA-002 và `dwgdump`; báo cáo `core/dwg/reports/corpus.md` theo phiên bản và tầng lỗi, kèm danh sách tệp mới lỗi so với đêm trước.
- **Tiêu chí nghiệm thu:**
  1. ≥ 98% tệp mở ở chế độ thường, không lỗi container và không lỗi đối tượng; mỗi phiên bản R13–R2018 ≥ 97% **[đề xuất]**.
  2. Số entity và object theo loại khớp DXF của ODA ở ≥ 99% tệp mở được **[đề xuất]**.
  3. Không tệp nào làm bộ đọc crash hoặc chạy quá 60 s **[đề xuất]**.
  4. Job đêm báo lỗi khi tỉ lệ mở giảm quá 0,2 điểm phần trăm so với đêm trước **[đề xuất]**; báo cáo ghi khoảng cách tới mục tiêu ≥ 99,5% của v1.0.
  5. Đạt DoD-C.

## 8. Schema lõi (S1) và metadata thuộc tính

### DWG-151 · Schema entity đường cong, điểm, text, block và attribute
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DWG-002, DWG-110
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1, phần các entity liên quan; DXF Reference của Autodesk; phần chung `EntityCommon` (DWG-110); mã hóa chuỗi DWG-028.
- **Đầu ra:**
  - `core/schema/entity/*.schema`: LINE, CIRCLE, ARC, ELLIPSE, SPLINE, LWPOLYLINE, POLYLINE_2D, POLYLINE_3D, VERTEX_2D, VERTEX_3D, SEQEND, POINT, SOLID, RAY, XLINE, TEXT, MTEXT, INSERT, MINSERT, ATTRIB, ATTDEF, BLOCK, ENDBLK (23 loại), kèm mã nhóm DXF.
  - MTEXT gồm nền che, giãn dòng, trường cột theo phiên bản; ATTRIB/ATTDEF gồm attribute nhiều dòng (MTEXT nhúng); INSERT gồm attribute sở hữu (prev/next ở R13–R2000, danh sách handle từ R2004).
  - Ca vàng `core/dwg/tests/golden/`: lát bit đối tượng theo phiên bản, giá trị kỳ vọng lấy từ DXF của ODA File Converter.
- **Tiêu chí nghiệm thu:**
  1. Ca vàng khớp 100% trường ở AC1012, AC1014, AC1015, AC1018, AC1024, AC1027, AC1032.
  2. **Khớp vừa khít:** giải mã xong thì con trỏ bit dừng đúng đầu luồng handle và luồng handle được đọc hết; đạt 100% ca vàng. Mọi task schema sau dùng phép kiểm này.
  3. Giải mã → mã hóa → giải mã giữ nguyên 100% trường; spline giữ đủ knot, trọng số, điểm fit; chuỗi tiếng Việt (TCVN3, VNI, `\U+`) giữ đúng từng byte, chuỗi ≥ 64 KB đọc đủ.
  4. INSERT có 10.000 attribute và MINSERT 100 × 100 giải mã đúng số lượng, không cấp phát theo từng phần tử (NT-004).
  5. Đạt DoD-C.

### DWG-011 · Schema entity lõi: kích thước, leader, hatch, viewport; nghiệm thu nhóm S1
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DWG-151, DWG-030, QA-001, QA-004
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1; DXF Reference; bộ 1.000 tệp (QA-001); DXF do ODA File Converter xuất từ cùng tệp (QA-004); JSON của `dwgdump` (DWG-030).
- **Đầu ra:**
  - `core/schema/entity/*.schema`: DIMENSION ORDINATE, LINEAR, ALIGNED, ANG3PT, ANG2LN, RADIUS, DIAMETER; LEADER; HATCH (biên gồm đoạn, cung, ellipse, spline, polyline; nét mẫu, gradient, điểm seed); VIEWPORT (10 loại).
  - Fuzz target `core/dwg/fuzz/fuzz_dwg_object.cpp` (loại, phiên bản, bit) phủ mọi loại có schema; hạt giống lấy từ ca vàng.
  - Báo cáo phủ JSON trong CI: loại × phiên bản, số đối tượng, tỉ lệ khớp.
- **Tiêu chí nghiệm thu:**
  1. Bộ 1.000 tệp: 100% đối tượng thuộc 33 loại (DWG-151 và task này) khớp vừa khít.
  2. So với DXF của ODA File Converter: ≥ 99,9% đối tượng khớp mọi trường (số thực lệch tương đối ≤ 1e-12 **[đề xuất]**); phần còn lại có giải thích trong báo cáo.
  3. HATCH 10.000 vòng biên đọc đúng; DIMENSION có ghi đè DIMVAR trong xdata `ACAD`/`DSTYLE` đọc đúng.
  4. Đạt DoD-C.

### DWG-012 · Schema bảng ký hiệu và control object
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DWG-110, DWG-030, QA-004
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1, phần control object và table record; DXF Reference, section TABLES; bộ 1.000 tệp.
- **Đầu ra:** `core/schema/table/*.schema`: BLOCK_CONTROL, BLOCK_HEADER, LAYER_CONTROL, LAYER, STYLE_CONTROL, STYLE, LTYPE_CONTROL, LTYPE, VIEW_CONTROL, VIEW, UCS_CONTROL, UCS, VPORT_CONTROL, VPORT, APPID_CONTROL, APPID, DIMSTYLE_CONTROL, DIMSTYLE, VX_CONTROL, VX_TABLE_RECORD (20 loại); ca vàng theo phiên bản.
- **Tiêu chí nghiệm thu:**
  1. Bộ 1.000 tệp: 100% đối tượng 20 loại khớp vừa khít; ≥ 99,9% khớp mọi trường với DXF của ODA File Converter.
  2. Control object có đủ handle bản ghi, đúng thứ tự. Bản ghi mặc định (`*Model_Space`, `*Paper_Space`, layer `0`, `Standard`, `ByLayer`, `ByBlock`, `Continuous`) nhận đúng 100%.
  3. DIMSTYLE đủ DIMVAR theo từng phiên bản, kể cả handle DIMTXSTY, DIMLDRBLK, DIMLTYPE. LTYPE đủ mảng nét, shape/text nhúng, vùng chuỗi 256/512 byte theo phiên bản.
  4. LAYER đủ cờ (tắt, đóng băng, khóa, đóng băng ở viewport mới, không in), true color, lineweight, plot style, material; transparency đọc từ xdata `AcCmTransparency`.
  5. Đạt DoD-C.

### DWG-166 · Điền và kiểm metadata thuộc tính cho Properties, LISP, so sánh
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-011, DWG-012, QA-008
- **Đầu vào:** Cú pháp tên thuộc tính, đơn vị, cờ và chú thích `@tên(giá trị)` (DWG-001), bảng metadata (DWG-102); tài liệu ActiveX công khai của AutoCAD; kết quả kỳ vọng `entget` trong bộ LISP (QA-008).
- **Đầu ra:**
  - Quy ước chú thích trong `core/schema/CONVENTIONS.md`: khóa chuỗi hiển thị vi/en, nhóm Properties, loại đơn vị (tọa độ, khoảng cách, góc, tỉ lệ), sửa được hay chỉ đọc, thứ tự `entget`/`entmod`, tên ActiveX cho `vla-*`, cờ so sánh bản vẽ, cờ `internal`.
  - Chú thích đủ cho 53 loại của DWG-151, DWG-011, DWG-012.
  - Luật lint trong `tools/codegen/`: chặn merge khi trường thiếu chú thích mà không gắn `internal`.
- **Tiêu chí nghiệm thu:**
  1. Lint đạt 100% trường của 53 loại; mọi task schema sau chạy cùng luật trong CI.
  2. Danh sách mã nhóm sinh cho `entget` khớp thứ tự và giá trị với kết quả kỳ vọng QA-008 ở mọi loại có trong bộ.
  3. Tên thuộc tính ActiveX của 33 loại entity khớp tệp tham chiếu soạn từ tài liệu công khai.
  4. Mọi trường tọa độ, khoảng cách, góc có loại đơn vị (kiểm tự động theo kiểu trường) để DOC-017 định dạng qua RGN-013.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** cơ chế metadata (DWG-001, DWG-102); API đọc/ghi theo mã thuộc tính (DOC-017); hàm `vla-*` (LSP-017).

## 9. Schema entity còn lại, dictionary, layout và kiểu

### DWG-153 · Schema entity còn lại; kiểm AC1021 cho mọi entity
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-011, DWG-166, DWG-006
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1; DXF Reference; bộ 10.000 tệp (QA-001); bộ đọc R2007 (DWG-006).
- **Đầu ra:** `core/schema/entity/*.schema`: TRACE, 3DFACE, SHAPE, POLYLINE_PFACE, POLYLINE_MESH, VERTEX_MESH, VERTEX_PFACE, VERTEX_PFACE_FACE, MLINE, TOLERANCE, OLEFRAME, OLE2FRAME, ARC_DIMENSION, LARGE_RADIAL_DIMENSION (14 loại); chú thích metadata theo DWG-166; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. Bộ 10.000 tệp: 100% đối tượng 14 loại khớp vừa khít; ≥ 99,9% khớp mọi trường với DXF của ODA File Converter.
  2. Tệp AC1021 của bộ 10.000 tệp: 47 loại entity (DWG-151, DWG-011, task này) khớp vừa khít 100%; chuỗi UTF-16 khớp DXF từng ký tự.
  3. OLE2FRAME giữ nguyên khối dữ liệu OLE từng byte; MLINE đủ phần tử và tham số đoạn theo số đường của kiểu.
  4. Đạt DoD-C.

### DWG-013 · Schema dictionary, đối tượng thường gặp, layout và thiết lập in
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DWG-012, DWG-166
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1; DXF Reference, section OBJECTS; bộ 10.000 tệp.
- **Đầu ra:**
  - `core/schema/object/*.schema`: DICTIONARY, ACDBDICTIONARYWDFLT, DICTIONARYVAR, XRECORD, GROUP, MLINESTYLE, SORTENTSTABLE, SCALE, ACDBPLACEHOLDER, IDBUFFER, LAYER_INDEX, SPATIAL_INDEX, SPATIAL_FILTER, LAYER_FILTER, VBA_PROJECT, LONG_TRANSACTION, LAYOUT, PLOTSETTINGS (18 loại); metadata; ca vàng.
  - Fuzz target `core/dwg/fuzz/fuzz_xrecord.cpp` cho dữ liệu XRECORD.
- **Tiêu chí nghiệm thu:**
  1. Bộ 10.000 tệp: 100% đối tượng 18 loại khớp vừa khít; ≥ 99,9% khớp mọi trường với DXF của ODA File Converter.
  2. DICTIONARY giữ đúng thứ tự mục, cờ hard owner và cloning; tra được mọi mục của named object dictionary (ACAD_GROUP, ACAD_LAYOUT, ACAD_PLOTSETTINGS, ACAD_PLOTSTYLENAME, ACAD_SCALELIST, ACAD_MLINESTYLE…).
  3. XRECORD: dữ liệu theo mã nhóm có kiểu (kể cả handle 330–369, nhị phân 310) giải mã → mã hóa giống từng bit.
  4. LAYOUT và PLOTSETTINGS đủ trường cho PLT-001 (thiết bị, khổ giấy, lề, vùng in, tỉ lệ, plot style, shade plot) ở mọi phiên bản; danh sách viewport của LAYOUT khớp DXF.
  5. Đạt DoD-C.

### DWG-155 · Schema kiểu hiển thị, vật liệu, ánh sáng, định vị địa lý, kiểu hình chiếu
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** DWG-013
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1; DXF Reference; bộ 10.000 tệp.
- **Đầu ra:** `core/schema/object/*.schema`: VISUALSTYLE, MATERIAL, DBCOLOR, SUN, SUNSTUDY, LIGHT, CAMERA, GEODATA, GEOPOSITIONMARKER, DETAILVIEWSTYLE, SECTIONVIEWSTYLE (11 loại); metadata; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. Bộ 10.000 tệp: 100% đối tượng 11 loại khớp vừa khít; ≥ 99,9% khớp mọi trường với DXF của ODA File Converter.
  2. VISUALSTYLE đủ trường theo từng phiên bản AC1021–AC1032; MATERIAL đủ các map (khuếch tán, phản xạ, độ mờ, bump, khúc xạ) và ma trận ánh xạ.
  3. DBCOLOR giữ đúng tên sách màu và tên màu; entity dùng màu sách màu trả đúng tên qua metadata.
  4. GEODATA đủ chuỗi hệ tọa độ, điểm thiết kế/tham chiếu và lưới chuyển đổi ở mọi phiên bản đối tượng (dùng cho bản vẽ VN-2000).
  5. Đạt DoD-C.

## 10. Schema bảng, field, MLEADER, khung liên kết, chú thích theo tỉ lệ, dynamic block

### DWG-154 · Schema bảng và field: ACAD_TABLE, TABLESTYLE, nội dung ô, FIELD
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DWG-013, DWG-151
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1, phần TABLE, đối tượng nội dung bảng, FIELD; DXF Reference; tệp có bảng gộp ô, ô block, ô field, data link trong bộ 10.000 tệp.
- **Đầu ra:** `core/schema/object/*.schema`: ACAD_TABLE, TABLESTYLE, CELLSTYLEMAP, TABLECONTENT, TABLEGEOMETRY, DATALINK, FIELD, FIELDLIST (8 loại); giá trị có kiểu dùng chung cho ô và field (số, chuỗi, ngày, điểm, handle, định dạng); metadata; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. Bộ 10.000 tệp: 100% đối tượng 8 loại khớp vừa khít; ≥ 99,9% khớp mọi trường với DXF của ODA File Converter.
  2. Bảng theo mô hình cũ và mô hình có TABLECONTENT/cell style đọc về cùng mô hình ô: gộp ô, ô block kèm attribute, ô field, ghi đè định dạng, chia đoạn bảng.
  3. FIELD: cây field lồng, mã biểu thức, giá trị cache mọi kiểu đọc đúng; TEXT, MTEXT, ô bảng tra được FIELD qua extension dictionary `ACAD_FIELD`.
  4. Block đại diện `*T` của bảng liên kết đúng cho RGN-017; bảng 1.000 × 50 ô giải mã ≤ 30 ms trên máy tầm trung **[đề xuất]**.
  5. Đạt DoD-C.

### DWG-167 · Schema khung liên kết (associative framework) và mảng liên kết
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** DWG-013
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1 và DXF Reference, phần đối tượng AcDbAssoc*; tệp có mảng liên kết (ARRAY từ AutoCAD 2012) và kích thước liên kết trong bộ 10.000 tệp.
- **Đầu ra:** `core/schema/assoc/*.schema`: ACDBASSOCNETWORK, ACDBASSOCACTION, ACDBASSOCDEPENDENCY, ACDBASSOCGEOMDEPENDENCY, ACDBASSOCVALUEDEPENDENCY, ACDBASSOCVARIABLE, ACDBASSOCARRAYACTIONBODY, ACDBASSOCARRAYRECTANGULARPARAMETERS, ACDBASSOCARRAYPOLARPARAMETERS, ACDBASSOCARRAYPATHPARAMETERS, ACDBASSOCVERTEXACTIONPARAM, ACDBASSOCEDGEACTIONPARAM, ACDBASSOCOBJECTACTIONPARAM, ACDBASSOCPATHACTIONPARAM, ACDBASSOCCOMPOUNDACTIONPARAM, ACDBASSOCOSNAPPOINTREFACTIONPARAM, ACDBASSOCPOINTREFACTIONPARAM, ACDBPERSSUBENTMANAGER, ACDBASSOCPERSSUBENTMANAGER (19 lớp); tham số dạng giá trị (tên, biểu thức, giá trị); metadata; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. Bộ 10.000 tệp: 100% đối tượng 19 lớp khớp vừa khít; ≥ 99,9% khớp mọi trường với DXF của ODA File Converter.
  2. Đồ thị liên kết toàn vẹn: network ↔ action ↔ dependency ↔ đối tượng không trỏ treo trên 100% tệp không hỏng.
  3. Từ INSERT của một mảng liên kết tra được action body, tham số (hàng, cột, tầng, khoảng cách, góc, số phần tử, đường dẫn) và danh sách phần tử bị ghi đè; đúng 100% trên tệp mẫu.
  4. Đạt DoD-C.
- **Ngoài phạm vi:** đánh giá lại mảng khi sửa (CMD-185); ràng buộc tham số 2D (giữ thô qua DWG-018).

### DWG-014 · Schema đối tượng nâng cao: MLEADER, Express text; nghiệm thu nhóm nâng cao
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DWG-154, DWG-155, DWG-167, DWG-151
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1, phần MULTILEADER, MLEADERSTYLE; DXF Reference (gồm ARCALIGNEDTEXT, RTEXT); bộ 10.000 tệp.
- **Đầu ra:**
  - `core/schema/object/*.schema`: MULTILEADER (context data: leader root, đường leader, điểm gãy, nội dung MTEXT hoặc block kèm attribute), MLEADERSTYLE, ARCALIGNEDTEXT (AcDbArcAlignedText), RTEXT (AcDbRText) (4 loại); metadata; ca vàng.
  - Nghiệm thu cả nhóm, để module khác tra: TABLE, TABLESTYLE, FIELD và nội dung bảng (DWG-154); VISUALSTYLE, MATERIAL, DBCOLOR và kiểu khác (DWG-155); khung liên kết AcDbAssocNetwork, AcDbAssocAction, AcDbAssocDependency, AcDbAssocGeomDependency, AcDbAssocArrayActionBody cùng tham số mảng chữ nhật/tròn/theo đường, AcDbAssocActionParam* (DWG-167).
- **Tiêu chí nghiệm thu:**
  1. Bộ 10.000 tệp: 100% đối tượng 4 loại khớp vừa khít; ≥ 99,9% khớp mọi trường với DXF của ODA File Converter.
  2. MLEADER: mọi biến thể nội dung (MTEXT, block, không nội dung), nhiều leader root, trường thêm từ AC1024 đọc đúng; MTEXT nhúng giữ nguyên mã định dạng.
  3. ARCALIGNEDTEXT đủ chuỗi, kiểu chữ, cung tham chiếu, căn lề, khoảng cách; RTEXT đủ nội dung chuỗi hoặc biểu thức DIESEL; đủ cho CMD-324.
  4. Cả nhóm (42 lớp của DWG-154, DWG-155, DWG-167 và task này) đạt tiêu chí 1 trên tệp AC1018–AC1032; giải mã → mã hóa → giải mã giữ nguyên 100% trường.
  5. Đạt DoD-C.

### DWG-158 · Schema object context data (chú thích theo tỉ lệ)
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-014, DWG-011
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1, phần object context data; DXF Reference; tệp có đối tượng annotative trong bộ 10.000 tệp.
- **Đầu ra:** `core/schema/context/*.schema`: ACDB_TEXTOBJECTCONTEXTDATA_CLASS, ACDB_MTEXTOBJECTCONTEXTDATA_CLASS, ACDB_MTEXTATTRIBUTEOBJECTCONTEXTDATA_CLASS, ACDB_BLKREFOBJECTCONTEXTDATA_CLASS, ACDB_LEADEROBJECTCONTEXTDATA_CLASS, ACDB_MLEADEROBJECTCONTEXTDATA_CLASS, ACDB_FCFOBJECTCONTEXTDATA_CLASS, ACDB_ALDIMOBJECTCONTEXTDATA_CLASS, ACDB_ANGDIMOBJECTCONTEXTDATA_CLASS, ACDB_DMDIMOBJECTCONTEXTDATA_CLASS, ACDB_ORDDIMOBJECTCONTEXTDATA_CLASS, ACDB_RADIMOBJECTCONTEXTDATA_CLASS, ACDB_RADIMLGOBJECTCONTEXTDATA_CLASS, ACDB_HATCHSCALECONTEXTDATA_CLASS, ACDB_HATCHVIEWCONTEXTDATA_CLASS (15 loại); metadata; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. Bộ 10.000 tệp: 100% đối tượng 15 loại khớp vừa khít; ≥ 99,9% khớp mọi trường với DXF của ODA File Converter.
  2. Từ mỗi entity annotative tra được mọi biểu diễn theo SCALE (qua extension dictionary `AcDbContextDataManager`) và biểu diễn mặc định; đúng 100% trên tệp mẫu, đủ cho DOC-021.
  3. Biểu diễn trỏ tới SCALE đã bị xóa được báo trong kết quả giải mã, không làm hỏng việc mở tệp.
  4. Đạt DoD-C.

### DWG-156 · Schema parameter và grip của dynamic block
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DWG-013
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1, phần đối tượng dynamic block; DXF Reference; tệp có dynamic block trong bộ 10.000 tệp.
- **Đầu ra:** `core/schema/dynblock/*.schema`: lớp cơ sở chung (biểu thức đánh giá, phần tử block, parameter 1 điểm và 2 điểm, grip) và BLOCKVISIBILITYPARAMETER, BLOCKVISIBILITYGRIP, BLOCKLINEARPARAMETER, BLOCKLINEARGRIP, BLOCKPOINTPARAMETER, BLOCKXYPARAMETER, BLOCKXYGRIP, BLOCKROTATIONPARAMETER, BLOCKROTATIONGRIP, BLOCKFLIPPARAMETER, BLOCKFLIPGRIP, BLOCKALIGNMENTPARAMETER, BLOCKALIGNMENTGRIP, BLOCKBASEPOINTPARAMETER, BLOCKPOLARPARAMETER, BLOCKPOLARGRIP, BLOCKLOOKUPPARAMETER, BLOCKLOOKUPGRIP (18 loại); metadata; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. Tệp có dynamic block của bộ 10.000 tệp: 100% đối tượng 18 loại khớp vừa khít; ≥ 99,9% khớp mọi trường với DXF của ODA File Converter.
  2. Trạng thái visibility: tên trạng thái và tập entity của từng trạng thái đúng 100% trên tệp mẫu.
  3. Tập giá trị của parameter (min, max, bước, danh sách) ở parameter tuyến tính, xoay, XY, polar đọc đủ.
  4. Đạt DoD-C.

### DWG-015 · Đối tượng dynamic block: EvalGraph, BlockRepresentation, action; đọc và giữ nguyên
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DWG-156
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1; DXF Reference; tệp có dynamic block trong bộ 10.000 tệp.
- **Đầu ra:**
  - `core/schema/dynblock/*.schema`: ACAD_EVALUATION_GRAPH (nút, cạnh, id biểu thức), ACDB_BLOCKREPRESENTATION_DATA, ACDB_DYNAMICBLOCKPURGEPREVENTER_VERSION, BLOCKMOVEACTION, BLOCKSTRETCHACTION, BLOCKSCALEACTION, BLOCKROTATEACTION, BLOCKFLIPACTION, BLOCKARRAYACTION, BLOCKPOLARSTRETCHACTION, BLOCKLOOKUPACTION, BLOCKPROPERTIESTABLE, BLOCKGRIPLOCATIONCOMPONENT, BLOCKPARAMDEPENDENCYBODY (14 loại); metadata; ca vàng.
  - API tra block `*U` → định nghĩa dynamic block gốc và giá trị tham số hiện tại của từng tham chiếu.
- **Tiêu chí nghiệm thu:**
  1. 100% đối tượng 14 loại khớp vừa khít; ≥ 99,9% khớp mọi trường với DXF của ODA File Converter.
  2. Mọi INSERT trỏ tới block `*U` tra ngược được định nghĩa gốc (qua BlockRepresentation và xdata `AcDbBlockRepBTag`) trên 100% tệp mẫu.
  3. EvalGraph không trỏ treo, id nút duy nhất, thứ tự nút và cạnh giữ nguyên để ghi lại đúng.
  4. Cả nhóm 32 loại (DWG-156 và task này): giải mã → mã hóa → giải mã giữ nguyên 100% trường.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** engine EvalGraph (RGN-019, 1.0); mô hình sửa dynamic block (DOC-023).

## 11. Ảnh, underlay, proxy và khối 3D

### DWG-157 · Schema ảnh raster, wipeout và underlay
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-013, DWG-011
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1; DXF Reference; tệp có ảnh, wipeout, PDF/DWF/DGN underlay trong bộ 10.000 tệp.
- **Đầu ra:** `core/schema/raster/*.schema`: IMAGE, IMAGEDEF, IMAGEDEF_REACTOR, RASTERVARIABLES, WIPEOUT, WIPEOUTVARIABLES, PDFDEFINITION, PDFUNDERLAY, DWFDEFINITION, DWFUNDERLAY, DGNDEFINITION, DGNUNDERLAY (12 loại); biên clip chữ nhật, đa giác, đảo ngược; metadata; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. 100% đối tượng 12 loại khớp vừa khít; ≥ 99,9% khớp mọi trường với DXF của ODA File Converter.
  2. Đường dẫn ảnh và underlay (tuyệt đối, tương đối, tên trang PDF) đọc đúng 100% cho việc tìm tệp (NT-011, UI-B06); liên kết IMAGE ↔ IMAGEDEF ↔ IMAGEDEF_REACTOR không trỏ treo.
  3. Biên clip đa giác 10.000 đỉnh và cờ clip đảo ngược đọc đúng.
  4. Đạt DoD-C.

### DWG-016 · Proxy entity, proxy object và proxy graphics: đọc, giữ nguyên
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** DWG-011, DWG-009
- **Đầu vào:** 03 §2 (proxy object, P1); Đặc tả DWG của ODA v5.4.1, phần PROXY ENTITY, PROXY OBJECT, proxy graphics; DXF Reference (ACAD_PROXY_ENTITY, ACAD_PROXY_OBJECT); tệp Civil 3D, Architecture, plugin trong bộ 10.000 tệp.
- **Đầu ra:**
  - `core/schema/proxy/*.schema` (2 loại) và `core/dwg/proxy/`: class id, phiên bản, cờ định dạng gốc, khối bit dữ liệu, danh sách handle, khối proxy graphics (span chỉ đọc cho RGN-020, kiểm biên từng chunk).
  - Cờ proxy của class (cho phép xóa, biến đổi, đổi thuộc tính…) đưa ra cho lệnh.
  - Fuzz target `core/dwg/fuzz/fuzz_dwg_proxy.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Tệp có proxy trong bộ 10.000 tệp (≥ 200 tệp **[đề xuất]**): 100% proxy khớp vừa khít; số proxy và class id khớp DXF của ODA File Converter.
  2. Giải mã → mã hóa: khối dữ liệu và proxy graphics giống từng bit, handle không đổi.
  3. Chunk proxy graphics khai báo sai kích thước bị chặn, không đọc tràn; entity vẫn mở được, chỉ thiếu hình.
  4. Đạt DoD-C.

### DWG-168 · Schema đối tượng 3D; đọc khối ACIS và section AcDsPrototype_1b
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DWG-011, DWG-005
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1, phần 3DSOLID/REGION/BODY, dữ liệu ACIS, section `AcDb:AcDsPrototype_1b` (R2013+); tệp 3D trong bộ 10.000 tệp; nhu cầu hiển thị khung bao của RGN-023.
- **Đầu ra:**
  - `core/schema/solid/*.schema`: 3DSOLID, REGION, BODY, PLANESURFACE, EXTRUDEDSURFACE, LOFTEDSURFACE, REVOLVEDSURFACE, SWEPTSURFACE, NURBSURFACE, MESH, HELIX (11 loại); metadata; ca vàng.
  - `core/dwg/acis/`: đọc khối SAT (ký tự mã hóa trong đối tượng ở bản cũ) và SAB, giữ thô trong bộ nhớ, không dựng mô hình ACIS; đọc section ACDS (ánh xạ handle → bản ghi, giữ segment chưa hiểu); quét token lấy hộp bao.
  - Fuzz target `fuzz_acis_scan`, `fuzz_acds`.
- **Tiêu chí nghiệm thu:**
  1. Tệp 3D của bộ 10.000 tệp: 100% đối tượng 11 loại khớp vừa khít; 100% solid, region, body, surface tìm được khối ACIS (trong đối tượng hoặc section ACDS) ở tệp không hỏng.
  2. Khối ACIS đọc ra giống từng byte dữ liệu gốc (sau giải mã ký tự với SAT); mọi bản ghi ACDS có handle chủ tồn tại.
  3. Hộp bao chứa mọi đỉnh và tâm ± bán kính của mặt cầu, trụ, nón, xuyến; khối không tính được trả rỗng, không lỗi; ≤ 1 ms cho khối 1 MB **[đề xuất]**.
  4. Đọc khối ACIS làm tăng thời gian mở BENCH-STD ≤ 3% **[đề xuất]**.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** mã hóa lại, chuyển SAT ↔ SAB, ghi section ACDS (DWG-017).

### DWG-017 · 3DSOLID, REGION, BODY: giữ nguyên khối ACIS khi lưu
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-168
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1 (dữ liệu ACIS, section `AcDb:AcDsPrototype_1b`); bộ đọc DWG-168; tệp 3D trong bộ 10.000 tệp.
- **Đầu ra:** `core/dwg/acis/`:
  - mã hóa lại khối ACIS vào đối tượng theo phiên bản đích (mã hóa ký tự SAT ở bản cũ, SAB ở bản mới);
  - dựng lại section ACDS: giữ nguyên segment chưa hiểu, bỏ bản ghi của solid đã xóa, thêm bản ghi cho solid sao chép;
  - chuyển SAT ↔ SAB dạng token, cùng phiên bản ACIS; lỗi có kiểu `AcisVersionTooNew` khi phiên bản ACIS vượt mức phiên bản DWG đích chấp nhận (cho DWG-162).
- **Tiêu chí nghiệm thu:**
  1. Tệp 3D của bộ 10.000 tệp, không sửa: khối ACIS trong đối tượng và section ACDS mã hóa lại giống từng byte.
  2. Xóa và sao chép solid: section ACDS chỉ đổi đúng bản ghi liên quan; DWG-168 đọc lại không lỗi, mọi bản ghi có chủ.
  3. SAT → SAB → SAT trên mọi khối của bộ: dãy token khớp 100%.
  4. Đạt DoD-C.
- **Ngoài phạm vi:** sửa hình học ACIS, kể cả MOVE/ROTATE solid (đối tượng 3D chỉ đọc đến P3) **[đề xuất]**; nâng/hạ phiên bản ACIS.

### DWG-169 · REGION, 3DSOLID, BODY: biến đổi đồng dạng qua bản ghi transform cấp body của SAT/SAB
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-168, DWG-017, GEO-022
- **Đầu vào:** 03 §6 (MOVE, ROTATE, SCALE, MIRROR là P0, áp cho mọi đối tượng); tài liệu định dạng tệp lưu SAT/SAB công khai của ACIS (chỉ cấu trúc bản ghi `body` và `transform`); API đọc khối ACIS của DWG-168; ma trận của GEO-022.
- **Đầu ra:** `core/dwg/acis/body_transform.{h,cpp}`: đọc/ghi bản ghi `body` và `transform` trong SAT (văn bản) và SAB (nhị phân, kể cả section AcDsPrototype_1b từ R2013); hợp ma trận mới với transform sẵn có hoặc tạo bản ghi mới; giữ nguyên mọi bản ghi khác; cập nhật hộp bao lưu kèm entity. Kiểm thử `core/dwg/tests/acis/`.
- **Tiêu chí nghiệm thu:**
  1. Dời, xoay, scale đều, đối xứng trên 300 khối ACIS từ QA-001: DWG TrueView và ODA File Converter (QA-004) mở lại không lỗi; hộp bao khớp biến đổi kỳ vọng, sai số tương đối ≤ 1e-9.
  2. Chỉ bản ghi `body`/`transform` thay đổi; mọi bản ghi khác trùng từng byte (so diff SAT/SAB).
  3. Scale không đều hoặc shear: trả `NonUniformNotSupported`, khối giữ nguyên.
  4. 10.000 khối ≤ 200 ms trên 8 nhân tầm trung **[đề xuất]**, song song theo khối, kết quả không đổi theo số luồng.
  5. Đạt DoD-C, DoD-H.

## 12. Ảnh xem trước và thông tin tệp

### DWG-026 · Ảnh xem trước (thumbnail) trong DWG: đọc và ghi
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-004, DWG-005, NT-011, QA-004
- **Màn hình:** UI-B01, UI-A09
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1 (ảnh xem trước R13–R2000, section `AcDb:Preview`); 07.1 UI-B01 (khung xem trước), UI-A09 (thumbnail trên tab); bộ 10.000 tệp.
- **Đầu ra:** `core/dwg/preview/`:
  - `readPreview(path)`: chỉ đọc header tệp và vùng ảnh, không giải mã đối tượng; trả loại (BMP, WMF, PNG), kích thước, byte ảnh đã kiểm header; kèm phiên bản DWG cho UI-B01;
  - `encodePreview(image, version)` cho bộ ghi, định dạng theo phiên bản đích;
  - ảnh xem trước của block trong BLOCK_HEADER; fuzz target `fuzz_dwg_preview`.
- **Tiêu chí nghiệm thu:**
  1. Bộ 10.000 tệp: 100% tệp có ảnh trả đúng loại và kích thước; byte ảnh khớp ảnh do LibreDWG `dwgbmp` trích (QA-004) ở mọi tệp công cụ đó đọc được.
  2. `readPreview` ≤ 5 ms p99 với tệp 100 MB trên máy tầm trung **[đề xuất]**.
  3. `encodePreview` → `readPreview` giống từng byte; ảnh ra hợp lệ theo bộ kiểm BMP/PNG độc lập.
  4. Ảnh hỏng (kích thước khai báo vượt tệp, header sai) bị bỏ qua có cảnh báo, không chặn việc mở tệp.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** render ảnh mới khi lưu (RND-021); hiển thị (UI).

### DWG-027 · Section SummaryInfo, AppInfo, FileDepList, RevHistory: đọc và ghi
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** DWG-005, DWG-028, DWG-013, QA-004
- **Màn hình:** UI-B08, UI-B06
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1 (section `AcDb:SummaryInfo`, `AcDb:AppInfo`, `AcDb:AppInfoHistory`, `AcDb:FileDepList`, `AcDb:RevHistory`); 07.2 UI-B08 (bốn tab); 07.1 UI-B06 (nhóm tham chiếu); 03 §2 (cảnh báo TrustedDWG).
- **Đầu ra:** `core/dwg/fileinfo/`:
  - `DrawingInfo`: tiêu đề, chủ đề, tác giả, từ khóa, ghi chú, gốc hyperlink, người lưu cuối, số lần sửa, ngày tạo/sửa, tổng thời gian sửa, thuộc tính tùy biến; ở R13–R2000 đọc/ghi dạng XRECORD `DWGPROPS` trong named object dictionary;
  - `FileDependency`: loại (Font, Xref, Ảnh, PDF underlay, Plot style, Shape), đường dẫn đã lưu; dựng lại từ bản vẽ khi lưu;
  - đọc/ghi theo phiên bản; fuzz target `fuzz_dwg_fileinfo`.
- **Tiêu chí nghiệm thu:**
  1. Bộ 10.000 tệp: 100% section đọc không lỗi; khớp kết quả LibreDWG `dwgread` (QA-004) ở ≥ 99,9% tệp, chênh lệch có giải thích.
  2. Thuộc tính tiếng Việt round-trip đúng: UTF-16 ở AC1021+, DWGCODEPAGE ở bản cũ (ký tự ngoài code page ghi `\U+`).
  3. FileDepList ghi từ bản vẽ (font của STYLE, xref, IMAGEDEF, underlay, CTB/STB) đọc lại khớp 100%; AppInfo ghi đúng tên và phiên bản TrionCAD, không giả dấu TrustedDWG.
  4. Đọc toàn bộ thông tin ≤ 10 ms p99 với tệp 100 MB, không giải mã đối tượng **[đề xuất]**.
  5. Đạt DoD-C.

## 13. Bộ ghi DWG

### DWG-160 · Nén LZ77 biến thể R2004+ nhanh, song song theo trang
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-107, NT-005, NT-006
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1, phần nén R2004; bộ giải nén DWG-107; các trang section của bộ 1.000 tệp làm dữ liệu đo.
- **Đầu ra:** `core/dwg/compress/lz77_2004_encoder.{h,cpp}`: tìm khớp bằng bảng băm và chuỗi có giới hạn độ sâu, so khớp bằng SIMD (NT-006); nén độc lập từng trang (tối đa 0x7400 byte) song song qua NT-005; hai mức nhanh/cân bằng; benchmark `core/dwg/bench/lz77_encode_bench.cpp`; kiểm thử property.
- **Tiêu chí nghiệm thu:**
  1. Giải nén(nén(x)) = x trên 1 triệu khối ngẫu nhiên và suy biến (rỗng, 1 byte, lặp dài, không nén được) và trên mọi trang của bộ 1.000 tệp.
  2. Mức cân bằng: tổng kích thước nén ≤ 110% tổng kích thước trang nén trong tệp gốc của bộ 1.000 tệp **[đề xuất]**.
  3. Mức nhanh ≥ 150 MB/s mỗi luồng trên máy tầm trung **[đề xuất]**; không cấp phát trong vòng lặp nén.
  4. Đạt DoD-C, DoD-H.

### DWG-161 · Ghi luồng đối tượng, object map, header và class theo phiên bản
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DWG-002, DWG-110, DWG-008, DWG-009, DWG-010, NT-005
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1 (section Handles, Classes, Header, AuxHeader, AcDbObjects); bộ ghi sinh mã (DWG-002); khung đối tượng (DWG-110); bộ mã hóa header (DWG-008) và class (DWG-009).
- **Đầu ra:** `core/dwg/writer/`:
  - mã hóa đối tượng song song (NT-005), ghép theo thứ tự handle tất định, tính offset; đủ mọi phiên bản (AC1021 dùng ở DWG-163);
  - object map: cặp handle/offset mã hóa delta, khối ≤ 2032 byte có CRC;
  - header cập nhật khi lưu (TDUPDATE, TDUUPDATE, TDINDWG, HANDSEED, VERSIONGUID), AuxHeader; section class giữ số class cũ, thêm class cho loại mới;
  - đồng hồ tiêm vào được để kiểm thử tất định.
- **Tiêu chí nghiệm thu:**
  1. Bộ 1.000 tệp: ghi lại luồng đối tượng, object map, header, class ở phiên bản gốc rồi đọc bằng DWG-010: 100% trường khớp (so theo metadata schema).
  2. Tập handle vào = ra; đối tượng mới nhận handle từ HANDSEED; HANDSEED sau lưu > handle lớn nhất; số class của đối tượng cũ không đổi.
  3. Object map: mọi handle trỏ đúng offset, mọi khối có CRC hợp lệ.
  4. Mã hóa 1 triệu đối tượng (BENCH-STD 100 MB) ≤ 1 s trên máy tầm trung **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

### DWG-018 · Giữ nguyên đối tượng, trường và section chưa hiểu; không đánh lại handle
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DWG-161, DWG-016, DOC-006
- **Đầu vào:** 05 §6.2 mục 2; 03 §2 (proxy, dữ liệu Civil 3D và plugin); Đặc tả DWG của ODA v5.4.1 (khung đối tượng, class); mô hình khối thô DOC-006; tệp Civil 3D, Architecture, plugin trong bộ 10.000 tệp.
- **Đầu ra:** `core/dwg/raw/`:
  - đối tượng của class chưa có schema: giữ khối bit dữ liệu, luồng chuỗi, luồng handle; trích đủ handle tham chiếu (owner, reactor, xdictionary, con trỏ) cho phân tích tham chiếu;
  - đuôi bit chưa hiểu của đối tượng đã có schema (bản bảo trì mới hơn) và section chưa hiểu: giữ nguyên;
  - nhóm AcDb* chỉ giữ thô đến v1.0: point cloud, render, background, Navisworks, ràng buộc tham số 2D, AEC*/AECC*; báo cáo loại chưa hiểu theo tệp.
- **Tiêu chí nghiệm thu:**
  1. Tệp có đối tượng chưa hiểu (≥ 200 tệp **[đề xuất]**): mở → mã hóa lại cùng phiên bản → mọi đối tượng chưa hiểu giống từng bit, handle giữ nguyên 100%.
  2. Sửa, xóa đối tượng đã hiểu rồi mã hóa: đối tượng chưa hiểu vẫn giống từng bit; 100% handle trong luồng handle của chúng có trong danh sách tham chiếu.
  3. Không đánh lại handle: bất biến kiểm trên cả bộ 10.000 tệp.
  4. Đích khác nhóm mã hóa đối tượng: bọc thành proxy giữ bit gốc khi đặc tả cho phép, nếu không thì bỏ **[đề xuất]**; mỗi trường hợp trả một mục mất dữ liệu có kiểu cho DWG-162.
  5. Đạt DoD-C.

### DWG-162 · Quy tắc hạ phiên bản và danh sách tính năng bị giảm
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DWG-018, DWG-017, DWG-014, DWG-015, DWG-158, RGN-003, RGN-020
- **Màn hình:** UI-B02
- **Đầu vào:** 07.1 UI-B02 (cảnh báo, liệt kê ảnh hưởng, gợi ý tương thích); điều kiện phiên bản trong schema; primitive regen (RGN-003); parser proxy graphics (RGN-020).
- **Đầu ra:**
  - `core/dwg/downgrade/`: `planDowngrade(snapshot, version)` → nhóm tính năng bị giảm kèm số đối tượng (annotative, MLEADER, bảng mô hình mới, transparency, underlay, dynamic block kiểu mới, mảng liên kết, định vị địa lý, ACIS quá mới, dữ liệu chưa hiểu, biến header) và chuỗi gợi ý tương thích theo phiên bản cho UI-B02;
  - áp quy tắc trên khung nhìn chỉ đọc khi ghi: bỏ trường mới; loại mới ghi thành proxy kèm proxy graphics sinh từ primitive regen **[đề xuất]**; bộ ghi `core/dwg/proxy/proxy_graphics_writer.*`;
  - `docs/specs/dwg/ha-phien-ban.md`: bảng quy tắc theo phiên bản đích.
- **Tiêu chí nghiệm thu:**
  1. Mọi loại và trường có điều kiện phiên bản trong schema đều có quy tắc (lint codegen, 100%).
  2. Bộ 1.000 tệp AC1032, mã hóa đối tượng ở AC1018 (DWG-161) rồi giải mã: khớp 100% trường trừ mục trong danh sách; danh sách khớp 100% với diff thực tế.
  3. Proxy graphics ghi ra, đọc lại bằng RGN-020 cho cùng primitive (sai số tọa độ tương đối ≤ 1e-9 **[đề xuất]**).
  4. `planDowngrade` ≤ 200 ms cho BENCH-STD 100 MB, chạy nền; bản vẽ trong bộ nhớ không đổi (hash snapshot trước = sau).
  5. Đạt DoD-C, DoD-H.

### DWG-021 · Bộ ghi AC1018 (R2004): dựng section, page map, checksum; nghiệm thu tệp ghi
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DWG-160, DWG-161, DWG-162, DWG-026, DWG-027, NT-011, NT-014, QA-004
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1, phần bố cục R2004 (header tệp mã hóa, section page map, section map, trang dữ liệu, checksum); 05 §6.3 (kiểm tra tệp ta ghi); bộ nén DWG-160; luồng đối tượng và object map DWG-161.
- **Đầu ra:** `core/dwg/writer/r2004/`:
  - dựng section `AcDb:Header`, `AcDb:AuxHeader`, `AcDb:Classes`, `AcDb:Handles`, `AcDb:Template`, `AcDb:ObjFreeSpace`, `AcDb:AcDbObjects`, `AcDb:RevHistory`, `AcDb:SummaryInfo`, `AcDb:Preview`, `AcDb:AppInfo`, `AcDb:FileDepList` và section giữ nguyên (DWG-018);
  - chia trang, nén song song, header trang mã hóa, checksum trang, page map, section map, header tệp mã hóa kèm CRC32 (NT-014);
  - `dwg::save(snapshot, path, version, options)`: ghi an toàn qua tệp tạm (NT-011), tiến trình, hủy, áp quy tắc DWG-162; ảnh xem trước do người gọi cấp.
- **Tiêu chí nghiệm thu:**
  1. Bộ 10.000 tệp (trừ tệp nguồn hỏng theo QA-001): lưu AC1018 → 100% mở trong DWG TrueView không hỏi recover; ODA File Converter và LibreDWG không báo lỗi (QA-004).
  2. Bộ 10.000 tệp: đọc → ghi AC1018 → đọc khớp 100% trường (so theo metadata schema), trừ mục trong danh sách DWG-162; QA-003 dùng lại phép so này cho job đêm.
  3. Lưu BENCH-STD 20 MB ≤ 1,5 s, 100 MB ≤ 5 s trên máy tầm trung **[đề xuất]**; bộ nhớ thêm ≤ 25% bộ nhớ của bản vẽ **[đề xuất]**.
  4. Lỗi giữa chừng (đĩa đầy, hủy, mất quyền ghi) để nguyên tệp đích cũ và báo rõ nguyên nhân.
  5. Đạt DoD-C, DoD-H.

### DWG-022 · Bộ ghi AC1032 (R2018), định dạng lưu mặc định
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-021, DWG-017, DWG-007
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1, phần R2010/R2013/R2018; khung R2004 của DWG-021; section ACDS (DWG-017); đặc thù R2010+ (DWG-007).
- **Đầu ra:** `core/dwg/writer/r2018/`: đối tượng mã hóa kiểu AC1024+ qua DWG-161 (kiểu đối tượng dạng BOT, kích thước luồng handle); chuỗi UTF-16; section `AcDb:AcDsPrototype_1b`; AppInfo, SummaryInfo dạng mới (DWG-027); `Version::AC1032` là định dạng lưu mặc định.
- **Tiêu chí nghiệm thu:**
  1. Bộ 10.000 tệp: lưu AC1032 → 100% mở trong DWG TrueView không hỏi recover; ODA File Converter, LibreDWG không lỗi (QA-004).
  2. Đọc → ghi AC1032 → đọc khớp 100% trường (so theo metadata schema); tệp AC1032 gốc mở rồi lưu không sửa: đối tượng chưa hiểu và khối ACIS giống từng bit.
  3. Lưu BENCH-STD 20 MB ≤ 1,5 s, 100 MB ≤ 5 s trên máy tầm trung **[đề xuất]**.
  4. Kích thước tệp ≤ 115% tệp gốc do AutoCAD lưu, trung vị trên bộ 1.000 tệp **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

### DWG-163 · Mã hóa Reed–Solomon, nén R2007 và bộ ghi AC1021
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** DWG-006, DWG-021, NT-006, QA-003
- **Đầu vào:** Đặc tả DWG của ODA v5.4.1, phần R2007 (header tệp, trang hệ thống và dữ liệu, Reed–Solomon, nén, checksum); bộ đọc R2007 (DWG-006); `dwg::save` của DWG-021.
- **Đầu ra:** `core/dwg/writer/r2007/`: bộ mã hóa Reed–Solomon (bảng GF(256), SIMD qua NT-006) có xen kẽ khối theo đặc tả; nén LZ77 biến thể R2007 song song theo trang; page map, section map, header tệp kèm các checksum riêng của R2007; `Version::AC1021`.
- **Tiêu chí nghiệm thu:**
  1. Mã hóa RS rồi giải mã bằng DWG-006: khôi phục đúng khi chèn lỗi tới giới hạn sửa được, trên 1 triệu khối ngẫu nhiên.
  2. Giải nén(nén(x)) = x trên 1 triệu khối và mọi trang của tệp AC1021 trong bộ 1.000 tệp.
  3. Bộ 10.000 tệp: lưu AC1021 → 100% mở trong DWG TrueView không hỏi recover; ODA File Converter, LibreDWG không lỗi; QA-003 khớp 100% trừ mục hạ phiên bản.
  4. Lưu BENCH-STD 100 MB ≤ 6 s trên máy tầm trung **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

### DWG-023 · Bộ ghi AC1015, AC1024, AC1027; nghiệm thu bốn phiên bản P1
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** DWG-163, DWG-022, DWG-004, QA-003
- **Đầu vào:** 03 §2 (lưu DWG 2000/2007/2010/2013, P1); 07.1 UI-B02 (danh sách định dạng); Đặc tả DWG của ODA v5.4.1, phần bố cục R13–R2000 (locator, sentinel, CRC) và khác biệt R2010, R2013; bố cục đọc DWG-004.
- **Đầu ra:** `core/dwg/writer/r2000/`: header tệp có locator, sentinel, CRC-16, header thứ hai, ảnh xem trước ở vị trí cố định, DWGPROPS dạng XRECORD (DWG-027); bật `Version::AC1015`, `AC1024`, `AC1027`; ma trận 4 phiên bản × bộ 10.000 tệp trong job đêm.
- **Tiêu chí nghiệm thu:**
  1. Lưu AC1015, AC1021, AC1024, AC1027 từ bộ 10.000 tệp: 100% mở trong DWG TrueView không hỏi recover; ODA File Converter, LibreDWG không lỗi (QA-004).
  2. QA-003 khớp 100% trường trừ mục hạ phiên bản; danh sách DWG-162 của từng phiên bản đích khớp diff thực tế.
  3. Lưu nối tiếp AC1032 → AC1027 → AC1024 → AC1021 → AC1018 → AC1015 không mất thêm gì ngoài danh sách của phiên bản thấp nhất.
  4. Lưu BENCH-STD 100 MB ở mỗi phiên bản ≤ 6 s trên máy tầm trung **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

## 14. DXF

### DWG-164 · Bộ tách token và bộ ghi DXF ASCII tốc độ cao
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-104, DWG-028, NT-005, NT-006, NT-011
- **Đầu vào:** DXF Reference của Autodesk (cặp mã nhóm/giá trị, kiểu giá trị theo dải mã); `DxfGroupCursor`, `DxfGroupSink` (DWG-104); mã hóa chuỗi DWG-028.
- **Đầu ra:** `core/dxf/ascii/`:
  - đọc qua mmap (NT-011); đánh chỉ số dòng song song bằng SIMD (tìm `\n`, tổng tiền tố), tách cặp từ dòng chẵn, cấp qua `DxfGroupCursor`;
  - chấp nhận CRLF/LF, BOM, khoảng trắng đầu/cuối dòng; chuỗi theo DWGCODEPAGE kèm `\U+xxxx` (trước AC1021) và UTF-8 (AC1021+);
  - ghi từ `DxfGroupSink` vào bộ đệm lớn; fuzz target `fuzz_dxf_ascii`; benchmark.
- **Tiêu chí nghiệm thu:**
  1. Tách token ≥ 400 MB/s mỗi luồng trên máy tầm trung **[đề xuất]**; tệp 100 MB tách xong ≤ 150 ms với 8 luồng **[đề xuất]**.
  2. Số thực đọc đúng từng bit so với `std::from_chars` trên 10 triệu chuỗi, kể cả dạng `1.E+10`, `-.5`, số mũ ba chữ số.
  3. Lỗi cú pháp (mã không phải số, thiếu dòng giá trị, giá trị sai kiểu theo dải mã) báo đúng số dòng, không đọc tràn.
  4. Ghi ≥ 300 MB/s mỗi luồng **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

### DWG-024 · DXF ASCII đọc/ghi mọi phiên bản (sinh từ schema)
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DWG-164, DWG-002, DWG-013, DWG-014, DWG-015, DWG-016, DWG-168, DWG-026, QA-004
- **Đầu vào:** 03 §2 (DXF, P0, CNC và GIS); DXF Reference (cấu trúc section, khác biệt theo phiên bản); đọc/ghi DXF theo đối tượng do codegen sinh (DWG-002); DXF do ODA File Converter xuất (QA-004).
- **Đầu ra:** `core/dxf/`:
  - section HEADER, CLASSES, TABLES, BLOCKS, ENTITIES, OBJECTS, THUMBNAILIMAGE, ACDSDATA; phân giải handle và con trỏ 330–369, nhóm `{ACAD_REACTORS`, `{ACAD_XDICTIONARY`, xdata; gán handle tất định khi tệp thiếu handle;
  - đối tượng chưa có schema giữ nguyên dãy nhóm để ghi lại DXF; khi lưu DWG thì báo là dữ liệu không ghi được **[đề xuất]**;
  - đọc song song theo khối entity/object; `dxf::load`, `dxf::save(snapshot, path, version)` cho AC1012–AC1032; fuzz target `fuzz_dxf`.
- **Tiêu chí nghiệm thu:**
  1. DXF do ODA File Converter xuất từ bộ 1.000 tệp (AC1012–AC1032): 100% đọc được; khớp 100% trường với kết quả đọc DWG gốc (so theo metadata schema).
  2. DXF ta ghi ở mọi phiên bản AC1012–AC1032: ODA File Converter đọc không lỗi 100%; DWG TrueView mở không lỗi.
  3. DWG → DXF → mô hình khớp 100% trường; handle giữ nguyên.
  4. Đọc DXF 100 MB ≤ 3 s, ghi ≤ 2 s trên máy tầm trung **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

### DWG-025 · DXF nhị phân đọc/ghi
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-024, QA-004
- **Đầu vào:** DXF Reference (DXF nhị phân: sentinel, mã nhóm, kiểu giá trị); `DxfGroupCursor`, `DxfGroupSink` (DWG-104) và lớp section DWG-024; DXF nhị phân mẫu do công cụ đối chứng QA-004 tạo.
- **Đầu ra:** `core/dxf/binary/`: nhận sentinel `AutoCAD Binary DXF`; mã nhóm 2 byte; giá trị theo dải mã (chuỗi kết thúc 0, số thực 8 byte, số nguyên 2/4/8 byte, bool, khối nhị phân có độ dài); dùng chung lớp section với ASCII; `dxf::save(..., binary)`; fuzz target `fuzz_dxf_binary`.
- **Tiêu chí nghiệm thu:**
  1. Cùng bản vẽ đọc từ DXF ASCII và DXF nhị phân khớp 100% trường trên bộ 1.000 tệp.
  2. DXF nhị phân ta ghi: ODA File Converter đọc không lỗi 100%; ghi → đọc giữ số thực đúng từng bit.
  3. Đọc nhanh hơn DXF ASCII cùng nội dung ≥ 2 lần **[đề xuất]**.
  4. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** DXF nhị phân R12 mã nhóm 1 byte (DWG-165).

## 15. Định dạng cũ (sau v1.0)

### DWG-165 · Lưu định dạng cũ: DXF R12 đọc/ghi và DWG R14 (AC1014)
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** DWG-024, DWG-023, DWG-162, GEO-016, RGN-009, RGN-012
- **Màn hình:** UI-B02
- **Đầu vào:** 03 §2 (đọc DWG R12, lưu R12/R14: P2, làm DXF R12 trước); 07.1 UI-B02 (định dạng R14/R12); DXF Reference (AC1009); bố cục R13–R2000 của DWG-023.
- **Đầu ra:**
  - `core/dxf/r12/`: đọc/ghi DXF AC1009, ASCII và nhị phân, có hoặc không handle; khi ghi hạ cấp LWPOLYLINE → POLYLINE, ELLIPSE/SPLINE → POLYLINE xấp xỉ theo dung sai (GEO-016), MTEXT → TEXT theo dàn trang (RGN-009), HATCH → block vô danh chứa nét (RGN-012); mất mát vào danh sách DWG-162;
  - `core/dwg/writer/r14/`: `Version::AC1014` trên khung R13–R2000.
- **Tiêu chí nghiệm thu:**
  1. DXF R12 ta ghi từ bộ 1.000 tệp: ODA File Converter đọc không lỗi 100%; hình học lệch ≤ dung sai xấp xỉ (mặc định 0,01 đơn vị bản vẽ **[đề xuất]**).
  2. Tệp AC1014 ta ghi: DWG TrueView mở không hỏi recover; ODA File Converter, LibreDWG không lỗi, 100% tệp.
  3. DXF R12 do ODA File Converter xuất: đọc được 100%, số entity theo loại khớp.
  4. Đạt DoD-C.

### DWG-029 · Đọc DWG R12 trở về trước và DXF R12
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** DWG-165, DWG-004
- **Đầu vào:** 03 §2; tệp R10–R12 trong kho (QA-001); DXF do ODA File Converter xuất từ cùng tệp; tài liệu công khai về DWG R12 nếu có, còn lại thực nghiệm (không đọc mã LibreDWG).
- **Đầu ra:** `core/dwg/r12/`: nhận diện AC1006, AC1009 (bản cũ hơn báo rõ "không hỗ trợ" **[đề xuất]**); đọc header, bảng (BLOCK, LAYER, STYLE, LTYPE, VIEW, UCS, VPORT, APPID, DIMSTYLE), section entity, block và entity phụ; gán handle tất định khi tệp không có handle; dùng chung ánh xạ R12 → mô hình hiện đại với DXF R12 (DWG-165); fuzz target `fuzz_dwg_r12`.
- **Tiêu chí nghiệm thu:**
  1. Tệp R10–R12 trong kho: 100% mở được; số entity theo loại và giá trị trường khớp DXF do ODA File Converter xuất ở ≥ 99,9% entity.
  2. Cùng bản vẽ ở dạng DWG R12 và DXF R12 cho mô hình khớp 100% trường.
  3. Mở R12 rồi lưu AC1032: DWG TrueView mở không hỏi recover (QA-004).
  4. Đọc tệp R12 10 MB ≤ 1 s trên máy tầm trung **[đề xuất]**.
  5. Đạt DoD-C.

## Tóm tắt theo mốc

| Mốc | Task | Số task |
|---|---|---|
| S1 | DWG-101, DWG-105, DWG-003, DWG-028, DWG-001, DWG-102, DWG-103, DWG-104, DWG-002, DWG-004, DWG-107, DWG-106, DWG-005, DWG-008, DWG-009, DWG-111, DWG-110, DWG-007, DWG-010, DWG-114, DWG-030, DWG-113, DWG-151, DWG-011, DWG-012 | 25 |
| V | DWG-108, DWG-109, DWG-006, DWG-019, DWG-112, DWG-020, DWG-031, DWG-115, DWG-166, DWG-153, DWG-013, DWG-155, DWG-154, DWG-167, DWG-014, DWG-158, DWG-156, DWG-015, DWG-157, DWG-016, DWG-168, DWG-026, DWG-027, DWG-164, DWG-024 | 25 |
| VP | DWG-017, DWG-160, DWG-161, DWG-018, DWG-162, DWG-021, DWG-022 | 7 |
| EA | DWG-169, DWG-025 | 2 |
| 1.0 | DWG-163, DWG-023 | 2 |
| Sau | DWG-165, DWG-029 | 2 |
| **Tổng** | | **63** |
