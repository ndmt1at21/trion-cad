#include "app/core/UiExecutor.h"

#include "core/base/sched/Scheduler.h"

#include <QCoreApplication>
#include <QEvent>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <deque>
#include <exception>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace trion::ui {
namespace {

using Clock = std::chrono::steady_clock;

// Ngừng nhận callback mới sau 3 ms để cả lượt, kể cả callback cuối và phần dọn, ≤ 4 ms (UI0-101 tiêu chí 1).
constexpr auto kBudget = std::chrono::milliseconds(3);

QEvent::Type wakeType()
{
    static const auto type = QEvent::Type(QEvent::registerEventType());
    return type;
}

struct Item {
    std::function<void()> fn{};
    QPointer<const QObject> receiver{};
    bool tracked = false;   // có receiver: bỏ khi receiver đã hủy
    std::uint64_t key = 0;  // khác 0: chỗ giữ của postLatest, giá trị mới nhất nằm ở Shared::latest
};

// Phần các luồng gửi chạm tới, giữ bằng std::mutex (TSan thấy được; Qt bản cài sẵn thì không).
// Hàng đợi sự kiện của Qt chỉ mang tín hiệu đánh thức, không mang dữ liệu.
// ponytail: một mutex cho mọi luồng gửi, giữ vài chục ns; đổi sang hàng MPSC không khóa nếu đo thấy tranh chấp.
struct Shared {
    std::mutex m;
    UiExecutor* target = nullptr;
    std::deque<Item> incoming;
    std::unordered_map<std::uint64_t, Item> latest;
    bool wakePending = false; // đã đăng sự kiện đánh thức mà lượt chưa dọn xong hàng
    std::uint64_t wakeups = 0;
};
Shared g;

// Phần chỉ luồng UI chạm tới.
std::deque<Item> g_ready; // đã lấy khỏi incoming, chưa chạy (luôn cũ hơn mọi thứ còn trong incoming)
UiExecutor::Stats g_stats;

std::atomic<std::uint64_t> g_nextKey{1}; // khóa Receiver, không dùng lại

void wakeLocked(QObject* target) // giữ g.m: target không thể bị hủy giữa chừng
{
    ++g.wakeups;
    QCoreApplication::postEvent(target, new QEvent(wakeType()));
}

void enqueue(Item item)
{
    Item replaced; // hủy sau khi nhả khóa: closure bị hủy có thể gọi post() lần nữa
    std::lock_guard lock(g.m);
    TRION_ASSERT(g.target); // chưa có executor hoặc đã hủy: lỗi lập trình, kết quả bị bỏ
    if (!g.target)
        return;
    if (item.key) {
        auto [it, fresh] = g.latest.try_emplace(item.key);
        replaced = std::exchange(it->second, std::move(item));
        if (!fresh)
            return; // chỗ giữ đã nằm trong hàng, chỉ thay giá trị
        g.incoming.push_back(Item{{}, {}, false, it->first});
    } else {
        g.incoming.push_back(std::move(item));
    }
    if (!g.wakePending) {
        g.wakePending = true;
        wakeLocked(g.target);
    }
}

void run(const std::function<void()>& fn)
{
    try {
        fn();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "trion: callback luồng UI lỗi: %s\n", e.what());
    } catch (...) {
        std::fprintf(stderr, "trion: callback luồng UI lỗi\n");
    }
}

} // namespace

bool isUiThread()
{
    const QCoreApplication* app = QCoreApplication::instance();
    return app && app->thread() == QThread::currentThread();
}

Expected<void> requireUiThread()
{
    const bool onUiThread = isUiThread();
    TRION_ASSERT(onUiThread);
    if (onUiThread)
        return {};
    return Error{"ui", "not-ui-thread", "API ghi chỉ được gọi trên luồng UI"};
}

Receiver::Receiver(const QObject* object)
    : object_(object), key_(g_nextKey.fetch_add(1, std::memory_order_relaxed))
{
    TRION_ASSERT_UI_THREAD();
    TRION_ASSERT(object && object->thread() == QThread::currentThread());
}

void post(std::function<void()> fn)
{
    TRION_ASSERT(fn);
    enqueue(Item{std::move(fn)});
}

void post(const Receiver& receiver, std::function<void()> fn)
{
    TRION_ASSERT(fn);
    enqueue(Item{std::move(fn), receiver.object_, true});
}

void postLatest(const Receiver& receiver, std::function<void()> fn)
{
    TRION_ASSERT(fn);
    enqueue(Item{std::move(fn), receiver.object_, true, receiver.key_});
}

UiExecutor::UiExecutor()
{
    TRION_ASSERT_UI_THREAD();
    {
        std::lock_guard lock(g.m);
        TRION_ASSERT(!g.target); // một executor cho cả tiến trình
        g.target = this;
        g.wakeups = 0;
    }
    g_stats = {};
    sched::setUiPoster([](std::function<void()> fn) { post(std::move(fn)); });
}

UiExecutor::~UiExecutor()
{
    TRION_ASSERT_UI_THREAD();
    sched::setUiPoster({});
    // Kết quả còn chờ bị bỏ; hủy ngoài khóa. Sự kiện đánh thức còn trong hàng Qt do ~QObject gỡ.
    std::deque<Item> incoming, ready;
    std::unordered_map<std::uint64_t, Item> latest;
    {
        std::lock_guard lock(g.m);
        g.target = nullptr;
        g.wakePending = false;
        incoming.swap(g.incoming);
        latest.swap(g.latest);
    }
    ready.swap(g_ready);
}

UiExecutor::Stats UiExecutor::stats() const
{
    TRION_ASSERT_UI_THREAD();
    Stats s = g_stats;
    std::lock_guard lock(g.m);
    s.wakeups = g.wakeups;
    return s;
}

// Chỉ sự kiện đánh thức tới đây (QObject::event chuyển mọi kiểu ≥ QEvent::User sang customEvent). Không đọc *e:
// luồng gửi cấp phát nó và chuyển qua hàng sự kiện của Qt, nơi TSan không thấy đồng bộ.
void UiExecutor::customEvent(QEvent*)
{
    const auto start = Clock::now();
    while (Clock::now() - start < kBudget) {
        if (g_ready.empty()) {
            std::lock_guard lock(g.m);
            g_ready.swap(g.incoming);
        }
        if (g_ready.empty())
            break;
        Item item = std::move(g_ready.front());
        g_ready.pop_front();
        if (item.key) {
            decltype(g.latest)::node_type node; // hủy ngoài khóa
            {
                std::lock_guard lock(g.m);
                node = g.latest.extract(item.key);
            }
            TRION_ASSERT(!node.empty()); // chỗ giữ trong hàng ⇔ khóa trong latest
            if (node.empty())
                continue;
            item = std::move(node.mapped());
        }
        if (item.tracked && !item.receiver) {
            ++g_stats.dropped;
            continue;
        }
        run(item.fn);
        ++g_stats.ran;
    }
    {
        std::lock_guard lock(g.m);
        if (g_ready.empty() && g.incoming.empty())
            g.wakePending = false; // hết việc: lần gửi sau đánh thức lại
        else
            wakeLocked(this); // còn việc: hẹn lượt sau, vòng sự kiện chạy việc khác trước
    }
    const auto spent = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start);
    g_stats.busy += spent;
    g_stats.longest = std::max(g_stats.longest, spent);
}

} // namespace trion::ui
