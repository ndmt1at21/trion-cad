# CMD-2 · Lệnh sửa, hoàn tác, clipboard, tệp và tiện ích bản vẽ

> Phạm vi: lệnh sửa 2D (§1–§9: xóa, biến đổi, offset, cắt và kéo dài, bẻ và nối, bo và vát, phá khối, polyline, mảng, thuộc tính, thứ tự vẽ, làm sạch); hoàn tác, clipboard, lệnh tệp và tiện ích bản vẽ (§10 trở đi). Mã `CMD-151`…`CMD-299`.
> Thư mục mã (README §4.1): `commands/modify/` (ca vàng ở `commands/modify/tests/golden/<LỆNH>/`), `commands/file/`, `commands/utility/`. Đặc tả hành vi: `docs/specs/commands/<NAME>.md`.
> Căn cứ: [03](../03-chuc-nang-autocad.md) §6, §28, Phụ lục A–B; [05](../05-kien-truc-ky-thuat.md) §1, §7, §9.2, §10, §14; [07.1](../07-dac-ta-ui-1-khung-ung-dung.md) §7.2, §7.10, §9.5, §9.8; [07.2](../07-dac-ta-ui-2-bang-va-hop-thoai.md) UI-C10, UI-E02, UI-E04, UI-E10; prototype `design/prototype/js/draw.js`, `editor.js`.

## 0. Ghi chú thiết kế

- **Khung lệnh, đặc tả và KTL như [CMD-1 §0](core-cmd-1-ve-chon-truy-van.md).** Lệnh là coroutine INT-002; prompt và chip theo 07.2 UI-E10 qua INT-003; một lần chạy = một transaction DOC-007 = một mark DOC-008; Hoàn tác trong lệnh lùi bước con, không thêm mark. "Đạt KTL trên N ca vàng" theo đúng 8 điểm ở CMD-1 §0 (công cụ CMD-001, CMD-002): bàn phím, `.scr`, `(command ...)` trùng hash; một mark; Esc; PICKFIRST; chữ tắt tiếng Anh; cổng phiên bản; dịch +10⁹; khớp QA-007 khi có đáp án.
- **Hình học chỉ gọi kernel GEO:** biến đổi GEO-022, offset GEO-011/GEO-012, giao điểm GEO-010, bo/vát GEO-018, cắt/kéo dài GEO-019, nối đường cong GEO-167, phát hiện trùng GEO-168, spline GEO-023. Áp biến đổi lên entity trong transaction qua DOC-115 (transformBy). Lệnh không viết lại thuật toán đã có.
- **Bất biến bắt buộc** (05 §14, kiểm bằng QA-009): bản OFFSET cách nguồn đúng `d ± tol`; cung FILLET tiếp xúc cả hai đối tượng với bán kính `r`; TRIM không đổi phần còn lại; biến đổi rồi biến đổi ngược trả hình học trong dung sai GEO-002. Sai số ≤ 1e-9 × kích thước bản vẽ; tô-pô (số đoạn, thứ tự, chiều) trùng ca vàng (05 §1.2).
- **Ngân sách** (DoD-L): xem trước INT-012 ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp); hoàn tất lệnh trên đối tượng thường ≤ 20 ms / ≤ 50 ms. Biến đổi hàng loạt (MOVE, COPY, SCALE, mảng trên 100 nghìn đối tượng) tính song song trên snapshot DOC-009 qua NT-005, ghi một luồng (05 §9.2); ngưỡng ở CMD-152 **[đề xuất]**.
- **Xem trước không ghi DB.** Bóng vùng chọn của MOVE, ROTATE, SCALE, MIRROR, STRETCH, mảng vẽ lại primitive sẵn có với ma trận trên overlay, không regen mỗi khung **[đề xuất]**. Đoạn sẽ bị cắt, cung bo, bản offset là hình tạm của INT-012. Esc trả mọi thứ như cũ.
- **Grip nóng dùng lõi của lệnh.** Chế độ Di chuyển, Xoay, Tỉ lệ, Đối xứng của grip (INT-013) gọi cùng mã với MOVE, ROTATE, SCALE, MIRROR (CMD-159): cùng tham số cho cùng hash.
- **Tách lệnh lớn** như CMD-1 §0: chỉ task nghiệm thu cuối mang dòng **Lệnh:**; task con ghi tên lệnh trong tiêu đề.
- **LISP và script.** Bản `-` và lời gọi từ `.scr`/`(command)` không mở hộp thoại, prompt giữ như AutoCAD. Hành vi riêng khi gọi từ LISP (EXPLODE với QAFLAGS, TRIM/EXTEND khi TRIMEXTENDMODE = 1) ghi trong đặc tả, kiểm bằng QA-008 **[cần xác minh]**.
- **Lệnh tệp và tiện ích** (§10 trở đi): việc dài chạy nền qua NT-005, tiến trình và hủy qua NT-013, không chặn vùng vẽ; lưu nguyên tử từ snapshot DOC-009 (tệp tạm + đổi tên, NT-011); hộp thoại tệp theo FILEDIA; cổng phiên bản INT-020.
- **Giao diện ở tệp UI.** UI-C10, UI-E02, UI-E04 và chip UI-E10 thuộc các tệp `ui-*`. Task ở đây cung cấp engine, lệnh dòng lệnh, từ khóa và API thiết lập; UI đổi tham số bằng từ khóa hoặc sysvar, không ghi DB (DoD-U mục 5).

## 1. Nền chung của lệnh sửa

### CMD-151 · Khung lệnh sửa: chọn để sửa, lọc layer khóa, phạm vi sửa, báo cáo
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-002, INT-002, INT-003, INT-005, INT-008, DOC-007, DOC-008
- **Đầu vào:** §0; 07.2 UI-E10 (dòng SELECT); prototype `draw.js` `selEdit`, `IMPL.ERASE`; API INT-008 (chế độ chọn, PICKFIRST, tập Previous), DOC-007, DOC-008.
- **Đầu ra:**
  - `commands/modify/framework/`: `selectForEdit(ctx, filter)` (PICKFIRST, loại đối tượng trên layer khóa kèm thông báo số lượng, lọc theo không gian hiện hành); `EditScope` (transaction + mark, bước con cho Hoàn tác trong lệnh); đặt tập Previous, bỏ chọn sau lệnh; thông báo kết quả có số lượng.
  - Đăng ký `ktl_run` cho `commands/modify/tests/golden/`.
- **Tiêu chí nghiệm thu:**
  1. Lệnh thử `_KTLMOD` (đổi màu vùng chọn) đạt KTL trên 50 ca vàng.
  2. Đối tượng trên layer khóa bị loại 100%, thông báo đúng số lượng, hash của chúng không đổi.
  3. Hoàn tác trong lệnh lùi đúng một bước con trên 100 ca; số mark DOC-008 không đổi.
  4. Lọc vùng chọn sẵn 1 triệu đối tượng ≤ 30 ms tầm trung, song song qua NT-005 **[đề xuất]**.
  5. Đạt DoD-C, DoD-H, DoD-L.

### CMD-152 · Lõi biến đổi hàng loạt song song và bóng xem trước bằng ma trận
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** CMD-151, DOC-115, DOC-009, DOC-014, GEO-022, NT-005, NT-006, INT-012
- **Đầu vào:** 05 §9.2 (MOVE, SCALE 100 nghìn đối tượng), §7; API DOC-115 (transformBy trong transaction), DOC-014 (sao chép có ánh xạ ID), GEO-022, INT-012; BENCH-STD.
- **Đầu ra:** `commands/modify/xform/`: `planTransform(snapshot, ids, Matrix3d)` tính hình học mới song song trên snapshot (SoA, SIMD NT-006 cho mảng điểm); `commit(plan)` ghi một luồng qua DOC-115; `planCopy` (DOC-014 rồi biến đổi); `TransformGhost` vẽ primitive sẵn có của vùng chọn với ma trận trên overlay; benchmark `bench_bulk_xform`.
- **Tiêu chí nghiệm thu:**
  1. MOVE 100.000 đối tượng hỗn hợp (BENCH-STD) ≤ 150 ms tầm trung, ≤ 400 ms máy thấp; COPY cùng tập ≤ 300 ms tầm trung **[đề xuất]**; hiệu suất song song ≥ 60% đến 8 luồng.
  2. Bóng xem trước 100.000 đối tượng ≤ 8 ms/khung tầm trung, ≤ 16 ms máy thấp; thời gian CPU mỗi khung với 1.000 và 100.000 đối tượng chênh ≤ 1 ms **[đề xuất]**.
  3. Kết quả trùng từng bit giữa 1/4/16 luồng, giữa đường SIMD và scalar, trên 3 hệ điều hành.
  4. Biến đổi rồi biến đổi ngược trả hình học trong dung sai GEO-002 trên 1 triệu ca QA-009, kể cả tọa độ VN-2000.
  5. Bản sao giữ thuộc tính, xdata, extension dictionary: hash thuộc tính (trừ handle) trùng nguồn 100%.
  6. Đạt DoD-C, DoD-H, DoD-L.
- **Ngoài phạm vi:** quy tắc biến đổi từng loại entity (DOC-115); MIRRTEXT, MIRRHATCH (CMD-157).

### CMD-153 · Điểm kéo giãn theo loại entity cho STRETCH
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-007, DOC-115, GEO-005, GEO-006, GEO-007
- **Đầu vào:** 07.2 UI-E10 (STRETCH); prototype `draw.js` `stretchEnt`; API GEO-005/006/007, DOC-115 (dời nguyên entity).
- **Đầu ra:** `commands/modify/stretch/StretchPoints.h/.cpp`: `stretchPoints(entity)`, `moveStretchPoints(entity, mask, delta)` cho LINE, ARC, CIRCLE và ELLIPSE (tâm), cung ellipse, LWPOLYLINE, POLYLINE 2D/3D, SPLINE (điểm điều khiển hoặc điểm đi qua), TEXT, MTEXT, INSERT, ATTDEF, các DIMENSION (điểm định nghĩa, sinh lại qua regen), LEADER, MLEADER, HATCH không liên kết, SOLID, TRACE, WIPEOUT, POINT, RAY, XLINE; phần quy tắc theo loại trong `docs/specs/commands/STRETCH.md`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 20 ca vàng mỗi loại: mọi điểm trong cửa sổ thì dời nguyên entity; không điểm nào trong cửa sổ thì hash không đổi; 100%.
  2. Polyline giữ bulge, bề rộng, số đỉnh; spline giữ bậc và vector nút; cung giữ chiều cao dây cung **[cần xác minh]**; 100.000 ca QA-009 không NaN, không đổi tô-pô.
  3. Kích thước sau kéo giãn có giá trị đo bằng giá trị tính lại từ điểm định nghĩa, 100%.
  4. Đạt DoD-C.

## 2. Xóa, biến đổi và kéo giãn

### CMD-154 · ERASE, OOPS
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-151, DOC-014
- **Lệnh:** ERASE, OOPS
- **Đầu vào:** 03 §6; 07.2 UI-E10 (ERASE); prototype `draw.js` `IMPL.ERASE`; API DOC-014 (xóa, khôi phục).
- **Đầu ra:** `commands/modify/erase/`; API `oops::recordErased(ids)` cho lệnh xóa nguồn khác (BLOCK, WBLOCK); `docs/specs/commands/ERASE.md`, `OOPS.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 40 ca vàng mỗi lệnh (mọi chế độ chọn của INT-008, layer khóa, tập rỗng).
  2. OOPS khôi phục đúng tập xóa gần nhất của ERASE, BLOCK, WBLOCK kể cả sau lệnh khác không xóa: hash các đối tượng đó trùng trước khi xóa; OOPS là một mark, `U` sau OOPS xóa lại.
  3. ERASE ALL 1 triệu đối tượng ≤ 300 ms tầm trung **[đề xuất]**; ERASE thường ≤ 20 ms / ≤ 50 ms.
  4. Đạt DoD-C, DoD-H, DoD-L.

### CMD-155 · MOVE, COPY
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-152, INT-004
- **Lệnh:** MOVE, COPY
- **Đầu vào:** 03 §6; 07.2 UI-E10 (MOVE/COPY); prototype `draw.js` `IMPL.MOVE`, `IMPL.COPY`; API CMD-152 (qua đó DOC-115), INT-004 (Displacement, `@`, nhập khoảng cách trực tiếp); sysvar COPYMODE.
- **Đầu ra:** `commands/modify/move/`; `docs/specs/commands/MOVE.md`, `COPY.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 60 ca vàng mỗi lệnh: điểm gốc → điểm đích, Displacement, Enter dùng điểm đầu làm độ dời, nhập khoảng cách trực tiếp với ORTHO/POLAR, COPY Mode Single/Multiple, Array có và không Fit, Hoàn tác trong COPY.
  2. COPY Array n phần tử: bản thứ k cách nguồn k·v (Fit: k·v/(n−1)) trong dung sai GEO-002; handle mới cấp theo k, tất định.
  3. MOVE v rồi −v trả hình học trong dung sai trên 10.000 ca QA-009.
  4. ≤ 1.000 đối tượng: hoàn tất ≤ 20 ms / ≤ 50 ms; 100.000 đối tượng theo ngân sách CMD-152.
  5. Đạt DoD-C, DoD-L.

### CMD-156 · ROTATE, SCALE
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-152, INT-004
- **Lệnh:** ROTATE, SCALE
- **Đầu vào:** 03 §6 (Reference, Copy); 07.2 UI-E10; prototype `draw.js` `IMPL.ROTATE`, `IMPL.SCALE`; API CMD-152 (qua đó DOC-115); sysvar ANGDIR, ANGBASE.
- **Đầu ra:** `commands/modify/rotate/`, `commands/modify/scale/`; `docs/specs/commands/ROTATE.md`, `SCALE.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 60 ca vàng mỗi lệnh: góc hoặc hệ số gõ và chọn bằng điểm, Copy, Reference (hai điểm hoặc giá trị, mục tiêu gõ hoặc Points), ANGDIR/ANGBASE khác mặc định; text, kích thước, block lồng, hatch.
  2. Reference: góc hoặc độ dài sau lệnh bằng giá trị mục tiêu trong dung sai GEO-002, 100%.
  3. Xoay 4 lần 90° và tỉ lệ s rồi 1/s trả hình học trong dung sai trên 10.000 ca QA-009.
  4. Copy giữ nguồn không đổi (hash) và tạo đúng một bản.
  5. Hoàn tất ≤ 20 ms / ≤ 50 ms trên ≤ 1.000 đối tượng.
  6. Đạt DoD-C, DoD-L.

### CMD-157 · MIRROR với MIRRTEXT, MIRRHATCH
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-152, INT-004, RGN-007
- **Lệnh:** MIRROR
- **Đầu vào:** 03 §6; 07.2 UI-E10 (MIRROR); prototype `draw.js` `IMPL.MIRROR`; API CMD-152 (qua đó DOC-115), RGN-007 (khung bao chữ để giữ hướng đọc).
- **Đầu ra:** `commands/modify/mirror/`: hậu xử lý MIRRTEXT cho TEXT, MTEXT, ATTDEF, attribute biến, chữ kích thước; MIRRHATCH cho hướng mẫu hatch; `docs/specs/commands/MIRROR.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 60 ca vàng: Yes/No xóa nguồn, MIRRTEXT 0/1, MIRRHATCH 0/1; cung, ellipse, spline, polyline có bulge, block lồng có attribute, kích thước, hatch.
  2. MIRRTEXT = 0: 100% TEXT, MTEXT, ATTDEF, attribute biến không bị lật; chữ trong định nghĩa block và attribute hằng lật theo block như AutoCAD.
  3. Đối xứng hai lần qua cùng trục trả hình học trong dung sai, chiều cung và dấu bulge đúng, trên 1 triệu ca QA-009.
  4. Hoàn tất ≤ 20 ms / ≤ 50 ms trên ≤ 1.000 đối tượng.
  5. Đạt DoD-C, DoD-L.

### CMD-158 · STRETCH
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-152, CMD-153, INT-008
- **Lệnh:** STRETCH
- **Đầu vào:** 07.2 UI-E10 (STRETCH); prototype `draw.js` `IMPL.STRETCH`; API CMD-153, CMD-152 (qua đó DOC-115), INT-008 (Crossing, CPolygon).
- **Đầu ra:** `commands/modify/stretch/Stretch.cpp`; phần prompt của `docs/specs/commands/STRETCH.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 60 ca vàng: C, CP, chọn từng đối tượng (dời nguyên), Displacement; nhiều cửa sổ thì chỉ cửa sổ cuối quyết định điểm kéo **[cần xác minh]**.
  2. Đối tượng ngoài vùng chọn có hash không đổi; kéo +v rồi −v trả hình học trong dung sai.
  3. Xem trước phần bị kéo giãn ≤ 8 ms/khung tầm trung với 10.000 đối tượng bị kéo **[đề xuất]**.
  4. Hoàn tất ≤ 20 ms / ≤ 50 ms trên ≤ 1.000 đối tượng.
  5. Đạt DoD-C, DoD-L.

### CMD-159 · Chế độ grip nóng Di chuyển, Xoay, Tỉ lệ, Đối xứng
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** INT-013, CMD-155, CMD-156, CMD-157
- **Đầu vào:** 07.1 §9.5 (Space luân phiên; menu Điểm gốc, Sao chép, Tham chiếu, Hoàn tác, Thoát); prototype `editor.js` `IMPL.GRIPEDIT`, `gripMenu`; API INT-013.
- **Đầu ra:** `commands/modify/grips/GripModes.cpp`: đăng ký 4 chế độ với INT-013, gọi chung lõi của MOVE, ROTATE, SCALE, MIRROR; prompt `** MOVE **`, `** ROTATE **`, `** SCALE **`, `** MIRROR **` như AutoCAD; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. Mỗi chế độ cho hash trùng lệnh tương ứng cùng tham số trên 50 ca vàng (cả Base point, Copy, Reference).
  2. Space luân phiên đúng thứ tự 07.1 §9.5; Esc trả nguyên hash; nhiều bản sao trong một chế độ là một mark.
  3. Khung kéo ≤ 8 ms tầm trung, ≤ 16 ms máy thấp với 1.000 đối tượng nóng.
  4. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** chế độ Kéo giãn theo loại entity và grip đa năng (INT-013).

### CMD-160 · ALIGN
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-152, INT-004
- **Lệnh:** ALIGN
- **Đầu vào:** 03 §6; 07.1 §7.2 (panel Sửa mở rộng); API CMD-152 (qua đó DOC-115).
- **Đầu ra:** `commands/modify/align/`; `docs/specs/commands/ALIGN.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 40 ca vàng: 1 cặp điểm (dời), 2 cặp (dời, xoay, có/không tỉ lệ theo điểm), 3 cặp (xoay 3D, entity phẳng đổi pháp tuyến OCS).
  2. Điểm nguồn 1 trùng đích 1, hướng nguồn 1→2 trùng hướng đích trong dung sai GEO-002; có tỉ lệ thì khoảng cách cũng trùng; 100%.
  3. Hoàn tất ≤ 20 ms / ≤ 50 ms trên ≤ 1.000 đối tượng.
  4. Đạt DoD-C, DoD-L.

### CMD-161 · MOCORO, MSTRETCH
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-155, CMD-156, CMD-158
- **Lệnh:** MOCORO, MSTRETCH
- **Đầu vào:** 03 §28 (nhóm Hiệu chỉnh); 07.1 §7.10 (panel Sửa của tab Mở rộng); API CMD-155, CMD-156, CMD-158.
- **Đầu ra:** `commands/modify/express/`: MOCORO (dời, rồi Copy, Rotate, Scale, Base, Undo trong cùng lệnh); MSTRETCH (nhiều cửa sổ C/CP, Remove objects, rồi điểm gốc và điểm đích); `docs/specs/commands/MOCORO.md`, `MSTRETCH.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 30 ca vàng mỗi lệnh.
  2. MOCORO cho hash trùng chuỗi MOVE, COPY, ROTATE, SCALE tương đương, 100%.
  3. MSTRETCH với k cửa sổ rời nhau cho hash trùng k lần STRETCH cùng độ dời, 100%.
  4. Đạt DoD-C, DoD-L.

### CMD-162 · Dịch nhỏ vùng chọn bằng Ctrl+mũi tên
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** S · **Phụ thuộc:** CMD-155, UI0-015, INT-011, INT-016
- **Đầu vào:** 03 §6 (Ctrl+mũi tên); API UI0-015 (bảng phím KDEF), INT-011 (SNAP), INT-016 (cỡ pixel của view).
- **Đầu ra:** lệnh ẩn `_NUDGE` (không vào tự hoàn thành) gọi qua INT-002, dùng lõi MOVE; bốn phím trong bảng KDEF; `docs/specs/commands/NUDGE.md` **[đề xuất]**.
- **Tiêu chí nghiệm thu:**
  1. Không lệnh nào chạy và có vùng chọn: mỗi lần nhấn dời 2 pixel theo hướng màn hình; SNAP bật thì dời một bước SNAPUNIT; không có vùng chọn thì phím không làm gì.
  2. Mỗi lần nhấn là một mark undo **[cần xác minh]**; giữ phím 30 lần/s không trễ tích lũy; từ phím đến khung ≤ 16 ms tầm trung.
  3. Đạt DoD-C, DoD-L.

## 3. Offset

### CMD-163 · Dựng bản offset theo loại entity
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-011, GEO-012, DOC-014
- **Đầu vào:** 03 §6 (offset polyline có cung tự viết); 05 §7 (Offset); prototype `draw.js` `offsetEnt`; API GEO-011 (đường cong đơn, ellipse/spline xấp xỉ), GEO-012 (polyline, nhận OFFSETGAPTYPE).
- **Đầu ra:** `commands/modify/offset/OffsetBuilder.h/.cpp`: LINE → LINE, ARC → ARC (bán kính ≤ 0 thì không tạo), CIRCLE → CIRCLE, ELLIPSE và SPLINE → SPLINE, LWPOLYLINE/POLYLINE 2D → một hoặc nhiều polyline, RAY, XLINE; giữ thuộc tính, bề rộng, elevation, pháp tuyến; benchmark.
- **Tiêu chí nghiệm thu:**
  1. Mọi điểm mẫu trên kết quả cách nguồn đúng d: sai số ≤ 1e-9 × kích thước bản vẽ cho line, cung, polyline; ≤ 1e-6 × kích thước cho ellipse, spline **[đề xuất]**; 1 triệu ca QA-009.
  2. Polyline: không tự giao, đúng số mảnh, đoạn cung ra cung, trên 100 ca vàng.
  3. Polyline 10.000 đỉnh có cung ≤ 20 ms tầm trung, ≤ 50 ms máy thấp **[đề xuất]**.
  4. Đạt DoD-C, DoD-L.

### CMD-164 · OFFSET
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-163, CMD-151, INT-004, INT-012
- **Lệnh:** OFFSET
- **Đầu vào:** 07.2 UI-E10 (OFFSET); 07.1 §9.8; prototype `draw.js` `IMPL.OFFSET`; sysvar OFFSETDIST, OFFSETERASE, OFFSETGAPTYPE (0 = kéo dài đoạn, góc nhọn, mặc định AutoCAD; 1 = cung bo; 2 = vát).
- **Đầu ra:** `commands/modify/offset/Offset.cpp`; bản offset tạm theo phía con trỏ; `docs/specs/commands/OFFSET.md`; ca vàng.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 80 ca vàng: khoảng cách gõ hoặc hai điểm, Through, Erase, Layer (Current/Source), Multiple, Undo, Exit; OFFSETDIST nhớ giá trị (âm = Through); OFFSETERASE 0/1.
  2. OFFSETGAPTYPE 0, 1, 2 đều có ≥ 20 ca vàng polyline có góc lồi và lõm; kết quả khớp 100%.
  3. Bản tạm đúng phía con trỏ trên 100% ca; cập nhật ≤ 8 ms/khung tầm trung, ≤ 16 ms máy thấp với polyline 1.000 đỉnh.
  4. Hoàn tất ≤ 20 ms / ≤ 50 ms trên đối tượng thường.
  5. Đạt DoD-C, DoD-L.

## 4. Cắt và kéo dài

### CMD-165 · Tập biên cắt và kéo dài: biên chọn, biên ngầm, Project, Edge
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** GEO-010, GEO-019, DOC-009, DOC-012, INT-009, INT-015
- **Đầu vào:** 03 §6 (TRIM, EXTEND); 05 §7 (biên ngầm); API GEO-010 (giao hàng loạt), GEO-019 (phần kéo dài của biên), INT-009 (chỉ mục BVH của bản vẽ), INT-015 (UCS), DOC-012 (block lồng).
- **Đầu ra:** `commands/modify/trim/BoundarySet.h/.cpp`: dựng từ biên chọn hoặc mọi đối tượng thấy được (Quick, hoặc Standard với "chọn tất cả"), gồm đối tượng lồng trong block, xref qua ma trận; PROJMODE None/UCS/View; EDGEMODE Extend/No extend; truy vấn giao điểm đã sắp theo tham số cho một đường cong; cập nhật gia tăng khi eRase, Undo trong lệnh.
- **Tiêu chí nghiệm thu:**
  1. Giao điểm trùng đối chứng brute-force (không sót, không thừa) trên 1 triệu ca QA-009, kể cả tiếp xúc và trùng đoạn.
  2. Truy vấn giao của một đối tượng với biên ngầm trên cảnh 1 triệu entity ≤ 1 ms tầm trung, ≤ 3 ms máy thấp, không dựng lại chỉ mục **[đề xuất]**.
  3. PROJMODE 0/1/2, EDGEMODE 0/1 đúng trên 60 ca vàng; đối tượng lồng làm biên đúng trên 30 ca **[cần xác minh]**.
  4. Tất định trên 1/4/16 luồng.
  5. Đạt DoD-C, DoD-H, DoD-L.

### CMD-166 · Phép cắt và kéo dài theo loại entity
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** CMD-165, GEO-006, GEO-007, GEO-019, DOC-014
- **Đầu vào:** 05 §1.2 (tô-pô trùng bộ tham chiếu), §14; prototype `draw.js` `trimAt`; API CMD-165, GEO-019 (cắt theo tham số, kéo dài tới biên).
- **Đầu ra:** `commands/modify/trim/TrimOps.h/.cpp`: `trimAt(entity, pick, boundary)`, `extendAt(...)` cho LINE, ARC, CIRCLE (thành cung), ELLIPSE và cung ellipse, LWPOLYLINE, POLYLINE 2D/3D, SPLINE, RAY, XLINE (thành ray hoặc line); polyline rộng cắt theo đường tâm; kéo dài đoạn vát giữ độ vát, bề rộng âm thành 0; một mảnh giữ handle gốc, mảnh khác nhận handle theo thứ tự tham số.
- **Tiêu chí nghiệm thu:**
  1. TRIM không đổi phần còn lại: mọi điểm mẫu của phần giữ nằm trên đường cong gốc, đầu mút trùng giao điểm, sai số ≤ 1e-9 × kích thước bản vẽ; thuộc tính, xdata giữ nguyên; 1 triệu ca QA-009.
  2. EXTEND: đầu mới nằm trên biên trong cùng sai số, tiếp tuyến tại đầu giữ nguyên; đầu xa điểm chọn không đổi từng bit.
  3. Số đoạn, thứ tự, chiều trùng ca vàng trên 200 ca (polyline kín bị cắt thành hở, đường tròn thành cung).
  4. Đạt DoD-C.
- **Ngoài phạm vi:** cắt hatch và kích thước bằng TRIM (03 chưa ghi; ghi **[cần xác minh]** trong đặc tả).

### CMD-167 · TRIM, EXTEND (phần 1): chế độ Standard
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-166, CMD-151, INT-008, INT-012
- **Đầu vào:** 07.2 UI-E10 (TRIM/EXTEND); 07.1 §9.8; prototype `draw.js` `trimCmd`, `trimPreview`; API INT-008 (Fence, Crossing); sysvar PROJMODE, EDGEMODE.
- **Đầu ra:** `commands/modify/trim/TrimExtendCmd.cpp` chế độ Standard: chọn biên (Enter = tất cả); chọn đối tượng bằng điểm, Fence, Crossing; Shift+chọn đảo TRIM ↔ EXTEND; Project, Edge, eRase, Undo; đoạn sẽ bị xóa hiện mờ, đoạn kéo dài hiện tạm khi rê chuột; phần Standard của `docs/specs/commands/TRIM.md`, `EXTEND.md`.
- **Tiêu chí nghiệm thu:**
  1. 150 ca vàng Standard đạt KTL mục 1–4: mọi tùy chọn, Fence cắt nhiều đối tượng theo thứ tự dọc đường fence, Undo từng bước.
  2. Xem trước khi rê ≤ 8 ms/khung tầm trung, ≤ 16 ms máy thấp trên cảnh 1 triệu entity.
  3. Mỗi lần cắt hoặc kéo ≤ 20 ms / ≤ 50 ms trên đối tượng thường.
  4. Đạt DoD-C, DoD-L.

### CMD-168 · TRIM, EXTEND (phần 2): chế độ Quick và nghiệm thu lệnh
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-167
- **Lệnh:** TRIM, EXTEND
- **Đầu vào:** 03 §6 (Quick 2021+ và Standard); 07.2 UI-E10; sysvar TRIMEXTENDMODE.
- **Đầu ra:** chế độ Quick: biên ngầm, kéo đường tự do và Crossing; đối tượng không giao biên nào thì TRIM xóa cả đối tượng, EXTEND kéo tới biên gần nhất; mOde chuyển chế độ; hành vi khi gọi từ `.scr`/`(command)` ở TRIMEXTENDMODE = 1 **[cần xác minh]**; hoàn tất `TRIM.md`, `EXTEND.md`.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 300 ca vàng tổng (Quick và Standard, mọi tùy chọn UI-E10).
  2. Mọi LISP trong QA-008 gọi `(command "_.TRIM" ...)` hoặc `"_.EXTEND"` chạy đúng ở TRIMEXTENDMODE = 0 và 1.
  3. Kéo đường tự do qua 1.000 đối tượng: hoàn tất ≤ 50 ms tầm trung, ≤ 120 ms máy thấp **[đề xuất]**.
  4. Đạt DoD-C, DoD-L.

### CMD-169 · EXTRIM
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-166, CMD-151, INT-012
- **Lệnh:** EXTRIM
- **Đầu vào:** 03 §28; 07.1 §7.10; 07.2 UI-E10 (EXTRIM: chọn đường bao → phía cần cắt, phần sẽ cắt hiện mờ).
- **Đầu ra:** `commands/modify/express/Extrim.cpp`: biên là polyline, line, đường tròn, cung, ellipse, ảnh, text (khung chữ); cắt mọi đối tượng cắt qua biên ở phía chọn; `docs/specs/commands/EXTRIM.md`.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 40 ca vàng.
  2. Phần ở phía giữ lại không đổi (bất biến TRIM của CMD-166); đối tượng nằm trọn một phía không bị xóa **[cần xác minh]**.
  3. Biên kín cắt 10.000 đối tượng ≤ 200 ms tầm trung, song song NT-005 **[đề xuất]**.
  4. Đạt DoD-C, DoD-H.

## 5. Bẻ, nối và đổi độ dài

### CMD-170 · BREAK, BREAKATPOINT
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-151, GEO-019, DOC-014, INT-012
- **Lệnh:** BREAK, BREAKATPOINT
- **Đầu vào:** 07.2 UI-E10 (BREAK, BREAKATPOINT); prototype `draw.js` `IMPL.BREAK`, `IMPL.BREAKATPOINT`; API GEO-019 (cắt theo tham số).
- **Đầu ra:** `commands/modify/break/`: điểm chọn là điểm bẻ thứ nhất trừ khi chọn First point; `@` bẻ tại một điểm; đường tròn, ellipse bỏ phần ngược chiều kim đồng hồ từ điểm 1 đến điểm 2; XLINE thành hai RAY; BREAKATPOINT tạo hai đối tượng nối nhau; `docs/specs/commands/BREAK.md`, `BREAKATPOINT.md`.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 60 ca vàng mỗi lệnh (line, cung, tròn, ellipse, polyline kín và hở, spline, ray, xline).
  2. Phần còn lại nằm trên đường cong gốc, sai số ≤ 1e-9 × kích thước bản vẽ; tô-pô trùng ca vàng.
  3. BREAKATPOINT: hai mảnh chung đầu mút, tổng độ dài bằng độ dài gốc trong dung sai; đường tròn báo lỗi như AutoCAD.
  4. Hoàn tất ≤ 20 ms / ≤ 50 ms.
  5. Đạt DoD-C, DoD-L.

### CMD-171 · Nối chuỗi entity cho JOIN, PEDIT Join, FILLET, OVERKILL
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** GEO-167, DOC-014
- **Đầu vào:** 03 §6 (JOIN); API GEO-167 (nối đường cong có dung sai, fuzz, Jointype Extend/Add/Both), DOC-014.
- **Đầu ra:** `commands/modify/join/EntityJoin.h/.cpp`: entity → đường cong GEO và ngược lại; chọn loại entity kết quả (line thẳng hàng → LINE; cung cùng tâm, bán kính → ARC, hoặc CIRCLE khi cLose; có spline hoặc ellipse → SPLINE; line, cung, polyline → LWPOLYLINE, khác cao độ → POLYLINE 3D **[cần xác minh]**); thuộc tính, xdata và handle lấy từ đối tượng nguồn; danh sách đối tượng bị bỏ.
- **Tiêu chí nghiệm thu:**
  1. Loại entity kết quả đúng bảng quy tắc trên 150 ca vàng, 100%.
  2. Kết quả mang thuộc tính, xdata, handle của nguồn; đối tượng bị bỏ giữ nguyên hash.
  3. Nối 10.000 entity ≤ 20 ms tầm trung **[đề xuất]**; kết quả không phụ thuộc số luồng.
  4. Đạt DoD-C.
- **Ngoài phạm vi:** hình học nối và dung sai (GEO-167).

### CMD-172 · JOIN
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-171, CMD-151
- **Lệnh:** JOIN
- **Đầu vào:** 07.2 UI-E10 (JOIN); prototype `draw.js` `IMPL.JOIN`; API CMD-171.
- **Đầu ra:** `commands/modify/join/Join.cpp`: chọn nguồn hoặc nhiều đối tượng một lúc; cLose cho cung, cung ellipse; thông báo số đối tượng đã nối và bị bỏ; `docs/specs/commands/JOIN.md`.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 60 ca vàng (line thẳng hàng có khe, cung cùng tâm, cLose, hỗn hợp thành polyline, có spline).
  2. Thông báo số đã nối và bị bỏ đúng 100%.
  3. Hoàn tất ≤ 20 ms / ≤ 50 ms trên ≤ 1.000 đối tượng.
  4. Đạt DoD-C, DoD-L.

### CMD-173 · LENGTHEN
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-151, GEO-019, INT-012
- **Lệnh:** LENGTHEN
- **Đầu vào:** 07.2 UI-E10 (LENGTHEN); prototype `draw.js` `IMPL.LENGTHEN`; API GEO-019.
- **Đầu ra:** `commands/modify/lengthen/`: hiện độ dài (và góc ôm của cung) khi chọn; DElta (độ dài hoặc Angle), Percent, Total (độ dài hoặc Angle), DYnamic; đổi ở đầu gần điểm chọn; chọn lặp đến Enter, Undo; độ dài mới hiện tạm; `docs/specs/commands/LENGTHEN.md`.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 60 ca vàng (line, cung, polyline hở, cung ellipse, spline hở **[cần xác minh]**).
  2. Total L cho độ dài L; Percent p cho L·p/100; DElta Δ cho L + Δ; Angle cho đúng góc ôm; sai số ≤ 1e-9 × kích thước bản vẽ; đầu xa điểm chọn không đổi từng bit.
  3. Xem trước ≤ 8 ms/khung tầm trung; hoàn tất ≤ 20 ms / ≤ 50 ms.
  4. Đạt DoD-C, DoD-L.

## 6. Bo góc và vát góc

### CMD-174 · Áp bo góc và vát góc vào entity
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** GEO-018, GEO-019, CMD-171, DOC-014
- **Đầu vào:** 03 §6 (Polyline, Multiple, Trim); 05 §14; API GEO-018 (hình học bo và vát), GEO-019, CMD-171.
- **Đầu ra:** `commands/modify/fillet/CornerOps.h/.cpp`: chọn nghiệm theo hai điểm chọn; cắt hoặc kéo hai đối tượng theo TRIMMODE; line song song thành nửa đường tròn; bán kính hoặc khoảng bằng 0 thành góc nhọn; đường tròn không bị cắt; hai đoạn của cùng polyline thì chèn đoạn cung hoặc đoạn vát; polyline với line, cung thì nối vào polyline **[cần xác minh]**; tùy chọn Polyline áp mọi đỉnh và đếm đỉnh có đoạn quá ngắn.
- **Tiêu chí nghiệm thu:**
  1. FILLET: cung tiếp xúc cả hai đối tượng với bán kính r (khoảng cách tâm–đường cong = r, lệch tiếp tuyến tại tiếp điểm ≤ dung sai góc GEO-002) trên 1 triệu ca QA-009, kể cả ellipse, cung ellipse, spline (GEO-018).
  2. CHAMFER: hai đầu vát cách góc đúng d1, d2 hoặc đúng d và góc, sai số ≤ 1e-9 × kích thước bản vẽ.
  3. Phần ngoài vùng bo hoặc vát không đổi (bất biến TRIM); tô-pô polyline trùng 150 ca vàng.
  4. Đạt DoD-C.
- **Ngoài phạm vi:** CHAMFER với cung, ellipse, spline (AutoCAD cũng không cho).

### CMD-175 · FILLET
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-174, CMD-151, INT-012
- **Lệnh:** FILLET
- **Đầu vào:** 07.2 UI-E10 (FILLET/CHAMFER); 07.1 §9.8; prototype `draw.js` `IMPL.FILLET`; sysvar FILLETRAD, TRIMMODE.
- **Đầu ra:** `commands/modify/fillet/Fillet.cpp`: Undo, Polyline, Radius, Trim, Multiple; Shift+chọn đối tượng 2 dùng bán kính 0; cung bo tạm khi rê lên đối tượng 2; `docs/specs/commands/FILLET.md`.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 80 ca vàng (line, cung, tròn, polyline, line song song, bán kính 0, TRIMMODE 0/1, Multiple với Undo).
  2. Cung bo tạm ≤ 8 ms/khung tầm trung, ≤ 16 ms máy thấp; hoàn tất ≤ 20 ms / ≤ 50 ms.
  3. FILLETRAD, TRIMMODE lưu đúng phạm vi INT-005; giá trị mới dùng cho lần chạy sau.
  4. Đạt DoD-C, DoD-L.

### CMD-176 · CHAMFER
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-174, CMD-151, INT-012
- **Lệnh:** CHAMFER
- **Đầu vào:** 07.2 UI-E10; prototype `draw.js` `IMPL.CHAMFER`; sysvar CHAMFERA, CHAMFERB, CHAMFERC, CHAMFERD, CHAMMODE, TRIMMODE.
- **Đầu ra:** `commands/modify/fillet/Chamfer.cpp`: Undo, Polyline, Distance, Angle, Trim, mEthod, Multiple; Shift+chọn dùng khoảng 0; đoạn vát tạm khi rê; `docs/specs/commands/CHAMFER.md`.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 80 ca vàng (Distance và Angle, CHAMMODE 0/1, Polyline có đoạn quá ngắn, TRIMMODE 0/1, Multiple).
  2. Đoạn vát tạm ≤ 8 ms/khung tầm trung, ≤ 16 ms máy thấp; hoàn tất ≤ 20 ms / ≤ 50 ms.
  3. Đạt DoD-C, DoD-L.

## 7. Phá khối và polyline

### CMD-177 · EXPLODE (phần 1): block, polyline, attribute, đối tượng không phá được
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-151, DOC-012, DOC-014, GEO-022
- **Đầu vào:** 03 §6; prototype `draw.js` `IMPL.EXPLODE`; API DOC-012, DOC-014, GEO-022 (tỉ lệ không đều: tròn → ellipse); sysvar EXPLMODE.
- **Đầu ra:** `commands/modify/explode/`: INSERT phá một cấp (tỉ lệ không đều theo EXPLMODE, attribute thành ATTDEF); LWPOLYLINE, POLYLINE thành line và cung (mất bề rộng, có thông báo); MLINE; từ chối MINSERT, xref, block không cho phá kèm thông báo; giữ ByLayer, ByBlock như AutoCAD.
- **Tiêu chí nghiệm thu:**
  1. Hình học sau phá trùng primitive hiển thị trước khi phá (sai số ≤ 1e-9 × kích thước bản vẽ) trên 500 block lồng của QA-001.
  2. Block tỉ lệ không đều: tròn thành ellipse, cung thành cung ellipse đúng tham số trên 100 ca.
  3. Phá block 10.000 phần tử ≤ 50 ms tầm trung **[đề xuất]**.
  4. Đạt DoD-C.

### CMD-178 · EXPLODE (phần 2): kích thước, leader, MLEADER, MTEXT, bảng, hatch, region; nghiệm thu
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-177, CMD-021, RGN-009, RGN-012, RGN-014, RGN-015, RGN-016, RGN-017
- **Lệnh:** EXPLODE
- **Đầu vào:** API regen theo loại: RGN-009 (dàn trang MTEXT), RGN-012 (nét hatch), RGN-014/RGN-015 (kích thước), RGN-016 (leader), RGN-017 (bảng); CMD-021 (region).
- **Đầu ra:** phá kích thước, LEADER, MLEADER, bảng thành nội dung block đại diện; MTEXT thành TEXT theo đoạn định dạng; hatch thành nét mẫu; region thành line, cung, spline; mảng liên kết khi chưa có CMD-185 phá như block vô danh; từ `(command)` chỉ phá một đối tượng trừ khi QAFLAGS bật bit 1 **[cần xác minh]**; `docs/specs/commands/EXPLODE.md`.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 80 ca vàng (mọi loại của phần 1 và 2).
  2. Hình học sau phá trùng primitive hiển thị trước khi phá (sai số ≤ 1e-9 × kích thước bản vẽ); chữ giữ nội dung, font, vị trí.
  3. Mọi LISP trong QA-008 gọi EXPLODE chạy đúng.
  4. Hoàn tất ≤ 20 ms / ≤ 50 ms trên đối tượng thường.
  5. Đạt DoD-C, DoD-L.

### CMD-179 · PEDIT (phần 1): chuyển thành polyline, Close/Open, Join, Width, Edit vertex, Ltype gen, Reverse, Multiple
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** CMD-151, CMD-171, GEO-006, INT-012
- **Đầu vào:** 07.2 UI-E10 (PEDIT); prototype `draw.js` `IMPL.PEDIT`; API CMD-171 (Join, fuzz, Jointype), GEO-006; sysvar PEDITACCEPT, PLINEGEN.
- **Đầu ra:** `commands/modify/pedit/`: hỏi chuyển line, cung thành polyline (PEDITACCEPT); Close/Open, Join, Width, Ltype gen, Reverse, Undo, Multiple; Edit vertex đủ Next, Previous, Break, Insert, Move, Regen, Straighten, Tangent, Width, eXit, dấu đỉnh hiện hành trên overlay.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL mục 1–4 trên 120 ca vàng (LWPOLYLINE, POLYLINE 2D/3D, mọi tùy chọn và tùy chọn con của Edit vertex).
  2. Reverse hai lần trả hash ban đầu; Width đặt bề rộng đều mọi đoạn; Join cho hash trùng JOIN tương đương.
  3. Mỗi thao tác ≤ 20 ms / ≤ 50 ms trên polyline 10.000 đỉnh **[đề xuất]**.
  4. Đạt DoD-C, DoD-L.

### CMD-180 · PEDIT (phần 2): Fit, Spline, Decurve, LWPOLYLINE ↔ POLYLINE 2D; nghiệm thu
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-179, GEO-007, GEO-023
- **Lệnh:** PEDIT
- **Đầu vào:** 07.2 UI-E10 (Fit, Spline, Decurve; kết quả hiện tạm); sysvar SPLINETYPE, SPLINESEGS, SPLFRAME; hình học Fit, Spline, Decurve của GEO-023 (GEO-163), API GEO-007.
- **Đầu ra:** Fit (cung qua đỉnh, có hướng tiếp tuyến); Spline (B-spline bậc 2 hoặc 3 theo SPLINETYPE, SPLINESEGS đoạn, giữ khung điều khiển, SPLFRAME); Decurve trả khung; tự đổi LWPOLYLINE ↔ POLYLINE 2D khi cần, API dùng chung cho CONVERTPOLY; thuật toán đặt ở `commands/modify/pedit/` vì GEO chưa có neo cho nó **[đề xuất]**; `docs/specs/commands/PEDIT.md`.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 200 ca vàng PEDIT tổng (phần 1 và 2).
  2. Spline rồi Decurve, Fit rồi Decurve trả các đỉnh khung gốc từng bit.
  3. Đỉnh sinh bởi Spline trùng đánh giá B-spline của GEO-007 tại tham số tương ứng, sai số tương đối ≤ 1e-12 **[đề xuất]**.
  4. Kết quả tạm ≤ 8 ms/khung tầm trung với polyline 1.000 đỉnh.
  5. Đạt DoD-C, DoD-L.

### CMD-181 · MPEDIT, CONVERTPOLY, REVERSE
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-180, GEO-007
- **Lệnh:** MPEDIT, CONVERTPOLY, REVERSE
- **Đầu vào:** 03 §6, §28; 07.2 UI-E10 (MPEDIT như PEDIT); API CMD-179, CMD-180.
- **Đầu ra:** MPEDIT (PEDIT Multiple với prompt Express); CONVERTPOLY Light/Heavy; REVERSE cho line, polyline, spline (đảo thứ tự đỉnh, bulge, bề rộng, vector nút); `docs/specs/commands/MPEDIT.md`, `CONVERTPOLY.md`, `REVERSE.md`.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 40 ca vàng mỗi lệnh.
  2. CONVERTPOLY Light rồi Heavy (và ngược lại) giữ hình học, bề rộng, cao độ từng bit với polyline không có Fit, Spline.
  3. REVERSE hai lần trả hash ban đầu cho line, polyline; spline trong dung sai GEO-002.
  4. MPEDIT trên 10.000 polyline ≤ 100 ms tầm trung **[đề xuất]**.
  5. Đạt DoD-C.

## 8. Mảng

### CMD-182 · Engine mảng: bố cục chữ nhật và tròn, sinh phần tử song song
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-152
- **Đầu vào:** 07.2 UI-E02 (tham số mảng chữ nhật, tròn); API CMD-152 (sao chép, biến đổi hàng loạt).
- **Đầu ra:** `commands/modify/array/ArrayLayout.h/.cpp`: tham số → danh sách ma trận theo thứ tự hàng, cột, cấp tất định (chữ nhật: số và khoảng hàng, cột, góc trục, cao độ tăng dần; tròn: tâm, số phần tử, góc giữa, góc phủ có dấu, xoay phần tử); `buildNonAssoc()` sinh bản sao qua CMD-152; benchmark.
- **Tiêu chí nghiệm thu:**
  1. Phần tử (i, j) cách gốc đúng i·dy, j·dx theo trục; phần tử tròn thứ k ở góc k·θ, xoay k·θ khi bật; trong dung sai GEO-002 trên 100.000 ca QA-009.
  2. 10.000 bản sao của nhóm 10 đối tượng ≤ 100 ms tầm trung **[đề xuất]**; hiệu suất song song ≥ 60% đến 8 luồng.
  3. Thứ tự handle tất định trên 1/4/16 luồng.
  4. Đạt DoD-C, DoD-H.

### CMD-183 · ARRAYRECT, ARRAYPOLAR, ARRAY: mảng không liên kết
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-182, CMD-151, INT-012
- **Lệnh:** ARRAYRECT, ARRAYPOLAR, ARRAY
- **Màn hình:** UI-E02
- **Đầu vào:** 03 §6 (P0 không liên kết); 07.2 UI-E02, UI-E10 (ARRAY*); sysvar ARRAYASSOCIATIVITY.
- **Đầu ra:** `commands/modify/array/`: ARRAYRECT với ASsociative, Base point, COUnt, Spacing, COLumns, Rows, Levels (Sau), eXit; ARRAYPOLAR với Base point, Items, Angle between, Fill angle, ROWs, ROTate items, eXit; ARRAY hỏi loại rồi chuyển lệnh; mảng tạm khi đổi tham số; API tham số cho tab ngữ cảnh; `docs/specs/commands/ARRAYRECT.md`, `ARRAYPOLAR.md`, `ARRAY.md`.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 80 ca vàng; trước khi có CMD-185, ASsociative = Yes tạo mảng không liên kết và báo một lần **[đề xuất]**.
  2. Đổi tham số qua tab ngữ cảnh cho hash trùng dòng lệnh cùng tham số, 100%.
  3. Mảng tạm 1.000 phần tử ≤ 8 ms/khung tầm trung, ≤ 16 ms máy thấp **[đề xuất]**.
  4. Đạt DoD-C, DoD-L.

### CMD-184 · ARRAYCLASSIC, -ARRAY: mảng cổ điển
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-182, CMD-151
- **Lệnh:** ARRAYCLASSIC, -ARRAY
- **Màn hình:** UI-E02
- **Đầu vào:** 03 §6; 07.2 UI-E02 (hộp thoại cổ điển: chữ nhật hoặc tròn, Chọn đối tượng, Xem trước, luôn không liên kết); sysvar SNAPANG.
- **Đầu ra:** `commands/modify/array/Classic.cpp`: API tham số và xem trước cho hộp thoại; `-ARRAY` với prompt cổ điển (Rectangular/Polar, ô đơn vị, góc phủ có dấu, xoay phần tử), mảng chữ nhật theo SNAPANG; `docs/specs/commands/ARRAYCLASSIC.md`, `-ARRAY.md`.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 60 ca vàng `-ARRAY`, gồm dạng LISP cũ `(command "_.-ARRAY" ss "" "_R" 3 4 10 20)`.
  2. Cùng tham số, hộp thoại và `-ARRAY` cho hash trùng nhau, 100%.
  3. Đạt DoD-C.

### CMD-185 · Mảng liên kết: mô hình, đọc/ghi, đánh giá lại
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** CMD-182, DOC-005, DOC-012, DWG-014
- **Đầu vào:** 07.2 UI-E02 (liên kết, 1.0); mảng liên kết trong DWG do AutoCAD tạo (QA-001); API DOC-012 (block vô danh), DOC-005 (dictionary, reactor), lớp đối tượng liên kết mảng của DWG-014.
- **Đầu ra:** `commands/modify/array/assoc/`: đọc tham số mảng chữ nhật, tròn, theo đường từ mạng liên kết của bản vẽ; đánh giá lại thành block vô danh khi tham số hoặc nguồn đổi; ghi lại để AutoCAD nhận là mảng liên kết; ghi đè phần tử (xóa, thay); bật ASsociative cho CMD-183; phá mảng thành phần tử cho EXPLODE.
- **Tiêu chí nghiệm thu:**
  1. 200 mảng liên kết của QA-001 mở, đánh giá lại, lưu: hình học phần tử trùng bản gốc trong dung sai GEO-002.
  2. Mảng do TrionCAD tạo hoặc sửa mở trong AutoCAD vẫn là mảng liên kết sửa được (QA-007), 100%.
  3. Round-trip mảng không sửa trùng từng trường (QA-003).
  4. Đánh giá lại mảng 10.000 phần tử ≤ 50 ms tầm trung **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

### CMD-186 · ARRAYPATH
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-185, CMD-151, GEO-006, GEO-007, INT-012
- **Lệnh:** ARRAYPATH
- **Màn hình:** UI-E02
- **Đầu vào:** 07.2 UI-E02 (Mảng theo đường), UI-E10 (ARRAY*); API GEO-006, GEO-007 (tham số theo độ dài cung).
- **Đầu ra:** `commands/modify/array/Path.cpp`: đường dẫn là line, cung, tròn, ellipse, polyline, spline; Method Divide/Measure, Base point, Tangent direction, Items, Rows, Levels (Sau), Align items, Z direction, ASsociative, eXit; mảng tạm; `docs/specs/commands/ARRAYPATH.md`.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 60 ca vàng.
  2. Phần tử k ở độ dài cung k·s dọc đường (Divide: chia đều toàn chiều dài), sai số ≤ 1e-9 × kích thước bản vẽ; Align items: góc phần tử bằng góc tiếp tuyến cộng góc ban đầu.
  3. Mảng tạm 1.000 phần tử trên spline ≤ 8 ms/khung tầm trung **[đề xuất]**.
  4. Đạt DoD-C, DoD-L.

### CMD-187 · ARRAYEDIT, ARRAYCLOSE và grip mảng liên kết
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-185, CMD-186, INT-013
- **Lệnh:** ARRAYEDIT, ARRAYCLOSE
- **Màn hình:** UI-E02
- **Đầu vào:** 07.2 UI-E02 (Sửa nguồn, Thay phần tử, Đặt lại mảng; grip đổi khoảng cách, số lượng); 07.1 §9.5 (grip mảng liên kết); API INT-013 (đăng ký grip).
- **Đầu ra:** ARRAYEDIT: Source (sửa nguồn tại chỗ, ARRAYCLOSE lưu hoặc bỏ), REPlace, Base point, Rows, Columns, Levels, Items, RESet, eXit; Ctrl+chọn để xóa hoặc thay một phần tử; grip gốc, số lượng, khoảng cách đăng ký với INT-013; `docs/specs/commands/ARRAYEDIT.md`, `ARRAYCLOSE.md`.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 60 ca vàng.
  2. Kéo grip trên mảng 10.000 phần tử cập nhật trực tiếp ≤ 8 ms/khung tầm trung, ≤ 16 ms máy thấp **[đề xuất]**.
  3. Sửa nguồn rồi ARRAYCLOSE lưu: mọi phần tử đổi theo; bỏ: hash trở về trước khi sửa; RESet khôi phục phần tử đã xóa hoặc thay.
  4. Mỗi lần sửa là một mark undo.
  5. Đạt DoD-C, DoD-L.

## 9. Thuộc tính, không gian, thứ tự vẽ và làm sạch

### CMD-188 · MATCHPROP và thiết lập Khớp thuộc tính
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-151, DOC-017, INT-012, NT-010
- **Lệnh:** MATCHPROP
- **Màn hình:** UI-C10
- **Đầu vào:** 07.2 UI-C10, UI-E10 (MATCHPROP); 07.1 §9.8 (đích đổi thuộc tính tạm khi rê); API DOC-017 (đọc/ghi thuộc tính chung), NT-010 (lưu thiết lập).
- **Đầu ra:** `commands/modify/matchprop/`: bảng ánh xạ theo cặp loại nguồn–đích cho thuộc tính cơ bản (màu, layer, linetype, tỉ lệ linetype, độ dày nét, độ trong suốt, độ dày đùn, plot style) và đặc biệt (kích thước, chữ, hatch, polyline, viewport, bảng, multileader, đường tâm); Settings mở UI-C10, gọi từ `.scr`/`(command)` thì giữ thiết lập hiện tại, không mở hộp thoại **[đề xuất]**; API thiết lập cho UI-C10; xem trước khi rê; `docs/specs/commands/MATCHPROP.md`.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 120 ca vàng: mỗi thuộc tính bật thì chép, tắt thì giữ nguyên (hash từng thuộc tính).
  2. Thuộc tính không áp được cho loại đích thì giữ nguyên, 100%.
  3. Xem trước khi rê ≤ 8 ms/khung tầm trung, ≤ 16 ms máy thấp; Esc để hash không đổi.
  4. Áp lên 10.000 đích ≤ 50 ms tầm trung **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.

### CMD-189 · CHANGE, CHPROP, SETBYLAYER
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-151, DOC-017, DOC-012, INT-004
- **Lệnh:** CHANGE, CHPROP, SETBYLAYER
- **Đầu vào:** 03 §6 (thường gọi từ LISP); 07.1 §7.2 (Đặt về ByLayer); 07.2 UI-E10 (CHANGE/CHPROP); sysvar SETBYLAYERMODE.
- **Đầu ra:** `commands/modify/props/`: CHPROP và phần Properties của CHANGE (Color, Elev, LAyer, LType, ltScale, LWeight, Thickness, TRansparency, Material, PLotstyle, Annotative); điểm thay đổi của CHANGE cho line, tròn, text, ATTDEF, block; SETBYLAYER (Settings, đổi ByBlock thành ByLayer, gồm cả block); `docs/specs/commands/CHANGE.md`, `CHPROP.md`, `SETBYLAYER.md`.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 60 ca vàng mỗi lệnh.
  2. Mọi LISP trong QA-008 gọi CHANGE, CHPROP chạy đúng.
  3. SETBYLAYER gồm block sửa mỗi định nghĩa block một lần dù có nhiều tham chiếu; ALL trên 1 triệu entity ≤ 500 ms tầm trung, song song **[đề xuất]**.
  4. Đạt DoD-C, DoD-H.

### CMD-190 · CHSPACE
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-152, DOC-011, DOC-014
- **Lệnh:** CHSPACE
- **Đầu vào:** 07.1 §7.2 (Chuyển không gian); API DOC-011 (viewport, không gian), DOC-014, CMD-152.
- **Đầu ra:** `commands/modify/chspace/`: chọn đối tượng, viewport nguồn và đích; chuyển giữa Model và Paper theo tỉ lệ và góc xoay của viewport; `docs/specs/commands/CHSPACE.md`.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 40 ca vàng (viewport xoay, tỉ lệ khác 1, nhiều viewport).
  2. Vị trí hiển thị trên giấy trước và sau trùng trong dung sai GEO-002; chiều cao chữ đổi đúng theo tỉ lệ viewport.
  3. Model → Paper → Model trả hình học trong dung sai.
  4. Đạt DoD-C.

### CMD-191 · DRAWORDER, TEXTTOFRONT, HATCHTOBACK
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-151, DOC-020
- **Lệnh:** DRAWORDER, TEXTTOFRONT, HATCHTOBACK
- **Đầu vào:** 03 §6 (P0 / P1 / P1); 07.2 UI-E10 (DRAWORDER); API DOC-020 (SORTENTS); sysvar DRAWORDERCTL.
- **Đầu ra:** `commands/modify/draworder/`: Above objects, Under objects, Front, Back; TEXTTOFRONT (Text, Dimensions, Leaders, All); HATCHTOBACK; ba lệnh dùng chung engine nên làm cùng một task **[đề xuất]**; `docs/specs/commands/DRAWORDER.md`, `TEXTTOFRONT.md`, `HATCHTOBACK.md`.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 40 ca vàng DRAWORDER, 20 ca mỗi lệnh còn lại.
  2. Thứ tự tương đối trong nhóm được chọn và trong nhóm không chọn đều giữ nguyên, 100%.
  3. Đổi thứ tự 100.000 đối tượng ≤ 50 ms tầm trung **[đề xuất]**.
  4. Đạt DoD-C, DoD-L.

### CMD-192 · OVERKILL, -OVERKILL
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-151, CMD-171, GEO-168, DOC-017
- **Lệnh:** OVERKILL, -OVERKILL
- **Màn hình:** UI-E04
- **Đầu vào:** 07.2 UI-E04 (dung sai, bỏ qua thuộc tính, 6 tùy chọn, thông báo kết quả); API GEO-168 (phát hiện trùng và chồng lấn), CMD-171 (gộp đoạn), DOC-017 (so thuộc tính).
- **Đầu ra:** `commands/modify/overkill/`: nhóm entity theo loại và thuộc tính không bị bỏ qua, gọi GEO-168 theo nhóm, áp kết quả (xóa, gộp đoạn thẳng hàng chồng một phần hoặc nối tiếp, tối ưu đoạn trong polyline, bỏ qua bề rộng, không phá polyline, giữ đối tượng liên kết); API tham số cho UI-E04; `-OVERKILL` cho script; `docs/specs/commands/OVERKILL.md`.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 60 ca vàng (từng tùy chọn và từng thuộc tính bỏ qua).
  2. Không xóa nhầm: mọi điểm mẫu của đối tượng bị xóa nằm trên đối tượng còn lại trong dung sai đã đặt; tìm đủ 100% bản trùng cấy sẵn trong bản vẽ thử.
  3. 1 triệu entity có 10% trùng ≤ 2 s tầm trung **[đề xuất]**; kết quả không phụ thuộc số luồng.
  4. Thông báo số đối tượng xóa và số đoạn gộp đúng; một mark undo.
  5. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** thuật toán phát hiện trùng (GEO-168).

### CMD-193 · FLATTEN
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-152, GEO-022, INT-015
- **Lệnh:** FLATTEN
- **Đầu vào:** 03 §6 (bản vẽ khảo sát), §28; 07.2 UI-E10 (FLATTEN: bỏ nét khuất Yes/No); API GEO-022, INT-015 (UCS), CMD-152.
- **Đầu ra:** `commands/modify/express/Flatten.cpp`: chiếu lên mặt XY của UCS hiện hành cho điểm, line, polyline (cao độ, Z đỉnh), polyline 3D (thành polyline 2D **[cần xác minh]**), text, MTEXT, block, kích thước; cung, tròn nghiêng thành cung ellipse, ellipse; loại không chiếu được giữ nguyên, báo số lượng; `docs/specs/commands/FLATTEN.md`.
- **Tiêu chí nghiệm thu:**
  1. Đạt KTL trên 40 ca vàng.
  2. Sau lệnh, Z của mọi điểm thuộc loại hỗ trợ bằng 0 chính xác; X, Y của đối tượng có pháp tuyến song song Z không đổi từng bit.
  3. Bản vẽ khảo sát VN-2000 1 triệu entity ≤ 1 s tầm trung, song song **[đề xuất]**.
  4. Đạt DoD-C, DoD-H.

## 10. Hoàn tác và clipboard

### CMD-231 · UNDO đủ tùy chọn và U
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-008, INT-002, INT-003, INT-005, LSP-006, CMD-001
- **Lệnh:** UNDO, U
- **Đầu vào:** 03 Phụ lục A (UNDO, U); API DOC-008 (mark, nhóm, Back, Control, giới hạn bộ nhớ); tài liệu lệnh UNDO công khai của AutoCAD; KTL (CMD-001).
- **Đầu ra:** `commands/modify/undo/UndoCommand.{h,cpp}`; ca vàng `commands/modify/tests/undo/`; `docs/specs/commands/UNDO.md`, `U.md`.
- **Tiêu chí nghiệm thu:**
  1. Đủ tùy chọn: số bước, Auto, Control (All/None/One/Combine/Layer), BEgin, End, Mark, Back; prompt, mặc định và từng bit `UNDOCTL` như AutoCAD.
  2. Ca biên có test: Back không gặp Mark thì hỏi "Hoàn tác toàn bộ? [Có/Không]"; BEgin không có End rồi U; UNDO trong lệnh trong suốt; Control None và One; Combine gom pan/zoom liên tiếp; hết lịch sử báo "Đã hoàn tác hết".
  3. LISP: chuỗi `(command ...)` bọc `UNDO BEgin`/`End` hoàn tác một lần, không bọc thì mỗi lệnh một bước; đúng trên mọi tệp dùng UNDO của bộ QA-008.
  4. U sau lệnh cơ bản trên bản vẽ 1 triệu entity ≤ 20 ms; U một lệnh sửa 100.000 đối tượng ≤ 0,5 s **[đề xuất]**; hash sau U trùng hash trước lệnh.
  5. KTL: bàn phím, `.scr`, `(command ...)` cho cùng hash.
  6. Đạt DoD-C, DoD-L.

### CMD-232 · REDO, MREDO và lịch sử hoàn tác cho QAT
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-231
- **Lệnh:** REDO, MREDO
- **Màn hình:** UI-A04
- **Đầu vào:** 07.1 UI-A04 (nút Hoàn tác có danh sách bước); API DOC-008; prototype `design/prototype/index.html#UI-A04`.
- **Đầu ra:** `commands/modify/undo/RedoCommand.{h,cpp}`; API `UndoHistory` (nhãn bước: tên lệnh địa phương và toàn cục, số đối tượng); ca vàng; `docs/specs/commands/REDO.md`, `MREDO.md`.
- **Tiêu chí nghiệm thu:**
  1. REDO chỉ có tác dụng ngay sau U/UNDO; sau một lệnh sửa mới thì báo không còn gì để làm lại (như AutoCAD); MREDO nhận số bước, All, Last.
  2. Hoàn tác hoặc làm lại n bước qua `UndoHistory` cho cùng hash với `UNDO n` / `MREDO n`.
  3. Danh sách cập nhật sau mỗi lệnh ≤ 1 ms với lịch sử 10.000 bước **[đề xuất]**.
  4. KTL: ba đường nhập cùng hash.
  5. Đạt DoD-C, DoD-L.

### CMD-233 · Gói clipboard: tuần tự hóa vùng chọn thành bản vẽ con
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-014, DOC-009, DWG-022, RND-021, NT-005
- **Đầu vào:** README §4.2 (clipboard thuộc CMD, bảng Excel thuộc VN-013); API DOC-014 (sao chép sâu giữa hai bản vẽ), DOC-009 (snapshot), DWG-022 (bộ ghi AC1032), RND-021 (ảnh ngoài màn hình).
- **Đầu ra:**
  - `commands/modify/clipboard/ClipPayload.{h,cpp}`: gói = DWG AC1032 trong bộ nhớ + điểm gốc + INSUNITS + mã bản vẽ nguồn; kèm ảnh PNG và text thuần cho ứng dụng khác.
  - `ClipboardPort`: bản trong bộ nhớ cho `trion-console` và test, bản Qt `app/core/QtClipboardPort.cpp`; fuzz target cho bộ đọc gói.
- **Tiêu chí nghiệm thu:**
  1. Gói → dán vào bản vẽ trống: 100% trường trùng (DOC-017) trên 1.000 vùng chọn ngẫu nhiên từ QA-001, kể cả block lồng, kích thước, MLEADER, hatch, xdata, đối tượng chưa hiểu.
  2. Gói chỉ mang phụ thuộc thật sự dùng (layer, linetype, kiểu, block, regapp); test đếm bảng ký hiệu không có mục thừa.
  3. Tạo gói 100.000 entity ≤ 300 ms ở nền **[đề xuất]**; luồng UI chỉ dừng để lấy snapshot (≤ 1 khung).
  4. Hai tiến trình TrionCAD trao đổi qua clipboard hệ thống trên Windows, macOS, Linux (test tích hợp CI); gói nhận từ ngoài là đầu vào không tin cậy.
  5. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** đọc định dạng clipboard của AutoCAD; OLE.

### CMD-234 · COPYCLIP, CUTCLIP, COPYBASE
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-233, INT-008, CMD-002
- **Lệnh:** COPYCLIP, CUTCLIP, COPYBASE
- **Đầu vào:** 03 Phụ lục A; API CMD-233, INT-008 (PICKFIRST, chọn); KTL sửa (CMD-002).
- **Đầu ra:** `commands/modify/clipboard/CopyCommands.{h,cpp}`; ca vàng; `docs/specs/commands/COPYCLIP.md`, `CUTCLIP.md`, `COPYBASE.md`.
- **Tiêu chí nghiệm thu:**
  1. PICKFIRST: dùng vùng chọn sẵn, không có thì hỏi chọn; chọn rỗng thì clipboard giữ nguyên.
  2. Điểm gốc: COPYCLIP/CUTCLIP lấy góc dưới trái hộp bao vùng chọn; COPYBASE hỏi điểm gốc (OSNAP, tọa độ theo UCS).
  3. CUTCLIP = sao chép + xóa trong một mark undo; U khôi phục đối tượng với handle cũ, clipboard không đổi.
  4. Đối tượng trên layer khóa: COPYCLIP lấy được, CUTCLIP bỏ qua và báo "n đối tượng trên layer khóa".
  5. KTL: ba đường nhập cùng hash, CUTCLIP đúng một mark; COPYCLIP 100.000 entity xong ≤ 0,5 s **[đề xuất]**.
  6. Đạt DoD-C, DoD-L.

### CMD-235 · PASTECLIP, PASTEBLOCK
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-233, INT-012, INT-004, DOC-012, CMD-002
- **Lệnh:** PASTECLIP, PASTEBLOCK
- **Đầu vào:** 03 Phụ lục A; 07.1 §9.11 (văn bản từ ứng dụng khác dán thành MTEXT); API CMD-233, INT-012 (jig), DOC-012 (block vô danh).
- **Đầu ra:** `commands/modify/clipboard/PasteCommands.{h,cpp}`; ca vàng; `docs/specs/commands/PASTECLIP.md`, `PASTEBLOCK.md`.
- **Tiêu chí nghiệm thu:**
  1. Jig tại điểm chèn ≤ 8 ms/khung với 10.000 entity; gói > 100.000 entity xem trước bằng khung bao **[đề xuất]**.
  2. Trùng tên layer, kiểu, block: giữ định nghĩa của bản vẽ đích, chỉ thêm mục mới; handle mới không trùng; tỉ lệ theo INSUNITS nguồn/đích như AutoCAD (xác nhận bằng QA-007).
  3. PASTEBLOCK tạo block vô danh tên `A$C` + mã hex như AutoCAD rồi chèn; dán vào paper space, viewport, UCS khác sai số ≤ 1e-9 × kích thước bản vẽ.
  4. Clipboard chỉ có văn bản: tạo MTEXT theo kiểu chữ hiện hành, tiếng Việt chuẩn hóa NFC (NT-012).
  5. Một mark undo; KTL ba đường nhập cùng hash; dán 100.000 entity xong ≤ 0,5 s sau khi chọn điểm **[đề xuất]**.
  6. Đạt DoD-C, DoD-H, DoD-L.

### CMD-236 · PASTEORIG, PASTESPEC
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-235, CMD:IMAGEATTACH
- **Lệnh:** PASTEORIG, PASTESPEC
- **Đầu vào:** API CMD-235; tạo ảnh qua lệnh IMAGEATTACH.
- **Đầu ra:** `commands/modify/clipboard/PasteSpecial.{h,cpp}`; ca vàng; `docs/specs/commands/PASTEORIG.md`, `PASTESPEC.md`.
- **Tiêu chí nghiệm thu:**
  1. PASTEORIG dán tại tọa độ gốc vào bản vẽ khác; dán vào chính bản vẽ nguồn thì báo và không làm gì (như AutoCAD).
  2. PASTESPEC liệt kê định dạng đang có: bản vẽ TrionCAD, ảnh (lưu PNG cạnh bản vẽ rồi gắn IMAGE), văn bản thuần (MTEXT hoặc TEXT); FILEDIA=0 hỏi trên dòng lệnh **[đề xuất]**.
  3. Một mark undo; KTL ba đường nhập cùng hash.
  4. Đạt DoD-C.
- **Ngoài phạm vi:** Dán liên kết (OLE).

## 11. Mở và tạo bản vẽ

### CMD-237 · Đường ống mở tệp: nhận diện định dạng, mở song song, nạp dần
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-004, DWG-019, DWG-024, DWG-031, DOC-016, DOC-018, NT-005, NT-011, NT-013
- **Màn hình:** UI-B13
- **Đầu vào:** 05 §1.1 (mở 20 MB, 100 MB), §6.2 mục 3 (nạp lười), §9.4 (mmap, đọc song song); 07.1 UI-B13; API DOC-016, DOC-018, NT-013.
- **Đầu ra:** `commands/file/OpenPipeline.{h,cpp}`: `openDocuments(paths, OpenOptions)` (chỉ đọc, phục hồi), nhận diện định dạng theo nội dung, tiến trình và hủy, cổng giữ lệnh sửa khi đang nạp; `commands/file/bench/open_bench.cpp`; test.
- **Tiêu chí nghiệm thu:**
  1. Nhận diện theo nội dung, không theo phần mở rộng: DWG AC1012–AC1032, DXF ASCII/nhị phân, DWT; R12 trở về trước, mới hơn AC1032, tệp rỗng, tệp lạ → mã lỗi riêng, thông báo rõ; kho QA-001 + 200 tệp lỗi tự tạo, 0 crash.
  2. BENCH-STD 20 MB ≤ 1,5 s, 100 MB ≤ 5 s (tầm trung, DWG-031); đường ống thêm ≤ 5% **[đề xuất]**; khung hình đầu của tệp 100 MB ≤ 1 s **[đề xuất]**.
  3. Mở 8 tệp một lúc: tổng thời gian ≤ 1,5 × tệp lớn nhất **[đề xuất]**; tiến trình từng tệp ("Đang nạp 63% · 1,2 triệu đối tượng"); hủy ≤ 50 ms, không để lại tài liệu dở.
  4. Trong khi nạp: lệnh xem và đo chạy ngay, pan/zoom đạt DoD-L; lệnh sửa xếp hàng đến khi nạp xong.
  5. Đường dẫn tiếng Việt có dấu, UNC, dài hơn 260 ký tự (Windows), tệp đang bị ứng dụng khác ghi: mở đúng hoặc báo lỗi rõ.
  6. Đạt DoD-C, DoD-H, DoD-L.

### CMD-238 · Phát hiện tham chiếu thiếu khi mở
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-237, DOC-013, RGN-006, DWG-027, NT-010, NT-011
- **Màn hình:** UI-B06
- **Đầu vào:** 07.1 UI-B06; API DOC-013 (phân giải xref), RGN-006 (font thiếu, font thay thế), NT-011 (đường dẫn tìm kiếm, đường dẫn tương đối), DWG-027 (FileDepList); prototype `#UI-B06`.
- **Đầu ra:** `commands/file/MissingRefs.{h,cpp}`: quét nền sau khi mở → danh sách theo nhóm (Font, Xref, Ảnh, PDF underlay, Plot style, Shape) gồm đường dẫn đã lưu, trạng thái, đường dẫn tìm được; API "Tìm tệp…", "Tìm trong thư mục…", ghi nhớ lựa chọn (NT-010); chỉ áp trong phiên, không sửa bản vẽ.
- **Tiêu chí nghiệm thu:**
  1. 300 bản vẽ có tham chiếu thiếu đã gán nhãn (QA-001): phát hiện 100%, 0 báo nhầm.
  2. Thứ tự tìm như AutoCAD: đường dẫn đã lưu, thư mục bản vẽ chủ, đường dẫn dự án và hỗ trợ; xref lồng tìm theo bản vẽ cha.
  3. Quét nền không chặn mở: 1.000 tham chiếu trên đĩa cục bộ ≤ 200 ms **[đề xuất]**; đường mạng chậm có giới hạn thời gian, hủy ≤ 50 ms.
  4. "Tìm trong thư mục…" giải quyết mọi mục cùng tên một lần; lựa chọn đã nhớ được dùng lại ở phiên sau (test hai phiên).
  5. Sự kiện kết quả có số đếm theo loại, đủ cho banner UI-B06.
  6. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** sửa đường dẫn lưu trong bản vẽ (lệnh xref, ảnh của CMD-4); thay font (Bác sĩ Font).

### CMD-239 · RECOVER: mở có phục hồi và báo cáo
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-237, DWG-020, DOC-015, INT-020, CMD-001
- **Lệnh:** RECOVER
- **Màn hình:** UI-B04, UI-B10
- **Đầu vào:** 03 §2; 05 §6.2 mục 4 (chế độ Recover); 07.1 UI-B04; 07.2 UI-B10; API DWG-020 (nhật ký sửa), DOC-015 (sửa toàn vẹn); QA-004 (DWG TrueView).
- **Đầu ra:** `commands/file/RecoverCommand.{h,cpp}`; `RecoveryReport` (tóm tắt, nhật ký, xuất văn bản) dùng chung với AUDIT; bộ tệp hỏng sinh từ QA-001 `commands/file/tests/corrupt/`; `docs/specs/commands/RECOVER.md`.
- **Tiêu chí nghiệm thu:**
  1. 1.000 tệp hỏng sinh từ QA-001 (cắt cụt, lật bit, sai CRC, lệch offset, mất section): 0 crash, không treo > 30 s **[đề xuất]**; tỉ lệ mở được không thấp hơn DWG TrueView trên cùng bộ (QA-004).
  2. Báo cáo: số lỗi tìm thấy và đã sửa, nhật ký từng mục (loại, handle, hành động), lưu được thành tệp văn bản; đủ dữ liệu cho UI-B10.
  3. Bản vẽ phục hồi gắn nhãn "[Đã phục hồi]", đánh dấu cần lưu, gợi ý Lưu thành; Viewer mở chỉ đọc, không lưu (INT-020).
  4. Tệp lành: kết quả trùng hash của OPEN, thời gian ≤ 2 × OPEN **[đề xuất]**.
  5. KTL: FILEDIA=0 hỏi tên tệp; bàn phím, `.scr`, `(command ...)` cùng hash.
  6. Đạt DoD-C, DoD-H.

### CMD-240 · OPEN: lệnh hoàn chỉnh
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-237, CMD-238, CMD-239, INT-003, INT-020, NT-010, CMD-001
- **Lệnh:** OPEN
- **Màn hình:** UI-B01
- **Đầu vào:** 03 §2; 07.1 UI-B01, UI-B04, UI-A09, §9.11 (kéo-thả tệp DWG/DXF vào dải tab); prototype `#UI-B01`.
- **Đầu ra:** `commands/file/OpenCommand.{h,cpp}`; ca vàng `commands/file/tests/open/`; `docs/specs/commands/OPEN.md` (FILEDIA, nhiều tệp, chỉ đọc, tệp đã mở, phục hồi).
- **Tiêu chí nghiệm thu:**
  1. FILEDIA=1 phát yêu cầu hộp thoại UI-B01 (nhiều tệp, "Mở chỉ đọc"); FILEDIA=0 hỏi tên tệp, đường dẫn tương đối theo thư mục hiện hành; nhiều tệp mở thành nhiều tài liệu theo thứ tự chọn.
  2. Tệp đã mở: chuyển sang tài liệu đó và báo, không mở lần hai; tệp có thuộc tính chỉ đọc hoặc chọn "Mở chỉ đọc" → tài liệu chỉ đọc.
  3. Lỗi cấu trúc: hỏi "Tệp có lỗi cấu trúc. Chạy Phục hồi?" → RECOVER (UI-B04); từ chối thì không tạo tài liệu.
  4. Cập nhật tệp gần đây (NT-010), DWGNAME, DWGPREFIX, DWGTITLED, DBMOD = 0; Viewer mở mọi định dạng ở chế độ chỉ đọc.
  5. KTL ba đường nhập cùng hash; `.scr` chạy tiếp trên bản vẽ vừa mở như AutoCAD.
  6. Đạt DoD-C, DoD-L.

### CMD-241 · NEW, QNEW và bản vẽ mẫu .dwt
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-237, VN-008, NT-010, CMD-001
- **Lệnh:** NEW, QNEW
- **Đầu vào:** 03 §2 (bản vẽ mẫu, mẫu TCVN mặc định); 07.1 UI-A03 (Mới: chọn mẫu); API VN-008 (mẫu `.dwt` A0–A4).
- **Đầu ra:** `commands/file/NewCommand.{h,cpp}`; `TemplateResolver` (thư mục mẫu, mẫu mặc định của QNEW lưu trong NT-010); ca vàng; `docs/specs/commands/NEW.md`, `QNEW.md`.
- **Tiêu chí nghiệm thu:**
  1. NEW: FILEDIA=1 phát yêu cầu chọn mẫu; FILEDIA=0 hỏi "Nhập tên mẫu hoặc [. (không dùng mẫu)] <mặc định>"; "." tạo bản vẽ trống hệ mét hoặc hệ Anh theo MEASUREINIT.
  2. QNEW dùng mẫu mặc định trong cài đặt (mặc định: mẫu TCVN A3 của VN-008); mẫu thiếu hoặc hỏng thì báo và tạo bản vẽ trống hệ mét.
  3. Bản vẽ mới giữ đủ nội dung mẫu (layer, kiểu, layout, page setup, block); đặt lại TDCREATE, TDUPDATE, TDINDWG; FINGERPRINTGUID, VERSIONGUID mới; DWGTITLED = 0; tên `Drawing<n>.dwg` để LISP cũ chạy đúng **[đề xuất]**.
  4. NEW từ mẫu TCVN A3 ≤ 300 ms **[đề xuất]**.
  5. KTL ba đường nhập cùng hash (bỏ trường thời gian theo testkit).
  6. Đạt DoD-C, DoD-L.

## 12. Đóng, lưu, tự lưu và phục hồi sau sự cố

### CMD-242 · CLOSE, CLOSEALL, QUIT
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-240, DOC-018, NT-013, INT-003, PLF-002, CMD-001
- **Lệnh:** CLOSE, CLOSEALL, QUIT
- **Màn hình:** UI-B03
- **Đầu vào:** 03 §2; 07.1 UI-B03, UI-A09 (đóng tab, đóng tất cả, đóng các tab khác); API DOC-018, NT-013 (hủy tác vụ nền), PLF-002 (quyền lưu).
- **Đầu ra:** `commands/file/CloseCommands.{h,cpp}`; danh sách bản vẽ chưa lưu cho UI-B03; ca vàng; `docs/specs/commands/CLOSE.md`, `CLOSEALL.md`, `QUIT.md`.
- **Tiêu chí nghiệm thu:**
  1. Đóng hủy mọi tác vụ nền của tài liệu (nạp dần, regen, quét tham chiếu) ≤ 50 ms; RSS giảm ≥ 90% phần bộ nhớ của tài liệu trong 1 s **[đề xuất]**.
  2. Bản vẽ đã sửa ở phiên bản có quyền lưu (PLF-002): hỏi "Lưu thay đổi vào <tên>? [Có/Không/Hủy]", Có thì chạy QSAVE; CLOSEALL, QUIT gom danh sách cho UI-B03; Hủy dừng cả chuỗi. Phiên bản không có quyền lưu: không hỏi.
  3. QUIT đóng hết rồi thoát; `trion-console` trả mã thoát 0 khi thành công, khác 0 khi bị hủy hoặc lỗi.
  4. `.scr`: CLOSE chuyển ngữ cảnh kịch bản sang bản vẽ còn lại như AutoCAD; KTL ba đường nhập.
  5. Đạt DoD-C, DoD-L.

### CMD-243 · Engine lưu nguyên tử từ snapshot
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-009, NT-005, NT-011, DWG-021, DWG-022, DWG-024, DWG-026
- **Đầu vào:** 05 §1.2 (ba bộ đọc độc lập mở không lỗi), §6.2 mục 2 (giữ nguyên dữ liệu chưa hiểu), §9.4 (tuần tự hóa trên luồng nền); API NT-011 (ghi an toàn), DOC-009, bộ ghi DWG-021, DWG-022, DXF DWG-024, thumbnail DWG-026.
- **Đầu ra:** `commands/file/SaveEngine.{h,cpp}`: `save(snapshot, path, format, SaveOptions)` → tuần tự hóa và nén ở nền, ghi tệp tạm cùng thư mục, flush, kiểm CRC section của tệp vừa ghi, `.bak` theo ISAVEBAK, đổi tên nguyên tử, giữ quyền truy cập của tệp gốc; test tiêm lỗi; `commands/file/bench/save_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Tiêm lỗi 1.000 lần (đầy đĩa, mất quyền, giết tiến trình ở điểm ngẫu nhiên, tệp đích bị khóa): tệp gốc luôn nguyên vẹn và mở được; không bao giờ còn tệp đích ghi dở.
  2. Luồng UI dừng ≤ 1 khung (16,6 ms) để lấy snapshot **[đề xuất]**; sửa tiếp trong lúc lưu: thay đổi sau snapshot không vào tệp, bản vẽ vẫn đánh dấu chưa lưu.
  3. Lưu BENCH-STD 20 MB ≤ 1,5 s, 100 MB ≤ 5 s (tầm trung) **[đề xuất]**; hiệu suất song song ≥ 60% đến 8 luồng.
  4. Tệp đã lưu: QA-003 trùng 100% trường; ba bộ đọc QA-004 mở không lỗi, không hỏi recover; handle không đánh lại.
  5. ISAVEBAK = 1 tạo `.bak` là bản trước đó, = 0 không tạo; lưu vào thư mục chia sẻ SMB đúng (share Samba trong CI); mỗi lần lưu cập nhật TDUPDATE và cộng thời gian mở vào TDINDWG, sai ≤ 1 s mỗi giờ.
  6. Đạt DoD-C, DoD-H.

### CMD-244 · SAVE, QSAVE, SAVEAS
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-243, CMD-242, INT-003, INT-020, PLF-002, NT-010, CMD-001
- **Lệnh:** SAVE, QSAVE, SAVEAS
- **Màn hình:** UI-B02
- **Đầu vào:** 03 §2 (lưu 2018, 2004, DXF; ghi chú TrustedDWG); 07.1 UI-B02; API CMD-243, PLF-002 (Viewer Pro chỉ lưu bản sao sau chuyển font); prototype `#UI-B02`.
- **Đầu ra:** `commands/file/SaveCommands.{h,cpp}`; ca vàng `commands/file/tests/save/`; `docs/specs/commands/SAVE.md`, `QSAVE.md`, `SAVEAS.md`.
- **Tiêu chí nghiệm thu:**
  1. FILEDIA=0: SAVEAS hỏi định dạng `[2004/2018/DXF/Template] <2018>` rồi tên tệp; FILEDIA=1 phát yêu cầu UI-B02; định dạng mặc định đọc từ cài đặt (NT-010); SAVE theo hành vi AutoCAD (xác nhận bằng QA-007).
  2. QSAVE bản vẽ chưa đặt tên → SAVEAS; bản vẽ chỉ đọc → từ chối, gợi ý SAVEAS; SAVEAS cập nhật tên tài liệu, DWGNAME, DWGPREFIX, DWGTITLED, tệp gần đây; `.dwt` lưu kèm mô tả và hệ đo.
  3. Lần lưu DWG đầu tiên phát sự kiện thông báo TrustedDWG một lần (cờ "Không hiện lại" trong NT-010); không phát khi chạy `.scr`, LISP, `trion-console`.
  4. CLOSE/QUIT bản vẽ đã sửa: Có → lưu xong mới đóng (kể cả khi đang lưu nền); Không → đóng; Hủy → giữ nguyên; test cả qua `.scr`.
  5. Viewer: chặn ba lệnh (INT-020), trừ bản vẽ đã sửa trước khi hạ xuống Viewer (hết dùng thử, mất chỗ floating) mà `canSaveDocument()` của PLF-002 cho lưu; Viewer Pro chỉ cho Lưu thành bản sao sau chuyển font; KTL ba đường nhập cùng hash.
  6. Đạt DoD-C, DoD-L.

### CMD-245 · SAVEAS: phân tích và cảnh báo hạ cấp phiên bản
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-244, DWG-001, NT-005
- **Màn hình:** UI-B02
- **Đầu vào:** 07.1 UI-B02 (cảnh báo, liệt kê ảnh hưởng rồi mới cho lưu); điều kiện phiên bản trong schema DWG-001.
- **Đầu ra:** `commands/file/DowngradeAnalysis.{h,cpp}`: ánh xạ điều kiện phiên bản của schema → nhóm ảnh hưởng dễ hiểu (annotative, multileader, độ trong suốt, bảng, field, dynamic block, attribute nhiều dòng…) kèm số đối tượng; móc xác nhận trong SAVEAS; test.
- **Tiêu chí nghiệm thu:**
  1. 200 tệp mẫu lưu sang 2004: liệt kê đúng 100% loại bị ảnh hưởng (đối chứng bằng cách đọc lại qua ODA File Converter, QA-004).
  2. Phân tích 1 triệu đối tượng ≤ 200 ms, song song **[đề xuất]**; không chặn luồng UI.
  3. Dòng lệnh hỏi xác nhận [Có/Không] trước khi lưu, UI-B02 dùng cùng dữ liệu; `.scr`/LISP không hỏi, chỉ ghi cảnh báo lên dòng lệnh **[đề xuất]**.
  4. Bản vẽ không dùng tính năng mới hơn đích: không cảnh báo.
  5. Đạt DoD-C, DoD-H.

## 13. Tự lưu, khóa tệp và trình phục hồi bản vẽ

### CMD-246 · Tự lưu trong phiên: SAVETIME, tệp `.sv$`, nhật ký phiên
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-019, DOC-112, DOC-119, CMD-244, CMD-242, INT-005, NT-013, PLF-002
- **Đầu vào:** 03 §2 (tự lưu, `SAVETIME` P0); 05 §9.4 (autosave không làm khựng); API DOC-019 (ghi `.sv$` từ snapshot ở nền), DOC-112 (DBMOD), DOC-119 (tài liệu chỉ đọc), PLF-002 (quyền lưu).
- **Đầu ra:** `commands/file/AutosaveScheduler.{h,cpp}`: hẹn giờ theo từng bản vẽ, hoãn khi đang có lệnh, gọi DOC-019; sysvar SAVETIME, SAVEFILEPATH, SAVEFILE (chỉ đọc); nhật ký phiên `session-<pid>.json` trong SAVEFILEPATH (bản vẽ đang mở, đường dẫn gốc, `.sv$` mới nhất); test với đồng hồ giả lập; `docs/specs/commands/SAVETIME.md`.
- **Tiêu chí nghiệm thu:**
  1. SAVETIME = n phút (0 tắt, mặc định 10): tự lưu khi DBMOD ≠ 0 và đã qua n phút từ lần lưu hoặc tự lưu trước; đang có lệnh thì hoãn đến khi lệnh xong; bản vẽ chỉ đọc, phiên bản không có quyền lưu: không tự lưu. 10.000 chuỗi sự kiện ngẫu nhiên với đồng hồ giả lập: 0 lần lỡ, 0 lần thừa.
  2. Luồng UI dừng ≤ 1 khung (16,6 ms) p99 để lấy snapshot, BENCH-STD 1 triệu entity **[đề xuất]**; tuần tự hóa và ghi chạy nền ưu tiên thấp; người dùng sửa tiếp không bị chặn (QA-011 đo độ trễ khung trong lúc tự lưu).
  3. Tên `<tên>_<a>_<b>_<số>.sv$` như AutoCAD, bản vẽ chưa đặt tên dùng `Drawing<n>`; lưu thành công hoặc đóng bình thường thì xóa `.sv$` và mục nhật ký; nhiều bản vẽ mở thì lịch tự lưu lệch nhau, không hai bản vẽ trong cùng khung.
  4. Giết tiến trình ở điểm ngẫu nhiên 1.000 lần trong lúc tự lưu: `.sv$` trước đó luôn mở được; `.sv$` mới hoặc đủ hoặc không có; nhật ký phiên luôn trỏ tới bản đủ.
  5. SAVEFILEPATH không ghi được hoặc đầy đĩa: thông báo một lần qua NT-013, thử lại ở chu kỳ sau, không có hộp thoại chặn.
  6. Đạt DoD-C, DoD-H, DoD-L.

### CMD-247 · Khóa `.dwl` khi mở, lưu thành, đóng; tệp đang được người khác mở
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** DOC-120, DOC-119, CMD-240, CMD-242, CMD-244
- **Màn hình:** UI-B05
- **Đầu vào:** 03 §2 (khóa `.dwl` P0); 07.1 UI-B05 (người đang mở, tên máy, thời điểm; Mở chỉ đọc / Hủy); API DOC-120 (`.dwl`/`.dwl2`, khóa hệ điều hành), DOC-119 (mở chỉ đọc).
- **Đầu ra:** `commands/file/FileLockFlow.{h,cpp}`: móc vào OPEN, SAVEAS, CLOSE, CLOSEALL, QUIT; dữ liệu cho UI-B05; test hai tiến trình trên ổ cục bộ và share SMB; bổ sung mục khóa tệp trong `docs/specs/commands/OPEN.md`.
- **Tiêu chí nghiệm thu:**
  1. Mở để sửa: tạo `.dwl`/`.dwl2` và giữ khóa. Tệp đã bị tiến trình khác khóa: FILEDIA = 1 phát yêu cầu UI-B05; FILEDIA = 0, `.scr`, LISP mở chỉ đọc và in một dòng nêu người, máy, thời điểm **[đề xuất]**.
  2. `.dwl` cũ không còn tiến trình giữ khóa (sau crash): tự tiếp quản, ghi log, không hỏi. `.dwl` do AutoCAD ghi (mẫu trong bộ test) hiện đúng người, máy, thời điểm.
  3. SAVEAS nhả khóa đường dẫn cũ, khóa đường dẫn mới; CLOSE, CLOSEALL, QUIT xóa `.dwl`/`.dwl2` của mọi bản vẽ đã đóng; mở chỉ đọc không tạo `.dwl`.
  4. Hai tiến trình mở cùng tệp 1.000 lần xen kẽ ngẫu nhiên (ổ cục bộ và share Samba trong CI): không bao giờ cả hai cùng có quyền sửa; kiểm khóa trên share ≤ 50 ms **[đề xuất]**.
  5. Đạt DoD-C.

### CMD-248 · DRAWINGRECOVERY: trình phục hồi bản vẽ sau sự cố
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-246, CMD-240, CMD-244, DWG-026, INT-003, NT-005, CMD-001
- **Lệnh:** DRAWINGRECOVERY, DRAWINGRECOVERYHIDE
- **Màn hình:** UI-B07
- **Đầu vào:** 03 §2 (DRAWINGRECOVERY P1); 07.1 UI-B07; prototype `#UI-B07` (`SC.recovery` trong `js/screens8.js`); nhật ký phiên của CMD-246; thumbnail DWG-026.
- **Đầu ra:** `commands/file/DrawingRecovery.{h,cpp}`: quét nhật ký của phiên đã chết khi khởi động; mô hình danh sách (bản vẽ → tệp gốc, `.bak`, `.sv$` kèm thời điểm, kích thước, thumbnail); hành động Mở, Lưu thành, Xóa bản sao lưu; `docs/specs/commands/DRAWINGRECOVERY.md`, `DRAWINGRECOVERYHIDE.md`.
- **Tiêu chí nghiệm thu:**
  1. Chỉ nhận nhật ký của phiên đã chết (PID và thời điểm khởi động tiến trình không khớp tiến trình nào đang sống); hai phiên TrionCAD chạy song song không thấy bản vẽ của nhau.
  2. Chỉ liệt kê bản vẽ có `.sv$` mới hơn tệp gốc; quét 50 bản vẽ ≤ 100 ms, thumbnail nạp nền **[đề xuất]**; khởi động ấm vẫn ≤ 1,5 s.
  3. Kill -9 sau một lần tự lưu rồi khởi động lại: danh sách đủ bản vẽ; Mở bản `.sv$` cho hash bằng trạng thái lúc tự lưu, gắn cờ đã phục hồi, DBMOD ≠ 0; QSAVE ghi về đường dẫn gốc kèm `.bak`; tệp gốc không đổi trước khi lưu.
  4. Xóa bản sao lưu cần xác nhận nêu tên tệp; bản vẽ đã xử lý hết thì rời nhật ký; DRAWINGRECOVERYHIDE đóng trình phục hồi, gọi lại DRAWINGRECOVERY hiện đủ danh sách còn lại.
  5. KTL: bàn phím, `.scr`, `(command ...)` cho cùng mô hình danh sách.
  6. Đạt DoD-C, DoD-L.

## 14. Lưu tất cả và DXF

### CMD-249 · SAVEALL
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-244, DOC-018, INT-020, CMD-001
- **Lệnh:** SAVEALL
- **Màn hình:** UI-A09
- **Đầu vào:** 07.1 §5.6 UI-A09 (menu tab: Lưu tất cả); API CMD-244 (QSAVE, SAVEAS), DOC-018 (danh sách bản vẽ mở).
- **Đầu ra:** `commands/file/SaveAllCommand.{h,cpp}`; ca vàng; `docs/specs/commands/SAVEALL.md`.
- **Tiêu chí nghiệm thu:**
  1. Lưu mọi bản vẽ có DBMOD ≠ 0 theo thứ tự tab; bản vẽ chưa đặt tên chạy luồng SAVEAS; Hủy ở một bản vẽ dừng các bản còn lại, bản đã lưu giữ nguyên.
  2. Bản vẽ chỉ đọc hoặc chưa sửa: bỏ qua; kết thúc in "Đã lưu n bản vẽ, bỏ qua m"; lỗi ghi ở một tệp không ảnh hưởng tệp khác, báo theo tệp.
  3. Tuần tự hóa các bản vẽ chạy song song ở nền: 5 bản vẽ BENCH-STD 20 MB xong ≤ 3 s trên máy tầm trung **[đề xuất]**; luồng UI dừng ≤ 1 khung mỗi bản vẽ.
  4. KTL ba đường nhập cùng hash; Viewer bị chặn (INT-020).
  5. Đạt DoD-C, DoD-H, DoD-L.

### CMD-250 · DXFIN, DXFOUT
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-240, CMD-244, DWG-024, DWG-025, INT-020, CMD-001
- **Lệnh:** DXFIN, DXFOUT
- **Đầu vào:** 03 §2 (DXF ASCII/nhị phân, `DXFIN` P0); 05 §6.1 (DXF mọi phiên bản); API DWG-024 (ASCII), DWG-025 (nhị phân), CMD-240, CMD-244.
- **Đầu ra:** `commands/file/DxfCommands.{h,cpp}`; ca vàng; `docs/specs/commands/DXFIN.md`, `DXFOUT.md`.
- **Tiêu chí nghiệm thu:**
  1. DXFIN mở DXF ASCII và nhị phân thành bản vẽ như OPEN (FILEDIA, chỉ đọc, nạp dần); DXF hỏng thì đề nghị phục hồi như RECOVER.
  2. DXFOUT với FILEDIA = 0 hỏi "Nhập số chữ số thập phân (0 đến 16) hoặc [Objects/Version/Binary] <16>"; đủ ba tùy chọn (chỉ đối tượng đã chọn, phiên bản DXF, nhị phân).
  3. Tên tài liệu, DWGNAME, DBMOD sau DXFOUT theo đúng hành vi AutoCAD (khớp bộ tham chiếu QA-007).
  4. Tệp xuất: ODA File Converter, LibreDWG CLI (QA-004) đọc không lỗi; DWG → DXF (16 chữ số) → DWG trùng 100% trường (QA-003).
  5. KTL ba đường nhập cùng hash; Viewer: DXFIN được, DXFOUT bị chặn (INT-020).
  6. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** DXF R12 (P2, DWG-029).

## 15. Kiểm tra lỗi, dọn dẹp, phục hồi cả xref

### CMD-251 · AUDIT: kiểm tra và sửa lỗi bản vẽ đang mở
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-015, DOC-009, CMD-239, NT-005, NT-013, INT-003, INT-020, CMD-002
- **Lệnh:** AUDIT
- **Màn hình:** UI-B10
- **Đầu vào:** 03 §2, Phụ lục A (AUDIT); 07.2 UI-B10; prototype `#UI-B10` (`IMPL.AUDIT` trong `js/screens8.js`); API DOC-015 (kiểm và sửa), `RecoveryReport` của CMD-239; bộ tệp hỏng `commands/file/tests/corrupt/`.
- **Đầu ra:** `commands/utility/AuditCommand.{h,cpp}`: pha kiểm chạy song song trên snapshot, pha sửa trong một transaction; báo cáo `.adt` theo AUDITCTL; ca vàng; `commands/utility/bench/audit_bench.cpp`; `docs/specs/commands/AUDIT.md`.
- **Tiêu chí nghiệm thu:**
  1. Prompt "Sửa các lỗi tìm thấy? [Có/Không] <Không>". Không: chỉ báo, hash bản vẽ giữ nguyên. Có: sửa trong đúng một mark undo, U trả về hash trước AUDIT.
  2. Kiểm cả định nghĩa block, bảng ký hiệu, dictionary, regapps; dòng tổng kết "Tổng số lỗi tìm thấy n, đã sửa m" như AutoCAD; AUDITCTL = 1 ghi `<tên>.adt` cạnh bản vẽ, = 0 không ghi.
  3. Bộ tệp hỏng: AUDIT Có rồi AUDIT lần hai báo 0 lỗi; tệp lưu sau đó được ba bộ đọc QA-004 mở không lỗi, không hỏi recover; số đối tượng hợp lệ không giảm.
  4. BENCH-STD 1 triệu entity: pha kiểm ≤ 2 s trên máy tầm trung, hiệu suất song song ≥ 60% đến 8 luồng **[đề xuất]**; > 2 s có tiến trình và nút hủy; hủy trong pha kiểm không đổi bản vẽ.
  5. KTL ba đường nhập cho cùng hash và cùng báo cáo; Viewer bị chặn (INT-020).
  6. Đạt DoD-C, DoD-H, DoD-L.

### CMD-252 · Mô hình dọn dẹp: mục dọn được, lý do giữ lại, ước tính dung lượng
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-117, DOC-014, DOC-005, DOC-009, NT-005
- **Màn hình:** UI-B09
- **Đầu vào:** 03 §2 (PURGE gồm regapps, đối tượng dài 0, text rỗng); 07.2 UI-B09; prototype `#UI-B09` (`IMPL.PURGE` trong `js/screens8.js`: danh mục, lý do); API DOC-117 (phân tích tham chiếu), DOC-014 (xóa/khôi phục), DOC-005 (xdata).
- **Đầu ra:** `commands/utility/purge/PurgeModel.{h,cpp}`: `analyze(snapshot, PurgeOptions)` → cây danh mục (Block, Kiểu kích thước, Nhóm, Layer, Linetype, Kiểu multileader, Kiểu mline, Plot style, Shape, Kiểu bảng, Kiểu chữ, Kiểu hiển thị, Vật liệu, Regapps) và mục không tên (hình học dài 0, text rỗng, dữ liệu mồ côi), lý do giữ lại, số byte ước tính; `apply(doc, mục, lồngNhau)` xóa qua DOC-014 trong một transaction; test; `commands/utility/bench/purge_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. 100% mục không dọn được có lý do cụ thể (hiện hành, mặc định, đang được n đối tượng dùng, lồng trong block, phụ thuộc xref, được kiểu khác tham chiếu); bộ ca vàng 200 bản vẽ: danh sách dọn được khớp AutoCAD (QA-007).
  2. Dọn lồng nhau lặp đến điểm bất động: `analyze` sau đó trả 0 mục; 1.000 bản vẽ QA-001 dọn hết rồi AUDIT báo 0 lỗi (không bao giờ xóa mục còn được tham chiếu).
  3. Regapp chỉ dọn khi không xdata nào dùng, kể cả trong định nghĩa block và đối tượng không đồ họa; bản vẽ 100.000 regapps phân tích ≤ 0,5 s **[đề xuất]**.
  4. Hình học dài 0 và text rỗng (chỉ khoảng trắng hoặc mã định dạng) nhận đúng 100% bộ ca vàng; POINT và text chứa field không bị coi là rỗng.
  5. BENCH-STD 1 triệu entity: phân tích ≤ 1 s trên máy tầm trung, hiệu suất song song ≥ 60% đến 8 luồng; ước tính dung lượng giảm sai ≤ 10% so với tệp lưu thật **[đề xuất]**.
  6. Đạt DoD-C, DoD-H.

### CMD-253 · PURGE, -PURGE
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-252, INT-003, INT-020, CMD-002
- **Lệnh:** PURGE, -PURGE
- **Màn hình:** UI-B09
- **Đầu vào:** 03 Phụ lục A (PURGE); 07.2 UI-B09 (dọn mục chọn, dọn lồng nhau, xác nhận từng mục, dọn tất cả); API CMD-252; tài liệu lệnh PURGE công khai của AutoCAD.
- **Đầu ra:** `commands/utility/purge/PurgeCommand.{h,cpp}`; ca vàng `commands/utility/tests/purge/`; `docs/specs/commands/PURGE.md`, `-PURGE.md`.
- **Tiêu chí nghiệm thu:**
  1. -PURGE hỏi loại với đủ từ khóa toàn cục như AutoCAD (Blocks, DEtailviewstyles, Dimstyles, Groups, LAyers, LTypes, MAterials, MUltileaderstyles, Plotstyles, SHapes, textSTyles, Mlinestyles, SEctionviewstyles, Tablestyles, Visualstyles, Regapps, Zero-length geometry, Empty text objects, Orphaned data, All), rồi tên có wildcard `<*>`, rồi "Xác nhận từng tên? [Có/Không] <Có>".
  2. PURGE gõ từ bàn phím phát yêu cầu UI-B09 dùng mô hình CMD-252; gọi từ `.scr`, `(command ...)`, `trion-console` chạy dạng dòng lệnh như AutoCAD (khớp QA-007).
  3. Mỗi lần dọn (kể cả lồng nhau, Dọn tất cả) là đúng một mark undo; U khôi phục đủ mục và tham chiếu, hash trùng trước khi dọn; dòng kết quả "Đã xóa n <loại>" theo từng loại.
  4. Dọn 100.000 regapps xong ≤ 1 s, luồng UI dừng ≤ 1 khung **[đề xuất]**; tệp lưu sau dọn được ba bộ đọc QA-004 mở không lỗi.
  5. KTL ba đường nhập cùng hash; Viewer bị chặn (INT-020).
  6. Đạt DoD-C, DoD-H, DoD-L.

### CMD-254 · RECOVERALL: phục hồi bản vẽ và mọi xref lồng nhau
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-239, CMD-251, CMD-243, CMD-238, CMD-247, DOC-013, NT-013
- **Lệnh:** RECOVERALL
- **Màn hình:** UI-B04, UI-B10
- **Đầu vào:** 03 §2 (RECOVERALL P1); 07.1 UI-B04 (phục hồi cả xref lồng nhau, báo cáo theo tệp); API CMD-239, CMD-251, CMD-243, đồ thị xref DOC-013, phân giải đường dẫn CMD-238, khóa CMD-247.
- **Đầu ra:** `commands/utility/RecoverAllCommand.{h,cpp}`: duyệt cây xref (chu trình, xref vắng), phục hồi, AUDIT có sửa, lưu từng tệp kèm `.bak`, báo cáo theo tệp; `docs/specs/commands/RECOVERALL.md`.
- **Tiêu chí nghiệm thu:**
  1. Mỗi tệp trong cây xref (attach lồng nhau, overlay cấp 1) được phục hồi, kiểm, lưu đúng một lần, kể cả khi nhiều cha cùng tham chiếu; chu trình xref không gây lặp vô hạn.
  2. Lưu giữ phiên bản gốc khi có bộ ghi, nếu không thì 2018 và ghi vào báo cáo **[đề xuất]**; mọi tệp bị ghi có `.bak` bản trước.
  3. Tệp chỉ đọc, đang bị khóa, không tìm thấy: bỏ qua, ghi lý do vào báo cáo, không dừng cả chuỗi.
  4. Bộ cây xref hỏng tự soạn: sau RECOVERALL, AUDIT từng tệp báo 0 lỗi; cây 50 tệp, 500 MB xong ≤ 30 s trên máy tầm trung, tệp độc lập phục hồi song song **[đề xuất]**; hủy giữa chừng không để tệp ghi dở.
  5. Báo cáo theo tệp (lỗi tìm thấy, đã sửa, đã lưu, bỏ qua) xuất được văn bản, cùng định dạng UI-B10.
  6. Đạt DoD-C, DoD-H.

## 16. Đơn vị, giới hạn, đổi tên, tùy chọn, thuộc tính bản vẽ

### CMD-255 · UNITS, -UNITS
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** INT-005, INT-003, INT-004, RGN-013, INT-020, CMD-002
- **Lệnh:** UNITS, -UNITS
- **Màn hình:** UI-D08
- **Đầu vào:** 03 Phụ lục A (UNITS); 07.2 UI-D08; API INT-005 (sysvar), RGN-013 (định dạng số cho dòng ví dụ); tài liệu lệnh UNITS công khai của AutoCAD.
- **Đầu ra:** `commands/utility/UnitsCommand.{h,cpp}`; mô hình dữ liệu cho UI-D08 (giá trị, dòng ví dụ, chọn góc gốc trên bản vẽ); ca vàng; `docs/specs/commands/UNITS.md`, `-UNITS.md`.
- **Tiêu chí nghiệm thu:**
  1. -UNITS hỏi đủ chuỗi như AutoCAD: kiểu độ dài 1–5; số chữ số 0–8 hoặc mẫu số phân số 1–256; kiểu góc 1–5; số chữ số góc; hướng góc 0 (gõ hoặc chọn hai điểm); "Đo góc theo chiều kim đồng hồ? [Có/Không] <Không>". Giá trị ngoài phạm vi bị từ chối và hỏi lại.
  2. Ghi đúng LUNITS, LUPREC, AUNITS, AUPREC, ANGBASE, ANGDIR, INSUNITS, MEASUREMENT; round-trip qua lưu 2018 và 2004 giữ nguyên (QA-003).
  3. Dòng ví dụ của UI-D08 trùng từng ký tự với RGN-013 trên 1.000 tổ hợp ngẫu nhiên; cập nhật ≤ 16 ms sau mỗi thay đổi.
  4. Đổi đơn vị không regen hình học; tọa độ thanh trạng thái và Properties đổi định dạng trong khung kế tiếp.
  5. Một mark undo, U trả mọi sysvar về giá trị cũ; KTL ba đường nhập cùng hash; Viewer bị chặn (INT-020).
  6. Đạt DoD-C, DoD-L.

### CMD-256 · LIMITS
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** INT-004, INT-005, INT-016, DOC-011, CMD-002
- **Lệnh:** LIMITS
- **Đầu vào:** tài liệu lệnh LIMITS công khai của AutoCAD; API INT-004 (nhập điểm), INT-005 (LIMMIN, LIMMAX, LIMCHECK), DOC-011 (giới hạn riêng của model space và từng layout).
- **Đầu ra:** `commands/utility/LimitsCommand.{h,cpp}`; bộ kiểm điểm theo LIMCHECK gắn vào đường nhập điểm của INT-004; ca vàng; `docs/specs/commands/LIMITS.md`.
- **Tiêu chí nghiệm thu:**
  1. "Chỉ định góc dưới trái hoặc [Bật/Tắt] <0,0>" rồi "Chỉ định góc trên phải <420,297>"; Bật/Tắt đặt LIMCHECK; góc trên phải không lớn hơn góc dưới trái thì báo và hỏi lại.
  2. Model space và từng layout giữ giới hạn riêng; lưu rồi đọc lại đúng (QA-003).
  3. LIMCHECK = 1: điểm ngoài giới hạn bị từ chối với "**Ngoài giới hạn" và hỏi lại, cho mọi lệnh nhận điểm và mọi nguồn (bàn phím, OSNAP, DYN, `.scr`, LISP).
  4. Kiểm điểm thêm ≤ 1 µs mỗi điểm **[đề xuất]**; ZOOM All (INT-016) dùng giới hạn mới ngay.
  5. Một mark undo; KTL ba đường nhập cùng hash.
  6. Đạt DoD-C, DoD-L.

### CMD-257 · RENAME, -RENAME
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DOC-004, DOC-007, NT-012, INT-003, INT-020, CMD-002
- **Lệnh:** RENAME, -RENAME
- **Màn hình:** UI-B12
- **Đầu vào:** 03 Phụ lục A (RENAME); 07.2 UI-B12; prototype `#UI-B12` (`IMPL.RENAME` trong `js/screens8.js`: loại, mục cố định, wildcard); API DOC-004 (tra và đổi tên trong bảng ký hiệu), NT-012 (so sánh không phân biệt hoa thường có dấu).
- **Đầu ra:** `commands/utility/RenameCommand.{h,cpp}`: đổi một tên và đổi hàng loạt theo wildcard (`KT-*` → `A-*`) có xem trước; ca vàng; `docs/specs/commands/RENAME.md`, `-RENAME.md`.
- **Tiêu chí nghiệm thu:**
  1. -RENAME hỏi loại [Block/Dimstyle/LAyer/LType/Material/multileadeRstyle/Plotstyle/textStyle/Tablestyle/Ucs/VIew/VPort], tên cũ, tên mới như AutoCAD; đủ loại của UI-B12.
  2. Từ chối: mục cố định (danh sách theo prototype UI-B12, chốt bằng QA-007), mục phụ thuộc xref (`XREF|tên`), block vô danh (`*U`, `*D`), tên trùng so không phân biệt hoa thường có dấu ("KT-Tường" trùng "kt-tường"), ký tự cấm `< > / \ " : ; ? * | , =`, tên quá dài.
  3. Wildcard: xem trước "Sẽ đổi n mục, ví dụ A → B"; xung đột tên trong lô thì không đổi mục nào và liệt kê xung đột; đổi 10.000 layer ≤ 200 ms, một mark undo **[đề xuất]**.
  4. Tham chiếu theo handle giữ nguyên; tham chiếu lưu theo tên (layer state, bộ lọc layer) xử lý khớp 100% bộ tham chiếu QA-007 của RENAME.
  5. U khôi phục tên cũ, hash trùng; KTL ba đường nhập cùng hash; Viewer bị chặn (INT-020).
  6. Đạt DoD-C, DoD-L.

### CMD-258 · OPTIONS: mở hộp thoại Tùy chọn
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** INT-001, INT-002, INT-005, INT-020, NT-010, CMD-001
- **Lệnh:** OPTIONS
- **Màn hình:** UI-D10
- **Đầu vào:** 03 Phụ lục A (OPTIONS); 07.2 UI-D10 (mở từ `OPTIONS`, menu ứng dụng, chuột phải; ô tìm thiết lập); API INT-001, INT-005, NT-010 (kho cài đặt), INT-020.
- **Đầu ra:** `commands/utility/OptionsCommand.{h,cpp}`: phát yêu cầu mở UI-D10 kèm tab hoặc khóa thiết lập; dạng `+OPTIONS`; `docs/specs/commands/OPTIONS.md`.
- **Tiêu chí nghiệm thu:**
  1. OPTIONS phát yêu cầu mở UI-D10; `+OPTIONS` hỏi chỉ số tab như AutoCAD; tham số khóa thiết lập (ví dụ `SAVETIME`) mở thẳng tới ô đó.
  2. Lệnh kết thúc khi hộp thoại đóng; mọi thay đổi đi qua INT-005 (sysvar) hoặc NT-010 (cài đặt), không có đường ghi nào khác (test bằng kiểm tra phụ thuộc mã nguồn).
  3. `.scr` và `(command "OPTIONS")` mở hộp thoại và chờ như AutoCAD; `trion-console` báo lỗi "cần giao diện", mã thoát khác 0, không treo.
  4. Viewer mở được, chỉ thấy các tab của Viewer (INT-020); từ lệnh đến khung hình đầu của hộp thoại ≤ 150 ms (QA-011).
  5. Đạt DoD-C, DoD-L.

### CMD-259 · DWGPROPS: mô hình thuộc tính bản vẽ (xem)
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** DWG-027, DOC-010, QA-004, INT-020, CMD-001
- **Lệnh:** DWGPROPS
- **Màn hình:** UI-B08
- **Đầu vào:** 03 §2 (DWGPROPS P1); 07.2 UI-B08 (Chung, Tóm tắt, Thống kê, Tùy biến); prototype `#UI-B08` (`js/screens8.js`); API DWG-027 (SummaryInfo), DOC-010 (TDCREATE, TDUPDATE, TDINDWG).
- **Đầu ra:** `commands/utility/DwgProps.{h,cpp}`: `DwgPropsModel` bốn tab (thông tin tệp từ hệ điều hành; tiêu đề, chủ đề, tác giả, từ khóa, ghi chú, gốc hyperlink; ngày tạo, ngày sửa, người lưu cuối, số lần sửa, tổng thời gian sửa; danh sách tên/giá trị tùy biến); lệnh phát yêu cầu UI-B08; `docs/specs/commands/DWGPROPS.md`.
- **Tiêu chí nghiệm thu:**
  1. 500 tệp QA-001 có SummaryInfo (R2000–2018, chuỗi tiếng Việt theo DWGCODEPAGE và UTF-16): mọi trường khớp 100% với LibreDWG CLI (QA-004).
  2. Tệp thiếu hoặc hỏng section SummaryInfo: mô hình rỗng hợp lệ, không lỗi, không chặn mở tệp.
  3. Ngày hiển thị dd/mm/yyyy; tổng thời gian sửa từ TDINDWG dạng ngày, giờ, phút; giá trị ở tab Thống kê khớp DWG TrueView trên 20 tệp mẫu.
  4. Dựng mô hình ≤ 5 ms, không đọc lại tệp **[đề xuất]**; Viewer mở ở chế độ chỉ xem (INT-020).
  5. KTL: bàn phím, `.scr`, `(command ...)` cho cùng mô hình.
  6. Đạt DoD-C, DoD-L.

### CMD-260 · DWGPROPS: sửa, lưu, thuộc tính tùy biến cho field
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-259, CMD-243, DOC-007, DWG-028, CMD-002
- **Màn hình:** UI-B08
- **Đầu vào:** 07.2 UI-B08 (EA: sửa; Tùy biến dùng cho field trong khung tên); API CMD-259, DOC-007, CMD-243 (ghi SummaryInfo qua DWG-027), DWG-028 (mã hóa chuỗi).
- **Đầu ra:** `commands/utility/DwgPropsEdit.{h,cpp}`: sửa Tóm tắt và Tùy biến qua transaction; API đọc thuộc tính tùy biến theo tên cho field (`CustomDP`); test.
- **Tiêu chí nghiệm thu:**
  1. Sửa tiêu đề, tác giả, từ khóa, gốc hyperlink; thêm, xóa, đổi tên thuộc tính tùy biến: mỗi lần áp là một mark undo, đặt DBMOD; tên tùy biến trùng (không phân biệt hoa thường, có dấu) bị từ chối.
  2. Lưu 2018 và 2004 rồi đọc lại trùng 100% (QA-003); bản 2004 ghi theo DWGCODEPAGE, ký tự ngoài code page ghi dạng `\U+xxxx` (DWG-028); AutoCAD đọc đúng (bộ tham chiếu QA-007).
  3. Tab Thống kê hiện TDINDWG, TDUPDATE do engine lưu CMD-243 cập nhật, khớp giá trị trong tệp vừa lưu.
  4. `CustomDP` trả giá trị theo tên ≤ 1 µs **[đề xuất]** và phát sự kiện khi đổi để field cập nhật (RGN-025).
  5. Viewer, Viewer Pro chỉ xem; KTL ba đường nhập cùng hash.
  6. Đạt DoD-C.

## 17. Đóng gói gửi đi (ETRANSMIT)

### CMD-261 · Đồ thị tệp phụ thuộc của bản vẽ cho đóng gói
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-238, DOC-013, RGN-006, VN-007, PLT-001, PLT-002, NT-011, NT-005
- **Màn hình:** UI-B11
- **Đầu vào:** 03 §2 (ETRANSMIT: xref, font, CTB, ảnh; đóng gói cả font SHX tiếng Việt); 07.2 UI-B11 (Cây tệp / Bảng tệp); prototype `#UI-B11` (`txFiles` trong `js/screens12.js`); API CMD-238 (quét tham chiếu), DOC-013, RGN-006 (FontResolver), VN-007 (bộ font TrionCAD), PLT-001, PLT-002.
- **Đầu ra:** `commands/utility/transmit/TransmittalGraph.{h,cpp}`: `collect(doc, TransmitOptions)` → cây tệp theo nhóm (bản vẽ, xref lồng nhau, ảnh, PDF underlay, font SHX/bigfont/TTF, shape của linetype phức, CTB/STB, PC3/PMP, tệp thêm); mỗi mục có đường dẫn lưu, đường dẫn tìm được, kích thước, trạng thái (thiếu, font hệ thống không gửi kèm, không cần vì chuyển Unicode); test.
- **Tiêu chí nghiệm thu:**
  1. 100 bộ hồ sơ mẫu tự soạn (xref lồng 5 cấp, overlay, ảnh, font tiếng Việt, CTB, PC3): thu đủ 100% tệp so với danh sách kỳ vọng; xref lồng chỉ nạp lười, không regen.
  2. Font lấy từ kiểu chữ, kiểu chữ của kích thước, bảng, mleader, mã `\f`/`\F` trong MTEXT, shape của linetype, kể cả trong xref; font SHX tiếng Việt và font của TrionCAD (VN-007) đánh dấu gửi kèm được; font TTF của hệ điều hành đánh dấu không gửi kèm.
  3. Chu trình xref và cùng tệp qua hai đường dẫn (so theo đường dẫn chuẩn hóa) chỉ xuất hiện một lần.
  4. Hồ sơ 200 tệp ≤ 3 s trên máy tầm trung, đọc song song **[đề xuất]**; hủy ≤ 50 ms; bản vẽ đang mở không đổi (hash giữ nguyên).
  5. Đạt DoD-C, DoD-H.

### CMD-262 · Ghi gói: thư mục hoặc ZIP, sửa đường dẫn, báo cáo
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-261, CMD-243, NT-015, NT-011, NT-005, NT-013
- **Màn hình:** UI-B11
- **Đầu vào:** 07.2 UI-B11 (kiểu gói, thư mục đích, cách đặt tên, cấu trúc đường dẫn, báo cáo, ghi chú); prototype `txReport` trong `js/screens12.js`; API CMD-261, CMD-243 (lưu bản sao), NT-015 (ZIP), NT-011 (ghi an toàn).
- **Đầu ra:** `commands/utility/transmit/PackageWriter.{h,cpp}`: sao chép hoặc nén ZIP; ba kiểu cấu trúc (giữ cây thư mục, một thư mục phẳng, giữ nguyên); `PathRewriter` sửa đường dẫn xref, ảnh, underlay trong bản sao thành tương đối; báo cáo `.txt`; test.
- **Tiêu chí nghiệm thu:**
  1. Giải nén gói trên máy CI sạch, ổ khác, không có thư mục gốc: bản vẽ chủ mở với 0 tham chiếu thiếu (trừ mục đã báo thiếu từ đầu), đúng cho cả ba kiểu cấu trúc.
  2. Thư mục phẳng: tên trùng được đổi tên, không đè nhau, đường dẫn trong bản sao trỏ đúng; tên tiếng Việt có dấu giữ nguyên trong ZIP (cờ UTF-8).
  3. Bản vẽ và tệp gốc không bị sửa (hash, mtime giữ nguyên); gói ghi tạm rồi đổi tên, hủy giữa chừng không để gói dở.
  4. Gói 1 GB ≤ 20 s trên NVMe máy tầm trung, nén song song theo tệp **[đề xuất]**; có tiến trình và nút hủy.
  5. Báo cáo liệt kê tệp trong gói, tệp thiếu, tệp không đưa vào kèm lý do, ghi chú người dùng; nội dung khớp UI-B11.
  6. Đạt DoD-C, DoD-H.

### CMD-263 · ETRANSMIT, -ETRANSMIT
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-261, CMD-262, CMD-252, VN-005, PLT-001, DWG-023, CMD:XREF, NT-010, CMD-001
- **Lệnh:** ETRANSMIT, -ETRANSMIT
- **Màn hình:** UI-B11
- **Đầu vào:** 03 §2 (ETRANSMIT P1); 07.2 UI-B11 (thiết lập đóng gói, hành động trên bản sao); prototype `TX_SETS` trong `js/screens12.js`; API CMD-261, CMD-262, CMD-252 (dọn dẹp), VN-005 (chuyển Unicode), bind xref (CMD:XREF), PLT-001 (máy in "Không"), DWG-023 (phiên bản cũ).
- **Đầu ra:** `commands/utility/transmit/EtransmitCommand.{h,cpp}`; kho thiết lập đóng gói có tên trong NT-010 (mặc định Standard); ca vàng; `docs/specs/commands/ETRANSMIT.md`, `-ETRANSMIT.md`.
- **Tiêu chí nghiệm thu:**
  1. -ETRANSMIT hỏi với từ khóa toàn cục như AutoCAD (Create transmittal package, Report only, CUrrent setup, CHoose setup; mặc định Report only); ETRANSMIT gõ từ bàn phím phát yêu cầu UI-B11.
  2. Hành động chỉ áp lên bản sao, thứ tự cố định: bind xref → dọn dẹp → chuyển font Unicode → máy in "Không" → lưu theo định dạng đích (giữ nguyên, 2018, 2013, 2010, 2007, 2004, 2000); bản gốc không đổi.
  3. Bản sao đã chuyển Unicode: 0 ký tự lỗi mã trên bộ mẫu VN-003; font tiếng Việt cũ không còn trong gói, font Unicode tương ứng được thêm.
  4. Gói lưu 2013 và 2004: ba bộ đọc QA-004 mở không lỗi; bind xref cho hình học khớp bản do AutoCAD bind (QA-007).
  5. Bản vẽ đang có thay đổi chưa lưu: hỏi lưu trước như AutoCAD; KTL ba đường nhập cho cùng danh sách tệp và cùng báo cáo.
  6. Đạt DoD-C, DoD-H, DoD-L.
- **Ngoài phạm vi:** đóng gói sheet set, tải lên dịch vụ đám mây.

## 18. Xử lý tệp hàng loạt (TCBATCH, chuyển phiên bản)

### CMD-264 · Bộ chạy xử lý hàng loạt: chuỗi thao tác trên nhiều tệp
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** VN-006, VN-005, CMD-239, CMD-251, CMD-252, CMD-243, PLT-001, DWG-023, NT-013, NT-008
- **Màn hình:** UI-B14
- **Đầu vào:** 03 §2 (chuyển phiên bản hàng loạt P1); 07.2 UI-B14 (đầu vào, thao tác theo thứ tự, ba kiểu đầu ra, chạy nền, tạm dừng, báo cáo, mẫu thiết lập); bộ chạy nhiều tệp của VN-006; API CMD-239, CMD-251, CMD-252, VN-005, PLT-001, CMD-243, DWG-023.
- **Đầu ra:** `commands/utility/batch/BatchJob.{h,cpp}`: danh sách `BatchOp` có thứ tự (kiểm và sửa lỗi, dọn dẹp gồm regapps, chuyển font Unicode, chuyển phiên bản DWG/DXF, máy in của layout về "Không") cắm vào bộ chạy của VN-006; ba kiểu đầu ra (ghi đè có `.bak`, thư mục khác giữ cấu trúc, thêm hậu tố); mẫu thiết lập JSON `.tcbatch`; báo cáo theo tệp; test; `commands/utility/bench/batch_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. FIXFONTBATCH chạy trên cùng bộ chạy như một cấu hình chỉ có thao tác chuyển font; mọi test của VN-006 vẫn đạt; không có bộ chạy nhiều tệp thứ hai.
  2. 1.000 tệp QA-001 với đủ thao tác: tệp mở lỗi (thử chế độ phục hồi khi bật kiểm lỗi), ghi lỗi, crash giả lập trong một thao tác chỉ hỏng mục của tệp đó; báo cáo ghi trạng thái, thao tác đã áp, dung lượng trước và sau từng tệp.
  3. Ghi đè luôn tạo `.bak` và ghi nguyên tử; thư mục khác giữ cấu trúc thư mục con; hậu tố không đè tệp có sẵn.
  4. Song song theo tệp, số tệp đồng thời giới hạn để bộ nhớ ≤ 70% RAM máy (NT-008) **[đề xuất]**; 1.000 tệp 2 MB chuyển 2018 → 2013 ≤ 3 phút trên máy tầm trung **[đề xuất]**; tạm dừng, tiếp tục, hủy ≤ 1 s.
  5. Tất định: cùng đầu vào và mẫu thiết lập cho tệp đầu ra cùng hash (bỏ trường thời gian theo testkit) ở 1 luồng và 16 luồng.
  6. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** thao tác chạy `.scr` trên từng tệp (P2).

### CMD-265 · TCBATCH, DWGCONVERT và chế độ hàng loạt của `trion-console`
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-264, INT-019, INT-020, PLF-002, CMD-001
- **Lệnh:** TCBATCH, DWGCONVERT
- **Màn hình:** UI-B14
- **Đầu vào:** 03 §2 (DWG Convert chạy qua `trion-console`); 07.2 UI-B14 (Viewer Pro chỉ bật chuyển font); prototype `#UI-B14` (`IMPL.TCBATCH` trong `js/screens12.js`); API CMD-264, INT-019.
- **Đầu ra:** `commands/utility/batch/BatchCommands.{h,cpp}`; lệnh con `trion-console batch --preset <tệp> <đầu vào…>` và `trion-console convert --to <phiên bản> --out <thư mục> <đầu vào…>`; `docs/specs/commands/TCBATCH.md`, `DWGCONVERT.md`.
- **Tiêu chí nghiệm thu:**
  1. TCBATCH phát yêu cầu UI-B14 với đủ thao tác; DWGCONVERT mở cùng màn hình, chỉ bật thao tác chuyển phiên bản.
  2. Cổng phiên bản (PLF-002): Viewer Pro chỉ bật chuyển font; Viewer bị chặn; `trion-console` kiểm cùng quyền.
  3. `trion-console batch` và `convert` cho tệp đầu ra cùng hash với chạy trong ứng dụng cùng mẫu thiết lập; mã thoát 0 khi mọi tệp đạt, 2 khi có tệp lỗi, 1 khi sai tham số **[đề xuất]**; báo cáo JSON theo tệp.
  4. Console chạy được trên CI Linux không có màn hình; Ctrl+C hủy sạch, không để tệp ghi dở.
  5. Đạt DoD-C.

## 19. Tiện ích tệp sau v1.0

### CMD-266 · REVERT, QQUIT
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** S · **Phụ thuộc:** CMD-240, CMD-242, CMD-244, CMD-247, CMD-001
- **Lệnh:** REVERT, QQUIT
- **Đầu vào:** tài liệu Express Tools công khai của AutoCAD (REVERT, QQUIT); API CMD-240, CMD-242, CMD-244, khóa CMD-247.
- **Đầu ra:** `commands/file/RevertQuit.{h,cpp}`; ca vàng; `docs/specs/commands/REVERT.md`, `QQUIT.md`.
- **Tiêu chí nghiệm thu:**
  1. REVERT trên bản vẽ đã sửa hỏi "Bỏ mọi thay đổi và mở lại từ đĩa? [Có/Không] <Không>"; Có thì mở lại cùng đường dẫn, giữ vị trí tab và view; bản vẽ chưa đặt tên thì báo không làm được.
  2. Sau REVERT: hash bản vẽ trùng tệp trên đĩa, lịch sử undo rỗng; khóa `.dwl` giữ liên tục, tiến trình khác không chiếm được khóa trong lúc mở lại.
  3. QQUIT theo hành vi Express Tools: một danh sách mọi bản vẽ chưa lưu (UI-B03), lưu các bản được chọn rồi thoát; Hủy dừng thoát.
  4. KTL ba đường nhập.
  5. Đạt DoD-C.

### CMD-267 · MOVEBAK, REDIR
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-243, CMD-262, CMD-238, NT-010, INT-003, CMD-002
- **Lệnh:** MOVEBAK, REDIR
- **Đầu vào:** tài liệu Express Tools công khai của AutoCAD (MOVEBAK, REDIR); `PathRewriter` của CMD-262; API CMD-243 (`.bak`), CMD-238 (phân giải lại tham chiếu).
- **Đầu ra:** `commands/utility/RedirCommands.{h,cpp}`; ca vàng; `docs/specs/commands/MOVEBAK.md`, `REDIR.md`.
- **Tiêu chí nghiệm thu:**
  1. MOVEBAK hỏi thư mục chứa `.bak` ("." để về mặc định); từ đó `.bak` ghi vào thư mục đã chọn (lưu trong NT-010); thư mục không ghi được thì báo và giữ giá trị cũ.
  2. REDIR thay đường dẫn cũ → mới (có `*`) cho xref, ảnh, underlay, font của kiểu chữ, shape; chọn loại đối tượng như Express Tools; xem trước số đường dẫn sẽ đổi.
  3. REDIR một mark undo; 10.000 đường dẫn ≤ 200 ms **[đề xuất]**; tham chiếu phân giải lại ngay, danh sách thiếu của CMD-238 cập nhật.
  4. KTL ba đường nhập cùng hash.
  5. Đạt DoD-C.

### CMD-268 · XDATA, XDLIST
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** S · **Phụ thuộc:** DOC-005, INT-003, INT-008, CMD-002
- **Lệnh:** XDATA, XDLIST
- **Đầu vào:** tài liệu Express Tools công khai của AutoCAD (XDATA, XDLIST); API DOC-005 (xdata, regapp), INT-008 (chọn đối tượng).
- **Đầu ra:** `commands/utility/XdataCommands.{h,cpp}`; ca vàng; `docs/specs/commands/XDATA.md`, `XDLIST.md`.
- **Tiêu chí nghiệm thu:**
  1. XDATA: chọn đối tượng, tên ứng dụng (tạo regapp nếu chưa có), rồi lặp hỏi kiểu [3Real/DIR/DISP/DIST/Hand/Int/LAyer/LOng/Pos/Real/SCale/STr/eXit] và giá trị như Express Tools; ghi đúng mã nhóm 1000–1071.
  2. Vượt giới hạn 16 KB xdata mỗi đối tượng thì báo, không ghi một phần.
  3. XDLIST in xdata theo ứng dụng hoặc `*`, đúng mã nhóm và giá trị, chuỗi tiếng Việt hiển thị đúng.
  4. XDATA một mark undo; KTL ba đường nhập cùng hash.
  5. Đạt DoD-C.

### CMD-269 · EDITTIME
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** S · **Phụ thuộc:** CMD-260, DOC-112, DOC-005
- **Lệnh:** EDITTIME
- **Đầu vào:** tài liệu Express Tools công khai của AutoCAD (EDITTIME); API DOC-112 (sự kiện trạng thái), DOC-005 (xrecord lưu bộ đếm), CMD-260 (TDINDWG).
- **Đầu ra:** `commands/utility/EditTime.{h,cpp}`; `docs/specs/commands/EDITTIME.md`.
- **Tiêu chí nghiệm thu:**
  1. Đếm thời gian sửa thực, bỏ khoảng rảnh dài hơn ngưỡng (mặc định 5 phút **[đề xuất]**); tách theo người dùng; lưu trong bản vẽ và đọc lại đúng sau round-trip (QA-003).
  2. Không dùng timer chạy nền khi rảnh: tính từ dấu thời gian của sự kiện nhập; CPU ~0% khi rảnh.
  3. Các tùy chọn bật, tắt, hiển thị, đặt lại theo Express Tools; sai số ≤ 1 s mỗi giờ trên đồng hồ giả lập.
  4. Đạt DoD-C.

## Tóm tắt theo mốc

| Mốc | Task | Số task |
|---|---|---|
| V | CMD-237, CMD-238, CMD-239, CMD-240, CMD-242, CMD-258, CMD-259 | 7 |
| VP | CMD-231, CMD-232, CMD-243, CMD-244 | 4 |
| EA | CMD-151, CMD-152, CMD-153, CMD-154, CMD-155, CMD-156, CMD-157, CMD-158, CMD-159, CMD-163, CMD-164, CMD-165, CMD-166, CMD-167, CMD-168, CMD-170, CMD-171, CMD-172, CMD-173, CMD-174, CMD-175, CMD-176, CMD-177, CMD-178, CMD-179, CMD-180, CMD-182, CMD-183, CMD-184, CMD-188, CMD-191, CMD-233, CMD-234, CMD-235, CMD-241, CMD-245, CMD-246, CMD-247, CMD-248, CMD-249, CMD-250, CMD-251, CMD-252, CMD-253, CMD-255, CMD-256, CMD-257, CMD-260 | 48 |
| 1.0 | CMD-160, CMD-169, CMD-181, CMD-185, CMD-186, CMD-187, CMD-189, CMD-190, CMD-192, CMD-193, CMD-236, CMD-254, CMD-261, CMD-262, CMD-263, CMD-264, CMD-265 | 17 |
| Sau | CMD-161, CMD-162, CMD-266, CMD-267, CMD-268, CMD-269 | 6 |
| **Tổng** | | **82** |
