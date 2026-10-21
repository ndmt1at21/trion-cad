# app/widgets/gallery (UI0-006): bảng mẫu mọi thành phần cơ bản ở 8 trạng thái của 07.3 Phụ lục C (Gallery.h);
# exe trion_gallery chạy riêng, `trion --gallery` mở cùng cửa sổ trong ứng dụng.
trion_add_library(trion_gallery
  SOURCES Gallery.h Gallery.cpp
  LIBS trion_controls trion_menu trion_tooltip trion_views trion_model trion_i18n trion_fonts trion_icons trion_theme
       Qt6::Widgets)

# Exe tên trion_gallery (target khác tên thư viện).
add_executable(trion_gallery_exe WIN32 main.cpp)
set_target_properties(trion_gallery_exe PROPERTIES OUTPUT_NAME trion_gallery)
target_link_libraries(trion_gallery_exe PRIVATE trion_gallery trion_display trion_keymap trion_perf trion_appcore)
trion_apply_flags(trion_gallery_exe)

trion_add_test(gallery_test SOURCES tests/gallery_test.cpp
  LIBS trion_gallery trion_keymap trion_perf trion_appcore LABELS gallery)
# Ảnh baseline cả trang theo nền tảng × chủ đề × mật độ × DPR, nhúng vào test để bản Windows đọc được; ghi lại:
# TRION_UPDATE_BASELINE=<thư mục> chạy gallery_test, chép ảnh vào tests/baseline (xem tests/gallery_test.cpp).
file(GLOB baseline CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} tests/baseline/*.png)
if(baseline)
  qt_add_resources(gallery_test gallery_baseline PREFIX /baseline BASE tests/baseline FILES ${baseline})
endif()
