# app/shell/start (UIA-004): tab Start UI-A02 — StartPage; RecentFiles (tệp gần đây, dùng chung với menu ứng dụng
# UIA-003); ThumbnailQueue (ảnh xem trước ở nền, chỗ nối DWG-026 và RND-021).
trion_add_library(trion_start
  SOURCES RecentFiles.h RecentFiles.cpp ThumbnailQueue.h ThumbnailQueue.cpp StartPage.h StartPage.cpp
  LIBS trion_views trion_edition trion_message trion_menu trion_tooltip trion_controls trion_appcore trion_i18n
       trion_fonts trion_icons trion_theme trion_base Qt6::Widgets)

trion_add_test(start_test SOURCES tests/start_test.cpp
  LIBS trion_start trion_perf trion_keymap trion_i18n LABELS start)
