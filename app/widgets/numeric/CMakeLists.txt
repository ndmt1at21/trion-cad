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

# UI0-007: ô số NumericField (widget). Thư viện riêng để bộ phân tích ở trên (hàm thuần, fuzz, dòng lệnh dùng) không
# kéo theo Qt Widgets và hộp thoại.
trion_add_library(trion_numeric_field
  SOURCES NumericField.h NumericField.cpp
  LIBS trion_numeric trion_dialog trion_icons trion_fonts trion_theme trion_base Qt6::Widgets)

trion_add_test(numeric_field_test SOURCES tests/numeric_field_test.cpp
  LIBS trion_numeric_field trion_i18n trion_perf trion_appcore LABELS numeric)
# Ảnh baseline theo nền tảng × chủ đề × DPR, nhúng vào test (như controls); ghi lại: TRION_UPDATE_BASELINE=<thư mục>
# chạy numeric_field_test, chép field-*-<nền tảng>.png vào tests/baseline.
file(GLOB field_baseline CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/baseline/*.png)
if(field_baseline)
  qt_add_resources(numeric_field_test numeric_field_baseline PREFIX /baseline BASE tests/baseline
    FILES ${field_baseline})
endif()
