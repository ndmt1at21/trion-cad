# Integration test: chạy nhiều thành phần cùng nhau, sau mỗi nhóm 3–5 task (xem ci/integration.sh).
# Chỉ build khi các thành phần cần đều được cấu hình (TRION_COMPONENTS có thể chọn một phần).

# Nhóm 1: UI0-101 executor, UI0-102 token, UI0-103 biểu tượng, UI0-104 dịch, UI0-114 phím tắt.
set(it1_libs trion_base trion_appcore trion_tokens trion_i18n trion_keymap)
set(it1_ok ON)
foreach(t IN LISTS it1_libs)
  if(NOT TARGET ${t})
    set(it1_ok OFF)
  endif()
endforeach()
if(it1_ok)
  trion_add_test(it_nen_ui_test SOURCES it_nen_ui_test.cpp
    LIBS ${it1_libs} Qt6::Widgets Qt6::Svg LABELS integration)
  target_compile_definitions(it_nen_ui_test PRIVATE TRION_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
endif()
