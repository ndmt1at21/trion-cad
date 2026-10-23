# app/widgets/views (UI0-108): VirtualTableView, VirtualTreeView — QAbstractItemView dòng cao đều theo --h-row, chỉ chạm
# các dòng đang nhìn (bộ nhớ, bố trí không theo số dòng); sắp ở nền qua model, lưu bố cục cột, gõ để nhảy, sửa tại chỗ.
# TextCache: chữ một dòng đã shaping cho delegate tự vẽ (tab Start, palette Layer của Viewer).
trion_add_library(trion_views
  SOURCES VirtualTableView.h VirtualTableView.cpp VirtualTreeView.h VirtualTreeView.cpp TextCache.h TextCache.cpp
  LIBS trion_icons trion_theme trion_base Qt6::Widgets)
trion_add_test(text_cache_test SOURCES tests/text_cache_test.cpp LIBS trion_views trion_fonts LABELS views)

trion_add_test(views_test SOURCES tests/views_test.cpp tests/SampleModels.h
  LIBS trion_views trion_model trion_fonts trion_i18n trion_perf LABELS views)
# Ảnh baseline theo nền tảng × chủ đề × DPR, nhúng vào test để bản Windows đọc được; ghi lại:
# TRION_UPDATE_BASELINE=<thư mục> chạy views_test, chép ảnh vào tests/baseline (xem tests/views_test.cpp).
file(GLOB baseline CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/baseline/*.png)
if(baseline)
  qt_add_resources(views_test views_baseline PREFIX /baseline BASE tests/baseline FILES ${baseline})
endif()

# Benchmark 1.000.000 dòng (bản release; debug, sanitizer: 100.000 dòng, chỉ in số đo thời gian).
trion_add_test(views_bench SOURCES tests/views_bench.cpp tests/SampleModels.h
  LIBS trion_views trion_model trion_fonts trion_perf LABELS views bench)
