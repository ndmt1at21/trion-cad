# app/shell/cmdline (UIA-107): dòng lệnh UI-A06 — lịch sử, prompt có chip tùy chọn, ô nhập CommandInput (UIA-106);
# Autocomplete (UIA-108): tự hoàn thành, gợi ý khi gõ sai, AutoCorrect (chỉ mục lệnh của app/core/actions).
# UIA-007: HistoryWindow (F2, TEXTSCR), InputSearchOptions (INPUTSEARCHOPTIONS), dạng nổi, thông báo cổng phiên bản.
trion_add_library(trion_commandline
  SOURCES CommandLine.h CommandLine.cpp Autocomplete.h Autocomplete.cpp HistoryWindow.h HistoryWindow.cpp
          InputSearchOptions.h InputSearchOptions.cpp
  LIBS trion_cmdline trion_actions trion_edition trion_dialog trion_views trion_numeric_field trion_message
       trion_controls trion_menu trion_tooltip trion_perf trion_icons trion_fonts trion_i18n trion_appcore trion_theme
       trion_base Qt6::Widgets)

trion_add_test(commandline_test SOURCES tests/commandline_test.cpp
  LIBS trion_commandline trion_perf trion_i18n trion_appcore LABELS commandline)
trion_add_test(autocomplete_test SOURCES tests/autocomplete_test.cpp
  LIBS trion_commandline trion_actions trion_perf trion_i18n trion_appcore LABELS commandline)
trion_add_test(cmdline_extras_test SOURCES tests/cmdline_extras_test.cpp
  LIBS trion_commandline trion_edition trion_actions trion_perf trion_i18n trion_appcore LABELS commandline)
# Ảnh baseline Tối/Sáng theo nền tảng, nhúng vào test để bản Windows đọc được; ghi lại: TRION_UPDATE_BASELINE=<thư mục>
# chạy commandline_test, chép ảnh vào tests/baseline.
file(GLOB baseline CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/baseline/*.png)
if(baseline)
  qt_add_resources(commandline_test commandline_baseline PREFIX /baseline BASE tests/baseline FILES ${baseline})
endif()
