# CMD 4 · Layer và thuộc tính, block và attribute, xref, đếm, so sánh, trích xuất dữ liệu

> Task lõi (CMD-501–699) cho lệnh layer và thuộc tính đối tượng, block và attribute (Block Editor, sửa tại chỗ, dynamic block, công cụ Mở rộng nhóm Block), tham chiếu ngoài, ảnh, PDF underlay, đếm block, so sánh bản vẽ, trích xuất dữ liệu, chuẩn `.dws`.
> - Căn cứ: [03](../03-chuc-nang-autocad.md) §10–§12, §17, §18, §28, Phụ lục A; [05](../05-kien-truc-ky-thuat.md) §6.2, §8, §10, §14; [07.1](../07-dac-ta-ui-1-khung-ung-dung.md) §2, §7.2, §7.3, §7.10, §9.4, §9.10, §9.11; [07.2](../07-dac-ta-ui-2-bang-va-hop-thoai.md) UI-C01–C08, UI-C11, UI-E10, UI-G01–G19, UI-I06, UI-I07; [07.3](../07-dac-ta-ui-3-tinh-nang-vn-va-he-thong.md) UI-M03, UI-M04, UI-J13.
> - Thư mục mã: `commands/layer/`, `commands/block/`, `commands/xref/`, `commands/data/`, `commands/express/`. Đặc tả lệnh: `docs/specs/commands/`. Mẫu task, DoD, task neo: [README](README.md) §2–§5.

## 0. Ghi chú thiết kế

- **Ranh giới.** Tệp này làm engine và lệnh. Palette, hộp thoại, tab ngữ cảnh thuộc UIC, UIG, UII (README §4.2). Mọi sửa đổi bản vẽ đi qua lệnh (INT-002) và transaction DOC-007. Palette Layer và Thuộc tính ghi bản vẽ qua lệnh ẩn `TCLAYEREDIT`, `TCPROPSET` **[đề xuất]**; mỗi lần sửa một ô là một bước hoàn tác (07.2 Quy ước).
- **Tương thích script.** Tên lệnh, tên toàn cục `_`, tiền tố `-`, từ khóa và thứ tự prompt giữ như AutoCAD (03 Quy ước, 07.2 UI-E10). Lệnh `-` không mở hộp thoại. Chạy từ `.scr`, LISP, hoặc `FILEDIA`/`CMDDIA` = 0 thì lệnh có hộp thoại dùng prompt dòng lệnh như AutoCAD. Lệnh tôn trọng `PICKFIRST`, `EXPERT` (bỏ câu hỏi xác nhận theo mức), `ATTREQ`, `ATTDIA`.
- **Đặc tả và ca vàng.** Mỗi task lệnh nộp `docs/specs/commands/<TÊN>.md` (05 §10): prompt, tùy chọn, sysvar, ca biên. Prompt viết theo Command Reference công khai, chốt bằng QA-007. Ca vàng đặt ở `commands/<nhóm>/tests/golden/<TÊN>/` (kịch bản `.scr` và DXF/JSON kỳ vọng, cùng định dạng QA-007) **[đề xuất]**.
- **Kiểm thử ba đường vào.** Cùng kịch bản chạy qua nguồn bàn phím của INT-002, qua `.scr` (INT-007; mốc V dùng `trion-console` INT-019) và qua `(command ...)` (LSP-006) phải cho DXF trùng từng trường (so bằng QA-003). **Kiểm undo:** mỗi lần chạy lệnh tạo đúng 1 mark undo (DOC-008); `U` đưa hash snapshot DOC-009 về đúng giá trị trước lệnh.
- **Cổng phiên bản** (07.1 §2, INT-020, PLF-002). Viewer chỉ đổi layer **tạm thời** qua lớp ghi đè theo phiên (CMD-501): không transaction, không undo, không lưu. Viewer Pro (đếm, so sánh, `.tcview`) **không bao giờ sửa DWG**: tiêu chí kiểm hash tệp và bộ đếm sửa đổi của document bằng 0.
- **Đọc qua snapshot, tính ở nền.** Palette, đếm, so sánh, trích xuất đọc snapshot DOC-009 ở luồng nền (NT-005), áp kết quả theo lô. Phần đồng bộ ≤ 16 ms khi vùng chọn ≤ 10.000 đối tượng (DoD-L).
- **Đổi hiển thị không regen.** Bật/tắt, đóng băng, cô lập, làm mờ, tô sáng kết quả chỉ đổi cờ hiển thị của RND-013, không dựng lại batch GPU **[đề xuất]**. Đổi màu, linetype, độ dày nét đi qua regen gia tăng RGN-003, chỉ trên entity bị ảnh hưởng.
- **Tên block hiệu lực.** Mọi nơi hiện hoặc lọc theo tên block (Thuộc tính, đếm, thay block, trích xuất, ATTSYNC) dùng tên hiệu lực của dynamic block qua liên kết `*U` của DOC-012. Trước khi có EvalGraph (1.0), lệnh không được phá liên kết `*U`.
- **Đường dẫn tham chiếu** (xref, ảnh, PDF) phân giải qua NT-011: đường dẫn tìm kiếm, đường dẫn tương đối theo tệp chủ, `PROJECTNAME`; theo dõi thay đổi tệp để báo "Cần nạp lại".
- **Dữ liệu.** Đầu vào không tin cậy của module (`.lin`, `.las`, `.tcview`, mẫu ATTEXT, tệp xref, ảnh, PDF) có parser kiểm biên và fuzz target (DoD-C mục 7). Kiểm thử dùng QA-001 (lọc theo đặc trưng: ≥ 1.000 layer, block lồng, dynamic block, xref lồng, layer state, ghi đè viewport), BENCH-STD (QA-012) và QA-007.

## 1. Layer: trạng thái, engine sửa và lệnh P0

### CMD-501 · Trạng thái layer hiệu lực, tìm kiếm, đếm đối tượng và lớp ghi đè theo phiên; LAYER ở chế độ xem
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-004, DOC-009, DOC-011, RND-013, NT-005, NT-012, LSP-004, INT-001, INT-002, PLF-002
- **Lệnh:** LAYER, LAYERCLOSE
- **Màn hình:** UI-C03, UI-M04
- **Đầu vào:** 07.1 §2 (Viewer bật/tắt layer tạm thời, không lưu), §7.2 panel Layer; 07.2 UI-C03 (cột, thanh dưới "Hiện 48/312 layer", phát hành V); 07.3 UI-M04, UI-M03. Prototype `#UI-C03`, `#UI-M04`: `js/palettes.js` › `TC.layers` (`get`, `set`, `restore`, `isolate`, `endIso`), `layerRows`, `P.layers`; `js/screens1.js` › `SC.layerManager`, `SC.layerDrop`. API bảng LAYER (DOC-004), snapshot (DOC-009), danh sách VP Freeze (DOC-011), cờ hiển thị theo entity (RND-013), khớp mẫu `wcmatch` (LSP-004). Dữ liệu: QA-001 (tệp ≥ 1.000 layer, có layer xref), BENCH-STD 1 triệu entity.
- **Đầu ra:**
  - `commands/layer/layer_view.{h,cpp}`: `LayerViewModel` dựng từ snapshot (tên, cờ hiện hành, đang dùng, phụ thuộc xref, bật, đóng băng, khóa, in, màu, linetype, độ dày nét, độ trong suốt, plot style, mô tả, số đối tượng), sắp theo cột, cập nhật theo lô sự kiện.
  - `commands/layer/layer_overlay.{h,cpp}`: `LayerSessionOverlay` (bật/tắt, đóng băng, VP Freeze tạm, cô lập có ảnh chụp trạng thái, "Khôi phục như tệp gốc"); `effectiveLayerState(doc, overlay, viewport)` dùng chung cho renderer, chọn đối tượng và palette.
  - `commands/layer/layer_usage.cpp`: đếm đối tượng theo layer ở nền (Model, mọi layout, định nghĩa block), hủy được. `commands/layer/layer_search.cpp`: lọc theo `wcmatch`, không phân biệt hoa thường và dấu (NT-012).
  - Lệnh `LAYER` (mở palette Layer; ở Viewer là chế độ xem), `LAYERCLOSE`.
  - Kiểm thử `commands/layer/tests/layer_view_test.cpp`, `layer_overlay_test.cpp`; benchmark `commands/layer/bench/layer_toggle_bench.cpp`, `layer_usage_bench.cpp`; đặc tả `docs/specs/commands/LAYER.md` (mở palette, chế độ xem), `LAYERCLOSE.md`.
- **Tiêu chí nghiệm thu:**
  1. Lớp ghi đè không ghi bản vẽ: 10.000 thao tác ngẫu nhiên (seed tái lập) trên 50 tệp QA-001 **[đề xuất]** không tạo transaction DOC-007, không tạo mark undo; snapshot bảng LAYER trước và sau trùng từng trường.
  2. "Khôi phục như tệp gốc" đưa trạng thái hiệu lực của 100% layer về giá trị trong tệp. Kết thúc cô lập trả về trạng thái trước khi cô lập và giữ thay đổi làm sau đó (như `LAYUNISO`).
  3. Tắt hoặc đóng băng một layer chứa 200.000 entity trên cảnh BENCH-STD 1 triệu entity: khung hình kế tiếp đã đúng, ≤ 16,6 ms p99 (tầm trung), ≤ 33 ms (máy thấp); 0 lần regen, 0 lần dựng lại batch GPU **[đề xuất]**.
  4. Đếm đối tượng theo layer cho bản vẽ 5.000 layer, 1 triệu entity: ≤ 150 ms trên 8 nhân **[đề xuất]**, hiệu suất song song ≥ 60% đến 8 luồng, hủy ≤ 50 ms khi bản vẽ đổi; kết quả trùng nhau với 1–32 luồng.
  5. Tìm kiếm cho kết quả trùng `wcmatch` của LSP-004 trên bảng 200 mẫu **[đề xuất]** (`*`, `?`, `#`, `@`, `.`, `[...]`, `~`, dấu phẩy, ký tự thoát); "cua" khớp "CỬA-ĐI"; lọc 5.000 layer ≤ 16 ms mỗi phím (DoD-L).
  6. Mọi thay đổi trong một khung hình gộp thành tối đa 1 lô sự kiện cho palette, dropdown layer và thanh trạng thái **[đề xuất]**.
  7. Ở Viewer, `LAYER` mở palette chế độ xem; trường sửa (màu, linetype, khóa…) trả mã "cần Pro" cho UI0-011 qua PLF-002.
  8. Đạt DoD-C, DoD-H, DoD-L.
- **Ngoài phạm vi:** ghi thay đổi layer vào bản vẽ (CMD-502, CMD-505); giao diện palette (UI-C03, UI-M04).

### CMD-502 · Engine sửa bảng layer qua transaction
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-004, DOC-007, DOC-011, DOC-013, INT-005, NT-007, RGN-003, QA-009, DWG-024
- **Đầu vào:** 03 §10; 07.2 UI-C03 (luật xóa layer, layer mới đóng băng ở mọi viewport, layer mới lấy thuộc tính của layer đang chọn); DXF Reference công khai của Autodesk (bảng LAYER: cờ, màu, linetype, độ dày nét, độ trong suốt, in, plot style); API DOC-004, DOC-007 (transaction, sự kiện gộp lô), DOC-011 (danh sách VP Freeze), DOC-013 (layer phụ thuộc xref); sysvar `CLAYER`, `EXTNAMES`, `VISRETAIN`, `PSTYLEMODE` (INT-005).
- **Đầu ra:**
  - `commands/layer/layer_edit.{h,cpp}`: `LayerEdit` trên một transaction DOC-007: `create(name, template)`, `createFrozenInAllViewports(name)`, `rename`, `setFields(layers, FieldMask, values)`, `setCurrent`, `deleteUnused`, `setNewVpFreeze`. Mỗi lời gọi là 1 transaction và 1 lô sự kiện kèm mặt nạ trường đổi.
  - `commands/layer/layer_rules.{h,cpp}`: kiểm luật, trả mã lý do (`IsLayer0`, `IsDefpoints`, `IsCurrent`, `HasObjects`, `XrefDependent`, `NameInvalid`, `NameExists`, `LinetypeNotLoaded`, `PlotStyleUnavailable`, `ValueOutOfRange`) để lệnh và UI in thông báo.
  - Kiểm thử `commands/layer/tests/layer_edit_test.cpp`, `layer_rules_test.cpp`, `layer_edit_prop_test.cpp` (property-based); benchmark `commands/layer/bench/layer_edit_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Luật tên: ≤ 255 ký tự; cấm `< > / \ " : ; ? * | =` và dấu nháy ngược; `EXTNAMES` = 0 thì chỉ chữ, số, `$`, `-`, `_` và ≤ 31 ký tự; so trùng không phân biệt hoa thường (NT-007). Mỗi luật và mỗi mã lý do có ≥ 1 ca kiểm.
  2. Luật trạng thái: không đóng băng layer hiện hành; layer đóng băng không thành hiện hành; không xóa `0`, `Defpoints`, layer hiện hành, layer có đối tượng hoặc được tham chiếu (kể cả trong định nghĩa block), layer phụ thuộc xref; không đổi tên `0` và layer phụ thuộc xref; thay đổi layer xref chỉ lưu vào bản vẽ chủ khi `VISRETAIN` = 1; plot style chỉ nhận khi `PSTYLEMODE` = 0; màu layer không nhận ByLayer, ByBlock.
  3. Property-based (khung QA-009): 1 triệu chuỗi thao tác ngẫu nhiên **[đề xuất]** giữ bất biến: có layer `0`; `CLAYER` trỏ tới layer tồn tại, không đóng băng; tên duy nhất; tham chiếu linetype và plot style hợp lệ. Ca lỗi được thu nhỏ và tái lập bằng seed.
  4. Sửa 5.000 layer trong một lời gọi là đúng 1 transaction và 1 lô sự kiện. Đổi màu 1.000 layer ≤ 5 ms **[đề xuất]**. RGN-003 chỉ regen entity có thuộc tính ByLayer phụ thuộc layer đổi (kiểm bằng bộ đếm entity được regen).
  5. Ghi DXF (DWG-024) rồi đọc lại: bảng LAYER trùng từng trường; xdata và extension dictionary của layer (kể cả của ứng dụng khác) giữ nguyên.
  6. Đạt DoD-C.
- **Ngoài phạm vi:** prompt và lệnh (CMD-503, CMD-505); ghi đè thuộc tính theo viewport (CMD-523).

### CMD-503 · Lệnh layer nhanh: LAYOFF, LAYFRZ, LAYON, LAYTHW, LAYISO, LAYUNISO
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-501, CMD-502, INT-001, INT-002, INT-003, INT-008, INT-009, INT-019, INT-020, NT-010, LSP-006
- **Lệnh:** LAYOFF, LAYFRZ, LAYON, LAYTHW, LAYISO, LAYUNISO
- **Màn hình:** UI-M04
- **Đầu vào:** 03 §10 (công cụ layer nhanh P0), Phụ lục A; 07.1 §2, §7.2 panel Layer; 07.2 UI-E10 (`LAYISO`); prototype `js/cmd.js` › `IMPL.LAYOFF`, `IMPL.LAYFRZ`, `IMPL.LAYISO`, `IMPL.LAYUNISO`, `IMPL.LAYON`, `IMPL.LAYTHW`; Command Reference công khai của AutoCAD cho 6 lệnh; API CMD-501 (Viewer, Viewer Pro), CMD-502 (Pro), chọn lồng trong block và xref (INT-009).
- **Đầu ra:**
  - `commands/layer/layer_quick_cmds.cpp`: 6 lệnh với hai đường ghi: lớp ghi đè theo phiên (Viewer, Viewer Pro) và `LayerEdit` (Pro), chọn theo PLF-002 qua INT-020; cờ phiên bản khai báo ở INT-001 để Viewer gọi được.
  - `LAYOFF`: chọn đối tượng lặp lại; `Settings` (`Viewports`: `Vpfreeze`/`Off` khi ở layout; `Block selection`: `Block`/`Entity`/`None`); `Undo` trong lệnh; layer hiện hành thì hỏi xác nhận như AutoCAD.
  - `LAYFRZ`: như `LAYOFF`, `Viewports`: `Vpfreeze`/`Freeze`; layer hiện hành báo không đóng băng được.
  - `LAYISO`: chọn đối tượng; `Settings`: `Off` hoặc `Lock and fade` (độ mờ 0–90, `LAYLOCKFADECTL`); ở layout `Vpfreeze`/`Off`; đổi layer hiện hành sang một layer được cô lập khi cần, như AutoCAD. Viewer: `Lock and fade` chỉ làm mờ, không khóa.
  - `LAYUNISO`: khôi phục trạng thái trước `LAYISO` gần nhất, giữ thay đổi làm sau đó; không có cô lập thì báo. `LAYON`, `LAYTHW`: không prompt; `LAYTHW` không rã đông VP Freeze.
  - Thiết lập của lệnh lưu theo người dùng (NT-010) **[đề xuất]**. Đặc tả `docs/specs/commands/LAYOFF.md`, `LAYFRZ.md`, `LAYON.md`, `LAYTHW.md`, `LAYISO.md`, `LAYUNISO.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 40 ca vàng **[đề xuất]** (Settings, Undo trong lệnh, chọn lồng block và xref ở 3 mức, layer hiện hành, layout có viewport kích hoạt) đạt 100%; khớp 100% QA-007 của 6 lệnh khi có đáp án.
  2. Viewer, Viewer Pro: lệnh chỉ đổi lớp ghi đè, snapshot bảng LAYER không đổi. Pro: mỗi lần chạy là đúng 1 transaction DOC-007.
  3. `LAYUNISO` đưa 100% layer về trạng thái trước `LAYISO` và giữ thay đổi làm sau `LAYISO`: 1.000 chuỗi ngẫu nhiên **[đề xuất]** trùng mô hình tham chiếu.
  4. Chọn tại con trỏ ≤ 1 ms (DoD-L, qua INT-009). Hoàn tất lệnh ≤ 20 ms p99 trên BENCH-STD 1 triệu entity, 5.000 layer; khung hình kế tiếp đã đúng.
  5. `PICKFIRST`: chọn trước rồi gọi `LAYISO` áp ngay cho vùng chọn; `LAYOFF`, `LAYFRZ` theo đúng hành vi AutoCAD (ca vàng).
  6. Đạt kiểm thử ba đường vào (§0) qua `trion-console` và `(command ...)`.
  7. Đạt DoD-C, DoD-L.

### CMD-504 · Lưu trạng thái hiển thị của Viewer Pro ra tệp phụ `.tcview`
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-501, INT-016, NT-011, PLF-002, QA-006
- **Màn hình:** UI-M03
- **Đầu vào:** 07.3 UI-M03 ("Lưu trạng thái hiển thị": view và trạng thái bật/tắt layer vào `.tcview`, không sửa DWG), UI-M04; 07.1 §2; API CMD-501 (lớp ghi đè), INT-016 (view hiện hành), NT-011 (ghi an toàn).
- **Đầu ra:** `commands/layer/tcview.{h,cpp}`: đọc/ghi `.tcview` (JSON có số phiên bản; mỗi layer lưu handle và tên; view của Model và từng layout; nhận diện DWG bằng `$FINGERPRINTGUID` và hash) **[đề xuất]**; áp lại khi mở cùng DWG; mô tả định dạng `commands/layer/tcview_format.md`; fuzz target `commands/layer/fuzz/tcview_fuzz.cpp`; kiểm thử `commands/layer/tests/tcview_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Đọc → ghi → đọc trùng 100%. Hash tệp DWG trước và sau khi lưu trạng thái trùng nhau.
  2. Mở lại cùng DWG: trạng thái hiệu lực của layer và view trùng 100% lúc lưu. DWG đã đổi: khớp layer theo handle rồi theo tên; layer không còn thì bỏ qua và báo số lượng.
  3. Tệp hỏng hoặc phiên bản định dạng lạ: báo lỗi, không áp một phần. Fuzz `tcview_fuzz` theo DoD-C.
  4. Ghi nguyên tử (NT-011): ngắt giả lập giữa chừng không để lại tệp hỏng. Đọc tệp 5.000 layer ≤ 20 ms **[đề xuất]**.
  5. Viewer: tính năng khóa (PLF-002). Viewer Pro và Pro chạy đủ.
  6. Đạt DoD-C.

### CMD-505 · `-LAYER` đủ tùy chọn và sửa layer từ palette (`TCLAYEREDIT`)
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** CMD-501, CMD-502, CMD-507, CMD-512, INT-001, INT-002, INT-003, INT-007, DOC-008, DWG-022, LSP-006, QA-004, QA-007
- **Lệnh:** -LAYER, TCLAYEREDIT
- **Màn hình:** UI-C03
- **Đầu vào:** 03 §10, Phụ lục A; 07.2 UI-C03 (cột P0; chọn nhiều layer rồi sửa một ô; menu chuột phải; luật xóa kèm lý do), UI-E10 (lệnh `-`), Quy ước (palette áp dụng ngay, mỗi lần sửa một bước hoàn tác); prototype `#UI-C03`: `js/palettes.js` › `bindLayerTable`, `layerUndo`; `js/screens14.js` (công cụ layer); Command Reference công khai (`-LAYER`); QA-007.
- **Đầu ra:**
  - `commands/layer/layer_cmd.cpp`: `-LAYER` với vòng tùy chọn `?`, `Make`, `Set`, `New`, `Rename`, `ON`, `OFF`, `Color` (kèm `Truecolor`; `COlorbook` là P2), `Ltype`, `LWeight`, `TRansparency`, `MATerial` (giữ tham chiếu vật liệu có sẵn), `Plot`, `PStyle` (chỉ hiện khi `PSTYLEMODE` = 0), `Freeze`, `Thaw`, `LOck`, `Unlock`, `stAte`, `Description`, `rEconcile`. Danh sách tên nhận dấu phẩy, wildcard, ký tự thoát nháy ngược và `<select objects>`. Tắt layer hiện hành thì hỏi xác nhận, bỏ hỏi khi `EXPERT` ≥ 1.
  - `commands/layer/layer_edit_cmd.cpp`: lệnh ẩn `TCLAYEREDIT` **[đề xuất]**: nhận danh sách handle layer, mã trường và giá trị, gọi `LayerEdit` (CMD-502) trong 1 transaction. Đây là đường ghi duy nhất của palette Layer: tạo, tạo đóng băng ở mọi viewport, đổi tên, xóa layer rỗng, đặt hiện hành, sửa cột P0 cho nhiều layer, Đóng băng VP và Đóng băng ở VP mới (qua CMD-507). Không có trong danh sách tự hoàn thành.
  - Đặc tả `docs/specs/commands/-LAYER.md`, `TCLAYEREDIT.md`; bổ sung chế độ sửa vào `LAYER.md`; ca vàng `commands/layer/tests/golden/-LAYER/`.
- **Tiêu chí nghiệm thu:**
  1. Mỗi tùy chọn của `-LAYER` có ≥ 2 ca vàng **[đề xuất]** (wildcard, dấu phẩy, ký tự thoát, chọn đối tượng, `EXPERT` 0 và 1, `PSTYLEMODE` 0 và 1, tên không tồn tại) đạt 100%; khớp 100% QA-007 của `-LAYER` khi có đáp án.
  2. Một lần chạy `-LAYER` gồm nhiều tùy chọn trước Enter cuối là 1 mark undo; đạt kiểm undo (§0).
  3. Đạt kiểm thử ba đường vào (§0).
  4. `TCLAYEREDIT` sửa một cột cho 3.000 layer đang chọn ≤ 50 ms p99 tầm trung **[đề xuất]**, đúng 1 mark undo, palette nhận 1 lô sự kiện.
  5. Tên không tồn tại hoặc vi phạm luật: in thông báo theo mã lý do của CMD-502 rồi tiếp tục như AutoCAD, không dừng script.
  6. Sau 1.000 thao tác ngẫu nhiên **[đề xuất]**, lưu R2018 (DWG-022): 3 bộ đọc độc lập mở không lỗi, không hỏi recover (QA-004).
  7. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** nội dung tùy chọn `stAte` (CMD-525) và `rEconcile` (CMD-531): trước khi có, lệnh báo "có ở bản 1.0", hủy tùy chọn và tiếp tục vòng; ghi đè theo viewport (CMD-523); giao diện palette (UI-C03).

### CMD-506 · LAYMCUR, LAYMCH
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-502, DOC-017, INT-002, INT-003, INT-007, INT-008, DOC-008, LSP-006, QA-007
- **Lệnh:** LAYMCUR, LAYMCH
- **Đầu vào:** 03 §10, Phụ lục A; 07.1 §7.2 panel Layer; prototype `js/screens6.js` › `IMPL.LAYMCUR`, `IMPL.LAYMCH`; Command Reference công khai (LAYMCUR, LAYMCH).
- **Đầu ra:** `commands/layer/layer_match_cmds.cpp`:
  - `LAYMCUR`: chọn một đối tượng; layer của nó thành hiện hành; báo tên layer. `PICKFIRST` với một đối tượng chọn trước thì áp ngay.
  - `LAYMCH`: chọn đối tượng cần đổi → chọn đối tượng trên layer đích hoặc `Name` (hộp chọn layer; từ script hoặc `CMDDIA` = 0 thì hỏi tên trên dòng lệnh); đối tượng trên layer khóa bị bỏ qua và báo số lượng; báo "N đối tượng đã đổi sang layer X".
  - Đặc tả `docs/specs/commands/LAYMCUR.md`, `LAYMCH.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 20 ca vàng **[đề xuất]** (`PICKFIRST`, `Name`, layer khóa, đối tượng trong layout, chọn rỗng) đạt 100%; khớp QA-007 khi có đáp án.
  2. `LAYMCH` đổi 100.000 đối tượng ≤ 100 ms p99 tầm trung **[đề xuất]**; so từng trường bằng DOC-017: chỉ trường layer thay đổi.
  3. Đạt kiểm undo và kiểm thử ba đường vào (§0).
  4. Đạt DoD-C, DoD-L.

### CMD-507 · VPLAYER: đóng băng layer theo viewport
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-502, DOC-011, RGN-024, INT-002, INT-003, INT-007, INT-008, DOC-008, LSP-006, QA-003, QA-007
- **Lệnh:** VPLAYER
- **Màn hình:** UI-C03
- **Đầu vào:** 03 §10 (VP Freeze P0); 07.2 UI-C03 (cột Đóng băng VP, Đóng băng ở VP mới, nút Layer mới đóng băng ở mọi viewport); prototype `js/screens7.js` › `IMPL.VPLAYERFRZ`, `js/palettes.js` › `vpTgl`; Command Reference công khai (VPLAYER); API DOC-011 (VIEWPORT, danh sách layer đóng băng), RGN-024.
- **Đầu ra:** `commands/layer/vp_layer.{h,cpp}`: engine VP Freeze dùng chung cho `VPLAYER`, `TCLAYEREDIT` và `LAYVPI`; cờ đóng băng ở viewport mới của layer; áp mặc định khi viewport mới được tạo qua DOC-011. Lệnh `VPLAYER` với `?`, `Freeze`, `Thaw`, `Reset`, `Newfrz`, `Vpvisdflt`; hỏi phạm vi `All`/`Select`/`Current`/`Except current`; ở tab Model báo lệnh chỉ dùng trong layout. Đặc tả `docs/specs/commands/VPLAYER.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 25 ca vàng **[đề xuất]** (mọi tùy chọn và phạm vi, viewport đa giác, viewport tắt, layer phụ thuộc xref) đạt 100%; khớp QA-007 khi có đáp án.
  2. Đóng băng một layer trong 1 viewport của layout 6 viewport (BENCH-STD nhiều viewport): chỉ viewport đó vẽ lại; khung hình kế tiếp ≤ 16,6 ms p99 tầm trung **[đề xuất]**.
  3. Viewport mới tạo qua DOC-011 tự đóng băng đúng 100% layer có cờ đóng băng ở VP mới.
  4. Đạt kiểm undo và kiểm thử ba đường vào (§0). Đọc → ghi → đọc giữ nguyên danh sách VP Freeze (QA-003).
  5. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** tùy chọn ghi đè `Color`, `Ltype`, `LWeight`, `PStyle`, `TRansparency` (CMD-523).

### CMD-508 · Nạp bộ layer chuẩn TCVN và từ tệp: TCLAYERSET
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-502, CMD-514, VN-008, DOC-013, DOC-014, NT-013, INT-002, INT-003, INT-007, DOC-008, LSP-006
- **Lệnh:** TCLAYERSET, -TCLAYERSET
- **Màn hình:** UI-C03, UI-J13
- **Đầu vào:** 03 §10 (bộ layer theo TCVN và theo bộ môn, P0 khác biệt VN); 07.1 §7.2 (Nạp bộ layer chuẩn); 07.2 UI-C03 (nguồn TCVN, bộ môn, DWG/DWT/`.las`; trùng tên chọn giữ hay ghi đè); 07.3 UI-J13 (Bộ layer: xem trước, Nạp vào bản vẽ); dữ liệu bộ layer của VN-008; API DOC-013 (nạp DWG/DWT chỉ đọc), DOC-014 (chép sâu giữa hai bản vẽ), CMD-514 (nạp linetype).
- **Đầu ra:** `commands/layer/layer_set.{h,cpp}`: đọc bộ layer từ VN-008 hoặc DWG/DWT; tính khác biệt từng trường (mới, trùng giữ, trùng ghi đè) cho bản xem trước; áp trong 1 transaction; nạp linetype còn thiếu. Lệnh `TCLAYERSET` (mở xem trước ở UI) và `-TCLAYERSET` (nguồn `Tcvn`/`File`, bộ môn hoặc đường dẫn, xử lý trùng `Keep`/`Overwrite`) **[đề xuất]**. Đặc tả `docs/specs/commands/TCLAYERSET.md`, `-TCLAYERSET.md`; kiểm thử `commands/layer/tests/layer_set_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Nạp từng bộ layer của VN-008 vào bản vẽ trống: tên, màu, linetype, độ dày nét, độ trong suốt, in/không in, mô tả trùng 100% dữ liệu nguồn.
  2. Layer trùng tên: `Keep` không đổi trường nào; `Overwrite` đổi đúng các trường có trong bộ, đối tượng vẫn ở layer cũ. Bản xem trước liệt kê đúng từng dòng trước khi áp.
  3. Linetype còn thiếu được nạp từ `.lin` hoặc chép từ tệp nguồn, không tạo bản trùng tên.
  4. Nạp 500 layer từ DWG 20 MB ≤ 1 s tầm trung **[đề xuất]**: đọc nguồn ở nền (NT-013), hủy được, luồng UI không bị chặn quá 16 ms.
  5. Đạt kiểm undo và kiểm thử ba đường vào (§0).
  6. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** nguồn `.las` (CMD-525); nội dung bộ layer (VN-008).

## 2. Thuộc tính đối tượng: Properties, màu, linetype, độ dày nét

### CMD-510 · Engine palette Thuộc tính: gom vùng chọn, thuộc tính chung, chế độ chỉ đọc; PROPERTIES
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-009, DOC-017, INT-001, INT-002, INT-008, GEO-021, RGN-013, VN-004, NT-005
- **Lệnh:** PROPERTIES, PROPERTIESCLOSE
- **Màn hình:** UI-C01
- **Đầu vào:** 03 §1 (PROPERTIES P0), Phụ lục A; 07.2 UI-C01 (dropdown loại "Tất cả (125)" / "Line (80)", nhóm theo loại, `*Khác nhau*`, trường tính toán chỉ đọc nhưng sao chép được, ngưỡng 25.000 đối tượng, ô mô tả có mã DXF hoặc sysvar, khi không chọn thì hiện thiết lập hiện hành; V chỉ đọc); 07.3 UI-M03. Prototype `#UI-C01`: `js/palettes.js` › `P.props`, `common`, `geom`, `attrRows`, `groupHTML`. API DOC-017 (metadata thuộc tính sinh từ schema), DOC-009, RGN-013 (định dạng số), GEO-021 (diện tích, chu vi), VN-004 (bảng mã phát hiện của chữ).
- **Đầu ra:**
  - `commands/layer/properties/prop_view.{h,cpp}`: `SelectionSummary` (loại → số lượng, thứ tự ổn định); `PropertyGrid` (nhóm Chung, Hình học, Chữ, Kích thước, Mẫu, Attribute, Khác; mỗi dòng: mã thuộc tính, nhãn, giá trị đã định dạng, cờ chỉ đọc, `*Khác nhau*`, mô tả kèm mã DXF hoặc sysvar).
  - Trường tính toán: độ dài, diện tích, tổng độ dài line và polyline, diện tích cộng dồn của hatch. Bản "không chọn": `CECOLOR`, `CLAYER`, `CELTYPE`, `CELTSCALE`, `CELWEIGHT`, `CETRANSPARENCY`, view, tỉ lệ chú thích.
  - Tổng hợp ở nền theo lô, hủy khi vùng chọn đổi; vùng chọn > 25.000 đối tượng chỉ trả tóm tắt đến khi người dùng yêu cầu hiện đủ.
  - Lệnh `PROPERTIES` (mở palette; Viewer chỉ đọc), `PROPERTIESCLOSE`.
  - Kiểm thử `commands/layer/properties/tests/prop_view_test.cpp`; benchmark `commands/layer/properties/bench/prop_view_bench.cpp`; đặc tả `docs/specs/commands/PROPERTIES.md`, `PROPERTIESCLOSE.md`.
- **Tiêu chí nghiệm thu:**
  1. Vùng chọn ≤ 10.000 đối tượng: phần đồng bộ ≤ 16 ms p99 (DoD-L). 100.000 đối tượng: tổng hợp nền xong ≤ 60 ms trên 8 nhân **[đề xuất]**, hiệu suất song song ≥ 60% đến 8 luồng, hủy ≤ 50 ms.
  2. Giá trị và `*Khác nhau*` trùng 100% với bộ tính tuần tự tham chiếu trên 10.000 vùng chọn ngẫu nhiên **[đề xuất]** từ QA-001; kết quả trùng từng bit với 1–32 luồng.
  3. Mọi loại đối tượng trong bảng UI-C01 có đúng nhóm và trường (bảng kỳ vọng sinh từ metadata DOC-017, kiểm tự động). Số định dạng qua RGN-013, không tự định dạng.
  4. Viewer: không có đường ghi; mọi giá trị sao chép được dạng chuỗi đúng như hiển thị.
  5. Đạt DoD-C, DoD-H, DoD-L.
- **Ngoài phạm vi:** sửa thuộc tính (CMD-515); nhóm Tùy biến của dynamic block (CMD-559); giao diện palette (UI-C01).

### CMD-511 · LWEIGHT, -LWEIGHT, LWDISPLAY
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** INT-002, INT-003, INT-005, INT-019, INT-020, RND-004, NT-010, DWG-024, LSP-006
- **Lệnh:** LWEIGHT, -LWEIGHT, LWDISPLAY
- **Màn hình:** UI-C08, UI-M01
- **Đầu vào:** 03 §10 (LWEIGHT, LWDISPLAY P0), Phụ lục A; 07.2 UI-C08; 07.3 UI-M01 (thanh trạng thái Viewer có hiển thị lineweight); prototype `#UI-C08`: `js/screens5.js` › `SC.lweight`, `IMPL.LWEIGHT`; Command Reference công khai (LWEIGHT, -LWEIGHT, `LWDISPLAY`, `LWUNITS`, `LWDEFAULT`, `CELWEIGHT`).
- **Đầu ra:** `commands/layer/lweight.{h,cpp}`: bảng 24 độ dày hợp lệ 0,00–2,11 mm cùng ByLayer, ByBlock, Mặc định; quy đổi mm/inch theo `LWUNITS`; làm tròn giá trị nhập về độ dày hợp lệ như AutoCAD; backend hộp thoại (danh sách, đơn vị, hiển thị, mặc định, tỉ lệ hiển thị lưu ở profile NT-010). Lệnh `-LWEIGHT` ("Độ dày nét mặc định cho đối tượng mới hoặc [?]"), `LWDISPLAY` (Viewer dùng được; Pro lưu vào bản vẽ). `CELWEIGHT`, `LWDISPLAY` lưu trong bản vẽ; `LWUNITS`, `LWDEFAULT` lưu theo người dùng. Đặc tả `docs/specs/commands/LWEIGHT.md`, `-LWEIGHT.md`, `LWDISPLAY.md`; kiểm thử `commands/layer/tests/lweight_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Bảng độ dày và quy đổi inch trùng 100% bảng của AutoCAD trong ca vàng; nhập giá trị không chuẩn (ví dụ `0.33`) cho đúng giá trị AutoCAD chọn (QA-007 khi có đáp án).
  2. Đổi `LWDISPLAY` trên BENCH-STD 1 triệu entity: khung hình kế tiếp đúng ≤ 16,6 ms p99 tầm trung; 0 regen, 0 dựng lại batch **[đề xuất]**.
  3. `CELWEIGHT`, `LWDISPLAY` ghi DXF (DWG-024) rồi đọc lại đúng giá trị.
  4. Viewer: `LWDISPLAY` chạy; `LWEIGHT`, `-LWEIGHT` báo cần Pro theo 07.1 §2 (INT-020).
  5. Đạt kiểm thử ba đường vào (§0) qua `trion-console` và `(command ...)`.
  6. Đạt DoD-C, DoD-L.

### CMD-512 · Màu: bộ phân tích màu dùng chung, COLOR, -COLOR
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** INT-002, INT-003, INT-005, INT-007, NT-010, DOC-008, LSP-006, QA-007
- **Lệnh:** COLOR, -COLOR
- **Màn hình:** UI-C06
- **Đầu vào:** 03 §10 (ACI và True Color P0; Color Books P2), Phụ lục A; 07.2 UI-C06; prototype `#UI-C06`: `js/screens5.js` › `SC.colorPicker`, `IMPL.COLOR`; bảng RGB của 256 màu ACI (tài liệu công khai); Command Reference công khai (COLOR, -COLOR, `CECOLOR`).
- **Đầu ra:** `commands/layer/color_spec.{h,cpp}`: phân tích và định dạng màu dùng chung cho `-COLOR`, `-LAYER`, `VPLAYER`, `XPLODE`, `-ATTEDIT`, `TCPROPSET`: ByLayer, ByBlock, ACI 1–255 theo số hoặc tên (`red`…`white`), True Color `R,G,B`; Color Book trả mã "chưa hỗ trợ". Bảng ACI ↔ RGB; danh sách màu dùng gần đây (NT-010). Lệnh `COLOR` (hộp thoại; từ script hoặc `CMDDIA` = 0 thì dùng prompt của `-COLOR` như AutoCAD) và `-COLOR` ("Màu mặc định cho đối tượng mới [Truecolor/COlorbook] <BYLAYER>"); đặt `CECOLOR`. Đặc tả `docs/specs/commands/COLOR.md`, `-COLOR.md`; kiểm thử `commands/layer/tests/color_spec_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Bảng ACI ↔ RGB trùng 100% bảng chuẩn 256 màu. Định dạng rồi phân tích lại cho đúng giá trị ban đầu với mọi màu ACI và 10.000 True Color ngẫu nhiên **[đề xuất]**.
  2. ≥ 100 ca phân tích **[đề xuất]** (hoa/thường, khoảng trắng, tên tiếng Anh, giá trị ngoài phạm vi `0`, `256`, `256,0,0`) cho đúng giá trị, hoặc thông báo và hỏi lại như AutoCAD.
  3. Đổi `CECOLOR` là 1 mark undo; đạt kiểm undo và kiểm thử ba đường vào (§0).
  4. Đạt DoD-C.

### CMD-513 · Parser tệp `.lin`: linetype đơn giản và phức có shape, text
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-004, NT-011, NT-012, RGN-004, QA-006
- **Đầu vào:** 03 §10 (nạp `.lin`, linetype phức), §31 (`.lin` tự viết, giữ tên chuẩn); 05 §8 (Linetype); 07.2 UI-C07 (tệp `trion.lin`, `trionISO.lin`, `TCVN.lin`); mô tả định dạng `.lin` trong tài liệu tùy biến công khai của AutoCAD; API RGN-004 (tra shape theo tên trong tệp SHX), DOC-004 (bản ghi LTYPE); tệp `.lin` của VN-008 làm dữ liệu kiểm thử khi có.
- **Đầu ra:** `commands/layer/linetype/lin_parser.{h,cpp}`: `parseLin(bytes)` trả danh sách định nghĩa và chẩn đoán (dòng, cột) cho dòng tiêu đề `*TÊN,mô tả`, dòng mẫu `A,…`, phần tử shape `[tên,tệp.shx,S=…,R=|A=|U=…,X=…,Y=…]`, phần tử text `["chuỗi",kiểu,S=…,R=|A=|U=…,X=…,Y=…]`, chú thích `;;`, mã hóa ANSI theo code page và UTF-8 (NT-012); dựng bản ghi LTYPE (DOC-004); bộ ghi `.lin` (cho `-LINETYPE Create`). Fuzz target `commands/layer/fuzz/lin_fuzz.cpp`; kiểm thử `commands/layer/linetype/tests/lin_parser_test.cpp`; bộ mẫu ≥ 200 định nghĩa **[đề xuất]** ở `commands/layer/linetype/tests/data/`.
- **Tiêu chí nghiệm thu:**
  1. Đọc đúng 100% định nghĩa trong bộ mẫu (đơn giản, shape, text, xoay `R`/`A`/`U`, lệch `X`/`Y`, tỉ lệ `S`). LTYPE dựng ra trùng từng trường với LTYPE trong DXF do AutoCAD nạp cùng tệp (QA-007 khi có đáp án).
  2. Định nghĩa sai (thiếu phần tử, shape không có trong tệp SHX, kiểu chữ chưa có, vượt giới hạn số phần tử và độ dài dòng của AutoCAD) báo đúng dòng và cột, không nạp một phần.
  3. Đọc → ghi → đọc trùng 100%.
  4. Fuzz `lin_fuzz` theo DoD-C. Đọc tệp 5.000 định nghĩa ≤ 20 ms **[đề xuất]**.
  5. Đạt DoD-C.

### CMD-514 · LINETYPE, -LINETYPE, LTSCALE và các biến tỉ lệ linetype
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-513, DOC-004, DOC-014, RGN-003, RGN-011, NT-011, INT-002, INT-003, INT-005, INT-007, DOC-008, LSP-006, QA-007
- **Lệnh:** LINETYPE, -LINETYPE, LTSCALE, CELTSCALE, PSLTSCALE, MSLTSCALE
- **Màn hình:** UI-C07
- **Đầu vào:** 03 §10, Phụ lục A; 05 §8 (Linetype tôn trọng `LTSCALE`, `PSLTSCALE`, `CELTSCALE`); 07.2 UI-C07; prototype `#UI-C07`: `js/screens5.js` › `SC.linetype` và hộp Tải linetype; Command Reference công khai (LINETYPE, -LINETYPE, LTSCALE, `CELTYPE`, `CELTSCALE`, `PSLTSCALE`, `MSLTSCALE`, `MEASUREMENT`).
- **Đầu ra:** `commands/layer/linetype/linetype_cmd.cpp`:
  - Backend hộp thoại: bộ lọc Tất cả / Đang dùng / Phụ thuộc xref (đảo được); Tải (tệp mặc định theo `MEASUREMENT`: `trion.lin` hoặc `trionISO.lin`; `TCVN.lin`; duyệt); Xóa (chỉ linetype không dùng; không xóa `Continuous`, `ByLayer`, `ByBlock`, linetype hiện hành, phụ thuộc xref); Hiện hành (`CELTYPE`); chi tiết (tên, mô tả, `LTSCALE`, `CELTSCALE`, bề rộng nét ISO, `PSLTSCALE`, `MSLTSCALE`).
  - `-LINETYPE`: `?` (liệt kê định nghĩa trong tệp), `Create`, `Load` (wildcard; đã nạp thì hỏi nạp lại, bỏ hỏi khi `EXPERT` ≥ 3), `Set` (`BYLAYER`, `BYBLOCK`, tên đã nạp).
  - `LTSCALE` ("Hệ số tỉ lệ linetype mới <1.0000>") kèm regen theo `REGENAUTO`; `CELTSCALE`, `PSLTSCALE`, `MSLTSCALE` gõ được như biến hệ thống.
  - Đặc tả `docs/specs/commands/LINETYPE.md`, `-LINETYPE.md`, `LTSCALE.md`, `CELTSCALE.md`, `PSLTSCALE.md`, `MSLTSCALE.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 40 ca vàng **[đề xuất]** (nạp, nạp lại có đổi định nghĩa, xóa bị từ chối kèm lý do khi linetype dùng ở entity, layer hoặc trong block, `Set`, `Create`) đạt 100%; khớp QA-007 khi có đáp án.
  2. Đổi `LTSCALE` trên BENCH-STD 1 triệu entity: chỉ regen entity có linetype không liên tục, xong ≤ 0,5 s trên 8 nhân (ngân sách regen 05 §1.1).
  3. Nạp lại linetype đang dùng: mọi entity dùng nó regen theo định nghĩa mới, không entity nào khác regen (bộ đếm RGN-003).
  4. Đổi `LTSCALE`, `CELTYPE`, `CELTSCALE` là 1 mark undo; đạt kiểm undo và kiểm thử ba đường vào (§0).
  5. Đạt DoD-C, DoD-H, DoD-L.

### CMD-515 · Sửa thuộc tính từ palette: lệnh TCPROPSET
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-510, CMD-512, DOC-007, DOC-008, DOC-017, DOC-021, INT-002, INT-003, INT-008, RGN-003, LSP-006, QA-003
- **Lệnh:** TCPROPSET
- **Màn hình:** UI-C01, UI-C02
- **Đầu vào:** 07.2 UI-C01 (EA sửa các loại P0; chọn nhiều loại chỉ sửa thuộc tính chung; gõ giá trị mới thì áp cho tất cả), Quy ước (mỗi lần sửa là một bước hoàn tác); prototype `#UI-C01` ở bản Pro: `js/palettes.js` › `TC.propEdit`, `propMenu`, `setNum`; API DOC-017 (ghi theo mã thuộc tính, kiểm phạm vi), DOC-021 (thêm/bớt tỉ lệ chú thích), CMD-512 (màu).
- **Đầu ra:** `commands/layer/properties/prop_set.{h,cpp}` và lệnh ẩn `TCPROPSET` **[đề xuất]**: nhận phạm vi (vùng chọn hiện hành hoặc nhóm loại đang lọc), mã thuộc tính DOC-017 và giá trị dạng chuỗi theo kiểu. Kiểm hợp lệ (phạm vi, enum, layer phải tồn tại, linetype phải đã nạp); bỏ qua đối tượng trên layer khóa và báo số lượng; trường nghịch (sửa chu vi hoặc diện tích của Circle thì đổi bán kính; tọa độ đỉnh polyline đang chọn); 1 transaction cho toàn bộ đối tượng. Đặc tả `docs/specs/commands/TCPROPSET.md`; kiểm thử `commands/layer/properties/tests/prop_set_test.cpp`; benchmark `commands/layer/properties/bench/prop_set_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Mỗi thuộc tính sửa được của các loại P0 trong bảng UI-C01 có ≥ 1 ca kiểm **[đề xuất]**; giá trị sai bị từ chối kèm lý do, bản vẽ không đổi.
  2. Áp màu cho 100.000 đối tượng ≤ 200 ms p99 tầm trung **[đề xuất]** (transaction và sự kiện; regen gia tăng chạy nền qua RGN-003); đúng 1 mark undo; đạt kiểm undo (§0).
  3. Chỉ trường được sửa thay đổi: so từng trường trước và sau bằng DOC-017 trên 1.000 vùng chọn ngẫu nhiên **[đề xuất]** từ QA-001.
  4. `(command "TCPROPSET" ...)` và thao tác palette cho DXF trùng từng trường (QA-003).
  5. Đạt DoD-C, DoD-H, DoD-L.
- **Ngoài phạm vi:** thay block qua thuộc tính Tên (CMD-568); thuộc tính Tùy biến của dynamic block (CMD-559).

### CMD-516 · Dữ liệu Thuộc tính nhanh: QUICKPROPERTIES
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-510, CMD-515, INT-005, INT-018
- **Lệnh:** QUICKPROPERTIES
- **Màn hình:** UI-C02
- **Đầu vào:** 03 §1 (QUICKPROPERTIES P1); 07.2 UI-C02 (5–8 trường, trường mặc định theo loại); 07.1 §9.10 (nhấp đúp đối tượng hình học cơ bản mở Thuộc tính nhanh); prototype `#UI-C02`: `js/screens14.js` (`OPEN['UI-C02']`); Command Reference công khai (QUICKPROPERTIES, `QPMODE`, `QPLOCATION`).
- **Đầu ra:** `commands/layer/properties/quick_props.{h,cpp}`: danh sách trường theo loại (Màu, Layer, Linetype cộng: Line → Độ dài; Circle → Bán kính; Text → Nội dung, Chiều cao; Block → Tên, Góc xoay; Hatch → Mẫu, Tỉ lệ, Góc; Kích thước → Kiểu, Ghi đè chữ; Polyline → Bề rộng, Đóng, Diện tích); `QPMODE`, `QPLOCATION`; hành động nhấp đúp đăng ký qua INT-018; lệnh `QUICKPROPERTIES`; đặc tả `docs/specs/commands/QUICKPROPERTIES.md`; kiểm thử `commands/layer/properties/tests/quick_props_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Trường của 7 loại trùng bảng UI-C02; giá trị lấy từ CMD-510, sửa qua `TCPROPSET` (1 mark undo mỗi lần sửa).
  2. Đổi vùng chọn ≤ 10.000 đối tượng: dữ liệu sẵn sàng ≤ 16 ms p99 (DoD-L).
  3. `QPMODE` đúng các giá trị như AutoCAD (tắt; mọi đối tượng; chỉ loại có cấu hình; giá trị âm giữ thiết lập khi tắt) trên ca kiểm.
  4. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** người dùng chọn trường theo loại (P2, CUI).

## 3. Layer: công cụ P1, trạng thái, bộ lọc, ghi đè theo viewport

### CMD-520 · LAYLCK, LAYULK
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-502, RND-013, INT-002, INT-003, INT-007, INT-008, DOC-008, LSP-006
- **Lệnh:** LAYLCK, LAYULK
- **Đầu vào:** 03 §10 (P1); 07.1 §7.2 (Khóa/Mở khóa trên panel Layer, EA), §9.4 (đối tượng trên layer khóa hiện mờ); prototype `js/screens6.js` › `lockCmd`, `IMPL.LAYLCK`, `IMPL.LAYULK`; Command Reference công khai (LAYLCK, LAYULK, `LAYLOCKFADECTL`).
- **Đầu ra:** `commands/layer/layer_lock_cmds.cpp`: `LAYLCK` (chọn đối tượng, khóa layer của nó), `LAYULK` (chọn được đối tượng trên layer khóa để mở); `LAYLOCKFADECTL` (0–90) áp độ mờ qua RND-013; Viewer bị cổng (INT-020). Đặc tả `docs/specs/commands/LAYLCK.md`, `LAYULK.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 10 ca vàng **[đề xuất]** (chọn lồng, layer đã khóa, `PICKFIRST`) đạt 100%; khớp QA-007 khi có đáp án.
  2. Đổi `LAYLOCKFADECTL` trên BENCH-STD 1 triệu entity: khung hình kế tiếp ≤ 16,6 ms p99 tầm trung, 0 dựng lại batch **[đề xuất]**.
  3. Đạt kiểm undo và kiểm thử ba đường vào (§0).
  4. Đạt DoD-C, DoD-L.

### CMD-521 · LAYERP, LAYERPMODE: trở lại thiết lập layer trước
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-502, INT-002, INT-003, INT-005, DOC-008
- **Lệnh:** LAYERP, LAYERPMODE
- **Đầu vào:** 03 §10 (P1); 07.1 §7.2 (Trạng thái trước); prototype `js/screens6.js` › `IMPL.LAYERP`; Command Reference công khai (LAYERP, LAYERPMODE).
- **Đầu ra:** `commands/layer/layer_prev.{h,cpp}`: ngăn xếp thay đổi thiết lập layer theo từng lệnh (nghe sự kiện của CMD-502); `LAYERP` khôi phục bật/tắt, đóng băng, khóa, màu, linetype, độ dày nét, độ trong suốt, in, plot style, VP Freeze của lần đổi gần nhất; không hoàn tác đổi tên, xóa, tạo layer (như AutoCAD); `LAYERPMODE` `ON`/`OFF`; ngăn xếp theo bản vẽ trong phiên, không lưu. Đặc tả `docs/specs/commands/LAYERP.md`, `LAYERPMODE.md`; kiểm thử `commands/layer/tests/layer_prev_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Property-based: 10.000 chuỗi ngẫu nhiên **[đề xuất]** gồm thay đổi layer và `LAYERP`: trạng thái sau mỗi `LAYERP` trùng mô hình tham chiếu; không entity nào thay đổi.
  2. `LAYERPMODE` = `OFF` thì không ghi ngăn xếp; hết ngăn xếp thì báo và không đổi gì.
  3. Mỗi lần `LAYERP` là 1 mark undo; đạt kiểm undo (§0).
  4. Đạt DoD-C.

### CMD-522 · LAYCUR, COPYTOLAYER
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-502, DOC-014, DOC-017, INT-002, INT-003, INT-004, INT-007, INT-008, INT-012, DOC-008, LSP-006, QA-007
- **Lệnh:** LAYCUR, COPYTOLAYER
- **Đầu vào:** 03 §10 (P1); 07.1 §7.2 panel Layer mở rộng; prototype `js/screens14.js` › `IMPL.LAYCUR`, `IMPL.COPYTOLAYER`; Command Reference công khai (LAYCUR, COPYTOLAYER).
- **Đầu ra:** `commands/layer/layer_copy_cmds.cpp`:
  - `LAYCUR`: chọn đối tượng → đổi sang layer hiện hành, báo số lượng; bỏ qua layer khóa.
  - `COPYTOLAYER`: chọn đối tượng → chọn đối tượng trên layer đích hoặc `Name` (tên chưa có thì hỏi tạo như AutoCAD) → bản sao trên layer đích → điểm gốc hoặc [`Displacement`/`eXit`] → điểm thứ hai, có jig (INT-012). Bản sao tạo qua DOC-014, giữ liên kết nội bộ theo AutoCAD.
  - Đặc tả `docs/specs/commands/LAYCUR.md`, `COPYTOLAYER.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 25 ca vàng **[đề xuất]** (`PICKFIRST`, `Name`, layer mới, `Displacement`, `eXit`, đối tượng trong layout, hatch kết hợp biên) đạt 100%; khớp QA-007 khi có đáp án.
  2. Bản sao trùng từng trường với nguồn (DOC-017) trừ layer, handle và độ dời.
  3. Jig với 10.000 đối tượng ≤ 8 ms/khung (DoD-L).
  4. Đạt kiểm undo và kiểm thử ba đường vào (§0).
  5. Đạt DoD-C, DoD-L.

### CMD-523 · Ghi đè thuộc tính layer theo viewport
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-505, CMD-507, RGN-002, DOC-005, INT-005, DOC-008, QA-003, QA-005
- **Màn hình:** UI-C03
- **Đầu vào:** 03 §10 (ghi đè theo viewport P1); 07.2 UI-C03 (cột Màu VP, Linetype VP, Độ dày VP, Trong suốt VP, Plot style VP; Gỡ ghi đè viewport; nút lọc "Viewport có ghi đè"), UI-C04 ("áp dụng như ghi đè viewport"); Command Reference công khai (VPLAYER, `VPLAYEROVERRIDES`, `VPLAYEROVERRIDESMODE`); API RGN-002 (phân giải ghi đè khi vẽ), DOC-005 (extension dictionary, xrecord).
- **Đầu ra:** `commands/layer/vp_override.{h,cpp}`: đọc/ghi ghi đè màu, linetype, độ dày nét, độ trong suốt, plot style theo cặp (layer, viewport), lưu đúng cấu trúc AutoCAD trong extension dictionary của layer; thêm tùy chọn `Color`, `Ltype`, `LWeight`, `PStyle`, `TRansparency` và gỡ ghi đè cho `VPLAYER` (CMD-507), nhóm trường VP cho `TCLAYEREDIT` (CMD-505); gỡ theo layer, theo viewport hoặc tất cả; `VPLAYEROVERRIDES` (chỉ đọc), `VPLAYEROVERRIDESMODE`. Cập nhật `docs/specs/commands/VPLAYER.md`, `TCLAYEREDIT.md`; kiểm thử `commands/layer/tests/vp_override_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 50 tệp QA-001 có ghi đè viewport **[đề xuất]**: hiển thị và bản in khớp DWG TrueView, SSIM ≥ 0,98 (QA-005).
  2. Đặt và gỡ ghi đè là 1 mark undo. Đọc → ghi → đọc giữ nguyên 100% ghi đè (QA-003).
  3. `VPLAYEROVERRIDESMODE` = 0 thì vẽ và in bỏ qua ghi đè; nút lọc "Viewport có ghi đè" liệt kê đúng layer có ghi đè trong viewport hiện hành.
  4. Đạt DoD-C.

### CMD-524 · Bộ lọc layer: bộ lọc thuộc tính, bộ lọc nhóm, đảo bộ lọc
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-501, CMD-505, DOC-005, DOC-006, LSP-004, QA-003
- **Màn hình:** UI-C05, UI-C03, UI-C11
- **Đầu vào:** 03 §10 (bộ lọc theo thuộc tính, theo nhóm, tìm kiếm: P1); 07.2 UI-C05, UI-C03 (cây bộ lọc: Tất cả, Layer đang dùng, Viewport có ghi đè, theo Xref, của người dùng; áp bộ lọc lên dropdown layer của ribbon), UI-C11; prototype `#UI-C05`: `js/screens14.js` › `SC.layerFilterEdit`, `filters`, `TC.layerFilterTest`; API DOC-006 (giữ dữ liệu chưa hiểu).
- **Đầu ra:** `commands/layer/layer_filter.{h,cpp}`: bộ lọc thuộc tính (điều kiện trên tên có wildcard, bật, đóng băng, khóa, in, màu, linetype, độ dày, độ trong suốt, plot style, Đóng băng VP, Đóng băng ở VP mới, mô tả; điều kiện trong một dòng là AND, các dòng là OR như AutoCAD), bộ lọc nhóm (danh sách handle), bộ lọc lồng, đảo bộ lọc; nút có sẵn; đọc/ghi bộ lọc lưu trong bản vẽ tương thích AutoCAD; `LAYERFILTERALERT`; nhóm thao tác `Filter` của `TCLAYEREDIT` (tạo, sửa, đổi tên, xóa bộ lọc) **[đề xuất]**. Cập nhật `docs/specs/commands/TCLAYEREDIT.md`; kiểm thử `commands/layer/tests/layer_filter_test.cpp`; benchmark `commands/layer/bench/layer_filter_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Bộ lọc do AutoCAD tạo trong ≥ 100 tệp QA-001 **[đề xuất]** cho cùng danh sách layer như AutoCAD (QA-007 khi có đáp án; trước đó ca vàng).
  2. Đánh giá 50 bộ lọc trên 5.000 layer ≤ 10 ms **[đề xuất]**.
  3. Đọc → ghi → đọc giữ 100% bộ lọc (QA-003); loại bộ lọc chưa hiểu giữ nguyên khối dữ liệu (DOC-006).
  4. Mỗi thao tác bộ lọc là 1 mark undo.
  5. Đạt DoD-C, DoD-L.

### CMD-525 · LAYERSTATE và tệp `.las`
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** CMD-502, CMD-505, CMD-508, CMD-523, DOC-005, NT-011, NT-012, INT-002, INT-003, INT-007, DOC-008, LSP-006, QA-001, QA-003, QA-006
- **Lệnh:** LAYERSTATE
- **Màn hình:** UI-C04
- **Đầu vào:** 03 §10 (`LAYERSTATE` `.las` P1), §31; 07.2 UI-C04 (Mới, Cập nhật, Sửa, Đổi tên, Xóa, Nhập `.las`/`.dwg`, Xuất `.las`, Khôi phục; 3 tùy chọn; 11 thuộc tính khôi phục); prototype `#UI-C04`: `js/screens14.js` › `SC.layerStates`, `SC.layerStateNew`; Command Reference công khai (LAYERSTATE, `-LAYER` tùy chọn `stAte`).
- **Đầu ra:**
  - `commands/layer/layer_state.{h,cpp}`: đọc/ghi layer state lưu trong bản vẽ (dictionary của bảng LAYER, cấu trúc tương thích AutoCAD); lưu, khôi phục, sửa, đổi tên, xóa; chọn thuộc tính khôi phục; tùy chọn không liệt kê state của xref, tắt layer không có trong state, áp như ghi đè viewport (CMD-523).
  - `commands/layer/las_io.{h,cpp}`: đọc/ghi `.las` (văn bản theo mã nhóm kiểu DXF); nhập từ `.las` hoặc `.dwg`, xuất `.las`; fuzz target `commands/layer/fuzz/las_fuzz.cpp`.
  - Tùy chọn `stAte` của `-LAYER` (`?`, `Save`, `Restore`, `Edit`, `Name`, `Delete`, `Import`, `EXport`); nguồn `.las` cho `TCLAYERSET` (CMD-508); lệnh `LAYERSTATE` (hộp thoại; prompt khi `CMDDIA` = 0).
  - Đặc tả `docs/specs/commands/LAYERSTATE.md`; cập nhật `-LAYER.md`, `TCLAYERSET.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. Layer state do AutoCAD tạo trong ≥ 100 tệp QA-001 **[đề xuất]** được đọc và khôi phục đúng 100% các thuộc tính đã chọn.
  2. `.las` do AutoCAD xuất nhập vào đúng; `.las` ta xuất được AutoCAD nhập (QA-007 khi có đáp án); đọc → ghi → đọc `.las` trùng 100%. Fuzz `las_fuzz` theo DoD-C.
  3. Khôi phục state cho 5.000 layer ≤ 50 ms **[đề xuất]** và là 1 mark undo.
  4. Đọc → ghi → đọc DWG giữ nguyên layer state (QA-003).
  5. Đạt kiểm thử ba đường vào (§0) cho `-LAYER` `stAte`.
  6. Đạt DoD-C, DoD-L.

### CMD-526 · LAYMRG, -LAYMRG, LAYDEL, -LAYDEL
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-502, CMD-524, CMD-525, DOC-014, DOC-015, NT-005, INT-002, INT-003, INT-007, INT-008, DOC-008, LSP-006, QA-007
- **Lệnh:** LAYMRG, -LAYMRG, LAYDEL, -LAYDEL
- **Màn hình:** UI-C03
- **Đầu vào:** 03 §10 (P1); 07.2 UI-C03 (gợi ý "Gộp vào layer khác…", menu "Gộp các layer đã chọn vào…"); prototype `js/screens14.js` › `IMPL.LAYMRG`, `IMPL.LAYDEL`; Command Reference công khai (LAYMRG, -LAYMRG, LAYDEL, -LAYDEL); API DOC-014 (phân tích tham chiếu).
- **Đầu ra:** `commands/layer/layer_merge.{h,cpp}`:
  - `LAYMRG`: chọn đối tượng trên layer cần gộp hoặc `Name`, lặp → chọn layer đích → xác nhận nêu số layer và số đối tượng. Chuyển mọi đối tượng (Model, layout, định nghĩa block, block lồng) sang layer đích; cập nhật mọi tham chiếu tới layer nguồn (VP Freeze, ghi đè viewport, layer state, bộ lọc nhóm); xóa layer nguồn.
  - `LAYDEL`: chọn đối tượng trên layer hoặc `Name` → xác nhận → xóa mọi đối tượng trên layer, kể cả trong định nghĩa block, rồi xóa layer.
  - `-LAYMRG`, `-LAYDEL` hỏi tên trên dòng lệnh. Không gộp hoặc xóa `0`, `Defpoints`, layer phụ thuộc xref; `LAYDEL` không xóa layer hiện hành.
  - Đặc tả `docs/specs/commands/LAYMRG.md`, `-LAYMRG.md`, `LAYDEL.md`, `-LAYDEL.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 30 ca vàng **[đề xuất]** đạt 100%; khớp QA-007 khi có đáp án.
  2. Bất biến: sau lệnh không còn tham chiếu nào tới layer bị gộp hoặc xóa (phân tích DOC-014 trả 0), AUDIT (DOC-015) sạch; số đối tượng trên layer đích bằng tổng trước khi gộp.
  3. Gộp 50 layer trong bản vẽ 1 triệu entity ≤ 1 s trên 8 nhân **[đề xuất]**; bước quét chạy song song, hiệu suất ≥ 60% đến 8 luồng; ghi trong 1 transaction.
  4. Đạt kiểm undo và kiểm thử ba đường vào (§0).
  5. Đạt DoD-C, DoD-H, DoD-L.

### CMD-527 · LAYWALK
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-501, CMD-502, CMD-524, INT-002, DOC-008
- **Lệnh:** LAYWALK
- **Màn hình:** UI-C11
- **Đầu vào:** 03 §10 (P1); 07.2 UI-C11; prototype `#UI-C11`: `js/screens14.js` › `SC.layWalk`, `IMPL.LAYWALK`; Command Reference công khai (LAYWALK).
- **Đầu ra:** `commands/layer/laywalk.{h,cpp}`: chỉ hiện đối tượng của các layer đang chọn qua lớp ghi đè (CMD-501), không ghi bản vẽ; giữ "Luôn hiện"; lọc wildcard và bộ lọc có sẵn (CMD-524); "Dọn" xóa layer rỗng đang chọn qua CMD-502 trong 1 mark undo; "Khôi phục trạng thái khi đóng" (mặc định bật; tắt thì trạng thái đang hiện được áp thành thay đổi bật/tắt trong 1 mark undo). Đặc tả `docs/specs/commands/LAYWALK.md`; kiểm thử `commands/layer/tests/laywalk_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Đổi chọn layer trên BENCH-STD 1 triệu entity: khung hình kế tiếp ≤ 16,6 ms p99 tầm trung, 0 dựng lại batch **[đề xuất]**.
  2. Đóng khi bật khôi phục: snapshot bảng LAYER trùng 100% lúc mở.
  3. "Dọn" chỉ xóa layer không có đối tượng (kể cả trong block) và không được tham chiếu.
  4. Đạt DoD-C, DoD-L.

### CMD-528 · LAYVPI: cô lập layer trong viewport
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-507, DOC-011, INT-002, INT-003, INT-007, INT-008, DOC-008, LSP-006, QA-007
- **Lệnh:** LAYVPI
- **Đầu vào:** 03 §10 (P1); 07.1 §7.2 (Cô lập layer trong viewport); prototype `js/screens11.js` › `IMPL.LAYVPI`; Command Reference công khai (LAYVPI).
- **Đầu ra:** `commands/layer/layvpi.cpp`: chọn đối tượng trên các layer cần cô lập hoặc [`Settings`/`Undo`]; `Settings`: `Viewports` (mọi viewport hoặc chọn viewport), `Layouts` (mọi layout hoặc layout hiện hành); đóng băng các layer đã chọn ở mọi viewport khác, giữ viewport hiện hành; chỉ chạy trong layout có viewport kích hoạt. Đặc tả `docs/specs/commands/LAYVPI.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 15 ca vàng **[đề xuất]**: danh sách VP Freeze sau lệnh trùng đáp án 100%; khớp QA-007 khi có đáp án.
  2. Đạt kiểm undo và kiểm thử ba đường vào (§0).
  3. Đạt DoD-C.

## 4. Layer: dịch layer, đối chiếu layer mới, chuẩn `.dws` (P2, sau v1.0)

### CMD-530 · LAYTRANS: dịch layer theo chuẩn
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-502, CMD-526, DOC-013, DOC-014, NT-011, INT-002, DOC-008
- **Lệnh:** LAYTRANS
- **Màn hình:** UI-I09
- **Đầu vào:** 03 §10 (LAYTRANS P2); 07.2 UI-I09 (dịch layer: ánh xạ layer nguồn → layer chuẩn, lưu và nạp ánh xạ); prototype `#UI-I09`: `js/screens18.js` › `SC.layTrans`, `IMPL.LAYTRANS`; Command Reference công khai (LAYTRANS).
- **Đầu ra:** `commands/layer/standards/laytrans.{h,cpp}`: nạp layer chuẩn từ `.dwg`, `.dws`, `.dwt` (DOC-013); ánh xạ theo tên trùng, theo chọn, tạo layer mới ở phía đích; thiết lập: ép màu và linetype của đối tượng về BYLAYER, dịch cả đối tượng trong block, ghi nhật ký; dịch dùng engine gộp của CMD-526; lưu ánh xạ vào tệp chuẩn như AutoCAD. Đặc tả `docs/specs/commands/LAYTRANS.md`; kiểm thử `commands/layer/standards/tests/laytrans_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Mọi đối tượng trên layer nguồn (kể cả trong block khi bật) sang đúng layer đích; layer nguồn không còn tham chiếu thì bị xóa.
  2. Dịch bản vẽ 1 triệu entity ≤ 1 s trên 8 nhân **[đề xuất]**; 1 mark undo.
  3. Ánh xạ lưu rồi nạp lại trùng 100%; nhật ký liệt kê đúng từng ánh xạ và số đối tượng.
  4. Đạt DoD-C, DoD-H.

### CMD-531 · Đối chiếu layer mới: LAYEREVAL, LAYERNOTIFY, LAYEREVALCTL
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** S · **Phụ thuộc:** CMD-502, CMD-505, CMD-524, DOC-005, INT-005
- **Lệnh:** LAYEREVAL, LAYERNOTIFY, LAYEREVALCTL
- **Màn hình:** UI-C03
- **Đầu vào:** 03 §10 (LAYEREVAL P2); 07.2 UI-C03 (báo khi bản vẽ có layer mới, nút lọc "Chưa đối chiếu"); Command Reference công khai (`LAYEREVAL`, `LAYERNOTIFY`, `LAYEREVALCTL`, `-LAYER` tùy chọn `rEconcile`).
- **Đầu ra:** `commands/layer/layer_eval.{h,cpp}`: cờ "chưa đối chiếu" của layer lưu tương thích AutoCAD; phát hiện layer mới theo `LAYEREVAL` (tắt, chỉ layer xref, mọi layer); thông báo theo các bit của `LAYERNOTIFY` (in, mở, nạp/gắn xref, khôi phục layer state, lưu, chèn); tùy chọn `rEconcile` của `-LAYER`; nút lọc "Chưa đối chiếu" (CMD-524). Đặc tả `docs/specs/commands/LAYEREVAL.md`, `LAYERNOTIFY.md`, `LAYEREVALCTL.md`; kiểm thử.
- **Tiêu chí nghiệm thu:**
  1. Cờ đối chiếu do AutoCAD ghi được đọc đúng trên ≥ 30 tệp **[đề xuất]**.
  2. Mỗi bit của `LAYERNOTIFY` phát đúng 1 thông báo ở đúng sự kiện (ca kiểm cho cả 6 sự kiện).
  3. `rEconcile` là 1 mark undo.
  4. Đạt DoD-C.

### CMD-532 · STANDARDS: gắn tệp chuẩn `.dws`
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** S · **Phụ thuộc:** DOC-005, DOC-013, NT-011, INT-002, INT-005, DOC-008
- **Lệnh:** STANDARDS
- **Màn hình:** UI-I09
- **Đầu vào:** 03 §18 (STANDARDS, CHECKSTANDARDS P2), §31 (`.dws`); 07.2 UI-I09 (cùng khung với TCCHECK: gắn tệp chuẩn, chạy kiểm tra, sửa từng lỗi); prototype `js/screens18.js` › `stdConfig`, `IMPL.STANDARDS`; Command Reference công khai (STANDARDS, `STANDARDSVIOLATION`).
- **Đầu ra:** `commands/layer/standards/standards.{h,cpp}`: danh sách tệp `.dws` gắn với bản vẽ theo thứ tự ưu tiên, lưu tương thích AutoCAD; thiết lập: báo vi phạm (`STANDARDSVIOLATION`), tự sửa thuộc tính không chuẩn, hiện lỗi đã bỏ qua, tệp chuẩn ưu tiên khi thay thế; nạp `.dws` chỉ đọc (DOC-013). Đặc tả `docs/specs/commands/STANDARDS.md`; kiểm thử.
- **Tiêu chí nghiệm thu:**
  1. Danh sách tệp chuẩn do AutoCAD gắn được đọc đúng; lưu rồi mở lại giữ nguyên (QA-007 khi có đáp án).
  2. Gắn, gỡ, đổi thứ tự là 1 mark undo.
  3. Nạp phần bảng ký hiệu của `.dws` 20 MB ≤ 300 ms **[đề xuất]**.
  4. Đạt DoD-C.

### CMD-533 · CHECKSTANDARDS: kiểm tra và sửa theo `.dws`
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-530, CMD-532, DOC-014, NT-005, INT-002, DOC-008
- **Lệnh:** CHECKSTANDARDS
- **Màn hình:** UI-I09
- **Đầu vào:** 07.2 UI-I09; prototype `js/screens18.js` › `IMPL.CHECKSTANDARDS`, `ckOpen('dws')`; Command Reference công khai (CHECKSTANDARDS).
- **Đầu ra:** `commands/layer/standards/check_standards.{h,cpp}`: so layer, linetype, kiểu chữ, kiểu kích thước của bản vẽ với các `.dws` (tên và thuộc tính); danh sách vấn đề kèm đề xuất sửa (trùng tên hoặc tệp ưu tiên); Sửa (chép định nghĩa chuẩn qua DOC-014, dịch layer qua CMD-530), Bỏ qua (đánh dấu lưu trong bản vẽ như AutoCAD), Kế tiếp; báo cáo; báo khi thêm đối tượng không chuẩn. Đặc tả `docs/specs/commands/CHECKSTANDARDS.md`; kiểm thử.
- **Tiêu chí nghiệm thu:**
  1. ≥ 50 ca vàng **[đề xuất]**: phát hiện đúng 100% vấn đề, không báo thừa.
  2. Sửa một lỗi là 1 mark undo; Sửa tất cả là 1 mark undo.
  3. Kiểm 5.000 layer và 500 kiểu ≤ 200 ms trên 8 nhân **[đề xuất]**.
  4. Đạt DoD-C, DoD-H.

## 5. Block và attribute: P0

### CMD-540 · Engine định nghĩa block: tạo, định nghĩa lại, kiểm tra
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-007, DOC-012, DOC-014, DOC-015, RGN-018, INT-005, NT-007, QA-003, QA-009
- **Đầu vào:** 03 §11; 05 §8 (dynamic block hiển thị qua `*U`); 07.2 UI-G02 (kiểm tên; cảnh báo "Định nghĩa lại block X sẽ cập nhật N tham chiếu"); DXF Reference công khai (BLOCK_RECORD, BLOCK, INSERT, ATTDEF, ATTRIB); API DOC-012 (định nghĩa, tham chiếu, block vô danh, liên kết `*U`), DOC-014 (chép sâu có ánh xạ ID), RGN-018 (instancing).
- **Đầu ra:** `commands/block/block_define.{h,cpp}`:
  - `defineBlock`, `redefineBlock`, đếm tham chiếu (kể cả lồng và trong layout).
  - Kiểm tên (luật như layer, `EXTNAMES`); chặn tự tham chiếu trực tiếp hoặc lồng; chặn định nghĩa lại block xref, block phụ thuộc xref, block vô danh (`*U`, `*D`, `*T`…) và block của layout.
  - Cờ: chú thích theo tỉ lệ (kèm khớp hướng layout), co giãn đều, cho phép phá khối; đơn vị block (`INSUNITS`); mô tả.
  - Định nghĩa lại không đổi ATTRIB có sẵn (chỉ ATTSYNC đổi), giữ xdata. Dynamic block khi chưa có EvalGraph: tham chiếu giữ biểu diễn `*U` cũ và có cảnh báo (đến CMD-558).
  - Kiểm thử `commands/block/tests/block_define_test.cpp`, `block_define_prop_test.cpp`; benchmark `commands/block/bench/block_redefine_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Property-based (QA-009): 100.000 cây block ngẫu nhiên sâu đến 8 mức **[đề xuất]** không bao giờ tạo chu trình; số tham chiếu cập nhật đúng; AUDIT (DOC-015) sạch sau mỗi thao tác.
  2. Định nghĩa lại block có 100.000 tham chiếu ≤ 100 ms trên 8 nhân **[đề xuất]**: hình học định nghĩa regen một lần, tham chiếu chỉ cập nhật dữ liệu instancing (RGN-018).
  3. Mỗi luật có ≥ 1 ca kiểm và mã lý do riêng cho UI.
  4. Đọc → ghi → đọc giữ nguyên định nghĩa, cờ, xdata và liên kết `*U` của ≥ 300 dynamic block trong QA-001 **[đề xuất]** (QA-003).
  5. Đạt DoD-C, DoD-H.

### CMD-541 · BLOCK, -BLOCK
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-540, CMD:OOPS, INT-002, INT-003, INT-004, INT-007, INT-008, DOC-008, LSP-006, QA-007
- **Lệnh:** BLOCK, -BLOCK
- **Màn hình:** UI-G02
- **Đầu vào:** 03 §11, Phụ lục A; 07.2 UI-G02; prototype `#UI-G02`: `js/screens7.js` › `blockDlg`, `IMPL.BLOCK`; Command Reference công khai (BLOCK, -BLOCK); API CMD-540.
- **Đầu ra:** `commands/block/block_cmd.cpp`:
  - Backend hộp thoại: tên (chọn tên có sẵn để định nghĩa lại, kèm số tham chiếu bị ảnh hưởng), điểm gốc (chỉ trên bản vẽ hoặc X/Y/Z), đối tượng (chọn, chọn nhanh; sau khi tạo: Giữ nguyên / Chuyển thành block / Xóa; số đối tượng đã chọn), hành vi (chú thích theo tỉ lệ, khớp hướng layout, co giãn đều, cho phép phá khối), đơn vị block, mô tả.
  - `-BLOCK`: tên hoặc `?` (liệt kê block) → trùng tên thì hỏi định nghĩa lại (bỏ hỏi khi `EXPERT` ≥ 2) → điểm gốc hoặc `Annotative` → chọn đối tượng; đối tượng nguồn bị xóa và `OOPS` khôi phục được như AutoCAD.
  - `PICKFIRST`: đối tượng chọn trước được dùng. Đặc tả `docs/specs/commands/BLOCK.md`, `-BLOCK.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 30 ca vàng **[đề xuất]** đạt 100%; khớp QA-007 khi có đáp án.
  2. Chuyển thành block: đối tượng được thay bằng một INSERT tại điểm gốc với ma trận đơn vị; hình học trùng từng bit.
  3. `OOPS` sau `-BLOCK` khôi phục đúng đối tượng nguồn.
  4. Tạo block từ 100.000 đối tượng ≤ 300 ms tầm trung **[đề xuất]**.
  5. Đạt kiểm undo và kiểm thử ba đường vào (§0).
  6. Đạt DoD-C, DoD-L.

### CMD-542 · INSERT, -INSERT
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** CMD-540, CMD:EXPLODE, DOC-012, DOC-021, RND-009, INT-002, INT-003, INT-004, INT-007, INT-012, INT-013, DOC-008, LSP-006, QA-007
- **Lệnh:** INSERT, -INSERT
- **Màn hình:** UI-G01
- **Đầu vào:** 03 §11, Phụ lục A; 07.2 UI-G01 (tùy chọn chèn), UI-E10 (`INSERT`: Basepoint, Scale, X/Y/Z, Rotate, Explode, Repeat); 07.1 §9.5 (grip của block), §9.11 (kéo-thả block); prototype `js/screens7.js` › `IMPL.INSERT`, `askAttrs`; Command Reference công khai (INSERT, -INSERT, `ATTREQ`, `ATTDIA`, `INSNAME`, `INSUNITS`, `INSUNITSDEFSOURCE`, `INSUNITSDEFTARGET`).
- **Đầu ra:** `commands/block/insert_cmd.cpp`:
  - `INSERT` mở palette Blocks (UI-G01) rồi chèn theo tùy chọn của palette; `-INSERT`: tên hoặc `?` (mặc định `INSNAME`) → báo đơn vị và hệ số quy đổi → điểm chèn hoặc [`Basepoint`/`Scale`/`X`/`Y`/`Z`/`Rotate`/`Explode`/`REpeat`] → tỉ lệ X (`Corner`, `XYZ`), tỉ lệ Y → góc xoay → attribute.
  - Quy đổi đơn vị theo `INSUNITS` của block và bản vẽ; block chú thích theo tỉ lệ nhân nghịch đảo tỉ lệ chú thích (DOC-021); block co giãn đều thì chỉ hỏi một tỉ lệ; tỉ lệ âm (đối xứng).
  - Attribute: hỏi theo thứ tự định nghĩa khi `ATTREQ` = 1; `ATTDIA` = 1 dùng hộp nhập; `Constant` không tạo ATTRIB; `Preset` không hỏi; `Verify` hỏi hai lần; attribute nhiều dòng.
  - `Explode` (hoặc tiền tố `*` ở tên) chèn bản đã phá qua API của `EXPLODE`. `REpeat` chèn liên tục đến khi Esc. Dynamic block chèn ở trạng thái mặc định.
  - Jig bóng block theo con trỏ dùng instancing (RND-009, INT-012). Đăng ký grip điểm chèn của INSERT và grip vị trí của attribute không khóa vị trí qua INT-013.
  - Đặc tả `docs/specs/commands/INSERT.md`, `-INSERT.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 60 ca vàng **[đề xuất]** (đơn vị, tỉ lệ âm, chú thích theo tỉ lệ, `Constant`, `Preset`, `Verify`, nhiều dòng, `ATTREQ` 0/1, `ATTDIA` 0/1, `Explode`, `REpeat`, dynamic block) đạt 100%; khớp 100% QA-007 khi có đáp án.
  2. Ma trận chèn khớp DXF tham chiếu với sai số ≤ 1e-9 × kích thước bản vẽ (05 §1.2).
  3. Jig cho block 50.000 entity ≤ 8 ms/khung (DoD-L) **[đề xuất: cỡ block]**.
  4. Mỗi lần chạy lệnh là 1 mark undo, kể cả `REpeat` **[đề xuất]**; đạt kiểm undo và kiểm thử ba đường vào (§0, script dùng `ATTDIA` = 0).
  5. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** chèn từ tệp DWG (CMD-543); chọn trạng thái hiển thị khi chèn (P2, CMD-571).

### CMD-543 · Chèn bản vẽ DWG làm block và định nghĩa lại từ tệp
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-542, DOC-013, DOC-014, NT-011, NT-013
- **Đầu vào:** 03 §11; 07.1 §9.11 (kéo tệp DWG vào vùng vẽ thì chèn thành block); 07.2 UI-G01 (chuột phải › Định nghĩa lại từ tệp); Command Reference công khai (-INSERT với đường dẫn tệp và cú pháp `tên=tệp`); API DOC-013 (nạp DWG chỉ đọc), DOC-014 (chép sâu giữa hai bản vẽ).
- **Đầu ra:** `commands/block/insert_file.{h,cpp}` bổ sung cho `INSERT`/`-INSERT` (CMD-542): tên là đường dẫn tệp hoặc `tên=tệp`; nạp tệp ở nền (NT-013, hủy được); chép Model space của tệp thành định nghĩa block mang tên tệp, điểm gốc theo `INSBASE` của tệp; quy đổi `INSUNITS`; xref lồng trong tệp được gắn thành xref; định nghĩa trùng tên đã có trong bản vẽ được giữ (như AutoCAD); block trùng tên hỏi định nghĩa lại (bỏ hỏi khi `EXPERT` ≥ 2). Cập nhật `docs/specs/commands/-INSERT.md`; kiểm thử `commands/block/tests/insert_file_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 25 ca vàng **[đề xuất]** đạt 100%; kết quả khớp DXF tham chiếu QA-007 khi có đáp án.
  2. Tên trùng: định nghĩa có sẵn được giữ, không tạo bản trùng; AUDIT sạch.
  3. Chèn DWG 20 MB làm block ≤ 2 s tầm trung **[đề xuất]**; luồng UI không bị chặn quá 16 ms; hủy giữa chừng không để lại gì trong bản vẽ.
  4. 1 mark undo cho mỗi lần chèn.
  5. Đạt DoD-C, DoD-H.

### CMD-544 · WBLOCK, -WBLOCK, BASE
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-540, DOC-014, DWG-021, DWG-022, NT-011, INT-002, INT-003, INT-004, INT-007, INT-008, DOC-008, LSP-006, QA-004, QA-007, CMD:OOPS
- **Lệnh:** WBLOCK, -WBLOCK, BASE
- **Màn hình:** UI-G03
- **Đầu vào:** 03 §11, Phụ lục A; 07.2 UI-G03 (nguồn block / cả bản vẽ / đối tượng; điểm gốc; đích; đơn vị chèn; phiên bản DWG); prototype `#UI-G03`: `js/screens7.js` › `blockDlg(ctx, true)`, `js/screens6.js` › `IMPL.BASE`; Command Reference công khai (WBLOCK, -WBLOCK, BASE, `INSBASE`).
- **Đầu ra:** `commands/block/wblock_cmd.cpp`:
  - Backend hộp thoại và `-WBLOCK` (tên tệp → block có sẵn, `=` cùng tên tệp, `*` cả bản vẽ, hoặc định nghĩa mới → điểm gốc → chọn đối tượng); tệp đã có thì hỏi ghi đè (bỏ hỏi khi `EXPERT` ≥ 2).
  - Dựng bản vẽ mới bằng DOC-014; `*` loại các named object không được dùng như AutoCAD; ghi bằng bộ ghi R2018 (DWG-022) hoặc R2004 (DWG-021), ghi nguyên tử (NT-011); các phiên bản khác thêm khi có DWG-023.
  - `BASE`: đặt `INSBASE` ("Điểm gốc <0,0,0>"). Đặc tả `docs/specs/commands/WBLOCK.md`, `-WBLOCK.md`, `BASE.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 25 ca vàng **[đề xuất]** đạt 100%; khớp QA-007 khi có đáp án.
  2. Tệp ghi ra mở được bằng 3 bộ đọc độc lập, không lỗi, không hỏi recover (QA-004).
  3. `WBLOCK *` trên 500 tệp QA-001 **[đề xuất]**: mọi đối tượng nhìn thấy được giữ, named object không dùng bị loại; `INSBASE` và đơn vị chèn đúng.
  4. Khi Giữ nguyên, bản vẽ nguồn không đổi (hash snapshot trùng); Xóa hoặc Chuyển thành block là 1 mark undo; `BASE` là 1 mark undo.
  5. `WBLOCK` 1 triệu entity ≤ 3 s trên 8 nhân **[đề xuất]**.
  6. Đạt kiểm thử ba đường vào (§0).
  7. Đạt DoD-C, DoD-H.

### CMD-545 · ATTDEF, -ATTDEF
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD:TEXT, CMD:MTEXT, INT-002, INT-003, INT-004, INT-005, INT-007, DOC-008, LSP-006, QA-007
- **Lệnh:** ATTDEF, -ATTDEF
- **Màn hình:** UI-G06
- **Đầu vào:** 03 §11 (ATTDEF P0), Phụ lục A; 07.2 UI-G06; 07.1 §9.10 (nhấp đúp attribute definition sửa Tag/Prompt/Default); prototype `#UI-G06`: `js/screens7.js` › `IMPL.ATTDEF`; Command Reference công khai (ATTDEF, -ATTDEF, `AFLAGS`).
- **Đầu ra:** `commands/block/attdef_cmd.cpp`:
  - Chế độ `Invisible`, `Constant`, `Verify`, `Preset`, `Lock position`, `Multiple lines` (mặc định theo `AFLAGS`); Tag (không khoảng trắng, đổi thành chữ hoa), Prompt (rỗng thì dùng Tag), Default.
  - Điểm chèn và thiết lập chữ (canh lề, kiểu chữ, chú thích theo tỉ lệ, chiều cao, góc, bề rộng khung cho nhiều dòng) theo đúng chuỗi prompt và quy tắc đặt chữ của `TEXT`/`MTEXT`; ô "Canh dưới attribute trước".
  - `-ATTDEF`: vòng đổi chế độ → Tag → Prompt → Default → điểm đầu hoặc [`Justify`/`Style`] → chiều cao → góc.
  - Đặc tả `docs/specs/commands/ATTDEF.md`, `-ATTDEF.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 30 ca vàng **[đề xuất]** đạt 100%; khớp QA-007 khi có đáp án.
  2. Cờ chế độ, Tag, Prompt, Default đọc → ghi → đọc trùng 100%.
  3. Vị trí và hình chữ trùng `TEXT`/`MTEXT` với cùng canh lề và kiểu chữ (so primitive regen).
  4. Đạt kiểm undo và kiểm thử ba đường vào (§0).
  5. Đạt DoD-C.
- **Ngoài phạm vi:** chèn field vào Default (1.0, theo DOC-022).

### CMD-546 · EATTEDIT
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-542, CMD:MTEXT, DOC-012, INT-002, INT-003, INT-008, INT-018, DOC-008, QA-007
- **Lệnh:** EATTEDIT
- **Màn hình:** UI-G07
- **Đầu vào:** 03 §11 (EATTEDIT P0), Phụ lục A; 07.2 UI-G07; 07.1 §9.10 (nhấp đúp block có attribute mở trình sửa attribute); prototype `#UI-G07`: `js/screens7.js` › `attCmd(ctx, pre, false)`; Command Reference công khai (EATTEDIT).
- **Đầu ra:** `commands/block/eattedit.{h,cpp}`: chọn block có attribute (block không có attribute sửa được, xref, layer khóa thì báo lý do); dữ liệu 3 tab: Attribute (tag, prompt, giá trị; attribute nhiều dòng mở trình soạn MTEXT), Tùy chọn chữ (kiểu, canh, cao, góc, viết ngược, lộn ngược, hệ số rộng, nghiêng, chú thích theo tỉ lệ, bề rộng khung), Thuộc tính (layer, linetype, màu, plot style, độ dày nét); Áp dụng nhiều lần rồi Lưu vẫn là 1 mark undo; nút Chọn block; attribute `Constant` không hiện; hành động nhấp đúp qua INT-018. Đặc tả `docs/specs/commands/EATTEDIT.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 25 ca vàng **[đề xuất]** đạt 100%; khớp QA-007 khi có đáp án.
  2. Chỉ tham chiếu được chọn thay đổi; các tham chiếu khác cùng block trùng từng trường như trước.
  3. Mỗi lần chạy lệnh là 1 mark undo kể cả khi bấm Áp dụng nhiều lần; đạt kiểm undo (§0).
  4. Mở dữ liệu cho block 100 attribute ≤ 16 ms (DoD-L).
  5. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** sửa attribute bằng dòng lệnh (`-ATTEDIT`, CMD-553); nút "Block kế tiếp" (P2).

### CMD-547 · ATTSYNC, ATTDISP
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-540, DOC-012, RGN-018, NT-005, INT-002, INT-003, INT-005, INT-007, INT-008, DOC-008, LSP-006, QA-007
- **Lệnh:** ATTSYNC, ATTDISP
- **Đầu vào:** 03 §11 (P0), Phụ lục A; 07.2 UI-E10 (`ATTSYNC`: `?`, `Name`, `Select`; `ATTDISP`: `Normal`, `ON`, `OFF`); 07.1 §7.3 (Hiển thị attribute); prototype `js/screens10.js` › `IMPL.ATTSYNC`, `js/screens7.js` › `IMPL.ATTDISP`; Command Reference công khai (ATTSYNC, ATTDISP, `ATTMODE`).
- **Đầu ra:** `commands/block/attsync.{h,cpp}`: engine đồng bộ dùng chung cho `ATTSYNC`, BATTMAN, BLOCKREPLACE: thêm ATTRIB cho ATTDEF mới (giá trị mặc định), xóa ATTRIB có tag không còn, giữ giá trị của tag trùng, đặt lại thuộc tính và vị trí theo định nghĩa như AutoCAD; chạy song song theo tham chiếu (NT-005); dynamic block đồng bộ mọi biểu diễn `*U`. Lệnh `ATTSYNC` (`?`/`Name`/`Select` → xác nhận theo tên block), `ATTDISP` (`Normal`/`ON`/`OFF` → `ATTMODE` → regen chỉ attribute). Đặc tả `docs/specs/commands/ATTSYNC.md`, `ATTDISP.md`; ca vàng; benchmark `commands/block/bench/attsync_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 25 ca vàng **[đề xuất]** đạt 100%; khớp 100% QA-007 khi có đáp án.
  2. `ATTSYNC` cho 100.000 tham chiếu ≤ 1 s trên 8 nhân **[đề xuất]**, hiệu suất song song ≥ 60% đến 8 luồng, kết quả trùng từng bit với 1–32 luồng.
  3. `ATTDISP` trên BENCH-STD: chỉ attribute regen (bộ đếm), xong ≤ 0,5 s.
  4. Đạt kiểm undo và kiểm thử ba đường vào (§0).
  5. Đạt DoD-C, DoD-H.

## 6. Block: palette, Block Editor, sửa tại chỗ, dynamic block (P1)

### CMD-550 · Dữ liệu palette Blocks: bản vẽ hiện hành và thư viện; BLOCKSPALETTE
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-542, CMD-543, DOC-012, DOC-013, DWG-026, RND-021, VN-009, NT-011, NT-012, NT-013
- **Lệnh:** BLOCKSPALETTE, BLOCKSPALETTECLOSE
- **Màn hình:** UI-G01
- **Đầu vào:** 03 §11 (BLOCKSPALETTE P1; thư viện block VN); 07.2 UI-G01 (tab Bản vẽ hiện hành, Thư viện kèm liên kết Thư viện TCVN; tìm, lọc; tùy chọn chèn; trạng thái rỗng; EA cho Bản vẽ hiện hành và Thư viện); 07.3 UI-J13; 07.1 §9.11 (kéo-thả block); prototype `#UI-G01`: `js/screens7.js` › `IMPL.BLOCKSPALETTE`; API VN-009 (chỉ mục Thư viện TCVN), DWG-026 (ảnh xem trước trong tệp), RND-021 (render ngoài màn hình).
- **Đầu ra:** `commands/block/blocks_palette_model.{h,cpp}`: danh sách block của bản vẽ hiện hành (bỏ block vô danh, xref, layout; tên hiệu lực của dynamic block), ảnh xem trước (ảnh lưu trong bản ghi block, nếu không có thì render ngoài màn hình), mô tả; thư viện là thư mục hoặc tệp DWG (mỗi tệp liệt kê block bên trong) và Thư viện TCVN qua VN-009; tìm không phân biệt dấu; chỉ mục nền (NT-013) có cache, cập nhật khi tệp đổi (NT-011); chèn hoặc kéo-thả gửi chuỗi lệnh `-INSERT` có đủ tham số qua INT-002. Lệnh `BLOCKSPALETTE`, `BLOCKSPALETTECLOSE`; đặc tả `docs/specs/commands/BLOCKSPALETTE.md`; kiểm thử `commands/block/tests/blocks_palette_model_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Bản vẽ 2.000 block: danh sách có nội dung ≤ 150 ms (DoD-L), ảnh xem trước nạp dần ở nền.
  2. Thư mục 500 tệp DWG: lập chỉ mục nền ≤ 10 s tầm trung lần đầu, lần sau dùng cache ≤ 300 ms **[đề xuất]**; tệp thêm hoặc đổi xuất hiện ≤ 2 s **[đề xuất]**.
  3. Tìm "cua di" khớp "CỬA-ĐI-900"; lọc 10.000 mục ≤ 16 ms mỗi phím (DoD-L).
  4. Chèn từ palette và từ dòng lệnh với cùng tùy chọn cho DXF trùng từng trường (QA-003).
  5. Đạt DoD-C, DoD-H, DoD-L.

### CMD-551 · Palette Blocks: Gần đây, Yêu thích, định nghĩa lại từ tệp
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-543, CMD-550, NT-010
- **Màn hình:** UI-G01
- **Đầu vào:** 07.2 UI-G01 (tab Gần đây, Yêu thích; chuột phải: Định nghĩa lại từ tệp, Thêm vào Yêu thích, Mở tệp nguồn); Command Reference công khai (`BLOCKMRULIST`, `BLOCKREDEFINEMODE`).
- **Đầu ra:** `commands/block/blocks_recent.{h,cpp}`: danh sách block dùng gần đây qua mọi bản vẽ (số mục theo `BLOCKMRULIST`), Yêu thích (thư mục trong profile NT-010); Định nghĩa lại từ tệp gửi `-INSERT tên=tệp` (CMD-543) và hỏi theo `BLOCKREDEFINEMODE`; Mở tệp nguồn. Kiểm thử `commands/block/tests/blocks_recent_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Chèn block ở bản vẽ A rồi mở bản vẽ B: block có trong Gần đây của B với ảnh xem trước; giữ đúng `BLOCKMRULIST` mục, mới nhất trước.
  2. Yêu thích giữ qua lần khởi động lại; mục trỏ tới tệp đã xóa được đánh dấu thiếu, không lỗi.
  3. Định nghĩa lại từ tệp cập nhật mọi tham chiếu trong 1 mark undo.
  4. Đạt DoD-C.

### CMD-552 · MINSERT
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-542, RGN-018, INT-002, INT-003, INT-004, INT-007, INT-012, DOC-008, LSP-006, QA-007
- **Lệnh:** MINSERT
- **Đầu vào:** 03 §11 (MINSERT P1); Command Reference công khai (MINSERT); API RGN-018 (hiển thị MINSERT).
- **Đầu ra:** `commands/block/minsert_cmd.cpp`: tên block → điểm chèn hoặc [`Basepoint`/`Scale`/`X`/`Y`/`Z`/`Rotate`] → tỉ lệ → góc → số hàng → số cột → khoảng cách hàng (hoặc ô đơn vị) → khoảng cách cột; tạo INSERT có hàng/cột; attribute như `INSERT`; MINSERT không phá khối được (như AutoCAD). Đặc tả `docs/specs/commands/MINSERT.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 15 ca vàng **[đề xuất]** đạt 100%; khớp QA-007 khi có đáp án.
  2. Jig ≤ 8 ms/khung cho lưới 100 × 100 bản sao (DoD-L) **[đề xuất]**.
  3. Đạt kiểm undo và kiểm thử ba đường vào (§0).
  4. Đạt DoD-C, DoD-L.

### CMD-553 · ATTEDIT, -ATTEDIT
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-512, CMD-546, NT-012, INT-002, INT-003, INT-007, INT-008, DOC-008, LSP-006, QA-007
- **Lệnh:** ATTEDIT, -ATTEDIT
- **Màn hình:** UI-G07
- **Đầu vào:** 03 §11 (ATTEDIT P1); 07.2 UI-G07; prototype `js/screens7.js` › `attCmd(ctx, pre, true)`; Command Reference công khai (ATTEDIT, -ATTEDIT).
- **Đầu ra:** `commands/block/attedit_cmd.cpp`: `ATTEDIT` (hộp sửa giá trị kiểu cổ điển); `-ATTEDIT`: sửa từng cái hay toàn cục → lọc theo tên block, tag, giá trị (wildcard) → chọn attribute; từng cái: [`Value` (`Change`/`Replace`)/`Position`/`Height`/`Angle`/`Style`/`Layer`/`Color`/`Next`]; toàn cục: chỉ attribute nhìn thấy hay tất cả → chuỗi cần đổi → chuỗi mới. Đặc tả `docs/specs/commands/ATTEDIT.md`, `-ATTEDIT.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 30 ca vàng **[đề xuất]** (từng cái, toàn cục, wildcard, chữ tiếng Việt có dấu) đạt 100%; khớp QA-007 khi có đáp án.
  2. Sửa toàn cục 100.000 attribute ≤ 300 ms trên 8 nhân **[đề xuất]**.
  3. Đạt kiểm undo và kiểm thử ba đường vào (§0).
  4. Đạt DoD-C, DoD-L.

### CMD-554 · BATTMAN
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-545, CMD-547, DOC-012, INT-002, INT-008, DOC-008, QA-007
- **Lệnh:** BATTMAN
- **Màn hình:** UI-G08
- **Đầu vào:** 03 §11 (BATTMAN P1); 07.2 UI-G08; prototype `#UI-G08`: `js/screens10.js` › `IMPL.BATTMAN`; Command Reference công khai (BATTMAN).
- **Đầu ra:** `commands/block/battman.{h,cpp}`: chọn block (dropdown hoặc chọn trên bản vẽ), số tham chiếu; danh sách ATTDEF (tag, prompt, mặc định, chế độ); Lên/Xuống (đổi thứ tự hỏi khi chèn), Sửa (các tab như UI-G07), Gỡ, Đồng bộ (CMD-547); thiết lập áp thay đổi cho tham chiếu có sẵn; đánh dấu tag trùng. Đặc tả `docs/specs/commands/BATTMAN.md`; kiểm thử `commands/block/tests/battman_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 20 ca vàng **[đề xuất]** đạt 100%; khớp QA-007 khi có đáp án.
  2. Đổi thứ tự rồi `INSERT`: thứ tự hỏi attribute đúng thứ tự mới.
  3. Mỗi lần đóng hộp thoại có thay đổi là 1 mark undo; đạt kiểm undo (§0).
  4. Đạt DoD-C.

### CMD-555 · Block Editor: BEDIT, -BEDIT, BCLOSE, BSAVE, BSAVEAS
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** CMD-540, CMD-544, CMD-545, DOC-014, DOC-023, INT-002, INT-003, INT-005, INT-015, DOC-008, QA-007
- **Lệnh:** BEDIT, -BEDIT, BCLOSE, BSAVE, BSAVEAS, BATTORDER
- **Màn hình:** UI-G04
- **Đầu vào:** 03 §11 (Block Editor P1); 07.2 UI-G04 (hộp chọn block có xem trước; UCS tại gốc block; thanh "Đang sửa block…"; Mở/Lưu; Đóng hỏi lưu); 07.1 §9.10 (nhấp đúp block không có attribute hỏi mở Block Editor); prototype `#UI-G04`: `js/screens10.js` › `IMPL.BEDIT`, `IMPL.BSAVE`, `IMPL.BCLOSE`, `IMPL.BASE`, `beSaveAs`; Command Reference công khai (BEDIT, -BEDIT, BCLOSE, BSAVE, BSAVEAS, `BLOCKEDITOR`, `BLOCKEDITLOCK`).
- **Đầu ra:** `commands/block/bedit/block_editor.{h,cpp}`: BATTORDER: đặt thứ tự prompt attribute của block đang sửa, lưu cùng định nghĩa block.
  - Phiên sửa: chép định nghĩa vào không gian sửa tạm (DOC-014), UCS tại gốc block (INT-015), `BLOCKEDITOR` = 1, danh sách lệnh bị cấm trong Block Editor; `BASE` trong phiên đổi gốc block; ATTDEF trong phiên.
  - `BSAVE` định nghĩa lại qua CMD-540 (cập nhật mọi tham chiếu, cảnh báo số lượng); `BSAVEAS` lưu thành block mới; `BCLOSE` hỏi lưu nếu có thay đổi; undo trong phiên tách khỏi undo của bản vẽ.
  - `BEDIT` (hộp chọn hoặc block đang chọn; tạo block mới), `-BEDIT` (tên hoặc `?`); không mở cho xref, block phụ thuộc xref, block của layout; `BLOCKEDITLOCK` = 1 thì chặn; dynamic block: sửa hình học giữ nguyên dữ liệu parameter và action (DOC-023).
  - Đặc tả `docs/specs/commands/BEDIT.md`, `-BEDIT.md`, `BCLOSE.md`, `BSAVE.md`, `BSAVEAS.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 30 ca vàng **[đề xuất]** đạt 100%; khớp QA-007 khi có đáp án.
  2. Sửa rồi `BSAVE`: mọi tham chiếu hiện hình mới, 1 mark undo ở bản vẽ. `BCLOSE` không lưu: định nghĩa trùng từng bit như trước.
  3. Dynamic block trong QA-001 (≥ 100 định nghĩa **[đề xuất]**): sửa hình học tĩnh rồi lưu, dữ liệu EvalGraph giữ nguyên từng trường (QA-003).
  4. Vào Block Editor với block 100.000 entity ≤ 300 ms tầm trung **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** tạo parameter, action, visibility (CMD-570, CMD-571); `BTESTBLOCK` (CMD-574).

### CMD-556 · Sửa tại chỗ block và xref: REFEDIT, -REFEDIT, REFSET, REFCLOSE
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** CMD-540, DOC-012, DOC-013, DOC-014, RND-013, INT-002, INT-003, INT-005, INT-008, INT-009, DOC-008, QA-007
- **Lệnh:** REFEDIT, -REFEDIT, REFSET, REFCLOSE
- **Màn hình:** UI-G19
- **Đầu vào:** 03 §11 (REFEDIT, REFSET, REFCLOSE P1); 07.2 UI-G19 (cây tham chiếu lồng tại điểm chọn; tự chọn mọi đối tượng lồng hoặc chọn từng cái; tên duy nhất cho layer và kiểu trùng; hiện attribute definition; khóa đối tượng ngoài tập làm việc; đối tượng ngoài tập làm việc mờ đi); 07.1 §9.10 (nhấp đúp xref thì sửa tại chỗ, có xác nhận); prototype `#UI-G19`: `js/screens10.js` › `IMPL.REFEDIT`, `IMPL.REFSET`, `IMPL.REFCLOSE`; Command Reference công khai (REFEDIT, -REFEDIT, REFSET, REFCLOSE, `REFEDITNAME`, `XFADECTL`, `XEDIT`).
- **Đầu ra:** `commands/block/refedit/ref_edit.{h,cpp}`:
  - Chọn tham chiếu → chọn mức lồng (`Ok`/`Next` ở `-REFEDIT`) → cách chọn đối tượng (`All`/`Nested`) → hiện attribute definition để sửa (`Yes`/`No`, chỉ cho block).
  - Tập làm việc: đối tượng lồng được đưa ra không gian chủ với ma trận ghép; tên trùng của layer và kiểu đổi bằng tiền tố như AutoCAD; đối tượng ngoài tập làm việc mờ theo `XFADECTL` (RND-013) và bị khóa nếu chọn.
  - `REFSET` [`Add`/`Remove`] chuyển đối tượng giữa tập làm việc và bản vẽ chủ. `REFCLOSE` [`Save`/`Discard`]: lưu block cập nhật mọi tham chiếu (xác nhận nêu số lượng); Discard trả mọi thứ về như cũ.
  - Không sửa xref có `XEDIT` = 0, block vô danh, MINSERT. Đặc tả `docs/specs/commands/REFEDIT.md`, `-REFEDIT.md`, `REFSET.md`, `REFCLOSE.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 30 ca vàng **[đề xuất]** (block lồng 3 mức, xref lồng, attribute definition, `REFSET` hai chiều, Discard) đạt 100%; khớp QA-007 khi có đáp án.
  2. Discard: snapshot bản vẽ chủ và định nghĩa trùng từng bit như trước `REFEDIT`.
  3. Save block: hình học mới đúng ở mọi tham chiếu (so với áp ma trận ngược lên tập làm việc, sai số ≤ 1e-9 × kích thước bản vẽ); 1 mark undo cho cả phiên.
  4. Vào phiên với tham chiếu 100.000 entity ≤ 300 ms tầm trung **[đề xuất]**; làm mờ không dựng lại batch.
  5. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** ghi thay đổi về tệp xref (CMD-557).

### CMD-557 · Ghi thay đổi sửa tại chỗ về tệp xref
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-556, DOC-018, DOC-019, DWG-018, DWG-022, DWG-023, NT-011, QA-003, QA-004
- **Màn hình:** UI-G19, UI-B05
- **Đầu vào:** 07.2 UI-G19 (banner "thay đổi sẽ ghi vào KT-01.dwg"; xref đang được người khác mở thì không cho lưu); 07.1 UI-B05 (tệp đang được người khác mở); API DOC-019 (khóa `.dwl`/`.dwl2`, `.bak`), DOC-018 (bản vẽ đang mở trong phiên), DWG-018 (giữ dữ liệu chưa hiểu), DWG-022, DWG-023 (bộ ghi theo phiên bản gốc).
- **Đầu ra:** `commands/block/refedit/xref_writeback.{h,cpp}` được `REFCLOSE` `Save` gọi khi đối tượng là xref: kiểm khóa tệp (báo tên máy và người đang mở), ghi lại tệp xref theo phiên bản gốc, tạo `.bak`, ghi nguyên tử (NT-011); nạp lại xref ở bản vẽ chủ và ở mọi bản vẽ khác đang mở có tham chiếu tới nó. Kiểm thử `commands/block/refedit/tests/xref_writeback_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Tệp xref sau khi lưu mở được bằng 3 bộ đọc độc lập, không hỏi recover (QA-004); đối tượng không sửa trùng 100% từng trường kể cả dữ liệu chưa hiểu (QA-003).
  2. Tệp đang bị khóa: không ghi, báo người đang mở; tệp gốc không đổi (hash).
  3. Ngắt giả lập giữa chừng: tệp xref cũ còn nguyên, không có tệp hỏng.
  4. Bản vẽ chủ và các bản vẽ đang mở khác hiện hình mới ≤ 1,5 s sau khi lưu xref 20 MB (ngân sách mở tệp 05 §1.1).
  5. Đạt DoD-C.

### CMD-558 · Grip tham số của dynamic block trên bản vẽ
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** CMD-540, RGN-019, DOC-023, INT-012, INT-013, DOC-008, QA-003, QA-007, RGN-160
- **Màn hình:** UI-A12
- **Đầu vào:** 03 §11 (grip tương tác linear, polar, XY, rotation, flip, visibility, lookup: P1); 05 §8, §10 (grip của dynamic block); 07.1 §9.5 (grip tam giác/mũi tên; phát hành 1.0); README §4.2 (grip dynamic block đăng ký qua INT-013); API RGN-019 (engine EvalGraph), DOC-023 (parameter, action, value set); kho dynamic block của QA-001.
- **Đầu ra:** `commands/block/dynblock/dyn_grips.{h,cpp}`: đăng ký với INT-013 grip của parameter point, linear, polar, XY, rotation, alignment, flip, base point (hình grip theo loại); khi kéo: jig gọi EvalGraph với giá trị mới, bám value set (danh sách, bước, min/max), chạy chuỗi action; khi thả: cập nhật biểu diễn `*U` (dùng lại biểu diễn có cùng trạng thái, nếu không thì tạo mới) **[đề xuất]**; 1 mark undo cho mỗi lần kéo grip. Kiểm thử `commands/block/dynblock/tests/dyn_grips_test.cpp`; benchmark `dyn_grips_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 300 dynamic block trong QA-001 **[đề xuất]**: kéo grip theo kịch bản cho hình học khớp đáp án QA-007 với sai số ≤ 1e-9 × kích thước bản vẽ (trước khi có đáp án: ca vàng tự soạn cho từng loại parameter và action).
  2. Lưu rồi mở lại (QA-003): trạng thái tham số và liên kết `*U` giữ nguyên; DWG TrueView hiện đúng trạng thái mới.
  3. Jig khi kéo grip ≤ 8 ms/khung (DoD-L) với block 5.000 entity **[đề xuất]**.
  4. Value set: giá trị thả luôn nằm trong tập cho phép (property-based 100.000 lần kéo ngẫu nhiên **[đề xuất]**).
  5. Đạt DoD-C, DoD-L.

### CMD-559 · Trạng thái hiển thị, bảng tra, thuộc tính Tùy biến của dynamic block; RESETBLOCK
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-510, CMD-515, CMD-547, CMD-558, RGN-019, DOC-023, INT-002, INT-008, DOC-008
- **Lệnh:** RESETBLOCK
- **Màn hình:** UI-C01, UI-A12
- **Đầu vào:** 07.1 §9.5 (rê lên grip của dynamic block hiện danh sách trạng thái hiển thị hoặc giá trị tra bảng); 07.2 UI-C01 (Block: nhóm Tùy biến gồm Khoảng cách, Lật, Trạng thái hiển thị, Tra bảng…); prototype `js/screens16.js` › `IMPL.RESETBLOCK`; Command Reference công khai (RESETBLOCK).
- **Đầu ra:** `commands/block/dynblock/dyn_props.{h,cpp}`: menu grip cho visibility và lookup (danh sách giá trị, tra ngược khi bảng cho phép); nhóm Tùy biến của palette Thuộc tính (đọc qua CMD-510, ghi qua `TCPROPSET` với mã thuộc tính động); tham chiếu đổi trạng thái thì attribute giữ giá trị và đồng bộ vị trí theo action (CMD-547); lệnh `RESETBLOCK` (chọn đối tượng → đưa tham số về mặc định). Đặc tả `docs/specs/commands/RESETBLOCK.md`; kiểm thử `commands/block/dynblock/tests/dyn_props_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Đổi trạng thái hiển thị và giá trị tra bảng trên ≥ 100 dynamic block **[đề xuất]** cho hình học khớp đáp án QA-007 (trước đó: ca vàng).
  2. Sửa thuộc tính Tùy biến cho 1.000 tham chiếu đang chọn là 1 mark undo, ≤ 500 ms trên 8 nhân **[đề xuất]**.
  3. `RESETBLOCK` đưa tham chiếu về đúng hình của trạng thái mặc định; 1 mark undo.
  4. Đạt DoD-C, DoD-L.

### CMD-560 · Dữ liệu DesignCenter: ADCENTER, ADCCLOSE, ADCNAVIGATE
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-543, DOC-013, DOC-014, DOC-016, DWG-026, NT-011, NT-013, INT-002, DOC-008
- **Lệnh:** ADCENTER, ADCCLOSE, ADCNAVIGATE, TCADCIMPORT
- **Màn hình:** UI-G17
- **Đầu vào:** 03 §11 (ADCENTER P1); 07.2 UI-G17 (cây thư mục, bản vẽ đang mở, lịch sử; nội dung: Block, Kiểu kích thước, Layer, Layout, Linetype, Kiểu multileader, Kiểu bảng, Kiểu chữ, Xref; xem trước; kéo-thả; tìm theo tên, loại, ngày; yêu thích); prototype `#UI-G17`: `js/screens10.js` › `IMPL.ADCENTER`, `dcGo`; Command Reference công khai (ADCENTER, ADCCLOSE, ADCNAVIGATE).
- **Đầu ra:** `commands/block/adcenter/adc_model.{h,cpp}`: liệt kê nội dung của bản vẽ đang mở hoặc tệp đóng (nạp lười chỉ bảng ký hiệu và block record qua DOC-016, DOC-013); ảnh xem trước (DWG-026); tìm ở nền có chỉ mục (NT-013); lịch sử và yêu thích. Kéo block vào bản vẽ gửi `-INSERT`; kéo layer, kiểu, layout gửi lệnh ẩn `TCADCIMPORT` **[đề xuất]** chép named object qua DOC-014 (tên đã có trong bản vẽ thì giữ bản có sẵn như AutoCAD). Lệnh `ADCENTER`, `ADCCLOSE`, `ADCNAVIGATE` (đường dẫn). Đặc tả `docs/specs/commands/ADCENTER.md`, `ADCCLOSE.md`, `ADCNAVIGATE.md`, `TCADCIMPORT.md`; kiểm thử.
- **Tiêu chí nghiệm thu:**
  1. Liệt kê nội dung tệp DWG 20 MB đang đóng ≤ 300 ms tầm trung **[đề xuất]** (chỉ giải mã bảng ký hiệu và block record).
  2. Kéo layer, kiểu chữ, kiểu kích thước, linetype, layout từ tệp khác: thuộc tính trùng nguồn 100%; phụ thuộc kéo theo (linetype của layer, kiểu chữ của kiểu kích thước, block mũi tên) được chép; 1 mark undo mỗi lần thả.
  3. Tìm trên thư mục 1.000 tệp **[đề xuất]**: kết quả đầu ≤ 1 s, chạy nền, hủy ≤ 50 ms.
  4. Đạt DoD-C, DoD-H, DoD-L.

### CMD-561 · Dữ liệu Tool Palettes: TOOLPALETTES, TOOLPALETTESCLOSE
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-542, CMD:HATCH, CUS-001, NT-010, DWG-026, RND-021, INT-002
- **Lệnh:** TOOLPALETTES, TOOLPALETTESCLOSE
- **Màn hình:** UI-G18
- **Đầu vào:** 07.2 UI-G18 (mỗi công cụ là block, hatch, lệnh hoặc macro LISP; thuộc tính công cụ: layer, màu, tỉ lệ, góc, hỏi góc khi chèn, phá khối…; kéo đối tượng vào để tạo công cụ; nhóm palette; `.xtp` là P2); 07.1 §9.11; prototype `#UI-G18`: `js/screens10.js` › `IMPL.TOOLPALETTES`, `tpNewPal`, thuộc tính công cụ; API CUS-001 (chạy macro menu).
- **Đầu ra:** `commands/block/toolpalettes/tool_palette_model.{h,cpp}`: lưu palette, nhóm palette và công cụ trong profile (NT-010, JSON có phiên bản) **[đề xuất]**; tạo công cụ từ đối tượng đang chọn (block, hatch, đối tượng hình học thành lệnh vẽ tương ứng) kèm ảnh xem trước; chạy công cụ thành chuỗi lệnh qua INT-002 (`-INSERT` có thuộc tính công cụ, `HATCH` có mẫu, tỉ lệ, góc, lệnh hoặc macro qua CUS-001). Lệnh `TOOLPALETTES`, `TOOLPALETTESCLOSE`. Đặc tả `docs/specs/commands/TOOLPALETTES.md`, `TOOLPALETTESCLOSE.md`; kiểm thử.
- **Tiêu chí nghiệm thu:**
  1. Chạy công cụ block, hatch, lệnh, macro cho DXF trùng khi gõ chuỗi lệnh tương đương (QA-003); thuộc tính công cụ (layer, màu, tỉ lệ, góc, phá khối) áp đúng 100%.
  2. Lưu palette rồi khởi động lại: công cụ, thứ tự, nhóm trùng 100%; tệp profile hỏng thì báo và giữ bản sao lưu.
  3. Mở 20 palette × 100 công cụ: dữ liệu sẵn sàng ≤ 150 ms (DoD-L).
  4. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** nhập, xuất `.xtp` (P2).

## 7. Block: công cụ Mở rộng (Express) P1

### CMD-565 · BURST, XPLODE
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-512, CMD:EXPLODE, DOC-012, GEO-022, INT-002, INT-003, INT-007, INT-008, DOC-008, LSP-006, QA-007
- **Lệnh:** BURST, XPLODE
- **Màn hình:** UI-MR
- **Đầu vào:** 03 §11, §28 (BURST P1; XPLODE P1 theo 03 §11); 07.1 §7.10 (panel Block của tab Mở rộng); 07.2 UI-E10 (`BURST` / `XPLODE`: `All`, `Color`, `LAyer`, `LType`, `LWeight`, `Inherit from parent block`, `Explode`; `Individually` / `Globally`); prototype `#UI-MR`: `js/screens10.js` › `IMPL.BURST`, `IMPL.XPLODE`; tài liệu công khai của Express Tools (BURST, XPLODE); API của `EXPLODE` (CMD-2), GEO-022 (biến đổi đường cong khi tỉ lệ không đều).
- **Đầu ra:** `commands/express/burst_xplode.cpp`:
  - `BURST`: chọn block → phá một mức; ATTRIB nhìn thấy thành TEXT (MTEXT nếu nhiều dòng) mang giá trị và thuộc tính hiện tại; ATTRIB ẩn bị bỏ; thuộc tính ByBlock của đối tượng con xử lý như Express BURST.
  - `XPLODE`: chọn đối tượng → báo số tìm thấy → `Individually`/`Globally` → tùy chọn thuộc tính áp cho đối tượng sau khi phá (màu dùng bộ phân tích CMD-512).
  - Đặc tả `docs/specs/commands/BURST.md`, `XPLODE.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 30 ca vàng **[đề xuất]** (block lồng, tỉ lệ không đều, attribute nhiều dòng, attribute ẩn, ByBlock, layer 0) đạt 100%; khớp QA-007 khi có đáp án.
  2. Chữ tạo từ attribute trùng vị trí và hình với ATTRIB gốc (so primitive regen, sai số ≤ 1e-9 × kích thước bản vẽ).
  3. `BURST` 10.000 block ≤ 1 s trên 8 nhân **[đề xuất]**.
  4. Đạt kiểm undo và kiểm thử ba đường vào (§0).
  5. Đạt DoD-C, DoD-H.

### CMD-566 · NCOPY: sao chép đối tượng lồng
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** DOC-012, DOC-013, DOC-014, INT-004, INT-007, INT-009, INT-012, DOC-008, LSP-006, QA-007
- **Lệnh:** NCOPY
- **Màn hình:** UI-MR
- **Đầu vào:** 03 §28 (NCOPY P1); 07.2 UI-E10 (`NCOPY`: Settings Insert hoặc Bind xref chứa đối tượng); prototype `js/screens10.js` › `IMPL.NCOPY`; tài liệu công khai của Express Tools (NCOPY); API INT-009 (chọn lồng như `nentselp`).
- **Đầu ra:** `commands/express/ncopy.cpp`: chọn đối tượng lồng trong block hoặc xref (lặp) hoặc [`Settings`] → điểm gốc → điểm đích có jig; bản sao ở không gian hiện hành với ma trận ghép của đường lồng; `Settings` chọn cách mang named object của xref (`Insert` / `Bind` / không) như Express Tools; layer và kiểu của xref được tạo với tên phụ thuộc theo quy tắc bind. Đặc tả `docs/specs/commands/NCOPY.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 20 ca vàng **[đề xuất]** (block lồng 3 mức, xref lồng, tỉ lệ âm) đạt 100%; khớp QA-007 khi có đáp án.
  2. Bản sao trùng hình đối tượng lồng nhìn thấy (sai số ≤ 1e-9 × kích thước bản vẽ); bản vẽ nguồn của xref không đổi.
  3. Jig ≤ 8 ms/khung (DoD-L).
  4. Đạt kiểm undo và kiểm thử ba đường vào (§0).
  5. Đạt DoD-C, DoD-L.

### CMD-567 · GATTE: sửa attribute hàng loạt
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-546, DOC-012, NT-005, INT-002, INT-003, INT-007, INT-008, DOC-008, LSP-006
- **Lệnh:** GATTE
- **Màn hình:** UI-MR
- **Đầu vào:** 03 §28 (GATTE P1); 07.2 UI-E10 (`GATTE`: chọn block hoặc gõ tên → tag → giá trị mới; `Name`; hỏi xác nhận nêu số block sẽ đổi); prototype `js/screens10.js` › `IMPL.GATTE`; tài liệu công khai của Express Tools (GATTE).
- **Đầu ra:** `commands/express/gatte.cpp`: chọn block hoặc attribute hoặc [`Name`] → liệt kê tag đã biết → chọn attribute hoặc gõ tag → giá trị mới → báo số tham chiếu và hỏi xử lý tất cả; đổi giá trị cho mọi tham chiếu cùng tên hiệu lực (kể cả trong layout). Đặc tả `docs/specs/commands/GATTE.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 10 ca vàng **[đề xuất]** đạt 100%; khớp QA-007 khi có đáp án.
  2. Đổi 100.000 tham chiếu ≤ 500 ms trên 8 nhân **[đề xuất]**; 1 mark undo.
  3. Đạt kiểm thử ba đường vào (§0).
  4. Đạt DoD-C, DoD-H.

### CMD-568 · BLOCKREPLACE: thay block
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-540, CMD-543, CMD-547, DOC-014, INT-002, INT-003, INT-007, DOC-008, LSP-006, QA-007
- **Lệnh:** BLOCKREPLACE
- **Màn hình:** UI-G16
- **Đầu vào:** 03 §11 (thay block P1), §28; 07.2 UI-G16 (thay block chọn hoặc mọi block cùng tên; block thay từ bản vẽ hoặc thư viện; giữ tỉ lệ và góc, ánh xạ attribute theo tag, dọn định nghĩa cũ); 07.2 UI-C01 (Block: Tên là dropdown để thay block); prototype `#UI-G16`: `js/screens10.js` › `IMPL.BLOCKREPLACE`; tài liệu công khai của Express Tools (BLOCKREPLACE).
- **Đầu ra:** `commands/express/blockreplace.{h,cpp}`: thay tham chiếu đã chọn hoặc mọi tham chiếu cùng tên hiệu lực bằng block khác (block trong bản vẽ hoặc từ tệp qua CMD-543); giữ điểm chèn, tỉ lệ, góc, layer, thuộc tính; ánh xạ giá trị attribute theo tag rồi đồng bộ bằng CMD-547; dọn định nghĩa cũ không còn dùng (hỏi như Express Tools). Khi `CMDDIA` = 0 hoặc chạy từ script: hỏi tên block cũ, tên block mới, dọn định nghĩa **[đề xuất]**. Dùng cho thuộc tính Tên của block ở palette Thuộc tính (qua `TCPROPSET`). Đặc tả `docs/specs/commands/BLOCKREPLACE.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 20 ca vàng **[đề xuất]** đạt 100%; khớp QA-007 khi có đáp án.
  2. Thay 10.000 tham chiếu ≤ 300 ms trên 8 nhân **[đề xuất]**; giá trị attribute cùng tag giữ 100%.
  3. Đạt kiểm undo và kiểm thử ba đường vào (§0).
  4. Đạt DoD-C, DoD-H.

### CMD-569 · TCBLOCKMATCH: tìm cụm hình tương tự và chuyển thành block
- **Mốc:** 1.0 · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** CMD-540, GEO-017, GEO-025, INT-009, NT-005, INT-002, INT-003, INT-008, DOC-008
- **Lệnh:** TCBLOCKMATCH
- **Màn hình:** UI-G16
- **Đầu vào:** 03 §11 (Smart Blocks: Search & Convert, khớp hình học tất định); 07.2 UI-G16 (Tìm & chuyển đổi: chọn mẫu hình học, tìm cụm tương tự theo dung sai, bất biến khi xoay và đổi tỉ lệ; danh sách kết quả, số lượng, tô sáng; chuyển thành block, đếm, chọn); 07.3 §0 (tên lệnh `TCBLOCKMATCH`); prototype `#UI-G16` (`js/cmd.js`: `TCBLOCKMATCH`); API GEO-025 (chữ ký hình học bất biến), GEO-017 (BVH).
- **Đầu ra:** `commands/express/blockmatch/block_match.{h,cpp}`: dựng chữ ký của mẫu (GEO-025); sinh ứng viên quanh đối tượng neo có cùng loại và chữ ký cục bộ qua truy vấn BVH; xác minh toàn cụm theo dung sai; suy ra phép biến đổi (dời, xoay, tỉ lệ đều, đối xứng); kết quả có thứ tự tất định; chuyển mỗi cụm thành INSERT của block mẫu với ma trận tính được (1 mark undo). Lệnh `TCBLOCKMATCH`: chọn đối tượng mẫu → dung sai → phạm vi → hành động [`Block`/`Count`/`Select`] **[đề xuất]**. Bộ dữ liệu gán nhãn ≥ 200 cụm **[đề xuất]** ở `commands/express/blockmatch/tests/data/`; đặc tả `docs/specs/commands/TCBLOCKMATCH.md`; benchmark `block_match_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Trên bộ gán nhãn: recall ≥ 99% và precision ≥ 99% **[đề xuất]**; không có kết quả chồng lấn nhau.
  2. Tìm mẫu 20 entity trong bản vẽ 1 triệu entity ≤ 2 s trên 8 nhân **[đề xuất]**, hiệu suất song song ≥ 60% đến 8 luồng, hủy ≤ 50 ms.
  3. Kết quả trùng từng bit với 1–32 luồng và trên 3 hệ điều hành.
  4. Sau khi chuyển thành block, hình hiển thị trùng cụm gốc với sai số ≤ 1e-9 × kích thước bản vẽ; 1 mark undo.
  5. Đạt DoD-C, DoD-H.

## 8. Block: tạo dynamic block và công cụ P2 (sau v1.0)

### CMD-570 · Tạo parameter và action cho dynamic block: BPARAMETER, BACTION
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** CMD-555, CMD-558, DOC-023, DWG-015, RGN-019, INT-002, INT-003, DOC-008
- **Lệnh:** BPARAMETER, BACTION
- **Màn hình:** UI-G04, UI-G05
- **Đầu vào:** 03 §11 (tạo dynamic block P2); 07.2 UI-G04 (Panel Tham số & Action: point, linear, polar, XY, rotation, alignment, flip, visibility, lookup, base point; action move, scale, stretch, polar stretch, array, rotate, flip, lookup), UI-G05; prototype `#UI-G05`: `js/screens16.js` › `IMPL.BPARAMETER`, `IMPL.BACTION`; Command Reference công khai (BPARAMETER, BACTION).
- **Đầu ra:** `commands/block/authoring/params_actions.{h,cpp}`: tạo, sửa, xóa parameter (tên, nhãn, chuỗi, mô tả, value set) và action (parameter gắn, tập đối tượng, khung stretch, điểm gốc) trong Block Editor, ghi đối tượng EvalGraph theo DOC-023 sao cho AutoCAD đọc được; kiểm hợp lệ (action phải có parameter và tập đối tượng). Đặc tả `docs/specs/commands/BPARAMETER.md`, `BACTION.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 40 ca vàng **[đề xuất]** (mỗi loại parameter và action): block tạo ra chạy đúng ở CMD-558 và mở đúng ở AutoCAD (QA-007 khi có đáp án).
  2. Đọc → ghi → đọc giữ 100% dữ liệu EvalGraph (QA-003).
  3. Mỗi lệnh là 1 mark undo trong phiên Block Editor.
  4. Đạt DoD-C.

### CMD-571 · Trạng thái hiển thị: BVSTATE, BVHIDE, BVSHOW
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-555, CMD-570, DOC-023, RGN-019, INT-002, DOC-008
- **Lệnh:** BVSTATE, BVHIDE, BVSHOW
- **Màn hình:** UI-G04, UI-G01
- **Đầu vào:** 07.2 UI-G04 (Panel Visibility: chế độ hiển thị, hiện/ẩn đối tượng theo trạng thái, dropdown trạng thái, quản lý trạng thái), UI-G01 (trạng thái hiển thị khi chèn, P2); prototype `js/screens16.js` › `IMPL.BVSTATE`, hộp `bvstate`, `bvnew`; Command Reference công khai (BVSTATE, BVHIDE, BVSHOW, `BVMODE`).
- **Đầu ra:** `commands/block/authoring/vis_states.{h,cpp}`: tạo, đổi tên, xóa, sắp thứ tự trạng thái; ẩn/hiện đối tượng theo trạng thái hiện hành hoặc mọi trạng thái; `BVMODE`; tùy chọn chọn trạng thái khi chèn cho `INSERT` (CMD-542) và palette Blocks. Đặc tả `docs/specs/commands/BVSTATE.md`, `BVHIDE.md`, `BVSHOW.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 20 ca vàng **[đề xuất]** đạt 100%; block tạo ra hiện đúng từng trạng thái ở AutoCAD (QA-007 khi có đáp án).
  2. Đọc → ghi → đọc giữ 100% dữ liệu visibility (QA-003).
  3. Đạt DoD-C.

### CMD-572 · Bảng thuộc tính block và bảng tra: BTABLE
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-570, DOC-023, RGN-019, INT-002, DOC-008
- **Lệnh:** BTABLE
- **Màn hình:** UI-G05
- **Đầu vào:** 03 §11 (bảng thuộc tính block P2); 07.2 UI-G04 (action lookup, bảng thuộc tính block); prototype `js/screens16.js` › `IMPL.BTABLE`, `tblDialog` (bảng tra cứu và bảng thuộc tính block); Command Reference công khai (BTABLE).
- **Đầu ra:** `commands/block/authoring/block_tables.{h,cpp}`: bảng thuộc tính block (cột là parameter và thuộc tính, dòng là tổ hợp giá trị; ràng buộc giá trị phải khớp một dòng) và bảng tra của action lookup (tra xuôi, tra ngược); kiểm cột trùng, dòng trùng, giá trị ngoài value set. Đặc tả `docs/specs/commands/BTABLE.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 15 ca vàng **[đề xuất]** đạt 100%; khớp QA-007 khi có đáp án.
  2. Bảng 1.000 dòng: chọn một dòng cập nhật tham chiếu ≤ 100 ms **[đề xuất]**.
  3. Đọc → ghi → đọc giữ 100% (QA-003).
  4. Đạt DoD-C.

### CMD-573 · Tham số ràng buộc trong block: BCPARAMETER
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-570, GEO-024, DOC-023, INT-002, DOC-008
- **Lệnh:** BCPARAMETER
- **Màn hình:** UI-G05
- **Đầu vào:** 03 §11 (ràng buộc trong block P2), §15; 07.2 UI-G05 (tab Constraints); Command Reference công khai (BCPARAMETER); API GEO-024 (bộ giải ràng buộc 2D).
- **Đầu ra:** `commands/block/authoring/constraint_params.{h,cpp}`: tham số ràng buộc thẳng, ngang, đứng, nghiêng, góc, bán kính, đường kính trong Block Editor; giải bằng GEO-024 khi đổi giá trị; lưu tương thích AutoCAD. Đặc tả `docs/specs/commands/BCPARAMETER.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 20 ca vàng **[đề xuất]**: hình sau khi đổi giá trị thỏa mọi ràng buộc với sai số ≤ 1e-9 × kích thước block.
  2. Đọc → ghi → đọc giữ 100% (QA-003); AutoCAD mở đúng (QA-007 khi có đáp án).
  3. Đạt DoD-C.

### CMD-574 · BAUTHORPALETTE, BTESTBLOCK
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-555, CMD-558, CMD-570, DOC-018, INT-002
- **Lệnh:** BAUTHORPALETTE, BAUTHORPALETTECLOSE, BTESTBLOCK
- **Màn hình:** UI-G05, UI-G04
- **Đầu vào:** 03 §11 (BAUTHORPALETTE P2; BTESTBLOCK nằm trong nhóm Block Editor P1 của 03, còn 07.2 UI-G04 ghi Panel Thử là P2); 07.2 UI-G05 (tab Parameters, Actions, Parameter Sets, Constraints), UI-G04 (cửa sổ thử block); prototype `js/screens16.js` › `IMPL.BAUTHORPALETTE`, `IMPL.BTESTBLOCK`; Command Reference công khai.
- **Đầu ra:** `commands/block/authoring/author_palette.{h,cpp}`: dữ liệu công cụ của 4 tab (mỗi công cụ là một lời gọi `BPARAMETER`/`BACTION`/`BCPARAMETER` có sẵn tham số; parameter set là chuỗi parameter kèm action); `BTESTBLOCK`: mở bản vẽ thử tạm (DOC-018) chứa block đang sửa, dùng được grip (CMD-558), đóng không ghi gì về bản vẽ chính. Đặc tả `docs/specs/commands/BAUTHORPALETTE.md`, `BAUTHORPALETTECLOSE.md`, `BTESTBLOCK.md`; kiểm thử.
- **Tiêu chí nghiệm thu:**
  1. Mỗi công cụ của 4 tab tạo đúng đối tượng như gõ lệnh tương ứng (so DXF, QA-003).
  2. Đóng cửa sổ thử: hash snapshot bản vẽ chính và định nghĩa block không đổi.
  3. Mở cửa sổ thử với block 5.000 entity ≤ 300 ms **[đề xuất]**.
  4. Đạt DoD-C.

### CMD-575 · XLIST, BSCALE
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** S · **Phụ thuộc:** DOC-012, INT-002, INT-003, INT-004, INT-007, INT-009, INT-012, DOC-008, LSP-006
- **Lệnh:** XLIST, BSCALE
- **Màn hình:** UI-MR2
- **Đầu vào:** 03 §28 (XLIST, BSCALE P2); 07.1 §7.10 (Liệt kê đối tượng lồng, Tỉ lệ block: P2); prototype `#UI-MR2`: `js/screens21.js` › `IMPL.XLIST`, `IMPL.BSCALE`; tài liệu công khai của Express Tools (XLIST, BSCALE).
- **Đầu ra:** `commands/express/xlist_bscale.cpp`: `XLIST` chọn đối tượng lồng (INT-009) → báo loại, đường lồng block/xref, layer, màu, linetype; `BSCALE` chọn block → hệ số tỉ lệ hoặc `Reference`, có jig → đổi tỉ lệ từng block quanh điểm chèn của nó. Đặc tả `docs/specs/commands/XLIST.md`, `BSCALE.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 15 ca vàng **[đề xuất]** đạt 100%; khớp QA-007 khi có đáp án.
  2. `BSCALE`: điểm chèn mỗi block không đổi, tỉ lệ mới = cũ × hệ số (sai số ≤ 1e-12 tương đối) **[đề xuất]**; 1 mark undo.
  3. Đạt kiểm thử ba đường vào (§0).
  4. Đạt DoD-C.

### CMD-576 · BLOCKTOXREF: đổi block thành xref
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-544, CMD-581, DOC-014, INT-002, INT-003, DOC-008, QA-004
- **Lệnh:** BLOCKTOXREF
- **Màn hình:** UI-MR2
- **Đầu vào:** 03 §28 (BLOCKTOXREF P2); prototype `#UI-MR2`: `js/screens21.js` › `IMPL.BLOCKTOXREF`, hộp `b2x`; tài liệu công khai của Express Tools (BLOCKTOXREF).
- **Đầu ra:** `commands/express/blocktoxref.cpp`: chọn block → ghi định nghĩa ra DWG (CMD-544) hoặc chọn tệp có sẵn → gắn xref (`Attach`/`Overlay`, đường dẫn tương đối hoặc đầy đủ, CMD-581) thay mọi tham chiếu, giữ điểm chèn, tỉ lệ, góc; dọn định nghĩa block nếu chọn; từ chối khi block lồng trong block khác mà tên xref sẽ trùng. Đặc tả `docs/specs/commands/BLOCKTOXREF.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 10 ca vàng **[đề xuất]**: hình hiển thị trước và sau trùng (so primitive regen); tệp xref mở được bằng 3 bộ đọc (QA-004).
  2. 1 mark undo; Undo trả bản vẽ về hash cũ (tệp xref đã ghi giữ lại).
  3. Đạt DoD-C.

### CMD-577 · ATTOUT, ATTIN (Express)
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** S · **Phụ thuộc:** VN-014, NT-011, NT-012, QA-006, INT-002, INT-003, INT-008, DOC-008, LSP-006
- **Lệnh:** ATTOUT, ATTIN
- **Màn hình:** UI-MR2
- **Đầu vào:** 03 §11 (ATTOUT, ATTIN P2); 07.1 §7.10 (công cụ đã có bản đầy đủ ở tab Công cụ VN thì nút mở thẳng công cụ đó); 07.3 UI-J15 (TCATTOUT, TCATTIN); tài liệu công khai của Express Tools (ATTOUT, ATTIN) và tệp mẫu do Express Tools tạo; API VN-014.
- **Đầu ra:** `commands/express/attout_attin.cpp`: định dạng văn bản phân tách bằng tab của Express Tools (cột `HANDLE` có nháy đơn đứng trước, `BLOCKNAME`, các tag; ô `<>` cho tag không có ở block) **[đối chiếu tệp mẫu]**; `ATTOUT` (tên tệp → chọn đối tượng) và `ATTIN` (tên tệp → cập nhật theo handle, cả lần nhập là 1 mark undo) dùng engine của VN-014; fuzz target `commands/express/fuzz/attin_fuzz.cpp`. Đặc tả `docs/specs/commands/ATTOUT.md`, `ATTIN.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. Tệp do Express Tools xuất được `ATTIN` nhập đúng 100%; tệp `ATTOUT` ghi ra được Express Tools `ATTIN` nhập (QA-007 khi có đáp án).
  2. Xuất rồi nhập lại không sửa: bản vẽ không đổi (hash snapshot), 0 mark undo; dòng không tìm thấy handle được báo số lượng.
  3. Fuzz `attin_fuzz` theo DoD-C.
  4. Đạt DoD-C.

## 9. Tham chiếu ngoài, ảnh, PDF underlay

### CMD-580 · Dữ liệu palette Tham chiếu ngoài, trạng thái và nạp lại khi xem; EXTERNALREFERENCES
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-013, DOC-016, DWG-026, NT-011, NT-013, PLF-002, INT-001, INT-002
- **Lệnh:** EXTERNALREFERENCES, XREF, EXTERNALREFERENCESCLOSE
- **Màn hình:** UI-G10, UI-B06
- **Đầu vào:** 03 §12 (xref P0, xem xref và ảnh từ Viewer); 07.2 UI-G10 (cột Tên, Trạng thái: Đã nạp, Chưa nạp, Không tìm thấy, Chưa phân giải, Mồ côi, Cần nạp lại; Kích thước, Kiểu, Ngày, Đường dẫn đã lưu, Tìm thấy tại; dạng danh sách hoặc cây lồng; thông báo "Xref KT-01 đã thay đổi — [Nạp lại]"; V: xem trạng thái, nạp lại); 07.1 UI-B06 (thiếu tham chiếu khi mở); prototype `#UI-G10`: `js/screens7.js` (palette `xref`), `js/screens1.js` › `SC.missingRefs`; API DOC-013 (block record ngoài, trạng thái phân giải, lồng nhau, nạp xref chỉ đọc), NT-011 (đường dẫn tìm kiếm, theo dõi tệp).
- **Đầu ra:** `commands/xref/xref_palette_model.{h,cpp}`: danh sách và cây tham chiếu (DWG xref, ảnh, PDF underlay; DGN và DWF hiện là loại giữ nguyên chưa hỗ trợ) kèm trạng thái, kích thước, kiểu, ngày, đường dẫn đã lưu, tìm thấy tại; theo dõi tệp gốc thay đổi và phát sự kiện "Cần nạp lại"; nạp lại ở nền (NT-013, hủy được); ở Viewer chỉ đường dẫn tìm thấy trong phiên được đổi (chọn thư mục), không ghi vào bản vẽ. Lệnh `EXTERNALREFERENCES`, `XREF` (cùng mở palette), `EXTERNALREFERENCESCLOSE`. Đặc tả `docs/specs/commands/EXTERNALREFERENCES.md`, `XREF.md`, `EXTERNALREFERENCESCLOSE.md`; kiểm thử `commands/xref/tests/xref_palette_model_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 200 tệp QA-001 có xref **[đề xuất]** (thiếu, chưa phân giải, mồ côi, lồng, overlay): trạng thái trùng 100% với trạng thái AutoCAD báo (QA-007 khi có đáp án; trước đó ca vàng).
  2. Dựng model cho 200 tham chiếu ≤ 20 ms **[đề xuất]**. Tệp xref bị ghi đè: sự kiện "Cần nạp lại" ≤ 2 s **[đề xuất]**.
  3. Nạp lại xref 20 MB ≤ 1,5 s tầm trung (ngân sách mở tệp 05 §1.1), luồng UI không bị chặn quá 16 ms.
  4. Viewer: nạp lại và tìm tệp không đổi bản vẽ chủ (snapshot trùng, bộ đếm sửa đổi = 0).
  5. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** gắn, gỡ, đổi đường dẫn lưu vào bản vẽ (CMD-581, CMD-582).

### CMD-581 · Gắn xref: XATTACH, ATTACH, -XREF
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** CMD-580, DOC-013, DWG-026, NT-011, INT-002, INT-003, INT-004, INT-005, INT-007, INT-012, DOC-008, LSP-006, QA-007
- **Lệnh:** XATTACH, ATTACH, -XREF
- **Màn hình:** UI-G11
- **Đầu vào:** 03 §12 (Attach/Overlay, đường dẫn tương đối, xref lồng: P0), Phụ lục A; 07.2 UI-G11; 07.1 §7.3 (Gắn DWG/Ảnh/PDF), §9.11 (kéo DWG vào vùng vẽ giữ Ctrl thì gắn xref); prototype `#UI-G11`: `js/screens7.js` › `IMPL.XATTACH`, `IMPL.ATTACH`; Command Reference công khai (XATTACH, ATTACH, -XREF, `XREFTYPE`, `REFPATHTYPE`, `XREFLAYER`, `XLOADCTL`, `XREFNOTIFY`, `PROJECTNAME`, `XDWGFADECTL`).
- **Đầu ra:** `commands/xref/xattach.{h,cpp}`:
  - Backend hộp thoại và prompt: tệp → kiểu `Attach`/`Overlay` (mặc định `XREFTYPE`) → đường dẫn đầy đủ / tương đối / không (mặc định `REFPATHTYPE`; tương đối cần bản vẽ chủ đã lưu) → điểm chèn hoặc [`Scale`/`X`/`Y`/`Z`/`Rotate`/`PScale`/`PX`/`PY`/`PZ`/`PRotate`] → tỉ lệ → góc; đơn vị theo `INSUNITS`; layer theo `XREFLAYER`.
  - Luật: tham chiếu vòng khi Attach bị chặn; Overlay lồng không nạp; cùng tệp gắn lại dùng chung định nghĩa; tên trùng với tệp khác thì hỏi tên mới.
  - `-XREF` gồm vòng tùy chọn `?`/`Bind`/`Detach`/`Path`/`Unload`/`Reload`/`Overlay`/`Attach`; tùy chọn `Attach`, `Overlay` ở task này, các tùy chọn còn lại ở CMD-582, CMD-583.
  - `ATTACH`: chọn tệp rồi chuyển theo phần mở rộng (DWG → XATTACH, ảnh → IMAGEATTACH, PDF → PDFATTACH ở 1.0; DGN, DWF báo chưa hỗ trợ).
  - Đặc tả `docs/specs/commands/XATTACH.md`, `ATTACH.md`, `-XREF.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 40 ca vàng **[đề xuất]** (Attach, Overlay, lồng 3 mức, vòng, đường dẫn tương đối khác ổ đĩa, UNC, macOS và Linux, đơn vị khác nhau) đạt 100%; khớp 100% QA-007 khi có đáp án.
  2. Bản ghi block xref (cờ, đường dẫn) trùng từng trường với DXF tham chiếu; đường dẫn tương đối phân giải đúng sau khi chuyển cả thư mục dự án.
  3. Gắn xref 20 MB ≤ 1,5 s tầm trung (ngân sách mở tệp 05 §1.1), không chặn luồng UI quá 16 ms; xref hiện mờ theo `XDWGFADECTL`.
  4. Đạt kiểm undo và kiểm thử ba đường vào (§0).
  5. Đạt DoD-C, DoD-L.

### CMD-582 · Quản lý xref: Unload, Reload, Detach, Path; XOPEN
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-581, DOC-013, DOC-018, NT-011, INT-002, INT-003, INT-007, INT-008, DOC-008, LSP-006, QA-007
- **Lệnh:** XOPEN
- **Màn hình:** UI-G10
- **Đầu vào:** 03 §12 (nạp lại, gỡ, XOPEN, xref lồng: P0); 07.2 UI-G10 (chuột phải: Mở, Gắn, Gỡ nạp, Nạp lại, Gỡ bỏ, Đường dẫn ▸; Đổi đường dẫn: tuyệt đối, tương đối, bỏ, chọn mới, tìm và thay cho nhiều tham chiếu); prototype `#UI-G10`: `js/screens7.js` (menu `path`); Command Reference công khai (-XREF, XOPEN).
- **Đầu ra:** `commands/xref/xref_ops.{h,cpp}`: tùy chọn `?`, `Unload`, `Reload`, `Detach`, `Path` của `-XREF` (CMD-581) và các hành động palette tương ứng: Unload giữ định nghĩa, không hiện; Reload nạp lại cả xref con; Detach gỡ mọi tham chiếu và định nghĩa, xref con chỉ gỡ khi không còn ai dùng, không gỡ trực tiếp xref lồng; Path đổi đường dẫn đã lưu, tìm và thay đường dẫn hàng loạt cho xref, ảnh, PDF. `XOPEN`: chọn xref (cả lồng) → mở tệp ở tab mới (DOC-018), đã mở thì chuyển sang. Cập nhật `docs/specs/commands/-XREF.md`; đặc tả `docs/specs/commands/XOPEN.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 30 ca vàng **[đề xuất]** (chuyển trạng thái, xref lồng, Detach có con dùng chung, tìm và thay đường dẫn cho 100 tham chiếu) đạt 100%; khớp QA-007 khi có đáp án.
  2. Detach để lại 0 tham chiếu treo (AUDIT sạch); Unload rồi Reload trả hiển thị trùng trước (so primitive regen).
  3. Đạt kiểm undo và kiểm thử ba đường vào (§0) cho các tùy chọn ghi vào bản vẽ.
  4. Đạt DoD-C.

### CMD-583 · Bind xref: XBIND, -XBIND, tùy chọn Bind của -XREF
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-540, CMD-581, DOC-013, DOC-014, NT-005, INT-002, INT-003, INT-007, DOC-008, LSP-006, QA-007
- **Lệnh:** XBIND, -XBIND
- **Màn hình:** UI-G10
- **Đầu vào:** 03 §12 (bind P0), Phụ lục A; 07.2 UI-G10 (Bind ▸ Bind / Insert / Từng đối tượng phụ thuộc: block, kiểu kích thước, layer, linetype, kiểu chữ); 07.1 §7.3; prototype `js/screens7.js` › `IMPL.XBIND`; Command Reference công khai (XBIND, -XBIND, -XREF Bind, `BINDTYPE`).
- **Đầu ra:** `commands/xref/xbind.{h,cpp}`:
  - Bind cả xref (tùy chọn `Bind` của `-XREF`, menu palette): `Bind` đặt tên phụ thuộc thành `xref$0$tên` (tăng số khi trùng), `Insert` gộp tên và giữ định nghĩa có sẵn của bản vẽ chủ (`BINDTYPE`); xref lồng bind theo; xref thành block thường.
  - `XBIND`, `-XBIND` ([`Block`/`Dimstyle`/`LAyer`/`LType`/`Style`] → tên phụ thuộc): đưa từng đối tượng phụ thuộc vào bản vẽ cùng phụ thuộc kéo theo (linetype của layer, kiểu chữ và block mũi tên của kiểu kích thước).
  - Cập nhật `docs/specs/commands/-XREF.md`; đặc tả `docs/specs/commands/XBIND.md`, `-XBIND.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 30 ca vàng **[đề xuất]** (cả hai kiểu bind, xref lồng, tên trùng, phụ thuộc kéo theo) đạt 100%; khớp 100% QA-007 khi có đáp án.
  2. Sau bind: hình hiển thị trùng trước bind (so primitive regen); AUDIT sạch; không còn tham chiếu tới định nghĩa xref.
  3. Bind xref 500.000 entity ≤ 1,5 s trên 8 nhân **[đề xuất]**.
  4. Đạt kiểm undo và kiểm thử ba đường vào (§0).
  5. Đạt DoD-C, DoD-H.

### CMD-584 · Clip xref và block: XCLIP, CLIP
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-581, CMD:VPCLIP, RGN-024, DOC-005, GEO-006, INT-002, INT-003, INT-007, INT-008, INT-012, INT-013, DOC-008, LSP-006, QA-005, QA-007
- **Lệnh:** XCLIP, CLIP
- **Màn hình:** UI-G15
- **Đầu vào:** 03 §12 (clip P0), Phụ lục A; 07.2 UI-E10 (`XCLIP`/`CLIP`: New boundary [Select polyline, Polygonal, Rectangular, Invert clip], ON, OFF, Clipdepth, Delete, generate Polyline); 07.1 §9.5 (grip biên clip, mũi tên đảo vùng clip); prototype `js/screens7.js` › `IMPL.CLIP`, `IMPL.XCLIP`; Command Reference công khai (XCLIP, CLIP, `XCLIPFRAME`); DXF Reference công khai (SPATIAL_FILTER); API RGN-024 (hiển thị clip).
- **Đầu ra:** `commands/xref/xclip.{h,cpp}`:
  - `XCLIP`: chọn xref hoặc block → [`ON`/`OFF`/`Clipdepth`/`Delete`/`generate Polyline`/`New boundary`]; biên mới: `Select polyline` (cung xấp xỉ như AutoCAD, polyline tự cắt bị từ chối), `Polygonal`, `Rectangular`, `Invert clip`; đã có biên thì hỏi xóa biên cũ; `Clipdepth` lưu và giữ nguyên, hiển thị 2D bỏ qua **[đề xuất]**.
  - Bộ lọc không gian lưu tương thích AutoCAD; `XCLIPFRAME` (0 ẩn, 1 hiện và in, 2 hiện không in).
  - Grip đỉnh biên clip và mũi tên đảo đăng ký qua INT-013, có jig (INT-012).
  - `CLIP`: điều phối theo loại đối tượng chọn: xref, block → `XCLIP`; viewport → `VPCLIP`; ảnh, PDF qua bộ xử lý do CMD-586, CMD-588 đăng ký.
  - Đặc tả `docs/specs/commands/XCLIP.md`, `CLIP.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 30 ca vàng **[đề xuất]** đạt 100%; khớp QA-007 khi có đáp án.
  2. Tệp QA-001 có xref bị clip (≥ 50 tệp **[đề xuất]**): hiển thị và bản in khớp DWG TrueView, SSIM ≥ 0,98 (QA-005); đọc → ghi → đọc giữ 100% bộ lọc không gian.
  3. Kéo grip biên clip: jig ≤ 8 ms/khung (DoD-L).
  4. Đạt kiểm undo và kiểm thử ba đường vào (§0).
  5. Đạt DoD-C, DoD-L.

### CMD-585 · Gắn ảnh raster: IMAGEATTACH, IMAGE, -IMAGE
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-580, CMD-581, RGN-021, RND-015, DOC-005, NT-011, INT-002, INT-003, INT-004, INT-007, DOC-008, LSP-006, QA-007
- **Lệnh:** IMAGEATTACH, IMAGE, -IMAGE
- **Màn hình:** UI-G12
- **Đầu vào:** 03 §12 (ảnh PNG, JPG, TIFF, BMP, GIF: P0), Phụ lục A; 07.2 UI-G12 (tên, xem trước, đường dẫn, điểm chèn, tỉ lệ, góc, độ phân giải, đơn vị, kích thước pixel và theo đơn vị bản vẽ); prototype `#UI-G12`: `js/screens7.js` › `IMPL.IMAGEATTACH`; Command Reference công khai (IMAGEATTACH, IMAGE, -IMAGE); DXF Reference công khai (IMAGE, IMAGEDEF, IMAGEDEF_REACTOR); API RGN-021 (nạp ảnh bất đồng bộ), DOC-005 (dictionary, reactor).
- **Đầu ra:** `commands/xref/image_attach.{h,cpp}`:
  - `IMAGEATTACH`: tệp → đường dẫn (đầy đủ, tương đối, không; dùng chung logic với CMD-581) → điểm chèn → báo kích thước gốc theo đơn vị → tỉ lệ hoặc `Unit` → góc. Tạo IMAGEDEF trong dictionary ảnh, IMAGEDEF_REACTOR và IMAGE theo độ phân giải và đơn vị của tệp; lệnh hoàn tất khi mới đọc phần đầu tệp, giải mã điểm ảnh chạy nền (RGN-021).
  - `-IMAGE` [`?`/`Detach`/`Path`/`Reload`/`Unload`/`Attach`]; `IMAGE` mở palette Tham chiếu ngoài.
  - Đặc tả `docs/specs/commands/IMAGEATTACH.md`, `IMAGE.md`, `-IMAGE.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 30 ca vàng **[đề xuất]** (5 định dạng, có và không có DPI, đơn vị khác nhau, đường dẫn tương đối) đạt 100%; khớp QA-007 khi có đáp án.
  2. Kích thước ảnh theo đơn vị bản vẽ và các vector u, v trùng DXF tham chiếu với sai số ≤ 1e-9 × kích thước bản vẽ.
  3. Gắn ảnh 200 megapixel: lệnh hoàn tất ≤ 20 ms **[đề xuất]**, luồng UI không bị chặn quá 16 ms trong khi giải mã.
  4. Đạt kiểm undo và kiểm thử ba đường vào (§0).
  5. Đạt DoD-C, DoD-L.

### CMD-586 · Công cụ ảnh: IMAGECLIP, IMAGEADJUST, IMAGEQUALITY, IMAGEFRAME, TRANSPARENCY, ADJUST, FRAME
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-584, CMD-585, RGN-021, RND-015, INT-002, INT-003, INT-005, INT-007, INT-008, INT-013, DOC-008, LSP-006, QA-005, QA-007
- **Lệnh:** IMAGECLIP, IMAGEADJUST, IMAGEQUALITY, IMAGEFRAME, TRANSPARENCY, ADJUST, FRAME
- **Màn hình:** UI-G15
- **Đầu vào:** 03 §12 (IMAGECLIP, IMAGEADJUST, IMAGEQUALITY, IMAGEFRAME P1), §10 (TRANSPARENCY P1); 07.2 UI-G15 (Ảnh: Điều chỉnh độ sáng, tương phản, mờ; Clip tạo và bỏ; hiện ảnh, nền trong suốt; phát hành EA); 07.1 §7.3 (Khung tham chiếu: hiện / ẩn / hiện nhưng không in), §9.5 (grip 4 góc, biên clip); prototype `js/screens7.js` › `IMPL.ADJUST`, `IMPL.FRAME`, `IMPL.CLIP`; Command Reference công khai cho 7 lệnh.
- **Đầu ra:** `commands/xref/image_tools.{h,cpp}`:
  - `IMAGECLIP`: [`ON`/`OFF`/`Delete`/`New boundary`] với `Polygonal`, `Rectangular`, `Invert clip`; đăng ký bộ xử lý ảnh cho `CLIP` (CMD-584).
  - `IMAGEADJUST`: [`Contrast`/`Fade`/`Brightness`] 0–100; `ADJUST` điều phối cho ảnh (phần PDF ở CMD-588); `IMAGEQUALITY` [`High`/`Draft`]; `TRANSPARENCY` [`ON`/`OFF`] nền ảnh hai màu như AutoCAD.
  - `IMAGEFRAME` (0, 1, 2); `FRAME` đặt chung cho ảnh, PDF, xref clip (3 là hỗn hợp).
  - Grip ảnh: điểm chèn, 4 góc (đổi kích thước giữ tỉ lệ), đỉnh biên clip, mũi tên đảo (INT-013).
  - Đặc tả cho 7 lệnh trong `docs/specs/commands/`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 35 ca vàng **[đề xuất]** đạt 100%; khớp QA-007 khi có đáp án.
  2. Tệp QA-001 có ảnh clip và chỉnh sáng/tương phản (≥ 30 tệp **[đề xuất]**): hiển thị và bản in khớp DWG TrueView, SSIM ≥ 0,98 (QA-005).
  3. Kéo thanh độ sáng: khung hình kế tiếp đã đổi ≤ 16,6 ms trên ảnh 50 megapixel **[đề xuất]** (xử lý trên GPU qua RND-015, không giải mã lại).
  4. Đạt kiểm undo và kiểm thử ba đường vào (§0).
  5. Đạt DoD-C, DoD-L.

### CMD-587 · Gắn PDF underlay: PDFATTACH, -PDFATTACH
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-580, CMD-581, PLT-011, RGN-022, DOC-005, INT-002, INT-003, INT-004, INT-007, DOC-008, LSP-006, QA-007
- **Lệnh:** PDFATTACH, -PDFATTACH
- **Màn hình:** UI-G13
- **Đầu vào:** 03 §12 (PDF underlay P1); 07.2 UI-G13 (danh sách trang có thumbnail, chọn nhiều trang, đường dẫn, điểm chèn, tỉ lệ, góc, liên kết "Nhập PDF"); prototype `#UI-G13`: `js/screens10.js` › `IMPL.PDFATTACH`; Command Reference công khai (PDFATTACH, -PDFATTACH); DXF Reference công khai (PDFDEFINITION, PDFUNDERLAY); API PLT-011 (mở PDF, render ô, thumbnail trang).
- **Đầu ra:** `commands/xref/pdf_attach.{h,cpp}`: tệp → trang (một hoặc nhiều; mỗi trang một underlay) → đường dẫn → điểm chèn → tỉ lệ → góc; PDF có mật khẩu thì hỏi, không lưu mật khẩu; định nghĩa và underlay lưu tương thích AutoCAD. `-PDFATTACH`: đường dẫn → số trang hoặc `?` → điểm chèn → tỉ lệ → góc. Đặc tả `docs/specs/commands/PDFATTACH.md`, `-PDFATTACH.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 20 ca vàng **[đề xuất]** đạt 100%; khớp QA-007 khi có đáp án.
  2. Kích thước underlay theo đơn vị bản vẽ đúng khổ trang PDF với sai số ≤ 1e-9 × kích thước bản vẽ.
  3. Danh sách 200 trang: thumbnail trang đầu ≤ 300 ms, còn lại nạp nền **[đề xuất]**.
  4. Đạt kiểm undo và kiểm thử ba đường vào (§0).
  5. Đạt DoD-C, DoD-L.

### CMD-588 · Công cụ PDF underlay: PDFCLIP, PDFADJUST, PDFLAYERS, ULAYERS, UOSNAP
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-584, CMD-586, CMD-587, PLT-011, RGN-022, INT-005, INT-010, INT-013, DOC-008, QA-005, QA-007
- **Lệnh:** PDFCLIP, PDFADJUST, PDFLAYERS, ULAYERS, UOSNAP
- **Màn hình:** UI-G15
- **Đầu vào:** 03 §12 (PDFCLIP, PDFLAYERS, PDFADJUST, bắt điểm: P1); 07.2 UI-G15 (PDF underlay: tương phản, mờ, đơn sắc; Clip; hiện, bắt điểm, chỉnh màu theo nền; Layer PDF; phát hành 1.0); 07.1 §7.3 (ULAYERS, UOSNAP); prototype `js/screens10.js` › `IMPL.ULAYERS`, `IMPL.UOSNAP`, `IMPL.CLIP`; Command Reference công khai cho 5 lệnh; API PLT-011 (trích hình học và layer của PDF), INT-010 (bắt điểm vào underlay).
- **Đầu ra:** `commands/xref/pdf_tools.{h,cpp}`: `PDFCLIP` như `IMAGECLIP` và đăng ký bộ xử lý PDF cho `CLIP`; `PDFADJUST` (mờ, tương phản, đơn sắc, chỉnh màu theo nền) và phần PDF của `ADJUST`; `PDFLAYERS`, `ULAYERS` (bật/tắt layer của PDF, lưu danh sách layer tắt trong underlay); `UOSNAP` (bật/tắt bắt điểm vào hình học của underlay, cấp hình học cho INT-010); grip PDF (điểm chèn, 4 góc, biên clip); `PDFFRAME` qua `FRAME`. Đặc tả cho 5 lệnh; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 30 ca vàng **[đề xuất]** đạt 100%; khớp QA-007 khi có đáp án.
  2. Tệp QA-001 có PDF underlay (≥ 30 tệp **[đề xuất]**): hiển thị và bản in khớp DWG TrueView, SSIM ≥ 0,98 (QA-005).
  3. Bắt điểm vào underlay ≤ 1 ms tại con trỏ (DoD-L) trên PDF bản vẽ kỹ thuật A0 **[đề xuất: dữ liệu]**.
  4. Đạt kiểm undo và kiểm thử ba đường vào (§0).
  5. Đạt DoD-C, DoD-L.

### CMD-589 · XCOMPARE: so sánh xref với bản trước
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-582, CMD-610, DOC-013, NT-013, INT-002
- **Lệnh:** XCOMPARE
- **Màn hình:** UI-G10
- **Đầu vào:** 03 §12 (so sánh xref P2); 07.2 UI-G10 (chuột phải › So sánh với bản trước); 07.1 §7.3; Command Reference công khai (XCOMPARE); API engine so sánh CMD-610.
- **Đầu ra:** `commands/xref/xcompare.{h,cpp}`: giữ snapshot của xref lúc nạp gần nhất; khi tệp xref đổi, so bản cũ với bản mới bằng CMD-610 trong hệ tọa độ của bản vẽ chủ (áp ma trận chèn và clip); trả danh sách thay đổi cho lớp phủ so sánh. Đặc tả `docs/specs/commands/XCOMPARE.md`; kiểm thử.
- **Tiêu chí nghiệm thu:**
  1. Ca vàng cho xref có xoay, tỉ lệ, clip, lồng 2 mức: vùng thay đổi trùng 100% hình học kỳ vọng ở hệ tọa độ bản vẽ chủ.
  2. Không ghi bản vẽ chủ và tệp xref (hash trùng).
  3. Đạt DoD-C.

## 10. Đếm block

### CMD-600 · Engine đếm block; COUNT, COUNTLIST, COUNTCLOSE, COUNTNEXT, COUNTPREV, BCOUNT
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** DOC-009, DOC-012, NT-005, RND-013, INT-016, PLF-002, INT-002, INT-003, INT-008, INT-019, INT-020, LSP-006
- **Lệnh:** COUNT, COUNTLIST, COUNTCLOSE, COUNTNEXT, COUNTPREV, BCOUNT
- **Màn hình:** UI-G09, UI-M01
- **Đầu vào:** 03 §11 (COUNT, COUNTLIST, COUNTTABLE, BCOUNT: P1), §28; 07.1 §2 (đếm block từ Viewer Pro); 07.2 UI-G09 (phạm vi Model / layout / toàn bộ; tên, số lượng, ảnh xem trước; chế độ đếm tô sáng và ◀ ▶; cảnh báo block chồng nhau; VP: đếm, duyệt, xuất Excel); 07.3 UI-M01 (Công cụ › Đếm block); prototype `#UI-G09`: `js/screens3.js` › `countGroups`, `overlaps`, `P.count`, `IMPL.COUNT`, `TC.count.enter`; `js/screens10.js` › `IMPL.BCOUNT`; Command Reference công khai (COUNT, COUNTLIST, COUNTCLOSE, COUNTNEXT, COUNTPREV); tài liệu Express Tools (BCOUNT).
- **Đầu ra:**
  - `commands/data/count/count_engine.{h,cpp}`: đếm tham chiếu theo tên hiệu lực (gộp các `*U` của dynamic block về tên gốc) trong phạm vi Model, layout hiện hành, toàn bộ hoặc vùng chọn; tùy chọn đếm cả block lồng **[đề xuất]**; phát hiện block chồng (cùng tên, điểm chèn trùng trong dung sai, cùng góc và tỉ lệ); danh sách phần tử để duyệt có thứ tự ổn định (theo không gian, rồi trên xuống, trái sang) **[đề xuất]**; cập nhật gia tăng theo sự kiện DOC-007 khi bản vẽ đổi (Pro); bảng dữ liệu xuất (tên, mô tả, số lượng, số chồng) để UI-G09 ghi XLSX/CSV.
  - `COUNT`: một block chọn trước thì vào chế độ đếm block đó, nếu không thì mở palette; chạy từ script thì hỏi chọn block. `COUNTLIST` mở palette Đếm. `COUNTNEXT`, `COUNTPREV` duyệt và zoom tới phần tử (INT-016). `COUNTCLOSE` thoát chế độ đếm. Tô sáng bằng cờ kết quả của RND-013.
  - `commands/express/bcount.cpp`: `BCOUNT` (Express) chọn đối tượng, Enter là cả bản vẽ → bảng tên block và số lượng, kể cả block lồng như Express Tools, in ra cửa sổ văn bản.
  - Tên các lệnh phụ chốt theo Command Reference ở Giai đoạn 0. Đặc tả `docs/specs/commands/COUNT.md`, `COUNTLIST.md`, `COUNTCLOSE.md`, `COUNTNEXT.md`, `COUNTPREV.md`, `BCOUNT.md`; kiểm thử `commands/data/count/tests/count_engine_test.cpp`; benchmark `commands/data/count/bench/count_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Số đếm trùng 100% bộ đếm tuần tự tham chiếu trên 1.000 tệp QA-001; dynamic block đếm theo tên hiệu lực khớp số AutoCAD báo (QA-007 khi có đáp án).
  2. Bản vẽ 1 triệu entity có 100.000 block: đếm ≤ 50 ms trên 8 nhân **[đề xuất]**; cập nhật gia tăng sau một lệnh sửa 1.000 đối tượng ≤ 2 ms **[đề xuất]**; kết quả trùng từng bit với 1–32 luồng.
  3. Viewer Pro: hash tệp DWG và bộ đếm sửa đổi document không đổi sau mọi lệnh của task. Viewer: lệnh khóa theo 07.1 §2 (INT-020).
  4. Phát hiện block chồng đúng 100% trên ≥ 50 ca tổng hợp **[đề xuất]**.
  5. `COUNTNEXT`, `COUNTPREV` zoom tới phần tử ≤ 100 ms (DoD-L).
  6. `BCOUNT` từ bàn phím, `trion-console` và `(command ...)` cho văn bản kết quả trùng từng ký tự.
  7. Đạt DoD-C, DoD-H, DoD-L.
- **Ngoài phạm vi:** ghi tệp XLSX/CSV và giao diện palette (UI-G09); bảng đếm (CMD-601); field đếm (CMD-602).

### CMD-601 · COUNTTABLE: bảng đếm trong bản vẽ
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-600, VN-013, INT-002, INT-003, INT-004, INT-007, DOC-008, LSP-006
- **Lệnh:** COUNTTABLE
- **Màn hình:** UI-G09
- **Đầu vào:** 03 §11 (COUNTTABLE P1); 07.2 UI-G09 (Tạo bảng đếm: EA, cần quyền sửa bản vẽ); Command Reference công khai (COUNTTABLE); API tạo bảng TABLE từ lưới ô của VN-013 (dùng chung với TCEXCELPASTE).
- **Đầu ra:** `commands/data/count/count_table.cpp`: chọn phạm vi và block (mặc định toàn bộ danh sách đang đếm) → điểm chèn → bảng TABLE (cột Tên block, Số lượng; kiểu bảng hiện hành `CTABLESTYLE`); bảng tĩnh, chạy lệnh trên bảng có sẵn thì cập nhật số **[đề xuất]**; 1 mark undo. Đặc tả `docs/specs/commands/COUNTTABLE.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. Số trong bảng trùng 100% kết quả CMD-600 trên 50 tệp QA-001 **[đề xuất]**.
  2. Cập nhật bảng có sẵn giữ vị trí, kiểu bảng và thứ tự dòng.
  3. Đạt kiểm undo và kiểm thử ba đường vào (§0).
  4. Đạt DoD-C.

### CMD-602 · COUNTFIELD: field số lượng block
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-600, DOC-022, RGN-025, INT-002, DOC-008, QA-004
- **Lệnh:** COUNTFIELD
- **Màn hình:** UI-G09
- **Đầu vào:** 07.2 UI-G09 (chèn field đếm); Command Reference công khai (COUNTFIELD); API DOC-022 (đối tượng field, giá trị cache), RGN-025 (đánh giá và cập nhật field).
- **Đầu ra:** `commands/data/count/count_field.{h,cpp}`: field số lượng theo tên block và phạm vi; mã field tương thích AutoCAD nếu QA-007 xác nhận định dạng, nếu không thì field riêng kèm giá trị cache để AutoCAD vẫn hiện số **[đề xuất]**; cập nhật theo luật cập nhật field của RGN-025; lệnh `COUNTFIELD` (chọn block → chèn MTEXT chứa field, hoặc trả field cho ô chữ đang sửa). Đặc tả `docs/specs/commands/COUNTFIELD.md`; kiểm thử.
- **Tiêu chí nghiệm thu:**
  1. Sau 1.000 chỉnh sửa ngẫu nhiên **[đề xuất]**, giá trị field trùng kết quả CMD-600.
  2. Tệp lưu ra mở bằng DWG TrueView hiện đúng số từ giá trị cache (QA-004).
  3. 1 mark undo cho mỗi lần chèn.
  4. Đạt DoD-C.

## 11. So sánh bản vẽ

### CMD-610 · Engine so sánh hai bản vẽ trên snapshot song song
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** DOC-009, DOC-010, DOC-012, DOC-013, DOC-017, GEO-002, GEO-017, NT-005, NT-013, QA-009
- **Màn hình:** UI-I07
- **Đầu vào:** 03 §17 (COMPARE P1); 06 (T18: Viewer Pro có COMPARE); 07.2 UI-I07 (5 nhóm đối tượng, dung sai, so định dạng chữ; thêm / xóa / sửa; thuộc tính đổi); 07.1 §9.4 (tô sáng kết quả so sánh); prototype `#UI-I07`: `js/screens3.js` › `buildB`, `KIND`, `cmpStart`; API DOC-009 (snapshot), DOC-013 (nạp bản vẽ chỉ đọc), DOC-017 (so thuộc tính theo metadata), DOC-010 (`$FINGERPRINTGUID`), GEO-017 (BVH), GEO-002 (dung sai).
- **Đầu ra:** `commands/data/compare/compare_engine.{h,cpp}`:
  - Đầu vào: snapshot A; đường dẫn B (tệp khác hoặc `.bak`) nạp chỉ đọc ở nền; nhóm đối tượng (Hình học, Chữ, Kích thước, Hatch, Block); dung sai (> 0, mặc định 0.01); cờ so định dạng chữ.
  - Ghép cặp: A và B cùng `$FINGERPRINTGUID` thì ghép theo handle trước **[đề xuất]**; phần còn lại ghép theo khóa hình học lượng tử hóa theo dung sai (loại cộng hình học chuẩn hóa), rồi lượt gần đúng qua BVH; hòa thì theo thứ tự handle.
  - Phân loại: giống; sửa (thuộc tính khác hoặc hình học lệch vượt dung sai; danh sách thuộc tính đổi kèm giá trị trước → sau theo mã DOC-017); chỉ có ở A (xóa); chỉ có ở B (thêm). Block so theo tên hiệu lực, ma trận và attribute; định nghĩa đổi thì tham chiếu là sửa.
  - Kết quả: danh sách thay đổi (loại, đối tượng A/B, loại đối tượng, layer, hộp bao, thuộc tính đổi) sắp theo vị trí trên xuống, trái sang, tất định; tiến trình và hủy qua NT-013.
  - Kiểm thử `commands/data/compare/tests/compare_engine_test.cpp`, `compare_prop_test.cpp`; benchmark `commands/data/compare/bench/compare_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Bộ ≥ 500 ca tổng hợp **[đề xuất]** (thêm, xóa, sửa từng loại; lệch dưới và trên dung sai; đổi layer; đổi thứ tự trong cơ sở dữ liệu; đổi handle; block lồng đổi định nghĩa) phân loại đúng 100%.
  2. Tính chất (khung QA-009): so A với chính nó cho 0 thay đổi; so A với B rồi B với A thì tập "chỉ có ở A" và "chỉ có ở B" đổi chỗ, tập "sửa" bằng nhau.
  3. Hai bản vẽ 1 triệu entity: ≤ 2 s trên 8 nhân **[đề xuất]**; hiệu suất song song ≥ 60% đến 8 luồng; bộ nhớ thêm ≤ 1,5 lần dung lượng B trong RAM **[đề xuất]**; hủy ≤ 50 ms.
  4. Kết quả và thứ tự trùng từng bit với 1–32 luồng và trên 3 hệ điều hành.
  5. Không ghi A, B: hash tệp và hash snapshot trước, sau trùng nhau.
  6. Đạt DoD-C, DoD-H.

### CMD-611 · COMPARE, COMPARECLOSE: lệnh và phiên so sánh
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-610, DOC-018, PLF-002, NT-013, INT-001, INT-002, INT-003, INT-019, INT-020
- **Lệnh:** COMPARE, COMPARECLOSE
- **Màn hình:** UI-I07
- **Đầu vào:** 07.2 UI-I07; 07.1 §2 (so sánh bản vẽ từ Viewer Pro), §7.11 (tab ngữ cảnh So sánh bản vẽ); 06 (T18); Command Reference công khai (COMPARE, COMPARECLOSE).
- **Đầu ra:** `commands/data/compare/compare_cmd.cpp`: `COMPARE` mở hộp thiết lập UI-I07 qua điểm nối trình bày của INT-001; chạy từ script, `CMDDIA` = 0 hoặc `trion-console` thì hỏi tệp B hoặc bản `.bak`, nhóm, dung sai, định dạng chữ **[đề xuất]**; chạy CMD-610 ở nền rồi in "So sánh xong: N thay đổi (a thêm, b xóa, c sửa)"; `trion-console` ghi kết quả JSON cho QA **[đề xuất]**. Kết quả giữ trong phiên của document cho UI dựng lớp phủ và danh sách; chạy lại thì thay phiên cũ. `COMPARECLOSE` kết thúc, giải phóng B. Viewer: lệnh khóa (INT-020). Đặc tả `docs/specs/commands/COMPARE.md`, `COMPARECLOSE.md`; kiểm thử `commands/data/compare/tests/compare_cmd_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. `.scr` và `trion-console` cho cùng số thay đổi và cùng JSON kết quả như chạy từ UI (trùng từng byte).
  2. Viewer Pro và Pro: hash A, B không đổi; 0 mark undo; bộ đếm sửa đổi document = 0.
  3. Sau `COMPARECLOSE`, bộ nhớ trở về trong 5% so với trước `COMPARE` **[đề xuất]**.
  4. Viewer: thông báo nâng cấp theo 07.1 §2.
  5. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** lớp phủ, danh sách thay đổi, đám mây đánh dấu, báo cáo PDF/CSV/ảnh (giao diện UI-I07).

### CMD-612 · COMPAREIMPORT: nhập đối tượng đã thay đổi từ B sang A
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** S · **Phụ thuộc:** CMD-611, DOC-014, INT-002, DOC-008
- **Lệnh:** COMPAREIMPORT
- **Màn hình:** UI-I07
- **Đầu vào:** 07.2 UI-I07 (nhập đối tượng đã thay đổi từ B sang A, P2); Command Reference công khai (COMPAREIMPORT); API DOC-014 (chép sâu giữa hai bản vẽ).
- **Đầu ra:** `commands/data/compare/compare_import.cpp`: nhận các thay đổi đã chọn: chỉ có ở B thì chép sang A (named object thiếu chép qua DOC-014); sửa thì thay đối tượng A bằng bản của B; chỉ có ở A thì xóa chỉ khi có cờ rõ ràng; toàn bộ là 1 mark undo. Đặc tả `docs/specs/commands/COMPAREIMPORT.md`; kiểm thử.
- **Tiêu chí nghiệm thu:**
  1. Nhập k thay đổi rồi so lại A với B: đúng k mục biến mất khỏi danh sách, mục khác giữ nguyên.
  2. Undo trả A về hash snapshot cũ.
  3. Đạt DoD-C.

## 12. Trích xuất dữ liệu

### CMD-620 · DATAEXTRACTION, EATTEXT: trích block và attribute ra bảng hoặc Excel
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** DOC-009, DOC-012, DOC-017, VN-013, NT-005, NT-013, INT-002, INT-004, INT-008, DOC-008
- **Lệnh:** DATAEXTRACTION, EATTEXT
- **Màn hình:** UI-I06
- **Đầu vào:** 03 §17 (P1 bản rút gọn: block, attribute ra Excel; P2 đầy đủ), §11 (EATTEXT P1); 07.2 UI-I06 (bước 1–5; phát hành 1.0 rút gọn); 07.1 §7.3 (Trích xuất dữ liệu); prototype `#UI-I06`: `js/screens12.js` › `IMPL.DATAEXTRACTION`; API VN-013 (ghi XLSX, CSV; tạo bảng TABLE), DOC-017.
- **Đầu ra:** `commands/data/extract/data_extract.{h,cpp}`: nguồn (bản vẽ hiện hành hoặc vùng chọn); loại (chỉ block, không phải block, tất cả; chỉ block có attribute); thuộc tính theo nhóm từ DOC-017 (attribute, chung, hình học); tinh chỉnh: gộp dòng giống nhau kèm cột Số lượng, cột Tên, sắp xếp, lọc; đầu ra: chèn bảng (VN-013, chọn kiểu bảng) trong 1 mark undo, hoặc tệp XLSX, CSV. `EATTEXT` mở cùng trình hướng dẫn như AutoCAD. Đặc tả `docs/specs/commands/DATAEXTRACTION.md`, `EATTEXT.md`; kiểm thử `commands/data/extract/tests/data_extract_test.cpp`; benchmark `data_extract_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 30 ca vàng **[đề xuất]**: bảng và tệp XLSX, CSV trùng dữ liệu kỳ vọng từng ô.
  2. Trích 100.000 block có attribute ≤ 300 ms trên 8 nhân **[đề xuất]**, hiệu suất song song ≥ 60% đến 8 luồng; kết quả và thứ tự dòng trùng từng bit với 1–32 luồng.
  3. Dynamic block trích theo tên hiệu lực; chữ tiếng Việt giữ đúng Unicode NFC trong XLSX, CSV.
  4. Chèn bảng là 1 mark undo; xuất tệp không đổi bản vẽ (hash snapshot trùng).
  5. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** nhiều bản vẽ, tệp thiết lập `.dxe`, `-DATAEXTRACTION`, liên kết dữ liệu Excel (P2).

### CMD-621 · ATTEXT, -ATTEXT: trích attribute theo tệp mẫu
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** DOC-012, DWG-024, NT-011, NT-012, QA-006, INT-002, INT-003, INT-007, INT-008, LSP-006, QA-007
- **Lệnh:** ATTEXT, -ATTEXT
- **Đầu vào:** 03 §11 (ATTEXT P1; dùng nhiều ở VN để đánh số, lập bảng); Command Reference công khai (ATTEXT, -ATTEXT và định dạng tệp mẫu trích attribute).
- **Đầu ra:** `commands/data/attext/attext.{h,cpp}`: `-ATTEXT` [`Cdf`/`Sdf`/`Dxf`/`Objects`] → tệp mẫu → tệp kết quả; `ATTEXT` (hộp thoại cổ điển); parser tệp mẫu (trường `BL:NAME`, `BL:LEVEL`, `BL:X`, `BL:Y`, `BL:Z`, `BL:NUMBER`, `BL:HANDLE`, `BL:LAYER`, `BL:ORIENT`, `BL:XSCALE`, `BL:YSCALE`, `BL:ZSCALE`, `BL:XEXTRUDE`…, tag; định dạng `Cwww000`, `Nwwwddd`; `C:QUOTE`, `C:DELIM`); xuất CDF, SDF và DXF (DXX qua DWG-024); fuzz target `commands/data/fuzz/attext_template_fuzz.cpp`. Đặc tả `docs/specs/commands/ATTEXT.md`, `-ATTEXT.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. ≥ 25 ca vàng **[đề xuất]** trùng từng byte với tệp do AutoCAD xuất (QA-007 khi có đáp án).
  2. Tệp mẫu sai báo đúng dòng; fuzz `attext_template_fuzz` theo DoD-C.
  3. Trích 100.000 block ≤ 500 ms **[đề xuất]**.
  4. Đạt kiểm thử ba đường vào (§0).
  5. Đạt DoD-C.

### CMD-622 · DATAEXTRACTION đầy đủ: nhiều bản vẽ, tệp thiết lập `.dxe`, -DATAEXTRACTION
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-620, INT-019, DOC-018, NT-005, NT-011, NT-013
- **Lệnh:** -DATAEXTRACTION
- **Màn hình:** UI-I06
- **Đầu vào:** 03 §17 (DATAEXTRACTION P2 đầy đủ), 03 Phụ lục B (`.dxe` P2); 07.2 UI-I06 (nguồn nhiều bản vẽ và thư mục, kiểu bảng, lưu thiết lập); engine CMD-620; mở không giao diện qua INT-019.
- **Đầu ra:** `commands/data/extract/extract_multi.{h,cpp}`: nguồn là danh sách bản vẽ hoặc thư mục (có thư mục con), mở chỉ đọc ở nền, gộp dòng theo khóa kèm cột Tên tệp; đọc/ghi `.dxe` (JSON nội bộ, tên trường như AutoCAD) **[đề xuất]**; `-DATAEXTRACTION` chạy theo `.dxe` cho `.scr`; đặc tả `docs/specs/commands/-DATAEXTRACTION.md`; kiểm thử `commands/data/extract/tests/extract_multi_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. 200 bản vẽ, mỗi bản 5.000 block có attribute: trích ≤ 20 s trên 8 nhân tầm trung **[đề xuất]**, hiệu suất song song ≥ 60% đến 8 luồng; kết quả trùng từng ô với chạy tuần tự.
  2. Bản vẽ không mở được bị bỏ qua và ghi vào báo cáo; không dừng cả lô.
  3. Lưu `.dxe` rồi chạy `-DATAEXTRACTION` từ `.scr` cho tệp XLSX trùng từng byte với lần chạy bằng trình hướng dẫn.
  4. Bản vẽ nguồn không đổi (hash tệp trước và sau).
  5. Đạt DoD-C, DoD-H.

## Tóm tắt theo mốc

| Mốc | Task | Số task |
|---|---|---|
| V | CMD-501, CMD-502, CMD-503, CMD-510, CMD-511, CMD-580 | 6 |
| VP | CMD-504, CMD-600, CMD-610, CMD-611 | 4 |
| EA | CMD-505, CMD-506, CMD-507, CMD-508, CMD-512, CMD-513, CMD-514, CMD-515, CMD-520, CMD-521, CMD-540, CMD-541, CMD-542, CMD-543, CMD-544, CMD-545, CMD-546, CMD-547, CMD-550, CMD-581, CMD-582, CMD-583, CMD-584, CMD-585, CMD-586, CMD-601 | 26 |
| 1.0 | CMD-516, CMD-522, CMD-523, CMD-524, CMD-525, CMD-526, CMD-527, CMD-528, CMD-551, CMD-552, CMD-553, CMD-554, CMD-555, CMD-556, CMD-557, CMD-558, CMD-559, CMD-560, CMD-561, CMD-565, CMD-566, CMD-567, CMD-568, CMD-569, CMD-587, CMD-588, CMD-602, CMD-620, CMD-621 | 29 |
| Sau | CMD-530, CMD-531, CMD-532, CMD-533, CMD-570, CMD-571, CMD-572, CMD-573, CMD-574, CMD-575, CMD-576, CMD-577, CMD-589, CMD-612, CMD-622 | 15 |
| **Tổng** | | **80** |
