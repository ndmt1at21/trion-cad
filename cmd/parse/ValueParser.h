#pragma once
// Chuỗi nhập → số (INT-105): parser duy nhất cho prompt dòng lệnh, ô số (UI0-109), tọa độ (INT-004), 'CAL,
// QUICKCALC, distof/angtof (LSP). Chiều số → chuỗi là core/regen/numfmt (RGN-013); parse(format(x)) lệch ≤ nửa đơn
// vị hiển thị.
//
// - Dấu thập phân luôn là '.', không nhóm nghìn; có ',' cạnh chữ số thì trả CommaAsDecimal để giao diện nhắc (07.1 §8).
// - Không locale (std::from_chars và tự đọc), không cấp phát, không trạng thái toàn cục, noexcept, an toàn đa luồng.
// - Cú pháp từng chế độ, mã lỗi, chỗ khác AutoCAD: docs/specs/input-values.md.

#include "core/regen/numfmt/UnitContext.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace trion::cmd {

enum class ParseError : std::uint8_t {
    None,
    Empty,          // chuỗi rỗng hoặc chỉ có khoảng trắng
    Syntax,         // sai cú pháp hoặc thừa ký tự
    CommaAsDecimal, // có ',' cạnh chữ số ("1,5"): dấu thập phân là '.' (chỉ ở ParseMode::Whole)
    // Tràn double (bậc thập phân ngoài −307…308), int ngoài −32768…32767, phút/giây ≥ 60, phương vị khảo sát > 90°.
    OutOfRange,
    NotInteger,         // parseInt: số thực không nguyên ("1.5", "2.0", "1e3")
    FeetInchNotAllowed, // dấu ' hoặc " khi LUNITS không phải kỹ thuật (3) hay kiến trúc (4)
    ZeroDenominator,    // phân số mẫu 0 ("1/0")
};

// Mã ổn định cho máy đọc (trion::Error::code, log, test): "syntax", "comma", "range"…
std::string_view errorCode(ParseError e) noexcept;

enum class ParseMode : std::uint8_t {
    Whole,  // cả chuỗi là một giá trị; bỏ khoảng trắng (' ', '\t') hai đầu
    Prefix, // đọc giá trị dài nhất ở đầu chuỗi (bỏ khoảng trắng đầu), dừng trước ký tự không thuộc giá trị
            // (',', '<', toán tử…): cho bộ tách từ của biểu thức, tọa độ. Không trả CommaAsDecimal.
};

template <class T>
struct Parsed {
    T value{};
    ParseError error = ParseError::None;
    std::size_t end = 0; // thành công: số byte đã đọc (Whole: cả chuỗi); lỗi: 0
    explicit operator bool() const noexcept { return error == ParseError::None; }
};

// Số thực như getreal: [+|-] 12 | 12.5 | .5 | 5. | 1.5E+01. Không nhận phân số, feet-inch.
Parsed<double> parseReal(std::string_view s, ParseMode mode = ParseMode::Whole) noexcept;

// Số nguyên như getint: [+|-] chữ số, −32768…32767.
Parsed<int> parseInt(std::string_view s, ParseMode mode = ParseMode::Whole) noexcept;

// Khoảng cách (đơn vị bản vẽ; feet-inch: 1 đơn vị = 1 inch) như getdist/distof. Mọi LUNITS nhận số thực, khoa học,
// phân số (1/2, 5-1/2, 5 1/2); chỉ ctx.lunits kỹ thuật (3), kiến trúc (4) nhận feet-inch (1'6", 1'-6 1/2",
// 1'6-1/2"), cả hai dạng UNITMODE. Chỉ dùng ctx.lunits.
Parsed<double> parseDistance(std::string_view s, const regen::UnitContext& ctx,
                             ParseMode mode = ParseMode::Whole) noexcept;

// Góc → radian, như getangle/angtof. Số không hậu tố theo ctx.aunits (độ với 0, 1, 4; grad 2; radian 3); có hậu tố
// thì theo hậu tố, mọi AUNITS: 45d30'15", 45°30', 50g, 0.7854r, phương vị N45d30'E | N 45d30' E | N, E, S, W.
// ctx.useAngbase: chuỗi là hướng gõ theo ANGBASE/ANGDIR (prompt điểm, hướng) → trả góc tuyệt đối (từ trục X,
// ngược chiều kim đồng hồ) = angbase ± t; phương vị luôn tuyệt đối. getangle (ANGDIR có, ANGBASE không) = kết quả
// − angbase. Không quy về [0, 2π): giữ dấu và độ lớn (góc chắn cung, góc quay); angtof tự quy.
Parsed<double> parseAngle(std::string_view s, const regen::UnitContext& ctx,
                          ParseMode mode = ParseMode::Whole) noexcept;

} // namespace trion::cmd
