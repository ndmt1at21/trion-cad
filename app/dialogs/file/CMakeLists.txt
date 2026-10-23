# app/dialogs/file: hộp thoại và dịch vụ tệp bản vẽ (07.1 UI-B01…).
# UIB-001: FilePreviewService — đọc nhanh thông tin xem trước (phiên bản DWG/DXF, ngày sửa, dung lượng; chỗ nối ảnh nhúng
# DWG-026 và "lưu lần cuối bởi" DWG-027), nền, yêu cầu mới hủy yêu cầu cũ, LRU 64 mục.
trion_add_library(trion_filedialogs
  SOURCES FilePreviewService.h FilePreviewService.cpp
  LIBS trion_appcore trion_base Qt6::Gui)

trion_add_test(file_preview_test SOURCES tests/file_preview_test.cpp LIBS trion_filedialogs LABELS filedialogs)

# Fuzz bộ đọc đầu tệp (DoD-C 7: tệp bản vẽ là đầu vào không tin cậy), chỉ ở preset fuzz.
file(COPY fuzz/corpus DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/fuzz)
trion_add_fuzz(file_header_fuzz SOURCES fuzz/file_header_fuzz.cpp LIBS trion_filedialogs
  CORPUS ${CMAKE_CURRENT_BINARY_DIR}/fuzz/corpus)
