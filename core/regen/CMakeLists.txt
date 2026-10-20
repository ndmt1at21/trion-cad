# core/regen: hiện có định dạng số và đơn vị (RGN-013); các phần regen khác thêm vào thư viện này.
trion_add_library(trion_regen
  SOURCES
    numfmt/UnitContext.h numfmt/NumFormat.h numfmt/Digits.h
    numfmt/LinearFormat.cpp numfmt/AngularFormat.cpp)

trion_add_test(regen_numfmt_test SOURCES tests/numfmt_test.cpp LIBS trion_regen LABELS regen)
