# app/core/i18n (UI0-104): Translator, đổi ngôn ngữ giao diện khi đang chạy. Bước lupdate/lrelease và nhúng
# .qm vào trion_i18n ở app/i18n/i18n.cmake (gọi ở cuối CMakeLists.txt gốc, sau mọi thành phần).
trion_add_library(trion_i18n
  SOURCES Translator.h Translator.cpp
  LIBS trion_base Qt6::Core)

trion_add_test(i18n_translator_test SOURCES tests/translator_test.cpp LIBS trion_i18n Qt6::Widgets LABELS i18n)
