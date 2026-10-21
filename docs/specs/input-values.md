# Giá trị nhập: số thực, số nguyên, khoảng cách, góc (INT-105)

Thư viện `cmd/parse/ValueParser` (`trion_parse`, namespace `trion::cmd`) là parser chuỗi → số duy nhất (README task
§4.2, core-int §ranh giới): prompt dòng lệnh, ô số UI-C09 (UI0-109), tọa độ (INT-004), `'CAL`/QUICKCALC (INT-022),
`distof`/`angtof` (LSP-011). Chiều số → chuỗi là RGN-013 (`docs/specs/regen/number-format.md`); mọi chuỗi RGN-013
sinh ra (kể cả UNITMODE 0/1, mọi DIMZIN/DIMAZIN) đọc lại được, lệch ≤ nửa đơn vị hiển thị.

Hành vi chia hai loại như RGN-013: **chắc** (suy từ tài liệu AutoCAD/AutoLISP hoặc từ yêu cầu 07.1) và **cần đối
chiếu** (suy luận, cờ `V` trong `cmd/parse/tests/golden.tsv`, danh sách §6). Khi có bộ tham chiếu QA-110: chạy lại
các ca `V`, sửa ca lệch.

## 1. API

```cpp
#include "cmd/parse/ValueParser.h"
using namespace trion::cmd;
trion::regen::UnitContext c;                 // cùng struct với RGN-013; parser chỉ đọc lunits, aunits, angbase, angdir, useAngbase
Parsed<double> d = parseDistance("1'-6 1/2\"", c);   // d.value, d.error (ParseError), d.end, bool(d)
Parsed<double> a = parseAngle("N45d30'E", c);        // radian
Parsed<double> r = parseReal("1.5E+01");
Parsed<int> i = parseInt("-32768");
Parsed<double> p = parseDistance("100,200", c, ParseMode::Prefix);   // p.value = 100, p.end = 3
std::string_view code = errorCode(d.error);            // "comma", "syntax"… cho trion::Error::code
```

- `ParseMode::Whole` (mặc định): cả chuỗi là một giá trị, bỏ ' ' và '\t' hai đầu; thành công thì `end` = độ dài chuỗi.
- `ParseMode::Prefix`: đọc giá trị dài nhất ở đầu chuỗi (bỏ khoảng trắng đầu), dừng trước ký tự không thuộc giá trị
  (',', '<', toán tử, chữ đơn vị…); `end` = số byte đã đọc (kể cả khoảng trắng đầu). Cho bộ tách từ của biểu thức
  (UI0-109, `'CAL`) và tọa độ (INT-004). Không trả `CommaAsDecimal`. Bất biến (fuzz kiểm): Prefix đọc n byte thì
  Whole của n byte đầu cho cùng giá trị từng bit.
- Lỗi: `value` = 0, `end` = 0. Không cấp phát, không locale, không trạng thái toàn cục, `noexcept`, an toàn đa luồng.

## 2. Quy tắc chung

- **Dấu thập phân luôn là `.`**, không nhóm nghìn. Ở chế độ Whole, chuỗi có `,` kề chữ số (`1,5`, `,5`, `1,000.5`,
  `1'6,5"`) trả `CommaAsDecimal` (mã `comma`) trước mọi phân tích khác, để giao diện nhắc một lần (07.1 §8). `,` không
  kề chữ số (`a,b`) là `Syntax`.
- **Số** (dùng chung mọi hàm): `[+|-]` rồi `chữ_số+ [. chữ_số*] | . chữ_số+`, rồi mũ `e|E [+|-] chữ_số+` (`12`,
  `12.5`, `.5`, `5.`, `1.7500E+01`, `1E+01`). Không nhận `inf`, `nan`, hex, khoảng trắng giữa dấu và số, NBSP, chữ
  số toàn khổ. Chữ cái mũ không có chữ số theo sau không thuộc số (`1e` ở Prefix là `1`, đọc 1 byte).
- **Phạm vi:** bậc thập phân của chữ số có nghĩa đầu trong −307…308 (≈ 1e-307 ≤ |x| < 1.8e308), ngoài ra
  `OutOfRange` (cả `1e-308`: không nhận số dưới chuẩn, vì thư viện chuẩn báo tràn dưới mỗi nơi một kiểu); 0 viết kiểu
  nào cũng được (`0e999999`). Kết quả cuối cùng theo cùng phạm vi: không hữu hạn (feet·12 tràn, ANGBASE không hữu
  hạn) hoặc khác 0 mà < 1e-307 (phân số `6/6…6` 308 chữ số, `1e-307` độ đổi ra radian) cũng là `OutOfRange`, nên
  mọi kết quả định dạng lại (RGN-013) rồi đọc lại được (lỗi fuzz tìm ra, đã thêm ca vàng).
- **Đổi thập phân → double:** gần nhất, hòa về chẵn (như IEEE, `strtod`, `from_chars`), tự viết: ≤ 16 chữ số và
  |mũ| ≤ 22 là một phép nhân/chia chính xác (Clinger); còn lại số nguyên lớn chính xác (D·5^e hoặc ⌊D·2^s/5^k⌋ kèm cờ
  dư) rồi làm tròn một lần. Chữ số thứ 801 trở đi gộp thành một chữ số "dính" (điểm giữa hai double có ≤ 767 chữ số
  có nghĩa nên không đổi kết quả). Không dùng `std::from_chars` cho số thực: DLL libc++ đi kèm Qt cho Windows (LLVM
  17) thiếu ký hiệu này, tệp chạy dựng bằng llvm-mingw 20 không khởi động được (`STATUS_ENTRYPOINT_NOT_FOUND`);
  trên Apple, header libc++ 20 còn đánh dấu hàm này `unavailable`.

## 3. `parseReal`, `parseInt`

| Hàm | Nhận | Lỗi riêng |
|---|---|---|
| `parseReal` (getreal) | số §2 | phân số, feet-inch là `Syntax` (getreal không nhận, dùng getdist) **[V]** |
| `parseInt` (getint) | `[+|-] chữ_số+`, −32768…32767 (`007`, `-0`, `+7`) | `1.5`, `2.0`, `2.`, `.5`, `1e3` là `NotInteger`; ngoài phạm vi `OutOfRange` (`1e999` cũng `OutOfRange`) |

## 4. `parseDistance` (getdist, distof)

Đơn vị bản vẽ; với feet-inch 1 đơn vị = 1 inch. Chỉ dùng `ctx.lunits`; UNITMODE không cần: **nhận cả hai dạng**.

| LUNITS | Nhận |
|---|---|
| 1 khoa học, 2 thập phân, 5 phân số (6 và ngoài 1–6: như 2) | số §2; phân số `tử/mẫu`; hỗn số `nguyên-tử/mẫu` hoặc `nguyên tử/mẫu` (một hay nhiều khoảng trắng) **[V cho 1, 2]** |
| 3 kỹ thuật, 4 kiến trúc (distof coi 3 và 4 như nhau) | như trên, thêm feet-inch |

- Phân số: tử, mẫu chỉ gồm chữ số (`1/2`, `255/256`, `1/3`, `5/2`, `001/002`); mẫu 0 là `ZeroDenominator`; `1.5/2`,
  `1/2.5`, `1/2/3`, `1 /2`, `1/ 2`, `1 - 1/2` là `Syntax`. Dấu âm chỉ ở đầu: `-17 1/2` = −17.5.
- Feet-inch: `[+|-] độ_lớn ' [khoảng trắng] [-] [khoảng trắng] [độ_lớn ["]]` hoặc `độ_lớn ["]` (inch), với `độ_lớn` là
  số, phân số hoặc hỗn số. Giá trị = feet·12 + inch.
  - Dạng rtos UNITMODE 0: `1'-5 1/2"`, `1'-5.50"`, `0'-6"`, `2'-0"`, `1'-0 1/2"`, `-1'-5 1/2"`.
  - Dạng nhập UNITMODE 1: `1'5-1/2"`, `1'5.50"`, `1'0-1/2"`, `6-1/2"`.
  - Thêm: dấu `"` tùy chọn (`1'6`, `6`), `1' 6"`, `1'- 6"`, `1'-5-1/2"`, `1'1/2"`, feet lẻ `1.5'`, `1 1/2'`,
    `1/2'`, khoa học `1E1'`, inch ≥ 12 sau feet `1'14"` = 26 **[V]**.
  - `Syntax`: `1'-` (Whole), `1''`, `'6`, `1'6'`, `6"1'`, `1'6""`, `1 '`, `1'--6"`, `1'-6.5 1/2"`, ký hiệu ′ ″.
  - Dấu `'` hoặc `"` khi LUNITS không phải 3, 4: `FeetInchNotAllowed` (mã `feet-inch`) **[V]**.

## 5. `parseAngle` (getangle/getorient, angtof)

Trả radian. Số không hậu tố theo `ctx.aunits`: độ với 0 (độ thập phân), 1 (độ/phút/giây), 4 (khảo sát) và ngoài 0–4;
grad với 2; radian với 3. Có hậu tố thì theo hậu tố, **ở mọi AUNITS** **[V]**:

| Dạng | Ví dụ | Ghi chú |
|---|---|---|
| độ/phút/giây | `45d`, `45d30'`, `45d30'15"`, `45d30'15.5"`, `45°30'`, `-45d30'` | `d`, `D` hoặc `°` (UTF-8, ngoài AutoCAD, cho ô số UI); dấu `'` sau phút và `"` sau giây tùy chọn (`45d30`, `45d30'15`); chỉ thành phần cuối có phần lẻ (`45.5d30'` là `Syntax`); phút, giây ≥ 60 là `OutOfRange` **[V]**; không khoảng trắng bên trong |
| grad | `50g`, `50G`, `-50g` | |
| radian | `0.7854r`, `1R` | |
| khảo sát | `N45d30'E`, `N 45d30' E`, `S30dW`, `N45E`, `N45.5E`, `n45de`; một chữ `N`, `E`, `S`, `W` | phương vị là độ thập phân hoặc d/'/" (không mũ, không g/r), ≤ 90° (`N91dE` `OutOfRange`); không dấu |

- Khảo sát: N b E = 90° − b, N b W = 90° + b, S b E = 270° + b, S b W = 270° − b (E = 0, N = 90°, W = 180°, S = 270°);
  luôn tuyệt đối, không theo ANGBASE/ANGDIR (như RGN-013 định dạng).
- **ANGBASE/ANGDIR** (`ctx.useAngbase`): chuỗi là hướng gõ theo hệ người dùng (prompt điểm `@d<a`, hướng) → góc tuyệt
  đối `angbase + t` (ANGDIR 0) hoặc `angbase − t` (ANGDIR 1). Ví dụ bảng getorient của tài liệu AutoLISP (ANGBASE 90°,
  ANGDIR 1): `0` → 90°, `-90` → 180°, `180` → −90° (≡ 270°), `90` → 0. `useAngbase` = false: đổi đơn vị thuần
  (`angtof`, như `angtos` không dùng ANGBASE).
- **Không quy về [0, 2π)**: giữ dấu và độ lớn (`720` → 4π; góc chắn cung, góc quay âm có nghĩa). Người gọi tự quy:
  `angtof` (`(angtof "45.0000" 3)` = 45 rad → 1.0177), getorient. getangle (ANGDIR có, ANGBASE không) = kết quả
  `useAngbase` − angbase.

## 6. Mã lỗi

| `ParseError` | `errorCode` | Khi |
|---|---|---|
| `Empty` | `empty` | rỗng hoặc chỉ khoảng trắng |
| `Syntax` | `syntax` | sai cú pháp, thừa ký tự |
| `CommaAsDecimal` | `comma` | Whole: `,` kề chữ số |
| `OutOfRange` | `range` | §2 phạm vi; int ngoài −32768…32767; phút/giây ≥ 60; phương vị > 90° |
| `NotInteger` | `not-integer` | parseInt nhận số thực không nguyên |
| `FeetInchNotAllowed` | `feet-inch` | `'`/`"` khi LUNITS ≠ 3, 4 |
| `ZeroDenominator` | `zero-denominator` | mẫu số 0 |

Thứ tự: `Empty`, rồi `CommaAsDecimal`, rồi lỗi đầu tiên gặp khi đọc (`OutOfRange`, `ZeroDenominator`, `NotInteger`,
`FeetInchNotAllowed`), cuối cùng `Syntax`.

## 7. Cần đối chiếu AutoCAD (QA-110) và chỗ cố ý khác

Cần đối chiếu (38 ca cờ `V`, cùng các quyết định dưới):

1. Phân số, hỗn số khi LUNITS 1, 2 (`1/2`, `17 1/2` ở prompt khoảng cách và `distof "1/2" 2`).
2. Feet-inch khi LUNITS 1, 2, 5: lỗi (`FeetInchNotAllowed`) hay AutoCAD vẫn nhận; `distof` trả nil?
3. Inch ≥ 12 sau feet (`1'14"`), feet lẻ (`1.5'`), khoa học trong feet-inch (`1E1'`), khoảng trắng sau `'` (hộp thoại).
4. `getreal` có nhận phân số không (ở đây không).
5. `getint` với `2.0`, `1e3` (ở đây `NotInteger`).
6. Hậu tố góc ở mọi AUNITS (`50g` khi AUNITS 0, `N45dE` khi AUNITS 0, `45d` khi AUNITS 3); `N45E` không có `d`;
   phút/giây ≥ 60, phần lẻ ở thành phần không cuối; dấu `'`/`"` tùy chọn ở cuối.
7. Phương vị khi ANGBASE ≠ 0 ở prompt hướng: tuyệt đối (ở đây) hay tương đối ANGBASE.
8. `angtof` có quy về [0, 2π) mọi đơn vị không (ví dụ tài liệu cho radian: có).

Cố ý khác AutoCAD: `,` kề chữ số luôn là lỗi `comma` ở Whole (AutoCAD coi `1,5` ở prompt khoảng cách là điểm — tầng
prompt INT-106 xét điểm trước khi gọi parser); nhận `°`; |x| < 1e-307 khác 0 là lỗi thay vì 0 hoặc số dưới chuẩn.

## 8. Dùng cho task sau

- **UI0-109 (ô số):** `parseDistance`/`parseAngle`/`parseReal`/`parseInt` với `UnitContext` của bản vẽ (cùng struct
  định dạng lại bằng RGN-013); biểu thức `=1200/2+50`, `3'6"+2`: tách từ bằng `ParseMode::Prefix`, hậu tố đơn vị
  (`m`, `cm`) đọc sau `end`; lỗi `comma` → câu gợi ý "12.5". Lưu ý: Prefix đọc `5-1/2`, `5 1/2` là hỗn số 5.5, `1/2`
  là phân số (trùng phép chia).
- **INT-004 (tọa độ):** tách `,` `<` `@` `#` `*` rồi gọi Whole, hoặc Prefix rồi xét ký tự tại `end`; góc sau `<` gọi
  `parseAngle` với `useAngbase = true` (kết quả tuyệt đối).
- **LSP-011:** `distof s mode` = `parseDistance(s, {lunits = mode})`, nil khi lỗi; `angtof s units` =
  `parseAngle(s, {aunits = units})` rồi `fmod` về [0, 2π); getangle/getorient như §5.

## 9. Kiểm thử, số đo (`cmd/parse/tests/value_parser_test.cpp`, nhãn `cmd`)

- 930 ca vàng `golden.tsv` (hợp lệ, sai, biên; giá trị kỳ vọng tính bằng phân số chính xác/Python, không lấy từ
  parser): real, int so từng bit; gồm điểm giữa chính xác hai double kề tới 772 chữ số và > 800 chữ số (chữ số dính).
- Chuỗi thập phân ngẫu nhiên (1–40 chữ số, mũ ±330) so từng bit với `strtod` ở locale "C": release 300 nghìn, dev
  100 nghìn, ASan/TSan 30 nghìn.
- Kiểm vòng `parse(format(x))` (RGN-013) mỗi giá trị × LUNITS 1–5 × LUPREC 0–8 và × AUNITS 0–4 × AUPREC 0–8 (90 cặp),
  DIMZIN, DIMAZIN, UNITMODE, ANGBASE/ANGDIR đổi theo giá trị, song song qua `trion::sched`: release 1 triệu giá trị
  (4,6 s khi máy tải vừa, 14–16 s khi load ≈ 27), dev 200 nghìn, ASan/TSan 20 nghìn. Lệch ≤ nửa đơn vị + 4 ulp (độ
  dài), ≤ nửa đơn vị + 1e-12 rad (góc). ctest TIMEOUT 120 s cho máy dùng chung quá tải.
- Locale: kết quả (hash từng bit) không đổi dưới vi_VN, de_DE (và `,` thật là dấu thập phân của locale); Docker
  Linux không có hai locale này nên bỏ qua, bản Windows kiểm thật. Tất định: hash kết quả tập chuỗi sinh tất định
  `0xF713FE2EE66CDBFF` trùng nhau ở Linux (libstdc++) và Windows (libc++, UCRT).
- Không cấp phát (đếm `operator new`, preset dev, release, win). Fuzz `cmd/fuzz/value_parser_fuzz.cpp` (preset fuzz),
  hạt giống trong `cmd/fuzz/corpus` (gồm `regress-tiny-fraction`).
- Hiệu năng (release, i7-1255U 2 nhân P + 8 nhân E dùng chung với phiên khác, đo theo lô 16 chuỗi, mỗi loại lấy vòng
  tốt nhất/100, các loại đo xen kẽ): load ≈ 20: p50 59 ns, p99 148 ns cho mọi loại chuỗi ≤ 32 ký tự; loại chậm nhất
  là số 30 chữ số có nghĩa (đường số nguyên lớn) p99 166 ns, kiến trúc p8 131 ns, khảo sát 116 ns, số nguyên 52 ns.
  Load ≈ 24–27: p99 179–330 ns tùy luồng đo rơi vào nhân P hay E. Ngưỡng p99 ≤ 300 ns chỉ chặn khi
  `trion::test::kTimed` (release không sanitizer); máy quá tải có thể làm hỏng giả.
