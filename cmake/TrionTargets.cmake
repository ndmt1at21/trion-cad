# trion_add_library(<tên> [STATIC] SOURCES ... LIBS ...)
# trion_add_test(<tên> SOURCES ... LIBS ... [LABELS ...] [TIMEOUT s])
# trion_add_fuzz(<tên> SOURCES ... LIBS ... [CORPUS thư mục]): chỉ build ở preset fuzz. ctest chạy
#   khói 20 s (nhãn fuzz); chạy dài: build/fuzz/bin/<tên> -max_total_time=3600 <corpus>.
#   Test dùng Qt Test (có sẵn trong Qt, 05 §2). Chạy với QT_QPA_PLATFORM=offscreen.
#   DoD-C 2: bộ test của một module chạy dưới 60 s, nên TIMEOUT mặc định 60. Mọi test C++ có thêm nhãn cpp
#   (bản Windows chỉ chạy nhãn này: test Python/node chạy ở bản Linux).
# Bản Windows (preset win): mọi .exe vào <build>/bin cạnh DLL của Qt (trion_deploy_qt_runtime), nên
# chạy được ngay, kể cả từ WSL (ctest trên WSL gọi .exe; biến môi trường qua WSLENV).

if(WIN32)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

# Chép DLL và plugin Qt cần cho test và ứng dụng vào <build>/bin. Gọi sau find_package(Qt6).
function(trion_deploy_qt_runtime)
  if(NOT WIN32)
    return()
  endif()
  set(qt ${QT6_INSTALL_PREFIX})
  set(bin ${CMAKE_BINARY_DIR}/bin)
  foreach(dll Qt6Core Qt6Gui Qt6Widgets Qt6Svg Qt6Test)
    if(EXISTS ${qt}/bin/${dll}.dll)
      file(COPY ${qt}/bin/${dll}.dll DESTINATION ${bin})
    endif()
  endforeach()
  # libc++ của chính trình biên dịch (llvm-mingw), bản đi kèm Qt chỉ là dự phòng: bản của Qt cũ hơn (LLVM 17) thiếu hàm
  # mà header mới gọi tới (from_chars(double)…) nên exe không khởi động; libc++ giữ ABI, Qt chạy được với bản mới hơn.
  get_filename_component(tc ${CMAKE_CXX_COMPILER} DIRECTORY)
  foreach(dll libc++ libunwind)
    foreach(dir ${tc}/../x86_64-w64-mingw32/bin ${qt}/bin)
      if(EXISTS ${dir}/${dll}.dll)
        file(COPY ${dir}/${dll}.dll DESTINATION ${bin})
        break()
      endif()
    endforeach()
  endforeach()
  foreach(plugin platforms/qwindows platforms/qoffscreen iconengines/qsvgicon imageformats/qsvg styles/qmodernwindowsstyle)
    if(EXISTS ${qt}/plugins/${plugin}.dll)
      get_filename_component(dir ${plugin} DIRECTORY)
      file(COPY ${qt}/plugins/${plugin}.dll DESTINATION ${bin}/${dir})
    endif()
  endforeach()
endfunction()

function(trion_add_library name)
  cmake_parse_arguments(A "" "" "SOURCES;LIBS" ${ARGN})
  add_library(${name} STATIC ${A_SOURCES})
  target_link_libraries(${name} PUBLIC ${A_LIBS})
  target_include_directories(${name} PUBLIC ${CMAKE_SOURCE_DIR})
  trion_apply_flags(${name})
endfunction()

function(trion_add_fuzz name)
  cmake_parse_arguments(A "" "CORPUS" "SOURCES;LIBS" ${ARGN})
  if(NOT TRION_FUZZ)
    return()
  endif()
  add_executable(${name} ${A_SOURCES})
  target_link_libraries(${name} PRIVATE ${A_LIBS})
  target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR})
  trion_apply_flags(${name})
  target_link_options(${name} PRIVATE -fsanitize=fuzzer)
  set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  add_test(NAME ${name} COMMAND ${name} -max_total_time=20 ${A_CORPUS})
  set_tests_properties(${name} PROPERTIES LABELS fuzz TIMEOUT 60)
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
  set(env "QT_QPA_PLATFORM=offscreen;QT_ENABLE_HIGHDPI_SCALING=0")
  set(wslenv "QT_QPA_PLATFORM:QT_ENABLE_HIGHDPI_SCALING")
  if(WIN32) # offscreen trên Windows tìm font ở <Qt>/lib/fonts (trống): không có font nào, đo chữ sai
    string(APPEND env ";QT_QPA_FONTDIR=C:/Windows/Fonts")
    string(APPEND wslenv ":QT_QPA_FONTDIR")
  endif()
  set_tests_properties(${name} PROPERTIES
    TIMEOUT ${A_TIMEOUT}
    LABELS "${A_LABELS};cpp"
    ENVIRONMENT "${env};WSLENV=${wslenv}")
endfunction()
