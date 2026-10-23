# app/dialogs/plot (UIH-001…): hộp thoại In, thiết lập trang, xem trước, xuất PDF/ảnh.
# UIH-001: PlotSettings (thiết lập trang tối thiểu theo PLT-001, thiết bị, PlotContext), PlotCommand (bộ phân tích tối
# thiểu của -PLOT), PlotSettingsModel (một nguồn trạng thái), PlotRunner (in nền qua NT-013, toast), PlotDialog (UI-H02).
trion_add_library(trion_plotdialogs
  SOURCES PlotSettings.h PlotSettings.cpp PlotCommand.h PlotCommand.cpp PlotSettingsModel.h PlotSettingsModel.cpp
          PlotRunner.h PlotRunner.cpp PlotDialog.h PlotDialog.cpp
  LIBS trion_dialog trion_numeric_field trion_message trion_edition trion_jobs trion_regen trion_controls trion_icons
       trion_fonts trion_theme trion_appcore trion_base Qt6::Widgets)

# Máy in hệ thống qua QPrinterInfo (Qt PrintSupport, có trong Qt của 05 §2; PLT-005 dùng tiếp). Không có module thì
# danh sách máy in hệ thống rỗng. ponytail: find_package gốc và trion_deploy_qt_runtime chưa có PrintSupport: tìm ở
# đây và chép DLL cho test Windows; đề nghị chuyển lên CMakeLists gốc, cmake/TrionTargets.cmake.
find_package(Qt6 QUIET COMPONENTS PrintSupport)
if(TARGET Qt6::PrintSupport)
  target_link_libraries(trion_plotdialogs PRIVATE Qt6::PrintSupport)
  target_compile_definitions(trion_plotdialogs PRIVATE TRION_PLOT_PRINTSUPPORT)
  if(WIN32 AND EXISTS ${QT6_INSTALL_PREFIX}/bin/Qt6PrintSupport.dll)
    file(COPY ${QT6_INSTALL_PREFIX}/bin/Qt6PrintSupport.dll DESTINATION ${CMAKE_BINARY_DIR}/bin)
  endif()
endif()

# Tiêu chí 1–5, 7, 8; tiêu chí 6 phần kiểm được (hộp thoại và -PLOT cho cùng PlotSettings); vi-XL, a11y, bàn phím.
trion_add_test(plot_dialog_test SOURCES tests/plot_dialog_test.cpp
  LIBS trion_plotdialogs trion_progress trion_a11y trion_i18n trion_perf LABELS plot)
