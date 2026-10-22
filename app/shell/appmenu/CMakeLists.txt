# app/shell/appmenu (UIA-003): menu ứng dụng UI-A03 — AppMenu (nút logo của TitleBar): mục lệnh qua ActionRegistry, menu
# con theo phiên bản, tệp gần đây (RecentFiles, ThumbnailQueue của app/shell/start), ô tìm lệnh và tệp (CommandSearch).
trion_add_library(trion_appmenu
  SOURCES AppMenu.h AppMenu.cpp
  LIBS trion_start trion_titlebar trion_edition trion_actions trion_menu trion_tooltip trion_controls trion_perf
       trion_icons trion_fonts trion_theme trion_base Qt6::Widgets)

trion_add_test(appmenu_test SOURCES tests/appmenu_test.cpp
  LIBS trion_appmenu trion_a11y trion_keymap trion_i18n trion_appcore LABELS appmenu)
