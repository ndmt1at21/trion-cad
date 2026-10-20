# Core INT · Tương tác và command framework

> Phạm vi mục 1–7: registry lệnh, runtime lệnh dạng coroutine, input pipeline duy nhất, prompt và từ khóa, parser giá trị và tọa độ, sysvar, alias PGP, script `.scr`, mô hình dòng lệnh, cổng phiên bản, `trion-console`, DIESEL, `'CAL`, QUICKCALC. Từ mục 8: chọn, hit test, OSNAP, tracking, jig, grip, DYN, UCS, điều hướng, menu ngữ cảnh, cô lập.
> Thư mục mã: `cmd/`, `interact/`, `console/`.
> Căn cứ: [05](../05-kien-truc-ky-thuat.md) §1.1, §5.3, §5.4, §10; [03](../03-chuc-nang-autocad.md) §1, §3, §18, §19; [07.1](../07-dac-ta-ui-1-khung-ung-dung.md) §1, §8, §11, §12; [07.2](../07-dac-ta-ui-2-bang-va-hop-thoai.md) UI-E10, UI-I10; [07.3](../07-dac-ta-ui-3-tinh-nang-vn-va-he-thong.md) UI-K04, UI-K05, UI-K13.

## 0. Ghi chú thiết kế

- **Một luồng ghi** (05 §5.4): registry, runtime lệnh, prompt, sysvar, dòng lệnh chạy trên luồng UI của document. Truy vấn tương tác (hit test, OSNAP, tracking, jig, grip) đọc snapshot DOC-009 và BVH, ưu tiên cao trên nhân P (NT-005). Việc nặng chạy nền, hủy được.
- **Một input pipeline** (05 §5.3): bàn phím, vùng vẽ, script, LISP, macro, menu/ribbon và nguồn ngoài (AI, SDK) đều thành token có kiểu đi vào `cmd::InputPipeline`. Không có đường tắt; mọi sửa bản vẽ trong lệnh đi qua DOC-007.
- **Tương thích AutoCAD là hợp đồng:** tên lệnh, thứ tự prompt, từ khóa, cách tách token của `.scr`, thông báo. Chữ tắt tiếng Anh luôn gõ được; tiếng Việt dùng để hiển thị (chip, mô tả) và tìm lệnh. Hành vi ghi ở `docs/specs/`, đối chiếu QA-007 khi có đáp án.
- **Lõi không dùng Qt Widgets:** `cmd/`, `interact/` xuất mô hình và sự kiện; UIA-007, UIA-013… vẽ. `trion-console` chạy cùng mã với vòng sự kiện không GUI.
- **Dữ liệu thay cho mã** **[đề xuất]**: metadata lệnh, alias mặc định, bảng sysvar, chuỗi prompt và từ khóa vi/en, từ khóa tìm tiếng Việt nằm ở `cmd/data/` (JSON, TSV), sinh bảng C++ lúc build bằng script Python chỉ dùng thư viện chuẩn, có lint trong CI.
- **Ranh giới** **[đề xuất]**: chuỗi → số chỉ có một parser (INT-105) dùng cho prompt, `'CAL`, QUICKCALC, ô số UI-C09 và `distof`/`angtof`; số → chuỗi thuộc RGN-013. Prompt chọn (`getSelection`) và `entsel`/`nentsel` do INT-008, INT-009 dựng trên API prompt INT-106; chuỗi task của INT-003 không phụ thuộc engine chọn.
- **Tầng tương tác cắm vào prompt, không ngược lại:** OSNAP, tracking, jig, DYN, UCS đọc `PromptState` và trả điểm đã xử lý thành token. Parser điểm nhận hướng con trỏ đã ràng buộc làm tham số, nên chạy được ở V trước khi có tracking (EA).
- **Kiểm thử không vòng:** test của INT dùng lệnh giả lập ở `cmd/tests/fixtures/` (TESTLINE, TESTPROMPT…), không phụ thuộc task CMD vì CMD phụ thuộc INT. Nghiệm thu với lệnh thật nằm ở bộ KTL của CMD và từng task lệnh.
- **Đầu vào không tin cậy** (DoD-C 7): `.scr`, `.pgp`, chuỗi DIESEL, biểu thức CAL, chuỗi từ LISP và macro. Kiểm biên, giới hạn độ dài và độ sâu, fuzz ≥ 1 giờ.
- **Thư mục** **[đề xuất]**: `cmd/registry`, `cmd/runtime`, `cmd/input`, `cmd/prompt`, `cmd/parse`, `cmd/sysvar`, `cmd/pgp`, `cmd/script`, `cmd/cmdline`, `cmd/gate`, `cmd/calc`, `cmd/data`; `interact/`; `console/`. Kiểm thử `cmd/tests/`, benchmark `cmd/bench/`, fuzz `cmd/fuzz/`.

## 1. Registry lệnh và metadata

### INT-101 · Metadata lệnh dạng dữ liệu và bảng alias mặc định
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-001, NT-007
- **Đầu vào:** README §4.2 (INT-001 giữ metadata, INT-017 giữ chỉ mục); 03 Phụ lục A (danh sách lệnh, ưu tiên); 07.1 §1 nguyên tắc 5 và 8, §7 (vị trí ribbon), §8 (từ khóa tiếng Việt); `design/prototype/js/icons.js` (tên biểu tượng).
- **Đầu ra:**
  - `cmd/data/commands/<nhóm>.json`: mỗi lệnh có tên, alias mặc định, mô tả ngắn vi/en, từ khóa tìm tiếng Việt, mã biểu tượng, mã trợ giúp, khả năng phiên bản (PLF-002), vị trí ribbon, cờ (trong suốt, dùng PICKFIRST, sửa bản vẽ, không mark undo), trạng thái `planned`/`shipped`.
  - `cmd/registry/gen_metadata.py` → `generated/CommandMetadata.inc` lúc build; `cmd/registry/CommandMetadata.h`; lint `cmd/registry/lint_metadata.py` trong CI.
- **Tiêu chí nghiệm thu:**
  1. Có mục cho 100% lệnh P0, P1, P2 trong 03 Phụ lục A thuộc phạm vi v1.0; 100% có mô tả vi và en, mã trợ giúp, khả năng phiên bản; mã biểu tượng có trong `icons.js` hoặc ghi rõ `none`.
  2. Lint chặn merge khi lệnh `shipped` thiếu `docs/specs/commands/<TÊN>.md` (05 §10), khi alias trùng nhau, trùng tên lệnh hoặc trỏ tới lệnh không có, khi có khóa JSON lạ.
  3. Bảng alias mặc định giữ các alias 2D phổ biến của AutoCAD (L, C, Z, P, TR, EX, O, F, CO, M…), soạn từ tài liệu công khai, ≥ 150 alias **[đề xuất]**.
  4. Mỗi lệnh P0 có ≥ 2 từ khóa tiếng Việt ("cắt" → TRIM, "bo góc" → FILLET).
  5. Nạp bảng sinh sẵn ≤ 1 ms lúc khởi động, không cấp phát heap **[đề xuất]**.
  6. Đạt DoD-C.
- **Ngoài phạm vi:** alias từ tệp `.pgp` (INT-006); chỉ mục tìm (INT-110).

### INT-001 · Registry lệnh: tên toàn cục và địa phương, tiền tố, UNDEFINE/REDEFINE
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-101, NT-007
- **Lệnh:** UNDEFINE, REDEFINE
- **Đầu vào:** 05 §10 (`_LINE`, tiền tố `-` `.` `'`, UNDEFINE/REDEFINE), §5.3 (luồng lệnh LINE); README §5.9 INT-001; metadata INT-101.
- **Đầu ra:**
  - `cmd/registry/Registry.h/.cpp`: đăng ký theo nhóm (có sẵn, plugin, LISP `C:`), tên toàn cục và địa phương, cờ, metadata; `resolve()` tách tiền tố `_` `.` `'` theo mọi thứ tự hợp lệ (`_.LINE`, `._LINE`, `'_.ZOOM`, `_-LAYER`), `-TÊN` là lệnh riêng; bảng alias; sự kiện `registryChanged` gom lô cho INT-110, LSP-107, SDK.
  - Lệnh UNDEFINE, REDEFINE; `docs/specs/commands/UNDEFINE.md`, `REDEFINE.md`; `docs/specs/command-names.md` (thứ tự phân giải lệnh có sẵn, lệnh LISP/plugin, alias, sysvar theo AutoCAD).
- **Tiêu chí nghiệm thu:**
  1. ≥ 300 chuỗi ca vàng (tiền tố, hoa/thường, alias, tên LISP, tên plugin, bản `-`, tên lạ) phân giải đúng 100%.
  2. UNDEFINE LINE rồi đăng ký `C:LINE`: `LINE` chạy bản thay thế, `.LINE` và `_.LINE` chạy bản gốc; REDEFINE khôi phục; tên không có báo lỗi theo spec; trạng thái chỉ sống trong phiên.
  3. Gỡ một nhóm 1.000 lệnh (plugin, LISP nạp lại) ≤ 1 ms, phát đúng một sự kiện **[đề xuất]**.
  4. `resolve` ≤ 200 ns p99 với 3.000 tên, 0 cấp phát (NT-004) **[đề xuất]**; sửa registry ngoài luồng UI bị assert ở bản debug.
  5. Đạt DoD-C.

## 2. Runtime lệnh và input pipeline

### INT-102 · Runtime lệnh coroutine trên luồng UI: ngăn xếp lệnh, lệnh trong suốt, vòng đời
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** INT-001, DOC-007, DOC-018, NT-005, NT-007, NT-009
- **Đầu vào:** 05 §5.3, §5.4 (lệnh và LISP chạy trên luồng UI); 07.1 §8 (lệnh trong suốt, prompt lồng, Esc); API DOC-007, DOC-018, NT-005 (đưa việc về luồng UI).
- **Đầu ra:**
  - `cmd/runtime/Task.h`: `cmd::Task<T>` (coroutine C++20, frame cấp từ pool NT-007); hủy hợp tác bằng Esc truyền qua mọi `co_await`.
  - `cmd/runtime/CommandStack.h/.cpp`: ngăn xếp theo document (lệnh chính + lệnh trong suốt); khe "prompt đang chờ" (loại prompt, cờ nhận khoảng trắng); lệnh vừa dùng để lặp; tập chọn ngầm PICKFIRST theo document (INT-008 ghi, lệnh có cờ `UsePickfirst` nhận).
  - Sự kiện `commandWillStart/Ended/Cancelled/Failed`; sysvar CMDACTIVE, CMDNAMES (dạng `LINE'ZOOM`); `cmd/runtime/Executor.h` (vòng Qt do UI0-001 nối, vòng không GUI cho INT-019); `docs/specs/command-runtime.md`.
- **Tiêu chí nghiệm thu:**
  1. Esc tại mọi điểm chờ của 50 lệnh giả lập: lệnh kết thúc Cancelled, 0 frame coroutine rò (bộ đếm), RAII chạy đủ, ASan sạch.
  2. Lệnh trong suốt chạy lồng khi lệnh chính đang chờ; CMDNAMES, CMDACTIVE đúng bảng ca vàng; lệnh không trong suốt gọi bằng `'` bị từ chối với thông báo theo spec.
  3. Ngoại lệ trong lệnh: trạng thái Failed, thông điệp dòng lệnh, log NT-009, ứng dụng chạy tiếp.
  4. Lệnh rỗng từ bắt đầu đến kết thúc ≤ 20 µs p99; tiếp tục coroutine sau một input ≤ 2 µs p99; 0 cấp phát heap mỗi lần chờ prompt **[đề xuất]**.
  5. Hai document có ngăn xếp riêng; đổi document khi đang có lệnh thì hủy lệnh của document cũ **[đề xuất]**.
  6. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** mark undo theo lệnh (INT-115, khi có DOC-008).

### INT-103 · Input pipeline: token có kiểu, nguồn nhập, Enter/Space/Esc, bộ chặn theo tiền tố
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** INT-102, INT-001
- **Đầu vào:** 05 §5.3 (script, `(command ...)`, macro, AI đi qua cùng một pipeline); 07.1 §1 nguyên tắc 1, §8 (Enter/Space lặp lệnh, Esc hai lần bỏ vùng chọn), §11; 07.2 UI-E10 (lệnh `-` không mở hộp thoại).
- **Đầu ra:**
  - `cmd/input/InputPipeline.h/.cpp`: `submit(Token, Source)` theo document; token Text, Point (WCS), Number, EntityPick, SelectionRef, Keyword, Enter, Cancel; nguồn Keyboard, Viewport, Script, Lisp, Macro, Menu, External (AI, SDK).
  - Ngăn xếp nguồn: nguồn chương trình cấp token, người dùng nhập khi nguồn hết token hoặc nhường lượt; Esc của người dùng cắt ngang mọi nguồn.
  - Quy tắc: Enter/Space rỗng ở prompt lệnh lặp lệnh vừa dùng; Space là Enter trừ prompt chuỗi nhận khoảng trắng; Esc thứ hai bỏ tập chọn; `'` mở lệnh trong suốt ở mọi prompt; bộ chặn đăng ký theo ký tự đầu (`(`, `!` cho LSP-107).
  - `isInteractive()` cho FILEDIA/CMDDIA; echo theo nguồn (CMDECHO, NOMUTT); `docs/specs/input-pipeline.md`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 200 chuỗi sự kiện ca vàng (gõ, nhấp, Enter, Space, Esc, Esc Esc, `'ZOOM` giữa lệnh, Enter rỗng) cho trạng thái lệnh và token nhận được đúng 100%.
  2. Cùng kịch bản qua Keyboard, Script, Macro, Menu, External cho cùng hash bản vẽ và cùng nhật ký prompt (50 kịch bản lệnh giả lập).
  3. Esc của người dùng dừng nguồn Script/Macro trong ≤ 16 ms, bỏ phần token còn lại, báo vị trí bị ngắt **[đề xuất]**.
  4. Lệnh chạy từ nguồn không tương tác nhận `isInteractive() == false` kể cả khi FILEDIA = 1, như AutoCAD.
  5. Nhận token đến khi giao cho prompt đang chờ ≤ 5 µs p99; fuzz 1 triệu chuỗi token ngẫu nhiên không treo, không rò **[đề xuất]**.
  6. Đạt DoD-C, DoD-L.

### INT-002 · Input pipeline duy nhất và lệnh coroutine: nguồn LISP, tạm dừng/tiếp tục, nghiệm thu chung
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-102, INT-103, LSP-003, LSP-006
- **Đầu vào:** 05 §5.3, §5.4 (evaluator tạm dừng ở `getpoint`, `(command ... pause)` mà không chặn UI); hợp đồng `lisp::CommandHost` và bộ kiểm tuân thủ 300 ca của LSP-006; API INT-102, INT-103.
- **Đầu ra:**
  - `cmd/input/LispSource.h/.cpp`: hiện thực `lisp::CommandHost` trên pipeline: lệnh dở dang qua nhiều lần `command`, `pause` nhường một lần nhập, `command-s` phải xong trong lần gọi, `vl-cmdf` kiểm đối số trước.
  - Cầu tạm dừng/tiếp tục evaluator LSP-003 khi chờ input, trên luồng UI.
  - `cmd/Cmd.h`: header công khai duy nhất cho CMD, LSP, SDK, UI0-015; `cmd/bench/dispatch_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Qua 100% bộ kiểm tuân thủ `lisp/tests/bridge/conformance/` của LSP-006.
  2. `(command "TESTLINE" pause pause "")` và `getpoint` gọi khi lệnh trong suốt đang chạy: luồng UI không bị chặn quá 2 ms lúc chờ, khung hình vẫn vẽ **[đề xuất]**.
  3. 50 kịch bản lệnh giả lập qua bàn phím mô phỏng, nguồn Script và `(command ...)` cho cùng hash bản vẽ.
  4. LISP đệ quy 10.000 mức, mỗi mức gọi `command`: không tràn stack C++.
  5. Lệnh giả lập một prompt gọi từ `(command)`: overhead điều phối ≤ 10 µs p99 **[đề xuất]**.
  6. Đạt DoD-C, DoD-L.

### INT-115 · Mark undo theo lệnh: nối DOC-008 vào runtime
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** INT-002, DOC-008
- **Đầu vào:** 05 §5.3 (`undo.beginMark` … `undo.endMark` quanh lệnh); API mark của DOC-008; cờ `NoUndoMark` trong metadata INT-101.
- **Đầu ra:** `cmd/runtime/UndoBinding.cpp`: mỗi lệnh không trong suốt mở và đóng một mark, kể cả khi bị hủy hay lỗi; API nhóm cho LSP-107 (cả lệnh `defun c:` là một nhóm); bổ sung mục undo vào `docs/specs/command-runtime.md`.
- **Tiêu chí nghiệm thu:**
  1. 50 lệnh giả lập: mỗi lần chạy tạo đúng một mark; undo một bước trả bản vẽ về hash trước lệnh.
  2. Esc giữa lệnh: phần đã ghi giữ lại và thuộc mark của lệnh; ngoại lệ giữa lệnh: mark vẫn đóng.
  3. Lệnh trong suốt không tách mark của lệnh ngoài; nhóm BEgin/End của DOC-008 mở từ lệnh giả lập trong script và LISP gom đúng.
  4. Overhead mark ≤ 5 µs mỗi lệnh **[đề xuất]**.
  5. Đạt DoD-C.

## 3. Prompt, từ khóa, giá trị và tọa độ

### INT-104 · Engine từ khóa: so khớp, từ khóa toàn cục và ẩn, catalog prompt vi/en, dữ liệu chip
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-007, NT-012
- **Đầu vào:** 05 §10 (cú pháp `[Option/Another]`, từ khóa toàn cục `_`); 07.2 UI-E10 (nhãn "Bán kính · R", chữ tắt tiếng Anh luôn có tác dụng, ngôn ngữ prompt chọn ở UI-D10); 07.1 §8 (chip, chữ tắt gạch chân), §13.4 (thuật ngữ).
- **Đầu ra:**
  - `cmd/prompt/Keywords.h/.cpp`: khai báo từ khóa (tên toàn cục, chữ tắt theo chữ hoa hoặc dạng `LTYPE,LT`, nhãn vi, ẩn, vô hiệu); so khớp; chuỗi en `[Undo/Close] <Close>:`; dữ liệu chip {nhãn vi, nhãn en, chữ tắt, vị trí gạch chân}.
  - `cmd/data/prompts/<LỆNH>.json`: catalog lời nhắc và từ khóa vi/en theo mã `LỆNH.prompt`; kiểm catalog trong CI.
- **Tiêu chí nghiệm thu:**
  1. ≥ 400 ca vàng đúng 100%: chữ tắt, mọi tiền tố dài hơn chữ tắt, cả từ, không phân biệt hoa thường, `_Close` khớp tên toàn cục, từ khóa số và ký hiệu (`2`, `?`), từ khóa ẩn nhận được nhưng không hiện.
  2. Khai báo mơ hồ (hai từ khóa cùng chữ tắt, chữ tắt không là tiền tố) bị phát hiện lúc đăng ký (bản debug) và bằng test quét toàn catalog.
  3. Chế độ vi cho chip "Bán kính · R", chế độ en cho "Radius" gạch chân R; gõ "R" khớp ở cả hai chế độ.
  4. 100% mục catalog có vi và en; nhãn vi qua kiểm thuật ngữ 07.1 §13.4 (danh sách từ cấm).
  5. So khớp một chuỗi ≤ 1 µs p99 **[đề xuất]**.
  6. Đạt DoD-C.

### INT-105 · Parser giá trị: số thực, số nguyên, khoảng cách và góc theo mọi LUNITS, AUNITS
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-013, QA-006
- **Đầu vào:** 03 §3; 07.1 §8 (dấu thập phân luôn là "."; nhắc một lần khi gõ ","); sysvar LUNITS, AUNITS, ANGBASE, ANGDIR, UNITMODE; RGN-013 (định dạng, dùng kiểm vòng).
- **Đầu ra:**
  - `cmd/parse/ValueParser.h/.cpp`: `parseReal`; `parseInt` (−32768…32767 như `getint`); `parseDistance` (khoa học, thập phân, kỹ thuật, kiến trúc, phân số; feet-inch theo LUNITS); `parseAngle` (độ, độ-phút-giây, grad, radian, góc khảo sát `N45d30'E`; ANGBASE, ANGDIR); mã lỗi có kiểu, kể cả `CommaAsDecimal`.
  - Fuzz target `cmd/fuzz/value_parser_fuzz.cpp`; `docs/specs/input-values.md`.
- **Tiêu chí nghiệm thu:**
  1. Kiểm vòng 1 triệu giá trị × mọi LUNITS, AUNITS, độ chính xác: `parse(format(x))` lệch ≤ nửa đơn vị của độ chính xác hiển thị.
  2. ≥ 500 chuỗi ca vàng (hợp lệ, sai, biên) đúng giá trị và mã lỗi; "1,5" ở prompt số trả `CommaAsDecimal`.
  3. Không phụ thuộc locale hệ điều hành (vi_VN và de_DE cho cùng kết quả); tất định trên 3 hệ điều hành.
  4. Fuzz ≥ 1 giờ không lỗi.
  5. ≤ 300 ns p99 cho chuỗi ≤ 32 ký tự **[đề xuất]**.
  6. Đạt DoD-C.
- **Ngoài phạm vi:** số → chuỗi (RGN-013).

### INT-004 · Parser tọa độ và điểm: tuyệt đối, `@`, `<`, `#`, `*`, khoảng cách trực tiếp, ép góc, bộ lọc, From, M2P
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-105, DOC-010, GEO-001, GEO-002
- **Đầu vào:** 03 §3; 07.1 §8 (`100,200`, `@100,0`, `@500<45`, khoảng cách trực tiếp, `<30`); README §5.9 INT-004; biến header UCSORG, UCSXDIR, UCSYDIR, ELEVATION, LASTPOINT (DOC-010).
- **Đầu ra:**
  - `cmd/parse/PointParser.h/.cpp`: chuỗi → `PointSpec` (tuyệt đối, tương đối `@`, cực `<`, `#` ép tuyệt đối khi DYN mặc định tương đối, `*` ép WCS); giải theo ngữ cảnh (điểm cuối, điểm gốc rubber-band, hướng con trỏ đã ràng buộc do tầng tương tác cấp, ma trận UCS).
  - Khoảng cách trực tiếp; ép góc `<a` (các dạng `<<`, `<<<` theo spec); bộ lọc `.X .Y .Z .XY .XZ .YZ` có prompt phụ "(cần YZ)"; FROM (điểm gốc + offset); M2P/MTP.
  - Fuzz target; `docs/specs/input-coordinates.md`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 400 ca vàng (chuỗi + ngữ cảnh → điểm WCS) đúng 100% với WCS và 20 UCS xoay/dịch, so qua `Tolerance` (GEO-002).
  2. Tọa độ VN-2000 (≈ 10⁹, BENCH-STD): `@0.001<30` cho điểm lệch ≤ 1e-6 đơn vị.
  3. Bộ lọc điểm, FROM, M2P: chuỗi prompt phụ, Esc trong prompt phụ và kết quả khớp spec 100%.
  4. Fuzz ≥ 1 giờ không lỗi.
  5. Phân tích và giải ≤ 1 µs p99 **[đề xuất]**.
  6. Đạt DoD-C.
- **Ngoài phạm vi:** POLAR, ORTHO, OTRACK, TK (INT-011); lệnh UCS (INT-015).

### INT-106 · API prompt coroutine: điểm, góc chữ nhật, khoảng cách, góc, chuỗi, số nguyên, số thực, từ khóa
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** INT-102, INT-103, INT-104, INT-004, INT-105, RGN-013
- **Đầu vào:** 07.2 UI-E10; 05 §10; ngữ nghĩa `getpoint`, `getcorner`, `getdist`, `getangle`, `getorient`, `getstring`, `getint`, `getreal`, `getkword`, `initget` của AutoLISP (tài liệu công khai).
- **Đầu ra:**
  - `cmd/prompt/Prompt.h/.cpp`: `co_await ctx.getPoint/getCorner/getDistance/getAngle/getOrientation/getString/getInteger/getReal/getKeyword(opts)` trả `PromptResult<T>` {Ok, Keyword, None, Cancel}; dạng callback cho evaluator LISP (LSP-007).
  - Tùy chọn tương đương cờ `initget` (không rỗng, khác 0, không âm, bỏ kiểm LIMITS, rubber-band nét đứt, bỏ Z, nhận chuỗi tùy ý); mặc định `<>` định dạng qua RGN-013; điểm gốc; nhập lại kèm thông báo khi sai; ghi LASTPOINT.
  - `PromptState` (loại, lời nhắc, từ khóa, mặc định, điểm gốc, mức lồng) phát cho INT-003, jig INT-012, DYN INT-014; `docs/specs/prompts.md`.
- **Tiêu chí nghiệm thu:**
  1. Ma trận 9 loại prompt × tùy chọn × đầu vào (chuỗi, điểm, Enter, Esc, từ khóa): ≥ 1.000 ca vàng đúng 100% theo spec.
  2. `getDistance`, `getAngle` nhận số gõ hoặc hai điểm; `getAngle` theo ANGBASE/ANGDIR, `getOrientation` bỏ ANGBASE; `getInteger` từ chối ngoài −32768…32767 kèm thông báo.
  3. LIMCHECK = 1 từ chối điểm ngoài LIMMIN/LIMMAX, trừ khi có cờ bỏ kiểm.
  4. Mỗi lần đổi prompt phát đúng một `PromptState`; từ lúc nhận input đến khi coroutine chạy tiếp ≤ 10 µs p99 **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** prompt chọn đối tượng, `entsel`/`nentsel` (INT-008, INT-009 dựng trên API này).

### INT-003 · Prompt và từ khóa cho giao diện: trạng thái prompt, chip tùy chọn, prompt lồng, ngôn ngữ prompt
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-106, INT-104, INT-001
- **Màn hình:** UI-E10
- **Đầu vào:** 07.2 UI-E10 (bảng prompt và tùy chọn); 07.1 §8 (lời nhắc, chip bấm được, mặc định `<10.0000>`, prompt lồng ">>"); metadata INT-001 (mã trợ giúp F1).
- **Đầu ra:**
  - `cmd/prompt/PromptModel.h/.cpp`: mô hình chỉ đọc cho dòng lệnh (UIA-007), DYN (INT-014), menu chuột phải (INT-018): lời nhắc, chip hiện/ẩn, mặc định, mức lồng, mã trợ giúp F1 theo lệnh và prompt.
  - Nhấp chip → token Keyword vào pipeline như gõ chữ tắt; đổi ngôn ngữ prompt vi/en không cần khởi động lại.
  - `PromptCatalog::forCommand(TÊN)` liệt kê prompt và từ khóa của lệnh cho bộ kiểm chip UIE-015; `docs/specs/prompt-chips.md`.
- **Tiêu chí nghiệm thu:**
  1. 30 prompt mẫu dựng theo 07.2 UI-E10 (LINE, CIRCLE, OFFSET, TRIM, FILLET, SELECT…) trên lệnh giả lập: chuỗi en ghép đúng định dạng AutoCAD từng ký tự; chip vi đúng nhãn và chữ tắt.
  2. Nhấp chip và gõ chữ tắt cho cùng token, cùng kết quả; từ khóa ẩn gõ được, không có chip.
  3. `'ZOOM` giữa lệnh: mô hình báo mức lồng 1, tiền tố `>>`; thoát lệnh trong suốt trả lại đúng prompt, chip và mặc định cũ.
  4. Đổi ngôn ngữ prompt giữa lệnh cập nhật prompt hiện tại ≤ 16 ms, không mất input đã nhận.
  5. Cập nhật mô hình mỗi lần đổi prompt ≤ 0,2 ms **[đề xuất]**.
  6. Đạt DoD-C, DoD-L.

## 4. Biến hệ thống

### INT-107 · Engine sysvar: kiểu, phạm vi lưu, kiểm miền, sự kiện đổi, snapshot cho luồng nền
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DOC-007, DOC-010, DOC-018, NT-010
- **Đầu vào:** 05 §10 (registry có kiểu và phạm vi, thông báo khi đổi); 07.3 UI-K13 (lưu ở bản vẽ, người dùng hay không lưu; giá trị cũ → mới); API DOC-010 (tra biến header theo tên), DOC-007, DOC-018, NT-010.
- **Đầu ra:**
  - `cmd/sysvar/SysvarRegistry.h/.cpp`: kiểu int16, int32, real, string, điểm 2D/3D, bitcode; phạm vi header bản vẽ (ghi qua DOC-007 nên hoàn tác được), profile (NT-010), phiên, chỉ đọc tính toán (DATE, DWGNAME, CMDACTIVE…); kiểm miền (khoảng, liệt kê, bitmask).
  - `get`/`set` có mã lỗi tương thích AutoLISP; sự kiện đổi gom lô kèm giá trị cũ, mới và nguồn (lệnh, LISP, người dùng) cho SYSVARMONITOR; `SysvarSnapshot` bất biến cho luồng nền; benchmark.
- **Tiêu chí nghiệm thu:**
  1. Biến header đổi trong lệnh được hoàn tác cùng lệnh; biến profile còn sau khởi động lại; biến phiên mất khi đóng ứng dụng.
  2. Hai document mở: đổi document hiện hành thì biến header theo document mới, biến profile giữ nguyên.
  3. Ghi biến chỉ đọc hoặc ngoài miền bị từ chối, giá trị giữ nguyên, mã lỗi đúng; 100% biến khai báo miền.
  4. `get` ≤ 50 ns p99, `set` ≤ 2 µs p99 không tính bộ nghe **[đề xuất]**; 16 luồng nền đọc snapshot, TSan sạch.
  5. Mỗi `set` làm đổi giá trị phát đúng một sự kiện (gom lô trong lệnh); `set` cùng giá trị không phát.
  6. Đạt DoD-C, DoD-L.

### INT-108 · Bảng khoảng 250 sysvar ưu tiên theo bộ LISP
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-107, QA-008, DWG-008
- **Đầu vào:** 05 §10 (khoảng 250 sysvar ở GA, ưu tiên theo bộ LISP); 07.3 UI-K13 (mặc định, nơi lưu, mô tả tiếng Việt); bộ LISP QA-008; biến header của DWG-008; tài liệu sysvar công khai của AutoCAD.
- **Đầu ra:**
  - `cmd/data/sysvars.tsv`: tên, kiểu, phạm vi, mặc định metric và imperial, miền, chỉ đọc, mô tả vi/en, hạng ưu tiên, lý do chọn (tần suất trong LISP, lệnh dùng, UI).
  - `cmd/sysvar/scan_lisp_usage.py` (đếm `getvar`/`setvar` trong QA-008, xuất xếp hạng); `cmd/sysvar/gen_sysvars.py` (sinh bảng C++ lúc build); test nhất quán.
- **Tiêu chí nghiệm thu:**
  1. ≥ 99% lượt `getvar`/`setvar` có tên hằng trong QA-008 trỏ tới biến có trong bảng **[đề xuất]**; CI in danh sách thiếu.
  2. 100% sysvar nhắc trong `docs/specs/commands/*.md` có trong bảng (lint CI).
  3. Biến header khớp tên và kiểu với DWG-008; khi QA-007 có bản vẽ mới tạo từ mẫu metric, mặc định khớp 100%.
  4. 100% biến có mô tả vi và en, nơi lưu, miền; tổng 230–280 biến.
  5. Đạt DoD-C.

### INT-005 · SETVAR, gõ thẳng tên biến, API cho trình duyệt và theo dõi biến
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-107, INT-108, INT-003, INT-002
- **Lệnh:** SETVAR
- **Màn hình:** UI-K13
- **Đầu vào:** 07.3 UI-K13 (SETVAR và gõ thẳng tên biến; lọc "khác mặc định", đặt lại, xuất/nhập; danh sách theo dõi mặc định OSMODE, CMDECHO, FILEDIA, PICKFIRST, ORTHOMODE); 07.2 UI-E10 dòng `SETVAR`.
- **Đầu ra:**
  - Lệnh SETVAR (`cmd/sysvar/SetvarCommand.cpp`): tên biến → giá trị mới với `<giá trị hiện tại>`; `?` liệt kê theo mẫu wildcard; gõ tên biến ở prompt lệnh; `'SETVAR` và `'TÊNBIẾN` trong suốt.
  - API cho UIK (SYSVDLG, SYSVARMONITOR): liệt kê có lọc, khác mặc định, đặt lại mặc định, xuất/nhập danh sách JSON; `docs/specs/commands/SETVAR.md`.
- **Tiêu chí nghiệm thu:**
  1. Ca vàng `SETVAR OSMODE 33`, `OSMODE` + Enter giữ nguyên, `'ORTHOMODE 1` giữa lệnh giả lập, `SETVAR ?` với `*SNAP*` đúng 100%, cùng kết quả qua `.scr` và `(command "SETVAR" …)`.
  2. Biến chỉ đọc, sai kiểu, ngoài miền: thông báo theo spec rồi hỏi lại; Esc không đổi giá trị.
  3. Xuất rồi nhập danh sách trả đúng giá trị mọi biến ghi được; đặt lại mặc định khớp bảng INT-108.
  4. Từ Enter đến khi biến đổi xong ≤ 1 ms trong lõi **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.

## 5. Dòng lệnh, tự hoàn thành, cổng phiên bản

### INT-110 · Chỉ mục tự hoàn thành: lệnh, alias, sysvar, lệnh LISP, từ khóa tiếng Việt, xếp theo tần suất
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-001, INT-101, INT-107, NT-010, NT-012
- **Màn hình:** UI-A06
- **Đầu vào:** 07.1 §8 (danh sách tự hoàn thành: lệnh, alias, sysvar, lệnh LISP/plugin, từ khóa tiếng Việt; tìm cả giữa chuỗi; sắp theo tần suất của người dùng); README §4.2 (ô Tìm lệnh, Trợ giúp, Start dùng lại chỉ mục); DoD-L (≤ 16 ms mỗi phím).
- **Đầu ra:**
  - `cmd/cmdline/CompletionIndex.h/.cpp`: mục từ registry và sysvar; khóa chuẩn hóa không dấu (`đ` → `d`, nhiều từ "bo goc"); khớp tiền tố và giữa chuỗi; hạng = tần suất cá nhân có suy giảm theo thời gian + độ khớp + ưu tiên tĩnh, hòa thì theo tên.
  - Tần suất lưu theo profile (NT-010); cập nhật gia tăng theo `registryChanged`; `cmd/tests/data/vn_queries.tsv` (300 câu tìm tiếng Việt có nhãn).
- **Tiêu chí nghiệm thu:**
  1. "cắt" → TRIM, "bo góc" và "bo goc" → FILLET đứng đầu; ≥ 95% trong 300 câu có lệnh đúng trong 3 kết quả đầu **[đề xuất]**.
  2. Truy vấn ≤ 1 ms p99 với 5.000 mục trên máy thấp, 0 cấp phát, để vòng phím → danh sách đạt ≤ 16 ms (DoD-L) **[đề xuất]**.
  3. Lệnh `C:` mới đăng ký hiện trong ≤ 10 ms; lệnh bị UNDEFINE ẩn; tắt tìm sysvar hoặc tìm tiếng Việt (INPUTSEARCHOPTIONS) có hiệu lực ngay.
  4. Lịch sử dùng mô phỏng (20 lần FILLET rồi gõ "F") cho thứ tự đúng 100% ca vàng; cùng lịch sử cho cùng thứ tự.
  5. Đạt DoD-C, DoD-L.

### INT-017 · Mô hình dòng lệnh: lịch sử, giá trị đã nhập, thông điệp, gợi ý khi gõ sai, bỏ qua bộ gõ khi chờ tên lệnh
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-110, INT-003, INT-002, INT-107
- **Lệnh:** INPUTSEARCHOPTIONS
- **Màn hình:** UI-A06
- **Đầu vào:** 07.1 §8 (lịch sử 0–10 dòng, F2, ↑/↓, gõ sai "Ý bạn là", gõ tiếng Việt, thiết lập dòng lệnh), §12 (thông điệp dòng lệnh); 03 §1 (INPUTSEARCHOPTIONS; Unikey/EVKey).
- **Đầu ra:**
  - `cmd/cmdline/CommandLineModel.h/.cpp`: chữ đang gõ, prompt (INT-003), danh sách tự hoàn thành (INT-110), cờ `expectsCommandName` để UI tắt bộ gõ tiếng Việt khi tùy chọn bật.
  - `History`: lệnh và giá trị đã nhập theo loại prompt (↑/↓, "Giá trị đã nhập gần đây"), số dòng hiện, lịch sử đầy đủ có tìm và sao chép cho F2, lưu theo profile; `Messages` (thường, cảnh báo, lỗi; NOMUTT).
  - `TypoSuggester`: Damerau–Levenshtein có trọng số bàn phím, tối đa 3 gợi ý bấm được.
  - Thiết lập dạng sysvar (AUTOCOMPLETEMODE, AUTOCOMPLETEDELAY, CLIPROMPTLINES, INPUTHISTORYMODE, CMDINPUTHISTORYMAX, tìm tiếng Việt, bỏ qua bộ gõ); lệnh INPUTSEARCHOPTIONS phát yêu cầu mở hộp thiết lập cho UIA; `docs/specs/commands/INPUTSEARCHOPTIONS.md`.
- **Tiêu chí nghiệm thu:**
  1. "LNE" cho "Không có lệnh 'LNE'. Ý bạn là: LINE, LEN?"; ≥ 95% trong 500 lỗi gõ mẫu có lệnh đúng trong 3 gợi ý **[đề xuất]**; bấm gợi ý chạy lệnh.
  2. ↑/↓ chỉ duyệt giá trị hợp loại prompt hiện tại; đúng 100% ca vàng.
  3. `expectsCommandName` đúng ở prompt lệnh, sai ở prompt chuỗi, đổi ngay khi prompt đổi.
  4. 100.000 dòng lịch sử: tìm ≤ 5 ms; mỗi thao tác của mô hình theo phím ≤ 1 ms p99 **[đề xuất]**.
  5. Đổi thiết lập qua sysvar hay qua hộp thiết lập có hiệu lực ngay, còn sau khởi động lại.
  6. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** AutoCorrect và từ đồng nghĩa (INT-111); vẽ hộp thiết lập (UIA).

### INT-111 · AutoCorrect và từ đồng nghĩa lệnh
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** INT-017, NT-010
- **Đầu vào:** 07.1 §8 (chọn cùng một gợi ý 3 lần thì lỗi gõ ấy tự sửa; xem và xóa trong thiết lập dòng lệnh); 03 §1 (AutoCorrect, Synonyms: P1).
- **Đầu ra:** `cmd/cmdline/AutoCorrect.h/.cpp` (đếm lựa chọn, quy tắc, liệt kê và xóa cho UI thiết lập); `cmd/data/synonyms.tsv` (từ đồng nghĩa lệnh, kể cả tiếng Việt); lưu theo profile.
- **Tiêu chí nghiệm thu:**
  1. Lần chọn thứ 3 cùng gợi ý tạo quy tắc; lần gõ sau chạy đúng lệnh, lịch sử ghi "LNE → LINE".
  2. Quy tắc và từ đồng nghĩa không che tên lệnh, alias, sysvar có thật (kiểm lúc tạo và khi registry đổi).
  3. Xóa quy tắc có hiệu lực ngay; quy tắc còn sau khởi động lại.
  4. Chỉ sửa nguồn Keyboard; script, LISP, macro không bị đổi.
  5. Đạt DoD-C.

### INT-020 · Cổng phiên bản cho lệnh: Viewer chỉ xem và đo, thông báo trên dòng lệnh
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** INT-001, INT-002, PLF-002
- **Đầu vào:** 07.1 §1 nguyên tắc 8, §2 (phiên bản; Viewer không chạy LISP); UI-M04 (layer trong Viewer, UIA-118); README §4.2 (PLF-002 khả năng, INT-020 lệnh, UI0-011 giao diện); khả năng phiên bản trong metadata INT-101.
- **Đầu ra:** `cmd/gate/EditionGate.h/.cpp`: kiểm khi điều phối lệnh từ mọi nguồn (lệnh chính, trong suốt, script, LISP, macro, menu); nguồn LISP bị từ chối ở Viewer; danh sách cho phép của Viewer gồm lệnh xem, đo và LAYOFF, LAYON, LAYFRZ, LAYTHW, LAYISO, LAYUNISO ở chế độ chỉ trong phiên (cờ `sessionOnly` trong ngữ cảnh lệnh); thông điệp dòng lệnh theo 07.1 §2 (vi, en) kèm sự kiện mở trang nâng cấp cho UI0-011; script dừng ở dòng bị chặn với mã lỗi riêng; `docs/specs/edition-gate.md`.
- **Tiêu chí nghiệm thu:**
  1. Ma trận mọi lệnh trong metadata × mọi phiên bản của PLF-002 khớp 100% bảng khả năng; lệnh xem, đo và 6 lệnh layer trên chạy được ở Viewer.
  2. Ở Viewer, 6 lệnh layer nhận `sessionOnly = true`: thay đổi không đánh dấu bản vẽ đã sửa (DBMOD giữ nguyên), không vào được tệp khi lưu (kiểm bằng lệnh giả lập cùng tên).
  3. Lệnh bị chặn không chạy, không đổi bản vẽ; thông điệp đúng văn bản 07.1 §2; bị chặn như nhau qua bàn phím, `.scr`, `(command)`, macro.
  4. Hết hạn dùng thử giữa phiên: lệnh đang chạy kết thúc bình thường, lệnh kế bị chặn, không mất dữ liệu; SAVE, QSAVE, SAVEAS vẫn chạy cho bản vẽ đã sửa trước khi hạ cấp, theo `canSaveDocument()` của PLF-002.
  5. Kiểm cổng ≤ 100 ns mỗi lần điều phối **[đề xuất]**.
  6. Đạt DoD-C.

## 6. Script, `trion-console`, alias

### INT-109 · Bộ chạy `.scr`: tách token như AutoCAD, mã hóa, vị trí dòng, lỗi
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-002, INT-003, NT-012, QA-006
- **Đầu vào:** 03 §18 (`.scr` là nền kiểm thử tự động); 07.3 UI-K04 (dòng đang chạy, tiến trình, lỗi báo số dòng và nội dung); 05 §5.3.
- **Đầu ra:**
  - `cmd/script/ScriptReader.h/.cpp`: khoảng trắng và xuống dòng là Enter, nhiều khoảng trắng liền là nhiều Enter, khoảng trắng là chữ khi prompt chuỗi nhận khoảng trắng, dòng trống là Enter, chú thích `;`, dòng bắt đầu bằng `(` chuyển cho bộ chặn LISP đến khi đủ ngoặc; nhận diện UTF-8, BOM, ANSI theo code page (NT-012).
  - `cmd/script/ScriptRunner.h/.cpp`: cấp token qua nguồn Script; sự kiện dòng, tiến trình, lỗi (dòng, nội dung, lý do); dừng và tiếp tục; fuzz target; `docs/specs/script-format.md`.
- **Tiêu chí nghiệm thu:**
  1. 200 `.scr` ca vàng cho cùng hash bản vẽ và nhật ký prompt như nhập tay qua pipeline; khớp 100% bộ tham chiếu QA-007 phần script khi có đáp án.
  2. Lệnh không có, giá trị sai, lệnh lỗi: dừng đúng dòng, sự kiện có số dòng, nội dung, lý do (100% trên 50 script lỗi).
  3. Chạy không giao diện: ≥ 20.000 dòng/s với lệnh giả lập không sửa bản vẽ, ≥ 5.000 lệnh TESTLINE/s; bộ nhớ không tăng theo độ dài script **[đề xuất]**.
  4. Fuzz ≥ 1 giờ không lỗi; tệp sai mã hóa báo lỗi rõ, không treo.
  5. Đạt DoD-C.

### INT-116 · Đầu ra cho QA của `trion-console`: thống kê mở và regen, render ảnh, thông tin backend GPU, in PDF qua lệnh
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-002, DWG-031, RGN-003, RND-018, RND-021
- **Đầu vào:** nhu cầu của harness QA (QA-001 kho bản vẽ và BENCH-STD, QA-012 cảnh benchmark): thống kê JSON, ảnh để so sánh, quyết định backend cho ma trận phần cứng; API DWG-031 (số đo mở tệp), RGN-003 (regen), RND-021 (render ngoài màn hình), RND-018 (backend, MSAA, compute, VRAM).
- **Đầu ra:**
  - `console/QaOutputs.h/.cpp`: `--stats` (JSON: thời gian từng pha mở, bộ nhớ đỉnh, số đối tượng theo loại, lỗi đã phục hồi; regen: thời gian, số primitive, số luồng); `--render out.png` (extents hoặc view có tên, kích thước, nền) qua RND-021; `--gpu-info` (backend được chọn, lý do, MSAA, compute, VRAM; tùy chọn ép backend).
  - `--pdf out.pdf`: chạy lệnh in PDF theo tên qua pipeline (lệnh do PLT/CMD đăng ký); không phụ thuộc PLT lúc biên dịch để tránh vòng với QA-005.
  - Schema `console/schema/*.schema.json`.
- **Tiêu chí nghiệm thu:**
  1. 100% tệp của bộ 1.000 tệp QA-001 cho JSON `--stats` hợp lệ schema; hai lần chạy cùng tệp cho cùng số đếm (thời gian ở trường riêng).
  2. `--render` chạy không màn hình trên D3D11, Vulkan, Metal, OpenGL (theo hệ điều hành); hai lần chạy trên cùng máy cho ảnh giống từng pixel.
  3. `--gpu-info` khi ép từng backend trả đúng backend đó; JSON hợp lệ schema trên 4 máy tham chiếu.
  4. `--pdf` với lệnh giả lập đăng ký cùng tên tạo đúng tệp; lệnh chưa đăng ký trả mã thoát "không hỗ trợ".
  5. Đạt DoD-C.

### INT-019 · `trion-console`: chạy không giao diện, script và LISP, mã thoát, nhật ký JSON
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-002, INT-109, INT-116, INT-005, LSP-006, DWG-010, DWG-024, DOC-018, QA-001
- **Đầu vào:** 03 §19 (console không giao diện, nội bộ dùng từ P0 để kiểm thử); README §3.1 (QA-001); `lisp::Host` và `lisprun` của LSP-006; API đọc DWG/DXF của DWG-010, DWG-024.
- **Đầu ra:**
  - `console/main.cpp`: `--open`, `--script`, `--lisp`, `--eval`, `--json`, `--timeout`, `--threads`, `--lang` cùng các cờ của INT-116; nhận cả `/i`, `/s`, `/l` quen thuộc với AutoCAD Core Console **[đề xuất]**; mở tệp bằng API codec, không qua lệnh OPEN (tránh vòng với CMD-001).
  - `console/HeadlessHost.h/.cpp`: `lisp::Host` không giao diện (alert ra stdout; hộp thoại, DCL, `getfiled` trả hủy kèm cảnh báo); vòng sự kiện không GUI (Executor của INT-102).
  - Bảng mã thoát, nhật ký JSON (lệnh, lỗi theo dòng, thời gian); `docs/specs/trion-console.md`.
- **Tiêu chí nghiệm thu:**
  1. Mã thoát đúng 100% ca vàng: 0 xong, 2 không mở được tệp, 3 lỗi script kèm số dòng, 4 lỗi LISP, 5 hết giờ, 6 lệnh không hỗ trợ, 64 sai tham số, 130 bị ngắt **[đề xuất]**.
  2. Nhiều tiến trình song song trên bộ 1.000 tệp QA-001: không treo, hết giờ được báo; nhật ký tất định (cùng hash trên 3 hệ điều hành và mọi `--threads`, trừ trường thời gian).
  3. Khởi động đến lúc nhận lệnh đầu ≤ 300 ms trên máy tầm trung **[đề xuất]**; không liên kết Qt Widgets; chạy được trên máy không có màn hình.
  4. `--eval` in kết quả theo `prin1`; lỗi LISP in thông báo như AutoLISP và trả mã 4.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** ghi tệp (script gọi SAVEAS khi lệnh có); phát hành cho khách hàng (P2).

### INT-006 · Alias `.pgp`: đọc `acad.pgp` của người dùng, REINIT, backend trình sửa ALIASEDIT
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-001, INT-101, NT-010, NT-011, NT-012, QA-006
- **Lệnh:** REINIT
- **Màn hình:** UI-K05
- **Đầu vào:** 03 §1 (đọc `.pgp` có sẵn của người dùng); 07.3 UI-K05 (nguồn mặc định, người dùng, nhập từ AutoCAD; cảnh báo xung đột; nhập gộp hoặc thay; xuất; đặt lại; áp dụng ngay); bảng alias mặc định INT-101.
- **Đầu ra:**
  - `cmd/pgp/PgpParser.h/.cpp`: dòng alias `X, *LỆNH`; dòng lệnh ngoài được đọc và giữ khi ghi lại nhưng không thực thi **[đề xuất]**; nhận diện UTF-8/ANSI (NT-012); giới hạn kích thước.
  - `cmd/pgp/AliasStore.h/.cpp`: lớp nguồn (mặc định → nhập từ AutoCAD → người dùng); phát hiện xung đột (trùng alias, che tên lệnh, trỏ lệnh không có); nhập gộp/thay, xuất `.pgp`, đặt lại; áp vào registry INT-001 ngay.
  - Lệnh REINIT nạp lại; fuzz target; `docs/specs/commands/REINIT.md`.
- **Tiêu chí nghiệm thu:**
  1. 50 tệp `acad.pgp` thật của người dùng pilot (CRLF/LF, BOM, chú thích có dấu): đọc đúng 100% alias; ghi rồi đọc lại không đổi ngữ nghĩa **[đề xuất]**.
  2. Phát hiện 100% xung đột trong bộ ca vàng, báo dòng và lý do.
  3. Sau REINIT hoặc lưu từ ALIASEDIT, alias mới dùng được ngay; 2.000 alias áp dụng và phát `registryChanged` ≤ 50 ms.
  4. Fuzz ≥ 1 giờ không lỗi; tệp > 1 MB hoặc dòng > 4 KB bị từ chối có thông báo **[đề xuất]**.
  5. Đạt DoD-C.

### INT-007 · Lệnh SCRIPT, RSCRIPT, DELAY, RESUME
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-109, INT-005, INT-003, NT-011
- **Lệnh:** SCRIPT, RSCRIPT, DELAY, RESUME
- **Màn hình:** UI-K04
- **Đầu vào:** 03 §18; 07.3 UI-K04 (chọn tệp `.scr`; dòng đang chạy, tiến trình, Esc để dừng; lỗi có số dòng và nội dung); bộ chạy INT-109.
- **Đầu ra:**
  - `cmd/script/ScriptCommands.cpp`: SCRIPT (FILEDIA = 1 và nguồn tương tác → yêu cầu UI mở hộp chọn tệp; còn lại hỏi tên tệp trên dòng lệnh; tìm theo đường dẫn hỗ trợ NT-011); RSCRIPT chạy lại từ đầu; DELAY 0–32767 ms không chặn luồng UI; RESUME tiếp sau Esc hoặc Backspace.
  - Trạng thái chạy, tạm dừng, chờ, xong cho UI-K04; SCRIPT gọi trong script theo spec; `docs/specs/commands/SCRIPT.md`, `RSCRIPT.md`, `DELAY.md`, `RESUME.md`.
- **Tiêu chí nghiệm thu:**
  1. Script có RSCRIPT lặp đến khi Esc; RESUME tiếp đúng token kế tiếp; đúng 100% ca vàng.
  2. DELAY 1000 lệch ≤ 20 ms, CPU ≤ 1% trong lúc chờ (DoD-H 6) **[đề xuất]**.
  3. Esc dừng script trong ≤ 16 ms; UI-K04 nhận dòng đang chạy; 50 script lỗi báo đúng số dòng và nội dung.
  4. SCRIPT trong `.scr` và qua `(command "SCRIPT" …)` hỏi tên tệp trên dòng lệnh, không mở hộp thoại; khớp 100% bộ tham chiếu QA-007 phần script khi có đáp án.
  5. Đạt DoD-C, DoD-L.

## 7. DIESEL và máy tính

### INT-112 · DIESEL và MODEMACRO
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** INT-107, RGN-013, NT-010, QA-006
- **Đầu vào:** 03 §19 (DIESEL, macro menu, MODEMACRO: P1); README §5.10 CUS-001 (macro `$M=` dùng DIESEL); tài liệu DIESEL công khai của AutoCAD.
- **Đầu ra:**
  - `cmd/calc/Diesel.h/.cpp`: parser và bộ tính đủ hàm DIESEL (số học, so sánh, `and`/`or`/`xor`, `if`, `eq`, `eval`, `fix`, `index`, `nth`, `strlen`, `substr`, `upper`, `getvar`, `getenv`, `rtos`, `angtos`, `edtime`, `linelen`); chuỗi lỗi như AutoCAD (`$?`, `$(tên,??)`); giới hạn độ sâu và độ dài.
  - MODEMACRO: chuỗi đã tính cho thanh trạng thái, tính lại khi sysvar liên quan đổi; API `evaluate()` cho CUS-001; fuzz target; `docs/specs/diesel.md`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 300 biểu thức ca vàng (gồm macro `$M=` lấy từ CUIX của người dùng pilot) cho kết quả đúng từng ký tự.
  2. Lỗi cú pháp, hàm lạ, sai số đối số trả chuỗi lỗi như AutoCAD, không ném ngoại lệ; lồng quá 256 mức hoặc kết quả quá 4 KB bị cắt kèm cảnh báo **[đề xuất]**.
  3. MODEMACRO chỉ tính lại khi biến nó đọc thay đổi; mỗi lần ≤ 0,1 ms **[đề xuất]**.
  4. Fuzz ≥ 1 giờ không lỗi.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** thực thi macro menu (CUS-001); vẽ thanh trạng thái (UIA).

### INT-113 · Engine biểu thức CAL: số, vector, điểm, đơn vị, hàm hình học, hàm bắt điểm
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** INT-105, INT-004, INT-106, INT-010, GEO-004, QA-006
- **Đầu vào:** 03 §3 (CAL: P1); 07.1 §8 (`'CAL` tính ngay trong lệnh, kể cả hàm lấy điểm bắt); 07.2 UI-E10 dòng `'CAL` (end, mid, cen, int, nea, per, qua, tan; rad, cur); tài liệu CAL công khai của AutoCAD.
- **Đầu ra:**
  - `cmd/calc/Cal.h/.cpp`: parser (số, feet-inch và góc qua INT-105, vector `[x,y,z]`, cực `[d<a]`, `@`); toán tử số học và vector (tích vô hướng, tích có hướng); hàm lượng giác qua GEO-004, `sqrt`, `abs`, `round`, `trunc`, `r2d`, `d2r`, `pi`, `dist`, `dpl`, `ang`, `pld`, `plt`, `rot`, `ill`, `nor`, `vec`, `vec1`, lối tắt `dee`, `mee`, `ille`; biến trong phiên.
  - Hàm bắt điểm chạy prompt điểm với OSNAP một lần (INT-010); `rad`, `cur`; fuzz target; `docs/specs/cal.md`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 500 biểu thức ca vàng đúng đến 1e-12 tương đối; kết quả giống từng bit trên 3 hệ điều hành.
  2. `(mid+cen)/2`, `ill(end,end,end,end)`, `dist(end,end)` trên bản vẽ mẫu với chuỗi pick mô phỏng cho kết quả đúng 100%.
  3. Lỗi cú pháp, chia cho 0, sai kiểu: thông báo có vị trí ký tự, không hủy lệnh ngoài.
  4. Một biểu thức không pick ≤ 20 µs p99 **[đề xuất]**; fuzz ≥ 1 giờ không lỗi.
  5. Đạt DoD-C.

### INT-022 · Lệnh CAL trong suốt (`'CAL`), nghiệm thu DIESEL và MODEMACRO
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** INT-113, INT-112, INT-003, INT-002
- **Lệnh:** CAL
- **Đầu vào:** README §5.9 INT-022; 07.1 §8 (máy tính `'CAL`); 07.2 UI-E10 dòng `'CAL`; engine INT-113, INT-112.
- **Đầu ra:**
  - Lệnh CAL (`cmd/calc/CalCommand.cpp`): gọi thường in kết quả; `'CAL` giữa prompt điểm, khoảng cách, góc, số → kết quả thành input của prompt ngoài; sai kiểu thì báo và hỏi lại; lịch sử biểu thức cho ↑/↓.
  - Ca vàng đầu-cuối cho MODEMACRO trên mô hình thanh trạng thái; `docs/specs/commands/CAL.md`.
- **Tiêu chí nghiệm thu:**
  1. Prompt điểm → `'CAL` → `(mid+end)/2` cho điểm đúng; prompt khoảng cách → `dist(cen,end)/2` đúng; 100% trên 60 kịch bản lệnh giả lập **[đề xuất]**.
  2. Vector trả cho prompt số: báo lỗi, quay lại prompt ngoài, lệnh ngoài không bị hủy.
  3. Cùng kết quả khi gọi từ `.scr`, `(command "'CAL" …)` và gõ tay.
  4. 50 chuỗi MODEMACRO mẫu hiện đúng trên mô hình thanh trạng thái khi sysvar đổi.
  5. Đạt DoD-C, DoD-L.

### INT-114 · Engine QUICKCALC: lịch sử, biến, quy đổi đơn vị, lấy giá trị từ bản vẽ
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** INT-022, INT-113, NT-010
- **Lệnh:** QUICKCALC
- **Màn hình:** UI-I10
- **Đầu vào:** 07.2 UI-I10 (biểu thức kèm lịch sử, hàm khoa học, biến người dùng; lấy tọa độ, khoảng cách, góc, giao điểm hai đường qua 4 điểm; quy đổi độ dài, diện tích, thể tích, góc; dán vào dòng lệnh, áp vào ô Thuộc tính); 03 §3 (QUICKCALC: P2).
- **Đầu ra:**
  - `cmd/calc/QuickCalc.h/.cpp`: mô hình cho palette UI-I10 trên engine CAL: lịch sử, biến và hằng người dùng lưu theo profile (NT-010), bảng quy đổi đơn vị, hành động lấy từ bản vẽ chạy như prompt.
  - Đưa kết quả: token vào pipeline (dán vào dòng lệnh) hoặc sự kiện áp vào ô Thuộc tính; lệnh QUICKCALC (yêu cầu UI mở palette, gọi trong suốt được); `docs/specs/commands/QUICKCALC.md`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 200 cặp quy đổi đơn vị đúng đến 1e-12 tương đối so với hệ số SI.
  2. Giao điểm hai đường qua 4 điểm pick mô phỏng đúng theo `Tolerance` (GEO-002); hai đường song song báo "không có giao điểm".
  3. Biến người dùng còn sau khởi động lại; `'QUICKCALC` giữa lệnh đưa kết quả về prompt ngoài như `'CAL`.
  4. Đạt DoD-C.

## 8. Chỉ mục không gian và hit test

### INT-151 · Chỉ mục không gian của bản vẽ trên snapshot
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** GEO-017, DOC-007, DOC-009, DOC-011, DOC-012, RGN-003, NT-005
- **Đầu vào:** README §4.2 (BVH dùng chung, INT giữ chỉ mục trên bản vẽ); 05 §9.1 (một chỉ mục cho chọn và OSNAP); API GEO-017, snapshot DOC-009, sự kiện DOC-007; hộp bao từ cache regen RGN-003; QA-012.
- **Đầu ra:**
  - `interact/index/SpatialIndex.{h,cpp}`: chỉ mục theo không gian (Model, từng layout) và theo định nghĩa block; truy vấn hộp, điểm, đa giác trả `ObjectId`; phiên bản gắn snapshot; cập nhật gia tăng theo lô sự kiện.
  - `interact/tests/index/`, `interact/bench/index_bench.cpp`, `docs/specs/interact/spatial-index.md`.
- **Tiêu chí nghiệm thu:**
  1. Dựng chỉ mục 1 triệu entity (BENCH-STD) ≤ 200 ms trên máy tầm trung **[đề xuất]**; hiệu suất song song ≥ 70% đến 16 luồng (05 §1.1, dựng BVH).
  2. Cập nhật gia tăng: 1 entity ≤ 50 µs, lô 10.000 entity ≤ 10 ms **[đề xuất]**; sau 10⁵ chuỗi sửa ngẫu nhiên, kết quả truy vấn trùng 100% với chỉ mục dựng lại từ đầu.
  3. INSERT/MINSERT truy vấn qua ma trận vào BVH của định nghĩa block, không làm phẳng; block lồng 10 cấp đúng.
  4. XLINE, RAY (không có hộp bao hữu hạn) nằm ở danh sách riêng; mọi truy vấn vẫn trả đúng.
  5. Nhiều luồng đọc snapshot cũ trong khi dựng bản mới; TSan sạch; kết quả tất định theo mọi số luồng.
  6. Đạt DoD-C, DoD-H.

### INT-152 · Vị từ va chạm cho entity phức qua primitive hiển thị
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-151, GEO-010, RGN-003, RGN-018, DOC-012
- **Đầu vào:** 05 §9.1 (chọn chạy trên CPU, kiểm va chạm chính xác); vị từ vùng GEO-108 qua `selectByRegion` của GEO-010; điểm gần nhất GEO-005/006/007; primitive hiển thị RGN-001/RGN-003; BVH định nghĩa block của INT-151.
- **Đầu ra:** `interact/hittest/ComplexPredicates.{h,cpp}`: `nearPoint`, `insideRegion`, `crossesRegion` cho TEXT, MTEXT, INSERT/MINSERT lồng, xref có XCLIP, kích thước, leader, mleader, bảng, hatch (vùng tô, nét mẫu, biên), SOLID, TRACE, WIPEOUT, ảnh và underlay (theo IMAGEFRAME), viewport, MLINE, REGION, proxy; đường cong cơ bản gọi thẳng GEO, không viết lại; test.
- **Tiêu chí nghiệm thu:**
  1. Ca vàng ≥ 600 ca trên 18 loại entity đạt 100% cho cả ba vị từ; khớp QA-007 khi có đáp án.
  2. INSERT lồng 10 cấp, tỉ lệ không đều, đối xứng: kiểm qua ma trận vào BVH định nghĩa block, không làm phẳng; block 10⁵ entity con: pick ≤ 0,3 ms **[đề xuất]**.
  3. Hatch 10⁵ nét mẫu: pick ≤ 0,5 ms **[đề xuất]**; hatch đặc trúng khi bấm bên trong vùng tô.
  4. Đường có linetype trúng cả ở khe hở (xét hình học, không xét nét); phần bị XCLIP hoặc clip viewport cắt bỏ thì không trúng.
  5. Đạt DoD-C, DoD-H.

### INT-009 · Hit test chính xác tại con trỏ trên BVH, ≤ 1 ms
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-151, INT-152, GEO-005, GEO-006, GEO-007, GEO-017, DOC-020, QA-012
- **Đầu vào:** README §5.9; 05 §1.1, §9.1; 07.1 §9.3, §9.4; điểm gần nhất GEO-005/006/007 cho đường cong cơ bản; vị từ entity phức INT-152; cảnh QA-012 (1 triệu entity, hatch dày, text dày, block lồng).
- **Đầu ra:**
  - `interact/hittest/HitTester.{h,cpp}`: `pick(viewport, screenPt, pickboxPx, PickOptions) → HitList` có thứ tự tất định; duyệt BVH, gọi GEO cho đường cong cơ bản và INT-152 cho entity phức; lọc theo trạng thái layer, viewport, không gian; móc lọc hiển thị theo phiên (INT-021 dùng).
  - `docs/specs/interact/hit-test.md`, `interact/bench/hittest_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Pick tại con trỏ ≤ 1 ms p99 máy tầm trung, ≤ 3 ms máy thấp trên BENCH-STD 1 triệu entity và ba cảnh hatch dày, text dày, block lồng.
  2. Chỉ trúng cái nhìn thấy: bỏ layer tắt, đóng băng, VP Freeze của viewport hiện hành, phần ngoài clip viewport và XCLIP; đối tượng trên layer khóa trả về kèm cờ `locked`.
  3. Đối tượng chồng nhau: thứ tự theo draw order khi SORTENTS bật bit chọn đối tượng, còn lại theo quy tắc ghi trong spec; giống nhau ở mọi số luồng.
  4. PICKBOX (pixel) quy ra đơn vị thế giới theo từng viewport, đúng ở DPI 100–200%.
  5. Ca vàng ≥ 1.000 cú pick trên 25 loại entity đạt 100%; khớp 100% QA-007 khi có đáp án.
  6. Đạt DoD-C, DoD-H, DoD-L.

## 9. Engine chọn đối tượng

### INT-153 · Truy vấn vùng song song: Window, Crossing, WPolygon, CPolygon, Fence, All, Last
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-009, INT-152, GEO-010, NT-005
- **Đầu vào:** 03 §4; 07.1 §9.3 (Window lấy đối tượng nằm trọn, Crossing lấy cả đối tượng chạm khung); `selectByRegion` của GEO-010 (vị từ GEO-108) cho đường cong cơ bản, INT-152 cho entity phức, bộ lọc trạng thái của INT-009; BENCH-STD.
- **Đầu ra:** `interact/select/AreaQuery.{h,cpp}`; `interact/tests/select/area/`; `interact/bench/area_query_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Window và WPolygon chỉ lấy đối tượng nằm trọn; Crossing và CPolygon thêm đối tượng cắt biên; Fence lấy đối tượng cắt đường fence; XLINE, RAY không bao giờ nằm trọn; INSERT xét mọi hình nhìn thấy, kể cả attribute.
  2. All lấy mọi đối tượng của không gian hiện hành trừ layer đóng băng (layer tắt vẫn được lấy); Last lấy đối tượng nhìn thấy được tạo gần nhất trong không gian hiện hành.
  3. Nút BVH nằm trọn trong vùng nhận cả cây con, không kiểm từng entity: Window bao toàn bộ 1 triệu entity ≤ 30 ms, khung nhỏ ≤ 2 ms trên máy tầm trung **[đề xuất]**; hiệu suất song song ≥ 60% đến 8 luồng.
  4. Trùng 100% với duyệt vét cạn trên 10⁴ vùng ngẫu nhiên (QA-009); tất định ở mọi số luồng.
  5. Đạt DoD-C, DoD-H, DoD-L.

### INT-154 · Bộ lọc kiểu `ssget` dùng chung
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-017, LSP-004, NT-005
- **Đầu vào:** 03 §4 (FILTER là nền của `ssget` có bộ lọc); 05 §11; mã nhóm DXF từ metadata DOC-017; `lisp::Wildcard` chế độ không phân biệt hoa thường (LSP-004); hợp đồng với LSP-009 (dịch danh sách lọc LISP sang vị từ này).
- **Đầu ra:** `interact/select/Filter.{h,cpp}`: cây vị từ (mã nhóm; `-4` `<AND <OR <XOR <NOT`; so sánh `= != /= <> < <= > >= & &= *`; so điểm từng tọa độ `">,>,*"`; `-3` theo ứng dụng xdata; danh sách loại `"LINE,ARC"`; 67/410 theo không gian), biên dịch một lần, đánh giá song song trên cột SoA; `docs/specs/interact/selection-filter.md`. Dùng chung cho LSP-009, FILTER, QSELECT, SELECTSIMILAR.
- **Tiêu chí nghiệm thu:**
  1. Ca vàng ≥ 2.000 bộ lọc tự soạn đạt 100%; các LISP dùng `ssget` có bộ lọc trong QA-008 cho kết quả đúng khi chạy qua LSP-009.
  2. Biên: `(62 . 1)` không khớp đối tượng ByLayer trên layer đỏ; `(62 . 256)` khớp ByLayer; tên layer, block so không phân biệt hoa thường, đúng với tiếng Việt có dấu; `-4` lồng sai trả lỗi kèm vị trí.
  3. Lọc theo loại và layer trên 1 triệu entity ≤ 20 ms máy tầm trung **[đề xuất]**; hiệu suất song song ≥ 60% đến 8 luồng.
  4. Đạt DoD-C, DoD-H.

### INT-155 · Phiên "Chọn đối tượng" và xem trước vùng chọn
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** INT-153, INT-002, INT-003, INT-004, RND-012, RND-013
- **Đầu vào:** 03 §4; 07.1 §9.1, §9.3, §9.4; Tài liệu trợ giúp công khai của AutoCAD (SELECT, PICKAUTO, PICKDRAG, PICKADD, PICKBOX, SELECTIONPREVIEW, PREVIEWFILTER); prompt INT-003.
- **Đầu ra:** `interact/select/SelectSession.{h,cpp}`: prompt "Chọn đối tượng:" đủ tùy chọn Window, Last, Crossing, BOX, ALL, Fence, WPolygon, CPolygon, Group, Add, Remove, Multiple, Previous, Undo, AUto, SIngle; implied windowing; khung chọn và tô sáng xem trước qua RND-012/RND-013; `docs/specs/interact/select-session.md`.
- **Tiêu chí nghiệm thu:**
  1. Mọi tùy chọn, kể cả dạng `_W`, cho cùng vùng chọn khi nhập từ bàn phím, script và LISP qua INT-002; ca vàng ≥ 300 kịch bản; khớp QA-007 khi có đáp án.
  2. PICKAUTO, PICKDRAG 0/1/2, PICKADD 0/1/2 (Shift để bớt hoặc thêm) đúng ngữ nghĩa AutoCAD; kéo trái sang phải là Window, phải sang trái là Crossing; WPolygon, CPolygon từ chối điểm làm đa giác tự cắt.
  3. Dòng lệnh báo số tìm thấy, số trùng, số trên layer khóa, số không thuộc không gian hiện hành; phát sự kiện số lượng cho bộ đếm UI.
  4. Xem trước (SELECTIONPREVIEW, PREVIEWFILTER): tô sáng khi rê và trong khung đang kéo; kéo khung trên 1 triệu entity ≤ 8 ms/khung máy tầm trung, ≤ 16 ms máy thấp.
  5. Đạt DoD-C, DoD-H, DoD-L.
- **Ngoài phạm vi:** SUbobject, Object (3D).

### INT-008 · Engine chọn: vùng chọn hiện hành, PICKFIRST, PICKSTYLE và API chung
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-154, INT-155, INT-001, INT-005, DOC-018, DOC-020, GEO-010
- **Đầu vào:** README §5.9; 03 §4 (PICKFIRST, PICKADD, nhóm, PICKSTYLE); 07.1 §9.3; cờ PICKFIRST trong metadata INT-001; hợp đồng LSP-009 (chế độ `ssget`, cờ `:E :S :N :L :D :$ +.`).
- **Đầu ra:** `interact/select/Selector.{h,cpp}`: `select(SelectRequest) → SelectionSet` cho lệnh, LSP và UI; vùng chọn hiện hành theo bản vẽ; Previous, Last; PICKFIRST; PICKSTYLE; sự kiện đổi vùng chọn kèm số lượng theo loại; `docs/specs/interact/selection.md`. Lasso ở INT-156.
- **Tiêu chí nghiệm thu:**
  1. PICKFIRST: lệnh có cờ dùng vùng chọn sẵn, không hỏi lại; lệnh không có cờ xóa vùng chọn; Esc xóa vùng chọn; Previous giữ qua lệnh, bỏ đối tượng đã xóa, bỏ qua khi đổi không gian.
  2. PICKSTYLE 0–3: bấm một thành viên chọn cả nhóm (nhóm cho phép chọn); hatch liên kết kéo theo biên; Ctrl+Shift+A đổi bit nhóm.
  3. Mọi chế độ và cờ `ssget` của LSP-009 ánh xạ được vào `SelectRequest`; thứ tự phần tử tất định, đúng trên QA-008.
  4. Vùng chọn 1 triệu đối tượng: tạo, hợp, trừ ≤ 10 ms **[đề xuất]**; sự kiện đổi vùng chọn ≤ 10.000 đối tượng, phần đồng bộ ≤ 16 ms.
  5. Đạt DoD-C, DoD-H, DoD-L.

### INT-156 · Chọn bằng lasso
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** INT-008
- **Đầu vào:** 03 §4 (Lasso P1); 07.1 §9.3; Tài liệu trợ giúp công khai của AutoCAD (PICKAUTO, cho phép nhấn-kéo lasso).
- **Đầu ra:** `interact/select/Lasso.{h,cpp}`: lấy mẫu đường con trỏ, giản lược đa giác, Space luân phiên Window/Crossing/Fence, gọi truy vấn INT-153; test.
- **Tiêu chí nghiệm thu:**
  1. Nhấn-kéo trên vùng trống bắt đầu lasso khi tùy chọn bật; kéo sang phải là Window, sang trái là Crossing; Space luân phiên ba chế độ.
  2. Đường lasso giản lược ≤ 512 đỉnh, lệch ≤ 1 px **[đề xuất]**; lasso tự cắt xử lý theo quy tắc ghi trong spec.
  3. Kết quả trùng WPolygon, CPolygon, Fence cùng đa giác trên 10⁴ ca ngẫu nhiên.
  4. Xem trước ≤ 8 ms/khung trên 1 triệu entity.
  5. Đạt DoD-C, DoD-L.

## 10. Truy bắt điểm (OSNAP)

### INT-157 · Chế độ bắt trên một đối tượng: END, MID, CEN, GCE, NOD, QUA, INS, NEA, PER, TAN
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-151, GEO-005, GEO-006, GEO-007, GEO-021
- **Đầu vào:** 03 §3; 07.1 §9.6; Tài liệu trợ giúp công khai của AutoCAD (OSNAP, OSMODE, OSNAPHATCH, OSNAPZ, OSNAPNODELEGACY); điểm gần nhất, chân vuông góc, tiếp điểm của GEO-005, GEO-006, GEO-007; trọng tâm GEO-021 cho GCE.
- **Đầu ra:** `interact/osnap/SnapPoints.{h,cpp}`: sinh điểm ứng viên theo chế độ cho line, arc, circle, ellipse, polyline (từng đoạn), spline, point, TEXT, MTEXT, attribute, INSERT, kích thước, hatch; PER, TAN tính từ điểm gốc của prompt, chưa có điểm gốc thì trả dữ liệu hoãn cho INT-158; `docs/specs/interact/osnap-modes.md`; test.
- **Tiêu chí nghiệm thu:**
  1. Bảng ca vàng chế độ × loại entity (≥ 500 ca) đạt 100%; điểm tính bằng `double` trong WCS, sai số ≤ 1e-9 tương đối **[đề xuất]**.
  2. Biên: QUA của cung chỉ lấy góc phần tư nằm trên cung; MID của polyline cho từng đoạn, kể cả đoạn cung; polyline kín không lặp đỉnh đầu; GCE (P1) chỉ cho polyline, spline kín; NOD gồm POINT, điểm định nghĩa kích thước, MTEXT theo OSNAPNODELEGACY.
  3. PER, TAN: nhiều nghiệm thì chọn nghiệm gần con trỏ; PER rơi trên phần kéo dài của line, cung theo quy tắc AutoCAD ghi trong spec.
  4. Hatch bị bỏ qua khi OSNAPHATCH = 0; OSNAPZ thay Z bằng cao độ hiện hành.
  5. Sinh ứng viên cho 64 đối tượng trong ô bắt ≤ 0,3 ms máy tầm trung **[đề xuất]**.
  6. Đạt DoD-C, DoD-L.

### INT-010 · OSNAP: giao điểm, xếp hạng như AutoCAD, Tab luân phiên, ghi đè một lần
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** INT-157, INT-009, INT-004, INT-005, GEO-005, GEO-006, GEO-007, GEO-010, GEO-017
- **Đầu vào:** README §5.9; 03 §3 (giống hệt hành vi và thứ tự ưu tiên); 05 §10; 07.1 §9.6, §10 (F3, danh sách chế độ); Tài liệu trợ giúp công khai của AutoCAD (OSMODE, APERTURE, APBOX, AUTOSNAP, OSNAPCOORD); parser INT-004 (FROM, M2P, bộ lọc `.X .Y .Z .XY .XZ .YZ`).
- **Đầu ra:**
  - `interact/osnap/OsnapEngine.{h,cpp}`: `query(viewport, cursor, ctx) → SnapResult` (điểm, chế độ, đối tượng, dữ liệu marker và tooltip cho RND-012, UIA-013).
  - INT gồm giao kéo dài khi chọn lần lượt hai đối tượng; chế độ chạy OSMODE và F3; ghi đè một lần gõ tay (END, MID, CEN, GCE, NOD, QUA, INT, INS, PER, TAN, NEA, NON/NONE; EXT, TT ở INT-161; APP, PAR ở INT-160); điểm bắt dùng được trong chuỗi FROM, M2P và bộ lọc tọa độ của INT-004.
  - `docs/specs/interact/osnap-ranking.md`, `interact/bench/osnap_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Truy vấn OSNAP ≤ 1 ms p99 máy tầm trung, ≤ 3 ms máy thấp trên BENCH-STD 1 triệu entity; vùng dày xét tối đa 64 đối tượng gần con trỏ nhất, giới hạn theo số lượng để tất định **[đề xuất]**.
  2. Xếp hạng theo bảng quy tắc trong spec (khoảng cách trong APERTURE, NEA thấp nhất…): ca vàng ≥ 500 vị trí con trỏ nhiều đối tượng, nhiều chế độ đạt 100%; khớp 100% QA-007 khi có đáp án.
  3. Tab duyệt lần lượt mọi điểm khả dĩ trong ô bắt theo thứ tự xếp hạng, quay vòng; ghi đè một lần chỉ áp cho lần lấy điểm kế tiếp.
  4. OSNAPCOORD 0/1/2 đúng giữa tọa độ gõ tay, script và OSNAP chạy.
  5. INT đúng cho line, cung, tròn, ellipse, polyline, spline, cả giao thật và giao kéo dài, trên ca vàng ≥ 300 ca.
  6. Đạt DoD-C, DoD-H, DoD-L.

### INT-158 · PER, TAN hoãn: giải cặp ràng buộc giữa hai đối tượng
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** INT-010, INT-012, GEO-020
- **Đầu vào:** 03 §3; 07.1 §9.6; tiếp tuyến chung và đường vuông góc giữa hai đường cong của GEO-020 (TAN/PER trì hoãn).
- **Đầu ra:** `interact/osnap/DeferredSnap.{h,cpp}`: PER hoặc TAN chọn khi chưa có điểm gốc trả `DeferredSnap` (chế độ, đối tượng, điểm bấm) kèm marker hoãn; giải khi biết điểm thứ hai hoặc khi cả hai đầu cùng hoãn; API cho lệnh (LINE, CIRCLE TTR, TTT) và jig; test.
- **Tiêu chí nghiệm thu:**
  1. LINE TAN–TAN với hai đường tròn cho tiếp tuyến chung gần hai điểm bấm trong 4 nghiệm; PER–PER, TAN–PER, kể cả ellipse và spline: ca vàng ≥ 200 ca đạt 100%, sai số ≤ 1e-9 tương đối.
  2. Không có nghiệm thì báo trên dòng lệnh, giữ prompt, không ghi bản vẽ.
  3. Rubber-band của cặp hoãn cập nhật ≤ 8 ms/khung máy tầm trung.
  4. Đạt DoD-C, DoD-L.

### INT-159 · Bắt điểm vào block, xref và kích thước
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-010, DOC-012, DOC-013, GEO-022, RGN-018, RGN-024
- **Đầu vào:** README §5.9 (bắt vào block và xref ở EA); 07.1 §9.6; DOC-012, DOC-013; XCLIP và VP Freeze của RGN-024.
- **Đầu ra:** `interact/osnap/NestedSnap.{h,cpp}`: duyệt lồng qua ma trận INSERT, MINSERT, xref, kích thước, leader, bảng, `*U` của dynamic block; mọi chế độ của INT-157, INT-010 chạy trên hình con; test.
- **Tiêu chí nghiệm thu:**
  1. Block lồng 10 cấp, tỉ lệ không đều (tròn thành ellipse qua GEO-022), đối xứng: điểm bắt đúng trên ca vàng ≥ 300 ca.
  2. Xref có XCLIP chỉ bắt phần nhìn thấy; hình trên layer tắt hoặc đóng băng của xref bị bỏ.
  3. INS của INSERT và attribute; NOD điểm định nghĩa kích thước; INT giữa hình trong block và hình ngoài block.
  4. ≤ 1 ms p99 máy tầm trung trên cảnh block lồng QA-012.
  5. Đạt DoD-C, DoD-L.

### INT-160 · APP, PAR và bắt vào PDF underlay
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** INT-010, INT-161, PLT-011, RGN-022
- **Đầu vào:** 03 §3 (APP, PAR P1); 07.1 §9.6 (1.0: Apparent, Parallel, bắt vào underlay); hình học trích từ PDF qua PLT-011; RGN-022.
- **Đầu ra:** `interact/osnap/{Apparent,Parallel,UnderlaySnap}.{h,cpp}`; test.
- **Tiêu chí nghiệm thu:**
  1. APP: giao biểu kiến theo hướng nhìn hiện hành cho đối tượng khác cao độ, kể cả giao biểu kiến kéo dài; ca vàng ≥ 100 ca.
  2. PAR: rê lên đoạn thẳng để lấy hướng; đường theo dõi song song hiện khi rubber-band gần song song; ca vàng ≥ 100 ca.
  3. Bắt vào PDF underlay khi PDFOSNAP bật: END, MID, CEN, QUA, INT, NEA, PER trên hình trích; tôn trọng clip underlay.
  4. ≤ 1 ms p99 máy tầm trung trên underlay 10.000 đoạn **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.

## 11. Theo dõi và hỗ trợ vẽ

### INT-161 · OTRACK, EXT và điểm theo dõi tạm
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-010, GEO-008, RGN-013
- **Đầu vào:** 03 §3; 05 §10 (điểm theo dõi, phần kéo dài); 07.1 §9.6 (lấy điểm, đường theo dõi, tooltip); Tài liệu trợ giúp công khai của AutoCAD (POLARMODE, AUTOSNAP, TRACKPATH).
- **Đầu ra:** `interact/track/ObjectTracking.{h,cpp}`: lấy điểm khi dừng trên điểm bắt (hoặc Shift theo POLARMODE), tối đa 7 điểm, bỏ khi rê lại; đường theo dõi trực giao hoặc theo góc polar; giao hai đường; EXT theo line và cung; ghi đè TT/TK; dữ liệu đường và tooltip ("Kéo dài: 1250.00 < 0°") định dạng qua RGN-013; test.
- **Tiêu chí nghiệm thu:**
  1. Ca vàng ≥ 300 chuỗi rê–dừng–bấm cho điểm đúng, sai số ≤ 1e-9 tương đối; khớp QA-007 khi có đáp án.
  2. EXT của cung chạy theo đường tròn chứa cung; giao "Polar và Kéo dài" và giao hai đường theo dõi đúng.
  3. Thời gian dừng để lấy điểm chỉnh được, mặc định 0,5 s **[đề xuất]**; con trỏ đứng yên thì không còn timer chạy.
  4. Cập nhật tracking mỗi lần di chuột ≤ 0,5 ms p99 máy tầm trung **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.

### INT-162 · SNAP, GRID (chữ nhật, isometric) và lệnh thiết lập hỗ trợ vẽ
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** INT-168, INT-170, INT-003, INT-005, INT-007, RND-020
- **Lệnh:** SNAP, GRID, ISODRAFT, ISOPLANE, OSNAP, -OSNAP, DSETTINGS
- **Màn hình:** UI-D09
- **Đầu vào:** 03 §3 (ISODRAFT P1); 07.1 §10 (F3, F5, F7, F9); 07.2 UI-D09; Tài liệu trợ giúp công khai của AutoCAD (SNAPUNIT, SNAPBASE, SNAPANG, SNAPSTYL, SNAPISOPAIR, SNAPTYPE, GRIDUNIT, GRIDMAJOR, GRIDDISPLAY, GRIDSTYLE, LIMMIN, LIMMAX).
- **Đầu ra:** `interact/track/GridSnap.{h,cpp}`: làm tròn điểm theo lưới chữ nhật và isometric trên mặt phẳng UCS; mô hình lưới (đường chính, phụ, thích ứng) cho RND-020; lệnh ở `interact/track/cmd/`; `docs/specs/commands/{SNAP,GRID,ISODRAFT,ISOPLANE,OSNAP,-OSNAP,DSETTINGS}.md`.
- **Tiêu chí nghiệm thu:**
  1. SNAP [ON/OFF/Aspect/Legacy/Style/Type], GRID [ON/OFF/Snap/Major/aDaptive/Limits/Follow/Aspect], ISODRAFT, ISOPLANE đủ tùy chọn; F5/Ctrl+E đổi mặt isometric; cùng kết quả từ bàn phím, `.scr` và `(command)`.
  2. -OSNAP nhận danh sách "END,MID,INT" và NON; OSNAP, DSETTINGS mở UI-D09 đúng tab qua móc giao diện; trong `trion-console` báo không có giao diện và kết thúc sạch.
  3. Lưới thích ứng: ≤ 4.000 đường trên màn hình ở mọi mức zoom **[đề xuất]**; dựng mô hình lưới ≤ 1 ms mỗi lần view đổi.
  4. Snap isometric 3 mặt, SNAPANG ≠ 0, SNAPBASE lệch gốc đúng trên ca vàng ≥ 200 ca.
  5. Đạt DoD-C, DoD-L.

### INT-011 · Chuỗi xử lý điểm: POLAR, ORTHO và tích hợp tracking
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-158, INT-161, INT-162, INT-012, INT-004, INT-007
- **Lệnh:** ORTHO
- **Đầu vào:** README §5.9; 03 §3 (POLAR F10, OTRACK F11, ORTHO F8); 07.1 §9.6, §10; 07.2 UI-D09 (tab Theo dõi polar); Tài liệu trợ giúp công khai của AutoCAD (POLARANG, POLARADDANG, POLARMODE, POLARDIST, ORTHOMODE, AUTOSNAP).
- **Đầu ra:** `interact/track/PointPipeline.{h,cpp}` cắm vào bộ lấy mẫu jig INT-012: thứ tự ưu tiên ghi trong spec (ghi đè một lần, OSNAP, OTRACK/POLAR, SNAP, ORTHO); POLAR (góc tăng, góc bổ sung, đo tương đối đoạn trước, polar snap theo POLARDIST); ORTHO theo UCS, SNAPANG và mặt isometric, giữ Shift để tạm đảo; nhập khoảng cách trực tiếp theo hướng đang theo dõi; `docs/specs/interact/point-pipeline.md`, `docs/specs/commands/ORTHO.md`.
- **Tiêu chí nghiệm thu:**
  1. Bật ORTHO thì tắt POLAR và ngược lại; F8, F10, F11 đổi ORTHOMODE và các bit AUTOSNAP đúng như AutoCAD.
  2. Ca vàng ≥ 500 tình huống phối hợp OSNAP, OTRACK, POLAR, SNAP, ORTHO đạt 100%; khớp 100% QA-007 khi có đáp án.
  3. Gõ "3000" khi đang theo polar 45° cho điểm cách 3000 theo 45°, sai số ≤ 1e-9 tương đối.
  4. Toàn chuỗi mỗi lần di chuột ≤ 2 ms p99 máy tầm trung, ≤ 5 ms máy thấp trên BENCH-STD 1 triệu entity **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.

## 12. Jig và xem trước trong lệnh

### INT-012 · Khung jig và hợp đồng xem trước trong lệnh
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-010, INT-002, INT-003, DOC-009, RND-012, NT-005
- **Đầu vào:** README §5.9; 05 §1.1 (jig ≤ 8 ms/khung), §9.1 (overlay riêng); 07.1 §9.8 (xem trước không ghi bản vẽ, Esc trả về như cũ); overlay RND-012; snapshot DOC-009.
- **Đầu ra:** `interact/jig/Jig.{h,cpp}`: hợp đồng `Jig` (lấy mẫu con trỏ qua chuỗi điểm → cập nhật → danh sách primitive overlay); lấy điểm, khoảng cách, góc có rubber-band từ điểm gốc; gộp sự kiện chuột, mỗi khung chỉ xử lý mẫu mới nhất; kiểu nét xem trước (rubber-band, bóng mờ, phần sẽ xóa, tô sáng); `docs/specs/interact/jig.md`; test, benchmark. Lệnh dùng ở V: DIST, AREA, MEASUREGEOM, ZOOM Window.
- **Tiêu chí nghiệm thu:**
  1. Jig chỉ nhận snapshot chỉ đọc; test chặn: chạy mọi jig đã đăng ký không mở transaction DOC-007 nào, không thêm bước undo.
  2. Esc trả overlay và trạng thái lệnh về như cũ; hash bản vẽ không đổi.
  3. Rubber-band line, rectangle, circle trên BENCH-STD 1 triệu entity ≤ 8 ms/khung p99 máy tầm trung, ≤ 16 ms máy thấp; phần CPU của jig ≤ 2 ms **[đề xuất]**.
  4. Chuột 1.000 Hz: mỗi khung xử lý đúng 1 mẫu; con trỏ đứng yên thì không vẽ lại (~0% CPU).
  5. Primitive overlay giữ tọa độ `double` đến bước trừ gốc của RND-011; không rung ở tọa độ ≈ 10⁹.
  6. Đạt DoD-C, DoD-H, DoD-L.

### INT-163 · Xem trước kéo vùng chọn đã biến đổi
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-012, INT-008, RND-009, RND-012, RGN-003, QA-005
- **Đầu vào:** 07.1 §9.8; 05 §9.1 (overlay, instancing); Tài liệu trợ giúp công khai của AutoCAD (DRAGMODE); dùng cho MOVE, COPY, ROTATE, SCALE, MIRROR, STRETCH và grip.
- **Đầu ra:** `interact/jig/TransformDrag.{h,cpp}`: vẽ lại batch GPU sẵn có của vùng chọn dưới một ma trận trên overlay, không regen mỗi khung; STRETCH regen riêng đối tượng bị kéo giãn; DRAGMODE ON/OFF/Auto; test, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Kéo 100.000 đối tượng đã chọn ≤ 8 ms/khung p99 máy tầm trung, ≤ 16 ms máy thấp **[đề xuất]**.
  2. STRETCH 10.000 đối tượng cắt khung ≤ 16 ms/khung máy tầm trung **[đề xuất]**; phần regen chạy song song trên NT-005.
  3. Ảnh xem trước khớp ảnh sau khi lệnh ghi bản vẽ (QA-005, SSIM ≥ 0,99) trên 50 cảnh.
  4. Không ghi bản vẽ; Esc trả về như cũ.
  5. Đạt DoD-C, DoD-H, DoD-L.

### INT-164 · Xem trước kết quả tính nền
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-012, NT-005, RGN-002, RGN-003
- **Đầu vào:** 07.1 §9.8 (TRIM, EXTEND, OFFSET, FILLET, HATCH, MATCHPROP); DoD-H 5 (việc tương tác ưu tiên cao, hủy nhanh); regen RGN-002, RGN-003.
- **Đầu ra:** `interact/jig/AsyncPreview.{h,cpp}`: hợp đồng `computePreview(snapshot, sample, cancel) → PreviewResult` là hàm thuần chạy trên NT-005 ưu tiên tương tác; mẫu mới nhất thắng, bỏ kết quả cũ theo số thứ tự mẫu; xem trước đổi thuộc tính bằng regen bản sao có thuộc tính ghi đè; lệnh CMD hiện thực xem trước của mình trên hợp đồng này; test.
- **Tiêu chí nghiệm thu:**
  1. Xem trước giả lập tốn 100 ms không làm khung nào vượt 8 ms; hủy khi con trỏ đi tiếp ≤ 50 ms.
  2. Lệnh mẫu kiểu TRIM trên cảnh 10.000 đối tượng cắt: xem trước hiện ≤ 50 ms sau khi con trỏ dừng **[đề xuất]**.
  3. Không bao giờ áp kết quả cũ hơn mẫu đang hiển thị; tất định với cùng chuỗi mẫu.
  4. Xem trước đổi thuộc tính (kiểu MATCHPROP) hiện khi rê, hash bản vẽ không đổi.
  5. Đạt DoD-C, DoD-H, DoD-L.

## 13. Grip

### INT-165 · Mô hình grip, trạng thái và API đăng ký
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-008, INT-005, RND-012, NT-005
- **Đầu vào:** 07.1 §9.5 (loại grip, trạng thái, giới hạn hiện grip); README §4.2 (module khác đăng ký grip qua API của INT-013); Tài liệu trợ giúp công khai của AutoCAD (GRIPS, GRIPOBJLIMIT, GRIPSIZE, GRIPBLOCK, GRIPMULTIFUNCTIONAL, GRIPTIPS).
- **Đầu ra:** `interact/grip/GripModel.{h,cpp}`, `interact/grip/GripRegistry.h`: `GripProvider` theo lớp entity (vị trí; hình vuông, chữ nhật, tam giác/mũi tên, tròn; mục menu đa năng; hàm thuần `apply(entity, grips, delta) → entity mới`); trạng thái nguội/rê/nóng; nhiều grip nóng (Shift+bấm); hit test grip; con trỏ hút vào grip khi kéo; khung menu đa năng (rê để hiện, Ctrl luân phiên khi kéo); `docs/specs/interact/grip-api.md`.
- **Tiêu chí nghiệm thu:**
  1. Chọn vượt GRIPOBJLIMIT (mặc định 1.000 theo 07.1 §9.5) thì không hiện grip; GRIPS 0/1/2 và GRIPBLOCK đúng.
  2. Thu thập grip cho 1.000 đối tượng ≤ 2 ms (song song); hit test grip ≤ 0,2 ms p99 máy tầm trung **[đề xuất]**.
  3. Provider giả của module khác đăng ký, hiện, kéo, hoàn tác được mà không sửa mã `interact/grip`.
  4. Trạng thái xuất đủ dữ liệu để UI phân biệt nguội/rê/nóng bằng màu và độ đậm.
  5. Đạt DoD-C, DoD-H, DoD-L.

### INT-166 · Grip của đường cong cơ bản và text
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** INT-165, GEO-005, GEO-006, GEO-007, GEO-022, RGN-007, RGN-009
- **Đầu vào:** 07.1 §9.5 (bảng grip theo loại); README §4.2.
- **Đầu ra:** provider ở `interact/grip/providers/` cho line, arc, circle, ellipse, cung ellipse, LWPOLYLINE, POLYLINE 2D, spline (điểm đi qua hoặc điểm điều khiển, tam giác chuyển chế độ), point, xline, ray, TEXT, ATTDEF, ATTRIB, MTEXT (điểm chèn, mũi tên đổi rộng và cao); test.
- **Tiêu chí nghiệm thu:**
  1. Vị trí grip đúng bảng 07.1 §9.5; ca vàng ≥ 400 thao tác kéo đạt 100%; khớp QA-007 khi có đáp án.
  2. Bất biến: kéo trung điểm line di chuyển cả đoạn; kéo tâm cung, tròn giữ bán kính; kéo góc phần tư tròn đổi bán kính; kéo đỉnh polyline giữ độ rộng và bulge theo quy tắc spec; TEXT căn Aligned/Fit có grip điểm thứ hai.
  3. Provider là hàm thuần: cùng đầu vào cho cùng entity từng bit; không ghi bản vẽ khi đang kéo.
  4. Polyline 10⁵ đỉnh: tạo grip ≤ 5 ms, kéo một đỉnh ≤ 8 ms/khung máy tầm trung **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.

### INT-013 · Khung grip: chế độ sửa, Space luân phiên, Sao chép, Điểm gốc, Tham chiếu
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-165, INT-166, INT-163, INT-003, DOC-008, GEO-022
- **Đầu vào:** README §5.9; 05 §10 (grip đa năng, Space luân phiên); 07.1 §9.5, §9.9 (menu grip nóng); design/README Quy ước (từ khóa ẩn MO, RO, SC, MI; mỗi lệnh một bước hoàn tác).
- **Đầu ra:** `interact/grip/GripEdit.{h,cpp}`: hiện grip khi chọn lúc không có lệnh; Kéo giãn → Di chuyển → Xoay → Tỉ lệ → Đối xứng bằng Space/Enter; từ khóa ẩn ST, MO, RO, SC, MI; tùy chọn Điểm gốc, Sao chép, Hoàn tác, Tham chiếu, Thoát; dữ liệu menu grip nóng cho INT-018; `docs/specs/interact/grip-edit.md`. Thao tác đa năng đầy đủ ở INT-167.
- **Tiêu chí nghiệm thu:**
  1. Prompt từng chế độ ("** KÉO GIÃN **" kèm tùy chọn) đúng spec; ca vàng ≥ 300 chuỗi thao tác bàn phím và chuột đạt 100%; khớp QA-007 khi có đáp án.
  2. Sao chép đặt nhiều bản; giữ Ctrl khi sao chép thì bắt theo bội số độ dời đầu tiên; Tham chiếu cho Xoay và Tỉ lệ đúng.
  3. Một phiên sửa bằng grip là một bước hoàn tác, kể cả nhiều bản sao; Esc hủy không để lại thay đổi.
  4. Di chuyển, Xoay, Tỉ lệ, Đối xứng cho hình học trùng biến đổi GEO-022 cùng ma trận.
  5. Kéo grip nóng khi đã chọn 1.000 đối tượng ≤ 8 ms/khung p99 máy tầm trung.
  6. Đạt DoD-C, DoD-L.

### INT-167 · Grip đa năng đầy đủ
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** INT-013, GEO-006, GEO-007, GEO-023, RGN-010
- **Đầu vào:** 07.1 §9.5 (menu đa năng, phát hành 1.0); Tài liệu trợ giúp công khai của AutoCAD (GRIPMULTIFUNCTIONAL).
- **Đầu ra:** thao tác đa năng trong `interact/grip/providers/`: polyline (Kéo giãn, Thêm đỉnh, Bớt đỉnh, Chuyển thành cung hoặc đoạn thẳng), line và cung (Kéo dài thêm, Bán kính), spline (Thêm/Bớt điểm, Tiếp tuyến), tay nắm cột MTEXT; test.
- **Tiêu chí nghiệm thu:**
  1. Menu hiện khi rê lên grip đa năng; Ctrl luân phiên lựa chọn khi đang kéo.
  2. Ca vàng ≥ 200 thao tác đạt 100%; khớp QA-007 khi có đáp án.
  3. Thêm, bớt đỉnh giữ độ rộng và bulge của đoạn kề theo spec; Chuyển thành cung đi qua điểm kéo.
  4. Mỗi thao tác là một bước hoàn tác.
  5. Đạt DoD-C, DoD-L.

## 14. Nhập động (DYN)

### INT-014 · Nhập động: nhập con trỏ, nhập kích thước, prompt động, khóa ô, kiểm tra hợp lệ
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** INT-012, INT-165, INT-003, INT-004, INT-017, RGN-013
- **Đầu vào:** README §5.9; 07.1 §9.7, §10 (F12); 07.2 UI-D09 (tab Nhập động); Tài liệu trợ giúp công khai của AutoCAD (DYNMODE, DYNPIFORMAT, DYNPICOORDS, DYNPIVIS, DYNDIVIS, DYNDIGRIP, DYNPROMPT).
- **Đầu ra:** `interact/dyn/DynInput.{h,cpp}`: mô hình nhập con trỏ (X/Y hoặc khoảng cách/góc, tuyệt đối/tương đối); nhập kích thước (ô độ dài, góc, bán kính gắn vào hình jig và grip); prompt động cạnh con trỏ, ↓ mở danh sách từ khóa INT-003; Tab chuyển ô, khóa ô, kiểm tra khi gõ; định tuyến phím giữa DYN và dòng lệnh INT-017; `docs/specs/interact/dyn.md`.
- **Tiêu chí nghiệm thu:**
  1. Cùng chuỗi giá trị nhập qua DYN và qua dòng lệnh cho bản vẽ trùng hash trên ≥ 200 kịch bản (LINE, PLINE, CIRCLE, ARC, RECTANG, grip kéo giãn).
  2. `#` tuyệt đối, `@` tương đối; điểm thứ hai mặc định tương đối theo DYNPICOORDS; ô đã gõ khóa giá trị và jig chịu ràng buộc của ô khóa.
  3. Giá trị sai định dạng cho trạng thái lỗi ngay khi gõ, không gửi vào lệnh; số hiển thị theo LUNITS, AUNITS qua RGN-013.
  4. F12 đổi DYNMODE (giá trị âm giữ thiết lập); DYNDIVIS 0/1/2, DYNDIGRIP đúng.
  5. Phím gõ đến khi ô cập nhật ≤ 16 ms máy tầm trung, ≤ 33 ms máy thấp.
  6. Đạt DoD-C, DoD-L.

## 15. Hệ tọa độ người dùng (UCS)

### INT-168 · Engine hệ tọa độ: WCS, UCS, OCS, DCS, PSDCS
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-001, GEO-004, DOC-004, DOC-010, DOC-011
- **Đầu vào:** 03 §3; 05 §10; DXF Reference (Autodesk), mục Arbitrary Axis Algorithm; bảng UCS (DOC-004); UCSORG, UCSXDIR, UCSYDIR (DOC-010); UCS theo viewport (DOC-011).
- **Đầu ra:** `interact/ucs/CoordSystems.{h,cpp}`: biến đổi WCS, UCS, OCS, DCS, PSDCS; UCS hiện hành theo viewport (UCSVP); ngăn xếp UCS trước đó; móc biến đổi tọa độ nhập (INT-004) và tọa độ hiển thị (thanh trạng thái, ID); API kiểu `trans` (mã 0–3, entity, cờ vector) cho LSP-011; test.
- **Tiêu chí nghiệm thu:**
  1. Khứ hồi WCS → UCS → WCS sai số ≤ 4 ulp theo độ lớn tọa độ trên 10⁶ ca ngẫu nhiên (QA-009), kể cả tọa độ ≈ 10⁹ **[đề xuất]**.
  2. OCS khớp Arbitrary Axis Algorithm trên ≥ 500 vector pháp tuyến, kể cả sát ngưỡng 1/64 quanh trục Z.
  3. Bản vẽ lưu UCS khác World: tọa độ hiển thị và kết quả ID trong Viewer theo UCS hiện hành.
  4. API `trans` cho kết quả đúng với các LISP dùng `trans` trong QA-008.
  5. Đạt DoD-C.

### INT-015 · UCS (Origin, 3P, Object, View, World), UCSICON, PLAN
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** INT-168, INT-170, INT-010, INT-008, INT-003, INT-007, RND-020, DOC-007
- **Lệnh:** UCS, UCSICON, PLAN
- **Đầu vào:** README §5.9; 03 §3 (UCS P0 cơ bản), §21 (PLAN); 07.1 §9.13 (biểu tượng UCS); Tài liệu trợ giúp công khai của AutoCAD (UCS, UCSICON, PLAN, UCSFOLLOW, UCSVP).
- **Đầu ra:** lệnh ở `interact/ucs/cmd/`; mô hình biểu tượng UCS (ở gốc hoặc ở góc, kiểu, theo viewport) cho RND-020; `docs/specs/commands/{UCS,UCSICON,PLAN}.md`. NAmed, X/Y/Z, ZAxis, UCS trực giao ở INT-169.
- **Tiêu chí nghiệm thu:**
  1. UCS: gốc; gốc + trục X + mặt XY (3 điểm); OBject theo quy tắc từng loại entity; View; World; Previous. Ca vàng ≥ 200 ca; khớp QA-007 khi có đáp án.
  2. UCSICON [ON/OFF/All/Noorigin/ORigin/Selectable/Properties] và biến UCSICON đúng; biểu tượng tự về góc khi gốc ra ngoài viewport; Properties mở hộp thoại qua móc giao diện.
  3. PLAN [Current ucs/Ucs/World] đặt view mặt bằng và zoom vừa bản vẽ; UCSFOLLOW tự chuyển mặt bằng khi đổi UCS.
  4. Đổi UCS là một bước hoàn tác; cùng kết quả từ bàn phím, `.scr` và `(command)`.
  5. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** UCS Face (3D).

### INT-169 · UCS đầy đủ và UCSMAN
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** INT-015
- **Lệnh:** UCSMAN
- **Màn hình:** UI-D13
- **Đầu vào:** 03 §3 (UCS P1 đầy đủ); 07.2 UI-D13; Tài liệu trợ giúp công khai của AutoCAD (UCS, UCSMAN, UCSBASE, UCSORTHO, UCSFOLLOW, UCSVP).
- **Đầu ra:** tùy chọn UCS NAmed (Save, Restore, Delete, ?), X/Y/Z, ZAxis, Apply; UCS trực giao (Top, Bottom, Front, Back, Left, Right theo UCSBASE, có độ sâu); mô hình dữ liệu cho UI-D13 (UCS đã đặt tên, UCS trực giao, tab Thiết lập); `docs/specs/commands/UCSMAN.md`, cập nhật `UCS.md`.
- **Tiêu chí nghiệm thu:**
  1. Ca vàng ≥ 150 ca cho mọi tùy chọn mới đạt 100%; khớp QA-007 khi có đáp án.
  2. UCS đã đặt tên ghi vào bảng UCS qua DOC-007; lưu DWG rồi đọc lại trùng từng trường.
  3. Đổi tên, xóa UCS đang dùng xử lý như AutoCAD (spec ghi rõ); mỗi thao tác một bước hoàn tác.
  4. Nạp mô hình UCSMAN với 1.000 UCS đã đặt tên ≤ 50 ms **[đề xuất]**.
  5. Đạt DoD-C.

## 16. Điều hướng

### INT-170 · Engine view: trạng thái, lịch sử, extents, lăn chuột, chuột giữa
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** INT-151, INT-168, INT-005, DOC-011
- **Đầu vào:** 03 §21; 05 §1.1 (pan/zoom ≥ 60 fps với 1 triệu entity); 07.1 §9.1, §9.2; Tài liệu trợ giúp công khai của AutoCAD (ZOOMFACTOR, ZOOMWHEEL, MBUTTONPAN, VTENABLE, VTDURATION, EXTMIN, EXTMAX, LIMMIN, LIMMAX).
- **Đầu ra:** `interact/nav/ViewEngine.{h,cpp}`: trạng thái view theo viewport (tâm, chiều cao, twist, hướng); ánh xạ màn hình ↔ thế giới bằng `double`; lịch sử view ≥ 10 bước mỗi viewport; extents cập nhật gia tăng từ INT-151; lăn chuột zoom quanh con trỏ; giữ chuột giữa để pan; nhấp đúp chuột giữa zoom extents; API `zoomAbout`, `panBy` cho cử chỉ touchpad (P1, UIA-008); sự kiện đổi view có cờ nguồn; `docs/specs/interact/view.md`.
- **Tiêu chí nghiệm thu:**
  1. Điểm thế giới dưới con trỏ đứng yên sau 1.000 lần lăn liên tiếp, lệch ≤ 0,5 px ở tọa độ ≈ 10⁹.
  2. ZOOMFACTOR 3–100, ZOOMWHEEL đảo chiều đúng; delta nhỏ của touchpad cộng dồn; sự kiện lăn gộp theo khung.
  3. Điều hướng không gây regen, không dựng lại chỉ mục; CPU mỗi sự kiện ≤ 0,5 ms **[đề xuất]**; khung pan/zoom 1 triệu entity ≤ 16,6 ms máy tầm trung, ≤ 33 ms máy thấp (đo cùng renderer).
  4. Extents sau 10⁵ sửa ngẫu nhiên trùng extents tính lại từ đầu; layer tắt, đóng băng tính theo quy tắc spec.
  5. Chuyển cảnh mượt ≤ 150 ms, tắt được (VTENABLE), tôn trọng giảm chuyển động.
  6. Đạt DoD-C, DoD-H, DoD-L.

### INT-171 · Kích hoạt viewport: MSPACE, PSPACE
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-170, INT-009, DOC-011
- **Lệnh:** MSPACE, PSPACE
- **Đầu vào:** README §5.9 (kích hoạt viewport trong layout); 07.1 §9.10, §9.12, §10 (MODEL/PAPER); Tài liệu trợ giúp công khai của AutoCAD (MSPACE, PSPACE, CVPORT, TILEMODE).
- **Đầu ra:** `interact/nav/ViewportActivation.{h,cpp}`: viewport tiled trong Model (bấm để kích hoạt); viewport trong layout (nhấp đúp bên trong vào Model space, nhấp đúp ra ngoài về Paper space, Ctrl+R luân phiên); viewport khóa hiển thị; viewport đa giác; lệnh MSPACE, PSPACE; `docs/specs/commands/{MSPACE,PSPACE}.md`.
- **Tiêu chí nghiệm thu:**
  1. CVPORT, lịch sử view, UCS riêng từng viewport đúng; viewport tắt không kích hoạt được.
  2. Viewport khóa: zoom bên trong tác động Paper space; tỉ lệ viewport không đổi.
  3. Viewer: đổi view trong viewport chỉ giữ trong phiên, không ghi bản vẽ **[đề xuất]**.
  4. Chuyển viewport ≤ 16,6 ms trên layout 20 viewport của BENCH-STD.
  5. Đạt DoD-C, DoD-L.

### INT-016 · Điều hướng: ZOOM, PAN, lăn chuột, lịch sử view, kích hoạt viewport
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-170, INT-171, INT-012, INT-008, INT-001, INT-003
- **Lệnh:** ZOOM, PAN
- **Đầu vào:** README §4.2, §5.9; 03 §3 (lệnh trong suốt), §21, Phụ lục A; 07.1 §9.1, §9.2, §9.9; Tài liệu trợ giúp công khai của AutoCAD (ZOOM, PAN, -PAN).
- **Đầu ra:** lệnh ở `interact/nav/cmd/`; `docs/specs/commands/{ZOOM,PAN}.md`; test, benchmark. Engine ở INT-170, viewport ở INT-171, undo của view ở INT-172.
- **Tiêu chí nghiệm thu:**
  1. ZOOM đủ tùy chọn: thời gian thực (mặc định), All, Center, Dynamic, Extents, Previous, Scale (n, nX, nXP), Window, Object (nhận PICKFIRST); PAN thời gian thực và -PAN theo độ dời; menu chuột phải khi zoom/pan thời gian thực (Thoát, Pan, Zoom, Zoom Window, Zoom Original, Zoom Extents); Esc, Enter thoát.
  2. `'ZOOM`, `'PAN` chạy trong suốt giữa lệnh khác rồi trả lại prompt cũ; cùng kết quả từ bàn phím, script và LISP qua INT-002.
  3. View sau lệnh trùng ca vàng ≥ 200 ca (tâm, chiều cao lệch ≤ 1e-9 tương đối); khớp QA-007 khi có đáp án.
  4. ZOOM Extents trên 1 triệu entity ≤ 20 ms phần tính toán máy tầm trung **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.

### INT-172 · ZOOM, PAN trong undo: ghi bước view, gộp liên tiếp
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** INT-016, INT-115, DOC-008
- **Đầu vào:** 07.1 §9.2 (zoom và pan liên tiếp gộp thành một bước hoàn tác, tùy chọn); design/README Quy ước (mỗi lệnh một bước hoàn tác); tùy chọn Combine của UNDO Control (DOC-008); mark undo theo lệnh của INT-115.
- **Đầu ra:** ghi bước view vào DOC-008 cho ZOOM, PAN, lăn chuột, chuột giữa; gộp chuỗi liên tiếp khi Combine bật; test ở `interact/tests/nav/`.
- **Tiêu chí nghiệm thu:**
  1. U sau ZOOM trả đúng view trước, REDO làm lại; bước view không đổi hash nội dung bản vẽ.
  2. Combine bật: 50 lần lăn và pan liên tiếp là 1 bước; tắt: mỗi ZOOM, PAN là 1 bước, lăn chuột theo quy tắc spec.
  3. DBMOD chỉ bật bit thay đổi view như AutoCAD.
  4. Đạt DoD-C.

## 17. Menu chuột phải, nhấp đúp, kéo-thả

### INT-018 · Mô hình menu chuột phải theo ngữ cảnh, nhấp đúp theo loại đối tượng, kéo-thả
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-008, INT-009, INT-171, INT-002, INT-020
- **Màn hình:** UI-A11, UI-A13
- **Đầu vào:** README §5.9; 07.1 §9.9 (V rút gọn: Pan, Zoom, Đo, Thuộc tính), §9.10 (Viewer: nhấp đúp mở Thuộc tính chỉ đọc), §9.11 (V: mở tệp); Tài liệu trợ giúp công khai của AutoCAD (SHORTCUTMENU, DBLCLKEDIT).
- **Đầu ra:** `interact/context/{ContextMenuModel,DoubleClick,DropDispatch}.{h,cpp}`: xác định ngữ cảnh (không lệnh và không chọn, có vùng chọn, mục thêm theo loại, trong lệnh, trên grip nóng); registry mục do module đăng ký (mục → chuỗi lệnh qua INT-002, bật/tắt, đánh dấu, cổng phiên bản INT-020); bộ điều phối nhấp đúp và thả theo loại; nội dung mức V; `docs/specs/interact/context-actions.md`. Nội dung đầy đủ ở INT-173, INT-174.
- **Tiêu chí nghiệm thu:**
  1. Viewer: menu chỉ có Pan, Zoom, Đo, Thuộc tính; nhấp đúp đối tượng mở Thuộc tính chỉ đọc; nhấp đúp trong viewport kích hoạt viewport (INT-171); thả tệp DWG, DXF lên dải tab tệp hoặc tab Start thì mở tệp.
  2. Mọi hành động đi qua chuỗi lệnh INT-002: cùng kết quả như gõ lệnh, một bước hoàn tác; mục bị khóa theo phiên bản mang cờ khóa cho UI.
  3. Các bit SHORTCUTMENU chọn đúng menu mặc định, sửa, lệnh như AutoCAD.
  4. Dựng mô hình menu cho vùng chọn 10.000 đối tượng ≤ 2 ms **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.

### INT-173 · Menu chuột phải đầy đủ và DBLCLKEDIT
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-018, INT-013, INT-010, INT-003, INT-017
- **Màn hình:** UI-A11
- **Đầu vào:** 07.1 §9.9 (đủ các ngữ cảnh), §9.10 (bảng nhấp đúp); Tài liệu trợ giúp công khai của AutoCAD (DBLCLKEDIT, SHORTCUTMENUDURATION, INPUTHISTORYMODE).
- **Đầu ra:** nội dung đầy đủ trong `interact/context/`:
  - Lặp lệnh [tên], Lệnh gần đây và Giá trị đã nhập gần đây (INT-017), Clipboard ▸, Cô lập ▸ (mục do INT-021 đăng ký), mục theo loại (text, block, xref, viewport, kích thước, hatch, ảnh/PDF) gọi lệnh theo tên.
  - Trong lệnh: Enter, Hủy, tùy chọn của lệnh từ dữ liệu từ khóa INT-003; menu Ghi đè bắt điểm (điểm theo dõi tạm, From, trung điểm 2 điểm, bộ lọc `.X .Y .Z .XY .XZ .YZ`, các chế độ, Thiết lập bắt điểm…) dùng chung cho Shift+chuột phải; menu grip nóng (INT-013).
  - Chuột phải theo thời gian (P1); bảng DBLCLKEDIT theo loại.
- **Tiêu chí nghiệm thu:**
  1. Nội dung từng ngữ cảnh khớp bảng 07.1 §9.9; mục trỏ tới lệnh chưa đăng ký thì ẩn, không lỗi.
  2. DBLCLKEDIT bật/tắt; bảng 07.1 §9.10 (TEXT, MTEXT → sửa chữ; block có attribute → EATTEDIT; xref → REFEDIT có xác nhận; polyline → PEDIT; spline → SPLINEDIT; MLINE → MLEDIT; viewport → kích hoạt; loại khác → Thuộc tính nhanh) đúng trên ca vàng mọi loại.
  3. Shift+chuột phải ở prompt điểm mở menu ghi đè; chọn mục cho cùng kết quả như gõ từ khóa tương ứng.
  4. Chuột phải theo thời gian: bấm nhanh là Enter, giữ ≥ SHORTCUTMENUDURATION mở menu.
  5. Đạt DoD-C, DoD-L.

### INT-174 · Kéo-thả đầy đủ
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-018, INT-008, INT-163
- **Màn hình:** UI-A13
- **Đầu vào:** 07.1 §9.11; Tài liệu trợ giúp công khai của AutoCAD (kéo-thả tệp và đối tượng).
- **Đầu ra:** hành động thả trong `interact/context/DropDispatch.cpp`: tệp DWG lên vùng vẽ chèn thành block tại điểm thả (giữ Ctrl thì gắn xref); ảnh, PDF gắn ảnh hoặc PDF underlay; block từ palette, thư viện TCVN, DesignCenter chèn tại điểm thả theo tỉ lệ chú thích; đối tượng đã chọn nhấn giữ rồi kéo thì di chuyển, kéo chuột phải mở menu Di chuyển tới đây / Sao chép tới đây / Dán thành block / Hủy; văn bản, bảng từ ứng dụng khác thành MTEXT hoặc bảng (P1, qua lệnh của VN-013); test.
- **Tiêu chí nghiệm thu:**
  1. Mỗi hành động thả sinh đúng chuỗi lệnh (`_-INSERT`, `_XATTACH`, `_IMAGEATTACH`, `_PDFATTACH`…) qua INT-002, một bước hoàn tác, bản vẽ trùng hash với gõ lệnh tương đương trên ≥ 50 ca.
  2. Kéo 10.000 đối tượng đã chọn có xem trước INT-163 ≤ 8 ms/khung máy tầm trung.
  3. Thả tệp hỏng hoặc loại không hỗ trợ: báo lỗi, bản vẽ không đổi.
  4. Đường dẫn Unicode tiếng Việt và đường dẫn mạng UNC đúng.
  5. Đạt DoD-C, DoD-L.

## 18. Cô lập đối tượng và chọn luân phiên

### INT-021 · Cô lập, ẩn đối tượng và chọn luân phiên
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** INT-008, INT-009, INT-018, INT-007, RND-013, DOC-018
- **Lệnh:** ISOLATEOBJECTS, HIDEOBJECTS, UNISOLATEOBJECTS, SELECTIONCYCLING
- **Đầu vào:** README §5.9; 03 §3 (SELECTIONCYCLING P1), §4 (cô lập, ẩn P1); 07.1 §9.3, §9.9 (Cô lập ▸), §10 (Ctrl+W, Cô lập đối tượng); 07.2 UI-D09 (tab Chọn luân phiên); Tài liệu trợ giúp công khai của AutoCAD (OBJECTISOLATIONMODE, SELECTIONCYCLING).
- **Đầu ra:** `interact/isolate/Isolation.{h,cpp}`: tập ẩn theo bản vẽ cắm vào móc lọc hiển thị của INT-009, hiển thị (RND-013), OSNAP, extents; `interact/select/SelectionCycling.{h,cpp}`: danh sách ứng viên chồng nhau có thứ tự tất định cho UI; đăng ký mục Cô lập ▸ vào INT-018; `docs/specs/commands/{ISOLATEOBJECTS,HIDEOBJECTS,UNISOLATEOBJECTS,SELECTIONCYCLING}.md`.
- **Tiêu chí nghiệm thu:**
  1. Đối tượng bị ẩn không hiện, không chọn được, không bắt điểm, không tính vào ZOOM Extents; UNISOLATEOBJECTS trả lại đủ.
  2. OBJECTISOLATIONMODE 0: trạng thái mất khi đóng bản vẽ, DWG lưu ra trùng hash với khi không cô lập; 1: giữ qua phiên trong dữ liệu riêng của TrionCAD **[đề xuất]**.
  3. SELECTIONCYCLING 0/1/2 và Ctrl+W: pick trúng ≥ 2 đối tượng thì có danh sách (loại, layer, màu) theo draw order; chọn được bằng chuột và phím.
  4. Cô lập hoặc ẩn 100.000 đối tượng ≤ 16 ms phần đồng bộ, không regen toàn bộ **[đề xuất]**.
  5. Cùng kết quả từ bàn phím, `.scr` và `(command)`; khớp QA-007 khi có đáp án.
  6. Đạt DoD-C, DoD-L.

## Tóm tắt theo mốc

| Mốc | Task | Số task |
|---|---|---|
| V | INT-101, INT-001, INT-102, INT-103, INT-002, INT-104, INT-105, INT-004, INT-106, INT-003, INT-107, INT-108, INT-005, INT-110, INT-017, INT-020, INT-109, INT-116, INT-019, INT-151, INT-152, INT-009, INT-153, INT-154, INT-155, INT-008, INT-157, INT-010, INT-159, INT-012, INT-168, INT-170, INT-171, INT-016, INT-018 | 35 |
| VP | INT-115 | 1 |
| EA | INT-111, INT-006, INT-007, INT-112, INT-158, INT-161, INT-162, INT-011, INT-163, INT-164, INT-165, INT-166, INT-013, INT-014, INT-015, INT-172, INT-173, INT-174 | 18 |
| 1.0 | INT-113, INT-022, INT-156, INT-160, INT-167, INT-169, INT-021 | 7 |
| Sau | INT-114 | 1 |
| **Tổng** | | **62** |
