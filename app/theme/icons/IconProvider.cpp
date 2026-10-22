#include "app/theme/icons/IconProvider.h"

#include "app/core/UiExecutor.h"
#include "app/theme/ThemeManager.h"
#include "core/base/assert.h"

#include <QFile>
#include <QIconEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSet>
#include <QSvgRenderer>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace trion::theme {

namespace {

IconProvider* g_instance = nullptr;

constexpr quint8 kMask = 5;             // mặt nạ của biểu tượng (khóa không theo chủ đề, trạng thái)
constexpr quint8 kBadge = 6;            // mặt nạ ổ khóa của huy hiệu Locked
constexpr double kLargeStroke = 0.62;   // .ic.lg{stroke-width: calc(var(--sw) * .62)}
constexpr double kBadgeStroke = 2;      // .lkb .ic{stroke-width: 2}
constexpr double kLockedOpacity = 0.75; // .locked .ic{opacity: .75}

QString path(const QString& name)
{
    return u":/trion/icons/"_s + name + u".svg"_s;
}

// Tên trong manifest (đọc một lần): icon(), prewarm() tra ở đây thay vì hỏi hệ tệp resource mỗi lần.
const QSet<QString>& known()
{
    static const QSet<QString> set = [] {
        const QStringList all = IconProvider::names();
        return QSet<QString>(all.cbegin(), all.cend());
    }();
    return set;
}

// Nét thẻ gốc (đơn vị viewBox 16) theo cỡ logic. ponytail: hai mức (16 và ≥ 32 px, đúng 07.1 §13.1); .ic.md (20 px,
// ×0,9) và .ic.xs (9 px, ×1,5) của prototype thêm vào đây khi có widget dùng.
double strokeFor(int logicalPx)
{
    const double sw = tok::num(tok::sw);
    return logicalPx >= tok::px(tok::ic_lg, tok::Mode{}) ? sw * kLargeStroke : sw;
}

// SVG → mặt nạ alpha px×px, raster thẳng ở cỡ này. Thread-safe (raster trước chạy ở nền).
QImage renderMask(const QString& name, int px, double stroke)
{
    QFile f(path(name));
    if (px <= 0 || !f.open(QIODevice::ReadOnly))
        return {};
    QByteArray svg = f.readAll();
    const QByteArray root = "stroke-width=\"" + QByteArray::number(tok::num(tok::sw)) + '"';
    if (const qsizetype at = svg.indexOf(root); at >= 0) // lần đầu là thẻ <svg> (quy ước UI0-103)
        svg.replace(at, root.size(), "stroke-width=\"" + QByteArray::number(stroke) + '"');
    // Vẽ lên ARGB32 premultiplied (đường nhanh của raster engine; vẽ thẳng lên Alpha8 chậm hơn 2–7 lần, đo ở
    // UI0-004) rồi chỉ giữ alpha: cache tốn 1 byte/pixel.
    QImage img(px, px, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QSvgRenderer renderer(svg);
    QPainter p(&img);
    renderer.render(&p);
    p.end();
    return img.convertToFormat(QImage::Format_Alpha8);
}

// Mặt nạ × màu (alpha của màu nhân vào: Locked mờ). Kết quả ARGB32 premultiplied, chưa đặt DPR.
QImage tint(const QImage& mask, QColor color)
{
    const QRgb c = color.rgba();
    QRgb lut[256]; // độ phủ → pixel đã tô
    for (int a = 0; a < 256; ++a)
        lut[a] = qPremultiply(qRgba(qRed(c), qGreen(c), qBlue(c), (qAlpha(c) * a + 127) / 255));
    QImage out(mask.size(), QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < out.height(); ++y) {
        const uchar* m = mask.constScanLine(y);
        auto* o = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < out.width(); ++x)
            o[x] = lut[m[x]];
    }
    return out;
}

// Huy hiệu khóa: ổ khóa (bỏ lề trong suốt của SVG) sát góc dưới phải, nền dưới nó và một khe `gap` px quanh nó
// được xóa để nét của biểu tượng không lẫn vào ổ khóa.
void addBadge(QImage& img, const QImage& lockMask, QColor color, int gap)
{
    QRect box;
    for (int y = 0; y < lockMask.height(); ++y)
        for (int x = 0; x < lockMask.width(); ++x)
            if (lockMask.constScanLine(y)[x])
                box |= QRect(x, y, 1, 1);
    const QPoint at = QPoint(img.width() - 1, img.height() - 1) - box.bottomRight();
    QPainter p(&img);
    p.setCompositionMode(QPainter::CompositionMode_Clear);
    p.fillRect(box.translated(at).adjusted(-gap, -gap, 0, 0), Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.drawImage(at, tint(lockMask, color));
}

QColor colorFor(IconState s)
{
    const ThemeManager& t = ThemeManager::instance();
    switch (s) {
    case IconState::Hover:
        return t.color(tok::c_text);
    case IconState::On:
        return t.color(tok::c_on);
    case IconState::Disabled:
        return t.color(tok::c_text_dis);
    case IconState::Locked: {
        QColor c = t.color(tok::c_text_2);
        c.setAlphaF(float(c.alphaF() * kLockedOpacity));
        return c;
    }
    case IconState::Normal:
        break;
    }
    return t.color(tok::c_text_2);
}

class Engine final : public QIconEngine {
public:
    Engine(QString name, bool locked) : name_(std::move(name)), locked_(locked) {}

    QIconEngine* clone() const override { return new Engine(name_, locked_); }
    QString iconName() override { return name_; }
    bool isNull() override { return false; }
    QSize actualSize(const QSize& size, QIcon::Mode, QIcon::State) override
    {
        const int side = qMin(size.width(), size.height());
        return {side, side};
    }
    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1);
    }
    QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        const IconState s = locked_                   ? IconState::Locked
                            : mode == QIcon::Disabled ? IconState::Disabled
                            : state == QIcon::On      ? IconState::On
                            : mode == QIcon::Normal   ? IconState::Normal
                                                      : IconState::Hover; // Active (rê), Selected
        return IconProvider::instance().pixmap(name_, qMin(size.width(), size.height()), scale, s);
    }
    void paint(QPainter* p, const QRect& rect, QIcon::Mode mode, QIcon::State state) override
    {
        const int side = qMin(rect.width(), rect.height());
        QRect r(0, 0, side, side);
        r.moveCenter(rect.center());
        p->drawPixmap(r, scaledPixmap(r.size(), mode, state, p->device()->devicePixelRatio()));
    }

private:
    QString name_;
    bool locked_;
};

} // namespace

IconProvider::IconProvider(QObject* parent) : QObject(parent)
{
    TRION_ASSERT_UI_THREAD();
    g_instance = this;
}

IconProvider::~IconProvider()
{
    cancel_.cancel();
    if (g_instance == this)
        g_instance = nullptr;
}

IconProvider& IconProvider::instance()
{
    TRION_CHECK(g_instance && "chưa tạo IconProvider");
    return *g_instance;
}

QIcon IconProvider::icon(const QString& name, bool locked)
{
    if (!known().contains(name)) {
        qWarning("IconProvider: không có biểu tượng \"%s\"", qUtf8Printable(name));
        return {};
    }
    return QIcon(new Engine(name, locked));
}

QStringList IconProvider::names(bool viewerOnly)
{
    static const auto lists = [] {
        QFile f(u":/trion/icons/manifest.json"_s);
        std::pair<QStringList, QStringList> all; // mọi tên, bộ Viewer
        if (f.open(QIODevice::ReadOnly))
            for (const QJsonValue v : QJsonDocument::fromJson(f.readAll())["icons"_L1].toArray()) {
                all.first << v["name"_L1].toString();
                if (v["viewer"_L1].toBool())
                    all.second << all.first.last();
            }
        return all;
    }();
    return viewerOnly ? lists.second : lists.first;
}

QPixmap IconProvider::pixmap(const QString& name, int logicalPx, qreal dpr, IconState state)
{
    TRION_ASSERT_UI_THREAD();
    const int px = qRound(logicalPx * dpr);
    const Key key{name, px, dpr, quint8(ThemeManager::instance().mode().light), quint8(state)};
    if (const QImage* hit = cache_.object(key))
        return QPixmap::fromImage(*hit);
    // ponytail: mặt nạ chưa có (chưa raster trước) thì raster ngay trên luồng UI, ~0,1–0,2 ms mỗi biểu tượng. Màn
    // hình mở ra nhiều biểu tượng mới cùng lúc (tab ribbon, UI0-106) thì cho prewarm() nhận danh sách tên của nó.
    const QImage m = mask({name, px, dpr, 0, kMask}, strokeFor(logicalPx));
    if (m.isNull())
        return {};
    QImage img = tint(m, colorFor(state));
    if (state == IconState::Locked)
        addBadge(img, mask({u"lock"_s, px / 2, dpr, 0, kBadge}, kBadgeStroke),
                 ThemeManager::instance().color(tok::c_text), qMax(1, qRound(dpr)));
    img.setDevicePixelRatio(dpr);
    ++tints_;
    put(key, img);
    return QPixmap::fromImage(img);
}

QImage IconProvider::mask(const Key& key, double stroke)
{
    if (const QImage* hit = cache_.object(key))
        return *hit;
    QImage m = renderMask(key.name, key.px, stroke);
    if (!m.isNull()) {
        ++rasters_;
        put(key, m);
    }
    return m;
}

void IconProvider::put(const Key& key, const QImage& image)
{
    cache_.insert(key, new QImage(image), image.sizeInBytes());
}

void IconProvider::prewarm(qreal dpr, const QStringList& list)
{
    TRION_ASSERT_UI_THREAD();
    const ThemeManager& t = ThemeManager::instance();
    const int sizes[] = {qRound(t.px(tok::ic)), qRound(t.px(tok::ic_lg))};
    const ui::Receiver rx(this);
    const sched::CancelToken token = cancel_.token();
    QStringList todo = list;
    todo.removeDuplicates();
    for (const QString& name : std::as_const(todo)) {
        const auto cached = [&](int logical) { return cache_.contains({name, qRound(logical * dpr), dpr, 0, kMask}); };
        if (!known().contains(name) || std::ranges::all_of(sizes, cached))
            continue;
        sched::submit(sched::Priority::Background, [=, this] {
            QList<std::pair<Key, QImage>> done; // nền: không chạm `this`
            for (const int logical : sizes) {
                if (token.cancelled())
                    return;
                const int px = qRound(logical * dpr);
                done.append({Key{name, px, dpr, 0, kMask}, renderMask(name, px, strokeFor(logical))});
            }
            ui::post(rx, [this, done = std::move(done)] {
                for (const auto& [key, m] : done) {
                    ++rasters_;
                    if (!cache_.contains(key))
                        put(key, m);
                }
            });
        });
    }
}

} // namespace trion::theme
