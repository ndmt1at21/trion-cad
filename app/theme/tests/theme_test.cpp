// UI0-002 tiêu chí 1, 5 và Theo hệ thống: đổi chủ đề, mật độ, Giao diện nhẹ khi đang chạy với 2.000 widget, đo đến
// khung đầu đã đổi, quét cây widget không còn màu/kích thước cũ; nhẹ: bóng thành viền 1 px, chuyển động 0 ms.
#include "app/theme/ThemeManager.h"

#include <QtTest>
#include <QtWidgets>

#include <algorithm>
#include <functional>
#include <memory>

using namespace Qt::StringLiterals;
using namespace trion;
using namespace trion::theme;

namespace {

constexpr double kBudgetMs = 100; // DoD-L: đổi trạng thái giao diện ≤ 100 ms đến khung đầu
// Sanitizer thay malloc của cả tiến trình (kể cả Qt): thời gian đo được chỉ để báo, không kiểm.
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
constexpr bool kSanitized = true;
#elif defined(__has_feature)
constexpr bool kSanitized = __has_feature(address_sanitizer) || __has_feature(thread_sanitizer);
#else
constexpr bool kSanitized = false;
#endif

QColor tokColor(tok::ColorId id, tok::Mode m) { return toQColor(tok::color(id, m)); }

// Cửa sổ thử: ghi lúc vẽ xong khung đầu sau một thay đổi. UpdateRequest của cửa sổ = vẽ vùng bẩn + flush backing
// store (khung hình); việc có ưu tiên cao hơn (LayoutRequest khi đổi mật độ) đã xử lý trước nó.
class Window : public QWidget {
public:
    QElapsedTimer clock;
    qint64 framedNs = -1;
    bool armed = false;

protected:
    bool event(QEvent* e) override
    {
        const bool r = QWidget::event(e);
        if (armed && e->type() == QEvent::UpdateRequest) {
            framedNs = clock.nsecsElapsed();
            armed = false;
        }
        return r;
    }
};

// Widget tự vẽ kiểu popup (mẫu cho UI0-003…): nền --c-raised, bóng --sh-pop; Giao diện nhẹ: viền 1 px.
class Card : public QWidget {
public:
    explicit Card(QWidget* parent) : QWidget(parent)
    {
        connect(&ThemeManager::instance(), &ThemeManager::changed, this, qOverload<>(&QWidget::update));
    }
    QRect panel() const { return rect().adjusted(4, 4, -4, -4); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        const ThemeManager& t = ThemeManager::instance();
        const tok::Shadow s = t.shadow(tok::sh_pop);
        QPainter p(this);
        p.fillRect(panel().translated(qRound(s.x), qRound(s.y)) + QMargins(1, 1, 1, 1) * qRound(s.spread),
                   toQColor(s.color));
        p.fillRect(panel(), t.color(tok::c_raised));
    }
};

QWidget* makeControl(int i, QWidget* parent)
{
    const QString n = QString::number(i);
    switch (i % 10) {
    case 0:
    case 8: {
        auto* b = new QToolButton(parent);
        b->setText(n);
        b->setAutoRaise(true);
        return b;
    }
    case 1: {
        auto* b = new QToolButton(parent);
        b->setText(n);
        b->setCheckable(true);
        b->setChecked(i % 20 == 1);
        return b;
    }
    case 2:
        return new QPushButton(n, parent);
    case 3:
        return new QLabel(n, parent);
    case 4:
        return new QCheckBox(n, parent);
    case 5: {
        auto* c = new QComboBox(parent);
        c->addItems({n, n + n});
        return c;
    }
    case 6:
        return new QLineEdit(n, parent);
    case 7:
        return new QRadioButton(n, parent);
    default:
        return new QSpinBox(parent);
    }
}

double median(QList<double> v)
{
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

QString stats(QList<double> v)
{
    std::sort(v.begin(), v.end());
    return u"n=%1 min %2 · trung vị %3 · max %4 ms"_s.arg(v.size())
        .arg(v.first(), 0, 'f', 1)
        .arg(v[v.size() / 2], 0, 'f', 1)
        .arg(v.last(), 0, 'f', 1);
}

} // namespace

class ThemeTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ThemeManager> tm_;
    std::unique_ptr<Window> win_;
    std::unique_ptr<QDialog> dialog_; // tạo sẵn, chưa mở
    QList<QMenu*> menus_;
    QStandardItemModel model_;
    QTabWidget* ribbon_ = nullptr;
    QTreeView* tree_ = nullptr;
    QPushButton* button_ = nullptr;
    Card* card_ = nullptr;

    // ms từ lúc đổi đến khi cửa sổ vẽ xong khung đầu; -1 nếu quá 5 s.
    double frame(const std::function<void()>& change)
    {
        for (int i = 0; i < 5; ++i)
            QCoreApplication::processEvents(); // xả việc tồn đọng
        win_->framedNs = -1;
        win_->armed = true;
        win_->clock.start();
        change();
        const QDeadlineTimer deadline(5000);
        while (win_->framedNs < 0 && !deadline.hasExpired())
            QCoreApplication::processEvents();
        return win_->framedNs < 0 ? -1 : win_->framedNs / 1e6;
    }

    // Khung vừa flush (backing store của nền tảng offscreen), không vẽ lại.
    QImage shownFrame() const { return win_->screen()->grabWindow(win_->winId()).toImage(); }
    QColor pixel(const QImage& img, const QWidget* w, QPoint p) const
    {
        return img.pixelColor(w->mapTo(win_.get(), p));
    }

    // Không widget nào còn giữ màu cũ: mọi vai trò QPalette (QSS cũng ghi vào palette) không trùng màu token chỉ có
    // ở chế độ cũ, và QSS gắn trên widget là của chế độ hiện tại.
    void verifyNoStaleColors(tok::Mode old)
    {
        QSet<QRgb> stale, now;
        for (const tok::Token& t : tok::tokens) {
            if (t.kind == tok::Kind::Color) {
                now.insert(tokColor(tok::ColorId{t.i}, tm_->mode()).rgba());
                stale.insert(tokColor(tok::ColorId{t.i}, old).rgba());
            }
        }
        stale.subtract(now);
        QVERIFY(old.light == tm_->mode().light || stale.size() > 30);
        for (const QWidget* w : QApplication::allWidgets()) {
            for (const auto g : {QPalette::Active, QPalette::Inactive, QPalette::Disabled})
                for (int r = 0; r < QPalette::NColorRoles; ++r)
                    if (r != QPalette::NoRole && stale.contains(w->palette().color(g, QPalette::ColorRole(r)).rgba()))
                        QFAIL(qPrintable(u"%1 (con của %2) giữ màu cũ ở vai trò %3"_s.arg(w->metaObject()->className())
                                             .arg(w->parent() ? w->parent()->metaObject()->className() : "")
                                             .arg(r)));
            if (!w->styleSheet().isEmpty())
                QCOMPARE(w->styleSheet(), tm_->styleSheet());
        }
    }

    // Không widget nào giữ kích thước của mật độ cũ (kể cả widget ẩn, chưa mở).
    void verifyNoStaleSizes()
    {
        const int ctl = qRound(tm_->px(tok::h_ctl));
        int checked = 0;
        for (const QWidget* w : QApplication::allWidgets()) {
            const bool control = qobject_cast<const QPushButton*>(w) || qobject_cast<const QComboBox*>(w) ||
                                 qobject_cast<const QAbstractSpinBox*>(w) || qobject_cast<const QToolButton*>(w) ||
                                 (qobject_cast<const QLineEdit*>(w) && !qobject_cast<QAbstractSpinBox*>(w->parent()));
            if (!control || w->parentWidget() == ribbon_->tabBar()) // nút cuộn của dải tab: ngoài phạm vi
                continue;
            const char* what = w->metaObject()->className();
            QVERIFY2(w->sizeHint().height() == ctl, what);
            QVERIFY2(!w->isVisible() || w->height() == ctl, what);
            ++checked;
        }
        QVERIFY(checked > 900);
        QCOMPARE(tree_->visualRect(model_.index(0, 0)).height(), qRound(tm_->px(tok::h_row)));
        QCOMPARE(tree_->header()->height(), ctl + 1);                                    // + đường chia 1 px
        QCOMPARE(ribbon_->tabBar()->tabRect(0).height(), qRound(tm_->px(tok::h_ftab)) + 2); // + gạch chân 2 px
        for (QMenu* m : std::as_const(menus_))
            QCOMPARE(m->actionGeometry(m->actions().constFirst()).height(), ctl);
    }

private slots:
    void init()
    {
        QTest::failOnWarning(QRegularExpression(u"^theme:|parse|stylesheet"_s, QRegularExpression::CaseInsensitiveOption));
    }

    void initTestCase()
    {
        init();
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        tm_ = std::make_unique<ThemeManager>(*store_);
        QVERIFY(!tm_->mode().light && !tm_->mode().comfortable && !tm_->mode().lite); // mặc định: Tối, gọn

        // Cửa sổ kiểu khung CAD: ribbon 10 tab (một tab hiện), palette (cây 300 dòng, thuộc tính), thanh trạng
        // thái, widget tự vẽ; cộng menu đã mở một lần và hộp thoại tạo sẵn chưa mở.
        win_ = std::make_unique<Window>();
        auto* v = new QVBoxLayout(win_.get());
        ribbon_ = new QTabWidget(win_.get());
        for (int t = 0; t < 10; ++t) {
            auto* page = new QWidget;
            auto* g = new QGridLayout(page);
            for (int i = 0; i < 150; ++i)
                g->addWidget(makeControl(i, page), i / 30, i % 30);
            ribbon_->addTab(page, QString::number(t));
        }
        v->addWidget(ribbon_);
        auto* mid = new QHBoxLayout;
        v->addLayout(mid, 1);
        card_ = new Card(win_.get());
        card_->setFixedSize(160, 120);
        mid->addWidget(card_, 0, Qt::AlignTop);
        model_.setColumnCount(3);
        for (int r = 0; r < 300; ++r)
            model_.appendRow({new QStandardItem(QString::number(r)), new QStandardItem(QString::number(r * 2)),
                              new QStandardItem(QString::number(r * 3))});
        tree_ = new QTreeView(win_.get());
        tree_->setModel(&model_);
        mid->addWidget(tree_, 1);
        auto* props = new QWidget(win_.get());
        auto* form = new QFormLayout(props);
        for (int r = 0; r < 24; ++r)
            form->addRow(new QLabel(QString::number(r), props), makeControl(r * 7 % 10 == 3 ? 2 : r * 7, props));
        mid->addWidget(props);
        auto* status = new QHBoxLayout;
        v->addLayout(status);
        for (int i = 0; i < 25; ++i)
            status->addWidget(makeControl(1, win_.get()));
        auto* progress = new QProgressBar(win_.get());
        progress->setValue(40);
        status->addWidget(progress);
        button_ = new QPushButton(QString::number(0), win_.get());
        status->addWidget(button_);
        for (int m = 0; m < 20; ++m) {
            auto* menu = new QMenu(win_.get());
            for (int a = 0; a < 10; ++a)
                menu->addAction(QString::number(a))->setEnabled(a != 3);
            menu->addSeparator();
            menus_ << menu;
        }
        dialog_ = std::make_unique<QDialog>();
        auto* dg = new QGridLayout(dialog_.get());
        for (int i = 0; i < 200; ++i)
            dg->addWidget(makeControl(i, dialog_.get()), i / 10, i % 10);

        win_->resize(1920, 1080);
        win_->show();
        QVERIFY(QTest::qWaitForWindowExposed(win_.get()));
        for (int i = ribbon_->count() - 1; i >= 0; --i) { // người dùng đã mở mọi tab ribbon
            ribbon_->setCurrentIndex(i);
            QCoreApplication::processEvents();
        }
        for (QMenu* m : std::as_const(menus_)) { // và mọi menu (QSS gắn lúc polish)
            m->popup(QPoint(10, 10));
            QCoreApplication::processEvents();
            m->hide();
        }
        QCoreApplication::processEvents();
    }

    void cleanupTestCase()
    {
        win_.reset();
        dialog_.reset();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        // Dọn trong cleanupTestCase, không để hàm hủy của lớp test: sau qExec, Qt Test đã join luồng watchdog của nó
        // mà TSan không thấy (Qt không có TSan), stack của luồng chính dùng lại thì bị báo race giả.
        store_.reset();
    }

    void switchAtRuntimeWith2000Widgets()
    {
        const QList<QWidget*> all = QApplication::allWidgets();
        const auto visible = std::count_if(all.begin(), all.end(), [](QWidget* w) { return w->isVisible(); });
        qInfo("%lld widget, %lld đang hiện, cửa sổ %dx%d", qint64(all.size()), qint64(visible), win_->width(),
              win_->height());
        QVERIFY(all.size() >= 2000);
        QCOMPARE(win_->size(), QSize(1920, 1080));

        QList<double> theme, density, lite, repaint;
        for (int round = 0; round < 3; ++round) {
            for (int i = 0; i < 2; ++i) // sàn: vẽ lại cả cửa sổ, không đổi gì (đổi chủ đề thì mọi điểm ảnh đổi)
                repaint << frame([&] { win_->update(); });
            for (const bool light : {true, false}) {
                const tok::Mode old = tm_->mode();
                theme << frame([&] { tm_->setTheme(light ? Theme::Light : Theme::Dark); });
                QVERIFY(theme.last() >= 0);
                QCOMPARE(tm_->mode().light, light);
                const QImage f = shownFrame(); // khung đầu đã đổi
                QCOMPARE(pixel(f, win_.get(), {2, 2}), tokColor(tok::c_base, tm_->mode()));
                QCOMPARE(pixel(f, button_, {3, button_->height() / 2}), tokColor(tok::c_raised, tm_->mode()));
                QCOMPARE(pixel(f, card_, card_->panel().center()), tokColor(tok::c_raised, tm_->mode()));
                verifyNoStaleColors(old);
                if (QTest::currentTestFailed())
                    return;
            }
            for (const bool comfy : {true, false}) {
                density << frame([&] { tm_->setDensity(comfy ? Density::Comfortable : Density::Compact); });
                QVERIFY(density.last() >= 0);
                QCOMPARE(button_->height(), comfy ? 26 : 24); // đã bố trí lại trước khung đầu
                verifyNoStaleSizes();
                if (QTest::currentTestFailed())
                    return;
            }
            for (const bool on : {true, false}) {
                lite << frame([&] { tm_->setLite(on); });
                QVERIFY(lite.last() >= 0);
                const QPoint edge(card_->panel().left() - 1, card_->panel().center().y());
                QCOMPARE(pixel(shownFrame(), card_, edge),
                         on ? tm_->color(tok::c_border_ctl) : tm_->color(tok::c_base));
            }
        }
        // Ngân sách 100 ms (DoD-L), nới theo máy khi chính việc vẽ lại cả cửa sổ đã chậm. Máy CI dùng chung (nhiều build
        // song song) làm số đo chậm 2–4 lần thất thường, nhiễu chỉ cộng thêm: lần nhanh nhất phản ánh chi phí thật
        // nên phải trong ngân sách, trung vị trong hai lần ngân sách. Số p99 trên máy tham chiếu do QA-011 thu.
        const double budget = std::max(kBudgetMs, 1.5 * median(repaint));
        qInfo("vẽ lại cả cửa sổ: %s → ngân sách %.0f ms", qPrintable(stats(repaint)), budget);
        qInfo("đổi chủ đề:  %s", qPrintable(stats(theme)));
        qInfo("đổi mật độ:  %s", qPrintable(stats(density)));
        qInfo("đổi nhẹ:     %s", qPrintable(stats(lite)));
        for (const QList<double>* v : {&theme, &density, &lite})
            QVERIFY2(kSanitized || (*std::min_element(v->begin(), v->end()) <= budget && median(*v) <= 2 * budget),
                     qPrintable(stats(*v)));
    }

    void styleSheetFromTemplate()
    {
        QFile f(u":/trion/theme/app.qss.in"_s);
        QVERIFY(f.open(QIODevice::ReadOnly));
        static const QRegularExpression comment(uR"(/\*.*?\*/)"_s, QRegularExpression::DotMatchesEverythingOption);
        const QString tpl = QString::fromUtf8(f.readAll()).remove(comment);
        static const QRegularExpression var(uR"(var\((--[a-z0-9-]+)\))"_s);
        int used = 0;
        for (auto it = var.globalMatch(tpl); it.hasNext(); ++used) {
            const QByteArray name = it.next().captured(1).toLatin1();
            QVERIFY2(tok::find({name.constData(), std::size_t(name.size())}), name.constData());
        }
        QVERIFY(used >= 10);
        for (const tok::Mode m : {tok::Mode{}, tok::Mode{.light = true}, tok::Mode{.comfortable = true}}) {
            const QString qss = ThemeManager::makeStyleSheet(m);
            QVERIFY(!qss.contains(u"var("_s));
            QVERIFY(qss.contains(u"QHeaderView { background: %1;"_s.arg(m.light ? u"#eaedf0"_s : u"#15171a"_s)));
            QVERIFY(qss.contains(m.comfortable ? u"min-height: 26px"_s : u"min-height: 24px"_s));
        }
        QVERIFY(!tpl.contains(u"*/"_s) && !tpl.contains(u"/*"_s)); // chú thích bỏ hết, không dính vào luật
        // Cỡ chữ lẻ giữ đúng qua pt (QFont::setPixelSize, QSS "px" chỉ nhận số nguyên): --fs-s 11,5 px = 8,625 pt.
        QCOMPARE(tm_->font(tok::fs_s).pointSizeF(), 8.625);
        QCOMPARE(QApplication::font().pointSizeF(), 9.0); // --fs 12 px

        // QSS gắn vào đầu cột và đổi theo chủ đề; menu đang mở, tooltip do TrionStyle vẽ theo chủ đề.
        QMenu* menu = menus_.constFirst();
        menu->popup(QPoint(10, 10));
        for (const Theme t : {Theme::Light, Theme::Dark}) {
            tm_->setTheme(t);
            QToolTip::showText(win_->mapToGlobal(QPoint(40, 40)), QString::number(42), win_.get());
            QCoreApplication::processEvents();
            QCOMPARE(tree_->header()->styleSheet(), tm_->styleSheet());
            QCOMPARE(tree_->header()->grab().toImage().pixelColor(2, 2), tm_->color(tok::c_sunken));
            const QImage m = menu->grab().toImage(); // .menu: nền --c-raised, viền --c-border-ctl
            QCOMPARE(m.pixelColor(0, m.height() / 2), tm_->color(tok::c_border_ctl));
            QCOMPARE(m.pixelColor(2, 2), tm_->color(tok::c_raised));
            const auto tops = QApplication::topLevelWidgets();
            const auto tip = std::find_if(tops.begin(), tops.end(), [](QWidget* w) { return w->inherits("QTipLabel"); });
            QVERIFY(tip != tops.end()); // .tip: nền --c-dyn-bg, viền --c-dyn-b, chữ --fs-s
            const QImage img = (*tip)->grab().toImage();
            QCOMPARE(img.pixelColor(img.width() / 2, 0), tm_->color(tok::c_dyn_b));
            QCOMPARE(img.pixelColor(2, img.height() / 2), tm_->color(tok::c_dyn_bg));
            QCOMPARE((*tip)->palette().color((*tip)->foregroundRole()), tm_->color(tok::c_dyn_t));
            QCOMPARE((*tip)->font().pointSizeF(), tm_->font(tok::fs_s).pointSizeF());
            QToolTip::hideText();
        }
        menu->hide();
    }

    void liteBordersInsteadOfShadowsNoMotion()
    {
        for (const Theme t : {Theme::Dark, Theme::Light}) {
            tm_->setTheme(t);
            tm_->setLite(true);
            for (const tok::ShadowId id : {tok::sh_pop, tok::sh_dlg}) {
                const tok::Shadow s = tm_->shadow(id);
                QCOMPARE(QList<double>({s.x, s.y, s.blur, s.spread}), QList<double>({0, 0, 0, 1}));
                QCOMPARE(toQColor(s.color), tm_->color(tok::c_border_ctl));
            }
            QCOMPARE(tm_->ms(tok::t), 0);
            QCOMPARE(tm_->ms(tok::t_fast), 0);
            QCOMPARE(QApplication::style()->styleHint(QStyle::SH_Widget_Animation_Duration), 0);
            QVERIFY(!QApplication::isEffectEnabled(Qt::UI_AnimateMenu));
            QVERIFY(!QApplication::isEffectEnabled(Qt::UI_FadeTooltip));
            for (QMenu* m : std::as_const(menus_)) // popup không bóng hệ thống
                QVERIFY(m->windowFlags().testFlag(Qt::NoDropShadowWindowHint));
            tm_->setLite(false);
            QCOMPARE(tm_->shadow(tok::sh_pop).blur, 14.0);
            QCOMPARE(tm_->ms(tok::t), 120);
            QCOMPARE(QApplication::style()->styleHint(QStyle::SH_Widget_Animation_Duration), 120);
            QVERIFY(QApplication::isEffectEnabled(Qt::UI_General));
            for (QMenu* m : std::as_const(menus_))
                QVERIFY(!m->windowFlags().testFlag(Qt::NoDropShadowWindowHint));
        }
        tm_->setTheme(Theme::Dark);
        // Giảm chuyển động (UI0-013 nối hệ điều hành): chỉ tắt chuyển động, bóng giữ nguyên.
        tm_->setReducedMotion(true);
        QCOMPARE(tm_->ms(tok::t), 0);
        QCOMPARE(QApplication::style()->styleHint(QStyle::SH_Widget_Animation_Duration), 0);
        QCOMPARE(tm_->shadow(tok::sh_pop).blur, 14.0);
        tm_->setReducedMotion(false);
        QCOMPARE(tm_->ms(tok::t_fast), 80);
    }

    void noModeUsesBlur()
    {
        // "Không làm mờ" (hướng A): không hiệu ứng blur, không nền trong suốt kiểu kính ở mọi tổ hợp chế độ.
        for (int bits = 0; bits < 16; ++bits) {
            tm_->setTheme(bits & 1 ? Theme::Light : Theme::Dark);
            tm_->setDensity(bits & 2 ? Density::Comfortable : Density::Compact);
            tm_->setLite(bits & 4);
            tm_->setReducedMotion(bits & 8);
            QVERIFY(!tm_->styleSheet().contains(u"blur"_s));
            for (const QWidget* w : QApplication::allWidgets())
                QVERIFY(!qobject_cast<QGraphicsBlurEffect*>(w->graphicsEffect()) &&
                        !w->testAttribute(Qt::WA_TranslucentBackground));
        }
        tm_->setTheme(Theme::Dark);
        tm_->setDensity(Density::Compact);
        tm_->setLite(false);
        tm_->setReducedMotion(false);
    }

    void followsSystemColorScheme()
    {
        // Nền tảng offscreen không đổi được colorScheme thật: phát tín hiệu như khi hệ điều hành đổi.
        QStyleHints* hints = QGuiApplication::styleHints();
        QSignalSpy spy(tm_.get(), &ThemeManager::changed);
        tm_->setTheme(Theme::System);
        emit hints->colorSchemeChanged(Qt::ColorScheme::Light);
        QVERIFY(tm_->mode().light);
        QCOMPARE(QApplication::palette().color(QPalette::Window), tokColor(tok::c_base, {.light = true}));
        emit hints->colorSchemeChanged(Qt::ColorScheme::Dark);
        QVERIFY(!tm_->mode().light);
        QCOMPARE(QApplication::palette().color(QPalette::Window), tokColor(tok::c_base, {}));
        QCOMPARE(spy.count(), 2);
        tm_->setTheme(Theme::Light); // chọn cố định thì không theo hệ thống
        emit hints->colorSchemeChanged(Qt::ColorScheme::Dark);
        QVERIFY(tm_->mode().light);
        tm_->setTheme(Theme::Dark);
    }
};

QTEST_MAIN(ThemeTest)
#include "theme_test.moc"
