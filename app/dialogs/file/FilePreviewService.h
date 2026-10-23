#pragma once
// UIB-001 · Đọc nhanh thông tin xem trước của tệp bản vẽ (07.1 UI-B01 khung xem trước, UI-B07; prototype TC.files.open:
// info(), drawRecentThumb trong js/screens1.js).
//
// requestPreview(đường dẫn) đọc ở nền (sched, Normal) rồi phát ready(FilePreviewInfo) trên luồng UI (ui::postLatest):
// mã phiên bản AC10xx và tên ("DWG 2018", "DXF 2013"), ứng dụng lưu lần cuối, ngày sửa, dung lượng, ảnh xem trước, mã
// lỗi. DWG, DXF (chữ và nhị phân), DWT chung một API; định dạng nhận theo nội dung, không theo phần mở rộng.
// - Chỉ đọc kHeadBytes đầu tệp, không giải mã đối tượng (tệp 100 MB đọc như tệp 1 KB). DWG: mã 6 byte, header cố định
//   theo Open Design Specification for .dwg (R13–R2000: bảng section locator; R2004 trở lên: khối 0x6C byte mã hóa phải
//   giải ra "AcFssFcAJMB"); section trỏ quá cuối tệp là tệp cụt. DXF: biến $ACADVER trong HEADER.
// - Yêu cầu mới hủy yêu cầu cũ: CancelToken của việc trước bật ngay trong lượt gọi, kết quả của nó không phát; cùng
//   đường dẫn với việc đang chạy thì giữ việc đó.
// - Cache LRU kCacheEntries mục theo (đường dẫn, giờ sửa, kích thước): chọn lại tệp đã xem không đọc lại, tệp đổi thì
//   khóa đổi. Không cache NotFound, NoAccess (đổi được mà giờ sửa không đổi).
// - Luồng UI chỉ bật token, gửi việc, phát kết quả: không chạm đĩa (stat của ổ mạng chậm cũng nằm ở nền).
// Rảnh: không timer, không polling. Luồng UI cho mọi hàm không static.
//
// Dùng (luồng UI, sau UiExecutor; Application giữ một bản cho mọi hộp thoại tệp để cache sống qua các lần mở):
//   auto* previews = new trion::files::FilePreviewService(app);
//   connect(previews, &FilePreviewService::ready, pane, &Pane::show);  // info.path: tệp của kết quả
//   previews->requestPreview(path);                                    // mỗi lần đổi lựa chọn
//
// ponytail: ảnh nhúng và "lưu lần cuối bởi" là nguồn tạm (std::function): DWG-026 (ảnh xem trước BMP/WMF/PNG nhúng) và
// DWG-027 (SummaryInfo, AppInfo) cắm vào setThumbnailSource/setSavedBySource; chưa cắm thì thumbnail, savedBy rỗng.
// Kiểm hỏng chỉ ở mức header: DWG-004 (nhận diện phiên bản) thay parseHeader khi có, giữ API. Việc đang chờ stat của ổ
// mạng chết không hủy được (stat chặn): mỗi yêu cầu một việc riêng nên yêu cầu sau không phải chờ nó.

#include "app/core/UiExecutor.h"
#include "core/base/sched/Scheduler.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QDateTime>
#include <QImage>
#include <QObject>
#include <QString>

#include <cstdint>
#include <functional>
#include <memory>

namespace trion::files {

// Thông tin xem trước của một tệp.
struct FilePreviewInfo {
    enum class Format : std::uint8_t { Unknown, Dwg, Dxf }; // theo nội dung; DWT là Dwg
    enum class Error : std::uint8_t {
        None,       // đọc được
        NotFound,   // không có tệp ở đường dẫn (đã xóa, đổi tên, ổ mạng không vào được) hoặc đó là thư mục
        NoAccess,   // có tệp nhưng không mở được để đọc (quyền, tệp bị khóa độc quyền)
        NotDrawing, // nội dung không phải DWG, DXF
        Damaged,    // đầu tệp là bản vẽ nhưng hỏng: trống, cụt, mã phiên bản sai, header sai
    };

    QString path;     // như lúc yêu cầu
    Format format = Format::Unknown;
    QByteArray code;  // "AC1032"; rỗng: DXF không ghi $ACADVER, hoặc lỗi
    QString version;  // "DWG 2018", "DXF 2013"; "DXF": không rõ phiên bản; mã lạ: chính mã; rỗng: không nhận ra
    QString savedBy;  // ứng dụng lưu lần cuối, "AutoCAD 2024" (DWG-027); rỗng: không rõ
    QDateTime modified; // giờ sửa, UTC; không hợp lệ khi NotFound
    qint64 size = -1;   // byte; -1 khi NotFound
    QImage thumbnail;   // ảnh nhúng (DWG-026); rỗng: tệp không có ảnh
    Error error = Error::None;

    bool ok() const { return error == Error::None; }
};

class FilePreviewService final : public QObject {
    Q_OBJECT
public:
    static constexpr int kCacheEntries = 64;        // [đề xuất] UIB-001
    static constexpr qint64 kHeadBytes = 16 * 1024; // phần đầu tệp được đọc (DXF: $ACADVER ở đầu HEADER)

    // Nguồn chạy trên luồng nền, có thể nhiều lời gọi cùng lúc, chỉ cho tệp đọc được header (ok()). Xét cancel thường
    // xuyên (≤ 50 ms giữa hai lần xét), thấy thì trả rỗng; ném ngoại lệ: như rỗng. Mọi thứ nguồn dùng phải sống tới khi
    // việc nền xong (sched::waitIdle).
    using ThumbnailSource = std::function<QImage(const QString& path, sched::CancelToken cancel)>;
    using SavedBySource = std::function<QString(const QString& path, sched::CancelToken cancel)>;
    struct Sources {
        ThumbnailSource thumbnail;
        SavedBySource savedBy;
    };

    explicit FilePreviewService(QObject* parent = nullptr);
    ~FilePreviewService() override; // hủy yêu cầu đang chạy; việc nền xong sau đó không chạm tới đối tượng

    // Đặt nguồn: bỏ cache, hủy yêu cầu đang chạy (kết quả cũ không phát).
    void setThumbnailSource(ThumbnailSource source);
    void setSavedBySource(SavedBySource source);

    // Hủy yêu cầu trước, đọc path ở nền, phát ready khi xong. Trả số của yêu cầu (tăng dần, khác 0).
    quint64 requestPreview(const QString& path);
    void cancel();                                // hủy yêu cầu đang chạy; ready không phát cho nó
    quint64 pending() const { return pending_; } // số của yêu cầu đang chạy, 0 nếu không có
    int cacheSize() const;

    // Mọi luồng, chặn: đọc một tệp (stat, đầu tệp, nguồn), không qua cache.
    static FilePreviewInfo read(const QString& path, const Sources& sources = {}, sched::CancelToken cancel = {});
    // Hàm thuần (test, fuzz): head là tối đa kHeadBytes byte đầu của tệp cỡ size. Điền format, code, version; error là
    // None, NotDrawing hoặc Damaged.
    static FilePreviewInfo parseHeader(QByteArrayView head, qint64 size);
    // Tên phiên bản của mã đầu tệp: "AC1032" → "DWG 2018" (Dxf: "DXF 2018"); mã đúng dạng nhưng lạ ("AC1040") → chính
    // mã; sai dạng → rỗng.
    static QString versionName(QByteArrayView code, FilePreviewInfo::Format format);
    // Câu giải thích lỗi cho người dùng; rỗng với None.
    static QString errorText(FilePreviewInfo::Error error);

signals:
    // Kết quả của yêu cầu mới nhất (đã thay thế hoặc hủy thì không phát).
    void ready(const trion::files::FilePreviewInfo& info);

private:
    struct Cache; // LRU dùng chung với việc nền (shared_ptr), khóa bằng mutex

    void finished(quint64 id, const FilePreviewInfo& info);
    void reset();

    ui::Receiver rx_; // một khóa cho mọi kết quả: postLatest giữ kết quả mới nhất
    std::shared_ptr<Cache> cache_;
    Sources sources_;
    sched::CancelSource cancel_;
    QString pendingPath_;
    quint64 pending_ = 0;
    quint64 nextId_ = 0;
};

} // namespace trion::files
