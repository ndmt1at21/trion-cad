#include "app/theme/fonts/FontRegistry.h"

#include "app/theme/generated/Tokens.h"
#include "app/theme/ThemeManager.h"

#include <QFile>
#include <QFontDatabase>
#include <QStringList>

#include <iterator>

using namespace Qt::StringLiterals;

namespace trion::theme {
namespace {

struct RoleSpec {
    bool mono;
    tok::PxId size;
};

// Theo thứ tự TextRole.
constexpr RoleSpec kRoles[] = {
    {false, tok::fs},   {false, tok::fs_s}, {false, tok::fs_xs}, {false, tok::fs_2xs},
    {false, tok::fs_m}, {false, tok::fs_l}, {false, tok::fs_xl}, {false, tok::fs_2xl},
    {true, tok::fs},    {true, tok::fs_s},  {true, tok::fs_xs},
};
static_assert(std::size(kRoles) == std::size_t(TextRole::MonoCaption) + 1, "mỗi TextRole một dòng");

// "IBM Plex Mono", ui-monospace, … → danh sách họ cho QFont::setFamilies (cùng cách ThemeManager đọc --f-ui).
// Tên chung của CSS (system-ui, monospace…) Qt bỏ qua nếu không có font tên đó.
QStringList families(tok::StrId id)
{
    const std::string_view css = tok::str(id);
    QStringList out = QString::fromUtf8(css.data(), qsizetype(css.size())).split(u',');
    for (QString& name : out)
        name = name.trimmed().remove(u'"');
    return out;
}

} // namespace

Expected<void> loadFonts()
{
    QStringList failed;
    for (const QString& file :
         {u"IBMPlexSans-Regular.ttf"_s, u"IBMPlexSans-Medium.ttf"_s, u"IBMPlexSans-SemiBold.ttf"_s,
          u"IBMPlexMono-Regular.ttf"_s, u"IBMPlexMono-Medium.ttf"_s})
        if (QFontDatabase::addApplicationFont(":/trion/fonts/"_L1 + file) < 0)
            failed << file;
    if (failed.isEmpty())
        return {};
    return Error{"ui", "font-load", "Không nạp được font nhúng: " + failed.join(", "_L1).toStdString()};
}

QFont font(TextRole role, Weight weight)
{
    static const QStringList ui = families(tok::f_ui), mono = families(tok::f_mono);
    const RoleSpec& r = kRoles[std::size_t(role)];
    QFont f = ThemeManager::instance().font(r.size); // cỡ theo chế độ hiện tại
    f.setFamilies(r.mono ? mono : ui);
    f.setStyleHint(r.mono ? QFont::TypeWriter : QFont::SansSerif); // hết họ trong danh sách: font hệ thống cùng kiểu
    const int weights[] = {QFont::Normal, int(tok::num(tok::fw_m)), int(tok::num(tok::fw_b))};
    f.setWeight(QFont::Weight(weights[std::size_t(weight)]));
    return f;
}

QString fontLicense()
{
    QFile f(u":/trion/fonts/OFL.txt"_s);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

} // namespace trion::theme
