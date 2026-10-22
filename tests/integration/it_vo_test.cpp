// Integration test nhóm 5 (khung ứng dụng đợt 5): UIA-001 (cửa sổ chính, Clean Screen, workspace), UIA-002 (thanh tiêu
// đề, ô Tìm lệnh), UIA-010 (tab tệp), UIA-102 (ribbon JSON, điều khiển), UIA-107 (dòng lệnh), UIA-112 (nút chế độ),
// UI0-013 (trợ năng) lắp vào cửa sổ chính bằng chính Application của exe trion (app/main/ShellAssembly): Store tạm,
// phiên bản đặt tường minh, hai màn hình giả của plugin offscreen (1920×1080 Wide, 1366×768 Small). Lệnh đi tới bộ
// định tuyến tạm (CommandRouter): lệnh chưa có xử lý tới tín hiệu unhandled, test ghi lại cùng tài liệu đang chọn.
//   1 regions1920       vùng là widget thật (trừ vùng vẽ, tab layout), tỉ lệ vùng vẽ, a11y::audit
//   2 cleanScreen       Ctrl+0 bật/tắt: hình học từng vùng, ảnh cửa sổ từng pixel, dòng lịch sử
//   3 commandLineModes  Ctrl+9 ẩn/hiện dòng lệnh (CLISTATE); F8, nút trạng thái, toggle ribbon cùng đổi trong lượt
//   4 commandSearch     "đường tròn" → CIRCLE, vị trí ribbon; Enter → ^C^C_CIRCLE
//   5 commandLineInput  "LINE" + Enter; chip và chữ tắt cho cùng token; Esc bỏ prompt
//   6 fileTabs          chuột giữa → CLOSE cho đúng tài liệu; Ctrl+Tab; thả .dwg/.pdf
//   7 languages         vi → en → vi mọi vùng dịch lại; vi-XL ở 1366×768 không cắt chữ, dòng lệnh compact
//   8 viewer            workspace viewer, 3 nút chế độ, lệnh Pro khóa trong ô Tìm lệnh
//   9 jobFailureToast   tác vụ nền lỗi → toast lỗi trên cửa sổ chính (ProgressPresenter, followJobs)
//  10 idle              rảnh 2 s: không timer nào chạy

#include "app/core/a11y/AccessibilityAudit.h"
#include "app/core/actions/ActionRegistry.h"
#include "app/core/display/DisplayContext.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/main/Application.h"
#include "app/main/ShellAssembly.h"
#include "app/main/StartupTrace.h"
#include "app/shell/CleanScreen.h"
#include "app/shell/cmdline/CommandLine.h"
#include "app/shell/CommandRouter.h"
#include "app/shell/filetabs/FileTabBar.h"
#include "app/shell/MainWindow.h"
#include "app/shell/ribbon/controls/RibbonControls.h"
#include "app/shell/status/ModeToggles.h"
#include "app/shell/titlebar/CommandSearch.h"
#include "app/shell/titlebar/TitleBar.h"
#include "app/shell/Workspace.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/cmdline/CommandInput.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/dock/AutoHideStrip.h"
#include "app/widgets/dock/PaletteHost.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/progress/ProgressPresenter.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/jobs/BackgroundJobs.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"

#include <QAbstractItemView>
#include <QAccessible>
#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaEnum>
#include <QMimeData>
#include <QPainter>
#include <QScreen>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>

#include <algorithm>
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

QString g_cwd; // thư mục chạy test; initMain đổi sang thư mục chứa cấu hình màn hình tới khi tạo lớp test

constexpr QRect kWide(0, 0, 1920, 1080);    // màn hình chính (Wide)
constexpr QRect kSmall(2000, 0, 1366, 768); // màn hình thứ hai (Small, ≤ 1366)

// Prompt giả như lệnh CIRCLE hỏi (INT-003 chưa có): chữ qua bộ dịch đang cài, đọc qua biến nên lupdate không thu.
constexpr const char* kRadiusPrompt = "Chỉ định bán kính hoặc"; // i18n-ok: chữ mẫu của test
constexpr const char* kDiameter = "Đường kính";                 // i18n-ok: chữ mẫu của test
constexpr const char* kJobOpen = "Đang mở Nha-pho.dwg";         // i18n-ok: chữ mẫu của test
constexpr const char* kCorrupt = "Bảng đối tượng hỏng ở mục 3"; // i18n-ok: thông điệp của lỗi giả

QString tx(const char* source)
{
    return QCoreApplication::translate("IntegrationVo", source);
}

// Chạy vòng sự kiện tới khi cond đúng hoặc hết giờ. Timer trên stack: chờ xong không để lại timer nào chạy tiếp.
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
    QEventLoop loop;
    QTimer limit;
    limit.setSingleShot(true);
    QObject::connect(&limit, &QTimer::timeout, &loop, &QEventLoop::quit);
    limit.start(d);
    loop.exec();
}

// Bố cục xong (LayoutRequest gửi qua hàng đợi), sự kiện đang chờ đã chạy.
void settle()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
}

// Focus vào w trên cửa sổ đã kích hoạt. Plugin offscreen kích hoạt cả cửa sổ popup lúc hiện và không trả lại khi
// popup đóng: cửa sổ focus còn là popup QCompleter đã ẩn, lần kích hoạt cửa sổ chính sau đó đưa focus về proxy của
// popup (ô Tìm lệnh). Nền tảng thật không kích hoạt popup.
bool focusOn(QWidget* w)
{
    w->window()->activateWindow();
    if (!spinUntil([w] { return QGuiApplication::focusWindow() == w->window()->windowHandle(); }))
        return false;
    w->setFocus(Qt::OtherFocusReason);
    return spinUntil([w] { return QApplication::focusWidget() == w; });
}

// Gõ chữ vào ô (hoặc popup đang mở của nó) như bàn phím: mỗi ký tự một cặp phím có chữ.
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

// Phím tới popup đang mở, không có thì tới widget đang có focus.
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

// Chữ trình đọc màn hình đọc được trong cây của root (tên, mô tả của mọi phần tử đang hiện), theo thứ tự cây.
QStringList spoken(QWidget* root)
{
    QStringList out;
    const std::function<void(QAccessibleInterface*)> visit = [&](QAccessibleInterface* i) {
        if (!i || !i->isValid() || i->state().invisible)
            return;
        out << i->text(QAccessible::Name) << i->text(QAccessible::Description);
        for (int k = 0; k < i->childCount(); ++k)
            visit(i->child(k));
    };
    visit(QAccessible::queryAccessibleInterface(root));
    out.removeAll(QString());
    return out;
}

QString regionKey(Region r)
{
    return QString::fromLatin1(QMetaEnum::fromType<Region>().valueToKey(int(r)));
}

// Đếm QTimerEvent tới mọi đối tượng trong tiến trình, trừ timer đo của test.
class TimerLog final : public QObject {
public:
    explicit TimerLog(const QObject* ignore) : ignore_(ignore) {}
    QStringList seen;

protected:
    bool eventFilter(QObject* watched, QEvent* e) override
    {
        if (e->type() == QEvent::Timer && watched != ignore_) {
            const QObject* owner = watched->parent() ? watched->parent() : watched;
            seen << QString::fromLatin1(watched->metaObject()->className()) + u" ← "_s +
                        QString::fromLatin1(owner->metaObject()->className());
        }
        return false;
    }

private:
    const QObject* ignore_;
};

} // namespace

class IntegrationVo : public QObject {
    Q_OBJECT
public:
    // Sau QApplication (plugin đã đọc cấu hình màn hình), trước qExec: về lại thư mục chạy test.
    IntegrationVo() { QDir::setCurrent(g_cwd); }

    // Trước QApplication (QTEST_MAIN), như main.cpp: DPR không làm tròn; hai màn hình giả 100%.
    static void initMain()
    {
        for (const char* v : {"QT_ENABLE_HIGHDPI_SCALING", "QT_SCALE_FACTOR", "QT_SCREEN_SCALE_FACTORS",
                              "QT_SCALE_FACTOR_ROUNDING_POLICY"})
            qunsetenv(v);
        ui::DisplayContext::initMain();
        QJsonArray list;
        // Plugin offscreen giữ màn hình theo tên: tên xếp trước ("p…", như shell_test) là màn hình chính.
        for (const auto& [name, r] : {std::pair{"p1920", kWide}, std::pair{"s1366", kSmall}})
            list.append(QJsonObject{{u"name"_s, QLatin1StringView(name)},
                                    {u"x"_s, r.x()},
                                    {u"y"_s, r.y()},
                                    {u"width"_s, r.width()},
                                    {u"height"_s, r.height()},
                                    {u"logicalDpi"_s, 96},
                                    {u"logicalBaseDpi"_s, 96},
                                    {u"dpr"_s, 1}});
        // Đường dẫn tương đối: tùy chọn của plugin ngăn cách bằng ':', đường dẫn Windows "C:/…" có ':'.
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
    std::unique_ptr<StartupTrace> trace_;
    std::unique_ptr<Application> app_;
    ShellAssembly* shell_ = nullptr;
    MainWindow* window_ = nullptr;
    QStringList sent_;    // chuỗi lệnh tới router chưa có xử lý (INT-002 sẽ nhận)
    QList<quint64> docs_; // tài liệu đang chọn ở tab tệp lúc router nhận từng chuỗi
    QList<QUrl> opened_;  // trang nâng cấp đã mở (thay QDesktopServices)

    QStringList take()
    {
        docs_.clear();
        return std::exchange(sent_, {});
    }
    QWidget* region(Region r) const { return window_->region(r); }
    QWidget* canvas() const { return region(Region::Canvas); }
    ui::PaletteHost& palettes() const { return window_->palettes(); }

    double canvasShare() const
    {
        settle();
        return double(canvas()->width()) * canvas()->height() / (double(window_->width()) * window_->height());
    }

    // Vùng, palette, dải tự ẩn: hiện/ẩn và hình học theo tọa độ cửa sổ.
    QStringList layout() const
    {
        settle();
        QStringList s;
        const auto rect = [this](const QWidget* w) {
            const QRect r(w->mapTo(window_, QPoint()), w->size());
            return u"%1,%2 %3×%4"_s.arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
        };
        for (int i = 0; i < MainWindow::kRegionCount; ++i) {
            const QWidget* w = region(Region(i));
            s << regionKey(Region(i)) + u' ' + (w->isVisible() ? rect(w) : u"ẩn"_s);
        }
        for (const QString& id : palettes().ids()) {
            const QDockWidget* d = palettes().dock(id);
            s << id + u' ' + (d->isVisible() ? (d->isFloating() ? u"nổi"_s : rect(d)) : u"ẩn"_s);
        }
        for (const Qt::DockWidgetArea side : {Qt::LeftDockWidgetArea, Qt::RightDockWidgetArea})
            if (const ui::AutoHideStrip* st = palettes().strip(side); st && st->isVisible())
                s << u"strip %1 %2"_s.arg(int(side)).arg(rect(st));
        return s;
    }

    // Ảnh cửa sổ; vùng dòng lệnh tô một màu (lịch sử nhận dòng thông báo, phần còn lại so từng pixel).
    QImage shot() const
    {
        settle();
        QImage img = window_->grab().toImage();
        const QWidget* cmd = region(Region::CommandLine);
        QPainter(&img).fillRect(QRect(cmd->mapTo(window_, QPoint()), cmd->size()), Qt::magenta);
        return img;
    }

    int visibleModes() const
    {
        int n = 0;
        for (const QToolButton* b : shell_->modes->findChildren<QToolButton*>(Qt::FindDirectChildrenOnly))
            n += b->isVisibleTo(shell_->modes);
        return n;
    }

private slots:
    void initTestCase()
    {
        QCOMPARE(QGuiApplication::screens().size(), qsizetype(2));
        QCOMPARE(QGuiApplication::primaryScreen()->geometry(), kWide);
        dir_ = std::make_unique<QTemporaryDir>();
        QVERIFY(dir_->isValid());
        // Linux: "giảm chuyển động" của hệ điều hành (DisplayContext) đọc $XDG_CONFIG_HOME, không lấy của máy chạy
        // test.
        QVERIFY(QDir(dir_->path()).mkpath(u"config"_s));
        qputenv("XDG_CONFIG_HOME", QFile::encodeName(dir_->filePath(u"config"_s)));

        // Như main.cpp: Application dựng mọi phần theo thứ tự khởi động, cửa sổ chính phóng to trên màn hình chính.
        trion::app::Options options;
        options.settingsPath = dir_->filePath(u"user.json"_s);
        trace_ = std::make_unique<StartupTrace>();
        app_ = std::make_unique<Application>(options, *trace_);
        shell_ = app_->shell();
        window_ = qobject_cast<MainWindow*>(app_->mainWindow());
        QVERIFY(shell_ && window_);
        QVERIFY(spinUntil([&] { return trace_->at(u"first_frame"_s).has_value(); }));
        QVERIFY(QTest::qWaitForWindowActive(window_));
        QCOMPARE(i18n::Translator::instance()->language(), u"vi"_s);

        connect(&shell_->router, &shell::CommandRouter::unhandled, this, [this](const QString& macro) {
            sent_ << macro;
            docs_ << shell_->fileTabs->current();
        });
        shell_->gate.upgrade().setUrlOpener([this](const QUrl& url) {
            opened_ << url;
            return true;
        });
        shell_->toasts->setTimeout(1h); // toast ở lại tới khi kịch bản đóng
    }

    void cleanupTestCase()
    {
        // Thoát như exe trion: Application hủy cửa sổ (widget) trước ShellAssembly, ActionRegistry, Sysvars,
        // EditionGate, rồi chờ việc nền và ghi cài đặt.
        app_.reset();
        trace_.reset();
        sched::waitIdle();
        QCoreApplication::processEvents(); // kết quả nền đã gửi về chạy khi đối tượng nhận đã hủy
        dir_.reset();
    }

    void init()
    {
        shell_->gate.setEdition(Edition::Pro); // phiên bản tường minh; kịch bản Viewer tự đổi
        take();
    }

    // Sau mỗi kịch bản, kể cả khi trượt giữa chừng: đóng popup, toast, trả focus về vùng vẽ.
    void cleanup()
    {
        while (QWidget* popup = QApplication::activePopupWidget())
            popup->close();
        for (ui::Toast* t : shell_->toasts->toasts())
            t->closeButton()->click();
        spinUntil([&] { return shell_->toasts->toasts().isEmpty(); }, 5s);
        if (shell_->commandLine->hasPrompt())
            shell_->commandLine->clearPrompt();
        focusOn(canvas());
    }

    // Kịch bản 1 — 1920×1080, Pro: mọi vùng là widget thật trừ vùng vẽ và tab layout (UIA-109, UIA-011 chưa có), tỉ
    // lệ vùng vẽ, a11y::audit của cả cửa sổ (tên, vai trò, thứ tự Tab, bẫy focus, viền focus).
    void regions1920()
    {
        QVERIFY(window_->isMaximized());
        QCOMPARE(window_->frameGeometry(), kWide); // offscreen có khung 2 px
        QCOMPARE(window_->screenClass(), ui::ScreenClass::Wide);
        QVERIFY(qobject_cast<shell::TitleBar*>(region(Region::TitleBar)));
        QVERIFY(region(Region::Ribbon)->findChild<trion::ribbon::RibbonPages*>());
        QVERIFY(region(Region::Ribbon)->findChild<ui::TabBar*>());
        QVERIFY(qobject_cast<shell::FileTabBar*>(region(Region::FileTabs)));
        QVERIFY(qobject_cast<ui::CommandLine*>(region(Region::CommandLine)));
        QVERIFY(region(Region::StatusBar)->findChild<shell::ModeToggles*>());
        for (const Region r : {Region::Canvas, Region::LayoutTabs}) // ô giữ chỗ của MainWindow
            QCOMPARE(region(r)->metaObject(), &QWidget::staticMetaObject);
        int y = 0; // xếp liền nhau từ trên xuống, vùng thật có chiều cao
        for (int i = 0; i < MainWindow::kRegionCount; ++i) {
            const QWidget* w = Region(i) == Region::Canvas ? window_->dockHost() : region(Region(i));
            const QRect g(w->mapTo(window_, QPoint()), w->size());
            QVERIFY2(g.y() == y && g.height() > 0 && g.width() == window_->width(), qPrintable(regionKey(Region(i))));
            y += g.height();
        }
        QCOMPARE(y, window_->height());
        QVERIFY(shell_->modes->button(u"ortho"_s)->isVisible());
        QVERIFY(shell_->ribbonPages->isBuilt(0) && !shell_->ribbonPages->isBuilt(1)); // tab khác dựng khi mở

        const double share = canvasShare();
        QStringList heights;
        for (int i = 0; i < MainWindow::kRegionCount; ++i)
            if (Region(i) != Region::Canvas)
                heights << u"%1 %2"_s.arg(regionKey(Region(i))).arg(region(Region(i))->height());
        qInfo("1920×1080 Pro: vùng vẽ %d×%d = %.1f%% diện tích cửa sổ (%s)", canvas()->width(), canvas()->height(),
              share * 100, qPrintable(heights.join(u", "_s)));
        QVERIFY2(share >= 0.55, qPrintable(QString::number(share)));

        // Phần tử nhận Tab chưa có objectName: tên tạm là chuỗi lớp#tên của nó và các cha, để vi phạm chỉ ra thành phần
        // (trả lại tên rỗng sau khi kiểm).
        QList<QWidget*> named;
        for (QWidget* w : window_->findChildren<QWidget*>())
            if (w->objectName().isEmpty() && (w->focusPolicy() & Qt::TabFocus)) {
                QStringList chain;
                for (const QWidget* p = w; p != window_; p = p->parentWidget())
                    chain << QString::fromLatin1(p->metaObject()->className()) + u'#' + p->objectName();
                named << w;
                w->setObjectName(chain.join(u'<'));
            }
        QVERIFY(QTest::qWaitForWindowActive(window_));
        QStringList order;
        const QStringList issues = trion::a11y::checkNames(window_) + trion::a11y::checkFocus(window_, &order);
        for (QWidget* w : std::as_const(named))
            w->setObjectName(QString());
        qInfo("a11y: %lld điểm dừng Tab", qlonglong(order.size()));
        QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
    }

    // Kịch bản 2 — Ctrl+0 với widget thật: bật ẩn thanh tiêu đề, ribbon, tab tệp, tab layout, palette; tắt trả đúng
    // hình học từng vùng, palette, dải và ảnh cửa sổ từng pixel (trừ dòng lệnh); mỗi lần một dòng lịch sử.
    void cleanScreen()
    {
        shell::CleanScreen& clean = shell_->clean;
        ui::CommandLine* cmd = shell_->commandLine;
        const QString key = ui::Menu::keyText(shell_->actions.action(u"CLEANSCREENON"_s));
        QVERIFY(!key.isEmpty());
        QVERIFY(!clean.isOn());
        QTest::mouseMove(canvas(), canvas()->rect().center()); // chuột trên vùng vẽ: không hiệu ứng rê ở vùng ẩn/hiện
        const QStringList before = layout();
        const QImage beforeShot = shot();
        const int lines = cmd->historyCount();

        QTest::keyClick(window_, Qt::Key_0, Qt::ControlModifier);
        QVERIFY(clean.isOn());
        QCOMPARE(shell_->sysvars.get(u"CLEANSCREENSTATE"_s), 1);
        settle();
        for (const Region r : {Region::TitleBar, Region::Ribbon, Region::FileTabs, Region::LayoutTabs})
            QVERIFY2(!region(r)->isVisible(), qPrintable(regionKey(r)));
        for (const Region r : {Region::Canvas, Region::CommandLine, Region::StatusBar})
            QVERIFY2(region(r)->isVisible(), qPrintable(regionKey(r)));
        for (const QString& id : palettes().ids())
            QVERIFY2(!palettes().dock(id)->isVisible(), qPrintable(id));
        QCOMPARE(canvas()->width(), window_->width());
        QCOMPARE(cmd->historyCount(), lines + 1);
        QCOMPARE(cmd->historyLine(lines), u"<Clean Screen bật · %1 để tắt>"_s.arg(key)); // i18n-ok: chữ mong đợi
        qInfo("Clean Screen 1920×1080: vùng vẽ %.1f%%", canvasShare() * 100);

        QTest::keyClick(window_, Qt::Key_0, Qt::ControlModifier);
        QVERIFY(!clean.isOn());
        QCOMPARE(shell_->sysvars.get(u"CLEANSCREENSTATE"_s), 0);
        QCOMPARE(layout(), before);
        QVERIFY(shot() == beforeShot);
        QCOMPARE(cmd->historyCount(), lines + 2);
        QCOMPARE(cmd->historyLine(lines + 1), u"<Clean Screen tắt>"_s); // i18n-ok: chữ mong đợi
        QCOMPARE(take(), QStringList()); // CLEANSCREENON/OFF do giao diện chạy, không tới chỗ nối INT-002
    }

    // Kịch bản 3 — Ctrl+9 ẩn/hiện dòng lệnh (CLISTATE theo vùng, vùng vẽ giãn ra phần đó, hiện lại thì focus ở ô
    // nhập); F8 đổi ORTHOMODE và nút ORTHO ngay trong lượt phím; LWDISPLAY: nút thanh trạng thái và toggle ribbon
    // (tab Hiển thị) cùng đổi trong lượt bấm của nhau.
    void commandLineModes()
    {
        trion::actions::Sysvars& vars = shell_->sysvars;
        QCOMPARE(vars.get(u"CLISTATE"_s), 1);
        QCOMPARE(shell_->actions.macro(u"COMMANDLINE"_s), u"'_COMMANDLINEHIDE"_s);
        const int height = canvas()->height();
        QTest::keyClick(window_, Qt::Key_9, Qt::ControlModifier);
        QVERIFY(!window_->isRegionVisible(Region::CommandLine));
        QCOMPARE(vars.get(u"CLISTATE"_s), 0);
        settle();
        QCOMPARE(canvas()->height(), height + shell_->commandLine->height());
        QCOMPARE(shell_->actions.macro(u"COMMANDLINE"_s), u"'_COMMANDLINE"_s);
        QTest::keyClick(window_, Qt::Key_9, Qt::ControlModifier);
        QVERIFY(window_->isRegionVisible(Region::CommandLine));
        QCOMPARE(vars.get(u"CLISTATE"_s), 1);
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == shell_->commandLine->input(); }));
        settle();
        QCOMPARE(canvas()->height(), height);
        // Ẩn khi focus ở ô nhập: focus về vùng vẽ (phím gõ vào ô nhập động, COMMANDLINEHIDE.md)
        QTest::keyClick(window_, Qt::Key_9, Qt::ControlModifier);
        QCOMPARE(QApplication::focusWidget(), canvas());
        QTest::keyClick(window_, Qt::Key_9, Qt::ControlModifier);
        QVERIFY(window_->isRegionVisible(Region::CommandLine));
        QCOMPARE(take(), QStringList());
        QVERIFY(focusOn(canvas()));

        QToolButton* ortho = shell_->modes->button(u"ortho"_s);
        QVERIFY(ortho);
        for (const bool on : {true, false}) {
            QTest::keyClick(window_, Qt::Key_F8); // không chạy vòng sự kiện giữa phím và lúc kiểm
            QCOMPARE(vars.get(u"ORTHOMODE"_s), on ? 1 : 0);
            QCOMPARE(ortho->isChecked(), on);
        }

        const auto& tabs = shell_->ribbonModel.tabs();
        const auto view = std::ranges::find(tabs, u"view"_s, &trion::ribbon::Tab::id);
        QVERIFY(view != tabs.end());
        shell_->ribbonTabs->setCurrentIndex(int(view - tabs.begin()));
        settle();
        auto* ribbonLw = region(Region::Ribbon)->findChild<trion::ribbon::RibbonButton*>(u"lwtgl"_s);
        QToolButton* modeLw = shell_->modes->button(u"LWDISPLAY"_s);
        QVERIFY(ribbonLw && ribbonLw->isVisible() && modeLw);
        const bool lw = ribbonLw->isChecked();
        QCOMPARE(modeLw->isChecked(), lw);
        QTest::mouseClick(modeLw, Qt::LeftButton);
        QCOMPARE(ribbonLw->isChecked(), !lw);
        QCOMPARE(modeLw->isChecked(), !lw);
        QTest::mouseClick(ribbonLw, Qt::LeftButton);
        QCOMPARE(ribbonLw->isChecked(), lw);
        QCOMPARE(modeLw->isChecked(), lw);
        QCOMPARE(take(), QStringList()); // 'ORTHOMODE 1, 'LWDISPLAY … là sysvar trong suốt: router đặt Sysvars
        shell_->ribbonTabs->setCurrentIndex(0);
    }

    // Kịch bản 4 — ô Tìm lệnh: "đường tròn" → CIRCLE đầu danh sách, dòng có vị trí ribbon từ RibbonModel; Enter chạy
    // qua QAction của ActionRegistry tới router.
    void commandSearch()
    {
        shell::CommandSearch* search = shell_->titleBar->search();
        const QString query = u"đường tròn"_s; // i18n-ok: chữ người dùng gõ
        QCOMPARE(search->search(query).value(0), u"CIRCLE"_s);
        const QString where = shell_->ribbonModel.locate(u"CIRCLE"_s);
        QCOMPARE(where, u"Trang chủ › Vẽ"_s); // i18n-ok: chữ mong đợi

        QVERIFY(focusOn(search));
        type(search, query);
        QVERIFY(spinUntil([&] { return search->popup()->isVisible(); }));
        const QModelIndex first = search->popup()->model()->index(0, 0);
        QCOMPARE(first.data(Qt::UserRole).toString(), u"CIRCLE"_s);
        QVERIFY2(first.data(Qt::AccessibleTextRole).toString().contains(where),
                 qPrintable(first.data(Qt::AccessibleTextRole).toString()));
        key(Qt::Key_Return);
        QCOMPARE(take(), QStringList{u"^C^C_CIRCLE"_s});
        QVERIFY(!search->popup()->isVisible());
    }

    // Kịch bản 5 — dòng lệnh: gõ tên lệnh rồi Enter tới router; prompt giả có chip: bấm chip và gõ chữ tắt (cả "_" +
    // tên toàn cục) cho cùng token; Esc khi có prompt bỏ prompt (cancelRequested → clearPrompt).
    void commandLineInput()
    {
        ui::CommandLine* cmd = shell_->commandLine;
        QVERIFY(focusOn(cmd->input()));
        type(cmd->input(), u"LINE"_s);
        key(Qt::Key_Return);
        QCOMPARE(take(), QStringList{u"LINE"_s});
        QVERIFY(cmd->input()->text().isEmpty());

        cmd->setPrompt({.command = u"CIRCLE"_s,
                        .text = tx(kRadiusPrompt),
                        .keywords = {{tx(kDiameter), u"D"_s, u"_Diameter"_s}},
                        .defaultValue = u"10.0000"_s});
        settle();
        const QRect chip = cmd->keywordRect(u"D"_s);
        auto* view = cmd->findChild<QWidget*>(u"prompt"_s);
        QVERIFY(!chip.isNull() && view);
        QTest::mouseClick(view, Qt::LeftButton, {}, view->mapFrom(cmd, chip.center()));
        QCOMPARE(take(), QStringList{u"D"_s});
        for (const QString& typed : {u"d"_s, u"_Diameter"_s}) {
            QVERIFY(focusOn(cmd->input()));
            type(cmd->input(), typed);
            key(Qt::Key_Return);
            QCOMPARE(take(), QStringList{u"D"_s});
        }
        QVERIFY(cmd->hasPrompt());
        key(Qt::Key_Escape);
        QVERIFY(!cmd->hasPrompt());
        QCOMPARE(take(), QStringList());
    }

    // Kịch bản 6 — tab tệp: 3 tài liệu qua chỗ nối (DOC-018 chưa có); chuột giữa trên tab thứ hai: tài liệu đó thành
    // hiện hành rồi router nhận ^C^C_CLOSE; Ctrl+Tab / Ctrl+Shift+Tab đổi tab; thả .dwg và .pdf: .dwg tới
    // Application::openRequested, .pdf bị từ chối bằng toast lỗi nêu tên.
    void fileTabs()
    {
        shell::FileTabBar* tabs = shell_->fileTabs;
        for (const quint64 id : {1, 2, 3})
            tabs->addDocument({.id = id,
                               .name = u"Ban-ve-%1.dwg"_s.arg(id),
                               .path = dir_->filePath(u"Ban-ve-%1.dwg"_s.arg(id)),
                               .dirty = id == 2,
                               .readOnly = false,
                               .layouts = {}});
        tabs->setCurrent(1);
        settle();
        QCOMPARE(tabs->documents(), (QList<quint64>{1, 2, 3}));
        QTest::mouseClick(tabs->tabBar(), Qt::MiddleButton, {}, tabs->tabBar()->tabRect(2).center());
        QCOMPARE(sent_, QStringList{u"^C^C_CLOSE"_s});
        QCOMPARE(docs_, QList<quint64>{2});
        QCOMPARE(tabs->current(), quint64(2));
        take();

        QTest::keyClick(window_, Qt::Key_Tab, Qt::ControlModifier);
        QCOMPARE(tabs->current(), quint64(3));
        QTest::keyClick(window_, Qt::Key_Backtab, Qt::ControlModifier | Qt::ShiftModifier); // như bàn phím thật
        QCOMPARE(tabs->current(), quint64(2));

        QSignalSpy open(app_.get(), &Application::openRequested);
        const QString dwg = dir_->filePath(u"Nha-pho.dwg"_s), pdf = dir_->filePath(u"Ho-so.pdf"_s);
        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(dwg), QUrl::fromLocalFile(pdf)});
        const QPoint pos = tabs->rect().center();
        QDragEnterEvent enter(pos, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(tabs, &enter);
        QVERIFY(enter.isAccepted());
        QDropEvent drop(QPointF(pos), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(tabs, &drop);
        QCOMPARE(open.count(), 1);
        QCOMPARE(open[0][0].toStringList(), QStringList{dwg});
        const QList<ui::Toast*> toasts = shell_->toasts->toasts();
        QCOMPARE(toasts.size(), 1);
        QCOMPARE(toasts[0]->severity(), ui::Severity::Error);
        QVERIFY2(toasts[0]->text().contains(u"Ho-so.pdf"_s), qPrintable(toasts[0]->text()));
        QCOMPARE(shell_->notes.rowCount(), 1);
        QCOMPARE(take(), QStringList());

        for (const quint64 id : {1, 2, 3})
            tabs->removeDocument(id);
        tabs->setCurrent(shell::FileTabBar::kStart);
    }

    // Kịch bản 7 — đổi ngôn ngữ khi đang chạy: mọi vùng dịch lại (chữ trình đọc màn hình đọc được của vùng đổi ở en, về
    // đúng như cũ ở vi); kéo cửa sổ sang màn hình 1366×768: dòng lệnh compact, ở vi-XL không vùng nào cắt chữ.
    void languages()
    {
        i18n::Translator* translator = i18n::Translator::instance();
        QHash<int, QStringList> vi;
        for (int i = 0; i < MainWindow::kRegionCount; ++i)
            vi[i] = spoken(region(Region(i)));
        for (const QString& lang : {u"en"_s, u"vi"_s}) {
            QVERIFY(translator->setLanguage(lang));
            QCoreApplication::processEvents(); // LanguageChange → retranslate(), rồi bố cục lại
            settle();
            for (int i = 0; i < MainWindow::kRegionCount; ++i) {
                const QStringList now = spoken(region(Region(i)));
                const QByteArray where = (lang + u' ' + regionKey(Region(i))).toUtf8();
                QVERIFY2(!now.isEmpty(), where.constData());
                if (lang == u"en"_s)
                    QVERIFY2(now != vi[i], where.constData());
                else
                    QCOMPARE(now, vi[i]);
            }
        }

        window_->setWindowState(Qt::WindowNoState);
        window_->setGeometry(kSmall);
        QVERIFY(spinUntil([&] { return window_->screenClass() == ui::ScreenClass::Small; }));
        QCOMPARE(window_->screen()->geometry(), kSmall);
        QVERIFY(shell_->commandLine->isCompact());
        QVERIFY(translator->setLanguage(i18n::kPseudoLanguage.toString()));
        QCoreApplication::processEvents();
        settle();
        QCOMPARE(window_->size(), kSmall.size());
        qInfo("1366×768 vi-XL: vùng vẽ %.1f%%", canvasShare() * 100);
        QVERIFY2(i18n::findTruncations(window_).isEmpty(), qPrintable(truncations(window_)));

        QVERIFY(translator->setLanguage(u"vi"_s));
        QCoreApplication::processEvents();
        window_->setGeometry(kWide);
        window_->showMaximized();
        QVERIFY(spinUntil([&] { return window_->screenClass() == ui::ScreenClass::Wide; }));
        QVERIFY(!shell_->commandLine->isCompact());
        settle();
    }

    // Kịch bản 8 — Viewer: workspace viewer, ModeToggles còn 3 nút (OSNAP, lineweight, độ trong suốt), lệnh Pro
    // trong ô Tìm lệnh có huy hiệu khóa, Enter mở trang nâng cấp thay vì gửi lệnh.
    void viewer()
    {
        shell_->gate.setEdition(Edition::Viewer);
        QCOMPARE(window_->workspace().current(), u"viewer"_s);
        QCOMPARE(visibleModes(), 3);
        for (const char* id : {"osnap", "LWDISPLAY", "TRANSPARENCYDISPLAY"})
            QVERIFY2(shell_->modes->button(QString::fromLatin1(id))->isVisibleTo(shell_->modes), id);

        shell::CommandSearch* search = shell_->titleBar->search();
        QVERIFY(focusOn(search));
        type(search, u"đường tròn"_s); // i18n-ok: chữ người dùng gõ
        QVERIFY(spinUntil([&] { return search->popup()->isVisible(); }));
        const QModelIndex first = search->popup()->model()->index(0, 0);
        QCOMPARE(first.data(Qt::UserRole).toString(), u"CIRCLE"_s);
        QVERIFY(first.data(Qt::AccessibleTextRole)
                    .toString()
                    .contains(trion::edition::EditionGate::availableIn(ui::Edition::Pro)));
        opened_.clear();
        key(Qt::Key_Return);
        QCOMPARE(opened_.size(), 1);
        QCOMPARE(take(), QStringList());

        shell_->gate.setEdition(Edition::Pro);
        QCOMPARE(window_->workspace().current(), u"2d"_s);
        QCOMPARE(visibleModes(), 10); // 9 nút EA + Isometric (P1, hành động "iso" gắn SNAPSTYL)
    }

    // Kịch bản 9 — tác vụ nền lỗi trên registry của ứng dụng (BackgroundJobs::instance()): ProgressPresenter (cha là
    // cửa sổ chính) báo xong, ToastHost::followJobs hiện toast lỗi nêu lý do trên cửa sổ chính, có mục trong
    // NotificationCenter (trình đọc màn hình đọc qua toast, UI0-013).
    void jobFailureToast()
    {
        trion::jobs::BackgroundJobs* jobs = trion::jobs::BackgroundJobs::instance();
        QVERIFY(jobs);
        QCOMPARE(shell_->presenter->parent(), window_);
        const int notes = shell_->notes.rowCount();
        jobs->start(tx(kJobOpen), u"open"_s, 0,
                    [](trion::jobs::Progress&, sched::CancelToken) -> trion::Expected<void> {
                        return trion::Error{"dwg", "corrupt", kCorrupt};
                    });
        QVERIFY(spinUntil([&] { return !shell_->toasts->toasts().isEmpty(); }));
        const ui::Toast* toast = shell_->toasts->toasts().constLast();
        QCOMPARE(toast->severity(), ui::Severity::Error);
        QCOMPARE(toast->parentWidget(), window_);
        QVERIFY2(toast->text().contains(tx(kJobOpen)) && toast->text().contains(QString::fromUtf8(kCorrupt)),
                 qPrintable(toast->text()));
        QCOMPARE(shell_->notes.rowCount(), notes + 1);
        QVERIFY(spinUntil([&] { return jobs->size() == 0; }));
    }

    // Kịch bản 10 — rảnh sau mọi kịch bản trên, focus ở vùng vẽ, chuột trên vùng vẽ: 2 s không QTimerEvent nào tới
    // bất kỳ đối tượng nào (timer định kỳ, polling, tooltip, toast), không việc nền nào.
    void idle()
    {
        QTest::mouseMove(canvas(), canvas()->rect().center());
        if (ui::RichTooltip* tips = ui::RichTooltip::instance())
            tips->leave();
        QVERIFY(focusOn(canvas()));
        QVERIFY(spinUntil([&] { return trion::jobs::BackgroundJobs::instance()->size() == 0; }));
        sched::waitIdle();
        // Việc còn chờ (vẽ, bố cục, raster trước) xong; timer một lần của Qt sau lần di chuột cuối (tooltip, 700 ms)
        // đã chạy.
        spinFor(1s);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        QTimer end;
        end.setSingleShot(true);
        TimerLog log(&end);
        qApp->installEventFilter(&log);
        QEventLoop loop;
        connect(&end, &QTimer::timeout, &loop, &QEventLoop::quit);
        end.start(2s);
        loop.exec();
        qApp->removeEventFilter(&log);
        QVERIFY2(log.seen.isEmpty(), qPrintable(log.seen.join(u", "_s)));
    }
};

QTEST_MAIN(IntegrationVo)
#include "it_vo_test.moc"
