#pragma once
// UIA-004 · Hàng đợi ảnh xem trước của tệp gần đây (07.1 UI-A02 "Thumbnail tạo ở nền"; README task §4.2: DWG-026 đọc
// ảnh nhúng, RND-021 render ngoài màn hình, UI chỉ hiển thị). Prototype: drawRecentThumb trong js/screens1.js.
//
// - request(đường dẫn đang thấy, cỡ): mỗi tệp chưa có ảnh một việc nền (sched, Background) theo đúng thứ tự đưa vào:
//   ảnh nhúng trong tệp trước (setEmbeddedSource), không có thì render (setRenderSource); ảnh thu vào cỡ yêu cầu (giữ tỉ
//   lệ) ngay ở việc nền nên luồng UI chỉ vẽ ảnh 1:1. Lần gọi sau thay danh sách: việc của tệp không còn thấy (hoặc khác
//   cỡ) bị hủy qua CancelToken, tệp đã có ảnh, đang làm hoặc đã lỗi không làm lại.
// - cancelAll(): rời tab Start (StartPage::hideEvent), mọi CancelToken bật ngay trong lượt gọi; việc chưa chạy về luôn.
// - Ảnh xong vào cache ảnh đã giải mã (QCache: LRU theo dung lượng, kCacheBytes) rồi phát ready(path) qua ui::post.
//   Nguồn trả ảnh rỗng, ném ngoại lệ: tệp ghi là lỗi (failed), ready vẫn phát, thẻ giữ ô giữ chỗ; không thử lại tới khi
//   forget(path) (tệp đổi) hoặc đặt nguồn mới. Bị hủy: không ghi gì, lần request sau làm lại.
// - Rảnh không timer, không polling. Luồng UI cho mọi hàm.
//
// ponytail: nguồn tạm là std::function: DWG-026 (core/dwg/preview readPreview: BMP/WMF/PNG nhúng) và RND-021
// (OffscreenRenderer, thumbnail theo LOD) cắm vào đây. Mỗi tệp một việc nền, song song tới số luồng của pool;
// giới hạn đồng thời, ưu tiên theo khoảng cách tới vùng nhìn do NT-005 (QoS) quyết định khi cần.

#include "core/base/sched/Scheduler.h"

#include <QCache>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QSet>
#include <QSize>
#include <QStringList>

#include <cstdint>
#include <functional>

namespace trion::shell {

class ThumbnailQueue final : public QObject {
    Q_OBJECT
public:
    // Chạy trên luồng nền, có thể nhiều lời gọi cùng lúc; xét cancel thường xuyên (≤ 50 ms giữa hai lần xét), thấy
    // thì trả ảnh rỗng. Mọi thứ nguồn dùng phải sống tới khi việc nền xong (sched::waitIdle).
    using EmbeddedSource = std::function<QImage(const QString& path, sched::CancelToken cancel)>;
    // size: px thiết bị của ô ảnh; ảnh trả về cỡ khác thì được thu lại giữ tỉ lệ.
    using RenderSource = std::function<QImage(const QString& path, QSize size, sched::CancelToken cancel)>;

    static constexpr qsizetype kCacheBytes = 64 * 1024 * 1024; // [đề xuất] đủ 200 thẻ ở 200%

    explicit ThumbnailQueue(QObject* parent = nullptr);
    ~ThumbnailQueue() override; // cancelAll; việc đang chạy vẫn dùng bản chép của nguồn

    void setEmbeddedSource(EmbeddedSource source); // bỏ cache, lỗi đã ghi; hủy việc đang chạy
    void setRenderSource(RenderSource source);

    // Ảnh trong cache (đánh dấu vừa dùng), nullptr nếu chưa có. Không giữ con trỏ qua lượt vòng sự kiện.
    const QImage* image(const QString& path, QSize size) const;
    bool failed(const QString& path) const { return failed_.contains(path); }
    bool busy() const { return !running_.isEmpty(); }
    int running() const { return int(running_.size()); }

    void request(const QStringList& paths, QSize size);
    void cancelAll();
    void forget(const QString& path); // tệp đổi: bỏ ảnh, lỗi đã ghi; lần request sau làm lại

signals:
    void ready(const QString& path); // ảnh (hoặc lỗi) của path đã có

private:
    struct Key {
        QString path;
        QSize size;
        friend bool operator==(const Key&, const Key&) = default;
        friend size_t qHash(const Key& k, size_t seed = 0) { return qHashMulti(seed, k.path, k.size.width(), k.size.height()); }
    };
    struct Job {
        std::uint64_t id;
        QSize size;
        sched::CancelSource cancel;
    };

    void start(const QString& path, QSize size);
    void finished(const QString& path, std::uint64_t id, QSize size, QImage image, bool cancelled);
    void reset();

    EmbeddedSource embedded_;
    RenderSource render_;
    mutable QCache<Key, QImage> cache_{kCacheBytes};
    QHash<QString, Job> running_;
    QSet<QString> failed_;
    std::uint64_t nextId_ = 0;
};

} // namespace trion::shell
