# app/theme. UI0-102: token sinh từ design/prototype/css/tokens.css, kiểm tương phản theo contrast-pairs.txt.
# Chỉ chạy lại khi tokens.css, bộ sinh hoặc tệp cặp đổi; Tokens.h chỉ ghi lại khi nội dung đổi (ninja restat),
# nên sửa tokens.css chỉ build lại mã include Tokens.h.
set(TRION_TOKENS_CSS ${CMAKE_SOURCE_DIR}/design/prototype/css/tokens.css)
set(gen ${CMAKE_CURRENT_BINARY_DIR}/generated)
add_custom_command(
  OUTPUT ${gen}/Tokens.h ${gen}/tokens.json
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/tokengen/tokengen.py
          ${TRION_TOKENS_CSS} ${CMAKE_CURRENT_SOURCE_DIR}/contrast-pairs.txt ${gen}
  DEPENDS ${TRION_TOKENS_CSS} ${CMAKE_SOURCE_DIR}/tools/tokengen/tokengen.py
          ${CMAKE_CURRENT_SOURCE_DIR}/contrast-pairs.txt
  COMMENT "tokengen: Tokens.h, tokens.json, kiểm tương phản"
  VERBATIM)
# #include "app/theme/generated/Tokens.h"; target nào link trion_tokens thì build sau bước sinh.
add_library(trion_tokens INTERFACE ${gen}/Tokens.h ${gen}/tokens.json)
target_include_directories(trion_tokens INTERFACE ${CMAKE_BINARY_DIR})

trion_add_test(theme_tokens_test SOURCES tests/tokens_test.cpp LIBS trion_tokens LABELS theme)
