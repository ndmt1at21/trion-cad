# DOC — Mô hình bản vẽ: handle, lưu trữ, bảng ký hiệu, transaction, undo, snapshot, xref, phiên, autosave

> Phạm vi: lõi dữ liệu của bản vẽ trong `core/doc/` (mã sinh từ schema ở `core/doc/generated/`, kiểm thử `core/doc/tests/`, benchmark `core/doc/bench/`, fuzz `core/doc/fuzz/`, tài liệu thiết kế `core/doc/docs/`).
> - Căn cứ: [05](../05-kien-truc-ky-thuat.md) §0, §1, §5, §6.2, §9.2, §9.4, §12, §16, §18; [03](../03-chuc-nang-autocad.md) §2, §6, §11, §12, §13, §15; [07.1](../07-dac-ta-ui-1-khung-ung-dung.md) §2, §5; [07.2](../07-dac-ta-ui-2-bang-va-hop-thoai.md) UI-B09, UI-B10, UI-B12.
> - Module này chỉ làm engine. Lệnh (UNDO, PURGE, AUDIT, RENAME, SAVE, WBLOCK…) thuộc các tệp CMD và gọi API ở đây.

## 0. Ghi chú thiết kế

- **Một luồng ghi** (05 §5.4): luồng UI sở hữu `Document`. Mọi sửa đổi đi qua transaction (DOC-007), mọi luồng khác chỉ đọc qua snapshot (DOC-009). Sửa hàng loạt chạy fork-join *bên trong* transaction của luồng ghi; luồng khác không thấy gì trước commit **[đề xuất]**.
- **Định danh:** `ObjectId` chỉ có nghĩa trong phiên; handle là định danh bền. Handle không bao giờ bị đánh lại hay dùng lại (05 §6.2). Mọi thứ ra khỏi phiên (nhật ký tuần tự hóa, tệp phụ, sự kiện cho SDK) dùng handle.
- **Bố cục:** cột phân trang (trang 16 KB **[đề xuất]**), SoA cho trường nóng, copy-on-write theo trang; dữ liệu dài (đỉnh polyline, chuỗi, vòng hatch, xdata) nằm trong blob bất biến, sửa là cấp blob mới.
- **Tất định:** chỉ số `ObjectId` gán theo thứ tự object map (thứ tự handle), không theo thứ tự hoàn thành của luồng. Mọi phép duyệt có thứ tự ổn định. Hàm băm chuẩn tắc của bản vẽ (DOC-003) là chuẩn so trong mọi test tất định.
- **Sự kiện** phát sau commit, gom lô, trên luồng ghi. Phản ứng cần ghi thêm (sinh lại block kích thước, hatch liên kết) chạy bằng hook trước commit trong cùng transaction. Khác ObjectARX (phát tức thời) **[đề xuất]**.
- **Hợp đồng cho SDK-001** (05 §12): API công khai chỉ dùng ID dạng POD, mã lỗi enum ổn định, callback có ngữ cảnh `void*`, không ném ngoại lệ qua biên. Database, transaction, reactor của SDK ánh xạ 1-1 sang `Document`, transaction (DOC-007), sự kiện và reactor (DOC-105, DOC-106).
- **Chuỗi** giữ nguyên giá trị đã giải mã, không chuẩn hóa NFC khi nạp hay khi so tên. Bác sĩ Font ở chế độ hiển thị không đổi dữ liệu (05 §13) **[đề xuất]**.
- **Không vòng phụ thuộc:** `core/doc` không phụ thuộc codec, regen hay Qt. Bộ nạp DWG, bộ ghi DWG, hàm hộp bao của text/kích thước… được module sở hữu đăng ký lúc chạy.
- **Ngân sách bộ nhớ:** phần `core/doc` ≤ 600 MB trong tổng 1,2 GB cho DWG 100 MB (05 §1.1) **[đề xuất]**.
- **Đầu vào không tin cậy** của module (xdata thô, `.tcview`, `.dwl`/`.dwl2`, tệp tràn undo, tệp autosave) theo DoD-C mục 7.

## 1. Định danh, lưu trữ, kiểu đối tượng

### DOC-001 · Định danh: handle 64-bit, handle seed, `ObjectId` (chỉ số + thế hệ), bảng handle → chỉ số
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-001, NT-003, NT-004, NT-007
- **Đầu vào:** 05 §5.2, §6.2 (handle không đánh lại); Đặc tả DWG của ODA v5.4.1, phần handle và object map; DXF Reference, biến `$HANDSEED`; API arena và pool của NT-007.
- **Đầu ra:**
  - `core/doc/id/handle.h`: `Handle` 64-bit, `0` là null, đọc/in dạng hex như DXF;
  - `core/doc/id/object_id.h`: `ObjectId` gồm 32 bit chỉ số và 32 bit thế hệ;
  - `core/doc/id/handle_table.{h,cpp}`: handle → `ObjectId` và ngược lại;
  - `core/doc/id/handle_seed.{h,cpp}`; `core/doc/docs/handles.md` (quy tắc seed, cấp phát, về hưu);
  - `core/doc/tests/id_*.cpp`, `core/doc/bench/handle_lookup.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Quy tắc seed: handle mới = seed, rồi seed tăng. Khi nạp: seed := max(`HANDSEED` trong tệp, handle lớn nhất + 1) và ghi cảnh báo nếu tệp sai. Seed không bao giờ giảm, kể cả khi undo việc tạo đối tượng.
  2. Không dùng lại handle: 10⁶ chu kỳ tạo/xóa/undo/redo ngẫu nhiên (tái lập bằng seed) không cấp trùng handle nào; handle của đối tượng đã xóa hoặc đã dọn "về hưu" vĩnh viễn.
  3. `ObjectId` cũ (khác thế hệ) sau khi slot được dùng lại → trả `StaleId`, không bao giờ trả nhầm đối tượng; đạt 100% ca thử.
  4. Tra handle → `ObjectId` trên bảng 10 triệu handle: trung bình ≤ 30 ns, p99 ≤ 100 ns trên máy tầm trung **[đề xuất]**; dựng bảng từ object map 10 triệu mục ≤ 150 ms bằng 8 luồng **[đề xuất]**; 0 cấp phát trong vòng tra (đếm bằng NT-004).
  5. Duyệt bảng luôn theo handle tăng, không phụ thuộc thứ tự băm; hash kết quả trùng trên 3 hệ điều hành.
  6. Đạt DoD-C, DoD-H.

### DOC-101 · Bảng đối tượng và cột dữ liệu phân trang
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DOC-001, GEO-001, NT-007
- **Đầu vào:** 05 §9.4 (bố cục hướng dữ liệu, arena theo document); README DoD-H mục 4; kiểu điểm và hộp bao của GEO-001; arena NT-007.
- **Đầu ra:**
  - `core/doc/store/object_table.{h,cpp}`: với mỗi `ObjectId` lưu handle, mã loại, slot trong bảng loại, owner, cờ trạng thái, phiên bản sửa;
  - `core/doc/store/column.h`: cột phân trang (trang 16 KB), trang có bộ đếm tham chiếu để DOC-109 làm copy-on-write;
  - `core/doc/store/type_table.h`: bảng theo loại, free list, thế hệ;
  - `core/doc/store/blob_arena.{h,cpp}`: blob bất biến sau khi công bố;
  - `core/doc/tests/store_*.cpp`, `core/doc/bench/store_*.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Duyệt tuần tự 1 triệu phần tử của một cột `double` đạt ≥ 80% băng thông của `std::vector` cùng dữ liệu trên máy tầm trung **[đề xuất]**.
  2. Thêm phần tử không di chuyển dữ liệu đã có (không realloc cả mảng); địa chỉ trang ổn định, có test.
  3. Slot giải phóng được dùng lại theo thứ tự tất định, tăng thế hệ; `ObjectId` cũ bị phát hiện theo DOC-001.
  4. Sao chép một trang 16 KB ≤ 2 µs; bộ phụ trội của bảng đối tượng ≤ 32 byte/đối tượng **[đề xuất]**.
  5. Không cấp phát heap khi thêm phần tử ở trạng thái ổn định (arena NT-007, đếm bằng NT-004).
  6. Đạt DoD-C, DoD-H.

### DOC-102 · Nạp song song tất định vào kho
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-101, NT-005
- **Đầu vào:** 05 §5.3 (giải mã đối tượng song song), §9.2; README DoD-C mục 4; `parallel_for` của NT-005.
- **Đầu ra:** `core/doc/store/load_builder.{h,cpp}`: `LoadBuilder` nhận danh sách handle theo thứ tự object map, cấp trước `ObjectId` và slot, cho nhiều luồng ghi đồng thời vào slot riêng, `finish()` công bố `Database`; test và benchmark.
- **Tiêu chí nghiệm thu:**
  1. `ObjectId` và slot gán trước khi giải mã, theo thứ tự object map: cùng tệp cho cùng `ObjectId` ở 1, 2, 8, 32 luồng (so hash chuẩn tắc).
  2. Ghi đồng thời không khóa trên đường nóng; TSan sạch với 32 luồng.
  3. Nạp 10 triệu đối tượng giả lập (LINE/ARC/TEXT trộn, payload dựng sẵn) ≤ 400 ms bằng 8 luồng trên máy tầm trung **[đề xuất]**; hiệu suất song song ≥ 70% đến 16 luồng (một đoạn của đường mở DWG, 05 §1.1).
  4. Đối tượng giải mã lỗi được đánh dấu `Failed` kèm mã lỗi, không ảnh hưởng slot khác; `finish()` liệt kê đủ các đối tượng lỗi.
  5. Đạt DoD-C, DoD-H.

### DOC-002 · Lưu trữ hướng dữ liệu: mảng theo loại (SoA cho trường nóng), header chung của entity
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-101, DOC-102
- **Đầu vào:** README §5.5 DOC-002; 05 §5.2, §9.4; DXF Reference, mã nhóm chung của entity (8, 6, 62, 420, 370, 440, 48, 60, 330, 360, 102).
- **Đầu ra:**
  - `core/doc/store/entity_header.{h,cpp}`: các cột layer, linetype, màu ACI/true color, lineweight, transparency, tỉ lệ linetype, cờ hiển thị, owner, tham chiếu xdata (blob), extension dictionary, plot style, material;
  - `core/doc/store/iterate.h`: duyệt theo loại, theo owner (không gian, block), theo thứ tự lưu;
  - test và benchmark bộ nhớ, tốc độ duyệt.
- **Tiêu chí nghiệm thu:**
  1. Header chung ≤ 48 byte/entity; LINE đầy đủ ≤ 112 byte/entity kể cả bảng đối tượng **[đề xuất]**; đo bằng benchmark bộ nhớ.
  2. Lọc 1 triệu entity theo layer ≤ 1 ms bằng 8 luồng, ≤ 4 ms một luồng **[đề xuất]**.
  3. Duyệt entity của một không gian giữ đúng thứ tự trong tệp (cần cho round-trip và thứ tự vẽ mặc định).
  4. Cột nóng là SoA, phần tử duyệt nằm gọn dòng cache 64 byte (kiểm bằng `static_assert` kích thước và căn lề).
  5. Đạt DoD-C, DoD-H.

### DOC-003 · Kiểu đối tượng sinh từ schema (khoảng 120 loại) vào `core/doc/generated`
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DOC-002, DWG-001, DWG-002
- **Đầu vào:** 05 §6.2 mục 1; README §4.2 (không viết tay struct trùng schema); DSL của DWG-001; bộ sinh của DWG-002; schema của DWG-011, DWG-012 khi có.
- **Đầu ra:**
  - emitter `core/doc/codegen/` cắm vào bộ sinh DWG-002;
  - mã sinh vào `core/doc/generated/` (sinh lúc build, không commit): bảng cột từng loại, view đọc có kiểu, enum mã thuộc tính, bảng loại tham chiếu (hard/soft owner, hard/soft pointer) của từng trường handle, hàm băm chuẩn tắc theo trường;
  - registry loại (tên lớp DXF, lớp động);
  - bộ schema mẫu `core/doc/tests/schema/`.
- **Tiêu chí nghiệm thu:**
  1. Mọi schema trong `core/schema/` sinh ra kiểu biên dịch sạch. Thêm một schema mới (ca thử: loại giả `TESTCURVE`) không phải sửa tay tệp nào khác.
  2. Bộ schema mẫu phủ 100% tính năng của DSL (điều kiện phiên bản, mảng, chuỗi, handle, nhóm lặp); mỗi tính năng có ca thử đọc/ghi giá trị qua view.
  3. Mã thuộc tính và tệp sinh ra trùng từng byte giữa hai lần build và giữa 3 hệ điều hành.
  4. Hàm băm chuẩn tắc của bản vẽ duyệt theo handle, không phụ thuộc `ObjectId`, địa chỉ, số luồng.
  5. Sinh mã cho khoảng 120 loại ≤ 10 s; mã sinh ra làm build sạch `core/doc` tăng ≤ 60 s **[đề xuất]**.
  6. Đạt DoD-C.
- **Ngoài phạm vi:** viết schema từng loại (DWG-011…DWG-016); setter ghi qua transaction (DOC-103 bổ sung vào emitter).

### DOC-010 · Biến header của bản vẽ: lưu và truy cập có kiểu
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-003
- **Màn hình:** UI-B08
- **Đầu vào:** DXF Reference, mục HEADER (khoảng 300 biến); schema header do DWG-008 viết; 03 §16 (`SETVAR`); 07.2 UI-B08 (`DWGPROPS`).
- **Đầu ra:**
  - `core/doc/header/header_vars.{h,cpp}` sinh từ schema: truy cập có kiểu (`get<LTSCALE>()`), tra theo tên không phân biệt hoa thường cho INT-005, biến chỉ có ở một số phiên bản, giá trị mặc định;
  - `core/doc/header/summary_info.{h,cpp}`: tiêu đề, chủ đề, tác giả, từ khóa, ghi chú, thuộc tính tùy biến, thống kê sửa (nơi DWG-027 điền vào);
  - test.
- **Tiêu chí nghiệm thu:**
  1. Tra theo tên ≤ 100 ns p99 **[đề xuất]**; truy cập có kiểu không qua bảng tra.
  2. Đặt sai kiểu hoặc ngoài miền giá trị của schema → `InvalidValue`, giá trị cũ giữ nguyên; có ca thử cho mọi kiểu (int16, int32, double, điểm, chuỗi, handle, màu).
  3. Biến tham chiếu đối tượng (`CLAYER`, `CELTYPE`, `TEXTSTYLE`, `DIMSTYLE`, `CMLSTYLE`…) lưu cả `ObjectId` và handle; handle treo được báo cho DOC-015.
  4. Biến chưa có trong schema được giữ dạng thô để ghi lại nguyên vẹn (phối hợp DWG-008).
  5. Đạt DoD-C.

### DOC-004 · Bảng ký hiệu và dictionary: 9 bảng, named object dictionary, cây sở hữu, tra tên
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DOC-003, DOC-010, NT-012
- **Đầu vào:** README §5.5 DOC-004; DXF Reference, mục TABLES và OBJECTS; Đặc tả DWG của ODA v5.4.1 (control object, danh sách đối tượng sở hữu); so sánh không phân biệt hoa thường của NT-007, NT-012.
- **Đầu ra:**
  - `core/doc/tables/`: LAYER, LTYPE, STYLE, DIMSTYLE, UCS, VIEW, VPORT, APPID, BLOCK_RECORD, control object, chỉ mục tên;
  - `core/doc/dict/`: DICTIONARY, dictionary có giá trị mặc định, named object dictionary;
  - `core/doc/owner/`: owner ↔ danh sách sở hữu; danh sách entity của block record theo thứ tự lưu;
  - `Database::createEmpty()`: bộ khung hợp lệ (layer `0`, linetype ByBlock/ByLayer/Continuous, kiểu chữ và kiểu kích thước Standard, APPID `ACAD`, `*Model_Space`, `*Paper_Space`, các dictionary chuẩn của NOD như `ACAD_GROUP`, `ACAD_LAYOUT`);
  - test.
- **Tiêu chí nghiệm thu:**
  1. Tra tên không phân biệt hoa thường như AutoCAD, kể cả chữ có dấu (NT-012), ≤ 100 ns p99 với bảng 100.000 bản ghi **[đề xuất]**; không chuẩn hóa NFC khi so (§0).
  2. Giữ nguyên thứ tự bản ghi và thứ tự mục dictionary như trong tệp; duyệt tất định.
  3. Cây sở hữu nhất quán hai chiều: owner liệt kê con và con trỏ về owner, kiểm trên mọi tệp thử của module.
  4. `createEmpty()` cho bản vẽ qua được bộ kiểm trong test (owner hai chiều, tên duy nhất trong mỗi bảng, không tham chiếu treo, biến header trỏ đúng bản ghi); là fixture chung cho mọi test của module.
  5. Bản ghi phụ thuộc xref (tên dạng `XREF|TÊN`, cờ phụ thuộc) tra và liệt kê tách biệt với bản ghi thường.
  6. Đạt DoD-C.

## 2. Thay đổi: transaction, nhật ký, sự kiện, dữ liệu mở rộng

### DOC-103 · Transaction lõi: ghi theo thuộc tính, commit/abort, lồng nhau, cổng ghi
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DOC-004, QA-009
- **Đầu vào:** 05 §5.3 (mọi thay đổi chỉ qua API của `core/doc`), §5.4 (một luồng ghi), §12 (khái niệm transaction); README DoD-C mục 8; mã thuộc tính của DOC-003; bộ sinh ca ngẫu nhiên và thu nhỏ ca lỗi của QA-009.
- **Đầu ra:**
  - `core/doc/tx/transaction.{h,cpp}`: `Document::beginWrite(label)`, tạo/sửa/xóa theo mã thuộc tính, `commit()`, `abort()`, transaction lồng (savepoint), RAII tự abort;
  - `core/doc/tx/write_gate.h`: cổng ghi và lý do từ chối (`ReadOnly`, `Loading`, `WrongThread`);
  - mở rộng emitter DOC-003 sinh setter có ghi nhật ký;
  - `core/doc/tx/invariants.cpp` (kiểm khi commit); test, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Abort (kể cả abort lồng) trả bản vẽ về đúng trạng thái trước, so bằng hàm băm chuẩn tắc; 10⁶ chuỗi thao tác ngẫu nhiên (QA-009) không sai.
  2. Nhật ký theo thuộc tính: mỗi lần đặt ghi (đối tượng, mã thuộc tính, giá trị cũ, giá trị mới); đặt nhiều lần trong một transaction gộp thành một mục.
  3. Mỗi commit tăng số phiên bản của bản vẽ; mỗi đối tượng mang phiên bản sửa gần nhất (khóa cache cho RGN-003).
  4. Ghi từ luồng không sở hữu → `WrongThread`; ghi khi cổng đóng → lỗi trả về trước khi đổi bất cứ dữ liệu nào.
  5. Kiểm bất biến khi commit (owner tồn tại, tham chiếu trỏ đúng loại theo schema) tốn ≤ 5% thời gian commit ở bản phát hành **[đề xuất]**.
  6. Một transaction đặt một thuộc tính rồi commit ≤ 2 µs p99, không tính phát sự kiện; tạo 1 triệu LINE trong một transaction ≤ 300 ms **[đề xuất]**; 0 cấp phát heap mỗi thao tác ở trạng thái ổn định.
  7. Đạt DoD-C, DoD-L.

### DOC-104 · Định dạng nhật ký thao tác theo handle, tuần tự hóa được
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-103, NT-014
- **Đầu vào:** 05 §18 (cộng tác dựa trên nhật ký thao tác theo handle, undo journal là nền), §5.2; CRC32 của NT-014.
- **Đầu ra:**
  - `core/doc/journal/journal_codec.{h,cpp}`: bản ghi tạo, sửa thuộc tính, xóa, khôi phục, đổi owner, mục dictionary, biến header, ranh giới nhóm có nhãn;
  - `core/doc/journal/replay.{h,cpp}`: áp và đảo bản ghi;
  - `core/doc/docs/journal-format.md`; `core/doc/fuzz/journal_fuzz.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Bản ghi định danh bằng handle và tên lớp/tên thuộc tính (bảng tên ở đầu mỗi khối), không dùng `ObjectId` hay địa chỉ. Phát lại trên bản nạp lại của cùng bản vẽ gốc cho hàm băm chuẩn tắc trùng 100%.
  2. Cùng chuỗi thao tác cho cùng chuỗi byte trên 3 hệ điều hành.
  3. Mọi bản ghi đảo được: áp rồi đảo trả về trạng thái gốc trên 10⁶ chuỗi ngẫu nhiên.
  4. Mỗi khối có phiên bản định dạng và CRC32. Tệp hỏng, cắt cụt, sai phiên bản → lỗi rõ, không crash; fuzz ≥ 1 giờ.
  5. Bản ghi sửa một số thực ≤ 24 byte; mã hóa 1 triệu bản ghi ≤ 50 ms một luồng **[đề xuất]**.
  6. Đạt DoD-C.
- **Ngoài phạm vi:** gộp thay đổi, giải xung đột, truyền qua mạng (cộng tác sau v1.0, 05 §18).

### DOC-105 · Sự kiện thay đổi gom lô cho palette và regen
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-103, NT-005
- **Đầu vào:** README §5.5 DOC-007 (sự kiện gom theo lô); README DoD-L (palette ≤ 16 ms phần đồng bộ); 05 §5.3 (regen entity mới, cập nhật batch GPU cục bộ); API đưa kết quả về luồng UI của NT-005.
- **Đầu ra:**
  - `core/doc/events/change_batch.{h,cpp}`: lô gồm thêm/sửa/xóa/khôi phục, mặt nạ thuộc tính đã đổi, đổi bảng ký hiệu, đổi biến header, đổi trạng thái phiên (DOC-112);
  - `core/doc/events/subscription.{h,cpp}`: đăng ký có bộ lọc theo loại và theo tập ID, mã đăng ký để hủy;
  - test, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Chỉ phát sau commit của transaction ngoài cùng, trên luồng ghi; transaction bị abort không phát gì.
  2. Gộp: N lần sửa cùng đối tượng trong một lô thành một mục với mặt nạ là hợp các thuộc tính; thêm rồi xóa trong cùng lô thì không phát. Property test: tập sự kiện bằng đúng diff giữa trạng thái trước và sau.
  3. Gom theo khung: nhiều commit nhỏ liên tiếp (vòng `entmod` của LISP) gộp thành tối đa 1 lô mỗi 16 ms **[đề xuất]**; người nhận có thể yêu cầu xả lô ngay (regen trước khung hình).
  4. Thứ tự mục trong lô tất định (theo handle).
  5. Dựng lô 100.000 đối tượng ≤ 5 ms; giao lô cho mỗi người nhận ≤ 50 µs ngoài phần xử lý của họ **[đề xuất]**; bộ nhớ lô được trả lại sau khi giao.
  6. Đạt DoD-C, DoD-L.

### DOC-005 · Xdata, xrecord, extension dictionary, reactor lưu trong bản vẽ
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-103
- **Đầu vào:** DXF Reference (mã nhóm xdata 1000–1071, XRECORD, extension dictionary, nhóm `{ACAD_REACTORS`); 05 §6.1, §16.
- **Đầu ra:**
  - `core/doc/xdata/`: phân tích blob xdata thành mục có kiểu theo APPID; không sửa thì ghi lại đúng byte cũ;
  - `core/doc/xrecord/`; `core/doc/extdict/` (tạo lười, tra theo tên);
  - `core/doc/reactor/persistent.{h,cpp}`: danh sách reactor bền của đối tượng;
  - thao tác sửa qua DOC-103; `core/doc/fuzz/xdata_fuzz.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Đọc rồi ghi lại không sửa: byte xdata và xrecord trùng 100% trên bộ tệp thử của module.
  2. Giới hạn như AutoCAD: tổng xdata một đối tượng ≤ 16 KB, chuỗi 1000 ≤ 255 byte, khối 1004 ≤ 127 byte. Vượt → `LimitExceeded`, không ghi dở.
  3. Gắn xdata của APPID chưa đăng ký → tự đăng ký trong cùng transaction. Xdata trỏ APPID không tồn tại (tệp hỏng) được báo cho DOC-015.
  4. Ngoặc 1002 `{` `}` lồng sai, kiểu không hợp lệ → lỗi kèm vị trí; fuzz ≥ 1 giờ không lỗi.
  5. Tra xdata theo APPID ≤ 200 ns khi đối tượng có 20 APPID **[đề xuất]**.
  6. Đạt DoD-C.

### DOC-106 · Hook trước commit và phát reactor bền
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-105, DOC-005
- **Đầu vào:** 05 §12 (reactor); 03 §5 (hatch liên kết), §8 (`DIMASSOC`); phạm vi RGN-014 (sửa kích thước thì sinh lại block); danh sách reactor bền của DOC-005.
- **Đầu ra:**
  - `core/doc/tx/commit_hooks.{h,cpp}`: đăng ký hook theo loại đối tượng và loại thay đổi; hook được ghi thêm trong cùng transaction;
  - `core/doc/reactor/dispatch.{h,cpp}`: khi đối tượng bị sửa, xóa, sao chép thì gọi bộ xử lý theo lớp của đối tượng phản ứng;
  - test với bộ xử lý giả.
- **Tiêu chí nghiệm thu:**
  1. Hook chạy trước commit theo thứ tự tất định (ưu tiên lúc đăng ký, rồi handle). Thay đổi do hook nằm trong cùng nhóm undo và cùng lô sự kiện.
  2. Lan truyền đến điểm dừng tối đa 8 vòng **[đề xuất]**; vượt → abort với `ReactorLoop`, bản vẽ không đổi.
  3. Reactor bền: B nằm trong danh sách reactor của A; sửa A thì bộ xử lý của lớp B nhận (B, A, loại thay đổi); xóa A thì B nhận thông báo xóa. Ca "đối tượng đi theo" đúng sau 10⁵ thao tác ngẫu nhiên.
  4. Lớp không có bộ xử lý (đối tượng của plugin chưa nạp) → bỏ qua, danh sách reactor giữ nguyên.
  5. Khi không có hook nào cho loại đối tượng, chi phí thêm ≤ 1% thời gian commit **[đề xuất]**.
  6. Đạt DoD-C.

### DOC-007 · API thay đổi: mọi sửa đổi qua transaction, nhật ký theo thuộc tính, sự kiện gom theo lô
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-104, DOC-105, DOC-106
- **Đầu vào:** README §5.5 DOC-007, DoD-C mục 8; 05 §5.3, §12, §18; API của DOC-103…DOC-106.
- **Đầu ra:** `core/doc/api/document_write.h` (mặt công khai duy nhất để sửa bản vẽ); `core/doc/docs/transactions.md` (hướng dẫn cho INT, CMD, LSP, SDK); `core/doc/tests/c_abi_smoke.c` (bọc C thử, chỉ trong test); test tích hợp.
- **Tiêu chí nghiệm thu:**
  1. Không có đường tắt: hàm ghi của kho (DOC-101, DOC-002) chỉ nhìn thấy bên trong `core/doc`; test biên dịch âm chứng minh module khác không gọi được.
  2. Kịch bản đầu-cuối (tạo, sửa, xóa, hook, abort lồng, sự kiện) cho nhật ký, sự kiện và phiên bản đúng đặc tả `transactions.md`; đạt 100% ca.
  3. Bọc C thử gọi được begin/commit/abort, đọc/ghi theo mã thuộc tính, đăng ký sự kiện chỉ bằng kiểu POD, mã lỗi enum, callback `void*`; biên dịch như C11 (chứng minh SDK-001 bọc được).
  4. Phần `core/doc` của một lệnh cơ bản (sửa ≤ 100 đối tượng) ≤ 2 ms, nằm trong ngân sách 20 ms của 05 §1.1 **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.

## 3. Snapshot và đọc song song

### DOC-109 · Copy-on-write theo trang và dựng snapshot
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-101, DOC-103
- **Đầu vào:** 05 §5.4, §9.2 (ghi một luồng, đọc song song trên ảnh chụp copy-on-write, không khóa trên đường nóng); trang đếm tham chiếu của DOC-101; commit và số phiên bản của DOC-103.
- **Đầu ra:**
  - `core/doc/snapshot/cow_page.{h,cpp}`: sao trang khi ghi lần đầu sau snapshot; bảng đối tượng và bảng handle cũng copy-on-write;
  - `core/doc/snapshot/snapshot.{h,cpp}`: `Document::snapshot()` trả `Snapshot` bất biến, đếm tham chiếu; thu hồi trang cũ theo epoch;
  - test, `core/doc/bench/snapshot_cow.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Tạo snapshot không sao dữ liệu: ≤ 20 µs với bản vẽ 10 triệu đối tượng **[đề xuất]**; luồng ghi không chờ khóa nào.
  2. Sau snapshot, sửa k đối tượng chỉ sao các trang chứa chúng: bộ nhớ thêm ≤ số trang bị chạm × 16 KB + 4 KB, đo bằng bộ đếm cấp phát NT-004.
  3. Blob dài (đỉnh polyline, chuỗi, vòng hatch) dùng chung giữa các snapshot, không sao.
  4. Trang không còn snapshot nào giữ được trả về pool ở commit kế tiếp; giữ 100 snapshot cùng lúc rồi thả không rò rỉ (ASan).
  5. Đọc qua snapshot (tra ID, đọc cột) chậm hơn đọc trực tiếp ≤ 10% **[đề xuất]**.
  6. Đạt DoD-C, DoD-H.

### DOC-110 · Chỉ mục không gian của bản vẽ, cập nhật gia tăng theo lô
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-109, DOC-105, GEO-017, GEO-005, GEO-006
- **Đầu vào:** README §4.2 (BVH: GEO-017 là cấu trúc, DOC-009 + INT-009 là chỉ mục trên bản vẽ, RND-010 là culling); hộp bao đường cong của GEO-005, GEO-006; lô sự kiện DOC-105.
- **Đầu ra:** `core/doc/spatial/spatial_index.{h,cpp}`: một BVH cho mỗi block record có entity (Model, từng layout, định nghĩa block), gắn với snapshot; sổ đăng ký hàm hộp bao theo loại (RGN đăng ký cho text, kích thước, hatch); test, `core/doc/bench/spatial_index.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Truy vấn hộp, điểm + bán kính, đoạn thẳng trả đúng tập so với quét vét cạn trên 10⁵ ca ngẫu nhiên tái lập bằng seed.
  2. Truy vấn trên snapshot cũ cho kết quả của phiên bản cũ, kể cả khi luồng ghi đang cập nhật chỉ mục.
  3. Dựng chỉ mục 1 triệu entity ≤ 150 ms với 8 luồng **[đề xuất]**, hiệu suất ≥ 70% đến 16 luồng (05 §1.1); cập nhật lô 1.000 đối tượng ≤ 2 ms **[đề xuất]**.
  4. Loại chưa có hàm hộp bao vào danh sách "luôn kiểm", không bao giờ bị bỏ sót khi chọn; có bộ đếm và cảnh báo log.
  5. Đạt DoD-C, DoD-H, DoD-L.

### DOC-009 · Snapshot bất biến copy-on-write cho các luồng đọc song song
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-109, DOC-110, QA-009
- **Đầu vào:** README §5.5 DOC-009, DoD-U mục 5 (UI chỉ đọc bản vẽ qua snapshot); 05 §5.4, §9.2; API DOC-109, DOC-110; bộ sinh ca và thu nhỏ ca lỗi QA-009.
- **Đầu ra:**
  - `core/doc/api/document_read.h`: mặt đọc công khai (duyệt theo loại, block record, layer; đọc theo mã thuộc tính; truy vấn không gian);
  - `core/doc/tests/snapshot_isolation_stress.cpp`: 1 luồng ghi chạy transaction ngẫu nhiên, N luồng đọc kiểm bất biến; job TSan đêm;
  - mở rộng `c_abi_smoke.c`; `core/doc/docs/snapshot.md`.
- **Tiêu chí nghiệm thu:**
  1. Cách ly: mọi snapshot bằng đúng một phiên bản đã commit (hàm băm chuẩn tắc trùng hàm băm ghi lúc commit); không thấy trạng thái giữa transaction hay transaction đã abort; owner hai chiều và tham chiếu không treo đúng trên 100% snapshot.
  2. Stress 1 ghi + 16 đọc: 60 giây ở PR, 10 phút mỗi đêm, TSan sạch, lỗi in seed để tái lập. Kiểm thử đột biến: tắt copy-on-write của một cột thì test bắt được trong ≤ 60 giây.
  3. Chi phí tỉ lệ phần đã đổi: sửa 1 đối tượng trong bản vẽ 10 triệu đối tượng, snapshot mới ≤ 50 µs và ≤ 64 KB bộ nhớ thêm **[đề xuất]**; đường đọc không chờ mutex (đếm bằng Tracy, NT-004).
  4. Duyệt 1 triệu entity của Model qua snapshot ≤ 5 ms một luồng **[đề xuất]**; `parallel_for` trên snapshot đạt hiệu suất ≥ 70% đến 16 luồng.
  5. Bọc C thử đọc đối tượng, thuộc tính, kết quả truy vấn không gian chỉ bằng kiểu POD (SDK-001 bọc được).
  6. Đạt DoD-C, DoD-H, DoD-L.

## 4. Nạp dần và bộ nhớ

### DOC-016 · Nạp lười và nạp dần: đối tượng chưa giải mã, xem trong khi nạp
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-102, DOC-009
- **Màn hình:** UI-B13
- **Đầu vào:** 05 §5.3 (khung hình đầu trong lúc phần còn lại tiếp tục nạp), §6.2 mục 3 (nạp lười); 07.1 UI-B13 (xem, pan, zoom khi đang nạp; lệnh sửa chờ nạp xong); cổng ghi `Loading` của DOC-103.
- **Đầu ra:**
  - `core/doc/load/lazy_object.{h,cpp}`: trạng thái `Pending`/`Decoded`/`Failed` từng đối tượng, giao diện giải mã do codec đăng ký lúc chạy;
  - `core/doc/load/progressive.{h,cpp}`: công bố theo lô, tiến độ, vùng ưu tiên do renderer gợi ý, hủy;
  - test với bộ giải mã giả.
- **Tiêu chí nghiệm thu:**
  1. Nhiều luồng đọc cùng chạm một đối tượng `Pending` → giải mã đúng một lần, các luồng khác nhận cùng kết quả; TSan sạch.
  2. Snapshot lấy giữa lúc nạp chỉ thấy các lô đã công bố; lô đầu (header, bảng ký hiệu, layout hiện hành) đủ để vẽ khung hình đầu.
  3. Vùng ưu tiên: bộ giải mã giả 1 µs/đối tượng, 1 triệu đối tượng, khung nhìn chứa 10% → phần trong khung nhìn công bố xong trong ≤ 20% thời gian nạp toàn bộ **[đề xuất]**.
  4. Hủy nạp dừng trong ≤ 50 ms, trả bộ nhớ; lỗi giải mã một đối tượng → `Failed` + nhật ký, phần còn lại nạp tiếp.
  5. Cổng ghi đóng (`Loading`) đến khi nạp xong; tiến độ phát qua sự kiện cho thanh trạng thái ("Đang nạp 63% · 1,2 triệu đối tượng").
  6. Đạt DoD-C, DoD-H, DoD-L.

### DOC-113 · Đo bộ nhớ của mô hình bản vẽ, ngân sách cho DWG 100 MB
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-016, DWG-010, QA-001, QA-002
- **Đầu vào:** 05 §1.1 (≤ 1,2 GB cho DWG 100 MB), README DWG-031; §0 (phần `core/doc` ≤ 600 MB **[đề xuất]**); tệp 20 MB và 100 MB của BENCH-STD; bộ chạy kho QA-002 (đo bộ nhớ).
- **Đầu ra:** `core/doc/mem/accounting.{h,cpp}`: bộ đếm byte theo hạng mục (cột, blob, chuỗi intern, bảng handle, chỉ mục không gian, trang snapshot còn giữ, undo, tài liệu xref); báo cáo JSON cho QA-002, `trion-console`, TRIONBENCH; tín hiệu áp lực bộ nhớ; `core/doc/bench/memory_budget.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Tổng bộ đếm khớp số byte thật của arena và pool trong `core/doc` (NT-007), sai lệch ≤ 2%.
  2. BENCH-STD nạp đủ: phần `core/doc` ≤ 600 MB với tệp 100 MB, ≤ 150 MB với tệp 20 MB **[đề xuất]**; CI chặn khi vượt hoặc tăng > 5% (NT-003).
  3. Một LINE tốn trung bình ≤ 128 byte, gồm header chung, hình học, mục bảng handle, chỉ mục không gian **[đề xuất]**.
  4. Đọc báo cáo ≤ 1 ms, không khóa luồng ghi.
  5. Vượt ngưỡng cấu hình → phát tín hiệu áp lực một lần mỗi lần vượt; người nhận (undo DOC-108, kho xref DOC-114, thả snapshot cũ) đăng ký được.
  6. Đạt DoD-C.

## 5. Không gian, layout, block, thứ tự vẽ, nhóm

### DOC-011 · Model space, layout, paper space, entity VIEWPORT, không gian hiện hành
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-004, DOC-010, DOC-103
- **Màn hình:** UI-A10
- **Đầu vào:** README §5.5 DOC-011; 07.1 UI-A10; DXF Reference: LAYOUT, VIEWPORT, biến `TILEMODE`, `CTAB`, `CVPORT`; schema của DWG-011, DWG-013.
- **Đầu ra:**
  - `core/doc/space/layouts.{h,cpp}`: cặp LAYOUT ↔ block record `*Model_Space`/`*Paper_Space…`, thứ tự tab, không gian hiện hành; tạo, sao chép, đổi tên, xóa, sắp xếp layout qua transaction;
  - `core/doc/space/viewport.{h,cpp}`: view có kiểu của VIEWPORT (tâm, kích thước, tâm view, chiều cao view, xoay, khóa, bật/tắt, danh sách VP Freeze, biên clip), viewport tổng thể của layout; test.
- **Tiêu chí nghiệm thu:**
  1. Kho QA-001 bộ 1.000 tệp: tên, thứ tự tab, block record, số viewport của mọi layout khớp `dwgdump` (DWG-030) 100%; viewport tổng thể nhận đúng.
  2. Đổi `TILEMODE`, `CTAB`, `CVPORT` đi qua transaction, đổi đúng biến header; abort trả đúng trạng thái cũ (hàm băm chuẩn tắc).
  3. Từ chối: xóa layout cuối cùng, tên trùng (không phân biệt hoa thường), đổi tên hoặc xóa Model.
  4. Tra viewport chứa một điểm giấy ≤ 10 µs với 100 viewport **[đề xuất]**.
  5. Đạt DoD-C.

### DOC-012 · Block: định nghĩa, tham chiếu, duyệt lồng nhau, ATTDEF/ATTRIB, MINSERT, block vô danh, `*U` của dynamic block
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DOC-004, DOC-005, DOC-103
- **Đầu vào:** README §5.5 DOC-012, RGN-018; DXF Reference: BLOCK_RECORD, INSERT, MINSERT, ATTDEF, ATTRIB, SEQEND, block vô danh `*U`, `*D`, `*T`; mẫu block lồng và dynamic block của QA-001.
- **Đầu ra:**
  - `core/doc/block/block_def.{h,cpp}`: định nghĩa (điểm gốc, cờ vô danh/xref/layout, đơn vị chèn, cho phép tách), chỉ mục ngược block → INSERT cập nhật theo lô sự kiện, cấp tên vô danh mới;
  - `core/doc/block/insert.{h,cpp}`: ma trận chèn, ATTRIB sở hữu, lưới MINSERT, liên kết `*U` → dynamic block gốc;
  - `core/doc/block/nested_walk.{h,cpp}`: duyệt lồng có đường dẫn (chuỗi ID), ma trận tích lũy, ngữ cảnh ByBlock; test, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Duyệt lồng trên cảnh "block lồng" của BENCH-STD khớp 100% bản duyệt tham chiếu đơn giản; 1 triệu lá ≤ 40 ms với 8 luồng **[đề xuất]**, 0 cấp phát mỗi lá.
  2. Block chứa chính nó (trực tiếp hoặc vòng) → lỗi `CyclicBlock`, báo DOC-015; không lặp vô hạn, không tràn stack.
  3. Chỉ mục ngược đúng sau 10⁵ thao tác ngẫu nhiên (QA-009) so với quét vét cạn; đếm tham chiếu một block ≤ 1 µs.
  4. Tên vô danh mới không trùng, đánh số tất định; MINSERT sinh đúng số hàng × số cột ma trận, kể cả khoảng cách âm; `*U` trả đúng block gốc trên 100% mẫu QA-001.
  5. Bọc C thử duyệt lồng chỉ bằng kiểu POD (SDK-001 bọc được).
  6. Đạt DoD-C, DoD-H.

### DOC-020 · Thứ tự vẽ (SORTENTS) và nhóm (GROUP)
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-005, DOC-106
- **Đầu vào:** README §5.5 DOC-020, RGN-027; 03 §6 (DRAWORDER, TEXTTOFRONT, HATCHTOBACK); DXF Reference: SORTENTSTABLE, GROUP, dictionary `ACAD_GROUP`; reactor bền DOC-106.
- **Đầu ra:**
  - `core/doc/order/draw_order.{h,cpp}`: bảng SORTENTS theo block record (tạo lười trong extension dictionary), duyệt theo thứ tự vẽ, đưa tập lên trên/xuống dưới/trên/dưới một đối tượng;
  - `core/doc/group/group.{h,cpp}`: nhóm có tên và vô danh `*A`, cờ chọn được, thành viên có thứ tự, rút thành viên bị xóa qua reactor; test, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Thứ tự duyệt khớp DXF xuất bởi ODA File Converter (QA-004) trên mẫu có SORTENTSTABLE của QA-001; entity không có trong bảng dùng chính handle làm khóa sắp xếp.
  2. Đổi thứ tự 100.000 đối tượng ≤ 20 ms, chỉ ghi các mục đổi; duyệt 1 triệu entity theo thứ tự vẽ ≤ 10 ms một luồng **[đề xuất]**.
  3. Thứ tự tương đối bên trong tập được đổi giữ nguyên (ổn định), kể cả tập không liên tục.
  4. Xóa thành viên → nhóm rút thành viên trong cùng transaction; nhóm rỗng giữ lại cho PURGE; abort khôi phục cả thành viên.
  5. Đạt DoD-C, DoD-H.

## 6. Thuộc tính chung và chú thích theo tỉ lệ

### DOC-017 · Metadata thuộc tính: đọc/ghi chung theo mã thuộc tính cho Properties, `entget`/`entmod`, so sánh
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DOC-003, DOC-009, DOC-103, DWG-002, DWG-166
- **Màn hình:** UI-C01
- **Đầu vào:** README §4.2 (DOC-017 là nguồn duy nhất), §5.5; 05 §6.2 mục 1; metadata thuộc tính do DWG-002 sinh; DXF Reference (mã nhóm); DoD-L dòng "Palette cập nhật khi vùng chọn đổi".
- **Đầu ra:**
  - `core/doc/props/property_meta.{h,cpp}`: danh mục theo loại (mã, khóa tên vi/en, kiểu giá trị, loại đơn vị cho RGN-013, chỉ đọc/sửa được, nhóm hiển thị, mã nhóm DXF);
  - `core/doc/props/property_access.{h,cpp}`: đọc/ghi theo mã, đối tượng ↔ danh sách mã nhóm DXF, so sánh hai đối tượng theo mã qua `Tolerance`;
  - `core/doc/props/common_props.{h,cpp}`: thuộc tính chung của vùng chọn, đánh dấu `*VARIES*`, chạy song song; test, benchmark.
- **Tiêu chí nghiệm thu:**
  1. 100% trường có tên thuộc tính trong schema có mục metadata; build lỗi nếu thiếu khóa dịch.
  2. Đối tượng → danh sách DXF → đối tượng giữ nguyên hàm băm chuẩn tắc cho mọi đối tượng của kho QA-001 bộ 1.000 tệp.
  3. Thuộc tính chung của 10.000 đối tượng trộn loại ≤ 16 ms (DoD-L); 1 triệu đối tượng ≤ 120 ms với 8 luồng **[đề xuất]**, hiệu suất ≥ 60%; kết quả không phụ thuộc số luồng.
  4. Ghi một thuộc tính cho cả tập trong một transaction, trả số đối tượng đổi, bỏ qua và lý do (chỉ đọc, ngoài miền giá trị).
  5. Bọc C thử đọc/ghi theo mã chỉ bằng kiểu POD (SDK-001 bọc được).
  6. Đạt DoD-C, DoD-H, DoD-L.

### DOC-021 · Mô hình chú thích theo tỉ lệ (object context data): đọc và chọn biểu diễn
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-005, DOC-011, DOC-017
- **Đầu vào:** README §5.5 DOC-021, RGN-026; DXF Reference: SCALE, dictionary `ACAD_SCALELIST`, object context data của TEXT, MTEXT, ATTRIB, kích thước, MLEADER, hatch, block; biến `CANNOSCALE`, `ANNOALLVISIBLE`.
- **Đầu ra:**
  - `core/doc/annot/scale_list.{h,cpp}`: danh sách tỉ lệ, tra theo tên và tỉ số;
  - `core/doc/annot/context_data.{h,cpp}`: tỉ lệ đối tượng hỗ trợ, biểu diễn theo tỉ lệ (vị trí, chiều cao, xoay, căn lề, điểm kích thước ghi đè), biểu diễn mặc định, chọn biểu diễn theo tỉ lệ của Model hoặc viewport; test.
- **Tiêu chí nghiệm thu:**
  1. Mẫu annotative của QA-001: 100% đối tượng đọc đủ biểu diễn, khớp DXF xuất bởi ODA File Converter (QA-004).
  2. Chọn biểu diễn theo tỉ lệ hiện hành và `ANNOALLVISIBLE` khớp bảng ca vàng 100%.
  3. Tra biểu diễn ≤ 100 ns mỗi đối tượng, 0 cấp phát, gọi an toàn từ regen song song trên snapshot.
  4. Dữ liệu context hỏng (tỉ lệ treo, thiếu biểu diễn mặc định) → dùng thuộc tính gốc, báo DOC-015.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** thêm/bớt tỉ lệ, đồng bộ vị trí (DOC-121, 1.0).

## 7. Phiên nhiều bản vẽ, tài liệu chỉ đọc, xref

### DOC-119 · Tài liệu chỉ đọc và trạng thái chỉ trong phiên (Viewer)
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-009, DOC-011, DOC-103
- **Màn hình:** UI-B01, UI-M03, UI-M04
- **Đầu vào:** 07.1 §2 (Viewer bật/tắt layer tạm thời, không lưu; Pro lưu được), UI-B01 (Mở chỉ đọc), UI-A09 (biểu tượng chỉ đọc); cổng ghi `ReadOnly` của DOC-103.
- **Đầu ra:**
  - `core/doc/session/access_mode.{h,cpp}`: `Editable`; `ReadOnlyFile` (sửa trong bộ nhớ được, cấm lưu đè tệp gốc, phải SAVEAS, như AutoCAD); `Immutable` (Viewer, tài liệu xref: cổng ghi đóng với `ReadOnly`);
  - `core/doc/session/session_overlay.{h,cpp}`: ghi đè chỉ trong phiên theo handle (layer On/Off/Freeze, layout và viewport hiện hành), có số phiên bản riêng gắn vào snapshot; không vào nhật ký, không undo, không lưu; test.
- **Tiêu chí nghiệm thu:**
  1. `Immutable`: 100% hàm của `document_write.h` trả `ReadOnly` trước khi đổi dữ liệu (test sinh từ danh sách hàm); 10⁴ thao tác xem ngẫu nhiên không đổi hàm băm chuẩn tắc.
  2. Overlay: bật/tắt layer tạm thời có hiệu lực ở snapshot kế tiếp ≤ 1 ms với 10.000 layer **[đề xuất]**; không đổi hàm băm, không bật `DBMOD`, mất khi đóng.
  3. `ReadOnlyFile`: lưu đè đường dẫn gốc bị từ chối với lý do riêng; SAVEAS sang đường dẫn khác được và chuyển tài liệu về `Editable`.
  4. Đổi overlay phát sự kiện riêng, regen gia tăng (RGN-003) phân biệt được với đổi dữ liệu.
  5. Đạt DoD-C, DoD-L.

### DOC-112 · Trạng thái tài liệu và sự kiện đổi trạng thái phiên
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** DOC-016, DOC-105, DOC-119
- **Màn hình:** UI-A09
- **Đầu vào:** 07.1 UI-A09 (chấm chưa lưu, biểu tượng chỉ đọc), UI-B13 (tiến độ nạp); DXF Reference: biến `DBMOD`, `DWGTITLED`, `DWGNAME`, `DWGPREFIX`; lô sự kiện DOC-105.
- **Đầu ra:** `core/doc/session/doc_state.{h,cpp}` (đường dẫn, tên hiển thị, `DWGTITLED`, các bit `DBMOD`, chế độ truy cập, trạng thái nạp/lưu, phiên bản đã lưu); `core/doc/session/session_events.{h,cpp}` (mở, đóng, kích hoạt, đổi cờ sửa đổi, đổi chế độ, bắt đầu/xong lưu, gom vào lô DOC-105); test.
- **Tiêu chí nghiệm thu:**
  1. `DBMOD` bật đúng bit theo loại thay đổi (đối tượng, biến, cửa sổ, view, field) trên bảng ca vàng; về 0 sau khi lưu.
  2. Bản vẽ trở về đúng phiên bản đã lưu (so số phiên bản, kể cả qua undo về sau) → tắt chấm chưa lưu, `DBMOD` = 0 **[đề xuất]**.
  3. Mỗi lần đổi trạng thái phát đúng một sự kiện, nhiều lần trong một khung gộp lại; người nhận trên luồng UI nhận ≤ 1 khung sau commit.
  4. Kịch bản 20 tài liệu mở/đóng/kích hoạt ngẫu nhiên: trình tự sự kiện khớp đặc tả, không có sự kiện cho tài liệu đã đóng.
  5. Bọc C thử đăng ký sự kiện phiên bằng callback `void*` (SDK-001, reactor tài liệu của LSP-016).
  6. Đạt DoD-C.

### DOC-018 · Phiên nhiều bản vẽ: danh sách bản vẽ mở, bản vẽ hiện hành, trạng thái riêng từng bản vẽ
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-112, DOC-113
- **Màn hình:** UI-A09
- **Đầu vào:** README §5.5 DOC-018; 05 §5.4 (luồng UI sở hữu document); 07.1 UI-A09, UI-B01 (chọn nhiều tệp mở nhiều tab); API DOC-112, DOC-113.
- **Đầu ra:** `core/doc/session/session.{h,cpp}`: danh sách tài liệu theo thứ tự tab, tài liệu hiện hành, mở/đóng/kích hoạt, chỗ gắn dịch vụ theo tài liệu (undo, chọn, namespace LISP, view do module khác đăng ký), giải phóng ở luồng nền khi đóng; `core/doc/docs/session.md`; mở rộng `c_abi_smoke.c`.
- **Tiêu chí nghiệm thu:**
  1. Cách ly: transaction, sự kiện, sysvar phạm vi bản vẽ, dịch vụ của tài liệu A không chạm tài liệu B; 20 tài liệu thao tác xen kẽ cho hàm băm từng tài liệu trùng khi chạy riêng.
  2. Đổi tài liệu hiện hành ≤ 1 ms phần `core/doc` **[đề xuất]**, không chờ việc nền của tài liệu khác.
  3. Đóng tài liệu: việc nền của nó hủy ≤ 50 ms; bộ nhớ (DOC-113) về mức trước khi mở ± 1% khi snapshot cuối được thả.
  4. Mở song song nhiều tệp: tab theo thứ tự người dùng chọn, không theo thứ tự nạp xong.
  5. Đạt DoD-C, DoD-H.

### DOC-114 · Kho chung tài liệu xref đã nạp
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-018, DOC-113, DOC-119, NT-005, NT-011
- **Đầu vào:** 05 §9.2 (nạp nhiều xref cùng lúc), §9.4 (đọc song song, prefetch xref ở nền); NT-011 (đường dẫn tìm kiếm, theo dõi thay đổi tệp); tín hiệu áp lực bộ nhớ DOC-113; chế độ `Immutable` của DOC-119.
- **Đầu ra:** `core/doc/xref/xref_store.{h,cpp}`: khóa theo đường dẫn chuẩn hóa + thời điểm sửa + kích thước; một tài liệu `Immutable` dùng chung cho mọi bản vẽ chủ và mọi cấp lồng, đếm tham chiếu; nạp song song qua bộ nạp đăng ký lúc chạy; sự kiện "tệp đã đổi, cần nạp lại"; thả theo LRU khi có áp lực bộ nhớ; test với bộ nạp giả.
- **Tiêu chí nghiệm thu:**
  1. Một tệp được 3 bản vẽ chủ và 2 cấp lồng tham chiếu → nạp đúng 1 lần, bộ nhớ tính 1 lần (DOC-113).
  2. 50 xref, bộ nạp giả 20 ms/tệp: xong ≤ 200 ms với 8 luồng **[đề xuất]**; thứ tự kết quả không phụ thuộc thứ tự nạp xong.
  3. Tệp đổi trên đĩa → sự kiện "cần nạp lại" ≤ 1 s; nạp lại tạo phiên bản mới, bản cũ sống đến khi snapshot cuối được thả.
  4. Áp lực bộ nhớ chỉ thả tài liệu không còn ai tham chiếu; tài liệu đang dùng không bao giờ bị thả.
  5. Đạt DoD-C, DoD-H.

### DOC-013 · Mô hình xref: block record ngoài, trạng thái phân giải, lồng nhau, attach/overlay
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DOC-012, DOC-114, NT-011
- **Màn hình:** UI-B06
- **Đầu vào:** README §5.5 DOC-013; 07.1 UI-B06 (thiếu xref, trạng thái); DXF Reference: cờ xref/overlay của BLOCK_RECORD, đường dẫn xref, ký hiệu phụ thuộc `xref|tên`, biến `VISRETAIN`; kho DOC-114.
- **Đầu ra:**
  - `core/doc/xref/xref_model.{h,cpp}`: đường dẫn lưu và đường dẫn tìm được, attach/overlay, nạp/bỏ nạp, trạng thái (`Loaded`, `Unloaded`, `NotFound`, `Unresolved`, `Unreferenced`, `Orphaned`), cây lồng, phát hiện vòng;
  - `core/doc/xref/dependent_symbols.{h,cpp}`: ký hiệu phụ thuộc của bản vẽ chủ ↔ bản ghi trong tài liệu xref, áp `VISRETAIN`; test.
- **Tiêu chí nghiệm thu:**
  1. Overlay lồng trong xref khác không được nạp, attach lồng nạp đủ cấp; khớp bảng ca vàng 100%.
  2. Vòng A → B → A: phát hiện, dừng ở lần lặp, trạng thái `Unresolved` kèm lý do.
  3. Đường dẫn tìm được và trạng thái khớp ca vàng cho đường dẫn tuyệt đối, tương đối, chỉ tên tệp (support path của NT-011).
  4. Layer `xref|tên` giữ ghi đè của bản vẽ chủ khi `VISRETAIN` = 1, lấy lại từ xref khi = 0; khớp mẫu QA-001.
  5. Bản vẽ dựng sẵn có 50 xref: khung hình đầu không chờ xref; mỗi xref nạp xong phát sự kiện regen riêng.
  6. Bọc C thử liệt kê xref và trạng thái bằng kiểu POD (SDK-001). Đạt DoD-C, DoD-H.

## 8. Kiểm tra toàn vẹn và dữ liệu chưa hiểu

### DOC-015 · Kiểm tra toàn vẹn và sửa lỗi (owner, handle, tham chiếu treo) cho AUDIT/RECOVER
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DOC-011, DOC-013, DOC-020, DOC-021, QA-001
- **Màn hình:** UI-B10
- **Đầu vào:** README §5.5 DOC-015; 03 §2 (AUDIT, RECOVER P0); 05 §6.2 mục 4, §9.2 (AUDIT song song); 07.2 UI-B10; lỗi do DOC-005, DOC-010, DOC-012, DOC-021 chuyển sang.
- **Đầu ra:**
  - `core/doc/audit/checks.{h,cpp}`: kiểm song song trên snapshot: owner hai chiều, handle trùng, `HANDSEED`, tham chiếu treo hoặc sai loại theo schema, tên bảng ký hiệu rỗng/trùng/ký tự cấm, bản ghi bắt buộc thiếu (layer `0`, linetype `CONTINUOUS`, kiểu chữ `STANDARD`), layout ↔ block record, block vòng, xref vòng, APPID thiếu;
  - `core/doc/audit/fixes.{h,cpp}`: sửa trong một transaction; `core/doc/audit/report.{h,cpp}`: nhật ký có cấu trúc (mã lỗi, handle, mô tả vi/en, đã sửa) cho UI-B10; test.
- **Tiêu chí nghiệm thu:**
  1. Bộ tệp hỏng sinh bằng đột biến có kiểm soát trên 200 tệp QA-001 (≥ 20 ca mỗi loại lỗi): phát hiện 100%; sau khi sửa, kiểm lần hai báo 0 lỗi.
  2. Không đánh lại handle hợp lệ: chỉ bản trùng nhận handle mới từ seed; `HANDSEED` nâng trên handle lớn nhất.
  3. Chế độ chỉ báo không đổi hàm băm; chế độ sửa chạy trong một transaction, abort trả nguyên trạng.
  4. BENCH-STD 100 MB ≤ 1 s với 8 luồng **[đề xuất]**, hiệu suất ≥ 60% đến 8 luồng; nội dung và thứ tự nhật ký như nhau ở mọi số luồng.
  5. Fuzz bộ kiểm trên tài liệu dựng từ byte ngẫu nhiên qua bộ nạp giả ≥ 1 giờ (QA-006).
  6. Đạt DoD-C, DoD-H.

### DOC-006 · Giữ nguyên dữ liệu chưa hiểu: khối bit thô và handle tham chiếu
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-003, DOC-015
- **Đầu vào:** README §5.5 DOC-006 (phần mô hình; phần codec là DWG-018); 05 §6.2 mục 2 (giữ nguyên khối bit, không đánh lại handle); 03 §2 (proxy của Civil 3D, plugin).
- **Đầu ra:**
  - `core/doc/opaque/opaque_object.{h,cpp}`: đối tượng chưa có schema (tên lớp DXF, số lớp, phiên bản, khối bit dữ liệu, luồng chuỗi, danh sách handle tham chiếu kèm loại hard/soft owner/pointer);
  - `core/doc/opaque/opaque_tail.{h,cpp}`: phần chưa hiểu của đối tượng đã biết; mở rộng bộ duyệt tham chiếu chung để gồm handle thô; test, fuzz.
- **Tiêu chí nghiệm thu:**
  1. Handle thô có mặt trong bộ duyệt tham chiếu chung: AUDIT (DOC-015) không báo treo sai; đối tượng thô theo owner khi owner bị xóa (hard owner).
  2. Không sửa → byte giữ nguyên qua mọi transaction; sửa trường đã biết không đụng phần đuôi thô; round-trip QA-003 trên mẫu Civil 3D/plugin của QA-001 không lệch byte (cùng DWG-018).
  3. Đối tượng thô trỏ tới đối tượng đã xóa → giữ handle, AUDIT cảnh báo, không sửa ngầm; không bao giờ đánh lại handle của đối tượng thô.
  4. Dữ liệu thô tính vào DOC-113 ở hạng mục riêng, sao vào arena của tài liệu (không giữ mmap của tệp nguồn).
  5. Fuzz cấu trúc thô (danh sách handle, kích thước) ≥ 1 giờ (QA-006). Đạt DoD-C.

## 9. Undo/redo

### DOC-108 · Kho undo và giới hạn bộ nhớ
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-104, DOC-113, NT-011, NT-014
- **Đầu vào:** 03 §6 (UNDO nhiều bước, nhật ký theo thuộc tính); README §5.5 DOC-008 (giới hạn bộ nhớ); định dạng bản ghi DOC-104; tín hiệu áp lực DOC-113; nén NT-014; ghi an toàn NT-011.
- **Đầu ra:** `core/doc/undo/undo_store.{h,cpp}`: chuỗi bước (mỗi bước là các bản ghi DOC-104 của một nhóm), con trỏ undo/redo, cắt nhánh redo khi có thay đổi mới; nén bước cũ; tràn ra tệp tạm của phiên khi vượt ngân sách; bỏ bước cũ nhất khi vượt giới hạn đĩa (kèm thông báo); `core/doc/fuzz/undo_spill_fuzz.cpp`; test, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Ngân sách mặc định min(512 MB, 1/8 RAM) trong bộ nhớ, 2 GB trên đĩa **[đề xuất]**; vượt ngân sách hoặc nhận tín hiệu áp lực → bước cũ ra đĩa ở luồng nền, luồng UI không chờ.
  2. Undo bước đã ra đĩa cho cùng hàm băm chuẩn tắc như khi còn trong bộ nhớ; tệp tràn hỏng → bước đó báo lỗi rõ, không crash, bước mới hơn vẫn undo được.
  3. Một bước đổi 1 thuộc tính ≤ 64 byte sau nén; 10.000 bước trên bản vẽ 1 triệu đối tượng dùng ≤ 3 lần dung lượng thay đổi thực **[đề xuất]**.
  4. Thêm bước ≤ 5 µs p99 ngoài thời gian commit, 0 cấp phát ở trạng thái ổn định.
  5. Fuzz đọc tệp tràn ≥ 1 giờ (QA-006). Đạt DoD-C, DoD-H.

### DOC-008 · Undo/redo: mark theo lệnh, nhóm, các tùy chọn của UNDO, giới hạn bộ nhớ
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-007, DOC-018, DOC-108
- **Đầu vào:** README §5.5 DOC-008; 03 §6 (UNDO, U, REDO, MREDO; mỗi lệnh một mark); 05 §5.3 (`undo.beginMark`/`endMark`), §18 (nhật ký theo handle là nền cho cộng tác); AutoCAD Command Reference: UNDO, biến `UNDOCTL`.
- **Đầu ra:** `core/doc/undo/undo_manager.{h,cpp}`: một bộ quản lý cho mỗi tài liệu; mark theo lệnh (API cho INT-002); BEgin/End lồng, Mark/Back, Auto, Control All/None/One, undo N bước; REDO một bước và nhiều bước (MREDO); xuất chuỗi bước thành nhật ký DOC-104; `core/doc/docs/undo.md`; mở rộng `c_abi_smoke.c`; test.
- **Tiêu chí nghiệm thu:**
  1. Bảng ca vàng cho mọi tùy chọn UNDO, `UNDOCTL` và tương tác (Back không có Mark, BEgin chưa End khi lệnh kết thúc, Control None/One cắt lịch sử) đạt 100% theo `undo.md`.
  2. 10⁶ bước undo/redo ngẫu nhiên (QA-009): mọi trạng thái trùng hàm băm chuẩn tắc đã ghi; seed handle không giảm (DOC-001).
  3. Undo lệnh thường ≤ 5 ms, lệnh sửa 100.000 đối tượng ≤ 50 ms **[đề xuất]**; mỗi lần undo phát một lô sự kiện.
  4. Nhật ký xuất ra áp lên bản sao khác của bản vẽ cho cùng hàm băm (nền cho cộng tác, 05 §18).
  5. Bọc C thử begin/end mark và undo bằng kiểu POD (SDK-001).
  6. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** lệnh UNDO, U, REDO, MREDO (CMD).

## 10. Sửa hàng loạt và biến đổi

### DOC-116 · Sửa hàng loạt song song trong một transaction
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-007, DOC-110, NT-005
- **Đầu vào:** 05 §9.2 (MOVE, SCALE 100 nghìn đối tượng; ghi vẫn một luồng); §0 tệp này (fork-join bên trong transaction **[đề xuất]**); `parallel_for` của NT-005; cập nhật lô chỉ mục DOC-110.
- **Đầu ra:** `core/doc/tx/bulk_edit.{h,cpp}`: `BulkEdit` chia tập ID thành khối theo thứ tự `ObjectId`; luồng thợ tính giá trị mới vào vùng đệm riêng và ghi slot riêng; nhật ký và lô sự kiện gộp theo thứ tự khối; hủy hợp tác; test, `core/doc/bench/bulk_edit.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Nhật ký, lô sự kiện, hàm băm sau commit trùng từng bit với bản chạy 1 luồng, ở 2/4/8/16/32 luồng.
  2. Đổi vị trí 100.000 đối tượng trộn loại (LINE, ARC, LWPOLYLINE, TEXT, INSERT), gồm nhật ký, sự kiện, chỉ mục không gian: ≤ 60 ms trên máy tầm trung 8 luồng **[đề xuất]**; hiệu suất ≥ 60% đến 8 luồng.
  3. Hủy giữa chừng hoặc một khối lỗi → cả transaction abort trong ≤ 50 ms, bản vẽ về đúng trạng thái trước.
  4. Luồng thợ không gọi được API ghi thường (test biên dịch âm), không thấy trạng thái dở của khối khác; TSan sạch.
  5. Đạt DoD-C, DoD-H, DoD-L.

### DOC-115 · Biến đổi entity (transformBy) qua transaction
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DOC-116, DOC-011, DOC-012, DOC-021, GEO-022, DWG-169
- **Đầu vào:** 03 §6 (MOVE, ROTATE, SCALE, MIRROR có `MIRRTEXT`, STRETCH, grip); 05 §9.2; GEO-022 (tỉ lệ không đều: tròn → ellipse, cung → cung ellipse; đối xứng); DXF Reference (OCS, hướng đùn, cờ ngược/lộn ngược của TEXT).
- **Đầu ra:** `core/doc/xform/transform.{h,cpp}`: `transformBy(tx, ids, M, opts)` cho mọi loại entity, tập lớn chạy qua DOC-116; `transformedCopy` khi kết quả phải đổi loại; tùy chọn `TextMirror` (`Reflect`/`KeepReadable`) và `HatchMirror` do lệnh chọn theo `MIRRTEXT`, `MIRRHATCH`; danh sách đối tượng bỏ qua kèm lý do; test theo loại, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Mọi loại trong registry có luật hoặc trả `NotSupported` rõ ràng (proxy, đối tượng thô, 3DSOLID/REGION/BODY: biến đổi đồng dạng qua DWG-169, scale không đều trả `NonUniformNotSupported`); test liệt kê tự động, không loại nào thiếu.
  2. Luật theo loại khớp bảng ca vàng 100%: TEXT/MTEXT/ATTRIB (chiều cao, xoay, căn lề); INSERT (vị trí, xoay, tỉ lệ; gương → tỉ lệ X âm; text và attribute hằng trong block luôn gương theo block, ATTRIB theo `TextMirror`); kích thước, leader (điểm định nghĩa, khối sinh lại qua hook DOC-106); hatch (biên, gốc, góc và tỉ lệ mẫu); biểu diễn annotative (DOC-021) đổi cùng.
  3. Tỉ lệ không đều: `transformBy` trả `NonUniformNotSupported` cho tròn, cung, LWPOLYLINE có cung; `transformedCopy` cho ellipse/cung ellipse theo GEO-022 với handle mới; TEXT nhận width factor = sx/sy và góc nghiêng tính lại **[đề xuất]**.
  4. Bất biến (QA-009, 10⁵ ca): biến đổi rồi biến đổi ngược, hoặc gương hai lần, trả về trạng thái trùng trong `Tolerance` (GEO-002); entity 2D trong mặt XY giữ pháp tuyến +Z sau gương, cung đảo đầu–cuối **[đề xuất]**.
  5. Một đối tượng ≤ 1 µs trung bình; 100.000 đối tượng đạt ngưỡng của DOC-116.
  6. Đạt DoD-C, DoD-H, DoD-L.
- **Ngoài phạm vi:** đọc `MIRRTEXT`/`MIRRHATCH`, chọn đối tượng, tách polyline khi tỉ lệ không đều (thuộc lệnh).

## 11. Sao chép sâu và dọn dẹp

### DOC-118 · Sao chép sâu có ánh xạ ID: đi đồ thị tham chiếu, xử lý trùng tên
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DOC-006, DOC-012, DOC-018
- **Đầu vào:** README §5.5 DOC-014; 05 §6.2 mục 2 (không đánh lại handle), §12 (khái niệm quen thuộc với lập trình viên ObjectARX); loại tham chiếu theo trường của DOC-003; handle thô DOC-006.
- **Đầu ra:** `core/doc/clone/deep_clone.{h,cpp}`: đi đồ thị theo hard owner (sao), hard pointer (sao, hoặc ánh xạ theo tên sang bản vẽ đích), soft pointer (ánh xạ nếu trong tập, nếu không giữ hoặc null theo luật); `IdMap` nguồn → đích; chính sách trùng tên `KeepExisting`, `Prefix` (`xref$0$tên`), `Rename`, `Replace`; handle mới cấp từ seed đích theo thứ tự handle nguồn; test, `core/doc/bench/deep_clone.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Sau mọi lần sao, bản vẽ đích qua AUDIT (DOC-015) 0 lỗi, không tham chiếu nào trỏ về bản vẽ nguồn; 10⁵ đồ thị ngẫu nhiên (QA-009) đúng.
  2. Tất định: cùng nguồn và đích cho cùng handle mới và hàm băm ở mọi số luồng; handle sẵn có của nguồn và đích không bao giờ bị đánh lại.
  3. Bảng ca vàng trùng tên (layer, linetype, kiểu chữ, kiểu kích thước, block lồng) theo từng chính sách đạt 100%; `Prefix` tăng `$0$`, `$1$`… khi vẫn trùng.
  4. Dữ liệu thô (DOC-006): handle trong danh sách tham chiếu được ánh xạ, byte dữ liệu không đổi.
  5. Sao 100.000 entity giữa hai bản vẽ ≤ 150 ms với 8 luồng **[đề xuất]**, hiệu suất ≥ 60% đến 8 luồng.
  6. Đạt DoD-C, DoD-H.

### DOC-117 · Phân tích tham chiếu và dọn dẹp (PURGE)
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-006, DOC-012, DOC-020, DOC-113, DWG-022
- **Màn hình:** UI-B09
- **Đầu vào:** 03 §2 (PURGE gồm regapps, đối tượng dài 0, text rỗng; bản vẽ VN phình to vì regapps); 05 §9.2 (PURGE song song); 07.2 UI-B09 (dọn được / không dọn được kèm lý do, lồng nhau, ước tính dung lượng giảm).
- **Đầu ra:** `core/doc/purge/ref_graph.{h,cpp}`: đếm tham chiếu song song trên snapshot cho bản ghi bảng ký hiệu, kiểu (multileader, bảng, hiển thị, plot style), nhóm, regapp, block; `core/doc/purge/purge_plan.{h,cpp}`: mục dọn được và không dọn được kèm lý do có cấu trúc, lặp lồng đến điểm bất động, hình học dài 0, text rỗng, dữ liệu mồ côi, ước tính byte giảm; áp kế hoạch trong một transaction; test, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Không bao giờ dọn mục còn được tham chiếu (kể cả qua xdata, dữ liệu thô, biến header, extension dictionary): 10⁴ bản vẽ ngẫu nhiên (QA-009) sau khi dọn qua AUDIT 0 lỗi.
  2. Không dọn mục chuẩn và mục đang dùng: layer `0`, `DEFPOINTS`, linetype `BYLAYER`/`BYBLOCK`/`CONTINUOUS`, kiểu `STANDARD`, regapp `ACAD`, mục hiện hành (`CLAYER`, `CELTYPE`, `TEXTSTYLE`, `DIMSTYLE`, `CMLEADERSTYLE`, `CTABLESTYLE`), mục phụ thuộc xref; lý do khớp bảng ca vàng.
  3. "Dọn cả mục lồng nhau" một lần cho cùng kết quả với chạy lặp đến khi hết.
  4. Phân tích BENCH-STD 100 MB ≤ 500 ms, bản vẽ 50.000 regapp ≤ 300 ms với 8 luồng **[đề xuất]**; hiệu suất ≥ 60% đến 8 luồng; kết quả không phụ thuộc số luồng.
  5. Ước tính dung lượng giảm lệch ≤ 15% so với tệp lưu lại bằng DWG-022 trên 50 tệp QA-001 **[đề xuất]**.
  6. Đạt DoD-C, DoD-H.

### DOC-014 · Sao chép sâu có ánh xạ ID, xóa/khôi phục, phân tích tham chiếu cho PURGE: tích hợp WBLOCK, INSERT tệp, clipboard, REFEDIT
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DOC-008, DOC-013, DOC-117, DOC-118
- **Đầu vào:** README §5.5 DOC-014; 03 §6 (COPY, ARRAY, ERASE/OOPS, clipboard COPYCLIP…PASTESPEC), §2 (PURGE); API DOC-117, DOC-118; tài liệu xref dùng chung DOC-114.
- **Đầu ra:** `core/doc/clone/profiles.{h,cpp}`: hồ sơ `CopyInPlace` (COPY, ARRAY), `Wblock` (tập đối tượng, block, toàn bản vẽ `*`), `InsertDwg` (tài liệu nguồn → định nghĩa block), `Clipboard` (tập → tài liệu tạm có điểm gốc; dán vào bản vẽ đích), `RefEdit` (tập làm việc từ tài liệu xref, ghi ngược khi lưu), `Bind` (Bind/Insert cho ký hiệu phụ thuộc); `core/doc/erase/oops.{h,cpp}`: tập xóa gần nhất khôi phục được ngoài undo; `core/doc/api/clone_purge.h`; mở rộng `c_abi_smoke.c`; test.
- **Tiêu chí nghiệm thu:**
  1. Mỗi hồ sơ có bảng ca vàng (block lồng, attribute, nhóm, thứ tự vẽ, dictionary, xdata, ký hiệu trùng tên) đạt 100%; bản vẽ đích qua AUDIT 0 lỗi.
  2. WBLOCK `*`: hàm băm chuẩn tắc trùng bản gốc trừ mục không tham chiếu bị bỏ; handle giữ nguyên **[đề xuất]**.
  3. RefEdit: ghi ngược chỉ đổi đối tượng trong tập làm việc; hủy thì bản vẽ chủ và xref không đổi (hàm băm).
  4. Clipboard 100.000 entity: sao ≤ 200 ms, dán ≤ 200 ms với 8 luồng **[đề xuất]**; tài liệu tạm tự hủy khi clipboard bị thay.
  5. OOPS khôi phục đúng tập xóa gần nhất kể cả sau lệnh khác; owner hoặc layer đã bị dọn → lỗi rõ, không khôi phục một phần **[đề xuất]**.
  6. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** các lệnh WBLOCK, INSERT, COPYCLIP…, REFEDIT, XREF Bind, PURGE (CMD).

## 12. Khóa tệp và tự lưu

### DOC-120 · Khóa tệp `.dwl`/`.dwl2` và khóa hệ điều hành trên tệp đang sửa
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** DOC-018, NT-011
- **Màn hình:** UI-B05
- **Đầu vào:** 03 §2 (khóa tệp `.dwl` P0); 07.1 UI-B05 (người đang mở, tên máy, thời điểm; Mở chỉ đọc); tệp `.dwl`/`.dwl2` mẫu do AutoCAD tạo, thu từ máy của doanh nghiệp pilot.
- **Đầu ra:** `core/doc/lock/dwl_lock.{h,cpp}`: giữ khóa hệ điều hành trên tệp DWG khi mở để sửa; ghi `.dwl`/`.dwl2` (người dùng, máy, thời điểm, thêm PID trong `.dwl2` **[đề xuất]**); đọc khóa có sẵn; nhận lại khóa mồ côi; xóa khi đóng; `core/doc/fuzz/dwl_fuzz.cpp`; test.
- **Tiêu chí nghiệm thu:**
  1. AutoCAD báo tệp đang bị khóa khi TrionCAD đang sửa và ngược lại (kiểm bằng mẫu thu từ máy pilot); người, máy, thời điểm đọc đúng, kể cả tên có dấu.
  2. Hai tiến trình mở cùng tệp đồng thời: đúng một bên được sửa, bên kia nhận `LockedBy{user, machine, time}` để mở chỉ đọc (DOC-119).
  3. Khóa mồ côi do TrionCAD trên cùng máy để lại (PID không còn chạy) được nhận lại; khóa của máy khác không bao giờ tự xóa.
  4. Thư mục không ghi được (ổ mạng chỉ đọc) → tài liệu mở ở `ReadOnlyFile` kèm lý do, không chặn việc mở.
  5. Fuzz đọc `.dwl`/`.dwl2` ≥ 1 giờ (QA-006). Đạt DoD-C.

### DOC-019 · Autosave từ snapshot ở luồng nền (`.sv$`), `.bak`, khóa `.dwl`/`.dwl2`
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-009, DOC-112, DOC-120, DWG-022, NT-013
- **Màn hình:** UI-B07
- **Đầu vào:** README §5.5 DOC-019; 03 §2 (`SAVETIME`, `ISAVEBAK`, DRAWINGRECOVERY); 05 §9.4 (autosave tuần tự hóa ảnh chụp trên luồng nền); 07.1 UI-B07; bộ ghi DWG-022; ghi an toàn NT-011; tác vụ nền NT-013.
- **Đầu ra:**
  - `core/doc/autosave/autosave.{h,cpp}`: hẹn giờ theo `SAVETIME`, chỉ chạy khi có thay đổi từ lần tự lưu trước; lấy snapshot, ghi `.sv$` vào `SAVEFILEPATH` ở luồng nền ưu tiên thấp;
  - `core/doc/autosave/backup.{h,cpp}`: `.bak` theo `ISAVEBAK` cho lệnh lưu;
  - `core/doc/autosave/recovery_index.{h,cpp}`: chỉ mục phục hồi (tệp gốc, `.bak`, `.sv$`, thời điểm, thumbnail) cho UI-B07, dọn khi đóng bình thường; fuzz chỉ mục; test.
- **Tiêu chí nghiệm thu:**
  1. Không khựng: phần trên luồng UI ≤ 1 ms với BENCH-STD 100 MB; QA-011 đo khung hình pan trong lúc tự lưu vẫn đạt DoD-L.
  2. Tự lưu không đổi `DBMOD`, đường dẫn, tên tài liệu, chấm chưa lưu.
  3. Kill tiến trình ở thời điểm ngẫu nhiên khi đang tự lưu, 1.000 lần: luôn còn một `.sv$` mở được (tệp tạm + đổi tên).
  4. `.bak` đúng là bản trước khi lưu; `ISAVEBAK` = 0 không tạo; đĩa đầy → báo lỗi, tệp gốc nguyên vẹn.
  5. Sau crash, chỉ mục liệt kê đủ phiên bản cho UI-B07; đóng bình thường xóa `.sv$` và mục tương ứng.
  6. Đạt DoD-C, DoD-H, DoD-L.

## 13. Field, dynamic block, sửa chú thích theo tỉ lệ

### DOC-022 · Mô hình field: đối tượng field, giá trị cache, móc đánh giá
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** DOC-005, DOC-017, DOC-106
- **Đầu vào:** README §5.5 DOC-022, RGN-025; DXF Reference: FIELD, FIELDLIST, dictionary `ACAD_FIELD` trong extension dictionary của TEXT/MTEXT/ATTRIB/ô bảng; biến `FIELDEVAL`, `FIELDDISPLAY`; mã field `%<\AcVar …>%`, `%<\AcObjProp …>%`.
- **Đầu ra:** `core/doc/field/field.{h,cpp}`: cây field (mã, giá trị cache có kiểu, định dạng, field con, cờ đánh giá), gắn/gỡ field vào chuỗi chủ; `core/doc/field/eval_hooks.{h,cpp}`: đăng ký bộ đánh giá theo loại field (RGN-025 cấp), kích hoạt theo `FIELDEVAL` (mở, lưu, in, regen) và theo thay đổi đối tượng nguồn; test.
- **Tiêu chí nghiệm thu:**
  1. Field mẫu của QA-001 (ngày, tên tệp, thuộc tính đối tượng, thuộc tính tùy biến DWGPROPS, công thức bảng): đọc đủ, ghi lại giữ nguyên mã và cache (QA-003).
  2. Loại field chưa có bộ đánh giá → giữ và hiển thị giá trị cache, không xóa.
  3. Đối tượng nguồn đổi → field phụ thuộc được đánh dấu trong cùng transaction qua hook; 10.000 đối tượng nguồn ≤ 20 ms phần đánh dấu **[đề xuất]**.
  4. Field tham chiếu vòng → giá trị lỗi `####` như AutoCAD, không lặp vô hạn.
  5. Đạt DoD-C.

### DOC-023 · Mô hình dữ liệu dynamic block (EvalGraph, parameter, action, visibility, lookup)
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** DOC-012, DOC-017, DWG-015
- **Đầu vào:** README §5.5 DOC-023, RGN-019 (engine 1.0); đối tượng dynamic block đọc và giữ nguyên của DWG-015; DXF Reference: lớp EvalGraph, parameter, action của block.
- **Đầu ra:** `core/doc/dynblock/eval_graph.{h,cpp}`: đồ thị node/cạnh có kiểu trên đối tượng sinh từ schema; `core/doc/dynblock/params.{h,cpp}`: parameter (linear, polar, XY, rotation, flip, visibility, lookup, alignment, base point), action, trạng thái hiển thị, bảng lookup; giá trị thuộc tính động trên từng INSERT (đọc/ghi qua DOC-017), liên kết với `*U`; test.
- **Tiêu chí nghiệm thu:**
  1. 100% dynamic block mẫu QA-001 dựng đồ thị không lỗi; parameter, action, trạng thái hiển thị, bảng lookup khớp DXF xuất bởi ODA File Converter (QA-004).
  2. Giá trị thuộc tính động của mỗi INSERT đọc qua DOC-017 khớp giá trị lưu trong tệp; ghi qua transaction đánh dấu `*U` cần sinh lại cho RGN-019.
  3. Đồ thị hỏng (node treo, cạnh sai loại) → báo DOC-015; INSERT vẫn hiển thị bằng `*U` sẵn có.
  4. Không sửa → round-trip giữ nguyên từng byte (QA-003).
  5. Đạt DoD-C.

### DOC-121 · Sửa chú thích theo tỉ lệ: thêm/bớt tỉ lệ, đồng bộ vị trí, danh sách tỉ lệ
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** DOC-021, DOC-116
- **Đầu vào:** README §5.5 DOC-021 (sửa ở 1.0); API đọc DOC-021; biến `ANNOAUTOSCALE`, `CANNOSCALE`; các lệnh OBJECTSCALE, ANNORESET, ANNOUPDATE, SCALELISTEDIT (thuộc CMD).
- **Đầu ra:** `core/doc/annot/context_edit.{h,cpp}`: thêm/bớt tỉ lệ cho tập đối tượng (tạo biểu diễn từ biểu diễn mặc định), đặt lại vị trí, cập nhật theo kiểu, tự thêm khi đổi `CANNOSCALE` theo `ANNOAUTOSCALE`; sửa danh sách tỉ lệ (thêm, đổi tên, xóa SCALE không dùng); test.
- **Tiêu chí nghiệm thu:**
  1. Bảng ca vàng thêm/bớt/đặt lại cho TEXT, MTEXT, kích thước, MLEADER, hatch, block annotative đạt 100%; bớt tỉ lệ cuối cùng bị từ chối.
  2. Tệp ghi lại mở trong AutoCAD đúng biểu diễn (bộ tham chiếu QA-007 khi có đáp án); round-trip QA-003 không lệch.
  3. Thêm 1 tỉ lệ cho 10.000 đối tượng trong một transaction ≤ 50 ms qua DOC-116 **[đề xuất]**.
  4. Xóa SCALE đang được tham chiếu bị từ chối kèm số đối tượng dùng.
  5. Đạt DoD-C, DoD-H.

## Tóm tắt theo mốc

| Mốc | Task | Số task |
|---|---|---|
| S1 | DOC-001, DOC-101, DOC-102, DOC-002, DOC-003, DOC-010, DOC-004 | 7 |
| V | DOC-103, DOC-104, DOC-105, DOC-005, DOC-106, DOC-007, DOC-109, DOC-110, DOC-009, DOC-016, DOC-113, DOC-011, DOC-012, DOC-020, DOC-017, DOC-021, DOC-119, DOC-112, DOC-018, DOC-114, DOC-013, DOC-015 | 22 |
| VP | DOC-006, DOC-108, DOC-008 | 3 |
| EA | DOC-116, DOC-115, DOC-118, DOC-117, DOC-014, DOC-120, DOC-019 | 7 |
| 1.0 | DOC-022, DOC-023, DOC-121 | 3 |
| **Tổng** | | **42** |
