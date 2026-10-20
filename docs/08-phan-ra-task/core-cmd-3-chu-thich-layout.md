# CMD-3 · Lệnh chú thích và layout

> Mục 1–7: tìm và thay, kiểu chữ, TEXT, MTEXT và trình soạn tại chỗ, công cụ chữ (kể cả Express), chính tả, multileader, leader, bảng, field, hyperlink (CMD-301…CMD-399). Mục 8–14: kích thước, layout, viewport, tỉ lệ chú thích (CMD-401…CMD-499).
> Mã: `commands/annot/`, `commands/layout/`. Hợp đồng chung: [README](README.md) §2–§5.
> Căn cứ: [03](../03-chuc-nang-autocad.md) §7, §9, §28 · [05](../05-kien-truc-ky-thuat.md) §1.2, §8, §10 · [07.1](../07-dac-ta-ui-1-khung-ung-dung.md) §7.4, §7.11, §9.10 · [07.2](../07-dac-ta-ui-2-bang-va-hop-thoai.md) UI-D01, UI-D03, UI-D04, UI-E10, UI-F01–F15.

## 0. Ghi chú thiết kế

- **Thư mục.** Mỗi nhóm lệnh một thư mục `commands/annot/<nhóm>/` hoặc `commands/layout/<nhóm>/`; kiểm thử ở `tests/`, benchmark ở `bench/` của nhóm **[đề xuất]**.
- **Đặc tả.** Task có **Lệnh:** xuất `docs/specs/commands/<TÊN>.md` cho từng tên (05 §10): prompt, từ khóa, sysvar, trường hợp biên, quy tắc hình học. Không có đặc tả thì không merge. Các task không nhắc lại.
- **Prompt.** Theo 07.2 UI-E10: chip "nhãn Việt · chữ tắt tiếng Anh"; chữ tắt tiếng Anh và từ khóa `_` luôn chạy. Bản `-` (-STYLE, -MTEXT, -TABLE, -HYPERLINK, -DIMSTYLE…) giữ đúng prompt AutoCAD.
- **KTL.** "Đạt KTL" = qua bộ kiểm thử lệnh chuẩn CMD-001, CMD-002: cùng hash bản vẽ qua bàn phím, `.scr`, `(command ...)`; đúng 1 mark undo mỗi lần chạy; Esc ở mọi prompt không đổi bản vẽ; PICKFIRST = 0/1. Ca vàng tự soạn đến khi có đáp án QA-007.
- **Lệnh chỉ có hộp thoại** (FIND, MLEADERSTYLE, TABLESTYLE, FIELD, SPELL, DIMSTYLE…): lõi là engine có API, kiểm thử qua API. Gọi từ `.scr`/`(command)` mở cùng hộp thoại; trong `trion-console` báo "lệnh cần giao diện", mã thoát ≠ 0, không treo **[đề xuất]**.
- **Mã đặc biệt** `%%c %%d %%p %%u %%o %%%`, `%%nnn`, `\U+xxxx`, `\M+`: dùng bộ giải mã của RGN-007 và DWG-028, không viết lại. CMD-301 ánh xạ vị trí hiển thị ↔ chuỗi gốc; trình soạn hiện ký hiệu ngay khi gõ (CMD-308, CMD-310).
- **Trình soạn** (README §4.2): CMD sở hữu buffer, caret, vùng chọn, áp sự kiện bộ gõ (lớp IME là UIA-106, không làm lại), undo trong trình soạn, thao tác trên cây RGN-008; dàn trang của RGN-007, RGN-009; tab ngữ cảnh, thước, widget Qt thuộc UIF. Kết thúc soạn = 1 transaction DOC-007 = 1 mark undo. Ngân sách: phím → khung hình ≤ 16 ms p99 trên MTEXT 2.000 ký tự, phần lõi ≤ 4 ms **[đề xuất]**. Chữ kích thước, mleader, ô bảng dùng cùng trình soạn.
- **Tiếng Việt.** Lưu Unicode NFC (NT-012). Tìm, chính tả, đổi hoa/thường giữ đúng dấu. Chữ TCVN3/VNI đọc qua VN-001, VN-002; khi sửa thì ghi lại đúng bảng mã gốc hoặc từ chối, không tạo chữ hỏng.
- **Không tự sinh hình.** Lệnh chỉ đổi dữ liệu qua DOC-007; hình do RGN (007, 009, 014–017, 025, 026) sinh lại. Định dạng số chỉ qua RGN-013. Grip của mleader, bảng, kích thước, viewport đăng ký qua INT-013 trong task của lệnh đó. Đối tượng annotative tạo theo CANNOSCALE qua DOC-021.
- **Hiệu năng.** Quét toàn bản vẽ (FIND, UPDATEFIELD, SPELL) chạy song song trên snapshot DOC-009 qua NT-005, hủy được, tiến trình qua NT-013. Jig ≤ 8 ms/khung, hoàn tất lệnh ≤ 20 ms (DoD-L). Viewer chỉ có FIND (tìm) trong nhóm chữ (INT-020).

## 1. Chữ phẳng, so khớp và tìm kiếm

### CMD-301 · Chữ hiển thị phẳng có ánh xạ vị trí cho mọi loại chú thích
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RGN-007, RGN-008, RGN-014, RGN-015, DWG-014, DWG-028, DOC-005, DOC-012, VN-001, VN-002, VN-003, QA-110
- **Đầu vào:** 03 §7 (mã đặc biệt); 07.2 UI-F09 (loại chữ); giải mã `%%` của RGN-007, `\U+`/`\M+` của DWG-028; cây MTEXT RGN-008; chữ số đo RGN-014, RGN-015; nhận diện bảng mã VN-002.
- **Đầu ra:**
  - `commands/annot/textflat/`: `FlatText flatten(EntityView, FlatOptions)` → chuỗi hiển thị NFC + bảng span (đoạn phẳng ↔ đoạn chuỗi gốc; cờ "sinh tự động" cho số đo `<>`, field, số thứ tự bullet).
  - Nguồn: TEXT, ATTRIB, ATTDEF (tag, prompt, mặc định), MTEXT, chữ kích thước, MLEADER, ô bảng, hyperlink (URL, mô tả trong xdata `PE_URL`).
  - Fuzz target, benchmark.
- **Tiêu chí nghiệm thu:**
  1. ≥ 500 ca vàng (mọi mã `%%`, `%%nnn`, `\U+`, `\M+`, `{}` lồng, xếp chồng, field `%<…>%`): chuỗi phẳng trùng 100% dãy ký tự RGN-007/RGN-009 dựng ra; chữ kích thước trùng 100% bộ QA-110.
  2. 1 triệu ca property-based: thay một đoạn phẳng qua span rồi phẳng lại cho đúng chuỗi mong đợi; không phá cặp `{}` hay mã định dạng.
  3. Chữ TCVN3/VNI trả Unicode NFC kèm bảng mã gốc; bộ mẫu VN-003 ≥ 99% đoạn trùng bản Unicode chuẩn.
  4. ≥ 20 triệu ký tự/s/luồng trên MTEXT của BENCH-STD text dày **[đề xuất]**; không cấp phát trong vòng lặp.
  5. Fuzz ≥ 1 giờ trên chuỗi MTEXT và `%%` tùy ý không lỗi.
  6. Đạt DoD-C, DoD-H.

### CMD-302 · Bộ so khớp chữ: hoa thường, dấu tiếng Việt, nguyên từ, wildcard
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** NT-006, NT-012
- **Đầu vào:** 07.2 UI-F09 (tùy chọn tìm); quy tắc wildcard của `wcmatch` (`# @ . * ? ~ [ ] [~ ] -` và `` ` ``); NT-012 (gập hoa thường có dấu, NFC).
- **Đầu ra:** `commands/annot/find/matcher.{h,cpp}`: `Matcher::compile(pattern, MatchOptions)`, `findAll(FlatText, spans&)`; bảng gập dấu (đ → d, ư → u…); tiền lọc SIMD qua NT-006, có đường scalar.
- **Tiêu chí nghiệm thu:**
  1. Tắt "phân biệt dấu": "duong" khớp "đường", "ĐƯỜNG"; bật: chỉ khớp đúng dấu. 134 chữ cái có dấu (66 thường, 66 hoa, đ, Đ), cả dạng NFD, đúng 100% ca vàng.
  2. Wildcard và nguyên từ: ≥ 1.000 ca vàng khớp 100%; chữ có dấu không bị coi là ranh giới từ.
  3. Mẫu không wildcard: ≥ 1 GB/s/luồng với chữ ASCII, ≥ 300 MB/s/luồng với chữ có dấu **[đề xuất]**.
  4. Đường SIMD và scalar cho cùng kết quả trên 10 triệu chuỗi ngẫu nhiên.
  5. Đạt DoD-C, DoD-H.

### CMD-303 · FIND cho Viewer: tìm song song trong bản vẽ, block, xref, attribute, kích thước, bảng
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-301, CMD-302, DOC-009, DOC-011, DOC-013, INT-008, INT-020, NT-005, NT-013
- **Lệnh:** FIND
- **Màn hình:** UI-F09
- **Đầu vào:** 03 §7; 07.2 UI-F09; API DOC-009 (snapshot), DOC-011 (không gian, layout), DOC-013 (xref chỉ đọc), INT-008 (vùng chọn, tập chọn), NT-005, NT-013; BENCH-STD text dày.
- **Đầu ra:**
  - `commands/annot/find/`: `FindQuery` (chuỗi, tùy chọn CMD-302; phạm vi toàn bản vẽ / không gian hiện tại / vùng chọn; loại chữ; trong block; trong xref; bỏ đối tượng ẩn).
  - `FindEngine::run(snapshot, query)` → luồng `FindHit` (đường sở hữu layout → INSERT lồng → entity, handle, loại, đoạn trích, span khớp, hộp bao WCS); `toSelectionSet(hits)`.
  - Lệnh FIND mở UI-F09; `bench/find_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. ≥ 300 bản vẽ QA-001 có đáp án: đúng 100% số lần khớp và vị trí theo từng loại chữ, kể cả block lồng ≥ 5 cấp và xref lồng.
  2. "Bỏ qua đối tượng ẩn": layer tắt/đông, attribute ẩn, VP Freeze của viewport hiện hành không vào kết quả; tắt tùy chọn thì có.
  3. 1 triệu đối tượng chữ: xong ≤ 300 ms p99 (tầm trung, 8 luồng), ≤ 1,2 s (máy thấp); kết quả đầu về UI ≤ 50 ms; hủy ≤ 50 ms **[đề xuất]**; hiệu suất song song ≥ 60% đến 8 luồng.
  4. Thứ tự kết quả tất định (layout, thứ tự vẽ, handle) với 1–32 luồng.
  5. Viewer: chỉ Tìm và Tạo tập chọn, không có đường gọi Thay (INT-020); hit trong block chọn INSERT ngoài cùng.
  6. Đạt DoD-C, DoD-H, DoD-L.
- **Ngoài phạm vi:** Thay (CMD-304); giao diện UI-F09 (UIF).

### CMD-304 · FIND bản Pro: Thay, Thay tất cả giữ định dạng và bảng mã
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-303, DOC-007, DOC-008, VN-001
- **Màn hình:** UI-F09
- **Đầu vào:** 07.2 UI-F09 (Thay, Thay tất cả); span của CMD-301; VN-001 (mã hóa ngược TCVN3/VNI); API DOC-007, DOC-008.
- **Đầu ra:** `commands/annot/find/replace.{h,cpp}`: `replaceOne(hit, text)`, `replaceAll(query, text)` → số đã thay, số bỏ qua kèm lý do (layer khóa, xref, field, chữ không mã hóa được về bảng mã cũ), số INSERT bị ảnh hưởng khi thay trong định nghĩa block (để UI xác nhận); cập nhật `docs/specs/commands/FIND.md`.
- **Tiêu chí nghiệm thu:**
  1. Phần thay nhận định dạng của ký tự đầu đoạn khớp; mã MTEXT ngoài đoạn khớp giữ nguyên từng byte trên 10.000 ca ngẫu nhiên.
  2. Chữ TCVN3/VNI ghi lại đúng bảng mã của đối tượng; ký tự không biểu diễn được thì bỏ qua đối tượng và báo.
  3. Chữ dim: thay trong số đo biến `<>` thành chữ ghi đè; thay trong ghi đè giữ `<>`. Xref, layer khóa, field: không thay, đếm vào "bỏ qua".
  4. Thay tất cả trên 100.000 đối tượng: 1 mark undo, `U` trả đúng hash trước đó; ≤ 1 s p99 (tầm trung) **[đề xuất]**.
  5. Đạt DoD-C, DoD-H, DoD-L.

## 2. Kiểu chữ, lõi trình soạn, TEXT

### CMD-305 · STYLE, -STYLE: kiểu chữ SHX, bigfont, TTF, annotative, bảng mã font
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-002, DOC-004, DOC-007, DOC-021, RGN-006, VN-002, INT-003, INT-005
- **Lệnh:** STYLE, -STYLE
- **Màn hình:** UI-D01
- **Đầu vào:** 03 §7; 07.2 UI-D01; 07.1 §7.4 (dropdown kiểu chữ); API DOC-004 (bảng STYLE), RGN-006 (danh sách font, ánh xạ SHX ↔ TTF), VN-002 (bảng mã của font).
- **Đầu ra:** `commands/annot/style/`: `TextStyleService` (tạo, sửa, đổi tên, xóa, đặt hiện hành TEXTSTYLE; đếm nơi dùng gồm chữ, ATTDEF, DIMSTYLE, MLEADERSTYLE, TABLESTYLE, block lồng; gợi ý font Unicode cho font mã cũ); STYLE mở UI-D01; -STYLE với prompt AutoCAD (tên hoặc `?`, font, chiều cao hoặc Annotative, hệ số rộng, góc nghiêng, ngược, lộn ngược, dọc khi SHX cho phép).
- **Tiêu chí nghiệm thu:**
  1. 60 ca vàng -STYLE (SHX, SHX + bigfont, TTF đậm/nghiêng, annotative, `?`, tên có dấu, tên trùng khác hoa thường): bảng STYLE khớp QA-007 từng trường.
  2. Xóa kiểu đang dùng (kể cả chỉ trong block lồng hoặc DIMSTYLE) bị chặn, báo số nơi dùng; Standard và kiểu hiện hành không xóa được.
  3. Sửa kiểu có 100.000 TEXT: 1 bản ghi STYLE đổi, 1 lô sự kiện; đếm nơi dùng ≤ 20 ms p99 **[đề xuất]**; 1 mark undo.
  4. ≥ 30 font mã cũ phổ biến ở VN (.VnTime, VNI-Times…): đúng bảng mã và font Unicode gợi ý.
  5. Đạt KTL với -STYLE.
  6. Đạt DoD-C, DoD-L.

### CMD-306 · Lõi phiên soạn chữ: buffer, caret theo grapheme, vùng chọn, undo trong trình soạn
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-007, NT-012
- **Đầu vào:** 07.2 UI-F01 (trình soạn tại chỗ), UI-F12; ranh giới grapheme và từ của Unicode (UAX #29); README §4.2 (Trình soạn MTEXT).
- **Đầu ra:** `commands/annot/editor/`:
  - `EditSession`: buffer UTF-16 NFC; caret và anchor; di chuyển theo grapheme, từ, dòng, đầu/cuối; Shift để chọn, nhấp đúp chọn từ, chọn tất cả; cắt/sao chép/dán chữ thuần.
  - `LayoutProvider` (hit test, hình caret, dòng trên/dưới), hiện thực ở CMD-308 (một dòng) và CMD-313 (MTEXT).
  - `EditUndoStack` gộp lượt gõ; bảng phân đoạn sinh từ dữ liệu Unicode.
- **Tiêu chí nghiệm thu:**
  1. 100% ca của GraphemeBreakTest và WordBreakTest (Unicode) đạt; caret không dừng giữa chữ cái và dấu tổ hợp.
  2. 1 triệu chuỗi thao tác ngẫu nhiên: undo hết trả văn bản và caret ban đầu, redo hết trả trạng thái cuối, từng ký tự.
  3. Gõ liên tiếp không quá 1 s và không qua khoảng trắng gộp thành 1 bước undo **[đề xuất]**.
  4. Một thao tác trên buffer 10.000 ký tự ≤ 50 µs p99; không cấp phát khi gõ ở cuối buffer **[đề xuất]**.
  5. Đạt DoD-C.

### CMD-307 · Áp sự kiện bộ gõ vào phiên soạn: thay vùng, preedit, NFC, undo (lớp IME UIA-106)
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-306, UIA-106, NT-012
- **Đầu vào:** 07.2 UI-F01 ("bộ gõ tiếng Việt hoạt động đầy đủ"), UI-F12; lớp IME và bộ vết gõ của UIA-106 (Unikey/EVKey, bộ gõ macOS, ibus/fcitx5, preedit, quy tắc bypass).
- **Đầu ra:** `commands/annot/editor/ime_apply.{h,cpp}`: `applyImeEdit(session, ImeEdit)` (commit thay vùng quanh caret; preedit tạm hiển thị qua `LayoutProvider`, không vào undo); `imeContext(session)` trả surrounding text, vị trí caret/anchor, hình caret cho UIA-106; commit chuẩn hóa NFC; chuỗi Backspace + ký tự của bộ gõ kiểu "gửi phím" gộp 1 bước undo. Không làm lại lớp IME.
- **Tiêu chí nghiệm thu:**
  1. Phát lại toàn bộ vết gõ của UIA-106 vào `EditSession`: văn bản NFC trùng đáp án 100%.
  2. Esc hoặc bấm ra ngoài khi đang preedit không để lại ký tự; preedit không tạo bước undo; mỗi commit, kể cả chuỗi Backspace + ký tự, đúng 1 bước.
  3. Thay vùng chỉ chạm đúng vùng bộ gõ báo: 10.000 ca ngẫu nhiên khớp mô hình tham chiếu.
  4. Xử lý một sự kiện ≤ 100 µs p99 **[đề xuất]**.
  5. Đạt DoD-C, DoD-L.

### CMD-308 · TEXT, DTEXT: chữ một dòng, 15 kiểu canh, soạn tại chỗ nhiều dòng
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-306, CMD-307, CMD-002, RGN-007, DOC-007, DOC-021, INT-003, INT-004, INT-012
- **Lệnh:** TEXT, DTEXT
- **Đầu vào:** 03 §7; 07.2 UI-E10 (TEXT), UI-F12; prototype `design/prototype/js/annot.js` `IMPL.TEXT`; API RGN-007 (dàn chữ một dòng, giải mã `%%`).
- **Đầu ra:** `commands/annot/text/`: lệnh TEXT/DTEXT; `SingleLineLayout` (hiện thực `LayoutProvider` trên RGN-007 cho 15 kiểu canh, Align, Fit); hiện `%%c %%d %%p %%u %%o` thành ký hiệu ngay khi gõ.
- **Tiêu chí nghiệm thu:**
  1. Đủ Justify (Left, Center, Right, Align, Middle, Fit, TL, TC, TR, ML, MC, MR, BL, BC, BR) và Style (`?` liệt kê); bỏ prompt chiều cao khi kiểu có chiều cao cố định; kiểu annotative hỏi chiều cao trên giấy. 120 ca vàng khớp QA-007, sai số ≤ 1e-9 × kích thước bản vẽ.
  2. Enter tạo TEXT mới cách 5/3 chiều cao theo hướng xoay; bấm điểm khác bắt đầu dòng tại đó (DTEXTED); Backspace ở đầu dòng rỗng quay về dòng trước; Enter ở dòng rỗng kết thúc.
  3. `.scr`: mỗi dòng một TEXT, dòng trống kết thúc; `(command "TEXT" p h a "chuỗi")`: một TEXT rồi kết thúc.
  4. Tôn trọng TEXTSTYLE, TEXTSIZE (ghi chiều cao vừa nhập), CANNOSCALE; phím → caret và chữ (phần lõi) ≤ 2 ms p99 với dòng 500 ký tự **[đề xuất]**.
  5. Đạt KTL: cả lượt nhiều dòng = 1 mark undo.
  6. Đạt DoD-C, DoD-L.

## 3. Trình soạn MTEXT, lệnh MTEXT, TEXTEDIT

### CMD-309 · Tài liệu soạn MTEXT trên cây định dạng: chèn, xóa, tách và gộp đoạn, ghi ngược
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-301, CMD-306, RGN-008, RGN-009
- **Đầu vào:** README §4.2 (Trình soạn MTEXT); 05 §8 (mã MTEXT); API RGN-008 (cây định dạng, ghi ngược), RGN-009.
- **Đầu ra:** `commands/annot/mtext/doc/`: `MTextEditDoc`: vị trí logic (đoạn, run, offset) ↔ chỉ số buffer CMD-306; chèn kế thừa định dạng bên trái; xóa qua ranh giới run và đoạn; xếp chồng, field là đơn vị nguyên tử; tách/gộp đoạn giữ thuộc tính đoạn; gộp run kề cùng định dạng; `serialize()` tối giản, ổn định; giữ nguyên chuỗi gốc nếu không sửa.
- **Tiêu chí nghiệm thu:**
  1. Mọi chuỗi MTEXT của kho QA-001 (≥ 1 triệu): mở rồi kết thúc không sửa → chuỗi giữ nguyên từng byte.
  2. 1 triệu chuỗi thao tác ngẫu nhiên: `parse(serialize(doc))` cho cây tương đương; chữ phẳng (CMD-301) khớp mô hình tham chiếu; độ dài chuỗi ≤ 1,1 × chuỗi tối giản tham chiếu **[đề xuất]**.
  3. Sau sửa, RGN-009 dàn chuỗi ghi ngược trùng 100% dàn trang trong trình soạn (ngắt dòng, vị trí glyph).
  4. Chèn/xóa một ký tự trên MTEXT 2.000 ký tự, 200 run ≤ 100 µs p99 **[đề xuất]**.
  5. Đạt DoD-C.

### CMD-310 · Định dạng ký tự theo vùng chọn, ký hiệu, đổi hoa/thường giữ dấu
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-309, RGN-006, NT-012
- **Màn hình:** UI-F01
- **Đầu vào:** 07.2 UI-F01 (panel Kiểu, Định dạng, Chèn › ký hiệu); mã `\f \H \W \Q \T \C \c \L \O \K` (RGN-008); RGN-006 (font có/không có đậm, nghiêng).
- **Đầu ra:** `commands/annot/mtext/format.{h,cpp}`:
  - `applyCharFormat(range, FormatDelta)`: kiểu, font, cao, màu, đậm, nghiêng, gạch chân/trên/ngang, góc nghiêng, giãn ký tự, hệ số rộng; khớp định dạng; xóa định dạng (ký tự/đoạn/tất cả); hoa, thường, viết hoa chữ đầu.
  - `queryFormat(range)`: bật/tắt/lẫn từng thuộc tính cho UI.
  - Chèn ký hiệu (°, ±, ⌀, ≈, ∠, Δ, Ω, ², ³, ≠, ≤, ≥, ‰, khoảng trắng không ngắt…); gõ `%%c %%d %%p` đổi thành ký tự Unicode như AutoCAD.
- **Tiêu chí nghiệm thu:**
  1. 300 ca vàng (đặt/bỏ thuộc tính trên vùng chọn cắt ngang run, đoạn, xếp chồng): dàn trang trùng chuỗi AutoCAD sinh cho cùng thao tác trong QA-007 (cùng ngắt dòng, glyph lệch ≤ 1e-6 × chiều cao chữ).
  2. `queryFormat` đúng 100% trên 100.000 vùng chọn ngẫu nhiên so với mô hình tham chiếu.
  3. "đường" → "ĐƯỜNG" → "Đường": 134 chữ cái có dấu đúng 100%.
  4. Áp định dạng cho vùng 2.000 ký tự ≤ 1 ms p99 **[đề xuất]**.
  5. Đạt DoD-C.

### CMD-311 · Xếp chồng: phân số ngang, chéo, dung sai, thập phân, AutoStack
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-309, NT-010
- **Màn hình:** UI-F02
- **Đầu vào:** 07.2 UI-F02; mã `\S` với `/`, `#`, `^` (RGN-008); sysvar TSTACKALIGN, TSTACKSIZE.
- **Đầu ra:** `commands/annot/mtext/stack.{h,cpp}`: tạo/bỏ xếp chồng từ vùng chọn; `StackProps` (trên, dưới, kiểu, vị trí trên/giữa/dưới, cỡ %) đọc/sửa; AutoStack khi gõ (số/số rồi Space; bỏ khoảng trắng đầu; `/` thành chéo hay ngang) theo thiết lập lưu ở NT-010.
- **Tiêu chí nghiệm thu:**
  1. 100 ca vàng (1/2, 1#2, +0.1^-0.2, mũ x^2, thập phân): chuỗi `\S` và dàn trang khớp QA-007.
  2. Caret, chọn, xóa coi xếp chồng là một đơn vị như AutoCAD; 30 ca vàng đúng 100%.
  3. Ctrl+Z ngay sau AutoStack trả chữ gốc trong 1 bước.
  4. Đạt DoD-C.

### CMD-312 · Đoạn: bullet, đánh số tự động, tab, thụt lề, canh đoạn, khoảng cách, giãn dòng
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-309, RGN-009, QA-109
- **Màn hình:** UI-F03
- **Đầu vào:** 07.2 UI-F01 (panel Đoạn, thước), UI-F03; mã đoạn `\p…;` (RGN-008); RGN-009 (đánh số, tab khi dàn trang).
- **Đầu ra:** `commands/annot/mtext/para.{h,cpp}`: áp thuộc tính cho mọi đoạn trong vùng chọn; danh sách bullet, số, chữ hoa/thường (bắt đầu lại, tiếp tục, tự nhận "1." hoặc "a." + Tab/Space như AutoCAD, tăng/giảm cấp); tab trái/giữa/phải/thập phân; thụt dòng đầu, treo, phải; canh trái/giữa/phải/đều/phân bố; khoảng trước/sau; giãn dòng chính xác/tối thiểu/bội số; dữ liệu thước cho UI.
- **Tiêu chí nghiệm thu:**
  1. 200 ca vàng: chuỗi ghi ngược khớp QA-007; số thứ tự hiển thị đúng 100%; ca đoạn của QA-109 ngắt dòng ≥ 99% đoạn khớp.
  2. Chèn/xóa một mục giữa danh sách 500 mục: đánh số lại ≤ 1 ms p99 **[đề xuất]**.
  3. Kéo tab, thụt lề trên thước: giá trị ghi lại làm tròn theo đặc tả, khớp từng ký tự với QA-007.
  4. Đạt DoD-C.

### CMD-313 · Phiên soạn MTEXT: dàn trang tăng dần, caret, thước, tìm/thay, ngân sách độ trễ
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** CMD-302, CMD-307, CMD-310, CMD-311, CMD-312, RGN-009, RND-012, DOC-007, UIA-106, QA-109
- **Màn hình:** UI-F01
- **Đầu vào:** 07.2 UI-F01 (trình soạn tại chỗ: khung có grip, thước, Ctrl+Enter, bấm ra ngoài, Esc hỏi nếu đã sửa; WYSIWYG, nền đục); README DoD-L; API RGN-009, RND-012 (overlay).
- **Đầu ra:** `commands/annot/mtext/session/`:
  - `MTextEditor` mở từ entity hoặc khung mới; `LayoutProvider` trên RGN-009 có cache theo đoạn, chỉ dàn lại đoạn bị sửa và dời các đoạn sau.
  - Primitive caret, vùng chọn, preedit cho RND-012; grip rộng/cao khung; dữ liệu thước; tìm/thay trong trình soạn (CMD-302); dán chữ thuần; AutoCAPS.
  - Kết thúc → 1 transaction DOC-007; `bench/mtext_edit_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Gõ, xóa, dán, đổi định dạng trên MTEXT 2.000 ký tự, 40 đoạn: phần lõi (thao tác + dàn lại + primitive) ≤ 4 ms p99 tầm trung, ≤ 8 ms máy thấp; phím → khung hình ≤ 16 ms p99 đo bằng QA-011 khi UI-F01 tích hợp **[đề xuất]**.
  2. Dàn trang tăng dần trùng từng bit với dàn lại toàn bộ trên 100.000 chuỗi thao tác ngẫu nhiên; vết gõ UIA-106 phát lại giữa run có định dạng, xếp chồng, field cho chuỗi ghi ngược đúng 100%.
  3. Ngắt dòng trong trình soạn trùng 100% với entity sau khi lưu; bộ tham chiếu ngắt dòng QA-109 ≥ 99% đoạn khớp (05 §1.2).
  4. Kết thúc soạn = 1 mark undo; Esc không đổi hash bản vẽ; undo trong trình soạn không chạm undo bản vẽ.
  5. Mở trình soạn trên MTEXT 2.000 ký tự đến khung đầu ≤ 50 ms p99 **[đề xuất]**.
  6. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** tab ngữ cảnh, thanh công cụ, thước vẽ bằng Qt (UIF); cột (CMD-316).

### CMD-314 · MTEXT, -MTEXT: tạo chữ nhiều dòng, tùy chọn khung, nền che
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-313, CMD-002, DOC-021, INT-003, INT-004, INT-012, QA-109
- **Lệnh:** MTEXT, -MTEXT
- **Đầu vào:** 03 §7; 07.2 UI-E10 (MTEXT); prototype `design/prototype/js/annot.js` `IMPL.MTEXT`; API CMD-313.
- **Đầu ra:** `commands/annot/mtext/cmd/`: MTEXT (góc thứ nhất → góc đối; Height, Justify 9 điểm, Line spacing At least/Exactly, Rotation, Style, Width, Columns — mốc này chỉ No columns; jig khung với MTJIGSTRING; mở CMD-313); -MTEXT nhập từng dòng trên dòng lệnh, dòng trống kết thúc; thuộc tính đối tượng: chiều cao khung, nền che (màu, hệ số lề, màu nền bản vẽ).
- **Tiêu chí nghiệm thu:**
  1. 150 ca vàng mọi tùy chọn: thuộc tính và chuỗi khớp QA-007; MTEXT tạo bằng lệnh theo tham số các ca QA-109 ngắt dòng ≥ 99% đoạn khớp.
  2. Tôn trọng TEXTSTYLE, TEXTSIZE, MTEXTED, MTJIGSTRING, CANNOSCALE (annotative theo kiểu chữ).
  3. `.scr`, `(command "MTEXT" …)` và `trion-console` dùng nhánh nhập dòng lệnh của -MTEXT, cùng hash với bàn phím **[đề xuất]**.
  4. Jig khung ≤ 8 ms/khung; tạo xong ≤ 20 ms p99.
  5. Đạt KTL.
  6. Đạt DoD-C, DoD-L.

### CMD-315 · TEXTEDIT, DDEDIT, MTEDIT: sửa chú thích, định tuyến trình soạn theo loại đối tượng
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-308, CMD-313, CMD-002, RGN-014, RGN-015, INT-008, INT-018, INT-020
- **Lệnh:** TEXTEDIT, DDEDIT, MTEDIT
- **Màn hình:** UI-F12
- **Đầu vào:** 03 §7; 07.1 §9.10 (nhấp đúp); 07.2 UI-F12, UI-F01; prototype `design/prototype/js/annot.js` `IMPL.TEXTEDIT`, `TC.cad.hook.dbl`.
- **Đầu ra:** `commands/annot/textedit/`: `AnnotEditorRegistry` (loại entity → trình soạn; TEXT, ATTDEF, MTEXT, DIMENSION đăng ký ở đây; MLEADER ở CMD-327, ô bảng ở CMD-333); TEXTEDIT (Mode Single/Multiple, Undo); DDEDIT, MTEDIT; nối nhấp đúp INT-018; ATTDEF sửa Tag, Prompt, Default; chữ dim soạn bằng CMD-313 với `<>` là số đo.
- **Tiêu chí nghiệm thu:**
  1. 100 ca vàng sửa TEXT, ATTDEF, MTEXT, chữ dim (giữ, xóa `<>`; chữ rỗng trở về số đo): khớp QA-007 từng trường.
  2. Mode Multiple lặp đến Enter; Undo trong lệnh lùi từng lần sửa; cả lệnh = 1 mark undo.
  3. Layer khóa: báo, không mở trình soạn. Viewer: không có lệnh (INT-020).
  4. Không giao diện (`trion-console`): sau khi chọn hỏi "Chữ mới <chữ cũ>:" trên dòng lệnh **[đề xuất]**; cùng hash ba đường nhập.
  5. Nhấp đúp đến trình soạn sẵn sàng ≤ 50 ms p99 **[đề xuất]**.
  6. Đạt KTL. Đạt DoD-C, DoD-L.

### CMD-316 · MTEXT cột: cột động, cột tĩnh, ngắt cột, tay nắm cột
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-314, RGN-010, QA-109
- **Màn hình:** UI-F04
- **Đầu vào:** 07.2 UI-F04, UI-F01 (Chèn › cột); API RGN-010.
- **Đầu ra:** `commands/annot/mtext/columns.{h,cpp}`: tùy chọn Columns của MTEXT (Dynamic tự cao hoặc cao thủ công, Static, No columns; số cột, rộng cột, khoảng giữa, tổng rộng); trong trình soạn: tay nắm rộng cột và chiều cao, ngắt cột; sysvar MTEXTCOLUMN.
- **Tiêu chí nghiệm thu:**
  1. 80 ca vàng: thuộc tính cột khớp QA-007; ca cột của QA-109 ngắt dòng ≥ 99% đoạn khớp.
  2. Kéo tay nắm cột trên MTEXT 2.000 ký tự, 3 cột: dàn lại ≤ 4 ms p99 **[đề xuất]**.
  3. Đạt KTL cho tùy chọn Columns.
  4. Đạt DoD-C, DoD-L.

### CMD-317 · Nhập .txt/.rtf và dán từ Word giữ định dạng cơ bản
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-313, NT-012
- **Màn hình:** UI-F01
- **Đầu vào:** 07.2 UI-F01 (Công cụ › Nhập văn bản; dán từ Word, tùy chọn "chỉ dán chữ"); đặc tả RTF 1.9.1 của Microsoft; định dạng HTML clipboard của Windows.
- **Đầu ra:** `commands/annot/mtext/import/`: parser RTF và HTML tập con (đậm, nghiêng, gạch chân, font, cỡ, màu, đoạn, tab, `\ansicpg`, `\u`, `\'xx`) → thao tác trên `MTextEditDoc`; nhập `.txt` UTF-8/UTF-16/CP1258 có dò BOM; giới hạn tệp 1 MB **[đề xuất]**.
- **Tiêu chí nghiệm thu:**
  1. 200 mẫu RTF/HTML (Word, LibreOffice, Excel): chữ phẳng khớp 100%; định dạng cơ bản khớp đáp án ≥ 98% run **[đề xuất]**.
  2. Tiếng Việt từ `\u` và CP1258 ra NFC đúng 100%.
  3. Fuzz RTF và HTML ≥ 1 giờ không lỗi; tệp 1 MB nhập ≤ 200 ms **[đề xuất]**.
  4. Đạt DoD-C.

## 4. Công cụ chữ, Express, chính tả

### CMD-318 · SCALETEXT, JUSTIFYTEXT: đổi cỡ và điểm canh không dời chữ
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-002, RGN-007, RGN-009, DOC-021, INT-003, INT-008
- **Lệnh:** SCALETEXT, JUSTIFYTEXT
- **Đầu vào:** 03 §7; 07.1 §7.4 (Đổi tỉ lệ chữ, Đổi điểm canh); hộp bao chữ của RGN-007, RGN-009.
- **Đầu ra:** `commands/annot/texttools/scaletext.cpp`, `justifytext.cpp`; áp cho TEXT, MTEXT, ATTDEF, ATTRIB.
- **Tiêu chí nghiệm thu:**
  1. SCALETEXT: điểm gốc Existing/Left/Center/Middle/Right/TL…BR; chiều cao mới, Paper height (annotative), Match object, Scale factor. 80 ca vàng khớp QA-007; điểm gốc giữ nguyên ≤ 1e-9 × kích thước bản vẽ.
  2. JUSTIFYTEXT: 15 kiểu cho TEXT/ATTDEF/ATTRIB, 9 kiểu cho MTEXT; hộp bao chữ trước và sau trùng ≤ 1e-9 × kích thước bản vẽ.
  3. 10.000 đối tượng: xong ≤ 100 ms p99 **[đề xuất]**.
  4. Đạt KTL. Đạt DoD-C, DoD-L.

### CMD-319 · TEXTALIGN: gióng thẳng và phân bố chữ
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-002, RGN-007, RGN-009, INT-008, INT-012
- **Lệnh:** TEXTALIGN
- **Đầu vào:** 03 §7; 07.1 §7.4 (Gióng thẳng chữ).
- **Đầu ra:** `commands/annot/texttools/textalign.cpp`: chọn chữ → chữ chuẩn → điểm thứ hai; Alignment (kiểu canh), Options (Distribute, Set spacing, Current vertical, Current horizontal); xem trước bằng jig.
- **Tiêu chí nghiệm thu:**
  1. 60 ca vàng (TEXT và MTEXT trộn, có xoay) khớp QA-007 ≤ 1e-9 × kích thước bản vẽ.
  2. Jig ≤ 8 ms/khung khi chọn 1.000 chữ.
  3. Đạt KTL. Đạt DoD-C, DoD-L.

### CMD-320 · TXT2MTXT: gộp TEXT thành MTEXT
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-301, CMD-309, CMD-002, INT-008
- **Lệnh:** TXT2MTXT
- **Đầu vào:** 03 §7; 07.1 §7.4 (Gộp chữ thành mtext); API CMD-309 (dựng MTEXT).
- **Đầu ra:** `commands/annot/texttools/txt2mtxt.cpp`: Settings (thứ tự theo vị trí hoặc theo lần chọn; ngắt dòng tự động; giãn dòng); chữ khác kiểu/cao/màu/font → mã định dạng nội tuyến; `%%u`/`%%o` → `\L`/`\O`; xóa chữ nguồn.
- **Tiêu chí nghiệm thu:**
  1. 60 ca vàng khớp QA-007 (chuỗi, điểm chèn, rộng khung).
  2. Giữ font từng chữ nguồn bằng `\f`: chữ TCVN3/VNI hiển thị như trước khi gộp (QA-005, SSIM ≥ 0,99).
  3. Đạt KTL. Đạt DoD-C.

### CMD-321 · QTEXT: hiển thị chữ dạng khung
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-002, RGN-003, RGN-007, RGN-009, INT-005
- **Lệnh:** QTEXT
- **Đầu vào:** 03 §7; sysvar QTEXTMODE; API RGN-003 (tham số regen).
- **Đầu ra:** `commands/annot/texttools/qtext.cpp` (QTEXT ON/OFF); cờ `qtext` trong tham số regen và nhánh khung bao cho TEXT, MTEXT, ATTRIB, chữ dim, mleader, ô bảng ở `core/regen/`.
- **Tiêu chí nghiệm thu:**
  1. Như AutoCAD: đổi QTEXTMODE có hiệu lực sau REGEN; khung trùng hộp bao chữ ≤ 1e-9 × kích thước bản vẽ; bản in tôn trọng QTEXTMODE.
  2. Regen BENCH-STD text dày khi bật QTEXT nhanh hơn khi tắt ≥ 3 lần **[đề xuất]**.
  3. Đạt KTL. Đạt DoD-C.

### CMD-322 · TXTEXP: phá chữ thành hình học
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-002, RGN-004, RGN-005, RGN-007, RGN-009, INT-008, NT-005
- **Lệnh:** TXTEXP
- **Đầu vào:** 03 §28; 07.2 UI-E10 (TXTEXP: chọn chữ); glyph nét SHX (RGN-004), đường viền TTF (RGN-005).
- **Đầu ra:** `commands/annot/express/txtexp.cpp`: TEXT, MTEXT, ATTDEF → polyline theo nét SHX và đường viền TTF (đường cong xấp xỉ sai số ≤ 1e-3 × chiều cao chữ **[đề xuất]**); giữ layer, màu, linetype của chữ; xóa chữ gốc như Express Tools.
- **Tiêu chí nghiệm thu:**
  1. 50 ca vàng (SHX, bigfont, TTF tiếng Việt, xoay, nghiêng, ngược): hình học trùng glyph RGN hiển thị ≤ 1e-9 × kích thước bản vẽ; so ảnh với QA-007 SSIM ≥ 0,98.
  2. 10.000 chữ ≤ 1 s p99 trên 8 luồng; hiệu suất song song ≥ 60%.
  3. Đạt KTL. Đạt DoD-C, DoD-H.

### CMD-323 · TEXTFIT, TORIENT, TEXTMASK, TEXTUNMASK
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-002, DOC-005, DOC-020, RGN-007, RGN-009, INT-008, INT-012
- **Lệnh:** TEXTFIT, TORIENT, TEXTMASK, TEXTUNMASK
- **Đầu vào:** 03 §7, §28.
- **Đầu ra:** `commands/annot/express/`: TEXTFIT (đổi hệ số rộng cho vừa hai điểm, có jig); TORIENT (xoay TEXT, MTEXT, ATTDEF quanh điểm chèn về góc dễ đọc nhất hoặc góc nhập); TEXTMASK (vùng che Wipeout/3dface/Solid hoặc nền che MTEXT, lề; liên kết chữ–vùng che như Express Tools); TEXTUNMASK.
- **Tiêu chí nghiệm thu:**
  1. 90 ca vàng (30 mỗi nhóm TEXTFIT, TORIENT, TEXTMASK) khớp QA-007.
  2. TEXTUNMASK xóa đúng vùng che đã tạo, không chạm đối tượng khác (20 ca).
  3. Đạt KTL. Đạt DoD-C.

### CMD-324 · ARCTEXT, RTEXT: chữ theo cung và chữ từ xa
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** CMD-002, DWG-014, DWG-016, RGN-001, RGN-004, RGN-005, INT-022, NT-011
- **Lệnh:** ARCTEXT, RTEXT
- **Đầu vào:** 03 §7, §28; đối tượng ARCALIGNEDTEXT và RTEXT của Express Tools trong kho QA-001; DIESEL (INT-022).
- **Đầu ra:** `commands/annot/express/`: ARCTEXT (chọn cung hoặc ARCALIGNEDTEXT; chữ, kiểu, cao, rộng, khoảng ký tự, lề, phía lồi/lõm, hướng, canh trái/phải/khớp/giữa); RTEXT (tệp văn bản hoặc biểu thức DIESEL; Style, Height, Rotate, Edit); hàm regen của hai loại đăng ký qua RGN-001; RTEXT nạp lại khi tệp nguồn đổi (NT-011).
- **Tiêu chí nghiệm thu:**
  1. ARCALIGNEDTEXT, RTEXT trong kho QA-001: round-trip 100% trường; hiển thị SSIM ≥ 0,98 so DWG TrueView.
  2. 60 ca vàng tạo và sửa khớp QA-007.
  3. RTEXT DIESEL cập nhật khi sysvar đổi; tệp nguồn đổi → cập nhật ≤ 1 s, không polling.
  4. Đạt KTL. Đạt DoD-C.
- **Ngoài phạm vi:** schema hai đối tượng (bổ sung vào DWG-014).

### CMD-325 · SPELL: chính tả tiếng Việt và tiếng Anh, kiểm khi gõ
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** L · **Phụ thuộc:** CMD-301, CMD-302, CMD-313, DOC-007, NT-005, NT-010
- **Lệnh:** SPELL
- **Màn hình:** UI-F10
- **Đầu vào:** 03 §7 (từ điển tiếng Việt); 07.2 UI-F10, UI-F01 (panel Chính tả); sysvar DCTMAIN, DCTCUST.
- **Đầu ra:** `commands/annot/spell/`:
  - Bộ kiểm âm tiết tiếng Việt (phụ âm đầu, vần, thanh; quy tắc c/k/q, g/gh, ng/ngh); từ điển âm tiết, từ ghép, thuật ngữ xây dựng; từ điển tiếng Anh; từ điển người dùng `.cus`.
  - Gợi ý theo khoảng cách sửa có trọng số dấu và lỗi gõ Telex/VNI.
  - Lệnh SPELL (phạm vi; Bỏ qua, Bỏ qua tất cả, Thêm vào từ điển, Sửa, Sửa tất cả; bỏ qua chữ hoa toàn bộ, chữ có số, địa chỉ web); API kiểm khi gõ cho CMD-313.
- **Tiêu chí nghiệm thu:**
  1. 5.000 câu kỹ thuật tiếng Việt có lỗi gán nhãn: phát hiện ≥ 95%, báo nhầm ≤ 2%, đáp án nằm trong 3 gợi ý đầu ≥ 85% **[đề xuất]**.
  2. Kiểm một đoạn 500 ký tự ≤ 1 ms p99; quét 1 triệu đối tượng chữ ≤ 3 s trên 8 luồng **[đề xuất]**.
  3. Sửa tất cả: 1 mark undo, giữ định dạng qua span CMD-301.
  4. Từ điển chỉ dùng dữ liệu có giấy phép thương mại; nguồn và giấy phép ghi trong `commands/annot/spell/README.md`.
  5. Đạt DoD-C, DoD-H, DoD-L.

## 5. Multileader và leader

### CMD-326 · MLEADERSTYLE: kiểu multileader
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** DWG-014, DOC-004, DOC-007, DOC-021, VN-008
- **Lệnh:** MLEADERSTYLE
- **Màn hình:** UI-D03
- **Đầu vào:** 03 §9; 07.2 UI-D03; 07.1 §7.4 (panel Đường dẫn); block TCVN (VN-008).
- **Đầu ra:** `commands/annot/mleader/style/`: `MLeaderStyleService` (tạo từ kiểu có sẵn, sửa, đổi tên, xóa, CMLEADERSTYLE; Standard tự tạo khi thiếu); đủ thuộc tính 3 tab UI-D03 (định dạng đường dẫn, cấu trúc, nội dung MText/Block/Không); block nguồn có sẵn và block TCVN; lan truyền thay đổi tới MLEADER, giữ thuộc tính đã ghi đè.
- **Tiêu chí nghiệm thu:**
  1. Mọi thuộc tính UI-D03 ghi đúng trường MLEADERSTYLE: round-trip QA-003 100% trường; 40 kiểu mẫu QA-001 hiển thị SSIM ≥ 0,98 (QA-005).
  2. Sửa kiểu có 10.000 MLEADER: tính xong danh sách cập nhật ≤ 50 ms p99 **[đề xuất]**; MLEADER có ghi đè giữ giá trị; 1 mark undo.
  3. Xóa kiểu đang dùng bị chặn và báo số MLEADER; Standard không xóa được.
  4. Đạt DoD-C, DoD-L.

### CMD-327 · MLEADER: tạo multileader, nội dung MTEXT hoặc block
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** CMD-313, CMD-315, CMD-326, CMD-002, RGN-016, DOC-012, DOC-021, INT-003, INT-004, INT-012
- **Lệnh:** MLEADER
- **Đầu vào:** 07.2 UI-E10 (MLEADER), UI-D03; prototype `design/prototype/js/annot.js` `IMPL.MLEADER`; API RGN-016.
- **Đầu ra:** `commands/annot/mleader/`: thứ tự arrowHead first / leader Landing first / Content first; Options: Leader type (Straight/sPline/None), leader lAnding, Content type (Block/Mtext/None), Maxpoints, First angle, Second angle, eXit options; nội dung MTEXT soạn bằng CMD-313 tại vị trí gắn; nội dung block: tên, giá trị attribute; jig; đăng ký trình soạn chữ MLEADER vào `AnnotEditorRegistry` (CMD-315).
- **Tiêu chí nghiệm thu:**
  1. 120 ca vàng (3 thứ tự × loại đường dẫn × loại nội dung × annotative): điểm, landing, dogleg, vị trí chữ khớp QA-007 ≤ 1e-9 × kích thước bản vẽ.
  2. Ràng buộc góc và số điểm tối đa đúng; Enter trước khi đủ điểm kết thúc đường dẫn như AutoCAD.
  3. Thuộc tính theo CMLEADERSTYLE; chỉ ghi đè cái người dùng đổi trong lệnh.
  4. Từ `.scr`/`(command)`: chữ nội dung nhập trên dòng lệnh, dòng trống kết thúc **[đề xuất]**.
  5. Jig ≤ 8 ms/khung; tạo xong ≤ 20 ms p99. Đạt KTL.
  6. Đạt DoD-C, DoD-L.

### CMD-328 · MLEADEREDIT và grip multileader
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-327, INT-013
- **Lệnh:** MLEADEREDIT
- **Đầu vào:** 03 §9 (thêm/bớt nhánh); 07.1 §7.4; prototype `design/prototype/js/annot.js` `IMPL.MLEADEREDIT`; API INT-013 (đăng ký grip, grip đa năng).
- **Đầu ra:** `commands/annot/mleader/edit/`: MLEADEREDIT (Add leader, Remove leaders); grip đầu mũi tên, đỉnh, đoạn ngang (kéo dài/dời), nội dung; menu grip đa năng (Thêm đường dẫn, Bớt đường dẫn, Thêm đỉnh, Bớt đỉnh, Kéo dài đoạn ngang).
- **Tiêu chí nghiệm thu:**
  1. 80 ca vàng thêm/bớt nhánh và kéo grip khớp QA-007; nhánh mới gắn cùng phía landing gần nhất như AutoCAD.
  2. Bớt nhánh cuối cùng bị từ chối và báo; mỗi lần chạy hoặc kéo grip = 1 mark undo.
  3. Xem trước khi rê grip ≤ 8 ms/khung; hit test grip ≤ 1 ms.
  4. Đạt KTL. Đạt DoD-C, DoD-L.

### CMD-329 · MLEADERALIGN, MLEADERCOLLECT
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-327, INT-008, INT-012
- **Lệnh:** MLEADERALIGN, MLEADERCOLLECT
- **Đầu vào:** 03 §9; 07.1 §7.4 (Gióng hàng, Gom nhóm).
- **Đầu ra:** `commands/annot/mleader/align/`: MLEADERALIGN (mleader chuẩn → hướng; Options: Distribute, make leader segments Parallel, Specify spacing, Use current spacing); MLEADERCOLLECT (mleader nội dung block; Horizontal, Vertical, Wrap theo bề rộng).
- **Tiêu chí nghiệm thu:**
  1. 60 ca vàng khớp QA-007 ≤ 1e-9 × kích thước bản vẽ.
  2. MLEADERCOLLECT giữ giá trị attribute, xóa mleader thừa; mleader nội dung chữ bị bỏ qua và báo số.
  3. Jig ≤ 8 ms/khung với 500 mleader. Đạt KTL.
  4. Đạt DoD-C, DoD-L.

### CMD-330 · LEADER: đường dẫn cũ và liên kết chú thích
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-313, CMD-002, CMD:TOLERANCE, RGN-016, DOC-005, INT-003, INT-012
- **Lệnh:** LEADER
- **Đầu vào:** 03 §9; API RGN-016, DOC-005 (reactor lưu trong bản vẽ); DIMVAR của DIMSTYLE hiện hành.
- **Đầu ra:** `commands/annot/leader/`: LEADER (điểm đầu → các điểm; Annotation: dòng chữ hoặc Options Tolerance/Copy/Block/None/Mtext; Format: Spline/STraight/Arrow/None; Undo); liên kết LEADER ↔ chú thích (dời, chép, kéo giãn chú thích thì đường dẫn bám theo); hook line theo phía chữ; theo DIMASZ, DIMLDRBLK, DIMGAP, DIMTAD, DIMCLRD.
- **Tiêu chí nghiệm thu:**
  1. 60 ca vàng mọi tùy chọn khớp QA-007.
  2. 50 ca MOVE, COPY, STRETCH, ROTATE chú thích: điểm cuối đường dẫn bám theo đúng 100%; xóa chú thích thì gỡ liên kết như AutoCAD.
  3. Đạt KTL. Đạt DoD-C, DoD-L.

### CMD-331 · QLEADER: leader nhanh và thiết lập
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-330
- **Lệnh:** QLEADER
- **Màn hình:** UI-F13
- **Đầu vào:** 07.2 UI-E10 (QLEADER), UI-F13; prototype `design/prototype/js/screens9.js` `IMPL.QLEADER`, `SC.qlSettings`.
- **Đầu ra:** `commands/annot/leader/qleader.cpp`: điểm đầu hoặc Settings → các điểm theo thiết lập → bề rộng chữ → dòng chú thích đầu `<Mtext>` → dòng tiếp; `QLeaderSettings` 3 tab UI-F13 (loại chú thích, tùy chọn MText, dùng lại chú thích; thẳng/spline, mũi tên, số điểm, ràng buộc góc; gắn chữ trái/phải 5 kiểu, gạch chân dòng cuối) lưu trong bản vẽ như AutoCAD.
- **Tiêu chí nghiệm thu:**
  1. 80 ca vàng (góc tự do/ngang/90/45/30/15°, số điểm tối đa, 5 kiểu gắn, dùng lại chú thích) khớp QA-007.
  2. Bản vẽ lưu xong mở trong AutoCAD: QLEADER dùng đúng thiết lập (QA-007, 10 ca).
  3. Đạt KTL (chữ nhập trên dòng lệnh từ `.scr`/`(command)`).
  4. Đạt DoD-C, DoD-L.

## 6. Bảng

### CMD-332 · TABLE, -TABLE: tạo bảng và API dựng bảng từ lưới ô
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-002, DWG-014, RGN-166, DOC-007, DOC-021, INT-003, INT-004, INT-012
- **Lệnh:** TABLE, -TABLE
- **Màn hình:** UI-F06
- **Đầu vào:** 03 §9; 07.2 UI-F06; 07.1 §7.4 (panel Bảng); prototype `design/prototype/js/screens9.js` `IMPL.TABLE`; API RGN-166 (sinh bảng từ dữ liệu ô và kiểu ô).
- **Đầu ra:** `commands/annot/table/`: `TableBuilder` (số cột, rộng cột, số hàng, cao hàng theo số dòng, kiểu ô hàng đầu/hàng hai/còn lại, lưới chuỗi đầu vào) dùng cho TCEXCELPASTE (VN-013) và trích xuất dữ liệu; TABLE mở UI-F06 (chỉ điểm chèn / chỉ khung); -TABLE (số cột hoặc Auto, số hàng hoặc Auto, điểm chèn hoặc Style/Width/Height); kiểu bảng Standard tự tạo khi thiếu.
- **Tiêu chí nghiệm thu:**
  1. 60 ca vàng (-TABLE, hai cách chèn, Auto theo khung): lưới, kiểu ô, kích thước khớp QA-007 từng trường.
  2. Bảng tạo ra mở trong DWG TrueView không lỗi (QA-004), hiển thị SSIM ≥ 0,98.
  3. Dựng bảng 1.000 hàng × 20 cột ≤ 200 ms p99 **[đề xuất]**; 1 mark undo.
  4. Đạt KTL với -TABLE.
  5. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** dán từ Excel (VN-013); kiểu bảng (CMD-334).

### CMD-333 · TABLEDIT: sửa chữ trong ô, di chuyển giữa các ô
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-313, CMD-315, CMD-332, RGN-166
- **Lệnh:** TABLEDIT
- **Đầu vào:** 03 §9 (sửa text trong ô ở P0); 07.1 §9.10 (Ô bảng); 07.2 UI-F07 (Tab/Enter di chuyển); prototype `design/prototype/js/screens9.js` `SC.tableCell`; API RGN-166.
- **Đầu ra:** `commands/annot/table/celledit/`: hit test ô (kể cả ô gộp, bảng xoay); trình soạn CMD-313 đặt trong ô (rộng = rộng ô trừ lề, canh theo kiểu ô); Tab/Shift+Tab, Enter, mũi tên ở biên chữ chuyển ô; cao hàng tự giãn theo nội dung; đăng ký vào `AnnotEditorRegistry` (CMD-315); TABLEDIT ("Chọn ô bảng").
- **Tiêu chí nghiệm thu:**
  1. 80 ca vàng (một dòng, nhiều dòng, định dạng, ô gộp, bảng xoay): chuỗi ô và cao hàng khớp QA-007.
  2. Ô khóa nội dung hoặc bảng trên layer khóa: báo, không mở trình soạn.
  3. Một lần TABLEDIT sửa nhiều ô = 1 mark undo; bảng 1.000 hàng: kết thúc soạn đến khung hình ≤ 50 ms p99 **[đề xuất]**.
  4. Gõ trong ô đạt ngân sách của CMD-313.
  5. Đạt KTL. Đạt DoD-C, DoD-L.

### CMD-334 · TABLESTYLE: kiểu bảng, kiểu ô, viền, kiểu mẫu TCVN
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-332, VN-008
- **Lệnh:** TABLESTYLE
- **Màn hình:** UI-D04
- **Đầu vào:** 07.2 UI-D04; prototype `design/prototype/js/screens9.js` `SC.tableStyle`, `SC.tableStyleEdit`; kiểu bảng TCVN (VN-008).
- **Đầu ra:** `commands/annot/table/style/`: `TableStyleService` (tạo, tạo từ bảng có sẵn, sửa, đổi tên, xóa, CTABLESTYLE; hướng xuống/lên); kiểu ô Tiêu đề/Đầu cột/Dữ liệu và tùy biến: Chung (nền, canh, định dạng, loại, lề, gộp khi tạo), Chữ (kiểu, cao, màu, góc), Viền (độ dày, linetype, màu, đôi, khoảng, cạnh); nạp kiểu mẫu thống kê thép, diện tích.
- **Tiêu chí nghiệm thu:**
  1. Round-trip QA-003 100% trường TABLESTYLE và kiểu ô; 20 kiểu QA-001 hiển thị SSIM ≥ 0,98.
  2. Sửa kiểu: bảng dùng kiểu cập nhật, ô có ghi đè giữ giá trị; 1 mark undo.
  3. Xóa kiểu đang dùng bị chặn; Standard không xóa được.
  4. Đạt DoD-C.

### CMD-335 · Ô bảng: vùng ô, chèn/xóa hàng cột, gộp/tách, sao chép ô, grip
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** CMD-333, INT-001, INT-013
- **Màn hình:** UI-F07
- **Đầu vào:** 07.2 UI-F07 (Hàng, Cột, Gộp, thao tác trên lưới bảng); 07.1 §7.11 (tab Ô bảng); API INT-013.
- **Đầu ra:** `commands/annot/table/cells/`: `CellRange` (nhấp, kéo, Shift); chèn hàng trên/dưới, cột trái/phải, xóa; gộp tất cả/theo hàng/theo cột, tách; Ctrl+C/V giữa các ô giữ định dạng; grip bảng (dời, rộng cột, cao hàng, kéo dãn đều); mỗi thao tác là lệnh nội bộ ẩn đăng ký INT-001 để UI-F07 gọi qua INT-002 **[đề xuất]**; sự kiện đổi cấu trúc cho CMD-337.
- **Tiêu chí nghiệm thu:**
  1. 150 ca vàng (ở biên, trong vùng gộp, gộp chồng, tách) khớp QA-007 lưới và kiểu ô.
  2. 100.000 chuỗi thao tác ngẫu nhiên: lưới luôn hợp lệ (vùng gộp không chồng, chỉ số liền, mọi ô có chủ).
  3. Mỗi thao tác = 1 mark undo, ≤ 20 ms p99 trên bảng 1.000 × 20.
  4. Rê grip ≤ 8 ms/khung.
  5. Đạt DoD-C, DoD-L.

### CMD-336 · Ô bảng: canh lề, kiểu dữ liệu, khóa, nền, viền, khớp ô
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-334, CMD-335, RGN-013
- **Màn hình:** UI-F07
- **Đầu vào:** 07.2 UI-F07 (Kiểu ô, Định dạng ô); API RGN-013.
- **Đầu ra:** `commands/annot/table/cellformat/`: canh 9 vị trí; khóa (mở/nội dung/định dạng/cả hai); kiểu dữ liệu góc, tiền tệ, ngày, thập phân, chung, phần trăm, điểm, chữ, số nguyên với định dạng hiển thị qua RGN-013; màu nền; sửa viền theo cạnh; khớp ô; gán kiểu ô.
- **Tiêu chí nghiệm thu:**
  1. 300 ca kiểu dữ liệu × định dạng: chuỗi hiển thị khớp QA-007 từng ký tự; ngày dd/MM/yyyy khi chọn kiểu Việt Nam.
  2. Ô khóa định dạng từ chối đổi định dạng nhưng vẫn sửa được nội dung, và ngược lại.
  3. Mỗi thao tác = 1 mark undo.
  4. Đạt DoD-C.

### CMD-337 · Công thức trong ô: Tổng, Trung bình, Đếm, Ô, Biểu thức
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-335, DOC-022, RGN-025
- **Màn hình:** UI-F07
- **Đầu vào:** 03 §9 (công thức); 07.2 UI-F07 (Chèn › công thức); prototype `design/prototype/js/screens9.js` `SC.tableEval`; API DOC-022, RGN-025.
- **Đầu ra:** `commands/annot/table/formula/`: nhập `=…` trong ô hoặc nút Chèn công thức; Sum, Average, Count trên vùng ô, Cell (ô của bảng khác), Equation; lưu dạng field biểu thức như AutoCAD; đồ thị phụ thuộc giữa các ô, tính lại khi ô nguồn đổi; vòng tham chiếu hiện `####`; dịch tham chiếu (A1:B3) khi chèn/xóa hàng, cột.
- **Tiêu chí nghiệm thu:**
  1. 200 ca vàng: giá trị và chuỗi field khớp QA-007.
  2. Bảng 10.000 ô công thức: đổi một ô nguồn → tính lại ≤ 10 ms p99 **[đề xuất]**; vòng tham chiếu được phát hiện 100%, không treo.
  3. Chèn/xóa hàng, cột: tham chiếu dịch đúng 100% trên 1.000 ca ngẫu nhiên so với mô hình tham chiếu.
  4. Đạt DoD-C.

### CMD-338 · TINSERT: chèn block vào ô
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-335, DOC-012
- **Lệnh:** TINSERT
- **Đầu vào:** 03 §9 (chèn block vào ô); 07.2 UI-F07 (Chèn › block).
- **Đầu ra:** `commands/annot/table/tinsert.cpp`: chọn ô → tên block → canh trong ô, tỉ lệ, xoay, tự khớp; giá trị attribute.
- **Tiêu chí nghiệm thu:**
  1. 40 ca vàng (tự khớp bật/tắt, ô gộp, block có attribute) khớp QA-007.
  2. Đạt KTL. Đạt DoD-C.

### CMD-339 · TABLEEXPORT: xuất bảng ra CSV và XLSX
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-332, VN-013
- **Lệnh:** TABLEEXPORT
- **Đầu vào:** 03 §9; 07.1 §7.4 (Xuất bảng CSV/Excel); bộ ghi XLSX của VN-013.
- **Đầu ra:** `commands/annot/table/export/`: CSV (dấu phẩy như AutoCAD; UTF-8 có BOM để Excel đọc đúng tiếng Việt **[đề xuất]**) và XLSX; ô gộp ghi ở ô trên trái; giá trị hiển thị (đã định dạng, đã tính công thức); FILEDIA = 0 hỏi đường dẫn trên dòng lệnh.
- **Tiêu chí nghiệm thu:**
  1. 50 bảng QA-001: giá trị từng ô trùng tệp CSV do AutoCAD TABLEEXPORT xuất (QA-007).
  2. XLSX mở được trong Excel và LibreOffice, giữ ô gộp.
  3. Bảng 10.000 hàng ≤ 500 ms **[đề xuất]**. Đạt KTL.
  4. Đạt DoD-C.

## 7. Field và hyperlink

### CMD-340 · FIELD: chèn field, định dạng, xem trước
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-313, DOC-017, DOC-022, RGN-013, RGN-025, VN-018
- **Lệnh:** FIELD
- **Màn hình:** UI-F08
- **Đầu vào:** 03 §9 (field cho khung tên); 07.2 UI-F08, UI-F01 (Chèn › field); prototype `design/prototype/js/screens9.js` `SC.fieldDlg`, `IMPL.FIELD`; API DOC-022, RGN-025, DOC-017.
- **Đầu ra:** `commands/annot/field/`: danh mục field theo nhóm UI-F08; sinh mã field (`\AcVar`, `\AcObjProp`, `\AcExpr`, `\AcDiesel`) kèm định dạng (hoa, thường, viết hoa chữ đầu, ngày dd/MM/yyyy, đơn vị qua RGN-013); xem trước giá trị; FIELD tạo MTEXT chứa field; chèn tại caret trong CMD-313 (field là đơn vị nguyên tử); sysvar FIELDDISPLAY, FIELDEVAL; Số tờ/Tổng số tờ lấy từ VN-018.
- **Tiêu chí nghiệm thu:**
  1. Mỗi field trong UI-F08: mã sinh ra được AutoCAD đọc cho cùng giá trị (QA-007, 150 ca).
  2. Field Đối tượng: mọi thuộc tính DOC-017 của LINE, CIRCLE, LWPOLYLINE, HATCH, INSERT (attribute) khớp từng ký tự.
  3. Caret không dừng trong field; xóa field xóa cả khối; 1 mark undo.
  4. Đạt DoD-C.

### CMD-341 · UPDATEFIELD và cập nhật tự động theo FIELDEVAL
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-002, DOC-009, DOC-022, RGN-025, INT-008, NT-005
- **Lệnh:** UPDATEFIELD
- **Đầu vào:** 03 §9; sysvar FIELDEVAL; API RGN-025 (đánh giá), DOC-009 (snapshot).
- **Đầu ra:** `commands/annot/field/update.cpp`: UPDATEFIELD (field trong TEXT, MTEXT, ATTRIB, MLEADER, ô bảng, chữ dim, block lồng; báo số field tìm thấy và đã cập nhật); móc cập nhật khi mở, lưu, in, ETRANSMIT, regen theo bit FIELDEVAL.
- **Tiêu chí nghiệm thu:**
  1. Giá trị sau cập nhật khớp QA-007 (100 ca); field lỗi hiển thị `####`.
  2. 100.000 field: đánh giá song song trên snapshot, ghi trong 1 transaction, ≤ 1 s p99 trên 8 luồng **[đề xuất]**; 1 mark undo.
  3. Mỗi bit FIELDEVAL bật đúng móc tương ứng, tắt thì không đánh giá (đếm số lần gọi).
  4. Đạt KTL. Đạt DoD-C, DoD-H.

### CMD-342 · HYPERLINK, -HYPERLINK: gắn, sửa, bỏ liên kết
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-002, DOC-005, DOC-007, INT-003, INT-008
- **Lệnh:** HYPERLINK, -HYPERLINK
- **Màn hình:** UI-F15
- **Đầu vào:** 07.2 UI-F15; xdata `PE_URL` như AutoCAD (DOC-005); sysvar HYPERLINKBASE.
- **Đầu ra:** `commands/annot/hyperlink/`: đọc/ghi liên kết (URL hoặc tệp, vị trí có tên là view/layout, mô tả); HYPERLINK mở UI-F15; -HYPERLINK (Remove/Insert, Area/Object, liên kết, vị trí có tên, mô tả); `resolve(link)` cho Ctrl+bấm (UI) và xuất PDF (PLT-004).
- **Tiêu chí nghiệm thu:**
  1. 60 ca vàng: xdata khớp AutoCAD từng byte (QA-007); AutoCAD mở thấy cùng liên kết.
  2. Đường dẫn tương đối theo HYPERLINKBASE và thư mục bản vẽ; liên kết tới view/layout trả đúng đích.
  3. Đạt KTL với -HYPERLINK.
  4. Đạt DoD-C.

## 8. Kích thước: lõi dựng, xem trước, kiểu, liên kết

### CMD-401 · Lõi dựng kích thước: số đo, điểm định nghĩa, entity DIMENSION, block `*D`
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DOC-004, DOC-007, DOC-008, DOC-012, GEO-002, GEO-005, GEO-006, RGN-013, RGN-014, RGN-015, NT-005, QA-003, QA-004, QA-009, QA-110, RGN-162, RGN-163, RGN-164
- **Đầu vào:** 03 §8; 05 §1.2 (chữ kích thước khớp 100%), 05 §8 (dòng Kích thước); dàn kích thước RGN-014, RGN-015 (gồm RGN-162, RGN-163, RGN-164); định dạng số RGN-013; bộ tham chiếu chữ QA-110.
- **Đầu ra:**
  - `commands/annot/dim/DimBuilder.{h,cpp}`: dựng 9 loại (xoay, nghiêng, góc 2 đường, góc 3 điểm, bán kính, đường kính, độ dài cung, gãy khúc, tọa độ) từ điểm định nghĩa, kiểu hiện hành và ghi đè; số đo có DIMLFAC; chữ ghi đè có `<>`.
  - `DimBlockWriter`: ghi block vô danh `*D` từ kết quả dàn của RGN; đánh dấu bẩn, ghi lại theo lô song song khi lưu hoặc khi cần **[đề xuất]**.
  - DIMASSOC = 0 (tách thành LINE, SOLID, MTEXT) và 1 (không liên kết); `commands/annot/dim/tests/`, `bench/`.
- **Tiêu chí nghiệm thu:**
  1. Số đo sai số ≤ 1e-9 × kích thước bản vẽ trên 1 triệu ca QA-009 cho cả 9 loại, kể cả tọa độ VN-2000 (≈ 10⁹ đơn vị).
  2. Chữ kích thước của dim dựng ra khớp 100% từng ký tự với bộ tham chiếu QA-110 (LUNITS, DIMZIN, DIMRND, phân số, dung sai, đơn vị phụ) và với QA-007 khi có đáp án.
  3. Dim tạo ra round-trip 100% trường (QA-003); DWG TrueView và ODA File Converter mở không lỗi, không hỏi recover (QA-004).
  4. Tạo một dim (entity và `*D`) ≤ 20 ms p99 (DoD-L); ghi lại 100.000 block `*D` ≤ 1,5 s trên 8 luồng **[đề xuất]**, tất định mọi số luồng.
  5. Đạt DoD-C, DoD-H.

### CMD-402 · Jig và tùy chọn chữ dùng chung của lệnh dim: xem trước ≤ 8 ms/khung, Mtext/Text/Angle
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-401, CMD-314, INT-003, INT-012, INT-014, RND-012, NT-004, QA-012
- **Đầu vào:** 07.2 UI-E10 (cột Xem trước: "Kích thước tạm"); DoD-L; API jig INT-012, nhập động INT-014; prototype `annot.js` (`IMPL.DIM`, `dimChain`).
- **Đầu ra:** `commands/annot/dim/DimJig.{h,cpp}` cho mọi lệnh dim và grip: dim tạm dàn lại mỗi khung, không ghi DB; cache glyph theo chuỗi chữ, mũi tên, DIMVAR đã giải; ô DYN cho số đo và góc; chế độ chữ đi theo con trỏ; tùy chọn chung Mtext (trình soạn MTEXT của CMD-314, chữ có `<>`), Text, Angle. Benchmark `commands/annot/dim/bench/dim_jig_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Cập nhật jig ≤ 8 ms/khung p99 (tầm trung), ≤ 16 ms (máy thấp) trên cảnh BENCH-STD 1 triệu entity, đủ 9 loại dim, có dung sai và đơn vị phụ.
  2. Từ khung thứ hai: 0 lần cấp phát mỗi khung (NT-004).
  3. Primitive xem trước trùng từng bit với primitive của dim được tạo tại cùng điểm (so hash).
  4. Esc ở mọi bước: hash DB không đổi, không sinh mark undo.
  5. Cùng chuỗi chữ ghi đè thì Mtext (trình soạn CMD-314) và Text (`.scr`) cho dim giống từng bit; không có trình soạn thứ hai.
  6. Đạt DoD-C, DoD-L.

### CMD-403 · Engine kiểu kích thước: kiểu, kiểu con, ghi đè, cập nhật dim theo kiểu
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** CMD-401, DOC-004, DOC-005, DOC-010, DOC-017, INT-005, RGN-003, RGN-014, QA-001, QA-003, RGN-161
- **Đầu vào:** 03 §8 (toàn bộ DIMVAR, ghi đè, kiểu con); 07.2 UI-D02 (Mới, Ghi đè, So sánh, "dùng cho"); schema DIMSTYLE qua DOC-004.
- **Đầu ra:** `commands/annot/dim/DimStyleEngine.{h,cpp}`:
  - tạo, sao chép, đổi tên, xóa (chặn kiểu đang dùng, kiểu hiện hành, kiểu của xref); kiểu con `$0 $2 $3 $4 $6 $7`;
  - ghi đè hiện hành qua sysvar `DIMxxx` trong header, lưu ghi đè vào kiểu; ghi đè theo entity (xdata `ACAD` `DSTYLE`);
  - So sánh hai kiểu (danh sách DIMVAR khác nhau); sửa kiểu thì đánh dấu và sinh lại mọi dim dùng kiểu, theo lô.
- **Tiêu chí nghiệm thu:**
  1. Mọi DIMVAR trong danh sách phủ RGN-161 đặt được qua sysvar, lưu vào kiểu, đọc lại đúng kiểu và miền giá trị (INT-005).
  2. Bất biến: DIMVAR hiệu lực = kiểu cha, rồi kiểu con theo loại dim, rồi ghi đè entity; 100.000 ca ngẫu nhiên khớp bộ giải của RGN-014.
  3. Xdata ghi đè do AutoCAD tạo trên QA-001 round-trip 100% trường (QA-003) và cho đúng chữ kích thước.
  4. Sửa kiểu dùng bởi 100.000 dim: xong ≤ 1 s trên 8 luồng **[đề xuất]**, hiệu suất ≥ 60% đến 8 luồng; một mark undo.
  5. Đạt DoD-C, DoD-H.

### CMD-404 · DIMSTYLE, -DIMSTYLE
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-403, INT-001, INT-003, INT-007, INT-019, LSP-006, NT-012, VN-008, QA-007
- **Lệnh:** DIMSTYLE, -DIMSTYLE
- **Màn hình:** UI-D02
- **Đầu vào:** 07.2 UI-D02; hành vi `-DIMSTYLE` của AutoCAD; kiểu mẫu TCVN của VN-008 (`TCVN-1-100`, `TCVN-1-50`).
- **Đầu ra:**
  - `commands/annot/dim/DimStyleCmd.cpp`: `-DIMSTYLE` đủ ANnotative, Save, Restore (kể cả `~tên` để liệt kê khác biệt), STatus, Variables, Apply, `?`; DIMSTYLE gọi móc mở UI-D02, khi chạy không giao diện (`trion-console`) thì chạy như `-DIMSTYLE` **[đề xuất]**.
  - `docs/specs/commands/DIMSTYLE.md`, `docs/specs/commands/-DIMSTYLE.md`; ca vàng `.scr` trong `commands/annot/dim/tests/ref/`.
- **Tiêu chí nghiệm thu:**
  1. Văn bản in ra của STatus, Variables, `?` khớp từng ký tự với QA-007 (trước khi có đáp án: ca vàng).
  2. Apply đưa N dim đã chọn về kiểu hiện hành, bỏ ghi đè entity; dòng lệnh báo đúng N.
  3. Tên kiểu dài quá 255 ký tự, có ký tự cấm, hoặc trùng (không phân biệt hoa thường có dấu, NT-012) bị từ chối kèm thông báo.
  4. Bàn phím, `.scr`, `(command "-DIMSTYLE" …)` cho DB giống từng bit; một mark undo mỗi lần chạy, U trả hash DB về trước lệnh.
  5. Đạt DoD-C.

### CMD-405 · Mô hình liên kết kích thước với hình học (DIMASSOC = 2)
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-401, DOC-005, DOC-012, DOC-013, DOC-015, INT-010, QA-001, QA-003
- **Đầu vào:** 03 §8 (dòng DIMASSOC, P0); đối tượng liên kết kích thước và tham chiếu điểm bắt trong "Đặc tả DWG của ODA v5.4.1"; kết quả bắt điểm INT-010.
- **Đầu ra:** `commands/annot/dim/DimAssoc.{h,cpp}`:
  - tham chiếu điểm: đối tượng (kể cả đường dẫn qua block lồng, xref), đoạn con, chế độ bắt (END, MID, CEN, QUA, INT, NEA, NOD, INS), tham số trên đường cong;
  - tạo từ kết quả OSNAP khi đặt dim; liên kết một phần; giải lại điểm; đánh dấu tham chiếu hỏng khi hình học bị xóa;
  - đọc và ghi giữ nguyên liên kết do AutoCAD tạo.
- **Tiêu chí nghiệm thu:**
  1. Mỗi chế độ bắt × mỗi loại hình học (LINE, ARC, CIRCLE, ELLIPSE, LWPOLYLINE có cung, SPLINE, POINT, INSERT, kể cả trong block lồng 3 cấp): điểm giải lại trùng điểm bắt ban đầu theo `Tolerance` (GEO-002).
  2. Tệp QA-001 có dim liên kết: round-trip 100% trường (QA-003); mở lại thì số dim liên kết không đổi.
  3. Xóa rồi undo hình học: tham chiếu hỏng rồi phục hồi; AUDIT (DOC-015) không báo handle treo.
  4. Đạt DoD-C.

## 9. Lệnh kích thước P0

### CMD-406 · DIMLINEAR, DIMALIGNED
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-401, CMD-402, CMD-403, CMD-405, RGN-008, INT-002, INT-003, INT-004, INT-007, INT-008, INT-010, LSP-006, QA-007
- **Lệnh:** DIMLINEAR, DIMALIGNED
- **Đầu vào:** 07.2 UI-E10 (dòng `DIMLINEAR`); 03 §8; prototype `annot.js` (`addDim`), `screens13.js` (`dimOpts`).
- **Đầu ra:** `commands/annot/dim/DimLinearCmd.cpp`, `DimAlignedCmd.cpp`; `docs/specs/commands/DIMLINEAR.md`, `DIMALIGNED.md`; ca vàng `.scr`.
- **Tiêu chí nghiệm thu:**
  1. Đủ prompt: gốc gióng 1 hoặc Enter để chọn đối tượng (LINE, ARC, CIRCLE, đoạn polyline) → gốc 2 → vị trí đường dim; Mtext, Text, Angle (CMD-402); DIMLINEAR thêm Horizontal, Vertical, Rotated và tự chọn ngang/dọc theo con trỏ.
  2. Chữ kích thước khớp 100% từng ký tự với QA-007 của hai lệnh; điểm định nghĩa sai số ≤ 1e-9 × kích thước bản vẽ.
  3. Ca biên: hai gốc trùng (báo, hỏi lại), Rotated 90°/270°, UCS xoay, gốc bắt trong block lồng; DIMASSOC 0/1/2 cho đúng loại kết quả.
  4. Xem trước ≤ 8 ms/khung p99; bàn phím, `.scr`, `(command "_.DIMLINEAR" …)` cho DB giống từng bit; một mark undo mỗi lệnh.
  5. Đạt DoD-C, DoD-L.

### CMD-407 · DIMANGULAR
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-401, CMD-402, CMD-405, GEO-008, INT-003, INT-004, INT-007, INT-008, INT-010, LSP-006, QA-007
- **Lệnh:** DIMANGULAR
- **Đầu vào:** 07.2 UI-E10 (dòng dim); 03 §8; giao điểm GEO-008.
- **Đầu ra:** `commands/annot/dim/DimAngularCmd.cpp`; `docs/specs/commands/DIMANGULAR.md`; ca vàng `.scr`.
- **Tiêu chí nghiệm thu:**
  1. Đủ prompt: chọn cung (góc ở tâm), tròn (điểm chọn và điểm thứ hai), hai đường (góc theo góc phần tư của con trỏ), Enter để chỉ định đỉnh và hai điểm; Mtext, Text, Angle, Quadrant.
  2. Hai đường song song: báo lỗi, hỏi lại; hai đoạn không cắt nhau: dùng giao điểm kéo dài, vẽ đường gióng; góc > 180° với cách 3 điểm.
  3. Góc đo trong dung sai góc của `Tolerance`; chữ khớp 100% QA-007 với mọi AUNITS (độ, độ/phút/giây, grad, radian, khảo sát) và DIMADEC.
  4. Xem trước ≤ 8 ms/khung p99; bàn phím, `.scr`, `(command)` cho DB giống từng bit; một mark undo.
  5. Đạt DoD-C, DoD-L.

### CMD-408 · DIMRADIUS, DIMDIAMETER
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-401, CMD-402, CMD-405, INT-003, INT-007, INT-008, INT-009, LSP-006, QA-007
- **Lệnh:** DIMRADIUS, DIMDIAMETER
- **Đầu vào:** 07.2 UI-E10; 03 §8; DIMVAR liên quan (DIMCEN, DIMTMOVE, DIMATFIT, DIMTIX, DIMUPT).
- **Đầu ra:** `commands/annot/dim/DimRadialCmd.cpp`; `docs/specs/commands/DIMRADIUS.md`, `DIMDIAMETER.md`; ca vàng `.scr`.
- **Tiêu chí nghiệm thu:**
  1. Đủ prompt: chọn cung hoặc tròn (kể cả cung của LWPOLYLINE) → vị trí đường dim; Mtext, Text, Angle; tiền tố R và Ø; dấu tâm theo DIMCEN; đường gióng cung khi điểm đặt nằm ngoài cung.
  2. Chọn ellipse, line: báo không hợp lệ, hỏi lại; cung rất nhỏ: chữ và mũi tên ra ngoài theo DIMATFIT, DIMTMOVE.
  3. Số đo sai số ≤ 1e-9 × kích thước bản vẽ; chữ khớp 100% QA-007.
  4. Xem trước ≤ 8 ms/khung p99; bàn phím, `.scr`, `(command)` cho DB giống từng bit; một mark undo.
  5. Đạt DoD-C, DoD-L.

### CMD-409 · DIMCONTINUE, DIMBASELINE
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-402, CMD-405, CMD-406, CMD-407, INT-003, INT-007, INT-008, LSP-006, QA-007
- **Lệnh:** DIMCONTINUE, DIMBASELINE
- **Đầu vào:** 07.2 UI-E10; 03 §8; prototype `annot.js` (`dimChain`).
- **Đầu ra:** `commands/annot/dim/DimChainCmd.cpp`; `docs/specs/commands/DIMCONTINUE.md`, `DIMBASELINE.md`; ca vàng `.scr`.
- **Tiêu chí nghiệm thu:**
  1. Đủ prompt: lấy dim vừa tạo trong phiên (thẳng, nghiêng, góc) hoặc Select; gốc gióng tiếp theo lặp đến Enter; Undo bỏ dim cuối trong lệnh.
  2. DIMBASELINE cách nhau đúng DIMDLI ± `Tolerance`; DIMCONTINUE thẳng hàng đường dim; DIMCONTINUEMODE chọn kiểu và layer của dim gốc hay hiện hành.
  3. Dim mới liên kết với hình học khi gốc lấy bằng OSNAP; chữ khớp 100% QA-007.
  4. Xem trước ≤ 8 ms/khung p99; bàn phím, `.scr`, `(command)` giống từng bit; cả lần chạy là một mark undo.
  5. Đạt DoD-C, DoD-L.

### CMD-410 · DIMEDIT, DIMTEDIT
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-401, CMD-402, CMD-403, CMD-314, CMD-315, RGN-008, INT-003, INT-007, INT-008, LSP-006, QA-007, QA-110
- **Lệnh:** DIMEDIT, DIMTEDIT
- **Đầu vào:** 07.2 UI-E10 (dòng `DIMEDIT`, `DIMTEDIT`); 03 §8 (ghi đè chữ `<>`); prototype `annot.js` (`IMPL.DIMEDIT`, `IMPL.DIMTEDIT`).
- **Đầu ra:** `commands/annot/dim/DimEditCmd.cpp`; trình sửa chữ dim đăng ký vào registry TEXTEDIT (CMD-315): TEXTEDIT, nhấp đúp trên dim mở trình soạn CMD-314 với chữ ghi đè; `docs/specs/commands/DIMEDIT.md`, `DIMTEDIT.md`; ca vàng `.scr`.
- **Tiêu chí nghiệm thu:**
  1. DIMEDIT: Home, New (chữ có `<>`, soạn trong trình soạn CMD-314), Rotate, Oblique trên nhiều dim; PICKFIRST.
  2. DIMTEDIT: chọn dim → chữ đi theo con trỏ, hoặc Left, Right, Center, Home, Angle; Left/Right chỉ áp cho dim thẳng, bán kính, đường kính (loại khác báo bỏ qua).
  3. DIMEDIT New, TEXTEDIT, nhấp đúp cho cùng chuỗi ghi đè, round-trip từng ký tự (kể cả `\P`, `%%c`, nhiều `<>`); chữ khớp 100% QA-110 và QA-007.
  4. Kéo chữ ≤ 8 ms/khung p99; bàn phím, `.scr`, `(command)` giống từng bit; một mark undo mỗi lệnh.
  5. Đạt DoD-C, DoD-L.

## 10. Cập nhật liên kết và grip kích thước

### CMD-411 · Engine cập nhật kích thước liên kết khi hình học đổi
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** CMD-405, CMD-406, CMD-407, CMD-408, DOC-007, DOC-009, NT-005, QA-009, CMD:MOVE, CMD:STRETCH, CMD:SCALE
- **Đầu vào:** 03 §8 (DIMASSOC P0); sự kiện thay đổi theo lô DOC-007; tham chiếu điểm CMD-405.
- **Đầu ra:** `commands/annot/dim/DimAssocUpdater.{h,cpp}`: chỉ mục ngược hình học → dim (cả qua block lồng); nhận sự kiện theo lô, giải lại điểm định nghĩa, sinh lại dim trong cùng transaction của lệnh sửa; song song khi lô lớn; dim và hình học cùng bị sửa thì chỉ cập nhật một lần; hình học bị xóa thì dim mất liên kết điểm đó, giữ vị trí. Kiểm thử bất biến, benchmark.
- **Tiêu chí nghiệm thu:**
  1. Bất biến: sau MOVE, STRETCH, SCALE hình học (lệnh thật) và sau 1 triệu biến đổi ngẫu nhiên QA-009 (dời, xoay, tỉ lệ, đối xứng, kéo đỉnh), số đo của mọi dim liên kết = số đo hình học ± `Tolerance`; chữ cập nhật đúng từng ký tự.
  2. Sửa một đối tượng có 50 dim liên kết: thêm ≤ 5 ms p99 vào thời gian lệnh **[đề xuất]**.
  3. MOVE 100.000 đối tượng kèm 100.000 dim liên kết ≤ 1 s trên 8 luồng **[đề xuất]**, hiệu suất ≥ 60%; tất định mọi số luồng.
  4. Undo/redo lệnh sửa trả về hash DB cũ của cả hình học và dim; cập nhật của dim không kích hoạt lại sự kiện (không vòng lặp).
  5. Đạt DoD-C, DoD-H, DoD-L.

### CMD-412 · Grip kích thước và menu grip đa năng
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-402, CMD-405, CMD-409, CMD-410, INT-007, INT-013, LSP-006, QA-003, QA-007
- **Lệnh:** AIDIMFLIPARROW, AIDIMTEXTMOVE
- **Màn hình:** UI-F05
- **Đầu vào:** 07.1 §9.5 (grip kích thước, menu đa năng); 07.2 UI-F05; API đăng ký grip INT-013.
- **Đầu ra:** `commands/annot/dim/DimGrips.{h,cpp}`: grip cho 9 loại dim (gốc gióng, đường dim, chữ; điểm trên cung; đỉnh góc; điểm đặc trưng và đầu leader của dim tọa độ); menu: Kéo giãn, Nối tiếp dim, Dim cùng gốc, Lật mũi tên, Vị trí chữ (đi cùng đường dim / chỉ di chuyển chữ / kèm đường dẫn / trên đường dim, giữa / đặt lại). `docs/specs/commands/AIDIMFLIPARROW.md`, `AIDIMTEXTMOVE.md`.
- **Tiêu chí nghiệm thu:**
  1. Mỗi mục menu cho DB giống từng bit với chuỗi lệnh tương đương (DIMCONTINUE, DIMBASELINE, DIMTEDIT, AIDIM…) chạy từ `.scr`.
  2. Kéo gốc gióng đang liên kết thì bỏ liên kết riêng điểm đó; kéo đường dim, chữ giữ liên kết.
  3. Kéo grip ≤ 8 ms/khung p99; hit test grip ≤ 1 ms; thả grip là một mark undo; tôn trọng GRIPOBJLIMIT.
  4. Cờ lật mũi tên round-trip qua DWG (QA-003), hiển thị khớp QA-007.
  5. Đạt DoD-C, DoD-L.

## 11. Lệnh kích thước P1

### CMD-413 · DIMARC, DIMCENTER
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-401, CMD-402, CMD-405, INT-003, INT-007, INT-008, LSP-006, QA-007
- **Lệnh:** DIMARC, DIMCENTER
- **Đầu vào:** 03 §8 (dòng DIMARC, DIMCENTER); 07.2 UI-D02 (ký hiệu độ dài cung, dấu tâm).
- **Đầu ra:** `commands/annot/dim/DimArcCmd.cpp`, `DimCenterCmd.cpp`; `docs/specs/commands/DIMARC.md`, `DIMCENTER.md`; ca vàng `.scr`.
- **Tiêu chí nghiệm thu:**
  1. DIMARC: chọn cung hoặc đoạn cung polyline → vị trí; Mtext, Text, Angle, Partial, Leader (cung > 90°); ký hiệu theo DIMARCSYM; đường gióng song song khi góc cung < 90°, hướng tâm khi lớn hơn.
  2. DIMCENTER: dấu tâm theo DIMCEN (dương: dấu; âm: dấu và đường tâm; 0: không) trên cung, tròn; kết quả là LINE không liên kết, khác CENTERMARK.
  3. Độ dài cung sai số ≤ 1e-9 × kích thước bản vẽ (cả cung > 180° và Partial); chữ khớp 100% QA-007.
  4. Xem trước ≤ 8 ms/khung p99; bàn phím, `.scr`, `(command)` giống từng bit; một mark undo.
  5. Đạt DoD-C, DoD-L.

### CMD-414 · DIMORDINATE
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-401, CMD-402, CMD-405, CMD-409, INT-003, INT-004, INT-007, INT-015, LSP-006, QA-007
- **Lệnh:** DIMORDINATE
- **Đầu vào:** 03 §8; UCS INT-015.
- **Đầu ra:** `commands/annot/dim/DimOrdinateCmd.cpp`; mở rộng DIMCONTINUE, DIMBASELINE cho dim tọa độ; `docs/specs/commands/DIMORDINATE.md`; ca vàng `.scr`.
- **Tiêu chí nghiệm thu:**
  1. Đủ prompt: điểm đặc trưng → điểm cuối leader; Xdatum, Ydatum, Mtext, Text, Angle; tự chọn X/Y theo hướng con trỏ; ORTHO giữ leader thẳng.
  2. Giá trị = tọa độ trong UCS lúc tạo (gốc lưu trong dim) ± `Tolerance`, kể cả UCS xoay và tọa độ âm; chữ khớp 100% QA-007.
  3. DIMCONTINUE, DIMBASELINE nhận dim tọa độ làm gốc.
  4. Xem trước ≤ 8 ms/khung p99; bàn phím, `.scr`, `(command)` giống từng bit; một mark undo.
  5. Đạt DoD-C, DoD-L.

### CMD-415 · DIMJOGGED, DIMJOGLINE
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-401, CMD-402, CMD-405, CMD-406, INT-003, INT-007, INT-008, LSP-006, QA-007
- **Lệnh:** DIMJOGGED, DIMJOGLINE
- **Đầu vào:** 03 §8; 07.2 UI-D02 (góc gãy bán kính, hệ số chiều cao gãy dim thẳng).
- **Đầu ra:** `commands/annot/dim/DimJogCmd.cpp`; `docs/specs/commands/DIMJOGGED.md`, `DIMJOGLINE.md`; ca vàng `.scr`.
- **Tiêu chí nghiệm thu:**
  1. DIMJOGGED: chọn cung hoặc tròn → vị trí tâm thay thế → vị trí đường dim → vị trí gãy; Mtext, Text, Angle; góc gãy theo DIMJOGANG.
  2. DIMJOGLINE: chọn dim thẳng hoặc nghiêng → vị trí gãy, hoặc Remove; chiều cao gãy = hệ số của kiểu × chiều cao chữ.
  3. Giá trị = bán kính thật ± `Tolerance` dù tâm thay thế ở đâu; chữ khớp 100% QA-007; Remove trả hash entity về trước khi gãy.
  4. Xem trước ≤ 8 ms/khung p99; bàn phím, `.scr`, `(command)` giống từng bit; một mark undo.
  5. Đạt DoD-C, DoD-L.

### CMD-416 · DIMBREAK
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-401, CMD-411, GEO-010, INT-003, INT-007, INT-008, LSP-006, QA-003, QA-007, QA-012
- **Lệnh:** DIMBREAK
- **Đầu vào:** 03 §8; 07.2 UI-D02 (cỡ ngắt dim); giao điểm hàng loạt GEO-010.
- **Đầu ra:** `commands/annot/dim/DimBreakCmd.cpp`; dữ liệu ngắt lưu trên dim; `docs/specs/commands/DIMBREAK.md`; ca vàng `.scr`.
- **Tiêu chí nghiệm thu:**
  1. Đủ prompt: chọn dim hoặc Multiple → đối tượng cắt, hoặc Auto, Manual (hai điểm), Remove; đối tượng cắt: dim, leader, line, cung, tròn, ellipse, spline, polyline, text, mtext, block.
  2. Ngắt Auto và theo đối tượng tự cập nhật khi dim hoặc đối tượng cắt di chuyển (qua CMD-411); Manual không tự cập nhật.
  3. Khoảng ngắt = cỡ ngắt của kiểu ± `Tolerance` tại mọi giao điểm; dữ liệu ngắt round-trip 100% (QA-003).
  4. Multiple + Auto trên 1.000 dim trong cảnh BENCH-STD 1 triệu entity ≤ 300 ms trên 8 luồng **[đề xuất]**; một mark undo; bàn phím, `.scr`, `(command)` giống từng bit.
  5. Đạt DoD-C, DoD-H.

### CMD-417 · DIMSPACE
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-401, CMD-406, CMD-407, INT-003, INT-007, INT-008, LSP-006, QA-007
- **Lệnh:** DIMSPACE
- **Đầu vào:** 03 §8; 07.1 §7.4 (Giãn cách dim).
- **Đầu ra:** `commands/annot/dim/DimSpaceCmd.cpp`; `docs/specs/commands/DIMSPACE.md`; ca vàng `.scr`.
- **Tiêu chí nghiệm thu:**
  1. Đủ prompt: dim gốc → các dim cần giãn → giá trị hoặc Auto (2 lần chiều cao chữ của kiểu dim gốc); giá trị 0 làm thẳng hàng.
  2. Áp cho dim thẳng song song và dim góc chung đỉnh; dim không hợp lệ bị bỏ qua, dòng lệnh báo số lượng.
  3. Khoảng cách đường dim kết quả = giá trị ± `Tolerance`; liên kết giữ nguyên.
  4. Bàn phím, `.scr`, `(command)` giống từng bit; một mark undo.
  5. Đạt DoD-C.

### CMD-418 · DIMOVERRIDE, DIMLAYER
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-401, CMD-403, INT-003, INT-005, INT-007, INT-008, LSP-006, QA-003, QA-007
- **Lệnh:** DIMOVERRIDE, DIMLAYER
- **Đầu vào:** 03 §8 (DIMOVERRIDE, DIMLAYER P1); 07.1 §7.4 (Ghi đè thuộc tính, dropdown layer mặc định cho dim).
- **Đầu ra:** `commands/annot/dim/DimOverrideCmd.cpp`; sysvar DIMLAYER dùng trong `DimBuilder`; `docs/specs/commands/DIMOVERRIDE.md`, `DIMLAYER.md`; ca vàng `.scr`.
- **Tiêu chí nghiệm thu:**
  1. DIMOVERRIDE: tên DIMVAR → giá trị (lặp đến Enter) → chọn dim; Clear overrides; PICKFIRST; tên hoặc giá trị sai báo lỗi theo miền INT-005.
  2. DIMLAYER: `.` là layer hiện hành, hoặc tên layer; mọi dim do lệnh của nhóm này tạo nằm trên layer đó; layer chưa có: theo AutoCAD, đối chiếu QA-007.
  3. Ghi đè round-trip qua xdata (QA-003); chữ khớp 100% QA-007 sau ghi đè.
  4. Bàn phím, `.scr`, `(command)` giống từng bit; một mark undo.
  5. Đạt DoD-C.

### CMD-419 · DIMREASSOCIATE, DIMDISASSOCIATE, DIMREGEN; kích thước xuyên không gian
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-405, CMD-411, DOC-011, RGN-024, INT-003, INT-007, INT-008, INT-010, LSP-006, QA-007
- **Lệnh:** DIMREASSOCIATE, DIMDISASSOCIATE, DIMREGEN
- **Đầu vào:** 03 §8 (P1); 07.1 §7.4 (Gán lại liên kết, Bỏ liên kết).
- **Đầu ra:** `commands/annot/dim/DimReassocCmd.cpp`; liên kết dim ở paper space với hình học model qua viewport; `docs/specs/commands/DIMREASSOCIATE.md`, `DIMDISASSOCIATE.md`, `DIMREGEN.md`; ca vàng `.scr`.
- **Tiêu chí nghiệm thu:**
  1. DIMREASSOCIATE: lần lượt từng điểm định nghĩa (đánh dấu X), chỉ điểm bằng OSNAP, Select object, Enter sang điểm kế giữ nguyên; dim góc, bán kính chọn đối tượng một lần.
  2. DIMDISASSOCIATE: lọc dim đang liên kết, báo số dim bỏ liên kết; PICKFIRST.
  3. Dim xuyên không gian: sau pan, zoom, đổi tỉ lệ viewport rồi DIMREGEN, số đo = độ dài model ± `Tolerance`; sửa hình học model thì dim tự cập nhật.
  4. Bàn phím, `.scr`, `(command)` giống từng bit; một mark undo.
  5. Đạt DoD-C.

### CMD-420 · QDIM
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** CMD-402, CMD-406, CMD-408, CMD-409, CMD-414, GEO-010, NT-005, INT-003, INT-007, INT-008, LSP-006, QA-007
- **Lệnh:** QDIM
- **Đầu vào:** 07.2 UI-E10 (dòng `QDIM`); 03 §8.
- **Đầu ra:** `commands/annot/dim/QDimCmd.cpp`: trích điểm (đầu mút, giao điểm theo seTtings), sắp theo hướng, khử trùng; sinh dãy dim theo chế độ; `docs/specs/commands/QDIM.md`; ca vàng `.scr`; benchmark.
- **Tiêu chí nghiệm thu:**
  1. Đủ tùy chọn: Continuous, Staggered, Baseline, Ordinate, Radius, Diameter, datumPoint, Edit (thêm/bớt điểm), seTtings (Endpoint/Intersection); chọn dãy dim có sẵn để thay.
  2. Tập điểm và dãy dim khớp QA-007; dim tạo ra liên kết với hình học; chữ khớp 100%.
  3. 10.000 đối tượng → dãy dim ≤ 500 ms trên 8 luồng **[đề xuất]**, hiệu suất ≥ 60%; tất định.
  4. Xem trước dãy ≤ 8 ms/khung p99 đến 500 dim, lớn hơn thì xem trước rút gọn **[đề xuất]**; một mark undo; bàn phím, `.scr`, `(command)` giống từng bit.
  5. Đạt DoD-C, DoD-H, DoD-L.

### CMD-421 · DIM: kích thước thông minh
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** CMD-406, CMD-407, CMD-408, CMD-409, CMD-413, CMD-414, CMD-417, CMD-418, INT-003, INT-007, INT-009, LSP-006, QA-007
- **Lệnh:** DIM
- **Màn hình:** UI-F05
- **Đầu vào:** 07.2 UI-E10 (dòng `DIM`), UI-F05 (phần Dim thông minh); prototype `annot.js` (`IMPL.DIM`).
- **Đầu ra:** `commands/annot/dim/DimSmartCmd.cpp`; `docs/specs/commands/DIM.md`; ca vàng `.scr`.
- **Tiêu chí nghiệm thu:**
  1. Rê lên đối tượng xem trước loại phù hợp: đoạn thẳng → thẳng/nghiêng, cung → bán kính (đổi được Diameter, Angular, Arc length), tròn → đường kính, hai đường → góc; chọn chỗ trống → gốc gióng.
  2. Đủ tùy chọn: Angular, Baseline, Continue, Ordinate, aliGn, Distribute (Equal, Offset), Layer, Undo; lệnh lặp đến Enter.
  3. Đặt trùng dim cũ: hỏi Thay thế / Giữ cả hai / Tách; cả ba nhánh có ca kiểm.
  4. Dim tạo bằng DIM giống từng bit dim của lệnh chuyên biệt với cùng đầu vào; xem trước khi rê ≤ 8 ms/khung p99 (hit test ≤ 1 ms); cả lần chạy là một mark undo.
  5. Đạt DoD-C, DoD-L.

### CMD-422 · TOLERANCE: khung dung sai hình học
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-315, DOC-007, RGN-004, VN-007, INT-003, INT-007, INT-018, INT-019, LSP-006, QA-001, QA-003, RGN-165
- **Lệnh:** TOLERANCE
- **Màn hình:** UI-F14
- **Đầu vào:** 03 §8 (TOLERANCE P1); 07.1 §7.4 (Dung sai hình học, UI-F14); font `gdt.shx` của VN-007.
- **Đầu ra:** `commands/annot/dim/ToleranceCmd.cpp`: tạo và sửa entity TOLERANCE (ký hiệu đặc tính, Ø, giá trị, điều kiện vật liệu M/L/S, chuẩn 1–3, vùng dung sai chiếu, định danh chuẩn, hai hàng); móc mở UI-F14; nhập dạng chuỗi FCF khi không có giao diện **[đề xuất]**; `docs/specs/commands/TOLERANCE.md`.
- **Tiêu chí nghiệm thu:**
  1. Chuỗi FCF đúng mã AutoCAD (`%%v`, `{\Fgdt;…}`, `^J` giữa hai hàng); round-trip 100% (QA-003) trên mẫu QA-001.
  2. Sửa FCF có sẵn qua registry TEXTEDIT (CMD-315) và nhấp đúp: mở UI-F14 với giá trị cũ, giữ nguyên các ô không đổi.
  3. Entity mang kiểu kích thước hiện hành (chiều cao chữ, khoảng hở, màu khung lấy từ kiểu); một mark undo; `(command "TOLERANCE" …)` trong `trion-console` dùng dạng chuỗi.
  4. Đạt DoD-C.
- **Ngoài phạm vi:** sinh hình khung FCF khi hiển thị và in (RGN-165).

## 12. Kích thước sau v1.0

### CMD-423 · DIMINSPECT, -DIMINSPECT
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** S · **Phụ thuộc:** CMD-401, CMD-403, INT-003, INT-007, INT-008, LSP-006, QA-003, QA-007
- **Lệnh:** DIMINSPECT, -DIMINSPECT
- **Đầu vào:** 03 §8 (DIMINSPECT P2); 07.1 §7.4 (Kiểm định, P2).
- **Đầu ra:** `commands/annot/dim/DimInspectCmd.cpp`; móc hộp thoại; `docs/specs/commands/DIMINSPECT.md`, `-DIMINSPECT.md`.
- **Tiêu chí nghiệm thu:**
  1. `-DIMINSPECT`: Add (hình Round, Angular, None; nhãn; tần suất kiểm %), Remove trên nhiều dim; PICKFIRST.
  2. Dữ liệu kiểm định round-trip 100% (QA-003); khung kiểm định hiển thị khớp QA-007.
  3. Bàn phím, `.scr`, `(command)` giống từng bit; một mark undo.
  4. Đạt DoD-C.

### CMD-424 · DIMEX, DIMIM: xuất/nhập kiểu kích thước
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** S · **Phụ thuộc:** CMD-403, NT-011, NT-012, INT-003, INT-007, LSP-006, QA-006
- **Lệnh:** DIMEX, DIMIM
- **Đầu vào:** 03 §8 (Express, P2); định dạng tệp `.dim` (văn bản) của Express Tools.
- **Đầu ra:** `commands/annot/dim/DimExImCmd.cpp`, bộ đọc/ghi `.dim`; fuzz target `commands/annot/dim/fuzz/dim_file_fuzz.cpp`; `docs/specs/commands/DIMEX.md`, `DIMIM.md`.
- **Tiêu chí nghiệm thu:**
  1. DIMEX rồi DIMIM sang bản vẽ khác giữ 100% DIMVAR, kể cả kiểu chữ (tạo theo tên nếu chưa có) và mũi tên block.
  2. Kiểu trùng tên: giữ hoặc ghi đè theo lựa chọn; tệp `.dim` mẫu của Express Tools nhập đúng.
  3. Tệp hỏng: báo dòng lỗi, DB không đổi; fuzz ≥ 1 giờ không lỗi (QA-006).
  4. Bàn phím, `.scr`, `(command)` giống từng bit; một mark undo.
  5. Đạt DoD-C.

### CMD-425 · QLATTACH, QLATTACHSET, QLDETACHSET
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** S · **Phụ thuộc:** RGN-016, DOC-005, INT-003, INT-007, INT-008, LSP-006, QA-003, CMD:QLEADER
- **Lệnh:** QLATTACH, QLATTACHSET, QLDETACHSET
- **Đầu vào:** Express Tools (P2): gắn LEADER vào chú thích MTEXT, TOLERANCE, INSERT.
- **Đầu ra:** `commands/annot/dim/QlAttachCmd.cpp`; `docs/specs/commands/QLATTACH.md`, `QLATTACHSET.md`, `QLDETACHSET.md`.
- **Tiêu chí nghiệm thu:**
  1. QLATTACH: chọn leader → chọn chú thích; QLATTACHSET gắn hàng loạt leader với chú thích tại đầu cuối leader; QLDETACHSET gỡ; dòng lệnh báo số lượng.
  2. Sau MOVE chú thích, đầu leader đi theo ± `Tolerance`; liên kết round-trip 100% (QA-003).
  3. Bàn phím, `.scr`, `(command)` giống từng bit; một mark undo.
  4. Đạt DoD-C.

## 13. Layout, không gian và viewport

### CMD-426 · LAYOUT: tạo, sao chép, xóa, đổi tên, mẫu, thứ tự tab
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** DOC-011, DOC-014, DOC-015, DOC-018, PLT-001, DWG-022, NT-011, NT-012, INT-003, INT-007, LSP-006, QA-004, QA-007
- **Lệnh:** LAYOUT
- **Màn hình:** UI-A10
- **Đầu vào:** 03 §13; 07.1 §9.12 (menu tab layout); 07.2 UI-E10 (dòng `LAYOUT`).
- **Đầu ra:** `commands/layout/LayoutOps.{h,cpp}` (API cho tab layout UI-A10: mới, sao chép, xóa nhiều, đổi tên, đổi thứ tự, đặt hiện hành) và `LayoutCmd.cpp`; `docs/specs/commands/LAYOUT.md`; ca vàng `.scr` trong `commands/layout/tests/ref/`.
- **Tiêu chí nghiệm thu:**
  1. Đủ tùy chọn: Copy, Delete, New, Template (chọn layout từ `.dwt`/`.dwg`/`.dxf`, mang theo block, layer, kiểu phụ thuộc), Rename, SAveas (layout ra `.dwt`), Set, `?`.
  2. Tên duy nhất (không phân biệt hoa thường có dấu), ≤ 255 ký tự, chặn ký tự cấm; không xóa được layout cuối cùng; layout mới có viewport theo LAYOUTCREATEVIEWPORT và thiết lập trang mặc định (PLT-001).
  3. Sau Copy, Template: AUDIT (DOC-015) không lỗi tham chiếu; tệp sau mọi tùy chọn mở trong DWG TrueView không hỏi recover (QA-004).
  4. Mỗi tùy chọn trừ Template, SAveas ≤ 20 ms p99 với layout ≤ 10.000 entity; một mark undo; bàn phím, `.scr`, `(command)` giống từng bit.
  5. Đạt DoD-C, DoD-L.

### CMD-428 · SCALELISTEDIT, -SCALELISTEDIT: danh sách tỉ lệ
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** DOC-007, DOC-021, INT-003, INT-007, LSP-006, QA-003
- **Lệnh:** SCALELISTEDIT, -SCALELISTEDIT
- **Màn hình:** UI-D11
- **Đầu vào:** 07.2 UI-D11 (nút, danh sách mặc định theo TCVN); 03 §13.
- **Đầu ra:** `commands/layout/ScaleList.{h,cpp}` (đối tượng SCALE trong dictionary danh sách tỉ lệ) và lệnh; `docs/specs/commands/SCALELISTEDIT.md`, `-SCALELISTEDIT.md`.
- **Tiêu chí nghiệm thu:**
  1. Add, Edit, Up/Down, Delete, Reset; `-SCALELISTEDIT` có `?`, Add, Delete, Reset, Exit.
  2. Reset cho đúng 20 tỉ lệ của UI-D11 theo thứ tự; tên tỉ lệ duy nhất; tỉ lệ ≤ 0 bị từ chối.
  3. Xóa tỉ lệ đang dùng (viewport, đối tượng annotative, CANNOSCALE) bị chặn, báo số nơi dùng.
  4. Round-trip 100% (QA-003); một mark undo; bàn phím, `.scr`, `(command)` giống từng bit.
  5. Đạt DoD-C.

### CMD-429 · Tỉ lệ viewport và khóa viewport: VPLOCK
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-428, DOC-011, DOC-017, INT-003, INT-005, INT-007, INT-016, RGN-024, LSP-006, QA-012
- **Lệnh:** VPLOCK
- **Đầu vào:** 03 §13 (đặt tỉ lệ, khóa); 07.1 §9.12, §10 (Tỉ lệ viewport, Khóa viewport).
- **Đầu ra:** `commands/layout/ViewportScale.{h,cpp}`: đặt tỉ lệ chuẩn/tùy chỉnh giữ tâm view, tỉ lệ chú thích của viewport theo tỉ lệ viewport; khóa/mở; viewport khóa thì zoom/pan bên trong chuyển sang paper (tùy chọn, qua INT-016); dữ liệu cho dropdown thanh trạng thái và Properties. VPLOCK: ON/OFF → chọn viewport **[đề xuất]**; `docs/specs/commands/VPLOCK.md`.
- **Tiêu chí nghiệm thu:**
  1. Sau đặt tỉ lệ 1:n: chiều cao view model = chiều cao viewport × n, sai lệch tương đối ≤ 1e-12; tâm view giữ nguyên.
  2. Viewport khóa: ZOOM, PAN, đổi tỉ lệ bên trong không đổi view (hash view giữ nguyên); mở khóa thì đổi được.
  3. Dropdown dùng danh sách CMD-428, hiện "Tùy chỉnh" khi tỉ lệ không thuộc danh sách.
  4. Đổi tỉ lệ đến khung đầu ≤ 100 ms p99 trên cảnh nhiều viewport QA-012; một mark undo; bàn phím, `.scr`, `(command)` giống từng bit.
  5. Đạt DoD-C, DoD-L.

### CMD-430 · MVIEW: viewport chữ nhật và tùy chọn P0
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-426, CMD-429, CMD:MSPACE, DOC-011, RGN-024, RND-014, INT-003, INT-004, INT-007, INT-012, LSP-006, QA-004, QA-007
- **Lệnh:** MVIEW
- **Màn hình:** UI-H11
- **Đầu vào:** 07.2 UI-E10 (dòng `MVIEW`), UI-H11 (tỉ lệ đặt sẵn khi tạo); 03 §13.
- **Đầu ra:** `commands/layout/MviewCmd.cpp`; `docs/specs/commands/MVIEW.md`; ca vàng `.scr`.
- **Tiêu chí nghiệm thu:**
  1. Đủ tùy chọn: góc thứ nhất → góc đối, Fit, ON/OFF, Shadeplot, Lock, NEw/NAmed (móc UI-D14), Restore, LAyer (đặt lại ghi đè layer theo viewport), 2/3/4 (Horizontal, Vertical, Above, Below, Left, Right, Fit); Object, Polygonal ở CMD-433.
  2. Viewport mới nằm trên layer hiện hành, hiện Extents của model hoặc theo tỉ lệ đặt sẵn (CMD-429); 2/3/4 chia đúng vùng chỉ định ± `Tolerance`.
  3. Gọi trong tab Model hoặc khi đang ở MSPACE: hành vi như AutoCAD (đối chiếu QA-007); tệp mở trong DWG TrueView không hỏi recover (QA-004).
  4. Khung viewport tạm ≤ 8 ms/khung p99; một mark undo; bàn phím, `.scr`, `(command)` giống từng bit.
  5. Đạt DoD-C, DoD-L.

### CMD-431 · Grip viewport: góc, tỉ lệ, di chuyển
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** CMD-429, CMD-430, INT-013
- **Màn hình:** UI-H11
- **Đầu vào:** 07.1 §9.5 (dòng Viewport), 07.2 UI-H11; API grip INT-013.
- **Đầu ra:** `commands/layout/ViewportGrips.{h,cpp}`: 4 grip góc đổi kích thước, grip tỉ lệ (mở danh sách tỉ lệ CMD-428), grip di chuyển; điểm mở để CMD-433 thêm grip đỉnh biên clip.
- **Tiêu chí nghiệm thu:**
  1. Đổi kích thước giữ tỉ lệ và vị trí trên giấy của nội dung model (một điểm model bất kỳ giữ tọa độ giấy ± `Tolerance`).
  2. Grip tỉ lệ cho kết quả giống từng bit đặt tỉ lệ qua CMD-429; viewport khóa vẫn đổi kích thước được, view không đổi.
  3. Hit test grip ≤ 1 ms; kéo ≤ 8 ms/khung p99; thả grip là một mark undo.
  4. Đạt DoD-C, DoD-L.

### CMD-432 · VPORTS, -VPORTS: cấu hình viewport
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** CMD-430, DOC-004, INT-003, INT-007, INT-016, RND-014, LSP-006, QA-004, QA-007
- **Lệnh:** VPORTS, -VPORTS
- **Màn hình:** UI-D14
- **Đầu vào:** 07.2 UI-D14; 03 §13.
- **Đầu ra:** `commands/layout/VportsCmd.cpp`: ô viewport ở Model (bảng VPORT `*Active`), cấu hình có tên; trong layout tạo viewport theo cấu hình qua CMD-430; móc mở UI-D14; `docs/specs/commands/VPORTS.md`, `-VPORTS.md`.
- **Tiêu chí nghiệm thu:**
  1. `-VPORTS` đủ: Save, Restore, Delete, Join, SIngle, `?`, 2, 3, 4 (kèm Horizontal, Vertical, Above, Below, Left, Right), Toggle, MOde; áp cho cả vùng vẽ hoặc ô hiện hành.
  2. Các ô phủ kín vùng vẽ, không chồng (tổng diện tích = vùng ± `Tolerance`); Join chỉ nhận hai ô kề chung một cạnh.
  3. Cấu hình có tên round-trip; DWG TrueView mở đúng số ô (QA-004).
  4. Đổi cấu hình đến khung đầu ≤ 100 ms p99; một mark undo; bàn phím, `.scr`, `(command)` giống từng bit.
  5. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** vẽ nhiều ô trong vùng vẽ (RND-014, UIA-008).

### CMD-433 · VPCLIP và viewport đa giác, theo đối tượng (MVIEW Polygonal, Object)
- **Mốc:** EA · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-430, CMD-431, GEO-005, GEO-006, RGN-024, RND-014, INT-003, INT-007, INT-008, LSP-006, QA-005, QA-007
- **Lệnh:** VPCLIP
- **Đầu vào:** 03 §13 (P1); 07.2 UI-E10 (`MVIEW` Polygonal, Object), UI-H11 (Đa giác, Theo đối tượng, Clip).
- **Đầu ra:** `commands/layout/ViewportClip.{h,cpp}`: biên clip đa giác (đoạn và cung như PLINE) hoặc đối tượng kín (polyline, tròn, ellipse, region, spline kín); tùy chọn Polygonal, Object của MVIEW; VPCLIP (chọn viewport → đối tượng clip, Polygonal, Delete); grip đỉnh biên; prompt biên clip dùng chung với XCLIP; `docs/specs/commands/VPCLIP.md`.
- **Tiêu chí nghiệm thu:**
  1. Hiển thị và bản in viewport clip khớp DWG TrueView: SSIM ≥ 0,98 (QA-005) trên 30 tệp mẫu; biên tự cắt hoặc không kín bị từ chối kèm thông báo.
  2. Delete trả về viewport chữ nhật theo hộp bao biên cũ; quan hệ xóa giữa viewport và đối tượng biên theo AutoCAD (đối chiếu QA-007).
  3. Handle biên clip của VIEWPORT round-trip 100%; một mark undo; bàn phím, `.scr`, `(command)` giống từng bit.
  4. Đạt DoD-C, DoD-L.

### CMD-434 · VPMAX, VPMIN
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** CMD-429, CMD:MSPACE, INT-007, INT-016, LSP-006
- **Lệnh:** VPMAX, VPMIN
- **Đầu vào:** 03 §13; 07.2 UI-H11 (Phóng to / Thu nhỏ viewport).
- **Đầu ra:** `commands/layout/VpMaxCmd.cpp`: phóng viewport đang kích hoạt hoặc được chọn ra đầy vùng vẽ ở model space (viewport đa giác hiện biên), chuyển sang viewport kế; VPMIN trả về layout; trạng thái chỉ trong phiên; `docs/specs/commands/VPMAX.md`, `VPMIN.md`.
- **Tiêu chí nghiệm thu:**
  1. VPMAX → sửa đối tượng, pan, zoom → VPMIN: view của viewport (tâm, chiều cao, twist) giống từng bit trước VPMAX; sửa đổi đối tượng được giữ.
  2. Trong tab Model hoặc không có viewport: báo không áp dụng; viewport khóa vẫn phóng được.
  3. Phóng hoặc thu đến khung đầu ≤ 100 ms p99; bàn phím, `.scr`, `(command)` cho cùng trạng thái.
  4. Đạt DoD-C, DoD-L.

### CMD-435 · Engine chiếu layout sang Model cho EXPORTLAYOUT
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** CMD-430, CMD-433, DOC-011, DOC-014, GEO-010, GEO-019, RGN-024, RGN-026, NT-005, QA-001, QA-005, QA-012
- **Đầu vào:** 03 §13 (EXPORTLAYOUT P1); prototype `screens11.js` (`IMPL.EXPORTLAYOUT`: thu nội dung theo tỉ lệ viewport, cắt theo khung, giữ đối tượng Paper).
- **Đầu ra:** `commands/layout/LayoutFlatten.{h,cpp}`: với mỗi viewport bật, biến đổi model → giấy (tỉ lệ, twist), cắt hình học theo biên chữ nhật hoặc đa giác (GEO-019), bỏ layer đóng băng trong viewport, chọn biểu diễn annotative theo tỉ lệ viewport, hệ số linetype theo PSLTSCALE; text, block cắt ngang biên giữ nguyên và ghi vào báo cáo **[đề xuất]**; song song theo viewport và theo khối entity (NT-005).
- **Tiêu chí nghiệm thu:**
  1. Hình học chiếu sai số ≤ 1e-9 × kích thước bản vẽ; không đối tượng nào vượt biên viewport trừ mục có trong báo cáo.
  2. Ảnh layout gốc và ảnh Model kết quả: SSIM ≥ 0,98 (QA-005) trên 50 layout QA-001.
  3. Cảnh BENCH-STD nhiều viewport 1 triệu entity ≤ 5 s trên 8 luồng **[đề xuất]**, hiệu suất ≥ 60%; tất định mọi số luồng.
  4. Hủy được trong ≤ 50 ms; bản vẽ nguồn không đổi (hash).
  5. Đạt DoD-C, DoD-H.

### CMD-436 · EXPORTLAYOUT
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-435, DOC-018, DWG-022, NT-011, NT-013, INT-003, INT-007, LSP-006, QA-004
- **Lệnh:** EXPORTLAYOUT
- **Màn hình:** UI-H11
- **Đầu vào:** 03 §13; 07.1 §9.12 (Xuất layout thành Model); prototype `screens11.js` (`IMPL.EXPORTLAYOUT`).
- **Đầu ra:** `commands/layout/ExportLayoutCmd.cpp`: chỉ chạy trong layout; tên mặc định `<bản vẽ>_<layout>.dwg` (hộp lưu khi FILEDIA = 1, dòng lệnh khi 0); ghi DWG qua tệp tạm rồi đổi tên; chạy nền có tiến trình, hủy được (NT-013); hỏi mở bản vẽ mới; `docs/specs/commands/EXPORTLAYOUT.md`.
- **Tiêu chí nghiệm thu:**
  1. Tệp kết quả mở trong DWG TrueView, ODA File Converter không lỗi, không hỏi recover (QA-004); bản vẽ gốc không đổi (hash) và không bị đánh dấu đã sửa.
  2. Tên trống hoặc có ký tự `\ / : * ? " < > |` bị từ chối; tệp đã có thì hỏi ghi đè.
  3. Hủy giữa chừng không để lại tệp; luồng UI không bị chặn quá 16 ms.
  4. `.scr` và `(command "EXPORTLAYOUT" …)` với FILEDIA = 0 cho tệp giống từng bit với bàn phím.
  5. Đạt DoD-C, DoD-L.

### CMD-437 · ALIGNSPACE, VPSYNC, VPSCALE (Express)
- **Mốc:** Sau · **Ưu tiên:** P2 · **Cỡ:** M · **Phụ thuộc:** CMD-429, CMD-430, INT-004, INT-007, INT-008, LSP-006
- **Lệnh:** ALIGNSPACE, VPSYNC, VPSCALE
- **Đầu vào:** Express Tools nhóm viewport (P2).
- **Đầu ra:** `commands/layout/ExpressViewportCmd.cpp`; `docs/specs/commands/ALIGNSPACE.md`, `VPSYNC.md`, `VPSCALE.md`.
- **Tiêu chí nghiệm thu:**
  1. ALIGNSPACE: hai điểm model, hai điểm paper → đặt tỉ lệ, twist, tâm view; sau lệnh hai điểm model trùng hai điểm paper ± `Tolerance`.
  2. VPSYNC: viewport chủ → các viewport khác cùng tỉ lệ, gióng theo phương ngang hoặc dọc ± `Tolerance`.
  3. VPSCALE in tỉ lệ viewport chọn (paper:model, kèm tên trong danh sách tỉ lệ nếu có).
  4. Bàn phím, `.scr`, `(command)` giống từng bit; một mark undo.
  5. Đạt DoD-C.

## 14. Tỉ lệ chú thích

### CMD-438 · CANNOSCALE, ANNOAUTOSCALE, ANNOALLVISIBLE
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-428, CMD-429, DOC-021, RGN-026, INT-005, INT-007, LSP-006, NT-005, QA-007
- **Lệnh:** CANNOSCALE, ANNOAUTOSCALE, ANNOALLVISIBLE
- **Đầu vào:** 03 §13; 07.1 §7.4 (Tỉ lệ chú thích), §10 (Hiển thị chú thích / Tự thêm tỉ lệ).
- **Đầu ra:** `commands/annot/scale/AnnoScale.{h,cpp}`: đặt CANNOSCALE (Model và viewport đang kích hoạt), tự thêm tỉ lệ theo ANNOAUTOSCALE, ẩn/hiện theo ANNOALLVISIBLE (theo viewport); `docs/specs/commands/CANNOSCALE.md`, `ANNOAUTOSCALE.md`, `ANNOALLVISIBLE.md`.
- **Tiêu chí nghiệm thu:**
  1. ANNOAUTOSCALE 1–4 và giá trị âm: ma trận mức × trạng thái layer (tắt, đóng băng, khóa, đóng băng theo viewport) cho đúng số đối tượng được thêm tỉ lệ, đối chiếu QA-007.
  2. CANNOSCALE không có trong danh sách tỉ lệ bị từ chối; đổi CANNOSCALE kèm tự thêm tỉ lệ là một mark undo.
  3. Đổi tỉ lệ với 100.000 đối tượng annotative ≤ 300 ms trên 8 luồng **[đề xuất]**; tất định.
  4. `.scr`, `(setvar …)` và `(command …)` cho cùng DB.
  5. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** hiển thị theo tỉ lệ hiện hành (RGN-026, có từ V).

### CMD-439 · Kích thước annotative
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-401, CMD-403, CMD-411, CMD-438, DOC-021, RGN-026, QA-003, QA-007
- **Đầu vào:** 03 §8 (Kích thước annotative, P1); 07.2 UI-D02 (Mới: chú thích theo tỉ lệ; tab Fit: tỉ lệ chú thích).
- **Đầu ra:** `commands/annot/dim/DimAnnotative.{h,cpp}`: dim theo kiểu annotative nhận tỉ lệ CANNOSCALE, mỗi tỉ lệ một biểu diễn (vị trí chữ, đường dim), bỏ qua DIMSCALE; hàm theo loại cho OBJECTSCALE, ANNORESET; cập nhật liên kết cho mọi biểu diễn.
- **Tiêu chí nghiệm thu:**
  1. Ở mọi tỉ lệ viewport của dim, chiều cao chữ trên giấy = DIMTXT ± `Tolerance`; mũi tên, khoảng hở theo cùng tỉ lệ.
  2. Bất biến liên kết của CMD-411 đúng cho từng biểu diễn sau MOVE, STRETCH, SCALE hình học.
  3. Dữ liệu ngữ cảnh tỉ lệ của dim round-trip 100% (QA-003); chữ khớp 100% QA-007 ở từng tỉ lệ.
  4. Đạt DoD-C.

### CMD-440 · OBJECTSCALE, -OBJECTSCALE, ANNORESET
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** CMD-438, CMD-439, DOC-021, RGN-026, INT-003, INT-007, INT-008, LSP-006, QA-003
- **Lệnh:** OBJECTSCALE, -OBJECTSCALE, ANNORESET
- **Màn hình:** UI-F11
- **Đầu vào:** 07.2 UI-F11; 07.1 §7.4 (Thêm/xóa tỉ lệ cho đối tượng, Đồng bộ vị trí theo tỉ lệ).
- **Đầu ra:** `commands/annot/scale/ObjectScaleCmd.cpp`: thêm/xóa tỉ lệ cho đối tượng annotative (text, mtext, dim, leader, mleader, hatch, block, attribute) qua hàm theo loại; danh sách tỉ lệ chung cho bộ lọc UI-F11; ANNORESET; `docs/specs/commands/OBJECTSCALE.md`, `-OBJECTSCALE.md`, `ANNORESET.md`.
- **Tiêu chí nghiệm thu:**
  1. `-OBJECTSCALE`: Add, Delete, `?`; không xóa tỉ lệ cuối cùng của đối tượng; đối tượng không annotative bị bỏ qua, báo số lượng; PICKFIRST.
  2. ANNORESET đưa vị trí mọi biểu diễn khác về vị trí của biểu diễn hiện hành ± `Tolerance`.
  3. Thêm một tỉ lệ cho 10.000 đối tượng ≤ 200 ms trên 8 luồng **[đề xuất]**; round-trip 100% (QA-003).
  4. Một mark undo; bàn phím, `.scr`, `(command)` giống từng bit.
  5. Đạt DoD-C, DoD-H.

## Tóm tắt theo mốc

| Mốc | Task | Số task |
|---|---|---|
| V | CMD-301, CMD-302, CMD-303 | 3 |
| EA | CMD-304, CMD-305, CMD-306, CMD-307, CMD-308, CMD-309, CMD-310, CMD-311, CMD-312, CMD-313, CMD-314, CMD-315, CMD-326, CMD-327, CMD-328, CMD-332, CMD-333, CMD-401, CMD-402, CMD-403, CMD-404, CMD-405, CMD-406, CMD-407, CMD-408, CMD-409, CMD-410, CMD-411, CMD-412, CMD-426, CMD-428, CMD-429, CMD-430, CMD-431, CMD-432, CMD-433 | 36 |
| 1.0 | CMD-316, CMD-317, CMD-318, CMD-319, CMD-320, CMD-321, CMD-322, CMD-329, CMD-330, CMD-331, CMD-334, CMD-335, CMD-336, CMD-337, CMD-338, CMD-339, CMD-340, CMD-341, CMD-413, CMD-414, CMD-415, CMD-416, CMD-417, CMD-418, CMD-419, CMD-420, CMD-421, CMD-422, CMD-434, CMD-435, CMD-436, CMD-438, CMD-439, CMD-440 | 34 |
| Sau | CMD-323, CMD-324, CMD-325, CMD-342, CMD-423, CMD-424, CMD-425, CMD-437 | 8 |
| **Tổng** | | **81** |
