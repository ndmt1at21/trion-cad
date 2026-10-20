// Định dạng độ dài: LUNITS/DIMLUNIT 1–6 (đặc tả: docs/specs/regen/number-format.md §3).
#include "core/regen/numfmt/Digits.h"
#include "core/regen/numfmt/NumFormat.h"

#include <algorithm>

namespace trion::regen {
namespace {

using detail::Writer;

// Chia chuỗi chữ số phần nguyên (inch) cho 12 tại chỗ: còn lại số feet, trả số inch nguyên 0–11.
unsigned splitFeet(detail::Fixed& r) noexcept
{
    char* d = r.intBuf;
    unsigned rem = 0;
    std::size_t n = 0;
    for (const char c : r.ip) { // ghi đè vị trí đã đọc (n ≤ chỉ số đang đọc)
        rem = rem * 10 + unsigned(c - '0');
        const char q = char('0' + rem / 12);
        rem %= 12;
        if (n || q != '0')
            d[n++] = q;
    }
    if (!n)
        d[n++] = '0';
    r.ip = {d, n};
    return rem;
}

// Làm tròn a ≥ 0 tới 1/2^p (kiến trúc, phân số). Nhân với lũy thừa 2 là chính xác, std::round là nửa xa số 0.
// Phần nguyên vào r.ip, phân số tối giản num/den (num = 0: không có phân số).
void roundBinary(double a, int p, detail::Fixed& r, unsigned& num, unsigned& den) noexcept
{
    double w = std::floor(a);
    den = 1u << p;
    num = unsigned(std::round((a - w) * den));
    if (num == den) {
        w += 1;
        num = 0;
    }
    const int s = num ? std::min(std::countr_zero(num), p) : p;
    num >>= s;
    den >>= s;
    detail::wholeDigits(w, r);
}

// Phân số sau phần nguyên (afterWhole) hoặc đứng một mình. Mtext + DIMFRAC 0/1: xếp chồng \S1/2; hoặc \S1#2;
// liền sau phần nguyên; còn lại "1/2" cách phần nguyên bằng ' ' (UNITMODE 1: '-').
void putFraction(Writer& o, unsigned num, unsigned den, const UnitContext& c, bool afterWhole) noexcept
{
    if (c.form == TextForm::Mtext && c.dimfrac != 2) {
        o.put("\\S");
        o.putUInt(num);
        o.put(c.dimfrac == 1 ? '#' : '/');
        o.putUInt(den);
        o.put(';');
        return;
    }
    if (afterWhole)
        o.put(c.unitmode ? '-' : ' ');
    o.putUInt(num);
    o.put('/');
    o.putUInt(den);
}

// DIMZIN bit 0–1 cho feet-inch: 0 bỏ 0 feet và inch đúng bằng 0; 1 giữ cả hai; 2 giữ 0 feet, bỏ inch 0;
// 3 giữ inch 0, bỏ 0 feet. Ẩn feet thì luôn hiện inch (giá trị 0 → 0").
struct Shown {
    bool feet, inch;
};
Shown feetInch(const UnitContext& c, bool feetZero, bool inchZero) noexcept
{
    const unsigned z = c.dimzin & 3u;
    const bool feet = !feetZero || z == 1 || z == 2;
    return {feet, !inchZero || z == 1 || z == 3 || !feet};
}

void putFeet(Writer& o, std::string_view feet, Shown s, const UnitContext& c) noexcept
{
    if (!s.feet)
        return;
    o.put(feet);
    o.put('\'');
    if (s.inch && !c.unitmode)
        o.put('-');
}

// 1'-5.50": inch thập phân, DIMZIN 4/8 áp lên số inch.
void engineering(Writer& o, bool neg, double a, int p, const UnitContext& c) noexcept
{
    detail::Fixed r;
    detail::roundFixed(a, p, r);
    const unsigned inch = splitFeet(r);
    const bool feetZero = r.ip == "0";
    const bool inchZero = inch == 0 && r.fp.find_first_not_of('0') == std::string_view::npos;
    const Shown s = feetInch(c, feetZero, inchZero);
    if (neg && !(feetZero && inchZero))
        o.put('-');
    putFeet(o, r.ip, s, c);
    if (!s.inch)
        return;
    char ib[2];
    std::string_view ip(ib, std::size_t(std::to_chars(ib, ib + 2, inch).ptr - ib)), fp = r.fp;
    detail::suppressZeros(ip, fp, c.dimzin & 4, c.dimzin & 8);
    detail::putDigits(o, ip, fp, c.dimdsep);
    o.put('"');
}

// 1'-5 1/2": inch nguyên 0 bị bỏ khi chỉ còn phân số và không hiện feet (1/2", nhưng 1'-0 1/2").
void architectural(Writer& o, bool neg, double a, int p, const UnitContext& c) noexcept
{
    detail::Fixed r;
    unsigned num, den;
    roundBinary(a, p, r, num, den);
    const unsigned inch = splitFeet(r);
    const bool feetZero = r.ip == "0";
    const bool inchZero = inch == 0 && num == 0;
    const Shown s = feetInch(c, feetZero, inchZero);
    if (neg && !(feetZero && inchZero))
        o.put('-');
    putFeet(o, r.ip, s, c);
    if (!s.inch)
        return;
    const bool whole = num == 0 || inch != 0 || s.feet;
    if (whole)
        o.putUInt(inch);
    if (num)
        putFraction(o, num, den, c, whole);
    o.put('"');
}

// 17 1/2, 1/2, 17.
void fractional(Writer& o, bool neg, double a, int p, const UnitContext& c) noexcept
{
    detail::Fixed r;
    unsigned num, den;
    roundBinary(a, p, r, num, den);
    const bool wholeZero = r.ip == "0";
    if (neg && !(wholeZero && num == 0))
        o.put('-');
    if (!wholeZero || num == 0)
        o.put(r.ip);
    if (num)
        putFraction(o, num, den, c, !wholeZero);
}

// Đường nhanh của định trị (p ≤ 8, a > 0, |p − E| ≤ 22): N = a·10^(p−E) làm tròn, chỉ một phép nhân hoặc chia với
// 10^k chính xác nên lệch giá trị đúng ≤ 2^-23 (N < 10^9 < 2^30); phần lẻ cách 0,5 quá 1e-6 thì hướng làm tròn chắc.
// E ước từ số mũ nhị phân (đúng hoặc nhỏ hơn 1). Trả false khi không chắc: đi đường chính xác.
bool scientificFast(double a, int p, char* md, int& exp) noexcept
{
    if (p > 8 || a == 0)
        return false;
    int e2;
    std::frexp(a, &e2); // a ∈ [2^(e2−1), 2^e2)
    for (int e = int(std::floor((e2 - 1) * 0.30102999566398120)), pass = 0; pass < 2; ++pass, ++e) {
        const int k = p - e;
        if (k < -22 || k > 22)
            return false;
        const double y = k >= 0 ? a * detail::kPow10[k] : a / detail::kPow10[-k];
        if (y >= detail::kPow10[p + 1])
            continue; // E thật là e + 1
        const double f = std::floor(y), d = y - f;
        if (std::fabs(d - 0.5) <= 1e-6)
            return false;
        auto n = std::uint64_t(f) + (d > 0.5);
        if (n == std::uint64_t(detail::kPow10[p + 1])) { // 9.99…5 → 1.00…E+1
            n /= 10;
            ++e;
        }
        for (int i = p; i >= 0; --i, n /= 10)
            md[i] = char('0' + n % 10);
        exp = e;
        return true;
    }
    return false;
}

// 1.7500E+01: mũ có dấu, ít nhất 2 chữ số; DIMZIN 4/8 áp lên phần định trị.
void scientific(Writer& o, bool neg, double a, int p, const UnitContext& c) noexcept
{
    char md[detail::kMaxDecimals + 1];
    int exp = 0;
    if (!scientificFast(a, p, md, exp)) {
        // Chính xác: to_chars làm tròn đúng ngoài ca hòa; ca hòa (isHalfway) in thêm 1 chữ số (chính xác, tận cùng
        // '5') rồi tự cộng 1, nên không phụ thuộc quy tắc hòa của thư viện chuẩn.
        char t[detail::kMaxDecimals + 16];
        auto parse = [&](int prec) {
            char* end = std::to_chars(t, t + sizeof t, a, std::chars_format::scientific, prec).ptr;
            const char* e = std::find(t, end, 'e') + 1;
            std::from_chars(e + (*e == '+'), end, exp);
        };
        parse(p);
        const bool tie = detail::isHalfway(a, p - exp);
        if (tie)
            parse(p + 1);
        md[0] = t[0];
        for (int i = 0; i < p; ++i)
            md[1 + i] = t[2 + i];
        if (tie) {
            int i = p;
            for (; i >= 0 && md[i] == '9'; --i)
                md[i] = '0';
            if (i >= 0) {
                ++md[i];
            } else {
                md[0] = '1';
                ++exp;
            }
        }
    }
    const bool zero = md[0] == '0'; // chỉ khi a = 0
    std::string_view ip(md, 1), fp(md + 1, std::size_t(p));
    detail::suppressZeros(ip, fp, c.dimzin & 4, c.dimzin & 8);
    if (neg && !zero)
        o.put('-');
    detail::putDigits(o, ip, fp, c.dimdsep);
    o.put('E');
    o.put(exp < 0 ? '-' : '+');
    const unsigned ue = unsigned(exp < 0 ? -exp : exp);
    if (ue < 10)
        o.put('0');
    o.putUInt(ue);
}

} // namespace

std::size_t formatLinear(double v, const UnitContext& c, std::span<char> out) noexcept
{
    Writer o{out.data(), out.size()};
    if (c.dimrnd > 0 && std::isfinite(c.dimrnd)) { // DIMRND: bội gần nhất, hòa thì xa số 0
        const double q = v / c.dimrnd;
        if (std::isfinite(q))
            v = std::round(q) * c.dimrnd;
    }
    if (!std::isfinite(v)) {
        detail::putNonFinite(o, v);
        return o.n;
    }
    const bool neg = std::signbit(v);
    const double a = std::fabs(v);
    const int pd = std::clamp(c.luprec, 0, detail::kMaxDecimals);
    const int pb = std::clamp(c.luprec, 0, 8);
    switch (c.lunits) {
    case LinearUnits::Scientific:
        scientific(o, neg, a, pd, c);
        break;
    case LinearUnits::Engineering:
        engineering(o, neg, a, pd, c);
        break;
    case LinearUnits::Architectural:
        architectural(o, neg, a, pb, c);
        break;
    case LinearUnits::Fractional:
        fractional(o, neg, a, pb, c);
        break;
    case LinearUnits::WindowsDesktop:
        detail::putFixed(o, neg, a, pd, c.dimzin & 4, c.dimzin & 8, c.regionDecimal, c.regionGroup);
        break;
    default: // Decimal; giá trị ngoài 1–6 coi như thập phân
        detail::putFixed(o, neg, a, pd, c.dimzin & 4, c.dimzin & 8, c.dimdsep, 0);
        break;
    }
    return o.n;
}

std::string linearString(double value, const UnitContext& ctx)
{
    char b[kMaxNumText];
    return std::string(b, std::min(formatLinear(value, ctx, b), sizeof b));
}

} // namespace trion::regen
