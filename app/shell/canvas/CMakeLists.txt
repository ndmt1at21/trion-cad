# app/shell/canvas (UIA-109): widget vùng vẽ — con trỏ, định tuyến phím, khung chọn, tô sáng. CanvasWidget nhận sự
# kiện; CanvasOverlay vẽ chữ thập, ô chọn, khung (RND-001/012/020 thay phần vẽ, giữ phần nhận).
# UIA-008: Navigation (lăn, chuột giữa, touchpad → chỗ nối INT-016; TempView là view tạm), LoadGate (hiển thị dần khi mở
# tệp lớn: việc nạp NT-013, lệnh sửa chờ nạp xong).
trion_add_library(trion_canvas
  SOURCES CanvasWidget.h CanvasWidget.cpp CanvasOverlay.h CanvasOverlay.cpp Navigation.h Navigation.cpp
          LoadGate.h LoadGate.cpp
  LIBS trion_actions trion_message trion_controls trion_ime trion_icons trion_theme trion_jobs trion_appcore
       Qt6::Widgets)

# Kiểm thuộc tính và điểm ảnh (con trỏ, khung chỉ có hình, không chữ): một bộ cho mọi nền tảng, không ảnh baseline.
trion_add_test(canvas_test SOURCES tests/canvas_test.cpp
  LIBS trion_canvas trion_a11y trion_cmdline trion_perf trion_keymap trion_i18n trion_icons trion_fonts trion_appcore
  LABELS canvas)
trion_add_test(canvas_nav_test SOURCES tests/canvas_nav_test.cpp
  LIBS trion_canvas trion_cmdline trion_i18n trion_icons trion_fonts trion_appcore trion_jobs
  LABELS canvas)
