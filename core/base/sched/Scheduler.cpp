#include "core/base/sched/Scheduler.h"

#include "core/base/assert.h"

#include <algorithm>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

namespace trion::sched {
namespace {

// Pool tái dùng từ snapone/core/jobs.cpp (cùng tác giả): luồng riêng, hàng đợi theo ưu tiên.
// Đồng bộ bằng std::mutex/condition_variable nên TSan thấy được (QThreadPool thì không, vì Qt
// bản cài sẵn không biên dịch với TSan).
// ponytail: một mutex cho mọi hàng đợi; work-stealing và QoS nhân P/E do NT-105/NT-005.
class Pool {
public:
    Pool()
    {
        const unsigned n = std::max(2u, std::thread::hardware_concurrency());
        for (unsigned i = 0; i < n; ++i)
            threads_.emplace_back([this] { run(); });
    }
    ~Pool()
    {
        {
            std::lock_guard l(m_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& t : threads_)
            t.join();
    }
    void push(Priority p, std::function<void()> fn)
    {
        {
            std::lock_guard l(m_);
            q_[int(p)].push_back(std::move(fn));
            ++outstanding_;
        }
        cv_.notify_one();
    }
    void waitIdle()
    {
        std::unique_lock l(m_);
        idle_.wait(l, [&] { return outstanding_ == 0; });
    }

private:
    void run()
    {
        for (;;) {
            std::function<void()> fn;
            {
                std::unique_lock l(m_);
                int p = 0;
                cv_.wait(l, [&] {
                    for (p = 0; p < 3; ++p)
                        if (!q_[p].empty())
                            return true;
                    return stop_;
                });
                if (stop_)
                    return;
                fn = std::move(q_[p].front());
                q_[p].pop_front();
            }
            try {
                fn();
            } catch (const std::exception& e) {
                std::fprintf(stderr, "trion: việc nền lỗi: %s\n", e.what());
            } catch (...) {
                std::fprintf(stderr, "trion: việc nền lỗi\n");
            }
            fn = nullptr; // hủy closure trước khi báo xong
            std::lock_guard l(m_);
            if (--outstanding_ == 0)
                idle_.notify_all();
        }
    }

    std::mutex m_;
    std::condition_variable cv_, idle_;
    std::deque<std::function<void()>> q_[3]; // theo thứ tự Priority
    long outstanding_ = 0;                   // đang chờ + đang chạy
    bool stop_ = false;
    std::vector<std::thread> threads_;
};

Pool& pool()
{
    static Pool p;
    return p;
}

std::mutex g_posterMutex;
UiPoster g_poster;

} // namespace

void submit(Priority priority, std::function<void()> fn)
{
    pool().push(priority, std::move(fn));
}

void waitIdle()
{
    pool().waitIdle();
}

void setUiPoster(UiPoster poster)
{
    std::lock_guard lock(g_posterMutex);
    g_poster = std::move(poster);
}

void postToUi(std::function<void()> fn)
{
    UiPoster poster;
    {
        std::lock_guard lock(g_posterMutex);
        poster = g_poster;
    }
    TRION_ASSERT(poster);
    if (poster)
        poster(std::move(fn));
}

} // namespace trion::sched
