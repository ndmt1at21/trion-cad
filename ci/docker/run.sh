#!/bin/sh
# Chạy lệnh trong image trion-dev với kho mã và Qt được mount. Ví dụ:
#   ci/docker/run.sh cmake --preset dev && ci/docker/run.sh cmake --build --preset dev
# Bản Windows: ci/docker/run.sh cmake --preset win && ci/docker/run.sh cmake --build --preset win,
# rồi chạy test ngay trên máy (WSL gọi được .exe): ctest --test-dir build/win
# QT_DIR mặc định ~/Qt/6.11.3/gcc_64.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
QT_DIR=${QT_DIR:-$HOME/Qt/6.11.3/gcc_64}
# Cho preset win (build chéo Windows): Qt llvm-mingw và bộ biên dịch llvm-mingw, mount nếu có.
QT_WIN_DIR=${QT_WIN_DIR:-$HOME/Qt/6.11.3/llvm-mingw_64}
LLVM_MINGW_DIR=${LLVM_MINGW_DIR:-$HOME/.local/opt/llvm-mingw-20250402-ucrt-ubuntu-20.04-x86_64}
EXTRA=""
[ -d "$QT_WIN_DIR" ] && EXTRA="$EXTRA -v $QT_WIN_DIR:/opt/qt-win:ro"
[ -d "$LLVM_MINGW_DIR" ] && EXTRA="$EXTRA -v $LLVM_MINGW_DIR:/opt/llvm-mingw:ro"
docker image inspect trion-dev >/dev/null 2>&1 || docker build -t trion-dev "$ROOT/ci/docker"
# seccomp=unconfined: TSan cần personality(ADDR_NO_RANDOMIZE) để tắt ASLR, profile seccomp mặc định chặn.
exec docker run --rm -i --security-opt seccomp=unconfined --user "$(id -u):$(id -g)" -e HOME=/tmp -v "$ROOT:$ROOT" -v "$QT_DIR:/opt/qt:ro" $EXTRA -w "$ROOT" trion-dev "$@"
