# trion_add_library(<tên> [STATIC] SOURCES ... LIBS ...)
# trion_add_test(<tên> SOURCES ... LIBS ... [LABELS ...] [TIMEOUT s])
#   Test dùng Qt Test (có sẵn trong Qt, 05 §2). Chạy với QT_QPA_PLATFORM=offscreen.
#   DoD-C 2: bộ test của một module chạy dưới 60 s, nên TIMEOUT mặc định 60.

function(trion_add_library name)
  cmake_parse_arguments(A "" "" "SOURCES;LIBS" ${ARGN})
  add_library(${name} STATIC ${A_SOURCES})
  target_link_libraries(${name} PUBLIC ${A_LIBS})
  target_include_directories(${name} PUBLIC ${CMAKE_SOURCE_DIR})
  trion_apply_flags(${name})
endfunction()

function(trion_add_test name)
  cmake_parse_arguments(A "" "TIMEOUT" "SOURCES;LIBS;LABELS" ${ARGN})
  add_executable(${name} ${A_SOURCES})
  target_link_libraries(${name} PRIVATE Qt6::Test ${A_LIBS})
  target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR})
  trion_apply_flags(${name})
  add_test(NAME ${name} COMMAND ${name})
  if(NOT A_TIMEOUT)
    set(A_TIMEOUT 60)
  endif()
  set_tests_properties(${name} PROPERTIES
    TIMEOUT ${A_TIMEOUT}
    LABELS "${A_LABELS}"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_ENABLE_HIGHDPI_SCALING=0")
endfunction()
