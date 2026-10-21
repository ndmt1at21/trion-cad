# app/main (UI0-001): ứng dụng trion — thứ tự khởi động, vòng đời và thoát, chạy một phiên, mốc khởi động.
# Thư viện trion_application cho exe và test; exe trion chỉ có main.cpp.
trion_add_library(trion_application
  SOURCES Application.h Application.cpp SingleInstance.h SingleInstance.cpp StartupTrace.h StartupTrace.cpp
  LIBS trion_display trion_keymap trion_i18n trion_fonts trion_icons trion_perf trion_appcore Qt6::Widgets Qt6::Network)

add_executable(trion WIN32 main.cpp)
target_link_libraries(trion PRIVATE trion_application)
trion_apply_flags(trion)

trion_add_test(app_main_test SOURCES tests/application_test.cpp LIBS trion_application LABELS app_main)
# Chạy exe trion thật (cùng thư mục với test: <build>/app/main, bản Windows <build>/bin).
trion_add_test(trion_exe_test SOURCES tests/trion_exe_test.cpp LIBS trion_application LABELS app_main)
add_dependencies(trion_exe_test trion)
