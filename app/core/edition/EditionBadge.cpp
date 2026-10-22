#include "app/core/edition/EditionBadge.h"

#include "app/core/edition/EditionGate.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/ThemeManager.h"

#include <QEvent>
#include <QPainter>

#include <utility>

namespace trion::edition {

namespace {

const theme::ThemeManager& tm()
{
    return theme::ThemeManager::instance();
}

QFont badgeFont()
{
    return theme::font(theme::TextRole::Caption, theme::Weight::Bold);
}

bool warn(const EditionGate& g) // .ed.warn: lâu chưa xác minh giấy phép; Viewer không cần giấy phép
{
    return g.offlineDays() >= 0 && g.edition() != Edition::Viewer;
}

std::pair<tok::ColorId, tok::ColorId> colors(const EditionGate& g) // nền, chữ
{
    if (warn(g))
        return {tok::c_warn_soft, tok::c_warn};
    switch (g.edition()) {
    case Edition::Viewer:
        return {tok::c_ed_viewer, tok::c_ed_viewer_t};
    case Edition::ViewerPro:
        return {tok::c_ed_vp, tok::c_ed_vp_t};
    case Edition::Trial:
        return {tok::c_ed_trial, tok::c_ed_trial_t};
    case Edition::Education:
        return {tok::c_ed_edu, tok::c_ed_edu_t};
    case Edition::Enterprise:
        return {tok::c_ed_ent, tok::c_ed_ent_t};
    case Edition::Pro:
        break;
    }
    return {tok::c_ed_pro, tok::c_ed_pro_t};
}

} // namespace

EditionBadge::EditionBadge(EditionGate& gate, QWidget* parent) : QLabel(parent), gate_(gate)
{
    setTextFormat(Qt::PlainText);
    setFont(badgeFont()); // chữ của QLabel: trình đọc màn hình, TruncationProbe; vẽ bằng paint()
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(&gate, &EditionGate::editionChanged, this, &EditionBadge::sync);
    connect(&gate, &EditionGate::statusChanged, this, &EditionBadge::sync);
    sync();
}

QSize EditionBadge::sizeHint() const
{
    return sizeFor(gate_);
}

QSize EditionBadge::minimumSizeHint() const
{
    return sizeHint();
}

QString EditionBadge::label(const EditionGate& gate)
{
    QString s;
    switch (gate.edition()) {
    case Edition::Viewer:
        s = tr("Viewer");
        break;
    case Edition::ViewerPro:
        s = tr("Viewer Pro");
        break;
    case Edition::Pro:
        s = tr("Pro");
        break;
    case Edition::Trial:
        s = tr("Dùng thử: còn %n ngày", nullptr, gate.trialDays());
        break;
    case Edition::Education:
        s = tr("Giáo dục");
        break;
    case Edition::Enterprise:
        s = tr("Doanh nghiệp");
        break;
    }
    return warn(gate) ? tr("%1 · còn %n ngày offline", nullptr, gate.offlineDays()).arg(s) : s;
}

QSize EditionBadge::sizeFor(const EditionGate& gate)
{
    const QFontMetrics fm(badgeFont());
    return {fm.horizontalAdvance(label(gate)) + 2 * qRound(tm().px(tok::sp_3)),
            fm.height() + 2 * qRound(tm().px(tok::sp_1))};
}

void EditionBadge::paint(QPainter& p, const QRectF& r, const EditionGate& gate)
{
    const auto [bg, fg] = colors(gate);
    const qreal radius = tm().px(tok::r);
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(tm().color(bg));
    p.drawRoundedRect(r, radius, radius);
    p.setFont(badgeFont());
    p.setPen(tm().color(fg));
    p.drawText(r, Qt::AlignCenter | Qt::TextSingleLine, label(gate));
    p.restore();
}

void EditionBadge::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    paint(p, rect(), gate_);
}

void EditionBadge::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        sync();
    else if (e->type() == QEvent::StyleChange) // mật độ đổi cỡ chữ
        setFont(badgeFont());
    QLabel::changeEvent(e);
}

void EditionBadge::sync()
{
    setText(label(gate_)); // đổi chữ: updateGeometry, update
    update();              // cùng chữ, khác màu (theo phiên bản)
}

} // namespace trion::edition
