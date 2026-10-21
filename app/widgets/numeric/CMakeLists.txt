# app/widgets/numeric (UI0-109): bộ phân tích giá trị của ô số UI-C09 — hàm thuần, ô số UI0-007 và Properties dùng.
# Đọc số qua INT-105 (trion_parse), hiển thị lại qua RGN-013 (trion_regen); Qt Core chỉ cho câu lỗi qua tr().
trion_add_library(trion_numeric SOURCES ValueParser.h ValueParser.cpp LIBS trion_parse trion_regen Qt6::Core)

trion_add_test(numeric_value_parser_test SOURCES tests/value_parser_test.cpp LIBS trion_numeric trion_base
  LABELS numeric)
qt_add_resources(numeric_value_parser_test numeric_golden PREFIX / FILES tests/golden.tsv)

# libFuzzer ghi đầu vào mới vào thư mục corpus đầu tiên: chạy trên bản sao trong thư mục build, không bẩn kho mã.
file(COPY fuzz/corpus DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/fuzz)
trion_add_fuzz(numeric_value_parser_fuzz SOURCES fuzz/value_parser_fuzz.cpp LIBS trion_numeric
  CORPUS ${CMAKE_CURRENT_BINARY_DIR}/fuzz/corpus)
