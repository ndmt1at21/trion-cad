#pragma once
// UI0-004 · Bộ biểu tượng lúc chạy (07.1 §13.5): tô màu theo chủ đề và trạng thái, cache raster theo DPI.
//
// SVG của bộ (UI0-103, nhúng ở :/trion/icons/<tên>.svg, chỉ currentColor/none) được raster thẳng ở đúng cỡ pixel
// thiết bị thành mặt nạ alpha: 16 px logic ở 100/125/150/175/200% là 16/20/24/28/32 px, không phóng bitmap; cỡ
// logic ≥ --ic-lg (32 px) dùng nét mảnh --sw × 0,62 như .ic.lg của prototype. Ảnh hiện = mặt nạ × màu token:
//   Normal --c-text-2 · Hover --c-text · On --c-on · Disabled --c-text-dis (như .ib của prototype)
//   Locked --c-text-2 mờ 75% (.locked) + huy hiệu ổ khóa --c-text ở góc dưới phải: khác Disabled cả về hình.
// Đổi chủ đề chỉ tô lại từ mặt nạ (vài µs mỗi biểu tượng), không raster lại SVG. Mặt nạ và ảnh đã tô chung một
// cache LRU ≤ 16 MB, khóa (tên, cỡ px thiết bị, DPR, chủ đề, trạng thái).
//
// Dùng, trên luồng UI, sau ThemeManager và UiExecutor:
//   trion::theme::IconProvider icons;                       // main giữ suốt đời ứng dụng; test tạo riêng
//   icons.prewarm(window.devicePixelRatio());               // bộ Viewer raster ở nền
//   button->setIcon(IconProvider::icon(u"layers"_s));       // Disabled → Disabled, On → On, Active/Selected → Hover
//   rbButton->setIcon(IconProvider::icon(u"publish"_s, true)); // tính năng khóa theo phiên bản: mọi mode vẽ Locked
//   p.drawPixmap(r, IconProvider::instance().pixmap(u"lock"_s, 16, dpr, IconState::Hover)); // widget tự vẽ
// Widget tự vẽ lấy pixmap trong paintEvent, không giữ lại: đổi chủ đề thì Qt gửi PaletteChange, lần vẽ sau lấy
// bản của chủ đề mới. Đổi màn hình (DPR mới): gọi lại prewarm(dpr mới); biểu tượng khác raster khi vẽ lần đầu.

#include "core/base/sched/Scheduler.h"

#include <QCache>
#include <QIcon>
#include <QImage>
#include <QObject>
#include <QPixmap>
#include <QString>
#include <QStringList>

namespace trion::theme {

enum class IconState : quint8 { Normal, Hover, On, Disabled, Locked };

class IconProvider : public QObject {
    Q_OBJECT
public:
    static constexpr qsizetype kCacheBytes = 16 * 1024 * 1024; // [đề xuất] UI0-004 tiêu chí 3

    explicit IconProvider(QObject* parent = nullptr);
    ~IconProvider() override; // hủy việc raster trước còn chờ; kết quả về muộn bị bỏ

    // IconProvider tạo gần nhất còn sống. Chưa có thì dừng (TRION_CHECK).
    static IconProvider& instance();

    // QIcon vẽ qua instance() lúc vẽ (theo chủ đề lúc đó). Tên không có trong bộ: QIcon rỗng và cảnh báo.
    static QIcon icon(const QString& name, bool locked = false);
    // Tên trong manifest.json (UI0-103); viewerOnly: bộ Viewer (raster trước).
    static QStringList names(bool viewerOnly = false);

    // Ảnh đã tô: vuông cạnh logicalPx, cỡ pixel qRound(logicalPx * dpr), devicePixelRatio = dpr. Luồng UI.
    // Tên lạ: QPixmap rỗng.
    QPixmap pixmap(const QString& name, int logicalPx, qreal dpr, IconState state);

    // Raster mặt nạ bộ Viewer ở --ic và --ic-lg với dpr này trên luồng nền (sched, Background); kết quả vào cache
    // qua ui::post. Không chặn: chỉ gửi việc.
    void prewarm(qreal dpr);

    // Số đo cho test và UI0-105. Chỉ đọc trên luồng UI.
    struct Stats {
        quint64 rasters = 0; // SVG đã raster thành mặt nạ (cả ở nền, đếm khi kết quả về)
        quint64 tints = 0;   // ảnh đã tô từ mặt nạ
        qsizetype bytes = 0; // cache đang giữ
    };
    Stats stats() const { return {rasters_, tints_, cache_.totalCost()}; }

private:
    struct Key {
        QString name;
        int px; // cạnh, pixel thiết bị
        qreal dpr;
        quint8 light; // ảnh đã tô: chủ đề Sáng; mặt nạ: 0
        quint8 kind;  // IconState, hoặc mặt nạ (kMask, kBadge trong .cpp)
        friend bool operator==(const Key&, const Key&) = default;
        friend size_t qHash(const Key& k, size_t seed = 0)
        {
            return qHashMulti(seed, k.name, k.px, k.dpr, k.light, k.kind);
        }
    };

    QImage mask(const Key& key, double stroke); // từ cache, thiếu thì raster
    void put(const Key& key, const QImage& image);

    QCache<Key, QImage> cache_{kCacheBytes}; // giá = số byte
    quint64 rasters_ = 0;
    quint64 tints_ = 0;
    sched::CancelSource cancel_;
};

} // namespace trion::theme
