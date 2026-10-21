# app/widgets/dock (UI0-113): PaletteHost — palette trên dock của QMainWindow (trái, phải, dưới, nổi, nhóm tab), dựng
# nội dung ở lần hiện đầu, lưu/khôi phục bố cục qua Store; PaletteHeader — đầu palette theo prototype.
# UI0-010: AutoHideStrip — dải tự ẩn 22 px và khung trượt; bố cục mặc định theo lớp màn hình, đổi DPI (trion_display).
trion_add_library(trion_dock
  SOURCES PaletteHost.h PaletteHost.cpp PaletteHeader.h PaletteHeader.cpp AutoHideStrip.h AutoHideStrip.cpp
  LIBS trion_controls trion_display trion_perf trion_theme trion_base Qt6::Widgets)

trion_add_test(dock_test SOURCES tests/dock_test.cpp LIBS trion_dock trion_fonts trion_i18n LABELS dock)
# Ảnh baseline theo nền tảng × chủ đề × DPR, nhúng vào test để bản Windows đọc được; ghi lại:
# TRION_UPDATE_BASELINE=<thư mục> chạy test, chép ảnh vào tests/baseline (xem đầu tệp test).
file(GLOB baseline CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/baseline/header-*.png)
if(baseline)
  qt_add_resources(dock_test dock_baseline PREFIX /baseline BASE tests/baseline FILES ${baseline})
endif()

# UI0-010: nhiều màn hình giả 100%/150% (initMain); rút màn hình lúc đang chạy qua cấu hình của plugin offscreen, gọi
# bằng QPlatformNativeInterface (QPA, Qt6::GuiPrivate) — chỉ test dùng; cảnh báo "module riêng" của Qt tắt vì đã biết
# (mỗi lần cấu hình in 16 dòng), cần xem lại khi nâng Qt.
set(QT_NO_PRIVATE_MODULE_WARNING ON)
find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
trion_add_test(autohide_test SOURCES tests/autohide_test.cpp
  LIBS trion_dock trion_fonts trion_i18n Qt6::GuiPrivate LABELS dock)
file(GLOB autohide_baseline CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/baseline/autohide-*.png)
if(autohide_baseline)
  qt_add_resources(autohide_test autohide_baseline PREFIX /baseline BASE tests/baseline FILES ${autohide_baseline})
endif()
