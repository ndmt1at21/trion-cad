# app/core/display (UI0-012): DPI theo màn hình (hệ số lẻ không làm tròn), tín hiệu đổi DPI và đổi màn hình của cửa
# sổ, ScreenClass cho bố cục khung, giảm chuyển động của hệ điều hành → ThemeManager.
trion_add_library(trion_display
  SOURCES DisplayContext.h DisplayContext.cpp
  LIBS trion_icons trion_appcore Qt6::Widgets)

# Test tự dựng môi trường DPI trong initMain(): bỏ QT_ENABLE_HIGHDPI_SCALING=0 của môi trường test chung, plugin
# offscreen nhiều màn hình 100–200% (cấu hình JSON). Ảnh bảng mẫu (tiêu chí 2) khác nhau theo nền tảng vì dựng chữ:
# tests/baseline/<linux|windows|macos>/ nhúng vào test; tạo lại: TRION_UPDATE_BASELINE=1 khi chạy qua ctest (ghi vào
# thư mục nguồn theo đường dẫn tương đối, nên bản Windows chạy từ WSL cũng ghi được).
if(WIN32)
  set(platform windows)
elseif(APPLE)
  set(platform macos)
else()
  set(platform linux)
endif()
trion_add_test(display_context_test SOURCES tests/display_context_test.cpp
  LIBS trion_display trion_fonts trion_perf LABELS display)
file(RELATIVE_PATH baseline_out ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests/baseline/${platform})
target_compile_definitions(display_context_test PRIVATE TRION_BASELINE_OUT="${baseline_out}")
file(GLOB baseline CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/baseline/${platform}/*.png)
if(baseline)
  qt_add_resources(display_context_test display_baseline PREFIX /baseline BASE tests/baseline/${platform}
    FILES ${baseline})
endif()
