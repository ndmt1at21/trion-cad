// NT-013 (tập con): BackgroundJobs. Mỗi tiêu chí nghiệm thu một nhóm hàm test:
//   1  batching: 1.000 việc, mỗi việc báo tiến trình 1 kHz (50 lần, ngủ 1 ms giữa hai lần; mỗi worker của pool một
//      việc): mọi cửa sổ 1 s có ≤ 10 lô (lô cách nhau ≥ 100 ms); áp lô 1.000 việc (chụp + phát + người nghe) ≤ 1 ms
//      ở vòng tốt nhất, chỉ kiểm ở release (tests/timing.h); xong thì rảnh: 0 lô, timer tắt.
//   2  cancelRunning: việc tôn trọng CancelToken thành Cancelled ≤ 50 ms (đo bằng info(), kiểm ở release), lô báo
//      hủy đến ngay, không chờ nhịp 100 ms; cancelQueued: việc còn chờ thành Cancelled ngay trong cancel(), fn không
//      chạy; cancelOwner: đóng document hủy 100% việc của nó (cả việc đang chờ), việc của document khác chạy xong.
//   3  manySequential: 10⁵ việc theo 10 đợt: registry về rỗng sau mỗi đợt, mọi closure đã hủy (rò bộ nhớ:
//      LeakSanitizer của preset asan-ubsan). Preset tsan chạy 2 đợt.
//   Các hàm công khai khác và nhánh lỗi: progressTree, progressParallel, results (Done, Failed, ném, hủy rồi trả
//   lỗi), flags (cờ 0,5 s/2 s theo thời gian, việc im lặng vẫn có lô khi cờ đổi), unknownIds, instance.
#include "core/base/jobs/BackgroundJobs.h"

#include "core/base/tsan.h"
#include "tests/timing.h"

#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QTimer>
#include <QtTest>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <map>
#include <mutex>
#include <semaphore>
#include <stdexcept>
#include <thread>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using namespace trion::jobs;
namespace sched = trion::sched;
using trion::Expected;
using trion::test::kTimed;

namespace {

// Poster của test cho sched::postToUi: hàng đợi std::mutex + một sự kiện đánh thức mỗi lô (như UiExecutor của
// UI0-101, rút gọn; core/base không phụ thuộc app/). TSan thấy hàng đợi; sự kiện Qt chỉ để đánh thức.
class TestPoster final : public QObject {
public:
    void post(std::function<void()> fn)
    {
        bool wake = false;
        {
            std::lock_guard l(m_);
            q_.push_back(std::move(fn));
            wake = !std::exchange(posted_, true);
        }
        if (wake) {
            TRION_TSAN_RELEASE(this);
            QCoreApplication::postEvent(this, new QEvent(QEvent::User));
        }
    }

protected:
    void customEvent(QEvent*) override
    {
        TRION_TSAN_ACQUIRE(this);
        std::vector<std::function<void()>> q;
        {
            std::lock_guard l(m_);
            q.swap(q_);
            posted_ = false;
        }
        for (auto& fn : q)
            fn();
    }

private:
    std::mutex m_;
    std::vector<std::function<void()>> q_;
    bool posted_ = false;
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

// Chạy vòng sự kiện thật trong d (kiểm "rảnh").
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

// Ghi các lô nhận được trên luồng UI. Khai báo trước registry (hủy sau nó).
struct Recorder {
    std::vector<Clock::time_point> at;          // lúc nhận từng lô
    std::map<JobId, JobInfo> last;              // ảnh mới nhất của mỗi việc
    std::map<JobId, Clock::time_point> endedAt; // lúc nhận lô báo kết thúc
    std::size_t ended = 0;

    void attach(BackgroundJobs& r)
    {
        QObject::connect(&r, &BackgroundJobs::updated, &r, [this](const std::vector<JobInfo>& batch) {
            const auto now = Clock::now();
            at.push_back(now);
            for (const JobInfo& i : batch) {
                last[i.id] = i;
                if (i.state != State::Running) {
                    ++ended;
                    endedAt[i.id] = now;
                }
            }
        });
    }
    State state(JobId id) const
    {
        const auto it = last.find(id);
        return it == last.end() ? State::Running : it->second.state;
    }
};

bool timersIdle(const BackgroundJobs& r)
{
    const auto timers = r.findChildren<QTimer*>();
    return !timers.isEmpty() && std::none_of(timers.begin(), timers.end(), [](QTimer* t) { return t->isActive(); });
}

} // namespace

class JobsTest : public QObject {
    Q_OBJECT
    TestPoster poster_;

private slots:
    void initTestCase()
    {
        sched::setUiPoster([this](std::function<void()> fn) { poster_.post(std::move(fn)); });
    }

    void cleanupTestCase()
    {
        sched::waitIdle();
        QCoreApplication::processEvents();
        sched::setUiPoster({});
    }

    // Tiêu chí 1.
    void batching()
    {
        constexpr int kJobs = 1000, kUpdates = 50;
        Recorder rec;
        BackgroundJobs r;
        rec.attach(r);
        std::atomic<long> reports{0};
        const auto t0 = Clock::now();
        for (int k = 0; k < kJobs; ++k)
            r.start(u"Việc %1"_s.arg(k), u"test"_s, 0, [&reports](Progress& p, sched::CancelToken) -> Expected<void> {
                auto tick = Clock::now();
                for (int i = 1; i <= kUpdates; ++i) {
                    p.set(double(i) / kUpdates);
                    reports.fetch_add(1, std::memory_order_relaxed);
                    std::this_thread::sleep_until(tick += 1ms); // 1 kHz
                }
                return {};
            });
        QVERIFY(spinUntil([&] { return rec.ended == std::size_t(kJobs); }, 50s));
        const auto span = Clock::now() - t0;
        QCOMPARE(r.size(), std::size_t(0));
        for (const auto& [id, info] : rec.last) {
            QCOMPARE(info.state, State::Done);
            QCOMPARE(info.progress, 1.0);
        }
        // Mọi cửa sổ 1 s (nửa mở) chứa ≤ 10 lô; khoảng cách nhỏ nhất giữa hai lô.
        std::size_t worst = 0;
        auto minGap = Clock::duration::max();
        for (std::size_t i = 0; i < rec.at.size(); ++i) {
            const auto end = std::lower_bound(rec.at.begin() + qsizetype(i), rec.at.end(), rec.at[i] + 1s);
            worst = std::max(worst, std::size_t(end - (rec.at.begin() + qsizetype(i))));
            if (i)
                minGap = std::min(minGap, rec.at[i] - rec.at[i - 1]);
        }
        const auto stats = r.stats();
        qInfo("1.000 việc × 1 kHz: %ld lần báo trong %.0f ms (%.0f lần/s); %zu lô (%.1f lô/s), nhiều nhất %zu lô/1 s, "
              "cách nhau ít nhất %.1f ms; áp lô lâu nhất %.3f ms",
              reports.load(), ms(span), double(reports.load()) / (ms(span) / 1000), rec.at.size(),
              double(rec.at.size()) / (ms(span) / 1000), worst, ms(minGap), ms(stats.longest));
        QCOMPARE(stats.batches, std::uint64_t(rec.at.size()));
        QVERIFY2(worst <= 10, "quá 10 lô trong một giây");
        QVERIFY(minGap >= 100ms); // nhịp tính từ lúc áp xong lô trước: không phụ thuộc tải

        // Áp lô: đồng hồ thật của lô lâu nhất ở trên gồm cả lúc luồng UI bị hệ điều hành dừng (máy dùng chung). Đo
        // riêng lô lớn nhất của kịch bản này (1.000 việc đang chạy cùng đổi trong một lô: lúc bắt đầu, lúc qua mốc
        // cờ) 5 vòng, lấy vòng tốt nhất làm chi phí áp lô của registry (chụp + phát; người nghe chỉ đếm: phần áp của
        // UI do UI0-112 đo).
        auto best = Clock::duration::max();
        for (int round = 0; round < 5; ++round) {
            BackgroundJobs one;
            std::size_t seen = 0;
            QObject::connect(&one, &BackgroundJobs::updated, &one,
                             [&seen](const std::vector<JobInfo>& batch) { seen += batch.size(); });
            std::atomic<bool> release{false};
            for (int k = 0; k < kJobs; ++k)
                one.start(u"Việc %1"_s.arg(k), u"test"_s, 0,
                          [&release](Progress& p, sched::CancelToken) -> Expected<void> {
                              p.set(0.5);
                              while (!release.load())
                                  std::this_thread::sleep_for(1ms);
                              return {};
                          });
            QVERIFY(spinUntil([&] { return one.stats().batches == 1; })); // lô đầu: cả 1.000 việc mới
            QCOMPARE(seen, std::size_t(kJobs));
            best = std::min<Clock::duration>(best, one.stats().longest);
            release = true;
            sched::waitIdle();
            QVERIFY(spinUntil([&] { return one.size() == 0; }));
        }
        qInfo("áp lô 1.000 việc: vòng tốt nhất %.3f ms", ms(best));
        QVERIFY2(!kTimed || best <= 1ms, "áp một lô quá 1 ms");

        // Rảnh: không lô, không timer.
        const auto before = rec.at.size();
        spinFor(300ms);
        QCOMPARE(rec.at.size(), before);
        QVERIFY(timersIdle(r));
    }

    // Tiêu chí 2.
    void cancelRunning()
    {
        Recorder rec;
        BackgroundJobs r;
        rec.attach(r);
        std::atomic<bool> running{false};
        const JobId id =
            r.start(u"Đang mở"_s, u"open"_s, 1, [&running](Progress& p, sched::CancelToken t) -> Expected<void> {
                running = true;
                for (int i = 0; !t.cancelled(); ++i) {
                    p.set(1.0 - 1.0 / (i + 2));
                    std::this_thread::sleep_for(1ms);
                }
                return trion::Error{"test", "stopped", "dừng vì bị hủy"}; // i18n-ok: dữ liệu thử
            });
        QVERIFY(spinUntil([&] { return running.load(); }));
        spinFor(150ms); // nhịp lô đang bận vì tiến trình: trường hợp xấu nhất cho lô báo hủy
        QVERIFY(r.info(id)->progress > 0);

        const auto t0 = Clock::now();
        QVERIFY(r.cancel(id));
        QVERIFY(r.info(id)->cancelRequested);
        // Không chạy vòng sự kiện: lô không lấy việc ra khỏi registry nên info() thấy trạng thái cuối.
        std::optional<JobInfo> i;
        while ((i = r.info(id)) && i->state == State::Running && Clock::now() - t0 < 10s)
            std::this_thread::sleep_for(100us);
        const auto stateLatency = Clock::now() - t0;
        QVERIFY(i);
        QCOMPARE(i->state, State::Cancelled); // đã yêu cầu hủy: lỗi fn trả về không biến thành Failed
        QVERIFY(spinUntil([&] { return rec.state(id) == State::Cancelled; }));
        const auto reportLatency = rec.endedAt[id] - t0;
        qInfo("hủy → Cancelled: %.2f ms (registry), %.2f ms (lô tới luồng UI)", ms(stateLatency), ms(reportLatency));
        QVERIFY2(!kTimed || stateLatency <= 50ms, "Cancelled sau hơn 50 ms");
        QVERIFY2(!kTimed || reportLatency <= 50ms, "lô báo hủy sau hơn 50 ms");
        QVERIFY(!r.cancel(id)); // đã kết thúc
        QCOMPARE(r.size(), std::size_t(0));
    }

    void cancelQueued()
    {
        Recorder rec;
        BackgroundJobs r;
        rec.attach(r);
        // Chặn mọi worker của pool để việc đích nằm chờ trong hàng đợi.
        const int blockers = 2 * int(std::max(2u, std::thread::hardware_concurrency()));
        std::atomic<bool> release{false};
        std::vector<JobId> ids;
        for (int k = 0; k < blockers; ++k)
            ids.push_back(r.start(u"chặn"_s, u"test"_s, 0, [&release](Progress&, sched::CancelToken) -> Expected<void> {
                while (!release.load())
                    std::this_thread::sleep_for(1ms);
                return {};
            }));
        std::atomic<bool> ran{false};
        const JobId target = r.start(u"đích"_s, u"test"_s, 0, [&ran](Progress&, sched::CancelToken) -> Expected<void> {
            ran = true;
            return {};
        });
        QVERIFY(r.cancel(target));
        QCOMPARE(r.info(target)->state, State::Cancelled); // ngay trong cancel()
        QVERIFY(spinUntil([&] { return rec.state(target) == State::Cancelled; }));
        release = true;
        QVERIFY(spinUntil([&] { return rec.ended == std::size_t(blockers) + 1; }));
        sched::waitIdle();
        QVERIFY(!ran.load());
        for (JobId id : ids)
            QCOMPARE(rec.state(id), State::Done);
    }

    void cancelOwner()
    {
        Recorder rec;
        BackgroundJobs r;
        rec.attach(r);
        constexpr std::uint64_t kDocA = 7, kDocB = 8;
        std::vector<JobId> a, b;
        // Việc của A không tự xong (đủ nhiều để có việc còn chờ trong hàng); việc của B xong sau ~20 ms.
        for (int k = 0; k < 20; ++k)
            a.push_back(r.start(u"A"_s, u"autosave"_s, kDocA, [](Progress& p, sched::CancelToken t) -> Expected<void> {
                while (!t.cancelled()) {
                    p.set(0.5);
                    std::this_thread::sleep_for(1ms);
                }
                return {};
            }));
        for (int k = 0; k < 5; ++k)
            b.push_back(r.start(u"B"_s, u"plot"_s, kDocB, [](Progress& p, sched::CancelToken) -> Expected<void> {
                std::this_thread::sleep_for(20ms);
                p.set(1);
                return {};
            }));
        spinFor(50ms);
        QCOMPARE(r.cancelOwner(kDocA), 20);
        QCOMPARE(r.cancelOwner(kDocA), 0); // đã yêu cầu hủy hết
        QCOMPARE(r.cancelOwner(12345), 0);
        QVERIFY(spinUntil([&] { return rec.ended == a.size() + b.size(); }));
        for (JobId id : a)
            QCOMPARE(rec.state(id), State::Cancelled);
        for (JobId id : b)
            QCOMPARE(rec.state(id), State::Done);
        QCOMPARE(r.size(), std::size_t(0));
    }

    // Tiêu chí 3.
    void manySequential()
    {
#ifdef TRION_TSAN
        constexpr int kWaves = 2; // TSan chậm ~8 lần ở việc rất ngắn: 2×10⁴ đủ bắt race, giữ bộ test < 60 s
#else
        constexpr int kWaves = 10;
#endif
        constexpr int kPerWave = 10000;
        Recorder rec;
        BackgroundJobs r;
        rec.attach(r);
        auto sentinel = std::make_shared<int>(0); // mỗi closure giữ một bản: còn closure nào sống thì đếm > 1
        std::atomic<int> runs{0};
        for (int w = 0; w < kWaves; ++w) {
            for (int k = 0; k < kPerWave; ++k)
                r.start(u"việc"_s, u"test"_s, std::uint64_t(k % 3),
                        [sentinel, &runs](Progress& p, sched::CancelToken) -> Expected<void> {
                            p.set(1);
                            runs.fetch_add(1, std::memory_order_relaxed);
                            return {};
                        });
            sched::waitIdle();
            QVERIFY(spinUntil([&] { return r.size() == 0; }));
            QCOMPARE(rec.ended, std::size_t(w + 1) * std::size_t(kPerWave));
        }
        QCOMPARE(runs.load(), kWaves * kPerWave);
        QCOMPARE(sentinel.use_count(), 1L);
        qInfo("10⁵ việc: %zu lô", rec.at.size());
    }

    void progressTree()
    {
        BackgroundJobs r;
        std::binary_semaphore reached{0}, go{0};
        const auto step = [&] {
            reached.release();
            go.acquire();
        };
        const JobId id = r.start(u"cây"_s, u"test"_s, 0, [&](Progress& p, sched::CancelToken) -> Expected<void> {
            Progress read = p.child(0.7);
            Progress regen = p.child(0.3);
            read.set(0.5);
            step();                         // 0,35
            read.set(0.2);                  // lùi: bỏ
            Progress sub = read.child(0.5); // nửa sau của read (đã báo 0,5): 35%..70% của việc
            sub.set(1);
            step();                                              // 0,35 + 0,35 = 0,7
            regen.set(std::numeric_limits<double>::quiet_NaN()); // NaN: bỏ
            regen.set(-1);
            regen.set(0.5);
            step();                     // 0,85
            p.set(0.9);                 // phần của cha đã giao hết cho con: không cộng
            Progress none = p.child(0); // trọng số 0: phạm vi rỗng
            none.set(1);
            Progress tail = regen.child(2); // kẹp 1: phần còn lại của regen
            tail.set(7);                    // kẹp 1
            step();                         // 1
            return {};
        });
        const auto expect = [&](double want) {
            reached.acquire();
            const double got = r.info(id)->progress;
            go.release();
            return std::abs(got - want) < 1e-9;
        };
        QVERIFY(expect(0.35));
        QVERIFY(expect(0.70));
        QVERIFY(expect(0.85));
        QVERIFY(expect(1.0));
        QVERIFY(spinUntil([&] { return r.size() == 0; }));
    }

    void progressParallel()
    {
        BackgroundJobs r;
        std::binary_semaphore reached{0}, go{0};
        const JobId id = r.start(u"song song"_s, u"test"_s, 0, [&](Progress& p, sched::CancelToken) -> Expected<void> {
            std::vector<Progress> parts;
            for (int k = 0; k < 3; ++k)
                parts.push_back(p.child(1.0 / 3)); // làm tròn xuống: còn dư vài đơn vị cho cha
            std::vector<std::thread> threads;      // chỉ trong test (mã sản phẩm dùng sched::submit)
            for (Progress& part : parts)
                threads.emplace_back([&part] {
                    for (int i = 1; i <= 2000; ++i)
                        part.set(i / 2000.0);
                });
            for (auto& t : threads)
                t.join();
            reached.release();
            go.acquire();
            p.set(1); // phần dư sau ba con
            reached.release();
            go.acquire();
            return {};
        });
        reached.acquire();
        const double three = r.info(id)->progress;
        go.release();
        reached.acquire();
        const double all = r.info(id)->progress;
        go.release();
        QVERIFY(three > 1 - 1e-9 && three < 1);
        QCOMPARE(all, 1.0); // đơn vị nguyên: đúng tuyệt đối
        QVERIFY(spinUntil([&] { return r.size() == 0; }));
    }

    void results()
    {
        Recorder rec;
        BackgroundJobs r;
        rec.attach(r);
        const JobId done =
            r.start(u"xong"_s, u"test"_s, 0, [](Progress&, sched::CancelToken) -> Expected<void> { return {}; });
        const JobId failed = r.start(u"lỗi"_s, u"test"_s, 3, [](Progress& p, sched::CancelToken) -> Expected<void> {
            p.set(0.25);
            return trion::Error{"dwg", "corrupt", "Tệp hỏng ở bảng layer"}; // i18n-ok: dữ liệu thử
        });
        const JobId thrown = r.start(u"ném"_s, u"test"_s, 0, [](Progress&, sched::CancelToken) -> Expected<void> {
            throw std::runtime_error("boom");
        });
        const JobId thrownOther =
            r.start(u"ném khác"_s, u"test"_s, 0, [](Progress&, sched::CancelToken) -> Expected<void> { throw 42; });
        QVERIFY(spinUntil([&] { return rec.ended == 4; }));
        QCOMPARE(rec.last[done].state, State::Done);
        QCOMPARE(rec.last[done].progress, 1.0);
        const JobInfo& f = rec.last[failed];
        QCOMPARE(f.state, State::Failed);
        QCOMPARE(f.error.module, std::string("dwg"));
        QCOMPARE(f.error.code, std::string("corrupt"));
        QCOMPARE(f.progress, 0.25);
        QCOMPARE(f.owner, std::uint64_t(3));
        QCOMPARE(f.name, u"lỗi"_s);
        QCOMPARE(f.kind, u"test"_s);
        QCOMPARE(rec.last[thrown].state, State::Failed);
        QCOMPARE(rec.last[thrown].error.code, std::string("exception"));
        QCOMPARE(rec.last[thrown].error.message, std::string("boom"));
        QCOMPARE(rec.last[thrownOther].state, State::Failed);
        QVERIFY(!rec.last[thrownOther].error.message.empty());
        QVERIFY(rec.last[done].elapsed >= Clock::duration::zero());
    }

    void flags()
    {
        Recorder rec;
        BackgroundJobs r(Timing{40ms, 120ms, 10ms});
        rec.attach(r);
        std::atomic<bool> release{false};
        const JobId quick =
            r.start(u"nhanh"_s, u"test"_s, 0, [](Progress&, sched::CancelToken) -> Expected<void> { return {}; });
        const JobId slow =
            r.start(u"chậm"_s, u"test"_s, 0, [&release](Progress&, sched::CancelToken) -> Expected<void> {
                while (!release.load()) // im lặng: không báo tiến trình
                    std::this_thread::sleep_for(1ms);
                return {};
            });
        const auto started = r.info(slow)->startedAt;
        Clock::time_point busyAt{}, showAt{};
        QVERIFY(spinUntil([&] {
            const auto it = rec.last.find(slow);
            if (it != rec.last.end() && it->second.busyCursor && busyAt == Clock::time_point{})
                busyAt = Clock::now();
            if (it != rec.last.end() && it->second.showProgress && showAt == Clock::time_point{})
                showAt = Clock::now();
            return showAt != Clock::time_point{};
        }));
        QVERIFY(busyAt != Clock::time_point{});
        QVERIFY(busyAt - started >= 40ms);
        QVERIFY(showAt - started >= 120ms);
        QCOMPARE(rec.state(quick), State::Done);
        // Cờ của việc đã xong tính theo thời gian chạy thật (máy tải nặng có thể giữ worker > 40 ms).
        QCOMPARE(rec.last[quick].busyCursor, rec.last[quick].elapsed >= 40ms);
        QCOMPARE(rec.last[quick].showProgress, rec.last[quick].elapsed >= 120ms);
        release = true;
        QVERIFY(spinUntil([&] { return rec.state(slow) == State::Done; }));
        QVERIFY(rec.last[slow].busyCursor && rec.last[slow].showProgress);
        QVERIFY(rec.last[slow].elapsed >= 120ms);
        qInfo("cờ bận sau %.1f ms (mốc 40), hiện tiến trình sau %.1f ms (mốc 120), nhịp lô 10 ms", ms(busyAt - started),
              ms(showAt - started));
        QVERIFY2(!kTimed || busyAt - started <= 40ms + 10ms + 20ms, "cờ bận đến muộn"); // mốc + nhịp lô + nhiễu
        QVERIFY2(!kTimed || showAt - started <= 120ms + 10ms + 20ms, "cờ hiện tiến trình đến muộn");
        spinFor(50ms);
        QVERIFY(timersIdle(r));
    }

    void unknownIds()
    {
        BackgroundJobs r;
        QVERIFY(!r.cancel(0));
        QVERIFY(!r.cancel(999999));
        QVERIFY(!r.info(999999));
        QCOMPARE(r.cancelOwner(42), 0);
        QCOMPARE(r.size(), std::size_t(0));
        QCOMPARE(r.timing().busyCursor, 500ms);
        QCOMPARE(r.timing().showProgress, 2000ms);
        QCOMPARE(r.timing().batchInterval, 100ms);
        std::atomic<bool> running{false};
        const JobId id = r.start(u"x"_s, u"test"_s, 0, [&running](Progress&, sched::CancelToken t) -> Expected<void> {
            running = true;
            while (!t.cancelled())
                std::this_thread::sleep_for(1ms);
            return {};
        });
        QVERIFY(spinUntil([&] { return running.load(); }));
        QVERIFY(r.cancel(id));
        QVERIFY(spinUntil([&] { return r.size() == 0; }));
    }

    void instance()
    {
        QCOMPARE(BackgroundJobs::instance(), nullptr);
        {
            BackgroundJobs first;
            QCOMPARE(BackgroundJobs::instance(), &first);
            {
                BackgroundJobs second;
                QCOMPARE(BackgroundJobs::instance(), &first);
            }
            QCOMPARE(BackgroundJobs::instance(), &first);
        }
        QCOMPARE(BackgroundJobs::instance(), nullptr);
    }
};

QTEST_GUILESS_MAIN(JobsTest)
#include "jobs_test.moc"
