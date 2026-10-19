# 03 — Kiểm kê chức năng AutoCAD và ma trận ưu tiên cho TrionCAD

> Mục đích: liệt kê **toàn bộ nhóm chức năng của AutoCAD 2027**, kèm lệnh tiêu biểu, để làm "bản đồ tương thích" cho TrionCAD.
> - TrionCAD **tự xây toàn bộ lõi** trên nền mã nguồn mở có giấy phép thoáng ([05](05-kien-truc-ky-thuat.md)), nên bảng này là **backlog chính** của sản phẩm.
> - Cột "Ghi chú" nêu thư viện mã nguồn mở dùng lại được (nếu có). Không ghi gì nghĩa là tự viết.
>
> AutoCAD có **hơn 1.000 lệnh** (tính cả biến thể dòng lệnh như `-LAYER`, `-INSERT`) và **hàng trăm biến hệ thống** (sysvar) **[ước tính, đối chiếu Command Reference chính thức ở Giai đoạn 0]**. Mục tiêu của TrionCAD **không phải** sao chép tất cả. Mục tiêu là phủ khoảng 95% thao tác hằng ngày của người dùng 2D, chạy được script và LISP hiện có, và mở/lưu DWG chính xác.

## Quy ước

**Mức ưu tiên**

| Mức | Ý nghĩa | Mốc phát hành |
|---|---|---|
| **P0** | Bắt buộc để làm một bộ bản vẽ 2D thực tế từ đầu đến khi in (phần hiển thị và in đã có từ Viewer, T16–17) | Pro Early Access (khoảng T25), nghiệm thu MVP |
| **P1** | Ngang AutoCAD LT cho công việc 2D | v1.0 (T36 ±4) |
| **P2** | Ngang AutoCAD đầy đủ ở 2D, cộng tùy biến và tự động hóa | v1.x–v2 |
| **P3** | 3D, trực quan hóa, đám mây và cộng tác, AI | v2–v3 |
| **P4** | Tương lai, chuyên ngành, hoặc làm qua đối tác | Theo nhu cầu |
| **✕** | Không làm (Autodesk đã bỏ, lỗi thời, hoặc giá trị thấp) | — |

**Cột LT:** ✓ là có trong AutoCAD LT, ✗ là không có, ~ là có giới hạn, ? là cần xác minh. Cột này chỉ để định hướng; đối chiếu với trang so sánh chính thức của Autodesk ở Giai đoạn 0.

**Nguyên tắc tương thích**
- **Bắt buộc giữ nguyên** những gì mà kịch bản `.scr`, LISP `(command ...)` và thói quen bàn phím phụ thuộc vào: tên lệnh, tên toàn cục dạng `_LINE`, tiền tố `-` cho phiên bản dòng lệnh, `.` để gọi lệnh gốc, `'` cho lệnh trong suốt, từ khóa tùy chọn và thứ tự prompt.
- **Không sao chép** biểu tượng, văn bản trợ giúp hay giao diện đồ họa đặc trưng của Autodesk. Tự thiết kế những phần này.

---

## 1. Giao diện và môi trường làm việc

| Chức năng | Lệnh / thành phần AutoCAD | LT | Ưu tiên | Ghi chú |
|---|---|---|---|---|
| Dòng lệnh: tự hoàn thành, lịch sử, gợi ý tùy chọn | Command line, `INPUTSEARCHOPTIONS` | ✓ | **P0** | Trái tim của CAD kiểu AutoCAD. Gõ được tiếng Việt qua Unikey/EVKey |
| Tự sửa lỗi gõ, từ đồng nghĩa lệnh | AutoCorrect, Synonyms | ✓ | P1 | |
| Nhập động tại con trỏ | `DYN`, `DSETTINGS` | ✓ | **P0** | |
| Ribbon (các tab Home, Insert, Annotate, View, Manage, Output) | `RIBBON` | ✓ | **P0** bản cơ bản; P1 đầy đủ, có tab ngữ cảnh | |
| Menu và toolbar cổ điển | `MENUBAR`, `TOOLBAR` | ✓ | P1 | Nhiều người dùng VN vẫn quen giao diện cổ điển |
| Thanh trạng thái (Grid, Snap, Ortho, Polar, Osnap, Otrack, LWT, Transparency…) | Status bar | ✓ | **P0** | |
| Bảng thuộc tính, thuộc tính nhanh | `PROPERTIES` (Ctrl+1), `QUICKPROPERTIES` | ✓ | **P0** / P1 | |
| Tab tệp, tab Layout, trang Start | File tabs, Layout tabs, Start tab | ✓ | **P0** / **P0** / P1 | |
| Tool Palettes, DesignCenter, Blocks palette | `TOOLPALETTES`, `ADCENTER`, `BLOCKSPALETTE` | ✓ | P1 | |
| Workspace, profile | `WORKSPACE`, `WSSAVE`, `OPTIONS` > Profiles | ✓ | P1 | |
| Hộp thoại tùy chọn | `OPTIONS` | ✓ | **P0** cơ bản; P1 đầy đủ | |
| Viết tắt lệnh (`acad.pgp`) | `ALIASEDIT`, `REINIT` | ✓ | **P0** | Đọc được tệp `.pgp` có sẵn của người dùng |
| Chuột: lăn để zoom, nút giữa để pan, menu chuột phải, nhấp đúp để sửa | `DBLCLKEDIT`, `SHORTCUTMENU` | ✓ | **P0** | |
| Chủ đề sáng/tối, màn hình High-DPI | `COLORTHEME` | ✓ | **P0** | |
| Clean screen | `CLEANSCREENON` | ✓ | P1 | |
| Cửa sổ bản vẽ nổi, nhiều màn hình | Floating windows (2022) | ✓ | P2 | |
| Xem trước kết quả lệnh, xem trước vùng chọn | `COMMANDPREVIEW`, `SELECTIONPREVIEW` | ✓ | P1 | |
| Navigation bar, ViewCube, SteeringWheels | `NAVBAR`, `NAVVCUBE`, `NAVSWHEEL` | ~ | P2 / P1 (xem 3D) / P4 | |
| Trợ giúp, My Insights (gợi ý macro) | `HELP`, My Insights | ✓ | P1 / P4 | Trợ giúp tiếng Việt có video |
| Ngôn ngữ giao diện | Bản địa hóa | ✓ | **P0** | Tiếng Việt và tiếng Anh, chuyển đổi không cần cài lại |

## 2. Quản lý tệp và bản vẽ

| Chức năng | Lệnh | LT | Ưu tiên | Ghi chú |
|---|---|---|---|---|
| Tạo, mở, lưu, đóng | `NEW`, `QNEW`, `OPEN`, `SAVE`, `QSAVE`, `SAVEAS`, `CLOSE`, `CLOSEALL`, `QUIT` | ✓ | **P0** | |
| Bản vẽ mẫu `.dwt` | Templates | ✓ | **P0** | Mẫu TCVN mặc định ([04](04-chien-luoc-san-pham.md)) |
| Đọc DWG R13–2018, lưu DWG 2018 (AC1032) và 2004 (AC1018) | `OPEN`, `SAVEAS` | ✓ | **P0** | Codec tự viết theo đặc tả DWG miễn phí của ODA (ACadSharp và LibreDWG chỉ dùng để đối chứng). AutoCAD 2018–2027 vẫn dùng định dạng 2018 (AC1032) |
| Lưu DWG 2000/2007/2010/2013 | `SAVEAS` | ✓ | P1 | Tự bổ sung bộ ghi theo đặc tả DWG miễn phí của ODA |
| Đọc DWG R12 trở về trước, lưu R12/R14 | `OPEN`, `SAVEAS` | ✓ | P2 | Làm DXF R12 trước |
| DXF ASCII/nhị phân | `SAVEAS`, `DXFIN` | ✓ | **P0** | Quan trọng cho CNC và GIS |
| Tự lưu, tệp `.bak`, khóa tệp `.dwl` | `SAVETIME`, `ISAVEBAK` | ✓ | **P0** | |
| Phục hồi, kiểm tra lỗi | `RECOVER`, `RECOVERALL`, `AUDIT`, `DRAWINGRECOVERY` | ✓ | **P0** / P1 / **P0** / P1 | |
| Dọn rác bản vẽ | `PURGE` (gồm regapps, đối tượng dài 0, text rỗng) | ✓ | **P0** | Bản vẽ VN thường phình to do regapps |
| Thuộc tính bản vẽ | `DWGPROPS` | ✓ | P1 | |
| Đóng gói gửi đi (kèm xref, font, CTB, ảnh) | `ETRANSMIT` | ✓ | P1 | Đóng gói cả font SHX tiếng Việt |
| Chuyển phiên bản hàng loạt | DWG Convert | ✓ | P1 | Chạy qua `trion-console` |
| Batch Save, Reference Manager | Công cụ độc lập | ? | P2 | |
| Thumbnail trong Explorer, liên kết `.dwg` với ứng dụng, kéo-thả tệp | — | ✓ | P1 / **P0** / **P0** | |
| Mở một phần bản vẽ | `PARTIALOPEN` | ✗ | ✕ | Ít dùng |
| Đặt mật khẩu bản vẽ | — | — | ✕ | Autodesk đã bỏ từ 2016 |
| Cảnh báo TrustedDWG khi mở trong AutoCAD | — | — | Ghi chú | Tệp do TrionCAD lưu sẽ hiện thông báo "không phải ứng dụng Autodesk". Cần FAQ và hướng dẫn |
| Hiển thị và giữ nguyên proxy object (đối tượng của Civil 3D, Architecture, plugin…) | — | ✓ | P1 | Vẽ bằng proxy graphics lưu trong DWG; khi lưu phải ghi lại nguyên vẹn dữ liệu chưa hiểu |

## 3. Tọa độ và hỗ trợ chính xác

| Chức năng | Lệnh | LT | Ưu tiên | Ghi chú |
|---|---|---|---|---|
| Nhập tọa độ tuyệt đối, tương đối `@`, cực `<`, nhập khoảng cách trực tiếp, `#` | — | ✓ | **P0** | |
| Hệ tọa độ người dùng | `UCS` (Origin/3P/Object/View/World), `UCSMAN`, `UCSICON`, `PLAN` | ✓ | **P0** cơ bản; P1 đầy đủ | |
| Truy bắt điểm: END, MID, CEN, NOD, QUA, INT, EXT, INS, PER, TAN, NEA, FROM, M2P, TT, NONE | `OSNAP`, ghi đè bằng Shift+chuột phải | ✓ | **P0** | Phải giống hệt về hành vi và thứ tự ưu tiên |
| GCE, APP, PAR | `OSNAP` | ✓ | P1 | |
| Polar tracking, Object snap tracking | `POLAR` (F10), `OTRACK` (F11) | ✓ | **P0** | |
| Ortho, Snap, Grid | `ORTHO` (F8), `SNAP` (F9), `GRID` (F7) | ✓ | **P0** | |
| Vẽ isometric | `ISODRAFT`, F5 | ✓ | P1 | |
| Lệnh trong suốt | `'ZOOM`, `'PAN`, `'CAL`… | ✓ | **P0** | |
| Máy tính | `CAL`, `QUICKCALC` | ✓ | P1 / P2 | |
| Đơn vị, giới hạn | `UNITS`, `LIMITS`, `MEASUREMENT` | ✓ | **P0** / P1 / P1 | |
| Chọn luân phiên khi đối tượng chồng nhau | `SELECTIONCYCLING` | ✓ | P1 | |

## 4. Lựa chọn đối tượng

| Chức năng | Lệnh | LT | Ưu tiên | Ghi chú |
|---|---|---|---|---|
| Chọn từng đối tượng, Window, Crossing, Fence, WPolygon, CPolygon, All, Last, Previous, Add/Remove | `SELECT` | ✓ | **P0** | |
| Lasso | — | ✓ | P1 | |
| Chọn trước rồi mới gọi lệnh | `PICKFIRST`, `PICKADD` | ✓ | **P0** | |
| Chọn nhanh theo thuộc tính, chọn đối tượng tương tự | `QSELECT`, `SELECTSIMILAR` | ✓ | **P0** | |
| Bộ lọc nâng cao | `FILTER` | ✓ | P1 | Cũng là nền cho `ssget` có bộ lọc trong LISP (bắt buộc ở P0) |
| Nhóm đối tượng | `GROUP`, `UNGROUP`, `GROUPEDIT`, `CLASSICGROUP`, `PICKSTYLE` | ✓ | P1 | |
| Cô lập, ẩn đối tượng | `ISOLATEOBJECTS`, `HIDEOBJECTS`, `UNISOLATEOBJECTS` | ✓ | P1 | |
| Truy vấn chọn theo thuộc tính, lưu lại để dùng tiếp | (BricsCAD V26.2) | — | P2 | Điểm khác biệt |

## 5. Vẽ 2D

| Chức năng | Lệnh | LT | Ưu tiên | Ghi chú |
|---|---|---|---|---|
| Đoạn thẳng, đường dựng hình | `LINE`, `XLINE`, `RAY` | ✓ | **P0** / **P0** / P1 | |
| Đa tuyến (đoạn thẳng và cung, độ rộng, đóng kín) | `PLINE`, `PEDIT` | ✓ | **P0** | |
| Hình chữ nhật, đa giác | `RECTANG`, `POLYGON` | ✓ | **P0** | |
| Đường tròn (CR, 2P, 3P, TTR, TTT) | `CIRCLE` | ✓ | **P0** | |
| Cung (11 cách dựng) | `ARC` | ✓ | **P0** | |
| Ellipse, cung ellipse | `ELLIPSE` | ✓ | **P0** | |
| Spline (Fit/CV), sửa spline, nối cong | `SPLINE`, `SPLINEDIT`, `BLEND` | ✓ | **P0** / P1 / P1 | |
| Điểm, kiểu điểm, chia đều, chia theo khoảng | `POINT`, `DDPTYPE`, `DIVIDE`, `MEASURE` | ✓ | **P0** | `DIVIDE`/`MEASURE` có tùy chọn chèn block |
| Đám mây sửa đổi | `REVCLOUD` | ✓ | P1 | |
| Vùng che, vùng region, đường biên kín | `WIPEOUT`, `REGION`, `BOUNDARY` | ✓ | P1 / P1 / **P0** | `BOUNDARY` dùng nhiều để tính diện tích |
| Mặt cắt, tô màu chuyển sắc | `HATCH`, `GRADIENT`, `HATCHEDIT`, `HATCHGENERATEBOUNDARY`, `HATCHSETORIGIN` | ✓ | **P0** hatch cơ bản (pick point, island, liên kết, mẫu ANSI/ISO, annotative); P1 gradient và nâng cao | Nhận diện biên khi pick point là phần khó: kernel sắp xếp phẳng tự viết, giữ nguyên cung/spline thật |
| Đường đa tuyến song song (thường dùng vẽ tường ở VN) | `MLINE`, `MLSTYLE`, `MLEDIT` | ✓ | P1 | |
| Vùng tô đặc 2D (Solid) | `SOLID` | ✓ | P1 | Bắt buộc hiển thị đúng từ P0 |
| Dấu tâm, đường tâm có liên kết | `CENTERMARK`, `CENTERLINE` | ✓ | P1 | |
| Vành khuyên | `DONUT` | ✓ | P1 | |
| Vẽ tay, Trace | `SKETCH`, `TRACE` | ✓ | P4 / ✕ | |
| Đường xoắn, đa tuyến 3D | `HELIX`, `3DPOLY` | ~ | P3 | |
| Breakline, Superhatch (Express) | `BREAKLINE`, `SUPERHATCH` | ✗ | P2 | |

## 6. Hiệu chỉnh

| Chức năng | Lệnh | LT | Ưu tiên | Ghi chú |
|---|---|---|---|---|
| Xóa, khôi phục | `ERASE`, `OOPS` | ✓ | **P0** | |
| Di chuyển, sao chép, xoay, tỉ lệ, đối xứng, kéo giãn | `MOVE`, `COPY`, `ROTATE`, `SCALE`, `MIRROR` (`MIRRTEXT`), `STRETCH` | ✓ | **P0** | Cả tùy chọn Reference và Copy |
| Offset | `OFFSET` (Through/Erase/Layer/Multiple) | ✓ | **P0** | Offset polyline có cung: thuật toán tự viết (cung offset ra cung, cắt tự giao) |
| Cắt, kéo dài | `TRIM`, `EXTEND` (chế độ Quick 2021+ và Standard) | ✓ | **P0** | Lệnh dùng nhiều nhất. Phải nhanh và đúng |
| Bẻ, nối | `BREAK`, `BREAKATPOINT`, `JOIN` | ✓ | **P0** | |
| Bo góc, vát góc | `FILLET`, `CHAMFER` (Polyline/Multiple/Trim) | ✓ | **P0** | |
| Phá khối, kéo dài thêm | `EXPLODE`, `LENGTHEN` | ✓ | **P0** | |
| Mảng chữ nhật, mảng tròn | `ARRAYRECT`, `ARRAYPOLAR`, `ARRAYCLASSIC` | ✓ | **P0** (không liên kết) | |
| Mảng có liên kết, mảng theo đường | `ARRAYPATH`, `ARRAYEDIT` | ✓ | P1 | |
| Căn chỉnh | `ALIGN` | ✓ | P1 | |
| Sao chép thuộc tính | `MATCHPROP` | ✓ | **P0** | |
| Đổi thuộc tính | `CHANGE`, `CHPROP` | ✓ | P1 | Thường được gọi từ LISP |
| Thứ tự hiển thị | `DRAWORDER`, `TEXTTOFRONT`, `HATCHTOBACK` | ✓ | **P0** / P1 / P1 | |
| Xóa trùng, đảo chiều | `OVERKILL`, `REVERSE` | ✓ | P1 | |
| Grip: kéo, di chuyển, xoay, tỉ lệ, đối xứng; grip đa năng trên polyline | Grips | ✓ | **P0** cơ bản; P1 đa năng | |
| Undo/Redo nhiều bước, có đánh dấu | `UNDO`, `U`, `REDO`, `MREDO` | ✓ | **P0** | Tự xây: nhật ký thay đổi theo thuộc tính, mỗi lệnh một mark |
| Clipboard | `COPYCLIP`, `CUTCLIP`, `PASTECLIP`, `COPYBASE`, `PASTEBLOCK`, `PASTEORIG`, `PASTESPEC` | ✓ | **P0** / P1 | Dán bảng Excel thành bảng CAD: P1 (rất phổ biến ở VN) |
| Chuyển polyline nhẹ/nặng | `CONVERTPOLY` | ✓ | P1 | |
| Làm phẳng Z (bản vẽ khảo sát), cắt theo đường bao (Express) | `FLATTEN`, `EXTRIM`, `MPEDIT` | ✗ | P1 | |
| Boolean cho region | `UNION`, `SUBTRACT`, `INTERSECT` | ✓ | P2 | |
| Dịch nhỏ bằng phím mũi tên | Ctrl+mũi tên | ✓ | P2 | |

## 7. Văn bản

| Chức năng | Lệnh | LT | Ưu tiên | Ghi chú |
|---|---|---|---|---|
| Text một dòng | `TEXT`/`DTEXT` | ✓ | **P0** | |
| Text nhiều dòng, soạn tại chỗ: định dạng, xếp chồng phân số, bullet, đánh số, tab, giãn dòng, nền che | `MTEXT`, `MTEDIT` | ✓ | **P0** lõi; P1 cột và nâng cao | Engine dàn trang MTEXT tự viết, phải khớp AutoCAD (kiểm bằng so sánh ảnh) |
| Kiểu chữ: SHX + bigfont, TTF, annotative | `STYLE` | ✓ | **P0** | |
| Sửa text | `TEXTEDIT` (`DDEDIT`), nhấp đúp | ✓ | **P0** | |
| Tìm và thay thế (cả trong block, thuộc tính, dim) | `FIND` | ✓ | **P0** | |
| Kiểm tra chính tả | `SPELL` | ✓ | P2 | Có từ điển tiếng Việt |
| Đổi cỡ, căn lề, gióng thẳng text | `SCALETEXT`, `JUSTIFYTEXT`, `TEXTALIGN` | ✓ | P1 | |
| Gộp text thành mtext | `TXT2MTXT` | ✓ | P1 | |
| Ký tự đặc biệt: `%%c` `%%d` `%%p` `%%u` `%%o`, `\U+xxxx` | — | ✓ | **P0** | |
| Text annotative, danh sách tỉ lệ | `ANNOTATIVE`, `SCALELISTEDIT`, `OBJECTSCALE` | ✓ | P1 | |
| Hiển thị text nhanh | `QTEXT` | ✓ | P1 | |
| Công cụ text Express | `TXTEXP`, `ARCTEXT`, `TCASE`, `TCOUNT`, `TORIENT`, `TEXTMASK`, `TEXTFIT`, `RTEXT` | ✗ | P1 (`TCASE`, `TCOUNT`, `TXTEXP`); P2 phần còn lại | |
| **Bác sĩ Font** (nhận diện và chuyển TCVN3/VNI → Unicode, ánh xạ SHX) | Không có | — | **P0 (khác biệt VN)** | Thiết kế ở [05](05-kien-truc-ky-thuat.md) |
| Text SHX tìm được trong PDF xuất ra | `PDFSHX` | ✓ | P1 | |

## 8. Kích thước

| Chức năng | Lệnh | LT | Ưu tiên | Ghi chú |
|---|---|---|---|---|
| Kích thước thẳng, nghiêng, góc, bán kính, đường kính, nối tiếp, cùng gốc | `DIMLINEAR`, `DIMALIGNED`, `DIMANGULAR`, `DIMRADIUS`, `DIMDIAMETER`, `DIMCONTINUE`, `DIMBASELINE` | ✓ | **P0** | |
| Kích thước thông minh | `DIM` | ✓ | P1 | |
| Cung, gãy khúc, tọa độ điểm, ghi nhanh, dấu tâm | `DIMARC`, `DIMJOGGED`, `DIMORDINATE`, `QDIM`, `DIMCENTER` | ✓ | P1 | |
| Kiểu kích thước (toàn bộ DIMVAR), ghi đè, kiểu con | `DIMSTYLE`, `DIMOVERRIDE` | ✓ | **P0** lõi; P1 đầy đủ (dung sai, đơn vị phụ) | Có sẵn kiểu theo TCVN |
| Sửa kích thước, grip trên dim, ghi đè text `<>` | `DIMEDIT`, `DIMTEDIT` | ✓ | **P0** | |
| Kích thước liên kết với hình học | `DIMASSOC`, `DIMREASSOCIATE`, `DIMDISASSOCIATE` | ✓ | **P0** / P1 / P1 | |
| Ngắt, giãn cách, gãy, kiểm định | `DIMBREAK`, `DIMSPACE`, `DIMJOGLINE`, `DIMINSPECT` | ✓ | P1 / P1 / P1 / P2 | |
| Khung dung sai hình học GD&T | `TOLERANCE` | ✓ | P1 | |
| Layer mặc định cho dim | `DIMLAYER` | ✓ | P1 | |
| Kích thước annotative | — | ✓ | P1 | |
| Xuất/nhập kiểu dim (Express) | `DIMEX`, `DIMIM` | ✗ | P2 | |

## 9. Đường dẫn chú thích, bảng, trường dữ liệu

| Chức năng | Lệnh | LT | Ưu tiên | Ghi chú |
|---|---|---|---|---|
| Multileader và kiểu, thêm/bớt nhánh, gióng hàng, gom nhóm | `MLEADER`, `MLEADERSTYLE`, `MLEADEREDIT`, `MLEADERALIGN`, `MLEADERCOLLECT` | ✓ | **P0** tạo và sửa; P1 phần còn lại | |
| Leader cũ | `LEADER`, `QLEADER` | ✓ | P1 | |
| Bảng: kiểu bảng, công thức, gộp ô, chèn block vào ô | `TABLE`, `TABLESTYLE`, `TABLEDIT`, `TINSERT` | ✓ | P1 (bắt buộc hiển thị và sửa text trong ô ở P0) | Bảng thống kê thép, bảng diện tích |
| Xuất bảng CSV/Excel | `TABLEEXPORT` | ✓ | P1 | |
| Liên kết Excel | `DATALINK`, `DATALINKUPDATE` | ✓ | P2 | |
| Trường dữ liệu (ngày, tên tệp, thuộc tính đối tượng, công thức) | `FIELD`, `UPDATEFIELD` | ✓ | P1 | Dùng cho khung tên |

## 10. Layer và thuộc tính

| Chức năng | Lệnh | LT | Ưu tiên | Ghi chú |
|---|---|---|---|---|
| Layer Properties Manager: On/Off, Freeze, Lock, màu, linetype, lineweight, transparency, plot style, Plot/NoPlot, mô tả | `LAYER`, `-LAYER` | ✓ | **P0** | |
| VP Freeze, ghi đè thuộc tính layer theo viewport | `VPLAYER`, VP overrides | ✓ | **P0** VP Freeze; P1 ghi đè | |
| Bộ lọc layer (theo thuộc tính, theo nhóm), tìm kiếm | — | ✓ | P1 | |
| Trạng thái layer `.las` | `LAYERSTATE` | ✓ | P1 | |
| Công cụ layer nhanh | `LAYISO`, `LAYUNISO`, `LAYOFF`, `LAYON`, `LAYFRZ`, `LAYTHW`, `LAYMCUR`, `LAYMCH` | ✓ | **P0** | Dùng rất nhiều |
| Công cụ layer khác | `LAYLCK`, `LAYULK`, `LAYCUR`, `LAYMRG`, `LAYDEL`, `LAYWALK`, `COPYTOLAYER`, `LAYERP`, `LAYVPI` | ✓ | P1 | |
| Dịch layer theo chuẩn, thông báo layer mới | `LAYTRANS`, `LAYEREVAL` | ? | P2 | |
| Màu ACI, True Color, Color Books | `COLOR` | ✓ | **P0** (ACI + True Color); P2 (Color Books) | |
| Linetype (nạp `.lin`, linetype phức có shape/text) | `LINETYPE`, `LTSCALE`, `CELTSCALE`, `PSLTSCALE`, `MSLTSCALE` | ✓ | **P0** | Tự xây dựng tệp `.lin` với tên chuẩn |
| Lineweight, hiển thị lineweight, transparency | `LWEIGHT`, `LWDISPLAY`, `TRANSPARENCY` | ✓ | **P0** / **P0** / P1 | |
| ByLayer, ByBlock, đặt về ByLayer | `SETBYLAYER` | ✓ | **P0** / P1 | |
| **Bộ layer theo TCVN và theo bộ môn** | — | — | **P0 (khác biệt VN)** | |

## 11. Block và thuộc tính (attribute)

| Chức năng | Lệnh | LT | Ưu tiên | Ghi chú |
|---|---|---|---|---|
| Tạo, chèn, xuất block | `BLOCK`, `INSERT`, `-INSERT`, `WBLOCK`, `BASE`, `RENAME` | ✓ | **P0** | |
| Blocks palette (Current, Recent, Favorites, Libraries), MINSERT | `BLOCKSPALETTE`, `MINSERT` | ✓ | P1 | Đồng bộ thư viện qua đám mây: P3 |
| Block Editor | `BEDIT`, `BCLOSE`, `BSAVE`, `BTESTBLOCK` | ✓ | P1 | |
| Sửa tại chỗ block và xref | `REFEDIT`, `REFSET`, `REFCLOSE` | ✓ | P1 | |
| **Dynamic block: hiển thị đúng** | — | ✓ | **P0** | DWG lưu sẵn block đại diện (`*U…`) của từng tham chiếu nên hiển thị được ngay |
| Dynamic block: dùng grip tương tác (linear, polar, XY, rotation, flip, visibility, lookup) | — | ✓ | P1 | Engine tính parameter và action tự viết |
| Dynamic block: tạo mới (parameter, action, visibility state, bảng thuộc tính block, ràng buộc trong block) | `BPARAMETER`, `BACTION`, `BVSTATE`, `BTABLE`, `BCPARAMETER`, `BAUTHORPALETTE` | ✓ | P2 | Tự viết |
| Attribute: định nghĩa, sửa, đồng bộ, hiển thị | `ATTDEF`, `EATTEDIT`, `ATTEDIT`, `ATTSYNC`, `ATTDISP`, `BATTMAN` | ✓ | **P0** (`ATTDEF`, `EATTEDIT`, `ATTSYNC`, `ATTDISP`); P1 phần còn lại | |
| Trích xuất attribute, xuất/nhập attribute (Express) | `ATTEXT`, `EATTEXT`, `ATTOUT`, `ATTIN` | ~ | P1 / P2 | Dùng nhiều ở VN để đánh số, lập bảng |
| Đếm block | `COUNT`, `COUNTLIST`, `COUNTTABLE` (2022), `BCOUNT` | ✓ | P1 | |
| Thay block | `BLOCKREPLACE` (Express), Smart Blocks: Replace (2024) | ~ | P1 | |
| Phá block giữ giá trị attribute, sao chép đối tượng lồng (Express) | `BURST`, `NCOPY`, `XPLODE` | ✗ | P1 | |
| Smart Blocks: Placement (2024), Search & Convert (2025), Object Detection (2025–2026) | — | ✓ | P2 | Làm bằng khớp hình học tất định trước, ML sau |
| DesignCenter | `ADCENTER` | ✓ | P1 | |
| **Thư viện block VN** (cửa, thiết bị, nội thất, ký hiệu kết cấu, thép, điện, cao độ, trục) | — | — | **P0/P1 (khác biệt VN)** | |

## 12. Tham chiếu ngoài (xref) và underlay

| Chức năng | Lệnh | LT | Ưu tiên | Ghi chú |
|---|---|---|---|---|
| Xref DWG: Attach/Overlay, đường dẫn tương đối, nạp lại, gỡ, bind, clip, xref lồng nhau | `XREF`/`EXTERNALREFERENCES`, `XATTACH`, `XBIND`, `XCLIP`/`CLIP`, `XOPEN` | ✓ | **P0** | |
| So sánh xref, Missing References | `XCOMPARE`, Connected References (2027) | ✓ | P2 | |
| Ảnh raster: PNG/JPG/TIFF/BMP/GIF | `IMAGEATTACH` | ✓ | **P0** | |
| Ảnh: clip, chỉnh sáng/tương phản, chất lượng, khung | `IMAGECLIP`, `IMAGEADJUST`, `IMAGEQUALITY`, `IMAGEFRAME` | ✓ | P1 | |
| Ảnh JP2 / ECW | — | ✓ | P2 (JP2 qua OpenJPEG, BSD: thư viện giải mã ảnh thuộc hạ tầng phổ thông); ECW ✕ | AutoCAD 2027 đã bỏ MrSID (✕) |
| PDF underlay: layer, clip, bắt điểm | `PDFATTACH`, `PDFCLIP`, `PDFLAYERS`, `PDFADJUST` | ✓ | P1 | |
| **Chuyển PDF thành hình học DWG**, nhận dạng chữ SHX | `PDFIMPORT`, `PDFSHXTEXT` | ✓ | P1 | Giá trị cao ở VN. Đọc PDF bằng PDFium (BSD) |
| DGN: nhập, xuất, underlay | `DGNIMPORT`, `DGNEXPORT`, `DGNATTACH`, `DGNCLIP` | ✓ | P3 (nhập DGN V7); P4 (V8, xuất, underlay) | Ngành địa chính dùng MicroStation. Không có thư viện mở đủ tốt, và DGN V8 rất phức tạp |
| DWF underlay | `DWFATTACH` | ✓ | P4 | |
| Point cloud RCP/RCS | `POINTCLOUDATTACH` | ? | ✕ (định dạng riêng của Autodesk); E57/LAS: P4 | |
| Mô hình phối hợp NWD/NWC | `COORDINATIONMODELATTACH` | ? | P4 | |

## 13. Layout, viewport, in ấn và xuất bản

| Chức năng | Lệnh | LT | Ưu tiên | Ghi chú |
|---|---|---|---|---|
| Model/Paper space, tạo, sao chép, đổi tên layout | `LAYOUT`, `LAYOUTWIZARD`, `MSPACE`/`PSPACE` | ✓ | **P0** | |
| Viewport chữ nhật, đặt tỉ lệ, khóa | `MVIEW`, `VPORTS`, `VPLOCK` | ✓ | **P0** | |
| Viewport đa giác hoặc theo đối tượng, clip, phóng to/thu nhỏ | `MVIEW`, `VPCLIP`, `VPMAX`, `VPMIN` | ✓ | P1 | |
| Annotation scale và đối tượng annotative | `CANNOSCALE`, `ANNOAUTOSCALE`, `OBJECTSCALE` | ✓ | P1 | |
| Chuyển đối tượng giữa không gian | `CHSPACE` | ✓ | P1 | |
| Page Setup: máy in, khổ giấy, vùng in, tỉ lệ, plot style, lineweight | `PAGESETUP` | ✓ | **P0** | |
| In, xem trước, **in ra PDF** (layer, hyperlink, text tìm được) | `PLOT`, `PREVIEW`, `EXPORTPDF` | ✓ | **P0** | |
| Plot style CTB (theo màu) | `STYLESMANAGER` | ✓ | **P0** (dùng và sửa) | Kèm CTB mẫu theo TCVN |
| Plot style STB (đặt tên), chuyển đổi giữa CTB và STB | `CONVERTCTB`, `CONVERTPSTYLES` | ✓ | P1 | |
| Cấu hình máy in PC3, PMP | `PLOTTERMANAGER` | ✓ | P1 đọc; P2 soạn | |
| In hàng loạt nhiều layout hoặc nhiều bản vẽ | `PUBLISH` | ✓ | P1 | |
| **In hàng loạt theo khung tên đặt trong Model** | (ZWCAD Smart Plot) | — | **P1 (khác biệt VN)** | Quy trình rất phổ biến, hiện làm bằng LISP |
| Xuất layout ra Model, xuất DWF/DWFx, ảnh, WMF | `EXPORTLAYOUT`, `EXPORTDWF`, `EXPORTDWFX`, `PNGOUT`, `JPGOUT`, `BMPOUT`, `TIFOUT`, `WMFOUT` | ✓ | P1 / P2 / P1 / P2 | |
| Xuất SVG | — | — | P2 | Điểm khác biệt nhỏ, tự viết |
| Dấu in | `PLOTSTAMP` | ✓ | P2 | |
| Xuất PostScript | `PSOUT` | — | ✕ | AutoCAD 2027 đã bỏ |
| In viewport có shading/visual style | — | ~ | P3 | |

## 14. Sheet Set (quản lý bộ bản vẽ)

| Chức năng | Lệnh | LT | Ưu tiên | Ghi chú |
|---|---|---|---|---|
| Sheet Set Manager: danh sách tờ, sheet view, callout, trường khung tên, publish, lưu trữ | `SHEETSET`, `NEWSHEETSET`, `OPENSHEETSET`, `ARCHIVE` | ✓ | P2 | Đọc/ghi `.dst` |
| Sheet Set Manager trên web (Autodesk Docs/Forma) | — | ✓ | P4 | |
| **"Quản lý bộ bản vẽ" kiểu VN**: danh mục bản vẽ, tự điền ký hiệu "KT 1/18" vào khung tên, in cả bộ | — | — | **P1 (khác biệt VN)** | Gọn hơn SSM |

## 15. Ràng buộc tham số

| Chức năng | Lệnh | LT | Ưu tiên | Ghi chú |
|---|---|---|---|---|
| Giữ nguyên ràng buộc khi mở và lưu DWG | — | — | P1 | Không làm hỏng dữ liệu |
| 12 ràng buộc hình học (coincident, collinear, concentric, fix, parallel, perpendicular, horizontal, vertical, tangent, smooth, symmetric, equal) | `GEOMCONSTRAINT` | ~? | P2 | Bộ giải ràng buộc tự viết (tham khảo thuật toán của PlaneGCS) |
| Ràng buộc kích thước (dynamic/annotational) | `DIMCONSTRAINT` | ~? | P2 | |
| Tự suy luận ràng buộc, quản lý tham số, biểu thức | `AUTOCONSTRAIN`, `CONSTRAINTINFER`, `PARAMETERS`, `CONSTRAINTBAR`, `DELCONSTRAINT`, `DCDISPLAY` | ~? | P2 | |

## 16. Truy vấn và đo lường

| Chức năng | Lệnh | LT | Ưu tiên | Ghi chú |
|---|---|---|---|---|
| Khoảng cách, diện tích (cộng/trừ), đo nhanh | `DIST`, `AREA`, `MEASUREGEOM` (Quick) | ✓ | **P0** | |
| Liệt kê thuộc tính, tọa độ điểm | `LIST`, `ID` | ✓ | **P0** | |
| Tính chất khối lượng | `MASSPROP` | ✓ | P1 | |
| Thông tin bản vẽ, thời gian, theo dõi sysvar | `STATUS`, `TIME`, `DBLIST`, `SETVAR`, `SYSVARMONITOR` | ✓ | P1 (riêng `SETVAR`: **P0**) | |
| **Tổng chiều dài, tổng diện tích, bảng diện tích phòng** | (LISP phổ biến ở VN) | — | **P1 (khác biệt VN)** | Thay cho các LISP rời |

## 17. Dữ liệu, bóc tách, thông tin

| Chức năng | Lệnh | LT | Ưu tiên | Ghi chú |
|---|---|---|---|---|
| Trích xuất dữ liệu ra bảng hoặc Excel | `DATAEXTRACTION` | ? | P1 bản rút gọn (block, attribute ra Excel); P2 đầy đủ | |
| **So sánh hai bản vẽ hoặc hai phiên bản** | `COMPARE` (2019+) | ✓ | P1 | Đối thủ đã có (progeCAD 2027, IntelliCAD 15) |
| Hyperlink | `HYPERLINK` | ✓ | P2 | |
| Đối tượng OLE (Excel, Word nhúng) | `INSERTOBJ` | ✓ | P2 (hiển thị); P4 (sửa) | |
| Vị trí địa lý, bản đồ nền | `GEOGRAPHICLOCATION`, `GEOMAP` | ? | P3 | Bản đồ nền qua dịch vụ tile mở; chuyển đổi VN-2000 tự viết theo công thức công khai |
| Activity Insights, lịch sử DWG | — | ✓ | P3 | |
| Geometry Cleanup: khe hở, đoạn thừa, góc lệch (2027) | — | ? | P2 | Viết bằng thuật toán tất định |
| Kiểm tra sức khỏe bản vẽ (BricsCAD V27) | — | — | P2 | |
| Nhập ghi chú từ PDF/ảnh (Markup Import/Assist) | `MARKUPIMPORT` | ✓ | P3 | |
| Kết nối CSDL | `DBCONNECT` | ✗ | ✕ | Lỗi thời |

## 18. Tiện ích, kiểm tra, chuẩn hóa, tự động hóa

| Chức năng | Lệnh | LT | Ưu tiên | Ghi chú |
|---|---|---|---|---|
| Regen, redraw, độ mịn cung tròn, tô đặc | `REGEN`, `REGENALL`, `REDRAW`, `VIEWRES`, `FILL` | ✓ | **P0** | |
| Kịch bản `.scr` | `SCRIPT`, `RSCRIPT`, `DELAY`, `RESUME` | ✓ | **P0** | Nền của kiểm thử tự động |
| Kiểm tra chuẩn `.dws` | `STANDARDS`, `CHECKSTANDARDS` | ✗? | P2 | |
| **Kiểm tra bản vẽ theo TCVN** (layer, chiều cao chữ theo tỉ lệ, lineweight, khung tên) | — | — | P2 (khác biệt VN) | Kết hợp với AI để giải thích lỗi: P3 |
| Action Recorder | `ACTRECORD`, `ACTMANAGER` | ✗ | P4 | |
| Chuyển cài đặt từ AutoCAD (profile, PGP, CUI, LISP startup) | Migrate settings | ✓ | P1 | Công cụ "Chuyển từ AutoCAD sang" |

## 19. Tùy biến và lập trình

| Chức năng | AutoCAD | LT | Ưu tiên | Ghi chú |
|---|---|---|---|---|
| **AutoLISP** lõi: danh sách, chuỗi, toán; `entget`/`entmod`/`entmake`; `ssget` có bộ lọc; `command`/`command-s`/`vl-cmdf`; `getXXX`; `tblsearch`; dictionary; xdata; `*error*` | AutoLISP | ✓ (từ 2024) | **P0** | Quyết định khả năng chuyển đổi. Kiểm thử bằng bộ LISP của cộng đồng VN |
| Hộp thoại DCL | `load_dialog`, `new_dialog`… | ✓ | **P0** | Dựng giao diện DCL bằng Qt |
| Visual LISP: `vl-*`, `vlax-*`, `vlr-*` (reactor) | Visual LISP | ~ | P1 | |
| Mô hình ActiveX qua `vla-*` | Visual LISP + ActiveX | ✗? | P2 | Khối lượng rất lớn. Ưu tiên tập con mà LISP phổ biến hay dùng |
| Nạp LISP biên dịch FAS/VLX | — | ✓ | P2 | Cần đánh giá pháp lý trước (IntelliCAD đã làm được) |
| Nạp tự động `acad.lsp`/`acaddoc.lsp`/`.mnl`, Startup Suite | `APPLOAD` | ✓ | **P0** | Tương thích tên tệp khởi động (alias `trion.lsp`/`triondoc.lsp`) |
| Gỡ lỗi LISP | VS Code extension (từ 2021) | ✓ | P2 | |
| DIESEL, macro menu `^C^C` | `MODEMACRO` | ✓ | P1 | |
| Tùy biến giao diện CUI, nhập CUIX của AutoCAD | `CUI`, `CUIIMPORT`, `CUILOAD` | ✓ | P1 nhập CUIX một phần (toolbar, menu, macro); P2 trình sửa CUI | Theo lộ trình v1.0 ([06](06-lo-trinh-trien-khai.md) §5) |
| Menu cũ MNU/MNS | — | — | P4 | |
| Tool palette tùy biến, phím tắt, workspace, xuất/nhập profile | `CUSTOMIZE`, `.arg` | ✓ | P1 / P1 / P1 / P2 | |
| ObjectARX (C++) | ObjectARX | ✗ | ✕ tương thích nhị phân; **SDK C API + C++ wrapper: P2** | Lõi viết bằng C++20. Khái niệm gần ObjectARX nên plugin Visual C++ (như Nova-TDN) port được với công sức vừa phải |
| .NET API | AutoCAD .NET | ✗ | P3 | Bọc trên C API. Mô phỏng các khái niệm quen thuộc (Document, Database, Transaction, Entity) để plugin VN dễ port |
| ActiveX/COM điều khiển từ ứng dụng ngoài (Excel VBA) | COM | ✗ | P3 (Windows) | Bảng tính kỹ thuật VN hay điều khiển CAD qua COM |
| VBA nhúng | VBA 7.1 | ✗ | ✕ | Microsoft đã ngừng cấp phép VBA mới cho ISV từ khoảng 2008 **[cần xác minh]**. Thay bằng LISP, SDK C/C++ và (P3) .NET/Python |
| **Python API** | Không có | — | P2 (khác biệt) | Tự động hóa, tích hợp AI |
| Console không giao diện | AutoCAD Core Console (`accoreconsole`) | ✗ | P2 (nội bộ dùng từ P0 để kiểm thử) | `trion-console` |
| Chạy CAD trên đám mây | Design Automation API (APS) | — | P4 | |
| Bảo mật nạp mã | `SECURELOAD`, `TRUSTEDPATHS` | ✓ | P1 | |
| Gói ứng dụng tự nạp (`.bundle`), chợ ứng dụng | Autoloader, App Store | ✓ | P3 | |

## 20. Mô hình 3D: solid, surface, mesh

> **Lưu ý:**
> - *Giữ nguyên* dữ liệu 3D khi mở và lưu DWG là **P1** (giữ khối dữ liệu ACIS nguyên vẹn), vì không được làm hỏng bản vẽ 3D của khách hàng.
> - *Hiển thị* solid cần bộ đọc ACIS SAT/SAB tự viết: **P3**.
> - Tạo và sửa 3D là **P3** và phụ thuộc quyết định kernel ([05](05-kien-truc-ky-thuat.md) §7.1).

| Nhóm | Lệnh AutoCAD | LT | Ưu tiên |
|---|---|---|---|
| Khối cơ bản | `BOX`, `CYLINDER`, `CONE`, `SPHERE`, `PYRAMID`, `WEDGE`, `TORUS`, `POLYSOLID` | ✗ | P3 |
| Tạo khối từ biên dạng | `EXTRUDE`, `REVOLVE`, `SWEEP`, `LOFT`, `PRESSPULL`, `THICKEN` | ✗ | P3 |
| Boolean và cắt | `UNION`, `SUBTRACT`, `INTERSECT`, `SLICE`, `SECTION`, `SECTIONPLANE`, `INTERFERE` | ✗ | P3 |
| Sửa solid | `SOLIDEDIT` (mặt, cạnh, thân: extrude, offset, taper, shell, imprint, separate, clean, check), `FILLETEDGE`, `CHAMFEREDGE`, `OFFSETEDGE` | ✗ | P3 |
| Thao tác 3D | `3DMOVE`, `3DROTATE`, `3DSCALE`, `3DALIGN`, `MIRROR3D`, `3DARRAY`, gizmo, chọn đối tượng con (Ctrl) | ✗ | P3 |
| Chuyển đổi | `CONVTOSOLID`, `CONVTOSURFACE`, `CONVTOMESH`, `XEDGES`, `FLATSHOT` | ✗ | P3 (`FLATSHOT`: P3, nhu cầu cao) |
| Surface (procedural/NURBS) | `PLANESURF`, `SURFNETWORK`, `SURFBLEND`, `SURFPATCH`, `SURFOFFSET`, `SURFFILLET`, `SURFTRIM`, `SURFUNTRIM`, `SURFEXTEND`, `SURFSCULPT`, `SURFEXTRACTCURVE`, `CVADD`, `CVREMOVE`, `CVREBUILD`, phân tích zebra/curvature/draft | ✗ | P4 |
| Mesh | `MESH` (primitive), `MESHSMOOTH`, `MESHREFINE`, `MESHCREASE`, `MESHSPLIT`, `MESHEXTRUDE`, `MESHMERGE`, `MESHCAP`, `3DMESH`, `PFACE`, `3DFACE`, `RULESURF`, `TABSURF`, `REVSURF`, `EDGESURF` | ✗ | P3 (`3DFACE`, `RULESURF`, `REVSURF`, `EDGESURF`); P4 phần còn lại |

## 21. Hiển thị và điều hướng

| Chức năng | Lệnh | LT | Ưu tiên |
|---|---|---|---|
| Zoom (realtime, window, extents, all, previous, scale, object), Pan, zoom bằng con lăn | `ZOOM`, `PAN`, `ZOOMFACTOR` | ✓ | **P0** |
| Named views, View Manager | `VIEW`, `-VIEW` | ✓ | P1 |
| Xoay 3D để xem bản vẽ 3D, ViewCube | `3DORBIT`, `3DFORBIT`, `3DCORBIT`, `NAVVCUBE` | ~ | P1 |
| Camera, đi bộ/bay | `CAMERA`, `3DWALK`, `3DFLY` | ✗ | P4 |
| `VPOINT`, `DVIEW`, `PLAN` | | ✓ | P1 / P4 / P1 |
| Visual styles: 2D Wireframe, Wireframe, Hidden, Shaded, Realistic | `VISUALSTYLES`, `VSCURRENT`, `HIDE`, `SHADEMODE` | ~ | P1 |
| Visual styles khác: Conceptual, Shades of Gray, Sketchy, X-ray | | ✗ | P3 |
| Tăng tốc phần cứng, khử răng cưa, hiển thị lineweight/transparency | `GRAPHICSCONFIG` | ✓ | **P0** |

## 22. Trực quan hóa và render

| Chức năng | Lệnh | LT | Ưu tiên | Ghi chú |
|---|---|---|---|---|
| Render, preset, môi trường IBL, phơi sáng | `RENDER`, `RENDERPRESETS`, `RENDERENVIRONMENT`, `RENDEREXPOSURE` | ✗ | P4 | Hoặc tích hợp render AI hay renderer bên ngoài |
| Vật liệu | `MATBROWSEROPEN`, `MATEDITOROPEN`, `MATERIALATTACH`, `MATERIALMAP` | ✗ | P4 | |
| Ánh sáng, mặt trời | `POINTLIGHT`, `SPOTLIGHT`, `DISTANTLIGHT`, `WEBLIGHT`, `SUNPROPERTIES` | ✗ | P4 | |
| Hoạt hình theo đường | `ANIPATH` | ✗ | ✕ | |

## 23. Tài liệu hóa từ mô hình 3D

| Chức năng | Lệnh | LT | Ưu tiên |
|---|---|---|---|
| Hình chiếu cơ sở, hình chiếu phụ, mặt cắt, chi tiết, cập nhật | `VIEWBASE`, `VIEWPROJ`, `VIEWSECTION`, `VIEWDETAIL`, `VIEWEDIT`, `VIEWUPDATE`, `VIEWSTD` | ✗ | P4 |
| Lệnh cũ | `SOLVIEW`, `SOLDRAW`, `SOLPROF` | ✗ | ✕ |

## 24. Nhập/xuất định dạng khác

| Định dạng | AutoCAD | Ưu tiên TrionCAD | Ghi chú |
|---|---|---|---|
| PDF (xuất, nhập, underlay) | ✓ | **P0** xuất; P1 nhập và underlay | Bộ ghi PDF vector tự viết; đọc và hiển thị PDF bằng PDFium (BSD) |
| DGN V7/V8 | ✓ | P3 nhập V7; P4 V8 | Không có thư viện mở đủ tốt. Tạm thời người dùng xuất DWG từ MicroStation |
| DWF/DWFx | ✓ | P4 | Ít dùng ở VN |
| STL (`STLOUT`, `3DPRINT`) | ✓ | P3 | Tự viết (đơn giản) |
| SAT (`ACISIN`/`ACISOUT`) | ✓ | P3 | |
| IGES, STEP | ✓ (`IMPORT`, `IGESEXPORT`) | P3 | Tự viết bộ đọc STEP/IGES (định dạng công khai); có thể dùng Open CASCADE nếu quyết định ở G3 |
| FBX | ✓ | P4 | |
| CATIA, SolidWorks, NX, Creo, Inventor, Parasolid, JT | ✓ (`IMPORT`) | ✕ | Định dạng đóng. Người dùng xuất STEP từ phần mềm gốc |
| Rhino 3DM | ✓ | P4 | |
| IFC | Toolset | P3 (nhập, xem) | Bộ đọc IFC (STEP Part 21) tự viết, đối chứng với IfcOpenShell |
| Revit RVT/RFA | Toolset | ✕ | Định dạng đóng, không có thư viện miễn phí. Dùng IFC xuất từ Revit |
| glTF/OBJ (web) | — | P3 | Phục vụ trình xem web |
| SVG | — | P2 xuất; P3 nhập | |
| Ảnh BMP/PNG/JPG/TIF, WMF | ✓ | P1 / P2 | |
| KML/KMZ, SHP | Map 3D | P4 | |
| Excel/CSV (bảng, dữ liệu) | ✓ | P1 | |

## 25. Cộng tác và đám mây

| Chức năng | AutoCAD | LT | Ưu tiên | Ghi chú |
|---|---|---|---|---|
| Chia sẻ bản xem qua link | `SHARE` | ✓ | P3 | Chia sẻ qua Zalo (khác biệt VN) |
| Shared Views | | | ✕ | AutoCAD 2027 đã bỏ |
| Trace (lớp ghi chú phủ lên bản vẽ) | `TRACE` | ✓ | P3 | |
| Nhập ghi chú, Markup Assist | `MARKUPIMPORT` | ✓ | P3 | |
| Đẩy PDF lên đám mây (Push to Docs → Push to Forma) | | ✓ | P3 | |
| Kết nối lưu trữ đám mây (Dropbox, OneDrive, Google Drive, Box) | | ✓ | P3 | Thư mục đồng bộ cục bộ vẫn chạy được ngay |
| Ứng dụng web và mobile (xem và sửa DWG) | AutoCAD Web/Mobile | ✓ | P3 (trình xem web); P4 (sửa trên web và mobile) | |
| Lịch sử phiên bản, Activity Insights | | ✓ | P3 | |
| **Checkout** (2027): sửa song song ở mức đối tượng trên DWG dùng chung | | ? | P4 | Mục tiêu dài hạn là đồng chỉnh sửa thời gian thực |

## 26. AI và tự động hóa thông minh (AutoCAD 2024–2027)

| Chức năng AutoCAD | Năm | Ưu tiên TrionCAD | Cách làm đề xuất |
|---|---|---|---|
| Autodesk Assistant: chatbot hỗ trợ (2025), có MCP và kiểm tra chuẩn công ty (xem trước, 2027), Insights (2027.1) | 2025–2027 | P3 | **Trợ lý TrionCAD tiếng Việt**: MCP server trong ứng dụng, dùng công cụ lệnh/LISP có xác nhận của người dùng |
| Smart Blocks: Placement, Replace, Search & Convert, Object Detection | 2024–2026 | P2 | Khớp hình học tất định (chữ ký hình học, bất biến khi xoay và đổi tỉ lệ) trước, ML sau |
| Markup Assist (nhận dạng ghi chú sửa đổi) | 2023 | P3 | OCR tiếng Việt (PaddleOCR) |
| Geometry Cleanup | 2027 | P2 | Thuật toán tất định |
| My Insights / Macro Advisor | 2022 | P4 | |
| **AI cho VN**: giải thích lỗi TCVN, sinh LISP từ mô tả tiếng Việt, đọc PDF/scan thành DWG, bóc khối lượng | — | P3–P4 | |

## 27. Bảo mật và quản trị

| Chức năng | Ưu tiên | Ghi chú |
|---|---|---|
| Nạp mã an toàn (`SECURELOAD`, `TRUSTEDPATHS`) | P1 | Chặn LISP độc hại đi kèm bản vẽ (mã độc `acad.lsp` từng phổ biến) |
| Cài đặt im lặng, triển khai qua mạng, cấu hình quản trị | P1 | Doanh nghiệp và cơ quan nhà nước cần |
| Giấy phép: người dùng định danh, **kích hoạt offline**, giấy phép mạng (floating) | **P0** | Floating là lợi thế so với Autodesk |
| Telemetry chỉ khi người dùng đồng ý, nhật ký riêng tư | **P0** | |
| Chữ ký số bản vẽ | P4 | |

## 28. Express Tools (theo nhóm)

| Nhóm | Lệnh | Ưu tiên |
|---|---|---|
| Block | `BURST`, `NCOPY`, `BLOCKREPLACE`, `BCOUNT`, `GATTE`, `BLOCKTOXREF`, `XLIST` | P1 (`BURST`, `NCOPY`, `BCOUNT`, `GATTE`); P2 phần còn lại |
| Text | `TXTEXP`, `TCASE`, `TCOUNT`, `TORIENT`, `ARCTEXT`, `TEXTMASK`, `TEXTFIT`, `RTEXT` | P1 / P2 |
| Hiệu chỉnh | `FLATTEN`, `EXTRIM`, `MPEDIT`, `MOCORO`, `MSTRETCH`, `BSCALE` | P1 (`FLATTEN`, `EXTRIM`, `MPEDIT`); P2 phần còn lại |
| Vẽ | `BREAKLINE`, `SUPERHATCH`, `MKSHAPE`, `MKLTYPE` | P2 |
| Layout | `ALIGNSPACE`, `VPSYNC`, `VPSCALE` | P2 |
| Tệp | `SAVEALL`, `REVERT`, `MOVEBAK`, `QQUIT`, `REDIR` | P1 (`SAVEALL`); P2 phần còn lại |
| Dim | `DIMEX`, `DIMIM`, `QLATTACH` | P2 |
| Web | `SHOWURLS`, `REPURLS`, `CHURLS` | P4 |
| Công cụ | `SYSVDLG`, `XDATA`, `XDLIST`, `EDITTIME`, `SHP2BLK` | P2 |

## 29. Các LISP cộng đồng VN nên tích hợp sẵn (khác biệt VN)

| Công cụ | Ưu tiên |
|---|---|
| Tổng chiều dài, tổng diện tích, xuất ra Excel | P1 |
| Đánh số tự động, sao chép có tăng số (trục, phòng, cọc) | P1 |
| Thống kê thép (bảng thép từ ký hiệu), bảng diện tích phòng | P1–P2 |
| In hàng loạt theo khung tên | P1 |
| Chuyển mã font, sửa lỗi font | **P0** |
| Ký hiệu cao độ, trục định vị, mặt cắt, ký hiệu vật liệu theo TCVN | P1 |
| Nối, cắt, gộp text; đổi chữ hoa/thường có dấu tiếng Việt | P1 |
| Xuất/nhập attribute hoặc bảng giữa CAD và Excel | P1 |

> Trước khi đóng gói lại LISP của cộng đồng, phải **xin phép tác giả** (hoặc tự viết lại theo đặc tả) và ghi công rõ ràng.

## 30. Toolset chuyên ngành của AutoCAD ("including specialized toolsets")

| Toolset | Nội dung (theo Autodesk, số liệu marketing có thể đã thay đổi) | Hướng của TrionCAD |
|---|---|---|
| Architecture | Hơn 8.000 đối tượng thông minh (tường, cửa, cửa sổ), tự sinh mặt bằng, mặt cắt, mặt đứng | P4: gói "Kiến trúc VN", hoặc đối tác. Trước mắt, đối tượng AutoCAD Architecture hiển thị qua proxy graphics (P1) |
| Mechanical | Hơn 700.000 chi tiết tiêu chuẩn, kích thước cơ khí, BOM, GD&T, ISO/ANSI/DIN/JIS/GB | P4: gói "Cơ khí", có tiêu chuẩn TCVN |
| Electrical | Hơn 65.000 ký hiệu, sơ đồ, PLC, tủ điện, báo cáo | P4: đối tác |
| MEP | Hơn 10.500 đối tượng ống gió, ống nước, máng cáp | P4 |
| Map 3D | GIS: SHP, SDF, FDO, hệ tọa độ | P4: gói "Địa chính" với VN-2000 |
| Plant 3D | P&ID, đường ống 3D, isometric | ✕ |
| Raster Design | Sửa ảnh raster, vector hóa | P4 (dùng AI và thị giác máy tính) |

## 31. Tệp hỗ trợ và định dạng hệ sinh thái

| Tệp | Vai trò | Ưu tiên |
|---|---|---|
| `.dwg`, `.dxf`, `.dwt` | Bản vẽ, trao đổi, mẫu | **P0** |
| `.bak`, `.sv$`, `.dwl`/`.dwl2` | Sao lưu, tự lưu, khóa | **P0** |
| `.shx`/`.shp`, `.ttf`/`.otf` | Font và shape | **P0** (TrionCAD tự phát hành font SHX tương thích tên, xem [05](05-kien-truc-ky-thuat.md) §13) |
| `.pat`, `.lin` | Mẫu hatch, linetype | **P0** (tự viết tệp, giữ tên mẫu chuẩn) |
| `.ctb`/`.stb`, `.pc3`/`.pmp` | Plot style, cấu hình máy in | **P0** / P1 |
| `.pgp` | Viết tắt lệnh | **P0** |
| `.lsp`, `.dcl`, `.mnl`, `.scr` | LISP, hộp thoại, script | **P0** |
| `.fas`, `.vlx` | LISP biên dịch | P2 |
| `.cuix`/`.cui`, `.mnu`/`.mns` | Giao diện tùy biến | P1 nhập một phần, P2 đầy đủ / P4 |
| `.las`, `.dws`, `.dst`, `.dxe` | Trạng thái layer, chuẩn, sheet set, trích xuất dữ liệu | P1 / P2 / P2 / P2 |
| `.arx`/`.dbx`/`.crx`, `.dll` (.NET), `.dvb` (VBA) | Plugin AutoCAD | ✕ (không tương thích nhị phân) |
| `.bundle` (PackageContents.xml) | Gói tự nạp | P3 |

---

## Phụ lục A — Danh sách lệnh nghiệm thu MVP (P0)

Bản Pro Early Access chỉ được "đạt" khi **tất cả** lệnh dưới đây cho kết quả trùng AutoCAD trên bộ kịch bản kiểm thử: cùng tùy chọn, cùng kết quả hình học, cùng cách gọi từ `.scr` và `(command)`.

- **Tệp:** NEW, OPEN, SAVE, SAVEAS, QSAVE, CLOSE, RECOVER, AUDIT, PURGE, UNITS, OPTIONS
- **Hiển thị:** ZOOM, PAN, REGEN, REDRAW, VIEWRES
- **Vẽ:** LINE, XLINE, PLINE, RECTANG, POLYGON, CIRCLE, ARC, ELLIPSE, SPLINE, POINT, DDPTYPE, DIVIDE, MEASURE, BOUNDARY, HATCH, HATCHEDIT
- **Sửa:** ERASE, OOPS, MOVE, COPY, ROTATE, SCALE, MIRROR, STRETCH, OFFSET, TRIM, EXTEND, BREAK, BREAKATPOINT, JOIN, FILLET, CHAMFER, EXPLODE, LENGTHEN, ARRAYRECT, ARRAYPOLAR, MATCHPROP, PEDIT, DRAWORDER, UNDO, U, REDO, COPYCLIP, CUTCLIP, PASTECLIP
- **Chọn:** SELECT, QSELECT, SELECTSIMILAR
- **Chú thích:** TEXT, MTEXT, STYLE, TEXTEDIT, FIND, DIMLINEAR, DIMALIGNED, DIMANGULAR, DIMRADIUS, DIMDIAMETER, DIMCONTINUE, DIMBASELINE, DIMSTYLE, DIMEDIT, DIMTEDIT, MLEADER
- **Layer và thuộc tính:** LAYER, LAYISO, LAYUNISO, LAYOFF, LAYON, LAYFRZ, LAYTHW, LAYMCUR, LAYMCH, COLOR, LINETYPE, LTSCALE, LWEIGHT, PROPERTIES
- **Block:** BLOCK, INSERT, WBLOCK, BASE, RENAME, ATTDEF, EATTEDIT, ATTSYNC, ATTDISP
- **Xref, ảnh:** XREF/XATTACH, XBIND, XCLIP, IMAGEATTACH
- **Layout, in:** LAYOUT, MVIEW, VPORTS, PAGESETUP, PLOT, PREVIEW, EXPORTPDF, STYLESMANAGER
- **Truy vấn:** DIST, AREA, LIST, ID, MEASUREGEOM, SETVAR
- **Tự động hóa:** SCRIPT, APPLOAD, AutoLISP lõi + DCL, viết tắt lệnh `.pgp`
- **VN:** Bác sĩ Font (FIXFONT), mẫu TCVN

## Phụ lục B — Lệnh dùng nhiều nhất **[giả thuyết, đo lại bằng telemetry ở bản beta]**

Theo kinh nghiệm chung của người dùng 2D, nhóm lệnh sau chiếm phần lớn thao tác hằng ngày: `LINE`, `PLINE`, `TRIM`, `EXTEND`, `OFFSET`, `COPY`, `MOVE`, `ERASE`, `ZOOM`/`PAN`, `FILLET`, `MIRROR`, `ROTATE`, `SCALE`, `STRETCH`, `CIRCLE`, `ARC`, `RECTANG`, `HATCH`, `TEXT`/`MTEXT`, `DIMLINEAR`, `DIMCONTINUE`, `MATCHPROP`, `PROPERTIES`, `LAYER`, `INSERT`, `BLOCK`, `EXPLODE`, `ARRAY`, `UNDO`, `PLOT`. Chúng phải được **tối ưu độ trễ và độ chính xác trước tiên**: hoàn tất dưới 20 ms trên máy tầm trung, dưới 50 ms trên máy thấp ([05](05-kien-truc-ky-thuat.md) §1).

## Nguồn

- Tính năng mới các năm: [Autodesk — AutoCAD 2025](https://www.autodesk.com/blogs/autocad/autocad-2025/) · [AutoCAD 2026.1](https://www.autodesk.com/blogs/autocad/autocad-2026-1/) · [Architosh — AutoCAD 2026](https://architosh.com/2025/04/whats-new-in-autocad-2026/) · [What's New in AutoCAD 2024 (help.autodesk.com)](https://help.autodesk.com/cloudhelp/2025/ENU/AutoCAD-WhatsNew/files/GUID-3890D5F7-04CB-4F3B-97FF-7A9577D41852.htm). Thông tin AutoCAD 2027 tổng hợp từ các bài review của Graitec, Hagerman, CAD Masters, Symetri và Arkance (03–09/2026) **[đối chiếu trang What's New chính thức ở Giai đoạn 0]**.
- Định dạng DWG: [Wikipedia — .dwg](https://en.wikipedia.org/wiki/.dwg) (AC1032 dùng cho AutoCAD 2018–2027) · [Đặc tả DWG miễn phí của ODA, v5.4.1](https://opendesign.com/files/guestdownloads/OpenDesign_Specification_for_.dwg_files.pdf) · [ACadSharp (MIT)](https://github.com/DomCR/ACadSharp).
- So sánh Mac/Windows: bài hỗ trợ "Compare Features: AutoCAD for Windows against AutoCAD for Mac" của Autodesk (10/02/2025). Mac thiếu Express Tools, Sheet Set Manager, .NET/ObjectARX, VBA, render tích hợp **[cần xác minh từng mục]**.
