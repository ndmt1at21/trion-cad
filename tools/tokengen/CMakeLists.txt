# Bộ sinh token (UI0-102). Bản thân bộ sinh chạy lúc build ở app/theme/CMakeLists.txt; đây chỉ là test của nó.
add_test(NAME tokengen_test COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tests)
set_tests_properties(tokengen_test PROPERTIES LABELS tokengen TIMEOUT 60 ENVIRONMENT PYTHONDONTWRITEBYTECODE=1)
