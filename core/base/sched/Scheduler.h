#pragma once
// Bộ lập lịch — tập con của NT-005 đủ cho các task UI.
// ponytail: pool luồng riêng, một hàng đợi có ưu tiên (từ snapone/core/jobs). Work-stealing, QoS nhân P/E,
// TaskGraph, parallel_* và hủy trong ≤ 50 ms do NT-105/NT-106/NT-005 thay thế sau, giữ nguyên API.
//
// Quy tắc (DoD-H 1, 6): không tự tạo std::thread trong mã sản phẩm; việc nền gửi qua submit().
// Kết quả về luồng UI qua postToUi(); UI0-101 (UiExecutor) cài poster khi ứng dụng khởi động.

#include <atomic>
#include <functional>
#include <memory>

namespace trion::sched {

enum class Priority { Interactive, Normal, Background };

class CancelToken {
public:
    CancelToken() = default;
    bool cancelled() const noexcept { return flag_ && flag_->load(std::memory_order_relaxed); }

private:
    friend class CancelSource;
    explicit CancelToken(std::shared_ptr<std::atomic<bool>> f) : flag_(std::move(f)) {}
    std::shared_ptr<std::atomic<bool>> flag_;
};

class CancelSource {
public:
    CancelSource() : flag_(std::make_shared<std::atomic<bool>>(false)) {}
    CancelToken token() const { return CancelToken(flag_); }
    void cancel() noexcept { flag_->store(true, std::memory_order_relaxed); }
    bool cancelled() const noexcept { return flag_->load(std::memory_order_relaxed); }

private:
    std::shared_ptr<std::atomic<bool>> flag_;
};

// Chạy fn trên worker nền. An toàn gọi từ mọi luồng.
void submit(Priority priority, std::function<void()> fn);

// Chờ mọi việc đã submit chạy xong (thoát ứng dụng, test). Không gọi từ worker.
void waitIdle();

using UiPoster = std::function<void(std::function<void()>)>;

// UI0-101 cài hàm đưa callback về luồng UI. Truyền {} để gỡ.
void setUiPoster(UiPoster poster);

// Đưa fn về luồng UI. Chưa có poster là lỗi lập trình (assert), fn bị bỏ.
void postToUi(std::function<void()> fn);

} // namespace trion::sched
