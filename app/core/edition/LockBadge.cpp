#include "app/core/edition/LockBadge.h"

#include "app/core/edition/EditionGate.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/ThemeManager.h"

#include <QEvent>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

using trion::theme::TextRole;
using trion::theme::ThemeManager;
using trion::theme::Weight;

namespace trion::edition {

namespace {

QFont badgeFont()
{
    return theme::font(TextRole::Badge, Weight::Bold);
}

} // namespace

LockBadge::LockBadge(ui::Edition need, Style style, QWidget* parent) : QLabel(parent), need_(need), style_(style)
{
    setFont(badgeFont()); // chữ của QLabel: trình đọc màn hình, TruncationProbe; vẽ bằng paint()
    retranslate();
    if (auto* g = EditionGate::instance())
        connect(g, &EditionGate::editionChanged, this, &LockBadge::sync);
    sync();
}

QSize LockBadge::sizeHint() const
{
    return size(need_, style_);
}

QSize LockBadge::minimumSizeHint() const
{
    return sizeHint();
}

QString LockBadge::label(ui::Edition need, Style style)
{
    if (style == Style::Ribbon && need == ui::Edition::ViewerPro)
        return tr("VP");
    return ui::RichTooltip::editionName(need);
}

QSize LockBadge::size(ui::Edition need, Style style)
{
    if (style == Style::Tag || need == ui::Edition::None)
        return ui::RichTooltip::badgeSize(need);
    const auto& t = ThemeManager::instance();
    const QFontMetrics fm(badgeFont());
    const int pad = qRound(t.px(tok::sp_1)); // lề hai bên và khoảng cách ổ khóa – chữ
    return {3 * pad + qRound(t.px(tok::sp_4)) + fm.horizontalAdvance(label(need, style)), fm.height()};
}

void LockBadge::paint(QPainter& p, const QRect& r, ui::Edition need, Style style)
{
    if (style == Style::Tag || need == ui::Edition::None) {
        ui::RichTooltip::paintBadge(p, r, need);
        return;
    }
    const auto& t = ThemeManager::instance();
    const bool vp = need == ui::Edition::ViewerPro;
    const QColor ink = t.color(vp ? tok::c_ed_vp_t : tok::c_ed_pro_t);
    const qreal pad = qRound(t.px(tok::sp_1)), side = qRound(t.px(tok::sp_4)), radius = t.px(tok::r);
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(t.color(vp ? tok::c_ed_vp : tok::c_ed_pro));
    p.drawRoundedRect(QRectF(r), radius, radius);

    // Ổ khóa .lkb .ic: hình src/lock.svg vẽ lại trong ô 8 × 8, nét 1 trên lưới nửa điểm ảnh để sắc ở 100%, 200%
    // (thu SVG 16 đơn vị về 8 px thì nét rơi giữa hai điểm ảnh, nhòe).
    p.save();
    p.translate(r.left() + pad, r.top() + std::floor((r.height() - side) / 2));
    p.scale(side / 8, side / 8);
    p.setPen(QPen(ink, 1, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    p.setBrush(Qt::NoBrush);
    QPainterPath lock;
    lock.addRect(1.5, 3.5, 5, 4);
    lock.moveTo(2.5, 3.5);
    lock.lineTo(2.5, 2.5);
    lock.arcTo(2.5, 1, 3, 3, 180, -180);
    lock.lineTo(5.5, 3.5);
    p.drawPath(lock);
    p.restore();

    p.setPen(ink);
    p.setFont(badgeFont());
    p.drawText(QRectF(r).adjusted(2 * pad + side, 0, -pad, 0), Qt::AlignCenter, label(need, style));
    p.restore();
}

void LockBadge::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    const QSize s = size(need_, style_);
    paint(p, QRect(QPoint(0, (height() - s.height()) / 2), s), need_, style_);
}

void LockBadge::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    else if (e->type() == QEvent::ParentChange) // vào bố cục sau khi tạo
        sync();
    QLabel::changeEvent(e);
}

void LockBadge::sync()
{
    const EditionGate* g = EditionGate::instance();
    const bool on = g && g->locked(need_);
    if (parentWidget())
        setVisible(on); // hiện cùng cha
    else if (!on)
        hide(); // chưa có cha: không tự mở thành cửa sổ
}

void LockBadge::retranslate()
{
    setText(label(need_, style_));
    setAccessibleName(EditionGate::availableIn(need_));
}

} // namespace trion::edition
