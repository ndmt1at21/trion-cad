# app/widgets/dialog (UI0-110): khung hộp thoại DialogFrame (đầu, thân, chân, phím, focus, đang tải, ẩn tạm) và
# DialogHost (mở theo mã, một bản mỗi mã, nhớ vị trí và cỡ, danh sách mã cho CI).
trion_add_library(trion_dialog
  SOURCES DialogFrame.h DialogFrame.cpp DialogHost.h DialogHost.cpp
  LIBS trion_controls trion_progress trion_fonts trion_appcore trion_theme trion_base Qt6::Widgets)

trion_add_test(dialog_test SOURCES tests/dialog_test.cpp LIBS trion_dialog trion_i18n trion_perf LABELS dialog)
# Ảnh baseline khung hộp thoại theo nền tảng × chủ đề × DPR, nhúng vào test (như controls); ghi lại:
# TRION_UPDATE_BASELINE=<thư mục> chạy dialog_test, chép frame-*-<nền tảng>.png vào tests/baseline.
file(GLOB baseline CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/baseline/*.png)
if(baseline)
  qt_add_resources(dialog_test dialog_baseline PREFIX /baseline BASE tests/baseline FILES ${baseline})
endif()

# UI0-008: xác nhận phá hủy (ConfirmDialog), "Không hỏi lại" (SuppressedPrompts), hộp thoại lỗi (ErrorDialog). Cùng
# trion_dialog: dựng trên DialogFrame, dùng chung chỗ với mọi hộp thoại. TRION_VERSION: phiên bản trong chi tiết lỗi.
target_sources(trion_dialog PRIVATE ConfirmDialog.h ConfirmDialog.cpp SuppressedPrompts.h SuppressedPrompts.cpp
                                    ErrorDialog.h ErrorDialog.cpp)
target_link_libraries(trion_dialog PRIVATE trion_i18n)
target_compile_definitions(trion_dialog PRIVATE TRION_VERSION="${PROJECT_VERSION}")
trion_add_test(confirm_test SOURCES tests/confirm_test.cpp LIBS trion_dialog trion_i18n trion_perf LABELS dialog)
# Ảnh baseline hộp xác nhận và hộp lỗi (cùng cách, cùng thư mục với frame-*): ghi lại bằng TRION_UPDATE_BASELINE=<thư
# mục> chạy confirm_test, chép confirm-*, error-*-<nền tảng>.png vào tests/baseline.
file(GLOB prompt_baseline CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/baseline/confirm-*.png
     tests/baseline/error-*.png)
if(prompt_baseline)
  qt_add_resources(confirm_test confirm_baseline PREFIX /baseline BASE tests/baseline FILES ${prompt_baseline})
endif()
