# app/system/about (UIL-011): hộp Giới thiệu (lệnh ABOUT, UI-L13) — AboutDialog (phiên bản, bản build, kênh, huy hiệu
# phiên bản, thông tin hệ thống có Sao chép, thư viện mã nguồn mở và văn bản giấy phép, điều khoản, nhãn hiệu, hỗ trợ).
#
# licenses.json sinh lúc build bằng gen_licenses.py từ third_party.json (ponytail: danh sách tĩnh theo 05 §2 tới khi có
# vcpkg manifest NT-001) và licenses/*.txt; thiếu văn bản giấy phép thì bước sinh lỗi, build dừng. Nhúng
# :/trion/about/licenses.json.
set(TRION_BUILD_ID "dev" CACHE STRING "Mã bản build ở hộp Giới thiệu (pipeline phát hành đặt, ví dụ 2026.10.23.1)")
set(TRION_CHANNEL "internal" CACHE STRING "Kênh phát hành ở hộp Giới thiệu: internal, beta, stable")
set_property(CACHE TRION_CHANNEL PROPERTY STRINGS internal beta stable)

set(gen ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(GLOB license_texts CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/licenses/*.txt)
add_custom_command(
  OUTPUT ${gen}/licenses.json
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gen_licenses.py ${CMAKE_CURRENT_SOURCE_DIR}/third_party.json
          ${gen}/licenses.json --root ${CMAKE_SOURCE_DIR}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/gen_licenses.py ${CMAKE_CURRENT_SOURCE_DIR}/third_party.json ${license_texts}
          ${CMAKE_SOURCE_DIR}/app/theme/fonts/OFL.txt
  COMMENT "gen_licenses: licenses.json (UIL-011)"
  VERBATIM)

trion_add_library(trion_about
  SOURCES AboutDialog.h AboutDialog.cpp
  LIBS trion_dialog trion_message trion_edition trion_views trion_controls trion_icons trion_fonts trion_i18n
       trion_appcore trion_theme trion_base Qt6::Widgets)
target_compile_definitions(trion_about PRIVATE TRION_VERSION="${PROJECT_VERSION}" TRION_BUILD_ID="${TRION_BUILD_ID}"
                                               TRION_CHANNEL="${TRION_CHANNEL}")
qt_add_resources(trion_about about_licenses PREFIX /trion/about BASE ${gen} FILES ${gen}/licenses.json)

trion_add_test(about_test SOURCES tests/about_test.cpp
  LIBS trion_about trion_a11y trion_perf trion_appcore trion_i18n LABELS about)
# Tiêu chí 2 (CI): thiếu văn bản giấy phép → lỗi; danh sách thật đạt. Chỉ bản Linux (test Python).
add_test(NAME about_gen_licenses_test
  COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tests -p "test_*.py")
set_tests_properties(about_gen_licenses_test PROPERTIES LABELS about TIMEOUT 60 ENVIRONMENT PYTHONDONTWRITEBYTECODE=1)
