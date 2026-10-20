// Định dạng góc: AUNITS/DIMAUNIT 0–4 (đặc tả: docs/specs/regen/number-format.md §4).
#include "core/regen/numfmt/Digits.h"
#include "core/regen/numfmt/NumFormat.h"

#include <algorithm>
#include <numbers>

namespace trion::regen {
namespace {

using detail::Writer;

constexpr double kTwoPi = 2 * std::numbers::pi;
constexpr double kDegPerRad = 180 / std::numbers::pi;
constexpr double kGradPerRad = 200 / std::numbers::pi;

// Đơn vị nhỏ nhất của độ/phút/giây theo AUPREC: 0 độ, 1–2 phút, 3–4 giây, 5–8 giây có 1–4 số lẻ.
std::int64_t dmsUnitsPerDegree(int p) noexcept
{
    if (p == 0)
        return 1;
    if (p <= 2)
        return 60;
    std::int64_t u = 3600;
    for (int i = 4; i < p; ++i)
        u *= 10;
    return u;
}

// n ≥ 0 đơn vị (đã làm tròn) → 45d30'15.5". Phút, giây không thêm số 0 đầu (angtos: 180d0'0").
void putDms(Writer& o, std::int64_t n, int p, const UnitContext& c) noexcept
{
    const std::string_view deg = c.form == TextForm::Mtext ? "%%d" : "d";
    if (p == 0) {
        o.putUInt(std::uint64_t(n));
        o.put(deg);
        return;
    }
    if (p <= 2) {
        o.putUInt(std::uint64_t(n / 60));
        o.put(deg);
        o.putUInt(std::uint64_t(n % 60));
        o.put('\'');
        return;
    }
    const std::int64_t q = dmsUnitsPerDegree(p) / 3600; // số đơn vị trong một giây
    const std::int64_t deg0 = n / (3600 * q);
    const auto rest = std::uint32_t(n - deg0 * 3600 * q); // < 3,6·10^7: các phép chia sau dùng 32 bit
    const auto q32 = std::uint32_t(q), min = rest / (60 * q32), sec = rest - min * 60 * q32;
    o.putUInt(std::uint64_t(deg0));
    o.put(deg);
    o.putUInt(min);
    o.put('\'');
    o.putUInt(sec / q32);
    if (p > 4) { // phần lẻ của giây, đủ p − 4 chữ số; DIMAZIN 2 bỏ số 0 cuối
        char f[4];
        std::uint32_t r = sec % q32;
        for (int i = p - 5; i >= 0; --i, r /= 10)
            f[i] = char('0' + r % 10);
        std::string_view ip, fp(f, std::size_t(p - 4));
        detail::suppressZeros(ip, fp, false, c.dimazin & 2);
        detail::putDigits(o, {}, fp, c.dimdsep);
    }
    o.put('"');
}

// Góc khảo sát từ n đơn vị tính từ hướng Đông, ngược chiều kim đồng hồ: N 45d0' E; đúng trục: E, N, W, S.
void putSurveyor(Writer& o, std::int64_t n, std::int64_t unitsPerDeg, int p, const UnitContext& c) noexcept
{
    const std::int64_t q = 90 * unitsPerDeg;
    if (n >= 4 * q) // làm tròn có thể chạm đúng 360°
        n -= 4 * q;
    const int quad = n < q ? 0 : n < 2 * q ? 1 : n < 3 * q ? 2 : 3;
    if (n == quad * q) {
        o.put("ENWS"[quad]);
        return;
    }
    const std::int64_t b = quad == 0 ? q - n : quad == 1 ? n - q : quad == 2 ? 3 * q - n : n - 3 * q;
    o.put(quad < 2 ? 'N' : 'S');
    if (!c.unitmode)
        o.put(' ');
    putDms(o, b, p, c);
    if (!c.unitmode)
        o.put(' ');
    o.put(quad == 0 || quad == 3 ? 'E' : 'W');
}

} // namespace

std::size_t formatAngle(double rad, const UnitContext& c, std::span<char> out) noexcept
{
    Writer o{out.data(), out.size()};
    double t = rad;
    if (c.useAngbase && c.aunits != AngularUnits::Surveyor)
        t = c.angdir ? c.angbase - t : t - c.angbase;
    if (!std::isfinite(t)) {
        detail::putNonFinite(o, t);
        return o.n;
    }
    t = std::fmod(t, kTwoPi); // chính xác
    if (t < 0)
        t += kTwoPi;
    if (t >= kTwoPi) // −ε + 2π có thể làm tròn thành 2π
        t = 0;
    const int p = std::clamp(c.auprec, 0, 8);
    const bool lead = c.dimazin & 1, trail = c.dimazin & 2;
    switch (c.aunits) {
    case AngularUnits::DegMinSec:
    case AngularUnits::Surveyor: {
        const std::int64_t u = dmsUnitsPerDegree(p);
        const auto n = std::int64_t(std::round(t * kDegPerRad * double(u)));
        if (c.aunits == AngularUnits::Surveyor)
            putSurveyor(o, n, u, p, c);
        else
            putDms(o, n, p, c);
        break;
    }
    case AngularUnits::Grads:
        detail::putFixed(o, false, t * kGradPerRad, p, lead, trail, c.dimdsep, 0);
        o.put('g');
        break;
    case AngularUnits::Radians:
        detail::putFixed(o, false, t, p, lead, trail, c.dimdsep, 0);
        o.put('r');
        break;
    default: // Degrees; giá trị ngoài 0–4 coi như độ thập phân
        detail::putFixed(o, false, t * kDegPerRad, p, lead, trail, c.dimdsep, 0);
        if (c.form == TextForm::Mtext)
            o.put("%%d");
        break;
    }
    return o.n;
}

std::string angleString(double radians, const UnitContext& ctx)
{
    char b[kMaxNumText];
    return std::string(b, std::min(formatAngle(radians, ctx, b), sizeof b));
}

} // namespace trion::regen
