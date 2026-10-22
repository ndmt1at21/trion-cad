// UI0-009: thông điệp (Messages.h). Mỗi tiêu chí nghiệm thu một nhóm hàm test; thời gian rút ngắn qua setTimeout(),
// trừ realTime chạy đúng 5 s/2 s. Vùng vẽ, dòng lệnh, sự kiện ứng dụng (UIA-015) chưa có: QMainWindow mẫu với vùng vẽ
// giả đếm lần vẽ và QLineEdit giả làm dòng lệnh.
//   1  toastAutoHide, toastHoverPause, toastLimit: toast thường tự ẩn sau timeout (không sớm hơn), rê chuột hoặc focus
//      bàn phím thì dừng đếm, ra thì đếm tiếp phần còn lại; Esc đóng toast đang có focus, focus về dòng lệnh; toast lỗi
//      ở lại; tối đa 3 cùng lúc (toast mới đẩy toast cũ nhất), chồng ở góc dưới phải trên thanh trạng thái; mọi toast
//      có mục trong NotificationCenter (chưa đọc; bấm ×, hành động, Esc thì đã đọc).
//   2  cursorTip: ẩn sau timeout; lệch đúng (16, 18) px khỏi điểm, lật ở mép vùng vẽ vẫn cách ≥ 16 px; là cửa sổ riêng;
//      hiện/ẩn không làm vùng vẽ giả vẽ lại (0 lần paintEvent).
//   3  banner: ở lại (không timer), nút gọi hành động đăng ký, × (chuột, bàn phím; focus về chỗ cũ) phát closed; tối đa
//      2 hiện, banner cuối ghi số còn chờ; thay cùng id giữ chỗ; remove không phát closed.
//   4  accessibility (bộ bắt sự kiện QAccessible): toast lỗi vai trò AlertMessage, sự kiện Alert, thông báo Assertive;
//      toast khác StatusBar, Polite, không Alert; banner (cả banner hết chờ), chú thích, FieldMessage Polite; mỗi mức
//      một biểu tượng riêng.
//   5  frameAndFocus: hiện toast, banner, chú thích không đổi focus (dòng lệnh giả) và cửa sổ kích hoạt; lần vẽ đầu
//      ≤ 1 khung sau lời gọi (vòng tốt nhất của 3, kiểm ở release).
//   6  idle: rảnh 0 timer đang chạy; timer chỉ chạy khi toast thường, chú thích đang hiện. realTime: 5 s và 2 s thật.
//   Còn lại: followJobs (NT-013/UI0-112), centerModel, emptyAndField; DoD-U: baseline (ảnh mọi loại Tối/Sáng
//   × 100/150/200% so baseline theo nền tảng; ghi lại: TRION_UPDATE_BASELINE=<thư mục>), pseudoNoTruncation (vi-XL).
#include "app/widgets/message/Messages.h"

#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/progress/ProgressPresenter.h"
#include "core/base/sched/Scheduler.h"
#include "tests/timing.h"

#include <QAccessible>
#include <QApplication>
#include <QBoxLayout>
#include <QLineEdit>
#include <QMainWindow>
#include <QPainter>
#include <QScreen>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <atomic>
#include <memory>
#include <thread>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using namespace trion::theme;
using namespace trion::ui;
using trion::Expected;
using trion::test::kTimed;
namespace i18n = trion::i18n;
namespace jobs = trion::jobs;
namespace sched = trion::sched;
namespace settings = trion::settings;
using Clock = std::chrono::steady_clock;

namespace {

#if defined(Q_OS_WIN)
constexpr auto kPlatform = "windows";
#elif defined(Q_OS_MACOS)
constexpr auto kPlatform = "macos";
#else
constexpr auto kPlatform = "linux";
#endif
constexpr int kNoise = 8; // baseline: lệch ≤ 8/255 mỗi kênh bỏ qua, điểm lệch hơn được tối đa 0,1% ảnh
constexpr int kSheetW = 460; // bề rộng tấm ảnh baseline
// PreciseTimer chính xác tới mili giây: trên Windows có thể đến sớm ≤ 1 ms so với steady_clock.
constexpr std::chrono::milliseconds kTimerSlack{1};

// Chữ mẫu theo văn phong 07.1 §12.
enum Text {
    Exported,
    FontErrors,
    Tip,
    DiskLow,
    NoIntersection,
    Finish,
    ReadOnly,
    FontsSwapped,
    Recovered,
    MissingRefs,
    OpenFolder,
    View,
    SaveAs,
    ShowList,
    OpenOriginal,
    FindRefs,
    Ignore,
    EmptyTitle,
    EmptyWhy,
    EmptyAction,
    EmptyMore,
    ScaleError,
    DotHint,
    TextCount
};
constexpr const char* kText[TextCount] = {
    "Đã xuất 24 tờ PDF",                                 // i18n-ok: dữ liệu thử
    "Chuyển font xong 48/50 tệp: 2 lỗi",                 // i18n-ok: dữ liệu thử
    "Mẹo: gõ “đo” vào dòng lệnh để thấy các lệnh đo.",   // i18n-ok: dữ liệu thử
    "Ổ đĩa còn 2 GB, lưu tự động có thể chậm hơn.",      // i18n-ok: dữ liệu thử
    "Không có giao điểm với đường biên",                 // i18n-ok: dữ liệu thử
    "Nhấn Enter để kết thúc",                            // i18n-ok: dữ liệu thử
    "Bản vẽ chỉ đọc",                                    // i18n-ok: dữ liệu thử
    "Đã thay 3 font thiếu bằng font tương đương",        // i18n-ok: dữ liệu thử
    "Bản vẽ được phục hồi từ bản lưu tự động lúc 14:05", // i18n-ok: dữ liệu thử
    "Thiếu 2 tham chiếu ngoài",                          // i18n-ok: dữ liệu thử
    "Mở thư mục",                                        // i18n-ok: dữ liệu thử
    "Xem",                                               // i18n-ok: dữ liệu thử
    "Lưu thành…",                                        // i18n-ok: dữ liệu thử
    "Xem danh sách",                                     // i18n-ok: dữ liệu thử
    "Mở bản gốc",                                        // i18n-ok: dữ liệu thử
    "Tìm lại",                                           // i18n-ok: dữ liệu thử
    "Bỏ qua",                                            // i18n-ok: dữ liệu thử
    "Bản vẽ chưa có block nào",                          // i18n-ok: dữ liệu thử
    "Tạo block bằng BLOCK hoặc chèn từ Thư viện TCVN.",  // i18n-ok: dữ liệu thử
    "Mở Thư viện TCVN",                                  // i18n-ok: dữ liệu thử
    "Xem video 2 phút",                                  // i18n-ok: dữ liệu thử
    "Tỉ lệ phải lớn hơn 0. Gõ một số dương, ví dụ 1.5.", // i18n-ok: dữ liệu thử
    "Trong bản vẽ, dấu thập phân luôn là dấu chấm.",     // i18n-ok: dữ liệu thử
};

// Chữ mẫu qua bộ dịch đang cài: ở vi-XL được kéo dài như chuỗi thật (biến, không phải hằng: lupdate không thu).
QString text(Text t)
{
    const char* source = kText[t];
    return QCoreApplication::translate("MessageTest", source);
}

bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 20s)
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

void spinFor(std::chrono::milliseconds d)
{
    QEventLoop loop;
    QTimer::singleShot(d, &loop, &QEventLoop::quit);
    loop.exec();
}

double ms(Clock::duration d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

Clock::duration frameTime()
{
    const QScreen* s = QGuiApplication::primaryScreen();
    return std::chrono::nanoseconds(qint64(1e9 / (s && s->refreshRate() > 1 ? s->refreshRate() : 60)));
}

// Vùng vẽ giả: đếm lần vẽ (RND chưa có; tiêu chí 2: 0 khung khi hiện/ẩn chú thích).
class Canvas final : public QWidget {
public:
    using QWidget::QWidget;
    int paints = 0;

protected:
    void paintEvent(QPaintEvent*) override { ++paints; }
};

// Ghi lần vẽ đầu và lúc ẩn của một widget.
class Watcher final : public QObject {
public:
    explicit Watcher(QWidget* w) { w->installEventFilter(this); }
    Clock::time_point firstPaintAt{}, hiddenAt{};
    int paints = 0;

protected:
    bool eventFilter(QObject*, QEvent* e) override
    {
        if (e->type() == QEvent::Paint && !paints++)
            firstPaintAt = Clock::now();
        if (e->type() == QEvent::Hide && hiddenAt == Clock::time_point{})
            hiddenAt = Clock::now();
        return false;
    }
};

// Bộ bắt sự kiện tiếp cận (tiêu chí 4).
struct A11yEvent {
    QAccessible::Event type;
    QObject* object;
    QString message; // Announcement
    QAccessible::AnnouncementPoliteness politeness = QAccessible::AnnouncementPoliteness::Polite;
};
std::vector<A11yEvent> g_a11y;

void recordA11y(QAccessibleEvent* e)
{
    A11yEvent r{e->type(), e->object(), {}};
    if (e->type() == QAccessible::Announcement) {
        const auto* say = static_cast<QAccessibleAnnouncementEvent*>(e);
        r.message = say->message();
        r.politeness = say->politeness();
    }
    g_a11y.push_back(r);
}

int count(QObject* object, QAccessible::Event type)
{
    return int(std::count_if(g_a11y.begin(), g_a11y.end(),
                             [&](const A11yEvent& e) { return e.object == object && e.type == type; }));
}

const A11yEvent* announcement(QObject* object)
{
    for (const A11yEvent& e : g_a11y)
        if (e.object == object && e.type == QAccessible::Announcement)
            return &e;
    return nullptr;
}

Button* closeButton(QWidget* row)
{
    for (Button* b : row->findChildren<Button*>())
        if (b->kind() == Button::Kind::Icon)
            return b;
    return nullptr;
}

Button* buttonWithText(QWidget* row, const QString& label)
{
    for (Button* b : row->findChildren<Button*>())
        if (b->text() == label && b->isVisible())
            return b;
    return nullptr;
}

QString truncations(QWidget* root)
{
    QStringList out;
    for (const i18n::Truncation& t : i18n::findTruncations(root))
        out << t.toString();
    return out.join(u"; "_s);
}

} // namespace

class MessageTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<UiExecutor> executor_;
    std::unique_ptr<ThemeManager> tm_;
    std::unique_ptr<IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<QMainWindow> window_; // QMainWindow mẫu: vùng vẽ giả + dòng lệnh giả
    Canvas* canvas_ = nullptr;
    QLineEdit* cmd_ = nullptr;
    std::unique_ptr<NotificationCenter> center_;
    std::unique_ptr<ToastHost> toasts_;
    QPointer<CanvasBanner> banners_;
    QPointer<CursorTip> tip_;

    int visibleToasts() const
    {
        const auto all = window_->findChildren<Toast*>();
        return int(std::count_if(all.begin(), all.end(), [](const Toast* t) { return t->isVisible(); }));
    }

    int activeTimers() const // mọi QTimer trong cửa sổ, kể cả của chú thích (con của vùng vẽ)
    {
        const auto all = window_->findChildren<QTimer*>();
        return int(std::count_if(all.begin(), all.end(), [](const QTimer* t) { return t->isActive(); }));
    }

    void focusCommandLine()
    {
        window_->activateWindow();
        cmd_->setFocus(Qt::OtherFocusReason);
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == cmd_; }));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        QVERIFY(loadFonts()); // chữ Plex nhúng: ảnh không phụ thuộc font của máy
        executor_ = std::make_unique<UiExecutor>();
        tm_ = std::make_unique<ThemeManager>(*store_);
        icons_ = std::make_unique<IconProvider>();
        translator_ = std::make_unique<i18n::Translator>();
        window_ = std::make_unique<QMainWindow>();
        auto* central = new QWidget;
        auto* col = new QVBoxLayout(central);
        col->setContentsMargins({});
        col->setSpacing(0);
        canvas_ = new Canvas;
        cmd_ = new QLineEdit;
        col->addWidget(canvas_, 1);
        col->addWidget(cmd_);
        window_->setCentralWidget(central);
        window_->resize(1366, 768);
        window_->show();
        QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
        focusCommandLine();
    }

    void init()
    {
        tm_->setTheme(Theme::Dark);
        tm_->setDensity(Density::Compact);
        center_ = std::make_unique<NotificationCenter>();
        toasts_ = std::make_unique<ToastHost>(window_.get(), *center_);
        banners_ = new CanvasBanner(canvas_);
        tip_ = new CursorTip(canvas_);
    }

    void cleanup()
    {
        toasts_.reset();
        center_.reset();
        delete banners_;
        delete tip_;
        QTest::mouseMove(window_->windowHandle(), QPoint(1, 1)); // chuột ra khỏi chỗ toast hiện
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
        if (translator_->language() != u"vi"_s)
            QVERIFY(translator_->setLanguage(u"vi"_s));
    }

    void cleanupTestCase()
    {
        window_.reset();
        translator_.reset();
        icons_.reset();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        sched::waitIdle();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    // Tiêu chí 1: tự ẩn sau timeout (không sớm), lỗi ở lại; mọi toast vào center; × đánh dấu đã đọc.
    void toastAutoHide()
    {
        const auto timeout = 150ms;
        toasts_->setTimeout(timeout);
        const auto posted = Clock::now();
        QPointer<Toast> ok = toasts_->post(Severity::Ok, text(Exported));
        Watcher okWatch(ok);
        QPointer<Toast> err = toasts_->post(Severity::Error, text(FontErrors));
        QVERIFY(ok->isVisible() && ok->counting());
        QVERIFY(err->isVisible() && !err->counting());
        QVERIFY(spinUntil([&] { return !ok || !ok->isVisible(); }));
        QVERIFY(okWatch.hiddenAt - posted >= timeout - kTimerSlack);
        qInfo("toast thường ẩn sau %.1f ms (timeout %lld ms)", ms(okWatch.hiddenAt - posted), qint64(timeout.count()));
        spinFor(3 * timeout);
        QVERIFY(err && err->isVisible());
        QCOMPARE(toasts_->toasts(), QList<Toast*>{err.data()});

        QCOMPARE(center_->rowCount(), 2); // mới nhất ở hàng 0
        QCOMPARE(center_->index(0).data().toString(), text(FontErrors));
        QCOMPARE(Severity(center_->index(0).data(NotificationCenter::SeverityRole).toInt()), Severity::Error);
        QCOMPARE(center_->index(1).data().toString(), text(Exported));
        QCOMPARE(center_->unreadCount(), 2); // tự ẩn không tính là đã đọc
        QCOMPARE(err->entry(), center_->index(0).data(NotificationCenter::IdRole).value<NotificationCenter::Id>());

        QTest::mouseClick(err->closeButton(), Qt::LeftButton);
        QVERIFY(!err->isVisible());
        QVERIFY(spinUntil([&] { return !err; }));
        QCOMPARE(center_->unreadCount(), 1);
        QVERIFY(center_->index(0).data(NotificationCenter::ReadRole).toBool());
        QVERIFY(toasts_->toasts().isEmpty());

        // Hành động: gọi hàm đăng ký, đóng toast, đánh dấu đã đọc.
        int ran = 0;
        QPointer<Toast> act = toasts_->post(Severity::Ok, text(Exported), {text(OpenFolder), [&ran] { ++ran; }});
        QVERIFY(act->actionButton() && act->actionButton()->text() == text(OpenFolder));
        QTest::mouseClick(act->actionButton(), Qt::LeftButton);
        QCOMPARE(ran, 1);
        QVERIFY(spinUntil([&] { return !act; }));
        QVERIFY(center_->index(0).data(NotificationCenter::ReadRole).toBool());
        QCOMPARE(center_->rowCount(), 3);
    }

    // Tiêu chí 1: rê chuột dừng đếm, ra thì đếm tiếp phần còn lại; focus bàn phím trong toast cũng dừng.
    void toastHoverPause()
    {
        const auto timeout = 200ms;
        toasts_->setTimeout(timeout);
        QPointer<Toast> t = toasts_->post(Severity::Info, text(Tip));
        QVERIFY(t->counting());
        QTest::mouseMove(window_->windowHandle(), t->geometry().center());
        QVERIFY(spinUntil([&] { return t && t->underMouse() && !t->counting(); }, 5s));
        spinFor(2 * timeout);
        QVERIFY(t && t->isVisible());
        QTest::mouseMove(window_->windowHandle(), t->closeButton()->geometry().center() + t->pos()); // nút con: vẫn rê
        spinFor(2 * timeout);
        QVERIFY(t && !t->counting());
        QTest::mouseMove(window_->windowHandle(), QPoint(1, 1));
        QVERIFY(spinUntil([&] { return !t || !t->isVisible(); })); // đếm tiếp phần còn lại, không kẹt

        // Focus bàn phím (Tab tới nút ×) dừng đếm; ra thì đếm tiếp.
        QPointer<Toast> k = toasts_->post(Severity::Warning, text(DiskLow));
        QCOMPARE(QApplication::focusWidget(), cmd_);
        k->closeButton()->setFocus(Qt::TabFocusReason);
        QVERIFY(spinUntil([&] { return !k->counting(); }));
        spinFor(2 * timeout);
        QVERIFY(k && k->isVisible());
        focusCommandLine();
        QVERIFY(spinUntil([&] { return !k || !k->isVisible(); }));

        // Esc khi focus trong toast: đóng, đánh dấu đã đọc, focus về dòng lệnh.
        QPointer<Toast> e = toasts_->post(Severity::Error, text(FontErrors));
        e->closeButton()->setFocus(Qt::TabFocusReason);
        QTest::keyClick(e->closeButton(), Qt::Key_Escape);
        QVERIFY(!e || !e->isVisible());
        QCOMPARE(QApplication::focusWidget(), cmd_);
        QVERIFY(center_->index(0).data(NotificationCenter::ReadRole).toBool());
    }

    // Tiêu chí 1: tối đa 3 cùng lúc, toast mới đẩy toast cũ nhất; chồng ở góc dưới phải; mọi toast có mục trong center.
    void toastLimit()
    {
        toasts_->setTimeout(1h);
        std::vector<QPointer<Toast>> all;
        std::vector<double> took;
        for (int i = 0; i < 50; ++i) {
            const auto t0 = Clock::now();
            all.emplace_back(toasts_->post(i % 7 == 3 ? Severity::Error : Severity::Info, text(Tip)));
            took.push_back(ms(Clock::now() - t0));
            QCOMPARE(visibleToasts(), std::min(i + 1, ToastHost::kMaxVisible));
            QCOMPARE(toasts_->toasts().size(), qsizetype(std::min(i + 1, ToastHost::kMaxVisible)));
        }
        QCOMPARE(center_->rowCount(), 50);
        QCOMPARE(center_->unreadCount(), 50);
        const QList<Toast*> shown = toasts_->toasts(); // ba toast mới nhất, cũ nhất trước
        QCOMPARE(shown, (QList<Toast*>{all[47], all[48], all[49]}));
        QVERIFY(spinUntil([&] { return std::all_of(all.begin(), all.end() - 3, [](auto& t) { return !t; }); }));

        // .toasts: right 12px, bottom --h-status + 12px, gap 8px; mới nhất ở dưới cùng
        const int margin = qRound(tm_->px(tok::sp_5)), gap = qRound(tm_->px(tok::sp_4));
        QCOMPARE(shown[2]->geometry().right(), window_->width() - margin - 1);
        QCOMPARE(shown[2]->geometry().bottom(), window_->height() - qRound(tm_->px(tok::h_status)) - margin - 1);
        QCOMPARE(shown[1]->geometry().bottom(), shown[2]->geometry().top() - gap - 1);
        QCOMPARE(shown[0]->geometry().bottom(), shown[1]->geometry().top() - gap - 1);
        QVERIFY(shown[2]->width() <= 420);

        // Toast dài xuống dòng trong bề rộng tối đa; cửa sổ đổi cỡ thì đi theo góc.
        QString longText;
        for (int i = 0; i < 4; ++i)
            longText += text(FontErrors) + u' ';
        Toast* wide = toasts_->post(Severity::Error, longText.trimmed());
        QCOMPARE(wide->width(), 420);
        QVERIFY(wide->height() > shown[2]->height());
        window_->resize(1200, 700);
        QVERIFY(spinUntil([&] { return wide->geometry().right() == 1200 - margin - 1; }));
        window_->resize(1366, 768);
        QVERIFY(spinUntil([&] { return wide->geometry().right() == 1366 - margin - 1; }));

        std::sort(took.begin(), took.end());
        qInfo("post(): tốt nhất %.3f ms, p50 %.3f ms, max %.3f ms (50 toast liên tiếp)", took.front(),
              took[took.size() / 2], took.back());
        QVERIFY2(!kTimed || took.front() <= 2.0, "post() chặn luồng UI > 2 ms"); // tốt nhất: không phụ thuộc tải máy
    }

    // Tiêu chí 2: chú thích cạnh con trỏ.
    void cursorTip()
    {
        const auto timeout = 150ms;
        tip_->setTimeout(timeout);
        Watcher watch(tip_);
        spinFor(50ms); // vùng vẽ đã vẽ xong mọi thứ còn chờ
        const int paints = canvas_->paints;

        const QPoint at(200, 150);
        const auto shownAt = Clock::now();
        tip_->showAt(at, text(NoIntersection));
        QVERIFY(tip_->isVisible());
        QVERIFY(tip_->isWindow()); // cửa sổ riêng, không phải lớp con của vùng vẽ
        QCOMPARE(tip_->text(), text(NoIntersection));
        QVERIFY(tip_->counting());
        const QPoint g = canvas_->mapToGlobal(at);
        QCOMPARE(tip_->geometry().topLeft(), g + CursorTip::kOffset);
        QVERIFY(spinUntil([&] { return watch.paints > 0; }));

        // Sát mép phải-dưới: lật sang trái, lên trên, vẫn cách điểm ≥ 16 px, nằm trong vùng vẽ.
        const QPoint corner(canvas_->width() - 4, canvas_->height() - 4);
        tip_->showAt(corner, text(Finish), Severity::Info);
        const QRect r = tip_->geometry();
        const QPoint gc = canvas_->mapToGlobal(corner);
        QVERIFY(gc.x() - r.right() >= 16);
        QVERIFY(gc.y() - r.bottom() >= 16);
        QVERIFY(QRect(canvas_->mapToGlobal(QPoint(0, 0)), canvas_->size()).contains(r));
        QCOMPARE(tip_->severity(), Severity::Info);

        QVERIFY(spinUntil([&] { return !tip_->isVisible(); }));
        QVERIFY(watch.hiddenAt - shownAt >= timeout - kTimerSlack);
        QVERIFY(!tip_->counting());
        spinFor(50ms);
        QCOMPARE(canvas_->paints, paints); // 0 khung vùng vẽ
        QCOMPARE(QApplication::focusWidget(), cmd_);
    }

    // Tiêu chí 3: banner trên vùng vẽ.
    void banner()
    {
        QSignalSpy closed(banners_.data(), &CanvasBanner::closed);
        int saved = 0;
        banners_->add(u"readonly"_s, Severity::Warning, text(ReadOnly),
                      {{text(SaveAs), [&saved] { ++saved; }, Button::Kind::Primary}});
        QVERIFY(banners_->isVisible());
        QCOMPARE(banners_->geometry(), QRect(0, 0, canvas_->width(), banners_->height()));
        QVERIFY(banners_->height() >= 38); // .banner min-height
        QCOMPARE(banners_->visibleCount(), 1);
        QCOMPARE(activeTimers(), 0); // ở lại đến khi đóng: không timer
        spinFor(200ms);
        QVERIFY(banners_->isVisible());

        QWidget* readonly = banners_->row(u"readonly"_s);
        Button* save = buttonWithText(readonly, text(SaveAs));
        QVERIFY(save);
        QCOMPARE(save->kind(), Button::Kind::Primary);
        QTest::mouseClick(save, Qt::LeftButton);
        QCOMPARE(saved, 1);
        QVERIFY(banners_->contains(u"readonly"_s)); // hành động không đóng banner

        banners_->add(u"fonts"_s, Severity::Info, text(FontsSwapped), {{text(ShowList), [] {}}});
        banners_->add(u"recovered"_s, Severity::Ok, text(Recovered), {{text(OpenOriginal), [] {}}});
        banners_->add(u"xref"_s, Severity::Error, text(MissingRefs),
                      {{text(FindRefs), [] {}}, {text(Ignore), [] {}, Button::Kind::Ghost}});
        QCOMPARE(banners_->visibleCount(), 2);
        QCOMPARE(banners_->ids(), (QStringList{u"readonly"_s, u"fonts"_s, u"recovered"_s, u"xref"_s}));
        QWidget* fonts = banners_->row(u"fonts"_s);
        QVERIFY(readonly->isVisible() && fonts->isVisible());
        QVERIFY(!banners_->row(u"recovered"_s)->isVisible() && !banners_->row(u"xref"_s)->isVisible());
        QCOMPARE(banners_->height(), readonly->height() + fonts->height());
        const auto more = [&](QWidget* row) {
            for (QLabel* l : row->findChildren<QLabel*>())
                if (l->isVisible() && l->text().startsWith(u'+'))
                    return l->text();
            return QString();
        };
        QCOMPARE(more(fonts), CanvasBanner::tr("+%n thông báo khác", nullptr, 2));
        QCOMPARE(more(readonly), QString());

        // × bằng chuột: phát closed, banner chờ đầu tiên hiện, số còn chờ giảm.
        QTest::mouseClick(closeButton(readonly), Qt::LeftButton);
        QCOMPARE(closed.size(), 1);
        QCOMPARE(closed[0][0].toString(), u"readonly"_s);
        QWidget* recovered = banners_->row(u"recovered"_s);
        QVERIFY(recovered->isVisible());
        QCOMPARE(more(recovered), CanvasBanner::tr("+%n thông báo khác", nullptr, 1));
        QCOMPARE(more(fonts), QString());

        // × bằng bàn phím: Tab tới được (TabFocus), Space đóng; cú nhấp không lấy focus.
        Button* x = closeButton(fonts);
        QCOMPARE(x->focusPolicy(), Qt::TabFocus);
        x->setFocus(Qt::TabFocusReason);
        QTest::keyClick(x, Qt::Key_Space);
        QCOMPARE(closed.size(), 2);
        QCOMPARE(closed[1][0].toString(), u"fonts"_s);
        QCOMPARE(banners_->ids(), (QStringList{u"recovered"_s, u"xref"_s}));
        QVERIFY(banners_->row(u"xref"_s)->isVisible());
        QCOMPARE(QApplication::focusWidget(), cmd_); // focus về chỗ trước khi Tab vào banner

        // Thay cùng id: giữ chỗ, đổi chữ; remove không phát closed.
        banners_->add(u"recovered"_s, Severity::Info, text(FontsSwapped));
        QCOMPARE(banners_->ids(), (QStringList{u"recovered"_s, u"xref"_s}));
        QCOMPARE(banners_->row(u"recovered"_s)->accessibleName(), text(FontsSwapped));
        QVERIFY(!buttonWithText(banners_->row(u"recovered"_s), text(OpenOriginal)));
        banners_->remove(u"recovered"_s);
        banners_->remove(u"none"_s);
        QCOMPARE(closed.size(), 2);
        QVERIFY(banners_->isVisible());
        banners_->remove(u"xref"_s);
        QVERIFY(!banners_->isVisible());
        QVERIFY(!banners_->contains(u"xref"_s));

        // Vùng vẽ đổi cỡ: banner rộng theo.
        banners_->add(u"readonly"_s, Severity::Warning, text(ReadOnly));
        window_->resize(1200, 700);
        QVERIFY(spinUntil([&] { return banners_->width() == canvas_->width() && canvas_->width() < 1366; }));
        window_->resize(1366, 768);
        QVERIFY(spinUntil([&] { return banners_->width() == canvas_->width() && canvas_->width() == 1366; }));
    }

    // Tiêu chí 4: trình đọc màn hình, biểu tượng theo mức.
    void accessibility()
    {
        QAccessible::installUpdateHandler(recordA11y);
        QAccessible::setActive(true);
        g_a11y.clear();
        Toast* err = toasts_->post(Severity::Error, text(FontErrors));
        Toast* ok = toasts_->post(Severity::Ok, text(Exported));
        banners_->add(u"readonly"_s, Severity::Warning, text(ReadOnly));
        banners_->add(u"fonts"_s, Severity::Info, text(FontsSwapped));
        banners_->add(u"recovered"_s, Severity::Ok, text(Recovered)); // chờ: chưa đọc
        QWidget* recovered = banners_->row(u"recovered"_s);
        const int waiting = count(recovered, QAccessible::Announcement);
        banners_->remove(u"fonts"_s); // hết chờ: đọc khi hiện
        tip_->showAt(QPoint(100, 100), text(NoIntersection));
        QLineEdit field(window_.get());
        FieldMessage why(&field, window_.get());
        why.setError(text(ScaleError));
        why.setError(text(ScaleError)); // không đổi: không đọc lại
        why.setHint(text(DotHint));     // gợi ý: không đọc
        QAccessible::installUpdateHandler(nullptr);
        QAccessible::setActive(false);

        QCOMPARE(count(err, QAccessible::Alert), 1);
        const A11yEvent* e = announcement(err);
        QVERIFY(e);
        QCOMPARE(e->message, text(FontErrors));
        QCOMPARE(e->politeness, QAccessible::AnnouncementPoliteness::Assertive);
        QCOMPARE(count(ok, QAccessible::Alert), 0);
        QVERIFY(announcement(ok));
        QCOMPARE(announcement(ok)->politeness, QAccessible::AnnouncementPoliteness::Polite);
        QCOMPARE(QAccessible::queryAccessibleInterface(err)->role(), QAccessible::AlertMessage);
        QCOMPARE(QAccessible::queryAccessibleInterface(ok)->role(), QAccessible::StatusBar);
        QCOMPARE(QAccessible::queryAccessibleInterface(err)->text(QAccessible::Name), text(FontErrors));

        QWidget* row = banners_->row(u"readonly"_s);
        QVERIFY(announcement(row));
        QCOMPARE(announcement(row)->politeness, QAccessible::AnnouncementPoliteness::Polite);
        QCOMPARE(announcement(row)->message, text(ReadOnly));
        QCOMPARE(QAccessible::queryAccessibleInterface(row)->role(), QAccessible::StatusBar);
        QCOMPARE(waiting, 0);
        QCOMPARE(count(recovered, QAccessible::Announcement), 1);
        QCOMPARE(announcement(recovered)->message, text(Recovered));
        QVERIFY(announcement(tip_));
        QCOMPARE(announcement(tip_)->politeness, QAccessible::AnnouncementPoliteness::Polite);
        QCOMPARE(QAccessible::queryAccessibleInterface(tip_)->role(), QAccessible::StatusBar);

        QCOMPARE(count(&field, QAccessible::Announcement), 1); // từ ô được mô tả
        QCOMPARE(announcement(&field)->message, text(ScaleError));
        QCOMPARE(field.accessibleDescription(), text(DotHint));
        QCOMPARE(QAccessible::queryAccessibleInterface(&why)->role(), QAccessible::StaticText);
        QCOMPARE(QAccessible::queryAccessibleInterface(&why)->text(QAccessible::Name), text(DotHint));
        QCOMPARE(QAccessible::queryAccessibleInterface(closeButton(err))->text(QAccessible::Name),
                 Toast::tr("Đóng thông báo"));

        // Mỗi mức một biểu tượng khác hình (không chỉ màu), có trên toast, banner, chú thích.
        QSet<QString> names;
        for (Severity s : {Severity::Ok, Severity::Info, Severity::Warning, Severity::Error}) {
            names.insert(iconName(s));
            QVERIFY(IconProvider::names().contains(iconName(s)));
        }
        QCOMPARE(names.size(), 4);
        for (Toast* t : {err, ok}) {
            auto* glyph = t->findChild<Glyph*>();
            QVERIFY(glyph && glyph->isVisible());
            QCOMPARE(glyph->name(), iconName(t->severity()));
        }
        QCOMPARE(row->findChild<Glyph*>()->name(), iconName(Severity::Warning));
        QCOMPARE(tip_->findChild<Glyph*>()->name(), iconName(Severity::Error));
    }

    // Tiêu chí 5: không lấy focus, không kích hoạt cửa sổ; lần vẽ đầu ≤ 1 khung sau lời gọi.
    void frameAndFocus()
    {
        toasts_->setTimeout(1h);
        focusCommandLine();
        QWidget* active = QApplication::activeWindow();
        QCOMPARE(active, window_.get());
        Watcher tipWatch(tip_);
        Clock::duration toastBest = Clock::duration::max(), bannerBest = toastBest, tipBest = toastBest;
        for (int round = 0; round < 3; ++round) {
            auto t0 = Clock::now();
            Toast* t = toasts_->post(round ? Severity::Info : Severity::Error, text(DiskLow));
            Watcher w(t);
            QVERIFY(spinUntil([&] { return w.paints > 0; }));
            toastBest = std::min(toastBest, w.firstPaintAt - t0);

            const QString id = u"b%1"_s.arg(round);
            t0 = Clock::now();
            banners_->add(id, Severity::Warning, text(ReadOnly));
            Watcher b(banners_->row(id));
            QVERIFY(spinUntil([&] { return b.paints > 0; }));
            bannerBest = std::min(bannerBest, b.firstPaintAt - t0);
            banners_->remove(id);

            tip_->hide();
            tipWatch.paints = 0;
            t0 = Clock::now();
            tip_->showAt(QPoint(300, 200), text(NoIntersection));
            QVERIFY(spinUntil([&] { return tipWatch.paints > 0; }));
            tipBest = std::min(tipBest, tipWatch.firstPaintAt - t0);

            QCOMPARE(QApplication::focusWidget(), cmd_);
            QCOMPARE(QApplication::activeWindow(), active);
            QVERIFY(!tip_->isActiveWindow());
        }
        qInfo(
            "lần vẽ đầu sau lời gọi (tốt nhất của 3): toast %.2f ms, banner %.2f ms, chú thích %.2f ms; khung %.2f ms",
            ms(toastBest), ms(bannerBest), ms(tipBest), ms(frameTime()));
        for (Clock::duration d : {toastBest, bannerBest, tipBest})
            QVERIFY2(!kTimed || d <= frameTime(), "hiện chậm hơn 1 khung");
        for (Toast* t : toasts_->toasts())
            QCOMPARE(t->focusPolicy(), Qt::NoFocus);
        QCOMPARE(toasts_->toasts().back()->closeButton()->focusPolicy(), Qt::TabFocus);
        QCOMPARE(tip_->focusPolicy(), Qt::NoFocus);
        QVERIFY(tip_->testAttribute(Qt::WA_ShowWithoutActivating));
    }

    // Tiêu chí 6: rảnh không timer; timer chỉ chạy khi toast thường, chú thích đang hiện.
    void idle()
    {
        toasts_->setTimeout(150ms);
        tip_->setTimeout(100ms);
        QCOMPARE(activeTimers(), 0);
        QPointer<Toast> info = toasts_->post(Severity::Info, text(Tip));
        QCOMPARE(activeTimers(), 1);
        QPointer<Toast> err = toasts_->post(Severity::Error, text(FontErrors));
        QCOMPARE(activeTimers(), 1); // lỗi không đếm giờ
        tip_->showAt(QPoint(50, 50), text(NoIntersection));
        QCOMPARE(activeTimers(), 2);
        banners_->add(u"readonly"_s, Severity::Warning, text(ReadOnly));
        QCOMPARE(activeTimers(), 2);
        QVERIFY(spinUntil([&] { return !info && !tip_->isVisible(); }));
        QCOMPARE(activeTimers(), 0);
        QTest::mouseClick(err->closeButton(), Qt::LeftButton);
        banners_->remove(u"readonly"_s);
        QVERIFY(spinUntil([&] { return !err; }));
        QCOMPARE(activeTimers(), 0);
        QVERIFY(toasts_->toasts().isEmpty());
    }

    // Thời gian thật: chú thích ẩn sau 2 s, toast sau 5 s (cùng lúc, ~5 s).
    void realTime()
    {
        QCOMPARE(toasts_->timeout(), 5000ms);
        QCOMPARE(tip_->timeout(), 2000ms);
        Watcher tipWatch(tip_);
        const auto t0 = Clock::now();
        QPointer<Toast> t = toasts_->post(Severity::Ok, text(Exported));
        Watcher toastWatch(t);
        tip_->showAt(QPoint(400, 300), text(NoIntersection));
        QVERIFY(spinUntil([&] { return !tip_->isVisible(); }, 10s));
        QVERIFY(spinUntil([&] { return !t || !t->isVisible(); }, 15s));
        const double tipMs = ms(tipWatch.hiddenAt - t0), toastMs = ms(toastWatch.hiddenAt - t0);
        qInfo("thời gian thật: chú thích ẩn sau %.1f ms (2.000), toast sau %.1f ms (5.000)", tipMs, toastMs);
        const double slack = double(kTimerSlack.count());
        QVERIFY(tipMs >= 2000 - slack && toastMs >= 5000 - slack); // không sớm; độ trễ phụ thuộc tải máy: chỉ báo
    }

    // followJobs: tác vụ nền lỗi → toast lỗi; xong sau mốc hiện tiến trình → toast; xong nhanh, hủy → không.
    void followJobs()
    {
        jobs::BackgroundJobs registry({1ms, 300ms, 1ms}); // mốc hiện tiến trình 300 ms: việc nhanh chắc chắn dưới mốc
        ProgressPresenter presenter(registry);
        toasts_->setTimeout(1h);
        toasts_->followJobs(presenter);
        const QString name = u"Chuyển font"_s, disk = u"Ổ đĩa đầy"_s; // i18n-ok: dữ liệu thử
        registry.start(name, u"fontfix"_s, 0, [](jobs::Progress&, sched::CancelToken) -> Expected<void> {
            return trion::Error{"font", "disk", "Ổ đĩa đầy"}; // i18n-ok: dữ liệu thử
        });
        QVERIFY(spinUntil([&] { return toasts_->toasts().size() == 1; }));
        QCOMPARE(toasts_->toasts()[0]->severity(), Severity::Error);
        QCOMPARE(toasts_->toasts()[0]->text(), ToastHost::tr("%1 — lỗi: %2").arg(name, disk));

        registry.start(name, u"quick"_s, 0, [](jobs::Progress&, sched::CancelToken) -> Expected<void> { return {}; });
        std::atomic<bool> release{false};
        const jobs::JobId cancelled =
            registry.start(name, u"gated"_s, 0, [&release](jobs::Progress&, sched::CancelToken c) -> Expected<void> {
                while (!release && !c.cancelled())
                    std::this_thread::sleep_for(1ms);
                return {};
            });
        registry.start(u"Xuất PDF"_s, u"plot"_s, 0, [](jobs::Progress& p, sched::CancelToken) -> Expected<void> {
            std::this_thread::sleep_for(400ms); // quá mốc hiện tiến trình
            p.set(1);
            return {};
        });
        QVERIFY(spinUntil([&] { return presenter.shown(cancelled); }));
        registry.cancel(cancelled);
        QVERIFY(spinUntil([&] { return registry.size() == 0; }));
        spinFor(50ms);
        QCOMPARE(toasts_->toasts().size(), 2); // lỗi + xuất PDF; xong nhanh và bị hủy: không toast
        QCOMPARE(toasts_->toasts()[1]->severity(), Severity::Ok);
        QCOMPARE(toasts_->toasts()[1]->text(), ToastHost::tr("%1 — xong").arg(u"Xuất PDF"_s));
        QCOMPARE(center_->rowCount(), 2);

        registry.start(name, u"bare"_s, 0, [](jobs::Progress&, sched::CancelToken) -> Expected<void> {
            return trion::Error{"font", "x", {}};
        });
        QVERIFY(spinUntil([&] { return toasts_->toasts().size() == 3; }));
        QCOMPARE(toasts_->toasts()[2]->text(), ToastHost::tr("%1 — lỗi").arg(name));
        QCOMPARE(ToastHost::instance(), toasts_.get());
        release = true;
        sched::waitIdle();
    }

    void centerModel()
    {
        NotificationCenter c;
        QSignalSpy unread(&c, &NotificationCenter::unreadChanged);
        const auto before = QDateTime::currentDateTime();
        const auto a = c.add(u"a"_s, Severity::Info, u"tip"_s);
        const auto b = c.add(u"b"_s, Severity::Warning);
        QCOMPARE(c.rowCount(), 2);
        QCOMPARE(c.rowCount(c.index(0)), 0);
        QCOMPARE(c.unreadCount(), 2);
        QCOMPARE(unread.size(), 2);
        const QModelIndex top = c.index(0);
        QCOMPARE(top.data().toString(), u"b"_s);
        QCOMPARE(top.data(NotificationCenter::IdRole).value<NotificationCenter::Id>(), b);
        QCOMPARE(Severity(top.data(NotificationCenter::SeverityRole).toInt()), Severity::Warning);
        QCOMPARE(top.data(NotificationCenter::KindRole).toString(), u"task"_s);
        QCOMPARE(c.index(1).data(NotificationCenter::KindRole).toString(), u"tip"_s);
        QVERIFY(top.data(NotificationCenter::TimeRole).toDateTime() >= before);
        QVERIFY(!top.data(NotificationCenter::ReadRole).toBool());
        QVERIFY(!top.data(Qt::DecorationRole).isValid());
        QVERIFY(!c.index(5).data().isValid());

        QSignalSpy changed(&c, &QAbstractItemModel::dataChanged);
        c.markRead(a);
        QCOMPARE(c.unreadCount(), 1);
        QCOMPARE(changed.size(), 1);
        c.markRead(a); // đã đọc
        c.markRead(999);
        QCOMPARE(changed.size(), 1);
        QCOMPARE(unread.size(), 3);
        c.markAllRead();
        QCOMPARE(c.unreadCount(), 0);
        c.markAllRead();
        QCOMPARE(unread.size(), 4);
        c.remove(b);
        c.remove(b);
        QCOMPARE(c.rowCount(), 1);
        QCOMPARE(c.index(0).data().toString(), u"a"_s);

        // Giới hạn: bỏ mục cũ nhất, đếm chưa đọc vẫn đúng.
        for (int i = 0; i < NotificationCenter::kMaxEntries + 10; ++i)
            c.add(QString::number(i), Severity::Ok);
        QCOMPARE(c.rowCount(), NotificationCenter::kMaxEntries);
        QCOMPARE(c.unreadCount(), NotificationCenter::kMaxEntries);
        QCOMPARE(c.index(0).data().toString(), QString::number(NotificationCenter::kMaxEntries + 9));
        c.remove(c.index(0).data(NotificationCenter::IdRole).value<NotificationCenter::Id>());
        QCOMPARE(c.unreadCount(), NotificationCenter::kMaxEntries - 1);
    }

    void emptyAndField()
    {
        EmptyState empty(u"insert"_s, window_.get());
        QSignalSpy triggered(&empty, &EmptyState::actionTriggered);
        empty.setText(text(EmptyTitle), text(EmptyWhy));
        empty.setAction(text(EmptyAction));
        empty.setGeometry(0, 0, 300, 220);
        empty.show();
        const auto labels = empty.findChildren<QLabel*>();
        QCOMPARE(labels.size(), 2);
        QVERIFY(labels[0]->isVisible() && labels[1]->isVisible());
        QCOMPARE(labels[0]->font().weight(), QFont::DemiBold); // --fw-b
        QCOMPARE(empty.findChild<Glyph*>()->name(), u"insert"_s);
        QCOMPARE(empty.actionButton()->text(), text(EmptyAction));
        QTest::mouseClick(empty.actionButton(), Qt::LeftButton);
        QCOMPARE(triggered.size(), 1);

        // Nút nữa cùng hàng (.empty .hrow): sau nút gợi ý, cách --sp-4, cả hàng giữa; bấm chỉ phát clicked của nó
        const int center = empty.width() / 2;
        QVERIFY(std::abs(empty.actionButton()->geometry().center().x() - center) <= 1); // một nút: giữa như trước
        Button* more = empty.addAction(text(EmptyMore));
        QSignalSpy moreClicked(more, &QPushButton::clicked);
        QCoreApplication::processEvents(); // con thêm vào widget đang hiện: Qt hiện nó ở lượt sự kiện kế
        empty.layout()->activate();
        const QRect a = empty.actionButton()->geometry(), b = more->geometry();
        QVERIFY(more->isVisible());
        QCOMPARE(more->text(), text(EmptyMore));
        QCOMPARE(more->controlSize(), Button::Size::Small);
        QCOMPARE(a.center().y(), b.center().y());
        QCOMPARE(b.left() - a.right() - 1, qRound(tm_->px(tok::sp_4)));
        QVERIFY(std::abs((a.left() + b.right()) / 2 - center) <= 1);
        QCOMPARE(empty.actionButton()->nextInFocusChain(), more); // Tab: nút gợi ý rồi nút thêm
        QTest::mouseClick(more, Qt::LeftButton);
        QCOMPARE(moreClicked.size(), 1);
        QCOMPARE(triggered.size(), 1);

        empty.setText(text(EmptyTitle));
        empty.setAction({});
        QVERIFY(!labels[1]->isVisible());
        QVERIFY(!empty.actionButton()->isVisible());
        empty.layout()->activate();
        QVERIFY(std::abs(more->geometry().center().x() - center) <= 1); // còn một nút: giữa
        empty.setIconName(u"layers"_s);
        QCOMPARE(empty.findChild<Glyph*>()->name(), u"layers"_s);

        QLineEdit field(window_.get());
        FieldMessage message(&field, window_.get());
        message.setGeometry(0, 230, 200, 40);
        QVERIFY(message.minimumHeight() >= 14);
        message.setError(text(ScaleError));
        QVERIFY(message.isError());
        QCOMPARE(message.text(), text(ScaleError));
        QCOMPARE(field.accessibleDescription(), text(ScaleError));
        QVERIFY(message.indent() > 0); // chỗ biểu tượng cảnh báo
        message.setHint(text(DotHint));
        QVERIFY(!message.isError());
        QCOMPARE(message.indent(), 0);
        message.setError({});
        QVERIFY(!message.isError());
        QVERIFY(message.text().isEmpty());
        QCOMPARE(field.accessibleDescription(), QString());
        FieldMessage alone(nullptr, window_.get()); // không có ô: vẫn chạy
        alone.setError(text(ScaleError));
        QVERIFY(alone.isError());
    }

    // DoD-U: ảnh mọi loại (toast 4 mức, banner kèm đếm và banner lỗi, chú thích lỗi/khác, trạng thái rỗng, dòng dưới
    // ô).
    void baseline_data()
    {
        QTest::addColumn<bool>("light");
        QTest::addColumn<qreal>("dpr");
        for (bool light : {false, true})
            for (qreal dpr : {1.0, 1.5, 2.0})
                QTest::addRow("%s-%d", light ? "light" : "dark", qRound(dpr * 100)) << light << dpr;
    }

    void baseline()
    {
        QFETCH(bool, light);
        QFETCH(qreal, dpr);
        tm_->setTheme(light ? Theme::Light : Theme::Dark);
        QCoreApplication::processEvents(); // Qt 6.11 gửi ApplicationPaletteChange qua hàng đợi
        toasts_->setTimeout(1h);
        tip_->setTimeout(1h);
        const QColor bg = tm_->color(tok::c_base);
        const int pad = 10, gap = 12;
        QImage sheet(QSize(kSheetW, 1000) * dpr, QImage::Format_ARGB32_Premultiplied);
        sheet.setDevicePixelRatio(dpr);
        sheet.fill(bg);
        QPainter p(&sheet);
        int y = pad;
        const auto draw = [&](QWidget* w, QWidget::RenderFlags flags = QWidget::DrawChildren) {
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            w->render(&p, QPoint(pad, y), {}, flags);
            y += w->height() + gap;
        };

        const auto none = [] {};
        draw(toasts_->post(Severity::Ok, text(Exported), {text(OpenFolder), none}));
        draw(toasts_->post(Severity::Info, text(Tip)));
        draw(toasts_->post(Severity::Warning, text(DiskLow)));
        draw(toasts_->post(Severity::Error, text(FontErrors), {text(View), none}));

        QWidget canvas(window_.get()); // vùng vẽ nhỏ cho banner
        canvas.resize(kSheetW - 2 * pad, 200);
        canvas.show();
        auto* banners = new CanvasBanner(&canvas);
        banners->add(u"recovered"_s, Severity::Info, text(Recovered),
                     {{text(OpenOriginal), none, Button::Kind::Primary}});
        banners->add(u"readonly"_s, Severity::Warning, text(ReadOnly), {{text(SaveAs), none}});
        banners->add(u"xref"_s, Severity::Error, text(MissingRefs), {{text(FindRefs), none}});
        draw(banners);
        banners->remove(u"recovered"_s);
        banners->remove(u"readonly"_s);
        banners->add(u"fonts"_s, Severity::Ok, text(FontsSwapped));
        draw(banners);

        tip_->showAt(QPoint(10, 10), text(NoIntersection));
        draw(tip_, QWidget::DrawWindowBackground | QWidget::DrawChildren);
        tip_->showAt(QPoint(10, 10), text(Finish), Severity::Info);
        draw(tip_, QWidget::DrawWindowBackground | QWidget::DrawChildren);

        EmptyState empty(u"insert"_s, window_.get());
        empty.setText(text(EmptyTitle), text(EmptyWhy));
        empty.setAction(text(EmptyAction));
        empty.resize(kSheetW - 2 * pad, empty.heightForWidth(kSheetW - 2 * pad));
        empty.show();
        draw(&empty);

        FieldMessage error(nullptr, window_.get()), hint(nullptr, window_.get());
        error.setError(text(ScaleError));
        hint.setHint(text(DotHint));
        for (FieldMessage* m : {&error, &hint}) {
            m->resize(220, m->heightForWidth(220)); // xuống dòng
            m->show();
            draw(m);
        }
        p.end();
        sheet = sheet.copy(0, 0, qRound(kSheetW * dpr), qRound(y * dpr));
        sheet.setDevicePixelRatio(dpr);

        const QString name = u"message-%1-%2-%3.png"_s.arg(light ? u"light"_s : u"dark"_s)
                                 .arg(qRound(dpr * 100))
                                 .arg(QLatin1StringView(kPlatform));
        const QString out = qEnvironmentVariable("TRION_UPDATE_BASELINE");
        if (!out.isEmpty()) {
            QVERIFY(sheet.save(out + u'/' + name, "PNG", 0));
            QSKIP("đã ghi baseline");
        }
        const QImage want = QImage(u":/baseline/"_s + name).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QVERIFY2(!want.isNull(), qPrintable(u"thiếu baseline "_s + name));
        QCOMPARE(sheet.size(), want.size());
        qsizetype off = 0;
        int maxDiff = 0;
        for (int yy = 0; yy < sheet.height(); ++yy) {
            const auto* a = reinterpret_cast<const QRgb*>(sheet.constScanLine(yy));
            const auto* b = reinterpret_cast<const QRgb*>(want.constScanLine(yy));
            for (int x = 0; x < sheet.width(); ++x) {
                const int d = std::max({std::abs(qRed(a[x]) - qRed(b[x])), std::abs(qGreen(a[x]) - qGreen(b[x])),
                                        std::abs(qBlue(a[x]) - qBlue(b[x])), std::abs(qAlpha(a[x]) - qAlpha(b[x]))});
                maxDiff = std::max(maxDiff, d);
                off += d > kNoise;
            }
        }
        qInfo("%s: %lld điểm ảnh lệch > %d/255, lệch lớn nhất %d/255", qPrintable(name), qint64(off), kNoise, maxDiff);
        if (off > sheet.width() * sheet.height() / 1000)
            sheet.save(QDir::temp().filePath(u"got-"_s + name)); // để so bằng mắt
        QVERIFY(off <= sheet.width() * sheet.height() / 1000);
    }

    // DoD-U: chữ dài hơn 40% (vi-XL) không bị cắt, cả gọn và thoáng.
    void pseudoNoTruncation()
    {
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QVERIFY(spinUntil([&] { return text(ReadOnly).size() >= QString::fromUtf8(kText[ReadOnly]).size() + 2; }));
        toasts_->setTimeout(1h);
        tip_->setTimeout(1h);
        const auto none = [] {};
        toasts_->post(Severity::Ok, text(Exported), {text(OpenFolder), none});
        toasts_->post(Severity::Warning, text(DiskLow));
        toasts_->post(Severity::Error, text(FontErrors), {text(View), none});
        banners_->add(u"readonly"_s, Severity::Warning, text(ReadOnly), {{text(SaveAs), none, Button::Kind::Primary}});
        banners_->add(u"xref"_s, Severity::Error, text(MissingRefs),
                      {{text(FindRefs), none}, {text(Ignore), none, Button::Kind::Ghost}});
        banners_->add(u"fonts"_s, Severity::Info, text(FontsSwapped)); // chờ: "+1 thông báo khác"
        tip_->showAt(QPoint(300, 200), text(NoIntersection));

        QWidget form(window_.get()); // palette mẫu: trạng thái rỗng, ô có dòng lỗi, ô có gợi ý
        auto* col = new QVBoxLayout(&form);
        auto* empty = new EmptyState(u"insert"_s);
        empty->setText(text(EmptyTitle), text(EmptyWhy));
        empty->setAction(text(EmptyAction));
        auto* field = new QLineEdit;
        auto* error = new FieldMessage(field);
        auto* hint = new FieldMessage(nullptr);
        error->setError(text(ScaleError));
        hint->setHint(text(DotHint));
        for (QWidget* w : std::initializer_list<QWidget*>{empty, field, error, hint})
            col->addWidget(w);
        form.setGeometry(20, 60, 272, 400); // rộng --pal-w
        form.show();

        for (Density d : {Density::Comfortable, Density::Compact}) {
            tm_->setDensity(d);
            QVERIFY(spinUntil([&] { return truncations(window_.get()).isEmpty(); }, 5s));
            QCOMPARE(truncations(window_.get()), QString());
            QCOMPARE(truncations(tip_), QString());
        }
        QVERIFY(toasts_->toasts().size() == 3);
        QVERIFY(banners_->row(u"xref"_s)->findChildren<QLabel*>().size() >= 2);
    }
};

QTEST_MAIN(MessageTest)
#include "message_test.moc"
