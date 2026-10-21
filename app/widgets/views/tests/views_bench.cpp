// UI0-108 · Benchmark view ảo hóa, đo qua UiProbe (UI0-105). Cỡ đầy đủ ở release không sanitizer (trion::test::kTimed):
// 10⁶ dòng, sắp 10⁵ dòng; bản debug, sanitizer: 10⁵ dòng, sắp 2·10⁴ dòng, chỉ in số đo thời gian. Máy dùng chung: mỗi
// đại lượng lấy vòng tốt nhất trong kRounds vòng; "bận" của luồng UI xét theo thời gian CPU của chính nó (Busy, Linux),
// watchdog UiProbe (thời gian thật, gồm lúc máy cho luồng khác chạy) chỉ báo kèm nơi xảy ra.
//   viewMemory: heap view cấp phát khi mở trên model 10⁴ và 10⁶ dòng (debug: 10³ và 10⁵), cùng loại model: hiện, cuộn
//      giữa, cuối, chọn tất cả, đổi dòng hiện hành. VirtualTableView, VirtualTreeView: chênh ≤ 64 KB (tiêu chí 1);
//      QTreeView, QTableView chỉ in số đo (so sánh khi chọn).
//   millionRows: (1) mở view trên model đã có 10⁶ dòng đến khung đầu ≤ 150 ms; model đang dựng: từ lúc dòng vào model
//      đến khung đầu ≤ 150 ms (cả quãng từ refresh() chỉ báo: phần lớn là việc nền của BackgroundModel); (3) cuộn bánh
//      xe: CPU luồng UI mỗi khung ≤ 16,6 ms (60 fps); scroll_frame p50/p99 thời gian thật chỉ báo.
//   sortHundredThousand: bấm đầu cột, model sắp ở nền; luồng UI không bận > 16 ms CPU liên tục, lô áp dài nhất ≤ 16 ms;
//      giữ chọn và dòng hiện hành (tiêu chí 2).
//   typeToJumpMillion: gõ chữ khớp dòng cuối của 10⁶ dòng: quét theo lát, luồng UI không bận > 16 ms CPU liên tục.
#include "app/widgets/views/tests/SampleModels.h"
#include "app/widgets/views/VirtualTableView.h"
#include "app/widgets/views/VirtualTreeView.h"

#include "app/core/perf/UiProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"
#include "tests/timing.h"

#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QElapsedTimer>
#include <QHeaderView>
#include <QJsonObject>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeView>
#include <QWheelEvent>

#include <cstdlib>
#include <ctime>
#include <functional>
#include <memory>
#include <optional>
#include <set>

#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#define VIEWS_SANITIZER_HEAP 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#define VIEWS_SANITIZER_HEAP 1
#endif
#endif
#if defined(VIEWS_SANITIZER_HEAP)
#include <sanitizer/allocator_interface.h>
#elif defined(__GLIBC__)
#include <malloc.h>
#endif

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;
namespace probe = trion::ui::probe;
using trion::test::kTimed;
using trion::ui::VirtualTableView;
using trion::ui::VirtualTreeView;

namespace {

constexpr int kRounds = kTimed ? 3 : 1;
constexpr int kMillion = kTimed ? 1'000'000 : 100'000;
constexpr int kSmall = kTimed ? 10'000 : 1'000;
constexpr int kSortRows = kTimed ? 100'000 : 20'000;
constexpr int kWheel = kTimed ? 120 : 30;

// Byte heap đang cấp phát của cả tiến trình; -1 nếu nền tảng không đo được.
std::int64_t heapBytes()
{
#if defined(VIEWS_SANITIZER_HEAP)
    return std::int64_t(__sanitizer_get_current_allocated_bytes());
#elif defined(__GLIBC__)
    const struct mallinfo2 m = mallinfo2();
    return std::int64_t(m.uordblks + m.hblkhd);
#else
    return -1; // ponytail: Windows, macOS chưa đo (heap của CRT/zone); mã view không theo nền tảng, Linux đủ đại diện
#endif
}

bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 30s)
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

double ms(Clock::duration d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

QJsonObject probeOp(const char* name)
{
    return probe::toJson()["ops"].toObject()[name].toObject();
}

// Khựng dài nhất theo watchdog UiProbe (thời gian thật) và nơi xảy ra.
std::pair<double, std::string> longestStall()
{
    std::pair<double, std::string> worst{0, {}};
    for (const auto& s : probe::stalls())
        if (ms(s.duration) > worst.first)
            worst = {ms(s.duration), s.where};
    return worst;
}

// Thời gian CPU của luồng gọi. Linux: CLOCK_THREAD_CPUTIME_ID; nơi khác chưa có (GetThreadTimes của Windows nhảy theo
// nhịp 15,6 ms): -1.
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

// Lượt vòng sự kiện bận nhất của luồng UI tính bằng thời gian CPU của chính nó (giữa hai lần aboutToBlock; lúc ngủ chờ
// không tốn CPU). Watchdog UiProbe đo thời gian thật nên tính cả lúc hệ điều hành cho luồng khác chạy: trên máy dùng
// chung tải cao, khựng đó là của máy, không phải của việc ta làm. Số này không phụ thuộc tải.
struct Busy : QObject {
    std::chrono::nanoseconds last = threadCpu(), longest{0};
    Busy()
    {
        connect(QAbstractEventDispatcher::instance(), &QAbstractEventDispatcher::aboutToBlock, this, [this] {
            const auto now = threadCpu();
            longest = std::max(longest, now - last);
            last = now;
        });
    }
    static bool supported() { return threadCpu().count() >= 0; }
};

// Số đo CPU để in: nan nếu nền tảng chưa đo được.
double cpuOr(double ms)
{
    return Busy::supported() ? ms : qQNaN();
}

// Khung = UpdateRequest tới cửa sổ; chờ khung kế tiếp.
struct Frames : QObject {
    int count = 0;
    bool eventFilter(QObject*, QEvent* e) override
    {
        count += e->type() == QEvent::UpdateRequest;
        return false;
    }
    bool next()
    {
        const int c = count;
        return spinUntil([&] { return count > c; }, 5s);
    }
};

// Một nấc bánh xe giữa viewport, gửi tới QWindow như từ hệ thống (UiProbe đo scroll_frame).
void wheel(QAbstractItemView* view, int delta)
{
    QWidget* top = view->window();
    const QPointF pos = view->viewport()->mapTo(top, view->viewport()->rect().center());
    QWheelEvent e(pos, top->mapToGlobal(pos), QPoint(), QPoint(0, delta), Qt::NoButton, Qt::NoModifier,
                  Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(top->windowHandle(), &e);
}

// Cây n dòng, mọi nhóm mở: 10³ nhóm gốc (10² ở cỡ nhỏ), mỗi nhóm n/nhóm − 1 lá.
std::pair<std::shared_ptr<const sample::Nodes>, std::set<std::uint64_t>> openTree(int rows)
{
    const int groups = rows >= 100'000 ? 1'000 : 100;
    auto nodes = sample::makeTree(groups, 0, rows / groups - 1);
    std::set<std::uint64_t> open;
    for (const sample::Node& n : *nodes)
        if (n.parent)
            open.insert(n.key);
    return {nodes, open};
}

} // namespace

class ViewsBench : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<trion::settings::Store> store_;
    std::unique_ptr<trion::ui::UiExecutor> executor_;
    std::unique_ptr<trion::theme::ThemeManager> tm_;
    std::unique_ptr<trion::theme::IconProvider> icons_;

    static bool settle(const trion::ui::BackgroundModelBase& m)
    {
        return spinUntil([&] { return !m.busy(); }, 60s);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<trion::settings::Store>(dir_.filePath(u"user.json"_s));
        trion::settings::Store::setInstance(store_.get());
        QVERIFY(trion::theme::loadFonts());
        executor_ = std::make_unique<trion::ui::UiExecutor>();
        tm_ = std::make_unique<trion::theme::ThemeManager>(*store_);
        icons_ = std::make_unique<trion::theme::IconProvider>();
        probe::enable();
        probe::setStallThreshold(16ms);
    }
    void cleanupTestCase()
    {
        probe::disable();
        icons_.reset();
        tm_.reset();
        trion::settings::Store::setInstance(nullptr);
        store_.reset();
        trion::sched::waitIdle();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    void viewMemory_data()
    {
        QTest::addColumn<int>("kind");
        QTest::addRow("VirtualTableView") << 0;
        QTest::addRow("VirtualTreeView") << 1;
        QTest::addRow("QTreeView") << 2;
        QTest::addRow("QTableView") << 3;
    }
    void viewMemory()
    {
        QFETCH(int, kind);
        if (heapBytes() < 0)
            QSKIP("chưa đo heap trên nền tảng này (glibc, ASan, TSan)");
        const auto make = [kind]() -> QAbstractItemView* {
            switch (kind) {
            case 0:
                return new VirtualTableView;
            case 1:
                return new VirtualTreeView;
            case 2: {
                auto* t = new QTreeView;
                t->setUniformRowHeights(true);
                t->setRootIsDecorated(false);
                return t;
            }
            default:
                return new QTableView;
            }
        };
        // Model cùng loại, n dòng; dựng xong, việc nền (hủy bảng cũ) xong rồi mới đo.
        const auto model = [kind](int rows) -> std::unique_ptr<trion::ui::BackgroundModelBase> {
            if (kind == 1) {
                auto m = std::make_unique<sample::TreeModel>();
                auto [nodes, open] = openTree(rows);
                m->load(nodes, open);
                return m;
            }
            auto m = std::make_unique<sample::TableModel>();
            m->load(sample::makeItems(rows));
            return m;
        };
        // Δheap từ trước khi tạo view đến khi view đã hiện, cuộn giữa, cuối, chọn tất cả, đổi dòng hiện hành.
        const auto measure = [&](trion::ui::BackgroundModelBase& m) -> std::optional<std::int64_t> {
            trion::sched::waitIdle();
            QCoreApplication::processEvents();
            const std::int64_t before = heapBytes();
            std::int64_t delta = 0;
            {
                std::unique_ptr<QAbstractItemView> view(make());
                view->setSelectionMode(QAbstractItemView::ExtendedSelection);
                view->setSelectionBehavior(QAbstractItemView::SelectRows);
                view->setModel(&m);
                view->resize(800, 600);
                Frames frames;
                view->installEventFilter(&frames);
                view->show();
                if (!QTest::qWaitForWindowExposed(view.get()))
                    return std::nullopt;
                const int rows = m.rowCount();
                view->scrollTo(m.index(rows / 2, 0));
                frames.next();
                view->scrollTo(m.index(rows - 1, 0));
                frames.next();
                view->selectAll();
                view->setCurrentIndex(m.index(rows - 2, 0));
                frames.next();
                trion::sched::waitIdle();
                QCoreApplication::processEvents();
                delta = heapBytes() - before;
            }
            trion::sched::waitIdle();
            QCoreApplication::processEvents();
            return delta;
        };
        const auto small = model(kSmall);
        const auto big = model(kMillion);
        QVERIFY(settle(*small) && settle(*big));
        measure(*small); // làm nóng: cache style, chữ, pixmap của lần hiện đầu
        const auto ma = measure(*small);
        const auto mb = measure(*big);
        QVERIFY(ma && mb);
        const std::int64_t a = *ma, b = *mb;
        qInfo("%s: heap view %d dòng %.1f KB, %d dòng %.1f KB, chênh %.1f KB (%.2f byte/dòng)", QTest::currentDataTag(),
              kSmall, double(a) / 1024, kMillion, double(b) / 1024, double(b - a) / 1024,
              double(b - a) / (kMillion - kSmall));
        if (kind <= 1)
            QVERIFY2(b - a <= 64 * 1024, "(1) bộ nhớ của view tăng theo số dòng");
    }

    void millionRows_data()
    {
        QTest::addColumn<bool>("tree");
        QTest::addRow("table") << false;
        QTest::addRow("tree") << true;
    }
    void millionRows()
    {
        QFETCH(bool, tree);
        double open = 1e9, first = 1e9, model = 1e9, sync = 1e9, scrollP50 = 1e9, scrollP99 = 1e9, scrollMax = 1e9;
        double frameCpu = 1e9, frameCpuMax = 1e9;
        const auto items = sample::makeItems(kMillion);
        const auto [nodes, opened] = openTree(kMillion);
        for (int round = 0; round < kRounds; ++round) {
            sample::TableModel table;
            sample::TreeModel treeModel;
            trion::ui::BackgroundModelBase& m = tree ? static_cast<trion::ui::BackgroundModelBase&>(treeModel)
                                                     : static_cast<trion::ui::BackgroundModelBase&>(table);
            const auto load = [&] {
                if (tree)
                    treeModel.load(nodes, opened);
                else
                    table.load(items);
            };
            const auto make = [tree] {
                return std::unique_ptr<VirtualTableView>(tree ? new VirtualTreeView : new VirtualTableView);
            };

            // (1a) Khung đầu khi model đang dựng: từ refresh() đến hết khung kế sau khi dòng vào model.
            {
                auto view = make();
                view->setModel(&m);
                view->resize(800, 600);
                view->show();
                QVERIFY(QTest::qWaitForWindowExposed(view.get()));
                probe::reset();
                const auto t0 = Clock::now();
                load();
                const auto call = Clock::now() - t0;
                Clock::time_point inserted{};
                connect(
                    &m, &QAbstractItemModel::rowsInserted, view.get(),
                    [&] {
                        inserted = Clock::now();
                        probe::untilFrame(probe::Op::PaletteOpen, view.get(), t0);
                    },
                    Qt::SingleShotConnection);
                QVERIFY(spinUntil([&] { return !m.busy() && probeOp("palette_open")["count"].toInt() > 0; }));
                QCOMPARE(m.rowCount(), kMillion);
                first = std::min(first, probeOp("palette_open")["max_ms"].toDouble());
                model = std::min(model, ms(inserted - t0)); // dựng ở nền + diff + chèn (BackgroundModel)
                sync = std::min(sync, std::max(ms(call), ms(m.stats().longestBatch)));
            }
            // (1b) Mở view trên model đã có 10⁶ dòng: show() đến hết khung đầu.
            auto view = make();
            view->setModel(&m);
            view->resize(800, 600);
            Frames frames;
            view->installEventFilter(&frames);
            probe::reset();
            const auto t0 = Clock::now();
            view->show();
            probe::untilFrame(probe::Op::PaletteOpen, view.get(), t0);
            QVERIFY(spinUntil([&] { return probeOp("palette_open")["count"].toInt() > 0; }));
            open = std::min(open, probeOp("palette_open")["max_ms"].toDouble());

            // (3) Cuộn: kWheel nấc bánh xe xuống từ giữa danh sách, mỗi nấc chờ khung của nó.
            view->scrollTo(m.index(kMillion / 2, 0), QAbstractItemView::PositionAtTop);
            QVERIFY(frames.next());
            probe::reset();
            const auto cpu0 = threadCpu();
            std::optional<Busy> busy(std::in_place);
            for (int k = 0; k < kWheel; ++k) {
                wheel(view.get(), -120);
                QVERIFY(frames.next());
            }
            frameCpu = std::min(frameCpu, ms(threadCpu() - cpu0) / kWheel);
            frameCpuMax = std::min(frameCpuMax, ms(busy->longest));
            busy.reset();
            const QJsonObject sf = probeOp("scroll_frame");
            QCOMPARE(sf["count"].toInt(), kWheel);
            QVERIFY(view->indexAt({0, 0}).row() > kMillion / 2);
            scrollP50 = std::min(scrollP50, sf["p50_ms"].toDouble());
            scrollP99 = std::min(scrollP99, sf["p99_ms"].toDouble());
            scrollMax = std::min(scrollMax, sf["max_ms"].toDouble());
            view.reset();
            trion::sched::waitIdle();
        }
        qInfo("%s %d dòng, tốt nhất %d vòng: (1) mở view trên model có sẵn %.1f ms; từ refresh() đến khung có nội dung "
              "%.1f ms, trong đó model dựng + diff + chèn %.1f ms (view: phần còn lại), đồng bộ dài nhất %.2f ms; (3) "
              "khung "
              "cuộn p50 %.2f ms, p99 %.2f ms (%.0f fps), tối đa %.2f ms (thời gian thật); CPU luồng UI mỗi khung %.2f "
              "ms, "
              "lượt bận nhất %.2f ms",
              tree ? "VirtualTreeView" : "VirtualTableView", kMillion, kRounds, open, first, model, sync, scrollP50,
              scrollP99, 1000.0 / scrollP99, scrollMax, cpuOr(frameCpu), cpuOr(frameCpuMax));
        // ponytail: p99 thời gian thật chỉ báo (máy dùng chung, tải cao); 60 fps xét theo CPU luồng UI mỗi khung. Khung
        // đầu tính từ refresh() chủ yếu là việc nền của BackgroundModel (UI0-014): chỉ báo; phần của view xét ở đây.
        if constexpr (kTimed) {
            QVERIFY2(open <= 150.0, "(1) mở view > 150 ms");
            QVERIFY2(first - model <= 150.0, "(1) từ lúc dòng vào model đến khung đầu > 150 ms");
            if (Busy::supported())
                QVERIFY2(frameCpuMax <= 16.6, "(1) một khung cuộn tốn > 16,6 ms CPU luồng UI");
        }
    }

    // Vẽ lại cả vùng nhìn 800×600 (3 cột): ViewItemDelegate (đường vẽ chữ nhanh) so với QStyledItemDelegate, xen kẽ,
    // CPU tốt nhất mỗi bên.
    void repaintCost()
    {
        sample::TableModel model;
        model.load(sample::makeItems(1'000));
        VirtualTableView view;
        view.setModel(&model);
        view.resize(800, 600);
        view.show();
        QVERIFY(settle(model) && QTest::qWaitForWindowExposed(&view));
        QAbstractItemDelegate* fast = view.itemDelegate();
        auto* slow = new QStyledItemDelegate(&view);
        const auto paint = [&] {
            const auto start = Clock::now();
            const auto cpu = threadCpu();
            view.viewport()->repaint();
            return Busy::supported() ? ms(threadCpu() - cpu) : ms(Clock::now() - start);
        };
        double best[2] = {1e9, 1e9};
        for (int k = 0; k < 20; ++k) {
            view.setItemDelegate(k % 2 ? slow : fast);
            best[k % 2] = std::min(best[k % 2], paint());
        }
        view.setItemDelegate(fast);
        qInfo("vẽ lại cả vùng nhìn (%d dòng × 3 cột), tốt nhất 10 lần: ViewItemDelegate %.2f ms, QStyledItemDelegate "
              "%.2f ms (%.1f lần)",
              view.viewport()->height() / view.rowHeight(), best[0], best[1], best[1] / best[0]);
        if constexpr (kTimed)
            QVERIFY2(best[0] < best[1], "đường vẽ chữ nhanh không nhanh hơn");
    }

    void sortHundredThousand()
    {
        double settled = 1e9, batch = 1e9, stall = 1e9, cpu = 1e9;
        std::string where;
        const auto items = sample::makeItems(kSortRows);
        for (int round = 0; round < kRounds; ++round) {
            sample::TableModel model;
            model.load(items);
            VirtualTableView view;
            view.setModel(&model);
            view.setSortingEnabled(true);
            view.resize(800, 600);
            view.show();
            QVERIFY(settle(model) && QTest::qWaitForWindowExposed(&view));
            for (int r = 0; r < kSortRows; r += 997)
                view.selectionModel()->select(model.index(r, 0),
                                              QItemSelectionModel::Select | QItemSelectionModel::Rows);
            view.selectionModel()->setCurrentIndex(model.index(kSortRows / 2, 0), QItemSelectionModel::NoUpdate);
            std::set<std::uint64_t> keys;
            for (const QModelIndex& i : view.selectionModel()->selectedRows())
                keys.insert(model.row(i.row()).key);
            const std::uint64_t current = model.row(view.currentIndex().row()).key;

            probe::reset();
            const auto before = model.stats();
            const auto t0 = Clock::now();
            std::optional<Busy> busy(std::in_place);
            QTest::mouseClick(view.header()->viewport(), Qt::LeftButton, {},
                              QPoint(view.header()->sectionSize(0) / 2, view.header()->height() / 2));
            QVERIFY(model.busy());
            QVERIFY(settle(model));
            cpu = std::min(cpu, ms(busy->longest));
            busy.reset();
            settled = std::min(settled, ms(Clock::now() - t0));
            batch = std::min(batch, ms(model.stats().longestBatch));
            if (const auto [worst, at] = longestStall(); worst < stall) {
                stall = worst;
                where = at;
            }
            QCOMPARE(model.stats().resets, before.resets);
            for (int r = 1; r < kSortRows; ++r)
                QVERIFY(!sample::TableModel::before(model.row(r), model.row(r - 1), 0, Qt::AscendingOrder));
            std::set<std::uint64_t> after;
            for (const QModelIndex& i : view.selectionModel()->selectedRows())
                after.insert(model.row(i.row()).key);
            QCOMPARE(after, keys);
            QCOMPARE(model.row(view.currentIndex().row()).key, current);
        }
        qInfo("sắp %d dòng ở nền, tốt nhất %d vòng: bấm đến xong %.1f ms; lô áp dài nhất %.2f ms; lượt bận nhất của "
              "luồng "
              "UI %.2f ms CPU; watchdog (thời gian thật): khựng ≥ 16 ms dài nhất %.1f ms %s",
              kSortRows, kRounds, settled, batch, cpuOr(cpu), stall, where.c_str());
        if constexpr (kTimed) {
            if (Busy::supported())
                QVERIFY2(cpu <= 16.0, "(2) luồng UI bận > 16 ms CPU liên tục khi sắp");
            QVERIFY2(batch <= 16.0, "(2) lô áp > 16 ms");
        }
    }

    void typeToJumpMillion()
    {
        double found = 1e9, stall = 1e9, cpu = 1e9;
        std::string where;
        auto items = std::make_shared<sample::Items>(*sample::makeItems(kMillion));
        items->back().name = u"Zeta"_s; // i18n-ok: dữ liệu thử; dòng duy nhất bắt đầu bằng "z"
        for (int round = 0; round < kRounds; ++round) {
            sample::TableModel model;
            model.load(items);
            VirtualTableView view;
            view.setModel(&model);
            view.resize(800, 600);
            view.show();
            view.activateWindow();
            QVERIFY(settle(model) && QTest::qWaitForWindowActive(&view));
            view.setCurrentIndex(model.index(0, 0));
            probe::reset();
            const auto t0 = Clock::now();
            std::optional<Busy> busy(std::in_place);
            QTest::keyClick(&view, 'z');
            QVERIFY(spinUntil([&] { return view.currentIndex().row() == kMillion - 1; }, 60s));
            cpu = std::min(cpu, ms(busy->longest));
            busy.reset();
            found = std::min(found, ms(Clock::now() - t0));
            if (const auto [worst, at] = longestStall(); worst < stall) {
                stall = worst;
                where = at;
            }
        }
        qInfo("gõ để nhảy, khớp dòng cuối của %d dòng, tốt nhất %d vòng: %.0f ms; lượt bận nhất của luồng UI %.2f ms "
              "CPU; "
              "watchdog (thời gian thật): khựng ≥ 16 ms dài nhất %.1f ms %s",
              kMillion, kRounds, found, cpuOr(cpu), stall, where.c_str());
        if constexpr (kTimed)
            if (Busy::supported())
                QVERIFY2(cpu <= 16.0, "gõ để nhảy: luồng UI bận > 16 ms CPU liên tục");
    }
};

QTEST_MAIN(ViewsBench)
#include "views_bench.moc"
