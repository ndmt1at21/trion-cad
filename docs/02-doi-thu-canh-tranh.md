# 02 — Phân tích đối thủ cạnh tranh

> Cập nhật 18/10/2026. Giá là giá niêm yết hoặc giá đại lý công bố, chưa gồm VAT nếu không ghi khác. Dấu "?" là chưa xác minh được.

## 1. Bản đồ cạnh tranh

```
                         ĐẮT ▲
                             │  AutoCAD (2.095 USD/năm)
                             │  BricsCAD Pro/BIM (710–1.176 €/năm)
       Lõi riêng / ODA       │  ZWCAD Pro (1.399 USD vĩnh viễn)
  (đầu tư R&D lớn, có 3D)    │  DraftSight Premium (599 USD/năm)
                             │  GstarCAD Pro · ARES (≥375 USD/năm)
 ────────────────────────────┼──────────────────────────────────────
                             │  progeCAD (540 USD vĩnh viễn)
   Lõi IntelliCAD (ITC)      │  CMS IntelliCAD (300–400 USD) · ActCAD (249–399 USD)
   (dùng chung lõi, chi phí  │  TurboCAD Designer/Deluxe (200–400 USD)
    thấp, khác biệt nhỏ)     │  ← khoảng trống: CAD bản địa hóa sâu cho VN + giá VNĐ
                             │  LibreCAD / QCAD / FreeCAD (miễn phí, hạn chế DWG)
                         RẺ  ▼
```

Có sáu nhóm đối thủ:
1. **Autodesk** (AutoCAD, AutoCAD LT, AutoCAD Web).
2. **Hãng có lõi riêng:** Octave BricsCAD, ZWSOFT ZWCAD, Gstarsoft GstarCAD, Graebert ARES và DraftSight (Dassault).
3. **Thành viên IntelliCAD:** progeCAD, ActCAD, CMS, CADian, 4M, Carlson…
4. **Mã nguồn mở:** LibreCAD, QCAD, FreeCAD.
5. **3D, đám mây, BIM nằm ngoài DWG 2D:** Fusion, Onshape, SolidWorks, Shapr3D, Rhino, SketchUp, Revit, ArchiCAD.
6. **Startup AI-native.**

---

## 2. Bảng so sánh chính

| Sản phẩm (phiên bản 2026) | Hãng | Giá | Nền tảng | Lõi DWG | API | 3D/BIM | AI | Tại VN |
|---|---|---|---|---|---|---|---|---|
| **AutoCAD 2027** | Autodesk (Mỹ) | 2.095 USD/năm; VN 39–48 tr/năm | Win, Mac, Web, Mobile | Gốc | LISP, ObjectARX, .NET, COM/VBA, APS | Có + 7 toolset | Assistant + MCP (xem trước), Smart Blocks | Nhiều đại lý |
| **AutoCAD LT 2027** | Autodesk | ~540 USD/năm; VN 10,9–12,8 tr/năm | Win, Mac | Gốc | LISP (từ 2024) | Không | Assistant, Smart Blocks | Nhiều đại lý |
| **Octave BricsCAD V26 (V27 ra mắt 20/10/2026)** | Octave (Bỉ), tách từ Hexagon 28/5/2026 | Lite 330 €/năm (bán gói 5 ghế); Pro 710 €/năm; BIM 1.060 €; Mech 998 €; Ultimate 1.176 €. Vĩnh viễn Pro ~1.752 USD | Win, Mac, Linux | ODA (thành viên sáng lập) | LISP (không chạy FAS/VLX), BRX ≈ ObjectARX, .NET, COM, VBA | Pro/BIM/Mech | BricsCAD Assist | Đại lý tự nhận |
| **ZWCAD 2026** (2027 beta bị lùi) | ZWSOFT (TQ, 688083.SH) | Std 899 / Pro 1.399 / MFG 1.699 USD vĩnh viễn; VN 21,6 / 35 / 42,5 tr | Win (Linux chỉ ở TQ) | Riêng (ZWCAD+ 2012, có thành phần ODA và Spatial) | LISP (Std); ZRX, .NET, VBA (Pro) | Pro | Smart Match, Similar Search | **VP Hà Nội, 12 đại lý** |
| **GstarCAD 2026** | Gstarsoft 浩辰 (TQ, 688657.SH) | Std ~599 USD vĩnh viễn (~239 USD/năm); Pro ~759–949 USD | Win | Riêng (thành viên ODA) | LISP, GRX, .NET, VBA ? | Pro | ? | **ViHoth (từ 2009)**, 4CTECH |
| **DraftSight 2026** | Dassault Systèmes (Pháp) | Professional 299 USD/năm; Premium 599 USD/năm | Win, Mac | Công nghệ Graebert | LISP, API ? | Premium | ? | Không thấy |
| **ARES Commander 2026** | Graebert (Đức) | Từ 375 USD/năm; có vĩnh viễn | Win, Mac, Linux + Kudo (web) / Touch (mobile) | Riêng | LISP, C++, .NET ? | 3D (ACIS ?) | ? | Không thấy |
| **progeCAD 2027** | ProgeSOFT (Thụy Sĩ) | 540 USD vĩnh viễn; Flex/NLM 665 USD; Mac (iCADMac) 925 USD | Win, Mac | IntelliCAD 14.1 | LISP (chạy được FAS/VLX), VLISP + VS Code | 3D | Dịch thuật AI | Không thấy |
| **ActCAD 2027** | Jytra (Ấn Độ) | Std 249 / Premium 399 USD vĩnh viễn; Ấn Độ 21.000 ₹ + GST | Win | IntelliCAD | LISP/FAS/VLX, DCL, SDS, .NET | Premium | — | Không thấy |
| **CMS IntelliCAD 15** | CMS (Mỹ) | PE 299,95 / PE Plus 399,95 USD vĩnh viễn; 249,95 USD/năm | Win | IntelliCAD 15 | LISP, VBA 7.1, .NET, IRX/DRX, SDS, IcAPI | PE Plus | Render AI (Gemini) | Không |
| **CADian 2026** | CADian (Hàn Quốc) | ? (vĩnh viễn và thuê bao) | Win | ? | LISP | Pro | Nhận dạng chữ PDF chạy cục bộ, AI-CE (dự toán) | Không |
| **TurboCAD 2026** | IMSI (Mỹ) | Designer 199,99 / Deluxe 399,99 / Pro 999,99 / Platinum 1.499,99 USD vĩnh viễn | Win, Mac | ODA (Teigha) | SDK, Ruby | ACIS | Copilot | Không |
| **nanoCAD 26** | Nanosoft AS (Na Uy) | Thuê bao | Win | ? | LISP, .NET, C++, JS, VBS, Python (COM) | Module | ? | Không |
| **LibreCAD / QCAD** | Mã nguồn mở (GPL) | Miễn phí (QCAD Pro trả phí thấp) | Win, Mac, Linux | DXF; DWG hạn chế | JS (QCAD) | 2D | — | — |
| **FreeCAD 1.1** (03/2026) | Mã nguồn mở (LGPL) | Miễn phí | Win, Mac, Linux | DXF; DWG qua ODA File Converter | Python | 3D tham số (OCCT) | — | — |

Nguồn: trang giá chính thức của các hãng và đại lý (xem cuối tài liệu).

---

## 3. Hồ sơ đối thủ quan trọng

### 3.1 Autodesk AutoCAD / AutoCAD LT (đối thủ "mặc định")
- **Sức mạnh:** chuẩn ngành, giáo trình, kỹ năng ghi trong CV, hệ sinh thái plugin (ObjectARX/.NET), toolset chuyên ngành, đám mây Forma, AI (Assistant, Smart Blocks).
- **Điểm yếu tại VN:** giá thuê bao cao và tăng đều, chỉ có named user, không có giá VNĐ chính thức. Autodesk đang cắt chiết khấu và chuyển sang bán trực tiếp, làm giảm động lực của đại lý.
- **Phản ứng có thể có:** khuyến mãi theo mùa (ASEAN từng tặng 2 tháng cho gói 1 năm vào 06–07/2025), chương trình "Autodesk for Small Business" (05/2026), siết kiểm toán bản quyền. Giáo dục vẫn miễn phí.

### 3.2 Octave BricsCAD — "người kế nhiệm AutoCAD" ở phương Tây
- Do Bricsys thành lập năm 2002 tại Ghent, ra bản đầu năm 2004, được Hexagon mua năm 2018 và thuộc Octave từ 2026. Khoảng 400 nghìn người dùng tại 110 quốc gia (nguồn thứ cấp).
- **Mạnh:** chạy 3 hệ điều hành, BIM và Mechanical tích hợp, BRX tương thích mã nguồn với ObjectARX (cần biên dịch lại), LISP có COM chạy cả trên Linux/Mac.
- **Yếu:** giá đang tăng (khoảng 6–10% năm 2026), chính sách vĩnh viễn bị siết (chỉ kích hoạt 1 máy từ 17/3/2025; từ 01/01/2026 chỉ nâng cấp được từ 3 phiên bản trước), không chạy được FAS/VLX, gần như không có mặt tại VN.
- **Bài học:** chi tiết lịch sử cần kiểm chứng thêm, nhưng BricsCAD được cho là khởi đầu từ IntelliCAD rồi chuyển sang lõi riêng trên ODA. Bricsys mua IP ràng buộc tham số của LEDAS năm 2011.

### 3.3 ZWSOFT ZWCAD — đối thủ trực tiếp số 1 tại Việt Nam
- Doanh thu 2025 đạt 893,4 triệu NDT (+0,6%), lãi ròng 20,7 triệu NDT (giảm 67,6%). **Doanh thu nước ngoài 274 triệu NDT (+35%)**, chiếm 30,7%. Năm 2024, mảng 2D CAD đạt 482 triệu NDT (+2,1%).
- **Tại VN:** có văn phòng Hà Nội, country manager người Việt, 12 đại lý, tổ chức sự kiện ZCON. Giá vĩnh viễn 21,6–42,5 triệu đ (chưa VAT).
- **Mạnh:** lõi riêng nhanh, giấy phép vĩnh viễn, nhiều tính năng "smart" (Smart Plot in hàng loạt theo khung, Smart Match, Smart Dimension, ràng buộc tham số với lõi riêng).
- **Yếu:** chỉ chạy Windows ngoài Trung Quốc, chưa bản địa hóa VN sâu (trang VN không nhắc TCVN hay font), lợi nhuận đang giảm mạnh. Chiến lược 2027 có vẻ bị lùi: ZWSOFT kéo dài vòng đời ZWCAD 2026.
- **Dự báo:** thị trường nội địa Trung Quốc chững lại nên ZWSOFT sẽ **đẩy mạnh Đông Nam Á**, và có khả năng giảm giá tại VN để giữ thị phần.
- **Bài học:** lịch sử công khai (Wikipedia IntelliCAD) cho thấy ZWCAD từng là sản phẩm dựa trên IntelliCAD trước khi ra ZWCAD+ (2012) với thành phần DWG từ ODA và 3D từ Spatial.

### 3.4 Gstarsoft GstarCAD
- Doanh thu 2025 đạt 332,1 triệu NDT (**+14,9%**), lãi ròng 50,8 triệu NDT. Tăng trưởng tốt, mạnh ở các thị trường mới nổi (công bố hiện diện tại 200 quốc gia và vùng lãnh thổ).
- **Tại VN:** ViHoth phân phối từ 2009, giá vĩnh viễn khoảng 600 USD.
- **Bài học:** một hãng tầm trung vẫn tăng trưởng hai chữ số nhờ giá tốt cộng kênh phân phối tại các thị trường mới nổi.

### 3.5 Graebert (ARES) và Dassault (DraftSight)
- Graebert là công ty gia đình ở Berlin, thành lập năm 1977. Họ cung cấp công nghệ cho DraftSight (Dassault) và cho phần bản vẽ của Onshape. ARES có bộ ba Commander (desktop), Kudo (web) và Touch (mobile).
- DraftSight từng miễn phí, chuyển sang thuê bao từ 2019 (299–599 USD/năm).
- Cả hai gần như không có mặt tại VN.

### 3.6 Nhóm IntelliCAD (ITC) — lối tắt có trả phí (TrionCAD không chọn vì định hướng tự xây)
- **ITC** là consortium phi lợi nhuận thành lập năm 1999 (Portland, Oregon). Họ cấp mã nguồn IntelliCAD cho thành viên thương mại để thành viên phát hành dưới thương hiệu riêng. IntelliCAD dùng nền ODA từ 2008.
- **Phí thành viên 2026** ([intellicad.org/membership-fees](https://www.intellicad.org/membership-fees)):
  - Thương mại, không phí bản quyền (royalty-free): **75.000 USD/năm**.
  - Thương mại có phí bản quyền: **38.000 USD/năm + 25 USD/giấy phép mới**.
  - Thành viên API: 1.000 USD/năm (không có mã nguồn, không được phân phối).
  - Nhóm chuyên đề: AI 7.500 USD/năm, BIM 6.500 USD/năm, Mobile 3.000 USD/năm, thành phần STEP/IGES của Open Cascade 1.800 USD năm đầu, 1.400 USD các năm sau.
- **IntelliCAD 15.0 (04/8/2026):**
  - Hoàn tất engine LISP mới: nạp được FAS/VLX từ bản 14.1, có `VLISP-COMPILE`.
  - Drawing Compare (xem trước).
  - **AI xem trước:** hỏi đáp về bản vẽ, chọn đối tượng, sinh và chạy LISP qua Claude, Google Antigravity hoặc ChatGPT.
  - Chỉ chạy Windows 10/11 64-bit.
- **Thành viên tiêu biểu:** progeCAD, ActCAD, CMS, CADian, 4M, Carlson, MicroSurvey, Leica, Trimble StabiCAD…
- **Bài học ActCAD (Ấn Độ):** một công ty ở thị trường mới nổi, dựa trên IntelliCAD và định giá 249–399 USD vĩnh viễn, đã bán **hơn 110.000 giấy phép tại hơn 106 quốc gia**, gồm hơn 200 cơ quan chính phủ và quốc phòng. Đây là tham chiếu gần nhất về **giá và cách tiếp cận thị trường mới nổi** cho TrionCAD.

### 3.7 Mã nguồn mở
- **LibreCAD** (GPLv2) và **QCAD** (GPLv3, bản Pro trả phí) mạnh ở 2D DXF nhưng yếu với DWG, thiếu nhiều tính năng chuyên nghiệp và không có LISP.
- **FreeCAD 1.1** (03/2026) là CAD 3D tham số trên OCCT, không phải phần mềm thay thế AutoCAD cho bản vẽ 2D.
- **LibreDWG** (GPLv3, bản 0.13.4 ngày 17/3/2026) vẫn chưa đủ trưởng thành cho sản phẩm thương mại mã nguồn đóng (xem [05](05-kien-truc-ky-thuat.md)).

### 3.8 3D, đám mây, BIM (không cạnh tranh trực tiếp ở 2D, nhưng chia sẻ ngân sách khách hàng)
Gồm Fusion, Onshape, SolidWorks (3DEXPERIENCE), Shapr3D, Plasticity, Rhino 8, SketchUp, Revit, ArchiCAD, Vectorworks và MicroStation. **Hàm ý:** TrionCAD cần **nhập và xem được** RVT, IFC, STEP và SKP ở mức đủ dùng, chứ không cạnh tranh về mô hình hóa 3D trong 3 năm đầu.

### 3.9 Startup AI-native và web-native
- Có thể kể Zoo (trước là KittyCAD, với Text-to-CAD và Design Studio), Adam, Leo AI, Snaptrude, Arcol, Motif, Hypar, MxCAD (SDK CAD web của Trung Quốc). Phần lớn tập trung vào 3D cơ khí hoặc BIM trên trình duyệt, **không** cạnh tranh trực tiếp ở mảng bản vẽ 2D DWG cho SME **[cần xác minh số vốn và mức độ thu hút người dùng của từng hãng]**.
- Autodesk trình diễn **Neural CAD** (mô hình nền tảng sinh B-rep) tại AU 2025 cho Fusion và Forma, cùng các MCP server (Revit, Fusion Data). Tại AU 2026, Claude đọc ngữ cảnh và thao tác trong Fusion qua MCP.
- **Hàm ý:** AI sẽ là lớp tương tác chuẩn của mọi phần mềm CAD. TrionCAD cần **mở API hoặc MCP sớm** và giữ dữ liệu bản vẽ sạch, có cấu trúc. Lợi thế bền vững nằm ở dữ liệu và quy trình bản địa (TCVN, tiếng Việt), không nằm ở mô hình AI.

---

## 4. Những bài học rút ra

1. **"Bắt đầu bằng IntelliCAD, trưởng thành bằng lõi riêng"** là con đường đã có tiền lệ: ZWCAD đi đúng như vậy, BricsCAD và GstarCAD được cho là cũng thế. Lõi thuê giúp ra thị trường nhanh; lõi riêng tạo khác biệt và giá trị doanh nghiệp. **TrionCAD đi thẳng vào lõi riêng**: tự xây trên mã nguồn mở, không trả phí bản quyền, chấp nhận ra thị trường chậm hơn.
2. **Giá vĩnh viễn là vũ khí** chống lại thuê bao của Autodesk. Tuy vậy, các hãng đang siết dần điều kiện (BricsCAD), nên điều khoản giấy phép rõ ràng và công bằng là một điểm bán hàng.
3. **Đến 2026, AI, so sánh bản vẽ, nhập RVT và PDF sang DWG đều đã là tính năng chuẩn.** Thiếu là bị loại. Có cũng chưa đủ để thắng.
4. **Tương thích LISP quyết định khả năng chuyển đổi.** Chạy được FAS/VLX (đã có ở IntelliCAD) là lợi thế lớn với người dùng Việt vốn dùng nhiều LISP biên dịch sẵn.
5. **Hiện diện địa phương quan trọng:** ZWSOFT đầu tư văn phòng và mạng đại lý tại VN. TrionCAD phải thắng bằng việc làm sâu hơn về bản địa hóa và dịch vụ, chứ không chỉ bằng giá.
6. **Không nên đánh nhau về 3D/BIM** với các hãng lớn trong giai đoạn đầu.

---

## 5. Khoảng trống và cơ hội cho TrionCAD

| Khoảng trống | Đối thủ đang làm | TrionCAD làm gì |
|---|---|---|
| **Font tiếng Việt** (TCVN3, VNI, SHX tự phát) | Không ai xử lý tự động; người dùng tự cài font hoặc chạy LISP | "Bác sĩ Font": tự nhận diện bảng mã khi mở tệp, hiển thị đúng ngay, chuyển sang Unicode bằng một lệnh, kèm bộ font SHX tương thích có bản quyền rõ ràng |
| **Mẫu TCVN** | Mỗi công ty tự làm | Bộ mẫu chuẩn TCVN (khung tên, layer, nét, chữ, kích thước, mặt cắt, ký hiệu) đi kèm bản cài |
| **Quy trình Việt** | Chủ yếu dùng LISP rời | Tích hợp sẵn: in hàng loạt theo khung tên, thống kê thép, tổng chiều dài và diện tích, đánh số, xuất Excel |
| **Giá theo sức mua VN** | Bán vĩnh viễn 15–35 triệu đ, trả trước một lần | Thuê bao 2–4 triệu đ/năm và gói vĩnh viễn giá thấp |
| **Hỗ trợ** | Email, đại lý | Zalo OA, video tiếng Việt, cộng đồng, hỗ trợ chuyển đổi tại chỗ |
| **Nền tảng cho phần mềm chuyên ngành Việt** | Không có | SDK, chương trình OEM và chia sẻ doanh thu cho Nova-TDN và các phần mềm kết cấu, dự toán |
| **"Make in Vietnam"** | Không có CAD nội | Chứng nhận sản phẩm số trong nước để hưởng ưu đãi đấu thầu |
| **Cộng tác DWG giá rẻ** | Chỉ AutoCAD 2027 (Checkout + Forma) | Giai đoạn 3–4: lịch sử phiên bản, chia sẻ, ghi chú, khóa ở mức đối tượng |
| **Mac/Linux giá rẻ** | BricsCAD, ARES (đắt) | Lõi .NET + Avalonia chạy Windows/macOS/Linux |

---

## 6. Kịch bản phản ứng của đối thủ và phương án đối phó

| Kịch bản | Xác suất | Đối phó |
|---|---|---|
| ZWCAD hoặc GstarCAD giảm giá mạnh tại VN | Cao | Không chạy đua giá đơn thuần. Tạo khác biệt bằng bản địa hóa, thuê bao rẻ, bản miễn phí và hệ sinh thái. Đóng gói giá trị (đào tạo, chuyển đổi) |
| Autodesk tung gói SME hoặc khuyến mãi lớn tại ASEAN | Trung bình | Nhấn vào điều khoản minh bạch, vĩnh viễn, dữ liệu tại VN và hỗ trợ tiếng Việt. Nhắm phân khúc nhạy giá mà Autodesk không phục vụ |
| Đối thủ sao chép "Bác sĩ Font" và mẫu TCVN | Trung bình | Chạy nhanh hơn, xây cộng đồng, đóng góp chuẩn mở. Hào lũy thật nằm ở hệ sinh thái đối tác chuyên ngành và dữ liệu |
| Thắt chặt bản quyền tạm lắng | Trung bình | Không phụ thuộc vào "nỗi sợ thanh tra". Bản miễn phí và giá thấp phải đủ hấp dẫn tự thân |

---

## Nguồn

[bricscad.octave.com/store](https://bricscad.octave.com/en-us/store/bricscad) · [Bricsys → Octave](https://bricscad.octave.com/news/bricsys-becomes-octave) · [BricsCAD V27](https://bricscad.octave.com/v27/launch) · [zwsoft.com pricing](https://www.zwsoft.com/product/zwcad/pricing) · [zwsoft.com/vn](https://www.zwsoft.com/vn/product/zwcad/pricing) · [ZCON Vietnam 2024](https://www.zwsoft.com/vn/news/events/zcon-vietnam-2024) · [gstarcad.vn](https://gstarcad.vn/gstarcad-pc8.html) · [IntelliCAD 15](https://www.intellicad.org/articles-and-press-releases/intellicad-15.0-released-ai-feature-previews-drawing-compare-and-a-completed-lisp-engine) · [ITC fees](https://www.intellicad.org/membership-fees) · [progecad.us/pricing](https://progecad.us/pricing) · [progeCAD 2027 PR](https://www.prweb.com/releases/progesoft-releases-progecad-2027-with-faster-core-new-compare-command-and-flexible-licensing-302795948.html) · [actcad.com](https://actcad.com/) · [CMS store](https://www.intellicadms.com/store) · [TurboCAD](https://www.imsidesign.com/products/turbocad) · [nanoCAD 26](https://nanocad.com/company/news/nanocad-26-release-smarter-tools-enhanced-precision-and-a-more-intuitive-experience/) · [DraftSight pricing (Capterra)](https://www.capterra.com/p/172808/DraftSight/pricing) · [ARES (Capterra)](https://www.capterra.com/p/164140/ARES-Commander/) · [FreeCAD 1.1](https://osarch.org/2026/03/25/freecad-1-1-released/) · Báo cáo thường niên 2025 của 中望软件 (688083) và 浩辰软件 (688657) (SSE).
