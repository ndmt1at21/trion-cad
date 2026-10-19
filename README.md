# TrionCAD — Kế hoạch dự án

> Phần mềm CAD tương thích DWG với **lõi tự viết 100%**: hiệu năng cao, logic chính xác, tận dụng tối đa sức mạnh phần cứng. Giá phù hợp người dùng Việt Nam và các thị trường mới nổi. Mục tiêu v1.0 là thay thế AutoCAD LT cho công việc 2D hằng ngày.

**Trạng thái:** Giai đoạn 0, đang lập kế hoạch. Tài liệu lập ngày 18/10/2026. "TrionCAD" là tên làm việc, cần tra cứu nhãn hiệu trước khi dùng chính thức.

## Tóm tắt điều hành

**Cơ hội**
- Dòng AutoCAD và AutoCAD LT đạt **1,787 tỷ USD** trong FY2026 (+14%), phần lớn nhờ tăng giá và cắt chiết khấu.
- Ở Việt Nam, AutoCAD LT có giá **11–12,8 triệu đ/năm**, AutoCAD **39–48 triệu đ/năm**. So với thu nhập, gánh nặng cao gấp khoảng 10–20 lần ở Mỹ (BSA: 78% phần mềm không bản quyền năm 2015).
- **2025–2026 là điểm gãy pháp lý:** Nghị định 341/2025/NĐ-CP; vụ khởi tố hình sự đầu tiên về dùng phần mềm lậu (06/2026); Autodesk siết cảnh báo bản quyền trong phần mềm.
- Luật Công nghiệp công nghệ số có hiệu lực từ 01/01/2026; sản phẩm số trong nước được ưu đãi trong đấu thầu. **Chưa có phần mềm CAD nào có lõi do người Việt làm chủ.**
- Nỗi đau riêng của người Việt chưa ai giải quyết trọn vẹn: font TCVN3/VNI/SHX, mẫu TCVN, LISP cộng đồng, phần mềm chuyên ngành chỉ chạy trên AutoCAD.

**Thách thức**
- Thị trường đông và giá đã thấp: ZWCAD có văn phòng ở Hà Nội; ngoài ra có GstarCAD, BricsCAD, DraftSight, ARES; ActCAD bán 249 USD vĩnh viễn.
- Tương thích DWG là rào cản số 1.
- **Tự viết toàn bộ lõi là con đường tốn công nhất:** khoảng 47 năm-người đến v1.0.

**Chiến lược đã chọn**
1. **Không mua SDK hay giấy phép lõi. Lõi tự viết bằng C++20:**
   - codec DWG/DXF (sinh code từ schema);
   - kernel hình học 2D chính xác (predicate bền vững, giữ nguyên cung/spline);
   - regen (MTEXT, kích thước, hatch…);
   - renderer GPU trên Qt RHI;
   - lệnh và engine LISP.
   
   Chỉ dùng thư viện **hạ tầng phổ thông miễn phí** (Qt LGPL, FreeType, HarfBuzz, zlib, PDFium để đọc PDF, libsodium, Crashpad). Chi tiết ở [05 §2](docs/05-kien-truc-ky-thuat.md).
2. **Tận dụng tối đa phần cứng:**
   - đọc DWG và regen song song trên mọi nhân;
   - SIMD (AVX2/AVX-512/NEON);
   - GPU qua D3D12/Vulkan/Metal kèm compute, instancing block, BVH, đường cong giải tích trong shader;
   - tự thích ứng để chạy mượt cả trên máy văn phòng cũ.
3. **Bù cho thời gian ra thị trường dài bằng ba nấc sản phẩm:**
   - công cụ miễn phí cho người dùng AutoCAD (tháng 2–3);
   - **TrionCAD Viewer miễn phí (tháng 16–17)**;
   - **Viewer Pro trả phí (tháng 18)**, sau đó **Pro Early Access (tháng 25)** và **v1.0 (tháng 36 ±4)**.
4. **Khác biệt hóa:** Bác sĩ Font, bộ chuẩn TCVN, LISP tương thích, in hàng loạt theo khung tên, hỗ trợ qua Zalo, **tốc độ** (công bố benchmark).
5. **Giá v1.0:** Pro **2,99 triệu đ/năm** hoặc **7,99 triệu đ vĩnh viễn**, bằng khoảng 1/4 AutoCAD LT. Không có phí bản quyền theo ghế, nên biên lợi nhuận gộp ≥ 90%.

**Con số kế hoạch (kịch bản cơ sở, cần kiểm chứng)**
- Đội ngũ: 6 → 18 → 24 → 32 → 38 người (tháng 3, 12, 18, 27, 36), trong đó 19 kỹ sư C++.
- Tổng chi 36 tháng ≈ **89 tỷ đ (~3,4 triệu USD)**, **phí bản quyền lõi 0 đ**.
- Thâm hụt ròng ≈ **83 tỷ đ (~3,1 triệu USD)**.
- Hòa vốn khoảng tháng 50–60.
- Gọi vốn: pre-seed (T0–3) → seed (T9–12) → Series A (T20–24).
- Có **phương án tinh gọn**: 8–10 kỹ sư, v1.0 khoảng tháng 54–60.

## Bộ tài liệu

| # | Tài liệu | Nội dung |
|---|---|---|
| 01 | [Nghiên cứu thị trường](docs/01-nghien-cuu-thi-truong.md) | Thị trường toàn cầu, Autodesk (tài chính, giá, cấp phép), thị trường VN (giá, bản quyền, chính sách, kênh, quy mô), phân khúc, SWOT |
| 02 | [Đối thủ cạnh tranh](docs/02-doi-thu-canh-tranh.md) | So sánh hơn 15 sản phẩm, hồ sơ đối thủ chính, bài học, khoảng trống, kịch bản phản ứng |
| 03 | [Chức năng AutoCAD và ma trận ưu tiên](docs/03-chuc-nang-autocad.md) | Kiểm kê toàn bộ nhóm chức năng AutoCAD 2027 kèm lệnh, xếp hạng P0–P4, danh sách lệnh nghiệm thu |
| 04 | [Chiến lược sản phẩm và đưa ra thị trường](docs/04-chien-luoc-san-pham.md) | Quyết định tự viết và hệ quả, persona, ba nấc sản phẩm, bảng giá, gói bản địa hóa VN, go-to-market, đối tác, chỉ số |
| 05 | [Kiến trúc kỹ thuật](docs/05-kien-truc-ky-thuat.md) | Ranh giới "tự viết", ngân sách hiệu năng và độ chính xác, codec DWG, kernel hình học, regen, renderer, **tận dụng phần cứng**, LISP, SDK, kiểm thử, pháp lý, ước lượng khối lượng, spike |
| 06 | [Lộ trình triển khai](docs/06-lo-trinh-trien-khai.md) | Kế hoạch 90 ngày, các giai đoạn và mốc, cổng G0–G3, đội ngũ, ngân sách, vốn, phương án tinh gọn, sổ rủi ro |
| 07 | Đặc tả chức năng cho thiết kế UI: [1. Khung ứng dụng](docs/07-dac-ta-ui-1-khung-ung-dung.md) · [2. Palette và hộp thoại](docs/07-dac-ta-ui-2-bang-va-hop-thoai.md) · [3. Tính năng VN, Viewer, hệ thống](docs/07-dac-ta-ui-3-tinh-nang-vn-va-he-thong.md) | 172 màn hình và thành phần có mã UI-xxx. Mỗi mục ghi: mở từ đâu, thành phần, hành vi, trạng thái, phiên bản phát hành. Kèm ribbon đầy đủ (cả tab Mở rộng), prompt của các lệnh P0/P1, tương tác trên vùng vẽ, thanh trạng thái, phím tắt, khác biệt macOS/Linux, tên lệnh của tính năng riêng, công cụ miễn phí, danh mục màn hình, các đợt thiết kế, danh sách kiểm, phạm vi chưa đặc tả |
| 08 | [Phân rã task cho agent](docs/08-phan-ra-task/README.md) | Task lõi và UI vừa sức một agent, mỗi task có đầu vào, đầu ra, tiêu chí nghiệm thu đo được. Kèm định nghĩa hoàn thành chung (hiệu năng, tận dụng phần cứng, độ trễ, độ đúng), task neo giữa các module, thứ tự theo mốc |

## Quyết định cần chốt trong 90 ngày

| Quyết định | Hạn | Căn cứ |
|---|---|---|
| Tiếp tục mở rộng đội theo kế hoạch hay điều chỉnh | Cổng G0, cuối tháng 3 | Kết quả spike S1–S6 (codec, renderer, hình học, Bác sĩ Font, LISP, IME), xem [06 §2.2](docs/06-lo-trinh-trien-khai.md) |
| Phương án vốn: chuẩn (gọi vốn) hay tinh gọn | Tuần 8 | Khả năng gọi vốn; [06 §8.3](docs/06-lo-trinh-trien-khai.md) |
| Tên và thương hiệu | Tuần 2 | Kết quả tra cứu nhãn hiệu |
| Khung giá | Tuần 10 | Khảo sát giá (Van Westendorp) trên hơn 500 người |

## Việc cần làm ngay

Xem [06 §11](docs/06-lo-trinh-trien-khai.md):
- thành lập công ty;
- tuyển CTO và 3 kỹ sư C++ cao cấp;
- mua máy benchmark;
- thuê luật sư sở hữu trí tuệ;
- thu thập kho DWG;
- bắt đầu spike codec và renderer;
- phát hành Bác sĩ Font miễn phí.

## Quy ước tài liệu

- Mỗi số liệu đều có nguồn. Mục **[ước tính]** là giả định của nhóm lập kế hoạch; mục **[cần xác minh]** là thông tin chưa đối chiếu được với nguồn gốc. Cả hai phải được kiểm chứng ở Giai đoạn 0.
- Quyết định kỹ thuật mới được ghi thành ADR trong `docs/adr/`. Đặc tả hành vi lệnh nằm trong `docs/specs/`.
- AutoCAD® là nhãn hiệu của Autodesk, Inc. Các nhãn hiệu khác thuộc chủ sở hữu tương ứng.
