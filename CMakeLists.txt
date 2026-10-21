cmake_minimum_required(VERSION 3.25)
project(TrionCAD VERSION 0.1.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(TrionFlags)
include(TrionTargets)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
enable_testing()

find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Widgets Svg Network Test LinguistTools)
qt_standard_project_setup(I18N_SOURCE_LANGUAGE vi I18N_TRANSLATED_LANGUAGES en)
trion_deploy_qt_runtime()

# Mỗi thư mục dưới đây là một thành phần (một thư viện trion_<tên> + test của nó).
# Nhiều người/agent làm song song thì mỗi người cấu hình riêng phần mình cần, để mã đang viết dở
# của thành phần khác không làm hỏng build của mình:
#   cmake --preset dev -B build/<tên> -DTRION_COMPONENTS="core/base;app/theme"
set(TRION_COMPONENTS "" CACHE STRING "Chỉ cấu hình các thành phần này (rỗng = tất cả)")
macro(trion_component dir)
  if(NOT TRION_COMPONENTS OR "${dir}" IN_LIST TRION_COMPONENTS)
    add_subdirectory(${dir})
  endif()
endmacro()

trion_component(ci)
trion_component(core/base)
trion_component(core/regen)
trion_component(cmd)
trion_component(tools/tokengen)
trion_component(tools/iconconv)
trion_component(tools/iconlint)
trion_component(app/theme)
trion_component(app/theme/fonts)
trion_component(app/theme/icons)
trion_component(app/core)
trion_component(app/core/i18n)
trion_component(app/core/perf)
trion_component(app/core/display)
trion_component(app/core/keymap)
trion_component(app/widgets/controls)
trion_component(app/widgets/model)
trion_component(app/widgets/bench)
trion_component(app/widgets/numeric)
trion_component(tests/integration)

# Bước dịch (UI0-104): lupdate/lrelease cho mọi target đã cấu hình và test i18n. Phải đứng sau mọi
# trion_component(); xem app/i18n/i18n.cmake.
include(app/i18n/i18n.cmake)
