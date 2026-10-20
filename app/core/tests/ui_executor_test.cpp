// UI0-101: executor luồng UI. Số trong ngoặc là tiêu chí nghiệm thu của task.
#include "app/core/UiExecutor.h"

#include "core/base/sched/Scheduler.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTest>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

using namespace std::chrono_literals;
using trion::sched::Priority;
using trion::ui::Receiver;
using trion::ui::UiExecutor;

namespace {

// Việc giả của một callback: bận đúng d theo đồng hồ thật (cả khi chạy dưới sanitizer).
void spin(std::chrono::nanoseconds d)
{
    const auto end = std::chrono::steady_clock::now() + d;
    while (std::chrono::steady_clock::now() < end) {
    }
}

double ms(std::chrono::nanoseconds d)
{
    return double(d.count()) / 1e6;
}

// "Khung hình" giả 16 ms trên luồng UI: khoảng cách lớn nhất giữa hai khung cho biết khung bị trễ bao nhiêu.
struct FrameProbe {
    QTimer timer;
    QElapsedTimer clock;
    qint64 last = 0, maxGapNs = 0;
    FrameProbe()
    {
        timer.setTimerType(Qt::PreciseTimer);
        QObject::connect(&timer, &QTimer::timeout, [this] {
            const qint64 now = clock.nsecsElapsed();
            maxGapNs = std::max(maxGapNs, now - last);
            last = now;
        });
        clock.start();
        timer.start(16ms);
    }
};

// Số callback mỗi lượt, đo ngoài executor: bộ lọc sự kiện thấy sự kiện đánh thức ngay trước mỗi lượt.
// Callback của test bận ≥ 1 µs, nên lượt ≤ 4.000 callback là ≤ 4 ms việc trên luồng UI. Đại lượng này không bị
// phóng đại khi hệ điều hành hay hypervisor giành CPU giữa lượt; thời gian thật của lượt thì có, nên chỉ báo số.
struct TurnCounter : QObject {
    explicit TurnCounter(const int& counter) : done(counter) {}
    const int& done;
    int atStart = 0, maxItems = 0;
    bool eventFilter(QObject*, QEvent*) override
    {
        close();
        atStart = done;
        return false;
    }
    void close() { maxItems = std::max(maxItems, done - atStart); }
};
constexpr int kMaxItemsPerTurn = 4000; // 4 ms / 1 µs

std::atomic<int> g_asserts{0};
void countAssert(const char*, const char*, int)
{
    ++g_asserts;
}

#ifdef NDEBUG
constexpr int kAssertsPerMisuse = 0; // release: không assert, chỉ trả lỗi hoặc bỏ
#else
constexpr int kAssertsPerMisuse = 1; // debug: assert (test thay handler để chạy tiếp)
#endif

} // namespace

class UiExecutorTest : public QObject {
    Q_OBJECT

    std::optional<UiExecutor> exec_;

    // Chạy hết các lượt đang chờ: mỗi sendPostedEvents là một lượt, lượt hẹn sau đăng sự kiện mới.
    void drain()
    {
        for (std::uint64_t seen = ~0ull; seen != exec_->stats().wakeups;) {
            seen = exec_->stats().wakeups;
            QCoreApplication::sendPostedEvents(&*exec_, 0);
        }
    }

    // Vòng sự kiện thật (ngủ chờ sự kiện, không polling 10 ms như qWait) đến khi loop.quit() hoặc quá hạn.
    static void runLoop(QEventLoop& loop, std::chrono::milliseconds timeout)
    {
        QTimer guard;
        guard.setSingleShot(true);
        QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
        guard.start(timeout);
        loop.exec();
    }

private slots:
    void init() { exec_.emplace(); }
    void cleanup()
    {
        trion::sched::waitIdle();
        exec_.reset();
    }

    // (1) 100.000 kết quả từ 4 luồng rải đều trong ~1 s, mỗi callback ~1 µs việc; luồng UI chạy vòng sự kiện
    // thật kèm khung hình 16 ms. Theo lô: số lần đánh thức nhỏ hơn nhiều số kết quả.
    void batchesUnderSteadyLoad()
    {
        constexpr int kThreads = 4, kSlices = 100, kPerSlice = 250, kTotal = kThreads * kSlices * kPerSlice;
        QEventLoop loop;
        int received = 0; // chỉ luồng UI chạm tới
        TurnCounter turns(received);
        exec_->installEventFilter(&turns);
        FrameProbe frames;
        QElapsedTimer wall;
        wall.start();
        std::vector<std::thread> producers;
        for (int t = 0; t < kThreads; ++t)
            producers.emplace_back([&] {
                for (int s = 0; s < kSlices; ++s) {
                    for (int i = 0; i < kPerSlice; ++i)
                        trion::ui::post([&] {
                            spin(1us);
                            if (++received == kTotal)
                                loop.quit();
                        });
                    std::this_thread::sleep_for(10ms);
                }
            });
        runLoop(loop, 30s);
        const qint64 wallMs = wall.elapsed();
        for (auto& p : producers)
            p.join();
        turns.close();
        exec_->removeEventFilter(&turns);
        const auto st = exec_->stats();
        qInfo("(1) đều: %d kết quả trong %lld ms từ %d luồng; %llu lần đánh thức (%.0f kết quả/lô, nhiều nhất %d); "
              "lượt dài nhất %.3f ms, trung bình %.3f ms; khoảng cách khung lớn nhất %.1f ms",
              kTotal, wallMs, kThreads, static_cast<unsigned long long>(st.wakeups),
              double(kTotal) / double(st.wakeups), turns.maxItems, ms(st.longest),
              ms(st.busy) / double(st.wakeups), double(frames.maxGapNs) / 1e6);
        QCOMPARE(received, kTotal);
        QVERIFY(st.wakeups <= std::uint64_t(kTotal / 20)); // trung bình ≥ 20 kết quả mỗi lần đánh thức
        QVERIFY(turns.maxItems <= kMaxItemsPerTurn);
    }

    // (1) 100.000 kết quả dồn khi luồng UI đang bận: một sự kiện đánh thức cho cả lô; luồng UI chia nhiều lượt
    // ≤ 4 ms việc, và sự kiện, timer khác chạy ngay giữa các lượt (khung không phải chờ hết hàng).
    void burstIsSplitIntoTurns()
    {
        constexpr int kThreads = 4, kPerThread = 25'000, kTotal = kThreads * kPerThread;
        QEventLoop loop;
        int received = 0;
        std::vector<std::thread> producers;
        for (int t = 0; t < kThreads; ++t)
            producers.emplace_back([&] {
                for (int i = 0; i < kPerThread; ++i)
                    trion::ui::post([&] {
                        spin(1us);
                        if (++received == kTotal)
                            loop.quit();
                    });
            });
        for (auto& p : producers)
            p.join();
        QCOMPARE(exec_->stats().wakeups, std::uint64_t(1));

        TurnCounter turns(received);
        exec_->installEventFilter(&turns);
        int atPosted = -1, atTimer = -1; // số kết quả đã nhận khi sự kiện thăm dò chạy
        QMetaObject::invokeMethod(&loop, [&] { atPosted = received; }, Qt::QueuedConnection);
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, [&] { atTimer = received; });
        timer.start(1ms);
        FrameProbe frames;
        QElapsedTimer wall;
        wall.start();
        runLoop(loop, 30s);
        const qint64 wallMs = wall.elapsed();
        turns.close();
        exec_->removeEventFilter(&turns);
        const auto st = exec_->stats();
        qInfo("(1) dồn: %d kết quả xử lý trong %lld ms qua %llu lượt (%.0f kết quả/lượt, nhiều nhất %d); lượt dài "
              "nhất %.3f ms, trung bình %.3f ms; khoảng cách khung lớn nhất %.1f ms; sự kiện thăm dò chạy khi đã "
              "nhận %d, timer 1 ms khi đã nhận %d",
              kTotal, wallMs, static_cast<unsigned long long>(st.wakeups), double(kTotal) / double(st.wakeups),
              turns.maxItems, ms(st.longest), ms(st.busy) / double(st.wakeups), double(frames.maxGapNs) / 1e6,
              atPosted, atTimer);
        QCOMPARE(received, kTotal);
        QVERIFY(turns.maxItems <= kMaxItemsPerTurn);
        QVERIFY(st.wakeups >= 25); // ≥ 100 ms việc chia lượt 3 ms
        QVERIFY(atPosted >= 0 && atPosted < kTotal / 10);
        QVERIFY(atTimer >= 0 && atTimer < kTotal / 10);
    }

    // (2) 1.000 kết quả cùng khóa trong một khung: receiver nhận đúng 1 lần, giá trị cuối.
    void latestWinsPerKey()
    {
        QObject sink;
        const Receiver rx(&sink);
        int calls = 0, value = 0;
        trion::sched::submit(Priority::Normal, [rx, &calls, &value] {
            for (int i = 1; i <= 1000; ++i)
                trion::ui::postLatest(rx, [i, &calls, &value] {
                    ++calls;
                    value = i;
                });
        });
        trion::sched::waitIdle(); // luồng UI không chạy vòng sự kiện: cả 1.000 kết quả rơi vào một khung
        QCOMPARE(exec_->stats().wakeups, std::uint64_t(1));
        drain();
        QCOMPARE(calls, 1);
        QCOMPARE(value, 1000);

        // Đã giao thì khóa trống, kết quả sau lại được giao; Receiver tạo riêng (dù cùng đối tượng) là khóa khác.
        const Receiver other(&sink);
        trion::ui::postLatest(rx, [&] {
            ++calls;
            value = 1001;
        });
        trion::ui::postLatest(other, [&] { ++calls; });
        drain();
        QCOMPARE(calls, 3);
        QCOMPARE(value, 1001);
    }

    // (3) Receiver hủy trước khi kết quả về, hoặc bị callback trước trong cùng lượt hủy: kết quả bị bỏ.
    void dropsResultsOfDestroyedReceiver()
    {
        auto* gone = new QObject;
        const Receiver rx(gone);
        bool ran = false;
        trion::sched::submit(Priority::Normal, [rx, &ran] {
            trion::ui::post(rx, [&ran] { ran = true; });
            trion::ui::postLatest(rx, [&ran] { ran = true; });
        });
        trion::sched::waitIdle();
        delete gone;

        auto* dying = new QObject;
        const Receiver rx2(dying);
        trion::ui::post(rx2, [dying] { delete dying; });
        trion::ui::post(rx2, [&ran] { ran = true; });
        drain();
        QVERIFY(!ran);
        QCOMPARE(exec_->stats().ran, std::uint64_t(1));
        QCOMPARE(exec_->stats().dropped, std::uint64_t(3));
    }

    // (3) 10.000 vòng ngẫu nhiên (chạy dưới ASan và TSan ở preset asan-ubsan, tsan): mỗi vòng tạo một receiver,
    // giao việc nền gửi về một kết quả thường và một kết quả gộp, rồi ngẫu nhiên hủy nó ngay, hủy một receiver
    // bất kỳ, chạy một lượt, hoặc không làm gì.
    void randomRoundsWithDestroyedReceivers()
    {
        struct Sink : QObject {
            int id = 0, plain = 0, latest = 0;
        };
        constexpr int kRounds = 10'000;
        std::mt19937 rng(20261020);
        std::vector<Sink*> live;
        std::unordered_set<int> liveIds; // chỉ luồng UI chạm tới
        int violations = 0;              // callback chạy khi receiver đã hủy
        const auto destroy = [&](std::size_t i) {
            liveIds.erase(live[i]->id);
            delete live[i];
            live[i] = live.back();
            live.pop_back();
        };
        for (int round = 0; round < kRounds; ++round) {
            auto* sink = new Sink;
            sink->id = round;
            live.push_back(sink);
            liveIds.insert(round);
            const Receiver rx(sink);
            const int yields = int(rng() % 64);
            // Luồng nền chỉ chép rx và con trỏ sink, không chạm vào sink.
            trion::sched::submit(Priority::Normal, [rx, sink, round, yields, &liveIds, &violations] {
                for (int i = 0; i < yields; ++i)
                    std::this_thread::yield(); // xáo thời điểm gửi so với lúc hủy
                trion::ui::post(rx, [sink, round, &liveIds, &violations] {
                    liveIds.count(round) ? ++sink->plain : ++violations;
                });
                trion::ui::postLatest(rx, [sink, round, &liveIds, &violations] {
                    liveIds.count(round) ? ++sink->latest : ++violations;
                });
            });
            switch (rng() % 4) {
            case 0: destroy(live.size() - 1); break;                       // thường trước khi kết quả về
            case 1: destroy(rng() % live.size()); break;                   // có thể đang có kết quả chờ
            case 2: QCoreApplication::sendPostedEvents(&*exec_, 0); break; // một lượt
            default: break;
            }
        }
        trion::sched::waitIdle();
        drain();
        const auto st = exec_->stats();
        qInfo("(3) %d vòng: %zu receiver còn sống; %llu callback chạy, %llu bỏ vì receiver đã hủy", kRounds,
              live.size(), static_cast<unsigned long long>(st.ran), static_cast<unsigned long long>(st.dropped));
        QCOMPARE(violations, 0);
        QCOMPARE(st.ran + st.dropped, std::uint64_t(2 * kRounds));
        for (const Sink* s : live) {
            QCOMPARE(s->plain, 1);
            QCOMPARE(s->latest, 1);
        }
        for (Sink* s : live)
            delete s;
    }

    // (4) Rảnh (2 s cho CI; tiêu chí gốc 10 s): không sự kiện nào tới executor, không đăng đánh thức, không timer.
    void idleCausesNoWakeups()
    {
        trion::ui::post([] {});
        drain(); // có việc rồi hết việc
        struct Counter : QObject {
            int events = 0;
            bool eventFilter(QObject*, QEvent*) override
            {
                ++events;
                return false;
            }
        } counter;
        exec_->installEventFilter(&counter);
        const auto before = exec_->stats().wakeups;
        QEventLoop loop;
        runLoop(loop, 2s);
        exec_->removeEventFilter(&counter);
        qInfo("(4) rảnh 2 s: %d sự kiện tới executor, %llu lần đánh thức", counter.events,
              static_cast<unsigned long long>(exec_->stats().wakeups - before));
        QCOMPARE(counter.events, 0);
        QCOMPARE(exec_->stats().wakeups, before);
        QVERIFY(exec_->findChildren<QObject*>().isEmpty()); // không QTimer con
    }

    // (5) API ghi gọi ngoài luồng UI: assert ở debug, trả lỗi ở release (test chạy ở cả preset dev và release).
    void writeApiOffUiThreadFails()
    {
        const auto old = trion::setAssertHandler(&countAssert);
        g_asserts = 0;
        QVERIFY(trion::ui::requireUiThread().has_value());
        TRION_ASSERT_UI_THREAD();
        QCOMPARE(g_asserts.load(), 0);

        bool ok = true;
        std::string code;
        std::thread([&] {
            const auto r = trion::ui::requireUiThread();
            ok = r.has_value();
            if (!r)
                code = r.error().code;
            TRION_ASSERT_UI_THREAD();
        }).join();
        trion::setAssertHandler(old);
        QVERIFY(!ok);
        QVERIFY(code == "not-ui-thread");
        QCOMPARE(g_asserts.load(), 2 * kAssertsPerMisuse);
    }

    // trion::sched::postToUi() của lõi đi qua executor.
    void corePostToUiRunsOnUiThread()
    {
        QThread* ui = QThread::currentThread();
        bool onUi = false;
        trion::sched::submit(Priority::Normal,
                             [&] { trion::sched::postToUi([&] { onUi = QThread::currentThread() == ui; }); });
        trion::sched::waitIdle();
        drain();
        QVERIFY(onUi);
        QCOMPARE(exec_->stats().ran, std::uint64_t(1));
    }

    // Hủy executor: kết quả chờ bị bỏ, poster của lõi được gỡ; gửi sau đó là lỗi lập trình và bị bỏ.
    void destroyDropsPendingAndUninstallsPoster()
    {
        bool ran = false;
        trion::ui::post([&ran] { ran = true; });
        exec_.reset();
        QCoreApplication::sendPostedEvents();
        QVERIFY(!ran);

        const auto old = trion::setAssertHandler(&countAssert);
        g_asserts = 0;
        trion::ui::post([&ran] { ran = true; });
        trion::sched::postToUi([&ran] { ran = true; });
        trion::setAssertHandler(old);
        QCOMPARE(g_asserts.load(), 2 * kAssertsPerMisuse);
        exec_.emplace();
        drain();
        QVERIFY(!ran);
    }

    // Callback ném ngoại lệ: ghi stderr, lượt chạy tiếp, executor không kẹt.
    void throwingCallbackDoesNotStall()
    {
        bool ran = false;
        trion::ui::post([] { throw std::runtime_error("thử"); });
        trion::ui::post([&ran] { ran = true; });
        drain();
        QVERIFY(ran);
        QCOMPARE(exec_->stats().ran, std::uint64_t(2));
    }
};

QTEST_GUILESS_MAIN(UiExecutorTest)
#include "ui_executor_test.moc"
