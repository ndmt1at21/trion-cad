# app/dialogs/find (UIF-001): hộp thoại Tìm chữ (FIND, UI-F09) không modal — FindDialog (tìm, phạm vi, tùy chọn, bảng kết
# quả ảo hóa, Tạo tập chọn, Thay/Thay tất cả khóa ở Viewer) và FindMatcher (so khớp hoa thường, nguyên từ, wildcard,
# bỏ dấu tiếng Việt; hàm thuần chạy nền theo đoạn).
trion_add_library(trion_finddialog
  SOURCES FindMatcher.h FindMatcher.cpp FindDialog.h FindDialog.cpp
  LIBS trion_dialog trion_views trion_message trion_progress trion_menu trion_edition trion_controls trion_i18n
       trion_fonts trion_theme trion_appcore trion_base Qt6::Widgets)

# Tiêu chí 1–6, DoD-U (vi-XL, a11y, bàn phím, rảnh), DoD-L (mở hộp); hiệu năng 200.000 chữ chỉ kiểm ở release.
trion_add_test(find_dialog_test SOURCES tests/find_dialog_test.cpp
  LIBS trion_finddialog trion_a11y trion_i18n trion_perf trion_appcore LABELS find)
