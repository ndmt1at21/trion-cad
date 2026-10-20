#include "app/core/i18n/UiFormat.h"

#include "app/core/i18n/Translator.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"

#include <QLocale>
#include <QTemporaryDir>
#include <QTest>

#include <limits>
#include <memory>

using namespace Qt::StringLiterals;
using namespace trion::i18n;
using trion::settings::Store;

class UiFormatTest : public QObject {
    Q_OBJECT

    QTemporaryDir dir_;
    std::unique_ptr<Store> store_;
    std::unique_ptr<Translator> translator_;

    // Mọi ca vàng của một ngôn ngữ: (kết quả, mong đợi)
    static QList<std::pair<QString, QString>> golden(bool en)
    {
        const auto v = [en](const char* vi, const char* english) { return QString::fromUtf8(en ? english : vi); };
        const QDateTime noon(QDate(2026, 3, 5), QTime(14, 5));
        return {
            {formatCount(0), u"0"_s},
            {formatCount(999), u"999"_s},
            {formatCount(1000), v("1.000", "1,000")},
            {formatCount(1250), v("1.250", "1,250")},
            {formatCount(12345), v("12.345", "12,345")},
            {formatCount(1250000), v("1.250.000", "1,250,000")},
            {formatCount(-1250), v("-1.250", "-1,250")},
            {formatCount(std::numeric_limits<qint64>::max()),
             v("9.223.372.036.854.775.807", "9,223,372,036,854,775,807")},
            {formatNumber(12.5, 1), v("12,5", "12.5")},
            {formatNumber(1234.5, 1), v("1.234,5", "1,234.5")},
            {formatNumber(0.25, 2), v("0,25", "0.25")},
            {formatNumber(-3.76, 1), v("-3,8", "-3.8")},
            {formatBytes(0), u"0 B"_s},
            {formatBytes(1023), v("1.023 B", "1,023 B")},
            {formatBytes(1024), v("1,0 KB", "1.0 KB")},
            {formatBytes(1536), v("1,5 KB", "1.5 KB")},
            {formatBytes(101376), v("99,0 KB", "99.0 KB")},
            {formatBytes(102349), u"100 KB"_s}, // 99,95 KB: làm tròn rồi mới chọn số chữ số lẻ
            {formatBytes(13107200), v("12,5 MB", "12.5 MB")},
            {formatBytes(1048575), v("1,0 MB", "1.0 MB")}, // 1.023,999 KB không thành "1.024 KB"
            {formatBytes(1610612736), v("1,5 GB", "1.5 GB")},
            {formatBytes(5 * (qint64(1) << 40)), v("5,0 TB", "5.0 TB")},
            {formatBytes(-13107200), v("-12,5 MB", "-12.5 MB")},
            {formatBytes(std::numeric_limits<qint64>::max()), v("8,0 EB", "8.0 EB")},
            {formatDate(QDate(2026, 3, 5)), u"05/03/2026"_s},
            {formatDate(QDate(1999, 12, 31)), u"31/12/1999"_s},
            {formatDate(QDate()), QString()},
            {formatTime(QTime(14, 5)), u"14:05"_s},
            {formatTime(QTime(23, 59, 59)), u"23:59"_s},
            {formatDateTime(noon), u"05/03/2026 14:05"_s},
            {formatDuration(-5), u"0 ms"_s},
            {formatDuration(850), u"850 ms"_s},
            {formatDuration(1000), v("1 giây", "1 s")},
            {formatDuration(1500), v("1,5 giây", "1.5 s")},
            {formatDuration(9949), v("9,9 giây", "9.9 s")},
            {formatDuration(9950), v("10 giây", "10 s")},
            {formatDuration(59499), v("59 giây", "59 s")},
            {formatDuration(59500), v("1 phút", "1 min")},
            {formatDuration(125000), v("2 phút 5 giây", "2 min 5 s")},
            {formatDuration(3599499), v("59 phút 59 giây", "59 min 59 s")},
            {formatDuration(3600000), v("1 giờ", "1 h")},
            {formatDuration(3900000), v("1 giờ 5 phút", "1 h 5 min")},
            {formatDuration(qint64(3600000) * 1000), v("1.000 giờ", "1,000 h")},
            {formatDuration(std::numeric_limits<qint64>::max()).left(5), v("1.281", "1,281")}, // không tràn
        };
    }

private slots:
    void initTestCase()
    {
        store_ = std::make_unique<Store>(dir_.filePath(u"user.json"_s));
        Store::setInstance(store_.get());
        translator_ = std::make_unique<Translator>();
    }

    void cleanupTestCase()
    {
        translator_.reset();
        Store::setInstance(nullptr);
        store_.reset();
        trion::sched::waitIdle(); // Store ghi nền xong trước khi xóa thư mục tạm
    }

    // Tiêu chí 1: vi 1.250, 1.250.000, 12,5 MB, 05/03/2026; en 1,250, 12.5 MB; ≥ 50 ca vàng.
    void goldenCases()
    {
        int cases = 0;
        QStringList wrong;
        for (const char* lang : {"vi", "en"}) {
            QVERIFY(translator_->setLanguage(QString::fromLatin1(lang)));
            for (const auto& [got, want] : golden(qstrcmp(lang, "en") == 0)) {
                ++cases;
                if (got != want)
                    wrong << u"%1: «%2», cần «%3»"_s.arg(QString::fromLatin1(lang), got, want);
            }
        }
        QVERIFY2(wrong.isEmpty(), qPrintable(wrong.join(u'\n')));
        QVERIFY2(cases >= 50, qPrintable(QString::number(cases)));
        QVERIFY(translator_->setLanguage(kPseudoLanguage.toString())); // vi-XL: số theo vi, chữ đơn vị kéo dài
        QCOMPARE(formatCount(1250), u"1.250"_s);
        QVERIFY(formatDuration(125000).startsWith(u"2 phút 5 giây Ẫ"_s));
        QVERIFY(translator_->setLanguage(u"vi"_s));
    }

    // Tiêu chí 4 (phần làm được ở đây): định dạng giao diện không theo QLocale mặc định / hệ điều hành, và đổi
    // ngôn ngữ không đổi QLocale mặc định mà số bản vẽ có thể chạm tới. Đầy đủ khi RGN-013 vào cùng build: so
    // core/regen/numfmt formatLinear/formatAngle trước và sau đổi ngôn ngữ.
    void independentOfDefaultLocale()
    {
        const QLocale saved;
        QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
        QVERIFY(translator_->setLanguage(u"en"_s));
        QCOMPARE(formatCount(1250), u"1,250"_s);
        QCOMPARE(formatBytes(13107200), u"12.5 MB"_s);
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QCOMPARE(formatCount(1250), u"1.250"_s);
        QCOMPARE(formatNumber(12.5, 1), u"12,5"_s);
        QLocale::setDefault(saved);
    }

    void languageSwitchKeepsDefaultLocale()
    {
        const QLocale before;
        const QString drawing = QLocale().toString(1250.5, 'f', 1);
        for (const QString& lang : {u"en"_s, kPseudoLanguage.toString(), u"vi"_s}) {
            QVERIFY(translator_->setLanguage(lang));
            QCoreApplication::processEvents();
            QCOMPARE(QLocale(), before);
            QCOMPARE(QLocale().toString(1250.5, 'f', 1), drawing);
            QCOMPARE(QString::number(1250.5), u"1250.5"_s);
        }
    }
};

QTEST_GUILESS_MAIN(UiFormatTest)
#include "uiformat_test.moc"
