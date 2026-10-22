# app/shell/status: thanh trạng thái UI-A08. UIA-112: ModeToggles (dải nút chế độ hỗ trợ vẽ, nút .sbt StatusButton);
# UIA-009: StatusBar (tọa độ, MODEL/PAPER, chỉ báo, tác vụ nền, thông báo, huy hiệu, Tùy biến, menu tràn).
trion_add_library(trion_status
  SOURCES ModeToggles.h ModeToggles.cpp StatusBar.h StatusBar.cpp
  LIBS trion_actions trion_edition trion_menu trion_tooltip trion_progress trion_message trion_controls trion_regen
       trion_icons trion_fonts trion_i18n trion_theme Qt6::Widgets)

trion_add_test(mode_toggles_test SOURCES tests/mode_toggles_test.cpp
  LIBS trion_status trion_i18n trion_icons trion_fonts trion_perf trion_appcore LABELS status)
# Ảnh baseline Tối/Sáng × 100/200% (không có chữ: một bộ cho mọi nền tảng), nhúng vào test để bản Windows đọc được;
# ghi lại: TRION_UPDATE_BASELINE=<thư mục> chạy mode_toggles_test, chép ảnh vào tests/baseline.
file(GLOB baseline CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/baseline/*.png)
if(baseline)
  qt_add_resources(mode_toggles_test status_baseline PREFIX /baseline BASE tests/baseline FILES ${baseline})
endif()

# UIA-009: kiểm thuộc tính và điểm ảnh, không ảnh baseline.
trion_add_test(status_bar_test SOURCES tests/status_bar_test.cpp
  LIBS trion_status trion_a11y trion_jobs trion_i18n trion_icons trion_fonts trion_appcore LABELS status)
