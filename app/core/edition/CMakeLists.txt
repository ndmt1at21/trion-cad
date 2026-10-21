# app/core/edition (UI0-011): cổng phiên bản trong UI — EditionGate (phiên bản, bảng khả năng 07.1 §2, UiProfile,
# khóa QAction của ActionRegistry), UpgradeRouter (trang nâng cấp UI-L04 hoặc trang web), LockBadge (huy hiệu khóa).
trion_add_library(trion_edition
  SOURCES EditionGate.h EditionGate.cpp LockBadge.h LockBadge.cpp
  LIBS trion_actions trion_tooltip trion_fonts trion_theme Qt6::Widgets)

trion_add_test(edition_test SOURCES tests/edition_test.cpp
  LIBS trion_edition trion_menu trion_icons trion_i18n trion_appcore LABELS edition)
# Ảnh baseline LockBadge theo nền tảng × chủ đề × DPR, nhúng vào test để bản Windows đọc được; ghi lại:
# TRION_UPDATE_BASELINE=<thư mục> chạy edition_test, chép badges-*-<nền tảng>.png vào tests/baseline.
file(GLOB baseline CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/baseline/*.png)
if(baseline)
  qt_add_resources(edition_test edition_baseline PREFIX /baseline BASE tests/baseline FILES ${baseline})
endif()
