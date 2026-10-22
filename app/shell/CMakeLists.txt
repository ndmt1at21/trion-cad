# app/shell (UIA-001): cửa sổ chính UI-A01 — vùng (ô giữ chỗ theo token, setRegion), palette dock (PaletteHost),
# workspace dựng sẵn (mô tả JSON trong resources/workspaces), Clean Screen (CLEANSCREENON/OFF, Ctrl+0), bộ định tuyến
# lệnh tạm (INT-002 thay). Thư mục con titlebar, filetabs, ribbon, cmdline, status là thành phần riêng.
trion_add_library(trion_shell
  SOURCES MainWindow.h MainWindow.cpp Workspace.h Workspace.cpp CleanScreen.h CleanScreen.cpp CommandRouter.h
    CommandRouter.cpp
  LIBS trion_dock trion_message trion_actions trion_edition trion_display trion_i18n trion_theme trion_base
    Qt6::Widgets)
qt_add_resources(trion_shell shell_workspaces PREFIX /trion/shell BASE resources
  FILES resources/workspaces/2d.json resources/workspaces/viewer.json)

# Màn hình giả cắm/rút lúc chạy qua cấu hình của plugin offscreen (QPlatformNativeInterface, Qt6::GuiPrivate) — chỉ
# test dùng, như app/widgets/dock.
set(QT_NO_PRIVATE_MODULE_WARNING ON)
find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
trion_add_test(shell_test SOURCES tests/shell_test.cpp
  LIBS trion_shell trion_keymap trion_tooltip trion_fonts trion_icons Qt6::GuiPrivate LABELS shell)
