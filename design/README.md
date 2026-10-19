# Thiết kế UI TrionCAD

Prototype bấm được và hệ thống thiết kế của TrionCAD, làm theo đặc tả `docs/07-dac-ta-ui-1/2/3` (172 mã màn hình).
Viết bằng HTML, CSS và JS thuần, không cần build. Mở tệp bằng trình duyệt là chạy được, kể cả qua `file://`.

## Mở

| Trang | Tệp | Nội dung |
|---|---|---|
| Prototype | `prototype/index.html` | Cửa sổ chính: Start, ribbon, vùng vẽ tương tác thật, palette, dòng lệnh, các hộp thoại |
| Hệ thống thiết kế | `prototype/styleguide.html` | Token màu (Tối, Sáng), chữ, kích thước, biểu tượng, thành phần, danh sách kiểm |
| Công cụ miễn phí (Đợt 0) | `prototype/cong-cu-mien-phi.html` | UI-N01 Bác sĩ Font, UI-N02 Thư viện TCVN, UI-N03 trang tải |
| Thanh toán, tài khoản (Đợt 2) | `prototype/thanh-toan.html` | UI-L16 trang mua và trang tài khoản trên web |
| Ba hướng hình ảnh | `phuong-an/index.html` | Trang so sánh dùng để chọn hướng A · Than chì |

Trong prototype, nút **Prototype** trên thanh tiêu đề dùng để:

- đổi phiên bản (Viewer, Viewer Pro, Pro, Dùng thử, Giáo dục, Doanh nghiệp). Màn hình Đợt 2 cần Viewer Pro, Đợt 3 cần Pro; mở từ Danh mục thì prototype tự chuyển;
- đổi chủ đề, mật độ, Giao diện nhẹ;
- xem khung 1366 hoặc 1920;
- mở **Danh mục 172 màn hình**;
- chạy các kịch bản như tệp lỗi, tệp lớn, thiếu xref.

Mở thẳng một màn hình bằng mã: `index.html#UI-H02`.

## Cấu trúc

```
prototype/
  css/tokens.css   token màu, chữ, kích thước (hợp đồng với bản Qt)
  css/app.css      mọi thành phần và khung ứng dụng
  css/web.css      thành phần trang web (tải về, thanh toán, tài khoản)
  css/page.css     khung các trang duyệt thiết kế (Đợt 0, trang web)
  js/core.js       tiện ích, trạng thái, phiên bản
  js/icons.js      bộ biểu tượng tự vẽ 16/32 px
  js/ui.js         menu, tooltip, hộp thoại, toast, ô nhập số UI-C09
  js/scene.js      dữ liệu bản vẽ mẫu (nhà 2 tầng, kết cấu)
  js/canvas.js     vẽ 2D, bắt điểm, chọn, layout và viewport
  js/cad.js        tương tác vùng vẽ (chọn, lấy điểm, đo nhanh)
  js/cmd.js        dòng lệnh và lệnh
  js/palettes.js   Thuộc tính, Layer, Bác sĩ Font, Xref
  js/ribbons.js    dữ liệu ribbon, ô Tìm lệnh, menu Prototype
  js/shell.js      khung ứng dụng
  js/screens1.js   Start, menu ứng dụng, mở tệp, layer, tìm, ...
  js/screens2.js   in và xuất, tùy chọn, nâng cấp, trợ giúp, ...
  js/screens3.js   Đợt 2: Đếm, Tổng chiều dài/diện tích, So sánh bản vẽ, Markup
  js/screens4.js   Đợt 2: In nhiều layout, In hàng loạt theo khung tên, chuyển font hàng loạt, attribute với Excel, giấy phép
  js/editor.js     Đợt 3: lõi bản biên tập: hoàn tác theo nhóm lệnh, biến đổi, grip, nhập động, Lưu thành, đóng khi chưa lưu, clipboard
  js/draw.js       Đợt 3: lệnh vẽ và sửa (LINE, PLINE, CIRCLE, ARC… MOVE, COPY, TRIM, FILLET, OFFSET, PEDIT…)
  js/annot.js      Đợt 3: TEXT và sửa chữ tại chỗ, MTEXT và tab Soạn văn bản, kích thước, multileader, nhấp đúp theo loại đối tượng
  js/screens5.js   Đợt 3: bộ chọn màu, linetype, lineweight, khớp thuộc tính, kiểu chữ, kiểu kích thước, kiểu multileader, plot style, đơn vị, hỗ trợ vẽ…
  js/screens6.js   Đợt 3: Hatch, Mảng, Đường biên, Xóa trùng lặp, Nhóm, Chọn nhanh, Bộ lọc chọn, Chọn tương tự, lệnh layer
  js/screens7.js   Đợt 3: block và attribute, tham chiếu ngoài, ảnh, layout và viewport, khung tên, mẫu TCVN, Excel, Thư viện TCVN
  js/screens8.js   Đợt 3: tệp bị khóa, phục hồi, dọn dẹp, kiểm tra lỗi, đổi tên, tìm và thay, LISP, script, viết tắt lệnh, chuyển cài đặt, dùng thử
  js/catalog.js    danh mục 172 mã, trạng thái thiết kế
```

## Tiến độ (07.3 Phụ lục B)

| Đợt | Số mã | Đã xong | Một phần | Ghi chú |
|---|---|---|---|---|
| 0 | 3 | 3 | 0 | Công cụ miễn phí |
| 1 | 48 | 48 | 0 | Viewer. Phần Pro của A12, C01, C03, D10, J02 xong ở Đợt 3 |
| 2 | 12 | 12 | 0 | Viewer Pro. Các nút VP trên ribbon Viewer đều chạy được: G09, H04, H06, I04, I07, J03, J15, L01–L03, L16, M02 |
| 3 | 59 | 59 | 0 | Pro EA (bản biên tập): ribbon đủ 8 tab và workspace Cổ điển, grip, nhập động, lệnh vẽ, sửa có hoàn tác, chú thích, kiểu, hatch, mảng, block, xref, layout, TCVN, LISP |
| 4 | 38 | 0 | 0 | Pro 1.0 |
| 5 | 12 | 0 | 0 | Sau 1.0 |

Danh mục trong prototype luôn hiện trạng thái mới nhất.

## Quy ước

- **Token:** dùng tên token, không dùng mã màu trực tiếp. Bản Qt giữ đúng tên và kích thước trong `tokens.css`.
- **Biểu tượng:** tự vẽ, nét 1,25 px, đầu vuông. Không dùng emoji, không mô phỏng biểu tượng của Autodesk.
- **Chữ:** chữ phải đạt tương phản 4,5:1, viền điều khiển 3:1, ở cả hai chủ đề.
- **Khóa theo phiên bản:** tính năng bị khóa hiện huy hiệu, bấm vào thì mở trang nâng cấp UI-L04. Lệnh gõ tay bị khóa thì báo ngay trên dòng lệnh.
- **Vùng vẽ:** palette Thuộc tính mở sẵn ở ≥ 1600 px. Ở 1920×1080, vùng vẽ chiếm khoảng 60%; khi palette tự ẩn hoặc bật Clean Screen thì ≥ 75% (07.1 §1).
- **Viewer Pro không sửa DWG:** markup lưu tệp `.tcmk` cạnh bản vẽ, chuyển font lưu bản sao. Ghi đè tệp gốc, chèn vào bản vẽ, nhập attribute là của Pro.
- **Tác vụ nền** (in hàng loạt, chuyển font): đóng hộp thoại vẫn chạy; tiến trình và nút hủy ở thanh trạng thái; xong thì có toast kèm hành động.
- **Hoàn tác theo lệnh:** mỗi lệnh là một bước hoàn tác (Ctrl+Z, nút tách trên QAT liệt kê 12 bước). Trong lệnh nhiều bước, tùy chọn Hoàn tác · U chỉ lùi bước con.
- **Prompt:** chip tùy chọn ghi nhãn tiếng Việt kèm chữ tắt tiếng Anh (Bán kính · R). Gõ chữ tắt kiểu AutoCAD luôn chạy, kể cả từ khóa ẩn của grip (MO, RO, SC, MI).
- **Lệnh chưa đến đợt:** nút ribbon của lệnh v1.0 hoặc sau v1.0 vẫn hiện, bấm thì báo đợt có lệnh đó.
