#include "app/dialogs/pickers/Color.h"

#include "core/base/assert.h"
#include "core/base/text/Fold.h"

#include <QCoreApplication>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace trion::ui {

namespace {

// Chữ hiện (prototype TC.aciName, SC.colorLabel), dịch lúc hiện.
constexpr const char* kByLayer = QT_TRANSLATE_NOOP("trion::ui::Color", "ByLayer");
constexpr const char* kByBlock = QT_TRANSLATE_NOOP("trion::ui::Color", "ByBlock");
constexpr const char* kIndexed = QT_TRANSLATE_NOOP("trion::ui::Color", "Màu %1");
constexpr const char* kNames[] = {
    QT_TRANSLATE_NOOP("trion::ui::Color", "Đỏ"),         QT_TRANSLATE_NOOP("trion::ui::Color", "Vàng"),
    QT_TRANSLATE_NOOP("trion::ui::Color", "Xanh lá"),    QT_TRANSLATE_NOOP("trion::ui::Color", "Xanh lơ"),
    QT_TRANSLATE_NOOP("trion::ui::Color", "Xanh dương"), QT_TRANSLATE_NOOP("trion::ui::Color", "Tím"),
    QT_TRANSLATE_NOOP("trion::ui::Color", "Trắng"),
};

QString translated(const char* source)
{
    return QCoreApplication::translate("trion::ui::Color", source);
}

// Khóa so khớp: bỏ dấu, chữ thường (foldForSearch), bỏ mọi khoảng trắng ("Xanh lá" → "xanhla", "By Layer" → "bylayer").
QString key(QStringView s)
{
    QString k = text::foldForSearch(s);
    k.removeIf([](QChar c) { return c.isSpace(); });
    return k;
}

QString key(std::string_view utf8)
{
    return key(QString::fromUtf8(utf8.data(), qsizetype(utf8.size())));
}

} // namespace

Color Color::byBlock()
{
    Color c;
    c.kind_ = Kind::ByBlock;
    return c;
}

Color Color::aci(int index)
{
    TRION_CHECK(index >= regen::kAciFirst && index <= regen::kAciLast);
    Color c;
    c.kind_ = Kind::Aci;
    c.index_ = index;
    return c;
}

Color Color::rgb(regen::Rgb rgb)
{
    Color c;
    c.kind_ = Kind::True;
    c.rgb_ = rgb;
    return c;
}

Color Color::book(const QString& book, const QString& name, regen::Rgb rgb)
{
    Color c;
    c.kind_ = Kind::Book;
    c.rgb_ = rgb;
    c.book_ = book;
    c.name_ = name;
    return c;
}

regen::Rgb Color::rgb() const
{
    return kind_ == Kind::Aci ? regen::aciRgb(index_) : rgb_;
}

QString Color::label() const
{
    switch (kind_) {
    case Kind::ByLayer:
        return translated(kByLayer);
    case Kind::ByBlock:
        return translated(kByBlock);
    case Kind::Aci:
        return index_ <= 7 ? translated(kNames[index_ - 1]) : translated(kIndexed).arg(index_);
    case Kind::True:
        return hexName(rgb_);
    case Kind::Book:
        return name_.isEmpty() ? hexName(rgb_) : name_;
    }
    return {};
}

QString hexName(regen::Rgb c)
{
    return u"#%1%2%3"_s.arg(c.r, 2, 16, u'0').arg(c.g, 2, 16, u'0').arg(c.b, 2, 16, u'0').toUpper();
}

std::optional<regen::Rgb> parseHex(QStringView text)
{
    QStringView s = text.trimmed();
    if (s.startsWith(u'#'))
        s = s.sliced(1);
    if (s.size() != 6)
        return std::nullopt;
    std::uint32_t v = 0;
    for (const QChar c : s) { // chỉ 0–9, a–f, A–F (toUInt còn nhận dấu, khoảng trắng)
        const char16_t u = c.unicode();
        const int d = u >= u'0' && u <= u'9'   ? u - u'0'
                      : u >= u'a' && u <= u'f' ? u - u'a' + 10
                      : u >= u'A' && u <= u'F' ? u - u'A' + 10
                                               : -1;
        if (d < 0)
            return std::nullopt;
        v = v << 4 | std::uint32_t(d);
    }
    return regen::Rgb{std::uint8_t(v >> 16), std::uint8_t(v >> 8), std::uint8_t(v)};
}

std::optional<Color> parseColor(QStringView text)
{
    const QString k = key(text);
    if (k.isEmpty())
        return std::nullopt;
    for (const Color& c : {Color::byLayer(), Color::byBlock()})
        if (k == key(c.label()) || k == (c.kind() == Color::Kind::ByLayer ? u"bylayer"_s : u"byblock"_s))
            return c;
    for (int i = 1; i <= 7; ++i)
        if (k == key(regen::aciNameVi(i)) || k == key(regen::aciNameEn(i)) || k == key(Color::aci(i).label()))
            return Color::aci(i);
    QStringView n = k;
    for (const QStringView prefix : {u"mau", u"color", u"aci"})
        if (n.startsWith(prefix)) {
            n = n.sliced(prefix.size());
            break;
        }
    if (!n.isEmpty() && n.size() <= 3 && std::all_of(n.begin(), n.end(), [](QChar c) { return c.isDigit(); })) {
        const int i = n.toInt();
        return i >= regen::kAciFirst && i <= regen::kAciLast ? std::optional(Color::aci(i)) : std::nullopt;
    }
    if (const std::optional<regen::Rgb> c = parseHex(k))
        return Color::rgb(*c);
    return std::nullopt;
}

Hsl toHsl(regen::Rgb c)
{
    const double r = c.r / 255.0, g = c.g / 255.0, b = c.b / 255.0;
    const double hi = std::max({r, g, b}), lo = std::min({r, g, b}), d = hi - lo;
    Hsl o;
    o.l = (hi + lo) / 2;
    if (c.r == c.g && c.g == c.b)
        return o; // xám: không có sắc độ
    o.s = std::clamp(d / (1 - std::abs(2 * o.l - 1)), 0.0, 1.0);
    if (hi == r)
        o.h = 60 * std::fmod((g - b) / d + 6, 6);
    else if (hi == g)
        o.h = 60 * ((b - r) / d + 2);
    else
        o.h = 60 * ((r - g) / d + 4);
    return o;
}

regen::Rgb toRgb(const Hsl& c)
{
    const double h = std::fmod(std::fmod(c.h, 360.0) + 360.0, 360.0) / 60; // 0 ≤ h < 6
    const double s = std::clamp(c.s, 0.0, 1.0), l = std::clamp(c.l, 0.0, 1.0);
    const double chroma = (1 - std::abs(2 * l - 1)) * s;
    const double x = chroma * (1 - std::abs(std::fmod(h, 2.0) - 1)), m = l - chroma / 2;
    double r = 0, g = 0, b = 0;
    switch (std::min(int(h), 5)) {
    case 0:
        r = chroma, g = x;
        break;
    case 1:
        r = x, g = chroma;
        break;
    case 2:
        g = chroma, b = x;
        break;
    case 3:
        g = x, b = chroma;
        break;
    case 4:
        r = x, b = chroma;
        break;
    default:
        r = chroma, b = x;
        break;
    }
    const auto byte = [](double v) { return std::uint8_t(std::clamp(std::lround(v * 255), 0L, 255L)); };
    return {byte(r + m), byte(g + m), byte(b + m)};
}

} // namespace trion::ui
