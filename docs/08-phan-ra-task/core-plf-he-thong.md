# PLF · Hệ thống: giấy phép, cổng tính năng, cập nhật, crash, telemetry, cài đặt, dịch vụ máy chủ

> Phạm vi: cổng tính năng; giấy phép và kích hoạt (online, offline, dùng thử, floating); thanh toán và hóa đơn điện tử; cập nhật và nội dung trực tuyến; crash, telemetry, gửi bản vẽ lỗi; trình cài đặt và tích hợp hệ điều hành; backend quản trị doanh nghiệp.
> Thư mục mã: `platform/` (máy khách, C++20), `services/` (máy chủ), `packaging/` (đóng gói, ký số, phát hành). Giao diện các màn hình UI-L01…L18 thuộc [ui-3](ui-3-vn-tudong-hethong.md).
> Căn cứ: [05](../05-kien-truc-ky-thuat.md) §2, §3, §16–18; [07.1](../07-dac-ta-ui-1-khung-ung-dung.md) §2; [07.3](../07-dac-ta-ui-3-tinh-nang-vn-va-he-thong.md) §6; [03](../03-chuc-nang-autocad.md) §27.

## 0. Ghi chú thiết kế

- **Máy khách** (`platform/`): chỉ dùng Qt Core/Network, libsodium, Crashpad (05 §2). Không gọi mạng trên đường khởi động hay trên luồng UI; việc mạng chạy nền qua NT-013, hủy được; khi rảnh không có timer (DoD-H). Phần PLF trên đường khởi động ấm ≤ 10 ms **[đề xuất]**.
- **Máy chủ** (`services/`) **[đề xuất, cần ADR]**: Go (thư viện chuẩn, `pgx`, `golang.org/x/crypto`), PostgreSQL, Caddy, systemd trên Ubuntu LTS. Tự vận hành, đặt tại Việt Nam (05 §18, **[rà soát pháp lý]**). Không thành phần AGPL, không dịch vụ đám mây nước ngoài. Một tệp chạy `trion-svc`, bật vai trò theo cấu hình.
- **DoD cho mã Go:** task máy chủ ghi "DoD-C (bản máy chủ)" = DoD-C mục 2, 4, 7, 9, 10, 11; mục 1, 3, 5, 6 thay bằng `go vet`, `staticcheck`, `go test -race`; fuzz bằng `go test -fuzz` **[đề xuất]**. Task có cả hai phần ghi "DoD-C (cả bản máy chủ)".
- **Mật mã:** chỉ libsodium (C++), `crypto/ed25519` và `golang.org/x/crypto` (Go); không tự viết thuật toán (05 §2). Khóa tách theo mục đích: giấy phép, feed cập nhật, nội dung, máy chủ floating. Máy khách nhúng 2 khóa công khai cho mỗi mục đích để xoay khóa; khóa ký cập nhật giữ offline **[đề xuất]**.
- **Tin cậy dựa trên chữ ký, không dựa trên kênh:** giấy phép, feed, gói trợ giúp, bản cài đều kiểm Ed25519 ở máy khách. Không ghim chứng chỉ TLS, để chạy được sau proxy doanh nghiệp **[đề xuất]**.
- **Không khóa dữ liệu:** hết hạn, hết dùng thử, mất ghế floating → về Viewer; bản vẽ vẫn mở, xem, đo, in (07.1 §2). Bản vẽ đã sửa trước lúc hạ phiên bản vẫn lưu được tới khi đóng ("quyền lưu tạm") **[đề xuất]**.
- **Riêng tư (05 §16):** telemetry, crash, báo lỗi chỉ gửi khi người dùng đồng ý. Không gửi nội dung bản vẽ trừ khi người dùng tự đính kèm. Đường dẫn được băm. Tải feed và nội dung không kèm định danh nào.
- **Nền tảng:** Windows trước (V); macOS từ alpha T26–27; Linux sau 1.0 (06). Máy chủ floating chạy Windows Server 2016+, Windows 10/11, Ubuntu 22.04/24.04 (prototype `quan-tri.html`).
- Các số mặc định lấy từ prototype `quan-tri.html` (ân hạn 2 giờ, thu hồi sau 60 phút nhàn rỗi, giữ ghế 3 phút, lời mời 14 ngày, báo cáo 5 phút, cổng 27080/27081), tên tham số MSI, đuôi `.tcconf`/`.tcprofile` đều là **[đề xuất]**, cần chốt.

## 1. Nền chung: dịch vụ máy chủ, mạng, cổng tính năng

### PLF-101 · Nền dịch vụ máy chủ: stack, triển khai tự host, sao lưu, bộ ký
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-002
- **Đầu vào:** 05 §3 (máy chủ tải về, kích hoạt, telemetry, crash), §16, §17 (ADR), §18 (tự vận hành, đặt tại VN); ghi chú §0.
- **Đầu ra:**
  - ADR chọn stack máy chủ trong `docs/adr/`; `services/cmd/trion-svc/` (một tệp chạy, vai trò theo cấu hình);
  - `services/internal/base/`: cấu hình, log `slog`, middleware (giới hạn kích thước, hạn mức theo IP và token), PostgreSQL và migration SQL đánh số, kho blob trên đĩa theo SHA-256, gửi email SMTP, `/healthz`;
  - `services/internal/signer/`: tiến trình ký riêng giữ khóa, nghe socket cục bộ, ghi nhật ký kiểm toán;
  - `services/deploy/`: kịch bản dựng Ubuntu 24.04 (Caddy, PostgreSQL, systemd), sao lưu đêm sang máy thứ hai; job CI cho Go.
- **Tiêu chí nghiệm thu:**
  1. Máy ảo trắng → mọi vai trò chạy, `/healthz` đạt trong ≤ 30 phút bằng một lệnh; chạy lại lần 2 không đổi gì.
  2. Khôi phục bản sao lưu mới nhất sang máy khác: checksum từng bảng và blob khớp 100%; CI diễn tập khôi phục mỗi tuần **[đề xuất]**.
  3. Khởi động lại cuốn chiếu dưới tải 200 req/s: 0 lỗi 5xx; vượt hạn mức trả 429 kèm `Retry-After` **[đề xuất]**.
  4. Chỉ TLS 1.2 trở lên, có HSTS; khóa riêng không nằm trong DB, log hay kho mã; mỗi lần ký có đúng một dòng kiểm toán.
  5. Đạt DoD-C (bản máy chủ).

### PLF-102 · Mạng phía máy khách: HTTP nền, proxy, tải tiếp tục được, tài nguyên ký số
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** NT-005, NT-011, NT-013
- **Đầu vào:** 05 §2 (Qt Network, libsodium), §16; ghi chú §0 (tin cậy dựa trên chữ ký); API NT-013 (tiến trình, hủy), NT-011 (ghi an toàn).
- **Đầu ra:** `platform/net/HttpClient.{h,cpp}`: QNetworkAccessManager trên một luồng mạng riêng, proxy hệ thống/PAC/thủ công, thử lại lũy thừa có jitter, `Retry-After`, tải xuống theo Range, tải lên theo khối có offset. `platform/net/SignedFetch.{h,cpp}`: tệp + chữ ký Ed25519 tách rời, bộ khóa theo mục đích, chống lùi `sequence`, giữ bản tốt cuối. Máy chủ HTTPS giả lập lỗi cho test.
- **Tiêu chí nghiệm thu:**
  1. Đang tải 1 GB: vòng sự kiện luồng chính trễ ≤ 2 ms p99 **[đề xuất]**; không API nào chặn chờ mạng.
  2. Ngắt kết nối ngẫu nhiên 50 lần khi tải 1 GB: tệp cuối đúng SHA-256, dữ liệu tải lại ≤ 5% **[đề xuất]**.
  3. 10.000 ca chữ ký sai, tệp cắt cụt, `sequence` cũ, khóa lạ: từ chối 100%, bản tốt cuối giữ nguyên; fuzz bộ đọc chữ ký và manifest 1 giờ.
  4. Test tích hợp qua proxy có xác thực (Basic; NTLM trên Windows) và qua PAC đều đạt.
  5. Không có yêu cầu nào: 0 timer, ~0% CPU; bộ đệm tải ≤ 1 MB, ghi thẳng ra đĩa.
  6. Đạt DoD-C, DoD-H.

### PLF-002 · API cổng tính năng: phiên bản → khả năng, cờ tính năng theo kênh
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** S · **Phụ thuộc:** NT-001, NT-010
- **Màn hình:** UI-L04
- **Đầu vào:** 07.1 §2 (bảng khả năng theo phiên bản, hành vi cổng, Dùng thử, Giáo dục); 05 §17 (feature flag); README §4.2 (PLF-002 khả năng, INT-020 lệnh, UI0-011 giao diện).
- **Đầu ra:** `platform/license/Capabilities.{h,cpp}`: `Edition` (Viewer, ViewerPro, Pro, Trial, Education, Enterprise); `Capability` (mỗi dòng bảng 07.1 §2 một mục); `has()`; `minimumEdition()` cho thông báo "Lệnh … có trong TrionCAD Pro"; `canSaveDocument(docKey)` (kể cả quyền lưu tạm do PLF-001 cấp); `subscribe()`; `featureEnabled()` theo kênh nội bộ/beta/ổn định; test bảng vàng.
- **Tiêu chí nghiệm thu:**
  1. Test vàng khớp 100% ô của bảng 07.1 §2. Dùng thử = Pro; Giáo dục = Pro kèm cờ huy hiệu; floating, cổng quản trị, cài hàng loạt chỉ có ở Doanh nghiệp.
  2. `has()` không khóa, ≤ 10 ns p99 **[đề xuất]**; TSan sạch với 16 luồng đọc và 1 luồng đổi phiên bản.
  3. Đổi phiên bản lúc chạy: mỗi bên đăng ký nhận đúng 1 sự kiện trên luồng UI trong ≤ 16,6 ms.
  4. Chưa có giấy phép thì là Viewer. Ghi đè phiên bản bằng biến môi trường chỉ có ở bản nội bộ (test trên 2 cấu hình build).
  5. Đạt DoD-C, DoD-L.

## 2. Cài đặt, cập nhật, nội dung trực tuyến

### PLF-009 · Trình cài đặt Windows và tích hợp hệ điều hành: MSI ký số, liên kết tệp, gỡ sạch
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** NT-001, NT-002, NT-101, NT-110, UI0-001
- **Màn hình:** UI-L18
- **Đầu vào:** 07.3 UI-L18 (liên kết tệp, quy tắc "Đặt làm mặc định", menu chuột phải, jump list, gỡ cài đặt); 05 §16; NT-110 (ký mã), NT-101 (`THIRD_PARTY_NOTICES.txt`); tài liệu Windows Installer, WiX Toolset **[cần xác minh điều khoản phí bảo trì của WiX bản mới]**.
- **Đầu ra:**
  - `packaging/windows/` (WiX): MSI x64 cài theo máy, nâng cấp chính, lối tắt, ProgID và Capabilities, động từ chuột phải, AppUserModelID, thuộc tính `INSTALLDIR`, `SET_DEFAULT`, `REMOVE_USER_DATA`; `docs/specs/msi-properties.md` (kể cả tên dành sẵn cho PLF-114);
  - mọi tệp PE và MSI ký qua NT-110 (task này không tự cấu hình ký);
  - `platform/os/ShellIntegration.{h,cpp}`: `isDefaultFor()`, `openDefaultAppsSettings()`, `addRecent()` cho jump list;
  - đóng gói `THIRD_PARTY_NOTICES.txt` do NT-101 sinh, đặt cạnh tệp chạy cho ABOUT.
- **Tiêu chí nghiệm thu:**
  1. Máy ảo sạch Windows 10 22H2 và 11: cài, nâng cấp, gỡ đều đạt; so ảnh chụp registry và tệp trước cài/sau gỡ: 0 mục sót (trừ dữ liệu người dùng khi chọn giữ).
  2. `signtool verify /pa` đạt với 100% tệp PE và MSI.
  3. Máy chưa có ứng dụng CAD: nhấp đúp `.dwg` mở TrionCAD. Máy có AutoCAD giả lập: liên kết cũ giữ nguyên, TrionCAD có trong "Mở bằng".
  4. `msiexec /i … /qn` cài xong không giao diện, mã thoát 0; cài trên máy thấp ≤ 3 phút **[đề xuất]**.
  5. `addRecent()` đưa tệp vào jump list; "Mở chỉ đọc" truyền đúng tham số chỉ đọc cho ứng dụng.
  6. Đạt DoD-C.
- **Ngoài phạm vi:** giao diện trình cài đặt (UI-L18 ở ui-3); tham số doanh nghiệp (PLF-114); thumbnail Explorer (PLF-115); macOS (PLF-116); Linux (sau 1.0).

### PLF-006 · Cập nhật: feed ký số, kênh nội bộ/beta/ổn định, tải nền, kiểm chữ ký trước khi áp dụng
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** PLF-101, PLF-102, PLF-009, NT-013
- **Màn hình:** UI-L06
- **Đầu vào:** 05 §16 (kiểm Ed25519 trước khi áp dụng), §17 (nội bộ mỗi ngày, beta 2 tuần, ổn định mỗi quý); 07.3 UI-L06 (ghi chú phát hành tiếng Việt, dung lượng; Cài ngay / Cài khi đóng ứng dụng / Nhắc sau; hết quyền cập nhật), UI-L09 (tắt tự kiểm tra).
- **Đầu ra:**
  - `docs/specs/update-feed.md`; `packaging/release/sign_feed` (Go: sinh feed theo kênh, ký bằng khóa offline, `sequence` tăng dần, `expires`), dùng lại cho mọi nội dung ký số;
  - `platform/update/Updater.{h,cpp}`: lịch kiểm tra, tải nền qua PLF-102, kiểm SHA-256 và Ed25519, chạy MSI khi ứng dụng đã đóng (ngay hoặc lúc thoát), chọn kênh, trạng thái cho UI;
  - feed và bản cài phục vụ tĩnh trên máy chủ PLF-101.
- **Tiêu chí nghiệm thu:**
  1. Bản cài sửa 1 byte, chữ ký sai, feed cũ phát lại (hạ phiên bản), feed quá `expires`: 100% không cài, báo lỗi rõ.
  2. Kiểm tra lần đầu 60 s sau khởi động, sau đó mỗi 24 giờ **[đề xuất]**; tắt tự kiểm tra thì 0 yêu cầu mạng; yêu cầu không mang định danh nào.
  3. Tải 400 MB ở nền: không khung hình nào > 16,6 ms do updater; mất mạng giữa chừng thì tải tiếp từ chỗ dừng.
  4. "Cài khi đóng ứng dụng" 20 lần liên tiếp trên máy ảo Windows 10/11: phiên bản sau cài đúng, thiết lập người dùng giữ nguyên.
  5. Feed có ghi chú phát hành vi/en; dung lượng hiển thị sai ≤ 1%.
  6. Đạt DoD-C (cả bản máy chủ), DoD-H.
- **Ngoài phạm vi:** quay về bản trước (P2); kênh do quản trị duyệt (PLF-114); áp dụng trên macOS (PLF-116).

### PLF-103 · Nội dung trực tuyến: tin, mẹo cho Trung tâm thông báo và tab Start; cập nhật gói trợ giúp
- **Mốc:** V · **Ưu tiên:** P1 · **Cỡ:** S · **Phụ thuộc:** PLF-102, PLF-006, UI-L14, UI-L12, UI-A02
- **Đầu vào:** 07.3 UI-L14 (loại Mẹo, Tin cộng đồng; chọn loại muốn nhận), UI-L12 (tài liệu offline đóng gói sẵn, cập nhật thêm khi có mạng); API kho thông báo của UIL-001 (UI-L14), gói `help-<phiên bản>.pack` của UIL-010 (UI-L12), khối tin của tab Start (UI-A02); công cụ `sign_feed` (PLF-006).
- **Đầu ra:** `services/content/news/` (tin viết dạng tệp, sinh feed ký số theo ngôn ngữ); `platform/content/NewsFeed.{h,cpp}` (tải, lọc theo phiên bản, dải phiên bản ứng dụng, kênh, ngôn ngữ, khung thời gian; đăng vào Trung tâm thông báo và tab Start); `platform/content/HelpPackUpdater.{h,cpp}` (tải bản sửa mới của gói trợ giúp cùng phiên bản ứng dụng, kiểm chữ ký, thay nguyên tử).
- **Tiêu chí nghiệm thu:**
  1. Yêu cầu tải feed không có cookie, mã cài đặt hay tham số định danh (test kiểm từng byte yêu cầu).
  2. 200 mục test với điều kiện lọc khác nhau: hiện/ẩn đúng 100%; mỗi mục chỉ đăng một lần qua nhiều lần khởi động; loại người dùng đã tắt thì không đăng.
  3. Gói trợ giúp mới: tải nền, kiểm chữ ký và SHA-256, áp ở lần mở Trợ giúp kế tiếp; tải hỏng hoặc dừng giữa chừng thì giữ gói hiện tại; gói đóng kèm luôn là dự phòng.
  4. Tải lần đầu 30 s sau khởi động, sau đó tối đa mỗi 12 giờ **[đề xuất]**; tab Start hiện 3 tin mới nhất từ cache, không chờ mạng.
  5. Đạt DoD-C.
- **Ngoài phạm vi:** viết nội dung trợ giúp (UIL-010).

## 3. Crash, telemetry, gửi bản vẽ lỗi

### PLF-104 · Máy chủ crash: nhận dump, kho symbol, symbol hóa, gom nhóm
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLF-101, NT-002
- **Đầu vào:** 05 §2 (Crashpad), §17 (CI, kênh phát hành); định dạng tải lên trong tài liệu Crashpad; công cụ Breakpad `dump_syms`, `minidump_stackwalk` (BSD-3) **[đề xuất]**.
- **Đầu ra:** `services/internal/crash/`: nhận tải lên của Crashpad (multipart, ≤ 20 MB **[đề xuất]**), lưu dump; hàng đợi symbol hóa chạy trong sandbox giới hạn CPU, RAM, thời gian; kho symbol theo debug ID; chữ ký nhóm từ 5 khung đầu của luồng crash đã chuẩn hóa **[đề xuất]**; thống kê theo phiên bản, xuất JSON hằng ngày cho QA-010. `packaging/symbols/upload_symbols` cho job phát hành (PDB, dSYM).
- **Tiêu chí nghiệm thu:**
  1. 100 dump mẫu Windows và macOS từ bản có symbol: 100% khung của TrionCAD có tên hàm, tệp, dòng.
  2. p95 từ lúc nhận tới lúc có ngăn xếp ≤ 5 phút ở tải 100 dump/giờ **[đề xuất]**.
  3. 1.000 dump của 20 lỗi cài sẵn → đúng 20 nhóm, không gộp nhầm, không tách nhầm.
  4. Fuzz bộ nhận 1 giờ; stackwalk quá 60 s hoặc 1 GB RAM bị dừng, dump đánh dấu lỗi, dịch vụ vẫn chạy.
  5. Dump xóa sau 90 ngày, giữ chữ ký nhóm và số đếm **[đề xuất]**.
  6. Đạt DoD-C (bản máy chủ).

### PLF-105 · Máy chủ nhận tệp và telemetry: tải lên theo khối, mã theo dõi, lưu tối đa 12 tháng
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLF-101, QA-001
- **Đầu vào:** 07.3 UI-L08 (mã theo dõi; ô cho phép dùng trong bộ kiểm thử nội bộ), UI-L09 (máy chủ tại VN, nhật ký đã gửi, yêu cầu xóa), UI-L07 (đính kèm bản vẽ); 05 §16; metadata kho bản vẽ QA-001 (nguồn, quyền).
- **Đầu ra:** `services/internal/upload/`: phiên tải lên, khối 4 MB có offset, hoàn tất kiểm SHA-256, mã theo dõi, cờ quyền sử dụng, liên kết crash ID; xuất tệp có đồng ý sang định dạng nhập của QA-001. `services/internal/telemetry/`: nhận lô gzip JSON, kiểm theo danh mục sự kiện có phiên bản, bảng phân vùng theo tháng, xóa theo mã cài đặt, tổng hợp theo ngày.
- **Tiêu chí nghiệm thu:**
  1. Tải lên 500 MB với 30 lần ngắt kết nối: hoàn tất, SHA-256 khớp, dữ liệu gửi lại ≤ 5%.
  2. Lô có sự kiện hay trường ngoài danh mục bị loại cả lô; fuzz bộ đọc lô và bộ nhận khối 1 giờ.
  3. ≥ 5.000 sự kiện/s, p99 nhận lô ≤ 100 ms trên máy 4 nhân, 8 GB **[đề xuất]**.
  4. Yêu cầu xóa theo mã cài đặt xong ≤ 24 giờ; phân vùng quá 12 tháng tự xóa (test đồng hồ giả lập).
  5. Chỉ tệp có cờ đồng ý được xuất sang QA-001; tệp không đồng ý xóa sau 180 ngày **[đề xuất]**.
  6. Đạt DoD-C (bản máy chủ).

### PLF-007 · Crash: Crashpad, gói chẩn đoán, phát hiện lần chạy lỗi, liên kết khôi phục bản vẽ
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLF-102, PLF-104, PLF-105, NT-008, NT-009, RND-001, DOC-018
- **Màn hình:** UI-L07
- **Đầu vào:** 07.3 UI-L07 (đính kèm bản vẽ mặc định tắt; xem nội dung sẽ gửi; luôn tự gửi, không kèm bản vẽ; phục hồi bản vẽ UI-B07); 05 §2, §16; NT-009 (`crashRingRegion()`), NT-008, RND-001 (GPU, driver).
- **Đầu ra:** `platform/crash/CrashReporter.{h,cpp}` (khởi động `trion_crashpad_handler`, annotation, đăng ký `crashRingRegion()` vào dump, tải lên tắt mặc định; báo cáo chờ: xem, gửi kèm mô tả/email/bản vẽ, bỏ, luôn tự gửi). `Diagnostics.{h,cpp}` (hệ điều hành, CPU, RAM, GPU, driver, phiên bản, kênh, 20 lệnh cuối không kèm tham số, log đã băm đường dẫn; PLF-010 dùng lại). `SessionMarker.{h,cpp}` (lần trước thoát bất thường; bản vẽ đang mở và tệp tự lưu của chúng).
- **Tiêu chí nghiệm thu:**
  1. Crash cưỡng bức (cờ chỉ có ở bản nội bộ) trên Windows và macOS: lần mở sau báo đúng, danh sách bản vẽ đúng 100%; dump tới PLF-104 và có ngăn xếp symbol hóa.
  2. Ghi lại mọi byte gửi đi trong test: mặc định 0 byte; "Luôn tự gửi" không kèm bản vẽ; bản vẽ chỉ đi khi người dùng bật ô đính kèm.
  3. Dump của crash cưỡng bức chứa các dòng log cuối trong `crashRingRegion()` (NT-009). "Xem nội dung sẽ gửi" khớp từng byte phần văn bản thật sự gửi; minidump hiện dung lượng.
  4. Crashpad thêm ≤ 20 ms vào khởi động ấm; cập nhật annotation ≤ 1 µs, không cấp phát **[đề xuất]**.
  5. Mất mạng thì báo cáo xếp hàng; thư mục báo cáo ≤ 200 MB, giữ 20 báo cáo mới nhất **[đề xuất]**.
  6. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** dump khi treo luồng UI (chưa có trong 07.3).

### PLF-008 · Telemetry chỉ khi đồng ý: mức, băm đường dẫn, nhật ký dữ liệu đã gửi
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLF-102, PLF-105, NT-010
- **Màn hình:** UI-L09
- **Đầu vào:** 03 §27 (P0); 05 §16; 07.3 UI-L09 (Tắt; Cơ bản: lỗi và hiệu năng, ẩn danh; Đầy đủ: thêm tần suất lệnh; mô tả cụ thể dữ liệu; xem nhật ký đã gửi; yêu cầu xóa), UI-L11 (gửi kết quả TRIONBENCH khi đồng ý).
- **Đầu ra:** `platform/telemetry/Telemetry.{h,cpp}`: mức, mặc định Tắt tới khi người dùng chọn; `track()` không cấp phát; lô gửi mỗi giờ; nhật ký đã gửi 30 ngày; yêu cầu xóa; mã cài đặt ngẫu nhiên đổi được; `sendOnce()` cho dữ liệu có đồng ý riêng. `events.def` sinh `docs/specs/telemetry-events.md` (vi/en). `PathHash` (BLAKE2b có khóa riêng mỗi máy, libsodium).
- **Tiêu chí nghiệm thu:**
  1. 100.000 sự kiện sinh ngẫu nhiên: quét tự động không thấy đường dẫn, tên tệp, tên người dùng, tên máy hay chuỗi lấy từ bản vẽ trong dữ liệu gửi.
  2. Mức Tắt: 0 byte gửi qua 24 giờ sử dụng mô phỏng (đồng hồ giả lập); hạ mức có hiệu lực ngay, lô chưa gửi của mức cao hơn bị bỏ.
  3. Nhật ký "dữ liệu đã gửi" khớp từng byte payload thật.
  4. `track()` ≤ 200 ns p99, 0 cấp phát; không có sự kiện chờ thì 0 timer **[đề xuất]**.
  5. Tài liệu danh mục sinh từ `events.def`, CI lỗi khi lệch; mọi trường có mô tả vi/en.
  6. Đạt DoD-C, DoD-H.

### PLF-010 · Gửi bản vẽ lỗi (TCREPORT): đóng gói, ẩn danh tùy chọn, tải lên có tiến trình
- **Mốc:** V · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLF-007, PLF-102, PLF-105, INT-001, DOC-013, DWG-027, RGN-006, NT-013, NT-014
- **Lệnh:** TCREPORT
- **Màn hình:** UI-L08
- **Đầu vào:** 07.3 UI-L08 (loại vấn đề, mô tả, ảnh chụp vùng màn hình, tệp đính kèm tự gợi ý xref và font, ô quyền sử dụng, email, mã theo dõi), UI-L17 (biểu mẫu hỗ trợ tự đính kèm nhật ký và thông tin hệ thống); FileDepList (DWG-027), xref (DOC-013), font (RGN-006).
- **Đầu ra:** `platform/report/DrawingReport.{h,cpp}`: gợi ý tệp liên quan kèm dung lượng; gói chẩn đoán PLF-007; ảnh chụp do UI cấp; ẩn danh (bỏ tên người dùng, tên máy, đường dẫn tuyệt đối); nén song song NT-014; tải lên theo khối, tiến trình và hủy qua NT-013; mã theo dõi. API gửi yêu cầu hỗ trợ cho UI-L17. Lệnh TCREPORT (mở hộp UI-L08 qua móc do UI đăng ký); `docs/specs/commands/TCREPORT.md`.
- **Tiêu chí nghiệm thu:**
  1. 50 bản vẽ QA-001 có xref, font, ảnh: gợi ý đủ 100% tệp tồn tại, báo đúng tệp thiếu.
  2. Đóng gói và nén 100 MB ≤ 2 s trên máy tầm trung, luồng UI không chờ **[đề xuất]**.
  3. Bật ẩn danh: manifest và metadata không còn tên người dùng, tên máy, đường dẫn tuyệt đối (quét tự động); "nội dung sẽ gửi" liệt kê đủ tệp và trường.
  4. Mất mạng giữa chừng: tiếp tục từ khối cuối; hủy thì xóa phiên trên máy chủ; xong trả mã theo dõi.
  5. TCREPORT gọi từ dòng lệnh và từ menu Trợ giúp cho cùng hành vi.
  6. Đạt DoD-C, DoD-H.
- **Ngoài phạm vi:** xóa thông tin tác giả bên trong tệp DWG (cần bộ ghi DWG) **[đề xuất]**.

## 4. Giấy phép và kích hoạt

### PLF-106 · Tệp giấy phép ký Ed25519 và dấu vân tay máy
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLF-002, PLF-101
- **Đầu vào:** 05 §2 (libsodium; cấm tự viết mật mã); RFC 8032 (EdDSA); 07.3 UI-L03 (phiên bản, thuê bao/vĩnh viễn, hạn, quyền cập nhật, thiết bị); ghi chú §0 (khóa theo mục đích, xoay khóa).
- **Đầu ra:**
  - `docs/specs/license-format.md`: `.lic` = payload JSON giữ nguyên byte + chữ ký tách rời, `kid`, phiên bản định dạng; trường phiên bản, loại, ghế, `valid_until`, `updates_until`, `recheck_after`, vân tay, tổ chức, khóa công khai của máy chủ floating;
  - `platform/license/LicenseFile.{h,cpp}`; `MachineId.{h,cpp}` (3 thành phần băm BLAKE2b. Windows: MachineGuid, UUID SMBIOS, serial ổ hệ thống. macOS: IOPlatformUUID. Linux: `machine-id`, UUID DMI);
  - `services/internal/licfmt/` (ký và kiểm bằng Go); vector dùng chung `platform/license/testdata/`.
- **Tiêu chí nghiệm thu:**
  1. 200 vector dùng chung: C++ và Go cho cùng kết luận 100% (hợp lệ, sai chữ ký, `kid` thu hồi, hết hạn, sai máy, trường lạ).
  2. Fuzz bộ đọc `.lic` 1 giờ; tệp > 64 KB bị từ chối trước khi cấp phát.
  3. Vân tay khớp khi ≥ 2/3 thành phần trùng **[đề xuất]**; giữ nguyên khi cập nhật hệ điều hành, đổi GPU, RAM, card mạng (nguồn giả lập và CI 3 hệ điều hành).
  4. Kiểm một giấy phép ≤ 1 ms; tính vân tay ≤ 20 ms, không dùng WMI **[đề xuất]**.
  5. Không API nào trả thông tin phần cứng thô; chỉ giá trị băm rời khỏi máy.
  6. Đạt DoD-C (cả bản máy chủ).

### PLF-001 · Mô hình giấy phép: các phiên bản, ràng buộc máy, trạng thái hiệu lực
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLF-002, PLF-106, PLF-006, NT-010, NT-011, INT-019, DWG-022
- **Màn hình:** UI-L03
- **Đầu vào:** 07.1 §2 (Viewer, Viewer Pro, Pro, Dùng thử, Giáo dục, Doanh nghiệp; hết hạn về Viewer, không khóa dữ liệu; Giáo dục không chèn dấu vào bản vẽ); 07.3 UI-L03 (nhắc 30, 7, 1 ngày; đếm ngược ngày dùng offline; quyền cập nhật), UI-L06 (hết quyền cập nhật); 03 §27; API PLF-106.
- **Đầu ra:** `platform/license/LicenseManager.{h,cpp}`: kho giấy phép phạm vi máy và người dùng (ghi nguyên tử); chọn giấy phép hiệu lực cao nhất, đẩy phiên bản vào PLF-002; trạng thái cho UI (loại, hạn, số ngày còn, lý do); sự kiện nhắc hạn; chống lùi đồng hồ; quyền lưu tạm khi hạ phiên bản; `updatesUntil()` cho PLF-006; test đồng hồ giả lập.
- **Tiêu chí nghiệm thu:**
  1. Bảng ≥ 40 ca (thuê bao, vĩnh viễn, dùng thử, Giáo dục, floating × còn hạn, sắp hết, hết hạn, sai máy, `kid` thu hồi): khớp 100% kết quả kỳ vọng.
  2. Nhắc ở mốc 30, 7, 1 ngày, mỗi mốc đúng một lần. Quá `recheck_after` mà chưa kết nối thì đếm ngược; hết 30 ngày offline thì về Viewer **[đề xuất]**. Lùi đồng hồ > 48 giờ: giấy phép có hạn chuyển "cần kết nối" **[đề xuất]**.
  3. Hạ về Viewer giữa phiên: bản vẽ đã sửa vẫn SAVE, SAVEAS được tới khi đóng; 0 thay đổi bị mất trong 100 kịch bản.
  4. Bản đang chạy phát hành sau `updates_until`: về Viewer với lý do "cần gia hạn cập nhật"; feed PLF-006 đánh dấu bản mới cần gia hạn **[đề xuất]**.
  5. DWG lưu ở Giáo dục giống từng byte bản lưu ở Pro (20 tệp QA-001, `trion-console`, đồng hồ cố định). Nạp và kiểm giấy phép lúc khởi động ≤ 5 ms, không gọi mạng.
  6. Đạt DoD-C, DoD-L.

### PLF-107 · Dịch vụ tài khoản và xác minh Giáo dục
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLF-101
- **Màn hình:** UI-L01
- **Đầu vào:** 07.3 UI-L01 (đăng nhập tài khoản, đăng nhập qua trình duyệt; Giáo dục: email trường như `.edu.vn` hoặc mã trường, hạn 1 năm, gia hạn khi vẫn là sinh viên, giảng viên); RFC 8252 (OAuth 2.0 cho ứng dụng gốc), RFC 7636 (PKCE); 05 §18 (dữ liệu cá nhân, **[rà soát pháp lý]**).
- **Đầu ra:** `services/internal/account/`: đăng ký, xác minh email, đăng nhập (argon2id), đặt lại mật khẩu, phiên web, mã cho ứng dụng (authorization code + PKCE, chuyển hướng loopback), xác minh Giáo dục (OTP tới email thuộc danh sách tên miền trường; mã trường có hạn mức), xóa tài khoản; `docs/specs/account-api.md`.
- **Tiêu chí nghiệm thu:**
  1. PKCE đúng RFC 7636: mã dùng một lần, sai `code_verifier` bị từ chối; dùng lại refresh token cũ thì thu hồi cả chuỗi.
  2. Sai mật khẩu 10 lần trong 15 phút: chậm dần và gửi email cảnh báo **[đề xuất]**; DB chỉ chứa mật khẩu dạng argon2id.
  3. Giáo dục: email ngoài danh sách bị từ chối; trạng thái hết đúng sau 365 ngày; 100 yêu cầu đồng thời không làm mã trường vượt hạn mức.
  4. Đăng nhập p99 ≤ 300 ms ở 50 req/s trên máy 4 nhân **[đề xuất]**.
  5. Xóa tài khoản xóa dữ liệu cá nhân trong ≤ 30 ngày, kể cả bản sao lưu xoay vòng **[đề xuất]**.
  6. Đạt DoD-C (bản máy chủ).

### PLF-108 · Dịch vụ kích hoạt: key, giới hạn thiết bị, kích hoạt offline, ký giấy phép
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** PLF-106, PLF-107
- **Màn hình:** UI-L01, UI-L02
- **Đầu vào:** 07.3 UI-L01 (lỗi key sai, hết lượt kích hoạt kèm liên kết quản lý thiết bị), UI-L02 (`.req` kèm QR, tệp phản hồi `.lic`), UI-L03 (thiết bị đã kích hoạt, hủy kích hoạt); `license-format.md` (PLF-106); bộ ký PLF-101.
- **Đầu ra:** `services/internal/activation/`: key `TRION-XXXXX-…` base32 Crockford có số kiểm **[đề xuất]**; quyền sử dụng (phiên bản, ghế, hạn, số máy mỗi ghế); kích hoạt bằng key hoặc tài khoản; hủy kích hoạt; làm mới; giấy phép Giáo dục 1 năm; trang nhận `.req` trả `.lic` (cả URL rút gọn cho QR); danh sách thiết bị; mã lỗi có kiểu; `docs/specs/activation-api.md`.
- **Tiêu chí nghiệm thu:**
  1. Kích hoạt lại cùng máy không tốn lượt; vượt lượt trả `ACTIVATION_LIMIT` kèm URL quản lý thiết bị; hủy kích hoạt trả lượt ngay.
  2. 1.000 yêu cầu đồng thời cho một key 5 lượt: đúng 5 thành công.
  3. `.req` hỏng, quá 8 KB, trường lạ: từ chối có mã lỗi; fuzz bộ đọc `.req` 1 giờ; URL cho QR ≤ 300 ký tự **[đề xuất]**.
  4. Key bị hoàn tiền hoặc thu hồi: lần làm mới kế tiếp trả trạng thái thu hồi; mỗi giấy phép đã cấp có một dòng kiểm toán.
  5. p99 ≤ 200 ms ở 50 req/s **[đề xuất]**; số kiểm phát hiện 100% key gõ sai 1 ký tự (máy khách kiểm trước khi gọi mạng).
  6. Đạt DoD-C (bản máy chủ).

### PLF-003 · Kích hoạt online và offline: máy khách, tích hợp dịch vụ kích hoạt
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLF-001, PLF-102, PLF-108
- **Màn hình:** UI-L01, UI-L02
- **Đầu vào:** 07.3 UI-L01, UI-L02, UI-L03; `activation-api.md` (PLF-108), `account-api.md` (PLF-107); prototype `js/screens4.js` `SC.activate` (tab tài khoản, key, máy chủ, offline; `errBox`).
- **Đầu ra:** `platform/license/Activation.{h,cpp}`: đăng nhập qua trình duyệt (loopback 127.0.0.1, PKCE, hết hạn 5 phút) hoặc bằng mật khẩu; kích hoạt bằng key; xác minh Giáo dục; hủy kích hoạt; làm mới nền theo `recheck_after`; tạo `.req` và chuỗi cho QR (UI tự vẽ QR); nhập `.lic`; mã lỗi có kiểu cho UI; test đầu-cuối với dịch vụ chạy cục bộ.
- **Tiêu chí nghiệm thu:**
  1. Đầu-cuối trên Windows và macOS qua dịch vụ cục bộ: key, tài khoản, Giáo dục, hết lượt, hủy kích hoạt, offline `.req` → `.lic` đều đạt.
  2. Không gọi nào chặn luồng UI; mất mạng giữa chừng trả `NETWORK` trong ≤ 10 s **[đề xuất]**.
  3. `.lic` của máy khác, sửa 1 byte, hết hạn: từ chối với mã lỗi riêng; giấy phép đang dùng không bị ghi đè.
  4. Làm mới nền mặc định mỗi 7 ngày khi có mạng **[đề xuất]**; ngoài một hẹn giờ cho lần làm mới kế tiếp thì 0 timer.
  5. Cổng loopback chỉ nghe 127.0.0.1, đóng ngay sau khi nhận mã; `state` sai bị bỏ qua.
  6. Đạt DoD-C, DoD-H.

## 5. Bán hàng: thanh toán, hóa đơn điện tử, dùng thử

### PLF-109 · Đơn hàng, bảng giá, mã giảm giá, thuê bao và cấp key
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** PLF-108
- **Màn hình:** UI-L16
- **Đầu vào:** 07.3 UI-L16 (gói, số ghế, thuê bao hay vĩnh viễn; mã giảm giá hoặc mã đại lý; gia hạn tự động; quản lý ghế); prototype `thanh-toan.html` (`#mua`: đơn giữ 24 giờ, tự kích hoạt khi tiền về; `#tai-khoan`); API PLF-108.
- **Đầu ra:** `services/internal/order/`: sản phẩm và bảng giá có ngày hiệu lực (VND, số nguyên); mã giảm giá, mã đại lý (hạn, số lần, sản phẩm áp dụng); máy trạng thái đơn (chờ thanh toán → đã thanh toán → đã cấp | hết hạn | hủy | hoàn tiền); cấp key và gửi email; thuê bao năm (đơn gia hạn tự sinh, hủy cuối kỳ, thêm ghế giữa kỳ tính theo ngày **[đề xuất]**); gia hạn quyền cập nhật cho giấy phép vĩnh viễn; `docs/specs/order-api.md`.
- **Tiêu chí nghiệm thu:**
  1. 100 thông báo thanh toán trùng, đồng thời, cho một đơn: cấp đúng một bộ key.
  2. 10.000 tổ hợp giá × mã giảm × số ghế khớp 100% bảng tính độc lập; không phép tính số thực nào trên tiền.
  3. Đơn chuyển khoản quá 24 giờ chưa trả thì hết hạn; hoàn tiền thu hồi key qua PLF-108.
  4. Gia hạn thuê bao kéo dài `valid_until`, gia hạn cập nhật kéo dài `updates_until`; máy khách nhận ở lần làm mới kế tiếp.
  5. Báo cáo theo tháng cho từng mã đại lý, xuất CSV.
  6. Đạt DoD-C (bản máy chủ).

### PLF-110 · Cổng thanh toán: VNPay, MoMo, ZaloPay, thẻ quốc tế, chuyển khoản VietQR
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** PLF-109
- **Màn hình:** UI-L16
- **Đầu vào:** 07.3 UI-L16; prototype `thanh-toan.html#mua` (trang không nhận số thẻ; mã VietQR, nội dung chuyển khoản là mã đơn); tài liệu tích hợp hiện hành của VNPay, MoMo, ZaloPay và cổng thẻ quốc tế **[cần xác minh: chọn cổng thẻ]**; đặc tả VietQR của NAPAS.
- **Đầu ra:** `services/internal/payment/`: bộ chuyển cho từng cổng (tạo giao dịch, chuyển hướng, kiểm chữ ký callback/IPN, truy vấn trạng thái, hoàn tiền nếu cổng hỗ trợ); thanh toán định kỳ bằng token nếu cổng hỗ trợ **[cần xác minh]**; mã VietQR theo đơn; đối soát giao dịch ngân hàng **[cần xác minh nguồn: API ngân hàng hay dịch vụ trung gian]**; công cụ xác nhận tay cho nhân viên; job đối soát cuối ngày.
- **Tiêu chí nghiệm thu:**
  1. Bộ fixture ghi từ sandbox mỗi cổng: callback sai chữ ký, sai số tiền, sai mã đơn bị từ chối 100%; callback gửi lại được xử lý đúng một lần.
  2. Quét DB và log sau bộ test: 0 số thẻ hay dữ liệu thẻ.
  3. Đơn đang chờ được truy vấn lại mỗi 15 phút **[đề xuất]**; đối soát cuối ngày khớp báo cáo cổng 100%, lệch thì cảnh báo.
  4. Chuyển khoản đúng mã đơn và số tiền: đơn sang "đã thanh toán" ≤ 5 phút sau khi ngân hàng báo **[đề xuất]**; sai tiền hoặc sai nội dung thì chờ nhân viên xác nhận.
  5. Fuzz bộ đọc callback của mỗi cổng 1 giờ.
  6. Đạt DoD-C (bản máy chủ).

### PLF-012 · Backend thanh toán và hóa đơn điện tử: hóa đơn, trang tài khoản, nghiệm thu đầu-cuối
- **Mốc:** VP · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** PLF-109, PLF-110, PLF-003
- **Màn hình:** UI-L16
- **Đầu vào:** 07.3 UI-L16 (thông tin xuất hóa đơn: tên công ty, mã số thuế, địa chỉ, email; trang tài khoản: hóa đơn, gia hạn tự động, quản lý ghế); prototype `thanh-toan.html#tai-khoan`; quy định hóa đơn điện tử: Nghị định 123/2020/NĐ-CP, Thông tư 78/2021/TT-BTC và văn bản sửa đổi **[cần xác minh]**; API của một nhà cung cấp hóa đơn điện tử được cấp phép **[cần xác minh: chọn nhà cung cấp]**.
- **Đầu ra:** `services/internal/invoice/` (phát hành qua nhà cung cấp sau khi đơn đã thanh toán, lưu PDF và XML, gửi email, hóa đơn điều chỉnh/thay thế **[cần xác minh]**, hàng đợi thử lại); `services/internal/billing/` (API trang tài khoản: thuê bao, bật/tắt gia hạn tự động, ghế, thiết bị, phương thức thanh toán, hóa đơn); `docs/specs/billing-api.md`; test đầu-cuối.
- **Tiêu chí nghiệm thu:**
  1. Đầu-cuối trên môi trường thử, với mỗi cổng của PLF-110: chọn gói → thanh toán sandbox → key → kích hoạt máy khách (PLF-003) → hóa đơn PDF/XML ở trang tài khoản.
  2. Mỗi đơn đã thanh toán có đúng một hóa đơn; tiền hàng, thuế, tổng khớp đơn từng đồng; mã số thuế sai định dạng bị chặn lúc đặt hàng.
  3. Nhà cung cấp hóa đơn ngừng 2 giờ: hóa đơn xếp hàng, phát hành bù 100% khi có lại, không trùng.
  4. Trạng thái "Đang hoạt động", "Còn 7 ngày", "Đã hết hạn" đúng theo ngày; thuê bao không có token thanh toán thì có email nhắc ở 30, 7, 1 ngày.
  5. Người dùng chỉ đọc được dữ liệu của mình (test phân quyền mọi endpoint); p99 API ≤ 300 ms **[đề xuất]**.
  6. Đạt DoD-C (bản máy chủ).

### PLF-004 · Dùng thử 30 ngày: nhắc 7/3/1 ngày, hết hạn về Viewer, không khóa dữ liệu
- **Mốc:** EA · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLF-003, PLF-109
- **Màn hình:** UI-L05
- **Đầu vào:** 07.1 §2 (huy hiệu "Dùng thử: còn 12 ngày"; nhắc 7, 3, 1 ngày; hết hạn về Viewer, không khóa dữ liệu); 07.3 UI-L05 (cần email; màn hình hết hạn kèm ưu đãi), UI-L04 (nút Dùng thử khi chưa dùng thử).
- **Đầu ra:** máy chủ `services/internal/activation/trial.go` (giấy phép dùng thử 30 ngày gắn email đã xác minh bằng OTP và vân tay máy; ưu đãi lấy từ mã giảm giá PLF-109); máy khách `platform/license/Trial.{h,cpp}` (`canStartTrial()`, bắt đầu, số ngày còn, sự kiện nhắc, hết hạn, ưu đãi).
- **Tiêu chí nghiệm thu:**
  1. Đồng hồ giả lập: nhắc ở ngày 23, 27, 29, mỗi mốc đúng một lần; ngày 30 + 1 giây về Viewer; khởi động lại giữ đúng trạng thái.
  2. Mỗi máy và mỗi email chỉ dùng thử một lần; gỡ rồi cài lại không đặt lại thời gian (máy chủ kiểm theo vân tay).
  3. Sau hết hạn: mở, xem, đo, in 20 tệp QA-001 vẫn chạy; thay đổi chưa lưu lúc hết hạn vẫn lưu được (PLF-001).
  4. `canStartTrial()` trả sai khi đã dùng thử hoặc đang có giấy phép trả phí.
  5. Ưu đãi (mã, hạn) lấy từ máy chủ; quá hạn ưu đãi thì không trả về.
  6. Đạt DoD-C (cả bản máy chủ).

## 6. Doanh nghiệp: floating, cổng quản trị, triển khai

### PLF-111 · Máy chủ floating: cấp ghế ký số, hàng đợi, thu hồi, ân hạn, nhật ký bền
- **Mốc:** 1.0 · **Ưu tiên:** P0 · **Cỡ:** L · **Phụ thuộc:** PLF-101, PLF-106
- **Màn hình:** UI-L15
- **Đầu vào:** 03 §27 (giấy phép mạng floating: P0); 07.3 UI-L01 ("Đã dùng hết 10/10 ghế", báo khi có ghế trống), UI-L15; prototype `quan-tri.html` (ân hạn 2 giờ khi mất kết nối, thu hồi sau 60 phút nhàn rỗi, giữ ghế 3 phút cho người đầu hàng đợi, thu hồi cho 5 phút để lưu, cổng 27080) **[đề xuất]**; `license-format.md` (PLF-106).
- **Đầu ra:** `services/floating/` (tệp chạy `trion-license-server`): giao thức HTTP/JSON trên TLS **[đề xuất]**; tìm máy chủ bằng UDP broadcast trong LAN; ghế cấp dạng lease ký bằng khóa máy chủ, khóa này được chứng nhận trong giấy phép máy chủ do TrionCAD ký; nhịp 2 phút, lease hết sau 10 phút không nhịp **[đề xuất]**; hàng đợi FIFO; thu hồi; nhật ký ghi trước (fsync) + snapshot; `docs/specs/floating-protocol.md`.
- **Tiêu chí nghiệm thu:**
  1. Kiểm thử thuộc tính 1 triệu sự kiện ngẫu nhiên (cấp, trả, mất nhịp, thu hồi, khởi động lại): số lease hiệu lực không lúc nào vượt số ghế; hàng đợi đúng thứ tự.
  2. Tắt đột ngột rồi chạy lại trong ≤ 10 s: mọi lease còn hiệu lực giữ nguyên, không ghế nào bị mất.
  3. Lease ký bằng khóa không được chứng nhận (máy chủ giả): máy khách test từ chối 100%.
  4. 1.000 máy khách giả lập: p99 cấp ghế ≤ 50 ms, RAM ≤ 100 MB trên máy 2 nhân, 2 GB **[đề xuất]**.
  5. Fuzz mọi bộ đọc thông điệp 1 giờ; IP ngoài danh sách cho phép bị từ chối.
  6. Đạt DoD-C (bản máy chủ).

### PLF-112 · Máy chủ floating: kích hoạt máy chủ, dịch vụ hệ điều hành, gói cài, API quản trị, sao lưu
- **Mốc:** 1.0 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLF-111, PLF-108, PLF-009
- **Màn hình:** UI-L15
- **Đầu vào:** 07.3 UI-L15 (ứng dụng dịch vụ có giao diện quản lý nhỏ: trạng thái, ghế đang dùng, nhật ký); prototype `quan-tri.html` (Windows Server 2016+, Windows 10/11, Ubuntu 22.04/24.04; nhập khóa máy chủ; cổng quản lý 27081 chỉ mở trong mạng công ty; sao lưu 02:00, giữ 14 bản `.tcbak`; báo cáo lên cổng mỗi 5 phút, không gửi nội dung bản vẽ) **[đề xuất]**.
- **Đầu ra:** trong `services/floating/`: kích hoạt máy chủ bằng khóa (online, hoặc `.req`/`.lic` qua PLF-108), sinh khóa máy chủ tại chỗ; dịch vụ Windows và unit systemd; gói `.msi` (bộ công cụ PLF-009, ký qua NT-110) và `.deb`; API quản trị cục bộ (trạng thái, ghế, hàng đợi, nhật ký lọc được, thu hồi ghế, cổng, danh sách IP, sao lưu/khôi phục) cho trang tĩnh của UI-L15; gửi báo cáo lên cổng quản trị theo `docs/specs/floating-report.md`.
- **Tiêu chí nghiệm thu:**
  1. Máy ảo sạch Windows Server 2019, Windows 11, Ubuntu 24.04: dịch vụ tự chạy sau khởi động lại; ≤ 5 phút từ lúc cài tới lúc cấp ghế đầu tiên.
  2. Cổng 27080 bị chiếm: dịch vụ dừng, nhật ký ghi tên tiến trình và PID đang chiếm cổng.
  3. Khôi phục từ bất kỳ bản nào trong 14 bản `.tcbak`: trạng thái ghế khớp 100% lúc sao lưu.
  4. API quản trị: sai mật khẩu hoặc IP ngoài mạng cho phép bị từ chối; mọi thao tác thu hồi có dòng nhật ký.
  5. Mất kết nối tới cổng quản trị: báo cáo xếp hàng, gửi bù khi có lại; báo cáo chỉ gồm số ghế, tên máy, tài khoản.
  6. Đạt DoD-C (bản máy chủ).

### PLF-005 · Máy chủ giấy phép floating: máy khách, tích hợp, nghiệm thu đầu-cuối
- **Mốc:** 1.0 · **Ưu tiên:** P0 · **Cỡ:** M · **Phụ thuộc:** PLF-001, PLF-102, PLF-111, PLF-112
- **Màn hình:** UI-L01, UI-L03
- **Đầu vào:** 07.3 UI-L01 (nhập địa chỉ máy chủ; hết ghế: Thử lại, Báo khi có ghế trống, Mở ở chế độ Viewer), UI-L03 (máy chủ, ghế đang dùng/tổng); `floating-protocol.md` (PLF-111); prototype `js/screens4.js` (tab `srv`, `seats`, `scan`, `notify`).
- **Đầu ra:** `platform/license/FloatingClient.{h,cpp}`: tìm máy chủ trong LAN, nhập tay, đọc `LICENSE_SERVER` từ cài đặt phạm vi máy; lấy ghế ở nền lúc khởi động, gửi nhịp, trả ghế khi thoát; báo hoạt động người dùng cho luật nhàn rỗi; hàng đợi và báo khi có ghế; thu hồi với 5 phút để lưu; ân hạn 2 giờ có đếm ngược; trạng thái cho UI; kịch bản đầu-cuối nhiều máy.
- **Tiêu chí nghiệm thu:**
  1. Đầu-cuối 12 máy khách, 10 ghế: hết ghế, xếp hàng, nhận ghế khi có người trả, thu hồi, nhàn rỗi 60 phút, máy chủ khởi động lại: đúng kịch bản 100%.
  2. Mất kết nối 1 giờ vẫn làm việc; quá 2 giờ thì về Viewer, bản vẽ đã sửa vẫn lưu được.
  3. Khởi động không chờ ghế, vẫn trong ngân sách khởi động ấm; lấy ghế trong LAN p99 ≤ 200 ms **[đề xuất]**.
  4. Tìm máy chủ trong LAN ≤ 3 s; máy khách tắt đột ngột thì máy chủ thu ghế sau ≤ 10 phút.
  5. Đạt DoD-C, DoD-L.
- **Ngoài phạm vi:** mượn giấy phép offline N ngày (P2 ở UI-L03, sau 1.0).

### PLF-113 · Backend quản trị: tổ chức, vai trò, ghế theo email, lời mời, cấu hình triển khai
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** L · **Phụ thuộc:** PLF-107, PLF-109
- **Màn hình:** UI-L15
- **Đầu vào:** 07.3 UI-L15 (ghế, người dùng; gán ghế cho email nhân viên; cấu hình mặc định cho cả công ty: hồ sơ, đường dẫn mẫu chung, thư viện chung, CTB chung); prototype `quan-tri.html` (Quản trị phụ chỉ xem, mời hàng loạt, lời mời hết hạn 14 ngày, thu hồi; cấu hình có phiên bản và khóa, xuất `.tcconf`, kênh Ổn định/Beta/Do quản trị duyệt) **[đề xuất]**.
- **Đầu ra:** `services/internal/org/`: tổ chức và kho ghế (từ đơn PLF-109); quản trị viên với vai trò Chủ sở hữu, Quản trị, Quản trị phụ chỉ xem; gán và thu hồi ghế; lời mời hàng loạt có kiểm định dạng email; cấu hình chung có phiên bản, cờ khóa từng mục, xuất `.tcconf`; nhật ký thao tác quản trị; `docs/specs/admin-api.md`.
- **Tiêu chí nghiệm thu:**
  1. Bảng phân quyền phủ mọi endpoint: Quản trị phụ chỉ xem nhận 403 ở 100% thao tác ghi; không tổ chức nào đọc được dữ liệu của tổ chức khác.
  2. Mời 500 email trong một lần: email sai định dạng được báo theo dòng, phần đúng vẫn gửi; lời mời quá 14 ngày hết hiệu lực.
  3. Thu hồi ghế: thiết bị của người đó mất quyền ở lần làm mới kế tiếp, ≤ 24 giờ với ghế của tổ chức **[đề xuất]**.
  4. Mỗi lần lưu cấu hình tạo phiên bản mới, xem lại được mọi phiên bản; đường dẫn không hợp lệ (UNC sai, rỗng) bị chặn với lỗi theo ô.
  5. Đạt DoD-C (bản máy chủ).

### PLF-114 · Triển khai doanh nghiệp phía máy khách: tham số MSI, cấu hình công ty, mục khóa
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** PLF-113, PLF-005, PLF-006, PLF-009, NT-010, INT-019
- **Màn hình:** UI-L18
- **Đầu vào:** 03 §27 (cài đặt im lặng, triển khai qua mạng, cấu hình quản trị: P1); 07.3 UI-L18 (cài đặt im lặng), UI-L15 (cấu hình chung); prototype `quan-tri.html` (`ORG_ID`, `LICENSE_MODE` named/floating/both, `LICENSE_SERVER`, `CONFIG` trỏ tệp `.tcconf` trên mạng, `UPDATE_CHANNEL` stable/beta/managed, `DESKTOP_SHORTCUT`) **[đề xuất]**; `msi-properties.md` (PLF-009).
- **Đầu ra:** thuộc tính MSI trong `packaging/windows/`, ghi vào phạm vi máy của NT-010 (kiểm giá trị, sai thì dừng cài với log rõ); `platform/org/OrgConfig.{h,cpp}`: tải cấu hình theo `ORG_ID` hoặc đọc `.tcconf`; áp hồ sơ, đường dẫn mẫu, thư viện, CTB, kênh cập nhật; mục khóa thành chính sách phạm vi máy; kênh "do quản trị duyệt" chỉ nhận bản đã duyệt từ feed PLF-006.
- **Tiêu chí nghiệm thu:**
  1. Máy ảo Windows 10/11, `msiexec /i … /qn` với từng tổ hợp tham số: ứng dụng báo đúng chế độ giấy phép, máy chủ, kênh, phiên bản cấu hình (kiểm qua `trion-console`).
  2. Mục khóa không đổi được ở phạm vi người dùng (API trả lỗi khóa); mục không khóa giữ thay đổi của người dùng qua lần áp cấu hình mới.
  3. Máy không ra Internet đọc `.tcconf` qua đường dẫn mạng; mất mạng thì dùng bản cache; cấu hình mới áp ở lần mở kế tiếp, không chặn khởi động.
  4. Fuzz bộ đọc `.tcconf` 1 giờ; gỡ im lặng giữ thiết lập người dùng, trừ khi có `REMOVE_USER_DATA=1`.
  5. Đạt DoD-C.

### PLF-011 · Backend cổng quản trị doanh nghiệp: thiết bị, lịch sử dùng, floating, nghiệm thu đầu-cuối
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** PLF-113, PLF-114, PLF-112, PLF-108, PLF-006
- **Màn hình:** UI-L15
- **Đầu vào:** 07.3 UI-L15 (thiết bị, lịch sử dùng; tải bộ cài MSI; máy chủ floating); prototype `quan-tri.html` (Thiết bị; Lịch sử dùng lọc 7/30/90 ngày, xuất CSV; trạng thái Mất kết nối máy chủ floating); `floating-report.md` (PLF-112).
- **Đầu ra:** `services/internal/admin/`: danh sách thiết bị (tên máy, người dùng, phiên bản, lần cuối thấy, hủy kích hoạt); lịch sử dùng tổng hợp theo ngày từ lần làm mới giấy phép và báo cáo floating, xuất CSV; nhận báo cáo của máy chủ floating, phát hiện mất kết nối; danh sách bộ cài MSI kèm SHA-256 lấy từ feed PLF-006; kịch bản đầu-cuối của cổng.
- **Tiêu chí nghiệm thu:**
  1. Đầu-cuối: tạo tổ chức → mời 50 người → nhận lời mời → gán ghế → thiết bị hiện ≤ 1 phút sau kích hoạt → cấu hình v2 tới máy khách (PLF-114) ở lần mở kế tiếp.
  2. Quá 15 phút không có báo cáo floating: trạng thái "Mất kết nối" **[đề xuất]**; có báo cáo lại thì tự hết.
  3. CSV lịch sử dùng khớp 100% số liệu gốc theo bộ lọc 7/30/90 ngày.
  4. Truy vấn tổng quan p99 ≤ 500 ms với tổ chức 5.000 ghế, 1 năm dữ liệu **[đề xuất]**.
  5. Đạt DoD-C (bản máy chủ).

## 7. Tích hợp hệ điều hành bổ sung: thumbnail Explorer, macOS

### PLF-115 · Thumbnail và khung xem trước DWG trong File Explorer
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** PLF-009, DWG-026
- **Màn hình:** UI-L18
- **Đầu vào:** 07.3 UI-L18 (thumbnail và khung xem trước DWG trong File Explorer, P1, phát hành 1.0); bộ đọc ảnh xem trước DWG-026; tài liệu Windows Shell (IThumbnailProvider, IInitializeWithStream, IPreviewHandler).
- **Đầu ra:** `platform/shellext/win/`: DLL COM chạy trong tiến trình cô lập của Explorer. Thumbnail cho `.dwg`, `.dwt` từ ảnh xem trước nhúng, đọc qua stream, chỉ đọc header và section ảnh. Khung xem trước hiện ảnh lớn kèm tên tệp, phiên bản DWG. Không có ảnh thì dùng biểu tượng mặc định. Đăng ký trong MSI của PLF-009.
- **Tiêu chí nghiệm thu:**
  1. 10.000 tệp QA-001 và 1.000 tệp hỏng: Explorer 0 crash, 0 treo; fuzz đường đọc ảnh xem trước 1 giờ.
  2. Mỗi thumbnail ≤ 30 ms p99, đọc ≤ 1 MB, RAM ≤ 32 MB, kể cả tệp 100 MB **[đề xuất]**.
  3. Thumbnail giống từng pixel ảnh xem trước mà DWG-026 giải mã được.
  4. Gỡ cài đặt gỡ hết đăng ký COM; Explorer không giữ khóa tệp sau khi xem.
  5. Đạt DoD-C.

### PLF-116 · Gói macOS: ứng dụng, DMG, loại tệp, cập nhật trên macOS
- **Mốc:** 1.0 · **Ưu tiên:** P1 · **Cỡ:** M · **Phụ thuộc:** PLF-006, PLF-007, NT-002, NT-113, UI0-001
- **Màn hình:** UI-L18
- **Đầu vào:** 07.3 UI-L18 (bản macOS là gói kéo vào Applications); 06 (bản macOS alpha T26–27: task phải xong trước mốc này); ký số và notarize của NT-113; tài liệu Apple về Hardened Runtime và khai báo loại tệp (UTI).
- **Đầu ra:** `packaging/macos/`: `.app` arm64 **[đề xuất]** kèm Qt framework và `trion_crashpad_handler`; bật Hardened Runtime, ký, notarize và staple qua NT-113; DMG có lối tắt Applications; khai báo loại tệp `.dwg`, `.dxf`, `.dwt` trong Info.plist; phần áp dụng bản cập nhật của PLF-006 cho macOS (kiểm Ed25519 và chữ ký mã của Apple, thay gói khi thoát).
- **Tiêu chí nghiệm thu:**
  1. Máy macOS sạch: tải DMG, kéo vào Applications, mở lần đầu không có cảnh báo Gatekeeper; `spctl --assess` và `codesign --verify --deep --strict` đạt.
  2. Nhấp đúp `.dwg` trong Finder mở TrionCAD; "Mở bằng" có TrionCAD.
  3. Cập nhật qua feed 20 lần liên tiếp đều cài đúng; gói tải về bị sửa 1 byte thì không cài.
  4. Crash cưỡng bức trên macOS tạo dump symbol hóa được (PLF-007).
  5. Đạt DoD-C.

## Tóm tắt theo mốc

| Mốc | Task | Số task |
|---|---|---|
| V | PLF-101, PLF-102, PLF-002, PLF-009, PLF-006, PLF-103, PLF-104, PLF-105, PLF-007, PLF-008, PLF-010 | 11 |
| VP | PLF-106, PLF-001, PLF-107, PLF-108, PLF-003, PLF-109, PLF-110, PLF-012 | 8 |
| EA | PLF-004 | 1 |
| 1.0 | PLF-111, PLF-112, PLF-005, PLF-113, PLF-114, PLF-011, PLF-115, PLF-116 | 8 |
| **Tổng** | | **28** |
