add_test(NAME ci_check_flags_selftest COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tests)
# Kho mã thật có 0 vi phạm (NT-001 tiêu chí 2).
add_test(NAME ci_check_flags COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_flags.py ${CMAKE_BINARY_DIR}/compile_commands.json)
