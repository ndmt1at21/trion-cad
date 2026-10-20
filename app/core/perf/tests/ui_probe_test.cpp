// UI0-105: UiProbe. Số trong ngoặc là tiêu chí nghiệm thu của task. Chưa có Tracy (NT-004): mốc đối chiếu là
// steady_clock của chính test.
#include "app/core/perf/UiProbe.h"

#include <QAbstractEventDispatcher>
#include <QDialog>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QMenu>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QWidget>
#include <QWindow>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <new>
#include <thread>
#include <utility>

// Đếm cấp phát của cả tiến trình (thay operator new toàn cục) để kiểm "khi tắt không cấp phát". Dưới ASan, TSan
// không thay (runtime sanitizer tự định nghĩa new/delete, như numfmt_test); preset dev, release, win vẫn kiểm.
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#define TRION_COUNT_ALLOCS 0
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#define TRION_COUNT_ALLOCS 0
#endif
#endif
#ifndef TRION_COUNT_ALLOCS
#define TRION_COUNT_ALLOCS 1
#endif
static std::atomic<long> g_allocs{0};
#if TRION_COUNT_ALLOCS
void* operator new(std::size_t n)
{
    g_allocs.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(n ? n : 1))
        return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
#endif

using namespace std::chrono_literals;
namespace probe = trion::ui::probe;
using probe::Clock;
using probe::Op;

namespace {

double ms(Clock::duration d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

// Vòng sự kiện thật (có aboutToBlock như ứng dụng, không qSleep như qWait) đến khi cond() đúng hoặc quá hạn.
bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 10s)
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

// Vòng sự kiện thật d, không sự kiện nào khác của test.
void spinFor(std::chrono::milliseconds d)
{
    QEventLoop loop;
    QTimer::singleShot(d, &loop, &QEventLoop::quit);
    loop.exec();
}

// Bản const để operator[] trả QJsonValue (bản không const trả QJsonValueRef, không lồng [] được).
const QJsonObject json()
{
    return probe::toJson();
}

int count(const char* op)
{
    return json()["ops"][QLatin1StringView(op)]["count"].toInt();
}

// Cửa sổ có độ trễ cài sẵn: phím/nhấn chuột mất `lag` rồi (sau `hops` lần gọi hàng đợi) yêu cầu vẽ lại; kéo
// chuột vẽ lại ngay; nhả chuột không vẽ. Ghi mốc vẽ xong.
class LagWidget : public QWidget {
public:
    std::chrono::milliseconds lag{0};
    int hops = 0;
    bool disableOnPaint = false;
    Clock::time_point painted{};

    bool paintAfter(const std::function<void()>& act)
    {
        painted = {};
        act();
        return spinUntil([this] { return painted != Clock::time_point{}; });
    }

protected:
    void keyPressEvent(QKeyEvent*) override { react(); }
    void mousePressEvent(QMouseEvent*) override { react(); }
    void mouseMoveEvent(QMouseEvent*) override { update(); }
    void paintEvent(QPaintEvent*) override
    {
        if (std::exchange(disableOnPaint, false))
            probe::disable();
        painted = Clock::now();
    }

private:
    void react()
    {
        const auto end = Clock::now() + lag; // chờ bận: Sleep của Windows có thể ngủ ít hơn yêu cầu
        while (Clock::now() < end) {
        }
        hop(hops);
    }
    void hop(int n)
    {
        if (n == 0)
            update();
        else
            QMetaObject::invokeMethod(this, [this, n] { hop(n - 1); }, Qt::QueuedConnection);
    }
};

// Mốc của chính test: filter ứng dụng cài sau probe nên chạy ngay trước filter của probe.
class InputMark : public QObject {
public:
    Clock::time_point at{};
    bool eventFilter(QObject* o, QEvent* e) override
    {
        if (o->isWindowType() && (e->type() == QEvent::KeyPress || e->type() == QEvent::MouseButtonPress))
            at = Clock::now();
        return false;
    }
};

// Handler khựng: sự kiện User làm luồng UI bận `stall`, có thể trong một vùng đo.
class StallTarget : public QObject {
    Q_OBJECT
public:
    std::chrono::milliseconds stall{0};
    const char* zone = nullptr;
    Clock::duration took{};
    bool done = false;

    bool event(QEvent* e) override
    {
        if (e->type() != QEvent::User)
            return QObject::event(e);
        const auto t0 = Clock::now();
        if (zone) {
            TRION_UI_PROBE_SCOPE(zone);
            std::this_thread::sleep_for(stall);
        } else {
            std::this_thread::sleep_for(stall);
        }
        took = Clock::now() - t0;
        done = true;
        return true;
    }

    // Gửi một sự kiện khựng qua vòng sự kiện thật; trả các lần khựng mới ghi.
    std::vector<probe::Stall> run(std::chrono::milliseconds d, const char* inZone = nullptr)
    {
        const auto before = probe::stalls().size();
        stall = d;
        zone = inZone;
        done = false;
        QCoreApplication::postEvent(this, new QEvent(QEvent::User));
        spinUntil([this] { return done; });
        spinFor(5ms); // mốc tiến triển kế tiếp chốt độ dài
        const auto all = probe::stalls();
        return std::vector<probe::Stall>(all.begin() + std::ptrdiff_t(before), all.end());
    }
};

bool showAndSettle(LagWidget& w)
{
    w.resize(240, 120);
    w.show();
    return QTest::qWaitForWindowExposed(&w) && spinUntil([&] { return w.painted != Clock::time_point{}; });
}

} // namespace

class UiProbeTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() // dispatcher quyết định cách phát awake/aboutToBlock; probe phải đúng với mọi loại
    {
        qInfo("dispatcher: %s", QAbstractEventDispatcher::instance()->metaObject()->className());
    }
    void init()
    {
        probe::disable();
        probe::reset();
        probe::setStallThreshold(50ms);
    }
    void cleanup() { probe::disable(); }

    // (1) Input → khung: sai số so với mốc của test ≤ 1 ms với độ trễ cài sẵn 5/20/100 ms. Lấy trung vị 5 lần đo:
    // máy tải cao có thể dừng luồng giữa mốc của test và mốc của probe (vài chục µs), làm lệch một lần đo.
    void accuracy_data()
    {
        QTest::addColumn<int>("lag");
        QTest::addColumn<bool>("mouse");
        for (int lag : {5, 20, 100}) {
            QTest::addRow("phím %d ms", lag) << lag << false;
            QTest::addRow("nhấn chuột %d ms", lag) << lag << true;
        }
    }
    void accuracy()
    {
        QFETCH(int, lag);
        QFETCH(bool, mouse);
        LagWidget w;
        QVERIFY(showAndSettle(w));
        probe::enable();
        InputMark mark;
        qApp->installEventFilter(&mark);
        QWindow* win = w.windowHandle();
        std::vector<double> errors;
        for (int i = 0; i < 5; ++i) {
            const QPoint at(20 + 40 * i, 20); // khác chỗ: timestamp giả của QTest sát nhau, cùng chỗ thành nhấp đúp
            probe::reset();
            w.lag = std::chrono::milliseconds(lag);
            QVERIFY(w.paintAfter([&] {
                if (mouse)
                    QTest::mousePress(win, Qt::LeftButton, {}, at);
                else
                    QTest::keyClick(win, Qt::Key_A);
            }));
            const double ref = ms(w.painted - mark.at);
            const QJsonValue op = json()["ops"][mouse ? "ui_state" : "typing"];
            QCOMPARE(op["count"].toInt(), 1);
            const double got = op["max_ms"].toDouble();
            QVERIFY2(got >= lag, qPrintable(QString::number(got)));
            errors.push_back(std::abs(got - ref));
            if (mouse) { // nhả chuột không vẽ gì: mẫu của nó phải bị bỏ trước vòng sau
                QTest::mouseRelease(win, Qt::LeftButton, {}, at);
                spinFor(20ms);
            }
        }
        std::sort(errors.begin(), errors.end());
        qInfo("độ trễ cài %d ms: sai số trung vị %.3f ms, lớn nhất %.3f ms", lag, errors[2], errors[4]);
        QVERIFY2(errors[2] <= 1.0, qPrintable(QString::number(errors[2])));
    }

    // Input không gây khung thì bỏ mẫu, không ghép với khung không liên quan về sau; khung đến sau một chuỗi
    // lời gọi hàng đợi vẫn được tính.
    void inputWithoutFrame()
    {
        LagWidget w;
        QVERIFY(showAndSettle(w));
        probe::enable();
        QWindow* win = w.windowHandle();
        QVERIFY(w.paintAfter([&] { QTest::mousePress(win, Qt::LeftButton, {}, QPoint(200, 100)); }));
        QTest::mouseRelease(win, Qt::LeftButton, {}, QPoint(200, 100)); // không vẽ
        spinFor(100ms);
        QVERIFY(w.paintAfter([&] { w.update(); })); // khung không do input
        QCOMPARE(count("ui_state"), 1);

        w.hops = 3;
        QVERIFY(w.paintAfter([&] { QTest::keyClick(win, Qt::Key_B); }));
        QCOMPARE(count("typing"), 1);
    }

    // (2) Tắt: không cấp phát, chi phí nhỏ, không filter trên đường sự kiện.
    void disabledIsFree()
    {
        QVERIFY(!probe::enabled());
        LagWidget w;
        QVERIFY(showAndSettle(w));
        constexpr int kN = 200000;
        const long before = g_allocs.load();
        for (int i = 0; i < kN; ++i) {
            TRION_UI_PROBE_SCOPE("test.zone");
            probe::Scope op(Op::PaletteUpdate);
            probe::record(Op::OsnapHitTest, 1us);
            probe::untilFrame(Op::PaletteOpen, &w);
        }
        if (TRION_COUNT_ALLOCS)
            QCOMPARE(g_allocs.load() - before, 0L);
        else
            qInfo("sanitizer: không đếm cấp phát (kiểm ở preset dev, release, win)");

        const auto t0 = Clock::now();
        for (int i = 0; i < kN; ++i) {
            TRION_UI_PROBE_SCOPE("test.zone");
        }
        const double perZone = double(std::chrono::nanoseconds(Clock::now() - t0).count()) / kN;
        // Giả sử 100 vùng đo mỗi khung 16,6 ms; ngưỡng 1%.
        qInfo("tắt: %.2f ns mỗi vùng đo = %.4f%% khung khi có 100 vùng/khung", perZone, perZone * 100 / 16.6e6 * 100);
        QVERIFY2(perZone * 100 < 0.01 * 16.6e6, qPrintable(QString::number(perZone)));

        const auto events = probe::counters().events;
        QVERIFY(w.paintAfter([&] { QTest::keyClick(w.windowHandle(), Qt::Key_A); }));
        QCOMPARE(probe::counters().events, events);
        QCOMPARE(count("typing"), 0);
    }

    // Chi phí khi bật, mỗi sự kiện (chỉ báo số).
    void enabledCost()
    {
        QObject target;
        auto perEvent = [&] {
            constexpr int kN = 100000;
            QEvent e(QEvent::User);
            const auto t0 = Clock::now();
            for (int i = 0; i < kN; ++i)
                QCoreApplication::sendEvent(&target, &e);
            return double(std::chrono::nanoseconds(Clock::now() - t0).count()) / kN;
        };
        const double off = perEvent();
        probe::enable();
        const double on = perEvent();
        qInfo("giao một sự kiện: tắt %.1f ns, bật %.1f ns", off, on);
        QVERIFY(on < 10000);
    }

    // (2) Bật/tắt bằng đối số; --ui-probe=<tệp> ghi JSON khi tắt.
    void arguments()
    {
        QVERIFY(!probe::configure({"trion"}));
        QVERIFY(!probe::configure({"trion", "--ui-probes"}));
        QVERIFY(probe::configure({"trion", "--ui-probe"}));
        QVERIFY(probe::enabled());
        probe::disable();
        QVERIFY(!probe::enabled());

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("probe.json");
        QVERIFY(probe::configure({"trion", "--ui-probe=" + path}));
        probe::record(Op::CommandComplete, 3ms);
        probe::disable();
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonObject j = QJsonDocument::fromJson(f.readAll()).object();
        QCOMPARE(j["format"].toString(), QStringLiteral("trion-ui-probe/1"));
        QCOMPARE(j["ops"]["command_complete"]["count"].toInt(), 1);
        QCOMPARE(j["ops"]["command_complete"]["p50_ms"].toDouble(), 3.0);
    }

    // disable() gọi từ trong một khung (lúc probe đang giao lại sự kiện vẽ) rồi bật lại.
    void disableInsideFrame()
    {
        LagWidget w;
        QVERIFY(showAndSettle(w));
        probe::enable();
        w.disableOnPaint = true;
        QVERIFY(w.paintAfter([&] { w.update(); }));
        QVERIFY(!probe::enabled());
        spinFor(10ms);
        probe::enable();
        QVERIFY(w.paintAfter([&] { QTest::keyClick(w.windowHandle(), Qt::Key_C); }));
        QCOMPARE(count("typing"), 1);
    }

    // (3) Watchdog bắt mọi lần khựng ≥ 50 ms kèm tên handler và vùng đo; dưới ngưỡng không báo.
    void watchdogCatchesStalls()
    {
        probe::enable();
        StallTarget t;
        struct Case {
            int ms;
            const char* zone;
        };
        const Case cases[] = {{20, nullptr}, {60, "test.short"}, {80, nullptr}, {100, "test.stall"},
                              {150, nullptr}, {300, "test.long"}, {30, "test.fast"}};
        int expected = 0, caught = 0, live = 0;
        for (const Case& c : cases) {
            const auto got = t.run(std::chrono::milliseconds(c.ms), c.zone);
            // Không báo nhầm: mọi bản ghi là một khoảng ≥ ngưỡng thật (máy tải cao có thể dừng luồng UI quá ngưỡng
            // cả khi handler ngắn; khi đó báo là đúng).
            for (const probe::Stall& s : got)
                QVERIFY2(s.duration >= 50ms, s.where.c_str());
            const double took = ms(t.took);
            if (took < 50)
                continue;
            ++expected;
            const auto it = std::find_if(got.begin(), got.end(), [](const probe::Stall& s) {
                return s.where.find("User -> ") != std::string::npos && s.where.find("StallTarget") != std::string::npos;
            });
            QVERIFY2(it != got.end(), got.empty() ? "không có bản ghi" : got.front().where.c_str());
            if (c.zone)
                QVERIFY2(it->where.find(c.zone) != std::string::npos, it->where.c_str());
            QVERIFY(it->duration >= t.took);
            if (c.ms >= 300)
                QVERIFY2(it->live, "luồng watchdog không bắt được lần khựng 300 ms khi đang khựng");
            ++caught;
            live += it->live;
            qInfo("khựng %.1f ms → ghi %.1f ms, %s: %s", took, ms(it->duration), it->live ? "bắt khi đang khựng" : "chốt sau",
                  it->where.c_str());
        }
        QCOMPARE(caught, expected);
        QVERIFY(expected >= 4);
        qInfo("bắt %d/%d lần khựng ≥ 50 ms, %d lần ngay khi đang khựng", caught, expected, live);
    }

    // Ngưỡng đặt được.
    void watchdogThreshold()
    {
        probe::enable();
        probe::setStallThreshold(200ms);
        StallTarget t;
        for (const probe::Stall& s : t.run(100ms)) // dưới ngưỡng mới: chỉ được báo nếu luồng UI thật sự dừng ≥ 200 ms
            QVERIFY2(s.duration >= 200ms, s.where.c_str());
        const auto got = t.run(260ms);
        QVERIFY(!got.empty() && got.back().duration >= 260ms);
        QCOMPARE(json()["stalls"]["threshold_ms"].toDouble(), 200.0);
    }

    // (3) Rảnh: 0 báo nhầm, watchdog ngủ không thời hạn (không polling). Bộ đếm đọc từ luồng phụ giữa khoảng rảnh,
    // xa hai sự kiện timer mở/đóng vòng.
    void watchdogIdle()
    {
        probe::enable();
        spinFor(100ms);
        probe::Counters c0, c1;
        std::size_t stalls0 = 0, stalls1 = 0;
        std::thread sampler([&] {
            std::this_thread::sleep_for(500ms);
            c0 = probe::counters();
            stalls0 = probe::stalls().size();
            std::this_thread::sleep_for(2500ms);
            c1 = probe::counters();
            stalls1 = probe::stalls().size();
        });
        spinFor(3500ms);
        sampler.join();
        qInfo("rảnh 2,5 s: watchdog thức %llu lần, %llu sự kiện",
              (unsigned long long)(c1.watchdogWakeups - c0.watchdogWakeups), (unsigned long long)(c1.events - c0.events));
        QCOMPARE(stalls1, stalls0);
        QCOMPARE(c1.events, c0.events);
        QCOMPARE(c1.watchdogWakeups, c0.watchdogWakeups);
    }

    // (4) JSON có count, p50, p99 cho mọi loại thao tác của bảng DoD-L.
    void jsonHasEveryOp()
    {
        LagWidget w;
        QVERIFY(showAndSettle(w));
        probe::enable();
        QWindow* win = w.windowHandle();

        // Tự đo.
        QVERIFY(w.paintAfter([&] { QTest::keyClick(win, Qt::Key_A); }));        // typing
        QVERIFY(w.paintAfter([&] { QTest::keyClick(win, Qt::Key_Escape); }));   // ui_state
        QVERIFY(w.paintAfter([&] { QTest::mousePress(win, Qt::LeftButton, {}, QPoint(10, 60)); }));
        QVERIFY(w.paintAfter([&] { QTest::mouseMove(win, QPoint(40, 80)); })); // scroll_frame: kéo khi giữ nút
        QTest::mouseRelease(win, Qt::LeftButton, {}, QPoint(40, 80));
        spinFor(20ms);
        QMenu menu;
        menu.addAction(QString());
        menu.popup(win->mapToGlobal(QPoint(5, 5)));
        QVERIFY(spinUntil([] { return count("menu_open") > 0; }));
        menu.hide();
        QDialog dialog;
        dialog.resize(200, 100);
        dialog.show();
        QVERIFY(spinUntil([] { return count("dialog_open") > 0; }));
        dialog.hide();

        // Do nơi sở hữu đo.
        probe::record(Op::OsnapHitTest, 200us);
        { probe::Scope s(Op::JigPreview); }
        probe::record(Op::ViewportFrame, 8ms);
        { probe::Scope s(Op::CommandComplete); }
        { probe::Scope s(Op::PaletteUpdate); }
        probe::record(Op::WarmStart, 900ms);
        probe::untilFrame(Op::Autocomplete, &w);
        probe::untilFrame(Op::PaletteOpen, &w);
        QVERIFY(w.paintAfter([&] { w.update(); }));

        const QJsonObject j = probe::toJson();
        QCOMPARE(QJsonDocument::fromJson(QJsonDocument(j).toJson()).object(), j);
        const QJsonObject ops = j["ops"].toObject();
        QCOMPARE(ops.size(), qsizetype(Op::Count));
        for (int i = 0; i < int(Op::Count); ++i) {
            const char* n = probe::name(Op(i));
            const QJsonObject o = ops[QLatin1StringView(n)].toObject();
            QVERIFY2(o["count"].toInt() >= 1, n);
            QVERIFY2(o["p50_ms"].isDouble() && o["p99_ms"].isDouble() && o["max_ms"].isDouble(), n);
            QVERIFY2(o["p50_ms"].toDouble() <= o["p99_ms"].toDouble(), n);
            QVERIFY2(o["budget_ms"].toDouble() > 0 && o["budget_low_ms"].toDouble() >= o["budget_ms"].toDouble(), n);
        }
        QVERIFY(ops["scroll_frame"]["fps_p50"].isDouble());
        QVERIFY(j["stalls"]["count"].isDouble() && j["stalls"]["items"].isArray());
        qInfo("%s", QJsonDocument(ops).toJson(QJsonDocument::Compact).constData());

        // p50/p99 theo nearest-rank: 1..100 ms → 50 và 99.
        probe::reset();
        for (int k = 1; k <= 100; ++k)
            probe::record(Op::ViewportFrame, std::chrono::milliseconds(k));
        const QJsonValue vf = json()["ops"]["viewport_frame"];
        QCOMPARE(vf["p50_ms"].toDouble(), 50.0);
        QCOMPARE(vf["p99_ms"].toDouble(), 99.0);
        QCOMPARE(json()["ops"]["typing"]["p50_ms"].type(), QJsonValue::Null);
    }
};

QTEST_MAIN(UiProbeTest)
#include "ui_probe_test.moc"
