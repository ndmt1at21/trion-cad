/* TrionCAD prototype · dòng lệnh UI-A06 và các lệnh của Viewer. Lệnh của bản Pro hiện thông báo nâng cấp (07.1 §2). */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic, G = TC.geo;

  /* ---------- danh mục lệnh: TÊN|viết tắt|phiên bản|đợt thiết kế|mô tả|từ khóa tiếng Việt|biểu tượng|vị trí ribbon ---------- */
  var RAW = [
    'LINE|L|PRO|3|Vẽ đoạn thẳng|duong thang, doan thang, ve duong|line|Trang chủ › Vẽ',
    'PLINE|PL|PRO|3|Vẽ polyline|da tuyen, duong gap khuc|pline|Trang chủ › Vẽ',
    'CIRCLE|C|PRO|3|Vẽ đường tròn|duong tron, hinh tron, vong tron|circle|Trang chủ › Vẽ',
    'ARC|A|PRO|3|Vẽ cung tròn|cung tron, cung|arc|Trang chủ › Vẽ',
    'RECTANG|REC|PRO|3|Vẽ hình chữ nhật|hinh chu nhat, chu nhat|rect|Trang chủ › Vẽ',
    'POLYGON|POL|PRO|3|Vẽ đa giác đều|da giac|rect|Trang chủ › Vẽ',
    'ELLIPSE|EL|PRO|3|Vẽ ellipse|elip, hinh bau duc|ellipse|Trang chủ › Vẽ',
    'HATCH|H|PRO|3|Tô vật liệu (hatch)|to vat lieu, mat cat, hatch, to mau|hatch|Trang chủ › Vẽ',
    'SPLINE|SPL|PRO|3|Vẽ spline|duong cong tron|spline|Trang chủ › Vẽ',
    'XLINE|XL|PRO|3|Đường dựng hình vô hạn|duong dung hinh|xline|Trang chủ › Vẽ',
    'REVCLOUD|REVCLOUD|PRO|3|Đám mây sửa đổi|dam may|cloud|Trang chủ › Vẽ',
    'MTEXT|T,MT|PRO|3|Văn bản nhiều dòng|chu, van ban, viet chu|mtext|Trang chủ › Chú thích',
    'TEXT|DT|PRO|3|Văn bản một dòng|chu mot dong|mtext|Trang chủ › Chú thích',
    'DIM|DIM|PRO|3|Kích thước thông minh|kich thuoc, ghi kich thuoc, do kich thuoc|dim|Trang chủ › Chú thích',
    'DIMLINEAR|DLI|PRO|3|Kích thước thẳng|kich thuoc thang|dim|Chú thích › Kích thước',
    'MLEADER|MLD|PRO|3|Multileader, đường dẫn ghi chú|duong dan, ghi chu, leader|leader|Trang chủ › Chú thích',
    'QLEADER|LE|PRO|4|Leader nhanh|leader nhanh|leader|Chú thích › Đường dẫn',
    'TABLE|TB|PRO|4|Bảng|bang, bang bieu|table|Trang chủ › Chú thích',
    'MOVE|M|PRO|3|Di chuyển|di chuyen, dich chuyen|move|Trang chủ › Sửa',
    'COPY|CO,CP|PRO|3|Sao chép|sao chep, copy|copy|Trang chủ › Sửa',
    'ROTATE|RO|PRO|3|Xoay|xoay|rotate|Trang chủ › Sửa',
    'MIRROR|MI|PRO|3|Đối xứng|doi xung, lat|mirror|Trang chủ › Sửa',
    'STRETCH|S|PRO|3|Kéo giãn|keo gian|stretch|Trang chủ › Sửa',
    'SCALE|SC|PRO|3|Tỉ lệ|ti le, phong to doi tuong|scale|Trang chủ › Sửa',
    'TRIM|TR|PRO|3|Cắt|cat, cat bo, xen|trim|Trang chủ › Sửa',
    'EXTEND|EX|PRO|3|Kéo dài|keo dai|extend|Trang chủ › Sửa',
    'FILLET|F|PRO|3|Bo góc|bo goc, bo tron|fillet|Trang chủ › Sửa',
    'CHAMFER|CHA|PRO|3|Vát góc|vat goc|fillet|Trang chủ › Sửa',
    'ARRAY|AR|PRO|3|Mảng|mang, nhan ban|array|Trang chủ › Sửa',
    'ERASE|E|PRO|3|Xóa đối tượng|xoa|erase|Trang chủ › Sửa',
    'EXPLODE|X|PRO|3|Phá khối|pha khoi, pha block|explode|Trang chủ › Sửa',
    'OFFSET|O|PRO|3|Offset, dịch song song|offset, song song, dich|offset|Trang chủ › Sửa',
    'JOIN|J|PRO|3|Nối|noi|pline|Trang chủ › Sửa',
    'BREAK|BR|PRO|3|Bẻ|be, cat doan|trim|Trang chủ › Sửa',
    'LENGTHEN|LEN|PRO|3|Kéo dài thêm|keo dai them|extend|Trang chủ › Sửa',
    'MATCHPROP|MA|PRO|3|Khớp thuộc tính|khop thuoc tinh, quet thuoc tinh|match|Trang chủ › Thuộc tính',
    'OVERKILL|OVERKILL|PRO|3|Xóa trùng lặp|xoa trung, xoa trung lap|erase|Trang chủ › Sửa',
    'INSERT|I|PRO|3|Chèn block|chen block, chen|insert|Trang chủ › Block',
    'BLOCK|B|PRO|3|Tạo block|tao block|bcreate|Trang chủ › Block',
    'BEDIT|BE|PRO|4|Block Editor|sua block|bedit|Trang chủ › Block',
    'U|U|PRO|3|Hoàn tác|hoan tac, undo|undo|Thanh truy cập nhanh',
    'REDO|REDO|PRO|3|Làm lại|lam lai|redo|Thanh truy cập nhanh',
    'NEW|NEW|PRO|3|Bản vẽ mới|ban ve moi, tao moi|new|Menu ứng dụng',
    'QSAVE|QSAVE|PRO|3|Lưu bản vẽ|luu|save|Thanh truy cập nhanh',
    'SAVEAS|SAVEAS|PRO|3|Lưu thành|luu thanh, luu ban khac|saveas|Menu ứng dụng',
    'PURGE|PU|PRO|3|Dọn dẹp bản vẽ|don dep, xoa rac|wrench|Quản lý › Làm sạch',
    'AUDIT|AUDIT|PRO|3|Kiểm tra lỗi|kiem tra loi|wrench|Quản lý › Làm sạch',
    'TCLIB|TCLIB|PRO|3|Thư viện TCVN|thu vien, ky hieu tcvn|lib|Công cụ VN › TCVN',
    'TCTITLE|TCTITLE|PRO|3|Khung tên|khung ten|layout|Công cụ VN › TCVN',
    // Viewer và Viewer Pro
    'OPEN|OPEN|V|1|Mở bản vẽ|mo, mo tep, mo ban ve|open|Thanh truy cập nhanh',
    'CLOSE|CLOSE|V|1|Đóng bản vẽ|dong, dong ban ve|fileclose|Menu ứng dụng',
    'QUIT|EXIT|V|1|Thoát TrionCAD|thoat|exit|Menu ứng dụng',
    'NEWTAB|NEWTAB|V|1|Mở tab Start|start, trang dau|home|Tab tệp',
    'ZOOM|Z|V|1|Phóng to, thu nhỏ|phong to, thu nho, zoom|zoom|Xem › Điều hướng',
    'PAN|P|V|1|Kéo màn hình|keo man hinh, di chuyen man hinh|pan|Xem › Điều hướng',
    'REGEN|RE|V|1|Tạo lại hình|ve lai, tao lai hinh|regen|Xem › Điều hướng',
    'REGENALL|REA|V|1|Tạo lại hình ở mọi viewport|ve lai tat ca|regen|Xem › Điều hướng',
    'VIEW|V|V|1|Khôi phục view đã đặt tên|khung nhin, view|views|Xem › Điều hướng',
    'MEASUREGEOM|MEA|V|1|Đo nhanh, khoảng cách, bán kính, góc, diện tích|do, do dac, do nhanh|mquick|Đo › Đo',
    'DIST|DI|V|1|Đo khoảng cách|khoang cach, do dai, do khoang cach|dist|Đo › Đo',
    'AREA|AA|V|1|Đo diện tích, chu vi|dien tich, chu vi, do dien tich|area|Đo › Đo',
    'ID|ID|V|1|Tọa độ điểm|toa do, toa do diem|idpoint|Đo › Đo',
    'LIST|LI,LS|V|1|Liệt kê thông tin đối tượng|liet ke, thong tin|list|Đo › Thông tin',
    'TEXTSCR|TEXTSCR|V|1|Cửa sổ văn bản, lịch sử dòng lệnh|lich su lenh|list|F2',
    'PROPERTIES|PR,CH,MO|V|1|Palette Thuộc tính|thuoc tinh, tinh chat|props|Xem › Palette',
    'LAYER|LA|V|1|Quản lý layer|layer, lop, quan ly layer|layers|Xem › Layer',
    'LAYOFF|LAYOFF|V|1|Tắt layer của đối tượng (tạm thời trong Viewer)|tat layer, an layer|bulboff|Xem › Layer',
    'LAYISO|LAYISO|V|1|Cô lập layer|co lap layer|isolate|Xem › Layer',
    'LAYUNISO|LAYUNISO|V|1|Bỏ cô lập layer|bo co lap|unisolate|Xem › Layer',
    'LAYON|LAYON|V|1|Bật tất cả layer|bat tat ca layer, hien layer|bulb|Xem › Layer',
    'LAYFRZ|LAYFRZ|V|1|Đóng băng layer của đối tượng|dong bang layer|snow|Xem › Layer',
    'LAYTHW|LAYTHW|V|1|Rã đông tất cả layer|ra dong layer|sun|Xem › Layer',
    'FIXFONT|FIXFONT|V|1|Bác sĩ Font: sửa lỗi font tiếng Việt|font, loi font, sua font, tcvn3, vni, bac si font|fontdoc|Công cụ VN › Font',
    'EXTERNALREFERENCES|XR,XREF|V|1|Palette Tham chiếu ngoài|tham chieu ngoai, xref|xref|Xem › Palette',
    'PLOT|PRINT|V|1|In|in, in an, may in|print|In & Xuất › In',
    'PAGESETUP|PAGESETUP|V|1|Quản lý thiết lập trang|thiet lap trang, kho giay|print|In & Xuất › In',
    'PREVIEW|PRE|V|1|Xem trước bản in|xem truoc|preview|In & Xuất › In',
    'EXPORTPDF|EXPORTPDF|V|1|Xuất PDF|xuat pdf, pdf|pdf|In & Xuất › Xuất',
    'PNGOUT|PNGOUT|V|1|Xuất ảnh|xuat anh, png, jpg, chup anh|image|In & Xuất › Xuất',
    'FIND|FIND|V|1|Tìm chữ trong bản vẽ|tim, tim chu, tim kiem|findtext|Công cụ › Bản vẽ',
    'OPTIONS|OP|V|1|Tùy chọn|tuy chon, cai dat|gear|Menu ứng dụng',
    'GRAPHICSCONFIG|GRAPHICSCONFIG|V|1|Thiết lập đồ họa|do hoa, card man hinh, gpu|gpu|Công cụ › Hệ thống',
    'TRIONBENCH|TRIONBENCH|V|1|Kiểm tra hiệu năng máy|hieu nang, benchmark, toc do may|gauge|Công cụ › Hệ thống',
    'TCREPORT|TCREPORT|V|1|Gửi bản vẽ lỗi cho TrionCAD|bao loi, gui loi|send|Trợ giúp',
    'HELP|HELP|V|1|Trợ giúp|tro giup, huong dan|help|F1',
    'ABOUT|ABOUT|V|1|Giới thiệu TrionCAD|gioi thieu, phien ban|info|Trợ giúp',
    'RECOVER|RECOVER|V|1|Phục hồi bản vẽ lỗi|phuc hoi, tep loi|recover|Menu ứng dụng',
    'CLEANSCREENON|CLEANSCREENON|V|1|Clean Screen: thu gọn giao diện|man hinh sach|fullscreen|Ctrl+0',
    'SELECTALL|ALL|V|1|Chọn tất cả|chon tat ca|selall|Ctrl+A',
    'INPUTSEARCHOPTIONS|INPUTSEARCHOPTIONS|V|1|Thiết lập dòng lệnh|thiet lap dong lenh|gear|Chuột phải trên dòng lệnh',
    'COUNT|COUNT|VP|2|Đếm block|dem, dem block|count|Xem › Palette',
    'TCLENGTH|TCLENGTH|VP|2|Tổng chiều dài|tong chieu dai|sigma|Đo › Tổng hợp',
    'TCAREA|TCAREA|VP|2|Tổng diện tích|tong dien tich|sigma|Đo › Tổng hợp',
    'COMPARE|COMPARE|VP|2|So sánh bản vẽ|so sanh|compare|Công cụ › Bản vẽ',
    'FIXFONTBATCH|FIXFONTBATCH|VP|2|Chuyển font hàng loạt nhiều tệp|chuyen font hang loat|fontbatch|Công cụ › Bản vẽ',
    'TCBATCHPLOT|TCBATCHPLOT|VP|2|In hàng loạt theo khung tên|in hang loat, in khung ten|batchplot|In & Xuất › In',
    'PUBLISH|PUBLISH|VP|2|In nhiều layout, nhiều bản vẽ|in nhieu layout|publish|In & Xuất › In',
    'TCMARKUP|TCMARKUP|VP|2|Ghi chú markup|ghi chu, markup, danh dau, dam may, con dau|markup|Ghi chú › Markup',
    'TCATTOUT|TCATTOUT|VP|2|Xuất attribute ra Excel|xuat attribute, excel, khung ten|stats|Công cụ › Bản vẽ',
    'SELECT|SELECT|V|1|Chọn đối tượng|chon doi tuong|select|Dòng lệnh',
    'TCATTIN|TCATTIN|PRO|2|Nhập attribute từ Excel|nhap attribute, excel|stats|Công cụ › Bản vẽ',
    // Đợt 3 · bản biên tập (EA); đợt 4, 5 là lệnh của v1.0 và sau v1.0
    'GRADIENT|GD|PRO|4|Tô chuyển màu (gradient)|to chuyen mau, gradient|hatch|Trang chủ › Vẽ',
    'BOUNDARY|BO|PRO|3|Tạo đường biên kín|duong bien, boundary|pline|Trang chủ › Vẽ',
    'RAY|RAY|PRO|3|Vẽ tia|tia|xline|Trang chủ › Vẽ',
    'POINT|PO|PRO|3|Vẽ điểm|diem, point|idpoint|Trang chủ › Vẽ',
    'DIVIDE|DIV|PRO|3|Chia đều đối tượng|chia deu|idpoint|Trang chủ › Vẽ',
    'MEASURE|ME|PRO|3|Chia theo khoảng|chia theo khoang|idpoint|Trang chủ › Vẽ',
    'REGION|REG|PRO|4|Tạo region|region|area|Trang chủ › Vẽ',
    'WIPEOUT|WIPEOUT|PRO|4|Vùng che|vung che|rect|Chú thích › Đánh dấu',
    'DONUT|DO|PRO|4|Vành khuyên|vanh khuyen|circle|Trang chủ › Vẽ',
    'MLINE|ML|PRO|4|Đa tuyến song song|da tuyen song song, tuong doi|pline|Trang chủ › Vẽ',
    'CENTERMARK|CM|PRO|4|Dấu tâm|dau tam|circle|Chú thích › Kích thước',
    'CENTERLINE|CENL|PRO|4|Đường tâm|duong tam|line|Chú thích › Kích thước',
    'ARRAYRECT|ARRAYRECT|PRO|3|Mảng chữ nhật|mang chu nhat, nhan ban|array|Trang chủ › Sửa',
    'ARRAYPOLAR|ARRAYPOLAR|PRO|3|Mảng tròn|mang tron, nhan ban tron|array|Trang chủ › Sửa',
    'ARRAYPATH|ARRAYPATH|PRO|4|Mảng theo đường|mang theo duong|array|Trang chủ › Sửa',
    'ARRAYCLASSIC|ARRAYCLASSIC|PRO|3|Hộp thoại mảng cổ điển|mang co dien|array|Trang chủ › Sửa',
    'BLEND|BLEND|PRO|4|Nối cong|noi cong|spline|Trang chủ › Sửa',
    'BREAKATPOINT|BREAKATPOINT|PRO|3|Bẻ tại điểm|be tai diem|trim|Trang chủ › Sửa',
    'ALIGN|AL|PRO|4|Căn chỉnh|can chinh|move|Trang chủ › Sửa',
    'PEDIT|PE|PRO|3|Sửa polyline|sua polyline, noi polyline|pline|Trang chủ › Sửa',
    'SPLINEDIT|SPE|PRO|4|Sửa spline|sua spline|spline|Trang chủ › Sửa',
    'HATCHEDIT|HE|PRO|3|Sửa hatch|sua hatch|hatch|Trang chủ › Sửa',
    'REVERSE|REVERSE|PRO|4|Đảo chiều|dao chieu|undo|Trang chủ › Sửa',
    'DRAWORDER|DR|PRO|3|Thứ tự hiển thị|thu tu hien thi, len tren, xuong duoi|layers|Trang chủ › Sửa',
    'TEXTTOFRONT|TEXTTOFRONT|PRO|3|Đưa chữ lên trên cùng|dua chu len tren|mtext|Trang chủ › Sửa',
    'HATCHTOBACK|HATCHTOBACK|PRO|3|Đưa hatch xuống dưới cùng|dua hatch xuong duoi|hatch|Trang chủ › Sửa',
    'SETBYLAYER|SETBYLAYER|PRO|3|Đặt thuộc tính về ByLayer|dat ve bylayer|layers|Trang chủ › Sửa',
    'CHSPACE|CHSPACE|PRO|4|Chuyển đối tượng giữa Model và Paper|chuyen khong gian|layout|Layout › Không gian',
    'COPYBASE|COPYBASE|PRO|3|Sao chép có điểm gốc|sao chep co diem goc|copybase|Trang chủ › Clipboard',
    'COPYCLIP|COPYCLIP|PRO|3|Sao chép vào clipboard|sao chep clipboard|copyclip|Trang chủ › Clipboard',
    'CUTCLIP|CUTCLIP|PRO|3|Cắt vào clipboard|cat clipboard|cut|Trang chủ › Clipboard',
    'PASTECLIP|PASTECLIP|PRO|3|Dán|dan|paste|Trang chủ › Clipboard',
    'PASTEBLOCK|PASTEBLOCK|PRO|3|Dán dạng block|dan dang block|paste|Trang chủ › Clipboard',
    'PASTEORIG|PASTEORIG|PRO|3|Dán về tọa độ gốc|dan ve toa do goc|paste|Trang chủ › Clipboard',
    'PASTESPEC|PASTESPEC|PRO|4|Dán đặc biệt|dan dac biet|paste|Trang chủ › Clipboard',
    'SELECTSIMILAR|SELECTSIMILAR|PRO|3|Chọn tương tự|chon tuong tu|select|Trang chủ › Tiện ích',
    'QSELECT|QSELECT|PRO|3|Chọn nhanh theo thuộc tính|chon nhanh, loc doi tuong|qselect|Trang chủ › Tiện ích',
    'FILTER|FI|PRO|3|Bộ lọc chọn|bo loc chon|filter|Trang chủ › Tiện ích',
    'ADDSELECTED|ADDSELECTED|PRO|3|Tạo đối tượng cùng loại|tao doi tuong cung loai|plus|Chuột phải › Có vùng chọn',
    'GRIPEDIT|GRIPEDIT|PRO|3|Sửa bằng grip|grip|move|Vùng vẽ',
    'LAYMCUR|LAYMCUR|PRO|3|Đặt layer của đối tượng làm hiện hành|dat layer hien hanh|laycur|Trang chủ › Layer',
    'LAYMCH|LAYMCH|PRO|3|Khớp layer|khop layer, doi layer|laymatch|Trang chủ › Layer',
    'LAYLCK|LAYLCK|PRO|3|Khóa layer của đối tượng|khoa layer|lock|Trang chủ › Layer',
    'LAYULK|LAYULK|PRO|3|Mở khóa layer|mo khoa layer|unlock|Trang chủ › Layer',
    'LAYERP|LAYERP|PRO|3|Trạng thái layer trước|trang thai layer truoc|undo|Trang chủ › Layer',
    'LAYMRG|LAYMRG|PRO|4|Gộp layer|gop layer|layers|Trang chủ › Layer',
    'LAYDEL|LAYDEL|PRO|4|Xóa layer và đối tượng|xoa layer|trash|Trang chủ › Layer',
    'LAYWALK|LAYWALK|PRO|4|Duyệt layer|duyet layer|layers|Trang chủ › Layer',
    'COPYTOLAYER|COPYTOLAYER|PRO|4|Sao chép sang layer|sao chep sang layer|copy|Trang chủ › Layer',
    'LAYCUR|LAYCUR|PRO|4|Đổi sang layer hiện hành|doi sang layer hien hanh|laycur|Trang chủ › Layer',
    'LAYVPI|LAYVPI|PRO|4|Cô lập layer trong viewport|co lap layer viewport|isolate|Layout › Layer',
    'LAYERSTATE|LAS|PRO|4|Trạng thái layer|trang thai layer|layers|Trang chủ › Layer',
    'EATTEDIT|EATTEDIT|PRO|3|Sửa attribute|sua attribute, khung ten|attedit|Trang chủ › Block',
    'ATTEDIT|ATE|PRO|3|Sửa attribute (cổ điển)|sua attribute|attedit|Trang chủ › Block',
    'ATTDEF|ATT|PRO|3|Định nghĩa attribute|dinh nghia attribute|attedit|Chèn › Định nghĩa block',
    'BATTMAN|BATTMAN|PRO|4|Quản lý attribute của block|quan ly attribute|attedit|Chèn › Định nghĩa block',
    'ATTSYNC|ATTSYNC|PRO|4|Đồng bộ attribute|dong bo attribute|refresh|Chèn › Định nghĩa block',
    'ATTDISP|ATTDISP|PRO|3|Hiển thị attribute|hien thi attribute|eye|Chèn › Định nghĩa block',
    'BLOCKSPALETTE|BLOCKSPALETTE|PRO|3|Palette Blocks|palette block, thu vien block|blocks|Chèn › Block',
    'WBLOCK|W|PRO|3|Ghi block ra tệp|ghi block ra tep|saveas|Chèn › Block',
    'BASE|BASE|PRO|3|Điểm gốc bản vẽ|diem goc ban ve|idpoint|Chèn › Block',
    'GROUP|G|PRO|4|Tạo nhóm|nhom, tao nhom|group|Trang chủ › Nhóm',
    'UNGROUP|UNGROUP|PRO|4|Bỏ nhóm|bo nhom|ungroup|Trang chủ › Nhóm',
    'GROUPEDIT|GROUPEDIT|PRO|4|Sửa nhóm|sua nhom|bedit|Trang chủ › Nhóm',
    'CLASSICGROUP|CLASSICGROUP|PRO|3|Quản lý nhóm|quan ly nhom|group|Trang chủ › Nhóm',
    'PICKSTYLE|PICKSTYLE|PRO|4|Bật/tắt chọn theo nhóm|chon theo nhom|select|Trang chủ › Nhóm',
    'COLOR|COL|PRO|3|Bộ chọn màu|mau, chon mau|plotstyle|Định dạng › Màu',
    'LINETYPE|LT,LTYPE|PRO|3|Quản lý linetype|linetype, kieu duong, net dut|lw|Định dạng › Linetype',
    'LWEIGHT|LW|PRO|3|Thiết lập độ dày nét|do day net, lineweight|lw|Định dạng › Lineweight',
    'STYLE|ST|PRO|3|Kiểu chữ|kieu chu, font chu|mtext|Chú thích › Văn bản',
    'DIMSTYLE|D,DST|PRO|3|Quản lý kiểu kích thước|kieu kich thuoc, dim style|dim|Chú thích › Kích thước',
    'MLEADERSTYLE|MLS|PRO|3|Kiểu multileader|kieu multileader, kieu ghi chu|leader|Chú thích › Đường dẫn',
    'TABLESTYLE|TS|PRO|4|Kiểu bảng|kieu bang|table|Chú thích › Bảng',
    'STYLESMANAGER|STYLESMANAGER|PRO|3|Quản lý plot style (CTB, STB)|plot style, ctb, net in|plotstyle|Xuất › In',
    'DDPTYPE|DDPTYPE,PTYPE|PRO|3|Kiểu điểm|kieu diem|idpoint|Định dạng › Kiểu điểm',
    'MLSTYLE|MLSTYLE|PRO|3|Kiểu đa tuyến song song|kieu tuong, mline style|pline|Định dạng › Kiểu đa tuyến song song',
    'UNITS|UN|PRO|3|Đơn vị bản vẽ|don vi, don vi ban ve|dist|Quản lý › Làm sạch',
    'DSETTINGS|DS,SE,OS|PRO|3|Thiết lập hỗ trợ vẽ|ho tro ve, snap, luoi, bat diem|osnap|Thanh trạng thái › chuột phải',
    'SCALELISTEDIT|SCALELISTEDIT|PRO|3|Danh sách tỉ lệ|danh sach ti le|anno|Chú thích › Tỉ lệ chú thích',
    'VPORTS|VPORTS|PRO|3|Cấu hình viewport|cau hinh viewport, chia man hinh|ws|Hiển thị › Viewport',
    'MVIEW|MV|PRO|3|Tạo viewport trong layout|tao viewport|layout|Layout › Viewport',
    'VPMAX|VPMAX|PRO|3|Phóng to viewport|phong to viewport|fullscreen|Layout › Viewport',
    'LAYOUT|LO|PRO|3|Layout mới, đổi tên, xóa|layout moi|layout|Layout › Layout',
    'MLEDIT|MLEDIT|PRO|3|Sửa đa tuyến song song|sua tuong|pline|Sửa đổi › Đa tuyến song song',
    'DIMALIGNED|DAL|PRO|3|Kích thước nghiêng|kich thuoc nghieng|dim|Chú thích › Kích thước',
    'DIMANGULAR|DAN|PRO|3|Kích thước góc|kich thuoc goc|angle|Chú thích › Kích thước',
    'DIMARC|DAR|PRO|4|Kích thước độ dài cung|kich thuoc cung|arc|Chú thích › Kích thước',
    'DIMRADIUS|DRA|PRO|3|Kích thước bán kính|kich thuoc ban kinh|radius|Chú thích › Kích thước',
    'DIMDIAMETER|DDI|PRO|3|Kích thước đường kính|kich thuoc duong kinh|circle|Chú thích › Kích thước',
    'DIMJOGGED|DJO|PRO|4|Kích thước bán kính gãy|kich thuoc gay|radius|Chú thích › Kích thước',
    'DIMORDINATE|DOR|PRO|4|Kích thước tọa độ điểm|kich thuoc toa do|idpoint|Chú thích › Kích thước',
    'DIMCONTINUE|DCO|PRO|3|Kích thước nối tiếp|kich thuoc noi tiep|dim|Chú thích › Kích thước',
    'DIMBASELINE|DBA|PRO|3|Kích thước cùng gốc|kich thuoc cung goc|dim|Chú thích › Kích thước',
    'QDIM|QDIM|PRO|4|Ghi kích thước nhanh|kich thuoc nhanh|dim|Chú thích › Kích thước',
    'DIMBREAK|DIMBREAK|PRO|4|Ngắt kích thước|ngat dim|dim|Chú thích › Kích thước',
    'DIMSPACE|DIMSPACE|PRO|4|Giãn cách kích thước|gian cach dim|dim|Chú thích › Kích thước',
    'DIMEDIT|DED|PRO|3|Sửa kích thước|sua kich thuoc|dim|Chú thích › Kích thước',
    'DIMTEDIT|DIMTEDIT|PRO|3|Sửa vị trí chữ kích thước|vi tri chu kich thuoc|dim|Chú thích › Kích thước',
    'MLEADEREDIT|MLE|PRO|3|Thêm, bớt nhánh multileader|them nhanh|leader|Chú thích › Đường dẫn',
    'MLEADERALIGN|MLA|PRO|4|Gióng hàng multileader|giong hang ghi chu|leader|Chú thích › Đường dẫn',
    'MLEADERCOLLECT|MLC|PRO|4|Gom nhóm multileader|gom ghi chu|leader|Chú thích › Đường dẫn',
    'TEXTEDIT|ED,DDEDIT|PRO|3|Sửa chữ tại chỗ|sua chu, sua text|mtext|Nhấp đúp vào chữ',
    'SCALETEXT|SCALETEXT|PRO|4|Đổi tỉ lệ chữ|ti le chu|mtext|Chú thích › Văn bản',
    'JUSTIFYTEXT|JUSTIFYTEXT|PRO|4|Đổi điểm canh chữ|canh chu|mtext|Chú thích › Văn bản',
    'TXT2MTXT|TXT2MTXT|PRO|4|Gộp chữ thành MText|gop chu|mtext|Chú thích › Văn bản',
    'SPELL|SP|PRO|5|Kiểm tra chính tả|chinh ta|findtext|Chú thích › Văn bản',
    'XATTACH|XA|PRO|3|Gắn DWG làm xref|gan xref, tham chieu ngoai|xref|Chèn › Tham chiếu',
    'IMAGEATTACH|IAT|PRO|3|Gắn ảnh|gan anh, chen anh|image|Chèn › Tham chiếu',
    'ATTACH|ATTACH|PRO|3|Gắn tham chiếu (DWG, ảnh, PDF)|gan tham chieu|xref|Chèn › Tham chiếu',
    'PDFATTACH|PDFATTACH|PRO|4|Gắn PDF làm underlay|gan pdf|pdf|Chèn › Tham chiếu',
    'CLIP|CLIP|PRO|3|Clip tham chiếu, ảnh, viewport|clip, cat khung|zwin|Chèn › Tham chiếu',
    'XCLIP|XC|PRO|3|Clip xref, block|clip xref|zwin|Chèn › Tham chiếu',
    'ADJUST|ADJUST|PRO|3|Điều chỉnh ảnh, underlay|dieu chinh anh|image|Chèn › Tham chiếu',
    'FRAME|FRAME|PRO|3|Khung tham chiếu|khung tham chieu|rect|Chèn › Tham chiếu',
    'XBIND|XB|PRO|3|Bind đối tượng phụ thuộc của xref|bind xref|xref|Chèn › Tham chiếu',
    'REFEDIT|REFEDIT|PRO|4|Sửa tại chỗ block, xref|sua tai cho|bedit|Chèn › Tham chiếu',
    'PDFIMPORT|PDFIMPORT|PRO|4|Nhập PDF thành đối tượng|nhap pdf|pdf|Chèn › Nhập',
    'FIELD|FIELD|PRO|4|Chèn field|field, truong du lieu|mtext|Chèn › Dữ liệu',
    'DATAEXTRACTION|DX|PRO|4|Trích xuất dữ liệu|trich xuat du lieu|stats|Chèn › Dữ liệu',
    'ADCENTER|ADC|PRO|4|DesignCenter|designcenter|folder|Chèn › Nội dung',
    'TOOLPALETTES|TP|PRO|4|Tool Palettes|tool palette|blocks|Chèn › Nội dung',
    'DWGPROPS|DWGPROPS|V|1|Thuộc tính bản vẽ|thuoc tinh ban ve|info|Quản lý › Làm sạch',
    'RENAME|REN|PRO|3|Đổi tên đối tượng có tên|doi ten, doi ten layer|bedit|Quản lý › Làm sạch',
    'APPLOAD|AP|PRO|3|Nạp ứng dụng, LISP|nap lisp, nap ung dung|pkg|Quản lý › Ứng dụng',
    'SCRIPT|SCR|PRO|3|Chạy script|chay script|play|Quản lý › Ứng dụng',
    'SECURITYOPTIONS|SECURITYOPTIONS|PRO|3|Tùy chọn bảo mật|bao mat, thu muc tin cay|shield|Quản lý › Ứng dụng',
    'ALIASEDIT|ALIASEDIT|PRO|3|Viết tắt lệnh|viet tat lenh, pgp|keyboard|Quản lý › Tùy biến',
    'TCMIGRATE|TCMIGRATE|PRO|3|Chuyển cài đặt từ AutoCAD|chuyen cai dat, tu autocad|ext|Quản lý › Tùy biến',
    'SYSVARMONITOR|SYSVARMONITOR|PRO|4|Theo dõi biến hệ thống|bien he thong|gear|Quản lý › Ứng dụng',
    'SETVAR|SET|PRO|4|Đặt biến hệ thống|bien he thong|gear|Quản lý › Ứng dụng',
    'CUI|CUI|PRO|5|Tùy biến giao diện|tuy bien giao dien|gear|Quản lý › Tùy biến',
    'WORKSPACE|WSCURRENT|PRO|4|Workspace|workspace, khong gian lam viec|ws|Quản lý › Tùy biến',
    'DRAWINGRECOVERY|DRM|PRO|3|Trình phục hồi bản vẽ|phuc hoi ban ve, tu luu|recover|Menu ứng dụng',
    'TCEXCELPASTE|TCEXCELPASTE|PRO|3|Dán bảng từ Excel|dan excel, bang excel|table|Chú thích › Bảng',
    'TCEXCELOUT|TCEXCELOUT|PRO|3|Xuất bảng ra Excel|xuat excel|stats|Chú thích › Bảng',
    'TCFRAME|TCFRAME|PRO|4|Tạo khung in trong Model theo tỉ lệ|khung in|layout|Công cụ VN › TCVN',
    'TCNUMBER|TCNUMBER|PRO|4|Đánh số tự động|danh so|mtext|Công cụ VN › Văn bản',
    'TCINCCOPY|TCINCCOPY|PRO|4|Sao chép tăng số|sao chep tang so|copy|Công cụ VN › Văn bản',
    'TCTEXT|TCTEXT|PRO|4|Công cụ text tiếng Việt|chu hoa chu thuong, noi text|mtext|Công cụ VN › Văn bản',
    'TCSYMBOL|TCSYMBOL|PRO|4|Ký hiệu kỹ thuật TCVN|cao do, truc, ky hieu|anno|Công cụ VN › Ký hiệu',
    'TCROOMAREA|TCROOMAREA|PRO|4|Bảng diện tích phòng|dien tich phong|area|Công cụ VN › Đo và thống kê',
    'TCSHEETS|TCSHEETS|PRO|4|Quản lý bộ bản vẽ|bo ban ve|layout|Công cụ VN › In ấn',
    'TCBATCH|TCBATCH|PRO|4|Xử lý tệp hàng loạt|xu ly hang loat|fontbatch|Quản lý › Tệp hàng loạt',
    'ETRANSMIT|ETRANSMIT|PRO|4|Đóng gói gửi đi|dong goi|pkg|Xuất › Gửi',
    'EXPORTLAYOUT|EXPORTLAYOUT|PRO|4|Xuất layout thành Model|xuat layout|layout|Xuất › Xuất',
    'UCS|UCS|PRO|3|Hệ tọa độ người dùng|ucs, toa do|iso|Hiển thị › Tọa độ',
    'UCSMAN|UC|PRO|4|Quản lý UCS|quan ly ucs|iso|Hiển thị › Tọa độ',
    'PLAN|PLAN|PRO|3|Nhìn vuông góc mặt bằng UCS|mat bang|views|Hiển thị › Tọa độ',
    'BURST|BURST|PRO|4|Phá block giữ chữ attribute|pha block giu chu|explode|Mở rộng › Block',
    'NCOPY|NCOPY|PRO|4|Sao chép đối tượng lồng|sao chep long|copy|Mở rộng › Block',
    'GATTE|GATTE|PRO|4|Sửa attribute hàng loạt|sua attribute hang loat|attedit|Mở rộng › Block',
    'TXTEXP|TXTEXP|PRO|4|Phá chữ thành nét|pha chu|explode|Mở rộng › Văn bản',
    'FLATTEN|FLATTEN|PRO|4|Làm phẳng Z|lam phang|move|Mở rộng › Sửa',
    'EXTRIM|EXTRIM|PRO|4|Cắt theo đường bao|cat theo duong bao|trim|Mở rộng › Sửa',
    'SAVEALL|SAVEALL|PRO|4|Lưu tất cả|luu tat ca|save|Mở rộng › Tệp',
    // Đợt 4 (v1.0): lệnh mới của các màn hình C02–L15, tab Mở rộng P1
    'UPDATEFIELD|UPDATEFIELD|PRO|4|Cập nhật field|cap nhat field, cap nhat truong du lieu|refresh|Chèn › Dữ liệu',
    'OBJECTSCALE|OBJECTSCALE|PRO|4|Tỉ lệ chú thích của đối tượng|ti le chu thich doi tuong|anno|Chú thích › Tỉ lệ chú thích',
    'TOLERANCE|TOL|PRO|4|Khung dung sai hình học|dung sai hinh hoc, gdt, dung sai|dim|Chú thích › Kích thước',
    'BLOCKREPLACE|BLOCKREPLACE|PRO|4|Thay block|thay block, doi block|insert|Mở rộng › Block',
    'TCBLOCKMATCH|TCBLOCKMATCH|PRO|5|Tìm và chuyển đổi block|tim va chuyen doi block, smart match|insert|Mở rộng › Block',
    'XPLODE|XPLODE|PRO|4|Phá block theo tùy chọn thuộc tính|pha block tuy chon|explode|Mở rộng › Block',
    'MPEDIT|MPEDIT|PRO|4|Sửa nhiều polyline|sua nhieu polyline|pline|Mở rộng › Sửa',
    'BCOUNT|BCOUNT|PRO|4|Đếm block (Express Tools)|dem block|count|Mở rộng › Block',
    'TCASE|TCASE|PRO|4|Đổi chữ hoa, thường|chu hoa, chu thuong, doi hoa thuong|mtext|Mở rộng › Văn bản',
    'TCOUNT|TCOUNT|PRO|4|Đánh số thứ tự cho chữ|danh so chu|mtext|Mở rộng › Văn bản',
    'SYSVDLG|SYSVDLG|PRO|4|Trình sửa biến hệ thống|bien he thong, sysvar|gear|Mở rộng › Công cụ',
    'TCKEYS|TCKEYS|PRO|4|Phím tắt|phim tat, gan phim|keyboard|Quản lý › Tùy biến',
    'WSSAVE|WSSAVE|PRO|4|Lưu workspace hiện tại|luu workspace, luu khong gian lam viec|ws|Quản lý › Tùy biến',
    'WSSETTINGS|WSSETTINGS|PRO|4|Thiết lập workspace|thiet lap workspace|ws|Quản lý › Tùy biến',
    'TCATTIN|TCATTIN|PRO|4|Nhập attribute từ Excel|nhap attribute, excel, khung ten|stats|Công cụ VN › Đo và thống kê',
    'BCLOSE|BCLOSE|PRO|4|Đóng Block Editor|dong block editor|close|Block Editor › Đóng',
    'BSAVE|BSAVE|PRO|4|Lưu block trong Block Editor|luu block|save|Block Editor › Mở/Lưu',
    'REFCLOSE|REFCLOSE|PRO|4|Lưu hoặc hủy thay đổi khi sửa tại chỗ|luu tham chieu|save|Sửa tham chiếu',
    'REFSET|REFSET|PRO|4|Thêm, bớt đối tượng của tập làm việc|tap lam viec|plus|Sửa tham chiếu',
    'ULAYERS|ULAYERS|PRO|4|Layer của underlay|layer underlay, layer pdf|layers|Chèn › Tham chiếu',
    'UOSNAP|UOSNAP|PRO|4|Bắt điểm vào underlay|bat diem underlay|osnap|Chèn › Tham chiếu',
    'VPCLIP|VPCLIP|PRO|4|Clip viewport|clip viewport|zwin|Layout › Viewport',
    'UCSICON|UCSICON|V|4|Biểu tượng UCS|bieu tuong ucs|iso|Hiển thị › Giao diện',
    'NAVVCUBE|NAVVCUBE|V|4|ViewCube|viewcube, khoi xem|views|Hiển thị › Giao diện',
    '3DORBIT|3DO|V|4|Xoay 3D|xoay 3d, orbit|views|Hiển thị › Điều hướng'
  ];
  var SYSVARS = [['OSMODE','Chế độ bắt điểm đang bật (mã bit)','4133'],['LTSCALE','Tỉ lệ linetype toàn cục','100'],['CLAYER','Layer hiện hành','KT-TUONG'],['LWDISPLAY','Hiển thị độ dày nét','0'],['FILEDIA','Hiện hộp thoại chọn tệp','1'],['PICKFIRST','Chọn trước rồi gọi lệnh','1'],['DIMSCALE','Tỉ lệ tổng thể kích thước','100'],['CANNOSCALE','Tỉ lệ chú thích hiện hành','1:100'],['INSUNITS','Đơn vị nội dung chèn vào','4 (mm)'],['ORTHOMODE','Chế độ Ortho','0'],['CMDECHO','Hiện lệnh khi chạy LISP','1'],['MIRRTEXT','Lật chữ khi đối xứng','0']];
  var CMDS = {}, ALIAS = {};
  RAW.forEach(function(r){ var f = r.split('|'); var c = { name:f[0], aliases:f[1] ? f[1].split(',') : [], need:f[2], wave:Number(f[3]), desc:f[4], vi:f[5], icon:f[6], where:f[7] }; CMDS[c.name] = c; c.aliases.forEach(function(a){ ALIAS[a] = c.name; }); });
  var HIDDEN = { GRIPEDIT:1 };
  TC.CMDS = CMDS; TC.SYSVARS = SYSVARS;
  var IMPL = {};
  var cmd = TC.cmd = {};
  cmd.impl = IMPL; // screens3/4 đăng ký lệnh của Đợt 2
  var usage = TC.load('usage', {});
  var recentCmds = [];
  cmd.alias = function(a, n){ a = String(a).toUpperCase(); if (n) ALIAS[a] = n; else delete ALIAS[a]; };
  cmd.def = function(n){ n = String(n).toUpperCase().replace(/^[_.'\-]+/, ''); return CMDS[n] || CMDS[ALIAS[n]] || null; };
  cmd.last = function(){ return recentCmds[0] || null; };
  cmd.recent = function(){ return recentCmds.slice(0, 8); };

  /* ---------- giao diện dòng lệnh ---------- */
  var CL = TC.cmdline = {}, hist = [], els = {}, prompt = null, acSel = 0, histIdx = -1, entered = [];
  CL.mount = function(root){
    root.innerHTML = '<div class="cmd-hist" id="cmd-hist"></div>' +
      '<div class="cmd-in"><span class="nest" id="cmd-nest"></span>' + ic('right', 'xs') + '<span class="cmd-name" id="cmd-name"></span><span class="cmd-pr" id="cmd-pr"></span>' +
      '<input id="cmd-in" aria-label="Dòng lệnh" autocomplete="off" spellcheck="false" placeholder="Gõ lệnh hoặc từ khóa tiếng Việt, ví dụ: đo, in, layer">' +
      '<span class="cmd-r"><button class="ib sm" type="button" id="cmd-histbtn" aria-label="Lịch sử dòng lệnh (F2)" data-tip="Lịch sử đầy đủ" data-key="F2">' + ic('up', 'xs') + '</button></span></div>' +
      '<div class="ac" id="ac" role="listbox" hidden></div>';
    els = { hist:$('#cmd-hist'), name:$('#cmd-name'), pr:$('#cmd-pr'), inp:$('#cmd-in'), ac:$('#ac'), nest:$('#cmd-nest') };
    els.inp.addEventListener('keydown', onKey);
    els.inp.addEventListener('input', function(){ if (!prompt) updateAC(); else hideAC(); if (TC.dyn) TC.dyn.typed(els.inp.value); });
    els.inp.addEventListener('blur', function(){ setTimeout(hideAC, 120); });
    $('#cmd-histbtn').addEventListener('click', function(){ TC.screens.textWindow(); });
    els.pr.addEventListener('click', function(e){ var c = e.target.closest('[data-opt]'); if (c) CL.submit(c.dataset.opt); });
    els.hist.addEventListener('click', function(e){ var c = e.target.closest('[data-run]'); if (c) { e.stopPropagation(); cmd.run(c.dataset.run); } var t = e.target.closest('[data-trialfor]'); if (t) TC.screens.upgrade(t.dataset.trialfor, 'PRO'); });
    root.addEventListener('contextmenu', function(e){ e.preventDefault(); TC.menu([{ label:'Lệnh gần đây', sub:recentCmds.slice(0, 8).map(function(c){ return { label:c, act:function(){ cmd.run(c); } }; }).concat(recentCmds.length ? [] : [{ label:'Chưa có', dis:true }]) }, { label:'Giá trị đã nhập gần đây', sub:entered.slice(0, 8).map(function(v){ return { label:esc(v), act:function(){ els.inp.value = v; els.inp.focus(); } }; }).concat(entered.length ? [] : [{ label:'Chưa có', dis:true }]) }, { label:'Sao chép lịch sử', icon:'copyclip', act:function(){ TC.copyText(hist.map(function(h){ return h.text; }).join('\n')); } }, { sep:1 }, { label:'Tùy chọn dòng lệnh…', icon:'gear', act:function(){ cmd.run('INPUTSEARCHOPTIONS'); } }], e.clientX, e.clientY, { up:true }); });
    CL.msg('TrionCAD đã sẵn sàng. Gõ lệnh như AutoCAD, hoặc gõ tiếng Việt (ví dụ “đo”, “in”) để tìm lệnh.', 'faint');
    setIdle();
  };
  function line(html, cls, text){ hist.push({ html:html, text:text || html.replace(/<[^>]+>/g, ''), cls:cls }); var p = document.createElement('p'); p.innerHTML = html; if (cls) p.className = cls; els.hist.appendChild(p); while (els.hist.children.length > 3) els.hist.removeChild(els.hist.firstChild); }
  CL.msg = function(html, cls){ line(html, cls); };
  CL.hist = function(){ return hist; };
  CL.status = function(html){ var last = els.hist.lastElementChild; if (last && last.dataset.status) last.innerHTML = html; else { line(html); els.hist.lastElementChild.dataset.status = '1'; } };
  function setIdle(){ prompt = null; els.name.textContent = ''; els.pr.innerHTML = ''; els.nest.textContent = ''; els.inp.placeholder = 'Gõ lệnh hoặc từ khóa tiếng Việt, ví dụ: đo, in, layer'; }
  function renderPrompt(){
    if (TC.dyn) TC.dyn.prompt(prompt);
    if (!prompt) { setIdle(); return; }
    els.nest.textContent = prompt.nested ? '>>' : '';
    els.name.textContent = prompt.cmd;
    els.pr.innerHTML = esc(prompt.text) + (prompt.opts && prompt.opts.length ? ' ' + prompt.opts.map(function(o){ return '<button class="chip" type="button" data-opt="' + esc(o[1]) + '">' + esc(o[0]) + ' <span class="k">·</span> <u>' + esc(o[1]) + '</u></button>'; }).join(' ') : '') + (prompt.def != null ? ' <span class="mono">&lt;' + esc(prompt.def) + '&gt;</span>' : '') + ':';
    els.inp.placeholder = '';
  }
  CL.busy = function(){ return !!running; };
  CL.current = function(){ return running ? running.name : null; };
  CL.prompt = function(){ return prompt; };
  CL.options = function(){ return prompt && prompt.opts ? prompt.opts.map(function(o){ return { label:o[0], key:o[1] }; }) : []; };
  CL.focus = function(){ els.inp.focus(); };
  CL.key = function(e){
    if (A0() === 'start') return;
    var k = e.key;
    if (k === 'Escape'){ e.preventDefault(); if (running) cmd.cancel(); else { TC.cad.clearSel(); TC.cad.reset(); } return; }
    if (k === 'Enter' || (k === ' ' && !els.inp.value)){ e.preventDefault(); CL.enter(); return; }
    if (k === 'ArrowUp' || k === 'ArrowDown'){ e.preventDefault(); els.inp.focus(); browse(k === 'ArrowUp' ? 1 : -1); return; }
    if (k.length === 1 || k === 'Backspace'){ els.inp.focus(); }
  };
  function A0(){ return TC.app.active; }
  CL.enter = function(){ var v = els.inp.value.trim(); if (!prompt && !v){ var l = cmd.last(); if (l) cmd.run(l); return; } CL.submit(v); };
  function onKey(e){
    var k = e.key;
    if (!els.ac.hidden && (k === 'ArrowDown' || k === 'ArrowUp')){ e.preventDefault(); var n = $$('.ai', els.ac).length; acSel = (acSel + (k === 'ArrowDown' ? 1 : -1) + n) % n; paintAC(); return; }
    if (!els.ac.hidden && (k === 'Tab' || k === 'Enter' || (k === ' ' && !prompt))){ var pick = $$('.ai', els.ac)[acSel]; if (pick){ e.preventDefault(); els.inp.value = ''; hideAC(); cmd.run(pick.dataset.n); return; } }
    if (prompt && TC.dyn && k === 'Tab' && TC.dyn.tab(els.inp)){ e.preventDefault(); return; }
    if (prompt && TC.dyn && k === 'ArrowDown' && TC.dyn.down()){ e.preventDefault(); return; }
    if (k === 'Enter' || (k === ' ' && !(prompt && prompt.allowSpace))){ e.preventDefault(); var v = els.inp.value.trim(); if (prompt && TC.dyn) v = TC.dyn.compose(v); els.inp.value = ''; hideAC(); if (!prompt && !v){ var l = cmd.last(); if (l) cmd.run(l); return; } CL.submit(v); return; }
    if (k === 'Escape'){ e.preventDefault(); els.inp.value = ''; hideAC(); if (running) cmd.cancel(); else { TC.cad.clearSel(); TC.cad.reset(); } els.inp.blur(); $('#cv').focus(); return; }
    if ((k === 'ArrowUp' || k === 'ArrowDown') && els.ac.hidden){ e.preventDefault(); browse(k === 'ArrowUp' ? 1 : -1); return; }
    if (k === 'F2'){ e.preventDefault(); TC.screens.textWindow(); }
  }
  function browse(dir){ var list = prompt ? entered : recentCmds; if (!list.length) return; histIdx = TC.clamp(histIdx + dir, -1, list.length - 1); els.inp.value = histIdx < 0 ? '' : list[histIdx]; }
  CL.submit = function(v){
    els.inp.value = ''; hideAC(); histIdx = -1;
    if (prompt){ var p = prompt; if (v) { entered.unshift(v); entered = entered.slice(0, 20); } line('<b>' + esc(p.cmd) + '</b> ' + esc(p.text) + ': <span class="mono">' + esc(v) + '</span>', 'muted'); p.take(v); return; }
    if (!v) return;
    cmd.run(v, null, true);
  };
  // tự hoàn thành: lệnh, viết tắt, biến hệ thống, từ khóa tiếng Việt, sắp theo tần suất dùng
  function matchCmds(q){
    var Q = q.toUpperCase(), F = TC.fold(q), out = [];
    Object.keys(CMDS).forEach(function(n){
      if (HIDDEN[n]) return;
      var c = CMDS[n], s = 0;
      if (n === Q || c.aliases.indexOf(Q) >= 0) s = 100; else if (n.indexOf(Q) === 0) s = 80; else if (c.aliases.some(function(a){ return a.indexOf(Q) === 0; })) s = 70; else if (n.indexOf(Q) > 0 && Q.length > 2) s = 40;
      else if (F.length > 1 && (c.vi.split(',').some(function(k){ return k.trim().indexOf(F) === 0; }) || TC.fold(c.desc).indexOf(F) >= 0)) s = 55;
      if (s) out.push({ c:c, s:s + Math.min(20, (usage[n] || 0) * 2) });
    });
    out.sort(function(a, b){ return b.s - a.s || a.c.name.localeCompare(b.c.name); });
    return out;
  }
  TC.matchCmds = matchCmds;
  function updateAC(){
    var q = els.inp.value.trim(); if (!q || /[,@<#]/.test(q) || /^[\d.\-]/.test(q)){ hideAC(); return; }
    var res = matchCmds(q.replace(/^'/, '')).slice(0, 9), sv = SYSVARS.filter(function(s){ return q.length > 1 && s[0].indexOf(q.toUpperCase()) === 0; }).slice(0, 3);
    if (!res.length && !sv.length){ hideAC(); return; }
    els.ac.innerHTML = res.map(function(r){ var c = r.c, lock = !TC.can(c.need); return '<button class="ai" type="button" role="option" data-n="' + c.name + '">' + ic(c.icon) + '<span class="n">' + c.name + '</span><span class="al">' + c.aliases.filter(function(a){ return a !== c.name; }).slice(0, 2).join(', ') + '</span><span class="d">' + esc(c.desc) + (lock ? ' <span class="ed xs" data-ed="' + (c.need === 'VP' ? 'vp' : 'pro') + '">' + (c.need === 'VP' ? 'Viewer Pro' : 'Pro') + '</span>' : '') + '</span></button>'; }).join('') +
      sv.map(function(s){ return '<button class="ai sysvar" type="button" role="option" data-n="' + s[0] + '" data-sv="1">' + ic('gear') + '<span class="n">' + s[0] + '</span><span class="al">biến</span><span class="d">' + esc(s[1]) + ' · hiện tại: ' + esc(s[2]) + '</span></button>'; }).join('') +
      '<div class="af">↑↓ chọn · Enter hoặc Tab để chạy · tìm cả theo tiếng Việt: “cắt”, “bo góc”, “in”</div>';
    acSel = 0; paintAC(); els.ac.hidden = false;
    $$('.ai', els.ac).forEach(function(b){ b.addEventListener('mousedown', function(e){ e.preventDefault(); els.inp.value = ''; hideAC(); cmd.run(b.dataset.n); }); });
  }
  function paintAC(){ $$('.ai', els.ac).forEach(function(b, i){ b.classList.toggle('is-hot', i === acSel); if (i === acSel) b.scrollIntoView({ block:'nearest' }); }); }
  function hideAC(){ if (els.ac) els.ac.hidden = true; }

  /* ---------- chạy lệnh ---------- */
  var running = null, cancelers = [];
  // Esc giữa chuỗi prompt lồng trong lệnh (sau hộp thoại…) là luồng bình thường, không phải lỗi
  addEventListener('unhandledrejection', function(e){ if (e.reason === 'cancel') e.preventDefault(); });
  cmd.cancel = function(){
    if (!running) return;
    var r = running; running = null; prompt = null;
    cancelers.forEach(function(f){ f(); }); cancelers = [];
    TC.cad.cancelPoint(); TC.cad.endMode(); renderPrompt(); line('<b>' + esc(r.name) + '</b> *Hủy*', 'muted');
    TC.app.view.badge = null;
  };
  function lev(a, b){ var m = []; for (var i = 0; i <= b.length; i++) m[i] = [i]; for (var j = 0; j <= a.length; j++) m[0][j] = j; for (i = 1; i <= b.length; i++) for (j = 1; j <= a.length; j++) m[i][j] = b[i - 1] === a[j - 1] ? m[i - 1][j - 1] : Math.min(m[i - 1][j - 1] + 1, m[i][j - 1] + 1, m[i - 1][j] + 1); return m[b.length][a.length]; }
  cmd.run = function(input, preset, typed){
    if (TC.app.active === 'start' && !/^(OPEN|NEWTAB|OPTIONS|HELP|ABOUT|GRAPHICSCONFIG|TRIONBENCH|TCREPORT|QUIT|PLOT)$/i.test(String(input).replace(/^[_.'-]+/, ''))) { if (TC.app.docs.some(function(d){ return d.open; })) TC.app.activate(TC.app.docs.filter(function(d){ return d.open; })[0].id); else return; }
    var raw = String(input).trim(), transparent = raw[0] === "'";
    var c = cmd.def(raw);
    if (!c){
      // gõ thẳng tên biến hệ thống: bản Pro hỏi giá trị mới (SETVAR), Viewer chỉ xem (UI-K13)
      var svn = raw.toUpperCase(), sdf = TC.SV && TC.SV.def(svn);
      if (sdf && TC.can('PRO')) return cmd.run('SETVAR', svn);
      if (sdf){ line('<b>' + svn + '</b> = <span class="mono">' + esc(TC.SV.fmt(svn)) + '</span> · ' + esc(sdf.desc) + ' (chỉ xem trong Viewer)'); return; }
      var sv = SYSVARS.filter(function(s){ return s[0] === raw.toUpperCase(); })[0];
      if (sv){ line('<b>' + sv[0] + '</b> = <span class="mono">' + esc(sv[2]) + '</span> · ' + esc(sv[1]) + (TC.isViewer() ? ' (chỉ xem trong Viewer)' : '')); return; }
      var viHits = matchCmds(raw).filter(function(m){ return m.s >= 55; }).slice(0, 4);
      var sug = Object.keys(CMDS).concat(Object.keys(ALIAS)).filter(function(n){ return Math.abs(n.length - raw.length) <= 2 && lev(raw.toUpperCase(), n) <= (raw.length > 4 ? 2 : 1); }).map(function(n){ return CMDS[n] ? n : ALIAS[n]; });
      var pool = viHits.map(function(m){ return m.c.name; }).concat(sug).filter(function(n, i, a){ return a.indexOf(n) === i; }).slice(0, 4);
      line('<span class="err">Không có lệnh “' + esc(raw) + '”.</span>' + (pool.length ? ' Ý bạn là: ' + pool.map(function(n){ return '<button class="chip" type="button" data-run="' + n + '">' + n + '</button>'; }).join(' ') + '?' : ' Gõ ? hoặc mở Trợ giúp (F1) để xem danh sách lệnh.'));
      return;
    }
    if (running && !transparent) cmd.cancel();
    if (!TC.can(c.need)){
      var ed = c.need === 'VP' ? 'TrionCAD Viewer Pro' : 'TrionCAD Pro';
      line('Lệnh <b>' + c.name + '</b> có trong ' + ed + '. <button class="chip" type="button" data-trialfor="' + esc(c.desc) + '">Dùng thử 30 ngày</button> <button class="chip" type="button" data-trialfor="' + esc(c.desc) + '">Tìm hiểu</button>');
      if (!typed) TC.screens.upgrade(c.desc, c.need);
      return;
    }
    if (!HIDDEN[c.name]){ usage[c.name] = (usage[c.name] || 0) + 1; TC.save('usage', usage); recentCmds = [c.name].concat(recentCmds.filter(function(n){ return n !== c.name; })).slice(0, 12); }
    var impl = IMPL[c.name];
    if (!impl){
      line('<b>' + c.name + '</b> · ' + esc(c.desc) + ': màn hình này thiết kế ở Đợt ' + c.wave + '.', 'faint');
      TC.toast('<b>' + esc(c.desc) + '</b> (' + c.name + ') được thiết kế ở Đợt ' + c.wave + '. Xem danh mục màn hình trong nút Prototype.', { type:'info', log:false });
      return;
    }
    if (transparent && running){
      var outer = prompt; line('<b>' + c.name + '</b> (lệnh trong suốt)', 'muted');
      var nested = { name:c.name };
      return Promise.resolve(impl(ctxFor(nested, true), preset)).then(function(){ prompt = outer; renderPrompt(); TC.cad.move(TC.app.view.mouse || [0, 0], {}); });
    }
    TC.cad.reset();
    if (TC.app.view) TC.app.view.cursor = 'idle';
    var job = { name:c.name }; running = job;
    if (!HIDDEN[c.name]) line('Lệnh: <b>' + c.name + '</b>');
    if (preset == null && /\s/.test(raw)) preset = raw.split(/\s+/)[1];
    Promise.resolve(impl(ctxFor(job), preset)).catch(function(err){ if (err !== 'cancel') console.error(err); }).then(function(){ if (running === job){ running = null; prompt = null; renderPrompt(); TC.app.view.badge = null; } TC.emit('cmdend', c.name); });
  };
  function ctxFor(job, nested){
    function alive(){ return nested || running === job; }
    function ask(text, o){
      o = o || {};
      return new Promise(function(res, rej){
        if (!alive()) return rej('cancel');
        prompt = { cmd:job.name, text:text, opts:o.opts, def:o.def, nested:nested, allowSpace:o.allowSpace, take:function(v){ prompt = null; renderPrompt(); res({ raw:v }); } };
        cancelers.push(function(){ rej('cancel'); });
        renderPrompt();
      });
    }
    var ctx = {
      out:function(html, cls){ line(html, cls); },
      point:function(text, o){
        o = o || {};
        return new Promise(function(res, rej){
          if (!alive()) return rej('cancel');
          var done = false, fin = function(v){ if (done) return; done = true; prompt = null; renderPrompt(); res(v); };
          prompt = { cmd:job.name, text:text, opts:o.opts, def:o.def, nested:nested, num:!!o.num, dyn:o.dyn, take:function(v){
            if (!v){ TC.cad.cancelPoint(); fin({ enter:true }); return; }
            if (o.num && /^-?[\d.]+$/.test(v)){ TC.cad.cancelPoint(); fin({ num:Number(v) }); return; }
            var kw = (o.opts || []).concat(o.hidden || []).filter(function(op){ return op[1].toUpperCase() === v.toUpperCase() || TC.fold(op[0]).indexOf(TC.fold(v)) === 0; })[0];
            if (kw){ TC.cad.cancelPoint(); fin({ kw:kw[1] }); return; }
            var p = parsePoint(v, o.base);
            if (p){ TC.cad.feedPoint(p); return; }
            line('<span class="err">Cần một điểm hoặc một tùy chọn.</span> Nhập x,y hoặc @dx,dy hoặc @khoảng-cách&lt;góc, hoặc bấm trên bản vẽ.');
            renderPrompt();
          } };
          cancelers.push(function(){ if (!done){ done = true; rej('cancel'); } });
          renderPrompt();
          TC.cad.getPoint({ base:o.base, temp:o.temp, onMove:o.onMove, rubber:o.rubber, pick:o.pick, nosnap:o.nosnap }).then(function(r){ if (done) return; if (!r) { if (!alive()) return; return; } if (!o.pick) line('<b>' + esc(job.name) + '</b> ' + esc(text) + ': <span class="mono">' + TC.d(r.p[0]) + ',' + TC.d(r.p[1]) + '</span>', 'muted'); fin({ p:r.p, s:r.s, shift:r.shift }); });
        });
      },
      sel:function(text){
        var d = TC.app.doc();
        if (d.sel.length) return Promise.resolve(d.sel.slice());
        return new Promise(function(res, rej){
          if (!alive()) return rej('cancel');
          prompt = { cmd:job.name, text:text || 'Chọn đối tượng', opts:TC.can('PRO') ? [['Tất cả','ALL'],['Cuối','L'],['Trước đó','P']] : [['Tất cả','ALL']], nested:nested, take:function(v){ var k = (v || '').toUpperCase(); if (k === 'ALL') TC.cad.selectAll(); else if (k === 'L' && d.lastEnt && d.byId[d.lastEnt]){ d.sel.push(d.lastEnt); TC.cad.afterSel(); renderPrompt(); return; } else if (k === 'P' && d.prevSel){ d.prevSel.forEach(function(id){ if (d.byId[id] && d.sel.indexOf(id) < 0) d.sel.push(id); }); TC.cad.afterSel(); renderPrompt(); return; } TC.cad.endSel(); } };
          cancelers.push(function(){ rej('cancel'); });
          renderPrompt();
          TC.cad.getSel({ onPick:function(){ TC.cmdline.status('Đã chọn ' + TC.n(d.sel.length) + ' · Enter để kết thúc chọn'); } }).then(function(ids){ prompt = null; renderPrompt(); if (ids && ids.length) d.prevSel = ids.slice(); res(ids || []); });
        });
      },
      kw:function(text, opts, def){ return ask(text, { opts:opts, def:def }).then(function(r){ var v = r.raw || def; var k = opts.filter(function(o){ return o[1].toUpperCase() === String(v).toUpperCase() || TC.fold(o[0]).indexOf(TC.fold(v)) === 0; })[0]; return k ? k[1] : v; }); },
      val:function(text, def, o){ return ask(text, Object.assign({ def:def }, o || {})).then(function(r){ return r.raw === '' ? def : r.raw; }); },
      badge:function(b){ TC.app.view.badge = b; }
    };
    return ctx;
  }
  function parsePoint(v, base){
    v = v.replace(/\s+/g, '');
    var m;
    if ((m = v.match(/^@(-?[\d.]+)<(-?[\d.]+)$/)) && base){ var a = Number(m[2]) * Math.PI / 180; return [base[0] + Number(m[1]) * Math.cos(a), base[1] + Number(m[1]) * Math.sin(a)]; }
    if ((m = v.match(/^@(-?[\d.]+),(-?[\d.]+)$/)) && base) return [base[0] + Number(m[1]), base[1] + Number(m[2])];
    if ((m = v.match(/^#?(-?[\d.]+),(-?[\d.]+)$/))) return [Number(m[1]), Number(m[2])];
    if ((m = v.match(/^(-?[\d.]+)$/)) && base){ var cur = TC.cad.curPoint(); if (!cur) return null; var ang = Math.atan2(cur[1] - base[1], cur[0] - base[0]); return [base[0] + Number(m[1]) * Math.cos(ang), base[1] + Number(m[1]) * Math.sin(ang)]; }
    return null;
  }
  TC.parsePoint = parsePoint;

  /* ================= lệnh của Viewer ================= */
  function view(){ return TC.app.view; } function doc(){ return TC.app.doc(); }
  function fmtLen(mm){ return TC.d(mm) + ' mm (' + TC.d(mm / 1000, 3) + ' m)'; }
  IMPL.ZOOM = function(ctx, pre){
    var go = function(k){
      var v = view(); k = String(k || '').toUpperCase();
      if (k === 'E' || k === 'A'){ v.zoomExtents(); return; }
      if (k === 'P'){ if (!v.zoomPrev()) ctx.out('Không còn view trước.'); return; }
      if (k === 'W'){ ctx.out('Chỉ định hai góc cửa sổ.'); return TC.cad.zwin(); }
      if (k === 'O'){ return ctx.sel('Chọn đối tượng để phóng tới').then(function(ids){ if (!ids.length) return; var d = doc(), bb = [Infinity, Infinity, -Infinity, -Infinity]; ids.forEach(function(id){ var b = d.byId[id]._bb; bb = [Math.min(bb[0], b[0]), Math.min(bb[1], b[1]), Math.max(bb[2], b[2]), Math.max(bb[3], b[3])]; }); v.zoomTo(bb); }); }
      if (k === 'IN' || k === 'OUT'){ v.zoomAt(v.W / 2, v.H / 2, k === 'IN' ? 2 : .5); return; }
      var m = k.match(/^([\d.]+)X$/); if (m){ v.zoomAt(v.W / 2, v.H / 2, Number(m[1])); return; }
      if (k === '' || k === 'R'){ ctx.out('Lăn chuột hoặc kéo chuột giữa để zoom và pan. Esc hoặc Enter để kết thúc.'); return TC.cad.pan(); }
    };
    if (pre) return go(pre);
    return ctx.kw('Chỉ định góc cửa sổ, nhập hệ số (nX) hoặc', [['Tất cả','A'],['Toàn bộ','E'],['Trước đó','P'],['Cửa sổ','W'],['Đối tượng','O']], 'thời gian thực').then(go);
  };
  IMPL.PAN = function(ctx){ ctx.out('Kéo để di chuyển màn hình. Esc hoặc Enter để kết thúc.'); ctx.badge('P'); return Promise.race([TC.cad.pan(), ctx.val('Kéo chuột để pan', null).then(function(){ TC.cad.endMode(); })]); };
  IMPL.REGEN = function(ctx){ var t = performance.now(); view().draw(); ctx.out('Tạo lại hình xong: ' + TC.n(doc().ents.length) + ' đối tượng trong ' + Math.max(1, Math.round(performance.now() - t)) + ' ms.'); };
  IMPL.REGENALL = IMPL.REGEN;
  IMPL.SELECTALL = function(){ TC.cad.selectAll(); };
  IMPL.SELECT = function(ctx){ TC.cad.clearSel(); return ctx.sel('Chọn đối tượng').then(function(ids){ TC.app.doc().sel = ids; TC.cad.afterSel(); }); };
  IMPL.CLEANSCREENON = function(){ TC.app.cleanScreen(); };
  IMPL.NEWTAB = function(){ TC.app.activate('start'); };
  IMPL.PROPERTIES = function(){ TC.app.showPalette('props', true); };
  IMPL.LAYER = function(){ TC.app.showPalette('layers'); };
  IMPL.FIXFONT = function(){ TC.app.showPalette('fontdoc'); };
  IMPL.EXTERNALREFERENCES = function(){ TC.app.showPalette('xref'); };
  IMPL.TEXTSCR = function(){ TC.screens.textWindow(); };
  IMPL.VIEW = function(ctx){ var d = doc(); if (!d.views.length){ ctx.out('Bản vẽ chưa có view đã đặt tên.'); return; } return ctx.kw('Chọn view để khôi phục', d.views.map(function(v, i){ return [v.name, String(i + 1)]; }), '1').then(function(k){ var v = d.views[Number(k) - 1]; if (v){ if (d.space !== 'model') TC.app.setSpace('model'); var vv = view(); vv.pushHist(); d.view = { cx:v.c[0], cy:v.c[1], s:vv.H / v.h }; vv.draw(); TC.emit('view'); ctx.out('Đã khôi phục view “' + esc(v.name) + '”.'); } }); };

  IMPL.ID = function(ctx){ ctx.badge('ID'); return ctx.point('Chỉ định điểm').then(function(r){ if (!r || !r.p) return; var p = r.p; ctx.out('X = <b class="mono">' + TC.d(p[0]) + '</b>   Y = <b class="mono">' + TC.d(p[1]) + '</b>   Z = <b class="mono">0.00</b>'); TC.cad.pin('X ' + TC.d(p[0]) + ' · Y ' + TC.d(p[1]), p); }); };
  // Đo khoảng cách: đường tạm, nhãn trên vùng vẽ và thẻ kết quả (UI-I02)
  IMPL.DIST = function(ctx){
    ctx.badge('DI');
    return ctx.point('Chỉ định điểm đầu').then(function(a){
      if (!a || !a.p) return;
      var pts = [a.p], multi = false;
      var step = function(){
        var base = pts[pts.length - 1];
        return ctx.point(multi ? 'Chỉ định điểm kế tiếp hoặc' : 'Chỉ định điểm thứ hai hoặc', { base:base, opts:multi ? [['Hoàn tác','U'],['Tổng','T']] : [['Nhiều điểm','M']],
          temp:pts.length > 1 ? [{ k:'poly', pts:pts.slice(), color:TC.cssv('--c-accent'), w:1.5 }] : [],
          onMove:function(p){ TC.cad.clearLabels(); var s = view().mToS([(base[0] + p[0]) / 2, (base[1] + p[1]) / 2]); if (s) TC.cad.label(TC.d(G.dist(base, p)) + (multi ? ' · tổng ' + TC.d(G.polyLen(pts.concat([p]))) : ''), s[0], s[1] - 14); } })
        .then(function(r){
          if (!r) return;
          if (r.kw === 'M'){ multi = true; return step(); }
          if (r.kw === 'U' && pts.length > 1){ pts.pop(); return step(); }
          if (r.p){ pts.push(r.p); if (multi) return step(); return show(); }
          if (r.enter || r.kw === 'T') return pts.length > 1 ? show() : null;
        });
      };
      var show = function(){
        TC.cad.clearLabels();
        var a1 = pts[0], b1 = pts[pts.length - 1], L = G.polyLen(pts), dx = b1[0] - a1[0], dy = b1[1] - a1[1], ang = (G.ang(a1, b1) + 360) % 360;
        view().temp = [{ k:'poly', pts:pts, color:TC.cssv('--c-accent'), w:1.6 }].concat(pts.map(function(p){ return { k:'pt', p:p }; })); view().drawOver();
        TC.cad.pin(TC.d(L), [(a1[0] + b1[0]) / 2, (a1[1] + b1[1]) / 2]);
        ctx.out((pts.length > 2 ? 'Tổng chiều dài' : 'Khoảng cách') + ' = <b class="mono">' + TC.d(L) + '</b>' + (pts.length > 2 ? ' qua ' + (pts.length - 1) + ' đoạn' : ', Góc trong mặt XY = <span class="mono">' + TC.d(ang, 0) + '°</span>, Delta X = <span class="mono">' + TC.d(dx) + '</span>, Delta Y = <span class="mono">' + TC.d(dy) + '</span>'));
        resultCard([[pts.length > 2 ? 'Tổng chiều dài' : 'Khoảng cách', L, 'len'], pts.length > 2 ? null : ['Góc', ang, 'deg'], pts.length > 2 ? null : ['Delta X', dx, 'len'], pts.length > 2 ? null : ['Delta Y', dy, 'len']].filter(Boolean), b1);
      };
      return step();
    });
  };
  function resultCard(rows, wp){
    var unit = 'mm';
    var render = function(){
      var html = '<div class="hrow" style="justify-content:space-between"><b style="font-weight:600">Kết quả đo</b><span class="seg" role="group" aria-label="Đơn vị"><button type="button" data-u="mm" aria-pressed="' + (unit === 'mm') + '">mm</button><button type="button" data-u="m" aria-pressed="' + (unit === 'm') + '">m</button></span></div>' +
        rows.map(function(r){ var v = r[2] === 'len' ? (unit === 'm' ? TC.d(r[1] / 1000, 3) + ' m' : TC.d(r[1]) + ' mm') : r[2] === 'area' ? (unit === 'm' ? TC.d(r[1] / 1e6, 2) + ' m²' : TC.d(r[1], 0) + ' mm²') : TC.d(r[1], 0) + '°'; return '<div class="mrow"><span class="muted">' + r[0] + '</span><b>' + v + '</b></div>'; }).join('') +
        '<div class="acts"><button class="btn sm" type="button" data-copy>' + ic('copyclip') + 'Sao chép</button><button class="btn sm" type="button" data-totext>Chèn thành text' + (TC.can('PRO') ? '' : ' <span class="ed xs" data-ed="pro">Pro</span>') + '</button><button class="ib sm" type="button" data-x aria-label="Đóng">' + ic('x') + '</button></div>';
      card.innerHTML = html;
    };
    var card = TC.cad.card('', wp); render();
    card.addEventListener('click', function(e){
      var u = e.target.closest('[data-u]'); if (u){ unit = u.dataset.u; render(); return; }
      if (e.target.closest('[data-copy]')){ var r = rows[0]; TC.copyText(r[2] === 'area' ? (unit === 'm' ? TC.d(r[1] / 1e6, 2) : TC.d(r[1], 0)) : r[2] === 'len' ? (unit === 'm' ? TC.d(r[1] / 1000, 3) : TC.d(r[1])) : TC.d(r[1], 0)); }
      if (e.target.closest('[data-totext]')){ if (!TC.can('PRO')) TC.screens.upgrade('Chèn kết quả đo thành text', 'PRO'); }
      if (e.target.closest('[data-x]')) TC.cad.reset();
    });
  }
  TC.resultCard = resultCard;
  IMPL.AREA = function(ctx, pre){
    ctx.badge('AA');
    var total = 0, mode = 'add';
    var fromPts = function(){
      var pts = [];
      var step = function(){
        return ctx.point(pts.length ? 'Chỉ định điểm kế tiếp hoặc' : 'Chỉ định điểm góc đầu tiên hoặc', { base:pts[pts.length - 1], opts:pts.length ? [['Hoàn tác','U'],['Xong','T']] : [['Đối tượng','O'],['Cộng diện tích','A'],['Trừ diện tích','S']],
          temp:[],
          onMove:function(p, temp){ if (pts.length > 1) temp.push({ k:'poly', pts:pts.concat([p]), closed:true, fill:TC.cssv('--c-sel-wf'), color:TC.cssv('--c-sel-w'), dash:[5, 4] }); TC.cad.clearLabels(); if (pts.length > 1){ var a = G.polyArea(pts.concat([p])); var s = view().mToS(p); if (s) TC.cad.label('Diện tích ' + TC.d(a / 1e6) + ' m²', s[0] + 50, s[1] + 24, 'area'); } } })
        .then(function(r){
          if (!r) return;
          if (r.kw === 'O') return fromObj();
          if (r.kw === 'A' || r.kw === 'S'){ mode = r.kw === 'A' ? 'add' : 'sub'; ctx.out('Chế độ ' + (mode === 'add' ? 'cộng' : 'trừ') + ' diện tích.'); return step(); }
          if (r.kw === 'U'){ pts.pop(); return step(); }
          if (r.p){ pts.push(r.p); return step(); }
          if ((r.enter || r.kw === 'T') && pts.length > 2) return report(pts);
          if (r.enter) return;
        });
      };
      return step();
    };
    var report = function(pts){
      TC.cad.clearLabels();
      var a = G.polyArea(pts), per = G.polyLen(pts, true); total += mode === 'sub' ? -a : a;
      view().temp = [{ k:'poly', pts:pts, closed:true, fill:TC.cssv('--c-sel-wf'), color:TC.cssv('--c-sel-w'), w:1.6 }]; view().drawOver();
      var c = pts.reduce(function(s, q){ return [s[0] + q[0] / pts.length, s[1] + q[1] / pts.length]; }, [0, 0]);
      TC.cad.pin(TC.d(a / 1e6) + ' m²', c, 'area');
      ctx.out('Diện tích = <b class="mono">' + TC.d(a, 0) + '</b> mm² (<b>' + TC.d(a / 1e6) + ' m²</b>), Chu vi = <b class="mono">' + TC.d(per) + '</b> mm' + (total !== a ? ' · Tổng = ' + TC.d(total / 1e6) + ' m²' : ''));
      resultCard([['Diện tích', a, 'area'], ['Chu vi', per, 'len']].concat(total !== a ? [['Tổng cộng dồn', total, 'area']] : []), c);
    };
    var fromObj = function(){
      return ctx.sel('Chọn đối tượng kín (polyline, hatch, đường tròn)').then(function(ids){
        var d = doc(), e = ids.map(function(id){ return d.byId[id]; }).filter(function(x){ return x && ((x.t === 'pline' && x.closed) || x.t === 'hatch' || x.t === 'circle'); })[0];
        TC.cad.clearSel();
        if (!e){ ctx.out('<span class="err">Đối tượng không kín.</span> Chọn polyline kín, hatch hoặc đường tròn.'); return; }
        if (e.t === 'circle'){ var A = Math.PI * e.r * e.r; ctx.out('Diện tích = <b>' + TC.d(A / 1e6) + ' m²</b>, Chu vi = <b class="mono">' + TC.d(2 * Math.PI * e.r) + '</b>'); return; }
        var pts = e.t === 'pline' ? e.pts : e.loops[0]; report(pts);
      });
    };
    if (pre === 'O') return fromObj();
    return fromPts();
  };
  IMPL.MEASUREGEOM = function(ctx, pre){
    var go = function(k){
      k = String(k || 'Q').toUpperCase();
      if (k === 'D') return IMPL.DIST(ctx);
      if (k === 'AR') return IMPL.AREA(ctx);
      if (k === 'R'){ return ctx.sel('Chọn cung hoặc đường tròn').then(function(ids){ var d = doc(), e = ids.map(function(i){ return d.byId[i]; }).filter(function(x){ return x && (x.t === 'circle' || x.t === 'arc' || (x.t === 'insert' && x._p.some(function(p){ return p.k === 'arc' || p.k === 'circ'; }))); })[0]; TC.cad.clearSel(); if (!e) { ctx.out('<span class="err">Không phải cung hoặc đường tròn.</span>'); return; } var pr = e._p.filter(function(p){ return p.k === 'arc' || p.k === 'circ'; })[0]; ctx.out('Bán kính = <b class="mono">' + TC.d(pr.r) + '</b>, Đường kính = <b class="mono">' + TC.d(pr.r * 2) + '</b>'); TC.cad.pin('R ' + TC.d(pr.r), pr.c); }); }
      if (k === 'A'){ return ctx.sel('Chọn hai đoạn thẳng').then(function(ids){ var d = doc(), segs = []; ids.forEach(function(i){ d.byId[i]._p.forEach(function(p){ if (p.k === 'seg' && segs.length < 2) segs.push(p); }); }); TC.cad.clearSel(); if (segs.length < 2){ ctx.out('<span class="err">Cần hai đoạn thẳng.</span>'); return; } var a1 = G.ang(segs[0].a, segs[0].b), a2 = G.ang(segs[1].a, segs[1].b), an = Math.abs(((a1 - a2) % 180 + 180) % 180); ctx.out('Góc = <b class="mono">' + TC.d(Math.min(an, 180 - an) === an ? an : an, 0) + '°</b>'); }); }
      if (k === 'X') return;
      ctx.out('Đo nhanh: rê chuột để thấy khoảng cách quanh con trỏ; bấm trong vùng kín để đo diện tích; Shift + bấm để cộng dồn. Esc để thoát.');
      ctx.badge('Q');
      return Promise.race([TC.cad.quick(), ctx.kw('Rê chuột để đo hoặc', [['Khoảng cách','D'],['Bán kính','R'],['Góc','A'],['Diện tích','AR'],['Thoát','X']], 'Thoát').then(function(k2){ TC.cad.quickEnd(); return k2 && k2 !== 'X' && k2 !== 'Thoát' ? go(k2) : null; })]).then(function(){ TC.cad.quickEnd(); });
    };
    if (pre) return go(pre);
    return ctx.kw('Nhập tùy chọn', [['Khoảng cách','D'],['Bán kính','R'],['Góc','A'],['Diện tích','AR'],['Nhanh','Q'],['Thoát','X']], 'Nhanh').then(go);
  };
  IMPL.LIST = function(ctx){
    return ctx.sel('Chọn đối tượng').then(function(ids){
      if (!ids.length) return; var d = doc();
      TC.screens.textWindow(ids.map(function(id){ return TC.describe(d, d.byId[id]); }).join('\n\n'), 'LIST · ' + ids.length + ' đối tượng');
      ctx.out('Đã liệt kê ' + ids.length + ' đối tượng trong cửa sổ văn bản (F2).');
    });
  };
  // bật/tắt layer tạm thời: Viewer không lưu vào tệp (UI-M04)
  function tempLayer(ctx, fn, verb){ return ctx.sel('Chọn đối tượng trên layer cần ' + verb).then(function(ids){ var d = doc(), names = []; ids.forEach(function(id){ var e = d.byId[id]; if (e && names.indexOf(e.layer) < 0) names.push(e.layer); }); TC.cad.clearSel(); names.forEach(function(n){ fn(d, n); }); view().draw(); TC.app.refreshPalettes(); if (names.length) ctx.out('Layer ' + names.map(function(n){ return '<b>' + esc(n) + '</b>'; }).join(', ') + ' đã ' + verb + (TC.isViewer() ? ' (tạm thời, không lưu vào tệp)' : '') + '.'); }); }
  IMPL.LAYOFF = function(ctx){ return tempLayer(ctx, function(d, n){ TC.layers.set(d, n, 'on', false); }, 'tắt'); };
  IMPL.LAYFRZ = function(ctx){ return tempLayer(ctx, function(d, n){ TC.layers.set(d, n, 'frz', true); }, 'đóng băng'); };
  IMPL.LAYISO = function(ctx){ return ctx.sel('Chọn đối tượng trên các layer cần cô lập').then(function(ids){ var d = doc(), keep = {}; ids.forEach(function(id){ keep[d.byId[id].layer] = 1; }); TC.cad.clearSel(); TC.layers.isolate(d, Object.keys(keep)); ctx.out('Đã cô lập ' + Object.keys(keep).length + ' layer: ' + Object.keys(keep).join(', ') + '. Gõ LAYUNISO để bỏ cô lập.'); }); };
  IMPL.LAYUNISO = function(ctx){ TC.layers.endIso(); ctx.out('Đã bỏ cô lập layer.'); };
  IMPL.LAYON = function(ctx){ var d = doc(); d.layers.forEach(function(l){ TC.layers.set(d, l.name, 'on', true); }); view().draw(); TC.app.refreshPalettes(); ctx.out('Đã bật tất cả layer.'); };
  IMPL.LAYTHW = function(ctx){ var d = doc(); d.layers.forEach(function(l){ TC.layers.set(d, l.name, 'frz', false); }); view().draw(); TC.app.refreshPalettes(); ctx.out('Đã rã đông tất cả layer.'); };

  IMPL.OPEN = function(){ TC.files.open(); };
  IMPL.CLOSE = function(){ var d = doc(); if (d) TC.app.closeDoc(d.id); };
  IMPL.QUIT = function(){ TC.confirm({ title:'Thoát TrionCAD', msg:'Đóng TrionCAD và mọi bản vẽ đang mở?', ok:'Thoát' }).then(function(y){ if (y) TC.toast('Prototype: ứng dụng thật sẽ đóng ở bước này.', { type:'info', log:false }); }); };
  IMPL.RECOVER = function(){ TC.files.recover(); };
  IMPL.PLOT = function(){ TC.screens.plot(); };
  IMPL.PAGESETUP = function(){ TC.screens.pageSetup(); };
  IMPL.PREVIEW = function(){ TC.screens.preview(); };
  IMPL.EXPORTPDF = function(ctx, pre){ TC.screens.exportPdf(pre); };
  IMPL.PNGOUT = function(){ TC.screens.exportImage(); };
  IMPL.FIND = function(){ TC.screens.find(); };
  IMPL.OPTIONS = function(){ TC.screens.options(); };
  IMPL.GRAPHICSCONFIG = function(){ TC.screens.graphics(); };
  IMPL.TRIONBENCH = function(){ TC.screens.bench(); };
  IMPL.TCREPORT = function(){ TC.screens.sendDrawing(); };
  IMPL.HELP = function(){ TC.openHelp(); };
  IMPL.ABOUT = function(){ TC.screens.about(); };
  IMPL.INPUTSEARCHOPTIONS = function(){ TC.screens.options('cmdline'); };
})();
