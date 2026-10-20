# 08 — Phân rã task cho agent: lõi (core) và giao diện (UI)

> Bộ tài liệu này chia toàn bộ việc kỹ thuật đến v1.0 (và phần P2 đã có thiết kế UI) thành các **task vừa đủ nhỏ để một agent lập trình hoàn thành trong một phiên**. Mỗi task ghi rõ **đầu vào**, **đầu ra** và **tiêu chí nghiệm thu** đo được.
> - Căn cứ: [03](../03-chuc-nang-autocad.md) (chức năng, ưu tiên), [05](../05-kien-truc-ky-thuat.md) (kiến trúc, ngân sách hiệu năng và độ chính xác), [06](../06-lo-trinh-trien-khai.md) (lộ trình), [07.1](../07-dac-ta-ui-1-khung-ung-dung.md) · [07.2](../07-dac-ta-ui-2-bang-va-hop-thoai.md) · [07.3](../07-dac-ta-ui-3-tinh-nang-vn-va-he-thong.md) (đặc tả UI), prototype `design/prototype/`.
> - Ngoài phạm vi: 3D mô hình hóa, render, đám mây, AI, DGN/IFC… (07.3 §8). Các nhóm này có bộ task riêng khi được đưa vào lộ trình.
> - Mục **[đề xuất]** là ngưỡng do bộ task này đặt thêm (05 và 07 chưa ghi), cần chốt lại khi đo trên máy tham chiếu.

## 1. Cách dùng

1. Chọn task có mọi **Phụ thuộc** đã xong (trạng thái theo dõi ở công cụ quản lý dự án, không ghi trong tệp này).
2. Giao cho agent: README này (§2–§5) + khối task + các tài liệu ghi ở **Đầu vào**.
3. Agent nộp một PR: mã, kiểm thử, benchmark, tài liệu đúng mục **Đầu ra**. PR chỉ được merge khi đạt toàn bộ **Tiêu chí nghiệm thu** và các DoD áp dụng (§3).
4. Phát hiện việc chưa có task (thiếu, phải tách nhỏ): thêm task mới vào đúng tệp module, số kế tiếp, ghi lý do trong PR.
5. Mỗi lần sửa bộ task, chạy `python3 tools/check_tasks.py` (thêm `-v` để xem mọi cảnh báo). Script kiểm cú pháp trường, mã trùng, phụ thuộc trỏ tới task có thật, chu trình, mốc của phụ thuộc không muộn hơn task, đủ task neo, đủ 172 mã màn hình và lệnh P0 của 03 Phụ lục A. Còn LỖI thì không merge.

## 2. Quy ước task

### 2.1 Mẫu

```
### GEO-003 · Predicate bền vững orient2d, incircle
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-001, NT-003
- **Lệnh:** (chỉ ghi khi task hiện thực lệnh, ví dụ TRIM, EXTEND)
- **Màn hình:** (chỉ task UI, hoặc task lõi phục vụ trực tiếp một màn hình: UI-C03)
- **Đầu vào:** đặc tả, mục tài liệu, dữ liệu kiểm thử, API của task phụ thuộc, prototype.
- **Đầu ra:** tệp/thư mục mã, API công khai, kiểm thử, benchmark, tài liệu.
- **Tiêu chí nghiệm thu:**
  1. Tiêu chí riêng, đo được (con số, bộ dữ liệu, cách đo).
  2. …
  3. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** những gì task này cố ý không làm (nếu cần nói rõ).
```

### 2.2 Trường

| Trường | Giá trị |
|---|---|
| **Mã** | `TIỀNTỐ-NNN`, ví dụ `RGN-014`, `UIC-007`. Tiền tố theo module (§4). Số không dùng lại |
| **Mốc** | Mốc phát hành sớm nhất cần task: `S1`–`S6` (spike Giai đoạn 0, 05 §21), `CC` (công cụ miễn phí, T2–3), `V` (Viewer, T16–17), `VP` (Viewer Pro, T18), `EA` (Pro Early Access, T25), `1.0` (T36 ±4), `Sau` (sau v1.0) |
| **Ưu tiên** | P0, P1, P2 theo [03](../03-chuc-nang-autocad.md) |
| **Cỡ** | `S` ≤ ~300 dòng mã sản phẩm · `M` ≤ ~800 · `L` ≤ ~1.500 (không tính mã sinh tự động, dữ liệu kiểm thử) **[đề xuất]**. Lớn hơn `L` thì phải tách |
| **Phụ thuộc** | Mã task, hoặc `CMD:TÊN` (task có dòng **Lệnh:** chứa TÊN), hoặc `UI-xxx` (mọi task có **Màn hình:** chứa mã đó) |

### 2.3 Quy tắc kích thước "vừa một agent"

- Một task tạo ra **một năng lực kiểm chứng được tự động**: có kiểm thử chạy trong CI dưới 10 phút, không cần người ngồi bấm.
- Chạm tối đa 1–2 module. Không gộp "viết engine" với "viết UI của engine" vào một task.
- Không có task "khung sườn để sau dùng": mỗi task phải có người dùng thật (task khác hoặc người dùng cuối) ngay khi xong.
- **Task neo** (§5) là điểm mà module khác phụ thuộc vào. Nếu cần tách task neo, mã neo giữ phần tích hợp và nghiệm thu cuối, các task con nhận số mới và là phụ thuộc của neo. Như vậy mọi tham chiếu chéo vẫn đúng.
- Spike (`S1`–`S6`) viết đúng chuẩn sản phẩm, không viết "để vứt". Phần chưa đủ chuẩn ghi rõ giới hạn ngay trong mã (05 §17).

## 3. Định nghĩa hoàn thành (DoD) chung

Mỗi task ghi DoD nào áp dụng ở dòng cuối của **Tiêu chí nghiệm thu**. Không lặp lại nội dung DoD trong từng task.

### DoD-C · Mọi task lõi

1. Biên dịch sạch trên MSVC (bản 2022 mới nhất) và Clang (≥ 17) cho Windows, macOS, Linux; cảnh báo là lỗi; clang-tidy sạch theo cấu hình kho mã.
2. Kiểm thử đơn vị cho mọi hàm công khai, kể cả nhánh lỗi. Bộ kiểm thử của module chạy dưới 60 giây.
3. ASan và UBSan sạch. Mã đa luồng chạy TSan sạch.
4. **Tất định:** cùng đầu vào cho cùng đầu ra từng bit trên 3 hệ điều hành và mọi số luồng (CI so hash kết quả, job NT-107). Không phụ thuộc thứ tự băm, địa chỉ bộ nhớ, thứ tự hoàn thành của luồng.
5. Không `-ffast-math`. Bật `-ffp-contract=off` (tắt FMA) ở `core/geom` và mọi mã quyết định tô-pô. So sánh số thực đi qua `Tolerance` (GEO-002), không so sánh `==` trực tiếp.
6. Không con trỏ thô sở hữu. Đường nóng không cấp phát trong vòng lặp (benchmark đếm số lần cấp phát, NT-004); dùng arena/pool (NT-007).
7. **Đầu vào không tin cậy** (tệp bản vẽ, LISP, DCL, PAT, LIN, SHX, PGP, CUIX, CTB, PC3, dữ liệu mạng): kiểm biên mọi lần đọc, giới hạn kích thước cấp phát (NT-103), có fuzz target chạy ≥ 1 giờ không lỗi trước khi merge, sau đó vào fuzzing liên tục (QA-006).
8. Mọi thay đổi bản vẽ đi qua API `core/doc` (DOC-007), có undo và sự kiện. Không có đường tắt.
9. Đường nóng có benchmark. Ngưỡng ghi trong task. CI chặn merge nếu vượt ngưỡng hoặc chậm hơn baseline quá 5% (NT-003).
10. Header công khai có chú thích. Chỗ cố ý làm đơn giản ghi rõ giới hạn và hướng nâng cấp ngay trong mã.
11. Không thêm phụ thuộc ngoài danh sách 05 §2. Cấm GPL/AGPL, SDK trả phí, mã Autodesk. Không đọc mã LibreDWG.

### DoD-H · Tận dụng phần cứng (task xử lý khối lượng lớn)

1. Việc trên khoảng 10.000 phần tử hoặc dài hơn 2 ms chạy song song qua bộ lập lịch NT-005. Không tự tạo `std::thread`.
2. Hiệu suất song song ≥ 70% so với tuyến tính đến 16 luồng cho đọc DWG, regen, dựng BVH, xuất PDF (05 §1.1). Các tác vụ song song khác ≥ 60% đến 8 luồng **[đề xuất]**. Kết quả đo 1→32 luồng lưu trong CI.
3. Vòng lặp số học trên mảng điểm, hộp bao, bit-stream, glyph dùng lớp SIMD NT-006 (chọn SSE4.2/AVX2/AVX-512/NEON lúc chạy), có đường scalar cho ra **cùng kết quả** để đối chiếu.
4. Bố cục dữ liệu SoA ở đường nóng; cấu trúc duyệt vừa dòng cache 64 byte.
5. Việc tương tác (khung hình, jig, OSNAP) chạy ưu tiên cao trên nhân P; việc nền ưu tiên thấp trên nhân E (NT-005). Việc nền hủy được trong ≤ 50 ms **[đề xuất]**.
6. Khi rảnh: ~0% CPU và GPU. Không polling, không timer chạy vô ích.
7. Chạy pin trên laptop: tôn trọng chính sách giảm luồng nền và tần số vẽ (RND-018).

### DoD-L · Độ trễ (task chạm đường tương tác)

Đo trên máy tham chiếu (§3.1) bằng QA-011 (UI) hoặc benchmark (lõi). Ghi p50 và p99; tiêu chí áp cho **p99** trừ khi ghi khác.

| Thao tác | Máy tầm trung | Máy thấp | Nguồn |
|---|---|---|---|
| Truy vấn OSNAP hoặc hit test tại con trỏ | ≤ 1 ms | ≤ 3 ms | 05 §1.1 |
| Cập nhật rubber-band, jig, xem trước trong lệnh | ≤ 8 ms/khung | ≤ 16 ms | 05 §1.1 |
| Khung hình pan/zoom 1 triệu entity 2D | ≤ 16,6 ms (≥ 60 fps) | ≤ 33 ms (≥ 30 fps) | 05 §1.1 |
| Hoàn tất lệnh cơ bản (TRIM, OFFSET, FILLET… trên đối tượng thường) | ≤ 20 ms | ≤ 50 ms | 05 §1.1 |
| Đổi trạng thái giao diện (toggle, mở menu, đổi tab ribbon) | ≤ 100 ms, mục tiêu trong 1 khung | ≤ 100 ms | 07.1 §1.3 |
| Ký tự gõ hiện trong ô nhập, dòng lệnh | ≤ 16 ms | ≤ 33 ms | **[đề xuất]** |
| Danh sách tự hoàn thành cập nhật sau mỗi phím | ≤ 16 ms | ≤ 33 ms | **[đề xuất]** |
| Mở palette hoặc hộp thoại đến khung hình đầu có nội dung | ≤ 150 ms | ≤ 300 ms | **[đề xuất]** |
| Palette cập nhật khi vùng chọn đổi (≤ 10.000 đối tượng) | ≤ 16 ms phần đồng bộ, phần còn lại nền | ≤ 33 ms | **[đề xuất]** |
| Cuộn danh sách, kéo palette, đổi kích thước cửa sổ | 60 fps, không khựng > 1 khung | 30 fps | **[đề xuất]** |
| Khởi động ấm | ≤ 1,5 s | ≤ 3 s | 05 §1.1 |
| Thao tác dài | > 0,5 s: con trỏ bận; > 2 s: tiến trình và nút hủy; không chặn vùng vẽ | | 07.1 §12 |

### DoD-U · Mọi task UI

1. **Khớp prototype** `design/prototype/index.html#UI-xxx` (mở bằng trình duyệt, chọn phiên bản ở nút Prototype): bố cục, kích thước, trạng thái, hành vi. Ảnh chụp kiểm (QA-011) ở Tối và Sáng, 100% / 150% / 200%, 1366×768 và 1920×1080. Chỗ nào Qt không làm giống được thì ghi lý do trong PR và chụp so sánh.
2. Chỉ dùng token sinh từ `design/prototype/css/tokens.css` (UI0-002). Không mã màu, cỡ chữ, khoảng cách viết thẳng.
3. Đạt danh sách kiểm 07.3 Phụ lục C: đủ trạng thái (mặc định, rê, nhấn, vô hiệu, khóa theo phiên bản, đang tải, rỗng, lỗi); chuỗi tiếng Việt dài hơn 40% không bị cắt; có bản English; dùng hoàn toàn bằng bàn phím; tooltip có tên lệnh tiếng Anh và F1; ô số theo UI-C09 (UI0-007); hành động phá hủy có xác nhận nêu số lượng và hoàn tác được; thao tác > 2 s có tiến trình và nút hủy; không dùng màu làm tín hiệu duy nhất; chữ tương phản ≥ 4,5:1; biểu tượng tự vẽ.
4. **Không chặn luồng UI:** việc dài hơn 2 ms (đọc tệp, quét bản vẽ, tính toán) chạy nền qua NT-005; kết quả áp vào UI theo lô. Danh sách hơn 200 dòng phải ảo hóa (model/view).
5. Mọi hành động **sửa bản vẽ** đi qua lệnh (INT-002), nhờ đó undo, script, LISP và macro nhất quán. UI chỉ đọc bản vẽ qua snapshot (DOC-009) hoặc API đọc của `core/doc`.
6. Mọi chuỗi nằm trong tệp dịch (UI0-005). Số trong vùng vẽ dùng dấu chấm thập phân theo `UNITS`; số ở giao diện chung dùng định dạng Việt Nam (1.250); ngày dd/mm/yyyy.
7. Có tên cho trình đọc màn hình, thứ tự focus hợp lý, Enter/Esc đúng. Animation ≤ 150 ms, tắt được, tôn trọng "giảm chuyển động" và chế độ Giao diện nhẹ.
8. Cổng phiên bản theo 07.1 §2 qua UI0-011.
9. Từ mốc macOS alpha (T26–27): đúng các khác biệt ở 07.1 §13.6.
10. Đạt các ngưỡng DoD-L liên quan; QA-011 đo trong CI.

### 3.1 Máy tham chiếu và dữ liệu chuẩn

| Máy | Cấu hình | Dùng cho |
|---|---|---|
| **Thấp** | Core i3 thế hệ 6–8, 8 GB RAM, Intel HD 520/620 | Ngưỡng "máy thấp" |
| **Tầm trung** | Core i5 thế hệ 10–12, 16 GB RAM, GPU tích hợp | Ngưỡng chính |
| **Mạnh** | Nhiều nhân (≥ 16 luồng), GPU rời | Scaling 1→32 luồng, 10 triệu entity |
| **Mac** | Apple Silicon | Metal, NEON, bộ nhớ hợp nhất |

Dữ liệu chung, do module QA quản lý (task neo ở §5):
- **Kho bản vẽ** (QA-001): bộ 1.000 tệp cho spike, bộ 10.000 tệp cho Viewer và v1.0.
- **BENCH-STD** (QA-001, QA-012): tệp 20 MB và 100 MB; cảnh 1 triệu và 10 triệu entity; hatch dày; text dày; block lồng; nhiều viewport; tọa độ VN-2000 (≈ 10⁹ đơn vị).
- **Bộ tham chiếu** (QA-007): kịch bản `.scr` do doanh nghiệp pilot có bản quyền AutoCAD chạy và gửi lại DWG/PDF làm đáp án. Trước khi có đáp án, task lệnh nghiệm thu bằng bất biến hình học và ca vàng tự soạn; khi có đáp án thì thêm tiêu chí khớp 100%.
- **Bộ LISP** (QA-008): 100 tệp ở Giai đoạn 0, 500 tệp đến v1.0.

## 4. Module, tệp task, thư mục mã

### 4.1 Bản đồ

| Tệp task | Tiền tố | Phạm vi | Thư mục mã |
|---|---|---|---|
| [core-nt-nen-tang.md](core-nt-nen-tang.md) | `NT` | Build, CI, cổng hiệu năng, bộ lập lịch, SIMD, bộ nhớ, hồ sơ phần cứng, log, cài đặt, I/O, mã hóa văn bản, nén, tác vụ nền | `cmake/`, `ci/`, `core/base/` |
| [core-qa-kiem-thu.md](core-qa-kiem-thu.md) | `QA` | Kho bản vẽ, round-trip, đối chứng chéo, so sánh ảnh, fuzzing, bộ tham chiếu, bộ LISP, kiểm thử UI tự động, bảng chỉ số | `qa/` |
| [core-plf-he-thong.md](core-plf-he-thong.md) | `PLF` | Giấy phép, cổng tính năng, kích hoạt, floating, cập nhật, crash, telemetry, trình cài đặt, dịch vụ phía máy chủ | `platform/`, `services/`, `packaging/` |
| [core-geo-hinh-hoc.md](core-geo-hinh-hoc.md) | `GEO` | Kernel hình học 2D (05 §7), BVH, bộ giải ràng buộc (P2) | `core/geom/` |
| [core-doc-mo-hinh.md](core-doc-mo-hinh.md) | `DOC` | Mô hình bản vẽ, handle, bảng ký hiệu, undo, snapshot, xref, phiên nhiều bản vẽ, autosave | `core/doc/` |
| [core-dwg-codec.md](core-dwg-codec.md) | `DWG` | Schema và codegen, codec DWG R13–2018 đọc/ghi, DXF, recover | `core/schema/`, `tools/codegen/`, `core/dwg/`, `core/dxf/` |
| [core-rgn-regen.md](core-rgn-regen.md) | `RGN` | Regen: font, text, MTEXT, linetype, hatch, kích thước, leader, bảng, block, dynamic block, proxy, ảnh, viewport, field, định dạng số | `core/regen/` |
| [core-rnd-renderer.md](core-rnd-renderer.md) | `RND` | Renderer trên Qt RHI, GPU compute, tự thích ứng, TRIONBENCH | `render/` |
| [core-int-tuong-tac.md](core-int-tuong-tac.md) | `INT` | Command framework, input pipeline, prompt, tọa độ, sysvar, PGP, script, chọn, OSNAP, tracking, jig, grip, DYN, UCS, điều hướng, `trion-console` | `cmd/`, `interact/`, `console/` |
| [core-cmd-1-ve-chon-truy-van.md](core-cmd-1-ve-chon-truy-van.md) | `CMD` 001–149 | Lệnh vẽ, hatch, chọn, hiển thị, truy vấn, ràng buộc tham số (P2), làm sạch hình học (P2) | `commands/draw/`, `commands/select/`, `commands/inquiry/`, `commands/display/` |
| [core-cmd-2-sua-tep-tien-ich.md](core-cmd-2-sua-tep-tien-ich.md) | `CMD` 151–299 | Lệnh sửa, undo, clipboard, tệp (NEW…SAVEAS, mẫu, autosave), tiện ích bản vẽ (PURGE, AUDIT, RECOVER, RENAME, ETRANSMIT, TCBATCH) | `commands/modify/`, `commands/file/`, `commands/utility/` |
| [core-cmd-3-chu-thich-layout.md](core-cmd-3-chu-thich-layout.md) | `CMD` 301–499 | Text, MTEXT, kiểu, kích thước, leader, bảng, field, tìm và thay, layout, viewport, tỉ lệ chú thích | `commands/annot/`, `commands/layout/` |
| [core-cmd-4-layer-block-xref.md](core-cmd-4-layer-block-xref.md) | `CMD` 501–699 | Layer và thuộc tính, block và attribute, xref, ảnh, PDF underlay, đếm, so sánh, trích xuất dữ liệu, tab Mở rộng (P1, P2), chuẩn `.dws` | `commands/layer/`, `commands/block/`, `commands/xref/`, `commands/data/`, `commands/express/` |
| [core-lsp-lisp-sdk.md](core-lsp-lisp-sdk.md) | `LSP`, `CUS`, `SDK` | AutoLISP/Visual LISP, DCL (parser và runtime), macro menu, nhập CUIX, chuyển cài đặt, workspace, SDK C/C++ | `lisp/`, `custom/`, `sdk/` |
| [core-plt-in-xuat.md](core-plt-in-xuat.md) | `PLT` | Thiết lập trang, CTB/STB, pipeline in, bộ ghi PDF, QPrinter, xem trước, xuất ảnh, PUBLISH, in hàng loạt theo khung tên, PC3, PDFium, nhập PDF, sheet set | `plot/` |
| [core-vn-viet-nam.md](core-vn-viet-nam.md) | `VN` | Bác sĩ Font, bộ font SHX, nội dung TCVN, công cụ VN (TC…), Excel, công cụ miễn phí CC | `vn/`, `content/tcvn/`, `tools/cc/` |
| [ui-0-nen-tang-khung.md](ui-0-nen-tang-khung.md) | `UI0`, `UIA` | Nền UI Qt (theme, biểu tượng, i18n, thành phần, hộp thoại, palette, thông điệp) và khung ứng dụng (A01–A15, M01, M03, M04, macOS) | `app/core/`, `app/theme/`, `app/widgets/`, `app/shell/` |
| [ui-1-tep-thuoc-tinh-kieu-ve.md](ui-1-tep-thuoc-tinh-kieu-ve.md) | `UIB`, `UIC`, `UID`, `UIE` | B01–B14, C01–C11, D01–D14, E01–E10, tab Tham số (UI-PAR) | `app/dialogs/`, `app/palettes/` |
| [ui-2-chu-thich-block-in-truy-van.md](ui-2-chu-thich-block-in-truy-van.md) | `UIF`, `UIG`, `UIH`, `UII` | F01–F15, G01–G19, H01–H11, I01–I10 | `app/dialogs/`, `app/palettes/`, `app/contextual/` |
| [ui-3-vn-tudong-hethong.md](ui-3-vn-tudong-hethong.md) | `UIJ`, `UIK`, `UIL`, `UIM`, `UIN` | J01–J15, K01–K13, L01–L18, M02, N01–N03, tab Mở rộng (UI-MR, UI-MR2) | `app/vn/`, `app/custom/`, `app/system/`, `web/` |

### 4.2 Ranh giới sở hữu (tránh hai task làm cùng một việc)

| Việc | Thuộc | Không thuộc |
|---|---|---|
| Kiểu dữ liệu đối tượng bản vẽ | Sinh từ schema (DWG-001, DWG-002) vào `core/doc` (DOC-003) | Không viết tay struct trùng schema |
| Đọc/ghi thuộc tính chung (Properties, `entget`/`entmod`, so sánh) | Metadata thuộc tính DOC-017 | UI và LISP không tự ánh xạ trường |
| BVH cho chọn, OSNAP, culling | GEO-017 (cấu trúc), DOC-009 + INT-009 (chỉ mục trên bản vẽ), RND-010 (culling) | Không dựng BVH riêng trong từng module |
| Định dạng số và đơn vị | RGN-013 dùng chung cho kích thước, `rtos`/`angtos`, DYN, tọa độ thanh trạng thái, Properties | Không tự định dạng số ở nơi khác |
| Đọc số, khoảng cách, góc từ chuỗi | INT-105 dùng chung cho dòng lệnh, ô nhập, `distof`/`angtof` | RGN-013 chỉ định dạng, không phân tích |
| Biến đổi hình học | GEO-022 (đường cong), DOC-115 (entity trong transaction), DWG-169 (khối ACIS) | Lệnh không tự biến đổi entity |
| Chạy nhiều tệp | INT-019 (mở không giao diện); hàng đợi của VN-006 dùng lại ở CMD-264 (TCBATCH) | Không viết hàng đợi riêng cho từng công cụ |
| Zoom, pan, lăn chuột, lịch sử view | INT-016 (engine và lệnh ZOOM, PAN) | CMD chỉ có REGEN, REDRAW, VIEWRES, VIEW |
| UCS, OSNAP, POLAR, GRID, SNAP, ORTHO, DSETTINGS | INT-010, INT-011, INT-015 | |
| Grip của đường cong cơ bản và text | INT-013 | Grip của kích thước, mleader, hatch, bảng, viewport, xref, ảnh, mảng, dynamic block: task CMD/RGN của đối tượng đó đăng ký qua API của INT-013 |
| Engine và lệnh in, xuất PDF/ảnh, PUBLISH, in hàng loạt theo khung tên | PLT | UIH chỉ làm giao diện |
| Engine và lệnh Bác sĩ Font, công cụ TC… của VN | VN | UIJ chỉ làm giao diện |
| DCL | LSP-018, LSP-019 (parser, cây tile, runtime) | Vẽ tile bằng Qt: UIK |
| Trình soạn MTEXT | RGN-008, RGN-009 (dàn trang), task MTEXT của CMD (thao tác soạn trên cây định dạng, IME, caret) | UIF-… (tab ngữ cảnh, thanh công cụ, tương tác) |
| Clipboard (COPYCLIP, PASTECLIP…) | CMD (nhóm sửa) | Dán từ Excel thành bảng: VN-013 |
| Thumbnail | DWG-026 (đọc/ghi ảnh xem trước), RND-021 (render ngoài màn hình) | UI chỉ hiển thị |
| Tác vụ nền (tiến trình, hủy) | NT-013 | UI0-009 chỉ hiển thị |
| Cổng phiên bản | PLF-002 (khả năng), INT-020 (lệnh), UI0-011 (giao diện) | |
| Chỉ mục tìm lệnh, từ khóa tiếng Việt | INT-001 (metadata), INT-017 (chỉ mục) | Ô Tìm lệnh, Trợ giúp, Start dùng lại |
| Phím tắt | UI0-015 (một nguồn) | Hộp Phím tắt UI-K06 sửa nguồn này |

## 5. Task neo (hợp đồng giữa các module)

Task neo là task mà module khác được phép phụ thuộc. Mã, tên và phạm vi dưới đây là **cố định**. Tệp module ghi chi tiết đầy đủ (đầu vào, đầu ra, tiêu chí) cho từng neo và thêm các task khác, đánh số từ 101 trở đi (riêng `CMD` theo dải ở §4.1). Phụ thuộc chéo module chỉ trỏ tới task neo, `CMD:TÊN` hoặc `UI-xxx`.

### 5.1 NT · Nền tảng

| Mã | Task neo | Mốc |
|---|---|---|
| NT-001 | Kho mã monorepo, CMake presets, vcpkg ghim phiên bản; cờ biên dịch (C++20, LTO + PGO bản phát hành, cấm fast-math, `-ffp-contract=off`), preset ASan/UBSan/TSan | S1 |
| NT-002 | CI: cổng PR trên 3 hệ điều hành (build, test, sanitizer, clang-tidy), job đêm, runner tự host có GPU thật | S1 |
| NT-003 | Khung benchmark và cổng hiệu năng: Google Benchmark, baseline theo máy, chặn merge khi vượt ngưỡng hoặc chậm hơn 5%, chạy trên 4 máy tham chiếu | S1 |
| NT-004 | Tracy và macro đánh dấu (không tốn gì khi tắt); bộ đếm cấp phát cho benchmark | S1 |
| NT-005 | Bộ lập lịch work-stealing và đồ thị tác vụ: `parallel_for`/`reduce`/`scan`, task group, ưu tiên tương tác/nền, hủy hợp tác, QoS nhân P/E, đưa kết quả về luồng UI | S1 |
| NT-006 | Lớp SIMD tự viết, chọn đường chạy lúc khởi động (SSE4.2/AVX2/AVX-512/NEON); kernel biến đổi điểm, hộp bao, min/max | S2 |
| NT-007 | Bộ nhớ: arena theo document và theo lần regen, pool, small-vector, intern chuỗi (`StringId`, so sánh không phân biệt hoa thường kiểu AutoCAD) | S1 |
| NT-008 | Hồ sơ phần cứng: số nhân và loại nhân P/E, ISA SIMD, RAM, nguồn điện/pin, DPI, tần số màn hình (GPU và VRAM do RND-001 bổ sung) | S2 |
| NT-009 | Log, assert, chẩn đoán: kênh theo module, mức log, ring buffer an toàn khi crash | S1 |
| NT-010 | Kho cài đặt có kiểu (phạm vi máy / người dùng / profile), ghi nguyên tử, di trú phiên bản, xuất/nhập profile | V |
| NT-011 | I/O tệp: mmap, đọc bất đồng bộ song song, đường dẫn tìm kiếm (support path, đường dẫn tương đối của xref), theo dõi thay đổi tệp, ghi an toàn (tệp tạm + đổi tên) | S1 |
| NT-012 | Văn bản: UTF-8/UTF-16, code page Windows (1252, 1258, 932, 936, 949, 950…) cho DWG trước 2007, chuẩn hóa NFC, so sánh không phân biệt hoa thường có dấu | S1 |
| NT-013 | Đăng ký tác vụ nền: tiến trình, hủy, kết quả, thông báo; dùng chung cho thanh trạng thái, toast, hộp thoại | V |
| NT-014 | Nén và checksum: bọc zlib/libdeflate, CRC32 dùng lệnh phần cứng, Adler-32, giải nén/nén song song theo khối | S1 |
| NT-015 | Đọc/ghi ZIP tối thiểu (cho XLSX, CUIX, ETRANSMIT, gói thư viện TCVN) trên NT-014 | EA |

### 5.2 QA · Kiểm thử

| Mã | Task neo | Mốc |
|---|---|---|
| QA-001 | Kho bản vẽ: lưu trữ có thỏa thuận sử dụng, metadata (phiên bản, kích thước, nguồn, quyền), bộ 1.000 tệp spike, bộ 10.000 tệp, bộ BENCH-STD | S1 |
| QA-002 | Bộ chạy kho bản vẽ song song: mở từng tệp (qua `dwgdump`/`trion-console`), đo thời gian và bộ nhớ, phân loại lỗi, xuất JSON | S1 |
| QA-003 | So khớp round-trip từng trường (đọc → ghi → đọc) dựa trên metadata schema | VP |
| QA-004 | Harness đối chứng chéo: ODA File Converter (xuất DXF), LibreDWG CLI (hộp đen), DWG TrueView (mở không hỏi recover, in PDF) | S1 |
| QA-005 | So sánh ảnh in: SSIM, bản đồ chênh lệch, so vector với PDF do DWG TrueView in | V |
| QA-006 | Fuzzing liên tục: libFuzzer + ASan/UBSan, seed corpus, phân loại và khử trùng crash | S1 |
| QA-007 | Bộ tham chiếu: định dạng kịch bản `.scr`, đóng gói cho doanh nghiệp pilot, nhập DWG/PDF kết quả, so khớp hình học và tô-pô tự động | EA |
| QA-008 | Bộ chạy bộ LISP: kịch bản và kết quả kỳ vọng cho từng LISP, so DWG và output | S5 |
| QA-009 | Khung kiểm thử property-based cho hình học: bộ sinh ca ngẫu nhiên và suy biến, thu nhỏ ca lỗi, tái lập bằng seed | S3 |
| QA-010 | Bảng chỉ số hằng ngày: tỉ lệ mở kho, benchmark, bộ tham chiếu, crash | V |
| QA-011 | Harness kiểm thử UI tự động: điều khiển ứng dụng Qt, ảnh chụp so với baseline, đo độ trễ input → khung hình | V |
| QA-012 | Bộ cảnh benchmark đồ họa và tương tác (1 triệu / 10 triệu entity, hatch dày, text dày, block lồng, nhiều viewport, tọa độ VN-2000) | S2 |

### 5.3 PLF · Hệ thống

| Mã | Task neo | Mốc |
|---|---|---|
| PLF-001 | Mô hình giấy phép: tệp giấy phép ký Ed25519 (libsodium), các phiên bản (Viewer, Viewer Pro, Pro, Dùng thử, Giáo dục, Doanh nghiệp), ràng buộc máy | VP |
| PLF-002 | API cổng tính năng (phiên bản → khả năng) cho lệnh và UI | V |
| PLF-003 | Kích hoạt online và offline (client) cùng dịch vụ kích hoạt (server) | VP |
| PLF-004 | Dùng thử 30 ngày: nhắc mốc 7/3/1 ngày, hết hạn về Viewer, không khóa dữ liệu | EA |
| PLF-005 | Máy chủ giấy phép floating | 1.0 |
| PLF-006 | Cập nhật: feed ký số, kênh nội bộ/beta/ổn định, tải nền, kiểm chữ ký Ed25519 trước khi áp dụng | V |
| PLF-007 | Crash: Crashpad, máy chủ thu dump, symbol hóa, liên kết khôi phục bản vẽ | V |
| PLF-008 | Telemetry chỉ khi đồng ý: băm đường dẫn, không gửi nội dung bản vẽ, xem được dữ liệu sẽ gửi | V |
| PLF-009 | Trình cài đặt và tích hợp hệ điều hành: liên kết tệp, cài im lặng, thumbnail trong Explorer (P1), gỡ cài đặt sạch | V |
| PLF-010 | Gửi bản vẽ lỗi (TCREPORT): đóng gói, ẩn danh tùy chọn, tải lên có tiến trình | V |
| PLF-011 | Backend cổng quản trị doanh nghiệp: chỗ ngồi, triển khai, floating | 1.0 |
| PLF-012 | Backend thanh toán và hóa đơn điện tử (web) | VP |

### 5.4 GEO · Hình học

| Mã | Task neo | Mốc |
|---|---|---|
| GEO-001 | Kiểu cơ bản: điểm/vector 2D-3D, ma trận affine 2D/3D, hộp bao; mảng điểm SoA cho SIMD | S1 |
| GEO-002 | Chính sách dung sai `Tolerance` (điểm, vector, góc; tỉ lệ theo kích thước bản vẽ) và API so sánh | S3 |
| GEO-003 | Predicate bền vững `orient2d`, `incircle` (Shewchuk) với dự phòng số học chính xác | S3 |
| GEO-004 | Thư viện hàm lượng giác và hàm sơ cấp tất định (cùng kết quả trên mọi nền tảng) cho chỗ quyết định tô-pô | S3 |
| GEO-005 | Đường cong cơ bản: line, ray, xline, cung, đường tròn, ellipse, cung ellipse (tham số hóa, đánh giá, đạo hàm, độ dài, điểm gần nhất, chia, đảo chiều, hộp bao) | S3 |
| GEO-006 | Polyline có bulge (LWPOLYLINE, POLYLINE 2D): đoạn, độ dài, diện tích có dấu, điểm gần nhất, độ rộng đoạn | S3 |
| GEO-007 | NURBS: đánh giá, đạo hàm, chèn nút, chia, độ dài, điểm gần nhất, hộp bao; chuyển đổi với dạng lưu trong DWG | V |
| GEO-008 | Giao điểm giải tích line/cung/tròn/ellipse, phân loại tiếp xúc và trùng đoạn | S3 |
| GEO-009 | Giao điểm có spline: chia nhỏ + bao khoảng + Newton, cô lập nghiệm có bảo đảm | V |
| GEO-010 | Bộ điều phối giao điểm cho mọi cặp đường cong và giao hàng loạt (broad-phase + song song) | S3 |
| GEO-011 | Offset một đường cong (cung ra cung; ellipse/spline xấp xỉ có kiểm soát sai số) | S3 |
| GEO-012 | Offset polyline có cung: offset thô, nối, cắt tự giao, lọc theo khoảng cách, kín/hở | S3 |
| GEO-013 | Sắp xếp phẳng (DCEL) với cạnh là đường cong thật | S3 |
| GEO-014 | Trích mặt và vùng kín: biên hatch theo pick point, island lồng nhau, BOUNDARY | S3 |
| GEO-015 | Boolean region (hợp, trừ, giao) trên vùng có cạnh cong | Sau |
| GEO-016 | Tessellation có sai số dây cung theo pixel cho cung, ellipse, spline (SIMD) | V |
| GEO-017 | BVH 2D: dựng song song (SAH binning), nút 64 byte, duyệt không rẽ nhánh, cập nhật gia tăng | S2 |
| GEO-018 | Hình học bo góc và vát góc giữa các cặp đường cong (line, cung, tròn, đoạn polyline) | EA |
| GEO-019 | Nguyên hàm cắt và kéo dài: cắt đường cong theo tham số, kéo dài tới biên (kể cả biên ngầm và phần kéo dài) | EA |
| GEO-020 | Dựng hình: đường tròn 2P/3P/TTR/TTT, 11 cách dựng cung, tiếp tuyến, vuông góc, điểm chia đều/theo khoảng | EA |
| GEO-021 | Diện tích, chu vi, trọng tâm, tính chất khối lượng của vùng (có cung, spline) | V |
| GEO-022 | Biến đổi đường cong (kể cả tỉ lệ không đều: tròn → ellipse, cung → cung ellipse; đối xứng) | V |
| GEO-023 | Spline qua điểm (Fit), chuyển polyline ↔ spline, đường nối cong (BLEND) | EA |
| GEO-024 | Bộ giải ràng buộc hình học và kích thước 2D (tham khảo thuật toán PlaneGCS) | Sau |
| GEO-025 | Chữ ký hình học bất biến khi xoay và đổi tỉ lệ (tìm đối tượng tương tự, TCBLOCKMATCH) | 1.0 |

### 5.5 DOC · Mô hình bản vẽ

| Mã | Task neo | Mốc |
|---|---|---|
| DOC-001 | Định danh: handle 64-bit, handle seed, `ObjectId` (chỉ số + thế hệ), bảng handle → chỉ số | S1 |
| DOC-002 | Lưu trữ hướng dữ liệu: mảng liên tục theo loại (SoA cho trường nóng), header chung của entity (layer, màu, linetype, lineweight, transparency, hiển thị, owner, xdata, extension dictionary) | S1 |
| DOC-003 | Kiểu đối tượng sinh từ schema (khoảng 120 loại) vào `core/doc/generated` | S1 |
| DOC-004 | Bảng ký hiệu và dictionary: LAYER, LTYPE, STYLE, DIMSTYLE, UCS, VIEW, VPORT, APPID, BLOCK_RECORD; named object dictionary; cây sở hữu; tra tên | S1 |
| DOC-005 | Xdata, xrecord, extension dictionary, reactor lưu trong bản vẽ | V |
| DOC-006 | Giữ nguyên dữ liệu chưa hiểu: khối bit thô và handle tham chiếu của đối tượng/trường chưa có trong schema | VP |
| DOC-007 | API thay đổi: mọi sửa đổi qua transaction, nhật ký theo thuộc tính, sự kiện thêm/sửa/xóa gom theo lô | V |
| DOC-008 | Undo/redo: mark theo lệnh, nhóm, các tùy chọn của UNDO (Mark/Back/BEgin/End/Auto/Control), giới hạn bộ nhớ | VP |
| DOC-009 | Snapshot bất biến copy-on-write cho các luồng đọc song song; chi phí tỉ lệ với phần đã đổi | V |
| DOC-010 | Biến header của bản vẽ: lưu và truy cập có kiểu | S1 |
| DOC-011 | Model space, layout, paper space, entity VIEWPORT, không gian hiện hành | V |
| DOC-012 | Block: định nghĩa, tham chiếu, duyệt lồng nhau, ATTDEF/ATTRIB, MINSERT, block vô danh, block đại diện `*U` của dynamic block | V |
| DOC-013 | Mô hình xref: block record ngoài, trạng thái phân giải, lồng nhau, attach/overlay; nạp tài liệu xref chỉ đọc vào phiên | V |
| DOC-014 | Sao chép sâu có ánh xạ ID (trong một bản vẽ và giữa hai bản vẽ), xóa/khôi phục, phân tích tham chiếu cho PURGE | EA |
| DOC-015 | Kiểm tra toàn vẹn và sửa lỗi (owner, handle, tham chiếu treo) cho AUDIT/RECOVER | V |
| DOC-016 | Nạp lười và nạp dần: trạng thái đối tượng chưa giải mã, xem/pan/zoom trong khi nạp | V |
| DOC-017 | Metadata thuộc tính: đọc/ghi chung theo mã thuộc tính (sinh từ schema) cho Properties, `entget`/`entmod`, so sánh | V |
| DOC-018 | Phiên nhiều bản vẽ: danh sách bản vẽ mở, bản vẽ hiện hành, trạng thái riêng từng bản vẽ | V |
| DOC-019 | Autosave từ snapshot ở luồng nền (`.sv$`), `.bak`, khóa `.dwl`/`.dwl2` | EA |
| DOC-020 | Thứ tự vẽ (SORTENTS) và nhóm (GROUP) | V |
| DOC-021 | Mô hình chú thích theo tỉ lệ (object context data): đọc và hiển thị ở V, sửa ở 1.0 | V |
| DOC-022 | Mô hình field (đối tượng field, giá trị cache, móc đánh giá) | 1.0 |
| DOC-023 | Mô hình dữ liệu dynamic block (đối tượng EvalGraph, parameter, action, visibility, lookup) | 1.0 |

### 5.6 DWG · Codec

| Mã | Task neo | Mốc |
|---|---|---|
| DWG-001 | DSL schema: trường, kiểu bit, điều kiện theo phiên bản, mã nhóm DXF, tên thuộc tính; parser schema | S1 |
| DWG-002 | Codegen từ schema: bộ đọc DWG, bộ ghi DWG, đọc/ghi DXF, metadata thuộc tính | S1 |
| DWG-003 | Bit-stream đọc/ghi (B, BB, 3B, BS, BL, BLL, BD, RC, RS, RL, RD, MC, MS, handle reference, TV/TU, CMC, BE, DD, BT, OT), kiểm biên | S1 |
| DWG-004 | Nhận diện phiên bản (AC1012–AC1032), bố cục tệp R13–R2000 (section locator, CRC) | S1 |
| DWG-005 | R2004+: giải mã header tệp, section page map, section info, giải nén LZ77 biến thể (song song theo trang), checksum trang | S1 |
| DWG-006 | R2007: Reed–Solomon, tổ chức trang/section, chuỗi UTF-16, tách luồng data/strings/handles | V |
| DWG-007 | Đặc thù R2010/R2013/R2018 (object map, luồng handle, thay đổi trường theo phiên bản) | S1 |
| DWG-008 | Section biến header: đọc và ghi | S1 |
| DWG-009 | Section class (class động) | S1 |
| DWG-010 | Object map và giải mã đối tượng song song (work-stealing) | S1 |
| DWG-011 | Schema entity lõi (khoảng 20 loại cho S1: LINE, CIRCLE, ARC, LWPOLYLINE, POLYLINE/VERTEX, TEXT, MTEXT, INSERT, ATTRIB, ATTDEF, các DIMENSION, HATCH, SPLINE, ELLIPSE, POINT, SOLID, LEADER, VIEWPORT) | S1 |
| DWG-012 | Schema đối tượng bảng và control object (LAYER, LTYPE, STYLE, DIMSTYLE, BLOCK_HEADER, VPORT, VIEW, UCS, APPID) | S1 |
| DWG-013 | Schema đối tượng thường gặp: DICTIONARY, XRECORD, LAYOUT, PLOTSETTINGS, GROUP, MLINESTYLE, IMAGEDEF, SORTENTSTABLE, SCALE… | V |
| DWG-014 | Schema đối tượng nâng cao: MLEADER/MLEADERSTYLE, TABLE/TABLESTYLE, FIELD, visual style, material, DBCOLOR… | V |
| DWG-015 | Đối tượng dynamic block (EvalGraph, BlockRepresentation, parameter, action): đọc và giữ nguyên | V |
| DWG-016 | Proxy entity/object và proxy graphics: đọc, giữ nguyên | V |
| DWG-017 | 3DSOLID/REGION/BODY: giữ nguyên khối dữ liệu ACIS (SAT/SAB) khi mở và lưu | VP |
| DWG-018 | Giữ nguyên đối tượng và trường chưa hiểu (khối bit + handle), ghi lại y hệt; không đánh lại handle | VP |
| DWG-019 | Chế độ giải mã lười cho Viewer và tệp lớn | V |
| DWG-020 | Chế độ Recover: chịu CRC sai, offset lệch, đối tượng thiếu; nhật ký sửa | V |
| DWG-021 | Bộ ghi AC1018 (R2004): dựng section, nén LZ77 nhanh, page map, CRC, object map | VP |
| DWG-022 | Bộ ghi AC1032 (R2018) | VP |
| DWG-023 | Bộ ghi AC1015, AC1021 (gồm mã hóa Reed–Solomon), AC1024, AC1027 | 1.0 |
| DWG-024 | DXF ASCII đọc/ghi mọi phiên bản (sinh từ schema) | V |
| DWG-025 | DXF nhị phân đọc/ghi | EA |
| DWG-026 | Ảnh xem trước (thumbnail) trong DWG: đọc và ghi | V |
| DWG-027 | Section SummaryInfo, AppInfo, FileDepList, RevHistory, Preview… (cho DWGPROPS, thiếu tham chiếu) | V |
| DWG-028 | Mã hóa chuỗi: DWGCODEPAGE cho bản trước 2007, `\U+xxxx`, `\M+` | S1 |
| DWG-029 | Đọc DWG R12 trở về trước và DXF R12 | Sau |
| DWG-030 | Công cụ `dwgdump`: in đối tượng ra JSON cho QA và gỡ lỗi | S1 |
| DWG-031 | Ngân sách mở tệp: DWG 20 MB ≤ 1,5 s, 100 MB ≤ 5 s (tầm trung); bộ nhớ ≤ 1,2 GB cho 100 MB; scaling ≥ 70% đến 16 luồng | V |

### 5.7 RGN · Regen

| Mã | Task neo | Mốc |
|---|---|---|
| RGN-001 | Mô hình primitive hiển thị và khóa vật liệu; API regen là hàm thuần (entity, tham số khung nhìn) → primitive | S2 |
| RGN-002 | Phân giải thuộc tính hiển thị: ACI và true color, ByLayer/ByBlock qua block lồng, linetype, lineweight, transparency; ghi đè layer theo viewport (P1) | V |
| RGN-003 | Bộ điều phối regen song song trên snapshot; regen gia tăng theo sự kiện thay đổi; cache theo entity | V |
| RGN-004 | Font SHX: parser shape, bigfont, unifont; cache glyph dạng nét | S6 |
| RGN-005 | TrueType qua FreeType + HarfBuzz: đường viền, shaping, cache glyph; cấp glyph cho atlas SDF (RND-007) | S6 |
| RGN-006 | FontResolver: tìm font, thay font thiếu, ánh xạ SHX ↔ TTF, móc giải mã khi vẽ của Bác sĩ Font (VN-004) | V |
| RGN-007 | TEXT, ATTRIB, ATTDEF một dòng: 15 kiểu căn lề, width factor, oblique, ngược/lộn ngược, mã `%%`, `\U+` | S6 |
| RGN-008 | Parser MTEXT (mã `\f \H \W \Q \T \S \P \L \O \K \A \C`…) → cây định dạng; ghi ngược cây thành chuỗi MTEXT | V |
| RGN-009 | Dàn trang MTEXT: ngắt dòng, đoạn, tab, thụt lề, bullet, giãn dòng, xếp chồng, nền che; ≥ 99% đoạn khớp bộ tham chiếu | V |
| RGN-010 | MTEXT nâng cao: cột, cột động, chiều cao tự động (P1) | 1.0 |
| RGN-011 | Linetype đơn giản và phức (shape, text) theo chiều dài cung thật; LTSCALE, PSLTSCALE, CELTSCALE, MSLTSCALE, PLINEGEN | V |
| RGN-012 | Hatch: parser `.pat`, sinh nét mẫu cắt theo biên cong (GEO-014), island, tô đặc, gradient, LOD | V |
| RGN-013 | Thư viện định dạng số và đơn vị (LUNITS, AUNITS, LUPREC, DIMZIN, DIMRND, kiến trúc, kỹ thuật, phân số, góc khảo sát); khớp từng ký tự | V |
| RGN-014 | Kích thước thẳng, nghiêng, xoay theo đủ DIMVAR (mũi tên, vị trí chữ, quy tắc fit, đơn vị chính và phụ, dung sai); xem dùng block lưu sẵn, sửa thì sinh lại | V |
| RGN-015 | Kích thước góc, bán kính, đường kính, cung, gãy khúc, tọa độ điểm | V |
| RGN-016 | Leader và MLeader (nội dung MTEXT/block, landing, dogleg, mũi tên) | V |
| RGN-017 | Bảng: dùng block đại diện khi xem (V); sinh lại từ dữ liệu ô khi sửa (EA) | V |
| RGN-018 | Block: insert lồng nhau, attribute, ATTDISP, MINSERT; dữ liệu instancing (hình học định nghĩa + ma trận) | V |
| RGN-019 | Dynamic block: hiển thị qua `*U` (V); engine EvalGraph cho parameter và action (1.0) | V |
| RGN-020 | Proxy graphics: parser và regen (P1) | V |
| RGN-021 | Ảnh raster (PNG, JPG, TIFF, BMP, GIF qua QImage): nạp bất đồng bộ, clip, sáng/tương phản, trong suốt | V |
| RGN-022 | PDF underlay (qua PLT-011): ô render theo mức zoom, layer PDF, clip | 1.0 |
| RGN-023 | Entity còn lại: POINT (PDMODE/PDSIZE), SOLID, TRACE, WIPEOUT, XLINE/RAY, 3DFACE, POLYLINE 2D/3D, MLINE, REGION/3DSOLID (hiện khung bao đến P3) | V |
| RGN-024 | Viewport và xref: tham số view theo viewport, VP Freeze, clip; xref attach/overlay, làm mờ, XCLIP | V |
| RGN-025 | Field: đánh giá và cập nhật (ngày, tên tệp, thuộc tính đối tượng, công thức) | 1.0 |
| RGN-026 | Chú thích theo tỉ lệ (annotative): chọn biểu diễn theo tỉ lệ hiện hành | V |
| RGN-027 | Thứ tự vẽ (SORTENTS, draw order) trong luồng primitive | V |
| RGN-028 | Ngân sách regen: 1 triệu entity ≤ 0,5 s (8 nhân), ≤ 2 s (4 nhân máy thấp); hiệu suất song song ≥ 70% | V |

### 5.8 RND · Renderer

| Mã | Task neo | Mốc |
|---|---|---|
| RND-001 | Khởi tạo QRhi trong vùng vẽ; chọn backend (D3D11/12, Vulkan, Metal, OpenGL); mất thiết bị; ép GPU rời trên laptop; dò MSAA, compute, VRAM | S2 |
| RND-002 | Lập lịch khung hình: chỉ vẽ khi có thay đổi, vsync, 120/144 Hz, giám sát ngân sách 16 ms, hạ LOD tạm và tinh chỉnh dần | S2 |
| RND-003 | Bộ đệm GPU theo vật liệu: cấp phát con, cập nhật cục bộ theo vùng, upload bất đồng bộ qua staging buffer | S2 |
| RND-004 | Đường thẳng có lineweight: mở rộng thành quad trong vertex shader, mối nối, đầu mút | S2 |
| RND-005 | Cung, đường tròn, ellipse giải tích trong fragment shader (mịn ở mọi mức zoom) | S2 |
| RND-006 | Linetype trong fragment shader theo tham số chiều dài cung | S2 |
| RND-007 | Text: atlas SDF cho chữ nhỏ, tam giác hóa cho chữ lớn, SHX dạng nét | S6 |
| RND-008 | Vùng tô (hatch đặc, solid, wipeout, gradient), transparency, thứ tự vẽ | V |
| RND-009 | Instancing block (cả block lồng) | V |
| RND-010 | Culling theo BVH (GEO-017) và LOD (dưới 1 pixel → điểm, chữ nhỏ → khung, hatch dày → mảng tô, spline giảm điểm) | S2 |
| RND-011 | Độ chính xác RTC: trừ gốc khung nhìn bằng `double` trước khi chuyển `float` | S2 |
| RND-012 | Lớp overlay (jig, grip, marker bắt điểm, khung chọn, đường theo dõi) vẽ lại độc lập với cảnh chính | V |
| RND-013 | Tô sáng (rê chuột, đã chọn, kết quả, làm mờ) qua bộ đệm cờ theo entity, không dựng lại batch | V |
| RND-014 | Paper space: giấy, bóng, vùng in; viewport với ma trận riêng, scissor/stencil cho viewport đa giác; viền viewport kích hoạt | V |
| RND-015 | Ảnh raster và PDF underlay: texture, chia ô, mipmap | V |
| RND-016 | GPU compute (culling/LOD, nét hatch/linetype dày, xử lý ảnh, so sánh ảnh) kèm đường dự phòng trên CPU | V |
| RND-017 | Quản lý VRAM: ngân sách theo dung lượng, chia ô bản vẽ khổng lồ, loại bỏ LRU, bộ nhớ hợp nhất trên Apple Silicon | V |
| RND-018 | Tự thích ứng theo hồ sơ phần cứng: backend, MSAA, mật độ tessellation, ngưỡng LOD, số luồng nền; chế độ pin | V |
| RND-019 | Khử răng cưa: MSAA khi có, nếu không thì làm mượt cạnh trong shader | S2 |
| RND-020 | Nền vùng vẽ, lưới (GRID), biểu tượng UCS, con trỏ chữ thập độ trễ thấp | V |
| RND-021 | Render ngoài màn hình: thumbnail, xem trước in, xuất ảnh, ảnh cho so sánh | V |
| RND-022 | Cache shader và pipeline trên đĩa | V |
| RND-023 | Xem 3D cơ bản: 3DORBIT, kiểu hiển thị wireframe/hidden/shaded cho mesh và 3DFACE, ViewCube | 1.0 |
| RND-024 | Engine TRIONBENCH: bài đo chuẩn, gợi ý cấu hình, kết quả ẩn danh khi đồng ý | V |
| RND-025 | Benchmark đồ họa trong CI trên GPU thật, kể cả Intel HD cũ (dùng QA-012) | S2 |

### 5.9 INT · Tương tác và command framework

| Mã | Task neo | Mốc |
|---|---|---|
| INT-001 | Registry lệnh: tên toàn cục `_`, tên địa phương, tiền tố `-` `.` `'`, UNDEFINE/REDEFINE; metadata (biểu tượng, trợ giúp, từ khóa tiếng Việt, cờ phiên bản, vị trí trên ribbon) | V |
| INT-002 | Input pipeline duy nhất (bàn phím, script, LISP, macro, menu, AI) và mô hình lệnh dạng coroutine trên luồng UI; Enter/Space/Esc; lệnh trong suốt lồng nhau | V |
| INT-003 | Prompt và từ khóa `[A/B]`, từ khóa toàn cục `_`, giá trị mặc định `<>`, dữ liệu chip tùy chọn cho UI (UI-E10) | V |
| INT-004 | Parser tọa độ và giá trị: tuyệt đối, `@`, `<`, nhập khoảng cách trực tiếp, ép góc, `#`, bộ lọc `.X/.Y/.XY…`, From, M2P, đơn vị (feet-inch, góc khảo sát) | V |
| INT-005 | Registry sysvar có kiểu và phạm vi (profile, header, phiên), giới hạn giá trị, thông báo khi đổi, khoảng 250 biến; SETVAR | V |
| INT-006 | Alias `.pgp` (đọc `acad.pgp` có sẵn của người dùng), REINIT | EA |
| INT-007 | Script `.scr`: SCRIPT, RSCRIPT, DELAY, RESUME | EA |
| INT-008 | Engine chọn: pick, Window, Crossing, Fence, WPolygon, CPolygon, All, Last, Previous, Add/Remove, lasso (P1), PICKFIRST/PICKADD, bộ lọc kiểu `ssget`, chạy song song trên hàng triệu đối tượng | V |
| INT-009 | Hit test chính xác theo loại đường cong trên BVH, ≤ 1 ms | V |
| INT-010 | OSNAP đủ chế độ, xếp hạng ưu tiên như AutoCAD, Tab luân phiên; bắt vào block và xref (EA), underlay (1.0) | V |
| INT-011 | Tracking: POLAR, OTRACK (điểm đã lấy, kéo dài, giao biểu kiến), ORTHO, SNAP và GRID (chữ nhật; isometric P1) | EA |
| INT-012 | Khung jig và xem trước kết quả trong lệnh: vẽ trên overlay RND-012, không ghi DB, ≤ 8 ms/khung | V |
| INT-013 | Khung grip: grip theo loại entity (đường cong cơ bản, text), nguội/rê/nóng, grip đa năng, Space luân phiên chế độ, Sao chép/Điểm gốc/Tham chiếu, GRIPOBJLIMIT; API đăng ký grip cho module khác | EA |
| INT-014 | Nhập động (DYN): nhập con trỏ, nhập kích thước, prompt động, khóa ô, kiểm tra hợp lệ | EA |
| INT-015 | UCS (Origin, 3P, Object, View, World), biến đổi tọa độ nhập/xuất, UCSICON, PLAN | EA |
| INT-016 | Điều hướng: ZOOM (realtime, window, extents, all, previous, scale, object), PAN, lăn chuột quanh con trỏ, lịch sử view, ZOOMFACTOR; kích hoạt viewport trong layout | V |
| INT-017 | Mô hình dòng lệnh: lịch sử, chỉ mục tự hoàn thành (lệnh, alias, sysvar, lệnh LISP, từ khóa tiếng Việt; xếp theo tần suất của người dùng), gợi ý khi gõ sai, AutoCorrect (P1) | V |
| INT-018 | Mô hình menu chuột phải theo ngữ cảnh, nhấp đúp theo loại đối tượng (DBLCLKEDIT), hành động kéo-thả | V |
| INT-019 | `trion-console`: chạy không giao diện (mở tệp, chạy script/LISP, mã thoát) cho QA và tác vụ hàng loạt | V |
| INT-020 | Cổng phiên bản cho lệnh: Viewer chỉ có lệnh xem và đo; thông báo trên dòng lệnh theo 07.1 §2 | V |
| INT-021 | Cô lập/ẩn đối tượng, chọn luân phiên (P1) | 1.0 |
| INT-022 | DIESEL và MODEMACRO (P1); biểu thức `'CAL` (P1); engine QUICKCALC (P2) | 1.0 |

### 5.10 LSP, CUS, SDK · Tự động hóa, tùy biến, SDK

| Mã | Task neo | Mốc |
|---|---|---|
| LSP-001 | Reader và printer AutoLISP (cú pháp, chú thích `;` và `;\| \|;`, chuỗi, số, in số giống AutoLISP) | S5 |
| LSP-002 | Kiểu dữ liệu (INT 32-bit tràn số như AutoLISP, REAL, STR, SYM, LIST, FILE, SUBR, USUBR, ENAME, PICKSET, VLA-OBJECT, VARIANT, SAFEARRAY) và GC mark-and-sweep phân thế hệ | S5 |
| LSP-003 | Evaluator máy ngăn xếp tường minh, dynamic scoping, tạm dừng/tiếp tục khi chờ input | S5 |
| LSP-004 | Hàm lõi: danh sách, số học, chuỗi, điều khiển, kiểu | S5 |
| LSP-005 | Xử lý lỗi: `*error*`, `vl-catch-all-*`, `exit`/`quit`, thông báo lỗi như AutoLISP | EA |
| LSP-006 | Cầu nối lệnh: `command`, `command-s`, `vl-cmdf`, `pause` (qua INT-002) | S5 |
| LSP-007 | Hàm `get*` và chọn: `getpoint`, `getdist`, `getangle`, `getreal`, `getint`, `getstring`, `getkword`, `initget`, `getcorner`, `entsel`, `nentsel`, `nentselp`, `grread` | EA |
| LSP-008 | Hàm entity: `entget`, `entmod`, `entmake`, `entmakex`, `entdel`, `entnext`, `entlast`, `entupd`, `handent` (mã nhóm DXF từ DOC-017) | EA |
| LSP-009 | `ssget` đủ chế độ và bộ lọc (`-4`, `wcmatch`), các hàm `ss*` | EA |
| LSP-010 | Bảng, dictionary, xdata, regapp | EA |
| LSP-011 | Hình học và định dạng số: `rtos`, `distof`, `angtos`, `angtof` (RGN-013), `polar`, `distance`, `angle`, `inters`, `trans`, `osnap`, `textbox` | EA |
| LSP-012 | Tệp và môi trường: `open`, `read-line`, `write-line`, `findfile`, `getfiled`, `getenv`/`setenv`, `vl-registry-*` (theo giới hạn bảo mật) | EA |
| LSP-013 | Nạp: `load`, `autoload`, `acad.lsp`/`acaddoc.lsp`/`.mnl`/`trion.lsp`/`triondoc.lsp`, Startup Suite, SECURELOAD, TRUSTEDPATHS | EA |
| LSP-014 | Hàm `vl-*` | 1.0 |
| LSP-015 | `vlax-*` trên mô hình đối tượng (variant, safearray, thuộc tính, collection) | 1.0 |
| LSP-016 | Reactor `vlr-*` | 1.0 |
| LSP-017 | `vla-*` sinh từ mô tả mô hình đối tượng (dùng chung schema DWG-001) | 1.0 |
| LSP-018 | DCL: parser, `base.dcl`, cây tile, layout xấp xỉ kích thước của AutoCAD | EA |
| LSP-019 | Runtime DCL: `load_dialog`, `new_dialog`, `start_dialog`, `done_dialog`, `action_tile`, `set_tile`, `get_tile`, `mode_tile`, list, image/slide | EA |
| LSP-020 | Hiệu năng LISP: bộ benchmark; biên dịch bytecode nếu số đo cho thấy cần | 1.0 |
| CUS-001 | Thực thi macro menu (`^C^C`, `;`, `\`, `$M=` DIESEL) qua INT-002 | EA |
| CUS-002 | Nhập CUIX một phần: toolbar, menu, macro, alias | 1.0 |
| CUS-003 | Engine chuyển cài đặt từ AutoCAD (TCMIGRATE): profile, PGP, đường dẫn hỗ trợ, LISP khởi động, CUIX | EA |
| CUS-004 | Mô hình workspace: lưu và chuyển bố cục ribbon, palette, toolbar | 1.0 |
| CUS-005 | Backend trình sửa CUI | Sau |
| SDK-001 | C API ổn định (database, transaction, entity, reactor, command, jig), có phiên bản ABI | 1.0 |
| SDK-002 | C++ wrapper header-only trên C API | 1.0 |
| SDK-003 | Nạp plugin: manifest, chữ ký số, cô lập lỗi (1.0 beta); quản lý plugin (P2) | 1.0 |

### 5.11 PLT · In và xuất

| Mã | Task neo | Mốc |
|---|---|---|
| PLT-001 | Mô hình thiết lập trang (thiết bị, khổ giấy, vùng in, tỉ lệ, offset, xoay, plot style, tùy chọn lineweight); đọc/ghi PLOTSETTINGS và LAYOUT; nhập page setup từ bản vẽ khác | V |
| PLT-002 | CTB/STB: đọc/ghi, áp dụng (màu, bút, screening, linetype, đầu mút, mối nối); CTB mẫu | V |
| PLT-003 | Pipeline in vector: regen theo tọa độ giấy, áp plot style và lineweight, clip viewport, bỏ đối tượng không in | V |
| PLT-004 | Bộ ghi PDF vector tự viết: path, fill, clip, transparency, nhúng tập con font TTF, SHX dạng hình kèm text tìm được (P1), layer OCG, hyperlink, ảnh, nén song song, song song theo tờ | V |
| PLT-005 | In qua QPrinter (máy in hệ thống), khổ giấy, raster hóa khi cần | V |
| PLT-006 | Engine xem trước bản in dùng chung pipeline in | V |
| PLT-007 | Xuất ảnh PNGOUT, JPGOUT, BMPOUT, TIFOUT (theo vùng, độ phân giải) | V |
| PLT-008 | PUBLISH: nhiều layout, nhiều bản vẽ, chạy nền, tiến trình, hủy | VP |
| PLT-009 | In hàng loạt theo khung tên trong Model (TCBATCHPLOT): nhận diện khung tên, sắp thứ tự, đặt tên tờ | VP |
| PLT-010 | PC3/PMP: đọc (P1); trình sửa (P2) | 1.0 |
| PLT-011 | Lớp bọc PDFium: mở PDF, render ô, trích hình học và text | 1.0 |
| PLT-012 | PDFIMPORT và PDFSHXTEXT | 1.0 |
| PLT-013 | CONVERTCTB, CONVERTPSTYLES | 1.0 |

### 5.12 VN · Gói Việt Nam

| Mã | Task neo | Mốc |
|---|---|---|
| VN-001 | Bảng mã TCVN3 (ABC), VNI-Windows, VISCII, Unicode dựng sẵn và tổ hợp; chuyển hai chiều | S4 |
| VN-002 | Nhận diện bảng mã theo đối tượng và theo đoạn MTEXT: tên font, đặc trưng byte, chấm điểm âm tiết n-gram, ngưỡng tin cậy | S4 |
| VN-003 | Bộ mẫu gán nhãn (1.000 cho S4, 5.000 cho v1.0) và công cụ đo độ chính xác | S4 |
| VN-004 | Chế độ hiển thị: quét khi mở (`vn::scanEncodings`), thay font và giải mã khi vẽ, không đổi dữ liệu | V |
| VN-005 | Chế độ chuyển đổi: ghi Unicode NFC, đổi text style; xử lý `%%`, mã MTEXT, attribute, ghi đè text của dim, block lồng, bảng, field; xem trước; một mark undo; báo cáo | VP |
| VN-006 | Chuyển font hàng loạt nhiều tệp (FIXFONTBATCH): chạy nền, lưu bản sao | VP |
| VN-007 | Bộ font SHX tự thiết kế, tương thích tên (vntime, vnarial, romansvn, txt, simplex, romans, isocp, gdt…): công cụ biên dịch SHX, kiểm phủ glyph tiếng Việt, kiểm metric | V |
| VN-008 | Nội dung TCVN: mẫu `.dwt` A0–A4, bộ layer theo bộ môn, CTB, kiểu chữ và kích thước, `.pat`/`.lin` tự viết giữ tên chuẩn, khung tên, thư viện block | EA |
| VN-009 | Định dạng gói và chỉ mục Thư viện TCVN (tìm kiếm, ảnh xem trước, chèn theo tỉ lệ) | EA |
| VN-010 | Engine khung tên TCTITLE và khung in trong Model TCFRAME | EA |
| VN-011 | TCNUMBER, TCINCCOPY | 1.0 |
| VN-012 | TCTEXT (nối, tách, gộp text; đổi hoa/thường có dấu), TCSYMBOL | 1.0 |
| VN-013 | Cầu nối Excel: TCEXCELPASTE (HTML/TSV từ clipboard → bảng), TCEXCELOUT (ghi XLSX tự viết), CSV | EA |
| VN-014 | TCATTOUT (VP), TCATTIN (1.0) | VP |
| VN-015 | TCLENGTH, TCAREA (VP); TCROOMAREA (1.0) | VP |
| VN-016 | TCREBAR, TCCHECK (P2) | Sau |
| VN-017 | Công cụ miễn phí CC: FIXFONT và TCLIB bản LISP + DCL chạy trên AutoCAD, ZWCAD, BricsCAD; gói cài đặt | CC |
| VN-018 | Quản lý bộ bản vẽ kiểu VN (TCSHEETS): danh mục tờ, tự điền ký hiệu "KT 1/18", in cả bộ | 1.0 |

### 5.13 UI0 · Nền giao diện

| Mã | Task neo | Mốc |
|---|---|---|
| UI0-001 | Ứng dụng Qt `trion`: nối lõi, luồng UI sở hữu document, nhận kết quả nền từ NT-005, khởi động ấm ≤ 1,5 s | V |
| UI0-002 | Theme sinh từ `tokens.css` (QPalette + QSS + hằng C++): Tối/Sáng, token vùng vẽ và màu ngữ nghĩa (07.1 §13.2), kiểm tương phản tự động | V |
| UI0-003 | Font giao diện IBM Plex Sans/Mono (OFL) đủ tiếng Việt, thang cỡ chữ theo token | V |
| UI0-004 | Bộ biểu tượng SVG 16/32 px (chuyển từ `design/prototype/js/icons.js`), biến thể chủ đề và vô hiệu, cache raster theo DPI | V |
| UI0-005 | Bản địa hóa vi/en đổi không cần khởi động lại; định dạng số và ngày; bảng thuật ngữ 07.1 §13.4; kiểm chuỗi dài +40% | V |
| UI0-006 | Thành phần cơ bản: nút, split button, toggle, dropdown, combo, ô tìm kiếm, tab, tree/table ảo hóa, slider, menu, tooltip 2 mức | V |
| UI0-007 | Ô nhập số chuẩn UI-C09: đơn vị, biểu thức, nút lấy từ bản vẽ, kiểm tra hợp lệ | V |
| UI0-008 | Khung hộp thoại: modal/modeless, sheet trên macOS, focus, Enter/Esc, xác nhận phá hủy nêu số lượng, "Không hỏi lại" (khôi phục được), hộp thoại lỗi có chi tiết, sao chép, gửi báo lỗi | V |
| UI0-009 | Thông điệp: banner trên vùng vẽ, toast, chú thích cạnh con trỏ, tiến trình (gắn NT-013), trạng thái rỗng | V |
| UI0-010 | Khung palette và docking: dock trái/phải/dưới, nổi, tự ẩn thành dải, nhóm tab, nhiều màn hình, đổi DPI, lưu bố cục | V |
| UI0-011 | Cổng phiên bản trong UI: huy hiệu khóa, mở trang nâng cấp UI-L04, giao diện giản lược của Viewer (dựa PLF-002) | V |
| UI0-012 | High-DPI 100–200%, đổi DPI khi kéo giữa màn hình, 1366×768, mật độ gọn/thoáng, Giao diện nhẹ | V |
| UI0-013 | Khả năng tiếp cận: tên cho trình đọc màn hình, focus nhìn thấy được, giảm chuyển động | V |
| UI0-014 | Hạ tầng hiệu năng UI: model cập nhật ở nền và áp diff theo lô, ảo hóa, đo thời gian khung của widget theo DoD-L | V |
| UI0-015 | Liên kết hành động UI → chuỗi lệnh qua INT-002; nguồn phím tắt duy nhất (bảng KDEF), nhãn phím macOS | V |

### 5.14 UIA · Khung ứng dụng

| Mã | Task neo | Màn hình | Mốc |
|---|---|---|---|
| UIA-001 | Cửa sổ chính, workspace Soạn thảo 2D / Cổ điển / Viewer, hành vi theo kích thước màn hình | UI-A01 | V |
| UIA-002 | Thanh tiêu đề, Quick Access Toolbar, ô Tìm lệnh, menu tài khoản | UI-A01, UI-A04 | V |
| UIA-003 | Menu ứng dụng | UI-A03 | V |
| UIA-004 | Tab Start | UI-A02 | V |
| UIA-005 | Khung ribbon: định nghĩa JSON, các loại điều khiển, panel mở rộng, thu gọn 3 mức, co panel theo ưu tiên, gallery xem trước, tooltip, chuột phải | UI-A05 | V |
| UIA-006 | Nội dung ribbon: các tab 07.1 §7.2–7.10, tab ngữ cảnh §7.11, thanh menu và toolbar Cổ điển §7.12 | UI-A05 | EA |
| UIA-007 | Dòng lệnh | UI-A06 | V |
| UIA-008 | Vùng vẽ: widget QRhi, con trỏ, điều hướng, khung chọn, tô sáng, định tuyến phím | UI-A07 | V |
| UIA-009 | Thanh trạng thái | UI-A08 | V |
| UIA-010 | Tab tệp | UI-A09 | V |
| UIA-011 | Tab layout và viewport | UI-A10 | V |
| UIA-012 | Menu chuột phải | UI-A11 | V |
| UIA-013 | Grip, marker bắt điểm, đường theo dõi, ô nhập động, xem trước trong lệnh | UI-A12 | V |
| UIA-014 | Nhấp đúp, kéo-thả, thành phần nổi trên vùng vẽ | UI-A13 | V |
| UIA-015 | Thông điệp, toast, tiến trình, lỗi, trạng thái rỗng (gắn sự kiện của ứng dụng) | UI-A14 | V |
| UIA-016 | Lần chạy đầu | UI-A15 | V |
| UIA-017 | Viewer: ribbon Viewer, chế độ chỉ xem, layer trong Viewer | UI-M01, UI-M03, UI-M04 | V |
| UIA-018 | Bản macOS: thanh menu hệ thống, nút cửa sổ bên trái, ⌘, hộp thoại dạng sheet, cử chỉ touchpad, tổ hợp thay F-key | UI-MAC | 1.0 |
| UIA-019 | Nhiều cửa sổ bản vẽ (xếp lớp, chia ô P1; cửa sổ nổi P2), Clean Screen | UI-A01 | 1.0 |

Các nhóm UI còn lại (UIB…UIN) không có task neo: phụ thuộc giữa chúng ghi bằng mã màn hình `UI-xxx`.

## 6. Thứ tự theo mốc

Theo [06](../06-lo-trinh-trien-khai.md). Cột cuối là các chuỗi phụ thuộc dài nhất (đường găng), cần người giỏi nhất và bắt đầu sớm nhất.

| Giai đoạn | Mốc | Phạm vi task | Đường găng |
|---|---|---|---|
| GĐ0 (T1–3) | S1–S6, CC | Mốc `S*` và `CC`: hạ tầng NT, QA tối thiểu, spike codec (S1), renderer (S2), hình học (S3), Bác sĩ Font (S4), LISP (S5), IME và text (S6), công cụ miễn phí | NT-001 → DWG-001/002/003 → DWG-005 → DWG-010 → QA-002 (S1); RND-001 → RND-004/005 → RND-010 → RND-025 (S2); GEO-003 → GEO-008 → GEO-013 → GEO-014 (S3) |
| GĐ1 (T4–17) | V | Mốc `V`: codec đọc đủ phiên bản, regen, renderer đầy đủ, khung UI, Viewer | DOC-003 → DOC-009 → RGN-003 → RGN-009 (MTEXT) và RGN-014 (dim); RND-014 (paper space) → PLT-003 → PLT-004 (PDF) → QA-005 (SSIM); UI0-001 → UIA-008 → UIA-017 |
| GĐ2a (T18) | VP | Mốc `VP`: bộ ghi DWG, undo, chuyển font, PUBLISH, in hàng loạt theo khung tên, COMPARE, giấy phép | DWG-018 → DWG-021/022 → QA-003, QA-004 (3 bộ đọc); VN-005 → VN-006 |
| GĐ2b (T18–25) | EA | Mốc `EA`: command framework đủ, khoảng 80 lệnh P0, LISP/DCL, các màn hình biên tập | INT-002 → INT-012 → các lệnh P0 (03 Phụ lục A) → QA-007; LSP-003 → LSP-006 → LSP-009 → QA-008 ≥ 60% |
| GĐ3 (T28–36) | 1.0 | Mốc `1.0`: P1, dynamic block, bảng, nhập PDF, Doanh nghiệp, SDK, macOS | RGN-019 (EvalGraph) → grip dynamic block; LSP-015/017 → QA-008 ≥ 90%; UIA-018 (macOS) |
| Sau v1.0 | Sau | Mốc `Sau`: P2 đã có thiết kế UI (tab Tham số, Sheet Set Manager, PC3, kiểm tra TCVN, CUI…) | GEO-024 → lệnh ràng buộc → UI-PAR |

**Nghiệm thu theo mốc** (lấy từ 05, 06):
- **G0 (T3):** đạt tiêu chí spike 05 §21. Mỗi spike có một task nghiệm thu ở module chính (DWG, RND, GEO, VN, LSP, UIA) đo đúng tiêu chí đó.
- **G1 (T17):** mở thành công ≥ 98% kho 10.000 DWG; SSIM ≥ 0,97 trên ≥ 95% tờ; nhanh hơn DWG TrueView ở ≥ 70% bài đo.
- **G2 (T27):** toàn bộ lệnh P0 khớp bộ tham chiếu; tệp ghi ra qua được 3 bộ đọc độc lập; crash-free ≥ 99,5%; LISP ≥ 75%.
- **G3 (v1.0):** đạt toàn bộ ngân sách 05 §1; LISP ≥ 90%; SSIM ≥ 0,98 trên ≥ 97% tờ; mở ≥ 99,5% kho.

## 7. Task phát sinh từ QA

Lỗi tìm được khi chạy kho bản vẽ, bộ tham chiếu, bộ LISP, fuzzing không liệt kê trước được. Mỗi lỗi (hoặc mỗi cụm lỗi cùng nguyên nhân) thành một task theo mẫu:

```
### FIX-DWG-0001 · R2007: sai offset luồng string khi object có > 64 KB dữ liệu
- **Mốc:** theo mốc của tính năng · **Nguồn:** QA-002, lần chạy 2027-03-14, 37 tệp
- **Đầu vào:** danh sách tệp lỗi trong kho, log, ca tái hiện tối thiểu.
- **Đầu ra:** sửa lỗi ở đúng chỗ gốc + ca kiểm thử hồi quy (tệp tối thiểu hoặc fuzz seed).
- **Tiêu chí nghiệm thu:** mọi tệp trong danh sách mở được và round-trip trùng 100%; tỉ lệ đạt của toàn kho không giảm; đạt DoD-C.
```

Mã `FIX-<module>-NNNN` đặt theo thứ tự tạo. Bảng chỉ số QA-010 xếp hạng cụm lỗi theo số tệp bị ảnh hưởng, để sửa theo tần suất thực tế (06 §9 rủi ro 1).

## 8. Thống kê

Số liệu ngày 20/10/2026, sinh bằng `python3 tools/check_tasks.py`; chạy lại để có số mới.

- **Tổng:** 1.266 task trong 20 tệp, gồm 286 task neo.
- **Theo mốc:** S1 57 · S2 31 · S3 28 · S4 9 · S5 9 · S6 17 · CC 10 · V 335 · VP 70 · EA 334 · 1.0 264 · Sau 102.
- **Theo cỡ:** S 237 · M 791 · L 238. **Theo ưu tiên:** P0 799 · P1 358 · P2 109.
- **Độ phủ:** 176/176 mã màn hình có task; 125/125 lệnh P0 của 03 Phụ lục A có task chủ; 559 tên lệnh được khai báo, mỗi tên đúng một task.
- **Kiểm tra:** `check_tasks.py -v` cho 0 lỗi, 0 cảnh báo (cú pháp, mã trùng, phụ thuộc, chu trình, mốc, neo, độ phủ).

| Tệp | Task |
|---|---|
| [core-cmd-1-ve-chon-truy-van.md](core-cmd-1-ve-chon-truy-van.md) | 81 |
| [core-cmd-2-sua-tep-tien-ich.md](core-cmd-2-sua-tep-tien-ich.md) | 82 |
| [core-cmd-3-chu-thich-layout.md](core-cmd-3-chu-thich-layout.md) | 81 |
| [core-cmd-4-layer-block-xref.md](core-cmd-4-layer-block-xref.md) | 80 |
| [core-doc-mo-hinh.md](core-doc-mo-hinh.md) | 42 |
| [core-dwg-codec.md](core-dwg-codec.md) | 63 |
| [core-geo-hinh-hoc.md](core-geo-hinh-hoc.md) | 62 |
| [core-int-tuong-tac.md](core-int-tuong-tac.md) | 62 |
| [core-lsp-lisp-sdk.md](core-lsp-lisp-sdk.md) | 62 |
| [core-nt-nen-tang.md](core-nt-nen-tang.md) | 29 |
| [core-plf-he-thong.md](core-plf-he-thong.md) | 28 |
| [core-plt-in-xuat.md](core-plt-in-xuat.md) | 34 |
| [core-qa-kiem-thu.md](core-qa-kiem-thu.md) | 28 |
| [core-rgn-regen.md](core-rgn-regen.md) | 63 |
| [core-rnd-renderer.md](core-rnd-renderer.md) | 53 |
| [core-vn-viet-nam.md](core-vn-viet-nam.md) | 57 |
| [ui-0-nen-tang-khung.md](ui-0-nen-tang-khung.md) | 70 |
| [ui-1-tep-thuoc-tinh-kieu-ve.md](ui-1-tep-thuoc-tinh-kieu-ve.md) | 97 |
| [ui-2-chu-thich-block-in-truy-van.md](ui-2-chu-thich-block-in-truy-van.md) | 108 |
| [ui-3-vn-tudong-hethong.md](ui-3-vn-tudong-hethong.md) | 84 |
