#pragma once
// Bộ phân tích giá trị của ô số UI-C09 (UI0-109): chuỗi gõ → số theo loại ô; đơn vị gõ kèm (12.5m, 250cm, 3'6")
// quy về đơn vị bản vẽ; biểu thức bắt đầu bằng '=' (=1200/2+50, có ngoặc); số → chuỗi hiển thị lại qua RGN-013.
// Hàm thuần, không widget. Mọi số đọc qua INT-105 (cmd/parse), không có bộ đọc số thứ hai.
//
// - Dấu thập phân luôn là '.'; ',' cạnh chữ số trả ValueError::Comma, errorText gợi ý chuỗi đã sửa ("12.5").
// - parseValue, formatValue: không cấp phát, không locale, không ném ngoại lệ, an toàn đa luồng.
// - Cú pháp (phân số và phép chia, feet-inch trong biểu thức), mã lỗi: docs/specs/input-values.md §10.

#include "core/regen/numfmt/UnitContext.h"

#include <QString>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace trion::ui {

enum class ValueKind : std::uint8_t {
    Distance, // độ dài theo đơn vị bản vẽ (LUNITS 3, 4: 1 đơn vị = 1 inch)
    Angle,    // góc, radian (số không hậu tố theo AUNITS)
    Scale,    // hệ số tỉ lệ: số, phân số 1/4, tỉ số 1:50
    Integer,  // số nguyên −32768…32767
    Real,     // số không đơn vị
};

// Thiết lập của một ô số. UI0-007 giữ một ValueSpec cho mỗi ô, điền từ biến header của bản vẽ.
struct ValueSpec {
    ValueKind kind = ValueKind::Real;
    // INT-105 đọc lunits (feet-inch), aunits, angbase, angdir, useAngbase; RGN-013 định dạng theo cả struct.
    regen::UnitContext units;
    // INSUNITS 0–24: đơn vị bản vẽ, đích quy đổi của hậu tố mm, cm, m, km, in, ft, ' và ". 0 hoặc ngoài khoảng:
    // không đơn vị (hậu tố là lỗi Units). LUNITS 3, 4 luôn coi 1 đơn vị = 1 inch như RGN-013, bỏ qua INSUNITS.
    int insunits = 0;
};

enum class ValueError : std::uint8_t {
    None,
    Empty,      // "empty": rỗng hoặc chỉ khoảng trắng
    Syntax,     // "syntax": sai cú pháp, thừa ký tự
    Comma,      // "comma": ',' cạnh chữ số
    OutOfRange, // "range": tràn double, |x| < 1e-307 khác 0, số nguyên ngoài −32768…32767, phút/giây ≥ 60…
    NotInteger,   // "not-integer": ô số nguyên nhận số lẻ
    DivideByZero, // "zero-denominator": phân số mẫu 0, chia cho 0, tỉ số x:0
    Paren,        // "paren": ngoặc lệch hoặc lồng quá 32 tầng
    Units,        // "units": gõ kèm đơn vị khi bản vẽ không có đơn vị (INSUNITS 0)
};

struct ValueResult {
    double value = 0; // Distance: đơn vị bản vẽ; Angle: radian (như INT-105 parseAngle); còn lại: số. Lỗi: 0
    ValueError error = ValueError::None;
    explicit operator bool() const noexcept { return error == ValueError::None; }
};

// Chuỗi người dùng gõ → giá trị. Không '=': một giá trị theo INT-105 (Distance thêm hậu tố đơn vị và feet-inch ở mọi
// LUNITS, Scale thêm tỉ số a:b). Có '=' ở đầu: biểu thức + − × ÷ (* /), ngoặc; số có đơn vị đọc như khi không '='
// (=1/2"+3' là nửa inch cộng 3 feet), số trần là số thực nên '/' là phép chia, '-' là phép trừ (=5-1/2 là 4.5).
ValueResult parseValue(std::string_view text, const ValueSpec& spec) noexcept;

// Chuỗi hiển thị lại (RGN-013): Distance formatLinear, Angle formatAngle, Scale/Real thập phân theo LUPREC, Integer
// không phần lẻ. Trả số byte như regen::formatLinear; bộ đệm regen::kMaxNumText luôn đủ.
std::size_t formatValue(double value, const ValueSpec& spec, std::span<char> out) noexcept;

// Mã ổn định cho log, test, trion::Error::code: "ok", "empty", "syntax", "comma", "range"…
std::string_view errorCode(ValueError e) noexcept;

// Câu giải thích (qua tr) hiện dưới ô khi lỗi; `text` là chuỗi đã gõ (Comma: gợi ý chuỗi sửa ',' → '.').
QString errorText(ValueError e, std::string_view text, const ValueSpec& spec);

} // namespace trion::ui
