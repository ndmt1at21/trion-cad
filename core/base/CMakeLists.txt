# core/base: tập con của NT-005, NT-009, NT-010, NT-012 mà các task UI cần (xem chú thích trong từng header).
trion_add_library(trion_base
  SOURCES
    assert.h assert.cpp error.h
    sched/Scheduler.h sched/Scheduler.cpp
    settings/Settings.h settings/Settings.cpp
    text/Fold.h text/Fold.cpp
  LIBS Qt6::Core)

trion_add_test(base_error_test SOURCES tests/error_test.cpp LIBS trion_base LABELS base)
trion_add_test(base_scheduler_test SOURCES tests/scheduler_test.cpp LIBS trion_base LABELS base)
trion_add_test(base_settings_test SOURCES tests/settings_test.cpp LIBS trion_base LABELS base)
trion_add_test(base_fold_test SOURCES tests/fold_test.cpp LIBS trion_base LABELS base)
