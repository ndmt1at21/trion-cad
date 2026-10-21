# app/widgets/dialog (UI0-110): khung hộp thoại DialogFrame (đầu, thân, chân, phím, focus, đang tải, ẩn tạm) và
# DialogHost (mở theo mã, một bản mỗi mã, nhớ vị trí và cỡ, danh sách mã cho CI).
trion_add_library(trion_dialog
  SOURCES DialogFrame.h DialogFrame.cpp DialogHost.h DialogHost.cpp
  LIBS trion_controls trion_fonts trion_appcore trion_theme trion_base Qt6::Widgets)

trion_add_test(dialog_test SOURCES tests/dialog_test.cpp LIBS trion_dialog trion_i18n trion_perf LABELS dialog)
# Ảnh baseline khung hộp thoại theo nền tảng × chủ đề × DPR, nhúng vào test (như controls); ghi lại:
# TRION_UPDATE_BASELINE=<thư mục> chạy dialog_test, chép frame-*-<nền tảng>.png vào tests/baseline.
file(GLOB baseline CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/baseline/*.png)
if(baseline)
  qt_add_resources(dialog_test dialog_baseline PREFIX /baseline BASE tests/baseline FILES ${baseline})
endif()
