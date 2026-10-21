#include "core/base/jobs/BackgroundJobs.h"

#include <algorithm>
#include <atomic>
#include <exception>
#include <mutex>

namespace trion::jobs {
namespace {

// 100% của một việc tính bằng số nguyên: các phạm vi con cộng dồn đúng tuyệt đối, không lệch do làm tròn số thực.
constexpr std::int64_t kUnits = std::int64_t(1) << 40;

std::atomic<BackgroundJobs*> g_instance{nullptr};

} // namespace

namespace detail {

// Trường flush() đọc mỗi lô đứng đầu (chung dòng cache).
struct Job {
    std::atomic<std::int64_t> done{0}; // phần đã xong, đơn vị kUnits
    std::atomic<bool> dirty{true};     // có thay đổi chưa vào lô; việc mới cũng là thay đổi
    // Dưới Registry::m:
    State state = State::Running;
    bool started = false;                            // fn đã bắt đầu chạy
    bool cancelRequested = false;                    // = cancel.cancelled(), không chạm khối của CancelSource
    bool reportedBusy = false, reportedShow = false; // cờ đã báo trong lô
    JobId id = 0;
    Clock::time_point startedAt, finishedAt;
    // Không đổi sau start():
    QString name, kind;
    std::uint64_t owner = 0;
    Registry* registry = nullptr; // không sở hữu: closure của việc giữ shared_ptr tới registry
    sched::CancelSource cancel;
    Error error; // dưới Registry::m
};

struct Registry : std::enable_shared_from_this<Registry> {
    mutable std::mutex m;
    std::vector<std::shared_ptr<Job>> jobs; // tăng dần theo id (id cấp tăng dần): lô có thứ tự tất định
    JobId next = 1;
    bool wakePosted = false;      // đã đăng đánh thức luồng UI, chưa flush
    bool urgent = false;          // trong số thay đổi chờ có cái phải báo ngay
    BackgroundJobs* ui = nullptr; // chỉ luồng UI đọc, ghi

    // Dưới m: ghi nhận có thay đổi; trả true thì người gọi post() sau khi nhả m. Mỗi lô một lần đánh thức, thêm
    // một lần nếu thay đổi khẩn đến sau khi đã đánh thức (lần trước có thể đã hẹn theo nhịp 100 ms).
    bool markLocked(bool urgentChange)
    {
        const bool wake = !wakePosted || (urgentChange && !urgent);
        wakePosted = true;
        urgent = urgent || urgentChange;
        return wake;
    }

    void post()
    {
        sched::postToUi([w = weak_from_this()] {
            if (auto s = w.lock(); s && s->ui)
                s->ui->onWake();
        });
    }

    void changed(bool urgentChange)
    {
        bool wake;
        {
            std::lock_guard l(m);
            wake = markLocked(urgentChange);
        }
        if (wake)
            post();
    }

    // Dưới m.
    Job* find(JobId id) const
    {
        const auto it =
            std::lower_bound(jobs.begin(), jobs.end(), id, [](const auto& j, JobId v) { return j->id < v; });
        return it != jobs.end() && (*it)->id == id ? it->get() : nullptr;
    }

    // Dưới m. Yêu cầu hủy; false nếu việc đã kết thúc hoặc đã được yêu cầu hủy. Việc còn chờ thành Cancelled ngay.
    bool cancelLocked(Job& j)
    {
        if (j.state != State::Running || j.cancelRequested)
            return false;
        j.cancelRequested = true;
        j.cancel.cancel();
        if (!j.started) {
            j.state = State::Cancelled;
            j.finishedAt = Clock::now();
        }
        j.dirty.store(true, std::memory_order_relaxed);
        return true;
    }

    // Dưới m.
    static JobInfo snapshot(const Job& j, Clock::time_point now, const Timing& t)
    {
        JobInfo i;
        i.id = j.id;
        i.name = j.name;
        i.kind = j.kind;
        i.owner = j.owner;
        i.state = j.state;
        i.progress = j.state == State::Done ? 1.0 : double(j.done.load(std::memory_order_relaxed)) / double(kUnits);
        i.cancelRequested = j.cancelRequested;
        i.startedAt = j.startedAt;
        i.elapsed = (j.state == State::Running ? now : j.finishedAt) - j.startedAt;
        i.busyCursor = i.elapsed >= t.busyCursor;
        i.showProgress = i.elapsed >= t.showProgress;
        i.error = j.error;
        return i;
    }

    // Trên worker của pool.
    void run(Job& j, BackgroundJobs::Fn& fn)
    {
        {
            std::lock_guard l(m);
            if (j.state != State::Running)
                return; // đã hủy khi còn chờ
            j.started = true;
        }
        Expected<void> result;
        try {
            Progress root(&j, 0, kUnits);
            result = fn(root, j.cancel.token());
        } catch (const std::exception& e) {
            result = Error{"jobs", "exception", e.what()};
        } catch (...) {
            result = Error{"jobs", "exception", "Việc nền dừng vì lỗi không xác định"};
        }
        fn = nullptr; // hủy closure (và tài nguyên nó giữ) trước khi báo xong
        bool wake;
        {
            std::lock_guard l(m);
            j.finishedAt = Clock::now();
            const bool cancelled = j.cancelRequested;
            j.state = cancelled ? State::Cancelled : result ? State::Done : State::Failed;
            if (j.state == State::Failed)
                j.error = result.error();
            j.dirty.store(true, std::memory_order_relaxed);
            wake = markLocked(cancelled); // người dùng đang chờ việc mình hủy: báo ngay
        }
        if (wake)
            post();
    }
};

} // namespace detail

void Progress::set(double fraction)
{
    if (!(fraction > 0)) // 0, âm, NaN
        return;
    advance(fraction >= 1 ? hi_ : lo_ + std::int64_t(double(hi_ - lo_) * fraction));
}

Progress Progress::child(double weight)
{
    std::int64_t end = pos_;
    if (weight >= 1)
        end = hi_;
    else if (weight > 0)
        end = std::min(hi_, pos_ + std::int64_t(double(hi_ - lo_) * weight));
    Progress c(job_, pos_, end);
    pos_ = end;
    return c;
}

void Progress::advance(std::int64_t to)
{
    if (to <= pos_)
        return;
    job_->done.fetch_add(to - pos_, std::memory_order_relaxed);
    pos_ = to;
    // Chỉ lần đổi đầu tiên sau mỗi lô chạm khóa của registry; các lần sau là hai phép nguyên tử.
    if (!job_->dirty.exchange(true, std::memory_order_acq_rel))
        job_->registry->changed(false);
}

BackgroundJobs::BackgroundJobs(Timing timing, QObject* parent)
    : QObject(parent), timing_(timing), s_(std::make_shared<detail::Registry>())
{
    s_->ui = this;
    timer_.setSingleShot(true);
    timer_.setTimerType(Qt::PreciseTimer);
    // Qua schedule(): timer của Windows có thể tới sớm dưới 1 ms, khi đó hẹn lại phần còn thiếu.
    connect(&timer_, &QTimer::timeout, this, [this] { schedule(deadline_); });
    BackgroundJobs* none = nullptr;
    g_instance.compare_exchange_strong(none, this);
}

BackgroundJobs::~BackgroundJobs()
{
    {
        std::lock_guard l(s_->m);
        for (auto& job : s_->jobs)
            s_->cancelLocked(*job);
        s_->ui = nullptr; // đánh thức còn trong hàng của luồng UI thành rỗng
    }
    BackgroundJobs* self = this;
    g_instance.compare_exchange_strong(self, nullptr);
}

BackgroundJobs* BackgroundJobs::instance()
{
    return g_instance.load();
}

JobId BackgroundJobs::start(QString name, QString kind, std::uint64_t owner, Fn fn)
{
    auto job = std::make_shared<detail::Job>();
    job->name = std::move(name);
    job->kind = std::move(kind);
    job->owner = owner;
    job->registry = s_.get();
    job->startedAt = Clock::now();
    JobId id = 0;
    bool wake = false;
    {
        std::lock_guard l(s_->m);
        id = job->id = s_->next++;
        s_->jobs.push_back(job);
        wake = s_->markLocked(false);
    }
    if (wake)
        s_->post();
    sched::submit(sched::Priority::Background,
                  [s = s_, job = std::move(job), fn = std::move(fn)]() mutable { s->run(*job, fn); });
    return id;
}

bool BackgroundJobs::cancel(JobId id)
{
    bool running = false, wake = false;
    {
        std::lock_guard l(s_->m);
        if (detail::Job* j = s_->find(id); j && j->state == State::Running) {
            running = true;
            if (s_->cancelLocked(*j))
                wake = s_->markLocked(j->state != State::Running);
        }
    }
    if (wake)
        s_->post();
    return running;
}

int BackgroundJobs::cancelOwner(std::uint64_t owner)
{
    int n = 0;
    bool wake = false;
    {
        std::lock_guard l(s_->m);
        for (auto& job : s_->jobs)
            if (job->owner == owner && s_->cancelLocked(*job)) {
                ++n;
                wake = s_->markLocked(job->state != State::Running) || wake;
            }
    }
    if (wake)
        s_->post();
    return n;
}

std::optional<JobInfo> BackgroundJobs::info(JobId id) const
{
    std::lock_guard l(s_->m);
    const detail::Job* j = s_->find(id);
    if (!j)
        return std::nullopt;
    return detail::Registry::snapshot(*j, Clock::now(), timing_);
}

std::size_t BackgroundJobs::size() const
{
    std::lock_guard l(s_->m);
    return s_->jobs.size();
}

void BackgroundJobs::onWake()
{
    bool pending = false, urgent = false;
    {
        std::lock_guard l(s_->m);
        pending = s_->wakePosted;
        urgent = s_->urgent;
    }
    if (pending) // lô trước đã gom thay đổi này: không hẹn gì
        schedule(urgent ? Clock::now() : lastFlush_ + timing_.batchInterval);
}

void BackgroundJobs::schedule(Clock::time_point when)
{
    const auto now = Clock::now();
    if (when <= now) {
        flush();
        return;
    }
    if (timer_.isActive() && deadline_ <= when)
        return;
    deadline_ = when;
    timer_.start(std::chrono::ceil<std::chrono::milliseconds>(when - now));
}

void BackgroundJobs::flush()
{
    const auto now = Clock::now();
    timer_.stop();
    std::vector<JobInfo> batch;
    auto next = Clock::time_point::max(); // mốc cờ gần nhất của việc đang chạy
    {
        std::lock_guard l(s_->m);
        s_->wakePosted = s_->urgent = false;
        for (const auto& job : s_->jobs) {
            detail::Job& j = *job;
            const bool over = j.state != State::Running;
            const auto elapsed = (over ? j.finishedAt : now) - j.startedAt;
            const bool busy = elapsed >= timing_.busyCursor, show = elapsed >= timing_.showProgress;
            // acq_rel: thấy mọi phần tiến trình cộng trước lần đánh dấu mà lần đổi này xóa
            if (j.dirty.exchange(false, std::memory_order_acq_rel) || over || busy != j.reportedBusy ||
                show != j.reportedShow) {
                j.reportedBusy = busy;
                j.reportedShow = show;
                batch.push_back(detail::Registry::snapshot(j, now, timing_));
            }
            if (!over && !show)
                next = std::min(next, j.startedAt + (busy ? timing_.showProgress : timing_.busyCursor));
        }
        std::erase_if(s_->jobs, [](const auto& j) { return j->state != State::Running; }); // đã báo: rời registry
    }
    if (!batch.empty()) {
        ++stats_.batches;
        emit updated(batch);
    }
    // Nhịp tính từ lúc áp xong: lô sau đến ≥ batchInterval sau khi người nghe xử lý xong lô này (UI chậm thì nhận
    // ít lô hơn, không dồn).
    lastFlush_ = Clock::now();
    stats_.longest = std::max<std::chrono::nanoseconds>(stats_.longest, lastFlush_ - now);
    if (next != Clock::time_point::max())
        schedule(std::max(next, lastFlush_ + timing_.batchInterval));
}

} // namespace trion::jobs
