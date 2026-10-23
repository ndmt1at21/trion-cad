#pragma once
// UIC-017 · Màu của bộ chọn màu UI-C06 (ColorPickerDialog): ByLayer, ByBlock, ACI 1–255, true color, màu Color Book
// (tên + RGB lưu kèm); chữ hiển thị, đọc chữ người dùng gõ ở ô "Màu", đổi RGB ⇄ HSL ⇄ mã hex của tab True Color.
//
//   Color::aci(140).label()            → "Màu 140"; Color::aci(1).label() → "Đỏ" (theo ngôn ngữ giao diện)
//   parseColor(u"xanh la")             → Color::aci(3); parseColor(u"#3cc8c0") → Color::rgb({0x3C, 0xC8, 0xC0})
//   toRgb(toHsl(c)) == c               (HSL số thực: đổi qua lại không trôi)
//
// RGB của ACI lấy từ bảng RGN-002 (core/regen/props/AciPalette.h), không có bảng thứ hai.
// ponytail: kiểu riêng của bộ chọn; màu của đối tượng trong bản vẽ là kiểu của lõi DOC (DOC-004) khi có — bộ chọn đổi
// sang kiểu đó (hoặc thêm hàm chuyển), giữ dạng API.

#include "core/regen/props/AciPalette.h"

#include <QString>
#include <QStringView>

#include <cstdint>
#include <optional>

namespace trion::ui {

class Color {
public:
    enum class Kind : std::uint8_t { ByLayer, ByBlock, Aci, True, Book };

    Color() = default; // ByLayer
    static Color byLayer() { return {}; }
    static Color byBlock();
    static Color aci(int index); // 1–255 (TRION_CHECK)
    static Color rgb(regen::Rgb c);
    // Màu của một Color Book: tên sổ ("RAL CLASSIC"), tên màu ("RAL 3020"), RGB bản vẽ lưu kèm.
    static Color book(const QString& book, const QString& name, regen::Rgb c);

    Kind kind() const { return kind_; }
    bool hasRgb() const { return kind_ >= Kind::Aci; } // ACI, true color, Color Book: có màu riêng
    int index() const { return index_; }               // ACI 1–255; loại khác: 0
    regen::Rgb rgb() const;                            // ACI theo bảng RGN-002; ByLayer, ByBlock: {0, 0, 0}
    QString bookName() const { return book_; }
    QString colorName() const { return name_; }

    // Chữ của màu (ô "Màu", nút, danh sách) theo ngôn ngữ giao diện: "ByLayer", "ByBlock", ACI 1–7 tên ("Đỏ"),
    // 8–255 "Màu 140", true color "#3CC8C0", Color Book tên màu ("RAL 3020"). parseColor đọc lại được (trừ Color Book).
    QString label() const;

    friend bool operator==(const Color&, const Color&) = default;

private:
    Kind kind_ = Kind::ByLayer;
    int index_ = 0;
    regen::Rgb rgb_{};
    QString book_, name_;
};

// "#RRGGBB", chữ hoa.
QString hexName(regen::Rgb c);
// "#RRGGBB" hoặc "RRGGBB", không phân biệt hoa thường, bỏ khoảng trắng hai đầu. Sai: nullopt.
std::optional<regen::Rgb> parseHex(QStringView text);

// Chữ gõ ở ô "Màu" (07.2 UI-C06; prototype #col-in): số 1–255 (cả "Màu 140", "color 140"); tên ACI 1–7 tiếng Việt
// hoặc English, không phân biệt dấu, hoa thường, khoảng trắng ("đỏ", "Do", "RED", "xanh lá", "XanhDuong") và tên đang
// hiện ở ngôn ngữ giao diện; ByLayer, ByBlock; #RRGGBB hoặc RRGGBB. Sai (0, 256, chữ lạ): nullopt.
std::optional<Color> parseColor(QStringView text);

// HSL của tab True Color: h 0–360 (độ), s, l 0–1. Số thực, nên đổi rgb → hsl → rgb trả đúng rgb.
struct Hsl {
    double h = 0, s = 0, l = 0;
};
Hsl toHsl(regen::Rgb c);        // xám: h = s = 0
regen::Rgb toRgb(const Hsl& c); // h lấy theo modulo 360, s, l kẹp 0–1; mỗi kênh làm tròn gần nhất

} // namespace trion::ui
