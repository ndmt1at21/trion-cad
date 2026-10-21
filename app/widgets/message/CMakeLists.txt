# app/widgets/message (UI0-009): thông điệp 07.1 §12 — CanvasBanner, Toast/ToastHost, NotificationCenter, CursorTip,
# EmptyState, FieldMessage (Messages.h).
trion_add_library(trion_message
  SOURCES Messages.h Messages.cpp
  LIBS trion_progress trion_controls trion_fonts trion_icons trion_theme trion_base Qt6::Widgets)

trion_add_test(message_test SOURCES tests/message_test.cpp LIBS trion_message trion_i18n LABELS message)
# Ảnh baseline theo nền tảng × chủ đề × DPR (như app/widgets/controls), nhúng vào test để bản Windows đọc được; ghi
# lại: TRION_UPDATE_BASELINE=<thư mục> chạy message_test, chép ảnh vào tests/baseline.
file(GLOB baseline CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/baseline/*.png)
if(baseline)
  qt_add_resources(message_test message_baseline PREFIX /baseline BASE tests/baseline FILES ${baseline})
endif()
