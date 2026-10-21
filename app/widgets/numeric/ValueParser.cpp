// Ô số UI-C09 (UI0-109). Đặc tả: docs/specs/input-values.md §10.
#include "app/widgets/numeric/ValueParser.h"

#include "cmd/parse/ValueParser.h"
#include "core/regen/numfmt/NumFormat.h"

#include <QCoreApplication>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <numbers>
#include <string>

namespace trion::ui {
namespace {

using cmd::ParseError;
using cmd::ParseMode;
using regen::AngularUnits;
using regen::LinearUnits;
using regen::UnitContext;

// Cùng hằng số với INT-105: "=45" và "45" ở ô góc cho cùng một double.
constexpr double kRadPerDeg = std::numbers::pi / 180;
constexpr double kRadPerGrad = std::numbers::pi / 200;
constexpr int kMaxDepth = 32; // ngoặc lồng tối đa: chặn độ sâu đệ quy (fuzz)
constexpr int kInch = 1;      // mã INSUNITS
constexpr int kBare = -1;     // số trần: không gõ kèm đơn vị

// Mét trên một đơn vị theo mã INSUNITS, dạng tử/mẫu nguyên: quy đổi v·(tử_a·mẫu_b) / (mẫu_a·tử_b) chỉ làm tròn
// một lần khi v·tử chính xác (42 inch → 1066.8 mm là double gần nhất; 25.4 thì không biểu diễn chính xác được).
struct Ratio {
    double num, den;
};
constexpr Ratio kMeters[] = {
    {0, 1},                     // 0 không đơn vị
    {127, 5000},                // 1 inch
    {381, 1250},                // 2 feet
    {201168, 125},              // 3 dặm
    {1, 1000},                  // 4 mm
    {1, 100},                   // 5 cm
    {1, 1},                     // 6 m
    {1000, 1},                  // 7 km
    {127, 5e9},                 // 8 microinch
    {127, 5e6},                 // 9 mil
    {1143, 1250},               // 10 yard
    {1, 1e10},                  // 11 angstrom
    {1, 1e9},                   // 12 nanomet
    {1, 1e6},                   // 13 micromet
    {1, 10},                    // 14 dm
    {10, 1},                    // 15 dam
    {100, 1},                   // 16 hm
    {1e9, 1},                   // 17 gigamet
    {149597870700, 1},          // 18 đơn vị thiên văn
    {9460730472580800, 1},      // 19 năm ánh sáng
    {3.0856775814913673e16, 1}, // 20 parsec (648000/π au, xấp xỉ double)
    {1200, 3937},               // 21 foot khảo sát Mỹ
    {100, 3937},                // 22 inch khảo sát Mỹ
    {3600, 3937},               // 23 yard khảo sát Mỹ
    {6336000, 3937},            // 24 dặm khảo sát Mỹ
};
constexpr int kUnitCount = int(std::size(kMeters));

// Hậu tố chữ (không phân biệt hoa/thường) → mã INSUNITS. ' và " đọc riêng (feet-inch của INT-105).
struct Suffix {
    std::string_view name;
    int unit;
};
constexpr Suffix kSuffixes[] = {{"mm", 4}, {"cm", 5}, {"m", 6}, {"km", 7}, {"in", 1}, {"ft", 2}};

// Toán tử Unicode của biểu thức (UTF-8): − (U+2212), × (U+00D7), ÷ (U+00F7).
constexpr std::string_view kMinus = "\xE2\x88\x92", kTimes = "\xC3\x97", kDivide = "\xC3\xB7";

bool isSpace(char c) noexcept
{
    return c == ' ' || c == '\t';
}

bool isDigit(char c) noexcept
{
    return c >= '0' && c <= '9';
}

bool isAlpha(char c) noexcept
{
    return (c | 0x20) >= 'a' && (c | 0x20) <= 'z';
}

std::string_view trimLeft(std::string_view s) noexcept
{
    while (!s.empty() && isSpace(s.front()))
        s.remove_prefix(1);
    return s;
}

std::string_view trim(std::string_view s) noexcept
{
    s = trimLeft(s);
    while (!s.empty() && isSpace(s.back()))
        s.remove_suffix(1);
    return s;
}

// ',' cạnh chữ số, như INT-105 ở ParseMode::Whole (biểu thức, hậu tố đọc bằng Prefix nên phải tự xét).
bool decimalComma(std::string_view s) noexcept
{
    for (std::size_t i = s.find(','); i < s.size(); i = s.find(',', i + 1))
        if ((i > 0 && isDigit(s[i - 1])) || (i + 1 < s.size() && isDigit(s[i + 1])))
            return true;
    return false;
}

ValueError fromCmd(ParseError e) noexcept
{
    switch (e) {
    case ParseError::None:
        return ValueError::None;
    case ParseError::Empty:
        return ValueError::Empty;
    case ParseError::CommaAsDecimal:
        return ValueError::Comma;
    case ParseError::OutOfRange:
        return ValueError::OutOfRange;
    case ParseError::NotInteger:
        return ValueError::NotInteger;
    case ParseError::ZeroDenominator:
        return ValueError::DivideByZero;
    case ParseError::Syntax:
    case ParseError::FeetInchNotAllowed: // chỉ ô tỉ lệ gặp: ' hoặc " ở số không đơn vị
        return ValueError::Syntax;
    }
    return ValueError::Syntax;
}

// Đơn vị bản vẽ thực (mã INSUNITS): LUNITS kỹ thuật, kiến trúc hiển thị 1 đơn vị = 1 inch (RGN-013); 0 = không
// đơn vị.
int drawingUnit(const ValueSpec& spec) noexcept
{
    const LinearUnits u = spec.units.lunits;
    if (u == LinearUnits::Engineering || u == LinearUnits::Architectural)
        return kInch;
    return spec.insunits > 0 && spec.insunits < kUnitCount ? spec.insunits : 0;
}

// Radian trên một đơn vị của ô góc: số không hậu tố theo AUNITS (độ với 0, 1, 4 và ngoài khoảng).
double angleUnit(const ValueSpec& spec) noexcept
{
    return spec.units.aunits == AngularUnits::Grads     ? kRadPerGrad
           : spec.units.aunits == AngularUnits::Radians ? 1
                                                        : kRadPerDeg;
}

int suffixUnit(std::string_view w) noexcept
{
    for (const Suffix& s : kSuffixes)
        if (std::equal(w.begin(), w.end(), s.name.begin(), s.name.end(),
                       [](char a, char b) { return (a | 0x20) == b; }))
            return s.unit;
    return -1;
}

// v theo đơn vị `from` → đơn vị `to` (mã INSUNITS). Bản vẽ không đơn vị mà chuỗi có đơn vị: Units.
double convert(double v, int from, int to, ValueError& err) noexcept
{
    if (from == to)
        return v;
    if (to == 0) {
        err = ValueError::Units;
        return 0;
    }
    const Ratio a = kMeters[from], b = kMeters[to];
    return v * (a.num * b.den) / (a.den * b.num);
}

// Độ dài ở đầu s theo INT-105 (kiến trúc, Prefix: số, phân số, hỗn số, feet-inch ở mọi LUNITS); không có ' " thì
// thêm [khoảng trắng] hậu tố chữ. unit: mã INSUNITS của đơn vị gõ kèm (feet-inch: inch), kBare khi không có hoặc chữ
// lạ (phần sau báo Syntax). Trong biểu thức (expr) feet-inch dừng sau ' khi theo sau là khoảng trắng (2' - 1" là phép
// trừ) hoặc khi phần inch chạm ' thứ hai (10'-2' là 10' trừ 2').
struct Length {
    cmd::Parsed<double> n;
    int unit = kBare;
    std::size_t end = 0;
};

Length readLength(std::string_view s, bool expr) noexcept
{
    UnitContext c;
    c.lunits = LinearUnits::Architectural;
    Length l{cmd::parseDistance(s, c, ParseMode::Prefix)};
    if (!l.n)
        return l;
    const std::size_t q = s.substr(0, l.n.end).find('\'');
    if (expr && q != std::string_view::npos &&
        ((q + 1 < l.n.end && isSpace(s[q + 1])) || (l.n.end < s.size() && s[l.n.end] == '\'')))
        l.n = cmd::parseDistance(s.substr(0, q + 1), c, ParseMode::Prefix);
    l.end = l.n.end;
    if (s.substr(0, l.end).find_first_of("'\"") != std::string_view::npos) {
        l.unit = kInch;
        return l;
    }
    const std::size_t k = std::size_t(trimLeft(s.substr(l.end)).data() - s.data());
    std::size_t w = k;
    while (w < s.size() && isAlpha(s[w]))
        ++w;
    if (w > k)
        l.unit = suffixUnit(s.substr(k, w - k));
    if (l.unit != kBare)
        l.end = w;
    return l;
}

// Biểu thức sau '=' (đặc tả §10.3): tổng → tích → dấu một ngôi → ngoặc | toán hạng, đệ quy xuống, ngoặc lồng
// ≤ kMaxDepth. Tính theo đơn vị của ô: độ dài theo đơn vị bản vẽ, góc theo AUNITS (độ, grad, radian) để "=180/4" là
// 45°; đổi sang radian một lần ở cuối. Lỗi đầu tiên được giữ, sau đó các hàm trả 0 và dừng.
struct Expr {
    const char* p;
    const char* e;
    const ValueSpec& spec;
    int unit = drawingUnit(spec);
    ValueError err = ValueError::None;
    int depth = 0;

    void fail(ValueError x) noexcept
    {
        if (err == ValueError::None)
            err = x;
    }
    void spaces() noexcept
    {
        while (p < e && isSpace(*p))
            ++p;
    }
    bool eat(std::string_view t) noexcept
    {
        if (std::size_t(e - p) < t.size() || std::memcmp(p, t.data(), t.size()) != 0)
            return false;
        p += t.size();
        return true;
    }
    double checked(double v) noexcept
    {
        if (!std::isfinite(v))
            fail(ValueError::OutOfRange);
        return v;
    }

    double sum() noexcept
    {
        double v = product();
        while (err == ValueError::None) {
            spaces();
            if (eat("+"))
                v = checked(v + product());
            else if (eat("-") || eat(kMinus))
                v = checked(v - product());
            else
                break;
        }
        return v;
    }

    double product() noexcept
    {
        double v = unary();
        while (err == ValueError::None) {
            spaces();
            const bool mul = eat("*") || eat(kTimes);
            if (!mul && !eat("/") && !eat(kDivide))
                break;
            const double d = unary();
            if (!mul && d == 0) {
                fail(ValueError::DivideByZero);
                break;
            }
            const double r = mul ? v * d : v / d;
            // Tràn, hoặc tích/thương khác 0 mà nhỏ dưới 1e-307 (kể cả làm tròn về 0): cùng phạm vi với INT-105.
            if (!std::isfinite(r) || (v != 0 && d != 0 && std::fabs(r) < 1e-307))
                fail(ValueError::OutOfRange);
            v = r;
        }
        return v;
    }

    double unary() noexcept
    {
        bool neg = false;
        for (;;) {
            spaces();
            if (eat("-") || eat(kMinus))
                neg = !neg;
            else if (!eat("+"))
                break;
        }
        double v = 0;
        if (eat("(")) {
            if (++depth > kMaxDepth) {
                fail(ValueError::Paren);
                return 0;
            }
            v = sum();
            spaces();
            if (!eat(")"))
                fail(p == e ? ValueError::Paren : ValueError::Syntax);
            --depth;
        } else if (p < e && (isDigit(*p) || *p == '.')) {
            v = operand();
        } else {
            fail(ValueError::Syntax);
        }
        return neg ? -v : v;
    }

    // Độ dài: số có đơn vị (', ", mm, cm, m, km, in, ft) đọc như khi gõ không '=' (1/2" là nửa inch, 3'-6 1/2");
    // số trần là số thực INT-105, nên '/' là phép chia, '-' là phép trừ (5-1/2 là 4.5). Góc: số trần theo đơn vị ô.
    double operand() noexcept
    {
        const std::string_view s(p, std::size_t(e - p));
        Length l;
        if (spec.kind == ValueKind::Distance) {
            l = readLength(s, true);
            if (l.n && l.unit != kBare) {
                p += l.end;
                return checked(convert(l.n.value, l.unit, unit, err));
            }
        }
        const cmd::Parsed<double> n = cmd::parseReal(s, ParseMode::Prefix);
        if (!n) {
            fail(fromCmd(n.error));
            return 0;
        }
        if (spec.kind == ValueKind::Angle)
            return angle(s, n);
        // Feet-inch INT-105 báo lỗi (1e308' tràn khi nhân 12): giữ mã đó thay cho Syntax ở dấu ' ".
        if (!l.n && n.end < s.size() && (s[n.end] == '\'' || s[n.end] == '"'))
            fail(fromCmd(l.n.error));
        p += n.end;
        return n.value;
    }

    // Số trần theo đơn vị của ô; có hậu tố d, °, g, r (45d30'15", 50g, 1.2r) thì INT-105 đọc ra radian rồi đổi về
    // đơn vị ô. Phương vị (N45dE) không nhận trong biểu thức: luôn tuyệt đối, không cộng được với góc theo ANGBASE.
    double angle(std::string_view s, cmd::Parsed<double> n) noexcept
    {
        const char c = n.end < s.size() ? char(s[n.end] | 0x20) : 0;
        const bool degree = n.end + 1 < s.size() && s[n.end] == '\xC2' && s[n.end + 1] == '\xB0';
        if (c != 'd' && c != 'g' && c != 'r' && !degree) {
            p += n.end;
            return n.value;
        }
        UnitContext ctx = spec.units;
        ctx.useAngbase = false;
        const cmd::Parsed<double> a = cmd::parseAngle(s, ctx, ParseMode::Prefix);
        if (!a) {
            fail(fromCmd(a.error));
            return 0;
        }
        p += a.end;
        return a.value / angleUnit(spec);
    }
};

double expression(std::string_view s, const ValueSpec& spec, ValueError& err) noexcept
{
    Expr x{s.data(), s.data() + s.size(), spec};
    double v = x.sum();
    x.spaces();
    if (x.p != x.e)
        x.fail(*x.p == ')' ? ValueError::Paren : ValueError::Syntax);
    if (x.err == ValueError::None && spec.kind == ValueKind::Angle) {
        v *= angleUnit(spec);
        const UnitContext& c = spec.units;
        if (c.useAngbase) // như INT-105: hướng gõ theo ANGBASE/ANGDIR → góc tuyệt đối
            v = c.angdir ? c.angbase - v : c.angbase + v;
    }
    if (x.err == ValueError::None && spec.kind == ValueKind::Integer) {
        if (v != std::trunc(v))
            x.fail(ValueError::NotInteger);
        else if (v < -32768 || v > 32767)
            x.fail(ValueError::OutOfRange);
    }
    err = x.err;
    return v;
}

// Một độ dài theo INT-105 (feet-inch nhận ở mọi LUNITS) rồi [khoảng trắng] [hậu tố chữ]; quy về đơn vị bản vẽ.
double plainDistance(std::string_view s, const ValueSpec& spec, ValueError& err) noexcept
{
    const Length l = readLength(s, false);
    if (!l.n) {
        err = fromCmd(l.n.error);
        return 0;
    }
    if (l.end != s.size()) {
        err = ValueError::Syntax;
        return 0;
    }
    const int to = drawingUnit(spec);
    return convert(l.n.value, l.unit == kBare ? to : l.unit, to, err);
}

// Tỉ lệ: số, phân số, hỗn số (INT-105 parseDistance thập phân), hoặc tỉ số a:b.
double plainScale(std::string_view s, ValueError& err) noexcept
{
    const UnitContext c; // thập phân: ' và " là lỗi cú pháp
    const cmd::Parsed<double> a = cmd::parseDistance(s, c, ParseMode::Prefix);
    if (!a) {
        err = fromCmd(a.error);
        return 0;
    }
    const std::string_view rest = trimLeft(s.substr(a.end));
    if (rest.empty())
        return a.value;
    const cmd::Parsed<double> b = rest[0] == ':' ? cmd::parseDistance(rest.substr(1), c) : cmd::Parsed<double>{};
    if (rest[0] != ':' || b.error == ParseError::Empty)
        err = ValueError::Syntax;
    else if (!b)
        err = fromCmd(b.error);
    else if (b.value == 0)
        err = ValueError::DivideByZero;
    return err == ValueError::None ? a.value / b.value : 0;
}

template <class T>
double fromParsed(cmd::Parsed<T> r, ValueError& err) noexcept
{
    err = fromCmd(r.error);
    return double(r.value);
}

} // namespace

ValueResult parseValue(std::string_view text, const ValueSpec& spec) noexcept
{
    const std::string_view s = trim(text);
    if (s.empty())
        return {0, ValueError::Empty};
    if (decimalComma(s))
        return {0, ValueError::Comma};
    ValueError err = ValueError::None;
    double v = 0;
    if (s[0] == '=') {
        v = expression(s.substr(1), spec, err);
    } else {
        switch (spec.kind) {
        case ValueKind::Distance:
            v = plainDistance(s, spec, err);
            break;
        case ValueKind::Angle:
            v = fromParsed(cmd::parseAngle(s, spec.units), err);
            break;
        case ValueKind::Scale:
            v = plainScale(s, err);
            break;
        case ValueKind::Integer:
            v = fromParsed(cmd::parseInt(s), err);
            break;
        case ValueKind::Real:
            v = fromParsed(cmd::parseReal(s), err);
            break;
        }
    }
    // Cùng phạm vi với INT-105 (kết quả quy đổi, phép tính có thể ra ngoài): định dạng lại rồi đọc lại được.
    if (err == ValueError::None && (!std::isfinite(v) || (v != 0 && std::fabs(v) < 1e-307)))
        err = ValueError::OutOfRange;
    if (err != ValueError::None)
        return {0, err};
    return {v, ValueError::None};
}

std::size_t formatValue(double value, const ValueSpec& spec, std::span<char> out) noexcept
{
    if (spec.kind == ValueKind::Distance)
        return regen::formatLinear(value, spec.units, out);
    if (spec.kind == ValueKind::Angle)
        return regen::formatAngle(value, spec.units, out);
    UnitContext c = spec.units; // tỉ lệ, số không đơn vị: thập phân theo LUPREC (như bảng Properties)
    c.lunits = LinearUnits::Decimal;
    if (spec.kind == ValueKind::Integer)
        c.luprec = 0;
    return regen::formatLinear(value, c, out);
}

std::string_view errorCode(ValueError e) noexcept
{
    switch (e) {
    case ValueError::None:
        return "ok";
    case ValueError::Empty:
        return "empty";
    case ValueError::Syntax:
        return "syntax";
    case ValueError::Comma:
        return "comma";
    case ValueError::OutOfRange:
        return "range";
    case ValueError::NotInteger:
        return "not-integer";
    case ValueError::DivideByZero:
        return "zero-denominator";
    case ValueError::Paren:
        return "paren";
    case ValueError::Units:
        return "units";
    }
    return "unknown";
}

QString errorText(ValueError e, std::string_view text, const ValueSpec& spec)
{
    switch (e) {
    case ValueError::None:
        return {};
    case ValueError::Empty:
        return QCoreApplication::translate("ValueParser", "Nhập một giá trị.");
    case ValueError::Syntax:
        if (trim(text).starts_with('='))
            return QCoreApplication::translate(
                "ValueParser", "Biểu thức chưa đúng. Biểu thức gồm số, đơn vị, + − × ÷ và ngoặc, ví dụ =1200/2+50.");
        switch (spec.kind) {
        case ValueKind::Distance:
            return QCoreApplication::translate("ValueParser",
                                               "Chưa đọc được độ dài. Ví dụ: 1250, 12.5m, 250cm, 3'6\", =1200/2+50.");
        case ValueKind::Angle:
            return QCoreApplication::translate("ValueParser",
                                               "Chưa đọc được góc. Ví dụ: 45, 45d30', 50g, 0.785r, N45dE.");
        case ValueKind::Scale:
            return QCoreApplication::translate("ValueParser", "Chưa đọc được tỉ lệ. Ví dụ: 0.5, 1/4, 1:50.");
        case ValueKind::Integer:
            return QCoreApplication::translate("ValueParser", "Chưa đọc được số nguyên. Ví dụ: 12, -3.");
        case ValueKind::Real:
            break;
        }
        return QCoreApplication::translate("ValueParser", "Chưa đọc được số. Ví dụ: 12.5, -3, 1.5e3.");
    case ValueError::Comma: {
        // Gợi ý: ',' cạnh chữ số đổi thành '.'; chuỗi sửa vẫn sai (1,000.5) thì gợi ý mẫu 12.5.
        std::string fixed(trim(text));
        for (std::size_t i = 0; i < fixed.size(); ++i)
            if (fixed[i] == ',' &&
                ((i > 0 && isDigit(fixed[i - 1])) || (i + 1 < fixed.size() && isDigit(fixed[i + 1]))))
                fixed[i] = '.';
        if (!parseValue(fixed, spec))
            fixed = "12.5";
        return QCoreApplication::translate("ValueParser", "Dấu thập phân là dấu chấm: gõ %1")
            .arg(QString::fromUtf8(fixed.data(), qsizetype(fixed.size())));
    }
    case ValueError::OutOfRange:
        if (spec.kind == ValueKind::Integer)
            return QCoreApplication::translate("ValueParser", "Số nguyên phải từ -32768 đến 32767.");
        if (spec.kind == ValueKind::Angle)
            return QCoreApplication::translate(
                "ValueParser", "Góc vượt giới hạn: quá lớn, phút hoặc giây từ 60 trở lên, hoặc phương vị quá 90°.");
        return QCoreApplication::translate("ValueParser", "Giá trị quá lớn hoặc quá gần 0.");
    case ValueError::NotInteger:
        return QCoreApplication::translate("ValueParser", "Nhập số nguyên, ví dụ 12.");
    case ValueError::DivideByZero:
        return QCoreApplication::translate("ValueParser", "Không chia được cho 0.");
    case ValueError::Paren:
        return QCoreApplication::translate("ValueParser", "Ngoặc chưa khớp hoặc lồng quá 32 tầng.");
    case ValueError::Units:
        return QCoreApplication::translate(
            "ValueParser", "Bản vẽ chưa đặt đơn vị nên không quy đổi được đơn vị gõ kèm. Gõ số theo đơn vị bản vẽ.");
    }
    return {};
}

} // namespace trion::ui
