// TrionStyle (Việc nhỏ A): tab đang mở chữ --fw-m; viền focus của tab qua style (không còn khung focus của Fusion
// bên trong); chữ tắt '&' gạch chân khi đang giữ Alt (macOS: không bao giờ).
#include "app/theme/ThemeManager.h"

#include <QtTest>
#include <QtWidgets>

#include <memory>

using namespace Qt::StringLiterals;
using namespace trion;
using namespace trion::theme;

namespace {

// Thiết bị vẽ chỉ ghi font của các đoạn chữ: kiểm độ đậm không phụ thuộc font có trên máy.
class FontLog final : public QPaintDevice {
public:
    QList<QFont> fonts() const { return engine_.fonts; }
    QPaintEngine* paintEngine() const override { return &engine_; }

protected:
    int metric(PaintDeviceMetric m) const override
    {
        switch (m) {
        case PdmWidth:
        case PdmHeight:
        case PdmWidthMM:
        case PdmHeightMM:
            return 200;
        case PdmDepth:
            return 32;
        case PdmNumColors:
            return INT_MAX;
        case PdmDpiX:
        case PdmDpiY:
        case PdmPhysicalDpiX:
        case PdmPhysicalDpiY:
            return 96;
        case PdmDevicePixelRatio:
            return 1;
        default:
            return QPaintDevice::metric(m);
        }
    }

private:
    struct Engine final : QPaintEngine {
        QList<QFont> fonts;
        bool begin(QPaintDevice*) override { return true; }
        bool end() override { return true; }
        void updateState(const QPaintEngineState&) override {}
        void drawPixmap(const QRectF&, const QPixmap&, const QRectF&) override {}
        void drawPolygon(const QPointF*, int, PolygonDrawMode) override {}
        void drawPath(const QPainterPath&) override {}
        void drawTextItem(const QPointF&, const QTextItem& t) override { fonts << t.font(); }
        Type type() const override { return User; }
    };
    mutable Engine engine_;
};

class Counted final : public QPushButton {
public:
    using QPushButton::QPushButton;
    int paints = 0;

protected:
    void paintEvent(QPaintEvent* e) override
    {
        ++paints;
        QPushButton::paintEvent(e);
    }
};

QImage shot(QWidget* w, const QColor& bg)
{
    QImage img(w->size(), QImage::Format_ARGB32_Premultiplied);
    img.fill(bg);
    w->render(&img, {}, {}, QWidget::DrawWindowBackground | QWidget::DrawChildren);
    return img;
}

bool underline()
{
    return QApplication::style()->styleHint(QStyle::SH_UnderlineShortcut);
}

} // namespace

class StyleTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ThemeManager> tm_;
    std::unique_ptr<QWidget> win_;

private slots:
    void initTestCase()
    {
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        tm_ = std::make_unique<ThemeManager>(*store_);
        win_ = std::make_unique<QWidget>();
        win_->resize(400, 200);
        win_->show();
        QVERIFY(QTest::qWaitForWindowActive(win_.get()));
    }

    void cleanupTestCase()
    {
        win_.reset();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
    }

    // .tabs .tab[aria-selected], .rb-tab[aria-selected]: font-weight --fw-m; tab khác giữ độ đậm của widget.
    void selectedTabMedium()
    {
        QTabBar bar(win_.get());
        bar.addTab(u"Chèn"_s); // i18n-ok: dữ liệu thử
        QStyleOptionTab opt;
        opt.initFrom(&bar);
        opt.text = bar.tabText(0);
        opt.rect = QRect(0, 0, 80, 30);
        for (const bool selected : {true, false}) {
            opt.state.setFlag(QStyle::State_Selected, selected);
            FontLog log;
            {
                QPainter p(&log);
                p.setFont(bar.font());
                QApplication::style()->drawControl(QStyle::CE_TabBarTabLabel, &opt, &p, &bar);
            }
            QVERIFY(!log.fonts().isEmpty());
            for (const QFont& f : log.fonts())
                QCOMPARE(int(f.weight()), selected ? qRound(tok::num(tok::fw_m)) : int(bar.font().weight()));
        }
    }

    // Viền focus của tab do TrionStyle vẽ (2 px --c-focus quanh tab đang mở, chỉ khi focus từ bàn phím); bên trong viền
    // không còn khung focus của Fusion: ảnh có focus khớp ảnh không focus trừ dải viền.
    void tabFocusRing()
    {
        QTabBar bar(win_.get());
        bar.addTab(u"Chung"_s);    // i18n-ok: dữ liệu thử
        bar.addTab(u"Thống kê"_s); // i18n-ok: dữ liệu thử
        bar.setFocusPolicy(Qt::StrongFocus);
        bar.show();
        const QColor bg = tm_->color(tok::c_base);
        win_->setAttribute(Qt::WA_KeyboardFocusChange, false);
        const QImage plain = shot(&bar, bg);
        bar.setFocus();
        QTRY_VERIFY(bar.hasFocus());
        QCOMPARE(shot(&bar, bg), plain); // focus do chuột: không viền
        win_->setAttribute(Qt::WA_KeyboardFocusChange);
        const QImage focused = shot(&bar, bg);
        const QRect tab = bar.tabRect(bar.currentIndex());
        QCOMPARE(focused.pixelColor(tab.left(), tab.center().y()), tm_->color(tok::c_focus));
        QCOMPARE(focused.pixelColor(tab.right(), tab.center().y()), tm_->color(tok::c_focus));
        int inner = 0; // trong viền, trừ 4 điểm ảnh góc mà nét bo của viền chạm tới
        const QRect in = tab.adjusted(2, 2, -2, -2);
        for (int y = in.top(); y <= in.bottom(); ++y)
            for (int x = in.left(); x <= in.right(); ++x)
                inner += (x != in.left() && x != in.right()) || (y != in.top() && y != in.bottom())
                             ? focused.pixel(x, y) != plain.pixel(x, y)
                             : 0;
        QCOMPARE(inner, 0);
        win_->setAttribute(Qt::WA_KeyboardFocusChange, false);
    }

    // Chữ tắt '&': không gạch chân tới khi nhấn Alt (Windows, Linux), thả Alt hoặc rời ứng dụng thì tắt; nút có '&' vẽ
    // lại ngay. macOS: không bao giờ.
    void mnemonicsWhileAltHeld()
    {
        Counted save(u"&Lưu"_s, win_.get()); // i18n-ok: dữ liệu thử
        save.setGeometry(0, 0, 80, 24);
        save.show();
        const QColor bg = tm_->color(tok::c_base);
        QVERIFY(!underline());
        const QImage hidden = shot(&save, bg);
        QCoreApplication::processEvents();
        const int before = save.paints;
        QTest::keyPress(&save, Qt::Key_Alt);
#ifdef Q_OS_MACOS
        QVERIFY(!underline());
        QCOMPARE(shot(&save, bg), hidden);
#else
        QVERIFY(underline());
        QTRY_VERIFY(save.paints > before);
        QVERIFY(shot(&save, bg) != hidden);
#endif
        QTest::keyRelease(&save, Qt::Key_Alt);
        QVERIFY(!underline());
        QCOMPARE(shot(&save, bg), hidden);
        QTest::keyPress(&save, Qt::Key_Alt);
        QEvent away(QEvent::ApplicationDeactivate);
        QApplication::sendEvent(qApp, &away);
        QVERIFY(!underline());
        QTest::keyRelease(&save, Qt::Key_Alt);
    }
};

QTEST_MAIN(StyleTest)
#include "style_test.moc"
