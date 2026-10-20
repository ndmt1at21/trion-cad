// Mẫu đúng: token, số 0, phép gọi phương thức, chú thích có mã màu (#FF0000, rgb(1,2,3), padding: 4px).
#include "app/theme/ThemeManager.h"
#define TRION_X 1
#ifdef TRION_X
#endif

using trion::theme::ThemeManager;

/* chú thích nhiều dòng: QColor(255, 0, 0), setSpacing(4)
   font-size: 12px */
void paint(QPainter& p, QWidget* w, QVBoxLayout* l, QFont& f, const QColor& c, const QStringList& families)
{
    const ThemeManager& t = ThemeManager::instance();
    p.setPen(t.color(tok::c_accent));
    p.setPen(QPen(t.color(tok::c_focus), 2)); // độ dày nét, không phải khoảng cách
    p.fillRect(w->rect(), QColor(c.red(), c.green(), c.blue(), c.alpha()));
    p.setBrush(Qt::transparent);
    p.setBrush(Qt::NoBrush);
    const QRgb v = c.rgba() + c.rgb() + qRgba(c.red(), 0, 0, 255) * 0;
    w->setStyleSheet("QToolButton#add { padding: 0 var(--sp-2); border: 1px solid var(--c-border); margin: 0; }");
    w->setStyleSheet(u"QLabel { color: %1; font-size: var(--fs-s); }"_s.arg(t.color(tok::c_text).name()));
    QFont font(families);
    font.setPointSizeF(t.font(tok::fs_s).pointSizeF());
    f.setPixelSize(qRound(t.px(tok::fs)));
    l->setSpacing(0);
    l->setSpacing(qRound(t.px(tok::sp_2)));
    l->setContentsMargins(0, 0, 0, 0);
    const int m = qRound(t.px(tok::sp_4));
    l->setContentsMargins(m, 0, m, 0);
    l->addSpacing(0);
    p.setPen(QColor(255, 0, 0)); // style-ok: mẫu lỗi cố ý trong test dò màu ACI
    (void)v;
}
