# RND — Renderer trên Qt RHI, GPU compute, tự thích ứng, TRIONBENCH

> Phân rã module renderer: vùng vẽ (Model, Layout, viewport), overlay và tô sáng, ảnh raster và PDF underlay, kết xuất ngoài màn hình, xem 3D cơ bản, tự thích ứng phần cứng, TRIONBENCH và benchmark đồ họa trong CI.
> Thư mục mã: `render/` (lõi), `render/commands/` (lệnh của module), `render/tests/`, `render/benchmarks/`, `render/fuzz/`, `render/docs/`; đặc tả lệnh ở `docs/specs/commands/`.
> Căn cứ: [05](../05-kien-truc-ky-thuat.md) §1.1, §4 (ADR-002), §9, §14, §21 (S2); [07.1](../07-dac-ta-ui-1-khung-ung-dung.md) §9.1–§9.6, §9.12–§9.13, §13.2; [07.2](../07-dac-ta-ui-2-bang-va-hop-thoai.md) UI-I07; [07.3](../07-dac-ta-ui-3-tinh-nang-vn-va-he-thong.md) UI-L10, UI-L11; [03](../03-chuc-nang-autocad.md) §21. Quy ước task, DoD và task neo: [README](README.md).

## 0. Ghi chú thiết kế

- **Lớp bọc mỏng:** chỉ `render/rhi/` gọi QRhi (06 §9 rủi ro 11; bản Qt LTS ghim ở NT-001). Mỗi API QRhi được dùng có cách làm tương đương trên OpenGL 3.3 ghi trong `render/docs/rhi-gl33.md`, để còn giữ phương án dự phòng của ADR-002.
- **Mức tính năng nền** là tập con chung của OpenGL 3.3 core và GLES 3.0 (RND-106). Mọi đường vẽ phải chạy được ở mức này. Compute, MSAA, timestamp GPU, base instance là tăng tốc tùy chọn, luôn có đường dự phòng (05 §9.1).
- **Shader viết một lần** bằng GLSL 450, bake lúc build thành `.qsb` cho mọi backend (RND-101). Không có chuỗi shader trong mã C++.
- **Cảnh chia chunk không gian:** mỗi chunk là một lá của cây dựng bằng GEO-017, khoảng 8.000 primitive **[đề xuất]**, có gốc tọa độ `double` riêng; đỉnh lưu `float` tương đối gốc chunk (RTC). Renderer không dựng BVH theo entity: chọn đối tượng và OSNAP dùng chỉ mục DOC-009 + INT-009 trên CPU, không đọc ngược từ GPU (README §4.2, 05 §9.1).
- **Batch theo pipeline, không theo màu:** khóa vật liệu của RGN-001 ánh xạ thành chỉ số vào bảng kiểu trên GPU (màu, lineweight, linetype, transparency). Batch chỉ tách theo loại primitive, đục hay trong suốt và vùng clip **[đề xuất]**, nên số draw call không tăng theo số màu hay số layer.
- **Thứ tự vẽ mã hóa vào depth:** primitive đục vẽ theo batch, depth test giữ đúng thứ tự (SORTENTS). Primitive trong suốt vẽ sau, theo thứ tự, với depth test chặt nên một entity không blend chồng lên chính nó **[đề xuất]**.
- **Một luồng ghi lệnh GPU:** QRhi chỉ được gọi trên luồng UI. Việc nặng (đóng gói đỉnh, tam giác hóa, SDF, culling nhiều viewport, LOD) chạy trên NT-005 rồi áp theo lô, có ngân sách upload mỗi khung. Khung hình ổn định không cấp phát heap (đếm bằng NT-004).
- **Chỉ vẽ khi có thay đổi:** cảnh chính được cache thành texture; con trỏ, overlay và rê chuột vẽ lại mà không vẽ lại cảnh (RND-109). Rảnh thì 0 khung hình.
- **Tất định (DoD-C mục 4)** áp cho dữ liệu CPU: nội dung buffer, danh sách chunk thấy được, quyết định LOD, thứ tự vẽ (CI so hash). Ảnh do GPU vẽ được so bằng ngưỡng (SSIM, sai khác điểm ảnh) với ảnh chuẩn và với ảnh tham chiếu vẽ bằng `double` qua QPainter (RND-103), không so từng bit giữa các GPU **[đề xuất]**.
- **Màu và lệnh:** màu vùng vẽ và màu ngữ nghĩa (07.1 §13.2) vào renderer qua `CanvasPalette` do UI nạp từ token; `render/` không chứa mã màu. Lệnh của module nằm ở target `render/commands/` (liên kết `render` và `cmd`), để lõi `render/` không phụ thuộc command framework.

## 1. Nền QRhi, shader, thiết bị

### RND-101 · Pipeline shader: GLSL 450 bake thành `.qsb` cho mọi backend
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-001
- **Đầu vào:** 05 §4 (ADR-002), §9.1; tài liệu Qt 6 về QShader, QShaderBaker và công cụ `qsb` (Qt Shader Tools); đặc tả GLSL 4.50, GLSL 3.30, GLSL ES 3.00.
- **Đầu ra:**
  - `render/shaders/`: nguồn `.vert`, `.frag`, `.comp` và phần dùng chung `common/*.glsl`.
  - Hàm CMake `trion_add_shaders()` trong `render/shaders/CMakeLists.txt`: gọi `qsb` lúc build, nhúng `.qsb` vào tài nguyên Qt. Đích: SPIR-V (Vulkan), HLSL SM 5.0 (D3D11, D3D12), MSL (Metal), GLSL 330 core, GLSL 300 es; shader compute thêm GLSL 430 và GLSL 310 es. HLSL kèm DXBC biên dịch sẵn, MSL kèm metallib khi build trên đúng hệ điều hành **[đề xuất]**.
  - `render/rhi/ShaderLibrary.h/.cpp`: nạp `QShader` theo tên, kiểm phiên bản, cache trong tiến trình.
  - `render/shaders/generated/*_layout.h` sinh từ reflection: struct C++ của uniform block, `static_assert` kích thước và offset theo std140.
  - `render/tests/shader_library_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Mọi shader đồ họa có đủ các đích trên, kể cả GLSL 330 và 300 es (tức không dùng SSBO, image load/store); thiếu một đích thì build lỗi (CI có ca cố ý xóa một đích).
  2. Struct C++ khớp layout uniform do reflection báo, từng offset; sửa shader mà quên sửa struct thì build lỗi.
  3. Lúc chạy không biên dịch shader từ chuỗi: `ShaderLibrary` chỉ nạp `.qsb`; nạp toàn bộ ≤ 20 ms trên máy tầm trung **[đề xuất]**.
  4. Bake toàn bộ ≤ 30 s trên máy tầm trung; build tăng dần chỉ bake tệp đổi **[đề xuất]**.
  5. Đạt DoD-C.

### RND-102 · Thiết bị QRhi cho vùng vẽ: chọn backend, chuỗi dự phòng, dò khả năng
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-101, NT-008, NT-009
- **Đầu vào:** 05 §4 (ADR-002: `QRhiWidget`/QRhi, D3D11 cho GPU Intel cũ), §9.1, §9.5; 07.3 UI-L10 (danh sách backend, GPU, VRAM); 07.1 §10 (chỉ báo đồ họa: backend, GPU, FPS); tài liệu Qt 6 về QRhi, QRhiWidget; tài liệu DXGI (`IDXGIAdapter3::QueryVideoMemoryInfo`), Vulkan (`VK_EXT_memory_budget`), Metal (`recommendedMaxWorkingSetSize`) về dung lượng và ngân sách bộ nhớ GPU.
- **Đầu ra:**
  - `render/rhi/RhiDevice.h/.cpp`: tạo QRhi cho widget vùng vẽ và cho kết xuất ngoài màn hình (không cửa sổ). Thứ tự mặc định: Windows D3D11, lỗi thì OpenGL (D3D12 chọn được theo cấu hình); macOS Metal; Linux Vulkan, lỗi thì OpenGL **[đề xuất]**. Chính sách tự động theo phần cứng thuộc RND-018.
  - Tạo thất bại thì thử backend kế, ghi lý do vào log (NT-009).
  - `render/rhi/DeviceCaps.h/.cpp`: số mẫu MSAA hỗ trợ, compute, instancing, base vertex/instance, index 32-bit, cỡ texture tối đa, timestamp GPU, bộ nhớ hợp nhất, VRAM riêng và dùng chung, ngân sách bộ nhớ hiện tại, hãng, thiết bị, phiên bản driver; ghi vào hồ sơ phần cứng NT-008.
  - API cho chỉ báo đồ họa của thanh trạng thái: tên backend, tên GPU.
  - `render/tests/rhi_device_test.cpp`, `render/tests/device_caps_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Trên 4 máy tham chiếu (README §3.1) và runner phần mềm (WARP, llvmpipe, lavapipe): tạo được thiết bị, vẽ tam giác kiểm tra, đọc lại đúng màu.
  2. Ép lỗi tạo backend (cờ kiểm thử): chuyển sang backend kế ≤ 500 ms, không crash, log ghi lý do **[đề xuất]**.
  3. `DeviceCaps` khớp giá trị đọc trực tiếp bằng API gốc (DXGI, Vulkan, Metal, OpenGL) trên từng máy; VRAM sai ≤ 1%.
  4. Từ lúc tạo thiết bị đến khung hình đầu ≤ 150 ms trên máy tầm trung, cache pipeline nguội **[đề xuất]**.
  5. Đạt DoD-C.

### RND-103 · Bộ kiểm thử ảnh vàng đa backend và kết xuất ngoài màn hình tối thiểu
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-102, NT-002, NT-006
- **Đầu vào:** 05 §14 (so sánh ảnh), §17 (runner tự host có GPU thật); kết xuất ngoài màn hình và đọc lại texture của QRhi; QPainter raster của Qt làm bộ vẽ tham chiếu.
- **Đầu ra:**
  - `render/offscreen/OffscreenTarget.h/.cpp`: render target màu và depth/stencil, MSAA tùy chọn, đọc lại thành `QImage`.
  - `render/image/ImageCompare.h/.cpp`: sai khác từng điểm ảnh có ngưỡng, tỉ lệ điểm ảnh sai, SSIM, bản đồ chênh lệch; bản SIMD (NT-006) và bản scalar cho cùng kết quả.
  - `render/tests/golden/`: khung chạy ca (cảnh nhỏ dựng bằng mã; ảnh tham chiếu vẽ bằng `double` qua QPainter; ảnh chuẩn lưu trong kho theo từng backend); lệnh cập nhật ảnh chuẩn, thay đổi phải qua review.
  - Ma trận CI: D3D11, D3D12, Vulkan, Metal, OpenGL trên runner GPU thật (mỗi đêm); WARP, llvmpipe, lavapipe (mỗi PR).
- **Tiêu chí nghiệm thu:**
  1. Một ca chạy trên mọi backend của ma trận bằng cùng một lệnh; kết quả JSON gồm SSIM, tỉ lệ điểm ảnh sai; trượt thì đính kèm ảnh chênh lệch.
  2. `ImageCompare`: bản SIMD và scalar trùng từng bit; SSIM lệch ≤ 1e-6 so với cài đặt tham chiếu bằng `double`; so hai ảnh 4K ≤ 30 ms trên máy tầm trung **[đề xuất]**.
  3. Ngưỡng mặc định: SSIM ≥ 0,99 so với ảnh chuẩn cùng backend, ≥ 0,98 so với tham chiếu QPainter, ≤ 0,5% điểm ảnh lệch quá 8/255 **[đề xuất]**. Từng ca được đặt ngưỡng chặt hơn.
  4. Phần chạy mỗi PR trên runner phần mềm ≤ 5 phút **[đề xuất]**.
  5. Đạt DoD-C, DoD-H.

### RND-104 · Laptop hai GPU: ưu tiên GPU rời, chọn adapter
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** RND-102
- **Đầu vào:** 05 §9.1 (export `NvOptimusEnablement`, `AmdPowerXpressRequestHighPerformance`); 07.3 UI-L10 (chọn GPU rời hoặc tích hợp, hiện VRAM); tài liệu DXGI về liệt kê adapter theo ưu tiên GPU; tài liệu Metal về liệt kê thiết bị.
- **Đầu ra:**
  - `render/rhi/gpu_preference_exports.cpp`: hai biến export, đóng gói thành object library để tệp thực thi `trion` liên kết (biến chỉ có tác dụng khi nằm trong `.exe`).
  - `render/rhi/AdapterSelect.h/.cpp`: liệt kê adapter (tên, VRAM, rời hay tích hợp, mã định danh); chọn theo thiết lập: Tự động là GPU hiệu năng cao **[đề xuất]**, hoặc adapter người dùng chọn; truyền adapter khi tạo QRhi (D3D11/D3D12 theo LUID, Vulkan theo physical device, Metal theo `MTLDevice`).
  - `render/tests/adapter_select_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Trên laptop hai GPU trong ma trận CI (NT-002): chế độ Tự động chạy trên GPU rời với D3D11, D3D12 và Vulkan; kiểm bằng `DeviceCaps`.
  2. `trion.exe` có đủ hai ký hiệu export (CI kiểm bằng `dumpbin /exports`).
  3. Chọn GPU tích hợp trong thiết lập: lần tạo thiết bị sau chạy trên GPU tích hợp. Adapter đã chọn không còn thì về Tự động, log cảnh báo.
  4. Logic chọn là hàm thuần trên danh sách adapter, có kiểm thử đơn vị cho 10 cấu hình giả lập **[đề xuất]**.
  5. Đạt DoD-C.

### RND-105 · Mất thiết bị (device lost) và khôi phục
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-102, RND-103, NT-009
- **Đầu vào:** 05 §9.1; tài liệu Qt 6 về `QRhi::FrameOpDeviceLost` và `QRhi::isDeviceLost()`; cơ chế giả lập TDR của backend D3D11 trong QRhi; công cụ `dxcap -forcetdr` (Windows Graphics Tools).
- **Đầu ra:**
  - `render/rhi/DeviceResources.h/.cpp`: sổ đăng ký tài nguyên GPU; mỗi chủ sở hữu cài `release()` và `recreate()`; dữ liệu nguồn giữ ở CPU hoặc dựng lại được từ cache regen.
  - Xử lý trong vòng khung: phát hiện, hủy tài nguyên, tạo lại QRhi, gọi `recreate()` theo thứ tự phụ thuộc, vẽ lại.
  - Bộ đếm lỗi đồ họa theo ngày (cho phần chẩn đoán của UI-L10). Mất thiết bị 3 lần trong 60 s thì đổi sang backend an toàn hơn và phát sự kiện để UI hiện banner **[đề xuất]**.
  - `render/tests/device_lost_test.cpp` (giả lập trong lớp bọc, chạy mọi backend); job đêm trên Windows dùng TDR thật.
- **Tiêu chí nghiệm thu:**
  1. Giả lập mất thiết bị ở 1.000 thời điểm ngẫu nhiên (giữa upload, giữa khung, khi đổi kích thước): không crash, không rò tài nguyên (ASan và lớp kiểm của backend); ảnh sau khôi phục khớp ảnh trước theo ngưỡng RND-103.
  2. TDR thật trên D3D11 và D3D12: thiết bị và tài nguyên kiểm thử (200 MB buffer, 50 pipeline) dựng lại ≤ 1 s; bản vẽ không mất dữ liệu **[đề xuất]**.
  3. Trong lúc khôi phục, luồng UI không chặn quá 100 ms; thao tác nhập không bị mất **[đề xuất]**.
  4. Đổi backend khi mất thiết bị lặp lại có kiểm thử riêng; mọi sự kiện ghi log NT-009.
  5. Đạt DoD-C, DoD-L.

### RND-106 · Mức tính năng nền OpenGL 3.3 core / GLES 3.0 (dự phòng của ADR-002)
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-102, RND-103
- **Đầu vào:** 05 §4 (ADR-002: nếu QRhi không đạt thì dùng OpenGL 3.3 core, tập con chung với GLES 3.0/WebGL2); 06 §9 rủi ro 11; đặc tả OpenGL 3.3 Core Profile, OpenGL ES 3.0.
- **Đầu ra:**
  - `render/rhi/FeatureLevel.h`: mức `Baseline` (GL 3.3 core / GLES 3.0) và các mức cao hơn; mỗi đường vẽ khai báo mức tối thiểu.
  - Cách thay cho tính năng thiếu ở mức nền: dữ liệu bảng (kiểu, cờ) đọc qua texture bằng `texelFetch` thay cho SSBO; `firstInstance` qua offset của vertex binding khi thiếu `QRhi::BaseInstance`; uniform block ≤ 16 KB.
  - `render/docs/rhi-gl33.md`: danh sách API QRhi mà `render/rhi/` dùng, mỗi API kèm cách làm tương đương trên OpenGL 3.3 core.
  - Bộ ảnh vàng RND-103 chạy thêm với ngữ cảnh GL 3.3 core và GLES 3.0 (Mesa llvmpipe) mỗi PR.
- **Tiêu chí nghiệm thu:**
  1. Mọi ca ảnh vàng đạt ngưỡng RND-103 trên GL 3.3 core và GLES 3.0, kể cả ca thêm về sau; ca không đạt ở mức nền thì CI chặn merge.
  2. `rhi-gl33.md` phủ 100% API QRhi dùng trong `render/rhi/` (script so với danh sách ký hiệu được gọi).
  3. Ép backend OpenGL trên máy thấp (Intel HD 520/620, Windows): mọi ca ảnh vàng đạt.
  4. Đạt DoD-C.

### RND-001 · Khởi tạo QRhi trong vùng vẽ: backend, mất thiết bị, GPU rời, dò khả năng
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** RND-102, RND-104, RND-105, RND-106
- **Đầu vào:** API của RND-102, RND-104, RND-105, RND-106; 05 §1.1 (tự chọn backend đúng trên 100% máy trong ma trận kiểm thử).
- **Đầu ra:**
  - `render/rhi/RenderSurface.h/.cpp`: điểm vào duy nhất cho UIA-008: tạo bề mặt vẽ, sự kiện mất và khôi phục thiết bị, đổi backend hoặc GPU lúc chạy (đi qua đường tạo lại của RND-105).
  - `render/tests/rhi_matrix_test.cpp`; `render/docs/backend.md` (thứ tự backend, cách ép backend để gỡ lỗi).
- **Tiêu chí nghiệm thu:**
  1. Ma trận CI (4 máy tham chiếu, laptop hai GPU, runner phần mềm): backend và adapter được chọn khớp bảng kỳ vọng 100% (05 §1.1).
  2. Đổi backend lúc chạy (D3D11 ↔ OpenGL, D3D11 ↔ D3D12): không khởi động lại, vùng vẽ hiện lại ≤ 2 s **[đề xuất]**.
  3. Kiểm thử tích hợp mất thiết bị, đổi GPU, kéo cửa sổ sang màn hình khác DPI chạy hết trên mọi backend.
  4. Đạt DoD-C.

## 2. Dữ liệu GPU

### RND-107 · Cấp phát con trên bộ đệm GPU và upload bất đồng bộ
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-102, NT-004, NT-007
- **Đầu vào:** 05 §9.1 (cập nhật cục bộ, upload bất đồng bộ qua staging buffer), §9.4; `QRhiBuffer` (Immutable, Static, Dynamic) và `QRhiResourceUpdateBatch` của QRhi.
- **Đầu ra:**
  - `render/gpu/BufferArena.h/.cpp`: trang buffer lớn 64 MB **[đề xuất]**, cấp phát con kiểu TLSF O(1), gom vùng trống, thống kê phân mảnh.
  - `render/gpu/UploadQueue.h/.cpp`: luồng nền đóng gói dữ liệu vào vùng nhớ CPU; luồng UI nộp theo ngân sách mỗi khung (byte và thời gian); vùng đang thấy được ưu tiên.
  - `render/tests/buffer_arena_test.cpp`, `render/benchmarks/buffer_arena_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Cấp phát hoặc giải phóng ≤ 100 ns trung bình, ≤ 1 µs p99; 10 triệu thao tác ngẫu nhiên không lỗi; phân mảnh ≤ 15% sau 1 triệu chu kỳ **[đề xuất]**.
  2. Upload 32 MB (cảnh 1 triệu đoạn) xong ≤ 100 ms trên máy tầm trung, ≤ 300 ms trên máy thấp; trong lúc đó luồng UI tốn ≤ 2 ms mỗi khung cho upload **[đề xuất]**.
  3. 0 cấp phát heap mỗi khung ở trạng thái ổn định (bộ đếm NT-004).
  4. Đường luồng nền → luồng UI sạch TSan.
  5. Đạt DoD-C, DoD-H.

### RND-003 · Bộ đệm GPU theo vật liệu: chunk, batch, cập nhật cục bộ theo vùng
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** RND-105, RND-107, RGN-001, GEO-017, QA-012, NT-005, NT-006
- **Đầu vào:** 05 §9.1 (bộ đệm đỉnh tĩnh theo vật liệu; cập nhật cục bộ theo vùng, không dựng lại toàn bộ), §9.4 (SoA, dòng cache 64 byte); mô hình primitive và khóa vật liệu RGN-001; dựng BVH song song GEO-017; cảnh QA-012 dưới dạng primitive RGN-001.
- **Đầu ra:**
  - `render/scene/Scene.h/.cpp`: cảnh gồm chunk không gian (lá của cây GEO-017), mỗi chunk có gốc `double`, hộp bao và dải instance theo từng pipeline.
  - `render/scene/StyleTable.h/.cpp`: khóa vật liệu → chỉ số bảng kiểu trên GPU (màu RGBA, lineweight, linetype, cờ "theo màu nền" cho ACI 7, wipeout, nền MTEXT, phân giải theo từng lượt vẽ **[đề xuất]**).
  - `render/scene/EntitySlots.h/.cpp`: entity → slot (dùng cho cờ tô sáng RND-013) → các dải primitive; thêm, thay, xóa O(1) trung bình; nén vùng chết ở nền.
  - Khóa batch = (loại primitive, đục hay trong suốt, vùng clip); các dải chunk liền nhau gộp thành một draw.
  - Đăng ký tài nguyên với RND-105 để dựng lại khi mất thiết bị.
  - `render/tests/scene_test.cpp`, `render/benchmarks/scene_build_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Dựng cảnh 1 triệu đoạn thẳng và cung (QA-012) ≤ 300 ms trên 8 luồng máy tầm trung; hiệu suất song song ≥ 60% đến 8 luồng **[đề xuất]**.
  2. Sửa 1 entity: upload ≤ 64 KB, ≤ 0,5 ms luồng UI, hiện ở khung kế tiếp. Sửa 10.000 entity: áp xong trong ≤ 2 khung, không khung nào vượt 16,6 ms **[đề xuất]**.
  3. Draw call mỗi khung ở zoom toàn bộ ≤ 2 × số pipeline, không phụ thuộc số màu, layer, linetype **[đề xuất]**.
  4. Bộ nhớ GPU ≤ 40 byte mỗi đoạn thẳng, ≤ 48 byte mỗi cung **[đề xuất]**.
  5. Đổi giá trị một mục bảng kiểu: cập nhật đúng một dòng bảng, không dựng lại batch.
  6. Nội dung buffer và danh sách chunk trùng hash trên 3 hệ điều hành và 1–32 luồng.
  7. Mất thiết bị (TDR thật trên D3D11): cảnh 1 triệu entity dựng lại từ dữ liệu CPU, vùng vẽ hiện lại ≤ 2 s; ảnh khớp ảnh trước khi mất theo ngưỡng RND-103 **[đề xuất]**.
  8. Đạt DoD-C, DoD-H.

## 3. Primitive cơ bản: nét, cung, linetype, khử răng cưa, RTC

### RND-004 · Đường thẳng có lineweight: quad trong vertex shader, mối nối, đầu mút
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-003, RND-103
- **Đầu vào:** 05 §9.1 (lineweight mở rộng thành quad trong vertex shader, mối nối và đầu mút đúng); 03 §10 (`LWEIGHT`, `LWDISPLAY`, P0); 07.1 §10 (bật/tắt hiển thị lineweight); kiểu đầu mút và mối nối của CTB (PLT-002) cho chế độ xem trước in.
- **Đầu ra:**
  - `render/prims/LinePipeline.h/.cpp`, `render/shaders/line.vert`, `line.frag`: mỗi đoạn là một instance, mang điểm trước và sau để dựng mối nối của polyline; nét mảnh 1 px và nét có độ rộng.
  - Đầu mút phẳng, vuông, tròn; mối nối miter (có giới hạn), vát, tròn. Mặc định trên màn hình: đầu mút và mối nối tròn **[đề xuất]**.
  - Hai chế độ độ rộng: Model (pixel, theo bảng quy đổi lineweight → pixel và hệ số hiển thị) và Paper/in (mm trên giấy theo zoom). `LWDISPLAY` tắt thì mọi nét 1 px; đổi không dựng lại batch.
  - Đoạn polyline có độ rộng (đều, vát) vẽ theo đơn vị bản vẽ.
  - `render/docs/lineweight.md` (bảng quy đổi), ca ảnh vàng `render/tests/golden/lines/`.
- **Tiêu chí nghiệm thu:**
  1. Ảnh vàng cho mọi tổ hợp đầu mút × mối nối × 12 độ rộng × góc 0–180°: SSIM ≥ 0,98 so với QPainter vẽ bằng `double`; mép nét lệch ≤ 0,5 px **[đề xuất]**.
  2. Polyline trong suốt có mối nối chồng nhau: chỗ chồng không đậm hơn phần còn lại (không blend hai lần).
  3. 1 triệu đoạn lineweight 0,5 mm, zoom toàn bộ: GPU ≤ 8 ms trên máy tầm trung, ≤ 20 ms trên máy thấp **[đề xuất]**.
  4. Bật/tắt `LWDISPLAY`: khung kế tiếp đúng, 0 byte upload.
  5. Đạt DoD-C, DoD-L.

### RND-005 · Cung, đường tròn, ellipse giải tích trong fragment shader
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** RND-003, RND-103
- **Đầu vào:** 05 §9.1 (quad + fragment shader tính khoảng cách chính xác, mịn ở mọi mức zoom, không tessellate lại), §1.2; primitive cung, ellipse, đoạn cung (bulge) của polyline theo mô hình RGN-001.
- **Đầu ra:**
  - `render/prims/CurvePipeline.h/.cpp`, `render/shaders/arc.vert`, `arc.frag`, `ellipse.frag`: cung, đường tròn, cung ellipse, ellipse, đoạn cung của polyline; cung có độ rộng đều và vát dần.
  - Hình phủ là dải quad bám theo cung, số đoạn theo góc quét và cỡ trên màn hình; không dùng một quad bao cả đường tròn.
  - Khoảng cách tính tương đối điểm neo trên cung để không mất chính xác khi bán kính lớn: d = (|q|² − 2r(q·n)) / (|p − c| + r), với q = p − điểm neo, n là pháp tuyến hướng tâm **[đề xuất]**; ellipse dùng 2–3 bước Newton.
  - Đầu mút và mối nối tròn với đoạn thẳng kề (RND-004); lineweight theo hai chế độ như RND-004.
  - Ca ảnh vàng `render/tests/golden/curves/`, `render/benchmarks/curve_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Đường tâm nét lệch ≤ 0,25 px so với tham chiếu `double`, với bán kính trên màn hình từ 0,5 px đến 10⁶ px **[đề xuất]**.
  2. Ellipse tỉ lệ trục tới 1:10⁴: lệch ≤ 0,5 px **[đề xuất]**.
  3. Diện tích dải phủ ≤ 3 lần diện tích nét thật (đo trên CPU với 100.000 cung ngẫu nhiên) **[đề xuất]**.
  4. Ảnh vàng (góc quét 0,001°–360°, cung ngược chiều, lineweight 0–2 mm, độ rộng vát): SSIM ≥ 0,98 so với QPainter **[đề xuất]**.
  5. 1 triệu cung, zoom toàn bộ: GPU ≤ 10 ms trên máy tầm trung **[đề xuất]**.
  6. Đạt DoD-C, DoD-L.

### RND-006 · Linetype trong fragment shader theo tham số chiều dài cung
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-004, RND-005
- **Đầu vào:** 05 §8 (linetype theo chiều dài cung thật; `LTSCALE`, `PSLTSCALE`, `CELTSCALE`, `PLINEGEN`), §9.1; mẫu nét trong khóa vật liệu RGN-001 (RGN-011 điền đủ ngữ nghĩa ở mốc V).
- **Đầu ra:**
  - `render/prims/LinetypeTable.h/.cpp`: bảng mẫu (dash, gap, chấm) trên GPU dạng texture; tỉ lệ mẫu và pha bắt đầu theo primitive.
  - Sửa `line.frag`, `arc.frag`, `ellipse.frag`: chiều dài cung tích lũy theo đỉnh; ellipse qua bảng chiều dài; polyline liên tục khi `PLINEGEN` = 1, bắt đầu lại ở mỗi đỉnh theo pha do RGN cấp khi `PLINEGEN` = 0.
  - Pha tại mỗi đỉnh lưu theo modulo chu kỳ mẫu, tính bằng `double` trên CPU, để giữ chính xác trên đường rất dài.
  - Chấm (dash dài 0) vẽ thành điểm tròn đường kính bằng nét. Chu kỳ mẫu ngắn hơn 2 px trên màn hình thì vẽ nét liền **[đề xuất]**.
  - Hệ số tỉ lệ mẫu theo lượt vẽ (uniform) để RND-014 áp `PSLTSCALE` theo từng viewport.
  - Ca ảnh vàng `render/tests/golden/linetype/`.
- **Tiêu chí nghiệm thu:**
  1. Đầu và cuối từng dash lệch ≤ 0,5 px so với dash sinh bằng `double` trên CPU, cho 20 mẫu tự soạn trên đường thẳng, cung, ellipse, polyline có bulge **[đề xuất]**.
  2. Polyline dài 10⁸ đơn vị với mẫu 1 đơn vị: dash ở cuối đường vẫn lệch ≤ 0,5 px.
  3. Mẫu liên tục qua đỉnh khi `PLINEGEN` = 1; bắt đầu lại đúng pha khi `PLINEGEN` = 0.
  4. Pan, zoom từ 10⁻³ đến 10⁶ lần: hai khung cùng view cho cùng dash (không nhấp nháy).
  5. Chi phí GPU thêm ≤ 10% so với nét liền ở cảnh 1 triệu đoạn **[đề xuất]**.
  6. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** phần tử shape và text của linetype phức (RGN-011 sinh thành primitive riêng).

### RND-019 · Khử răng cưa: MSAA khi có, làm mượt cạnh trong shader khi không
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** RND-004, RND-005, RND-102
- **Đầu vào:** 05 §9.1 (MSAA khi phần cứng hỗ trợ, nếu không thì shader làm mượt cạnh), §9.5; 07.3 UI-L10 (bật/tắt MSAA, làm mượt nét); 07.2 UI-H09 (khử răng cưa khi xuất ảnh).
- **Đầu ra:**
  - `render/rhi/Antialiasing.h/.cpp`: chọn số mẫu MSAA từ `DeviceCaps` và mức chất lượng; render target MSAA và resolve.
  - Nét và đường cong: độ phủ tính từ khoảng cách trong fragment shader (`fwidth`), dùng được cả khi có và không có MSAA.
  - Hai cờ độc lập: MSAA, làm mượt nét; đổi lúc chạy không dựng lại cảnh.
- **Tiêu chí nghiệm thu:**
  1. Ảnh vàng với 4 tổ hợp cờ: SSIM ≥ 0,98 so với QPainter có khử răng cưa khi bật, không khử khi tắt **[đề xuất]**.
  2. Máy không có MSAA (ép bằng cờ kiểm thử): tự dùng đường shader, log một lần.
  3. MSAA 4× trên Intel HD 520/620 tốn thêm ≤ 25% thời gian GPU ở cảnh S2; vượt thì RND-018 hạ xuống 2× hoặc tắt **[đề xuất]**.
  4. Đổi cờ có hiệu lực ở khung kế tiếp.
  5. Đạt DoD-C.

### RND-011 · Độ chính xác RTC: trừ gốc khung nhìn bằng `double` trước khi chuyển `float`
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-004, RND-005, QA-012
- **Đầu vào:** 05 §9.1 (hình không rung ở tọa độ lớn VN-2000); README §3.1 (BENCH-STD có tọa độ VN-2000 ≈ 10⁹ đơn vị); cảnh VN-2000 của QA-012.
- **Đầu ra:**
  - `render/scene/Rtc.h/.cpp`: mỗi khung tính offset (gốc chunk − gốc khung nhìn) bằng `double` cho từng chunk thấy được, đẩy `float` vào uniform.
  - Đường cho primitive lớn: XLINE, RAY, đoạn dài hơn 64 lần khung nhìn, cung có bán kính trên màn hình > 4.096 px **[đề xuất]**: cắt theo khung nhìn bằng `double` trên CPU mỗi khung (song song NT-005), chỉ phần thấy được vào bộ đệm động.
  - `render/tests/rtc_test.cpp`, ca ảnh vàng VN-2000.
- **Tiêu chí nghiệm thu:**
  1. Cảnh VN-2000: mọi primitive lệch ≤ 0,5 px so với tham chiếu `double` ở mọi zoom từ toàn cảnh tới 1 px = 10⁻³ đơn vị **[đề xuất]**.
  2. Pan liên tục 10 s: nội dung tĩnh lệch giữa hai khung ≤ 0,1 px (không rung) **[đề xuất]**.
  3. XLINE và RAY chạm đúng mép vùng vẽ ở mọi zoom và mọi góc.
  4. 10.000 primitive lớn: phần cắt trên CPU ≤ 0,5 ms mỗi khung trên máy tầm trung **[đề xuất]**.
  5. Đạt DoD-C, DoD-H, DoD-L.

## 4. Culling và LOD

### RND-010 · Culling theo BVH và LOD
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** RND-003, RND-004, RND-005, GEO-017, QA-012, NT-005, NT-006
- **Đầu vào:** 05 §9.1 (culling bằng BVH; LOD: dưới 1 pixel thành điểm, chữ nhỏ thành khung, hatch dày thành mảng tô, spline giảm điểm); README §4.2 (cấu trúc BVH của GEO-017; không dựng BVH entity riêng); cảnh QA-012.
- **Đầu ra:**
  - `render/cull/ChunkCuller.h/.cpp`: duyệt cây chunk (GEO-017) theo khung nhìn bằng SIMD (NT-006); nhiều viewport song song (NT-005); kết quả là danh sách dải để vẽ.
  - `render/cull/LodPolicy.h/.cpp`: hàm thuần (loại primitive, số đo trên màn hình, độ lệch LOD) → mức LOD, cho mọi loại primitive của RGN-001 (đường, cung, polyline nhiều đỉnh, chữ, hatch, ảnh, instance). Ngưỡng mặc định ghi trong `render/docs/lod.md` **[đề xuất]**. RND-007, RND-009, RND-116 vẽ biểu diễn LOD của chữ, block, hatch theo hàm này.
  - Biểu diễn LOD cho đường và cung: chunk nhỏ hơn ngưỡng pixel vẽ bằng tập điểm dựng sẵn (một điểm cho mỗi ô lưới có hình, ở từng mức 2ᵏ). Polyline nhiều đỉnh (spline đã tessellate, đường đồng mức) có 4 mức giảm điểm theo sai số tính bằng đơn vị bản vẽ; chọn mức có sai số ≤ 0,5 px **[đề xuất]**.
  - Tham số độ lệch LOD do bộ điều tốc RND-110 điều khiển.
  - `render/benchmarks/cull_bench.cpp`, `render/tests/lod_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Culling ≤ 0,3 ms mỗi viewport với 1 triệu entity, ≤ 1,5 ms với 10 triệu, một luồng máy tầm trung **[đề xuất]**; nhiều viewport song song đạt hiệu suất ≥ 60% đến 8 luồng.
  2. Không bỏ sót: mọi primitive giao khung nhìn đều được vẽ (so với kiểm tra vét cạn trên 10.000 khung nhìn ngẫu nhiên).
  3. Zoom toàn bộ cảnh 10 triệu: số đỉnh nộp GPU ≤ 4 triệu; SSIM giữa ảnh có LOD và ảnh đầy đủ ≥ 0,97 **[đề xuất]**.
  4. Bộ nhớ thêm cho dữ liệu LOD ≤ 35% bộ nhớ hình gốc **[đề xuất]**.
  5. `LodPolicy` cho cùng kết quả trên 3 hệ điều hành; bản SIMD và scalar của culling cho cùng danh sách.
  6. Đạt DoD-C, DoD-H, DoD-L.

## 5. Khung hình và độ trễ

### RND-108 · Vòng khung hình theo yêu cầu: vsync, 120/144 Hz, nhịp khung, thống kê
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-102, NT-004, NT-008
- **Đầu vào:** 05 §1.1 (rảnh ~0% CPU/GPU), §9.1 (120/144 Hz; chỉ vẽ khi có thay đổi); DoD-H mục 6; 07.1 §10 (chỉ báo đồ họa có FPS); 07.3 UI-L10 (hiện FPS); tần số màn hình từ NT-008.
- **Đầu ra:**
  - `render/frame/FrameScheduler.h/.cpp`: cờ bẩn theo nguồn (cảnh, view, overlay, hoạt cảnh); gộp nhiều yêu cầu trong một chu kỳ quét; vẽ liên tục theo vsync chỉ khi đang tương tác hoặc có hoạt cảnh; không timer khi rảnh.
  - Tần số theo màn hình đang chứa cửa sổ, cập nhật khi kéo cửa sổ sang màn hình khác.
  - `render/frame/FrameStats.h/.cpp`: thời gian CPU, GPU (timestamp khi có `QRhi::Timestamps`), FPS, p50/p99, số khung vượt ngân sách; API cho thanh trạng thái và lớp FPS.
  - `render/tests/frame_scheduler_test.cpp`, `render/benchmarks/idle_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Rảnh 60 s: 0 khung được vẽ; CPU tiến trình trung bình ≤ 0,1%; module render không đánh thức vòng sự kiện lần nào **[đề xuất]**.
  2. Pan liên tục với tải vẽ giả lập 2 ms mỗi khung trên màn 60, 120, 144 Hz: đủ tần số quét; độ lệch chuẩn khoảng cách khung ≤ 0,5 ms; ≤ 1 khung trễ trên 1.000 khung **[đề xuất]**.
  3. 10 yêu cầu vẽ trong một chu kỳ quét chỉ sinh 1 khung.
  4. 0 cấp phát heap mỗi khung ở trạng thái ổn định (NT-004).
  5. Đạt DoD-C, DoD-H, DoD-L.

### RND-109 · Đường độ trễ thấp: cache ảnh cảnh, composite, chốt con trỏ muộn
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-108
- **Đầu vào:** 05 §9.1 (overlay vẽ lại không động vào cảnh chính); DoD-L (jig ≤ 8 ms/khung); cờ `MinimalBufferCount`, `NoVSync` của `QRhiSwapChain`; tài liệu QRhiWidget (vẽ vào texture rồi composite theo cửa sổ cấp cao nhất).
- **Đầu ra:**
  - `render/frame/SceneCache.h/.cpp`: cảnh chính vẽ vào texture (màu và depth) chỉ khi cảnh hoặc view đổi; khung chỉ đổi overlay thì composite texture rồi vẽ overlay.
  - Chốt vị trí con trỏ muộn: đọc vị trí chuột mới nhất ngay trước khi ghi lượt overlay; gộp sự kiện di chuột, chỉ dùng sự kiện cuối.
  - Đo và chọn cách nhúng: `QRhiWidget` so với `QWindow` có swapchain riêng (ít bộ đệm nhất) nhúng bằng `QWidget::createWindowContainer`; chọn cách đạt ngân sách, ghi quyết định vào `render/docs/latency.md` **[đề xuất]**.
  - Đầu dò độ trễ: dấu thời gian sự kiện → nộp khung → số khung đang xếp hàng; RND-025 dùng trong benchmark.
- **Tiêu chí nghiệm thu:**
  1. Chỉ con trỏ di chuyển, cảnh chính giả lập tốn 30 ms mỗi lần vẽ: từ sự kiện chuột tới nộp khung ≤ 3 ms p99 trên máy tầm trung, ≤ 6 ms trên máy thấp **[đề xuất]**.
  2. Số khung xếp hàng trước khi hiển thị ≤ 1; độ trễ input → photon ước tính ≤ 1 chu kỳ quét + 3 ms **[đề xuất]**.
  3. Khung chỉ đổi overlay tốn ≤ 1 ms CPU và ≤ 1 ms GPU, không phụ thuộc chi phí vẽ cảnh chính **[đề xuất]**.
  4. Ảnh composite trùng ảnh vẽ trực tiếp, từng điểm ảnh trên cùng GPU.
  5. Đạt DoD-C, DoD-L.

### RND-110 · Bộ điều tốc khung hình: hạ LOD tạm và tinh chỉnh dần
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-108, RND-010, QA-012
- **Đầu vào:** 05 §9.5 (khung vượt 16 ms thì hạ LOD tạm; dừng thao tác thì tinh chỉnh dần lên chất lượng tối đa); 07.1 UI-B13 (hiển thị dần khi mở tệp lớn).
- **Đầu ra:**
  - `render/frame/FrameGovernor.h/.cpp`: đọc thời gian khung (RND-108), điều độ lệch LOD (RND-010) có trễ (hysteresis) để giữ ngân sách theo tần số quét; tải quá thì giữ nhịp đều ở nửa tần số thay vì giật **[đề xuất]**.
  - Tinh chỉnh dần: sau 150 ms không thao tác **[đề xuất]**, vẽ lại chất lượng đầy đủ, chia nhiều khung theo thứ tự ưu tiên (giữa màn hình trước); có thao tác mới thì hủy ngay.
  - `render/tests/frame_governor_test.cpp` (đồng hồ giả lập, tất định).
- **Tiêu chí nghiệm thu:**
  1. Cảnh 10 triệu trên máy thấp: pan p99 ≤ 33 ms khi bật bộ điều tốc **[đề xuất]**.
  2. Dừng thao tác: ảnh đạt chất lượng đầy đủ ≤ 1 s, trùng từng điểm ảnh với ảnh vẽ thẳng ở chất lượng đầy đủ trên cùng GPU **[đề xuất]**.
  3. Không dao động: với tải cố định, mức LOD đổi ≤ 2 lần trong 10 s.
  4. Thao tác mới chen vào lúc đang tinh chỉnh: khung kế tiếp vẫn trong ngân sách.
  5. Đạt DoD-C, DoD-L.

### RND-002 · Lập lịch khung hình: chỉ vẽ khi có thay đổi, vsync, 120/144 Hz, ngân sách 16 ms, hạ LOD tạm và tinh chỉnh dần
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** RND-108, RND-109, RND-110, QA-012
- **Đầu vào:** API của RND-108, RND-109, RND-110; 05 §1.1, §9.1, §9.5; cảnh QA-012.
- **Đầu ra:**
  - `render/frame/FrameLoop.h/.cpp`: điểm vào duy nhất cho UIA-008 (yêu cầu vẽ, bắt đầu và kết thúc tương tác, ngân sách theo tần số quét, thống kê).
  - Kiểm thử tích hợp `render/tests/frame_loop_it.cpp` với kịch bản tương tác ghi sẵn.
- **Tiêu chí nghiệm thu:**
  1. Kịch bản ghi sẵn (pan, zoom, dừng, rê chuột, rảnh) trên cảnh 1 triệu: p99 ≤ 16,6 ms trên máy tầm trung, ≤ 33 ms trên máy thấp; khi rảnh 0 khung.
  2. Màn 144 Hz: cảnh 100.000 entity giữ 144 fps; cảnh 1 triệu giữ nhịp đều, không giật **[đề xuất]**.
  3. Mọi tiêu chí của RND-108, RND-109, RND-110 vẫn đạt khi chạy chung.
  4. Đạt DoD-C, DoD-L.

## 6. Benchmark đồ họa trong CI và nghiệm thu S2

### RND-025 · Benchmark đồ họa trong CI trên GPU thật, kể cả Intel HD cũ
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-002, RND-010, QA-012, NT-002, NT-003
- **Đầu vào:** 05 §1.1, §9.1 (bộ cảnh benchmark chạy trong CI trên máy GPU thật, kể cả Intel HD cũ), §14 (hiệu năng: mỗi PR rút gọn, mỗi đêm đầy đủ), §17 (runner tự host); README §3.1 (4 máy tham chiếu); DoD-C mục 9 (chặn merge khi chậm hơn baseline quá 5%); cảnh QA-012.
- **Đầu ra:**
  - `render/benchmarks/gpu_scene_bench.cpp`: chạy cảnh QA-012 với đường camera tất định (zoom toàn bộ, zoom sâu 1.000 lần, pan, chuyển viewport) ở hai chế độ: theo vsync (đo nhịp khung) và tắt vsync (đo thông lượng).
  - Chỉ số: thời gian khung CPU và GPU p50/p99, số draw call, byte upload, VRAM, số đỉnh nộp, độ trễ (đầu dò RND-109), CPU và GPU khi rảnh; xuất JSON theo định dạng của NT-003.
  - `render/benchmarks/CMakeLists.txt`: đăng ký với cổng hiệu năng NT-003; bộ rút gọn cho PR, bộ đầy đủ cho job đêm; ngưỡng theo máy trong `render/benchmarks/thresholds.json`.
  - Ảnh chụp khung cuối của mỗi cảnh, so nhanh với ảnh chuẩn RND-103.
- **Tiêu chí nghiệm thu:**
  1. Chạy được trên 4 máy tham chiếu với mọi backend có trên máy đó; Intel HD 520/620 có mặt trong mọi lần chạy đêm.
  2. Bộ PR ≤ 10 phút mỗi máy; bộ đêm ≤ 60 phút **[đề xuất]**.
  3. Lặp 5 lần trên cùng máy: p99 dao động ≤ 3% **[đề xuất]**.
  4. Cố ý thêm 10% tải GPU vào shader thì PR bị chặn; chậm 4% thì không bị chặn.
  5. Kết quả lưu theo máy và commit, ở dạng JSON mà bảng chỉ số QA-010 đọc được.
  6. Đạt DoD-C.

### RND-111 · Nghiệm thu spike S2: 1 triệu đoạn thẳng, cung giải tích, lineweight, linetype
- **Mốc:** S2 · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** RND-001, RND-002, RND-006, RND-011, RND-019, RND-025
- **Đầu vào:** 05 §21 (S2: QRhi D3D11/Metal/Vulkan với 1 triệu đoạn thẳng, cung giải tích, lineweight, linetype; ≥ 60 fps trên GPU tích hợp tầm trung, ≥ 30 fps trên Intel HD 520/620); 05 §4 ADR-002 (rủi ro QRhi, phương án OpenGL 3.3); README §6 (cổng G0); cảnh S2 của QA-012.
- **Đầu ra:**
  - `render/docs/spike-s2.md`: số đo từng máy và backend, ảnh chụp, danh sách API QRhi đã dùng kèm đánh giá rủi ro khi nâng bản Qt, kết luận cho ADR-002 (giữ QRhi hay chuyển sang OpenGL 3.3 trực tiếp). Bảng số sinh tự động từ JSON.
  - JSON kết quả RND-025 cho cảnh S2, lưu cùng commit.
- **Tiêu chí nghiệm thu:**
  1. Cảnh S2 (1 triệu đoạn thẳng và cung giải tích, bật lineweight và linetype), đường camera của RND-025, tắt vsync: p99 ≤ 16,6 ms (≥ 60 fps) trên máy tầm trung và ≤ 33 ms (≥ 30 fps) trên Intel HD 520/620, với D3D11.
  2. Cùng cảnh đạt ảnh vàng trên Metal (Mac), Vulkan (runner Linux) và OpenGL 3.3; số khung hình của từng backend có trong báo cáo.
  3. Rảnh 60 s: 0 khung, CPU ≤ 0,1% (RND-108).
  4. Tọa độ VN-2000 không rung (RND-011); mất thiết bị khôi phục được (RND-105).
  5. Đạt DoD-C, DoD-L.

## 7. Text (S6)

### RND-112 · Vẽ instanced cho hình định nghĩa dùng lại (glyph SHX, block)
- **Mốc:** S6 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-003, RND-004, RND-005
- **Đầu vào:** 05 §9.1 (instancing: hình học định nghĩa upload một lần, mỗi tham chiếu là một ma trận); mô hình instance của RGN-001.
- **Đầu ra:**
  - `render/scene/InstancedGeometry.h/.cpp`: định nghĩa (đoạn, cung, vùng tô trong hệ tọa độ riêng) và danh sách instance (ma trận affine `float` tương đối gốc chunk, slot entity, ghi đè màu, lineweight, linetype cho ByBlock, khóa thứ tự vẽ).
  - Biến thể instanced của pipeline đường và cung (RND-004, RND-005): lineweight vẫn tính trên màn hình; cung dưới tỉ lệ không đều hoặc đối xứng vẽ đúng như ellipse; tỉ lệ mẫu linetype nhân theo tỉ lệ của instance.
  - `render/tests/instancing_test.cpp`, `render/benchmarks/instancing_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Ảnh vẽ bằng instance khớp ảnh vẽ phẳng (mỗi instance biến đổi sẵn trên CPU), SSIM ≥ 0,995, kể cả tỉ lệ âm, tỉ lệ không đều, xoay **[đề xuất]**.
  2. 100.000 instance của định nghĩa 50 primitive: bộ nhớ GPU ≤ cỡ định nghĩa + 64 byte × số instance; zoom toàn bộ ≤ 8 ms GPU trên máy tầm trung **[đề xuất]**.
  3. Thêm hoặc xóa 1 instance: upload ≤ 1 KB.
  4. Đạt DoD-C, DoD-H.

### RND-113 · Tam giác hóa đa giác có lỗ cho vùng tô và chữ lớn
- **Mốc:** S6 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** GEO-001, GEO-003, QA-009, QA-012, NT-005, NT-007
- **Đầu vào:** predicate bền vững GEO-003; khung property-based QA-009; quy tắc tô even-odd (biên hatch, island) và nonzero (đường viền TrueType); cảnh hatch dày của QA-012.
- **Đầu ra:**
  - `render/tess/Triangulator.h/.cpp`: đầu vào là các vòng đa giác đã tessellate và quy tắc tô; xử lý lỗ lồng nhau, điểm trùng, cạnh thẳng hàng, vòng chạm nhau, tự cắt; đầu ra là lưới tam giác kín (không khe, không chồng).
  - Gọi hàng loạt song song (NT-005) cho nhiều vùng tô; dùng arena (NT-007), không cấp phát trong vòng lặp.
  - `render/tests/triangulator_test.cpp` (ca vàng, ca suy biến), `render/tests/triangulator_prop_test.cpp` (QA-009), `render/benchmarks/triangulator_bench.cpp`, fuzz target `render/fuzz/triangulator_fuzz.cpp` (biên lấy từ tệp bản vẽ là đầu vào không tin cậy).
- **Tiêu chí nghiệm thu:**
  1. 1 triệu ca ngẫu nhiên và suy biến (QA-009): tổng diện tích tam giác bằng diện tích vùng theo quy tắc tô, sai ≤ 1e-9 tương đối; không tam giác nào nằm ngoài vùng; mọi cạnh trong dùng đúng 2 lần (không khe).
  2. Đa giác 10.000 đỉnh có 100 lỗ ≤ 3 ms một luồng trên máy tầm trung **[đề xuất]**.
  3. 100.000 vùng tô của cảnh hatch dày: hiệu suất song song ≥ 60% đến 8 luồng.
  4. Fuzz 1 giờ không lỗi; kết quả trùng hash trên 3 hệ điều hành.
  5. Đạt DoD-C, DoD-H.

### RND-114 · Atlas SDF cho chữ TrueType nhỏ
- **Mốc:** S6 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-003, RND-107, NT-005
- **Đầu vào:** 05 §8 (text nhỏ dùng atlas SDF), §9.1; đường viền glyph (đoạn thẳng, Bézier bậc 2 và bậc 3) và metric; font IBM Plex Sans (OFL) cho ca kiểm thử và chữ trên overlay.
- **Đầu ra:**
  - `render/text/GlyphSource.h`: giao diện lấy đường viền glyph và metric; RGN-004 (SHX), RGN-005 (TrueType) cài giao diện này; kiểm thử dùng bản cài qua FreeType.
  - `render/text/SdfGenerator.h/.cpp`: sinh SDF nhiều kênh (MSDF) từ đường viền, song song, tự viết (không thêm thư viện ngoài, DoD-C mục 11).
  - `render/text/GlyphAtlas.h/.cpp`: trang atlas 2048², xếp kiểu skyline, LRU theo glyph, sinh bất đồng bộ; glyph chưa sẵn sàng thì hiện chậm tối đa 2 khung **[đề xuất]**.
  - `render/shaders/sdf_text.vert`, `sdf_text.frag`; ca ảnh vàng `render/tests/golden/text_sdf/`.
- **Tiêu chí nghiệm thu:**
  1. Chữ 6–48 px: SSIM ≥ 0,95 so với glyph raster hóa trực tiếp ở cùng cỡ **[đề xuất]**.
  2. Sinh một glyph ≤ 0,2 ms một luồng; 5.000 glyph vừa trong ≤ 32 MB atlas **[đề xuất]**.
  3. Đủ 134 chữ cái tiếng Việt có dấu (67 thường, 67 hoa), cả dạng dựng sẵn và tổ hợp: dấu không bị cắt, không lệch so với ảnh tham chiếu.
  4. Đạt DoD-C, DoD-H.

### RND-115 · Chữ lớn: lưới tam giác của glyph, vẽ instanced
- **Mốc:** S6 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-112, RND-113, RND-114
- **Đầu vào:** 05 §8 (text lớn dùng đường viền tam giác hóa); giao diện `GlyphSource` (RND-114).
- **Đầu ra:**
  - `render/text/GlyphMeshCache.h/.cpp`: làm phẳng Bézier theo sai số so với cỡ hiển thị (nhóm cỡ theo bậc 2 lần), tam giác hóa bằng RND-113 với quy tắc nonzero, cache theo (font, glyph, nhóm cỡ), LRU.
  - Mỗi ký tự là một instance (RND-112) của lưới glyph.
  - Ca ảnh vàng `render/tests/golden/text_mesh/`.
- **Tiêu chí nghiệm thu:**
  1. Chữ 48–4.000 px: mép lệch ≤ 0,5 px so với đường viền `double`; SSIM ≥ 0,98 so với QPainter vẽ cùng đường viền **[đề xuất]**.
  2. Chuyển giữa SDF và lưới quanh ngưỡng cỡ: bề rộng nét chữ đổi ≤ 0,5 px **[đề xuất]**.
  3. Tam giác hóa một glyph ≤ 0,5 ms; 100.000 ký tự cỡ lớn trên màn hình ≤ 8 ms GPU trên máy tầm trung **[đề xuất]**.
  4. Đạt DoD-C, DoD-H.

### RND-007 · Text: atlas SDF cho chữ nhỏ, tam giác hóa cho chữ lớn, SHX dạng nét
- **Mốc:** S6 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-010, RND-112, RND-114, RND-115, QA-012
- **Đầu vào:** 05 §8, §9.1, §21 (S6: render SHX/TTF tiếng Việt đúng 100% bộ ký tự); primitive chuỗi glyph của RGN-001; glyph SHX dạng nét (đoạn, cung) và đường viền TrueType qua `GlyphSource`; cảnh text dày của QA-012.
- **Đầu ra:**
  - `render/text/TextRenderer.h/.cpp`: chọn đường theo cỡ trên màn hình. SHX: instance của glyph nét (RND-112), có lineweight. TrueType: SDF khi ≤ 48 px, lưới khi > 48 px **[đề xuất]**. LOD theo RND-010: chữ thấp hơn 3 px vẽ thành khung, dưới 1 px thành điểm **[đề xuất]**; chế độ `QTEXTMODE` vẽ khung.
  - Ký tự đặt theo ma trận do RGN cấp (chiều cao, width factor, oblique, góc xoay, ngược, lộn ngược); RTC như mọi primitive.
  - Ca ảnh vàng tiếng Việt `render/tests/golden/text_vi/` cho SHX và TrueType.
- **Tiêu chí nghiệm thu:**
  1. 134 chữ cái tiếng Việt có dấu với font SHX và TrueType thử nghiệm: SSIM ≥ 0,97 so với ảnh tham chiếu ở 8, 16, 32 và 128 px **[đề xuất]**; kết quả là đầu vào cho nghiệm thu spike S6.
  2. Cảnh text dày của QA-012: pan, zoom ≥ 60 fps trên máy tầm trung, ≥ 30 fps trên máy thấp.
  3. Width factor, oblique, ngược, lộn ngược khớp ảnh tham chiếu vẽ bằng `double`.
  4. Lần đầu hiện chữ mới: không khung nào vượt ngân sách vì sinh glyph (sinh ở nền, RND-114).
  5. Đạt DoD-C, DoD-H, DoD-L.

## 8. Vùng tô, hatch, block

### RND-008 · Vùng tô (hatch đặc, solid, wipeout, gradient), transparency, thứ tự vẽ
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-003, RND-113, GEO-016
- **Đầu vào:** 05 §8 (hatch: tô đặc, gradient), §9.1 (tuân thủ draw order); 03 §5 (`WIPEOUT`, P1), §6 (`DRAWORDER`, P0), §10 (transparency, P1); 07.1 §10 (bật/tắt hiển thị độ trong suốt); khóa thứ tự vẽ trong primitive RGN-001 (giá trị do RGN-027 điền); tessellation theo sai số pixel GEO-016.
- **Đầu ra:**
  - `render/prims/FillPipeline.h/.cpp`, `render/shaders/fill.vert`, `fill.frag`: vùng tô từ biên (cạnh cong tessellate qua GEO-016, tam giác hóa bằng RND-113 ở nền), SOLID, TRACE. Khi zoom làm sai số cạnh cong vượt 0,25 px thì tessellate lại ở nền, không chặn khung **[đề xuất]**.
  - Gradient 9 kiểu (LINEAR, CYLINDER, INVCYLINDER, SPHERICAL, INVSPHERICAL, HEMISPHERICAL, INVHEMISPHERICAL, CURVED, INVCURVED), một hoặc hai màu, góc, lệch tâm; công thức ghi trong `render/docs/gradient.md`.
  - Wipeout và nền MTEXT tô bằng màu nền của lượt vẽ (nền Model, giấy, nền viewport).
  - Thứ tự vẽ mã hóa vào depth (§0); lượt trong suốt vẽ sau lượt đục; `TRANSPARENCYDISPLAY` tắt thì vẽ đục, không dựng lại batch.
  - Không có MSAA: viền vùng tô làm mượt bằng nét 1 px cùng màu **[đề xuất]**.
  - `render/tests/draw_order_prop_test.cpp`, ca ảnh vàng `render/tests/golden/fills/`.
- **Tiêu chí nghiệm thu:**
  1. Property test 10.000 cảnh ngẫu nhiên (vùng tô và nét chồng nhau, khóa thứ tự ngẫu nhiên, một phần trong suốt): ảnh khớp tham chiếu QPainter vẽ tuần tự đúng thứ tự, SSIM ≥ 0,99 **[đề xuất]**.
  2. Gradient: mỗi điểm ảnh lệch ≤ 1/255 mỗi kênh so với công thức tính bằng `double` (tắt khử răng cưa).
  3. Entity trong suốt tự chồng lên mình (polyline rộng, hatch nhiều vòng): không blend hai lần.
  4. 100.000 vùng tô đặc: pan ≥ 60 fps trên máy tầm trung **[đề xuất]**.
  5. Đạt DoD-C, DoD-H, DoD-L.

### RND-116 · Hatch mẫu dày tính trên GPU bằng shader thủ tục
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** RND-008, RND-010, RGN-001, QA-012
- **Đầu vào:** 05 §9.1 (GPU sinh nét mẫu hatch và linetype dày đặc), §8 (hatch: LOD khi nét quá dày); họ đường mẫu theo định dạng `.pat` (góc, gốc, dịch, khoảng cách, dash) trong primitive vùng tô mẫu của RGN-001 (RGN-012 điền); cảnh hatch dày của QA-012.
- **Đầu ra:**
  - `render/prims/PatternFillPipeline.h/.cpp`, `render/shaders/pattern_fill.frag`: vùng tô (tam giác của RND-008) cộng tối đa 8 họ đường mẫu **[đề xuất]** tính trong fragment shader: khoảng cách tới đường gần nhất của họ, dash theo pha, lineweight, khử răng cưa.
  - LOD (RND-010): khoảng cách giữa các đường < 2 px thì vẽ mảng tô với độ phủ trung bình **[đề xuất]**.
  - Đường dự phòng CPU: mẫu hơn 8 họ, hoặc khi cần nét đúng như bản in, vẽ các đoạn do RGN-012 sinh bằng pipeline đường.
  - Gốc mẫu tính bằng `double` theo gốc chunk (RTC).
- **Tiêu chí nghiệm thu:**
  1. So với đoạn mẫu sinh trên CPU (vẽ bằng pipeline đường): SSIM ≥ 0,99 khi khoảng cách đường ≥ 4 px, với 30 mẫu tự soạn **[đề xuất]**.
  2. Cảnh hatch dày của QA-012: ≥ 60 fps trên máy tầm trung, ≥ 30 fps trên máy thấp; không đoạn mẫu nào được sinh trên CPU chỉ để hiển thị.
  3. Tọa độ VN-2000: đường mẫu lệch ≤ 0,5 px.
  4. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** linetype dày (đã tính trong fragment shader ở RND-006, không sinh đoạn trên CPU).

### RND-009 · Instancing block, kể cả block lồng
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-010, RND-112, RGN-001, QA-012
- **Đầu vào:** 05 §9.1 (hình học mỗi định nghĩa block upload một lần, mỗi tham chiếu là một ma trận; bản vẽ có hàng chục nghìn block giống nhau); dữ liệu instancing theo mô hình RGN-001 (RGN-018 điền: insert lồng, MINSERT, ByBlock); cảnh block lồng của QA-012.
- **Đầu ra:**
  - `render/scene/BlockInstancing.h/.cpp`: làm phẳng cây insert lồng thành danh sách (định nghĩa lá, ma trận tổng hợp tính bằng `double` rồi đổi về tương đối gốc chunk); MINSERT sinh instance theo hàng và cột; thuộc tính ByBlock lấy từ insert gần nhất.
  - Định nghĩa dùng chung giữa các insert và giữa các layout; đếm tham chiếu, giải phóng khi hết dùng.
  - Culling và LOD theo instance: hộp bao instance; instance nhỏ hơn 2 px vẽ thành điểm hoặc khung **[đề xuất]**.
  - Tô sáng theo slot của insert cấp cao nhất (RND-013).
  - `render/tests/block_instancing_test.cpp`, `render/benchmarks/block_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Cảnh block lồng (3 cấp, 50.000 insert): bộ nhớ GPU giảm ≥ 10 lần so với vẽ phẳng; pan ≥ 60 fps trên máy tầm trung **[đề xuất]**.
  2. Ảnh khớp vẽ phẳng (SSIM ≥ 0,995), kể cả insert đối xứng, tỉ lệ không đều, MINSERT, màu, linetype, lineweight ByBlock qua 3 cấp.
  3. Sửa định nghĩa block (thay một primitive): mọi insert đổi ở khung kế tiếp; chỉ upload phần định nghĩa đã đổi.
  4. Đạt DoD-C, DoD-H, DoD-L.

## 9. Cảnh theo bản vẽ

### RND-117 · Cảnh theo bản vẽ: nhận regen gia tăng, Model và layout, nạp dần
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** RND-003, RGN-003, DOC-007, DOC-009, DOC-011, DOC-016, DOC-018, PLT-001, QA-001
- **Đầu vào:** 05 §5.3 (mở DWG: regen song song → upload GPU → khung hình đầu trong lúc nạp tiếp; lệnh LINE: cập nhật batch GPU cục bộ); 07.1 UI-B13 (hiển thị dần; xem, pan, zoom khi đang nạp); 07.2 UI-D10 tab Hệ thống (khi chuyển layout: regen, cache Model và layout gần nhất, cache tất cả); đầu ra regen gia tăng của RGN-003; snapshot DOC-009; layout và viewport DOC-011; nạp dần DOC-016; phiên nhiều bản vẽ DOC-018; khổ giấy và vùng in PLT-001; BENCH-STD (QA-001).
- **Đầu ra:**
  - `render/scene/DocumentScene.h/.cpp`: mỗi bản vẽ một bộ cảnh: Model, mỗi layout (giấy, viewport, primitive paper space); nhận lô thêm, thay, xóa theo entity từ RGN-003, áp vào RND-003 theo ngân sách upload mỗi khung.
  - Mô tả layout cho RND-014: khổ giấy, vùng in, danh sách viewport (tâm, tỉ lệ, góc xoay, biên clip, bật/tắt, khóa, viewport đang kích hoạt).
  - Nạp dần: nhận primitive ngay khi regen xong từng phần; vùng đang thấy được ưu tiên.
  - Chính sách cache layout theo `LAYOUTREGENCTL` (0, 1, 2); bản vẽ không hiện hành có thể bị RND-017 đẩy khỏi VRAM.
  - `LWDISPLAY`, `TRANSPARENCYDISPLAY` áp ở lượt vẽ, không dựng lại cảnh.
  - `render/tests/document_scene_test.cpp`, `render/benchmarks/open_to_first_frame_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Mở tệp 100 MB của BENCH-STD: khung hình đầu có nội dung ≤ 1,5 s trên máy tầm trung **[đề xuất]**; trong lúc nạp, pan và zoom ≥ 30 fps.
  2. Sửa 1 entity qua DOC-007: màn hình đúng ở khung kế tiếp sau khi regen trả kết quả; luồng UI tốn ≤ 0,5 ms.
  3. Chuyển giữa Model và một layout đã cache: ≤ 16,6 ms. `LAYOUTREGENCTL` = 0 thì dựng lại, có tiến trình nếu quá 0,5 s.
  4. Undo/redo 10.000 entity: hash nội dung cảnh trùng với cảnh dựng mới từ trạng thái bản vẽ.
  5. 5 bản vẽ mở cùng lúc: chỉ bản vẽ hiện hành tốn GPU mỗi khung.
  6. Đạt DoD-C, DoD-H, DoD-L.

## 10. Overlay, tô sáng, nền vùng vẽ

### RND-012 · Lớp overlay: jig, grip, marker bắt điểm, khung chọn, đường theo dõi
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** RND-004, RND-005, RND-008, RND-109, RND-114
- **Màn hình:** UI-A07, UI-A12
- **Đầu vào:** 05 §9.1 (overlay tạm thời ở lớp riêng); 07.1 §9.3 (Window viền liền nền màu A, Crossing viền đứt nền màu B, lasso, fence, WPolygon, CPolygon), §9.5 (grip vuông, chữ nhật, tam giác/mũi tên, tròn; nguội, rê, nóng phân biệt bằng màu và độ đậm), §9.6 (14 marker bắt điểm, đường theo dõi chấm kéo dài vô hạn), §9.8 (xem trước trong lệnh), §13.2 (`selection.*`, `grip.*`, `snap.marker`, `tracking.vector`); token trong `design/prototype/css/tokens.css`; prototype `design/prototype/index.html#UI-A12` (`js/canvas.js`: `drawOver`, `drawSnap`, `drawTemp`).
- **Đầu ra:**
  - `render/overlay/Overlay.h/.cpp`: lượt vẽ trên ảnh cảnh cache (RND-109), gồm hai loại nội dung: hình trong hệ tọa độ bản vẽ (jig, xem trước kết quả, markup, đám mây so sánh) dùng lại pipeline đường, cung, vùng tô, chữ; và ký hiệu cỡ cố định trên màn hình (grip, marker, khung chọn, đường theo dõi, nhãn chữ).
  - API cho INT-012 (jig), INT-013 (grip), INT-010 (marker), INT-011 (theo dõi), INT-008 (khung chọn), UI-M02 (markup); mỗi khung chỉ đẩy phần đổi.
  - `render/overlay/CanvasPalette.h`: màu ngữ nghĩa 07.1 §13.2 cho chủ đề Tối và Sáng (UI nạp từ token); cỡ grip, marker, ô bắt điểm theo thiết lập.
  - Lớp FPS (UI-L10) dùng thống kê RND-108.
  - Ca ảnh vàng `render/tests/golden/overlay/`.
- **Tiêu chí nghiệm thu:**
  1. Jig 10.000 primitive trên cảnh 10 triệu: ≤ 8 ms/khung trên máy tầm trung, ≤ 16 ms trên máy thấp (05 §1.1); cảnh chính không vẽ lại.
  2. Khung chỉ có con trỏ, marker, khung chọn: ≤ 1 ms CPU và ≤ 1 ms GPU **[đề xuất]**.
  3. 14 marker, 4 hình grip × 3 trạng thái, Window, Crossing, lasso, fence khớp ảnh chuẩn ở 100%, 150%, 200% DPI; Window và Crossing khác nhau cả kiểu viền.
  4. Đổi chủ đề Tối/Sáng: màu overlay đổi ở khung kế tiếp; script CI tìm thấy 0 hằng màu trong `render/`.
  5. Ghost của tập chọn: vẽ lại primitive sẵn có dưới ma trận affine, không regen (MOVE, ROTATE, SCALE, MIRROR, CMD-152): 100.000 đối tượng ≤ 8 ms/khung tầm trung **[đề xuất]**.
  6. Đạt DoD-C, DoD-L.

### RND-013 · Tô sáng (rê chuột, đã chọn, kết quả, làm mờ) qua bộ đệm cờ theo entity
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-003, RND-012
- **Màn hình:** UI-A07
- **Đầu vào:** 05 §9.1; 07.1 §9.3 (xem trước vùng chọn), §9.4 (rê chuột, đã chọn, kết quả tìm kiếm hoặc so sánh, bị khóa hoặc làm mờ), §13.2 (`highlight.hover`, `highlight.selected`, `highlight.result`, `fade.xref`, `fade.locked`); 07.2 UI-D10 tab Chọn (hiệu ứng tô sáng); biến `HIGHLIGHT`, `SELECTIONPREVIEW`, `XDWGFADECTL`, `LAYLOCKFADECTL`.
- **Đầu ra:**
  - `render/overlay/EntityFlags.h/.cpp`: texture cờ theo slot (đã chọn, kết quả, làm mờ, ẩn tạm, lớp so sánh); shader đọc cờ bằng `texelFetch` (mức nền RND-106); cập nhật theo dải slot.
  - Rê chuột: vẽ lại riêng primitive của entity dưới con trỏ trên overlay; cảnh chính không vẽ lại.
  - Hai kiểu hiệu ứng cho trạng thái đã chọn: đổi màu và dày thêm, hoặc nét đứt **[đề xuất]**; làm mờ là trộn với màu nền theo phần trăm của biến.
  - Làm mờ layer khóa áp cho entity cấp cao nhất trên layer đó **[đề xuất]**.
  - API cho INT-008 (vùng chọn), INT-021 (cô lập, ẩn), FIND, COMPARE.
  - `render/tests/entity_flags_test.cpp`, `render/benchmarks/highlight_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Chọn tất cả 1 triệu entity: cập nhật cờ ≤ 2 ms luồng UI, tô sáng hiện ở khung kế tiếp, 0 batch dựng lại (bộ đếm) **[đề xuất]**.
  2. Rê chuột qua 1.000 entity liên tiếp trên cảnh 10 triệu: mỗi khung ≤ 2 ms GPU, cảnh chính không vẽ lại **[đề xuất]**.
  3. Mỗi trạng thái của 07.1 §9.4 khớp ảnh chuẩn ở chủ đề Tối và Sáng; trạng thái đã chọn khác bình thường cả về nét, không chỉ về màu.
  4. Đổi `XDWGFADECTL`, `LAYLOCKFADECTL`: khung kế tiếp đúng, 0 byte upload hình học.
  5. Đạt DoD-C, DoD-L.

### RND-020 · Nền vùng vẽ, lưới (GRID), biểu tượng UCS, con trỏ chữ thập độ trễ thấp
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-012, RND-109
- **Màn hình:** UI-A07, UI-A13
- **Đầu vào:** 07.1 §9.1 (con trỏ: chờ, chọn điểm, chọn đối tượng, bận; cỡ chữ thập 1–100% màn hình), §9.13 (biểu tượng UCS góc dưới trái hoặc tại gốc), §13.2 (nền Model tối, Layout trắng); 07.2 UI-D10 tab Hiển thị (màu theo ngữ cảnh Model, Layout, Block Editor, xem trước; nền, chữ thập, lưới chính, phụ, trục); token `--c-canvas*`, `--c-cross` trong `design/prototype/css/tokens.css`; biến `GRIDMODE`, `GRIDUNIT`, `GRIDMAJOR`, `GRIDDISPLAY`, `GRIDSTYLE`, `LIMMIN`, `LIMMAX`, `SNAPSTYL`, `CURSORSIZE`, `PICKBOX`, `APERTURE`, `UCSICON`; prototype `design/prototype/index.html#UI-A07` (`js/canvas.js`: `View.prototype.drawGrid`, `View.prototype.draw`).
- **Đầu ra:**
  - `render/canvas/Background.h/.cpp`: màu nền theo ngữ cảnh.
  - `render/canvas/Grid.h/.cpp`, `render/shaders/grid.frag`: lưới tính trong shader, không sinh hình; dạng chấm hoặc đường; đường chính mỗi `GRIDMAJOR`; trục; lưới thích ứng, ô lưới không nhỏ hơn 5 px **[đề xuất]**; giới hạn `LIMMIN`/`LIMMAX`; xoay theo UCS hiện hành (ma trận do INT-015 cấp); lưới isometric (P1).
  - `render/canvas/UcsIcon.h/.cpp`: biểu tượng 2D ở góc hoặc tại gốc theo `UCSICON` (ON, OFF, Noorigin, ORigin); biểu tượng paper space trong layout.
  - `render/canvas/Crosshair.h/.cpp`: chữ thập, ô chọn, ô bắt điểm vẽ trên overlay theo đường độ trễ thấp (RND-109); trạng thái bận có vòng xoay nhỏ và chỉ khi đó mới vẽ liên tục, 10 fps **[đề xuất]**.
- **Tiêu chí nghiệm thu:**
  1. Chữ thập trên cảnh 10 triệu: từ sự kiện chuột tới nộp khung ≤ 3 ms p99 trên máy tầm trung, ≤ 6 ms trên máy thấp **[đề xuất]**.
  2. Lưới: đường lưới nằm đúng bội số `GRIDUNIT`, lệch ≤ 0,5 px ở tọa độ VN-2000; chi phí ≤ 0,3 ms GPU **[đề xuất]**.
  3. Mọi tổ hợp bit của `GRIDDISPLAY` và mọi giá trị `UCSICON` khớp ảnh chuẩn.
  4. `CURSORSIZE` 1–100 và kéo cửa sổ sang màn hình khác DPI: cỡ chữ thập đúng tỉ lệ; ô chọn đúng `PICKBOX` theo pixel logic.
  5. Con trỏ đứng yên và không bận: 0 khung được vẽ.
  6. Đạt DoD-C, DoD-L.

## 11. Paper space, viewport, clip

### RND-118 · Clip vùng tổng quát qua stencil: đa giác, đảo vùng, lồng nhau
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-003, RND-103
- **Đầu vào:** 05 §9.1 (scissor hoặc stencil cho viewport đa giác); 03 §12 (`XCLIP` P0; `IMAGECLIP`, `PDFCLIP` P1), §13 (`VPCLIP`, viewport đa giác hoặc theo đối tượng, P1); mã vùng clip trong primitive RGN-001 (biên do RGN-024, RGN-021 cấp).
- **Đầu ra:**
  - `render/paper/ClipStack.h/.cpp`: vùng clip là đa giác (cạnh cong đã tessellate), tô vào stencil theo even-odd bằng quạt tam giác (không cần tam giác hóa); clip lồng nhau qua bộ đếm mức; đảo vùng (XCLIP đảo, clip ảnh đảo); hình chữ nhật thẳng trục dùng scissor.
  - Khóa batch theo mã clip (RND-003); thứ tự đặt và xóa stencil theo cây clip.
  - Ca ảnh vàng `render/tests/golden/clip/`.
- **Tiêu chí nghiệm thu:**
  1. Lồng 8 mức, xen kẽ clip thường và đảo, đa giác lõm và tự cắt: ảnh khớp tham chiếu QPainter clip path, SSIM ≥ 0,995 **[đề xuất]**.
  2. Đặt clip đa giác 1.000 đỉnh ≤ 0,05 ms GPU **[đề xuất]**.
  3. Biên clip ở tọa độ VN-2000 lệch ≤ 0,5 px.
  4. Đạt DoD-C, DoD-L.

### RND-119 · Hiển thị theo viewport: VP Freeze, ghi đè thuộc tính layer, chú thích theo tỉ lệ
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-003, RND-013
- **Đầu vào:** 03 §10 (VP Freeze P0; ghi đè thuộc tính layer theo viewport P1), §13 (annotation scale P1); tập entity ẩn theo viewport (RGN-024), bảng ghi đè (RGN-002), biểu diễn theo tỉ lệ (RGN-026) do RGN cấp qua RND-117.
- **Đầu ra:**
  - `render/paper/ViewportMasks.h/.cpp`: mặt nạ hiển thị theo slot, mỗi viewport một bit; đến 64 viewport mỗi layout qua hai mặt nạ 32 bit, nhiều hơn thì vẽ thêm lượt **[đề xuất]**. Dùng cho VP Freeze và để mỗi viewport chỉ hiện biểu diễn chú thích đúng tỉ lệ của nó.
  - `render/paper/LayerOverrideTable.h/.cpp`: bảng (layer × viewport) → màu, linetype, lineweight, transparency ghi đè; primitive mang chỉ số layer nguồn và cờ ByLayer cho từng thuộc tính (RGN-001); shader tra bảng.
  - `render/tests/viewport_masks_test.cpp`, ca ảnh vàng.
- **Tiêu chí nghiệm thu:**
  1. Layout 32 viewport, cảnh 1 triệu entity: đổi VP Freeze của 1 layer ≤ 2 ms luồng UI, hiện ở khung kế tiếp, 0 batch dựng lại **[đề xuất]**.
  2. Ghi đè màu, linetype, lineweight, transparency theo viewport: ảnh khớp ảnh dựng bằng cách regen riêng từng viewport, SSIM ≥ 0,995.
  3. Text annotative ở 3 viewport khác tỉ lệ: mỗi viewport hiện đúng biểu diễn của tỉ lệ mình.
  4. Đạt DoD-C, DoD-L.

### RND-014 · Paper space: giấy, bóng, vùng in; viewport ma trận riêng, scissor/stencil cho viewport đa giác; viền viewport kích hoạt
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-117, RND-118, RND-119, QA-012
- **Màn hình:** UI-A10
- **Đầu vào:** 05 §9.1 (viewport vẽ model space với ma trận riêng và clip); 07.1 §9.12 (khổ giấy, vùng in đường đứt, bóng giấy, viền viewport, viewport kích hoạt viền đậm, biểu tượng khóa), §13.2 (`paper.bg`, `paper.printable`, `paper.shadow`); 03 §13; mô tả layout của RND-117; cảnh nhiều viewport của QA-012; prototype `design/prototype/index.html#UI-A10` (`js/canvas.js`: `vpXf`, `vpRect`, `draw`).
- **Đầu ra:**
  - `render/paper/LayoutRenderer.h/.cpp`: giấy, bóng, vùng in; mỗi viewport vẽ cảnh Model dùng chung với ma trận riêng (tâm, tỉ lệ, `VIEWTWIST`); scissor cho viewport chữ nhật, stencil cho viewport đa giác hoặc theo đối tượng (RND-118); culling theo từng viewport (RND-010) chạy song song.
  - Lineweight theo mm giấy trong layout và trong viewport (RND-004); hệ số tỉ lệ linetype theo `PSLTSCALE` truyền cho RND-006.
  - Màu tương phản nền theo từng lượt vẽ: ACI 7 đen trên giấy, trắng trên nền tối.
  - Viền viewport (ẩn khi viewport nằm trên layer tắt hoặc không in), viền đậm cho viewport kích hoạt; trả hình chữ nhật màn hình của viewport để UI đặt biểu tượng khóa.
  - Viewport chia ô trong Model (cấu hình `*Active` của bảng VPORT): mỗi ô một view, ô hiện hành có viền.
  - `render/benchmarks/many_viewports_bench.cpp`, ca ảnh vàng `render/tests/golden/layout/`.
- **Tiêu chí nghiệm thu:**
  1. Cảnh nhiều viewport của QA-012 (16 viewport trên một tờ, Model 1 triệu entity): ≥ 30 fps trên máy tầm trung, ≥ 15 fps trên máy thấp **[đề xuất]**; 64 viewport vẫn đúng hình.
  2. 50 layout tự soạn (viewport chữ nhật, đa giác, theo đường tròn, xoay, khóa, XCLIP lồng trong viewport) khớp ảnh chuẩn; biên viewport lệch ≤ 0,5 px.
  3. Kích hoạt viewport khác: viền đổi ở khung kế tiếp, cảnh Model không dựng lại.
  4. `PSLTSCALE` = 1: dash cùng chiều dài trên giấy ở các viewport khác tỉ lệ, lệch ≤ 0,5 px.
  5. Đạt DoD-C, DoD-H, DoD-L.

## 12. Ảnh raster và PDF underlay

### RND-015 · Ảnh raster và PDF underlay: texture, chia ô, mipmap
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** RND-008, RND-107, RND-118
- **Đầu vào:** 05 §9.1 (xử lý ảnh raster underlay; bản vẽ lớn chia ô, upload qua staging); 03 §12 (`IMAGEATTACH` P0; `IMAGECLIP`, `IMAGEADJUST`, `IMAGEQUALITY`, `IMAGEFRAME`, PDF underlay P1); 07.2 UI-G15 (ảnh: sáng, tương phản, mờ, nền trong suốt; PDF: tương phản, mờ, đơn sắc, chỉnh màu theo nền, layer PDF).
- **Đầu ra:**
  - `render/raster/TileSource.h`: giao diện nguồn ô (ảnh raster; PDF theo mức zoom); RGN-021 và RGN-022 cài.
  - `render/raster/TiledTexture.h/.cpp`: kim tự tháp mức chia ô 512² **[đề xuất]**; chỉ nạp ô thấy được ở mức cần; ô mức thấp hiện trước rồi thay bằng ô nét (tinh chỉnh dần); dựng kim tự tháp trên CPU bằng SIMD (NT-006) ở nền, RND-122 tăng tốc trên GPU.
  - `render/prims/ImagePipeline.h/.cpp`, `render/shaders/image.frag`: sáng, tương phản, mờ (`IMAGEADJUST`); ảnh hai màu vẽ bằng màu của entity, nền trong suốt; kênh alpha của PNG; chất lượng nháp hoặc cao (`IMAGEQUALITY`); PDF đơn sắc và chỉnh màu theo nền tối (công thức trong `render/docs/underlay.md`) **[đề xuất]**; clip qua RND-118; thứ tự vẽ như RND-008.
  - `render/tests/tiled_texture_test.cpp`, `render/benchmarks/raster_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Ảnh 20.000 × 20.000 px: khung đầu có ảnh mức thấp ≤ 1 s; pan, zoom ≥ 60 fps trên máy tầm trung; VRAM cho ảnh ≤ 256 MB **[đề xuất]**.
  2. Sáng, tương phản, mờ, hai màu, trong suốt: mỗi kênh lệch ≤ 2/255 so với tham chiếu CPU.
  3. Ranh giới giữa các ô và giữa các mức không lộ đường nối (so cột điểm ảnh hai bên ranh giới).
  4. Nguồn ô giả lập (PDF) trả chậm 200 ms: vùng vẽ không khựng, ô mờ hiện trước.
  5. Đạt DoD-C, DoD-H, DoD-L.

## 13. GPU compute

### RND-120 · Khung GPU compute và đường dự phòng CPU
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** RND-102, RND-106, NT-006
- **Đầu vào:** 05 §9.1 (GPU compute khi backend hỗ trợ: D3D11/12, Vulkan, Metal, OpenGL 4.3+; luôn có đường dự phòng trên CPU); `QRhiComputePipeline`, tính năng `QRhi::Compute`.
- **Đầu ra:**
  - `render/compute/ComputeKernel.h/.cpp`: khai báo kernel một lần với hai cài đặt cùng đặc tả: shader compute (`.comp` qua RND-101) và hàm C++ (SIMD NT-006, chạy trên NT-005); chọn đường theo `DeviceCaps`, ép được bằng cờ kiểm thử.
  - Bộ đệm storage, rào bộ nhớ, đọc lại bất đồng bộ không chặn khung.
  - `render/tests/compute_kernel_test.cpp`: kernel mẫu (thu gọn mảng) chạy cả hai đường.
- **Tiêu chí nghiệm thu:**
  1. Kernel mẫu: hai đường cho kết quả trùng từng bit (dữ liệu nguyên) trên mọi backend có compute.
  2. Máy không có compute (GL 3.3, GLES 3.0): tự dùng đường CPU, không lỗi, log một lần.
  3. Đọc lại kết quả không làm khung hiện tại chờ GPU (đo bằng RND-108).
  4. Đạt DoD-C, DoD-H.

### RND-121 · Culling và chọn LOD trên GPU cho hàng triệu đối tượng
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** RND-010, RND-120
- **Đầu vào:** 05 §9.1 (culling và chọn LOD cho hàng triệu đối tượng ngay trên GPU); `ChunkCuller`, `LodPolicy` của RND-010 làm đường CPU.
- **Đầu ra:**
  - `render/compute/GpuCull.h/.cpp`, `render/shaders/cull.comp`: culling và mức LOD theo từng primitive trong các chunk chỉ thấy một phần; ghi danh sách thu gọn và tham số vẽ. Dùng draw gián tiếp nếu bản QRhi đang ghim hỗ trợ; nếu không, ghi cờ hiển thị để vertex shader bỏ primitive **[đề xuất]**.
  - Đường CPU là culling theo chunk của RND-010.
- **Tiêu chí nghiệm thu:**
  1. Kết quả GPU chứa mọi primitive mà kiểm tra chính xác trên CPU giữ lại; phần thừa ≤ 0,1% **[đề xuất]**.
  2. Cảnh 10 triệu, zoom một phần: culling ≤ 0,3 ms GPU trên máy mạnh; thời gian khung giảm ≥ 20% so với chỉ culling theo chunk **[đề xuất]**.
  3. Máy không compute dùng RND-010; ảnh trùng từng điểm ảnh với ảnh khi bật GPU culling (cùng GPU).
  4. Đạt DoD-C, DoD-H, DoD-L.

### RND-122 · Xử lý ảnh và so sánh ảnh trên GPU
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** RND-015, RND-103, RND-120
- **Đầu vào:** 05 §9.1 (xử lý ảnh raster underlay, so sánh ảnh khi COMPARE); `ImageCompare` (RND-103) và dựng kim tự tháp trên CPU (RND-015) làm đường dự phòng.
- **Đầu ra:**
  - `render/compute/ImageKernels.h/.cpp`, `render/shaders/mip_downsample.comp`, `image_diff.comp`, `ssim.comp`: dựng mức mip cho ảnh lớn, đổi định dạng (ảnh hai màu → alpha), sai khác theo ô, SSIM.
  - API so hai ảnh: bản đồ chênh lệch, danh sách ô thay đổi; dùng cho ảnh chụp của COMPARE và cho so sánh hàng loạt của QA-005.
- **Tiêu chí nghiệm thu:**
  1. GPU khớp đường CPU: mip lệch ≤ 1/255 mỗi kênh; SSIM lệch ≤ 1e-4; danh sách ô thay đổi trùng nhau.
  2. Ảnh 20.000 × 20.000 px: dựng kim tự tháp ≤ 0,5 s trên máy tầm trung; SSIM của hai ảnh 8K ≤ 50 ms **[đề xuất]**.
  3. Máy không compute: đường CPU cho cùng kết quả như tiêu chí 1.
  4. Đạt DoD-C, DoD-H.

### RND-016 · GPU compute (culling/LOD, nét hatch/linetype dày, xử lý ảnh, so sánh ảnh) kèm đường dự phòng trên CPU
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** RND-116, RND-120, RND-121, RND-122, QA-012
- **Đầu vào:** API của RND-116, RND-120, RND-121, RND-122; 05 §9.1; cảnh QA-012.
- **Đầu ra:**
  - `render/compute/ComputePolicy.h/.cpp`: bật, tắt từng nhóm (culling/LOD, hatch thủ tục, xử lý ảnh, so sánh ảnh) theo khả năng thiết bị và mức chất lượng (RND-018).
  - Kiểm thử chéo `render/tests/compute_parity_it.cpp`: mọi cảnh QA-012 chạy với đường GPU bật và tắt.
- **Tiêu chí nghiệm thu:**
  1. Mọi cảnh QA-012: ảnh khi bật và khi tắt đường GPU khớp ngưỡng RND-103 trên mọi backend.
  2. Máy mạnh: cảnh 10 triệu và cảnh hatch dày nhanh hơn ≥ 20% khi bật **[đề xuất]**; máy thấp không bao giờ chậm hơn (đường nào chậm hơn trong benchmark thì chính sách tự tắt).
  3. Đạt DoD-C, DoD-H.

## 14. Bộ nhớ GPU, tự thích ứng, cache shader

### RND-017 · Quản lý VRAM: ngân sách theo dung lượng, chia ô bản vẽ khổng lồ, loại bỏ LRU, bộ nhớ hợp nhất trên Apple Silicon
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-015, RND-107, RND-117, QA-012
- **Đầu vào:** 05 §9.1 (đọc VRAM, đặt ngân sách; bản vẽ khổng lồ chia ô, chỉ giữ trên GPU phần đang thấy, loại bỏ theo LRU; Apple Silicon tránh sao chép), §1.1 (bộ nhớ ≤ 1,2 GB cho DWG 100 MB); `DeviceCaps` (RND-102: VRAM, ngân sách, bộ nhớ hợp nhất); cảnh 10 triệu của QA-012.
- **Đầu ra:**
  - `render/gpu/VramBudget.h/.cpp`: ngân sách = 70% VRAM riêng; GPU tích hợp lấy min(ngân sách hệ điều hành báo, 25% RAM) **[đề xuất]**. Theo dõi mọi buffer, texture, render target theo bản vẽ, không gian, chunk, ô ảnh.
  - Vượt ngân sách thì đẩy ra theo LRU (khung dùng cuối): chunk ngoài màn hình, ô ảnh, glyph ít dùng, bản vẽ không hiện hành; nạp lại từ bản CPU hoặc cache regen khi thấy lại.
  - Bộ nhớ hợp nhất (Apple Silicon, GPU tích hợp báo UMA): ghi thẳng vào bộ nhớ dùng chung, không giữ bản sao CPU thứ hai.
  - API thống kê cho phần chẩn đoán của UI-L10 (đã dùng / ngân sách).
  - `render/tests/vram_budget_test.cpp`, `render/benchmarks/huge_drawing_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Cảnh 10 triệu với ngân sách ép 512 MB: không lỗi cấp phát; zoom một phần ≥ 30 fps trên máy tầm trung; đỉnh bộ nhớ GPU ≤ ngân sách + 5% **[đề xuất]**.
  2. Pan qua toàn bản vẽ khổng lồ: ô thiếu hiện lại trong ≤ 3 khung, vùng giữa màn hình trước **[đề xuất]**.
  3. Apple Silicon: tổng RAM và bộ nhớ GPU cho cảnh 1 triệu ≤ 1,1 lần cỡ dữ liệu cảnh **[đề xuất]**.
  4. Số liệu thống kê lệch ≤ 5% so với số đo của công cụ GPU của hãng (PIX, Xcode GPU tools).
  5. Đạt DoD-C, DoD-H.

### RND-018 · Tự thích ứng theo hồ sơ phần cứng: backend, MSAA, mật độ tessellation, ngưỡng LOD, số luồng nền; chế độ pin
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-019, RND-102, RND-110, NT-005, NT-008, NT-010
- **Đầu vào:** 05 §9.5 (chọn backend và mức chất lượng theo cấu hình đo lúc khởi động; chạy pin thì giảm luồng nền và tần số vẽ), §1.1 (đúng trên 100% máy trong ma trận kiểm thử); DoD-H mục 7 (chính sách pin dùng chung); 07.3 UI-L10 (mức chất lượng Tự động / Cao / Cân bằng / Tiết kiệm; giới hạn khung hình khi chạy pin; mức LOD); prototype `design/prototype/index.html#UI-L10` (`js/screens2.js`, `SC.graphics`: giới hạn 30 khung hình/giây khi chạy pin, LOD 1–5).
- **Đầu ra:**
  - `render/adapt/QualityProfile.h/.cpp`: hàm thuần (hồ sơ NT-008, `DeviceCaps`, điểm TRIONBENCH nếu có, thiết lập người dùng) → backend ưu tiên (D3D12, Vulkan, Metal trên GPU đời mới; D3D11, OpenGL cho máy cũ), số mẫu MSAA, làm mượt nét, sai số tessellation (tham số truyền cho GEO-016 qua regen), ngưỡng và độ lệch LOD, trần khung hình, số luồng nền. Bảng mức Cao / Cân bằng / Tiết kiệm trong `render/adapt/quality_presets.json`.
  - `render/adapt/gpu_blocklist.json`: hãng, thiết bị, khoảng phiên bản driver → backend cần tránh **[đề xuất]**.
  - `render/adapt/PowerPolicy.h/.cpp`: theo dõi nguồn điện (NT-008). Chạy pin: trần 30 fps (prototype UI-L10), giới hạn luồng nền qua API của NT-005 còn ≤ 50% số nhân **[đề xuất]**, không tinh chỉnh dần lên mức cao nhất. Cắm điện thì trả lại.
  - Lưu thiết lập qua NT-010 (phạm vi máy).
  - `render/tests/quality_profile_test.cpp`: ma trận ≥ 20 hồ sơ phần cứng ghi sẵn với bảng kỳ vọng **[đề xuất]**.
- **Tiêu chí nghiệm thu:**
  1. Ma trận hồ sơ ghi sẵn và 4 máy tham chiếu: backend và mức chất lượng chọn khớp bảng kỳ vọng 100% (05 §1.1).
  2. Rút sạc trên laptop tham chiếu: trần khung hình và giới hạn luồng nền có hiệu lực ≤ 1 s sau khi hệ điều hành báo; cắm lại thì trả về **[đề xuất]**.
  3. Chạy pin, pan liên tục 60 s: ≤ 30 khung mỗi giây; số luồng nền hoạt động không vượt giới hạn (bộ đếm NT-004).
  4. Đổi mức chất lượng lúc chạy có hiệu lực ở khung kế tiếp (đổi backend đi qua RND-001).
  5. Đạt DoD-C, DoD-H.

### RND-123 · Thiết lập đồ họa và lệnh GRAPHICSCONFIG
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-001, RND-018, INT-001, INT-002, INT-003, NT-010
- **Lệnh:** GRAPHICSCONFIG
- **Màn hình:** UI-L10
- **Đầu vào:** 03 §21 (`GRAPHICSCONFIG`: tăng tốc phần cứng, khử răng cưa, hiển thị lineweight/transparency, P0); 07.3 UI-L10 (backend; GPU đang dùng và VRAM; mức chất lượng; MSAA; làm mượt nét; giới hạn khung hình khi chạy pin; mức LOD; hiện FPS; chạy kiểm tra hiệu năng; đặt lại mặc định; chẩn đoán: phiên bản driver, lỗi đồ họa đã gặp, sao chép báo cáo); prototype `design/prototype/index.html#UI-L10` (`js/screens2.js`, `SC.graphics`); 07.2 UI-D10 tab Hệ thống (mở UI-L10).
- **Đầu ra:**
  - `render/adapt/GraphicsSettings.h/.cpp`: mô hình thiết lập có kiểu (giá trị, mặc định, giá trị Tự động đang dùng, các lựa chọn có trên máy), áp dụng lúc chạy; đổi backend hoặc GPU đi qua đường tạo lại thiết bị của RND-001, không khởi động lại.
  - Báo cáo chẩn đoán dạng văn bản: phiên bản ứng dụng, backend, GPU, driver, VRAM đã dùng/ngân sách (RND-017 khi có), số lỗi đồ họa trong 30 ngày (RND-105).
  - `render/commands/GraphicsConfigCommand.cpp`:
    - chạy tương tác và `FILEDIA` = 1: mở UI-L10;
    - `-GRAPHICSCONFIG`, script `.scr`, `(command ...)`, `trion-console` hoặc `FILEDIA` = 0: prompt dòng lệnh `[Backend/GPU/Chất lượng/MSAA/Làm mượt nét/Pin/LOD/FPS/Đặt lại/Thoát] <Thoát>`, từ khóa toàn cục tiếng Anh (`_Backend`, `_Gpu`, `_Quality`, `_Msaa`, `_Smooth`, `_Battery`, `_Lod`, `_Fps`, `_Reset`, `_eXit`) **[đề xuất]**;
    - Backend: `[Tự động/D3D12/D3D11/Vulkan/Metal/OpenGL]`, chỉ nhận backend có trên hệ điều hành; GPU: danh sách adapter đánh số hoặc Tự động; Chất lượng: `[Tự động/Cao/Cân bằng/Tiết kiệm]`; MSAA, Làm mượt nét, Pin, FPS: `[Bật/Tắt]`; LOD: số nguyên 1–5; giá trị sai thì báo và hỏi lại (INT-003); prompt lặp đến khi Thoát hoặc Enter;
    - thiết lập theo máy, không ghi bản vẽ, không tạo mark undo; có ở mọi phiên bản, kể cả Viewer.
  - `docs/specs/commands/GRAPHICSCONFIG.md`; `render/tests/graphics_settings_test.cpp`; kịch bản `.scr` kiểm thử.
- **Tiêu chí nghiệm thu:**
  1. Mọi mục của UI-L10 đọc, ghi, áp dụng được qua API; giá trị lưu qua NT-010, còn nguyên sau khi khởi động lại.
  2. Đổi MSAA, làm mượt nét, LOD, FPS: có hiệu lực ở khung kế tiếp. Đổi backend hoặc GPU: vùng vẽ hiện lại ≤ 2 s, bản vẽ giữ nguyên view, vùng chọn và lệnh đang chạy **[đề xuất]**.
  3. Cùng kết quả khi gọi từ bàn phím, `.scr` và `(command "_-GRAPHICSCONFIG" ...)`; mọi nhánh lỗi có ca kiểm thử.
  4. Đặt lại mặc định đưa mọi mục về Tự động hoặc giá trị mặc định.
  5. Đạt DoD-C, DoD-L.

### RND-022 · Cache shader và pipeline trên đĩa
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** RND-102, NT-011
- **Đầu vào:** 05 §9.4 (cache trên đĩa: shader và pipeline đã biên dịch), §1.1 (khởi động ấm ≤ 1,5 s); cờ `EnablePipelineCacheDataSave` và hàm `pipelineCacheData()`, `setPipelineCacheData()` của QRhi; ghi an toàn NT-011.
- **Đầu ra:**
  - `render/rhi/PipelineCache.h/.cpp`: khóa = (backend, hãng và mã thiết bị GPU, phiên bản driver, bản build ứng dụng); tệp trong thư mục cache người dùng, header có phiên bản và hash nội dung; trần 64 MB **[đề xuất]**; ghi khi thoát và sau mỗi 50 pipeline mới, ghi an toàn qua NT-011.
  - Tệp hỏng, khác driver hoặc khác bản build: bỏ qua, xóa, log một dòng.
  - Fuzz target `render/fuzz/pipeline_cache_fuzz.cpp` cho phần đọc header.
- **Tiêu chí nghiệm thu:**
  1. Lần chạy thứ hai: tổng thời gian tạo pipeline giảm ≥ 70% trên D3D11, D3D12, Vulkan, Metal; khung hình đầu của vùng vẽ ≤ 200 ms sau khi cửa sổ hiện **[đề xuất]**.
  2. Tệp cache bị cắt cụt, đảo bit, hoặc lấy từ driver khác: không crash, tự dựng lại.
  3. Fuzz 1 giờ không lỗi.
  4. Đạt DoD-C.

## 15. Kết xuất ngoài màn hình

### RND-021 · Render ngoài màn hình: thumbnail, xem trước in, xuất ảnh, ảnh cho so sánh
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** RND-014, RND-103, RND-117
- **Màn hình:** UI-H03, UI-H09, UI-A09
- **Đầu vào:** README §4.2 (thumbnail: DWG-026 đọc/ghi, RND-021 render ngoài màn hình, UI chỉ hiển thị); 07.2 UI-H03 (xem trước in: tờ giấy, vùng in, độ dày nét theo CTB), UI-H09 (PNG, JPG, TIF, BMP; DPI hoặc cỡ pixel; vùng màn hình, toàn bộ, cửa sổ, layout; nền trong suốt, trắng hoặc màu nền vùng vẽ; độ dày nét; khử răng cưa); 07.1 §5.6 và §9.12 (thumbnail khi rê tab tệp, tab layout); 07.2 UI-I07 (ảnh xem trước A/B, xuất ảnh chụp).
- **Đầu ra:**
  - `render/offscreen/OffscreenRenderer.h/.cpp`: vẽ cảnh (Model, layout, cảnh xem trước in) ra ảnh theo vùng, cỡ pixel hoặc DPI, nền, lineweight theo DPI, khử răng cưa; chạy không cần cửa sổ (cho `trion-console`); không có GPU thì dùng GPU phần mềm (WARP, llvmpipe).
  - Ảnh lớn hơn cỡ texture tối đa: chia ô, ghép tuần tự vào bộ mã hóa ảnh, không giữ cả ảnh trong RAM khi xuất.
  - Chế độ cảnh xem trước in cho PLT-006: primitive đã áp CTB, lineweight theo mm giấy, đầu mút và mối nối theo CTB (RND-004); hiện trên màn hình bằng cùng renderer, có pan, zoom.
  - Thumbnail cho DWG-026, tab tệp, tab layout, ảnh A/B của COMPARE; có dùng LOD.
  - `render/tests/offscreen_test.cpp`, `render/benchmarks/offscreen_bench.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Ảnh chia ô trùng từng điểm ảnh với ảnh vẽ một lần ở cỡ cho phép (cùng GPU), không lộ đường ghép.
  2. PNG 8.000 × 6.000 px của cảnh 1 triệu ≤ 1,5 s trên máy tầm trung; xuất 20.000 × 14.000 px làm RAM tăng ≤ 300 MB **[đề xuất]**.
  3. Thumbnail 256 px của bản vẽ 1 triệu entity ≤ 50 ms **[đề xuất]**.
  4. `trion-console` trên runner không có GPU (WARP, llvmpipe): ảnh đạt ngưỡng RND-103.
  5. Đọc lại ảnh bất đồng bộ: luồng UI không chặn quá 2 ms **[đề xuất]**.
  6. Đạt DoD-C, DoD-H.

## 16. TRIONBENCH và nghiệm thu mốc V

### RND-024 · Engine TRIONBENCH: bài đo chuẩn, gợi ý cấu hình, kết quả ẩn danh khi đồng ý
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** RND-018, RND-021, RND-123, QA-012, NT-005, NT-006, NT-011, NT-013, PLF-008, INT-001, INT-002, INT-003
- **Lệnh:** TRIONBENCH
- **Màn hình:** UI-L11
- **Đầu vào:** 05 §9.5 (TRIONBENCH chạy bài đo chuẩn, gợi ý cấu hình, gửi kết quả ẩn danh nếu người dùng đồng ý); 07.3 §0 (tên lệnh), UI-L11 (các bước: CPU một luồng, CPU đa luồng, SIMD, cảnh GPU 1 triệu và 10 triệu đối tượng, hatch dày, chữ dày, đọc đĩa; điểm từng mục; so với máy tham chiếu; cấu hình đề xuất có nút Áp dụng; lịch sử; ô chọn gửi ẩn danh); prototype `design/prototype/index.html#UI-L11` (`js/screens2.js`, `SC.bench`: khoảng 1 phút; 3 mốc máy văn phòng cũ, tầm trung, máy trạm); bộ sinh cảnh tất định QA-012; telemetry có đồng ý PLF-008.
- **Đầu ra:**
  - `render/trionbench/Bench.h/.cpp`: 8 bài theo UI-L11, chạy là tác vụ nền NT-013 (tiến trình theo bài, hủy ≤ 50 ms); bài GPU vẽ ngoài màn hình (RND-021) với cảnh QA-012; không đủ VRAM thì bỏ bài đó và ghi lý do.
  - `render/trionbench/reference.json`: điểm của 3 mốc máy, sinh tự động từ job CI chạy trên máy tham chiếu (README §3.1).
  - Gợi ý cấu hình qua `QualityProfile` (RND-018); "Áp dụng" ghi qua `GraphicsSettings` (RND-123).
  - Lịch sử 20 lần đo gần nhất: JSON trong thư mục dữ liệu người dùng, ghi an toàn (NT-011); tệp hỏng thì bỏ qua.
  - Gửi ẩn danh qua PLF-008 chỉ khi người dùng chọn: cấu hình phần cứng, điểm, phiên bản ứng dụng; không đường dẫn, không tên máy.
  - `render/commands/TrionBenchCommand.cpp`:
    - chạy tương tác và `FILEDIA` = 1: mở UI-L11, chưa tự chạy;
    - `-TRIONBENCH`, script `.scr`, `(command ...)`, `trion-console` hoặc `FILEDIA` = 0: prompt `Chọn bài đo [Tất cả/CPU/GPU/Đĩa] <Tất cả>` (từ khóa toàn cục `_All`, `_Cpu`, `_Gpu`, `_Disk`) **[đề xuất]**, chạy, in bảng điểm lên dòng lệnh, ghi JSON và in đường dẫn; dạng dòng lệnh không bao giờ gửi kết quả **[đề xuất]**;
    - Esc hủy; không ghi bản vẽ, không tạo mark undo.
  - `docs/specs/commands/TRIONBENCH.md`; `render/tests/trionbench_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Toàn bộ bài ≤ 90 s trên máy thấp, ≤ 60 s trên máy tầm trung **[đề xuất]**.
  2. Lặp 5 lần trên cùng máy: mỗi điểm dao động ≤ 5% **[đề xuất]**.
  3. Cấu hình đề xuất trùng kết quả `QualityProfile` cho cùng hồ sơ; "Áp dụng" có hiệu lực ngay.
  4. Không chọn gửi: 0 yêu cầu mạng (kiểm bằng máy chủ giả). Có chọn: gói gửi đúng lược đồ, không chứa đường dẫn hay tên máy.
  5. Hủy giữa chừng: dừng ≤ 50 ms, không để lại tệp tạm.
  6. Bàn phím, `.scr` và `(command "_-TRIONBENCH" ...)` cho cùng định dạng kết quả.
  7. Đạt DoD-C, DoD-H.

### RND-124 · Nghiệm thu ngân sách đồ họa mốc V
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** RND-007, RND-009, RND-011, RND-012, RND-014, RND-016, RND-017, RND-018, RND-021, RND-025, QA-004, QA-005, PLT-006
- **Đầu vào:** 05 §1.1 (pan/zoom 1 triệu entity ≥ 60 fps tầm trung, ≥ 30 fps máy thấp; 10 triệu ≥ 30 fps với GPU rời; jig ≤ 8 ms/khung; rảnh ~0%; tự chọn backend và mức chất lượng đúng 100%), §1.2 (hiển thị và in so với bản in DWG TrueView); README §6 (G1: SSIM ≥ 0,97 trên ≥ 95% tờ); bộ cảnh QA-012; kho 10.000 tệp (QA-001); đối chứng DWG TrueView (QA-004); so sánh ảnh QA-005; cảnh xem trước in PLT-006.
- **Đầu ra:** `render/benchmarks/v_budget_suite.json` (cảnh, máy, ngưỡng); `render/docs/nghiem-thu-v.md` sinh tự động từ kết quả đêm.
- **Tiêu chí nghiệm thu:**
  1. Pan/zoom p99: 1 triệu entity ≤ 16,6 ms (tầm trung), ≤ 33 ms (thấp); 10 triệu ≤ 33 ms (máy mạnh, GPU rời).
  2. Cảnh hatch dày, text dày, block lồng, nhiều viewport, VN-2000 của QA-012 đạt ngưỡng ghi trong RND-116, RND-007, RND-009, RND-014, RND-011.
  3. Jig 10.000 primitive ≤ 8 ms/khung (tầm trung), ≤ 16 ms (thấp); rảnh 60 s: 0 khung.
  4. Ảnh xem trước in (RND-021 + PLT-006) so với PDF in từ DWG TrueView (QA-004, QA-005) trên các tờ của kho 10.000 tệp: SSIM ≥ 0,97 trên ≥ 95% tờ (G1). Theo dõi hằng đêm tới ngưỡng 1.0: ≥ 0,98 trên ≥ 97% tờ (05 §1.2).
  5. Backend và mức chất lượng chọn đúng 100% trên ma trận máy (RND-018).
  6. Mọi chỉ số đạt 7 đêm liên tiếp.
  7. Đạt DoD-C, DoD-H, DoD-L.

## 17. So sánh bản vẽ

### RND-125 · Chế độ hiển thị so sánh bản vẽ: chồng hình và hai khung đồng bộ
- **Mốc:** VP · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** RND-012, RND-013, RND-014, RND-021, RND-117
- **Màn hình:** UI-I07
- **Đầu vào:** 03 §17 (`COMPARE`, P1); 07.2 UI-I07 (chồng hình theo màu chỉ có ở A / chỉ có ở B / giống nhau kèm ký hiệu; hai khung cạnh nhau đồng bộ zoom/pan; bật/tắt từng nhóm; ◀ ▶ tự zoom tới; đám mây quanh vùng thay đổi; xuất ảnh chụp); 07.1 §9.4 (tô sáng kết quả so sánh), §13.2 (`compare.onlyA`, `compare.onlyB`, `compare.common`), §13.3 (so sánh có cả ký hiệu, không chỉ màu); token `--c-cmp-a`, `--c-cmp-b`, `--c-cmp-common` trong `design/prototype/css/tokens.css`; prototype `design/prototype/index.html#UI-I07` (`js/screens3.js`: `SC.compare`, `TC.cmpDraw`, `cloud`, `pill`).
- **Đầu ra:**
  - `render/compare/CompareView.h/.cpp`: cảnh A (bản vẽ hiện hành) và cảnh B (bản vẽ thứ hai, chỉ đọc) trong một vùng vẽ; lớp so sánh theo slot (cờ RND-013) do engine của lệnh COMPARE cấp: giống nhau, chỉ có ở A, chỉ có ở B, đã sửa.
  - Chồng hình: nhóm giống nhau vẽ màu `compare.common`, nét 1 px; nhóm chỉ có ở A, chỉ có ở B vẽ màu riêng, nét 2 px; hatch đổi màu theo nhóm; bật/tắt từng nhóm không dựng lại batch.
  - Hai khung: hai view dùng chung camera, chia đôi vùng vẽ bằng scissor (RND-014), có đường phân cách.
  - Overlay (RND-012): đám mây hoặc hình chữ nhật quanh hộp bao của thay đổi, nhãn ký hiệu − + ~ kèm số thứ tự; thay đổi đang xem vẽ bằng màu nhấn.
  - Ảnh chụp đúng chế độ đang xem qua RND-021.
  - `render/tests/compare_view_test.cpp`, ca ảnh vàng `render/tests/golden/compare/`.
- **Tiêu chí nghiệm thu:**
  1. Chồng hình hai bản vẽ 1 triệu entity: pan, zoom ≥ 30 fps trên máy tầm trung **[đề xuất]**.
  2. Bật/tắt một nhóm hoặc đổi chế độ xem: khung kế tiếp đúng, 0 batch dựng lại.
  3. Hai khung: cùng một điểm bản vẽ nằm cùng vị trí tương đối trong hai nửa, lệch ≤ 0,5 px.
  4. Chuyển ảnh sang thang xám vẫn phân biệt được ba nhóm nhờ ký hiệu và độ dày nét (07.1 §13.3).
  5. Đạt DoD-C, DoD-L.

## 18. Xem 3D

### RND-126 · Camera 3D và kiểu hiển thị: 2D Wireframe, Wireframe, Hidden, Shaded, Realistic
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** RND-008, RND-020, RND-117, DOC-007, INT-001, INT-002, INT-003, QA-001
- **Lệnh:** VSCURRENT, SHADEMODE, HIDE
- **Màn hình:** UI-M01, UI-A05
- **Đầu vào:** 03 §21 (visual style 2D Wireframe, Wireframe, Hidden, Shaded, Realistic; `VSCURRENT`, `HIDE`, `SHADEMODE`, P1; Conceptual, Shades of Gray, Sketchy, X-ray là P3); 07.1 §7.6 (dropdown Kiểu hiển thị: P1, mốc V chỉ xem); 07.3 UI-M01 (ribbon Viewer có Kiểu hiển thị), UI-M03 (chế độ chỉ xem, không sửa tệp), §8 (bản 1.0 chỉ có phần xem 3D); tham số view của VPORT và VIEWPORT (hướng nhìn, điểm đích, twist, phối cảnh, kiểu hiển thị) qua RND-117; primitive của 3DFACE, polyface mesh, polygon mesh, polyline 3D do RGN-023 sinh.
- **Đầu ra:**
  - `render/view3d/Camera.h/.cpp`: ma trận nhìn tổng quát (hướng nhìn, điểm đích, twist, chiếu song song hoặc phối cảnh theo độ dài ống kính), RTC như 2D; view mặt bằng dùng lại đường 2D.
  - `render/view3d/VisualStyles.h/.cpp`:
    - 2D Wireframe: đường 2D hiện có;
    - Wireframe: nét 3D có depth, không khử khuất;
    - Hidden: mặt chỉ ghi depth, nét vẽ sau với polygon offset;
    - Shaded: mặt tô theo ánh sáng mặc định hai nguồn, tùy chọn vẽ cạnh;
    - Realistic: như Shaded, màu mặt theo entity **[đề xuất]**;
    - 3DFACE tôn trọng cạnh ẩn; nét và cung không song song mặt nhìn thì regen tessellate theo tham số view (RGN-001).
  - Biểu tượng UCS kiểu 3D khi view không phải mặt bằng (mở rộng RND-020).
  - `render/commands/VisualStyleCommands.cpp`:
    - VSCURRENT: `[2dwireframe/Wireframe/Hidden/Realistic/Shaded/shaded with Edges/Other/Current]` theo AutoCAD, giữ từ khóa toàn cục; Conceptual, shades of Gray, SKetchy, X-ray được nhận nhưng báo chưa hỗ trợ và giữ kiểu hiện tại **[đề xuất]**; áp cho viewport hiện hành (ô hiện hành trong Model, viewport đang kích hoạt trong layout); tạo đối tượng visual style mặc định khi bản vẽ thiếu.
    - SHADEMODE: cùng hành vi VSCURRENT. `-SHADEMODE` nhận thêm tùy chọn cũ (2D wireframe, 3D wireframe, Hidden, Flat, Gouraud, fLat+edges, gOuraud+edges), ánh xạ sang kiểu tương ứng.
    - HIDE: không prompt; khử khuất tạm cho viewport hiện hành đến lần REGEN kế tiếp.
    - Bản Pro ghi kiểu hiển thị của viewport qua DOC-007, một mark undo mỗi lần đổi. Viewer chỉ đổi hiển thị, không ghi bản vẽ (UI-M03).
  - `docs/specs/commands/VSCURRENT.md`, `docs/specs/commands/SHADEMODE.md`, `docs/specs/commands/HIDE.md`; ca ảnh vàng `render/tests/golden/view3d/`.
- **Tiêu chí nghiệm thu:**
  1. Cảnh khối lập phương, mặt cắt nhau, mặt đồng phẳng: Hidden khớp tham chiếu z-buffer tính trên CPU, ≤ 0,5% điểm ảnh khác **[đề xuất]**.
  2. 1 triệu tam giác ở kiểu Shaded: xoay view ≥ 60 fps trên máy tầm trung, ≥ 30 fps trên máy thấp **[đề xuất]**.
  3. Đổi kiểu hiển thị: ≤ 100 ms tới khung đầu có nội dung (07.1 §1.3).
  4. Tệp trong kho QA-001 lưu ở view 3D với kiểu Shaded hoặc Hidden: Viewer mở lên hiện đúng kiểu đã lưu; đổi kiểu trong Viewer không sửa tệp (hash tệp không đổi).
  5. VSCURRENT, SHADEMODE, HIDE cho cùng kết quả từ bàn phím, `.scr`, `(command ...)`; bản Pro tạo đúng một mark undo mỗi lần đổi.
  6. Đạt DoD-C, DoD-L.

### RND-127 · Xoay 3D: 3DORBIT, 3DFORBIT, 3DCORBIT, VPOINT
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** RND-126, INT-002, INT-003, INT-008, INT-016
- **Lệnh:** 3DORBIT, 3DFORBIT, 3DCORBIT, VPOINT
- **Màn hình:** UI-A05
- **Đầu vào:** 03 §21 (`3DORBIT`, `3DFORBIT`, `3DCORBIT` P1; `VPOINT` P1); 07.1 §7.6 (Xoay 3D trong tab Hiển thị), §9.2 (gộp zoom và pan liên tiếp thành một bước hoàn tác); 07.2 UI-D10 tab Mô hình 3D (kiểu hiển thị khi xoay 3D); lịch sử view và gộp hoàn tác của INT-016; vùng chọn PICKFIRST của INT-008; biến `WORLDVIEW`.
- **Đầu ra:**
  - `render/view3d/OrbitController.h/.cpp`: hàm thuần (trạng thái camera, sự kiện chuột) → camera mới:
    - xoay ràng buộc (3DORBIT): kéo ngang quay quanh trục Z thế giới qua điểm đích, kéo dọc quay quanh trục X của màn hình;
    - xoay tự do (3DFORBIT): vòng tròn có 4 tay nắm; trong vòng thì xoay tự do, ngoài vòng thì lăn quanh trục vuông góc màn hình, tay nắm trái/phải quay quanh trục đứng, trên/dưới quay quanh trục ngang;
    - xoay liên tục (3DCORBIT): kéo rồi thả thì giữ vận tốc góc, bấm để đổi hướng; chỉ khi đang quay mới vẽ liên tục.
  - Điểm đích là tâm hộp bao của đối tượng đã chọn, hoặc tâm view. Có đối tượng chọn trước (PICKFIRST) thì trong lúc xoay chỉ hiện các đối tượng đó, như AutoCAD. Trong lúc xoay dùng kiểu hiển thị đặt ở UI-D10.
  - `render/commands/OrbitCommands.cpp`:
    - 3DORBIT, 3DFORBIT, 3DCORBIT: prompt "Nhấn Esc hoặc Enter để thoát."; gọi trong suốt được (`'3DORBIT`…); Shift + giữ chuột giữa vào xoay ràng buộc tạm thời; từ `.scr` hoặc `(command ...)` thì Enter kế tiếp kết thúc lệnh, không treo.
    - VPOINT: `Chỉ định điểm nhìn hoặc [Xoay] <hiện la bàn và ba trục>`; nhập vector hướng nhìn; Xoay hỏi góc trong mặt XY tính từ trục X, rồi góc so với mặt XY; Enter hiện la bàn và ba trục để chọn bằng chuột; tôn trọng `WORLDVIEW`; trong Paper space thì báo lệnh không dùng được, như AutoCAD.
    - Mỗi lần đổi view là một mục trong lịch sử view (ZOOM Previous trả lại được) và một bước hoàn tác view, gộp theo thiết lập của INT-016.
  - `docs/specs/commands/3DORBIT.md`, `docs/specs/commands/3DFORBIT.md`, `docs/specs/commands/3DCORBIT.md`, `docs/specs/commands/VPOINT.md`; `render/tests/orbit_controller_test.cpp`.
- **Tiêu chí nghiệm thu:**
  1. Xoay trên cảnh 1 triệu entity: ≥ 60 fps trên máy tầm trung, ≥ 30 fps trên máy thấp (có LOD).
  2. 10.000 chuỗi sự kiện ngẫu nhiên: camera luôn hợp lệ (ma trận trực chuẩn, không NaN, điểm đích giữ nguyên khi xoay ràng buộc); kết quả tất định.
  3. `VPOINT` với `1,-1,1` cho hướng nhìn isometric Đông Nam; ZOOM Previous sau 3DORBIT trả đúng view trước đó.
  4. 3DCORBIT dừng thì vòng khung trở về 0 khung khi rảnh.
  5. VPOINT cho cùng kết quả từ bàn phím, `.scr`, `(command ...)`; trong Paper space báo đúng thông điệp.
  6. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** menu chuột phải của 3DORBIT (chưa có thiết kế UI); DVIEW, 3DWALK, 3DFLY (P4).

### RND-128 · ViewCube và lệnh NAVVCUBE
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** RND-007, RND-012, RND-126, INT-002, INT-003, QA-001
- **Lệnh:** NAVVCUBE
- **Màn hình:** UI-A13
- **Đầu vào:** 03 §1 (ViewCube P1, để xem 3D), §21; 07.1 §9.13 (ViewCube mặc định ẩn ở bản vẽ 2D, tự hiện khi bản vẽ có đối tượng 3D), §7.6 (bật/tắt ViewCube ở panel Giao diện), §1.3 (animation ≤ 150 ms, tắt được, tôn trọng giảm chuyển động); 07.2 UI-D10 tab Mô hình 3D (hiển thị ViewCube); biến `NAVVCUBEDISPLAY`, `NAVVCUBELOCATION`, `NAVVCUBESIZE`, `NAVVCUBEOPACITY`, `NAVVCUBEORIENT`.
- **Đầu ra:**
  - `render/view3d/ViewCube.h/.cpp`: khối lập phương ở góc vùng vẽ, nhãn mặt theo ngôn ngữ giao diện; 6 mặt, 12 cạnh, 8 góc, vòng la bàn N/E/S/W, nút Home; vẽ trên overlay (RND-012), chữ qua RND-007; thử chạm trên màn hình.
  - Bấm một vùng: chuyển view có hoạt cảnh ≤ 150 ms, tức thì khi bật giảm chuyển động. Kéo khối: xoay như 3DORBIT. Độ mờ khi con trỏ ở xa theo `NAVVCUBEOPACITY`, rõ khi rê lên.
  - Tự hiện khi cảnh có đối tượng 3D (cờ từ regen), ẩn với bản vẽ 2D thuần (07.1 §9.13).
  - `render/commands/NavVCubeCommand.cpp`: NAVVCUBE `[ON/OFF/Settings]` theo AutoCAD; Settings mở UI-D10 ở tab Mô hình 3D. `docs/specs/commands/NAVVCUBE.md`.
- **Tiêu chí nghiệm thu:**
  1. Bấm từng vùng trong 26 vùng: view đích đúng (so ma trận với bảng kỳ vọng); Home trả về view gốc.
  2. Thử chạm ≤ 0,1 ms; vẽ ViewCube ≤ 0,2 ms GPU **[đề xuất]**.
  3. Bản vẽ 2D thuần trong kho QA-001: ViewCube không hiện. Bản vẽ có 3DFACE hoặc mesh: có hiện.
  4. Bật giảm chuyển động: chuyển view tức thì. NAVVCUBE OFF, ON: có hiệu lực ở khung kế tiếp.
  5. Đạt DoD-C, DoD-L.

### RND-023 · Xem 3D cơ bản: 3DORBIT, kiểu hiển thị wireframe/hidden/shaded cho mesh và 3DFACE, ViewCube
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** RND-126, RND-127, RND-128, QA-001
- **Đầu vào:** API và lệnh của RND-126, RND-127, RND-128; 07.3 §8 (bản 1.0 chỉ có phần xem 3D: xoay 3D, ViewCube, kiểu hiển thị); tệp có 3DFACE, mesh, polyline 3D trong kho QA-001.
- **Đầu ra:** kiểm thử tích hợp `render/tests/view3d_it.cpp` trên tập tệp 3D của kho; `render/docs/view3d.md`.
- **Tiêu chí nghiệm thu:**
  1. Tập tệp 3D của kho (≥ 100 tệp **[đề xuất]**): mở, đổi qua 5 kiểu hiển thị, xoay bằng 3DORBIT, bấm ViewCube không lỗi; ảnh đạt ngưỡng RND-103 so với ảnh chuẩn.
  2. Đi 2D Wireframe → Shaded → 2D Wireframe: ảnh cuối trùng ảnh ban đầu.
  3. Đạt DoD-C, DoD-L.

## Tóm tắt theo mốc

| Mốc | Task | Số task |
|---|---|---|
| S2 | RND-101, RND-102, RND-103, RND-104, RND-105, RND-106, RND-001, RND-107, RND-003, RND-004, RND-005, RND-006, RND-019, RND-011, RND-010, RND-108, RND-109, RND-110, RND-002, RND-025, RND-111 | 21 |
| S6 | RND-112, RND-113, RND-114, RND-115, RND-007 | 5 |
| V | RND-008, RND-116, RND-009, RND-117, RND-012, RND-013, RND-020, RND-118, RND-119, RND-014, RND-015, RND-120, RND-121, RND-122, RND-016, RND-017, RND-018, RND-123, RND-022, RND-021, RND-024, RND-124, RND-126 | 23 |
| VP | RND-125 | 1 |
| 1.0 | RND-127, RND-128, RND-023 | 3 |
| **Tổng** | | **53** |
