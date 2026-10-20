# app/core: lõi giao diện dùng chung. UI0-101: executor luồng UI, kiểm sở hữu luồng.
trion_add_library(trion_appcore
  SOURCES UiExecutor.h UiExecutor.cpp
  LIBS trion_base Qt6::Core)

trion_add_test(appcore_ui_executor_test SOURCES tests/ui_executor_test.cpp LIBS trion_appcore LABELS appcore)
