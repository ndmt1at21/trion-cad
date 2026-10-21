#pragma once
// Ngưỡng thời gian trong test chỉ kiểm ở bản tối ưu không sanitizer (preset release). Bản debug (dev, win) không tối
// ưu, sanitizer thay malloc của cả tiến trình (kể cả Qt): số đo ở đó chỉ để báo.
//   QVERIFY2(!trion::test::kTimed || best <= budget, …);

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

} // namespace trion::test
