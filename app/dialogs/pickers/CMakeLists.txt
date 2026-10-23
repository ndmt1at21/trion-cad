# app/dialogs/pickers (UIC-017): bộ chọn màu ColorPickerDialog dùng chung (ACI, True Color, ByLayer/ByBlock, Color Book
# P2, màu gần đây, lấy màu từ bản vẽ), kiểu Color và đổi RGB ⇄ HSL ⇄ hex (Color.h), phần tự vẽ (ColorWidgets.h).
# Bảng ACI → RGB lấy từ RGN-002 (core/regen/props/AciPalette.h, trion_regen).
trion_add_library(trion_pickers
  SOURCES Color.h Color.cpp ColorWidgets.h ColorWidgets.cpp ColorPickerDialog.h ColorPickerDialog.cpp
  LIBS trion_numeric_field trion_dialog trion_message trion_tooltip trion_controls trion_icons trion_fonts trion_theme
       trion_regen trion_base Qt6::Widgets)

trion_add_test(color_picker_test SOURCES tests/color_picker_test.cpp
  LIBS trion_pickers trion_a11y trion_i18n trion_perf trion_appcore LABELS pickers)
