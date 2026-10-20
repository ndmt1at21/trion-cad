# QA · Kiểm thử, dữ liệu chuẩn và chỉ số chất lượng

> Phạm vi: kho bản vẽ có thỏa thuận, bộ chạy kho, đối chứng chéo, round-trip, fuzzing, property-based, bộ tham chiếu, bộ LISP, so sánh ảnh in, cảnh và bộ benchmark, ma trận phần cứng, kiểm thử UI tự động, chỉ số crash-free, bảng chỉ số, báo cáo cổng G0–G3.
> Thư mục mã: `qa/`. Căn cứ: [05](../05-kien-truc-ky-thuat.md) §1, §6.3, §14, §15, §21; [06](../06-lo-trinh-trien-khai.md) §2–§5; [README](README.md) §3 (DoD, máy tham chiếu, dữ liệu chuẩn).

## 0. Ghi chú thiết kế

- QA giao harness và bộ dữ liệu. Kiểm thử đơn vị, fuzz target, thuộc tính property-based và benchmark của từng module do task của module đó viết, chạy trên hạ tầng QA.
- Pháp lý (05 §15): LibreDWG chỉ chạy CLI dạng nhị phân trong container riêng; không tải, không đọc, không chép mã nguồn, không link. DWG TrueView và ODA File Converter chỉ dùng để đối chứng mở, in, chuyển đổi, theo EULA đã được luật sư rà (06 §2.1).
- Mọi tệp bản vẽ và LISP có bản ghi thỏa thuận (nguồn, phạm vi dùng, thời hạn) trong QA-101. Tệp không rời hạ tầng nội bộ. Báo cáo ra ngoài chỉ chứa số tổng hợp.
- Điều phối viết bằng Python ≥ 3.11, chỉ dùng thư viện chuẩn; phép tính nặng (so ảnh, so hình học) viết C++ trong `qa/` trên NT-005, NT-006 **[đề xuất]**. Mã Python áp DoD-C mục 2, 4, 7, 10, 11; mã C++ áp đủ DoD-C.
- Mọi bộ chạy xuất JSON theo schema có phiên bản trong `qa/schemas/`. Bảng chỉ số và báo cáo cổng chỉ đọc JSON này **[đề xuất]**.
- Tất định: cùng đầu vào cho cùng JSON (bỏ trường thời gian); thứ tự theo SHA-256 của tệp; seed ghi trong kết quả.
- Nhịp chạy (05 §14): mỗi PR (bộ rút gọn ≤ 10 phút mỗi runner **[đề xuất]**), mỗi đêm (đầy đủ), liên tục (fuzz), mỗi quý (đáp án bộ tham chiếu từ pilot).
- Bộ con dùng chung: `spike-1000`, `bench-std`, `pr-smoke-200` (QA-001); `v-10000` (QA-102); `tcreport`, `regression` (QA-103).
- Bộ tham chiếu so hình học với sai số ≤ 1e-9 × kích thước bản vẽ và tô-pô trùng (05 §1.2). Trước khi có đáp án, nghiệm thu bằng bất biến và ca vàng tự soạn (README §3.1).

## 1. Kho bản vẽ và bộ chạy kho

### QA-101 · Lưu trữ kho bản vẽ, metadata và hồ sơ thỏa thuận
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-002
- **Đầu vào:** 05 §14 (kho có thỏa thuận sử dụng), 05 §15; mẫu thỏa thuận do luật sư soạn (06 §2.1).
- **Đầu ra:**
  - Kho định địa chỉ theo SHA-256 (blob + `meta/<sha>.json`) trên máy chủ nội bộ có sao lưu; schema `qa/schemas/corpus-meta.schema.json` (phiên bản AC10xx, kích thước, nguồn, mã thỏa thuận, phạm vi, hạn, nhãn bộ môn, font, bảng mã).
  - CLI `qa/corpus/corpus.py`: `add`, `verify`, `list --subset`, `revoke`.
- **Tiêu chí nghiệm thu:**
  1. `add` từ chối 100% tệp thiếu mã thỏa thuận hoặc trùng SHA-256 (bộ ca 50 tệp mẫu).
  2. Nhận diện phiên bản từ chữ ký đầu tệp đúng 100% cho AC1012–AC1032 và DXF; tệp lạ gắn `unknown`, không lỗi.
  3. `revoke <mã thỏa thuận>` gỡ mọi tệp liên quan khỏi mọi bộ con, có nhật ký; `verify` phát hiện 100% blob bị đổi 1 byte.
  4. `verify` 10.000 tệp ≤ 5 phút trên máy tầm trung, song song theo tiến trình **[đề xuất]**.
  5. Kho chỉ đọc được từ runner tự host nội bộ; job CI kiểm quyền truy cập.
  6. Đạt DoD-C.

### QA-001 · Kho bản vẽ: bộ 1.000 tệp spike, BENCH-STD và bộ con PR
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** QA-101
- **Đầu vào:** README §3.1 (BENCH-STD); 05 §21 S1 (R2000, R2004, R2018); tệp từ pilot và đối tác có thỏa thuận (06 §2.1, tuần 1–4).
- **Đầu ra:**
  - Công cụ chọn mẫu phân tầng `qa/corpus/select.py`: hạn ngạch theo phiên bản, kích thước, nguồn, bộ môn; khử gần trùng.
  - Bộ con `qa/corpus/subsets/spike-1000.txt`, `bench-std.txt`, `pr-smoke-200.txt`; hướng dẫn `qa/corpus/README.md`.
- **Tiêu chí nghiệm thu:**
  1. `spike-1000` gồm 1.000 tệp duy nhất, 100% có thỏa thuận; R2000, R2004, R2018 mỗi loại ≥ 25% **[đề xuất]**, phần còn lại trải R13/R14, R2007, R2010, R2013.
  2. ≥ 60% tệp nguồn Việt Nam, ≥ 15% có text TCVN3/VNI, ≥ 10% có xref hoặc ảnh **[đề xuất]**.
  3. `bench-std` có ≥ 3 tệp thật 18–22 MB và ≥ 2 tệp 90–110 MB, ghi số entity theo loại.
  4. `pr-smoke-200` phủ mọi phiên bản và mọi loại lỗi đã biết, tổng ≤ 300 MB **[đề xuất]**.
  5. Chọn mẫu tất định: cùng kho và seed cho cùng danh sách; CI so hash danh sách.
  6. Đạt DoD-C.
- **Ngoài phạm vi:** bộ 10.000 tệp (QA-102); cảnh tổng hợp 1 triệu và 10 triệu entity (QA-012).

### QA-002 · Bộ chạy kho bản vẽ song song
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** QA-001, DWG-030, NT-002
- **Đầu vào:** CLI `dwgdump` (DWG-030); bộ con QA-001; 05 §21 S1 (đọc ≥ 90% kho ở mức entity lõi).
- **Đầu ra:** `qa/runner/corpus_run.py` (mỗi tệp một tiến trình con, giới hạn thời gian và bộ nhớ); `qa/schemas/corpus-run.schema.json`; job đêm và job PR trên `pr-smoke-200`.
- **Tiêu chí nghiệm thu:**
  1. Phân loại mỗi tệp: đạt, đạt một phần (có đối tượng chưa hiểu), lỗi giải mã, crash, quá thời gian, hết bộ nhớ; kèm mã lỗi, section, handle.
  2. Tính tỉ lệ đọc ở mức entity lõi (05 §21 S1) theo phiên bản và nguồn; liệt kê tệp tụt hạng so với lượt trước.
  3. Tệp crash hoặc treo không dừng lượt chạy; timeout 60 s, trần bộ nhớ 4 GB mỗi tệp **[đề xuất]**.
  4. Ghi thời gian mở và bộ nhớ đỉnh từng tệp; `spike-1000` chạy ≤ 15 phút trên máy mạnh 16 luồng **[đề xuất]**.
  5. Hai lượt liên tiếp cho cùng hash JSON (bỏ trường thời gian).
  6. Đạt DoD-C.

### QA-102 · Bộ 10.000 tệp và chạy kho ở cấp tài liệu qua `trion-console`
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** QA-001, QA-002, INT-019
- **Đầu vào:** 05 §1.2 (mở thành công ≥ 99,5% kho ≥ 10.000 bản vẽ VN); 06 §3 (KPI ≥ 98% cuối Giai đoạn 1); CLI `trion-console` (INT-019).
- **Đầu ra:** bộ con `v-10000`; chế độ `--engine console` của `corpus_run.py` (mở, regen toàn bộ, đóng, đọc số đo do console xuất); định nghĩa "mở thành công" trong `qa/corpus/README.md`.
- **Tiêu chí nghiệm thu:**
  1. `v-10000` có ≥ 10.000 tệp duy nhất có thỏa thuận; ≥ 70% nguồn VN; đủ R13–R2018; ≥ 300 tệp > 10 MB **[đề xuất]**.
  2. "Mở thành công" = không crash, không hỏi recover (hoặc recover tự động không mất entity), mọi đối tượng giải mã hoặc giữ dạng thô, regen không lỗi **[đề xuất]**.
  3. Lượt đầy đủ ≤ 2 giờ trên máy mạnh; job đêm xuất tỉ lệ mở theo phiên bản, nguồn, loại lỗi.
  4. Thời gian mở, regen, bộ nhớ đỉnh do console báo lệch ≤ 5% so với đo từ bên ngoài tiến trình.
  5. Đạt DoD-C.

### QA-103 · Tiếp nhận "Gửi bản vẽ lỗi" vào kho
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLF-010, QA-101, QA-102
- **Đầu vào:** 05 §14 (nút "Gửi bản vẽ lỗi" để kho tự lớn lên); gói và cờ đồng ý tải lên qua PLF-010; 06 §3 (sửa lỗi hằng tuần theo tệp mới).
- **Đầu ra:** job `qa/intake/`: kéo gói, cách ly, kiểm cờ đồng ý, khử trùng, chạy QA-102, phân loại, tạo issue; bộ con `tcreport` và `regression`.
- **Tiêu chí nghiệm thu:**
  1. Gói không có cờ "cho phép dùng để kiểm thử" chỉ dùng để tái hiện lỗi, tự xóa sau 30 ngày **[đề xuất]**, không vào kho.
  2. Gói hỏng, zip bomb, đường dẫn `../` bị từ chối an toàn; giải gói có trần kích thước; fuzz bộ giải gói ≥ 1 giờ.
  3. Trùng SHA-256 gom về một issue và đếm số lần gửi; issue có phiên bản, loại lỗi, chữ ký crash.
  4. Từ lúc tải lên đến khi có issue ≤ 30 phút; xử lý ≥ 500 gói/giờ **[đề xuất]**.
  5. Tệp đã sửa xong chuyển sang `regression`; tệp < 5 MB vào job PR **[đề xuất]**.
  6. Đạt DoD-C.

## 2. Đối chứng chéo, round-trip, fuzzing

### QA-104 · Bộ điều khiển DWG TrueView trên runner Windows
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-002, QA-001
- **Đầu vào:** 05 §1.1 (đối chứng thời gian mở với DWG TrueView), 05 §6.3; DWG TrueView bản mới nhất với `DWG To PDF.pc3`.
- **Đầu ra:** `qa/oracles/trueview/` (Windows UI Automation): mở tệp, nhận diện hộp thoại recover, lỗi, thiếu font; đo thời gian mở; in PDF Model và mọi layout; JSON theo `qa/schemas/oracle.schema.json`.
- **Tiêu chí nghiệm thu:**
  1. Phân loại đúng 100% trên bộ 30 tệp có nhãn: mở sạch, hỏi recover, lỗi, treo.
  2. Thời gian mở tính đến khi bản vẽ hiện xong; lặp 5 lần, lấy trung vị; chênh giữa các lần ≤ 5% **[đề xuất]**.
  3. PDF đặt tên ổn định theo tên layout; tệp lỗi không làm hỏng lượt chạy (timeout 120 s, tự đóng hộp thoại lạ).
  4. Chạy phân mảnh trên nhiều runner; `spike-1000` ≤ 12 giờ với 1 runner.
  5. Đạt DoD-C.

### QA-004 · Harness đối chứng chéo ODA File Converter, LibreDWG CLI, DWG TrueView
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** QA-104, QA-002, DWG-030
- **Đầu vào:** 05 §6.3, 05 §15; ODA File Converter (xuất DXF); `dwgread` của LibreDWG (xuất JSON); JSON của `dwgdump`.
- **Đầu ra:**
  - `qa/oracles/oda/`, `qa/oracles/libredwg/`: chạy công cụ trong container riêng, chuẩn hóa về mô hình so sánh chung (handle, loại, layer, hình học, text).
  - `qa/crosscheck/diff.py`: so từng handle với `dwgdump`, có dung sai số thực; sổ chênh lệch đã giải thích `qa/crosscheck/known-diffs.toml`.
  - Chế độ `--written`: chạy 3 bộ đọc trên tệp TrionCAD ghi, trả kết quả mở sạch, lỗi, hỏi recover.
- **Tiêu chí nghiệm thu:**
  1. Mỗi chênh lệch là: khớp, đã giải thích (có lý do và người duyệt trong sổ) hoặc chưa giải thích; job đêm đỏ khi có chênh lệch chưa giải thích mới.
  2. Image CI chỉ chứa nhị phân LibreDWG; kiểm tự động: kho mã không có mã nguồn hay header LibreDWG, không binary nào của ta link tới nó.
  3. Bộ 20 tệp cài lỗi (dời tọa độ, xóa entity, đổi layer, hỏng chuỗi) bị phát hiện 100%.
  4. `spike-1000` đối chứng xong ≤ 2 giờ trên máy mạnh, song song theo tệp.
  5. Đạt DoD-C.

### QA-003 · Round-trip từng trường và cổng 3 bộ đọc cho tệp TrionCAD ghi
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** QA-004, QA-102, DWG-018, DWG-021, DWG-022
- **Đầu vào:** 05 §1.2 (round-trip 100% từng trường; 3 bộ đọc mở không lỗi), 05 §6.3; metadata schema sinh từ DWG-002, xuất qua `dwgdump`.
- **Đầu ra:** `qa/roundtrip/rt.py` (đọc → ghi AC1018 và AC1032 → đọc → so); báo cáo theo loại đối tượng và trường; job đêm trên `v-10000`, job PR trên `pr-smoke-200`.
- **Tiêu chí nghiệm thu:**
  1. So mọi trường theo metadata schema, kể cả khối bit thô và handle của đối tượng chưa hiểu; số thực so từng bit; handle không bị đánh lại.
  2. `v-10000`: 100% đối tượng không bị sửa trùng khớp từng trường; mỗi lệch ra issue có tệp, handle, trường.
  3. 100% tệp ghi ra mở được bằng DWG TrueView (không hỏi recover), ODA File Converter, LibreDWG, không lỗi (QA-004 `--written`).
  4. Bộ ca cài lỗi (bỏ trường, đổi 1 bit, đánh lại handle) bị phát hiện 100%.
  5. Lượt đầy đủ ≤ 6 giờ trên máy mạnh **[đề xuất]**.
  6. Đạt DoD-C.
- **Ngoài phạm vi:** bộ ghi AC1015, AC1021, AC1024, AC1027 thêm vào cấu hình khi DWG-023 xong.

### QA-006 · Fuzzing liên tục
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** NT-001, NT-002, QA-001, DWG-005
- **Đầu vào:** 05 §14 (fuzz codec, parser LISP/DCL/PGP/CUI/PAT/LIN/SHX, MTEXT); DoD-C mục 7; preset sanitizer NT-001.
- **Đầu ra:**
  - `qa/fuzz/`: sổ đăng ký target `targets.toml` (module tự khai báo); máy fuzz chạy 24/7 với libFuzzer + ASan/UBSan; đồng bộ và thu nhỏ corpus hằng ngày.
  - Phân loại crash: khử trùng theo 5 khung stack đầu đã symbol hóa, thu nhỏ ca, tạo issue, đưa ca vào corpus hồi quy.
  - Job PR "fuzz 1 giờ" cho target mới hoặc bị sửa; bộ đột biến có cấu trúc cho DWG (giải nén trang R2004+, đột biến, ghi lại trang, tính lại CRC/checksum) **[đề xuất]**.
- **Tiêu chí nghiệm thu:**
  1. Seed corpus DWG/DXF lấy từ QA-001 (tệp ≤ 1 MB), thu nhỏ mà giữ nguyên độ phủ cạnh.
  2. Crash cài sẵn trong target mẫu: 10 lần gặp → 1 issue; ca thu nhỏ tái hiện bằng một lệnh trên 3 hệ điều hành.
  3. Sau 24 giờ, bộ đột biến có cấu trúc đạt độ phủ cạnh bộ đọc DWG cao hơn đột biến byte ≥ 20% **[đề xuất]**.
  4. Corpus hồi quy chạy mỗi PR ≤ 3 phút.
  5. Xuất hằng ngày theo target: giờ CPU, độ phủ, crash mở/đóng (cho QA-010).
  6. Đạt DoD-C.

## 3. Cảnh và bộ benchmark

### QA-012 · Bộ cảnh benchmark đồ họa và tương tác
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** QA-001, QA-004
- **Đầu vào:** README §3.1 (BENCH-STD), 05 §1.1, 05 §21 S2.
- **Đầu ra:**
  - `qa/scenes/gen.py`: sinh DXF tất định theo seed, chuyển sang DWG R2004 và R2018 bằng ODA File Converter (QA-004) **[đề xuất]**; cache theo hash (phiên bản bộ sinh, seed).
  - Cảnh: 1 triệu và 10 triệu entity hỗn hợp; hatch dày; text dày (SHX, TTF, tiếng Việt); block lồng 8 cấp; 4 layout × 8 viewport; bản sao mỗi cảnh dời sang tọa độ VN-2000 (≈ 10⁹).
  - Kịch bản tương tác `qa/scenes/scenarios/*.json`: đường pan/zoom 600 khung, 10.000 điểm truy vấn OSNAP và hit test, chuỗi jig LINE, MOVE, ROTATE.
- **Tiêu chí nghiệm thu:**
  1. Cùng seed cho cùng DXF từng byte; `dwgdump` đọc DWG sinh ra đủ số entity theo tệp mô tả cảnh.
  2. Mỗi cảnh có tệp mô tả: số entity theo loại, hộp bao, kích thước tệp, số khung và số điểm của kịch bản.
  3. Điểm truy vấn phủ vùng dày, vùng trống, mép đối tượng; đường zoom đi từ toàn cảnh đến tỉ lệ 1:10⁶ **[đề xuất]**.
  4. Cảnh đăng ký vào `bench-std` (QA-001); sinh toàn bộ ≤ 30 phút trên máy mạnh.
  5. Đạt DoD-C.

### QA-105 · Bộ benchmark rút gọn mỗi PR và đầy đủ mỗi đêm trên runner GPU
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-003, QA-012, QA-104
- **Đầu vào:** 05 §1.1; 05 §14 (hiệu năng: mỗi PR rút gọn, mỗi đêm đầy đủ); README §3.1; DoD-L.
- **Đầu ra:**
  - `qa/bench/budgets.toml`: ngân sách 05 §1.1 và DoD-L dạng máy đọc, ghi nguồn từng dòng.
  - `qa/bench/suites.toml`: bộ PR và bộ đêm theo nhãn runner `thap`, `tam-trung`, `manh`, `mac`; bước chuẩn bị máy (dừng tiến trình nền, chế độ nguồn, chờ nguội).
  - Bài đối chứng thời gian mở với DWG TrueView (QA-104) trên máy Windows thấp và tầm trung.
- **Tiêu chí nghiệm thu:**
  1. Bộ PR ≤ 10 phút mỗi runner; bộ đêm đo đủ chỉ số 05 §1.1 trên 4 máy tham chiếu, scaling 1→32 luồng trên máy mạnh.
  2. Mỗi chỉ số lặp ≥ 5 lần, ghi trung vị, p99, độ lệch; hai đêm liên tiếp không đổi mã lệch ≤ 3% **[đề xuất]**.
  3. Ngưỡng chưa có benchmark hiện "chưa đo" trên QA-010, không bị bỏ qua âm thầm.
  4. Bảng đối chứng DWG TrueView ra tỉ lệ bài đo TrionCAD nhanh hơn (KPI G1 ≥ 70%, 06 §3).
  5. Đạt DoD-C.
- **Ngoài phạm vi:** khung benchmark, baseline, cổng chặn merge (NT-003); dựng runner GPU (NT-002).

## 4. Hình học property-based

### QA-009 · Khung kiểm thử property-based cho hình học
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-001, GEO-002, NT-005
- **Đầu vào:** 05 §14 (bất biến hình học); 05 §21 S3 (1 triệu ca ngẫu nhiên và suy biến không sai tô-pô); kiểu cơ bản GEO-001, dung sai GEO-002.
- **Đầu ra:** `qa/proptest/` (C++ trên GoogleTest): bộ sinh điểm, vector, ma trận, hộp bao và mô tả tham số của line, cung, tròn, ellipse, polyline có bulge (dữ liệu thuần, task GEO tự dựng đối tượng của mình); bộ sinh suy biến; thu nhỏ ca lỗi; tái lập bằng seed; chạy song song qua NT-005.
- **Tiêu chí nghiệm thu:**
  1. Bộ sinh suy biến có: thẳng hàng, tiếp xúc, trùng đoạn, gần tiếp xúc (1e-12 × kích thước), đoạn dài 0, bulge ≈ 0 và rất lớn, tọa độ ≈ 10⁹, tỉ lệ 1e-6 đến 1e6 **[đề xuất]**.
  2. Ca lỗi in seed và ca đã thu nhỏ; `--seed` tái lập đúng ca trên 3 hệ điều hành và mọi số luồng.
  3. Lỗi cài sẵn được thu nhỏ về ≤ 3 đối tượng trong ≤ 10 s.
  4. 1 triệu ca của thuộc tính mẫu (biến đổi affine rồi biến đổi ngược trả về điểm trong dung sai GEO-002) ≤ 60 s trên máy tầm trung; hiệu suất song song ≥ 60% đến 8 luồng.
  5. PR chạy 100.000 ca mỗi thuộc tính; đêm chạy 100 triệu ca **[đề xuất]**.
  6. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** thuộc tính của từng thuật toán (task GEO, CMD tự viết trên khung này).

## 5. So sánh ảnh in và ma trận phần cứng

### QA-005 · So sánh ảnh in và vector với bản in DWG TrueView
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** QA-104, QA-102, NT-005, NT-006
- **Đầu vào:** 05 §1.2 (SSIM ≥ 0,98 trên ≥ 97% tờ); 06 §3 (G1: SSIM ≥ 0,97 trên ≥ 95% tờ); 05 §14; PDF do DWG TrueView in (QA-104); nhận PDF/PNG bất kỳ: PDF của ta (PLT-004) và ảnh ngoài màn hình (RND-021) do task nghiệm thu của module đưa vào; bài báo SSIM của Wang và cộng sự (2004).
- **Đầu ra:**
  - `qa/imgcmp/pdfprobe` (C++, PDFium): raster hóa trang theo DPI, trích path và text ra JSON.
  - `qa/imgcmp/`: SSIM cửa sổ Gauss 11×11, bản đồ chênh lệch PNG, so vector (ghép path, khoảng cách Hausdorff); chế độ in và chế độ hiển thị; báo cáo theo tờ.
- **Tiêu chí nghiệm thu:**
  1. SSIM tất định từng bit trên 3 hệ điều hành; đường SIMD và scalar cho cùng kết quả.
  2. Tờ A1 ở 150 dpi so ≤ 1 s trên máy tầm trung **[đề xuất]**; song song theo tờ và theo dải ảnh.
  3. Ca cài sẵn (lệch 1 px, mất một nét, sai lineweight, sai màu, sai font) bị khoanh đúng vùng trên bản đồ chênh lệch.
  4. So vector báo nét thiếu, thừa, lệch > 0,1 mm trên giấy **[đề xuất]**.
  5. Xuất tỉ lệ tờ đạt ngưỡng theo chế độ, nguồn, phiên bản cho QA-010; kiểm trên bộ cặp PDF/PNG mẫu cài sẵn, không cần bộ ghi PDF của sản phẩm.
  6. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** lớp bọc PDFium của sản phẩm (PLT-011); đường GPU (RND-016); chạy cổng SSIM trên kho (task nghiệm thu của PLT, RND).

### QA-107 · Ma trận phần cứng cho tự chọn backend đồ họa
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-018, RND-021, INT-019, QA-012, QA-005
- **Đầu vào:** 05 §1.1 (tự chọn backend và mức chất lượng đúng trên 100% máy trong ma trận); README §3.1; bảng quyết định của RND-018.
- **Đầu ra:** `qa/hwmatrix/matrix.toml` (≥ 20 cấu hình **[đề xuất]**: Intel HD 520/620, Iris Xe, AMD APU, NVIDIA và AMD rời, laptop hai GPU, Apple Silicon, Linux Mesa, máy ảo không GPU, Remote Desktop, hai màn hình khác DPI và tần số, driver cũ) kèm quyết định kỳ vọng; `qa/hwmatrix/run.py`.
- **Tiêu chí nghiệm thu:**
  1. Mỗi cấu hình ghi quyết định thực tế (backend, MSAA, compute, LOD, số luồng nền) ở chế độ cắm điện và chạy pin.
  2. "Đúng" = trùng quyết định kỳ vọng, khởi tạo được, cảnh chuẩn khớp ảnh vàng SSIM ≥ 0,99 (QA-005) **[đề xuất]**, đạt fps của lớp máy (05 §1.1).
  3. Máy có GPU dùng được không rơi về render phần mềm.
  4. Chạy đêm trên máy lab, hằng tuần trên máy thuê có GPU **[đề xuất]**; cấu hình chưa có máy thật bị đánh dấu, không tính là đạt.
  5. Đạt DoD-C.

## 6. Bộ tham chiếu

### QA-108 · Định dạng ca tham chiếu, gói kịch bản cho pilot, nhập đáp án
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** QA-101, DWG-027, DWG-030
- **Đầu vào:** 05 §14 (bộ tham chiếu do người dùng AutoCAD tạo, cập nhật mỗi quý); README §3.1; danh sách doanh nghiệp pilot (06 §2.1).
- **Đầu ra:**
  - Định dạng ca `qa/refset/cases/<nhóm>/<mã>/case.toml` + `run.scr`: dữ liệu vào, các bước, tệp phải lưu, bất biến kỳ vọng, cách so.
  - `qa/refset/pack.py`: gói theo pilot (zip, hướng dẫn tiếng Việt, tệp chạy cả lô bằng `accoreconsole` hoặc SCRIPT, checksum).
  - `qa/refset/ingest.py`: nhận DWG/PDF/TXT trả về, ghi nguồn (pilot, phiên bản AutoCAD theo AppInfo, ngày), lưu `answers/<mã>/<phiên bản>`.
- **Tiêu chí nghiệm thu:**
  1. Bộ kiểm định dạng từ chối ca thiếu trường, `.scr` dùng đường dẫn tuyệt đối hoặc lệnh ngoài danh sách cho phép.
  2. Gói chạy cả lô không cần người bấm; mỗi ca lưu kết quả riêng; ca lỗi không chặn ca sau.
  3. Ingest từ chối tệp không do AutoCAD lưu (AppInfo), sai checksum gói, sai mã ca; báo ca thiếu đáp án.
  4. Hai pilot cho đáp án khác nhau trên cùng ca → đánh dấu "mâu thuẫn", loại khỏi nghiệm thu đến khi xử lý.
  5. Đạt DoD-C.

### QA-109 · Bộ tham chiếu ngắt dòng MTEXT
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** QA-108, QA-001, DWG-030
- **Đầu vào:** 05 §1.2 (ngắt dòng MTEXT ≥ 99% đoạn trùng); mã định dạng MTEXT theo DXF Reference công khai của Autodesk; MTEXT trong kho QA-001.
- **Đầu ra:**
  - `qa/refset/mtext/gen.py`: sinh ca theo chiều rộng, font SHX/TTF, cỡ chữ, mã `\P \T \S \W \Q \H \f`, tab, thụt lề, bullet, giãn dòng, tiếng Việt có dấu, từ dài hơn khung.
  - Kịch bản pilot: sao chép MTEXT, EXPLODE bản sao, lưu; trích dòng (chuỗi, đường chân chữ) từ TEXT thu được thành `answer.json`.
  - API `qa/refset/mtext/mtext_refset.h`: nạp ca và đáp án, chấm điểm theo đoạn cho RGN-009.
- **Tiêu chí nghiệm thu:**
  1. ≥ 1.500 đoạn tổng hợp và ≥ 1.000 đoạn trích từ kho; mỗi nhóm mã định dạng ≥ 50 đoạn **[đề xuất]**; ≥ 100 ca MTEXT nhiều cột (cột tĩnh và động, chiều cao cột tự động và thủ công) cho RGN-010.
  2. Ca ghi tên và hash tệp font phía pilot; ca không có font tương thích metric trong CI bị loại khỏi điểm và đếm riêng.
  3. Đoạn trùng = cùng số dòng, cùng chuỗi mỗi dòng (bỏ khoảng trắng cuối), đường chân chữ lệch ≤ 1e-6 × chiều cao chữ **[đề xuất]**.
  4. Báo cáo độ phủ đáp án (số ca có đáp án, số pilot, ca mâu thuẫn) sinh tự động; điểm chỉ tính trên ca có đáp án không mâu thuẫn.
  5. Đạt DoD-C.

### QA-110 · Bộ tham chiếu chuỗi kích thước và định dạng số
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** QA-108, QA-001, DWG-027, DWG-030
- **Đầu vào:** 05 §1.2 (chuỗi text của kích thước 100% trùng); DIMVAR theo DXF Reference công khai của Autodesk; kích thước trong kho QA-001.
- **Đầu ra:**
  - Trích từ kho: với tệp do AutoCAD lưu (AppInfo), lấy chuỗi trong block `*D` của mỗi DIMENSION làm đáp án, kèm số đo và DIMVAR hiệu lực (kiểu + ghi đè).
  - Bộ sinh ca pilot: ma trận DIMLUNIT, DIMDEC, DIMZIN, DIMRND, DIMLFAC, DIMPOST, DIMDSEP, DIMTOL, DIMLIM, nhóm DIMALT, DIMAUNIT, DIMADEC, DIMAZIN; `rtos`, `angtos` mọi chế độ và độ chính xác.
  - API `qa/refset/dimtext/dimtext_refset.h`: nạp ca, so chuỗi sau chuẩn hóa mã định dạng MTEXT, cho RGN-013, RGN-014, RGN-015.
- **Tiêu chí nghiệm thu:**
  1. ≥ 20.000 ca trích từ kho, ≥ 3.000 ca kích thước tổng hợp, ≥ 2.000 ca `rtos`/`angtos` **[đề xuất]**.
  2. Mỗi DIMZIN 0–15 và mỗi DIMLUNIT 1–6 có ≥ 50 ca; có số âm, số ≈ 10⁹, số rất nhỏ, làm tròn ở biên .5.
  3. Ca có text ghi đè hoặc `<>` tách nhóm riêng; ca không xác định được DIMVAR hiệu lực bị loại và đếm.
  4. Bộ so báo vị trí ký tự khác đầu tiên; 25.000 ca so ≤ 10 s.
  5. Đạt DoD-C.

### QA-111 · Thư viện kịch bản tham chiếu: lệnh sửa hình học
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** QA-108
- **Đầu vào:** 05 §14 (khoảng 500 ca TRIM/OFFSET/FILLET/DIM/HATCH…); 03 Phụ lục A; 07.2 UI-E10 (tùy chọn lệnh); 05 §1.2.
- **Đầu ra:** `qa/refset/cases/modify/` ≥ 300 ca **[đề xuất]** cho TRIM, EXTEND (TRIMEXTENDMODE 0/1, Fence, Crossing, Edge, Project), BREAK, JOIN, LENGTHEN, OFFSET (Through, Erase, Layer, Multiple), FILLET, CHAMFER (Radius/Distance 0, Trim/No trim, Polyline, Multiple, Angle); bộ sinh tham số `gen_modify.py`.
- **Tiêu chí nghiệm thu:**
  1. Mỗi tùy chọn của mỗi lệnh có ≥ 5 ca; mỗi cặp loại đối tượng (line, cung, tròn, ellipse, polyline có cung, spline) có ≥ 2 ca cho TRIM, EXTEND, OFFSET, FILLET.
  2. Có ca suy biến: song song, tiếp xúc, trùng đoạn, đối tượng trong block, tọa độ VN-2000, độ dài 0 (kỳ vọng thông báo lỗi).
  3. Mỗi ca có bất biến kiểm được khi chưa có đáp án (khoảng cách offset = d, cung bo tiếp xúc với bán kính r, phần còn lại không đổi).
  4. 100% ca qua bộ kiểm định dạng QA-108; ma trận phủ tùy chọn sinh tự động.
  5. Đạt DoD-C.

### QA-112 · Thư viện kịch bản tham chiếu: HATCH, BOUNDARY, kích thước, leader
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** QA-108
- **Đầu vào:** 05 §14; 03 Phụ lục A; 07.2 UI-E10; 05 §1.2.
- **Đầu ra:** `qa/refset/cases/annot/` ≥ 200 ca **[đề xuất]** cho HATCH (pick point, chọn đối tượng, island Normal/Outer/Ignore, HPGAPTOL, liên kết, gốc mẫu, tỉ lệ, góc, gradient), BOUNDARY, DIMLINEAR, DIMALIGNED, DIMANGULAR, DIMRADIUS, DIMDIAMETER, DIMARC, DIMORDINATE, DIMJOGGED, DIMBASELINE, DIMCONTINUE, MLEADER, QLEADER; bộ sinh `gen_annot.py`.
- **Tiêu chí nghiệm thu:**
  1. Mỗi tùy chọn có ≥ 5 ca; HATCH có ≥ 40 ca biên cong, island lồng ≥ 3 cấp, khe hở nhỏ hơn và lớn hơn HPGAPTOL.
  2. Kích thước phủ quy tắc fit (DIMATFIT 0–3, DIMTMOVE 0–2), mũi tên, chữ trong và ngoài, kiểu có đơn vị phụ và dung sai.
  3. Mỗi ca có bất biến kiểm được khi chưa có đáp án (biên kín, diện tích vùng hatch, điểm định nghĩa kích thước).
  4. 100% ca qua bộ kiểm định dạng QA-108; ma trận phủ tùy chọn sinh tự động.
  5. Đạt DoD-C.

### QA-007 · Bộ tham chiếu: chạy phía TrionCAD, so khớp hình học và tô-pô tự động
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** QA-108, QA-111, QA-112, INT-007, INT-019, DWG-030, GEO-002, GEO-005, GEO-006, GEO-007
- **Đầu vào:** 05 §1.2 (sai số ≤ 1e-9 × kích thước bản vẽ, tô-pô trùng); 05 §14; ca và đáp án (QA-108, QA-111, QA-112); `trion-console` và SCRIPT.
- **Đầu ra:**
  - `qa/refset/compare/` (C++): ghép đối tượng giữa kết quả và đáp án không dựa vào handle; so hình học, tô-pô (số đoạn, thứ tự, chiều, kín/hở), thuộc tính; bộ đánh giá bất biến.
  - `qa/refset/run.py`: chạy cùng `.scr` bằng `trion-console`, so, báo cáo theo lệnh và tùy chọn; job đêm.
- **Tiêu chí nghiệm thu:**
  1. Ca cài lỗi (dời 1e-6 × kích thước, đảo chiều, thêm hoặc bớt đoạn, đổi layer) bị phát hiện 100%; nhiễu ≤ 1e-10 × kích thước không bị báo.
  2. Kết quả ghép không phụ thuộc thứ tự tạo; đối tượng trùng nhau được ghép tối ưu, tất định.
  3. Báo tỉ lệ khớp theo lệnh; lệnh đã đạt 100% mà tụt thì job đỏ và chặn merge.
  4. 500 ca chạy ≤ 20 phút trên máy mạnh, song song theo ca.
  5. Đáp án lưu theo phiên bản AutoCAD; đổi phiên bản đáp án sinh báo cáo khác biệt.
  6. Đạt DoD-C, DoD-H.

## 7. Bộ LISP

### QA-008 · Bộ LISP và bộ chạy bộ LISP
- **Mốc:** S5 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** QA-101, NT-002
- **Đầu vào:** 05 §14 (bộ LISP); 05 §21 S5 (chạy được 10 LISP cộng đồng); 100 LISP có giấy phép hoặc thỏa thuận (06 §2.1, tuần 1–4); hợp đồng bản ghi JSON của trình chạy `lisprun` (LSP-006).
- **Đầu ra:** `qa/lisp/`: 100 tệp LISP kèm `.dcl` đi cùng; định dạng ca `case.toml` (tệp LISP, biểu thức gọi, kịch bản input cho `get*`, output kỳ vọng, bản ghi lệnh và sysvar kỳ vọng, nguồn kỳ vọng); `qa/lisp/run.py` gọi trình chạy ngoài qua tham số `--runner`, so và phân loại; job đêm.
- **Tiêu chí nghiệm thu:**
  1. 100 LISP có hồ sơ nguồn và giấy phép trong QA-101; ≥ 30 tệp dùng `command` có `pause` **[đề xuất]**; kỳ vọng ghi nguồn: AutoCAD của pilot hoặc soạn tay.
  2. So output in ra (từng ký tự, chuẩn hóa xuống dòng), bản ghi lệnh và sysvar cuối; số thực so với dung sai tương đối 1e-9.
  3. Phân loại: đúng, sai output, sai bản ghi lệnh, lỗi LISP (thống kê hàm thiếu theo tên), treo (timeout 30 s); kiểm bằng trình chạy giả lập trên 20 ca cài sẵn, đúng 100%.
  4. Báo cáo hàm thiếu xếp theo số LISP bị chặn, làm đầu vào xếp ưu tiên LSP.
  5. 100 ca ≤ 5 phút, mỗi ca một tiến trình.
  6. Đạt DoD-C.
- **Ngoài phạm vi:** so trạng thái bản vẽ và chạy qua `trion-console` (QA-106).

### QA-106 · Bộ LISP 500 tệp chạy qua `trion-console`
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** QA-008, QA-108, INT-019, LSP-019, LSP-128
- **Đầu vào:** 05 §1.2 (≥ 90% LISP chạy đúng không phải sửa); 06 §4 (≥ 60% tháng 22–24, ≥ 75% tháng 26–27); 06 §5 (≥ 90% có tập con `vla-*`); gói pilot QA-108.
- **Đầu ra:** ≥ 500 ca trong `qa/lisp/`; chế độ `--engine console` của `run.py`; đáp án AutoCAD lấy qua gói QA-108; LISP có DCL chạy với kịch bản trả lời hộp thoại (chế độ không giao diện của LSP-019).
- **Tiêu chí nghiệm thu:**
  1. ≥ 500 LISP chia nhóm (vẽ, sửa, text, block và attribute, bảng, tệp, DCL, `vla-*`), mỗi nhóm ≥ 30.
  2. ≥ 80% ca có đáp án do AutoCAD tạo **[đề xuất]**; ca soạn tay đánh dấu riêng.
  3. Tỉ lệ chạy đúng theo nhóm và tổng, xu hướng theo đêm; mốc 60%, 75%, 90% hiện trên QA-010.
  4. Lượt đầy đủ ≤ 30 phút, mỗi ca một tiến trình, song song theo ca.
  5. Đạt DoD-C.

## 8. Kiểm thử UI tự động

### QA-113 · Quản lý ảnh baseline giao diện
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** QA-005, NT-002
- **Đầu vào:** DoD-U mục 1 (Tối/Sáng; 100/150/200%; 1366×768, 1920×1080); `design/prototype/js/catalog.js` (172 mã màn hình).
- **Đầu ra:** kho `qa/ui-baselines/` (chỉ mục + blob theo hash); khóa ảnh: mã UI-xxx × chủ đề × tỉ lệ × độ phân giải × ngôn ngữ × hệ điều hành; so pixel có dung sai khử răng cưa và mặt nạ vùng động (dùng bộ so QA-005); trang HTML xem chênh lệch, liên kết `design/prototype/index.html#UI-xxx`; lệnh `baseline approve`.
- **Tiêu chí nghiệm thu:**
  1. Đổi màu 1 px ngoài mặt nạ bị phát hiện; vùng có mặt nạ (đồng hồ, caret) bị bỏ qua.
  2. Baseline chỉ đổi qua `approve` trong PR có người duyệt; CI chặn sửa blob trực tiếp.
  3. Báo baseline mồ côi và mã màn hình thiếu baseline so với `catalog.js`.
  4. So 2.000 ảnh ≤ 2 phút trên runner **[đề xuất]**.
  5. Đạt DoD-C.

### QA-011 · Harness kiểm thử UI tự động
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** UI0-001, UI0-014, UI0-105, UIA-007, QA-113
- **Đầu vào:** DoD-U, DoD-L, 07.1 §1.3; Qt Test và cây accessibility của Qt; số đo độ trễ và thời gian khung trong ứng dụng (UI0-105).
- **Đầu ra:**
  - Tác nhân kiểm thử nạp vào `trion` khi bật cờ build `TRION_QA_AGENT` (không có trong bản phát hành): tìm widget theo `objectName` hoặc tên accessible, nhấp, gõ, chờ rảnh, chụp cửa sổ.
  - Kịch bản `qa/ui/scenarios/<UI-xxx>.json`; bộ chạy trên 3 hệ điều hành; ảnh gửi QA-113.
  - Đo độ trễ đầu-cuối: tiêm input mức hệ điều hành (SendInput, CGEvent, XTest) có dấu thời gian, ghép với dấu khung hình của UI0-105, tổng hợp p50/p99 theo dòng DoD-L.
- **Tiêu chí nghiệm thu:**
  1. Mỗi kịch bản sinh đủ ảnh theo ma trận DoD-U mục 1 và so qua QA-113.
  2. Độ trễ gõ phím → ký tự hiện ở dòng lệnh (UIA-007) báo p50, p99; sai số phép đo ≤ 1 ms **[đề xuất]**.
  3. Có kịch bản mẫu cho gõ dòng lệnh, mở menu, pan/zoom; vượt ngưỡng p99 của DoD-L thì job đỏ. Task UI thêm kịch bản của mình theo mẫu.
  4. Chạy 20 lần liên tiếp cho cùng ảnh và cùng kết luận đạt/trượt.
  5. Kiểm symbol: bản phát hành không chứa tác nhân.
  6. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** đo trong ứng dụng và watchdog luồng UI (UI0-105); cổng khởi động ấm (UI0-115, dùng QA-011).

## 9. Chỉ số và báo cáo cổng

### QA-114 · Bộ sinh báo cáo cổng và báo cáo G0
- **Mốc:** S6 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** QA-002, QA-008, QA-009, QA-105, VN-003
- **Đầu vào:** 05 §21 (tiêu chí S1–S6); 06 §2.2 (cổng G0); JSON của QA-002, QA-008, QA-009, QA-105, bộ đo VN-003, kết quả CI các spike S6.
- **Đầu ra:** `qa/gates/gate.py`; định nghĩa `qa/gates/g0.toml` (chỉ số, ngưỡng, nguồn JSON); báo cáo Markdown và HTML; mã thoát khác 0 khi trượt.
- **Tiêu chí nghiệm thu:**
  1. G0 kiểm: S1 đọc ≥ 90% `spike-1000` mức entity lõi, mở không chậm hơn DWG TrueView; S2 1 triệu entity ≥ 60 fps tầm trung, ≥ 30 fps Intel HD 520/620; S3 1 triệu ca không sai tô-pô; S4 ≥ 95% trên 1.000 mẫu; S5 chạy 10 LISP; S6 đúng 100% bộ ký tự tiếng Việt.
  2. Chỉ số thiếu dữ liệu hiện "chưa đo" và làm cổng trượt.
  3. Mỗi số liên kết tới lượt CI và commit nguồn.
  4. Thêm cổng mới chỉ cần thêm tệp `.toml`; QA-010, QA-116 không sửa `gate.py`.
  5. Đạt DoD-C.

### QA-115 · Pipeline chỉ số phiên không crash
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLF-007, PLF-008
- **Đầu vào:** 05 §1.2 (≥ 99,7% phiên không crash); 06 §4 (G2: ≥ 99,5%); sự kiện phiên từ telemetry có đồng ý (PLF-008); crash dump (PLF-007).
- **Đầu ra:** job hằng ngày `qa/metrics/crashfree/`: ghép phiên với crash theo mã phiên; tỉ lệ theo phiên bản, kênh, hệ điều hành, lớp máy; khoảng tin cậy Wilson 95%; JSON cho QA-010 và cổng phát hành.
- **Tiêu chí nghiệm thu:**
  1. Phiên = từ khởi động đến thoát; crash = có dump, hoặc phiên mất sự kiện kết thúc và lần chạy sau gửi dump; treo > 30 s bị buộc đóng tính là crash **[đề xuất]**.
  2. Dữ liệu giả lập 1 triệu phiên có tỉ lệ crash biết trước: sai số ≤ 0,01 điểm phần trăm.
  3. Chỉ xử lý phiên có đồng ý; chỉ dùng mã phiên ngẫu nhiên, không đường dẫn hay nội dung bản vẽ.
  4. Phiên bản có < 1.000 phiên hiện "chưa đủ mẫu", không kết luận đạt/trượt **[đề xuất]**.
  5. Top 20 chữ ký crash theo số phiên bị ảnh hưởng, liên kết issue.
  6. Đạt DoD-C.

### QA-010 · Bảng chỉ số hằng ngày và báo cáo cổng G1
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** QA-102, QA-004, QA-005, QA-006, QA-008, QA-105, QA-114, QA-115
- **Đầu vào:** JSON của các bộ chạy QA; 06 §3 (KPI chất lượng G1).
- **Đầu ra:** `qa/dashboard/`: trang HTML tĩnh sinh mỗi đêm, lịch sử trong SQLite; các trang kho bản vẽ, đối chứng chéo, so ảnh, benchmark so ngân sách, fuzz, LISP, crash-free; `qa/gates/g1.toml`.
- **Tiêu chí nghiệm thu:**
  1. Cập nhật trước 8:00 mỗi ngày từ lượt đêm; nguồn thiếu hiện rõ, không dùng số cũ âm thầm.
  2. Cảnh báo khi tỉ lệ mở giảm > 0,1 điểm %, benchmark chậm > 5%, có crash fuzz mới, crash-free giảm > 0,2 điểm % **[đề xuất]**.
  3. Xu hướng 90 ngày cho mỗi chỉ số; mỗi điểm liên kết lượt CI.
  4. G1 kiểm: mở thành công ≥ 98% `v-10000`, SSIM ≥ 0,97 trên ≥ 95% tờ, nhanh hơn DWG TrueView ở ≥ 70% bài đo.
  5. Sinh trang ≤ 2 phút; chỉ truy cập trong mạng nội bộ, không hiện tên tệp của khách hàng.
  6. Đạt DoD-C.

### QA-116 · Báo cáo cổng G2 và G3
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** QA-010, QA-003, QA-007, QA-106, QA-109, QA-110
- **Đầu vào:** 06 §4 (G2); 06 §5 (G3); 05 §1; danh sách lệnh P0 (03 Phụ lục A).
- **Đầu ra:** `qa/gates/g2.toml`, `qa/gates/g3.toml`; trang cổng và trang bộ tham chiếu trên QA-010.
- **Tiêu chí nghiệm thu:**
  1. G2: mọi lệnh P0 đạt bộ tham chiếu (QA-007); tệp ghi qua 3 bộ đọc (QA-003); crash-free ≥ 99,5%.
  2. G3: đủ chỉ số 05 §1.1 và §1.2 (mở ≥ 99,5% kho, round-trip 100%, SSIM ≥ 0,98 trên ≥ 97% tờ, chuỗi kích thước 100%, MTEXT ≥ 99%, LISP ≥ 90%, crash-free ≥ 99,7%, mọi ngân sách hiệu năng).
  3. Lệnh P0 chưa có ca tham chiếu bị liệt kê và làm G2 trượt.
  4. Chạy hằng đêm từ EA; mỗi số có nguồn JSON và lượt CI.
  5. Đạt DoD-C.

## Tóm tắt theo mốc

| Mốc | Task | Số task |
|---|---|---|
| S1 | QA-101, QA-001, QA-002, QA-104, QA-004, QA-006 | 6 |
| S2 | QA-012, QA-105 | 2 |
| S3 | QA-009 | 1 |
| S5 | QA-008 | 1 |
| S6 | QA-114 | 1 |
| V | QA-102, QA-103, QA-005, QA-107, QA-108, QA-109, QA-110, QA-113, QA-011, QA-115, QA-010 | 11 |
| VP | QA-003 | 1 |
| EA | QA-111, QA-112, QA-007, QA-106, QA-116 | 5 |
| **Tổng** | | **28** |
