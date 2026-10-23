// UIA-001 · Cửa sổ chính, workspace, hành vi theo kích thước màn hình, Clean Screen (MainWindow.h, Workspace.h,
// CleanScreen.h, CommandRouter.h). Ô giữ chỗ cao theo token (mật độ gọn) cho mọi vùng chưa có widget thật. Màn hình giả
// (offscreen, như autohide_test): p100 1920×1080 100% (chính), p150 1920×1080 150% (1280×720 logic, Small), p200
// 1920×1080 200% (960×540), s1366 1366×768 100% (Small).
//   1  canvasShare: 1920×1080 Wide → ribbon đầy đủ, Thuộc tính dock mở, vùng vẽ ≈ 60% ±3%; 1366×768 (lớp truyền vào,
//      và cửa sổ trên màn hình 1366 thật) → palette ở dải tự ẩn, dòng lệnh 1 dòng. Ribbon giữ chỗ cao theo 3 mức thu gọn
//      của RibbonBar (số in ra): ≥ 70% [đề xuất] ở 1366×768 chỉ đạt ở mức chỉ tên tab; đầy đủ, chỉ tên panel
//      QEXPECT_FAIL kèm số (mức mặc định cho màn Small chờ chủ dự án quyết).
//   2  cleanScreen: Ctrl+0 và CommandRouter ('_CLEANSCREENON, ^C^C_CLEANSCREENOFF) cho cùng kết quả: ẩn tiêu đề,
//      ribbon, tab tệp, tab layout, palette (dock, nổi, dải, khung trượt), giữ dòng lệnh và thanh trạng thái; vùng vẽ
//      ≥ 75%; tắt thì hình học từng vùng, từng palette và ảnh chụp cửa sổ giống từng pixel. Thời gian bật/tắt.
//      router: cú pháp macro, sysvar trong suốt, lệnh không xử lý.
//   3  workspaces: mô tả JSON đọc đủ; Soạn thảo 2D ↔ Viewer, hồ sơ Viewer của EditionGate, lưu lựa chọn; đổi
//      workspace khi Clean Screen bật; thời gian đổi. restarts: 100 lần (release; debug/sanitizer 10) dựng lại với
//      cùng Store giữ workspace, dock, kích thước; phóng to; màn hình đã lưu; màn hình đã rút → màn hình chính.
//   4  screenClassRuntime: kéo cửa sổ sang màn hình 1366 rồi về: bố cục mặc định áp lại; người dùng đã đổi thì giữ.
//      dprChange: 100% → 150% → 200% → 100%: bố cục dựng lại đúng token, không cảnh báo. resizeLayout: thời gian
//      bố cục + vẽ mỗi lần đổi cỡ (khung 60 fps thật: QA-011).
//   5  titleBar (nhấp đúp phóng to, sự kiện từ widget con), setRegion, accessibility, pseudoNoTruncation (vi-XL),
//      idleNoTimers. nativeTitleBar: chỉ chạy với -platform windows (đo Qt::ExpandedClientAreaHint trên máy thật).
//   6  Integration nhóm 7: commandLineFloating (dòng lệnh nổi: rời cột, về đúng ô, kẹp khi vùng vẽ đổi cỡ, giữ chỗ
//      khi đổi widget vùng vẽ, Clean Screen, ẩn vùng, lưu và dựng lại với cùng Store), belowRibbon (hàng dưới ribbon
//      theo thanh tiêu đề, tự ẩn khi rỗng), paletteContent (nội dung thật thay ô giữ chỗ), layoutApplied.
#include "app/shell/CleanScreen.h"
#include "app/shell/CommandRouter.h"
#include "app/shell/MainWindow.h"
#include "app/shell/Workspace.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/core/display/DisplayContext.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/keymap/Keymap.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/dock/AutoHideStrip.h"
#include "app/widgets/dock/PaletteHost.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/sched/Scheduler.h"
#include "tests/timing.h"

#include <QAbstractButton>
#include <QAccessible>
#include <QApplication>
#include <QBoxLayout>
#include <QDockWidget>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMetaEnum>
#include <QPointer>
#include <QScreen>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <QWindow>

#include <qpa/qplatformnativeinterface.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <utility>

#if defined(__linux__)
#include <time.h>
#endif

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;
using namespace trion;
using trion::shell::CleanScreen;
using trion::shell::CommandRouter;
using trion::shell::MainWindow;
using trion::shell::Workspace;
using Region = MainWindow::Region;
using trion::ui::ScreenClass;

namespace {

constexpr int kLine = 1, kHistoryPad = 3, kInputTop = 3; // số prototype viết thẳng (app.css)
const QRect kFull(0, 0, 1920, 1080);                     // phủ màn hình p100
const QRect kSmall(0, 0, 1366, 768);                     // 1366×768 trên p100, lớp truyền vào
const QPoint kParked(1500, 1076); // con trỏ trên thanh trạng thái giữ chỗ (không hiệu ứng rê)

struct Screen {
    const char* name;
    int x, width, height, dpi;
};
constexpr Screen kScreens[] = {{"p100", 0, 1920, 1080, 96},
                               {"p150", 2000, 1920, 1080, 144},
                               {"p200", 4000, 1920, 1080, 192},
                               {"s1366", 6000, 1366, 768, 96}};
QString g_cwd;

QJsonObject screens(qsizetype count)
{
    QJsonArray list;
    for (qsizetype i = 0; i < count; ++i)
        list.append(QJsonObject{{u"name"_s, QLatin1StringView(kScreens[i].name)},
                                {u"x"_s, kScreens[i].x},
                                {u"y"_s, 0},
                                {u"width"_s, kScreens[i].width},
                                {u"height"_s, kScreens[i].height},
                                {u"logicalDpi"_s, kScreens[i].dpi},
                                {u"logicalBaseDpi"_s, 96},
                                {u"dpr"_s, 1}});
    return QJsonObject{{u"screens"_s, list}};
}

// Cắm, rút màn hình giả lúc đang chạy (cấu hình mới của plugin offscreen; như autohide_test).
bool setScreens(qsizetype count)
{
    QPlatformNativeInterface* native = QGuiApplication::platformNativeInterface();
    using Set = void (*)(const QJsonObject&, QPlatformNativeInterface*);
    const auto set = reinterpret_cast<Set>(native->nativeResourceForIntegration("setConfiguration"));
    if (!set)
        return false;
    set(screens(count), native);
    return QGuiApplication::screens().size() == count;
}

QScreen* screenNamed(const char* name)
{
    for (QScreen* s : QGuiApplication::screens())
        if (s->name() == QLatin1StringView(name))
            return s;
    return nullptr;
}

bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 5s)
{
    if (cond())
        return true;
    QEventLoop loop; // timer trên stack: chờ xong không còn timer nào chạy tiếp (idleNoTimers)
    QTimer tick, limit;
    QObject::connect(&tick, &QTimer::timeout, &loop, [&] {
        if (cond())
            loop.quit();
    });
    limit.setSingleShot(true);
    QObject::connect(&limit, &QTimer::timeout, &loop, &QEventLoop::quit);
    limit.start(timeout);
    tick.start(1);
    loop.exec();
    return cond();
}

void settle()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
}

// Thời gian CPU của luồng gọi (Linux); nơi khác: đồng hồ thật (GetThreadTimes của Windows nhảy theo nhịp 15,6 ms).
double nowMs()
{
#if defined(__linux__)
    timespec t{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return double(t.tv_sec) * 1e3 + double(t.tv_nsec) / 1e6;
#else
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}

double percentile(std::vector<double> v, double p)
{
    std::sort(v.begin(), v.end());
    return v.empty() ? 0 : v[std::min(v.size() - 1, size_t(p * double(v.size())))];
}

bool onScreen(const QWidget* w)
{
    return w && w->isVisible() && !w->visibleRegion().isEmpty();
}

QString text(const QRect& r)
{
    return u"%1,%2 %3x%4"_s.arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
}

class TimerCounter : public QObject {
public:
    const QObject* skip = nullptr;
    QStringList seen;

protected:
    bool eventFilter(QObject* o, QEvent* e) override
    {
        if (e->type() == QEvent::Timer && o != skip)
            seen << QString::fromLatin1(o->metaObject()->className());
        return false;
    }
};

} // namespace

class ShellTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;  // chủ đề, ngôn ngữ
    std::unique_ptr<settings::Store> window_; // khóa của cửa sổ, palette: mỗi test một kho mới
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<theme::ThemeManager> tm_;
    std::unique_ptr<theme::IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<keymap::Keymap> keymap_;
    std::unique_ptr<ui::RichTooltip> tips_;
    std::unique_ptr<actions::Sysvars> sysvars_;
    std::unique_ptr<actions::ActionRegistry> actions_;
    std::unique_ptr<CommandRouter> router_;
    std::unique_ptr<MainWindow> win_;
    std::unique_ptr<ui::DisplayContext> display_;
    CleanScreen* clean_ = nullptr;
    int stores_ = 0;
    std::function<void()> setup_; // build(): sau khi dựng cửa sổ, trước restore() (vùng thật, nối tín hiệu)

    int px(tok::PxId id) const { return qRound(tm_->px(id)); }
    // Cao mong đợi của vùng (prototype app.css), tính độc lập với MainWindow.cpp.
    int expectedHeight(Region r, bool compact = false) const
    {
        switch (r) {
        case Region::TitleBar:
            return px(tok::h_title);
        case Region::Ribbon:
            return px(tok::h_tab) + 3 * px(tok::h_btn) + 8 + px(tok::h_lbl) + kLine;
        case Region::FileTabs:
            return px(tok::h_ftab);
        case Region::LayoutTabs:
            return px(tok::h_ltab);
        case Region::CommandLine:
            return kLine + (compact ? 1 : 3) * px(tok::h_cmdl) + kHistoryPad + kInputTop + px(tok::h_cmdin) + 6;
        case Region::StatusBar:
            return px(tok::h_status);
        case Region::Canvas:
            break;
        }
        return 0;
    }

    ui::PaletteHost& host() const { return win_->palettes(); }
    QDockWidget* dock(const char* id) const { return host().dock(QString::fromLatin1(id)); }
    ui::AutoHideStrip* strip(Qt::DockWidgetArea side) const { return host().strip(side); }
    QStringList stripIds(Qt::DockWidgetArea side) const
    {
        QStringList ids;
        if (const ui::AutoHideStrip* s = strip(side); s && s->isVisible())
            for (const QDockWidget* d : s->palettes())
                ids << d->objectName();
        return ids;
    }
    QWidget* canvas() const { return win_->region(Region::Canvas); }
    double share() const
    {
        settle();
        return double(canvas()->width()) * canvas()->height() / (double(win_->width()) * win_->height());
    }

    // Cửa sổ chính với wiring như Application (UIA-001): Clean Screen, ActionRegistry → CommandRouter. Khôi phục từ
    // window_ (Store), rồi geometry (nếu có, trạng thái thường) và lớp màn hình (nếu có); hiện, active.
    bool build(std::optional<QRect> geometry = kFull, std::optional<ScreenClass> cls = {}, bool track = false)
    {
        win_ = std::make_unique<MainWindow>(*window_);
        clean_ = new CleanScreen(*win_, *sysvars_, *router_);
        actions_->install(win_.get());
        actions_->addViewport(canvas());
        if (setup_)
            setup_();
        win_->restore();
        if (geometry) {
            win_->setWindowState(Qt::WindowNoState);
            win_->setGeometry(*geometry);
        }
        if (cls)
            win_->setScreenClass(*cls);
        if (track) {
            display_ = std::make_unique<ui::DisplayContext>(win_.get());
            win_->setScreenClass(display_->screenClass());
            connect(display_.get(), &ui::DisplayContext::screenClassChanged, win_.get(), &MainWindow::setScreenClass);
            host().trackDisplay(*display_);
            tm_->setReducedMotion(true); // DisplayContext áp "giảm chuyển động" của máy chạy test
        }
        win_->show();
        if (!QTest::qWaitForWindowActive(win_.get()))
            return false;
        settle();
        return true;
    }

    // Đóng (lưu vào window_) và hủy cửa sổ.
    void closeWindow()
    {
        if (!win_)
            return;
        win_->close();
        display_.reset();
        win_.reset();
        clean_ = nullptr;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    // Hình học từng vùng, từng palette, dải, khung trượt (tọa độ cửa sổ; palette nổi: toàn cục).
    QStringList snapshot() const
    {
        settle();
        QStringList s;
        const QMetaEnum names = QMetaEnum::fromType<Region>();
        for (int i = 0; i < MainWindow::kRegionCount; ++i) {
            const QWidget* w = win_->region(Region(i));
            s << u"%1 %2 %3"_s.arg(QLatin1StringView(names.valueToKey(i)))
                     .arg(w->isVisible())
                     .arg(w->isVisible() ? text(QRect(w->mapTo(win_.get(), QPoint()), w->size())) : QString());
        }
        for (const QString& id : host().ids()) {
            const QDockWidget* d = host().dock(id);
            const QRect r = !onScreen(d)      ? QRect()
                            : d->isFloating() ? d->geometry()
                                              : QRect(d->mapTo(win_.get(), QPoint()), d->size());
            s << u"%1 %2 %3 %4"_s.arg(id).arg(onScreen(d)).arg(d->isFloating()).arg(text(r));
        }
        for (const Qt::DockWidgetArea side :
             {Qt::LeftDockWidgetArea, Qt::RightDockWidgetArea, Qt::BottomDockWidgetArea})
            if (const ui::AutoHideStrip* st = strip(side); st && st->isVisible()) // dải ẩn = chưa có dải
                s << u"strip %1 %2 flyout %3"_s.arg(int(side)).arg(text(st->geometry())).arg(st->flyout()->isVisible());
        return s;
    }
    QImage shot() const
    {
        settle();
        return win_->grab().toImage();
    }

public:
    ShellTest() { QDir::setCurrent(g_cwd); }

    // Trước QApplication (QTEST_MAIN): co giãn DPI bật lại (môi trường test chung tắt), bốn màn hình giả.
    // TRION_NATIVE_PROBE=1: dùng nền tảng thật (nativeTitleBar với -platform windows).
    static void initMain()
    {
        g_cwd = QDir::currentPath();
        if (qEnvironmentVariableIsSet("TRION_NATIVE_PROBE"))
            return;
        for (const char* v : {"QT_ENABLE_HIGHDPI_SCALING", "QT_SCALE_FACTOR", "QT_SCREEN_SCALE_FACTORS",
                              "QT_SCALE_FACTOR_ROUNDING_POLICY"})
            qunsetenv(v);
        ui::DisplayContext::initMain();
        static QTemporaryDir config; // đường dẫn tương đối: tùy chọn của plugin ngăn cách bằng ':'
        QFile f(config.filePath(u"screens.json"_s));
        const QByteArray json = QJsonDocument(screens(std::size(kScreens))).toJson();
        if (!config.isValid() || !f.open(QIODevice::WriteOnly) || f.write(json) != json.size() ||
            !QDir::setCurrent(config.path()))
            qFatal("không ghi được cấu hình màn hình");
        qputenv("QT_QPA_PLATFORM", "offscreen:configfile=screens.json");
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        QVERIFY(QDir(dir_.path()).mkpath(u"config"_s)); // "giảm chuyển động" Linux đọc $XDG_CONFIG_HOME
        qputenv("XDG_CONFIG_HOME", QFile::encodeName(dir_.filePath(u"config"_s)));
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        QVERIFY(theme::loadFonts());
        executor_ = std::make_unique<ui::UiExecutor>();
        tm_ = std::make_unique<theme::ThemeManager>(*store_);
        icons_ = std::make_unique<theme::IconProvider>();
        translator_ = std::make_unique<i18n::Translator>();
        keymap_ = std::make_unique<keymap::Keymap>(*store_, keymap::Platform::Windows);
        tips_ = std::make_unique<ui::RichTooltip>();
        tips_->setKeymap(keymap_.get());
        sysvars_ = std::make_unique<actions::Sysvars>();
        actions_ = std::make_unique<actions::ActionRegistry>(*keymap_, *sysvars_);
        router_ = std::make_unique<CommandRouter>(*sysvars_);
        actions_->setSink([r = router_.get()](const QString& m) { r->submit(m); });
        QApplication::setCursorFlashTime(0);
        QApplication::setQuitOnLastWindowClosed(false); // đóng cửa sổ chính trong test không thoát
        if (!qEnvironmentVariableIsSet("TRION_NATIVE_PROBE")) {
            QCOMPARE(QGuiApplication::screens().size(), qsizetype(std::size(kScreens)));
            QCOMPARE(screenNamed("p150")->devicePixelRatio(), 1.5);
        }
    }

    void init()
    {
        tm_->setTheme(theme::Theme::Dark);
        tm_->setDensity(theme::Density::Compact);
        tm_->setReducedMotion(true);
        window_ = std::make_unique<settings::Store>(dir_.filePath(u"window-%1.json"_s.arg(++stores_)));
        if (!qEnvironmentVariableIsSet("TRION_NATIVE_PROBE")) // máy thật: không dời chuột của người dùng
            QCursor::setPos(kParked);
    }

    void cleanup()
    {
        if (QWidget* p = QApplication::activePopupWidget())
            p->close();
        closeWindow();
        setup_ = nullptr;
        window_.reset();
        if (!qEnvironmentVariableIsSet("TRION_NATIVE_PROBE") &&
            QGuiApplication::screens().size() != qsizetype(std::size(kScreens)))
            QVERIFY(setScreens(std::size(kScreens)));
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QCoreApplication::processEvents();
    }

    void cleanupTestCase()
    {
        closeWindow();
        window_.reset();
        router_.reset();
        actions_.reset();
        sysvars_.reset();
        tips_.reset();
        keymap_.reset();
        translator_.reset();
        icons_.reset();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        sched::waitIdle();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    // Tiêu chí 1.
    void canvasShare_data()
    {
        QTest::addColumn<QRect>("geometry");
        QTest::addColumn<bool>("wide");
        QTest::addColumn<bool>("track"); // lớp theo màn hình thật (DisplayContext), không truyền vào
        QTest::addRow("1920x1080") << kFull << true << false;
        QTest::addRow("1366x768") << kSmall << false << false;
        QTest::addRow("1366x768-screen") << QRect(6000, 0, 1366, 768) << false << true;
    }

    void canvasShare()
    {
        QFETCH(QRect, geometry);
        QFETCH(bool, wide);
        QFETCH(bool, track);
        QVERIFY(build(geometry, track ? std::nullopt : std::optional(wide ? ScreenClass::Wide : ScreenClass::Small),
                      track));
        QCOMPARE(win_->size(), geometry.size());
        QCOMPARE(win_->screenClass(), wide ? ScreenClass::Wide : ScreenClass::Small);
        for (int i = 0; i < MainWindow::kRegionCount; ++i) // mọi vùng cao đúng token, xếp liền nhau
            if (Region(i) != Region::Canvas)
                QCOMPARE(win_->region(Region(i))->height(), expectedHeight(Region(i), !wide));
        QCOMPARE(win_->region(Region::CommandLine)->property("compact").toBool(), !wide);
        QCOMPARE(win_->region(Region::StatusBar)->geometry().bottom(), win_->height() - 1);
        QCOMPARE(stripIds(Qt::LeftDockWidgetArea), (QStringList{u"tcvnlib"_s, u"blocks"_s}));
        if (wide) { // Thuộc tính dock mở rộng --pal-w; Layer, Bác sĩ Font cùng nhóm tab
            QVERIFY(host().isPinned(u"props"_s));
            QVERIFY(onScreen(dock("props")));
            QCOMPARE(dock("props")->width(), px(tok::pal_w));
            QCOMPARE(win_->dockHost()->tabifiedDockWidgets(dock("props")),
                     (QList<QDockWidget*>{dock("layers"), dock("fontdoc")}));
            QVERIFY(!onScreen(dock("layers")));
            QCOMPARE(stripIds(Qt::RightDockWidgetArea), QStringList());
        } else { // hẹp hơn Wide: mọi palette tự ẩn ở dải
            QCOMPARE(stripIds(Qt::RightDockWidgetArea), (QStringList{u"props"_s, u"layers"_s, u"fontdoc"_s}));
            for (const QString& id : host().ids())
                QVERIFY2(!onScreen(host().dock(id)) && !host().isPinned(id), qPrintable(id));
        }
        int top = 0, bottom = 0;
        for (const Region r : {Region::TitleBar, Region::FileTabs})
            top += expectedHeight(r);
        for (const Region r : {Region::LayoutTabs, Region::CommandLine, Region::StatusBar})
            bottom += expectedHeight(r, !wide);
        // Ribbon giữ chỗ cao theo 3 mức thu gọn của RibbonBar (UIA-005, bodyHeight trong RibbonBar.cpp): dải tab, thân
        // (đầy đủ: 3 hàng nút, --sp-4, tên panel; chỉ tên panel: tên panel, --sp-4; chỉ tên tab: không có), nét dưới.
        const int tabs = px(tok::h_tab) + kLine;
        const std::pair<const char*, int> levels[] = {
            {"Full", tabs + 3 * px(tok::h_btn) + px(tok::sp_4) + px(tok::h_lbl)},
            {"PanelTitles", tabs + px(tok::h_lbl) + px(tok::sp_4)},
            {"TabNames", tabs}};
        QCOMPARE(levels[0].second, expectedHeight(Region::Ribbon)); // ô giữ chỗ của MainWindow: mức đầy đủ
        for (const auto& [level, ribbon] : levels) {
            win_->region(Region::Ribbon)->setFixedHeight(ribbon);
            const double s = share();
            qInfo("%dx%d %s, ribbon %s %d px: khung trên %d px, dưới %d px, vùng vẽ %dx%d = %.1f%% diện tích cửa sổ",
                  win_->width(), win_->height(), wide ? "Wide" : "Small", level, ribbon, top + ribbon, bottom,
                  canvas()->width(), canvas()->height(), s * 100);
            const bool full = ribbon == levels[0].second;
            if (wide) {
                if (full)
                    QVERIFY2(s >= 0.57 && s <= 0.63, qPrintable(QString::number(s)));
                continue;
            }
            if (full)
                QVERIFY2(s >= 0.61, qPrintable(QString::number(s))); // số của bố cục prototype: không lùi
            // ≥ 70% [đề xuất] chỉ đạt khi ribbon còn tên tab. Mức thu gọn mặc định của màn Small chờ chủ dự án quyết
            // (không tự chọn): báo lệch tiêu chí kèm số, không giả kết quả.
            const QByteArray why = QByteArray("1366x768, ribbon ") + level + ": " + QByteArray::number(s * 100, 'f', 1) +
                                   "% < 70% (Small chưa có mức thu gọn mặc định)";
            if (ribbon != tabs)
                QEXPECT_FAIL("", why.constData(), Continue);
            QVERIFY2(s >= 0.70, why.constData());
        }
    }

    // Tiêu chí 2.
    void cleanScreen_data()
    {
        QTest::addColumn<bool>("extra"); // thêm palette nổi và khung trượt đang mở
        QTest::addRow("default") << false;
        QTest::addRow("floating-flyout") << true;
    }

    void cleanScreen()
    {
        QFETCH(bool, extra);
        QVERIFY(build());
        if (extra) {
            host().moveTo(u"layers"_s, Qt::NoDockWidgetArea);
            dock("layers")->setGeometry(300, 300, 280, 400);
        }
        const QStringList before = snapshot();
        const QImage beforeShot = shot();
        if (extra) { // khung trượt đang mở (tạm thời như menu): Clean Screen thu lại, tắt không mở lại
            host().showPalette(u"tcvnlib"_s);
            QVERIFY(spinUntil([&] { return strip(Qt::LeftDockWidgetArea)->current() != nullptr; }));
        }
        const QKeyCombination ctrl0(Qt::ControlModifier, Qt::Key_0);
        const auto state = [&] { return sysvars_->get(u"CLEANSCREENSTATE"_s); };
        QCOMPARE(state(), 0);

        // Ctrl+0 → '_CLEANSCREENON
        QCOMPARE(actions_->macro(u"CLEANSCREENON"_s), u"'_CLEANSCREENON"_s);
        QTest::keyClick(win_.get(), ctrl0.key(), ctrl0.keyboardModifiers());
        QVERIFY(clean_->isOn());
        QCOMPARE(state(), 1);
        const QStringList on = snapshot();
        const QImage onShot = shot();
        for (const Region r : {Region::TitleBar, Region::Ribbon, Region::FileTabs, Region::LayoutTabs})
            QVERIFY(!win_->region(r)->isVisible());
        for (const Region r : {Region::Canvas, Region::CommandLine, Region::StatusBar})
            QVERIFY(win_->region(r)->isVisible());
        for (const QString& id : host().ids())
            QVERIFY2(!host().dock(id)->isVisible(), qPrintable(id));
        for (const Qt::DockWidgetArea side : {Qt::LeftDockWidgetArea, Qt::RightDockWidgetArea})
            if (strip(side))
                QVERIFY(!strip(side)->isVisible() && !strip(side)->flyout()->isVisible());
        const double s = share();
        QCOMPARE(canvas()->width(), win_->width());
        qInfo("Clean Screen 1920×1080: vùng vẽ %dx%d = %.1f%%", canvas()->width(), canvas()->height(), s * 100);
        QVERIFY2(s >= 0.75, qPrintable(QString::number(s)));

        // Ctrl+0 lần nữa → '_CLEANSCREENOFF: đúng từng vùng, từng palette, từng pixel
        QCOMPARE(actions_->macro(u"CLEANSCREENON"_s), u"'_CLEANSCREENOFF"_s);
        QTest::keyClick(win_.get(), ctrl0.key(), ctrl0.keyboardModifiers());
        QVERIFY(!clean_->isOn());
        QCOMPARE(state(), 0);
        QCOMPARE(snapshot(), before);
        QVERIFY(shot() == beforeShot);

        // Cùng kết quả khi chạy qua bộ định tuyến (macro của menu, ribbon, script sau này)
        router_->submit(u"'_CLEANSCREENON"_s);
        QCOMPARE(snapshot(), on);
        QVERIFY(shot() == onShot);
        router_->submit(u"^C^C_CLEANSCREENOFF"_s);
        QCOMPARE(snapshot(), before);
        QVERIFY(shot() == beforeShot);
        router_->submit(u"^C^C_CLEANSCREENOFF"_s); // đã tắt: không làm gì
        QCOMPARE(snapshot(), before);

        // Thời gian: lệnh → bố cục → khung (grab) xong
        std::vector<double> took;
        for (int i = 0; i < 10; ++i) {
            const double t0 = nowMs();
            router_->submit(i % 2 ? u"_CLEANSCREENOFF"_s : u"_CLEANSCREENON"_s);
            (void)shot();
            took.push_back(nowMs() - t0);
        }
        const double worst = percentile(took, 1);
        qInfo("bật/tắt Clean Screen đến khung vẽ xong: p50 %.2f ms, chậm nhất %.2f ms (1 khung 16,7 ms)",
              percentile(took, 0.5), worst);
        QVERIFY2(!test::kTimed || worst <= 100, qPrintable(QString::number(worst)));
    }

    void router()
    {
        CommandRouter r(*sysvars_);
        QSignalSpy unhandled(&r, &CommandRouter::unhandled);
        int calls = 0;
        r.add(u"tcping"_s, [&] { ++calls; }); // tên không phân biệt hoa thường
        for (const QString& m : {u"TCPING"_s, u"^C^C_tcping"_s, u"'_TCPING"_s, u"^C^C._TcPing"_s, u"_.TCPING 1 2"_s})
            r.submit(m);
        QCOMPARE(calls, 5);
        QCOMPARE(unhandled.count(), 0);

        // Sysvar trong suốt (hành động gắn sysvar): đặt Sysvars
        sysvars_->set(u"ORTHOMODE"_s, 0);
        r.submit(u"'ORTHOMODE 1"_s);
        QCOMPARE(sysvars_->get(u"ORTHOMODE"_s), 1);
        r.submit(u"'_OSMODE 4133"_s);
        QCOMPARE(sysvars_->get(u"OSMODE"_s), 4133);
        r.submit(u"'DYNMODE -3"_s);
        QCOMPARE(sysvars_->get(u"DYNMODE"_s), -3);
        QCOMPARE(unhandled.count(), 0);

        // Không xử lý: lệnh của lõi, lệnh của danh mục có đối số số, sysvar không giá trị hoặc giá trị không phải số,
        // không trong suốt, DIESEL
        const QStringList rest{u"^C^C_LINE"_s,   u"'_ZOOM 2"_s,       u"'ORTHOMODE"_s,   u"'ORTHOMODE x"_s,
                               u"ORTHOMODE 0"_s, u"'ORTHOMODE 1 2"_s, u"$M=$(if,1,_A)"_s};
        for (const QString& m : rest)
            r.submit(m);
        QCOMPARE(unhandled.count(), rest.size());
        for (qsizetype i = 0; i < rest.size(); ++i)
            QCOMPARE(unhandled[i][0].toString(), rest[i]);
        QCOMPARE(sysvars_->get(u"ORTHOMODE"_s), 1);
        r.submit(u""_s); // rỗng, chỉ hủy lệnh: không gì
        r.submit(u"^C^C"_s);
        QCOMPARE(unhandled.count(), rest.size());

        // Đầu cuối: F8 (ActionRegistry) → '_ORTHOMODE 0 → router của test → Sysvars → nút ORTHO bỏ chọn
        QAction* ortho = actions_->action(u"ortho"_s);
        QVERIFY(ortho->isChecked());
        ortho->trigger();
        QCOMPARE(sysvars_->get(u"ORTHOMODE"_s), 0);
        QVERIFY(!ortho->isChecked());
        ortho->trigger();
        QVERIFY(ortho->isChecked());
    }

    // Tiêu chí 3.
    void workspaces()
    {
        // Mô tả đọc đủ: tên vùng đúng, có vùng vẽ, palette đã đăng ký, cạnh hợp lệ
        QVERIFY(build());
        const QMetaEnum regions = QMetaEnum::fromType<Region>();
        for (const QString& id : Workspace::ids()) {
            QVERIFY(!Workspace::name(id).isEmpty());
            QFile f(u":/trion/shell/workspaces/%1.json"_s.arg(id));
            QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(id));
            const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
            QStringList names;
            for (const QJsonValue& v : o[u"regions"_s].toArray()) {
                bool ok = false;
                regions.keyToValue(v.toString().toLatin1().constData(), &ok);
                QVERIFY2(ok, qPrintable(v.toString()));
                names << v.toString();
            }
            QVERIFY(names.contains(u"Canvas"_s));
            QVERIFY(!o[u"palettes"_s].toArray().isEmpty());
            for (const QJsonValue& v : o[u"palettes"_s].toArray()) {
                QVERIFY2(host().ids().contains(v[u"id"_s].toString()), qPrintable(v[u"id"_s].toString()));
                QVERIFY((QStringList{u"left"_s, u"right"_s, u"bottom"_s}).contains(v[u"area"_s].toString()));
            }
        }
        QCOMPARE(Workspace::name(u"classic"_s), QString());

        Workspace& ws = win_->workspace();
        QSignalSpy changed(&ws, &Workspace::changed);
        QCOMPARE(ws.current(), u"2d"_s);
        QVERIFY(ws.isDefaultLayout());
        const QStringList twoD = snapshot();
        const QImage twoDShot = shot();

        ws.apply(u"viewer"_s); // Viewer: không có palette trái
        QCOMPARE(ws.current(), u"viewer"_s);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed[0][0].toString(), u"viewer"_s);
        QCOMPARE(window_->value(u"ui/window/workspace"_s).toString(), u"viewer"_s);
        QCOMPARE(stripIds(Qt::LeftDockWidgetArea), QStringList());
        QVERIFY(onScreen(dock("props")));
        QVERIFY(!dock("tcvnlib")->isVisible() && !dock("blocks")->isVisible());
        QVERIFY(ws.isDefaultLayout());
        ws.apply(u"2d"_s);
        QCOMPARE(changed.count(), 2);
        QCOMPARE(snapshot(), twoD); // về đúng bố cục mặc định
        QVERIFY(shot() == twoDShot);
        ws.apply(u"classic"_s); // chưa có (UIA-105): bỏ qua
        QCOMPARE(ws.current(), u"2d"_s);
        QCOMPARE(changed.count(), 2);

        // Hồ sơ Viewer của EditionGate: ép "viewer", không đổi lựa chọn đã lưu
        ws.setProfile(edition::UiProfile::Viewer);
        QCOMPARE(ws.current(), u"viewer"_s);
        QCOMPARE(ws.chosen(), u"2d"_s);
        QCOMPARE(changed.count(), 3);
        QCOMPARE(stripIds(Qt::LeftDockWidgetArea), QStringList());
        ws.setProfile(edition::UiProfile::Viewer);
        QCOMPARE(changed.count(), 3);
        ws.setProfile(edition::UiProfile::Full);
        QCOMPARE(ws.current(), u"2d"_s);
        QCOMPARE(snapshot(), twoD);

        // Đổi workspace khi Clean Screen bật: workspace mới hiện đủ vùng và palette, Clean Screen tắt
        clean_->setOn(true);
        ws.apply(u"viewer"_s);
        QVERIFY(!clean_->isOn());
        QCOMPARE(sysvars_->get(u"CLEANSCREENSTATE"_s), 0);
        QVERIFY(win_->region(Region::Ribbon)->isVisible());
        QVERIFY(onScreen(dock("props")));

        // Thời gian: đổi workspace đến khung vẽ xong
        std::vector<double> took;
        for (int i = 0; i < 10; ++i) {
            const double t0 = nowMs();
            ws.apply(i % 2 ? u"viewer"_s : u"2d"_s);
            (void)shot();
            took.push_back(nowMs() - t0);
        }
        const double worst = percentile(took, 1);
        qInfo("đổi workspace đến khung vẽ xong: p50 %.2f ms, chậm nhất %.2f ms (1 khung 16,7 ms)",
              percentile(took, 0.5), worst);
        QVERIFY2(!test::kTimed || worst <= 100, qPrintable(QString::number(worst)));
    }

    void restarts()
    {
        // Lần đầu: phóng to trên màn hình chính, bố cục mặc định của Soạn thảo 2D
        QVERIFY(build(std::nullopt));
        QVERIFY(win_->isMaximized());
        QCOMPARE(win_->screen(), QGuiApplication::primaryScreen());
        QCOMPARE(win_->frameGeometry(), QGuiApplication::primaryScreen()->availableGeometry());
        // Người dùng đổi: cửa sổ thường 1500×900, Viewer, Layer sang dock trái, Bác sĩ Font nổi
        win_->showNormal();
        win_->setGeometry(100, 120, 1500, 900);
        win_->workspace().apply(u"viewer"_s);
        host().moveTo(u"layers"_s, Qt::LeftDockWidgetArea);
        host().moveTo(u"fontdoc"_s, Qt::NoDockWidgetArea);
        dock("fontdoc")->setGeometry(700, 300, 300, 420);
        QVERIFY(!win_->workspace().isDefaultLayout());
        settle();
        const QStringList want = snapshot();
        const QRect geometry = win_->geometry();
        closeWindow();

        const int runs = test::kTimed ? 100 : 10;
        std::vector<double> took;
        for (int i = 0; i < runs; ++i) {
            const double t0 = nowMs();
            QVERIFY(build(std::nullopt));
            took.push_back(nowMs() - t0);
            QVERIFY2(!win_->isMaximized(), qPrintable(QString::number(i)));
            QCOMPARE(win_->geometry(), geometry);
            QCOMPARE(win_->workspace().current(), u"viewer"_s);
            QCOMPARE(snapshot(), want);
            closeWindow();
        }
        qInfo("%d lần dựng lại cửa sổ với cùng Store: giữ hình học, workspace, dock; dựng + khôi phục + hiện: p50 "
              "%.1f ms, chậm nhất %.1f ms",
              runs, percentile(took, 0.5), percentile(took, 1));

        // Phóng to: lưu cả hình học thường
        QVERIFY(build(std::nullopt));
        win_->showMaximized();
        QVERIFY(spinUntil([&] { return win_->isMaximized(); }));
        closeWindow();
        QVERIFY(build(std::nullopt));
        QVERIFY(win_->isMaximized());
        win_->showNormal();
        QVERIFY(spinUntil([&] { return win_->geometry() == geometry; }));

        // Màn hình: cửa sổ trên p150 (Small) khôi phục về p150
        win_->setGeometry(2100, 50, 1100, 600);
        QVERIFY(spinUntil([&] { return win_->screen() == screenNamed("p150"); }));
        closeWindow();
        QVERIFY(build(std::nullopt));
        QCOMPARE(win_->screen(), screenNamed("p150"));
        QCOMPARE(win_->screenClass(), ScreenClass::Small);
        closeWindow();

        // Màn hình đã lưu không còn (rút p150): về màn hình chính, nằm trọn trong vùng làm việc
        QVERIFY(setScreens(1));
        QVERIFY(build(std::nullopt));
        QCOMPARE(win_->screen(), QGuiApplication::primaryScreen());
        QVERIFY(QGuiApplication::primaryScreen()->availableGeometry().contains(win_->frameGeometry()));
        QCOMPARE(win_->screenClass(), ScreenClass::Wide);
    }

    // Tiêu chí 4.
    void screenClassRuntime()
    {
        QVERIFY(build(kFull, {}, true));
        QCOMPARE(win_->screenClass(), ScreenClass::Wide);
        QVERIFY(onScreen(dock("props")));
        win_->setGeometry(6000, 0, 1366, 768); // kéo sang màn hình 1366 (Small)
        QVERIFY(spinUntil([&] { return display_->screenClass() == ScreenClass::Small; }));
        QCOMPARE(win_->screenClass(), ScreenClass::Small);
        settle();
        QCOMPARE(stripIds(Qt::RightDockWidgetArea), (QStringList{u"props"_s, u"layers"_s, u"fontdoc"_s}));
        QCOMPARE(win_->region(Region::CommandLine)->height(), expectedHeight(Region::CommandLine, true));
        win_->setGeometry(kFull); // về
        QVERIFY(spinUntil([&] { return display_->screenClass() == ScreenClass::Wide; }));
        settle();
        QVERIFY(onScreen(dock("props")));
        QCOMPARE(stripIds(Qt::RightDockWidgetArea), QStringList());
        QCOMPARE(win_->region(Region::CommandLine)->height(), expectedHeight(Region::CommandLine));

        // Người dùng đã đổi (Thuộc tính tự ẩn ở màn hình rộng): giữ nguyên khi đổi màn hình; dòng lệnh vẫn theo
        host().setPinned(u"props"_s, false);
        QVERIFY(!win_->workspace().isDefaultLayout());
        const auto layout = [&] {
            QStringList s{stripIds(Qt::LeftDockWidgetArea).join(u','), stripIds(Qt::RightDockWidgetArea).join(u',')};
            for (const QString& id : host().ids())
                s << u"%1 %2 %3 %4"_s.arg(id)
                         .arg(host().isPinned(id))
                         .arg(host().dock(id)->isHidden())
                         .arg(int(win_->dockHost()->dockWidgetArea(host().dock(id))));
            return s;
        };
        const QStringList custom = layout();
        win_->setGeometry(6000, 0, 1366, 768);
        QVERIFY(spinUntil([&] { return display_->screenClass() == ScreenClass::Small; }));
        settle();
        QCOMPARE(layout(), custom);
        QCOMPARE(win_->region(Region::CommandLine)->height(), expectedHeight(Region::CommandLine, true));
    }

    void dprChange()
    {
        QVERIFY(build(QRect(100, 100, 1200, 700), {}, true));
        const auto check = [&](const char* screen, qreal dpr, QRect geometry) {
            const double t0 = nowMs();
            win_->setGeometry(geometry);
            if (!spinUntil([&] { return win_->screen() == screenNamed(screen) && win_->devicePixelRatio() == dpr; }))
                return false;
            settle();
            const double ms = nowMs() - t0;
            int y = 0; // các vùng xếp liền, cao đúng token ở mọi DPR (px logic)
            for (int i = 0; i < MainWindow::kRegionCount; ++i) {
                const QWidget* w = win_->region(Region(i));
                const QRect r(w->mapTo(win_.get(), QPoint()), w->size());
                if (r.y() != y || r.width() != (Region(i) == Region::Canvas ? r.width() : win_->width()))
                    return false;
                if (Region(i) != Region::Canvas &&
                    r.height() != expectedHeight(Region(i), win_->screenClass() != ScreenClass::Wide))
                    return false;
                y += Region(i) == Region::Canvas ? win_->dockHost()->height() : r.height();
            }
            const QImage img = shot();
            qInfo("%s DPR %.2f: bố cục dựng lại sau %.2f ms (CPU luồng UI, gồm chờ sự kiện màn hình), ảnh %dx%d",
                  screen, dpr, ms, img.width(), img.height());
            return y == win_->height() && img.devicePixelRatio() == dpr && img.width() == qRound(win_->width() * dpr);
        };
        // Đổi DPR không sinh cảnh báo nào (trừ cảnh báo propagateSizeHints của chính plugin offscreen)
        QTest::failOnWarning(QRegularExpression(u"^(?!This plugin does not support propagateSizeHints)"_s));
        QVERIFY(check("p150", 1.5, QRect(2100, 50, 1100, 600)));
        QVERIFY(check("p200", 2.0, QRect(4050, 20, 800, 480)));
        QVERIFY(check("p100", 1.0, QRect(100, 100, 1200, 700)));
    }

    void resizeLayout()
    {
        QVERIFY(build(kFull));
        std::vector<double> took;
        const int steps = test::kTimed ? 300 : 60;
        for (int i = 0; i < steps; ++i) {
            const int k = i % 40;
            const QSize size(1366 + k * 14, 768 + k * 8);
            const double t0 = nowMs();
            win_->resize(size);
            settle();
            win_->repaint(); // vẽ đồng bộ như một khung
            took.push_back(nowMs() - t0);
            QCOMPARE(win_->size(), size);
        }
        const double p99 = percentile(took, 0.99);
        qInfo("đổi cỡ cửa sổ liên tục (%d lần, 1366×768 ↔ 1912×1080): bố cục + vẽ mỗi lần p50 %.2f ms, p99 %.2f ms; "
              "khung 60 fps thật do QA-011 đo",
              steps, percentile(took, 0.5), p99);
        QVERIFY2(!test::kTimed || p99 <= 16.6, qPrintable(QString::number(p99)));
        QCOMPARE(win_->region(Region::StatusBar)->geometry().bottom(), win_->height() - 1);
    }

    // Tiêu chí 5 và hợp đồng với UIA-002, integration.
    void titleBar()
    {
        QVERIFY(build(QRect(100, 100, 1200, 700)));
        // Widget thật của thanh tiêu đề không nhận chuột ở vùng trống: sự kiện lên MainWindow
        auto* bar = new QWidget;
        bar->setFixedHeight(px(tok::h_title));
        win_->setRegion(Region::TitleBar, bar);
        bar->setContentsMargins(0, 0, 138, 0); // lề chừa nút cửa sổ (hợp đồng): không đổi chỗ bắt nhấp đúp
        settle();
        QTest::mouseDClick(bar, Qt::LeftButton, {}, QPoint(400, 5));
        QVERIFY(spinUntil([&] { return win_->isMaximized(); }));
        QTest::mouseDClick(bar, Qt::LeftButton, {}, QPoint(400, 5));
        QVERIFY(spinUntil([&] { return !win_->isMaximized(); }));
        // Nhấp đúp ở vùng vẽ không phóng to; kéo trên thanh tiêu đề gọi startSystemMove (offscreen không di chuyển)
        QTest::mouseDClick(canvas(), Qt::LeftButton, {}, QPoint(50, 50));
        settle();
        QVERIFY(!win_->isMaximized());
        const QRect g = win_->geometry();
        QTest::mousePress(bar, Qt::LeftButton, {}, QPoint(400, 5));
        QTest::mouseMove(bar, QPoint(440, 5));
        QTest::mouseRelease(bar, Qt::LeftButton, {}, QPoint(440, 5));
        QCOMPARE(win_->geometry(), g);
    }

    void setRegion()
    {
        QVERIFY(build());
        QPointer<QWidget> old = win_->region(Region::Ribbon);
        auto* ribbon = new QLabel(u"ribbon"_s); // i18n-ok: widget mẫu của test
        ribbon->setFixedHeight(60);
        win_->setRegion(Region::Ribbon, ribbon);
        QVERIFY(old.isNull()); // ô giữ chỗ bị hủy
        QCOMPARE(win_->region(Region::Ribbon), ribbon);
        settle();
        QCOMPARE(ribbon->geometry().top(), expectedHeight(Region::TitleBar));
        QVERIFY(ribbon->isVisible());

        win_->setRegionVisible(Region::FileTabs, false); // vùng đang ẩn: widget thay thế cũng ẩn
        auto* tabs = new QWidget;
        win_->setRegion(Region::FileTabs, tabs);
        QVERIFY(!win_->isRegionVisible(Region::FileTabs) && !tabs->isVisible());
        win_->setRegionVisible(Region::FileTabs, true);
        QVERIFY(tabs->isVisible());

        auto* cmd = new QWidget; // hợp đồng UIA-107: compact theo lớp màn hình
        win_->setRegion(Region::CommandLine, cmd);
        QCOMPARE(cmd->property("compact"), QVariant(false));
        win_->setScreenClass(ScreenClass::Narrow);
        QCOMPARE(cmd->property("compact"), QVariant(true));

        QPointer<QWidget> oldCanvas = canvas(); // vùng vẽ: widget trung tâm của vùng dock, Esc/F6 về đây
        auto* viewport = new QWidget;
        viewport->setFocusPolicy(Qt::StrongFocus);
        win_->setRegion(Region::Canvas, viewport);
        QVERIFY(oldCanvas.isNull());
        QCOMPARE(win_->dockHost()->centralWidget(), viewport);
        host().focusCanvas();
        QVERIFY(spinUntil([&] { return viewport->hasFocus(); }));
    }

    void accessibility()
    {
        QVERIFY(build());
        QVERIFY(spinUntil([&] { return canvas()->hasFocus(); })); // vùng vẽ nhận phím gõ ngay
        for (int i = 0; i < MainWindow::kRegionCount; ++i) {
            QWidget* w = win_->region(Region(i));
            QVERIFY(!MainWindow::regionName(Region(i)).isEmpty());
            QAccessibleInterface* a = QAccessible::queryAccessibleInterface(w);
            QVERIFY(a);
            QCOMPARE(a->text(QAccessible::Name), MainWindow::regionName(Region(i)));
        }
        QCOMPARE(MainWindow::regionName(Region::Canvas), u"Vùng vẽ"_s);
        QCOMPARE(canvas()->focusPolicy(), Qt::StrongFocus);
        // Ô giữ chỗ vùng vẽ (UIA-109 thay): vai trò Canvas (QWidget thường là Client), viền 2 px --c-focus chỉ khi focus
        // đến từ bàn phím.
        QCOMPARE(QAccessible::queryAccessibleInterface(canvas())->role(), QAccessible::Canvas);
        const auto edge = [this] {
            const QImage shot = canvas()->grab().toImage();
            return shot.pixel(1, shot.height() / 2);
        };
        QCOMPARE(edge(), tm_->color(tok::c_canvas).rgb());
        win_->setAttribute(Qt::WA_KeyboardFocusChange); // như vừa Tab tới
        QCOMPARE(edge(), tm_->color(tok::c_focus).rgb());
        win_->setAttribute(Qt::WA_KeyboardFocusChange, false);
        // F6: vào palette đang mở rồi về vùng vẽ (PaletteHost)
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_F6);
        QVERIFY(spinUntil([&] { return dock("props")->isAncestorOf(QApplication::focusWidget()); }));
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_F6);
        QVERIFY(spinUntil([&] { return canvas()->hasFocus(); }));
        // Đổi ngôn ngữ: tên của ô giữ chỗ dịch lại
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QVERIFY(spinUntil([&] { return canvas()->accessibleName() == MainWindow::regionName(Region::Canvas); }));
        QVERIFY(canvas()->accessibleName().size() > u"Vùng vẽ"_s.size());
    }

    void pseudoNoTruncation_data()
    {
        QTest::addColumn<QRect>("geometry");
        QTest::addColumn<int>("cls");
        QTest::addRow("wide") << kFull << int(ScreenClass::Wide);
        QTest::addRow("small") << kSmall << int(ScreenClass::Small);
    }

    void pseudoNoTruncation()
    {
        QFETCH(QRect, geometry);
        QFETCH(int, cls);
        QVERIFY(build(geometry, ScreenClass(cls)));
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QCoreApplication::processEvents(); // LanguageChange: tiêu đề palette, tab của dải, EmptyState
        settle();
        QStringList cut;
        for (const QString& id : host().ids()) { // từng palette lên đầu nhóm tab hoặc trượt ra từ dải
            host().showPalette(id);
            QVERIFY(spinUntil([&] { return onScreen(host().content(id)); }));
            settle();
            for (const i18n::Truncation& t : i18n::findTruncations(win_.get()))
                cut << id + u": "_s + t.toString();
        }
        QVERIFY2(cut.isEmpty(), qPrintable(cut.join(u"\n"_s)));
    }

    void idleNoTimers()
    {
        QVERIFY(build());
        spinUntil([] { return false; }, 300ms); // việc còn chờ (vẽ, bố cục, biểu tượng) xong
        QEventLoop loop;
        QTimer end;
        TimerCounter counter;
        counter.skip = &end;
        qApp->installEventFilter(&counter);
        end.setSingleShot(true);
        connect(&end, &QTimer::timeout, &loop, &QEventLoop::quit);
        end.start(500);
        loop.exec();
        qApp->removeEventFilter(&counter);
        QVERIFY2(counter.seen.isEmpty(), qPrintable(counter.seen.join(u", "_s)));
    }

    // Dòng lệnh nổi (UIA-007) với widget giả làm như CommandLine::setFloating: vừa rời cột thì rộng 2/3 vùng vẽ, chỗ kẹp
    // trong vùng vẽ.
    void commandLineFloating()
    {
        QWidget* cmd = nullptr;
        QList<bool> calls; // commandLineFloatChanged theo thứ tự
        bool floating = false;
        setup_ = [&] {
            cmd = new QWidget;
            cmd->setObjectName(u"cmd"_s);
            cmd->setFixedHeight(expectedHeight(Region::CommandLine));
            win_->setRegion(Region::CommandLine, cmd);
            floating = false;
            connect(win_.get(), &MainWindow::commandLineFloatChanged, cmd, [&](bool on, QPoint at) {
                calls << on;
                QWidget* host = cmd->parentWidget();
                if (on && !floating)
                    cmd->resize(std::min(cmd->width(), host->width() * 2 / 3), cmd->height());
                floating = on;
                if (!on)
                    return;
                const QPoint p = host->mapFromGlobal(at);
                cmd->move(std::clamp(p.x(), 0, std::max(0, host->width() - cmd->width())),
                          std::clamp(p.y(), 0, std::max(0, host->height() - cmd->height())));
                cmd->raise();
            });
        };
        QVERIFY(build());
        auto* column = static_cast<QBoxLayout*>(win_->centralWidget()->layout());
        const int slot = column->indexOf(cmd);
        QVERIFY(slot > column->indexOf(win_->region(Region::LayoutTabs)));
        const int canvasHeight = canvas()->height();
        QVERIFY(!win_->isCommandLineFloating() && calls.isEmpty());

        // Nổi: rời cột (vùng vẽ giãn ra phần đó), con của vùng vẽ, hiện, đúng chỗ, rộng 2/3 vùng vẽ
        win_->setCommandLineFloating(true, canvas()->mapToGlobal(QPoint(200, 150)));
        settle();
        QVERIFY(win_->isCommandLineFloating());
        QCOMPARE(cmd->parentWidget(), canvas());
        QCOMPARE(column->indexOf(cmd), -1);
        QVERIFY(cmd->isVisible() && win_->isRegionVisible(Region::CommandLine));
        QCOMPARE(cmd->pos(), QPoint(200, 150));
        QCOMPARE(cmd->width(), canvas()->width() * 2 / 3);
        QCOMPARE(canvas()->height(), canvasHeight + expectedHeight(Region::CommandLine));
        QVERIFY(!calls.isEmpty() && !calls.contains(false)); // nổi, rồi kẹp lại khi vùng vẽ giãn

        // Ẩn/hiện vùng (COMMANDLINEHIDE / COMMANDLINE) và Clean Screen khi nổi: vẫn nổi, đúng chỗ
        win_->setRegionVisible(Region::CommandLine, false);
        QVERIFY(!cmd->isVisible() && !win_->isRegionVisible(Region::CommandLine));
        win_->setRegionVisible(Region::CommandLine, true);
        QVERIFY(cmd->isVisible() && win_->isRegionVisible(Region::CommandLine));
        router_->submit(u"'_CLEANSCREENON"_s);
        settle();
        QVERIFY(clean_->isOn() && cmd->isVisible() && win_->isCommandLineFloating());
        QCOMPARE(cmd->pos(), QPoint(200, 150)); // vùng vẽ lớn ra: chỗ đã chọn vẫn vừa
        router_->submit(u"'_CLEANSCREENOFF"_s);
        settle();
        QCOMPARE(cmd->pos(), QPoint(200, 150));

        // Góc dưới phải; cửa sổ nhỏ lại: kẹp trong vùng vẽ; lớn lại: về đúng chỗ đã chọn
        const QPoint corner(canvas()->width() - cmd->width(), canvas()->height() - cmd->height());
        win_->setCommandLineFloating(true, canvas()->mapToGlobal(corner));
        QCOMPARE(cmd->pos(), corner);
        win_->resize(win_->width() - 200, win_->height() - 150);
        settle();
        QCOMPARE(cmd->pos(), QPoint(canvas()->width() - cmd->width(), canvas()->height() - cmd->height()));
        win_->setGeometry(kFull);
        settle();
        QCOMPARE(cmd->pos(), corner);

        // Người dùng kéo (widget tự dời): chỗ mới là chỗ đã chọn, đổi cỡ vùng vẽ không kéo về chỗ cũ
        cmd->move(300, 120);
        win_->resize(win_->width() - 1, win_->height());
        settle();
        QCOMPARE(cmd->pos(), QPoint(300, 120));

        // Đổi widget vùng vẽ: dòng lệnh không bị hủy theo, thôi nổi rồi nổi lại trên vùng vẽ mới đúng chỗ, rộng
        const QRect g = cmd->geometry();
        calls.clear();
        QPointer<QWidget> old = canvas();
        auto* viewport = new QWidget;
        viewport->setFocusPolicy(Qt::StrongFocus);
        win_->setRegion(Region::Canvas, viewport);
        settle();
        QVERIFY(old.isNull() && cmd);
        QCOMPARE(cmd->parentWidget(), viewport);
        QVERIFY(cmd->isVisible());
        QCOMPARE(cmd->geometry(), g);
        QCOMPARE(calls.first(), false);
        QVERIFY(calls.size() >= 2 && calls.mid(1) == QList<bool>(calls.size() - 1, true));

        // Lưu, dựng lại với cùng Store: nổi lại đúng chỗ, rộng (sau khi cửa sổ hiện, vùng vẽ có cỡ thật)
        closeWindow();
        calls.clear();
        QVERIFY(build());
        QVERIFY(win_->isCommandLineFloating());
        QCOMPARE(cmd->parentWidget(), canvas());
        column = static_cast<QBoxLayout*>(win_->centralWidget()->layout()); // cột của cửa sổ mới
        QCOMPARE(column->indexOf(cmd), -1);
        QCOMPARE(cmd->geometry(), g);
        QVERIFY(cmd->isVisible());

        // Bám lại: về đúng ô trong cột, mọi vùng như lúc chưa nổi
        calls.clear();
        win_->setCommandLineFloating(false);
        settle();
        QVERIFY(!win_->isCommandLineFloating());
        QCOMPARE(calls, QList<bool>{false});
        QCOMPARE(column->indexOf(cmd), slot);
        QCOMPARE(cmd->parentWidget(), win_->centralWidget());
        QCOMPARE(cmd->geometry().top(), win_->region(Region::LayoutTabs)->geometry().bottom() + 1);
        QCOMPARE(canvas()->height(), canvasHeight);
        closeWindow();
        QVERIFY(build()); // đã bám khi lưu: khởi động ở cột
        QVERIFY(!win_->isCommandLineFloating());
        QCOMPARE(static_cast<QBoxLayout*>(win_->centralWidget()->layout())->indexOf(cmd), slot);
    }

    // Hàng dưới ribbon (QAT "Hiện bên dưới ribbon", UIA-101): giữa ribbon và tab tệp, theo thanh tiêu đề (Clean Screen
    // ẩn), không theo ribbon (RIBBONCLOSE); widget rời hàng thì hàng tự ẩn.
    void belowRibbon()
    {
        QVERIFY(build());
        QWidget* row = win_->belowRibbon();
        QVERIFY(row && !row->isVisible());
        const QStringList before = snapshot();
        auto* qat = new QLabel(u"QAT"_s); // i18n-ok: widget mẫu của test
        win_->addBelowRibbon(qat);
        settle();
        QVERIFY(row->isVisible() && qat->isVisible());
        const QWidget* ribbon = win_->region(Region::Ribbon);
        const QWidget* tabs = win_->region(Region::FileTabs);
        QCOMPARE(row->geometry().top(), ribbon->geometry().bottom() + 1);
        QCOMPARE(tabs->geometry().top(), row->geometry().bottom() + 1);
        QCOMPARE(qat->x(), px(tok::sp_2)); // xếp từ trái, lề như thanh tiêu đề
        QVERIFY(row->height() >= qat->sizeHint().height() + kLine);

        router_->submit(u"'_CLEANSCREENON"_s); // ẩn cùng thanh tiêu đề
        QVERIFY(!row->isVisible());
        router_->submit(u"'_CLEANSCREENOFF"_s);
        QVERIFY(row->isVisible());
        win_->setRegionVisible(Region::Ribbon, false); // RIBBONCLOSE: QAT vẫn dùng được
        QVERIFY(row->isVisible());
        win_->setRegionVisible(Region::Ribbon, true);

        QWidget other;
        auto* layout = new QHBoxLayout(&other);
        layout->addWidget(qat); // về chỗ khác (thanh tiêu đề lấy lại): hàng rỗng tự ẩn
        QVERIFY(!row->isVisible());
        QCOMPARE(snapshot(), before);
    }

    // Nội dung thật của palette giữ chỗ (setPaletteContent): dựng ở lần hiện đầu, nhận ô giữ chỗ; palette khác giữ ô giữ
    // chỗ. Workspace::layoutApplied khi khôi phục và khi đổi workspace.
    void paletteContent()
    {
        QPointer<QWidget> placeholder;
        int built = 0, applied = 0;
        setup_ = [&] {
            connect(&win_->workspace(), &Workspace::layoutApplied, win_.get(), [&] { ++applied; });
            win_->setPaletteContent(u"layers"_s, [&](QWidget* p) {
                ++built;
                placeholder = p;
                auto* box = new QWidget;
                auto* column = new QVBoxLayout(box);
                column->addWidget(p); // dùng tiếp ô giữ chỗ (như palette Layer của Viewer ở hồ sơ đầy đủ)
                box->setObjectName(u"realLayers"_s);
                return box;
            });
        };
        QVERIFY(build());
        QCOMPARE(applied, 1); // khôi phục lần đầu (chưa có bố cục đã lưu): áp bố cục mặc định
        host().showPalette(u"layers"_s);
        QVERIFY(spinUntil([&] { return host().content(u"layers"_s) != nullptr; }));
        QCOMPARE(host().content(u"layers"_s)->objectName(), u"realLayers"_s);
        QCOMPARE(built, 1);
        QVERIFY(placeholder && host().content(u"layers"_s)->isAncestorOf(placeholder));
        host().showPalette(u"props"_s);
        QVERIFY(spinUntil([&] { return host().content(u"props"_s) != nullptr; }));
        QVERIFY(host().content(u"props"_s)->objectName() != u"realLayers"_s);

        win_->workspace().apply(u"viewer"_s);
        QCOMPARE(applied, 2);
        win_->workspace().apply(u"2d"_s);
        QCOMPARE(applied, 3);
    }

    // Khung cửa sổ thật (07.1 §13.6): Qt::ExpandedClientAreaHint có đưa vùng client lên hàng tiêu đề không. Chạy tay:
    //   TRION_NATIVE_PROBE=1 shell_test -platform windows nativeTitleBar
    void nativeTitleBar()
    {
        if (QGuiApplication::platformName() != u"windows"_s)
            QSKIP("chỉ đo trên nền tảng thật: TRION_NATIVE_PROBE=1 shell_test -platform windows nativeTitleBar");
        for (const bool expanded : {false, true}) {
            QWidget w;
            w.setWindowFlag(Qt::ExpandedClientAreaHint, expanded);
            w.setGeometry(200, 200, 800, 500);
            w.show();
            QVERIFY(QTest::qWaitForWindowExposed(&w));
            spinUntil([] { return false; }, 300ms);
            const QMargins safe = w.windowHandle()->safeAreaMargins();
            qInfo("ExpandedClientAreaHint=%d: geometry %s, frameGeometry %s, safeAreaMargins %d,%d,%d,%d, flags 0x%x",
                  expanded, qPrintable(text(w.geometry())), qPrintable(text(w.frameGeometry())), safe.left(),
                  safe.top(), safe.right(), safe.bottom(), uint(w.windowHandle()->flags()));
        }
    }
};

QTEST_MAIN(ShellTest)
#include "shell_test.moc"
