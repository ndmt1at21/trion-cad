#include "app/core/perf/UiProbe.h"

#include "app/core/UiExecutor.h"
#include "core/base/tsan.h"

#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QDialog>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QMenu>
#include <QMetaEnum>
#include <QMouseEvent>
#include <QPointer>
#include <QWindow>

#include <algorithm>
#include <array>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>
#include <utility>

#if defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace trion::ui::probe {

namespace detail {
std::atomic<bool> g_on{false};
} // namespace detail

namespace {

using std::chrono::nanoseconds;
using Ns = std::int64_t; // mốc steady_clock tính bằng ns, so sánh bằng được giữa hai luồng

constexpr std::size_t kOps = static_cast<std::size_t>(Op::Count);

struct OpInfo {
    const char* name;
    double budgetMs;    // p99, máy tầm trung (README §3 DoD-L)
    double budgetLowMs; // p99, máy thấp
};
constexpr std::array<OpInfo, kOps> kInfo{{
    {"osnap_hit_test", 1, 3},
    {"jig_preview", 8, 16},
    {"viewport_frame", 16.6, 33.3},
    {"command_complete", 20, 50},
    {"ui_state", 100, 100},
    {"menu_open", 100, 100},
    {"typing", 16, 33},
    {"autocomplete", 16, 33},
    {"dialog_open", 150, 300},
    {"palette_open", 150, 300},
    {"palette_update", 16, 33},
    {"scroll_frame", 16.6, 33.3},
    {"warm_start", 1500, 3000},
}};

Ns ns(Clock::time_point t) { return std::chrono::duration_cast<nanoseconds>(t.time_since_epoch()).count(); }
double ms(Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); }

// Tổng thời gian luồng tạo đối tượng nằm hàng chờ CPU (sẵn sàng chạy mà chưa được chạy), ns; đọc được từ mọi luồng.
// Khựng = đồng hồ thật trừ phần này: máy tải (agent, build song song) không thành khựng giả, còn chạy, chờ khóa, I/O,
// sleep vẫn tính (giao diện đứng thật). Linux: cột 2 của /proc/thread-self/schedstat.
// ponytail: Windows, macOS trả 0 (đồng hồ thật); tách thời gian chờ CPU ở đó khi cần (QueryThreadCycleTime,
// thread_info).
// Mẫu đọc cũ hơn mốc sự kiện tối đa kWaitSample: luồng UI đang chạy lúc đọc nên mọi lần chờ CPU cộng vào sau đó nằm
// trọn trong khoảng ấy, thời gian khựng thiếu tối đa kWaitSample.
constexpr Ns kWaitSample = 1'000'000;

class RunQueue {
public:
    RunQueue()
    {
#if defined(__linux__)
        fd_ = ::open("/proc/thread-self/schedstat", O_RDONLY | O_CLOEXEC);
#endif
    }
    ~RunQueue()
    {
#if defined(__linux__)
        if (fd_ >= 0)
            ::close(fd_);
#endif
    }
    RunQueue(const RunQueue&) = delete;
    RunQueue& operator=(const RunQueue&) = delete;

    Ns waited() const
    {
#if defined(__linux__)
        char buf[96];
        const ssize_t n = fd_ >= 0 ? ::pread(fd_, buf, sizeof buf - 1, 0) : -1; // pread: an toàn giữa hai luồng
        unsigned long long run = 0, wait = 0;
        if (n > 0) {
            buf[n] = 0;
            if (std::sscanf(buf, "%llu %llu", &run, &wait) == 2)
                return Ns(wait);
        }
#endif
        return 0;
    }

#if defined(__linux__)
private:
    int fd_ = -1;
#endif
};

// Mẫu, mọi luồng ghi.
// ponytail: giữ nguyên mẫu (tối đa 2^20 mỗi loại, ~8 MB); đổi sang histogram cố định nếu cần bật nhiều ngày.
constexpr std::size_t kMaxSamples = std::size_t(1) << 20;
struct Samples {
    std::mutex m;
    std::array<std::vector<double>, kOps> ms;
};
Samples g_samples;

void addSample(Op op, Clock::duration d)
{
    std::lock_guard lk(g_samples.m);
    auto& v = g_samples.ms[std::size_t(op)];
    if (v.size() < kMaxSamples)
        v.push_back(ms(d));
}

// Khựng. Một lần khựng nhận diện bằng mốc bắt đầu (start): luồng watchdog tạo bản ghi khi đang khựng, luồng UI
// chốt độ dài (hoặc tự tạo nếu watchdog chưa kịp thấy).
constexpr std::size_t kMaxStalls = 1000;
struct StallRec {
    Ns start;
    Clock::duration duration;
    std::string where;
    bool live;
};
struct Stalls {
    std::mutex m;
    std::vector<StallRec> items;
    std::uint64_t count = 0;
    Ns lastStart = -1;
};
Stalls g_stalls;

std::atomic<Ns> g_thresholdNs{50'000'000};
std::atomic<std::uint64_t> g_wakeups{0};
std::atomic<std::uint64_t> g_events{0};

// Vùng đo trong cùng đang mở của mỗi luồng; watchdog đọc vùng của luồng UI qua g_uiZone.
constinit thread_local std::atomic<const char*> t_zone{nullptr};
std::atomic<std::atomic<const char*>*> g_uiZone{nullptr};

std::string describe(int type, const char* cls, const char* zone)
{
    std::string s;
    if (cls) {
        const char* key = QMetaEnum::fromType<QEvent::Type>().valueToKey(type);
        s = key ? key : std::to_string(type);
        s += " -> ";
        s += cls;
    } else {
        s = "event loop";
    }
    if (zone) {
        s += " [";
        s += zone;
        s += ']';
    }
    return s;
}

// ponytail: báo ra stderr; chuyển sang log NT-009 khi có.
void print(const char* prefix, Clock::duration d, const std::string& where)
{
    std::fprintf(stderr, "trion ui-probe: luồng UI khựng %s%.1f ms: %s\n", prefix, ms(d), where.c_str());
}

// Luồng UI, khi đã chạy tiếp sau lần khựng bắt đầu ở start.
void stallEnded(Ns start, Clock::duration d, int type, const char* cls, const char* zone)
{
    std::string where = describe(type, cls, zone);
    {
        std::lock_guard lk(g_stalls.m);
        if (g_stalls.lastStart == start) { // watchdog đã báo khi đang khựng: chốt độ dài
            if (!g_stalls.items.empty() && g_stalls.items.back().start == start) {
                StallRec& rec = g_stalls.items.back();
                rec.duration = d;
                // Watchdog chỉ thấy vùng đo đang mở lúc nó thức: vùng vừa đóng mà luồng UI chưa tới mốc tiến triển (máy
                // tải, watchdog thức trễ) thì mô tả lúc bắt thiếu vùng. Khi đó lấy mô tả lúc chốt, có vùng đã đi qua.
                if (zone && rec.where.find(" [") == std::string::npos)
                    rec.where = where;
                else
                    where = rec.where;
            }
        } else {
            g_stalls.lastStart = start;
            ++g_stalls.count;
            if (g_stalls.items.size() < kMaxStalls)
                g_stalls.items.push_back({start, d, where, false});
        }
    }
    print("", d, where);
}

// Luồng UI, khi lần bắt đầu ở start dài quá ngưỡng theo đồng hồ thật nhưng phần lớn là chờ CPU. Linux chỉ cộng một lần
// chờ CPU vào schedstat khi luồng được chạy lại, nên watchdog đọc giữa lúc luồng UI đang chờ thấy thiếu và có thể đã
// báo nhầm: bỏ bản ghi đó. Ghi start lại để watchdog thức muộn không báo lần này nữa.
void stallRetracted(Ns start)
{
    bool reported = false;
    {
        std::lock_guard lk(g_stalls.m);
        if (g_stalls.lastStart == start) {
            reported = true;
            --g_stalls.count;
            if (!g_stalls.items.empty() && g_stalls.items.back().start == start)
                g_stalls.items.pop_back();
        }
        g_stalls.lastStart = start;
    }
    if (reported)
        std::fprintf(stderr, "trion ui-probe: bỏ lần khựng vừa báo: luồng UI chủ yếu chờ CPU (máy tải)\n");
}

// Luồng watchdog, khi luồng UI vẫn đang ở trong lần khựng bắt đầu ở start.
void stallLive(Ns start, Clock::duration d, std::string where, const std::atomic<Ns>& progress)
{
    {
        std::lock_guard lk(g_stalls.m);
        // Luồng UI chốt lần khựng trước khi ghi mốc mới, nên mốc còn bằng start nghĩa là nó chưa chạy tiếp.
        if (g_stalls.lastStart == start || progress.load(std::memory_order_acquire) != start)
            return;
        g_stalls.lastStart = start;
        ++g_stalls.count;
        if (g_stalls.items.size() < kMaxStalls)
            g_stalls.items.push_back({start, d, where, true});
    }
    print("≥ ", d, where + " (đang chạy)");
}

QEvent::Type markerType()
{
    static const auto type = QEvent::Type(QEvent::registerEventType());
    return type;
}

// Probe đang bật; chỉ luồng UI đọc/ghi.
class Probe;
Probe* g_probe = nullptr;

class Probe final : public QObject {
public:
    Probe();
    ~Probe() override;

    bool eventFilter(QObject* o, QEvent* e) override;
    bool event(QEvent* e) override;

    void untilFrame(Op op, const QWindow* w, Clock::time_point start);
    void zoneLeft(const char* zone, Clock::time_point now);
    bool dispatching() const { return redispatching_ != nullptr; }

    QString jsonPath;

private:
    struct Pending {
        const QWindow* window; // chỉ so sánh, không truy cập
        Clock::time_point start;
        Op op;
        bool input; // do input: bỏ khi vòng sự kiện đã cạn việc mà chưa có khung
    };

    bool frame(QObject* o, QEvent* e);
    void frameDone(const QWindow* w, Clock::time_point end);
    void add(Op op, const QWindow* w, Clock::time_point start, bool input);
    bool has(Op op, const QWindow* w) const;
    Clock::time_point inputStart(Clock::time_point now) const;
    void drop(const QWindow* w, bool inputOnly);
    void loopPoint(bool block);
    void progress(Clock::time_point now, int type, const char* cls);
    void checkStall(Ns now);
    void watch();

    const RunQueue runQueue_; // của luồng UI (tạo trong enable(), trên luồng UI)

    // Luồng UI.
    std::array<Pending, 64> pend_{};
    std::size_t npend_ = 0;
    QEvent* redispatching_ = nullptr;
    std::uint64_t events_ = 0;
    std::uint64_t markerEvents_ = 0;
    bool markerQueued_ = false;
    bool busy_ = false; // đang xử lý sự kiện (từ sự kiện đầu sau aboutToBlock đến aboutToBlock kế)
    Ns last_ = 0;       // mốc tiến triển gần nhất: đầu một sự kiện hoặc một lượt vòng sự kiện
    Ns lastWait_ = 0;   // runQueue_.waited() đọc ở waitAt_ (≤ last_)
    Ns waitAt_ = -kWaitSample;
    int lastType_ = 0;
    const char* lastCls_ = nullptr;
    Ns stickyAt_ = -1;  // vùng đo đã vượt ngưỡng trong lần khựng bắt đầu ở stickyAt_
    const char* sticky_ = nullptr;

    // Chung với luồng watchdog.
    std::atomic<Ns> progress_{0};
    std::atomic<Ns> wait_{0}; // lastWait_, ghi trước progress_
    std::atomic<int> type_{0};
    std::atomic<const char*> cls_{nullptr};
    std::atomic<bool> idle_{true};
    std::mutex m_;
    std::condition_variable cv_;
    std::uint64_t armSeq_ = 0; // tăng mỗi lần luồng UI từ rảnh sang bận (giữ m_)
    bool sleeping_ = false;    // watchdog đang ngủ không thời hạn (giữ m_)
    bool quit_ = false;
    // ponytail: luồng riêng là ngoại lệ có lý do của DoD-H 1 (không tự tạo std::thread): watchdog phải chạy
    // được khi luồng UI và cả bộ lập lịch bị chặn. Đề nghị NT-005 có API luồng chuyên dụng (tên, ưu tiên) để thay.
    std::thread thread_;
};

Probe::Probe()
    : QObject(QCoreApplication::instance())
{
    g_uiZone.store(&t_zone, std::memory_order_relaxed);
    QCoreApplication::instance()->installEventFilter(this);
    auto* dispatcher = QAbstractEventDispatcher::instance();
    connect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, this, [this] { loopPoint(true); });
    connect(dispatcher, &QAbstractEventDispatcher::awake, this, [this] { loopPoint(false); });
    thread_ = std::thread([this] { watch(); });
}

Probe::~Probe()
{
    if (g_probe == this) { // ứng dụng hủy khi còn bật
        g_probe = nullptr;
        detail::g_on.store(false, std::memory_order_relaxed);
    }
    {
        std::lock_guard lk(m_);
        quit_ = true;
    }
    cv_.notify_one();
    thread_.join();
    if (!jsonPath.isEmpty()) {
        QFile f(jsonPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(QJsonDocument(toJson()).toJson()) < 0)
            std::fprintf(stderr, "trion ui-probe: không ghi được %s\n", qPrintable(jsonPath));
    }
}

bool Probe::eventFilter(QObject* o, QEvent* e)
{
    if (e == redispatching_ || this != g_probe)
        return false;
    const auto now = Clock::now();
    TRION_TSAN_ACQUIRE(e); // sự kiện đánh thức của UiExecutor dựng ở luồng nền (core/base/tsan.h)
    const auto type = e->type();
    ++events_;
    g_events.store(g_events.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
    progress(now, type, o->metaObject()->className());

    // Input tính một lần ở QWindow (cửa sổ nhận từ hệ thống), không tính lần chuyển tiếp xuống widget.
    auto* w = o->isWindowType() ? static_cast<QWindow*>(o) : nullptr;
    switch (type) {
    case QEvent::KeyPress:
        if (w) {
            const auto* k = static_cast<QKeyEvent*>(e);
            const QString text = k->text();
            const bool typed = !text.isEmpty() && text.front().isPrint()
                && !(k->modifiers() & (Qt::ControlModifier | Qt::MetaModifier));
            add(typed ? Op::Typing : Op::UiState, w, now, true);
        }
        break;
    case QEvent::MouseButtonPress: // DblClick luôn theo sau một Press: không tính lại
    case QEvent::MouseButtonRelease:
        if (w)
            add(Op::UiState, w, now, true);
        break;
    case QEvent::MouseMove:
        if (w && static_cast<QMouseEvent*>(e)->buttons() != Qt::NoButton && !has(Op::ScrollFrame, w))
            add(Op::ScrollFrame, w, now, true);
        break;
    case QEvent::Wheel:
    case QEvent::Resize: // chỉ khi cửa sổ đã hiện: lần đặt kích thước lúc mở không phải thao tác kéo
        if (w && w->isExposed() && !has(Op::ScrollFrame, w))
            add(Op::ScrollFrame, w, now, true);
        break;
    case QEvent::Show:
    case QEvent::Hide:
        if (o->isWidgetType() && static_cast<QWidget*>(o)->isWindow()) {
            const QWindow* handle = static_cast<QWidget*>(o)->windowHandle();
            if (type == QEvent::Hide)
                drop(handle, false);
            else if (qobject_cast<QMenu*>(o))
                add(Op::MenuOpen, handle, inputStart(now), false);
            else if (qobject_cast<QDialog*>(o))
                add(Op::DialogOpen, handle, inputStart(now), false);
        }
        break;
    case QEvent::UpdateRequest:
    case QEvent::Expose:
        return frame(o, e);
    default:
        break;
    }
    return false;
}

// Khung hình: giao lại chính sự kiện ngay trong filter để biết lúc vẽ + flush xong (filter chỉ chạy trước khi
// giao), rồi trả true để Qt không giao lần nữa. Filter ứng dụng cài sau probe (chạy trước nó) thấy sự kiện này
// hai lần.
bool Probe::frame(QObject* o, QEvent* e)
{
    QWindow* w = nullptr;
    if (o->isWindowType())
        w = static_cast<QWindow*>(o);
    else if (o->isWidgetType() && static_cast<QWidget*>(o)->isWindow())
        w = static_cast<QWidget*>(o)->windowHandle();
    if (!w)
        return false;
    const QPointer<QWindow> alive(w);
    QEvent* const outer = redispatching_;
    redispatching_ = e;
    QCoreApplication::sendEvent(o, e);
    redispatching_ = outer;
    const auto end = Clock::now();
    // Expose khi cửa sổ bị che hoặc ẩn không vẽ gì.
    if (this == g_probe && alive && (e->type() == QEvent::UpdateRequest || alive->isExposed()))
        frameDone(w, end);
    return true;
}

void Probe::frameDone(const QWindow* w, Clock::time_point end)
{
    for (std::size_t i = 0; i < npend_;) {
        if (pend_[i].window != w) {
            ++i;
            continue;
        }
        addSample(pend_[i].op, end - pend_[i].start);
        pend_[i] = pend_[--npend_];
    }
}

void Probe::add(Op op, const QWindow* w, Clock::time_point start, bool input)
{
    if (w && npend_ < pend_.size()) // ponytail: quá 64 mẫu chờ khung thì bỏ mẫu mới
        pend_[npend_++] = {w, start, op, input};
}

bool Probe::has(Op op, const QWindow* w) const
{
    return std::any_of(pend_.begin(), pend_.begin() + npend_, [&](const Pending& p) { return p.op == op && p.window == w; });
}

// Mốc bắt đầu cho thao tác mở: phím/nhấp gần nhất còn chờ khung (thường chính là cú nhấp mở nó).
Clock::time_point Probe::inputStart(Clock::time_point now) const
{
    Clock::time_point t{};
    for (std::size_t i = 0; i < npend_; ++i)
        if (pend_[i].op == Op::Typing || pend_[i].op == Op::UiState)
            t = std::max(t, pend_[i].start);
    return t == Clock::time_point{} ? now : t;
}

void Probe::drop(const QWindow* w, bool inputOnly)
{
    for (std::size_t i = 0; i < npend_;) {
        if (inputOnly ? pend_[i].input : pend_[i].window == w)
            pend_[i] = pend_[--npend_];
        else
            ++i;
    }
}

void Probe::untilFrame(Op op, const QWindow* w, Clock::time_point start)
{
    const auto now = Clock::now();
    add(op, w, start == Clock::time_point{} ? inputStart(now) : start, false);
}

// Mốc tiến triển của luồng UI: đầu mỗi sự kiện (mọi mức lồng) và mỗi lượt vòng sự kiện.
void Probe::progress(Clock::time_point now, int type, const char* cls)
{
    const Ns t = ns(now);
    if (busy_)
        checkStall(t);
    last_ = t;
    if (t - waitAt_ >= kWaitSample) { // đọc tối đa mỗi ms (pread procfs vài µs khi máy tải), không mỗi sự kiện
        lastWait_ = runQueue_.waited();
        waitAt_ = t;
    }
    lastType_ = type;
    lastCls_ = cls;
    type_.store(type, std::memory_order_relaxed);
    cls_.store(cls, std::memory_order_relaxed);
    wait_.store(lastWait_, std::memory_order_relaxed);
    progress_.store(t, std::memory_order_release);
    if (!busy_) { // rảnh → bận: đánh thức watchdog nếu nó đang ngủ (một lần mỗi đợt bận, không mỗi sự kiện)
        busy_ = true;
        idle_.store(false, std::memory_order_release);
        bool wake = false;
        {
            std::lock_guard lk(m_);
            ++armSeq_;
            wake = sleeping_;
        }
        if (wake)
            cv_.notify_one();
    }
}

void Probe::checkStall(Ns now)
{
    const Ns threshold = g_thresholdNs.load(std::memory_order_relaxed);
    if (now - last_ < threshold) // đồng hồ thật chưa tới thì thời gian không chờ CPU càng chưa
        return;
    const Ns d = now - last_ - (runQueue_.waited() - lastWait_); // đang chạy: thời gian chờ đã cộng đủ
    if (d < threshold) { // phần lớn là chờ CPU (máy tải): không phải khựng của ứng dụng
        stallRetracted(last_);
        return;
    }
    const char* zone = stickyAt_ == last_ ? sticky_ : t_zone.load(std::memory_order_relaxed);
    stallEnded(last_, nanoseconds(d), lastType_, lastCls_, zone);
}

void Probe::zoneLeft(const char* zone, Clock::time_point now)
{
    if (busy_ && stickyAt_ != last_ && ns(now) - last_ >= g_thresholdNs.load(std::memory_order_relaxed)) {
        stickyAt_ = last_; // vùng trong cùng ra trước nên được giữ
        sticky_ = zone;
    }
}

// awake (block = false) và aboutToBlock (block = true). Với dispatcher glib, aboutToBlock phát ở đầu mỗi lượt
// kể cả khi còn việc, nên "rảnh" ở đây chỉ để watchdog ngủ; sự kiện kế tiếp đánh thức lại.
void Probe::loopPoint(bool block)
{
    if (this != g_probe)
        return;
    if (busy_) {
        if (block) {
            checkStall(ns(Clock::now()));
            busy_ = false;
            idle_.store(true, std::memory_order_release);
        } else {
            progress(Clock::now(), QEvent::None, nullptr);
        }
    }
    // Input còn chờ khung: gửi một marker ưu tiên thấp. Khi nó tới mà không sự kiện nào khác chen vào thì mọi
    // việc đã đăng trước (gồm UpdateRequest do input gây ra) đã xong mà không có khung: bỏ mẫu (Probe::event).
    const bool waiting = std::any_of(pend_.begin(), pend_.begin() + npend_, [](const Pending& p) { return p.input; });
    if (waiting && !markerQueued_) {
        markerQueued_ = true;
        markerEvents_ = events_;
        QCoreApplication::postEvent(this, new QEvent(markerType()), Qt::LowEventPriority);
    }
}

bool Probe::event(QEvent* e)
{
    if (e->type() != markerType())
        return QObject::event(e);
    markerQueued_ = false;
    if (events_ - markerEvents_ <= 1) // chỉ có chính marker
        drop(nullptr, true);
    return true;
}

void Probe::watch()
{
    std::unique_lock lk(m_);
    std::uint64_t seen = armSeq_;
    Ns reported = -1;
    while (!quit_) {
        if (idle_.load(std::memory_order_acquire)) {
            if (armSeq_ == seen) { // rảnh: ngủ đến khi luồng UI bận lại
                sleeping_ = true;
                cv_.wait(lk, [&] { return quit_ || armSeq_ != seen; });
                sleeping_ = false;
                g_wakeups.fetch_add(1, std::memory_order_relaxed);
            }
            seen = armSeq_;
            continue;
        }
        seen = armSeq_;
        const Ns p = progress_.load(std::memory_order_acquire);
        const Ns wait = wait_.load(std::memory_order_relaxed);
        const Ns now = ns(Clock::now());
        const Ns threshold = g_thresholdNs.load(std::memory_order_relaxed);
        const Ns d = now - p - (runQueue_.waited() - wait); // không tính thời gian luồng UI chờ CPU
        if (d >= threshold && p != reported) {
            reported = p;
            std::string where = describe(type_.load(std::memory_order_relaxed), cls_.load(std::memory_order_relaxed),
                                         g_uiZone.load(std::memory_order_relaxed)->load(std::memory_order_relaxed));
            lk.unlock();
            stallLive(p, nanoseconds(d), std::move(where), progress_);
            lk.lock();
            continue;
        }
        // Chưa tới ngưỡng: ngủ đến sớm nhất lúc có thể tới (thời gian không chờ CPU tăng chậm hơn hoặc bằng đồng hồ
        // thật). Đã báo lần khựng này: xem lại sau một ngưỡng (chỉ khi UI vẫn bận).
        const Ns due = d >= threshold ? now + threshold : now + (threshold - d);
        cv_.wait_until(lk, Clock::time_point(nanoseconds(due)), [&] { return quit_; });
        g_wakeups.fetch_add(1, std::memory_order_relaxed);
    }
}

double percentile(std::vector<double>& v, double p) // nearest-rank; v khác rỗng, bị sắp lại
{
    const auto k = static_cast<std::size_t>(std::ceil(p * double(v.size()))) - 1;
    std::nth_element(v.begin(), v.begin() + std::ptrdiff_t(k), v.end());
    return v[k];
}

} // namespace

const char* name(Op op)
{
    return op < Op::Count ? kInfo[std::size_t(op)].name : "?";
}

bool configure(const QStringList& args)
{
    for (const QString& a : args) {
        if (a == u"--ui-probe") {
            enable();
        } else if (a.startsWith(u"--ui-probe=")) {
            enable();
            if (g_probe)
                g_probe->jsonPath = a.mid(int(sizeof("--ui-probe=") - 1));
        }
    }
    return enabled();
}

void enable()
{
    TRION_ASSERT_UI_THREAD();
    if (g_probe || !QCoreApplication::instance())
        return;
    g_probe = new Probe;
    detail::g_on.store(true, std::memory_order_relaxed);
}

void disable()
{
    TRION_ASSERT_UI_THREAD();
    Probe* p = std::exchange(g_probe, nullptr);
    if (!p)
        return;
    detail::g_on.store(false, std::memory_order_relaxed);
    if (p->dispatching()) // gọi từ trong một khung probe đang giao lại
        p->deleteLater();
    else
        delete p;
}

void setStallThreshold(std::chrono::milliseconds threshold)
{
    g_thresholdNs.store(std::chrono::duration_cast<nanoseconds>(threshold).count(), std::memory_order_relaxed);
}

void record(Op op, Clock::duration duration)
{
    if (enabled() && op < Op::Count)
        addSample(op, duration);
}

void untilFrame(Op op, const QWidget* widget, Clock::time_point start)
{
    if (!enabled())
        return;
    TRION_ASSERT_UI_THREAD();
    const QWindow* w = widget ? widget->window()->windowHandle() : nullptr;
    if (g_probe && w && op < Op::Count)
        g_probe->untilFrame(op, w, start);
}

void Scope::enter(const char* zone, Op op) noexcept
{
    zone_ = zone ? zone : "?";
    op_ = op;
    prev_ = t_zone.load(std::memory_order_relaxed); // chỉ luồng này ghi t_zone
    t_zone.store(zone_, std::memory_order_relaxed);
    if (op != Op::Count)
        start_ = Clock::now();
}

void Scope::leave() noexcept
{
    t_zone.store(prev_, std::memory_order_relaxed);
    const bool ui = &t_zone == g_uiZone.load(std::memory_order_relaxed);
    if (op_ == Op::Count && !ui)
        return;
    const auto now = Clock::now();
    if (op_ != Op::Count)
        record(op_, now - start_);
    if (ui && g_probe)
        g_probe->zoneLeft(zone_, now);
}

std::vector<Stall> stalls()
{
    std::lock_guard lk(g_stalls.m);
    std::vector<Stall> out;
    out.reserve(g_stalls.items.size());
    for (const auto& r : g_stalls.items)
        out.push_back({r.duration, r.where, r.live});
    return out;
}

QJsonObject toJson()
{
    std::array<std::vector<double>, kOps> samples;
    {
        std::lock_guard lk(g_samples.m);
        samples = g_samples.ms;
    }
    QJsonObject ops;
    for (std::size_t i = 0; i < kOps; ++i) {
        auto& v = samples[i];
        QJsonObject o{{"count", qint64(v.size())},
                      {"budget_ms", kInfo[i].budgetMs},
                      {"budget_low_ms", kInfo[i].budgetLowMs},
                      {"p50_ms", QJsonValue::Null},
                      {"p99_ms", QJsonValue::Null},
                      {"max_ms", QJsonValue::Null}};
        if (!v.empty()) {
            const double p50 = percentile(v, 0.5);
            const double p99 = percentile(v, 0.99);
            o["p50_ms"] = p50;
            o["p99_ms"] = p99;
            o["max_ms"] = *std::max_element(v.begin(), v.end());
            if (Op(i) == Op::ScrollFrame) {
                o["fps_p50"] = p50 > 0 ? 1000 / p50 : 0.0;
                o["fps_p99"] = p99 > 0 ? 1000 / p99 : 0.0;
            }
        }
        ops[QLatin1StringView(kInfo[i].name)] = o;
    }
    QJsonArray items;
    std::uint64_t count = 0;
    {
        std::lock_guard lk(g_stalls.m);
        count = g_stalls.count;
        for (const auto& r : g_stalls.items)
            items.append(QJsonObject{{"ms", ms(r.duration)}, {"where", QString::fromStdString(r.where)}, {"live", r.live}});
    }
    const QJsonObject stallObj{{"threshold_ms", double(g_thresholdNs.load(std::memory_order_relaxed)) / 1e6},
                               {"count", qint64(count)},
                               {"items", items}};
    return {{"format", "trion-ui-probe/1"}, {"ops", ops}, {"stalls", stallObj}};
}

void reset()
{
    {
        std::lock_guard lk(g_samples.m);
        for (auto& v : g_samples.ms)
            v.clear();
    }
    std::lock_guard lk(g_stalls.m);
    g_stalls.items.clear();
    g_stalls.count = 0;
    g_stalls.lastStart = -1;
}

Counters counters()
{
    return {g_wakeups.load(std::memory_order_relaxed), g_events.load(std::memory_order_relaxed)};
}

} // namespace trion::ui::probe
