#include "app/widgets/dock/PaletteHeader.h"

#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"

#include <QBoxLayout>
#include <QDockWidget>
#include <QEvent>
#include <QLabel>
#include <QPainter>

using namespace Qt::StringLiterals;

namespace trion::ui {

namespace {

theme::ThemeManager& tm()
{
    return theme::ThemeManager::instance();
}
int px(tok::PxId id)
{
    return qRound(tm().px(id));
}

// Giá trị của prototype không có token riêng.
constexpr int kBorder = 1; // nét chia đáy 1 px (.pal-tabs border-bottom)
constexpr int kAccent = 2; // gạch tab đang mở (.pal-tab::after height:2px)

} // namespace

PaletteHeader::PaletteHeader(QDockWidget* dock) : QWidget(dock)
{
    title_ = new QLabel(dock->windowTitle(), this);
    pin_ = new ToggleButton({}, Button::Kind::Icon, this);
    pin_->setIconName(u"pin"_s);
    pin_->setChecked(true);
    menu_ = new Button({}, Button::Kind::Icon, this);
    menu_->setIconName(u"more"_s);
    close_ = new Button({}, Button::Kind::Icon, this);
    close_->setIconName(u"x"_s);

    auto* row = new QHBoxLayout(this);
    row->setSpacing(0);
    row->addWidget(title_);
    row->addStretch(); // .acts{margin-left:auto; padding-left:4px}: chỗ trống, ít nhất --sp-2 (applyMetrics)
    row->addWidget(pin_);
    row->addWidget(menu_);
    row->addWidget(close_);
    applyMetrics();
    retranslate();

    connect(dock, &QWidget::windowTitleChanged, title_, &QLabel::setText);
    connect(pin_, &QAbstractButton::toggled, this, &PaletteHeader::retranslate);
    connect(close_, &QAbstractButton::clicked, dock, &QWidget::close);
}

QSize PaletteHeader::sizeHint() const
{
    return {QWidget::sizeHint().width(), px(tok::h_ctl_l)}; // 28 px như .pal-tabs (prototype viết thẳng)
}

QSize PaletteHeader::minimumSizeHint() const
{
    return {QWidget::minimumSizeHint().width(), px(tok::h_ctl_l)};
}

void PaletteHeader::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), tm().color(tok::c_chrome));
    const QRect tab(0, 0, title_->geometry().right() + 1, height() - kBorder);
    p.fillRect(tab, tm().color(tok::c_base));
    p.fillRect(QRect(tab.left(), 0, tab.width(), kAccent), tm().color(tok::c_accent));
    p.fillRect(QRect(0, height() - kBorder, width(), kBorder), tm().color(tok::c_border));
}

void PaletteHeader::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    else if (e->type() == QEvent::StyleChange)
        applyMetrics();
    QWidget::changeEvent(e);
}

void PaletteHeader::retranslate()
{
    const bool pinned = pin_->isChecked();
    pin_->setAccessibleName(pinned ? tr("Tự ẩn palette") : tr("Ghim palette"));
    pin_->setToolTip(pinned ? tr("Tự ẩn") : tr("Ghim"));
    menu_->setAccessibleName(tr("Tùy chọn palette"));
    menu_->setToolTip(tr("Tùy chọn palette"));
    close_->setAccessibleName(tr("Đóng palette"));
    close_->setToolTip(tr("Đóng"));
}

void PaletteHeader::applyMetrics()
{
    const int pad = px(tok::sp_4) + px(tok::sp_1); // .pal-tab{padding:0 10px}
    title_->setContentsMargins(pad, 0, pad, 0);
    QFont f = tm().font(tok::fs);
    f.setWeight(QFont::Weight(qRound(tok::num(tok::fw_m))));
    title_->setFont(f);
    const int ib = px(tok::strip); // .pal-tabs .acts .ib: 22 px cả hai mật độ (prototype viết thẳng, trùng --strip)
    for (Button* b : {static_cast<Button*>(pin_), menu_, close_})
        b->setFixedSize(ib, ib);
    auto* row = static_cast<QHBoxLayout*>(layout());
    row->setContentsMargins(0, 0, px(tok::sp_1), kBorder); // .acts{padding-right:2px}
    row->itemAt(1)->spacerItem()->changeSize(px(tok::sp_2), 0, QSizePolicy::MinimumExpanding);
    row->invalidate();
    updateGeometry();
}

} // namespace trion::ui
