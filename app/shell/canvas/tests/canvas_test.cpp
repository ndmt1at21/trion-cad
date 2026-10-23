// UIA-109 · Vùng vẽ (CanvasWidget, CanvasOverlay). Nhóm theo tiêu chí nghiệm thu (_handoff/ui-impl/w8-uia109.md):
//   1  inputToPhoton: di chuột (qua QWindow như chuột thật) đến hết khung có chữ thập mới: UiProbe::untilFrame
//      (viewport_frame) báo p50/p99 thời gian thật; CPU luồng UI mỗi mẫu ≤ 25 ms (kiểm ở release); ảnh khung thật
//      (grabWindow) có chữ thập ở chỗ mới, chỗ cũ đã sạch. 144 Hz, dấu thời gian present: Hoãn (cần RND-001).
//   2  hoverPartialRepaint: 1.000 lần rê với hit test giả O(1): mỗi lần chỉ vẽ lại vùng bẩn (không cả widget), tô sáng
//      mới có ở lần vẽ kế tiếp, vùng vẽ lại phủ đối tượng. Hit test 1 triệu entity ≤ 1 ms: Hoãn (cần INT-009, RND-013).
//      crossingPreview: khung Crossing đang kéo, truy vấn giả trả 10.000 id: CPU mỗi khung ≤ 8 ms (release).
//   3  idle: 5 s rảnh, chuột trong vùng vẽ: 0 lần vẽ, 0 QTimerEvent trong cả tiến trình.
//   4  windowCrossingGray: Window viền liền, Crossing viền đứt ở ảnh xám, cả khi đặt màu B = màu A. keys10k: 10.000
//      phím (qua ShortcutOverride và KeyPress như phím thật, gửi liền: nhanh hơn 30 phím/s) xen 204 chuỗi commit của bộ
//      gõ khi focus ở vùng vẽ: đích nhận đủ, đúng thứ tự.
//   Còn lại: cursorStates (6 trạng thái, con trỏ hệ thống, huy hiệu, rời vùng vẽ), sizes (PICKBOX, CURSORSIZE, APERTURE
//   đếm điểm ảnh), clickAndBox (nhấp, PICKDRAG 0/1/2, khung hai cú nhấp, Esc, chuột phải), lasso (nhấn giữ, Space),
//   polygon (F/WP/CP của lệnh), commandInputSink (ô dòng lệnh thật: chữ, bộ gõ, bỏ qua bộ gõ, ô soạn chữ), shortcuts
//   (phím của ActionRegistry không bị nuốt), hoverTip; DoD-U: a11y (tên, vai trò Canvas, checkNames, checkFocus).
//   Ảnh kiểm là điểm ảnh của hình không chữ: một bộ cho mọi nền tảng, không có ảnh baseline.
#include "app/shell/canvas/CanvasWidget.h"

#include "app/core/a11y/AccessibilityAudit.h"
#include "app/core/actions/ActionRegistry.h"
#include "app/core/i18n/Translator.h"
#include "app/core/keymap/Keymap.h"
#include "app/core/perf/UiProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/cmdline/CommandInput.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/message/Messages.h"
#include "core/base/sched/Scheduler.h"
#include "tests/timing.h"

#include <QAccessible>
#include <QApplication>
#include <QBoxLayout>
#include <QInputMethodEvent>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QSignalSpy>
#include <QStyle>
#include <QStyleHints>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using namespace trion;
using namespace trion::shell;
using theme::Sem;
using theme::ThemeManager;
namespace probe = trion::ui::probe;

namespace {

bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 10s)
{
    if (cond())
        return true;
    QEventLoop loop;
    QTimer tick, limit;
    QObject::connect(&tick, &QTimer::timeout, &loop, [&] {
        if (cond())
            loop.quit();
    });
    limit.setSingleShot(true);
    QObject::connect(&limit, &QTimer::timeout, &loop, &QEventLoop::quit);
    tick.start(1);
    limit.start(timeout);
    loop.exec();
    return cond();
}

void spinFor(std::chrono::milliseconds d)
{
    QEventLoop loop;
    QTimer::singleShot(d, &loop, &QEventLoop::quit);
    loop.exec();
}

double ms(std::chrono::nanoseconds d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

bool cpuSupported()
{
    return test::threadCpu().count() >= 0;
}

// Ngưỡng CPU chỉ kiểm ở bản tối ưu (kTimed) và khi đo được CPU luồng.
bool withinCpu(double v, double budget)
{
    return !test::kTimed || !cpuSupported() || v <= budget;
}

double percentile(std::vector<double> v, double p)
{
    if (v.empty())
        return qQNaN();
    std::sort(v.begin(), v.end());
    return v[std::min(v.size() - 1, size_t(p * double(v.size() - 1) + 0.5))];
}

QJsonObject probeOp(const char* name)
{
    return probe::toJson()["ops"].toObject()[QLatin1StringView(name)].toObject();
}

// Đích phím giả: ghi phím và chuỗi commit của bộ gõ theo thứ tự nhận.
class Recorder final : public QWidget {
public:
    using QWidget::QWidget;
    QStringList log;
    bool imOn = true;

protected:
    void keyPressEvent(QKeyEvent* e) override { log << u"k%1:%2"_s.arg(e->key()).arg(e->text()); }
    void inputMethodEvent(QInputMethodEvent* e) override { log << u"i:"_s + e->commitString(); }
    QVariant inputMethodQuery(Qt::InputMethodQuery q) const override
    {
        return q == Qt::ImEnabled ? QVariant(imOn) : QWidget::inputMethodQuery(q);
    }
};

// Lần vẽ của vùng vẽ: số lần, vùng của lần cuối, có lần nào vẽ cả widget.
struct Paints : QObject {
    QWidget* w = nullptr;
    int count = 0;
    int full = 0;
    QRegion last;
    double maxShare = 0; // phần diện tích lớn nhất của một lần vẽ so với cả widget
    bool eventFilter(QObject* o, QEvent* e) override
    {
        if (o == w && e->type() == QEvent::Paint) {
            ++count;
            last = static_cast<QPaintEvent*>(e)->region();
            if ((QRegion(w->rect()) - last).isEmpty())
                ++full;
            qint64 area = 0;
            for (const QRect& r : last)
                area += qint64(r.width()) * r.height();
            maxShare = std::max(maxShare, double(area) / (double(w->width()) * w->height()));
        }
        return false;
    }
};

// QTimerEvent tới mọi đối tượng trong tiến trình, trừ timer đo của test (như tests/integration/it_vo_test.cpp).
class TimerLog final : public QObject {
public:
    explicit TimerLog(const QObject* ignore) : ignore_(ignore) {}
    QStringList seen;

protected:
    bool eventFilter(QObject* watched, QEvent* e) override
    {
        if (e->type() == QEvent::Timer && watched != ignore_)
            seen << QString::fromLatin1(watched->metaObject()->className());
        return false;
    }

private:
    const QObject* ignore_;
};

int count(const QImage& img, QRgb c, const QRect& in = {})
{
    const QRect r = in.isNull() ? img.rect() : in & img.rect();
    int n = 0;
    for (int y = r.top(); y <= r.bottom(); ++y)
        for (int x = r.left(); x <= r.right(); ++x)
            n += img.pixel(x, y) == c;
    return n;
}

// Điểm ảnh gần màu c (khử răng cưa): lệch mỗi kênh ≤ tol.
int near(const QImage& img, const QColor& c, const QRect& in, int tol = 48)
{
    const QRect r = in & img.rect();
    int n = 0;
    for (int y = r.top(); y <= r.bottom(); ++y)
        for (int x = r.left(); x <= r.right(); ++x) {
            const QRgb p = img.pixel(x, y);
            n += std::abs(qRed(p) - c.red()) <= tol && std::abs(qGreen(p) - c.green()) <= tol &&
                 std::abs(qBlue(p) - c.blue()) <= tol;
        }
    return n;
}

// Lưới hit test giả O(1): ô 20 px, ô chẵn (hàng + cột) có một đối tượng, id = hàng × 1000 + cột + 1.
constexpr int kCell = 20;
std::optional<quint64> gridHit(QPointF p, double)
{
    const int col = int(p.x()) / kCell, row = int(p.y()) / kCell;
    if ((row + col) % 2)
        return std::nullopt;
    return quint64(row) * 1000 + quint64(col) + 1;
}
QRectF gridBounds(quint64 id)
{
    const auto row = int((id - 1) / 1000), col = int((id - 1) % 1000);
    return {double(col * kCell), double(row * kCell), double(kCell), double(kCell)};
}

} // namespace

class CanvasTest : public QObject {
    Q_OBJECT

    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<ThemeManager> tm_;
    std::unique_ptr<theme::IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<actions::Sysvars> sysvars_;
    std::unique_ptr<QWidget> window_;
    CanvasWidget* canvas_ = nullptr;
    ui::CommandInput* input_ = nullptr;
    Recorder* recorder_ = nullptr;
    quint64 ts_ = 1000; // dấu thời gian sự kiện chuột (ms)

    QRgb cross() const { return canvasColor(tok::c_cross).rgb(); }
    QRgb background() const { return canvasColor(tok::c_canvas).rgb(); }
    QImage shot() const { return canvas_->grab().toImage().convertToFormat(QImage::Format_RGB32); }

    void mouse(QEvent::Type type, QPoint pos, Qt::MouseButton button = Qt::NoButton, Qt::MouseButtons buttons = {},
               Qt::KeyboardModifiers mods = {}, quint64 at = 0)
    {
        ts_ = at ? at : ts_ + 16;
        QMouseEvent e(type, QPointF(pos), QPointF(canvas_->mapToGlobal(pos)), button, buttons, mods);
        e.setTimestamp(ts_);
        QApplication::sendEvent(canvas_, &e);
    }
    void move(QPoint p, Qt::MouseButtons buttons = {}, quint64 at = 0)
    {
        mouse(QEvent::MouseMove, p, Qt::NoButton, buttons, {}, at);
    }
    void press(QPoint p, Qt::KeyboardModifiers mods = {}, Qt::MouseButton b = Qt::LeftButton, quint64 at = 0)
    {
        mouse(QEvent::MouseButtonPress, p, b, b, mods, at);
    }
    void release(QPoint p, Qt::KeyboardModifiers mods = {}, Qt::MouseButton b = Qt::LeftButton)
    {
        mouse(QEvent::MouseButtonRelease, p, b, {}, mods);
    }
    void click(QPoint p, Qt::KeyboardModifiers mods = {})
    {
        press(p, mods);
        release(p, mods);
    }
    void leave()
    {
        QEvent e(QEvent::Leave);
        QApplication::sendEvent(canvas_, &e);
    }
    void focusCanvas()
    {
        window_->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(window_.get()));
        canvas_->setFocus(Qt::OtherFocusReason);
        QVERIFY(spinUntil([&] { return canvas_->hasFocus(); }));
    }
    void setVar(const char* name, int v) { sysvars_->set(QString::fromLatin1(name), v); }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        QVERIFY(theme::loadFonts());
        executor_ = std::make_unique<ui::UiExecutor>();
        tm_ = std::make_unique<ThemeManager>(*store_);
        icons_ = std::make_unique<theme::IconProvider>();
        translator_ = std::make_unique<i18n::Translator>();
        sysvars_ = std::make_unique<actions::Sysvars>();
        CanvasWidget::initSysvars(*sysvars_);
        QCOMPARE(sysvars_->get(u"PICKBOX"_s), 3);
        QCOMPARE(sysvars_->get(u"CURSORSIZE"_s), 5);

        window_ = std::make_unique<QWidget>();
        auto* col = new QVBoxLayout(window_.get());
        col->setContentsMargins({});
        col->setSpacing(0);
        canvas_ = new CanvasWidget(*sysvars_);
        input_ = new ui::CommandInput;
        col->addWidget(canvas_, 1);
        col->addWidget(input_);
        recorder_ = new Recorder(window_.get());
        recorder_->hide();
        window_->resize(800, 600);
        window_->show();
        QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
        focusCanvas();
    }

    void cleanupTestCase()
    {
        window_.reset();
        sysvars_.reset();
        translator_.reset();
        icons_.reset();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        sched::waitIdle();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    // Trạng thái đầu mỗi test: chờ, không chỗ nối, sysvar mặc định, đích là bộ ghi, chuột ngoài vùng vẽ.
    void init()
    {
        // Bỏ chỗ nối trước: chỗ nối của test trước bắt biến cục bộ theo tham chiếu, cancelSelection/setCursorState gọi lại
        // hit test (ASan: stack-use-after-return).
        canvas_->setHitTest({});
        canvas_->setAreaQuery({});
        canvas_->setBounds({});
        canvas_->setScenePainter({});
        canvas_->setHoverTip({});
        canvas_->cancelSelection();
        canvas_->setCursorState(CursorState::Idle);
        canvas_->setCursorBadge({});
        canvas_->setApertureVisible(false);
        setVar("CURSORSIZE", 5);
        setVar("PICKBOX", 3);
        setVar("APERTURE", 10);
        setVar("SELECTIONPREVIEW", 3);
        setVar("PICKDRAG", 2);
        setVar("PICKAUTO", 5);
        setVar("CURSORBADGE", 2);
        setVar("ROLLOVERTIPS", 1);
        canvas_->setKeySink(recorder_);
        recorder_->log.clear();
        leave();
    }

    // DoD-U: tên "Vùng vẽ", vai trò Canvas, Tab tới và rời được, viền focus --c-focus khi Tab tới.
    void a11y()
    {
        QCOMPARE(canvas_->accessibleName(), u"Vùng vẽ"_s); // i18n-ok: tên gốc tiếng Việt
        QAccessibleInterface* ai = QAccessible::queryAccessibleInterface(canvas_);
        QVERIFY(ai);
        QCOMPARE(ai->role(), QAccessible::Canvas);
        QCOMPARE(ai->text(QAccessible::Name), canvas_->accessibleName());

        QWidget w;
        auto* col = new QVBoxLayout(&w);
        auto* canvas = new CanvasWidget(*sysvars_);
        auto* button = new ui::Button(u"OK"_s); // i18n-ok: chữ mẫu của test
        col->addWidget(canvas, 1);
        col->addWidget(button);
        w.resize(400, 300);
        w.show();
        w.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        const QStringList names = a11y::checkNames(&w);
        QVERIFY2(names.isEmpty(), qPrintable(names.join(u'\n')));
        QStringList order;
        const QStringList focus = a11y::checkFocus(&w, &order);
        QVERIFY2(focus.isEmpty(), qPrintable(focus.join(u'\n')));
        QCOMPARE(order.size(), 2);
        w.close();
        focusCanvas();
    }

    // 07.1 §9.1: 6 trạng thái con trỏ; con trỏ hệ thống ẩn khi vùng vẽ tự vẽ; huy hiệu tắt được; rời vùng vẽ thì sạch.
    void cursorStates()
    {
        const QPoint c(300, 200);
        move(c);
        struct Want {
            CursorState state;
            bool cross, pick, ring;
            Qt::CursorShape shape;
        };
        const Want wants[] = {
            {CursorState::Idle, true, true, false, Qt::BlankCursor},
            {CursorState::PickPoint, true, false, false, Qt::BlankCursor},
            {CursorState::PickObject, false, true, false, Qt::BlankCursor},
            {CursorState::Pan, false, false, false, Qt::OpenHandCursor},
            {CursorState::Zoom, false, false, false, Qt::BitmapCursor},
            {CursorState::Busy, true, false, true, Qt::BlankCursor},
        };
        const QColor accent = canvasColor(tok::c_accent);
        const QRect ringArea(c + QPoint(8, 8), QSize(14, 14)); // CanvasOverlay: vòng 14 px ở x, y + --sp-4
        for (const Want& w : wants) {
            canvas_->setCursorState(w.state);
            QCOMPARE(canvas_->cursorState(), w.state);
            QCOMPARE(canvas_->cursor().shape(), w.shape);
            const QImage img = shot();
            const int h = canvas_->overlay().crossHalf, p = canvas_->overlay().pickHalf;
            QCOMPARE(img.pixel(c.x() + h - 1, c.y()) == cross(), w.cross); // đầu chữ thập, ngoài ô chọn
            QCOMPARE(img.pixel(c.x(), c.y() - h + 1) == cross(), w.cross);
            QCOMPARE(img.pixel(c.x() + p, c.y() - p) == cross(), w.pick); // góc ô chọn
            QCOMPARE(near(img, accent, ringArea) > 0, w.ring);
            if (!w.cross && !w.pick)
                QCOMPARE(count(img, cross()), 0);
            if (w.state == CursorState::Zoom)
                QVERIFY(!canvas_->cursor().pixmap().isNull());
        }
        // Bàn tay mở → nắm khi giữ chuột (prototype cur-pan / cur-panning).
        canvas_->setCursorState(CursorState::Pan);
        press(c);
        QCOMPARE(canvas_->cursor().shape(), Qt::ClosedHandCursor);
        release(c);
        QCOMPARE(canvas_->cursor().shape(), Qt::OpenHandCursor);

        // Huy hiệu: ô viền --c-dyn-b cạnh con trỏ; CURSORBADGE 1 tắt.
        canvas_->setCursorState(CursorState::Idle);
        canvas_->setCursorBadge(u"move"_s);
        const QRgb border = tm_->color(Sem::DynTooltipBorder).rgb();
        const QRect badge(c.x() + 10, c.y() - 24, 18, 18);
        const QImage withBadge = shot();
        QCOMPARE(count(withBadge, border, badge) - count(withBadge, border, badge.adjusted(1, 1, -1, -1)), 4 * 18 - 4);
        setVar("CURSORBADGE", 1);
        QCOMPARE(count(shot(), border, badge), 0);
        setVar("CURSORBADGE", 2);
        canvas_->setCursorBadge({});

        // Rời vùng vẽ: không còn chữ thập, ô chọn.
        leave();
        QVERIFY(!canvas_->overlay().mouse);
        QCOMPARE(count(shot(), cross()), 0);
    }

    // Đổi PICKBOX, CURSORSIZE, APERTURE → vẽ đúng cỡ (đếm điểm ảnh màu --c-cross).
    void sizes()
    {
        canvas_->clearFocus(); // không viền focus ở mép
        const QPoint c(200, 150);
        const int w = canvas_->width(), h = canvas_->height();
        canvas_->setCursorState(CursorState::PickObject);
        move(c);
        for (const int pick : {3, 7, 0, 50}) {
            setVar("PICKBOX", pick);
            QCOMPARE(count(shot(), cross()), 8 * pick); // viền cạnh 2·PICKBOX + 1
        }
        canvas_->setCursorState(CursorState::PickPoint);
        auto crossCount = [&](int half) {
            const int across = std::min(c.x() + half, w - 1) - std::max(c.x() - half, 0) + 1;
            const int down = std::min(c.y() + half, h - 1) - std::max(c.y() - half, 0) + 1;
            return across + down - 1;
        };
        for (const int percent : {1, 5, 10, 100}) {
            setVar("CURSORSIZE", percent);
            const int half = (percent * std::max(w, h) + 99) / 100;
            QCOMPARE(canvas_->overlay().crossHalf, half);
            QCOMPARE(count(shot(), cross()), crossCount(half));
        }
        // CURSORSIZE 100: chữ thập hết vùng vẽ từ mọi điểm.
        move(QPoint(0, 0));
        QCOMPARE(count(shot(), cross()), w + h - 1);
        move(c);
        // APERTURE chỉ khi chỗ nối báo đang bắt điểm; viền cắt chữ thập ở 4 điểm.
        setVar("CURSORSIZE", 5);
        const int half = (5 * std::max(w, h) + 99) / 100;
        canvas_->setApertureVisible(true);
        setVar("APERTURE", 10);
        QCOMPARE(count(shot(), cross()), crossCount(half) + 8 * 10 - 4);
        canvas_->setApertureVisible(false);
        QCOMPARE(count(shot(), cross()), crossCount(half));
        focusCanvas();
    }

    // 07.1 §9.3: nhấp, kéo dưới ngưỡng, Window, Crossing, khung hai cú nhấp, PICKDRAG 0/1/2, Esc và chuột phải hủy.
    void clickAndBox()
    {
        const QRectF object(100, 100, 40, 40);
        canvas_->setHitTest([&](QPointF p, double pick) -> std::optional<quint64> {
            return object.adjusted(-pick, -pick, pick, pick).contains(p) ? std::optional<quint64>(7) : std::nullopt;
        });
        QSignalSpy picks(canvas_, &CanvasWidget::picked);
        QSignalSpy boxes(canvas_, &CanvasWidget::boxSelected);
        const int drag = QApplication::startDragDistance();

        click(QPoint(120, 120), Qt::ShiftModifier);
        QCOMPARE(picks.size(), 1);
        QCOMPARE(picks.at(0).at(0).toPointF(), QPointF(120, 120));
        QCOMPARE(picks.at(0).at(1).value<Qt::KeyboardModifiers>(), Qt::KeyboardModifiers(Qt::ShiftModifier));
        press(QPoint(120, 120)); // kéo dưới ngưỡng vẫn là nhấp
        move(QPoint(120 + drag - 1, 120), Qt::LeftButton);
        QVERIFY(!canvas_->selecting());
        release(QPoint(120 + drag - 1, 120));
        QCOMPARE(picks.size(), 2);

        press(QPoint(300, 200)); // trái → phải: Window
        move(QPoint(400, 260), Qt::LeftButton);
        QVERIFY(canvas_->selecting());
        QCOMPARE(canvas_->overlay().mode, SelectMode::Window);
        release(QPoint(400, 260));
        QCOMPARE(boxes.size(), 1);
        QCOMPARE(boxes.at(0).at(0).value<QPolygonF>(),
                 QPolygonF({QPointF(300, 200), QPointF(400, 200), QPointF(400, 260), QPointF(300, 260)}));
        QCOMPARE(boxes.at(0).at(1).value<SelectMode>(), SelectMode::Window);
        QVERIFY(canvas_->overlay().shape.isEmpty());

        press(QPoint(400, 200)); // phải → trái: Crossing, Shift lúc nhả
        move(QPoint(300, 260), Qt::LeftButton);
        QCOMPARE(canvas_->overlay().mode, SelectMode::Crossing);
        release(QPoint(300, 260), Qt::ShiftModifier);
        QCOMPARE(boxes.size(), 2);
        QCOMPARE(boxes.at(1).at(1).value<SelectMode>(), SelectMode::Crossing);
        QCOMPARE(boxes.at(1).at(2).value<Qt::KeyboardModifiers>(), Qt::KeyboardModifiers(Qt::ShiftModifier));

        press(QPoint(300, 200)); // Esc hủy khung đang kéo; phím không tới đích
        move(QPoint(400, 260), Qt::LeftButton);
        QTest::keyClick(canvas_, Qt::Key_Escape);
        QVERIFY(!canvas_->selecting());
        QVERIFY(canvas_->overlay().shape.isEmpty());
        release(QPoint(400, 260));
        QCOMPARE(boxes.size(), 2);
        QVERIFY(recorder_->log.isEmpty());
        QTest::keyClick(canvas_, Qt::Key_Escape); // không còn khung: Esc tới dòng lệnh
        QCOMPARE(recorder_->log.size(), 1);

        press(QPoint(300, 200)); // chuột phải hủy
        move(QPoint(400, 260), Qt::LeftButton);
        press(QPoint(400, 260), {}, Qt::RightButton);
        QVERIFY(!canvas_->selecting());
        release(QPoint(400, 260));
        QCOMPARE(boxes.size(), 2);

        click(QPoint(500, 300)); // nhấp chỗ trống: khung hai cú nhấp
        QCOMPARE(picks.size(), 2);
        QVERIFY(canvas_->selecting());
        move(QPoint(450, 350));
        QCOMPARE(canvas_->overlay().mode, SelectMode::Crossing);
        QCOMPARE(canvas_->overlay().shape.size(), 4);
        click(QPoint(450, 350));
        QCOMPARE(boxes.size(), 3);
        QCOMPARE(boxes.at(2).at(1).value<SelectMode>(), SelectMode::Crossing);
        QVERIFY(!canvas_->selecting());
        click(QPoint(500, 300)); // hai cú nhấp trùng chỗ: không chọn
        click(QPoint(500, 300));
        QCOMPARE(boxes.size(), 3);
        QVERIFY(!canvas_->selecting());

        setVar("PICKDRAG", 1); // chỉ nhấn-kéo: nhấp chỗ trống là picked
        click(QPoint(500, 300));
        QCOMPARE(picks.size(), 3);
        QVERIFY(!canvas_->selecting());
        setVar("PICKDRAG", 0); // chỉ hai cú nhấp: nhả sau khi kéo chưa xong
        press(QPoint(300, 200));
        move(QPoint(400, 260), Qt::LeftButton);
        release(QPoint(400, 260));
        QCOMPARE(boxes.size(), 3);
        QVERIFY(canvas_->selecting());
        move(QPoint(420, 280));
        click(QPoint(420, 280));
        QCOMPARE(boxes.size(), 4);
        QCOMPARE(boxes.at(3).at(0).value<QPolygonF>().at(2), QPointF(420, 280));

        // Chọn điểm: lấy điểm lúc nhấn, không khung.
        canvas_->setCursorState(CursorState::PickPoint);
        press(QPoint(600, 100));
        QCOMPARE(picks.size(), 4);
        move(QPoint(650, 150), Qt::LeftButton);
        release(QPoint(650, 150));
        QVERIFY(!canvas_->selecting());
        QCOMPARE(boxes.size(), 4);
    }

    // Lasso (P1): nhấn giữ rồi kéo (PICKAUTO bit 4); Space luân phiên Window / Crossing / Fence, không tới đích.
    void lasso()
    {
        QSignalSpy boxes(canvas_, &CanvasWidget::boxSelected);
        const auto hold = quint64(QGuiApplication::styleHints()->mousePressAndHoldInterval());
        press(QPoint(200, 200), {}, Qt::LeftButton, 10'000);
        move(QPoint(215, 205), Qt::LeftButton, 10'000 + hold);
        QCOMPARE(canvas_->overlay().mode, SelectMode::WPolygon); // sang phải: Window
        const QPoint path[] = {{240, 220}, {260, 260}, {230, 290}, {205, 250}};
        for (const QPoint& p : path)
            move(p, Qt::LeftButton);
        QCOMPARE(canvas_->overlay().shape.size(), 6);
        const SelectMode cycle[] = {SelectMode::CPolygon, SelectMode::Fence, SelectMode::WPolygon, SelectMode::CPolygon};
        for (const SelectMode m : cycle) {
            QTest::keyClick(canvas_, Qt::Key_Space);
            QCOMPARE(canvas_->overlay().mode, m);
        }
        QVERIFY(recorder_->log.isEmpty());
        release(QPoint(205, 250));
        QCOMPARE(boxes.size(), 1);
        QCOMPARE(boxes.at(0).at(1).value<SelectMode>(), SelectMode::CPolygon);
        QCOMPARE(boxes.at(0).at(0).value<QPolygonF>().size(), 6);
        QCOMPARE(boxes.at(0).at(0).value<QPolygonF>().front(), QPointF(200, 200));

        press(QPoint(400, 200), {}, Qt::LeftButton, 20'000); // sang trái: Crossing
        move(QPoint(380, 210), Qt::LeftButton, 20'000 + hold);
        QCOMPARE(canvas_->overlay().mode, SelectMode::CPolygon);
        move(QPoint(370, 250), Qt::LeftButton);
        release(QPoint(370, 250));
        QCOMPARE(boxes.size(), 2);
        QCOMPARE(boxes.at(1).at(1).value<SelectMode>(), SelectMode::CPolygon);

        press(QPoint(200, 200), {}, Qt::LeftButton, 30'000); // kéo ngay: khung
        move(QPoint(260, 240), Qt::LeftButton, 30'000 + hold / 4);
        QCOMPARE(canvas_->overlay().mode, SelectMode::Window);
        QCOMPARE(canvas_->overlay().shape.size(), 4);
        canvas_->cancelSelection();
        release(QPoint(260, 240));
        setVar("PICKAUTO", 1); // tắt lasso: nhấn giữ vẫn là khung
        press(QPoint(200, 200), {}, Qt::LeftButton, 40'000);
        move(QPoint(260, 240), Qt::LeftButton, 40'000 + hold);
        QCOMPARE(canvas_->overlay().mode, SelectMode::Window);
        release(QPoint(260, 240));
        QCOMPARE(boxes.size(), 3);
    }

    // Lệnh hỏi F / WP / CP: mỗi cú nhấp thêm đỉnh, đỉnh cuối theo con trỏ; finishPolygon (Enter của lệnh) phát vùng.
    void polygon()
    {
        QSignalSpy boxes(canvas_, &CanvasWidget::boxSelected);
        canvas_->setCursorState(CursorState::PickPoint);
        canvas_->beginPolygon(SelectMode::WPolygon);
        for (const QPoint p : {QPoint(100, 100), QPoint(300, 100), QPoint(200, 250)})
            click(p);
        move(QPoint(120, 240));
        QCOMPARE(canvas_->overlay().shape.size(), 4);
        QCOMPARE(canvas_->overlay().shape.back(), QPointF(120, 240));
        leave();
        QCOMPARE(canvas_->overlay().shape.size(), 3);
        QVERIFY(canvas_->finishPolygon());
        QCOMPARE(boxes.size(), 1);
        QCOMPARE(boxes.at(0).at(0).value<QPolygonF>(),
                 QPolygonF({QPointF(100, 100), QPointF(300, 100), QPointF(200, 250)}));
        QCOMPARE(boxes.at(0).at(1).value<SelectMode>(), SelectMode::WPolygon);
        QVERIFY(!canvas_->selecting());

        canvas_->beginPolygon(SelectMode::Fence); // Fence cần ≥ 2 đỉnh
        click(QPoint(100, 100));
        QVERIFY(!canvas_->finishPolygon());
        QCOMPARE(boxes.size(), 1);
        canvas_->beginPolygon(SelectMode::Fence);
        click(QPoint(100, 100));
        click(QPoint(300, 120));
        QVERIFY(canvas_->finishPolygon());
        QCOMPARE(boxes.at(1).at(1).value<SelectMode>(), SelectMode::Fence);

        canvas_->beginPolygon(SelectMode::CPolygon); // Esc của lệnh
        for (const QPoint p : {QPoint(100, 100), QPoint(300, 100), QPoint(200, 250)})
            click(p);
        canvas_->cancelSelection();
        QVERIFY(!canvas_->selecting());
        QVERIFY(canvas_->overlay().shape.isEmpty());
        QCOMPARE(boxes.size(), 2);
    }

    // Tiêu chí 4: Window viền liền, Crossing viền đứt, phân biệt ở ảnh xám cả khi màu B đặt bằng màu A.
    void windowCrossingGray()
    {
        auto& sem = tm_->semantic();
        sem.setColor(Sem::SelectionCrossing, tm_->color(Sem::SelectionWindow));
        sem.setColor(Sem::SelectionCrossingFill, tm_->color(Sem::SelectionWindowFill));
        QCOMPARE(tm_->color(Sem::SelectionCrossing), tm_->color(Sem::SelectionWindow));
        auto topEdge = [&](QPoint a, QPoint b) { // hàng điểm ảnh xám của cạnh trên, xa góc và con trỏ
            press(a);
            move(b, Qt::LeftButton);
            const QImage img = shot();
            canvas_->cancelSelection();
            release(b);
            QList<int> row;
            for (int x = 110; x <= 290; ++x)
                row << qGray(img.pixel(x, 100));
            return row;
        };
        const QList<int> window = topEdge(QPoint(100, 100), QPoint(300, 200));
        const QList<int> crossing = topEdge(QPoint(300, 100), QPoint(100, 200));
        sem.reset(Sem::SelectionCrossing);
        sem.reset(Sem::SelectionCrossingFill);
        auto changes = [](const QList<int>& row) {
            int n = 0;
            for (qsizetype i = 1; i < row.size(); ++i)
                n += row[i] != row[i - 1];
            return n;
        };
        QCOMPARE(changes(window), 0); // liền
        QVERIFY(window.front() != qGray(background()));
        QVERIFY2(changes(crossing) >= 30, qPrintable(QString::number(changes(crossing)))); // đứt 6/4 px: 36 lần đổi
        QVERIFY(crossing.contains(window.front()));
    }

    // Tiêu chí 2: 1.000 lần rê, hit test giả O(1): chỉ vẽ lại vùng bẩn; tô sáng mới có ở lần vẽ kế tiếp.
    void hoverPartialRepaint()
    {
        struct Seen {
            std::optional<quint64> hover;
            QRegion region;
        };
        std::vector<Seen> scenes;
        canvas_->setHitTest(gridHit);
        canvas_->setBounds(gridBounds);
        canvas_->setScenePainter([&](QPainter& p, const QRegion& dirty, const CanvasWidget::Highlight& hl) {
            if (hl.hover)
                p.fillRect(gridBounds(*hl.hover), Qt::white); // tô sáng giả
            scenes.push_back({hl.hover, dirty});
        });
        move(QPoint(10, 10));
        QVERIFY(spinUntil([&] { return !scenes.empty(); }));
        spinFor(50ms);
        Paints paints;
        paints.w = canvas_;
        canvas_->installEventFilter(&paints);
        QSignalSpy hovers(canvas_, &CanvasWidget::hovered);
        int changed = 0;
        for (int i = 0; i < 1000; ++i) {
            const QPoint p(15 + (i * 37) % 700, 15 + (i * 53) % 500);
            const std::optional<quint64> before = canvas_->hoveredId(), want = gridHit(p, 0);
            const int painted = paints.count;
            scenes.clear();
            move(p);
            QCOMPARE(canvas_->hoveredId(), want);
            QVERIFY(spinUntil([&] { return paints.count > painted; }));
            QVERIFY(!scenes.empty());
            QCOMPARE(scenes.back().hover, want); // lần vẽ kế tiếp đã có tô sáng mới
            if (want != before) {
                ++changed;
                for (const std::optional<quint64>& id : {before, want})
                    if (id)
                        QVERIFY((QRegion(gridBounds(*id).toRect()) - paints.last).isEmpty());
            }
        }
        canvas_->removeEventFilter(&paints);
        QCOMPARE(paints.full, 0);
        QCOMPARE(hovers.size(), changed);
        qInfo("1.000 lần rê: %d lần vẽ, %d lần đổi tô sáng; lần vẽ lớn nhất %.1f%% diện tích vùng vẽ", paints.count,
              changed, paints.maxShare * 100);
    }

    // Tiêu chí 2: khung Crossing đang kéo, truy vấn giả trả 10.000 id (đổi mỗi lần): CPU mỗi khung ≤ 8 ms.
    void crossingPreview()
    {
        std::vector<quint64> a(10'000), b(10'000);
        for (quint64 i = 0; i < 10'000; ++i) {
            a[i] = i + 1;
            b[i] = i + 20'001;
        }
        bool flip = false;
        int queries = 0;
        canvas_->setAreaQuery([&](const QPolygonF&, SelectMode mode) {
            ++queries;
            [&] { QCOMPARE(mode, SelectMode::Crossing); }();
            flip = !flip;
            return flip ? a : b;
        });
        canvas_->setBounds([](quint64 id) { return QRectF(double(id % 700), double(id % 500), 4, 4); });
        quint64 sum = 0;
        size_t lastPreview = 0;
        canvas_->setScenePainter([&](QPainter&, const QRegion&, const CanvasWidget::Highlight& hl) {
            for (const quint64 id : hl.preview) // RND giả: đọc mọi id tô sáng
                sum += id;
            lastPreview = hl.preview.size();
        });
        Paints paints;
        paints.w = canvas_;
        canvas_->installEventFilter(&paints);
        press(QPoint(700, 100));
        move(QPoint(680, 110), Qt::LeftButton);
        QVERIFY(spinUntil([&] { return paints.count > 0; }));
        std::vector<double> cpu;
        for (int i = 0; i < 100; ++i) {
            const int painted = paints.count;
            const auto start = test::threadCpu();
            move(QPoint(670 - i * 5, 120 + i * 3), Qt::LeftButton);
            QVERIFY(spinUntil([&] { return paints.count > painted; }));
            cpu.push_back(ms(test::threadCpu() - start));
            QCOMPARE(canvas_->previewIds().size(), size_t(10'000));
            QCOMPARE(lastPreview, size_t(10'000));
        }
        canvas_->removeEventFilter(&paints);
        QSignalSpy boxes(canvas_, &CanvasWidget::boxSelected);
        release(QPoint(175, 417));
        QCOMPARE(boxes.size(), 1);
        QVERIFY(canvas_->previewIds().empty());
        QVERIFY(sum > 0 && queries >= 100);
        const double p50 = percentile(cpu, 0.5), p99 = percentile(cpu, 0.99);
        qInfo("khung Crossing, 10.000 id xem trước: CPU mỗi khung p50 %.2f ms, p99 %.2f ms, max %.2f ms", p50, p99,
              *std::max_element(cpu.begin(), cpu.end()));
        QVERIFY2(withinCpu(p99, 8.0), qPrintable(QString::number(p99)));
    }

    // Tiêu chí 1: di chuột tới hết khung có chữ thập mới.
    void inputToPhoton()
    {
        canvas_->setCursorState(CursorState::PickPoint);
        QWindow* win = window_->windowHandle();
        auto toWindow = [&](QPoint p) { return canvas_->mapTo(window_.get(), p); };
        for (int i = 0; i < 20; ++i) // khởi động: lần vẽ đầu, raster
            QTest::mouseMove(win, toWindow(QPoint(100 + i, 100)));
        spinFor(50ms);
        probe::enable();
        probe::reset();
        std::vector<double> cpu;
        QPoint p, previous;
        constexpr int kN = 200;
        for (int i = 0; i < kN; ++i) {
            previous = p;
            p = QPoint(150 + (i * 7) % 400, 120 + (i * 11) % 300);
            const auto start = probe::Clock::now();
            const auto cpu0 = test::threadCpu();
            // Đăng mẫu trước: QTest::mouseMove trên QWindow tự chạy vòng sự kiện, khung có thể xong ngay trong đó.
            probe::untilFrame(probe::Op::ViewportFrame, canvas_, start);
            QTest::mouseMove(win, toWindow(p));
            QVERIFY2(spinUntil([&] { return probeOp("viewport_frame")["count"].toInt() == i + 1; }, 2s),
                     qPrintable(u"mẫu %1, đếm %2"_s.arg(i).arg(probeOp("viewport_frame")["count"].toInt())));
            cpu.push_back(ms(test::threadCpu() - cpu0));
        }
        const QJsonObject op = probeOp("viewport_frame");
        probe::disable();
        // Khung thật (backing store): chữ thập ở chỗ mới, chỗ cũ đã sạch.
        const int h = canvas_->overlay().crossHalf;
        const QImage frame = window_->screen()->grabWindow(window_->winId()).toImage().convertToFormat(
            QImage::Format_RGB32);
        const qreal dpr = frame.devicePixelRatio();
        auto at = [&](QPoint q) {
            const QPoint w = toWindow(q);
            return frame.pixel(qFloor(w.x() * dpr), qFloor(w.y() * dpr));
        };
        QCOMPARE(at(p + QPoint(h - 1, 0)), cross());
        QCOMPARE(at(p + QPoint(0, h - 1)), cross());
        const QPoint old = previous + QPoint(h - 1, 0); // đầu chữ thập cũ: chỉ còn màu chữ thập nếu nằm trên chữ thập mới
        const bool onNew = (old.y() == p.y() && std::abs(old.x() - p.x()) <= h) ||
                           (old.x() == p.x() && std::abs(old.y() - p.y()) <= h);
        QCOMPARE(at(old) == cross(), onNew);
        const double p99 = percentile(cpu, 0.99);
        qInfo("di chuột → khung có chữ thập mới: thời gian thật p50 %.2f ms, p99 %.2f ms (%d mẫu); CPU p50 %.2f ms, "
              "p99 %.2f ms",
              op["p50_ms"].toDouble(), op["p99_ms"].toDouble(), op["count"].toInt(), percentile(cpu, 0.5), p99);
        QVERIFY2(withinCpu(p99, 25.0), qPrintable(QString::number(p99)));
    }

    // Tiêu chí 3: rảnh 5 s, chuột trong vùng vẽ: không vẽ, không timer.
    void idle()
    {
        canvas_->setHitTest(gridHit);
        canvas_->setBounds(gridBounds);
        focusCanvas();
        move(QPoint(30, 10)); // ô trống của lưới: không tô sáng, không chờ tooltip
        QVERIFY(!canvas_->hoveredId());
        spinFor(300ms);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        Paints paints;
        paints.w = canvas_;
        canvas_->installEventFilter(&paints);
        QTimer end;
        end.setSingleShot(true);
        TimerLog timers(&end);
        qApp->installEventFilter(&timers);
        QEventLoop loop;
        QObject::connect(&end, &QTimer::timeout, &loop, &QEventLoop::quit);
        end.start(5s);
        loop.exec();
        qApp->removeEventFilter(&timers);
        canvas_->removeEventFilter(&paints);
        QCOMPARE(paints.count, 0);
        QVERIFY2(timers.seen.isEmpty(), qPrintable(timers.seen.join(u", "_s)));
    }

    // Tiêu chí 4: 10.000 phím khi focus ở vùng vẽ (xen 204 chuỗi commit của bộ gõ): đích nhận đủ, đúng thứ tự.
    void keys10k()
    {
        focusCanvas();
        QStringList want;
        struct Special {
            Qt::Key key;
            const char* text;
        };
        const Special specials[] = {{Qt::Key_Backspace, "\b"}, {Qt::Key_Return, "\r"}, {Qt::Key_Space, " "},
                                    {Qt::Key_Escape, "\x1b"},  {Qt::Key_Up, ""},       {Qt::Key_Down, ""},
                                    {Qt::Key_Enter, "\r"}};
        const QString vietnamese = u"ăâđêôơưĐỆ"_s; // i18n-ok: chữ commit mẫu của bộ gõ
        int keys = 0, commits = 0;
        for (int i = 0; keys < 10'000; ++i) {
            if (i % 50 == 49) {
                ++commits;
                QInputMethodEvent ime;
                ime.setCommitString(QString(vietnamese.at((i / 50) % vietnamese.size())));
                QApplication::sendEvent(canvas_, &ime);
                want << u"i:"_s + ime.commitString();
                continue;
            }
            Qt::Key key;
            QString text;
            if (i % 10 == 9) {
                const Special& s = specials[(i / 10) % std::size(specials)];
                key = s.key;
                text = QString::fromLatin1(s.text);
            } else if (i % 3 == 0) {
                key = Qt::Key(Qt::Key_0 + i % 10);
                text = QString(QChar(char16_t(u'0' + i % 10)));
            } else {
                key = Qt::Key(Qt::Key_A + i % 26);
                text = QString(QChar(char16_t(u'a' + i % 26)));
            }
            QTest::sendKeyEvent(QTest::Click, canvas_, key, text, Qt::NoModifier);
            want << u"k%1:%2"_s.arg(key).arg(text);
            ++keys;
        }
        QCOMPARE(commits, 204);
        QCOMPARE(recorder_->log.size(), want.size());
        QCOMPARE(recorder_->log, want);
        QVERIFY(canvas_->hasFocus());
    }

    // Đích thật: ô dòng lệnh (CommandInput). Chữ, Backspace, Enter, bộ gõ (preedit, commit NFD → NFC), bỏ qua bộ gõ
    // khi chờ tên lệnh (Qt::ImEnabled của vùng vẽ theo ô), ô soạn chữ của bộ gõ đặt ở con trỏ chữ của ô.
    void commandInputSink()
    {
        focusCanvas();
        canvas_->setKeySink(input_);
        input_->clear();
        input_->setAwaitingCommand(false);
        QVERIFY(canvas_->inputMethodQuery(Qt::ImEnabled).toBool());
        QTest::keyClicks(canvas_, u"line"_s);
        QCOMPARE(input_->text(), u"line"_s);
        QInputMethodEvent preedit(u"o"_s, {});
        QApplication::sendEvent(canvas_, &preedit);
        QInputMethodEvent commit;
        commit.setCommitString(u"ơ"_s); // i18n-ok: chữ bộ gõ mẫu
        QApplication::sendEvent(canvas_, &commit);
        QInputMethodEvent e;
        e.setCommitString(u"e"_s);
        QApplication::sendEvent(canvas_, &e);
        QInputMethodEvent mark;
        mark.setCommitString(QString(QChar(0x0302))); // dấu mũ rời (NFD tách lượt)
        QApplication::sendEvent(canvas_, &mark);
        QCOMPARE(input_->text(), u"lineơê"_s); // i18n-ok: kết quả NFC
        QTest::keyClick(canvas_, Qt::Key_Backspace);
        QCOMPARE(input_->text(), u"lineơ"_s); // i18n-ok: kết quả mong đợi
        QSignalSpy returned(input_, &QLineEdit::returnPressed);
        QTest::keyClick(canvas_, Qt::Key_Return);
        QCOMPARE(returned.size(), 1);

        input_->clear();
        input_->setAwaitingCommand(true); // bỏ qua bộ gõ: vùng vẽ báo ImEnabled = false, chữ ra hoa như ô
        QVERIFY(!canvas_->inputMethodQuery(Qt::ImEnabled).toBool());
        QTest::keyClicks(canvas_, u"lm"_s);
        QCOMPARE(input_->text(), u"LM"_s);
        input_->setAwaitingCommand(false);
        QVERIFY(canvas_->inputMethodQuery(Qt::ImEnabled).toBool());

        const QRect inInput = input_->inputMethodQuery(Qt::ImCursorRectangle).toRect();
        const QPoint shift = input_->mapTo(window_.get(), QPoint()) - canvas_->mapTo(window_.get(), QPoint());
        QCOMPARE(canvas_->inputMethodQuery(Qt::ImCursorRectangle).toRect(), inInput.translated(shift));
        canvas_->setKeySink(nullptr);
        QVERIFY(!canvas_->inputMethodQuery(Qt::ImEnabled).toBool());
        canvas_->setKeySink(recorder_);
        input_->clear();
    }

    // Phím tắt của ActionRegistry (Toàn cục trên cửa sổ, Vùng vẽ qua addViewport) chạy, không bị vùng vẽ nuốt.
    void shortcuts()
    {
        focusCanvas();
        keymap::Keymap keymap(*store_);
        actions::ActionRegistry actions(keymap, *sysvars_);
        QStringList macros;
        actions.setSink([&](const QString& m) { macros << m; });
        actions.install(window_.get());
        actions.addViewport(canvas_);
        QTest::keyClick(canvas_, Qt::Key_F3);
        QCOMPARE(macros.size(), 1);
        QTest::keyClick(canvas_, Qt::Key_Delete);
        QCOMPARE(macros.size(), 2);
        QVERIFY2(macros.back().contains(u"ERASE"_s), qPrintable(macros.back()));
        QTest::keyClick(canvas_, Qt::Key_S, Qt::ControlModifier);
        QCOMPARE(macros.size(), 3);
        QVERIFY2(macros.back().contains(u"QSAVE"_s), qPrintable(macros.back()));
        QVERIFY(recorder_->log.isEmpty());
        QTest::keyClick(canvas_, Qt::Key_L); // chữ vẫn tới đích
        QCOMPARE(recorder_->log, QStringList{u"k%1:l"_s.arg(int(Qt::Key_L))});
        QCOMPARE(macros.size(), 3);
    }

    // Tooltip rê (P1, ROLLOVERTIPS): CursorTip hiện sau thời gian chờ, ẩn khi rời đối tượng; tắt được.
    void hoverTip()
    {
        canvas_->setHitTest(gridHit);
        canvas_->setHoverTip([](quint64 id) { return u"Line %1"_s.arg(id); }); // i18n-ok: chữ của chỗ nối giả
        move(QPoint(10, 10));
        auto tip = [&] { return canvas_->findChild<ui::CursorTip*>(); };
        QVERIFY(spinUntil([&] { return tip() && tip()->isVisible(); }, 5s));
        QCOMPARE(tip()->text(), u"Line 1"_s);
        move(QPoint(30, 10)); // ô trống
        QVERIFY(!tip()->isVisible());
        setVar("ROLLOVERTIPS", 0);
        move(QPoint(10, 10));
        QCOMPARE(canvas_->hoveredId(), std::optional<quint64>(1));
        spinFor(std::chrono::milliseconds(canvas_->style()->styleHint(QStyle::SH_ToolTip_WakeUpDelay) + 200));
        QVERIFY(!tip()->isVisible());
    }
};

QTEST_MAIN(CanvasTest)
#include "canvas_test.moc"
