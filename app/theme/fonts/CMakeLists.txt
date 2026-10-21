# app/theme/fonts (UI0-003): IBM Plex Sans 400/500/600, IBM Plex Mono 400/500 và OFL.txt nhúng resource /trion/fonts.
# Nguồn: bản phát hành chính thức github.com/IBM/plex, thư mục fonts/complete/ttf trong zip (giấy phép SIL OFL 1.1):
#   @ibm/plex-sans 1.1.0 (font "Version 3.005"), zip sha256 fb365d910566e6d199cc2c15579a7dd9a267128e18431a394ed81f1970c69200
#     https://github.com/IBM/plex/releases/download/%40ibm%2Fplex-sans%401.1.0/ibm-plex-sans.zip
#   @ibm/plex-mono 2.5.0 (font "Version 2.005"), zip sha256 6d23f01257663d8cc49a0d64c22ced630b79e0e2a0ac08a0da86e9a38bbc481c
#     https://github.com/IBM/plex/releases/download/%40ibm%2Fplex-mono%402.5.0/ibm-plex-mono.zip
# sha256 từng tệp:
#   975dcda37d80f038dcd143c22e33ca2d97a0cc5a929aace1c749153b0fe1afa5  IBMPlexSans-Regular.ttf
#   331c8639d7598b2cde62a911a71db195e30cb655cd6bdf2e324a7e984955f907  IBMPlexSans-Medium.ttf
#   a20caf8286023a6a7a85e40b1d2a4ae9fc3e3b1f9eda8f4c542dd4986af67bb1  IBMPlexSans-SemiBold.ttf
#   7c6fbddca4b700be918f5f6183d9bd4464fa427fe435f0b480d77fe2bb8c5a43  IBMPlexMono-Regular.ttf
#   98fbd727aae340b236955879dabed4d991aac9e8e90b3b2a67ce4a59221cc97c  IBMPlexMono-Medium.ttf
#   91c25c350d3cac39da2736d74f7ba37ef648f5237a4e330a240615bc8d8c4360  OFL.txt (= fonts/complete/ttf/license.txt, như nhau ở hai gói)
# Cập nhật font: thay tệp, sửa bảng trên, chạy theme_fonts_test (đủ tiếng Việt, ảnh baseline).
trion_add_library(trion_fonts SOURCES FontRegistry.h FontRegistry.cpp LIBS trion_theme)
qt_add_resources(trion_fonts plex_fonts PREFIX /trion/fonts FILES
  IBMPlexSans-Regular.ttf IBMPlexSans-Medium.ttf IBMPlexSans-SemiBold.ttf IBMPlexMono-Regular.ttf IBMPlexMono-Medium.ttf
  OFL.txt)

trion_add_test(theme_fonts_test SOURCES tests/fonts_test.cpp LIBS trion_fonts LABELS fonts)
# Ảnh baseline (tiêu chí 2) theo nền tảng, ghi lại bằng TRION_UPDATE_BASELINE=<thư mục>: xem tests/fonts_test.cpp.
foreach(p linux windows)
  foreach(dpr 100 150 200)
    list(APPEND baseline tests/baseline/stacked-${dpr}-${p}.png)
  endforeach()
endforeach()
qt_add_resources(theme_fonts_test fonts_baseline PREFIX /baseline BASE tests/baseline FILES ${baseline})
