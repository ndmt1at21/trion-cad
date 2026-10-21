#!/bin/sh
# Integration: cấu hình đủ mọi thành phần, build và chạy toàn bộ test ở dev, release (ngưỡng thời gian), asan-ubsan,
# tsan (Docker) và
# Windows (build chéo trong Docker, test C++ chạy thật trên Windows qua WSL). Chạy sau mỗi nhóm 3–5 task.
set -e
cd "$(dirname "$0")/.."
ci/docker/run.sh sh -ec 'for p in dev release asan-ubsan tsan win; do cmake --preset $p -DTRION_COMPONENTS= >/dev/null; cmake --build --preset $p; done
for p in dev release asan-ubsan tsan; do ctest --preset $p; done'
ctest --test-dir build/win -L '^cpp$' --output-on-failure
