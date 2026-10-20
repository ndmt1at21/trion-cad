# Bước dịch (UI0-104). include() ở CUỐI CMakeLists.txt gốc: phạm vi thư mục gốc, sau mọi trion_component(),
# nên thấy mọi target đã cấu hình (TRION_COMPONENTS chọn vài thành phần thì chỉ các thành phần đó).
# Thành phần mới không cần làm gì: target của nó tự vào danh sách quét.
#
#  - `cmake --build <build> --target update_translations` (chạy tay): lupdate quét nguồn mọi target →
#    app/i18n/trion_en.ts (bản English) và trion_vi.ts (ngôn ngữ nguồn, chỉ dạng số nhiều); dịch bằng Qt
#    Linguist. Không -no-obsolete: build chỉ vài thành phần chỉ đánh dấu "vanished" chuỗi của thành phần
#    khác, bản dịch còn nguyên, chạy lại trên build đủ thành phần là dùng lại.
#  - Mỗi lần build: lrelease → .qm nhúng vào trion_i18n tại :/i18n/trion_<mã>.qm (Translator nạp).
#  - ctest -L i18n: i18n_lint (chuỗi viết thẳng, ghép bằng + trong app/), i18n_ts (.ts đủ bản dịch, khớp
#    nguồn), i18n_lint_glossary (nhãn đúng bảng thuật ngữ glossary.tsv, UI0-005), i18n_lint_selftest (test của
#    ci/lint/i18n).
#  - qtbase_vi.ts (chuỗi có sẵn của Qt bằng tiếng Việt) viết tay, không qua lupdate: lrelease ở
#    app/core/i18n/CMakeLists.txt.

qt_collect_translation_source_targets(trion_i18n_targets)

# Nguồn mà update_translations quét: cho lupdate của i18n_ts, phần trong app/ cho lint
set(lupdate_lst "-I${CMAKE_SOURCE_DIR}\n")
set(lint_lst "")
set(app_dir "${CMAKE_SOURCE_DIR}/app")
foreach(t IN LISTS trion_i18n_targets)
  get_target_property(dir ${t} SOURCE_DIR)
  get_target_property(srcs ${t} SOURCES)
  foreach(s IN LISTS srcs)
    cmake_path(ABSOLUTE_PATH s BASE_DIRECTORY "${dir}" NORMALIZE)
    cmake_path(IS_PREFIX CMAKE_BINARY_DIR "${s}" generated)
    if(generated OR NOT EXISTS "${s}" OR s MATCHES "\\.ts$")
      continue()
    endif()
    string(APPEND lupdate_lst "${s}\n")
    cmake_path(IS_PREFIX app_dir "${s}" in_app)
    if(in_app)
      string(APPEND lint_lst "${s}\n")
    endif()
  endforeach()
endforeach()
file(WRITE ${CMAKE_BINARY_DIR}/i18n/lupdate.lst "${lupdate_lst}")
file(WRITE ${CMAKE_BINARY_DIR}/i18n/lint.lst "${lint_lst}")

set(lint_dir ${CMAKE_SOURCE_DIR}/ci/lint/i18n)
add_test(NAME i18n_lint COMMAND ${Python3_EXECUTABLE} ${lint_dir}/lint.py @${CMAKE_BINARY_DIR}/i18n/lint.lst)
add_test(NAME i18n_lint_selftest COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${lint_dir}/tests)
file(GLOB all_ts ${app_dir}/i18n/*.ts)
add_test(NAME i18n_lint_glossary
  COMMAND ${Python3_EXECUTABLE} ${lint_dir}/glossary.py ${app_dir}/i18n/glossary.tsv ${all_ts})
set_tests_properties(i18n_lint i18n_lint_selftest i18n_lint_glossary PROPERTIES LABELS i18n
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1;LUPDATE=$<TARGET_FILE:Qt6::lupdate>")

if(TARGET trion_i18n)
  get_target_property(qm_dir trion_i18n BINARY_DIR) # resource lấy BASE là thư mục build của target
  qt_add_translations(trion_i18n
    SOURCE_TARGETS ${trion_i18n_targets}
    TS_FILE_DIR ${app_dir}/i18n
    TS_FILE_BASE trion
    TS_FILES_OUTPUT_VARIABLE ts_files
    QM_OUTPUT_DIRECTORY ${qm_dir}
    LUPDATE_OPTIONS -locations none
    LRELEASE_OPTIONS -silent)
  add_test(NAME i18n_ts COMMAND ${Python3_EXECUTABLE} ${lint_dir}/check_ts.py $<TARGET_FILE:Qt6::lupdate>
    @${CMAKE_BINARY_DIR}/i18n/lupdate.lst -ts ${ts_files})
  set_tests_properties(i18n_ts PROPERTIES LABELS i18n ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
endif()
