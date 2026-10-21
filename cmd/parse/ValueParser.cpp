// Phân tích giá trị nhập (INT-105). Đặc tả: docs/specs/input-values.md.
#include "cmd/parse/ValueParser.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>

namespace trion::cmd {
namespace {

using regen::AngularUnits;
using regen::LinearUnits;
using regen::UnitContext;

constexpr double kRadPerDeg = std::numbers::pi / 180;
constexpr double kRadPerGrad = std::numbers::pi / 200;

bool isDigit(char c) noexcept
{
    return c >= '0' && c <= '9';
}

char upper(char c) noexcept
{
    return c >= 'a' && c <= 'z' ? char(c - 'a' + 'A') : c;
}

// Con trỏ đọc trên [p, e). Lỗi cứng (tràn, mẫu 0, phút ≥ 60…) ghi vào err, giữ lỗi đầu tiên. Phần tùy chọn đọc thử
// trên bản sao rồi gán lại khi khớp, nên không khớp thì r không đổi.
struct Cursor {
    const char* p;
    const char* e;
    ParseError err = ParseError::None;

    bool at(char c) const noexcept { return p < e && *p == c; }
    bool eat(char c) noexcept
    {
        if (!at(c))
            return false;
        ++p;
        return true;
    }
    bool eatUpper(char c) noexcept // c viết hoa; nhận cả chữ thường
    {
        if (p == e || upper(*p) != c)
            return false;
        ++p;
        return true;
    }
    bool space() const noexcept { return at(' ') || at('\t'); }
    void spaces() noexcept
    {
        while (space())
            ++p;
    }
    void fail(ParseError x) noexcept
    {
        if (err == ParseError::None)
            err = x;
    }
};

// Dấu đầu: '-' trả true; '+' bỏ qua.
bool sign(Cursor& r) noexcept
{
    if (r.eat('-'))
        return true;
    r.eat('+');
    return false;
}

// ---- Thập phân → double, làm tròn đúng (gần nhất, hòa về chẵn như IEEE). Tự viết vì std::from_chars cho số thực
// thiếu ở DLL libc++ đi kèm Qt cho Windows (LLVM 17), bị đánh dấu unavailable trên Apple, và mỗi thư viện chuẩn
// cài một kiểu.
// Đường nhanh (Clinger): ≤ 16 chữ số (w < 2^53), |e| ≤ 22 là một phép nhân/chia chính xác. Còn lại tính bằng số nguyên
// lớn chính xác (D·5^e, hoặc thương D·2^s / 5^k kèm cờ dư) rồi làm tròn một lần ở bit 53.
// ponytail: > 16 chữ số luôn qua số nguyên lớn (~150 ns với 30 chữ số); cần nhanh hơn (đọc DXF hàng loạt) thì thêm
// Eisel-Lemire (bảng lũy thừa 5 128 bit) trước đường này.

constexpr int kMaxDigits = 800; // > 767 chữ số có nghĩa của mọi điểm giữa; phần sau gộp thành một chữ số "dính"
constexpr double kPow10[] = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
                             1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};

// Số nguyên không dấu, limb 32 bit, không cấp phát. Với giá trị trong [1e-307, 1e309) và ≤ 801 chữ số: < 2^2700
// (≤ 85 limb), nên 128 limb đủ.
struct Big {
    std::uint32_t d[128];
    int n = 0;
};

void mulAdd(Big& x, std::uint32_t m, std::uint32_t a) noexcept // x = x·m + a
{
    std::uint64_t carry = a;
    for (int i = 0; i < x.n; ++i) {
        const std::uint64_t t = std::uint64_t(x.d[i]) * m + carry;
        x.d[i] = std::uint32_t(t);
        carry = t >> 32;
    }
    if (carry)
        x.d[x.n++] = std::uint32_t(carry);
}

std::uint32_t pow5(int k) noexcept // k ≤ 13
{
    std::uint32_t p = 1;
    while (k-- > 0)
        p *= 5;
    return p;
}

template <std::uint32_t D> // x = ⌊x / D⌋; trả true nếu dư khác 0. D hằng: trình biên dịch thay phép chia bằng nhân
bool divSmall(Big& x) noexcept
{
    std::uint64_t r = 0;
    for (int i = x.n - 1; i >= 0; --i) {
        const std::uint64_t cur = r << 32 | x.d[i];
        x.d[i] = std::uint32_t(cur / D);
        r = cur % D;
    }
    while (x.n > 0 && x.d[x.n - 1] == 0)
        --x.n;
    return r != 0;
}

bool divPow5(Big& x, int k) noexcept // x = ⌊x / 5^k⌋, k ≤ 12, một lượt chia với hằng số
{
    switch (k) {
    case 1:
        return divSmall<5>(x);
    case 2:
        return divSmall<25>(x);
    case 3:
        return divSmall<125>(x);
    case 4:
        return divSmall<625>(x);
    case 5:
        return divSmall<3125>(x);
    case 6:
        return divSmall<15625>(x);
    case 7:
        return divSmall<78125>(x);
    case 8:
        return divSmall<390625>(x);
    case 9:
        return divSmall<1953125>(x);
    case 10:
        return divSmall<9765625>(x);
    case 11:
        return divSmall<48828125>(x);
    case 12:
        return divSmall<244140625>(x);
    default:
        return false;
    }
}

void shiftLeft(Big& x, int k) noexcept
{
    if (x.n == 0 || k == 0)
        return;
    const int w = k / 32, b = k % 32;
    x.d[x.n + w] = b ? x.d[x.n - 1] >> (32 - b) : 0;
    for (int i = x.n - 1; i > 0; --i)
        x.d[i + w] = b ? x.d[i] << b | x.d[i - 1] >> (32 - b) : x.d[i];
    x.d[w] = x.d[0] << b;
    for (int i = 0; i < w; ++i)
        x.d[i] = 0;
    x.n += w + (x.d[x.n + w] != 0);
}

int bitLength(const Big& x) noexcept
{
    return x.n ? (x.n - 1) * 32 + int(std::bit_width(x.d[x.n - 1])) : 0;
}

// Giá trị của k ≤ 19 chữ số; mỗi bước 4 chữ số để chuỗi phụ thuộc nhân-cộng ngắn.
std::uint64_t digitsValue(const char* p, int k) noexcept
{
    std::uint64_t w = 0;
    int i = 0;
    for (; i + 4 <= k; i += 4)
        w = w * 10000 + unsigned(p[i] - '0') * 1000u + unsigned(p[i + 1] - '0') * 100u +
            unsigned(p[i + 2] - '0') * 10u + unsigned(p[i + 3] - '0');
    for (; i < k; ++i)
        w = w * 10 + unsigned(p[i] - '0');
    return w;
}

// Chữ số dg[0, n) (không có số 0 đầu) × 10^e10, giá trị trong [1e-307, 1e309): double gần nhất. Tràn: false.
bool decimalToDouble(const char* dg, int n, int e10, double& out) noexcept
{
    if (n <= 16 && e10 >= -22 && e10 <= 22) { // w < 10^16 vừa uint64; w ≤ 2^53 thì double(w) chính xác
        const std::uint64_t w = digitsValue(dg, n);
        if (w <= (1ull << 53)) {
            out = e10 < 0 ? double(w) / kPow10[-e10] : double(w) * kPow10[e10];
            return true;
        }
    }
    Big x; // D, mỗi lần ≤ 9 chữ số
    for (int i = 0; i < n; i += 9) {
        const int k = std::min(n - i, 9);
        mulAdd(x, std::uint32_t(kPow10[k]), std::uint32_t(digitsValue(dg + i, k)));
    }
    int e2 = e10; // giá trị = x·2^e2 (+ phần dư nếu sticky)
    bool sticky = false;
    if (e10 >= 0) {
        for (int k = e10; k > 0; k -= 13)
            mulAdd(x, pow5(std::min(k, 13)), 0);
    } else { // ⌊D·2^s / 5^k⌋ có ≥ 64 bit: k·log2(5) < k·2.322
        const int k = -e10, s = std::max(0, 67 + k * 2322 / 1000 - bitLength(x));
        shiftLeft(x, s);
        e2 -= s;
        int r = k;
        for (; r >= 13; r -= 13)
            sticky |= divSmall<1220703125>(x); // 5^13
        sticky |= divPow5(x, r);
    }
    // 64 bit cao của x (căn trái); bit thấp hơn khác 0 thì gộp vào sticky.
    const int len = bitLength(x), sh = len - 64;
    const auto limb = [&x](int i) { return i < x.n ? std::uint64_t(x.d[i]) : 0; };
    std::uint64_t top = 0;
    if (sh <= 0) {
        top = (limb(0) | limb(1) << 32) << -sh;
    } else {
        const int li = sh / 32, bo = sh % 32;
        const std::uint64_t lo = limb(li) | limb(li + 1) << 32;
        top = bo ? lo >> bo | limb(li + 2) << (64 - bo) : lo;
        sticky |= (x.d[li] & ((1u << bo) - 1)) != 0;
        for (int i = 0; i < li && !sticky; ++i)
            sticky = x.d[i] != 0;
    }
    std::uint64_t m = top >> 11; // 53 bit; bit 10 là bit làm tròn
    if (top >> 10 & 1 && ((top & 0x3FF) || sticky || (m & 1)))
        ++m;
    int ex = e2 + len - 1 + 1023; // trường mũ IEEE: m ∈ [2^52, 2^53) là 1.f·2^(e2 + len − 1)
    if (m >> 53) {
        m >>= 1;
        ++ex;
    }
    if (ex >= 2047) // tràn trên; số dưới chuẩn không tới được (giá trị ≥ 1e-307)
        return false;
    out = std::bit_cast<double>(std::uint64_t(ex) << 52 | (m & ((1ull << 52) - 1)));
    return true;
}

// Số không dấu: chữ_số+ [. chữ_số*] | . chữ_số+, rồi [e|E [+|-] chữ_số+] nếu exp. Không có số: trả false, giữ r.p.
// integer: chỉ gồm chữ số. Bậc thập phân của chữ số có nghĩa đầu ngoài −307…308 là OutOfRange.
// ponytail: không nhận số dưới chuẩn (0 < |x| < 1e-307), vô nghĩa với giá trị nhập; định dạng tệp cần thì làm tròn
// dưới chuẩn ở decimalToDouble.
bool number(Cursor& r, bool exp, double& v, bool* integer = nullptr) noexcept
{
    // Một lượt: chữ số có nghĩa vào dg (≤ kMaxDigits; phần sau khác 0 thành chữ số dính), giá trị = dg·10^ex.
    char dg[kMaxDigits + 1];
    int n = 0;
    long long ex = 0;
    bool dot = false, any = false, sticky = false;
    const char* q = r.p;
    for (; q < r.e; ++q) {
        const char c = *q;
        if (isDigit(c)) {
            any = true;
            if (n < kMaxDigits && (n || c != '0')) { // số 0 đầu không ghi, chỉ đổi bậc nếu sau dấu chấm
                dg[n++] = c;
                ex -= dot;
            } else if (n == 0) {
                ex -= dot;
            } else {
                sticky |= c != '0';
                ex += !dot;
            }
        } else if (c == '.' && !dot) {
            dot = true;
        } else {
            break;
        }
    }
    if (!any) // rỗng hoặc chỉ "."
        return false;
    const char* mantEnd = q;
    long long e10 = 0;
    if (exp && q < r.e && (*q == 'e' || *q == 'E')) {
        const char* t = q + 1;
        const bool neg = t < r.e && *t == '-';
        if (t < r.e && (*t == '+' || *t == '-'))
            ++t;
        if (t < r.e && isDigit(*t)) {
            for (; t < r.e && isDigit(*t); ++t) // bão hòa ở 1e15: lớn hơn mọi độ dài chuỗi, đủ quyết tràn
                e10 = std::min(e10 * 10 + (*t - '0'), 1'000'000'000'000'000LL);
            if (neg)
                e10 = -e10;
            q = t;
        }
    }
    if (integer)
        *integer = !dot && q == mantEnd;
    r.p = q;
    v = 0;
    if (n == 0)
        return true;
    ex += e10;
    if (const long long mag = ex + n - 1; mag > 308 || mag < -307) {
        r.fail(ParseError::OutOfRange);
        return true;
    }
    if (sticky) {
        dg[n++] = '1';
        --ex;
    } else {
        while (dg[n - 1] == '0') {
            --n;
            ++ex;
        }
    }
    if (!decimalToDouble(dg, n, int(ex), v)) // tràn trên ~1.8e308
        r.fail(ParseError::OutOfRange);
    return true;
}

// Sau số nguyên v vừa đọc: "/mẫu" (v là tử) hoặc ('-' | khoảng trắng) "tử/mẫu" (v là phần nguyên); tử, mẫu chỉ gồm
// chữ số. Không khớp: giữ r, v.
void fraction(Cursor& r, double& v) noexcept
{
    Cursor t = r;
    double whole = 0, num = v, den = 0;
    bool isInt = true;
    if (!t.eat('/')) {
        if (!t.eat('-')) {
            if (!t.space())
                return;
            t.spaces();
        }
        whole = v;
        if (!number(t, false, num, &isInt) || !isInt || !t.eat('/'))
            return;
    }
    if (!number(t, false, den, &isInt) || !isInt)
        return;
    if (den == 0)
        t.fail(ParseError::ZeroDenominator);
    r = t;
    v = den == 0 ? 0 : whole + num / den;
}

// Độ lớn không dấu của độ dài: số thực, khoa học, phân số, hỗn số.
bool magnitude(Cursor& r, double& v) noexcept
{
    bool isInt = false;
    if (!number(r, true, v, &isInt))
        return false;
    if (isInt)
        fraction(r, v);
    return true;
}

// [+|-] độ_lớn [' [khoảng trắng] [-] [khoảng trắng] [độ_lớn ["]] | "]. Feet nhân 12 cộng inch.
bool distance(Cursor& r, const UnitContext& c, double& v) noexcept
{
    const bool neg = sign(r);
    if (!magnitude(r, v))
        return false;
    if (r.at('\'') || r.at('"')) {
        if (c.lunits != LinearUnits::Engineering && c.lunits != LinearUnits::Architectural) {
            r.fail(ParseError::FeetInchNotAllowed);
            return true;
        }
        if (r.eat('\'')) {
            v *= 12;
            Cursor t = r;
            t.spaces();
            t.eat('-');
            t.spaces();
            double inch = 0;
            if (magnitude(t, inch)) {
                t.eat('"');
                v += inch;
                r = t;
            }
        } else {
            r.eat('"');
        }
    }
    if (neg)
        v = -v;
    return true;
}

bool eatDegree(Cursor& r) noexcept // d, D hoặc ° (UTF-8 C2 B0)
{
    if (r.eatUpper('D'))
        return true;
    if (r.e - r.p < 2 || r.p[0] != '\xC2' || r.p[1] != '\xB0')
        return false;
    r.p += 2;
    return true;
}

// Sau "độ" + d: [phút ['\'' [giây ['"']]]] cộng vào deg. Thành phần có thành phần sau phải nguyên (45d30', không
// 45.5d30'); phút, giây < 60.
void dms(Cursor& r, double& deg, bool degInt) noexcept
{
    Cursor t = r;
    double m = 0, s = 0;
    bool minInt = false;
    if (!degInt || !number(t, false, m, &minInt))
        return;
    if (m >= 60)
        t.fail(ParseError::OutOfRange);
    deg += m / 60;
    r = t;
    if (!r.eat('\'') || !minInt)
        return;
    t = r;
    if (!number(t, false, s))
        return;
    if (s >= 60)
        t.fail(ParseError::OutOfRange);
    deg += s / 3600;
    t.eat('"');
    r = t;
}

// Phương vị: N|S [khoảng trắng] độ [d [phút…]] [khoảng trắng] E|W, hoặc một chữ N, E, S, W (hoa hay thường).
// deg: độ tuyệt đối từ hướng Đông, ngược chiều kim đồng hồ.
bool surveyor(Cursor& r, double& deg) noexcept
{
    if (r.p == r.e)
        return false;
    const char ns = upper(*r.p);
    if (ns != 'N' && ns != 'S' && ns != 'E' && ns != 'W')
        return false;
    ++r.p;
    deg = ns == 'E' ? 0 : ns == 'N' ? 90 : ns == 'W' ? 180 : 270;
    if (ns == 'E' || ns == 'W')
        return true;
    Cursor t = r;
    t.spaces();
    double b = 0;
    bool isInt = false;
    if (!number(t, false, b, &isInt))
        return true;
    if (eatDegree(t))
        dms(t, b, isInt);
    t.spaces();
    const bool east = t.eatUpper('E');
    if (!east && !t.eatUpper('W'))
        return true;
    if (b > 90)
        t.fail(ParseError::OutOfRange);
    deg += (ns == 'N') == east ? -b : b;
    r = t;
    return true;
}

bool angle(Cursor& r, const UnitContext& c, double& rad) noexcept
{
    if (surveyor(r, rad)) {
        rad *= kRadPerDeg;
        return true;
    }
    const bool neg = sign(r);
    double v = 0;
    bool isInt = false;
    if (!number(r, true, v, &isInt))
        return false;
    if (eatDegree(r)) {
        dms(r, v, isInt);
        rad = v * kRadPerDeg;
    } else if (r.eatUpper('G')) {
        rad = v * kRadPerGrad;
    } else if (r.eatUpper('R')) {
        rad = v;
    } else {
        rad = c.aunits == AngularUnits::Grads     ? v * kRadPerGrad
              : c.aunits == AngularUnits::Radians ? v
                                                  : v * kRadPerDeg;
    }
    if (neg)
        rad = -rad;
    if (c.useAngbase)
        rad = c.angdir ? c.angbase - rad : c.angbase + rad;
    return true;
}

// Có ',' kề chữ số: dấu thập phân kiểu Việt/Đức ("1,5") hoặc dấu nhóm nghìn ("1,000").
bool decimalComma(std::string_view s) noexcept
{
    for (std::size_t i = s.find(','); i < s.size(); i = s.find(',', i + 1))
        if ((i > 0 && isDigit(s[i - 1])) || (i + 1 < s.size() && isDigit(s[i + 1])))
            return true;
    return false;
}

// Khung chung: khoảng trắng, chuỗi rỗng, dấu phẩy, thừa ký tự, lỗi cứng, kết quả không hữu hạn.
template <class F>
Parsed<double> run(std::string_view s, ParseMode mode, F f) noexcept
{
    Cursor r{s.data(), s.data() + s.size()};
    r.spaces();
    if (mode == ParseMode::Whole)
        while (r.e > r.p && (r.e[-1] == ' ' || r.e[-1] == '\t'))
            --r.e;
    if (r.p == r.e)
        return {0, ParseError::Empty};
    if (mode == ParseMode::Whole && decimalComma(s))
        return {0, ParseError::CommaAsDecimal};
    double v = 0;
    const bool found = f(r, v);
    if (r.err == ParseError::None && (!found || (mode == ParseMode::Whole && r.p != r.e)))
        r.err = ParseError::Syntax;
    // Kết quả cùng phạm vi với số gõ trực tiếp: tràn (feet·12, ANGBASE không hữu hạn) và khác 0 nhưng < 1e-307 (phân số
    // 6/7e307, độ → radian của 1e-307), để mọi kết quả định dạng lại (RGN-013) rồi đọc lại được.
    if (r.err == ParseError::None && (!std::isfinite(v) || (v != 0 && std::fabs(v) < 1e-307)))
        r.err = ParseError::OutOfRange;
    if (r.err != ParseError::None)
        return {0, r.err};
    return {v, ParseError::None, mode == ParseMode::Whole ? s.size() : std::size_t(r.p - s.data())};
}

} // namespace

std::string_view errorCode(ParseError e) noexcept
{
    switch (e) {
    case ParseError::None:
        return "ok";
    case ParseError::Empty:
        return "empty";
    case ParseError::Syntax:
        return "syntax";
    case ParseError::CommaAsDecimal:
        return "comma";
    case ParseError::OutOfRange:
        return "range";
    case ParseError::NotInteger:
        return "not-integer";
    case ParseError::FeetInchNotAllowed:
        return "feet-inch";
    case ParseError::ZeroDenominator:
        return "zero-denominator";
    }
    return "unknown";
}

Parsed<double> parseReal(std::string_view s, ParseMode mode) noexcept
{
    return run(s, mode, [](Cursor& r, double& v) {
        const bool neg = sign(r);
        if (!number(r, true, v))
            return false;
        if (neg)
            v = -v;
        return true;
    });
}

Parsed<int> parseInt(std::string_view s, ParseMode mode) noexcept
{
    const Parsed<double> d = run(s, mode, [](Cursor& r, double& v) {
        const bool neg = sign(r);
        bool isInt = false;
        if (!number(r, true, v, &isInt))
            return false;
        if (neg)
            v = -v;
        if (!isInt)
            r.fail(ParseError::NotInteger);
        else if (v < -32768 || v > 32767)
            r.fail(ParseError::OutOfRange);
        return true;
    });
    return {int(d.value), d.error, d.end};
}

Parsed<double> parseDistance(std::string_view s, const UnitContext& ctx, ParseMode mode) noexcept
{
    return run(s, mode, [&ctx](Cursor& r, double& v) { return distance(r, ctx, v); });
}

Parsed<double> parseAngle(std::string_view s, const UnitContext& ctx, ParseMode mode) noexcept
{
    return run(s, mode, [&ctx](Cursor& r, double& v) { return angle(r, ctx, v); });
}

} // namespace trion::cmd
