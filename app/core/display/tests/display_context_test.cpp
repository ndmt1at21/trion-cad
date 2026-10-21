// UI0-012 · DisplayContext. Plugin offscreen nhiều màn hình (initMain): 100/125/150/175/200% và 1366, 1599 px logic.
// Tiêu chí 1: kéo cửa sổ chính, palette nổi sang màn hình tỉ lệ khác → tín hiệu (vùng vẽ giả nghe), biểu tượng raster
// lại đúng cỡ pixel mới; đến hết khung đầu ≤ 100 ms (UiProbe ui_state, kiểm ở bản release, vòng tốt nhất).
// 2: 125%, 175% không làm tròn; bảng mẫu tạm ở 5 tỉ lệ khớp baseline theo nền tảng. 3: ScreenClass ở 1366, 1599,
// 1600, 1920 px. 4: gọn ↔ thoáng ở mọi DPI, Giao diện nhẹ (API ThemeManager), giảm chuyển động của hệ điều hành.
#include "app/core/display/DisplayContext.h"

#include "app/core/perf/UiProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "core/base/assert.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"
#include "tests/timing.h"

#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QPushButton>
#include <QScreen>
#include <QSignalSpy>
#include <QStringListModel>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <array>
#include <limits>
#include <memory>

using namespace Qt::StringLiterals;
using namespace trion;
using theme::Density;
using theme::IconProvider;
using theme::IconState;
using theme::TextRole;
using theme::Theme;
using theme::ThemeManager;
using ui::DisplayContext;
using ui::ScreenClass;
namespace probe = ui::probe;

namespace {

// Màn hình giả của plugin offscreen. Qt giữ gốc toạ độ (px thiết bị) làm gốc logic, cỡ logic = cỡ thiết bị / tỉ lệ.
struct Screen {
    const char* name;
    int x, width, height, dpi; // dpi logic: 96 = 100%
    qreal dpr;
    int logicalWidth;
};
constexpr Screen kScreens[] = {
    {"p100", 0, 1920, 1080, 96, 1.0, 1920},      // Wide
    {"p125", 2000, 1920, 1080, 120, 1.25, 1536}, // Narrow
    {"p150", 4000, 1920, 1080, 144, 1.5, 1280},  // Small
    {"p175", 6000, 2800, 1575, 168, 1.75, 1600}, // Wide
    {"p200", 9000, 3200, 1800, 192, 2.0, 1600},  // Wide
    {"w1366", 13000, 1366, 768, 96, 1.0, 1366},  // Small
    {"w1599", 15000, 1599, 900, 96, 1.0, 1599},  // Narrow
};
constexpr QSize kSheet{360, 240}; // bảng mẫu: chia hết cho 4 nên ở 125%, 175% vẫn là số pixel nguyên

QString g_cwd; // thư mục chạy test (ctest); initMain đổi sang thư mục tạm chứa cấu hình màn hình tới khi tạo lớp test
int g_asserts = 0;

const Screen& spec(const char* name)
{
    const Screen* s = std::find_if(std::begin(kScreens), std::end(kScreens),
                                   [&](const Screen& x) { return qstrcmp(x.name, name) == 0; });
    TRION_CHECK(s != std::end(kScreens));
    return *s;
}

QScreen* screenNamed(const char* name)
{
    for (QScreen* s : QGuiApplication::screens())
        if (s->name() == QLatin1StringView(name))
            return s;
    return nullptr;
}

QPoint inside(const char* name)
{
    return screenNamed(name)->geometry().topLeft() + QPoint(40, 40);
}

// Như kéo: tâm cửa sổ sang màn hình kia thì Qt đổi màn hình và DPR, kích thước logic giữ nguyên.
bool moveTo(QWidget& w, const char* name)
{
    w.move(inside(name));
    return QTest::qWaitFor([&] { return w.screen() == screenNamed(name); });
}

// Việc raster trước ở nền xong và kết quả đã vào cache: stats().rasters không đổi giữa chừng một bước kiểm. Executor
// chạy kết quả theo lượt ≤ 3 ms nên chờ tới khi một callback gửi sau cùng đã chạy.
void settle()
{
    sched::waitIdle();
    bool done = false;
    ui::post([&done] { done = true; });
    while (!done)
        QCoreApplication::processEvents();
}

QJsonObject uiState()
{
    return probe::toJson()["ops"_L1]["ui_state"_L1].toObject();
}

// Đợi hết khung kế tiếp của cửa sổ chứa w (UiProbe, mẫu ui_state tính từ start).
bool waitFrame(QWidget& w, probe::Clock::time_point start)
{
    const int n = uiState()["count"_L1].toInt();
    probe::untilFrame(probe::Op::UiState, &w, start);
    return QTest::qWaitFor([&] { return uiState()["count"_L1].toInt() > n; }, 10000);
}

// n tên ngoài bộ Viewer (không được raster trước: phải raster khi vẽ ở DPR mới), bỏ qua skip tên đầu.
QStringList nonViewer(int n, int skip = 0)
{
    const QStringList viewer = IconProvider::names(true);
    QStringList out;
    for (const QString& name : IconProvider::names())
        if (!viewer.contains(name) && skip-- <= 0 && out.size() < n)
            out << name;
    return out;
}

// Giống khung chính: nút biểu tượng 16 px (hàng 25 nút), nút 32 px có nhãn như ribbon, vài dòng chữ Plex.
std::unique_ptr<QWidget> ribbon(const QStringList& small, const QStringList& large, Qt::WindowFlags flags = {})
{
    auto w = std::make_unique<QWidget>(nullptr, flags);
    auto* grid = new QGridLayout(w.get());
    for (int i = 0; i < small.size(); ++i) {
        auto* b = new QToolButton;
        b->setIcon(IconProvider::icon(small[i]));
        b->setIconSize({16, 16});
        b->setAutoRaise(true);
        grid->addWidget(b, i / 25, i % 25);
    }
    const int row = int(small.size() + 24) / 25;
    for (int i = 0; i < large.size(); ++i) {
        auto* b = new QToolButton;
        b->setIcon(IconProvider::icon(large[i]));
        b->setIconSize({32, 32});
        b->setText(u"Lệnh %1"_s.arg(i + 1));  // i18n-ok: chữ mẫu của test
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setAutoRaise(true);
        grid->addWidget(b, row + i / 10, (i % 10) * 2, 1, 2);
    }
    const QString prop = u"Thuộc tính %1: Lớp “Kích thước”, Màu, Kiểu đường, Độ dày nét 0,25 mm"_s;
    for (int i = 0; i < 4; ++i)
        grid->addWidget(new QLabel(prop.arg(i + 1)), row + 2 + i, 0, 1, 25);
    for (QWidget* c : w->findChildren<QWidget*>())
        c->setFocusPolicy(Qt::NoFocus);
    return w;
}

// Bảng mẫu tạm cho tiêu chí 2 (UI0-006 làm bảng mẫu thật): widget Qt chuẩn theo chủ đề, biểu tượng 16/32 px, chữ Plex
// Sans/Mono tiếng Việt. Không widget nào nhận focus: không con trỏ nhấp nháy trong ảnh.
std::unique_ptr<QWidget> sampleSheet()
{
    auto w = std::make_unique<QWidget>();
    w->setFixedSize(kSheet);
    auto* g = new QGridLayout(w.get());
    auto* title = new QLabel(u"Thuộc tính — Lớp “Kích thước” đã khóa"_s);  // i18n-ok: chữ mẫu của test
    title->setFont(theme::font(TextRole::Heading, theme::Weight::Bold));
    g->addWidget(title, 0, 0, 1, 2);
    auto* coords = new QLabel(u"X 1.250,00  Y 830,50  Đ 45°"_s);  // i18n-ok: chữ mẫu của test
    coords->setFont(theme::font(TextRole::MonoCaption));
    g->addWidget(coords, 1, 0, 1, 2);
    g->addWidget(new QLineEdit(u"Độ dày nét 0,25 mm"_s), 2, 0);
    auto* combo = new QComboBox;
    combo->addItem(u"Liên tục"_s);  // i18n-ok: chữ mẫu của test
    g->addWidget(combo, 2, 1);
    g->addWidget(new QPushButton(IconProvider::icon(u"layers"_s), u"Lớp"_s), 3, 0);  // i18n-ok: chữ mẫu của test
    auto* check = new QCheckBox(u"Bắt điểm đối tượng"_s);  // i18n-ok: chữ mẫu của test
    check->setChecked(true);
    g->addWidget(check, 3, 1);
    auto* tools = new QHBoxLayout;
    for (const QString& n : {u"osnap"_s, u"zoom"_s, u"lw"_s, u"bulboff"_s}) {
        auto* b = new QToolButton;
        b->setIcon(IconProvider::icon(n));
        b->setAutoRaise(true);
        tools->addWidget(b);
    }
    auto* big = new QToolButton;
    big->setIcon(IconProvider::icon(u"layers"_s));
    big->setIconSize({32, 32});
    big->setAutoRaise(true);
    tools->addWidget(big);
    tools->addStretch();
    g->addLayout(tools, 4, 0, 1, 2);
    auto* list = new QListView;
    list->setModel(new QStringListModel({u"Lớp 0"_s, u"Đường tâm"_s, u"Chữ ghi chú"_s}, list));
    g->addWidget(list, 5, 0, 1, 2);
    for (QWidget* c : w->findChildren<QWidget*>())
        c->setFocusPolicy(Qt::NoFocus);
    return w;
}

void countAssert(const char*, const char*, int)
{
    ++g_asserts;
}

bool writeFile(const QString& path, const QByteArray& data)
{
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
}

} // namespace

class DisplayContextTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<ThemeManager> tm_;

public:
    // Sau QApplication (plugin đã đọc cấu hình màn hình), trước qExec: về lại thư mục chạy test (tệp -o, baseline).
    DisplayContextTest() { QDir::setCurrent(g_cwd); }

    // Trước QApplication (QTEST_MAIN). Môi trường test chung tắt co giãn DPI (QT_ENABLE_HIGHDPI_SCALING=0): bật lại và
    // dựng các màn hình giả ở trên. Đặt Round trước để thấy initMain() đổi sang PassThrough.
    static void initMain()
    {
        for (const char* v : {"QT_ENABLE_HIGHDPI_SCALING", "QT_SCALE_FACTOR", "QT_SCREEN_SCALE_FACTORS",
                              "QT_SCALE_FACTOR_ROUNDING_POLICY"})
            qunsetenv(v);
        QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::Round);
        DisplayContext::initMain();

        QJsonArray screens;
        for (const Screen& s : kScreens)
            screens.append(QJsonObject{{u"name"_s, QLatin1StringView(s.name)},
                                       {u"x"_s, s.x},
                                       {u"y"_s, 0},
                                       {u"width"_s, s.width},
                                       {u"height"_s, s.height},
                                       {u"logicalDpi"_s, s.dpi},
                                       {u"logicalBaseDpi"_s, 96},
                                       {u"dpr"_s, 1}});
        // Đường dẫn tương đối: tùy chọn của plugin ngăn cách bằng ':', đường dẫn Windows "C:/…" có ':'.
        static QTemporaryDir config;
        g_cwd = QDir::currentPath();
        if (!config.isValid() || !QDir::setCurrent(config.path()) ||
            !writeFile(u"screens.json"_s, QJsonDocument(QJsonObject{{u"screens"_s, screens}}).toJson()))
            qFatal("không ghi được cấu hình màn hình");
        qputenv("QT_QPA_PLATFORM", "offscreen:configfile=screens.json");
    }

private slots:
    void initTestCase()
    {
        QCOMPARE(QDir::currentPath(), g_cwd);
        QCOMPARE(QGuiApplication::screens().size(), qsizetype(std::size(kScreens)));
        QVERIFY(dir_.isValid());
        // Linux: systemReducedMotion() đọc $XDG_CONFIG_HOME, không để cấu hình của máy chạy test lọt vào.
        QVERIFY(QDir(dir_.path()).mkpath(u"config"_s));
        qputenv("XDG_CONFIG_HOME", QFile::encodeName(dir_.filePath(u"config"_s)));
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        executor_ = std::make_unique<ui::UiExecutor>();
        QVERIFY(theme::loadFonts());
        tm_ = std::make_unique<ThemeManager>(*store_);
        probe::enable();
    }

    void init()
    {
        tm_->setTheme(Theme::Dark);
        tm_->setDensity(Density::Compact);
        tm_->setLite(false);
        tm_->setReducedMotion(false);
        probe::reset();
    }

    void cleanupTestCase()
    {
        probe::disable();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        sched::waitIdle();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    // Tiêu chí 2: DPR đúng tỉ lệ màn hình, 125% và 175% không làm tròn; ảnh chụp đúng cỡ pixel. initMain() gọi sau
    // QApplication là lỗi lập trình (TRION_CHECK).
    void fractionalScaleNotRounded()
    {
        QCOMPARE(QGuiApplication::highDpiScaleFactorRoundingPolicy(),
                 Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
        QWidget w;
        w.resize(400, 300);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        for (const Screen& s : kScreens) {
            QScreen* screen = screenNamed(s.name);
            QVERIFY2(screen, s.name);
            QCOMPARE(screen->devicePixelRatio(), s.dpr);
            QCOMPARE(screen->geometry().width(), s.logicalWidth);
            QVERIFY2(moveTo(w, s.name), s.name);
            QCOMPARE(w.devicePixelRatio(), s.dpr);
            QCOMPARE(w.size(), QSize(400, 300));
            QCOMPARE(w.grab().size(), QSize(400, 300) * s.dpr); // 500×375 ở 125%, 700×525 ở 175%
        }

        g_asserts = 0;
        const AssertHandler old = setAssertHandler(&countAssert);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"setHighDpiScaleFactorRoundingPolicy"_s));
        DisplayContext::initMain();
        setAssertHandler(old);
        QCOMPARE(g_asserts, 1);
    }

    // Tiêu chí 1: kéo cửa sổ chính qua 100 → 125 → 150 → 175 → 200 → 100 → 150%. Mỗi lần: đúng một tín hiệu DPR (Qt
    // gửi DevicePixelRatioChange hai lần) và một tín hiệu màn hình; vùng vẽ giả nhận DPR mới, lúc đó screenClass() đã
    // mới. Khung đầu raster đúng một lần mỗi biểu tượng ngoài bộ Viewer (bộ Viewer đã raster trước ở DPR mọi màn hình)
    // ở đúng cỡ pixel mới: lấy lại cùng cỡ thì 0 raster; DPR gặp lại: 0 raster.
    void dragMainWindowBetweenScreens()
    {
        IconProvider icons;
        const QStringList small = nonViewer(30), large = nonViewer(5, 30);
        const auto w = ribbon(small, large);
        w->show();
        QVERIFY(QTest::qWaitForWindowExposed(w.get()));
        QVERIFY(moveTo(*w, "p100"));
        DisplayContext display(w.get());
        QSignalSpy dprSpy(&display, &DisplayContext::devicePixelRatioChanged);
        QSignalSpy screenSpy(&display, &DisplayContext::screenChanged);

        // Vùng vẽ (RND, chưa có): nghe tín hiệu, lọc theo cửa sổ chứa nó.
        QWidget* viewport = w.get();
        qreal viewportDpr = 0;
        ScreenClass classAtSignal{};
        QObject receiver;
        connect(&display, &DisplayContext::devicePixelRatioChanged, &receiver, [&](QWidget* window, qreal dpr) {
            if (window == viewport->window()) {
                viewportDpr = dpr;
                classAtSignal = display.screenClass();
            }
        });

        QList<qreal> seen{1.0};
        qreal last = 1.0;
        for (const char* name : {"p125", "p150", "p175", "p200", "p100", "p150"}) {
            const Screen& s = spec(name);
            settle();
            dprSpy.clear();
            screenSpy.clear();
            const quint64 before = icons.stats().rasters;
            const auto start = probe::Clock::now();
            w->move(inside(name));
            QVERIFY2(waitFrame(*w, start), name);
            QCOMPARE(w->screen(), screenNamed(name));
            QCOMPARE(w->devicePixelRatio(), s.dpr);
            QCOMPARE(display.screenClass(), DisplayContext::classify(s.logicalWidth));

            QCOMPARE(dprSpy.count(), s.dpr != last ? 1 : 0);
            if (s.dpr != last) {
                QCOMPARE(dprSpy[0][0].value<QWidget*>(), w.get());
                QCOMPARE(dprSpy[0][1].toReal(), s.dpr);
                QCOMPARE(viewportDpr, s.dpr);
                QCOMPARE(classAtSignal, DisplayContext::classify(s.logicalWidth));
            }
            QCOMPARE(screenSpy.count(), 1);
            QCOMPARE(screenSpy[0][0].value<QWidget*>(), w.get());
            QCOMPARE(screenSpy[0][1].value<QScreen*>(), screenNamed(name));

            const quint64 afterFrame = icons.stats().rasters;
            for (const auto& [names, logical] : {std::pair{small, 16}, std::pair{large, 32}})
                for (const QString& n : names) {
                    const QPixmap pm = icons.pixmap(n, logical, s.dpr, IconState::Normal);
                    QCOMPARE(pm.size(), QSize(qRound(logical * s.dpr), qRound(logical * s.dpr)));
                    QCOMPARE(pm.devicePixelRatio(), s.dpr);
                }
            QCOMPARE(icons.stats().rasters, afterFrame);
            QCOMPARE(afterFrame - before, seen.contains(s.dpr) ? 0 : quint64(small.size() + large.size()));
            seen << s.dpr;
            last = s.dpr;
        }
    }

    // Tiêu chí 1, palette nổi: cửa sổ cấp cao thứ hai theo màn hình của nó; cửa sổ chính và ScreenClass không đổi.
    // track() trước show (palette mở ngay trên màn hình 150%), gọi lại thì bỏ qua; palette hủy thì bỏ theo dõi.
    void floatingPaletteFollowsItsScreen()
    {
        IconProvider icons;
        QWidget main;
        main.resize(800, 500);
        main.show();
        QVERIFY(QTest::qWaitForWindowExposed(&main));
        QVERIFY(moveTo(main, "p100"));
        DisplayContext display(&main);
        const QStringList names = nonViewer(8, 60);
        auto palette = ribbon(names, {}, Qt::Tool);
        palette->move(inside("p150"));
        display.track(palette.get());
        display.track(palette.get());
        palette->show();
        QVERIFY(QTest::qWaitForWindowExposed(palette.get()));
        QCOMPARE(palette->devicePixelRatio(), 1.5);
        QSignalSpy dprSpy(&display, &DisplayContext::devicePixelRatioChanged);
        QSignalSpy screenSpy(&display, &DisplayContext::screenChanged);
        QSignalSpy classSpy(&display, &DisplayContext::screenClassChanged);

        // Sang 100%: DisplayContext đã ghi palette ở 150% (không theo màn hình chính lúc track): đúng một tín hiệu.
        QVERIFY(moveTo(*palette, "p100"));
        QTRY_COMPARE(dprSpy.count(), 1);
        QCOMPARE(dprSpy[0][0].value<QWidget*>(), palette.get());
        QCOMPARE(dprSpy[0][1].toReal(), 1.0);
        dprSpy.clear();
        screenSpy.clear();

        settle();
        const auto start = probe::Clock::now();
        palette->move(inside("p200"));
        QVERIFY(waitFrame(*palette, start));
        QCOMPARE(palette->devicePixelRatio(), 2.0);
        QCOMPARE(dprSpy.count(), 1);
        QCOMPARE(dprSpy[0][0].value<QWidget*>(), palette.get());
        QCOMPARE(dprSpy[0][1].toReal(), 2.0);
        QCOMPARE(screenSpy.count(), 1);
        QCOMPARE(screenSpy[0][1].value<QScreen*>(), screenNamed("p200"));
        QCOMPARE(classSpy.count(), 0);
        QCOMPARE(main.devicePixelRatio(), 1.0);
        QCOMPARE(display.screenClass(), ScreenClass::Wide);
        const quint64 afterFrame = icons.stats().rasters;
        for (const QString& n : names)
            QCOMPARE(icons.pixmap(n, 16, 2.0, IconState::Normal).size(), QSize(32, 32));
        QCOMPARE(icons.stats().rasters, afterFrame);

        palette.reset();
        dprSpy.clear();
        QVERIFY(moveTo(main, "p150"));
        QTRY_COMPARE(dprSpy.count(), 1);
        QCOMPARE(dprSpy[0][0].value<QWidget*>(), &main);

        // Chỉ theo dõi cửa sổ cấp cao (TRION_CHECK).
        QWidget child(&main);
        g_asserts = 0;
        const AssertHandler old = setAssertHandler(&countAssert);
        display.track(&child);
        setAssertHandler(old);
        QCOMPARE(g_asserts, 1);
    }

    // Tiêu chí 1: từ lúc cửa sổ 120 nút biểu tượng ngoài bộ Viewer sang màn hình tỉ lệ khác đến hết khung đầu (bố cục,
    // chữ, 120 biểu tượng raster lại trên luồng UI) ≤ 100 ms. Mỗi vòng một IconProvider mới (cache trống) như lần kéo
    // đầu tiên. Máy tải nặng: mỗi lần đổi lấy vòng tốt nhất, kiểm ở bản release; các bản khác chỉ báo số.
    void dpiSwitchWithinBudget()
    {
        const QStringList small = nonViewer(100), large = nonViewer(20, 100);
        const char* const order[] = {"p150", "p200", "p125", "p175", "p100"};
        std::array<double, std::size(order)> best;
        best.fill(std::numeric_limits<double>::infinity());
        for (int round = 0; round < 5; ++round) {
            IconProvider icons;
            const auto w = ribbon(small, large);
            w->show();
            QVERIFY(QTest::qWaitForWindowExposed(w.get()));
            QVERIFY(moveTo(*w, "p100"));
            DisplayContext display(w.get());
            QString line;
            for (std::size_t i = 0; i < std::size(order); ++i) {
                settle(); // bộ Viewer ở DPR mọi màn hình đã raster trước xong: không tranh CPU với khung đo
                probe::reset();
                const auto start = probe::Clock::now();
                w->move(inside(order[i]));
                QVERIFY2(waitFrame(*w, start), order[i]);
                QCOMPARE(w->devicePixelRatio(), spec(order[i]).dpr);
                const QJsonObject o = uiState();
                QCOMPARE(o["budget_ms"_L1].toDouble(), 100.0); // ngưỡng DoD-L "đổi trạng thái giao diện"
                best[i] = std::min(best[i], o["max_ms"_L1].toDouble());
                line += u" %1 %2"_s.arg(QLatin1StringView(order[i])).arg(o["max_ms"_L1].toDouble(), 0, 'f', 1);
            }
            qInfo("vòng %d, ms:%s", round, qPrintable(line));
        }
        const double worst = *std::max_element(best.cbegin(), best.cend());
        qInfo("tốt nhất mỗi lần đổi, lớn nhất: %.1f ms (ngưỡng 100 ms)", worst);
        QVERIFY2(!test::kTimed || worst <= 100.0, qPrintable(u"đổi DPI %1 ms > 100 ms"_s.arg(worst)));
    }

    // Tiêu chí 2: bảng mẫu ở 100/125/150/175/200% khớp ảnh trong tests/baseline/<nền tảng>/ (nhúng vào test).
    void sampleSheetMatchesBaseline()
    {
        IconProvider icons;
        const auto sheet = sampleSheet();
        sheet->show();
        QVERIFY(QTest::qWaitForWindowExposed(sheet.get()));
        const bool update = qEnvironmentVariableIsSet("TRION_UPDATE_BASELINE");
        if (update)
            QVERIFY(QDir().mkpath(QStringLiteral(TRION_BASELINE_OUT)));
        for (const char* name : {"p100", "p125", "p150", "p175", "p200"}) {
            const Screen& s = spec(name);
            QVERIFY(moveTo(*sheet, name));
            const QImage got = sheet->grab().toImage().convertToFormat(QImage::Format_ARGB32);
            QCOMPARE(got.size(), kSheet * s.dpr);
            const QString file = u"dpr%1.png"_s.arg(qRound(s.dpr * 100));
            if (update) {
                QVERIFY(got.save(QStringLiteral(TRION_BASELINE_OUT "/") + file));
                continue;
            }
            const QImage want = QImage(u":/baseline/"_s + file).convertToFormat(QImage::Format_ARGB32);
            QVERIFY2(!want.isNull(), qPrintable(file + u": chưa có baseline, chạy test với TRION_UPDATE_BASELINE=1"_s));
            QCOMPARE(got.size(), want.size());
            int bad = 0, worst = 0;
            for (int y = 0; y < got.height(); ++y)
                for (int x = 0; x < got.width(); ++x) {
                    const QRgb a = got.pixel(x, y), b = want.pixel(x, y);
                    const int d = std::max(
                        {std::abs(qRed(a) - qRed(b)), std::abs(qGreen(a) - qGreen(b)), std::abs(qBlue(a) - qBlue(b))});
                    worst = std::max(worst, d);
                    bad += d > 0;
                }
            if (bad)
                got.save(file); // để so: thư mục chạy test
            QVERIFY2(bad == 0, qPrintable(u"%1: %2 pixel lệch, lớn nhất %3"_s.arg(file).arg(bad).arg(worst)));
        }
    }

    // Tiêu chí 3: ngưỡng của ScreenClass; khung đọc screenClass() theo bề rộng logic của màn hình chứa cửa sổ chính
    // (1600 px ở 175% và 200% là Wide, 1920×1080 ở 150% là Small), tín hiệu chỉ khi lớp đổi.
    void screenClassThresholds()
    {
        const std::pair<int, ScreenClass> table[] = {
            {1024, ScreenClass::Small},  {1280, ScreenClass::Small},  {1366, ScreenClass::Small},
            {1367, ScreenClass::Narrow}, {1599, ScreenClass::Narrow}, {1600, ScreenClass::Wide},
            {1920, ScreenClass::Wide},   {3840, ScreenClass::Wide},
        };
        for (const auto& [width, want] : table)
            QCOMPARE(DisplayContext::classify(width), want);

        IconProvider icons;
        QWidget main;
        main.resize(640, 400);
        main.show();
        QVERIFY(QTest::qWaitForWindowExposed(&main));
        QVERIFY(moveTo(main, "p100"));
        DisplayContext display(&main);
        QCOMPARE(&DisplayContext::instance(), &display);
        QCOMPARE(display.screenClass(), ScreenClass::Wide);
        QSignalSpy spy(&display, &DisplayContext::screenClassChanged);
        const std::pair<const char*, ScreenClass> steps[] = {
            {"w1366", ScreenClass::Small}, {"w1599", ScreenClass::Narrow}, {"p175", ScreenClass::Wide},
            {"p100", ScreenClass::Wide},   {"p150", ScreenClass::Small},   {"p125", ScreenClass::Narrow},
            {"p200", ScreenClass::Wide},
        };
        ScreenClass last = ScreenClass::Wide;
        for (const auto& [name, want] : steps) {
            spy.clear();
            QVERIFY2(moveTo(main, name), name);
            QCOMPARE(display.screenClass(), want);
            QCOMPARE(spy.count(), want != last ? 1 : 0);
            if (want != last)
                QCOMPARE(spy[0][0].value<ScreenClass>(), want);
            last = want;
        }
    }

    // Tiêu chí 4: chiều cao theo token gọn ↔ thoáng (dòng 22 ↔ 26, điều khiển 24 ↔ 26 px logic) như nhau ở 100% và
    // 150%; Giao diện nhẹ toàn ứng dụng: 0 animation, bóng thành viền 1 px đục, popup không bóng, không trong suốt.
    void densityAndLite()
    {
        QWidget w;
        auto* lay = new QVBoxLayout(&w);
        auto* button = new QPushButton(u"Áp dụng"_s);  // i18n-ok: chữ mẫu của test
        auto* edit = new QLineEdit(u"0,25"_s);
        auto* list = new QListView;
        list->setModel(new QStringListModel({u"Lớp 0"_s, u"Đường tâm"_s}, list));
        lay->addWidget(button);
        lay->addWidget(edit);
        lay->addWidget(list);
        w.resize(300, 240);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        for (const char* name : {"p100", "p150"}) {
            QVERIFY(moveTo(w, name));
            for (const bool comfy : {false, true}) {
                tm_->setDensity(comfy ? Density::Comfortable : Density::Compact);
                const int row = comfy ? 26 : 22, ctl = comfy ? 26 : 24;
                QCOMPARE(tm_->px(tok::h_row), double(row));
                QCOMPARE(tm_->px(tok::h_ctl), double(ctl));
                QTRY_COMPARE(list->visualRect(list->model()->index(0, 0)).height(), row);
                QTRY_COMPARE(button->height(), ctl);
                QTRY_COMPARE(edit->height(), ctl);
                QCOMPARE(w.grab().height(), qRound(w.height() * spec(name).dpr));
            }
        }

        tm_->setLite(true);
        QCOMPARE(tm_->ms(tok::t), 0);
        QCOMPARE(tm_->ms(tok::t_fast), 0);
        QCOMPARE(QApplication::style()->styleHint(QStyle::SH_Widget_Animation_Duration), 0);
        for (const Qt::UIEffect e : {Qt::UI_AnimateMenu, Qt::UI_FadeMenu, Qt::UI_AnimateCombo, Qt::UI_AnimateTooltip,
                                     Qt::UI_FadeTooltip, Qt::UI_AnimateToolBox})
            QVERIFY(!QApplication::isEffectEnabled(e));
        for (const tok::ShadowId id : {tok::sh_pop, tok::sh_dlg}) {
            const tok::Shadow s = tm_->shadow(id);
            QCOMPARE(QList<double>({s.x, s.y, s.blur, s.spread}), QList<double>({0, 0, 0, 1}));
            QCOMPARE(s.color.a, 255); // viền đục
        }
        QMenu menu;
        menu.addAction(u"Lớp"_s);  // i18n-ok: chữ mẫu của test
        menu.ensurePolished();
        QVERIFY(menu.windowFlags().testFlag(Qt::NoDropShadowWindowHint));
        QVERIFY(!menu.testAttribute(Qt::WA_TranslucentBackground));
        QCOMPARE(menu.windowOpacity(), 1.0);
        tm_->setLite(false);
        QCOMPARE(tm_->ms(tok::t), 120);
        QVERIFY(tm_->shadow(tok::sh_pop).blur > 0);
        QVERIFY(QApplication::isEffectEnabled(Qt::UI_General));
    }

    // Tiêu chí 4: tôn trọng giảm chuyển động của hệ điều hành (ThemeManager: mọi --t* 0 ms, hiệu ứng menu tắt; bóng
    // giữ nguyên vì không phải Giao diện nhẹ).
    void followsSystemReducedMotion()
    {
        IconProvider icons;
        QWidget w;
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
#if defined(Q_OS_WIN)
        BOOL animate = TRUE;
        QVERIFY(SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animate, 0));
        const bool reduced = !animate;
        QCOMPARE(DisplayContext::systemReducedMotion(), reduced);
        DisplayContext display(&w);
        QCOMPARE(tm_->reducedMotion(), reduced);
        // Người dùng đổi thiết lập khi đang chạy: WM_SETTINGCHANGE đưa thẳng vào bộ lọc của dispatcher (không đổi thiết
        // lập thật của máy). ThemeManager lệch trước thì phải về đúng giá trị của hệ thống.
        tm_->setReducedMotion(!reduced);
        MSG msg{};
        msg.message = WM_SETTINGCHANGE;
        msg.wParam = SPI_SETWORKAREA; // thiết lập khác: bỏ qua
        qintptr result = 0;
        QAbstractEventDispatcher::instance()->filterNativeEvent("windows_generic_MSG", &msg, &result);
        QCOMPARE(tm_->reducedMotion(), !reduced);
        msg.wParam = SPI_SETCLIENTAREAANIMATION;
        QAbstractEventDispatcher::instance()->filterNativeEvent("windows_generic_MSG", &msg, &result);
        QCOMPARE(tm_->reducedMotion(), reduced);
#elif defined(Q_OS_MACOS)
        QSKIP("ponytail: macOS chưa đọc giảm chuyển động (DisplayContext.cpp)");
#else
        const QString config = qEnvironmentVariable("XDG_CONFIG_HOME");
        const QString kde = config + u"/kdeglobals"_s, gtk = config + u"/gtk-3.0/settings.ini"_s;
        QVERIFY(!DisplayContext::systemReducedMotion());
        QVERIFY(writeFile(kde, "[KDE]\nAnimationDurationFactor=0.5\n"));
        QVERIFY(!DisplayContext::systemReducedMotion());
        QVERIFY(writeFile(kde, "[KDE]\nAnimationDurationFactor=0\n"));
        QVERIFY(DisplayContext::systemReducedMotion());
        QVERIFY(QFile::remove(kde));
        QVERIFY(QDir(config).mkpath(u"gtk-3.0"_s));
        QVERIFY(writeFile(gtk, "[Settings]\ngtk-enable-animations=true\n"));
        QVERIFY(!DisplayContext::systemReducedMotion());
        QVERIFY(writeFile(gtk, "[Settings]\ngtk-enable-animations=false\n"));
        QVERIFY(DisplayContext::systemReducedMotion());

        DisplayContext display(&w);
        QVERIFY(tm_->reducedMotion());
        QCOMPARE(tm_->ms(tok::t), 0);
        QCOMPARE(tm_->ms(tok::t_fast), 0);
        QVERIFY(!QApplication::isEffectEnabled(Qt::UI_AnimateMenu));
        QCOMPARE(tm_->shadow(tok::sh_pop).blur, 14.0);
        QVERIFY(QFile::remove(gtk));
        QVERIFY(!DisplayContext::systemReducedMotion());
#endif
    }
};

QTEST_MAIN(DisplayContextTest)
#include "display_context_test.moc"
