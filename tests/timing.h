#pragma once
// Ngưỡng thời gian trong test chỉ kiểm ở bản tối ưu không sanitizer (preset release). Bản debug (dev, win) không tối
// ưu, sanitizer thay malloc của cả tiến trình (kể cả Qt): số đo ở đó chỉ để báo.
//   QVERIFY2(!trion::test::kTimed || best <= budget, …);
// Đo phần việc của luồng UI khi máy tải cao (agent, phiên khác cùng build): threadCpu() không tính thời gian chờ CPU.

#include <chrono>
#if defined(__linux__)
#include <time.h>
#endif

namespace trion::test {

#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
inline constexpr bool kSanitized = true;
#elif defined(__has_feature)
inline constexpr bool kSanitized = __has_feature(address_sanitizer) || __has_feature(thread_sanitizer);
#else
inline constexpr bool kSanitized = false;
#endif

#ifdef NDEBUG
inline constexpr bool kTimed = !kSanitized;
#else
inline constexpr bool kTimed = false;
#endif

// Thời gian CPU của luồng gọi. Linux: CLOCK_THREAD_CPUTIME_ID; nơi khác -1 (GetThreadTimes của Windows nhảy theo nhịp
// 15,6 ms): chỉ kiểm ngưỡng theo CPU khi threadCpu() >= 0.
inline std::chrono::nanoseconds threadCpu()
{
#if defined(__linux__)
    timespec t{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return std::chrono::seconds(t.tv_sec) + std::chrono::nanoseconds(t.tv_nsec);
#else
    return std::chrono::nanoseconds(-1);
#endif
}

} // namespace trion::test
