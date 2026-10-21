# app/widgets/menu (UI0-107): Menu — QMenu vẽ theo token (mục, nhóm, phím theo Keymap và nền tảng, huy hiệu khóa,
# trợ năng), tooltip 2 mức trên mục (Menu.h).
trion_add_library(trion_menu
  SOURCES Menu.h Menu.cpp
  LIBS trion_tooltip trion_icons trion_fonts trion_theme Qt6::Widgets)

trion_add_test(menu_test SOURCES tests/menu_test.cpp LIBS trion_menu trion_perf trion_i18n trion_keymap LABELS menu)
# Ảnh baseline menu và tooltip theo nền tảng × chủ đề × DPR, nhúng vào test để bản Windows đọc được; ghi lại:
# TRION_UPDATE_BASELINE=<thư mục> chạy menu_test, chép ảnh vào tests/baseline (xem tests/menu_test.cpp).
file(GLOB baseline CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/baseline/*.png)
if(baseline)
  qt_add_resources(menu_test menu_baseline PREFIX /baseline BASE tests/baseline FILES ${baseline})
endif()
