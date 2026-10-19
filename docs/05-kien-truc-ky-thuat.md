# 05 — Kiến trúc kỹ thuật: lõi tự viết, hiệu năng cao, logic chính xác

> **Quyết định của nhà sáng lập (18/10/2026):**
> - không chi tiền mua SDK hay giấy phép lõi (ODA, IntelliCAD, Spatial…);
> - lõi CAD **tự viết**;
> - **hiệu năng cao**, **logic chính xác** và **tận dụng tối đa sức mạnh phần cứng** là yêu cầu hàng đầu.
>
> Tài liệu này mô tả kiến trúc đáp ứng các yêu cầu đó. Mục gắn **[spike]** phải được chứng minh bằng prototype ở Giai đoạn 0.

## 0. Nguyên tắc

1. **Tự viết mọi thứ tạo nên CAD.** Gồm định dạng DWG/DXF, mô hình dữ liệu, hình học, regen, hiển thị, lệnh, LISP và in ấn. Chỉ dùng thư viện ngoài cho **hạ tầng phổ thông** (cửa sổ, font, nén, mật mã…), theo danh sách ở §2.
2. **Hiệu năng là tính năng, đo bằng benchmark trong CI.** Mỗi thay đổi làm chậm hơn ngân sách đều bị chặn merge.
3. **Chính xác trước, nhanh sau, nhưng phải đạt cả hai trước khi phát hành.**
   - Kernel hình học dùng predicate bền vững và chính sách dung sai thống nhất.
   - Mọi lệnh có đặc tả hành vi viết thành văn bản và có bộ kiểm thử.
4. **Một nguồn sự thật.** Mô tả định dạng DWG viết bằng schema và sinh ra code đọc/ghi/DXF/metadata thuộc tính, không viết tay hai lần.
5. **Tất định (deterministic).** Cùng đầu vào cho cùng kết quả trên Windows, macOS và Linux.
6. **Tận dụng tối đa phần cứng.** Song song hóa mọi chỗ an toàn, dùng SIMD và GPU (cả compute). Tự thích ứng để chạy mượt từ máy văn phòng cũ đến máy trạm nhiều nhân (§9).

## 1. Mục tiêu hiệu năng và độ chính xác (ngân sách cho bản v1.0)

Máy tham chiếu tầm trung là Core i5 thế hệ 10–12, 16 GB RAM, GPU tích hợp. Máy tham chiếu thấp là Core i3 thế hệ 6–8, 8 GB RAM, Intel HD 520/620, đại diện cho máy cũ ở văn phòng SME **[khảo sát lại ở Giai đoạn 0]**.

### 1.1 Hiệu năng

| Chỉ số | Mục tiêu (máy tầm trung) | Máy thấp | Đối chứng |
|---|---|---|---|
| Mở DWG 20 MB | ≤ 1,5 s | ≤ 4 s | DWG TrueView (miễn phí) trên cùng máy |
| Mở DWG 100 MB | ≤ 5 s (đọc song song) | ≤ 15 s | DWG TrueView |
| Pan/zoom 1 triệu entity 2D | ≥ 60 fps | ≥ 30 fps | — |
| Pan/zoom 10 triệu entity (GPU rời) | ≥ 30 fps | — | — |
| Truy vấn OSNAP hoặc chọn đối tượng tại con trỏ | ≤ 1 ms | ≤ 3 ms | — |
| Cập nhật rubber-band (jig) | ≤ 8 ms/khung | ≤ 16 ms | — |
| Hoàn tất lệnh cơ bản (TRIM, OFFSET, FILLET trên đối tượng thường) | ≤ 20 ms | ≤ 50 ms | — |
| Khởi động ấm | ≤ 1,5 s | ≤ 3 s | — |
| Bộ nhớ cho DWG 100 MB | ≤ 1,2 GB | — | — |
| Regen toàn bộ 1 triệu entity | ≤ 0,5 s (8 nhân) | ≤ 2 s (4 nhân) | — |
| **Hiệu suất song song** (đọc DWG, regen, dựng BVH, xuất PDF) | ≥ 70% so với tuyến tính, đến 16 luồng | — | Benchmark scaling 1→32 luồng |
| **Tự chọn backend đồ họa và mức chất lượng theo phần cứng** | Đúng trên 100% máy trong ma trận kiểm thử | — | — |
| Khi máy rảnh | ~0% CPU/GPU (chỉ vẽ khi cần), tiết kiệm pin laptop | — | — |

### 1.2 Độ chính xác

| Chỉ số | Mục tiêu |
|---|---|
| Mở DWG thành công (kho ≥ 10.000 bản vẽ VN) | ≥ 99,5% |
| Round-trip: đọc → ghi → đọc | **100% trùng khớp từng trường** với đối tượng không bị sửa, kể cả đối tượng chưa hiểu (giữ nguyên dạng thô) |
| Tệp TrionCAD lưu | Ba bộ đọc độc lập (DWG TrueView, ODA File Converter, LibreDWG CLI) mở không lỗi, không hỏi recover |
| Hiển thị và in so với bản in DWG TrueView | SSIM ≥ 0,98 trên ≥ 97% tờ |
| Chuỗi text của kích thước (đơn vị, làm tròn, lược số 0, dung sai) | 100% trùng với bộ tham chiếu |
| Ngắt dòng MTEXT | ≥ 99% đoạn trùng với bộ tham chiếu |
| Kết quả hình học của lệnh (TRIM, EXTEND, OFFSET, FILLET, CHAMFER, BREAK, JOIN, HATCH boundary…) | Đúng đặc tả; sai số ≤ 1e-9 × kích thước bản vẽ; tô-pô (số đoạn, thứ tự, chiều) trùng bộ tham chiếu |
| LISP | ≥ 90% bộ LISP cộng đồng chạy đúng, không phải sửa |
| Ổn định | ≥ 99,7% phiên không crash |

"Bộ tham chiếu" được giải thích ở §14.

## 2. Chính sách phụ thuộc: ranh giới "tự viết"

| Nhóm | Thành phần | Cách làm |
|---|---|---|
| **Tự viết 100% (lõi)** | Codec DWG/DXF; mô hình bản vẽ và undo; kernel hình học 2D; regen (SHX, MTEXT, kích thước, hatch, linetype, leader/mleader, bảng, dynamic block, proxy graphics, viewport); renderer; chỉ mục không gian, OSNAP, chọn, grip; command framework và toàn bộ lệnh; engine AutoLISP/Visual LISP và DCL; plot engine và **bộ ghi PDF vector**; Bác sĩ Font; hệ thống giấy phép; công cụ QA | Mã của TrionCAD, sở hữu 100% |
| **Hạ tầng phổ thông được phép** (miễn phí, giấy phép thoáng, không tạo khác biệt, thay được) | **Qt 6** (LGPLv3, link động): cửa sổ, widget, IME tiếng Việt, **Qt RHI** (lớp đồ họa D3D11/12, Metal, Vulkan, OpenGL), in ấn (`QPrinter`), đọc ảnh (`QImage`), chuẩn hóa Unicode (`QString::normalized`). **FreeType** + **HarfBuzz**: đường viền và shaping cho font TrueType. **zlib/libdeflate**: nén. **PDFium** (BSD): chỉ để *đọc và hiển thị* PDF (underlay, nhập PDF). **libsodium** (ISC): chữ ký Ed25519 cho giấy phép. **Crashpad** (Apache-2.0): thu crash dump. Công cụ dev: GoogleTest/Catch2, Google Benchmark, Tracy profiler | Bọc sau một lớp mỏng của TrionCAD. Không đưa logic CAD vào đây |
| **Chỉ dùng tham khảo hoặc làm công cụ đối chứng** (không đưa vào sản phẩm) | Đặc tả DWG miễn phí của ODA (v5.4.1, R13–2018); DXF Reference công khai của Autodesk; ACadSharp (MIT, được đọc mã); thuật toán của Clipper2, CavalierContours, PlaneGCS; **LibreDWG (GPL): chỉ chạy CLI làm đối chứng, không đọc hay chép mã**; ODA File Converter và DWG TrueView (miễn phí): đối chứng mở và in | — |
| **Cấm** | Thư viện GPL/AGPL trong sản phẩm; mã hoặc SDK của Autodesk (ObjectARX, RealDWG); mọi SDK trả phí; tự viết thuật toán mật mã | — |

**Vì sao không tự viết cả hạ tầng phổ thông?**
- Tự viết bộ font rasterizer, nén, giải mã ảnh hay mật mã không làm CAD tốt hơn.
- Các phần này lại rất dễ có lỗi bảo mật.
- Tốn hàng chục năm-người mà không tạo khác biệt.

Các thư viện này đều miễn phí, nên không trái với quyết định "không mua tài nguyên".

**Ngoại lệ có chủ đích: kernel hình học 2D tự viết sẽ *chính xác hơn* thư viện có sẵn.** Clipper2 và các thư viện đa giác chỉ xử lý đoạn thẳng, nên buộc phải xấp xỉ cung và spline. CAD cần biên hatch, region và offset **giữ nguyên cung/spline thật**. Vì vậy kernel phải tự viết.

## 3. Chi phí bắt buộc nhỏ (không phải SDK lõi)

| Khoản | Ước tính | Lý do không tránh được |
|---|---|---|
| Chứng thư ký số mã nguồn (Windows) | ~200–600 USD/năm | Thiếu ký số thì Windows SmartScreen chặn trình cài đặt |
| Apple Developer Program (khi làm bản macOS) | 99 USD/năm | Bắt buộc để notarize |
| Tên miền, máy chủ (tải về, kích hoạt, telemetry, crash) | ~10–30 triệu đ/tháng | Vận hành |
| Văn bản TCVN chính thức, phí đăng ký nhãn hiệu | Vài chục triệu đ | Mẫu đúng chuẩn, bảo vệ thương hiệu |
| Máy kiểm thử (máy cũ, máy Mac, GPU khác nhau) | Mua một lần | Đo hiệu năng thật |

## 4. Lựa chọn nền tảng (ADR)

**ADR-001: C++20 cho toàn bộ lõi**
- *Bối cảnh:* cần hiệu năng tối đa, quản lý bộ nhớ chủ động, SIMD và đa luồng; sau này cần chạy được trên web (WASM). Phần mềm chuyên ngành VN như Nova-TDN cũng viết bằng Visual C++/ARX.
- *Quyết định:* C++20 (MSVC và Clang), CMake. Release build bật LTO và PGO. SIMD (SSE4.2/AVX2, NEON) chọn khi chạy.
- *Hệ quả:* phải kỷ luật về an toàn bộ nhớ: sanitizer trong CI, fuzzing, quy tắc code, không dùng con trỏ thô sở hữu. Rust đã được cân nhắc: an toàn hơn, nhưng hệ sinh thái UI desktop yếu và khó tuyển người tại VN.

**ADR-002: Qt 6 Widgets cho UI, Qt RHI cho đồ họa**
- *Quyết định:* Qt 6 LGPLv3, link động. Vùng vẽ dùng `QRhiWidget`/QRhi: viết renderer **một lần** cho D3D11 (Windows, tương thích GPU Intel cũ tốt nhất), Metal (macOS), Vulkan/OpenGL (Linux), WebGL (Qt for WebAssembly, sau này).
- *Rủi ro:* QRhi cam kết tương thích API ở mức hạn chế giữa các bản Qt. **[Spike S2]** Nếu không đạt, dùng OpenGL 3.3 core (tập con chung với GLES 3.0/WebGL2).

**ADR-003: Mô hình dữ liệu, codec và kernel đều tự viết.** Không phụ thuộc mô hình của bên thứ ba. Lý do: kiểm soát bố cục bộ nhớ (hiệu năng), round-trip chính xác và sở hữu trí tuệ.

**ADR-004: Số học dấu phẩy động nghiêm ngặt**
- Kernel hình học bắt buộc tuân thủ IEEE-754, cấm `-ffast-math`.
- Tắt `-ffp-contract` (FMA) để kết quả giống nhau giữa các nền tảng và trình biên dịch.
- Hàm lượng giác dùng bản thống nhất của TrionCAD ở những chỗ quyết định tô-pô.

## 5. Kiến trúc tổng thể

### 5.1 Sơ đồ phân lớp

```
┌────────────────────────────────────────────────────────────────────────────┐
│ UI shell (Qt 6 Widgets): Ribbon (JSON) · Dòng lệnh · Thanh trạng thái ·     │
│ Tab tệp/layout · Palette (Properties, Layers, Blocks, Tool, Xref) · Hộp thoại│
├────────────────────────────────────────────────────────────────────────────┤
│ Tương tác & lệnh: Command registry · Input pipeline (bàn phím/script/LISP/  │
│ macro/AI) · Prompt & keyword · Jig · Selection · OSNAP/POLAR/OTRACK · Grip · │
│ Dynamic Input · UCS · Sysvar · ~200 lệnh                                     │
├────────────────────────────────────────────────────────────────────────────┤
│ Mở rộng: AutoLISP/Visual LISP engine · DCL (Qt) · SDK C API + C++ wrapper ·  │
│ CUI/PGP · (P3) .NET/Python bọc trên C API · (P3) MCP server                 │
├────────────────────────────────────────────────────────────────────────────┤
│ Lõi bản vẽ (TỰ VIẾT):                                                       │
│  Document model (handle, owner, dictionary, xdata, reactor) · Undo journal  │
│  Codec DWG R13–2018 / DXF (schema → codegen) · Kernel hình học 2D           │
│  Regen: SHX/TTF · MTEXT · Dimension · Hatch · Linetype · Leader/MLeader ·   │
│         Table · Dynamic block · Proxy graphics · Viewport                   │
│  Plot engine (CTB/STB) · Bộ ghi PDF vector · (PDFium: đọc PDF)              │
├────────────────────────────────────────────────────────────────────────────┤
│ Renderer (TỰ VIẾT trên Qt RHI): batch theo vật liệu · instancing block ·    │
│ BVH culling · LOD · cung/đường tròn giải tích trong shader · RTC precision · │
│ overlay tạm thời · nhiều viewport/paper space                              │
├────────────────────────────────────────────────────────────────────────────┤
│ Nền: FontResolver (+Bác sĩ Font) · Tìm tệp · Giấy phép · Cập nhật · Crash · │
│ Telemetry (opt-in) · Cài đặt/profile · Thread pool (work-stealing)          │
└────────────────────────────────────────────────────────────────────────────┘
  Cùng một lõi cho 3 sản phẩm: trion (GUI) · trion-console (headless, CI) ·
  TrionCAD Viewer (cùng tệp chạy, giấy phép Viewer tắt lệnh sửa)
```

### 5.2 Các module

| Module | Trách nhiệm |
|---|---|
| `core/doc` | Mô hình bản vẽ: entity và object theo kiểu dữ liệu hướng dữ liệu (mảng liên tục theo loại, handle → chỉ số), owner/dictionary/xdata/xrecord, sự kiện thay đổi, undo journal |
| `core/dwg`, `core/dxf` | Codec định dạng, sinh code từ schema (§6) |
| `core/geom` | Kernel hình học 2D (§7) |
| `core/regen` | Biến đối tượng thành hình hiển thị: text, MTEXT, kích thước, hatch, linetype, leader, bảng, dynamic block, proxy (§8) |
| `render` | Renderer QRhi, BVH, LOD, overlay (§9) |
| `cmd`, `interact`, `commands/*` | Lệnh và tương tác (§10) |
| `lisp` | AutoLISP/Visual LISP, DCL (§11) |
| `plot` | Page setup, CTB/STB, bộ ghi PDF, in qua `QPrinter` |
| `vn` | Bác sĩ Font, quy tắc TCVN (§13) |
| `sdk` | C API ổn định và C++ wrapper header-only (§12) |
| `platform` | Giấy phép, cập nhật, crash, telemetry, cài đặt |
| `qa` | Bộ chạy kho bản vẽ, so khớp round-trip, đối chứng chéo, so sánh ảnh, bộ LISP, benchmark, fuzzer (§14) |

### 5.3 Luồng chính

**Mở DWG**
```
OPEN → mmap tệp → đọc header và section map → giải nén các section (song song theo trang)
     → đọc object map (handle → offset) → GIẢI MÃ ĐỐI TƯỢNG SONG SONG (work-stealing)
     → dựng chỉ mục owner/dictionary → (Viewer) giải mã lười đối tượng chưa cần
     → vn::scanEncodings (chế độ hiển thị) → regen song song từng entity → upload GPU
     → khung hình đầu tiên (trong lúc phần còn lại tiếp tục nạp)
```

**Lệnh LINE**
```
"L" + Enter → alias PGP → cmd::run("_LINE") → undo.beginMark
→ prompt → interact: OSNAP qua BVH (≤ 1 ms), POLAR, DYN → điểm
→ jig: chỉ vẽ lại overlay, không chạm DB → điểm 2 → tạo entity qua API của doc (ghi journal)
→ regen entity mới → cập nhật batch GPU cục bộ → lặp → undo.endMark
```
Script `.scr`, `(command ...)` của LISP, macro và AI **đi qua cùng một input pipeline**. Mọi thay đổi DB **chỉ đi qua API của `core/doc`**: undo, sự kiện và regen luôn nhất quán, không có đường tắt.

### 5.4 Mô hình luồng

- **Một luồng ghi:** luồng UI sở hữu document. Lệnh và LISP chạy trên luồng này.
- **LISP:** evaluator kiểu **máy ngăn xếp tường minh**, không đệ quy trên stack của C++. Nhờ vậy, khi gặp `getpoint` hay `(command ... pause)`, engine có thể *tạm dừng rồi tiếp tục* mà không chặn UI. Cách này cũng chống tràn stack khi LISP đệ quy sâu.
- **Song song chỉ đọc:**
  - giải mã DWG;
  - regen từng entity (là hàm thuần của entity và tham số khung nhìn);
  - dựng BVH, tô-pô hatch, so sánh bản vẽ, xuất PDF trên ảnh chụp bất biến của DB.
- **Thread pool work-stealing** dùng chung cho mọi tác vụ.

## 6. Codec DWG/DXF tự viết

### 6.1 Phạm vi
- **Đọc:** R13, R14, 2000, 2004, 2007, 2010, 2013, 2018 (AC1012–AC1032). AutoCAD 2018–2027 đều dùng AC1032. Đọc R12 trở về trước: P2.
- **Ghi:** AC1032 và AC1018 (P0); AC1015, AC1021, AC1024, AC1027 (P1).
- **DXF:** đọc và ghi mọi phiên bản (ASCII và nhị phân).
- **Phải xử lý:**
  - bit-stream có mã hóa bit (BS, BL, BD, MC, MS, handle reference);
  - section và page map;
  - nén LZ77 biến thể của R2004+;
  - mã Reed–Solomon và cách tổ chức riêng của **R2007** (phiên bản khó nhất);
  - CRC;
  - class động;
  - khoảng 120 loại entity/object;
  - proxy entity và proxy graphics;
  - xdata, xrecord, extension dictionary;
  - layout, plot settings, visual style, material, table style, mleader style, field, dynamic block (EvalGraph, BlockRepresentation…).

### 6.2 Thiết kế
1. **Schema làm nguồn sự thật duy nhất.** Mỗi loại đối tượng được mô tả bằng một tệp schema (YAML/JSON hoặc DSL nhỏ): trường, kiểu bit, điều kiện theo phiên bản, mã nhóm DXF tương ứng, tên thuộc tính. **Codegen** sinh từ schema:
   - bộ đọc DWG;
   - bộ ghi DWG;
   - bộ đọc/ghi DXF;
   - metadata thuộc tính (palette Properties, LISP `entget`/`entmod`, so sánh bản vẽ).

   Một nguồn duy nhất loại bỏ được cả một lớp lỗi lệch giữa đọc và ghi.
2. **Giữ nguyên dữ liệu chưa hiểu.** Đối tượng hoặc trường chưa có trong schema được giữ **nguyên khối bit** cùng các handle tham chiếu, rồi ghi lại y hệt. Handle không bao giờ bị đánh lại số. Người dùng không bao giờ mất dữ liệu của Civil 3D, plugin hay phiên bản mới.
3. **Nạp lười cho Viewer và tệp lớn:** chỉ giải mã đối tượng khi cần hiển thị hoặc truy vấn.
4. **Chế độ Recover:** chịu được tệp hỏng (CRC sai, offset lệch). Ghi nhật ký những gì đã sửa.
5. **Nguồn kiến thức:**
   - đặc tả DWG miễn phí của ODA (v5.4.1);
   - DXF Reference công khai của Autodesk (mã nhóm, ngữ nghĩa trường);
   - ACadSharp (MIT) để tham khảo;
   - thực nghiệm trên kho bản vẽ.

   **Không đọc mã LibreDWG (GPL)**, chỉ chạy nó như hộp đen để đối chứng.

### 6.3 Kiểm thử codec
- **Round-trip từng trường:** đọc → ghi → đọc phải trùng 100%. Chạy trên toàn kho bản vẽ mỗi đêm.
- **Đối chứng chéo:** so dữ liệu ta đọc được với kết quả xuất DXF của ODA File Converter và với `dwgread` (LibreDWG) trên cùng tệp. Chênh lệch nào cũng phải được giải thích.
- **Kiểm tra tệp ta ghi:** mở bằng DWG TrueView (không hỏi recover), ODA File Converter (không lỗi), LibreDWG (không lỗi).
- **Fuzzing** bộ đọc (libFuzzer kết hợp ASan/UBSan) liên tục.
- **Hiệu năng:** đo thời gian và bộ nhớ khi mở các tệp chuẩn, đặt ngưỡng chặn tụt hiệu năng.

## 7. Kernel hình học 2D tự viết ("logic chính xác")

| Thành phần | Thiết kế |
|---|---|
| **Chính sách dung sai** | Đối tượng `Tolerance` toàn cục (điểm trùng, vector trùng, góc), tỉ lệ theo kích thước bản vẽ. Mọi so sánh hình học đi qua nó, cấm so sánh `==` trực tiếp trên số thực |
| **Predicate bền vững** | `orient2d`, `incircle`… theo phương pháp số học thích ứng độ chính xác (Shewchuk). Trường hợp suy biến chuyển sang số học chính xác (expansion, int128). Quyết định tô-pô **không bao giờ** dựa trên phép trừ thô |
| **Kiểu đường cong** | Đoạn thẳng, cung, đường tròn, ellipse/cung ellipse, spline (NURBS: đánh giá, đạo hàm, chèn nút, chia), polyline có bulge, đa tuyến 3D chiếu xuống mặt phẳng |
| **Giao điểm** | Giải tích cho đoạn thẳng/cung/đường tròn/ellipse. Spline dùng chia nhỏ + bao khoảng + Newton tinh chỉnh, **cô lập nghiệm có bảo đảm** (không sót giao điểm). Phân loại tiếp xúc và trùng đoạn |
| **Offset** | Offset thô từng đoạn (cung offset thành cung), rồi nối, cắt tự giao và lọc theo khoảng cách. Thuật toán tự viết, tham khảo ý tưởng CavalierContours. Kiểm thử bằng bất biến: mọi điểm trên kết quả cách nguồn đúng `d ± tol` |
| **Sắp xếp phẳng (arrangement)** | DCEL với cạnh là đường cong thật (không xấp xỉ), dùng cho biên hatch (pick point, island), `BOUNDARY`, region boolean, `TRIM`/`EXTEND` theo biên ngầm |
| **Tessellation để hiển thị** | Có **sai số dây cung theo pixel**. Cung/đường tròn ưu tiên vẽ giải tích trong shader (§9) |
| **Tất định** | Sắp xếp theo khóa ổn định; không phụ thuộc thứ tự băm hay địa chỉ bộ nhớ |
| **Tọa độ lớn** | Bản vẽ VN-2000 có tọa độ khoảng 10⁶ m, đơn vị mm nên khoảng 10⁹. Tính toán dùng `double`; hiển thị dùng RTC (§9). Thêm tọa độ lớn vào bộ kiểm thử bất biến |

**Kiểm thử kernel:**
- Property-based: hàng triệu ca ngẫu nhiên và suy biến (đồng tuyến, tiếp xúc, trùng điểm, cung rất nhỏ hoặc rất lớn).
- Ca vàng (golden) cho từng lệnh.
- Fuzzing.
- Benchmark từng thuật toán.

### 7.1 Kernel 3D (P3, quyết định ở cổng G3)

DWG lưu `3DSOLID`/`REGION`/`BODY` dưới dạng dữ liệu ACIS (SAT/SAB). Lộ trình:
1. **P1 — giữ nguyên** khối dữ liệu ACIS khi mở và lưu: không làm hỏng bản vẽ 3D.
2. **P3 — bộ đọc SAT/SAB tự viết:**
   - các mặt phẳng, trụ, nón, cầu, xuyến, NURBS;
   - mặt procedural xấp xỉ có kiểm soát sai số;
   - lưới hóa (faceting) để hiển thị và `FLATSHOT`.
3. **P3–P4 — mô hình hóa:** chọn giữa hai hướng.
   - Tự viết một kernel B-rep tập trung vào thao tác hay dùng (extrude, revolve, boolean khối đơn giản, fillet bán kính hằng).
   - Dùng Open CASCADE (LGPL, miễn phí).

   Điều kiện chung: phải **ghi lại được SAT/SAB hợp lệ** để AutoCAD mở. Đây là phần khó nhất của 3D, nên chỉ quyết định khi có nhu cầu thật và đủ người.

## 8. Regen (sinh hình hiển thị từ dữ liệu)

| Engine | Ghi chú về độ chính xác |
|---|---|
| **SHX** | Parser shape font, bigfont và unifont. Glyph là nét (đoạn thẳng, cung), cache theo glyph. Khớp với SHX tiêu chuẩn |
| **TrueType** | FreeType lấy đường viền, HarfBuzz shaping. Cache glyph; text nhỏ dùng atlas SDF, text lớn dùng đường viền tam giác hóa |
| **MTEXT** | Engine dàn trang tự viết: mã định dạng (`\f \H \W \Q \T \S \P \L \O \K \A \C`…), xếp chồng phân số/dung sai, tab, thụt lề, bullet, cột, giãn dòng, bao quanh. Bộ tham chiếu ngắt dòng: ≥ 99% trùng khớp |
| **Kích thước** | Sinh block kích thước theo DIMVAR (mũi tên, vị trí text, quy tắc fit `DIMATFIT`/`DIMTMOVE`, đơn vị chính và phụ, `DIMRND`, `DIMZIN`, dung sai…). **Định dạng số khớp từng ký tự** với bộ tham chiếu. Khi chỉ xem: dùng block lưu sẵn trong DWG |
| **Hatch** | Parser `.pat`. Sinh nét mẫu cắt chính xác theo biên đường cong (kernel §7). Island, gradient, tô đặc. LOD khi nét quá dày |
| **Linetype** | Mẫu nét đơn giản và phức (shape, text). Sinh theo chiều dài cung thật, tôn trọng `LTSCALE`/`PSLTSCALE`/`CELTSCALE` và chế độ `PLINEGEN` |
| **Leader/MLeader, bảng** | Sinh lại từ dữ liệu ngữ cảnh. Bảng dùng block đại diện khi chỉ xem |
| **Dynamic block** | Khi xem: dùng block đại diện `*U`. Khi sửa: engine EvalGraph tự viết cho parameter và action (P1) |
| **Proxy graphics** | Parser định dạng proxy graphics để hiển thị đối tượng của Civil 3D, Architecture… (P1) |

## 9. Renderer và tận dụng tối đa sức mạnh phần cứng

Mục tiêu: **chạy mượt trên máy văn phòng cũ, và trên máy mạnh thì dùng hết mọi nhân CPU, mọi đơn vị SIMD và GPU**. Hiệu năng phải tăng theo phần cứng, không bị giới hạn bởi một luồng.

### 9.1 GPU: renderer tự viết trên Qt RHI

- **Backend chọn tự động:**
  - D3D12, Vulkan hoặc Metal trên GPU đời mới: overhead CPU thấp, đẩy được nhiều draw call.
  - D3D11 hoặc OpenGL cho máy cũ.
  - Laptop có hai GPU: yêu cầu dùng GPU rời (export `NvOptimusEnablement`/`AmdPowerXpressRequestHighPerformance` trên Windows).
- **Dữ liệu hướng GPU:** bộ đệm đỉnh tĩnh theo **vật liệu** (màu, linetype, lineweight, transparency). Cập nhật cục bộ theo vùng khi entity thay đổi, không dựng lại toàn bộ.
- **Instancing cho block:** hình học của mỗi định nghĩa block được upload một lần, mỗi tham chiếu chỉ là một ma trận. Bản vẽ có hàng chục nghìn block giống nhau (cửa, cọc, cây, ký hiệu) nhẹ đi hàng chục lần.
- **Culling:** BVH phân cấp (entity và block). Chỉ mục này dùng chung cho OSNAP và chọn.
- **LOD:**
  - entity nhỏ hơn 1 pixel vẽ thành điểm;
  - text nhỏ hơn ngưỡng vẽ thành khung;
  - hatch quá dày vẽ thành mảng tô;
  - spline giảm điểm theo sai số pixel.
- **Đường cong giải tích:** cung, đường tròn và ellipse vẽ bằng quad + fragment shader tính khoảng cách chính xác, nên **mịn ở mọi mức zoom mà không phải tessellate lại**.
- **Lineweight:** mở rộng thành quad trong vertex shader, có mối nối và đầu mút đúng. **Linetype** tính trong fragment shader theo tham số chiều dài cung.
- **Độ chính xác hiển thị (RTC):** tọa độ được trừ gốc khung nhìn bằng `double` trên CPU trước khi chuyển sang `float`. Hình không rung ở tọa độ lớn (VN-2000).
- **Overlay tạm thời** (jig, grip, snap marker, khung chọn) nằm ở một lớp riêng, vẽ lại mà không động vào cảnh chính.
- **Paper space và viewport:** viewport vẽ model space với ma trận riêng và clip (scissor hoặc stencil cho viewport đa giác). Tuân thủ draw order (`SORTENTS`).
- **Khử răng cưa:** MSAA khi phần cứng hỗ trợ, nếu không thì dùng shader làm mượt cạnh.
- **Chọn đối tượng** chạy trên CPU (BVH + kiểm tra va chạm chính xác), không đọc ngược từ GPU.
- **GPU compute** (khi backend hỗ trợ: D3D11/12, Vulkan, Metal, OpenGL 4.3+): culling và chọn LOD cho hàng triệu đối tượng ngay trên GPU, sinh nét mẫu hatch và linetype dày đặc, xử lý ảnh raster underlay, so sánh ảnh khi `COMPARE`. Luôn có đường dự phòng trên CPU cho máy không hỗ trợ.
- **Bộ nhớ GPU:** đọc dung lượng VRAM, đặt ngân sách. Bản vẽ khổng lồ được chia ô (tile), chỉ giữ trên GPU phần đang thấy, loại bỏ theo LRU. Upload bất đồng bộ qua staging buffer. Trên Apple Silicon tận dụng bộ nhớ hợp nhất để tránh sao chép.
- **Màn hình:** hỗ trợ tần số quét cao (120/144 Hz) và High-DPI. **Chỉ vẽ khi có thay đổi**: máy rảnh thì không tốn CPU/GPU.
- **Đo lường:** Tracy profiler (CPU), RenderDoc, PIX, Xcode GPU tools và Nsight (đều miễn phí) cho GPU. Bộ cảnh benchmark chuẩn (1 triệu và 10 triệu entity, hatch dày, text dày, nhiều block lồng, nhiều viewport) chạy trong CI trên máy có GPU thật, kể cả máy Intel HD cũ.

### 9.2 CPU đa nhân

- **Bộ lập lịch work-stealing tự viết** kèm đồ thị tác vụ. Nhận diện số nhân, và trên CPU lai Intel thế hệ 12+ thì phân biệt nhân P/E: tác vụ cần phản hồi nhanh (khung hình, jig, OSNAP) chạy trên nhân P qua API QoS/ưu tiên luồng của hệ điều hành; tác vụ nền chạy trên nhân E.
- **Song song hóa mọi chỗ an toàn:**
  - giải nén từng trang section và giải mã từng đối tượng DWG;
  - regen từng entity;
  - dựng BVH (SAH binning song song);
  - tô-pô hatch và biên;
  - xuất PDF theo tờ;
  - `AUDIT`, `COMPARE`, `PURGE`;
  - lọc chọn trên hàng triệu đối tượng (`QSELECT`, `ssget` có bộ lọc);
  - biến đổi hàng loạt (`MOVE`, `SCALE` 100 nghìn đối tượng);
  - nạp nhiều xref và ảnh cùng lúc.
- **Ghi vẫn một luồng, đọc song song trên ảnh chụp bất biến** (copy-on-write) của document. Không khóa trên đường nóng.
- LISP giữ ngữ nghĩa đơn luồng như AutoLISP. Các hàm nặng bên dưới nó (`ssget`, `entget` hàng loạt) vẫn dùng song song bên trong.

### 9.3 SIMD và tối ưu theo kiến trúc CPU

- Một lớp SIMD mỏng tự viết (intrinsics) với **chọn đường chạy lúc khởi động**: SSE4.2 / AVX2 / AVX-512 trên x86-64, NEON trên ARM64 (Apple Silicon, Windows on ARM).
- **Ứng dụng:**
  - biến đổi ma trận hàng loạt điểm;
  - tính hộp bao và culling;
  - tessellation;
  - giải mã bit-stream và giải nén;
  - checksum (dùng lệnh phần cứng khi thuật toán cho phép);
  - xử lý glyph.
- **Build:** bản phát hành có PGO + LTO; có bản native cho x86-64 và ARM64 (Windows, macOS, Linux).

### 9.4 Bộ nhớ và lưu trữ

- **Bố cục hướng dữ liệu:** entity lưu theo mảng liên tục theo loại (SoA ở đường nóng); nút BVH 64 byte vừa một dòng cache, duyệt không rẽ nhánh; arena allocator cho mỗi document và mỗi lần regen; chuỗi (tên layer, style) được intern.
- **I/O:**
  - mmap tệp DWG;
  - đọc song song nhiều tệp (xref, ảnh) để tận dụng NVMe;
  - prefetch xref và ảnh ở nền.
- **Autosave không làm khựng:** tuần tự hóa ảnh chụp document trên luồng nền.
- **Cache trên đĩa:** shader và pipeline đã biên dịch, glyph SHX đã phân tích, thumbnail. Khởi động lần sau nhanh hơn.

### 9.5 Tự thích ứng theo phần cứng

- Lúc khởi động, đo cấu hình: số nhân, tập lệnh SIMD, GPU và API, VRAM, DPI, tần số màn hình. Từ đó chọn backend và **mức chất lượng** (MSAA, mật độ tessellation, ngưỡng LOD, số luồng nền).
- **Giữ ngân sách khung hình:** nếu một khung vượt 16 ms, tự hạ LOD tạm thời; khi người dùng dừng thao tác thì tinh chỉnh dần lên chất lượng tối đa (progressive refinement).
- Lệnh `TRIONBENCH` ("Kiểm tra hiệu năng máy") chạy bài đo chuẩn, gợi ý cấu hình và gửi kết quả ẩn danh (nếu người dùng đồng ý) để đội ngũ biết phần cứng thực tế của khách hàng.
- Laptop chạy pin: giảm số luồng nền và tần số vẽ.

### 9.6 Tương lai: NPU và GPU cho AI (P3–P4)

Suy luận AI (vector hóa ảnh scan, OCR tiếng Việt, nhận dạng block) chạy trên GPU hoặc NPU qua DirectML, Core ML hoặc ONNX Runtime (thư viện hạ tầng miễn phí), có dự phòng trên CPU.

## 10. Command framework và tương tác

- **Tên lệnh:**
  - tên toàn cục `_LINE`;
  - tiền tố `-` cho bản dòng lệnh;
  - `.` để gọi lệnh gốc;
  - `'` cho lệnh trong suốt;
  - `UNDEFINE`/`REDEFINE`.
- **Từ khóa:** cú pháp `[Option/Another]`; từ khóa toàn cục có tiền tố `_`.
- **Sysvar:** registry có kiểu và phạm vi (profile, header bản vẽ, phiên), thông báo khi đổi. Khoảng 250 sysvar ở bản GA, ưu tiên theo bộ LISP.
- **Đặc tả hành vi cho từng lệnh** (`docs/specs/commands/*.md`) gồm prompt, tùy chọn, sysvar liên quan, trường hợp biên và quy tắc hình học. Đặc tả được viết từ kiến thức nghiệp vụ, tài liệu công khai và **bộ tham chiếu do người dùng AutoCAD hợp pháp tạo ra** (§14). Không có đặc tả thì không merge lệnh.
- **OSNAP và tracking:** truy vấn BVH; xếp hạng ưu tiên giống AutoCAD; điểm theo dõi (acquired points); giao điểm biểu kiến (apparent intersection); phần kéo dài (extension).
- **Grip:** grip đa năng, grip của dynamic block, nhấn Space để luân phiên chế độ.

## 11. Engine AutoLISP / Visual LISP (C++)

| Hạng mục | Thiết kế |
|---|---|
| Kiểu dữ liệu | INT (32-bit, tràn số giống AutoLISP), REAL, STR, SYM, LIST, FILE, SUBR/USUBR, ENAME, PICKSET, VLA-OBJECT, VARIANT, SAFEARRAY |
| Ngữ nghĩa | **Dynamic scoping** (biến cục bộ sau `/`), quy tắc so sánh và in số giống AutoLISP |
| Evaluator | Máy ngăn xếp tường minh: tạm dừng và tiếp tục được khi chờ input (§5.4). Có thể biên dịch sang bytecode sau, nếu benchmark cho thấy cần |
| Bộ nhớ | GC mark-and-sweep phân thế hệ, tối ưu cho danh sách lớn |
| Hàm | Khoảng 300 hàm AutoLISP lõi; `vl-*`, `vlax-*`, `vlr-*`; `vla-*` sinh từ mô tả mô hình đối tượng (dùng chung schema với §6) |
| Định dạng số | `rtos`, `distof`, `angtos`, `angtof` theo mọi chế độ đơn vị. Kiểm thử từng ký tự |
| `ssget` | Đầy đủ chế độ chọn và bộ lọc (mã nhóm, toán tử `-4`, wildcard `wcmatch`) |
| DCL | Parser, layout xấp xỉ kích thước của AutoCAD, render bằng Qt |
| An toàn | Chế độ `SECURELOAD`, trusted paths, cảnh báo khi nạp LISP nằm cạnh bản vẽ |
| FAS/VLX | Chờ ý kiến pháp lý (§15) |
| Kiểm thử | Hơn 500 LISP thực tế, mỗi cái có kịch bản và kết quả kỳ vọng. Có bộ kiểm thử ngữ nghĩa riêng cho từng hàm |

## 12. SDK cho đối tác

- **Tầng 1: AutoLISP/DCL.** Tương thích với cộng đồng hiện có.
- **Tầng 2: C API ổn định** (ABI C, có phiên bản), kèm **C++ wrapper header-only**. Đây là SDK chính cho phần mềm chuyên ngành:
  - API phản ánh các khái niệm quen thuộc với lập trình viên ObjectARX (database, transaction, entity, reactor, command, jig) nhưng là API riêng của TrionCAD;
  - Nova-TDN và các plugin viết bằng Visual C++ có thể port với công sức vừa phải.
- **Tầng 3 (P3):** .NET và Python bọc trên C API.

## 13. Bác sĩ Font (`vn`)

- **Nhận diện bảng mã** cho từng đối tượng hoặc từng đoạn MTEXT, dựa trên:
  1. tên font của text style (`.Vn*` là TCVN3, `VNI-*` là VNI, các SHX tiếng Việt đã biết);
  2. đặc trưng byte;
  3. **chấm điểm âm tiết tiếng Việt** (mô hình n-gram nhỏ dựng từ thuật ngữ xây dựng) sau khi thử giải mã theo từng bảng mã;
  4. ngưỡng tin cậy: độ tin cậy thấp thì để người dùng xác nhận.
- **Xử lý đầy đủ:** mã `%%`, mã định dạng MTEXT, attribute, ghi đè text của dim, block lồng, bảng, field.
- **Chế độ *hiển thị*:** không đổi dữ liệu; `FontResolver` thay font thiếu và giải mã chỉ khi vẽ.
- **Chế độ *chuyển đổi*:** ghi Unicode NFC, đổi text style; có xem trước, một mark undo, chạy hàng loạt, xuất báo cáo.
- **Mục tiêu:** độ chính xác nhận diện ≥ 98% trên bộ 5.000 mẫu có nhãn.
- **Font đi kèm:** tự thiết kế bộ SHX tương thích tên và kích thước (`vntime`, `vnarial`, `romansvn`, `txt`, `simplex`, `romans`, `isocp`, `gdt`…) có đủ glyph tiếng Việt. Không đóng gói font của người khác khi chưa có giấy phép.

## 14. Bảo đảm "logic chính xác": chiến lược kiểm thử

| Lớp | Cách làm | Tần suất |
|---|---|---|
| **Đặc tả hành vi** | Mỗi lệnh và mỗi hàm LISP có tài liệu đặc tả; reviewer đối chiếu code với đặc tả | Mỗi PR |
| **Bộ tham chiếu do người dùng AutoCAD tạo** | TrionCAD soạn kịch bản `.scr` (ví dụ 500 ca TRIM/OFFSET/FILLET/DIM/HATCH…). Doanh nghiệp pilot **có bản quyền AutoCAD** chạy kịch bản và gửi lại DWG/PDF kết quả. Đó là "đáp án" để so khớp tự động. Không cần TrionCAD mua AutoCAD | Cập nhật mỗi quý |
| **Bất biến hình học (property-based)** | Ví dụ: kết quả OFFSET cách nguồn đúng `d`; cung FILLET tiếp xúc cả hai đối tượng với bán kính `r`; TRIM không đổi phần còn lại | Mỗi PR |
| **Round-trip và đối chứng chéo DWG** | §6.3 | Mỗi đêm |
| **So sánh ảnh khi in** | So với PDF do DWG TrueView (miễn phí) in từ cùng tệp: SSIM + bản đồ chênh lệch + so vector | Mỗi đêm |
| **Bộ LISP** | Hơn 500 LISP, so kết quả DWG và output | Mỗi đêm |
| **Fuzzing** | Codec, parser LISP/DCL/PGP/CUI/PAT/LIN/SHX, MTEXT | Liên tục |
| **Hiệu năng** | Benchmark §1.1 trên máy GPU thật; chặn merge nếu chậm hơn ngưỡng | Mỗi PR (rút gọn), mỗi đêm (đầy đủ) |
| **Phân tích tĩnh, sanitizer** | clang-tidy, ASan/UBSan/TSan, cảnh báo là lỗi | Mỗi PR |
| **Kho bản vẽ VN** | ≥ 10.000 tệp có thỏa thuận cho phép sử dụng; nút "Gửi bản vẽ lỗi" trong Viewer để kho tự lớn lên | Liên tục |

## 15. Pháp lý kỹ thuật (cần luật sư sở hữu trí tuệ xác nhận)

| Chủ đề | Quy tắc |
|---|---|
| Tự viết codec DWG | Dựa trên đặc tả công khai và thực nghiệm, phục vụ mục đích **tương tác** (interoperability). Không dùng mã hay SDK của Autodesk, không dịch ngược AutoCAD. Đọc kỹ điều khoản của tài liệu đặc tả ODA (cung cấp "AS IS"). Lưu nhật ký nguồn tham khảo |
| LibreDWG (GPL) | Chỉ chạy như công cụ đối chứng riêng biệt trong CI. Không đọc, không chép, không link mã |
| ACadSharp (MIT) | Được tham khảo. Nếu dùng lại đoạn mã nào thì phải giữ thông báo bản quyền MIT |
| TrustedDWG | AutoCAD sẽ hiện thông báo "không phải ứng dụng Autodesk" khi mở tệp TrionCAD lưu. Chuẩn bị FAQ. Không giả mạo watermark (vụ Autodesk–ODA năm 2007) |
| Nhãn hiệu "DWG" | USPTO từ chối đăng ký "DWG" vì chỉ mô tả, nhưng theo các dàn xếp năm 2010, SolidWorks và ODA đã ngừng dùng "DWG" trong thương hiệu. Chỉ dùng "DWG" để mô tả định dạng |
| Tên lệnh, hàm LISP | Được tái hiện như thông lệ ngành (BricsCAD, ZWCAD, IntelliCAD; Oracle v. Google 2021 ở Mỹ). Không chép tài liệu trợ giúp, biểu tượng, đồ họa giao diện |
| Font SHX, `acad.pat`, `acad.lin` của Autodesk | Tự thiết kế font và tự viết tệp mẫu (giữ tên chuẩn, định nghĩa riêng) |
| FAS/VLX | Chỉ làm khi luật sư xác nhận ngoại lệ tương tác áp dụng được |
| Qt LGPL | Link động; cho phép người dùng thay thư viện Qt; kèm văn bản giấy phép; không sửa Qt, hoặc nếu sửa thì công bố phần sửa |
| SBOM | Liệt kê mọi phụ thuộc và giấy phép trong mỗi bản phát hành |

## 16. Bảo mật

- Ký số tệp cài đặt và bản cập nhật. Cập nhật kiểm tra chữ ký Ed25519 trước khi áp dụng.
- `SECURELOAD` mặc định bật. Cảnh báo khi nạp mã nằm ngoài trusted paths.
- Codec chạy với đầu vào không tin cậy, nên cần fuzzing liên tục, kiểm tra biên mọi chỗ đọc bit và giới hạn kích thước cấp phát.
- Telemetry chỉ khi người dùng đồng ý. Không gửi nội dung bản vẽ. Đường dẫn tệp được băm.
- AI (P3): mặc định chỉ đọc; thao tác ghi cần người dùng xác nhận và có mark undo.

## 17. DevOps

- **Kho mã:** monorepo (`core/`, `render/`, `app/`, `lisp/`, `plot/`, `vn/`, `sdk/`, `console/`, `qa/`, `packaging/`, `docs/specs/`).
- **Build:** CMake; vcpkg ghim phiên bản cho các thư viện hạ tầng ở §2; sccache. Release bật LTO + PGO.
- **CI:**
  - GitHub Actions cho phần nhẹ;
  - **runner tự host** (máy của công ty, có GPU thật kể cả máy Intel cũ) cho benchmark, so sánh ảnh và kho bản vẽ, không tốn phí thuê;
  - đầy đủ Windows, macOS, Linux.
- **Kênh phát hành:** nội bộ (mỗi ngày) → beta (2 tuần/lần) → ổn định (mỗi quý). Feature flag cho tính năng chưa xong.
- **Phiên bản:** đặt theo năm (TrionCAD 2028…). Quyết định ghi thành ADR trong `docs/adr/`. Code review bắt buộc. Trong code, ghi rõ chỗ nào cố ý làm đơn giản và giới hạn của nó.

## 18. Đám mây và web (sau v1.0)

- **Hạ tầng tự vận hành,** máy chủ đặt tại VN (Luật Bảo vệ dữ liệu cá nhân, Nghị định 53/2022 **[rà soát pháp lý]**):
  - dịch vụ tài khoản;
  - lưu trữ tệp và phiên bản (object storage S3-compatible, tự host);
  - chia sẻ link (kể cả qua Zalo), ghi chú;
  - worker chuyển đổi chạy `trion-console`.
- **Trình xem web:** biên dịch lõi (codec + regen + renderer) sang WebAssembly (Emscripten), dùng Qt for WebAssembly hoặc WebGL2 trực tiếp **[spike khi đến lúc]**.
- **Cộng tác:** lịch sử phiên bản → khóa ở mức đối tượng → chỉnh sửa đồng thời dựa trên nhật ký thao tác theo handle. Undo journal (§5.2) đã là nền tảng cho bước này.

## 19. AI (sau v1.0)

- **Trước tiên là các tính năng tất định, tự viết:**
  - kiểm tra TCVN theo bộ quy tắc;
  - Smart Match: chữ ký hình học bất biến khi xoay/đổi tỉ lệ, dùng tìm đối tượng tương tự, chuyển thành block, đếm;
  - Geometry Cleanup;
  - Bác sĩ Font.
- **Trợ lý ngôn ngữ tự nhiên:** MCP server trong ứng dụng, các công cụ đọc/ghi có xác nhận. Mô hình ngôn ngữ lớn là dịch vụ tính phí theo lượt dùng, nên tính vào giá gói AI, không phải tài nguyên mua trước.

## 20. Ước lượng khối lượng đến v1.0 (năm-người·tháng, PM)

| Hạng mục | PM |
|---|---|
| Khung UI Qt (cửa sổ, docking, ribbon, dòng lệnh, thanh trạng thái, palette, chủ đề, i18n) | 18 |
| Mô hình bản vẽ, undo journal, sự kiện, handle, owner/dictionary/xdata | 20 |
| Codec DWG: **đọc** R13–2018 (gồm R2007), proxy, giữ dữ liệu thô, recover | 48 |
| Codec DWG: **ghi** AC1032 + AC1018 (+AC1015/1021/1024/1027) | 30 |
| DXF đọc/ghi mọi phiên bản; hạ tầng schema và codegen | 8 |
| Kernel hình học 2D (§7) | 40 |
| Regen text: SHX, TrueType, MTEXT, field | 20 |
| Regen kích thước (mọi loại, định dạng số chính xác) | 26 |
| Regen hatch (mẫu, biên cong, island, gradient) | 12 |
| Regen linetype, leader/mleader, bảng, wipeout, ảnh, proxy graphics, viewport | 18 |
| Dynamic block (EvalGraph cho parameter và action cơ bản, grip) | 14 |
| Renderer QRhi (batch, instancing, BVH, LOD, shader giải tích, RTC, overlay, viewport, MSAA, GPU compute, quản lý VRAM, tối ưu) | 44 |
| Hạ tầng tận dụng phần cứng: bộ lập lịch work-stealing, lớp SIMD + chọn đường chạy, tự thích ứng, `TRIONBENCH`, ma trận benchmark | 10 |
| Chỉ mục không gian, chọn, truy vấn OSNAP | 8 |
| Command framework, input pipeline, prompt, script, alias, sysvar | 14 |
| Hỗ trợ chính xác (OSNAP đủ chế độ, polar/otrack, ortho, grid/snap, DYN, UCS) | 14 |
| Chọn đối tượng, grip, palette thuộc tính, quản lý layer và style, page setup | 26 |
| Khoảng 120 lệnh vẽ/sửa theo đặc tả hành vi | 40 |
| UI soạn text/dim/mleader/bảng (gồm trình soạn MTEXT) | 16 |
| Block, attribute, xref, ảnh, PDF underlay (UI và lệnh) | 14 |
| Plot engine (CTB/STB), **bộ ghi PDF vector tự viết**, in qua `QPrinter`, publish | 18 |
| Nhập PDF và underlay (đọc qua PDFium, chuyển thành entity) | 8 |
| LISP: lõi (18), DCL (5), `vl`/`vlax`/`vlr` (7), tập con `vla` (10) | 40 |
| Gói VN (Bác sĩ Font, nội dung TCVN, công cụ VN tích hợp) | 8 |
| Nền tảng: giấy phép (Ed25519, máy chủ kích hoạt, máy chủ floating), cập nhật, crash, trình cài đặt | 10 |
| Hạ tầng QA (§14) | 16 |
| Riêng cho macOS và Linux | 8 |
| Thiết kế UX | 14 |
| **Tổng** | **≈ 562 PM (~47 năm-người); dải bất định 460–670 PM** |

→ Với đội 18–20 kỹ sư ổn định từ khoảng tháng 13, **v1.0 rơi vào khoảng tháng 36 (±4)**. Chi tiết theo mốc ở [06](06-lo-trinh-trien-khai.md). Ước lượng này phải hiệu chỉnh lại sau các spike ở Giai đoạn 0.

## 21. Spike kỹ thuật ở Giai đoạn 0 (tuần 2–11)

| Spike | Câu hỏi | Tiêu chí đạt |
|---|---|---|
| **S1. Codec DWG** | Viết bộ đọc nguyên mẫu (header, section, giải nén R2004, object map, 20 loại entity lõi) cho R2000, R2004, R2018 | Đọc ≥ 90% kho 1.000 DWG ở mức entity lõi; thời gian mở bằng hoặc nhanh hơn DWG TrueView |
| **S2. Renderer** | QRhi (D3D11/Metal/Vulkan) với 1 triệu đoạn thẳng, cung giải tích, lineweight, linetype | ≥ 60 fps trên GPU tích hợp tầm trung; ≥ 30 fps trên Intel HD 520/620 |
| **S3. Kernel hình học** | Giao điểm và offset có cung; DCEL với cạnh cong cho biên hatch | Vượt 1 triệu ca kiểm thử ngẫu nhiên và suy biến, không sai tô-pô |
| **S4. Bác sĩ Font** | Nhận diện và chuyển mã (C++ và LISP) | ≥ 95% trên 1.000 mẫu có nhãn |
| **S5. LISP** | Evaluator máy ngăn xếp, 30 hàm lõi, cầu nối `(command)` có `pause` | Chạy được 10 LISP cộng đồng; ước lượng PM có căn cứ |
| **S6. IME tiếng Việt và text** | Unikey/EVKey trên Windows, bộ gõ macOS, ibus trên Linux trong dòng lệnh và trình soạn MTEXT; render SHX/TTF tiếng Việt | Gõ và hiển thị đúng 100% bộ ký tự tiếng Việt |

Kết quả spike là đầu vào cho **cổng G0** ([06](06-lo-trinh-trien-khai.md)).

## Phụ lục — Phương án dự phòng có trả phí (chỉ để tham khảo, không dùng)

Nếu sau này nhà sáng lập muốn rút ngắn thời gian bằng tiền:
- **ODA Drawings SDK:** Commercial 3.000 USD năm đầu, 2.250 USD các năm sau (tối đa 100 bản); Sustaining 7.500 USD → 4.500 USD/năm; Founding 37.500 USD → 18.000 USD/năm, có mã nguồn. **Ngừng trả phí thì mất quyền phân phối.**
- **IntelliCAD (ITC):** 38.000 USD/năm + 25 USD/bản, hoặc 75.000 USD/năm.

Nguồn: [opendesign.com/pricing](https://www.opendesign.com/pricing.md), [intellicad.org/membership-fees](https://www.intellicad.org/membership-fees). Cả hai **không nằm trong kế hoạch**.
