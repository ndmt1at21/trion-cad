# app/shell/viewer (UIA-118): Viewer — palette Layer (ViewerLayers: bật/tắt, đóng băng tạm, cô lập, tìm, "Khôi phục như
# tệp gốc") và tệp trạng thái hiển thị .tcview (ViewState: định dạng, đọc/ghi, chuỗi lệnh -LAYER; ViewStateFile: cổng
# Viewer Pro, việc nền, banner). ViewState là thư viện riêng chỉ Qt Core để bản fuzz không kéo theo Qt Widgets.
trion_add_library(trion_viewstate SOURCES ViewState.h ViewState.cpp LIBS trion_base Qt6::Core)

trion_add_library(trion_viewer
  SOURCES ViewerLayers.h ViewerLayers.cpp
  LIBS trion_viewstate trion_views trion_model trion_edition trion_message trion_menu trion_tooltip trion_controls
       trion_appcore trion_i18n trion_fonts trion_icons trion_theme trion_base Qt6::Widgets)

trion_add_test(viewer_layers_test SOURCES tests/viewer_layers_test.cpp
  LIBS trion_viewer trion_a11y trion_i18n trion_appcore LABELS viewer)

# libFuzzer ghi đầu vào mới vào thư mục corpus đầu tiên: chạy trên bản sao trong thư mục build, không bẩn kho mã.
file(COPY fuzz/corpus DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/fuzz)
trion_add_fuzz(viewer_state_fuzz SOURCES fuzz/view_state_fuzz.cpp LIBS trion_viewstate
  CORPUS ${CMAKE_CURRENT_BINARY_DIR}/fuzz/corpus)

# UIA-017: ribbon Viewer, panel nâng cấp, thanh trạng thái giản lược, chế độ chỉ xem tới chỗ nối (RibbonBar, StatusBar ở
# hồ sơ Viewer). Ảnh baseline theo nền tảng × chủ đề nhúng vào test; ghi lại: TRION_UPDATE_BASELINE=<thư mục> chạy
# viewer_test, chép viewer-*-<nền tảng>.png vào tests/baseline.
trion_add_test(viewer_test SOURCES tests/viewer_test.cpp
  LIBS trion_ribbon trion_status trion_a11y trion_jobs trion_i18n trion_icons trion_fonts trion_appcore LABELS viewer)
file(GLOB viewer_baseline CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/baseline/*.png)
if(viewer_baseline)
  qt_add_resources(viewer_test viewer_baseline PREFIX /baseline BASE tests/baseline FILES ${viewer_baseline})
endif()
