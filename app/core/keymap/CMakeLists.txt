# app/core/keymap: bảng phím tắt một nguồn và nhãn phím theo nền tảng (UI0-114).
trion_add_library(trion_keymap SOURCES Keymap.h Keymap.cpp keymap_strings.cpp LIBS trion_base Qt6::Gui)
qt_add_resources(trion_keymap keymap PREFIX /keymap FILES keymap.json)

trion_add_test(keymap_test SOURCES tests/keymap_test.cpp LIBS trion_keymap LABELS keymap)
# Ca vàng nhúng vào test: chạy được cả khi test chạy trên máy khác nơi build (preset win).
qt_add_resources(keymap_test keymap_golden PREFIX / FILES tests/golden.json)

# keymap.json, keymap_strings.cpp, tests/golden.json còn khớp prototype: so từng dòng KDEF, từng trường.
# Cần node (có trong ci/docker).
find_program(TRION_NODE node)
if(TRION_NODE)
  add_test(NAME keymap_prototype_check
    COMMAND ${TRION_NODE} ${CMAKE_CURRENT_SOURCE_DIR}/keymap_gen.mjs --check)
  set_tests_properties(keymap_prototype_check PROPERTIES LABELS keymap TIMEOUT 60)
else()
  message(WARNING "keymap: không thấy node, bỏ test keymap_prototype_check")
endif()
