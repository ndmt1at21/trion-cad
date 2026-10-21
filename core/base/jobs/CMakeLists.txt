# core/base/jobs (NT-013, tập con cho UI0-112): BackgroundJobs — đăng ký tác vụ nền: tiến trình phân cấp có trọng số,
# hủy (cả theo document), trạng thái, cờ con trỏ bận / hiện tiến trình; báo về luồng UI theo lô qua sched::postToUi.
trion_add_library(trion_jobs SOURCES BackgroundJobs.h BackgroundJobs.cpp LIBS trion_base Qt6::Core)

trion_add_test(jobs_test SOURCES tests/jobs_test.cpp LIBS trion_jobs LABELS jobs)
