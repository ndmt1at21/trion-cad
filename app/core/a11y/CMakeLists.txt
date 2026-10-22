# app/core/a11y (UI0-013): kiểm khả năng tiếp cận (AccessibilityAudit.h: tên, vai trò, thứ tự Tab, viền focus, cây
# QAccessible ra JSON) và thông báo cho trình đọc màn hình (Announce.h, chỉ header: thành phần widget dùng không cần link).
trion_add_library(trion_a11y
  SOURCES AccessibilityAudit.h AccessibilityAudit.cpp Announce.h
  LIBS trion_theme Qt6::Widgets)

# Cổng CI (tiêu chí 1): bảng mẫu và các hộp thoại của app/widgets/dialog không vi phạm; cây QAccessible của bảng mẫu
# khớp baseline JSON ở vi và en (tests/baseline, nhúng vào test cho bản Windows). Cần các thành phần: app/widgets/
# gallery, dialog, message, progress, dock (và phụ thuộc của chúng). Ghi lại baseline: TRION_UPDATE_BASELINE=<thư mục>
# chạy a11y_test, chép gallery-<ngôn ngữ>.json vào tests/baseline.
trion_add_test(a11y_test SOURCES tests/a11y_test.cpp
  LIBS trion_a11y trion_gallery trion_dialog trion_message trion_progress trion_dock trion_tooltip trion_keymap
       trion_perf trion_appcore trion_i18n trion_fonts trion_icons trion_jobs LABELS a11y)
file(GLOB baseline CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/baseline/*.json)
if(baseline)
  qt_add_resources(a11y_test a11y_baseline PREFIX /baseline BASE tests/baseline FILES ${baseline})
endif()
