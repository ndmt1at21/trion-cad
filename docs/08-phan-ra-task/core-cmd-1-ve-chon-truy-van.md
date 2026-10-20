# CMD-1 · Lệnh vẽ, hatch, chọn, hiển thị và truy vấn

> Phạm vi: lệnh vẽ 2D, hatch và đường biên, region, chọn và nhóm, hiển thị, truy vấn và đo; ràng buộc tham số và làm sạch hình học (P2, sau v1.0); các lệnh vẽ P2 của tab Mở rộng được giao cho tệp này. Mã `CMD-001`…`CMD-149`.
> Thư mục mã (README §4.1): lệnh vẽ, hatch, region, ràng buộc, làm sạch hình học ở `commands/draw/`; chọn và nhóm ở `commands/select/`; truy vấn, đo, sức khỏe bản vẽ ở `commands/inquiry/`; hiển thị ở `commands/display/`. Bộ kiểm thử chung của module ở `commands/draw/testkit/`. Đặc tả hành vi: `docs/specs/commands/<NAME>.md`.
> Căn cứ: [03](../03-chuc-nang-autocad.md) §4, §5, §15–§18, §21, §28, Phụ lục A–B; [05](../05-kien-truc-ky-thuat.md) §1, §7, §10, §14; [07.1](../07-dac-ta-ui-1-khung-ung-dung.md) §7.2, §7.5, §9.8; [07.2](../07-dac-ta-ui-2-bang-va-hop-thoai.md) UI-E01–E10, UI-D06, UI-D07, UI-D12, UI-I01–I03, UI-I08; prototype `design/prototype/js/draw.js`, `screens6.js`, `screens13.js`, `screens15.js`, `screens18.js`.

## 0. Ghi chú thiết kế

- **Khung lệnh.** Mỗi lệnh là coroutine trên INT-002. Prompt, chip tùy chọn và từ khóa theo bảng 07.2 UI-E10 qua INT-003: nhãn tiếng Việt kèm chữ tắt tiếng Anh (ví dụ `[Bán kính · R]`); gõ chữ tắt tiếng Anh luôn có tác dụng. Tọa độ và giá trị qua INT-004, sysvar qua INT-005.
- **Một lần chạy lệnh = một transaction DOC-007 = một mark undo DOC-008.** Tùy chọn Hoàn tác bên trong lệnh lùi một bước con trong transaction, không sinh mark mới.
- **Hình học chỉ gọi kernel GEO** (dựng hình GEO-020, giao điểm GEO-010, vùng kín GEO-013/GEO-014, diện tích GEO-021…). So sánh số thực qua `Tolerance` (GEO-002). Lệnh không viết lại thuật toán đã có ở GEO.
- **Ngân sách chung.** Xem trước dùng jig INT-012 trên overlay, không ghi DB: ≤ 8 ms/khung (máy tầm trung), ≤ 16 ms (máy thấp). Hoàn tất lệnh trên đối tượng thường ≤ 20 ms / ≤ 50 ms (DoD-L, 03 Phụ lục B).
- **Đặc tả lệnh** `docs/specs/commands/<NAME>.md` (05 §10) có mục cố định: prompt tiếng Việt và English, tùy chọn, sysvar (mặc định, phạm vi lưu), Enter/Esc, PICKFIRST, cổng phiên bản, quy tắc hình học, trường hợp biên, khác biệt có chủ đích với AutoCAD (ghi **[đề xuất]**), điểm **[cần xác minh]** chờ đáp án QA-007.
- **KTL: bộ kiểm thử lệnh chuẩn** (công cụ ở CMD-001, CMD-002). "Đạt KTL trên N ca vàng" nghĩa là trên mọi ca đó:
  1. bàn phím mô phỏng (INT-002), `.scr` chạy bằng `trion-console` (INT-019) và `(command ...)` (LSP-006) cho bản vẽ có hash dump thuộc tính (DOC-017) trùng 100%;
  2. lệnh sửa bản vẽ: `U` đưa hash về trước lệnh, `REDO` về sau lệnh, đúng một mark; lệnh chỉ đọc: hash không đổi;
  3. Esc ở từng prompt để lại bản vẽ đúng trạng thái ghi trong đặc tả;
  4. lệnh có prompt "Chọn đối tượng": PICKFIRST = 1 và có chọn trước thì bỏ prompt chọn; PICKFIRST = 0 thì bỏ qua vùng chọn sẵn;
  5. chữ tắt tiếng Anh, tiền tố `_` và `.` chạy ở cả prompt tiếng Việt lẫn English;
  6. cổng phiên bản đúng INT-020;
  7. chạy lại trên bản vẽ dịch +10⁹ đơn vị (tọa độ VN-2000) cho kết quả dịch tương ứng trong dung sai GEO-002;
  8. khi QA-007 có đáp án cho lệnh: khớp 100% (sai số ≤ 1e-9 × kích thước bản vẽ, tô-pô trùng; 05 §1.2).
- **Tách lệnh lớn.** Lệnh chia nhiều task thì chỉ task nghiệm thu cuối mang dòng **Lệnh:**, để `CMD:TÊN` trỏ đúng một task; task con ghi tên lệnh trong tiêu đề **[đề xuất]**.
- **Tất định.** Entity tạo ra theo khóa ổn định (tham số dọc đường cong, diện tích, tọa độ), không theo thứ tự hoàn thành của luồng.
- **Cổng phiên bản.** Viewer chạy được DIST, AREA, ID, LIST, MEASUREGEOM, TEXTSCR, GRAPHSCR, REGEN, REGENALL, REDRAW, REDRAWALL, VIEW/-VIEW (chỉ khôi phục), VSCURRENT (chỉ trong phiên); thêm STATUS, TIME, DBLIST, MASSPROP **[đề xuất]**. Lệnh khác ở Viewer báo theo 07.1 §2.
- **Giao diện ở tệp UI.** Hộp thoại, tab ngữ cảnh, palette (UI-E01, E03, E05–E09, D06, D07, D12, I01–I03, I08, UI-PAR, UI-MR2) thuộc các tệp `ui-*`. Task ở đây cung cấp engine, lệnh dòng lệnh và API đọc. UI đổi thiết lập bằng sysvar hoặc từ khóa của lệnh, không ghi DB trực tiếp (DoD-U mục 5) **[đề xuất]**.

## 1. Nền chung của module

### CMD-001 · Bộ kiểm thử lệnh chuẩn (KTL): ba đường nhập, hash bản vẽ, đo thời gian
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-002, INT-008, INT-019, INT-020, LSP-006, DOC-009, DOC-017, NT-003
- **Đầu vào:** §0 (KTL); README §3 (DoD-L), §3.1 (máy tham chiếu); 05 §14; API INT-002 (đẩy sự kiện nhập), INT-019 (chạy `.scr` không giao diện), LSP-006 (`command`), DOC-009 (snapshot), DOC-017 (đọc thuộc tính chung), INT-008 (PICKFIRST, tập Previous), INT-012 (móc đo thời gian khung jig); định dạng kết quả của NT-003.
- **Đầu ra:**
  - `commands/draw/testkit/`: định dạng ca vàng `*.tcase.json` (bản vẽ đầu vào DXF hoặc bộ sinh, view cố định, sysvar, chuỗi nhập gồm điểm, từ khóa, giá trị, chọn bằng điểm hoặc khung, Esc; kết quả kỳ vọng kèm dung sai); bộ sinh `.scr` và `.lsp` từ ca vàng; `canonicalDump(snapshot)` và `dumpHash`; `runThreeWays()`; tiêm Esc ở từng prompt; kiểm PICKFIRST; đo thời gian hoàn tất lệnh và thời gian khung jig, xuất JSON cho NT-003.
  - Lệnh CTest `ktl_run <thư mục ca vàng>` dùng cho `commands/draw`, `commands/select`, `commands/inquiry`, `commands/display`.
  - Hướng dẫn viết ca vàng `commands/draw/testkit/README.md`.
- **Tiêu chí nghiệm thu:**
  1. Lệnh thử `_KTLDEMO` đăng ký trong test: 50 ca vàng mẫu cho cùng hash qua cả ba đường; 100 ca cố ý làm lệch một đường đều bị báo (100%), kèm prompt đầu tiên bị lệch.
  2. `dumpHash` ổn định trên 3 hệ điều hành và 1/4/16 luồng; 10.000 đột biến một thuộc tính cho 10.000 hash khác nhau.
  3. Dump bỏ qua trường không tất định (TDUPDATE, đường dẫn tạm); danh sách trường bị bỏ ghi trong README của testkit.
  4. Tiêm Esc ở từng prompt và kiểm PICKFIRST = 0/1 chạy tự động từ một ca vàng, không phải viết thêm mã cho từng lệnh.
  5. Đo ≥ 30 lần chạy mỗi ca, xuất p50/p99; p99 của hai lần đo liên tiếp trên cùng máy lệch ≤ 5% **[đề xuất]**.
  6. 1.000 ca vàng chạy xong ≤ 120 s trên runner CI (song song theo ca) **[đề xuất]**.
  7. Đạt DoD-C.

### CMD-002 · KTL cho lệnh sửa bản vẽ: một mark undo, Hoàn tác trong lệnh, SCRIPT
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-001, DOC-008, INT-007
- **Đầu vào:** §0 (KTL mục 2); API DOC-008 (nhật ký mark), INT-007 (lệnh SCRIPT trong ứng dụng).
- **Đầu ra:** mở rộng `commands/draw/testkit/`: kiểm `U`/`REDO` theo hash; đếm số mark DOC-008 thêm sau mỗi lần chạy; kiểm tùy chọn Hoàn tác bên trong lệnh; thêm đường chạy `.scr` qua lệnh SCRIPT (INT-007) bên cạnh `trion-console`.
- **Tiêu chí nghiệm thu:**
  1. 50 ca của `_KTLDEMO`: mỗi lần chạy thêm đúng 1 mark; `U` trả hash trước lệnh, `REDO` trả hash sau lệnh, 100%.
  2. Lệnh giả lập tạo 0 hoặc 2 mark bị phát hiện 100%.
  3. `.scr` qua SCRIPT và qua `trion-console` cho hash trùng trên 50 ca.
  4. Đạt DoD-C.

### CMD-003 · Khung lệnh vẽ: tạo entity theo thuộc tính hiện hành, OCS, trạng thái nối tiếp
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-007, DOC-021, INT-003, INT-004, INT-005, INT-012, INT-014, INT-015
- **Đầu vào:** 03 §10 (ByLayer, ByBlock); sysvar CLAYER, CECOLOR, CELTYPE, CELWEIGHT, CETRANSPARENCY, CELTSCALE, CPLOTSTYLE, THICKNESS, ELEVATION, CANNOSCALE, LASTPOINT, LASTANGLE; DXF Reference của Autodesk (tài liệu công khai: thuật toán trục tùy ý của OCS); API INT-003, INT-004 (prompt, nhập), INT-012 (jig), INT-014 (ô nhập kích thước DYN), INT-015 (UCS); prototype `draw.js` (`ED.make`, `ED.run`, `ED.mark`), `screens6.js` (`ED.override`).
- **Đầu ra:**
  - `commands/draw/common/draw_command.{h,cpp}`: lớp nền `DrawCommand`: prompt điểm, khoảng cách, góc, số, từ khóa có jig và ô DYN; một transaction cho một lần chạy; bước con cho Hoàn tác trong lệnh.
  - `commands/draw/common/entity_factory.{h,cpp}`: `EntityFactory` gán thuộc tính hiện hành, OCS và cao độ theo UCS, ngữ cảnh tỉ lệ chú thích theo CANNOSCALE cho đối tượng annotative; `PropertyOverrideScope` (cho ADDSELECTED và lệnh tạo theo mẫu).
  - `commands/draw/common/last_drawn.{h,cpp}`: đối tượng vừa vẽ, điểm cuối, tiếp tuyến cuối (LINE, ARC, PLINE nối tiếp); cập nhật LASTPOINT, LASTANGLE.
  - Kiểm thử `commands/draw/common/tests/`.
- **Tiêu chí nghiệm thu:**
  1. Entity mới mang đúng layer, màu, linetype, lineweight, transparency, tỉ lệ linetype, plot style, độ dày, cao độ hiện hành: 100% trên 60 tổ hợp (ByLayer, ByBlock, ACI, true color, transparency ByLayer).
  2. Vẽ trong UCS xoay và UCS 3D: pháp tuyến entity = trục Z của UCS; đổi WCS ↔ OCS khớp thuật toán trục tùy ý trên 10.000 UCS ngẫu nhiên, sai số ≤ 1e-12 tương đối.
  3. `PropertyOverrideScope` trả lại mọi thuộc tính hiện hành khi lệnh kết thúc, bị Esc hoặc lỗi: 100% trên 200 ca.
  4. Hoàn tác trong lệnh lùi đúng một bước con, không thêm mark DOC-008 (kiểm bằng nhật ký).
  5. Tạo 100.000 entity qua `EntityFactory` chậm hơn gọi thẳng DOC-007 không quá 10% **[đề xuất]**; không cấp phát trong vòng lặp.
  6. Đạt DoD-C, DoD-L.

### CMD-004 · Dò vùng kín tại điểm chọn trên bản vẽ lớn
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** GEO-010, GEO-014, INT-009, DOC-009, DOC-012, RGN-007, RGN-009, NT-005, QA-009, QA-012
- **Đầu vào:** 03 §5 (dòng hatch: nhận diện biên khi pick point là phần khó); 05 §7 (DCEL cạnh cong thật), §9.2 (tô-pô hatch và biên chạy song song); 07.2 UI-E01, UI-E03, UI-I01; API GEO-014 (mặt và island trên DCEL của GEO-013), GEO-010 (giao hàng loạt), INT-009 (BVH trên snapshot), DOC-012 (duyệt block lồng); RGN-007, RGN-009 (khung chữ TEXT, MTEXT, ATTRIB); cảnh BENCH-STD (QA-012); prototype `screens6.js` (`regionAt`).
- **Đầu ra:**
  - `commands/draw/boundary/boundary_set.{h,cpp}`: tập biên (khung nhìn của viewport hiện hành hoặc tập chọn); đường cong lấy từ line, cung, đường tròn, ellipse, polyline (cả có bề rộng), spline, region, solid, trace, wipeout, mline, xline và ray (cắt theo phạm vi); block, xref lồng nhau theo ma trận (cả tỉ lệ không đều); khung chữ TEXT, MTEXT, ATTRIB làm island; bỏ layer đóng băng, layer tắt theo đặc tả **[cần xác minh]**.
  - `commands/draw/boundary/region_finder.{h,cpp}`: `RegionFinder::findAt(point, Options) → RegionResult | OpenRegion`: tia từ điểm chọn tới đường cong gần nhất; hộp tìm mở rộng theo cấp nhân đôi đến khi mặt chứa điểm đóng và nằm trọn trong hộp; island Normal, Outer, Ignore (HPISLANDDETECTION) và tắt nhận diện island; mỗi cạnh của vòng kết quả mang nguồn gốc (entity, đoạn con) cho hatch liên kết.
  - Benchmark `commands/draw/boundary/bench/` kèm bộ sinh tất định mặt bằng 100.000 đường cong khi BENCH-STD chưa có cảnh tương đương.
- **Tiêu chí nghiệm thu:**
  1. 2.000 cảnh vàng (island lồng 5 cấp, island chạm biên, tiếp xúc, cung, ellipse, spline, polyline có bề rộng, block lồng 3 cấp tỉ lệ không đều, chữ trong vùng): tô-pô đúng 100%, tọa độ sai ≤ 1e-9 × kích thước bản vẽ.
  2. Property-based QA-009, 100.000 ca mỗi PR và 1 triệu ca mỗi đêm: điểm chọn nằm trong vòng ngoài và ngoài mọi island; diện tích bằng tích phân độc lập ± 1e-9 tương đối; mọi vòng kín, không tự cắt, vòng ngoài ngược chiều kim đồng hồ.
  3. Tìm cục bộ cho kết quả trùng từng bit với dựng arrangement trên toàn tập biên, trên 10.000 cảnh.
  4. Cảnh 100.000 đường cong, chỉ điểm trong phòng có ≤ 2.000 đường cong lân cận: ≤ 30 ms p99 (tầm trung), ≤ 80 ms (thấp) **[đề xuất]**. Vùng phủ cả 100.000 đường cong: ≤ 1,5 s (tầm trung, 8 luồng) **[đề xuất]**; quá 0,5 s có tiến trình, hủy ≤ 50 ms.
  5. Tập ứng viên ≥ 10.000 đường cong chạy song song qua NT-005, hiệu suất ≥ 60% đến 8 luồng; kết quả tất định từ 1 đến 32 luồng.
  6. Đạt DoD-C, DoD-H, DoD-L.
- **Ngoài phạm vi:** dung sai khe hở, chẩn đoán vùng hở, cache xem trước (CMD-005).

### CMD-005 · Dung sai khe hở, chẩn đoán vùng hở, cache xem trước khi rê chuột
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-004, DOC-007
- **Đầu vào:** 07.2 UI-E01 (dung sai khe hở; vùng hở chỉ bằng vòng tròn đỏ kèm gợi ý tăng dung sai; xem trước vùng tô khi rê chuột); 07.1 §9.8; sysvar HPGAPTOL (0–5000), HPQUICKPREVIEW, HPQUICKPREVTIMEOUT; sự kiện thay đổi DOC-007; prototype `screens6.js` (`gapHint`).
- **Đầu ra:**
  - `commands/draw/boundary/gap_bridge.{h,cpp}`: đoạn nối ảo giữa hai đầu mút hở, hoặc từ đầu mút tới đường cong, trong dung sai; chỉ dùng khi vùng chưa kín.
  - `commands/draw/boundary/open_region.{h,cpp}`: vị trí và độ rộng khe gần điểm chọn, dung sai tối thiểu để đóng vùng.
  - `commands/draw/boundary/preview_cache.{h,cpp}`: DCEL vùng lân cận giữ trong một lần chạy lệnh; định vị điểm; tính vùng mới ở nền với ưu tiên tương tác, hủy khi chuột rời; vô hiệu theo sự kiện DOC-007.
- **Tiêu chí nghiệm thu:**
  1. 500 cảnh vàng: khe ≤ HPGAPTOL được đóng bằng đoạn nối ngắn nhất, khe > HPGAPTOL không đóng (100%); đoạn nối không cắt hình học khác (10.000 cảnh ngẫu nhiên).
  2. 1.000 mặt bằng sinh có đúng một khe: chỉ đúng khe ≥ 95% **[đề xuất]**; dung sai gợi ý đóng được vùng 100%.
  3. Rê chuột trong vùng đã có trong cache ≤ 1 ms/truy vấn; sang vùng mới trên cảnh 100.000 đường cong: có xem trước ≤ 50 ms sau khi chuột dừng **[đề xuất]**; khung hình không chờ việc tính (≤ 8 ms/khung tầm trung, ≤ 16 ms thấp).
  4. 10.000 chuỗi sửa và rê ngẫu nhiên: 0 lần hiện xem trước cũ sau khi bản vẽ đổi.
  5. HPQUICKPREVIEW = 0 tắt xem trước; việc tính vượt HPQUICKPREVTIMEOUT thì dừng và không hiện xem trước.
  6. Đạt DoD-C, DoD-H, DoD-L.

## 2. Đường thẳng, đa tuyến, hình chữ nhật, đa giác

### CMD-006 · LINE
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-002, CMD-003
- **Lệnh:** LINE
- **Đầu vào:** 03 §5, Phụ lục A, B; 07.2 UI-E10 (dòng LINE); 07.1 §8 (Enter/Space, nhập tọa độ), §9.8; 05 §5.3 (luồng lệnh LINE); prototype `draw.js` (`IMPL.LINE`).
- **Đầu ra:** `commands/draw/line/` (`line_command.{h,cpp}`, `tests/golden/`, `bench/`); `docs/specs/commands/LINE.md`.
- **Tiêu chí nghiệm thu:**
  1. "Điểm đầu" → "Điểm tiếp theo" `[Hoàn tác · U]`, từ đoạn thứ hai thêm `[Đóng · C]`. Enter ở prompt đầu nối tiếp từ điểm cuối của line hoặc cung vừa vẽ; sau cung thì hỏi độ dài và đi theo tiếp tuyến (lệch hướng ≤ 1e-12 rad).
  2. Mỗi đoạn là một LINE tạo ngay khi chỉ điểm; Hoàn tác bỏ đoạn cuối; Esc giữ các đoạn đã vẽ; `U` sau lệnh xóa mọi đoạn của lần chạy.
  3. Cập nhật LASTPOINT; tôn trọng THICKNESS, ELEVATION, UCS (qua CMD-003).
  4. Rubber-band (jig INT-012) ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp) trên cảnh BENCH-STD 1 triệu entity. Mỗi đoạn: nhận điểm → ghi DB ≤ 1 ms p99 **[đề xuất]**; nhận điểm → đoạn hiện trên vùng vẽ ≤ 20 ms (tầm trung), ≤ 50 ms (thấp).
  5. Đạt KTL (§0) trên ≥ 40 ca vàng: Đóng, Hoàn tác giữa chừng, nối tiếp sau cung, `@`, `<`, nhập khoảng cách trực tiếp, ORTHO, POLAR.
  6. Đạt DoD-C, DoD-L.

### CMD-007 · XLINE, RAY
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-002, CMD-003, GEO-005
- **Lệnh:** XLINE, RAY
- **Đầu vào:** 03 §5 (XLINE P0, RAY P1, gộp vì chung mã); 07.2 UI-E10 (dòng XLINE); sysvar ANGBASE, ANGDIR; prototype `draw.js` (`IMPL.XLINE`, `IMPL.RAY`).
- **Đầu ra:** `commands/draw/xline/` (`xline_command.{h,cpp}`, `ray_command.{h,cpp}`, `tests/golden/`); `docs/specs/commands/XLINE.md`, `RAY.md`.
- **Tiêu chí nghiệm thu:**
  1. XLINE: "Chỉ định một điểm" `[Ngang · H / Dọc · V / Góc · A / Phân giác · B / Song song · O]`. Góc có Tham chiếu (chọn đối tượng thẳng, góc tương đối); Phân giác: đỉnh, điểm đầu, rồi lặp điểm cuối; Song song: khoảng cách hoặc Đi qua, chọn đối tượng thẳng, chỉ phía. Mỗi điểm đi qua tạo ngay một XLINE, lặp đến Enter.
  2. RAY: điểm đầu → điểm đi qua, lặp đến Enter.
  3. Bất biến trên 10.000 ca ngẫu nhiên có seed: hướng là vector đơn vị, sai ≤ 1e-15; Phân giác lệch góc hai bên ≤ 1e-12 rad; Song song cách đúng d ± 1e-9 tương đối; Góc tôn trọng ANGBASE, ANGDIR và UCS.
  4. Xem trước (jig INT-012) ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp); tạo mỗi đường ≤ 1 ms p99 **[đề xuất]**.
  5. Đạt KTL (§0) trên ≥ 40 ca vàng.
  6. Đạt DoD-C, DoD-L.

### CMD-008 · PLINE (phần 1): đoạn thẳng, bề rộng, Độ dài, Đóng, Hoàn tác
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-003, GEO-006
- **Đầu vào:** 03 §5; 07.2 UI-E10 (dòng PLINE); sysvar PLINEWID, PLINETYPE, PLINEGEN, FILLMODE; DXF Reference (LWPOLYLINE, POLYLINE, VERTEX, SEQEND); prototype `draw.js` (`IMPL.PLINE`).
- **Đầu ra:** `commands/draw/pline/` (`pline_command.{h,cpp}`: máy trạng thái prompt, chế độ đoạn thẳng; `pline_builder.{h,cpp}`: dựng LWPOLYLINE hoặc POLYLINE 2D theo PLINETYPE, bề rộng đầu/cuối từng đoạn, cờ bề rộng không đổi, PLINEGEN; `tests/golden/`); phần chế độ đoạn thẳng của `docs/specs/commands/PLINE.md`.
- **Tiêu chí nghiệm thu:**
  1. "Điểm đầu", báo bề rộng hiện tại, "Điểm tiếp theo" `[Cung · A / Nửa bề rộng · H / Độ dài · L / Hoàn tác · U / Bề rộng · W]`, từ đoạn thứ hai thêm `[Đóng · C]`.
  2. Bề rộng và Nửa bề rộng hỏi đầu rồi cuối (mặc định cuối = đầu), cập nhật PLINEWID; Độ dài đi theo hướng đoạn trước hoặc tiếp tuyến cung trước; Hoàn tác bỏ đoạn cuối.
  3. PLINETYPE = 0 tạo POLYLINE 2D (VERTEX, SEQEND), 1 và 2 tạo LWPOLYLINE; PLINEGEN đặt cờ sinh linetype; Esc và Enter giữ polyline khi có ≥ 2 đỉnh.
  4. Bất biến trên 10.000 chuỗi ngẫu nhiên: số đỉnh, tọa độ, bề rộng từng đoạn đúng; hai đỉnh liên tiếp trùng nhau xử lý theo đặc tả **[cần xác minh]**.
  5. Xem trước đoạn đang vẽ kèm bề rộng (jig INT-012) ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp); kết thúc lệnh với polyline 10.000 đỉnh ≤ 20 ms (tầm trung), ≤ 50 ms (thấp).
  6. Đạt KTL (§0) trên ≥ 40 ca vàng.
  7. Đạt DoD-C, DoD-L.

### CMD-009 · PLINE (phần 2): chế độ cung và nghiệm thu lệnh
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-008, GEO-020
- **Lệnh:** PLINE
- **Đầu vào:** 07.2 UI-E10 (chế độ cung: Angle, CEnter, Direction, Line, Radius, Second pt); AutoCAD Command Reference (tài liệu trợ giúp công khai của Autodesk, chỉ để đối chiếu hành vi, không chép văn bản); API GEO-020 (dựng cung); prototype `draw.js` (`bulge3`, `bulgeTo`, `tangent`).
- **Đầu ra:** chế độ cung trong `commands/draw/pline/pline_command.cpp`; ca vàng `commands/draw/pline/tests/golden/`; `docs/specs/commands/PLINE.md` hoàn chỉnh.
- **Tiêu chí nghiệm thu:**
  1. "Điểm cuối của cung" `[Góc · A / Tâm · CE / Đóng · CL / Hướng · D / Nửa bề rộng · H / Đoạn thẳng · L / Bán kính · R / Điểm thứ hai · S / Hoàn tác · U / Bề rộng · W]` và prompt con: Góc → góc bao → điểm cuối `[Tâm / Bán kính]`; Tâm → tâm → điểm cuối `[Góc / Độ dài dây cung]`; Bán kính → bán kính → điểm cuối `[Góc]`; Hướng → hướng tiếp tuyến → điểm cuối; Điểm thứ hai → điểm thứ hai → điểm cuối.
  2. Cung nối tiếp mặc định tiếp xúc đoạn trước (lệch hướng ≤ 1e-12 rad); giữ Ctrl đổi chiều cung; góc âm vẽ thuận chiều kim đồng hồ; hướng của cung đầu tiên lấy theo đối tượng vẽ trước **[cần xác minh]**.
  3. Bất biến trên 10.000 ca ngẫu nhiên: bulge = tan(θ/4) với θ là góc bao; điểm cuối đúng điểm chỉ định ± 1e-9 tương đối; Đóng trong chế độ cung tạo cung tiếp xúc đoạn cuối.
  4. Xem trước cung (jig INT-012) ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp).
  5. Đạt KTL (§0) trên ≥ 80 ca vàng: mọi tùy chọn, chuyển qua lại đoạn thẳng và cung, bề rộng thay đổi trên cung.
  6. Đạt DoD-C, DoD-L.

### CMD-010 · RECTANG
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-003, GEO-006
- **Lệnh:** RECTANG
- **Đầu vào:** 03 §5; 07.2 UI-E10 (dòng RECTANG); AutoCAD Command Reference (chỉ đối chiếu hành vi); prototype `draw.js` (`IMPL.RECTANG`, `plCorner`).
- **Đầu ra:** `commands/draw/rectang/` (`rectang_command.{h,cpp}`, `tests/golden/`); `docs/specs/commands/RECTANG.md`.
- **Tiêu chí nghiệm thu:**
  1. "Góc thứ nhất" `[Vát góc · C / Cao độ · E / Bo góc · F / Độ dày · T / Bề rộng · W]` → "Góc đối" `[Diện tích · A / Kích thước · D / Xoay · R]`. Chế độ giữ trong phiên, báo "Chế độ hiện tại" khi mở lệnh.
  2. Diện tích: nhập diện tích, chọn tính theo Chiều dài hoặc Chiều rộng; diện tích tính cả phần bo và vát, sai ≤ 1e-9 tương đối so với LW − (4 − π)r² (bo bán kính r) và LW − 2·d₁·d₂ (vát d₁, d₂).
  3. Kích thước: dài, rộng rồi chỉ phía của góc đối; Xoay: góc hoặc `[Chọn điểm · P]`.
  4. Kết quả là LWPOLYLINE kín 4 đỉnh, hoặc 8 đỉnh khi bo/vát vừa; cung bo tiếp xúc hai cạnh (lệch ≤ 1e-12 rad); bo/vát không vừa thì theo đặc tả **[cần xác minh]**.
  5. Xem trước (jig INT-012) ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp); hoàn tất ≤ 20 ms (tầm trung), ≤ 50 ms (thấp).
  6. Đạt KTL (§0) trên ≥ 50 ca vàng.
  7. Đạt DoD-C, DoD-L.

### CMD-011 · POLYGON
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-002, CMD-003, GEO-006
- **Lệnh:** POLYGON
- **Đầu vào:** 03 §5; 07.2 UI-E10 (dòng POLYGON); sysvar POLYSIDES, SNAPANG; prototype `draw.js` (`IMPL.POLYGON`).
- **Đầu ra:** `commands/draw/polygon/` (`polygon_command.{h,cpp}`, `tests/golden/`); `docs/specs/commands/POLYGON.md`.
- **Tiêu chí nghiệm thu:**
  1. "Số cạnh" 3–1024 (mặc định POLYSIDES) → "Tâm đa giác" `[Cạnh · E]` → `[Nội tiếp · I / Ngoại tiếp · C]` (nhớ lựa chọn trước) → bán kính; Cạnh: hai đầu mút của một cạnh.
  2. Bán kính chỉ bằng chuột quyết định cả góc xoay; bán kính gõ số đặt cạnh dưới theo SNAPANG.
  3. Bất biến trên 10.000 ca: đỉnh (nội tiếp) hoặc trung điểm cạnh (ngoại tiếp) cách tâm đúng R ± 1e-12 tương đối; mọi cạnh bằng nhau.
  4. Xem trước (jig INT-012) ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp); hoàn tất với 1.024 cạnh ≤ 20 ms (tầm trung), ≤ 50 ms (thấp).
  5. Đạt KTL (§0) trên ≥ 30 ca vàng.
  6. Đạt DoD-C, DoD-L.

## 3. Đường tròn, cung, ellipse

### CMD-012 · CIRCLE: Tâm–Bán kính, Tâm–Đường kính, 2P, 3P, TTR, TTT
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-003, GEO-020, INT-010
- **Lệnh:** CIRCLE
- **Đầu vào:** 03 §5; 07.1 §7.2 (nút split CIRCLE); 07.2 UI-E10 (dòng CIRCLE); sysvar CIRCLERAD; API GEO-020 (2P, 3P, TTR, TTT và tổ hợp điểm với tiếp xúc hoãn), INT-010 (bắt điểm TAN, PER hoãn); prototype `draw.js` (`IMPL.CIRCLE`, `ttr`).
- **Đầu ra:** `commands/draw/circle/` (`circle_command.{h,cpp}`, `tests/golden/`); `docs/specs/commands/CIRCLE.md` kèm macro của các mục split (Tâm–Đường kính, Tiếp–Tiếp–Tiếp).
- **Tiêu chí nghiệm thu:**
  1. "Tâm đường tròn" `[3P / 2P / Ttr]` → "Bán kính" `[Đường kính · D]`, mặc định CIRCLERAD (0 là không có mặc định). 2P, 3P nhận điểm thường hoặc TAN, PER hoãn; TTR chọn hai đối tượng rồi bán kính, lấy nghiệm có tiếp điểm gần điểm chọn nhất; TTT chạy bằng macro `_3P _TAN _TAN _TAN`.
  2. Đối tượng tiếp xúc: line, xline, ray, cung, đường tròn, đoạn polyline. Vô nghiệm thì báo "Đường tròn không tồn tại", không tạo gì.
  3. Bất biến trên 10.000 ca mỗi cách dựng: |khoảng cách từ tâm tới đối tượng tiếp xúc − r| ≤ 1e-9 × kích thước; ba điểm của 3P nằm trên đường tròn ± 1e-9 tương đối.
  4. Xem trước (jig INT-012) ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp); hoàn tất ≤ 20 ms (tầm trung), ≤ 50 ms (thấp).
  5. Đạt KTL (§0) trên ≥ 60 ca vàng.
  6. Đạt DoD-C, DoD-L.

### CMD-013 · ARC (phần 1): 3 điểm, Đầu–Tâm–…, Tâm–Đầu–…
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-003, GEO-020
- **Đầu vào:** 03 §5; 07.1 §7.2 (11 cách dựng cung); 07.2 UI-E10 (dòng ARC); AutoCAD Command Reference (chỉ đối chiếu hành vi); API GEO-020; prototype `draw.js` (`IMPL.ARC`).
- **Đầu ra:** `commands/draw/arc/arc_command.{h,cpp}`: máy trạng thái prompt cho 7 cách (3 điểm; Đầu–Tâm–Cuối, Đầu–Tâm–Góc, Đầu–Tâm–Dây cung; Tâm–Đầu–Cuối, Tâm–Đầu–Góc, Tâm–Đầu–Dây cung); ca vàng `commands/draw/arc/tests/golden/`; phần tương ứng của `docs/specs/commands/ARC.md`.
- **Tiêu chí nghiệm thu:**
  1. "Điểm đầu của cung" `[Tâm · C]` → "Điểm thứ hai" `[Tâm · C / Điểm cuối · E]`; sau khi có tâm: "Điểm cuối" `[Góc · A / Độ dài dây cung · L]`.
  2. Quy tắc chiều: mặc định ngược chiều kim đồng hồ; giữ Ctrl đổi chiều; góc bao âm vẽ thuận chiều; dây cung dương cho cung nhỏ, âm cho cung lớn.
  3. Bất biến trên 10.000 ca mỗi cách: điểm đầu và điểm cuối (khi được chỉ định) đúng ± 1e-9 tương đối; góc bao và dây cung đúng giá trị nhập ± 1e-12 tương đối.
  4. Xem trước (jig INT-012) ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp).
  5. Đạt KTL (§0) trên ≥ 70 ca vàng.
  6. Đạt DoD-C, DoD-L.

### CMD-014 · ARC (phần 2): Đầu–Cuối–…, Nối tiếp, macro ribbon và nghiệm thu lệnh
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-013
- **Lệnh:** ARC
- **Đầu vào:** 07.1 §7.2; 07.2 UI-E10; sysvar LASTANGLE, LASTPOINT; `LastDrawn` (CMD-003); prototype `draw.js` (`IMPL.ARC`, nhánh `CONT`).
- **Đầu ra:** cách Đầu–Cuối–Góc, Đầu–Cuối–Hướng, Đầu–Cuối–Bán kính và Nối tiếp trong `commands/draw/arc/arc_command.cpp`; bảng macro cho 11 mục split; ca vàng; `docs/specs/commands/ARC.md` hoàn chỉnh.
- **Tiêu chí nghiệm thu:**
  1. Sau Điểm cuối: "Tâm của cung" `[Góc · A / Hướng · D / Bán kính · R]`; bán kính dương cho cung nhỏ, âm cho cung lớn; Hướng là tiếp tuyến tại điểm đầu.
  2. Nối tiếp (Enter ở prompt đầu): cung tiếp xúc điểm cuối của line, cung hoặc polyline vừa vẽ (lệch hướng ≤ 1e-12 rad); chưa có đối tượng để nối thì báo, không tạo gì.
  3. 11 macro của nút split trên ribbon cho kết quả trùng gõ tay (so hash KTL).
  4. Bất biến trên 10.000 ca mỗi cách như CMD-013.
  5. Đạt KTL (§0) trên ≥ 60 ca vàng.
  6. Đạt DoD-C, DoD-L.

### CMD-015 · ELLIPSE: Trục–Đầu mút, Tâm, Cung ellipse
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-003, GEO-005, INT-011
- **Lệnh:** ELLIPSE
- **Đầu vào:** 03 §5; 07.1 §7.2; 07.2 UI-E10 (dòng ELLIPSE); sysvar PELLIPSE, SNAPSTYL, SNAPISOPAIR; prototype `draw.js` (`IMPL.ELLIPSE`).
- **Đầu ra:** `commands/draw/ellipse/` (`ellipse_command.{h,cpp}`, `tests/golden/`); `docs/specs/commands/ELLIPSE.md`.
- **Tiêu chí nghiệm thu:**
  1. "Đầu mút trục" `[Cung · A / Tâm · C]` → đầu mút kia → "Khoảng cách tới trục kia" `[Xoay · R]`. Tâm: tâm, đầu mút trục, khoảng cách hoặc Xoay (0 ≤ góc < 89,4°). Cung: thêm "Góc đầu" `[Tham số · P]` và "Góc cuối" `[Tham số · P / Góc bao · I]`. `[Isocircle · I]` chỉ hiện khi SNAPSTYL = 1.
  2. PELLIPSE = 0 tạo ELLIPSE; PELLIPSE = 1 tạo polyline cung xấp xỉ (không có tùy chọn Cung), lệch ≤ 1e-4 × bán trục lớn **[đề xuất]**, số đỉnh theo đáp án QA-007 **[cần xác minh]**.
  3. Bất biến trên 10.000 ca: tỉ số trục trong (0, 1]; với Xoay, bán trục nhỏ = bán trục lớn × cos θ ± 1e-12 tương đối; góc và tham số của cung đúng giá trị nhập.
  4. Xem trước (jig INT-012) ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp).
  5. Đạt KTL (§0) trên ≥ 50 ca vàng.
  6. Đạt DoD-C, DoD-L.

## 4. Spline

### CMD-016 · SPLINE: Điểm đi qua (Fit) và Điểm điều khiển (CV)
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-003, GEO-007, GEO-023, QA-004
- **Lệnh:** SPLINE
- **Đầu vào:** 03 §5; 07.1 §7.2; 07.2 UI-E10 (dòng SPLINE); sysvar SPLMETHOD, SPLKNOTS, SPLDEGREE, SPLPERIODIC, DELOBJ; API GEO-023 (spline qua điểm, polyline ↔ spline), GEO-007 (NURBS); prototype `draw.js` (`IMPL.SPLINE`).
- **Đầu ra:** `commands/draw/spline/` (`spline_command.{h,cpp}`, `tests/golden/`); `docs/specs/commands/SPLINE.md`.
- **Tiêu chí nghiệm thu:**
  1. Báo thiết lập hiện tại; "Điểm đầu" `[Phương pháp · M / Nút · K / Đối tượng · O]` (CV: `[Phương pháp · M / Bậc · D / Đối tượng · O]`); điểm kế `[Tiếp tuyến đầu · T / Dung sai · L]`, sau đó `[Tiếp tuyến cuối · T / Dung sai · L / Hoàn tác · U]`, từ điểm thứ ba thêm `[Đóng · C]`.
  2. Nút Dây cung / Căn bậc hai / Đều (SPLKNOTS); bậc 1–10 (SPLDEGREE); Đóng tạo spline tuần hoàn khi SPLPERIODIC = 1; Đối tượng chuyển polyline đã PEDIT › Spline thành SPLINE, giữ hoặc xóa nguồn theo DELOBJ.
  3. Bất biến trên 10.000 ca: Fit với dung sai 0 đi qua mọi điểm ± 1e-9 tương đối; dung sai t thì cách mỗi điểm ≤ t + 1e-9; CV có điểm điều khiển đúng các điểm chỉ định; spline đóng liên tục C² tại điểm nối.
  4. Lưu DWG đúng cờ Fit/CV, kiểu nút, bậc, dữ liệu khớp; 300 spline sinh mở trong DWG TrueView không lỗi (QA-004).
  5. Xem trước (jig INT-012) ≤ 8 ms/khung với ≤ 500 điểm (tầm trung), ≤ 16 ms (thấp).
  6. Đạt KTL (§0) trên ≥ 50 ca vàng.
  7. Đạt DoD-C, DoD-L.

### CMD-017 · SPLINEDIT
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** CMD-016
- **Lệnh:** SPLINEDIT
- **Đầu vào:** 03 §5; 07.2 UI-E10 (dòng SPLINEDIT); 07.1 §9.10 (nhấp đúp spline); sysvar PLINECONVERTMODE, DELOBJ; API GEO-007 (chèn nút, nâng bậc, trọng số), GEO-023; AutoCAD Command Reference (chỉ đối chiếu hành vi); prototype `screens13.js` (`IMPL.SPLINEDIT`).
- **Đầu ra:** `commands/draw/spline/` (`splinedit_command.{h,cpp}`, `tests/golden/splinedit/`); `docs/specs/commands/SPLINEDIT.md`.
- **Tiêu chí nghiệm thu:**
  1. "Chọn spline" (nhận chọn trước) → `[Đóng/Mở · C/O / Nối · J / Khớp dữ liệu · F / Sửa đỉnh · E / Chuyển thành polyline · P / Đảo chiều · R / Hoàn tác · U / Thoát · X]`.
  2. Khớp dữ liệu (chỉ hiện khi spline có dữ liệu khớp): `[Thêm · A / Đóng/Mở / Xóa · D / Gấp khúc · K / Di chuyển · M / Bỏ dữ liệu khớp · P / Tiếp tuyến · T / Dung sai · L / Thoát · X]`; Di chuyển có `[Kế · N / Trước · P / Chọn điểm · S / Thoát · X]`.
  3. Sửa đỉnh: `[Thêm · A / Xóa · D / Nâng bậc · E / Di chuyển · M / Trọng số · W / Thoát · X]`; sửa điểm điều khiển làm mất dữ liệu khớp, có cảnh báo trước.
  4. Thao tác giữ hình (thêm điểm điều khiển, nâng bậc đến 26, đảo chiều): lệch hình ≤ 1e-12 × kích thước trên 10.000 ca. Nối nhận line, cung, polyline, spline chạm đầu mút. Chuyển thành polyline: độ chính xác 0–99, dùng cung hoặc đoạn thẳng theo PLINECONVERTMODE, lệch ≤ dung sai ghi trong đặc tả.
  5. Xem trước spline mới (jig INT-012) ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp); mỗi thao tác ≤ 20 ms (tầm trung), ≤ 50 ms (thấp) với spline ≤ 1.000 điểm điều khiển.
  6. Đạt KTL (§0) trên ≥ 80 ca vàng.
  7. Đạt DoD-C, DoD-L.

### CMD-018 · BLEND
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-002, CMD-003, GEO-023
- **Lệnh:** BLEND
- **Đầu vào:** 03 §5; 07.1 §7.2 (split Bo góc / Vát góc / Nối cong); API GEO-023 (đường nối cong); prototype `screens13.js` (`IMPL.BLEND`, `blendE`).
- **Đầu ra:** `commands/draw/spline/` (`blend_command.{h,cpp}`, `tests/golden/blend/`); `docs/specs/commands/BLEND.md`.
- **Tiêu chí nghiệm thu:**
  1. Báo "Liên tục = …"; "Chọn đối tượng thứ nhất" `[Liên tục · CON]` → `[Tiếp tuyến · T / Mượt · S]`; "Chọn đối tượng thứ hai" có xem trước khi rê.
  2. Nhận line, cung, cung ellipse, polyline hở, spline hở; nối tại đầu mút gần điểm chọn; đối tượng kín bị từ chối kèm thông báo.
  3. Bất biến trên 10.000 ca: hai đầu spline trùng đầu mút nguồn; Tiếp tuyến (bậc 3) lệch hướng ≤ 1e-9 rad; Mượt (bậc 5) liên tục cả độ cong, lệch ≤ 1e-6 tương đối.
  4. Xem trước (jig INT-012) ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp).
  5. Đạt KTL (§0) trên ≥ 30 ca vàng.
  6. Đạt DoD-C, DoD-L.

## 5. Điểm và chia

### CMD-019 · POINT, DDPTYPE
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-002, CMD-003, RGN-023
- **Lệnh:** POINT, DDPTYPE
- **Màn hình:** UI-D06
- **Đầu vào:** 03 §5; 07.2 UI-D06; sysvar PDMODE, PDSIZE; RGN-023 (vẽ POINT theo PDMODE, PDSIZE); prototype `draw.js` (`IMPL.POINT`), `screens5.js` (`SC.pointStyle`).
- **Đầu ra:** `commands/draw/point/` (`point_command.{h,cpp}`, `ddptype_command.{h,cpp}`: mở UI-D06, API đặt PDMODE và PDSIZE; `tests/golden/`); `docs/specs/commands/POINT.md`, `DDPTYPE.md`.
- **Tiêu chí nghiệm thu:**
  1. POINT báo PDMODE, PDSIZE hiện tại và tạo một điểm mỗi lần chạy; "Nhiều điểm" trên ribbon là macro lặp lệnh, mỗi điểm một mark.
  2. Điểm lưu góc trục X theo UCS lúc tạo, cao độ, độ dày.
  3. DDPTYPE chỉ nhận 20 giá trị PDMODE hợp lệ (0–4, 32–36, 64–68, 96–100); PDSIZE 0 (5% chiều cao vùng vẽ), dương (đơn vị tuyệt đối), âm (% kích thước viewport).
  4. Đổi PDMODE hoặc PDSIZE chỉ regen các POINT: 100.000 điểm ≤ 100 ms (tầm trung) **[đề xuất]**.
  5. Đặt bằng SETVAR, `.scr` và API hộp thoại cho cùng giá trị; đạt KTL (§0) trên ≥ 20 ca vàng.
  6. Đạt DoD-C, DoD-L.

### CMD-020 · DIVIDE, MEASURE (kèm tùy chọn Block)
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-003, GEO-020, DOC-012
- **Lệnh:** DIVIDE, MEASURE
- **Đầu vào:** 03 §5 (có tùy chọn chèn block); 07.2 UI-E10 (dòng DIVIDE/MEASURE); sysvar SNAPANG; API GEO-020 (điểm chia đều, chia theo khoảng), GEO-005, GEO-006, GEO-007 (tiếp tuyến để canh block); DOC-012 (INSERT, attribute); prototype `draw.js` (`IMPL.DIVIDE`, `IMPL.MEASURE`, `pathOf`).
- **Đầu ra:** `commands/draw/point/` (`divide_command.{h,cpp}`, `measure_command.{h,cpp}`, `tests/golden/`); `docs/specs/commands/DIVIDE.md`, `MEASURE.md`.
- **Tiêu chí nghiệm thu:**
  1. DIVIDE: "Chọn đối tượng cần chia" → "Số đoạn hoặc" `[Block · B]` (2–32767; ngoài khoảng thì báo và hỏi lại). MEASURE: "Độ dài mỗi đoạn hoặc" `[Block · B]`. Block: tên block (phải có trong bản vẽ), "Canh block theo đối tượng?" `[Có · Y / Không · N]`, rồi số đoạn hoặc độ dài.
  2. Nhận line, cung, đường tròn, ellipse, cung ellipse, polyline, spline. MEASURE bắt đầu từ đầu mút gần điểm chọn; polyline kín bắt đầu ở đỉnh đầu; đường tròn bắt đầu ở góc SNAPANG, đi ngược chiều kim đồng hồ.
  3. Bất biến trên 10.000 ca: khoảng cách dọc đường giữa hai dấu liên tiếp bằng nhau (DIVIDE) hoặc bằng độ dài nhập (MEASURE) ± 1e-9 tương đối; số dấu đúng (n − 1 với đối tượng hở, n với đối tượng kín); block canh theo tiếp tuyến ± 1e-12 rad.
  4. Các dấu vừa tạo thành tập chọn Previous; attribute không cố định của block xử lý theo đặc tả **[cần xác minh]**.
  5. DIVIDE 32.767 đoạn trên spline 1.000 điểm điều khiển ≤ 100 ms (tầm trung) **[đề xuất]**; xem trước dấu chia (jig INT-012) ≤ 8 ms/khung.
  6. Đạt KTL (§0) trên ≥ 50 ca vàng.
  7. Đạt DoD-C, DoD-L.

## 6. Region và đường biên

### CMD-021 · Mô hình REGION phẳng: đọc và ghi dữ liệu ACIS
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DWG-017, DOC-003, GEO-005, GEO-007, GEO-021, CMD-056, CMD-057, QA-004, QA-006
- **Đầu vào:** 03 §5 (REGION; BOUNDARY và hatch cần tạo region từ P0), §20 (giữ nguyên dữ liệu ACIS); 05 §7.1; Đặc tả DWG của ODA v5.4.1 (phần dữ liệu ACIS của 3DSOLID/REGION/BODY); tài liệu công khai về định dạng SAT (ACIS Save File Format); API DWG-017 (khối SAT/SAB theo phiên bản DWG); kho QA-001 (tệp có REGION).
- **Đầu ra:**
  - `commands/draw/region/acis_planar.{h,cpp}`: đọc SAT/SAB của region phẳng (body, lump, shell, face phẳng, loop, coedge, edge thẳng/ellipse/spline, vertex) thành vòng biên; ghi vòng biên thành SAT/SAB hợp lệ cho mọi phiên bản DWG mà DWG-017 hỗ trợ.
  - `commands/draw/region/region_geometry.{h,cpp}`: `RegionGeometry` (mặt phẳng, vòng ngoài, lỗ, nhiều mặt).
  - Nguồn diện tích và định dạng LIST cho region, đăng ký vào AREA (CMD-056) và LIST (CMD-057).
  - Fuzz target `commands/draw/region/fuzz/`.
- **Tiêu chí nghiệm thu:**
  1. 1.000 region sinh (cạnh line, cung, ellipse, spline; có lỗ; nhiều mặt) ghi AC1018 và AC1032: DWG TrueView và ODA File Converter mở không lỗi, không hỏi recover (QA-004).
  2. Ghi rồi đọc lại: vòng biên trùng ± 1e-12 tương đối, diện tích (GEO-021) trùng ± 1e-12 tương đối.
  3. Region phẳng trong kho QA-001: đọc được vòng biên ≥ 99,5% **[đề xuất]**; phần còn lại báo "không hỗ trợ" và giữ nguyên khối ACIS (DWG-017).
  4. AREA `_O` và LIST trên region cho diện tích, chu vi đúng ± 1e-9 tương đối trên 200 region.
  5. Fuzz parser SAT/SAB ≥ 1 giờ không lỗi.
  6. Đọc hoặc ghi region 1.000 cạnh ≤ 5 ms (tầm trung) **[đề xuất]**.
  7. Đạt DoD-C.
- **Ngoài phạm vi:** vẽ region (RGN-023); solid, surface 3D (P3).

### CMD-022 · BOUNDARY, -BOUNDARY
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-003, CMD-005, CMD-021
- **Lệnh:** BOUNDARY, -BOUNDARY
- **Màn hình:** UI-E03
- **Đầu vào:** 03 §5 (BOUNDARY dùng nhiều để tính diện tích), Phụ lục A; 07.1 §7.2; 07.2 UI-E03; prototype `screens6.js` (`IMPL.BOUNDARY`).
- **Đầu ra:**
  - `commands/draw/boundary/boundary_command.{h,cpp}`: BOUNDARY (API cho hộp thoại UI-E03), -BOUNDARY (dòng lệnh).
  - `commands/draw/boundary/loop_to_entity.{h,cpp}`: vòng → LWPOLYLINE có bulge, hoặc REGION qua CMD-021; dùng lại cho hatch.
  - Ca vàng `commands/draw/boundary/tests/golden/`; `docs/specs/commands/BOUNDARY.md`, `-BOUNDARY.md`.
- **Tiêu chí nghiệm thu:**
  1. -BOUNDARY: "Chỉ điểm bên trong" `[Tùy chọn nâng cao · A]` → `[Tập biên · B / Nhận diện island · I / Loại đối tượng · O]`; Tập biên `[Mới · N / Tất cả · E]`; Loại `[Region · R / Polyline · P]`. API hộp thoại có đủ thiết lập của UI-E03.
  2. Mỗi vòng (cả island khi bật nhận diện) thành một đối tượng trên layer hiện hành; dòng lệnh báo số đối tượng đã tạo kèm diện tích và chu vi đường biên vừa tạo (UI-E03).
  3. Cạnh line và cung thành đoạn và bulge chính xác; cạnh ellipse, spline với loại Polyline xử lý theo đặc tả **[cần xác minh]**, lệch xấp xỉ ≤ 1e-6 × kích thước vùng **[đề xuất]**.
  4. 1.000 cảnh vàng: diện tích đối tượng tạo ra bằng diện tích vùng của CMD-004 ± 1e-9 tương đối.
  5. Mỗi lần chỉ điểm trên cảnh 100.000 đường cong: ngân sách CMD-004 cộng ≤ 5 ms tạo đối tượng **[đề xuất]**; vùng không kín hiện chẩn đoán của CMD-005.
  6. Đạt KTL (§0) trên ≥ 50 ca vàng.
  7. Đạt DoD-C, DoD-L.

## 7. Hatch

### CMD-023 · Thiết lập hatch: sysvar HP*, thư viện mẫu, kiểm mật độ
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-005, RGN-012, VN-008, NT-011
- **Màn hình:** UI-E01
- **Đầu vào:** 03 §5 (hatch P0: pick point, island, liên kết, mẫu ANSI/ISO, annotative); 07.2 UI-E01 (panel Mẫu, Thuộc tính, Gốc, Tùy chọn); sysvar HPNAME, HPSCALE, HPANG, HPDOUBLE, HPSPACE, HPCOLOR, HPBACKGROUNDCOLOR, HPTRANSPARENCY, HPLAYER, HPASSOC, HPANNOTATIVE, HPISLANDDETECTION, HPISLANDDETECTIONMODE, HPGAPTOL, HPSEPARATE, HPDRAWORDER, HPORIGIN, HPORIGINMODE, HPINHERIT, HPBOUNDRETAIN, HPOBJWARNING, HPMAXLINES, HPQUICKPREVIEW, HPQUICKPREVTIMEOUT, HPDLGMODE, MEASUREMENT; RGN-012 (parser `.pat`); VN-008 (bộ `.pat` tự viết giữ tên chuẩn, nhóm TCVN); NT-011 (đường dẫn hỗ trợ); prototype `screens6.js` (`HS`, `PATS`).
- **Đầu ra:**
  - `commands/draw/hatch/hatch_settings.{h,cpp}`: `HatchSettings` (loại Tô đặc / Mẫu / Người dùng / Tùy biến / Gradient; tên, tỉ lệ, góc, bề rộng nét ISO, đan chéo, khoảng cách; màu, màu nền, transparency, layer ghi đè, tương đối Paper space; liên kết, annotative, island, dung sai khe hở, hatch riêng, thứ tự hiển thị, gốc, giữ đường biên, tập biên), đọc và ghi sysvar.
  - `commands/draw/hatch/pattern_library.{h,cpp}`: quét `.pat` trên đường dẫn hỗ trợ; nhóm ANSI, ISO, Khác, TCVN, Người dùng, Tùy biến; chọn bộ hệ inch hay hệ mét theo MEASUREMENT; tìm theo tên không phân biệt hoa thường; dữ liệu xem trước cho gallery.
  - `commands/draw/hatch/density_check.{h,cpp}`; mục thiết lập trong `docs/specs/commands/HATCH.md`.
- **Tiêu chí nghiệm thu:**
  1. Mọi sysvar HP* trong danh sách có kiểu, giới hạn, mặc định, phạm vi lưu đúng bảng sysvar của đặc tả; đặt bằng SETVAR, `.scr`, LISP `setvar` cho cùng giá trị.
  2. Nạp 100% tệp `.pat` của VN-008; mẫu trùng tên phân giải theo thứ tự đường dẫn hỗ trợ; tệp `.pat` lỗi bị bỏ qua kèm thông báo, thư viện vẫn dùng được.
  3. Tìm trong thư viện 2.000 mẫu ≤ 5 ms **[đề xuất]**.
  4. Kiểm mật độ ước lượng số nét trước khi sinh: vượt HPMAXLINES thì từ chối với thông báo "mẫu quá dày"; 1.000 ca tỉ lệ cực nhỏ không treo, cấp phát ≤ 64 MB **[đề xuất]**.
  5. Bề rộng nét ISO chỉ áp cho mẫu ISO, đổi tỉ lệ theo đặc tả **[cần xác minh]**.
  6. Đạt DoD-C.

### CMD-024 · Dựng entity HATCH từ vòng biên
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** CMD-003, CMD-022, CMD-023, DOC-005, DOC-020, DOC-021, GEO-021, QA-004
- **Đầu vào:** DXF Reference của Autodesk (HATCH: đường biên dạng polyline và dạng cạnh, cờ vòng, seed point, dữ liệu dòng mẫu, gradient); Đặc tả DWG của ODA v5.4.1 (HATCH); 03 §5; 07.2 UI-E01 (panel Gốc, Tùy chọn); API DOC-005 (reactor), DOC-020 (thứ tự vẽ), DOC-021 (ngữ cảnh tỉ lệ chú thích); dữ liệu dòng mẫu của RGN-012.
- **Đầu ra:** `commands/draw/hatch/hatch_builder.{h,cpp}`: `buildHatches(settings, loops, ctx)`:
  - đường biên dạng polyline khi vòng là một polyline, dạng cạnh (line, cung, cung ellipse, spline) cho trường hợp còn lại; cờ external, derived, textbox, outermost; seed point;
  - dữ liệu dòng mẫu đã áp tỉ lệ và góc; gốc theo HPORIGINMODE (0–5) và HPORIGIN; annotative; layer, màu, màu nền, transparency;
  - thứ tự hiển thị theo HPDRAWORDER (0–4); một hatch hoặc nhiều hatch (HPSEPARATE); giữ đường biên (HPBOUNDRETAIN) qua `loop_to_entity` (CMD-022);
  - danh sách nguồn của từng vòng cho hatch liên kết.
- **Tiêu chí nghiệm thu:**
  1. 500 hatch sinh (mọi loại cạnh, island, Tô đặc, Người dùng có đan chéo, annotative) ghi AC1018 và AC1032: DWG TrueView và ODA File Converter mở không lỗi (QA-004); đọc lại trùng từng trường.
  2. Diện tích hatch bằng diện tích vùng (GEO-021) ± 1e-9 tương đối; vòng ngoài và island đúng hướng, đúng cờ.
  3. 6 chế độ HPORIGINMODE cho gốc đúng góc hoặc tâm hộp bao vùng ± 1e-9; "Lưu làm mặc định" ghi HPORIGIN.
  4. HPDRAWORDER = 3 (mặc định): hatch nằm ngay dưới đối tượng biên thấp nhất trong thứ tự vẽ, 100% trên 300 ca; các giá trị khác đúng đặc tả.
  5. Annotative: có ngữ cảnh CANNOSCALE; tỉ lệ mẫu hiển thị theo tỉ lệ chú thích (RGN-026) **[cần xác minh]**.
  6. Dựng hatch 1.000 vòng, 50.000 cạnh ≤ 20 ms (tầm trung), ≤ 50 ms (thấp) **[đề xuất]**.
  7. Đạt DoD-C, DoD-L.

### CMD-025 · HATCH: lệnh tương tác, xem trước và nghiệm thu
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-005, CMD-024
- **Lệnh:** HATCH
- **Màn hình:** UI-E01
- **Đầu vào:** 03 §5, Phụ lục A, B; 07.1 §7.11 (tab ngữ cảnh Hatch), §9.8; 07.2 UI-E01, UI-E10 (dòng HATCH); prototype `screens6.js` (`IMPL.HATCH`, `hatchTab`, `setHS`).
- **Đầu ra:** `commands/draw/hatch/hatch_command.{h,cpp}`; API phiên lệnh cho tab ngữ cảnh UI-E01 (thiết lập hiện hành, hatch của lần chạy, trạng thái); ca vàng `commands/draw/hatch/tests/golden/`; `docs/specs/commands/HATCH.md` hoàn chỉnh.
- **Tiêu chí nghiệm thu:**
  1. "Chọn điểm bên trong" `[Chọn đối tượng · S / Bớt biên · B / Hoàn tác · U / Thiết lập · T]`. Mỗi lần chỉ điểm hoặc chọn đối tượng thêm vùng vào hatch của lần chạy (một hatch, hoặc mỗi vùng một hatch khi HPSEPARATE = 1); dòng lệnh báo diện tích vùng và số island.
  2. Chọn đối tượng: đối tượng kín thành vòng; vòng nằm trong vòng khác là island theo kiểu island hiện hành **[cần xác minh với QA-007]**; Bớt biên bỏ island đã chọn.
  3. Thiết lập đổi trong lúc lệnh chạy (sysvar HP* từ tab ngữ cảnh) áp ngay lên hatch của lần chạy, trong cùng transaction; Enter kết thúc; Esc theo đặc tả **[cần xác minh]**.
  4. Xem trước khi rê chuột (HPQUICKPREVIEW = 1) tô sáng biên và mẫu ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp); rê trên gallery mẫu dựng lại mẫu của hatch đang tạo ≤ 16 ms với hatch ≤ 10.000 nét **[đề xuất]**.
  5. Chỉ điểm trên cảnh 100.000 đường cong: ngân sách CMD-004 cộng ≤ 20 ms dựng hatch (tầm trung); tập biên vượt HPOBJWARNING thì hỏi xác nhận như AutoCAD.
  6. Đạt KTL (§0) trên ≥ 100 ca vàng: pick point, chọn đối tượng, 3 kiểu island, khe hở, biên trong block, annotative, các chế độ gốc, nhiều vùng.
  7. Đạt DoD-C, DoD-L.

### CMD-026 · -HATCH và chuỗi prompt cổ điển khi HATCH chạy từ script, LISP
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-009, CMD-025, QA-008
- **Lệnh:** -HATCH
- **Đầu vào:** 07.2 UI-E10 (lệnh tiền tố `-` giữ prompt như AutoCAD để script chạy được); AutoCAD Command Reference (-HATCH; chỉ đối chiếu hành vi); bộ LISP QA-008 (mẫu `(command "_.HATCH" ...)`, `(command "-HATCH" ...)`).
- **Đầu ra:** `commands/draw/hatch/hatch_cmdline.{h,cpp}`; ca vàng; `docs/specs/commands/-HATCH.md`; mục "gọi từ script, LISP" trong `HATCH.md`.
- **Tiêu chí nghiệm thu:**
  1. -HATCH: "Chỉ điểm bên trong" `[Thuộc tính · P / Chọn đối tượng · S / Vẽ biên · W / Bớt biên · B / Nâng cao · A / Thứ tự vẽ · DR / Gốc · O / Chú thích theo tỉ lệ · AN / Màu hatch · CO / Layer · LA / Transparency · T]` và prompt con: Thuộc tính `[? / Solid / User defined / Gradient]` → tỉ lệ → góc; Nâng cao `[Tập biên / Giữ biên / Nhận diện island / Kiểu / Liên kết / Dung sai khe hở / Hatch riêng]`; Gốc `[Dùng gốc hiện hành / Đặt gốc mới / Theo hộp bao biên]`.
  2. Vẽ biên dùng lại máy trạng thái PLINE (CMD-009) và hỏi có giữ polyline biên không.
  3. HATCH gọi từ `.scr` hoặc `(command ...)` dùng chuỗi prompt cổ điển: tên mẫu `[? / Solid / User defined]` → tỉ lệ → góc → chọn đối tượng hoặc hatch trực tiếp **[cần xác minh từng prompt]**.
  4. 100% LISP trong QA-008 có gọi HATCH hoặc -HATCH chạy không lệch prompt; kết quả trùng ca vàng.
  5. Đạt KTL (§0) trên ≥ 60 ca vàng.
  6. Đạt DoD-C, DoD-L.

### CMD-027 · Hatch liên kết: cập nhật khi biên đổi, mất liên kết đúng lúc
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-004, CMD-024, DOC-005, DOC-007, DOC-014, GEO-022
- **Đầu vào:** 03 §5 (hatch liên kết là P0); DXF Reference (HATCH: nguồn biên, reactor); API DOC-005 (reactor lưu trong bản vẽ), DOC-007 (sự kiện theo lô), DOC-014 (sao chép có ánh xạ ID), GEO-022 (biến đổi đường cong).
- **Đầu ra:** `commands/draw/hatch/hatch_assoc.{h,cpp}`: nghe sự kiện sửa đối tượng biên; tính lại vòng (từ seed point trên tập nguồn với vùng chỉ điểm, từ đối tượng với vùng chọn đối tượng); ghi trong cùng transaction của lệnh sửa; quy tắc mất liên kết; bản sao hatch cùng biên liên kết với biên sao.
- **Tiêu chí nghiệm thu:**
  1. 1.000 chuỗi sửa ngẫu nhiên trên biên qua API DOC-007 (dời đỉnh, biến đổi affine như MOVE, ROTATE, SCALE, MIRROR, STRETCH một phần): sau mỗi bước vòng hatch trùng hình học biên ± 1e-9 tương đối.
  2. Mất liên kết đúng đặc tả khi biên hở, biên bị xóa, chỉ dời hatch mà không dời biên; dời cả hai thì giữ liên kết **[cần xác minh từng trường hợp với QA-007]**.
  3. Sao chép hatch cùng biên: bản sao liên kết với biên sao; sao chép riêng hatch: bản sao không liên kết.
  4. Cập nhật một hatch ≤ 1.000 cạnh ≤ 5 ms; một lần sửa làm đổi 1.000 hatch ≤ 200 ms (tầm trung, 8 luồng: tính song song trên snapshot, ghi một luồng) **[đề xuất]**.
  5. Undo/redo lệnh sửa trả cả biên lẫn hatch về đúng trên 200 ca (so hash).
  6. Đạt DoD-C, DoD-H, DoD-L.

### CMD-028 · Grip của hatch
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-027, INT-013
- **Đầu vào:** 07.1 §9.5 (Hatch: grip tâm, đỉnh biên nếu hatch không liên kết; menu grip tâm Kéo giãn / Gốc / Góc / Tỉ lệ); README §4.2 (grip của hatch đăng ký qua API INT-013).
- **Đầu ra:** `commands/draw/hatch/hatch_grips.{h,cpp}`: grip tâm có menu Kéo giãn, Điểm gốc, Góc hatch, Tỉ lệ hatch; hatch không liên kết có thêm grip đỉnh (Kéo giãn, Thêm đỉnh, Bớt đỉnh) và grip giữa cạnh (Kéo giãn, Thêm đỉnh, Chuyển thành cung hoặc đoạn thẳng); ô nhập DYN cho góc và tỉ lệ; kiểm thử `commands/draw/hatch/tests/grips/`.
- **Tiêu chí nghiệm thu:**
  1. Vị trí grip đúng đặc tả (grip tâm **[cần xác minh vị trí]**, mọi đỉnh và trung điểm cạnh) trên 200 hatch.
  2. Kéo grip góc, tỉ lệ đổi đúng góc và tỉ lệ mẫu ± 1e-12; kéo đỉnh của hatch không liên kết sửa đúng vòng; hatch liên kết chỉ có grip tâm.
  3. Mỗi lần kéo grip là một mark undo; Esc giữa chừng không đổi bản vẽ.
  4. Kéo grip: xem trước ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp) với hatch ≤ 10.000 nét; hiện grip cho 1.000 hatch đã chọn ≤ 16 ms **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.

### CMD-029 · HATCHEDIT, -HATCHEDIT
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-022, CMD-025, CMD-027
- **Lệnh:** HATCHEDIT, -HATCHEDIT
- **Màn hình:** UI-E01
- **Đầu vào:** 03 §5, Phụ lục A; 07.1 §9.9 (menu chuột phải hatch: Sửa hatch, Đặt gốc, Tạo đường biên, Tách hatch), §9.10 (nhấp đúp hatch); 07.2 UI-E01 (tab Sửa hatch); AutoCAD Command Reference (-HATCHEDIT; chỉ đối chiếu hành vi); prototype `screens6.js` (`IMPL.HATCHEDIT`).
- **Đầu ra:** `commands/draw/hatch/hatchedit_command.{h,cpp}`; ca vàng; `docs/specs/commands/HATCHEDIT.md`, `-HATCHEDIT.md`.
- **Tiêu chí nghiệm thu:**
  1. HATCHEDIT: chọn một hoặc nhiều hatch (nhận chọn trước), mở phiên sửa cho tab ngữ cảnh; mỗi thay đổi từ tab chạy như một -HATCHEDIT, một mark undo.
  2. -HATCHEDIT `[Bỏ liên kết · DI / Kiểu · S / Thuộc tính · P / Thứ tự vẽ · DR / Thêm biên · AD / Bớt biên · R / Tạo lại biên · B / Liên kết · AS / Tách hatch · H / Gốc · OR / Transparency · T]`; Tạo lại biên hỏi loại `[Region / Polyline]` và có liên kết với biên mới không.
  3. Tách hatch: hatch có N vùng ngoài rời nhau thành N hatch giữ thuộc tính; tổng diện tích không đổi ± 1e-9 tương đối.
  4. Áp một thay đổi cho 1.000 hatch đã chọn ≤ 50 ms (tầm trung) **[đề xuất]**.
  5. Đạt KTL (§0) trên ≥ 70 ca vàng.
  6. Đạt DoD-C, DoD-L.

### CMD-030 · HATCHGENERATEBOUNDARY, HATCHSETORIGIN
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-029
- **Lệnh:** HATCHGENERATEBOUNDARY, HATCHSETORIGIN
- **Đầu vào:** 03 §5 (P1); 07.1 §9.9; AutoCAD Command Reference (chỉ đối chiếu hành vi).
- **Đầu ra:** `commands/draw/hatch/hatch_boundary_cmds.{h,cpp}`; ca vàng; `docs/specs/commands/HATCHGENERATEBOUNDARY.md`, `HATCHSETORIGIN.md`.
- **Tiêu chí nghiệm thu:**
  1. HATCHGENERATEBOUNDARY: chọn hatch (nhận chọn trước) → polyline không liên kết quanh mỗi vòng, layer theo đặc tả **[cần xác minh]**; cạnh ellipse, spline xử lý như CMD-022.
  2. HATCHSETORIGIN: chọn hatch → điểm gốc mới; mẫu dịch đúng, vòng biên giữ nguyên ± 1e-12.
  3. Diện tích polyline tạo ra bằng diện tích hatch ± 1e-9 tương đối trên 300 ca.
  4. Hoàn tất ≤ 20 ms (tầm trung), ≤ 50 ms (thấp) với hatch ≤ 1.000 cạnh.
  5. Đạt KTL (§0) trên ≥ 30 ca vàng.
  6. Đạt DoD-C, DoD-L.

### CMD-031 · GRADIENT
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-025, CMD-026, RGN-012, QA-005
- **Lệnh:** GRADIENT
- **Màn hình:** UI-E01
- **Đầu vào:** 03 §5 (gradient P1); 07.1 §7.2; 07.2 UI-E01 (gradient ở 1.0); sysvar GFNAME, GFCLR1, GFCLR2, GFCLRSTATE, GFCLRLUM, GFANG, GFSHIFT; RGN-012 (vẽ gradient); prototype `screens13.js` (`IMPL.GRADIENT`, `gradTab`).
- **Đầu ra:** `commands/draw/hatch/gradient_command.{h,cpp}`; loại Gradient trong `HatchSettings`, `hatch_builder`, -HATCH › Thuộc tính › Gradient và -HATCHEDIT; ca vàng; `docs/specs/commands/GRADIENT.md`.
- **Tiêu chí nghiệm thu:**
  1. 9 mẫu gradient (tuyến tính, trụ, trụ ngược, cầu, cầu ngược, bán cầu, bán cầu ngược, cong, cong ngược); một màu kèm sắc độ (GFCLRLUM) hoặc hai màu; góc; canh giữa (GFSHIFT).
  2. Mọi cách lấy vùng của HATCH (chỉ điểm, chọn đối tượng, island, liên kết, thứ tự vẽ) chạy y hệt với gradient.
  3. 300 gradient sinh lưu DWG đúng trường gradient; PDF do DWG TrueView in so với hiển thị TrionCAD SSIM ≥ 0,98 (QA-005) **[đề xuất]**.
  4. Xem trước (jig INT-012) ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp).
  5. Đạt KTL (§0) trên ≥ 40 ca vàng.
  6. Đạt DoD-C, DoD-L.

## 8. Region, vùng tô, vùng che, đám mây sửa đổi

### CMD-032 · REGION
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-003, CMD-021, GEO-010
- **Lệnh:** REGION
- **Đầu vào:** 03 §5; 07.1 §7.2; sysvar DELOBJ; prototype `screens13.js` (`IMPL.REGION`).
- **Đầu ra:** `commands/draw/region/` (`region_command.{h,cpp}`, `tests/golden/`); `docs/specs/commands/REGION.md`.
- **Tiêu chí nghiệm thu:**
  1. "Chọn đối tượng" (nhận chọn trước): polyline kín, đường tròn, ellipse, spline kín, và chuỗi line, cung, cung ellipse, spline, polyline hở nối đầu–cuối thành vòng kín; vòng tự cắt bị từ chối.
  2. Báo "Đã trích N vòng. Đã tạo M region."; giữ hoặc xóa nguồn theo DELOBJ; layer của region theo đặc tả **[cần xác minh]**.
  3. 1.000 cảnh vàng: số vòng và diện tích (± 1e-9 tương đối) đúng; region mở được trong DWG TrueView (dữ liệu ghi qua CMD-021).
  4. Tạo region từ 10.000 đối tượng rời ≤ 200 ms (tầm trung) **[đề xuất]**; trường hợp thường ≤ 20 ms (tầm trung), ≤ 50 ms (thấp).
  5. Đạt KTL (§0) trên ≥ 40 ca vàng.
  6. Đạt DoD-C, DoD-L.

### CMD-033 · SOLID, DONUT
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-002, CMD-003, GEO-006
- **Lệnh:** SOLID, DONUT
- **Đầu vào:** 03 §5 (SOLID P1, hiển thị đúng từ P0; DONUT P1); sysvar FILLMODE, DONUTID, DONUTOD, THICKNESS; prototype `screens13.js` (`IMPL.DONUT`).
- **Đầu ra:** `commands/draw/fill/` (`solid_command.{h,cpp}`, `donut_command.{h,cpp}`, `tests/golden/`); `docs/specs/commands/SOLID.md`, `DONUT.md`.
- **Tiêu chí nghiệm thu:**
  1. SOLID: điểm 1 → 2 → 3 → "Điểm thứ tư hoặc <thoát>"; Enter ở điểm thứ tư tạo tam giác; lặp cặp điểm 3–4 tạo SOLID nối tiếp dùng hai điểm cuối của SOLID trước; thứ tự đỉnh lưu theo quy ước "hình nơ" của AutoCAD.
  2. DONUT: đường kính trong (DONUTID) → đường kính ngoài (DONUTOD) → tâm, lặp đến Enter; kết quả LWPOLYLINE kín hai cung bulge 1, bề rộng (D − d)/2, bán kính (D + d)/4; d = 0 cho chấm đặc; D ≤ d bị từ chối.
  3. Bất biến trên 10.000 ca: hình học đúng ± 1e-12 tương đối.
  4. Xem trước (jig INT-012) ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp).
  5. Đạt KTL (§0) trên ≥ 30 ca vàng.
  6. Đạt DoD-C, DoD-L.

### CMD-034 · WIPEOUT
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-003, RGN-023, INT-013, QA-004
- **Lệnh:** WIPEOUT
- **Đầu vào:** 03 §5; 07.1 §7.4 (panel Đánh dấu); sysvar WIPEOUTFRAME; DXF Reference (WIPEOUT, từ điển ACAD_WIPEOUT_VARS); RGN-023 (vẽ WIPEOUT); prototype `screens13.js` (`IMPL.WIPEOUT`).
- **Đầu ra:** `commands/draw/fill/` (`wipeout_command.{h,cpp}`, grip đỉnh đăng ký qua INT-013, `tests/golden/wipeout/`); `docs/specs/commands/WIPEOUT.md`.
- **Tiêu chí nghiệm thu:**
  1. "Điểm đầu" `[Khung · F / Polyline · P] <Polyline>`; Khung `[Bật · ON / Tắt · OFF / Hiện nhưng không in · D]` đặt WIPEOUTFRAME cho mọi vùng che của bản vẽ; Polyline: chọn polyline kín chỉ gồm đoạn thẳng **[cần xác minh với AutoCAD 2027]**, hỏi có xóa polyline không; chỉ điểm: tối thiểu 3 điểm, `[Hoàn tác · U]`.
  2. Tạo đủ đối tượng phụ (từ điển ACAD_WIPEOUT_VARS) khi bản vẽ chưa có; 300 vùng che sinh mở trong DWG TrueView không lỗi (QA-004).
  3. Grip đỉnh kéo giãn đúng, mỗi lần kéo một mark undo.
  4. Xem trước (jig INT-012) ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp).
  5. Đạt KTL (§0) trên ≥ 30 ca vàng.
  6. Đạt DoD-C, DoD-L.

### CMD-035 · REVCLOUD: Chữ nhật, Đa giác, Tự do, Đối tượng, Sửa
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** CMD-002, CMD-003, GEO-006, GEO-007, INT-013
- **Lệnh:** REVCLOUD
- **Đầu vào:** 03 §5; 07.1 §7.2 (đám mây: chữ nhật, đa giác, tự do, chọn đối tượng), §7.4; 07.2 UI-E10 (dòng REVCLOUD); sysvar REVCLOUDCREATEMODE, REVCLOUDARCVARIANCE, REVCLOUDGRIPS, DIMSCALE, DELOBJ; prototype `draw.js` (`IMPL.REVCLOUD`, `cloud`).
- **Đầu ra:** `commands/draw/revcloud/` (`revcloud_command.{h,cpp}`, `cloud_builder.{h,cpp}`, grip theo REVCLOUDGRIPS qua INT-013, `tests/golden/`); `docs/specs/commands/REVCLOUD.md`.
- **Tiêu chí nghiệm thu:**
  1. Báo độ dài cung, kiểu, chế độ; "Điểm đầu" `[Độ dài cung · A / Đối tượng · O / Chữ nhật · R / Đa giác · P / Tự do · F / Kiểu · S / Sửa · M] <Đối tượng>`; chế độ nhớ ở REVCLOUDCREATEMODE.
  2. Kiểu `[Thường · N / Nét bút · C]` (Nét bút: cung có bề rộng thay đổi); độ dài cung nhân DIMSCALE **[cần xác minh]**; REVCLOUDARCVARIANCE bật/tắt độ dài cung không đều.
  3. Đối tượng: đường tròn, ellipse, polyline kín, spline kín thành đám mây, `[Đảo chiều]`, xử lý nguồn theo đặc tả; Sửa: chọn đám mây, vẽ lại một đoạn, chọn phía bỏ, `[Đảo chiều]`.
  4. Bất biến trên 10.000 ca: polyline kín; mọi cung cùng lồi ra ngoài (hoặc vào trong khi đảo chiều); dây cung nằm trong khoảng cho phép quanh độ dài cung đặt **[cần xác minh hệ số]**; Tự do không tạo đỉnh trùng.
  5. Xem trước, kể cả Tự do khi rê chuột (jig INT-012) ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp).
  6. Đạt KTL (§0) trên ≥ 60 ca vàng; Tự do kiểm bằng chuỗi sự kiện chuột mô phỏng, `.scr` không áp dụng (ghi trong đặc tả).
  7. Đạt DoD-C, DoD-L.

## 9. Đa tuyến song song

### CMD-036 · MLSTYLE: kiểu đa tuyến song song, tệp .mln
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** DOC-004, DWG-013, NT-011, QA-006
- **Lệnh:** MLSTYLE
- **Màn hình:** UI-D07
- **Đầu vào:** 03 §5 (MLINE thường dùng vẽ tường ở VN); 07.2 UI-D07 (mẫu tường 110, 220 theo TCVN); DXF Reference (MLINESTYLE, từ điển ACAD_MLINESTYLE); định dạng tệp `.mln`; nội dung TCVN của VN-008 (nếu có tệp `.mln`); prototype `screens5.js` (`SC.mlStyle`), `screens13.js` (`mlStyles`).
- **Đầu ra:**
  - `commands/draw/mline/mlstyle_model.{h,cpp}`: thêm, sửa, đổi tên, xóa, đặt hiện hành; quy tắc khóa khi đang dùng; tối đa 16 thành phần (độ lệch, màu, linetype); nắp đầu/cuối (đoạn thẳng, cung ngoài, cung trong, góc); màu tô; hiện mối nối.
  - `commands/draw/mline/mln_file.{h,cpp}`: đọc, ghi `.mln`; fuzz target.
  - `commands/draw/mline/mlstyle_command.{h,cpp}`: mở UI-D07, API cho hộp thoại; `docs/specs/commands/MLSTYLE.md`.
- **Tiêu chí nghiệm thu:**
  1. STANDARD không đổi tên, không xóa; kiểu hiện hành hoặc đang dùng không xóa; kiểu đã có MLINE dùng thì không sửa thành phần và nắp **[cần xác minh]**; mỗi lần từ chối có thông báo nêu lý do.
  2. Đọc 100% tệp `.mln` mẫu; ghi rồi đọc lại trùng từng trường; fuzz parser ≥ 1 giờ.
  3. Có mẫu TUONG-110, TUONG-220 khi tệp mẫu cung cấp; kiểu tạo mới qua API lưu DWG, DWG TrueView mở không lỗi.
  4. Mỗi thao tác của hộp thoại là một lệnh, một mark undo; đạt KTL (§0) mục 2 và 6 trên ≥ 30 ca qua API (lệnh chỉ có hộp thoại như AutoCAD, `.scr` không áp dụng).
  5. Đạt DoD-C.

### CMD-037 · MLINE
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-003, CMD-036, RGN-023, INT-013, QA-004, QA-005
- **Lệnh:** MLINE
- **Đầu vào:** 03 §5; 07.1 §7.2; sysvar CMLJUST, CMLSCALE, CMLSTYLE; DXF Reference (MLINE: đỉnh, hướng đoạn, hướng nối góc, tham số hóa từng thành phần, tham số vùng tô); RGN-023 (vẽ MLINE); prototype `screens13.js` (`IMPL.MLINE`, `X.mline`).
- **Đầu ra:** `commands/draw/mline/` (`mline_command.{h,cpp}`, `mline_geometry.{h,cpp}`: hướng nối góc, tham số hóa thành phần ban đầu; grip đỉnh qua INT-013; `tests/golden/`); `docs/specs/commands/MLINE.md`.
- **Tiêu chí nghiệm thu:**
  1. Báo "Canh lề = …, Tỉ lệ = …, Kiểu = …"; "Điểm đầu" `[Canh lề · J / Tỉ lệ · S / Kiểu · ST]`; Canh lề `[Trên · T / Không · Z / Dưới · B]`; Kiểu nhận tên hoặc `?`; điểm kế `[Hoàn tác · U]`, từ điểm thứ ba thêm `[Đóng · C]`.
  2. Bất biến trên 10.000 ca: mỗi thành phần song song với đường tâm và cách đúng độ lệch × CMLSCALE ± 1e-9 tương đối; hướng nối góc là phân giác ± 1e-12 rad.
  3. 300 MLINE sinh lưu DWG đúng các trường tham số hóa; DWG TrueView mở không lỗi (QA-004); PDF do TrueView in so với hiển thị TrionCAD SSIM ≥ 0,98 (QA-005) **[đề xuất]**.
  4. Kéo grip đỉnh giữ đúng bất biến ở mục 2.
  5. Xem trước (jig INT-012) ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp).
  6. Đạt KTL (§0) trên ≥ 40 ca vàng.
  7. Đạt DoD-C, DoD-L.

### CMD-038 · MLEDIT, -MLEDIT: 12 công cụ sửa đa tuyến song song
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** CMD-037, GEO-008, QA-004
- **Lệnh:** MLEDIT, -MLEDIT
- **Màn hình:** UI-E09
- **Đầu vào:** 03 §5; 07.1 §9.10 (nhấp đúp MLINE mở MLEDIT); 07.2 UI-E09; AutoCAD Command Reference (MLEDIT; chỉ đối chiếu hành vi); DXF Reference (tham số hóa thành phần MLINE); prototype `screens6.js` (`IMPL.MLEDIT`, `MLT`).
- **Đầu ra:** `commands/draw/mline/` (`mledit_command.{h,cpp}`, `mledit_tools.{h,cpp}`: ngắt và nối trên tham số hóa thành phần; `tests/golden/mledit/`); `docs/specs/commands/MLEDIT.md`, `-MLEDIT.md`.
- **Tiêu chí nghiệm thu:**
  1. 12 công cụ: chữ thập kín, hở, hợp nhất (CC, OC, MC); chữ T kín, hở, hợp nhất (CT, OT, MT); nối góc (CJ); thêm đỉnh (AV); xóa đỉnh (DV); cắt một (CS); cắt tất cả (CA); hàn tất cả (WA). Prompt chọn đa tuyến thứ nhất, thứ hai (hoặc điểm thứ hai với CS, CA, WA), lặp, `[Hoàn tác · U]`.
  2. -MLEDIT nhận 12 mã trên cho script; API cho lưới công cụ UI-E09.
  3. Ngắt ghi trong tham số hóa thành phần, đối tượng vẫn là một MLINE; điểm ngắt nằm trên giao điểm thành phần ± 1e-9 tương đối; hàn bỏ đúng các ngắt trong đoạn chọn.
  4. 500 cảnh vàng (giao vuông, giao xiên, nhiều thành phần, có nắp) đúng tô-pô; DWG TrueView mở không lỗi (QA-004).
  5. Mỗi thao tác ≤ 20 ms (tầm trung), ≤ 50 ms (thấp); xem trước (jig INT-012) ≤ 8 ms/khung.
  6. Đạt KTL (§0) trên ≥ 80 ca vàng.
  7. Đạt DoD-C, DoD-L.

## 10. Dấu tâm, đường tâm, tạo đối tượng cùng loại

### CMD-039 · CENTERMARK, CENTERLINE: hình học và sysvar
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-003, DOC-012, INT-013, QA-004
- **Lệnh:** CENTERMARK, CENTERLINE
- **Đầu vào:** 03 §5 (dấu tâm, đường tâm có liên kết); 07.1 §7.2, §7.4; sysvar CENTERCROSSSIZE, CENTERCROSSGAP, CENTEREXE, CENTERMARKEXE, CENTERLTYPE, CENTERLTSCALE, CENTERLAYER; AutoCAD Command Reference (chỉ đối chiếu hành vi); prototype `screens13.js` (`IMPL.CENTERMARK`, `IMPL.CENTERLINE`).
- **Đầu ra:** `commands/draw/center/` (`centermark_command.{h,cpp}`, `centerline_command.{h,cpp}`, `center_geometry.{h,cpp}`, grip kéo dài đầu đường tâm qua INT-013, `tests/golden/`); `docs/specs/commands/CENTERMARK.md`, `CENTERLINE.md`.
- **Tiêu chí nghiệm thu:**
  1. CENTERMARK: chọn đường tròn hoặc cung, lặp; dấu chữ thập theo CENTERCROSSSIZE, khe CENTERCROSSGAP, đường kéo dài CENTEREXE khi CENTERMARKEXE = 1; linetype, tỉ lệ linetype, layer theo sysvar.
  2. CENTERLINE: chọn hai line hoặc đoạn polyline; song song thì vẽ đường giữa, không song song thì theo phân giác giữa các đầu mút; kéo dài CENTEREXE ở mỗi đầu.
  3. Dạng lưu tương thích AutoCAD **[cần xác minh: block vô danh kèm đối tượng liên kết]**; 200 dấu tâm, đường tâm sinh mở trong DWG TrueView hiển thị đúng (QA-004).
  4. Bất biến trên 10.000 ca: tâm dấu trùng tâm cung ± 1e-12; đường tâm cách đều hai cạnh ± 1e-9 tương đối.
  5. Xem trước khi rê lên đối tượng (jig INT-012) ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp).
  6. Đạt KTL (§0) trên ≥ 40 ca vàng.
  7. Đạt DoD-C, DoD-L.

### CMD-040 · Liên kết của dấu tâm và đường tâm
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-039, DOC-005, DOC-007, DWG-018
- **Đầu vào:** 03 §5; API DOC-005 (reactor), DOC-007 (sự kiện); DWG-018 (giữ nguyên đối tượng chưa hiểu, gồm đối tượng liên kết của AutoCAD); kho QA-001 (bản vẽ có dấu tâm do AutoCAD tạo).
- **Đầu ra:** `commands/draw/center/center_assoc.{h,cpp}`: cập nhật dấu tâm, đường tâm khi cung, đường tròn, line nguồn đổi (dời, kéo giãn, đổi tỉ lệ, grip); nguồn bị xóa thì xử lý theo đặc tả; nhận diện dấu tâm, đường tâm do AutoCAD tạo khi dữ liệu liên kết đọc được.
- **Tiêu chí nghiệm thu:**
  1. 1.000 chuỗi sửa ngẫu nhiên trên nguồn: dấu tâm và đường tâm thỏa bất biến của CMD-039 sau mỗi bước.
  2. Undo/redo trả cả nguồn và dấu về đúng trên 200 ca (so hash).
  3. ≥ 50 bản vẽ AutoCAD có dấu tâm, đường tâm (QA-001): mở, sửa nguồn, lưu; DWG TrueView mở không lỗi; dữ liệu liên kết chưa hiểu được giữ nguyên **[cần xác minh: dạng lưu associative framework chưa có schema ở DWG]**.
  4. Cập nhật một dấu ≤ 1 ms; 1.000 dấu khi đổi tỉ lệ cả bản vẽ ≤ 50 ms (tầm trung) **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** CENTERREASSOC, CENTERDISASSOCIATE (chưa có trong 03).

### CMD-041 · ADDSELECTED
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-003, CMD-006, CMD-009, CMD-012, CMD-014, CMD-015, CMD-016, CMD-025, CMD-037, CMD:TEXT, CMD:MTEXT, CMD:INSERT, CMD:DIMLINEAR, CMD:MLEADER
- **Lệnh:** ADDSELECTED
- **Đầu vào:** 03 §5; 07.1 §9.9 (menu chuột phải: Tạo đối tượng cùng loại); `PropertyOverrideScope` (CMD-003); prototype `screens6.js` (`IMPL.ADDSELECTED`).
- **Đầu ra:** `commands/draw/addselected/` (`addselected_command.{h,cpp}`: bảng ánh xạ loại đối tượng → lệnh tạo và thuộc tính riêng; `tests/golden/`); `docs/specs/commands/ADDSELECTED.md`.
- **Tiêu chí nghiệm thu:**
  1. "Chọn đối tượng" (nhận một đối tượng chọn trước) → chạy lệnh tạo cùng loại với thuộc tính chung của mẫu (layer, màu, linetype, tỉ lệ linetype, lineweight, transparency, plot style, độ dày) và thuộc tính riêng (kiểu chữ và chiều cao, kiểu kích thước, mẫu, tỉ lệ, góc hatch, tên block, kiểu MLINE, kiểu multileader).
  2. Bảng ánh xạ phủ mọi loại có lệnh tạo trong bộ lệnh đến v1.0; loại không có lệnh tạo thì báo, không làm gì.
  3. Thuộc tính hiện hành trả về như cũ sau lệnh đích, kể cả khi Esc: 100% trên 100 ca.
  4. Đạt KTL (§0) trên ≥ 40 ca vàng, mỗi loại trong bảng ít nhất một ca.
  5. Đạt DoD-C, DoD-L.

## 11. Chọn và nhóm

### CMD-042 · SELECT, SELECTALL
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-002, INT-008
- **Lệnh:** SELECT, SELECTALL
- **Đầu vào:** 03 §4, Phụ lục A; 07.1 §9.3, §11 (Ctrl+A); 07.2 UI-E10 (dòng SELECT); sysvar PICKFIRST, PICKADD, PICKAUTO; API INT-008 (mọi chế độ chọn, tập Previous); prototype `cmd.js` (`IMPL.SELECT`, `IMPL.SELECTALL`).
- **Đầu ra:** `commands/select/` (`select_command.{h,cpp}`, `selectall_command.{h,cpp}`, `tests/golden/`); `docs/specs/commands/SELECT.md`, `SELECTALL.md`.
- **Tiêu chí nghiệm thu:**
  1. SELECT nhận đủ chế độ của UI-E10 (W, L, C, BOX, ALL, F, WP, CP, G, A, R, M, P, U, AU, SI) qua INT-008; `?` liệt kê tùy chọn; kết quả thành tập Previous; dòng lệnh đếm "Đã chọn N".
  2. SELECTALL chọn mọi đối tượng của không gian hiện hành trừ layer đóng băng; đối tượng trên layer khóa theo đặc tả **[cần xác minh]**; kết quả thành vùng chọn PICKFIRST.
  3. SELECTALL trên 1 triệu entity ≤ 30 ms (tầm trung), ≤ 80 ms (thấp) **[đề xuất]**.
  4. Đạt KTL (§0) trên ≥ 40 ca vàng.
  5. Đạt DoD-C, DoD-H, DoD-L.

### CMD-043 · QSELECT: engine truy vấn chọn nhanh
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-002, DOC-009, DOC-017, INT-008, NT-005
- **Lệnh:** QSELECT
- **Màn hình:** UI-E06
- **Đầu vào:** 03 §4 (QSELECT P0); 05 §9.2 (lọc chọn song song trên hàng triệu đối tượng); 07.2 UI-E06; API DOC-017 (thuộc tính chung và theo loại), DOC-009 (snapshot); prototype `screens6.js` (`IMPL.QSELECT`, `QPROPS`, `qval`).
- **Đầu ra:**
  - `commands/select/qselect/query.{h,cpp}`: `Query` gồm phạm vi (toàn bản vẽ hoặc vùng chọn hiện tại), loại đối tượng, thuộc tính, toán tử (=, ≠, >, <, khớp wildcard kiểu `wcmatch`, chọn tất cả), cách áp (đưa vào hoặc loại khỏi tập mới; cộng vào tập hiện tại).
  - `commands/select/qselect/query_eval.{h,cpp}`: quét song song trên snapshot.
  - `commands/select/qselect/qselect_command.{h,cpp}`: API cho UI-E06 (loại có trong bản vẽ kèm số lượng, thuộc tính theo loại, giá trị có sẵn); `docs/specs/commands/QSELECT.md`.
- **Tiêu chí nghiệm thu:**
  1. Toán tử chỉ hiện theo kiểu thuộc tính (số, chuỗi, màu, liệt kê); wildcard theo cú pháp `wcmatch`, không phân biệt hoa thường, so chuỗi tiếng Việt sau chuẩn hóa NFC; so số theo độ chính xác hiển thị LUPREC, AUPREC **[cần xác minh]**.
  2. Kết quả trùng 100% bộ lọc tham chiếu viết đơn luồng trên 1.000 truy vấn ngẫu nhiên × 20 bản vẽ QA-001.
  3. Truy vấn một thuộc tính lưu sẵn trên 1 triệu entity ≤ 50 ms (tầm trung), ≤ 150 ms (thấp); thuộc tính tính toán (độ dài, diện tích) ≤ 150 ms (tầm trung) **[đề xuất]**; hiệu suất song song ≥ 60% đến 8 luồng; kết quả tất định.
  4. Danh sách loại kèm số lượng cho UI-E06 trên 1 triệu entity ≤ 30 ms **[đề xuất]**.
  5. Đạt KTL (§0) mục 2, 4, 6 trên ≥ 40 ca qua API hộp thoại (lệnh không có bản dòng lệnh, như AutoCAD).
  6. Đạt DoD-C, DoD-H, DoD-L.

### CMD-044 · SELECTSIMILAR và thiết lập
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-002, DOC-017, INT-008, NT-005
- **Lệnh:** SELECTSIMILAR
- **Màn hình:** UI-E08
- **Đầu vào:** 03 §4; 07.1 §9.9; 07.2 UI-E08; sysvar SELECTSIMILARMODE (bit: màu 1, layer 2, linetype 4, tỉ lệ linetype 8, lineweight 16, plot style 32, kiểu đối tượng 64, tên 128; mặc định 130); prototype `screens6.js` (`similar`, `IMPL.SELECTSIMILAR`).
- **Đầu ra:** `commands/select/` (`selectsimilar_command.{h,cpp}`, `tests/golden/selectsimilar/`); `docs/specs/commands/SELECTSIMILAR.md`.
- **Tiêu chí nghiệm thu:**
  1. "Chọn đối tượng" `[Thiết lập · SE]` (nhận chọn trước); luôn so khớp loại đối tượng, cộng các thuộc tính bật trong SELECTSIMILARMODE; Thiết lập đọc và ghi SELECTSIMILARMODE cho UI-E08.
  2. "Kiểu đối tượng" so kiểu chữ, kiểu kích thước, kiểu bảng, kiểu multileader, kiểu MLINE; "Tên" so tên block (dynamic block theo tên hiệu dụng **[cần xác minh]**), xref, ảnh, underlay.
  3. Phạm vi: không gian hiện hành, bỏ layer đóng băng và layer tắt; trùng 100% bộ so tham chiếu đơn luồng trên 1.000 ca × 20 bản vẽ QA-001.
  4. 1 triệu entity ≤ 50 ms (tầm trung), ≤ 150 ms (thấp) **[đề xuất]**; hiệu suất song song ≥ 60% đến 8 luồng.
  5. Đạt KTL (§0) trên ≥ 30 ca vàng.
  6. Đạt DoD-C, DoD-H, DoD-L.

### CMD-045 · FILTER: bộ lọc chọn có nhóm logic, bộ lọc đã lưu
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-043, INT-008, NT-011, QA-006
- **Lệnh:** FILTER
- **Màn hình:** UI-E07
- **Đầu vào:** 03 §4 (FILTER là nền cho `ssget` có bộ lọc); 07.2 UI-E07; API INT-008 (bộ lọc kiểu `ssget`, toán tử nhóm `-4`); tệp bộ lọc có tên `filter.nfl` của AutoCAD **[cần xác minh định dạng]**; prototype `screens6.js` (`IMPL.FILTER`).
- **Đầu ra:**
  - `commands/select/filter/filter_list.{h,cpp}`: danh sách điều kiện (loại, thuộc tính, toán tử =, !=, <, <=, >, >=, *; nhóm AND, OR, XOR, NOT), chuyển sang bộ lọc `ssget` của INT-008.
  - `commands/select/filter/nfl_file.{h,cpp}`: đọc, ghi bộ lọc có tên; fuzz target.
  - `commands/select/filter/filter_command.{h,cpp}`: chạy thường hoặc trong suốt (`'FILTER` ở prompt "Chọn đối tượng"), "Thêm từ đối tượng chọn"; API cho UI-E07; `docs/specs/commands/FILTER.md`.
- **Tiêu chí nghiệm thu:**
  1. Danh sách điều kiện chuyển sang bộ lọc `ssget` cho kết quả trùng 100% đánh giá trực tiếp trên 1.000 bộ lọc ngẫu nhiên (lồng 4 cấp) × 10 bản vẽ QA-001.
  2. Nhóm sai (thiếu Kết thúc, NOT chứa hơn một điều kiện) bị báo trước khi áp dụng.
  3. `'FILTER` trong prompt chọn của lệnh khác trả tập đã lọc về đúng prompt đó.
  4. Đọc, ghi `filter.nfl` trùng từng bộ lọc; fuzz parser ≥ 1 giờ.
  5. Áp bộ lọc 10 điều kiện trên 1 triệu entity ≤ 100 ms (tầm trung) **[đề xuất]**, song song qua INT-008.
  6. Đạt KTL (§0) mục 2, 4, 6 trên ≥ 30 ca qua API hộp thoại.
  7. Đạt DoD-C, DoD-H, DoD-L.

### CMD-046 · GROUP, UNGROUP, GROUPEDIT
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-002, DOC-020, DWG-013, INT-008
- **Lệnh:** GROUP, UNGROUP, GROUPEDIT
- **Đầu vào:** 03 §4 (nhóm P1); 07.1 §7.2 (panel Nhóm), §9.9; 07.2 UI-E05 (trước 1.0 dùng GROUP trên dòng lệnh); DOC-020 (mô hình nhóm, từ điển ACAD_GROUP); prototype `screens13.js` (`IMPL.GROUP`, `IMPL.UNGROUP`, `IMPL.GROUPEDIT`).
- **Đầu ra:** `commands/select/group/` (`group_commands.{h,cpp}`, `tests/golden/`); `docs/specs/commands/GROUP.md`, `UNGROUP.md`, `GROUPEDIT.md`.
- **Tiêu chí nghiệm thu:**
  1. GROUP: "Chọn đối tượng hoặc" `[Tên · N / Mô tả · D]` (nhận chọn trước); tên kiểm trùng không phân biệt hoa thường, ký tự cho phép theo đặc tả **[cần xác minh]**, `?` liệt kê; không đặt tên thì tạo nhóm vô danh `*An`.
  2. UNGROUP: chọn nhóm (bấm một thành viên) hoặc `[Tên · N]`; có chọn sẵn thì rã mọi nhóm chứa đối tượng chọn. GROUPEDIT: chọn nhóm → `[Thêm đối tượng · A / Bớt đối tượng · R / Đổi tên · REN]`.
  3. Đối tượng thuộc nhiều nhóm: chọn nhóm theo quy tắc của đặc tả **[cần xác minh]**; xóa đối tượng thì tự rời nhóm; nhóm rỗng xử lý theo đặc tả.
  4. 300 bản vẽ có nhóm sinh lưu DWG, đọc lại trùng; DWG TrueView mở không lỗi.
  5. Đạt KTL (§0) trên ≥ 50 ca vàng.
  6. Đạt DoD-C, DoD-L.

### CMD-047 · -GROUP, CLASSICGROUP
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-046
- **Lệnh:** -GROUP, CLASSICGROUP
- **Màn hình:** UI-E05
- **Đầu vào:** 07.2 UI-E05 (danh sách, tạo nhóm, sửa nhóm); AutoCAD Command Reference (-GROUP; chỉ đối chiếu hành vi); prototype `screens6.js` (`IMPL.CLASSICGROUP`).
- **Đầu ra:** `commands/select/group/classic_group.{h,cpp}`: -GROUP dòng lệnh; API cho UI-E05 (danh sách kèm số đối tượng, hiện nhóm vô danh, tìm nhóm của đối tượng, tô sáng, tạo, thêm/bớt, đổi tên, sắp xếp lại, mô tả, rã, bật/tắt chọn được); ca vàng; `docs/specs/commands/-GROUP.md`, `CLASSICGROUP.md`.
- **Tiêu chí nghiệm thu:**
  1. -GROUP `[? / Sắp xếp · O / Thêm · A / Bớt · R / Rã · E / Đổi tên · REN / Chọn được · S / Tạo · C] <Tạo>` với prompt con như AutoCAD (Sắp xếp: vị trí cũ, vị trí mới, số đối tượng).
  2. Mỗi thao tác của UI-E05 chạy như một -GROUP, một mark undo.
  3. Sắp xếp lại giữ đủ phần tử, đúng thứ tự trên 1.000 ca ngẫu nhiên.
  4. Danh sách 10.000 nhóm cho UI-E05 ≤ 20 ms **[đề xuất]**.
  5. Đạt KTL (§0) trên ≥ 50 ca vàng.
  6. Đạt DoD-C, DoD-L.

### CMD-048 · PICKSTYLE: chọn theo nhóm, chọn hatch kèm biên
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-027, CMD-046, INT-008, INT-013
- **Lệnh:** PICKSTYLE
- **Đầu vào:** 03 §4; 07.1 §7.2 (nút bật/tắt chọn theo nhóm); sysvar PICKSTYLE (0–3), GROUPDISPLAYMODE (0–2); prototype `screens13.js` (`IMPL.PICKSTYLE`, `grpSel`).
- **Đầu ra:** `commands/select/group/pickstyle.{h,cpp}`: bộ mở rộng vùng chọn cho INT-008 (bấm hoặc khung trúng thành viên của nhóm chọn được thì chọn cả nhóm, Shift bớt cả nhóm; bit 2 chọn hatch liên kết kèm biên); hiển thị nhóm theo GROUPDISPLAYMODE (grip mọi đối tượng, một grip tâm, khung bao kèm một grip); nút bật/tắt; `docs/specs/commands/PICKSTYLE.md`.
- **Tiêu chí nghiệm thu:**
  1. Bấm nút trên ribbon đảo bit 1 của PICKSTYLE; gõ PICKSTYLE hỏi giá trị 0–3 như sysvar.
  2. Mở rộng vùng chọn đúng trên 1.000 ca (nhóm lồng, nhóm không chọn được, thành viên trên layer khóa); GROUPEDIT Thêm/Bớt tạm tắt mở rộng.
  3. Mở rộng vùng chọn 100.000 đối tượng thuộc 10.000 nhóm ≤ 10 ms (tầm trung) **[đề xuất]**.
  4. Đạt KTL (§0) trên ≥ 20 ca vàng.
  5. Đạt DoD-C, DoD-L.

## 12. Hiển thị

### CMD-049 · REGEN, REGENALL, REDRAW, REDRAWALL
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-001, RGN-003, RND-002
- **Lệnh:** REGEN, REGENALL, REDRAW, REDRAWALL
- **Đầu vào:** 03 §18, Phụ lục A; 07.1 §7.6; ngân sách RGN-028; API RGN-003 (regen toàn bộ theo viewport), RND-002 (yêu cầu khung hình); README §4.2 (CMD chỉ có REGEN, REDRAW, VIEWRES, VIEW; ZOOM, PAN thuộc INT-016); prototype `cmd.js` (`IMPL.REGEN`).
- **Đầu ra:** `commands/display/regen_commands.{h,cpp}`; ca vàng `commands/display/tests/golden/`; `docs/specs/commands/REGEN.md`, `REGENALL.md`, `REDRAW.md`, `REDRAWALL.md`.
- **Tiêu chí nghiệm thu:**
  1. REGEN tạo lại hình của viewport hiện hành, REGENALL của mọi viewport, báo "Đang tạo lại hình Model/Layout"; REDRAW, REDRAWALL xóa hình tạm (dấu ID, nhãn đo, khung chọn) và vẽ lại từ bộ đệm GPU, không regen.
  2. Có cờ Viewer (INT-020); dùng trong suốt (`'REDRAW`) theo đặc tả **[cần xác minh]**.
  3. REGEN qua lệnh trên cảnh 1 triệu entity đạt ngân sách RGN-028 (≤ 0,5 s 8 nhân, ≤ 2 s máy thấp); REDRAW ≤ 1 khung (≤ 16,6 ms).
  4. Đạt KTL (§0) trên ≥ 10 ca vàng.
  5. Đạt DoD-C, DoD-L.

### CMD-050 · VIEWRES, FILL
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-002, CMD-049, DOC-011, GEO-016
- **Lệnh:** VIEWRES, FILL
- **Đầu vào:** 03 §18, Phụ lục A; sysvar FILLMODE; dữ liệu phần trăm zoom đường tròn của VPORT và VIEWPORT (DOC-011); GEO-016 (sai số dây cung khi tessellation).
- **Đầu ra:** `commands/display/` (`viewres_command.{h,cpp}`, `fill_command.{h,cpp}`); `docs/specs/commands/VIEWRES.md`, `FILL.md`.
- **Tiêu chí nghiệm thu:**
  1. VIEWRES: "Zoom nhanh?" `[Có · Y / Không · N]` (giữ cho script, không đổi hành vi), "Phần trăm zoom đường tròn (1–20000)" lưu theo viewport; chỉ ảnh hưởng mật độ tessellation của đường cong không vẽ giải tích (GEO-016), không ảnh hưởng bản in.
  2. FILL `[ON / OFF]` đặt FILLMODE (biến header); TrionCAD regen ngay, không chờ REGEN như AutoCAD (khác biệt có chủ đích) **[đề xuất]**.
  3. Giá trị ngoài khoảng bị từ chối kèm thông báo; lưu DWG và đọc lại trùng.
  4. Đạt KTL (§0) trên ≥ 15 ca vàng.
  5. Đạt DoD-C, DoD-L.

### CMD-051 · VIEW, -VIEW: khôi phục view đã đặt tên
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-001, DOC-004, DOC-011, INT-016
- **Lệnh:** VIEW, -VIEW
- **Màn hình:** UI-D12
- **Đầu vào:** 03 §21 (VIEW P1); 07.1 §7.6 (V: chỉ khôi phục view); 07.2 UI-D12 (V: chỉ Đặt hiện hành); 07.3 UI-M01 (View đã lưu); bảng VIEW (DOC-004); API INT-016 (đổi view, lịch sử view); prototype `cmd.js` (`IMPL.VIEW`), `screens14.js` (`SC.viewManager`).
- **Đầu ra:** `commands/display/view/view_restore.{h,cpp}`: đọc bản ghi VIEW (tâm, chiều cao, chiều rộng, hướng, góc xoay, UCS gắn kèm, view Model hay Layout); khôi phục qua INT-016, kể cả chuyển layout; VIEW mở UI-D12 ở chế độ Đặt hiện hành; -VIEW `[? / Khôi phục · R]`; tùy chọn sửa ở Viewer báo theo 07.1 §2; `docs/specs/commands/VIEW.md`, `-VIEW.md`.
- **Tiêu chí nghiệm thu:**
  1. Khôi phục đúng tâm, chiều cao, góc xoay ± 1e-9 tương đối trên 500 view của bản vẽ QA-001 (Model và Layout); view có UCS gắn kèm đổi UCS theo đặc tả.
  2. ZOOM Previous sau khi khôi phục trả về view cũ (lịch sử INT-016).
  3. Khôi phục ≤ 1 khung khi không cần regen; chuyển cảnh mượt ≤ 150 ms và tắt được (README DoD-U mục 7).
  4. Viewer: không ghi gì vào tệp; Lưu, Xóa, Cửa sổ báo cần bản Pro.
  5. Đạt KTL (§0) trên ≥ 20 ca vàng.
  6. Đạt DoD-C, DoD-L.

### CMD-052 · VIEW, -VIEW (phần 1.0): lưu, xóa, cửa sổ, thiết lập, ảnh chụp layer
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-051, DOC-021, QA-004, CMD:LAYERSTATE
- **Màn hình:** UI-D12
- **Đầu vào:** 07.2 UI-D12 (cây view; thuộc tính tên, nhóm, UCS, ảnh chụp trạng thái layer, tỉ lệ chú thích, kiểu hiển thị, nền, khung bao; nút Mới, Cập nhật layer, Sửa khung bao, Xóa); AutoCAD Command Reference (-VIEW; chỉ đối chiếu hành vi); API trạng thái layer (CMD:LAYERSTATE).
- **Đầu ra:** `commands/display/view/view_edit.{h,cpp}`: -VIEW `[? / Xóa · D / Trực giao · O / Khôi phục · R / Lưu · S / Thiết lập · E / Cửa sổ · W]`, Thiết lập `[Nhóm · C / Ảnh chụp layer · L / UCS · U / Nền · B / Kiểu hiển thị · V]`; API cho UI-D12 (mới, cập nhật layer, sửa khung bao, xóa, đổi tên, nhóm); `VIEW.md`, `-VIEW.md` hoàn chỉnh.
- **Tiêu chí nghiệm thu:**
  1. Lưu view rồi lưu DWG: ODA File Converter đọc lại giữ đủ trường (QA-004); đọc lại trong TrionCAD trùng từng trường.
  2. Ảnh chụp layer lưu cùng view và khôi phục đúng trạng thái layer trên 300 ca.
  3. Mỗi thao tác của UI-D12 là một lệnh, một mark undo.
  4. Đạt KTL (§0) trên ≥ 40 ca vàng.
  5. Đạt DoD-C, DoD-L.

## 13. Truy vấn và đo

### CMD-054 · Báo cáo truy vấn có cấu trúc, TEXTSCR, GRAPHSCR
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** INT-003, INT-017, RGN-013
- **Lệnh:** TEXTSCR, GRAPHSCR
- **Màn hình:** UI-I02, UI-I03
- **Đầu vào:** 07.2 UI-I02 (thẻ kết quả: sao chép, đổi đơn vị mm ↔ m, m²), UI-I03 (kết quả LIST, MASSPROP, STATUS, TIME, DBLIST và lịch sử lệnh: tìm, sao chép, lưu tệp); 07.1 §11 (F2); API INT-017 (lịch sử dòng lệnh), RGN-013 (định dạng số theo LUNITS, LUPREC, AUNITS, AUPREC); prototype `cmd.js` (`resultCard`, `IMPL.TEXTSCR`).
- **Đầu ra:** `commands/inquiry/report/` (`report.{h,cpp}`: `InquiryReport` gồm các dòng nhãn và giá trị có kiểu độ dài, diện tích, góc, tọa độ, số, chuỗi; định dạng lúc hiển thị để UI đổi đơn vị; ghi vào lịch sử INT-017 dạng chữ kèm dữ liệu có cấu trúc; phân trang "Nhấn ENTER để tiếp tục"; lưu ra tệp văn bản; `textscr_command.{h,cpp}`); `docs/specs/commands/TEXTSCR.md`, `GRAPHSCR.md`.
- **Tiêu chí nghiệm thu:**
  1. Cùng một báo cáo cho chuỗi trên dòng lệnh, dữ liệu thẻ UI-I02 và nội dung cửa sổ UI-I03 khớp nhau; đổi đơn vị mm ↔ m, mm² ↔ m² không mất độ chính xác (giữ giá trị double gốc).
  2. Định dạng số qua RGN-013 trùng từng ký tự với chuỗi kỳ vọng trên 2.000 giá trị × 5 kiểu đơn vị.
  3. Phân trang theo đặc tả: bật khi tương tác; khi chạy script theo quy tắc của AutoCAD và QAFLAGS **[cần xác minh]**; ba đường nhập không lệch (KTL §0 mục 1).
  4. Ghi báo cáo 100.000 dòng vào lịch sử theo lô, không chặn luồng UI quá 16 ms mỗi lần **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.

### CMD-055 · DIST, ID
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-001, CMD-054, INT-010, INT-012
- **Lệnh:** DIST, ID
- **Màn hình:** UI-I02
- **Đầu vào:** 03 §16, Phụ lục A; 07.1 §2 (Viewer có đo); 07.2 UI-I02; sysvar DISTANCE, LASTPOINT; prototype `cmd.js` (`IMPL.DIST`, `IMPL.ID`).
- **Đầu ra:** `commands/inquiry/` (`dist_command.{h,cpp}`, `id_command.{h,cpp}`, `tests/golden/`); `docs/specs/commands/DIST.md`, `ID.md`.
- **Tiêu chí nghiệm thu:**
  1. DIST: "Điểm đầu" → "Điểm thứ hai" `[Nhiều điểm · M]`; Nhiều điểm `[Cung · A / Độ dài · L / Hoàn tác · U / Tổng · T] <Tổng>`, cộng dồn cả đoạn cung; báo khoảng cách, góc trong mặt XY, góc so với mặt XY, Delta X, Y, Z; đặt DISTANCE.
  2. ID: "Chỉ định điểm" → X, Y, Z; đặt LASTPOINT; ghim nhãn tọa độ (UI-I02).
  3. Giá trị đúng ± 1e-12 tương đối trên 10.000 ca, kể cả tọa độ VN-2000; chuỗi kết quả định dạng qua RGN-013.
  4. Nhãn đo khi rê (jig INT-012) ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp); kết quả sau điểm cuối ≤ 20 ms.
  5. Chạy được ở Viewer; `(command "DIST" …)` rồi `(getvar "DISTANCE")` cho đúng giá trị.
  6. Đạt KTL (§0) trên ≥ 30 ca vàng.
  7. Đạt DoD-C, DoD-L.

### CMD-056 · AREA: điểm, đối tượng, cộng và trừ diện tích
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-055, GEO-021, QA-008
- **Lệnh:** AREA
- **Màn hình:** UI-I02
- **Đầu vào:** 03 §16, Phụ lục A; 07.2 UI-E10 (dòng AREA: vùng đang đo tô nhạt, tổng cộng dồn), UI-I02; sysvar AREA, PERIMETER; API GEO-021; bộ LISP QA-008 (LISP diện tích dùng `(command "AREA" …)` rồi `(getvar "AREA")`); prototype `cmd.js` (`IMPL.AREA`).
- **Đầu ra:** `commands/inquiry/` (`area_command.{h,cpp}`; giao diện nguồn diện tích theo loại đối tượng để region đăng ký thêm ở CMD-021; `tests/golden/area/`); `docs/specs/commands/AREA.md`.
- **Tiêu chí nghiệm thu:**
  1. "Điểm góc đầu" `[Đối tượng · O / Cộng diện tích · A / Trừ diện tích · S] <Đối tượng>`; điểm kế `[Cung · A / Độ dài · L / Hoàn tác · U]`, từ điểm thứ ba thêm `[Tổng · T] <Tổng>`; chế độ Cộng hoặc Trừ có thêm `[Thoát · X]` và báo "Tổng diện tích" sau mỗi vùng.
  2. Đối tượng: đường tròn (báo chu vi đường tròn), ellipse, polyline (hở thì tính như đóng bằng đoạn thẳng, báo độ dài), spline; polyline có bề rộng tính theo đường tâm; hatch là mở rộng của TrionCAD **[đề xuất]**.
  3. Diện tích, chu vi đúng ± 1e-9 tương đối trên 10.000 ca (cung, spline, tọa độ VN-2000); đa giác tự cắt xử lý theo đặc tả **[cần xác minh]**.
  4. Đặt AREA, PERIMETER; 100% LISP tính diện tích trong QA-008 cho kết quả đúng.
  5. Vùng đang đo tô nhạt và tổng cập nhật khi rê (jig INT-012) ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp).
  6. Chạy được ở Viewer.
  7. Đạt KTL (§0) trên ≥ 50 ca vàng.
  8. Đạt DoD-C, DoD-L.

### CMD-057 · LIST
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-001, CMD-054, DOC-017, GEO-021, INT-008
- **Lệnh:** LIST
- **Màn hình:** UI-I03
- **Đầu vào:** 03 §16, Phụ lục A; 07.1 §7.2 (Liệt kê thông tin, có từ V); 07.2 UI-I03; API DOC-017 (thuộc tính chung và theo loại); AutoCAD Command Reference (thứ tự trường; chỉ đối chiếu); prototype `cmd.js` (`IMPL.LIST`).
- **Đầu ra:** `commands/inquiry/list/`:
  - `list_command.{h,cpp}`, `list_formatters.{h,cpp}`: khung chung theo DOC-017 (loại, layer, không gian, handle, thuộc tính khác ByLayer); phần riêng theo loại: độ dài, góc, delta của LINE; tâm, bán kính, chu vi, diện tích của CIRCLE, ARC; đỉnh, bulge, bề rộng, diện tích, chu vi của polyline; mẫu, diện tích, liên kết của HATCH; tên, điểm chèn, tỉ lệ, góc, attribute của INSERT; kiểu và giá trị đo của kích thước; chữ, kiểu, chiều cao của TEXT, MTEXT…
  - Ca vàng dạng ảnh chụp đầu ra `tests/golden/`; `docs/specs/commands/LIST.md`.
- **Tiêu chí nghiệm thu:**
  1. Nhận chọn trước; đầu ra vào cửa sổ văn bản UI-I03 qua CMD-054, thứ tự trường theo AutoCAD, có bản tiếng Việt và English.
  2. Định dạng riêng cho ≥ 30 loại entity thường gặp; loại còn lại in khung chung từ DOC-017, không bỏ sót đối tượng nào.
  3. Giá trị suy ra (độ dài, diện tích, góc) đúng ± 1e-9 tương đối; ảnh chụp đầu ra trùng 100% trên 300 đối tượng vàng.
  4. LIST 10.000 đối tượng: dòng đầu hiện ≤ 50 ms, phần còn lại đổ theo lô, không chặn UI quá 16 ms mỗi lần **[đề xuất]**.
  5. Chạy được ở Viewer.
  6. Đạt KTL (§0) trên ≥ 30 ca vàng.
  7. Đạt DoD-C, DoD-L.

### CMD-058 · Đo nhanh khi rê chuột (MEASUREGEOM, tùy chọn Nhanh)
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-004, CMD-054, INT-009, INT-012
- **Màn hình:** UI-I01
- **Đầu vào:** 07.2 UI-I01 (khoảng cách và góc quanh con trỏ dưới dạng kích thước tạm; bấm trong vùng kín cho diện tích và chu vi; Shift + bấm để cộng dồn); 07.1 §9.8; API INT-009 (truy vấn lân cận trên BVH), CMD-004 (vùng kín); prototype `cmd.js` (`IMPL.MEASUREGEOM`, `TC.cad.quick`).
- **Đầu ra:** `commands/inquiry/measure/quick_measure.{h,cpp}`: theo đường ngang và đường dọc qua con trỏ (theo UCS) tìm giao gần nhất ở mỗi phía trên đường cong lân cận (kể cả trong block, xref) và tạo kích thước tạm; góc giữa hai đường gặp nhau gần con trỏ; bán kính khi con trỏ nằm trên cung, đường tròn; bấm trong vùng kín cho diện tích, chu vi; Shift + bấm cộng dồn; dữ liệu hiển thị cho UI-I01 qua jig INT-012.
- **Tiêu chí nghiệm thu:**
  1. 1.000 vị trí vàng: các khoảng cách, góc, bán kính hiện ra trùng giá trị giải tích ± 1e-9 tương đối.
  2. Mỗi lần rê ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp) trên cảnh BENCH-STD 1 triệu entity ở mức zoom làm việc thường.
  3. Bấm trong vùng kín: diện tích, chu vi lấy từ CMD-004, trong ngân sách của CMD-004; cộng dồn cho đúng tổng trên 200 chuỗi.
  4. Bỏ qua nét mẫu hatch và chữ **[cần xác minh]**; chạy được ở Viewer.
  5. Đạt DoD-C, DoD-L.

### CMD-059 · MEASUREGEOM
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-055, CMD-056, CMD-058
- **Lệnh:** MEASUREGEOM
- **Màn hình:** UI-I01, UI-I02
- **Đầu vào:** 03 §16, Phụ lục A; 07.1 §7.2 (split Đo: Nhanh / Khoảng cách / Bán kính / Góc / Diện tích); 07.2 UI-E10 (dòng MEASUREGEOM), UI-I01, UI-I02; prototype `cmd.js` (`IMPL.MEASUREGEOM`).
- **Đầu ra:** `commands/inquiry/measure/` (`measuregeom_command.{h,cpp}`, `tests/golden/`); `docs/specs/commands/MEASUREGEOM.md`.
- **Tiêu chí nghiệm thu:**
  1. `[Khoảng cách · D / Bán kính · R / Góc · A / Diện tích · AR / Thể tích · V / Nhanh · Q / Chế độ · M / Thoát · X]`; sau mỗi kết quả quay lại prompt tùy chọn, mặc định là tùy chọn vừa dùng.
  2. Khoảng cách và Diện tích dùng lại DIST, AREA (cùng prompt con, cùng sysvar). Bán kính: chọn cung hoặc đường tròn → bán kính, đường kính. Góc: chọn cung, đường tròn (rồi điểm thứ hai trên đường tròn), line (rồi line thứ hai), hoặc chỉ đỉnh và hai điểm. Chế độ đặt chế độ mặc định khi mở lệnh **[cần xác minh]**. Thể tích báo chưa hỗ trợ (P3).
  3. Giá trị đúng ± 1e-9 tương đối trên 5.000 ca; macro của 5 mục split cho kết quả trùng gõ tay.
  4. Chạy được ở Viewer.
  5. Đạt KTL (§0) trên ≥ 50 ca vàng.
  6. Đạt DoD-C, DoD-L.

### CMD-060 · STATUS, TIME, DBLIST
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-057, DOC-010, DOC-018, NT-008, NT-013
- **Lệnh:** STATUS, TIME, DBLIST
- **Màn hình:** UI-I03
- **Đầu vào:** 03 §16 (P1); 07.2 UI-I03; biến header TDCREATE, TDUPDATE, TDINDWG, TDUSRTIMER, TDUCREATE, TDUUPDATE, LIMMIN, LIMMAX, EXTMIN, EXTMAX, INSBASE (DOC-010); sự kiện phiên DOC-018; NT-008 (RAM); NT-013 (tác vụ nền có hủy); AutoCAD Command Reference (chỉ đối chiếu hành vi).
- **Đầu ra:** `commands/inquiry/`:
  - `status_command.{h,cpp}`;
  - `time_command.{h,cpp}` và đồng hồ chỉnh sửa theo phiên (tích lũy TDINDWG, TDUSRTIMER; ghi vào header khi lưu qua sự kiện phiên DOC-018) **[đề xuất]**;
  - `dblist_command.{h,cpp}`; ca vàng; `docs/specs/commands/STATUS.md`, `TIME.md`, `DBLIST.md`.
- **Tiêu chí nghiệm thu:**
  1. STATUS: số đối tượng; giới hạn và phạm vi của Model hoặc Layout (đánh dấu khi vượt giới hạn); điểm chèn gốc, snap, lưới; không gian, layout, layer, màu, linetype, lineweight, cao độ, độ dày hiện hành; trạng thái FILL, GRID, ORTHO, QTEXT, SNAP; chế độ bắt điểm; bộ nhớ trống.
  2. TIME: giờ hiện tại, lúc tạo, lần cập nhật cuối, tổng thời gian chỉnh sửa, đồng hồ người dùng; `[Hiển thị · D / Bật · ON / Tắt · OFF / Đặt lại · R]`; thời gian chỉnh sửa đúng ± 1 s trên phiên mô phỏng 1 giờ.
  3. DBLIST: liệt kê mọi đối tượng theo thứ tự cơ sở dữ liệu, định dạng như LIST; 1 triệu entity chạy nền (NT-013), dòng đầu hiện ≤ 100 ms, có tiến trình, hủy ≤ 50 ms **[đề xuất]**.
  4. Chạy được ở Viewer **[đề xuất]**.
  5. Đạt KTL (§0) trên ≥ 20 ca vàng.
  6. Đạt DoD-C, DoD-H, DoD-L.

### CMD-061 · MASSPROP cho region
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-021, CMD-054, GEO-021, NT-011
- **Lệnh:** MASSPROP
- **Màn hình:** UI-I03
- **Đầu vào:** 03 §16 (P1); 07.2 UI-I03; API GEO-021 (diện tích, chu vi, trọng tâm, mômen quán tính); vòng biên region (CMD-021); AutoCAD Command Reference (thứ tự trường; chỉ đối chiếu).
- **Đầu ra:** `commands/inquiry/` (`massprop_command.{h,cpp}`, `tests/golden/massprop/`); `docs/specs/commands/MASSPROP.md`.
- **Tiêu chí nghiệm thu:**
  1. Chọn region (nhận chọn trước): diện tích, chu vi, hộp bao, trọng tâm, mômen quán tính, tích quán tính, bán kính quán tính, mômen chính và hướng quanh trọng tâm; region không nằm trên mặt XY của UCS báo theo đặc tả **[cần xác minh]**; solid 3D báo chưa hỗ trợ (P3).
  2. "Ghi kết quả ra tệp?" `[Có · Y / Không · N]` ghi tệp `.mpr`.
  3. Giá trị đúng ± 1e-9 tương đối so với công thức giải tích trên 1.000 region (chữ nhật, tròn, vành khuyên, có lỗ, có cạnh spline).
  4. Hoàn tất ≤ 20 ms (tầm trung), ≤ 50 ms (thấp) với region ≤ 1.000 cạnh.
  5. Đạt KTL (§0) trên ≥ 20 ca vàng.
  6. Đạt DoD-C, DoD-L.

## 14. Ràng buộc tham số (tab Tham số, sau v1.0)

### CMD-062 · Mô hình ràng buộc trong bản vẽ: tham chiếu hình học, tham số, biểu thức
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** DOC-005, DOC-007, DWG-018, QA-006
- **Màn hình:** UI-PAR
- **Đầu vào:** 03 §15 (12 ràng buộc hình học, ràng buộc kích thước, tham số, biểu thức; giữ nguyên ràng buộc khi mở, lưu là P1); 07.1 §7.5; 07.3 Phụ lục B (Đợt 5: tab Tham số); DWG-018 (giữ nguyên dữ liệu ràng buộc của AutoCAD); prototype `screens15.js` (`P`, `GT`, `DT`, bộ phân tích biểu thức).
- **Đầu ra:**
  - `commands/draw/constraints/model.{h,cpp}`: 12 loại ràng buộc hình học, 7 loại ràng buộc kích thước; tham chiếu tới điểm cuối, trung điểm, tâm, đỉnh thứ i, đoạn thứ i của entity; dạng động hoặc chú thích; chủ động hoặc tham chiếu.
  - `commands/draw/constraints/params.{h,cpp}`: tham số kích thước và tham số người dùng (tên, biểu thức, giá trị, mô tả), đồ thị phụ thuộc, phát hiện vòng.
  - `commands/draw/constraints/expr.{h,cpp}`: biểu thức (+ − × / ^, ngoặc, hằng pi và e, hàm lượng giác và lượng giác ngược, sqrt, abs, round, trunc, floor, ceil, min, max, exp, ln, log, d2r, r2d **[đối chiếu Command Reference]**); fuzz target.
  - Lưu vào bản vẽ **[cần xác minh: dạng associative framework của AutoCAD, chưa có schema ở DWG]**.
- **Tiêu chí nghiệm thu:**
  1. 5.000 biểu thức vàng đúng giá trị ± 1e-12 tương đối; lỗi cú pháp báo vị trí; fuzz parser ≥ 1 giờ.
  2. Vòng phụ thuộc giữa tham số bị từ chối, thông báo nêu chuỗi vòng; đổi một tham số cập nhật các tham số phụ thuộc theo thứ tự tô-pô, tất định.
  3. Xóa, sao chép đối tượng giữ tham chiếu đúng theo ánh xạ ID trên 1.000 ca.
  4. ≥ 50 bản vẽ AutoCAD có ràng buộc (QA-001): mở rồi lưu không sửa, dữ liệu ràng buộc trùng từng byte (DWG-018).
  5. Đạt DoD-C.

### CMD-063 · Cầu nối bộ giải GEO-024: giải khi sửa, bậc tự do, thừa và mâu thuẫn
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-062, GEO-024, DOC-008
- **Màn hình:** UI-PAR
- **Đầu vào:** 03 §15; API GEO-024; sysvar CONSTRAINTSOLVEMODE, CONSTRAINTRELAX **[đối chiếu Command Reference]**; prototype `screens15.js` (`sys`, `solve`, `post`, hộp thoại "Thừa ràng buộc", "Ràng buộc mâu thuẫn").
- **Đầu ra:** `commands/draw/constraints/solve_bridge.{h,cpp}`: dựng hệ từ thành phần liên thông chứa đối tượng bị sửa; giải khi thêm ràng buộc, đổi tham số, sau lệnh sửa hình (MOVE, STRETCH, ROTATE, SCALE, grip) trong cùng transaction; lệnh đổi tô-pô (TRIM, BREAK, FILLET…) bỏ ràng buộc không còn hợp lệ kèm thông báo; bậc tự do của từng đối tượng; tập ràng buộc xung đột tối thiểu khi thừa hoặc mâu thuẫn.
- **Tiêu chí nghiệm thu:**
  1. 10.000 hệ ngẫu nhiên giải được: mọi ràng buộc thỏa ± 1e-9 tương đối sau khi giải; lời giải tất định.
  2. Thừa và mâu thuẫn được báo đúng 100% trên 1.000 ca vàng; tập xung đột là tối thiểu (bỏ bất kỳ phần tử nào thì hết xung đột).
  3. Kéo grip đối tượng trong hệ 200 ràng buộc: giải ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp); thêm một ràng buộc ≤ 20 ms **[đề xuất]**.
  4. Undo một lệnh sửa trả cả hình học và tham số về trước lệnh trên 200 ca.
  5. Đạt DoD-C, DoD-L.

### CMD-064 · GEOMCONSTRAINT, các lệnh GC*, CONSTRAINTBAR, DELCONSTRAINT
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-063
- **Lệnh:** GEOMCONSTRAINT, GCCOINCIDENT, GCCOLLINEAR, GCCONCENTRIC, GCFIX, GCPARALLEL, GCPERPENDICULAR, GCHORIZONTAL, GCVERTICAL, GCTANGENT, GCSMOOTH, GCSYMMETRIC, GCEQUAL, CONSTRAINTBAR, DELCONSTRAINT
- **Màn hình:** UI-PAR
- **Đầu vào:** 03 §15; 07.1 §7.5; AutoCAD Command Reference (chỉ đối chiếu hành vi); prototype `screens15.js` (`IMPL.GEOMCONSTRAINT`, `gcRun`, `IMPL.CONSTRAINTBAR`, `IMPL.DELCONSTRAINT`).
- **Đầu ra:** `commands/draw/constraints/geom_commands.{h,cpp}`; dữ liệu thanh ràng buộc (biểu tượng theo đối tượng, hiện, ẩn) cho UI-PAR; ca vàng; `docs/specs/commands/GEOMCONSTRAINT.md` (gồm 12 lệnh GC*), `CONSTRAINTBAR.md`, `DELCONSTRAINT.md`.
- **Tiêu chí nghiệm thu:**
  1. GEOMCONSTRAINT `[Nằm ngang · H / Thẳng đứng · V / Vuông góc · P / Song song · PA / Tiếp xúc · T / Trơn · SM / Trùng điểm · C / Đồng tâm · CON / Thẳng hàng · COL / Đối xứng · S / Bằng nhau · E / Cố định · F]` và prompt chọn của từng lệnh GC* (2 điểm, Đối tượng, Nhiều, đường đối xứng); đối tượng thứ hai di chuyển, đối tượng thứ nhất đứng yên.
  2. Ràng buộc trùng lặp, không áp được cho loại đối tượng, thừa hoặc mâu thuẫn: báo theo CMD-063, bản vẽ không đổi.
  3. CONSTRAINTBAR `[Hiện · S / Ẩn · H / Đặt lại · R]`; DELCONSTRAINT xóa mọi ràng buộc của đối tượng chọn và báo số lượng.
  4. Sau mỗi lệnh, ràng buộc thỏa ± 1e-9 trên 10.000 ca.
  5. Đạt KTL (§0) trên ≥ 100 ca vàng, mỗi loại ràng buộc ≥ 6 ca.
  6. Đạt DoD-C, DoD-L.

### CMD-065 · Sysvar và thiết lập ràng buộc, CONSTRAINTSETTINGS
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** S · **Phụ thuộc:** CMD-062, INT-005, NT-010
- **Lệnh:** CONSTRAINTSETTINGS
- **Màn hình:** UI-PAR
- **Đầu vào:** 07.1 §7.5; prototype `screens15.js` (`CS0`, `settingsDlg`: loại hiện trên thanh, định dạng nhãn, thứ tự 9 loại tự ràng buộc, cờ "tiếp xúc, vuông góc phải có giao điểm", dung sai 0,05 và 1°); sysvar CONSTRAINTBARMODE, CONSTRAINTBARDISPLAY, CONSTRAINTNAMEFORMAT, CONSTRAINTINFER, DYNCONSTRAINTMODE, CCONSTRAINTFORM, DIMCONSTRAINTICON **[đối chiếu Command Reference]**.
- **Đầu ra:** `commands/draw/constraints/settings.{h,cpp}`: đăng ký các sysvar trên qua INT-005 (CONSTRAINTSOLVEMODE, CONSTRAINTRELAX thuộc CMD-063); thiết lập tự ràng buộc và độ trong suốt thanh theo người dùng (NT-010); lệnh mở UI-PAR ở tab Hình học, Kích thước hoặc Tự ràng buộc; `docs/specs/commands/CONSTRAINTSETTINGS.md`.
- **Tiêu chí nghiệm thu:**
  1. Mỗi sysvar: giá trị ngoài miền bị từ chối kèm thông báo; đổi bằng SETVAR, `(setvar)`, `.scr` phát đúng một sự kiện đổi; giữ đúng phạm vi (bản vẽ hoặc profile) sau lưu và mở lại.
  2. Thiết lập tự ràng buộc đọc lại đúng 100% sau khởi động lại và xuất/nhập profile; thứ tự ưu tiên hỏng (không là hoán vị của 9 loại) thì về mặc định và ghi log.
  3. Không có giao diện (`trion-console`, `.scr`): lệnh in thiết lập hiện hành, hash bản vẽ không đổi.
  4. Đạt KTL (§0) trên ≥ 10 ca vàng.
  5. Đạt DoD-C.

### CMD-066 · Ràng buộc kích thước: đo, dạng động và dạng chú thích, nhãn
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-063, CMD-065, RGN-013, RGN-014, RGN-015, QA-004
- **Màn hình:** UI-PAR
- **Đầu vào:** 03 §15; 07.1 §7.5 (thẳng, ngang, đứng, nghiêng, bán kính, đường kính, góc; chuyển dim thành ràng buộc); prototype `screens15.js` (`labelOf`, `convert`); API RGN-013, RGN-014, RGN-015.
- **Đầu ra:** `commands/draw/constraints/dim_constraint.{h,cpp}`:
  - đo 7 loại trên tham chiếu hình học của CMD-062; ràng buộc chủ động hoặc tham chiếu (chỉ đo, không vào hệ giải);
  - dạng động: cỡ cố định theo màn hình, không là entity, dữ liệu vẽ cho overlay của UI-PAR; dạng chú thích: DIMENSION theo kiểu kích thước hiện hành, in được, chữ là nhãn;
  - đổi dạng; chuyển kích thước liên kết thành ràng buộc.
- **Tiêu chí nghiệm thu:**
  1. Giá trị đo đúng ± 1e-12 tương đối so với công thức giải tích, 10.000 ca mỗi loại; ràng buộc góc chọn đúng góc phần tư như DIMANGULAR.
  2. Nhãn theo CONSTRAINTNAMEFORMAT (Tên, Giá trị, Tên=Biểu thức), số qua RGN-013 khớp từng ký tự với ca vàng; ràng buộc tham chiếu có ngoặc.
  3. Bật/tắt dạng động không đổi hash bản vẽ; dạng chú thích mở không hỏi recover trong DWG TrueView (QA-004) **[cần xác minh: cách AutoCAD lưu ràng buộc chú thích]**.
  4. Chuyển 200 kích thước liên kết ca vàng giữ vị trí đường kích thước, kiểu, giá trị; kích thước không liên kết bị từ chối kèm lý do.
  5. Dữ liệu vẽ cho 1.000 ràng buộc động ≤ 4 ms **[đề xuất]**.
  6. Đạt DoD-C, DoD-L.

### CMD-067 · DIMCONSTRAINT và các lệnh DC*
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-066
- **Lệnh:** DIMCONSTRAINT, DCLINEAR, DCHORIZONTAL, DCVERTICAL, DCALIGNED, DCANGULAR, DCRADIUS, DCDIAMETER, DCCONVERT, DCFORM
- **Màn hình:** UI-PAR
- **Đầu vào:** 03 §15; 07.1 §7.5; prototype `screens15.js` (`IMPL.DIMCONSTRAINT`, `IMPL.DCLINEAR`…); AutoCAD Command Reference (chỉ đối chiếu hành vi).
- **Đầu ra:** `commands/draw/constraints/dim_commands.{h,cpp}`; ca vàng; `docs/specs/commands/DIMCONSTRAINT.md` (gồm các lệnh DC*).
- **Tiêu chí nghiệm thu:**
  1. DIMCONSTRAINT: "Chọn kích thước cần chuyển hoặc `[Thẳng · LI / Ngang · H / Đứng · V / Nghiêng · AL / Góc · AN / Bán kính · R / Đường kính · D / Dạng · F]` <Nghiêng>"; chọn kích thước liên kết (nhận PICKFIRST, nhiều đối tượng) thì chuyển như DCCONVERT.
  2. DC*: hai điểm ràng buộc hoặc `[Đối tượng · O]`; DCALIGNED thêm `[Điểm và đường · P / Hai đường · 2L]`; DCANGULAR thêm `[Ba điểm · 3P]`; đặt đường kích thước; sửa nhãn tại chỗ, tên tự sinh d1, rad1, dia1, ang1; biểu thức lỗi giữ lệnh ở prompt nhãn **[đối chiếu Command Reference]**.
  3. DCFORM đặt CCONSTRAINTFORM (động hoặc chú thích) cho ràng buộc mới.
  4. Hình học đổi theo CMD-063 (đối tượng chọn sau di chuyển **[cần xác minh]**); thừa hoặc mâu thuẫn thì báo, bản vẽ không đổi.
  5. Đạt KTL (§0) trên ≥ 80 ca vàng, mỗi lệnh ≥ 6 ca; hoàn tất ≤ 20 ms (tầm trung), ≤ 50 ms (thấp) với hệ ≤ 200 ràng buộc.
  6. Đạt DoD-C, DoD-L.

### CMD-068 · DCDISPLAY
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** S · **Phụ thuộc:** CMD-002, CMD-066
- **Lệnh:** DCDISPLAY
- **Màn hình:** UI-PAR
- **Đầu vào:** 07.1 §7.5 (hiện/ẩn ràng buộc động); prototype `screens15.js` (`IMPL.DCDISPLAY`); sysvar DYNCONSTRAINTMODE.
- **Đầu ra:** `commands/draw/constraints/dcdisplay_command.{h,cpp}`: cờ ẩn theo ràng buộc, lưu trong bản vẽ; danh sách ràng buộc động cần vẽ, cập nhật theo vùng chọn, cho overlay của UI-PAR; `docs/specs/commands/DCDISPLAY.md`.
- **Tiêu chí nghiệm thu:**
  1. Chọn đối tượng (nhận PICKFIRST) rồi `[Hiện · S / Ẩn · H]` <Hiện>; Enter khi chưa chọn: `[Hiện tất cả · S / Ẩn tất cả · H]`; đối tượng không có ràng buộc kích thước thì báo, bản vẽ không đổi.
  2. DYNCONSTRAINTMODE = 1: ràng buộc đang ẩn vẫn hiện khi chọn đối tượng mang nó; = 0 thì không.
  3. Không đổi hình học; trạng thái giữ sau lưu và mở lại; một mark undo.
  4. Danh sách vẽ cập nhật ≤ 2 ms khi vùng chọn đổi, bản vẽ 10.000 ràng buộc **[đề xuất]**.
  5. Đạt KTL (§0) trên ≥ 20 ca vàng.
  6. Đạt DoD-C, DoD-L.

### CMD-069 · PARAMETERS, -PARAMETERS: tham số người dùng, sửa biểu thức
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-063, CMD-066, DOC-009
- **Lệnh:** PARAMETERS, -PARAMETERS
- **Màn hình:** UI-PAR
- **Đầu vào:** 03 §15 (quản lý tham số, biểu thức); 07.1 §7.5; prototype `screens15.js` (`IMPL.PARAMETERS`, palette `params`: nhóm Ràng buộc kích thước và Biến người dùng; cột Tên, Biểu thức, Giá trị); tham số và biểu thức của CMD-062.
- **Đầu ra:** `commands/draw/constraints/parameters_command.{h,cpp}`: PARAMETERS mở palette UI-PAR (không có giao diện thì chạy như -PARAMETERS); -PARAMETERS `[Mới · N / Sửa · E / Đổi tên · R / Xóa · D / ?]`; API đọc danh sách tham số trên snapshot DOC-009 cho palette; `docs/specs/commands/PARAMETERS.md`.
- **Tiêu chí nghiệm thu:**
  1. Mới: tên hợp lệ, không trùng tham số, hàm hoặc hằng; Sửa: đổi biểu thức, hình học giải lại trong cùng transaction; Đổi tên: cập nhật mọi biểu thức tham chiếu; Xóa: từ chối khi biểu thức khác đang dùng, nêu tên chúng; `?`: liệt kê theo mẫu ký tự đại diện.
  2. Biểu thức tạo vòng hoặc làm hệ mâu thuẫn: từ chối, bản vẽ không đổi, thông báo nêu chuỗi vòng hoặc ràng buộc xung đột.
  3. Đổi một tham số trong hệ 200 ràng buộc: giải và cập nhật ≤ 20 ms (tầm trung), ≤ 50 ms (thấp).
  4. API đọc 10.000 tham số ≤ 16 ms, không giữ khóa DB **[đề xuất]**.
  5. Đạt KTL (§0) trên ≥ 40 ca vàng.
  6. Đạt DoD-C, DoD-L.

### CMD-070 · AUTOCONSTRAIN
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-064, CMD-065, GEO-017
- **Lệnh:** AUTOCONSTRAIN
- **Màn hình:** UI-PAR
- **Đầu vào:** 03 §15; 07.1 §7.5; prototype `screens15.js` (`autoCands`, `IMPL.AUTOCONSTRAIN`); thiết lập tự ràng buộc của CMD-065.
- **Đầu ra:** `commands/draw/constraints/autoconstrain.{h,cpp}`: sinh ứng viên song song trên BVH (GEO-017), sắp theo (ưu tiên, handle, tham chiếu), áp lần lượt, bỏ ứng viên làm hệ thừa hoặc mâu thuẫn; `docs/specs/commands/AUTOCONSTRAIN.md`.
- **Tiêu chí nghiệm thu:**
  1. "Chọn đối tượng hoặc `[Thiết lập · S]`" (nhận PICKFIRST); kết thúc báo số ràng buộc đã áp và số đối tượng.
  2. 9 loại ứng viên theo dung sai khoảng cách và góc; loại đã tắt không áp; hai cờ giao điểm đúng; đúng 100% trên ≥ 200 ca vàng.
  3. Mọi ràng buộc đã áp thỏa ± 1e-9 sau khi giải; đối tượng ngoài vùng chọn giữ nguyên hash; kết quả trùng từng bit với 1, 8, 32 luồng.
  4. 100 đối tượng ≤ 20 ms; 10.000 đối tượng ≤ 1 s, hiệu suất song song ≥ 60% đến 8 luồng **[đề xuất]**.
  5. Đạt KTL (§0) trên ≥ 40 ca vàng.
  6. Đạt DoD-C, DoD-H, DoD-L.

### CMD-071 · CONSTRAINTINFER: suy luận ràng buộc khi vẽ và sửa
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-006, CMD-010, CMD-064, CMD-065, INT-010, CMD:FILLET, CMD:CHAMFER
- **Lệnh:** CONSTRAINTINFER
- **Màn hình:** UI-PAR
- **Đầu vào:** 03 §15; 07.1 §7.5 (nút suy luận trên thanh trạng thái); kết quả bắt điểm INT-010 của từng điểm nhập; AutoCAD Command Reference (CONSTRAINTINFER, các ràng buộc được suy luận; chỉ đối chiếu).
- **Đầu ra:** `commands/draw/constraints/infer.{h,cpp}`: CONSTRAINTINFER (0/1) gõ như lệnh; quy tắc: bắt Endpoint, Midpoint, Center, Node, Intersection → trùng điểm; Nearest → điểm nằm trên đường; Parallel → song song; Perpendicular → vuông góc; Tangent → tiếp xúc; quy tắc riêng của RECTANG (ngang, đứng, song song, vuông góc), FILLET (tiếp xúc, trùng điểm), CHAMFER (trùng điểm) **[đối chiếu Command Reference]**; ràng buộc thêm trong transaction của lệnh gốc; `docs/specs/commands/CONSTRAINTINFER.md`.
- **Tiêu chí nghiệm thu:**
  1. CONSTRAINTINFER = 0: mọi lệnh cho hash bản vẽ trùng với khi không nạp module ràng buộc.
  2. = 1: đúng 100% trên ≥ 300 ca vàng (mỗi quy tắc ≥ 10 ca); điểm nhập bằng tọa độ gõ tay, không bắt điểm thì không suy luận.
  3. `U` sau lệnh gỡ cả đối tượng lẫn ràng buộc suy luận (một mark).
  4. Chi phí suy luận ≤ 1 ms mỗi điểm nhập; lệnh vẫn hoàn tất ≤ 20 ms (tầm trung), ≤ 50 ms (thấp).
  5. Đạt KTL (§0) trên ≥ 30 ca vàng.
  6. Đạt DoD-C, DoD-L.

## 15. Làm sạch hình học và sức khỏe bản vẽ (sau v1.0)

### CMD-072 · Engine quét vấn đề hình học (TCCLEANUP phần 1)
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** GEO-002, GEO-010, GEO-017, DOC-009, NT-005, NT-013, QA-009
- **Màn hình:** UI-I08
- **Đầu vào:** 03 §17 (Geometry Cleanup: khe hở, đoạn thừa, góc lệch; thuật toán tất định); 07.2 UI-I08; prototype `screens18.js` (`GC.tol` mặc định: khe hở 5, thừa 50, hụt 50, góc 1°, trùng 0,01, độ dài 0,001; lọc layer hình học; `scan`: độ dài → trùng → góc → đầu mút hở).
- **Đầu ra:** `commands/draw/cleanup/scan.{h,cpp}` trên snapshot DOC-009, chạy nền NT-013:
  - đối tượng: line, polyline (đoạn thẳng và cung), cung, tròn, ellipse, spline trên layer đang hiện; phạm vi Model hoặc vùng chọn; lọc layer: hình học, tất cả, tự chọn;
  - 6 loại: khe hở; đoạn thừa (đầu vượt qua đường giao); đoạn hụt (đầu chưa chạm đường); góc lệch (đoạn lệch nhỏ so với 0°/90° của UCS, hai đoạn nối gần thẳng hàng); trùng (toàn phần, hoặc chồng một phần cùng phương, cùng layer); đoạn ngắn hơn dung sai;
  - mỗi vấn đề có loại, đối tượng, vị trí, độ lớn; mỗi đầu mút thuộc tối đa một vấn đề theo thứ tự khe hở → hụt → thừa.
- **Tiêu chí nghiệm thu:**
  1. ≥ 600 ca vàng (100 mỗi loại, gồm cung, tiếp xúc với đường tròn, đỉnh polyline, tọa độ VN-2000): phát hiện đủ, không báo sai.
  2. 1 triệu ca property-based (QA-009) cài sẵn vấn đề biết trước: tìm lại đủ và đúng loại.
  3. Danh sách sắp theo khóa ổn định (loại, handle, tham số), trùng từng bit với 1, 8, 32 luồng.
  4. BENCH-STD 1 triệu entity: ≤ 3 s trên máy tầm trung, hiệu suất song song ≥ 60% đến 8 luồng, có tiến trình, hủy ≤ 50 ms **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

### CMD-073 · Engine sửa vấn đề hình học, xem trước, danh sách bỏ qua (TCCLEANUP phần 2)
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-072, GEO-019, DOC-005, DOC-007, CMD:OVERKILL
- **Màn hình:** UI-I08
- **Đầu vào:** 07.2 UI-I08 (Sửa / Sửa tất cả / Bỏ qua); prototype `screens18.js` ("đã sửa N vấn đề trong một bước hoàn tác"); GEO-019 (cắt, kéo dài tới biên); lõi gộp đối tượng trùng của OVERKILL.
- **Đầu ra:** `commands/draw/cleanup/fix.{h,cpp}`:
  - quy tắc sửa: khe hở → kéo dài hai đầu tới giao điểm, không có giao điểm thì nối hai đầu tại trung điểm khe; thừa → cắt tại đường giao; hụt → kéo dài tới đường; góc lệch → xoay đoạn quanh đầu đã nối (hai đầu đều nối thì chỉ báo), gần thẳng hàng → bỏ đỉnh; trùng → giữ một bản, gộp phần chồng; đoạn ngắn → xóa và nối hai đầu kề **[đề xuất]**;
  - hình xem trước trước/sau cho lưới xem trước của UI-I08; danh sách bỏ qua trong xrecord của bản vẽ (DOC-005); nhật ký sửa.
- **Tiêu chí nghiệm thu:**
  1. Sửa tất cả trên 600 ca vàng của CMD-072: quét lại không còn vấn đề đã sửa, không sinh vấn đề mới; đối tượng không liên quan giữ nguyên hash.
  2. Sửa một mục hoặc Sửa tất cả là một transaction, một mark undo; `U` trả hash về trước.
  3. Hai sửa chạm cùng đối tượng áp theo thứ tự tất định; 100.000 ca property-based không sinh hình hỏng (tự giao mới, độ dài 0, polyline đứt đỉnh nối).
  4. Xem trước một mục ≤ 8 ms; Sửa tất cả 10.000 vấn đề ≤ 2 s, có tiến trình, hủy được trước khi ghi **[đề xuất]**.
  5. Mục bỏ qua không hiện lại sau lưu, mở lại và quét lại; xóa được danh sách bỏ qua.
  6. Đạt DoD-C, DoD-H, DoD-L.

### CMD-074 · Engine sức khỏe bản vẽ: chỉ số, điểm, sửa nhanh
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-072, DOC-013, DOC-014, DOC-015, DWG-016, RGN-006, NT-013, QA-001
- **Màn hình:** UI-I08
- **Đầu vào:** 03 §17 (kiểm tra sức khỏe bản vẽ); 07.2 UI-I08, trang Sức khỏe bản vẽ; prototype `screens18.js` (điểm trên 100 và mức).
- **Đầu ra:** `commands/inquiry/health/health_scan.{h,cpp}`:
  - chỉ số: regapp không dùng; block, layer, linetype, kiểu không dùng (DOC-014); đối tượng độ dài 0 (CMD-072); proxy (DWG-016); xref không phân giải (DOC-013); font bị thay (RGN-006); lỗi toàn vẹn (DOC-015, chỉ kiểm); dung lượng ước tính giảm được;
  - điểm 0–100 và 3 mức, trọng số ghi trong đặc tả **[đề xuất]**; mỗi mục kèm chuỗi lệnh sửa nhanh hoặc mã màn hình cần mở (UI-B09, UI-J02) **[đề xuất]**.
- **Tiêu chí nghiệm thu:**
  1. Số đếm đúng 100% trên 200 bản vẽ ca vàng đã đếm tay; quét không đổi hash bản vẽ.
  2. Dung lượng ước tính lệch ≤ 10% so với kích thước thật sau khi chạy các lệnh sửa nhanh rồi lưu, trên 100 bản vẽ QA-001 **[đề xuất]**.
  3. Bản vẽ sạch được 100 điểm; điểm tất định, mỗi trọng số có kiểm thử biên.
  4. BENCH-STD 1 triệu entity: ≤ 2 s ở nền, có tiến trình, hủy ≤ 50 ms **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

### CMD-075 · TCCLEANUP, -TCCLEANUP
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-073, CMD-074, NT-010
- **Lệnh:** TCCLEANUP, -TCCLEANUP
- **Màn hình:** UI-I08
- **Đầu vào:** 07.2 UI-I08; prototype `screens18.js` (`IMPL.TCCLEANUP`: vùng chọn sẵn thành phạm vi quét; tùy chọn sức khỏe; dừng quét giữ kết quả cũ; báo cáo `Lam-sach-hinh-hoc_<tên>`, `Suc-khoe_<tên>`).
- **Đầu ra:** `commands/draw/cleanup/tccleanup_command.{h,cpp}`: TCCLEANUP mở UI-I08 (không có giao diện thì chạy như -TCCLEANUP); -TCCLEANUP `[Quét · S / Sửa · F / Sửa tất cả · A / Bỏ qua · I / Thiết lập · T / Sức khỏe · H / Báo cáo · R]`; dung sai và bật/tắt từng loại theo người dùng (NT-010); báo cáo CSV UTF-8 **[đề xuất]**; `docs/specs/commands/TCCLEANUP.md`.
- **Tiêu chí nghiệm thu:**
  1. Có vùng chọn (PICKFIRST) thì quét vùng chọn, không thì Model; Esc khi đang quét dừng ≤ 50 ms và giữ kết quả lần trước.
  2. Mọi hành động sửa bản vẽ của UI-I08 (Sửa, Sửa tất cả, Bỏ qua) và đổi dung sai có chuỗi -TCCLEANUP tương đương, gửi qua INT-002.
  3. Mỗi lần Sửa hoặc Sửa tất cả đúng một mark undo; Quét, Sức khỏe, Báo cáo không đổi hash.
  4. Báo cáo khớp 100% danh sách của engine (loại, handle, layer, vị trí, độ lớn, trạng thái).
  5. Đạt KTL (§0) trên ≥ 60 ca vàng; cổng phiên bản: Pro **[đề xuất]**.
  6. Đạt DoD-C, DoD-L.

## 16. Lệnh vẽ P2 của tab Mở rộng, boolean region (sau v1.0)

### CMD-076 · BREAKLINE
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** S · **Phụ thuộc:** CMD-002, CMD-003, NT-010
- **Lệnh:** BREAKLINE
- **Màn hình:** UI-MR2
- **Đầu vào:** 03 §5, §28; 07.1 §7.10 (panel Vẽ); prototype `screens21.js` (`IMPL.BREAKLINE`, `bkMake`: ký hiệu zic zắc hoặc lượn sóng, cỡ mặc định 5 mm giấy × tỉ lệ chú thích, kéo dài hai đầu, ký hiệu mặc định ở trung điểm); tài liệu công khai của Express Tools (BREAKLINE).
- **Đầu ra:** `commands/draw/express/breakline_command.{h,cpp}`; thiết lập theo người dùng (NT-010); ca vàng; `docs/specs/commands/BREAKLINE.md`.
- **Tiêu chí nghiệm thu:**
  1. "Điểm đầu đường ngắt hoặc `[Ký hiệu · B / Cỡ · S / Kéo dài · E]`", "Điểm cuối", "Vị trí ký hiệu <Trung điểm>"; Ký hiệu: Zic zắc · Z, Lượn sóng · S, hoặc tên block có hai điểm nối như BRKLINE.DWG của Express **[cần xác minh]**.
  2. Kết quả là một LWPOLYLINE theo thuộc tính hiện hành; vị trí ký hiệu bị kẹp trong đoạn; đường ngắn hơn 1,2 × cỡ thì báo và hỏi lại điểm đầu.
  3. Cỡ ≤ 0 hoặc kéo dài < 0 bị từ chối; xem trước jig ≤ 8 ms/khung.
  4. Đạt KTL (§0) trên ≥ 30 ca vàng.
  5. Đạt DoD-C, DoD-L.

### CMD-077 · SUPERHATCH (phần 1): lát ô và cắt theo biên
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-004, GEO-015, DOC-005, DOC-012, DOC-020, RGN-021, RGN-024, QA-004
- **Màn hình:** UI-MR2
- **Đầu vào:** 07.1 §7.10 (hatch bằng ảnh, block hoặc xref); prototype `screens21.js` (`SH`: nguồn ảnh, block, xref, vùng che; ô rộng × cao, góc, khe, so le, tỉ lệ, làm mờ); vòng biên từ CMD-004 hoặc đối tượng kín; giao vùng GEO-015; tài liệu công khai của Express Tools (SUPERHATCH, sai số cong).
- **Đầu ra:** `commands/draw/express/superhatch_tiling.{h,cpp}`:
  - lưới ô theo gốc, góc, khe, so le; ô nằm trọn trong biên giữ nguyên, ô chạm biên bị cắt, đảo bị trừ;
  - IMAGE có biên clip, INSERT có clip không gian như XCLIP, WIPEOUT theo phần giao; biên cong xấp xỉ đa giác theo sai số cong (clip của IMAGE và XCLIP chỉ nhận đa giác);
  - gom thành GROUP vô danh, xdata giữ tham số để sửa lại; tối đa 10.000 ô, vượt thì hỏi **[đề xuất]**.
- **Tiêu chí nghiệm thu:**
  1. Hợp các ô sau cắt trùng vùng biên trừ đảo, lệch Hausdorff ≤ sai số cong, trên 2.000 vùng ngẫu nhiên (có cung, đảo lồng).
  2. Không ô nào lấn ra ngoài biên; ô có phần giao rỗng không được tạo.
  3. Mở không hỏi recover trong DWG TrueView (QA-004), ảnh, block, clip hiện đúng; lưu và mở lại giữ xdata.
  4. 1.000 ô ≤ 200 ms, chạy song song, kết quả tất định với mọi số luồng **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

### CMD-078 · SUPERHATCH (phần 2): lệnh, sửa lại, nghiệm thu
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-005, CMD-077, CMD:IMAGEATTACH, CMD:XATTACH
- **Lệnh:** SUPERHATCH
- **Màn hình:** UI-MR2
- **Đầu vào:** prototype `screens21.js` (`IMPL.SUPERHATCH`, `shDlg`: hộp chọn nguồn và tham số; chọn SuperHatch có sẵn để sửa; layer khóa thì từ chối); nạp ảnh, xref theo logic của IMAGEATTACH, XATTACH.
- **Đầu ra:** `commands/draw/express/superhatch_command.{h,cpp}`; ca vàng; `docs/specs/commands/SUPERHATCH.md`.
- **Tiêu chí nghiệm thu:**
  1. Có giao diện: lấy nguồn và tham số từ hộp của UI-MR2; chạy từ `.scr`, `(command)` hoặc CMDDIA = 0: prompt dòng lệnh `[Ảnh · I / Block · B / Xref · X / Vùng che · W / Chọn có sẵn · S]` rồi từng tham số **[đề xuất]**, cho cùng kết quả.
  2. Vùng: điểm bên trong (nhiều điểm) hoặc chọn đối tượng kín; vùng hở thì báo và đánh dấu chỗ hở theo CMD-005.
  3. Chọn SuperHatch có sẵn: sửa tham số, dựng lại ô trong cùng mark undo; nằm trên layer khóa thì từ chối, bản vẽ không đổi.
  4. Cả lệnh là một mark undo: `U` gỡ nhóm, ô và định nghĩa ảnh, xref vừa nạp.
  5. Đạt KTL (§0) trên ≥ 40 ca vàng, mỗi nguồn ≥ 8 ca.
  6. Đạt DoD-C, DoD-L.

### CMD-079 · MKSHAPE, SHAPE
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-003, DOC-004, RGN-004, VN-007, QA-004
- **Lệnh:** MKSHAPE, SHAPE
- **Màn hình:** UI-MR2
- **Đầu vào:** 03 §28; 07.1 §7.10 (Tạo shape); prototype `screens21.js` (`IMPL.MKSHAPE`, `IMPL.SHAPE`, `shapePlace`); AutoCAD Customization Guide, phần mô tả shape (tài liệu công khai); trình biên dịch SHX của VN-007; parser shape RGN-004.
- **Đầu ra:** `commands/draw/express/shape_encoder.{h,cpp}` (line, cung, tròn, polyline → mã vector, cung bát phân, cung theo bulge; chuẩn hóa theo độ phân giải); `mkshape_command.{h,cpp}` (ghi `.shp`, biên dịch `.shx`, nạp vào bảng STYLE); `shape_command.{h,cpp}`; `docs/specs/commands/MKSHAPE.md`, `SHAPE.md`.
- **Tiêu chí nghiệm thu:**
  1. MKSHAPE: tên tệp, tên shape (chữ không dấu, số, gạch nối, ≤ 31 ký tự), độ phân giải <128>, điểm gốc, chọn đối tượng; trùng tên thì hỏi ghi đè `[Có · Y / Không · N]` <Không>; số hiệu shape trong tệp không trùng.
  2. Shape đọc lại qua RGN-004 lệch Hausdorff ≤ cạnh lớn nhất / độ phân giải so với nét gốc, trên 2.000 hình ngẫu nhiên.
  3. Bản vẽ dùng `.shx` sinh ra mở trong DWG TrueView (QA-004) hiện cùng hình **[cần xác minh]**.
  4. SHAPE: "Tên shape hoặc [?]", điểm chèn, chiều cao, góc xoay; `?` liệt kê shape đã nạp; tên không có thì báo.
  5. Đạt KTL (§0) trên ≥ 30 ca vàng.
  6. Đạt DoD-C, DoD-L.

### CMD-080 · MKLTYPE
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-079, CMD:LINETYPE, RGN-011, NT-011, QA-005
- **Lệnh:** MKLTYPE
- **Màn hình:** UI-MR2
- **Đầu vào:** 03 §28; 07.1 §7.10 (Tạo linetype từ hình vẽ); prototype `screens21.js` (`IMPL.MKLTYPE`, `ltBuild`: nét trên đoạn mẫu → nét gạch, điểm → chấm, chữ và shape → phần tử nhúng; dung sai 1% chiều dài mẫu); AutoCAD Customization Guide, phần linetype tùy chỉnh (tài liệu công khai).
- **Đầu ra:** `commands/draw/express/mkltype_command.{h,cpp}`: dựng định nghĩa linetype phức từ đoạn mẫu, ghi thêm vào tệp `.lin` (giữ nội dung cũ, ghi an toàn NT-011), nạp qua LINETYPE; `docs/specs/commands/MKLTYPE.md`.
- **Tiêu chí nghiệm thu:**
  1. Prompt: tệp `.lin`, tên (≤ 31 ký tự), mô tả, điểm đầu và cuối đoạn mẫu, chọn đối tượng; trùng tên trong bản vẽ hoặc trong tệp thì từ chối kèm hướng dẫn.
  2. Line, polyline thẳng trên đoạn mẫu → nét gạch; khoảng trống → khoảng hở; POINT → chấm; TEXT → phần tử chữ (kiểu, S, R, X, Y); SHAPE → phần tử shape; đối tượng lệch khỏi đoạn mẫu quá dung sai bị bỏ và liệt kê.
  3. Đọc lại `.lin` bằng LINETYPE cho định nghĩa trùng; nét vẽ bằng linetype mới so với mẫu lặp lại đạt SSIM ≥ 0,99 (QA-005) trên 200 mẫu **[đề xuất]**.
  4. Ngắt ghi giả lập giữa chừng không làm hỏng `.lin` cũ.
  5. Đạt KTL (§0) trên ≥ 30 ca vàng.
  6. Đạt DoD-C, DoD-L.

### CMD-081 · SHP2BLK
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** S · **Phụ thuộc:** CMD-002, CMD-079, CMD:BLOCK
- **Lệnh:** SHP2BLK
- **Màn hình:** UI-MR2
- **Đầu vào:** 03 §28 (nhóm Công cụ); 07.1 §7.10; prototype `screens21.js` (`IMPL.SHP2BLK`: mọi shape cùng tên thành tham chiếu block, định nghĩa từ nét của shape ở chiều cao 1); tài liệu công khai của Express Tools (SHP2BLK).
- **Đầu ra:** `commands/draw/express/shp2blk_command.{h,cpp}`; ca vàng; `docs/specs/commands/SHP2BLK.md`.
- **Tiêu chí nghiệm thu:**
  1. "Chọn shape cần chuyển", "Tên block <tên shape>"; tên block đã có thì xử lý theo đặc tả **[cần xác minh]**; bản vẽ không có shape thì báo.
  2. Mọi SHAPE cùng tên trong Model, layout và định nghĩa block thành INSERT giữ vị trí, cỡ, góc, hệ số rộng, layer, màu; shape có độ xiên dùng định nghĩa block riêng theo góc xiên **[đề xuất]**; primitive hiển thị trùng trước khi chuyển ± 1e-9.
  3. Một mark undo; `U` trả lại các SHAPE.
  4. 10.000 shape ≤ 500 ms **[đề xuất]**.
  5. Đạt KTL (§0) trên ≥ 20 ca vàng.
  6. Đạt DoD-C, DoD-L.

### CMD-082 · UNION, SUBTRACT, INTERSECT cho region
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-002, CMD-021, CMD-032, GEO-015, GEO-021, QA-004
- **Lệnh:** UNION, SUBTRACT, INTERSECT
- **Đầu vào:** API GEO-015 (hợp, trừ, giao trên vùng có cạnh cong); ghi region của CMD-021; AutoCAD Command Reference (UNION, SUBTRACT, INTERSECT với region; chỉ đối chiếu hành vi).
- **Đầu ra:** `commands/draw/region/boolean_commands.{h,cpp}`; ca vàng; `docs/specs/commands/UNION.md`, `SUBTRACT.md`, `INTERSECT.md`.
- **Tiêu chí nghiệm thu:**
  1. UNION, INTERSECT: chọn ≥ 2 region (nhận PICKFIRST); SUBTRACT: chọn region bị trừ, Enter, chọn region đem trừ; region khác mặt phẳng xử lý theo từng nhóm đồng phẳng; 3DSOLID, BODY báo chưa hỗ trợ (P3).
  2. Mỗi nhóm cho một region (có thể nhiều vùng rời, có lỗ), giữ thuộc tính của đối tượng chọn đầu; kết quả rỗng xử lý theo đặc tả **[cần xác minh]**; cạnh cung giữ là cung.
  3. Diện tích qua GEO-021 thỏa |A∪B| + |A∩B| = |A| + |B| và |A−B| = |A| − |A∩B| ± 1e-9 tương đối, trên 10.000 ca ngẫu nhiên có cung, lỗ, tiếp xúc.
  4. Region kết quả mở không hỏi recover trong DWG TrueView (QA-004); hai region × 1.000 cạnh hoàn tất ≤ 20 ms (tầm trung), ≤ 50 ms (thấp).
  5. Đạt KTL (§0) trên ≥ 60 ca vàng.
  6. Đạt DoD-C, DoD-L.

## Tóm tắt theo mốc

| Mốc | Task | Số task |
|---|---|---|
| V | CMD-001, CMD-004, CMD-049, CMD-051, CMD-054, CMD-055, CMD-056, CMD-057, CMD-058, CMD-059 | 10 |
| EA | CMD-002, CMD-003, CMD-005, CMD-006, CMD-007, CMD-008, CMD-009, CMD-010, CMD-011, CMD-012, CMD-013, CMD-014, CMD-015, CMD-016, CMD-019, CMD-020, CMD-021, CMD-022, CMD-023, CMD-024, CMD-025, CMD-026, CMD-027, CMD-028, CMD-029, CMD-042, CMD-043, CMD-044, CMD-050 | 29 |
| 1.0 | CMD-017, CMD-018, CMD-030, CMD-031, CMD-032, CMD-033, CMD-034, CMD-035, CMD-036, CMD-037, CMD-038, CMD-039, CMD-040, CMD-041, CMD-045, CMD-046, CMD-047, CMD-048, CMD-052, CMD-060, CMD-061 | 21 |
| Sau | CMD-062, CMD-063, CMD-064, CMD-065, CMD-066, CMD-067, CMD-068, CMD-069, CMD-070, CMD-071, CMD-072, CMD-073, CMD-074, CMD-075, CMD-076, CMD-077, CMD-078, CMD-079, CMD-080, CMD-081, CMD-082 | 21 |
| **Tổng** | | **81** |
