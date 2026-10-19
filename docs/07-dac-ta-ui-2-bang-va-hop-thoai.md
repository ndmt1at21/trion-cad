# 07.2 — Đặc tả chức năng cho thiết kế UI: Palette, bảng và hộp thoại

> Phần 2 của bộ đặc tả UI. Ký hiệu phát hành (CC, V, VP, EA, 1.0, Sau), mẫu mô tả và nguyên tắc chung xem ở [phần 1](07-dac-ta-ui-1-khung-ung-dung.md) §0–§2.

## 0. Thành phần dùng chung

### UI-C09 · Ô nhập số chuẩn
Dùng cho mọi trường số trong palette và hộp thoại.
- **Hiển thị:** theo đơn vị bản vẽ (`UNITS`), ví dụ `1250.00` hoặc `4'-1 1/2"`. Góc theo đơn vị góc.
- **Nhận đầu vào:**
  - số thường;
  - có đơn vị (`12.5m`, `250cm`, `3'6"`), tự quy đổi;
  - **biểu thức** (`=1200/2+50`, có ngoặc);
  - dấu thập phân luôn là "." (gõ "," thì gợi ý).
- **Nút lấy từ bản vẽ (⌖):** ẩn hộp thoại → người dùng chỉ điểm, khoảng cách hoặc góc trên vùng vẽ → quay lại với giá trị.
- **Kiểm tra:** viền lỗi kèm câu giải thích (ví dụ "Phải lớn hơn 0"). Mũi tên ↑/↓ để tăng/giảm theo bước.
- **Nhiều đối tượng có giá trị khác nhau:** hiện `*Khác nhau*`. Gõ giá trị mới thì áp dụng cho tất cả.

### Quy ước áp dụng
- **Palette** (Thuộc tính, Layer…) **áp dụng ngay**; mỗi lần sửa là một bước hoàn tác.
- **Hộp thoại modal** áp dụng khi bấm nút xác nhận. Nút xác nhận mang tên hành động ("Tạo block", "In"), không dùng "OK".
- **Danh sách dài** (layer, block, style, font) luôn có ô tìm kiếm và sắp xếp được.
- **Xem trước trực tiếp** trên vùng vẽ khi có thể (hatch, mảng, kiểu chữ, kích thước).

## 1. Thuộc tính đối tượng

### UI-C01 · Palette Thuộc tính
- **Mở từ:** Ctrl+1, `PROPERTIES` (`PR`, `CH`), chuột phải › Thuộc tính, ribbon Trang chủ › Thuộc tính.
- **Dạng:** palette dock phải (mặc định), tự ẩn được, nổi được.
- **Thành phần:**
  1. **Đầu palette:**
     - dropdown loại đối tượng: "Không chọn" / "Tất cả (125)" / "Line (80)" / "Circle (20)"…;
     - nút bật/tắt chế độ cộng dồn vùng chọn (`PICKADD`), nút Chọn đối tượng, nút Chọn nhanh;
     - ô lọc tên thuộc tính (P1).
  2. **Các nhóm thuộc tính thu gọn được**, xem bảng dưới.
  3. **Ô mô tả ở đáy:** tên thuộc tính, giải thích ngắn, mã DXF hoặc biến hệ thống liên quan.
- **Khi không chọn gì:** hiện thiết lập hiện hành.
  - Chung: màu, layer, linetype, tỉ lệ linetype, độ dày nét, độ trong suốt hiện hành.
  - View: tâm, cao, rộng.
  - Khác: tỉ lệ chú thích, biểu tượng UCS, kiểu hiển thị.
- **Hành vi:**
  - Chọn nhiều loại thì chỉ hiện thuộc tính chung; `*Khác nhau*` khi giá trị khác nhau.
  - Trường tính toán (độ dài, diện tích…) chỉ đọc nhưng sao chép được.
  - Quá 25.000 đối tượng: hiện "Nhiều đối tượng — [Vẫn hiển thị]" để giữ hiệu năng.
- **Phát hành:** V (chỉ đọc) · EA (sửa các loại P0) · 1.0 (đủ loại).

**Nhóm "Chung" (mọi đối tượng):** Màu, Layer, Linetype, Tỉ lệ linetype, Plot style, Độ dày nét, Độ trong suốt, Hyperlink (P2), Độ dày đùn (Thickness).

| Loại đối tượng | Thuộc tính riêng (RO = chỉ đọc) |
|---|---|
| **Line** | Điểm đầu X/Y/Z; Điểm cuối X/Y/Z; Delta X/Y/Z (RO); Độ dài (RO); Góc (RO) |
| **Polyline** | Đỉnh hiện tại (◀ ▶ để duyệt, tô sáng đỉnh trên vùng vẽ); Tọa độ X/Y của đỉnh; Bề rộng đầu/cuối đoạn; Bề rộng toàn cục; Cao độ; Đóng (Có/Không); Linetype liên tục qua đỉnh (Bật/Tắt); Diện tích (RO); Độ dài (RO) |
| **Arc** | Tâm X/Y/Z; Bán kính; Góc đầu; Góc cuối; Tổng góc (RO); Độ dài cung (RO); Diện tích (RO) |
| **Circle** | Tâm X/Y/Z; Bán kính; Đường kính; Chu vi; Diện tích (sửa chu vi hoặc diện tích sẽ đổi bán kính) |
| **Ellipse** | Tâm; Bán trục lớn; Bán trục nhỏ; Tỉ lệ bán kính; Góc đầu/cuối; Diện tích (RO) |
| **Spline** | Phương pháp (Điểm đi qua / Điểm điều khiển); Bậc; Đóng; Tuần hoàn; Dung sai; Kiểu tham số hóa nút; Duyệt điểm điều khiển/điểm đi qua kèm tọa độ; Độ dài (RO) |
| **Point** | Vị trí X/Y/Z |
| **Text** | Nội dung; Kiểu chữ; Chú thích theo tỉ lệ; Điểm canh; Chiều cao; Góc xoay; Hệ số rộng; Góc nghiêng; Điểm canh X/Y; Vị trí X/Y; Lộn ngược; Viết ngược; **Bảng mã phát hiện (TCVN3/VNI/Unicode, RO) + nút "Chuyển sang Unicode"** |
| **MText** | Nội dung (nút "…" mở trình soạn); Kiểu chữ; Chú thích theo tỉ lệ; Điểm canh; Hướng; Chiều cao chữ; Góc xoay; Hệ số và kiểu giãn dòng; Che nền; Chiều rộng, chiều cao khung; Cột (kiểu, số, rộng, khoảng cách, cao); Khung chữ; Vị trí; **Bảng mã + nút chuyển** |
| **Kích thước** | **Khác:** Kiểu kích thước, Chú thích theo tỉ lệ. **Đường & mũi tên:** mũi tên 1/2, cỡ mũi tên, màu/linetype/độ dày của đường dim và đường gióng, độ vượt, khoảng cách gốc, đường gióng cố định, bật/tắt từng đường. **Chữ:** màu, màu nền, chiều cao, khoảng cách, vị trí ngang/dọc, kiểu chữ, canh trong/ngoài, góc, hướng nhìn, **giá trị đo (RO)**, **ghi đè chữ**. **Fit:** ép đường dim, tỉ lệ tổng thể, kiểu fit, di chuyển chữ. **Đơn vị chính:** dấu thập phân, tiền tố, hậu tố, làm tròn, hệ số tỉ lệ, định dạng, độ chính xác, bỏ số 0. **Đơn vị phụ** và **Dung sai:** như trong UI-D02 |
| **Multileader** | Kiểu multileader; Tỉ lệ tổng thể; Kiểu đường dẫn (thẳng/spline/không); màu, linetype, độ dày; Mũi tên và cỡ; Đoạn ngang (Có/Không, độ dài); Nội dung chữ và các thuộc tính chữ như MText; Vị trí gắn trái/phải; Khoảng cách chữ; Khung chữ; nếu nội dung là block: Block nguồn, cách gắn, tỉ lệ, góc |
| **Hatch** | Loại (Có sẵn / Người dùng / Tùy biến / Tô đặc / Gradient); Tên mẫu (nút "…" mở gallery); Chú thích theo tỉ lệ; Góc; Tỉ lệ; Khoảng cách và Đan chéo (kiểu người dùng); Bề rộng nét ISO; Gốc X/Y; Liên kết; Kiểu nhận diện island; Màu nền; Độ trong suốt; Gradient (màu 1/2, tên, căn giữa, góc, sắc độ); **Diện tích (RO, cộng dồn khi chọn nhiều hatch)** |
| **Block** | Tên (RO hoặc dropdown để thay block); Vị trí X/Y/Z; Tỉ lệ X/Y/Z; Góc xoay; Đơn vị block (RO); Chú thích theo tỉ lệ. **Tùy biến** (dynamic block): Khoảng cách, Lật, Trạng thái hiển thị (dropdown), Tra bảng (dropdown)… **Attribute:** danh sách tag → giá trị (sửa trực tiếp) |
| **Attribute definition** | Tag; Prompt; Giá trị mặc định; Hằng số; Ẩn; Xác nhận; Đặt sẵn; Khóa vị trí; Nhiều dòng; các thuộc tính chữ |
| **Xref** | Tên; Vị trí; Tỉ lệ; Góc; Đường dẫn đã lưu; Tìm thấy tại (RO); Kiểu (Attach/Overlay); Có clip (RO) |
| **Ảnh raster** | Tên; Đường dẫn đã lưu; Tìm thấy tại; Vị trí; Góc; Rộng; Cao; Tỉ lệ; Độ sáng; Tương phản; Mờ; Hiện ảnh; Hiện phần clip; Nền trong suốt; Độ phân giải và kích thước pixel (RO) |
| **PDF underlay** | Tên; Đường dẫn; Trang; Vị trí; Tỉ lệ; Góc; Tương phản; Mờ; Đơn sắc; Chỉnh màu theo nền; Hiện underlay; Hiện phần clip; Layer PDF (nút "…") |
| **Viewport** | Bật; Có clip; **Khóa hiển thị**; **Tỉ lệ chuẩn** (dropdown); Tỉ lệ tùy chỉnh; Tỉ lệ chú thích; Kiểu hiển thị; Kiểu in shade; Tâm X/Y; Rộng; Cao |
| **Bảng** | Kiểu bảng; Số hàng; Số cột; Hướng; Rộng; Cao; Vị trí. **Khi chọn ô:** kiểu ô, cao hàng, rộng cột, canh lề, nền, màu nội dung, đường viền, kiểu chữ, chiều cao chữ, kiểu dữ liệu và định dạng |
| **Region / Wipeout / Solid 2D** | Diện tích, chu vi (RO); hiện khung (wipeout) |
| **Đa tuyến song song** | Kiểu; Canh lề; Tỉ lệ; Duyệt đỉnh |
| **Nhóm** | Tên; Mô tả; Chọn được |
| **Leader (cũ)** | Kiểu kích thước; Mũi tên và cỡ; Kiểu đường (thẳng / spline); Đoạn ngang; Chú thích gắn kèm (RO) |
| **Xline / Ray** | Điểm gốc X/Y/Z; Điểm thứ hai X/Y/Z; Vector hướng (RO) |
| **Dung sai hình học** | Kiểu kích thước; Chiều cao chữ; Màu chữ; Nội dung khung (nút "…" mở UI-F14) |
| **Đối tượng proxy, đối tượng chưa hỗ trợ** | Tên lớp và ứng dụng tạo ra (RO); ghi chú *"Chỉ xem. Dữ liệu được giữ nguyên khi lưu"* (1.0) |
| **3D solid, surface, mesh** | Chỉ xem: loại, hộp bao (RO). Sửa: Sau v1.0 |

### UI-C02 · Thuộc tính nhanh
- **Mở từ:** tự hiện khi chọn đối tượng (nếu bật trên thanh trạng thái), hoặc nhấp đúp vào đối tượng hình học cơ bản.
- **Dạng:** popup nhỏ cạnh con trỏ, gồm 5–8 trường.
- **Trường mặc định:** Màu, Layer, Linetype, cộng các trường quan trọng nhất của từng loại:
  - Line: Độ dài;
  - Circle: Bán kính;
  - Text: Nội dung, Chiều cao;
  - Block: Tên, Góc xoay;
  - Hatch: Mẫu, Tỉ lệ, Góc;
  - Kích thước: Kiểu, Ghi đè chữ;
  - Polyline: Bề rộng, Đóng, Diện tích (RO).
- **Hành vi:**
  - vị trí theo góc phần tư của con trỏ, hoặc cố định;
  - tự thu gọn khi không dùng;
  - nút "…" mở palette Thuộc tính đầy đủ;
  - chọn trường cho từng loại đối tượng: P2.
- **Phát hành:** 1.0.

## 2. Layer

### UI-C03 · Quản lý Layer (Layer Properties Manager)
- **Mở từ:** `LAYER` (`LA`); ribbon Trang chủ › Layer; menu Định dạng › Layer.
- **Dạng:** palette **không modal**, dock được.

**Thanh công cụ:**
- Layer mới; Layer mới đóng băng ở mọi viewport; Xóa; Đặt hiện hành; Làm mới;
- Trạng thái layer (UI-C04);
- Bộ lọc thuộc tính mới, Bộ lọc nhóm mới, Đảo bộ lọc (UI-C05);
- **Nạp bộ layer chuẩn…** (TCVN, theo bộ môn, hoặc từ tệp DWG/DWT/`.las`); layer trùng tên thì chọn giữ hay ghi đè thuộc tính;
- Thiết lập (xem dưới);
- **ô tìm kiếm** (có wildcard `KT-*`).

**Cây bộ lọc** (bên trái, ẩn được): Tất cả; Layer đang dùng; Viewport có ghi đè; theo Xref (mỗi xref một nút); Bộ lọc của người dùng; Chưa đối chiếu (P2).

**Danh sách layer, các cột:**

| Cột | Ghi chú |
|---|---|
| Trạng thái | Hiện hành, đang dùng hay không, là bộ lọc |
| Tên | Sửa tên trực tiếp (F2) |
| Bật/Tắt | |
| Đóng băng | |
| Khóa | |
| In / Không in | |
| Màu | Mở UI-C06 |
| Linetype | Mở bộ chọn linetype |
| Độ dày nét | Mở bộ chọn lineweight |
| Độ trong suốt | |
| Plot style | Chỉ có với bản vẽ dùng STB |
| Đóng băng ở VP mới | |
| Mô tả | |
| **Số đối tượng** | Cột TrionCAD, tính ở nền |
| *Khi đang ở layout:* Đóng băng VP, Màu VP, Linetype VP, Độ dày VP, Trong suốt VP, Plot style VP | Ghi đè theo viewport, hiện bằng màu nền khác |

**Hành vi:**
- Chọn nhiều layer rồi sửa một ô là áp dụng cho tất cả; sắp xếp theo cột; kéo, ẩn và hiện cột.
- Thanh dưới cùng ghi *"Hiện 48/312 layer · 3 đang chọn"*.
- Không xóa được layer `0`, `Defpoints`, layer hiện hành, layer đang chứa đối tượng hay layer của xref. Bấm Xóa với các layer này thì giải thích lý do và gợi ý **"Gộp vào layer khác…"**.

**Menu chuột phải:**
- Hiện cây bộ lọc; Đặt hiện hành; Layer mới; Đổi tên; Xóa; Sửa mô tả;
- Gỡ ghi đè viewport; Bộ lọc mới;
- Chọn tất cả; Bỏ chọn; Đảo chọn;
- **Chọn đối tượng trên layer này**; **Gộp các layer đã chọn vào…**; **Cô lập các layer đã chọn**;
- Lưu trạng thái layer; Khôi phục trạng thái.

**Thiết lập:**
- Cô lập layer (`LAYISO`): khóa và làm mờ (độ mờ %) hoặc tắt các layer khác; trong layout: đóng băng ở viewport hiện tại hoặc tắt.
- Áp bộ lọc đang chọn lên dropdown layer trên ribbon; màu nền cho các ô ghi đè viewport.
- Báo khi bản vẽ có layer mới (P2, `LAYEREVAL`).

**Phát hành:**
- **V:** chỉ bật/tắt và đóng băng tạm thời. Ghi chú "Thay đổi không lưu vào tệp".
- **EA:** đầy đủ các cột cơ bản, kể cả **Đóng băng VP** và **Đóng băng ở VP mới** (P0); nạp bộ layer chuẩn; thiết lập cô lập.
- **1.0:** bộ lọc, trạng thái layer, ghi đè màu, linetype, độ dày, độ trong suốt và plot style theo viewport.

### UI-C04 · Trạng thái layer (LAYERSTATE)
- **Danh sách:** tên, không gian (Model/Layout), mô tả.
- **Hành động:** Mới, Cập nhật, Sửa, Đổi tên, Xóa, Nhập (.las, .dwg), Xuất (.las), **Khôi phục**.
- **Tùy chọn:**
  - không liệt kê trạng thái của xref;
  - tắt các layer không có trong trạng thái;
  - áp dụng như ghi đè viewport.
- **Thuộc tính được khôi phục** (ô chọn): Bật/Tắt, Đóng băng, Khóa, In, Đóng băng VP, Màu, Linetype, Độ dày, Trong suốt, Plot style, Đóng băng VP mới.
- **Phát hành:** 1.0.

### UI-C05 · Bộ lọc layer
- **Bộ lọc thuộc tính:** tên bộ lọc; lưới điều kiện (mỗi dòng là một điều kiện; các cột giống cột layer; tên hỗ trợ wildcard); xem trước danh sách layer khớp.
- **Bộ lọc nhóm:** kéo layer vào nhóm, hoặc "Chọn layer từ bản vẽ".
- **Phát hành:** 1.0.

### UI-C11 · Duyệt layer (LAYWALK)
- Danh sách layer; chọn một hay nhiều layer thì vùng vẽ **chỉ hiện đối tượng của các layer đó**.
- Ô lọc (wildcard, ví dụ `KC-*`) và các bộ lọc có sẵn; nhấp đúp để giữ một layer luôn hiện.
- Nút **Dọn** (xóa các layer rỗng đang chọn); ô chọn "Khôi phục trạng thái khi đóng" (mặc định bật).
- **Phát hành:** 1.0.

## 3. Bộ chọn dùng chung

### UI-C06 · Bộ chọn màu (COLOR)
- **Tab Màu chỉ số (ACI):**
  - bảng 255 màu (dải chuẩn 1–9, dải xám 250–255, toàn bảng 10–249);
  - nút ByLayer, ByBlock;
  - ô nhập số màu;
  - rê chuột lên ô màu thì hiện số và giá trị RGB.
- **Tab True Color:** mô hình HSL hoặc RGB, thanh và vùng chọn, ô nhập số, mã hex.
- **Tab Color Book:** P2.
- **Chung:** hàng màu dùng gần đây; ô so sánh màu cũ/mới; **công cụ lấy màu từ đối tượng trên bản vẽ** (P1).
- **Phát hành:** EA.

### UI-C07 · Quản lý linetype (LINETYPE)
- **Đầu hộp thoại:** bộ lọc (Tất cả / Đang dùng / Phụ thuộc xref, kèm Đảo bộ lọc), các nút Tải…, Xóa, Hiện hành, Ẩn/Hiện chi tiết.
- **Danh sách:** tên, hình mẫu, mô tả.
- **Phần chi tiết:** tên, mô tả, tỉ lệ toàn cục (`LTSCALE`), tỉ lệ cho đối tượng mới (`CELTSCALE`), bề rộng nét ISO, dùng đơn vị Paper space cho tỉ lệ (`PSLTSCALE`), co linetype theo tỉ lệ chú thích trong Model (`MSLTSCALE`).
- **Hộp thoại Tải linetype:**
  - chọn tệp (`trion.lin`, `trionISO.lin`, **`TCVN.lin`**, hoặc duyệt);
  - danh sách có ô chọn và tìm kiếm.
- **Bộ chọn linetype** dùng chung trong palette là dropdown có hình mẫu và mục "Khác…".
- **Phát hành:** EA.

### UI-C08 · Thiết lập lineweight (LWEIGHT)
- Danh sách độ dày: ByLayer, ByBlock, Mặc định, 0,00–2,11 mm.
- Đơn vị mm hoặc inch; hiển thị lineweight; độ dày mặc định; thanh trượt tỉ lệ hiển thị.
- **Phát hành:** EA.

### UI-C10 · Thiết lập Khớp thuộc tính
- **Mở từ:** trong lệnh `MATCHPROP`, chọn tùy chọn *Thiết lập*.
- **Thuộc tính cơ bản:** Màu, Layer, Linetype, Tỉ lệ linetype, Độ dày nét, Độ trong suốt, Độ dày đùn, Plot style.
- **Thuộc tính đặc biệt:** Kích thước, Chữ, Hatch, Polyline, Viewport, Bảng, Multileader, Đường tâm.
- **Phát hành:** EA.

## 4. Style và thiết lập

### UI-D01 · Kiểu chữ (Text Style)
- **Mở từ:** `STYLE` (`ST`); dropdown kiểu chữ › "Quản lý…".
- **Bên trái:** danh sách kiểu (lọc Tất cả / Đang dùng) và **ô xem trước có câu mẫu tiếng Việt** "Bản vẽ kỹ thuật ÂĂÊÔƠƯĐ 0123".
- **Bên phải, nhóm Font:**
  - Tên font: dropdown có biểu tượng phân biệt SHX và TTF, có tìm kiếm;
  - Kiểu font;
  - Dùng Big Font, chọn Big Font.
- **Nhóm Kích thước:** Chú thích theo tỉ lệ; Khớp hướng với layout; Chiều cao (hoặc Chiều cao trên giấy).
- **Nhóm Hiệu ứng:** Lộn ngược, Viết ngược, Viết dọc (chỉ SHX), Hệ số rộng, Góc nghiêng.
- **Nhóm Bảng mã** (TrionCAD): bảng mã phát hiện được của font (TCVN3/VNI/Unicode/không rõ). Nếu là font mã cũ thì cảnh báo và **gợi ý font Unicode tương đương**.
- **Nút:** Đặt hiện hành, Mới…, Xóa (vô hiệu nếu kiểu đang được dùng), Áp dụng, Đóng.
- **Phát hành:** EA.

### UI-D02 · Quản lý kiểu kích thước
- **Mở từ:** `DIMSTYLE` (`D`).
- **Hộp quản lý:**
  - danh sách kiểu (lọc Tất cả / Đang dùng, không liệt kê kiểu của xref);
  - ô xem trước, mô tả;
  - nút: Đặt hiện hành; Mới… (tên, bắt đầu từ, chú thích theo tỉ lệ, dùng cho: Tất cả / Thẳng / Góc / Bán kính / Đường kính / Tọa độ / Đường dẫn); Sửa…; Ghi đè…; **So sánh** (liệt kê khác biệt giữa hai kiểu); Đóng.
- **Kiểu mẫu theo TCVN có sẵn** (ví dụ `TCVN-1-100`, `TCVN-1-50`).
- **Hộp Sửa kiểu, 7 tab** (mỗi tab đều có xem trước):

| Tab | Trường |
|---|---|
| **Đường** | Đường dim: màu, linetype, độ dày, độ vượt qua đường gióng, khoảng cách cùng gốc, ẩn dim 1/2. Đường gióng: màu, linetype gióng 1/2, độ dày, ẩn gióng 1/2, độ vượt qua đường dim, khoảng cách từ gốc, độ dài cố định |
| **Ký hiệu & mũi tên** | Mũi tên thứ nhất/thứ hai/đường dẫn (gồm **gạch nghiêng kiến trúc**, mũi tên kín, chấm, block người dùng…); cỡ mũi tên; dấu tâm (không / dấu / đường, cỡ); cỡ ngắt dim; ký hiệu độ dài cung (trước / trên / không); góc gãy bán kính; hệ số chiều cao gãy dim thẳng |
| **Chữ** | Kiểu chữ, màu chữ, màu nền, chiều cao, tỉ lệ chiều cao phân số, khung chữ. Vị trí: dọc (giữa / trên / ngoài / JIS / dưới), ngang (giữa / tại gióng 1/2 / trên gióng 1/2), hướng nhìn, khoảng cách tới đường dim. Canh: nằm ngang / theo đường dim / ISO |
| **Fit** | Khi không đủ chỗ: ưu tiên chữ hay mũi tên, luôn giữ chữ giữa các đường gióng, bỏ mũi tên nếu không vừa. Vị trí chữ khi bị dời: cạnh đường dim / trên kèm đường dẫn / trên không đường dẫn. Tỉ lệ: chú thích theo tỉ lệ / theo layout / tỉ lệ tổng thể. Tinh chỉnh: đặt chữ thủ công, luôn vẽ đường dim giữa các đường gióng |
| **Đơn vị chính** | Định dạng; độ chính xác; định dạng phân số; dấu thập phân; làm tròn; tiền tố; hậu tố; hệ số tỉ lệ đo (chỉ áp dụng cho layout); bỏ số 0 đầu/cuối; đơn vị phụ (hệ số, hậu tố); góc: định dạng, độ chính xác, bỏ số 0 |
| **Đơn vị phụ** | Hiện đơn vị phụ; định dạng; độ chính xác; hệ số nhân; làm tròn; tiền tố/hậu tố; bỏ số 0; vị trí (sau hoặc dưới giá trị chính) |
| **Dung sai** | Phương pháp (không / đối xứng / sai lệch / giới hạn / cơ bản); độ chính xác; giá trị trên/dưới; tỉ lệ chiều cao; vị trí dọc; canh; bỏ số 0; dung sai cho đơn vị phụ |

- **Phát hành:** EA (Đường, Ký hiệu, Chữ, Fit, Đơn vị chính) · 1.0 (đủ 7 tab).

### UI-D03 · Kiểu multileader (MLEADERSTYLE)
- **Hộp quản lý:** danh sách, xem trước; nút Hiện hành, Mới, Sửa, Xóa.
- **Hộp Sửa, 3 tab:**
  - **Định dạng đường dẫn:** kiểu (thẳng / spline / không), màu, linetype, độ dày, mũi tên và cỡ, cỡ ngắt.
  - **Cấu trúc:** số điểm tối đa, góc đoạn 1/2, đoạn ngang (tự thêm, độ dài), tỉ lệ (chú thích theo tỉ lệ / theo layout / giá trị).
  - **Nội dung:** loại (MText / Block / Không); với chữ: chữ mặc định, kiểu, góc, màu, cao, luôn canh trái, khung, cách gắn ngang/dọc trái-phải, khoảng cách tới đường ngang, kéo đường dẫn tới chữ; với block: block nguồn (có block TCVN như ký hiệu chi tiết, số hiệu), cách gắn, màu, tỉ lệ.
- **Phát hành:** EA (cơ bản) · 1.0.

### UI-D04 · Kiểu bảng (TABLESTYLE)
- **Thành phần:** bảng khởi đầu; hướng (xuống / lên); kiểu ô (Tiêu đề / Đầu cột / Dữ liệu, thêm kiểu tùy biến). Mỗi kiểu ô có:
  - **Chung:** màu nền, canh lề, định dạng, loại (dữ liệu / nhãn), lề ngang/dọc, gộp ô khi tạo hàng/cột;
  - **Chữ:** kiểu, chiều cao, màu, góc;
  - **Viền:** độ dày, linetype, màu, đường đôi, khoảng cách, các nút chọn cạnh viền.
- **Xem trước** bảng mẫu. Có **kiểu bảng mẫu theo TCVN** (bảng thống kê thép, bảng diện tích).
- **Phát hành:** 1.0.

### UI-D05 · Plot style: quản lý và trình sửa (STYLESMANAGER)
- **Quản lý:** thư mục các tệp CTB/STB, trình hướng dẫn thêm mới, mở để sửa. Có **CTB mẫu theo TCVN** (màu → độ dày nét theo TCVN 8-20).
- **Trình sửa, tab Chung:** mô tả, thông tin tệp, áp hệ số tỉ lệ cho linetype không phải ISO.
- **Trình sửa, tab Bảng** và **tab Biểu mẫu** (hai cách xem cùng dữ liệu):
  - danh sách kiểu (màu 1–255 với CTB; tên kiểu với STB);
  - thuộc tính mỗi kiểu: màu in (theo đối tượng / màu cụ thể), dither, thang xám, số bút, bút ảo, screening (0–100%), linetype, thích ứng, độ dày nét, kiểu đầu nét, kiểu mối nối, kiểu tô, mô tả.
- **Nút:** Thêm/Xóa kiểu (STB); Sửa danh sách độ dày; Lưu thành.
- **Chọn nhiều màu** để sửa một lần (ví dụ màu 1–7 cùng độ dày 0,25).
- **Chuyển đổi** (1.0): tạo STB từ CTB (`CONVERTCTB`); chuyển bản vẽ giữa kiểu in theo màu và theo tên (`CONVERTPSTYLES`), có cảnh báo trước.
- **Phát hành:** V (chọn CTB khi in) · EA (sửa).

### UI-D06 · Kiểu điểm (DDPTYPE)
Lưới 20 kiểu điểm; cỡ điểm; theo % màn hình hoặc theo đơn vị tuyệt đối. **Phát hành:** EA.

### UI-D07 · Kiểu đa tuyến song song (MLSTYLE)
- **Danh sách:** Hiện hành, Mới, Sửa, Đổi tên, Xóa, Tải, Lưu, xem trước.
- **Hộp Sửa:**
  - mô tả;
  - nắp đầu/cuối (đường thẳng, cung ngoài, cung trong, góc);
  - màu tô; hiện mối nối;
  - các thành phần (độ lệch, màu, linetype), Thêm/Xóa.
- **Mẫu có sẵn:** tường 110, 220 (TCVN).
- **Phát hành:** 1.0.

### UI-D08 · Đơn vị bản vẽ (UNITS)
- **Độ dài:** loại (Kiến trúc, Thập phân, Kỹ thuật, Phân số, Khoa học), độ chính xác.
- **Góc:** loại (Độ thập phân, Độ/Phút/Giây, Grad, Radian, Trắc địa), độ chính xác, chiều kim đồng hồ.
- **Tỉ lệ chèn:** đơn vị của nội dung chèn vào (Không đơn vị, mm, cm, m, km, inch, feet…).
- **Khác:** dòng ví dụ kết quả; nút "Hướng…" (góc gốc: Đông / Bắc / Tây / Nam / Khác, chỉ trên bản vẽ được).
- **Hệ đo** cho tệp hatch và linetype mặc định: Mét / Anh (`MEASUREMENT`, 1.0).
- **Phát hành:** EA.

### UI-D09 · Thiết lập hỗ trợ vẽ (DSETTINGS)

| Tab | Trường |
|---|---|
| **Snap & Lưới** | Bật snap; khoảng snap X/Y; bằng nhau; khoảng polar; kiểu snap (lưới chữ nhật / isometric / polar). Bật lưới; kiểu lưới (chấm hay đường, cho Model / Block Editor / Paper); khoảng lưới X/Y; đường chính sau mỗi N ô; lưới thích ứng; cho chia nhỏ; hiện ngoài giới hạn; theo UCS động |
| **Theo dõi polar** | Bật; góc tăng; góc bổ sung (thêm/xóa); theo dõi chỉ trực giao hay theo mọi góc polar; đo góc tuyệt đối hay tương đối so với đoạn trước |
| **Bắt điểm** | Bật bắt điểm; bật theo dõi bắt điểm; ô chọn cho **14 chế độ** (phần 1 §9.6); Chọn tất cả; Bỏ tất cả; Tùy chọn… |
| **Nhập động** | Nhập con trỏ (định dạng cực hay Đề-các, tương đối hay tuyệt đối, khi nào hiện); nhập kích thước (1 hay 2 ô, các ô hiện đồng thời); prompt động (hiện prompt cạnh con trỏ, gợi ý thêm); giao diện tooltip (màu, cỡ, độ trong suốt) |
| **Thuộc tính nhanh** | Hiện khi chọn; áp dụng cho mọi loại hay chỉ loại đã tùy biến; vị trí (theo con trỏ: góc phần tư + khoảng cách / cố định); tự thu gọn; số dòng tối thiểu |
| **Chọn luân phiên** | Cho phép; hiện danh sách; vị trí; hiện thanh tiêu đề |

- **Phát hành:** EA (Snap & Lưới, Polar, Bắt điểm, Nhập động) · 1.0.

### UI-D10 · Tùy chọn (Options)
- **Mở từ:** `OPTIONS` (`OP`); menu ứng dụng; chuột phải trên vùng vẽ.
- **Dạng:** hộp thoại có các tab và **ô tìm kiếm thiết lập** (gõ "autosave" sẽ nhảy tới đúng ô).

| Tab | Nhóm thiết lập chính |
|---|---|
| **Tệp** | Đường dẫn tìm tệp hỗ trợ (sắp xếp, thêm, xóa); thư mục trusted (UI-K03); đường dẫn dự án; tệp tùy biến; font thay thế mặc định và **bảng ánh xạ font** (liên kết Bác sĩ Font); tệp từ điển; đường dẫn máy in, plot style, PC3/PMP; thư mục tự lưu; mẫu mặc định cho `QNEW`; thư mục tool palette; thư mục tạm; nhật ký |
| **Hiển thị** | Chủ đề (Tối/Sáng); Giao diện nhẹ; cỡ biểu tượng ribbon; tooltip (hiện phím tắt, tooltip mở rộng sau N giây, tooltip khi rê lên đối tượng); hiện tab tệp; **Màu…** (theo ngữ cảnh: Model, Layout, Block Editor, dòng lệnh, xem trước; theo thành phần: nền, chữ thập, lưới chính/phụ/trục, đường theo dõi, marker bắt điểm, tooltip); font dòng lệnh; thành phần layout (tab layout, vùng in được, nền giấy, bóng giấy, hiện thiết lập trang khi tạo layout, tạo viewport trong layout mới); độ mịn cung tròn; số đoạn của polyline cong; hiệu năng (chỉ hiện khung ảnh raster, tô đặc, chỉ hiện khung chữ); cỡ chữ thập; độ mờ xref và đối tượng đang sửa tại chỗ |
| **Mở & Lưu** | Định dạng lưu mặc định; lưu thumbnail; an toàn tệp (tự lưu mỗi N phút, tạo bản `.bak`, ghi nhật ký); số tệp gần đây; **mở lại các bản vẽ của phiên trước**; hiện đường dẫn đầy đủ trên tiêu đề; xref (nạp theo nhu cầu, giữ thay đổi layer xref, cho người khác sửa tại chỗ); đối tượng proxy (không hiện / hiện hình / hiện khung bao, có báo không) |
| **In & Xuất** | Máy in mặc định; dùng thiết lập in thành công gần nhất; thư mục in ra tệp; in và xuất bản ở nền; nhật ký in; giữ khổ giấy của layout nếu được; ẩn máy in hệ thống; điểm gốc offset (vùng in được / mép giấy); dấu in; kiểu plot style mặc định (theo màu CTB / theo tên STB) và CTB mặc định |
| **Hệ thống** | **Đồ họa** (mở UI-L10); thiết bị trỏ; khi chuyển layout (regen / cache Model và layout gần nhất / cache tất cả); thông báo đã ẩn (khôi phục các "không hỏi lại"); tiếng bíp khi lỗi; bảo mật (`SECURELOAD`, UI-K03); **cập nhật** (kênh ổn định/beta, tự kiểm tra); **liên kết tệp** `.dwg`/`.dxf`/`.dwt` với TrionCAD (UI-L18); **quyền riêng tư** (UI-L09); **ngôn ngữ giao diện và ngôn ngữ prompt** |
| **Người dùng** | Nhấp đúp để sửa; menu chuột phải (bật/tắt theo ngữ cảnh, chuột phải theo thời gian); tỉ lệ chèn mặc định (đơn vị nguồn/đích); field (hiện nền, khi nào cập nhật); kích thước liên kết với hình học khi tạo (`DIMASSOC`); hoàn tác (gộp zoom/pan, gộp thay đổi layer); Block Editor; lineweight; danh sách tỉ lệ (UI-D11); ưu tiên nhập tọa độ (bắt điểm / bàn phím / bàn phím trừ script) |
| **Vẽ** | AutoSnap (marker, nam châm, tooltip, ô bắt điểm, màu); cỡ marker; tùy chọn bắt điểm (bỏ qua hatch, bỏ qua đường gióng kích thước, thay Z bằng cao độ hiện hành); AutoTrack (hiện vector polar, vector toàn màn hình, tooltip); lấy điểm theo dõi (tự động / giữ Shift); cỡ ô bắt điểm; giao diện tooltip |
| **Mô hình 3D** | (Tối thiểu ở v1.0) hiển thị ViewCube; kiểu hiển thị khi xoay 3D; hiện chữ thập 3D |
| **Chọn** | Cỡ ô chọn; chọn trước rồi gọi lệnh; dùng Shift để thêm; chọn theo nhóm; hatch liên kết; tự tạo khung chọn; nhấn-kéo đối tượng; nhấn-kéo để lasso; kiểu khung (bấm-bấm / nhấn-kéo / tự nhận diện); giới hạn số đối tượng cho palette Thuộc tính; xem trước vùng chọn (khi có lệnh / khi không có lệnh); hiệu ứng tô sáng. **Grip:** cỡ, màu (nguội / rê / nóng / viền), hiện grip, hiện grip trong block, tooltip grip, menu grip động, Ctrl để luân phiên, giới hạn số đối tượng hiện grip |
| **Hồ sơ (Profile)** | Danh sách; Đặt hiện hành; Thêm; Đổi tên; Xóa; **Xuất/Nhập** (.arg, tương thích AutoCAD); Đặt lại mặc định |
| **Tài khoản** | Tài khoản, giấy phép (UI-L03), cài đặt đám mây (Sau) |

- **Phát hành:** V (Hiển thị, Hệ thống, Hồ sơ, Tài khoản) · EA (gần đủ) · 1.0.

### UI-D11 · Danh sách tỉ lệ (SCALELISTEDIT)
- **Nút:** Thêm, Sửa, Lên/Xuống, Xóa, Đặt lại.
- **Danh sách mặc định theo TCVN:** 1:1, 1:2, 1:5, 1:10, 1:20, 1:25, 1:50, 1:75, 1:100, 1:125, 1:200, 1:250, 1:500, 1:1000, 1:2000, 1:5000, 1:10000; phóng to 2:1, 5:1, 10:1.
- **Phát hành:** EA.

### UI-D12 · Quản lý view (VIEW)
- **Cây view:** hiện hành, view Model, view Layout, view chuẩn.
- **Thuộc tính view:** tên, nhóm, UCS, ảnh chụp trạng thái layer, tỉ lệ chú thích, kiểu hiển thị, nền, khung bao.
- **Nút:** Đặt hiện hành, Mới…, Cập nhật layer, Sửa khung bao, Xóa.
- **Phát hành:** 1.0 (V: chỉ Đặt hiện hành).

### UI-D13 · Quản lý UCS (UCSMAN)
- **Tab:** UCS đã đặt tên; UCS trực giao.
- **Tab Thiết lập:** bật biểu tượng, hiện tại gốc, áp dụng cho mọi viewport, cho chọn biểu tượng UCS, lưu UCS theo viewport, chuyển về mặt bằng khi đổi UCS.
- **Phát hành:** 1.0.

### UI-D14 · Cấu hình viewport (VPORTS)
- **Tab Viewport mới:** tên, danh sách cấu hình chuẩn, xem trước, áp dụng cho (màn hình / viewport hiện hành), kiểu hiển thị.
- **Tab Viewport đã đặt tên.**
- **Phát hành:** EA (P0).

## 5. Vẽ và hiệu chỉnh

### UI-E01 · Tab ngữ cảnh Hatch
- **Mở từ:** `HATCH` (`H`); chọn một hatch có sẵn (tab Sửa hatch).
- **Panel Biên:** Chọn điểm (pick point); Chọn đối tượng; Bớt biên; Tạo lại biên; Hiện đối tượng biên; Giữ đường biên (polyline / region); Tập biên (viewport hiện hành / tập chọn).
- **Panel Mẫu:** **gallery** có xem trước khi rê chuột. Phân nhóm: ANSI, ISO, Khác, **TCVN (vật liệu: bê tông, BTCT, gạch, đất, cát, gỗ, kim loại, cách nhiệt…)**, Người dùng, Tùy biến. Có tìm kiếm theo tên.
- **Panel Thuộc tính:**
  - loại (Tô đặc / Gradient / Mẫu / Người dùng);
  - màu hatch; màu nền; độ trong suốt; góc; tỉ lệ;
  - layer ghi đè; tương đối với Paper space;
  - đan chéo (kiểu người dùng); bề rộng nét ISO.
- **Panel Gốc:** Đặt gốc; dưới trái / dưới phải / trên trái / trên phải / tâm; dùng gốc hiện hành; lưu làm mặc định.
- **Panel Tùy chọn:**
  - liên kết; chú thích theo tỉ lệ; khớp thuộc tính (dùng hatch hiện hành / dùng gốc của hatch nguồn);
  - **dung sai khe hở** (vùng có khe nhỏ hơn giá trị này vẫn coi là kín);
  - tạo các hatch riêng; nhận diện island (thường / ngoài cùng / bỏ qua / không);
  - thứ tự hiển thị.
- **Panel Đóng.**
- **Hành vi:**
  - rê chuột vào vùng kín thì **xem trước vùng sẽ tô** (tô sáng biên);
  - vùng không kín: chỉ ra chỗ hở bằng vòng tròn đỏ, kèm gợi ý tăng dung sai khe hở;
  - tùy chọn "Hộp thoại cổ điển" cho người dùng thích dạng hộp thoại (P1).
- **Phát hành:** EA (cơ bản) · 1.0 (gradient, cổ điển).

### UI-E02 · Tab ngữ cảnh Mảng (ARRAYRECT, ARRAYPOLAR, ARRAYPATH)
- **Mảng chữ nhật:** Cột (số, khoảng cách, tổng); Hàng (số, khoảng cách, tổng, cao độ tăng dần); Cấp (Sau); Thuộc tính (liên kết, điểm gốc); Đóng.
- **Mảng tròn:** Phần tử (số, góc giữa, góc phủ); Hàng; Thuộc tính (liên kết, điểm gốc, xoay phần tử, chiều).
- **Mảng theo đường:** Phần tử (số, khoảng cách, tổng); Hàng; Thuộc tính (liên kết, điểm gốc, hướng tiếp tuyến, chia đều hay theo khoảng, canh phần tử).
- **Grip trên mảng:** kéo để đổi khoảng cách hoặc số lượng, cập nhật trực tiếp.
- **Sửa mảng liên kết:** nhấp vào mảng thì tab hiện lại; có Sửa nguồn, Thay phần tử, Đặt lại mảng.
- **Hộp thoại mảng cổ điển** (`ARRAYCLASSIC`, EA) cho người quen kiểu cũ:
  - chọn **Chữ nhật** (số hàng, số cột, khoảng cách hàng/cột, góc mảng) hoặc **Tròn** (tâm, số phần tử, góc phủ, góc giữa các phần tử, xoay phần tử);
  - nút Chọn đối tượng, Xem trước;
  - luôn tạo mảng **không liên kết**.
- **Phát hành:** EA (chữ nhật, tròn, không liên kết, hộp thoại cổ điển) · 1.0 (liên kết, theo đường).

### UI-E03 · Đường biên (BOUNDARY)
Chọn điểm; nhận diện island; loại đối tượng tạo ra (polyline / region); tập biên (viewport hiện hành / chọn mới). Sau khi tạo, hiện **diện tích của đường biên vừa tạo** trên dòng lệnh. **Phát hành:** EA.

### UI-E04 · Xóa trùng lặp (OVERKILL)
- **Thông số:** dung sai; bỏ qua các thuộc tính (màu, layer, linetype, tỉ lệ linetype, độ dày, độ trong suốt, plot style, độ dày đùn, vật liệu).
- **Tùy chọn:**
  - tối ưu đoạn trong polyline;
  - bỏ qua bề rộng polyline;
  - không phá polyline;
  - gộp đoạn thẳng hàng chồng một phần;
  - gộp đoạn thẳng hàng nối tiếp;
  - giữ đối tượng liên kết.
- **Kết quả:** *"Đã xóa 1.240 đối tượng trùng, gộp 85 đoạn"* (hoàn tác được).
- **Phát hành:** 1.0.

### UI-E05 · Quản lý nhóm (CLASSICGROUP)
- **Danh sách nhóm:** tên, chọn được.
- **Tạo nhóm:** tên, mô tả, chọn được, không tên.
- **Sửa nhóm:** bớt/thêm đối tượng, đổi tên, sắp lại thứ tự, mô tả, rã nhóm, bật/tắt chọn được.
- **Phát hành:** 1.0 (EA dùng lệnh `GROUP` trên dòng lệnh).

### UI-E06 · Chọn nhanh (QSELECT)
- **Trường:**
  - áp dụng cho (toàn bản vẽ / vùng chọn hiện tại, kèm nút chọn);
  - loại đối tượng (Nhiều loại / từng loại có trong bản vẽ);
  - thuộc tính;
  - toán tử (= Bằng, ≠ Khác, > Lớn hơn, < Nhỏ hơn, * Khớp wildcard, Chọn tất cả);
  - giá trị (dropdown hoặc ô nhập).
- **Cách áp dụng:** đưa vào tập chọn mới / loại khỏi tập chọn mới; cộng vào tập chọn hiện tại.
- **Truy vấn đã lưu** (P2, TrionCAD): lưu điều kiện thành truy vấn có tên để dùng lại.
- **Phát hành:** EA.

### UI-E07 · Bộ lọc chọn (FILTER)
Danh sách điều kiện: loại đối tượng, thuộc tính, toán tử, nhóm logic AND/OR/XOR/NOT. Lưu và nạp bộ lọc có tên. Áp dụng vào lệnh "Chọn đối tượng" đang chạy. **Phát hành:** 1.0.

### UI-E08 · Thiết lập Chọn tương tự (SELECTSIMILAR › Settings)
Ô chọn: Màu, Layer, Linetype, Tỉ lệ linetype, Độ dày nét, Plot style, Kiểu đối tượng (kiểu chữ, dim, bảng…), Tên (block, xref, ảnh). **Phát hành:** EA.

### UI-E09 · Sửa đa tuyến song song (MLEDIT)
Lưới 12 công cụ: giao chữ thập (kín / hở / hợp nhất), giao chữ T (kín / hở / hợp nhất), nối góc, thêm đỉnh, xóa đỉnh, cắt một, cắt tất cả, hàn tất cả. **Phát hành:** 1.0.

### UI-E10 · Prompt và tùy chọn của các lệnh lõi
Người thiết kế dùng bảng này để thiết kế chip tùy chọn trên dòng lệnh và menu nhập động.
- **Nhãn chip:** tiếng Việt kèm chữ cái tắt tiếng Anh, ví dụ **"Bán kính · R"**.
- **Gõ chữ cái tắt tiếng Anh luôn có tác dụng**, để script và LISP viết cho AutoCAD vẫn chạy.
- Người dùng chọn được ngôn ngữ prompt là tiếng Việt hay English (UI-D10).

| Lệnh | Prompt chính | Tùy chọn (từ khóa) | Xem trước |
|---|---|---|---|
| `LINE` | Điểm đầu → Điểm tiếp theo | Undo (U), Close (C) | Đoạn tạm theo con trỏ |
| `PLINE` | Điểm đầu → Điểm tiếp theo | Arc (A), Halfwidth (H), Length (L), Undo (U), Width (W), Close (C); trong chế độ cung: Angle, CEnter, Direction, Line, Radius, Second pt | Đoạn hoặc cung tạm |
| `CIRCLE` | Tâm | 3P, 2P, Ttr; sau đó Radius / Diameter | Đường tròn tạm |
| `ARC` | Điểm đầu | Center (C), End (E), Angle (A), chord Length (L), Direction (D), Radius (R) | Cung tạm |
| `RECTANG` | Góc thứ nhất → góc đối | Chamfer, Elevation, Fillet, Thickness, Width; Area, Dimensions, Rotation | Hình chữ nhật tạm |
| `POLYGON` | Số cạnh → tâm | Edge; Inscribed / Circumscribed | Đa giác tạm |
| `ELLIPSE` | Đầu mút trục | Arc, Center; Rotation | Ellipse tạm |
| `SPLINE` | Điểm đầu | Method (Fit/CV), Knots, Object; Tangency, toLerance, Undo, Close | Spline tạm |
| `HATCH` | Chọn điểm bên trong | Select objects, Undo, seTtings… (tab ngữ cảnh) | Vùng tô khi rê chuột |
| `MOVE` / `COPY` | Chọn đối tượng → điểm gốc → điểm đích | Displacement; COPY thêm Mode (Single/Multiple), Array | Bóng đối tượng đi theo con trỏ |
| `ROTATE` | Chọn → điểm gốc → góc | Copy, Reference | Đối tượng xoay theo con trỏ |
| `SCALE` | Chọn → điểm gốc → hệ số | Copy, Reference | Đối tượng co giãn theo con trỏ |
| `MIRROR` | Chọn → điểm 1, 2 của trục → xóa nguồn? | Yes / No | Bản đối xứng tạm; chữ không bị lật nếu `MIRRTEXT` = 0 |
| `STRETCH` | Chọn bằng crossing → điểm gốc → điểm đích | Displacement | Phần bị kéo giãn |
| `OFFSET` | Khoảng cách → đối tượng → phía | Through, Erase, Layer; Multiple, Undo, Exit | **Bản offset tạm ở phía con trỏ** |
| `TRIM` / `EXTEND` | Chọn đối tượng cần cắt/kéo | cuTting edges / Boundary edges, Crossing, mOde (Quick/Standard), Project, Edge, eRase, Undo | **Đoạn sẽ bị xóa hiện mờ; đoạn sẽ kéo dài hiện tạm** |
| `FILLET` / `CHAMFER` | Đối tượng 1 → đối tượng 2 | Undo, Polyline, Radius (FILLET) / Distance, Angle, Method (CHAMFER), Trim, Multiple | **Cung bo hoặc vát hiện tạm khi rê lên đối tượng 2** |
| `ARRAY*` | Chọn → (tab ngữ cảnh Mảng) | Base point, COUnt, Spacing, COLumns, Rows, Levels, eXit… | Mảng tạm |
| `BREAK` | Chọn → điểm thứ 2 | First point | — |
| `JOIN` | Chọn nguồn và các đối tượng cần nối | — | — |
| `LENGTHEN` | Chọn đối tượng | DElta, Percent, Total, DYnamic | Độ dài mới tạm |
| `ERASE` | Chọn đối tượng | — | — |
| `EXPLODE` | Chọn đối tượng | — | — |
| `MATCHPROP` | Đối tượng nguồn → đích | Settings (UI-C10) | **Đích đổi thuộc tính tạm khi rê chuột** |
| `DIVIDE` / `MEASURE` | Chọn → số đoạn hoặc độ dài | Block | Điểm chia tạm |
| `TEXT` | Điểm đầu → chiều cao → góc → nhập chữ | Justify, Style | Khung soạn tại chỗ |
| `MTEXT` | Góc thứ nhất → góc đối | Height, Justify, Line spacing, Rotation, Style, Width, Columns | Khung chữ tạm |
| `DIMLINEAR` (và các dim khác) | Gốc gióng 1 → gốc 2 → vị trí đường dim | Mtext, Text, Angle, Horizontal, Vertical, Rotated | **Kích thước tạm** |
| `DIM` (thông minh) | Chọn đối tượng hoặc gốc gióng | Angular, Baseline, Continue, Ordinate, aliGn, Distribute, Layer, Undo | Rê lên đối tượng thì xem trước loại dim phù hợp |
| `MLEADER` | Vị trí mũi tên → vị trí đường ngang → chữ | leader Landing first, Content first, Options | Đường dẫn tạm |
| `INSERT` | (palette Blocks) → điểm chèn | Basepoint, Scale, X/Y/Z, Rotate, Explode, Repeat | Bóng block đi theo con trỏ |
| `BLOCK` | (hộp thoại UI-G02) | — | — |
| `ZOOM` | Góc cửa sổ hoặc tùy chọn | All, Center, Dynamic, Extents, Previous, Scale, Window, Object, Realtime | — |
| `MEASUREGEOM` | Chọn | Distance, Radius, Angle, ARea, Volume, Quick, Mode, eXit | Số đo cạnh con trỏ |
| `AREA` | Điểm góc | Object, Add area, Subtract area | Vùng đang đo tô nhạt; tổng cộng dồn |
| `LIST` / `ID` | Chọn đối tượng / chỉ điểm | — | — |
| `UNDO` | Số bước | Auto, Control, BEgin, End, Mark, Back | — |
| `SELECT` (mọi prompt "Chọn đối tượng") | Chọn đối tượng | Window (W), Last (L), Crossing (C), BOX, ALL, Fence (F), WPolygon (WP), CPolygon (CP), Group (G), Add (A), Remove (R), Multiple (M), Previous (P), Undo (U), AUto, SIngle (SI) | Khung hoặc đường chọn đang vẽ; dòng lệnh đếm "Đã chọn N" |
| `PEDIT` | Chọn polyline (line, arc thì hỏi chuyển thành polyline) | Close/Open, Join, Width, Edit vertex, Fit, Spline, Decurve, Ltype gen, Reverse, Undo; Multiple | Kết quả Fit / Spline / Join hiện tạm |
| `SPLINEDIT` | Chọn spline | Close/Open, Join, Fit data, Edit vertex, convert to Polyline, Reverse, Undo | Spline mới hiện tạm |
| `XLINE` | Điểm → điểm đi qua | Hor, Ver, Ang, Bisect, Offset | Đường tạm |
| `REVCLOUD` | Điểm đầu hoặc chọn đối tượng | Arc length, Object, Rectangular, Polygonal, Freehand, Style, Modify | Đám mây tạm |
| `BREAKATPOINT` | Chọn đối tượng → điểm bẻ | — | Điểm bẻ |
| `DRAWORDER` | Chọn đối tượng | Above objects, Under objects, Front, Back | — |
| `DIMEDIT` | Kiểu sửa → chọn dim | Home, New, Rotate, Oblique | — |
| `DIMTEDIT` | Chọn dim → vị trí chữ | Left, Right, Center, Home, Angle | Chữ đi theo con trỏ |
| `QDIM` | Chọn hình học → vị trí đường dim | Continuous, Staggered, Baseline, Ordinate, Radius, Diameter, datumPoint, Edit, seTtings | Dãy kích thước tạm |
| `QLEADER` | Điểm đầu → các điểm → bề rộng chữ → chữ | Settings (UI-F13) | Đường dẫn tạm |
| `XCLIP` / `CLIP` | Chọn xref, block, ảnh hoặc underlay | New boundary (Select polyline, Polygonal, Rectangular, Invert clip), ON, OFF, Clipdepth, Delete, generate Polyline | Biên clip tạm |
| `MVIEW` | Góc thứ nhất → góc đối | Fit, Shadeplot, Lock, Object, Polygonal, Restore, LAyer, 2, 3, 4 | Khung viewport tạm |
| `LAYOUT` | Tùy chọn | Copy, Delete, New, Template, Rename, SAveas, Set, ? | — |
| `LAYISO` | Chọn đối tượng trên các layer cần cô lập | Settings (Off / Lock and fade kèm độ mờ; trong layout: Vpfreeze / Off) | Layer khác mờ đi ngay |
| `ATTSYNC` / `ATTDISP` | Chọn block / chế độ | ?, Name, Select / Normal, ON, OFF | — |
| `AUDIT` | Sửa các lỗi tìm thấy? | Yes, No | Báo cáo UI-B10 |
| `SETVAR` | Tên biến → giá trị mới | ? (liệt kê; trình duyệt UI-K13) | — |
| `'CAL` | Biểu thức | Hàm điểm: end, mid, cen, int, nea, per, qua, tan; rad, cur | — |
| `CHANGE` / `CHPROP` | Chọn đối tượng → thuộc tính | Color, Elev, LAyer, LType, ltScale, LWeight, Thickness, TRansparency, Annotative; `CHANGE` thêm điểm thay đổi cho line, circle, text | — |
| `BURST` / `XPLODE` | Chọn block | `XPLODE`: All, Color, LAyer, LType, LWeight, Inherit from parent block, Explode; Individually / Globally | — |
| `NCOPY` | Chọn đối tượng lồng → điểm gốc → điểm đích | Settings (Insert hoặc Bind xref chứa đối tượng) | Bóng đối tượng đi theo con trỏ |
| `GATTE` | Chọn block hoặc gõ tên → tag → giá trị mới | Name | Hỏi xác nhận, nêu số block sẽ đổi |
| `TXTEXP` | Chọn chữ | — | — |
| `FLATTEN` | Chọn đối tượng | Bỏ nét khuất: Yes, No | — |
| `EXTRIM` | Chọn đường bao → phía cần cắt | — | Phần sẽ bị cắt hiện mờ |
| `MPEDIT` | Chọn nhiều đối tượng | Như `PEDIT` | — |

Lệnh có tiền tố `-` (ví dụ `-LAYER`, `-INSERT`, `-PURGE`) chạy hoàn toàn trên dòng lệnh, không mở hộp thoại. Prompt của chúng giữ như AutoCAD để script chạy được.

## 6. Chú thích

### UI-F01 · Tab ngữ cảnh Soạn văn bản (MTEXT) và trình soạn tại chỗ
- **Panel Kiểu:** kiểu chữ (gallery); chú thích theo tỉ lệ; chiều cao chữ.
- **Panel Định dạng:**
  - đậm, nghiêng, gạch chân, gạch trên, gạch ngang;
  - font, màu;
  - khớp định dạng; xóa định dạng (ký tự / đoạn / tất cả);
  - góc nghiêng, giãn ký tự, hệ số rộng;
  - chữ hoa / chữ thường (**giữ đúng dấu tiếng Việt**);
  - che nền.
- **Panel Đoạn:**
  - điểm canh (9 vị trí);
  - bullet và đánh số (số, chữ cái, ký hiệu; tự động);
  - giãn dòng;
  - canh đoạn (trái, giữa, phải, đều, phân bố);
  - hộp thoại Đoạn (UI-F03).
- **Panel Chèn:**
  - cột (không / động / tĩnh; UI-F04);
  - **ký hiệu**: độ (°), cộng trừ (±), đường kính (⌀), xấp xỉ (≈), góc (∠), delta (Δ), ohm (Ω), mũ 2/3 (², ³), không bằng (≠), ≤, ≥, ‰, khoảng trắng không ngắt, *m², m³*, "Khác…";
  - field (UI-F08).
- **Panel Chính tả** (P2): kiểm tra tự động, từ điển, thiết lập.
- **Panel Công cụ:** Tìm và thay thế; Nhập văn bản (.txt, .rtf); Tự viết hoa.
- **Panel Tùy chọn:** hoàn tác, làm lại, thước, thiết lập trình soạn (luôn hiển thị WYSIWYG, hiện thanh công cụ, nền đục, màu tô chữ).
- **Panel Đóng.**
- **Trình soạn tại chỗ:**
  - khung chữ có grip chỉnh rộng/cao;
  - thước có thụt lề và tab (kéo để chỉnh);
  - tay nắm cột;
  - **bộ gõ tiếng Việt hoạt động đầy đủ**;
  - dán từ Word giữ định dạng cơ bản (tùy chọn "chỉ dán chữ");
  - Ctrl+Enter hoặc bấm ra ngoài để kết thúc; Esc để hủy (hỏi nếu đã sửa).
- **Phát hành:** EA (Kiểu, Định dạng, Đoạn cơ bản, Ký hiệu, Tìm/thay) · 1.0 (cột, field, nhập văn bản).

### UI-F02 · Thuộc tính xếp chồng (Stack)
Chữ trên/dưới; kiểu (phân số ngang, phân số chéo, dung sai, thập phân); vị trí (trên, giữa, dưới); cỡ (%); thiết lập AutoStack. **Phát hành:** 1.0.

### UI-F03 · Đoạn văn
Tab dừng (trái / giữa / phải / thập phân, vị trí); thụt lề dòng đầu và treo; thụt lề phải; canh đoạn; khoảng cách trước/sau; giãn dòng (chính xác / tối thiểu / bội số). **Phát hành:** 1.0.

### UI-F04 · Thiết lập cột
Kiểu (không cột / động: tự cao hoặc cao thủ công / tĩnh); số cột; chiều cao; rộng cột, khoảng giữa, tổng rộng. **Phát hành:** 1.0.

### UI-F05 · Menu grip của kích thước và dim thông minh
- **Menu grip** (rê lên grip của dim):
  - Kéo giãn;
  - Nối tiếp dim; Dim cùng gốc;
  - Lật mũi tên;
  - Vị trí chữ ▸ (đi cùng đường dim / chỉ di chuyển chữ / di chuyển kèm đường dẫn / trên đường dim, giữa / đặt lại vị trí).
- **Dim thông minh (`DIM`):**
  - rê lên đối tượng thì xem trước loại dim phù hợp (đoạn thẳng → dim thẳng, cung → bán kính, tròn → đường kính, hai đường → góc);
  - chọn lại vị trí trùng với dim cũ thì hỏi: thay thế / giữ cả hai / tách.
- **Phát hành:** EA (grip cơ bản) · 1.0.

### UI-F06 · Chèn bảng (TABLE)
- **Kiểu bảng:** dropdown, kèm nút mở UI-D04.
- **Cách bắt đầu:** bảng trống / từ liên kết dữ liệu (P2) / từ dữ liệu đối tượng (UI-I06).
- **Xem trước.**
- **Cách chèn:** chỉ điểm chèn / chỉ khung.
- **Cột và hàng:** số cột, rộng cột; số hàng dữ liệu, cao hàng (theo số dòng).
- **Kiểu ô cho hàng đầu, hàng hai, các hàng còn lại:** tiêu đề / đầu cột / dữ liệu.
- **Phát hành:** 1.0.

### UI-F07 · Tab ngữ cảnh Ô bảng
- **Hàng:** chèn trên/dưới, xóa.
- **Cột:** chèn trái/phải, xóa.
- **Gộp:** gộp ô (tất cả / theo hàng / theo cột), tách ô.
- **Kiểu ô:** khớp ô, kiểu ô, màu nền, sửa viền.
- **Định dạng ô:** canh lề; khóa (mở / khóa nội dung / khóa định dạng); kiểu dữ liệu (góc, tiền tệ, ngày, số thập phân, chung, phần trăm, điểm, chữ, số nguyên).
- **Chèn:** block, field, **công thức** (Tổng, Trung bình, Đếm, Ô, Biểu thức).
- **Dữ liệu** (P2): liên kết ô, tải lại từ nguồn.
- **Trên lưới bảng:** nhấp chọn ô; kéo để chọn nhiều ô; Tab/Enter để di chuyển; Ctrl+C/V giữa các ô; **dán vùng ô từ Excel**; grip đổi rộng cột và cao hàng.
- **Phát hành:** 1.0.

### UI-F08 · Field, trường dữ liệu (FIELD)
- **Nhóm field:** Tất cả, Ngày giờ, Tài liệu, Liên kết, Đối tượng, Khác, In, Bộ bản vẽ.
- **Danh sách field:** Tác giả, Ghi chú, Ngày tạo, Thuộc tính tùy biến, Ngày, Tên tệp, Dung lượng, Công thức, Từ khóa, Người lưu cuối, Đối tượng có tên, Đối tượng (chọn đối tượng → thuộc tính), Tên thiết lập trang, Khổ giấy, Ngày in, Hướng in, Tỉ lệ in, Plot style, Ngày lưu, Tiêu đề, **Số tờ / Tổng số tờ (bộ bản vẽ VN)**…
- **Thành phần khác:** định dạng (hoa, thường, viết hoa chữ đầu; ngày kiểu Việt Nam dd/MM/yyyy); xem trước; biểu thức field (chỉ đọc).
- **Phát hành:** 1.0.

### UI-F09 · Tìm và thay thế (FIND)
- **Trường:** tìm; thay bằng; phạm vi (toàn bản vẽ / không gian hiện tại / vùng chọn, kèm nút chọn).
- **Tùy chọn:**
  - phân biệt hoa thường; nguyên từ; wildcard;
  - tìm trong xref; tìm trong block; bỏ qua đối tượng ẩn;
  - **phân biệt dấu tiếng Việt** (tắt thì "duong" khớp "đường");
  - **tìm cả chữ mã TCVN3/VNI** (gõ Unicode vẫn tìm ra chữ mã cũ);
  - loại chữ: attribute, chữ của dim/đường dẫn, text/mtext, chữ trong bảng, hyperlink.
- **Kết quả:** danh sách (vị trí, loại đối tượng, đoạn chữ); bấm một dòng để zoom và tô sáng.
- **Nút:** Thay, Thay tất cả, Tìm, Tạo tập chọn (tất cả / các dòng đang chọn).
- **Phát hành:** V (chỉ tìm) · EA.

### UI-F10 · Kiểm tra chính tả (SPELL, P2)
Từ điển tiếng Việt và tiếng Anh; phạm vi kiểm tra; gợi ý; Bỏ qua / Bỏ qua tất cả / Thêm vào từ điển / Sửa / Sửa tất cả; thiết lập (bỏ qua chữ hoa toàn bộ, có số, địa chỉ web…). **Phát hành:** Sau.

### UI-F11 · Tỉ lệ chú thích của đối tượng (OBJECTSCALE)
Danh sách tỉ lệ gắn với đối tượng đang chọn; Thêm/Xóa; lọc (tất cả / chỉ tỉ lệ chung của các đối tượng đã chọn). **Phát hành:** 1.0.

### UI-F12 · Sửa text một dòng tại chỗ (TEXTEDIT)
Khung soạn ngay tại vị trí chữ, có con trỏ nhập. Enter tạo dòng mới (đối tượng TEXT mới). Bấm ra ngoài hoặc Enter ở dòng trống để kết thúc. Bộ gõ tiếng Việt hoạt động. **Phát hành:** EA.

### UI-F13 · Thiết lập leader nhanh (QLEADER)
Nhiều người dùng Việt Nam quen gọi lệnh này bằng `LE`. Hộp thoại mở bằng tùy chọn *Settings* trong lệnh, có 3 tab:
- **Chú thích:** loại (MText / Sao chép một đối tượng / Dung sai / Block / Không); với MText: hỏi bề rộng, luôn canh trái, có khung; dùng lại chú thích (không / cho lần sau / chú thích hiện tại).
- **Đường dẫn và mũi tên:** thẳng / spline; mũi tên (cùng danh sách với kiểu kích thước); số điểm (không giới hạn hoặc tối đa N); ràng buộc góc đoạn thứ nhất và thứ hai (tự do, ngang, 90°, 45°, 30°, 15°).
- **Gắn chữ:** vị trí gắn khi chữ ở bên trái và bên phải (đỉnh dòng đầu, giữa dòng đầu, giữa khối chữ, giữa dòng cuối, đáy dòng cuối); gạch chân dòng cuối.
- **Phát hành:** 1.0.

### UI-F14 · Khung dung sai hình học (TOLERANCE)
- **Lưới nhập 2 hàng.** Mỗi hàng gồm: ký hiệu đặc tính; dung sai 1 và dung sai 2 (ô đường kính ⌀, giá trị, điều kiện vật liệu M / L / S); chuẩn 1–3 (chữ cái, điều kiện vật liệu).
- **Bảng 14 ký hiệu đặc tính** theo ISO 1101: độ thẳng, độ phẳng, độ tròn, độ trụ, profile đường, profile mặt, độ nghiêng, độ vuông góc, độ song song, vị trí, đồng tâm hoặc đồng trục, đối xứng, độ đảo hướng kính, độ đảo toàn phần. Ký hiệu tự vẽ.
- **Khác:** chiều cao và ký hiệu vùng dung sai chiếu; ký hiệu chuẩn (datum identifier).
- Xem trước khung; chỉ điểm để chèn; nhấp đúp để sửa lại.
- **Phát hành:** 1.0.

### UI-F15 · Hyperlink (HYPERLINK, P2)
- **Đích:** tệp hoặc trang web (đường dẫn, chữ hiển thị), hoặc một view hay layout trong bản vẽ.
- Rê lên đối tượng có liên kết thì con trỏ hiện huy hiệu liên kết kèm tooltip; **Ctrl + bấm** để mở.
- Xuất PDF giữ liên kết (UI-H05).
- **Phát hành:** Sau.

## 7. Block và attribute

### UI-G01 · Palette Blocks
- **Mở từ:** `INSERT` / `BLOCKSPALETTE`; ribbon gallery Chèn › "Mở thư viện…".
- **Tab:**
  - **Bản vẽ hiện hành;**
  - **Gần đây;**
  - **Yêu thích;**
  - **Thư viện**: thư mục hoặc tệp block, kèm liên kết Thư viện TCVN (UI-J13).
- **Đầu palette:** ô tìm kiếm, bộ lọc, kiểu xem (biểu tượng lớn, nhỏ, danh sách), cỡ xem trước.
- **Tùy chọn chèn:**
  - điểm chèn (trên màn hình / nhập tọa độ);
  - tỉ lệ (đều / X-Y-Z, trên màn hình);
  - góc (trên màn hình / nhập góc);
  - **lặp chèn liên tục**;
  - phá khối khi chèn;
  - trạng thái hiển thị của dynamic block (P2).
- **Hành vi:** bấm một block rồi chỉ điểm để chèn, hoặc kéo-thả vào vùng vẽ.
- **Chuột phải trên block:** Chèn; Block Editor; Định nghĩa lại từ tệp; Thêm vào Yêu thích; Mở tệp nguồn.
- **Trạng thái rỗng:** "Bản vẽ chưa có block — [Mở Thư viện TCVN]".
- **Phát hành:** EA (Bản vẽ hiện hành, Thư viện) · 1.0.

### UI-G02 · Định nghĩa block (BLOCK)
- **Tên:** combo (chọn tên có sẵn để định nghĩa lại).
- **Điểm gốc:** chỉ trên bản vẽ, hoặc nhập X/Y/Z.
- **Đối tượng:** chọn, chọn nhanh; sau khi tạo thì giữ nguyên / chuyển thành block / xóa; số đối tượng đã chọn.
- **Hành vi block:** chú thích theo tỉ lệ (cùng tùy chọn khớp hướng layout); co giãn đều; cho phép phá khối.
- **Thiết lập:** đơn vị block; hyperlink (P2).
- **Khác:** mô tả; mở trong Block Editor sau khi tạo.
- **Kiểm tra:**
  - tên hợp lệ;
  - trùng tên có sẵn thì cảnh báo "Định nghĩa lại block X sẽ cập nhật N tham chiếu".
- **Phát hành:** EA.

### UI-G03 · Ghi block ra tệp (WBLOCK)
Nguồn (block / cả bản vẽ / đối tượng); điểm gốc; đối tượng; đích (tên tệp, đường dẫn, đơn vị chèn); phiên bản DWG. **Phát hành:** EA.

### UI-G04 · Block Editor: tab ngữ cảnh và môi trường (BEDIT)
- **Vào Block Editor:** hộp chọn block, có danh sách và xem trước.
- **Môi trường:** nền vùng vẽ màu khác, UCS đặt tại gốc block, thanh thông báo "Đang sửa block *Cua-di-900*".
- **Panel Mở/Lưu:** Sửa block khác, Lưu block, Lưu thành.
- **Panel Quản lý:** xóa, đối tượng dựng hình, hiển thị ràng buộc, quản lý tham số, palette tạo block (UI-G05), định nghĩa attribute.
- **Panel Tham số & Action** (P2): point, linear, polar, XY, rotation, alignment, flip, visibility, lookup, base point. Action: move, scale, stretch, polar stretch, array, rotate, flip, lookup, bảng thuộc tính block.
- **Panel Visibility** (P2): chế độ hiển thị; hiện/ẩn đối tượng theo trạng thái; dropdown trạng thái; quản lý trạng thái.
- **Panel Thử** (P2): cửa sổ thử block.
- **Panel Đóng:** đóng Block Editor; hỏi lưu nếu có thay đổi.
- **Phát hành:** 1.0 (sửa block tĩnh) · Sau (tạo dynamic block).

### UI-G05 · Palette tạo dynamic block (BAUTHORPALETTE, P2)
Tab Parameters, Actions, Parameter Sets, Constraints. **Phát hành:** Sau.

### UI-G06 · Định nghĩa attribute (ATTDEF)
- **Chế độ:** ẩn, hằng số, xác nhận, đặt sẵn, khóa vị trí, nhiều dòng.
- **Attribute:** Tag, Prompt, Mặc định (nút chèn field).
- **Điểm chèn:** trên màn hình hoặc X/Y/Z.
- **Thiết lập chữ:** canh lề, kiểu chữ, chú thích theo tỉ lệ, chiều cao, góc, bề rộng khung (nhiều dòng).
- Ô "Canh dưới attribute trước".
- **Phát hành:** EA.

### UI-G07 · Trình sửa attribute (EATTEDIT)
- **Đầu hộp thoại:** tên block, tag, nút Chọn block.
- **Tab Attribute:** danh sách tag / prompt / giá trị; ô sửa giá trị (mở trình soạn nhiều dòng nếu là attribute nhiều dòng).
- **Tab Tùy chọn chữ:** kiểu, canh, cao, góc, viết ngược, lộn ngược, hệ số rộng, nghiêng, chú thích theo tỉ lệ, rộng khung.
- **Tab Thuộc tính:** layer, linetype, màu, plot style, độ dày.
- **Nút:** Áp dụng, Lưu, Hủy. Có nút "Block kế tiếp" để duyệt các block cùng tên (P2).
- **Phát hành:** EA.

### UI-G08 · Quản lý attribute block (BATTMAN)
- **Đầu hộp thoại:** dropdown block (kèm nút chọn); số tham chiếu.
- **Danh sách attribute:** tag, prompt, mặc định, chế độ.
- **Nút:** Lên/Xuống, Sửa (các tab như UI-G07), Gỡ, **Đồng bộ**.
- **Thiết lập:** cột hiển thị; tô nổi tag trùng; áp thay đổi cho tham chiếu có sẵn.
- **Phát hành:** 1.0.

### UI-G09 · Palette Đếm (COUNT)
- **Phạm vi:** Model / layout / toàn bộ.
- **Danh sách block:** tên, số lượng, ảnh xem trước. Có tìm kiếm và sắp xếp.
- **Bấm một dòng:** chuyển vùng vẽ sang "chế độ đếm" (tô sáng các block đó, mũi tên ◀ ▶ duyệt từng cái).
- **Cảnh báo:** block chồng nhau, block đã bị phá thành nét rời (P2).
- **Nút:** **Tạo bảng đếm**, chèn field đếm, xuất Excel, làm mới.
- **Phát hành:** VP (đếm, duyệt, xuất Excel) · EA (tạo bảng đếm, chèn field: cần quyền sửa bản vẽ).

### UI-G16 · Thay block và Tìm & chuyển đổi (BLOCKREPLACE, TCBLOCKMATCH)
- **Thay block:**
  - chọn block cần thay, hoặc mọi block cùng tên;
  - chọn block thay thế (từ bản vẽ hoặc thư viện);
  - tùy chọn: giữ tỉ lệ và góc, ánh xạ attribute theo tag, dọn định nghĩa block cũ.
- **Tìm & chuyển đổi** (P2, kiểu Smart Match):
  - chọn mẫu hình học, hệ thống tìm các cụm tương tự (dung sai; bất biến khi xoay, khi đổi tỉ lệ);
  - danh sách kết quả, số lượng, tô sáng;
  - hành động: chuyển thành block (đặt tên), đếm, chọn.
- **Phát hành:** 1.0 (Thay) · Sau (Tìm & chuyển đổi).

### UI-G17 · DesignCenter (ADCENTER)
- **Bên trái:** cây (thư mục, bản vẽ đang mở, lịch sử).
- **Vùng nội dung:** nội dung bên trong một bản vẽ (Block, Kiểu kích thước, Layer, Layout, Linetype, Kiểu multileader, Kiểu bảng, Kiểu chữ, Xref).
- **Khác:** xem trước, mô tả; kéo-thả sang bản vẽ hiện hành; tìm kiếm (theo tên, loại, ngày); yêu thích.
- **Phát hành:** 1.0.

### UI-G18 · Tool Palettes (TOOLPALETTES)
- **Cửa sổ:** tab dọc; mỗi công cụ là một block, hatch, lệnh hoặc macro LISP.
- **Thuộc tính công cụ:** layer, màu, tỉ lệ, góc, hỏi góc khi chèn, phá khối…
- **Thao tác:** kéo đối tượng từ bản vẽ vào để tạo công cụ; nhóm palette; tự ẩn; trong suốt; dock hoặc nổi.
- **Nhập/xuất:** tương thích `.xtp` (P2).
- **Phát hành:** 1.0.

### UI-G19 · Sửa tại chỗ block và xref (REFEDIT)
- **Mở từ:** `REFEDIT`; nhấp đúp vào xref (phần 1 §9.10); chuột phải › Sửa block tại chỗ.
- **Bước chọn:** cây các tham chiếu lồng nhau tại điểm chọn, có xem trước; tự chọn mọi đối tượng lồng hoặc chọn từng đối tượng.
- **Thiết lập:** tạo tên duy nhất cho layer và kiểu bị trùng; hiện attribute definition để sửa; khóa đối tượng không thuộc tập làm việc.
- **Trong khi sửa:**
  - đối tượng ngoài tập làm việc mờ đi (độ mờ chỉnh ở Tùy chọn › Hiển thị);
  - banner: *"Đang sửa tại chỗ xref KT-01 — thay đổi sẽ ghi vào KT-01.dwg"*;
  - tab ngữ cảnh *Sửa tham chiếu*: Thêm vào tập làm việc, Bớt khỏi tập làm việc, **Lưu thay đổi**, **Hủy thay đổi**.
- **Kiểm tra:** xref đang được người khác mở thì không cho lưu (UI-B05); lưu thay đổi của block sẽ cập nhật mọi tham chiếu, có xác nhận kèm số lượng.
- **Phát hành:** 1.0.

## 8. Tham chiếu ngoài và underlay

### UI-G10 · Palette Tham chiếu ngoài (EXTERNALREFERENCES)
- **Thanh công cụ:**
  - Gắn ▾ (DWG / Ảnh / PDF / DGN (Sau));
  - Làm mới;
  - Đổi đường dẫn ▾ (tuyệt đối / tương đối / bỏ đường dẫn / chọn đường dẫn mới / **tìm và thay đường dẫn** cho nhiều tham chiếu).
- **Danh sách tham chiếu** (xem dạng danh sách hoặc cây lồng nhau). Các cột:
  - Tên;
  - **Trạng thái**: Đã nạp, Chưa nạp, **Không tìm thấy**, Chưa phân giải, Mồ côi, **Cần nạp lại**;
  - Kích thước; Kiểu (Attach/Overlay); Ngày; Đường dẫn đã lưu; Tìm thấy tại.
- **Khung chi tiết hoặc xem trước:** sửa được đường dẫn.
- **Chuột phải:** Mở, Gắn, Gỡ nạp, Nạp lại, Gỡ bỏ, Bind ▸ (Bind / Insert / **Từng đối tượng phụ thuộc**: chọn block, kiểu kích thước, layer, linetype, kiểu chữ của xref để đưa vào bản vẽ, `XBIND`), Đường dẫn ▸, So sánh với bản trước (`XCOMPARE`, P2).
- **Thông báo:** xref thay đổi thì thanh trạng thái hiện *"Xref KT-01 đã thay đổi — [Nạp lại]"*.
- **Phát hành:** V (xem trạng thái, nạp lại) · EA.

### UI-G11 · Gắn DWG làm xref (XATTACH)
Tên (duyệt…), xem trước; kiểu (Attach / Overlay); đường dẫn (đầy đủ / tương đối / không); điểm chèn, tỉ lệ (đều), góc (trên màn hình hoặc nhập); đơn vị block (RO); chi tiết (tìm thấy tại, đường dẫn đã lưu). **Phát hành:** EA.

### UI-G12 · Gắn ảnh (IMAGEATTACH)
Tên, xem trước; đường dẫn; điểm chèn, tỉ lệ, góc; chi tiết (độ phân giải, đơn vị, kích thước pixel và theo đơn vị bản vẽ). Sau khi gắn thì hiện tab ngữ cảnh Ảnh. **Phát hành:** EA.

### UI-G13 · Gắn PDF (PDFATTACH)
Tệp; **danh sách trang có thumbnail** (chọn nhiều trang); đường dẫn; điểm chèn, tỉ lệ, góc. Liên kết "Muốn chuyển thành đối tượng sửa được? → Nhập PDF". **Phát hành:** 1.0.

### UI-G14 · Nhập PDF thành đối tượng (PDFIMPORT)
- **Nguồn:** tệp PDF, hoặc PDF underlay đang chọn; trang; vị trí (chỉ trên màn hình / gốc), tỉ lệ, góc.
- **Dữ liệu nhập** (ô chọn): hình học vector; vùng tô đặc; chữ TrueType; ảnh raster.
- **Layer:** dùng layer của PDF / tạo layer theo loại đối tượng / layer hiện hành.
- **Tùy chọn:**
  - nhập thành block;
  - nối đoạn thẳng và cung;
  - chuyển vùng tô đặc thành hatch;
  - áp độ dày nét;
  - suy ra linetype từ các nét đứt thẳng hàng.
- **Nhận dạng chữ SHX:** chọn font SHX để so khớp, ngưỡng tin cậy. Có tùy chọn **gộp chữ thành đoạn** (TXT2MTXT).
- **Báo cáo:** số đối tượng tạo ra, số chữ nhận dạng được.
- **Phát hành:** 1.0.

### UI-G15 · Tab ngữ cảnh Ảnh, PDF underlay, Tham chiếu ngoài
- **Ảnh:** Điều chỉnh (độ sáng, tương phản, mờ); Clip (tạo / bỏ); Tùy chọn (hiện ảnh, nền trong suốt); mở palette Tham chiếu.
- **PDF underlay:** Điều chỉnh (tương phản, mờ, đơn sắc); Clip; Tùy chọn (hiện, bắt điểm, chỉnh màu theo nền); **Layer PDF**; Nhập thành đối tượng.
- **Xref:** Sửa tại chỗ; Mở xref; Clip; Tùy chọn.
- **Phát hành:** EA (ảnh, xref) · 1.0 (PDF).

## 9. Layout, in và xuất

### UI-H01 · Quản lý thiết lập trang (PAGESETUP)
- **Danh sách:** layout hoặc Model hiện hành, các thiết lập trang có tên.
- **Chi tiết:** thiết bị, khổ giấy, nơi in, mô tả.
- **Nút:** Đặt hiện hành; Mới (tên, bắt đầu từ); Sửa (mở UI-H02 ở chế độ thiết lập trang); Nhập (từ DWG/DWT/DXF); Đóng.
- Ô chọn "Hiện khi tạo layout mới".
- **Phát hành:** V (xem, đặt hiện hành để in) · EA.

### UI-H02 · Hộp thoại In và Thiết lập trang
- **Mở từ:** `PLOT` (Ctrl+P); `PAGESETUP`.
- **Dạng:** modal, có nút "Thêm tùy chọn ▸" để mở rộng.

| Nhóm | Trường |
|---|---|
| Thiết lập trang | Tên (dropdown) / Thêm… |
| Máy in | Tên (máy in hệ thống, **PDF của TrionCAD**, PNG); Thuộc tính…; in ra tệp; thông tin máy in |
| Khổ giấy | Dropdown (A0–A4 ISO, khổ dài, khổ của máy in); **xem trước nhỏ** có vùng in được |
| Số bản | 1–999 |
| Vùng in | Layout / Toàn bộ đối tượng / Màn hình hiện tại / Giới hạn bản vẽ / View đã đặt tên / **Cửa sổ** (nút chọn cửa sổ) |
| Offset | Canh giữa; X/Y (mm) |
| Tỉ lệ | Vừa khổ giấy; tỉ lệ chuẩn (dropdown) hoặc tùy chỉnh (mm = đơn vị); co giãn độ dày nét |
| *Mở rộng:* Plot style | Dropdown CTB/STB (kể cả CTB mẫu TCVN); nút Sửa (UI-D05); hiển thị plot style trên layout |
| *Mở rộng:* Viewport có shade | In shade (như hiển thị / wireframe / ẩn nét…); chất lượng; DPI |
| *Mở rộng:* Tùy chọn in | In ở nền; in độ dày nét; in độ trong suốt; in theo plot style; in Paper space sau cùng; ẩn đối tượng Paper space; bật dấu in (thiết lập); lưu thay đổi vào layout |
| *Mở rộng:* Hướng giấy | Dọc / Ngang / In ngược |
| Đáy hộp thoại | **Xem trước…** (UI-H03); Áp dụng cho layout; **In**; Hủy |

- **Dấu in** (P2, nút thiết lập cạnh ô "bật dấu in"): chọn trường (tên bản vẽ, layout, ngày giờ, người in, máy in, khổ giấy, tỉ lệ) và trường tự đặt; vị trí (4 góc, ngang hoặc dọc), offset; font và cỡ chữ; lưu thành tệp thiết lập.
- **Kiểm tra trước khi in** (TrionCAD, P1): cảnh báo không chặn, ví dụ *"3 font bị thay thế"*, *"Có đối tượng nằm ngoài vùng in"*, *"Plot style không tìm thấy"*, *"Nét mảnh hơn 0,1 mm"*.
- **Phát hành:** V (in theo thiết lập có sẵn, chọn máy in, khổ, vùng in, tỉ lệ, CTB) · EA.

### UI-H03 · Xem trước bản in (PREVIEW)
Toàn màn hình. Công cụ: In, Pan, Zoom, Zoom cửa sổ, Zoom về ban đầu, Đóng. Hiện tờ giấy với vùng in được, **độ dày nét theo CTB**, thông tin trang. **Phát hành:** V.

### UI-H04 · In nhiều layout, nhiều bản vẽ (PUBLISH)
- **Danh sách tờ:**
  - thêm tờ (từ bản vẽ đang mở, từ tệp; chỉ Model, chỉ layout, hoặc cả hai);
  - bỏ tờ; lên/xuống;
  - lưu/nạp danh sách (`.dsd`).
- **Cột của mỗi tờ:** tên tờ, thiết lập trang (đổi riêng từng tờ), trạng thái.
- **Đích:** máy in theo thiết lập trang / PDF / PNG.
- **Tùy chọn PDF:**
  - một tệp nhiều trang hoặc mỗi tờ một tệp;
  - **mẫu đặt tên tệp** (theo tên bản vẽ, layout, attribute khung tên);
  - thư mục đích;
  - số bản.
- **Khác:** in dấu in; in ở nền; mở thư mục khi xong; tiến trình theo từng tờ; nhật ký lỗi.
- **Phát hành:** VP · EA.

### UI-H05 · Tùy chọn xuất PDF (EXPORTPDF)
- **Preset:** Chất lượng cao, Dung lượng nhỏ, Xem trên web/điện thoại, Tùy chỉnh.
- **Độ phân giải:** vector (DPI), gradient, màu/xám, đen trắng.
- **Dữ liệu kèm theo:**
  - layer của PDF;
  - hyperlink;
  - bookmark theo tờ;
  - **chữ SHX tìm được** (lưu dạng chữ ẩn để tìm và copy);
  - nhúng font TrueType hoặc chuyển thành nét.
- **Khác:** gộp các nét chồng nhau; mở PDF khi xong.
- **Phát hành:** V.

### UI-H06 · In hàng loạt theo khung tên (TCBATCHPLOT; TrionCAD, VN)
Quy trình người Việt dùng rất nhiều: **nhiều khung tên đặt sẵn trong Model**, in mỗi khung ra một tờ.

- **Mở từ:** tab Công cụ VN › In hàng loạt; tab Xuất.
- **Dạng:** palette lớn hoặc hộp thoại không modal, chia 4 bước có thể quay lại.

1. **Nhận diện khung**, theo một trong các cách:
   - theo **tên block** khung tên (chọn nhiều tên; có danh sách đề xuất tự quét);
   - theo **layer** của hình chữ nhật viền khung;
   - theo đối tượng mẫu (chọn một khung, hệ thống tìm các khung tương tự).

   Phạm vi: Model, các layout, nhiều tệp (P1).
2. **Danh sách khung tìm được**, dạng lưới thumbnail hoặc bảng:
   - ô chọn in/không in;
   - thứ tự in (trái → phải, trên → dưới; hoặc theo attribute số hiệu bản vẽ);
   - **khổ giấy tự suy ra** từ kích thước khung và tỉ lệ (sửa được);
   - hướng giấy;
   - tỉ lệ (tự suy ra hoặc vừa khổ);
   - tên tờ lấy từ attribute (ví dụ `KT-01 Mặt bằng tầng 1`).
   
   Bấm một dòng thì vùng vẽ zoom đến khung đó.
3. **Thiết lập in:**
   - máy in hoặc PDF (một tệp nhiều trang / mỗi khung một tệp);
   - CTB;
   - **mẫu tên tệp** từ attribute (`{SoHieu}_{TenBanVe}.pdf`);
   - thư mục đích;
   - offset hoặc canh giữa;
   - in độ dày nét.
4. **Chạy:** chạy ở nền; tiến trình theo từng khung; tạm dừng/hủy; báo cáo (thành công, lỗi, cảnh báo font).

- **Lưu cấu hình** thành "Mẫu in" để dùng lại cho các bộ bản vẽ sau.
- **Phát hành:** VP (một tệp) · 1.0 (nhiều tệp).

### UI-H07 · Quản lý bộ bản vẽ (TCSHEETS; TrionCAD, VN)
Phiên bản gọn hơn Sheet Set Manager, theo cách làm hồ sơ thiết kế ở Việt Nam.

- **Dạng:** palette dock.
- **Thông tin dự án:** tên công trình, hạng mục, chủ đầu tư, địa điểm, đơn vị thiết kế, logo, giai đoạn (thiết kế cơ sở / kỹ thuật / bản vẽ thi công), người chủ trì / chủ nhiệm / kiểm tra.
- **Danh sách tờ:** từ các layout, hoặc các khung trong Model, của nhiều tệp.
  - Cột: **số hiệu** (tự đánh theo bộ môn KT, KC, ĐN, CN…, kiểu "KT 01" hoặc "KT 1/18"), tên bản vẽ, tỉ lệ, khổ giấy, ngày, trạng thái (đã in / sửa sau khi in).
  - Kéo để sắp xếp.
- **Ánh xạ khung tên:** attribute của block khung tên ↔ thông tin dự án và thông tin tờ. Có nút "**Cập nhật tất cả khung tên**".
- **Hành động:** in cả bộ (UI-H06/H04); đóng gói gửi đi (UI-B11); **xuất danh mục bản vẽ** thành bảng trong bản vẽ hoặc Excel.
- **Phát hành:** 1.0.

### UI-H08 · Sheet Set Manager (SHEETSET, P2)
Tab Danh sách tờ, View của tờ, View Model. Thuộc tính bộ (tùy biến); tờ mới từ mẫu; callout; publish; lưu trữ; đọc/ghi `.dst`. **Phát hành:** Sau.

### UI-H09 · Xuất ảnh (PNGOUT, JPGOUT, TIFOUT, BMPOUT)
Định dạng (PNG, JPG, TIF, BMP); độ phân giải (DPI hoặc kích thước pixel); vùng (màn hình / toàn bộ / cửa sổ / layout); nền (trong suốt với PNG / trắng / màu nền vùng vẽ); độ dày nét; khử răng cưa. **Phát hành:** V.

### UI-H10 · Quản lý máy in, trình sửa PC3 (PLOTTERMANAGER, P2)
Danh sách cấu hình; thiết lập cổng, khổ giấy tùy chỉnh (vùng in được), hiệu chỉnh tỉ lệ (PMP). **Phát hành:** Sau. Trước đó chỉ **đọc** khổ giấy từ PC3 có sẵn (1.0).

### UI-H11 · Tab ngữ cảnh Layout và tạo viewport (MVIEW)
- **Hiện khi:** đang ở một tab layout. Viewer không có tab này.
- **Panel Layout:** Layout mới; Từ mẫu…; Thiết lập trang (UI-H01).
- **Panel Viewport:**
  - tạo viewport: **Chữ nhật**; Đa giác; Theo đối tượng (polyline kín, đường tròn, ellipse, region, spline); Theo cấu hình có tên (UI-D14);
  - **tỉ lệ đặt sẵn khi tạo** (dropdown, mặc định là tỉ lệ dùng gần nhất);
  - Clip; Khóa / Mở khóa; Phóng to / Thu nhỏ viewport (`VPMAX` / `VPMIN`).
- **Panel Layer:** Đóng băng layer ở viewport hiện tại; Cô lập layer trong viewport (`LAYVPI`).
- **Panel Không gian:** chuyển đối tượng giữa Model và Paper (`CHSPACE`).
- **Viewport sau khi tạo:** grip góc để đổi kích thước, **grip tỉ lệ** và grip di chuyển (phần 1 §9.5). Viewport bị khóa có biểu tượng khóa.
- **Phát hành:** EA (Layout mới, viewport chữ nhật, tỉ lệ, khóa, cấu hình có tên) · 1.0 (phần còn lại).

## 10. Truy vấn và dữ liệu

### UI-I01 · Đo nhanh (MEASUREGEOM)
- **Chế độ rê chuột:**
  - hiện **khoảng cách và góc** giữa các đối tượng gần con trỏ, dưới dạng kích thước tạm;
  - bấm vào bên trong vùng kín thì hiện **diện tích và chu vi**;
  - Shift + bấm để cộng dồn nhiều vùng.
- **Thanh tùy chọn:** Khoảng cách, Bán kính, Góc, Diện tích, Thể tích (Sau), Nhanh, Thoát.
- **Kết quả:** sao chép giá trị bằng một bấm; đơn vị theo `UNITS` hoặc quy đổi nhanh (mm ↔ m, m²).
- **Phát hành:** V.

### UI-I02 · Kết quả đo
Nhãn số đo trên vùng vẽ, cùng dòng ghi trên dòng lệnh. **Thẻ kết quả** có nút sao chép và "Chèn thành text" (P1). Đo diện tích cộng/trừ (`AREA`) có tổng cập nhật theo thời gian thực. **Phát hành:** V.

### UI-I03 · Cửa sổ văn bản (LIST, TEXTSCR)
Kết quả của `LIST`, `MASSPROP`, `STATUS`, `TIME`, `DBLIST` và lịch sử lệnh. Có tìm kiếm, sao chép, lưu ra tệp. **Phát hành:** V.

### UI-I04 · Tổng chiều dài, tổng diện tích (TCLENGTH, TCAREA; TrionCAD, VN)
- **Chọn đối tượng:** có bộ lọc loại.
  - Chiều dài: line, polyline, cung, spline, đường tròn.
  - Diện tích: đối tượng kín, hatch.
- **Bảng kết quả:**
  - số đối tượng;
  - **tổng**;
  - **phân theo layer** và **theo loại**;
  - quy đổi đơn vị (mm → m, mm² → m²), độ chính xác.
- **Hành động:** sao chép; chèn thành text hoặc bảng; xuất Excel/CSV; tô sáng các đối tượng đã tính.
- **Phát hành:** VP (xem, sao chép, xuất Excel/CSV) · EA (chèn vào bản vẽ).

### UI-I05 · Bảng diện tích phòng (TCROOMAREA; TrionCAD, VN)
- **Lấy dữ liệu:**
  - chỉ điểm bên trong từng phòng, hoặc chọn các đường biên kín / hatch;
  - tên phòng lấy từ: chữ nằm trong vùng / attribute / nhập tay.
- **Bảng:** STT, Tên phòng, Diện tích (m²), Chu vi (m), Ghi chú, có dòng tổng. Sắp xếp được; làm tròn 2 chữ số.
- **Hành động:**
  - **chèn bảng** theo kiểu bảng TCVN;
  - cập nhật khi đường biên thay đổi (P2, qua field);
  - xuất Excel.
- **Phát hành:** 1.0.

### UI-I06 · Trích xuất dữ liệu (DATAEXTRACTION)
Trình hướng dẫn theo bước:
1. **Nguồn:** bản vẽ hiện hành / chọn đối tượng / nhiều bản vẽ (P2).
2. **Loại đối tượng:** chỉ block / không phải block / tất cả; chỉ block có attribute.
3. **Thuộc tính:** chọn theo nhóm (attribute, chung, hình học…).
4. **Tinh chỉnh:**
   - lưới xem trước;
   - gộp các dòng giống nhau, cột số lượng, cột tên;
   - sắp xếp, lọc.
5. **Đầu ra:** chèn bảng vào bản vẽ; xuất XLSX/CSV.
6. **Kiểu bảng.**
7. **Hoàn tất:** lưu thiết lập thành tệp để chạy lại.

**Phát hành:** 1.0 (bước 1–5 rút gọn) · Sau (đầy đủ).

### UI-I07 · So sánh bản vẽ (COMPARE)
- **Chọn:** bản vẽ A (hiện hành) và bản vẽ B (tệp khác hoặc bản lưu trước).
- **Thiết lập:** loại đối tượng cần so; dung sai; có so định dạng chữ không.
- **Chế độ xem:** chồng hình (màu: chỉ có ở A / chỉ có ở B / giống nhau, kèm ký hiệu) **hoặc hai khung cạnh nhau** đồng bộ zoom/pan; bật/tắt từng nhóm.
- **Danh sách thay đổi:**
  - các cột loại thay đổi (thêm / xóa / sửa), loại đối tượng, layer, thuộc tính đổi;
  - ◀ ▶ để duyệt, vùng vẽ tự zoom tới;
  - lọc theo loại.
- **Đánh dấu:** tự vẽ đám mây quanh các vùng thay đổi (hình chữ nhật / đa giác / đám mây; lề). Ở Viewer Pro, đám mây lưu thành markup (UI-M02); ở Pro thì vẽ vào bản vẽ.
- **Xuất:** ảnh chụp; báo cáo PDF/CSV.
- **Nhập đối tượng** đã thay đổi từ B sang A (P2).
- **Phát hành:** VP · EA.

### UI-I08 · Làm sạch hình học (TCCLEANUP, P2)
Quét → danh sách vấn đề: khe hở, đoạn thừa, đoạn hụt, góc gần 0°/90° bị lệch, đối tượng trùng, độ dài 0. Thiết lập dung sai cho từng loại. Hành động: Sửa / Sửa tất cả / Bỏ qua; zoom tới; báo cáo. Kèm trang **Sức khỏe bản vẽ**: tóm tắt một trang (regapps, block không dùng, đối tượng độ dài 0, proxy, xref thiếu, font bị thay, dung lượng có thể giảm), mỗi mục có nút sửa nhanh (mở UI-B09, UI-J02…). **Phát hành:** Sau.

### UI-I09 · Kiểm tra bản vẽ theo TCVN (TCCHECK, P2)
- **Bộ quy tắc:** TCVN kiến trúc, TCVN kết cấu, quy tắc của công ty.
  - Ví dụ quy tắc: tên layer; chiều cao chữ theo tỉ lệ; độ dày nét; kiểu dim; ô khung tên bắt buộc.
- **Kết quả:** danh sách lỗi có mức độ (lỗi / cảnh báo / gợi ý), **giải thích kèm trích dẫn điều khoản**; Sửa tự động (nếu có) / Bỏ qua / Ghi chú.
- **Xuất báo cáo.** Trợ lý AI giải thích lỗi: Sau.
- Cùng khung giao diện dùng cho **kiểm tra chuẩn `.dws`** (`STANDARDS`, `CHECKSTANDARDS`: gắn tệp chuẩn, chạy kiểm tra, sửa từng lỗi) và **dịch layer** (`LAYTRANS`: ánh xạ layer nguồn → layer chuẩn, lưu và nạp ánh xạ).
- **Phát hành:** Sau.

### UI-I10 · Máy tính nhanh (QUICKCALC, P2)
- **Palette:** ô biểu thức kèm lịch sử; bàn phím số và hàm khoa học; biến người dùng.
- **Lấy từ bản vẽ:** tọa độ điểm, khoảng cách, góc, giao điểm của hai đường qua 4 điểm.
- **Quy đổi đơn vị:** độ dài, diện tích, thể tích, góc.
- **Đưa kết quả đi:** dán vào dòng lệnh; áp vào ô đang sửa của palette Thuộc tính. Mở được từ nút máy tính trong ô nhập số (UI-C09).
- **Phát hành:** Sau. Trước đó dùng `'CAL` và biểu thức trong ô nhập số.

## 11. Tiện ích bản vẽ

### UI-B08 · Thuộc tính bản vẽ (DWGPROPS)
- **Tab Chung:** thông tin tệp.
- **Tab Tóm tắt:** tiêu đề, chủ đề, tác giả, từ khóa, ghi chú, gốc hyperlink.
- **Tab Thống kê:** ngày tạo, ngày sửa, người lưu cuối, số lần sửa, tổng thời gian sửa.
- **Tab Tùy biến:** danh sách tên/giá trị, dùng cho field trong khung tên.
- **Phát hành:** V (xem) · EA.

### UI-B09 · Dọn dẹp (PURGE)
- **Hai chế độ xem:** Mục dọn được / Mục **không** dọn được (kèm lý do, ví dụ "Layer đang có 25 đối tượng").
- **Cây danh mục:** Block, Kiểu kích thước, Nhóm, Layer, Linetype, Kiểu multileader, Plot style, Shape, Kiểu bảng, Kiểu chữ, Kiểu hiển thị, **Ứng dụng đăng ký (regapps)**.
- **Tùy chọn:**
  - xác nhận từng mục;
  - dọn cả mục lồng nhau;
  - đối tượng không tên: hình học độ dài 0, text rỗng, dữ liệu mồ côi.
- **Nút:** Dọn mục đã chọn; Dọn tất cả.
- **Khác:** xem trước block; **ước tính dung lượng giảm được** (TrionCAD). Bản vẽ VN thường phình to vì regapps.
- **Phát hành:** EA.

### UI-B10 · Báo cáo kiểm tra lỗi và phục hồi (AUDIT, RECOVER)
Tóm tắt (số lỗi tìm thấy / đã sửa); nhật ký chi tiết; Lưu báo cáo; hành động tiếp theo (Lưu thành…, Mở bản đã phục hồi). **Phát hành:** V (phục hồi để xem) · EA.

### UI-B11 · Đóng gói gửi đi (ETRANSMIT)
- **Tab Cây tệp / Bảng tệp:** bản vẽ, xref, ảnh, **font (kể cả SHX tiếng Việt)**, plot style, PC3, liên kết dữ liệu…; ô chọn từng tệp; Thêm tệp.
- **Thiết lập đóng gói** (Mới / Sửa):
  - kiểu gói (thư mục / ZIP);
  - định dạng tệp (giữ nguyên / 2018 / 2013 / 2010 / 2007 / 2004 / 2000);
  - thư mục đích, cách đặt tên;
  - cấu trúc đường dẫn (giữ cấu trúc thư mục / đặt chung một thư mục / giữ nguyên);
  - hành động: đặt máy in mặc định là "Không", **bind xref**, **dọn dẹp bản vẽ**, **chuyển font sang Unicode** (TrionCAD);
  - kèm theo: font, tệp từ liên kết dữ liệu, tham chiếu chưa nạp.
- **Báo cáo đóng gói** (xem, lưu); ô ghi chú.
- **Phát hành:** 1.0.

### UI-B12 · Đổi tên (RENAME)
Loại đối tượng có tên (Block, Kiểu kích thước, Layer, Linetype, Kiểu multileader, Kiểu bảng, Kiểu chữ, UCS, View, Viewport); danh sách; tên cũ → tên mới (có wildcard để đổi hàng loạt, ví dụ `KT-*` → `A-*`). **Phát hành:** EA.

### UI-B14 · Xử lý tệp hàng loạt (TCBATCH)
Dùng chung màn hình với chuyển font hàng loạt (UI-J03): cùng phần đầu vào, đầu ra và bảng tiến trình, chỉ khác các thao tác được bật.
- **Đầu vào:** tệp hoặc thư mục (gồm thư mục con); lọc `*.dwg`, `*.dxf`, `*.dwt`.
- **Thao tác** (ô chọn, chạy theo thứ tự): kiểm tra và sửa lỗi; dọn dẹp (kể cả regapps); chuyển font sang Unicode; **chuyển phiên bản** (DWG 2018 / 2013 / 2010 / 2007 / 2004 / 2000, DXF); đặt máy in của các layout về "Không"; chạy script `.scr` (P2).
- **Đầu ra:** ghi đè (có `.bak`) / lưu sang thư mục khác (giữ cấu trúc) / thêm hậu tố vào tên tệp.
- **Chạy:** ở nền, đa luồng; tạm dừng, hủy; báo cáo theo từng tệp; lưu thiết lập thành mẫu.
- **Phát hành:** 1.0. Ở Viewer Pro chỉ bật thao tác chuyển font (chính là UI-J03).
