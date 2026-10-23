#include "app/widgets/views/TextCache.h"

#include <QFontMetricsF>
#include <QPainter>

namespace trion::ui {

qreal TextCache::draw(QPainter& p, const QFont& font, const QRect& box, const QString& text, Qt::TextElideMode mode,
                      Qt::Alignment align)
{
    if (box.width() <= 0)
        return 0;
    if (font != font_) {
        cache_.clear();
        font_ = font;
    }
    const QFontMetricsF fm(font);
    const QString key = QString::number(box.width()) + u'|' + QString::number(int(mode)) + u'|' + text;
    QStaticText* line = cache_.object(key);
    if (!line) {
        line = new QStaticText(text);
        line->setTextFormat(Qt::PlainText);
        line->prepare(QTransform(), font);
        // Bề rộng của QStaticText gồm phần nét của chữ cuối nhô quá bước tiến (~1 px, horizontalAdvance không tính):
        // so bằng nó để mực không ra ngoài ô.
        if (line->size().width() > box.width()) {
            line->setText(fm.elidedText(text, mode, box.width()));
            line->prepare(QTransform(), font);
        }
        cache_.insert(key, line);
    }
    p.setFont(font);
    const qreal w = line->size().width();
    p.drawStaticText(QPointF(align & Qt::AlignRight ? box.right() + 1 - w : box.left(),
                             box.top() + (box.height() - fm.height()) / 2),
                     *line);
    return w;
}

} // namespace trion::ui
