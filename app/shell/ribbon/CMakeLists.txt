# app/shell/ribbon (UIA-102): mô hình ribbon JSON (RibbonModel) và các loại điều khiển (controls/RibbonControls);
# (UIA-005) khung ribbon RibbonBar: dải tab, khung panel, co theo ưu tiên, 3 mức thu gọn, panel mở rộng có ghim.
# Nội dung 8 tab (resources/*.json) và nhãn cho lupdate (ribbon_strings.cpp) sinh từ prototype bằng ribbon_gen.mjs;
# JSON nhúng ở :/trion/ribbon/ (resources/schema.json chỉ là mô tả khóa, không nhúng).
trion_add_library(trion_ribbon
  SOURCES RibbonModel.h RibbonModel.cpp ribbon_strings.cpp controls/RibbonControls.h controls/RibbonControls.cpp
          RibbonBar.h RibbonBar.cpp
  LIBS trion_actions trion_edition trion_controls trion_menu trion_numeric_field trion_tooltip trion_icons
       trion_fonts trion_theme trion_base Qt6::Widgets)
file(GLOB ribbon_json CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} resources/*.json)
list(REMOVE_ITEM ribbon_json resources/schema.json)
qt_add_resources(trion_ribbon ribbon_json PREFIX /trion/ribbon BASE resources FILES ${ribbon_json})

trion_add_test(ribbon_test SOURCES tests/ribbon_test.cpp LIBS trion_ribbon trion_keymap trion_i18n trion_appcore
  LABELS ribbon)
# UIA-005: khung ribbon; số đo đổi tab, tính lại bố cục khi kéo (benchmark dạng test, ngưỡng khi kTimed).
trion_add_test(ribbon_bar_test SOURCES tests/ribbon_bar_test.cpp LIBS trion_ribbon trion_keymap trion_i18n
  trion_appcore LABELS ribbon bench)

# Tiêu chí 1, chạy trong CI: lệnh có trong ActionRegistry, biểu tượng có trong bộ UI0-103, nhãn có bản English trong
# app/i18n/trion_en.ts (nhúng vào test để bản Windows đọc được).
trion_add_test(ribbon_lint_test SOURCES tests/ribbon_lint_test.cpp LIBS trion_ribbon trion_keymap LABELS ribbon)
qt_add_resources(ribbon_lint_test ribbon_lint_ts PREFIX /ribbon_lint BASE ${CMAKE_SOURCE_DIR}/app/i18n
  FILES ${CMAKE_SOURCE_DIR}/app/i18n/trion_en.ts)

# resources/*.json, ribbon_strings.cpp còn khớp TC.RIBBON_PRO của prototype. Cần node (có trong ci/docker).
find_program(TRION_NODE node)
if(TRION_NODE)
  add_test(NAME ribbon_prototype_check COMMAND ${TRION_NODE} ${CMAKE_CURRENT_SOURCE_DIR}/ribbon_gen.mjs --check)
  set_tests_properties(ribbon_prototype_check PROPERTIES LABELS ribbon TIMEOUT 60)
else()
  message(WARNING "ribbon: không thấy node, bỏ test ribbon_prototype_check")
endif()
