# Integration test: chạy nhiều thành phần cùng nhau, sau mỗi nhóm 3–5 task (xem ci/integration.sh).
# Chỉ build khi các thành phần cần đều được cấu hình (TRION_COMPONENTS có thể chọn một phần).
function(trion_integration_test name)
  foreach(t IN LISTS ARGN)
    if(NOT TARGET ${t})
      return()
    endif()
  endforeach()
  trion_add_test(${name} SOURCES ${name}.cpp LIBS ${ARGN} Qt6::Widgets Qt6::Svg LABELS integration)
  target_compile_definitions(${name} PRIVATE TRION_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
endfunction()

# Nhóm 1: UI0-101 executor, UI0-102 token, UI0-103 biểu tượng, UI0-104 dịch, UI0-114 phím tắt.
trion_integration_test(it_nen_ui_test trion_base trion_appcore trion_tokens trion_i18n trion_keymap)
# Nhóm 2: UI0-002 chủ đề, UI0-005 bản địa hóa, UI0-105 đo độ trễ, UI0-003 font, UI0-004 biểu tượng (+ RGN-013).
trion_integration_test(it_giao_dien_test
  trion_base trion_appcore trion_theme trion_fonts trion_icons trion_i18n trion_perf trion_regen)
