# core/regen: định dạng số và đơn vị (RGN-013), bảng màu ACI (tập con RGN-002, cho UIC-017); các phần regen khác thêm
# vào thư viện này.
trion_add_library(trion_regen
  SOURCES
    numfmt/UnitContext.h numfmt/NumFormat.h numfmt/Digits.h
    numfmt/LinearFormat.cpp numfmt/AngularFormat.cpp
    props/AciPalette.h props/AciPalette.cpp)

trion_add_test(regen_numfmt_test SOURCES tests/numfmt_test.cpp LIBS trion_regen LABELS regen)
trion_add_test(regen_props_test SOURCES tests/props_aci_test.cpp LIBS trion_regen LABELS regen)
