# app/main (UI0-001): ứng dụng trion — thứ tự khởi động, vòng đời và thoát, chạy một phiên, mốc khởi động.
# Thư viện trion_application cho exe và test; exe trion chỉ có main.cpp. ShellAssembly (integration nhóm 5, 6): lắp khung
# ứng dụng đợt 5, 6 (thanh tiêu đề, ribbon, tab tệp, trang Start, dòng lệnh, thanh trạng thái, thông báo) vào cửa sổ
# chính.
trion_add_library(trion_application
  SOURCES Application.h Application.cpp ShellAssembly.h ShellAssembly.cpp SingleInstance.h SingleInstance.cpp
    StartupTrace.h StartupTrace.cpp
  LIBS trion_shell trion_titlebar trion_filetabs trion_ribbon trion_commandline trion_status trion_start trion_progress
    trion_jobs trion_gallery trion_tooltip trion_display trion_keymap trion_i18n trion_fonts trion_icons trion_perf
    trion_appcore Qt6::Widgets Qt6::Network)

add_executable(trion WIN32 main.cpp)
target_link_libraries(trion PRIVATE trion_application)
trion_apply_flags(trion)

trion_add_test(app_main_test SOURCES tests/application_test.cpp LIBS trion_application LABELS app_main)
# Chạy exe trion thật (cùng thư mục với test: <build>/app/main, bản Windows <build>/bin).
trion_add_test(trion_exe_test SOURCES tests/trion_exe_test.cpp LIBS trion_application LABELS app_main)
add_dependencies(trion_exe_test trion)
# Chạy một mình (ctest -j): đo khởi động, CPU lúc rảnh; dựng các tiến trình trion thật (tiến trình con sót lại khi
# test bị ctest dừng giữ phiên duy nhất, làm lần chạy sau treo). ~13 lần chạy trion: trên Windows qua WSL mỗi lần nạp DLL
# qua ổ chia sẻ ~1–3 s, cả bộ ~60 s, nên giới hạn 120 s.
set_tests_properties(trion_exe_test PROPERTIES RUN_SERIAL TRUE TIMEOUT 120)
