// TextCache (app/widgets/views/TextCache.h): chữ một dòng đã shaping cho delegate tự vẽ (tab Start, palette Layer của
// Viewer). fitAndElide: vừa ô thì vẽ đủ, trả bề rộng chữ; hẹp thì rút gọn, mực nằm trong ô (cả chiều dọc).
// alignRight: căn phải sát mép phải ô. reuse: một bản mỗi (bề rộng, cách rút gọn, chữ), vẽ lại không thêm; đổi font
// bỏ hết; không quá kCapacity bản. emptyBox: ô rộng 0 không vẽ.
#include "app/widgets/views/TextCache.h"

#include "app/theme/fonts/FontRegistry.h"

#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QtTest>

using namespace Qt::StringLiterals;
using trion::ui::TextCache;

namespace {

const QSize kCanvas(240, 48);
const QString kText = u"Mat-bang-tang-1.dwg"_s; // i18n-ok: chữ mẫu của test

QImage blank()
{
    QImage img(kCanvas, QImage::Format_RGB32);
    img.fill(Qt::white);
    return img;
}

// Hộp bao các điểm ảnh có mực (khác nền trắng).
QRect ink(const QImage& img)
{
    QRect r;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (img.pixel(x, y) != qRgb(255, 255, 255))
                r |= QRect(x, y, 1, 1);
    return r;
}

} // namespace

class TextCacheTest : public QObject {
    Q_OBJECT
    QFont font_;

    // Vẽ một lần bằng cache lên ảnh trắng; trả bề rộng draw() báo.
    qreal paint(TextCache& cache, QImage& img, const QRect& box, const QString& text, Qt::TextElideMode mode,
                Qt::Alignment align = Qt::AlignLeft)
    {
        QPainter p(&img);
        p.setPen(Qt::black);
        return cache.draw(p, font_, box, text, mode, align);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(trion::theme::loadFonts()); // Plex nhúng: số đo không phụ thuộc font của máy
        font_ = QFont(u"IBM Plex Sans"_s);
        font_.setPixelSize(12);
    }

    void fitAndElide()
    {
        TextCache cache;
        const qreal full = QFontMetricsF(font_).horizontalAdvance(kText);
        QImage img = blank();
        const QRect wide(8, 4, qCeil(full) + 16, 20);
        const qreal w = paint(cache, img, wide, kText, Qt::ElideRight); // bước tiến + phần nhô của chữ cuối
        QVERIFY2(w >= full && w <= full + 2, qPrintable(QString::number(w)));
        const QRect drawn = ink(img);
        QVERIFY2(drawn.left() >= wide.left() && drawn.right() <= wide.left() + qCeil(w) && drawn.width() > full - 4,
                 qPrintable(u"%1…%2"_s.arg(drawn.left()).arg(drawn.right())));

        for (const Qt::TextElideMode mode : {Qt::ElideRight, Qt::ElideMiddle}) {
            img = blank();
            const QRect narrow(8, 24, qFloor(full / 2), 20);
            const qreal cw = paint(cache, img, narrow, kText, mode);
            QVERIFY2(cw > 0 && cw <= narrow.width() + 1, qPrintable(QString::number(cw)));
            const QRect cut = ink(img); // rút gọn: mực trong ô (ElideMiddle: chữ cuối nhô tối đa 1 px)
            QVERIFY2(cut.left() >= narrow.left() - 1 && cut.right() <= narrow.right() + 1 &&
                         cut.top() >= narrow.top() && cut.bottom() <= narrow.bottom(),
                     qPrintable(u"%1,%2 %3×%4"_s.arg(cut.x()).arg(cut.y()).arg(cut.width()).arg(cut.height())));
        }
    }

    void alignRight()
    {
        TextCache cache;
        QImage img = blank();
        const QRect box(8, 4, 200, 20);
        const qreal w = paint(cache, img, box, u"1.250"_s, Qt::ElideRight, Qt::AlignRight);
        const QRect drawn = ink(img);
        QVERIFY(w > 0 && w < box.width() / 2);
        QVERIFY2(drawn.right() >= box.right() - 2 && drawn.right() <= box.right() && drawn.left() > box.center().x(),
                 qPrintable(u"%1…%2"_s.arg(drawn.left()).arg(drawn.right())));
    }

    void reuse()
    {
        TextCache cache;
        QImage img = blank();
        const QRect box(0, 0, 120, 20);
        paint(cache, img, box, kText, Qt::ElideRight);
        paint(cache, img, box, kText, Qt::ElideRight);
        paint(cache, img, box.adjusted(0, 24, 0, 24), kText, Qt::ElideRight); // cùng bề rộng, chỗ khác: dùng lại
        QCOMPARE(cache.size(), 1);
        paint(cache, img, box.adjusted(0, 0, -40, 0), kText, Qt::ElideRight); // bề rộng khác: rút gọn khác
        paint(cache, img, box, kText, Qt::ElideMiddle);
        QCOMPARE(cache.size(), 3);

        QFont bold = font_;
        bold.setWeight(QFont::DemiBold);
        QPainter p(&img);
        cache.draw(p, bold, box, kText); // đổi font (theme, mật độ): bỏ hết
        QCOMPARE(cache.size(), 1);
        for (int i = 0; i < TextCache::kCapacity + 100; ++i)
            cache.draw(p, bold, box, QString::number(i));
        QVERIFY(cache.size() <= TextCache::kCapacity);
    }

    void emptyBox()
    {
        TextCache cache;
        QImage img = blank();
        QCOMPARE(paint(cache, img, QRect(8, 4, 0, 20), kText, Qt::ElideRight), 0.0);
        QCOMPARE(cache.size(), 0);
        QVERIFY(ink(img).isNull());
    }
};

QTEST_MAIN(TextCacheTest)
#include "text_cache_test.moc"
