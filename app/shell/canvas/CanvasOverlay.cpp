#include "app/shell/canvas/CanvasOverlay.h"

#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Paint.h"

#include <QPainter>
#include <QPen>

namespace trion::shell {

namespace {

using theme::Sem;

theme::ThemeManager& tm()
{
    return theme::ThemeManager::instance();
}

int px(tok::PxId id)
{
    return qRound(tm().px(id));
}

// Vòng bận như .spin của prototype (css/app.css; app/widgets/progress/Spinner.cpp): 14 px, nét 2 px, cung trên quay;
// giá trị prototype không có token riêng.
constexpr int kRing = 14;
constexpr int kRingPen = 2;
// Viền Crossing đứt như prototype (canvas.js: setLineDash([6, 4])), đơn vị là bề rộng nét 1 px.
const QList<qreal> kDash{6, 4};

QRect square(QPoint c, int half)
{
    return {c.x() - half, c.y() - half, 2 * half + 1, 2 * half + 1};
}

QRect horizontal(const Overlay& o)
{
    return {o.mouse->x() - o.crossHalf, o.mouse->y(), 2 * o.crossHalf + 1, 1};
}

QRect vertical(const Overlay& o)
{
    return {o.mouse->x(), o.mouse->y() - o.crossHalf, 1, 2 * o.crossHalf + 1};
}

// Huy hiệu (prototype: ô 18 px ở x + 10, y − 24): ô biểu tượng --ic với viền 1 px mỗi bên.
QRect badgeRect(QPoint m)
{
    const int side = px(tok::ic) + 2;
    return {m.x() + px(tok::sp_4) + px(tok::sp_1), m.y() - px(tok::sp_7), side, side};
}

// Vòng bận ở dưới phải điểm nóng, như con trỏ "đang chạy nền" của hệ điều hành.
QRect ringRect(QPoint m)
{
    return {m.x() + px(tok::sp_4), m.y() + px(tok::sp_4), kRing, kRing};
}

// Viền 1 px trong mép r bằng fillRect: sắc ở mọi tỉ lệ, đếm được điểm ảnh (cạnh n điểm: 4n − 4 điểm).
void outline(QPainter& p, const QRect& r, const QColor& c)
{
    p.fillRect(QRect(r.left(), r.top(), r.width(), 1), c);
    p.fillRect(QRect(r.left(), r.bottom(), r.width(), 1), c);
    p.fillRect(QRect(r.left(), r.top() + 1, 1, r.height() - 2), c);
    p.fillRect(QRect(r.right(), r.top() + 1, 1, r.height() - 2), c);
}

bool windowStyle(SelectMode m)
{
    return m == SelectMode::Window || m == SelectMode::WPolygon;
}

// Window: viền liền, nền màu A; Crossing: viền đứt, nền màu B (07.1 §9.3: khác cả kiểu viền, không chỉ màu); Fence:
// đường đứt hở, không nền. Khung chữ nhật vẽ không khử răng cưa cho nét 1 px sắc; lasso, đa giác khử răng cưa.
void paintShape(QPainter& p, const Overlay& o)
{
    const bool window = windowStyle(o.mode), fence = o.mode == SelectMode::Fence;
    QPen pen(tm().color(window ? Sem::SelectionWindow : Sem::SelectionCrossing), 1);
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::MiterJoin);
    if (!window)
        pen.setDashPattern(kDash);
    p.setPen(pen);
    p.setBrush(fence ? QBrush() : QBrush(tm().color(window ? Sem::SelectionWindowFill : Sem::SelectionCrossingFill)));
    p.setRenderHint(QPainter::Antialiasing, o.mode != SelectMode::Window && o.mode != SelectMode::Crossing);
    const QPolygonF poly = o.shape.translated(0.5, 0.5); // nét 1 px nằm đúng hàng điểm ảnh của đỉnh
    if (fence)
        p.drawPolyline(poly);
    else
        p.drawPolygon(poly);
}

} // namespace

bool hasCross(CursorState s)
{
    return s == CursorState::Idle || s == CursorState::PickPoint || s == CursorState::Busy;
}

bool hasPickbox(CursorState s)
{
    return s == CursorState::Idle || s == CursorState::PickObject;
}

bool drawsCursor(CursorState s)
{
    return s != CursorState::Pan && s != CursorState::Zoom;
}

QColor canvasColor(tok::ColorId id)
{
    return theme::toQColor(tok::color(id, tok::Mode{}));
}

void paintOverlay(QPainter& p, const Overlay& o)
{
    p.save();
    if (o.shape.size() >= 2)
        paintShape(p, o);
    if (o.mouse && drawsCursor(o.cursor)) {
        const QPoint m = *o.mouse;
        const QColor cross = canvasColor(tok::c_cross);
        p.setRenderHint(QPainter::Antialiasing, false);
        if (hasCross(o.cursor)) {
            p.fillRect(horizontal(o), cross);
            p.fillRect(vertical(o), cross);
            if (o.apertureHalf > 0)
                outline(p, square(m, o.apertureHalf), cross);
        }
        if (hasPickbox(o.cursor) && o.pickHalf > 0)
            outline(p, square(m, o.pickHalf), cross);
        if (!o.badge.isEmpty()) { // prototype: nền --c-dyn-bg, viền --c-dyn-b, ký hiệu --c-dyn-t
            const QRect b = badgeRect(m);
            p.fillRect(b, tm().color(Sem::DynTooltipBg));
            outline(p, b, tm().color(Sem::DynTooltipBorder));
            const int ic = px(tok::ic);
            const QPixmap icon = theme::IconProvider::instance().pixmap(o.badge, ic, p.device()->devicePixelRatio(),
                                                                       theme::IconState::Normal);
            p.drawPixmap(QRect(b.left() + 1, b.top() + 1, ic, ic), ui::paint::tinted(icon, tm().color(Sem::DynTooltipText)));
        }
        if (o.cursor == CursorState::Busy) { // .spin: vòng --c-active, cung trên --c-accent
            const qreal h = kRingPen / 2.0;
            const QRectF r = QRectF(ringRect(m)).adjusted(h, h, -h, -h);
            p.setRenderHint(QPainter::Antialiasing);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(canvasColor(tok::c_active), kRingPen));
            p.drawEllipse(r);
            p.setPen(QPen(canvasColor(tok::c_accent), kRingPen, Qt::SolidLine, Qt::FlatCap));
            p.drawArc(r, qRound((45 - o.busyTurn) * 16), 90 * 16);
        }
    }
    p.restore();
}

QRegion cursorRegion(const Overlay& o)
{
    if (!o.mouse || !drawsCursor(o.cursor))
        return {};
    const QPoint m = *o.mouse;
    QRegion r;
    if (hasCross(o.cursor)) {
        r += horizontal(o);
        r += vertical(o);
        if (o.apertureHalf > 0)
            r += square(m, o.apertureHalf);
    }
    if (hasPickbox(o.cursor) && o.pickHalf > 0)
        r += square(m, o.pickHalf);
    if (!o.badge.isEmpty())
        r += badgeRect(m);
    if (o.cursor == CursorState::Busy)
        r += ringRect(m);
    // Lề 1 px: ở tỉ lệ lẻ (125%, 150%) nét làm tròn ra điểm ảnh thiết bị kề bên.
    QRegion out;
    for (const QRect& x : r)
        out += x.adjusted(-1, -1, 1, 1);
    return out;
}

QRegion shapeRegion(const Overlay& o)
{
    if (o.shape.size() < 2)
        return {};
    return QRegion(o.shape.boundingRect().toAlignedRect().adjusted(-2, -2, 2, 2)); // nét lệch 0,5 px và khử răng cưa
}

} // namespace trion::shell
