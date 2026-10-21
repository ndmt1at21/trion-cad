# Thành phần giao diện cơ bản (UI0-006)

Bảng mẫu tích hợp của các thành phần UI0-106 (nút, điều khiển nhập), UI0-107 (menu, tooltip), UI0-108 (bảng, cây ảo
hóa), cùng cách chọn thành phần và các chỗ bản Qt khác `design/prototype/styleguide.html`.

## Bảng mẫu

- Mở: `trion --gallery` (thay cửa sổ chính) hoặc exe `trion_gallery [--settings <tệp>] [--ui-probe[=<tệp.json>]]`.
  Mã: `app/widgets/gallery/Gallery.h` (lớp `trion::ui::GalleryWindow`, thư viện `trion_gallery`).
- Trang, từ trên xuống: thanh chọn **Xem ở** (chủ đề Theo hệ thống/Tối/Sáng, mật độ Gọn/Thoáng, Giao diện nhẹ, ngôn
  ngữ Tiếng Việt/English/vi-XL); **Dùng thử** (mỗi thành phần một widget thật, có tooltip 2 mức; Tab đi qua đúng thứ
  tự đọc); **Trạng thái** (ma trận 13 điều khiển × 8 trạng thái); **Menu, tooltip**; **Bảng, cây** (ảnh trạng thái và
  bảng 10.000 dòng, cây 100 nhóm dựng ở nền).
- Ô trạng thái là **ảnh chụp** từ chính widget, không phải widget sống: focus chỉ có ở một widget, mỗi lúc chỉ mở được
  một popup, còn rê/nhấn đổi theo chuột thật. `stage()` dựng từng điều khiển trong vùng 0×0 của cửa sổ, đặt trạng thái,
  chụp ở DPR của màn hình rồi hủy; tự chụp lại khi đổi chủ đề, mật độ, Giao diện nhẹ, DPR; đổi ngôn ngữ thì dựng lại
  trang. Chụp cần cửa sổ đang active (ô focus); ô mở popup chụp sau cùng.
- Rảnh: không timer, không vẽ lại (gallery_test đo). Thời gian chụp lại cả trang: ~170–250 ms ở bản debug.
- Ảnh baseline cả trang (Tối/Sáng × gọn/thoáng × 100/150/200%, Linux và Windows) ở
  `app/widgets/gallery/tests/baseline`. Ghi lại: `TRION_UPDATE_BASELINE=<thư mục> gallery_test baseline`, chép
  `gallery-*-<nền tảng>.png` vào thư mục baseline, build lại (ảnh nhúng vào test).

## API tóm tắt, khi nào dùng

| Thành phần | Lớp, header | API chính | Dùng khi |
|---|---|---|---|
| Nút | `Button` (`app/widgets/controls/Controls.h`) | `Kind::Normal/Primary/Danger/Ghost/Icon`, `setControlSize(Small/Normal/Large)`, `setIconName`, `setLocked`, `setError` | Hành động một lần. Primary: hành động chính, tối đa một nút mỗi hộp thoại, mang tên hành động ("In", không "OK"), ngoài cùng bên phải. Danger: phá hủy (kèm xác nhận nêu số lượng). Ghost: nút phụ ("Hủy"). Icon: thanh công cụ; luôn `setAccessibleName` và tooltip |
| Nút tách | `SplitButton` | `setDropMenu(Menu*)`, `showDropMenu()`; Alt+↓ mở | Một hành động mặc định và vài biến thể ("In" / "In hàng loạt") |
| Nút bật/tắt | `ToggleButton` | `QAbstractButton::toggled`, `setChecked` | Chế độ bật/tắt độc lập (Lưới, Bắt điểm, Giao diện nhẹ) |
| Nhóm phân đoạn | `SegmentedGroup` | `addSegment(text, icon)` trả `Button*`, `currentIndex`, `currentIndexChanged`, `setError` | Chọn một trong 2–5 lựa chọn ngắn, luôn thấy hết (đơn vị, chủ đề). Nhiều hơn hoặc chữ dài: `DropDown` |
| Danh sách thả xuống | `DropDown` | API của `QComboBox` không sửa được; `setLocked`, `setError` | Chọn một trong danh sách ngắn, cố định (khổ giấy) |
| Combo có ô lọc | `ComboBox` | như `DropDown`; popup có ô lọc không phân biệt dấu ("lop" khớp "Lớp") | Danh sách dài hoặc người dùng biết tên (layer, kiểu chữ); 10.000 mục mỗi phím ≤ 16 ms |
| Ô tìm kiếm | `SearchField` | API của `QLineEdit`; Esc xóa chữ; `setLocked`, `setError` | Lọc danh sách, bảng đang hiện |
| Tab | `TabBar` | API của `QTabBar`; `setTabLocked(i)`, `setTabError(i)` | Chia nội dung một hộp thoại, palette thành trang; tràn thì cuộn, không rút gọn chữ |
| Thanh trượt | `Slider` | API của `QSlider`; `setLocked`, `setError` | Giá trị liên tục, không cần chính xác (độ mờ). Cần số chính xác: ô số UI-C09 (UI0-007) |
| Menu | `Menu` (`app/widgets/menu/Menu.h`) | `addSection`, `addAction`, `addMenu(new Menu)`; phím, khóa, lệnh qua `RichTooltip::attach(action, TipInfo)` | Menu thả xuống, menu ngữ cảnh, menu của nút tách. Phím hiện từ Keymap, không từ `QAction::shortcut` |
| Tooltip 2 mức | `RichTooltip` (`app/widgets/tooltip/RichTooltip.h`) | main tạo một (`setKeymap`); `RichTooltip::attach(widget/QAction, {name, command, keyId, description, need})` | Mọi nút có lệnh: mức 1 (0,45 s) tên, lệnh tiếng Anh, phím; mức 2 (+1,5 s) mô tả, F1. Không dùng `setToolTip` cho nút có lệnh |
| Bảng ảo hóa | `VirtualTableView` (`app/widgets/views/VirtualTableView.h`) | `setModel` (thường `BackgroundModel<Row>`, `app/widgets/model`), `setStateKey`, `setSortingEnabled`, `setSearchColumn`, `setColumnHidden` | Danh sách > 200 dòng hoặc có thể lớn (DoD-U 4), nhiều cột, sắp theo cột |
| Cây ảo hóa | `VirtualTreeView` | model phẳng trả `DepthRole`, `ExpandedRole`; `setData(ExpandedRole)` để mở/đóng, model làm phẳng lại ở nền | Cây lớn (block, nhóm thuộc tính); cây nhỏ cố định vẫn dùng được |

Chung: mọi lớp là lớp con của widget Qt chuẩn (bàn phím, `QAccessible` sẵn có), tự vẽ theo token đọc trong
`paintEvent`/`sizeHint` (đổi chủ đề, mật độ là đúng ngay), không QSS. Chữ đặt qua `tr()` và đặt lại khi
`LanguageChange`; tooltip của widget có lệnh: gọi lại `attach()` với chuỗi mới.

## 8 trạng thái (07.3 Phụ lục C) theo thành phần

✓: có ô ảnh trong bảng mẫu. —: thành phần không có trạng thái đó (lý do bên dưới). Đang tải, rỗng (Phụ lục C) chỉ áp
dụng cho thành phần có dữ liệu: bảng, cây.

| Thành phần | Mặc định | Rê | Nhấn | Focus | Vô hiệu | Bật | Khóa | Lỗi |
|---|---|---|---|---|---|---|---|---|
| Nút, nút phụ, nút biểu tượng | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ (checkable) | ✓ | ✓ |
| Nút chính, nút nguy hiểm | ✓ | ✓ | ✓ | ✓ | ✓ | — (1) | ✓ | ✓ |
| Nút tách | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ menu đang mở | ✓ | ✓ |
| Nút bật/tắt | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Nhóm phân đoạn | ✓ | ✓ đoạn 2 | ✓ đoạn 2 | ✓ đoạn 1 | ✓ | ✓ chọn đoạn 2 | ✓ đoạn 3 | ✓ |
| Danh sách thả xuống, combo | ✓ | ✓ | ✓ danh sách mở | ✓ | ✓ | — (2) | ✓ | ✓ |
| Ô tìm kiếm | ✓ | ✓ | ✓ đang gõ | ✓ | ✓ | — (2) | ✓ | ✓ |
| Tab | ✓ | ✓ tab 2 | ✓ mở tab 2 | ✓ | ✓ | — (3) | ✓ tab 2 | ✓ tab 2 |
| Thanh trượt | ✓ | ✓ | ✓ đang kéo | ✓ | ✓ | — (2) | ✓ | ✓ |
| Menu | ✓ | ✓ mục đang trỏ | như rê (4) | như rê (4) | ✓ mục | ✓ dấu chọn | ✓ huy hiệu Pro | — (5) |
| Tooltip | ✓ mức 1 | ✓ mức 2 (rê thêm 1,5 s) | — (6) | — (6) | — (6) | — (6) | ✓ huy hiệu Pro | — (6) |
| Bảng, cây | ✓ | ✓ dòng | như bật (7) | ✓ khung dòng hiện hành | ✓ | ✓ dòng chọn | — (8) | — (8) |

Bảng, cây thêm: **rỗng** ✓ (chỉ đầu cột); **đang tải**: view không vẽ riêng: `BackgroundModel` giữ dòng cũ tới khi
có dòng mới và báo `busy()` để palette hiện "đang tải" (UI0-014).

1. Hành động một lần, không giữ trạng thái; cần trạng thái bật thì dùng `ToggleButton`.
2. Giá trị của điều khiển chính là trạng thái của nó (mục đang chọn, chữ, vị trí); không có "bật" riêng.
3. Tab đang mở có ở mọi ô của hàng; "nhấn" là mở tab.
4. `QMenu` chỉ có một mục đang trỏ: chuột, nhấn và phím mũi tên đều đưa tới cùng một hình.
5. Mục menu không nhận dữ liệu nên không có lỗi; mục không dùng được là vô hiệu hoặc khóa.
6. Tooltip không nhận chuột, focus, không bấm được; hiện hay ẩn theo phần tử được rê.
7. Nhấp một dòng là chọn dòng đó: hình của "nhấn" là dòng chọn.
8. Khóa, lỗi thuộc về dữ liệu trong ô (biểu tượng ổ khóa của layer, ô sai): palette vẽ bằng delegate, view không có
   API riêng.

## Khác biệt so với styleguide.html

| Chỗ khác | Lý do |
|---|---|
| Ô trạng thái là ảnh chụp, styleguide là widget HTML sống (và chỉ vài trạng thái tĩnh) | Focus, popup, rê không thể cùng lúc ở nhiều widget sống; mục Dùng thử giữ phần tương tác thật |
| Có ma trận 8 trạng thái, mục Dùng thử, chọn ngôn ngữ (Tiếng Việt/English/vi-XL) | Tiêu chí UI0-006 và QA-011 (Phụ lục C, chuỗi dài hơn 40%) |
| "Giao diện nhẹ" là nút bật/tắt, styleguide là ô đánh dấu | UI0-106 không có ô đánh dấu tự vẽ; `ToggleButton` cùng nghĩa bật/tắt |
| Tiêu đề trang 24 px (`--fs-2xl`), tiêu đề mục 18 px (`--fs-xl`); styleguide 30 px và 20 px | Không có token 30/20 px; DoD-U 2 chỉ cho dùng token |
| Khoảng giữa mục 36 px (`--sp-7` + `--sp-5`), khe trong hàng 8 px (`--sp-4`); styleguide 48 px, 10–14 px | Không có token 10/14 px; khoảng giữa mục rút ngắn để ảnh baseline nhỏ hơn |
| Không có mục Màu, Chữ, Kích thước, Biểu tượng, Hình khối, ô chữ (`.inp`), ô đánh dấu, radio, ô số UI-C09, hộp thoại, thông điệp, ribbon, palette, dòng lệnh… | Ngoài phạm vi UI0-006 (thành phần của UI0-106/107/108); token, chữ, biểu tượng có test riêng (UI0-002/003/004); ô số là UI0-007 |
| Nút khóa: ổ khóa trong nút, không có huy hiệu "Viewer Pro" cạnh chữ | Hình khóa của `Button::setLocked` (UI0-106); huy hiệu phiên bản chỉ ở tooltip và menu (UI0-107) |
| Danh sách thả xuống không có ô màu (`.sw`) trước "KT-CUA" | Dữ liệu mẫu không có màu; `DropDown` vẽ biểu tượng của mục khi model có |
| Tab: 2 tab ("Chung", "Thống kê"); styleguide 3 tab | Ma trận vừa màn hình 1366 px |
| Nhóm phân đoạn 3 đoạn (mm, m, in); styleguide 2 (mm, m) | Cần đoạn thứ ba để chụp "khóa một đoạn" cạnh đoạn đang bật |
| Có thanh trượt (styleguide không có) | UI0-106 có `Slider` |
| Bảng mẫu 2 cột (Tên, Số); styleguide có cột biểu tượng Bật, Băng, Màu và huy hiệu ✓ ở dòng hiện hành | Các cột biểu tượng, huy hiệu là delegate của palette Layer (UIC-*), không phải của view |
| Mục menu "Lặp lệnh DIST" không có phím "Enter" | Phím của mục chỉ lấy từ Keymap; Keymap chưa có hành động lặp lệnh |
| Tooltip mức 1 "CIRCLE", styleguide "CIRCLE · C" | `RichTooltip` chưa hiện bí danh lệnh (ponytail của UI0-107) |
| Không có điều hướng bên trái (mục lục) | Trang Qt cuộn một cột; mục lục là phần của trang HTML |

## Lỗi đã thấy ở thành phần (cho người điều phối)

- `SegmentedGroup` (UI0-106): Tab vào nhóm dừng ở đoạn **đã focus lần trước** (cơ chế `fixFocusPolicy` của Qt chỉ
  chạy khi một đoạn nhận focus), không phải đoạn đang bật: trước lần focus đầu, mọi đoạn đều là điểm dừng Tab, và
  `setCurrentIndex()` bằng mã không dời điểm dừng. Bảng mẫu tạm sửa bằng `tabStopOnCurrent()` trong `Gallery.cpp`.
