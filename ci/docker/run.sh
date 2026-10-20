#!/bin/sh
# Chạy lệnh trong image trion-dev với kho mã và Qt được mount. Ví dụ:
#   ci/docker/run.sh cmake --preset dev && ci/docker/run.sh cmake --build --preset dev
# QT_DIR mặc định ~/Qt/6.11.3/gcc_64.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
QT_DIR=${QT_DIR:-$HOME/Qt/6.11.3/gcc_64}
docker image inspect trion-dev >/dev/null 2>&1 || docker build -t trion-dev "$ROOT/ci/docker"
# seccomp=unconfined: TSan cần personality(ADDR_NO_RANDOMIZE) để tắt ASLR, profile seccomp mặc định chặn.
exec docker run --rm -i --security-opt seccomp=unconfined --user "$(id -u):$(id -g)" -e HOME=/tmp -v "$ROOT:$ROOT" -v "$QT_DIR:/opt/qt:ro" -w "$ROOT" trion-dev "$@"
