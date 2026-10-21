# app/widgets/controls (UI0-106): Button (thường, chính, nguy hiểm, ghost, chỉ biểu tượng), SplitButton, ToggleButton,
# SegmentedGroup, DropDown, ComboBox có ô lọc, SearchField, TabBar, Slider: lớp con widget Qt chuẩn tự vẽ theo token
# (Controls.h).
trion_add_library(trion_controls
  SOURCES Controls.h Controls.cpp
  LIBS trion_icons trion_perf trion_theme trion_base Qt6::Widgets)

trion_add_test(controls_test SOURCES tests/controls_test.cpp LIBS trion_controls trion_fonts trion_i18n LABELS controls)
# Ảnh baseline (tiêu chí 1) theo nền tảng × chủ đề × DPR, nhúng vào test để bản Windows đọc được; ghi lại:
# TRION_UPDATE_BASELINE=<thư mục> chạy controls_test, chép ảnh vào tests/baseline (xem tests/controls_test.cpp).
file(GLOB baseline CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/baseline/*.png)
if(baseline)
  qt_add_resources(controls_test controls_baseline PREFIX /baseline BASE tests/baseline FILES ${baseline})
endif()
