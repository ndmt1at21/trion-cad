# app/core/i18n (UI0-104, UI0-005): Translator (đổi ngôn ngữ khi đang chạy, ngôn ngữ giả vi-XL), UiFormat (số,
# ngày theo ngôn ngữ giao diện), TruncationProbe (tìm chữ bị cắt). Bước lupdate/lrelease của chuỗi TrionCAD và
# nhúng .qm vào trion_i18n ở app/i18n/i18n.cmake (gọi ở cuối CMakeLists.txt gốc, sau mọi thành phần).
trion_add_library(trion_i18n
  SOURCES Translator.h Translator.cpp UiFormat.h UiFormat.cpp TruncationProbe.h TruncationProbe.cpp
  LIBS trion_base Qt6::Widgets)

# Chuỗi có sẵn của Qt bằng tiếng Việt (Qt 6.11 không phát hành qtbase_vi): app/i18n/qtbase_vi.ts viết tay theo
# bảng thuật ngữ, không qua lupdate; nhúng tại :/i18n/qtbase_vi.qm.
qt_add_lrelease(TS_FILES ${CMAKE_SOURCE_DIR}/app/i18n/qtbase_vi.ts LRELEASE_TARGET trion_qtbase_vi_lrelease
  NO_GLOBAL_TARGET QM_FILES_OUTPUT_VARIABLE qtbase_qm OPTIONS -silent)
qt_add_resources(trion_i18n qtbase_vi PREFIX /i18n BASE ${CMAKE_CURRENT_BINARY_DIR} FILES ${qtbase_qm})

foreach(t translator uiformat truncation)
  trion_add_test(i18n_${t}_test SOURCES tests/${t}_test.cpp LIBS trion_i18n Qt6::Widgets LABELS i18n)
endforeach()
