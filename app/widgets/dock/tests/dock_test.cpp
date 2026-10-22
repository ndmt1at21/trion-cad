// UI0-113: palette dock (PaletteHost.h, PaletteHeader.h). Cửa sổ mẫu: QMainWindow với widget trung tâm giả làm vùng
// vẽ (UIA-001 chưa có). Mỗi tiêu chí nghiệm thu một nhóm hàm test:
//   1  areas: moveTo và menu của đầu palette đưa palette sang trái/phải/dưới (gộp tab với palette ở đó) và nổi;
//      drag: kéo đầu palette ra giữa cửa sổ → nổi; đến mép dưới → dock dưới; thả vào giữa palette khác → gộp tab; kéo
//      tab ra → tách thành palette nổi; đến mép phải → dock phải. Mỗi bước kéo (giao di chuột, bố cục lại, vẽ phần bẩn)
//      đo thời gian; ở vòng tốt nhất trong 5 vòng p99 ≤ 16,6 ms và không bước nào > 33,3 ms (khựng > 1 khung), chỉ
//      kiểm ở release (tests/timing.h). 60 fps trên màn hình thật (hợp thành, vsync): hoãn → QA-011.
//   2  restoreSame: dựng lại cửa sổ và PaletteHost từ cùng Store → chỗ, nhóm tab, tab đang mở, nổi, cỡ, ẩn/hiện, ghim
//      y hệt; lateRegistration: palette đăng ký sau restoreLayout về chỗ đã lưu; fitToScreens (hàm thuần): màn hình
//      đã rút → màn hình chính, trọn trong vùng làm việc; restoreOffscreen: palette nổi ngoài mọi màn hình về trọn
//      trong vùng làm việc của màn hình chính.
//   3  lazy: không palette nào dựng trước lần hiện đầu, kể cả tab nền của nhóm; firstFrame: UiProbe palette_open
//      ≤ 150 ms (chỉ kiểm ở release).
//   4  focusKeys: F6 / Shift+F6 vòng qua palette đang hiện và vùng vẽ; Esc về vùng vẽ (cả từ palette nổi); Esc của ô
//      tìm còn chữ là của ô; focusKeysFromKeymap: phím theo hành động "pane" của Keymap; accessibleNames: tên
//      trình đọc màn hình của palette (tiêu đề) và nút đầu palette, đổi theo trạng thái ghim và ngôn ngữ.
//      groupAccessibility: a11y::audit của cửa sổ mẫu 0 vi phạm (dải tab của
//      nhóm có viền --c-focus khi Tab tới); palette ở tab nền và dải tab bị Qt cất không nhận Tab (đổi tab, đóng,
//      mở, nhóm nổi).
//   5  header: ảnh baseline đầu palette Tối/Sáng × 100/150/200% theo nền tảng, màu token ở chỗ không có chữ, cỡ theo
//      token. Ghi lại baseline: TRION_UPDATE_BASELINE=<thư mục> chạy test, chép header-*-<nền tảng>.png vào
//      tests/baseline. pseudoNoTruncation: vi-XL, findTruncations = 0; idleNoTimers: rảnh không timer nào chạy.
//   errors: mã trùng, rỗng, thiếu hàm (TRION_CHECK); mã lạ; bố cục đã lưu hỏng.
#include "app/widgets/dock/PaletteHeader.h"
#include "app/widgets/dock/PaletteHost.h"

#include "app/core/a11y/AccessibilityAudit.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/keymap/Keymap.h"
#include "app/core/perf/UiProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "core/base/assert.h"
#include "core/base/sched/Scheduler.h"
#include "tests/timing.h"

#include <QAbstractEventDispatcher>
#include <QAccessible>
#include <QApplication>
#include <QBoxLayout>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QPainter>
#include <QScreen>
#include <QSignalSpy>
#include <QTabBar>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;
using namespace trion::theme;
using namespace trion::ui;
namespace i18n = trion::i18n;
namespace keymap = trion::keymap;
namespace sched = trion::sched;
namespace settings = trion::settings;

namespace {

using trion::test::kTimed;

#if defined(Q_OS_WIN)
constexpr auto kPlatform = "windows";
#elif defined(Q_OS_MACOS)
constexpr auto kPlatform = "macos";
#else
constexpr auto kPlatform = "linux";
#endif
constexpr int kNoise = 8;                // như controls_test: lệch ≤ 8/255 mỗi kênh là nhiễu dựng chữ
constexpr double kFrameMs = 1000.0 / 60; // DoD-L: kéo palette 60 fps
constexpr double kOpenBudgetMs = 150;    // DoD-L: mở palette đến khung đầu có nội dung, máy tầm trung
constexpr int kSteps = 24;               // số bước di chuột mỗi lần kéo
const QRect kWindow(40, 30, 720, 520);   // cửa sổ mẫu nằm trong màn hình offscreen 800×600

struct Pal {
    const char* id;
    const char* title;
    Qt::DockWidgetArea area;
    bool visible;
};
// Mặc định kiểu "Soạn thảo 2D": Thuộc tính mở ở phải, Layer gộp tab với nó (ẩn), thư viện ở trái, Đếm ở dưới.
const Pal kPalettes[] = {
    {"props", QT_TRANSLATE_NOOP("DockTest", "Thuộc tính"), Qt::RightDockWidgetArea, true},
    {"layers", QT_TRANSLATE_NOOP("DockTest", "Layer"), Qt::RightDockWidgetArea, false},
    {"tcvn", QT_TRANSLATE_NOOP("DockTest", "Thư viện TCVN"), Qt::LeftDockWidgetArea, false},
    {"count", QT_TRANSLATE_NOOP("DockTest", "Đếm"), Qt::BottomDockWidgetArea, false},
};

bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 5s)
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

// Sự kiện chuột giao thẳng cho target (như sau khi Qt đã chọn widget nhận và giữ chuột), nút trái.
void mouse(QWidget* target, QEvent::Type type, QPoint global)
{
    const bool edge = type != QEvent::MouseMove;
    QMouseEvent e(type, QPointF(target->mapFromGlobal(global)), QPointF(global), edge ? Qt::LeftButton : Qt::NoButton,
                  type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &e);
}

bool near(const QColor& a, const QColor& b)
{
    return std::max({std::abs(a.red() - b.red()), std::abs(a.green() - b.green()), std::abs(a.blue() - b.blue())}) <=
           kNoise;
}

bool onScreen(const QWidget* w)
{
    return w->isVisible() && !w->visibleRegion().isEmpty();
}

double percentile(QList<double> v, double q)
{
    std::sort(v.begin(), v.end());
    return v.isEmpty() ? 0 : v[qsizetype(std::ceil(q * double(v.size()))) - 1];
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

} // namespace

class DockTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;  // chủ đề, ngôn ngữ
    std::unique_ptr<settings::Store> layout_; // bố cục palette: mỗi test một kho mới
    std::unique_ptr<UiExecutor> executor_;
    std::unique_ptr<ThemeManager> tm_;
    std::unique_ptr<IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<QMainWindow> win_;
    PaletteHost* host_ = nullptr;
    QWidget* canvas_ = nullptr;
    QHash<QString, int> built_; // số lần hàm dựng nội dung chạy, theo mã
    int stores_ = 0;

    QWidget* makeContent(const QString& id)
    {
        ++built_[id];
        auto* w = new QWidget;
        auto* col = new QVBoxLayout(w);
        col->setContentsMargins({}); // như .pal-body: sát mép, palette rộng đúng --pal-w
        col->setSpacing(0);
        auto* search = new SearchField;
        search->setObjectName(u"search"_s);
        search->setAccessibleName(id); // tên cho trình đọc màn hình (a11y::audit ở groupAccessibility)
        auto* list = new QListWidget;
        list->setAccessibleName(id);
        for (int i = 0; i < 100; ++i)
            list->addItem(u"%1 %2"_s.arg(id).arg(i));
        col->addWidget(search);
        col->addWidget(list);
        return w;
    }

    PaletteSpec spec(const Pal& p)
    {
        const QString id = QString::fromLatin1(p.id);
        return {id, [t = p.title] { return QCoreApplication::translate("DockTest", t); },
                [this, id] { return makeContent(id); }, p.area, p.visible};
    }

    // Cửa sổ mẫu với các palette (mặc định kPalettes), bố cục đã lưu trong layout_ nếu có; hiện và active.
    bool build(QList<Pal> pals = {})
    {
        if (pals.isEmpty())
            pals = QList<Pal>(std::begin(kPalettes), std::end(kPalettes));
        win_ = std::make_unique<QMainWindow>();
        canvas_ = new QWidget;
        canvas_->setFocusPolicy(Qt::StrongFocus);
        win_->setCentralWidget(canvas_);
        win_->setGeometry(kWindow);
        host_ = new PaletteHost(win_.get(), *layout_);
        for (const Pal& p : std::as_const(pals))
            host_->registerPalette(spec(p));
        host_->restoreLayout();
        win_->show();
        return QTest::qWaitForWindowActive(win_.get());
    }

    QDockWidget* dock(const char* id) const { return host_->dock(QString::fromLatin1(id)); }
    Qt::DockWidgetArea area(const char* id) const
    {
        QDockWidget* d = dock(id);
        return d->isFloating() ? Qt::NoDockWidgetArea : win_->dockWidgetArea(d);
    }
    bool focusIn(const char* id) const
    {
        const QWidget* f = QApplication::focusWidget();
        return f && dock(id)->isAncestorOf(f);
    }

    // Dải tab của nhóm đang hiện (QMainWindow tạo, con trực tiếp của cửa sổ).
    QTabBar* groupBar() const
    {
        for (QTabBar* b : win_->findChildren<QTabBar*>(Qt::FindDirectChildrenOnly))
            if (b->isVisible() && b->count() > 1)
                return b;
        return nullptr;
    }
    static int tabIndex(const QTabBar* bar, const QString& text)
    {
        for (int i = 0; i < bar->count(); ++i)
            if (bar->tabText(i) == text)
                return i;
        return -1;
    }

    // Một bước kéo: giao di chuột rồi làm hết sự kiện chờ nó sinh ra — bố cục lại (chèn chỗ thả, tách/gắn palette) và
    // vẽ phần bẩn (Qt gửi UpdateRequest qua hàng đợi): ms luồng UI làm ra một khung.
    double step(QWidget* target, QPoint global)
    {
        QElapsedTimer t;
        t.start();
        mouse(target, QEvent::MouseMove, global);
        QCoreApplication::sendPostedEvents();
        return double(t.nsecsElapsed()) / 1e6;
    }

    // Kéo đầu palette (nắm ở tiêu đề) đến điểm toàn cục `to` rồi nhả. Sau khi bắt đầu kéo, QDockWidget giữ chuột.
    void dragHeader(const char* id, QPoint to, QList<double>& ms)
    {
        QDockWidget* d = dock(id);
        QLabel* title = host_->header(d->objectName())->titleLabel();
        const QPoint from = title->mapToGlobal(title->rect().center());
        mouse(title, QEvent::MouseButtonPress, from);
        for (int i = 1; i <= kSteps; ++i)
            ms << step(d, from + (to - from) * i / kSteps);
        mouse(d, QEvent::MouseButtonRelease, to);
        QCoreApplication::sendPostedEvents();
    }

    // Kéo tab ra khỏi dải tab của nhóm đến `to` rồi nhả.
    void dragTab(QTabBar* bar, int index, QPoint to, QList<double>& ms)
    {
        const QPoint from = bar->mapToGlobal(bar->tabRect(index).center());
        mouse(bar, QEvent::MouseButtonPress, from);
        ms << step(bar, from + QPoint(QApplication::startDragDistance() + 2, 0)); // kéo tab trong dải trước
        for (int i = 1; i <= kSteps; ++i)
            ms << step(bar, from + (to - from) * i / kSteps);
        mouse(bar, QEvent::MouseButtonRelease, to);
        QCoreApplication::sendPostedEvents();
    }

    struct Snap {
        bool shown, floating, pinned;
        Qt::DockWidgetArea area;
        QRect geometry; // toàn cục; chỉ so khi đang hiện
        QStringList tabs;
        QString toString() const
        {
            return u"hiện=%1 nổi=%2 ghim=%3 chỗ=%4 hình=%5,%6 %7×%8 nhóm=[%9]"_s.arg(shown)
                .arg(floating)
                .arg(pinned)
                .arg(int(area))
                .arg(geometry.x())
                .arg(geometry.y())
                .arg(geometry.width())
                .arg(geometry.height())
                .arg(tabs.join(u',')); // i18n-ok: nhật ký test
        }
        bool same(const Snap& o) const
        {
            return shown == o.shown && floating == o.floating && pinned == o.pinned && area == o.area &&
                   tabs == o.tabs && (!shown || geometry == o.geometry);
        }
    };
    QHash<QString, Snap> snapshot() const
    {
        QHash<QString, Snap> s;
        for (const QString& id : host_->ids()) {
            QDockWidget* d = host_->dock(id);
            QStringList tabs;
            for (const QDockWidget* t : win_->tabifiedDockWidgets(d))
                tabs << t->objectName();
            tabs.sort();
            s.insert(id, {onScreen(d), d->isFloating(), host_->isPinned(id), win_->dockWidgetArea(d),
                          QRect(d->mapToGlobal(QPoint()), d->size()), tabs});
        }
        return s;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
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
        layout_ = std::make_unique<settings::Store>(dir_.filePath(u"layout-%1.json"_s.arg(++stores_)));
        built_.clear();
    }

    void cleanup()
    {
        if (QWidget* p = QApplication::activePopupWidget())
            p->close();
        win_.reset(); // PaletteHost là con của cửa sổ
        host_ = nullptr;
        canvas_ = nullptr;
        layout_.reset();
        tm_->setReducedMotion(false);
        QCoreApplication::processEvents();
    }

    void cleanupTestCase()
    {
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
    void areas()
    {
        QVERIFY(build());
        host_->showPalette(u"tcvn"_s);
        host_->moveTo(u"layers"_s, Qt::LeftDockWidgetArea); // gộp tab với tcvn đang hiện ở trái
        QCOMPARE(area("layers"), Qt::LeftDockWidgetArea);
        QVERIFY(win_->tabifiedDockWidgets(dock("tcvn")).contains(dock("layers")));
        QVERIFY(onScreen(dock("layers")));
        QVERIFY(host_->content(u"layers"_s));

        host_->moveTo(u"props"_s, Qt::BottomDockWidgetArea);
        QCOMPARE(area("props"), Qt::BottomDockWidgetArea);
        host_->moveTo(u"props"_s, Qt::NoDockWidgetArea);
        QVERIFY(dock("props")->isFloating());
        QVERIFY(onScreen(dock("props")));
        host_->moveTo(u"props"_s, Qt::RightDockWidgetArea);
        QCOMPARE(area("props"), Qt::RightDockWidgetArea);
        QVERIFY(onScreen(dock("props")));

        // Menu của đầu palette như prototype: Ghim (đánh dấu), chuyển sang chỗ khác (không có chỗ hiện tại), Nổi,
        // Mật độ thoáng. "Chuyển sang dock phải" → gộp tab với Thuộc tính.
        host_->showPalette(u"count"_s);
        QTest::mouseClick(host_->header(u"count"_s)->menuButton(), Qt::LeftButton);
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        QVERIFY(menu);
        QStringList texts;
        for (const QAction* a : menu->actions())
            texts << (a->isChecked() ? u"✓"_s : QString()) + a->text(); // i18n-ok: so trong test
        QCOMPARE(texts, QStringList({u"✓Ghim"_s, u"Chuyển sang dock trái"_s, u"Chuyển sang dock phải"_s, u"Nổi"_s,
                                     QString(), u"Mật độ thoáng"_s}));
        menu->actions()[0]->trigger(); // bỏ ghim
        QVERIFY(!host_->isPinned(u"count"_s));
        menu->actions()[5]->trigger();
        QCOMPARE(tm_->density(), Density::Comfortable);
        menu->actions()[2]->trigger();
        menu->close();
        QCOMPARE(area("count"), Qt::RightDockWidgetArea);
        QVERIFY(win_->tabifiedDockWidgets(dock("props")).contains(dock("count")));
        QVERIFY(onScreen(dock("count")));
        QCOMPARE(built_.value(u"props"_s), 1); // đổi chỗ không dựng lại nội dung
    }

    void drag()
    {
        tm_->setReducedMotion(true); // Qt thả palette không animation: chỗ mới có ngay khi nhả chuột
        QVERIFY(build());
        host_->showPalette(u"tcvn"_s);
        QDockWidget* props = dock("props");
        QDockWidget* tcvn = dock("tcvn");
        const auto center = [&] { return canvas_->mapToGlobal(canvas_->rect().center()); };
        double bestP99 = 1e9, bestP50 = 0, bestMax = 0;
        qsizetype count = 0;
        for (int round = 0; round < 5; ++round) {
            QList<double> ms;
            dragHeader("props", center(), ms); // ra giữa vùng vẽ: không phải chỗ dock → nổi
            QVERIFY(props->isFloating());
            QVERIFY(onScreen(props));
            dragHeader("props", win_->mapToGlobal(QPoint(win_->width() / 2, win_->height() - 20)), ms);
            QCOMPARE(area("props"), Qt::BottomDockWidgetArea);
            dragHeader("props", tcvn->mapToGlobal(tcvn->rect().center()), ms); // giữa palette khác → gộp tab
            QCOMPARE(area("props"), Qt::LeftDockWidgetArea);
            QVERIFY(win_->tabifiedDockWidgets(tcvn).contains(props));
            QTabBar* bar = groupBar();
            QVERIFY(bar);
            const int tab = tabIndex(bar, props->windowTitle());
            QVERIFY(tab >= 0);
            dragTab(bar, tab, center(), ms); // kéo tab ra → tách thành palette nổi
            QVERIFY(props->isFloating());
            QVERIFY(!win_->tabifiedDockWidgets(tcvn).contains(props));
            QVERIFY(onScreen(tcvn));
            dragHeader("props", win_->mapToGlobal(QPoint(win_->width() - 20, win_->height() / 3)), ms);
            QCOMPARE(area("props"), Qt::RightDockWidgetArea);
            QVERIFY(onScreen(props));

            const double p99 = percentile(ms, 0.99);
            if (p99 < bestP99) {
                bestP99 = p99;
                bestP50 = percentile(ms, 0.5);
                bestMax = percentile(ms, 1);
                count = ms.size();
            }
        }
        QCOMPARE(built_.value(u"props"_s), 1); // nội dung giữ nguyên qua mọi lần kéo
        qInfo("kéo palette: %lld bước/vòng, vòng tốt nhất p50 %.2f ms, p99 %.2f ms, lớn nhất %.2f ms", qint64(count),
              bestP50, bestP99, bestMax);
        QVERIFY2(!kTimed || (bestP99 <= kFrameMs && bestMax <= 2 * kFrameMs),
                 qPrintable(u"p99 %1 ms, lớn nhất %2 ms"_s.arg(bestP99).arg(bestMax))); // i18n-ok: thông báo test
    }

    // Tiêu chí 2.
    void restoreSame()
    {
        QVERIFY(build());
        host_->showPalette(u"tcvn"_s);
        host_->moveTo(u"layers"_s, Qt::LeftDockWidgetArea); // nhóm tab tcvn + layers
        host_->showPalette(u"count"_s);
        host_->moveTo(u"props"_s, Qt::NoDockWidgetArea);
        dock("props")->setGeometry(QRect(120, 140, 300, 260));
        dock("tcvn")->raise(); // tab đang mở: tcvn
        win_->resizeDocks({dock("tcvn")}, {320}, Qt::Horizontal);
        host_->setPinned(u"count"_s, false);
        QCoreApplication::sendPostedEvents();
        const QHash<QString, Snap> before = snapshot();
        QVERIFY(win_->close()); // QEvent::Close → saveLayout
        win_.reset();

        built_.clear();
        QVERIFY(build());
        QCoreApplication::sendPostedEvents();
        const QHash<QString, Snap> after = snapshot();
        for (const QString& id : host_->ids())
            QVERIFY2(before[id].same(after[id]),
                     qPrintable(id + u": "_s + before[id].toString() + u" → "_s + after[id].toString()));
        QCOMPARE(built_.value(u"layers"_s), 0); // tab nền sau khôi phục vẫn chưa dựng
        QCOMPARE(built_.value(u"tcvn"_s), 1);
    }

    void lateRegistration()
    {
        QVERIFY(build());
        host_->moveTo(u"count"_s, Qt::LeftDockWidgetArea);
        host_->saveLayout();
        win_.reset();
        QVERIFY(build({kPalettes[0], kPalettes[1], kPalettes[2]})); // chưa có "count"
        QVERIFY(host_->registerPalette(spec(kPalettes[3])));
        QCoreApplication::sendPostedEvents();
        QCOMPARE(area("count"), Qt::LeftDockWidgetArea);
        QVERIFY(onScreen(dock("count")));
    }

    void fitToScreens_data()
    {
        QTest::addColumn<QRect>("r");
        QTest::addColumn<QList<QRect>>("areas");
        QTest::addColumn<int>("primary");
        QTest::addColumn<QRect>("want");
        const QRect a(0, 0, 1920, 1040);    // màn hình chính, thanh tác vụ 40 px ở dưới
        const QRect b(1920, 0, 2560, 1400); // màn hình thứ hai bên phải
        QTest::newRow("inside") << QRect(2000, 100, 300, 400) << QList{a, b} << 0 << QRect(2000, 100, 300, 400);
        QTest::newRow("straddle") << QRect(1800, 100, 300, 400) << QList{a, b} << 0 << QRect(1920, 100, 300, 400);
        QTest::newRow("removed") << QRect(2500, 100, 300, 400) << QList{a} << 0 << QRect(1620, 100, 300, 400);
        QTest::newRow("removed-primary-1")
            << QRect(-2000, 500, 300, 400) << QList{b, a} << 1 << QRect(0, 500, 300, 400);
        QTest::newRow("under-taskbar") << QRect(100, 900, 300, 400) << QList{a} << 0 << QRect(100, 640, 300, 400);
        QTest::newRow("too-big") << QRect(-50, -50, 2500, 1200) << QList{a} << 0 << QRect(0, 0, 1920, 1040);
        QTest::newRow("no-screens") << QRect(5, 5, 10, 10) << QList<QRect>{} << 0 << QRect(5, 5, 10, 10);
    }

    void fitToScreens()
    {
        QFETCH(QRect, r);
        QFETCH(QList<QRect>, areas);
        QFETCH(int, primary);
        QFETCH(QRect, want);
        QCOMPARE(PaletteHost::fitToScreens(r, areas, primary), want);
    }

    void restoreOffscreen()
    {
        QVERIFY(build());
        host_->moveTo(u"props"_s, Qt::NoDockWidgetArea);
        dock("props")->setGeometry(QRect(5000, 100, 300, 260)); // trên màn hình thứ hai, nay đã rút
        host_->saveLayout();
        win_.reset();
        QVERIFY(build());
        QDockWidget* d = dock("props");
        QVERIFY(d->isFloating());
        QVERIFY(onScreen(d));
        const QRect work = QGuiApplication::primaryScreen()->availableGeometry();
        QVERIFY2(
            work.contains(d->frameGeometry()),
            qPrintable(
                u"%1,%2 %3×%4"_s.arg(d->x()).arg(d->y()).arg(d->width()).arg(d->height()))); // i18n-ok: thông báo test
        QCOMPARE(d->size(), QSize(300, 260));
    }

    // Tiêu chí 3.
    void lazy()
    {
        // Hai palette cùng mở ở phải thành nhóm tab, palette đăng ký sau ở trên: tab nền chưa dựng.
        QVERIFY(build({{"props", kPalettes[0].title, Qt::RightDockWidgetArea, true},
                       {"layers", kPalettes[1].title, Qt::RightDockWidgetArea, true},
                       {"tcvn", kPalettes[2].title, Qt::LeftDockWidgetArea, false}}));
        QCOMPARE(built_.value(u"layers"_s), 1);
        QCOMPARE(built_.value(u"props"_s), 0);
        QVERIFY(!host_->content(u"props"_s));
        QCOMPARE(built_.value(u"tcvn"_s), 0);

        QTabBar* bar = groupBar(); // người dùng chọn tab Thuộc tính → dựng đúng một lần
        QVERIFY(bar);
        bar->setCurrentIndex(tabIndex(bar, dock("props")->windowTitle()));
        QTRY_COMPARE(built_.value(u"props"_s), 1);
        QVERIFY(onScreen(host_->content(u"props"_s)));
        bar->setCurrentIndex(tabIndex(bar, dock("layers")->windowTitle()));
        bar->setCurrentIndex(tabIndex(bar, dock("props")->windowTitle()));
        QCOMPARE(built_.value(u"props"_s), 1);
        QCOMPARE(built_.value(u"layers"_s), 1);

        host_->showPalette(u"tcvn"_s); // palette ẩn: dựng khi hiện
        QCOMPARE(built_.value(u"tcvn"_s), 1);
        QVERIFY(onScreen(host_->content(u"tcvn"_s)));
    }

    void firstFrame()
    {
        QVERIFY(build());
        probe::enable();
        probe::reset();
        host_->showPalette(u"tcvn"_s);
        const auto op = [] { return probe::toJson()[u"ops"_s].toObject()[u"palette_open"_s].toObject(); };
        const bool got = spinUntil([&] { return op()[u"count"_s].toInt() >= 1; });
        const double ms = op()[u"max_ms"_s].toDouble();
        probe::disable();
        QVERIFY(got);
        qInfo("palette_open: %.2f ms đến khung đầu có nội dung (ngưỡng %.0f ms)", ms, kOpenBudgetMs);
        QVERIFY2(!kTimed || ms <= kOpenBudgetMs, qPrintable(QString::number(ms)));
    }

    // Tiêu chí 4.
    void focusKeys()
    {
        QVERIFY(build());
        host_->showPalette(u"tcvn"_s);
        canvas_->setFocus();
        QTRY_VERIFY(canvas_->hasFocus());
        const auto key = [](Qt::Key k, Qt::KeyboardModifiers m = Qt::NoModifier) {
            QTest::keyClick(QApplication::focusWidget(), k, m);
        };

        key(Qt::Key_F6); // thứ tự đăng ký: Thuộc tính rồi thư viện; vào ô đầu tiên của nội dung
        QTRY_VERIFY(focusIn("props"));
        QCOMPARE(QApplication::focusWidget()->objectName(), u"search"_s);
        key(Qt::Key_F6);
        QTRY_VERIFY(focusIn("tcvn"));
        key(Qt::Key_F6);
        QTRY_VERIFY(canvas_->hasFocus());
        key(Qt::Key_F6, Qt::ShiftModifier);
        QTRY_VERIFY(focusIn("tcvn"));
        key(Qt::Key_Escape);
        QTRY_VERIFY(canvas_->hasFocus());

        // Esc của ô tìm còn chữ là của ô (xóa chữ); Esc sau đó về vùng vẽ.
        key(Qt::Key_F6);
        QTRY_VERIFY(focusIn("props"));
        auto* search = qobject_cast<SearchField*>(QApplication::focusWidget());
        QVERIFY(search);
        search->setText(u"abc"_s); // i18n-ok: chữ gõ thử
        key(Qt::Key_Escape);
        QVERIFY(search->text().isEmpty());
        QVERIFY(search->hasFocus());
        key(Qt::Key_Escape);
        QTRY_VERIFY(canvas_->hasFocus());

        // Palette nổi là cửa sổ riêng: F6 kích hoạt nó, Esc kích hoạt lại cửa sổ chính và vùng vẽ.
        host_->moveTo(u"props"_s, Qt::NoDockWidgetArea);
        win_->activateWindow();
        canvas_->setFocus();
        QTRY_VERIFY(canvas_->hasFocus());
        key(Qt::Key_F6);
        QTRY_VERIFY(focusIn("props"));
        QTRY_VERIFY(dock("props")->isActiveWindow());
        key(Qt::Key_Escape);
        QTRY_VERIFY(canvas_->hasFocus());
        QVERIFY(win_->isActiveWindow());
    }

    // Tiêu chí 4, phím theo bảng phím một nguồn: hành động "pane" của Keymap (F6, Shift+F6); phím người dùng gán chạy,
    // phím đã bỏ thôi chạy.
    void focusKeysFromKeymap()
    {
        QVERIFY(build());
        settings::Store keys(dir_.filePath(u"keys.json"_s));
        keymap::Keymap km(keys, keymap::Platform::Windows);
        host_->setKeymap(km);
        canvas_->setFocus();
        QTRY_VERIFY(canvas_->hasFocus());
        const auto key = [](Qt::Key k, Qt::KeyboardModifiers m = Qt::NoModifier) {
            QTest::keyClick(QApplication::focusWidget(), k, m);
        };
        key(Qt::Key_F6);
        QTRY_VERIFY(focusIn("props"));
        key(Qt::Key_Escape);
        QTRY_VERIFY(canvas_->hasFocus());

        QVERIFY(!km.assign(u"pane"_s, u"Ctrl+F6"_s));
        km.unassign(u"pane"_s, u"F6"_s);
        key(Qt::Key_F6);
        QCoreApplication::processEvents();
        QVERIFY(canvas_->hasFocus()); // F6 không còn trong bảng
        key(Qt::Key_F6, Qt::ControlModifier);
        QTRY_VERIFY(focusIn("props"));
        key(Qt::Key_F6, Qt::ShiftModifier); // tổ hợp có Shift: lùi, trước palette đầu là vùng vẽ
        QTRY_VERIFY(canvas_->hasFocus());
    }

    void accessibleNames()
    {
        QVERIFY(build());
        QDockWidget* d = dock("props");
        PaletteHeader* h = host_->header(u"props"_s);
        QAccessibleInterface* a = QAccessible::queryAccessibleInterface(d);
        QVERIFY(a);
        QCOMPARE(a->text(QAccessible::Name), u"Thuộc tính"_s);
        QCOMPARE(h->titleLabel()->text(), u"Thuộc tính"_s);
        QCOMPARE(h->pinButton()->accessibleName(), u"Tự ẩn palette"_s); // như aria-label của prototype
        QVERIFY(h->pinButton()->isChecked());
        QCOMPARE(h->menuButton()->accessibleName(), u"Tùy chọn palette"_s);
        QCOMPARE(h->closeButton()->accessibleName(), u"Đóng palette"_s);

        QSignalSpy pinned(host_, &PaletteHost::pinnedChanged);
        h->pinButton()->click();
        QCOMPARE(pinned.size(), 1);
        QCOMPARE(pinned[0][0].toString(), u"props"_s);
        QCOMPARE(pinned[0][1].toBool(), false);
        QVERIFY(!host_->isPinned(u"props"_s));
        QCOMPARE(h->pinButton()->accessibleName(), u"Ghim palette"_s);
        host_->setPinned(u"props"_s, true);
        QVERIFY(h->pinButton()->isChecked());
        QCOMPARE(pinned.size(), 2);

        h->closeButton()->click();
        QVERIFY(!d->isVisible());
        host_->showPalette(u"props"_s);
        QVERIFY(onScreen(d));

        // Đổi ngôn ngữ: tiêu đề (đầu palette, tên trình đọc màn hình) và tên nút theo bản dịch, không cần dựng lại.
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QTRY_VERIFY(d->windowTitle() != u"Thuộc tính"_s);
        QVERIFY(d->windowTitle().startsWith(u"Thuộc tính"_s));
        QCOMPARE(h->titleLabel()->text(), d->windowTitle());
        QCOMPARE(a->text(QAccessible::Name), d->windowTitle());
        QVERIFY(h->closeButton()->accessibleName().startsWith(u"Đóng palette "_s));
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QTRY_COMPARE(d->windowTitle(), u"Thuộc tính"_s);
        QCOMPARE(h->closeButton()->accessibleName(), u"Đóng palette"_s);
    }

    // Nhóm tab (DoD-U 7; integration nhóm 5 bắt trên cửa sổ chính): Qt cất palette ở tab nền và dải tab của nhóm còn
    // dưới hai palette mở ra tọa độ âm mà vẫn isVisible() — điểm dừng Tab không thấy, dải rỗng không tên; focus của
    // tab vẽ bằng Fusion, không qua TrionStyle.
    void groupAccessibility()
    {
        tm_->setReducedMotion(true); // như drag: thả palette không animation
        QVERIFY(build());
        // Cả cửa sổ mẫu: tên, vai trò, vòng Tab, viền --c-focus. Vùng vẽ giả của mẫu (không tên, không viền) ngoài phạm
        // vi.
        canvas_->setFocusPolicy(Qt::NoFocus);
        const auto audit = [this](QStringList* order = nullptr) {
            return (trion::a11y::checkNames(win_.get()) + trion::a11y::checkFocus(win_.get(), order)).join(u'\n');
        };
        QString issues = audit(); // Thuộc tính mở, Layer đóng: dải của nhóm bị cất
        QVERIFY2(issues.isEmpty(), qPrintable(issues));

        host_->showPalette(u"layers"_s); // lên đầu nhóm; Thuộc tính (đã dựng) thành tab nền
        QTabBar* bar = groupBar();
        QVERIFY(bar);
        // Điểm dừng Tab trong palette theo luật của QWidget::focusNextPrevChild (như a11y::checkFocus).
        const auto stops = [this](const char* id) {
            int n = 0;
            for (const QWidget* w : dock(id)->findChildren<QWidget*>())
                n += w->isVisible() && w->isEnabled() && (w->focusPolicy() & Qt::TabFocus) == Qt::TabFocus;
            return n;
        };
        QCOMPARE(stops("props"), 0);
        QVERIFY(stops("layers") >= 3); // ghim, tùy chọn, đóng, nội dung
        QStringList order;
        issues = audit(&order);
        QVERIFY2(issues.isEmpty(), qPrintable(issues));
        QVERIFY(order.contains(u"%1 ''"_s.arg(QLatin1StringView(bar->metaObject()->className())))); // dải có viền
        canvas_->setFocusPolicy(Qt::StrongFocus);

        // Đổi tab: tab mới mở nhận Tab, tab thành nền thì không; focus đang ở palette thành nền về vùng vẽ.
        host_->header(u"layers"_s)->menuButton()->setFocus();
        bar->setCurrentIndex(tabIndex(bar, dock("props")->windowTitle()));
        QTRY_VERIFY(stops("props") >= 3);
        QCOMPARE(stops("layers"), 0);
        QVERIFY(canvas_->hasFocus());
        // Đóng Layer: nhóm còn một palette, dải bị cất không nhận focus; mở lại thì nhận.
        dock("layers")->close();
        QTRY_COMPARE(bar->focusPolicy(), Qt::NoFocus); // Qt bố cục lại qua hàng đợi
        host_->showPalette(u"layers"_s);
        QTRY_COMPARE(bar->focusPolicy(), Qt::TabFocus);

        // Nhóm nổi tạo bằng kéo (palette thả vào giữa palette nổi): tab nền trong cửa sổ của nhóm không nhận Tab.
        host_->moveTo(u"tcvn"_s, Qt::NoDockWidgetArea);
        host_->moveTo(u"count"_s, Qt::NoDockWidgetArea);
        QList<double> ms;
        dragHeader("count", dock("tcvn")->mapToGlobal(dock("tcvn")->rect().center()), ms);
        QWidget* group = dock("tcvn")->parentWidget();
        QVERIFY(group->isWindow() && group->inherits("QDockWidgetGroupWindow"));
        QCOMPARE(dock("count")->parentWidget(), group);
        const QList<QTabBar*> bars = group->findChildren<QTabBar*>(Qt::FindDirectChildrenOnly);
        QCOMPARE(bars.size(), 1);
        const bool countFront = bars[0]->tabText(bars[0]->currentIndex()) == dock("count")->windowTitle();
        QVERIFY(stops(countFront ? "count" : "tcvn") >= 3);
        QCOMPARE(stops(countFront ? "tcvn" : "count"), 0);
    }

    // Tiêu chí 5 (DoD-U 1, 2).
    void header_data()
    {
        QTest::addColumn<bool>("light");
        QTest::addColumn<qreal>("dpr");
        for (bool light : {false, true})
            for (qreal dpr : {1.0, 1.5, 2.0})
                QTest::addRow("%s-%d", light ? "light" : "dark", qRound(dpr * 100)) << light << dpr;
    }

    void header()
    {
        QFETCH(bool, light);
        QFETCH(qreal, dpr);
        tm_->setTheme(light ? Theme::Light : Theme::Dark);
        QCoreApplication::processEvents(); // Qt 6.11 gửi ApplicationPaletteChange qua hàng đợi
        QVERIFY(build());
        host_->showPalette(u"tcvn"_s);
        host_->setPinned(u"tcvn"_s, false);
        // Hàng 1: Thuộc tính, đang ghim. Hàng 2: thư viện, bỏ ghim, nút menu đang rê.
        PaletteHeader* rows[] = {host_->header(u"props"_s), host_->header(u"tcvn"_s)};
        rows[1]->menuButton()->setAttribute(Qt::WA_UnderMouse);
        QCoreApplication::sendPostedEvents();
        const int w = qRound(tm_->px(tok::pal_w)), h = qRound(tm_->px(tok::h_ctl_l)), ib = qRound(tm_->px(tok::strip));
        for (const PaletteHeader* r : rows) {
            QCOMPARE(r->size(), QSize(w, h)); // palette rộng --pal-w, đầu cao 28 px
            QCOMPARE(r->pinButton()->size(), QSize(ib, ib));
            QCOMPARE(r->closeButton()->geometry().right(), w - 1 - qRound(tm_->px(tok::sp_1)));
        }

        QImage sheet(QSize(w, 2 * h) * dpr, QImage::Format_ARGB32_Premultiplied);
        sheet.setDevicePixelRatio(dpr);
        sheet.fill(tm_->color(tok::c_base));
        QPainter p(&sheet);
        for (int r = 0; r < 2; ++r)
            rows[r]->render(&p, QPoint(0, r * h), {}, QWidget::DrawChildren);
        p.end();
        const QString name = u"header-%1-%2-%3.png"_s.arg(light ? u"light"_s : u"dark"_s)
                                 .arg(qRound(dpr * 100))
                                 .arg(QLatin1StringView(kPlatform));
        const QString out = qEnvironmentVariable("TRION_UPDATE_BASELINE");
        if (!out.isEmpty())
            QVERIFY(sheet.save(out + u'/' + name, "PNG", 0));

        // Màu token ở chỗ không có chữ: gạch tab đang mở, nền tab tiêu đề, nền đầu, nét chia đáy.
        const auto at = [&](int x, int y) { return sheet.pixelColor(int((x + 0.5) * dpr), int((y + 0.5) * dpr)); };
        const int gap = rows[0]->titleLabel()->geometry().right() + 4;
        QVERIFY(near(at(1, 0), tm_->color(tok::c_accent)));
        QVERIFY(near(at(1, h / 2), tm_->color(tok::c_base)));
        QVERIFY(near(at(gap, h / 2), tm_->color(tok::c_chrome)));
        QVERIFY(near(at(gap, h - 1), tm_->color(tok::c_border)));
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
        QVERIFY(build({{"props", kPalettes[0].title, Qt::RightDockWidgetArea, true},
                       {"layers", kPalettes[1].title, Qt::RightDockWidgetArea, true},
                       {"tcvn", kPalettes[2].title, Qt::LeftDockWidgetArea, true},
                       {"count", kPalettes[3].title, Qt::BottomDockWidgetArea, true}}));
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QStringList cut;
        // Quét với mỗi tab của nhóm ở trên. Tab nền Qt dời ra ngoài cửa sổ với cỡ cũ (chưa từng bố cục thì 100×30):
        // không ai thấy, findTruncations bỏ qua.
        const auto scan = [&] {
            QCoreApplication::processEvents(); // LanguageChange → tiêu đề, tên nút; bố cục lại
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            for (const i18n::Truncation& t : i18n::findTruncations(win_.get()))
                cut << t.toString();
        };
        scan();
        QTabBar* bar = groupBar();
        QVERIFY(bar);
        bar->setCurrentIndex(1 - bar->currentIndex());
        scan();
        const bool translated = dock("tcvn")->windowTitle() != u"Thư viện TCVN"_s;
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QVERIFY(translated);
        QVERIFY2(cut.isEmpty(), qPrintable(cut.join(u'\n')));
    }

    void idleNoTimers()
    {
        QVERIFY(build());
        host_->showPalette(u"tcvn"_s);
        host_->moveTo(u"layers"_s, Qt::NoDockWidgetArea);
        spinUntil([] { return false; }, 200ms); // việc còn chờ sau khi hiện (vẽ, bố cục, ghi Store) xong

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

    void errors()
    {
        QVERIFY(build());
        static int fails = 0;
        fails = 0;
        const trion::AssertHandler old = trion::setAssertHandler([](const char*, const char*, int) { ++fails; });
        QVERIFY(!host_->registerPalette(spec(kPalettes[0])));                                     // mã trùng
        QVERIFY(!host_->registerPalette({{}, [] { return QString(); }, [] { return nullptr; }})); // mã rỗng
        QVERIFY(!host_->registerPalette({u"x"_s, {}, [] { return nullptr; }}));   // thiếu hàm tiêu đề
        QVERIFY(!host_->registerPalette({u"y"_s, [] { return QString(); }, {}})); // thiếu hàm dựng
        trion::setAssertHandler(old);
        QCOMPARE(fails, 4);
        QCOMPARE(host_->ids().size(), qsizetype(std::size(kPalettes)));

        // Mã lạ: tra về nullptr, lệnh theo mã bỏ qua.
        QVERIFY(!host_->dock(u"x"_s) && !host_->header(u"x"_s) && !host_->content(u"x"_s));
        host_->showPalette(u"x"_s);
        host_->moveTo(u"x"_s, Qt::LeftDockWidgetArea);
        host_->setPinned(u"x"_s, false);
        QVERIFY(host_->isPinned(u"x"_s));

        // Bố cục đã lưu hỏng: restoreLayout trả false, giữ mặc định.
        QVERIFY(!host_->restoreLayout());                      // chưa lưu
        layout_->setValue(u"ui/palettes/state"_s, u"aGjhu"_s); // khóa của PaletteHost.cpp; base64 của byte rác
        QVERIFY(!host_->restoreLayout());
        QCOMPARE(area("props"), Qt::RightDockWidgetArea);
        QVERIFY(onScreen(dock("props")));
    }
};

QTEST_MAIN(DockTest)
#include "dock_test.moc"
