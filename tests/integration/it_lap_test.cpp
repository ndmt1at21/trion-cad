// Integration test nhóm 7 (lắp đợt 7 vào cửa sổ chính): UIA-007 (dòng lệnh: lệnh khóa theo phiên bản, F2/TEXTSCR,
// INPUTSEARCHOPTIONS, dạng nổi), UIA-003 (menu ứng dụng), UIA-118 (palette Layer của Viewer, .tcview), UIA-101 (QAT
// dưới ribbon), tồn đọng nhóm 6 (toast RIBBONCLOSE, RIBBONSTATE và mức thu gọn qua lần chạy, raster trước biểu tượng
// sau khung đầu) — dựng bằng chính Application của exe trion như it_vo_test (kịch bản nhóm 5, 6 ở đó; tệp riêng để mỗi
// bộ < 60 s ở mọi preset). "Dựng lại" = hủy Application rồi dựng lại với cùng tệp cài đặt (như thoát rồi chạy lại).
//   1 lockedCommand      Viewer: gõ LINE → lịch sử có thông báo khóa, router không nhận; chip → UpgradeRouter
//   2 historyWindow      F2 mở lịch sử đủ dòng, mở lần nữa đưa bản đang mở lên; INPUTSEARCHOPTIONS; Esc, focus
//   3 floatingCommandLine floatRequested/dockRequested; trang Start ↔ vùng vẽ giữ trên cùng; Clean Screen, ẩn/hiện;
//                        dựng lại: nổi đúng chỗ
//   4 appMenu            nút logo mở, Esc đóng, focus; Mở → OPEN; tệp gần đây → requestOpen; Viewer không Mới/Lưu
//   5 viewerLayers       palette "layers" ở Viewer, F6; bật/tắt → '_-LAYER; .tcview lưu (Viewer Pro), nạp; Viewer →
//                        trang nâng cấp
//   6 qatBelowRibbon     bật trong menu tùy biến → hàng dưới ribbon, Tab sau ribbon, audit; tắt; giữ qua dựng lại
//   7 ribbonState        RIBBONCLOSE toast một lần; dựng lại giữ RIBBONSTATE, mức thu gọn
//   8 screens            1920×1080, 1366×768 vi-XL: audit, không cắt chữ (cả menu ứng dụng, lịch sử đang mở)
//   9 idle               dựng lại: raster trước nền không khựng luồng UI > 2 ms; rảnh 2 s không timer

#include "app/core/a11y/AccessibilityAudit.h"
#include "app/core/actions/ActionRegistry.h"
#include "app/core/display/DisplayContext.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/perf/UiProbe.h"
#include "app/main/Application.h"
#include "app/main/ShellAssembly.h"
#include "app/main/StartupTrace.h"
#include "app/shell/appmenu/AppMenu.h"
#include "app/shell/CleanScreen.h"
#include "app/shell/cmdline/CommandLine.h"
#include "app/shell/cmdline/HistoryWindow.h"
#include "app/shell/CommandRouter.h"
#include "app/shell/filetabs/FileTabBar.h"
#include "app/shell/MainWindow.h"
#include "app/shell/ribbon/RibbonBar.h"
#include "app/shell/titlebar/Qat.h"
#include "app/shell/titlebar/TitleBar.h"
#include "app/shell/viewer/ViewerLayers.h"
#include "app/shell/viewer/ViewState.h"
#include "app/theme/icons/IconProvider.h"
#include "app/widgets/cmdline/CommandInput.h"
#include "app/widgets/dock/PaletteHost.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "app/widgets/views/VirtualTableView.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"
#include "tests/timing.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScreen>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <chrono>
#include <functional>
#include <memory>
#include <utility>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;
using trion::app::Application;
using trion::app::ShellAssembly;
using trion::app::StartupTrace;
using trion::edition::Edition;
using trion::shell::MainWindow;
using Region = trion::shell::MainWindow::Region;
namespace i18n = trion::i18n;
namespace sched = trion::sched;
namespace shell = trion::shell;
namespace ui = trion::ui;

namespace {

// ponytail: các hàm chờ, gõ phím, màn hình giả chép từ it_vo_test (kịch bản nhóm 5, 6); gom vào header chung khi đụng
// tới it_vo_test lần sau.
QString g_cwd;
constexpr QRect kWide(0, 0, 1920, 1080);
constexpr QRect kSmall(2000, 0, 1366, 768);

bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 20s)
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
    limit.start(timeout);
    tick.start(1);
    loop.exec();
    return cond();
}

void spinFor(std::chrono::milliseconds d)
{
    spinUntil([] { return false; }, d);
}

void settle()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
}

bool focusOn(QWidget* w)
{
    w->window()->activateWindow();
    if (!spinUntil([w] { return QGuiApplication::focusWindow() == w->window()->windowHandle(); }))
        return false;
    w->setFocus(Qt::OtherFocusReason);
    QWidget* to = w;
    while (to->focusProxy())
        to = to->focusProxy();
    return spinUntil([to] { return QApplication::focusWidget() == to; });
}

void type(QWidget* field, const QString& text)
{
    for (const QChar ch : text) {
        QWidget* to = QApplication::activePopupWidget() ? QApplication::activePopupWidget() : field;
        const auto key = ch.unicode() < 0x80 ? Qt::Key(ch.toUpper().unicode()) : Qt::Key_unknown;
        QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, QString(ch));
        QCoreApplication::sendEvent(to, &press);
        QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, QString(ch));
        QCoreApplication::sendEvent(to, &release);
    }
}

void key(Qt::Key k, Qt::KeyboardModifiers m = Qt::NoModifier)
{
    QWidget* to = QApplication::activePopupWidget() ? QApplication::activePopupWidget() : QApplication::focusWidget();
    QVERIFY(to);
    QTest::keyClick(to, k, m);
}

QString truncations(QWidget* root)
{
    QStringList out;
    for (const i18n::Truncation& t : i18n::findTruncations(root))
        out << t.toString();
    return out.join(u"; "_s);
}

QStringList timersDuring(std::chrono::milliseconds d)
{
    struct Log final : QObject {
        const QObject* ignore = nullptr;
        QStringList seen;
        bool eventFilter(QObject* watched, QEvent* e) override
        {
            if (e->type() == QEvent::Timer && watched != ignore)
                seen << QString::fromLatin1(watched->metaObject()->className());
            return false;
        }
    } log;
    QTimer end;
    end.setSingleShot(true);
    log.ignore = &end;
    qApp->installEventFilter(&log);
    QEventLoop loop;
    QObject::connect(&end, &QTimer::timeout, &loop, &QEventLoop::quit);
    end.start(d);
    loop.exec();
    qApp->removeEventFilter(&log);
    return log.seen;
}

// Các widget trong chuỗi focus của cửa sổ, bắt đầu từ cửa sổ.
QList<QWidget*> focusChain(QWidget* window)
{
    QList<QWidget*> out;
    for (QWidget* w = window->nextInFocusChain(); w != window; w = w->nextInFocusChain())
        out << w;
    return out;
}

} // namespace

class IntegrationLap : public QObject {
    Q_OBJECT
public:
    IntegrationLap() { QDir::setCurrent(g_cwd); }

    static void initMain()
    {
        for (const char* v : {"QT_ENABLE_HIGHDPI_SCALING", "QT_SCALE_FACTOR", "QT_SCREEN_SCALE_FACTORS",
                              "QT_SCALE_FACTOR_ROUNDING_POLICY"})
            qunsetenv(v);
        ui::DisplayContext::initMain();
        QJsonArray list;
        for (const auto& [name, r] : {std::pair{"p1920", kWide}, std::pair{"s1366", kSmall}})
            list.append(QJsonObject{{u"name"_s, QLatin1StringView(name)}, {u"x"_s, r.x()}, {u"y"_s, r.y()},
                                    {u"width"_s, r.width()}, {u"height"_s, r.height()}, {u"logicalDpi"_s, 96},
                                    {u"logicalBaseDpi"_s, 96}, {u"dpr"_s, 1}});
        static QTemporaryDir config;
        g_cwd = QDir::currentPath();
        QFile f(config.filePath(u"screens.json"_s));
        const QByteArray json = QJsonDocument(QJsonObject{{u"screens"_s, list}}).toJson();
        if (!config.isValid() || !f.open(QIODevice::WriteOnly) || f.write(json) != json.size() ||
            !QDir::setCurrent(config.path()))
            qFatal("không ghi được cấu hình màn hình");
        qputenv("QT_QPA_PLATFORM", "offscreen:configfile=screens.json");
    }

private:
    std::unique_ptr<QTemporaryDir> dir_;
    trion::app::Options options_;
    std::unique_ptr<StartupTrace> trace_;
    std::unique_ptr<Application> app_;
    ShellAssembly* shell_ = nullptr;
    MainWindow* window_ = nullptr;
    QString recent_;           // tệp gần đây có sẵn trong Store
    QStringList sent_;         // chuỗi lệnh tới router chưa có xử lý
    QList<QUrl> opened_;       // trang nâng cấp đã mở (thay QDesktopServices)
    QList<QString> upgrades_;  // UpgradeRouter (handler giả) nhận
    std::function<void()> atFirstFrame_; // trước raster trước của Application (kịch bản idle)

    QStringList take() { return std::exchange(sent_, {}); }
    QWidget* canvas() const { return window_->region(Region::Canvas); }
    ui::CommandLine* cmd() const { return shell_->commandLine; }

    // Như main.cpp: Application dựng mọi phần, cửa sổ phóng to trên màn hình chính, chờ khung đầu.
    bool start(bool pro = true)
    {
        trace_ = std::make_unique<StartupTrace>();
        if (atFirstFrame_) // lượt sau khung đầu, trước lượt raster trước của Application (nối sau)
            connect(trace_.get(), &StartupTrace::firstFrame, this, atFirstFrame_, Qt::QueuedConnection);
        app_ = std::make_unique<Application>(options_, *trace_);
        shell_ = app_->shell();
        window_ = qobject_cast<MainWindow*>(app_->mainWindow());
        if (!shell_ || !window_ || !spinUntil([&] { return trace_->at(u"first_frame"_s).has_value(); }) ||
            !QTest::qWaitForWindowActive(window_))
            return false;
        connect(&shell_->router, &shell::CommandRouter::unhandled, this, [this](const QString& m) { sent_ << m; });
        shell_->gate.upgrade().setUrlOpener([this](const QUrl& url) {
            opened_ << url;
            return true;
        });
        shell_->toasts->setTimeout(1h);
        if (pro)
            shell_->gate.setEdition(Edition::Pro);
        return true;
    }
    // Thoát như exe trion (lưu cài đặt), rồi chạy lại với cùng tệp cài đặt.
    void stop()
    {
        app_.reset();
        trace_.reset();
        sched::waitIdle();
        QCoreApplication::processEvents();
        shell_ = nullptr;
        window_ = nullptr;
    }
    bool restart()
    {
        stop();
        return start();
    }

    void openDocument(quint64 id = 100)
    {
        shell_->fileTabs->addDocument({.id = id,
                                       .name = u"Ban-ve-%1.dwg"_s.arg(id),
                                       .path = dir_->filePath(u"Ban-ve-%1.dwg"_s.arg(id)),
                                       .dirty = false,
                                       .readOnly = false,
                                       .layouts = {}});
        shell_->fileTabs->setCurrent(id);
        settle();
    }
    void closeDocuments()
    {
        for (const quint64 id : shell_->fileTabs->documents())
            shell_->fileTabs->removeDocument(id);
        shell_->fileTabs->setCurrent(shell::FileTabBar::kStart);
    }
    QAbstractButton* logo() const { return shell_->titleBar->findChild<QAbstractButton*>(u"appMenuButton"_s); }
    QAction* qatBelow() const
    {
        const QString label = QCoreApplication::translate("trion::shell::Qat", "Hiện bên dưới ribbon");
        for (QAction* a : shell_->titleBar->qat()->menu()->actions())
            if (a->text() == label)
                return a;
        return nullptr;
    }

private slots:
    void initTestCase()
    {
        QCOMPARE(QGuiApplication::screens().size(), qsizetype(2));
        dir_ = std::make_unique<QTemporaryDir>();
        QVERIFY(dir_->isValid());
        QVERIFY(QDir(dir_->path()).mkpath(u"config"_s));
        qputenv("XDG_CONFIG_HOME", QFile::encodeName(dir_->filePath(u"config"_s)));
        options_.settingsPath = dir_->filePath(u"user.json"_s);
        recent_ = dir_->filePath(u"Nha-pho-A.dwg"_s);
        QFile f(recent_);
        QVERIFY(f.open(QIODevice::WriteOnly) && f.write(QByteArray("AC1032") + QByteArray(122, '\0')) == 128);
        f.close();
        {
            trion::settings::Store seed(options_.settingsPath);
            seed.setValue(u"recent/files"_s, QJsonArray{QJsonObject{{u"path"_s, recent_}}});
            seed.flush();
        }
        QVERIFY(start());
    }

    void cleanupTestCase()
    {
        stop();
        dir_.reset();
    }

    void init()
    {
        shell_->gate.setEdition(Edition::Pro);
        take();
    }

    void cleanup()
    {
        while (QWidget* popup = QApplication::activePopupWidget())
            popup->close();
        for (const QString& id : {u"textscr"_s, u"inputsearchoptions"_s})
            if (ui::DialogFrame* d = shell_->dialogs.find(id))
                d->reject();
        for (ui::Toast* t : shell_->toasts->toasts())
            t->closeButton()->click();
        spinUntil([&] { return shell_->toasts->toasts().isEmpty(); }, 5s);
        if (cmd()->hasPrompt())
            cmd()->clearPrompt();
        cmd()->input()->clear();
        closeDocuments();
        focusOn(canvas());
    }

    // Kịch bản 1 — Viewer: tên lệnh Pro gõ ở dấu nhắc lệnh không chạy; lịch sử có "Lệnh LINE có trong TrionCAD Pro."
    // (2 chip của CommandLine); chip phát upgradeRequested → EditionGate::upgrade().open → trang nâng cấp.
    void lockedCommand()
    {
        shell_->gate.setEdition(Edition::Viewer);
        shell_->gate.upgrade().setHandler([this](const trion::edition::UpgradeRequest& r) { upgrades_ << r.feature; });
        const int lines = cmd()->historyCount();
        QVERIFY(focusOn(cmd()->input()));
        type(cmd()->input(), u"LINE"_s);
        key(Qt::Key_Escape); // đóng danh sách tự hoàn thành, giữ chữ
        key(Qt::Key_Return);
        const QString notice = trion::edition::EditionGate::commandNotice(u"LINE"_s, ui::Edition::Pro).text;
        QVERIFY2(cmd()->historyCount() > lines && cmd()->historyLine(cmd()->historyCount() - 1).contains(notice),
                 qPrintable(cmd()->historyLines().join(u'|')));
        QCOMPARE(take(), QStringList()); // router không nhận LINE
        emit cmd()->upgradeRequested(u"LINE"_s); // chip (bấm chip: cmdline_extras_test)
        QCOMPARE(upgrades_, QList<QString>{u"LINE"_s});
        shell_->gate.upgrade().setHandler({});

        shell_->gate.setEdition(Edition::Pro); // Pro: cùng tên chạy qua router
        QVERIFY(focusOn(cmd()->input()));
        type(cmd()->input(), u"LINE"_s);
        key(Qt::Key_Escape);
        key(Qt::Key_Return);
        QCOMPARE(take(), QStringList{u"LINE"_s});
    }

    // Kịch bản 2 — F2 (QAction TEXTSCR của Keymap "hist") mở lịch sử đủ dòng; F2 lần nữa đưa bản đang mở lên (một bản);
    // INPUTSEARCHOPTIONS mở hộp thiết lập (modal); Esc đóng, focus về chỗ cũ.
    void historyWindow()
    {
        for (int i = 0; i < 30; ++i)
            cmd()->appendHistory(u"Dòng %1"_s.arg(i)); // i18n-ok: chữ mẫu của test
        QVERIFY(focusOn(canvas()));
        QWidget* before = QApplication::focusWidget();
        QTest::keyClick(window_, Qt::Key_F2);
        auto* history = qobject_cast<ui::HistoryWindow*>(shell_->dialogs.find(u"textscr"_s));
        QVERIFY(history && history->isVisible());
        QVERIFY(spinUntil([&] { return history->rowCount() == cmd()->historyLines().size(); }));
        QVERIFY(QTest::qWaitForWindowActive(history));
        window_->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(window_));
        shell_->router.submit(u"'_TEXTSCR"_s); // nút ^ của dòng lệnh
        QCOMPARE(shell_->dialogs.find(u"textscr"_s), history);
        QVERIFY(QTest::qWaitForWindowActive(history));
        QTest::keyClick(history->focusWidget() ? history->focusWidget() : history, Qt::Key_Escape);
        QVERIFY(spinUntil([&] { return !history->isVisible(); }));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); // DialogHost hủy bản đã đóng
        QCOMPARE(shell_->dialogs.find(u"textscr"_s), nullptr);
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == before; }));

        shell_->router.submit(u"^C^C_INPUTSEARCHOPTIONS"_s);
        ui::DialogFrame* options = shell_->dialogs.find(u"inputsearchoptions"_s);
        QVERIFY(options && options->isModal());
        QVERIFY(QTest::qWaitForWindowActive(options));
        QTest::keyClick(options->focusWidget() ? options->focusWidget() : options, Qt::Key_Escape);
        QVERIFY(spinUntil([&] { return !options->isVisible(); }));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(shell_->dialogs.find(u"inputsearchoptions"_s), nullptr);
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == before; }));
        QCOMPARE(take(), QStringList());
    }

    // Kịch bản 3 — dòng lệnh nổi qua tín hiệu kéo của CommandLine: rời cột thành con của vùng vẽ; đổi trang Start ↔
    // vùng vẽ vẫn ở trên cùng; Clean Screen, COMMANDLINEHIDE/COMMANDLINE khi nổi; về cột; dựng lại giữ nổi, chỗ.
    void floatingCommandLine()
    {
        QWidget* stack = canvas();
        emit cmd()->floatRequested(stack->mapToGlobal(QPoint(240, 180)));
        settle();
        QVERIFY(window_->isCommandLineFloating() && cmd()->isFloating());
        QCOMPARE(cmd()->parentWidget(), stack);
        QVERIFY(cmd()->isVisible());
        const QPoint at = cmd()->pos();
        QCOMPARE(at, QPoint(240, 180));
        const auto onTop = [&] {
            const QPoint c = QRect(cmd()->pos(), cmd()->size()).center();
            QWidget* hit = stack->childAt(c);
            return hit && (hit == cmd() || cmd()->isAncestorOf(hit));
        };
        QVERIFY(onTop());
        openDocument(); // trang vùng vẽ lên trên: dòng lệnh vẫn trên cùng
        QVERIFY(onTop());
        closeDocuments();
        settle();
        QVERIFY(onTop());

        shell_->router.submit(u"'_CLEANSCREENON"_s);
        settle();
        QVERIFY(shell_->clean.isOn() && cmd()->isVisible() && cmd()->pos() == at);
        shell_->router.submit(u"'_CLEANSCREENOFF"_s);
        shell_->router.submit(u"COMMANDLINEHIDE"_s);
        QVERIFY(!cmd()->isVisible());
        QCOMPARE(shell_->sysvars.get(u"CLISTATE"_s), 0);
        shell_->router.submit(u"COMMANDLINE"_s);
        QVERIFY(cmd()->isVisible() && cmd()->input()->hasFocus());
        QCOMPARE(shell_->sysvars.get(u"CLISTATE"_s), 1);

        emit cmd()->dockRequested();
        settle();
        QVERIFY(!window_->isCommandLineFloating() && !cmd()->isFloating());
        QCOMPARE(cmd()->parentWidget(), window_->centralWidget());
        emit cmd()->floatRequested(stack->mapToGlobal(at));
        const QRect g = cmd()->geometry();
        QVERIFY(restart());
        settle();
        QVERIFY(window_->isCommandLineFloating() && cmd()->isFloating());
        QCOMPARE(cmd()->parentWidget(), canvas());
        QCOMPARE(cmd()->geometry(), g);
        emit cmd()->dockRequested();
        QVERIFY(!window_->isCommandLineFloating());
    }

    // Kịch bản 4 — menu ứng dụng: nút logo mở, Esc đóng và trả focus; Mở → QAction OPEN; tệp gần đây → requestOpen;
    // Viewer không có Mới, Lưu, Lưu thành.
    void appMenu()
    {
        shell::AppMenu* menu = shell_->appMenu;
        QVERIFY(focusOn(canvas()));
        QWidget* before = QApplication::focusWidget();
        QTest::mouseClick(logo(), Qt::LeftButton); // chuột: nút StrongFocus lấy focus
        QVERIFY(spinUntil([&] { return menu->isVisible(); }));
        key(Qt::Key_Escape);
        QVERIFY(spinUntil([&] { return !menu->isVisible(); }));
        QCOMPARE(logo()->focusPolicy(), Qt::TabFocus); // bấm chuột không lấy focus: Esc trả về vùng vẽ
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == before; }, 2s));

        const QString open = shell_->actions.macro(u"OPEN"_s);
        logo()->click();
        QVERIFY(spinUntil([&] { return menu->isVisible(); }));
        QAbstractItemView* items = menu->items();
        QCOMPARE(items->model()->rowCount(), int(shell::AppMenu::ItemCount));
        QTest::mouseClick(items->viewport(), Qt::LeftButton, {},
                          items->visualRect(items->model()->index(shell::AppMenu::Open, 0)).center());
        QVERIFY(spinUntil([&] { return !menu->isVisible(); }));
        QCOMPARE(take(), QStringList{open});

        QSignalSpy requested(app_.get(), &Application::openRequested);
        logo()->click();
        QVERIFY(spinUntil([&] { return menu->isVisible() && menu->files()->model()->rowCount() == 1; }));
        QAbstractItemView* files = menu->files();
        QTest::mouseClick(files->viewport(), Qt::LeftButton, {},
                          files->visualRect(files->model()->index(0, 0)).center());
        QCOMPARE(requested.count(), 1);
        QCOMPARE(requested[0][0].toStringList(), QStringList{recent_});
        QVERIFY(spinUntil([&] { return !menu->isVisible(); }));

        shell_->gate.setEdition(Edition::Viewer);
        logo()->click();
        QVERIFY(spinUntil([&] { return menu->isVisible(); }));
        QStringList labels;
        for (int r = 0; r < items->model()->rowCount(); ++r)
            labels << items->model()->index(r, 0).data().toString();
        QCOMPARE(int(labels.size()), int(shell::AppMenu::ItemCount) - 3);
        for (const char* gone : {"Mới", "Lưu", "Lưu thành"})
            QVERIFY2(!labels.contains(QCoreApplication::translate("trion::shell::AppMenu", gone)), gone);
        key(Qt::Key_Escape);
    }

    // Kịch bản 5 — Viewer: palette "layers" là ViewerLayers, F6 tới; bật/tắt → '_-LAYER qua router; Viewer Pro lưu
    // .tcview (toast), nạp → '_-LAYER; bản Viewer: lưu mở trang nâng cấp. Hồ sơ đầy đủ: ô giữ chỗ.
    void viewerLayers()
    {
        shell_->gate.setEdition(Edition::ViewerPro);
        trion::ui::PaletteHost& host = window_->palettes();
        QVERIFY(host.ids().contains(u"layers"_s));
        openDocument();
        host.showPalette(u"layers"_s);
        QVERIFY(spinUntil([&] { return !shell_->viewerLayers.isNull() && shell_->viewerLayers->isVisible(); }));
        shell::ViewerLayers* layers = shell_->viewerLayers;
        QVERIFY(focusOn(canvas()));
        bool reached = false;
        for (int i = 0; i < 6 && !reached; ++i) {
            QTest::keyClick(QApplication::focusWidget(), Qt::Key_F6);
            reached = host.dock(u"layers"_s)->isAncestorOf(QApplication::focusWidget());
        }
        QVERIFY2(reached, "F6 không tới palette layers");

        shell::ViewerLayer zero, wall;
        zero.name = u"0"_s;
        wall.name = u"KT-TUONG"_s;
        wall.objects = 12;
        layers->setLayers({zero, wall});
        QVERIFY(spinUntil([&] { return layers->shownCount() == 2; }));
        ui::VirtualTableView* t = layers->table();
        int row = -1;
        for (int r = 0; r < t->model()->rowCount(); ++r)
            if (t->model()->index(r, shell::ViewerLayers::Name).data().toString() == u"KT-TUONG"_s)
                row = r;
        QVERIFY(row >= 0);
        t->setFocus();
        t->setCurrentIndex(t->model()->index(row, shell::ViewerLayers::On));
        QTest::keyClick(t, Qt::Key_Space);
        const QStringList sent = take();
        QVERIFY2(sent.size() == 1 && sent[0].contains(u"-LAYER"_s) && sent[0].contains(u"KT-TUONG"_s),
                 qPrintable(sent.join(u'|')));

        const QString drawing = dir_->filePath(u"Ban-ve-100.dwg"_s);
        QCOMPARE(shell_->documentPath(), drawing);
        emit layers->saveViewRequested();
        QVERIFY(spinUntil([&] { return QFile::exists(shell::viewStatePath(drawing)); }));
        QVERIFY(spinUntil([&] { return !shell_->toasts->toasts().isEmpty(); }));
        QCOMPARE(shell_->toasts->toasts().constLast()->severity(), ui::Severity::Ok);

        shell::ViewState state;
        state.layout = u"Model"_s;
        shell::LayerFlags off;
        off.name = u"KT-TUONG"_s;
        off.on = false;
        state.layers = {off};
        QVERIFY(bool(shell::writeViewState(drawing, state)));
        shell_->viewStates->load(drawing);
        QVERIFY(spinUntil([&] { return !sent_.isEmpty(); }));
        const QStringList loaded = take();
        QVERIFY2(loaded.size() == 1 && loaded[0].contains(u"-LAYER"_s) && loaded[0].contains(u"KT-TUONG"_s),
                 qPrintable(loaded.join(u'|')));

        shell_->gate.setEdition(Edition::Viewer);
        opened_.clear();
        emit layers->saveViewRequested();
        QCOMPARE(opened_.size(), 1); // trang nâng cấp Viewer Pro thay vì ghi

        shell_->gate.setEdition(Edition::Pro); // hồ sơ đầy đủ: ô giữ chỗ (UIC-009 chưa có)
        QVERIFY(!layers->isVisible());
    }

    // Kịch bản 6 — "Hiện bên dưới ribbon": QAT vào hàng dưới ribbon, Tab sau ribbon và trước tab tệp, audit 0 vi phạm;
    // giữ qua dựng lại; tắt → về thanh tiêu đề sau nút logo.
    void qatBelowRibbon()
    {
        shell::Qat* qat = shell_->titleBar->qat();
        QVERIFY(qatBelow() && !qat->isBelowRibbon());
        qatBelow()->trigger();
        settle();
        const auto checkBelow = [&] {
            QCOMPARE(qat->parentWidget(), window_->belowRibbon());
            QVERIFY(window_->belowRibbon()->isVisible() && qat->isVisible());
            const QList<QWidget*> chain = focusChain(window_);
            const qsizetype first = chain.indexOf(qat->findChild<QAbstractButton*>());
            QVERIFY(first > chain.indexOf(shell_->ribbon->tabBar()));
            QVERIFY(first < chain.indexOf(shell_->fileTabs->tabBar()));
        };
        checkBelow();
        QVERIFY(QTest::qWaitForWindowActive(window_));
        QStringList issues = trion::a11y::audit(window_);
        QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));

        QVERIFY(restart());
        qat = shell_->titleBar->qat();
        QVERIFY(qat->isBelowRibbon());
        settle();
        checkBelow();

        qatBelow()->trigger();
        settle();
        QCOMPARE(qat->parentWidget(), shell_->titleBar);
        QVERIFY(!window_->belowRibbon()->isVisible() && qat->isVisible());
        const QList<QWidget*> chain = focusChain(window_);
        const qsizetype first = chain.indexOf(qat->findChild<QAbstractButton*>());
        QVERIFY(first > chain.indexOf(logo()) && first < chain.indexOf(shell_->ribbon->tabBar()));
        issues = trion::a11y::audit(window_);
        QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
    }

    // Kịch bản 7 — RIBBONCLOSE: toast "Gõ RIBBON để hiện lại" một lần mỗi phiên; dựng lại giữ RIBBONSTATE và mức thu gọn.
    void ribbonState()
    {
        const int toasts = int(shell_->toasts->toasts().size());
        shell_->router.submit(u"RIBBONCLOSE"_s);
        QVERIFY(!window_->isRegionVisible(Region::Ribbon));
        QCOMPARE(shell_->sysvars.get(u"RIBBONSTATE"_s), 0);
        QCOMPARE(int(shell_->toasts->toasts().size()), toasts + 1);
        QVERIFY(shell_->toasts->toasts().constLast()->text().contains(u"RIBBON"_s));
        shell_->router.submit(u"RIBBON"_s);
        shell_->router.submit(u"RIBBONCLOSE"_s);
        QCOMPARE(int(shell_->toasts->toasts().size()), toasts + 1); // một lần mỗi phiên
        shell_->router.submit(u"RIBBON"_s);
        shell_->ribbon->setCollapse(trion::ribbon::RibbonBar::Collapse::PanelTitles);
        shell_->router.submit(u"RIBBONCLOSE"_s);

        QVERIFY(restart());
        QCOMPARE(shell_->sysvars.get(u"RIBBONSTATE"_s), 0);
        QVERIFY(!window_->isRegionVisible(Region::Ribbon));
        QCOMPARE(shell_->ribbon->collapse(), trion::ribbon::RibbonBar::Collapse::PanelTitles);
        QVERIFY(shell_->toasts->toasts().isEmpty()); // khởi động ẩn sẵn: không nhắc
        shell_->router.submit(u"RIBBON"_s);
        QVERIFY(window_->isRegionVisible(Region::Ribbon));
        shell_->ribbon->setCollapse(trion::ribbon::RibbonBar::Collapse::Full);
    }

    // Kịch bản 8 — 1920×1080 và 1366×768 vi-XL: a11y::audit cửa sổ 0 vi phạm, không chữ nào bị cắt; menu ứng dụng và
    // lịch sử dòng lệnh đang mở cũng vậy.
    void screens_data()
    {
        QTest::addColumn<QRect>("screen");
        QTest::addColumn<bool>("pseudo");
        QTest::addRow("1920x1080") << kWide << false;
        QTest::addRow("1366x768-viXL") << kSmall << true;
    }

    void screens()
    {
        QFETCH(QRect, screen);
        QFETCH(bool, pseudo);
        i18n::Translator* translator = i18n::Translator::instance();
        window_->setWindowState(Qt::WindowNoState);
        window_->setGeometry(screen);
        window_->showMaximized();
        QVERIFY(spinUntil([&] { return window_->screen()->geometry() == screen; }));
        if (pseudo) {
            QVERIFY(translator->setLanguage(i18n::kPseudoLanguage.toString()));
            QCoreApplication::processEvents();
        }
        settle();
        QVERIFY(QTest::qWaitForWindowActive(window_));
        QStringList issues = trion::a11y::audit(window_);
        QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
        // Dải tab của nhóm palette (QMainWindowTabBar của Qt, PaletteHost UI0-113) hẹp hơn chữ khi vùng dock hẹp: lỗi
        // của thành phần dock, ghi báo cáo; phần còn lại của cửa sổ không được cắt chữ.
        QStringList cut, dockTabs;
        for (const i18n::Truncation& t : i18n::findTruncations(window_))
            (t.toString().startsWith(u"QMainWindowTabBar"_s) ? dockTabs : cut) << t.toString();
        if (!dockTabs.isEmpty())
            qWarning("dải tab nhóm palette cắt chữ: %s", qPrintable(dockTabs.join(u"; "_s)));
        QVERIFY2(cut.isEmpty(), qPrintable(cut.join(u"; "_s)));

        logo()->click();
        QVERIFY(spinUntil([&] { return shell_->appMenu->isVisible(); }));
        settle();
        issues = trion::a11y::checkNames(shell_->appMenu);
        QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
        QVERIFY2(i18n::findTruncations(shell_->appMenu).isEmpty(), qPrintable(truncations(shell_->appMenu)));
        key(Qt::Key_Escape);

        shell_->router.submit(u"'_TEXTSCR"_s);
        ui::DialogFrame* history = shell_->dialogs.find(u"textscr"_s);
        QVERIFY(history && QTest::qWaitForWindowActive(history));
        settle();
        issues = trion::a11y::audit(history);
        QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
        QVERIFY2(i18n::findTruncations(history).isEmpty(), qPrintable(truncations(history)));
        history->reject();

        if (pseudo) {
            QVERIFY(translator->setLanguage(u"vi"_s));
            QCoreApplication::processEvents();
        }
        window_->setWindowState(Qt::WindowNoState);
        window_->setGeometry(kWide);
        window_->showMaximized();
        QVERIFY(spinUntil([&] { return window_->screen()->geometry() == kWide; }));
        settle();
    }

    // Kịch bản 9 — dựng lại; sau khung đầu raster trước mọi biểu tượng ở nền (tự hoàn thành): không lượt nào của luồng
    // UI quá 2 ms (UiProbe, ngưỡng khựng 2 ms, đo từ khung đầu); xong thì rảnh 2 s không timer nào.
    void idle()
    {
        namespace probe = trion::ui::probe;
        stop();
        probe::enable();
        probe::setStallThreshold(2ms);
        atFirstFrame_ = [] { probe::reset(); };
        QVERIFY(start(false)); // không đổi phiên bản (áp workspace là lượt dài, không phải raster trước)
        atFirstFrame_ = nullptr;
        trion::theme::IconProvider& icons = trion::theme::IconProvider::instance();
        quint64 rasters = 0;
        QVERIFY(spinUntil([&] {
            sched::waitIdle();
            QCoreApplication::processEvents();
            return std::exchange(rasters, icons.stats().rasters) == icons.stats().rasters;
        }));
        const std::vector<probe::Stall> stalls = probe::stalls();
        probe::setStallThreshold(50ms);
        probe::disable();
        // Lượt của raster trước: lời gọi prewarm (MetaCall tới Application) và kết quả nền về (UiExecutor). Lượt khác
        // (vẽ lại khi cửa sổ active, kiểm tệp gần đây) chỉ in ra.
        QStringList all, prewarm;
        for (const probe::Stall& s : stalls) {
            const QString w = QString::fromStdString(s.where);
            const QString line = u"%1 ms %2"_s.arg(std::chrono::duration<double, std::milli>(s.duration).count()).arg(w);
            all << line;
            if (w.contains(u"Application"_s) || w.contains(u"UiExecutor"_s))
                prewarm << line;
        }
        qInfo("raster trước sau khung đầu: %llu mặt nạ; lượt UI > 2 ms: %lld (%s)",
              static_cast<unsigned long long>(rasters), qlonglong(stalls.size()), qPrintable(all.join(u"; "_s)));
        QVERIFY(rasters > 0);
        QVERIFY2(!trion::test::kTimed || prewarm.isEmpty(), qPrintable(prewarm.join(u'\n')));

        QVERIFY(focusOn(canvas()));
        QTest::mouseMove(canvas(), canvas()->rect().center());
        if (ui::RichTooltip* tips = ui::RichTooltip::instance())
            tips->leave();
        sched::waitIdle();
        spinFor(3s);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        const QStringList timers = timersDuring(2s);
        QVERIFY2(timers.isEmpty(), qPrintable(timers.join(u", "_s)));
    }
};

QTEST_MAIN(IntegrationLap)
#include "it_lap_test.moc"
