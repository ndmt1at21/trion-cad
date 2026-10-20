#pragma once
// Assert và kiểm tra bất biến (tập con của NT-009 cho các task UI; log, ring buffer và logdump
// thuộc NT-009 đầy đủ).
//   TRION_ASSERT(cond): bất biến nội bộ, bật khi không có NDEBUG (debug, sanitizer).
//   TRION_CHECK(cond):  luôn bật.
// Không dùng assert cho đầu vào không tin cậy: trả trion::Error (core/base/error.h).

namespace trion {

using AssertHandler = void (*)(const char* expr, const char* file, int line);

// Đặt handler khi assert sai; trả handler cũ. Mặc định in ra stderr rồi abort().
// Test thay handler để kiểm nhánh lỗi; handler trả về thì chương trình chạy tiếp.
AssertHandler setAssertHandler(AssertHandler handler) noexcept;

void assertFailed(const char* expr, const char* file, int line);

} // namespace trion

#define TRION_CHECK(cond) ((cond) ? (void)0 : ::trion::assertFailed(#cond, __FILE__, __LINE__))

#ifdef NDEBUG
#define TRION_ASSERT(cond) ((void)0)
#else
#define TRION_ASSERT(cond) TRION_CHECK(cond)
#endif
