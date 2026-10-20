# Mã giao diện (Qt 6 Widgets): quy ước

Task: [docs/08-phan-ra-task](../docs/08-phan-ra-task/README.md) (`UI0`, `UIA`… và DoD-U ở §3). Thiết kế gốc: `design/prototype/`.

## Build và test

Máy dev Ubuntu 20.04 không chạy được Qt 6.11 (cần glibc ≥ 2.34), nên build trong Docker (ubuntu:24.04, clang 18,
Qt mount từ `~/Qt/6.11.3/gcc_64`). Trên Ubuntu ≥ 22.04, Windows, macOS chạy thẳng các lệnh cmake/ctest.

```sh
ci/docker/run.sh cmake --preset dev                  # cấu hình; preset: dev, release, asan-ubsan, tsan
ci/docker/run.sh cmake --build --preset dev
ci/docker/run.sh ctest --preset dev                  # -L <nhãn> để chạy một thành phần
```

Làm song song: mỗi người cấu hình thư mục build riêng và chỉ các thành phần mình cần, để mã đang viết dở
của thành phần khác không làm hỏng build:

```sh
ci/docker/run.sh cmake --preset dev -B build/theme -DTRION_COMPONENTS="core/base;tools/tokengen;app/theme"
ci/docker/run.sh cmake --build build/theme && ci/docker/run.sh ctest --test-dir build/theme -L theme --output-on-failure
```

Bản Windows (build chéo bằng llvm-mingw, như snapone; test chạy thật trên Windows, gọi từ WSL):

```sh
ci/docker/run.sh cmake --preset win && ci/docker/run.sh cmake --build --preset win
ctest --test-dir build/win --output-on-failure        # chạy trên máy, không qua Docker
```

## Bố cục

- **Thành phần** = một thư mục có `CMakeLists.txt`, khai báo ở `CMakeLists.txt` gốc bằng `trion_component(<thư mục>)`.
  Mỗi thành phần là một thư viện tĩnh `trion_<tên>` (`trion_add_library`) cùng test của nó.
- Test: Qt Test, tệp `<thành phần>/tests/<tên>_test.cpp`, đăng ký bằng `trion_add_test(<tên>_test SOURCES … LIBS … LABELS <tên>)`.
  Test chạy với `QT_QPA_PLATFORM=offscreen`; mỗi bộ test < 60 s (DoD-C 2). Tool Python test bằng `unittest`.
  Dọn dẹp (Store, cửa sổ, ThemeManager) trong `cleanupTestCase()`, không để hàm hủy của lớp test: chạy sau `qExec`
  thì TSan báo race giả với luồng watchdog của Qt Test.
- Include theo đường dẫn từ gốc kho: `#include "core/base/settings/Settings.h"`.
- Mã sinh tự động nằm trong thư mục build, include bằng cùng đường dẫn như ở kho (`app/theme/generated/Tokens.h`).

## Lõi dùng chung (tập con, `core/base/`)

Các task lõi neo (NT-005, NT-009, NT-010, NT-012) chưa làm; `core/base` có tập con đủ cho UI, giữ đúng API dự kiến:

| Header | Dùng cho |
|---|---|
| `core/base/assert.h` | `TRION_ASSERT` (debug), `TRION_CHECK` (luôn bật), `setAssertHandler` cho test |
| `core/base/error.h` | `trion::Error{module, code, message}`, `trion::Expected<T>` |
| `core/base/sched/Scheduler.h` | `submit(Priority, fn)`, `CancelSource/CancelToken`, `postToUi(fn)` (UI0-101 cài poster), `waitIdle()` |
| `core/base/settings/Settings.h` | `Setting<T>{"nhóm/khóa", mặc định}`, `Store::instance().get/set/reset`, tín hiệu `changed(key)`; test dùng `Store(tệp tạm)` + `Store::setInstance` |
| `core/base/text/Fold.h` | `foldForSearch` ("lop" khớp "Lớp"), `containsFolded`, `startsWithFolded` |

Không sửa `core/base` khi đang làm task UI; cần thêm thì ghi vào báo cáo cho người điều phối.

## Quy tắc viết

- Theo DoD-U (README task §3): chỉ dùng token (sau UI0-002), mọi chuỗi qua `tr()`, không chặn luồng UI quá 2 ms,
  danh sách > 200 dòng phải ảo hóa, rảnh không timer định kỳ.
- Kết quả việc nền về luồng UI qua `trion::ui::post`/`postLatest` (`app/core/UiExecutor.h`, UI0-101), không qua
  `QMetaObject::invokeMethod`/`postEvent`: Qt bản cài sẵn không biên dịch với TSan nên TSan không thấy đồng bộ
  bên trong Qt và báo race giả.
- Màu, cỡ, khoảng cách, thời gian chuyển động đọc qua `trion::theme::ThemeManager::instance()` (`app/theme/ThemeManager.h`)
  ngay trong `paintEvent`/`sizeHint`, không cache; test tự tạo `ThemeManager` với `Store` tạm.
- **Ngôn ngữ nguồn của chuỗi là tiếng Việt** (giống prototype); bản English nằm trong `app/i18n/trion_en.ts`.
- Số đếm, dung lượng, ngày giờ, thời lượng của giao diện qua `UiFormat` (`app/core/i18n/UiFormat.h`), không qua locale
  máy; số của bản vẽ (tọa độ, kích thước, góc) chỉ qua RGN-013 (`core/regen/numfmt`), luôn dấu chấm thập phân.
- Màn hình mới chạy thử với ngôn ngữ giả vi-XL (chuỗi dài hơn 40%) và `findTruncations` (`app/core/i18n/TruncationProbe.h`):
  0 chữ bị cắt.
- Chú thích trong mã viết tiếng Việt, tên định danh tiếng Anh. Chỗ cố ý làm đơn giản ghi `ponytail:` kèm giới hạn và
  task sẽ nâng cấp.
- Không thêm thư viện ngoài 05 §2. Mã của chính chủ dự án trong `~/snapone` (C++/Qt, cùng tác giả) được phép tái dùng:
  `core/lru.h` (LRU theo dung lượng), `core/jobs.*` (`Latest`: yêu cầu mới hủy yêu cầu cũ), `ui/i18n/check.py`.
- Không tự commit: người điều phối kiểm build và test rồi commit sau mỗi task (một task một commit, chỉ gồm tệp của task).
