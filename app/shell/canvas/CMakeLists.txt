# app/shell/canvas (UIA-109): widget vùng vẽ — con trỏ, định tuyến phím, khung chọn, tô sáng. CanvasWidget nhận sự
# kiện; CanvasOverlay vẽ chữ thập, ô chọn, khung (RND-001/012/020 thay phần vẽ, giữ phần nhận).
trion_add_library(trion_canvas
  SOURCES CanvasWidget.h CanvasWidget.cpp CanvasOverlay.h CanvasOverlay.cpp
  LIBS trion_actions trion_message trion_controls trion_ime trion_icons trion_theme Qt6::Widgets)

# Kiểm thuộc tính và điểm ảnh (con trỏ, khung chỉ có hình, không chữ): một bộ cho mọi nền tảng, không ảnh baseline.
trion_add_test(canvas_test SOURCES tests/canvas_test.cpp
  LIBS trion_canvas trion_a11y trion_cmdline trion_perf trion_keymap trion_i18n trion_icons trion_fonts trion_appcore
  LABELS canvas)
