# app/core/ime (UIA-106, spike S6): bỏ qua bộ gõ tiếng Việt khi ô dòng lệnh chờ tên lệnh, chuẩn hóa commit NFC.
# Phần thuần biên dịch mọi nền tảng; hook WH_KEYBOARD_LL cho bộ gõ kiểu hook (Unikey, EVKey, OpenKey) chỉ có ở
# Windows. macOS, Linux không cần mã riêng: bộ gõ ở đó đi qua input method của Qt (docs/spikes/S6-ime.md).
set(src ImeBypass.h ImeBypass.cpp)
if(WIN32)
  list(APPEND src ImeBypass_win.cpp)
endif()
trion_add_library(trion_ime SOURCES ${src} LIBS Qt6::Widgets)
if(WIN32)
  target_link_libraries(trion_ime PRIVATE user32)
endif()

trion_add_test(ime_bypass_test SOURCES tests/ime_bypass_test.cpp LIBS trion_ime LABELS ime)
