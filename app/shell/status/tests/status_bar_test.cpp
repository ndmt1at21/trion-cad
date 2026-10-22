// UIA-009 · StatusBar: thanh trạng thái UI-A08.
//   coordThrottle     tiêu chí 1: 1.000 điểm trong một khung → 1 lần vẽ, chữ là điểm cuối; điểm 1 kHz trong 300 ms →
//                     số lần vẽ ≤ số khung + 2; định dạng + vẽ ≤ 0,2 ms/lần (CPU luồng, kiểm ở kTimed)
//   coordModes        Ctrl+I đổi COORDS qua sink giả (1 → 2 → 0 → 1): tuyệt đối, tương đối khoảng cách<góc khi có
//                     điểm gốc, tắt; bấm ô như Ctrl+I; menu chuột phải 'COORDS n; số theo RGN-013 và đơn vị
//   idle              tiêu chí 2: rảnh 1 s: 0 lần vẽ trong thanh, không timer chạy (thanh, presenter), CPU luồng UI
//   cancelFromList    tiêu chí 3: hủy từ danh sách Tác vụ nền: việc nhận hủy ≤ 50 ms, hàng biến mất ≤ 1 khung sau khi
//                     NT-013 báo dừng (BackgroundJobs thật, Timing rút ngắn); danh sách rỗng có câu giải thích
//   narrowOverflow    tiêu chí 4: 1366 px, vi-XL, thoáng và gọn: không chữ nào bị cắt, mục không chồng, mục thừa vào
//                     menu tràn (ưu tiên thấp trước); 2560 px không tràn
//   customizePersist  tiêu chí 4: Tùy biến giữ sau khi dựng lại thanh với cùng Store
//   indicators        MODEL/PAPER, Bác sĩ Font, đồ họa (rê: backend · GPU, FPS), chuông, huy hiệu, Clean Screen
//   viewerProfile     Viewer: nút chế độ theo hồ sơ, Tùy biến chỉ liệt kê mục có
//   keyboard          DoD-U: a11y (tên, vòng Tab, viền focus); Shift+F10 menu tọa độ, Enter mở danh sách, Esc đóng
//   states            DoD-U (thuộc tính, điểm ảnh thay ảnh baseline): rê, nhấn, vô hiệu, chip cảnh báo, màu huy hiệu,
//                     chấm chưa đọc

#include "app/shell/status/ModeToggles.h"
#include "app/shell/status/StatusBar.h"

#include "app/core/a11y/AccessibilityAudit.h"
#include "app/core/actions/ActionRegistry.h"
#include "app/core/edition/EditionBadge.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/keymap/Keymap.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/progress/InlineProgress.h"
#include "app/widgets/progress/ProgressPresenter.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/jobs/BackgroundJobs.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"
#include "tests/timing.h"

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QPainter>
#include <QScreen>
#include <QTemporaryDir>
#include <QtTest>
#include <QVBoxLayout>

#include <atomic>
#include <chrono>
#include <ctime>
#include <functional>
#include <memory>
#include <thread>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;
using namespace trion;
using theme::Density;
using Clock = std::chrono::steady_clock;
using shell::StatusBar;

namespace {

// Lõi giả thay INT-002: ghi chuỗi lệnh; "'TÊN số" đặt sysvar như lõi.
struct FakeCore {
    actions::Sysvars* sysvars = nullptr;
    QStringList macros;
    void submit(const QString& macro)
    {
        macros << macro;
        const QStringList t = macro.split(u' ');
        bool number = false;
        const int v = t.value(1).toInt(&number);
        if (t.size() == 2 && number && t[0].startsWith(u'\'') && !t[0].startsWith(u"'_"_s))
            sysvars->set(t[0].mid(1), v);
    }
};

bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 20s)
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

double ms(Clock::duration d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

std::chrono::milliseconds frame()
{
    const QScreen* s = QGuiApplication::primaryScreen();
    return std::chrono::milliseconds(qRound(1000 / (s && s->refreshRate() > 1 ? s->refreshRate() : 60)));
}

// CPU của luồng gọi (Linux); nơi khác -1: số đo "nan", không kiểm.
std::chrono::nanoseconds threadCpu()
{
#if defined(__linux__)
    timespec t{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return std::chrono::seconds(t.tv_sec) + std::chrono::nanoseconds(t.tv_nsec);
#else
    return std::chrono::nanoseconds(-1);
#endif
}

// Đếm lần vẽ của w và mọi widget con.
struct PaintCounter : QObject {
    QWidget* root;
    int paints = 0;
    Clock::time_point last{};
    explicit PaintCounter(QWidget* r) : root(r) { qApp->installEventFilter(this); }
    ~PaintCounter() override { qApp->removeEventFilter(this); }
    bool eventFilter(QObject* o, QEvent* e) override
    {
        if (e->type() == QEvent::Paint && o->isWidgetType() &&
            (o == root || root->isAncestorOf(static_cast<QWidget*>(o)))) {
            ++paints;
            last = Clock::now();
        }
        return false;
    }
};

// Việc mẫu: chạy đến khi release hoặc bị hủy; ghi lúc thấy yêu cầu hủy.
struct Gate {
    std::atomic<bool> release{false};
    std::atomic<Clock::rep> cancelSeen{0};
};

jobs::JobId startGated(jobs::BackgroundJobs& r, const QString& name, const std::shared_ptr<Gate>& g)
{
    return r.start(name, u"test"_s, 0, [g](jobs::Progress& p, sched::CancelToken t) -> Expected<void> {
        p.set(0.4);
        while (!g->release.load()) {
            if (t.cancelled()) {
                g->cancelSeen = Clock::now().time_since_epoch().count();
                return {};
            }
            std::this_thread::sleep_for(1ms);
        }
        return {};
    });
}

QString truncations(QWidget* root)
{
    QStringList out;
    for (const i18n::Truncation& t : i18n::findTruncations(root))
        out << t.toString();
    return out.join(u"; "_s);
}

// Màu token (có thể trong suốt, như --c-hover) phủ lên nền --c-chrome của thanh.
QColor over(QColor bg, QColor fg)
{
    const double a = fg.alphaF();
    return QColor::fromRgbF(float(fg.redF() * a + bg.redF() * (1 - a)), float(fg.greenF() * a + bg.greenF() * (1 - a)),
                            float(fg.blueF() * a + bg.blueF() * (1 - a)));
}

bool near(QColor a, QColor b, int tol = 6)
{
    return std::abs(a.red() - b.red()) <= tol && std::abs(a.green() - b.green()) <= tol &&
           std::abs(a.blue() - b.blue()) <= tol;
}

} // namespace

class StatusBarTest : public QObject {
    Q_OBJECT

    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<theme::ThemeManager> tm_;
    std::unique_ptr<theme::IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<keymap::Keymap> keymap_;
    std::unique_ptr<ui::RichTooltip> tips_;
    std::unique_ptr<actions::Sysvars> sysvars_;
    std::unique_ptr<actions::ActionRegistry> reg_;
    std::unique_ptr<edition::EditionGate> gate_;
    std::unique_ptr<jobs::BackgroundJobs> jobs_;
    std::unique_ptr<ui::ProgressPresenter> progress_;
    std::unique_ptr<ui::NotificationCenter> notes_;
    std::unique_ptr<QWidget> window_;
    QWidget* viewport_ = nullptr;
    StatusBar* bar_ = nullptr;
    FakeCore core_;
    QStringList screens_; // screenRequested
    std::vector<std::shared_ptr<Gate>> gates_;

    template <class T = QWidget>
    T* item(const char* id) const
    {
        return bar_->findChild<T*>(QString::fromLatin1(id));
    }
    shell::StatusButton* button(const char* id) const { return static_cast<shell::StatusButton*>(item<QToolButton>(id)); }
    QMenu* menuOf(const char* id) const { return item(id)->findChild<QMenu*>(); }

    void makeBar()
    {
        delete bar_;
        bar_ = new StatusBar(*reg_, *sysvars_, *gate_, *progress_, *notes_, *store_);
        window_->layout()->addWidget(bar_);
        connect(bar_, &StatusBar::macroRequested, this, [this](const QString& m) { core_.submit(m); });
        connect(bar_, &StatusBar::screenRequested, this, [this](const QString& id) { screens_ << id; });
        QVERIFY(spinUntil([&] { return bar_->isVisible() && bar_->width() == window_->width(); }));
        settle();
    }

    void settle() { spinFor(4 * frame()); } // lần làm mới tọa độ, lô của presenter

    std::shared_ptr<Gate> gate() { return gates_.emplace_back(std::make_shared<Gate>()); }

    void stopJobs()
    {
        for (auto& g : gates_)
            g->release = true;
        gates_.clear();
        sched::waitIdle();
        QVERIFY(spinUntil([&] { return jobs_->size() == 0 && progress_->shownCount() == 0; }));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        executor_ = std::make_unique<ui::UiExecutor>();
        QVERIFY(theme::loadFonts());
        tm_ = std::make_unique<theme::ThemeManager>(*store_);
        icons_ = std::make_unique<theme::IconProvider>();
        translator_ = std::make_unique<i18n::Translator>();
        keymap_ = std::make_unique<keymap::Keymap>(*store_, keymap::Platform::Windows);
        tips_ = std::make_unique<ui::RichTooltip>();
        tips_->setKeymap(keymap_.get());
        sysvars_ = std::make_unique<actions::Sysvars>();
        core_.sysvars = sysvars_.get();
        reg_ = std::make_unique<actions::ActionRegistry>(*keymap_, *sysvars_);
        reg_->setSink([this](const QString& m) { core_.submit(m); });
        gate_ = std::make_unique<edition::EditionGate>();
        gate_->setEdition(edition::Edition::Pro);
        gate_->bind(*reg_);
        jobs_ = std::make_unique<jobs::BackgroundJobs>(jobs::Timing{20ms, 50ms, 20ms});
        progress_ = std::make_unique<ui::ProgressPresenter>(*jobs_);
        notes_ = std::make_unique<ui::NotificationCenter>();

        window_ = std::make_unique<QWidget>();
        auto* box = new QVBoxLayout(window_.get());
        box->setContentsMargins({});
        box->setSpacing(0);
        viewport_ = new QWidget;
        viewport_->setFocusPolicy(Qt::StrongFocus);
        viewport_->setMinimumHeight(120);
        box->addWidget(viewport_);
        reg_->install(window_.get());
        reg_->addViewport(viewport_);
        window_->resize(1920, 200);
        window_->show();
        window_->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(window_.get()));
        sysvars_->set(u"COORDS"_s, 1);
        makeBar();
    }

    void init()
    {
        if (QWidget* popup = QApplication::activePopupWidget())
            popup->close();
        window_->resize(1920, 200);
        window_->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(window_.get()));
        viewport_->setFocus();
        tm_->setDensity(Density::Compact);
        core_.macros.clear();
        screens_.clear();
    }

    void cleanup() { stopJobs(); }

    void cleanupTestCase()
    {
        window_.reset();
        notes_.reset();
        progress_.reset();
        jobs_.reset();
        gate_.reset();
        reg_.reset();
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
    void coordThrottle()
    {
        QWidget* coord = item("coord");
        PaintCounter count(coord);
        for (int i = 0; i < 1000; ++i) // 1.000 sự kiện chuột trong một khung
            bar_->setCursorPoint(i, 2 * i);
        QVERIFY(spinUntil([&] { return count.paints > 0; }));
        settle();
        QCOMPARE(count.paints, 1);
        QCOMPARE(bar_->shownCoord(), StatusBar::coordText({999, 1998}, {}, 1, {}));

        // Chuột 1 kHz trong 300 ms: tối đa một lần vẽ mỗi khung.
        count.paints = 0;
        QTimer feed;
        feed.setTimerType(Qt::PreciseTimer);
        int n = 0;
        connect(&feed, &QTimer::timeout, this, [&] {
            ++n;
            bar_->setCursorPoint(n, n);
        });
        const auto start = Clock::now();
        feed.start(1);
        spinFor(300ms);
        feed.stop();
        const double frames = ms(Clock::now() - start) / double(frame().count());
        settle();
        qInfo("1 kHz: %d điểm, %d lần vẽ, %.1f khung", n, count.paints, frames);
        QVERIFY(count.paints >= 1);
        QVERIFY2(count.paints <= frames + 2, "vẽ nhiều hơn một lần mỗi khung");

        // Định dạng (RGN-013) + vẽ một lần qua backing store như lần làm mới thật: ≤ 0,2 ms CPU, vòng tốt nhất của 3
        // (máy dùng chung, CPU luồng vẫn lệch theo tải).
        constexpr int kRuns = 200;
        double each = qInf();
        for (int round = 0; round < 3; ++round) {
            std::chrono::nanoseconds cpu{0};
            for (int i = 0; i < kRuns; ++i) {
                const auto t0 = threadCpu();
                const QString s = StatusBar::coordText({-12345.678 + i, 98765.4321 - i}, {}, 1, {});
                coord->repaint();
                cpu += threadCpu() - t0;
                QVERIFY(!s.isEmpty());
            }
            each = std::min(each, threadCpu().count() < 0 ? qQNaN() : ms(cpu) / kRuns);
        }
        qInfo("tọa độ: %.4f ms CPU mỗi lần (định dạng + vẽ), vòng tốt nhất của 3", each);
        QVERIFY2(!test::kTimed || qIsNaN(each) || each <= 0.2, "quá 0,2 ms mỗi lần");
    }

    void coordModes()
    {
        sysvars_->set(u"COORDS"_s, 1);
        bar_->setCursorPoint(100, 50);
        settle();
        QCOMPARE(bar_->shownCoord(), u"100.0000, 50.0000, 0.0000"_s);
        QTest::keyClick(window_.get(), Qt::Key_I, Qt::ControlModifier); // Ctrl+I
        QCOMPARE(core_.macros.takeLast(), u"'COORDS 2"_s);
        settle();
        QCOMPARE(bar_->shownCoord(), u"100.0000, 50.0000, 0.0000"_s); // không có điểm gốc: tuyệt đối
        bar_->setBasePoint(QPointF(100, 0));
        settle();
        QCOMPARE(bar_->shownCoord(), u"50.0000<90, 0.0000"_s);
        bar_->setBasePoint(std::nullopt);
        QTest::keyClick(window_.get(), Qt::Key_I, Qt::ControlModifier);
        QCOMPARE(core_.macros.takeLast(), u"'COORDS 0"_s);
        settle();
        QCOMPARE(bar_->shownCoord(), StatusBar::tr("Tọa độ tắt"));
        QTest::mouseClick(item("coord"), Qt::LeftButton); // bấm ô như Ctrl+I
        QCOMPARE(core_.macros.takeLast(), u"'COORDS 1"_s);

        QMenu* m = menuOf("coord"); // chuột phải: kiểu hiển thị theo COORDS
        emit m->aboutToShow();
        QAction* relative = nullptr;
        for (QAction* a : m->actions())
            if (a->isCheckable()) {
                QCOMPARE(a->isChecked(), a->data().toInt() == 1);
                if (a->data().toInt() == 2)
                    relative = a;
            }
        QVERIFY(relative);
        relative->trigger();
        QCOMPARE(core_.macros.takeLast(), u"'COORDS 2"_s);
        QVERIFY(relative->isChecked());

        sysvars_->set(u"COORDS"_s, 1);
        bar_->setUnits({.luprec = 2});
        settle();
        QCOMPARE(bar_->shownCoord(), u"100.00, 50.00, 0.00"_s);
        bar_->setUnits({});
        settle();
    }

    // Tiêu chí 2 (đo 1 s; 60 s đo bằng bộ đếm hệ điều hành: QA-011).
    void idle()
    {
        settle();
        PaintCounter count(bar_);
        const auto cpu0 = threadCpu();
        spinFor(1s);
        const auto cpu = threadCpu() - cpu0;
        qInfo("rảnh 1 s: %d lần vẽ, CPU luồng UI %.2f ms", count.paints, cpu0.count() < 0 ? qQNaN() : ms(cpu));
        QCOMPARE(count.paints, 0);
        for (QObject* root : {static_cast<QObject*>(bar_), static_cast<QObject*>(progress_.get())})
            for (QTimer* t : root->findChildren<QTimer*>())
                QVERIFY2(!t->isActive(), qPrintable(t->objectName()));
    }

    // Tiêu chí 3.
    void cancelFromList()
    {
        QTest::mouseClick(button("tasks"), Qt::LeftButton); // rỗng: câu giải thích
        QWidget* list = QApplication::activePopupWidget();
        QVERIFY(list);
        QVERIFY(list->findChildren<ui::InlineProgress*>().isEmpty());
        QVERIFY(!list->findChildren<QLabel*>().isEmpty());
        QTest::keyClick(list, Qt::Key_Escape);
        QVERIFY(spinUntil([&] { return !list->isVisible(); }));

        // Ba vòng, lấy vòng tốt nhất (máy dùng chung: độ trễ đồng hồ thật lệch theo tải).
        Clock::duration bestSeen = Clock::duration::max(), bestGone = Clock::duration::max();
        for (int round = 0; round < 3; ++round) {
            const auto g = gate();
            const jobs::JobId id = startGated(*jobs_, u"Đang in"_s, g); // i18n-ok: tên việc mẫu
            QVERIFY(spinUntil([&] { return progress_->shown(id) && item("taskProgress")->isVisible(); }));
            QTest::mouseClick(button("tasks"), Qt::LeftButton);
            QCOMPARE(QApplication::activePopupWidget(), list);
            QCOMPARE(list->findChildren<ui::InlineProgress*>().size(), 1);
            auto* cancel = list->findChild<ui::InlineProgress*>()->findChild<ui::Button*>();
            QVERIFY(cancel && cancel->isVisible());
            Clock::time_point finished{}, gone{};
            QObject scope;
            connect(progress_.get(), &ui::ProgressPresenter::finished, &scope, [&] { finished = Clock::now(); });
            connect(progress_->model(), &QAbstractItemModel::rowsRemoved, &scope, [&] { // danh sách bỏ hàng ở tín hiệu này
                gone = Clock::now();
                QVERIFY(list->findChildren<ui::InlineProgress*>().isEmpty());
            });
            PaintCounter paints(list);
            const auto clicked = Clock::now();
            QTest::mouseClick(cancel, Qt::LeftButton);
            QVERIFY(spinUntil([&] { return g->cancelSeen.load() != 0 && finished != Clock::time_point{}; }));
            QVERIFY(spinUntil([&] { return gone != Clock::time_point{} && paints.last > gone; }));
            const Clock::time_point seen{Clock::duration(g->cancelSeen.load())};
            qInfo("hủy từ danh sách: việc nhận hủy sau %.2f ms, NT-013 báo dừng sau %.2f ms, hàng bỏ sau %.2f ms "
                  "(1 khung %lld ms)",
                  ms(seen - clicked), ms(finished - clicked), ms(gone - finished), qint64(frame().count()));
            bestSeen = std::min(bestSeen, seen - clicked);
            bestGone = std::min(bestGone, gone - finished);
            QVERIFY(spinUntil([&] { return !item("taskProgress")->isVisible(); }));
            QTest::keyClick(list, Qt::Key_Escape);
            QVERIFY(spinUntil([&] { return !list->isVisible(); }));
        }
        QVERIFY2(!test::kTimed || bestSeen <= 50ms, "việc nhận hủy sau hơn 50 ms");
        QVERIFY2(!test::kTimed || bestGone <= frame() + 1ms, "hàng còn sau một khung");
    }

    // Tiêu chí 4.
    void narrowOverflow()
    {
        const auto g = gate();
        startGated(*jobs_, u"Đang chuyển font"_s, g); // i18n-ok: tên việc mẫu
        QVERIFY(spinUntil([&] { return progress_->shownCount() == 1; }));
        bar_->setFontIssues(3, 250);
        gate_->setEdition(edition::Edition::Trial);
        gate_->setTrialDays(12);
        gate_->setOfflineDays(3);
        notes_->add(u"Đã xuất"_s, ui::Severity::Ok); // i18n-ok: thông báo mẫu
        bar_->setCursorPoint(-12345.6789, -98765.4321);
        window_->resize(1366, 200);
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QVERIFY(spinUntil([&] { return item("tasks")->accessibleName() != StatusBar::tr("Tác vụ nền"); }));
        for (const Density d : {Density::Comfortable, Density::Compact}) {
            tm_->setDensity(d);
            settle();
            QCOMPARE(bar_->width(), 1366);
            QCOMPARE(truncations(window_.get()), QString());
            const QStringList spilled = bar_->overflowed();
            qInfo("%s: tràn %s", d == Density::Compact ? "gọn" : "thoáng", qPrintable(spilled.join(u", "_s)));
            if (d == Density::Comfortable)
                QVERIFY(!spilled.isEmpty());
            QCOMPARE(button("overflow")->isVisible(), !spilled.isEmpty());
            for (const char* keep : {"coord", "space", "osnap", "taskProgress"}) // ưu tiên cao còn trên thanh
                QVERIFY2(!spilled.contains(QLatin1StringView(keep)), keep);
            if (!spilled.isEmpty())
                QVERIFY(spilled.contains(u"graphics"_s)); // ưu tiên thấp nhất tràn trước
            // Mục đang hiện nằm trong thanh, không chồng nhau
            QList<QRect> rects;
            for (QWidget* w : bar_->findChildren<QWidget*>(Qt::FindDirectChildrenOnly))
                if (w->isVisible() && !w->isWindow()) {
                    QVERIFY2(bar_->rect().contains(w->geometry()), qPrintable(w->objectName()));
                    for (const QRect& r : rects)
                        QVERIFY2(!r.intersects(w->geometry()), qPrintable(w->objectName()));
                    rects << w->geometry();
                }
            // Menu tràn: đúng các mục tràn
            QMenu* m = menuOf("overflow");
            emit m->aboutToShow();
            QCOMPARE(m->actions().isEmpty(), spilled.isEmpty());
            if (spilled.contains(u"graphics"_s))
                QVERIFY(m->actions().contains(reg_->action(u"GRAPHICSCONFIG"_s)));
        }
        QVERIFY(translator_->setLanguage(u"vi"_s));
        window_->resize(2560, 200);
        QVERIFY(spinUntil([&] { return bar_->width() == 2560 && bar_->overflowed().isEmpty(); }));
        QVERIFY(!button("overflow")->isVisible());
        bar_->setFontIssues(0, 0);
        gate_->setOfflineDays(-1);
        gate_->setEdition(edition::Edition::Pro);
        notes_->markAllRead();
    }

    // Tiêu chí 4: lựa chọn Tùy biến giữ sau khi dựng lại với cùng Store.
    void customizePersist()
    {
        QMenu* m = menuOf("customize");
        emit m->aboutToShow();
        QAction* graphics = nullptr;
        for (QAction* a : m->actions())
            if (a->data().toString() == u"graphics"_s)
                graphics = a;
        QVERIFY(graphics && graphics->isChecked());
        QVERIFY(item("graphics")->isVisible());
        graphics->trigger(); // tắt
        QVERIFY(!item("graphics")->isVisible());
        QCOMPARE(store_->value(u"ui/statusbar/hidden"_s).toArray().first().toString(), u"graphics"_s);
        makeBar(); // dựng lại, cùng Store
        QVERIFY(!item("graphics")->isVisible());
        m = menuOf("customize");
        emit m->aboutToShow();
        for (QAction* a : m->actions())
            if (a->data().toString() == u"graphics"_s) {
                QVERIFY(!a->isChecked());
                a->trigger(); // bật lại
            }
        QVERIFY(item("graphics")->isVisible());
        QVERIFY(store_->value(u"ui/statusbar/hidden"_s).toArray().isEmpty());
    }

    void indicators()
    {
        // MODEL/PAPER: chỉ trong layout
        QVERIFY(!item("space")->isEnabled());
        bar_->setSpace(StatusBar::Space::Paper);
        QCOMPARE(item<QToolButton>("space")->text(), u"PAPER"_s);
        QTest::mouseClick(item("space"), Qt::LeftButton);
        QCOMPARE(core_.macros.takeLast(), u"^C^C_MSPACE"_s);
        bar_->setSpace(StatusBar::Space::ModelInViewport);
        QTest::mouseClick(item("space"), Qt::LeftButton);
        QCOMPARE(core_.macros.takeLast(), u"^C^C_PSPACE"_s);
        bar_->setSpace(StatusBar::Space::Model);

        // Bác sĩ Font
        QVERIFY(!item("fontDoctor")->isVisible());
        bar_->setFontIssues(3, 250);
        QVERIFY(spinUntil([&] { return item("fontDoctor")->isVisible(); }));
        QCOMPARE(item<QToolButton>("fontDoctor")->text(), StatusBar::tr("Font: %1 thay thế", nullptr, 3).arg(3));
        QVERIFY(ui::RichTooltip::info(item("fontDoctor")).description.contains(u"3 font đã thay thế · 250 đối tượng TCVN3"_s));
        QTest::mouseClick(item("fontDoctor"), Qt::LeftButton);
        QVERIFY(core_.macros.takeLast().endsWith(u"_FIXFONT"_s));
        bar_->setFontIssues(0, 0);
        QVERIFY(spinUntil([&] { return !item("fontDoctor")->isVisible(); }));

        // Đồ họa: thông tin đọc lúc rê
        int fps = 60;
        bar_->setGraphicsSource([&] { return StatusBar::GraphicsInfo{u"D3D11"_s, u"Intel UHD 620"_s, double(fps)}; });
        fps = 144;
        QEnterEvent enter(QPointF(2, 2), QPointF(2, 2), QPointF(2, 2));
        QCoreApplication::sendEvent(item("graphics"), &enter);
        const ui::TipInfo gpu = ui::RichTooltip::info(item("graphics"));
        QCOMPARE(gpu.name, StatusBar::tr("Đồ họa: %1").arg(u"D3D11 · Intel UHD 620"_s));
        QVERIFY(gpu.description.contains(u"144 FPS"_s));
        tips_->leave();
        QTest::mouseClick(item("graphics"), Qt::LeftButton);
        QVERIFY(core_.macros.takeLast().endsWith(u"_GRAPHICSCONFIG"_s));
        bar_->setGraphicsSource({});

        // Chuông: chấm khi có thông báo chưa đọc, số ở mô tả tiếp cận
        QVERIFY(!button("notifications")->dot);
        notes_->add(u"Đã in"_s, ui::Severity::Ok); // i18n-ok: thông báo mẫu
        QVERIFY(button("notifications")->dot);
        QVERIFY(button("notifications")->accessibleDescription().contains(u'1'));
        QTest::mouseClick(button("notifications"), Qt::LeftButton);
        QCOMPARE(screens_.takeLast(), u"notifications"_s);
        notes_->markAllRead();
        QVERIFY(!button("notifications")->dot);

        // Huy hiệu phiên bản: theo EditionGate (phiên bản, ngày dùng thử, ngày offline)
        gate_->setEdition(edition::Edition::Trial);
        gate_->setTrialDays(30);
        QCOMPARE(item<QToolButton>("edition")->text(), edition::EditionBadge::tr("Dùng thử: còn %n ngày", nullptr, 30));
        gate_->setTrialDays(12);
        const QString trial = edition::EditionBadge::tr("Dùng thử: còn %n ngày", nullptr, 12);
        QCOMPARE(item<QToolButton>("edition")->text(), trial);
        const int badge = edition::EditionBadge::sizeFor(*gate_).width();
        QCOMPARE(item("edition")->sizeHint().width(), badge + 2 * qRound(tm_->px(tok::sp_2))); // .sb .ed margin 0 4px
        gate_->setOfflineDays(3);
        QVERIFY(item<QToolButton>("edition")->text().startsWith(trial + u" · "_s));
        QTest::mouseClick(item("edition"), Qt::LeftButton);
        QCOMPARE(screens_.takeLast(), u"license"_s);
        gate_->setOfflineDays(-1);
        gate_->setEdition(edition::Edition::Pro);
        QCOMPARE(item<QToolButton>("edition")->text(), edition::EditionBadge::tr("Pro"));

        // Clean Screen: QAction CLEANSCREENON của registry (Ctrl+0)
        QTest::mouseClick(button("cleanScreen"), Qt::LeftButton);
        QCOMPARE(core_.macros.takeLast(), u"'_CLEANSCREENON"_s);
    }

    void viewerProfile()
    {
        gate_->setEdition(edition::Edition::Viewer);
        settle();
        QStringList shown;
        for (QToolButton* b : bar_->findChild<shell::ModeToggles*>()->findChildren<QToolButton*>(Qt::FindDirectChildrenOnly))
            if (b->isVisible())
                shown << b->defaultAction()->objectName();
        QCOMPARE(shown, (QStringList{u"osnap"_s, u"LWDISPLAY"_s, u"TRANSPARENCYDISPLAY"_s}));
        QMenu* m = menuOf("customize");
        emit m->aboutToShow();
        for (QAction* a : m->actions())
            if (a->data().toString() == u"grid"_s)
                QVERIFY(!a->isVisible()); // không có trong Viewer: không liệt kê
            else if (a->data().toString() == u"osnap"_s)
                QVERIFY(a->isVisible());
        QCOMPARE(item<QToolButton>("edition")->text(), edition::EditionBadge::tr("Viewer"));
        gate_->setEdition(edition::Edition::Pro);
        settle();
        QVERIFY(bar_->findChild<shell::ModeToggles*>()->button(u"grid"_s)->isVisible());
    }

    // DoD-U: dùng hoàn toàn bằng bàn phím, tên cho trình đọc màn hình.
    void keyboard()
    {
        bar_->setSpace(StatusBar::Space::Paper);
        QCOMPARE(a11y::checkNames(bar_), QStringList());
        viewport_->setFocusPolicy(Qt::NoFocus); // vùng vẽ mẫu không có viền focus: chỉ đi vòng Tab của thanh
        QStringList order;
        const QStringList issues = a11y::checkFocus(window_.get(), &order);
        viewport_->setFocusPolicy(Qt::StrongFocus);
        QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
        QVERIFY2(order.size() >= 15, qPrintable(order.join(u", "_s)));

        item("coord")->setFocus(Qt::TabFocusReason);
        QTest::keyClick(item("coord"), Qt::Key_F10, Qt::ShiftModifier);
        QCOMPARE(QApplication::activePopupWidget(), menuOf("coord"));
        QTest::keyClick(menuOf("coord"), Qt::Key_Escape);
        QVERIFY(spinUntil([&] { return !QApplication::activePopupWidget(); }));

        button("tasks")->setFocus(Qt::TabFocusReason);
        QTest::keyClick(button("tasks"), Qt::Key_Return);
        QWidget* list = QApplication::activePopupWidget();
        QVERIFY(list && list->isVisible());
        QTest::keyClick(list, Qt::Key_Escape);
        QVERIFY(spinUntil([&] { return !list->isVisible(); }));
        QVERIFY(spinUntil([&] { return button("tasks")->hasFocus(); }));

        button("customize")->setFocus(Qt::TabFocusReason);
        QTest::keyClick(button("customize"), Qt::Key_Space);
        QCOMPARE(QApplication::activePopupWidget(), menuOf("customize"));
        QTest::keyClick(menuOf("customize"), Qt::Key_Escape);
        bar_->setSpace(StatusBar::Space::Model);
    }

    // DoD-U: trạng thái theo token (điểm ảnh của thanh thay ảnh baseline có chữ theo nền tảng).
    void states()
    {
        const auto on = [&](tok::ColorId c) { return over(tm_->color(tok::c_chrome), tm_->color(c)); };
        const auto pixel = [&](const char* id, QPoint at) {
            const QImage img = bar_->grab().toImage();
            return img.pixelColor(item(id)->geometry().topLeft() + at);
        };
        bar_->setSpace(StatusBar::Space::Paper);
        auto* space = item<QToolButton>("space");
        QVERIFY(near(pixel("space", {1, 4}), tm_->color(tok::c_chrome)));
        space->setAttribute(Qt::WA_UnderMouse, true);
        QVERIFY(near(pixel("space", {1, 4}), on(tok::c_hover)));
        space->setDown(true);
        QVERIFY(near(pixel("space", {1, 4}), on(tok::c_press)));
        space->setDown(false);
        bar_->setSpace(StatusBar::Space::Model); // vô hiệu: rê không đổi nền
        QVERIFY(near(pixel("space", {1, 4}), tm_->color(tok::c_chrome)));
        space->setAttribute(Qt::WA_UnderMouse, false);

        bar_->setFontIssues(1, 0); // chip cảnh báo: nền --c-warn-soft
        QVERIFY(spinUntil([&] { return item("fontDoctor")->isVisible(); }));
        QVERIFY(near(pixel("fontDoctor", {2, item("fontDoctor")->height() / 2}), on(tok::c_warn_soft)));
        bar_->setFontIssues(0, 0);

        gate_->setEdition(edition::Edition::Trial); // huy hiệu theo phiên bản; offline: cảnh báo
        const QPoint badge(tm_->px(tok::sp_2) + 2, item("edition")->height() / 2);
        settle();
        QVERIFY(near(pixel("edition", badge), on(tok::c_ed_trial)));
        gate_->setOfflineDays(2);
        settle();
        QVERIFY(near(pixel("edition", badge), on(tok::c_warn_soft)));
        gate_->setOfflineDays(-1);
        gate_->setEdition(edition::Edition::Pro);

        auto* bell = button("notifications"); // chấm --c-danger
        const QPoint dot(bell->width() - tm_->px(tok::sp_2) - tm_->px(tok::sp_3) / 2, tm_->px(tok::sp_1) + tm_->px(tok::sp_3) / 2);
        QVERIFY(!near(pixel("notifications", dot), tm_->color(tok::c_danger), 40));
        notes_->add(u"Đã lưu"_s, ui::Severity::Ok); // i18n-ok: thông báo mẫu
        QVERIFY(near(pixel("notifications", dot), tm_->color(tok::c_danger), 40));
        notes_->markAllRead();
    }
};

QTEST_MAIN(StatusBarTest)
#include "status_bar_test.moc"
