# 07.1 — Đặc tả chức năng cho thiết kế UI: Khung ứng dụng và tương tác chung

> Bộ đặc tả UI gồm 3 phần:
> 1. **Khung ứng dụng và tương tác chung** (tài liệu này)
> 2. [Palette, bảng và hộp thoại](07-dac-ta-ui-2-bang-va-hop-thoai.md)
> 3. [Tính năng Việt Nam, Viewer, tùy biến và hệ thống](07-dac-ta-ui-3-tinh-nang-vn-va-he-thong.md), kèm danh mục toàn bộ màn hình
>
> Tài liệu mô tả **chức năng**: có gì, mở từ đâu, hành xử ra sao, có những trạng thái nào. Hình ảnh do người thiết kế quyết định.
> - Danh sách lệnh đầy đủ và mức ưu tiên lấy từ [03-chuc-nang-autocad.md](03-chuc-nang-autocad.md).
> - Lịch phát hành lấy từ [04](04-chien-luoc-san-pham.md) và [06](06-lo-trinh-trien-khai.md).
> - Quy ước tương tác bám theo thói quen AutoCAD để người dùng chuyển sang không phải học lại. **Biểu tượng, hình khối và văn bản trợ giúp phải tự thiết kế**, không sao chép của Autodesk.

## 0. Cách dùng tài liệu

### 0.1 Ký hiệu phát hành

| Ký hiệu | Phát hành | Thời điểm |
|---|---|---|
| **CC** | Công cụ miễn phí chạy trong AutoCAD/ZWCAD/BricsCAD: Bác sĩ Font và Thư viện TCVN bản LISP (phần 3 §7) | Tháng 2–3 |
| **V** | TrionCAD Viewer (miễn phí) | Tháng 16–17 |
| **VP** | TrionCAD Viewer Pro | Tháng 18 |
| **EA** | TrionCAD Pro Early Access (bản biên tập) | Tháng 25 |
| **1.0** | TrionCAD 1.0 (Pro, Doanh nghiệp) | Tháng 36 (±4) |
| **Sau** | Sau v1.0 | — |

Mức ưu tiên P0–P4 dùng như trong [03](03-chuc-nang-autocad.md).

### 0.2 Mẫu mô tả một thành phần

Mỗi màn hình, palette hay hộp thoại có **mã** (ví dụ `UI-C03`) và gồm các mục:
- **Mở từ:** lệnh, ribbon, menu, phím tắt, chuột phải.
- **Dạng:** palette dock được, hộp thoại modal, tab ngữ cảnh, popup, banner…
- **Thành phần:** vùng, trường, nút.
- **Hành vi:** kiểm tra hợp lệ, xem trước, mặc định.
- **Trạng thái:** rỗng, đang tải, lỗi, bị khóa theo phiên bản…
- **Phát hành.**

Mục nào không ghi **Mở từ** thì mở bằng lệnh ghi trong tiêu đề (gõ được trên dòng lệnh), từ ribbon (§7) và từ thanh menu cổ điển (§7.12). Tên lệnh của các tính năng riêng TrionCAD nằm ở phần 3 §0.

### 0.3 Thứ tự thiết kế đề xuất
0. **Công cụ miễn phí** (phần 3 §7): chỉ gồm vài hộp thoại DCL, nhưng ra mắt tháng 2–3 nên cần xong khoảng tháng 1–2.
1. **Khung ứng dụng + Viewer** (phần 1 và mục Viewer ở phần 3). Viewer ra mắt tháng 16–17, nên phải thiết kế xong khoảng tháng 8–10 để kịp kỹ thuật làm.
2. **Viewer Pro:** in hàng loạt theo khung tên, so sánh bản vẽ, ghi chú (markup), chuyển font hàng loạt.
3. **Bản biên tập EA:** lệnh vẽ/sửa, palette thuộc tính, layer, style, chú thích, block.
4. **1.0:** phần còn lại của P1. Sau đó là P2 trở đi.

## 1. Nguyên tắc UX

1. **Bàn phím trước, chuột đầy đủ.**
   - Mọi lệnh đều gõ được.
   - Giữ nguyên thói quen AutoCAD: tên lệnh, viết tắt, phím F1–F12, Enter/Space để lặp lệnh, chuột phải để xác nhận.
2. **Không chặn người dùng.**
   - Hạn chế hộp thoại modal; ưu tiên palette, tab ngữ cảnh và xem trước trực tiếp.
   - Thao tác dài (in hàng loạt, chuyển font, mở tệp lớn) chạy nền, có tiến trình và nút hủy.
3. **Phản hồi tức thì.**
   - Đổi trạng thái giao diện ≤ 100 ms; xem trước trong lệnh ≤ 16 ms (ngân sách ở [05](05-kien-truc-ky-thuat.md) §1).
   - Animation ≤ 150 ms, tắt được, và không bao giờ làm chậm thao tác.
4. **Không làm mất dữ liệu.**
   - Mọi thao tác hoàn tác được.
   - Hành động phá hủy cần xác nhận và nêu rõ số lượng ("Xóa 1.250 đối tượng?").
   - Có tự lưu và phục hồi sau sự cố.
5. **Tiếng Việt trước.**
   - Nhãn tiếng Việt rõ nghĩa; giữ thuật ngữ CAD người Việt quen dùng (Layer, Block, Layout…) theo bảng thuật ngữ ở §13.4.
   - **Tên lệnh tiếng Anh luôn hiện kèm** (trong tooltip và dòng lệnh) để người dùng học được và tra được tài liệu AutoCAD.
6. **Vùng vẽ là trung tâm.**
   - Vùng vẽ chiếm ≥ 75% diện tích ở 1920×1080 khi palette tự ẩn hoặc bật Clean Screen; khoảng 60% khi palette Thuộc tính dock mở sẵn (mặc định ở ≥ 1920 px, §3.2).
   - Thu gọn được toàn bộ giao diện (Clean Screen).
   - Dùng tốt ở **1366×768**, độ phân giải laptop phổ biến tại Việt Nam.
7. **Nhất quán.**
   - Chỉ có một bộ chọn màu, một bộ chọn linetype, một kiểu ô nhập số dùng cho mọi nơi.
   - Ô nhập số chấp nhận đơn vị và biểu thức, có nút "lấy từ bản vẽ" (pick).
8. **Minh bạch về phiên bản.** Tính năng chưa có trong phiên bản hiện tại hiện biểu tượng khóa kèm lý do, không ẩn hoàn toàn. Riêng giao diện Viewer được giản lược (§2).
9. **Nhẹ cho máy yếu.** Giao diện không tốn CPU/GPU khi rảnh; có tùy chọn "Giao diện nhẹ" tắt bóng đổ, làm mờ, hiệu ứng.

## 2. Phiên bản và cổng tính năng trong UI

| Khả năng | Viewer | Viewer Pro | Pro (EA → 1.0) | Doanh nghiệp | Giáo dục |
|---|---|---|---|---|---|
| Mở, xem DWG/DXF; layout, viewport; zoom/pan | ✓ | ✓ | ✓ | ✓ | ✓ |
| Đo (khoảng cách, diện tích, góc, tọa độ) | ✓ | ✓ | ✓ | ✓ | ✓ |
| Bật/tắt layer **tạm thời** (không lưu vào tệp) | ✓ | ✓ | ✓ (lưu được) | ✓ | ✓ |
| In và xuất PDF từng layout | ✓ | ✓ | ✓ | ✓ | ✓ |
| Bác sĩ Font: hiển thị đúng | ✓ | ✓ | ✓ | ✓ | ✓ |
| Bác sĩ Font: chuyển đổi và lưu bản sao, chạy hàng loạt | — | ✓ | ✓ | ✓ | ✓ |
| Markup (đám mây, mũi tên, chữ…) lưu ra tệp ghi chú riêng | — | ✓ | ✓ | ✓ | ✓ |
| In hàng loạt theo khung tên; so sánh bản vẽ; đếm block; xuất bảng thống kê | — | ✓ | ✓ | ✓ | ✓ |
| Sửa bản vẽ, lưu DWG/DXF | — | — | ✓ | ✓ | ✓ |
| LISP, script, plugin | — | — | ✓ | ✓ | ✓ |
| Giấy phép floating, cổng quản trị, cài đặt hàng loạt | — | — | — | ✓ | — |

**Hành vi cổng tính năng:**
- **Ribbon:** nút của tính năng chưa có hiện huy hiệu khóa nhỏ ("Pro" hoặc "Viewer Pro"). Bấm vào sẽ mở **trang nâng cấp** (UI-L04, phần 3).
- **Dòng lệnh:** gõ một lệnh không có trong phiên bản thì hiện: *"Lệnh LINE có trong TrionCAD Pro. [Dùng thử 30 ngày] [Tìm hiểu]"*. Không hiện lỗi khó hiểu.
- **Viewer** dùng **ribbon giản lược riêng** (UI-M01, phần 3), không hiển thị toàn bộ ribbon kèm khóa, để giao diện gọn.
- **Dùng thử:** thanh trạng thái hiện huy hiệu "Dùng thử: còn 12 ngày". Nhắc ở các mốc 7, 3 và 1 ngày. Hết hạn thì phần mềm chuyển về Viewer, không khóa dữ liệu.
- **Giáo dục:** thanh tiêu đề có huy hiệu "Giáo dục". Không chèn dấu hay watermark vào bản vẽ.

## 3. Bố cục cửa sổ chính (UI-A01)

```
┌──────────────────────────────────────────────────────────────────────────────────────────┐
│ [≡ TrionCAD] [QAT: Mới Mở Lưu Lưu-thành ↶ ↷ In ▾]  Banve-01.dwg* — TrionCAD Pro  [🔍 Tìm lệnh…] [👤] [?] │ ← Thanh tiêu đề
├──────────────────────────────────────────────────────────────────────────────────────────┤
│ Trang chủ │ Chèn │ Chú thích │ Hiển thị │ Quản lý │ Xuất │ Công cụ VN │ Mở rộng │ [ngữ cảnh]  │ ← Ribbon
│ ┌Vẽ──────┐┌Sửa──────────┐┌Chú thích┐┌Layer──────┐┌Block┐┌Thuộc tính┐┌Tiện ích┐┌Clipboard┐ │
├──────────────────────────────────────────────────────────────────────────────────────────┤
│ Start │ Banve-01.dwg ● │ Ketcau-T2.dwg │ + │                                                  │ ← Tab tệp
├───────────┬──────────────────────────────────────────────────────────────────┬───────────┤
│ Palette   │ [-][Trên][2D Wireframe]                                          │ Palette   │
│ dock trái │                                                                  │ dock phải │
│ (Blocks,  │                     VÙNG VẼ (canvas)                             │(Thuộc tính│
│  Thư viện │                                                                  │ Layer…)   │
│  TCVN…)   │  Y                                                               │           │
│           │  └─X  (biểu tượng UCS)                                           │           │
├───────────┴──────────────────────────────────────────────────────────────────┴───────────┤
│ Model │ A1-Mat bang │ A1-Mat cat │ + │                                                     │ ← Tab layout
├──────────────────────────────────────────────────────────────────────────────────────────┤
│ ▸ LINE Chỉ định điểm đầu hoặc [Undo]:                                       ▴ (lịch sử)   │ ← Dòng lệnh
├──────────────────────────────────────────────────────────────────────────────────────────┤
│ 12345.67, 8901.23 │ MODEL │ ▦ ⊹ ∟ ⊕ ∠ ◎ ≡ ◐ ⟲ │ 1:100 ▾ │ ⚠ Font: 3 thay thế │ ⏳ │ 🔔 │ Pro │ ⚙ │ ← Thanh trạng thái
└──────────────────────────────────────────────────────────────────────────────────────────┘
```

| Vùng | Chức năng | Ghi chú hành vi |
|---|---|---|
| Thanh tiêu đề | Nút menu ứng dụng, Quick Access Toolbar, tên tệp (kèm `*` khi chưa lưu, "[Chỉ đọc]", "[Đã phục hồi]"), huy hiệu phiên bản, **ô Tìm lệnh**, tài khoản, trợ giúp | Ô Tìm lệnh tìm theo tên lệnh tiếng Anh, viết tắt **và từ khóa tiếng Việt** ("đường tròn" → `CIRCLE`). Kết quả chỉ ra vị trí trên ribbon và có thể chạy ngay. Menu tài khoản: tên và email, phiên bản, Trạng thái giấy phép (UI-L03), Quản lý tài khoản (mở web), Đăng xuất |
| Ribbon | Lệnh theo tab và panel (§7) | Có 3 mức thu gọn: đầy đủ → chỉ tên panel → chỉ tên tab. Tự ẩn khi bật Clean Screen |
| Tab tệp | Mỗi bản vẽ đang mở là một tab (§5.6) | Kéo để sắp xếp; nhấn chuột giữa để đóng |
| Vùng vẽ | Model hoặc Layout, viewport, con trỏ, grip, snap (§9) | Luôn nhận phím gõ, chuyển vào dòng lệnh hoặc ô nhập động |
| Palette | Thuộc tính, Layer, Blocks, Xref, Bác sĩ Font, Đếm… | Dock trái, phải hoặc dưới; nổi; tự ẩn (thu thành dải); gộp thành nhóm tab; kéo sang màn hình thứ hai |
| Tab layout | Model và các layout (§9.12) | Có menu chuột phải riêng |
| Dòng lệnh | Nhập lệnh, prompt, tùy chọn, lịch sử (§8) | Dock dưới (mặc định) hoặc nổi bán trong suốt trên vùng vẽ |
| Thanh trạng thái | Tọa độ, các chế độ hỗ trợ vẽ, tỉ lệ, tác vụ nền, thông báo, phiên bản (§10) | Người dùng chọn được mục nào hiện |

### 3.1 Workspace (không gian làm việc)

| Workspace | Mô tả | Phát hành |
|---|---|---|
| **Soạn thảo 2D** (mặc định) | Ribbon, palette Thuộc tính dock phải, dòng lệnh dock dưới | EA |
| **Cổ điển** | Thanh menu cùng toolbar kiểu cũ, không có ribbon. Nhiều người dùng VN vẫn quen kiểu này | EA |
| **Viewer** | Ribbon giản lược (UI-M01) | V |
| Workspace người dùng | Lưu bố cục hiện tại thành workspace mới; chuyển nhanh từ biểu tượng bánh răng trên thanh trạng thái | 1.0 |

Toolbar của workspace Cổ điển: Standard, Styles, Layers, Properties, Draw, Modify, Dimension, Inquiry, Text, Zoom, Object Snap, Viewports, Công cụ VN, Mở rộng. Có thể bật/tắt từng toolbar.

### 3.2 Hành vi theo kích thước màn hình
- **≥ 1920 px:** ribbon đầy đủ; palette Thuộc tính mở sẵn.
- **1366–1600 px:** panel ribbon co lại theo thứ tự ưu tiên (panel ít dùng thu thành nút thả xuống trước); palette mặc định ở dạng tự ẩn; dòng lệnh hiện 1 dòng.
- Phải dùng được trên **màn hình 125–200% (High-DPI)** và khi kéo cửa sổ giữa các màn hình có DPI khác nhau.

## 4. Màn hình Start và lần chạy đầu

### UI-A02 · Tab Start
- **Mở từ:** khởi động ứng dụng; tab "Start" luôn nằm đầu dải tab tệp (tắt được trong Tùy chọn); lệnh `NEWTAB`.
- **Thành phần:**
  1. **Tạo mới:** bộ sưu tập mẫu gồm *TCVN A0–A4 ngang/dọc (mm)*, *Trống (mm)*, *Trống (inch)*, *Mẫu của tôi*. Có nút "Duyệt mẫu…".
  2. **Mở:** "Mở tệp…", "Mở thư mục gần đây".
  3. **Tệp gần đây:**
     - xem dạng lưới (thumbnail) hoặc danh sách;
     - mỗi mục có tên, đường dẫn, ngày sửa, kích thước, phiên bản DWG;
     - ghim, tìm kiếm, sắp xếp;
     - chuột phải: Mở, Mở chỉ đọc, Mở thư mục chứa, Sao chép đường dẫn, Xóa khỏi danh sách.
  4. **Học tập:** video ngắn, "Chuyển từ AutoCAD sang trong 1 ngày", mẹo dùng.
  5. **Thông báo:** bản cập nhật, tin cộng đồng.
  6. **Thẻ phiên bản:** phiên bản hiện tại, số ngày dùng thử còn lại, nút nâng cấp.
- **Hành vi:** kéo-thả tệp vào tab Start để mở. Thumbnail tạo ở nền.
- **Trạng thái rỗng:** chưa có tệp gần đây → hiện "Mở bản vẽ mẫu" và "Xem video 2 phút".
- **Phát hành:** V (Viewer không có mục Tạo mới) · EA (đầy đủ).

### UI-A15 · Lần chạy đầu (onboarding)
- **Các bước** (bỏ qua được, xem lại trong Tùy chọn):
  1. Ngôn ngữ (Tiếng Việt / English).
  2. Chủ đề (Tối / Sáng).
  3. Kiểu giao diện (Ribbon / Cổ điển).
  4. **Chuyển cài đặt từ AutoCAD** (chỉ hiện nếu phát hiện AutoCAD trên máy; mở UI-K12 ở phần 3).
  5. Đơn vị và mẫu mặc định (mm, TCVN A3/A1…).
  6. **Quyền riêng tư:** đồng ý gửi telemetry ẩn danh (mặc định **tắt**), có giải thích ngắn.
  7. Phiên bản: dùng Viewer miễn phí / dùng thử Pro 30 ngày / kích hoạt bằng key.
  8. Hướng dẫn nhanh 5 bước trên một bản vẽ mẫu.
- **Phát hành:** V (bỏ bước 3, 4, 5) · EA (đầy đủ).

## 5. Quản lý tệp và trạng thái tệp

### UI-B01 · Mở tệp
- **Mở từ:** `OPEN`, Ctrl+O, menu ứng dụng, tab Start, QAT.
- **Thành phần:**
  - hộp thoại chọn tệp của hệ điều hành, có thêm khung xem trước gồm thumbnail, phiên bản DWG, "lưu lần cuối bởi", kích thước;
  - bộ lọc định dạng: DWG, DXF, DWT;
  - nút "Mở" có tùy chọn thả xuống "Mở chỉ đọc";
  - chọn nhiều tệp thì mở nhiều tab.
- **Phát hành:** V (tất cả định dạng, chỉ đọc) · EA.

### UI-B02 · Lưu thành (Save As)
- **Mở từ:** `SAVEAS`, Ctrl+Shift+S; lưu lần đầu.
- **Thành phần:**
  - **Định dạng:** DWG 2018 (khuyến nghị), DWG 2013, 2010, 2007, 2004, 2000; DXF (cùng các phiên bản đó); mẫu DWT; R14/R12 (P2).
  - **Gợi ý tương thích** dưới ô định dạng, ví dụ: *"Mở được bằng AutoCAD 2018 trở lên và TrionCAD"*.
  - Ô chọn "Đặt làm định dạng mặc định".
  - Nếu lưu thành mẫu (.dwt): ô mô tả và đơn vị đo.
- **Hành vi:** lưu sang phiên bản cũ mà có tính năng sẽ bị giảm (ví dụ annotative khi lưu 2004) thì cảnh báo, liệt kê ảnh hưởng, rồi mới cho lưu.
- **Lần đầu lưu DWG:** thông báo một lần: *"Khi mở tệp này, AutoCAD có thể báo tệp không được lưu bởi ứng dụng Autodesk. Tệp vẫn mở và dùng bình thường."* Có nút [Tìm hiểu] (trang câu hỏi thường gặp) và ô "Không hiện lại".
- **Phát hành:** EA. VP chỉ dùng để lưu bản sao sau khi chuyển font.

### UI-B03 · Đóng khi chưa lưu
- **Một tệp:** "Lưu thay đổi vào *Banve-01.dwg*?", các nút **Lưu** / **Không lưu** / **Hủy**.
- **Đóng nhiều tệp:** danh sách tệp chưa lưu kèm ô chọn, các nút "Lưu các tệp đã chọn" / "Không lưu" / "Hủy".
- **Phát hành:** EA.

### UI-B04 · Tệp bị lỗi và phục hồi
- **Khi mở thất bại:** hỏi *"Tệp có lỗi cấu trúc. Chạy Phục hồi?"* → thanh tiến trình → **báo cáo phục hồi** (UI-B10) → mở bản đã phục hồi, gắn nhãn "[Đã phục hồi]", gợi ý lưu thành tệp mới.
- **Phục hồi cả xref** (`RECOVERALL`, 1.0): ô chọn "Phục hồi luôn các xref lồng nhau"; báo cáo theo từng tệp.
- **Phát hành:** V (phục hồi để xem) · EA.

### UI-B05 · Tệp đang được người khác mở (khóa)
- **Nội dung:** người đang mở, tên máy, thời điểm.
- **Lựa chọn:** "Mở chỉ đọc" / "Báo khi tệp được giải phóng" (P2) / "Hủy".
- **Phát hành:** EA.

### UI-B06 · Thiếu tham chiếu khi mở
- **Mặc định là banner không chặn** trên vùng vẽ: *"Thiếu 2 xref, 3 font, 1 ảnh — [Xem chi tiết] [Bỏ qua]"*.
- **Bảng chi tiết:** nhóm theo loại (Font, Xref, Ảnh, PDF underlay, Plot style, Shape). Mỗi mục có:
  - đường dẫn đã lưu;
  - trạng thái;
  - hành động: Tìm tệp…, Dùng font thay thế (liên kết Bác sĩ Font), Bỏ qua.

  Nút "Tìm trong thư mục…" xử lý hàng loạt. Ô chọn "Nhớ lựa chọn cho các lần sau".
- **Phát hành:** V · EA (thêm sửa đường dẫn và lưu).

### UI-B07 · Trình phục hồi bản vẽ sau sự cố
- **Mở từ:** tự hiện khi khởi động lại sau crash; lệnh `DRAWINGRECOVERY`.
- **Thành phần:** danh sách bản vẽ, mỗi bản có các phiên bản (tệp gốc, `.bak`, autosave) kèm thời điểm và thumbnail. Hành động: Mở, Lưu thành, Xóa bản sao lưu.
- **Phát hành:** EA.

### UI-B13 · Mở tệp lớn
- **Hiển thị dần:** khung hình đầu tiên xuất hiện sớm. Thanh trạng thái hiện *"Đang nạp 63% · 1,2 triệu đối tượng"* kèm nút hủy.
- Trong khi nạp phần còn lại, người dùng **đã xem, pan, zoom được**. Lệnh sửa chờ đến khi nạp xong.
- **Phát hành:** V.

### 5.6 Tab tệp (UI-A09)
- **Mỗi tab hiển thị:** tên tệp, chấm báo chưa lưu, biểu tượng chỉ đọc. Rê chuột lên tab thì hiện thumbnail của Model và các layout; bấm thumbnail để nhảy tới.
- **Menu chuột phải:** Mới, Mở, Lưu, Lưu tất cả, Đóng, Đóng tất cả, Đóng các tab khác, Sao chép đường dẫn, Mở thư mục chứa.
- **Khác:** nút "+" để tạo mới; mũi tên thả xuống khi có quá nhiều tab; kéo để sắp xếp; nhấn chuột giữa để đóng.
- **Phát hành:** V · EA.

## 6. Menu ứng dụng và Quick Access Toolbar

### UI-A03 · Menu ứng dụng (nút logo)
- **Mục:** Mới (chọn mẫu), Mở (bản vẽ, DXF), Lưu, Lưu thành (kèm menu con chọn phiên bản DWG/DXF/DWT), Xuất (PDF, ảnh, DXF…), In (In, In hàng loạt, Thiết lập trang), **Tiện ích bản vẽ** (Kiểm tra lỗi, Phục hồi, Dọn dẹp, Thuộc tính bản vẽ, Đơn vị), Đóng (bản vẽ này / tất cả), Tùy chọn, Thoát.
- **Bên phải:** danh sách **tệp gần đây** (ghim; sắp xếp theo ngày, tên, kích thước; xem trước khi rê chuột).
- **Có ô tìm kiếm** tìm cả lệnh lẫn tệp gần đây.
- **Phát hành:** V (bản rút gọn) · EA.

### UI-A04 · Quick Access Toolbar (QAT)
- **Mặc định:** Mới, Mở, Lưu, Lưu thành, Hoàn tác (có danh sách các bước để hoàn tác nhiều bước một lần), Làm lại, In.
- **Menu tùy biến:** bật/tắt các mục có sẵn (Khớp thuộc tính, In hàng loạt, Xem trước bản in, Thuộc tính, Chuyển workspace, Bác sĩ Font, Hiện thanh menu); "Hiện bên dưới ribbon"; kéo thêm lệnh từ ribbon vào bằng chuột phải → "Thêm vào QAT".
- **Phát hành:** V (Mở, In) · EA.

## 7. Ribbon

### 7.1 Hành vi chung
- **Loại điều khiển:**
  - nút lớn, nút nhỏ;
  - **split button** (bấm phần chính chạy lựa chọn dùng gần nhất, bấm mũi tên mở danh sách);
  - dropdown;
  - **gallery** (mẫu hatch, block, style);
  - toggle;
  - ô nhập số;
  - thanh trượt.
- **Panel mở rộng:** mũi tên ở đáy panel mở vùng phụ; có ghim để giữ mở.
- **Gallery xem trước khi rê chuột** (ví dụ rê lên mẫu hatch thì vùng hatch đổi theo ngay), áp dụng khi bấm (P1).
- **Tab ngữ cảnh** (§7.11) tự hiện khi chọn đối tượng tương ứng hoặc khi đang chạy lệnh; tự đóng khi xong.
- **Tooltip:**
  - mức 1: tên, tên lệnh tiếng Anh, phím tắt;
  - mức 2 (rê lâu hơn 1,5 giây): mô tả kèm ảnh minh họa hoặc animation ngắn; "Nhấn F1 để xem trợ giúp".
- **Chuột phải trên ribbon:** hiện/ẩn tab và panel, thu gọn, thêm lệnh vào QAT.
- **Keytips** bằng phím Alt: P2.
- Cột "Phát hành" trong các bảng dưới ghi **mức ưu tiên / phiên bản** của từng điều khiển.

### 7.2 Tab "Trang chủ"

| Panel | Điều khiển | Lệnh | Loại | Phát hành |
|---|---|---|---|---|
| **Vẽ** | Đoạn thẳng | `LINE` | Nút lớn | P0 / EA |
| | Polyline | `PLINE` | Nút lớn | P0 / EA |
| | Đường tròn: Tâm–Bán kính, Tâm–Đường kính, 2 điểm, 3 điểm, Tiếp–Tiếp–Bán kính, Tiếp–Tiếp–Tiếp | `CIRCLE` | Split | P0 / EA |
| | Cung: 3 điểm; Đầu–Tâm–Cuối; Đầu–Tâm–Góc; Đầu–Tâm–Dây cung; Đầu–Cuối–Góc; Đầu–Cuối–Hướng; Đầu–Cuối–Bán kính; Tâm–Đầu–Cuối; Tâm–Đầu–Góc; Tâm–Đầu–Dây cung; Nối tiếp | `ARC` | Split | P0 / EA |
| | Hình chữ nhật / Đa giác | `RECTANG` / `POLYGON` | Split | P0 / EA |
| | Ellipse: Tâm; Trục–Đầu mút; Cung ellipse | `ELLIPSE` | Split | P0 / EA |
| | Hatch / Gradient / Đường biên | `HATCH` / `GRADIENT` / `BOUNDARY` | Split | P0 / EA (gradient P1) |
| | Spline: Điểm đi qua / Điểm điều khiển | `SPLINE` | Split | P0 / EA |
| | *Panel mở rộng:* Đường dựng hình, Tia, Nhiều điểm, Chia đều, Chia theo khoảng, Đám mây sửa đổi (chữ nhật, đa giác, tự do, chọn đối tượng), Region, Vùng che, Vành khuyên, Đa tuyến song song, Dấu tâm, Đường tâm, Vùng tô đặc 2D | `XLINE`, `RAY`, `POINT`, `DIVIDE`, `MEASURE`, `REVCLOUD`, `REGION`, `WIPEOUT`, `DONUT`, `MLINE`, `CENTERMARK`, `CENTERLINE`, `SOLID` | Nút nhỏ | P0–P1 / EA–1.0 |
| **Sửa** | Di chuyển, Xoay, Sao chép, Đối xứng, Kéo giãn, Tỉ lệ | `MOVE`, `ROTATE`, `COPY`, `MIRROR`, `STRETCH`, `SCALE` | Nút | P0 / EA |
| | Cắt / Kéo dài | `TRIM` / `EXTEND` | Split | P0 / EA |
| | Bo góc / Vát góc / Nối cong | `FILLET` / `CHAMFER` / `BLEND` | Split | P0 / EA (Blend P1) |
| | Mảng: Chữ nhật / Theo đường / Tròn / Hộp thoại cổ điển (UI-E02) | `ARRAYRECT` / `ARRAYPATH` / `ARRAYPOLAR` / `ARRAYCLASSIC` | Split | P0 / EA (đường P1) |
| | Xóa, Phá khối, Offset | `ERASE`, `EXPLODE`, `OFFSET` | Nút | P0 / EA |
| | *Panel mở rộng:* Nối, Bẻ, Bẻ tại điểm, Kéo dài thêm, Căn chỉnh, Sửa polyline, Sửa spline, Sửa hatch, Đảo chiều, Xóa trùng lặp, Thứ tự hiển thị (lên trên cùng, xuống dưới cùng, trên/dưới đối tượng, đưa text lên trên, đưa hatch xuống dưới), Đặt về ByLayer, Chuyển không gian, Hợp / Trừ / Giao region (P2) | `JOIN`, `BREAK`, `BREAKATPOINT`, `LENGTHEN`, `ALIGN`, `PEDIT`, `SPLINEDIT`, `HATCHEDIT`, `REVERSE`, `OVERKILL`, `DRAWORDER`, `TEXTTOFRONT`, `HATCHTOBACK`, `SETBYLAYER`, `CHSPACE`, `UNION` / `SUBTRACT` / `INTERSECT` | Nút nhỏ / Split | P0–P2 |
| **Chú thích** | Text: Nhiều dòng / Một dòng | `MTEXT` / `TEXT` | Split | P0 / EA |
| | Kích thước: Thông minh; Thẳng; Nghiêng; Góc; Cung; Bán kính; Đường kính; Gãy khúc; Tọa độ điểm | `DIM`, `DIMLINEAR`, `DIMALIGNED`, `DIMANGULAR`, `DIMARC`, `DIMRADIUS`, `DIMDIAMETER`, `DIMJOGGED`, `DIMORDINATE` | Split | P0–P1 |
| | Đường dẫn: Multileader / Thêm nhánh / Bớt nhánh | `MLEADER`, `MLEADEREDIT` | Split | P0 / EA |
| | Bảng | `TABLE` | Nút | P1 / 1.0 |
| | Dropdown kiểu chữ, kiểu kích thước, kiểu multileader, kiểu bảng | `STYLE`, `DIMSTYLE`… | Dropdown | P0 / EA |
| **Layer** | Thuộc tính layer (mở UI-C03) | `LAYER` | Nút lớn | P0 / V (chế độ xem) |
| | **Dropdown layer**: đặt layer hiện hành; mỗi dòng có nút bật/tắt, đóng băng, khóa, màu; ô tìm kiếm | — | Dropdown | P0 / V |
| | Đặt layer của đối tượng làm hiện hành, Khớp layer, Cô lập, Bỏ cô lập, Tắt, Đóng băng, Khóa/Mở khóa | `LAYMCUR`, `LAYMCH`, `LAYISO`, `LAYUNISO`, `LAYOFF`, `LAYFRZ`, `LAYLCK`/`LAYULK` | Nút nhỏ | P0 / EA |
| | *Panel mở rộng:* Bật tất cả, Rã đông tất cả, Trạng thái trước, Duyệt layer (UI-C11), Gộp, Xóa layer, Sao chép sang layer, Đổi sang layer hiện hành, Cô lập layer trong viewport, Dropdown trạng thái layer; **Nạp bộ layer chuẩn** (TCVN, theo bộ môn) | `LAYON`, `LAYTHW`, `LAYERP`, `LAYWALK`, `LAYMRG`, `LAYDEL`, `COPYTOLAYER`, `LAYCUR`, `LAYVPI`, `LAYERSTATE`, — | Nút nhỏ | P0–P1 / EA (Bật tất cả, Rã đông tất cả, bộ layer chuẩn) · 1.0 |
| **Block** | Chèn (gallery các block trong bản vẽ + "Mở thư viện…") | `INSERT` / `BLOCKSPALETTE` | Gallery | P0 / EA |
| | Tạo, Sửa (Block Editor), Sửa attribute (một / nhiều), Định nghĩa attribute, Quản lý attribute, Đồng bộ attribute | `BLOCK`, `BEDIT`, `EATTEDIT`/`ATTEDIT`, `ATTDEF`, `BATTMAN`, `ATTSYNC` | Nút | P0–P1 |
| **Thuộc tính** | Khớp thuộc tính | `MATCHPROP` | Nút lớn | P0 / EA |
| | Dropdown màu, linetype, lineweight; thanh trượt độ trong suốt | — | Dropdown | P0 / EA |
| | Liệt kê thông tin; mở palette Thuộc tính | `LIST`, `PROPERTIES` | Nút | P0 / V |
| **Nhóm** | Nhóm, Bỏ nhóm, Sửa nhóm, Bật/tắt chọn theo nhóm, Quản lý nhóm | `GROUP`, `UNGROUP`, `GROUPEDIT`, `PICKSTYLE`, `CLASSICGROUP` | Nút | P1 / 1.0 |
| **Tiện ích** | Đo: Nhanh / Khoảng cách / Bán kính / Góc / Diện tích | `MEASUREGEOM` | Split | P0 / V |
| | Chọn nhanh, Chọn tất cả, Chọn tương tự, Tọa độ điểm | `QSELECT`, `SELECTALL`, `SELECTSIMILAR`, `ID` | Nút nhỏ | P0 / EA (`ID` từ V) |
| **Clipboard** | Dán: Dán / Dán dạng block / Dán về tọa độ gốc / Dán đặc biệt | `PASTECLIP`, `PASTEBLOCK`, `PASTEORIG`, `PASTESPEC` | Split | P0–P1 |
| | Cắt; Sao chép / Sao chép có điểm gốc | `CUTCLIP`, `COPYCLIP`, `COPYBASE` | Nút | P0 / EA |

### 7.3 Tab "Chèn"

| Panel | Điều khiển | Lệnh | Phát hành |
|---|---|---|---|
| **Block** | Chèn; Palette Blocks; Tạo block; Ghi block ra tệp; Block Editor; Điểm gốc bản vẽ | `INSERT`, `BLOCKSPALETTE`, `BLOCK`, `WBLOCK`, `BEDIT`, `BASE` | P0–P1 |
| **Định nghĩa block** | Định nghĩa attribute, Quản lý attribute, Đồng bộ; Hiển thị attribute ▾ (Bình thường / Hiện tất cả / Ẩn tất cả) | `ATTDEF`, `BATTMAN`, `ATTSYNC`, `ATTDISP` | P0–P1 |
| **Tham chiếu** | Gắn (Attach): DWG / Ảnh / PDF / DGN (P3); Clip; Điều chỉnh (ảnh, PDF); Layer của underlay; Khung tham chiếu (hiện / ẩn / hiện nhưng không in); Bắt điểm vào underlay; Palette Tham chiếu ngoài; Bind từng đối tượng phụ thuộc; So sánh xref (P2) | `ATTACH`/`XATTACH`/`IMAGEATTACH`/`PDFATTACH`, `CLIP`, `ADJUST`, `ULAYERS`, `FRAME`, `UOSNAP`, `EXTERNALREFERENCES`, `XBIND`, `XCOMPARE` | P0–P2 (xem xref, ảnh: V) |
| **Nhập** | Nhập PDF thành đối tượng; Nhập DGN (P3); Nhập định dạng khác (P3) | `PDFIMPORT`, `DGNIMPORT`, `IMPORT` | P1 / 1.0 |
| **Dữ liệu** | Trường dữ liệu (Field), Cập nhật field, Trích xuất dữ liệu, Liên kết dữ liệu Excel (P2), Đối tượng OLE (P2), Hyperlink (P2) | `FIELD`, `UPDATEFIELD`, `DATAEXTRACTION`, `DATALINK`, `INSERTOBJ`, `HYPERLINK` | P1–P2 |
| **Nội dung** | **Thư viện TCVN** (UI-J13); DesignCenter; Tool Palettes | — / `ADCENTER` / `TOOLPALETTES` | P1 / EA–1.0 |
| **Vị trí** | Đặt vị trí địa lý, bản đồ nền (P3) | `GEOGRAPHICLOCATION`, `GEOMAP` | Sau |

### 7.4 Tab "Chú thích"

| Panel | Điều khiển | Lệnh | Phát hành |
|---|---|---|---|
| **Văn bản** | Nhiều dòng / Một dòng; Kiểm tra chính tả (P2); Tìm và thay thế; Kiểu chữ (dropdown và "Quản lý kiểu chữ…"); Ô chiều cao chữ; Đổi tỉ lệ chữ; Đổi điểm canh; Gióng thẳng chữ; Gộp chữ thành mtext | `MTEXT`/`TEXT`, `SPELL`, `FIND`, `STYLE`, `SCALETEXT`, `JUSTIFYTEXT`, `TEXTALIGN`, `TXT2MTXT` | P0–P1 |
| **Kích thước** | Kích thước (split như §7.2); Ghi nhanh; Nối tiếp / Cùng gốc (split); Ngắt dim; Giãn cách dim; Gãy đường dim; Kiểm định (P2); Cập nhật theo style; Gán lại liên kết; Bỏ liên kết; Nghiêng; Góc chữ; Canh chữ trái/giữa/phải; Ghi đè thuộc tính; Kiểu kích thước (dropdown + "Quản lý…"); Dropdown layer mặc định cho dim; Dung sai hình học (UI-F14); Dấu tâm; Đường tâm | `DIM`…, `QDIM`, `DIMCONTINUE`/`DIMBASELINE`, `DIMBREAK`, `DIMSPACE`, `DIMJOGLINE`, `DIMINSPECT`, `-DIMSTYLE _Apply`, `DIMREASSOCIATE`, `DIMDISASSOCIATE`, `DIMEDIT _Oblique`, `DIMTEDIT`, `DIMOVERRIDE`, `DIMSTYLE`, `DIMLAYER`, `TOLERANCE`, `CENTERMARK`, `CENTERLINE` | P0–P1 |
| **Đường dẫn** | Multileader; Thêm nhánh; Bớt nhánh; Gióng hàng; Gom nhóm; Kiểu multileader; Leader nhanh (UI-F13) | `MLEADER`, `MLEADEREDIT`, `MLEADERALIGN`, `MLEADERCOLLECT`, `MLEADERSTYLE`, `QLEADER` | P0–P1 |
| **Bảng** | Bảng; Kiểu bảng; **Dán bảng từ Excel** (VN); Xuất bảng (CSV/Excel); Trích xuất dữ liệu; Liên kết dữ liệu (P2) | `TABLE`, `TABLESTYLE`, —, `TABLEEXPORT`, `DATAEXTRACTION`, `DATALINK` | P1 / 1.0 |
| **Đánh dấu** | Đám mây sửa đổi; Vùng che; Nhập ghi chú từ PDF/ảnh (P3) | `REVCLOUD`, `WIPEOUT`, `MARKUPIMPORT` | P1 |
| **Tỉ lệ chú thích** | Danh sách tỉ lệ; Thêm/xóa tỉ lệ hiện hành; Thêm/xóa tỉ lệ cho đối tượng; Đồng bộ vị trí theo tỉ lệ; Bật/tắt hiển thị chú thích; Tự thêm tỉ lệ | `SCALELISTEDIT`, `OBJECTSCALE`, `ANNORESET`, `ANNOALLVISIBLE`, `ANNOAUTOSCALE` | P1 / 1.0 |

### 7.5 Tab "Tham số" (Parametric, P2, sau v1.0)
- **Ràng buộc hình học:** trùng điểm, thẳng hàng, đồng tâm, cố định, song song, vuông góc, nằm ngang, thẳng đứng, tiếp xúc, trơn (G2), đối xứng, bằng nhau (`GEOMCONSTRAINT`).
- **Ràng buộc khác:** tự ràng buộc (`AUTOCONSTRAIN`), hiện/ẩn (`CONSTRAINTBAR`).
- **Ràng buộc kích thước:** thẳng, ngang, đứng, nghiêng, bán kính, đường kính, góc, chuyển dim thành ràng buộc (`DIMCONSTRAINT`).
- **Quản lý:** hiện/ẩn ràng buộc động, xóa ràng buộc (`DELCONSTRAINT`), trình quản lý tham số (`PARAMETERS`), thiết lập (`CONSTRAINTSETTINGS`).
- Bật/tắt tự suy luận ràng buộc nằm trên thanh trạng thái.

### 7.6 Tab "Hiển thị"

| Panel | Điều khiển | Lệnh | Phát hành |
|---|---|---|---|
| **Điều hướng** | Pan; Zoom (split: Toàn bộ đối tượng, Cửa sổ, Trước đó, Thời gian thực, Tất cả, Tỉ lệ, Tâm, Đối tượng, Phóng to, Thu nhỏ); Xoay 3D (xem bản vẽ 3D); Regen / Regen tất cả | `PAN`, `ZOOM`, `3DORBIT`, `REGEN`/`REGENALL` | P0 / V (xoay 3D: P1) |
| **Khung nhìn đã đặt tên** | Trình quản lý view; gallery các view; view mới | `VIEW` | P1 / 1.0 (V: chỉ khôi phục view) |
| **Viewport (Model)** | Cấu hình viewport (Một, Hai dọc/ngang, Ba, Bốn…); Viewport đã đặt tên; Gộp viewport | `VPORTS` | P0 / EA |
| **Kiểu hiển thị** | Dropdown: 2D Wireframe, Wireframe, Hidden, Shaded, Realistic | `VSCURRENT` | P1 / V (chỉ xem) |
| **Palette** | Thuộc tính, Layer, Blocks, Tham chiếu ngoài, Tool Palettes, DesignCenter, Đếm, **Bác sĩ Font**, Markup (VP), Dòng lệnh, Máy tính nhanh (UI-I10, P2) | — | V–1.0 |
| **Giao diện** | Bật/tắt tab tệp, tab layout; sắp cửa sổ (xếp lớp / chia ô); Clean Screen; chủ đề Sáng/Tối; biểu tượng UCS; Navigation bar; ViewCube | `CLEANSCREENON`, `UCSICON`, `NAVBAR`, `NAVVCUBE` | V–1.0 |
| **Tọa độ** | UCS: Thế giới, Gốc, 3 điểm, Theo đối tượng, Theo view, Trước đó; Trình quản lý UCS; Nhìn vuông góc mặt bằng UCS | `UCS`, `UCSMAN`, `PLAN` | P0–P1 / EA |

### 7.7 Tab "Quản lý"

| Panel | Điều khiển | Lệnh | Phát hành |
|---|---|---|---|
| **Tùy biến** | Giao diện người dùng (CUI, P2); Tool Palettes; **Viết tắt lệnh** (UI-K05); Phím tắt (UI-K06); **Chuyển cài đặt từ AutoCAD** (UI-K12); Workspace | `CUI`, `CUSTOMIZE`, `ALIASEDIT`, —, —, `WORKSPACE` | P1–P2 |
| **Ứng dụng** | Nạp ứng dụng (UI-K01); Chạy script; Bảng điều khiển LISP (P2); Quản lý plugin (P2); Tùy chọn bảo mật; Biến hệ thống (UI-K13) | `APPLOAD`, `SCRIPT`, —, —, `SECURITYOPTIONS`, `SYSVARMONITOR` | P0–P2 / EA |
| **Chuẩn CAD** | Cấu hình và kiểm tra chuẩn `.dws` (P2); Dịch layer (P2); **Kiểm tra TCVN** (P2) | `STANDARDS`, `CHECKSTANDARDS`, `LAYTRANS` | Sau |
| **Làm sạch** | Dọn dẹp (UI-B09); Kiểm tra lỗi; Phục hồi; Xóa trùng lặp; Làm sạch hình học (P2); Đổi tên; Đơn vị; Thuộc tính bản vẽ | `PURGE`, `AUDIT`, `RECOVER`, `OVERKILL`, —, `RENAME`, `UNITS`, `DWGPROPS` | P0–P1 |
| **Tệp hàng loạt** | Xử lý tệp hàng loạt: chuyển phiên bản DWG, kiểm tra lỗi, dọn dẹp (UI-B14); Chuyển font hàng loạt (UI-J03); Chạy script trên nhiều bản vẽ (P2) | `TCBATCH`, `FIXFONTBATCH` | 1.0 (chuyển font: VP) |
| **Hiệu năng** | Thiết lập đồ họa (UI-L10); Kiểm tra hiệu năng máy (UI-L11) | `GRAPHICSCONFIG`, `TRIONBENCH` | V |

### 7.8 Tab "Xuất"

| Panel | Điều khiển | Lệnh | Phát hành |
|---|---|---|---|
| **In** | In (UI-H02); **In hàng loạt theo khung tên** (UI-H06, VN); In nhiều layout / nhiều bản vẽ (UI-H04); Quản lý thiết lập trang (UI-H01); Xem trước; Quản lý plot style; Quản lý máy in (P2); Nhật ký in | `PLOT`, —, `PUBLISH`, `PAGESETUP`, `PREVIEW`, `STYLESMANAGER`, `PLOTTERMANAGER`, — | V–P2 |
| **Xuất** | PDF (split: layout hiện tại, tất cả layout, vùng đang xem, cửa sổ); Ảnh (PNG/JPG/TIF/BMP); DXF; Xuất layout thành Model (P1); SVG (P2); DWF/DWFx, WMF (P2) | `EXPORTPDF`, `PNGOUT`…, `SAVEAS`, `EXPORTLAYOUT`, —, `EXPORTDWF`/`EXPORTDWFX`, `WMFOUT` | V–P2 |
| **Gửi** | Đóng gói gửi đi (eTransmit, UI-B11); Chia sẻ link / Zalo (P3) | `ETRANSMIT`, — | P1 / 1.0 |

### 7.9 Tab "Công cụ VN" (riêng của TrionCAD)

| Panel | Điều khiển | Phát hành |
|---|---|---|
| **Font** | Bác sĩ Font (UI-J02); Chuyển sang Unicode (vùng chọn / cả bản vẽ); Chuyển hàng loạt nhiều tệp (UI-J03); Bảng thay thế font | V (xem) · VP · EA |
| **TCVN** | Thư viện TCVN (UI-J13); Mẫu bản vẽ TCVN; Khung tên: chèn / sửa / điền tự động (UI-J05); **Tạo khung in trong Model theo tỉ lệ** (UI-J06); Kiểm tra TCVN (P2) | EA–1.0 |
| **Đo và thống kê** | **Tổng chiều dài**, **Tổng diện tích** (UI-I04); Bảng diện tích phòng (UI-I05); Đếm block; Thống kê thép (P2); Xuất bảng ra Excel | VP (đo tổng, đếm, xuất) · EA–1.0 |
| **Văn bản** | Đánh số tự động (UI-J07); Sao chép tăng số (UI-J08); Đổi chữ hoa/thường có dấu; Nối text; Tách text; Canh hàng text (UI-J09) | EA–1.0 |
| **Ký hiệu** | Cao độ; Trục định vị; Ký hiệu mặt cắt và chi tiết; Mũi tên hướng Bắc; Ký hiệu vật liệu (gallery hatch TCVN) (UI-J10) | 1.0 |
| **In ấn** | In hàng loạt theo khung tên; **Quản lý bộ bản vẽ** (UI-H07) | VP · 1.0 |

### 7.10 Tab "Mở rộng" (tương đương Express Tools)

Gom các công cụ người dùng AutoCAD quen gọi từ Express Tools. Tên lệnh giữ nguyên để script và LISP vẫn chạy. Công cụ nào đã có bản đầy đủ hơn ở tab Công cụ VN thì nút ở đây mở thẳng công cụ đó.

| Panel | Điều khiển | Lệnh | Phát hành |
|---|---|---|---|
| **Block** | Phá block giữ chữ attribute; Sao chép đối tượng lồng; Phá block theo tùy chọn thuộc tính; Sửa attribute hàng loạt; Thay block (UI-G16); Đếm block (UI-G09); Liệt kê đối tượng lồng, Đổi block thành xref, Tỉ lệ block (P2) | `BURST`, `NCOPY`, `XPLODE`, `GATTE`, `BLOCKREPLACE`, `BCOUNT`, `XLIST`, `BLOCKTOXREF`, `BSCALE` | P1 / 1.0 (P2: Sau) |
| **Văn bản** | Đổi hoa/thường (mở UI-J09); Đánh số (mở UI-J07); Phá chữ thành nét; Chữ theo cung, Che nền chữ, Co chữ cho vừa, Xoay chữ theo hướng đọc, Chữ lấy từ tệp ngoài (P2) | `TCASE`, `TCOUNT`, `TXTEXP`, `ARCTEXT`, `TEXTMASK`, `TEXTFIT`, `TORIENT`, `RTEXT` | P1 / 1.0 (P2: Sau) |
| **Sửa** | Làm phẳng Z; Cắt theo đường bao; Sửa nhiều polyline; Sao chép kèm xoay, Kéo giãn nhiều vùng (P2) | `FLATTEN`, `EXTRIM`, `MPEDIT`, `MOCORO`, `MSTRETCH` | P1 / 1.0 (P2: Sau) |
| **Vẽ** | Đường ngắt (breakline); Hatch bằng ảnh, block hoặc xref; Tạo shape; Tạo linetype từ hình vẽ | `BREAKLINE`, `SUPERHATCH`, `MKSHAPE`, `MKLTYPE` | P2 / Sau |
| **Layout** | Gióng hình giữa Model và Paper; Đồng bộ vị trí viewport; Đặt tỉ lệ cho nhiều viewport | `ALIGNSPACE`, `VPSYNC`, `VPSCALE` | P2 / Sau |
| **Kích thước** | Xuất / nhập kiểu kích thước; Gắn đường dẫn vào chú thích | `DIMEX` / `DIMIM`, `QLATTACH` | P2 / Sau |
| **Tệp** | Lưu tất cả; Mở lại bản đã lưu, Đóng tất cả không lưu (có xác nhận), Chuyển tệp `.bak`, Đổi đường dẫn hàng loạt (P2) | `SAVEALL`, `REVERT`, `QQUIT`, `MOVEBAK`, `REDIR` | P1 (`SAVEALL`) / P2 |
| **Công cụ** | Trình sửa biến hệ thống (mở UI-K13); Xem và gắn xdata, Thời gian sửa, Shape thành block (P2) | `SYSVDLG`, `XDATA` / `XDLIST`, `EDITTIME`, `SHP2BLK` | 1.0 (`SYSVDLG`) / P2 |

Prompt của các lệnh P1 trong tab này nằm ở UI-E10 (phần 2).

### 7.11 Tab ngữ cảnh

| Tab | Hiện khi | Nội dung chính | Đặc tả | Phát hành |
|---|---|---|---|---|
| **Soạn văn bản** | Đang tạo hoặc sửa MTEXT | Kiểu chữ, Định dạng, Đoạn, Chèn, Chính tả, Công cụ, Tùy chọn, Đóng | UI-F01 (phần 2) | EA |
| **Hatch** | Đang tạo hatch hoặc chọn một hatch | Biên, Mẫu, Thuộc tính, Gốc, Tùy chọn, Đóng | UI-E01 | EA |
| **Mảng** | Đang tạo hoặc chọn một mảng liên kết | Cột, Hàng, Số cấp (P3), Thuộc tính, Đóng | UI-E02 | EA (liên kết P1) |
| **Ô bảng** | Chọn ô của bảng | Hàng, Cột, Gộp, Kiểu ô, Định dạng, Chèn, Dữ liệu | UI-F07 | 1.0 |
| **Block Editor** | Trong Block Editor | Mở/Lưu, Hình học, Quản lý, Tham số và Action (P2), Visibility (P2), Đóng | UI-G04 | 1.0 |
| **Ảnh** / **PDF underlay** / **Tham chiếu ngoài** | Chọn đối tượng tương ứng | Điều chỉnh, Clip, Tùy chọn, Layer (PDF), Sửa tại chỗ hoặc mở (Xref) | UI-G15 | EA |
| **So sánh bản vẽ** | Đang chạy `COMPARE` | Thiết lập, điều hướng các thay đổi, chế độ xem, xuất báo cáo | UI-I07 | VP |
| **Layout** | Đang ở một tab layout | Layout, Viewport, Layer theo viewport, Không gian | UI-H11 | EA |
| **Sửa tham chiếu** | Đang sửa tại chỗ block hoặc xref (`REFEDIT`) | Tập làm việc, Lưu thay đổi, Hủy thay đổi | UI-G19 | 1.0 |

### 7.12 Thanh menu (workspace Cổ điển)
Gồm: **Tệp**, **Sửa**, **Hiển thị**, **Chèn**, **Định dạng** (Layer, Màu, Linetype, Lineweight, Kiểu chữ, Kiểu kích thước, Kiểu bảng, Kiểu multileader, Plot style, Kiểu điểm, Kiểu đa tuyến song song, Đơn vị, Giới hạn bản vẽ), **Công cụ**, **Vẽ**, **Kích thước**, **Sửa đổi**, **Tham số** (P2), **Cửa sổ**, **Công cụ VN**, **Mở rộng**, **Trợ giúp**.

Nội dung là các lệnh của ribbon, nhóm theo quy ước menu cổ điển quen thuộc. Mỗi mục hiện tên lệnh tiếng Anh và phím tắt ở cột phải.

## 8. Dòng lệnh (UI-A06)

- **Mở từ:** luôn có; bật/tắt bằng Ctrl+9.
- **Dạng:** dock dưới (mặc định) hoặc nổi bán trong suốt.
- **Phát hành:** V (lệnh xem và đo) · EA (đầy đủ).

**Thành phần**
1. **Ô nhập.** Khi đang ở vùng vẽ và không ở trong ô nhập nào khác, mọi phím gõ đều vào đây (hoặc vào ô nhập động cạnh con trỏ nếu bật DYN, §9.7).
2. **Prompt hiện tại:**
   - lời nhắc, ví dụ *"Chỉ định điểm đầu hoặc"*;
   - **tùy chọn dạng chip bấm được** `[Undo]`, chữ cái tắt được gạch chân;
   - giá trị mặc định `<10.0000>`.
3. **Vùng lịch sử:** hiện 3 dòng gần nhất (cấu hình được 0–10). **F2** mở cửa sổ lịch sử đầy đủ, có tìm kiếm và sao chép.
4. **Danh sách tự hoàn thành** hiện khi gõ. Gồm:
   - lệnh;
   - viết tắt (alias);
   - biến hệ thống (biểu tượng khác để phân biệt);
   - lệnh do LISP hoặc plugin định nghĩa;
   - **từ khóa tiếng Việt** ("cắt" → `TRIM`, "bo góc" → `FILLET`).

   Mỗi dòng có tên lệnh, viết tắt, mô tả ngắn tiếng Việt, biểu tượng. Sắp xếp theo tần suất dùng của chính người dùng. Có tìm cả giữa chuỗi.

**Hành vi**
- **Enter / Space:**
  - khi không có lệnh: lặp lệnh vừa dùng;
  - trong lệnh: xác nhận hoặc kết thúc.
- **Esc:** hủy lệnh (bấm hai lần thì bỏ cả vùng chọn).
- **↑ / ↓:** duyệt các lệnh và giá trị đã nhập.
- **Chuột phải** trên dòng lệnh: Lệnh gần đây, Giá trị đã nhập gần đây, Sao chép lịch sử, Tùy chọn dòng lệnh.
- **Lệnh trong suốt:** tiền tố `'` (ví dụ `'ZOOM` ngay giữa lệnh LINE). Prompt hiện lồng nhau, ví dụ ">> Zoom…". Máy tính `'CAL` (P1) tính biểu thức ngay trong lệnh, kể cả hàm lấy điểm bắt (`end`, `mid`, `cen`, `int`…).
- **Nhập tọa độ:**
  - tuyệt đối `100,200`;
  - tương đối `@100,0`;
  - cực `@500<45`;
  - nhập khoảng cách trực tiếp (gõ số theo hướng con trỏ);
  - ép góc `<30`.

  **Dấu thập phân luôn là "."** (khác quy ước số của tiếng Việt). Gợi ý này hiện một lần khi người dùng gõ ",".
- **Gõ sai tên lệnh:** *"Không có lệnh 'LNE'. Ý bạn là: LINE, LEN?"*, các gợi ý bấm được. Người dùng chọn cùng một gợi ý 3 lần thì từ đó lỗi gõ ấy tự được sửa (AutoCorrect, P1; xem và xóa trong thiết lập dòng lệnh).
- **Thông báo của lệnh** (ví dụ *"Đã chọn 125 đối tượng"*, *"2 đối tượng bị khóa trên layer bị khóa"*) hiện trong lịch sử. Thông báo quan trọng còn hiện thêm ở góc vùng vẽ (§12).
- **Gõ tiếng Việt:** bộ gõ (Unikey/EVKey/bộ gõ hệ điều hành) hoạt động trong mọi ô nhập. Riêng **khi đang chờ tên lệnh**, chế độ gõ tiếng Việt tự bỏ qua để "lm" không biến thành "lầm" (tùy chọn, mặc định bật).

**Thiết lập dòng lệnh** (`INPUTSEARCHOPTIONS`): số dòng lịch sử, cỡ chữ, độ trong suốt khi nổi, độ trễ tự hoàn thành, có tìm biến hệ thống không, có tìm theo tiếng Việt không.

## 9. Vùng vẽ và tương tác

### 9.1 Con trỏ
| Trạng thái | Hình con trỏ | Khi nào |
|---|---|---|
| Chờ | Chữ thập kèm ô chọn (pickbox) ở tâm | Không có lệnh |
| Chọn điểm | Chữ thập, không có ô chọn | Lệnh đang chờ điểm |
| Chọn đối tượng | Chỉ ô chọn | Lệnh đang chờ "Chọn đối tượng" |
| Kéo pan | Bàn tay | Giữ chuột giữa hoặc lệnh PAN |
| Zoom thời gian thực | Kính lúp +/− | Lệnh ZOOM thời gian thực |
| Bận | Chữ thập kèm vòng tiến trình nhỏ | Đang xử lý mà vẫn cho tương tác |

- **Huy hiệu con trỏ** (P1): biểu tượng nhỏ cạnh con trỏ cho biết lệnh đang chạy (di chuyển, sao chép, xoay, tỉ lệ, xóa, chọn…). Tắt được.
- Kích thước chữ thập (1–100% màn hình), kích thước ô chọn và ô bắt điểm được chỉnh trong Tùy chọn (UI-D10).

### 9.2 Điều hướng
- **Lăn chuột:** zoom quanh vị trí con trỏ. Tốc độ chỉnh được; có tùy chọn đảo chiều.
- **Giữ chuột giữa:** pan. **Nhấp đúp chuột giữa:** zoom toàn bộ đối tượng.
- **Touchpad và màn hình cảm ứng** (P1): chụm hai ngón để zoom, kéo hai ngón để pan.
- Zoom và pan liên tiếp được **gộp thành một bước hoàn tác** (tùy chọn).

### 9.3 Chọn đối tượng
| Cách chọn | Thao tác | Hiển thị |
|---|---|---|
| Chọn từng cái | Bấm vào đối tượng | Đối tượng được tô sáng ở trạng thái "đã chọn" |
| **Window** | Kéo từ trái sang phải | Khung **viền liền**, nền màu A trong suốt (mặc định xanh dương). Chỉ chọn đối tượng nằm trọn trong khung |
| **Crossing** | Kéo từ phải sang trái | Khung **viền đứt**, nền màu B (mặc định xanh lá). Chọn cả đối tượng chạm khung |
| **Lasso** (P1) | Nhấn giữ và kéo theo hình tự do | Đường bao tự do; nhấn Space để luân phiên Window / Crossing / Fence |
| Fence, WPolygon, CPolygon | Gõ tùy chọn F / WP / CP khi được hỏi chọn | Hình đa giác đang vẽ |
| Thêm / bớt | Shift + bấm để bớt (mặc định); tùy chọn dùng Shift để thêm | — |
| Chọn tương tự, chọn nhanh | Chuột phải hoặc ribbon | Hộp thoại UI-E06 |
| Chọn luân phiên khi đối tượng chồng nhau (P1) | Bật trên thanh trạng thái; rê lên chỗ chồng nhau | Danh sách nhỏ các đối tượng tại đó để chọn |

- **Xem trước vùng chọn:** đối tượng dưới con trỏ (hoặc trong khung đang kéo) được tô sáng trước khi bấm.
- **Bộ đếm:** dòng lệnh hiện *"Đã chọn 1.250"*. Palette Thuộc tính hiện số lượng theo loại.
- Màu sắc ở trên phải **không là tín hiệu duy nhất**: Window và Crossing còn khác nhau ở kiểu viền (§13.3).

### 9.4 Tô sáng
| Trạng thái | Ý nghĩa |
|---|---|
| Rê chuột (hover) | Đối tượng sẽ được chọn nếu bấm |
| Đã chọn | Có trong vùng chọn hiện tại |
| Kết quả tìm kiếm hoặc so sánh | Đối tượng được lệnh chỉ ra (ví dụ kết quả `FIND`, `COMPARE`) |
| Bị khóa hoặc làm mờ | Đối tượng trên layer bị khóa, hoặc xref đang làm mờ |

- **Tooltip khi rê lên đối tượng** (P1, tắt được): loại đối tượng, layer, màu, linetype; block có thêm tên, xref có thêm đường dẫn. Chọn trường hiện theo từng loại: P2.

### 9.5 Grip
- **Hiện khi** chọn đối tượng mà không có lệnh nào đang chạy.
- **Loại grip:**
  - vuông: điểm thường;
  - hình chữ nhật: grip đa năng ở giữa đoạn polyline;
  - tam giác/mũi tên: grip của dynamic block (kéo dài, lật, chọn trạng thái hiển thị, tra bảng) và grip kéo dài thêm;
  - tròn: tâm, xoay.
- **Trạng thái:** nguội (chưa chọn), rê chuột, nóng (đang kéo). Ba trạng thái phải phân biệt được bằng màu **và** độ đậm.
- **Grip theo loại đối tượng:**

  | Đối tượng | Vị trí grip | Ghi chú |
  |---|---|---|
  | Line | Hai đầu mút; trung điểm | Kéo trung điểm thì di chuyển cả đoạn |
  | Polyline | Mỗi đỉnh; giữa mỗi đoạn (grip đa năng) | Menu đa năng ở dưới |
  | Arc | Hai đầu mút, trung điểm, tâm | Đầu mút: Kéo giãn / Kéo dài thêm. Trung điểm: Kéo giãn / Bán kính |
  | Circle, Ellipse | Tâm; 4 điểm góc phần tư | Kéo điểm góc phần tư để đổi bán kính |
  | Spline | Điểm đi qua hoặc điểm điều khiển; tam giác chuyển giữa hai chế độ | Menu: Thêm / Bớt điểm, Tiếp tuyến |
  | Text, MText | Điểm chèn; MText có thêm mũi tên đổi rộng, cao và tay nắm cột | — |
  | Kích thước | Gốc đường gióng, đường dim, vị trí chữ | Menu ở UI-F05 (phần 2) |
  | Multileader | Đầu mũi tên, đoạn ngang, chữ | Menu: Thêm / Bớt đường dẫn |
  | Hatch | Grip tâm; đỉnh biên nếu hatch không liên kết | Menu grip tâm: Kéo giãn / Gốc / Góc / Tỉ lệ |
  | Block | Điểm chèn; grip tham số của dynamic block | Hình grip theo loại tham số |
  | Xref, ảnh, PDF underlay | Điểm chèn; 4 góc; grip biên clip; mũi tên đảo vùng clip | — |
  | Viewport (layout) | 4 góc để đổi kích thước; **grip tỉ lệ** (bấm ra danh sách tỉ lệ); grip di chuyển | UI-H11 (phần 2) |
  | Bảng | Góc; mép cột để đổi rộng; grip ngắt bảng | — |
  | Mảng liên kết | Điểm gốc; grip số lượng và khoảng cách | UI-E02 (phần 2) |

- **Rê lên grip đa năng thì hiện menu nhỏ:**
  - polyline: Kéo giãn / Thêm đỉnh / Bớt đỉnh / Chuyển thành cung (hoặc thành đoạn thẳng);
  - kích thước: Kéo giãn / Nối tiếp / Cùng gốc / Lật mũi tên / các vị trí đặt chữ;
  - dynamic block: hiện các trạng thái hiển thị hoặc giá trị tra bảng dạng danh sách.
- **Khi grip nóng:** nhấn Space để luân phiên Kéo giãn → Di chuyển → Xoay → Tỉ lệ → Đối xứng. Prompt hiện trên dòng lệnh. Chuột phải mở menu: Điểm gốc, Sao chép, Tham chiếu, Hoàn tác, Thoát.
- **Giới hạn hiện grip:** chọn quá 1.000 đối tượng (cấu hình được) thì không hiện grip, để giữ hiệu năng.
- **Phát hành:** EA (cơ bản) · 1.0 (đa năng, dynamic block).

### 9.6 Truy bắt điểm (OSNAP) và theo dõi (tracking)
- **Mỗi chế độ bắt điểm có một marker hình học riêng** (tự thiết kế nhưng giữ ngữ nghĩa quen thuộc), kèm tooltip tên chế độ:

| Chế độ | Ngữ nghĩa hình marker gợi ý |
|---|---|
| Endpoint (Điểm cuối) | Hình vuông |
| Midpoint (Trung điểm) | Tam giác |
| Center (Tâm) | Hình tròn |
| Geometric Center (Tâm hình học) | Hình tròn có điểm ở giữa |
| Node (Điểm) | Hình tròn có dấu × |
| Quadrant (Góc phần tư) | Hình thoi |
| Intersection (Giao điểm) | Dấu × |
| Extension (Kéo dài) | Đường chấm nối dài + dấu cộng nhỏ |
| Insertion (Điểm chèn) | Hai hình vuông lồng nhau |
| Perpendicular (Vuông góc) | Ký hiệu vuông góc |
| Tangent (Tiếp xúc) | Hình tròn có đường tiếp tuyến |
| Nearest (Gần nhất) | Đồng hồ cát |
| Apparent Intersection (Giao biểu kiến) | Dấu × trong hình vuông |
| Parallel (Song song) | Hai vạch song song |

- **Hành vi:**
  - **Tab** luân phiên giữa các điểm bắt khả dĩ tại vị trí con trỏ.
  - **Shift + chuột phải** mở menu ghi đè bắt điểm một lần. Menu gồm:
    - Điểm theo dõi tạm thời, Từ (From), Trung điểm giữa 2 điểm;
    - bộ lọc tọa độ `.X .Y .Z .XY .XZ .YZ`;
    - các chế độ bắt điểm;
    - "Thiết lập bắt điểm…".
  - Bắt điểm vào **xref, block, PDF underlay** (P1).
- **Tracking:**
  - Rê và dừng trên một điểm bắt để "lấy điểm" (hiện dấu cộng nhỏ).
  - Đường theo dõi là đường chấm kéo dài vô hạn qua điểm đã lấy, kèm tooltip: *"Kéo dài: 1250.00 < 0°"*, *"Polar: 3000.00 < 45°"*, *"Giao điểm: Polar và Kéo dài"*.
- **Phát hành:** V (bắt điểm khi đo) · EA (đầy đủ) · 1.0 (Apparent, Parallel, bắt vào underlay).

### 9.7 Nhập động (Dynamic Input, DYN)
- **Bật/tắt:** F12 hoặc thanh trạng thái.
- **Ba phần:**
  1. **Nhập con trỏ:** hai ô X và Y cạnh con trỏ (hoặc khoảng cách và góc, tùy thiết lập). Nhấn Tab để chuyển ô; ô đã gõ hiện biểu tượng khóa.
  2. **Nhập kích thước:** khi vẽ hoặc kéo grip, hiện ô độ dài và góc gắn ngay trên đối tượng đang tạo.
  3. **Prompt động:** lời nhắc hiện cạnh con trỏ. Nhấn ↓ mở danh sách tùy chọn để chọn bằng chuột hoặc phím.
- **Hành vi:**
  - Gõ `#` để nhập tọa độ tuyệt đối, `@` cho tương đối.
  - Kiểm tra định dạng ngay khi gõ; ô viền đỏ nếu sai.
  - Tooltip có nền đủ tương phản với cả nền vùng vẽ sáng lẫn tối.
- **Phát hành:** EA.

### 9.8 Xem trước trong lệnh
- **Rubber-band:** đường hoặc đối tượng tạm đi theo con trỏ.
- **Xem trước kết quả:**
  - TRIM/EXTEND: đoạn sẽ bị xóa hiện mờ hoặc đứt nét khi rê chuột;
  - OFFSET: bản offset hiện tạm;
  - FILLET: cung bo hiện tạm;
  - HATCH: vùng tô hiện khi rê chuột vào vùng kín;
  - MATCHPROP: đối tượng đích đổi thuộc tính tạm khi rê lên.
- Xem trước **không ghi vào bản vẽ**. Esc trả mọi thứ về như cũ.
- **Phát hành:** EA (cơ bản) · 1.0 (đầy đủ).

### 9.9 Menu chuột phải trên vùng vẽ
| Ngữ cảnh | Mục chính |
|---|---|
| **Không lệnh, không chọn gì** | Lặp lệnh *[tên lệnh]*; Lệnh gần đây ▸; Giá trị đã nhập gần đây ▸; Clipboard ▸ (Cắt, Sao chép, Sao chép có điểm gốc, Dán, Dán dạng block, Dán về tọa độ gốc); Cô lập ▸; Hoàn tác *[tên lệnh]*; Làm lại; Pan; Zoom; Chọn nhanh; Tìm…; Tùy chọn… |
| **Có vùng chọn** | Lặp lệnh; Lệnh gần đây ▸; Clipboard ▸; Cô lập ▸ (Cô lập / Ẩn / Kết thúc cô lập); Xóa; Di chuyển; Sao chép; Tỉ lệ; Xoay; Thứ tự hiển thị ▸; Nhóm ▸; **Tạo đối tượng cùng loại** (`ADDSELECTED`); **Chọn tương tự**; Bỏ chọn tất cả; Chọn nhanh; Đếm (nếu là block); Thuộc tính; Thuộc tính nhanh |
| **Có vùng chọn, mục thêm theo loại** | Text/MText: Sửa, Trình soạn chữ · Block: Sửa block tại chỗ, Block Editor, Sửa attribute, Chọn các block cùng tên · Xref: Mở xref, Sửa tại chỗ, Clip, Nạp lại, Gỡ · Viewport: Phóng to viewport, Khóa hiển thị, Clip, Tỉ lệ ▸ · Kích thước: Vị trí chữ ▸, Độ chính xác ▸, Kiểu kích thước ▸, Lật mũi tên · Hatch: Sửa hatch, Đặt gốc, Tạo đường biên, Tách hatch · Ảnh/PDF: Điều chỉnh, Clip, Layer PDF |
| **Đang trong lệnh** | Enter; Hủy; Giá trị đã nhập gần đây ▸; **các tùy chọn của lệnh** (sinh động theo lệnh); Ghi đè bắt điểm ▸; Pan; Zoom |
| **Trên grip nóng** | Kéo giãn / Di chuyển / Xoay / Tỉ lệ / Đối xứng; Điểm gốc; Sao chép; Tham chiếu; Hoàn tác; Thuộc tính; Thoát |

- **Tùy chọn "chuột phải theo thời gian"** (P1): bấm nhanh = Enter, giữ lâu = mở menu.
- **Phát hành:** V (bản rút gọn: Pan, Zoom, Đo, Thuộc tính) · EA.

### 9.10 Nhấp đúp (theo loại đối tượng)
| Đối tượng | Hành động mặc định |
|---|---|
| Text, MText, chữ của Multileader, chữ kích thước | Sửa chữ tại chỗ (MText mở tab Soạn văn bản) |
| Block có attribute | Trình sửa attribute (UI-G07) |
| Block không có attribute, dynamic block | Hỏi mở Block Editor (UI-G04), hoặc sửa tại chỗ theo tùy chọn |
| Xref | Sửa tại chỗ (`REFEDIT`), có xác nhận |
| Hatch | Chọn hatch và mở tab ngữ cảnh Hatch |
| Polyline / Spline | `PEDIT` / `SPLINEDIT` |
| Đa tuyến song song | `MLEDIT` (UI-E09) |
| Ô bảng | Sửa nội dung ô |
| Attribute definition | Sửa Tag/Prompt/Default |
| Viewport (trong layout) | Kích hoạt Model space trong viewport; nhấp đúp ra ngoài viewport để về Paper space |
| Đường thẳng, cung, tròn… | Mở Thuộc tính nhanh (UI-C02) |
| Vùng trống | Không làm gì (hoặc đóng Thuộc tính nhanh) |

Hành động nhấp đúp tùy biến được (P2, CUI). **Phát hành:** EA. Trong Viewer, nhấp đúp mở Thuộc tính ở chế độ chỉ đọc.

### 9.11 Kéo-thả
| Kéo cái gì | Thả vào đâu | Kết quả |
|---|---|---|
| Tệp DWG | Vùng vẽ | Chèn thành block (hỏi điểm chèn) hoặc gắn xref (giữ Ctrl) |
| Tệp DWG/DXF | Dải tab tệp hoặc tab Start | Mở |
| Ảnh, PDF | Vùng vẽ | Gắn ảnh hoặc PDF underlay |
| Block từ palette, thư viện TCVN, DesignCenter | Vùng vẽ | Chèn tại vị trí thả (theo tỉ lệ chú thích nếu bật) |
| Đối tượng đã chọn (nhấn giữ rồi kéo) | Vùng vẽ | Di chuyển. Kéo bằng chuột phải thì mở menu: Di chuyển tới đây / Sao chép tới đây / Dán thành block / Hủy |
| Đối tượng đã chọn | Ô Tool Palette | Tạo công cụ mới |
| Văn bản, bảng từ ứng dụng khác | Vùng vẽ | Dán thành MText hoặc thành bảng (bảng Excel → bảng CAD, P1) |

**Phát hành:** V (mở tệp) · EA.

### 9.12 Model, Layout, Viewport
- **Tab layout** có menu chuột phải: Layout mới, Từ mẫu…, Xóa, Đổi tên, Di chuyển hoặc Sao chép, Chọn tất cả layout, Kích hoạt layout trước, Quản lý thiết lập trang, In…, Xuất layout thành Model (P1), Ẩn tab layout và Model.
- Tab layout có thumbnail khi rê chuột. Nút "+" thêm layout mới.
- **Trong layout:**
  - hiện khổ giấy, vùng in được (đường đứt), bóng giấy;
  - viewport có viền;
  - viewport đang kích hoạt có viền đậm;
  - thanh trạng thái hiện **tỉ lệ viewport** (dropdown) và **khóa viewport**.
- **Viewport bị khóa:** zoom bên trong sẽ zoom cả layout (tùy chọn). Có biểu tượng khóa ở góc viewport.
- **Phát hành:** V (xem layout, kích hoạt viewport để pan/zoom, không đổi tỉ lệ) · EA.

### 9.13 Thành phần nổi trên vùng vẽ
- **Biểu tượng UCS** (góc dưới trái): bật/tắt; hiện ở gốc tọa độ (tùy chọn).
- **Điều khiển viewport** (góc trên trái, P1): menu `[-]` (phóng to/thu nhỏ viewport, cấu hình viewport, ViewCube, Navigation bar), menu hướng nhìn (Trên, Dưới, Trái, Phải, Trước, Sau, các góc isometric), menu kiểu hiển thị.
- **ViewCube** (P1, chỉ hữu ích với bản vẽ 3D): mặc định ẩn ở bản vẽ 2D, tự hiện khi bản vẽ có đối tượng 3D.
- **Navigation bar** (P2): Pan, Zoom, Xoay.

### 9.14 Thông điệp trên vùng vẽ
- **Banner trên cùng** (không chặn): thiếu tham chiếu, đã thay font, tệp chỉ đọc, đã phục hồi. Có hành động và nút đóng.
- **Toast góc dưới phải:** kết quả tác vụ nền (*"Đã xuất 24 tờ PDF — [Mở thư mục]"*). Tự ẩn sau 5 giây.
- **Chú thích cạnh con trỏ:** lỗi tức thời (*"Không có giao điểm"*, *"Đối tượng nằm trên layer bị khóa"*).

### 9.15 Nhiều cửa sổ bản vẽ
- Sắp các bản vẽ dạng xếp lớp hoặc chia ô (P1).
- Kéo một tab tệp ra thành cửa sổ nổi, đặt sang màn hình khác (P2).
- Mỗi bản vẽ giữ riêng: view, layout đang mở, vùng chọn.

## 10. Thanh trạng thái (UI-A08)

| Mục | Phím | Bấm trái | Chuột phải / menu | Phát hành |
|---|---|---|---|---|
| Tọa độ con trỏ | Ctrl+I | Đổi kiểu hiển thị: tuyệt đối / tương đối / tắt | Kiểu hiển thị | V |
| MODEL / PAPER | — | Trong layout: chuyển giữa Paper space và Model space trong viewport | — | V |
| Lưới (Grid) | F7 | Bật/tắt | Thiết lập lưới | EA |
| Snap | F9 | Bật/tắt | Snap lưới / Snap polar; Thiết lập | EA |
| Suy luận ràng buộc | Ctrl+Shift+I | Bật/tắt | Thiết lập | P2 |
| Nhập động (DYN) | F12 | Bật/tắt | Nhập con trỏ / Nhập kích thước / Prompt động; Thiết lập | EA |
| Ortho | F8 | Bật/tắt | — | EA |
| Polar | F10 | Bật/tắt | Danh sách góc: 90, 45, 30, 22.5, 18, 15, 10, 5 + góc người dùng; Thiết lập | EA |
| Isometric | F5 (đổi mặt) | Bật/tắt | Mặt trái / trên / phải | P1 |
| Object snap tracking | F11 | Bật/tắt | — | EA |
| Object snap | F3 | Bật/tắt | **Danh sách chế độ có ô chọn** (bấm để bật/tắt từng chế độ); Thiết lập | V (khi đo) / EA |
| Hiển thị lineweight | — | Bật/tắt | Thiết lập lineweight | V |
| Hiển thị độ trong suốt | — | Bật/tắt | — | V |
| Chọn luân phiên | Ctrl+W | Bật/tắt | Thiết lập | P1 |
| Hiển thị chú thích / Tự thêm tỉ lệ | — | Bật/tắt | — | P1 |
| **Tỉ lệ chú thích** (Model) / **Tỉ lệ viewport** (Layout) | — | Dropdown: 1:1 … 1:1000, tỉ lệ người dùng, "Tùy chỉnh…" | — | EA (V: chỉ xem) |
| Khóa viewport | — | Khóa/mở khóa viewport đang chọn | — | EA |
| Chuyển workspace | — | Menu workspace | Lưu workspace hiện tại… | EA |
| Đơn vị | — | Hiện đơn vị hiện hành; bấm để đổi nhanh | — | P1 |
| Thuộc tính nhanh | Ctrl+Shift+P | Bật/tắt | Thiết lập | P1 |
| Cô lập đối tượng | — | Cô lập / Ẩn / Kết thúc cô lập | — | P1 |
| **Chỉ báo đồ họa** (TrionCAD) | — | Mở UI-L10 | Rê chuột: backend (D3D12/Vulkan/Metal…), GPU, FPS hiện tại | V |
| **Chỉ báo Bác sĩ Font** (TrionCAD) | — | Mở UI-J02 | Rê chuột: *"3 font đã thay thế · 250 đối tượng TCVN3"* | V |
| **Tác vụ nền** (TrionCAD) | — | Danh sách tác vụ (tự lưu, in hàng loạt, chuyển font, nạp tệp) kèm tiến trình và nút hủy | — | V |
| Thông báo | — | Trung tâm thông báo (UI-L14) | — | V |
| **Huy hiệu phiên bản** | — | Trạng thái giấy phép (UI-L03) | — | V |
| Clean Screen | Ctrl+0 | Bật/tắt | — | V |
| **Theo dõi biến hệ thống** | — | Chỉ hiện khi một biến trong danh sách theo dõi bị đổi; bấm để xem và khôi phục (UI-K13) | — | 1.0 |
| Ô tùy biến (DIESEL) | — | Hiện chuỗi do `MODEMACRO` định nghĩa (ví dụ layer, tỉ lệ hiện hành) | — | P1 |
| Tùy biến | — | Chọn các mục hiện trên thanh trạng thái | — | V |

Mỗi nút toggle có trạng thái bật/tắt rõ ràng và tooltip gồm tên, phím tắt, tên biến hệ thống liên quan.

## 11. Phím tắt mặc định

Giữ tương thích AutoCAD. Phím tắt mới của TrionCAD **không được trùng** các tổ hợp quen thuộc.

| Phím | Chức năng | Phím | Chức năng |
|---|---|---|---|
| F1 | Trợ giúp (theo ngữ cảnh) | Ctrl+N / O / S | Mới / Mở / Lưu |
| F2 | Lịch sử dòng lệnh đầy đủ | Ctrl+Shift+S | Lưu thành |
| F3 | Bật/tắt Object snap | Ctrl+P | In |
| F5 / Ctrl+E | Đổi mặt isometric | Ctrl+Z / Ctrl+Y | Hoàn tác / Làm lại |
| F7 / Ctrl+G | Lưới | Ctrl+X / C / V | Cắt / Sao chép / Dán |
| F8 / Ctrl+L | Ortho | Ctrl+Shift+C | Sao chép có điểm gốc |
| F9 / Ctrl+B | Snap | Ctrl+Shift+V | Dán dạng block |
| F10 / Ctrl+U | Polar | Ctrl+A | Chọn tất cả |
| F11 | Object snap tracking | Ctrl+1 | Palette Thuộc tính |
| F12 | Nhập động | Ctrl+2 / Ctrl+3 | DesignCenter / Tool Palettes |
| Esc | Hủy | Ctrl+9 | Bật/tắt dòng lệnh |
| Enter / Space | Xác nhận; lặp lệnh | Ctrl+0 | Clean Screen |
| Delete | Xóa đối tượng đã chọn | Ctrl+Shift+P | Thuộc tính nhanh |
| Shift (giữ) | Tạm đảo trạng thái Ortho | Ctrl+W | Chọn luân phiên |
| Shift + chuột phải | Menu ghi đè bắt điểm | Ctrl+I | Kiểu hiển thị tọa độ |
| Tab | Luân phiên điểm bắt / ô DYN | Ctrl+Tab / Ctrl+Shift+Tab | Tab tệp kế / trước |
| ↑ / ↓ | Lịch sử nhập | Ctrl+PgUp / PgDn | Layout trước / kế |
| Ctrl+mũi tên (P2) | Dịch đối tượng một bước nhỏ | Ctrl+F4 | Đóng bản vẽ |
| Ctrl+R | Chuyển viewport kích hoạt | Ctrl+Q | Thoát |

Người dùng xem toàn bộ phím tắt ở **Bảng phím tắt** (in được thành PDF một trang) và sửa ở UI-K06.

## 12. Thông báo, tiến trình, lỗi

| Loại | Dùng khi | Hành vi | Ví dụ |
|---|---|---|---|
| **Thông điệp dòng lệnh** | Kết quả bình thường của lệnh | Ghi vào lịch sử | "Đã xóa 12 đối tượng" |
| **Chú thích cạnh con trỏ** | Lỗi tức thời trong lệnh | Tự ẩn sau 2 giây | "Không có giao điểm với đường biên" |
| **Banner trên vùng vẽ** | Trạng thái của bản vẽ cần người dùng biết | Ở lại đến khi đóng; có hành động | "Bản vẽ chỉ đọc — [Lưu thành…]" |
| **Toast** | Tác vụ nền xong hoặc lỗi | Tự ẩn sau 5 giây (lỗi thì ở lại); vào trung tâm thông báo | "Chuyển font xong 48/50 tệp — 2 lỗi [Xem]" |
| **Hộp thoại xác nhận** | Hành động phá hủy hoặc khó hoàn tác | Nút chính nêu rõ hành động ("Xóa 3 layout"), không dùng chữ "OK"; có ô "Không hỏi lại" (khôi phục được trong Tùy chọn) | "Dọn 1.204 mục? Không hoàn tác được sau khi lưu." |
| **Hộp thoại lỗi** | Lỗi chặn thao tác | Lời giải thích dễ hiểu + chi tiết kỹ thuật thu gọn + "Sao chép chi tiết" + "Gửi báo lỗi" | "Không lưu được: ổ đĩa đầy" |
| **Tiến trình** | Thao tác trên 0,5 giây | Dưới 2 giây: con trỏ bận; trên 2 giây: thanh tiến trình có % và **nút hủy** ở thanh trạng thái hoặc trong hộp thoại | "Đang mở · 63%" |
| **Kiểm tra trong ô nhập** | Giá trị không hợp lệ | Viền lỗi kèm dòng giải thích dưới ô; chưa sửa thì không áp dụng | "Tỉ lệ phải lớn hơn 0" |
| **Trạng thái rỗng** | Danh sách hoặc palette chưa có gì | Giải thích và hành động gợi ý | "Bản vẽ chưa có block nào — [Mở Thư viện TCVN]" |

Văn phong: câu ngắn, xưng hô trung tính, không đổ lỗi cho người dùng, luôn chỉ ra cách khắc phục.

## 13. Chuẩn hiển thị, khả năng tiếp cận, bản địa hóa

### 13.1 Kích thước, DPI
- Hỗ trợ từ **1366×768**; tỉ lệ màn hình 100–200%; đổi DPI khi kéo cửa sổ giữa các màn hình.
- Ribbon có hai cỡ biểu tượng (16 và 32 px logic). Palette có hai mật độ: gọn / thoáng.

### 13.2 Chủ đề và màu ngữ nghĩa
- **Chủ đề giao diện:** Tối (mặc định) và Sáng.
- **Nền vùng vẽ:** đặt riêng (mặc định tối cho Model, trắng cho Layout).
- Các **màu ngữ nghĩa** dưới đây phải có trong bộ token, cho cả chủ đề tối và sáng. Người dùng chỉnh được ở UI-D10.

| Token | Dùng cho |
|---|---|
| `selection.window` / `selection.crossing` | Khung chọn Window / Crossing |
| `highlight.hover` / `highlight.selected` / `highlight.result` | Tô sáng (§9.4) |
| `grip.cold` / `grip.hover` / `grip.hot` | Grip |
| `snap.marker`, `tracking.vector` | Marker bắt điểm, đường theo dõi |
| `dyn.tooltip.bg` / `dyn.tooltip.text` | Ô nhập động |
| `fade.xref`, `fade.locked` | Làm mờ xref, layer bị khóa |
| `paper.bg`, `paper.printable`, `paper.shadow` | Layout |
| `status.on` / `status.off` | Toggle trên thanh trạng thái |
| `edition.badge.*` | Huy hiệu phiên bản |
| `compare.onlyA` / `compare.onlyB` / `compare.common` | So sánh bản vẽ |

### 13.3 Khả năng tiếp cận
- Toàn bộ thao tác làm được bằng bàn phím: thứ tự focus hợp lý, phím tắt trong hộp thoại, Esc để đóng.
- Phần tử UI có tên đọc được cho trình đọc màn hình (khả năng accessibility của Qt).
- Chữ giao diện đạt độ tương phản ≥ 4,5:1.
- **Không dùng màu làm tín hiệu duy nhất**: Window/Crossing khác nhau cả ở kiểu viền; so sánh bản vẽ có cả ký hiệu.
- Tôn trọng thiết lập "giảm chuyển động" của hệ điều hành.

### 13.4 Bản địa hóa và bảng thuật ngữ
- **Ngôn ngữ:** Tiếng Việt (mặc định) và English, chuyển được không cần cài lại. Chuỗi tiếng Việt thường dài hơn tiếng Anh khoảng 20–40%, nên bố cục phải chịu được.
- **Sau v1.0:** thêm Bahasa Indonesia, và tiếng Anh cho thị trường Philippines ([04](04-chien-luoc-san-pham.md) §11). Mọi chuỗi giao diện nằm trong tệp dịch, không viết cứng trong mã.
- **Quy tắc:** dịch hành động chung (Mở, Lưu, In, Xuất, Thuộc tính…). **Giữ nguyên** thuật ngữ CAD mà người dùng Việt quen nói bằng tiếng Anh, có thể kèm giải nghĩa trong tooltip.

| Thuật ngữ | Nhãn đề xuất | Thuật ngữ | Nhãn đề xuất |
|---|---|---|---|
| Layer | Layer | Properties | Thuộc tính |
| Block | Block | Attribute | Attribute (thuộc tính block) |
| Layout | Layout | Model space / Paper space | Model / Paper |
| Viewport | Viewport | Linetype | Kiểu đường (Linetype) |
| Lineweight | Độ dày nét | Hatch | Hatch (tô vật liệu) |
| Dimension | Kích thước | Leader / Multileader | Đường dẫn / Multileader |
| Text style / Dim style | Kiểu chữ / Kiểu kích thước | Plot / Plot style | In / Kiểu in (Plot style) |
| Page setup | Thiết lập trang | Xref | Tham chiếu ngoài (Xref) |
| Snap / Object snap | Snap / Bắt điểm (OSNAP) | Tracking | Theo dõi |
| Grip | Grip | Purge / Audit / Recover | Dọn dẹp / Kiểm tra lỗi / Phục hồi |
| Annotative / Annotation scale | Chú thích theo tỉ lệ / Tỉ lệ chú thích | Template | Mẫu bản vẽ |
| Workspace | Không gian làm việc | Command line | Dòng lệnh |

- **Định dạng:**
  - số trong vùng vẽ dùng dấu chấm thập phân, theo `UNITS`;
  - số ở giao diện chung (dung lượng, số lượng) dùng định dạng Việt Nam (1.250);
  - ngày dạng dd/mm/yyyy.

### 13.5 Biểu tượng
- Mỗi lệnh trên ribbon, toolbar và menu cần một biểu tượng **SVG** ở cỡ 16 và 32 px, có biến thể cho chủ đề tối và sáng, và trạng thái vô hiệu.
- Ước tính khoảng **450–550 biểu tượng** đến v1.0 (kể cả marker bắt điểm, grip, huy hiệu con trỏ, biểu tượng trạng thái).
- Ưu tiên thiết kế bộ biểu tượng cho **Viewer** (khoảng 80 biểu tượng) trước.
- Biểu tượng phải tự thiết kế, không mô phỏng biểu tượng của Autodesk.

### 13.6 Nền tảng: Windows, macOS, Linux
Dùng một thiết kế chung, không làm bộ biểu tượng riêng. Chỉ các điểm dưới đây khác theo hệ điều hành.

| Điểm | Windows (V → 1.0) | macOS (alpha T26–27, GA cùng 1.0 hoặc ngay sau) | Linux (sau v1.0) |
|---|---|---|---|
| Menu ứng dụng | Nút logo (UI-A03) | Thanh menu hệ thống: TrionCAD (Giới thiệu, **Cài đặt…** ⌘, thay cho Tùy chọn, Thoát ⌘Q), Tệp, Sửa, Hiển thị…, Cửa sổ, Trợ giúp | Như Windows |
| Thanh tiêu đề | Nút cửa sổ bên phải | Ba nút cửa sổ ở góc trái: thanh tiêu đề chừa chỗ, QAT dời sang phải | Như Windows |
| Phím hệ thống | Ctrl | ⌘ cho Mới, Mở, Lưu, In, Hoàn tác, Sao chép, Dán; Ctrl giữ nghĩa CAD (ví dụ Ctrl + chuột phải) | Ctrl |
| F1–F12 | Dùng trực tiếp | Laptop phải bấm kèm Fn, nên mỗi toggle F3–F12 có thêm tổ hợp thay thế (quản lý ở UI-K06) | Dùng trực tiếp |
| Chuột phải, cử chỉ | Chuột 3 nút; touchpad (§9.2) | Ctrl + bấm hoặc bấm hai ngón = chuột phải; chụm để zoom, kéo hai ngón để pan; Magic Mouse | Chuột 3 nút |
| Cửa sổ | Tab tệp trong một cửa sổ | Thêm toàn màn hình và Split View | Theo trình quản lý cửa sổ |
| Hộp thoại tệp, in | Của hệ điều hành | Của hệ điều hành, dạng sheet gắn vào cửa sổ | Của hệ điều hành |
| Bộ gõ tiếng Việt | Unikey, EVKey, bộ gõ Windows | Bộ gõ macOS (Telex, VNI), EVKey, OpenKey | ibus, fcitx5 (Bamboo, Unikey) |
