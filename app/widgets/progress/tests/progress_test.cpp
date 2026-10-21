// UI0-112: ProgressPresenter, ProgressItemModel, InlineProgress trên NT-013 (BackgroundJobs). Mỗi tiêu chí nghiệm thu
// một nhóm hàm test (ngưỡng rút ngắn qua jobs::Timing, trừ realTime chạy đúng 0,5 s/2 s):
//   1  thresholds, realTime: việc < 0,5 s không hiện gì (không con trỏ bận, không hàng, widget ẩn); 0,5–2 s con trỏ
//      bận, vùng vẽ mẫu vẫn nhận chuột, phím và vẽ lại; > 2 s hàng trong model, InlineProgress hiện thanh % và nút
//      hủy. Độ lệch so với mốc (đổi con trỏ, lần vẽ đầu của thanh) ≤ 1 khung ở vòng tốt nhất của 3 (thresholds), chỉ
//      kiểm ở release; realTime chạy đúng 0,5 s/2 s một lần, kiểm thứ tự và cận dưới, báo độ lệch.
//   2  cancelButton: bấm Hủy → nút "Đang hủy…" và tắt ngay khi cú bấm xử lý xong (lần vẽ kế đã có chữ mới, ≤ 1 khung
//      ở release); NT-013 báo Cancelled ≤ 50 ms (finished, kiểm ở release); hủy từ nơi khác cũng hiện "Đang hủy…".
//   3  throttle: việc báo 10.000 lần/s: changed() của presenter cách nhau ≥ 1 khung (tối đa 1 lần mỗi khung), số lần
//      vẽ thanh ≤ số lần áp; xong thì rảnh: 0 lần áp, 0 lần vẽ, mọi timer tắt.
//   4  multiple: 3 việc: tác vụ chính (cũ nhất) và "2 tác vụ khác"; trình đọc màn hình (bộ bắt sự kiện QAccessible):
//      giá trị thanh chỉ đổi theo bước 10% (10, 20, …, 100), tên tiếp cận của nhãn cũng vậy.
//   DoD-U: baseline (ảnh InlineProgress Tối/Sáng × 100/150/200% so baseline theo nền tảng, như UI0-106; ghi lại:
//   TRION_UPDATE_BASELINE=<thư mục>), pseudoNoTruncation (vi-XL, findTruncations rỗng). Hàm công khai còn lại:
//   presenterApi (model, finished, cancelling, label/percent, hủy presenter khi đang bận trả con trỏ).
#include "app/widgets/progress/InlineProgress.h"
#include "app/widgets/progress/ProgressPresenter.h"

#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "core/base/sched/Scheduler.h"
#include "tests/timing.h"

#include <QAccessible>
#include <QApplication>
#include <QBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QProgressBar>
#include <QScreen>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <atomic>
#include <limits>
#include <memory>
#include <thread>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using namespace trion::theme;
using namespace trion::ui;
using trion::Expected;
using trion::jobs::Clock;
using trion::jobs::JobId;
using trion::jobs::JobInfo;
using trion::jobs::State;
using trion::test::kTimed;
namespace i18n = trion::i18n;
namespace jobs = trion::jobs;
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
constexpr int kNoise = 8; // baseline: lệch ≤ 8/255 mỗi kênh bỏ qua, điểm lệch hơn được tối đa 0,1% ảnh
constexpr int kSheetW = 380; // bề rộng một InlineProgress trong tấm ảnh

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

Clock::duration frameTime()
{
    const QScreen* s = QGuiApplication::primaryScreen();
    return std::chrono::nanoseconds(qint64(1e9 / (s && s->refreshRate() > 1 ? s->refreshRate() : 60)));
}

// Việc mẫu do test điều khiển: báo `progress` khi đổi, chạy đến khi release (hoặc bị hủy nếu honorCancel).
struct Gate {
    std::atomic<bool> release{false};
    std::atomic<bool> honorCancel{true};
    std::atomic<double> progress{0};
};

JobId startGated(jobs::BackgroundJobs& r, const QString& name, const std::shared_ptr<Gate>& g, std::uint64_t owner = 0)
{
    return r.start(name, u"test"_s, owner, [g](jobs::Progress& p, sched::CancelToken t) -> Expected<void> {
        double last = -1;
        while (!g->release.load() && !(g->honorCancel.load() && t.cancelled())) {
            if (const double v = g->progress.load(); v != last) {
                p.set(v);
                last = v;
            }
            std::this_thread::sleep_for(1ms);
        }
        return {};
    });
}

// Vùng vẽ mẫu: đếm chuột, phím, lần vẽ (tiêu chí 1: không bị chặn khi đang bận).
class Canvas final : public QWidget {
public:
    using QWidget::QWidget;
    int clicks = 0, keys = 0, paints = 0;

protected:
    void mousePressEvent(QMouseEvent*) override { ++clicks; }
    void keyPressEvent(QKeyEvent*) override { ++keys; }
    void paintEvent(QPaintEvent*) override { ++paints; }
};

// Ghi lúc hiện và lần vẽ của một widget.
class Watcher final : public QObject {
public:
    explicit Watcher(QWidget* w) { w->installEventFilter(this); }
    Clock::time_point shownAt{}, firstPaintAt{};
    int paints = 0;

protected:
    bool eventFilter(QObject*, QEvent* e) override
    {
        if (e->type() == QEvent::Show && shownAt == Clock::time_point{})
            shownAt = Clock::now();
        if (e->type() == QEvent::Paint) {
            ++paints;
            if (shownAt != Clock::time_point{} && firstPaintAt == Clock::time_point{})
                firstPaintAt = Clock::now();
        }
        return false;
    }
};

// Bộ bắt sự kiện tiếp cận (tiêu chí 4).
struct A11yEvent {
    QAccessible::Event type;
    QObject* object;
    int value;    // ValueChanged
    QString name; // NameChanged: tên tiếp cận lúc đó
};
std::vector<A11yEvent> g_a11y;

void recordA11y(QAccessibleEvent* e)
{
    A11yEvent r{e->type(), e->object(), 0, {}};
    if (e->type() == QAccessible::ValueChanged)
        r.value = static_cast<QAccessibleValueChangeEvent*>(e)->value().toInt();
    if (e->type() == QAccessible::NameChanged)
        if (auto* w = qobject_cast<QWidget*>(e->object()))
            r.name = w->accessibleName();
    g_a11y.push_back(r);
}

} // namespace

class ProgressTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<UiExecutor> executor_;
    std::unique_ptr<ThemeManager> tm_;
    std::unique_ptr<IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<QWidget> host_; // cửa sổ đang hiện chứa widget của test
    std::unique_ptr<jobs::BackgroundJobs> jobs_;
    std::unique_ptr<ProgressPresenter> presenter_;
    std::vector<std::shared_ptr<Gate>> gates_; // thả hết ở cleanup()

    void use(jobs::Timing timing)
    {
        jobs_ = std::make_unique<jobs::BackgroundJobs>(timing);
        presenter_ = std::make_unique<ProgressPresenter>(*jobs_);
    }

    std::shared_ptr<Gate> gate() { return gates_.emplace_back(std::make_shared<Gate>()); }

    // Dừng mọi việc, chờ lô cuối, rồi hủy presenter trước registry.
    void stop()
    {
        for (auto& g : gates_)
            g->release = true;
        gates_.clear();
        sched::waitIdle();
        if (jobs_)
            QVERIFY(spinUntil([&] { return jobs_->size() == 0; }));
        presenter_.reset();
        jobs_.reset();
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
        host_ = std::make_unique<QWidget>();
        host_->resize(kSheetW + 40, 200);
        host_->show();
        QVERIFY(QTest::qWaitForWindowExposed(host_.get()));
    }

    void init()
    {
        tm_->setTheme(Theme::Dark);
        tm_->setDensity(Density::Compact);
    }

    void cleanup()
    {
        stop();
        qDeleteAll(host_->findChildren<QWidget*>(Qt::FindDirectChildrenOnly));
        QCoreApplication::processEvents();
    }

    void cleanupTestCase()
    {
        stop();
        host_.reset();
        translator_.reset();
        icons_.reset();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        sched::waitIdle();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    // Tiêu chí 1, ngưỡng rút ngắn: con trỏ bận 100 ms, tiến trình 300 ms.
    void thresholds()
    {
        use({100ms, 300ms, 20ms});
        auto* item = new InlineProgress(*presenter_, 0, host_.get());
        auto* canvas = new Canvas(host_.get());
        canvas->setGeometry(0, 60, 100, 100);
        canvas->setFocusPolicy(Qt::StrongFocus);
        canvas->show();
        QSignalSpy rows(presenter_->model(), &QAbstractItemModel::rowsInserted);
        QObject scope; // nối tín hiệu tới biến cục bộ: gỡ khi hàm test trả về
        Clock::time_point busyAt{};
        connect(presenter_.get(), &ProgressPresenter::busyChanged, &scope, [&busyAt](bool on) {
            if (on && busyAt == Clock::time_point{})
                busyAt = Clock::now();
        });
        std::vector<JobInfo> finished;
        connect(presenter_.get(), &ProgressPresenter::finished, &scope,
                [&finished](const JobInfo& i) { finished.push_back(i); });

        // Việc ngắn: xong ngay, qua cả hai mốc vẫn không hiện gì.
        const JobId quick = jobs_->start(u"nhanh"_s, u"test"_s, 0,
                                         [](jobs::Progress&, sched::CancelToken) -> Expected<void> { return {}; });
        QVERIFY(spinUntil([&] { return !finished.empty(); }));
        QCOMPARE(finished.front().id, quick);
        spinFor(400ms);
        if (finished.front().elapsed < 100ms) { // máy tải nặng có thể giữ worker quá mốc: khi đó việc không còn "ngắn"
            QVERIFY(busyAt == Clock::time_point{});
            QCOMPARE(rows.count(), 0);
            QVERIFY(!item->isVisible());
            QCOMPARE(QGuiApplication::overrideCursor(), nullptr);
        }

        // Việc dài, 3 vòng: vòng đầu kiểm đủ trạng thái; độ lệch so với mốc lấy vòng tốt nhất (máy dùng chung: hệ
        // điều hành có thể dừng luồng UI bất kỳ lúc nào).
        const auto frame = frameTime();
        auto bestBusy = Clock::duration::max(), bestShow = Clock::duration::max();
        for (int round = 0; round < 3; ++round) {
            Watcher bar(item->findChild<QProgressBar*>());
            busyAt = {};
            const auto g = gate();
            const JobId id = startGated(*jobs_, u"Việc mẫu"_s, g);
            const auto started = jobs_->info(id)->startedAt;
            QVERIFY(spinUntil([&] { return presenter_->busy(); }));
            if (round == 0) {
                QVERIFY(QGuiApplication::overrideCursor());
                QCOMPARE(QGuiApplication::overrideCursor()->shape(), Qt::BusyCursor);
                QCOMPARE(presenter_->model()->rowCount(), 0);
                QVERIFY(!item->isVisible());
                // Vùng vẽ không bị chặn: chuột, phím tới nơi, vẽ lại được.
                QTest::mouseClick(canvas, Qt::LeftButton);
                QTest::keyClick(canvas, Qt::Key_L);
                QCOMPARE(canvas->clicks, 1);
                QCOMPARE(canvas->keys, 1);
                const int paints = canvas->paints;
                canvas->update();
                QVERIFY(spinUntil([&] { return canvas->paints > paints; }));
            }
            QVERIFY(spinUntil([&] { return bar.firstPaintAt != Clock::time_point{}; }));
            QVERIFY(item->isVisible());
            QVERIFY(busyAt >= started + 100ms);
            QVERIFY(bar.shownAt >= started + 300ms);
            bestBusy = std::min<Clock::duration>(bestBusy, busyAt - started - 100ms);
            bestShow = std::min<Clock::duration>(bestShow, bar.firstPaintAt - started - 300ms);
            if (round == 0) {
                QCOMPARE(presenter_->model()->rowCount(), 1);
                QCOMPARE(presenter_->primary(), id);
                QCOMPARE(item->current(), id);
                QCOMPARE(item->job(), JobId(0));
                QVERIFY(item->findChild<Button*>()->isVisible());
                QCOMPARE(item->findChild<QLabel*>()->text(), ProgressPresenter::label(u"Việc mẫu"_s, 0));
                qInfo("mốc 100/300 ms, vòng đầu: con trỏ bận lệch %.2f ms, thanh hiện lệch %.2f ms, vẽ lần đầu lệch "
                      "%.2f ms",
                      ms(busyAt - started - 100ms), ms(bar.shownAt - started - 300ms),
                      ms(bar.firstPaintAt - started - 300ms));
            }
            g->release = true;
            QVERIFY(spinUntil([&] { return !item->isVisible() && !presenter_->busy(); }));
            QCOMPARE(QGuiApplication::overrideCursor(), nullptr);
            QCOMPARE(presenter_->model()->rowCount(), 0);
            QCOMPARE(item->current(), JobId(0));
            QCOMPARE(finished.back().id, id);
            QCOMPARE(finished.back().state, State::Done);
        }
        qInfo("tốt nhất 3 vòng: con trỏ bận lệch %.2f ms, thanh vẽ lần đầu lệch %.2f ms (1 khung %.2f ms)",
              ms(bestBusy), ms(bestShow), ms(frame));
        QVERIFY2(!kTimed || bestBusy <= frame, "con trỏ bận lệch quá 1 khung");
        QVERIFY2(!kTimed || bestShow <= frame, "thanh tiến trình lệch quá 1 khung");
    }

    // Tiêu chí 1, thời gian thật (0,5 s/2 s): một việc 0,2 s không hiện gì, một việc 2,3 s qua đủ hai mốc.
    void realTime()
    {
        use({});
        auto* item = new InlineProgress(*presenter_, 0, host_.get());
        Watcher bar(item->findChild<QProgressBar*>());
        QObject scope;
        Clock::time_point busyAt{};
        int busyChanges = 0;
        connect(presenter_.get(), &ProgressPresenter::busyChanged, &scope, [&](bool on) {
            ++busyChanges;
            if (on && busyAt == Clock::time_point{})
                busyAt = Clock::now();
        });
        Clock::duration shortRan{};
        connect(presenter_.get(), &ProgressPresenter::finished, &scope,
                [&shortRan](const JobInfo& i) { shortRan = i.elapsed; });
        const JobId shortJob =
            jobs_->start(u"ngắn"_s, u"test"_s, 0, [](jobs::Progress& p, sched::CancelToken) -> Expected<void> {
                std::this_thread::sleep_for(200ms);
                p.set(1);
                return {};
            });
        QVERIFY(spinUntil([&] { return !jobs_->info(shortJob); }));
        spinFor(500ms);         // quá mốc 0,5 s của việc ngắn
        if (shortRan < 500ms) { // máy tải nặng có thể giữ worker quá mốc
            QCOMPARE(busyChanges, 0);
            QVERIFY(!item->isVisible());
        }
        busyChanges = 0;
        busyAt = {};

        const auto g = gate();
        const JobId id = startGated(*jobs_, u"Đang mở"_s, g);
        const auto started = jobs_->info(id)->startedAt;
        QVERIFY(spinUntil([&] { return presenter_->busy(); }, 5s));
        QVERIFY(!item->isVisible());
        QVERIFY(spinUntil([&] { return bar.firstPaintAt != Clock::time_point{}; }, 5s));
        // Lệch ≤ 1 khung kiểm ở thresholds (vòng tốt nhất của 3); ở đây chỉ báo số đo của một lần chạy.
        qInfo("thời gian thật: con trỏ bận sau %.1f ms (mốc 500), thanh hiện sau %.1f ms, vẽ sau %.1f ms (mốc 2000)",
              ms(busyAt - started), ms(bar.shownAt - started), ms(bar.firstPaintAt - started));
        QVERIFY(busyAt - started >= 500ms);
        QVERIFY(bar.shownAt - started >= 2000ms);
        g->release = true;
        QVERIFY(spinUntil([&] { return !item->isVisible() && !presenter_->busy(); }));
        QCOMPARE(busyChanges, 2);
    }

    // Tiêu chí 2.
    void cancelButton()
    {
        use({20ms, 50ms, 20ms});
        const auto g = gate();
        const JobId id = startGated(*jobs_, u"Đang in"_s, g);
        auto* item = new InlineProgress(*presenter_, id, host_.get());
        item->resize(kSheetW, item->sizeHint().height());
        QVERIFY(spinUntil([&] { return item->isVisible(); }));
        auto* cancel = item->findChild<Button*>();
        QCOMPARE(cancel->text(), InlineProgress::tr("Hủy"));
        Watcher button(cancel);
        QSignalSpy cancelling(presenter_->model(), &QAbstractItemModel::dataChanged);
        QObject scope;
        Clock::time_point endedAt{};
        State endState = State::Running;
        connect(presenter_.get(), &ProgressPresenter::finished, &scope, [&](const JobInfo& i) {
            endedAt = Clock::now();
            endState = i.state;
        });
        const int paintsBefore = button.paints;
        const auto clicked = Clock::now();
        QTest::mouseClick(cancel, Qt::LeftButton);
        // Ngay sau khi cú bấm xử lý xong: chữ mới, nút tắt; lần vẽ kế (khung kế) đã có chữ đó.
        QCOMPARE(cancel->text(), InlineProgress::tr("Đang hủy…"));
        QVERIFY(!cancel->isEnabled());
        QVERIFY(spinUntil([&] { return button.paints > paintsBefore; }));
        const auto repainted = Clock::now();
        QVERIFY(spinUntil([&] { return endState != State::Running; }));
        QCOMPARE(endState, State::Cancelled);
        QVERIFY(spinUntil([&] { return !item->isVisible(); }));
        const auto frame = frameTime();
        qInfo("bấm hủy: vẽ lại sau %.2f ms (1 khung %.2f ms); NT-013 báo Cancelled sau %.2f ms",
              ms(repainted - clicked), ms(frame), ms(endedAt - clicked));
        QVERIFY2(!kTimed || repainted - clicked <= frame, "\"Đang hủy…\" chưa vẽ trong khung kế");
        QVERIFY2(!kTimed || endedAt - clicked <= 50ms, "báo đã hủy sau hơn 50 ms");

        // Hủy từ nơi khác (đóng document), việc chưa dừng ngay: vẫn hiện "Đang hủy…".
        const auto stubborn = gate();
        stubborn->honorCancel = false;
        const JobId other = startGated(*jobs_, u"Đang lưu"_s, stubborn, 9);
        item->setJob(other);
        QVERIFY(spinUntil([&] { return item->isVisible(); }));
        QCOMPARE(cancel->text(), InlineProgress::tr("Hủy"));
        QVERIFY(cancel->isEnabled());
        QCOMPARE(jobs_->cancelOwner(9), 1);
        QVERIFY(spinUntil([&] { return cancel->text() == InlineProgress::tr("Đang hủy…"); }));
        QVERIFY(presenter_->cancelling(other));
        QVERIFY(presenter_->model()->index(0).data(ProgressItemModel::CancellingRole).toBool());
        QVERIFY(cancelling.count() > 0);
        stubborn->release = true;
        QVERIFY(spinUntil([&] { return !item->isVisible(); }));
    }

    // Tiêu chí 3.
    void throttle()
    {
        use({20ms, 50ms, 100ms});
        auto* item = new InlineProgress(*presenter_, 0, host_.get());
        item->resize(kSheetW, item->sizeHint().height());
        Watcher bar(item->findChild<QProgressBar*>());
        QObject scope;
        std::vector<Clock::time_point> applies;
        connect(presenter_.get(), &ProgressPresenter::changed, &scope, [&applies] { applies.push_back(Clock::now()); });
        std::atomic<long> reports{0};
        const auto run = 1200ms;
        const JobId id = jobs_->start(u"Đang nạp"_s, u"open"_s, 0,
                                      [&reports, run](jobs::Progress& p, sched::CancelToken) -> Expected<void> {
                                          const auto start = Clock::now();
                                          for (auto tick = start; tick - start < run; tick += 100us) {
                                              while (Clock::now() < tick) { // 10 kHz đều
                                              }
                                              p.set(ms(Clock::now() - start) / ms(run));
                                              reports.fetch_add(1, std::memory_order_relaxed);
                                          }
                                          return {};
                                      });
        QVERIFY(spinUntil([&] { return !jobs_->info(id) && !presenter_->job(id) && !item->isVisible(); }));
        const double seconds = ms(run) / 1000;
        const auto frame = frameTime();
        auto minGap = Clock::duration::max();
        for (std::size_t i = 1; i < applies.size(); ++i)
            minGap = std::min(minGap, applies[i] - applies[i - 1]);
        qInfo("%ld lần báo trong %.1f s (%.0f lần/s): %zu lần áp UI, cách nhau ít nhất %.2f ms (1 khung %.2f ms), %d "
              "lần vẽ thanh",
              reports.load(), seconds, double(reports.load()) / seconds, applies.size(), ms(minGap), ms(frame),
              bar.paints);
        QVERIFY(reports.load() >= 1000); // việc thật sự báo dồn dập (máy tải nặng vẫn ≫ 10 lần/khung)
        QVERIFY(minGap >= frame);        // tối đa 1 lần mỗi khung, không phụ thuộc tải
        // UI chỉ áp khi có lô của NT-013 (≤ 10 lô/s) hoặc ở mốc 20/50 ms.
        QVERIFY(applies.size() <= jobs_->stats().batches + 2);
        QVERIFY(jobs_->stats().batches <= std::uint64_t(seconds * 10) + 4); // lô đầu, lô cuối, việc chạy quá 1,2 s
        QVERIFY(bar.paints <= int(applies.size()) + 3); // mỗi lần áp ≤ 1 lần vẽ, thêm lần hiện

        // Rảnh: không áp, không vẽ, không timer.
        const auto appliesBefore = applies.size();
        const int paintsBefore = bar.paints;
        spinFor(300ms);
        QCOMPARE(applies.size(), appliesBefore);
        QCOMPARE(bar.paints, paintsBefore);
        for (const QObject* owner :
             {static_cast<const QObject*>(presenter_.get()), static_cast<const QObject*>(jobs_.get())})
            for (const QTimer* t : owner->findChildren<QTimer*>())
                QVERIFY(!t->isActive());
    }

    // Tiêu chí 4.
    void multiple()
    {
        use({20ms, 50ms, 20ms});
        auto* item = new InlineProgress(*presenter_, 0, host_.get());
        item->resize(kSheetW + 40, item->sizeHint().height());
        auto* text = item->findChild<QLabel*>();
        auto* bar = item->findChild<QProgressBar*>();
        const auto a = gate(), b = gate(), c = gate();
        const JobId ida = startGated(*jobs_, u"Đang mở"_s, a);
        const JobId idb = startGated(*jobs_, u"Đang in"_s, b);
        const JobId idc = startGated(*jobs_, u"Chuyển font"_s, c);
        QVERIFY(spinUntil([&] { return presenter_->shownCount() == 3; }));
        QCOMPARE(presenter_->primary(), ida);
        auto* model = presenter_->model();
        QCOMPARE(model->rowCount(), 3);
        QCOMPARE(JobId(model->index(0).data(ProgressItemModel::JobIdRole).toULongLong()), ida);
        QCOMPARE(JobId(model->index(1).data(ProgressItemModel::JobIdRole).toULongLong()), idb);
        QCOMPARE(JobId(model->index(2).data(ProgressItemModel::JobIdRole).toULongLong()), idc);
        const QString two =
            InlineProgress::tr("%1 · %n tác vụ khác", nullptr, 2).arg(ProgressPresenter::label(u"Đang mở"_s, 0));
        QVERIFY(spinUntil([&] { return text->text() == two; }));

        // Trình đọc màn hình: chỉ bước 10%.
        g_a11y.clear();
        QAccessible::installUpdateHandler(recordA11y);
        QAccessible::setActive(true);
        const double steps[] = {0.03, 0.07, 0.12, 0.18, 0.25, 0.31, 0.47, 0.52, 0.55, 0.69, 0.71, 0.88, 0.93, 1.0};
        for (double v : steps) {
            a->progress = v;
            const QString want = InlineProgress::tr("%1 · %n tác vụ khác", nullptr, 2)
                                     .arg(ProgressPresenter::label(u"Đang mở"_s, ProgressPresenter::percent(v)));
            QVERIFY2(spinUntil([&] { return text->text() == want; }), qPrintable(want));
        }
        QAccessible::installUpdateHandler(nullptr);
        QAccessible::setActive(false);
        QList<int> values;
        QStringList names;
        for (const A11yEvent& e : g_a11y) {
            if (e.object == bar && e.type == QAccessible::ValueChanged)
                values.push_back(e.value);
            if (e.object == text && e.type == QAccessible::NameChanged)
                names << e.name;
        }
        QCOMPARE(values, (QList<int>{10, 20, 30, 40, 50, 60, 70, 80, 90, 100}));
        QCOMPARE(names.size(), 10);
        QCOMPARE(
            names.back(),
            InlineProgress::tr("%1 · %n tác vụ khác", nullptr, 2).arg(ProgressPresenter::label(u"Đang mở"_s, 100)));

        // Tác vụ chính xong: việc kế tiếp lên thay, còn 1 tác vụ khác.
        a->release = true;
        const QString one =
            InlineProgress::tr("%1 · %n tác vụ khác", nullptr, 1).arg(ProgressPresenter::label(u"Đang in"_s, 0));
        QVERIFY(spinUntil([&] { return text->text() == one; }));
        QCOMPARE(presenter_->primary(), idb);
        QCOMPARE(model->rowCount(), 2);
        b->release = true;
        QVERIFY(spinUntil([&] { return text->text() == ProgressPresenter::label(u"Chuyển font"_s, 0); }));
        QCOMPARE(bar->accessibleName(), u"Chuyển font"_s);
    }

    // DoD-U: ảnh InlineProgress (một việc 63%, đang hủy, tác vụ chính kèm 2 tác vụ khác).
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
        use({1ms, 2ms, 1ms});
        const auto open = gate(), print = gate(), pdf = gate();
        open->progress = 0.63;
        print->progress = 0.25;
        print->honorCancel = false; // giữ trạng thái "Đang hủy…"
        pdf->progress = 0.5;
        const JobId ido = startGated(*jobs_, u"Đang mở"_s, open);
        const JobId idp = startGated(*jobs_, u"Đang in"_s, print);
        startGated(*jobs_, u"Xuất PDF"_s, pdf);
        QVERIFY(spinUntil([&] {
            return presenter_->shownCount() == 3 && ProgressPresenter::percent(presenter_->job(ido)->progress) == 63;
        }));
        presenter_->cancel(idp);
        const QList<InlineProgress*> rows = {new InlineProgress(*presenter_, ido, host_.get()),
                                             new InlineProgress(*presenter_, idp, host_.get()),
                                             new InlineProgress(*presenter_, 0, host_.get())};
        QVERIFY(spinUntil([&] {
            return std::all_of(rows.begin(), rows.end(), [](InlineProgress* w) { return w->isVisible(); }) &&
                   presenter_->cancelling(idp);
        }));
        int height = 0;
        for (InlineProgress* w : rows) {
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            w->resize(kSheetW, w->sizeHint().height());
            height += w->height();
        }
        const QColor bg = tm_->color(tok::c_base);
        QImage sheet(QSize(kSheetW, height) * dpr, QImage::Format_ARGB32_Premultiplied);
        sheet.setDevicePixelRatio(dpr);
        sheet.fill(bg);
        QPainter p(&sheet);
        int y = 0;
        for (InlineProgress* w : rows) {
            w->render(&p, QPoint(0, y), {}, QWidget::DrawChildren);
            y += w->height();
        }
        p.end();
        const QString name = u"inline-%1-%2-%3.png"_s.arg(light ? u"light"_s : u"dark"_s)
                                 .arg(qRound(dpr * 100))
                                 .arg(QLatin1StringView(kPlatform));
        const QString out = qEnvironmentVariable("TRION_UPDATE_BASELINE");
        if (!out.isEmpty()) {
            QVERIFY(sheet.save(out + u'/' + name, "PNG", 0));
            QSKIP("đã ghi baseline");
        }
        const QImage want = QImage(u":/baseline/"_s + name).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QVERIFY2(!want.isNull(), qPrintable(u"thiếu baseline "_s + name));
        QCOMPARE(sheet.size(), want.size());
        qsizetype off = 0;
        int maxDiff = 0;
        for (int yy = 0; yy < sheet.height(); ++yy) {
            const auto* a = reinterpret_cast<const QRgb*>(sheet.constScanLine(yy));
            const auto* b = reinterpret_cast<const QRgb*>(want.constScanLine(yy));
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

    // DoD-U: chữ dài hơn 40% (vi-XL) không bị cắt, cả gọn và thoáng.
    void pseudoNoTruncation()
    {
        use({1ms, 2ms, 1ms});
        const auto one = gate(), other = gate();
        one->progress = 1.0;
        one->honorCancel = false; // bấm hủy rồi vẫn hiện "Đang hủy…" để đo
        const JobId id = startGated(*jobs_, u"Đang mở"_s, one);
        startGated(*jobs_, u"Đang in"_s, other);
        QWidget footer(host_.get());
        auto* rows = new QVBoxLayout(&footer);
        auto* single = new InlineProgress(*presenter_, id, &footer);
        auto* main = new InlineProgress(*presenter_, 0, &footer);
        rows->addWidget(single);
        rows->addWidget(main);
        footer.resize(kSheetW + 40, 120);
        footer.show();
        QVERIFY(spinUntil([&] { return single->isVisible() && main->isVisible(); }));
        const auto report = [&footer] {
            QStringList out;
            for (const i18n::Truncation& t : i18n::findTruncations(&footer))
                out << t.toString();
            return out.join(u"; "_s);
        };
        QTRY_COMPARE(report(), QString());
        auto* cancel = single->findChild<Button*>();
        const QString vi = cancel->text();
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QTRY_VERIFY(cancel->text().size() >= vi.size() + 2);
        QTRY_COMPARE(report(), QString());
        presenter_->cancel(id); // "Đang hủy…" dài hơn "Hủy"
        for (Density d : {Density::Comfortable, Density::Compact}) {
            tm_->setDensity(d);
            QTRY_COMPARE(report(), QString());
        }
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QTRY_COMPARE(single->findChild<QLabel*>()->text(), ProgressPresenter::label(u"Đang mở"_s, 100));
    }

    void presenterApi()
    {
        QCOMPARE(ProgressPresenter::percent(0.6299999999), 63);
        QCOMPARE(ProgressPresenter::percent(0.639), 63);
        QCOMPARE(ProgressPresenter::percent(-1), 0);
        QCOMPARE(ProgressPresenter::percent(std::numeric_limits<double>::quiet_NaN()), 0);
        QCOMPARE(ProgressPresenter::percent(7), 100);
        QCOMPARE(ProgressPresenter::label(u"Đang mở"_s, 63), u"Đang mở · 63%"_s);

        use({1ms, 2ms, 1ms});
        QObject scope;
        std::vector<JobInfo> finished;
        connect(presenter_.get(), &ProgressPresenter::finished, &scope,
                [&finished](const JobInfo& i) { finished.push_back(i); });
        const auto g = gate();
        g->progress = 0.4;
        const JobId id = startGated(*jobs_, u"Đang lưu"_s, g, 5);
        const JobId bad =
            jobs_->start(u"Lỗi"_s, u"open"_s, 0, [](jobs::Progress&, sched::CancelToken) -> Expected<void> {
                return trion::Error{"dwg", "corrupt", "hỏng"}; // i18n-ok: dữ liệu thử
            });
        QVERIFY(spinUntil([&] { return presenter_->shown(id) && finished.size() == 1; }));
        QCOMPARE(finished[0].id, bad);
        QCOMPARE(finished[0].state, State::Failed);
        QCOMPARE(finished[0].error.code, std::string("corrupt"));
        QVERIFY(!presenter_->job(bad));
        QVERIFY(!presenter_->shown(bad));
        QVERIFY(!presenter_->cancelling(bad));
        presenter_->cancel(bad); // không theo dõi: bỏ qua

        auto* model = presenter_->model();
        QVERIFY(spinUntil([&] { return model->index(0).data(ProgressItemModel::PercentRole).toInt() == 40; }));
        const QModelIndex row = model->index(0);
        QCOMPARE(row.data().toString(), ProgressPresenter::label(u"Đang lưu"_s, 40));
        QCOMPARE(row.data(ProgressItemModel::NameRole).toString(), u"Đang lưu"_s);
        QCOMPARE(row.data(ProgressItemModel::KindRole).toString(), u"test"_s);
        QCOMPARE(JobId(row.data(ProgressItemModel::JobIdRole).toULongLong()), id);
        QVERIFY(!row.data(ProgressItemModel::CancellingRole).toBool());
        QVERIFY(!row.data(Qt::DecorationRole).isValid());
        QCOMPARE(model->rowCount(row), 0);
        QVERIFY(!model->index(5).data().isValid());
        QCOMPARE(presenter_->job(id)->owner, std::uint64_t(5));

        // Hủy presenter khi đang bận: trả con trỏ.
        QVERIFY(presenter_->busy());
        QVERIFY(QGuiApplication::overrideCursor());
        presenter_.reset();
        QCOMPARE(QGuiApplication::overrideCursor(), nullptr);
    }
};

QTEST_MAIN(ProgressTest)
#include "progress_test.moc"
