#pragma once
// Định dạng số → chuỗi dùng chung (RGN-013, README task §4.2): kích thước (RGN-014), rtos/angtos (LSP-011),
// DYN, tọa độ thanh trạng thái, Properties. Không nơi nào khác tự định dạng số bản vẽ.
// Phân tích chuỗi → số là việc của INT-105 (kiểm vòng parse(format(x)) dựa trên các hàm này).
//
// - Làm tròn nửa xa số 0 trên giá trị nhị phân chính xác của double, không bù sai số, không printf/locale:
//   cùng đầu vào cho cùng chuỗi trên mọi nền tảng (đặc tả: docs/specs/regen/number-format.md).
// - Hàm định dạng ghi vào bộ đệm của người gọi, không cấp phát, không trạng thái toàn cục, an toàn đa luồng.
// - Chuỗi ra là UTF-8 (chỉ dấu thập phân/dấu nhóm do người gọi đặt có thể ngoài ASCII), không có '\0'.

#include "core/regen/numfmt/UnitContext.h"

#include <cstddef>
#include <span>
#include <string>

namespace trion::regen {

// Bộ đệm cỡ này luôn đủ cho mọi kết quả (kể cả |x| ≈ 1e308 có dấu nhóm 4 byte). Giá trị bản vẽ thực tế < 64 byte.
inline constexpr std::size_t kMaxNumText = 768;

// Định dạng độ dài `value` (đơn vị bản vẽ; feet-inch coi 1 đơn vị = 1 inch) theo ctx.lunits, luprec, dimzin,
// dimrnd, dimdsep, dimfrac, unitmode, form. Trả số byte của kết quả đầy đủ; nếu lớn hơn out.size() thì out chỉ
// giữ phần đầu (như snprintf). NaN/vô cực ra "nan", "inf", "-inf".
std::size_t formatLinear(double value, const UnitContext& ctx, std::span<char> out) noexcept;

// Định dạng góc `radians` theo ctx.aunits, auprec, dimazin, dimdsep, unitmode, form; quy về [0, 2π) như angtos.
// ctx.useAngbase: coi là góc hướng, đổi sang hệ ANGBASE/ANGDIR trước (không áp cho Surveyor, vốn theo phương Bắc).
// Trả số byte như formatLinear.
std::size_t formatAngle(double radians, const UnitContext& ctx, std::span<char> out) noexcept;

// Tiện dụng cho chỗ không nóng (có cấp phát). UI: QString::fromUtf8(s.data(), s.size()).
std::string linearString(double value, const UnitContext& ctx);
std::string angleString(double radians, const UnitContext& ctx);

} // namespace trion::regen
