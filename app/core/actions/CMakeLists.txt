# app/core/actions (UI0-015): ActionRegistry — một QAction cho mỗi lệnh; kích hoạt → chuỗi lệnh (chỗ nối INT-002),
# toggle gắn sysvar (chỗ nối INT-005), phím theo phạm vi Toàn cục/Vùng vẽ (ActionRegistry.h). commands.cpp sinh từ
# danh mục lệnh của prototype bằng actions_gen.mjs (tạm thay INT-001); search_terms.cpp (viết tắt, từ khóa tiếng Việt
# cho ô Tìm lệnh và tự hoàn thành dòng lệnh, SearchTerms.h) bằng search_gen.mjs (tạm thay INT-017).
trion_add_library(trion_actions
  SOURCES ActionRegistry.h ActionRegistry.cpp commands.cpp SearchTerms.h search_terms.cpp
  LIBS trion_keymap trion_tooltip trion_icons trion_i18n Qt6::Widgets)

trion_add_test(actions_test SOURCES tests/actions_test.cpp
  LIBS trion_actions trion_menu trion_cmdline trion_appcore LABELS actions)

# commands.cpp, search_terms.cpp còn khớp danh mục RAW của prototype. Cần node (có trong ci/docker).
find_program(TRION_NODE node)
if(TRION_NODE)
  add_test(NAME actions_prototype_check COMMAND ${TRION_NODE} ${CMAKE_CURRENT_SOURCE_DIR}/actions_gen.mjs --check)
  add_test(NAME actions_search_terms_check COMMAND ${TRION_NODE} ${CMAKE_CURRENT_SOURCE_DIR}/search_gen.mjs --check)
  set_tests_properties(actions_prototype_check actions_search_terms_check PROPERTIES LABELS actions TIMEOUT 60)
else()
  message(WARNING "actions: không thấy node, bỏ test actions_prototype_check, actions_search_terms_check")
endif()
