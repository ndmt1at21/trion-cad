// UI0-014 · Benchmark danh sách 100.000 dòng (QListView, QTreeView offscreen, dòng cao đều), đo qua UiProbe (UI0-105).
//  (1) Khung đầu ≤ 150 ms tính từ refresh(); phần đồng bộ trên luồng UI ≤ 16 ms (lời gọi refresh, lô áp dài nhất).
//  (2) 10.000 thay đổi cùng lúc (xóa, chèn, đổi, dời mỗi loại 2.500): lô áp của model ≤ 4 ms (đo không view); chọn
//      và vị trí cuộn giữ với dòng còn tồn tại.
//  (3) Cuộn bằng bánh xe: scroll_frame (UiProbe). Máy thấp (30 fps) đo ở QA-011 trên máy tham chiếu.
// ponytail: chỉ xét ngưỡng phần của model. Việc view Qt tự làm chỉ in số đo: QListView/QTreeView bố trí lại O(n) sau
// mỗi lô có chèn/xóa (ngay trong tín hiệu và ở khung sau: áp mỗi khung có view, khung có lô ≤ 16,6 ms, khựng ≥ 16 ms),
// bố trí 100.000 dòng lần đầu, vẽ khi cuộn (60 fps). View ảo hóa của UI0-108 nhận các ngưỡng này.
// Số đo theo DoD-L là p99 (khung đầu: một mẫu mỗi vòng). Ngưỡng chỉ xét khi trion::test::kTimed (release không
// sanitizer), cỡ đủ như trên; bản debug, sanitizer chạy 20.000 dòng, 1.000 thay đổi, 30 nấc cuộn, chỉ in số đo. Máy
// chạy nhiều việc cùng lúc: mỗi đại lượng lấy vòng tốt nhất trong kRounds vòng. Đại lượng không phụ thuộc tải (không
// reset, hai lô cách ≥ 16 ms, chọn, cuộn) xét ở mọi bản.
#include "app/core/perf/UiProbe.h"
#include "app/widgets/model/BackgroundModel.h"

#include "core/base/sched/Scheduler.h"
#include "tests/timing.h"

#include <QApplication>
#include <QItemSelectionModel>
#include <QJsonObject>
#include <QListView>
#include <QScrollBar>
#include <QTest>
#include <QTimer>
#include <QTreeView>
#include <QWheelEvent>
#include <QWindow>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numeric>
#include <optional>
#include <random>
#include <set>

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;
namespace probe = trion::ui::probe;

namespace {

using trion::test::kTimed;
constexpr int kRounds = kTimed ? 3 : 1;
// Mỗi loại thay đổi (xóa, chèn, đổi, dời): 2.500 → 10.000 thay đổi. Bản không đo giờ (debug, sanitizer) chậm hơn
// nhiều lần: cỡ nhỏ hơn, vẫn đủ nhánh (nhiều lô, dataChanged nhiều đoạn 4.096 dòng, nới khoảng trống), vẫn xét chọn,
// cuộn, không reset; bộ test dưới 60 s cả khi máy tải nặng.
constexpr int kEach = kTimed ? 2500 : 250;
constexpr int kRows = kTimed ? 100'000 : 20'000;
constexpr int kWheel = kTimed ? 120 : 30; // nấc bánh xe

// Snapshot tổng hợp (DOC-009 chưa có): đối tượng (khóa, giá trị); hàm dựng tạo chuỗi ở nền như palette thật.
struct Entity {
    std::uint64_t key = 0;
    int value = 0;
};
using Snapshot = std::vector<Entity>;

struct Row {
    std::uint64_t key = 0;
    QString name;
    int value = 0;
    bool operator==(const Row&) const = default;
};

class Model : public trion::ui::BackgroundModel<Row> {
public:
    int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : 2; }
    QVariant data(const QModelIndex& i, int role) const override
    {
        if (role != Qt::DisplayRole || !i.isValid())
            return {};
        const Row& r = row(i.row());
        return i.column() == 0 ? QVariant(r.name) : QVariant(r.value);
    }
};

std::vector<Row> build(const Snapshot& s, const trion::sched::CancelToken& cancel)
{
    std::vector<Row> rows;
    rows.reserve(s.size());
    for (const Entity& e : s) {
        if ((rows.size() & 4095) == 0 && cancel.cancelled())
            break;
        rows.push_back({e.key, QString::number(e.key), e.value});
    }
    return rows;
}

// Mỗi loại `each` đối tượng khác nhau: xóa, đổi giá trị, dời chỗ; thêm `each` đối tượng mới vào chỗ ngẫu nhiên.
Snapshot mutate(const Snapshot& rows, std::mt19937& rng, int each, std::uint64_t& nextKey)
{
    std::vector<std::size_t> pick(rows.size());
    std::iota(pick.begin(), pick.end(), 0);
    for (std::size_t k = 0; k < std::size_t(3 * each); ++k)
        std::swap(pick[k], pick[k + rng() % (pick.size() - k)]);
    std::vector<int> kind(rows.size(), 0); // 1 xóa, 2 đổi, 3 dời
    for (std::size_t k = 0; k < std::size_t(3 * each); ++k)
        kind[pick[k]] = 1 + int(k) / each;
    Snapshot kept, loose;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (kind[i] == 0)
            kept.push_back(rows[i]);
        else if (kind[i] == 2)
            kept.push_back({rows[i].key, rows[i].value + 1});
        else if (kind[i] == 3)
            loose.push_back(rows[i]);
    }
    for (int k = 0; k < each; ++k)
        loose.push_back({nextKey++, 0});
    std::vector<std::pair<std::size_t, Entity>> at;
    for (const Entity& e : loose)
        at.push_back({rng() % (kept.size() + 1), e});
    std::stable_sort(at.begin(), at.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    Snapshot out;
    out.reserve(kept.size() + at.size());
    for (std::size_t i = 0, a = 0; i <= kept.size(); ++i) {
        for (; a < at.size() && at[a].first == i; ++a)
            out.push_back(at[a].second);
        if (i < kept.size())
            out.push_back(kept[i]);
    }
    return out;
}

double ms(Clock::duration d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

QJsonObject probeOp(const char* name)
{
    return probe::toJson()["ops"].toObject()[name].toObject();
}

double longestStall()
{
    double worst = 0;
    for (const auto& s : probe::stalls())
        worst = std::max(worst, ms(s.duration));
    return worst;
}

// Chạy vòng sự kiện thật (ngủ chờ sự kiện) đến khi done() hoặc quá hạn. Nhịp 5 ms: trên Windows,
// processEvents(WaitForMoreEvents) giao sự kiện đã đăng ở đầu lượt rồi vẫn ngủ chờ thông điệp mới, nên lượt làm done()
// đúng có thể ngủ đến khi guard hết hạn.
template <class F>
bool runUntil(F done, std::chrono::milliseconds timeout = 30s)
{
    QTimer guard, tick;
    guard.setSingleShot(true);
    guard.start(timeout);
    tick.start(5ms);
    while (!done() && guard.isActive())
        QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
    return done();
}

// p99 theo hạng gần nhất.
double p99(std::vector<double> v)
{
    if (v.empty())
        return 0;
    std::sort(v.begin(), v.end());
    return v[std::min(v.size() - 1, std::size_t(std::ceil(0.99 * double(v.size()))) - 1)];
}

// Khung = UpdateRequest tới cửa sổ. Ghi thời gian áp và số lô của model từ khung trước. Khung có lô: UiProbe
// palette_update đo từ đầu lô đến hết chính khung này (filter của ta chạy trong lần giao lại của probe, mẫu xong
// khi probe giao xong khung).
struct Frames : QObject {
    QWidget* view = nullptr;
    const trion::ui::BackgroundModelBase* model = nullptr;
    int count = 0;
    std::chrono::nanoseconds lastApply{};
    std::uint64_t lastBatches = 0, worstBatches = 0;
    std::vector<double> apply; // ms áp mỗi khung có lô
    void restart()
    {
        lastApply = model->stats().applyTotal;
        lastBatches = model->stats().batches;
        worstBatches = 0;
        apply.clear();
    }
    bool eventFilter(QObject*, QEvent* e) override
    {
        if (e->type() == QEvent::UpdateRequest) {
            ++count;
            const auto& s = model->stats();
            if (s.batches != lastBatches) {
                apply.push_back(ms(s.applyTotal - lastApply));
                probe::untilFrame(probe::Op::PaletteUpdate, view, s.lastBatch);
            }
            worstBatches = std::max(worstBatches, s.batches - lastBatches);
            lastApply = s.applyTotal;
            lastBatches = s.batches;
        }
        return false;
    }
};

// Một nấc bánh xe tại giữa viewport, gửi tới QWindow như từ hệ thống (UiProbe đo scroll_frame).
void wheel(QAbstractItemView* view, int delta)
{
    QWidget* top = view->window();
    const QPointF pos = view->viewport()->mapTo(top, view->viewport()->rect().center());
    QWheelEvent e(pos, top->mapToGlobal(pos), QPoint(), QPoint(0, delta), Qt::NoButton, Qt::NoModifier,
                  Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(top->windowHandle(), &e);
}

} // namespace

class BackgroundModelBench : public QObject {
    Q_OBJECT
    std::optional<trion::ui::UiExecutor> exec_;
    std::shared_ptr<const Snapshot> base_;

private slots:
    void initTestCase()
    {
        exec_.emplace();
        auto s = std::make_shared<Snapshot>();
        for (int i = 0; i < kRows; ++i)
            s->push_back({std::uint64_t(i), 0});
        base_ = s;
        probe::enable();
        probe::setStallThreshold(16ms);
    }
    void cleanupTestCase()
    {
        probe::disable();
        trion::sched::waitIdle();
        QCoreApplication::processEvents();
        exec_.reset();
        base_.reset();
    }

    // (2) Phần của model: 10.000 thay đổi trên 100.000 dòng, model chỉ nối QItemSelectionModel (không view): lô áp dài
    // nhất ≤ 4 ms. Có view thì lô còn gồm việc view Qt làm ngay trong tín hiệu, đo ở hundredThousandRows.
    void applyBudgetModelOnly()
    {
        double worst = 1e9;
        for (int round = 0; round < kRounds; ++round) {
            Model model;
            QItemSelectionModel sel(&model);
            model.refresh(base_, 1, &build);
            QVERIFY(runUntil([&] { return !model.busy(); }));
            for (int r = 0; r < kRows; r += 997)
                sel.select(model.index(r, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
            std::mt19937 rng(std::uint32_t(round + 1));
            std::uint64_t nextKey = kRows;
            const auto next = std::make_shared<const Snapshot>(mutate(*base_, rng, kEach, nextKey));
            const auto before = model.stats();
            model.refresh(next, 2, &build);
            QVERIFY(runUntil([&] { return !model.busy(); }));
            const auto& after = model.stats();
            QCOMPARE(model.rowCount(), int(next->size()));
            QCOMPARE(after.resets, std::uint64_t(0));
            qInfo("vòng %d: %d thay đổi = %llu thao tác, %llu lô, lô áp dài nhất %.2f ms", round, 4 * kEach,
                  (unsigned long long)(after.ops - before.ops), (unsigned long long)(after.batches - before.batches),
                  ms(after.longestApply));
            worst = std::min(worst, ms(after.longestApply)); // gồm lô điền 100.000 dòng đầu (không view: < 1 ms)
        }
        qInfo("không view, %d dòng, tốt nhất %d vòng: lô áp dài nhất %.2f ms", kRows, kRounds, worst);
        QVERIFY2(!kTimed || worst <= 4.0, "(2) một lô áp > 4 ms");
    }

    void hundredThousandRows_data()
    {
        QTest::addColumn<bool>("tree");
        QTest::addRow("QListView") << false;
        QTest::addRow("QTreeView") << true;
    }
    void hundredThousandRows()
    {
        QFETCH(bool, tree);
        double first = 1e9, sync = 1e9, fillStall = 1e9, applyP99 = 1e9, applyMax = 1e9, frameP99 = 1e9, frameMax = 1e9;
        double batch = 1e9, diffStall = 1e9, scrollP50 = 1e9, scrollP99 = 1e9, scrollMax = 1e9;
        for (int round = 0; round < kRounds; ++round) {
            Model model;
            std::unique_ptr<QAbstractItemView> view;
            if (tree) {
                auto* t = new QTreeView;
                t->setUniformRowHeights(true);
                t->setRootIsDecorated(false);
                view.reset(t);
            } else {
                auto* l = new QListView;
                l->setUniformItemSizes(true);
                view.reset(l);
            }
            view->setSelectionMode(QAbstractItemView::ExtendedSelection);
            view->setModel(&model);
            model.keepScrollPosition(view.get());
            Frames frames;
            frames.view = view.get();
            frames.model = &model;
            view->installEventFilter(&frames);
            view->resize(800, 600);
            view->show();
            QVERIFY(QTest::qWaitForWindowExposed(view.get()));
            const auto nextFrame = [&] {
                const int c = frames.count;
                return runUntil([&] { return frames.count > c; }, 5s);
            };

            // (1) Khung đầu có nội dung: đo từ refresh() đến hết khung kế sau khi dòng vào model.
            probe::reset();
            const auto t0 = Clock::now();
            model.refresh(base_, 1, &build);
            const auto call = Clock::now() - t0;
            connect(&model, &QAbstractItemModel::rowsInserted, view.get(),
                    [&] { probe::untilFrame(probe::Op::PaletteOpen, view.get(), t0); }, Qt::SingleShotConnection);
            QVERIFY(runUntil([&] { return !model.busy() && probeOp("palette_open")["count"].toInt() > 0; }));
            QCOMPARE(model.rowCount(), kRows);
            first = std::min(first, probeOp("palette_open")["max_ms"].toDouble());
            sync = std::min(sync, std::max(ms(call), ms(model.stats().longestBatch)));
            fillStall = std::min(fillStall, longestStall());

            // (2) Chọn ~100 dòng rải rác, cuộn giữa danh sách, rồi 10.000 thay đổi cùng lúc.
            QItemSelectionModel* sel = view->selectionModel();
            for (int r = 0; r < kRows; r += 997)
                sel->select(model.index(r, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
            sel->setCurrentIndex(model.index(kRows / 2 + 10, 0), QItemSelectionModel::NoUpdate);
            view->scrollTo(model.index(kRows / 2, 0), QAbstractItemView::PositionAtTop);
            QVERIFY(nextFrame());
            const auto keysOf = [&](const QModelIndexList& list) {
                std::set<std::uint64_t> out;
                for (const QModelIndex& i : list)
                    out.insert(model.row(i.row()).key);
                return out;
            };
            std::vector<std::pair<std::uint64_t, int>> seen; // dòng nhìn thấy: khóa, đỉnh
            for (QModelIndex i = view->indexAt(QPoint(0, 0)); i.isValid() && seen.size() < 8;
                 i = model.index(i.row() + 1, 0))
                seen.push_back({model.row(i.row()).key, view->visualRect(i).top()});
            const auto selected = keysOf(sel->selectedRows());
            const std::uint64_t current = model.row(sel->currentIndex().row()).key;

            std::mt19937 rng(std::uint32_t(round + 1));
            std::uint64_t nextKey = kRows;
            const auto next = std::make_shared<const Snapshot>(mutate(*base_, rng, kEach, nextKey));
            probe::reset();
            frames.restart();
            const auto before = model.stats();
            model.refresh(next, 2, &build);
            QVERIFY(runUntil([&] { return !model.busy(); }));
            QVERIFY(nextFrame());
            const auto after = model.stats();
            const QJsonObject pu = probeOp("palette_update");
            applyP99 = std::min(applyP99, p99(frames.apply));
            applyMax = std::min(applyMax, *std::max_element(frames.apply.begin(), frames.apply.end()));
            frameP99 = std::min(frameP99, pu["p99_ms"].toDouble());
            frameMax = std::min(frameMax, pu["max_ms"].toDouble());
            batch = std::min(batch, ms(after.longestBatch));
            diffStall = std::min(diffStall, longestStall());
            qInfo("vòng %d: %d thay đổi = %llu thao tác, %llu lô, %d khung có lô; áp mỗi khung p99 %.2f ms, tối đa "
                  "%.2f ms; khung có lô p50 %.2f ms, p99 %.2f ms, tối đa %.2f ms; lô dài nhất %.2f ms (cả giữ cuộn); "
                  "hai lô cách ít nhất %.1f ms; tối đa %llu lô giữa hai khung",
                  round, 4 * kEach, (unsigned long long)(after.ops - before.ops),
                  (unsigned long long)(after.batches - before.batches),
                  pu["count"].toInt(), p99(frames.apply), *std::max_element(frames.apply.begin(), frames.apply.end()),
                  pu["p50_ms"].toDouble(), pu["p99_ms"].toDouble(), pu["max_ms"].toDouble(), ms(after.longestBatch),
                  ms(after.shortestGap), (unsigned long long)frames.worstBatches);
            QCOMPARE(std::size_t(pu["count"].toInt()), frames.apply.size());
            QCOMPARE(after.resets, std::uint64_t(0));
            QVERIFY(after.shortestGap >= 16ms); // mỗi khung tối đa một lô
            QCOMPARE(model.rowCount(), int(next->size()));

            std::set<std::uint64_t> alive;
            for (const Entity& e : *next)
                alive.insert(e.key);
            std::set<std::uint64_t> want;
            for (auto k : selected)
                if (alive.count(k))
                    want.insert(k);
            QCOMPARE(keysOf(sel->selectedRows()), want);
            if (alive.count(current))
                QCOMPARE(model.row(sel->currentIndex().row()).key, current);
            const auto anchor =
                std::find_if(seen.begin(), seen.end(), [&](const auto& s) { return alive.count(s.first) > 0; });
            QVERIFY(anchor != seen.end());
            const QModelIndex top = view->indexAt(QPoint(0, 0));
            int row = top.row();
            while (row < model.rowCount() && model.row(row).key != anchor->first)
                ++row;
            QVERIFY(row < model.rowCount());
            QCOMPARE(view->visualRect(model.index(row, 0)).top(), anchor->second);

            // (3) Cuộn: kWheel nấc bánh xe xuống, mỗi nấc chờ khung của nó.
            probe::reset();
            for (int k = 0; k < kWheel; ++k) {
                wheel(view.get(), -120);
                QVERIFY(nextFrame());
            }
            const QJsonObject sf = probeOp("scroll_frame");
            QCOMPARE(sf["count"].toInt(), kWheel);
            scrollP50 = std::min(scrollP50, sf["p50_ms"].toDouble());
            scrollP99 = std::min(scrollP99, sf["p99_ms"].toDouble());
            scrollMax = std::min(scrollMax, sf["max_ms"].toDouble());
        }

        qInfo("%s %d dòng, tốt nhất %d vòng. (1) khung đầu %.1f ms, đồng bộ dài nhất %.2f ms. Chỉ báo (gồm việc "
              "view Qt, UI0-108): (1) khựng dài nhất %.1f ms; (2) áp mỗi khung p99 %.2f ms (tối đa %.2f), khung có lô "
              "p99 %.2f ms (tối đa %.2f), lô dài nhất %.2f ms, khựng dài nhất %.1f ms; (3) khung cuộn p50 %.2f ms, p99 "
              "%.2f ms (%.0f fps), tối đa %.2f ms",
              tree ? "QTreeView" : "QListView", kRows, kRounds, first, sync, fillStall, applyP99, applyMax, frameP99,
              frameMax, batch, diffStall, scrollP50, scrollP99, 1000.0 / scrollP99, scrollMax);
        if constexpr (kTimed) {
            QVERIFY2(first <= 150.0, "(1) khung đầu > 150 ms");
            QVERIFY2(sync <= 16.0, "(1) phần đồng bộ > 16 ms");
        }
    }
};

QTEST_MAIN(BackgroundModelBench)
#include "background_model_bench.moc"
