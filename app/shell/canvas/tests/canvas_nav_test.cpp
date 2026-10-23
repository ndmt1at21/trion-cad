// UIA-008 · Điều hướng vùng vẽ (Navigation, TempView) và hiển thị dần khi mở tệp lớn (LoadGate). Nhóm theo tiêu chí
// nghiệm thu (_handoff/ui-impl/w9-uia008.md):
//   1  Pan/zoom BENCH-STD 1 triệu entity ≤ 16,6 ms/khung: Hoãn (cần RND-001, INT-170); phần widget đo ở loadNavigate.
//   2  wheelAnchor: 100 nấc lăn vào rồi ra (ở tọa độ ≈ 10³ và ≈ 10⁹), thêm 200 nấc sâu: điểm dưới con trỏ lệch ≤ 0,5 px
//      sau từng nấc; pan 1 px ở tọa độ ≈ 10⁹ khi zoom sâu không bị bỏ. sceneTransform: chỗ nối vẽ cảnh theo
//      TempView::transform() vẽ dấu ở điểm bản vẽ: sau lăn, pan dấu vẫn ở đúng chỗ (đếm điểm ảnh).
//   3  loadNavigate: trong lúc nạp (việc NT-013 giả công bố lô mỗi 5 ms), 120 khung lăn + kéo chuột giữa: CPU luồng UI
//      mỗi khung p99 ≤ 33 ms (≥ 30 fps, release), lô đầu công bố → khung vẽ (phần widget của "khung đầu ≤ 2 s"; phần
//      nạp tệp 100 MB: Hoãn, cần DOC-016, RND). loadCancel: hủy → finished(Cancelled) ≤ 50 ms (release), cả từ nút hủy
//      của thanh trạng thái (BackgroundJobs::cancel); mở tệp khác khi đang nạp hủy việc cũ.
//   4  editWaits: lệnh sửa khi đang nạp: CursorTip + notice báo chờ, lệnh xem/đo/sysvar đi thẳng, lệnh sau thay lệnh
//      trước, chạy khi nạp xong. escCancels: Esc ở vùng vẽ, ở ô dòng lệnh bỏ lệnh chờ (không tới dòng lệnh); nạp bị
//      hủy thì lệnh chờ bị bỏ (loadCancel).
//   §9.2: wheelSpeed (ZOOMFACTOR), wheelReverse (ZOOMWHEEL), middlePan (MBUTTONPAN, bàn tay nắm, trả trạng thái lệnh,
//      khung chọn bị bỏ), middleDoubleClick (zoom toàn bộ), realtimePanZoom (trạng thái Pan/Zoom kéo trái), touchpad
//      (chụm, Pan native, kéo hai ngón có pha/TouchPad, chuột lăn macOS, Ctrl + lăn), viewSteps (UNDOCTL 16).
//   DoD-U, DoD-H: idle (sau điều hướng và nạp: 0 lần vẽ, 0 QTimerEvent).
#include "app/shell/canvas/CanvasWidget.h"
#include "app/shell/canvas/LoadGate.h"
#include "app/shell/canvas/Navigation.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/core/i18n/Translator.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/cmdline/CommandInput.h"
#include "app/widgets/message/Messages.h"
#include "core/base/jobs/BackgroundJobs.h"
#include "core/base/sched/Scheduler.h"
#include "tests/timing.h"

#include <QApplication>
#include <QBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QPointingDevice>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <QWheelEvent>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using namespace trion;
using namespace trion::shell;
using theme::ThemeManager;
using Clock = std::chrono::steady_clock;

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

double percentile(std::vector<double> v, double p)
{
    if (v.empty())
        return qQNaN();
    std::sort(v.begin(), v.end());
    return v[std::min(v.size() - 1, size_t(p * double(v.size() - 1) + 0.5))];
}

double distance(QPointF a, QPointF b)
{
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

bool near(double a, double b)
{
    return std::abs(a - b) <= 1e-12 * std::max(std::abs(a), std::abs(b));
}

// Ngưỡng CPU chỉ kiểm ở bản tối ưu (kTimed) và khi đo được CPU luồng.
bool withinCpu(double v, double budget)
{
    return !test::kTimed || test::threadCpu().count() < 0 || v <= budget;
}

// Đích phím giả: ghi phím nhận được.
class Recorder final : public QWidget {
public:
    using QWidget::QWidget;
    QList<int> keys;

protected:
    void keyPressEvent(QKeyEvent* e) override { keys << e->key(); }
};

// Lần vẽ của vùng vẽ: số lần, lúc vẽ (ns của steady_clock).
struct Paints : QObject {
    QWidget* w = nullptr;
    int count = 0;
    std::vector<qint64> at;
    bool eventFilter(QObject* o, QEvent* e) override
    {
        if (o == w && e->type() == QEvent::Paint) {
            ++count;
            at.push_back(Clock::now().time_since_epoch().count());
        }
        return false;
    }
};

// QTimerEvent tới mọi đối tượng trong tiến trình, trừ timer đo của test.
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

// Việc nạp giả thay DOC-016: công bố một lô mỗi `every` (tiến trình tăng dần), kiểm hủy mỗi 1 ms, xong khi test mở
// cổng.
struct FakeLoad {
    std::atomic<bool> open{false};
    std::atomic<int> batches{0};
    std::atomic<qint64> firstPublish{0}; // ns của steady_clock lúc công bố lô đầu
};

LoadGate::LoadFn fakeLoad(std::shared_ptr<FakeLoad> f, std::chrono::milliseconds every = 5ms)
{
    return [f, every](jobs::Progress& p, sched::CancelToken cancel,
                      const LoadGate::Published& published) -> Expected<void> {
        auto next = Clock::now() + every;
        while (!f->open.load()) {
            if (cancel.cancelled())
                return {};
            std::this_thread::sleep_for(1ms);
            if (Clock::now() < next)
                continue;
            next += every;
            const int n = f->batches.load();
            p.set(std::min(0.95, (n + 1) * 0.01));
            if (n == 0)
                f->firstPublish = Clock::now().time_since_epoch().count();
            f->batches = n + 1;
            published();
        }
        p.set(1);
        published();
        return {};
    };
}

} // namespace

class CanvasNavTest : public QObject {
    Q_OBJECT

    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<ThemeManager> tm_;
    std::unique_ptr<theme::IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<actions::Sysvars> sysvars_;
    std::unique_ptr<jobs::BackgroundJobs> jobs_;
    std::unique_ptr<QPointingDevice> touchpad_;
    std::unique_ptr<QWidget> window_;
    CanvasWidget* canvas_ = nullptr;
    ui::CommandInput* input_ = nullptr;
    Recorder* recorder_ = nullptr;
    TempView* view_ = nullptr;
    Navigation* nav_ = nullptr;
    LoadGate* gate_ = nullptr;
    quint64 ts_ = 1000;

    void setVar(const char* name, int v) { sysvars_->set(QString::fromLatin1(name), v); }

    void mouse(QEvent::Type type, QPointF pos, Qt::MouseButton button = Qt::NoButton, Qt::MouseButtons buttons = {})
    {
        ts_ += 16;
        QMouseEvent e(type, pos, canvas_->mapToGlobal(pos), button, buttons, Qt::NoModifier);
        e.setTimestamp(ts_);
        QApplication::sendEvent(canvas_, &e);
    }
    void press(QPointF p, Qt::MouseButton b) { mouse(QEvent::MouseButtonPress, p, b, b); }
    void drag(QPointF p, Qt::MouseButtons held) { mouse(QEvent::MouseMove, p, Qt::NoButton, held); }
    void release(QPointF p, Qt::MouseButton b) { mouse(QEvent::MouseButtonRelease, p, b, {}); }
    void wheel(QPointF pos, QPoint angle, QPoint pixel = {}, Qt::ScrollPhase phase = Qt::NoScrollPhase,
               Qt::KeyboardModifiers mods = {}, const QPointingDevice* device = nullptr)
    {
        QWheelEvent e(pos, canvas_->mapToGlobal(pos), pixel, angle, Qt::NoButton, mods, phase, false,
                      Qt::MouseEventNotSynthesized, device ? device : QPointingDevice::primaryPointingDevice());
        QApplication::sendEvent(canvas_, &e);
    }
    void native(Qt::NativeGestureType type, QPointF pos, double value = 0, QPointF delta = {})
    {
        QNativeGestureEvent e(type, touchpad_.get(), 2, pos, pos, canvas_->mapToGlobal(pos), value, delta);
        QApplication::sendEvent(canvas_, &e);
    }
    // Như phím thật: ShortcutOverride rồi KeyPress (nếu không ai giành).
    void key(QWidget* to, Qt::Key k, const QString& text = {})
    {
        QKeyEvent so(QEvent::ShortcutOverride, k, Qt::NoModifier, text);
        so.ignore();
        QApplication::sendEvent(to, &so);
        if (so.isAccepted())
            return;
        QKeyEvent e(QEvent::KeyPress, k, Qt::NoModifier, text);
        QApplication::sendEvent(to, &e);
    }
    void leave()
    {
        QEvent e(QEvent::Leave);
        QApplication::sendEvent(canvas_, &e);
    }
    void focus(QWidget* w)
    {
        window_->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(window_.get()));
        w->setFocus(Qt::OtherFocusReason);
        QVERIFY(spinUntil([&] { return w->hasFocus(); }));
    }
    ui::CursorTip* shownTip() const
    {
        for (ui::CursorTip* t : canvas_->findChildren<ui::CursorTip*>())
            if (t->isVisible())
                return t;
        return nullptr;
    }
    double scaleAfterWheel(QPoint angle)
    {
        view_->setView({0, 0}, 1);
        wheel(QPointF(200, 150), angle);
        return view_->scale();
    }

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
        jobs_ = std::make_unique<jobs::BackgroundJobs>();
        CanvasWidget::initSysvars(*sysvars_);
        QCOMPARE(sysvars_->get(u"ZOOMFACTOR"_s), 60);
        QCOMPARE(sysvars_->get(u"MBUTTONPAN"_s), 1);
        QCOMPARE(sysvars_->get(u"UNDOCTL"_s), 53);
        touchpad_ = std::make_unique<QPointingDevice>(
            u"touchpad"_s, 4242, QInputDevice::DeviceType::TouchPad, QPointingDevice::PointerType::Finger,
            QInputDevice::Capability::Position | QInputDevice::Capability::Scroll |
                QInputDevice::Capability::PixelScroll,
            5, 0);

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
        view_ = new TempView(*canvas_, canvas_);
        nav_ = new Navigation(*canvas_, *sysvars_, *view_, canvas_);
        gate_ = new LoadGate(*jobs_, *canvas_, canvas_);
        window_->resize(800, 600);
        window_->show();
        QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
        focus(canvas_);
    }

    void cleanupTestCase()
    {
        window_.reset(); // vùng vẽ cùng Navigation, TempView, LoadGate (hủy việc nạp còn chạy)
        sched::waitIdle();
        QCoreApplication::processEvents();
        jobs_.reset();
        touchpad_.reset();
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

    // Trạng thái đầu mỗi test: chờ, sysvar mặc định, view gốc, không chỗ nối, chuột ngoài vùng vẽ, không có việc nạp.
    void init()
    {
        canvas_->setScenePainter({});
        canvas_->cancelSelection();
        canvas_->setCursorState(CursorState::Idle);
        setVar("ZOOMFACTOR", 60);
        setVar("ZOOMWHEEL", 0);
        setVar("MBUTTONPAN", 1);
        setVar("UNDOCTL", 53);
        nav_->flush();
        view_->setExtents({});
        view_->setView({0, 0}, 1);
        canvas_->setKeySink(recorder_);
        recorder_->keys.clear();
        gate_->setSink({});
        gate_->setWrites({});
        leave();
        QVERIFY(!gate_->loading());
    }

    // Tiêu chí 2: 100 nấc lăn vào rồi ra liên tiếp (tọa độ ≈ 10³; ≈ 10⁹), 200 nấc sâu (tỉ lệ tới ×8·10⁷): điểm bản vẽ
    // dưới con trỏ lệch ≤ 0,5 px sau từng nấc; quay về đúng tỉ lệ đầu.
    void wheelAnchor()
    {
        struct Case {
            QPointF center;
            double scale;
            int notches; // số nấc vào, rồi cùng số nấc ra
        };
        const QPointF pos(317.25, 211.75);
        for (const Case& c :
             {Case{{1234.5, -678.25}, 1.0, 50}, Case{{1e9, -1e9}, 0.01, 50}, Case{{1234.5, -678.25}, 1.0, 100}}) {
            view_->setView(c.center, c.scale);
            const QPointF w = view_->toWorld(pos);
            double worst = 0;
            for (int i = 0; i < 2 * c.notches; ++i) {
                wheel(pos, {0, i < c.notches ? 120 : -120});
                worst = std::max(worst, distance(view_->toScreen(w), pos));
            }
            QVERIFY2(worst <= 0.5, qPrintable(u"lệch %1 px"_s.arg(worst))); // i18n-ok: thông báo test
            QVERIFY2(std::abs(view_->scale() / c.scale - 1) < 1e-9, qPrintable(QString::number(view_->scale())));
            qInfo("wheelAnchor: tâm (%g, %g), %d nấc vào + %d ra: lệch lớn nhất %.3g px", c.center.x(), c.center.y(),
                  c.notches, c.notches, worst);
        }
        // Tọa độ ≈ 10⁹, zoom sâu (10⁴ px/đơn vị): pan 1 px dời tâm 10⁻⁴ (10⁻¹³ tương đối) vẫn có tác dụng.
        view_->setView({1e9, 1e9}, 1e4);
        const QPointF w = view_->toWorld(pos);
        press(pos, Qt::MiddleButton);
        drag(pos + QPointF(1, 0), Qt::MiddleButton);
        release(pos + QPointF(1, 0), Qt::MiddleButton);
        QVERIFY2(distance(view_->toScreen(w), pos + QPointF(1, 0)) <= 0.01,
                 qPrintable(QString::number(view_->center().x(), 'f', 6)));
    }

    // Chỗ nối vẽ cảnh nhận phép biến đổi: dấu tại điểm bản vẽ (vẽ theo transform()) ở đúng dưới con trỏ sau lăn, pan.
    void sceneTransform()
    {
        const QPointF world(250.0, 125.0);
        int scenes = 0;
        canvas_->setScenePainter([&](QPainter& p, const QRegion&, const CanvasWidget::Highlight&) {
            ++scenes;
            p.setTransform(view_->transform());
            const QPointF s = p.transform().map(world); // dấu 5 × 5 px, cỡ không theo zoom
            p.resetTransform();
            p.fillRect(QRectF(s - QPointF(2.5, 2.5), QSizeF(5, 5)), QColor(255, 0, 0));
        });
        const auto marker = [&]() -> QPointF { // tâm các điểm ảnh đỏ trong ảnh vùng vẽ
            const QImage img = canvas_->grab().toImage().convertToFormat(QImage::Format_RGB32);
            double x = 0, y = 0;
            int n = 0;
            for (int j = 0; j < img.height(); ++j)
                for (int i = 0; i < img.width(); ++i)
                    if (img.pixel(i, j) == qRgb(255, 0, 0)) {
                        x += i + 0.5;
                        y += j + 0.5;
                        ++n;
                    }
            return n ? QPointF(x / n, y / n) : QPointF(-1, -1);
        };
        const QPointF pos = view_->toScreen(world);
        QVERIFY(distance(marker(), pos) <= 0.5);
        for (int i = 0; i < 5; ++i)
            wheel(pos, {0, 120});
        QVERIFY(near(view_->scale(), std::pow(1.2, 5)));
        QVERIFY2(distance(marker(), pos) <= 1, qPrintable(u"%1,%2"_s.arg(marker().x()).arg(marker().y())));
        press(pos, Qt::MiddleButton);
        drag(pos + QPointF(40, 30), Qt::MiddleButton);
        release(pos + QPointF(40, 30), Qt::MiddleButton);
        leave(); // không vẽ chữ thập lên dấu
        QVERIFY2(distance(marker(), pos + QPointF(40, 30)) <= 1,
                 qPrintable(u"%1,%2"_s.arg(marker().x()).arg(marker().y())));
        QVERIFY(scenes > 0);
    }

    // Tốc độ: một nấc nhân 1 + ZOOMFACTOR/300 (3 → 1,01; 60 → 1,2; 100 → 4/3), nấc lẻ theo lũy thừa; ZOOMFACTOR ngoài
    // miền kẹp vào 3–100.
    void wheelSpeed()
    {
        struct Case {
            int factor;
            int delta;
            double want;
        };
        for (const Case& c :
             {Case{60, 120, 1.2}, Case{3, 120, 1.01}, Case{100, 120, 4.0 / 3}, Case{60, 60, std::sqrt(1.2)},
              Case{60, -240, 1 / 1.44}, Case{0, 120, 1.01}, Case{500, 120, 4.0 / 3}}) {
            setVar("ZOOMFACTOR", c.factor);
            const double got = scaleAfterWheel({0, c.delta});
            QVERIFY2(near(got, c.want), qPrintable(u"ZOOMFACTOR %1, %2: %3"_s.arg(c.factor).arg(c.delta).arg(got)));
        }
    }

    // ZOOMWHEEL 1: lăn tới thu nhỏ; cuộn ngang của chuột không zoom.
    void wheelReverse()
    {
        QVERIFY(near(scaleAfterWheel({0, 120}), 1.2));
        setVar("ZOOMWHEEL", 1);
        QVERIFY(near(scaleAfterWheel({0, 120}), 1 / 1.2));
        QVERIFY(near(scaleAfterWheel({0, -120}), 1.2));
        QCOMPARE(scaleAfterWheel({120, 0}), 1.0);
    }

    // Giữ chuột giữa pan (bàn tay nắm, trạng thái Pan), nhả trả trạng thái của lệnh; khung đang kéo bị bỏ; MBUTTONPAN
    // 0.
    void middlePan()
    {
        const QPointF a(100, 100), w = view_->toWorld(a);
        press(a, Qt::MiddleButton);
        QCOMPARE(canvas_->cursorState(), CursorState::Pan);
        QCOMPARE(canvas_->cursor().shape(), Qt::ClosedHandCursor);
        QVERIFY(nav_->dragging());
        drag(QPointF(115, 95), Qt::MiddleButton);
        drag(QPointF(130, 80), Qt::MiddleButton);
        QVERIFY(distance(view_->toScreen(w), QPointF(130, 80)) < 1e-9);
        release(QPointF(130, 80), Qt::MiddleButton);
        QCOMPARE(canvas_->cursorState(), CursorState::Idle);
        QCOMPARE(canvas_->cursor().shape(), Qt::BlankCursor);
        QVERIFY(!nav_->dragging());

        // Lệnh đang hỏi điểm: pan trong suốt rồi trả Chọn điểm.
        canvas_->setCursorState(CursorState::PickPoint);
        press(a, Qt::MiddleButton);
        QCOMPARE(canvas_->cursorState(), CursorState::Pan);
        drag(QPointF(90, 90), Qt::MiddleButton);
        release(QPointF(90, 90), Qt::MiddleButton);
        QCOMPARE(canvas_->cursorState(), CursorState::PickPoint);

        // PAN thời gian thực đang chạy: chuột giữa cũng nắm, nhả về bàn tay mở.
        canvas_->setCursorState(CursorState::Pan);
        QCOMPARE(canvas_->cursor().shape(), Qt::OpenHandCursor);
        press(a, Qt::MiddleButton);
        QCOMPARE(canvas_->cursor().shape(), Qt::ClosedHandCursor);
        release(a, Qt::MiddleButton);
        QCOMPARE(canvas_->cursorState(), CursorState::Pan);
        QCOMPARE(canvas_->cursor().shape(), Qt::OpenHandCursor);
        canvas_->setCursorState(CursorState::Idle);

        // Khung chọn đang kéo bị bỏ (như UIA-109), pan vẫn chạy.
        drag(QPointF(50, 50), {});
        press(QPointF(50, 50), Qt::LeftButton);
        drag(QPointF(160, 120), Qt::LeftButton);
        QVERIFY(canvas_->selecting());
        QSignalSpy boxes(canvas_, &CanvasWidget::boxSelected);
        const QPointF w2 = view_->toWorld(QPointF(160, 120));
        press(QPointF(160, 120), Qt::MiddleButton);
        QVERIFY(!canvas_->selecting());
        drag(QPointF(170, 125), Qt::LeftButton | Qt::MiddleButton);
        release(QPointF(170, 125), Qt::MiddleButton);
        release(QPointF(170, 125), Qt::LeftButton);
        QCOMPARE(boxes.size(), 0);
        QVERIFY(distance(view_->toScreen(w2), QPointF(170, 125)) < 1e-9);

        // MBUTTONPAN 0: nút giữa không pan, trạng thái không đổi.
        setVar("MBUTTONPAN", 0);
        const QPointF c = view_->center();
        press(a, Qt::MiddleButton);
        QCOMPARE(canvas_->cursorState(), CursorState::Idle);
        drag(QPointF(150, 150), Qt::MiddleButton);
        release(QPointF(150, 150), Qt::MiddleButton);
        QCOMPARE(view_->center(), c);
    }

    // Nhấp đúp chuột giữa: zoom toàn bộ (khít extents, hở 6%); kéo tiếp sau cú nhấp đúp không pan; MBUTTONPAN 0: không.
    void middleDoubleClick()
    {
        view_->setExtents(QRectF(0, 0, 1000, 500));
        const QPointF p(300, 200);
        const auto doubleClick = [&] {
            press(p, Qt::MiddleButton);
            release(p, Qt::MiddleButton);
            press(p, Qt::MiddleButton); // Qt 6: cú nhấn thứ hai rồi DblClick
            mouse(QEvent::MouseButtonDblClick, p, Qt::MiddleButton, Qt::MiddleButton);
            drag(p + QPointF(30, 30), Qt::MiddleButton);
            release(p + QPointF(30, 30), Qt::MiddleButton);
        };
        doubleClick();
        QCOMPARE(view_->center(), QPointF(500, 250));
        QVERIFY(near(view_->scale(), std::min(canvas_->width() / 1060.0, canvas_->height() / 530.0)));
        QCOMPARE(canvas_->cursorState(), CursorState::Idle);

        // Bản vẽ chỉ một đoạn ngang (cao 0): khít theo chiều rộng.
        view_->setExtents(QRectF(0, 0, 1000, 0));
        doubleClick();
        QCOMPARE(view_->center(), QPointF(500, 0));
        QVERIFY(near(view_->scale(), canvas_->width() / 1060.0));

        // Không có extents (bản vẽ rỗng): không đổi.
        view_->setExtents({});
        view_->setView({7, 8}, 2);
        doubleClick();
        QCOMPARE(view_->center(), QPointF(7, 8));

        view_->setExtents(QRectF(0, 0, 1000, 500));
        setVar("MBUTTONPAN", 0);
        doubleClick();
        QCOMPARE(view_->center(), QPointF(7, 8));
    }

    // PAN thời gian thực (trạng thái Pan): kéo trái pan. ZOOM thời gian thực (trạng thái Zoom): kéo lên nửa chiều cao
    // vùng vẽ = ×2 quanh giữa vùng vẽ; kéo xuống thu nhỏ.
    void realtimePanZoom()
    {
        canvas_->setCursorState(CursorState::Pan);
        const QPointF a(200, 200), w = view_->toWorld(a);
        press(a, Qt::LeftButton);
        QCOMPARE(canvas_->cursor().shape(), Qt::ClosedHandCursor);
        drag(QPointF(260, 230), Qt::LeftButton);
        release(QPointF(260, 230), Qt::LeftButton);
        QVERIFY(distance(view_->toScreen(w), QPointF(260, 230)) < 1e-9);
        QCOMPARE(canvas_->cursorState(), CursorState::Pan);
        QCOMPARE(canvas_->cursor().shape(), Qt::OpenHandCursor);

        canvas_->setCursorState(CursorState::Zoom);
        const QPointF mid(canvas_->width() / 2.0, canvas_->height() / 2.0), wm = view_->toWorld(mid);
        const double s0 = view_->scale();
        const QPointF b(400, mid.y());
        press(b, Qt::LeftButton);
        drag(b - QPointF(0, mid.y() / 4), Qt::LeftButton);
        drag(b - QPointF(0, mid.y()), Qt::LeftButton);
        QVERIFY(near(view_->scale(), 2 * s0));
        QVERIFY(distance(view_->toScreen(wm), mid) < 1e-9);
        drag(b + QPointF(30, mid.y()), Qt::LeftButton); // ngang không đổi, xuống thu nhỏ: hết kéo là ×0,5 so với đầu
        release(b + QPointF(30, mid.y()), Qt::LeftButton);
        QVERIFY(near(view_->scale(), s0 / 2));
        QCOMPARE(canvas_->cursorState(), CursorState::Zoom);
    }

    // Touchpad: chụm (native Zoom) quanh điểm giữa hai ngón, Pan native; kéo hai ngón (lăn có pixelDelta: có pha, hoặc
    // thiết bị TouchPad) pan; chuột lăn của macOS (pixelDelta, không pha, thiết bị chuột) zoom; Ctrl + lăn zoom.
    void touchpad()
    {
        const QPointF pos(320, 240), w = view_->toWorld(pos);
        native(Qt::BeginNativeGesture, pos);
        native(Qt::ZoomNativeGesture, pos, 0.25);
        QVERIFY(near(view_->scale(), 1.25));
        native(Qt::ZoomNativeGesture, pos, -0.2);
        QVERIFY(near(view_->scale(), 1.0));
        QVERIFY(distance(view_->toScreen(w), pos) < 1e-9);
        native(Qt::EndNativeGesture, pos);
        native(Qt::PanNativeGesture, pos, 0, QPointF(12, -7));
        QVERIFY(distance(view_->toScreen(w), pos + QPointF(12, -7)) < 1e-9);

        view_->setView({0, 0}, 1);
        const QPointF w0 = view_->toWorld(pos);
        wheel(pos, {0, 0}, {}, Qt::ScrollBegin, {}, touchpad_.get());
        wheel(pos, {20, -40}, {10, -20}, Qt::ScrollUpdate, {}, touchpad_.get());
        wheel(pos, {0, 0}, {}, Qt::ScrollEnd, {}, touchpad_.get());
        QCOMPARE(view_->scale(), 1.0);
        QVERIFY(distance(view_->toScreen(w0), pos + QPointF(10, -20)) < 1e-9);
        // X11: touchpad không có pha, nhận ra nhờ loại thiết bị.
        wheel(pos, {10, 10}, {5, 5}, Qt::NoScrollPhase, {}, touchpad_.get());
        QVERIFY(distance(view_->toScreen(w0), pos + QPointF(15, -15)) < 1e-9);
        // macOS, chuột lăn thường: pixelDelta (20 px một dòng) nhưng không pha → zoom một nấc.
        wheel(pos, {0, 120}, {0, 20});
        QVERIFY(near(view_->scale(), 1.2));
        // Windows đổi chụm thành Ctrl + lăn; Ctrl + kéo hai ngón cũng zoom.
        wheel(pos, {0, 60}, {0, 30}, Qt::ScrollUpdate, Qt::ControlModifier, touchpad_.get());
        QVERIFY(near(view_->scale(), 1.2 * std::sqrt(1.2)));
    }

    // Bước view (INT-172): UNDOCTL bit 16 bật → mọi zoom, pan liên tiếp là một bước, phát khi chuỗi dừng (nút chuột
    // khác, phím, chuột rời vùng vẽ, flush); tắt → mỗi thao tác một bước; thao tác không đổi view không tạo bước.
    void viewSteps()
    {
        QSignalSpy steps(nav_, &Navigation::viewStep);
        const QPointF p(300, 200);
        const auto run = [&] { // 5 lần lăn, một lần kéo giữa, một lần chụm, một lần kéo hai ngón
            for (int i = 0; i < 5; ++i)
                wheel(p, {0, i % 2 ? -120 : 120});
            press(p, Qt::MiddleButton);
            for (int i = 1; i <= 5; ++i)
                drag(p + QPointF(i, i), Qt::MiddleButton);
            release(p + QPointF(5, 5), Qt::MiddleButton);
            native(Qt::BeginNativeGesture, p);
            native(Qt::ZoomNativeGesture, p, 0.1);
            native(Qt::ZoomNativeGesture, p, 0.1);
            native(Qt::EndNativeGesture, p);
            wheel(p, {}, {}, Qt::ScrollBegin, {}, touchpad_.get());
            wheel(p, {0, 20}, {0, 10}, Qt::ScrollUpdate, {}, touchpad_.get());
            wheel(p, {0, 20}, {0, 10}, Qt::ScrollUpdate, {}, touchpad_.get());
            wheel(p, {}, {}, Qt::ScrollEnd, {}, touchpad_.get());
        };
        run();
        QCOMPARE(steps.size(), 0); // gộp: chuỗi chưa dừng
        press(p, Qt::LeftButton);  // chọn: bước view trước
        QCOMPARE(steps.size(), 1);
        release(p, Qt::LeftButton);
        canvas_->cancelSelection();
        key(canvas_, Qt::Key_A, u"a"_s); // không có thay đổi mới: không thêm bước
        QCOMPARE(steps.size(), 1);
        wheel(p, {0, 120});
        key(canvas_, Qt::Key_A, u"a"_s);
        QCOMPARE(steps.size(), 2);
        wheel(p, {0, 120});
        leave();
        QCOMPARE(steps.size(), 3);
        wheel(p, {0, 120});
        nav_->flush();
        QCOMPARE(steps.size(), 4);

        setVar("UNDOCTL", 5); // bỏ bit 16
        steps.clear();
        run(); // 5 lần lăn + 1 kéo + 1 chụm + 1 kéo hai ngón
        QCOMPARE(steps.size(), 8);
        press(p, Qt::MiddleButton); // nhấp giữa không kéo: view không đổi
        release(p, Qt::MiddleButton);
        QCOMPARE(steps.size(), 8);
        canvas_->cancelSelection();
    }

    // Tiêu chí 3: trong lúc nạp (lô công bố mỗi 5 ms, vùng vẽ vẽ lại theo lô), 120 khung lăn + kéo chuột giữa: CPU
    // luồng UI mỗi khung ≤ 33 ms (≥ 30 fps). Lô đầu công bố → khung vẽ (phần widget của khung đầu). Việc vào NT-013
    // ("open").
    void loadNavigate()
    {
        auto f = std::make_shared<FakeLoad>();
        int scenes = 0;
        canvas_->setScenePainter([&](QPainter&, const QRegion&, const CanvasWidget::Highlight&) { ++scenes; });
        Paints paints;
        paints.w = canvas_;
        canvas_->installEventFilter(&paints);
        QSignalSpy published(gate_, &LoadGate::published), loading(gate_, &LoadGate::loadingChanged);
        jobs::State end = jobs::State::Running;
        const auto conn = connect(gate_, &LoadGate::finished, this, [&](jobs::State s) { end = s; });
        const jobs::JobId id = gate_->start(7, fakeLoad(f));
        QVERIFY(gate_->loading());
        QCOMPARE(loading.size(), 1);
        const auto info = jobs_->info(id);
        QVERIFY(info && info->kind == u"open"_s && info->owner == 7);

        // Khung đầu, phần widget: lần vẽ đầu sau lúc công bố lô đầu.
        QVERIFY(spinUntil(
            [&] { return f->firstPublish.load() != 0 && !paints.at.empty() && paints.at.back() >= f->firstPublish; }));
        const qint64 first = *std::ranges::find_if(paints.at, [&](qint64 t) { return t >= f->firstPublish; });
        const double firstFrame = ms(std::chrono::nanoseconds(first - f->firstPublish));
        QVERIFY2(firstFrame <= 2000, qPrintable(QString::number(firstFrame)));

        const QPointF p(400, 280);
        std::vector<double> cpu;
        press(p, Qt::MiddleButton);
        for (int i = 0; i < 120; ++i) {
            const int painted = paints.count;
            const auto start = test::threadCpu();
            if (i % 2)
                drag(p + QPointF(i, i / 2), Qt::MiddleButton);
            else
                wheel(p + QPointF(i, i / 2), {0, i % 4 ? -120 : 120});
            QVERIFY(spinUntil([&] { return paints.count > painted; }));
            cpu.push_back(ms(test::threadCpu() - start));
        }
        release(p + QPointF(120, 60), Qt::MiddleButton);
        QVERIFY(gate_->loading()); // cả 120 khung trong lúc nạp
        const auto now = jobs_->info(id);
        QVERIFY(now && now->state == jobs::State::Running && now->progress > 0); // thanh trạng thái có tiến trình
        const int batches = f->batches.load();
        QVERIFY(batches > 0 && published.size() > 0);
        const double p50 = percentile(cpu, 0.5), p99 = percentile(cpu, 0.99);
        qInfo("loadNavigate: khung đầu (phần widget) %.2f ms; 120 khung pan/zoom trong lúc nạp (%d lô, %lld lần vẽ "
              "theo lô): CPU luồng UI p50 %.3f ms, p99 %.3f ms",
              firstFrame, batches, qlonglong(published.size()), p50, p99);
        QVERIFY2(withinCpu(p99, 1000.0 / 30), qPrintable(QString::number(p99)));

        f->open = true;
        QVERIFY(spinUntil([&] { return !gate_->loading(); }));
        QCOMPARE(end, jobs::State::Done);
        QCOMPARE(loading.size(), 2);
        QCOMPARE(loading.last().at(0).toBool(), false);
        disconnect(conn);
        canvas_->removeEventFilter(&paints);
        QVERIFY(scenes > 0);
    }

    // Tiêu chí 3: hủy nạp (cổng, hoặc nút hủy của thanh trạng thái gọi BackgroundJobs::cancel) → finished(Cancelled)
    // ≤ 50 ms; lệnh chờ bị bỏ kèm thông báo (tiêu chí 4).
    void loadCancel()
    {
        QStringList ran;
        gate_->setSink([&](const QString& m) { ran << m; });
        QSignalSpy notes(gate_, &LoadGate::notice);
        jobs::State end = jobs::State::Running;
        int ends = 0;
        const auto conn = connect(gate_, &LoadGate::finished, this, [&](jobs::State s) {
            end = s;
            ++ends;
        });
        std::vector<double> latency;
        // Vòng chẵn: cancel() của cổng; vòng lẻ: BackgroundJobs::cancel (nút hủy của thanh trạng thái).
        for (int round = 0; round < 4; ++round) {
            auto f = std::make_shared<FakeLoad>();
            const jobs::JobId id = gate_->start(9, fakeLoad(f));
            QVERIFY(spinUntil([&] { return f->batches.load() >= 3; }));
            gate_->submit(u"_LINE"_s);
            QCOMPARE(gate_->held(), u"_LINE"_s);
            const int before = ends;
            const auto t0 = Clock::now();
            QVERIFY(round % 2 == 0 ? gate_->cancel() : jobs_->cancel(id));
            QVERIFY(spinUntil([&] { return ends > before; }, 5s));
            latency.push_back(ms(Clock::now() - t0));
            QCOMPARE(end, jobs::State::Cancelled);
            QVERIFY(!gate_->loading());
            QVERIFY(gate_->held().isEmpty());
            QVERIFY(notes.last().at(0).toString().contains(u"LINE"_s));
            QVERIFY(!f->open.load());
        }
        QVERIFY(ran.isEmpty());
        QVERIFY(!gate_->cancel()); // không có việc nạp

        // Mở tệp khác khi đang nạp: việc cũ bị hủy, lệnh chờ của nó bị bỏ; việc mới nạp tiếp.
        auto a = std::make_shared<FakeLoad>(), b = std::make_shared<FakeLoad>();
        const jobs::JobId first = gate_->start(21, fakeLoad(a));
        gate_->submit(u"_LINE"_s);
        const int before = ends;
        const jobs::JobId second = gate_->start(22, fakeLoad(b));
        QCOMPARE(ends, before + 1);
        QCOMPARE(end, jobs::State::Cancelled);
        QVERIFY(gate_->held().isEmpty());
        QVERIFY(gate_->loading());
        QCOMPARE(gate_->job(), second);
        QVERIFY(spinUntil([&] { return !jobs_->info(first); })); // việc cũ dừng thật
        b->open = true;
        QVERIFY(spinUntil([&] { return !gate_->loading(); }));
        QCOMPARE(end, jobs::State::Done);
        QVERIFY(ran.isEmpty());
        disconnect(conn);
        // Thời gian thật phụ thuộc tải của máy dùng chung: kiểm lần tốt nhất, ghi mọi lần.
        const double best = *std::ranges::min_element(latency);
        qInfo("loadCancel: hủy → kết thúc, 4 vòng (cổng, BackgroundJobs, cổng, BackgroundJobs): %.2f %.2f %.2f %.2f ms",
              latency[0], latency[1], latency[2], latency[3]);
        QVERIFY2(!test::kTimed || best <= 50, qPrintable(QString::number(best)));
    }

    // Tiêu chí 4: lệnh sửa khi đang nạp được giữ, báo chờ (CursorTip tại con trỏ, notice), lệnh xem/đo/sysvar/lệnh giao
    // diện đi thẳng, lệnh sửa sau thay lệnh trước; nạp xong thì chạy; chỗ nối phân loại setWrites.
    void editWaits()
    {
        QVERIFY(LoadGate::writesByDefault(u"LINE"_s));
        QVERIFY(LoadGate::writesByDefault(u"ERASE"_s));
        QVERIFY(LoadGate::writesByDefault(u"COUNT"_s)); // Viewer Pro: đếm trên bản vẽ chưa đủ sẽ sai
        QVERIFY(!LoadGate::writesByDefault(u"ZOOM"_s));
        QVERIFY(!LoadGate::writesByDefault(u"DIST"_s));
        QVERIFY(!LoadGate::writesByDefault(u"NEW"_s));
        QVERIFY(!LoadGate::writesByDefault(u"COMMANDLINE"_s)); // ngoài danh mục: lệnh giao diện

        auto f = std::make_shared<FakeLoad>();
        QStringList ran;
        gate_->setSink([&](const QString& m) { ran << m; });
        QSignalSpy notes(gate_, &LoadGate::notice), holds(gate_, &LoadGate::heldChanged);
        drag(QPointF(240, 160), {}); // chuột trong vùng vẽ: CursorTip cạnh con trỏ
        gate_->start(11, fakeLoad(f));
        QCOMPARE(notes.size(), 1); // "Đang nạp tệp lớn ở nền…" cho dòng lệnh
        gate_->submit(u"^C^C_LINE"_s);
        QVERIFY(ran.isEmpty());
        QCOMPARE(gate_->held(), u"^C^C_LINE"_s);
        QCOMPARE(holds.size(), 1);
        QVERIFY(notes.last().at(0).toString().contains(u"LINE"_s));
        ui::CursorTip* tip = shownTip();
        QVERIFY(tip);
        QVERIFY(tip->text().contains(u"LINE"_s));
        QCOMPARE(tip->accessibleName(), tip->text()); // trình đọc màn hình
        const QPoint tipAt = canvas_->mapFromGlobal(tip->pos());
        QVERIFY2(tipAt.x() >= 240 && tipAt.y() >= 160, qPrintable(u"%1,%2"_s.arg(tipAt.x()).arg(tipAt.y())));

        for (const QString& m : {u"'_ZOOM"_s, u"_PAN"_s, u"DIST"_s, u"'ORTHOMODE 1"_s, u"COMMANDLINE"_s, u"_NEW"_s}) {
            gate_->submit(m);
            QCOMPARE(ran.last(), m);
        }
        QCOMPARE(gate_->held(), u"^C^C_LINE"_s);
        gate_->submit(u"_.erase"_s);
        QCOMPARE(gate_->held(), u"_.erase"_s);
        QVERIFY(notes.last().at(0).toString().contains(u"ERASE"_s));
        const int passed = int(ran.size());

        f->open = true;
        QVERIFY(spinUntil([&] { return !gate_->loading(); }));
        QCOMPARE(ran.size(), passed + 1);
        QCOMPARE(ran.last(), u"_.erase"_s);
        QVERIFY(gate_->held().isEmpty());
        QVERIFY(holds.last().at(0).toString().isEmpty());
        QVERIFY(!tip->isVisible());
        gate_->submit(u"LINE"_s); // nạp xong: đi thẳng
        QCOMPARE(ran.last(), u"LINE"_s);

        // Chỗ nối INT-001: phân loại do lõi.
        auto g = std::make_shared<FakeLoad>();
        gate_->setWrites([](const QString& c) { return c == u"DIST"_s; });
        gate_->start(12, fakeLoad(g));
        gate_->submit(u"LINE"_s);
        QCOMPARE(ran.last(), u"LINE"_s);
        gate_->submit(u"DIST"_s);
        QCOMPARE(gate_->held(), u"DIST"_s);
        g->open = true;
        QVERIFY(spinUntil([&] { return !gate_->loading(); }));
        QCOMPARE(ran.last(), u"DIST"_s);
    }

    // Tiêu chí 4: Esc bỏ lệnh chờ, ở vùng vẽ hay ô dòng lệnh; Esc đó không tới dòng lệnh. Không có lệnh chờ: Esc như
    // thường.
    void escCancels()
    {
        auto f = std::make_shared<FakeLoad>();
        QStringList ran;
        gate_->setSink([&](const QString& m) { ran << m; });
        QSignalSpy notes(gate_, &LoadGate::notice);
        gate_->start(13, fakeLoad(f));
        focus(canvas_);
        gate_->submit(u"_LINE"_s);
        key(canvas_, Qt::Key_Escape, u"\x1b"_s);
        QVERIFY(gate_->held().isEmpty());
        QVERIFY(notes.last().at(0).toString().contains(u"LINE"_s));
        QVERIFY(recorder_->keys.isEmpty());
        QVERIFY(!shownTip());

        gate_->submit(u"_MOVE"_s);
        focus(input_);
        key(input_, Qt::Key_Escape, u"\x1b"_s);
        QVERIFY(gate_->held().isEmpty());
        QVERIFY(notes.last().at(0).toString().contains(u"MOVE"_s));

        focus(canvas_);
        key(canvas_, Qt::Key_Escape, u"\x1b"_s); // không có lệnh chờ: tới đích như thường
        QCOMPARE(recorder_->keys, QList<int>{Qt::Key_Escape});

        f->open = true;
        QVERIFY(spinUntil([&] { return !gate_->loading(); }));
        QVERIFY(ran.isEmpty());
    }

    // DoD-U, DoD-H 6: sau điều hướng và một lần nạp, rảnh: 0 lần vẽ, 0 QTimerEvent (không timer định kỳ, không
    // polling).
    void idle()
    {
        auto f = std::make_shared<FakeLoad>();
        gate_->start(15, fakeLoad(f));
        gate_->submit(u"LINE"_s); // CursorTip (ẩn sau 2 s)
        const QPointF p(300, 200);
        drag(p, {});
        wheel(p, {0, 120});
        press(p, Qt::MiddleButton);
        drag(p + QPointF(20, 10), Qt::MiddleButton);
        release(p + QPointF(20, 10), Qt::MiddleButton);
        f->open = true;
        QVERIFY(spinUntil([&] { return !gate_->loading(); }));
        spinFor(2500ms); // CursorTip hết hạn, lô cuối của NT-013
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
        end.start(3s);
        loop.exec();
        qApp->removeEventFilter(&timers);
        canvas_->removeEventFilter(&paints);
        QCOMPARE(paints.count, 0);
        QVERIFY2(timers.seen.isEmpty(), qPrintable(timers.seen.join(u", "_s)));
    }
};

QTEST_MAIN(CanvasNavTest)
#include "canvas_nav_test.moc"
