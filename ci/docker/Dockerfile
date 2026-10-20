# Môi trường build Linux cho máy dev có glibc < 2.34 (Qt 6.11 bản cài sẵn cần glibc ≥ 2.34).
# Qt không nằm trong image: mount ~/Qt/<phiên bản>/gcc_64 vào /opt/qt (xem run.sh).
FROM ubuntu:24.04
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      clang-18 libclang-rt-18-dev llvm-18 lld-18 clang-tidy-18 clang-format-18 cmake ninja-build python3 git ca-certificates \
      libgl-dev libegl-dev libxkbcommon-dev libfontconfig1 libfreetype6 libglib2.0-0 libdbus-1-3 \
      libx11-xcb1 libxcb-cursor0 libxcb-icccm4 libxcb-keysyms1 libxcb-shape0 libxkbcommon-x11-0 \
      fonts-dejavu-core xvfb xauth nodejs \
 && rm -rf /var/lib/apt/lists/* \
 && update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 100 \
 && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-18 100 \
 && update-alternatives --install /usr/bin/llvm-symbolizer llvm-symbolizer /usr/bin/llvm-symbolizer-18 100
ENV LANG=C.UTF-8 CC=clang CXX=clang++ CMAKE_PREFIX_PATH=/opt/qt LD_LIBRARY_PATH=/opt/qt/lib QT_QPA_PLATFORM=offscreen \
    QT_HOST_DIR=/opt/qt QT_WIN_DIR=/opt/qt-win LLVM_MINGW_DIR=/opt/llvm-mingw
