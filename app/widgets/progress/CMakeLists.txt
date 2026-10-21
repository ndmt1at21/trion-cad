# app/widgets/progress (UI0-112): ProgressPresenter (theo dõi tác vụ NT-013, quy tắc 0,5 s/2 s, con trỏ bận, gộp
# nhiều tác vụ), ProgressItemModel (thanh trạng thái UIA-009), InlineProgress (hộp thoại, mục Tác vụ nền).
trion_add_library(trion_progress
  SOURCES ProgressPresenter.h ProgressPresenter.cpp InlineProgress.h InlineProgress.cpp
  LIBS trion_controls trion_jobs trion_i18n trion_theme Qt6::Widgets)

trion_add_test(progress_test SOURCES tests/progress_test.cpp LIBS trion_progress trion_fonts LABELS progress)
# Ảnh baseline của InlineProgress theo nền tảng × chủ đề × DPR (như app/widgets/controls), nhúng vào test để bản
# Windows đọc được; ghi lại: TRION_UPDATE_BASELINE=<thư mục> chạy progress_test, chép ảnh vào tests/baseline.
file(GLOB baseline CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/baseline/*.png)
if(baseline)
  qt_add_resources(progress_test progress_baseline PREFIX /baseline BASE tests/baseline FILES ${baseline})
endif()
