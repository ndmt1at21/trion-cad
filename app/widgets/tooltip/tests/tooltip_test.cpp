// UI0-107 · RichTooltip, tiêu chí:
//   2  levels: mức 1 sau độ trễ mức 1, mức 2 hiện ≥ 1,5 s sau mức 1 khi rê liên tục (đo bằng sự kiện Show, timer
//      chính xác không đến sớm nên không phụ thuộc tải); rê trong phần tử không đặt lại timer
//      leaveHides: rời phần tử, bấm chuột, gõ phím thì ẩn ngay, không cần chờ vòng sự kiện
//      cursorNotCovered: không đè con trỏ (phần tử nhỏ, chiếm gần hết màn hình, sát mép dưới), nằm trong màn hình
//      keysByPlatform: phím theo nền tảng của Keymap, đổi ngay khi Keymap đổi lúc đang hiện
//   4  f1Help: F1 khi rê nút có lệnh phát helpRequested(lệnh tiếng Anh), F1 toàn cục không chạy; nút không có lệnh,
//      không rê thì F1 đi tiếp
//   5  idle: rảnh không timer, bộ lọc của ứng dụng gỡ khi rời; pseudoNoTruncation: vi-XL không cắt chữ
#include "app/widgets/tooltip/RichTooltip.h"

#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/keymap/Keymap.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/ThemeManager.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <functional>
#include <memory>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;
using namespace trion::theme;
using namespace trion::ui;
namespace i18n = trion::i18n;
namespace keymap = trion::keymap;
namespace sched = trion::sched;
namespace settings = trion::settings;

namespace {

bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 10s)
{
    if (cond())
        return true;
    QEventLoop loop;
    QTimer tick;
    QObject::connect(&tick, &QTimer::timeout, &loop, [&] {
        if (cond())
            loop.quit();
    });
    QTimer::singleShot(timeout, &loop, &QEventLoop::quit);
    tick.start(1);
    loop.exec();
    return cond();
}

void enter(QWidget* w)
{
    const QPointF local = w->rect().center();
    QEnterEvent e(local, local, w->mapToGlobal(local));
    QApplication::sendEvent(w, &e);
}

void leave(QWidget* w)
{
    QEvent e(QEvent::Leave);
    QApplication::sendEvent(w, &e);
}

// Thời điểm (ns) mức 1 và mức 2 hiện: sự kiện Show của cửa sổ tooltip (mức 1) và của dòng mô tả (mức 2), phát đồng
// bộ trong lúc RichTooltip hiện mức đó.
class ShowClock : public QObject {
public:
    explicit ShowClock(RichTooltip* tips) : tips_(tips)
    {
        clock_.start();
        tips->tipWindow()->installEventFilter(this);
        for (QLabel* l : tips->tipWindow()->findChildren<QLabel*>())
            l->installEventFilter(this);
    }
    qint64 at[3] = {-1, -1, -1};

protected:
    bool eventFilter(QObject*, QEvent* e) override
    {
        if (e->type() == QEvent::Show && at[tips_->level()] < 0)
            at[tips_->level()] = clock_.nsecsElapsed();
        return false;
    }

private:
    RichTooltip* tips_;
    QElapsedTimer clock_;
};

} // namespace

class TooltipTest : public QObject {
    Q_OBJECT

    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<UiExecutor> executor_;
    std::unique_ptr<ThemeManager> tm_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<keymap::Keymap> keymap_;
    std::unique_ptr<RichTooltip> tips_;

    QString tipText() const // các dòng đang hiện của tooltip
    {
        QStringList out;
        for (QLabel* l : tips_->tipWindow()->findChildren<QLabel*>())
            if (!l->isHidden())
                out << l->text();
        return out.join(u'\n');
    }

    QTimer* timer() const { return tips_->findChild<QTimer*>(); }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        QVERIFY(loadFonts());
        executor_ = std::make_unique<UiExecutor>();
        tm_ = std::make_unique<ThemeManager>(*store_);
        translator_ = std::make_unique<i18n::Translator>();
        keymap_ = std::make_unique<keymap::Keymap>(*store_, keymap::Platform::Windows);
        tips_ = std::make_unique<RichTooltip>();
        QCOMPARE(RichTooltip::instance(), tips_.get());
    }

    void init()
    {
        tips_->setKeymap(keymap_.get());
        tips_->setDelays(1ms, 1ms);
        tm_->setDensity(Density::Compact);
    }

    void cleanup() { tips_->leave(); }

    void cleanupTestCase()
    {
        tips_.reset();
        QCOMPARE(RichTooltip::instance(), nullptr);
        keymap_.reset();
        translator_.reset();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        sched::waitIdle();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    // Tiêu chí 2: giữ một lần chờ đúng 1,5 s.
    void levels()
    {
        QWidget host;
        auto* button = new QPushButton(&host);
        button->setGeometry(20, 20, 24, 24);
        RichTooltip::attach(button, {.name = u"Bắt điểm"_s,
                                     .command = u"OSNAP"_s,
                                     .keyId = u"osnap"_s,
                                     .description = u"Bắt điểm cuối, giữa, tâm khi vẽ."_s});
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        tips_->setDelays(20ms, 1500ms);
        ShowClock clock(tips_.get());

        enter(button);
        QCOMPARE(tips_->target(), button);
        QCOMPARE(tips_->level(), 0);
        QVERIFY(spinUntil([&] { return tips_->level() == 1; }));
        QCOMPARE(tipText(), u"Bắt điểm\nOSNAP · F3"_s);
        // Rê trong phần tử không đặt lại timer mức 2.
        const int before = timer()->remainingTime();
        QVERIFY(before > 0);
        const QPointF in = button->mapTo(&host, QPointF(5, 5));
        QMouseEvent move(QEvent::MouseMove, in, host.mapToGlobal(in), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(button->window()->windowHandle(), &move);
        QCOMPARE(tips_->target(), button);
        QVERIFY(timer()->remainingTime() <= before);

        QVERIFY(spinUntil([&] { return tips_->level() == 2; }, 5s));
        QVERIFY(clock.at[1] >= 0 && clock.at[2] >= 0);
        const double gap = double(clock.at[2] - clock.at[1]) / 1e6;
        qInfo("mức 1 → mức 2: %.1f ms (tiêu chí: 1500 ms)", gap);
        QVERIFY2(gap >= 1500, qPrintable(QString::number(gap)));
        QCOMPARE(tipText(), u"Bắt điểm\nOSNAP · F3\nBắt điểm cuối, giữa, tâm khi vẽ.\nNhấn F1 để xem trợ giúp"_s);
        QVERIFY(!timer()->isActive());
    }

    void leaveHides()
    {
        QWidget host;
        auto* edit = new QLineEdit(&host);
        auto* button = new QPushButton(&host);
        edit->setGeometry(100, 20, 80, 24);
        button->setGeometry(20, 20, 24, 24);
        RichTooltip::attach(button, {.name = u"Lưới"_s, .command = u"GRID"_s, .description = u"Lưới nền."_s});
        host.show();
        QVERIFY(QTest::qWaitForWindowActive(&host));
        edit->setFocus();

        const auto shown = [&](int level) {
            tips_->setDelays(1ms, level == 1 ? 1h : 1ms); // mức 1 đứng yên tới lúc kiểm (không đua với timer mức 2)
            enter(button);
            return spinUntil([&] { return tips_->level() == level; }) && tips_->tipWindow()->isVisible();
        };
        const auto hidden = [&] {
            return tips_->level() == 0 && !tips_->target() && !tips_->tipWindow()->isVisible() && !timer()->isActive();
        };
        // Rời phần tử: ngay trong sự kiện Leave, ở mức 1 và mức 2.
        for (int level : {1, 2}) {
            QVERIFY(shown(level));
            leave(button);
            QVERIFY(hidden());
        }
        // Chuột thật: vào nút, di sang ô nhập.
        tips_->setDelays(1ms, 1h);
        QTest::mouseMove(button);
        QCOMPARE(tips_->target(), button);
        QVERIFY(spinUntil([&] { return tips_->level() == 1; }));
        QTest::mouseMove(edit);
        QVERIFY(hidden());
        // Bấm, gõ phím, lăn chuột.
        QVERIFY(shown(2));
        QTest::mousePress(button, Qt::LeftButton);
        QVERIFY(hidden());
        QTest::mouseRelease(button, Qt::LeftButton);
        leave(button);
        QVERIFY(shown(1));
        QTest::keyClick(edit, Qt::Key_Shift); // phím bổ trợ: giữ tooltip
        QCOMPARE(tips_->level(), 1);
        QTest::keyClick(edit, Qt::Key_A);
        QVERIFY(hidden());
        leave(button);
        QVERIFY(shown(1));
        QWheelEvent wheel(QPointF(5, 5), button->mapToGlobal(QPointF(5, 5)), {}, QPoint(0, 120), Qt::NoButton,
                          Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(button, &wheel);
        QVERIFY(hidden());
        // Tooltip thường của Qt không hiện chồng.
        button->setToolTip(u"x"_s); // i18n-ok: dữ liệu thử
        QHelpEvent help(QEvent::ToolTip, QPoint(5, 5), button->mapToGlobal(QPoint(5, 5)));
        QVERIFY(QApplication::sendEvent(button, &help));
    }

    void cursorNotCovered()
    {
        QAction target;
        RichTooltip::attach(&target, {.name = u"Đường tròn"_s,
                                      .command = u"CIRCLE"_s,
                                      .description = u"Vẽ đường tròn theo tâm và bán kính, hoặc qua 2, 3 điểm."_s});
        const QRect screen = QGuiApplication::primaryScreen()->availableGeometry();
        const QSize arrow(qRound(tm_->px(tok::sp_6)), qRound(tm_->px(tok::sp_7))); // mũi tên từ điểm nóng
        struct Case {
            const char* name;
            QRect anchor;
            QPoint cursor;
        };
        const QPoint c = screen.center();
        const QList<Case> cases = {
            {"nút nhỏ", QRect(100, 100, 24, 24), QPoint(112, 112)},
            {"nút nhỏ, con trỏ ở đáy", QRect(100, 100, 24, 24), QPoint(102, 122)},
            {"phần tử gần hết màn hình", screen.adjusted(8, 8, -8, -8), c},
            {"sát mép dưới", QRect(c.x(), screen.bottom() - 30, 24, 24), QPoint(c.x() + 12, screen.bottom() - 18)},
            {"cột cao, con trỏ ở đáy", QRect(c.x(), screen.top(), 40, screen.height()),
             QPoint(c.x() + 5, screen.bottom() - 10)},
        };
        for (const Case& k : cases) {
            for (int level : {1, 2}) { // mức 1 giữ nguyên (timer mức 2 1 giờ), mức 2 là mức cuối
                tips_->setDelays(1ms, level == 1 ? 1h : 1ms);
                tips_->hover(&target, k.anchor, k.cursor);
                QVERIFY(spinUntil([&] { return tips_->level() == level; }));
                const QRect tip = tips_->tipWindow()->geometry();
                QVERIFY2(!tip.intersects(QRect(k.cursor, arrow)), k.name);
                QVERIFY2(screen.contains(tip), k.name);
                if (k.anchor.height() == 24 && k.cursor.y() < screen.bottom() - 100)
                    QVERIFY2(tip.top() > k.anchor.bottom(), k.name); // nút nhỏ: dưới nút như prototype
                tips_->leave();
            }
        }
    }

    void keysByPlatform()
    {
        QAction a;
        RichTooltip::attach(&a, {.name = u"Lưu"_s, .command = u"QSAVE"_s, .keyId = u"save"_s});
        tips_->setDelays(1ms, 1h); // ở mức 1
        const auto show = [&] {
            tips_->hover(&a, QRect(100, 100, 24, 24), QPoint(112, 112));
            return spinUntil([&] { return tips_->level() == 1; });
        };
        for (auto [p, want] : {std::pair{keymap::Platform::Windows, u"QSAVE · Ctrl+S"_s},
                               std::pair{keymap::Platform::Linux, u"QSAVE · Ctrl+S"_s},
                               std::pair{keymap::Platform::Mac, u"QSAVE · ⌘S"_s}}) {
            keymap::Keymap km(*store_, p);
            tips_->setKeymap(&km);
            QVERIFY(show());
            QCOMPARE(tipText(), u"Lưu\n"_s + want);
            QCOMPARE(tips_->keyText(u"isop"_s), p == keymap::Platform::Mac ? u"F5 · ⌃E"_s : u"F5 · Ctrl+E"_s);
            tips_->leave();
        }
        // Keymap đổi lúc đang hiện: nhãn đổi ngay, không cần rê lại.
        tips_->setKeymap(keymap_.get());
        QSignalSpy keys(tips_.get(), &RichTooltip::keysChanged);
        QVERIFY(show());
        QVERIFY(!keymap_->assign(u"save"_s, u"Ctrl+Alt+K"_s));
        QTRY_COMPARE(tipText(), u"Lưu\nQSAVE · Ctrl+S · Ctrl+Alt+K"_s);
        QVERIFY(keys.count() >= 1);
        QCOMPARE(tips_->level(), 1);
        keymap_->reset(u"save"_s);
        QTRY_COMPARE(tipText(), u"Lưu\nQSAVE · Ctrl+S"_s);
        QCOMPARE(tips_->keyText(u"không có"_s), QString());
        QCOMPARE(tips_->keyText({}), QString());
    }

    // Tiêu chí 4.
    void f1Help()
    {
        QWidget host;
        auto* col = new QVBoxLayout(&host);
        auto* edit = new QLineEdit;
        auto* circle = new QPushButton;
        auto* plain = new QPushButton;
        for (QWidget* w : std::initializer_list<QWidget*>{edit, circle, plain})
            col->addWidget(w);
        QAction globalHelp(&host); // phím F1 toàn cục (hành động HELP của Keymap)
        globalHelp.setShortcut(Qt::Key_F1);
        host.addAction(&globalHelp);
        QSignalSpy global(&globalHelp, &QAction::triggered);
        RichTooltip::attach(circle, {.name = u"Đường tròn"_s, .command = u"CIRCLE"_s});
        RichTooltip::attach(plain, {.name = u"Thu gọn"_s});
        host.show();
        QVERIFY(QTest::qWaitForWindowActive(&host));
        edit->setFocus();
        QSignalSpy help(tips_.get(), &RichTooltip::helpRequested);
        tips_->setDelays(1ms, 1500ms);

        // Vừa rê (tooltip chưa hiện) và đang hiện mức 1: F1 → yêu cầu trợ giúp CIRCLE, F1 toàn cục không chạy.
        for (bool wait : {false, true}) {
            enter(circle);
            if (wait)
                QVERIFY(spinUntil([&] { return tips_->level() == 1; }));
            QTest::keyClick(edit, Qt::Key_F1);
            QCOMPARE(help.count(), wait ? 2 : 1);
            QCOMPARE(help.last().at(0).toString(), u"CIRCLE"_s);
            QCOMPARE(global.count(), 0);
            QCOMPARE(tips_->level(), 0); // F1 đóng tooltip
            leave(circle);
        }
        QCOMPARE(edit->text(), QString());
        // Nút không có lệnh, hoặc không rê: F1 đi tiếp tới phím toàn cục.
        enter(plain);
        QTest::keyClick(edit, Qt::Key_F1);
        QCOMPARE(help.count(), 2);
        QCOMPARE(global.count(), 1);
        leave(plain);
        QTest::keyClick(edit, Qt::Key_F1);
        QCOMPARE(help.count(), 2);
        QCOMPARE(global.count(), 2);
        // Shift+F1 không phải yêu cầu trợ giúp theo lệnh.
        enter(circle);
        QTest::keyClick(edit, Qt::Key_F1, Qt::ShiftModifier);
        QCOMPARE(help.count(), 2);
    }

    // Tiêu chí 5: rảnh không timer; bộ lọc của ứng dụng chỉ cài khi đang rê.
    void idle()
    {
        QVERIFY(timer() && !timer()->isActive());
        QAction full, nameOnly;
        RichTooltip::attach(&full, {.name = u"Đo nhanh"_s, .command = u"MEASUREGEOM"_s});
        RichTooltip::attach(&nameOnly, {.name = u"Thu gọn"_s});
        tips_->hover(&full, QRect(100, 100, 24, 24), QPoint(112, 112));
        QVERIFY(timer()->isActive());
        QVERIFY(spinUntil([&] { return tips_->level() == 2; }));
        QVERIFY(!timer()->isActive()); // mức cuối đã hiện
        tips_->hover(&nameOnly, QRect(100, 100, 24, 24), QPoint(112, 112));
        QVERIFY(spinUntil([&] { return tips_->level() == 1; }));
        QVERIFY(!timer()->isActive()); // không có mức 2
        tips_->leave();
        QVERIFY(!timer()->isActive());

        // Sau khi rời: F1, bấm chuột không qua RichTooltip nữa.
        QWidget w;
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        QSignalSpy help(tips_.get(), &RichTooltip::helpRequested);
        tips_->hover(&full, QRect(100, 100, 24, 24), QPoint(112, 112));
        tips_->leave();
        QTest::keyClick(&w, Qt::Key_F1);
        QCOMPARE(help.count(), 0);
        QVERIFY(!RichTooltip::hasInfo(&w));
        QCOMPARE(RichTooltip::info(&w).command, QString());
    }

    // Nhánh biên của API công khai.
    void api()
    {
        QCOMPARE(tips_->keymap(), keymap_.get());
        QVERIFY(!RichTooltip::hasInfo(nullptr));
        QCOMPARE(RichTooltip::info(nullptr).name, QString());
        QCOMPARE(RichTooltip::name(nullptr), QString());
        QWidget w;
        w.setAccessibleName(u"Lưới"_s); // i18n-ok: dữ liệu thử
        RichTooltip::attach(&w, {.command = u"GRID"_s});
        QCOMPARE(RichTooltip::name(&w), u"Lưới"_s); // nút chỉ biểu tượng: tên trợ năng
        QAction a(u"&Lưu && đóng"_s);               // i18n-ok: dữ liệu thử
        RichTooltip::attach(&a, {});
        QVERIFY(RichTooltip::hasInfo(&a));
        QCOMPARE(RichTooltip::name(&a), u"Lưu & đóng"_s); // bỏ &, giữ && thành &
        RichTooltip::attach(&a, {.name = u"Lưu rồi đóng"_s});
        QCOMPARE(RichTooltip::name(&a), u"Lưu rồi đóng"_s);
        QCOMPARE(RichTooltip::editionName(Edition::None), QString());
        QCOMPARE(RichTooltip::editionName(Edition::Pro), u"Pro"_s);
        QCOMPARE(RichTooltip::editionName(Edition::ViewerPro), u"Viewer Pro"_s);
        QVERIFY(RichTooltip::badgeSize(Edition::None).isEmpty());
        QVERIFY(RichTooltip::badgeSize(Edition::ViewerPro).width() > RichTooltip::badgeSize(Edition::Pro).width());
        tips_->setKeymap(nullptr);
        QCOMPARE(tips_->keyText(u"save"_s), QString());
        // Không tên, không lệnh: không hiện khung rỗng.
        QAction empty;
        RichTooltip::attach(&empty, {.keyId = u"save"_s});
        tips_->hover(&empty, QRect(100, 100, 24, 24), QPoint(112, 112));
        QVERIFY(spinUntil([&] { return !timer()->isActive(); }));
        QCOMPARE(tips_->level(), 0);
        QVERIFY(!tips_->tipWindow()->isVisible());
        // Phần tử bị hủy khi đang rê: tooltip tự ẩn, gỡ bộ lọc.
        auto* gone = new QAction;
        RichTooltip::attach(gone, {.name = u"Tạm"_s, .command = u"TMP"_s});
        tips_->setDelays(20ms, 1ms);
        tips_->hover(gone, QRect(100, 100, 24, 24), QPoint(112, 112));
        delete gone;
        QVERIFY(spinUntil([&] { return !timer()->isActive(); }));
        QCOMPARE(tips_->level(), 0);
        QCOMPARE(tips_->target(), nullptr);
    }

    void pseudoNoTruncation()
    {
        QAction a;
        const auto retranslate = [&] {
            RichTooltip::attach(&a, {.name = tr("Máy tính nhanh"),
                                     .command = u"QUICKCALC"_s,
                                     .keyId = u"qc"_s,
                                     .description = tr("Tính biểu thức, lấy số đo từ bản vẽ rồi dán kết quả vào lệnh "
                                                       "đang chạy."),
                                     .need = Edition::Pro});
        };
        const auto report = [&] {
            QStringList out;
            for (const i18n::Truncation& t : i18n::findTruncations(tips_->tipWindow()))
                out << t.toString();
            return out.join(u"; "_s);
        };
        const QString vi = tr("Máy tính nhanh");
        for (const QString& lang : {i18n::kPseudoLanguage.toString(), u"vi"_s}) {
            QVERIFY(translator_->setLanguage(lang));
            retranslate();
            QCOMPARE(RichTooltip::name(&a).size() > vi.size(), lang != u"vi"_s);
            for (Density d : {Density::Compact, Density::Comfortable}) {
                tm_->setDensity(d);
                for (int level : {1, 2}) {
                    tips_->setDelays(1ms, level == 1 ? 1h : 1ms);
                    tips_->hover(&a, QRect(100, 100, 24, 24), QPoint(112, 112));
                    QVERIFY(spinUntil([&] { return tips_->level() == level; }));
                    QTRY_COMPARE(report(), QString());
                    QVERIFY(tips_->tipWindow()->width() <= 300);
                    tips_->leave();
                }
            }
        }
    }
};

QTEST_MAIN(TooltipTest)
#include "tooltip_test.moc"
