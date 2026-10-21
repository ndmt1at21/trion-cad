# app/widgets/tooltip (UI0-107): RichTooltip — tooltip 2 mức, F1 theo tên lệnh, huy hiệu phiên bản (RichTooltip.h).
trion_add_library(trion_tooltip
  SOURCES RichTooltip.h RichTooltip.cpp
  LIBS trion_keymap trion_fonts trion_theme trion_base Qt6::Widgets)

trion_add_test(tooltip_test SOURCES tests/tooltip_test.cpp LIBS trion_tooltip trion_i18n trion_appcore LABELS tooltip)
