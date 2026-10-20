# app/core/perf (UI0-105): đo độ trễ input → khung hình, thời gian khung, watchdog luồng UI.
trion_add_library(trion_perf
  SOURCES UiProbe.h UiProbe.cpp
  LIBS trion_appcore Qt6::Widgets)

trion_add_test(perf_ui_probe_test SOURCES tests/ui_probe_test.cpp LIBS trion_perf LABELS perf)
