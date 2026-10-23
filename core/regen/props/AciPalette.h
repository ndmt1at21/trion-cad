#pragma once
// RGN-002 (tập con cho UIC-017) · Bảng màu chỉ số AutoCAD (ACI) 1–255 → RGB và tên chuẩn của 7 màu đầu.
//
// Nguồn: bảng công khai "AutoCAD Color Index RGB equivalents" (AutoCAD 2000 trở đi):
//   1–7      đỏ, vàng, xanh lá, xanh lơ, xanh dương, tím, trắng (255 hoặc 0 mỗi kênh)
//   8, 9     128,128,128 và 192,192,192
//   10–249   24 sắc độ cách 15° (hàng chục: (i/10 − 1) × 15°) × 10 sắc thái (chữ số cuối): HSV với độ sáng
//            V = 255, 204, 153, 127,5, 76,5 cho cặp 0-1, 2-3, 4-5, 6-7, 8-9; bão hòa 1 (chữ số chẵn) hoặc 0,5 (lẻ);
//            mỗi kênh lấy phần nguyên (floor) như bảng: 11 = 255,127,127; 30 = 255,127,0; 44 = 153,114,0
//   250–255  xám 51, 80, 105, 130, 190, 255
// Bảng tính bằng số nguyên lúc biên dịch (không số thực): cùng kết quả từng bit trên mọi nền tảng (DoD-C 4).
// Khác prototype (design/prototype/js/canvas.js TC.ACI): prototype làm tròn (Math.round) nên lệch đúng 1 đơn vị ở 180
// ô của dải 10–249 (11 = 255,128,128); bảng ở đây theo bảng công khai.
//
// ACI 7 trong bảng là trắng; khi vẽ, RGN-002 (DisplayProps) gắn cờ đảo theo nền (đen trên nền sáng).
// ponytail: tập con RGN-002 cho bộ chọn màu (UIC-017): chỉ bảng và tên. DisplayProps.h, resolveMaterial(), cờ ACI 7,
// memo theo (layer, chuỗi insert) do RGN-002 thêm vào core/regen/props.

#include <cstdint>
#include <string_view>

namespace trion::regen {

struct Rgb {
    std::uint8_t r = 0, g = 0, b = 0;
    friend constexpr bool operator==(Rgb, Rgb) = default;
};

inline constexpr int kAciFirst = 1;  // 0 là ByBlock
inline constexpr int kAciLast = 255; // 256 là ByLayer

// RGB của chỉ số ACI 1–255. Ngoài khoảng (ByBlock 0, ByLayer 256 chưa phân giải, giá trị hỏng đọc từ tệp): màu của
// ACI 7 (màu nét mặc định), không lỗi, không đọc ngoài bảng.
Rgb aciRgb(int aci) noexcept;

// Tên chuẩn của ACI 1–7, chữ thường UTF-8 — tiếng Việt: "đỏ", "vàng", "xanh lá", "xanh lơ", "xanh dương", "tím",
// "trắng"; English: "red", "yellow", "green", "cyan", "blue", "magenta", "white". Chỉ số khác: chuỗi rỗng.
// Dùng để đọc tên người dùng gõ (ô Màu của UIC-017, lệnh COLOR) ở cả hai ngôn ngữ; chữ hiển thị trên giao diện dịch
// qua tr() ở tầng UI.
std::string_view aciNameVi(int aci) noexcept;
std::string_view aciNameEn(int aci) noexcept;

} // namespace trion::regen
