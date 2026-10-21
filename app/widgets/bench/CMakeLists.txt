# app/widgets/bench (UI0-014): benchmark danh sách 100.000 dòng trên QListView/QTreeView offscreen, đo qua UiProbe
# (UI0-105): khung đầu, áp diff theo lô, cuộn. Ngưỡng thời gian chỉ xét ở bản release; bản khác chỉ in số đo.
trion_add_test(bench_background_model SOURCES background_model_bench.cpp LIBS trion_model trion_perf LABELS bench)
