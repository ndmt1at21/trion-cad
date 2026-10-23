# app/vn/fontdoctor (UIJ-001…): Bác sĩ Font — banner khi mở bản vẽ (FontDoctorBanner), chỉ báo trên thanh trạng thái
# (FontStatusChip), trạng thái theo từng bản vẽ (FontDoctorUi), thiết lập giao diện (FontDoctorUiSettings).
trion_add_library(trion_fontdoctor
  SOURCES FontDoctorUiSettings.h FontDoctorUiSettings.cpp FontDoctorBanner.h FontDoctorBanner.cpp
          FontStatusChip.h FontStatusChip.cpp FontDoctorUi.h FontDoctorUi.cpp
  LIBS trion_edition trion_message trion_controls trion_tooltip trion_icons trion_fonts trion_i18n trion_theme
       trion_base Qt6::Widgets)

trion_add_test(font_doctor_banner_test SOURCES tests/font_doctor_banner_test.cpp
  LIBS trion_fontdoctor trion_a11y trion_appcore LABELS fontdoctor)
