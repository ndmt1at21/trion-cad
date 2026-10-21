# app/theme/icons. UI0-103: SVG nguồn (src/), manifest.json, ảnh tham chiếu (ref/; tools/iconconv kiểm).
# UI0-004: IconProvider (QIconEngine tô màu theo chủ đề và trạng thái, cache raster LRU theo DPI, raster trước ở
# nền). SVG và manifest nhúng ở :/trion/icons/.
trion_add_library(trion_icons
  SOURCES IconProvider.h IconProvider.cpp
  LIBS trion_theme trion_appcore Qt6::Svg)
file(GLOB svgs CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.svg)
qt_add_resources(trion_icons icons_svg PREFIX /trion/icons BASE src FILES ${svgs})
qt_add_resources(trion_icons icons_manifest PREFIX /trion/icons FILES manifest.json)

# Ảnh baseline (tiêu chí 1) nhúng vào test để bản Windows đọc được; tạo lại: TRION_UPDATE_BASELINE=1 chạy test.
trion_add_test(icons_provider_test SOURCES tests/icon_provider_test.cpp LIBS trion_icons LABELS icons)
target_compile_definitions(icons_provider_test PRIVATE TRION_ICONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
file(GLOB baseline CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/baseline/*.png)
if(baseline)
  qt_add_resources(icons_provider_test icons_baseline PREFIX /baseline BASE tests/baseline FILES ${baseline})
endif()
