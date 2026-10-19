# 07.3 — Đặc tả chức năng cho thiết kế UI: Tính năng Việt Nam, Viewer, tùy biến và hệ thống

> Phần 3 của bộ đặc tả UI. Ký hiệu phát hành (CC, V, VP, EA, 1.0, Sau) và mẫu mô tả xem [phần 1](07-dac-ta-ui-1-khung-ung-dung.md) §0. Cuối tài liệu là **danh mục toàn bộ màn hình** của cả ba phần (Phụ lục A).

## 0. Tên lệnh của tính năng riêng TrionCAD

Nguyên tắc "mọi lệnh đều gõ được" (phần 1 §1) áp dụng cả cho tính năng riêng. Tên dưới đây là **đề xuất**; chúng hiện trong tooltip và trợ giúp.
- Tiền tố `TC` lấy từ tên làm việc TrionCAD, đổi theo tên chính thức. Tiền tố giúp tránh trùng các lệnh mà LISP của người dùng hay tự đặt (ví dụ `TL`, `DT`).
- **Viết tắt mặc định** chỉ chốt sau khi khảo sát các bộ LISP phổ biến ở Việt Nam, để không che mất lệnh người dùng đã quen. Người dùng tự đặt viết tắt ở UI-K05.
- Công cụ miễn phí (§7) dùng **cùng tên lệnh**, để người dùng giữ thói quen khi chuyển sang TrionCAD.

| Tính năng | Lệnh | Màn hình |
|---|---|---|
| Bác sĩ Font / chuyển font hàng loạt | `FIXFONT` / `FIXFONTBATCH` | UI-J02 / UI-J03 |
| Thư viện TCVN | `TCLIB` | UI-J13 |
| Khung tên | `TCTITLE` | UI-J05 |
| Tạo khung in trong Model | `TCFRAME` | UI-J06 |
| In hàng loạt theo khung tên | `TCBATCHPLOT` | UI-H06 |
| Quản lý bộ bản vẽ | `TCSHEETS` | UI-H07 |
| Tổng chiều dài / tổng diện tích | `TCLENGTH` / `TCAREA` | UI-I04 |
| Bảng diện tích phòng | `TCROOMAREA` | UI-I05 |
| Đánh số tự động / sao chép tăng số | `TCNUMBER` / `TCINCCOPY` | UI-J07 / UI-J08 |
| Công cụ text | `TCTEXT` | UI-J09 |
| Ký hiệu kỹ thuật | `TCSYMBOL` | UI-J10 |
| Dán từ Excel / xuất bảng ra Excel | `TCEXCELPASTE` / `TCEXCELOUT` | UI-J12 |
| Xuất / nhập attribute với Excel | `TCATTOUT` / `TCATTIN` | UI-J15 |
| Thống kê thép | `TCREBAR` | UI-J14 |
| Markup | `TCMARKUP` | UI-M02 |
| Xử lý tệp hàng loạt | `TCBATCH` | UI-B14 |
| Tìm và chuyển đổi block | `TCBLOCKMATCH` | UI-G16 |
| Làm sạch hình học / kiểm tra TCVN | `TCCLEANUP` / `TCCHECK` | UI-I08 / UI-I09 |
| Chuyển cài đặt từ AutoCAD | `TCMIGRATE` | UI-K12 |
| Gửi bản vẽ lỗi | `TCREPORT` | UI-L08 |
| Kiểm tra hiệu năng máy | `TRIONBENCH` | UI-L11 |

## 1. Bác sĩ Font (sửa lỗi font tiếng Việt)

Đây là tính năng khác biệt số 1 của TrionCAD ([04](04-chien-luoc-san-pham.md) §7.1). Có từ bản Viewer.

### UI-J01 · Banner khi mở bản vẽ
- **Hiện khi:** bản vẽ dùng font không có trên máy, **hoặc** có chữ mã cũ (TCVN3/VNI).
- **Nội dung:**
  > *"Bản vẽ dùng 3 font không có trên máy và 250 đối tượng chữ mã TCVN3. TrionCAD đã tự hiển thị đúng."*

  Các nút: [Xem chi tiết] · [Chuyển sang Unicode] (VP/Pro) · [Không nhắc lại với tệp này] · [Đóng].
- **Hành vi:** không chặn thao tác. Đóng banner thì vẫn còn **chỉ báo trên thanh trạng thái** ("⚠ Font: 3 thay thế").
- **Phát hành:** V.

### UI-J02 · Palette Bác sĩ Font
- **Mở từ:** banner; chỉ báo trên thanh trạng thái; tab Công cụ VN › Bác sĩ Font; lệnh `FIXFONT`.
- **Dạng:** palette dock, có 5 khu vực.

1. **Tóm tắt:**
   - số font thiếu, số kiểu chữ;
   - số đối tượng theo bảng mã (Unicode / TCVN3 / VNI / Không rõ), kèm thanh tỉ lệ;
   - độ tin cậy nhận diện.
2. **Bảng thay thế font:**

   | Font trong bản vẽ | Loại (SHX / TTF / Big Font) | Trạng thái (có / thiếu) | Bảng mã | Font thay thế | Áp dụng cho |
   |---|---|---|---|---|---|
   | `vntime.shx` | SHX | Thiếu | TCVN3 | `TrionVN-Times.shx` (gợi ý tự động) ▾ | Phiên này / Mọi bản vẽ |

   - Sửa được từng dòng.
   - Lưu bảng vào hồ sơ người dùng.
   - **Nhập/xuất bảng** để dùng chung cả công ty.
3. **Danh sách kiểu chữ:** tên, font, bảng mã, số đối tượng. Bấm một dòng thì tô sáng các đối tượng dùng kiểu chữ đó.
4. **Xem trước:** danh sách mẫu chữ trong bản vẽ, mỗi dòng có:
   - "Hiển thị gốc" (chữ lỗi như AutoCAD sẽ thấy) và "Hiển thị đúng";
   - bảng mã, độ tin cậy;
   - bấm để zoom tới đối tượng.
5. **Chuyển đổi** (VP: lưu bản sao · Pro: sửa trực tiếp):
   - **Phạm vi:** cả bản vẽ / vùng chọn / layout hiện tại. Ô chọn: gồm block lồng, attribute, chữ kích thước, bảng, field.
   - **Bảng mã nguồn:** tự nhận diện / ép TCVN3 / ép VNI.
   - **Font Unicode đích** cho từng nhóm (ví dụ TCVN3 → `TrionVN-Times`; VNI → `TrionVN-Arial`).
   - Ô chọn: **cập nhật kiểu chữ** (đổi font của style sang font Unicode); xử lý chữ hoa (font `.VnTimeH`); giữ định dạng MTEXT.
   - **Đối tượng có độ tin cậy thấp:** bỏ qua / hỏi từng cái / vẫn chuyển.
   - **Nút:** Xem trước thay đổi (danh sách trước → sau); **Chuyển đổi** (cả lần chuyển là một bước hoàn tác); Xuất báo cáo (CSV).

- **Trạng thái:**
  - Không có vấn đề: *"Bản vẽ dùng Unicode và đủ font ✓"*.
  - Đang quét bản vẽ lớn: thanh tiến trình, vẫn thao tác được.
- **Phát hành:** V (khu vực 1–4) · VP (chuyển đổi và lưu bản sao) · EA (chuyển trực tiếp trong bản vẽ).

### UI-J03 · Chuyển font hàng loạt nhiều tệp (FIXFONTBATCH)
- **Đầu vào:** chọn tệp hoặc thư mục (có thư mục con); lọc `*.dwg`, `*.dxf`.
- **Tùy chọn chuyển:** như khu vực 5 của UI-J02.
- **Đầu ra:**
  - ghi đè (có tạo `.bak`) / lưu sang thư mục khác (giữ cấu trúc) / thêm hậu tố `_unicode`;
  - định dạng lưu: giữ nguyên / 2018 / 2013 / 2010 / 2007 / 2004;
  - tùy chọn dọn regapps.
- **Chạy:** ở nền, dùng nhiều luồng (tự động).
- **Bảng tiến trình:** tệp, trạng thái, số đối tượng đã chuyển, cảnh báo. Có Tạm dừng, Hủy; báo cáo tổng.
- **Phát hành:** VP.

### UI-J04 · Thiết lập Bác sĩ Font
Nằm trong Tùy chọn › Tệp, hoặc nút bánh răng trên palette:
- tự quét khi mở tệp (bật/tắt);
- ngưỡng tin cậy;
- bảng thay thế mặc định;
- thư mục font tin cậy;
- hiện banner hay chỉ hiện chỉ báo.

**Phát hành:** V.

## 2. Nội dung và công cụ TCVN

### UI-J13 · Thư viện TCVN
- **Mở từ:** tab Chèn › Nội dung; tab Công cụ VN › TCVN; palette Blocks › Thư viện.
- **Dạng:** palette.
- **Danh mục:**
  - **Kiến trúc:** cửa đi, cửa sổ, thiết bị vệ sinh, bếp, nội thất, cầu thang, cây xanh, người, xe.
  - **Kết cấu:** ký hiệu thép, cọc, cột, dầm, móng, mặt cắt thép hình.
  - **Điện:** thiết bị, chiếu sáng, ổ cắm, công tắc.
  - **Cấp thoát nước:** thiết bị, van.
  - **Chung:** cao độ, trục, hướng Bắc, mặt cắt, chi tiết, khung tên, mũi tên.
  - **Hatch vật liệu TCVN**; **Linetype TCVN**.
  - **Kiểu chữ, kiểu kích thước, CTB, mẫu bản vẽ TCVN.**
  - **Bộ layer** theo TCVN và theo bộ môn (KT, KC, ĐN, CTN, M&E…): xem trước danh sách layer (tên, màu, nét, độ dày); nút **Nạp vào bản vẽ** (layer trùng tên: giữ hay ghi đè thuộc tính).
- **Tìm kiếm** không phân biệt dấu ("cua di" khớp "cửa đi").
- **Mỗi mục:** xem trước; thông tin tiêu chuẩn tham chiếu, kích thước, ghi chú.
- **Chèn:** bấm rồi chỉ điểm, hoặc kéo-thả. Tự áp **tỉ lệ chú thích hiện hành** với ký hiệu.
- **Khác:**
  - Yêu thích;
  - **Thư viện của tôi** (đưa đối tượng đang chọn vào thư viện: tên, danh mục, điểm gốc, mô tả);
  - vị trí thư viện (thư mục máy hoặc **thư mục mạng dùng chung cho cả công ty**);
  - cập nhật nội dung mới từ TrionCAD (P2).
- **Phát hành:** EA (cơ bản) · 1.0.

### UI-J11 · Chọn mẫu bản vẽ TCVN
- **Mở từ:** Start › Tạo mới; `NEW`.
- **Lọc theo:** khổ (A0–A4), hướng, bộ môn (kiến trúc, kết cấu, M&E, giao thông, cơ khí), tỉ lệ chính, kiểu khung tên (mẫu TCVN hoặc mẫu công ty).
- **Xem trước;** nút Tạo. Nhớ lựa chọn gần nhất.
- **Phát hành:** EA.

### UI-J05 · Khung tên (TCTITLE)
- **Chèn khung tên:**
  - chọn mẫu (TCVN 5571 ngang/dọc, mẫu công ty);
  - khổ giấy;
  - tỉ lệ (trong Model) hoặc 1:1 (trong layout);
  - điểm chèn.

  Các ô được **tự điền** từ thông tin dự án (UI-H07) và field.
- **Trình sửa khung tên:**
  - biểu mẫu theo các ô: đơn vị thiết kế, công trình, hạng mục, tên bản vẽ, ký hiệu, giai đoạn, tỉ lệ, người vẽ / kiểm / chủ trì / chủ nhiệm / duyệt, ngày, số tờ / tổng số tờ;
  - logo (ảnh);
  - ánh xạ attribute ↔ field;
  - "Áp dụng cho tất cả khung tên cùng loại trong bản vẽ / trong bộ bản vẽ";
  - lưu thành **mẫu của công ty**.
- **Phát hành:** EA (chèn từ thư viện, sửa attribute) · 1.0 (trình sửa đầy đủ).

### UI-J06 · Tạo khung in trong Model theo tỉ lệ (TCFRAME)
- **Thiết lập:** khổ giấy và hướng; tỉ lệ (1:100…); mẫu khung tên.
- **Cách đặt:** chỉ điểm chèn từng khung, **hoặc** mảng nhiều khung (n cột × m hàng, khoảng cách).
- **Tự đánh số tờ.**
- **Kết quả:** các block khung tên có attribute, dùng ngay được với **In hàng loạt theo khung tên** (UI-H06).
- **Phát hành:** 1.0.

## 3. Công cụ văn bản, ký hiệu, Excel

### UI-J07 · Đánh số tự động (TCNUMBER)
- **Đích:** text có sẵn / attribute của block (chọn tag) / tạo text mới tại điểm chỉ.
- **Định dạng:** tiền tố; số bắt đầu; bước; số chữ số (01, 001); hậu tố; hoặc chữ cái (A, B, C…).
- **Thứ tự:** theo thứ tự chọn / X tăng / Y giảm / theo hàng–cột.
- **Hành vi:**
  - **xem trước giá trị ngay trên vùng vẽ** trước khi áp dụng;
  - chế độ "chỉ điểm từng cái";
  - lưu preset (trục, phòng, cọc, thiết bị).
- **Phát hành:** 1.0.

### UI-J08 · Sao chép tăng số (TCINCCOPY)
- Chọn text hoặc attribute nguồn có chứa số → chỉ điểm vị trí các bản sao → số tăng theo bước.
- Hiểu được nhiều dạng: `C1` → `C2`, `1.05` → `1.10`, `A` → `B`.
- Tùy chọn phần số nào được tăng (cuối / đầu / thứ n).
- **Phát hành:** 1.0.

### UI-J09 · Công cụ text (TCTEXT)
- **Đổi hoa/thường đúng tiếng Việt:** HOA TOÀN BỘ / thường / Hoa đầu câu / Hoa Mỗi Từ.
- **Nối text:** nhiều TEXT thành một TEXT hoặc MTEXT; chọn thứ tự và ký tự nối.
- **Tách text:** MTEXT thành nhiều TEXT theo dòng; hoặc tách theo ký tự.
- **Canh hàng text:** trái / phải / giữa theo một đường; chia đều khoảng cách.
- **Thay tiền tố, hậu tố hàng loạt.**
- **Đổi số trong text:** cộng hoặc nhân giá trị số, ví dụ dời toàn bộ cao độ +0,450.
- **Phát hành:** 1.0.

### UI-J10 · Ký hiệu kỹ thuật (TCSYMBOL)
- **Cao độ:**
  - chỉ điểm để đặt ký hiệu (mẫu TCVN: mặt bằng, mặt cắt);
  - giá trị tính từ tọa độ Y so với gốc ±0.000 đã chọn, hoặc nhập tay;
  - định dạng `+1.200` (3 chữ số thập phân, đơn vị m);
  - tự cập nhật khi di chuyển (P2).
- **Trục định vị:**
  - nhập lưới trục bằng bảng (tên trục, khoảng cách) cho trục ngang (1, 2, 3…) và trục dọc (A, B, C…), hoặc lấy từ đối tượng có sẵn;
  - tự vẽ đường trục (linetype tâm), bóng trục (vòng tròn và chữ), **kích thước khoảng cách và kích thước tổng**;
  - sửa lại lưới trục sau khi tạo (P2).
- **Ký hiệu mặt cắt và chi tiết:** số hiệu, mũi tên hướng nhìn, tham chiếu tờ ("1-1 / KT 05").
- **Khác:** mũi tên hướng Bắc; ký hiệu độ dốc (`i = 2%`); ký hiệu vật liệu (liên kết gallery hatch TCVN).
- **Phát hành:** 1.0.

### UI-J14 · Thống kê thép (TCREBAR, P2)
- **Nguồn dữ liệu:** nhận dạng ký hiệu thép trên bản vẽ (dạng `2Ø16`, `Ø8a200`, số hiệu thanh), hoặc lấy từ block thanh thép có attribute.
- **Bảng thống kê theo mẫu TCVN:** số hiệu, hình dạng, đường kính, chiều dài một thanh, số lượng, tổng chiều dài, khối lượng (theo bảng trọng lượng đơn vị). Có tổng hợp theo đường kính.
- **Kiểm tra** ký hiệu thiếu thông tin; xuất Excel.
- **Phát hành:** Sau.

### UI-J12 · Excel (TCEXCELPASTE, TCEXCELOUT)
- **Dán từ Excel:** khi dán một vùng ô, hỏi dán thành:
  - **Bảng TrionCAD** (giữ định dạng cơ bản, độ rộng cột, ô gộp);
  - text thường;
  - đối tượng OLE (P2).

  Cỡ chữ theo tỉ lệ chú thích; chọn kiểu bảng.
- **Xuất ra Excel:** chọn một hoặc nhiều bảng → XLSX hoặc CSV; giữ ô gộp và định dạng số; mở tệp sau khi xuất.
- **Phát hành:** EA (CSV) · 1.0.

### UI-J15 · Xuất và nhập attribute với Excel (TCATTOUT, TCATTIN)
Quy trình quen thuộc: xuất attribute ra Excel, sửa hàng loạt, rồi nhập lại vào bản vẽ.
- **Xuất:**
  - chọn block theo tên hoặc theo vùng chọn; phạm vi cả bản vẽ hoặc layout hiện tại;
  - chọn tag cần xuất;
  - tệp XLSX hoặc CSV gồm cột mã đối tượng (handle; ẩn được; dùng để nhập lại đúng đối tượng), tên block, layout và các tag.
- **Nhập:** chọn tệp → **xem trước thay đổi** (giá trị cũ → mới; dòng không tìm thấy đối tượng; tag không tồn tại) → Áp dụng. Cả lần nhập là một bước hoàn tác.
- **Phát hành:** VP (xuất) · 1.0 (nhập).

## 4. Viewer và Viewer Pro

### UI-M01 · Ribbon Viewer
| Tab | Panel và nút | Phát hành |
|---|---|---|
| **Xem** | Điều hướng (Pan, Zoom ▾, View đã lưu); Layout (chuyển layout); Layer (dropdown, Quản lý layer ở chế độ xem); Kiểu hiển thị; Palette (Thuộc tính, Tham chiếu ngoài, Bác sĩ Font, Đếm) | V (Đếm: VP) |
| **Đo** | Đo nhanh; Khoảng cách; Diện tích; Góc; Bán kính; Tọa độ điểm; Liệt kê thông tin; **Tổng chiều dài / tổng diện tích** | V (tổng: VP) |
| **Ghi chú** | Công cụ markup (UI-M02) | VP |
| **In & Xuất** | In; Xem trước; In nhiều layout; **In hàng loạt theo khung tên**; Xuất PDF; Xuất ảnh | V (hàng loạt: VP) |
| **Công cụ** | So sánh bản vẽ; Chuyển font hàng loạt; Đếm block; Xuất bảng thống kê (attribute, số đếm ra Excel); Tìm chữ | VP |

- **Nâng cấp:** panel cố định cuối ribbon có nút **"Sửa bản vẽ? Dùng thử Pro 30 ngày"**.
- **Thanh trạng thái giản lược:** tọa độ, MODEL/PAPER, bắt điểm (khi đo), hiển thị lineweight, tỉ lệ viewport (chỉ xem), chỉ báo font, tác vụ nền, thông báo, huy hiệu phiên bản.

### UI-M02 · Markup, ghi chú (TCMARKUP, VP)
- **Công cụ:**
  - đám mây sửa đổi, hình chữ nhật, ellipse, mũi tên, đường thẳng, nét tự do, bút dạ (highlight);
  - chữ ghi chú, ghi chú có đường dẫn;
  - dấu tick;
  - đo (khoảng cách, diện tích, lưu thành markup);
  - **con dấu** (Đã duyệt / Cần sửa / Không duyệt);
  - chụp một vùng màn hình.
- **Thuộc tính:** màu, độ dày, độ mờ, cỡ chữ.
- **Danh sách markup:**
  - mỗi mục có loại, **tác giả**, thời gian, trạng thái (Mở / Đã xử lý), bình luận (luồng đơn giản);
  - lọc theo tác giả hoặc trạng thái; zoom tới; ẩn/hiện; xóa.
- **Lưu trữ:**
  - mặc định là **tệp ghi chú riêng** đặt cạnh bản vẽ (`.tcmk`), **không sửa tệp DWG**;
  - xuất PDF (bản vẽ kèm markup);
  - nhập markup từ tệp của người khác (gộp);
  - bản Pro có thêm "Đưa markup vào bản vẽ" (layer `TC-MARKUP`).
- **Phát hành:** VP.

### UI-M03 · Chế độ chỉ xem
- **Thanh tiêu đề:** "[Chỉ xem]".
- **Gõ lệnh sửa:** hiện thông báo nâng cấp (phần 1 §2).
- **Palette Thuộc tính:** chỉ đọc nhưng **sao chép được giá trị**.
- **"Lưu trạng thái hiển thị"** (VP): lưu view và trạng thái bật/tắt layer vào tệp phụ `.tcview`, không sửa DWG.
- **Phát hành:** V.

### UI-M04 · Layer trong Viewer
- Bật/tắt, đóng băng **tạm thời**; cô lập layer; tìm layer.
- Nút "Khôi phục như tệp gốc".
- Ghi chú rõ: "Thay đổi không lưu vào bản vẽ".
- **Phát hành:** V.

## 5. Tự động hóa và tùy biến

### UI-K01 · Nạp ứng dụng (APPLOAD)
- **Thành phần:**
  - duyệt tệp (`.lsp`, `.vlx`/`.fas` (P2), `.dcl`, `.scr`, plugin TrionCAD);
  - Nạp / Gỡ;
  - **danh sách đã nạp** (tệp, đường dẫn);
  - danh sách lịch sử, ô chọn "Thêm vào lịch sử";
  - **Startup Suite** (nút Nội dung… → danh sách tự nạp khi khởi động: thêm, bớt);
  - dòng trạng thái ("Đã nạp *thongkethep.lsp*").
- **Phát hành:** EA.

### UI-K02 · Cảnh báo bảo mật khi nạp mã
- **Dạng:** modal.
- **Nội dung:**
  - tên tệp, đường dẫn;
  - **lý do cảnh báo** (nằm ngoài thư mục tin cậy / là tệp `acad*.lsp` nằm cạnh bản vẽ / chưa ký số);
  - chi tiết (kích thước, ngày sửa).
- **Lựa chọn:** Nạp một lần / Luôn nạp (thêm thư mục vào danh sách tin cậy) / **Không nạp** (là nút mặc định).
- **Phát hành:** EA.

### UI-K03 · Tùy chọn bảo mật (SECURITYOPTIONS)
- **Mức `SECURELOAD`:**
  - nạp từ mọi nơi không cảnh báo;
  - **cảnh báo khi nằm ngoài thư mục tin cậy** (mặc định);
  - chỉ nạp từ thư mục tin cậy.
- **Danh sách thư mục tin cậy:** Thêm, Bớt. Cảnh báo nếu thư mục ai cũng ghi được.
- **Ô chọn:** cảnh báo khi bản vẽ đi kèm `acad.lsp`/`acaddoc.lsp`; chặn tự nạp LISP từ thư mục chứa bản vẽ.
- **Phát hành:** EA.

### UI-K04 · Chạy script (SCRIPT)
- Chọn tệp `.scr`.
- Khi chạy: dòng đang thực thi, tiến trình, Esc để dừng. Lỗi thì báo **số dòng** và nội dung.
- Chạy một script trên nhiều bản vẽ: P2.
- **Phát hành:** EA.

### UI-K05 · Viết tắt lệnh: trình sửa PGP (ALIASEDIT)
- **Bảng:** Viết tắt | Lệnh | Mô tả (tự điền) | Nguồn (mặc định / người dùng / nhập từ AutoCAD).
- **Thao tác:** tìm kiếm; Thêm, Sửa, Xóa; **cảnh báo xung đột** (viết tắt trùng nhau, hoặc che mất tên một lệnh có sẵn).
- **Nhập/xuất:** **nhập `acad.pgp`** (gộp hoặc thay thế); xuất `.pgp`; đặt lại mặc định.
- Áp dụng ngay, không cần khởi động lại.
- **Phát hành:** EA.

### UI-K06 · Phím tắt
- **Bảng:** Lệnh/macro | Phím tắt | Phạm vi.
- **Thao tác:** tìm kiếm; bấm tổ hợp phím để gán; **phát hiện trùng**; phím ghi đè tạm thời (P2); nhập/xuất; **in bảng phím tắt** (PDF một trang).
- **Phát hành:** 1.0.

### UI-K07 · Tùy biến giao diện (CUI) (P2)
- **Cây tùy biến:** workspace, QAT, tab và panel ribbon, toolbar, menu, menu chuột phải, phím tắt, hành động nhấp đúp, nút chuột, tệp LISP đi kèm, tệp tùy biến phụ.
- **Danh sách lệnh:** tìm kiếm; tạo lệnh mới (tên, macro, biểu tượng, tooltip).
- **Khác:** khung thuộc tính; kéo-thả để thêm; xem trước; **nhập CUIX của AutoCAD** (một phần: ribbon, toolbar, menu); tab chuyển dữ liệu giữa hai tệp tùy biến.
- **Phát hành:** Sau. Từ 1.0 đã nhập được CUIX một phần qua UI-K12.

### UI-K08 · Workspace (WORKSPACE, WSSAVE)
Lưu bố cục hiện tại thành workspace; thiết lập (workspace nào hiện trong menu, thứ tự, tự lưu thay đổi khi chuyển); chuyển workspace. **Phát hành:** 1.0.

### UI-K09 · Bảng điều khiển LISP (P2)
- **REPL:** ô nhập có lịch sử; kết quả có ghi kiểu dữ liệu.
- **Khác:** xem biến toàn cục; danh sách tệp đã nạp, nạp lại; dừng khi lỗi kèm vết gọi hàm; "**Mở trong VS Code**" để gỡ lỗi; đo thời gian chạy.
- **Phát hành:** Sau.

### UI-K10 · Quản lý plugin (P2)
- **Danh sách:** tên, nhà phát triển, phiên bản, **đã ký số hay chưa**, trạng thái.
- **Thao tác:** bật/tắt; chi tiết (quyền, các lệnh plugin thêm vào); cài từ tệp gói; cập nhật; gỡ.
- Hỏi xác nhận tin cậy khi cài plugin chưa ký.
- **Phát hành:** Sau.

### UI-K11 · Hộp thoại DCL
- Dựng từ tệp `.dcl` của LISP. Theo chủ đề và DPI của TrionCAD, nhìn nhất quán với hộp thoại gốc.
- DCL lỗi thì báo số dòng và nội dung lỗi.
- **Người thiết kế cần làm:** bộ quy tắc hiển thị cho từng loại tile DCL (button, edit_box, list_box, popup_list, toggle, radio, slider, image, text, boxed_row/column, ok_cancel).
- **Phát hành:** EA.

### UI-K12 · Chuyển cài đặt từ AutoCAD (TCMIGRATE)
Trình hướng dẫn 5 bước:
1. **Phát hiện AutoCAD** trên máy (phiên bản, hồ sơ), hoặc chọn tay thư mục/tệp (`acad.pgp`, `.arg`, `.cuix`…).
2. **Chọn mục cần chuyển:**
   - viết tắt lệnh;
   - đường dẫn hỗ trợ;
   - mẫu bản vẽ;
   - CTB/STB, PC3/PMP;
   - font SHX trong đường dẫn hỗ trợ;
   - LISP khởi động và Startup Suite;
   - thư mục tin cậy;
   - màu và thiết lập bắt điểm;
   - PAT/LIN tùy biến;
   - **CUIX, một phần** (1.0): toolbar, menu, macro, viết tắt và phím tắt có trong tệp. Ribbon và tool palette: P2.
3. **Xung đột** (ví dụ viết tắt trùng) và cách xử lý: giữ của TrionCAD / ghi đè / đổi tên.
4. **Chạy.**
5. **Báo cáo** (đã chuyển / bỏ qua / cảnh báo), kèm "Mở hướng dẫn chuyển đổi".

**Phát hành:** EA (viết tắt, đường dẫn, mẫu, CTB, LISP) · 1.0 (phần còn lại, gồm CUIX một phần theo [06](06-lo-trinh-trien-khai.md) §5). Toolbar và menu nhập từ CUIX hiện như toolbar và menu của workspace Cổ điển, bật được trong mọi workspace. Muốn sửa chi tiết thì cần trình CUI (UI-K07, Sau).

### UI-K13 · Biến hệ thống (SETVAR, SYSVARMONITOR)
- **Trình duyệt** (1.0, gọi được bằng cả `SYSVDLG`):
  - bảng Tên | Giá trị | Mặc định | Lưu ở đâu (bản vẽ / người dùng / không lưu) | Mô tả tiếng Việt;
  - tìm kiếm; lọc "khác mặc định";
  - sửa trực tiếp, có kiểm tra kiểu và miền giá trị; đặt lại mặc định; xuất/nhập danh sách.
- **Theo dõi** (1.0):
  - danh sách biến cần giữ giá trị. Mặc định gồm các biến mà LISP hay đổi rồi quên trả lại: `OSMODE`, `CMDECHO`, `FILEDIA`, `PICKFIRST`, `ORTHOMODE`…;
  - khi một biến bị đổi, thanh trạng thái hiện cảnh báo (phần 1 §10); bấm vào để xem giá trị cũ → mới và **khôi phục**.
- Trên dòng lệnh, `SETVAR` và gõ thẳng tên biến dùng được từ EA.
- **Phát hành:** EA (dòng lệnh) · 1.0.

## 6. Hệ thống, tài khoản, hỗ trợ

### UI-L01 · Kích hoạt giấy phép
- **Các cách kích hoạt:**
  - đăng nhập tài khoản TrionCAD;
  - nhập key;
  - **máy chủ giấy phép** (Doanh nghiệp: nhập địa chỉ máy chủ floating);
  - **kích hoạt offline** (UI-L02).
- **Lỗi được giải thích rõ:** key sai; **hết lượt kích hoạt** (kèm liên kết tới trang quản lý thiết bị); không kết nối được.
- **Giáo dục:** xác minh bằng email của trường (ví dụ đuôi `.edu.vn`) hoặc mã do trường cấp; hạn 1 năm, gia hạn khi vẫn là sinh viên hoặc giảng viên.
- **Floating hết ghế:** *"Đã dùng hết 10/10 ghế"* — [Thử lại] · [Báo khi có ghế trống] · [Mở ở chế độ Viewer].
- **Màn hình thành công:** phiên bản, thời hạn, tên thiết bị.
- **Phát hành:** VP.

### UI-L02 · Kích hoạt offline
1. Tạo **tệp yêu cầu** (`.req`), **kèm mã QR** để quét bằng điện thoại (P1, tiện cho công trường không có mạng).
2. Hướng dẫn: mang tệp sang máy có mạng, hoặc quét QR, mở trang kích hoạt, tải **tệp phản hồi** (`.lic`).
3. Nhập tệp phản hồi.

**Phát hành:** VP.

### UI-L03 · Trạng thái giấy phép
- **Thông tin:**
  - phiên bản (Viewer / Viewer Pro / Pro / Doanh nghiệp / Giáo dục / Dùng thử);
  - loại (thuê bao / vĩnh viễn);
  - hạn dùng; quyền cập nhật đến ngày nào;
  - **thiết bị đã kích hoạt** (tên máy, ngày, nút Hủy kích hoạt).
- **Với floating:** máy chủ; số ghế đang dùng / tổng; **mượn giấy phép offline N ngày** (P2).
- **Trạng thái cần báo:**
  - thuê bao sắp hết hạn: nhắc trước 30, 7 và 1 ngày;
  - đã hết hạn: chuyển về Viewer, bản vẽ vẫn mở và in được (như UI-L05);
  - nếu giấy phép cần xác minh định kỳ mà lâu chưa kết nối: huy hiệu phiên bản đếm ngược số ngày còn dùng offline.
- **Nút:** Gia hạn / Nâng cấp (mở web), Đổi key, Đăng xuất.
- **Phát hành:** VP.

### UI-L04 · Trang nâng cấp
- **Hiện khi:** dùng tính năng bị khóa.
- **Nội dung:** tên tính năng; lợi ích (1–2 câu); ảnh minh họa; phiên bản cần có; giá "từ … đ/năm".
- **Nút:** **Dùng thử 30 ngày** (nếu chưa dùng thử); Mua (mở web); Để sau.
- **Hành vi:** mỗi tính năng chỉ hiện tối đa một lần trong một phiên.
- **Phát hành:** V.

### UI-L05 · Dùng thử
- **Bắt đầu:** cần email.
- **Trong khi dùng:** nhắc hạn ở các mốc 7, 3, 1 ngày.
- **Khi hết hạn:** màn hình giải thích:
  - phần mềm chuyển về Viewer;
  - **dữ liệu an toàn, vẫn mở và in được**;
  - kèm ưu đãi mua.
- **Phát hành:** EA.

### UI-L15 · Cổng quản trị doanh nghiệp và máy chủ giấy phép
- **Cổng web:**
  - danh sách ghế, người dùng, thiết bị, lịch sử dùng;
  - gán ghế cho email nhân viên.
- **Triển khai:**
  - tải bộ cài MSI;
  - cấu hình mặc định cho cả công ty: hồ sơ, **đường dẫn mẫu chung**, **thư viện chung**, CTB chung.
- **Máy chủ floating:** một ứng dụng dịch vụ có giao diện quản lý nhỏ (trạng thái, ghế đang dùng, nhật ký).
- **Phát hành:** 1.0.

### UI-L16 · Thanh toán và hóa đơn (trên web)
- **Mua:** chọn gói, số ghế, thuê bao hay vĩnh viễn.
- **Thanh toán:** **VNPay, MoMo, ZaloPay**, chuyển khoản, thẻ quốc tế.
- **Thông tin xuất hóa đơn điện tử:** tên công ty, **mã số thuế**, địa chỉ, email nhận hóa đơn.
- **Khác:** mã giảm giá hoặc mã đại lý.
- **Trang tài khoản:** hóa đơn, gia hạn tự động, quản lý ghế.
- Giao diện web dùng chung hệ thống thiết kế (design system) với ứng dụng.
- **Phát hành:** VP.

### UI-L06 · Cập nhật
- **Thông báo bản mới:** toast, cùng một mục trong trung tâm thông báo.
- **Hộp chi tiết:** phiên bản, **ghi chú phát hành tiếng Việt**, dung lượng.
- **Lựa chọn:** Cài ngay / Cài khi đóng ứng dụng / Nhắc sau.
- **Khác:** tải ở nền; kiểm tra chữ ký trước khi cài; quay về bản trước (P2); chọn kênh ổn định/beta trong Tùy chọn.
- **Hết quyền cập nhật** (giấy phép vĩnh viễn): vẫn báo có bản mới nhưng ghi rõ *"Bản 1.3 cần gia hạn cập nhật"*, kèm nút Gia hạn. Bản đang dùng không bị ảnh hưởng.
- **Phát hành:** V.

### UI-L07 · Báo lỗi sau sự cố (crash)
- **Lần mở sau:** "TrionCAD đã đóng bất thường lần trước".
- **Bước 1:** phục hồi bản vẽ (UI-B07).
- **Bước 2:** gửi báo lỗi gồm:
  - mô tả (không bắt buộc), email (không bắt buộc);
  - ô chọn **"Đính kèm bản vẽ đang mở"**, mặc định **tắt**;
  - nút "Xem nội dung sẽ gửi".
- **Lựa chọn:** Gửi / Không gửi. Ô chọn "Luôn tự gửi báo lỗi (không kèm bản vẽ)".
- **Phát hành:** V.

### UI-L08 · Gửi bản vẽ lỗi (TCREPORT)
- **Mở từ:** menu Trợ giúp; banner khi hiển thị lỗi; chuột phải trên tab tệp.
- **Trường:**
  - loại vấn đề (hiển thị sai, chậm, không mở được, in sai…); mô tả;
  - **ảnh chụp vùng màn hình**;
  - tệp đính kèm (tự gợi ý thêm xref và font liên quan);
  - **ô chọn quyền sử dụng:** "Cho phép TrionCAD dùng tệp này trong bộ kiểm thử nội bộ (không chia sẻ ra ngoài)";
  - email nhận phản hồi.
- **Sau khi gửi:** hiện **mã theo dõi**.
- **Phát hành:** V.

### UI-L09 · Quyền riêng tư
- **Mức telemetry:**
  - Tắt;
  - Cơ bản: lỗi và hiệu năng, ẩn danh;
  - Đầy đủ: thêm thống kê tần suất dùng lệnh.
- **Minh bạch:** mô tả cụ thể dữ liệu thu thập; **máy chủ đặt tại Việt Nam**; xem nhật ký dữ liệu đã gửi; yêu cầu xóa dữ liệu.
- **Bật/tắt:** báo crash tự động; tự kiểm tra cập nhật.
- **Phát hành:** V.

### UI-L10 · Thiết lập đồ họa (GRAPHICSCONFIG)
- **Backend:** Tự động / D3D12 / D3D11 / Vulkan / Metal / OpenGL. Hiển thị **GPU đang dùng** (chọn được GPU rời hoặc tích hợp) và VRAM.
- **Mức chất lượng:** Tự động / Cao / Cân bằng / Tiết kiệm.
- **Bật/tắt:** MSAA; làm mượt nét; giới hạn khung hình khi chạy pin; mức LOD; hiện FPS (cho người dùng nâng cao).
- **Nút:** "Chạy kiểm tra hiệu năng" (UI-L11); Đặt lại mặc định.
- **Chẩn đoán:** phiên bản driver, lỗi đồ họa đã gặp, sao chép báo cáo.
- **Phát hành:** V.

### UI-L11 · Kiểm tra hiệu năng máy (TRIONBENCH)
- **Các bước** (có tiến trình): CPU một luồng, CPU đa luồng, SIMD, các cảnh render GPU (1 triệu và 10 triệu đối tượng, hatch dày, chữ dày), đọc đĩa.
- **Kết quả:**
  - điểm theo từng mục;
  - **so với máy tham chiếu** (máy văn phòng cũ / tầm trung / máy trạm);
  - **cấu hình đề xuất** kèm nút "Áp dụng";
  - lịch sử các lần đo.
- Ô chọn "Gửi kết quả ẩn danh để TrionCAD tối ưu cho phần cứng phổ biến".
- **Phát hành:** V.

### UI-L12 · Trợ giúp và tìm lệnh (HELP)
- **Cửa sổ trợ giúp tiếng Việt:**
  - có tìm kiếm;
  - mục lục: Bắt đầu, Chuyển từ AutoCAD, Lệnh A–Z, Biến hệ thống, LISP, Câu hỏi thường gặp, Phím tắt;
  - tài liệu offline đóng gói sẵn, cập nhật thêm khi có mạng.
- **F1 theo ngữ cảnh:** mở đúng trang của lệnh hoặc hộp thoại đang dùng.
- **Ô Tìm lệnh** trên thanh tiêu đề, kết quả gồm:
  - lệnh (chạy ngay);
  - **vị trí trên ribbon** ("Trang chủ › Sửa");
  - bài trợ giúp;
  - thiết lập liên quan.
- **Phát hành:** V.

### UI-L13 · Giới thiệu (ABOUT)
- Phiên bản, bản build, kênh; giấy phép; thông tin hệ thống (sao chép được).
- **Danh sách thư viện mã nguồn mở và giấy phép** (bắt buộc với LGPL/BSD).
- Điều khoản sử dụng, chính sách riêng tư.
- Ghi chú nhãn hiệu ("AutoCAD® là nhãn hiệu của Autodesk, Inc.").
- Liên hệ hỗ trợ.
- **Phát hành:** V.

### UI-L14 · Trung tâm thông báo
- **Loại thông báo:** cập nhật, giấy phép, tác vụ nền xong hoặc lỗi, mẹo, tin cộng đồng.
- **Thao tác:** lọc; đánh dấu đã đọc; xóa; chọn loại thông báo muốn nhận.
- **Phát hành:** V.

### UI-L17 · Liên hệ hỗ trợ
- **Kênh:** **Zalo OA** (mã QR, liên kết); biểu mẫu yêu cầu hỗ trợ (mô tả, ảnh chụp, **tự đính kèm nhật ký và thông tin hệ thống**); hỗ trợ từ xa (hướng dẫn UltraViewer/TeamViewer); diễn đàn cộng đồng.
- **Góp ý:** nút Góp ý (ý tưởng, lỗi nhỏ, kèm ảnh chụp); khảo sát NPS trong ứng dụng tối đa một lần mỗi quý, bỏ qua được.
- **Bản beta:** huy hiệu *Beta* trên thanh tiêu đề; nút Góp ý luôn hiện.
- **Phát hành:** V.

### UI-L18 · Trình cài đặt và tích hợp hệ điều hành
- **Trình cài đặt Windows** (bản macOS là gói kéo vào Applications):
  - ngôn ngữ; điều khoản; thư mục cài đặt;
  - **liên kết tệp** `.dwg`, `.dxf`, `.dwt`: đăng ký TrionCAD là ứng dụng mở được các tệp này. Ô "Đặt làm mặc định" bật sẵn nếu máy chưa có ứng dụng CAD; nếu đã có AutoCAD thì tắt sẵn, kèm giải thích. Windows có thể hỏi lại người dùng ở lần mở tệp đầu tiên;
  - lối tắt Desktop và Start Menu;
  - tiến trình; màn hình hoàn tất có nút "Mở TrionCAD".
- **Tích hợp hệ điều hành:**
  - menu chuột phải trên tệp: "Mở bằng TrionCAD", "Mở chỉ đọc";
  - jump list tệp gần đây trên thanh tác vụ;
  - **thumbnail và khung xem trước DWG trong File Explorer** (P1).
- **Trong ứng dụng:** Tùy chọn › Hệ thống có nút "Đặt TrionCAD làm ứng dụng mặc định cho DWG" (mở trang ứng dụng mặc định của hệ điều hành).
- **Gỡ cài đặt:** hỏi giữ hay xóa thiết lập người dùng; hỏi lý do gỡ (không bắt buộc).
- **Cài đặt im lặng** cho doanh nghiệp: tham số dòng lệnh, không có giao diện (UI-L15).
- **Phát hành:** V · 1.0 (thumbnail trong Explorer, cài đặt im lặng).

## 7. Công cụ miễn phí chạy trên AutoCAD, ZWCAD, BricsCAD (CC)

Phát hành tháng 2–3, trước Viewer hơn một năm ([04](04-chien-luoc-san-pham.md) §2.3, [06](06-lo-trinh-trien-khai.md) §2). Đây là **sản phẩm đầu tiên người dùng thấy**, nên cần thiết kế trước tiên.
- **Ràng buộc:** chạy dưới dạng LISP trong ứng dụng CAD khác, nên giao diện chỉ gồm **hộp thoại DCL** (do chính ứng dụng đó vẽ) và dòng lệnh. Không có palette, không có ribbon riêng, không dùng biểu tượng hay giao diện của Autodesk.
- **Kiểm tra trên:** AutoCAD (bản đầy đủ và LT 2024+), ZWCAD, BricsCAD.
- **Lệnh:** cùng tên với TrionCAD (§0), ví dụ `FIXFONT`, `TCLIB`.

### UI-N01 · Bác sĩ Font bản LISP
- **Hộp thoại quét:** phạm vi (cả bản vẽ / vùng chọn); kết quả gồm số font thiếu và số đối tượng theo bảng mã (TCVN3 / VNI / Unicode).
- **Hộp thoại chuyển:** bảng mã nguồn (tự nhận diện / ép TCVN3 / ép VNI); font Unicode đích; cập nhật kiểu chữ; gồm cả block lồng và attribute; nút Chuyển; báo cáo trên dòng lệnh.
- Chân hộp thoại: *"TrionCAD Viewer làm việc này tự động khi mở bản vẽ — [Đăng ký nhận bản dùng thử]"*.

### UI-N02 · Thư viện TCVN bản LISP
- **Hộp thoại duyệt:** danh mục; ảnh xem trước (tile `image` của DCL, dựng từ slide); ô tìm kiếm; nút Chèn. Tỉ lệ chèn theo tỉ lệ bản vẽ.
- Mẫu bản vẽ, CTB, bộ layer và hatch TCVN đi kèm dưới dạng tệp, có hướng dẫn cài.

### UI-N03 · Cài đặt và đăng ký
- **Gói tải về** (ZIP) có tệp hướng dẫn. Một lệnh cài đặt thêm thư mục vào đường dẫn hỗ trợ và Startup Suite.
- **Trang tải về** (web): đăng ký bằng Zalo hoặc email trước khi tải; đồng thời đăng ký **danh sách chờ TrionCAD Viewer**.
- **Phát hành:** CC (tháng 2–3).

## 8. Ngoài phạm vi tài liệu này

Các nhóm sau thuộc P3–P4 hoặc còn chờ quyết định kỹ thuật. Mỗi nhóm sẽ có đặc tả UI riêng khi được đưa vào lộ trình.
- **3D:** mô hình hóa solid, surface, mesh; gizmo; chọn đối tượng con. Phụ thuộc quyết định kernel 3D ở cổng G3 ([05](05-kien-truc-ky-thuat.md) §7.1). Bản 1.0 chỉ có phần xem 3D: xoay 3D, ViewCube, kiểu hiển thị (phần 1 §7.6, §9.13).
- **Render, vật liệu, ánh sáng, camera; hình chiếu sinh từ mô hình 3D.**
- **Đám mây và cộng tác:** chia sẻ link và Zalo, lịch sử phiên bản, lưu trữ đám mây, trình xem web, ứng dụng mobile, đồng chỉnh sửa ([05](05-kien-truc-ky-thuat.md) §18).
- **AI:** trợ lý tiếng Việt (MCP), giải thích lỗi TCVN, sinh LISP từ mô tả, đọc PDF/ảnh scan thành DWG, nhập ghi chú sửa đổi ([05](05-kien-truc-ky-thuat.md) §19).
- **Định dạng khác:** DGN, DWF underlay, point cloud, mô hình phối hợp, STEP/IGES/IFC; vị trí địa lý và bản đồ nền VN-2000.
- **Tự động hóa khác:** Action Recorder, chợ ứng dụng, console .NET/Python, gói `.bundle`.
- **Gói chuyên ngành** (kiến trúc, cơ khí, địa chính…).
- **Trang web marketing** (trang chủ, bảng giá, tải về). Tài liệu này chỉ đặc tả các trang web gắn với ứng dụng: thanh toán (UI-L16), cổng quản trị (UI-L15), đăng ký tải công cụ miễn phí (UI-N03).

---

## Phụ lục A — Danh mục màn hình và thành phần

Ký hiệu cột "Phần": 1, 2, 3 là ba tài liệu 07.1, 07.2, 07.3. Cột "Phát hành" ghi mốc **sớm nhất có thành phần đó**; một số thành phần có thêm chức năng ở các mốc sau, xem chi tiết trong đặc tả.

| Mã | Thành phần | Phần | Phát hành |
|---|---|---|---|
| UI-A01 | Cửa sổ chính, bố cục, workspace | 1 §3 | V |
| UI-A02 | Tab Start | 1 §4 | V |
| UI-A03 | Menu ứng dụng | 1 §6 | V |
| UI-A04 | Quick Access Toolbar | 1 §6 | V |
| UI-A05 | Ribbon: các tab (kể cả tab Mở rộng), panel, tab ngữ cảnh, thanh menu cổ điển | 1 §7 | EA (Viewer: UI-M01) |
| UI-A06 | Dòng lệnh | 1 §8 | V |
| UI-A07 | Vùng vẽ: con trỏ, điều hướng, chọn, tô sáng | 1 §9.1–9.4 | V |
| UI-A08 | Thanh trạng thái | 1 §10 | V |
| UI-A09 | Tab tệp | 1 §5.6 | V |
| UI-A10 | Tab layout, viewport | 1 §9.12 | V |
| UI-A11 | Menu chuột phải | 1 §9.9 | V |
| UI-A12 | Grip, bắt điểm, theo dõi, nhập động, xem trước trong lệnh | 1 §9.5–9.8 | V (bắt điểm khi đo) / EA |
| UI-A13 | Nhấp đúp, kéo-thả, thành phần nổi trên vùng vẽ | 1 §9.10–9.13 | V |
| UI-A14 | Thông điệp, toast, tiến trình, lỗi, trạng thái rỗng | 1 §9.14, §12 | V |
| UI-A15 | Lần chạy đầu | 1 §4 | V |
| UI-B01 – B07, B13 | Mở, Lưu thành, Đóng khi chưa lưu, Phục hồi, Tệp bị khóa, Thiếu tham chiếu, Trình phục hồi sau sự cố, Mở tệp lớn | 1 §5 | V (B02, B03, B05, B07: EA) |
| UI-B08 – B12 | Thuộc tính bản vẽ, Dọn dẹp, Báo cáo kiểm tra/phục hồi, Đóng gói gửi đi, Đổi tên | 2 §11 | V (B08, B10) / EA / 1.0 (B11) |
| UI-B14 | Xử lý tệp hàng loạt | 2 §11 | 1.0 |
| UI-C01 / C02 | Palette Thuộc tính / Thuộc tính nhanh | 2 §1 | V / 1.0 |
| UI-C03 – C05 | Quản lý Layer, Trạng thái layer, Bộ lọc layer | 2 §2 | V / 1.0 / 1.0 |
| UI-C06 – C10 | Bộ chọn màu, Linetype, Lineweight, Ô nhập số chuẩn, Thiết lập khớp thuộc tính | 2 §0, §3 | EA (C09: V) |
| UI-C11 | Duyệt layer | 2 §2 | 1.0 |
| UI-D01 – D05 | Kiểu chữ, Kiểu kích thước, Kiểu multileader, Kiểu bảng, Plot style | 2 §4 | EA (D04: 1.0; D05 chọn CTB: V) |
| UI-D06 – D09 | Kiểu điểm, Kiểu đa tuyến song song, Đơn vị, Thiết lập hỗ trợ vẽ | 2 §4 | EA (D07: 1.0) |
| UI-D10 | Tùy chọn | 2 §4 | V (một phần) / EA |
| UI-D11 – D14 | Danh sách tỉ lệ, Quản lý view, Quản lý UCS, Cấu hình viewport | 2 §4 | EA (D11, D14) / 1.0 (D12, D13) |
| UI-E01 / E02 | Tab Hatch / Tab Mảng | 2 §5 | EA |
| UI-E03 – E09 | Đường biên, Xóa trùng lặp, Nhóm, Chọn nhanh, Bộ lọc chọn, Chọn tương tự, Sửa đa tuyến song song | 2 §5 | EA / 1.0 |
| UI-E10 | Prompt và tùy chọn các lệnh lõi | 2 §5 | EA |
| UI-F01 – F04 | Soạn MTEXT, Xếp chồng, Đoạn văn, Cột | 2 §6 | EA / 1.0 |
| UI-F05 | Grip kích thước, dim thông minh | 2 §6 | EA |
| UI-F06 – F08 | Chèn bảng, Tab Ô bảng, Field | 2 §6 | 1.0 |
| UI-F09 | Tìm và thay thế | 2 §6 | V (chỉ tìm) / EA |
| UI-F10 – F12 | Chính tả, Tỉ lệ chú thích của đối tượng, Sửa text tại chỗ | 2 §6 | Sau / 1.0 / EA |
| UI-F13 – F15 | Leader nhanh, Dung sai hình học, Hyperlink | 2 §6 | 1.0 / 1.0 / Sau |
| UI-G01 – G03 | Palette Blocks, Định nghĩa block, Ghi block | 2 §7 | EA |
| UI-G04 / G05 | Block Editor / Palette tạo dynamic block | 2 §7 | 1.0 / Sau |
| UI-G06 – G08 | Định nghĩa attribute, Sửa attribute, Quản lý attribute | 2 §7 | EA / EA / 1.0 |
| UI-G09 | Palette Đếm | 2 §7 | VP |
| UI-G10 – G15 | Tham chiếu ngoài, Gắn DWG/Ảnh/PDF, Nhập PDF, Tab ngữ cảnh | 2 §8 | V (G10) / EA / 1.0 |
| UI-G16 – G18 | Thay block / Tìm chuyển đổi, DesignCenter, Tool Palettes | 2 §7 | 1.0 / Sau |
| UI-G19 | Sửa tại chỗ block và xref | 2 §7 | 1.0 |
| UI-H01 – H03 | Quản lý thiết lập trang, In / Thiết lập trang, Xem trước in | 2 §9 | V |
| UI-H04 / H05 | In nhiều tờ, Tùy chọn PDF | 2 §9 | VP / V |
| **UI-H06** | **In hàng loạt theo khung tên** | 2 §9 | **VP** |
| UI-H07 | Quản lý bộ bản vẽ (VN) | 2 §9 | 1.0 |
| UI-H08 – H10 | Sheet Set Manager, Xuất ảnh, Quản lý máy in | 2 §9 | Sau / V / Sau |
| UI-H11 | Tab ngữ cảnh Layout, tạo viewport | 2 §9 | EA |
| UI-I01 – I03 | Đo nhanh, Kết quả đo, Cửa sổ văn bản | 2 §10 | V |
| UI-I04 / I05 | Tổng chiều dài, diện tích / Bảng diện tích phòng | 2 §10 | VP / 1.0 |
| UI-I06 | Trích xuất dữ liệu | 2 §10 | 1.0 |
| **UI-I07** | **So sánh bản vẽ** | 2 §10 | **VP** |
| UI-I08 / I09 | Làm sạch hình học / Kiểm tra TCVN | 2 §10 | Sau |
| UI-I10 | Máy tính nhanh | 2 §10 | Sau |
| **UI-J01 – J04** | **Bác sĩ Font**: banner, palette, chuyển hàng loạt, thiết lập | 3 §1 | **V** (J03: VP) |
| UI-J05 / J06 | Khung tên / Tạo khung in trong Model | 3 §2 | EA / 1.0 |
| UI-J07 – J10 | Đánh số, Sao chép tăng số, Công cụ text, Ký hiệu kỹ thuật | 3 §3 | 1.0 |
| UI-J11 | Chọn mẫu bản vẽ TCVN | 3 §2 | EA |
| UI-J12 | Excel: dán và xuất | 3 §3 | EA (CSV) / 1.0 |
| UI-J13 | Thư viện TCVN | 3 §2 | EA |
| UI-J14 | Thống kê thép | 3 §3 | Sau |
| UI-J15 | Xuất/nhập attribute với Excel | 3 §3 | VP (xuất) / 1.0 |
| UI-M01 – M04 | Ribbon Viewer, Markup, Chế độ chỉ xem, Layer trong Viewer | 3 §4 | V (M02: VP) |
| UI-K01 – K05 | Nạp ứng dụng, Cảnh báo bảo mật, Tùy chọn bảo mật, Chạy script, Viết tắt lệnh | 3 §5 | EA |
| UI-K06 – K08 | Phím tắt, CUI, Workspace | 3 §5 | 1.0 / Sau / 1.0 |
| UI-K09 / K10 | Bảng điều khiển LISP / Quản lý plugin | 3 §5 | Sau |
| UI-K11 | Hộp thoại DCL | 3 §5 | EA |
| UI-K12 | Chuyển cài đặt từ AutoCAD | 3 §5 | EA |
| UI-K13 | Biến hệ thống | 3 §5 | EA (dòng lệnh) / 1.0 |
| UI-L01 – L03 | Kích hoạt, Kích hoạt offline, Trạng thái giấy phép | 3 §6 | VP |
| UI-L04 / L05 | Trang nâng cấp / Dùng thử | 3 §6 | V / EA |
| UI-L06 – L09 | Cập nhật, Báo crash, Gửi bản vẽ lỗi, Quyền riêng tư | 3 §6 | V |
| UI-L10 / L11 | Thiết lập đồ họa / TRIONBENCH | 3 §6 | V |
| UI-L12 – L14 | Trợ giúp và tìm lệnh, Giới thiệu, Trung tâm thông báo | 3 §6 | V |
| UI-L15 | Cổng quản trị doanh nghiệp, máy chủ giấy phép | 3 §6 | 1.0 |
| UI-L16 | Thanh toán và hóa đơn (web) | 3 §6 | VP |
| UI-L17 | Liên hệ hỗ trợ | 3 §6 | V |
| UI-L18 | Trình cài đặt, tích hợp hệ điều hành | 3 §6 | V |
| UI-N01 – N03 | Công cụ miễn phí: Bác sĩ Font LISP, Thư viện TCVN LISP, cài đặt và đăng ký | 3 §7 | CC |

## Phụ lục B — Các đợt thiết kế đề xuất

| Đợt | Phạm vi | Cần xong trước |
|---|---|---|
| **0. Công cụ miễn phí** | N01–N03: vài hộp thoại DCL (do AutoCAD, ZWCAD, BricsCAD vẽ), trang tải về và đăng ký | Khoảng tháng 1–2 |
| **1. Hệ thống thiết kế và Viewer** | Token màu (phần 1 §13.2), typography, lưới, thành phần cơ bản (nút, ô nhập số UI-C09, dropdown, bảng, palette, banner, toast); khoảng 80 biểu tượng của Viewer; A01–A04, A06–A15; B01, B04, B06, B13; C01 (chỉ đọc); C03 (chế độ xem); D10 (một phần); H01–H03, H05, H09; I01–I03; J01, J02, J04; M01, M03, M04; L04, L06–L14, L17, L18 | Khoảng tháng 8–10 |
| **2. Viewer Pro** | G09, H04, H06, I04, I07, J03, J15 (xuất), M02, L01–L03, L16 | Khoảng tháng 12 |
| **3. Bản biên tập (EA)** | Ribbon đầy đủ (A05), E10 (chip tùy chọn, prompt), C01 (sửa), C03 (đầy đủ), C06–C10, D01–D03, D05–D09, D11, D14, E01–E09, F01, F05, F09, F12, G01–G03, G06, G07, G10–G12, G15, J05, J11–J13, K01–K05, K11, K12, B02, B03, B05, B07–B10, B12, H11 (phần EA), L05 | Khoảng tháng 16–18 |
| **4. v1.0** | C02, C04, C05, D04, D12, D13, F02–F04, F06–F08, F11, G04, G08, G13, G14, G16–G18, H07, I05, I06, J06–J10, K06, K08, B11, L15; B14, C11, F13, F14, G19, H11 (phần còn lại), J15 (nhập), K13, tab Mở rộng (phần P1), bản macOS (phần 1 §13.6) | Khoảng tháng 26–28 |
| **5. Sau v1.0** | Tab Tham số, G05, H08, H10, I08, I09, J14, K07, K09, K10, F10, F15, I10, tab Mở rộng (phần P2); các nhóm ở §8 (đặc tả riêng sau) | — |

## Phụ lục C — Danh sách kiểm khi thiết kế một màn hình

- [ ] Có đủ trạng thái: mặc định, rê chuột, nhấn, vô hiệu, **bị khóa theo phiên bản**, đang tải, rỗng, lỗi.
- [ ] Đúng ở chủ đề **Tối và Sáng**; tỉ lệ màn hình 100% / 150% / 200%; màn hình **1366×768**.
- [ ] Chuỗi tiếng Việt dài không bị cắt (thử với chuỗi dài hơn 40% so với tiếng Anh); có bản English.
- [ ] Dùng được hoàn toàn bằng bàn phím: thứ tự focus, phím tắt, Enter/Esc.
- [ ] Tooltip có tên lệnh tiếng Anh; liên kết F1 tới trợ giúp.
- [ ] Ô nhập số theo UI-C09 (đơn vị, biểu thức, nút lấy từ bản vẽ).
- [ ] Hành động phá hủy có xác nhận, nêu rõ số lượng, hoàn tác được.
- [ ] Thao tác trên 2 giây có tiến trình và nút hủy; không chặn vùng vẽ nếu không cần.
- [ ] Không dùng màu làm tín hiệu duy nhất; đạt tương phản 4,5:1 cho chữ.
- [ ] Biểu tượng tự thiết kế (SVG, 16 và 32 px), không giống biểu tượng của Autodesk.
