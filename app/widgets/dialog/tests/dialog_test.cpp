// UI0-110: khung hộp thoại (DialogFrame, DialogHost). Mỗi tiêu chí nghiệm thu một nhóm hàm test:
//   1  firstFrame: mở bằng phím (như lệnh gõ ở dòng lệnh) rồi đóng 20 lần qua DialogHost; UiProbe dialog_open tính từ
//      phím đến hết khung đầu, p50/p99 (ngưỡng 150 ms chỉ kiểm ở release, tests/timing.h). lazyBody: loadBody với dữ
//      liệu do test thả: khung đầu hiện "Đang tải…" khi dữ liệu còn chặn ở luồng nền (luồng UI vẫn chạy), nút xác nhận
//      vô hiệu; dữ liệu về thì thân mới; đóng khi đang tải thì token hủy, không dựng. loadingSpinner: vòng quay của
//      thân đang tải là ui::Spinner dùng chung.
//   2  enterEsc, helpKey, tabCycle, focusReturn: Enter = nút xác nhận, không khi ô nhiều dòng hoặc nút xác nhận vô
//      hiệu; Esc hủy; F1 và nút Trợ giúp phát tên lệnh, thắng phím tắt F1 toàn cục; Tab/Shift+Tab vòng trong hộp thoại;
//      đóng thì focus về chỗ cũ, cả khi xếp chồng (modal mở từ modal chặn hộp thoại mở nó).
//   3  modelessModal: phím, chuột gửi tới cửa sổ chính qua QWindow (như sự kiện hệ điều hành: Qt bỏ sự kiện của cửa sổ
//      bị modal chặn) tới được dòng lệnh, vùng vẽ khi modeless, không tới khi modal; mở lại cùng mã: cùng bản, dựng một
//      lần, được đưa lên.
//   4  geometry: vị trí, cỡ khôi phục theo mã (Store "dialog/<mã>"), cỡ cố định theo nội dung; geometrySavedOnExit:
//      còn mở khi DialogHost hủy vẫn lưu; place: hàm thuần với màn hình giả lập, màn hình đã rút thì vào màn hình của
//      cửa sổ chính; dragMoveResize: kéo đầu di chuyển, kéo mép đổi cỡ.
//   5  confirmLabels: duyệt registry ở tiếng Việt, English thật, English giả dịch nhãn thành "OK".
//   6  baseline (DoD-U 1): ảnh khung Tối/Sáng × 100/150/200% so baseline theo nền tảng (ghi lại:
//      TRION_UPDATE_BASELINE=<thư mục> chạy test, chép frame-*-<nền tảng>.png vào tests/baseline); lite: Giao diện nhẹ
//      thay bóng bằng viền 1 px; pseudoNoTruncation: vi-XL, findTruncations rỗng; pickHide: ẩn tạm, cửa sổ chính dùng
//      được, hiện lại đúng chỗ, focus cũ.
#include "app/widgets/dialog/DialogFrame.h"
#include "app/widgets/dialog/DialogHost.h"
#include "app/widgets/progress/Spinner.h"

#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/perf/UiProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "core/base/sched/Scheduler.h"
#include "tests/timing.h"

#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QEventLoop>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QSet>
#include <QShortcut>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QTranslator>
#include <QtTest>
#include <QWindow>

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;
using namespace trion::theme;
using namespace trion::ui;
namespace i18n = trion::i18n;
namespace sched = trion::sched;
namespace settings = trion::settings;

namespace {

using trion::test::kTimed;

#if defined(Q_OS_WIN)
constexpr auto kPlatform = "windows";
#elif defined(Q_OS_MACOS)
constexpr auto kPlatform = "macos";
#else
constexpr auto kPlatform = "linux";
#endif
constexpr int kNoise = 8; // như controls_test: lệch ≤ 8/255 mỗi kênh bỏ qua, điểm lệch hơn tối đa 0,1%
constexpr double kOpenBudgetMs = 150; // DoD-L "mở hộp thoại đến khung đầu có nội dung", máy tầm trung

// Chữ mẫu, dịch qua ngữ cảnh "Sample" (QCoreApplication::translate với biến: lupdate không trích).
constexpr char kTitle[] = "Tạo block";          // i18n-ok: chữ mẫu của test
constexpr char kApply[] = "Áp dụng";            // i18n-ok: chữ mẫu của test
constexpr char kNameLabel[] = "Tên block";      // i18n-ok: chữ mẫu của test
constexpr char kNoteLabel[] = "Mô tả";          // i18n-ok: chữ mẫu của test
constexpr char kCheckLabel[] = "Giữ đối tượng"; // i18n-ok: chữ mẫu của test

QString sampleTr(const char* source)
{
    return QCoreApplication::translate("Sample", source);
}

// Hộp thoại mẫu: hai nhãn, ô một dòng, ô nhiều dòng, hộp kiểm; nút Hủy và nút xác nhận.
class Sample : public DialogFrame {
public:
    explicit Sample(QWidget* parent, const QString& id = u"sample"_s, bool modal = true, const char* confirm = kApply)
        : DialogFrame(id, parent), confirm_(confirm)
    {
        setModal(modal);
        setCommand(u"SAMPLE"_s);
        auto* body = new QWidget;
        auto* v = new QVBoxLayout(body);
        v->setContentsMargins(0, 0, 0, 0);
        name = new QLabel;
        line = new QLineEdit;
        note = new QLabel;
        plain = new QPlainTextEdit;
        plain->setFixedHeight(60);
        check = new QCheckBox;
        for (QWidget* w : {static_cast<QWidget*>(name), static_cast<QWidget*>(line), static_cast<QWidget*>(note),
                           static_cast<QWidget*>(plain), static_cast<QWidget*>(check)})
            v->addWidget(w);
        setBody(body);
        addCancel();
        retranslate();
    }

    QLabel* name;
    QLineEdit* line;
    QLabel* note;
    QPlainTextEdit* plain;
    QCheckBox* check;

protected:
    void changeEvent(QEvent* e) override
    {
        if (e->type() == QEvent::LanguageChange)
            retranslate();
        DialogFrame::changeEvent(e);
    }

private:
    void retranslate()
    {
        setWindowTitle(sampleTr(kTitle));
        setConfirm(sampleTr(confirm_));
        name->setText(sampleTr(kNameLabel));
        note->setText(sampleTr(kNoteLabel));
        check->setText(sampleTr(kCheckLabel));
    }
    const char* confirm_;
};

// Bản dịch English giả: nhãn "Áp dụng" của Sample thành "OK" (lỗi người dịch mà CI phải bắt).
class FakeEnglish : public QTranslator {
public:
    QString translate(const char* context, const char* source, const char*, int) const override
    {
        return qstrcmp(context, "Sample") == 0 && qstrcmp(source, kApply) == 0 ? u"OK"_s : QString();
    }
    bool isEmpty() const override { return false; }
};

// Vùng vẽ giả: đếm nhấn chuột.
class Canvas : public QWidget {
public:
    int presses = 0;

protected:
    void mousePressEvent(QMouseEvent*) override { ++presses; }
};

// Cửa sổ chính giả: vùng vẽ và dòng lệnh. F5 ở dòng lệnh gọi onF5 (như gõ lệnh mở hộp thoại).
class MainWindow : public QWidget {
public:
    MainWindow()
    {
        auto* v = new QVBoxLayout(this);
        canvas = new Canvas;
        canvas->setMinimumSize(400, 240);
        cmd = new QLineEdit;
        v->addWidget(canvas);
        v->addWidget(cmd);
        cmd->installEventFilter(this);
        resize(600, 400);
    }
    Canvas* canvas;
    QLineEdit* cmd;
    std::function<void()> onF5;

protected:
    bool eventFilter(QObject* watched, QEvent* e) override
    {
        if (watched == cmd && e->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(e)->key() == Qt::Key_F5 &&
            onF5) {
            onF5();
            return true;
        }
        return QWidget::eventFilter(watched, e);
    }
};

// Cửa sổ đang bị modal chặn (QEvent::WindowBlocked / WindowUnblocked).
class BlockWatch : public QObject {
public:
    explicit BlockWatch(QWidget* w) { w->installEventFilter(this); }
    bool blocked = false;
    bool eventFilter(QObject*, QEvent* e) override
    {
        if (e->type() == QEvent::WindowBlocked)
            blocked = true;
        else if (e->type() == QEvent::WindowUnblocked)
            blocked = false;
        return false;
    }
};

// Chờ điều kiện trong vòng sự kiện thật (QEventLoop + QTimer, như ui_probe_test).
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

// Cửa sổ đã active với QApplication (qWaitForWindowActive có thể trả sớm theo trạng thái của plugin offscreen, trước
// khi Qt chuyển focus sang cửa sổ).
bool waitActive(QWidget* w)
{
    return spinUntil([w] { return QApplication::activeWindow() == w->window(); });
}

// Khung trơn: tiêu đề, tên lệnh, Hủy, xác nhận; thân do test đặt.
DialogFrame* plainFrame(const QString& id, QWidget* parent)
{
    auto* f = new DialogFrame(id, parent);
    f->setWindowTitle(sampleTr(kTitle));
    f->setCommand(u"SAMPLE"_s);
    f->addCancel();
    f->setConfirm(sampleTr(kApply));
    return f;
}

int probeCount(const char* op)
{
    const QJsonObject j = probe::toJson();
    return j["ops"_L1][QLatin1StringView(op)]["count"_L1].toInt();
}

Button* headerButton(QWidget* dialog, const QString& icon)
{
    for (Button* b : dialog->findChildren<Button*>())
        if (b->iconName() == icon)
            return b;
    return nullptr;
}

QString truncations(QWidget* w)
{
    QStringList out;
    for (const i18n::Truncation& t : i18n::findTruncations(w))
        out << t.toString();
    return out.join(u"; "_s);
}

} // namespace

class DialogTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<UiExecutor> executor_;
    std::unique_ptr<ThemeManager> tm_;
    std::unique_ptr<IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<MainWindow> main_;

    bool activateMain()
    {
        main_->activateWindow();
        return waitActive(main_.get());
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        QVERIFY(loadFonts());
        executor_ = std::make_unique<UiExecutor>();
        tm_ = std::make_unique<ThemeManager>(*store_);
        icons_ = std::make_unique<IconProvider>();
        translator_ = std::make_unique<i18n::Translator>();
        QApplication::setCursorFlashTime(0); // con trỏ ô nhập không nhấp nháy: ảnh tất định
        main_ = std::make_unique<MainWindow>();
        main_->move(100, 80);
        main_->show();
        QVERIFY(activateMain());
    }

    void init()
    {
        tm_->setTheme(Theme::Dark);
        tm_->setLite(false);
        for (const QString& k : store_->keys())
            if (k.startsWith(u"dialog/"_s))
                store_->remove(k);
        main_->cmd->clear();
        main_->cmd->setFocus();
        QVERIFY(activateMain());
    }

    void cleanup()
    {
        probe::disable();
        main_->onF5 = nullptr;
        qDeleteAll(main_->findChildren<DialogFrame*>(Qt::FindDirectChildrenOnly));
        QCoreApplication::processEvents();
    }

    void cleanupTestCase()
    {
        main_.reset();
        translator_.reset();
        icons_.reset();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        sched::waitIdle();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    // Tiêu chí 1.
    void firstFrame()
    {
        DialogHost host(main_.get(), *store_);
        host.add(u"sample"_s, [](QWidget* p) { return new Sample(p); });
        QPointer<DialogFrame> f;
        main_->onF5 = [&] { f = host.open(u"sample"_s); };
        probe::enable();
        probe::reset();
        constexpr int kOpens = 20;
        for (int i = 0; i < kOpens; ++i) {
            QTest::keyClick(main_->windowHandle(), Qt::Key_F5); // phím tới cửa sổ chính, dòng lệnh mở hộp thoại
            QVERIFY(f);
            QVERIFY(spinUntil([&] { return probeCount("dialog_open") > i; }));
            f->reject();
            QVERIFY(spinUntil([&] { return !f; })); // WA_DeleteOnClose của DialogHost
        }
        const QJsonObject j = probe::toJson();
        const QJsonValue op = j["ops"_L1]["dialog_open"_L1];
        const double p50 = op["p50_ms"_L1].toDouble(), p99 = op["p99_ms"_L1].toDouble();
        qInfo("dialog_open (phím → hết khung đầu, %d lần): p50 %.2f ms, p99 %.2f ms, max %.2f ms (ngưỡng %.0f ms)",
              op["count"_L1].toInt(), p50, p99, op["max_ms"_L1].toDouble(), kOpenBudgetMs);
        QCOMPARE(op["count"_L1].toInt(), kOpens);
        QVERIFY2(!kTimed || p99 <= kOpenBudgetMs, qPrintable(QString::number(p99)));
    }

    void lazyBody()
    {
        probe::enable();
        probe::reset();
        std::promise<int> gate;
        const std::shared_future<int> ready = gate.get_future().share();
        int built = 0;
        DialogFrame* f = plainFrame(u"lazy"_s, main_.get());
        f->loadBody(
            [ready](sched::CancelToken) { return ready.wait_for(10s) == std::future_status::ready ? ready.get() : -1; },
            [&built](int v) {
                ++built;
                return new QLabel(QString::number(v));
            });
        QVERIFY(f->isLoading());
        QVERIFY(!f->confirmButton()->isEnabled());
        f->show();
        // Khung đầu (đầu, chân, "Đang tải…") hiện trong khi việc nền còn chặn: luồng UI không chờ dữ liệu.
        QVERIFY(spinUntil([] { return probeCount("dialog_open") == 1; }));
        QVERIFY(f->isLoading());
        QCOMPARE(built, 0);
        auto* text = f->body()->findChild<QLabel*>();
        QVERIFY(text && text->isVisible() && !text->text().isEmpty());
        qInfo("đang tải: %zu lần luồng UI khựng ≥ 50 ms (chỉ báo: máy dùng chung)", probe::stalls().size());

        gate.set_value(42);
        QVERIFY(spinUntil([&] { return !f->isLoading(); }));
        QCOMPARE(built, 1);
        auto* label = qobject_cast<QLabel*>(f->body());
        QVERIFY(label && label->text() == u"42"_s);
        QVERIFY(f->confirmButton()->isEnabled());

        // Đóng khi đang tải: token hủy, kết quả bỏ.
        std::promise<int> gate2;
        const std::shared_future<int> ready2 = gate2.get_future().share();
        auto sawCancel = std::make_shared<std::atomic<bool>>(false);
        int built2 = 0;
        QPointer<DialogFrame> g = plainFrame(u"lazy2"_s, main_.get());
        g->loadBody(
            [ready2, sawCancel](sched::CancelToken token) {
                const int v = ready2.wait_for(10s) == std::future_status::ready ? ready2.get() : -1;
                sawCancel->store(token.cancelled());
                return v;
            },
            [&built2](int) {
                ++built2;
                return new QWidget;
            });
        g->show();
        g->reject();
        gate2.set_value(7);
        sched::waitIdle();
        QCoreApplication::processEvents();
        QVERIFY(sawCancel->load());
        QCOMPARE(built2, 0);
        QVERIFY(g && g->isLoading());
    }

    // Thân "Đang tải…" dùng vòng quay chung ui::Spinner (app/widgets/progress): quay khi hiện, dừng khi giảm chuyển
    // động hoặc khi thân mới thay thân đang tải.
    void loadingSpinner()
    {
        DialogFrame* f = plainFrame(u"spin"_s, main_.get());
        f->setLoading();
        f->show();
        const QPointer<Spinner> spin = f->body()->findChild<Spinner*>();
        QVERIFY(spin && spin->isVisible());
        QVERIFY(spin->spinning());
        tm_->setReducedMotion(true);
        QVERIFY(!spin->spinning());
        tm_->setReducedMotion(false);
        f->setBody(new QLabel(sampleTr(kTitle)));
        QVERIFY(!spin); // thân đang tải xóa ngay, cùng vòng quay
    }

    // Tiêu chí 2.
    void enterEsc()
    {
        DialogHost host(main_.get(), *store_);
        host.add(u"sample"_s, [](QWidget* p) { return new Sample(p); });
        QPointer<Sample> f = static_cast<Sample*>(host.open(u"sample"_s));
        QVERIFY(waitActive(f));
        QCOMPARE(QApplication::focusWidget(), f->line); // focus đầu: ô nhập đầu tiên của thân
        QSignalSpy accepted(f.data(), &QDialog::accepted);
        QSignalSpy help(f.data(), &DialogFrame::helpRequested);

        // Ô nhiều dòng giữ Enter để xuống dòng.
        f->plain->setFocus();
        QTest::keyClick(f->plain, Qt::Key_Return);
        QCOMPARE(accepted.count(), 0);
        QVERIFY(f->plain->toPlainText().contains(u'\n'));
        // Nút xác nhận vô hiệu: Enter không làm gì.
        f->confirmButton()->setEnabled(false);
        QTest::keyClick(f->line, Qt::Key_Return);
        QCOMPARE(accepted.count(), 0);
        QVERIFY(f->isVisible());
        f->confirmButton()->setEnabled(true);
        // Focus đi qua nút Trợ giúp rồi về ô nhập: nút mặc định vẫn là nút xác nhận.
        headerButton(f, u"help"_s)->setFocus();
        f->line->setFocus();
        QTest::keyClick(f->line, Qt::Key_Enter, Qt::KeypadModifier);
        QCOMPARE(help.count(), 0);
        QCOMPARE(accepted.count(), 1);
        QTRY_VERIFY(!f);

        // Esc hủy.
        f = static_cast<Sample*>(host.open(u"sample"_s));
        QVERIFY(waitActive(f));
        QSignalSpy rejected(f.data(), &QDialog::rejected);
        QTest::keyClick(f->line, Qt::Key_Escape);
        QCOMPARE(rejected.count(), 1);
        QTRY_VERIFY(!f);
    }

    void helpKey()
    {
        QShortcut global(QKeySequence(Qt::Key_F1), main_.get()); // phím tắt HELP toàn cục (keymap scope global)
        global.setContext(Qt::ApplicationShortcut);
        QSignalSpy globalHelp(&global, &QShortcut::activated);
        auto* f = new Sample(main_.get());
        f->open();
        QVERIFY(waitActive(f));
        QSignalSpy help(f, &DialogFrame::helpRequested);
        QTest::keyClick(f->line, Qt::Key_F1);
        QCOMPARE(help.count(), 1);
        QCOMPARE(help.last().at(0).toString(), u"SAMPLE"_s);
        QCOMPARE(globalHelp.count(), 0);
        Button* helpButton = headerButton(f, u"help"_s);
        QVERIFY(helpButton && helpButton->isVisible() && !helpButton->accessibleName().isEmpty());
        QVERIFY(!headerButton(f, u"x"_s)->accessibleName().isEmpty());
        QTest::mouseClick(helpButton, Qt::LeftButton);
        QCOMPARE(help.count(), 2);
        // Không tên lệnh: không nút Trợ giúp, F1 dùng mã hộp thoại.
        f->setCommand({});
        QVERIFY(!helpButton->isVisible());
        QTest::keyClick(f->line, Qt::Key_F1);
        QCOMPARE(help.last().at(0).toString(), u"sample"_s);
        f->reject();
        QVERIFY(activateMain());
        QTest::keyClick(main_->cmd, Qt::Key_F1); // ngoài hộp thoại phím tắt toàn cục vẫn chạy
        QCOMPARE(globalHelp.count(), 1);
    }

    void tabCycle()
    {
        auto* f = new Sample(main_.get());
        f->open();
        QVERIFY(waitActive(f));
        QWidget* start = QApplication::focusWidget();
        QCOMPARE(start, f->line);
        // Điểm dừng Tab: Trợ giúp, Đóng, ô một dòng, ô nhiều dòng (Tab không chèn ký tự), hộp kiểm, Hủy, xác nhận.
        constexpr int kStops = 7;
        for (Qt::KeyboardModifiers mods : {Qt::KeyboardModifiers(), Qt::KeyboardModifiers(Qt::ShiftModifier)}) {
            QSet<QWidget*> seen;
            for (int i = 0; i < kStops; ++i) {
                QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab, mods);
                QWidget* w = QApplication::focusWidget();
                QVERIFY(w && w->window() == f);
                seen.insert(w);
            }
            QCOMPARE(QApplication::focusWidget(), start);
            QCOMPARE(seen.size(), kStops);
        }
        QVERIFY(f->plain->toPlainText().isEmpty());
    }

    void focusReturn()
    {
        DialogHost host(main_.get(), *store_);
        host.add(u"a"_s, [](QWidget* p) { return new Sample(p, u"a"_s); });
        host.add(u"b"_s, [](QWidget* p) { return new Sample(p, u"b"_s); });
        QCOMPARE(QApplication::focusWidget(), main_->cmd);
        QPointer<Sample> a = static_cast<Sample*>(host.open(u"a"_s));
        QCOMPARE(a->parentWidget(), main_.get());
        BlockWatch watchA(a);
        QVERIFY(waitActive(a));
        a->plain->setFocus();
        // Xếp chồng: modal mở từ hộp thoại đang active là con của nó và chặn nó.
        QPointer<Sample> b = static_cast<Sample*>(host.open(u"b"_s));
        QCOMPARE(b->parentWidget(), a.data());
        QVERIFY(waitActive(b));
        QTRY_VERIFY(watchA.blocked);
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
        QTRY_VERIFY(!b);
        QTRY_COMPARE(QApplication::focusWidget(), a->plain);
        QVERIFY(!watchA.blocked);
        QTest::keyClick(a->plain, Qt::Key_Escape);
        QTRY_VERIFY(!a);
        QTRY_COMPARE(QApplication::focusWidget(), main_->cmd);
    }

    // Tiêu chí 3.
    void modelessModal()
    {
        DialogHost host(main_.get(), *store_);
        int made = 0;
        host.add(u"tool"_s, [&made](QWidget* p) {
            ++made;
            return new Sample(p, u"tool"_s, false);
        });
        host.add(u"modal"_s, [](QWidget* p) { return new Sample(p, u"modal"_s, true); });
        const QPoint onCanvas = main_->canvas->geometry().center();

        QPointer<DialogFrame> f = host.open(u"tool"_s);
        QVERIFY(waitActive(f));
        QVERIFY(!QApplication::activeModalWidget());
        QTest::keyClick(main_->windowHandle(), Qt::Key_A);
        QCOMPARE(main_->cmd->text(), u"a"_s);
        const int presses = main_->canvas->presses;
        QTest::mouseClick(main_->windowHandle(), Qt::LeftButton, {}, onCanvas);
        QCOMPARE(main_->canvas->presses, presses + 1);

        // Mở lại cùng mã: cùng bản, không dựng lại, được đưa lên.
        QVERIFY(activateMain());
        QCOMPARE(host.open(u"tool"_s), f.data());
        QCOMPARE(made, 1);
        QCOMPARE(host.find(u"tool"_s), f.data());
        QVERIFY(waitActive(f));
        int copies = 0;
        for (QWidget* w : QApplication::topLevelWidgets())
            if (auto* d = qobject_cast<DialogFrame*>(w); d && d->id() == u"tool"_s)
                ++copies;
        QCOMPARE(copies, 1);
        f->reject();
        QTRY_VERIFY(!f);
        QCOMPARE(host.find(u"tool"_s), nullptr);

        // Modal: cửa sổ chính không nhận phím, chuột.
        QPointer<DialogFrame> m = host.open(u"modal"_s);
        QVERIFY(waitActive(m));
        QCOMPARE(QApplication::activeModalWidget(), m.data());
        QTest::keyClick(main_->windowHandle(), Qt::Key_B);
        QCOMPARE(main_->cmd->text(), u"a"_s);
        QTest::mouseClick(main_->windowHandle(), Qt::LeftButton, {}, onCanvas);
        QCOMPARE(main_->canvas->presses, presses + 1);
        m->reject();
        QTRY_VERIFY(!m);
        QCOMPARE(host.open(u"none"_s), nullptr); // mã chưa đăng ký
    }

    // Tiêu chí 4.
    void geometry()
    {
        DialogHost host(main_.get(), *store_);
        host.add(u"big"_s, [](QWidget* p) {
            auto* s = new Sample(p, u"big"_s, false);
            s->setResizable(true);
            return s;
        });
        host.add(u"fixed"_s, [](QWidget* p) { return new Sample(p, u"fixed"_s, false); });

        QPointer<DialogFrame> f = host.open(u"big"_s);
        QVERIFY(QTest::qWaitForWindowExposed(f));
        const QRect want(60, 50, 420, 330);
        f->setCardGeometry(want);
        QCOMPARE(f->cardGeometry(), want);
        f->reject();
        QTRY_VERIFY(!f);
        QCOMPARE(store_->value(u"dialog/big"_s).toObject()[u"w"_s].toInt(), want.width());
        f = host.open(u"big"_s);
        QVERIFY(QTest::qWaitForWindowExposed(f));
        QCOMPARE(f->cardGeometry(), want);
        f->reject();
        QTRY_VERIFY(!f);

        // Cỡ cố định: vị trí khôi phục, cỡ theo nội dung.
        f = host.open(u"fixed"_s);
        QVERIFY(QTest::qWaitForWindowExposed(f));
        const QSize natural = f->cardGeometry().size();
        f->setCardGeometry(QRect(QPoint(70, 40), natural + QSize(50, 30)));
        f->reject();
        QTRY_VERIFY(!f);
        f = host.open(u"fixed"_s);
        QVERIFY(QTest::qWaitForWindowExposed(f));
        QCOMPARE(f->cardGeometry(), QRect(QPoint(70, 40), natural));
    }

    // Còn mở khi DialogHost hủy (thoát ứng dụng): vẫn lưu.
    void geometrySavedOnExit()
    {
        QVERIFY(store_->value(u"dialog/fixed"_s).isUndefined()); // init() xóa
        {
            DialogHost host(main_.get(), *store_);
            host.add(u"fixed"_s, [](QWidget* p) { return new Sample(p, u"fixed"_s, false); });
            DialogFrame* f = host.open(u"fixed"_s);
            QVERIFY(QTest::qWaitForWindowExposed(f));
            f->setCardGeometry(QRect(QPoint(90, 60), f->cardGeometry().size()));
        }
        QCOMPARE(store_->value(u"dialog/fixed"_s).toObject()[u"x"_s].toInt(), 90);
    }

    void place_data()
    {
        QTest::addColumn<QRect>("saved");
        QTest::addColumn<QRect>("anchor");
        QTest::addColumn<QList<QRect>>("screens");
        QTest::addColumn<QRect>("expected");
        const QRect a(0, 0, 1920, 1040), b(1920, 0, 1920, 1040), left(-1920, 0, 1920, 1040);
        const QRect onA(200, 100, 1000, 700); // cửa sổ chính, tâm (699, 449)
        QTest::newRow("cùng màn hình") << QRect(100, 100, 400, 300) << onA << QList{a} << QRect(100, 100, 400, 300);
        QTest::newRow("màn hình thứ hai còn cắm")
            << QRect(2500, 200, 400, 300) << onA << QList{a, b} << QRect(2500, 200, 400, 300);
        QTest::newRow("màn hình đã rút") << QRect(2500, 200, 400, 300) << onA << QList{a} << QRect(500, 300, 400, 300);
        QTest::newRow("rút màn hình trái, cửa sổ chính ở màn hình phải")
            << QRect(-1500, 200, 400, 300) << QRect(2000, 100, 1200, 800) << QList{a, b} << QRect(2400, 350, 400, 300);
        QTest::newRow("tràn mép: dời vào")
            << QRect(1700, 900, 400, 300) << onA << QList{a} << QRect(1520, 740, 400, 300);
        QTest::newRow("lớn hơn màn hình: thu")
            << QRect(0, 0, 1600, 900) << onA << QList{QRect(0, 0, 1366, 728)} << QRect(0, 0, 1366, 728);
        QTest::newRow("màn hình tọa độ âm")
            << QRect(-1000, 100, 400, 300) << onA << QList{left, a} << QRect(-1000, 100, 400, 300);
        QTest::newRow("không có màn hình") << QRect(5, 5, 40, 30) << onA << QList<QRect>{} << QRect(5, 5, 40, 30);
    }

    void place()
    {
        QFETCH(QRect, saved);
        QFETCH(QRect, anchor);
        QFETCH(QList<QRect>, screens);
        QFETCH(QRect, expected);
        QCOMPARE(DialogHost::place(saved, anchor, screens), expected);
    }

    void dragMoveResize()
    {
        auto* f = new Sample(main_.get(), u"drag"_s, false);
        f->setResizable(true);
        f->show();
        QVERIFY(QTest::qWaitForWindowExposed(f));
        const QRect before = f->cardGeometry();
        QWidget* header = f->findChild<QLabel*>()->parentWidget(); // nhãn đầu tiên là tiêu đề, nằm trong đầu
        QVERIFY(header && header->parentWidget() == f);
        QTest::mousePress(header, Qt::LeftButton, {}, QPoint(40, 10));
        QTest::mouseMove(header, QPoint(70, 30));
        QTest::mouseRelease(header, Qt::LeftButton, {}, QPoint(70, 30));
        QCOMPARE(f->cardGeometry(), before.translated(30, 20));

        const QRect card = f->cardGeometry().translated(-f->geometry().topLeft()); // tọa độ của frame
        const QPoint grab(card.right() + 3, card.center().y());                    // dải mép phải, ngoài thẻ
        QTest::mousePress(f, Qt::LeftButton, {}, grab);
        QTest::mouseMove(f, grab + QPoint(40, 0));
        QTest::mouseRelease(f, Qt::LeftButton, {}, grab + QPoint(40, 0));
        QCOMPARE(f->cardGeometry(), before.translated(30, 20).adjusted(0, 0, 40, 0));
    }

    // Tiêu chí 5.
    void confirmLabels()
    {
        DialogHost good(main_.get(), *store_);
        good.add(u"sample"_s, [](QWidget* p) { return new Sample(p); });
        good.add(u"plain"_s, [](QWidget* p) { return new DialogFrame(u"plain"_s, p); }); // không nút xác nhận
        QCOMPARE(good.ids(), (QStringList{u"plain"_s, u"sample"_s}));
        QCOMPARE(good.confirmLabelIssues(), QStringList());
        QVERIFY(translator_->setLanguage(u"en"_s));
        QCOMPARE(good.confirmLabelIssues(), QStringList());
        FakeEnglish fake;
        QCoreApplication::installTranslator(&fake);
        const QStringList english = good.confirmLabelIssues();
        QCoreApplication::removeTranslator(&fake);
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QCOMPARE(english, QStringList{u"sample: «OK»"_s});

        static constexpr char kOk[] = "&Ok"; // i18n-ok: nhãn sai mà kiểm tra phải bắt
        DialogHost bad(main_.get(), *store_);
        bad.add(u"bad"_s, [](QWidget* p) { return new Sample(p, u"bad"_s, true, kOk); });
        QCOMPARE(bad.confirmLabelIssues(), QStringList{u"bad: «&Ok»"_s});
    }

    // Tiêu chí 6 (DoD-U 1): ảnh khung theo chủ đề × DPR.
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
        Sample f(main_.get(), u"shot"_s, false);
        f.setAttribute(Qt::WA_DontShowOnScreen); // không active, không focus thật: ảnh tất định
        f.show();
        f.resize(f.sizeHint());
        QImage shot(f.size() * dpr, QImage::Format_ARGB32_Premultiplied);
        shot.setDevicePixelRatio(dpr);
        shot.fill(tm_->color(tok::c_canvas)); // nền vùng vẽ dưới hộp thoại
        f.render(&shot);
        const QString name = u"frame-%1-%2-%3.png"_s.arg(light ? u"light"_s : u"dark"_s)
                                 .arg(qRound(dpr * 100))
                                 .arg(QLatin1StringView(kPlatform));
        const QString out = qEnvironmentVariable("TRION_UPDATE_BASELINE");
        if (!out.isEmpty()) {
            QVERIFY(shot.save(out + u'/' + name, "PNG", 0)); // nén cao nhất
            QSKIP("đã ghi baseline");
        }
        const QImage want = QImage(u":/baseline/"_s + name).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QVERIFY2(!want.isNull(), qPrintable(u"thiếu baseline "_s + name));
        QCOMPARE(shot.size(), want.size());
        qsizetype off = 0;
        int maxDiff = 0;
        for (int y = 0; y < shot.height(); ++y) {
            const auto* a = reinterpret_cast<const QRgb*>(shot.constScanLine(y));
            const auto* b = reinterpret_cast<const QRgb*>(want.constScanLine(y));
            for (int x = 0; x < shot.width(); ++x) {
                const int d = std::max({std::abs(qRed(a[x]) - qRed(b[x])), std::abs(qGreen(a[x]) - qGreen(b[x])),
                                        std::abs(qBlue(a[x]) - qBlue(b[x])), std::abs(qAlpha(a[x]) - qAlpha(b[x]))});
                maxDiff = std::max(maxDiff, d);
                off += d > kNoise;
            }
        }
        qInfo("%s: %lld điểm ảnh lệch > %d/255, lệch lớn nhất %d/255", qPrintable(name), qint64(off), kNoise, maxDiff);
        if (off > shot.width() * shot.height() / 1000)
            shot.save(QDir::temp().filePath(u"got-"_s + name)); // để so bằng mắt
        QVERIFY(off <= shot.width() * shot.height() / 1000);
    }

    // Giao diện nhẹ: bóng thành viền 1 px màu token ngay ngoài thẻ, lề bóng thu lại; thẻ đứng yên.
    void lite()
    {
        auto* f = new Sample(main_.get(), u"lite"_s, false);
        f->show();
        QVERIFY(QTest::qWaitForWindowExposed(f));
        const QMargins normal = f->contentsMargins();
        const QRect card = f->cardGeometry();
        tm_->setLite(true);
        QCOMPARE(f->contentsMargins(), QMargins(2, 2, 2, 2)); // viền bóng 1 px + viền thẻ 1 px
        QCOMPARE(f->cardGeometry(), card);
        QImage shot(f->size(), QImage::Format_ARGB32_Premultiplied);
        shot.fill(Qt::transparent);
        f->render(&shot);
        const tok::Shadow s = tm_->shadow(tok::sh_dlg);
        const int y = shot.height() / 2;
        QCOMPARE(shot.pixelColor(0, y), QColor(s.color.r, s.color.g, s.color.b, s.color.a));
        QCOMPARE(shot.pixelColor(1, y), tm_->color(tok::c_border_ctl));
        tm_->setLite(false);
        QCOMPARE(f->contentsMargins(), normal);
        QCOMPARE(f->cardGeometry(), card);
    }

    void pseudoNoTruncation()
    {
        auto* f = new Sample(main_.get(), u"xl"_s, false);
        DialogFrame* g = plainFrame(u"xl-loading"_s, main_.get());
        g->setLoading();
        for (DialogFrame* s : {static_cast<DialogFrame*>(f), g}) {
            s->show();
            QVERIFY(QTest::qWaitForWindowExposed(s));
            QCOMPARE(truncations(s), QString());
        }
        const QString vi = f->windowTitle();
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QTRY_VERIFY(f->windowTitle().size() >= vi.size() + 2);
        QTRY_COMPARE(truncations(f), QString());
        QTRY_COMPARE(truncations(g), QString());
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QTRY_COMPARE(f->windowTitle(), vi);
    }

    void pickHide()
    {
        DialogHost host(main_.get(), *store_);
        host.add(u"pick"_s, [](QWidget* p) { return new Sample(p, u"pick"_s, true); });
        QPointer<Sample> f = static_cast<Sample*>(host.open(u"pick"_s));
        QVERIFY(waitActive(f));
        f->plain->setFocus();
        // Thân cao thêm lúc đang mở (như dòng lỗi của ô số hiện ra): hộp thoại tự lớn theo bố cục, chưa từng
        // move/resize tường minh, nên show() của QDialog từng co lại và canh giữa lại (integration nhóm 4 bắt).
        const int h0 = f->height();
        auto* extra = new QWidget;
        extra->setFixedHeight(40);
        f->plain->parentWidget()->layout()->addWidget(extra);
        QTRY_VERIFY(f->height() >= h0 + 40);
        QSignalSpy finished(f.data(), &QDialog::finished);
        const QRect card = f->cardGeometry();
        const QRect frame = f->geometry();
        f->hideForPick(true);
        QVERIFY(!f->isVisible() && f->isHiddenForPick());
        QVERIFY(!QApplication::activeModalWidget());
        QVERIFY(waitActive(main_.get()));
        QTest::keyClick(main_->windowHandle(), Qt::Key_C); // cửa sổ chính dùng được trong lúc lấy từ bản vẽ
        QCOMPARE(main_->cmd->text(), u"c"_s);
        QCOMPARE(host.open(u"pick"_s), f.data()); // vẫn là bản đang mở, không hiện ra giữa chừng
        QVERIFY(!f->isVisible());
        f->hideForPick(false);
        QVERIFY(f->isVisible() && !f->isHiddenForPick());
        QCOMPARE(f->cardGeometry(), card);
        QCOMPARE(f->geometry(), frame);
        QVERIFY(waitActive(f));
        QTRY_COMPARE(QApplication::focusWidget(), f->plain);
        QCOMPARE(QApplication::activeModalWidget(), f.data());
        QCOMPARE(finished.count(), 0);
        f->reject();
        QTRY_VERIFY(!f);
        QCOMPARE(finished.count(), 1);
    }
};

QTEST_MAIN(DialogTest)
#include "dialog_test.moc"
