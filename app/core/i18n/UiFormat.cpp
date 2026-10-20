#include "app/core/i18n/UiFormat.h"

#include "app/core/i18n/Translator.h"

#include <QCoreApplication>
#include <QLocale>

#include <algorithm>
#include <cmath>
#include <limits>

using namespace Qt::StringLiterals;

namespace trion::i18n {

namespace {
// Dữ liệu CLDR có sẵn trong Qt (vi: nhóm ".", thập phân ","), không phải QLocale::system()
QLocale locale()
{
    static const QLocale vi(QLocale::Vietnamese, QLocale::Vietnam);
    static const QLocale en(QLocale::English, QLocale::UnitedStates);
    const Translator* t = Translator::instance();
    return t && t->language() == u"en"_s ? en : vi;
}
} // namespace

QString formatCount(qint64 n) { return locale().toString(n); }

QString formatNumber(double value, int decimals) { return locale().toString(value, 'f', decimals); }

QString formatBytes(qint64 bytes)
{
    static constexpr const char* kUnits[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    double v = double(bytes);
    int u = 0;
    while (std::abs(v) >= 1024 && u < 6) {
        v /= 1024;
        ++u;
    }
    if (u > 0 && u < 6 && std::round(std::abs(v)) >= 1024) { // 1.048.575 B làm tròn ra "1.024 KB": lên MB
        v /= 1024;
        ++u;
    }
    const int decimals = u > 0 && std::round(std::abs(v) * 10) < 1000 ? 1 : 0; // 99,96 KB → "100 KB"
    return u"%1 %2"_s.arg(formatNumber(v, decimals), QLatin1StringView(kUnits[u]));
}

QString formatDate(QDate date) { return date.toString(u"dd/MM/yyyy"); }

QString formatTime(QTime time) { return time.toString(u"HH:mm"); }

QString formatDateTime(const QDateTime& dt) { return dt.toString(u"dd/MM/yyyy HH:mm"); }

// Chuỗi đơn vị viết thẳng trong QCoreApplication::translate để lupdate thấy (không bọc qua hàm phụ)
QString formatDuration(qint64 ms)
{
    ms = std::clamp<qint64>(ms, 0, std::numeric_limits<qint64>::max() / 2); // làm tròn bên dưới không tràn
    if (ms < 1000)
        return u"%1 ms"_s.arg(formatCount(ms));
    const qint64 tenths = (ms + 50) / 100, s = (ms + 500) / 1000, min = (ms + 30'000) / 60'000;
    if (s < 60) // dưới 10 giây: một chữ số lẻ, bỏ ",0"
        return QCoreApplication::translate("UiFormat", "%1 giây")
            .arg(tenths < 100 ? formatNumber(tenths / 10.0, tenths % 10 ? 1 : 0) : formatCount(s));
    if (s < 3600)
        return s % 60 ? QCoreApplication::translate("UiFormat", "%1 phút %2 giây")
                            .arg(formatCount(s / 60), formatCount(s % 60))
                      : QCoreApplication::translate("UiFormat", "%1 phút").arg(formatCount(s / 60));
    return min % 60 ? QCoreApplication::translate("UiFormat", "%1 giờ %2 phút")
                          .arg(formatCount(min / 60), formatCount(min % 60))
                    : QCoreApplication::translate("UiFormat", "%1 giờ").arg(formatCount(min / 60));
}

} // namespace trion::i18n
