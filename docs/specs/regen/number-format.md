# Định dạng số và đơn vị (RGN-013)

Thư viện `core/regen/numfmt` (`trion_regen`) là nơi duy nhất đổi số bản vẽ thành chuỗi (README task §4.2): text
kích thước (RGN-014), `rtos`/`angtos` (LSP-011), DYN, tọa độ thanh trạng thái, Properties, kết quả lệnh. Đọc chuỗi
thành số thuộc INT-105; ghép chuỗi kích thước (tiền tố, hậu tố, đơn vị phụ, dung sai, `DIMTFAC`) thuộc RGN-014.

Mục tiêu (05 §1.2): chuỗi trùng AutoCAD từng ký tự. Bộ tham chiếu QA-110 chưa có, nên hành vi dưới đây chia hai
loại (§6): **chắc** (suy trực tiếp từ ví dụ hoặc định nghĩa trong tài liệu AutoCAD/AutoLISP, cộng số học chính xác) và
**cần đối chiếu** (suy luận, ghi lý do). Khi có QA-110: chạy lại toàn bộ ca, sửa ca lệch, bỏ cờ `V` của ca đã khớp.

## 1. API

```cpp
#include "core/regen/numfmt/NumFormat.h"
trion::regen::UnitContext c;                 // struct thuần, người gọi điền (§2)
char buf[64];
std::size_t n = formatLinear(17.5, c, buf);  // như snprintf: trả độ dài đầy đủ, ghi phần vừa bộ đệm
std::size_t m = formatAngle(rad, c, buf);
std::string s = linearString(17.5, c);       // tiện dụng, có cấp phát; UI: QString::fromUtf8(s.data(), s.size())
```

- Chuỗi ra UTF-8, không `'\0'`. `kMaxNumText` (768) luôn đủ, kể cả ±1.8e308 với 16 số lẻ và dấu nhóm 4 byte.
- Không cấp phát, không trạng thái toàn cục, không locale, `noexcept`; gọi đồng thời từ nhiều luồng an toàn.
- NaN, vô cực ra `nan`, `inf`, `-inf` (ngoài phạm vi so khớp AutoCAD).
- Kiểm vòng của INT-105: với mọi kiểu đơn vị và độ chính xác, `parse(format(x))` lệch ≤ nửa đơn vị hiển thị
  (1/2·10^-p; kiến trúc, phân số 1/2^(p+1); độ/phút/giây nửa đơn vị nhỏ nhất ở §4).

## 2. Ngữ cảnh (`UnitContext.h`)

| Trường | Biến AutoCAD | Ghi chú |
|---|---|---|
| `lunits`, `luprec` | LUNITS, LUPREC (kích thước: DIMLUNIT, DIMDEC; đơn vị phụ DIMALTU, DIMALTD; dung sai DIMTDEC) | `luprec` kẹp 0–16 (thập phân, khoa học, kỹ thuật), 0–8 (kiến trúc, phân số) |
| `dimzin` | DIMZIN (rtos dùng DIMZIN; đơn vị phụ DIMALTZ; dung sai DIMTZIN) | bit 0–1 feet/inch, 4 bỏ 0 đầu, 8 bỏ 0 cuối |
| `dimrnd` | DIMRND (DIMALTRND) | ≤ 0, NaN, vô cực: bỏ qua |
| `dimdsep` | DIMDSEP | rtos, tọa độ để `'.'`; 0 coi như `'.'` |
| `dimfrac` | DIMFRAC | chỉ khi `form = Mtext` |
| `regionDecimal`, `regionGroup` | thiết lập vùng của hệ điều hành (DIMLUNIT 6) | người gọi đọc từ hệ thống; thư viện không đọc locale |
| `aunits`, `auprec` | AUNITS, AUPREC (kích thước: DIMAUNIT, DIMADEC) | `auprec` kẹp 0–8 |
| `dimazin` | DIMAZIN | `angtos`: `dimazin = (DIMZIN >> 2) & 3` |
| `angbase`, `angdir`, `useAngbase` | ANGBASE, ANGDIR | chỉ đổi khi `useAngbase` (góc hướng ở thanh trạng thái, DYN) |
| `unitmode` | UNITMODE | |
| `form` | — | `Plain` (rtos, angtos, tọa độ…) hoặc `Mtext` (nội dung MTEXT của kích thước) |

Giá trị mặc định của struct là mặc định AutoCAD bản imperial (DIMZIN 0); bản vẽ metric (acadiso) có DIMZIN 8.

## 3. Độ dài

Feet-inch: 1 đơn vị bản vẽ = 1 inch.

| LUNITS | Dạng (17.5, p = 2) | Độ chính xác |
|---|---|---|
| 1 khoa học | `1.7500E+01` (p = 4) | p số lẻ của định trị; mũ có dấu, ít nhất 2 chữ số (`E+100` khi cần) |
| 2 thập phân | `17.50` | p số lẻ; không nhóm nghìn |
| 3 kỹ thuật | `1'-5.50"` | inch có p số lẻ |
| 4 kiến trúc | `1'-5 1/2"` | phân số mẫu 2^p, tối giản |
| 5 phân số | `17 1/2` | như trên |
| 6 Windows (chỉ DIMLUNIT) | `1.234.567,89` (vùng vi: `,` `.`) | như thập phân, dấu theo vùng, nhóm 3 chữ số |

- **Làm tròn trước, tách sau:** tổng được làm tròn ở đơn vị nhỏ nhất rồi mới tách feet/inch, nên không có `12"`
  (23.999 kỹ thuật p2 → `2'`; 11.99 kiến trúc p4 → `1'`).
- **UNITMODE 1** (dạng nhập): `1'5.50"`, `1'5-1/2"`, `17-1/2` (không gạch giữa feet và inch, gạch giữa phần nguyên và
  phân số).
- **DIMZIN bit 0–1 (feet-inch, kỹ thuật và kiến trúc):** 0 bỏ 0 feet và inch đúng bằng 0 (`6"`, `2'`); 1 giữ cả hai
  (`0'-6"`, `2'-0"`); 2 giữ 0 feet, bỏ inch 0 (`0'-6"`, `2'`); 3 giữ inch 0, bỏ 0 feet (`6"`, `2'-0"`). Khi ẩn feet thì
  inch luôn hiện (0 → `0"`). Kiến trúc: inch nguyên 0 bị bỏ khi chỉ còn phân số và không hiện feet (`1/2"`), giữ khi
  có feet (`1'-0 1/2"`).
- **DIMZIN 4/8:** bỏ 0 đầu (`0.5000` → `.5000`), bỏ 0 cuối (`12.5000` → `12.5`, `12.0000` → `12`, bỏ cả dấu thập
  phân); áp cho thập phân, Windows, định trị khoa học và số inch của kỹ thuật; không áp cho kiến trúc, phân số. Chỉ còn
  `0` thì giữ `0` (`0.0000` với DIMZIN 12 → `0`).
- **DIMRND:** làm tròn tới bội gần nhất `round(x / r)·r` (hòa xa số 0) trước khi định dạng.
- **Dấu âm** chỉ in khi kết quả làm tròn khác 0: −0.00004 p4 → `0.0000`, không `-0.0000`.
- **MTEXT (`form = Mtext`), DIMFRAC:** 0 → `\S1/2;`, 1 → `\S1#2;`, liền sau phần nguyên (`1'-5\S1/2;"`, `17\S1/2;`);
  2 → `1/2` cách bằng dấu cách như dạng thường. Thập phân, kỹ thuật không đổi.

## 4. Góc

Góc vào bằng radian, quy về [0, 2π) như `angtos` (−45° → `315`; 720° → `0`); `useAngbase` thì trước đó đổi
`θ − ANGBASE` (ANGDIR 1: `ANGBASE − θ`). Không quay vòng sau khi làm tròn: 359.99999° p2 → `360.00`, `360d0'0"`.

| AUNITS | Dạng | Độ chính xác |
|---|---|---|
| 0 độ thập phân | `45.0000` (Mtext: `45%%d`) | p số lẻ |
| 1 độ/phút/giây | `45d30'15"` (Mtext: `45%%d30'15"`) | 0 → `45d`; 1–2 → `45d30'`; 3–4 → `45d30'15"`; 5–8 → giây có 1–4 số lẻ |
| 2 grad | `50.0000g` | p số lẻ |
| 3 radian | `0.7854r` | p số lẻ |
| 4 khảo sát | `N 45d30' E` (UNITMODE 1: `N45d30'E`) | như độ/phút/giây; đúng trục: `E`, `N`, `W`, `S` |

- Phút, giây không thêm số 0 đầu (`angtos` ví dụ `180d0'0"`).
- Độ/phút/giây làm tròn một lần ở đơn vị nhỏ nhất rồi tách, nên không có `60'`, `60"` (29.999999° p2 → `30d0'`).
- Khảo sát: phương vị tính từ Bắc/Nam về Đông/Tây; làm tròn trước rồi xét trục (89.9999999° p2 → `N`). Không theo
  ANGBASE/ANGDIR.
- DIMAZIN: 1 bỏ 0 đầu (`.5000`, `.5000r`), 2 bỏ 0 cuối (độ, grad, radian; với độ/phút/giây chỉ phần lẻ của giây).

## 5. Quy tắc làm tròn

**Nửa xa số 0, trên giá trị nhị phân chính xác của double, không bù sai số.**

- Hòa chính xác (giá trị double đúng bằng …5): làm tròn xa số 0 — 0.5 → `1`, 2.5 → `3`, −2.5 → `-3`, 0.125 p2 →
  `0.13`. Lý do: (1) AutoCAD dựng bằng MSVC qua nhiều đời; printf của CRT Microsoft trước bản UCRT 2020 làm tròn hòa
  xa số 0 (`"%.0f"` của 2.5 ra `3`), và AutoCAD giữ chuỗi ổn định giữa các phiên bản nên nhiều khả năng giữ hành vi
  này; (2) mô tả DIMRND
  ("làm tròn tới bội gần nhất") và thói quen bản vẽ kỹ thuật (2.5 → 3) không nói gì về làm tròn chẵn; (3) làm tròn
  chẵn của printf glibc/UCRT mới là chi tiết nền tảng, đúng loại phụ thuộc 05 §1.2 cấm.
- Gần hòa: theo giá trị nhị phân — 2.675 (= 2.67499999…) p2 → `2.67`, 1.005 → `1.00`, 0.95 p1 → `0.9`, 0.45 p1 → `0.5`.
  Lý do không bù: bù (ví dụ làm tròn trước về 15 chữ số có nghĩa) cần một ngưỡng tùy chọn, sai với tọa độ ≈ 1e9 (đã
  dùng hết 15–16 chữ số) và làm hai giá trị double khác nhau ra cùng chuỗi ở biên; nếu QA-110 cho thấy AutoCAD bù, thêm
  bước đó ở một chỗ (`roundFixed`, `scientific`).
- Cách làm, không printf/locale: `std::to_chars` (chuẩn C++17 quy định kết quả đúng chính xác như printf của locale
  "C") cho ca không hòa; ca hòa nhận ra chính xác bằng số học bit (`isHalfway`: 2·x·10^k là số nguyên lẻ) rồi tự cộng,
  nên không phụ thuộc quy tắc hòa của thư viện chuẩn. Đường nhanh (p ≤ 8): `y = phần_lẻ·10^p` lệch ≤ 2^-27, chỉ dùng khi
  phần lẻ của y cách 0,5 quá 1e-7; khoa học tương tự với một phép nhân/chia 10^k chính xác (k ≤ 22), biên 1e-6.
- Kiến trúc, phân số: nhân 2^p chính xác rồi `std::round` (hòa xa số 0).
- Góc: đổi bằng một phép nhân double (`θ·(180/π)`, `θ·(200/π)`, độ/phút/giây thêm `·đơn_vị_mỗi_độ`) rồi làm tròn như
  trên. Góc nhập 22.5° đổi qua radian và ngược lại cho đúng 22.5 → p0 → `23`; ca .5 của góc phụ thuộc phép đổi nên
  thuộc nhóm cần đối chiếu.

## 6. Ca vàng (`core/regen/tests/numfmt_test.cpp`)

376 ca, mỗi ca ghi chuỗi kỳ vọng từng ký tự; cờ `V` = cần đối chiếu QA-110 (tên ca có `[đối chiếu]` khi chạy test).

| Nhóm | Ca | Chắc | Đối chiếu | Nguồn của ca chắc |
|---|---|---|---|---|
| rtos-doc | 8 | 8 | 0 | ví dụ tài liệu AutoLISP `rtos` (17.5 ở chế độ 1–5, UNITMODE 0/1) |
| decimal | 45 | 39 | 6 | LUPREC 0–8, số âm, ≈ 1e9, 1e20, rất nhỏ, dưới chuẩn; số học chính xác |
| tie | 23 | 1 | 22 | biên .5 (§5) |
| dimzin-decimal | 21 | 15 | 6 | ví dụ tài liệu DIMZIN (`.5000`, `12.5`, `.5`), "0–3 chỉ áp feet-inch" |
| scientific | 25 | 14 | 11 | dạng `1.7500E+01` của tài liệu rtos |
| engineering | 30 | 19 | 11 | định nghĩa DIMZIN 0–3; tài liệu rtos |
| architectural | 43 | 26 | 17 | định nghĩa DIMZIN 0–3; mẫu 2^p |
| fractional | 22 | 16 | 6 | tài liệu rtos |
| windows-vi/fr/nogroup | 13 | 0 | 13 | mô tả DIMLUNIT 6 (dấu thập phân và nhóm theo Control Panel) |
| dimrnd | 16 | 13 | 3 | ví dụ tài liệu DIMRND (bội 0.25, 1.0) |
| dimdsep | 7 | 5 | 2 | định nghĩa DIMDSEP |
| mtext-frac0/1/2 | 14 | 4 | 10 | mã xếp chồng MTEXT `\S…/…;`, `\S…#…;` |
| angtos-doc | 8 | 8 | 0 | ví dụ tài liệu `angtos` (π: `180`, `180.0000`, `180d0'0"`, `3.1416r`, `W`) và UNITMODE (`N 45d E`, `N45dE`) |
| degrees | 22 | 17 | 5 | ví dụ DIMAZIN; quy về [0, 2π) |
| dms | 22 | 16 | 6 | danh sách độ chính xác hộp UNITS (`0d`, `0d00'`, `0d00'00"`, `0d00'00.0"`…) |
| grads-radians | 14 | 10 | 4 | hậu tố `g`, `r` |
| surveyor | 21 | 18 | 3 | `N 45d0' E`, `W` (tài liệu) |
| angbase/angdir | 14 | 13 | 1 | định nghĩa ANGBASE, ANGDIR |
| angle-mtext, angle-dsep | 8 | 2 | 6 | |
| **Tổng** | **376** | **244** | **132** | |

Ngoài ca vàng: mọi x = i/1024, |i| ≤ 20000, p 0–8 so với tham chiếu số nguyên (360 nghìn ca, phủ mọi kiểu hòa);
300 nghìn giá trị ngẫu nhiên mọi cỡ và gần hòa so với `to_chars` cho thập phân và khoa học; bộ đệm thiếu, độ dài tối
đa, đầu vào hỏng, UTF-8; 32 luồng; đếm cấp phát; benchmark.

### Cần đối chiếu QA-110 (hành vi suy luận)

1. Hòa chính xác: xa số 0 hay chẵn (nhóm tie, các ca .5 ở kỹ thuật, kiến trúc, phân số, khoa học, DIMRND).
2. Gần hòa: AutoCAD có bù sai số không (2.675, 1.005, 0.95…), và 18 chữ số có nghĩa ở p8 với ≈ 1e9
   (`2345678901.23449993`: CRT cũ chỉ in 17 chữ số có nghĩa).
3. Không in `-0.00`; DIMZIN 4 với 0 (`.0000`), DIMZIN 12 với 0 (`0`); khoa học với DIMZIN 8 (`1E+01`) và 12 (`0E+00`).
4. DIMZIN 0–3 cho giá trị 0 (`0"`, `0'`), cho phân số không feet (`0'-0 1/2"`), DIMZIN 4 cho inch kỹ thuật (`1'-.50"`),
   DIMZIN 8 cho kiến trúc (không áp); số âm feet-inch (`-1'-5 1/2"`); UNITMODE 1 khi không có feet (`6-1/2"`).
5. DIMLUNIT 6: có nhóm nghìn không, ký tự vùng nào; DIMDSEP có áp cho khoa học, kỹ thuật, góc không.
6. MTEXT của kích thước: phân số xếp chồng liền phần nguyên hay có dấu cách, có bọc `\H<DIMTFAC>x;` không (thuộc
   RGN-014 nếu có); ký hiệu độ `%%d` hay `°`.
7. Góc: AUPREC 1 và 3 với độ/phút/giây; 360 sau làm tròn có quay về 0 không; −ε quy thành `0`; khảo sát sát trục
   (`N` hay `N 0d E`) và có theo ANGBASE không; DIMAZIN với độ/phút/giây; AUPREC > 8.
8. `rtos` với độ chính xác > 8 (LISP hay dùng 16): đang in đúng số chữ số yêu cầu (`0.1000000000000000`).

## 7. Hiệu năng

`benchmark` trong test (bản release, i7-1255U lai P/E, máy dùng chung): đo theo lô 16 giá trị, mỗi kiểu 25 vòng giữ
vòng p99 thấp nhất. Lúc máy rảnh toàn bộ mẫu p50 ≈ 36 ns, p99 ≈ 48 ns/giá trị (chậm nhất: kiến trúc, độ/phút/giây p99 ≈ 50 ns); khi tải nặng (lõi E) p99 ≈ 110–130 ns.
Ngưỡng p99 ≤ 150 ns chỉ chặn ở bản release. Đường nóng không cấp phát (thay `operator new` trong test; kiểm ở preset
dev, release, win; ASan, TSan tự định nghĩa new/delete nên bỏ qua). 32 luồng chạy sạch dưới TSan. Toàn bộ test cũng
chạy trên Windows (preset win: llvm-mingw, libc++), cho cùng chuỗi với Linux (libstdc++).
