#include "app/widgets/progress/Spinner.h"

#include "app/theme/generated/Tokens.h"
#include "app/theme/ThemeManager.h"

#include <QPainter>

namespace trion::ui {

namespace {

// Giá trị của prototype không có token riêng (css/app.css .spin).
constexpr int kSize = 14;
constexpr int kPen = 2;
constexpr int kTurnMs = 800;

} // namespace

Spinner::Spinner(QWidget* parent) : QWidget(parent)
{
    setFixedSize(kSize, kSize);
    anim_.setStartValue(0.0);
    anim_.setEndValue(360.0);
    anim_.setDuration(kTurnMs);
    anim_.setLoopCount(-1);
    connect(&anim_, &QVariantAnimation::valueChanged, this, qOverload<>(&QWidget::update));
}

void Spinner::showEvent(QShowEvent* e)
{
    if (theme::ThemeManager::instance().ms(tok::t) > 0)
        anim_.start();
    QWidget::showEvent(e);
}

void Spinner::hideEvent(QHideEvent* e)
{
    anim_.stop();
    QWidget::hideEvent(e);
}

void Spinner::paintEvent(QPaintEvent*)
{
    const auto& t = theme::ThemeManager::instance();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal h = kPen / 2.0;
    const QRectF r = QRectF(rect()).adjusted(h, h, -h, -h);
    p.setPen(QPen(t.color(tok::c_active), kPen));
    p.drawEllipse(r);
    p.setPen(QPen(t.color(tok::c_accent), kPen, Qt::SolidLine, Qt::FlatCap));
    const double turn = spinning() ? anim_.currentValue().toDouble() : 0;
    p.drawArc(r, qRound((45 - turn) * 16), 90 * 16); // border-top của vòng: cung 45°–135°, quay chiều kim đồng hồ
}

} // namespace trion::ui
