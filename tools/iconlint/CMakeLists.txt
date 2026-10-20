# Thành phần tools/iconlint (UI0-103): lint quy cách SVG biểu tượng; chạy trên toàn bộ app/theme/icons/src.
add_test(NAME iconlint_selftest COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tests)
add_test(NAME iconlint_icons COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/iconlint.py ${CMAKE_SOURCE_DIR}/app/theme/icons/src)
set_tests_properties(iconlint_selftest iconlint_icons PROPERTIES LABELS iconlint TIMEOUT 60)
