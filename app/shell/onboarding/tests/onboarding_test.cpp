// UIA-016: lần chạy đầu (Onboarding.h, QuickTour.h). Cửa sổ chính thật (MainWindow, ô giữ chỗ của các vùng) ở lớp màn
// hình Wide (palette Thuộc tính mở), Store tạm mỗi test, EditionGate Viewer. PLF-008 chưa có: chỗ nối gửi giả đếm gói.
// Tiêu chí của task:
//   1  applyNow: đổi ngôn ngữ ở bước 1, chủ đề ở bước 2 áp ngay ≤ 100 ms CPU luồng UI (release), không dựng lại cửa
//      sổ; chữ của chính hộp dịch lại sau LanguageChange.
//   2  skipTelemetry: bỏ qua mọi bước (Bỏ qua ở bước 1; Tiếp tới hết với mặc định): mức telemetry "off", chỗ nối
//      gửi giả nhận 0 gói; đối chứng: đồng ý thì đúng 1 gói. Proxy mạng của PLF-008: Hoãn.
//   3  skipAnyStep: bỏ qua ở từng bước (nút, Esc, nút Đóng): hộp đóng, cửa sổ chính active và có focus; Store mới cùng
//      tệp: needed() sai; start() chạy lại từ bước 1.
//   4  tourKeyboard: hướng dẫn 5 bước bằng phím (Enter, Shift+Tab/Tab, mũi tên, Esc); vùng tô sáng đúng thứ tự, có tên
//      cho trình đọc màn hình (thông báo theo MainWindow::regionName).
//   5  a11y, pseudoNoTruncation: checkNames/checkFocus 0 vi phạm mỗi bước; vi-XL 0 chữ cắt (hộp, thẻ hướng dẫn).
//   Còn lại: steps (addStep, removeStep, visible; bước Phiên bản ẩn khi đã có giấy phép), editionChoice (tín hiệu dùng
//   thử, key; mũi tên trong nhóm thẻ; Enter trên thẻ là Tiếp), overlay (không chặn chuột; scrim, viền accent, vùng giữ
//   nguyên: ảnh không chữ), stripFallback (màn hình hẹp: tab ở dải), openTime (mở hộp đến khung đầu ≤ 150 ms CPU,
//   release), idle (không timer).
#include "app/shell/onboarding/Onboarding.h"
#include "app/shell/onboarding/QuickTour.h"

#include "app/core/a11y/AccessibilityAudit.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/UiExecutor.h"
#include "app/shell/MainWindow.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/dock/AutoHideStrip.h"
#include "app/widgets/dock/PaletteHost.h"
#include "core/base/settings/Settings.h"
#include "tests/timing.h"

#include <QAbstractButton>
#include <QAccessible>
#include <QApplication>
#include <QCheckBox>
#include <QDockWidget>
#include <QLabel>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include <memory>
#include <vector>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using trion::shell::MainWindow;
using trion::shell::Onboarding;
using trion::shell::QuickTour;
using trion::test::kTimed;
using trion::test::threadCpu;
namespace edition = trion::edition;
namespace i18n = trion::i18n;
namespace settings = trion::settings;
namespace shell = trion::shell;
namespace theme = trion::theme;
namespace ui = trion::ui;
using Region = MainWindow::Region;

namespace {

bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 10s)
{
    if (cond())
        return true;
    QEventLoop loop;
    QTimer tick;
    QObject::connect(&tick, &QTimer::timeout, &loop, [&] {
        if (cond())
            loop.quit();
    });
    QTimer::singleShot(timeout, &loop, &QEventLoop::quit);
    tick.start(1);
    loop.exec();
    return cond();
}

void spinFor(std::chrono::milliseconds d)
{
    QEventLoop loop;
    QTimer::singleShot(d, &loop, &QEventLoop::quit);
    loop.exec();
}

double ms(std::chrono::nanoseconds d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

bool waitActive(QWidget* w)
{
    w->activateWindow();
    return spinUntil([w] { return w->isActiveWindow(); });
}

void key(int k, Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    QWidget* f = QApplication::focusWidget();
    QVERIFY(f);
    QTest::keyClick(f, Qt::Key(k), mods);
}

QColor col(tok::ColorId id)
{
    return theme::ThemeManager::instance().color(id);
}

// Màu fg (có alpha) phủ lên nền đặc bg.
QColor over(const QColor& fg, const QColor& bg)
{
    const int a = fg.alpha();
    const auto mix = [a](int f, int b) { return (f * a + b * (255 - a) + 127) / 255; };
    return QColor(mix(fg.red(), bg.red()), mix(fg.green(), bg.green()), mix(fg.blue(), bg.blue()));
}

bool near(const QColor& a, const QColor& b, int tolerance = 2)
{
    return std::abs(a.red() - b.red()) <= tolerance && std::abs(a.green() - b.green()) <= tolerance &&
           std::abs(a.blue() - b.blue()) <= tolerance;
}

QString truncations(QWidget* root)
{
    QStringList out;
    for (const i18n::Truncation& t : i18n::findTruncations(root))
        out << t.toString();
    return out.join(u"; "_s);
}

// Bộ bắt thông báo cho trình đọc màn hình.
std::vector<std::pair<QObject*, QString>> g_said;

void record(QAccessibleEvent* e)
{
    if (e->type() == QAccessible::Announcement)
        g_said.emplace_back(e->object(), static_cast<QAccessibleAnnouncementEvent*>(e)->message());
}

} // namespace

class OnboardingTest : public QObject {
    Q_OBJECT

    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_; // Store::instance: ngôn ngữ, chủ đề
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<theme::ThemeManager> tm_;
    std::unique_ptr<theme::IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<edition::EditionGate> gate_;
    std::unique_ptr<settings::Store> state_; // mỗi test một tệp: cửa sổ, cờ đã chạy, telemetry
    QString statePath_;
    std::unique_ptr<MainWindow> win_;
    QPointer<Onboarding> onb_;
    QList<QJsonObject> sent_; // chỗ nối gửi giả (PLF-008)
    int stores_ = 0;

    // Cửa sổ chính 1366×768 ở lớp màn hình cls, hiện, active.
    bool build(ui::ScreenClass cls = ui::ScreenClass::Wide)
    {
        statePath_ = dir_.filePath(u"state-%1.json"_s.arg(++stores_));
        state_ = std::make_unique<settings::Store>(statePath_);
        win_ = std::make_unique<MainWindow>(*state_);
        win_->restore();
        win_->setWindowState(Qt::WindowNoState);
        win_->setGeometry(QRect(0, 0, 1366, 768));
        win_->setScreenClass(cls);
        win_->show();
        return QTest::qWaitForWindowActive(win_.get());
    }

    // Hộp mới (tự xóa khi đóng) như Application gọi; hiện, active.
    Onboarding* open()
    {
        onb_ = new Onboarding(*win_, *state_);
        onb_->setAttribute(Qt::WA_DeleteOnClose);
        onb_->setTelemetrySink([this](const QJsonObject& o) { sent_ << o; });
        onb_->start();
        return waitActive(onb_) ? onb_.data() : nullptr;
    }

    bool closed()
    {
        return spinUntil([this] { return !onb_; });
    }

    template <class T = QAbstractButton>
    T* find(const QString& name, QWidget* root = nullptr) const
    {
        return (root ? root : onb_.data())->findChild<T*>(name);
    }

    QAbstractButton* closeButton() const
    {
        for (QAbstractButton* b : onb_->findChildren<QAbstractButton*>())
            if (b->accessibleName() == ui::DialogFrame::tr("Đóng (Esc)"))
                return b;
        return nullptr;
    }

    QString stepText(int i, int n, const char* name) const
    {
        return Onboarding::tr("Bước %1 / %2 · %3").arg(QString::number(i), QString::number(n), Onboarding::tr(name));
    }

    // Cửa sổ chính active và focus nằm trong nó.
    bool mainHasFocus() const
    {
        return spinUntil([this] {
            return QApplication::activeWindow() == win_.get() && win_->isAncestorOf(QApplication::focusWidget());
        });
    }

    QuickTour* runTour()
    {
        auto* tour = new QuickTour(*win_);
        tour->start();
        return tour;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        QVERIFY(theme::loadFonts());
        executor_ = std::make_unique<ui::UiExecutor>();
        tm_ = std::make_unique<theme::ThemeManager>(*store_);
        icons_ = std::make_unique<theme::IconProvider>();
        translator_ = std::make_unique<i18n::Translator>();
        gate_ = std::make_unique<edition::EditionGate>();
        QApplication::setQuitOnLastWindowClosed(false);
    }

    void init()
    {
        tm_->setTheme(theme::Theme::Dark);
        tm_->setDensity(theme::Density::Compact);
        tm_->setReducedMotion(true);
        gate_->setEdition(edition::Edition::Viewer);
        sent_.clear();
        g_said.clear();
    }

    void cleanup()
    {
        QAccessible::installUpdateHandler(nullptr);
        QAccessible::setActive(false);
        delete onb_.data();
        if (win_) {
            for (QuickTour* t : win_->findChildren<QuickTour*>())
                delete t;
            win_->close();
        }
        win_.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        state_.reset();
        if (translator_->language() != u"vi"_s) {
            QVERIFY(translator_->setLanguage(u"vi"_s));
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LanguageChange);
        }
    }

    void cleanupTestCase()
    {
        gate_.reset();
        translator_.reset();
        icons_.reset();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    // Tiêu chí 1: chọn English ở bước 1, Sáng ở bước 2: áp ngay (CPU luồng UI từ cú nhấp đến khi mọi widget đã dịch
    // lại, đổi màu và vẽ xong khung kế tiếp ≤ 100 ms, release); cửa sổ chính và hộp vẫn là đối tượng, cửa sổ hệ điều
    // hành cũ.
    void applyNow()
    {
        QVERIFY(build());
        QVERIFY(open());
        const QPointer<QWidget> window = win_.get();
        const WId winId = win_->winId();
        const WId dialogId = onb_->winId();
        const QPoint at = onb_->pos();
        auto* en = find(u"en"_s, onb_->page(u"lang"_s));
        QVERIFY(en && !en->isChecked() && find(u"vi"_s)->isChecked());
        QAbstractButton* close = closeButton();
        QVERIFY(close);

        auto cpu = threadCpu();
        QTest::mouseClick(en, Qt::LeftButton);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LanguageChange); // mỗi widget dịch lại một lần
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        onb_->repaint();
        win_->repaint();
        const double language = ms(threadCpu() - cpu);
        QCOMPARE(translator_->language(), u"en"_s);
        QCOMPARE(store_->get(i18n::kLanguage), u"en"_s);
        QCOMPARE(close->accessibleName(), u"Close (Esc)"_s); // bản English có sẵn của DialogFrame
        QVERIFY(en->isChecked());
        QCOMPARE(onb_->currentStep(), 0);

        // Chữ của chính khung onboarding dịch lại (vi-XL kéo dài chuỗi nguồn tiếng Việt của mọi tr(); title đang là bản
        // English, viTitle là nguồn mà vi và vi-XL dựng từ đó).
        const QString title = Onboarding::tr("Chào mừng đến với TrionCAD");
        const QString viTitle = u"Chào mừng đến với TrionCAD"_s; // i18n-ok: chuỗi nguồn để so
        const QString skip = find(u"skip"_s)->text();
        const QString step = find<QLabel>(u"step"_s)->text();
        const QString heading = onb_->page(u"lang"_s)->findChild<QLabel*>()->text();
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QVERIFY(spinUntil([&] { return onb_->windowTitle() != title; }));
        QCOMPARE(onb_->windowTitle(), Onboarding::tr("Chào mừng đến với TrionCAD")); // bản vi-XL
        QVERIFY(onb_->windowTitle().startsWith(viTitle));
        QCOMPARE(find(u"skip"_s)->text(), Onboarding::tr("Bỏ qua, xem lại trong Tùy chọn"));
        QVERIFY(find(u"skip"_s)->text().size() > skip.size());
        QCOMPARE(find<QLabel>(u"step"_s)->text(), stepText(1, 5, "Ngôn ngữ"));
        QVERIFY(find<QLabel>(u"step"_s)->text().size() > step.size());
        QVERIFY(onb_->page(u"lang"_s)->findChild<QLabel*>()->text().size() > heading.size()); // trang tự dịch lại
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QVERIFY(spinUntil([&] { return onb_->windowTitle() == viTitle; }));
        QCOMPARE(find<QLabel>(u"step"_s)->text(), stepText(1, 5, "Ngôn ngữ"));

        onb_->next();
        auto* light = find(u"light"_s, onb_->page(u"theme"_s));
        QVERIFY(light && !light->isChecked());
        cpu = threadCpu();
        QTest::mouseClick(light, Qt::LeftButton);
        QCoreApplication::sendPostedEvents();
        onb_->repaint();
        win_->repaint();
        const double themeMs = ms(threadCpu() - cpu);
        QCOMPARE(tm_->theme(), theme::Theme::Light);
        QCOMPARE(store_->get(theme::kTheme), u"light"_s);
        // Nền thân hộp (giữa thanh chấm và tiêu đề) đã là --c-base của Sáng.
        const QImage img = onb_->grab().toImage();
        const QPoint gap = onb_->body()->mapTo(onb_, QPoint(onb_->body()->width() / 2, qRound(tm_->px(tok::sp_2)) + 7));
        QCOMPARE(QColor(img.pixel((QPointF(gap) * img.devicePixelRatio()).toPoint())),
                 theme::toQColor(tok::color(tok::c_base, tok::Mode{.light = true})));

        QVERIFY(window); // không dựng lại
        QCOMPARE(win_->winId(), winId);
        QCOMPARE(onb_->winId(), dialogId);
        QCOMPARE(onb_->pos(), at);
        QVERIFY(onb_->isVisible() && win_->isVisible());
        QCOMPARE(onb_->currentStep(), 1);
        qInfo("đổi ngôn ngữ %.1f ms, đổi chủ đề %.1f ms CPU luồng UI", language, themeMs);
        const bool cpuClock = threadCpu().count() >= 0;
        QVERIFY2(!kTimed || !cpuClock || language <= 100, qPrintable(QString::number(language)));
        QVERIFY2(!kTimed || !cpuClock || themeMs <= 100, qPrintable(QString::number(themeMs)));
    }

    // Tiêu chí 2: bỏ qua mọi bước thì telemetry tắt, không gói nào qua chỗ nối gửi (PLF-008 sẽ thay chỗ nối; kiểm bằng
    // proxy mạng của test: Hoãn tới PLF-008).
    void skipTelemetry()
    {
        QVERIFY(build());
        // Bỏ qua ngay ở bước 1.
        QVERIFY(open());
        QTest::mouseClick(find(u"skip"_s), Qt::LeftButton);
        QVERIFY(closed());
        QCOMPARE(state_->get(shell::kTelemetryLevel), u"off"_s);
        QCOMPARE(sent_.size(), 0);
        // Tiếp tới hết bằng Enter với mặc định, rồi kết thúc hướng dẫn bằng Esc.
        QVERIFY(open());
        QCOMPARE(find<QCheckBox>(u"telemetry"_s), nullptr); // trang chưa dựng
        QPointer<QuickTour> tour;
        for (int i = 0; i < 5; ++i) {
            if (onb_->stepIds().value(onb_->currentStep()) == u"privacy"_s)
                QVERIFY(!find<QCheckBox>(u"telemetry"_s)->isChecked()); // mặc định tắt
            key(Qt::Key_Return);
        }
        QVERIFY(closed());
        QVERIFY(spinUntil([&] { return !onb_ && (tour = win_->findChild<QuickTour*>()) && tour->isRunning(); }));
        QVERIFY(spinUntil([&] { return tour->card()->isAncestorOf(QApplication::focusWidget()); }));
        key(Qt::Key_Escape);
        QVERIFY(spinUntil([&] { return !tour; }));
        QCOMPARE(state_->get(shell::kTelemetryLevel), u"off"_s);
        QCOMPARE(sent_.size(), 0);

        // Đối chứng: đồng ý ở bước Quyền riêng tư → mức basic, đúng một gói lúc kết thúc; bỏ đồng ý → off, không gửi.
        QVERIFY(open());
        onb_->next();
        onb_->next();
        auto* box = find<QCheckBox>(u"telemetry"_s);
        QVERIFY(box && box->isVisible() && !box->isChecked());
        QTest::mouseClick(box, Qt::LeftButton, {}, QPoint(6, box->height() / 2));
        QVERIFY(box->isChecked());
        QCOMPARE(state_->get(shell::kTelemetryLevel), u"basic"_s);
        onb_->skip();
        QVERIFY(closed());
        QCOMPARE(sent_.size(), 1);
        QCOMPARE(sent_[0].value(u"event"_s).toString(), u"onboarding"_s);
        QCOMPARE(sent_[0].value(u"result"_s).toString(), u"skipped"_s);
        QCOMPARE(sent_[0].value(u"step"_s).toString(), u"privacy"_s);
        QVERIFY(open());
        onb_->next();
        onb_->next();
        box = find<QCheckBox>(u"telemetry"_s);
        QVERIFY(box->isChecked()); // đọc lại theo cài đặt
        box->click();
        QCOMPARE(state_->get(shell::kTelemetryLevel), u"off"_s);
        onb_->skip();
        QVERIFY(closed());
        QCOMPARE(sent_.size(), 1);
        // Mức "full" (UI-L09) giữ nguyên khi chỉ đi qua bước này.
        state_->set(shell::kTelemetryLevel, u"full"_s);
        QVERIFY(open());
        onb_->next();
        onb_->next();
        QVERIFY(find<QCheckBox>(u"telemetry"_s)->isChecked());
        onb_->skip();
        QVERIFY(closed());
        QCOMPARE(state_->get(shell::kTelemetryLevel), u"full"_s);
    }

    // Tiêu chí 3: bỏ qua ở bất kỳ bước nào (nút Bỏ qua, Esc, nút Đóng) vào thẳng ứng dụng; lần chạy sau không hiện;
    // start() chạy lại từ bước 1.
    void skipAnyStep_data()
    {
        QTest::addColumn<int>("step");
        QTest::addColumn<QString>("how");
        const char* const hows[] = {"button", "escape", "close"};
        for (int step = 0; step < 5; ++step)
            for (const char* how : hows)
                QTest::addRow("%d-%s", step + 1, how) << step << QString::fromLatin1(how);
    }
    void skipAnyStep()
    {
        QFETCH(int, step);
        QFETCH(QString, how);
        QVERIFY(build());
        QVERIFY(Onboarding::needed(*state_));
        QVERIFY(open());
        QCOMPARE(onb_->stepIds(), (QStringList{u"lang"_s, u"theme"_s, u"privacy"_s, u"edition"_s, u"tour"_s}));
        for (int i = 0; i < step; ++i)
            key(Qt::Key_Return);
        QCOMPARE(onb_->currentStep(), step);
        QSignalSpy done(onb_.data(), &Onboarding::completed);
        if (how == u"button"_s)
            QTest::mouseClick(find(u"skip"_s), Qt::LeftButton);
        else if (how == u"escape"_s)
            key(Qt::Key_Escape);
        else
            QTest::mouseClick(closeButton(), Qt::LeftButton);
        QCOMPARE(done.size(), 1);
        QCOMPARE(done[0][0].toBool(), true);
        QVERIFY(closed());
        QVERIFY(mainHasFocus());
        QCOMPARE(win_->findChild<QuickTour*>(), nullptr); // bỏ qua: không chạy hướng dẫn
        QVERIFY(!Onboarding::needed(*state_));
        state_->flush();
        const settings::Store again(statePath_);
        QVERIFY(!Onboarding::needed(again));
        // "Xem lại" (Tùy chọn): từ bước 1.
        QVERIFY(open());
        QCOMPARE(onb_->currentStep(), 0);
        QCOMPARE(find<QLabel>(u"step"_s)->text(), stepText(1, 5, "Ngôn ngữ"));
        QVERIFY(!find(u"back"_s)->isEnabled());
        onb_->skip();
        QVERIFY(closed());
    }

    // Tiêu chí 4: hướng dẫn nhanh đi hết bằng bàn phím; vùng tô sáng theo prototype (vùng vẽ, ribbon, palette Thuộc
    // tính, thanh trạng thái, dòng lệnh), có tên cho trình đọc màn hình.
    void tourKeyboard()
    {
        QAccessible::installUpdateHandler(record);
        QAccessible::setActive(true);
        QVERIFY(build());
        QVERIFY(open());
        QSignalSpy sample(onb_.data(), &Onboarding::sampleRequested);
        for (int i = 0; i < 4; ++i)
            key(Qt::Key_Return);
        QCOMPARE(onb_->currentStep(), 4);
        QCOMPARE(find(u"next"_s)->text(), Onboarding::tr("Bắt đầu hướng dẫn"));
        QPointer<QuickTour> tour;
        connect(onb_, &Onboarding::sampleRequested, this, [&] { QCOMPARE(tour, nullptr); });
        key(Qt::Key_Return);
        QCOMPARE(sample.size(), 1);
        QVERIFY(spinUntil([&] { return (tour = win_->findChild<QuickTour*>()) && tour->isRunning(); }));
        QSignalSpy finished(tour.data(), &QuickTour::finished);

        QWidget* const expected[] = {win_->region(Region::Canvas), win_->region(Region::Ribbon),
                                     win_->palettes().dock(u"props"_s), win_->region(Region::StatusBar),
                                     win_->region(Region::CommandLine)};
        const QString names[] = {MainWindow::regionName(Region::Canvas), MainWindow::regionName(Region::Ribbon),
                                 QuickTour::tr("Palette Thuộc tính"), MainWindow::regionName(Region::StatusBar),
                                 MainWindow::regionName(Region::CommandLine)};
        auto* next = find(u"next"_s, tour->card());
        auto* end = find(u"end"_s, tour->card());
        for (int i = 0; i < QuickTour::kStops; ++i) {
            QCOMPARE(tour->current(), i);
            QVERIFY(expected[i] && expected[i]->isVisible());
            QCOMPARE(tour->target(), expected[i]);
            QCOMPARE(tour->targetName(), names[i]);
            QVERIFY(!names[i].isEmpty());
            QVERIFY(spinUntil([&] { return next->hasFocus(); }));
            // Tên vùng: thông báo của bước và mô tả của nút Tiếp.
            const auto said =
                std::find_if(g_said.rbegin(), g_said.rend(), [&](const auto& s) { return s.first == tour->card(); });
            QVERIFY(said != g_said.rend());
            QVERIFY2(said->second.contains(names[i]), qPrintable(said->second));
            QCOMPARE(said->second, next->accessibleDescription());
            if (i == 1) { // ← về bước trước, → tới lại
                key(Qt::Key_Left);
                QCOMPARE(tour->current(), 0);
                key(Qt::Key_Right);
                QCOMPARE(tour->current(), 1);
                QVERIFY(next->hasFocus());
            }
            if (i == 2) { // Shift+Tab tới Kết thúc, Tab về Tiếp
                key(Qt::Key_Tab, Qt::ShiftModifier);
                QVERIFY(end->hasFocus());
                key(Qt::Key_Tab);
                QVERIFY(next->hasFocus());
                QCOMPARE(tour->current(), 2);
            }
            if (i == 3) { // ↑ ↓ như ← →
                key(Qt::Key_Up);
                QCOMPARE(tour->current(), 2);
                key(Qt::Key_Down);
                QCOMPARE(tour->current(), 3);
            }
            QCOMPARE(next->text(), i == QuickTour::kStops - 1 ? QuickTour::tr("Xong") : QuickTour::tr("Tiếp"));
            key(Qt::Key_Return);
        }
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished[0][0].toBool(), true);
        QVERIFY(spinUntil([&] { return !tour; })); // tự hủy
        QVERIFY(mainHasFocus());
        QVERIFY(win_->region(Region::Canvas)->hasFocus());

        // Esc kết thúc giữa chừng; Shift+Tab rồi Enter trên Kết thúc cũng vậy.
        for (const bool viaEnd : {false, true}) {
            tour = runTour();
            QSignalSpy ended(tour.data(), &QuickTour::finished);
            QVERIFY(spinUntil([&] { return find(u"next"_s, tour->card())->hasFocus(); }));
            key(Qt::Key_Right);
            QCOMPARE(tour->current(), 1);
            if (viaEnd) {
                key(Qt::Key_Tab, Qt::ShiftModifier);
                key(Qt::Key_Return);
            } else {
                key(Qt::Key_Escape);
            }
            QCOMPARE(ended.size(), 1);
            QCOMPARE(ended[0][0].toBool(), false);
            QVERIFY(spinUntil([&] { return !tour; }));
            QVERIFY(win_->region(Region::Canvas)->hasFocus());
        }
    }

    // Lớp phủ: không chặn chuột (vùng vẽ nhận cú nhấp), --c-scrim phủ ngoài vùng, viền 2 px --c-accent sát ngoài vùng,
    // trong vùng giữ nguyên (ảnh không chữ, Tối và Sáng).
    void overlay()
    {
        QVERIFY(build());
        QWidget* canvas = win_->region(Region::Canvas);
        const QRect r(canvas->mapTo(win_.get(), QPoint()), canvas->size());
        for (const theme::Theme t : {theme::Theme::Dark, theme::Theme::Light}) {
            tm_->setTheme(t);
            QCoreApplication::sendPostedEvents();
            const QImage before = win_->grab().toImage();
            QPointer<QuickTour> tour = runTour();
            QCOMPARE(tour->target(), canvas);
            QCoreApplication::sendPostedEvents();
            const QImage after = win_->grab().toImage();
            const auto at = [](const QImage& img, QPoint p) {
                return QColor(img.pixel((QPointF(p) * img.devicePixelRatio()).toPoint()));
            };
            const QPoint outside(r.center().x(), r.top() - 10);
            const QPoint ring(r.center().x(), r.top() - 2);
            const QPoint gap(r.center().x(), r.top() - 1);
            const QPoint inside(r.right() - 30, r.bottom() - 30);
            QVERIFY(!tour->card()->geometry().contains(inside));
            QVERIFY2(near(at(after, outside), over(col(tok::c_scrim), at(before, outside))),
                     qPrintable(at(after, outside).name()));
            QCOMPARE(at(after, ring), col(tok::c_accent));
            QCOMPARE(at(after, gap), at(before, gap));
            QCOMPARE(at(after, inside), at(before, inside));
            // Không chặn chuột: cú nhấp qua cửa sổ tới vùng vẽ (lấy focus), thẻ vẫn hiện.
            QVERIFY(tour->overlay()->testAttribute(Qt::WA_TransparentForMouseEvents));
            QVERIFY(spinUntil([&] { return tour->card()->isAncestorOf(QApplication::focusWidget()); }));
            QTest::mouseClick(win_->windowHandle(), Qt::LeftButton, {}, inside);
            QVERIFY(spinUntil([&] { return canvas->hasFocus(); }));
            QVERIFY(tour->isRunning() && tour->card()->isVisible());
            tour->finish();
            QVERIFY(spinUntil([&] { return !tour; }));
            QVERIFY(win_->findChildren<QWidget*>(u"quickTour"_s).isEmpty());
        }
    }

    // Màn hình hẹp: palette Thuộc tính thu ở dải, bước 3 tô sáng tab của nó; vùng ẩn (ribbon tắt) thì bỏ qua bước đó.
    void stripFallback()
    {
        QVERIFY(build(ui::ScreenClass::Small));
        QDockWidget* props = win_->palettes().dock(u"props"_s);
        QVERIFY(props && !props->isVisible());
        const ui::AutoHideStrip* strip = win_->palettes().strip(Qt::RightDockWidgetArea);
        QVERIFY(strip && strip->contains(props));
        win_->setRegionVisible(Region::Ribbon, false);
        QPointer<QuickTour> tour = runTour();
        QCOMPARE(tour->current(), 0);
        tour->next(); // ribbon ẩn: bỏ qua
        QCOMPARE(tour->current(), 2);
        QCOMPARE(tour->target(), strip->tab(props));
        tour->previous();
        QCOMPARE(tour->current(), 0);
        tour->finish();
        QVERIFY(spinUntil([&] { return !tour; }));
    }

    // Khung bước mở rộng được (UIA-117 chèn bước không sửa bước cũ); bước có điều kiện hiện.
    void steps()
    {
        QVERIFY(build());
        auto* onb = new Onboarding(*win_, *state_);
        onb_ = onb;
        onb->setAttribute(Qt::WA_DeleteOnClose);
        bool autocad = false;
        const auto page = [] { return new QLabel(u"Ribbon"_s); }; // i18n-ok: trang mẫu của test
        onb->addStep({u"ui"_s, [] { return u"Kiểu giao diện"_s; }, page}, u"privacy"_s);
        onb->addStep({u"migrate"_s, [] { return u"Chuyển từ AutoCAD"_s; }, page, [&] { return autocad; }},
                     u"privacy"_s);
        onb->addStep({u"units"_s, [] { return u"Đơn vị và mẫu"_s; }, page}, u"privacy"_s);
        onb->start();
        QVERIFY(waitActive(onb));
        QCOMPARE(onb->stepIds(),
                 (QStringList{u"lang"_s, u"theme"_s, u"ui"_s, u"units"_s, u"privacy"_s, u"edition"_s, u"tour"_s}));
        onb->next();
        onb->next();
        QCOMPARE(onb->currentStep(), 2);
        QVERIFY(onb->page(u"ui"_s)->isVisible());
        QCOMPARE(find<QLabel>(u"step"_s)->text(),
                 Onboarding::tr("Bước %1 / %2 · %3").arg(u"3"_s, u"7"_s, u"Kiểu giao diện"_s));
        autocad = true; // máy có AutoCAD: bước hiện ở lần chạy sau
        onb->start();
        QCOMPARE(onb->currentStep(), 0);
        QCOMPARE(onb->stepIds().indexOf(u"migrate"_s), 3);
        QCOMPARE(onb->stepIds().size(), 8);
        // Đã có giấy phép: không có bước Phiên bản.
        gate_->setEdition(edition::Edition::Pro);
        onb->start();
        QVERIFY(!onb->stepIds().contains(u"edition"_s));
        // Thay bước dựng sẵn (UIA-117: Phiên bản đủ) không sửa bước cũ.
        gate_->setEdition(edition::Edition::Viewer);
        onb->removeStep(u"edition"_s);
        onb->addStep({u"edition-full"_s, [] { return u"Phiên bản"_s; }, page}, u"tour"_s);
        onb->start();
        QCOMPARE(onb->stepIds().mid(5), (QStringList{u"privacy"_s, u"edition-full"_s, u"tour"_s}));
        QCOMPARE(onb->editionChoice(), Onboarding::EditionChoice::Viewer);
        onb->skip();
        QVERIFY(closed());
    }

    // Bước Phiên bản: dùng thử, key phát tín hiệu lúc kết thúc (cả khi bỏ qua ở bước sau); Viewer: không phát.
    void editionChoice()
    {
        QVERIFY(build());
        for (const char* choice : {"trial", "key", "viewer"}) {
            QVERIFY(open());
            QSignalSpy trial(onb_.data(), &Onboarding::trialRequested);
            QSignalSpy activate(onb_.data(), &Onboarding::activationRequested);
            for (int i = 0; i < 3; ++i)
                onb_->next();
            QCOMPARE(onb_->stepIds().value(onb_->currentStep()), u"edition"_s);
            QVERIFY(find(u"viewer"_s)->isChecked()); // mặc định
            QTest::mouseClick(find(QString::fromLatin1(choice)), Qt::LeftButton);
            onb_->next(); // bước Hướng dẫn nhanh
            onb_->skip();
            QCOMPARE(trial.size(), QString::fromLatin1(choice) == u"trial"_s ? 1 : 0);
            QCOMPARE(activate.size(), QString::fromLatin1(choice) == u"key"_s ? 1 : 0);
            QVERIFY(closed());
        }
        // Mũi tên đổi lựa chọn trong nhóm (nhóm radio).
        QVERIFY(open());
        for (int i = 0; i < 3; ++i)
            onb_->next();
        auto* viewer = find(u"viewer"_s);
        viewer->setFocus(Qt::TabFocusReason);
        key(Qt::Key_Down);
        QVERIFY(find(u"trial"_s)->isChecked() && find(u"trial"_s)->hasFocus());
        QCOMPARE(onb_->editionChoice(), Onboarding::EditionChoice::Trial);
        key(Qt::Key_Return); // Enter trên thẻ: Tiếp (nút mặc định)
        QCOMPARE(onb_->currentStep(), 4);
        QVERIFY(find(u"next"_s)->hasFocus()); // focus không rơi vào Bỏ qua khi trang cũ ẩn
        onb_->skip();
        QVERIFY(closed());
    }

    // UI0-013: mỗi bước, phần tử nhận focus có tên và vai trò, Tab đi hết vòng có viền focus.
    void a11y()
    {
        QVERIFY(build());
        QVERIFY(open());
        for (int i = 0; i < 5; ++i) {
            QVERIFY(waitActive(onb_));
            QStringList order;
            const QStringList issues = trion::a11y::checkNames(onb_) + trion::a11y::checkFocus(onb_, &order);
            QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
            qInfo("bước %d: %s", i + 1, qPrintable(order.join(u" → "_s)));
            // Vòng Tab (bắt đầu sau Tiếp): × → nội dung trang → Bỏ qua → Quay lại (từ bước 2) → Tiếp.
            const qsizetype n = order.indexOf(u"trion::ui::Button 'next'"_s);
            QVERIFY(n >= 0);
            const QStringList cycle = order.mid(n + 1) + order.mid(0, n + 1);
            const qsizetype skip = cycle.indexOf(u"trion::ui::Button 'skip'"_s);
            const qsizetype back = cycle.indexOf(u"trion::ui::Button 'back'"_s);
            QVERIFY(skip > 0);
            QCOMPARE(back >= 0, i > 0);
            QVERIFY(back < 0 || back == skip + 1);
            qsizetype content = 0;
            for (qsizetype k = 0; k < cycle.size(); ++k)
                if (!cycle[k].startsWith(u"trion::ui::Button"_s)) {
                    QVERIFY2(k < skip, qPrintable(cycle[k]));
                    ++content;
                }
            QCOMPARE(content, i < 4 ? 1 : 0); // nhóm thẻ: một điểm dừng; ô đồng ý; trang Hướng dẫn không có
            onb_->next();
        }
        QVERIFY(closed()); // bước cuối: hướng dẫn chạy
        QPointer<QuickTour> tour = win_->findChild<QuickTour*>();
        QVERIFY(tour);
        const QStringList names = trion::a11y::checkNames(tour->card());
        QVERIFY2(names.isEmpty(), qPrintable(names.join(u'\n')));
        tour->finish();
    }

    // vi-XL (chuỗi dài hơn 40%): không chữ nào bị cắt ở mọi bước và mọi thẻ hướng dẫn, cả hai mật độ.
    void pseudoNoTruncation()
    {
        QVERIFY(build());
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LanguageChange);
        for (const theme::Density d : {theme::Density::Comfortable, theme::Density::Compact}) {
            tm_->setDensity(d);
            QVERIFY(open());
            for (int i = 0; i < 5; ++i) {
                spinFor(10ms); // bố cục lại theo trang mới
                const QString cut = truncations(onb_);
                QVERIFY2(cut.isEmpty(), qPrintable(u"bước %1: %2"_s.arg(i + 1).arg(cut)));
                if (i < 4)
                    onb_->next();
            }
            onb_->next(); // Bắt đầu hướng dẫn
            QVERIFY(closed());
            QPointer<QuickTour> tour = win_->findChild<QuickTour*>();
            QVERIFY(tour && tour->isRunning());
            for (int i = 0; i < QuickTour::kStops; ++i) {
                QCOMPARE(tour->current(), i);
                QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
                const QString cut = truncations(tour->card());
                QVERIFY2(cut.isEmpty(), qPrintable(cut));
                tour->next();
            }
            QVERIFY(spinUntil([&] { return !tour; }));
        }
    }

    // DoD-L: mở hộp (dựng, trang đầu, khung đầu) ≤ 150 ms CPU luồng UI (release).
    void openTime()
    {
        QVERIFY(build());
        const auto cpu = threadCpu();
        onb_ = new Onboarding(*win_, *state_);
        onb_->setAttribute(Qt::WA_DeleteOnClose);
        onb_->start();
        QCoreApplication::sendPostedEvents();
        onb_->repaint();
        const double open = ms(threadCpu() - cpu);
        qInfo("mở hộp: %.1f ms CPU luồng UI", open);
        QVERIFY2(!kTimed || threadCpu().count() < 0 || open <= 150, qPrintable(QString::number(open)));
        onb_->skip();
        QVERIFY(closed());
    }

    // Rảnh không timer: hộp đang mở và hướng dẫn đang chạy không có QTimer nào chạy.
    void idle()
    {
        QVERIFY(build());
        QVERIFY(open());
        spinFor(20ms);
        for (const QTimer* t : onb_->findChildren<QTimer*>())
            QVERIFY2(!t->isActive(), qPrintable(t->objectName()));
        for (int i = 0; i < 5; ++i)
            onb_->next();
        QVERIFY(closed());
        QPointer<QuickTour> tour = win_->findChild<QuickTour*>();
        QVERIFY(tour);
        spinFor(20ms);
        for (const QTimer* t : tour->card()->findChildren<QTimer*>())
            QVERIFY2(!t->isActive(), qPrintable(t->objectName()));
        QCOMPARE(tour->findChildren<QTimer*>().size(), 0);
        tour->finish();
    }
};

QTEST_MAIN(OnboardingTest)
#include "onboarding_test.moc"
