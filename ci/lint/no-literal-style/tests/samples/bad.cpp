// Mẫu sai: mỗi dòng có dấu "lỗi" phải bị báo, các dòng khác thì không.
#include <QtWidgets>

void paint(QPainter& p, QWidget* w, QVBoxLayout* l, QFont& f)
{
    p.setPen(QColor("#3CC8C0"));                              // lỗi
    p.fillRect(w->rect(), QColor(26, 29, 33));                 // lỗi
    p.setBrush(QColor(0x1A1D21));                              // lỗi
    QColor accent(60, 200, 192);                               // lỗi
    QColor named{Qt::red};                                     // lỗi
    p.setPen(Qt::white);                                       // lỗi
    p.setPen(QColor::fromRgb(255, 0, 0));                      // lỗi
    p.setPen(QColorConstants::Svg::teal);                      // lỗi
    const QRgb rgb = qRgba(0, 0, 0, 128);                      // lỗi
    w->setStyleSheet("QLabel { color: #E3E6EA; }");            // lỗi
    w->setStyleSheet("QLabel { background: rgba(0,0,0,.5); }"); // lỗi
    w->setStyleSheet("QLabel { border: 1px solid red; }");     // lỗi
    w->setStyleSheet(u"QLabel { font-size: 12px; }"_s);        // lỗi
    w->setStyleSheet("QLabel { font: 600 10.5pt 'Plex'; }");   // lỗi
    w->setStyleSheet("QLabel { padding: 0 8px; }");            // lỗi
    w->setStyleSheet("QMenu::item { margin-left: 4px; }");     // lỗi
    w->setStyleSheet("QCheckBox { spacing: 6px; }");           // lỗi
    f.setPointSize(10);                                        // lỗi
    f.setPointSizeF(10.5);                                     // lỗi
    f.setPixelSize(12);                                        // lỗi
    QFont mono("IBM Plex Mono", 11);                           // lỗi
    l->setSpacing(4);                                          // lỗi
    l->setContentsMargins(8, 0, 8, 0);                         // lỗi
    l->addSpacing(12);                                         // lỗi
    const QMargins m(0, 2, 0, 2);                              // lỗi
    (void)rgb;
    (void)m;
}
