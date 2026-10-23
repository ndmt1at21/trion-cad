# app/dialogs/options (UID-019): khung hộp thoại Tùy chọn (OPTIONS, UI-D10) — OptionsDialog (12 tab dọc, khóa theo phiên
# bản, trang đăng ký theo tab, bản nháp: xem trước, Hủy, Áp dụng, sysvar bản vẽ thành một nhóm SETVAR) và SettingsIndex
# (chỉ mục tìm thiết lập: tiếng Việt, tiếng Anh, sysvar, không phân biệt dấu).
trion_add_library(trion_options
  SOURCES SettingsIndex.h SettingsIndex.cpp OptionsDialog.h OptionsDialog.cpp
  LIBS trion_dialog trion_message trion_edition trion_actions trion_controls trion_tooltip trion_icons trion_fonts
       trion_theme trion_base Qt6::Widgets)

# Tiêu chí 1–5, vi-XL, a11y. Bảng TC.SETTINGS_INDEX, OPT_TABS của prototype nhúng vào test (bản Windows đọc được) để
# so với SettingsIndex.
trion_add_test(options_test SOURCES tests/options_test.cpp
  LIBS trion_options trion_a11y trion_i18n trion_appcore LABELS options)
qt_add_resources(options_test options_prototype PREFIX /options_test BASE ${CMAKE_SOURCE_DIR}/design/prototype/js
  FILES ${CMAKE_SOURCE_DIR}/design/prototype/js/screens2.js)
