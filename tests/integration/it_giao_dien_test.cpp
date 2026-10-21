// Integration test nhóm 2: UI0-002 (chủ đề), UI0-005 (bản địa hóa), UI0-105 (đo độ trễ), UI0-003 (font), UI0-004
// (biểu tượng) cùng RGN-013 (định dạng số) trên một QApplication, một kho cài đặt, một ThemeManager, một Translator
// và một IconProvider.

#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/i18n/UiFormat.h"
#include "app/core/perf/UiProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"
#include "core/regen/numfmt/NumFormat.h"
#include "tests/timing.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEventLoop>
#include <QFontInfo>
#include <QGridLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRawFont>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>

#include <memory>
#include <numbers>
#include <thread>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;
using trion::settings::Store;
using trion::theme::Density;
using trion::theme::Theme;
using trion::theme::ThemeManager;
namespace i18n = trion::i18n;
namespace probe = trion::ui::probe;
namespace regen = trion::regen;

namespace {

// Hộp thoại theo quy ước retranslate() của UI0-104; chuỗi của ngữ cảnh ProbeScreen (đã có bản dịch).
// fixed có bề rộng cố định: đối chứng cho TruncationProbe.
class Dialog : public QWidget {
public:
    Dialog()
    {
        auto* grid = new QGridLayout(this);
        grid->addWidget(title = new QLabel, 0, 0, 1, 2);
        grid->addWidget(check = new QCheckBox, 1, 0, 1, 2);
        grid->addWidget(apply = new QPushButton, 2, 0);
        grid->addWidget(saveAs = new QPushButton, 2, 1);
        grid->addWidget(fixed = new QPushButton, 3, 0);
        retranslate();
        fixed->setFixedWidth(fixed->sizeHint().width());
    }

    QLabel* title;
    QCheckBox* check;
    QPushButton* apply;
    QPushButton* saveAs;
    QPushButton* fixed;

protected:
    void changeEvent(QEvent* e) override
    {
        if (e->type() == QEvent::LanguageChange)
            retranslate();
        QWidget::changeEvent(e);
    }

private:
    void retranslate()
    {
        title->setText(QCoreApplication::translate("ProbeScreen", "Độ dày nét theo layer"));
        check->setText(QCoreApplication::translate("ProbeScreen", "Hiện độ dày nét"));
        apply->setText(QCoreApplication::translate("ProbeScreen", "Áp dụng"));
        saveAs->setText(QCoreApplication::translate("ProbeScreen", "Lưu bản vẽ thành tệp mới"));
        fixed->setText(QCoreApplication::translate("ProbeScreen", "Thuộc tính"));
    }
};

QJsonObject opStats(probe::Op op)
{
    return probe::toJson()[u"ops"_s].toObject()[QString::fromLatin1(probe::name(op))].toObject();
}

// Mọi chuỗi số bản vẽ (RGN-013) của một bộ giá trị theo mọi LUNITS và AUNITS.
QStringList drawingNumbers()
{
    using LU = regen::LinearUnits;
    using AU = regen::AngularUnits;
    const LU lunits[] = {LU::Scientific, LU::Decimal, LU::Engineering, LU::Architectural, LU::Fractional};
    const AU aunits[] = {AU::Degrees, AU::DegMinSec, AU::Grads, AU::Radians, AU::Surveyor};
    QStringList out;
    for (int i = 0; i < 5; ++i) {
        regen::UnitContext c;
        c.lunits = lunits[i];
        c.aunits = aunits[i];
        c.auprec = 4;
        for (const double v : {0.0, 12.5, -1234.5678, 1e6 / 3})
            out << QString::fromStdString(regen::linearString(v, c));
        for (const double a : {0.0, std::numbers::pi / 6, 1.23456})
            out << QString::fromStdString(regen::angleString(a, c));
    }
    return out;
}

} // namespace

class IntegrationGiaoDien : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<Store>(dir_.filePath(u"user.json"_s));
        Store::setInstance(store_.get());
        executor_ = std::make_unique<trion::ui::UiExecutor>();
        translator_ = std::make_unique<i18n::Translator>();
        QVERIFY(trion::theme::loadFonts()); // thứ tự của main: QApplication → loadFonts → ThemeManager
        theme_ = std::make_unique<ThemeManager>(*store_);
        icons_ = std::make_unique<trion::theme::IconProvider>();
        QCOMPARE(theme_->theme(), Theme::Dark);
        QCOMPARE(translator_->language(), u"vi"_s);
    }

    void cleanupTestCase()
    {
        probe::disable();
        trion::sched::waitIdle();
        icons_.reset();
        theme_.reset();
        translator_.reset();
        executor_.reset();
        Store::setInstance(nullptr);
        store_.reset();
    }

    // UI0-005 tiêu chí 4: số giao diện chung (UiFormat) đổi theo ngôn ngữ, số bản vẽ (RGN-013) thì không.
    void drawingNumbersIgnoreLanguage()
    {
        const QStringList vi = drawingNumbers();
        QVERIFY(vi.contains(u"12.5000"_s)); // dấu chấm thập phân cả khi giao diện tiếng Việt
        QCOMPARE(i18n::formatNumber(12.5, 1), u"12,5"_s);

        QVERIFY(translator_->setLanguage(u"en"_s));
        QCOMPARE(i18n::formatNumber(12.5, 1), u"12.5"_s);
        QCOMPARE(i18n::formatCount(1250), u"1,250"_s);
        QCOMPARE(drawingNumbers(), vi);

        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QCOMPARE(i18n::formatCount(1250), u"1.250"_s);
        QCOMPARE(drawingNumbers(), vi);

        QVERIFY(translator_->setLanguage(u"vi"_s));
        QCOMPARE(drawingNumbers(), vi);
    }

    // UI0-105 đo đổi chủ đề của UI0-002 từ lúc gọi tới khi khung mới lên màn hình; khung đó mang màu chủ đề mới.
    void themeSwitchMeasuredByProbe()
    {
        QWidget win;
        auto* grid = new QGridLayout(&win);
        for (int i = 0; i < 40; ++i) { // ~500 widget kể cả widget con của ô chọn, ô nhập
            const int row = i % 20, col = i / 20 * 4;
            grid->addWidget(new QLabel(QString::number(i)), row, col);
            grid->addWidget(new QPushButton(QString::number(i)), row, col + 1);
            grid->addWidget(new QLineEdit(QString::number(i)), row, col + 2);
            auto* combo = new QComboBox;
            combo->addItem(QString::number(i));
            grid->addWidget(combo, row, col + 3);
        }
        win.resize(1280, 900);
        win.show();
        QVERIFY(QTest::qWaitForWindowExposed(&win));

        probe::enable();
        probe::reset();
        const int rounds = 6;
        for (int i = 0; i < rounds; ++i) {
            const auto start = probe::Clock::now();
            theme_->setTheme(i % 2 == 0 ? Theme::Light : Theme::Dark);
            probe::untilFrame(probe::Op::UiState, &win, start);
            QTRY_COMPARE(opStats(probe::Op::UiState)[u"count"_s].toInt(), i + 1);
            QCOMPARE(win.grab().toImage().pixelColor(2, 2).rgb(), theme_->color(tok::c_base).rgb());
        }
        const QJsonObject s = opStats(probe::Op::UiState);
        qInfo("đổi chủ đề, %lld widget: p50 %.1f ms, max %.1f ms, ngân sách %g ms",
              qint64(win.findChildren<QWidget*>().size()), s[u"p50_ms"_s].toDouble(), s[u"max_ms"_s].toDouble(),
              s[u"budget_ms"_s].toDouble());
        QVERIFY(!trion::test::kTimed || s[u"p50_ms"_s].toDouble() <= s[u"budget_ms"_s].toDouble());
        probe::disable();
    }

    // vi-XL (dài hơn 40%) ở mật độ thoáng, chủ đề sáng: bố cục giãn theo, TruncationProbe chỉ báo widget cố định.
    void pseudoLanguageComfortableNoTruncation()
    {
        Dialog dlg;
        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg));
        QVERIFY(i18n::findTruncations(&dlg).isEmpty());

        theme_->setDensity(Density::Comfortable);
        theme_->setTheme(Theme::Light);
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        const QString xl = QCoreApplication::translate("ProbeScreen", "Áp dụng");
        QVERIFY(xl.size() >= qRound(u"Áp dụng"_s.size() * 1.4));
        QTRY_COMPARE(dlg.apply->text(), xl);
        QTRY_COMPARE(i18n::findTruncations(&dlg).size(), 1);
        const i18n::Truncation t = i18n::findTruncations(&dlg).first();
        QCOMPARE(t.widget, dlg.fixed);
        QVERIFY(t.need.width() > dlg.fixed->width());

        QVERIFY(translator_->setLanguage(u"vi"_s));
        theme_->setDensity(Density::Compact);
        theme_->setTheme(Theme::Dark);
        QTRY_VERIFY(i18n::findTruncations(&dlg).isEmpty());
    }

    // UI0-003 + UI0-002 + UI0-104: Plex nạp trước ThemeManager thành font ứng dụng; chữ có dấu chồng tầng dựng bằng
    // Plex trong widget theo chủ đề, qua mọi chủ đề, mật độ và ngôn ngữ; vai trò Mono dùng Plex Mono.
    void vietnameseTextUsesPlex()
    {
        using trion::theme::TextRole;
        QWidget win;
        auto* grid = new QGridLayout(&win);
        const QString text = u"Ấ Ẫ Ự Ỡ Đ ấ ẫ ự ỡ đ"_s;
        auto* label = new QLabel(text);
        auto* button = new QPushButton(text);
        grid->addWidget(label, 0, 0);
        grid->addWidget(button, 1, 0);
        win.show();
        QVERIFY(QTest::qWaitForWindowExposed(&win));
        const auto check = [&] {
            for (QWidget* w : {static_cast<QWidget*>(label), static_cast<QWidget*>(button)}) {
                QCOMPARE(QFontInfo(w->font()).family(), u"IBM Plex Sans"_s);
                const QRawFont raw = QRawFont::fromFont(w->font());
                for (const QChar c : text)
                    QVERIFY2(c.isSpace() || raw.supportsCharacter(c), qPrintable(QString(c)));
            }
            QCOMPARE(QFontInfo(trion::theme::font(TextRole::Mono)).family(), u"IBM Plex Mono"_s);
        };
        check();
        theme_->setTheme(Theme::Light);
        theme_->setDensity(Density::Comfortable);
        QVERIFY(translator_->setLanguage(u"en"_s));
        QCoreApplication::processEvents();
        check();
        QVERIFY(translator_->setLanguage(u"vi"_s));
        theme_->setDensity(Density::Compact);
        theme_->setTheme(Theme::Dark);
        QCoreApplication::processEvents();
        check();
    }

    // UI0-004 + UI0-002 + UI0-101 + UI0-105: bộ Viewer raster trước ở nền (về qua ui::post); đổi chủ đề thì nút đang
    // hiện đổi màu biểu tượng ngay khung kế, không dựng lại QIcon, không raster lại SVG (chỉ tô lại từ mặt nạ).
    void themeSwitchRecolorsIcons()
    {
        using trion::theme::IconProvider;
        const QStringList viewer = IconProvider::names(true);
        QVERIFY(viewer.size() >= 80);
        icons_->prewarm(1.0);
        trion::sched::waitIdle();
        QTRY_VERIFY(icons_->stats().rasters >= quint64(viewer.size()));

        QWidget win;
        auto* grid = new QGridLayout(&win);
        QList<QToolButton*> buttons;
        for (qsizetype i = 0; i < viewer.size(); ++i) {
            auto* b = new QToolButton;
            b->setIcon(IconProvider::icon(viewer[i]));
            b->setIconSize(QSize(16, 16));
            b->setAutoRaise(true);
            grid->addWidget(b, int(i / 20), int(i % 20));
            buttons << b;
        }
        QToolButton* disabled = buttons.takeLast();
        disabled->setEnabled(false);
        win.show();
        QVERIFY(QTest::qWaitForWindowExposed(&win));
        const auto rasters = icons_->stats().rasters; // bộ Viewer ở 100% đã có sẵn: hiện không raster thêm
        const auto tints = icons_->stats().tints;

        // Số điểm ảnh đúng màu c trong các nút (điểm ảnh biểu tượng phủ kín = màu token, không pha nền).
        const auto count = [](const QList<QToolButton*>& list, const QColor& c) {
            qsizetype n = 0;
            for (QToolButton* b : list) {
                const QImage img = b->grab().toImage().convertToFormat(QImage::Format_RGB32);
                for (int y = 0; y < img.height(); ++y)
                    for (int x = 0; x < img.width(); ++x)
                        n += img.pixel(x, y) == c.rgb();
            }
            return n;
        };
        const QColor darkNormal = theme_->color(tok::c_text_2), darkDisabled = theme_->color(tok::c_text_dis);
        QVERIFY(count(buttons, darkNormal) > 0);
        QVERIFY(count({disabled}, darkDisabled) > 0);

        probe::enable();
        probe::reset();
        const auto start = probe::Clock::now();
        theme_->setTheme(Theme::Light);
        probe::untilFrame(probe::Op::UiState, &win, start);
        QTRY_COMPARE(opStats(probe::Op::UiState)[u"count"_s].toInt(), 1);
        const QJsonObject s = opStats(probe::Op::UiState);
        qInfo("đổi chủ đề, %lld nút biểu tượng: %.1f ms đến khung, %llu lần tô lại, %llu lần raster",
              qint64(buttons.size() + 1), s[u"p50_ms"_s].toDouble(),
              (unsigned long long)(icons_->stats().tints - tints),
              (unsigned long long)(icons_->stats().rasters - rasters));
        QVERIFY(!trion::test::kTimed || s[u"p50_ms"_s].toDouble() <= s[u"budget_ms"_s].toDouble());
        probe::disable();

        QVERIFY(count(buttons, theme_->color(tok::c_text_2)) > 0);
        QCOMPARE(count(buttons, darkNormal), 0);
        QVERIFY(count({disabled}, theme_->color(tok::c_text_dis)) > 0);
        QCOMPARE(count({disabled}, darkDisabled), 0);
        QCOMPARE(icons_->stats().rasters, rasters);
        QVERIFY(icons_->stats().tints > tints);

        theme_->setTheme(Theme::Dark);
    }

    // Lựa chọn của các module cùng nằm trong một tệp cài đặt và nạp lại được.
    void choicesPersistTogether()
    {
        theme_->setTheme(Theme::Light);
        theme_->setDensity(Density::Comfortable);
        QVERIFY(translator_->setLanguage(u"en"_s));
        store_->flush();
        Store reloaded(dir_.filePath(u"user.json"_s));
        QCOMPARE(reloaded.get(trion::theme::kTheme), u"light"_s);
        QCOMPARE(reloaded.get(trion::theme::kDensity), u"comfortable"_s);
        QCOMPARE(reloaded.get(i18n::kLanguage), u"en"_s);

        QVERIFY(translator_->setLanguage(u"vi"_s));
        theme_->setDensity(Density::Compact);
        theme_->setTheme(Theme::Dark);
    }

    // Sau mọi việc trên, đang bật đo: rảnh thì cả executor lẫn watchdog không bị đánh thức. Rảnh thật là vòng sự
    // kiện chặn trong dispatcher (QTest::qWait gọi processEvents liên tục); bộ đếm đọc từ luồng phụ giữa khoảng rảnh,
    // xa sự kiện timer đóng vòng.
    void idleAfterwards()
    {
        probe::enable();
        trion::sched::waitIdle();
        QTest::qWait(100);
        const auto wakeups = executor_->stats().wakeups;
        probe::Counters c0, c1;
        std::thread sampler([&] {
            std::this_thread::sleep_for(200ms);
            c0 = probe::counters();
            std::this_thread::sleep_for(1000ms);
            c1 = probe::counters();
        });
        QEventLoop loop;
        QTimer::singleShot(1400ms, &loop, &QEventLoop::quit);
        loop.exec();
        sampler.join();
        QCOMPARE(executor_->stats().wakeups, wakeups);
        QCOMPARE(c1.events, c0.events);
        QCOMPARE(c1.watchdogWakeups, c0.watchdogWakeups);
        probe::disable();
    }

private:
    QTemporaryDir dir_;
    std::unique_ptr<Store> store_;
    std::unique_ptr<trion::ui::UiExecutor> executor_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<ThemeManager> theme_;
    std::unique_ptr<trion::theme::IconProvider> icons_;
};

QTEST_MAIN(IntegrationGiaoDien)
#include "it_giao_dien_test.moc"
