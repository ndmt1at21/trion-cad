# cmd: hiện có parser giá trị nhập (INT-105); các phần lệnh, prompt khác thêm vào sau.
trion_add_library(trion_parse SOURCES parse/ValueParser.h parse/ValueParser.cpp LIBS trion_regen)

# Bản release kiểm vòng 1 triệu giá trị × 90 cặp (~13 giây CPU, song song): 5–15 s khi máy bình thường, tới ~55 s khi
# máy dùng chung quá tải (load ≈ 35 trên 12 luồng); TIMEOUT 120 để không hỏng giả.
trion_add_test(cmd_value_parser_test SOURCES parse/tests/value_parser_test.cpp LIBS trion_parse trion_base LABELS cmd
  TIMEOUT 120)
qt_add_resources(cmd_value_parser_test value_parser_golden PREFIX / FILES parse/tests/golden.tsv)

# libFuzzer ghi đầu vào mới vào thư mục corpus đầu tiên: chạy trên bản sao trong thư mục build, không bẩn kho mã.
file(COPY fuzz/corpus DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/fuzz)
trion_add_fuzz(value_parser_fuzz SOURCES fuzz/value_parser_fuzz.cpp LIBS trion_parse
  CORPUS ${CMAKE_CURRENT_BINARY_DIR}/fuzz/corpus)
