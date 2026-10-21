#pragma once
// Cạnh happens-before mà TSan không thấy vì đi qua Qt bản cài sẵn (không biên dịch với TSan), ví dụ hàng đợi sự kiện:
// bên gửi TRION_TSAN_RELEASE(p) sau khi dựng p, bên nhận TRION_TSAN_ACQUIRE(p) trước khi đọc p. Bản không TSan: rỗng.

#if defined(__SANITIZE_THREAD__)
#define TRION_TSAN 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define TRION_TSAN 1
#endif
#endif

#ifdef TRION_TSAN
#include <sanitizer/tsan_interface.h>
#define TRION_TSAN_RELEASE(p) __tsan_release(const_cast<void*>(static_cast<const volatile void*>(p)))
#define TRION_TSAN_ACQUIRE(p) __tsan_acquire(const_cast<void*>(static_cast<const volatile void*>(p)))
#else
#define TRION_TSAN_RELEASE(p) ((void)0)
#define TRION_TSAN_ACQUIRE(p) ((void)0)
#endif
