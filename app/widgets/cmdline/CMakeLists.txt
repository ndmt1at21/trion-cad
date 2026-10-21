# app/widgets/cmdline (UIA-106): ô nhập của dòng lệnh, gõ tiếng Việt và bỏ qua bộ gõ khi chờ tên lệnh.
# UIA-107 thêm prompt, chip tùy chọn, lịch sử quanh ô này.
trion_add_library(trion_cmdline SOURCES CommandInput.h CommandInput.cpp LIBS trion_ime trion_fonts trion_base Qt6::Widgets)

trion_add_test(cmdline_command_input_test SOURCES tests/command_input_test.cpp LIBS trion_cmdline trion_perf
  LABELS cmdline)
