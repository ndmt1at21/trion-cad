# app/widgets/model (UI0-014): model nền cho danh sách lớn (dựng từ snapshot ở nền, diff theo khóa, áp theo lô,
# giữ chọn và vị trí cuộn); lint cấm QListWidget/QTableWidget/QTreeWidget trong app/.
trion_add_library(trion_model
  SOURCES BackgroundModel.h BackgroundModel.cpp
  LIBS trion_appcore Qt6::Widgets)

trion_add_test(model_background_test SOURCES tests/background_model_test.cpp LIBS trion_model LABELS model)
# Tiêu chí 4 dưới TSan: 10.000 vòng ngẫu nhiên. Mỗi vòng đánh thức một worker, nên thời gian tăng theo tải máy nhiều hơn
# theo việc (2,7 s lúc load 7, 29 s lúc load 23): tách khỏi bộ test chính (2.000 vòng), TIMEOUT 300.
if(TRION_SANITIZE STREQUAL "thread")
  add_test(NAME model_background_random COMMAND model_background_test newestResultWinsRandom)
  set_tests_properties(model_background_random PROPERTIES LABELS "model;cpp;stress" TIMEOUT 300
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_ENABLE_HIGHDPI_SCALING=0;TRION_RANDOM_ROUNDS=10000")
endif()

# Lint DoD-U 4: dữ liệu bản vẽ hiện bằng model/view; item widget chỉ khi dòng có `item-widget-ok: <lý do>`.
set(no_item ${CMAKE_SOURCE_DIR}/ci/lint/no-item-widgets)
add_test(NAME model_no_item_widgets COMMAND ${Python3_EXECUTABLE} ${no_item}/lint.py ${CMAKE_SOURCE_DIR}/app)
add_test(NAME model_no_item_widgets_selftest COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${no_item}/tests)
set_tests_properties(model_no_item_widgets model_no_item_widgets_selftest PROPERTIES
  LABELS model ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
