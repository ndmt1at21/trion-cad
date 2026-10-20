# Lõi · GEO — Kernel hình học 2D

> Phạm vi: kiểu cơ bản, dung sai, predicate bền vững, hàm sơ cấp tất định, đường cong (line, cung, tròn, ellipse, polyline, NURBS), giao điểm, phân loại vùng, BVH, biến đổi, tessellation, dựng hình (§1–§7). Offset, DCEL, vùng kín, boolean, bo góc, cắt/kéo dài, diện tích, spline fit, ràng buộc, chữ ký hình học ở §8 trở đi.
> Thư mục mã: `core/geom/`. Căn cứ: [05 §7](../05-kien-truc-ky-thuat.md), 05 §1.1–§1.2, ADR-004 (05 §4), 05 §9.3–§9.4, 05 §14, 05 §21 (S3), [03 §3–§4](../03-chuc-nang-autocad.md).

## 0. Ghi chú thiết kế

- **Thư mục [đề xuất]:** `core/geom/base` (kiểu, dung sai), `robust` (predicate, hàm sơ cấp, nghiệm), `curve`, `nurbs`, `intersect`, `bvh`, `xform`, `tess`, `construct`; kiểm thử `core/geom/tests/<nhóm>/`, benchmark `core/geom/bench/`, fuzz `core/geom/fuzz/`. GEO không phụ thuộc `core/doc`: đầu vào là dữ liệu thuần, DOC/RGN/CMD tự ánh xạ entity sang đường cong.
- **Số học (ADR-004):** IEEE-754 `double`, cấm fast-math, `-ffp-contract=off`, không dùng FMA kể cả tường minh [đề xuất]. Mọi hàm lượng giác và sơ cấp trong `core/geom` đi qua GEO-004; clang-tidy chặn `std::sin`, `std::atan2`… trong thư mục này [đề xuất].
- **Dung sai và tô-pô:** so sánh số thực qua `Tolerance` (GEO-002), truyền tường minh, mỗi bản vẽ một giá trị bất biến, không có biến toàn cục đổi được [đề xuất]. Quyết định tô-pô (bên nào, trong/ngoài, thứ tự) dùng predicate chính xác GEO-003, không dựa trên phép trừ thô (05 §7).
- **Tất định:** kết quả sắp theo khóa ổn định (tham số, tọa độ, chỉ số đầu vào). Song song qua NT-005 với phân hoạch cố định, ghép theo chỉ số. Đường SIMD cho cùng bit với scalar. GEO-109 so hash trên ma trận hệ điều hành × trình biên dịch × ISA × số luồng.
- **Tọa độ lớn:** VN-2000 ≈ 10⁹ đơn vị. Tính bằng `double`; biến đổi quanh điểm gốc theo thứ tự trừ gốc trước. Sai số kết quả ≤ 1e-9 × kích thước bản vẽ, tô-pô trùng (05 §1.2). Mọi bộ bất biến chạy thêm biến thể dịch gốc ≈ 10⁹ (GEO-109).
- **Tham số hóa giống AutoCAD** để `vlax-curve-*`, DIVIDE, MEASURE, LENGTHEN khớp: line theo khoảng cách từ điểm đầu, cung/tròn theo góc (rad), ellipse theo góc lệch tâm, polyline tham số i tại đỉnh i, spline theo nút. Đường cong là kiểu giá trị; `CurveRef` là tagged union, không hàm ảo.
- **Mặt phẳng:** kernel 2D. Pháp tuyến OCS −Z xử lý như phép đối xứng; OCS khác ±Z và đa tuyến 3D được chiếu về mặt phẳng XY qua GEO-022 trước khi vào kernel.
- **Đầu vào hỏng:** dữ liệu từ DWG có thể có NaN/Inf, bán kính 0, nút giảm, bậc vô lý. Hàm trả lỗi có kiểu hoặc kết quả rỗng, mọi vòng lặp có chặn số lần, không UB. Bộ chuyển đổi đọc dữ liệu tệp có fuzz (DoD-C 7).
- **Ngân sách nguyên hàm [đề xuất]:** line/cung/tròn ≤ 0,1 µs, ellipse ≤ 1 µs, spline ≤ 50 µs mỗi phép (p99, máy tầm trung). Lệnh cơ bản gọi ≤ 10⁴ phép vẫn ≤ 10 ms, còn nửa ngân sách 20 ms (05 §1.1) cho DOC và regen. Phần GEO của truy vấn OSNAP/hit test ≤ 0,3 ms trong 1 ms (DoD-L).
- **Kiểm thử (05 §14) và ranh giới:** property-based qua QA-009, ≥ 1 triệu ca mỗi nhóm thuật toán, có ca suy biến (đồng tuyến, tiếp xúc, trùng điểm, cung rất nhỏ/rất lớn); ca vàng; benchmark trên NT-003. Áp biến đổi vào entity trong transaction thuộc DOC; chỉ mục BVH trên bản vẽ thuộc DOC-009/INT-009, culling thuộc RND-010 (README §4.2).

## 1. Nền tảng số học

### GEO-001 · Kiểu cơ bản: điểm/vector 2D-3D, ma trận affine, hộp bao, mảng điểm SoA
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-001, NT-006, NT-007
- **Đầu vào:** 05 §7, §9.3, §9.4; API SIMD của NT-006, arena của NT-007.
- **Đầu ra:**
  - `core/geom/base/{point,vector,matrix,box,point_array}.h`: `Point2/Point3` tách khỏi `Vec2/Vec3` (điểm − điểm = vector), `Matrix2` (affine 3×3), `Matrix3` (affine 4×4), `Box2/Box3` (rỗng, hợp, giao, nở, hộp của ảnh biến đổi).
  - `PointArray2` SoA căn 64 byte, đệm theo độ rộng SIMD; biến đổi và hộp bao hàng loạt gọi NT-006, có đường scalar.
  - Kiểm thử `core/geom/tests/base/`, benchmark `core/geom/bench/base_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. `M·M⁻¹` lệch ma trận đơn vị ≤ 1e-9 trên 10⁶ ma trận có số điều kiện ≤ 10⁶; ma trận suy biến hoặc không hữu hạn trả `nullopt`.
  2. Hộp của ảnh biến đổi chứa ảnh mọi điểm trong hộp nguồn (10⁶ ca, có tọa độ 10⁹).
  3. Biến đổi 100.000 điểm ≤ 40 µs với AVX2 (máy tầm trung) [đề xuất]; SIMD và scalar trùng từng bit.
  4. `static_assert` kích thước và căn lề; không cấp phát trong vòng lặp (NT-004).
  5. Đạt DoD-C, DoD-H.

### GEO-002 · Chính sách dung sai `Tolerance` và API so sánh
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** GEO-001
- **Đầu vào:** 05 §7 (chính sách dung sai, tọa độ lớn), 05 §1.2.
- **Đầu ra:**
  - `core/geom/base/tolerance.h`: `Tolerance{equalPoint, equalVector, angle}`; `Tolerance::forExtent(S)` với S = max(|tọa độ|, kích thước bản vẽ): `equalPoint = max(1e-10, 1e-12·S)`, `equalVector` = 1e-10, góc 1e-10 rad [đề xuất].
  - Hàm `isEqual`, `isZero`, `isParallel`, `isPerpendicular`, `isZeroLength`, `paramTol(tỉ lệ đường cong)`.
  - Bật `-Wfloat-equal` cho `core/geom` (so sánh `==` trên số thực là lỗi biên dịch).
- **Tiêu chí nghiệm thu:**
  1. Quan hệ bằng phản xạ, đối xứng; `forExtent` đơn điệu theo S (kiểm 10⁶ ca).
  2. S = 10⁹: hai điểm lệch ≤ 10³ ulp được coi là trùng, lệch ≥ 1e-2 thì khác.
  3. S ≤ 10²: giữ giá trị mặc định của AutoCAD (1e-10) [đề xuất].
  4. `Tolerance` là giá trị bất biến, dùng an toàn từ nhiều luồng (TSan sạch).
  5. Đạt DoD-C.

### GEO-003 · Predicate bền vững orient2d, incircle với dự phòng số học chính xác
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** GEO-002, NT-003
- **Đầu vào:** 05 §7 (predicate bền vững); bài báo Shewchuk "Adaptive Precision Floating-Point Arithmetic and Fast Robust Geometric Predicates".
- **Đầu ra:**
  - `core/geom/robust/predicates.h`: `orient2d`, `incircle`, `sideOfLine(base, dir, p)`, `inCircleCR(p, c, r)` (dấu của |p−c|² − r²), `compareDistance(p, a, b)`.
  - Số học expansion (two-sum, two-product tách Dekker, không FMA); phân loại có dung sai `classifySide(p, a, b, tol)`: ON khi khoảng cách ≤ tol, còn lại lấy dấu từ predicate chính xác.
  - Bộ đánh giá chính xác bằng số nguyên lớn (chỉ dùng trong kiểm thử), benchmark.
- **Tiêu chí nghiệm thu:**
  1. Trùng 100% với bộ đánh giá chính xác trên 10⁷ ca ngẫu nhiên và 10⁶ ca suy biến (gần thẳng hàng/đồng viên lệch 1 ulp, tọa độ đến 10⁹, trùng điểm).
  2. orient2d trung bình ≤ 3 ns, incircle ≤ 10 ns với đầu vào ngẫu nhiên; ca suy biến p99 ≤ 0,3 µs và ≤ 2 µs [đề xuất].
  3. Hoán vị vòng đối số cho cùng dấu; NaN/Inf trả mã lỗi, không UB.
  4. Đạt DoD-C.

### GEO-004 · Thư viện hàm lượng giác và hàm sơ cấp tất định
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** NT-003, NT-006
- **Đầu vào:** ADR-004 (05 §4); tài liệu công khai về khử đối số Cody–Waite, Payne–Hanek và đa thức minimax ("Handbook of Floating-Point Arithmetic").
- **Đầu ra:**
  - `core/geom/robust/detmath.h`: `sin`, `cos`, `sincos`, `tan`, `atan`, `atan2`, `asin`, `acos`, `hypot`, `cbrt`, chỉ dùng + − × ÷ và `sqrt` IEEE.
  - Bản lô `sincosBatch`, `atan2Batch` trên NT-006, cùng bit với scalar.
  - Bảng tham chiếu sinh ngoài bằng mpmath ở `core/geom/tests/data/detmath/`; benchmark.
- **Tiêu chí nghiệm thu:**
  1. Sai số ≤ 1 ulp so với bảng tham chiếu trên 10⁸ đối số (gần bội π/2, |x| đến 10⁹ rad, subnormal, ±0, Inf, NaN).
  2. Hash kết quả trùng nhau trên Windows/macOS/Linux, MSVC/Clang, x86-64/ARM64, scalar/SSE4.2/AVX2/AVX-512/NEON.
  3. `sincos` scalar ≤ 15 ns; `sincosBatch` AVX2 ≥ 300 triệu giá trị/giây/luồng (máy tầm trung) [đề xuất].
  4. sin, atan lẻ chính xác và đơn điệu trên miền chính (10⁷ cặp liền kề); atan2 đúng góc phần tư và ±0.
  5. Đạt DoD-C, DoD-H.

### GEO-101 · Bộ giải nghiệm đa thức bền vững: bậc ≤ 4 và dạng Bernstein
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-002
- **Đầu vào:** 05 §7 (giao ellipse, spline: cô lập nghiệm có bảo đảm); tài liệu công khai về quy tắc dấu Descartes trên hệ số Bernstein và chia de Casteljau.
- **Đầu ra:** `core/geom/robust/roots.h`: `solvePoly(hệ số bậc ≤ 4, [lo, hi])`, `bernsteinRoots(hệ số, [0, 1])` bậc tùy ý; cô lập bằng Descartes + chia đôi, đánh bóng Newton có khoảng chặn, báo bội số. Dùng cho GEO-008, GEO-106, GEO-110. Kiểm thử, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Không sót nghiệm trên 10⁶ đa thức dựng từ nghiệm biết trước (cụm cách 1e-8, nghiệm kép, bội 3, hệ số chênh 10¹²).
  2. Nghiệm đơn cách nghiệm khác ≥ 1e-6: sai số ≤ 4 ulp; nghiệm bội báo đúng bội, sai số ≤ 1e-7 tương đối [đề xuất].
  3. Bậc 4 ≤ 0,3 µs; Bernstein bậc 7 ≤ 2 µs, bậc 21 ≤ 15 µs (p99) [đề xuất].
  4. Số vòng lặp có chặn; hệ số NaN hoặc toàn 0 trả lỗi có kiểu.
  5. Đạt DoD-C.

## 2. BVH 2D

### GEO-102 · BVH: dựng song song SAH binning, nút 64 byte
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** GEO-001, NT-005, NT-006
- **Đầu vào:** 05 §9.4 (nút 64 byte, duyệt không rẽ nhánh), 05 §1.1 (hiệu suất song song dựng BVH); bài báo Wald "On fast Construction of SAH-based Bounding Volume Hierarchies".
- **Đầu ra:**
  - `core/geom/bvh/bvh_build.{h,cpp}`: dựng từ mảng (`Box2`, id) bằng SAH 16 bin [đề xuất], song song theo nhánh qua NT-005; lá ≤ 4 phần tử.
  - Nút nhánh 4 đúng 64 byte, hộp con nén tương đối (float theo gốc cây hoặc int16 theo hộp nút, chốt bằng benchmark), làm tròn ra ngoài; gốc cây lưu `double` [đề xuất]. Phần tử vô hạn (XLINE, RAY) ở danh sách riêng.
  - Benchmark `core/geom/bench/bvh_build_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Bảo thủ: hộp nén luôn chứa hộp gốc của phần tử (10⁷ hộp, gồm tọa độ 10⁹, hộp suy biến điểm/đoạn).
  2. Cây giống hệt từng byte với 1, 2, 8, 32 luồng.
  3. 1 triệu hộp ≤ 40 ms (8 luồng, máy tầm trung), 10 triệu ≤ 400 ms (máy mạnh) [đề xuất]; hiệu suất song song ≥ 70% đến 16 luồng (05 §1.1).
  4. Chi phí SAH ≤ 1,1 × bản dựng quét đầy đủ tuần tự; bộ nhớ ≤ 32 byte/phần tử [đề xuất].
  5. Đạt DoD-C, DoD-H.

### GEO-103 · BVH: cập nhật gia tăng (refit, chèn, xóa, dựng lại cục bộ)
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-102
- **Đầu vào:** API dựng của GEO-102; nhu cầu chỉ mục gia tăng của DOC-009 và INT-009 (README §4.2).
- **Đầu ra:** `core/geom/bvh/bvh_update.{h,cpp}`: `refit(ids)`, `insert`, `remove` (lá còn chỗ trống, tách lá), dựng lại cây con khi chi phí SAH vượt 1,3 × lúc dựng [đề xuất]; dựng lại toàn bộ ở nền qua NT-005, hủy được. Kiểm thử, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Sau 10⁶ thao tác ngẫu nhiên (chèn, xóa, di chuyển), truy vấn khớp 100% với quét vét cạn.
  2. Cùng chuỗi thao tác cho cây giống hệt từng byte.
  3. Di chuyển 1.000 phần tử trong cây 1 triệu: refit ≤ 0,5 ms; chèn/xóa một phần tử ≤ 2 µs khấu hao [đề xuất].
  4. Sau 10⁵ sửa đổi ngẫu nhiên, truy vấn chậm hơn cây dựng mới ≤ 20% [đề xuất]; dựng lại nền hủy trong ≤ 50 ms.
  5. Đạt DoD-C, DoD-H.

### GEO-017 · BVH 2D: dựng song song (SAH binning), nút 64 byte, duyệt không rẽ nhánh, cập nhật gia tăng
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-102, GEO-103, QA-012
- **Đầu vào:** 05 §9.4, 05 §1.1, DoD-L; GEO-102 (dựng), GEO-103 (cập nhật); cảnh BENCH-STD 1 triệu/10 triệu entity và VN-2000 (QA-012).
- **Đầu ra:**
  - `core/geom/bvh/bvh.h` (API công khai `Bvh2`): duyệt không đệ quy, ngăn xếp cố định, kiểm 4 hộp con trong một lệnh SIMD (NT-006).
  - Truy vấn: chồng hộp, điểm + bán kính (aperture), gần nhất trước (theo khoảng cách tới hộp, dừng sớm), lô truy vấn song song.
  - Benchmark `core/geom/bench/bvh_query_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Không sót kết quả so với quét vét cạn trên 10⁶ truy vấn ngẫu nhiên, gồm tọa độ VN-2000 và phần tử vô hạn.
  2. Aperture trên 1 triệu entity ≤ 20 µs p99; cửa sổ trả 10.000 id ≤ 0,3 ms; culling cảnh 10 triệu ≤ 2 ms/khung (máy mạnh) [đề xuất].
  3. Thứ tự kết quả tất định; lô song song cho cùng kết quả với 1–32 luồng.
  4. Không cấp phát khi truy vấn (NT-004).
  5. Đạt DoD-C, DoD-H, DoD-L.

## 3. Đường cong cơ bản và polyline

### GEO-104 · Line, ray, xline, đường tròn, cung: tham số hóa, đánh giá, NEA/PER/TAN
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-002, GEO-004, QA-009
- **Đầu vào:** 05 §7; 03 §3 (END, MID, CEN, QUA, PER, TAN, NEA); "DXF Reference" của Autodesk (LINE, RAY, XLINE, CIRCLE, ARC).
- **Đầu ra:**
  - `core/geom/curve/{line,circle_arc}.h`: tham số hóa như AutoCAD; cung có sweep có dấu, chuyển hai chiều với ARC của DWG (luôn ngược chiều kim đồng hồ, pháp tuyến −Z).
  - Đánh giá, đạo hàm bậc 1–2, độ cong, chia, đảo chiều, hộp bao chặt.
  - Điểm gần nhất, mọi chân vuông góc, tiếp điểm từ điểm ngoài, điểm góc phần tư theo góc UCS do bên gọi truyền.
- **Tiêu chí nghiệm thu:**
  1. Property-based (QA-009) 10⁶ ca mỗi phép: `eval(paramAt(p))` = p trong dung sai; điểm gần nhất không xa hơn điểm tốt nhất trong 10⁴ mẫu; chân vuông góc và tiếp điểm thỏa điều kiện vuông góc trong dung sai.
  2. Ca suy biến: sweep 1e-8 rad, bán kính 1e-6 và 1e9, điểm trùng tâm, điểm nằm trên đường tròn (TAN trả chính điểm đó), line dài 0.
  3. Đánh giá ≤ 5 ns, điểm gần nhất ≤ 20 ns, tiếp điểm ≤ 50 ns (p99) [đề xuất].
  4. Đạt DoD-C.

### GEO-105 · Ellipse và cung ellipse: tham số hóa theo DWG, đánh giá, hộp bao, tiếp điểm
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-002, GEO-004, QA-009
- **Đầu vào:** "DXF Reference" (ELLIPSE: tâm, vector trục chính, tỉ lệ trục, tham số đầu/cuối); "Đặc tả DWG của ODA v5.4.1" (ELLIPSE).
- **Đầu ra:** `core/geom/curve/ellipse.h`: tâm + trục chính + tỉ lệ k (0 < k ≤ 1) + khoảng tham số; đổi góc ↔ tham số (góc lệch tâm) như lệnh ELLIPSE; đánh giá, đạo hàm, độ cong, chia, đảo chiều; hộp bao chặt giải tích; điểm QUA (đầu trục); tiếp điểm từ điểm ngoài dạng đóng (đường đối cực).
- **Tiêu chí nghiệm thu:**
  1. 10⁶ ca: điểm đánh giá thỏa phương trình ẩn trong dung sai; hộp bao chứa 10⁴ mẫu và chạm cực trị trong dung sai.
  2. Khứ hồi góc → tham số → góc lệch ≤ 1e-12 rad với k ≥ 1e-3 [đề xuất]; tiếp điểm thỏa điều kiện tiếp xúc.
  3. Suy biến: k = 1 (trùng tròn), k = 1e-6, cung qua mốc 2π, pháp tuyến −Z.
  4. Đánh giá ≤ 10 ns, hộp bao ≤ 50 ns, tiếp điểm ≤ 0,1 µs (p99) [đề xuất].
  5. Đạt DoD-C.

### GEO-106 · Điểm gần nhất và chân vuông góc trên ellipse, bền vững với mọi tỉ lệ trục
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-101, GEO-105
- **Đầu vào:** bài viết Eberly "Distance from a Point to an Ellipse, an Ellipsoid, or a Hyperellipsoid"; 03 §3 (NEA, PER trên ellipse); API GEO-105, GEO-101.
- **Đầu ra:** `core/geom/curve/ellipse_query.{h,cpp}`: điểm gần nhất bằng chia đôi có bảo vệ trên hàm đơn điệu (đưa về góc phần tư thứ nhất, xử lý điểm trên trục và gần tâm), Newton tăng tốc; mọi chân vuông góc (≤ 4) qua GEO-101; bản cung ellipse kẹp theo khoảng tham số. Dữ liệu tham chiếu mpmath ở `core/geom/tests/data/ellipse/`.
- **Tiêu chí nghiệm thu:**
  1. 10⁵ ca tham chiếu: sai số ≤ 1e-12 × bán trục lớn, k từ 1e-6 đến 1, điểm trong/ngoài/trên ellipse, gần tâm, trên trục, gần đường túc bế.
  2. 10⁷ ca ngẫu nhiên: luôn hội tụ, kết quả là cực tiểu toàn cục (so với mọi chân vuông góc và hai đầu cung).
  3. Không sót chân vuông góc so với lấy mẫu dày + tinh chỉnh trên 10⁶ ca.
  4. Điểm gần nhất ≤ 0,5 µs, mọi chân vuông góc ≤ 2 µs (p99) [đề xuất]; số vòng lặp có chặn.
  5. Đạt DoD-C.

### GEO-107 · Độ dài cung và tham số theo độ dài cho line, cung, ellipse
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-104, GEO-105
- **Đầu vào:** nhu cầu linetype theo chiều dài cung (RGN-011), DIVIDE, MEASURE, LENGTHEN, OSNAP MID; bài báo Carlson "Numerical computation of real or complex elliptic integrals".
- **Đầu ra:** `core/geom/curve/arclength.{h,cpp}`: `length(t0, t1)`, `paramAtLength(t0, s)` cho line, ray, xline, tròn, cung (dạng đóng), ellipse và cung ellipse (tích phân elliptic loại 2 theo Carlson, chỉ phép toán IEEE và `sqrt`); nghịch đảo bằng Newton có khoảng chặn. Dữ liệu tham chiếu mpmath.
- **Tiêu chí nghiệm thu:**
  1. Độ dài ellipse sai số tương đối ≤ 1e-13 trên 10⁵ ca tham chiếu, k từ 1e-6 đến 1 [đề xuất].
  2. `length` tăng ngặt theo t; `paramAtLength(length(t))` = t trong dung sai trên 10⁶ ca.
  3. Ellipse: độ dài ≤ 1 µs, tham số theo độ dài ≤ 3 µs; line/cung ≤ 20 ns (p99) [đề xuất].
  4. Kết quả trùng từng bit trên 3 hệ điều hành.
  5. Đạt DoD-C.

### GEO-005 · Đường cong cơ bản: line, ray, xline, cung, đường tròn, ellipse, cung ellipse
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-106, GEO-107, DWG-030, QA-001
- **Đầu vào:** phạm vi neo: tham số hóa, đánh giá, đạo hàm, độ dài, điểm gần nhất, chia, đảo chiều, hộp bao; GEO-104…GEO-107; kho 1.000 tệp (QA-001) qua `dwgdump` (DWG-030).
- **Đầu ra:**
  - `core/geom/curve/curve_ref.h`: `CurveRef` (tagged union) với API thống nhất: khoảng tham số, `eval`, `deriv`, `length`, `paramAtLength`, `closestPoint`, `perpendicularFeet`, `tangentPoints`, `split`, `reverse`, `bbox`, điểm OSNAP (END, MID theo độ dài, CEN, QUA).
  - Bộ kiểm thử chung `core/geom/tests/curve/curve_contract_test.cpp`, polyline (GEO-006) và NURBS (GEO-007) dùng lại; bảng ngân sách theo loại trong `core/geom/bench/`.
- **Tiêu chí nghiệm thu:**
  1. Bộ kiểm thử chung đạt 10⁶ ca mỗi phép mỗi loại, gồm biến thể dịch gốc 10⁹.
  2. Mọi LINE, ARC, CIRCLE, ELLIPSE (cả pháp tuyến −Z) trong kho 1.000 tệp chuyển sang `CurveRef` và đánh giá không lỗi, không NaN.
  3. Gọi qua `CurveRef` chậm hơn gọi trực tiếp ≤ 5% [đề xuất]; không cấp phát.
  4. Benchmark mọi phép mọi loại nằm trong cổng NT-003 với ngưỡng ở §0.
  5. Đạt DoD-C.

### GEO-006 · Polyline có bulge (LWPOLYLINE, POLYLINE 2D): đoạn, độ dài, diện tích có dấu, điểm gần nhất, độ rộng đoạn
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** GEO-005, GEO-017
- **Đầu vào:** "DXF Reference" (LWPOLYLINE, POLYLINE/VERTEX: bulge, độ rộng, cờ đỉnh fit và khung spline); API GEO-005, GEO-017.
- **Đầu ra:** `core/geom/curve/polyline.{h,cpp}`, gia nhập `CurveRef`:
  - Đỉnh SoA, bulge, độ rộng đầu/cuối, độ rộng hằng, cờ kín; đoạn thẳng/cung từ bulge (bulge = tan(θ/4)); bỏ đỉnh khung spline, giữ đỉnh fit.
  - Độ dài lũy kế, `paramAtLength`, diện tích có dấu (đa giác + hình viên phân, tính quanh đỉnh đầu), điểm gần nhất, chân vuông góc, tiếp điểm, độ rộng tại tham số.
  - Chỉ mục đoạn bằng GEO-017 khi ≥ 64 đoạn [đề xuất].
- **Tiêu chí nghiệm thu:**
  1. Đạt bộ kiểm thử chung GEO-005 (10⁶ ca mỗi phép), gồm đoạn dài 0, bulge ±10⁶, đỉnh trùng, polyline kín một đỉnh.
  2. Diện tích có dấu lệch tham chiếu mpmath ≤ 1e-12 × S² (S: kích thước polyline), kể cả ở tọa độ 10⁹.
  3. Polyline 100.000 đỉnh: dựng chỉ mục ≤ 2 ms, điểm gần nhất ≤ 5 µs p99, độ dài lũy kế ≤ 0,3 ms [đề xuất].
  4. Mọi LWPOLYLINE/POLYLINE 2D trong kho 1.000 tệp và cảnh VN-2000 (đường đồng mức, QA-012) xử lý không lỗi.
  5. Đạt DoD-C, DoD-H.

## 4. Giao điểm, phân loại vùng, tọa độ lớn

### GEO-008 · Giao điểm giải tích line/cung/tròn/ellipse, phân loại tiếp xúc và trùng đoạn
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** GEO-003, GEO-005, GEO-101
- **Đầu vào:** 05 §7 (giao điểm, phân loại tiếp xúc và trùng đoạn), 05 §21 (S3); API GEO-003, GEO-005, GEO-101.
- **Đầu ra:** `core/geom/intersect/analytic.{h,cpp}`:
  - Mọi cặp trong {line, ray, xline, tròn, cung, ellipse, cung ellipse}; kết quả `{tA, tB, điểm, loại: cắt | tiếp xúc | đầu/cuối đoạn trùng}` sắp theo tA.
  - Trùng đoạn: line cùng phương, cung cùng đường tròn, ellipse trùng nhau.
  - Cờ kéo dài từng phía (cho OSNAP EXT, APP và TRIM/EXTEND theo phần kéo dài); cặp có ellipse giải bậc 4 (GEO-101) rồi Newton 2D.
- **Tiêu chí nghiệm thu:**
  1. 10⁶ ca mỗi cặp: điểm giao nằm trên cả hai đường cong trong 1e-9 × kích thước; không sót giao so với tham chiếu chia nhỏ dày + predicate chính xác trên 10⁵ ca.
  2. Ca gần tiếp xúc trong dung sai trả đúng một điểm tiếp xúc, không dao động 0/2 điểm; ≥ 500 ca vàng tiếp xúc/trùng đoạn, có tọa độ VN-2000 [đề xuất].
  3. `intersect(a, b)` và `intersect(b, a)` cho cùng tập điểm, cùng loại.
  4. Line–line ≤ 20 ns, line–tròn ≤ 40 ns, tròn–tròn ≤ 60 ns, line–ellipse ≤ 0,1 µs, ellipse–ellipse ≤ 3 µs (p99) [đề xuất].
  5. Đạt DoD-C.
- **Ngoài phạm vi:** spline (GEO-009); offset (GEO-011, GEO-012); sắp xếp phẳng (GEO-013).

### GEO-108 · Phân loại vị trí: điểm trong vùng cạnh cong, vị từ Window/Crossing/WPolygon/CPolygon/Fence
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** GEO-003, GEO-006, GEO-008, GEO-017
- **Đầu vào:** 03 §4 (Window, Crossing, Fence, WPolygon, CPolygon; lasso P1); bài báo Hormann–Agathos "The point in polygon problem for arbitrary polygons"; API GEO-003, GEO-006, GEO-008.
- **Đầu ra:** `core/geom/intersect/region_query.{h,cpp}`:
  - `classify(p, vòng)` → trong / ngoài / trên biên: winding number trên vòng kín gồm line, cung, cung ellipse, polyline; cạnh thẳng dùng orient2d, cạnh cung dùng `inCircleCR` + phía dây cung; quy tắc chẵn-lẻ và khác-không; chỉ mục cạnh khi ≥ 64 cạnh.
  - Vị từ đường cong so với vùng chọn: nằm trong (Window, WPolygon, lasso), trong hoặc cắt (Crossing, CPolygon), cắt Fence; kiểm đa giác tự cắt cho prompt của INT-008.
- **Tiêu chí nghiệm thu:**
  1. 10⁷ điểm ngẫu nhiên: trùng 100% tham chiếu chính xác khi điểm cách biên > dung sai; điểm cách biên ≤ dung sai trả "trên biên".
  2. Ca suy biến: tia qua đỉnh, tiếp xúc cung, vòng là một cung 360°, island lồng 3 mức, tọa độ VN-2000.
  3. Vị từ vùng chọn khớp tham chiếu lấy mẫu dày + predicate chính xác trên 10⁶ ca mỗi chế độ.
  4. Điểm trong vòng 10.000 cạnh (có chỉ mục) ≤ 2 µs; line/cung với đa giác 16 đỉnh ≤ 0,2 µs (p99) [đề xuất].
  5. Đạt DoD-C.

### GEO-010 · Bộ điều phối giao điểm cho mọi cặp đường cong và giao hàng loạt (broad-phase + song song)
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** GEO-008, GEO-108, GEO-017, NT-005
- **Đầu vào:** 05 §7, DoD-H; API GEO-006, GEO-008, GEO-108, GEO-017.
- **Đầu ra:** `core/geom/intersect/intersect.h` (API công khai):
  - `intersect(CurveRef a, CurveRef b, opts)` cho mọi cặp, kể cả polyline (theo đoạn, có chỉ mục) và tự giao polyline; bảng đăng ký để GEO-009 thêm spline.
  - Lô một-với-nhiều (cạnh cắt của TRIM, OSNAP INT dưới aperture) và nhiều-với-nhiều (biên hatch, BOUNDARY): broad-phase bằng BVH, narrow-phase song song qua NT-005, ghép theo khóa (i, j, tA).
  - `selectByRegion(chế độ, vùng, ứng viên)` song song trên vị từ GEO-108, dùng cho INT-008.
- **Tiêu chí nghiệm thu:**
  1. Lô trùng từng bit với vòng lặp O(n²) tuần tự trên 10⁴ đường cong ngẫu nhiên, và trùng nhau với 1, 2, 8, 32 luồng.
  2. 100.000 đường cong nhiều-với-nhiều ≤ 150 ms; CPolygon trên 1 triệu ứng viên ≤ 60 ms; tự giao polyline 100.000 đỉnh ≤ 30 ms (8 luồng, máy tầm trung) [đề xuất].
  3. Hiệu suất song song ≥ 60% đến 8 luồng; lô hủy được trong ≤ 50 ms.
  4. Dispatch một cặp tốn thêm ≤ 10 ns so với gọi trực tiếp GEO-008 [đề xuất].
  5. Đạt DoD-C, DoD-H.

### GEO-109 · Bộ bất biến tọa độ lớn VN-2000 và cổng tất định đa nền tảng
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-010, QA-009, QA-012
- **Đầu vào:** 05 §7 (tọa độ lớn, tất định), ADR-004, 05 §21 (S3); cảnh VN-2000 của QA-012; bộ property-based của GEO-001…GEO-010, GEO-101…GEO-108.
- **Đầu ra:** `core/geom/tests/invariants/`: lớp bọc chạy mọi bộ property-based của module hai lần, gốc thường và dịch gốc ≈ (5·10⁸; 2·10⁹) sau khi đổi m → mm; job CI so hash kết quả trên ma trận hệ điều hành × trình biên dịch × ISA (scalar, SSE4.2, AVX2, AVX-512, NEON) × số luồng; ca lỗi kèm seed đã thu nhỏ. Task GEO sau thêm bộ của mình vào lớp bọc này.
- **Tiêu chí nghiệm thu:**
  1. Bất biến khi dịch gốc: loại giao (cắt/tiếp xúc/trùng), kết quả trong/ngoài, số giao điểm giống hệt; vị trí lệch ≤ dung sai GEO-002 của S tương ứng.
  2. Mốc S3 (05 §21): ≥ 1 triệu ca ngẫu nhiên và suy biến không sai tô-pô; job đêm ≥ 10⁷ ca.
  3. Hash trùng nhau trên toàn ma trận; lệch một bit là lỗi chặn merge.
  4. Chế độ PR (10⁵ ca) chạy ≤ 60 s.
  5. Đạt DoD-C.

## 5. NURBS và giao điểm spline

### GEO-110 · NURBS lõi: biểu diễn, đánh giá, đạo hàm, chèn nút, tách Bézier, chia, hộp bao
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** GEO-005, GEO-101
- **Đầu vào:** 05 §7 (NURBS: đánh giá, đạo hàm, chèn nút, chia); sách Piegl–Tiller "The NURBS Book"; API GEO-005, GEO-101.
- **Đầu ra:** `core/geom/nurbs/nurbs.{h,cpp}`:
  - Bậc 1–25 [đề xuất], nút kẹp/không kẹp/tuần hoàn, điểm điều khiển SoA, trọng số tùy chọn; kiểm hợp lệ dữ liệu không tin cậy (NaN, nút giảm, bội > bậc + 1, trọng số ≤ 0).
  - Đánh giá de Boor, đạo hàm hữu tỉ bậc 1–2, chèn nút Boehm, tách thành đoạn Bézier, chia tại tham số, đảo chiều, tuần hoàn → kẹp, hộp bao chặt (nghiệm đạo hàm qua GEO-101).
- **Tiêu chí nghiệm thu:**
  1. Đánh giá khớp tham chiếu mpmath ≤ 1e-12 tương đối trên 10⁵ ca (hữu tỉ, nút bội, bậc 1–25).
  2. Chèn nút, tách Bézier, chia rồi ghép giữ nguyên hình: lệch ≤ 1e-12 × kích thước trên 10⁴ mẫu mỗi ca.
  3. 10⁶ bộ dữ liệu hỏng sinh ngẫu nhiên: trả lỗi có kiểu, không UB.
  4. Bậc 3: đánh giá ≤ 50 ns, đạo hàm ≤ 80 ns; tách Bézier spline 1.000 điểm điều khiển ≤ 50 µs (p99) [đề xuất].
  5. Đạt DoD-C.

### GEO-111 · NURBS truy vấn: độ dài, tham số theo độ dài, điểm gần nhất, chân vuông góc, tiếp điểm
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-017, GEO-101, GEO-110
- **Đầu vào:** 03 §3 (NEA, PER, TAN, MID trên spline); nhu cầu linetype (RGN-011), DIVIDE, MEASURE; API GEO-110, GEO-101, GEO-017.
- **Đầu ra:** `core/geom/nurbs/nurbs_query.{h,cpp}`:
  - Độ dài bằng Gauss–Legendre thích nghi theo đoạn Bézier có ước lượng sai số; `paramAtLength`.
  - Điểm gần nhất: lọc đoạn theo hộp bao (chỉ mục GEO-017 khi ≥ 64 đoạn), nghiệm (C − P)·C′ = 0 dạng Bernstein (GEO-101), tinh chỉnh Newton; mọi chân vuông góc; tiếp điểm từ điểm ngoài ((C − P) × C′ = 0).
- **Tiêu chí nghiệm thu:**
  1. Điểm gần nhất không xa hơn kết quả lấy mẫu dày (10⁴ mẫu/đoạn) + tinh chỉnh trên 10⁶ ca: hữu tỉ, nút bội, điểm nhọn, spline tự cắt.
  2. Không sót chân vuông góc/tiếp điểm so với tham chiếu trên 10⁵ ca.
  3. Độ dài sai số tương đối ≤ 1e-10 [đề xuất]; `paramAtLength(length(t))` = t trong dung sai.
  4. Spline bậc 3, 1.000 đoạn: điểm gần nhất ≤ 20 µs p99, độ dài toàn phần ≤ 0,2 ms [đề xuất].
  5. Đạt DoD-C.

### GEO-112 · Chuyển đổi SPLINE của DWG/DXF ↔ NURBS, nội suy điểm fit
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-110, DWG-030, QA-004, QA-006
- **Đầu vào:** "Đặc tả DWG của ODA v5.4.1" (SPLINE: dạng điểm điều khiển, dạng điểm fit, cờ, cách tham số hóa nút); "DXF Reference" (SPLINE); `dwgdump` (DWG-030) trên kho QA-001; DXF do ODA File Converter xuất (QA-004).
- **Đầu ra:** `core/geom/nurbs/spline_data.{h,cpp}`:
  - Struct dữ liệu thuần mô tả trường SPLINE (không phụ thuộc `core/doc`); chuyển sang NURBS (kín, tuần hoàn, hữu tỉ, tuyến tính) và chiều ngược.
  - Dạng chỉ có điểm fit: nội suy toàn cục với tiếp tuyến đầu/cuối, tham số hóa nút Chord / Square root / Uniform; fit tolerance > 0: xấp xỉ bình phương tối thiểu trong dung sai.
  - Fuzz target `core/geom/fuzz/spline_data_fuzz.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Mọi SPLINE trong kho 10.000 tệp chuyển được hoặc trả lỗi có kiểu; tỉ lệ lỗi ghi vào báo cáo kho.
  2. Spline fit với fit tolerance = 0: điểm điều khiển khớp DXF của ODA File Converter ≤ 1e-9 × kích thước trên ≥ 99% ca [đề xuất].
  3. Khứ hồi NURBS → trường → NURBS trùng từng bit.
  4. Fuzz ≥ 1 giờ không lỗi rồi vào QA-006; spline 10⁶ điểm điều khiển chuyển ≤ 50 ms.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** sửa điểm fit, chuyển polyline ↔ spline, BLEND (GEO-023).

### GEO-007 · NURBS: đánh giá, đạo hàm, chèn nút, chia, độ dài, điểm gần nhất, hộp bao; chuyển đổi với dạng lưu trong DWG
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** GEO-109, GEO-111, GEO-112
- **Đầu vào:** GEO-110, GEO-111, GEO-112; bộ kiểm thử chung GEO-005; lớp bọc tọa độ lớn GEO-109; kho QA-001.
- **Đầu ra:** NURBS gia nhập `CurveRef` (`core/geom/curve/curve_ref.h`), mọi phép của API thống nhất chạy được cho spline; ngưỡng spline trong `core/geom/bench/`; bộ spline thêm vào lớp bọc GEO-109.
- **Tiêu chí nghiệm thu:**
  1. Spline đạt bộ kiểm thử chung GEO-005 (10⁶ ca mỗi phép), cả biến thể dịch gốc 10⁹.
  2. Mọi SPLINE trong kho 10.000 tệp: đánh giá, độ dài, điểm gần nhất, hộp bao chạy không lỗi, không NaN.
  3. Phần GEO của OSNAP trên spline 1.000 đoạn (NEA, PER, TAN, END, MID) ≤ 0,3 ms p99 [đề xuất].
  4. Đạt DoD-C, DoD-L.

### GEO-009 · Giao điểm có spline: chia nhỏ + bao khoảng + Newton, cô lập nghiệm có bảo đảm
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** GEO-007, GEO-010
- **Đầu vào:** 05 §7 (cô lập nghiệm có bảo đảm, không sót giao điểm); bài báo Sederberg–Nishita "Curve intersection using Bézier clipping"; API GEO-007, GEO-010, GEO-108.
- **Đầu ra:** `core/geom/intersect/spline_intersect.{h,cpp}`:
  - Spline với line, tròn, cung, ellipse, polyline, spline và tự giao: tách Bézier, loại cặp bằng bao lồi, chia nhỏ có bao khoảng, Newton 2D với kiểm nghiệm duy nhất (Krawczyk); phát hiện tiếp xúc và trùng đoạn.
  - Đăng ký vào bộ điều phối GEO-010; mở rộng phân loại vùng GEO-108 cho cạnh spline.
- **Tiêu chí nghiệm thu:**
  1. Không sót giao trên 10⁵ ca dựng sẵn giao điểm biết trước (tiếp xúc, hai giao cách nhau 1e-9, spline trùng một phần).
  2. Mỗi nghiệm có chứng nhận duy nhất hoặc mang nhãn tiếp xúc/trùng; không trả nghiệm lặp.
  3. Spline bậc 3 100 đoạn: với line ≤ 20 µs, với spline 100 đoạn ≤ 300 µs (p99) [đề xuất].
  4. Gọi qua lô GEO-010 cho kết quả trùng từng bit với 1–32 luồng.
  5. Đạt DoD-C, DoD-H.

## 6. Biến đổi và tessellation

### GEO-022 · Biến đổi đường cong (kể cả tỉ lệ không đều: tròn → ellipse, cung → cung ellipse; đối xứng)
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-006, GEO-007
- **Đầu vào:** 05 §7 (OCS, đa tuyến 3D chiếu xuống mặt phẳng); `CurveRef` của GEO-006, GEO-007.
- **Đầu ra:** `core/geom/xform/transform.{h,cpp}`: `transform(CurveRef, Matrix2)` → danh sách đường cong; `transformAbout(base, phần tuyến tính)` trừ gốc trước. Quy tắc:
  - Đồng dạng: tròn/cung giữ loại; đối xứng (det < 0) đảo chiều cung về lại ngược chiều kim đồng hồ, đổi dấu bulge.
  - Không đồng dạng hoặc xiên: tròn → ellipse, cung → cung ellipse (trục từ SVD phần tuyến tính, ánh xạ tham số giữ đúng đầu mút), ellipse → ellipse; polyline có cung → line + cung ellipse; độ rộng chỉ giữ khi đồng dạng.
  - NURBS: biến đổi điểm điều khiển; ray/xline: điểm gốc + hướng; `Matrix3` + chiếu XY cho OCS khác ±Z và đa tuyến 3D.
- **Tiêu chí nghiệm thu:**
  1. 10⁶ phép ngẫu nhiên mỗi loại: mẫu kết quả trùng ảnh mẫu nguồn ≤ 1e-9 × kích thước; đầu mút trong dung sai.
  2. Ma trận suy biến (det ≈ 0 theo dung sai) trả lỗi có kiểu; dấu det tính chính xác, đúng 100%.
  3. Xoay hình ở tọa độ 10⁹ quanh điểm gốc 360 lần × 1° trở về vị trí cũ trong dung sai GEO-002; thêm vào lớp bọc GEO-109.
  4. Tròn → ellipse ≤ 0,1 µs; polyline 10.000 đỉnh ≤ 50 µs [đề xuất].
  5. Đạt DoD-C.
- **Ngoài phạm vi:** áp biến đổi vào entity trong transaction (transformBy), chọn entity đích, MIRRTEXT: DOC, CMD.

### GEO-113 · Tessellation SIMD cho cung, tròn, ellipse và đoạn cung polyline
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-004, GEO-006, NT-006
- **Đầu vào:** 05 §7 (sai số dây cung), 05 §9.3 (tessellation SIMD), DoD-H 3; API `sincosBatch` (GEO-004), NT-006.
- **Đầu ra:** `core/geom/tess/conic_tess.{h,cpp}`: số đỉnh từ sai số dây cung e (tròn/cung: n = ⌈θ / (2·acos(1 − e/r))⌉; ellipse: bước tham số từ chặn |C″|); góc đều, `sincosBatch` theo làn SIMD; ghi vào `PointArray2` của arena; đường scalar cùng bit. Benchmark.
- **Tiêu chí nghiệm thu:**
  1. Sai số dây cung ≤ e trên 10⁶ đường cong, bán kính 1e-6 đến 1e9, tọa độ VN-2000.
  2. Số đỉnh ≤ 1,05 × tối thiểu lý thuyết với tròn/cung [đề xuất].
  3. SIMD và scalar trùng từng bit trên SSE4.2, AVX2, AVX-512, NEON (10⁶ đường cong).
  4. ≥ 100 triệu đỉnh/giây/luồng với AVX2 (máy tầm trung) [đề xuất]; không cấp phát trong vòng lặp.
  5. Đạt DoD-C, DoD-H.

### GEO-016 · Tessellation có sai số dây cung theo pixel cho cung, ellipse, spline (SIMD)
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-007, GEO-113
- **Đầu vào:** 05 §7 (cung/tròn ưu tiên vẽ giải tích trong shader, RND-005); nhu cầu linetype và hatch (RGN-011, RGN-012), in (PLT-003), mật độ theo phần cứng (RND-018).
- **Đầu ra:** `core/geom/tess/tessellate.h` (API công khai):
  - `tessellate(CurveRef, chordTol, clipBox)` cho mọi loại; `chordTol` = số pixel × đơn vị/pixel (mặc định 0,5 px [đề xuất]).
  - Spline: chia thích nghi theo đoạn Bézier với chặn đạo hàm bậc 2; polyline giữ đỉnh gốc; chỉ sinh phần tham số nằm trong `clipBox`; trần 65.536 đỉnh mỗi đường cong [đề xuất]; nhỏ hơn 1 pixel → 1 điểm.
- **Tiêu chí nghiệm thu:**
  1. Sai số dây cung ≤ chordTol trên 10⁶ đường cong mọi loại (đo bằng điểm gần nhất của `CurveRef`); tham số đầu ra tăng đơn điệu.
  2. Zoom sâu: tròn bán kính 10⁶ trên khung 1920 px sinh ≤ 4.096 đỉnh nhờ clip [đề xuất].
  3. 1 triệu cung ở 0,5 px ≤ 40 ms trên 8 luồng; spline bậc 3 100 đoạn ≤ 20 µs (máy tầm trung) [đề xuất].
  4. Cùng bit giữa các ISA và số luồng.
  5. Đạt DoD-C, DoD-H.

## 7. Dựng hình

### GEO-114 · Dựng đường tròn 2P, 3P, TTR, TTT
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-010
- **Đầu vào:** hành vi lệnh CIRCLE của AutoCAD (2P, 3P, TTR, TTT, chọn nghiệm theo điểm pick); đặc tả `docs/specs/commands/CIRCLE.md` của CMD; API GEO-010.
- **Đầu ra:** `core/geom/construct/circle_construct.{h,cpp}`: 2P; 3P (thẳng hàng → lỗi); TTR với line, cung, tròn, đoạn polyline: giao các đường offset ±r, chọn nghiệm gần điểm pick; TTT (Apollonius gồm đường thẳng, đến 8 nghiệm) chọn theo điểm pick; ellipse/spline: Newton bắt đầu từ điểm pick [đề xuất]; không có nghiệm → mã lỗi.
- **Tiêu chí nghiệm thu:**
  1. 10⁵ ca mỗi cách: khoảng cách tâm tới từng đối tượng = r trong dung sai, đúng phía theo điểm pick.
  2. TTT không sót nghiệm so với tham chiếu giải số dày trên 10⁴ cấu hình (đường thẳng song song, tròn lồng nhau, tròn tiếp xúc nhau).
  3. Khi QA-007 có đáp án: khớp 100% bộ tham chiếu CIRCLE.
  4. TTR ≤ 5 µs, TTT ba tròn ≤ 20 µs (p99) [đề xuất].
  5. Đạt DoD-C.

### GEO-115 · 11 cách dựng cung
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** GEO-005
- **Đầu vào:** hành vi lệnh ARC của AutoCAD (3P, SCE, SCA, SCL, SEA, SED, SER, CSE, CSA, CSL, Continue); ANGDIR/ANGBASE do bên gọi quy đổi.
- **Đầu ra:** `core/geom/construct/arc_construct.{h,cpp}`: 11 hàm dựng trả cung dạng DWG (ngược chiều kim đồng hồ) + cờ đảo; góc âm → chiều kim đồng hồ; dây cung âm → cung lớn; bán kính âm (SER) → cung lớn; Continue tiếp xúc line/cung cuối; mã lỗi khi dây > 2R, 3 điểm thẳng hàng, điểm trùng.
- **Tiêu chí nghiệm thu:**
  1. 10⁵ ca ngẫu nhiên mỗi cách: cung kết quả tái tạo đúng ràng buộc đầu vào (điểm đầu/cuối/tâm/góc/dây) trong dung sai.
  2. ≥ 200 ca vàng về dấu và chiều (góc âm, dây âm, bán kính âm, gần 360°) [đề xuất]; khi QA-007 có đáp án: khớp 100% bộ tham chiếu ARC.
  3. Mỗi phép ≤ 0,2 µs p99 [đề xuất].
  4. Đạt DoD-C.

### GEO-116 · Tiếp tuyến chung và đường vuông góc giữa hai đường cong (TAN/PER trì hoãn)
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-009, GEO-106
- **Đầu vào:** 03 §3 (TAN, PER); hành vi TAN/PER trì hoãn khi vẽ LINE trong AutoCAD (TAN–TAN, TAN–PER, PER tính sau khi có điểm thứ hai); API GEO-007, GEO-009.
- **Đầu ra:** `core/geom/construct/tangent_lines.{h,cpp}`: tiếp tuyến chung tròn/cung (≤ 4), tròn–ellipse, ellipse–ellipse, có spline (giải số có cô lập); đường tiếp xúc đường cong này và vuông góc đường cong kia; chân vuông góc trì hoãn; chọn nghiệm gần điểm pick.
- **Tiêu chí nghiệm thu:**
  1. Bất biến trên 10⁵ ca mỗi cặp: đường kết quả tiếp xúc (khoảng cách 0 và hướng song song tiếp tuyến trong dung sai) hoặc vuông góc trong dung sai.
  2. Không sót nghiệm so với tham chiếu lấy mẫu dày trên 10⁴ cấu hình (tròn lồng, tiếp xúc, ellipse dẹt k = 1e-3).
  3. Tròn–tròn ≤ 0,2 µs, ellipse–ellipse ≤ 20 µs, có spline 100 đoạn ≤ 0,2 ms (p99) [đề xuất].
  4. Đạt DoD-C.

### GEO-020 · Dựng hình: đường tròn 2P/3P/TTR/TTT, 11 cách dựng cung, tiếp tuyến, vuông góc, điểm chia đều/theo khoảng
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-006, GEO-007, GEO-114, GEO-115, GEO-116
- **Đầu vào:** hành vi DIVIDE, MEASURE của AutoCAD (điểm bắt đầu, căn block theo tiếp tuyến); GEO-114…GEO-116; `paramAtLength` của GEO-006, GEO-007, GEO-107.
- **Đầu ra:**
  - `core/geom/construct/construct.h` (API công khai gom GEO-114…GEO-116).
  - `divide(curve, n, tham số đầu)` với n từ 2 đến 32.767; `measure(curve, L, tham số đầu, chiều)`; trả điểm và góc tiếp tuyến (cho tùy chọn căn block). Quy tắc chọn điểm bắt đầu theo AutoCAD do CMD quyết định và truyền vào.
- **Tiêu chí nghiệm thu:**
  1. Khoảng cách theo chiều dài cung giữa hai điểm liên tiếp = L trong 1e-9 × kích thước, với mọi loại (spline, polyline có cung, ellipse).
  2. Số điểm và quy tắc đầu/cuối khớp 100% ca vàng trong đặc tả DIVIDE, MEASURE của CMD; khi QA-007 có đáp án: khớp bộ tham chiếu DIVIDE, MEASURE.
  3. Chia 32.767 đoạn: polyline 10.000 đỉnh ≤ 10 ms, spline 1.000 đoạn ≤ 15 ms (máy tầm trung) [đề xuất].
  4. Kết quả trùng từng bit trên 3 hệ điều hành.
  5. Đạt DoD-C.

## 8. Offset đường cong và polyline

### GEO-151 · Xấp xỉ đường cong tham số bằng chuỗi cung có cận sai số
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-002, GEO-005, GEO-006
- **Đầu vào:** 05 §7 (Offset: ellipse/spline xấp xỉ có kiểm soát sai số); API điểm, tiếp tuyến, độ cong của GEO-005; polyline bulge GEO-006.
- **Đầu ra:**
  - `core/geom/approx/biarc_fit.hpp|.cpp`: `fitArcs(const ParamCurve&, double eps) → Polyline2d` (cung và đoạn thẳng, nối G1), trả cận sai số đạt được.
  - Kiểm thử `core/geom/tests/approx/`, benchmark `core/geom/bench/bench_biarc.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Khoảng cách Hausdorff hai phía giữa kết quả và đường cong gốc ≤ `eps` trên 100.000 ca QA-009 (ellipse tỉ lệ trục đến 1:1000, đường cong có điểm uốn).
  2. Liên tục G1: lệch tiếp tuyến tại mỗi mối nối ≤ dung sai góc của GEO-002.
  3. Bậc hội tụ: giảm `eps` 1.000 lần thì số cung tăng ≤ 12 lần **[đề xuất]**.
  4. Ellipse đủ vòng, `eps` = 10⁻⁶ × trục lớn: ≤ 50 µs p99 trên máy tầm trung **[đề xuất]**.
  5. Đạt DoD-C.

### GEO-152 · Offset thô polyline có bulge và nối góc theo OFFSETGAPTYPE
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-002, GEO-006
- **Đầu vào:** 05 §7 (offset thô từng đoạn, cung offset thành cung, tham khảo ý tưởng CavalierContours); 03 §6 (OFFSET); sysvar OFFSETGAPTYPE; GEO-006.
- **Đầu ra:**
  - `core/geom/offset/raw_offset.hpp|.cpp`: `offsetSegment()` (line, cung bulge), `rawOffset(pl, d, gapType) → RawOffset` (mỗi đoạn mang chỉ số đoạn nguồn); cung có bán kính ≤ d về phía tâm thành đoạn suy biến có cờ `collapsed`.
  - Kiểm thử `core/geom/tests/offset/raw/`.
- **Tiêu chí nghiệm thu:**
  1. Mỗi đoạn offset thô cách đoạn nguồn đúng `d ± tol`; cung ra cung đồng tâm, bulge tính lại đúng.
  2. Góc lồi: OFFSETGAPTYPE 0 kéo dài hai đoạn tới giao, 1 nối cung tâm tại đỉnh bán kính d, 2 vát; góc lõm để lại giao cắt cho GEO-153.
  3. 1 triệu ca QA-009 (kín/hở, đỉnh trùng, đoạn độ dài 0, |bulge| < 10⁻¹², cung gần đủ vòng, tọa độ 10⁹): 0 NaN, 0 sai chỉ số đoạn nguồn.
  4. 10.000 đỉnh ≤ 0,5 ms, 0 cấp phát trong vòng lặp (NT-004) **[đề xuất]**.
  5. Đạt DoD-C.

### GEO-153 · Tự giao của offset thô và chia lát
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-152, GEO-010
- **Đầu vào:** 05 §7 (cắt tự giao); giao hàng loạt có broad-phase và song song của GEO-010; offset thô GEO-152.
- **Đầu ra:**
  - `core/geom/offset/slices.hpp|.cpp`: `findSelfIntersects(raw)`, `findDualIntersects(raw, dual)` (polyline hở: so với offset phía đối diện), `splitIntoSlices() → vector<Slice>`.
  - Kiểm thử `core/geom/tests/offset/slices/`.
- **Tiêu chí nghiệm thu:**
  1. Không sót giao điểm: so với bộ dò vét cạn O(n²) dùng predicate chính xác GEO-003, 0 sai lệch trên 100.000 ca QA-009.
  2. Giao tại đỉnh, tiếp xúc cung-cung, đoạn trùng một phần: mỗi điểm chia duy nhất (gộp qua `Tolerance`), không lát nào dài < tol.
  3. Hoán vị đầu vào cho cùng tập lát, cùng thứ tự (khóa ổn định: chỉ số đoạn, tham số).
  4. 10.000 đỉnh, 1.000 tự giao ≤ 3 ms trên 8 luồng; hiệu suất ≥ 60% đến 8 luồng **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

### GEO-154 · Lọc lát theo khoảng cách và ghép thành polyline kết quả
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-153, GEO-017
- **Đầu vào:** 05 §7 (lọc theo khoảng cách), 05 §14 (bất biến OFFSET); BVH GEO-017 cho truy vấn khoảng cách tới polyline nguồn; lát GEO-153.
- **Đầu ra:**
  - `core/geom/offset/stitch.hpp|.cpp`: `isValidSlice(slice, source, d)` (polyline hở kiểm thêm với hình tròn bán kính d tại hai đầu mút), `stitchSlices() → vector<Polyline2d>` (kín/hở, nhiều mảnh).
  - Kiểm thử `core/geom/tests/offset/stitch/`.
- **Tiêu chí nghiệm thu:**
  1. Lát giữ lại ⇔ mọi điểm cách nguồn ≥ d − tol: 0 sai trên 1 triệu ca QA-009 so với bộ lọc tham chiếu lấy mẫu 1.000 điểm/lát.
  2. Mỗi vòng kín kết quả có diện tích có dấu cùng dấu với nguồn; các kết quả không cắt nhau; đầu mút ghép khớp trong tol.
  3. Offset vào trong hình quả tạ sinh đúng 2 vòng; d lớn hơn bán kính nội tiếp sinh 0 vòng.
  4. 10.000 đỉnh ≤ 3 ms trên 8 luồng **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

### GEO-012 · Offset polyline có cung: offset thô, nối, cắt tự giao, lọc theo khoảng cách, kín/hở
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-154, QA-009, NT-003
- **Đầu vào:** 05 §7, §14; 03 §6 (OFFSET P0, cắt tự giao); 07.2 UI-E10 (OFFSET: bản offset tạm ở phía con trỏ); GEO-152…154.
- **Đầu ra:**
  - `core/geom/offset/offset_polyline.hpp`: `offsetPolyline(pl, d, opts) → vector<Polyline2d>` (phía theo dấu d, OFFSETGAPTYPE).
  - Kiểm thử bất biến và 300 ca vàng `core/geom/tests/offset/golden/`; benchmark `core/geom/bench/bench_offset_polyline.cpp`.
- **Tiêu chí nghiệm thu:**
  1. OFFSETGAPTYPE = 1: mọi điểm kết quả cách nguồn `d ± tol`; 0 và 2: mọi điểm ≥ d − tol, phần offset của từng đoạn cách đúng `d ± tol`. 0 vi phạm trên 1 triệu ca QA-009 (đồng tuyến, tiếp xúc, cung rất nhỏ/rất lớn, tọa độ 10⁹).
  2. Kết quả không tự giao, không giao nhau (kiểm bằng GEO-010 và GEO-003).
  3. Đa giác lồi, OFFSETGAPTYPE = 1: offset ra rồi vào cùng d trả lại hình gốc trong tol.
  4. p99 máy tầm trung: 1.000 đỉnh ≤ 2 ms (xem trước trong 8 ms/khung), 10.000 đỉnh ≤ 15 ms (OFFSET hoàn tất ≤ 20 ms) **[đề xuất]**.
  5. Khớp 100% ca OFFSET polyline của QA-007 khi có đáp án.
  6. Đạt DoD-C, DoD-H, DoD-L.

### GEO-011 · Offset một đường cong (cung ra cung; ellipse xấp xỉ có cận sai số)
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-012, GEO-151
- **Đầu vào:** 05 §7 (Offset); 03 §6 (OFFSET Through/Erase/Layer/Multiple); GEO-005; xấp xỉ cung GEO-151; offset polyline GEO-012.
- **Đầu ra:**
  - `core/geom/offset/offset_curve.hpp|.cpp`: `offsetCurve(curve, d) → OffsetResult` cho line, ray, xline, cung, tròn (chính xác) và ellipse, cung ellipse (GEO-151 rồi GEO-012); `pickSide(curve, pt)`; `throughDistance(curve, pt)` cho Through.
  - Kiểm thử `core/geom/tests/offset/curve/`, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Line, ray, xline, cung, tròn: kết quả chính xác, cùng loại; bán kính ≤ d về phía tâm trả `Collapsed`, không sinh hình.
  2. Ellipse: Hausdorff tới offset thật ≤ `eps` (mặc định 10⁻⁶ × trục lớn **[đề xuất]**), kể cả phía trong có điểm lùi (d > b²/a): vòng lặp đã cắt, không tự giao.
  3. `pickSide` đúng 100% trên 1 triệu điểm QA-009, kể cả điểm sát đường cong (quyết định bằng GEO-003).
  4. p99 máy tầm trung: ≤ 5 µs mỗi đường cong giải tích, ≤ 200 µs mỗi ellipse **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** spline và kết quả dạng NURBS (GEO-155, mốc V vì cần GEO-007).

### GEO-155 · Offset ellipse và spline ra NURBS có cận sai số
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-007, GEO-009, GEO-011
- **Đầu vào:** 05 §7 (spline NURBS; offset xấp xỉ có kiểm soát sai số); NURBS GEO-007; giao có spline GEO-009; GEO-011.
- **Đầu ra:**
  - `core/geom/offset/offset_nurbs.hpp|.cpp`: `offsetToNurbs(curve, d, eps) → vector<Nurbs>` (bậc 3, chèn nút thích ứng đến khi đạt `eps`), tách tại điểm lùi, cắt vòng tự giao bằng GEO-009; `offsetCurve` của GEO-011 nhận thêm spline.
  - Kiểm thử `core/geom/tests/offset/nurbs/`.
- **Tiêu chí nghiệm thu:**
  1. Hausdorff tới offset thật (10.000 điểm mẫu chiếu Newton) ≤ `eps` trên 100.000 spline QA-009 (bậc 2–5, hữu tỉ, kín, tuần hoàn, nút bội).
  2. Có điểm lùi (bán kính cong < d): không còn vòng lặp, không tự giao, tách đúng số mảnh.
  3. Số điểm điều khiển ≤ 8 lần nguồn với `eps` = 10⁻⁶ × hộp bao trên bộ 1.000 spline chuẩn **[đề xuất]**.
  4. Spline 100 điểm điều khiển ≤ 2 ms p99 **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.

## 9. Sắp xếp phẳng (DCEL), mặt và vùng kín

### GEO-156 · Nút hóa tập đường cong: chia tại giao điểm, gộp đỉnh và đoạn trùng
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-003, GEO-010, NT-005
- **Đầu vào:** 05 §7 (sắp xếp phẳng với cạnh là đường cong thật; tất định); giao hàng loạt và phân loại tiếp xúc/trùng đoạn của GEO-010; predicate GEO-003.
- **Đầu ra:**
  - `core/geom/arrangement/noding.hpp|.cpp`: `nodeCurves(span<CurveRef>) → NodedGraph` (đỉnh; cạnh là đường cong con thật kèm đoạn tham số; nhãn nguồn và bội số cho đoạn trùng).
  - Kiểm thử `core/geom/tests/arrangement/noding/`, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Sau nút hóa không hai cạnh nào giao nhau ngoài đỉnh chung: kiểm bằng bộ dò vét cạn với GEO-003 trên 1 triệu ca QA-009.
  2. Điểm cách nhau < tol gộp thành một đỉnh bằng gom cụm tất định (union-find theo khóa ổn định); cạnh ngắn hơn tol bị hủy, hai đầu gộp.
  3. Đoạn trùng (line-line, cung cùng tròn, cung ellipse cùng ellipse) thành một cạnh mang đủ nhãn nguồn.
  4. Hoán vị đầu vào cho cùng đồ thị từng bit; tịnh tiến 10⁹ cho đồ thị đẳng cấu.
  5. 10.000 đường cong, 100.000 giao điểm ≤ 60 ms trên 8 luồng; hiệu suất ≥ 60% đến 8 luồng **[đề xuất]**.
  6. Đạt DoD-C, DoD-H.

### GEO-157 · Dựng DCEL: thứ tự cạnh cong quanh đỉnh và liên kết nửa cạnh
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-156
- **Đầu vào:** 05 §7 (DCEL cạnh cong; quyết định tô-pô không dựa trên phép trừ thô); predicate GEO-003; đồ thị GEO-156.
- **Đầu ra:**
  - `core/geom/arrangement/dcel.hpp|.cpp`: `Dcel` (mảng SoA đỉnh, nửa cạnh, mặt; `twin`, `next`, `prev`, `origin`, `face`), `buildDcel(const NodedGraph&)`.
  - So sánh cạnh quanh đỉnh: hướng tiếp tuyến bằng predicate; trùng tiếp tuyến thì so độ cong có dấu, rồi đạo hàm bậc cao.
- **Tiêu chí nghiệm thu:**
  1. Bất biến đúng 100% trên 1 triệu ca QA-009: `twin(twin(e)) = e`, `next(prev(e)) = e`, `origin(next(e)) = dest(e)`, mỗi nửa cạnh thuộc đúng một chu trình mặt.
  2. Euler V − E + F = 1 + C đúng mọi ca (C: số thành phần liên thông).
  3. Cạnh tiếp xúc tại đỉnh (cung-line, hai cung cùng tiếp tuyến, cung lồng nhau): thứ tự đúng 100% so với tham chiếu số học chính xác.
  4. 100.000 cạnh ≤ 30 ms một luồng, 0 cấp phát trong vòng lặp **[đề xuất]**.
  5. Đạt DoD-C.

### GEO-013 · Sắp xếp phẳng (DCEL) với cạnh là đường cong thật
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-157, QA-006, QA-009, NT-003
- **Đầu vào:** 05 §7 (arrangement cho biên hatch, BOUNDARY, region boolean, TRIM/EXTEND theo biên ngầm); 05 §21 (S3); GEO-156, GEO-157.
- **Đầu ra:**
  - `core/geom/arrangement/arrangement.hpp`: `Arrangement::build(curves, opts)` (nút hóa + DCEL; truy vết cạnh → đường cong nguồn và đoạn tham số); `validate()` kiểm mọi bất biến.
  - Fuzz target `core/geom/fuzz/fuzz_arrangement.cpp`; benchmark `core/geom/bench/bench_arrangement.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Cạnh giữ đúng loại đường cong nguồn (line, cung, ellipse; spline khi GEO-009 có), không xấp xỉ: điểm giữa mỗi cạnh nằm trên nguồn trong tol.
  2. 1 triệu ca QA-009 ngẫu nhiên và suy biến (đồng tuyến, tiếp xúc, trùng điểm, cung rất nhỏ/rất lớn, tọa độ 10⁹): `validate()` 0 lỗi tô-pô.
  3. Fuzz target chạy ≥ 1 giờ không lỗi (QA-006).
  4. 10.000 đường cong ngẫu nhiên ≤ 100 ms trên 8 luồng máy tầm trung **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

### GEO-158 · Mặt, lỗ và cây lồng nhau
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-013, GEO-017
- **Đầu vào:** 05 §7 (tọa độ lớn); DCEL GEO-013; BVH GEO-017; diện tích có dấu polyline bulge GEO-006.
- **Đầu ra:**
  - `core/geom/arrangement/faces.hpp|.cpp`: chu trình mặt; `signedArea(cycle)` cho cạnh line, cung, cung ellipse (Green, công thức đóng, tính quanh gốc cục bộ); phân biên ngoài/lỗ; gán thành phần rời vào mặt chứa (tia + predicate); `FaceTree` lồng nhau.
  - Kiểm thử `core/geom/tests/arrangement/faces/`.
- **Tiêu chí nghiệm thu:**
  1. Biên ngoài CCW, lỗ CW; tổng diện tích có dấu các chu trình của mỗi thành phần = 0 trong tol.
  2. Thành phần rời gán đúng mặt nhỏ nhất chứa nó: khớp 100% tham chiếu vét cạn trên 1 triệu ca QA-009 (lồng 10 mức, đảo chạm biên ngoài tại một điểm).
  3. Diện tích chu trình line/cung: sai số tương đối ≤ 10⁻¹² so với tham chiếu double-double, kể cả tọa độ 10⁹ **[đề xuất]**.
  4. 10.000 thành phần ≤ 20 ms trên 8 luồng **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** diện tích cạnh spline (GEO-021).

### GEO-159 · Tìm vùng kín theo điểm pick và kiểu island
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-158, GEO-017
- **Đầu vào:** 03 §5 (HATCH pick point, island; BOUNDARY); 07.2 UI-E10 (HATCH: vùng tô khi rê chuột); sysvar HPISLANDDETECTION (Normal/Outer/Ignore); GEO-158.
- **Đầu ra:**
  - `core/geom/region/pick_region.hpp|.cpp`: giao diện `BoundarySource` (đường cong theo hộp; bản mặc định trên BVH GEO-017); `findRegionAt(pt, BoundarySource&, IslandStyle) → RegionLoops`: mở rộng hộp dần đến khi mặt chứa điểm kín và không chạm biên hộp; cache arrangement giữa các lần rê trong cùng lệnh.
  - Kiểm thử `core/geom/tests/region/pick/`, benchmark trên cảnh QA-012.
- **Tiêu chí nghiệm thu:**
  1. Vùng trả về chứa điểm pick và là mặt nhỏ nhất chứa nó: khớp 100% tham chiếu arrangement toàn cục trên 1 triệu ca QA-009.
  2. Island: Normal lấy xen kẽ theo độ sâu, Outer chỉ mức ngoài cùng, Ignore bỏ hết; điểm trong island trả vùng của island.
  3. Điểm không nằm trong vùng kín hoặc nằm trên cạnh: trả `NotClosed` hoặc `OnBoundary`, không treo, kết thúc ≤ 50 ms.
  4. Rê chuột trên cảnh hatch dày QA-012: ≤ 8 ms p99/khung khi vùng lân cận ≤ 5.000 cạnh; cache trúng ≤ 1 ms **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.

### GEO-014 · Trích mặt và vùng kín: biên hatch theo pick point, island lồng nhau, BOUNDARY
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-159, QA-009, GEO-108
- **Đầu vào:** 05 §7; 03 §5 (HATCH P0: pick point, island; BOUNDARY P0, dùng để tính diện tích); GEO-158, GEO-159.
- **Đầu ra:**
  - `core/geom/region/boundary.hpp`: `RegionLoops` → vòng biên cho hatch (đường cong thật, hướng chuẩn, cờ ngoài/island) và cho BOUNDARY (`toPolyline()` khi mọi cạnh là line/cung; ellipse/spline giữ nguyên dạng).
  - 200 ca vàng `core/geom/tests/region/golden/`.
- **Tiêu chí nghiệm thu:**
  1. Vòng biên khép kín trong tol, không tự giao; biên ngoài CCW, island CW.
  2. Cạnh giữ đúng cung/ellipse/spline nguồn; `toPolyline` giữ cung bằng bulge, điểm mẫu lệch nguồn ≤ tol.
  3. 200 ca vàng (tiếp xúc, island lồng 5 mức, cạnh trùng, biên có lỗ chạm nhau tại một điểm) đúng 100%; khớp QA-007 HATCH/BOUNDARY khi có đáp án.
  4. BOUNDARY trên vùng 100.000 cạnh ≤ 150 ms trên 8 luồng **[đề xuất]**.
  5. Đạt DoD-C, DoD-H, DoD-L.

### GEO-160 · Nghiệm thu spike S3: 1 triệu ca ngẫu nhiên và suy biến, không sai tô-pô
- **Mốc:** S3 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-010, GEO-012, GEO-014, QA-006, QA-009, GEO-109
- **Đầu vào:** 05 §21 (S3: vượt 1 triệu ca kiểm thử ngẫu nhiên và suy biến, không sai tô-pô); 05 §7 (kiểm thử kernel), §14; bộ sinh và thu nhỏ ca QA-009.
- **Đầu ra:**
  - `core/geom/tests/s3/`: bộ sinh ca cho giao điểm, offset, DCEL, vùng kín (ngẫu nhiên, đồng tuyến, tiếp xúc, trùng điểm, cung rất nhỏ/rất lớn, tọa độ 10⁹); bộ kiểm bất biến tô-pô dùng chung; job đêm trên NT-002.
  - Báo cáo cho cổng G0: `core/geom/docs/s3-ket-qua.md` và JSON do CI sinh.
- **Tiêu chí nghiệm thu:**
  1. 1.000.000 ca mỗi nhóm (giao điểm GEO-010, offset GEO-012, DCEL GEO-013, vùng kín GEO-014): 0 lỗi tô-pô, 0 crash, 0 NaN.
  2. Mọi ca lỗi được thu nhỏ (QA-009), tái lập bằng seed và đưa vào bộ hồi quy.
  3. Bản PR 50.000 ca ≤ 5 phút; bản đêm 4 triệu ca ≤ 60 phút trên máy mạnh **[đề xuất]**.
  4. Báo cáo ghi p50/p99 từng thuật toán, so với ngân sách 05 §1.1 và các ngưỡng **[đề xuất]** của GEO-012, GEO-013, GEO-014.
  5. Đạt DoD-C, DoD-H.

## 10. Vùng: cắt nét hatch, diện tích, khe hở biên

### GEO-161 · Cắt họ nét song song theo biên cong (cho hatch)
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-014, GEO-009, NT-005, NT-006
- **Đầu vào:** 03 §5 (HATCH giữ nguyên cung/spline thật); README §5.7 RGN-012 (sinh nét mẫu cắt theo biên cong); vòng biên GEO-014; giao có spline GEO-009.
- **Đầu ra:**
  - `core/geom/region/hatch_clip.hpp|.cpp`: `clipLineFamily(loops, angle, origin, spacing, FillRule) → LineIntervals` (SoA: chỉ số nét, t0, t1); biên chia trước thành mảnh đơn điệu theo phương vuông góc nét, quy tắc nửa mở tại đỉnh và tiếp điểm.
  - `estimateLineCount()` để RGN-012 áp giới hạn số nét và LOD trước khi sinh.
  - Kiểm thử `core/geom/tests/region/hatch_clip/`, benchmark trên cảnh hatch dày QA-012.
- **Tiêu chí nghiệm thu:**
  1. Mọi khoảng nằm trong vùng (điểm giữa kiểm bằng point-in-region chính xác): 0 sai trên 1 triệu nét QA-009, gồm nét qua đỉnh, tiếp xúc cung, chạy dọc cạnh.
  2. Σ độ dài khoảng × khoảng cách nét lệch diện tích vùng (GEO-158) ≤ 1% khi có ≥ 1.000 nét.
  3. Đường SIMD (NT-006) và đường scalar cho cùng kết quả từng bit.
  4. 10.000 nét × biên 1.000 cạnh ≤ 3 ms trên 8 luồng; hiệu suất ≥ 60% đến 8 luồng **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** nét đứt theo dash của `.pat`, LOD, tô đặc, gradient (RGN-012).

### GEO-021 · Diện tích, chu vi, trọng tâm, tính chất khối lượng của vùng
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-007, GEO-158
- **Đầu vào:** 03 §16 (AREA cộng/trừ, LIST, MASSPROP P1; tổng chiều dài, tổng diện tích của VN); 05 §7 (tọa độ lớn); GEO-005…007; chu trình mặt GEO-158.
- **Đầu ra:**
  - `core/geom/measure/mass_props.hpp|.cpp`: `area`, `perimeter`, `centroid`, `Ixx/Iyy/Ixy`, mô men chính và trục chính, bán kính quán tính cho đường cong kín, polyline bulge, vùng có lỗ; cạnh spline dùng cầu phương Gauss–Legendre thích ứng, trả cận sai số.
  - Kiểm thử `core/geom/tests/measure/`, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Line, cung, ellipse: công thức đóng, sai số tương đối ≤ 10⁻¹² so với tham chiếu double-double **[đề xuất]**.
  2. Có cạnh spline: sai số tương đối ≤ 10⁻¹⁰ và nằm trong cận sai số trả về **[đề xuất]**.
  3. Bất biến trên 1 triệu ca QA-009: tịnh tiến 10⁹ không đổi diện tích và mô men quanh trọng tâm; định lý trục song song; vùng có lỗ = ngoài − lỗ.
  4. Polyline 100.000 đỉnh ≤ 1 ms; tổng diện tích 1 triệu đối tượng ≤ 100 ms trên 8 luồng **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

### GEO-162 · Đóng khe hở biên theo HPGAPTOL
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** GEO-014
- **Đầu vào:** 03 §5 (HATCH nâng cao); sysvar HPGAPTOL; API vùng kín GEO-014.
- **Đầu ra:** `core/geom/region/gap_close.hpp|.cpp`: thêm cạnh nối ảo giữa đầu mút treo và điểm gần nhất trên đường cong khác trong HPGAPTOL trước khi dựng arrangement; cạnh ảo có cờ để hatch và BOUNDARY phân biệt; kiểm thử `core/geom/tests/region/gap/`.
- **Tiêu chí nghiệm thu:**
  1. Khe ≤ HPGAPTOL được đóng, khe > HPGAPTOL vẫn trả `NotClosed`: đúng 100% trên 100.000 ca QA-009 (khe đặt quanh ngưỡng ± 1%).
  2. Chọn cạnh nối ngắn nhất, tất định; không nối đầu mút vào chính đường cong chứa nó.
  3. HPGAPTOL = 0: kết quả trùng từng bit với GEO-014.
  4. Thêm ≤ 1 ms p99 vào một lần tìm vùng có 5.000 cạnh lân cận **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.

## 11. Spline và đường cong khớp

### GEO-163 · Đường cong khớp của polyline: PEDIT Fit, Spline, Decurve
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-006, GEO-007, GEO-151, QA-001
- **Đầu vào:** 07.2 UI-E10 (PEDIT: Fit, Spline, Decurve; kết quả hiện tạm); sysvar SPLINETYPE (5: bậc 2, 6: bậc 3), SPLINESEGS (âm: sinh cung), SPLFRAME; đỉnh khớp và đỉnh khung của POLYLINE 2D (DWG-011).
- **Đầu ra:** `core/geom/spline/polyline_fit.hpp|.cpp`: `fitArcs(pl)` (cặp cung qua mỗi đỉnh), `splineFit(pl, type, segs)` (đỉnh sinh trên B-spline đều, giữ đỉnh khung), `decurve(pl)`; kiểm thử `core/geom/tests/spline/polyline_fit/`.
- **Tiêu chí nghiệm thu:**
  1. Fit: kết quả qua mọi đỉnh gốc, liên tục G1 tại mọi đỉnh trong dung sai góc.
  2. Spline: đỉnh sinh nằm trên B-spline đều bậc 2/3 dựng từ đỉnh khung, sai số ≤ 10⁻⁹ tương đối; SPLINESEGS âm sinh cung qua GEO-151.
  3. Decurve khôi phục đúng đỉnh khung từng bit.
  4. Khớp số đỉnh và tọa độ (trong tol) với polyline Fit/Spline do AutoCAD tạo trong 100 tệp của QA-001 **[đề xuất]**.
  5. 10.000 đỉnh ≤ 5 ms p99 **[đề xuất]**.
  6. Đạt DoD-C, DoD-L.

### GEO-023 · Spline qua điểm (Fit), chuyển polyline ↔ spline, đường nối cong (BLEND)
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** GEO-007, GEO-151, GEO-163, GEO-112
- **Đầu vào:** 03 §5 (SPLINE Fit/CV P0, SPLINEDIT P1, BLEND P1); 07.2 UI-E10 (SPLINE: Method, Knots, Object, Tangency, toLerance, Close; SPLINEDIT: Fit data, convert to Polyline); nội suy fit → điểm điều khiển khớp DWG của GEO-007; sách "The NURBS Book" (Piegl, Tiller).
- **Đầu ra:** `core/geom/spline/`:
  - `fitSpline(points, KnotParam{Chord, SqrtChord, Uniform}, tangents, tol, closed)`: tol = 0 nội suy, tol > 0 xấp xỉ bình phương tối thiểu chèn nút đến khi đạt tol; sửa fit data tính lại gia tăng.
  - `toNurbs()` chính xác cho line, cung, ellipse, polyline bulge (NURBS hữu tỉ); `splineToPolyline(precision 0–99, PLINECONVERTMODE)`; `splineFitPolylineToSpline()` (SPLINE Object).
  - `blend(endA, endB, Tangent|Smooth)`: bậc 3 liên tục G1, bậc 5 liên tục G2.
- **Tiêu chí nghiệm thu:**
  1. tol = 0: qua mọi điểm fit ≤ 10⁻⁹ tương đối; tol > 0: mọi điểm fit cách spline ≤ tol; tiếp tuyến đầu/cuối đúng hướng trong dung sai góc.
  2. `toNurbs`: 10.000 điểm mẫu lệch ≤ 10⁻¹² tương đối; polyline → spline → polyline (PLINECONVERTMODE = 1) giữ hình trong tol.
  3. BLEND: liên tục G1 (Tangent) hoặc G2 (Smooth) tại hai đầu, lệch độ cong ≤ 10⁻⁹ tương đối.
  4. Jig SPLINE: thêm một điểm fit và tính lại ≤ 2 ms với 1.000 điểm (trong 8 ms/khung) **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.

## 12. Nguyên hàm sửa: bẻ, cắt, kéo dài, bo góc, nối

### GEO-164 · Tách đường cong theo tham số và trích đoạn con (BREAK)
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-005, GEO-006, GEO-007, GEO-107, GEO-111
- **Đầu vào:** 03 §6 (BREAK, BREAKATPOINT, TRIM P0); 07.2 UI-E10 (BREAK: First point; BREAKATPOINT); điểm gần nhất và chèn nút của GEO-005…007.
- **Đầu ra:** `core/geom/edit/split.hpp|.cpp`: `splitAt(curve, params) → pieces`, `subCurve(curve, t0, t1)` (vượt qua điểm nối tham số của đường cong kín), `removeBetween(curve, p1, p2)` cho BREAK. Ray → line + ray; xline → 2 ray; tròn → cung ngược chiều kim đồng hồ từ điểm 1 tới điểm 2; ellipse; polyline kín/hở (bulge và độ rộng tại điểm cắt); spline (chèn nút, kín → hở). Kiểm thử `core/geom/tests/edit/split/`.
- **Tiêu chí nghiệm thu:**
  1. Ghép các mảnh trả lại đường cong gốc: đầu mút khớp trong tol, tổng độ dài lệch ≤ 10⁻¹² tương đối, điểm mẫu của mảnh nằm trên gốc trong tol (1 triệu ca QA-009).
  2. Bẻ đường tròn tại một điểm trả lỗi như AutoCAD; điểm cắt trùng đầu mút không sinh mảnh độ dài 0.
  3. Polyline: bulge mới giữ đúng cung; độ rộng tại điểm cắt nội suy tuyến tính theo chiều dài đoạn.
  4. ≤ 10 µs mỗi lần tách đường cong giải tích, ≤ 100 µs spline 1.000 điểm điều khiển, p99 **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.

### GEO-165 · Kéo dài đường cong và đường mang cho biên ngầm
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-164, GEO-010
- **Đầu vào:** 03 §6 (EXTEND, LENGTHEN P0); 07.2 UI-E10 (TRIM/EXTEND: Edge; LENGTHEN: DElta, Percent, Total, DYnamic); sysvar EDGEMODE; giao điểm GEO-010.
- **Đầu ra:** `core/geom/edit/extend.hpp|.cpp`: `carrier(curve)` cho biên ngầm (line → xline, cung → tròn, cung ellipse → ellipse, spline → ngoại suy đa thức của nhịp cuối); `extendEnd(curve, end, target)`; `lengthenBy(curve, end, Delta|Percent|Total)` (giá trị âm rút ngắn qua GEO-164); polyline hở chỉ kéo đoạn đầu hoặc cuối. Kiểm thử `core/geom/tests/edit/extend/`.
- **Tiêu chí nghiệm thu:**
  1. Phần kéo dài nằm trên đường mang trong tol; phần cũ không đổi từng bit.
  2. LENGTHEN Delta/Percent/Total: độ dài (hoặc góc ở tâm của cung) mới lệch ≤ 10⁻¹² tương đối; cung vượt 360° trả lỗi như AutoCAD.
  3. Giao trên đường mang không sót so với tham chiếu vét cạn trên 1 triệu ca QA-009.
  4. ≤ 10 µs mỗi lần kéo đường cong giải tích, p99 **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.

### GEO-019 · Nguyên hàm cắt và kéo dài: cắt theo tham số, kéo dài tới biên (kể cả biên ngầm và phần kéo dài)
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-164, GEO-165, GEO-159, GEO-017
- **Đầu vào:** 03 §6 (TRIM, EXTEND chế độ Quick và Standard; lệnh dùng nhiều nhất); 05 §7 (biên ngầm); 07.2 UI-E10 (cuTting edges/Boundary edges, Crossing, mOde, Project, Edge, eRase; đoạn sẽ bị xóa hiện mờ); `BoundarySource` của GEO-159.
- **Đầu ra:** `core/geom/edit/trim_extend.hpp|.cpp`: `trimAt(target, pick, BoundarySource&, opts) → TrimResult` (mảnh giữ, mảnh bỏ), `extendAt(...)`; Quick (mọi đường cong lân cận là biên) và Standard (tập biên chỉ định); EDGEMODE; nhiều đối tượng theo fence/crossing. Kiểm thử và 300 ca vàng `core/geom/tests/edit/trim/`, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Phần giữ lại không đổi hình (điểm mẫu nằm trên nguồn trong tol); đầu mút mới nằm trên biên trong tol; mảnh bỏ chứa điểm pick (1 triệu ca QA-009).
  2. Quick: không gặp biên thì xóa cả đối tượng như AutoCAD; EXTEND chọn giao gần nhất về phía đầu gần điểm pick.
  3. Polyline tự giao, biên trùng đoạn với đối tượng: 300 ca vàng đúng 100%; khớp QA-007 TRIM/EXTEND khi có đáp án.
  4. BENCH-STD 1 triệu entity: xem trước mỗi lần rê ≤ 2 ms p99; fence qua 1.000 đối tượng ≤ 15 ms (TRIM hoàn tất ≤ 20 ms) **[đề xuất]**.
  5. Đạt DoD-C, DoD-H, DoD-L.

### GEO-166 · Cung bo tiếp xúc hai đường cong
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-155, GEO-010
- **Đầu vào:** 05 §14 (cung FILLET tiếp xúc cả hai đối tượng với bán kính r); 03 §6 (FILLET P0); offset GEO-011, GEO-155; giao GEO-010.
- **Đầu ra:** `core/geom/edit/fillet_arc.hpp|.cpp`: `filletArc(c1, pick1, c2, pick2, r) → FilletSolution` (tâm, hai điểm tiếp xúc, tham số trên mỗi đường cong). Tâm là giao của các offset ±r; line, ray, xline, cung, tròn giải tích; ellipse, spline tinh chỉnh Newton; r = 0 cho góc nhọn; hai line song song cho nửa đường tròn. Kiểm thử `core/geom/tests/edit/fillet_arc/`.
- **Tiêu chí nghiệm thu:**
  1. Tiếp xúc: khoảng cách tâm tới mỗi đường cong = r ± tol, tiếp tuyến tại điểm tiếp xúc song song trong dung sai góc (1 triệu ca QA-009).
  2. Chọn nghiệm theo điểm pick khớp 100% 200 cặp ca vàng (mọi góc phần tư, cung bo trong/ngoài đường tròn).
  3. Không có nghiệm (r quá lớn, offset không giao): trả lỗi rõ, không sinh hình.
  4. ≤ 20 µs mỗi cặp giải tích, ≤ 1 ms cặp có spline, p99 **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.

### GEO-018 · Hình học bo góc và vát góc giữa các cặp đường cong
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-166, GEO-019
- **Đầu vào:** 03 §6 (FILLET, CHAMFER: Polyline/Multiple/Trim); 07.2 UI-E10 (Radius; Distance, Angle, Method, Trim; cung bo hiện tạm khi rê lên đối tượng 2); sysvar TRIMMODE, FILLETRAD, CHAMFERA…CHAMFERD, CHAMMODE.
- **Đầu ra:** `core/geom/edit/fillet_chamfer.hpp|.cpp`: `fillet(c1, pick1, c2, pick2, r, trim) → EditResult` (cung bo, hai đường cong cắt/kéo tới điểm tiếp xúc; tròn không bị cắt); `chamfer` theo hai khoảng cách hoặc khoảng cách + góc cho line, ray, xline, đoạn polyline; `filletPolyline`/`chamferPolyline` mọi đỉnh (bỏ đỉnh có đoạn quá ngắn, trả số đỉnh bỏ qua); hai đoạn cùng polyline cho kết quả là một polyline. Kiểm thử `core/geom/tests/edit/fillet/`.
- **Tiêu chí nghiệm thu:**
  1. Đường cong được cắt/kéo đúng tới điểm tiếp xúc hoặc điểm vát trong tol; Trim = No giữ nguyên hai đường cong.
  2. Vát: hai khoảng cách và góc đo trên kết quả lệch ≤ 10⁻⁹ tương đối.
  3. Polyline: số đỉnh được bo/vát và số đỉnh bỏ qua đúng 100% trên 100 ca vàng; khớp QA-007 FILLET/CHAMFER khi có đáp án.
  4. Xem trước khi rê ≤ 1 ms p99; Polyline 10.000 đỉnh ≤ 15 ms (FILLET hoàn tất ≤ 20 ms) **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.

### GEO-167 · Nối đường cong có dung sai (JOIN, PEDIT Join)
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** GEO-023, GEO-164
- **Đầu vào:** 03 §6 (JOIN P0); 07.2 UI-E10 (JOIN; PEDIT Join, Multiple); quy tắc nối của AutoCAD (line thẳng hàng, cung cùng tròn, cung ellipse cùng ellipse được có khe; polyline, spline phải liền đầu mút); khoảng fuzz và kiểu nối Extend/Add/Both của PEDIT Join; `toNurbs` GEO-023.
- **Đầu ra:** `core/geom/edit/join.hpp|.cpp`: `joinCurves(source, others, opts) → JoinResult` (đường cong kết quả, danh sách đã nối, danh sách bỏ qua kèm lý do): gom đầu mút bằng lưới băm trong dung sai, dựng chuỗi tất định, đảo chiều mảnh khi cần, hợp line thẳng hàng và cung cùng tròn, kết quả polyline hoặc spline. Kiểm thử `core/geom/tests/edit/join/`.
- **Tiêu chí nghiệm thu:**
  1. Kết quả phủ mọi đối tượng được nối (Hausdorff ≤ tol); không có khe thì độ dài = tổng độ dài các mảnh (lệch ≤ 10⁻¹² tương đối).
  2. Line thẳng hàng có khe, cung cùng tròn có khe nối đúng; cung đủ 360° thành đường tròn.
  3. PEDIT Join: Extend/Add/Both đúng hình ở 100 ca vàng; khe > fuzz không nối.
  4. Tách (GEO-164) rồi nối trả lại đường cong gốc trong tol trên 1 triệu ca QA-009.
  5. 10.000 đối tượng ≤ 15 ms p99 **[đề xuất]**.
  6. Đạt DoD-C, DoD-L.

## 13. Làm sạch và nhận dạng hình học

### GEO-168 · Phát hiện trùng lặp và chồng lấn (OVERKILL)
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** GEO-164, GEO-167, NT-005
- **Đầu vào:** 03 §6 (OVERKILL P1); 07.2 UI-E04 (dung sai; bỏ qua thuộc tính; tối ưu đoạn trong polyline; bỏ qua bề rộng polyline; không phá polyline; gộp đoạn thẳng hàng chồng một phần; gộp đoạn thẳng hàng nối tiếp; giữ đối tượng liên kết).
- **Đầu ra:** `core/geom/cleanup/overkill.hpp|.cpp`: khóa đường mang lượng tử theo dung sai (đường thẳng; tròn: tâm, bán kính; ellipse), nhóm song song theo khóa, hợp khoảng tham số trên mỗi đường mang; `findOverkill(curves, opts) → Plan` (xóa, gộp, thay mảnh) để lệnh áp qua DOC-007. Kiểm thử `core/geom/tests/cleanup/overkill/`, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Trùng hoàn toàn (kể cả đảo chiều, polyline kín bắt đầu ở đỉnh khác): phát hiện 100% trên 1 triệu ca QA-009; 0 dương tính giả khi lệch > dung sai.
  2. Chồng một phần và nối tiếp: gộp đúng theo từng tùy chọn của UI-E04; hình sau khi áp kế hoạch phủ đúng hình trước (Hausdorff ≤ tol).
  3. Lệch đúng sát dung sai, rơi ở biên ô lượng tử: kết quả không phụ thuộc vị trí lưới (kiểm cả ô lân cận).
  4. 1 triệu entity ≤ 1,5 s trên 8 luồng; hiệu suất ≥ 60% đến 8 luồng **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

### GEO-025 · Chữ ký hình học bất biến khi xoay và đổi tỉ lệ (tìm cụm tương tự, TCBLOCKMATCH)
- **Mốc:** 1.0 · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** GEO-021, GEO-017, NT-005
- **Đầu vào:** 07.2 UI-G16 (Tìm & chuyển đổi kiểu Smart Match: cụm tương tự theo dung sai, bất biến khi xoay và đổi tỉ lệ); mô men GEO-021; BVH GEO-017.
- **Đầu ra:** `core/geom/match/`:
  - `computeSignature(curves)`: đếm theo loại, phân bố khoảng cách cặp điểm mẫu chuẩn hóa theo tỉ lệ, bất biến mô men.
  - Tách cụm theo liên thông đầu mút và khoảng cách; chỉ mục tìm gần nhất trên chữ ký.
  - `verifySimilarity(a, b, tol) → optional<Similarity2d>`: ước lượng xoay + tỉ lệ đều + tịnh tiến (tùy chọn đối xứng), kiểm Hausdorff.
- **Tiêu chí nghiệm thu:**
  1. Cụm biến đổi ngẫu nhiên (xoay, tỉ lệ 10⁻³–10³, tịnh tiến 10⁹, hoán vị và đảo chiều đường cong): tìm lại 100% trên 100.000 ca QA-009.
  2. 0 kết quả sai sau kiểm chứng: mọi cặp trả về có Hausdorff ≤ tol sau phép đồng dạng.
  3. Chữ ký của bản gốc và bản biến đổi lệch ≤ 10⁻⁹ tương đối.
  4. Bản vẽ 1 triệu entity: dựng chỉ mục ≤ 2 s trên 8 luồng, một truy vấn ≤ 50 ms **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

### GEO-169 · Nguyên hàm làm sạch hình học (TCCLEANUP)
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** GEO-168, GEO-156, GEO-165
- **Đầu vào:** 07.2 UI-I08 (khe hở, đoạn thừa, đoạn hụt, góc gần 0°/90° bị lệch, đối tượng trùng, độ dài 0; dung sai từng loại; Sửa / Sửa tất cả / Bỏ qua); nút hóa GEO-156; kéo dài GEO-165; GEO-168.
- **Đầu ra:** `core/geom/cleanup/cleanup.hpp|.cpp`: `scanIssues(curves, tolerances) → vector<Issue>` (loại, vị trí, đối tượng, đề xuất sửa) cho 6 loại vấn đề; `fixPlan(issues)`: nối khe, cắt đoạn thừa, kéo đoạn hụt tới đường gần nhất, xoay về 0°/90° quanh đầu mút, xóa trùng qua GEO-168, xóa độ dài 0. Kiểm thử `core/geom/tests/cleanup/`.
- **Tiêu chí nghiệm thu:**
  1. Mỗi loại vấn đề: phát hiện 100%, 0 dương tính giả trên bộ 1.000 ca cài lỗi quanh ngưỡng ± 1%.
  2. Áp kế hoạch rồi quét lại: 0 vấn đề còn lại; hình không liên quan không đổi từng bit.
  3. Sửa tất cả cho cùng kết quả với mọi số luồng quét.
  4. Quét 1 triệu entity ≤ 3 s trên 8 luồng; chạy nền, hủy được ≤ 50 ms **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

## 14. Boolean region và bộ giải ràng buộc (sau v1.0)

### GEO-015 · Boolean region (hợp, trừ, giao) trên vùng có cạnh cong
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** GEO-014, GEO-021
- **Đầu vào:** 03 §6 (UNION, SUBTRACT, INTERSECT P2); 05 §7 (arrangement dùng cho region boolean); nhãn nguồn trên cạnh GEO-013; GEO-014, GEO-021.
- **Đầu ra:** `core/geom/region/boolean.hpp|.cpp`: `regionBoolean(A, B, op)` và dạng nhiều toán hạng cho UNION: chồng arrangement, gán nhãn mặt theo tập nguồn, chọn mặt theo phép toán, trích vòng biên (đường cong thật, lỗ, nhiều mảnh). Kiểm thử `core/geom/tests/region/boolean/`.
- **Tiêu chí nghiệm thu:**
  1. S(A∪B) = S(A) + S(B) − S(A∩B) và S(A−B) = S(A) − S(A∩B), lệch ≤ 10⁻⁹ tương đối trên 1 triệu ca QA-009 (cạnh trùng, tiếp xúc tại điểm, vùng lồng nhau).
  2. A∪A = A, A∩A = A, A−A = ∅ đúng tô-pô.
  3. Kết quả là vùng hợp lệ: biên khép kín, không tự giao, lỗ nằm trong biên ngoài.
  4. Hai vùng 10.000 cạnh ≤ 50 ms trên 8 luồng **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** ghi REGION dạng ACIS (05 §7.1).

### GEO-170 · Lõi giải hệ phi tuyến thưa cho ràng buộc
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** GEO-002, NT-005
- **Đầu vào:** 03 §15 (bộ giải tự viết, tham khảo thuật toán PlaneGCS); tài liệu "Methods for Non-Linear Least Squares Problems" (Madsen, Nielsen, Tingleff) cho DogLeg và Levenberg–Marquardt.
- **Đầu ra:** `core/geom/constraint/solver.hpp|.cpp`: biến, phương trình dư, Jacobian thưa; DogLeg, dự phòng Levenberg–Marquardt; phân rã đồ thị ràng buộc thành hệ con độc lập giải song song; hạng Jacobian (QR có xoay cột) để đếm bậc tự do, phát hiện ràng buộc thừa và xung đột. Kiểm thử `core/geom/tests/constraint/solver/`.
- **Tiêu chí nghiệm thu:**
  1. Bộ 500 hệ chuẩn (tam giác, đa giác, cơ cấu thanh, chuỗi tiếp xúc): hội tụ 100%, dư ≤ 10⁻¹⁰ tương đối **[đề xuất]**.
  2. Bậc tự do và tập ràng buộc thừa/xung đột khớp 100% đáp án soạn tay.
  3. Biến không chịu ràng buộc nào không đổi giá trị sau khi giải.
  4. 2.000 ràng buộc ≤ 50 ms; hiệu suất ≥ 60% đến 8 luồng khi có ≥ 8 hệ con **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

### GEO-171 · Phương trình ràng buộc hình học và kích thước
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** GEO-170, GEO-005, GEO-006, GEO-007
- **Đầu vào:** 03 §15 (12 ràng buộc hình học; ràng buộc kích thước dynamic/annotational); GEO-005…007; lõi giải GEO-170.
- **Đầu ra:** `core/geom/constraint/constraints.hpp|.cpp`: ánh xạ entity → biến (đầu mút line; tâm, bán kính, góc của cung/tròn; ellipse; đỉnh và bulge polyline; điểm fit/điều khiển spline); 12 ràng buộc (coincident, collinear, concentric, fix, parallel, perpendicular, horizontal, vertical, tangent, smooth, symmetric, equal); kích thước thẳng, ngang, dọc, căn, góc, bán kính, đường kính; Jacobian giải tích. Kiểm thử `core/geom/tests/constraint/`.
- **Tiêu chí nghiệm thu:**
  1. Jacobian giải tích khớp sai phân hữu hạn ≤ 10⁻⁶ tương đối cho mọi loại ràng buộc trên 100.000 cấu hình QA-009.
  2. Sau khi giải: mọi ràng buộc thỏa trong tol khoảng cách và dung sai góc của GEO-002.
  3. Tangent và smooth giữ đúng nhánh (trong/ngoài) của cấu hình ban đầu: 100% trên 200 ca vàng.
  4. Đạt DoD-C.

### GEO-024 · Bộ giải ràng buộc hình học và kích thước 2D (tham khảo thuật toán PlaneGCS)
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** GEO-171
- **Đầu vào:** 03 §15 (GEOMCONSTRAINT, DIMCONSTRAINT, AUTOCONSTRAIN, CONSTRAINTINFER, PARAMETERS, DELCONSTRAINT); GEO-170, GEO-171; ngân sách jig 8 ms/khung (INT-012).
- **Đầu ra:** `core/geom/constraint/system.hpp|.cpp`: `ConstraintSystem` (thêm/xóa/sửa ràng buộc và tham số, giải gia tăng chỉ hệ con bị ảnh hưởng, giải khi kéo grip); `inferConstraints(curves, tol)` cho AUTOCONSTRAIN và CONSTRAINTINFER; trạng thái thiếu/đủ/thừa ràng buộc cho UI. Kiểm thử, benchmark `core/geom/bench/bench_constraint.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Kéo grip trên hệ 200 ràng buộc ≤ 4 ms p99 mỗi khung **[đề xuất]**.
  2. Đổi giá trị tham số: mọi ràng buộc thỏa trong tol; giải lại cùng đầu vào cho cùng kết quả từng bit.
  3. AUTOCONSTRAIN trên 100 ca vàng: đúng tập ràng buộc kỳ vọng, không suy ra ràng buộc thừa.
  4. Hệ xung đột: trả danh sách ràng buộc gây xung đột, hình không đổi.
  5. Đạt DoD-C, DoD-L.

## Tóm tắt theo mốc

| Mốc | Task | Số task |
|---|---|---|
| S1 | GEO-001 | 1 |
| S2 | GEO-102, GEO-103, GEO-017 | 3 |
| S3 | GEO-002, GEO-003, GEO-004, GEO-101, GEO-104, GEO-105, GEO-106, GEO-107, GEO-005, GEO-006, GEO-008, GEO-108, GEO-010, GEO-109, GEO-151, GEO-152, GEO-153, GEO-154, GEO-012, GEO-011, GEO-156, GEO-157, GEO-013, GEO-158, GEO-159, GEO-014, GEO-160 | 27 |
| V | GEO-110, GEO-111, GEO-112, GEO-007, GEO-009, GEO-022, GEO-113, GEO-016, GEO-155, GEO-161, GEO-021 | 11 |
| EA | GEO-114, GEO-115, GEO-116, GEO-020, GEO-162, GEO-163, GEO-023, GEO-164, GEO-165, GEO-019, GEO-166, GEO-018, GEO-167 | 13 |
| 1.0 | GEO-168, GEO-025 | 2 |
| Sau | GEO-169, GEO-015, GEO-170, GEO-171, GEO-024 | 5 |
| **Tổng** | | **62** |
