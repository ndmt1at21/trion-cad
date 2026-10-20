# Cờ biên dịch chung (05 ADR-001, ADR-004; README DoD-C 1, 3, 5).
# Áp cho mọi target của TrionCAD qua trion_apply_flags(); không đặt cờ toàn cục để
# mã sinh tự động và thư viện ngoài không bị -Werror.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Sanitizer theo preset: "address,undefined" hoặc "thread". Rỗng là tắt.
set(TRION_SANITIZE "" CACHE STRING "Sanitizer bật cho mọi target (address,undefined | thread)")
# Preset fuzz: mọi target được đo phủ cho libFuzzer; target fuzz tạo bằng trion_add_fuzz (DoD-C 7).
option(TRION_FUZZ "Build target fuzz (clang, libFuzzer)" OFF)

function(trion_apply_flags target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus /fp:precise)
  else()
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter
      -ffp-contract=off -fno-fast-math)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
      target_compile_options(${target} PRIVATE -march=x86-64-v2)
    endif()
  endif()
  if(TRION_SANITIZE)
    target_compile_options(${target} PRIVATE -fsanitize=${TRION_SANITIZE} -fno-omit-frame-pointer -fno-sanitize-recover=all)
    target_link_options(${target} PRIVATE -fsanitize=${TRION_SANITIZE})
  endif()
  if(TRION_FUZZ)
    target_compile_options(${target} PRIVATE -fsanitize=fuzzer-no-link)
  endif()
endfunction()
