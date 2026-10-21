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

## 10. Cú pháp ô số (UI0-109)

`app/widgets/numeric/ValueParser.h` (`trion_numeric`, namespace `trion::ui`): bộ phân tích của ô số UI-C09, dùng
cho ô số UI0-007 và bảng Properties. Hàm thuần, không widget; mọi số đọc qua INT-105 (§1–§6), hiển thị lại qua
RGN-013. `parseValue`, `formatValue`, `errorCode`: không cấp phát, không locale, `noexcept`, an toàn đa luồng.

```cpp
#include "app/widgets/numeric/ValueParser.h"
using namespace trion::ui;
ValueSpec spec{ValueKind::Distance, units, insunits};       // units: UnitContext của bản vẽ; insunits: INSUNITS 0–24
ValueResult r = parseValue("=1200/2+50", spec);             // r.value (đơn vị bản vẽ; góc: radian), r.error, bool(r)
char b[trion::regen::kMaxNumText];
std::string_view shown(b, formatValue(r.value, spec, b));   // chuỗi hiện lại trong ô (RGN-013)
QString why = errorText(r.error, text, spec);               // câu dưới ô (tr), rỗng khi không lỗi
std::string_view code = errorCode(r.error);                 // "comma", "paren"… cho log, trion::Error::code
```

### 10.1 Loại ô và dạng nhận (không `=`)

Bỏ ' ' và '\t' hai đầu. Cả chuỗi là một giá trị:

| `ValueKind` | Nhận | Kết quả |
|---|---|---|
| `Distance` | `parseDistance` với feet-inch ở **mọi** LUNITS (`1250`, `1/2`, `5-1/2`, `17 1/2`, `3'6"`, `1'-6 1/2"`, `6"`, `1/2'`), rồi [khoảng trắng] hậu tố đơn vị §10.2 khi không có `'` `"` (`12.5m`, `250 cm`, `1/2in`, `5 1/2m`) | đơn vị bản vẽ |
| `Angle` | `parseAngle` với `spec.units` (cả `useAngbase`): `45`, `45d30'15"`, `45°30'`, `50g`, `0.785r`, `N45dE` | radian |
| `Scale` | số, phân số, hỗn số (`parseDistance` thập phân), hoặc tỉ số `a:b` (`1:50` = 0.02; `1:0` là `zero-denominator`) | số |
| `Integer` | `parseInt`: −32768…32767 | số |
| `Real` | `parseReal` | số |

Chuỗi không `=` mà INT-105 đọc được (cùng `UnitContext`) cho đúng double đó từng bit; phần mở rộng của ô số là hậu tố
đơn vị, feet-inch ở LUNITS 1, 2, 5 (INT-105: `feet-inch`) và tỉ số. Kết quả theo cùng phạm vi §2 (hữu hạn, 0 hoặc
|x| ≥ 1e-307), kể cả sau quy đổi đơn vị và phép tính, nên mọi kết quả hiển thị lại rồi đọc lại được.

### 10.2 Đơn vị (ô độ dài)

- Hậu tố `mm`, `cm`, `m`, `km`, `in`, `ft` (không phân biệt hoa thường, cho phép khoảng trắng trước), `'` feet, `"`
  inch. Số không hậu tố theo đơn vị bản vẽ.
- Đơn vị bản vẽ = `spec.insunits` (mã INSUNITS 1–24); LUNITS 3, 4 luôn là inch như RGN-013 (bỏ qua INSUNITS).
  INSUNITS 0 hoặc ngoài 0–24 (LUNITS khác 3, 4): bản vẽ không đơn vị, gõ kèm đơn vị là lỗi `units`.
- Quy đổi qua tỉ số mét/đơn vị nguyên của từng mã (inch = 127/5000 m, foot khảo sát = 1200/3937 m…), một phép nhân
  và một phép chia: `42"` → 1066.8 mm là double gần nhất.
- Không xét thứ nguyên: mọi số quy về đơn vị bản vẽ rồi mới tính (`=1m*1m` ở bản vẽ mm là 1000000).

### 10.3 Biểu thức (chuỗi bắt đầu bằng `=`)

```
biểu_thức := tích ( ('+' | '-' | '−') tích )*
tích      := một_ngôi ( ('*' | '×' | '/' | '÷') một_ngôi )*
một_ngôi  := ('+' | '-' | '−')* ( '(' biểu_thức ')' | toán_hạng )
```

- Ưu tiên: dấu một ngôi > nhân, chia > cộng, trừ; cùng bậc tính từ trái sang (`=12/2/3` = 2). Ngoặc lồng tối đa 32
  tầng. Khoảng trắng (' ', '\t') giữa các phần tùy ý; không nhân ngầm (`=2(3)`, `=(2)(3)`, `=1 2` là `syntax`).
  Toán tử Unicode − × ÷ (UTF-8) như - * /.
- Toán hạng ô độ dài: §10.4. Ô góc: số trần theo đơn vị của ô (độ khi AUNITS 0, 1, 4; grad 2; radian 3), có hậu tố
  `d` `°` `g` `r` thì đọc bằng `parseAngle` rồi đổi về đơn vị ô (`=180/4` là 45°, `=45d30'+0d30'`, `=100g/2`); phương vị
  (`N45dE`) không nhận. Kết quả đổi sang radian một lần ở cuối rồi mới áp ANGBASE/ANGDIR khi `useAngbase`. Ô tỉ lệ,
  số, số nguyên: số thực §2; ô số nguyên đòi kết quả nguyên (`not-integer`) trong −32768…32767 (`range`).
- Mỗi phép tính kiểm phạm vi §2: tràn, tích/thương khác 0 mà dưới 1e-307 là `range`; chia cho 0 là `zero-denominator`.
- Ngoài phạm vi: biến, hàm, vector của `'CAL` (INT-022).

### 10.4 Phân số và phép chia, phép trừ

Phân số `1/2`, hỗn số `5-1/2`, `5 1/2` của INT-105 trùng phép chia, phép trừ và hai số cạnh nhau. Quy tắc:

1. **Không `=`**: theo INT-105, `5-1/2` = 5.5, `1/2` = 0.5, `17 1/2` = 17.5.
2. **Có `=`, số trần** (không đơn vị) là số thực §2: `/` là phép chia, `-` là phép trừ; hai số cạnh nhau là lỗi
   (không đoán): `=5-1/2` = 4.5, `=1/2` = 0.5, `=5 1/2` `syntax`; viết `=5+1/2`.
3. **Có `=`, số có đơn vị** (`'`, `"`, hậu tố chữ) đọc như khi không `=`: đơn vị áp cho cả phân số, hỗn số, feet-inch
   viết liền trước nó. `=1/2"` là nửa inch, `=1/2'` = 6", `=3/8"+1/4"`, `=1/2m` = 500 mm, `=6 1/2"*2` = 13",
   `=10/2m` = 5 m. Khác gợi ý "trong biểu thức `/` luôn là phép chia" vì: (a) chuỗi RGN-013 hiển thị trong ô
   (`6 1/2"`, `6-1/2"` UNITMODE 1, `0'-6 1/2"`, `5'0-1/2"`) phải dùng được làm toán hạng khi người dùng thêm `=` và
   phép tính; (b) chia cho một độ dài không có nghĩa ở ô độ dài: cách đọc "1 chia 2 inch" luôn sai ý (`=1/2'` sẽ
   là 1/24", `=3/8"` ở bản vẽ mm là 0.0148); (c) cùng nghĩa với khi gõ không `=`.
4. **Feet-inch**: phần inch viết liền sau `'` thuộc cùng số (`=3'-6 1/2"`, `=2'-1"` = 25, `=5'-1/2"` = 60.5);
   khoảng trắng ngay sau `'` kết thúc số (`=2' - 1"` = 23, `=3' 6"` `syntax`); gặp `'` thứ hai thì là phép trừ
   (`=10'-2'` = 96). RGN-013 không bao giờ in khoảng trắng sau `'`.
5. Còn lại nhập nhằng (đã chọn, có ca vàng): LUNITS 5 UNITMODE 1 hiển thị `5-1/2` không đơn vị, đặt vào biểu thức là
   phép trừ (`=5-1/2*2` = 4; viết `=(5+1/2)*2` hoặc `=5-1/2"*2`); `=1/2/2m` = 1 chia 1 m; góc không có phân số nên
   `/` luôn là phép chia (`=1/2r` là 1 chia 2 rad).

| Chuỗi (ô độ dài, kiến trúc, bản vẽ inch) | Giá trị | Chuỗi (bản vẽ mm, LUNITS 2) | Giá trị |
|---|---|---|---|
| `5-1/2` | 5.5 | `5-1/2` | 5.5 |
| `=5-1/2` | 4.5 | `=5-1/2` | 4.5 |
| `=5 1/2` | `syntax` | `=5 1/2m` | 5500 |
| `=6-1/2"*2` | 13 | `=5-1/2m` | 5500 |
| `=6 - 1/2"` | 5.5 | `=5 - 1/2m` | −495 |
| `=1/2'+3"` | 9 | `=1/2"` | 12.7 |
| `=3'6-1/2"` | 42.5 | `=1/2m`, `=1/2 m` | 500 |
| `=2'-1"` / `=2' - 1"` | 25 / 23 | `=1200/2m` | 600000 |

### 10.5 Mã lỗi và câu giải thích

Thứ tự: `empty`, rồi `comma` (`,` kề chữ số ở bất kỳ đâu, cả trong biểu thức), rồi lỗi đầu tiên gặp khi đọc từ trái
sang. Lỗi: `value` = 0. Câu qua `tr()` (ngữ cảnh `ValueParser`), hiện dưới ô.

| `ValueError` | `errorCode` | Khi | Câu |
|---|---|---|---|
| `Empty` | `empty` | rỗng, chỉ khoảng trắng | Nhập một giá trị. |
| `Syntax` | `syntax` | sai cú pháp, thừa ký tự, hậu tố lạ, `=` rỗng | biểu thức: "Biểu thức chưa đúng. Biểu thức gồm số, đơn vị, + − × ÷ và ngoặc, ví dụ =1200/2+50."; còn lại theo loại ô: "Chưa đọc được độ dài. Ví dụ: 1250, 12.5m, 250cm, 3'6", =1200/2+50." (góc, tỉ lệ, số nguyên, số: ví dụ riêng) |
| `Comma` | `comma` | `,` kề chữ số | "Dấu thập phân là dấu chấm: gõ 12.5": chuỗi đã đổi các `,` kề chữ số thành `.`; đổi rồi vẫn sai (`1,000.5`) thì mẫu `12.5` |
| `OutOfRange` | `range` | §2, phép tính tràn, số nguyên ngoài −32768…32767, phút/giây ≥ 60, phương vị > 90° | "Số nguyên phải từ -32768 đến 32767." / "Góc vượt giới hạn: …" / "Giá trị quá lớn hoặc quá gần 0." |
| `NotInteger` | `not-integer` | ô số nguyên nhận số lẻ | Nhập số nguyên, ví dụ 12. |
| `DivideByZero` | `zero-denominator` | phân số mẫu 0, chia cho 0 (`=1/(2-2)`), tỉ số `x:0` | Không chia được cho 0. |
| `Paren` | `paren` | thiếu `)` ở cuối, thừa `)`, lồng quá 32 tầng | Ngoặc chưa khớp hoặc lồng quá 32 tầng. |
| `Units` | `units` | gõ kèm đơn vị khi bản vẽ không đơn vị | Bản vẽ chưa đặt đơn vị nên không quy đổi được đơn vị gõ kèm. Gõ số theo đơn vị bản vẽ. |

`*Khác nhau*` (nhiều đối tượng khác giá trị) là trạng thái của ô UI0-007, không phải chuỗi gõ: ô không gọi parser khi
chuỗi chưa đổi.

### 10.6 Định dạng lại

`formatValue`: `Distance` → `regen::formatLinear(value, spec.units)` (`1250.00`, `4'-1 1/2"`), `Angle` →
`formatAngle`, `Scale` và `Real` → thập phân theo LUPREC, `Integer` → thập phân LUPREC 0. Luôn dấu chấm, không locale;
bộ đệm `regen::kMaxNumText` luôn đủ. Chuỗi hiển thị đọc lại (không `=`) lệch ≤ nửa đơn vị hiển thị.

### 10.7 Kiểm thử (`app/widgets/numeric/tests/value_parser_test.cpp`, nhãn `numeric`)

- Ca vàng `tests/golden.tsv` (nhúng resource): mọi dạng UI-C09, kiến trúc, kỹ thuật, phân số, khoa học, góc độ/DMS/
  grad/radian/khảo sát, ANGBASE/ANGDIR, 25 mã INSUNITS, biểu thức, lỗi. Giá trị kỳ vọng tính bằng phân số chính xác
  (Python `fractions`), không lấy từ parser; cột hiển thị do RGN-013 tính trên giá trị kỳ vọng, so từng ký tự.
- Chuỗi không `=` mà INT-105 đọc được: so từng bit với INT-105. Hash FNV-1a (giá trị, mã lỗi, chuỗi hiển thị) mọi ca
  vàng trùng nhau ở Linux và Windows.
- Kiểm vòng hiển thị → đọc lại: 20 nghìn giá trị ngẫu nhiên tất định (ASan 2 nghìn) × loại ô × LUNITS/LUPREC/AUNITS/
  AUPREC/INSUNITS/ANGBASE. Ngoặc lồng 32/33 tầng, 1 triệu `(`, 1 triệu dấu một ngôi, tổng 200 nghìn ký tự.
- Không cấp phát (đếm `operator new`; preset dev, release, win). Ngưỡng ≤ 20 µs/giá trị chặn khi
  `trion::test::kTimed` (release).
- Fuzz `fuzz/value_parser_fuzz.cpp` (preset fuzz, hạt giống `fuzz/corpus`): ngoài ASan/UBSan kiểm kết quả hữu hạn
  trong phạm vi, gọi lại cùng kết quả, hiển thị rồi đọc lại, `=` + chuỗi cho cùng giá trị (ô số; ô độ dài khi có đơn
  vị hoặc không có phân số).
