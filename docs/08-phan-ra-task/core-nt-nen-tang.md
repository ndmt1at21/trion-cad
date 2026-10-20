# Nền tảng (NT): build, CI, bộ lập lịch, SIMD, bộ nhớ, I/O, văn bản, nén

> Phạm vi: hạ tầng dùng chung cho mọi module lõi và UI. Gồm build, CI, cổng hiệu năng và tất định, tuân thủ pháp lý trong CI, ký số, phiên bản và kênh phát hành; bộ lập lịch, SIMD, bộ nhớ, hồ sơ phần cứng, log, cài đặt, I/O, văn bản, nén, ZIP, tác vụ nền.
> Thư mục mã: `cmake/`, `ci/`, `core/base/`. Workflow ở `.github/workflows/` chỉ gọi script trong `ci/`.
> Căn cứ: [05](../05-kien-truc-ky-thuat.md) §0, §1.1, §2, §3, §4, §9.2–9.5, §14–17; [06](../06-lo-trinh-trien-khai.md) §2.1. Neo NT-001…NT-015 theo [README](README.md) §5.1.

## 0. Ghi chú thiết kế

- **Tất định nằm trong API**, không chỉ trong kiểm thử: `parallel_reduce`/`parallel_scan` chia khối theo kích thước dữ liệu, không theo số luồng; kết quả giống từng bit ở 1–32 luồng (05 §0.5, DoD-C 4).
- `core/base` chỉ dùng thư viện chuẩn C++20, QtCore (chuẩn hóa Unicode, theo dõi tệp, JSON; 05 §2), zlib/libdeflate, libsodium. QtGui chỉ có trong thư viện con `core/base/hw/qt` (thông tin màn hình) **[đề xuất]**.
- Không tự tạo `std::thread`: mọi luồng (worker, làn I/O) do NT-005 sở hữu. Chờ bằng `std::atomic::wait`, không polling, không timer chạy không.
- Mức ISA nền: x86-64-v2 (SSE4.2) và ARMv8.0-A. AVX2, AVX-512, NEON chọn lúc chạy qua NT-108 **[đề xuất]**.
- Chuỗi nội bộ là UTF-8. Surrogate lẻ trong UTF-16 của DWG giữ được qua WTF-8 để round-trip 100% (05 §1.2) **[đề xuất]**.
- Lỗi trả bằng `trion::Expected<T>` mang `trion::Error` (NT-009). Assert chỉ cho bất biến nội bộ, không dùng cho đầu vào không tin cậy (NT-103).
- Ngưỡng hiệu năng đo trên máy tầm trung (README §3.1) bằng NT-003, trừ khi ghi khác. Ngưỡng mới trong tệp này là **[đề xuất]**, chốt lại khi có số đo trên 4 máy tham chiếu.
- Script CI viết bằng Python 3, chạy được trên 3 hệ điều hành; workflow chỉ là lớp gọi mỏng **[đề xuất]**.
- Công cụ dev: GoogleTest (chọn trong cặp GoogleTest/Catch2 của 05 §2) **[đề xuất]**, Google Benchmark, Tracy, libFuzzer. Thư viện sản phẩm chỉ theo 05 §2, ghim phiên bản qua vcpkg.

## 1. Build, CI và cổng chất lượng

### NT-001 · Monorepo, CMake presets, vcpkg ghim phiên bản, cờ biên dịch, preset sanitizer
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** —
- **Đầu vào:** 05 §17 (monorepo, CMake, vcpkg, sccache), §4 ADR-001 và ADR-004, §2 (thư viện được phép); README DoD-C 1, 3, 5.
- **Đầu ra:**
  - Cây thư mục theo 05 §17; `CMakePresets.json`: `dev`, `release` (LTO), `release-pgo-gen`, `release-pgo-use` (cho NT-112), `asan-ubsan`, `tsan`, `fuzz`; `vcpkg.json` với baseline ghim.
  - `cmake/TrionFlags.cmake` (C++20, cảnh báo là lỗi, `-ffp-contract=off`, `/fp:precise`, x86-64-v2/ARMv8.0-A); `cmake/TrionTargets.cmake` (`trion_add_library`, `trion_add_test`); `.clang-tidy`, `.clang-format`.
  - `ci/check_flags.py`: quét `compile_commands.json` tìm cờ cấm.
- **Tiêu chí nghiệm thu:**
  1. Clone sạch, `cmake --preset <p>` rồi build thành công với mọi preset trên Windows (MSVC 2022, Clang ≥ 17), macOS, Linux.
  2. `ci/check_flags.py` bắt 100% tệp mẫu có `-ffast-math`, `-Ofast`, `/fp:fast`, `-ffp-contract=fast`, `-funsafe-math-optimizations`; kho mã thật có 0 vi phạm.
  3. Ba test canary cố ý lỗi bị bắt: ASan (tràn heap), UBSan (tràn số có dấu), TSan (data race).
  4. Hai lần build sạch cho cùng danh sách phiên bản phụ thuộc (so băm); build lại khi không đổi mã đạt sccache hit ≥ 90% **[đề xuất]**.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** quy trình huấn luyện PGO (NT-112), workflow CI (NT-002).

### NT-002 · CI: cổng PR trên 3 hệ điều hành, job đêm, runner tự host có GPU thật
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** NT-001
- **Đầu vào:** 05 §14 (tần suất từng lớp kiểm thử), §17 (GitHub Actions cho phần nhẹ, runner tự host); 06 §2.1 (4 máy benchmark); README §3.1, DoD-C 1–3.
- **Đầu ra:**
  - `.github/workflows/pr.yml`, `nightly.yml` gọi `ci/*.py`; cache nhị phân vcpkg và sccache.
  - Cổng PR: build Windows/macOS/Linux (MSVC và Clang), test, clang-tidy, ASan+UBSan trên 3 hệ điều hành, TSan trên Linux và macOS.
  - `ci/runners/`: script dựng runner tự host trên 4 máy tham chiếu, nhãn `low-intelhd`, `mid`, `high-gpu`, `mac-arm`; `ci/README.md`.
- **Tiêu chí nghiệm thu:**
  1. Bốn PR mẫu (cảnh báo biên dịch, test hỏng, vi phạm clang-tidy, lỗi ASan) đều bị chặn merge.
  2. Cổng PR xong ≤ 30 phút khi cache nóng **[đề xuất]**; bộ kiểm thử module nào chạy quá 60 s thì job lỗi (DoD-C 2).
  3. PR từ fork không bao giờ chạy trên runner tự host (test cấu hình).
  4. Job đêm trên runner GPU xác nhận adapter thật (không phải WARP, llvmpipe hay bộ dựng mềm) và xuất `summary.json` cho bảng chỉ số QA-010.
  5. Đạt DoD-C (mục 2, 10, 11).

### NT-003 · Khung benchmark và cổng hiệu năng
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** NT-002
- **Đầu vào:** 05 §0.2, §1.1, §14 (PR rút gọn, đêm đầy đủ); README DoD-C 9, DoD-H 2, DoD-L (p50 và p99), §3.1; Google Benchmark.
- **Đầu ra:**
  - `core/base/bench/`: `trion_add_benchmark()`, `TRION_BENCH_BUDGET` (ngưỡng theo máy), `TRION_LATENCY_BENCH` (lưu từng mẫu, tính p50/p99), tham số `--trion_threads=N` (NT-105 đọc).
  - `ci/perf/`: chạy lặp, so baseline theo máy, chạy scaling, bình luận bảng kết quả vào PR, lưu lịch sử JSON.
- **Tiêu chí nghiệm thu:**
  1. Chặn merge khi trung vị vượt ngưỡng tuyệt đối, hoặc chậm hơn baseline cùng máy > 5% với p < 0,01 (Mann–Whitney, ≥ 10 lần lặp) **[đề xuất]**. PR mẫu chậm 8% bị chặn; chạy lại cùng commit không bị chặn.
  2. Chạy lại cùng commit trên cùng máy: hệ số biến thiên ≤ 2% với benchmark ≥ 1 ms **[đề xuất]**; lần chạy nhiễu hơn được đánh dấu và chạy lại.
  3. Scaling: đo ở 1, 2, 4, 8, 16, 32 luồng, tính hiệu suất song song, chặn khi thấp hơn mức benchmark khai báo (mặc định theo DoD-H 2).
  4. Bộ PR ≤ 10 phút mỗi máy; bộ đêm chạy đủ trên 4 máy tham chiếu (nhãn runner của NT-002).
  5. Đạt DoD-C (mục 2, 9, 10).

### NT-004 · Tracy, macro đánh dấu và bộ đếm cấp phát
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** NT-003
- **Đầu vào:** 05 §9.1 (Tracy); README DoD-C 6, 9; API Tracy bản ghim ở NT-001.
- **Đầu ra:**
  - `core/base/profile.h`: `TRION_ZONE`, `TRION_FRAME_MARK`, `TRION_PLOT`, `TRION_LOCKABLE`; biến mất hoàn toàn khi tắt `TRION_PROFILE`.
  - `core/base/alloc_counter/`: thay `operator new/delete` trong binary test và benchmark, `AllocScope`, `TRION_EXPECT_NO_ALLOC`, counter `allocs/iter` cho NT-003, cờ `TRION_BENCH_NO_ALLOC`.
- **Tiêu chí nghiệm thu:**
  1. Bản `release` không chứa ký hiệu Tracy (kiểm bằng `nm`/`dumpbin`); vòng lặp có macro chậm hơn không có ≤ 0,5% **[đề xuất]**.
  2. Bản bật profile: một zone ≤ 20 ns trung vị **[đề xuất]**; Tracy hiện đúng tên luồng worker và mốc khung hình.
  3. Bộ đếm đúng 100% với `new`, `new[]`, aligned new và cấp phát đồng thời từ 32 luồng (TSan sạch).
  4. Benchmark gắn `TRION_BENCH_NO_ALLOC` mà `allocs/iter` > 0 thì cổng NT-003 chặn merge (PR mẫu).
  5. Đạt DoD-C.

### NT-101 · SBOM và cổng giấy phép phụ thuộc
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** NT-002
- **Đầu vào:** 05 §2 (thư viện được phép, cấm GPL/AGPL), §15 (SBOM mỗi bản phát hành, Qt LGPL link động); tệp SPDX vcpkg sinh cho từng port; đặc tả SPDX 2.3.
- **Đầu ra:** `ci/legal/allowlist.yaml` (gói theo 05 §2 và giấy phép SPDX được phép); `ci/legal/sbom.py` (gộp SPDX của vcpkg với thành phần ngoài vcpkg: font IBM Plex, mã MIT ghi ở NT-102); `ci/legal/notices.py` sinh `THIRD_PARTY_NOTICES.txt` cho trình cài đặt (PLF-009).
- **Tiêu chí nghiệm thu:**
  1. PR thêm gói ngoài 05 §2, hoặc gói có giấy phép GPL, AGPL, SSPL hay không xác định, bị chặn (3 PR mẫu).
  2. SBOM của mỗi build qua trình kiểm SPDX 2.3 không lỗi và liệt kê 100% DLL/dylib/so trong thư mục cài đặt (`cmake --install`, đối chiếu bằng quét tệp).
  3. Binary nạp QtCore động (kiểm bảng import), không có ký hiệu Qt liên kết tĩnh.
  4. `THIRD_PARTY_NOTICES.txt` có đủ văn bản giấy phép của mọi thành phần trong SBOM.
  5. Đạt DoD-C (mục 2, 10, 11).

### NT-102 · Nhật ký nguồn tham khảo clean-room
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** NT-002
- **Đầu vào:** 05 §15 (lưu nhật ký nguồn tham khảo; LibreDWG chỉ chạy CLI; ACadSharp MIT giữ thông báo bản quyền); 06 §2.1 tuần 1–2 (chính sách clean-room); README DoD-C 11.
- **Đầu ra:** `docs/legal/reference-log/` (mỗi mục một YAML: mã `RL-NNNN`, ngày, người, loại nguồn, tên tài liệu và mục, điều rút ra, tệp hoặc trường schema liên quan); `ci/legal/reflog.py` (thêm, kiểm, báo cáo); bước kiểm trong cổng PR.
- **Tiêu chí nghiệm thu:**
  1. PR chạm `core/dwg/`, `core/dxf/`, `core/schema/` hoặc parser định dạng đóng (SHX, PAT, LIN, CTB, PC3, CUIX; danh sách glob cấu hình được) mà không có trailer `Ref-Source: RL-NNNN` trỏ tới mục có thật thì bị chặn (PR mẫu).
  2. Mục dẫn nguồn cấm (LibreDWG, ObjectARX/RealDWG, dịch ngược AutoCAD) bị từ chối khi kiểm schema.
  3. Tệp có mã dẫn xuất từ ACadSharp phải có đầu tệp MIT và mục loại `mit-code`; thiếu một trong hai thì lỗi; danh sách chuyển sang NT-101.
  4. `reflog.py report --since <tag>` xuất báo cáo Markdown cho luật sư, gồm tỉ lệ tệp codec có mục dẫn (mục tiêu 100% **[đề xuất]**).
  5. Đạt DoD-C (mục 2, 10, 11).
- **Ngoài phạm vi:** quét độ giống mã với LibreDWG trong job cô lập: chỉ làm khi luật sư xác nhận **[đề xuất]**.

## 2. Chẩn đoán và đầu vào không tin cậy

### NT-009 · Log, assert, chẩn đoán và kiểu lỗi
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-004
- **Đầu vào:** README DoD-C 2, 6, 10; nhu cầu của PLF-007 (log kèm crash dump) và PLF-010 (gửi bản vẽ lỗi kèm log).
- **Đầu ra:**
  - `core/base/log/`: `TRION_LOG(kênh, mức, fmt, …)` định dạng bằng `std::format_to_n`; kênh theo module; mức đặt qua `TRION_LOG=dwg=debug,…`; ring buffer ánh xạ tệp (8 MiB **[đề xuất]**) nên còn nguyên khi tiến trình chết; `crashRingRegion()` cho PLF-007; công cụ `logdump`.
  - `core/base/assert.h` (`TRION_ASSERT` ở bản debug và sanitizer, `TRION_CHECK` luôn bật); `core/base/error.h` (`trion::Error` có mã, module, thông điệp; `trion::Expected<T>`).
- **Tiêu chí nghiệm thu:**
  1. Lệnh log bị tắt theo mức ≤ 1 ns; lệnh log bật ≤ 100 ns p99 với thông điệp ≤ 128 byte; 0 cấp phát (NT-004) **[đề xuất]**.
  2. Đường log không gọi hệ thống sau khởi tạo (đếm bằng `strace -c` trên Linux qua 10⁶ lệnh log).
  3. 32 luồng ghi đồng thời 10⁶ dòng: TSan sạch, thứ tự trong từng luồng giữ nguyên, không mất dòng trước khi ring quay vòng.
  4. Tiến trình con bị kill (SIGKILL, TerminateProcess) ngay sau khi log: `logdump` đọc lại 100% bản ghi đã commit; bản ghi dở dang bị bỏ qua, không lỗi.
  5. Đạt DoD-C.

### NT-103 · Giới hạn cấp phát và tiện ích cho đầu vào không tin cậy
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** NT-009
- **Đầu vào:** 05 §16 (kiểm biên mọi chỗ đọc, giới hạn kích thước cấp phát); README DoD-C 7; nhu cầu của DWG-003, DWG-020, LSP-001, RGN-004, NT-014, NT-015.
- **Đầu ra:** `core/base/untrusted/`: `checkedAdd`, `checkedMul`, `checkedCast` (tràn số → lỗi); `ParseBudget` (tổng byte, cỡ một cấp phát, số phần tử, độ sâu; mặc định 12 × kích thước tệp + 64 MiB, một cấp phát ≤ 256 MiB **[đề xuất]**); `budget.reserve(vec, n)`; `DepthGuard`; macro `TRION_FUZZ_TARGET` đặt giới hạn mặc định cho libFuzzer; `docs/specs/base/untrusted-input.md`.
- **Tiêu chí nghiệm thu:**
  1. Dữ liệu giả khai báo 2³² phần tử: 0 byte được cấp phát (NT-004), trả `Error::LimitExceeded` có ngữ cảnh (vị trí, giới hạn).
  2. Ca biên `SIZE_MAX`, 2⁶³, số âm từ kiểu có dấu: phát hiện tràn 100%; 10⁶ ca ngẫu nhiên khớp phép tính 128 bit.
  3. Dữ liệu lồng 10⁶ mức dừng ở độ sâu giới hạn (mặc định 256 **[đề xuất]**) bằng lỗi, không tràn stack.
  4. `checkedMul` ≤ 1 ns; `ParseBudget::charge` ≤ 5 ns; 16 luồng cùng trừ một ngân sách đạt ≥ 90% thông lượng khi không có ngân sách (gom theo luồng) **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

## 3. Phần cứng, bộ lập lịch và SIMD

### NT-104 · Dò CPU: ISA SIMD, topo nhân P/E, cache
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** NT-009
- **Đầu vào:** 05 §9.2 (nhân P/E trên Intel thế hệ 12+), §9.3 (SSE4.2/AVX2/AVX-512/NEON), §9.5; tài liệu CPUID của Intel và AMD; API `GetLogicalProcessorInformationEx` (EfficiencyClass), sysfs `cpu_core`/`cpu_atom`, sysctl `hw.perflevel*`.
- **Đầu ra:** `core/base/hw/cpu.h` (`CpuInfo`: ISA có kiểm XCR0 cho AVX/AVX-512, PCLMUL, CRC32 và PMULL trên ARM; số nhân vật lý và logic; loại nhân P/E của từng luồng logic; L1d/L2/L3); biến `TRION_ISA=scalar|sse42|avx2|avx512|neon` ép đường chạy khi kiểm thử; bộ dữ liệu ghi lại cpuid/sysfs/sysctl của ≥ 10 CPU.
- **Tiêu chí nghiệm thu:**
  1. Khớp 100% tệp kỳ vọng của 4 máy tham chiếu và ≥ 10 bản ghi CPU (Skylake, Alder Lake, Raptor Lake, Zen 3, Zen 4, Apple M1, M2, Snapdragon X…), kiểm offline từ dữ liệu ghi lại.
  2. CPU có AVX-512 nhưng hệ điều hành chưa bật trạng thái (XCR0): báo không có AVX-512 (ca giả lập).
  3. Dò xong ≤ 2 ms lúc khởi động, không cấp phát sau khởi tạo **[đề xuất]**.
  4. Đạt DoD-C.

### NT-105 · Lõi bộ lập lịch: worker, deque work-stealing, task group
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** NT-104, NT-004
- **Đầu vào:** 05 §9.2 (bộ lập lịch work-stealing tự viết); README DoD-H 1, 6; bài báo "Dynamic Circular Work-Stealing Deque" và "Correct and Efficient Work-Stealing for Weak Memory Models".
- **Đầu ra:** `core/base/sched/`: pool worker (số luồng theo NT-104, ghi đè bằng `TRION_THREADS` hoặc `--trion_threads`); deque Chase–Lev mỗi worker; hàng đợi tiêm toàn cục; `TaskGroup::run/wait` (luồng chờ tự làm việc, lồng không deadlock); `currentWorkerIndex()`; chế độ hỗn loạn `TRION_SCHED_SHUFFLE` (trộm và nhường ngẫu nhiên) cho NT-107; benchmark `bench_sched`.
- **Tiêu chí nghiệm thu:**
  1. Tạo, chạy và chờ một task rỗng ≤ 80 ns trung bình ở 1 luồng, ≤ 150 ns ở 16 luồng có trộm việc; 0 cấp phát heap mỗi task (NT-004) **[đề xuất]**.
  2. Microtask (fib 32, tách đến ngưỡng): hiệu suất song song ≥ 70% đến 16 luồng; task thô ≥ 100 µs: ≥ 90% **[đề xuất]**.
  3. Worker đang ngủ nhận việc mới ≤ 50 µs p99; rảnh 10 s: tổng CPU của mọi worker ≤ 5 ms (chờ bằng `std::atomic::wait`) **[đề xuất]**.
  4. Stress 10⁸ task, task group lồng sâu 64, bật chế độ hỗn loạn: TSan sạch, không mất task, không deadlock trong 1 giờ chạy đêm.
  5. Đạt DoD-C, DoD-H.

### NT-107 · Job kiểm tất định đa nền tảng
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-002, NT-105
- **Đầu vào:** 05 §0.5, §4 ADR-004; README DoD-C 4, 5; chế độ hỗn loạn và ghi đè số luồng (NT-105), `TRION_ISA` (NT-104); BLAKE2b của libsodium (05 §2).
- **Đầu ra:** `core/base/determinism/`: `TRION_DETERMINISM_CASE(tên)` trả bytes chuẩn hóa, băm BLAKE2b-256; `ci/determinism/`: chạy mọi ca ở {1, 2, 3, 8, 16, 32} luồng, mọi ISA có trên máy, ASLR và `TRION_HASH_SEED` ngẫu nhiên; job gộp so băm giữa Windows, macOS, Linux, x86-64 và ARM64; ca canary FMA, FTZ/DAZ.
- **Tiêu chí nghiệm thu:**
  1. Băm lệch giữa hệ điều hành, kiến trúc, số luồng, ISA hoặc hạt giống băm làm job lỗi, nêu ca lệch đầu tiên và lưu hai đầu ra làm artifact.
  2. Ba lỗi cài sẵn bị bắt 100%: gộp theo thứ tự hoàn thành của luồng, duyệt `unordered_map` khóa con trỏ, một tệp bật `-ffp-contract=fast`.
  3. Job đêm ≤ 15 phút mỗi hệ điều hành; tập con cho PR ≤ 5 phút **[đề xuất]**.
  4. Module thêm ca mới chỉ bằng một macro trong tệp test (hướng dẫn ở `ci/determinism/README.md`).
  5. Đạt DoD-C (mục 2, 4, 10).

### NT-106 · parallel_for, parallel_reduce, parallel_scan tất định
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-105, NT-107
- **Đầu vào:** README DoD-C 4, DoD-H 1–2; 05 §9.2 (danh sách việc song song hóa); API `TaskGroup` của NT-105.
- **Đầu ra:** `core/base/sched/parallel.h`: `parallel_for(range, grain, body)`, `parallel_reduce`, `parallel_scan` (hai lượt), `parallel_invoke`; khối chia theo kích thước dữ liệu và grain, không theo số luồng; chạy tuần tự tại chỗ khi n < 10.000 hoặc ước lượng < 50 µs **[đề xuất]**; ca tất định đăng ký vào NT-107.
- **Tiêu chí nghiệm thu:**
  1. Tổng 10⁷ số `double` ngẫu nhiên và prefix-sum 10⁷ phần tử giống từng bit ở 1, 2, 3, 7, 16, 32 luồng và bằng hàm tham chiếu tuần tự cùng cây gộp.
  2. Chi phí cố định một `parallel_for` thân rỗng ≤ 5 µs ở 8 luồng; dưới ngưỡng tuần tự không đánh thức worker nào **[đề xuất]**.
  3. Thân nặng tính toán: `parallel_for` và `parallel_reduce` ≥ 70% tuyến tính đến 16 luồng; `parallel_scan` 10⁸ phần tử ≥ 60% đến 8 luồng **[đề xuất]**.
  4. Thân trả lỗi: các khối sau khối lỗi bị bỏ; lỗi trả về là lỗi của khối lỗi có chỉ số nhỏ nhất (tất định, không phụ thuộc thời điểm).
  5. Đạt DoD-C, DoD-H.

### NT-005 · Bộ lập lịch: ưu tiên, hủy hợp tác, QoS nhân P/E, đồ thị tác vụ, trả kết quả về luồng UI
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** NT-104, NT-106
- **Đầu vào:** 05 §9.2 (khung hình, jig, OSNAP trên nhân P; việc nền trên nhân E), §9.5 (pin: giảm luồng nền); README DoD-H 1, 5, 7, DoD-L; API QoS: `SetThreadInformation` (EcoQoS), `pthread_set_qos_class_self_np`, nice và affinity trên Linux.
- **Đầu ra:** `core/base/sched/`:
  - Lớp ưu tiên `Interactive`, `Normal`, `Background`; làn `Io` (luồng riêng được phép chặn, mặc định 4 **[đề xuất]**). Worker gắn QoS theo loại nhân; việc nền ưu tiên nhân E, không bao giờ chiếm worker nhân P cuối cùng **[đề xuất]**.
  - `CancelSource`/`CancelToken` (parallel_* ngừng nhận khối mới khi hủy); `TaskGraph` (DAG; nút lỗi hủy các nút phụ thuộc).
  - `postToUi(receiver, fn)`, `drainUi(budget)`, móc đánh thức vòng sự kiện cho UI0-001; `setBackgroundConcurrency(n)` cho RND-018.
- **Tiêu chí nghiệm thu:**
  1. Mọi worker bận việc nền dài: task `Interactive` mới bắt đầu ≤ 100 µs p99 **[đề xuất]**.
  2. Hủy việc nền đang chạy `parallel_for` 10⁸ phần tử: mọi task của nó dừng ≤ 50 ms (DoD-H 5).
  3. Máy lai Intel thế hệ 12+: ≥ 80% thời gian CPU của việc nền nằm trên nhân E khi có việc tương tác **[đề xuất]**; máy không lai: QoS làm giảm thông lượng ≤ 2%.
  4. `drainUi` chạy theo lô, mỗi lượt ≤ 2 ms rồi trả vòng sự kiện; callback không chạy sau khi receiver đã hủy (10⁶ lần thử).
  5. Đồ thị 10⁵ nút, 4×10⁵ cạnh: ≤ 100 ns mỗi cạnh; nút ≥ 50 µs: hiệu suất ≥ 70% đến 16 luồng **[đề xuất]**.
  6. Đạt DoD-C, DoD-H, DoD-L.

### NT-108 · Lớp SIMD tự viết và chọn đường chạy lúc khởi động
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** NT-104, NT-003
- **Đầu vào:** 05 §9.3; README DoD-H 3, DoD-C 5; Intel Intrinsics Guide, Arm NEON Intrinsics Reference.
- **Đầu ra:** `core/base/simd/`: kiểu `f64xN`, `f32xN`, `i32xN`, `u8xN` và mask; nạp/ghi, số học không FMA, min/max theo ngữ nghĩa `a < b ? a : b`, so sánh, chọn, rút gọn ngang theo thứ tự cố định, chuyển kiểu, tách/gộp xy; `cmake/TrionSimd.cmake` (`trion_simd_kernel()` biên dịch một nguồn cho từng ISA trong namespace riêng); bảng con trỏ hàm chọn một lần theo NT-104, ép được bằng `TRION_ISA`.
- **Tiêu chí nghiệm thu:**
  1. Mọi phép giống từng bit với đường scalar trên 10⁷ giá trị ngẫu nhiên và đặc biệt (±0, NaN, ±Inf, denormal, max) ở mọi ISA; AVX-512 chạy trên máy có hỗ trợ hoặc qua Intel SDE **[đề xuất]**.
  2. Toàn bộ test chạy dưới Intel SDE ở chế độ kiểm tập lệnh của CPU chỉ có SSE4.2: không có lệnh không hợp lệ (bắt mã AVX lọt vào đường chung).
  3. Kernel mẫu viết bằng lớp SIMD chậm hơn bản intrinsics viết tay ≤ 5% **[đề xuất]**.
  4. Đạt DoD-C, DoD-H.

### NT-006 · Kernel SIMD: biến đổi điểm, hộp bao, min/max
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-108, NT-106
- **Đầu vào:** 05 §9.3 (biến đổi ma trận hàng loạt điểm, hộp bao, culling), §9.1 (RTC); README DoD-H 2–4; bố cục SoA của GEO-001.
- **Đầu ra:** `core/base/simd/kernels/`: `transformPoints2D/3D` (affine, SoA `double`); `transformToFloatRTC` (trừ gốc bằng `double` rồi đổi `float`, cho RND-011); `bounds2D/3D`; `boundsRanges` (hộp bao theo dải chỉ số, cho BVH và culling); `minMax` cho `double`, `float`, `int32`; bản song song qua NT-106; benchmark và ca tất định NT-107.
- **Tiêu chí nghiệm thu:**
  1. Kết quả giống từng bit giữa scalar, SSE4.2, AVX2, AVX-512, NEON và ở 1–32 luồng (NT-107).
  2. Một nhân, dữ liệu trong L2: `transformPoints2D` ≥ 1 tỉ điểm/s, `bounds2D` ≥ 2 tỉ điểm/s; 10⁷ điểm từ RAM: ≥ 400 triệu và ≥ 800 triệu điểm/s **[đề xuất]**.
  3. Song song 10⁸ điểm: hiệu suất ≥ 60% đến 8 luồng hoặc ≥ 80% băng thông bộ nhớ đo bằng phép sao chép tham chiếu trên cùng máy **[đề xuất]**.
  4. NaN trong đầu vào `bounds*` bị bỏ qua như nhau ở mọi ISA (quy tắc ghi trong header); 0 cấp phát (NT-004).
  5. Đạt DoD-C, DoD-H.

### NT-008 · Hồ sơ phần cứng
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-104, NT-005
- **Đầu vào:** 05 §9.5 (đo cấu hình lúc khởi động, chạy pin), §1.1 (tự chọn backend và mức chất lượng); README DoD-H 6–7; API `GlobalMemoryStatusEx`, `PowerSettingRegisterNotification`, `IOPSNotificationCreateRunLoopSource`, netlink uevent (power_supply); QScreen.
- **Đầu ra:** `core/base/hw/profile.h` (`HardwareProfile`: `CpuInfo`, RAM tổng và khả dụng, nguồn AC/pin, mức pin, chế độ tiết kiệm pin, màn hình với DPI và tần số quét, chỗ cho GPU/VRAM do RND-001 điền); sự kiện thay đổi nguồn điện và màn hình qua NT-005; `core/base/hw/qt/` (màn hình qua QScreen, chỉ app liên kết); xuất JSON ẩn danh cho RND-024, PLF-007, PLF-008.
- **Tiêu chí nghiệm thu:**
  1. Thu hồ sơ ≤ 5 ms lúc khởi động; khớp tệp kỳ vọng của 4 máy tham chiếu **[đề xuất]**.
  2. Đổi nguồn AC/pin (thông báo hệ điều hành giả lập; Linux qua sysfs giả) → sự kiện tới bên đăng ký ≤ 1 s.
  3. Thêm/bớt màn hình, đổi DPI hoặc tần số (QScreen giả lập) → danh sách và sự kiện đúng.
  4. Rảnh 60 s: module không thức dậy lần nào (bộ đếm nội bộ); JSON chỉ chứa trường trong danh sách cho phép (không tên máy, tên người dùng, số serial).
  5. Đạt DoD-C, DoD-H.

## 4. Văn bản và bộ nhớ

### NT-012 · Văn bản: UTF-8/UTF-16, code page Windows, NFC, so sánh không phân biệt hoa thường có dấu
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** NT-009
- **Đầu vào:** 05 §2 (`QString::normalized`); bảng ánh xạ code page của Microsoft công bố trên unicode.org, tệp CaseFolding.txt của Unicode; bảng DWGCODEPAGE trong đặc tả DWG của ODA v5.4.1; nhu cầu của DWG-028, NT-007, NT-015.
- **Đầu ra:** `core/base/text/`:
  - UTF-8 ⇄ UTF-16 có kiểm hợp lệ; surrogate lẻ giữ qua WTF-8; đường nhanh ASCII (SWAR).
  - Mọi code page trong bảng DWGCODEPAGE (tối thiểu 874, 1250–1258, 932, 936, 949, 950) và OEM 437, 850: giải mã, mã hóa, cờ `lossless`; bảng sinh bằng `core/base/text/gen/`.
  - `toNfc()`, `foldCase()` (case folding đơn giản, không theo locale), `equalsNoCase()`, `hashNoCase()`; `toUpper`/`toLower` có dấu qua QtCore.
- **Tiêu chí nghiệm thu:**
  1. Round-trip UTF-16 → UTF-8 → UTF-16 giống từng đơn vị trên 10⁷ chuỗi ngẫu nhiên có surrogate lẻ; giải mã rồi mã hóa code page giống từng byte khi `lossless`.
  2. "Đường", "ĐƯỜNG", "đường" (cả NFC và NFD) bằng nhau qua `equalsNoCase`; cp1258 giải mã + `toNfc` khớp 100% bộ ca vàng ≥ 2.000 từ tiếng Việt **[đề xuất]**.
  3. Một nhân: UTF-8→UTF-16 ASCII ≥ 2 GB/s, tiếng Việt ≥ 400 MB/s; cp1252 ≥ 1 GB/s; 932/936/949/950 ≥ 200 MB/s; `equalsNoCase` tên ≤ 32 byte ≤ 10 ns (ASCII), ≤ 50 ns (có dấu) **[đề xuất]**.
  4. Hàm thuần, không trạng thái chung: chạy đồng thời ≥ 60% tuyến tính đến 8 luồng; fuzz mọi bộ giải mã ≥ 1 giờ.
  5. Đạt DoD-C, DoD-H.

### NT-007 · Bộ nhớ: arena, pool, small-vector, intern chuỗi
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** NT-012, NT-105, NT-107
- **Đầu vào:** 05 §9.4 (arena theo document và theo lần regen, intern tên layer và style); README DoD-C 6, DoD-H 4; quy tắc tên ký hiệu của AutoCAD (không phân biệt hoa thường, ≤ 255 ký tự).
- **Đầu ra:**
  - `core/base/mem/`: `Arena` (bump, danh sách chunk, thống kê byte cho DWG-031), `WorkerArenas` (một arena mỗi worker theo `currentWorkerIndex()`, giải phóng cùng lúc khi xong lần regen), `ArenaAllocator<T>`, `Pool<T>` (mỗi worker một pool), `small_vector<T, N>`.
  - `core/base/strings/`: `StringId` 32 bit, bảng intern toàn cục chia shard, `foldedId()` cho so sánh không phân biệt hoa thường theo NT-012. Chỉ intern tên (layer, style, linetype, block, tag, appid), không intern nội dung text.
- **Tiêu chí nghiệm thu:**
  1. Cấp phát arena ≤ 64 B: ≤ 2 ns; `Pool` cấp và trả ≤ 5 ns; reset arena 1 GB ≤ 1 ms (không xóa trắng) **[đề xuất]**.
  2. `small_vector` trong sức chứa nội tuyến và vòng lặp cấp phát arena: 0 cấp phát heap (NT-004).
  3. Intern chuỗi đã có (≤ 32 B) ≤ 50 ns; so hai `StringId`, chính xác hoặc không phân biệt hoa thường, ≤ 1 ns **[đề xuất]**.
  4. `WorkerArenas` ≥ 80% tuyến tính đến 16 luồng; intern đồng thời ≥ 60% đến 8 luồng **[đề xuất]**; TSan sạch.
  5. Giá trị `StringId` phụ thuộc thứ tự intern nhưng đầu ra không phụ thuộc giá trị đó (ca NT-107 với thứ tự intern xáo trộn).
  6. Đạt DoD-C, DoD-H.

## 5. Tệp, nén và ZIP

### NT-109 · Đường dẫn native, ánh xạ tệp, đọc song song, ghi an toàn
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-005, NT-012
- **Đầu vào:** 05 §9.4 (mmap tệp DWG, đọc song song xref và ảnh, prefetch); README DoD-H 1, 6; làn `Io` của NT-005.
- **Đầu ra:** `core/base/io/`: `Path` (UTF-8, đổi sang dạng native, tiền tố `\\?\` cho đường dài trên Windows); `FileView` (mmap trên ổ cục bộ; ổ mạng, USB hoặc khi mmap lỗi thì đọc vào bộ đệm theo khối 4 MiB song song **[đề xuất]**; `prefetch`); `readMany(paths, cb)` trên làn `Io` (tối đa 8 tệp cùng lúc, ổ mạng 4 **[đề xuất]**); `AtomicFileWriter` (tệp tạm cùng thư mục, flush, `fsync`/`F_FULLFSYNC`/`FlushFileBuffers`, `ReplaceFileW`/`rename`, giữ quyền tệp).
- **Tiêu chí nghiệm thu:**
  1. Map tệp 100 MB ≤ 1 ms; đọc tuần tự qua view ≥ 90% băng thông ổ đo bằng công cụ chuẩn trên cùng runner **[đề xuất]**.
  2. `readMany` 200 tệp × 2 MB trên NVMe nhanh hơn đọc lần lượt ≥ 2,5×; worker tính toán không bị chặn bởi I/O **[đề xuất]**.
  3. Kill tiến trình con ở thời điểm ngẫu nhiên khi đang ghi (1.000 lần): tệp đích luôn là bản cũ hoặc bản mới nguyên vẹn (kiểm checksum).
  4. Tệp bị thay bằng đổi tên đè khi đang map: view cũ vẫn đọc đúng; tệp bị cắt ngắn tại chỗ: trả `IoError`, không crash (3 hệ điều hành).
  5. Đường dẫn > 260 ký tự và tên tiếng Việt mở, đọc, ghi đúng trên Windows.
  6. Đạt DoD-C, DoD-H.

### NT-011 · I/O tệp: đường dẫn tìm kiếm, đường dẫn tương đối của xref, theo dõi thay đổi tệp
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-109
- **Đầu vào:** 05 §9.4; thứ tự tìm tệp của AutoCAD cho xref, ảnh, font, LISP (đường dẫn đã lưu → thư mục bản vẽ chủ → project search path → support file search path); `QFileSystemWatcher` (QtCore, 05 §2); API của NT-109.
- **Đầu ra:** `core/base/io/`: chuẩn hóa và so sánh đường dẫn (không phân biệt hoa thường trên Windows và macOS, NFC trên macOS); `relativePath(from, to)`; giải đường dẫn tương đối, tuyệt đối, UNC; `SearchPaths::resolve(tên, loại, thư mục chủ)` có cache, theo đặc tả `docs/specs/base/search-paths.md`; `FileWatcher` bọc QFileSystemWatcher (gộp sự kiện, theo lại tệp sau đổi tên đè; ổ mạng kiểm `mtime` khi ứng dụng được focus thay vì polling). Neo này là API I/O chung, gồm cả NT-109.
- **Tiêu chí nghiệm thu:**
  1. 60 ca giải đường dẫn (xref tương đối lồng nhau, đổi chữ ổ đĩa, UNC, bản vẽ chủ chuyển cùng thư mục, tên tiếng Việt NFC/NFD) khớp 100% đặc tả **[đề xuất]**.
  2. Sửa tệp đang theo dõi trên ổ cục bộ → một sự kiện ≤ 500 ms; lưu kiểu đổi tên đè gộp thành 1 sự kiện **[đề xuất]**.
  3. Theo dõi 1.000 tệp, rảnh 60 s: 0 lần thức dậy; `resolve` trúng cache ≤ 2 µs **[đề xuất]**.
  4. Đạt DoD-C, DoD-H.

### NT-014 · Nén và checksum
- **Mốc:** S1 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-106, NT-103
- **Đầu vào:** 05 §2 (zlib/libdeflate), §9.3 (checksum dùng lệnh phần cứng); README DoD-C 7, DoD-H 2; RFC 1950, RFC 1951; bộ dữ liệu Silesia corpus; nhu cầu của PLT-004 (nén song song theo tờ), NT-114, NT-015.
- **Đầu ra:** `core/base/compress/`: `crc32`, `crc32Combine`, `adler32` (libdeflate: PCLMULQDQ, lệnh CRC32/PMULL của ARMv8); `deflate`/`inflate` cả bộ đệm (libdeflate) và dạng luồng khi chưa biết kích thước (zlib); `deflateBatch`/`inflateBatch` xử lý song song nhiều khối độc lập qua NT-106; giải nén luôn trừ vào `ParseBudget` (NT-103); allocator của zlib đi qua móc đếm NT-004.
- **Tiêu chí nghiệm thu:**
  1. Một nhân: CRC32 và Adler-32 ≥ 8 GB/s (dữ liệu trong L2); inflate ≥ 500 MB/s đầu ra; deflate mức 6 ≥ 50 MB/s, mức 1 ≥ 200 MB/s trên Silesia **[đề xuất]**.
  2. `deflateBatch` 1.000 khối × 256 KiB: hiệu suất ≥ 70% đến 16 luồng (phục vụ xuất PDF); `inflateBatch` ≥ 60% đến 8 luồng; CRC32 gọi đồng thời ≥ 60% tuyến tính đến 8 luồng **[đề xuất]**.
  3. Đầu ra nén giống từng byte ở 1–32 luồng và trên 3 hệ điều hành (ca NT-107); zlib chuẩn giải nén đúng 100% đầu ra.
  4. Bom nén (tỉ lệ > 1.000:1 hoặc vượt ngân sách) dừng bằng lỗi trước khi cấp phát vượt ngân sách; fuzz `inflate` ≥ 1 giờ.
  5. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** nén song song bên trong một luồng deflate (kiểu pigz): thêm khi PLT-004 đo thấy cần. LZ77 riêng của DWG thuộc DWG-005, DWG-021.

### NT-114 · Ghi ZIP dạng luồng (stored, deflate, CRC-32)
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** NT-014, NT-109
- **Đầu vào:** APPNOTE.TXT của PKWARE (ZIP64, cờ UTF-8 bit 11, data descriptor); nhu cầu xuất XLSX/CSV của module VN ở mốc VP (thay cho bộ ghi ZIP tạm trong VN).
- **Đầu ra:** `core/base/zip/zip_writer.h`: `ZipWriter` ghi tuần tự vào sink (tệp qua `AtomicFileWriter` hoặc bộ đệm); entry stored hoặc deflate (NT-014, CRC-32 tính khi ghi); data descriptor khi chưa biết kích thước; ZIP64 tự bật khi > 4 GiB hoặc > 65.535 entry; tên UTF-8; mốc thời gian cố định tùy chọn; `addStored` cho PNG/JPG đã nén; nhiều entry nén song song qua `deflateBatch`, ghi theo thứ tự thêm.
- **Tiêu chí nghiệm thu:**
  1. Tệp ghi ra đọc đúng bằng 7-Zip CLI, `unzip` (Info-ZIP) và `zipfile` của Python, CRC khớp; ca > 4 GiB và > 65.535 entry dùng ZIP64 đúng.
  2. Ghi luồng entry 1 GB: bộ nhớ đỉnh ≤ 8 MiB, không giữ cả entry trong RAM **[đề xuất]**.
  3. Hai lần ghi cùng đầu vào cho tệp giống từng byte trên 3 hệ điều hành và mọi số luồng (ca NT-107).
  4. Entry lớn đạt ≥ 90% tốc độ deflate của NT-014; nhiều entry nén song song ≥ 60% tuyến tính đến 8 luồng **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

### NT-015 · Đọc/ghi ZIP tối thiểu
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-114, NT-011, NT-103
- **Đầu vào:** APPNOTE.TXT của PKWARE; nhu cầu của VN-013 (XLSX), VN-009 (gói Thư viện TCVN), CUS-002 (CUIX), ETRANSMIT; API của NT-114, NT-012.
- **Đầu ra:** `core/base/zip/`: `ZipReader` (central directory, ZIP64, stored và deflate, kiểm CRC, tên UTF-8 hoặc CP437/code page cấu hình qua NT-012, giải nén song song theo entry, giới hạn qua `ParseBudget`); giải nén ra thư mục qua `AtomicFileWriter`; API đọc/ghi chung gồm `ZipWriter` của NT-114.
- **Tiêu chí nghiệm thu:**
  1. Đọc đúng nội dung và CRC của 100% bộ mẫu: tệp do NT-114, 7-Zip, Info-ZIP, Windows Explorer, Excel (XLSX) và AutoCAD (CUIX) tạo, kể cả ZIP64 và data descriptor.
  2. Từ chối 100% ca tấn công mẫu: zip slip (`..`, đường tuyệt đối), symlink, entry chồng lấn, tỉ lệ nén > 1.000:1, > 100.000 entry **[đề xuất]**; fuzz `ZipReader` ≥ 1 giờ.
  3. Đọc central directory 10.000 entry ≤ 20 ms; giải nén song song ≥ 60% tuyến tính đến 8 luồng **[đề xuất]**.
  4. Tên không có cờ UTF-8 (tên tiếng Việt từ công cụ nén cũ) giải đúng 100% ca mẫu theo code page cấu hình.
  5. Đạt DoD-C, DoD-H.

## 6. Cài đặt và tác vụ nền

### NT-010 · Kho cài đặt có kiểu: phạm vi máy, người dùng, profile
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** NT-011, NT-005
- **Đầu vào:** 05 §17 (phiên bản theo năm); nhu cầu của INT-005 (sysvar phạm vi profile), UI0-010 (bố cục palette), CUS-003 (TCMIGRATE); `QJsonDocument` (QtCore).
- **Đầu ra:** `core/base/settings/`: khóa có kiểu `Setting<T>{"khóa", phạm vi, mặc định, ràng buộc}`; phạm vi máy, người dùng, profile; tệp JSON trong thư mục theo năm (`%APPDATA%\TrionCAD\<năm>`, `~/Library/Application Support/TrionCAD/<năm>`, `$XDG_CONFIG_HOME/trioncad/<năm>`); đọc từ snapshot bất biến; ghi gom bằng một tác vụ nền NT-005 qua `AtomicFileWriter`; chuỗi di trú `vN → vN+1`, nhập từ thư mục năm trước; xuất/nhập `.trionprofile`; thông báo thay đổi theo khóa.
- **Tiêu chí nghiệm thu:**
  1. Đọc một khóa ≤ 50 ns, không khóa; 10⁴ lần `set` liên tiếp gom thành ≤ 2 lần ghi đĩa **[đề xuất]**.
  2. Kill tiến trình khi đang ghi (1.000 lần): tệp luôn hợp lệ. Tệp hỏng hoặc sai kiểu: giữ bản `.bad`, dùng mặc định, log cảnh báo.
  3. Tệp mẫu của mọi phiên bản schema trước nạp đúng 100% giá trị; hai tiến trình cùng ghi các khóa khác nhau không mất khóa nào.
  4. Nhập profile không tin cậy: tối đa 10 MB **[đề xuất]**, khóa lạ bỏ qua có báo cáo; fuzz ≥ 1 giờ.
  5. Đạt DoD-C.

### NT-013 · Đăng ký tác vụ nền: tiến trình, hủy, kết quả, thông báo
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-005
- **Đầu vào:** README DoD-L (thao tác dài: > 0,5 s con trỏ bận, > 2 s tiến trình và nút hủy), DoD-H 5, §4.2 (NT-013 sở hữu, UI0-009 chỉ hiển thị); nhu cầu của PLT-008, VN-006, DOC-019.
- **Đầu ra:** `core/base/jobs/`: `BackgroundJobs::start(tên, loại, khóa chủ, fn(Progress&, CancelToken))`; tiến trình phân cấp có trọng số; trạng thái chạy, xong, lỗi (`trion::Error`), đã hủy; cờ `busyCursor` (0,5 s) và `showProgress` (2 s); hủy mọi việc theo khóa chủ (khi đóng document); người nghe nhận cập nhật gộp trên luồng UI qua `postToUi`.
- **Tiêu chí nghiệm thu:**
  1. 1.000 việc, mỗi việc cập nhật tiến trình 1 kHz: luồng UI nhận ≤ 10 lô/s, áp mỗi lô ≤ 1 ms **[đề xuất]**.
  2. Hủy → trạng thái `Cancelled` ≤ 50 ms với việc tôn trọng `CancelToken`; đóng document hủy 100% việc gắn với nó.
  3. 10⁵ việc liên tiếp: không rò bộ nhớ (ASan), registry trở về rỗng.
  4. Đạt DoD-C, DoD-H, DoD-L.

## 7. Phát hành: ký số, phiên bản, PGO

### NT-110 · Ký số mã trên Windows trong CI
- **Mốc:** CC · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** NT-002
- **Đầu vào:** 05 §3 (chứng thư ký số, SmartScreen), §16 (ký số tệp cài đặt và bản cập nhật); Baseline Requirements cho chứng thư ký mã của CA/Browser Forum (khóa trên HSM); nhu cầu của VN-017 (gói cài đặt CC), PLF-009.
- **Đầu ra:** `ci/signing/`: ký Authenticode SHA-256 kèm dấu thời gian RFC 3161 cho EXE, DLL, MSI/MSIX; khóa ở HSM đám mây hoặc token trên runner ký riêng **[đề xuất]**; chỉ job từ nhánh hoặc tag được bảo vệ gọi được; bước kiểm `signtool verify /pa /all`; quy trình gia hạn chứng thư.
- **Tiêu chí nghiệm thu:**
  1. 100% tệp PE trong artifact phát hành có chữ ký và dấu thời gian hợp lệ (quét tự động).
  2. Job từ nhánh không bảo vệ hoặc fork gọi bước ký bị từ chối (test cấu hình); quét secret không thấy khóa riêng trên runner thường.
  3. Tệp ký bằng chứng thư thử đã hết hạn vẫn hợp lệ nhờ dấu thời gian.
  4. Bước ký thêm ≤ 3 phút vào pipeline phát hành **[đề xuất]**.
  5. Đạt DoD-C (mục 2, 10, 11).

### NT-111 · Phiên bản theo năm, kênh phát hành, feature flag
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-110, NT-101, NT-010
- **Đầu vào:** 05 §17 (nội bộ mỗi ngày → beta 2 tuần → ổn định mỗi quý; feature flag cho tính năng chưa xong; phiên bản theo năm; ADR trong `docs/adr/`); nhu cầu của PLF-006 (feed theo kênh), PLF-007, DWG-027 (AppInfo).
- **Đầu ra:**
  - `cmake/version.cmake`: nguồn duy nhất `NĂM.U.B` (năm, bản cập nhật, số build) **[đề xuất]**, nhúng vào VERSIONINFO, Info.plist, `core/base/version.h`, `trion --version`.
  - `core/base/features.def` và `TRION_FEATURE(tên)`: chủ sở hữu, hạn gỡ, mặc định theo kênh; bản ổn định loại bỏ mã của tính năng chưa xong lúc biên dịch; ghi đè qua NT-010 chỉ ở kênh nội bộ.
  - `ci/release/`: workflow 3 kênh sinh artifact đã ký và manifest JSON (phiên bản, kênh, commit, băm, SBOM của NT-101); ADR phiên bản trong `docs/adr/`.
- **Tiêu chí nghiệm thu:**
  1. Mọi binary của một build mang cùng chuỗi phiên bản; số build tăng đơn điệu (script kiểm).
  2. Bản ổn định có 0 flag "chưa xong" bật và không chứa ký hiệu của mã bị flag loại (quét binary).
  3. Tra một flag ≤ 1 ns (hằng biên dịch hoặc bool đã cache) **[đề xuất]**; flag quá hạn gỡ bị báo trong job đêm.
  4. Manifest của 3 kênh qua kiểm `ci/release/manifest.schema.json`.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** cổng tính năng theo giấy phép (PLF-002); feed cập nhật và kiểm chữ ký phía client (PLF-006).

### NT-112 · Quy trình huấn luyện PGO cho bản phát hành
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** NT-111, NT-107, INT-019, QA-001
- **Đầu vào:** 05 §4 ADR-001, §9.3 (PGO + LTO, bản native x86-64 và ARM64), §17; preset PGO của NT-001; `trion-console` (INT-019); BENCH-STD (QA-001).
- **Đầu ra:** `ci/pgo/`: build đo cho MSVC (`/GENPROFILE`) và Clang (`-fprofile-generate`); kịch bản huấn luyện khai báo ở `ci/pgo/scenarios.yaml`, chạy qua `trion-console` (khởi đầu: mở DWG 20 MB và 100 MB của BENCH-STD, regen toàn bộ; module khác thêm kịch bản của mình); gộp hồ sơ (`pgomgr`, `llvm-profdata`) theo nền tảng, kiến trúc, trình biên dịch; build phát hành dùng hồ sơ; job đêm huấn luyện lại.
- **Tiêu chí nghiệm thu:**
  1. Bản PGO + LTO nhanh hơn bản chỉ LTO ≥ 5% trung vị trên bộ benchmark mở DWG và regen; không benchmark nào chậm hơn > 2% **[đề xuất]**.
  2. Băm tất định NT-107 của bản PGO và bản không PGO giống nhau 100%; toàn bộ test qua trên bản PGO.
  3. Hồ sơ cũ hơn 7 ngày hoặc thiếu dữ liệu cho > 20% hàm nóng → pipeline phát hành tự huấn luyện lại **[đề xuất]**.
  4. Huấn luyện ≤ 30 phút mỗi nền tảng (Windows x86-64 và ARM64, macOS ARM64, Linux x86-64) **[đề xuất]**.
  5. Đạt DoD-C (mục 2, 4, 9).

### NT-113 · Ký số và notarize bản macOS
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** NT-110
- **Đầu vào:** 05 §3 (Apple Developer Program, bắt buộc để notarize), §16; tài liệu Apple về Developer ID, hardened runtime, `notarytool`, stapling; nhu cầu của UIA-018, PLF-009.
- **Đầu ra:** `ci/signing/macos/`: ký từ trong ra ngoài (framework Qt, dylib, app, DMG/pkg) bằng Developer ID, hardened runtime, entitlements tối thiểu; gửi `notarytool`, chờ kết quả, `stapler staple`; bước kiểm `codesign --verify --deep --strict` và `spctl -a -vv`.
- **Tiêu chí nghiệm thu:**
  1. Gatekeeper chấp nhận app và DMG trên máy macOS sạch (VM trên runner `mac-arm`) không cảnh báo.
  2. 100% Mach-O trong gói có chữ ký hợp lệ; notarize lỗi thì job lỗi kèm log của Apple.
  3. Notarize và staple ≤ 15 phút **[đề xuất]**; khóa API chỉ dùng được từ nhánh hoặc tag bảo vệ.
  4. Đạt DoD-C (mục 2, 10, 11).

## Tóm tắt theo mốc

| Mốc | Task | Số task |
|---|---|---|
| S1 | NT-001, NT-002, NT-003, NT-004, NT-101, NT-102, NT-009, NT-103, NT-104, NT-105, NT-107, NT-106, NT-005, NT-012, NT-007, NT-109, NT-011, NT-014 | 18 |
| S2 | NT-108, NT-006, NT-008 | 3 |
| CC | NT-110 | 1 |
| V | NT-010, NT-013, NT-111, NT-112 | 4 |
| VP | NT-114 | 1 |
| EA | NT-015 | 1 |
| 1.0 | NT-113 | 1 |
| **Tổng** | | **29** |
