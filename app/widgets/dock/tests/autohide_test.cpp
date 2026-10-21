// UI0-010: dải tự ẩn và khung trượt (AutoHideStrip.h), bố cục mặc định theo lớp màn hình, đổi DPI, rút màn hình
// (PaletteHost.h). Cửa sổ mẫu (UIA-001 chưa có): QMainWindow với khung giả cao theo token như #app của prototype (mật
// độ gọn). Giả định đo diện tích: trên (menuWidget) = thanh tiêu đề --h-title + ribbon (tab --h-tab, thân 3 × --h-btn +
// 8 px + --h-lbl, nét 1 px) + tab tệp --h-ftab; dưới (QStatusBar) = tab layout --h-ltab + dòng lệnh (lịch sử 3 ×
// --h-cmdl + 3 px, ô lệnh --h-cmdin là QLineEdit giả nhận focus, nét 1 px) + thanh trạng thái --h-status; widget trung
// tâm giả làm vùng vẽ; cửa sổ 1920×1080 (cả thanh tiêu đề tự vẽ như prototype). Palette như mặc định "Soạn thảo 2D" của
// prototype (core.js): Thuộc tính mở ở phải, Layer gộp tab với nó (đóng), thư viện TCVN ở trái tự ẩn, Đếm ở dưới
// (đóng). Màn hình giả (initMain, như display_context_test): p100 1920×1080 100% (chính), p150 1920×1080 150% (1280×720
// logic, lớp Small). Di chuột bằng QCursor::setPos (plugin offscreen sinh Enter/Leave, di chuột như thật), chỉ trên
// p100.
//   1  defaults: Wide → Thuộc tính dock mở rộng --pal-w, TCVN ở dải trái 22 px; p150 (Small) → mọi palette bỏ ghim,
//      palette đang mở nằm ở dải, mở palette khác thì trượt ra; canvasShare: diện tích vùng vẽ / cửa sổ 1920×1080 khi
//      mở, khi tự ẩn, khi Clean Screen giả (ẩn tiêu đề, ribbon, tab tệp, tab layout như #app.clean).
//   2  hoverOpen: rê tab 300 ms → trượt ra trong --t, hình học vùng vẽ không đổi suốt lúc trượt; rời chuột → thu; rê
//      chưa đủ 300 ms → không mở. clickEsc: bấm tab mở ngay, bấm lại thu; Esc thu (dòng lệnh không nhận Esc đó);
//      Giao diện nhẹ, giảm chuyển động → hiện ngay. pinBack: ghim lại về đúng chỗ dock cũ; đóng → ra khỏi dải, vẫn bỏ
//      ghim, mở lại thì trượt ra; chuyển chỗ → ghim. restore: dải, ghim lưu và khôi phục.
//   3  dpiFloating: palette nổi sang màn hình 150% → DPR 1,5, về trọn vùng làm việc, giữ nội dung và trạng thái;
//      dpiFlyout: cửa sổ đang trượt ra sang màn hình 150% và đổi cỡ → khung theo dải; unplug: rút màn hình 150% khi
//      palette nổi và đang trượt ra ở đó.
//   4  focus: trượt ra (rê, bấm tab) không lấy focus dòng lệnh; bấm vào palette thì có, rời chuột vẫn mở, bấm vùng vẽ
//      thì thu; Esc của ô tìm còn chữ là của ô; Space trên tab (bàn phím) đưa focus vào palette, Esc về vùng vẽ.
//   5  baseline: ảnh dải + khung trượt Tối/Sáng × 100/150/200% theo nền tảng, màu token ở chỗ không có chữ. Ghi lại:
//      TRION_UPDATE_BASELINE=<thư mục> chạy test, chép autohide-*-<nền tảng>.png vào tests/baseline.
//      pseudoNoTruncation: vi-XL, findTruncations = 0; idleNoTimers: rảnh không timer nào chạy.
#include "app/widgets/dock/AutoHideStrip.h"
#include "app/widgets/dock/PaletteHeader.h"
#include "app/widgets/dock/PaletteHost.h"

#include "app/core/display/DisplayContext.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "core/base/sched/Scheduler.h"

#include <QAbstractButton>
#include <QAbstractEventDispatcher>
#include <QAccessible>
#include <QApplication>
#include <QBoxLayout>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPainter>
#include <QPropertyAnimation>
#include <QScreen>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QtTest>

#include <qpa/qplatformnativeinterface.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;
using namespace trion::theme;
using namespace trion::ui;
namespace i18n = trion::i18n;
namespace sched = trion::sched;
namespace settings = trion::settings;

namespace {

#if defined(Q_OS_WIN)
constexpr auto kPlatform = "windows";
#elif defined(Q_OS_MACOS)
constexpr auto kPlatform = "macos";
#else
constexpr auto kPlatform = "linux";
#endif
constexpr int kNoise = 8;              // như dock_test: lệch ≤ 8/255 mỗi kênh là nhiễu dựng chữ
constexpr int kLine = 1;               // nét 1 px của prototype (ribbon, dòng lệnh)
constexpr int kHistoryPad = 3;         // .cmd-hist: height calc(--h-cmdl * 3 + 3px), prototype viết thẳng
constexpr int kShotHeight = 160;       // ảnh baseline: phần trên của dải và khung trượt
const QRect kFull(0, 0, 1920, 1080);   // cửa sổ phủ màn hình p100
const QRect kMid(100, 100, 1400, 900); // cửa sổ nhỏ hơn trên p100
const QPoint kParked(960, 2);          // chỗ con trỏ đầu mỗi test: thanh tiêu đề giả

// Màn hình giả: tỉ lệ = logicalDpi / 96. Qt giữ gốc tọa độ thiết bị làm gốc logic.
struct Screen {
    const char* name;
    int x, dpi;
};
constexpr Screen kScreens[] = {{"p100", 0, 96}, {"p150", 2000, 144}};
QString g_cwd; // thư mục chạy test; initMain đổi sang thư mục chứa cấu hình màn hình tới khi tạo lớp test

QJsonObject screens(qsizetype count)
{
    QJsonArray list;
    for (qsizetype i = 0; i < count; ++i)
        list.append(QJsonObject{{u"name"_s, QLatin1StringView(kScreens[i].name)},
                                {u"x"_s, kScreens[i].x},
                                {u"y"_s, 0},
                                {u"width"_s, 1920},
                                {u"height"_s, 1080},
                                {u"logicalDpi"_s, kScreens[i].dpi},
                                {u"logicalBaseDpi"_s, 96},
                                {u"dpr"_s, 1}});
    return QJsonObject{{u"screens"_s, list}};
}

// Cắm, rút màn hình giả lúc đang chạy: cấu hình mới cho plugin offscreen (so theo tên; màn hình mất thì Qt phát
// screenRemoved rồi dời cửa sổ còn lại về màn hình chính, như rút cáp). Cách Qt tự test (tst_qhighdpi).
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

struct Pal {
    const char* id;
    const char* title;
    Qt::DockWidgetArea area;
    bool visible, pinned;
};
// Cùng ngữ cảnh và chuỗi với dock_test (không thêm chuỗi dịch).
const Pal kPalettes[] = {
    {"props", QT_TRANSLATE_NOOP("DockTest", "Thuộc tính"), Qt::RightDockWidgetArea, true, true},
    {"layers", QT_TRANSLATE_NOOP("DockTest", "Layer"), Qt::RightDockWidgetArea, false, true},
    {"tcvn", QT_TRANSLATE_NOOP("DockTest", "Thư viện TCVN"), Qt::LeftDockWidgetArea, true, false},
    {"count", QT_TRANSLATE_NOOP("DockTest", "Đếm"), Qt::BottomDockWidgetArea, false, true},
};

bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 5s)
{
    if (cond())
        return true;
    // Timer trên stack (không QTimer::singleShot): chờ xong không còn timer nào chạy tiếp (idleNoTimers).
    QEventLoop loop;
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

// Con trỏ chuột tới điểm toàn cục (p100): plugin offscreen sinh Enter/Leave và di chuột cho cửa sổ dưới con trỏ.
void mouseTo(QPoint global)
{
    QCursor::setPos(global);
    QCoreApplication::processEvents();
}

QPoint centerOf(const QWidget* w)
{
    return w->mapToGlobal(w->rect().center());
}

bool near(const QColor& a, const QColor& b)
{
    return std::max({std::abs(a.red() - b.red()), std::abs(a.green() - b.green()), std::abs(a.blue() - b.blue())}) <=
           kNoise;
}

// Màu bán trong suốt (--c-hover) phủ lên nền đặc.
QColor over(const QColor& top, const QColor& under)
{
    const float a = top.alphaF();
    return QColor::fromRgbF(top.redF() * a + under.redF() * (1 - a), top.greenF() * a + under.greenF() * (1 - a),
                            top.blueF() * a + under.blueF() * (1 - a));
}

bool onScreen(const QWidget* w)
{
    return w->isVisible() && !w->visibleRegion().isEmpty();
}

// Đếm sự kiện timer giao cho mọi đối tượng, trừ `skip`.
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

// Đếm Esc tới một widget.
class EscCounter : public QObject {
public:
    int count = 0;

protected:
    bool eventFilter(QObject*, QEvent* e) override
    {
        if (e->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(e)->key() == Qt::Key_Escape)
            ++count;
        return false;
    }
};

} // namespace

class AutoHideTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;  // chủ đề, ngôn ngữ
    std::unique_ptr<settings::Store> layout_; // bố cục palette: mỗi test một kho mới
    std::unique_ptr<UiExecutor> executor_;
    std::unique_ptr<ThemeManager> tm_;
    std::unique_ptr<IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<QMainWindow> win_;
    std::unique_ptr<DisplayContext> display_;
    PaletteHost* host_ = nullptr;
    QWidget* canvas_ = nullptr;
    QWidget* top_ = nullptr;       // thanh tiêu đề + ribbon + tab tệp giả
    QStatusBar* bottom_ = nullptr; // tab layout + dòng lệnh + thanh trạng thái giả
    QLineEdit* cmd_ = nullptr;     // ô dòng lệnh giả
    QHash<QString, int> built_;    // số lần hàm dựng nội dung chạy, theo mã
    int stores_ = 0;

    int px(tok::PxId id) const { return qRound(tm_->px(id)); }
    // fullRibbon = false: ribbon thu gọn còn tên panel (.rb.min-panels .rb-body: --h-lbl + 8 px).
    int topHeight(bool fullRibbon = true) const
    {
        const int body = (fullRibbon ? 3 * px(tok::h_btn) : 0) + px(tok::sp_4) + px(tok::h_lbl);
        return px(tok::h_title) + px(tok::h_tab) + body + kLine + px(tok::h_ftab);
    }
    int bottomHeight(bool layoutTabs = true) const
    {
        return (layoutTabs ? px(tok::h_ltab) : 0) + 3 * px(tok::h_cmdl) + kHistoryPad + px(tok::h_cmdin) + kLine +
               px(tok::h_status);
    }

    QWidget* makeContent(const QString& id)
    {
        ++built_[id];
        auto* w = new QWidget;
        auto* col = new QVBoxLayout(w);
        col->setContentsMargins({});
        col->setSpacing(0);
        auto* search = new SearchField;
        search->setObjectName(u"search"_s);
        auto* list = new QListWidget;
        list->setObjectName(u"list"_s);
        for (int i = 0; i < 100; ++i)
            list->addItem(u"%1 %2"_s.arg(id).arg(i));
        col->addWidget(search);
        col->addWidget(list);
        return w;
    }

    PaletteSpec spec(const Pal& p)
    {
        const QString id = QString::fromLatin1(p.id);
        return {id,
                [t = p.title] { return QCoreApplication::translate("DockTest", t); },
                [this, id] { return makeContent(id); },
                p.area,
                p.visible,
                p.pinned};
    }

    // Cửa sổ mẫu ở `geometry`, bố cục đã lưu trong layout_ nếu có, mặc định theo lớp màn hình chứa nó; hiện, active,
    // DisplayContext theo dõi; focus ở dòng lệnh.
    bool build(QRect geometry = kFull)
    {
        win_ = std::make_unique<QMainWindow>();
        top_ = new QWidget;
        top_->setFixedHeight(topHeight());
        win_->setMenuWidget(top_);
        bottom_ = new QStatusBar;
        bottom_->setSizeGripEnabled(false);
        bottom_->setFixedHeight(bottomHeight());
        cmd_ = new QLineEdit;
        bottom_->addWidget(cmd_, 1);
        win_->setStatusBar(bottom_);
        canvas_ = new QWidget;
        canvas_->setFocusPolicy(Qt::StrongFocus);
        win_->setCentralWidget(canvas_);
        win_->setGeometry(geometry);
        host_ = new PaletteHost(win_.get(), *layout_);
        for (const Pal& p : kPalettes)
            host_->registerPalette(spec(p));
        host_->restoreLayout(DisplayContext::classify(win_->screen()->geometry().width()));
        win_->show();
        if (!QTest::qWaitForWindowActive(win_.get()))
            return false;
        display_ = std::make_unique<DisplayContext>(win_.get());
        host_->trackDisplay(*display_);
        tm_->setReducedMotion(false); // DisplayContext áp "giảm chuyển động" của máy chạy test (Windows)
        cmd_->setFocus();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        return cmd_->hasFocus();
    }

    QDockWidget* dock(const char* id) const { return host_->dock(QString::fromLatin1(id)); }
    AutoHideStrip* right() const { return host_->strip(Qt::RightDockWidgetArea); }
    QWidget* field(const char* id, const char* name) const
    {
        return host_->content(QString::fromLatin1(id))->findChild<QWidget*>(QString::fromLatin1(name));
    }
    // Khung trượt đã mở hết (animation xong).
    static bool opened(const AutoHideStrip* s)
    {
        return s->current() && s->flyout()->isVisible() && s->flyout()->geometry() == s->flyoutRect();
    }
    // Khung trượt (dải phải) đúng chỗ: viền phải đè nét chia của dải, dọc theo cả dải, rộng --pal-w + viền.
    bool besideStrip(const AutoHideStrip* s) const
    {
        const QRect f = s->flyout()->geometry();
        return f == s->flyoutRect() && f.right() == s->geometry().left() && f.top() == s->geometry().top() &&
               f.height() == s->height() && f.width() == px(tok::pal_w) + 2;
    }

public:
    // Sau QApplication (plugin đã đọc cấu hình màn hình), trước qExec: về lại thư mục chạy test (baseline).
    AutoHideTest() { QDir::setCurrent(g_cwd); }

    // Trước QApplication (QTEST_MAIN): bật lại co giãn DPI (môi trường test chung tắt), hệ số lẻ không làm tròn
    // (UI0-012), hai màn hình giả.
    static void initMain()
    {
        for (const char* v : {"QT_ENABLE_HIGHDPI_SCALING", "QT_SCALE_FACTOR", "QT_SCREEN_SCALE_FACTORS",
                              "QT_SCALE_FACTOR_ROUNDING_POLICY"})
            qunsetenv(v);
        DisplayContext::initMain();
        // Đường dẫn tương đối: tùy chọn của plugin ngăn cách bằng ':', đường dẫn Windows "C:/…" có ':'.
        static QTemporaryDir config;
        g_cwd = QDir::currentPath();
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
        QCOMPARE(QGuiApplication::screens().size(), qsizetype(std::size(kScreens)));
        QCOMPARE(screenNamed("p150")->devicePixelRatio(), 1.5);
        QVERIFY(dir_.isValid());
        // Linux: "giảm chuyển động" của hệ điều hành (DisplayContext) đọc $XDG_CONFIG_HOME, không lấy của máy chạy
        // test.
        QVERIFY(QDir(dir_.path()).mkpath(u"config"_s));
        qputenv("XDG_CONFIG_HOME", QFile::encodeName(dir_.filePath(u"config"_s)));
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        QVERIFY(loadFonts()); // chữ Plex nhúng: ảnh không phụ thuộc font của máy
        executor_ = std::make_unique<UiExecutor>();
        tm_ = std::make_unique<ThemeManager>(*store_);
        icons_ = std::make_unique<IconProvider>();
        translator_ = std::make_unique<i18n::Translator>();
        QApplication::setCursorFlashTime(0); // con trỏ ô nhập không nhấp nháy: ảnh tất định, rảnh không timer
    }

    void init()
    {
        tm_->setTheme(Theme::Dark);
        tm_->setDensity(Density::Compact);
        tm_->setLite(false);
        layout_ = std::make_unique<settings::Store>(dir_.filePath(u"layout-%1.json"_s.arg(++stores_)));
        built_.clear();
        mouseTo(kParked); // không để con trỏ nằm sẵn trên tab của dải (setPos cùng chỗ không sinh sự kiện)
    }

    void cleanup()
    {
        if (QWidget* p = QApplication::activePopupWidget())
            p->close();
        display_.reset();
        win_.reset(); // PaletteHost là con của cửa sổ
        host_ = nullptr;
        canvas_ = top_ = nullptr;
        bottom_ = nullptr;
        cmd_ = nullptr;
        layout_.reset();
        tm_->setLite(false);
        tm_->setReducedMotion(false);
        if (QGuiApplication::screens().size() != qsizetype(std::size(kScreens)))
            QVERIFY(setScreens(std::size(kScreens)));
        QCoreApplication::processEvents();
    }

    void cleanupTestCase()
    {
        display_.reset();
        win_.reset();
        layout_.reset();
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
    void defaults_data()
    {
        QTest::addColumn<QRect>("geometry");
        QTest::addColumn<bool>("wide");
        QTest::addRow("p100-1920") << kFull << true;
        QTest::addRow("p150-1280") << QRect(2000, 0, 1280, 720) << false; // 1920×1080 ở 150%: Small
    }

    void defaults()
    {
        QFETCH(QRect, geometry);
        QFETCH(bool, wide);
        QVERIFY(build(geometry));
        QCOMPARE(display_->screenClass() == ScreenClass::Wide, wide);
        const int strip = px(tok::strip);
        AutoHideStrip* left = host_->strip(Qt::LeftDockWidgetArea);
        QVERIFY(left && left->isVisible());
        QCOMPARE(left->palettes(), QList<QDockWidget*>{dock("tcvn")});
        QCOMPARE(left->geometry().left(), 0);   // sát mép cửa sổ
        QCOMPARE(left->width(), strip + kLine); // dải 22 px + nét chia phía vùng vẽ
        QCOMPARE(left->tab(dock("tcvn"))->width(), strip);
        // Tên cho trình đọc màn hình: dải (aria-label của prototype), tab là tiêu đề palette, khung là tiêu đề.
        QCOMPARE(left->accessibleName(), u"Palette tự ẩn"_s);
        QAccessibleInterface* a = QAccessible::queryAccessibleInterface(left->tab(dock("tcvn")));
        QVERIFY(a);
        QCOMPARE(a->text(QAccessible::Name), u"Thư viện TCVN"_s);
        QCOMPARE(canvas_->geometry().left(), left->geometry().right() + 1);
        if (wide) {
            QVERIFY(host_->isPinned(u"props"_s));
            QVERIFY(onScreen(dock("props")));
            QCOMPARE(dock("props")->width(), px(tok::pal_w));
            QVERIFY(!right());
            QCOMPARE(built_.value(u"props"_s), 1);
        } else {
            for (const QString& id : host_->ids()) // hẹp hơn 1600 px: mọi palette bỏ ghim
                QVERIFY2(!host_->isPinned(id), qPrintable(id));
            QVERIFY(right());
            QCOMPARE(right()->palettes(), QList<QDockWidget*>{dock("props")});
            QVERIFY(!dock("props")->isVisible());
            QCOMPARE(right()->geometry().right(), win_->width() - 1);
            QCOMPARE(right()->width(), strip + kLine);
            QCOMPARE(canvas_->geometry().right(), right()->geometry().left() - 1);
            QCOMPARE(built_.value(u"props"_s), 0); // tự ẩn: dựng khi trượt ra lần đầu
            // Palette đang đóng mở ra (lệnh, phím tắt): vào dải và trượt ra, vùng vẽ giữ nguyên.
            const QRect canvas = canvas_->geometry();
            host_->showPalette(u"layers"_s);
            QCOMPARE(right()->current(), dock("layers"));
            QCOMPARE(right()->palettes(), QList<QDockWidget*>({dock("props"), dock("layers")}));
            QCOMPARE(canvas_->geometry(), canvas);
            QCOMPARE(built_.value(u"layers"_s), 1);
        }
    }

    void canvasShare()
    {
        QVERIFY(build(kFull));
        const auto share = [&] {
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            return double(canvas_->width()) * canvas_->height() / (double(win_->width()) * win_->height());
        };
        QCOMPARE(win_->size(), kFull.size());
        const double open = share(); // Thuộc tính dock mở, TCVN ở dải trái
        host_->setPinned(u"props"_s, false);
        const double hidden = share(); // mọi palette tự ẩn
        top_->setFixedHeight(topHeight(false));
        const double minPanels = share(); // tự ẩn và ribbon thu gọn còn tên panel
        top_->hide();                     // Clean Screen giả
        bottom_->setFixedHeight(bottomHeight(false));
        const double clean = share();
        host_->setPinned(u"props"_s, true);
        const double cleanOpen = share();
        qInfo("vùng vẽ / cửa sổ 1920×1080 (khung trên %d px, dưới %d px): Thuộc tính mở %.1f%%, tự ẩn %.1f%%, tự ẩn "
              "+ ribbon thu gọn %.1f%%; Clean Screen: tự ẩn %.1f%%, Thuộc tính mở %.1f%%",
              topHeight(), bottomHeight(), open * 100, hidden * 100, minPanels * 100, clean * 100, cleanOpen * 100);
        QVERIFY2(open >= 0.58 && open <= 0.62, qPrintable(QString::number(open)));
        QVERIFY(hidden > open);
        QVERIFY2(minPanels >= 0.75, qPrintable(QString::number(minPanels)));
        QVERIFY2(clean >= 0.75, qPrintable(QString::number(clean)));
        QVERIFY2(cleanOpen >= 0.75, qPrintable(QString::number(cleanOpen)));
        // Số của chính prototype: khung trên + dưới 305 px nên tự ẩn mà còn đủ ribbon chỉ được ~70%; ≥ 75% cần ribbon
        // thu gọn hoặc Clean Screen (UIA). Báo lệch tiêu chí, không giả kết quả.
        QEXPECT_FAIL("",
                     "tự ẩn với ribbon đầy đủ ~70% (khung của prototype); ≥ 75% khi ribbon thu gọn hoặc Clean Screen",
                     Continue);
        QVERIFY(hidden >= 0.75);
    }

    // Tiêu chí 2.
    void hoverOpen()
    {
        QVERIFY(build(kFull));
        host_->setPinned(u"props"_s, false);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        AutoHideStrip* s = right();
        QVERIFY(s);
        QAbstractButton* tab = s->tab(dock("props"));
        auto* slide = s->flyout()->findChild<QPropertyAnimation*>();
        QVERIFY(slide);
        mouseTo(centerOf(canvas_));
        const QRect canvas = canvas_->geometry();
        QList<QRect> during; // hình học vùng vẽ ở mỗi khung của animation
        QObject frames;
        connect(slide, &QVariantAnimation::valueChanged, &frames, [&] { during << canvas_->geometry(); });

        QElapsedTimer t;
        t.start();
        mouseTo(centerOf(tab));
        QVERIFY(!s->current()); // chưa mở khi vừa rê
        QVERIFY(spinUntil([&] { return s->current() != nullptr; }, 2s));
        const qint64 hoverMs = t.elapsed();
        t.restart();
        QVERIFY(spinUntil([&] { return opened(s); }, 2s));
        const qint64 slideMs = t.elapsed();
        qInfo("rê tab → mở: %lld ms (timer %d ms); trượt ra: %lld ms (animation %d ms, %lld khung)", hoverMs,
              AutoHideStrip::kHoverMs, slideMs, slide->duration(), qint64(during.size()));
        QVERIFY2(hoverMs >= AutoHideStrip::kHoverMs - 1, qPrintable(QString::number(hoverMs))); // timer chính xác
        QCOMPARE(tm_->ms(tok::t), 120);
        QCOMPARE(slide->duration(), tm_->ms(tok::t)); // ≤ 120 ms (--t)
        QVERIFY(!during.isEmpty());
        for (const QRect& r : std::as_const(during)) // không đẩy vùng vẽ
            QCOMPARE(r, canvas);
        QCOMPARE(canvas_->geometry(), canvas);
        QVERIFY(besideStrip(s));
        QVERIFY(canvas.intersects(s->flyout()->geometry())); // phủ lên vùng vẽ
        QVERIFY(onScreen(host_->content(u"props"_s)));
        QVERIFY(cmd_->hasFocus()); // tiêu chí 4: không lấy focus

        // Rê vào khung: vẫn mở. Rời khỏi dải và khung: thu ngay.
        mouseTo(centerOf(s->flyout()));
        QVERIFY(s->current());
        mouseTo(centerOf(canvas_));
        QVERIFY(!s->current());
        QVERIFY(!s->flyout()->isVisible());
        QVERIFY(!tab->isChecked());
        QCOMPARE(canvas_->geometry(), canvas);
        // Rê rồi rời trước 300 ms: không mở.
        mouseTo(centerOf(tab));
        mouseTo(centerOf(canvas_));
        spinUntil([] { return false; }, 400ms);
        QVERIFY(!s->current());
        QVERIFY(cmd_->hasFocus());
    }

    void clickEsc()
    {
        QVERIFY(build(kFull));
        host_->setPinned(u"props"_s, false);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        AutoHideStrip* s = right();
        QAbstractButton* tab = s->tab(dock("props"));
        mouseTo(centerOf(tab));
        QTest::mouseClick(tab, Qt::LeftButton);
        QCOMPARE(s->current(), dock("props")); // bấm: mở ngay, không chờ 300 ms
        QVERIFY(tab->isChecked());
        QVERIFY(cmd_->hasFocus());
        QTest::mouseClick(tab, Qt::LeftButton); // bấm lại: thu
        QVERIFY(!s->current());
        QTest::mouseClick(tab, Qt::LeftButton);
        QVERIFY(s->current());

        // Esc khi focus ở dòng lệnh: khung thu, dòng lệnh không nhận Esc này; Esc sau đó là của dòng lệnh.
        EscCounter esc;
        cmd_->installEventFilter(&esc);
        QTest::keyClick(cmd_, Qt::Key_Escape);
        QVERIFY(!s->current());
        QCOMPARE(esc.count, 0);
        QTest::keyClick(cmd_, Qt::Key_Escape);
        QCOMPARE(esc.count, 1);
        cmd_->removeEventFilter(&esc);
        QVERIFY(cmd_->hasFocus());

        // Giao diện nhẹ, giảm chuyển động: --t = 0, hiện ngay khi bấm.
        for (bool lite : {true, false}) {
            tm_->setLite(lite);
            tm_->setReducedMotion(!lite);
            QCOMPARE(tm_->ms(tok::t), 0);
            QTest::mouseClick(tab, Qt::LeftButton);
            QVERIFY(opened(s));
            QVERIFY(besideStrip(s));
            QCOMPARE(s->flyout()->findChild<QPropertyAnimation*>()->state(), QAbstractAnimation::Stopped);
            QTest::mouseClick(tab, Qt::LeftButton);
            QVERIFY(!s->current());
        }
    }

    void pinBack()
    {
        QVERIFY(build(kFull));
        const QRect docked = canvas_->geometry();
        host_->setPinned(u"props"_s, false);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        AutoHideStrip* s = right();
        QVERIFY(s && s->isVisible());
        QVERIFY(canvas_->width() > docked.width()); // tự ẩn: vùng vẽ rộng ra
        host_->showPalette(u"props"_s);
        QCOMPARE(s->current(), dock("props"));

        // Nút ghim trên khung trượt: về đúng chỗ dock cũ, cùng nội dung.
        QSignalSpy pinned(host_, &PaletteHost::pinnedChanged);
        host_->header(u"props"_s)->pinButton()->click();
        QCOMPARE(pinned.size(), 1);
        QVERIFY(host_->isPinned(u"props"_s));
        QVERIFY(s->palettes().isEmpty());
        QVERIFY(!s->isVisible());
        QVERIFY(!s->flyout()->isVisible());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QVERIFY(onScreen(dock("props")));
        QCOMPARE(dock("props")->titleBarWidget(), host_->header(u"props"_s));
        QCOMPARE(dock("props")->width(), px(tok::pal_w));
        QCOMPARE(canvas_->geometry(), docked);
        QCOMPARE(built_.value(u"props"_s), 1); // không dựng lại

        // Đóng palette tự ẩn: ra khỏi dải, vẫn bỏ ghim; mở lại thì trượt ra.
        host_->setPinned(u"props"_s, false);
        host_->showPalette(u"props"_s);
        host_->header(u"props"_s)->closeButton()->click();
        QVERIFY(s->palettes().isEmpty());
        QVERIFY(!dock("props")->isVisible());
        QVERIFY(!host_->isPinned(u"props"_s));
        host_->showPalette(u"props"_s);
        QCOMPARE(s->current(), dock("props"));
        QCOMPARE(built_.value(u"props"_s), 1);

        // Chuyển chỗ (menu của đầu palette): ghim, dock ở chỗ mới.
        host_->moveTo(u"props"_s, Qt::LeftDockWidgetArea);
        QVERIFY(host_->isPinned(u"props"_s));
        QCOMPARE(win_->dockWidgetArea(dock("props")), Qt::LeftDockWidgetArea);
        QVERIFY(onScreen(dock("props")));
        QVERIFY(!s->isVisible());
        // Bỏ ghim palette nổi: về dock mặc định của nó rồi vào dải ở cạnh đó.
        host_->moveTo(u"layers"_s, Qt::NoDockWidgetArea);
        QVERIFY(dock("layers")->isFloating());
        host_->setPinned(u"layers"_s, false);
        QVERIFY(!dock("layers")->isFloating());
        QCOMPARE(s->palettes(), QList<QDockWidget*>{dock("layers")});
        QCOMPARE(win_->dockWidgetArea(dock("layers")), Qt::RightDockWidgetArea);
    }

    void restore()
    {
        QVERIFY(build(kFull));
        host_->setPinned(u"props"_s, false);
        host_->setPinned(u"count"_s, false); // đang đóng: chỉ bỏ ghim
        host_->showPalette(u"layers"_s);     // ghim: dock ở phải
        host_->setPinned(u"layers"_s, false);
        QCOMPARE(right()->palettes(), QList<QDockWidget*>({dock("props"), dock("layers")}));
        QVERIFY(win_->close()); // QEvent::Close → saveLayout
        win_.reset();
        display_.reset();

        built_.clear();
        QVERIFY(build(kFull));
        QCOMPARE(right()->palettes(), QList<QDockWidget*>({dock("props"), dock("layers")}));
        QCOMPARE(host_->strip(Qt::LeftDockWidgetArea)->palettes(), QList<QDockWidget*>{dock("tcvn")});
        QVERIFY(!host_->strip(Qt::BottomDockWidgetArea));
        for (const char* id : {"props", "layers", "tcvn", "count"})
            QVERIFY2(!host_->isPinned(QString::fromLatin1(id)), id);
        QCOMPARE(built_.size(), 0);     // chưa trượt ra thì chưa dựng
        host_->showPalette(u"count"_s); // bỏ ghim đã lưu: mở ra là trượt ra từ dải dưới
        AutoHideStrip* bottom = host_->strip(Qt::BottomDockWidgetArea);
        QVERIFY(bottom);
        QCOMPARE(bottom->current(), dock("count"));
        QCOMPARE(bottom->geometry().bottom() + 1, bottom_->geometry().top());
        QCOMPARE(bottom->height(), px(tok::strip) + kLine);
        // Dải dưới vừa tạo: khung trượt lên từ dải, ngang theo dải, viền dưới đè nét chia của dải.
        QVERIFY(spinUntil([&] { return opened(bottom); }));
        const QRect f = bottom->flyout()->geometry();
        QCOMPARE(f.bottom(), bottom->geometry().top());
        QCOMPARE(f.left(), bottom->geometry().left());
        QCOMPARE(f.width(), bottom->width());
    }

    // Tiêu chí 3.
    void dpiFloating()
    {
        QVERIFY(build(kMid));
        host_->moveTo(u"layers"_s, Qt::NoDockWidgetArea);
        QDockWidget* d = dock("layers");
        QVERIFY(d->isFloating());
        d->setGeometry(QRect(300, 80, 272, 900)); // cao hơn vùng làm việc 720 px logic của p150
        QVERIFY(spinUntil([&] { return d->screen() == screenNamed("p100"); }));
        QWidget* content = host_->content(u"layers"_s);
        static_cast<QLineEdit*>(field("layers", "search"))->setText(u"abc"_s); // i18n-ok: chữ gõ thử
        static_cast<QListWidget*>(field("layers", "list"))->setCurrentRow(42);

        QSignalSpy dpr(display_.get(), &DisplayContext::devicePixelRatioChanged);
        d->setGeometry(QRect(2100, 50, 272, 900)); // hệ điều hành dời sang màn hình 150% (không đang kéo)
        QVERIFY(spinUntil([&] { return d->screen() == screenNamed("p150") && d->devicePixelRatio() == 1.5; }));
        QVERIFY(spinUntil([&] {
            return std::any_of(dpr.cbegin(), dpr.cend(),
                               [&](const QList<QVariant>& a) { return a[0].value<QWidget*>() == d; });
        }));
        const QRect work = screenNamed("p150")->availableGeometry();
        QVERIFY2(
            work.contains(d->geometry()),
            qPrintable(
                u"%1,%2 %3×%4"_s.arg(d->x()).arg(d->y()).arg(d->width()).arg(d->height()))); // i18n-ok: nhật ký test
        QVERIFY(d->isFloating());
        QVERIFY(host_->isPinned(u"layers"_s));
        QCOMPARE(host_->content(u"layers"_s), content);
        QCOMPARE(static_cast<QLineEdit*>(field("layers", "search"))->text(), u"abc"_s);
        QCOMPARE(static_cast<QListWidget*>(field("layers", "list"))->currentRow(), 42);
        QCOMPARE(built_.value(u"layers"_s), 1);
        // Palette dock của cửa sổ chính không đổi.
        QVERIFY(onScreen(dock("props")));
        QCOMPARE(dock("props")->width(), px(tok::pal_w));
    }

    void dpiFlyout()
    {
        QVERIFY(build(kMid));
        host_->setPinned(u"props"_s, false);
        AutoHideStrip* s = right();
        host_->showPalette(u"props"_s);
        QVERIFY(spinUntil([&] { return opened(s); }));
        QWidget* content = host_->content(u"props"_s);
        static_cast<QLineEdit*>(field("props", "search"))->setText(u"abc"_s); // i18n-ok: chữ gõ thử

        win_->setGeometry(QRect(2100, 50, 1100, 650)); // cửa sổ sang màn hình 150%, hệ điều hành đặt cỡ mới
        QVERIFY(spinUntil([&] { return win_->devicePixelRatio() == 1.5; }));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCOMPARE(s->current(), dock("props")); // vẫn mở, cùng palette
        QVERIFY(besideStrip(s));
        QCOMPARE(s->flyout()->devicePixelRatio(), 1.5);
        QCOMPARE(host_->content(u"props"_s), content);
        QCOMPARE(static_cast<QLineEdit*>(field("props", "search"))->text(), u"abc"_s);
        const QImage shot = s->flyout()->grab().toImage(); // vẽ ở DPR mới
        QCOMPARE(shot.size(), s->flyout()->size() * 1.5);
        // Dải trái (TCVN) cũng theo cỡ mới.
        AutoHideStrip* left = host_->strip(Qt::LeftDockWidgetArea);
        QCOMPARE(left->height(), s->height());
        QCOMPARE(built_.value(u"props"_s), 1);
    }

    void unplug()
    {
        QVERIFY(build(QRect(2050, 30, 1100, 650))); // cửa sổ chính trên màn hình 150%
        host_->setPinned(u"props"_s, false);
        AutoHideStrip* s = right();
        host_->showPalette(u"props"_s);
        QVERIFY(spinUntil([&] { return opened(s); }));
        static_cast<QLineEdit*>(field("props", "search"))->setText(u"abc"_s); // i18n-ok: chữ gõ thử
        host_->moveTo(u"layers"_s, Qt::NoDockWidgetArea);
        QDockWidget* d = dock("layers");
        d->setGeometry(QRect(2400, 100, 272, 500));
        QVERIFY(spinUntil([&] { return d->screen() == screenNamed("p150"); }));
        static_cast<QLineEdit*>(field("layers", "search"))->setText(u"xyz"_s); // i18n-ok: chữ gõ thử
        QWidget* props = host_->content(u"props"_s);
        QWidget* layers = host_->content(u"layers"_s);

        QVERIFY(setScreens(1)); // rút màn hình 150%
        QScreen* main = screenNamed("p100");
        QVERIFY(spinUntil([&] { return win_->screen() == main && d->screen() == main; }));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        // Palette nổi: về trọn vùng làm việc của màn hình còn lại, giữ nội dung và trạng thái.
        QVERIFY(main->availableGeometry().contains(d->geometry()));
        QVERIFY(d->isFloating() && d->isVisible());
        QCOMPARE(d->devicePixelRatio(), 1.0);
        QCOMPARE(host_->content(u"layers"_s), layers);
        QCOMPARE(static_cast<QLineEdit*>(field("layers", "search"))->text(), u"xyz"_s);
        // Đang trượt ra: vẫn mở, theo dải, DPR mới.
        QCOMPARE(s->current(), dock("props"));
        QVERIFY(besideStrip(s));
        QCOMPARE(s->flyout()->devicePixelRatio(), 1.0);
        QCOMPARE(host_->content(u"props"_s), props);
        QCOMPARE(static_cast<QLineEdit*>(field("props", "search"))->text(), u"abc"_s);
        QCOMPARE(built_.value(u"props"_s), 1);
        QCOMPARE(built_.value(u"layers"_s), 1);

        QVERIFY(setScreens(2)); // cắm lại
        QVERIFY(screenNamed("p150"));
    }

    // Tiêu chí 4.
    void focus()
    {
        QVERIFY(build(kFull));
        host_->setPinned(u"props"_s, false);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        AutoHideStrip* s = right();
        QAbstractButton* tab = s->tab(dock("props"));
        mouseTo(centerOf(tab));
        QTest::mouseClick(tab, Qt::LeftButton);
        QVERIFY(spinUntil([&] { return opened(s); }));
        QVERIFY(cmd_->hasFocus()); // bấm tab: dòng lệnh giữ focus

        // Bấm vào palette: có focus; rời chuột khi đang làm việc trong palette: vẫn mở.
        auto* search = static_cast<QLineEdit*>(field("props", "search"));
        mouseTo(centerOf(search));
        QTest::mouseClick(search, Qt::LeftButton);
        QVERIFY(search->hasFocus());
        mouseTo(centerOf(canvas_));
        QVERIFY(s->current());
        // Esc của ô tìm còn chữ là của ô; ô rỗng thì Esc thu khung, focus về vùng vẽ.
        QTest::keyClicks(search, u"ab"_s);
        QTest::keyClick(search, Qt::Key_Escape);
        QVERIFY(search->text().isEmpty());
        QVERIFY(s->current());
        QTest::keyClick(search, Qt::Key_Escape);
        QVERIFY(!s->current());
        QVERIFY(canvas_->hasFocus());

        // Bấm vùng vẽ khi focus trong palette: focus rời palette → thu.
        mouseTo(centerOf(tab));
        QTest::mouseClick(tab, Qt::LeftButton);
        mouseTo(centerOf(search));
        QTest::mouseClick(search, Qt::LeftButton);
        mouseTo(centerOf(canvas_));
        QTest::mouseClick(canvas_, Qt::LeftButton);
        QVERIFY(canvas_->hasFocus());
        QVERIFY(!s->current());

        // Bàn phím: tab của dải nhận focus bằng Tab (không bằng chuột); Space mở và đưa focus vào palette, Esc thu
        // và về vùng vẽ.
        QCOMPARE(tab->focusPolicy(), Qt::TabFocus);
        tab->setFocus(Qt::TabFocusReason);
        QTest::keyClick(tab, Qt::Key_Space);
        QCOMPARE(s->current(), dock("props"));
        QVERIFY(s->flyout()->isAncestorOf(QApplication::focusWidget()));
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
        QVERIFY(!s->current());
        QVERIFY(canvas_->hasFocus());
        // F6 (focusNextPalette) vào palette đang trượt ra.
        mouseTo(centerOf(tab));
        QTest::mouseClick(tab, Qt::LeftButton);
        QVERIFY(spinUntil([&] { return opened(s); }));
        host_->focusNextPalette();
        QVERIFY(s->flyout()->isAncestorOf(QApplication::focusWidget()));
    }

    // Tiêu chí 5 (DoD-U 1, 2).
    void baseline_data()
    {
        QTest::addColumn<bool>("light");
        QTest::addColumn<qreal>("dpr");
        for (bool light : {false, true})
            for (qreal dpr : {1.0, 1.5, 2.0})
                QTest::addRow("%s-%d", light ? "light" : "dark", qRound(dpr * 100)) << light << dpr;
    }

    void baseline()
    {
        QFETCH(bool, light);
        QFETCH(qreal, dpr);
        tm_->setTheme(light ? Theme::Light : Theme::Dark);
        QCoreApplication::processEvents(); // Qt 6.11 gửi ApplicationPaletteChange qua hàng đợi
        QVERIFY(build(kMid));
        tm_->setReducedMotion(true); // mở ngay
        // Dải phải: Thuộc tính (đang mở), Layer (đang rê).
        host_->setPinned(u"props"_s, false);
        host_->setPinned(u"layers"_s, false);
        host_->showPalette(u"layers"_s);
        AutoHideStrip* s = right();
        s->open(dock("props"));
        s->tab(dock("layers"))->setAttribute(Qt::WA_UnderMouse);
        QCoreApplication::sendPostedEvents();
        const int sw = s->width(), fw = s->flyout()->width(), h = kShotHeight;
        QCOMPARE(sw, px(tok::strip) + kLine);
        QCOMPARE(fw, px(tok::pal_w) + 2);

        // Như trên màn hình: dải rồi khung trượt nằm trên, viền phải của khung đè nét chia của dải.
        const int x0 = fw - 1;
        QCOMPARE(s->flyout()->geometry().right(), s->geometry().left());
        QImage sheet(QSize(x0 + sw, h) * dpr, QImage::Format_ARGB32_Premultiplied);
        sheet.setDevicePixelRatio(dpr);
        sheet.fill(tm_->color(tok::c_base));
        QPainter p(&sheet);
        s->render(&p, QPoint(x0, 0), QRegion(0, 0, sw, h), QWidget::DrawChildren);
        s->flyout()->render(&p, QPoint(0, 0), QRegion(0, 0, fw, h), QWidget::DrawChildren);
        p.end();
        const QString name = u"autohide-%1-%2-%3.png"_s.arg(light ? u"light"_s : u"dark"_s)
                                 .arg(qRound(dpr * 100))
                                 .arg(QLatin1StringView(kPlatform));
        const QString out = qEnvironmentVariable("TRION_UPDATE_BASELINE");
        if (!out.isEmpty())
            QVERIFY(sheet.save(out + u'/' + name, "PNG", 0));

        // Màu token ở chỗ không có chữ: nền dải, viền khung trượt (hai bên), nền tab đang mở và đang rê.
        const auto at = [&](int x, int y) { return sheet.pixelColor(int((x + 0.5) * dpr), int((y + 0.5) * dpr)); };
        const QRect open = s->tab(dock("props"))->geometry();
        const QRect hover = s->tab(dock("layers"))->geometry();
        QVERIFY(hover.bottom() < h - 4);
        QVERIFY(near(at(x0 + sw / 2, h - 2), tm_->color(tok::c_chrome)));
        QVERIFY(near(at(0, h / 2), tm_->color(tok::c_border_ctl)));
        QVERIFY(near(at(x0, h / 2), tm_->color(tok::c_border_ctl)));
        const QColor hot = over(tm_->color(tok::c_hover), tm_->color(tok::c_chrome));
        QVERIFY(near(at(x0 + open.left() + 2, open.top() + 3), hot));
        QVERIFY(near(at(x0 + hover.left() + 2, hover.top() + 3), hot));
        if (!out.isEmpty())
            QSKIP("đã ghi baseline");

        const QImage want = QImage(u":/baseline/"_s + name).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QVERIFY2(!want.isNull(), qPrintable(u"thiếu baseline "_s + name));
        QCOMPARE(sheet.size(), want.size());
        qsizetype off = 0;
        int maxDiff = 0;
        for (int y = 0; y < sheet.height(); ++y) {
            const auto* a = reinterpret_cast<const QRgb*>(sheet.constScanLine(y));
            const auto* b = reinterpret_cast<const QRgb*>(want.constScanLine(y));
            for (int x = 0; x < sheet.width(); ++x) {
                const int d = std::max({std::abs(qRed(a[x]) - qRed(b[x])), std::abs(qGreen(a[x]) - qGreen(b[x])),
                                        std::abs(qBlue(a[x]) - qBlue(b[x])), std::abs(qAlpha(a[x]) - qAlpha(b[x]))});
                maxDiff = std::max(maxDiff, d);
                off += d > kNoise;
            }
        }
        qInfo("%s: %lld điểm ảnh lệch > %d/255, lệch lớn nhất %d/255", qPrintable(name), qint64(off), kNoise, maxDiff);
        if (off > sheet.width() * sheet.height() / 1000)
            sheet.save(QDir::temp().filePath(u"got-"_s + name)); // để so bằng mắt
        QVERIFY(off <= sheet.width() * sheet.height() / 1000);
    }

    void pseudoNoTruncation()
    {
        QVERIFY(build(kFull));
        tm_->setReducedMotion(true);
        host_->setPinned(u"props"_s, false);
        host_->setPinned(u"layers"_s, false);
        host_->showPalette(u"layers"_s);
        right()->open(dock("props"));
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QCoreApplication::processEvents(); // LanguageChange → tiêu đề, tab của dải; bố cục lại
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QStringList cut;
        for (const i18n::Truncation& t : i18n::findTruncations(win_.get()))
            if (!t.widget->visibleRegion().isEmpty()) // tab nền của nhóm dock: Qt dời ra ngoài cửa sổ
                cut << t.toString();
        const bool translated = dock("props")->windowTitle() != u"Thuộc tính"_s;
        const QAbstractButton* tab = right()->tab(dock("props"));
        const bool fits = tab->size() == tab->sizeHint(); // dải xếp lại theo chữ dài hơn
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QVERIFY(translated);
        QVERIFY(fits);
        QVERIFY2(cut.isEmpty(), qPrintable(cut.join(u'\n')));
    }

    void idleNoTimers()
    {
        QVERIFY(build(kFull));
        host_->setPinned(u"props"_s, false);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        AutoHideStrip* s = right();
        mouseTo(centerOf(canvas_));
        mouseTo(centerOf(s->tab(dock("props"))));
        QVERIFY(spinUntil([&] { return opened(s); }));
        mouseTo(centerOf(canvas_));
        QVERIFY(!s->current());
        sched::waitIdle();
        spinUntil([] { return false; }, 200ms); // việc còn chờ (vẽ, bố cục, biểu tượng raster trước) xong

        // 300 ms rảnh: không sự kiện timer nào (trừ timer kết thúc vòng chờ của test).
        TimerCounter counter;
        QEventLoop loop;
        QTimer stop;
        stop.setSingleShot(true);
        connect(&stop, &QTimer::timeout, &loop, &QEventLoop::quit);
        counter.skip = &stop;
        qApp->installEventFilter(&counter);
        stop.start(300);
        loop.exec();
        qApp->removeEventFilter(&counter);
        QVERIFY2(counter.seen.isEmpty(), qPrintable(counter.seen.join(u','))); // i18n-ok: tên lớp
        QAbstractEventDispatcher* events = QAbstractEventDispatcher::instance();
        for (QObject* o : win_->findChildren<QObject*>())
            QVERIFY2(events->timersForObject(o).isEmpty(), o->metaObject()->className());
    }
};

QTEST_MAIN(AutoHideTest)
#include "autohide_test.moc"
