# Thành phần tools/iconconv (UI0-103): icons.js của prototype → app/theme/icons (SVG + manifest), báo cáo phủ,
# so raster QtSvg với ảnh tham chiếu chụp từ trình duyệt.
add_test(NAME iconconv_selftest COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tests)
set_tests_properties(iconconv_selftest PROPERTIES LABELS iconconv TIMEOUT 60)
# Báo cáo phủ đọc JS của prototype bằng node (có trong image CI Linux).
find_program(NODE_EXECUTABLE node)
if(NODE_EXECUTABLE)
  add_test(NAME iconconv_coverage COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/coverage_report.py)
  set_tests_properties(iconconv_coverage PROPERTIES LABELS iconconv TIMEOUT 60)
else()
  message(STATUS "iconconv: không thấy node, bỏ test iconconv_coverage")
endif()

trion_add_test(iconraster_test SOURCES tests/iconraster_test.cpp LIBS Qt6::Gui Qt6::Svg LABELS iconconv)
target_compile_definitions(iconraster_test PRIVATE TRION_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
