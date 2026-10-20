# Lõi · LSP, CUS, SDK: AutoLISP, DCL, tùy biến, chuyển cài đặt, SDK

> Engine AutoLISP/Visual LISP (reader, kiểu dữ liệu, evaluator, khoảng 300 hàm lõi, `vl-*`, `vlax-*`, `vlr-*`, tập con `vla-*`), DCL (parser, layout, runtime), nạp mã an toàn, macro menu, tệp tùy biến và nhập CUIX, chuyển cài đặt từ AutoCAD, workspace (không gian làm việc), SDK C/C++ và nạp plugin.
> - Thư mục mã: `lisp/`, `custom/`, `sdk/`. Đặc tả lệnh: `docs/specs/commands/`. Mẫu task, DoD, task neo: [README](README.md) §2–§5.
> - Căn cứ: [05](../05-kien-truc-ky-thuat.md) §5.4, §11, §12, §14–§16, §21; [03](../03-chuc-nang-autocad.md) §18, §19, §27, §31; [06](../06-lo-trinh-trien-khai.md) §4–§5; [07.3](../07-dac-ta-ui-3-tinh-nang-vn-va-he-thong.md) §5 (UI-K01–K12); prototype `design/prototype/js/screens8.js`, `screens20.js`.
> - Giao diện Qt của các màn hình UI-K (nạp ứng dụng, cảnh báo bảo mật, DCL, chuyển cài đặt, workspace, CUI, bảng điều khiển LISP, quản lý plugin) thuộc UIK. Tệp này làm engine, lệnh và backend của các màn hình đó.

## 0. Ghi chú thiết kế

- **Một luồng ghi, máy ngăn xếp tường minh** (05 §5.4). LISP chạy trên luồng UI. Evaluator không đệ quy trên stack C++, nên `getpoint`, `(command … pause)`, `start_dialog` tạm dừng rồi chạy tiếp mà không chặn UI. Song song chỉ dùng cho việc đọc hàng loạt trên snapshot DOC-009 qua NT-005: bộ lọc `ssget`, đọc trước `entget`, GC lớn, quét tệp.
- **Chuẩn tương thích là AutoCAD** (05 §11). Giá trị trả về, cách in, định dạng số và thông báo lỗi trùng từng ký tự. Chuỗi lỗi đưa vào LISP (`*error*`, `vl-catch-all-error-message`) giữ nguyên tiếng Anh như AutoCAD, vì LISP cộng đồng so sánh các chuỗi như "Function cancelled"; dòng lệnh hiện thêm diễn giải tiếng Việt **[đề xuất]**.
- **Ca vàng cho mọi task hàm.** Đặc tả ở `lisp/docs/spec/<nhóm>.md`. Ca kiểm ở `lisp/tests/golden/<nhóm>/<hàm>.lsp` kèm `.out`. Mỗi hàm ≥ 10 ca, trong đó ≥ 3 ca nhánh lỗi **[đề xuất]**. Trước khi có đáp án AutoCAD thì dùng ca tự soạn; khi có đáp án qua cơ chế QA-007 (kịch bản `.scr` gọi tệp kiểm `.lsp`, ghi kết quả ra tệp văn bản) thì phải khớp 100%.
- **Một nguồn cho mỗi việc** (README §4.2): định dạng số RGN-013; mã nhóm DXF DOC-017; engine chọn và bộ lọc INT-008; sysvar INT-005; lệnh đi qua INT-002; mọi sửa bản vẽ qua DOC-007, có undo và sự kiện. LISP không có đường tắt vào bản vẽ.
- **Host tách engine khỏi giao diện.** `lisp::Host` gom nhập liệu, xuất chữ, hộp thoại, DCL. Ứng dụng Qt hiện thực ở UIK; `trion-console` (INT-019) chạy không giao diện; kiểm thử dùng `ScriptedHost` (đầu vào theo kịch bản, ghi lại mọi đầu ra) để chạy tất định trong CI. `lisp::CommandHost` (LSP-006) là hợp đồng để INT-002 nhận LISP làm một nguồn input.
- **Đầu vào không tin cậy** (DoD-C 7): tệp LISP, DCL, slide, CUIX, `.arg`, `.tcui`, gói plugin. Nạp mã luôn qua chính sách SECURELOAD, mặc định mức 1 (05 §16).
- **Phân kỳ** (05 §21, 06 §4–§5). S5: evaluator, bộ hàm tối thiểu, `(command)` có `pause` trên host kịch bản. EA: đủ hàm lõi, DCL, nạp mã, cầu nối trên INT-002 thật; bộ LISP ≥ 60% ở T24, ≥ 75% ở cổng G2. Bản 1.0: `vl-*` còn lại, `vlax-*`, `vlr-*`, tập con `vla-*` (≥ 90%), CUIX, workspace, SDK.
- **`vla-*` sinh từ mô tả mô hình đối tượng** tham chiếu schema DWG-001. Tập con chọn theo số lần gọi trong bộ QA-008 (LSP-101).
- **Tệp tùy biến riêng `.tcui`** (JSON có phiên bản **[đề xuất]**, như `trion.tcui` trong prototype) là lớp phủ lên định nghĩa ribbon, menu, toolbar có sẵn (UIA-005, UIA-006). CUIX chỉ được đọc rồi chuyển vào `.tcui`, không ghi CUIX.
- **SDK không tương thích nhị phân với ObjectARX** (03 §31). C ABI có phiên bản, bảng hàm truyền cho plugin lúc nạp **[đề xuất]**, C++ wrapper header-only. Plugin chạy cùng tiến trình; lỗi được cô lập bằng tường lửa ngoại lệ, crash được quy về đúng plugin rồi plugin đó bị tắt.

## 1. Nền engine và spike S5

### LSP-002 · Kiểu dữ liệu AutoLISP và GC phân thế hệ
- **Mốc:** S5 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** NT-001, NT-004, NT-005, NT-007
- **Đầu vào:** 05 §11 (bảng kiểu dữ liệu, GC mark-and-sweep phân thế hệ), §5.4; API arena/pool và `StringId` của NT-007; bộ đếm cấp phát NT-004; bộ lập lịch NT-005.
- **Đầu ra:**
  - `lisp/core/Value.h`: giá trị gắn thẻ 64 bit **[đề xuất]** cho INT, REAL, STR, SYM, LIST (cons và `nil`), FILE, SUBR, USUBR, ENAME, PICKSET, VLA-OBJECT, VARIANT, SAFEARRAY; `type` trả đúng tên ký hiệu như AutoLISP.
  - INT 32 bit tràn vòng như AutoLISP; số nguyên vượt 32 bit khi đọc thành REAL theo đặc tả.
  - Chuỗi lưu UTF-16 để `strlen`, `substr`, `ascii` đếm giống AutoLISP bản Unicode **[đề xuất]**.
  - `lisp/core/Heap.h/.cpp`: ô cons và đối tượng cấp theo trang trong arena; GC mark-and-sweep hai thế hệ có write barrier; gốc là bảng ký hiệu, ngăn xếp evaluator, handle do host giữ. GC lớn chạy mark và sweep song song qua NT-005.
  - `lisp/tests/core/`, `lisp/bench/heap_bench.cpp`, `lisp/docs/spec/types.md`.
- **Tiêu chí nghiệm thu:**
  1. Mỗi kiểu trong 13 kiểu có kiểm thử tạo, `eq`/`equal`, tên kiểu; `(type x)` khớp ca vàng.
  2. `(+ 2147483647 1)` cho `-2147483648`; 1.000 ca biên cộng, trừ, nhân, chia số nguyên khớp ca vàng (§0).
  3. Ô cons ≤ 16 byte; tạo 10⁷ cons ≤ 150 ms trên máy tầm trung **[đề xuất]**; bộ cấp phát hệ thống không bị gọi trong vòng lặp tạo cons (NT-004 đếm bằng 0).
  4. Property-based 10⁵ đồ thị đối tượng ngẫu nhiên có chu trình: sau GC, mọi đối tượng còn tham chiếu giữ nguyên giá trị, mọi đối tượng không còn tham chiếu được thu hồi.
  5. GC thế hệ trẻ p99 ≤ 2 ms khi có 10⁶ cons đang sống; GC toàn bộ 10⁷ ô ≤ 50 ms, hiệu suất song song ≥ 60% đến 8 luồng **[đề xuất]**.
  6. Ép GC sau mỗi lần cấp phát, bộ ca vàng vẫn cho đầu ra giống từng byte (kết quả không phụ thuộc thời điểm GC).
  7. Đạt DoD-C, DoD-H.

### LSP-001 · Reader và printer AutoLISP
- **Mốc:** S5 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** LSP-002, NT-012
- **Đầu vào:** 05 §11 ("quy tắc so sánh và in số giống AutoLISP"), §14 (fuzzing parser LISP); NT-012 (UTF-8/UTF-16, code page Windows); bộ LISP QA-008 (100 tệp GĐ0) làm corpus.
- **Đầu ra:**
  - `lisp/reader/Reader.h/.cpp`: số nguyên, số thực (`1.`, `.5`, `1e3`), chuỗi có `\\ \" \e \n \r \t \nnn`, ký hiệu không phân biệt hoa thường, `'`, cặp chấm, chú thích `;` và `;| |;`. Mỗi biểu thức mang vị trí tệp:dòng để báo vết lỗi.
  - Nhận diện mã hóa tệp `.lsp`: BOM UTF-8/UTF-16, UTF-8 hợp lệ, còn lại theo code page ANSI của máy (Windows) hoặc theo thiết lập (mặc định 1252 trên macOS, Linux **[đề xuất]**). Nhờ vậy chuỗi TCVN3/VNI trong LISP cũ ra đúng byte như AutoCAD trên cùng máy.
  - `lisp/printer/Printer.h/.cpp`: `prin1`, `princ`, `print` cho mọi kiểu; REAL theo quy tắc AutoLISP (đặc tả); ENAME, PICKSET, FILE, SUBR in dạng `<Entity name: …>`, `<Selection set: …>`, `#<file "…">`, `#<SUBR @…>` với số định danh tất định theo phiên, không lộ địa chỉ bộ nhớ (DoD-C 4).
  - Giới hạn đầu vào **[đề xuất]**: lồng 256 mức, chuỗi 16 MB, tệp 64 MB. Lỗi cú pháp trả thông báo của AutoLISP ("malformed list on input", "extra right paren on input"…).
  - `lisp/tests/reader/`, `lisp/fuzz/reader_fuzz.cpp`, `lisp/bench/reader_bench.cpp`, `lisp/docs/spec/reader-printer.md`.
- **Tiêu chí nghiệm thu:**
  1. Đọc được 100% tệp hợp lệ của bộ QA-008 GĐ0; tệp lỗi cú pháp thật báo đúng thông báo và đúng dòng.
  2. `read` → `prin1` → `read` cho cùng giá trị trên 10⁶ biểu thức sinh ngẫu nhiên (property-based).
  3. ≥ 10.000 giá trị REAL (0, −0, ±1e±300, 1e6, 123456.0, 0.1, số dưới chuẩn, các điểm biên làm tròn) và INT biên in khớp từng ký tự ca vàng (§0).
  4. Đọc 1 MB mã nguồn ≤ 15 ms trên máy tầm trung **[đề xuất]**.
  5. Fuzz reader ≥ 1 giờ không lỗi; vượt giới hạn thì báo lỗi, không cấp phát quá giới hạn.
  6. Chuỗi TCVN3/VNI trong 20 tệp LISP cũ của bộ QA-008 cho đúng chuỗi byte như AutoCAD trên cùng code page (so ca vàng).
  7. Đạt DoD-C.

### LSP-101 · Công cụ thống kê hàm, sysvar và lệnh trong bộ LISP (`lispstat`)
- **Mốc:** S5 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** LSP-001, QA-008, NT-005
- **Đầu vào:** bộ LISP QA-008 (100 tệp GĐ0, 500 tệp đến v1.0) và các tệp `.dcl` đi kèm; reader LSP-001; 05 §10 ("khoảng 250 sysvar, ưu tiên theo bộ LISP"), §20 (ước lượng PM của LISP).
- **Đầu ra:**
  - `lisp/tools/lispstat/`: CLI quét thư mục, chịu được tệp lỗi cú pháp. Đếm số lần gọi và số tệp dùng cho từng hàm; tên sysvar trong `getvar`/`setvar`; tên lệnh trong `command`/`vl-cmdf`; loại tile DCL; tên `vla-*`, `vlax-*`, `vlr-*`. Đánh dấu hàm đã có trong engine.
  - Xuất JSON và bảng Markdown xếp hạng; quét song song theo tệp qua NT-005.
  - `lisp/tests/tools/lispstat_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Trên 20 tệp có đáp án đếm tay, số lần gọi từng hàm khớp 100%.
  2. Tệp lỗi cú pháp vẫn được đếm đến chỗ lỗi và có tên trong mục lỗi; lượt quét không dừng.
  3. Quét 500 tệp ≤ 3 s trên máy tầm trung **[đề xuất]**; kết quả giống từng byte khi chạy 1 và 16 luồng.
  4. Báo cáo có đủ mục hàm, sysvar, lệnh, tile DCL, `vla-*`; mỗi dòng có số lần gọi, số tệp, trạng thái đã hiện thực.
  5. Đạt DoD-C, DoD-H.

### LSP-003 · Evaluator máy ngăn xếp, dynamic scoping, tạm dừng và tiếp tục
- **Mốc:** S5 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** LSP-001, LSP-002
- **Đầu vào:** 05 §5.4, §11 (ngữ nghĩa, evaluator); hành vi AutoLISP theo AutoLISP Reference (tài liệu công khai, chỉ tham khảo hành vi, không chép nội dung — 05 §15).
- **Đầu ra:**
  - `lisp/eval/Machine.h/.cpp`: máy ngăn xếp tường minh với 14 dạng đặc biệt `quote`, `setq`, `if`, `cond`, `and`, `or`, `while`, `repeat`, `foreach`, `progn`, `defun`, `defun-q`, `lambda`, `function`; gọi SUBR (C++) và USUBR.
  - Dynamic scoping: tham số và biến cục bộ sau `/` nhận `nil` khi vào hàm và được khôi phục khi ra, kể cả khi lỗi (shallow binding, ngăn xếp giá trị cũ).
  - API đăng ký hàm có sẵn: số đối số tối thiểu, tối đa, kiểm kiểu; thông báo "too few arguments", "too many arguments", "bad argument type: <vị từ> <giá trị>", "no function definition: <TÊN>".
  - Tạm dừng/tiếp tục: hàm có sẵn trả về một yêu cầu nhập (`InputRequest`); máy giữ trạng thái, host trả giá trị thì chạy tiếp. Esc đưa lỗi "Function cancelled" vào tại điểm an toàn.
  - Lõi `lisp::Host` (xuất chữ, yêu cầu nhập, tín hiệu hủy) và `HeadlessHost`; khung lỗi tháo ngăn xếp và khôi phục binding (LSP-005 thêm `*error*`).
  - `lisp/tests/eval/`, `lisp/bench/eval_bench.cpp`, `lisp/docs/spec/eval.md`, `lisp/docs/spec/errors.md`.
- **Tiêu chí nghiệm thu:**
  1. Đệ quy LISP sâu 10⁶ mức chạy được khi stack C++ của luồng kiểm thử chỉ có 256 KB; vượt giới hạn độ sâu cấu hình thì báo lỗi theo đặc tả, không crash.
  2. 200 ca vàng dynamic scoping (biến cục bộ che biến toàn cục, hàm được gọi đọc biến của hàm gọi nó, khôi phục sau lỗi) khớp từng ký tự.
  3. 10⁴ chương trình sinh ngẫu nhiên chạy liền và chạy có tạm dừng ở mọi điểm nhập cho cùng kết quả; một lần tạm dừng và tiếp tục tốn ≤ 5 µs **[đề xuất]**.
  4. Esc dừng `(while T)` trong ≤ 100 ms **[đề xuất]**; mọi binding được khôi phục.
  5. Trên máy tầm trung, dùng SUBR kiểm thử cho phép tính **[đề xuất]**: `(fib 25)` đệ quy ≤ 120 ms; vòng `(repeat 10000000 (setq i (inc i)))` ≤ 400 ms; không cấp phát hệ thống trong vòng lặp (NT-004).
  6. Đạt DoD-C.

### LSP-004 · Hàm lõi bộ S5: danh sách, số học, chuỗi, `wcmatch`, kiểu, in
- **Mốc:** S5 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** LSP-003, GEO-004, NT-012
- **Đầu vào:** 05 §11, §21 (S5 ước khoảng 30 hàm; danh sách dưới đây rộng hơn vì phần lớn là hàm một dòng mà 10 LISP cộng đồng cần); quy ước ca vàng §0; GEO-004 (hàm sơ cấp tất định); NT-012 (phân loại ký tự, gấp hoa thường và dấu).
- **Đầu ra:** `lisp/builtins/{list,math,string,types,print}.cpp` và `lisp/text/Wildcard.h/.cpp`; 63 hàm hiện thực đủ ngữ nghĩa (task EA không làm lại):
  - Danh sách: `car`, `cdr`, `cadr`, `caddr`, `cons`, `list`, `append`, `length`, `nth`, `assoc`, `member`, `reverse`, `last`.
  - Số học: `+`, `-`, `*`, `/`, `1+`, `1-`, `abs`, `fix`, `float`, `sqrt`, `sin`, `cos`, `atan`, `rem`, `min`, `max`.
  - So sánh: `=`, `/=`, `<`, `<=`, `>`, `>=`, `eq`, `equal` (có fuzz).
  - Hình học phẳng: `polar`, `distance`, `angle`.
  - Chuỗi: `strcat`, `strlen`, `substr`, `strcase`, `itoa`, `atoi`, `atof`, `wcmatch`.
  - Kiểu và bậc cao: `type`, `null`, `listp`, `numberp`, `atom`, `boundp`, `mapcar`, `apply`, `eval`, `not`.
  - In ra dòng lệnh qua host: `princ`, `prin1`, `print`, `prompt`, `terpri`.
  - Bộ so khớp wildcard dùng chung `lisp::Wildcard`: đủ cú pháp `#`, `@`, `.`, `*`, `?`, `~`, `[...]`, `[~...]`, khoảng `-`, `` ` `` thoát, `,` nhiều mẫu; biên dịch mẫu một lần; ba chế độ: phân biệt hoa thường (`wcmatch`), không phân biệt hoa thường (bộ lọc `ssget`, tên bảng ký hiệu), không phân biệt hoa thường và dấu (ô tìm kiếm, theo NT-012).
  - `lisp/tests/golden/core/`, `lisp/docs/spec/{list,math,string,wcmatch}.md`.
- **Tiêu chí nghiệm thu:**
  1. Ca vàng (§0) cho cả 63 hàm, khớp từng ký tự, thông báo lỗi trùng.
  2. `sin`, `cos`, `atan`, `sqrt`, `polar`, `distance`, `angle` cho kết quả trùng từng bit trên Windows, macOS, Linux.
  3. `wcmatch`: thêm 2.000 cặp mẫu/chuỗi (cả chữ tiếng Việt có dấu) khớp ca vàng; property-based 10⁶ ca so với cài đặt tham chiếu quay lui viết riêng trong kiểm thử, ở cả ba chế độ.
  4. 10⁶ lần so khớp với mẫu đã biên dịch ≤ 80 ms; `mapcar` trên danh sách 10⁶ phần tử ≤ 60 ms; `append`, `length`, `assoc` không đệ quy C++ **[đề xuất]**.
  5. Đạt DoD-C.

### LSP-102 · Nhập liệu và sysvar qua host cho S5: getpoint, getdist, getreal, getint, getstring, getkword, initget, getvar, setvar
- **Mốc:** S5 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** LSP-003, LSP-101
- **Đầu vào:** 05 §11, §21; ngữ nghĩa `initget`/`getkword` theo AutoLISP Reference (chỉ tham khảo hành vi); `lisp::Host` của LSP-003; danh sách sysvar LISP hay dùng từ LSP-101.
- **Đầu ra:**
  - `lisp/builtins/input.cpp`: 9 hàm với ngữ nghĩa phía LISP đầy đủ: đối số, prompt, giá trị trả về; `initget` bit 1, 2, 4 và danh sách từ khóa (viết tắt bằng chữ hoa, từ khóa toàn cục `_`); giá trị khi nhập rỗng; nhập sai thì hỏi lại. Các bit khác của `initget` được lưu để LSP-007 dùng.
  - Yêu cầu nhập đi qua `Host::requestInput` (máy tạm dừng); `getvar`/`setvar` qua `Host::sysvar`.
  - `ScriptedHost` dùng chung cho mọi kiểm thử: hàng đợi đầu vào (điểm, số, chuỗi, Enter, Esc), bảng sysvar nạp từ `lisp/tests/data/sysvars.json` (khoảng 60 biến LISP hay dùng **[đề xuất]**), ghi lại mọi prompt và đầu ra.
  - `lisp/tests/golden/input/`, `lisp/docs/spec/input.md`.
- **Tiêu chí nghiệm thu:**
  1. Ca vàng (§0) cho cả 9 hàm, gồm mọi tổ hợp bit 1, 2, 4 với từ khóa, nhập rỗng, nhập sai rồi nhập lại, Esc ("Function cancelled").
  2. `setvar` sai kiểu hoặc vào biến chỉ đọc báo lỗi trùng chuỗi AutoLISP; `getvar` biến không có trả `nil`.
  3. Cùng kịch bản đầu vào cho bản ghi prompt giống từng byte trên 3 hệ điều hành.
  4. Đạt DoD-C.
- **Ngoài phạm vi:** prompt thật, rubber-band, OSNAP, DYN, bit 8/32/64/128/256 (LSP-007); registry sysvar thật (LSP-108).

### LSP-006 · Cầu nối lệnh: command, command-s, vl-cmdf, pause; hợp đồng `CommandHost` và trình chạy `lisprun`
- **Mốc:** S5 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** LSP-004, LSP-102
- **Đầu vào:** 05 §5.3 (`(command ...)` đi qua cùng input pipeline), §5.4, §11, §21; 03 Nguyên tắc tương thích (tên `_`, tiền tố `.`, thứ tự prompt); `ScriptedHost` của LSP-102.
- **Đầu ra:**
  - `lisp/bridge/Command.h/.cpp`: `command`, `command-s`, `vl-cmdf`, ký hiệu `pause`. Đối số thành token có kiểu (chuỗi, số, điểm, ENAME, PICKSET; `""` là Enter); `(command)` không đối số là hủy lệnh.
  - Hợp đồng `lisp::CommandHost` (gửi token, hỏi lệnh còn chờ nhập không, trạng thái CMDACTIVE) để INT-002 hiện thực LISP như một nguồn input. Ngữ nghĩa như AutoCAD:
    - hết token mà lệnh còn chờ thì `command` trả về và lệnh **vẫn hoạt động**, lần `command` sau nạp tiếp;
    - `pause` nhường đúng một lần nhập cho người dùng (máy tạm dừng);
    - `command-s` không nhận `pause`, lệnh phải xong trong lần gọi;
    - `vl-cmdf` tính và kiểm mọi đối số trước khi gửi token đầu tiên.
  - Bộ kiểm tuân thủ `lisp/tests/bridge/conformance/` (300 ca) mà mọi hiện thực `CommandHost` phải qua, kể cả hiện thực của INT-002.
  - Mô hình prompt kiểm thử cho `ScriptedHost` (`lisp/tests/fixtures/commands/*.json`: LINE, CIRCLE, TEXT, ZOOM, -LAYER… với chuỗi prompt và điều kiện kết thúc) để kiểm ngữ nghĩa "lệnh còn hoạt động" khi chưa có INT-002.
  - `lisp/tools/lisprun/`: CLI không giao diện, nạp tệp `.lsp`, nhận kịch bản đầu vào, xuất bản ghi JSON (đầu ra chữ, token lệnh, prompt, sysvar cuối). QA-008 dùng ở S5; từ mốc V dùng `trion-console` (INT-019).
  - `lisp/docs/spec/command.md`.
- **Tiêu chí nghiệm thu:**
  1. 300 ca tuân thủ khớp ca vàng: `(command "LINE")` rồi `(command p1 p2 "")`; vòng `(while (> (getvar "CMDACTIVE") 0) (command pause))`; `command-s` gặp `pause` báo lỗi theo đặc tả; `vl-cmdf` có đối số sai thì không gửi token nào.
  2. Esc trong lúc `pause` hủy lệnh và đưa "Function cancelled" vào LISP.
  3. `lisprun`: cùng đầu vào cho bản ghi giống từng byte trên 3 hệ điều hành và mọi số luồng (DoD-C 4).
  4. Một lần `command` tốn ≤ 20 µs, không tính việc của lệnh **[đề xuất]**.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** hiện thực `CommandHost` trên input pipeline (thuộc INT-002); lệnh `defun c:`, LISP trên dòng lệnh, nghiệm thu với lệnh thật (LSP-107).

### LSP-103 · Nghiệm thu spike S5: 10 LISP cộng đồng và ước lượng PM
- **Mốc:** S5 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** LSP-006, LSP-101, QA-008
- **Đầu vào:** 05 §21 (S5: "Chạy được 10 LISP cộng đồng; ước lượng PM có căn cứ"), §20 (LISP 40 PM: lõi 18, DCL 5, `vl`/`vlax`/`vlr` 7, tập con `vla` 10); 06 §2.2 (cổng G0); bộ QA-008 GĐ0; báo cáo `lispstat` (LSP-101).
- **Đầu ra:**
  - Danh sách 10 tệp chọn từ bộ 100 tệp theo quy tắc ghi trước: ≥ 5 tác giả, mỗi tệp ≥ 50 dòng, có get*, có `(command ...)` với `pause`, có xử lý danh sách và chuỗi **[đề xuất]**. Kịch bản đầu vào và kết quả kỳ vọng nộp vào QA-008.
  - `lisp/docs/bao-cao-s5.md`: kết quả chạy; số đo hiệu năng của LSP-002, LSP-003; tỉ lệ lời gọi trong 100 tệp đã có hàm; 50 hàm còn thiếu dùng nhiều nhất; ước lượng PM từng nhóm (số dòng mỗi hàm đo được × số hàm × hệ số kiểm thử), so với 05 §20.
- **Tiêu chí nghiệm thu:**
  1. 10/10 tệp chạy hết trong `lisprun`; đầu ra và bản ghi token lệnh trùng kết quả kỳ vọng của QA-008 trên 3 hệ điều hành.
  2. Đạt các ngưỡng hiệu năng ghi ở LSP-002 và LSP-003 trên máy tầm trung.
  3. Báo cáo có đủ các mục ở Đầu ra; mỗi con số PM kèm công thức và dữ liệu đo; chênh lệch so với 05 §20 quá 20% thì nêu nguyên nhân **[đề xuất]**.
  4. Đạt DoD-C.

## 2. Thư viện hàm lõi còn lại

### LSP-104 · Hàm danh sách, ký hiệu và bộ nhớ còn lại
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** LSP-004
- **Đầu vào:** 05 §11; 03 §19 (AutoLISP lõi là P0); quy ước ca vàng §0; tần suất dùng từ LSP-101.
- **Đầu ra:** `lisp/builtins/{list2,symbols,memory}.cpp`, 38 hàm:
  - Tổ hợp `c…r` còn lại (26 hàm): `caar`, `cdar`, `cddr`, `caaar`, `caadr`, `cadar`, `cdaar`, `cdadr`, `cddar`, `cdddr` và 16 hàm 4 mức từ `caaaar` đến `cddddr`.
  - Danh sách: `subst`, `acad_strlsort`.
  - Ký hiệu và hàm: `set`, `atoms-family`, `defun-q-list-ref`, `defun-q-list-set`, `trace`, `untrace`.
  - Bộ nhớ (giữ tương thích, trả số liệu của GC LSP-002): `alloc`, `expand`, `gc`, `mem`.
  - `lisp/tests/golden/list2/`, `lisp/docs/spec/symbols.md`.
- **Tiêu chí nghiệm thu:**
  1. Ca vàng (§0) cho cả 38 hàm, khớp từng ký tự, thông báo lỗi trùng.
  2. Mẫu `S::STARTUP` dựng bằng `defun-q` và `append` của 20 tệp `acaddoc.lsp` trong bộ QA-008 gọi các hàm con đúng thứ tự.
  3. Định dạng dòng `trace` (vào hàm, kết quả) trùng ca vàng.
  4. `subst` trên cây 10⁵ nút ≤ 20 ms; `acad_strlsort` 10⁵ chuỗi ≤ 60 ms **[đề xuất]**.
  5. Đạt DoD-C.

### LSP-105 · Hàm số học, bit và chuỗi còn lại
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** LSP-004
- **Đầu vào:** 05 §11; GEO-004; quy ước ca vàng §0; reader LSP-001 (cho `read`).
- **Đầu ra:** `lisp/builtins/{math2,string2}.cpp`, 14 hàm:
  - Số học và bit: `~`, `exp`, `expt`, `log`, `gcd`, `logand`, `logior`, `lsh`, `boole` (16 mã phép toán), `minusp`, `zerop`.
  - Chuỗi: `ascii`, `chr`, `read`.
  - `lisp/tests/golden/math2/`, `string2/`.
- **Tiêu chí nghiệm thu:**
  1. Ca vàng (§0) cho cả 14 hàm, khớp từng ký tự, thông báo lỗi trùng.
  2. `exp`, `expt`, `log` cho kết quả trùng từng bit trên 3 hệ điều hành (GEO-004); `lsh`, `logand`, `logior`, `~` trên số âm và khi tràn 32 bit khớp ca vàng.
  3. `ascii`, `chr` đúng với ký tự tiếng Việt dựng sẵn, tổ hợp và ký tự ngoài BMP (theo đặc tả UTF-16 của LSP-002).
  4. `read` báo cùng thông báo lỗi cú pháp như reader LSP-001.
  5. Đạt DoD-C.

### LSP-106 · Không gian tên theo bản vẽ và blackboard
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** LSP-104, DOC-018
- **Đầu vào:** DOC-018 (phiên nhiều bản vẽ, bản vẽ hiện hành); 05 §5.4; hành vi MDI của AutoLISP (mỗi bản vẽ một không gian tên, blackboard dùng chung).
- **Đầu ra:** `lisp/eval/Namespace.h/.cpp`; 9 hàm `vl-doc-set`, `vl-doc-ref`, `vl-bb-set`, `vl-bb-ref`, `vl-propagate`, `vl-load-all`, `vl-doc-export`, `vl-doc-import`, `vl-list-exported-functions`:
  - mỗi bản vẽ mở có bảng giá trị ký hiệu riêng; blackboard dùng chung cho mọi bản vẽ;
  - LISP đang chạy gắn với bản vẽ của nó; người dùng đổi bản vẽ khi LISP đang chờ (`getpoint`, `pause`) thì LISP vẫn chờ ở bản vẽ cũ **[đề xuất]**;
  - đóng bản vẽ thì không gian tên được GC thu hồi;
  - `vl-doc-export`, `vl-doc-import` cho VLX có không gian tên riêng trả lỗi theo đặc tả đến khi có LSP-134.
  - `lisp/tests/golden/namespace/`, `lisp/docs/spec/namespace.md`.
- **Tiêu chí nghiệm thu:**
  1. Ca vàng (§0) cho 9 hàm; 100 kịch bản nhiều bản vẽ (mở 3 bản vẽ, gán và đọc biến, định nghĩa hàm ở một bản vẽ rồi gọi ở bản vẽ khác) khớp ca vàng.
  2. Không gian tên rỗng ≤ 200 KB; đổi bản vẽ hiện hành đổi không gian tên ≤ 1 ms **[đề xuất]**.
  3. Mở và đóng 100 bản vẽ liên tiếp không rò bộ nhớ (ASan, bộ đếm của GC).
  4. Đạt DoD-C.

### LSP-005 · Xử lý lỗi: `*error*`, `vl-catch-all-*`, `exit`, `quit`, thông báo như AutoLISP
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** LSP-003, LSP-006, LSP-104
- **Đầu vào:** 05 §11; 03 §19 (`*error*` là P0); khung lỗi của LSP-003; quy ước §0; các mẫu `*error*` thường gặp trong bộ QA-008 (LSP-101).
- **Đầu ra:** `lisp/eval/Errors.h/.cpp`, `lisp/builtins/errors.cpp`:
  - `*error*` của người dùng nhận chuỗi thông báo; dạng `(defun f (/ *error*) ...)` thay tạm và tự khôi phục; lỗi xảy ra trong `*error*` xử lý theo đặc tả;
  - thông báo mặc định `; error: <thông báo>`; `exit`, `quit` ("quit / exit abort"); Esc ("Function cancelled");
  - `vl-catch-all-apply`, `vl-catch-all-error-p`, `vl-catch-all-error-message`, `vl-exit-with-error`, `vl-exit-with-value`;
  - `*push-error-using-command*`, `*push-error-using-stack*`, `*pop-error-mode*`: luật dùng `command` và `command-s` trong `*error*` như AutoCAD;
  - sysvar ERRNO đặt theo mã lỗi của từng hàm;
  - danh mục mọi thông báo lỗi `lisp/docs/spec/errors.md`: chuỗi tiếng Anh gốc và diễn giải tiếng Việt hiện trên dòng lệnh.
- **Tiêu chí nghiệm thu:**
  1. Mỗi thông báo trong danh mục có ≥ 3 ca vàng; tổng ≥ 300 ca khớp từng ký tự.
  2. 50 mẫu `*error*` lấy từ bộ QA-008 (khôi phục sysvar, đóng `UNDO`) cho cùng bản ghi trong `ScriptedHost`: sysvar về giá trị cũ, mark undo đóng đúng.
  3. Property-based 10⁴ ca lỗi ở độ sâu ngẫu nhiên: mọi binding động được khôi phục.
  4. `vl-catch-all-apply` khi không có lỗi tốn thêm ≤ 1 µs **[đề xuất]**.
  5. Đạt DoD-C.

## 3. Lệnh, nhập liệu và hiển thị

### LSP-107 · Lệnh LISP (`defun c:`), LISP trên dòng lệnh và nghiệm thu cầu nối trên INT-002
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** LSP-005, LSP-006, INT-001, INT-002, INT-003, INT-004, INT-005, INT-015, INT-016, INT-020
- **Đầu vào:** 05 §5.3, §10; 07.1 §2 (Viewer không chạy LISP), §8 (dòng lệnh); 03 Nguyên tắc tương thích; API INT-001 (registry lệnh), INT-002 (input pipeline có nguồn LISP hiện thực `lisp::CommandHost`), INT-003 (prompt, từ khóa), INT-004 (parser tọa độ); bộ kiểm tuân thủ của LSP-006.
- **Đầu ra:** `lisp/bridge/LispCommands.cpp`, `lisp/bridge/CommandLine.cpp`:
  - `(defun c:XXX ...)` đăng ký lệnh XXX vào INT-001 (nguồn: tệp và dòng), hiện trong tự hoàn thành và ô Tìm lệnh; định nghĩa lại hoặc `(setq c:XXX nil)` cập nhật registry. Toàn bộ một lệnh LISP được hoàn tác như một nhóm (theo ca vàng).
  - Dòng lệnh: chuỗi bắt đầu bằng `(` được tính (nhiều dòng đến khi đủ ngoặc), kết quả in theo `prin1`; `!ten` ở mọi prompt lấy giá trị biến; biểu thức LISP làm câu trả lời cho prompt (ví dụ `(list 0 0)` khi hỏi điểm).
  - `initdia`, `initcommandversion`, `getcname`; CMDECHO, CMDACTIVE khi LISP gọi lệnh; LISP gọi trong suốt khi người dùng đang ở giữa một lệnh.
  - Cổng phiên bản qua INT-020: Viewer và Viewer Pro không chạy LISP, thông báo theo 07.1 §2.
  - `lisp/tests/bridge/pipeline/`, `lisp/docs/spec/lisp-commands.md`.
- **Tiêu chí nghiệm thu:**
  1. 300 ca tuân thủ của LSP-006 qua trên INT-002 thật, bản ghi token và prompt giống `ScriptedHost`.
  2. Với ZOOM, PAN, UCS, SETVAR và mọi lệnh P0 đã merge (ma trận CI tăng dần): cùng đầu vào gõ phím, `.scr`, `(command)`, `(command-s)`, `(vl-cmdf)` cho bản dump DOC-017 và bản ghi prompt giống nhau 100%.
  3. Lệnh LISP gọi 50 lần `command` được hoàn tác hết bằng một lần U (theo ca vàng).
  4. Ký tự gõ vào dòng lệnh hiện ≤ 16 ms (DoD-L); từ Enter đến khi in kết quả `(+ 1 2)` ≤ 5 ms **[đề xuất]**.
  5. 10.000 lần `command` gọi một lệnh kiểm thử rỗng ≤ 100 ms **[đề xuất]**.
  6. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** hiện thực `CommandHost` trong input pipeline (INT-002).

### LSP-108 · Sysvar và môi trường: getvar/setvar trên INT-005, getenv, setenv, getcfg, setcfg, ver
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** LSP-101, LSP-102, INT-005, NT-010
- **Đầu vào:** 05 §10 (khoảng 250 sysvar, ưu tiên theo bộ LISP); API INT-005 (registry sysvar), NT-010 (kho cài đặt theo profile); báo cáo LSP-101.
- **Đầu ra:** `lisp/builtins/env.cpp`, `lisp/host/SysvarBinding.cpp`; 9 hàm:
  - `getvar`, `setvar` (của LSP-102) nối vào registry INT-005: kiểu, miền giá trị, biến chỉ đọc, thông báo từ chối như AutoCAD;
  - `getenv`, `setenv`: bảng ánh xạ các tên môi trường AutoCAD hay dùng sang cài đặt TrionCAD; tên khác lưu trong kho riêng theo profile (NT-010), không ghi registry của hệ điều hành;
  - `getcfg`, `setcfg` (mục AppData) lưu theo người dùng; `ver` trả chuỗi theo đặc tả **[đề xuất]**; `acad-push-dbmod`, `acad-pop-dbmod` giữ DBMOD;
  - đăng ký vào INT-005 các sysvar của LISP: ERRNO, LISPSYS (luôn 1, chỉ đọc **[đề xuất]**), ACADLSPASDOC;
  - `lisp/docs/sysvar-gap.md` sinh tự động từ LSP-101: sysvar bộ QA-008 dùng mà INT-005 chưa có, để chuyển cho INT.
- **Tiêu chí nghiệm thu:**
  1. Ca vàng (§0) cho 9 hàm; `getvar`/`setvar` đúng trên 100% sysvar mà bộ QA-008 dùng và INT-005 đã có.
  2. `acad-push-dbmod`/`acad-pop-dbmod`: 20 kịch bản giữ đúng DBMOD.
  3. Giá trị `setenv` còn sau khởi động lại và tách riêng từng profile.
  4. Một lần `getvar` ≤ 0,3 µs **[đề xuất]**.
  5. Đạt DoD-C.

### LSP-109 · Chọn đối tượng tại điểm: entsel, nentsel, nentselp
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** LSP-107, INT-009, DOC-012, DOC-013
- **Đầu vào:** 03 §19 (`getXXX` là P0); API INT-009 (hit test chính xác), DOC-012 (block lồng, ATTRIB, `*U`), DOC-013 (xref); sysvar PICKBOX.
- **Đầu ra:** `lisp/builtins/entsel.cpp`:
  - `entsel`: prompt, nhận từ khóa của `initget`, chọn được cả trên layer khóa; trả `(ename điểm)`, điểm theo UCS;
  - `nentsel`: entity lồng trong block hoặc xref trả thêm ma trận 4×3 và danh sách block chứa; chọn trúng ATTRIB trả ATTRIB;
  - `nentselp`: có hoặc không có điểm truyền vào, trả ma trận 4×4;
  - `lisp/tests/golden/entsel/`, `lisp/docs/spec/entsel.md`.
- **Tiêu chí nghiệm thu:**
  1. Ca vàng (§0) cho 3 hàm; thêm 200 ca trên bản vẽ QA-001 có block lồng 1–5 mức, xref, attribute, dynamic block `*U`.
  2. Ma trận sai khác ≤ 1e-12 tương đối so với ca vàng.
  3. Chọn tại con trỏ p99 ≤ 1 ms máy tầm trung, ≤ 3 ms máy thấp trên cảnh 1 triệu entity của BENCH-STD (DoD-L).
  4. Đạt DoD-C, DoD-L.

### LSP-110 · Hỏi và báo người dùng qua host: alert, getfiled, hộp chọn màu, grtext, cửa sổ văn bản, trợ giúp
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** LSP-102, INT-005, NT-011
- **Đầu vào:** 03 §19; 07.2 UI-C06 (bộ chọn màu), UI-I03 (cửa sổ văn bản), 07.3 UI-L12 (trợ giúp) để biết giao diện mà host gọi tới; `lisp::Host` (LSP-003, LSP-102); NT-011 (đường dẫn tìm kiếm cho cờ của `getfiled`).
- **Đầu ra:** `lisp/builtins/ui.cpp`, mở rộng `lisp::Host`; 12 hàm:
  - `alert`; `getfiled` (mọi bit cờ, đuôi tệp, thư mục đầu; hành vi khi FILEDIA = 0 và khi không có giao diện theo đặc tả);
  - `acad_colordlg`, `acad_truecolordlg` (trả `((62 . n) (420 . rgb) (430 . "sổ$màu"))`), `acad_truecolorcli`;
  - `grtext`, `textscr`, `graphscr`, `textpage`, `help`, `setfunhelp`, `acad_helpdlg`;
  - hành vi mặc định của `HeadlessHost` (trion-console) và câu trả lời theo kịch bản của `ScriptedHost`;
  - hộp thoại chạy bất đồng bộ qua host: LISP tạm dừng, luồng UI không bị chặn;
  - `lisp/docs/host-api.md` cho UIK.
- **Tiêu chí nghiệm thu:**
  1. Ca vàng (§0) cho 12 hàm: dạng giá trị trả về đúng từng phần tử; Hủy trả `nil`.
  2. Mọi phương thức host mới có kiểm thử với `ScriptedHost` và `HeadlessHost`.
  3. Khi hộp thoại đang mở, vòng sự kiện UI vẫn chạy: sự kiện giả được xử lý trong ≤ 16 ms.
  4. Đạt DoD-C.
- **Ngoài phạm vi:** hộp thoại Qt (UIK; chọn màu dùng UI-C06).

### LSP-007 · Hàm get* và chọn trên prompt thật
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** LSP-102, LSP-107, LSP-109, LSP-110, INT-003, INT-004, INT-010, INT-012, INT-014
- **Đầu vào:** 03 §19 (`getXXX` là P0); 07.1 §8 (dòng lệnh), §9.6–9.7 (OSNAP, DYN); 07.2 UI-E10 (chip tùy chọn); API INT-003, INT-004, INT-010, INT-012, INT-014.
- **Đầu ra:** `lisp/builtins/getx.cpp`, `lisp/host/PromptBinding.cpp`:
  - 11 hàm trên prompt thật của INT-003 (từ khóa thành chip UI-E10, giá trị mặc định): `getpoint`, `getdist`, `getreal`, `getint`, `getstring`, `getkword`, `initget` (ngữ nghĩa phía LISP đã có ở LSP-102) và `getcorner`, `getangle`, `getorient`, `grread` (mới);
  - `initget` đủ bit: 1, 2, 4, 8 (bỏ kiểm LIMCHECK), 32 (rubber-band nét đứt), 64 (bỏ Z khi đo), 128 (nhập tùy ý), 256 (ưu tiên nhập khoảng cách trực tiếp), các bit 3D theo đặc tả;
  - rubber-band từ điểm gốc qua jig INT-012 (`getpoint`, `getdist`, `getangle`: đường; `getcorner`: chữ nhật); OSNAP INT-010; DYN INT-014; lệnh trong suốt khi đang chờ nhập; tọa độ và đơn vị qua INT-004 (feet-inch, góc khảo sát, nhập khoảng cách trực tiếp);
  - `getangle` theo ANGBASE, ANGDIR; `getorient` tuyệt đối; kết quả bằng radian;
  - `grread`: theo dõi chuột, nhận mọi phím, kiểu con trỏ, loại kết quả theo đặc tả;
  - nghiệm thu chung với `entsel`, `nentsel`, `nentselp` (LSP-109) và dịch vụ host (LSP-110).
- **Tiêu chí nghiệm thu:**
  1. Ca vàng (§0) cho 11 hàm chạy trên INT-002 thật, gồm mọi bit `initget` kết hợp từ khóa (≥ 400 ca).
  2. Cùng đầu vào qua bàn phím, `.scr` và `ScriptedHost` cho cùng giá trị trả về.
  3. Rubber-band p99 ≤ 8 ms/khung máy tầm trung, ≤ 16 ms máy thấp; OSNAP trong `getpoint` ≤ 1 ms; cảnh 1 triệu entity của BENCH-STD (DoD-L).
  4. `grread` theo dõi chuột trễ ≤ 1 khung; khi chờ nhập CPU ~0% (DoD-H 6).
  5. Đạt DoD-C, DoD-L.

### LSP-111 · Đồ họa tạm và màn hình: grdraw, grvecs, redraw, vports, setview, menucmd
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** LSP-107, RND-012, RND-013, RND-021, DOC-004, DOC-011, INT-016
- **Đầu vào:** API RND-012 (overlay), RND-013 (tô sáng theo entity), RND-021 (render ngoài màn hình cho ảnh kiểm), DOC-011 (viewport), DOC-004 (bảng VIEW), INT-016 (đổi view).
- **Đầu ra:** `lisp/builtins/display.cpp`; 8 hàm:
  - `grdraw`, `grvecs` (danh sách vector có màu, ma trận biến đổi) vẽ lên overlay của viewport hiện hành; mất khi `redraw`, regen, pan/zoom như AutoCAD; không ghi vào bản vẽ;
  - `redraw` không đối số và `(redraw ename mode)` với mode 1–4 (hiện, ẩn, tô sáng, bỏ tô sáng; với block thì tác động cả entity con);
  - `vports`, `setview`;
  - `menucmd` với chuỗi điều khiển menu cũ trả `nil` theo đặc tả (dạng `"M="` ở CUS-104); `menugroup` (trả tên nhóm khi đã nạp tệp tùy biến CUS-002, trước đó trả `nil`); `tablet` (trả `nil`).
- **Tiêu chí nghiệm thu:**
  1. Ca vàng (§0) cho 8 hàm (giá trị trả về) và 50 ca ảnh overlay so với ảnh chuẩn.
  2. `grvecs` 10⁴ vector: khung hình ≤ 8 ms (DoD-L); tô sáng một block có 10⁴ entity con ≤ 16 ms.
  3. Hash bản vẽ không đổi sau mọi ca vẽ tạm.
  4. Đạt DoD-C, DoD-L.

## 4. Dữ liệu bản vẽ: entity, xdata, bảng, tập chọn, hình học

### LSP-112 · Đọc entity: entget, entnext, entlast, handent; đọc hàng loạt song song
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** LSP-004, DOC-001, DOC-009, DOC-012, DOC-017, DWG-024, NT-005
- **Đầu vào:** 03 §19; DXF Reference công khai của Autodesk (mã nhóm); API DOC-017 (metadata thuộc tính và mã nhóm DXF, sinh từ schema), DOC-012 (entity con: ATTRIB, VERTEX, SEQEND), DOC-001 (handle), DOC-009 (snapshot); bộ ghi DXF DWG-024 (để đối chiếu).
- **Đầu ra:** `lisp/db/EntGet.h/.cpp`; 4 hàm:
  - `entget`: danh sách mã nhóm theo đúng thứ tự AutoCAD (-1, 0, 330, 5, các marker 100, 67/410, 8, 62/420/430, 6, 370, 48, 60, 440…), hình học theo WCS hoặc OCS đúng loại entity (210);
  - `entnext` theo thứ tự trong cơ sở dữ liệu, kể cả entity con, bỏ entity đã xóa; `entlast`; `handent` (đối tượng đã xóa xử lý theo đặc tả);
  - ENAME gắn với bản vẽ; ENAME của entity đã xóa xử lý theo đặc tả;
  - đọc hàng loạt: khi LISP duyệt tập chọn bằng `(entget (ssname ss i))`, engine đọc trước các entity kế tiếp song song trên snapshot DOC-009 (NT-005) và bỏ kết quả đọc trước khi bản vẽ thay đổi **[đề xuất]**;
  - `lisp/tests/golden/entget/`, `lisp/docs/spec/entity.md`.
- **Tiêu chí nghiệm thu:**
  1. Với ≥ 10.000 entity lấy mẫu từ bộ 1.000 tệp QA-001, phủ mọi loại entity của DOC-003: mỗi cặp (mã . giá trị) của `entget` có trong DXF ASCII mà DWG-024 ghi cho cùng entity (trừ mã ENAME), đúng thứ tự.
  2. `entnext` đi qua đúng thứ tự entity của DOC-012 trên 100 bản vẽ, kể cả ATTRIB, VERTEX, SEQEND.
  3. Vòng `entget` qua `ssname` trên 10⁵ LINE ≤ 150 ms, trên 10⁵ entity hỗn hợp ≤ 300 ms (máy tầm trung) **[đề xuất]**; hiệu suất đọc trước ≥ 60% đến 8 luồng; bật hay tắt đọc trước cho danh sách giống từng bit.
  4. Khi có đáp án AutoCAD (QA-007): `entget` khớp 100% trên các bản vẽ tham chiếu.
  5. Đạt DoD-C, DoD-H.

### LSP-113 · Tạo entity: entmake, entmakex
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** LSP-112, DOC-004, DOC-007, DOC-008, DOC-011, DOC-012, DOC-017
- **Đầu vào:** 03 §19 (`entmake` là P0); DXF Reference công khai (mã nhóm bắt buộc, giá trị mặc định); API DOC-007 (transaction), DOC-017, DOC-012 (định nghĩa block, block vô danh), DOC-011 (không gian hiện hành).
- **Đầu ra:** `lisp/db/EntMake.h/.cpp`; 2 hàm:
  - `entmake` cho mọi loại entity của DOC-003; chuỗi phức: POLYLINE → VERTEX → SEQEND, INSERT → ATTRIB → SEQEND; định nghĩa block BLOCK … ENDBLK (trả tên block), block vô danh `*U`;
  - `entmakex` cho đối tượng không đồ họa (DICTIONARY, XRECORD, GROUP…), trả ENAME chưa có chủ;
  - mã nhóm bắt buộc, giá trị mặc định, tạo layer chưa có, kiểm style và linetype theo đặc tả; danh sách sai trả `nil` và đặt ERRNO;
  - mỗi lần tạo đi qua transaction DOC-007; chủ là không gian hiện hành; quy tắc OCS/WCS đúng loại entity;
  - `lisp/tests/golden/entmake/`.
- **Tiêu chí nghiệm thu:**
  1. Với 10.000 entity từ QA-001: `entmake` của `entget` (bỏ -1, 5, 330) tạo entity có bản dump DOC-017 trùng nguồn trừ handle và chủ.
  2. Ca vàng (§0) cho danh sách sai (thiếu mã bắt buộc, sai kiểu, layer hoặc style không có): giá trị trả về và ERRNO trùng.
  3. 200 ca chuỗi phức và định nghĩa block; chuỗi bị bỏ dở (`(entmake)` không đối số) hủy entity phức đang dựng.
  4. 10⁵ lần `entmake` LINE ≤ 500 ms **[đề xuất]**; hoàn tác cả lệnh đưa hash bản vẽ về như trước.
  5. Đạt DoD-C.

### LSP-008 · Hàm entity: entmod, entdel, entupd và nghiệm thu nhóm entity
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** LSP-112, LSP-113, DOC-007, DOC-008, RGN-003, QA-008, DWG-166
- **Đầu vào:** 03 §19 (`entget`/`entmod`/`entmake` là P0); README §4.2 (ánh xạ trường qua DOC-017, không tự ánh xạ); API DOC-007, DOC-008, RGN-003 (regen gia tăng theo sự kiện).
- **Đầu ra:** `lisp/db/EntMod.h/.cpp`; 3 hàm:
  - `entmod`: mọi mã sửa được theo DOC-017; không đổi được mã 0; sửa ATTRIB, VERTEX rồi `entupd`; entity đã xóa trả `nil`; mỗi lần sửa qua transaction DOC-007, gộp vào nhóm undo của lệnh đang chạy;
  - `entdel`: xóa và khôi phục (gọi lần hai) trong phiên;
  - `entupd`: cập nhật hiển thị entity phức ngay trong khung hình kế tiếp (RGN-003);
  - nghiệm thu chung cả nhóm entity: `entget`, `entnext`, `entlast`, `handent` (LSP-112), `entmake`, `entmakex` (LSP-113).
- **Tiêu chí nghiệm thu:**
  1. `entmod` của chính `entget` không đổi bản dump DOC-017 trên 10.000 entity của QA-001.
  2. Property-based 10⁵ lần sửa hợp lệ ngẫu nhiên qua `entmod` cho bản dump trùng với sửa trực tiếp qua DOC-007.
  3. `entdel` hai lần khôi phục entity; undo và redo sau `entmod`/`entdel` đưa hash bản vẽ về đúng giá trị.
  4. 10⁴ lần `entmod` ≤ 100 ms kể cả sự kiện regen gom lô **[đề xuất]**.
  5. 40 LISP dùng nhiều `entget`/`entmod` trong bộ QA-008 đạt (so DWG và đầu ra).
  6. Đạt DoD-C.

### LSP-114 · Xdata: regapp, xdroom, xdsize và nhóm -3 trong entget, entmod, entmake
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** LSP-112, LSP-113, DOC-004, DOC-005
- **Đầu vào:** 03 §19 (xdata là P0); DXF Reference công khai (mã 1000–1071, giới hạn kích thước xdata); API DOC-005 (xdata), DOC-004 (bảng APPID).
- **Đầu ra:** `lisp/db/XData.h/.cpp`; 3 hàm `regapp`, `xdroom`, `xdsize` và phần xdata của nhóm entity:
  - `(entget e apps)` có wildcard tên ứng dụng; `entmod`, `entmake` với nhóm -3;
  - mã 1000, 1001, 1002 (`{` `}`), 1003, 1004 (khối nhị phân dạng hex), 1005 (handle), 1010–1013, 1040–1042, 1070, 1071; vượt giới hạn kích thước báo lỗi theo đặc tả;
  - `lisp/tests/golden/xdata/`.
- **Tiêu chí nghiệm thu:**
  1. Ca vàng (§0) cho 3 hàm và cho nhóm -3 (≥ 300 ca).
  2. Xdata của 5.000 entity từ QA-001 qua `entget` rồi `entmod` không đổi từng byte.
  3. Khối 1004 và chuỗi 1000 có ký tự tiếng Việt đọc ghi đúng từng byte.
  4. Đạt DoD-C.

### LSP-010 · Bảng ký hiệu và dictionary: tblsearch, tblnext, tblobjname, snvalid, layoutlist, dict*
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** LSP-112, LSP-113, LSP-114, DOC-004, DOC-005, DOC-011, QA-008
- **Đầu vào:** 03 §19 (`tblsearch`, dictionary là P0); API DOC-004 (bảng ký hiệu, named object dictionary), DOC-005 (dictionary, xrecord), DOC-011 (layout).
- **Đầu ra:** `lisp/db/Tables.h/.cpp`; 11 hàm:
  - `tblsearch` (cờ setnext), `tblnext` (rewind), `tblobjname`, `snvalid`, `layoutlist`;
  - `dictadd`, `dictnext`, `dictremove`, `dictrename`, `dictsearch`, `namedobjdict`;
  - danh sách bản ghi bảng theo DOC-004 cho LAYER (cờ 70, màu âm khi tắt), LTYPE, STYLE, VIEW, UCS, VPORT (`*ACTIVE`), DIMSTYLE, APPID, BLOCK (mã -2 trỏ entity đầu);
  - `lisp/tests/golden/tables/`.
- **Tiêu chí nghiệm thu:**
  1. Ca vàng (§0) cho 11 hàm (≥ 300 ca).
  2. Trên 1.000 bản vẽ của QA-001, `tblnext` liệt kê đúng các bản ghi và đúng thứ tự như duyệt DOC-004 (bản ghi phụ thuộc xref theo đặc tả).
  3. `snvalid` trên 10⁴ tên ngẫu nhiên (kể cả tên tiếng Việt Unicode) khớp bảng quy tắc của đặc tả.
  4. 10⁴ thao tác thêm, xóa, đổi tên dictionary ngẫu nhiên nhất quán với DOC-005; hoàn tác đưa hash về như trước.
  5. 30 LISP dùng bảng và dictionary trong bộ QA-008 đạt.
  6. Đạt DoD-C.

### LSP-009 · ssget đủ chế độ và bộ lọc; các hàm ss*
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** LSP-007, LSP-112, LSP-114, INT-008, DOC-009, NT-005
- **Đầu vào:** 05 §11 (`ssget` đủ chế độ chọn và bộ lọc: mã nhóm, toán tử `-4`, wildcard `wcmatch`); 03 §4, §19; API INT-008 (engine chọn, bộ lọc kiểu `ssget`, chạy song song), `lisp::Wildcard` (LSP-004).
- **Đầu ra:** `lisp/db/SelSet.h/.cpp`; 9 hàm:
  - `ssget` với các chế độ `""` (tương tác), `"A"`, `"C"`, `"CP"`, `"F"`, `"G"`, `"I"`, `"L"`, `"M"`, `"P"`, `"W"`, `"WP"`, `"X"`, `":E"`, `":L"`, `":N"`, `":S"`, `":D"`, `":$"`, `"+."`, dạng có tiền tố `_`, có danh sách điểm và bộ lọc;
  - bộ lọc: mã nhóm; chuỗi wildcard (không phân biệt hoa thường với tên); `-4` logic `<AND AND> <OR OR> <XOR XOR> <NOT NOT>`; so sánh `= != /= <> < <= > >= & &= *`, so điểm theo từng tọa độ (`">,>,*"`); `-3` theo ứng dụng xdata; danh sách loại (`"LINE,ARC"`); 410/67 theo không gian; màu 256 (ByLayer);
  - bộ lọc LISP đổi sang vị từ của INT-008; chế độ `"X"` chạy song song trên snapshot DOC-009 qua engine INT-008; kết quả theo thứ tự cơ sở dữ liệu; PICKSET do GC quản lý;
  - `ssadd`, `ssdel`, `sslength`, `ssname`, `ssmemb`, `ssnamex` (thông tin cách chọn), `ssgetfirst`, `sssetfirst` (PICKFIRST và grip);
  - `lisp/tests/golden/ssget/`, `lisp/docs/spec/ssget.md`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 500 ca vàng (chế độ × bộ lọc) trên 50 bản vẽ QA-001; khi có đáp án AutoCAD (QA-007) thì khớp 100%.
  2. Property-based 10⁵ bộ lọc ngẫu nhiên: kết quả `ssget "X"` trùng bộ đánh giá vét cạn viết riêng trong kiểm thử trên dữ liệu DOC-017.
  3. So chuỗi trong bộ lọc cho cùng kết quả với `wcmatch` chế độ không phân biệt hoa thường trên 10⁴ mẫu.
  4. `ssget "X"` lọc theo loại, layer có wildcard và `-4 <OR` trên cảnh 1 triệu entity của BENCH-STD ≤ 40 ms với 8 luồng trên máy tầm trung **[đề xuất]**; hiệu suất song song ≥ 60% đến 8 luồng; kết quả giống nhau từ 1 đến 32 luồng.
  5. Chế độ tương tác (window, crossing, fence, polygon) cho cùng tập với SELECT của INT-008 trên 100 ca kịch bản; p99 cập nhật khung chọn ≤ 8 ms (DoD-L).
  6. Đạt DoD-C, DoD-H, DoD-L.

### LSP-011 · Hình học, định dạng số và đơn vị: rtos, distof, angtos, angtof, cvunit, inters, trans, osnap, textbox
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** LSP-004, LSP-112, RGN-007, RGN-013, INT-010, INT-015, DOC-011, GEO-008, INT-105
- **Đầu vào:** 05 §11 (định dạng số theo mọi chế độ đơn vị, kiểm thử từng ký tự); README §4.2 (RGN-013 là nguồn duy nhất); DXF Reference công khai (thuật toán trục tùy ý cho OCS); API RGN-013, RGN-007 (dàn chữ TEXT), INT-010, INT-015, GEO-008.
- **Đầu ra:** `lisp/builtins/geom.cpp`, `lisp/data/trion.unt`; 9 hàm:
  - `rtos` (chế độ 1–5, độ chính xác, DIMZIN, UNITMODE), `angtos` (chế độ 0–4): gọi RGN-013, không tự định dạng; `distof`, `angtof`: gọi parser INT-105, không tự phân tích;
  - `cvunit` với tệp định nghĩa đơn vị tự viết `trion.unt`, giữ tên đơn vị chuẩn (05 §15);
  - `inters` (cờ onseg); `trans` (0 WCS, 1 UCS, 2 DCS, 3 PSDCS, ENAME cho OCS, vector pháp tuyến; cờ disp); `osnap` (chuỗi chế độ, vùng APERTURE) qua INT-010; `textbox` cho TEXT và ATTRIB theo RGN-007;
  - ca vàng 3D và đối chiếu AutoCAD cho `polar`, `distance`, `angle` (đã có ở LSP-004);
  - `lisp/tests/golden/geom/`, `lisp/docs/spec/geom-format.md`.
- **Tiêu chí nghiệm thu:**
  1. `rtos`, `angtos`, `distof`, `angtof`: ≥ 10⁵ ca (mọi chế độ × độ chính xác × DIMZIN × UNITMODE × giá trị biên) khớp từng ký tự ca vàng; `distof` của `rtos` trả đúng giá trị trong độ chính xác đã in.
  2. `trans`: 10⁴ ca UCS, view, OCS ngẫu nhiên sai khác ≤ 1e-12 tương đối so với ma trận tham chiếu.
  3. `inters`: 10⁴ ca gồm song song, trùng, gần song song khớp ca vàng.
  4. `osnap` cho cùng điểm với INT-010 trên 500 ca kịch bản, ≤ 1 ms (DoD-L).
  5. `textbox`: 1.000 TEXT với font SHX và TTF, width factor, oblique, mọi kiểu căn lề sai khác ≤ 1e-6 × chiều cao chữ so với ca vàng.
  6. `cvunit`: 200 phép đổi khớp ca vàng.
  7. Đạt DoD-C, DoD-L.

## 5. Tệp, nạp mã và bảo mật

### LSP-115 · Chính sách nạp mã an toàn: SECURELOAD, TRUSTEDPATHS, LEGACYCODESEARCH; lệnh SECURITYOPTIONS
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** INT-001, INT-005, NT-009, NT-010, NT-011
- **Lệnh:** SECURITYOPTIONS
- **Màn hình:** UI-K02, UI-K03
- **Đầu vào:** 05 §16 (SECURELOAD mặc định bật, cảnh báo khi nạp mã ngoài trusted paths); 03 §19, §27 (chặn LISP độc hại đi kèm bản vẽ); 07.3 UI-K02 (lý do cảnh báo; Nạp một lần / Luôn nạp / **Không nạp** là nút mặc định), UI-K03 (3 mức, danh sách thư mục tin cậy, cảnh báo thư mục ai cũng ghi được, 2 ô chọn); prototype `js/screens8.js` › `SC.secWarn`, `IMPL.SECURITYOPTIONS`, `loadApp`, `trusted`.
- **Đầu ra:**
  - `lisp/security/LoadPolicy.h/.cpp`: `check(đường dẫn, ngữ cảnh)` → Cho nạp / Cảnh báo (kèm lý do) / Chặn. Ngữ cảnh: tự nạp khi khởi động, APPLOAD, `(load)`, autoload, script, `.mnl`, plugin (SDK-003).
  - SECURELOAD 0/1/2 (mặc định 1); TRUSTEDPATHS (danh sách cách nhau bằng `;`, hậu tố `\...` gồm thư mục con, biến môi trường); thư mục cài đặt TrionCAD luôn tin cậy; LEGACYCODESEARCH mặc định 0 (không tìm tệp khởi động trong thư mục bản vẽ).
  - Lý do cho UI-K02: ngoài thư mục tin cậy; `acad*.lsp`/`acaddoc*.lsp` nằm cạnh bản vẽ; chưa ký số (plugin); nội dung rủi ro (LSP-116).
  - Quyết định: "Nạp một lần" nhớ theo hash nội dung trong phiên; "Luôn nạp" thêm thư mục vào TRUSTEDPATHS sau khi kiểm quyền ghi (ACL Windows, quyền POSIX), thư mục ai cũng ghi được thì cảnh báo (UI-K03).
  - Hai thiết lập của UI-K03 (cảnh báo khi bản vẽ đi kèm `acad.lsp`/`acaddoc.lsp`; chặn tự nạp LISP từ thư mục bản vẽ) lưu ở NT-010; cấu hình phạm vi máy của quản trị khóa được SECURELOAD và TRUSTEDPATHS **[đề xuất]**.
  - LISP hoặc script hạ SECURELOAD, thêm TRUSTEDPATHS thì cần người dùng xác nhận qua host **[đề xuất]**.
  - Chuẩn hóa đường dẫn trước khi kiểm: hoa thường (Windows, macOS), UNC, symlink, junction, `..`, đường dẫn dài.
  - Lệnh SECURITYOPTIONS mở UI-K03 qua host; khi không có giao diện (`trion-console`, FILEDIA = 0) in trạng thái hiện tại và hướng dẫn dùng SETVAR **[đề xuất]**. Mọi quyết định ghi log (NT-009).
  - `docs/specs/commands/SECURITYOPTIONS.md`, `lisp/docs/spec/security.md`, `lisp/tests/security/policy/`.
- **Tiêu chí nghiệm thu:**
  1. Bảng quyết định ≥ 500 ca (mức × vị trí × tên tệp × ngữ cảnh × thiết lập) khớp bảng trong đặc tả; mặc định là mức 1.
  2. 10⁴ đường dẫn sinh tự động (hoa thường, UNC, symlink, junction, `..`, Unicode tiếng Việt): ở mức 2 không tệp nào phân giải ra ngoài thư mục tin cậy được nạp.
  3. "Luôn nạp" với thư mục ai cũng ghi được luôn có cảnh báo; khóa phạm vi máy ngăn hạ mức (kiểm thử).
  4. Quyết định cho một tệp ≤ 50 µs khi đã có cache **[đề xuất]**; tệp trong thư mục tin cậy không bị đọc nội dung.
  5. LISP hạ SECURELOAD trong `ScriptedHost`: có yêu cầu xác nhận; từ chối thì giá trị không đổi.
  6. Đạt DoD-C.

### LSP-116 · Nhận diện mẫu LISP độc hại
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** LSP-001, LSP-115, QA-008
- **Màn hình:** UI-K02
- **Đầu vào:** 03 §27 ("mã độc `acad.lsp` từng phổ biến"), 05 §16; báo cáo công khai về các dòng mã độc `acad.lsp`/`acaddoc.lsp` (chỉ dùng mô tả hành vi); bộ QA-008 làm tập LISP lành.
- **Đầu ra:**
  - `lisp/security/Scanner.h/.cpp`: phân tích tĩnh trên chuỗi token của reader LSP-001 (không dùng regex trên văn bản). Luật cho các hành vi đã biết: tự chép sang thư mục bản vẽ hoặc support path; ghi tệp `acad*.lsp`, `acaddoc*.lsp`, `*.mnl`, `*.fas`, `*.vlx`; đổi SECURELOAD, TRUSTEDPATHS, LEGACYCODESEARCH; UNDEFINE lệnh chuẩn; `vl-registry-write` vào khóa Autodesk; `startapp`; che giấu mã (`eval`/`read` chuỗi tính ra, chuỗi `chr` dài); xóa tệp hàng loạt; dấu vân tay của mẫu đã biết (hash chuỗi token chuẩn hóa) trong `lisp/security/signatures.json`.
  - Kết quả: mức rủi ro và lý do cho UI-K02; mức cao thì luôn hỏi, kể cả SECURELOAD = 0 **[đề xuất]**.
  - Chặn lúc chạy (dùng ở LSP-012): LISP ghi vào `acad*.lsp`, `acaddoc*.lsp`, `*.mnl` ở bất kỳ thư mục nào thì phải được người dùng xác nhận.
  - Bộ mẫu do nhóm tự viết mô phỏng các hành vi trên, không dùng mã độc thật: `lisp/tests/security/malicious/`.
- **Tiêu chí nghiệm thu:**
  1. Phát hiện 100% bộ mẫu tự viết (≥ 60 mẫu, mỗi luật ≥ 5 biến thể, có biến thể che giấu) **[đề xuất]**.
  2. Báo nhầm ≤ 1% số tệp QA-008 ở mức "cảnh báo" và 0% ở mức "chặn" **[đề xuất]**.
  3. Quét tệp 100 KB ≤ 5 ms **[đề xuất]**; chỉ quét tệp ngoài thư mục tin cậy hoặc có tên `acad*`.
  4. Fuzz bộ quét ≥ 1 giờ không lỗi.
  5. Đạt DoD-C.

### LSP-012 · Tệp và môi trường: open, read-line, write-line, findfile, getfiled, getenv/setenv, vl-registry-*, startapp
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** LSP-108, LSP-110, LSP-115, LSP-116, NT-011, NT-012
- **Đầu vào:** 03 §19, §31 (`.lsp`, `.scr`); API NT-011 (đường dẫn tìm kiếm, ghi an toàn), NT-012 (mã hóa); chính sách LSP-115 và chặn lúc chạy của LSP-116; `getfiled` (LSP-110), `getenv`/`setenv` (LSP-108).
- **Đầu ra:** `lisp/io/Files.h/.cpp`, `lisp/io/Registry.h/.cpp`; 13 hàm và phần tích hợp:
  - `open` (chế độ `"r"`, `"w"`, `"a"`, tham số mã hóa theo đặc tả), `close`, `read-line`, `write-line`, `read-char`, `write-char` (không có tệp thì đọc bàn phím, ghi dòng lệnh theo đặc tả), `princ`/`prin1`/`print` ra tệp;
  - đọc nhận diện mã hóa (BOM, UTF-8, code page ANSI qua NT-012); mã hóa khi ghi theo đặc tả, đối chiếu AutoCAD bản Unicode;
  - `findfile` và `findtrustedfile` theo thứ tự tìm của đặc tả (thư mục bản vẽ, support path NT-011…);
  - `vl-registry-read`, `vl-registry-write`, `vl-registry-delete`, `vl-registry-descendents` trên hive ảo lưu ở NT-010 theo profile; các khóa AutoCAD hay đọc (đường dẫn hồ sơ, phiên bản) ánh xạ sang giá trị TrionCAD; không truy cập registry thật **[đề xuất]**;
  - `startapp` chạy chương trình ngoài sau khi qua chính sách: mã nạp từ nơi không tin cậy thì phải xác nhận **[đề xuất]**;
  - tệp còn mở được đóng khi GC thu hồi hoặc khi đóng bản vẽ **[đề xuất]**;
  - nghiệm thu chung với `getfiled`, `getenv`, `setenv`;
  - `lisp/tests/golden/files/`, `lisp/docs/spec/files.md`.
- **Tiêu chí nghiệm thu:**
  1. Ca vàng (§0) cho 13 hàm (≥ 300 ca).
  2. 50 tệp ANSI, UTF-8, UTF-16 đọc rồi ghi lại đúng từng byte theo đặc tả.
  3. `findfile` cho đúng tệp trên 100 cấu hình đường dẫn.
  4. Hive ảo giữ giá trị qua khởi động lại, tách riêng từng profile; không lời gọi nào chạm registry thật (kiểm bằng hook trong kiểm thử).
  5. LISP ghi `acaddoc.lsp` hoặc gọi `startapp` từ mã không tin cậy: 100% ca có yêu cầu xác nhận trong `ScriptedHost`.
  6. `read-line` ≥ 200 MB/s trên máy tầm trung **[đề xuất]**.
  7. Đạt DoD-C.

### LSP-117 · Nạp tệp LISP: load, autoload; arxload và hàm ARX
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** LSP-005, LSP-012, LSP-106, LSP-107
- **Đầu vào:** 03 §19, §31 (`.arx`, `.fas`, `.vlx`); 05 §15 (FAS/VLX chờ ý kiến pháp lý); chính sách LSP-115, bộ quét LSP-116; prototype `js/screens8.js` › `loadApp` (thông báo khi gặp `.vlx`/`.fas`).
- **Đầu ra:** `lisp/load/Loader.h/.cpp`; 6 hàm:
  - `load` (tham số onfailure; không có đuôi thì tìm theo thứ tự của đặc tả; gặp `.vlx`, `.fas` thì báo "chưa hỗ trợ" kèm lý do, đến khi có LSP-134; lỗi "LOAD failed: …"; trả giá trị biểu thức cuối; nạp lồng; nạp lại thì định nghĩa lại);
  - `autoload`: lệnh giả đăng ký vào INT-001, lần dùng đầu nạp tệp rồi chạy lệnh đúng một lần;
  - `arx`, `arxload`, `arxunload`, `autoarxload`: không hỗ trợ ObjectARX (03 §31), trả lỗi hoặc gọi onfailure theo đặc tả;
  - trước khi đọc tệp không tin cậy: chính sách LSP-115 rồi bộ quét LSP-116;
  - sổ tệp đã nạp (đường dẫn, thời điểm, hàm và lệnh định nghĩa) cho APPLOAD, bảng điều khiển LISP, nạp lại;
  - `lisp/tests/golden/load/`.
- **Tiêu chí nghiệm thu:**
  1. Ca vàng (§0) cho 6 hàm (≥ 150 ca).
  2. 100 tệp của bộ QA-008 nạp xong cho cùng tập ký hiệu được định nghĩa như kết quả kỳ vọng.
  3. Tệp không tin cậy không bị mở trước khi có quyết định của chính sách (hook truy cập tệp trong kiểm thử).
  4. Nạp tệp 50 KB ≤ 10 ms **[đề xuất]**.
  5. Đạt DoD-C.

### LSP-013 · Nạp khi khởi động, Startup Suite và lệnh APPLOAD
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** LSP-115, LSP-117, LSP-118, INT-001, INT-007, INT-020, NT-010, DOC-018
- **Lệnh:** APPLOAD
- **Màn hình:** UI-K01
- **Đầu vào:** 03 §19 (nạp tự động `acad.lsp`/`acaddoc.lsp`/`.mnl`, Startup Suite, tên `trion.lsp`/`triondoc.lsp` là P0), Phụ lục A (APPLOAD); 07.3 UI-K01; prototype `js/screens8.js` › `IMPL.APPLOAD`, `APPS` (đã nạp, lịch sử, Startup Suite); API DOC-018 (vòng đời bản vẽ), INT-007 (SCRIPT).
- **Đầu ra:**
  - `lisp/load/Startup.h/.cpp`: trình tự nạp theo vòng đời bản vẽ: `acad.lsp` và `trion.lsp` một lần mỗi phiên (mỗi bản vẽ nếu ACADLSPASDOC = 1); `acaddoc.lsp` và `triondoc.lsp` cho từng bản vẽ, trong không gian tên của bản vẽ đó (LSP-106); Startup Suite; `S::STARTUP` sau khi bản vẽ khởi tạo xong. Thứ tự ghi trong `lisp/docs/spec/startup.md`, chốt bằng ca vàng AutoCAD. Tìm trong support path, không tìm trong thư mục bản vẽ trừ khi LEGACYCODESEARCH = 1. Mọi tệp qua LSP-115; lỗi trong tệp khởi động được ghi log và báo, không dừng ứng dụng.
  - API nạp `.mnl` cho nhóm menu, dùng ở CUS-002.
  - Backend APPLOAD cho UI-K01: lọc loại tệp (`.lsp`, `.dcl`, `.scr`, `.vlx`/`.fas` hiện mờ đến khi có LSP-134, plugin TrionCAD), Nạp/Gỡ (LISP không gỡ được thì nói rõ theo đặc tả), danh sách đã nạp, lịch sử có ô "Thêm vào lịch sử", Startup Suite (thêm, bớt, lưu NT-010), dòng trạng thái.
  - Định tuyến theo loại: `.lsp` → `load`; `.dcl` → kiểm cú pháp bằng LSP-118, báo dòng và cột; `.scr` → SCRIPT (INT-007); `.tcplugin` → bộ xử lý do SDK-003 đăng ký.
  - Lệnh APPLOAD mở UI-K01 qua host; khi không có giao diện hoặc FILEDIA = 0 thì hỏi tên tệp trên dòng lệnh rồi nạp **[đề xuất]**; cổng phiên bản qua INT-020; kéo-thả tệp `.lsp` vào vùng vẽ dùng cùng API nạp.
  - `docs/specs/commands/APPLOAD.md`, `lisp/tests/startup/`.
- **Tiêu chí nghiệm thu:**
  1. 40 kịch bản ca vàng (tổ hợp tệp có mặt, ACADLSPASDOC, LEGACYCODESEARCH, SECURELOAD, nhiều bản vẽ) cho đúng thứ tự nạp và lời gọi `S::STARTUP`.
  2. Mở 100 bản vẽ: `acaddoc.lsp` chạy đúng một lần mỗi bản vẽ, trong không gian tên của bản vẽ đó.
  3. Startup Suite giữ qua khởi động lại; mục đã bớt không còn được nạp.
  4. `acaddoc.lsp` 50 KB làm chậm việc mở bản vẽ ≤ 50 ms; không có tệp khởi động thì ≤ 2 ms **[đề xuất]**.
  5. Mọi thao tác của UI-K01 có API và kiểm thử; bảng định tuyến loại tệp đúng 100%.
  6. Đạt DoD-C.

## 6. DCL

### LSP-118 · DCL: parser, `base.dcl` tự viết, cây tile
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-011, NT-012
- **Đầu vào:** 03 §19 (DCL là P0); 05 §11, §14 (fuzz parser DCL), §15 (tự viết tệp mẫu, giữ tên chuẩn); 07.3 UI-K11 (DCL lỗi thì báo số dòng và nội dung); tệp `.dcl` trong bộ QA-008; prototype `js/screens8.js` › `dclError`.
- **Đầu ra:**
  - `lisp/dcl/Parser.h/.cpp`: định nghĩa `ten : loai { thuoc_tinh = gia_tri; ... }`, tile lồng, tham chiếu `: ten { }` và `ten;`, nguyên mẫu và kế thừa thuộc tính, `@include "tep.dcl"`, chú thích `//` và `/* */`, chuỗi có ký tự thoát, số, `true`/`false`, nhận diện mã hóa như tệp LISP.
  - Mọi loại tile: button, edit_box, image_button, list_box, popup_list, radio_button, slider, toggle, column, row, boxed_column, boxed_row, radio_column, radio_row, boxed_radio_column, boxed_radio_row, dialog, image, text, spacer, spacer_0, spacer_1, text_part, concatenation, paragraph, errtile.
  - Mọi thuộc tính: action, alignment, allow_accept, aspect_ratio, big_increment, children_alignment, children_fixed_height, children_fixed_width, color, edit_limit, edit_width, fixed_height, fixed_width, fixed_width_font, height, initial_focus, is_bold, is_cancel, is_default, is_enabled, is_tab_stop, key, label, layout, list, max_value, min_value, mnemonic, multiple_select, password_char, small_increment, tabs, tab_truncate, value, width.
  - `lisp/dcl/base.dcl` tự viết: cùng tên tile định sẵn (ok_only, ok_cancel, ok_cancel_help, ok_cancel_help_info, ok_cancel_help_errtile…) và thuộc tính tương thích; mức `audit_level` của `dcl_settings`.
  - Cây tile bất biến `lisp/dcl/Tile.h`; lỗi kèm tệp:dòng:cột.
  - `lisp/tests/dcl/`, `lisp/fuzz/dcl_fuzz.cpp`, `lisp/docs/spec/dcl.md`.
- **Tiêu chí nghiệm thu:**
  1. Đọc 100% tệp `.dcl` hợp lệ của bộ QA-008; 100 tệp lỗi mẫu báo đúng dòng, cột và thông báo.
  2. Property-based 10⁶ tài liệu DCL sinh ngẫu nhiên: đọc → in → đọc cho cùng cây.
  3. Mọi tile định sẵn mà DCL trong bộ QA-008 tham chiếu đều có trong `base.dcl`.
  4. Đọc tệp DCL 100 KB ≤ 3 ms **[đề xuất]**; fuzz ≥ 1 giờ không lỗi.
  5. Đạt DoD-C.

### LSP-018 · DCL: layout xấp xỉ kích thước AutoCAD và nghiệm thu cây tile
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** LSP-118, QA-007
- **Đầu vào:** 05 §11 ("layout xấp xỉ kích thước của AutoCAD, render bằng Qt"); 07.3 UI-K11 (theo chủ đề và DPI của TrionCAD); bộ ảnh chụp hộp thoại DCL do doanh nghiệp pilot chụp trên AutoCAD có bản quyền, gửi qua cơ chế QA-007; cây tile của LSP-118.
- **Đầu ra:**
  - `lisp/dcl/Layout.h/.cpp`: hàm thuần (cây tile, số đo font của host) → hình chữ nhật của từng tile theo đơn vị ký tự và theo pixel. Áp dụng width, height, fixed_width, fixed_height, alignment, children_alignment, lề của cụm có khung, chia phần dư cho row/column, spacer, edit_width, chiều cao list_box theo số dòng, aspect_ratio của ảnh, độ rộng nhãn và popup.
  - Nghiệm thu chung parser và layout: API mà UIK dùng để vẽ tile bằng Qt.
  - `lisp/tests/dcl/layout/`.
- **Tiêu chí nghiệm thu:**
  1. 50 hộp thoại tham chiếu (DCL của bộ QA-008): vị trí và kích thước từng tile sai khác ≤ 1 đơn vị ký tự và ≤ 10% so với số đo trên ảnh AutoCAD **[đề xuất]**.
  2. Nhãn tiếng Việt dài hơn 40%: tile giãn ra, không chồng lên nhau (kiểm tự động giao các hình chữ nhật).
  3. Layout hộp thoại 200 tile ≤ 2 ms **[đề xuất]**; cùng số đo font cho kết quả giống từng bit trên 3 hệ điều hành.
  4. Đạt DoD-C.

### LSP-119 · Slide `.sld` và thư viện `.slb` cho tile ảnh
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** NT-011
- **Đầu vào:** tệp `.sld`, `.slb` trong bộ QA-008 và LISP cộng đồng; mô tả định dạng công khai (nếu có) và thực nghiệm trên tệp mẫu (05 §15).
- **Đầu ra:** `lisp/dcl/Slide.h/.cpp`: đọc slide và thư viện slide → danh sách vector và vùng tô, co giãn vào kích thước tile; giới hạn kích thước và số phần tử (đầu vào không tin cậy); `lisp/fuzz/slide_fuzz.cpp`.
- **Tiêu chí nghiệm thu:**
  1. 100 slide mẫu cho danh sách vector trùng bản dump chuẩn.
  2. Đọc 1 MB ≤ 5 ms **[đề xuất]**; fuzz ≥ 1 giờ không lỗi.
  3. Đạt DoD-C.

### LSP-019 · Runtime DCL: load_dialog, new_dialog, start_dialog, action_tile, list, ảnh
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** LSP-005, LSP-018, LSP-119
- **Màn hình:** UI-K11
- **Đầu vào:** 03 §19 (`load_dialog`, `new_dialog`… là P0); 07.3 UI-K11; prototype `js/screens8.js` › `SC.dclDemo`; layout LSP-018; evaluator lồng của LSP-003.
- **Đầu ra:** `lisp/dcl/Runtime.h/.cpp`; 22 hàm và biến gọi lại:
  - `load_dialog`, `unload_dialog`, `new_dialog` (hành động mặc định, vị trí), `start_dialog`, `done_dialog` (trạng thái, trả vị trí), `term_dialog`;
  - `action_tile`, `get_tile`, `set_tile`, `mode_tile` (0–4), `get_attr`, `client_data_tile`;
  - `start_list`, `add_list`, `end_list` (thao tác 1, 2, 3);
  - `start_image`, `end_image`, `fill_image`, `vector_image`, `slide_image` (LSP-119), `dimx_tile`, `dimy_tile`;
  - biến `$key`, `$value`, `$data`, `$reason` (1–4), `$x`, `$y`.
  - `lisp::dcl::DialogHost`: dựng hộp thoại từ cây đã layout, đặt và đọc giá trị, bật/tắt, focus, nội dung list, lệnh vẽ ảnh, hiện modal, sự kiện về callback. UIK hiện thực bằng Qt; `HeadlessDialogHost` cho kiểm thử nhận thao tác theo kịch bản (bấm, nhập, chọn dòng, nhấp đúp).
  - `start_dialog` tạm dừng LISP gọi nó; callback chạy trong ngữ cảnh tính lồng; gọi `command`, get*, `entsel` khi hộp thoại đang mở báo lỗi theo đặc tả; mẫu "ẩn hộp thoại, chọn điểm, mở lại" (`done_dialog` với mã trạng thái rồi vòng `new_dialog`) chạy đúng; hộp thoại lồng theo giới hạn của đặc tả; nhớ vị trí theo tên hộp thoại **[đề xuất]**.
  - `lisp/tests/golden/dcl/`.
- **Tiêu chí nghiệm thu:**
  1. 200 phiên hộp thoại theo kịch bản (DCL của bộ QA-008) cho cùng chuỗi `$key`/`$value`/`$reason`, mã `done_dialog`, giá trị `get_tile` như ca vàng.
  2. 30 LISP có DCL trong bộ QA-008 đạt từ đầu đến cuối với thao tác theo kịch bản.
  3. Một lần gọi callback ≤ 0,2 ms; `add_list` 10⁴ dòng ≤ 20 ms **[đề xuất]**; luồng UI không bị chặn khi hộp thoại mở.
  4. Lỗi trong callback đi đúng đường `*error*`, hộp thoại đóng sạch.
  5. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** vẽ tile bằng Qt và bộ quy tắc hiển thị từng loại tile (UIK, UI-K11).

## 7. Visual LISP: `vl-*`, ActiveX (`vlax-*`, `vla-*`) và reactor

### LSP-127 · Hàm `vl-*` thông dụng: danh sách, chuỗi, ký hiệu, tên tệp, thư mục
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** LSP-104, LSP-105, LSP-012, LSP-101
- **Đầu vào:** 03 §19 (Visual LISP là P1); 05 §11; số lần gọi từng hàm trong bộ QA-008 (LSP-101); quy tắc truy cập tệp của LSP-012. Nhóm này làm sớm vì LISP cộng đồng dùng nhiều, cần cho mốc ≥ 60% ở T24.
- **Đầu ra:**
  - `lisp/vl/VlList.cpp`, `VlString.cpp`, `VlFile.cpp`: `vl-consp`, `vl-list*`, `vl-list-length`, `vl-position`, `vl-remove`, `vl-remove-if`, `vl-remove-if-not`, `vl-member-if`, `vl-member-if-not`, `vl-some`, `vl-every`, `vl-sort`, `vl-sort-i`; `vl-string-elt`, `vl-string-search`, `vl-string-position`, `vl-string-mismatch`, `vl-string-subst`, `vl-string-translate`, `vl-string-trim`, `vl-string-left-trim`, `vl-string-right-trim`, `vl-string->list`, `vl-list->string`, `vl-prin1-to-string`, `vl-princ-to-string`; `vl-symbolp`, `vl-symbol-name`, `vl-symbol-value`; `vl-filename-base`, `vl-filename-directory`, `vl-filename-extension`, `vl-filename-mktemp`, `vl-directory-files`, `vl-file-directory-p`, `vl-file-size`, `vl-file-systime`, `vl-file-copy`, `vl-file-rename`, `vl-file-delete`, `vl-mkdir`.
  - Đặc thù đã biết (chỉ số từ 0, `vl-string-subst` chỉ thay lần đầu, `vl-sort` với phần tử bằng nhau) chốt bằng đáp án AutoCAD qua QA-007.
  - `lisp/docs/spec/vl.md`, `lisp/tests/golden/vl/`.
- **Tiêu chí nghiệm thu:**
  1. Mọi hàm đạt ca vàng theo quy ước §0; khớp 100% đáp án AutoCAD hiện có.
  2. 100% lời gọi các hàm trên trong bộ QA-008 (đếm bằng LSP-101) chạy được.
  3. `vl-sort` 10⁵ số với `'<` ≤ 30 ms; `vl-string-search`, `vl-string-subst` trên chuỗi 1 MB ≤ 5 ms; `vl-directory-files` thư mục 10⁴ tệp ≤ 50 ms (máy tầm trung) **[đề xuất]**.
  4. Hàm sửa tệp đi qua kiểm tra của LSP-012; đường dẫn tiếng Việt có dấu đúng trên 3 hệ điều hành.
  5. Đạt DoD-C.

### LSP-121 · Hàm `vlax-curve-*` trên kernel hình học
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** LSP-112, GEO-005, GEO-006, GEO-007, GEO-021, QA-009
- **Đầu vào:** 03 §19; số lần gọi trong QA-008 (LSP-101; nhóm này dùng nhiều trong LISP đo đạc); API đường cong GEO-005, GEO-006, GEO-007, diện tích GEO-021; đáp án AutoCAD qua QA-007.
- **Đầu ra:**
  - `lisp/vl/Curve.h/.cpp`: `vlax-curve-getStartParam`, `-getEndParam`, `-getStartPoint`, `-getEndPoint`, `-getPointAtParam`, `-getParamAtPoint`, `-getDistAtParam`, `-getParamAtDist`, `-getDistAtPoint`, `-getPointAtDist`, `-getClosestPointTo` (có cờ kéo dài), `-getClosestPointToProjection`, `-getFirstDeriv`, `-getSecondDeriv`, `-getArea`, `-isClosed`, `-isPeriodic`, `-isPlanar`.
  - Nhận ENAME và VLA-OBJECT. Tham số hóa theo từng loại như AutoCAD: LINE theo khoảng cách, cung và đường tròn theo góc, polyline theo chỉ số đỉnh cộng phần lẻ, spline theo nút, ellipse, RAY, XLINE. Đối tượng không phải đường cong trả lỗi theo đặc tả.
  - Bảng độ dài cộng dồn của polyline và spline, cache theo phiên bản entity.
  - `lisp/docs/spec/curve.md`, `lisp/tests/golden/curve/`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 2.000 ca vàng trên mọi loại đường cong, kể cả polyline có bulge, kín, suy biến (độ dài 0), spline tuần hoàn; sai số tương đối ≤ 10⁻⁹ so với đáp án AutoCAD **[đề xuất]**.
  2. 10⁶ ca property-based (QA-009): khoảng cách tại điểm lấy theo khoảng cách d trả lại d; tham số đơn điệu theo khoảng cách; khoảng cách tại tham số cuối bằng độ dài của GEO.
  3. Một lời gọi `getDistAtPoint` trên polyline 10⁴ đỉnh ≤ 5 µs sau lần đầu, không cấp phát (NT-004) **[đề xuất]**.
  4. 100% lời gọi `vlax-curve-*` trong bộ QA-008 chạy được.
  5. Đạt DoD-C.

### LSP-014 · Hàm `vl-*` còn lại và nghiệm thu nhóm `vl-*`
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** LSP-127, LSP-005, LSP-106, LSP-117
- **Đầu vào:** danh sách tên hàm `vl-*` công khai của AutoLISP (chỉ dùng tên, 05 §15); các hàm `vl-*` đã làm ở LSP-005, LSP-006, LSP-012, LSP-106, LSP-127; số lần gọi LSP-101.
- **Đầu ra:**
  - `lisp/vl/VlMisc.cpp`: `vl-load-com`, `vl-load-reactors` (đã nạp sẵn, trả giá trị như AutoCAD); `vl-acad-defun`, `vl-acad-undefun` (đánh dấu hàm gọi được từ C API, SDK-001 dùng); `vl-vbaload`, `vl-vbarun` (báo không hỗ trợ VBA, 03 §19); `vl-arx-import`, `vl-vlx-loaded-p`, `vl-list-loaded-vlx`, `vl-unload-vlx`, `vl-get-resource` (trả theo đặc tả đến khi có LSP-134).
  - Bảng phủ `lisp/docs/spec/vl-coverage.md`: mỗi hàm `vl-*` một dòng (task làm, trạng thái: đủ hoặc báo lỗi theo đặc tả). Kiểm thử phủ: CI đỏ khi một tên trong danh sách không có cài đặt hay dòng đặc tả.
- **Tiêu chí nghiệm thu:**
  1. 100% tên `vl-*` trong danh sách có cài đặt hoặc lỗi theo đặc tả.
  2. Toàn bộ ca vàng nhóm `vl-*` của mọi task đạt; khớp 100% đáp án AutoCAD hiện có.
  3. 100% lời gọi `vl-*` trong bộ QA-008 chạy được, trừ hàm cần VLX (LSP-134).
  4. Đạt DoD-C.

### LSP-120 · VARIANT và SAFEARRAY: `vlax-make-variant`, `vlax-safearray-*`, hằng `vlax-vb*`
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** LSP-002, LSP-104
- **Đầu vào:** 05 §11 (kiểu VARIANT, SAFEARRAY); quy tắc đổi kiểu của OLE Automation (tài liệu công khai của Microsoft về VARIANT và SAFEARRAY); đáp án AutoCAD qua QA-007.
- **Đầu ra:**
  - `lisp/com/Variant.h/.cpp`: `vlax-make-variant`, `vlax-variant-type`, `vlax-variant-value`, `vlax-variant-change-type`; `vlax-make-safearray`, `vlax-safearray-type`, `vlax-safearray-fill`, `vlax-safearray-get-element`, `vlax-safearray-put-element`, `vlax-safearray-get-dim`, `vlax-safearray-get-l-bound`, `vlax-safearray-get-u-bound`, `vlax-safearray->list`; `vlax-3d-point`, `vlax-tmatrix`; hằng `vlax-vb*`, `vlax-true`, `vlax-false`.
  - Mảng nhiều chiều, cận dưới tùy ý, kiểm biên; đổi kiểu (làm tròn, tràn số, chuỗi ↔ số) theo quy tắc OLE; in `#<variant …>`, `#<safearray…>` như AutoCAD.
  - `lisp/docs/spec/variant.md`, `lisp/tests/golden/variant/`.
- **Tiêu chí nghiệm thu:**
  1. Ca vàng đạt; bảng đổi kiểu giữa mọi cặp kiểu có hằng `vlax-vb*` khớp 100% đáp án AutoCAD.
  2. 10⁶ ca property-based: danh sách → safearray → danh sách giữ nguyên giá trị và kiểu.
  3. Chỉ số ngoài cận, sai số chiều trả đúng thông báo lỗi; ASan sạch.
  4. `vlax-safearray-fill` 10⁶ phần tử ≤ 20 ms **[đề xuất]**.
  5. Đạt DoD-C.

### LSP-015 · `vlax-*` trên mô hình đối tượng: mô tả mô hình, thuộc tính, phương thức, collection
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** LSP-120, LSP-121, LSP-107, LSP-112, DOC-005, DOC-017, DOC-018, DWG-001
- **Đầu vào:** 05 §11 (`vla-*` sinh từ mô tả mô hình, dùng chung schema §6); 03 §19; metadata thuộc tính DOC-017; tên lớp, thuộc tính, phương thức ActiveX mà bộ QA-008 gọi (LSP-101; chỉ dùng tên, 05 §15).
- **Đầu ra:**
  - Mô tả mô hình `lisp/com/model/*.om` **[đề xuất]**: lớp (Application, Documents, Document, Utility, collection bảng và block, lớp entity), thuộc tính (kiểu VARIANT, chỉ đọc hay không, ánh xạ tới mã thuộc tính DOC-017 hoặc hàm truy cập C++), chữ ký phương thức. Bộ sinh `lisp/com/gen/` tạo bảng điều phối lúc build; thiếu ánh xạ thì build lỗi.
  - `lisp/com/Dispatch.h/.cpp`: `vlax-get-acad-object`, `vlax-get-property`, `vlax-put-property`, `vlax-invoke-method`, `vlax-get`, `vlax-put`, `vlax-invoke`, `vlax-property-available-p`, `vlax-method-applicable-p`, `vlax-dump-object`, `vlax-ename->vla-object`, `vlax-vla-object->ename`, `vlax-erased-p`, `vlax-read-enabled-p`, `vlax-write-enabled-p`, `vlax-object-released-p`, `vlax-release-object`, `vlax-typeinfo-available-p`, `vlax-for`, `vlax-map-collection`, `vlax-ldata-put`, `-get`, `-delete`, `-list`, `-test`, `vlax-add-cmd`, `vlax-remove-cmd` (qua LSP-107), `vlax-product-key`.
  - Ghi đi qua DOC-007. COM ngoài tiến trình (`vlax-get-or-create-object`, `vlax-create-object`, `vlax-get-object`, `vlax-import-type-library`) trả `nil` kèm thông báo (P3, 03 §19).
  - `lisp/docs/spec/vlax.md`, `lisp/tests/golden/vlax/`.
- **Tiêu chí nghiệm thu:**
  1. 100% lớp, thuộc tính, phương thức mà bộ QA-008 gọi có trong mô tả mô hình.
  2. Đọc rồi ghi mọi thuộc tính của tập con trên bản vẽ mẫu QA-001: giá trị và kiểu VARIANT khớp 100% đáp án AutoCAD (QA-007); `vlax-dump-object` khớp từng ký tự trên 30 lớp.
  3. Tên không có, thuộc tính chỉ đọc, đối tượng đã xóa: thông báo "ActiveX Server returned the error: …" như AutoCAD; `vl-catch-all-apply` bắt được.
  4. 10⁵ lần `vlax-get-property` trên LINE ≤ 100 ms; `vlax-for` qua model space 10⁶ entity ≤ 1 s (máy tầm trung) **[đề xuất]**.
  5. Đạt DoD-C.

### LSP-017 · `vla-*` sinh từ mô hình đối tượng và phương thức thông dụng
- **Mốc:** 1.0 · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** LSP-015, LSP-007, LSP-011, DOC-008, DOC-014, GEO-010, INT-016, CMD:OFFSET, CMD:EXPLODE
- **Đầu vào:** 03 §19 (`vla-*` P2, ưu tiên tập con LISP phổ biến hay dùng); 06 §5 (LISP ≥ 90% với tập con `vla-*`); mô tả mô hình LSP-015; số lần gọi LSP-101; biến đổi entity (DOC-115) và sao chép sâu (DOC-118) qua neo DOC-014.
- **Đầu ra:**
  - Bộ sinh `lisp/com/gen/` tạo `vla-get-<thuộc tính>`, `vla-put-<thuộc tính>`, `vla-<phương thức>` cho mọi thành viên của mô tả; ký hiệu tạo lười khi tra lần đầu.
  - `lisp/com/methods/`, thân phương thức của tập con:
    - tạo: `AddLine`, `AddCircle`, `AddArc`, `AddLightWeightPolyline`, `AddPolyline`, `AddText`, `AddMText`, `AddPoint`, `AddSpline`, `AddEllipse`, `AddHatch` kèm `AppendOuterLoop`, `AppendInnerLoop`, `Evaluate`, `InsertBlock`, `AddDimAligned`, `AddDimRotated`; collection: `Item`, `Add`, `Delete`;
    - sửa: `Move`, `Copy`, `Rotate`, `ScaleEntity`, `Mirror`, `Offset`, `Explode`, `IntersectWith`, `GetBoundingBox`, `GetAttributes`, `GetDynamicBlockProperties` (đọc), `Update`, `Highlight`;
    - Document, Utility, Application: `StartUndoMark`, `EndUndoMark`, `Regen`, `SendCommand`, `GetVariable`, `SetVariable`, `GetPoint`, `GetEntity`, `GetString`, `GetReal`, `GetInteger`, `GetKeyword`, `InitializeUserInput`, `AngleToReal`, `RealToString`, `PolarPoint`, `TranslateCoordinates`, `ZoomExtents`, `ZoomAll`, `ZoomWindow`.
  - `lisp/docs/spec/vla.md` (tập con và số lần gọi làm căn cứ), `lisp/tests/golden/vla/`.
- **Tiêu chí nghiệm thu:**
  1. Mọi `vla-*` mà bộ QA-008 gọi đều có cài đặt; CI đối chiếu tự động với LSP-101.
  2. Mỗi phương thức đạt ca vàng; hình học, giá trị trả và kiểu VARIANT khớp 100% đáp án AutoCAD (QA-007).
  3. Sửa bản vẽ qua DOC-007; khối `StartUndoMark`…`EndUndoMark` hoàn tác một lần; `SendCommand` chạy sau khi LISP trả quyền, như AutoCAD.
  4. Khởi động LISP chậm thêm ≤ 5 ms và tốn thêm ≤ 2 MB vì ký hiệu `vla-*`; 10⁵ lần `vla-AddLine` ≤ 300 ms **[đề xuất]**.
  5. Đạt DoD-C.

### LSP-122 · Reactor đối tượng `vlr-object-reactor` và reactor bền
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** LSP-015, DOC-005, DOC-007, DOC-014, DWG-022
- **Đầu vào:** 03 §19 (`vlr-*` P1); reactor lưu trong bản vẽ (DOC-005) và phát reactor bền sau commit (DOC-106); ánh xạ ID khi sao chép (DOC-118 qua DOC-014).
- **Đầu ra:**
  - `lisp/vlr/ObjectReactor.h/.cpp`: `vlr-object-reactor`, `vlr-owner-add`, `vlr-owner-remove`, `vlr-owners`, `vlr-pers`, `vlr-pers-p`, `vlr-pers-list`, `vlr-pers-release`; mọi phản ứng của reactor đối tượng (`:vlr-modified`, `:vlr-erased`, `:vlr-unerased`, `:vlr-copied`, `:vlr-cancelled`, `:vlr-openedForModify`, `:vlr-subObjModified`, `:vlr-modifyUndone`, `:vlr-modifiedXData`, `:vlr-unappended`, `:vlr-reappended`, `:vlr-objectClosed`, `:vlr-goodbye`).
  - Hàng đợi phản ứng: gom trong transaction, gọi callback ở điểm an toàn sau commit theo thứ tự sự kiện. Callback sửa chính đối tượng đang báo hoặc gọi `command` thì báo lỗi theo đặc tả, không lặp vô hạn.
  - Reactor bền lưu cùng bản vẽ, gắn lại callback theo tên hàm lúc phát; giữ nguyên reactor do AutoCAD lưu.
  - `lisp/tests/golden/vlr/object/`.
- **Tiêu chí nghiệm thu:**
  1. 50 kịch bản sửa đối tượng: chuỗi phản ứng (tên, owner, tham số) khớp 100% đáp án AutoCAD (QA-007; LISP ghi nhật ký phản ứng ra tệp).
  2. Bản vẽ có reactor bền do AutoCAD lưu: mở, sửa, lưu, mở lại vẫn đủ reactor và owner (so bằng `dwgdump`).
  3. Không có reactor: lệnh sửa trên BENCH-STD chậm thêm ≤ 1%; 10⁴ reactor, sửa 10⁵ đối tượng: phát phản ứng ≤ 50 ms **[đề xuất]**.
  4. Lỗi trong callback không làm hỏng transaction; vết lỗi ghi đúng.
  5. Đạt DoD-C.

### LSP-016 · Reactor `vlr-*`: editor, lệnh, LISP, sysvar, tài liệu, bản vẽ; API quản lý
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** LSP-122, LSP-117, INT-002, INT-005, INT-018, DOC-008, DOC-013, DOC-018
- **Đầu vào:** 03 §19; nguồn sự kiện: vòng đời lệnh (INT-002), đổi sysvar (INT-005), nhấp đúp và chuột phải (INT-018), undo (DOC-008), xref (DOC-013), phiên nhiều bản vẽ và trạng thái tài liệu (DOC-018, DOC-112), nạp ứng dụng (LSP-117).
- **Đầu ra:**
  - `lisp/vlr/`: `vlr-editor-reactor`, `vlr-command-reactor`, `vlr-lisp-reactor`, `vlr-sysvar-reactor`, `vlr-docmanager-reactor`, `vlr-dwg-reactor`, `vlr-acdb-reactor`, `vlr-mouse-reactor`, `vlr-undo-reactor`, `vlr-xref-reactor`, `vlr-insert-reactor`, `vlr-wblock-reactor`, `vlr-linker-reactor`, `vlr-miscellaneous-reactor`, `vlr-toolbar-reactor`, đủ tên phản ứng của từng loại.
  - Quản lý: `vlr-add`, `vlr-remove`, `vlr-remove-all`, `vlr-added-p`, `vlr-reactors`, `vlr-reactions`, `vlr-reaction-set`, `vlr-reaction-names`, `vlr-types`, `vlr-type`, `vlr-data`, `vlr-data-set`, `vlr-current-reaction-name`, `vlr-notification`, `vlr-trace-reaction`, `vlr-beep-reaction`.
  - Chỉ nghe nguồn sự kiện khi có reactor loại đó; phát ở điểm an toàn trên luồng UI; reactor chỉ báo cho bản vẽ của nó, trừ khi `vlr-notification` đặt cho mọi bản vẽ.
  - `lisp/docs/spec/vlr.md`, `lisp/tests/golden/vlr/`.
- **Tiêu chí nghiệm thu:**
  1. 100 kịch bản (mở, lưu, đóng, lệnh lồng và trong suốt, LISP, sysvar, undo, xref, insert, wblock, đổi bản vẽ): chuỗi phản ứng khớp 100% đáp án AutoCAD (QA-007).
  2. Callback gọi `command` báo lỗi như AutoCAD, các reactor khác vẫn chạy; lỗi trong callback không làm treo lệnh đang chạy.
  3. Không có reactor: 0 thuê bao sự kiện; 50 reactor editor làm mỗi lệnh chậm thêm ≤ 0,1 ms **[đề xuất]**.
  4. 100% LISP dùng `vlr-*` trong bộ QA-008 chạy đúng.
  5. Đạt DoD-C, DoD-L.

## 8. Nghiệm thu bộ LISP và hiệu năng

### LSP-128 · Tự trả lời hộp thoại DCL không giao diện cho `lisprun`
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** LSP-019, LSP-006
- **Đầu vào:** nhu cầu của bộ LISP QA-008 và bộ so sánh bản vẽ QA-106 ở EA: chạy LISP có DCL trong CI không cần giao diện; `HeadlessDialogHost` và biến gọi lại của LSP-019; trình chạy `lisprun` (LSP-006).
- **Đầu ra:**
  - `lisp/dcl/AutoResponder.h/.cpp` trên `HeadlessDialogHost`, đọc tệp trả lời JSON `*.dclresp.json` **[đề xuất]**: luật khớp theo tên hộp thoại, tệp DCL, lần mở thứ mấy; thao tác `set_tile`, chọn dòng list, bật toggle hoặc radio, đặt slider, bấm nút theo `key`, nhấp đúp (`$reason` 4), nhấp ảnh kèm `$x`, `$y`.
  - Không có luật khớp: áp chính sách mặc định ghi trong tệp (bấm `accept` hoặc `cancel`) và ghi cảnh báo. Giới hạn số thao tác mỗi hộp thoại và số hộp thoại mỗi lần chạy.
  - Chế độ ghi: sinh tệp trả lời khung (hộp thoại, tile, khóa, giá trị ban đầu) từ một lần chạy.
  - Tham số `lisprun --dcl <tệp>`, `--dcl-record <tệp>`; cùng API cho `trion-console` (INT-019); nhật ký hộp thoại nằm trong bản ghi của `lisprun`.
  - `lisp/tests/dcl/responder/`.
- **Tiêu chí nghiệm thu:**
  1. 30 LISP có DCL của bộ QA-008 chạy hết bằng tệp trả lời, kết quả trùng ca vàng; chạy hai lần cho bản ghi giống từng byte trên 3 hệ điều hành.
  2. Chạy lại bằng tệp do chế độ ghi sinh ra cho cùng chuỗi `$key`, `$value`, `$reason` và mã `done_dialog`.
  3. Thiếu luật: cảnh báo nêu tên hộp thoại và khóa tile. Vượt giới hạn thao tác: dừng LISP có thông báo, mã thoát khác 0, không treo.
  4. Chi phí ≤ 1 ms mỗi hộp thoại, ngoài thời gian chạy LISP **[đề xuất]**.
  5. Đạt DoD-C.

### LSP-123 · Nghiệm thu bộ LISP tháng 24: ≥ 60% tệp đạt
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** QA-008, LSP-128, LSP-127, LSP-005, LSP-007, LSP-008, LSP-009, LSP-010, LSP-011, LSP-012, LSP-013, LSP-019
- **Đầu vào:** 06 §4 (tháng 22–24: engine lõi, DCL, cầu nối `(command)`, bộ LISP ≥ 60%); 05 §11 (mỗi LISP có kịch bản và kết quả kỳ vọng); bộ chạy QA-008; thống kê hàm LSP-101.
- **Đầu ra:**
  - `lisp/tools/lisptriage/`: phân loại tự động tệp trượt theo nguyên nhân (thiếu hàm kèm tên, sai giá trị, lệch prompt khi gọi `command`, DCL, chặn bảo mật, crash, quá thời gian) → JSON cho bảng chỉ số QA-010.
  - Cổng đêm: tỉ lệ đạt dưới ngưỡng, hoặc một tệp đang đạt chuyển sang trượt, thì báo đỏ.
  - Sửa mọi nguyên nhân thuộc `lisp/`; nguyên nhân thuộc module khác ghi issue kèm tệp tái hiện.
  - `lisp/docs/qa/lisp-suite-t24.md`: kết quả và nguyên nhân còn mở.
- **Tiêu chí nghiệm thu:**
  1. ≥ 60% tệp của bộ QA-008 hiện có đạt (06 §4), cùng kết quả trên 3 hệ điều hành.
  2. 100% tệp trượt có nguyên nhân; nhóm "không rõ" ≤ 5% **[đề xuất]**.
  3. 0 crash, 0 treo (giới hạn 60 s mỗi tệp **[đề xuất]**); ASan sạch trên cả bộ.
  4. Cả bộ chạy ≤ 10 phút trên runner CI 8 luồng, chia tệp cho nhiều tiến trình **[đề xuất]**.
  5. Đạt DoD-C.

### LSP-124 · Nghiệm thu bộ LISP cổng G2: ≥ 75% tệp đạt
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** LSP-123, LSP-121, QA-007, INT-019
- **Đầu vào:** 06 §4 (tháng 26–27: bộ LISP ≥ 75%; cổng G2: toàn bộ P0 đạt kiểm thử tham chiếu); báo cáo phân loại LSP-123; đáp án AutoCAD (QA-007).
- **Đầu ra:**
  - Sửa các nhóm nguyên nhân lớn nhất theo báo cáo LSP-123 trong `lisp/`; issue cho module khác kèm tệp tái hiện.
  - So với đáp án AutoCAD cho mọi tệp có đáp án: DWG so bằng QA-106, chuỗi in ra so từng ký tự.
  - Mỗi tệp chạy qua cả `lisprun` và `trion-console`.
  - `lisp/docs/qa/lisp-suite-g2.md`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 75% tệp của bộ QA-008 đạt tại cổng G2 (06 §4), trên 3 hệ điều hành.
  2. 100 LISP phổ biến nhất (theo metadata QA-008) đạt ≥ 85% **[đề xuất]**.
  3. 100% hàm mà bộ gọi (LSP-101) có cài đặt, trừ nhóm của bản 1.0: `vlax-*` (ngoài `vlax-curve-*`), `vla-*`, `vlr-*`.
  4. `lisprun` và `trion-console` cho cùng bản ghi trên mọi tệp đạt.
  5. Đạt DoD-C.

### LSP-020 · Hiệu năng LISP: bộ benchmark, tối ưu evaluator, bytecode khi số đo cần
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** LSP-003, LSP-112, LSP-123, NT-003, NT-004
- **Đầu vào:** 05 §11 (biên dịch bytecode nếu benchmark cho thấy cần); thời gian chạy các LISP nặng nhất của bộ QA-008 trên AutoCAD, doanh nghiệp pilot đo qua cơ chế QA-007 **[đề xuất]**.
- **Đầu ra:**
  - `lisp/bench/`: micro (gọi hàm, `setq` và biến động, `cons`/`append`/`assoc` trên danh sách 10⁵, `strcat`, `mapcar` với `lambda`, đệ quy sâu 10⁴) và macro (20 LISP nặng nhất của QA-008; vòng `ssget`/`entget`/`entmod` trên 10⁵ entity), chạy trong cổng NT-003.
  - Tối ưu theo profile (NT-004): tra ký hiệu, khung ngăn xếp, cấp phát cons.
  - Nếu sau tối ưu vẫn chưa đạt tiêu chí 1 thì biên dịch USUBR sang bytecode (`lisp/vm/`): giữ dynamic scoping, `*error*`, tạm dừng và tiếp tục khi chờ input; `defun-q` vẫn là danh sách.
- **Tiêu chí nghiệm thu:**
  1. 20 benchmark macro bằng hoặc nhanh hơn AutoCAD trên cùng máy; `(repeat 1000000 (setq i (1+ i)))` ≤ 50 ms trên máy tầm trung **[đề xuất]**.
  2. Bật hay tắt tối ưu (và bytecode nếu có): toàn bộ ca vàng và bộ QA-008 cho bản ghi giống từng byte.
  3. `getpoint`, `(command … pause)`, `start_dialog` trong hàm đã tối ưu vẫn tạm dừng và chạy tiếp đúng.
  4. Đạt DoD-C.

### LSP-125 · Nghiệm thu bộ LISP bản 1.0: ≥ 90% tệp đạt, gồm tập con `vla-*`
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** LSP-124, LSP-014, LSP-015, LSP-016, LSP-017, LSP-020
- **Đầu vào:** 06 §5 (LISP ≥ 90% với tập con `vla-*`); bộ QA-008 500 tệp; báo cáo phân loại LSP-123.
- **Đầu ra:** sửa nguyên nhân còn lại trong `lisp/`; issue cho module khác; cổng đêm nâng ngưỡng lên 90%; `lisp/docs/qa/lisp-suite-1.0.md`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 90% tệp của bộ QA-008 (500 tệp) đạt trên 3 hệ điều hành.
  2. Nhóm tệp dùng `vlax-*`, `vla-*` hoặc `vlr-*` đạt ≥ 85% **[đề xuất]**.
  3. Tệp đạt khớp 100% đáp án AutoCAD (QA-007) ở mọi tệp có đáp án.
  4. Tổng thời gian chạy cả bộ không quá thời gian trên AutoCAD cùng máy (số đo LSP-020).
  5. Đạt DoD-C.

## 9. Sau v1.0: FAS/VLX, bảng điều khiển và gỡ lỗi LISP

### LSP-134 · Nạp LISP biên dịch `.fas` và `.vlx` (sau ý kiến pháp lý)
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** LSP-014, LSP-106, LSP-115, LSP-117
- **Đầu vào:** 03 §19 (FAS/VLX P2, cần đánh giá pháp lý trước); 05 §11, §15 (chỉ làm khi luật sư xác nhận ngoại lệ tương tác áp dụng được). **Điều kiện bắt đầu:** ý kiến pháp lý bằng văn bản, lưu ở `docs/legal/`, nêu rõ phạm vi được phép. Tệp `.fas`, `.vlx` trong bộ QA-008.
- **Đầu ra:**
  - `lisp/fas/`: đọc container VLX (nhiều FAS, tài nguyên DCL và văn bản), đọc FAS và chạy trên evaluator LSP-003 trong phạm vi ý kiến pháp lý cho phép; giới hạn kích thước, kiểm biên (đầu vào không tin cậy).
  - VLX có không gian tên riêng: `vl-doc-export`, `vl-doc-import`, `vl-get-resource`, `vl-list-loaded-vlx`, `vl-vlx-loaded-p`, `vl-unload-vlx`, `vl-arx-import` chạy đủ, thay phần báo lỗi ở LSP-014, LSP-106.
  - `load`, APPLOAD, Startup Suite nhận `.fas`, `.vlx` qua chính sách SECURELOAD (LSP-115).
  - `lisp/fuzz/fas_fuzz.cpp`, `lisp/tests/golden/fas/`.
- **Tiêu chí nghiệm thu:**
  1. PR có ý kiến pháp lý bằng văn bản; phần cài đặt không vượt phạm vi ý kiến cho phép.
  2. ≥ 95% tệp `.fas`, `.vlx` của bộ QA-008 nạp và chạy cho kết quả như ca vàng **[đề xuất]**.
  3. Tệp hỏng báo lỗi sạch; fuzz ≥ 1 giờ không lỗi.
  4. Nạp VLX 1 MB ≤ 50 ms **[đề xuất]**.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** tạo FAS/VLX (MAKELISPAPP); giải mã nội dung bị khóa nếu ý kiến pháp lý không cho phép.

### LSP-126 · Backend bảng điều khiển LISP và gỡ lỗi: REPL, vết gọi hàm, VS Code
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** LSP-003, LSP-005, LSP-013, LSP-107, LSP-020
- **Màn hình:** UI-K09
- **Đầu vào:** 07.3 UI-K09 (REPL có lịch sử, kết quả ghi kiểu, biến toàn cục, tệp đã nạp và nạp lại, dừng khi lỗi kèm vết gọi hàm, "Mở trong VS Code", đo thời gian chạy); 03 §19 (gỡ lỗi LISP P2); prototype `js/screens20.js` (LISPCONSOLE: "Dừng khi lỗi, hiện vết gọi hàm", thời gian chạy, tab biến và tab tệp).
- **Đầu ra:**
  - `lisp/debug/Hooks.h`: móc gọi, trả, lỗi, từng biểu thức kèm tệp:dòng từ reader; khi tắt chỉ tốn một phép kiểm cờ.
  - `lisp/console/Console.h/.cpp` cho UIK-018: tính chuỗi trong không gian tên của bản vẽ, kết quả kèm kiểu, gom `princ`/`print`, Esc ngắt, thời gian từng lần tính; biến toàn cục có lọc và phân trang; tệp đã nạp kèm hàm và lệnh `C:`, nạp lại; dừng khi lỗi: khung gọi (hàm, đối số, biểu thức, tệp:dòng), biến cục bộ từng khung, tính biểu thức trong khung đã chọn.
  - `lisp/debug/dap/`: máy chủ Debug Adapter Protocol (breakpoint theo dòng, step in/over/out, pause, stack, biến, evaluate); chỉ nghe 127.0.0.1, cổng ngẫu nhiên, có token, mặc định tắt **[đề xuất]**. `lisp/vscode/`: extension tối thiểu để attach.
- **Tiêu chí nghiệm thu:**
  1. 50 phiên REPL theo kịch bản cho kết quả và nhãn kiểu như ca vàng; Esc dừng vòng lặp vô hạn ≤ 50 ms.
  2. 30 ca lỗi: khung gọi và biến cục bộ đúng ca vàng; tiếp tục sau khi xem thì `*error*` chạy đúng.
  3. Client DAP theo kịch bản: 15 loại request đúng giao thức; breakpoint dừng đúng dòng ở 100 ca.
  4. Móc tắt: chậm ≤ 2% trên benchmark LSP-020; bật mà không có breakpoint: ≤ 30% **[đề xuất]**.
  5. Fuzz parser DAP ≥ 1 giờ; từ chối kết nối ngoài loopback hoặc sai token.
  6. Đạt DoD-C.

## 10. Macro menu, CUIX, chuyển cài đặt, workspace

### CUS-104 · Engine macro menu: `^C^C`, `;`, `\`, `*`, ký tự điều khiển, `$M=` DIESEL, dạng `"M="` của `menucmd`
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** INT-112, INT-001, LSP-111
- **Đầu vào:** 03 §19 (DIESEL, macro menu `^C^C` là P1); 07.3 UI-K07 (lệnh có macro); DIESEL của INT-112; `menucmd` của LSP-111; macro trong định nghĩa ribbon, toolbar (UIA-005, UIA-006) và trong CUIX mẫu.
- **Đầu ra:**
  - `custom/macro/Macro.h/.cpp`: parser macro → dòng token: `^C` và `^C^C` (hủy), `*` đầu macro (lặp đến khi Esc hoặc lệnh khác), `;` và dấu cách (Enter, trừ ngữ cảnh nhận chuỗi có dấu cách theo đặc tả), `\` (dừng chờ nhập), `^M`, `^I`, `^H`, `^Z`, `^P` (MENUECHO), `^B`, `^D`, `^E`, `^G`, `^O`, `^T`, `^V`; tiền tố `_`, `.`, `'`; biểu thức LISP trong macro.
  - `$M=`: tính DIESEL qua INT-112 ngay lúc chạy, chèn kết quả vào dòng token; lồng `$(if,…)`, `$(getvar,…)`; lỗi DIESEL cho chuỗi lỗi như AutoCAD.
  - `MacroRunner` dạng coroutine trên giao diện `InputSink` (bản thật ở CUS-001, bản giả cho kiểm thử); nhánh `"M="` của `menucmd` trả chuỗi kết quả DIESEL.
  - `custom/docs/spec/macro.md`, `custom/tests/macro/`, `custom/fuzz/macro_fuzz.cpp`.
- **Tiêu chí nghiệm thu:**
  1. 300 macro (ribbon, toolbar, CUIX mẫu, macro cộng đồng) cho dòng token và chuỗi input trùng ca vàng.
  2. 200 ca `$M=` và `(menucmd "M=…")` khớp 100% đáp án AutoCAD (QA-007).
  3. Token lạ hoặc cú pháp menu cũ (`$S=`, `$P1=`…) báo lỗi rõ, không để lệnh dở dang.
  4. Phân tích 10⁴ macro ≤ 10 ms **[đề xuất]**; fuzz ≥ 1 giờ không lỗi (macro đến từ CUIX là đầu vào không tin cậy).
  5. Đạt DoD-C.

### CUS-001 · Thực thi macro menu qua INT-002: tích hợp và nghiệm thu
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CUS-104, INT-112, INT-002, INT-003, INT-005, INT-018
- **Đầu vào:** README §4.2 (UI sửa bản vẽ chỉ qua lệnh); engine CUS-104; input pipeline INT-002 (nguồn macro và menu), prompt INT-003, MENUECHO và PICKFIRST (INT-005), hành động nhấp đúp (INT-018).
- **Đầu ra:**
  - `custom/macro/PipelineSink.h/.cpp`: nối `MacroRunner` vào INT-002 thành một nguồn input. `\` chờ đúng một lần nhập (chọn đối tượng thì chờ đến khi chọn xong), cho phép OSNAP, lệnh trong suốt và DYN trong lúc chờ; `^C^C` hủy cả lệnh lồng; `*` lặp dừng bằng Esc.
  - API `custom::runMacro(chuỗi, nguồn)` cho nút ribbon, toolbar, menu, QAT, nhấp đúp, tệp tùy biến; MENUECHO quyết định phần hiện trên dòng lệnh.
  - `custom/tests/macro/pipeline/`.
- **Tiêu chí nghiệm thu:**
  1. 50 macro chạy bằng nút, bằng gõ phím tương đương, bằng `.scr` và `(command …)`: DWG kết quả giống nhau (so `dwgdump`), mỗi lệnh một mark undo.
  2. PICKFIRST: macro không có `^C^C` giữ tập chọn sẵn, có `^C^C` thì bỏ, đúng đặc tả.
  3. Từ lúc bấm nút đến khi lệnh đầu nhận input ≤ 16 ms **[đề xuất]**; `*` lặp dừng sau Esc ≤ 1 khung.
  4. 30 macro có `$(getvar,…)`: kết quả đổi theo sysvar tại lúc chạy.
  5. Đạt DoD-C, DoD-L.

### CUS-003 · Engine TCMIGRATE bản EA: phát hiện AutoCAD, viết tắt, đường dẫn, mẫu, CTB/STB, LISP khởi động
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** INT-001, INT-006, LSP-013, NT-010, NT-011, NT-013
- **Lệnh:** TCMIGRATE, -TCMIGRATE
- **Màn hình:** UI-K12
- **Đầu vào:** 07.3 UI-K12 (5 bước; EA: viết tắt, đường dẫn, mẫu, CTB, LISP); prototype `js/catalog.js` › `UI-K12` (`RUN('TCMIGRATE')`); PGP của INT-006; Startup Suite của LSP-013; hồ sơ AutoCAD mẫu (khóa registry xuất ra `.reg`, tệp `.arg`, thư mục) do doanh nghiệp pilot gửi **[đề xuất]**; cấu trúc registry và `.arg` lấy theo thực nghiệm (05 §15).
- **Đầu ra:**
  - `custom/migrate/Detect.h/.cpp`: tìm AutoCAD đã cài và hồ sơ (Windows: đọc registry, chỉ đọc), đọc `.arg`, hoặc thư mục/tệp chọn tay. Bộ đọc `.arg` giới hạn kích thước; `custom/fuzz/arg_fuzz.cpp`.
  - `custom/migrate/Plan.h/.cpp`: danh sách mục, xung đột (viết tắt trùng, tệp trùng tên), cách xử lý (giữ của TrionCAD / ghi đè / đổi tên); bỏ qua tệp trong thư mục cài đặt của AutoCAD **[đề xuất]**.
  - Áp dụng có giao dịch: xuất profile hiện tại (NT-010) trước, lỗi giữa chừng thì khôi phục; chạy nền qua NT-013 (tiến trình, hủy); báo cáo JSON (đã chuyển / bỏ qua / cảnh báo).
  - TCMIGRATE mở trình 5 bước qua `MigrationHost` (UIK-009 hiện thực); -TCMIGRATE chạy trên dòng lệnh và `.scr`. `docs/specs/commands/TCMIGRATE.md`.
- **Tiêu chí nghiệm thu:**
  1. 20 hồ sơ mẫu: kế hoạch và báo cáo khớp ca vàng 100%.
  2. Tiêm lỗi ở từng bước: cài đặt sau khôi phục giống hệt trước khi chạy (so hash kho NT-010).
  3. Ba cách xử lý xung đột viết tắt cho tệp PGP đúng ca vàng; chọn "giữ" thì alias của TrionCAD không đổi.
  4. Phát hiện và lập kế hoạch cho hồ sơ 1.000 viết tắt, 50 đường dẫn ≤ 1 s, không chặn luồng UI **[đề xuất]**; fuzz `.arg` ≥ 1 giờ.
  5. Đạt DoD-C.

### CUS-101 · Đọc CUIX: gói ZIP, XML, mô hình CUI trong bộ nhớ
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** NT-015, NT-012, NT-005
- **Đầu vào:** 03 §19 (nhập CUIX một phần: toolbar, menu, macro); 07.3 UI-K12 (CUIX một phần ở 1.0), UI-K07; CUIX mẫu do doanh nghiệp pilot và cộng đồng cung cấp; cấu trúc tệp theo thực nghiệm (05 §15). Đọc XML bằng QXmlStreamReader của Qt Core, không thêm phụ thuộc.
- **Đầu ra:**
  - `custom/cuix/Reader.h/.cpp` → `CuiModel`: nhóm menu; macro (tên, chuỗi lệnh, chuỗi trợ giúp, mã phần tử, ảnh nhỏ và lớn); toolbar (nút, flyout, vạch ngăn, vị trí mặc định); menu kéo xuống và menu con; phím tắt; tệp LISP; tham chiếu CUIX phụ; ảnh nhúng. Ribbon và tool palette chỉ đếm để báo cáo (P2).
  - Giới hạn dung lượng giải nén, số phần tử, độ sâu XML; giải mã ảnh song song (NT-005).
  - `custom/tests/cuix/`, `custom/fuzz/cuix_fuzz.cpp`.
- **Tiêu chí nghiệm thu:**
  1. 50 CUIX mẫu đọc 100% không lỗi; số macro, toolbar, menu, phím tắt khớp bản dump chuẩn tự soạn.
  2. Tệp lỗi: thông báo nêu tệp con trong gói và dòng:cột XML.
  3. CUIX 5 MB ≤ 100 ms trên máy tầm trung **[đề xuất]**.
  4. Fuzz ZIP và XML ≥ 1 giờ không lỗi.
  5. Đạt DoD-C.

### CUS-002 · Nhập CUIX một phần vào `.tcui`; lệnh CUILOAD, CUIUNLOAD
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** CUS-101, CUS-104, LSP-013, LSP-111, INT-001
- **Lệnh:** CUILOAD, CUIUNLOAD
- **Màn hình:** UI-K12
- **Đầu vào:** 06 §5 (nhập CUIX một phần ở 1.0); 07.3 UI-K12 (toolbar, menu, macro, viết tắt và phím tắt có trong tệp; toolbar và menu nhập vào hiện như của workspace Cổ điển, bật được trong mọi workspace); mô hình CUI của CUS-101; ghi chú §0 về `.tcui`; API nạp `.mnl` của LSP-013.
- **Đầu ra:**
  - `custom/tcui/`: định dạng `.tcui` (JSON có phiên bản, lược đồ `custom/tcui/schema.json`): nhóm menu, lệnh (tên, macro, biểu tượng, tooltip, trợ giúp), toolbar, menu, phím tắt, tệp LISP đi kèm; phép phủ thêm/bớt/thay lên định nghĩa có sẵn (UIA-005, UIA-006); di trú phiên bản.
  - `custom/cuix/Import.h/.cpp`: CUI → `.tcui`. Macro giữ nguyên từng ký tự và kiểm bằng parser CUS-104; lệnh TrionCAD chưa có thì đánh dấu; ảnh xuất PNG; phím tắt trùng báo xung đột; ribbon, tool palette ghi "chưa hỗ trợ (P2)" trong báo cáo.
  - CUILOAD, CUIUNLOAD: nạp, gỡ CUIX hoặc `.tcui` làm nhóm menu (hộp chọn tệp khi FILEDIA = 1, dòng lệnh khi FILEDIA = 0); nạp `.mnl` cùng tên qua LSP-013; nối `menugroup` (LSP-111) vào danh sách nhóm đã nạp.
  - `docs/specs/commands/CUILOAD.md`, `docs/specs/commands/CUIUNLOAD.md`, `custom/tests/tcui/`.
- **Tiêu chí nghiệm thu:**
  1. 50 CUIX của CUS-101: mọi mục toolbar và menu trỏ tới lệnh có macro trùng từng ký tự với nguồn; số mục khớp.
  2. 200 macro đã nhập chạy dưới host kịch bản cho chuỗi input như ca vàng.
  3. `.tcui` đọc → ghi → đọc giống hệt; tệp phiên bản cũ di trú đúng.
  4. CUILOAD/CUIUNLOAD 20 vòng không rò bộ nhớ (ASan); `.mnl` nạp một lần; `menugroup` trả đúng tên; chạy giống nhau từ bàn phím, `.scr`, `(command …)`.
  5. Nhập CUIX 5 MB ≤ 300 ms **[đề xuất]**.
  6. Đạt DoD-C.

### CUS-105 · TCMIGRATE bản 1.0: thư mục tin cậy, màu và bắt điểm, PAT/LIN, font SHX, PC3/PMP, CUIX một phần
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CUS-003, CUS-002, LSP-115, INT-005
- **Màn hình:** UI-K12
- **Đầu vào:** 07.3 UI-K12 (bản 1.0: các mục còn lại, gồm CUIX một phần); kế hoạch và giao dịch của CUS-003; nhập CUIX của CUS-002; TRUSTEDPATHS của LSP-115; tên font SHX và mẫu PAT/LIN đi kèm TrionCAD (VN-007, VN-008).
- **Đầu ra:**
  - Mục mới trong `custom/migrate/`: thư mục tin cậy (thêm vào TRUSTEDPATHS; cảnh báo thư mục mạng hoặc ai cũng ghi được **[đề xuất]**); màu vùng vẽ và thiết lập bắt điểm (sysvar qua INT-005); PAT/LIN tùy biến; font SHX trong đường dẫn hỗ trợ; PC3/PMP; CUIX chính và CUIX phụ (toolbar, menu, macro, phím tắt) qua CUS-002.
  - Trùng tên với nội dung đi kèm TrionCAD (font, PAT, LIN) dùng ba cách xử lý của CUS-003, mặc định giữ của TrionCAD.
  - `custom/tests/migrate/v1/`.
- **Tiêu chí nghiệm thu:**
  1. 20 hồ sơ mẫu của CUS-003 bổ sung các mục 1.0: kế hoạch và báo cáo khớp ca vàng 100%.
  2. Tiêm lỗi ở từng mục mới: khôi phục giống hệt như CUS-003.
  3. Không thư mục nào vào TRUSTEDPATHS khi người dùng chưa chọn rõ; mỗi thư mục thêm vào có một dòng trong báo cáo.
  4. Toolbar và menu từ CUIX có trong workspace Cổ điển và bật được ở workspace khác (kiểm trên mô hình `.tcui`).
  5. Đạt DoD-C.

### CUS-004 · Mô hình workspace: lưu và chuyển bố cục ribbon, palette, toolbar; lệnh WORKSPACE
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CUS-002, INT-001, INT-005, NT-010
- **Lệnh:** WORKSPACE
- **Màn hình:** UI-K08
- **Đầu vào:** 07.3 UI-K08 (lưu bố cục thành workspace; thiết lập: workspace nào hiện trong menu, thứ tự, tự lưu khi chuyển; chuyển workspace); 03 §19 (workspace P1); workspace có sẵn Soạn thảo 2D, Cổ điển, Viewer (UIA-001); `.tcui` của CUS-002.
- **Đầu ra:**
  - `custom/workspace/Workspace.h/.cpp`: workspace gồm tab và panel ribbon, QAT, thanh menu, toolbar (hiện, vị trí dock, thứ tự), palette (mở, dock, kích thước, tự ẩn, nhóm tab). Workspace có sẵn chỉ đọc; workspace của người dùng lưu trong `.tcui`.
  - Chuyển workspace: tính diff giữa trạng thái hiện tại và đích, giao cho `WorkspaceHost` (UIK-015, UIA-001 hiện thực); sysvar WSCURRENT, WSAUTOSAVE; API lưu và thiết lập cho WSSAVE, WSSETTINGS (UIK-015).
  - WORKSPACE đủ tùy chọn `setCurrent/SAveas/Edit/Rename/Delete/SEttings/?`; Edit mở CUI khi đã có (UIK-016), trước đó báo chưa hỗ trợ. `docs/specs/commands/WORKSPACE.md`.
- **Tiêu chí nghiệm thu:**
  1. 3 workspace có sẵn và 10 workspace người dùng: lưu → chuyển → chuyển lại cho trạng thái giống hệt (so hash).
  2. Mọi tùy chọn WORKSPACE chạy giống nhau từ bàn phím, `.scr`, `(command …)`; xóa workspace hiện hành, trùng tên báo lỗi theo đặc tả.
  3. Tính diff ≤ 2 ms **[đề xuất]**; xuất rồi nhập profile (NT-010) giữ nguyên mọi workspace.
  4. Đạt DoD-C.

### CUS-005 · Backend trình sửa CUI
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** CUS-002, CUS-004, CUS-104
- **Màn hình:** UI-K07
- **Đầu vào:** 07.3 UI-K07 (cây tùy biến: workspace, QAT, tab và panel ribbon, toolbar, menu, menu chuột phải, phím tắt, nhấp đúp, nút chuột, tệp LISP, tệp tùy biến phụ; danh sách lệnh và tạo lệnh mới; kéo-thả; xem trước; nhập CUIX; chuyển dữ liệu giữa hai tệp); `.tcui` của CUS-002.
- **Đầu ra:**
  - `custom/cui/Editor.h/.cpp`: mô hình sửa trên `.tcui`: thêm, xóa, di chuyển, đổi tên, sao chép nút cây; tạo lệnh (tên, macro, biểu tượng, tooltip); kiểm vị trí thả hợp lệ; chuyển mục giữa hai tệp, giữ tham chiếu; ngăn xếp undo riêng của trình sửa; Áp dụng (ghi nguyên tử) và Hủy.
  - Kiểm hợp lệ: lệnh treo, phím tắt trùng (so với nguồn phím tắt UI0-015), macro lỗi (parser CUS-104); dữ liệu xem trước cho UI.
  - `custom/tests/cui/`.
- **Tiêu chí nghiệm thu:**
  1. 10⁵ chuỗi sửa ngẫu nhiên (property-based): undo hết về đúng tệp ban đầu; sau Áp dụng tệp luôn hợp lệ theo lược đồ.
  2. Chuyển mục giữa hai tệp không để lại tham chiếu treo (kiểm tự động).
  3. Kiểm hợp lệ bắt 100% lỗi tiêm vào (lệnh treo, phím trùng, macro lỗi).
  4. Mỗi thao tác trên cây 10⁴ nút ≤ 1 ms **[đề xuất]**.
  5. Đạt DoD-C.

## 11. SDK C/C++ và plugin

### SDK-101 · C API lõi: phiên bản ABI, handle, lỗi; database, transaction, entity
- **Mốc:** 1.0 · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** DOC-007, DOC-009, DOC-014, DOC-017, DOC-018, INT-005
- **Đầu vào:** 05 §12 (C ABI có phiên bản; khái niệm gần ObjectARX nhưng là API riêng); 03 §19 (SDK C API P2, không tương thích nhị phân ObjectARX); ghi chú §0 (bảng hàm truyền cho plugin lúc nạp); transaction DOC-007, thuộc tính DOC-017, snapshot DOC-009, biến đổi DOC-115 và sao chép sâu qua DOC-014.
- **Đầu ra:**
  - `sdk/include/trion/tc_api.h` (C11): bảng hàm `tc_api_v1` có trường kích thước để mở rộng, bắt tay phiên bản; handle mờ; mã `tc_status` và thông điệp lỗi theo luồng; chuỗi UTF-8 với bộ đệm do bên gọi cấp (hỏi kích thước trước).
  - `sdk/src/`: database (tài liệu mở, tài liệu hiện hành, bảng ký hiệu, block record, duyệt entity theo không gian và loại, sysvar); transaction (begin, commit, abort, lồng nhau); entity (tạo theo tên loại, xóa, đọc/ghi thuộc tính có kiểu theo mã DOC-017, biến đổi, sao chép, xdata); API đọc trên snapshot cho luồng nền.
  - Tường lửa ngoại lệ ở mọi điểm vào: ngoại lệ C++ không bao giờ vượt ABI.
  - `sdk/tests/c/` (kiểm thử viết bằng C).
- **Tiêu chí nghiệm thu:**
  1. Kiểm thử bằng C phủ 100% hàm, kể cả nhánh lỗi.
  2. Tiêm ngoại lệ vào từng hàm lõi bên dưới: hàm C trả mã lỗi, transaction hủy sạch, ứng dụng chạy tiếp.
  3. Đọc thuộc tính qua C API chậm hơn gọi C++ trực tiếp ≤ 50 ns; duyệt 10⁶ entity ≤ 30 ms (máy tầm trung) **[đề xuất]**.
  4. Đọc snapshot từ 8 luồng: TSan sạch, hiệu suất song song ≥ 60%.
  5. Đạt DoD-C, DoD-H.

### SDK-001 · C API ổn định: lệnh, nhập liệu, jig, reactor; kiểm ABI, tài liệu, mẫu
- **Mốc:** 1.0 · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** SDK-101, LSP-014, LSP-117, INT-001, INT-002, INT-003, INT-008, INT-012, INT-019
- **Đầu vào:** 05 §12 (database, transaction, entity, reactor, command, jig); 06 §5 (SDK beta cho 1–2 đối tác chuyên ngành, ví dụ Nova-TDN); registry lệnh INT-001, input INT-002, prompt INT-003, chọn INT-008, jig INT-012; hàm `vl-acad-defun` (LSP-014), hàm ARX của LSP-117.
- **Đầu ra:**
  - `sdk/include/trion/tc_cmd.h`, `tc_jig.h`, `tc_reactor.h`: đăng ký lệnh (tên toàn cục và địa phương, cờ modal, trong suốt, PICKFIRST); prompt và từ khóa; lấy điểm, khoảng cách, góc, chuỗi, từ khóa, entity, tập chọn có bộ lọc; in dòng lệnh; jig (lấy mẫu con trỏ, cập nhật, vẽ primitive lên overlay); reactor đối tượng, database, lệnh, tài liệu, sysvar, phát ở điểm an toàn; đăng ký hàm LISP viết bằng C, gọi hàm LISP đã `vl-acad-defun`.
  - `sdk/abi/`: dump bảng hàm và bố cục struct thành JSON baseline; CI chặn thay đổi không tương thích trong cùng phiên bản chính.
  - `sdk/samples/c/` (lệnh, jig vẽ đường, nhật ký reactor), nạp thẳng trong harness kiểm thử (nạp gói là SDK-003); tài liệu sinh từ header.
- **Tiêu chí nghiệm thu:**
  1. Plugin mẫu bằng C chạy theo kịch bản trên `trion-console` cho cùng kết quả trên 3 hệ điều hành.
  2. 20 đột biến ABI (đổi thứ tự, xóa hàm, đổi struct) đều bị phát hiện; thêm hàm cuối bảng được chấp nhận.
  3. Jig từ plugin ≤ 8 ms/khung (p99); không thuê bao thì reactor không tốn gì; 100 thuê bao làm mỗi lệnh chậm thêm ≤ 0,1 ms **[đề xuất]**.
  4. 100% hàm công khai có chú thích; cảnh báo của công cụ sinh tài liệu là lỗi.
  5. Đạt DoD-C, DoD-L.

### SDK-002 · C++ wrapper header-only trên C API
- **Mốc:** 1.0 · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** SDK-001
- **Đầu vào:** 05 §12 (C++ wrapper header-only); header C của SDK-101, SDK-001.
- **Đầu ra:**
  - `sdk/include/trion/cpp/`: RAII cho handle và transaction; view có kiểu cho entity thường gặp (Line, Circle, Arc, Polyline, Text, MText, BlockReference); range để duyệt; lambda cho lệnh, jig, reactor (thunk sang C); đổi `tc_status` ↔ ngoại lệ ở phía plugin. Chuẩn tối thiểu C++17 để đối tác dùng trình biên dịch cũ hơn **[đề xuất]**.
  - `sdk/samples/cpp/`: các mẫu C viết lại bằng C++.
- **Tiêu chí nghiệm thu:**
  1. Mẫu C++ cho nhật ký và DWG giống mẫu C từng byte.
  2. Biên dịch sạch với MSVC 2022, Clang ≥ 17, GCC ≥ 12 ở C++17 và C++20 **[đề xuất]**.
  3. Gọi qua wrapper chậm hơn gọi C trực tiếp ≤ 2 ns mỗi lần **[đề xuất]**; tiêm lỗi cho thấy không ngoại lệ nào vượt ABI.
  4. Đạt DoD-C.

### SDK-003 · Nạp plugin: gói `.tcplugin`, manifest, chữ ký số, cô lập lỗi
- **Mốc:** 1.0 · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** SDK-001, CUS-002, LSP-013, LSP-115, NT-015, PLF-002, PLF-007
- **Đầu vào:** 05 §12, §16 (ký số, SECURELOAD); 07.3 UI-K10 (đã ký số hay chưa, quyền, hỏi tin cậy khi chưa ký); prototype `js/screens20.js` (UI-K10: gói `.tcplugin`, `manifest.json`, quyền đọc/ghi/mạng, các bước "Giải nén gói → Kiểm tra chữ ký và tương thích SDK → Đăng ký lệnh, tệp tùy biến", chặn plugin chưa ký ở bản Doanh nghiệp); chính sách nạp LSP-115 (ngữ cảnh plugin); Crashpad PLF-007.
- **Đầu ra:**
  - `sdk/plugin/Package.h/.cpp`: đọc gói (NT-015) và `manifest.json` (id, tên, nhà phát triển, phiên bản, dải phiên bản SDK, nhị phân theo hệ điều hành và kiến trúc, quyền khai báo, lệnh, tệp `.tcui`, LISP); chữ ký Ed25519 (libsodium) trên manifest và băm từng tệp; khóa nhà phát triển do khóa gốc TrionCAD ký **[đề xuất]**.
  - `sdk/plugin/Loader.h/.cpp`: kiểm chính sách LSP-115; gói chưa ký thì hỏi tin cậy qua host, bản Doanh nghiệp chặn theo PLF-002; nạp thư viện động, bắt tay phiên bản, gọi `tc_plugin_init`; đăng ký lệnh, `.tcui` (CUS-002), LISP; bộ xử lý `.tcplugin` cho APPLOAD (LSP-013).
  - Cô lập lỗi: ngoại lệ trong callback → hủy transaction, tắt plugin, báo người dùng; crash → PLF-007 ghi module gây lỗi, lần khởi động sau plugin bị tắt kèm liên kết báo cáo. Quyền khai báo để hiển thị, không cưỡng chế trong tiến trình.
  - `sdk/tests/plugin/`, `sdk/fuzz/package_fuzz.cpp`.
- **Tiêu chí nghiệm thu:**
  1. 30 gói kiểm (hợp lệ, sửa 1 byte, sai khóa, sai nền tảng, thiếu `manifest.json`, SDK không tương thích): phán định đúng 100%.
  2. Plugin ném ngoại lệ trong lệnh, reactor, jig: bản vẽ không đổi, ứng dụng chạy tiếp, plugin bị tắt; plugin gây crash bị tắt ở lần khởi động sau.
  3. 10 plugin làm khởi động ấm chậm thêm ≤ 200 ms; kiểm chữ ký gói 10 MB ≤ 10 ms **[đề xuất]**.
  4. Fuzz gói và manifest ≥ 1 giờ không lỗi.
  5. Đạt DoD-C.

### SDK-102 · Backend quản lý plugin: cài, cập nhật, bật/tắt, gỡ
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** SDK-003, NT-010, NT-013
- **Màn hình:** UI-K10
- **Đầu vào:** 07.3 UI-K10 (danh sách: tên, nhà phát triển, phiên bản, đã ký hay chưa, trạng thái; bật/tắt; chi tiết quyền và lệnh; cài từ gói; cập nhật; gỡ; hỏi tin cậy khi chưa ký); prototype `js/screens20.js` (UI-K10: gói hỏng, cập nhật, chặn ở bản Doanh nghiệp).
- **Đầu ra:** `sdk/plugin/Manager.h/.cpp` cho UIK-019:
  - Danh sách plugin đã cài và trạng thái (bật, tắt, lỗi, bị tắt do crash); chi tiết: quyền, lệnh, tệp tùy biến.
  - Cài: giải nén → kiểm chữ ký và tương thích SDK → đăng ký. Cập nhật chỉ nhận cùng id, phiên bản cao hơn, cùng khóa nhà phát triển. Gỡ: hủy đăng ký lệnh, `.tcui`, LISP, xóa tệp. Bật/tắt có hiệu lực ở lần khởi động sau, hoặc ngay nếu plugin khai báo gỡ được **[đề xuất]**.
  - Quyết định tin cậy lưu trong NT-010; chạy nền qua NT-013 (tiến trình, hủy; hủy thì về trạng thái cũ).
  - `sdk/tests/plugin/manager/`.
- **Tiêu chí nghiệm thu:**
  1. Máy trạng thái: mọi chuyển hợp lệ đúng, chuyển không hợp lệ bị từ chối.
  2. 100 vòng cài → cập nhật → gỡ: cây tệp và kho cài đặt về đúng như trước (so hash).
  3. Bản cập nhật ký bằng khóa khác bị từ chối; hủy giữa chừng không để lại tệp hay đăng ký thừa.
  4. Đạt DoD-C.

## Tóm tắt theo mốc

| Mốc | Task | Số task |
|---|---|---|
| S5 | LSP-002, LSP-001, LSP-101, LSP-003, LSP-004, LSP-102, LSP-006, LSP-103 | 8 |
| EA | LSP-104, LSP-105, LSP-106, LSP-005, LSP-107, LSP-108, LSP-109, LSP-110, LSP-007, LSP-111, LSP-112, LSP-113, LSP-008, LSP-114, LSP-010, LSP-009, LSP-011, LSP-115, LSP-116, LSP-012, LSP-117, LSP-013, LSP-118, LSP-018, LSP-119, LSP-019, LSP-127, LSP-121, LSP-128, LSP-123, LSP-124, CUS-104, CUS-001, CUS-003 | 34 |
| 1.0 | LSP-014, LSP-120, LSP-015, LSP-017, LSP-122, LSP-016, LSP-020, LSP-125, CUS-101, CUS-002, CUS-105, CUS-004, SDK-101, SDK-001, SDK-002, SDK-003 | 16 |
| Sau | LSP-134, LSP-126, CUS-005, SDK-102 | 4 |
| **Tổng** | | **62** |
