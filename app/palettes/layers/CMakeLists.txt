# app/palettes/layers (UIC-008, UIC-009): model danh sách layer ảo hóa và palette Quản lý layer.
# UIC-008: LayerTableModel (snapshot layer → dòng lọc, sắp ở nền qua BackgroundModel; đếm đối tượng mỗi layer song song;
# lô sự kiện thêm/xóa/đổi layer áp gia tăng; bố cục cột lưu theo hồ sơ).
trion_add_library(trion_layers
  SOURCES LayerTableModel.h LayerTableModel.cpp
  LIBS trion_model trion_views trion_i18n trion_appcore trion_theme trion_base Qt6::Widgets)

trion_add_test(layer_model_test SOURCES tests/layer_model_test.cpp
  LIBS trion_layers trion_fonts trion_icons trion_i18n LABELS layers)

# Benchmark 10.000 layer, 10⁶ entity (bản release; debug, sanitizer: cỡ nhỏ, chỉ in số đo thời gian), theo mẫu
# app/widgets/bench: khung đầu, sắp, gõ tìm, cuộn (UiProbe), đếm song song, lô 1.000 sự kiện.
trion_add_test(layer_model_bench SOURCES bench/layer_model_bench.cpp
  LIBS trion_layers trion_perf trion_fonts trion_icons LABELS layers bench)
