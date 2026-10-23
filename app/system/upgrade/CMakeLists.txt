# app/system/upgrade (UIL-014): trang nâng cấp UI-L04 — UpgradeDialog (hộp thoại), UpgradeContent (upgrade-content.json:
# lợi ích vi/en, ảnh, giá theo phiên bản và tính năng; nhúng :/trion/upgrade/), UpgradePage (handler của
# EditionGate::upgrade(); lần sau trong phiên: toast "X có trong TrionCAD Y." kèm "Xem").
trion_add_library(trion_upgrade
  SOURCES UpgradeDialog.h UpgradeDialog.cpp
  LIBS trion_dialog trion_message trion_edition trion_controls trion_icons trion_fonts trion_i18n trion_theme
       Qt6::Widgets)
qt_add_resources(trion_upgrade upgrade_content PREFIX /trion/upgrade FILES upgrade-content.json)

# Danh mục lệnh (trion_actions) để kiểm phiên bản cần trong JSON khớp khóa thật.
trion_add_test(upgrade_test SOURCES tests/upgrade_test.cpp
  LIBS trion_upgrade trion_actions trion_a11y trion_perf trion_appcore trion_i18n LABELS upgrade)
