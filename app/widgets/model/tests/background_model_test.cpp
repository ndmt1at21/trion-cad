// UI0-014: model nền. Số trong ngoặc là tiêu chí nghiệm thu của task; số đo thời gian ở app/widgets/bench.
#include "app/widgets/model/BackgroundModel.h"

#include "core/base/assert.h"
#include "core/base/sched/Scheduler.h"
#include "tests/timing.h"

#include <QAbstractItemModelTester>
#include <QApplication>
#include <QListView>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QTreeView>

#include <algorithm>
#include <atomic>
#include <memory>
#include <numeric>
#include <optional>
#include <random>
#include <set>
#include <thread>

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;
using trion::sched::CancelToken;

namespace {

struct Row {
    std::uint64_t key = 0;
    int value = 0;
    bool operator==(const Row&) const = default;
};
using Snapshot = std::vector<Row>; // snapshot tổng hợp (DOC-009 chưa có): dãy dòng bất biến

// Số vòng đủ của tiêu chí 4 (10.000) ở bản tối ưu không sanitizer; bản debug và sanitizer chạy ít hơn, cùng nhánh
// mã, để bộ test dưới 60 s cả khi máy tải nặng. Preset tsan chạy thêm 10.000 vòng ở test riêng model_background_random
// (CMakeLists.txt).
constexpr bool kFull = trion::test::kTimed;

class Model : public trion::ui::BackgroundModel<Row> {
public:
    int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : 2; }
    QVariant data(const QModelIndex& i, int role) const override
    {
        if (!checkIndex(i, CheckIndexOption::IndexIsValid) || role != Qt::DisplayRole)
            return {};
        const Row& r = row(i.row());
        return i.column() == 0 ? QVariant(qulonglong(r.key)) : QVariant(r.value);
    }
};

// Hàm dựng: chép snapshot sau khi bận `busy` theo đồng hồ thật; xét hủy liên tục.
auto copyRows(std::chrono::microseconds busy = {})
{
    return [busy](const Snapshot& s, const CancelToken& cancel) {
        for (const auto end = Clock::now() + busy; Clock::now() < end && !cancel.cancelled();) {
        }
        return s;
    };
}

std::shared_ptr<const Snapshot> numbered(int n)
{
    auto s = std::make_shared<Snapshot>();
    for (int i = 0; i < n; ++i)
        s->push_back({std::uint64_t(i), 0});
    return s;
}

Snapshot shown(const Model& m)
{
    Snapshot out;
    for (int i = 0; i < m.rowCount(); ++i)
        out.push_back(m.row(i));
    return out;
}

double ms(Clock::duration d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

// Chạy vòng sự kiện thật (ngủ chờ sự kiện) đến khi model hết việc. Nhịp 5 ms: trên Windows,
// processEvents(WaitForMoreEvents) giao sự kiện đã đăng ở đầu lượt rồi vẫn ngủ chờ thông điệp mới, nên lượt làm model
// hết việc sẽ ngủ đến khi guard hết hạn (mỗi lần chờ 20 s).
bool settle(const trion::ui::BackgroundModelBase& m, std::chrono::milliseconds timeout = 20s)
{
    QTimer guard, tick;
    guard.setSingleShot(true);
    guard.start(timeout);
    tick.start(5ms);
    while (m.busy() && guard.isActive())
        QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
    return !m.busy();
}

// Mỗi loại `each` dòng khác nhau: xóa, đổi nội dung, dời chỗ; thêm `each` dòng mới vào chỗ ngẫu nhiên. O(n).
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
    std::vector<std::pair<std::size_t, Row>> at;
    for (const Row& r : loose)
        at.push_back({rng() % (kept.size() + 1), r});
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

// Đếm mọi tín hiệu thay đổi của model.
void countChanges(QAbstractItemModel& m, int& n)
{
    const auto inc = [&n] { ++n; };
    QObject::connect(&m, &QAbstractItemModel::dataChanged, inc);
    QObject::connect(&m, &QAbstractItemModel::headerDataChanged, inc);
    QObject::connect(&m, &QAbstractItemModel::rowsAboutToBeInserted, inc);
    QObject::connect(&m, &QAbstractItemModel::rowsInserted, inc);
    QObject::connect(&m, &QAbstractItemModel::rowsAboutToBeRemoved, inc);
    QObject::connect(&m, &QAbstractItemModel::rowsRemoved, inc);
    QObject::connect(&m, &QAbstractItemModel::rowsAboutToBeMoved, inc);
    QObject::connect(&m, &QAbstractItemModel::rowsMoved, inc);
    QObject::connect(&m, &QAbstractItemModel::layoutAboutToBeChanged, inc);
    QObject::connect(&m, &QAbstractItemModel::layoutChanged, inc);
    QObject::connect(&m, &QAbstractItemModel::modelAboutToBeReset, inc);
    QObject::connect(&m, &QAbstractItemModel::modelReset, inc);
}

std::atomic<int> g_asserts{0};
void countAssert(const char*, const char*, int)
{
    ++g_asserts;
}

} // namespace

class BackgroundModelTest : public QObject {
    Q_OBJECT
    std::optional<trion::ui::UiExecutor> exec_;

private slots:
    void initTestCase() { exec_.emplace(); }
    void cleanup()
    {
        trion::sched::waitIdle(); // kết quả còn chờ của model đã hủy: executor bỏ
        QCoreApplication::processEvents();
    }
    void cleanupTestCase() { exec_.reset(); }

    // Lần dựng đầu (model rỗng): một đoạn chèn, đúng nội dung và thứ tự (khóa không theo thứ tự hiển thị). Refresh ngay
    // khi dòng vào model (việc nền của lần dựng đầu thường còn đang dựng chỉ mục khóa): diff vẫn đúng, không reset,
    // persistent index đi theo dòng.
    void firstBuildInsertsDirectly()
    {
        constexpr int n = 5'000; // không gắn QAbstractItemModelTester: nó duyệt mọi dòng ở mỗi tín hiệu
        Model model;
        std::mt19937 rng(21);
        auto first = std::make_shared<Snapshot>(*numbered(n));
        std::shuffle(first->begin(), first->end(), rng);
        for (Row& r : *first)
            r.value = int(rng() % 100);
        std::uint64_t nextKey = n;
        const auto next = std::make_shared<const Snapshot>(mutate(*first, rng, 100, nextKey));
        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
        Snapshot firstShown;
        std::vector<std::pair<std::uint64_t, QPersistentModelIndex>> tracked;
        connect(
            &model, &QAbstractItemModel::rowsInserted, this,
            [&] {
                firstShown = shown(model);
                for (int r = 0; r < model.rowCount(); r += 7)
                    tracked.push_back({model.row(r).key, QPersistentModelIndex(model.index(r, r % 2))});
                model.refresh(next, 2, copyRows());
            },
            Qt::SingleShotConnection);
        model.refresh(std::shared_ptr<const Snapshot>(first), 1, copyRows());
        QVERIFY(settle(model));
        QCOMPARE(firstShown, *first);
        QCOMPARE(inserted[0][1].toInt(), 0);
        QCOMPARE(inserted[0][2].toInt(), n - 1);
        QCOMPARE(shown(model), *next);
        QCOMPARE(model.stats().resets, 0u);
        std::set<std::uint64_t> alive;
        for (const Row& r : *next)
            alive.insert(r.key);
        for (const auto& [key, index] : tracked) {
            QCOMPARE(index.isValid(), alive.count(key) > 0);
            if (index.isValid())
                QCOMPARE(model.row(index.row()).key, key);
        }
    }

    // Hủy giữa lần dựng đầu: việc đang dựng thấy hủy; kết quả đã gửi mà luồng UI chưa nhận thì bị bỏ. Chỉ kết quả mới
    // nhất hiện.
    void firstBuildCancelled()
    {
        Model model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        std::vector<quint64> settled;
        connect(&model, &Model::settled, this, [&](quint64 v) { settled.push_back(v); });
        std::atomic<bool> started{false};
        model.refresh(numbered(1000), 1, [&](const Snapshot& s, const CancelToken& cancel) {
            started = true;
            for (const auto end = Clock::now() + 10s; !cancel.cancelled() && Clock::now() < end;) {
            }
            return s;
        });
        while (!started)
            std::this_thread::yield();
        model.refresh(numbered(2000), 2, copyRows()); // hủy việc 1 đang dựng
        trion::sched::waitIdle();                     // kết quả 2 đã gửi, luồng UI chưa nhận
        auto last = std::make_shared<Snapshot>(*numbered(500));
        std::reverse(last->begin(), last->end());
        model.refresh(std::shared_ptr<const Snapshot>(last), 3, copyRows());
        QVERIFY(settle(model));
        QCOMPARE(shown(model), *last);
        QCOMPARE(settled, std::vector<quint64>{3});
        QCOMPARE(model.stats().stale, 1u); // kết quả 2
    }

    // Diff theo khóa: đúng nhóm tín hiệu cho xóa, dời, chèn, đổi; persistent index đi theo dòng.
    void diffEmitsGranularSignals()
    {
        Model model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.refresh(numbered(10), 1, copyRows());
        QVERIFY(settle(model));
        const QPersistentModelIndex nine(model.index(9, 1)), three(model.index(3, 0)), six(model.index(6, 0));

        // xóa 2, 3, 7; 9 lên đầu; chèn 100, 101 sau 5; đổi 6
        const auto next = std::make_shared<const Snapshot>(
            Snapshot{{9, 0}, {0, 0}, {1, 0}, {4, 0}, {5, 0}, {100, 0}, {101, 0}, {6, 1}, {8, 0}});
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved),
            inserted(&model, &QAbstractItemModel::rowsInserted), layout(&model, &QAbstractItemModel::layoutChanged),
            changed(&model, &QAbstractItemModel::dataChanged),
            moved(&model, &QAbstractItemModel::rowsMoved), reset(&model, &QAbstractItemModel::modelReset);
        model.refresh(next, 2, copyRows());
        QVERIFY(settle(model));

        QCOMPARE(shown(model), *next);
        QCOMPARE(removed.count(), 2); // hai đoạn: [7], [2, 3] (từ cuối lên)
        QCOMPARE(removed[0][1].toInt(), 7);
        QCOMPARE(removed[1][1].toInt(), 2);
        QCOMPARE(removed[1][2].toInt(), 3);
        QCOMPARE(layout.count(), 1);
        QCOMPARE(inserted.count(), 1); // một đoạn liên tiếp
        QCOMPARE(inserted[0][1].toInt(), 5);
        QCOMPARE(inserted[0][2].toInt(), 6);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed[0][0].value<QModelIndex>().row(), 7);
        QCOMPARE(changed[0][1].value<QModelIndex>().row(), 7);
        QCOMPARE(moved.count() + reset.count(), 0);
        QCOMPARE(nine.row(), 0);
        QCOMPARE(nine.column(), 1);
        QVERIFY(!three.isValid());
        QCOMPARE(six.row(), 7);
        QCOMPARE(model.stats().resets, 0u);

        model.refresh(std::make_shared<const Snapshot>(), 3, copyRows()); // xóa hết: một đoạn
        QVERIFY(settle(model));
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(removed.count(), 3);
    }

    // Mọi dòng còn tồn tại giữ persistent index (chọn, current, neo cuộn dựa vào đây): sau mỗi lần áp, index của
    // khóa k trỏ đúng dòng có khóa k; dòng bị xóa thì index hết hiệu lực. Hạn mức 0 để lô xen khung. 200 vòng (debug,
    // sanitizer 100).
    void persistentIndexesFollowRows()
    {
        Model model;
        model.setBatchBudget(0ns);
        std::mt19937 rng(3);
        std::uint64_t nextKey = 300;
        auto rows = numbered(300);
        model.refresh(rows, 0, copyRows());
        QVERIFY(settle(model));
        for (int round = 1; round <= (kFull ? 200 : 100); ++round) {
            std::vector<std::pair<std::uint64_t, QPersistentModelIndex>> tracked;
            for (int r = 0; r < model.rowCount(); ++r)
                tracked.push_back({model.row(r).key, QPersistentModelIndex(model.index(r, int(r % 2)))});
            rows = std::make_shared<const Snapshot>(mutate(*rows, rng, 1 + int(rng() % 40), nextKey));
            model.refresh(rows, std::uint64_t(round), copyRows());
            QVERIFY(settle(model));
            std::set<std::uint64_t> alive;
            for (const Row& r : *rows)
                alive.insert(r.key);
            for (const auto& [key, index] : tracked) {
                QCOMPARE(index.isValid(), alive.count(key) > 0);
                if (index.isValid())
                    QCOMPARE(model.row(index.row()).key, key);
            }
        }
    }

    // (4) 10.000 vòng ngẫu nhiên (release; debug, sanitizer 2.000; TRION_RANDOM_ROUNDS đặt số khác): yêu cầu dồn
    // dập, việc nền dài ngắn khác nhau, kết quả về lẫn lộn, lô áp xen giữa các yêu cầu. Model chỉ hiện các version
    // tăng dần, mỗi lần đúng nội dung version ấy; cuối cùng là version mới nhất.
    void newestResultWinsRandom()
    {
        Model model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        std::mt19937 rng(14);
        std::vector<std::shared_ptr<const Snapshot>> snaps(1); // theo version
        std::uint64_t last = 0;
        int settles = 0, wrongOrder = 0, wrongRows = 0;
        connect(&model, &Model::settled, this, [&](quint64 v) {
            ++settles;
            wrongOrder += v <= last;
            wrongRows += shown(model) != *snaps[std::size_t(v)];
            last = v;
        });

        int rounds = qEnvironmentVariableIntValue("TRION_RANDOM_ROUNDS");
        if (rounds <= 0)
            rounds = kFull ? 10'000 : 2'000;
        std::vector<std::uint64_t> keys(32);
        std::iota(keys.begin(), keys.end(), 0);
        for (int round = 1; round <= rounds; ++round) {
            std::shuffle(keys.begin(), keys.end(), rng);
            auto s = std::make_shared<Snapshot>();
            for (std::size_t i = 0, n = rng() % 25; i < n; ++i)
                s->push_back({keys[i], int(rng() % 3)});
            snaps.push_back(s);
            model.setBatchBudget(rng() % 2 ? 0ns : 4ms);
            model.refresh(snaps.back(), std::uint64_t(round), copyRows(std::chrono::microseconds(rng() % 40)));
            if (const auto r = rng() % 64; r < 16)
                QCoreApplication::processEvents(); // một lượt: nhận kết quả, áp vài lô
            else if (r == 16)
                QVERIFY(settle(model));
        }
        QVERIFY(settle(model));
        QCOMPARE(shown(model), *snaps.back());
        QCOMPARE(last, std::uint64_t(rounds));
        QCOMPARE(wrongOrder, 0);
        QCOMPARE(wrongRows, 0);
        QCOMPARE(model.stats().resets, 0u);
        qInfo("%d vòng: %d lần hiện xong, %llu việc dựng, %llu kết quả cũ bị bỏ, %llu lô, %llu thao tác", rounds,
              settles, (unsigned long long)model.stats().builds, (unsigned long long)model.stats().stale,
              (unsigned long long)model.stats().batches, (unsigned long long)model.stats().ops);
    }

    // (4) Yêu cầu mới hủy việc nền cũ ≤ 50 ms: từ refresh() mới đến lúc hàm dựng cũ thấy hủy và trả về (trung vị
    // 9 lần; ngưỡng chỉ xét ở bản release, xem tests/timing.h). Kết quả của yêu cầu cũ không bao giờ hiện.
    void cancelsOldRequestWithin50ms()
    {
        Model model;
        std::vector<quint64> settled;
        connect(&model, &Model::settled, this, [&](quint64 v) { settled.push_back(v); });
        std::vector<double> latency;
        for (int k = 0; k < 9; ++k) {
            std::atomic<bool> started{false};
            std::atomic<Clock::rep> stopped{0};
            const auto v = std::uint64_t(2 * k + 1);
            model.refresh(numbered(100), v, [&](const Snapshot& s, const CancelToken& cancel) {
                started = true;
                for (const auto end = Clock::now() + 10s; !cancel.cancelled() && Clock::now() < end;) {
                }
                stopped = Clock::now().time_since_epoch().count();
                return s;
            });
            while (!started)
                std::this_thread::yield();
            const auto t0 = Clock::now();
            model.refresh(numbered(k), v + 1, copyRows());
            while (!stopped)
                std::this_thread::yield();
            latency.push_back(ms(Clock::time_point(Clock::duration(stopped.load())) - t0));
            QVERIFY(settle(model));
            QCOMPARE(model.rowCount(), k);
        }
        for (quint64 v : settled)
            QCOMPARE(v % 2, 0u); // chỉ version mới
        std::sort(latency.begin(), latency.end());
        qInfo("hủy việc cũ: trung vị %.3f ms, lớn nhất %.3f ms", latency[4], latency.back());
        QVERIFY2(!trion::test::kTimed || latency[4] <= 50.0, qPrintable(QString::number(latency[4])));
    }

    // (5) Bản vẽ không đổi: cùng version thì không có việc nền; version mới mà dòng y hệt thì 0 cập nhật model.
    void unchangedSnapshotNoUpdates()
    {
        Model model;
        const auto s = numbered(1000);
        model.refresh(s, 1, copyRows());
        QVERIFY(settle(model));
        int changes = 0;
        countChanges(model, changes);
        QSignalSpy settled(&model, &Model::settled);
        const auto before = model.stats();

        model.refresh(s, 1, copyRows());
        QVERIFY(!model.busy());
        QCOMPARE(model.stats().builds, before.builds);

        model.refresh(std::make_shared<const Snapshot>(*s), 2, copyRows());
        QVERIFY(model.busy());
        QVERIFY(settle(model));
        QCOMPARE(settled.count(), 1);
        QCOMPARE(settled[0][0].value<quint64>(), 2u);
        QCOMPARE(changes, 0);
        QCOMPARE(model.stats().ops, before.ops);
        QCOMPARE(model.stats().batches, before.batches);
    }

    // Quay về version đang hiện khi việc khác còn chạy: chỉ hủy việc đó, không dựng lại, không tín hiệu.
    void returnToShownVersionOnlyCancels()
    {
        Model model;
        model.refresh(numbered(10), 1, copyRows());
        QVERIFY(settle(model));
        int changes = 0;
        countChanges(model, changes);
        std::atomic<bool> started{false}, sawCancel{false};
        model.refresh(numbered(20), 2, [&](const Snapshot& s, const CancelToken& cancel) {
            started = true;
            for (const auto end = Clock::now() + 10s; !cancel.cancelled() && Clock::now() < end;) {
            }
            sawCancel = cancel.cancelled();
            return s;
        });
        while (!started)
            std::this_thread::yield();
        model.refresh(numbered(10), 1, copyRows());
        QVERIFY(!model.busy());
        trion::sched::waitIdle();
        QCoreApplication::processEvents();
        QVERIFY(sawCancel);
        QCOMPARE(model.rowCount(), 10);
        QCOMPARE(model.stats().builds, 2u);
        QCOMPARE(changes, 0);
    }

    // Áp theo lô: hạn mức 0 thì mỗi lô đúng một thao tác, mỗi lô một lượt vòng sự kiện (xác định, không phụ
    // thuộc tải). 3 đoạn xóa + 3 đoạn chèn + trỏ dòng vào bảng mới + 1 dataChanged = 8 thao tác, 8 lô.
    void zeroBudgetAppliesOneOpPerBatch()
    {
        Model model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.refresh(numbered(100), 1, copyRows());
        QVERIFY(settle(model));
        auto next = std::make_shared<Snapshot>(*numbered(100));
        for (auto key : {50, 30, 10})
            next->erase(next->begin() + key);
        for (auto at : {80, 40, 5})
            next->insert(next->begin() + at, Row{std::uint64_t(1000 + at), 0});
        (*next)[60].value = 1;
        (*next)[61].value = 1;
        model.setBatchBudget(0ns);
        const auto before = model.stats();
        int rounds = 0; // lượt vòng sự kiện có lô
        model.refresh(std::shared_ptr<const Snapshot>(next), 2, copyRows());
        trion::sched::waitIdle();
        for (std::uint64_t seen = before.batches; model.busy() && rounds < 100; ++rounds) {
            QCoreApplication::processEvents();
            QVERIFY(model.stats().batches - seen <= 1); // không quá một lô mỗi lượt
            seen = model.stats().batches;
        }
        QVERIFY(!model.busy());
        QCOMPARE(shown(model), *next);
        QCOMPARE(model.stats().ops - before.ops, 8u);
        QCOMPARE(model.stats().batches - before.batches, 8u);
        QCOMPARE(model.stats().resets, 0u);
    }

    // (2) 1.000 thay đổi (xóa, chèn, đổi, dời mỗi loại 250) trên 5.000 dòng đang hiện, mọi bản (cỡ đủ 100.000 dòng,
    // 10.000 thay đổi ở app/widgets/bench, bản release): chọn và current index giữ với dòng còn tồn tại; dòng nhìn
    // thấy đầu tiên còn tồn tại giữ nguyên chỗ trong viewport. Không reset.
    void keepsSelectionAndScroll_data()
    {
        QTest::addColumn<bool>("tree");
        QTest::addColumn<bool>("pixels");
        QTest::addRow("QListView theo dòng") << false << false;
        QTest::addRow("QListView theo điểm ảnh") << false << true;
        QTest::addRow("QTreeView theo dòng") << true << false;
        QTest::addRow("QTreeView theo điểm ảnh") << true << true;
    }
    void keepsSelectionAndScroll()
    {
        QFETCH(bool, tree);
        QFETCH(bool, pixels);
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
        view->setVerticalScrollMode(pixels ? QAbstractItemView::ScrollPerPixel : QAbstractItemView::ScrollPerItem);
        view->setSelectionMode(QAbstractItemView::ExtendedSelection);
        view->setModel(&model);
        model.keepScrollPosition(view.get());
        view->resize(320, 240);
        view->show();
        QVERIFY(QTest::qWaitForWindowExposed(view.get()));

        constexpr int kRows = 5'000, kEach = 250;
        std::mt19937 rng(7);
        const auto base = numbered(kRows);
        model.refresh(base, 1, copyRows());
        QVERIFY(settle(model));
        QItemSelectionModel* sel = view->selectionModel();
        for (int r = 0; r < kRows; r += 37)
            sel->select(model.index(r, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        sel->setCurrentIndex(model.index(kRows / 2 + 100, 0), QItemSelectionModel::NoUpdate);
        view->scrollTo(model.index(kRows / 2, 0), QAbstractItemView::PositionAtTop);
        if (pixels)
            view->verticalScrollBar()->setValue(view->verticalScrollBar()->value() + 5); // đỉnh lệch giữa dòng
        QCoreApplication::processEvents();

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
        QVERIFY(!seen.empty());
        const auto selected = keysOf(sel->selectedRows());
        const std::uint64_t current = model.row(sel->currentIndex().row()).key;

        std::uint64_t nextKey = kRows;
        const auto next = std::make_shared<const Snapshot>(mutate(*base, rng, kEach, nextKey));
        model.refresh(next, 2, copyRows());
        QVERIFY(settle(model));
        QCoreApplication::processEvents();
        QCOMPARE(shown(model), *next);
        QCOMPARE(model.stats().resets, 0u);

        std::set<std::uint64_t> alive;
        for (const Row& r : *next)
            alive.insert(r.key);
        std::set<std::uint64_t> want;
        std::copy_if(selected.begin(), selected.end(), std::inserter(want, want.end()),
                     [&](auto k) { return alive.count(k) > 0; });
        QVERIFY(want.size() < selected.size()); // có dòng chọn bị xóa
        QCOMPARE(keysOf(sel->selectedRows()), want);
        if (alive.count(current))
            QCOMPARE(model.row(sel->currentIndex().row()).key, current);

        const auto anchor =
            std::find_if(seen.begin(), seen.end(), [&](const auto& s) { return alive.count(s.first) > 0; });
        QVERIFY(anchor != seen.end());
        int row = 0;
        while (model.row(row).key != anchor->first)
            ++row;
        QCOMPARE(view->visualRect(model.index(row, 0)).top(), anchor->second);
    }

    // Hàm dựng trả khóa trùng (lỗi lập trình): assert ở debug; vẫn hiện đúng nhờ reset (model rỗng: chèn thẳng), lần
    // sau diff lại bình thường.
    void duplicateKeysFallBackToReset()
    {
        Model model, empty;
        const auto old = trion::setAssertHandler(&countAssert);
        g_asserts = 0;
        const auto dup = std::make_shared<const Snapshot>(Snapshot{{1, 0}, {2, 0}, {1, 5}});
        model.refresh(numbered(1), 0, copyRows()); // model có dòng: bảng mới qua diff
        QVERIFY(settle(model));
        model.refresh(dup, 1, copyRows());
        QVERIFY(settle(model));
        QCOMPARE(shown(model), *dup);
        model.refresh(numbered(4), 2, copyRows()); // gốc có khóa trùng: reset lần nữa
        QVERIFY(settle(model));
        model.refresh(numbered(6), 3, copyRows()); // gốc sạch: diff
        QVERIFY(settle(model));
        QCOMPARE(shown(model), *numbered(6));
        QCOMPARE(model.stats().resets, 2u);

        empty.refresh(dup, 1, copyRows());
        QVERIFY(settle(empty));
        QCOMPARE(shown(empty), *dup);
        QCOMPARE(empty.stats().resets, 0u);
        empty.refresh(numbered(4), 2, copyRows()); // chỉ mục khóa dựng lười thấy khóa trùng: reset
        QVERIFY(settle(empty));
        trion::setAssertHandler(old);
        QCOMPARE(shown(empty), *numbered(4));
        QCOMPARE(empty.stats().resets, 1u);
#ifndef NDEBUG
        QCOMPARE(g_asserts.load(), 2);
#endif
    }

    // Model hủy khi việc nền đang chạy: việc thấy hủy, kết quả (nếu có) bị executor bỏ, không truy cập model đã hủy.
    void destroyWhileBuilding()
    {
        std::atomic<bool> started{false}, sawCancel{false};
        {
            Model model;
            model.refresh(numbered(1000), 1, [&](const Snapshot& s, const CancelToken& cancel) {
                started = true;
                for (const auto end = Clock::now() + 10s; !cancel.cancelled() && Clock::now() < end;) {
                }
                sawCancel = cancel.cancelled();
                return s;
            });
            while (!started)
                std::this_thread::yield();
        }
        trion::sched::waitIdle();
        QCoreApplication::processEvents();
        QVERIFY(sawCancel);
    }
};

QTEST_MAIN(BackgroundModelTest)
#include "background_model_test.moc"
