# app/shell/filetabs (UIA-010): dải tab tệp UI-A09 — FileTabBar.h.
trion_add_library(trion_filetabs
  SOURCES FileTabBar.h FileTabBar.cpp
  LIBS trion_edition trion_message trion_menu trion_tooltip trion_controls trion_appcore trion_fonts trion_icons
       trion_theme trion_base Qt6::Widgets)

trion_add_test(file_tabs_test SOURCES tests/file_tabs_test.cpp
  LIBS trion_filetabs trion_perf trion_i18n trion_keymap LABELS filetabs)
# Ảnh baseline theo nền tảng × chủ đề (100%), nhúng vào test để bản Windows đọc được; ghi lại:
# TRION_UPDATE_BASELINE=<thư mục> chạy file_tabs_test, chép ảnh vào tests/baseline.
file(GLOB baseline CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/baseline/*.png)
if(baseline)
  qt_add_resources(file_tabs_test filetabs_baseline PREFIX /baseline BASE tests/baseline FILES ${baseline})
endif()
