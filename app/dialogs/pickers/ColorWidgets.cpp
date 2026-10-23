#include "app/dialogs/pickers/ColorWidgets.h"

#include "app/theme/generated/Tokens.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Paint.h"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace trion::ui {

namespace {

// Giá trị của prototype không có token riêng.
constexpr int kPlaneW = 300, kPlaneH = 180; // canvas #col-hs
constexpr int kCompareCell = 30;            // .cmpbox: 2 cột 30 px, cao 30 px
constexpr int kBorder = 1;
constexpr qreal kStripe = 4;  // ByLayer: repeating-linear-gradient(45deg, … 0 4px, … 4px 8px)
constexpr qreal kMarker = 6;  // dấu chọn: vòng bán kính 6
constexpr int kFastStep = 10; // Shift + mũi tên

theme::ThemeManager& tm()
{
    return theme::ThemeManager::instance();
}
QColor col(tok::ColorId id)
{
    return tm().color(id);
}
QColor qcolor(regen::Rgb c)
{
    return QColor(c.r, c.g, c.b);
}

// Vai trò tiếp cận (QAccessibleWidget mặc định chỉ có Client): lưới là danh sách ô màu, vùng sắc độ là thanh chọn 2
// chiều, ô so sánh là hình.
QAccessibleInterface* accessibleFactory(const QString&, QObject* o)
{
    if (auto* w = qobject_cast<SwatchGrid*>(o))
        return new QAccessibleWidget(w, QAccessible::List);
    if (auto* w = qobject_cast<HueSatArea*>(o))
        return new QAccessibleWidget(w, QAccessible::Slider);
    if (auto* w = qobject_cast<ColorCompare*>(o))
        return new QAccessibleWidget(w, QAccessible::Graphic);
    return nullptr;
}

void installAccessible()
{
    static const bool installed = (QAccessible::installFactory(accessibleFactory), true);
    Q_UNUSED(installed);
}

} // namespace

void paintSwatch(QPainter& p, const QRectF& r, const Color& c)
{
    switch (c.kind()) {
    case Color::Kind::ByLayer: { // sọc chéo: dải màu đổi mỗi 4 px theo hướng 45° (lên phải)
        QLinearGradient stripes(r.topLeft(), r.topLeft() + QPointF(kStripe, -kStripe));
        stripes.setSpread(QGradient::RepeatSpread);
        stripes.setColorAt(0, col(tok::c_raised));
        stripes.setColorAt(0.4999, col(tok::c_raised));
        stripes.setColorAt(0.5, col(tok::c_border));
        stripes.setColorAt(1, col(tok::c_border));
        p.fillRect(r, stripes);
        break;
    }
    case Color::Kind::ByBlock: { // linear-gradient(135deg, --c-text 50%, --c-base 50%)
        p.fillRect(r, col(tok::c_base));
        const QPointF half[] = {r.topLeft(), r.topRight(), r.bottomLeft()};
        p.save();
        p.setPen(Qt::NoPen);
        p.setBrush(col(tok::c_text));
        p.drawPolygon(half, 3);
        p.restore();
        break;
    }
    default:
        p.fillRect(r, qcolor(c.rgb()));
        break;
    }
}

// ------------------------------------------------------------------------------------------------ SwatchGrid

SwatchGrid::SwatchGrid(QWidget* parent) : QWidget(parent)
{
    installAccessible();
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void SwatchGrid::setColors(const QList<Color>& colors, int columns)
{
    colors_ = colors;
    columns_ = std::max(1, columns);
    current_ = 0;
    hover_ = -1;
    updateGeometry();
    update();
}

void SwatchGrid::setCellSize(QSize size, int gap, bool stretch)
{
    cell_ = size;
    gap_ = gap;
    stretch_ = stretch;
    setSizePolicy(stretch ? QSizePolicy::Expanding : QSizePolicy::Fixed, QSizePolicy::Fixed);
    updateGeometry();
    update();
}

int SwatchGrid::rows() const
{
    return (count() + columns_ - 1) / columns_;
}

QRect SwatchGrid::cellRect(int i) const
{
    if (i < 0 || i >= count())
        return {};
    const int c = i % columns_, r = i / columns_;
    int x0 = kRing + c * (cell_.width() + gap_), x1 = x0 + cell_.width();
    if (stretch_) { // chia đều bề ngang cho các cột (cả khe), như grid-template-columns: repeat(n, 1fr)
        const int span = std::max(width() - 2 * kRing + gap_, columns_ * (cell_.width() + gap_));
        x0 = kRing + c * span / columns_;
        x1 = kRing + (c + 1) * span / columns_ - gap_;
    }
    return {x0, kRing + r * (cell_.height() + gap_), x1 - x0, cell_.height()};
}

int SwatchGrid::cellAt(QPoint pos) const
{
    for (int i = 0; i < count(); ++i)
        if (cellRect(i).contains(pos))
            return i;
    return -1;
}

void SwatchGrid::setCurrent(int i)
{
    if (i < 0 || i >= count() || i == current_)
        return;
    current_ = i;
    update();
}

void SwatchGrid::setSelected(const Color& c)
{
    if (c == selected_)
        return;
    selected_ = c;
    update();
}

QSize SwatchGrid::sizeHint() const
{
    const int n = std::max(1, std::min(count(), columns_)), r = std::max(1, rows());
    return {2 * kRing + n * cell_.width() + (n - 1) * gap_, 2 * kRing + r * cell_.height() + (r - 1) * gap_};
}

QSize SwatchGrid::minimumSizeHint() const
{
    return sizeHint();
}

void SwatchGrid::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    const QColor border = col(tok::c_border); // .acs outline 1 px tách ô khỏi nền
    for (int i = 0; i < count(); ++i) {
        const QRect r = cellRect(i);
        paintSwatch(p, r, colors_[i]);
        p.setPen(QPen(border, kBorder));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5));
    }
    p.setRenderHint(QPainter::Antialiasing);
    // outline 2 px ngoài ô; offset 1 px: thêm 1 px khe
    const auto ring = [&](int i, int offset, tok::ColorId id) {
        p.setPen(QPen(col(id), 2));
        p.setBrush(Qt::NoBrush);
        const qreal d = 1 + offset;
        p.drawRect(QRectF(cellRect(i)).adjusted(-d, -d, d, d));
    };
    if (hover_ >= 0) // .acs:hover
        ring(hover_, 0, tok::c_text);
    for (int i = 0; i < count(); ++i)
        if (colors_[i] == selected_) // .acs.on
            ring(i, 1, tok::c_accent);
    if (paint::focusVisible(this)) {
        if (count() > 0)
            ring(current_, 1, tok::c_focus);
        paint::drawRing(p, rect(), col(tok::c_focus));
    }
}

void SwatchGrid::setHover(int i)
{
    if (i == hover_)
        return;
    hover_ = i;
    update();
    emit hoverChanged(i);
}

void SwatchGrid::mouseMoveEvent(QMouseEvent* e)
{
    setHover(cellAt(e->position().toPoint()));
    QWidget::mouseMoveEvent(e);
}

void SwatchGrid::leaveEvent(QEvent* e)
{
    setHover(-1);
    QWidget::leaveEvent(e);
}

void SwatchGrid::mousePressEvent(QMouseEvent* e)
{
    const int i = cellAt(e->position().toPoint());
    if (e->button() != Qt::LeftButton || i < 0)
        return QWidget::mousePressEvent(e);
    current_ = i;
    update();
    emit picked(colors_[i]);
}

void SwatchGrid::mouseDoubleClickEvent(QMouseEvent* e)
{
    const int i = cellAt(e->position().toPoint());
    if (e->button() != Qt::LeftButton || i < 0)
        return QWidget::mouseDoubleClickEvent(e);
    emit activated(colors_[i]);
}

void SwatchGrid::move(int to)
{
    if (to < 0 || to >= count())
        return;
    current_ = to;
    update();
    emit currentChanged(to);
    emit picked(colors_[to]);
}

void SwatchGrid::keyPressEvent(QKeyEvent* e)
{
    if (count() == 0)
        return QWidget::keyPressEvent(e);
    const int column = current_ % columns_;
    switch (e->key()) {
    case Qt::Key_Left:
        if (column > 0)
            move(current_ - 1);
        break;
    case Qt::Key_Right:
        if (column + 1 < columns_)
            move(current_ + 1);
        break;
    case Qt::Key_Up:
        move(current_ - columns_);
        break;
    case Qt::Key_Down:
        move(current_ + columns_);
        break;
    case Qt::Key_Home:
        move(0);
        break;
    case Qt::Key_End:
        move(count() - 1);
        break;
    case Qt::Key_Space:
        emit picked(colors_[current_]);
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter: // chọn và xác nhận như nhấp đúp; không tới nút mặc định của hộp thoại
        emit activated(colors_[current_]);
        break;
    default:
        return QWidget::keyPressEvent(e);
    }
    e->accept();
}

void SwatchGrid::focusInEvent(QFocusEvent* e)
{
    if (const auto i = colors_.indexOf(selected_); i >= 0)
        current_ = int(i); // Tab vào lưới: con trỏ ở ô đang chọn
    update();
    QWidget::focusInEvent(e);
}

void SwatchGrid::focusOutEvent(QFocusEvent* e)
{
    update();
    QWidget::focusOutEvent(e);
}

// ------------------------------------------------------------------------------------------------ HueSatArea

HueSatArea::HueSatArea(QWidget* parent) : QWidget(parent)
{
    installAccessible();
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setCursor(Qt::CrossCursor);
}

void HueSatArea::setHsl(const Hsl& c)
{
    hsl_ = c;
    update();
}

QRect HueSatArea::plane() const
{
    return rect().adjusted(kRing, kRing, -kRing, -kRing);
}

QPoint HueSatArea::markerPos() const
{
    const QRect pl = plane();
    return QPointF(pl.left() + hsl_.h / 360 * pl.width(), pl.top() + (1 - hsl_.s) * pl.height()).toPoint();
}

QSize HueSatArea::sizeHint() const
{
    return {kPlaneW + 2 * kRing, kPlaneH + 2 * kRing};
}

void HueSatArea::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF pl(plane());
    const qreal rad = tm().px(tok::r);
    QPainterPath clip;
    clip.addRoundedRect(pl, rad, rad);
    p.save();
    p.setClipPath(clip);
    // Ở độ sáng l: mỗi kênh tuyến tính từng đoạn theo sắc độ (mốc mỗi 60°) và tuyến tính theo bão hòa giữa xám l
    // (s = 0) và màu bão hòa đủ (s = 1): hai gradient cho đúng HSL, không tính từng điểm ảnh.
    QLinearGradient hue(pl.topLeft(), pl.topRight());
    for (int k = 0; k <= 6; ++k)
        hue.setColorAt(k / 6.0, qcolor(toRgb({k * 60.0, 1, hsl_.l})));
    p.fillRect(pl, hue);
    QColor gray = qcolor(toRgb({0, 0, hsl_.l}));
    QLinearGradient sat(pl.topLeft(), pl.bottomLeft());
    sat.setColorAt(1, gray);
    gray.setAlpha(0);
    sat.setColorAt(0, gray);
    p.fillRect(pl, sat);
    p.restore();
    // Dấu chọn: vòng nét đậm --c-base rồi nét mảnh --c-text (prototype: đen 2 px, trắng 1 px), thấy được trên mọi màu.
    const QPointF m = markerPos();
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(col(tok::c_base), 2));
    p.drawEllipse(m, kMarker, kMarker);
    p.setPen(QPen(col(tok::c_text), 1));
    p.drawEllipse(m, kMarker, kMarker);
    if (paint::focusVisible(this))
        paint::drawRing(p, rect(), col(tok::c_focus));
}

void HueSatArea::offer(double h, double s)
{
    h = std::clamp(h, 0.0, 360.0);
    s = std::clamp(s, 0.0, 1.0);
    if (h == hsl_.h && s == hsl_.s)
        return;
    hsl_.h = h;
    hsl_.s = s;
    update();
    emit hueSatChanged(h, s);
}

void HueSatArea::pickAt(QPoint pos)
{
    const QRect pl = plane();
    const double x = std::clamp((pos.x() - pl.left()) / double(pl.width()), 0.0, 1.0);
    const double y = std::clamp((pos.y() - pl.top()) / double(pl.height()), 0.0, 1.0);
    offer(std::round(x * 360), std::round(100 - y * 100) / 100); // như prototype: độ và phần trăm nguyên
}

void HueSatArea::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton)
        return QWidget::mousePressEvent(e);
    pickAt(e->position().toPoint());
}

void HueSatArea::mouseMoveEvent(QMouseEvent* e)
{
    if (e->buttons() & Qt::LeftButton)
        pickAt(e->position().toPoint());
}

void HueSatArea::keyPressEvent(QKeyEvent* e)
{
    const int step = e->modifiers() & Qt::ShiftModifier ? kFastStep : 1;
    const double h = std::round(hsl_.h), s = std::round(hsl_.s * 100);
    switch (e->key()) {
    case Qt::Key_Left:
        offer(h - step, hsl_.s);
        break;
    case Qt::Key_Right:
        offer(h + step, hsl_.s);
        break;
    case Qt::Key_Up:
        offer(hsl_.h, (s + step) / 100);
        break;
    case Qt::Key_Down:
        offer(hsl_.h, (s - step) / 100);
        break;
    default:
        return QWidget::keyPressEvent(e);
    }
    e->accept();
}

void HueSatArea::focusInEvent(QFocusEvent* e)
{
    update();
    QWidget::focusInEvent(e);
}

void HueSatArea::focusOutEvent(QFocusEvent* e)
{
    update();
    QWidget::focusOutEvent(e);
}

// ------------------------------------------------------------------------------------------------ ColorCompare

ColorCompare::ColorCompare(QWidget* parent) : QWidget(parent)
{
    installAccessible();
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void ColorCompare::setColors(const Color& before, const Color& after)
{
    before_ = before;
    after_ = after;
    update();
}

QRect ColorCompare::beforeRect() const
{
    return {kBorder, kBorder, kCompareCell, height() - 2 * kBorder};
}

QRect ColorCompare::afterRect() const
{
    return {kBorder + kCompareCell, kBorder, width() - 2 * kBorder - kCompareCell, height() - 2 * kBorder};
}

QSize ColorCompare::sizeHint() const
{
    return {2 * kCompareCell + 2 * kBorder, kCompareCell};
}

void ColorCompare::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal rad = tm().px(tok::r);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(rect()), rad, rad);
    p.setClipPath(clip);
    paintSwatch(p, beforeRect(), before_);
    paintSwatch(p, afterRect(), after_);
    p.setClipping(false);
    p.setPen(QPen(col(tok::c_border_ctl), kBorder));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), rad, rad);
}

} // namespace trion::ui
