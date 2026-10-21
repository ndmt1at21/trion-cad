# app/widgets/dock (UI0-113): PaletteHost — palette trên dock của QMainWindow (trái, phải, dưới, nổi, nhóm tab), dựng
# nội dung ở lần hiện đầu, lưu/khôi phục bố cục qua Store; PaletteHeader — đầu palette theo prototype.
trion_add_library(trion_dock
  SOURCES PaletteHost.h PaletteHost.cpp PaletteHeader.h PaletteHeader.cpp
  LIBS trion_controls trion_perf trion_theme trion_base Qt6::Widgets)

trion_add_test(dock_test SOURCES tests/dock_test.cpp LIBS trion_dock trion_fonts trion_i18n LABELS dock)
# Ảnh baseline đầu palette theo nền tảng × chủ đề × DPR, nhúng vào test để bản Windows đọc được; ghi lại:
# TRION_UPDATE_BASELINE=<thư mục> chạy dock_test, chép ảnh vào tests/baseline (xem tests/dock_test.cpp).
file(GLOB baseline CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/baseline/*.png)
if(baseline)
  qt_add_resources(dock_test dock_baseline PREFIX /baseline BASE tests/baseline FILES ${baseline})
endif()
