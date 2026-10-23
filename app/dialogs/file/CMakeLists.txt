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

# UIB-002: OpenFileDialog — hộp Mở tệp của hệ điều hành (Windows: IFileOpenDialog + IFileDialogCustomize; nơi khác:
# QFileDialog của Qt kèm FilePreviewPane), bộ lọc DWG/DXF/DWT, chọn nhiều tệp, "Mở chỉ đọc", nhớ thư mục cuối.
target_sources(trion_filedialogs PRIVATE OpenFileDialog.h OpenFileDialog.cpp)
target_link_libraries(trion_filedialogs PUBLIC trion_menu trion_message trion_controls trion_i18n trion_fonts trion_icons
  trion_theme Qt6::Widgets)
if(WIN32)
  target_link_libraries(trion_filedialogs PRIVATE ole32 shell32 uuid)
endif()
trion_add_test(open_dialog_test SOURCES tests/open_dialog_test.cpp
  LIBS trion_filedialogs trion_tooltip trion_keymap trion_i18n LABELS filedialogs)
