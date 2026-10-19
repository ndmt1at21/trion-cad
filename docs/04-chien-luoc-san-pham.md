# 04 — Chiến lược sản phẩm, định giá và đưa ra thị trường

## 1. Tầm nhìn, sứ mệnh, định vị

- **Tầm nhìn:** trở thành nền tảng CAD mặc định của kỹ sư và kiến trúc sư Việt Nam và Đông Nam Á. Mỗi người làm thiết kế đều có công cụ hợp pháp, nhanh, chính xác, vừa túi tiền, và **công nghệ lõi do người Việt làm chủ hoàn toàn**.
- **Sứ mệnh (đến v1.0):** thay thế AutoCAD LT cho công việc 2D hằng ngày của doanh nghiệp vừa và nhỏ Việt Nam. Giá bằng khoảng 1/4, chạy nhanh hơn trên cùng máy, và xử lý bản vẽ Việt tốt hơn.
- **Câu định vị:**
  > *Với các văn phòng thiết kế và nhà thầu Việt Nam đang trả quá nhiều cho AutoCAD hoặc đang dùng bản không bản quyền, **TrionCAD** là phần mềm CAD tương thích DWG với:*
  > - *lõi tự phát triển 100%, tận dụng tối đa CPU nhiều nhân và GPU, mở bản vẽ lớn trong vài giây;*
  > - *lệnh quen thuộc, chạy được LISP sẵn có;*
  > - *tự sửa lỗi font tiếng Việt, có sẵn mẫu TCVN;*
  > - *hỗ trợ tiếng Việt qua Zalo;*
  > - *giá chỉ bằng một phần nhỏ.*

## 2. Chiến lược cốt lõi: tự viết toàn bộ lõi

### 2.1 Quyết định
Nhà sáng lập quyết định:
1. **không chi tiền mua SDK hay giấy phép lõi**;
2. **lõi tự viết**;
3. **hiệu năng cao, logic chính xác, tận dụng tối đa phần cứng**.

Kiến trúc chi tiết ở [05-kien-truc-ky-thuat.md](05-kien-truc-ky-thuat.md).

| | Mua lõi IntelliCAD (ITC) | Mua ODA SDK + tự viết phần còn lại | **Tự viết toàn bộ lõi (đã chọn)** |
|---|---|---|---|
| Phí bản quyền lõi | 38.000 USD/năm + 25 USD/bản, hoặc 75.000 USD/năm | 2.250–18.000 USD/năm (năm đầu cao hơn) | **0 đ** (chỉ dùng thư viện hạ tầng miễn phí) |
| Sản phẩm miễn phí đầu tiên | — | — | Công cụ cho AutoCAD/ZWCAD (tháng 2–3); **TrionCAD Viewer** (khoảng tháng 16–17) |
| Sản phẩm có doanh thu đầu tiên | ~9 tháng | ~15–18 tháng | **Viewer Pro (khoảng tháng 18)**, Pro Early Access (khoảng tháng 25) |
| Ngang AutoCAD LT ở 2D (v1.0) | Ngay khi ra mắt | ~24–30 tháng | **~36 tháng (±4)** |
| Hiệu năng | Phụ thuộc lõi mua | Phụ thuộc lõi ODA | **Tối ưu được đến tận cùng** (bố cục dữ liệu, đa luồng, SIMD, GPU) |
| Sở hữu trí tuệ | Thấp | Trung bình (ngừng trả phí ODA thì mất quyền phân phối) | **100%** |
| Rủi ro lớn nhất | Phụ thuộc ITC, dễ thành hàng phổ thông | Phụ thuộc ODA | **Khối lượng lớn (~47 năm-người đến v1.0); chất lượng codec DWG; thời gian ra thị trường** |

### 2.2 Hệ quả cần chấp nhận (nói thẳng)
- Tiền không mất đi mà **chuyển sang lương kỹ sư**: khoảng 562 PM (~47 năm-người) đến v1.0. Tổng chi 36 tháng khoảng 89 tỷ đ, xem [06](06-lo-trinh-trien-khai.md).
- Doanh thu đến muộn, nên **phải gọi vốn** hoặc có nguồn tài trợ dài hơi.
- **Đổi lại:**
  - sở hữu toàn bộ công nghệ, nên giá trị doanh nghiệp cao nhất;
  - không phụ thuộc nhà cung cấp, không có phí bản quyền theo ghế (biên lợi nhuận gộp có thể ≥ 90%);
  - tự do làm đa nền tảng, web và đám mây;
  - hiệu năng do chính mình quyết định.

### 2.3 Cách bù cho việc ra thị trường muộn
1. **Tháng 2–3:** phát hành **công cụ miễn phí chạy trên AutoCAD/ZWCAD/BricsCAD** (Bác sĩ Font bản LISP, Thư viện TCVN) để xây cộng đồng và danh sách người dùng.
2. **Tháng 16–17:** **TrionCAD Viewer miễn phí.** Viewer chỉ cần đọc, hiển thị và in, nhưng đó lại là phần khó nhất. Hàng chục nghìn người dùng gửi bản vẽ lỗi qua nút "Gửi bản vẽ lỗi", nên **chất lượng codec được tôi luyện trước khi bán bản biên tập**.
3. **Tháng 18:** **Viewer Pro (trả phí, rẻ)** cho người chỉ cần xem, in và đo: in hàng loạt theo khung tên, xuất PDF theo CTB, so sánh bản vẽ, chuyển font hàng loạt, đo và ghi chú. Đây là doanh thu sớm.
4. **Tháng 25:** **Pro Early Access** (bản biên tập đủ lệnh P0) với giá ưu đãi trọn đời cho người dùng sớm.
5. **Tháng 36 (±4):** **v1.0.**
6. *(Tùy chọn)* Một nhóm dịch vụ nhỏ (tùy biến LISP, đào tạo CAD, chuẩn hóa bản vẽ cho doanh nghiệp) tạo dòng tiền và quan hệ khách hàng sớm.

## 3. Khách hàng mục tiêu

### 3.1 Beachhead
1. **Nhà thầu, ban quản lý dự án, chủ đầu tư, kỹ sư hiện trường** chỉ cần xem, in, đo, so sánh: dùng **Viewer** (miễn phí) và **Viewer Pro** (từ tháng 18).
2. **Tư vấn thiết kế xây dựng SME** (kết cấu, kiến trúc, M&E, quy mô 5–50 người) tại Hà Nội, TP.HCM, Đà Nẵng: dùng **Pro** (từ Early Access).

### 3.2 Chân dung người dùng (persona)

| Persona | Bối cảnh | Nỗi đau | TrionCAD giải quyết | Người quyết định mua |
|---|---|---|---|---|
| **Anh Minh**, kỹ sư hiện trường của nhà thầu | Chỉ xem, đo, in, ghi chú; thường không có mạng; laptop cũ | Phải cài AutoCAD chỉ để xem; lỗi font; máy chậm | **Viewer miễn phí**, chạy offline, mở nhanh trên máy yếu, sửa font. Viewer Pro để in hàng loạt | Trưởng phòng kỹ thuật |
| **Anh Tuấn**, kỹ sư kết cấu, công ty tư vấn 20 người, Hà Nội | AutoCAD crack, LISP thống kê thép, Excel | Sợ thanh tra; lỗi font; bản vẽ lớn chạy chậm | Pro giá rẻ, chạy được LISP, sửa font tự động, nhanh với bản vẽ lớn | Giám đốc công ty |
| **Chị Lan**, KTS nội thất tự do, TP.HCM, dùng MacBook | AutoCAD LT for Mac thuê bao, SketchUp | Giá; xuất PDF đẹp; mẫu khung tên | Pro giá rẻ, mẫu và block nội thất VN, có bản macOS (cùng đợt v1.0 hoặc ngay sau) | Chính chị Lan |
| **Chị Hà**, trưởng IT/hành chính, công ty FDI 200 người | Kiểm toán bản quyền, quản lý giấy phép | Chi phí AutoCAD cho cả công ty; cần hóa đơn VAT; triển khai hàng loạt | Viewer cho số đông, Pro/Doanh nghiệp (floating) cho người thiết kế, hóa đơn, SLA | Tổng giám đốc + IT |
| **Nam**, sinh viên ĐH Xây dựng | Học AutoCAD | Muốn kỹ năng "chuẩn ngành" | Bản Giáo dục miễn phí; lệnh giống AutoCAD nên kỹ năng dùng được ở cả hai | Nhà trường |
| **Ông Hùng**, giám đốc công ty phần mềm chuyên ngành (thiết kế đường) | Phần mềm viết bằng Visual C++/ARX chạy trên AutoCAD | Khách hàng phải mua AutoCAD; API của Autodesk đổi hằng năm | **SDK C/C++** có khái niệm gần ObjectARX; OEM; chia sẻ doanh thu | Ông Hùng |

## 4. Giá trị cốt lõi và khác biệt hóa

| So với | TrionCAD thắng ở | TrionCAD chưa thắng ở |
|---|---|---|
| **AutoCAD / LT** | Giá (khoảng 1/4 LT), có vĩnh viễn, có floating, Việt hóa sâu, hỗ trợ tiếng Việt, dữ liệu đặt tại VN; mục tiêu **nhanh hơn trên cùng máy** | Thương hiệu, toolset chuyên ngành, 3D, đám mây, hệ sinh thái plugin |
| **ZWCAD / GstarCAD** | Bản địa hóa (font, TCVN, LISP VN), thuê bao rẻ không phải trả trước 15–35 triệu, "Make in Vietnam" với lõi Việt 100%, Zalo | Độ trưởng thành (TrionCAD là lõi mới), 3D, độ phủ đại lý |
| **Bản crack** | Hợp pháp, an toàn (không mã độc), cập nhật, hỗ trợ, hóa đơn; Viewer và Giáo dục miễn phí | "Miễn phí" về tiền. Phải giữ giá thấp và quy trình chuyển đổi thật dễ |

**Sáu trụ khác biệt** (phải có từ bản v1.0; trụ 1, 2, 6 có ngay từ Viewer):
1. **Bác sĩ Font:** mở bản vẽ là chữ tiếng Việt hiển thị đúng. Một lệnh chuyển cả bản vẽ sang Unicode.
2. **Bộ chuẩn TCVN:** mẫu khung tên, layer, nét, chữ, kích thước, mặt cắt, ký hiệu, CTB (mục 7).
3. **Tương thích thói quen:** lệnh, viết tắt `.pgp` và LISP chạy như AutoCAD; công cụ "Chuyển từ AutoCAD sang".
4. **Công cụ quy trình Việt tích hợp sẵn:** in hàng loạt theo khung tên, tổng chiều dài và diện tích, đánh số, thống kê thép, xuất Excel, dán bảng từ Excel.
5. **Dịch vụ tận tay:** Zalo OA, video tiếng Việt, hỗ trợ chuyển đổi tại chỗ cho doanh nghiệp, hóa đơn VAT.
6. **Hiệu năng:** mở và thao tác bản vẽ lớn nhanh, mượt cả trên máy cũ. Công bố benchmark so với DWG TrueView trên cùng máy.

## 5. Dòng sản phẩm và lộ trình phát hành

| Phiên bản | Thời điểm | Đối tượng | Nội dung |
|---|---|---|---|
| **Công cụ miễn phí** (Bác sĩ Font bản LISP, Thư viện TCVN) | Tháng 2–3 | Người đang dùng AutoCAD/ZWCAD/BricsCAD | Xây cộng đồng, danh sách người dùng |
| **TrionCAD Viewer** (miễn phí) | Tháng 16–17 | Hiện trường, chủ đầu tư, kế toán, sinh viên | Mở DWG/DXF, xem layout, đo, bật/tắt layer, in và xuất PDF, Bác sĩ Font (hiển thị). Không sửa được |
| **TrionCAD Viewer Pro** | Tháng 18 | Nhà thầu, ban QLDA, bộ phận in ấn | In hàng loạt theo khung tên, PDF theo CTB, so sánh bản vẽ, chuyển font hàng loạt, ghi chú, xuất bảng thống kê |
| **TrionCAD Pro (Early Access)** | Tháng 25 | Người dùng sớm, SME | Bản biên tập đủ lệnh P0 ([03](03-chuc-nang-autocad.md) Phụ lục A), LISP lõi + DCL, lưu DWG 2018/2004 |
| **TrionCAD Giáo dục** (miễn phí) | Từ Early Access | Sinh viên, giảng viên, trường | Như Pro, chỉ dùng phi thương mại |
| **TrionCAD 1.0: Pro, Doanh nghiệp** | Tháng 36 (±4) | Mọi đối tượng | Ngang AutoCAD LT ở 2D (P1), Windows (+ macOS cùng đợt hoặc ngay sau). Doanh nghiệp có floating license, quản trị tập trung, SLA |
| **Nhà nước / Dự án** | Sau v1.0 | Cơ quan, DNNN | Site license, máy chủ giấy phép tại chỗ, hồ sơ "sản phẩm số trong nước" |
| **Gói chuyên ngành, SDK C/C++ cho đối tác** | Sau v1.0 | Kết cấu, giao thông, địa chính | Tự làm hoặc qua đối tác |

Tất cả là **một bản cài đặt duy nhất**. Giấy phép quyết định tính năng nào được mở.

## 6. Định giá và giấy phép

### 6.1 Bảng giá Việt Nam đề xuất (giả thuyết, kiểm định bằng khảo sát Van Westendorp ở Giai đoạn 0)

| Phiên bản | Thuê bao | Vĩnh viễn | So sánh |
|---|---|---|---|
| Viewer, Giáo dục | 0 đ | 0 đ | — |
| **Viewer Pro** | **690.000 đ/năm** | 1.990.000 đ | Rẻ hơn mua AutoCAD chỉ để xem và in |
| **Pro Early Access** | **1.490.000 đ/năm**, khóa giá trọn đời cho 1.000 khách đầu tiên, lên v1.0 miễn phí | — | — |
| **Pro (v1.0)** | **2.990.000 đ/năm** (hoặc 349.000 đ/tháng) | **7.990.000 đ** (kèm 12 tháng cập nhật; gia hạn cập nhật 1.990.000 đ/năm, không bắt buộc) | AutoCAD LT 10,9–12,8 triệu/năm (≈ 25%); ZWCAD Std 21,6 triệu vĩnh viễn (≈ 37%); GstarCAD Std ~15,8 triệu (≈ 50%) |
| **Doanh nghiệp** (floating, v1.0) | 3.990.000 đ/ghế/năm | 10.990.000 đ/ghế | Autodesk không còn giấy phép mạng |
| Nhà nước, dự án | Báo giá | Báo giá | Theo hồ sơ thầu |

- Theo luật hiện hành, sản phẩm phần mềm thuộc diện không chịu thuế GTGT **[cần xác minh với tư vấn thuế theo Luật thuế GTGT 2024]**.
- **Chiết khấu kênh:** đại lý được 25–35%.
- **Chương trình hợp thức hóa** (từ v1.0): giảm 20% khi chuyển cả công ty, kèm hỗ trợ gỡ phần mềm lậu.

### 6.2 Giá quốc tế (sau v1.0, theo sức mua)

| Nhóm thị trường | Pro / năm | Pro vĩnh viễn | Tham chiếu |
|---|---|---|---|
| A: VN, ID, PH, IN, Mỹ Latinh, châu Phi | 129 USD | 299 USD | ActCAD 249–399 USD vĩnh viễn |
| B: TH, MY, Đông Âu | 179 USD | 399 USD | CMS PE Plus 399,95 USD |
| C: Mỹ, EU, Nhật, Úc | 249 USD | 549 USD | DraftSight Pro 299 USD/năm, ARES 375 USD/năm, progeCAD 540 USD |

Trước v1.0 chỉ bán tại Việt Nam (Viewer Pro và Early Access). Sau v1.0 mở nhóm A, rồi đến B và C.

### 6.3 "Cam kết giấy phép công bằng" (dùng làm điểm bán hàng)
1. **Vĩnh viễn nghĩa là vĩnh viễn:** dùng mãi phiên bản đã mua; tự kích hoạt lại khi đổi máy (2 lần mỗi năm); kích hoạt offline.
2. **Hết thuê bao không khóa dữ liệu:** phần mềm chuyển về chế độ Viewer, vẫn mở, xem và in được.
3. **Không phạt khi giảm ghế.** Có thể chuyển giấy phép cho nhân viên mới.
4. **Người dùng Early Access** được nâng cấp lên v1.0 miễn phí và giữ giá ưu đãi.
5. **Telemetry chỉ khi người dùng đồng ý**, dữ liệu lưu tại Việt Nam.

### 6.4 Kinh tế đơn vị **[ước tính]**
- **Không có phí bản quyền lõi theo ghế.** Chi phí biến đổi chỉ gồm hỗ trợ, hạ tầng và phí thanh toán, nên biên lợi nhuận gộp mục tiêu ≥ 90%.
- ARPU tổng hợp sau v1.0 khoảng 2,6 triệu đ/ghế/năm. Viewer Pro khoảng 0,69 triệu đ/ghế/năm.
- Thời gian hoàn vốn CAC ≤ 6 tháng.

## 7. Gói bản địa hóa Việt Nam (đặc tả sản phẩm)

### 7.1 Bác sĩ Font (có từ Viewer)
- **Nhận diện tự động** bảng mã của từng đối tượng: text, mtext, attribute, dim, bảng, block lồng. Dựa trên tên font của style, đặc trưng byte và **chấm điểm âm tiết tiếng Việt**.
- **Hai chế độ:**
  - *Hiển thị đúng*: không sửa dữ liệu. Là mặc định.
  - *Chuyển đổi*: chuyển sang Unicode NFC; có xem trước, hoàn tác, chạy hàng loạt.
- **Bộ font SHX** tương thích tên (`vntime`, `vnarial`, `romansvn`…) **do TrionCAD tự thiết kế**, cùng font Unicode mã nguồn mở (SIL OFL) làm font thay thế.
- **Báo cáo** font thiếu, font đã thay, số đối tượng đã chuyển.

### 7.2 Bộ chuẩn TCVN

| Thành phần | Căn cứ | Ghi chú |
|---|---|---|
| Khổ giấy A0–A4, khung bản vẽ | TCVN 7285:2003 (ISO 5457) | Mẫu `.dwt` cho từng khổ |
| Khung tên: góc phải dưới; các ô đơn vị thiết kế, công trình, tên bản vẽ, ký hiệu "KT 1/18", giai đoạn, tỉ lệ, chữ ký, số tờ | **TCVN 5571:2012**, TCVN 5896:2012 (ISO 9431) | Block có attribute và field tự điền |
| Nét vẽ: loại nét; bề rộng 0,13 / 0,18 / 0,25 / 0,35 / 0,5 / 0,7 / 1,0 / 1,4 / 2,0 mm; tỉ lệ đậm:mảnh ≥ 2:1 | TCVN 8-20:2002 (ISO 128-20), TCVN 8-21:2005, **TCVN 5570:2012**, TCVN 5672:2012 (nét đậm 0,5–0,7 mm) | Tệp `.lin`, layer mẫu, CTB |
| Chữ và chữ số | TCVN 7284-0:2003 (ISO 3098), **TCVN 4608:2012** | Text style chuẩn |
| Ký hiệu vật liệu, mặt cắt | TCVN 8-50:2005 (ISO 128-50) | Thư viện hatch `.pat` |
| Ký hiệu bộ phận nhà, cấu kiện, phòng | TCVN 4614:2012, 5686:2012, 6003-1/-2:2012, 6077:2012, 4607:2012 | Thư viện block |
| Kết cấu BTCT, thể hiện cốt thép, thép, gỗ | TCVN 5572:2012, 6084:2012, 4613:2012, 4610:2012 | Block và công cụ thống kê thép |
| Đường ống, cấp thoát nước, thông gió, chiếu sáng | TCVN 5422:2012, 5673:2012, 3989:2012, 4318:2012, 5681:2012 | Gói M&E |
| Ghi kích thước | ISO 129 và TCVN tương ứng **[xác minh số hiệu]** | Dimstyle mẫu theo tỉ lệ |

Nguồn danh mục: [Quyết định 3621/QĐ-BKHCN](https://hethongphapluat.com/quyet-dinh-3621-qd-bkhcn-nam-2012-cong-bo-tieu-chuan-quoc-gia-do-bo-truong-bo-khoa-hoc-va-cong-nghe-ban-hanh.html). Nên mua bản chính thức các tiêu chuẩn (chi phí nhỏ) và mời giảng viên ĐH Xây dựng, ĐH Kiến trúc thẩm định.

### 7.3 Ngôn ngữ, tài liệu, công cụ
- Giao diện tiếng Việt chuẩn thuật ngữ ngành (có bảng thuật ngữ Anh–Việt); lệnh giữ tên tiếng Anh.
- Trợ giúp, video và sách "Chuyển từ AutoCAD sang TrionCAD trong 1 ngày".
- Bộ LISP tiện ích VN ([03](03-chuc-nang-autocad.md) §29).
- Sau v1.0: hệ tọa độ VN-2000, đọc DGN địa chính.

## 8. Đưa ra thị trường (Go-to-market)

### 8.1 Tháng 0–16: xây cộng đồng trước khi có sản phẩm
1. **Công cụ miễn phí:** Bác sĩ Font (LISP cho AutoCAD/ZWCAD/BricsCAD) và Thư viện TCVN. Khi tải phải đăng ký bằng Zalo hoặc email. Mục tiêu: 15.000 người đăng ký vào tháng 15.
2. **Nội dung:** YouTube và TikTok tiếng Việt (mẹo CAD, sửa lỗi font); nhật ký phát triển công khai ("xây CAD Việt từ con số 0") với video benchmark tốc độ để tạo niềm tin.
3. **Cộng đồng:** hợp tác với diễn đàn CADViet và quản trị viên các nhóm Facebook/Zalo; chương trình "Tác giả LISP" (ghi công, chia doanh thu).
4. **Pilot:** 30–50 doanh nghiệp đóng góp bản vẽ cho kho kiểm thử và **chạy kịch bản tham chiếu trên AutoCAD của họ** ([05](05-kien-truc-ky-thuat.md) §14), đổi lại được dùng miễn phí 2 năm.
5. **Giáo dục:** MoU với 3–5 trường (ĐH Xây dựng Hà Nội, ĐH Kiến trúc Hà Nội và TP.HCM, ĐH Bách khoa, ĐH GTVT…): tuyển thực tập sinh C++ và CAD, cùng soạn giáo trình.

### 8.2 Ra mắt theo ba nấc
| Nấc | Thời điểm | Hoạt động |
|---|---|---|
| Viewer | Tháng 16–17 | Ra mắt trực tuyến, công bố benchmark tốc độ, chiến dịch "gửi bản vẽ khó nhất của bạn" |
| Viewer Pro | Tháng 18 | Bán tự phục vụ (VNPay/MoMo/chuyển khoản, hóa đơn điện tử); nhắm nhà thầu và ban QLDA |
| Pro Early Access → v1.0 | Tháng 25 → 36 | Ưu đãi trọn đời cho người dùng sớm; ra mắt v1.0 có sự kiện, chương trình hợp thức hóa, mạng đại lý |

### 8.3 Kênh tăng trưởng (sau v1.0)
| Kênh | Đối tượng | Cách làm |
|---|---|---|
| Tự phục vụ | Cá nhân, SME nhỏ | Viewer → dùng thử 30 ngày → thanh toán online |
| Bán hàng nội bộ (Zalo, điện thoại) | SME 5–50 ghế | Demo, dùng thử có hướng dẫn, hợp thức hóa |
| Đại lý | Toàn quốc | Chiết khấu 25–35%, chứng nhận kỹ thuật |
| Doanh nghiệp, Nhà nước | FDI, DNNN, cơ quan | Hồ sơ "sản phẩm số trong nước", thí điểm theo dự án |
| Đối tác chuyên ngành | Giao thông, kết cấu, dự toán | SDK C/C++, OEM (mục 9) |

### 8.4 Hỗ trợ
- **Kênh:** Zalo OA (kênh chính), hotline, email, hỗ trợ từ xa, cơ sở tri thức, diễn đàn.
- **SLA:** Pro trong 1 ngày làm việc; Doanh nghiệp trong 4 giờ làm việc; Nhà nước theo hợp đồng.

## 9. Hệ sinh thái đối tác
1. **Phần mềm chuyên ngành Việt** (giao thông như Nova-TDN, kết cấu, dự toán, địa chính):
   - **SDK C/C++** có khái niệm gần ObjectARX nên plugin Visual C++ port được;
   - hỗ trợ kỹ thuật, OEM "TrionCAD + phần mềm chuyên ngành", chia sẻ doanh thu.

   **Đây là hào lũy dài hạn quan trọng nhất.**
2. **Tác giả LISP và nội dung:** chợ tiện ích (sau v1.0), tác giả hưởng 70% doanh thu.
3. **Trường học:** giáo trình, chứng chỉ, thực tập (cũng là nguồn tuyển kỹ sư).
4. **Đại lý phần mềm.**
5. **Nhà sản xuất và tích hợp phần cứng:** cài sẵn bản Viewer.

## 10. Thương hiệu và pháp lý thương hiệu
- "TrionCAD" là **tên làm việc**. Cần tra cứu và đăng ký nhãn hiệu (Cục SHTT nhóm 9 và 42; Madrid cho ASEAN).
- Không dùng "AutoCAD", "ACAD" hay hình ảnh giống Autodesk. "DWG" chỉ dùng để mô tả định dạng ([05](05-kien-truc-ky-thuat.md) §15).
- Không quảng cáo "tương thích 100%". Dùng số đo thật (tỷ lệ mở thành công trên kho kiểm thử, benchmark tốc độ).
- Ghi chú nhãn hiệu: "AutoCAD® là nhãn hiệu của Autodesk, Inc."

## 11. Mở rộng quốc tế (sau v1.0)
1. Indonesia, Philippines (qua đại lý; tiếng Anh, Bahasa; tiêu chuẩn địa phương).
2. Thái Lan, Malaysia.
3. Ấn Độ, Mỹ Latinh, châu Phi.

## 12. Chỉ số đo lường

| Nhóm | Chỉ số | Mục tiêu |
|---|---|---|
| **North Star** | Số "ngày làm việc CAD" mỗi tuần (Viewer và Pro) | Tăng 10% mỗi tháng sau khi ra Viewer |
| Cộng đồng trước sản phẩm | Người đăng ký công cụ miễn phí | 15.000 vào tháng 15 |
| Viewer | Người dùng hoạt động hằng tháng | 10.000 vào tháng 20; 30.000 vào tháng 30 |
| Chuyển đổi | Viewer → Viewer Pro; Viewer Pro → Pro | ≥ 5%; ≥ 15% |
| Giữ chân | Tỷ lệ khách rời bỏ theo năm; NRR | ≤ 15%; ≥ 100% |
| Chất lượng | Mở DWG thành công; phiên không crash | ≥ 99,5%; ≥ 99,7% |
| Hiệu năng | Benchmark so với DWG TrueView trên cùng máy | Nhanh hơn ở ≥ 80% bài đo |
| Hài lòng | NPS | ≥ 40 |

## 13. Hào lũy theo thời gian
| Thời điểm | Hào lũy |
|---|---|
| Đến tháng 18 | Cộng đồng (công cụ miễn phí, Viewer), bản địa hóa VN, kho bản vẽ VN lớn nhất |
| Đến v1.0 | **Lõi tự viết 100%**: hiệu năng, độ chính xác, sở hữu trí tuệ; người dùng Early Access; trường học |
| Sau v1.0 | Hệ sinh thái phần mềm chuyên ngành Việt trên SDK; đa nền tảng; đám mây và cộng tác; AI theo TCVN; thương hiệu khu vực |
