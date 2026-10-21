// Fuzz INT-105 (DoD-C 7): byte đầu chọn LUNITS, AUNITS, ANGBASE/ANGDIR; phần còn lại vào cả bốn hàm, hai chế độ.
// Ngoài lỗi bộ nhớ, UB (ASan, UBSan), kiểm:
// - thành công: giá trị hữu hạn, Whole đọc hết chuỗi, Prefix đọc 1…độ dài; lỗi: end = 0;
// - Prefix đọc n byte thì Whole của n byte đầu cho cùng giá trị từng bit (hai chế độ cùng một ngữ pháp);
// - độ dài, góc đọc được: định dạng lại (RGN-013, độ chính xác 8) rồi đọc lại lệch ≤ nửa đơn vị.
#include "cmd/parse/ValueParser.h"
#include "core/regen/numfmt/NumFormat.h"

#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <numbers>

using namespace trion::cmd;
using trion::regen::AngularUnits;
using trion::regen::LinearUnits;
using trion::regen::UnitContext;

namespace {

void check(bool ok)
{
    if (!ok)
        std::abort();
}

template <class T, class F>
void modes(std::string_view s, F parse)
{
    const Parsed<T> w = parse(s, ParseMode::Whole);
    check(w ? std::isfinite(double(w.value)) && w.end == s.size() : w.end == 0);
    const Parsed<T> p = parse(s, ParseMode::Prefix);
    check(p ? std::isfinite(double(p.value)) && p.end > 0 && p.end <= s.size() : p.end == 0);
    if (p) {
        const Parsed<T> again = parse(s.substr(0, p.end), ParseMode::Whole);
        check(again && std::memcmp(&again.value, &p.value, sizeof(T)) == 0);
    }
}

void reformatDistance(double v, UnitContext c)
{
    if (std::fabs(v) > 1e300) // feet·12 sát DBL_MAX có thể tràn khi đọc lại
        return;
    c.luprec = 8;
    char b[trion::regen::kMaxNumText];
    const std::string_view s(b, trion::regen::formatLinear(v, c, b));
    const Parsed<double> r = parseDistance(s, c);
    const double half = c.lunits == LinearUnits::Architectural || c.lunits == LinearUnits::Fractional ? 0.5 / 256
                        : c.lunits == LinearUnits::Scientific ? 0.6e-8 * std::fabs(v)
                                                              : 0.5e-8;
    check(r && std::fabs(r.value - v) <= half + 4 * DBL_EPSILON * std::fabs(v));
}

void reformatAngle(double v, UnitContext c)
{
    if (std::fabs(v) > 1e3) // v − ANGBASE lệch ½ ulp(v): giữ dưới biên 1e-12
        return;
    constexpr double kTwoPi = 2 * std::numbers::pi, kRadPerDeg = std::numbers::pi / 180;
    c.auprec = 8;
    char b[trion::regen::kMaxNumText];
    const std::string_view s(b, trion::regen::formatAngle(v, c, b));
    const Parsed<double> r = parseAngle(s, c);
    const double half = c.aunits == AngularUnits::Grads     ? 0.5e-8 * std::numbers::pi / 200
                        : c.aunits == AngularUnits::Radians ? 0.5e-8
                        : c.aunits == AngularUnits::DegMinSec || c.aunits == AngularUnits::Surveyor
                            ? 0.5 / 3600e4 * kRadPerDeg
                            : 0.5e-8 * kRadPerDeg;
    check(r && std::fabs(std::remainder(std::remainder(r.value, kTwoPi) - std::remainder(v, kTwoPi), kTwoPi)) <=
                   half + 1e-12);
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    if (size == 0)
        return 0;
    const unsigned k = data[0];
    UnitContext c;
    c.lunits = LinearUnits(k % 7);      // 0 và 6: như thập phân
    c.aunits = AngularUnits(k / 7 % 6); // 5: như độ
    c.useAngbase = (k / 42) & 1;
    c.angdir = (k / 84) & 1;
    c.angbase = c.useAngbase ? 0.75 : 0;
    const std::string_view s(reinterpret_cast<const char*>(data) + 1, size - 1);

    modes<double>(s, [](std::string_view t, ParseMode m) { return parseReal(t, m); });
    modes<int>(s, [](std::string_view t, ParseMode m) { return parseInt(t, m); });
    modes<double>(s, [&c](std::string_view t, ParseMode m) { return parseDistance(t, c, m); });
    modes<double>(s, [&c](std::string_view t, ParseMode m) { return parseAngle(t, c, m); });
    if (const Parsed<double> d = parseDistance(s, c))
        reformatDistance(d.value, c);
    if (const Parsed<double> a = parseAngle(s, c))
        reformatAngle(a.value, c);
    return 0;
}
