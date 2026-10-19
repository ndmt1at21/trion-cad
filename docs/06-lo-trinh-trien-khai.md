# 06 — Lộ trình triển khai, đội ngũ, ngân sách, rủi ro

> Phương án: **tự viết toàn bộ lõi bằng C++20**, không mua SDK hay giấy phép lõi; hiệu năng cao, logic chính xác, tận dụng tối đa phần cứng ([05](05-kien-truc-ky-thuat.md)).
> Mốc tính từ **Tháng 1 = tháng bắt đầu dự án**. Mọi con số tài chính là **kịch bản cơ sở [ước tính]**. Tỷ giá 1 USD ≈ 26.400 đ. Chi phí nhân sự toàn phần = lương gross × 1,3.

## 1. Tổng quan lộ trình

```
Tháng:    1   3        9          17   18        25   27             36(±4)
GĐ0:     [nền móng + spike]
GĐ1:          [——— Lõi: codec đọc, hình học, regen, renderer ———] → Viewer miễn phí (T16–17)
GĐ2:                                          [—— Biên tập: lệnh, ghi DWG, LISP ——] → Viewer Pro (T18), Pro Early Access (T25)
GĐ3:                                                                  [—— P1 + hoàn thiện ——] → v1.0 (T36 ±4)
Cộng đồng: công cụ miễn phí (T2–3) ───────────────────────────────────────────────────────────────►
Cổng:      G0(T3)                   G1(T17)             G2(T27)         G3(v1.0)
```

## 2. Giai đoạn 0: Nền móng và spike (tháng 1–3)

**Mục tiêu:**
- chứng minh bằng prototype rằng tự viết đạt được hiệu năng và độ chính xác yêu cầu;
- có đội lõi;
- có tiền cho ít nhất 12 tháng.

### 2.1 Kế hoạch 90 ngày theo tuần

| Tuần | Kinh doanh và pháp lý | Kỹ thuật |
|---|---|---|
| 1–2 | Thành lập công ty cổ phần; tra cứu và nộp đơn nhãn hiệu (nhóm 9, 42); tên miền. Thuê luật sư SHTT để rà: chính sách tự viết và tài liệu tham khảo; điều khoản đặc tả DWG của ODA; EULA của DWG TrueView và ODA File Converter khi dùng làm công cụ đối chứng; giấy phép các thư viện hạ tầng | Kho mã, CI, quy ước C++20, chính sách clean-room và mã nguồn mở. Mua **4 máy benchmark** (máy yếu Intel HD, máy tầm trung, máy mạnh nhiều nhân + GPU rời, Mac Apple Silicon) |
| 1–4 | Tuyển CTO và **3 kỹ sư C++ cao cấp** (codec định dạng, đồ họa GPU, hình học tính toán) | Thu thập **kho DWG** (1.000 tệp đầu, có thỏa thuận sử dụng) và **bộ LISP** (100). Dựng harness đối chứng (TrueView, ODA File Converter, LibreDWG CLI) |
| 2–8 | **Phỏng vấn 50–60 khách hàng**; khảo sát hơn 500 người; tìm 10–20 **doanh nghiệp pilot** đồng ý chạy kịch bản tham chiếu trên AutoCAD có bản quyền của họ | Spike **S1** (codec DWG), **S2** (renderer QRhi), **S3** (kernel hình học), **S6** (IME tiếng Việt, render text) |
| 4–10 | Phát hành **Bác sĩ Font (LISP) + Thư viện TCVN miễn phí**; mở danh sách chờ TrionCAD Viewer | Spike **S4** (Bác sĩ Font C++), **S5** (engine LISP) |
| 6–12 | Chuẩn bị gọi vốn (demo: mở DWG nhanh, render 1 triệu entity mượt trên máy yếu); hồ sơ NIC/NATIF | Chốt ADR kiến trúc; ước lượng lại khối lượng theo kết quả spike |
| 12–13 | **Cổng G0** | |

### 2.2 Cổng G0 (cuối tháng 3)

| Tiêu chí | Ngưỡng |
|---|---|
| Spike S1–S6 | Đạt tiêu chí ở [05](05-kien-truc-ky-thuat.md) §21. Quan trọng nhất: codec nguyên mẫu đọc ≥ 90% kho 1.000 DWG ở mức entity lõi; renderer ≥ 60 fps với 1 triệu entity trên GPU tích hợp |
| Đội ngũ | CTO và 3 kỹ sư cao cấp đã vào làm; danh sách ứng viên cho 5 vị trí tiếp theo |
| Khách hàng | ≥ 60% người được phỏng vấn sẵn sàng thử; ≥ 10 doanh nghiệp pilot cam kết |
| Tài chính | Đủ tiền cho ít nhất 12 tháng |

Nếu spike không đạt, ước lượng lại và báo cáo trung thực với nhà sáng lập trước khi mở rộng đội. Không tăng người khi nền móng chưa chứng minh được.

## 3. Giai đoạn 1: Lõi và TrionCAD Viewer (tháng 4–17)

| Tháng | Mốc | Nội dung chính |
|---|---|---|
| 4–6 | Nền móng lõi | Mô hình bản vẽ; codec **đọc** R2000/R2004/R2018 cho entity lõi; schema và codegen; kernel hình học (đường cong, giao điểm, dung sai, predicate bền vững); renderer v1 (batch, BVH, đường cong giải tích, lineweight); bộ lập lịch work-stealing và lớp SIMD |
| 7–10 | Regen | SHX/TTF, MTEXT, linetype, hatch (biên cong, island), kích thước và bảng hiển thị từ block, block lồng, xref, ảnh, layout và viewport, proxy graphics (bản đầu) |
| 10–12 | **Viewer alpha nội bộ** | In và xuất PDF (bộ ghi PDF tự viết, CTB), đo, layer, Bác sĩ Font; tối ưu hiệu năng theo ngân sách ([05](05-kien-truc-ky-thuat.md) §1) |
| 13–14 | **Viewer beta kín** (300 người dùng) | Nút "Gửi bản vẽ lỗi"; mỗi tuần sửa lỗi theo kho tệp mới; benchmark so với DWG TrueView |
| 15–16 | Cứng hóa | R2007, R13/R14, recover, tệp lớn, máy yếu, tự thích ứng phần cứng |
| **16–17** | **TrionCAD Viewer miễn phí ra công chúng** | Ra mắt trực tuyến, công bố benchmark tốc độ |

Song song từ tháng 10: 2–3 kỹ sư dựng command framework, input pipeline, OSNAP và undo để chuẩn bị cho Giai đoạn 2.

**KPI cuối Giai đoạn 1:**
- ≥ 15.000 người đăng ký công cụ miễn phí và danh sách chờ.
- ≥ 5.000 người dùng Viewer trong tháng đầu.
- Mở thành công ≥ 98% kho 10.000 DWG.
- SSIM ≥ 0,97 trên ≥ 95% tờ.
- Nhanh hơn DWG TrueView ở ≥ 70% bài đo.

**Cổng G1 (tháng 17):** đạt KPI chất lượng và người dùng → chuẩn bị Series A. Chưa đạt → kéo dài cứng hóa Viewer, giữ nguyên quy mô đội.

## 4. Giai đoạn 2: Viewer Pro và bản biên tập (tháng 18–27)

| Tháng | Mốc | Nội dung chính |
|---|---|---|
| **18** | **Viewer Pro (trả phí)** | In hàng loạt theo khung tên, PDF theo CTB, `COMPARE`, chuyển font hàng loạt, ghi chú, xuất bảng thống kê; cổng thanh toán và hóa đơn điện tử |
| 18–20 | Nền tảng biên tập | Undo journal, codec **ghi** AC1032/AC1018 (đối chứng bằng 3 bộ đọc), command framework, prompt và keyword, OSNAP/POLAR/OTRACK, chọn đối tượng, grip, palette thuộc tính, quản lý layer |
| 21–23 | Lệnh lõi | Khoảng 80 lệnh P0 theo đặc tả hành vi; kích thước sinh lại theo DIMVAR; trình soạn text và MTEXT; block và attribute |
| 22–24 | LISP | Engine lõi, DCL, cầu nối `(command)`; bộ LISP ≥ 60% |
| **25** | **Pro Early Access** | Toàn bộ P0 ([03](03-chuc-nang-autocad.md) Phụ lục A); giá ưu đãi trọn đời |
| 26–27 | Hoàn thiện P0 | Bộ LISP ≥ 75%; bản macOS alpha; tối ưu trên máy yếu |

**KPI cuối Giai đoạn 2:**
- ≥ 3.000 ghế Viewer Pro.
- ≥ 1.000 ghế Pro Early Access.
- Viewer ≥ 20.000 người dùng hoạt động hằng tháng.
- NPS ≥ 30.

**Cổng G2 (tháng 27):**
- **Kỹ thuật:** toàn bộ P0 đạt kiểm thử tham chiếu; tệp ghi ra qua được 3 bộ đọc độc lập; crash-free ≥ 99,5%.
- **Kinh doanh:** đạt KPI trên.

## 5. Giai đoạn 3: TrionCAD 1.0 (tháng 28–36 ±4)

- Hoàn thành các tính năng P1 (ngang AutoCAD LT ở 2D), gồm:
  - dynamic block (grip, parameter, action);
  - bảng và mleader đầy đủ, nhập PDF;
  - `COMPARE`, in hàng loạt theo khung tên, quản lý bộ bản vẽ kiểu VN;
  - nhập CUIX một phần.
- LISP ≥ 90% với tập con `vla-*`.
- Bản Doanh nghiệp: máy chủ floating license, cổng quản trị, cài đặt hàng loạt.
- SDK C API + C++ wrapper (beta) cho 1–2 đối tác chuyên ngành.
- macOS beta, sau đó GA cùng đợt hoặc ngay sau Windows.
- Cứng hóa hiệu năng và chất lượng theo toàn bộ ngân sách ở [05](05-kien-truc-ky-thuat.md) §1.
- **v1.0 ra mắt** với chương trình hợp thức hóa bản quyền và mạng đại lý.

**Cổng G3 (v1.0):** đạt toàn bộ ngân sách hiệu năng và độ chính xác. Sau đó quyết định thứ tự cho Linux, đám mây, AI, kernel 3D, khu vực.

## 6. Sau v1.0 (từ khoảng tháng 37)

- Linux GA.
- Đám mây v1: lịch sử phiên bản, chia sẻ qua Zalo, ghi chú, trình xem web WASM.
- Trợ lý AI (MCP, kiểm tra TCVN, sinh LISP từ tiếng Việt).
- SDK GA và chương trình OEM; gói chuyên ngành.
- Indonesia và Philippines.
- Quyết định kernel 3D ([05](05-kien-truc-ky-thuat.md) §7.1).

## 7. Đội ngũ theo thời gian

| Vai trò | T3 | T6 | T12 | T18 | T27 | T36 |
|---|---|---|---|---|---|---|
| Lãnh đạo (CEO, CTO; COO từ T19) | 2 | 2 | 2 | 2 | 3 | 3 |
| Kỹ sư C++: codec, hình học, regen, renderer và hiệu năng, lệnh, LISP, nền tảng (không tính CTO) | 3 | 8 | 13 | 16 | 18 | 19 |
| QA, tự động hóa kiểm thử | 0 | 1 | 1 | 2 | 3 | 4 |
| Product, UX | 0,5 | 1 | 1 | 2 | 3 | 3 |
| Cloud, DevOps | 0 | 0 | 0 | 0 | 0 | 1 |
| Marketing, cộng đồng | 0,5 | 0,5 | 1 | 1 | 2 | 2 |
| Bán hàng, BD | 0 | 0 | 0 | 0,5 | 1 | 3 |
| Hỗ trợ, đào tạo | 0 | 0 | 0 | 0,5 | 2 | 3 |
| **Tổng** | **~6** | **~12,5** | **~18** | **~24** | **~32** | **~38** |

**Hồ sơ chuyên gia cần có trong đội lõi:**
- codec định dạng nhị phân và kỹ thuật ngược;
- hình học tính toán (độ bền số học);
- đồ họa GPU (D3D/Vulkan/Metal, shader);
- tối ưu hiệu năng (SIMD, đa luồng, cache);
- trình thông dịch (LISP);
- kỹ sư ứng dụng CAD (hiểu sâu cách AutoCAD hành xử).

**Nguồn tuyển:** studio game, công ty nhúng và robotics, kiều bào làm từ xa, thực tập sinh từ các trường kỹ thuật. Có ESOP cho đội lõi.

## 8. Ngân sách 36 tháng (kịch bản cơ sở)

### 8.1 Chi phí theo giai đoạn

| Hạng mục (tỷ đ) | GĐ0 (T1–3) | GĐ1 (T4–17) | GĐ2 (T18–27) | GĐ3 (T28–36) | Tổng |
|---|---|---|---|---|---|
| Nhân sự (gross × 1,3) | 1,7 | 20,0 | 21,4 | 21,3 | 64,4 |
| **Phí bản quyền lõi** | **0** | **0** | **0** | **0** | **0** |
| Chi phí bắt buộc nhỏ (ký số, Apple Developer, tên miền, văn bản TCVN, nhãn hiệu) | 0,05 | 0,12 | 0,1 | 0,08 | 0,35 |
| Phần cứng, hạ tầng (máy benchmark, runner CI tự host, máy chủ) | 0,4 | 0,9 | 0,9 | 0,8 | 3,0 |
| Marketing, cộng đồng | 0,1 | 0,9 | 1,8 | 2,4 | 5,2 |
| Văn phòng | 0,1 | 0,8 | 0,9 | 0,9 | 2,7 |
| Pháp lý, kế toán, thiết kế font | 0,2 | 0,5 | 0,4 | 0,3 | 1,4 |
| Mở rộng khu vực | — | — | — | 0,5 | 0,5 |
| Dự phòng 15% (tự viết nên rủi ro cao hơn) | 0,4 | 3,5 | 3,8 | 3,9 | 11,6 |
| **Tổng chi** | **2,9** | **26,8** | **29,3** | **30,3** | **≈ 89 tỷ đ (~3,4 triệu USD)** |

### 8.2 Doanh thu kịch bản cơ sở **[ước tính]**

| | Năm 1 (T1–12) | Năm 2 (T13–24) | Năm 3 (T25–36) |
|---|---|---|---|
| Viewer miễn phí, người dùng hoạt động hằng tháng cuối kỳ | — | 12.000 | 35.000 |
| Viewer Pro, ghế cuối kỳ (690.000 đ/năm) | — | 2.000 | 6.000 |
| Pro Early Access, ghế cuối kỳ (1.490.000 đ/năm) | — | — | 3.000 |
| Doanh thu phần mềm | 0 | 0,4 tỷ | 4,8 tỷ |
| Dịch vụ (đào tạo, tùy biến, chuẩn hóa bản vẽ) | 0 | 0,3 tỷ | 1,0 tỷ |
| **Tổng doanh thu** | **0** | **0,7 tỷ** | **5,8 tỷ** |

- **Dòng tiền ròng 36 tháng:** −89 + 6,5 ≈ **−83 tỷ đ (~3,1 triệu USD)**. Doanh thu chủ yếu đến sau v1.0, khi Pro bán giá đầy đủ 2,99 triệu đ/năm.
- **Lũy kế đến khi Viewer ra mắt (T17):** khoảng 30 tỷ đ (~1,1 triệu USD).
- **Hòa vốn dự kiến:** khoảng tháng 50–60, tức 1,5–2 năm sau v1.0, khi có khoảng 15.000–20.000 ghế trả phí (Pro và Viewer Pro).

### 8.3 Kế hoạch vốn

| Vòng | Thời điểm | Quy mô | Mục đích | Điều kiện |
|---|---|---|---|---|
| Vốn tự có / angel / pre-seed | T0–T3 | 0,5–0,7 triệu USD | GĐ0 và khoảng 8 tháng đầu GĐ1 | — |
| Seed | T9–T12 | 1,5–2 triệu USD | Hoàn thành Viewer, bắt đầu bản biên tập | Đạt G0; demo tốc độ; cộng đồng ≥ 10.000 |
| Series A | T20–T24 | 3–4 triệu USD | Đến v1.0 và 12 tháng sau | Đạt G1; Viewer tăng trưởng; có doanh thu Viewer Pro |
| Không pha loãng | Liên tục | — | NATIF, NIC, ưu đãi Luật CN CNS, hợp tác R&D với trường, doanh thu dịch vụ | — |

**Phương án tinh gọn** (nếu không muốn gọi vốn lớn):
- 8–10 kỹ sư; ưu tiên Viewer, Viewer Pro và dịch vụ để có doanh thu từ khoảng tháng 20.
- v1.0 lùi về khoảng **tháng 54–60**.
- Chi khoảng 15–18 tỷ đ/năm.
- Nhanh hay chậm là đánh đổi giữa vốn và thời gian. Logic sản phẩm không thay đổi.

## 9. Sổ rủi ro (top 15)

| # | Rủi ro | Khả năng | Tác động | Giảm thiểu |
|---|---|---|---|---|
| 1 | Codec DWG phức tạp hơn dự kiến (R2007, dynamic block, proxy, tệp hỏng) | Cao | Rất cao | Schema + codegen; kho bản vẽ lớn; đối chứng chéo 3 bộ đọc; Viewer sớm để thu tệp lỗi; ưu tiên theo tần suất thực tế |
| 2 | Không đạt hiệu năng trên máy yếu | TB | Cao | Spike S2 từ tháng 1; benchmark CI trên máy thật; thiết kế hướng dữ liệu; LOD thích ứng; tự chọn backend |
| 3 | Hành vi lệnh lệch so với AutoCAD | TB | Cao | Đặc tả hành vi; bộ tham chiếu do người dùng AutoCAD hợp pháp tạo; bất biến hình học; pilot |
| 4 | Thời gian đến doanh thu dài, thiếu vốn | Cao | Rất cao | Gọi vốn theo cổng; Viewer Pro sớm; dịch vụ; tài trợ; phương án tinh gọn; luôn giữ 6 tháng runway |
| 5 | Thiếu chuyên gia C++, hình học, GPU | Cao | Cao | Tuyển từ ngành lân cận; kiều bào; cố vấn; ESOP; đào tạo nội bộ; hợp tác trường |
| 6 | Đối thủ (ZWCAD, GstarCAD) chiếm làn sóng hợp thức hóa trong 2–3 năm tới | Cao | Cao | Công cụ miễn phí, Viewer, Viewer Pro để có mặt sớm; danh sách người dùng; câu chuyện "CAD Việt, nhanh hơn" |
| 7 | Phạm vi phình to | Cao | TB | P0/P1 khóa cứng; mọi bổ sung phải bỏ bớt thứ khác; ưu tiên theo telemetry |
| 8 | Pháp lý: kỹ thuật ngược định dạng, nhãn hiệu, font, FAS/VLX | Thấp–TB | Cao | Luật sư SHTT; nhật ký nguồn tham khảo; clean-room; không chép mã GPL; tự thiết kế font và biểu tượng |
| 9 | Phần mềm chuyên ngành giữ khách ở AutoCAD | Cao | Cao | SDK C/C++ gần khái niệm ObjectARX; hỗ trợ port; chia sẻ doanh thu |
| 10 | Bảo mật: codec đọc tệp không tin cậy, LISP độc hại | TB | Cao | Fuzzing liên tục; sanitizer; giới hạn cấp phát; `SECURELOAD`; ký số |
| 11 | Phụ thuộc Qt (giấy phép, thay đổi API của QRhi) | Thấp | TB | Lớp bọc mỏng; ghim bản LTS; phương án OpenGL 3.3 |
| 12 | Tiêu chí "sản phẩm số trong nước" | TB | TB | Hỏi Bộ KH&CN sớm. Lõi tự viết 100% là lợi thế lớn |
| 13 | Niềm tin với thương hiệu mới | Cao | TB | Viewer miễn phí; benchmark công khai; case study pilot; hoàn tiền trong 30 ngày |
| 14 | Kiệt sức do dự án dài | TB | Cao | Mốc nhỏ có thể ăn mừng (công cụ miễn phí, Viewer, Viewer Pro); không làm thêm giờ kéo dài; minh bạch tiến độ |
| 15 | Tỷ giá, chi phí thiết bị | Thấp | Thấp | Dự phòng 15% |

## 10. Quản trị dự án

- **Nhịp làm việc:**
  - OKR theo quý; sprint 2 tuần; demo hằng tuần;
  - **bảng chỉ số hằng ngày**: tỷ lệ mở kho bản vẽ, benchmark hiệu năng, kết quả bộ tham chiếu, crash;
  - hội đồng tư vấn họp mỗi quý.
- **Quyết định:** các cổng G0–G3 có tiêu chí định lượng; quyết định kỹ thuật ghi thành ADR.
- **Nguồn sự thật:** thư mục `docs/`. Cập nhật mỗi khi một giả định **[ước tính]** hoặc **[cần xác minh]** được kiểm chứng.

## 11. Việc cần làm ngay (2 tuần đầu)

1. Chốt đội sáng lập, vai trò và cổ phần; thành lập công ty.
2. Tra cứu nhãn hiệu "TrionCAD"; nếu trùng thì chọn tên khác trước khi làm thương hiệu.
3. Đăng tuyển CTO và 3 kỹ sư C++ cao cấp (codec, đồ họa GPU, hình học), kèm bài kiểm tra kỹ thuật thực tế.
4. Thuê luật sư SHTT; viết chính sách "tự viết, tham khảo, đối chứng" ([05](05-kien-truc-ky-thuat.md) §2 và §15).
5. Mua 4 máy benchmark; dựng CI với runner tự host.
6. Lập kế hoạch thu thập kho DWG và bộ LISP; mẫu thỏa thuận cho phép sử dụng; danh sách doanh nghiệp pilot.
7. Bắt đầu spike S1 (codec) và S2 (renderer). Đây là hai rủi ro lớn nhất, phải chứng minh sớm nhất.
8. Bắt đầu Bác sĩ Font bản LISP, công cụ miễn phí đầu tiên ra công chúng.
