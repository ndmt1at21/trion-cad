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

# UI0-002: ThemeManager (QPalette + TrionStyle + QSS từ qss/app.qss.in), màu ngữ nghĩa 07.1 §13.2.
trion_add_library(trion_theme
  SOURCES ThemeManager.h ThemeManager.cpp SemanticColors.h SemanticColors.cpp
  LIBS trion_tokens trion_base Qt6::Widgets)
qt_add_resources(trion_theme theme_qss PREFIX /trion/theme BASE qss FILES qss/app.qss.in)

trion_add_test(theme_switch_test SOURCES tests/theme_test.cpp LIBS trion_theme LABELS theme)
trion_add_test(theme_semantic_test SOURCES tests/semantic_test.cpp LIBS trion_theme LABELS theme)

# Lint DoD-U 2: màu, cỡ chữ, khoảng cách viết thẳng trong app/ (cả thành phần chưa cấu hình) làm PR lỗi.
set(no_literal ${CMAKE_SOURCE_DIR}/ci/lint/no-literal-style)
add_test(NAME theme_no_literal_style COMMAND ${Python3_EXECUTABLE} ${no_literal}/lint.py ${CMAKE_SOURCE_DIR}/app)
add_test(NAME theme_no_literal_style_selftest COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${no_literal}/tests)
set_tests_properties(theme_no_literal_style theme_no_literal_style_selftest PROPERTIES
  LABELS theme ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
