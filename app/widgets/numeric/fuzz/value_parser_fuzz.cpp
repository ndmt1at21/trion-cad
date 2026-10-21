// Fuzz UI0-109 (DoD-C 7): ba byte đầu chọn loại ô và LUNITS; AUNITS, ANGBASE/ANGDIR; INSUNITS (cả mã ngoài 0–24);
// phần còn lại là chuỗi gõ. Ngoài lỗi bộ nhớ, UB (ASan, UBSan), kiểm:
// - thành công: giá trị hữu hạn, 0 hoặc |x| ≥ 1e-307; lỗi: giá trị 0, mã lỗi biết; hai lần gọi cho cùng kết quả;
// - giá trị đọc được: hiển thị lại (formatValue, độ chính xác 8) rồi đọc lại lệch ≤ nửa đơn vị hiển thị;
// - ô số không đơn vị: "=" + chuỗi cho cùng giá trị từng bit với chuỗi (hai đường đọc chung INT-105);
// - lỗi comma: errorText (gợi ý sửa gọi lại parseValue) chạy được.
#include "app/widgets/numeric/ValueParser.h"
#include "core/regen/numfmt/NumFormat.h"

#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <numbers>

using namespace trion::ui;
using trion::regen::AngularUnits;
using trion::regen::LinearUnits;

namespace {

void check(bool ok)
{
    if (!ok)
        std::abort();
}

void reformat(double v, const ValueSpec& spec)
{
    constexpr double kTwoPi = 2 * std::numbers::pi, kRadPerDeg = std::numbers::pi / 180;
    if (spec.kind == ValueKind::Distance && std::fabs(v) > 1e300) // feet·12 sát DBL_MAX tràn khi đọc lại
        return;
    if (spec.kind == ValueKind::Angle && std::fabs(v) > 1e3) // v − ANGBASE lệch ½ ulp(v): giữ dưới biên 1e-12
        return;
    char b[trion::regen::kMaxNumText];
    const std::size_t n = formatValue(v, spec, b);
    check(n <= sizeof b);
    const ValueResult r = parseValue({b, n}, spec);
    const LinearUnits lu = spec.kind == ValueKind::Distance ? spec.units.lunits : LinearUnits::Decimal;
    switch (spec.kind) {
    case ValueKind::Angle: {
        const AngularUnits au = spec.units.aunits;
        const double half = au == AngularUnits::Grads     ? 0.5e-8 * std::numbers::pi / 200
                            : au == AngularUnits::Radians ? 0.5e-8
                            : au == AngularUnits::DegMinSec || au == AngularUnits::Surveyor ? 0.5 / 3600e4 * kRadPerDeg
                                                                                            : 0.5e-8 * kRadPerDeg;
        check(r && std::fabs(std::remainder(std::remainder(r.value, kTwoPi) - std::remainder(v, kTwoPi), kTwoPi)) <=
                       half + 1e-12);
        return;
    }
    case ValueKind::Integer:
        check(r && r.value == v);
        return;
    default: {
        const double half = lu == LinearUnits::Architectural || lu == LinearUnits::Fractional ? 0.5 / 256
                            : lu == LinearUnits::Scientific                                   ? 0.6e-8 * std::fabs(v)
                                                                                              : 0.5e-8;
        check(r && std::fabs(r.value - v) <= half + 4 * DBL_EPSILON * std::fabs(v));
    }
    }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    if (size < 3)
        return 0;
    ValueSpec spec;
    spec.kind = ValueKind(data[0] % 5);
    spec.units.lunits = LinearUnits(data[0] / 5 % 7); // 0 và 6: như thập phân
    spec.units.luprec = 8;
    spec.units.aunits = AngularUnits(data[1] % 6); // 5: như độ
    spec.units.auprec = 8;
    spec.units.useAngbase = (data[1] / 6) & 1;
    spec.units.angdir = (data[1] / 12) & 1;
    spec.units.angbase = spec.units.useAngbase ? 0.75 : 0;
    spec.insunits = data[2] % 26; // 25: ngoài khoảng, như không đơn vị
    const std::string_view s(reinterpret_cast<const char*>(data) + 3, size - 3);

    const ValueResult r = parseValue(s, spec);
    const ValueResult again = parseValue(s, spec);
    check(std::memcmp(&r.value, &again.value, sizeof r.value) == 0 && r.error == again.error);
    check(errorCode(r.error) != "unknown");
    if (!r) {
        check(r.value == 0);
        if (r.error == ValueError::Comma)
            check(!errorText(r.error, s, spec).isEmpty());
        return 0;
    }
    check(std::isfinite(r.value) && (r.value == 0 || std::fabs(r.value) >= 1e-307));
    reformat(r.value, spec);

    const std::size_t first = s.find_first_not_of(" \t");
    if (s.size() >= 256 || s[first] == '=')
        return 0;
    // Độ dài: số có đơn vị (' " hay chữ khác e của số mũ) đọc trong biểu thức như khi không '='; số trần thì chỉ khi
    // không có phân số ('/' là phép chia, '-' là phép trừ). Khoảng trắng sau ' cắt feet-inch trong biểu thức.
    bool tagged = false, quoteSpace = false;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = char(s[i] | 0x20);
        tagged |= s[i] == '\'' || s[i] == '"' || (c >= 'a' && c <= 'z' && c != 'e');
        quoteSpace |= s[i] == '\'' && i + 1 < s.size() && (s[i + 1] == ' ' || s[i + 1] == '\t');
    }
    const bool distance =
        spec.kind == ValueKind::Distance && !quoteSpace && (tagged || s.find('/') == std::string_view::npos);
    if (spec.kind == ValueKind::Real || distance) {
        char b[257] = {'='};
        std::memcpy(b + 1, s.data(), s.size());
        const ValueResult e = parseValue({b, s.size() + 1}, spec);
        check(e && (distance ? e.value == r.value : std::memcmp(&e.value, &r.value, sizeof r.value) == 0));
    }
    return 0;
}
