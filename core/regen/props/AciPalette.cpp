#include "core/regen/props/AciPalette.h"

#include <array>

namespace trion::regen {

namespace {

// Một kênh của dải 10–249 (xem AciPalette.h), số nguyên: V tính theo nửa đơn vị (v2 = 2V) để 127,5 và 76,5 chính xác.
// q = 0…4: phần "đi xuống" của kênh theo sắc độ (bước 15° = 1/4 cung 60°), như max(0, min(k, 4 − k, 1)) × 4 của
// công thức HSV k = (n + H/60) mod 6.
constexpr std::uint8_t channel(int v2, bool half, int n, int hue)
{
    const int k4 = (4 * n + hue) % 24; // k × 4
    int q = k4 < 16 - k4 ? k4 : 16 - k4;
    q = q < 0 ? 0 : q > 4 ? 4 : q;
    // bão hòa 1: V × (4 − q) / 4 = v2 × (4 − q) / 8; bão hòa 0,5: V × (8 − q) / 8 = v2 × (8 − q) / 16;
    // chia nguyên = floor
    return std::uint8_t(half ? v2 * (8 - q) / 16 : v2 * (4 - q) / 8);
}

constexpr std::array<Rgb, 256> makeTable()
{
    std::array<Rgb, 256> t{};
    constexpr Rgb kStandard[] = {{255, 0, 0},   {255, 255, 0},   {0, 255, 0},     {0, 255, 255},  {0, 0, 255},
                                 {255, 0, 255}, {255, 255, 255}, {128, 128, 128}, {192, 192, 192}};
    for (int i = 1; i <= 9; ++i)
        t[std::size_t(i)] = kStandard[i - 1];
    constexpr int kValue2[] = {510, 408, 306, 255, 153}; // 2V: 255, 204, 153, 127,5, 76,5
    for (int i = 10; i <= 249; ++i) {
        const int hue = i / 10 - 1, digit = i % 10, v2 = kValue2[digit / 2];
        const bool half = digit % 2 != 0;
        t[std::size_t(i)] = {channel(v2, half, 5, hue), channel(v2, half, 3, hue), channel(v2, half, 1, hue)};
    }
    constexpr std::uint8_t kGray[] = {51, 80, 105, 130, 190, 255};
    for (int i = 250; i <= 255; ++i) {
        const std::uint8_t g = kGray[i - 250];
        t[std::size_t(i)] = {g, g, g};
    }
    t[0] = t[7]; // ngoài khoảng: màu nét mặc định
    return t;
}

constexpr std::array<Rgb, 256> kTable = makeTable();

// Các ô đối chiếu của bảng công khai (bộ đầy đủ 255 ô: core/regen/tests/props_aci_test.cpp).
static_assert(kTable[8] == Rgb{128, 128, 128} && kTable[9] == Rgb{192, 192, 192});
static_assert(kTable[10] == Rgb{255, 0, 0} && kTable[11] == Rgb{255, 127, 127} && kTable[12] == Rgb{204, 0, 0});
static_assert(kTable[30] == Rgb{255, 127, 0} && kTable[140] == Rgb{0, 191, 255} && kTable[249] == Rgb{76, 38, 47});
static_assert(kTable[250] == Rgb{51, 51, 51} && kTable[254] == Rgb{190, 190, 190} && kTable[255] == Rgb{255, 255, 255});

constexpr std::string_view kNamesVi[] = {"đỏ", "vàng", "xanh lá", "xanh lơ", "xanh dương", "tím", "trắng"};
constexpr std::string_view kNamesEn[] = {"red", "yellow", "green", "cyan", "blue", "magenta", "white"};

} // namespace

Rgb aciRgb(int aci) noexcept
{
    return aci >= kAciFirst && aci <= kAciLast ? kTable[std::size_t(aci)] : kTable[0];
}

std::string_view aciNameVi(int aci) noexcept
{
    return aci >= 1 && aci <= 7 ? kNamesVi[aci - 1] : std::string_view();
}

std::string_view aciNameEn(int aci) noexcept
{
    return aci >= 1 && aci <= 7 ? kNamesEn[aci - 1] : std::string_view();
}

} // namespace trion::regen
