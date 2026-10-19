# Bàn giao · Đợt 4 (v1.0) · thiết kế UI TrionCAD

Viết lúc 19/10/2026 14:55, khi phiên làm việc đã dùng hơn nửa ngữ cảnh. Người nhận là phiên kế tiếp (Claude) làm tiếp Đợt 4. Xóa tệp này khi Đợt 4 đã đóng.

## 1. Đọc trước
- Bộ nhớ dự án: `~/.claude/projects/-home-ndmt1at21-trion-cad/memory/ui-design-project.md` (quyết định của nhà sáng lập, trạng thái các đợt, link artifact).
- `design/README.md`: cấu trúc prototype, quy ước. Đặc tả: `docs/07-dac-ta-ui-1/2/3`. Phạm vi Đợt 4: 07.3 Phụ lục B dòng 677.
- Phụ lục cuối tệp này: giao việc chung đã gửi cho các nhóm (phong cách, API, quyền sửa tệp, cách kiểm tra).

## 2. Phạm vi Đợt 4
38 mã v1.0: C02, C04, C05, C11, D04, D12, D13, F02–F04, F06–F08, F11, F13, F14, G04, G08, G13, G14, G16–G19, H07, I05, I06, J06–J10, K06, K08, K13, B11, B14, L15. Thêm: H11 phần còn lại, J15 phần nhập, J05 trình sửa đầy đủ, J12 phần v1.0, tab Mở rộng P1, bản macOS (07.1 §13.6), và các lệnh v1.0 chỉ chạy trên dòng lệnh (theo bảng UI-E10).

Chia việc: lead (phiên này) làm khung ứng dụng; 6 agent nền làm song song, mỗi nhóm một tệp riêng.

| Nhóm | Tệp | Phạm vi |
|---|---|---|
| A | `js/screens9.js` | D04 kiểu bảng, F06 chèn bảng, F07 tab Ô bảng (được sửa khối `case 'table'` trong canvas.js), F08 field + UPDATEFIELD (nối nút Field ở annot.js `mte-field` và hộp ATTDEF ở screens7.js ~dòng 163), F02–F04 (menu Đoạn, Cột, nhập .txt trong annot.js ~dòng 90–110), F11 OBJECTSCALE, F13 QLEADER + Settings, F14 TOLERANCE (đối tượng `tol` qua TC.flatExt) |
| B | `js/screens10.js` | G04 BEDIT/BSAVE/BCLOSE, G08 BATTMAN + ATTSYNC, G13 PDFATTACH (đối tượng `pdfu`, tab ngữ cảnh PDF, ULAYERS, UOSNAP), G14 PDFIMPORT, G16 BLOCKREPLACE (TCBLOCKMATCH chỉ là lối vào khóa, sau v1.0), G17 ADCENTER (Ctrl+2), G18 TOOLPALETTES (Ctrl+3), G19 REFEDIT/REFSET/REFCLOSE, tab Mở rộng P1: BURST, NCOPY, XPLODE, GATTE, BCOUNT, TXTEXP, FLATTEN, EXTRIM, MPEDIT, SAVEALL. Được sửa screens7.js ~dòng 129, 219, 265 |
| C | `js/screens11.js` | H07 TCSHEETS (palette), J05 phần v1.0 (IMPL.TCTITLE ở screens7.js), J06 TCFRAME, J07 TCNUMBER, J08 TCINCCOPY, J09 TCTEXT, J10 TCSYMBOL, H11 còn lại (MVIEW Đa giác/Đối tượng/Fit, VPCLIP, LAYVPI, CHSPACE, EXPORTLAYOUT, menu tab layout screens7.js ~dòng 398) |
| D | `js/screens12.js` | I05 TCROOMAREA, I06 DATAEXTRACTION, J15 nhập TCATTIN (lối Xuất ↔ Nhập ở screens4.js), J12 phần v1.0 (screens7.js ~457–477), B11 ETRANSMIT, B14 TCBATCH (tổng quát hóa SC.fontBatch ở screens4.js, Viewer Pro giữ nguyên J03), sửa câu screens8.js ~dòng 272 |
| E | `quan-tri.html` (trang web riêng, như thanh-toan.html) | L15 cổng quản trị doanh nghiệp + giao diện máy chủ floating |
| F | `js/screens13.js` | Lệnh v1.0 trên dòng lệnh: GRADIENT, REGION, WIPEOUT, DONUT, MLINE, CENTERMARK, CENTERLINE, ARRAYPATH, BLEND, ALIGN, SPLINEDIT, REVERSE, PASTESPEC, GROUP, UNGROUP, GROUPEDIT, PICKSTYLE, DIMARC, DIMJOGGED, DIMORDINATE, QDIM, DIMBREAK, DIMSPACE, MLEADERALIGN, MLEADERCOLLECT, SCALETEXT, JUSTIFYTEXT, TXT2MTXT. Được sửa screens5.js ~370, screens6.js ~75, 81, 144, 181, 206 |

Mỗi nhóm được dặn: không sửa tệp của lead (`cmd.js`, `catalog.js`, `index.html`, `shell.js`, `ribbons.js`, `palettes.js`, `canvas.js` trừ khối được giao, `README.md`); CSS chỉ thêm giữa hai dòng đánh dấu của nhóm trong `css/app.css`; biểu tượng giữa dấu của nhóm trong `js/icons.js`; đăng ký `OPEN['UI-xxx']` trong tệp của mình; gửi báo cáo cuối (đã làm gì, mặc định tự chọn, yêu cầu sửa tệp lead, ảnh tiêu biểu).

### Trạng thái lúc 14:55 (đo bằng `tools/snap.js`, xem mục 7)
- Cả 6 nhóm vẫn đang chạy; chưa nhóm nào gửi báo cáo. Kích thước tệp: screens9 95 KB, screens10 145 KB, screens11 140 KB, screens12 78 KB, screens13 109 KB, quan-tri.html 158 KB.
- Đã có `OPEN`: nhóm C (H07, H11, J05–J10), nhóm D (I05, I06, J12, J15, B11, B14). Nhóm A, B chưa đăng ký `OPEN`.
- Lệnh Đợt 4 chưa có IMPL: QLEADER, FIELD, UPDATEFIELD, OBJECTSCALE, TOLERANCE (A); REFEDIT, REFSET, REFCLOSE, BURST, NCOPY, GATTE, TXTEXP, FLATTEN, EXTRIM, SAVEALL, XPLODE, MPEDIT, BCOUNT (B); TCASE, TCOUNT (chưa giao ai, xem mục 5). Nhóm F đã có IMPL cho mọi lệnh của mình.
- Danh mục chạy 130 màn hình, không lỗi JS.

**Nếu phiên này đã kết thúc:** agent nền không còn liên lạc được và báo cáo của chúng bị mất. Đánh giá từng nhóm bằng tệp trên đĩa: chạy `snap.js`, mở từng mã trong danh mục, chụp ảnh. Phần nào thiếu thì làm tiếp, hoặc chạy lại một agent với giao việc ở phụ lục cộng phạm vi nhóm trong bảng trên (dặn đọc tệp hiện có và làm tiếp, không viết lại).

## 3. Phần lead đã xong (`js/screens14.js`, đã kiểm tra bằng ảnh chụp, Tối và Sáng, 1366)
- **C02 Thuộc tính nhanh:** popup `.qp` cạnh con trỏ, trường theo loại, sửa qua `TC.propEdit`, tự thu gọn sau 3 giây; nút `qp` trên thanh trạng thái (Ctrl+Shift+P, chuột phải = thiết lập); nhấp đúp line/arc/circle/ellipse/point/xline/ray mở popup (bọc `TC.cad.hook.dbl`).
- **C04** `SC.layerStates`, `SC.layerStateNew` (LAYERSTATE; mục “Lưu trạng thái layer…” trong menu chuột phải palette Layer). **C05** bộ lọc thuộc tính và nhóm: cây trong Quản lý layer (sửa `screens1.js`), `SC.layerFilterEdit`, kéo layer vào nhóm, “Đảo bộ lọc”; lọc qua `TC.layerFilterTest` (gọi từ `layerRows` ở palettes.js). **C11** `SC.layWalk` (LAYWALK, không modal; bọc `TC.layerVisible` bằng `doc.walk`).
- Lệnh layer v1.0: LAYMRG, LAYDEL, LAYCUR, COPYTOLAYER (hoàn tác qua `SC.layerRun`).
- **D12** `SC.viewManager`, `SC.viewNew` (IMPL.VIEW thay bản Viewer; Viewer chỉ Đặt hiện hành). **D13** `SC.ucsMan`, UCSICON (+ hộp thuộc tính biểu tượng), NAVVCUBE (ViewCube `#vcube`), 3DORBIT (thông báo bản vẽ 2D), menu điều khiển viewport `TC.vpcMenu`.
- **K06** `SC.shortcuts` (TCKEYS): bảng `KDEF` là nguồn duy nhất; gán bằng nhấn tổ hợp, báo trùng, chặn tổ hợp hệ thống; bảng in một trang `cheatSheet`; bộ phân phối phím chạy trước shell (phím mới: F5/Ctrl+E, Ctrl+G/L/B/U/W/R/Q, Ctrl+2/3, Ctrl+Shift+P; phím người dùng lưu `tc-proto-keys`).
- **K08** `TC.WS`, `TC.wsMenu` (nút bánh răng thanh trạng thái), WSSAVE, WSSETTINGS, WORKSPACE.
- **K13** `TC.SV` (51 biến, kiểm tra kiểu và miền, một số gắn trạng thái thật), `SC.sysvars('all'|'mon')` (SYSVDLG, SYSVARMONITOR), SETVAR; gõ tên biến trên dòng lệnh → SETVAR (sửa `cmd.js`); chip `#sb-sysv` “N biến đã đổi” + popover khôi phục; thông báo gom nhiều biến.
- **macOS** `TC.setPlatform('mac'|'win')` (Prototype › Nền tảng): `#macbar` thanh menu hệ thống, nút cửa sổ trái, `TC.keyLabel` đổi Ctrl → ⌘/⌃ trong menu và tooltip, hộp thoại tệp và in dạng `.sheet` (đổi đường dẫn sang kiểu macOS), “Tùy chọn” → “Cài đặt”, kéo hai ngón để pan, toàn màn hình.
- Thêm ngoài 38 mã: chia ô hai bản vẽ `SC.tile('v'|'h')` (07.1 §9.15; menu Cửa sổ cổ điển, ribbon Hiển thị › Giao diện › Sắp cửa sổ); phiên bản Doanh nghiệp trong menu Prototype; TCMIGRATE nhập CUIX một phần (screens8.js); thanh trạng thái thêm Hiển thị chú thích, Tự thêm tỉ lệ, Đơn vị, Cô lập đối tượng; styleguide có mục `#dot4` (phần của lead).
- `OPEN` của lead: UI-C02, C04, C05, C11, D12, D13, K06, K08, K13, và hai mục ngoài mã `UI-MAC`, `UI-MR` (danh mục hiện ở cuối nhóm Đợt 4).

### Hạ tầng chung lead đã thêm (để biết khi ghép)
- `canvas.js`: `TC.flatExt[t]` cho loại đối tượng mới; `palettes.js`: `TC.propsExt[t]`, `TC.propRows(d, e)`; `cad.js`: `TC.cad.ctxExt` (thêm mục menu chuột phải).
- `cmd.js`: RAW thêm ~27 lệnh Đợt 4 (UPDATEFIELD, OBJECTSCALE, TOLERANCE, BLOCKREPLACE, TCBLOCKMATCH (đợt 5), XPLODE, MPEDIT, BCOUNT, TCASE, TCOUNT, SYSVDLG, TCKEYS, WSSAVE, WSSETTINGS, TCATTIN, BCLOSE, BSAVE, REFCLOSE, REFSET, ULAYERS, UOSNAP, VPCLIP, UCSICON, NAVVCUBE, 3DORBIT).
- `catalog.js`: `Object.assign(OPEN, SC.open4)`; `SC.cat = { D1, PRO, MOD, RUN, plan, pick, flash }`; mảng `EXTRA` cho UI-MAC, UI-MR.
- `index.html` nạp screens9–14 trước catalog.js. `shell.js` phát `TC.emit('title')` cuối renderTitle, `'banners'` cuối renderBanners, `'toggle'` khi bật/tắt.
- `tokens.css`: `--c-mac-r/y/g`. `ribbons.js`: nút Phím tắt dùng TCKEYS; Trình quản lý view mở `SC.viewManager`.

## 4. Việc còn lại (theo thứ tự)
1. Nhận báo cáo 6 nhóm (hoặc tự đánh giá theo mục 2). Xem ảnh tiêu biểu của từng nhóm (`scratchpad/shots/<nhóm>/`), mở từng mã, sửa lỗi hình thức.
2. Áp các yêu cầu sửa tệp lead mà các nhóm ghi trong báo cáo. Đã biết trước:
   - dropdown Kiểu bảng trên ribbon (`ribbons.js` dòng 75, `bstyleDD`) đang dùng danh sách cố định `['Standard','TCVN-thong-ke']`: nối với hàm nhóm A cung cấp (ví dụ `SC.tableStyleNames(d)`);
   - `IMPL.TCASE` mở TCTEXT ở thẻ đổi hoa/thường, `IMPL.TCOUNT` mở TCNUMBER (cùng tab Mở rộng; nếu nhóm B, C chưa làm thì thêm một dòng mỗi lệnh);
   - `OPEN['UI-L15']` → `location.href = 'quan-tri.html'` (trong catalog.js, giống UI-L16).
3. Ghép và kiểm tra xung đột: screens7.js do B, C, D cùng sửa (vùng khác nhau); canvas.js khối table (A); screens4.js (D); annot.js (A). Chạy `node -e` kiểm cú pháp mọi tệp, `catrun.js` phải in `no errors`.
4. `catalog.js`: đổi trạng thái 38 mã Đợt 4 sang `d` (cột thứ năm trong RAW), kiểm danh mục đếm đúng.
5. `README.md`: bảng Mở (thêm `quan-tri.html` UI-L15), nút Prototype (Nền tảng, Doanh nghiệp), Cấu trúc (screens9–14), Tiến độ (Đợt 4: 38/38), quy ước mới (phím tắt một nguồn, macOS, lệnh v1.0 đã chạy).
6. Styleguide `#dot4`: thêm thành phần của các nhóm (ô bảng đang chọn, khung dung sai, nền Block Editor, PDF underlay, palette DesignCenter/Tool Palettes, bộ bản vẽ…), chụp kiểm Tối/Sáng.
7. Đăng lại prototype lên artifact cũ (memory có URL; đường đăng `scratchpad/publish/proto/index.html` dựng bằng `strip.sh`; scratchpad của phiên trước có `tools/strip.sh`), cập nhật memory, báo nhà sáng lập kèm danh sách mặc định cần duyệt (mục 6) và hỏi trước khi sang Đợt 5. Không commit nếu chưa được yêu cầu.

## 5. Mặc định lead đã tự chọn (cần hỏi nhà sáng lập)
- macOS: phím thay thế cho F3–F12 khi laptop không có phím F riêng là đề xuất: ⌃F bắt điểm, ⌃E, ⌃G, ⌃L, ⌃B, ⌃U (giống Ctrl của Windows), ⌃K theo dõi, ⌃D nhập động, ⌘? trợ giúp, ⌃⌘H lịch sử. Thanh menu không vẽ logo Apple. Có chỉ báo nguồn nhập “VI”.
- OSMODE mặc định 255 (8 chế độ hay dùng), khác 4133 của AutoCAD; danh sách theo dõi biến mặc định 10 biến (OSMODE, CMDECHO, FILEDIA, PICKFIRST, ORTHOMODE, ATTREQ, ATTDIA, PEDITACCEPT, MIRRTEXT, PICKADD).
- Phím đơn (không kèm Ctrl/Alt, không phải F) không gán được ở K06, hướng sang Viết tắt lệnh; tệp xuất phím tắt `.tckeys`.
- Thuộc tính nhanh: tắt mặc định, vị trí theo con trỏ, tự thu gọn bật; QPMODE chỉ 0/1.
- Workspace có sẵn không ghi đè được; 2 workspace mẫu của người dùng.
- Bộ lọc layer: các hàng điều kiện kết hợp kiểu “hoặc”; Duyệt layer mặc định khôi phục khi đóng.
- View chuẩn ngoài “Trên”, ViewCube, xoay 3D chỉ báo “bản vẽ 2D” (mẫu không có đối tượng 3D).
- Chia ô chỉ 2 bản vẽ (dọc hoặc ngang), chưa có xếp lớp kiểu cửa sổ nổi.

## 6. Mặc định của phiên trước vẫn hiệu lực
Xem memory (Đợt 2, Đợt 3).

## 7. Công cụ
Thư mục `T=/tmp/claude-1000/-home-ndmt1at21-trion-cad/39787da2-be92-4423-895c-4298b7b6a6a5/scratchpad/tools` (playwright-core trong `node_modules`, trình duyệt ở `browsers` là link tới `…/2134aa50-bad7-490f-9728-ce218ece3bf6/scratchpad/tools/browsers`). Luôn chạy với `PLAYWRIGHT_BROWSERS_PATH=$T/browsers`.
- `node app.js <tiền-tố-ảnh> 1920x1080 dark '<bước JSON>'`: chụp theo kịch bản (`js`, `click`, `mouse`, `key`, `type`, `wait`, `shot`).
- `node catrun.js`: mở mọi mã trong danh mục, in lỗi JS.
- `node snap.js`: lệnh Đợt 4 chưa có IMPL, mã đã đăng ký `OPEN`.
- `node sgshot.js <ảnh> <id mục> dark|light`: chụp một mục của styleguide.
- Nếu `/tmp` đã bị dọn: cài lại `playwright-core` và Chromium vào scratchpad mới.

## Phụ lục
### Giao việc chung cho các nhóm (bản sao của scratchpad/BRIEF-dot4.md)

Bạn là một trong 6 người làm song song Đợt 4 của prototype giao diện TrionCAD. Người điều phối (lead) làm phần khung và ghép cuối. Đọc hết tệp này trước khi đọc mã.

### Bối cảnh
- TrionCAD là phần mềm CAD 2D cho người Việt (thay AutoCAD), lõi C++ tự viết. Prototype ở `/home/ndmt1at21/trion-cad/design/prototype/` là **bản thiết kế UI tương tác** để nhà sáng lập duyệt; mỗi màn hình phải trông và hành xử như sản phẩm thật (độ trung thực cao), không phải mã sản xuất.
- Đặc tả: `/home/ndmt1at21/trion-cad/docs/07-dac-ta-ui-1-khung-ung-dung.md` (khung, ribbon §7, dòng lệnh §8, vùng vẽ §9, thanh trạng thái §10, phím §11, thông báo §12, chuẩn §13), `07-dac-ta-ui-2-bang-va-hop-thoai.md` (hộp thoại, palette), `07-dac-ta-ui-3-tinh-nang-vn-va-he-thong.md` (tính năng VN, hệ thống). Phụ lục C ở cuối 07.3 là danh sách kiểm cho mỗi màn hình. Bảng prompt lệnh: 07.2 §5 UI-E10.
- `design/README.md` có cấu trúc thư mục và quy ước. Đọc nó.
- Đợt 0–3 đã xong (117 màn hình). Đợt 4 là v1.0: 38 mã + phần còn lại của vài mã cũ.

### Phong cách (hướng A · Than chì, đã chốt)
- Màu chỉ dùng token trong `css/tokens.css` (`var(--c-…)`), không mã màu trực tiếp. Hai chủ đề Tối/Sáng phải đúng.
- IBM Plex Sans 12 px, IBM Plex Mono cho số và tên lệnh; mật độ gọn (hàng 22 px); bo góc 2 px; không bóng đổ, không blur.
- Biểu tượng tự vẽ trên lưới 16 px, nét 1,25 đầu vuông (xem `js/icons.js`), **không giống biểu tượng Autodesk, không emoji**.
- Chữ tiếng Việt có dấu đầy đủ, câu ngắn, xưng hô trung tính, không đổ lỗi người dùng, luôn chỉ cách khắc phục. Tên lệnh tiếng Anh hiện trong tooltip/tiêu đề (`cmd`). Nhãn chip tùy chọn: "Bán kính · R". Số trong bản vẽ dùng dấu chấm thập phân (`TC.d`), số trên giao diện kiểu Việt (`TC.n`), ngày dd/mm/yyyy (`TC.date`).
- Quy ước 07.2: palette áp dụng ngay, mỗi lần sửa là một bước hoàn tác; hộp thoại modal áp dụng khi bấm nút xác nhận mang **tên hành động** ("Chèn bảng", "Thay 12 block"), không dùng "OK"; danh sách dài có ô tìm và sắp xếp; xem trước trực tiếp trên vùng vẽ khi có thể; hành động phá hủy có xác nhận nêu số lượng; thao tác > 2 giây có tiến trình và nút hủy (tác vụ nền: thanh trạng thái `A().setTask`, toast khi xong).
- Mỗi màn hình có đủ trạng thái liên quan: mặc định, rỗng, lỗi/kiểm tra hợp lệ, đang chạy, **bị khóa theo phiên bản** (Viewer/Viewer Pro thấy huy hiệu, bấm mở trang nâng cấp). Dùng được bằng bàn phím (Tab, Enter, Esc, ↑↓). Vừa màn hình 1366×768.

### Mã: quy ước và API có sẵn (đọc mã để biết chi tiết)
- JS thuần, script thường (không module, không build), chạy được qua `file://`. Mỗi tệp là một IIFE dùng namespace `TC`. Viết gọn, cùng phong cách dày đặc của các tệp Đợt 3 (`js/screens5–8.js`, `draw.js`, `annot.js`, `editor.js`). Chú thích tiếng Việt, ít và đúng chỗ.
- Tệp khung của bạn đã tạo sẵn (`js/screensN.js`), đã nạp trong `index.html` trước `catalog.js`. Đầu tệp có sẵn các biến: `TC, $, $$, esc, ic, G (TC.geo), IMPL (TC.cmd.impl), ED (TC.ed), SC (TC.screens), H (SC.h)`, `A()`, `V()`, `D()`, `OPEN` (= `SC.open4`).
- **Lệnh:** `IMPL.TENLENH = function(ctx, preset){ … }`. Lệnh phải có trong bảng `RAW` của `js/cmd.js` (lead đã thêm các lệnh Đợt 4; thiếu lệnh nào thì báo lead, đừng tự sửa cmd.js). `ctx`: `out(html)`, `point(text, {opts, base, onMove(p, temp), rubber, pick})` → `{p}|{kw}|{enter}|{num}`, `sel(text)` → ids, `kw(text, opts, def)`, `val(text, def)`, `badge('XX')`… Xem `cmd.js` hàm `ctxFor` và cách dùng trong `draw.js`.
- **Sửa bản vẽ có hoàn tác:** `ED.run('TÊN', fn)` gom thành một bước; trong đó `ED.add(ED.make({t:…}))`, `ED.mod(e, fn)`, `ED.del(e)`, `ED.state(get, set)` cho dữ liệu không phải đối tượng (layer, kiểu, bảng…). Sau khi đổi: `V().draw()`; `TC.emit('sel')` khi đổi vùng chọn; `A().refreshPalettes()`.
- **Mô hình bản vẽ** (`js/scene.js`): `d.ents` (Model), `d.layouts[i].{name, paper, ents, vports}`, `d.blocks[name].ents`, `d.layers/d.layerMap`, `d.styles/d.styleMap`, `d.dimstyles`, `d.views`, `d.xrefs`, `d.space` ('model' hoặc chỉ số layout), `d.sel` (id), `d.byId`. Hai bản vẽ mẫu: `d1` Banve-01.dwg (nhà 2 tầng, phòng có chữ tên phòng, khung tên KHUNG-A3/A1 có attribute) và `d2` Ketcau-T2.dwg.
- **Loại đối tượng** vẽ trong `js/canvas.js` hàm `flat` (line, pline, circle, arc, ellipse, spline, text, mtext, dim, dimr, dima, mleader, insert, hatch, image, table, vpframe, point, xline, ray, attdef). Loại mới: đăng ký `TC.flatExt.loaimoi = function(x){ x.o({k:'seg', a:x.P(p1), b:x.P(p2)}); x.o({k:'text', p:x.P(p), h, s, rot, al, style}); x.sub(entCon, transform) }` ghép từ nguyên tố có sẵn (`seg, arc, circ, ell, text, tick, tri, hatch, img, pt`). Hàng thuộc tính cho palette Thuộc tính: `TC.propsExt.loaimoi = function(d, e, f){ return [['Tên', 'giá trị', 'ro'|'num'|'html'], …]; }` và `TC.TYPE_NAMES.loaimoi = 'Tên hiển thị'`. `TC.propRows(d, e)` trả mọi hàng của một đối tượng.
- **Hộp thoại:** `TC.dialog({ id, title, cmd, code:'UI-xxx', w, h, body, foot, flush, onClose, onOpen, focus })` → `{el, $, $$, close}`. Tiện ích dựng: `H.tabs/pane/row/chk/rad/sel/num/cv/gh/grp/later` (`js/screens5.js` đầu tệp), `TC.numField` (ô số chuẩn UI-C09), `SC.ask`, `TC.confirm({title,msg,detail,ok,danger,dontAsk})` (Promise), `TC.toast(html,{type,act,ms,log})`, `TC.menu(items,x,y,opts)`, `TC.listKeys`. Lớp CSS dùng lại: xem `css/app.css` (`.dlg`, `.frow`, `.grp`, `.gh`, `.tbl`, `.tree`, `.tn`, `.seg`, `.tabs`, `.chip`, `.tag`, `.btn`, `.ib`, `.inp`, `.empty`, `.note`, `.pal-tools`, `.search`…). Ưu tiên lớp có sẵn; chỉ thêm lớp mới khi cần.
- **Palette dock:** `TC.PALETTES.id = { name, render(host, d) }`, mở bằng `A().showPalette(id)`.
- **Tab ngữ cảnh ribbon:** `A().setCtxTab({ id, name, panels:[…] })` (cấu trúc panel như `js/ribbons.js`; ví dụ `SC.layoutTab`, `SC.imageTab` trong `screens7.js`, tab Hatch trong `screens6.js`). Bộ chọn tab theo vùng chọn là `ED.ctxRefresh` (`screens6.js`): muốn thêm loại thì bọc lại trong tệp của bạn: `var base = ED.ctxRefresh; ED.ctxRefresh = function(){ …; base(); };`. Tab "sở hữu" (đang trong một chế độ như Block Editor) thì giữ nguyên tab khi vùng chọn đổi.
- **Nhấp đúp theo loại đối tượng:** bọc `TC.cad.hook.dbl` (`annot.js` cuối mục 9.10) trong tệp của bạn, gọi lại hàm cũ cho loại không thuộc bạn.
- **Menu chuột phải trên vùng vẽ** (khi có vùng chọn): `TC.cad.ctxExt.push(function(it, first, d){ if (first && first.t === 'insert') it.push({ label:'…', act:function(){…} }); })`.
- **Bảng (`t:'table'`)** đã có từ Đợt 3: `{ t:'table', p:[x,y] (góc trên trái), cols:[rộng…], rows:[cao…], cells:[[chuỗi…]…], merge:[[hàng, cột0, cột1]…], head:số hàng tiêu đề, th:chiều cao chữ, tstyle:'tên kiểu bảng' }`. Mọi trường cũ giữ nguyên nghĩa; trường mới chỉ được thêm, là tùy chọn.
- **Phiên bản:** `TC.can('PRO'|'VP')`, `TC.isViewer()`. Lệnh trong RAW có cột `need`, `cmd.run` tự chặn và mở trang nâng cấp.
- **Danh mục màn hình:** đăng ký `OPEN['UI-xxx'] = function(){ … }` trong tệp của bạn: hàm này dựng trạng thái demo rồi mở màn hình (tự chuyển phiên bản Pro bằng `SC.cat.PRO()`/`SC.cat.MOD()`, zoom vùng phù hợp bằng `SC.cat.plan()`, chọn đối tượng bằng `SC.cat.pick(d, fn)`, chạy lệnh bằng `SC.cat.RUN('LENH')`). Xem `catalog.js` để biết mẫu. Lead sẽ đổi trạng thái mã sang "Đã thiết kế".
- **CSS:** chỉ thêm vào `css/app.css` **giữa hai dòng đánh dấu của tệp bạn** (`/* ==== Đợt 4 · screensN.js … ==== */` và `/* ==== hết screensN.js ==== */`). Biểu tượng mới: thêm vào `js/icons.js` giữa hai dòng `// ==== Đợt 4 · screensN.js …` và `// ==== hết screensN.js ====`, dạng `ten:'<path d="…"/>',`. Kiểm tra tên chưa có.
- **Không sửa** `cmd.js`, `catalog.js`, `index.html`, `shell.js`, `ribbons.js`, `palettes.js`, `canvas.js`, `README.md` (của lead). Cần đổi ở đó thì ghi vào báo cáo cuối. Chỉ sửa tệp Đợt 0–3 ở đúng chỗ được giao trong phần việc của bạn, sửa nhỏ, không đổi hành vi khác. Nhiều người sửa song song: luôn Read lại đoạn cần sửa ngay trước khi Edit.
- Không commit git. Không tạo tệp ngoài phạm vi được giao (trừ tệp tạm trong thư mục ảnh chụp của bạn).

### Kiểm tra (bắt buộc)
Công cụ trình duyệt không giao diện ở `$T=/tmp/claude-1000/-home-ndmt1at21-trion-cad/39787da2-be92-4423-895c-4298b7b6a6a5/scratchpad/tools`.
- Chụp màn hình theo kịch bản: `cd $T && PLAYWRIGHT_BROWSERS_PATH=$T/browsers node app.js <tiền-tố-ảnh> 1920x1080 dark '<các bước JSON>'`. Bước: `{"js":"…"}` chạy JS trong trang, `{"click":"selector"}`, `{"mouse":[["move",x,y],["down"],["up"],["click",x,y],["dbl",x,y],["wheel",dy]]}`, `{"key":["Enter"]}`, `{"type":"chữ"}`, `{"wait":ms}`, `{"shot":"ten"}`. In ra lỗi JS. Ví dụ mở màn hình qua danh mục: `[{"js":"TC.openScreen('UI-D04')","wait":600,"shot":"d04"}]`. Kích thước `1366x768` để thử màn hình nhỏ, `light` để thử chủ đề Sáng.
- Ảnh lưu vào thư mục riêng `$T/../shots/<nhóm>/` (tự tạo). Xem ảnh bằng công cụ Read. **Xem từng màn hình bằng mắt**: chữ không tràn/cắt, thẳng hàng, khoảng cách đều, đúng chủ đề.
- Cuối cùng chạy `PLAYWRIGHT_BROWSERS_PATH=$T/browsers node catrun.js` (mở mọi màn hình trong danh mục, phải in `no errors`), và `node -e` kiểm cú pháp tệp của bạn.

### Báo cáo cuối (gửi lead, tiếng Việt, ngắn)
1. Mỗi mã: đã làm gì, mở thế nào (lệnh, ribbon, `OPEN`), phần nào để sau v1.0 theo đặc tả.
2. Các mặc định bạn tự chọn khi đặc tả chưa nói (để lead hỏi nhà sáng lập).
3. Yêu cầu sửa tệp của lead (nếu có), ghi chính xác.
4. Tên các ảnh chụp tiêu biểu (đường dẫn đầy đủ) để lead xem lại.
