# app/shell/titlebar (UIA-002, UIA-101): TitleBar (nút menu ứng dụng, tên bản vẽ, huy hiệu phiên bản, menu tài khoản,
# trợ giúp), Qat (QAT: danh sách hoàn tác, tùy biến, "Thêm vào QAT"), CommandSearch (ô Tìm lệnh; viết tắt, từ khóa từ
# app/core/actions/SearchTerms.h).
trion_add_library(trion_titlebar
  SOURCES TitleBar.h TitleBar.cpp Qat.h Qat.cpp CommandSearch.h CommandSearch.cpp
  LIBS trion_edition trion_actions trion_menu trion_controls trion_tooltip trion_icons trion_fonts trion_theme
       trion_perf trion_base Qt6::Widgets)

trion_add_test(titlebar_test SOURCES tests/titlebar_test.cpp
  LIBS trion_titlebar trion_keymap trion_i18n trion_appcore LABELS titlebar)
trion_add_test(qat_test SOURCES tests/qat_test.cpp
  LIBS trion_titlebar trion_keymap trion_i18n trion_appcore LABELS titlebar)
# Ảnh baseline theo nền tảng × chủ đề, nhúng vào test để bản Windows đọc được; ghi lại:
# TRION_UPDATE_BASELINE=<thư mục> chạy titlebar_test, chép titlebar-*-<nền tảng>.png vào tests/baseline.
file(GLOB baseline CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/baseline/*.png)
if(baseline)
  qt_add_resources(titlebar_test titlebar_baseline PREFIX /baseline BASE tests/baseline FILES ${baseline})
endif()
