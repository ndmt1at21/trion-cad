# ABOUT — giới thiệu TrionCAD

Nguồn: 07.3 §6 UI-L13 ("Phiên bản, bản build, kênh; giấy phép; thông tin hệ thống (sao chép được). Danh sách thư viện mã
nguồn mở và giấy phép (bắt buộc với LGPL/BSD). Điều khoản sử dụng, chính sách riêng tư. Ghi chú nhãn hiệu. Liên hệ hỗ
trợ"), 05 §15 (Qt LGPL liên kết động, SBOM), prototype `SC.about` (`design/prototype/js/screens2.js`), tài liệu công khai
AutoCAD (ABOUT). Thực hiện: UIL-011 (`app/system/about/AboutDialog`, `gen_licenses.py`), bước integration (đăng ký hộp
thoại `about` với DialogHost, lệnh ABOUT vào CommandRouter).

## Gọi

| Cách | Ghi chú |
|---|---|
| Gõ `ABOUT` | |
| Thanh tiêu đề › Trợ giúp › Giới thiệu TrionCAD | Macro `^C^C_ABOUT` |
| macOS: menu TrionCAD › Giới thiệu TrionCAD | Từ mốc macOS alpha (07.1 §13.6) |
| Script, LISP | `(command "_.ABOUT")` mở hộp thoại; không có bản dòng lệnh |

Phiên bản: mọi phiên bản (V), kể cả Viewer. Không trong suốt: hủy lệnh đang chạy như lệnh thường. Không có prompt.

## Hành vi

Hộp thoại không chế độ "Giới thiệu TrionCAD" (600 px; mở lại khi đang mở thì đưa lên, không mở bản thứ hai):

| Phần | Nội dung |
|---|---|
| Đầu | Logo, "TrionCAD" và huy hiệu phiên bản (Viewer, Viewer Pro, Pro, "Dùng thử: còn N ngày", Giáo dục, Doanh nghiệp; cảnh báo ngày offline); dòng "phiên bản · bản build · kênh" (Nội bộ, Beta, Ổn định) |
| Thông tin hệ thống | Hệ điều hành và kiến trúc · CPU và số luồng · RAM; Đồ họa: backend · GPU · độ phân giải · tỉ lệ; Ngôn ngữ · Hồ sơ hiện hành. Nút **Sao chép** |
| Thư viện mã nguồn mở | Bảng tên thư viện, giấy phép (Qt 6: "LGPL-3.0, liên kết động"); **Văn bản giấy phép…** (hoặc Enter, nhấp đúp trên dòng) mở văn bản đầy đủ |
| Chân trang | "AutoCAD® là nhãn hiệu của Autodesk, Inc. Các nhãn hiệu khác thuộc chủ sở hữu tương ứng."; Điều khoản sử dụng, Chính sách riêng tư (mở trang web), Liên hệ hỗ trợ |

1. Hộp hiện ngay (≤ 150 ms đến khung đầu); CPU, RAM, danh sách giấy phép, backend và GPU đọc ở nền, chưa về thì phần đó ghi
   "Đang đọc…" và nút Sao chép vô hiệu. Huy hiệu phiên bản đổi ngay khi giấy phép đổi lúc hộp đang mở.
2. **Sao chép** chép vào clipboard: dòng "TrionCAD <phiên bản giấy phép> · <phiên bản> · bản build … · kênh …" và ba dòng
   thông tin hệ thống, đúng như đang hiện; nút thành "Đã sao chép". Dùng khi gửi bộ phận hỗ trợ.
3. **Văn bản giấy phép** của Qt nêu: Qt dùng theo LGPL-3.0, liên kết động, TrionCAD không sửa mã Qt; người dùng có quyền thay
   thư viện Qt bằng bản tương thích nhị phân (kể cả bản tự build từ mã nguồn đã sửa) và cách thay trên hệ điều hành đang
   chạy; nơi lấy mã nguồn Qt; rồi văn bản LGPL-3.0 và GPL-3.0. Thư viện khác: dòng bản quyền và văn bản giấy phép.
4. Danh sách thư viện sinh lúc build bằng `app/system/about/gen_licenses.py` từ danh sách phụ thuộc (hiện là danh sách tĩnh
   theo 05 §2 trong `third_party.json`; khi có vcpkg manifest thì đọc từ manifest). Một phụ thuộc thiếu văn bản giấy phép thì
   bước build lỗi, CI báo.
5. **Liên hệ hỗ trợ**: đóng hộp Giới thiệu rồi mở Liên hệ hỗ trợ (UI-L17); khi chưa có UI-L17 thì mở trang hỗ trợ trên web.
   Điều khoản, Chính sách riêng tư mở trình duyệt; không mở được thì hộp ở lại và báo kèm địa chỉ để chép.
6. Đóng (Enter), Esc, nút ×: đóng, focus về chỗ cũ. F1: trợ giúp của ABOUT. Không đổi bản vẽ, không tạo bước hoàn tác.

## Trường hợp biên

- Chưa có giấy phép: huy hiệu "Viewer".
- Không đọc được backend, GPU (chưa khởi tạo vùng vẽ): "Đồ họa: chưa rõ · <độ phân giải> · <tỉ lệ>".
- Hồ sơ mặc định: "<<Mặc định>>".
- Màn hình nhỏ nhất được hỗ trợ (1366×768): hộp vừa màn hình, không cắt chữ; bảng thư viện cuộn khi nhiều hơn 6 dòng.

## Khác AutoCAD

- AutoCAD ABOUT hiện phiên bản, bản build và liên kết thông tin sản phẩm; TrionCAD thêm thông tin hệ thống sao chép được
  (cho hỗ trợ), danh sách thư viện mã nguồn mở với văn bản giấy phép đầy đủ ngay trong hộp và cách thay thư viện Qt theo
  LGPL.
