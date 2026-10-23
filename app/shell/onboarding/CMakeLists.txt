# app/shell/onboarding (UIA-016): lần chạy đầu UI-A15 — Onboarding (hộp các bước: ngôn ngữ, chủ đề, quyền riêng tư,
# phiên bản, hướng dẫn nhanh; khung bước mở rộng cho UIA-117), QuickTour (hướng dẫn nhanh 5 bước trên cửa sổ chính).
trion_add_library(trion_onboarding
  SOURCES Onboarding.h Onboarding.cpp QuickTour.h QuickTour.cpp
  LIBS trion_shell trion_dialog trion_dock trion_edition trion_controls trion_fonts trion_i18n trion_theme trion_base
    Qt6::Widgets)

trion_add_test(onboarding_test SOURCES tests/onboarding_test.cpp
  LIBS trion_onboarding trion_a11y trion_icons trion_appcore LABELS onboarding)
