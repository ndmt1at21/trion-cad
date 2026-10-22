// Integration test nhóm 5, 6 (khung ứng dụng đợt 5, 6): UIA-001 (cửa sổ chính, Clean Screen, workspace), UIA-002 (thanh
// tiêu đề, ô Tìm lệnh), UIA-010 (tab tệp), UIA-102 (ribbon JSON, điều khiển), UIA-005 (khung ribbon RibbonBar), UIA-107
// (dòng lệnh), UIA-108 (tự hoàn thành, "Ý bạn là"), UIA-112 (nút chế độ), UIA-009 (thanh trạng thái), UIA-004 (tab
// Start), UI0-013 (trợ năng) lắp vào cửa sổ chính bằng chính Application của exe trion (app/main/ShellAssembly): Store
// tạm có sẵn 3 tệp gần đây, phiên bản đặt tường minh, hai màn hình giả của plugin offscreen (1920×1080 Wide, 1366×768
// Small). Lệnh đi tới bộ định tuyến tạm (CommandRouter): lệnh chưa có xử lý tới tín hiệu unhandled, test ghi lại cùng
// tài liệu đang chọn. Mỗi kịch bản bắt đầu ở tab Start như lúc mở app; cần vùng vẽ thì mở tài liệu giả qua chỗ nối của
// tab tệp (DOC-018 chưa có).
//   1 startup           tab Start chọn, trang Start hiện trong vùng Canvas, 3 tệp gần đây; rảnh 2 s khi ảnh đã xong
//   2 regions           Pro/Viewer, trang Start/vùng vẽ: vùng là widget thật, tỉ lệ vùng vẽ, a11y::audit
//   3 cleanScreen       Ctrl+0 bật/tắt: hình học từng vùng, ảnh cửa sổ từng pixel, dòng lịch sử
//   4 ribbon            3 mức thu gọn (cao, tỉ lệ vùng vẽ); Alt/Esc; RIBBONCLOSE/RIBBON gõ ở dòng lệnh, Clean Screen;
//                       chuột phải "Thêm vào thanh truy cập nhanh" → addToQatRequested, nút mới trên QAT (UIA-101);
//                       F1 → '_HELP
//   5 commandLineModes  Ctrl+9 (CLISTATE); F8 và nút ORTHO của thanh trạng thái; LWDISPLAY ribbon/thanh trạng thái;
//                       bấm ô tọa độ → 'COORDS n
//   6 commandSearch     "đường tròn" → CIRCLE, vị trí ribbon; Enter → ^C^C_CIRCLE
//   7 commandLineInput  "LINE" + Enter; chip và chữ tắt cho cùng token; Esc bỏ prompt
//   8 unknownCommand    "LNE" → "Ý bạn là" bấm LINE; Viewer: dòng tự hoàn thành của lệnh Pro có huy hiệu khóa
//   9 fileTabs          chuột giữa → CLOSE cho đúng tài liệu; Ctrl+Tab; thả .dwg/.pdf
//  10 startPage         chọn tài liệu → trang ẩn; NEWTAB → trang hiện; đóng tab cuối; bấm tệp gần đây → requestOpen
//  11 languages         vi → en → vi mọi vùng dịch lại; vi-XL ở 1366×768 không cắt chữ (ribbon đầy đủ, thanh trạng
//                       thái tràn vào menu), dòng lệnh compact
//  12 viewer            workspace viewer, nút của thanh trạng thái theo hồ sơ, lệnh Pro khóa trong ô Tìm lệnh
//  13 jobFailureToast   tác vụ nền lỗi → toast lỗi trên cửa sổ chính (ProgressPresenter, followJobs)
//  14 idle              rảnh 2 s sau mọi kịch bản, trang Start hiện: không timer nào chạy

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
#include "app/shell/cmdline/Autocomplete.h"
#include "app/shell/cmdline/CommandLine.h"
#include "app/shell/CommandRouter.h"
#include "app/shell/filetabs/FileTabBar.h"
#include "app/shell/MainWindow.h"
#include "app/shell/ribbon/controls/RibbonControls.h"
#include "app/shell/ribbon/RibbonBar.h"
#include "app/shell/start/RecentFiles.h"
#include "app/shell/start/StartPage.h"
#include "app/shell/status/ModeToggles.h"
#include "app/shell/status/StatusBar.h"
#include "app/shell/titlebar/CommandSearch.h"
#include "app/shell/titlebar/Qat.h"
#include "app/shell/titlebar/TitleBar.h"
#include "app/shell/Workspace.h"
#include "app/theme/generated/Tokens.h"
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
#include <QAbstractScrollArea>
#include <QAccessible>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDir>
#include <QDockWidget>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMenu>
#include <QMetaEnum>
#include <QMimeData>
#include <QPainter>
#include <QScreen>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTabBar>
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
using trion::ribbon::RibbonBar;
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
constexpr int kLine = 1;                    // nét dưới ribbon (prototype viết thẳng, như RibbonBar.cpp)

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

// Focus vào w trên cửa sổ đã kích hoạt (w có focus proxy, như vùng Canvas: proxy nhận). Plugin offscreen kích hoạt cả
// cửa sổ popup lúc hiện và không trả lại khi popup đóng: cửa sổ focus còn là popup QCompleter đã ẩn, lần kích hoạt cửa
// sổ chính sau đó đưa focus về proxy của popup (ô Tìm lệnh). Nền tảng thật không kích hoạt popup.
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

int px(tok::PxId id)
{
    return qRound(trion::theme::ThemeManager::instance().px(id));
}

// Cao vùng Ribbon theo mức thu gọn, tính từ token độc lập với RibbonBar.cpp: dải tab, thân (3 hàng nút, --sp-4, tên
// panel; tên panel, --sp-4; không có), nét dưới.
int ribbonHeight(RibbonBar::Collapse c)
{
    const int tabs = px(tok::h_tab) + kLine;
    switch (c) {
    case RibbonBar::Collapse::Full:
        return tabs + 3 * px(tok::h_btn) + px(tok::sp_4) + px(tok::h_lbl);
    case RibbonBar::Collapse::PanelTitles:
        return tabs + px(tok::h_lbl) + px(tok::sp_4);
    case RibbonBar::Collapse::TabNames:
        break;
    }
    return tabs;
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

// Rảnh d: QTimerEvent tới bất kỳ đối tượng nào (timer định kỳ, polling, tooltip, toast).
QStringList timersDuring(std::chrono::milliseconds d)
{
    QTimer end;
    end.setSingleShot(true);
    TimerLog log(&end);
    qApp->installEventFilter(&log);
    QEventLoop loop;
    QObject::connect(&end, &QTimer::timeout, &loop, &QEventLoop::quit);
    end.start(d);
    loop.exec();
    qApp->removeEventFilter(&log);
    return log.seen;
}

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
    QStringList recent_;  // 3 tệp gần đây có sẵn trong Store (DWG 2018 thật trên đĩa)
    QStringList sent_;    // chuỗi lệnh tới router chưa có xử lý (INT-002 sẽ nhận)
    QList<quint64> docs_; // tài liệu đang chọn ở tab tệp lúc router nhận từng chuỗi
    QList<QUrl> opened_;  // trang nâng cấp đã mở (thay QDesktopServices)

    QStringList take()
    {
        docs_.clear();
        return std::exchange(sent_, {});
    }
    QWidget* region(Region r) const { return window_->region(r); }
    QWidget* canvas() const { return region(Region::Canvas); } // ngăn xếp {vùng vẽ giữ chỗ, trang Start}
    ui::PaletteHost& palettes() const { return window_->palettes(); }

    // Tài liệu giả qua chỗ nối của tab tệp (DOC-018 chưa có), chọn nó: vùng vẽ thay trang Start.
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

    // Nút chế độ đang hiện của thanh trạng thái, theo thứ tự (id hành động của registry).
    QStringList visibleModes() const
    {
        QStringList ids;
        for (const QToolButton* b : shell_->modes->findChildren<QToolButton*>(Qt::FindDirectChildrenOnly))
            if (b->isVisibleTo(shell_->modes))
                ids << b->defaultAction()->objectName();
        return ids;
    }

    // Nút ribbon đang hiện có id điều khiển id (objectName), trong tab đang mở.
    trion::ribbon::RibbonButton* ribbonButton(const QString& id) const
    {
        for (auto* b : shell_->ribbon->pages()->currentWidget()->findChildren<trion::ribbon::RibbonButton*>(id))
            if (b->isVisible())
                return b;
        return nullptr;
    }

    // Trang Start đã kiểm lại tệp ở nền và ảnh xem trước của thẻ đang thấy đã xong (có ảnh hoặc lỗi).
    bool startSettled() const
    {
        return !shell_->start->recent().refreshing() && !shell_->start->thumbnails().busy();
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

        // Phiên trước để lại 3 tệp gần đây (DWG 2018 thật trên đĩa), như RecentFiles ghi vào Store.
        trion::app::Options options;
        options.settingsPath = dir_->filePath(u"user.json"_s);
        {
            QJsonArray list;
            for (const char* name : {"Nha-pho-A.dwg", "Ket-cau-T2.dwg", "Mat-bang.dwg"}) {
                const QString path = dir_->filePath(QString::fromLatin1(name));
                QFile f(path);
                QVERIFY(f.open(QIODevice::WriteOnly) && f.write(QByteArray("AC1032") + QByteArray(122, '\0')) == 128);
                recent_ << path;
                list.append(QJsonObject{{u"path"_s, path}});
            }
            trion::settings::Store seed(options.settingsPath);
            seed.setValue(u"recent/files"_s, list);
            seed.flush();
        }

        // Như main.cpp: Application dựng mọi phần theo thứ tự khởi động, cửa sổ chính phóng to trên màn hình chính.
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
        // EditionGate, RecentFiles, rồi chờ việc nền và ghi cài đặt.
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

    // Sau mỗi kịch bản, kể cả khi trượt giữa chừng: đóng popup, toast, bỏ prompt, về tab Start như lúc mở app, focus
    // về vùng Canvas (trang đang hiện).
    void cleanup()
    {
        while (QWidget* popup = QApplication::activePopupWidget())
            popup->close();
        for (ui::Toast* t : shell_->toasts->toasts())
            t->closeButton()->click();
        spinUntil([&] { return shell_->toasts->toasts().isEmpty(); }, 5s);
        if (shell_->commandLine->hasPrompt())
            shell_->commandLine->clearPrompt();
        shell_->commandLine->input()->clear();
        closeDocuments();
        focusOn(canvas());
    }

    // Kịch bản 1 — mở app: tab Start chọn, trang Start hiện trong vùng Canvas thay vùng vẽ và nhận focus của vùng (không
    // ô nhập: con trỏ chữ nhấp nháy là timer); 3 tệp gần đây của Store hiện, kiểm lại ở nền (phiên bản đọc từ tệp); ảnh
    // xem trước từ nguồn ảnh nhúng giả (DWG-026 chưa có) điền vào thẻ; xong thì rảnh 2 s không timer nào chạy.
    void startup()
    {
        const double ready = trace_->at(u"ready"_s).value_or(0), frame = trace_->at(u"first_frame"_s).value_or(0);
        qInfo("dựng cửa sổ chính có trang Start, 3 tệp gần đây → khung đầu: %.1f ms (mốc ready → first_frame)",
              frame - ready);
        shell::StartPage* start = shell_->start;
        QCOMPARE(shell_->fileTabs->current(), shell::FileTabBar::kStart);
        auto* stack = qobject_cast<QStackedWidget*>(canvas());
        QVERIFY(stack);
        QCOMPARE(stack->currentWidget(), start);
        QVERIFY(start->isVisible() && !shell_->canvas->isVisible());
        QCOMPARE(start->geometry(), stack->rect());
        QWidget* focus = QApplication::focusWidget();
        QVERIFY2(start->isAncestorOf(focus) && !qobject_cast<QLineEdit*>(focus),
                 focus ? focus->metaObject()->className() : "null");
        QCOMPARE(start->recent().size(), 3);
        QCOMPARE(start->filter().rowCount(), 3);
        QVERIFY(spinUntil([&] { return startSettled(); }));
        for (const QString& path : std::as_const(recent_)) {
            const int row = start->recent().indexOf(path);
            QVERIFY2(row >= 0, qPrintable(path));
            QCOMPARE(start->recent().entry(row).version, u"DWG 2018"_s); // kiểm tệp ở nền đã áp
        }

        start->thumbnails().setEmbeddedSource([](const QString&, sched::CancelToken) {
            QImage image(160, 100, QImage::Format_RGB32);
            image.fill(Qt::darkGray);
            return image;
        });
        openDocument(); // rời tab Start rồi về: trang xin ảnh cho thẻ đang thấy
        closeDocuments();
        QVERIFY(spinUntil([&] {
            return startSettled() && std::ranges::all_of(recent_, [&](const QString& path) {
                       return start->thumbnails().image(path, start->thumbnailSize()) != nullptr;
                   });
        }));

        QVERIFY(focusOn(canvas()));
        QTest::mouseMove(canvas(), canvas()->rect().center());
        if (ui::RichTooltip* tips = ui::RichTooltip::instance())
            tips->leave();
        sched::waitIdle();
        spinFor(3s); // vẽ, bố cục, timer một lần của lần di chuột cuối (tooltip 700 ms, "ngủ" 2 s) đã chạy
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        const QStringList timers = timersDuring(2s);
        QVERIFY2(timers.isEmpty(), qPrintable(timers.join(u", "_s)));
    }

    // Kịch bản 2 — 1920×1080, Pro và Viewer, trang Start hoặc vùng vẽ: mọi vùng là widget thật trừ vùng vẽ và tab layout
    // (UIA-109, UIA-011 chưa có), xếp liền nhau, tỉ lệ vùng vẽ, a11y::audit của cả cửa sổ (tên, vai trò, thứ tự Tab, bẫy
    // focus, viền focus).
    void regions_data()
    {
        QTest::addColumn<bool>("viewer");
        QTest::addColumn<bool>("start");
        QTest::addRow("pro-start") << false << true;
        QTest::addRow("pro-canvas") << false << false;
        QTest::addRow("viewer-start") << true << true;
        QTest::addRow("viewer-canvas") << true << false;
    }

    void regions()
    {
        QFETCH(bool, viewer);
        QFETCH(bool, start);
        if (viewer)
            shell_->gate.setEdition(Edition::Viewer);
        if (!start)
            openDocument();
        QVERIFY(window_->isMaximized());
        QCOMPARE(window_->frameGeometry(), kWide); // offscreen có khung 2 px
        QCOMPARE(window_->screenClass(), ui::ScreenClass::Wide);
        QCOMPARE(window_->workspace().current(), viewer ? u"viewer"_s : u"2d"_s);
        QVERIFY(qobject_cast<shell::TitleBar*>(region(Region::TitleBar)));
        QCOMPARE(region(Region::Ribbon), shell_->ribbon);
        QVERIFY(qobject_cast<shell::FileTabBar*>(region(Region::FileTabs)));
        auto* stack = qobject_cast<QStackedWidget*>(canvas());
        QVERIFY(stack && stack->count() == 2 && stack->widget(0) == shell_->canvas && stack->widget(1) == shell_->start);
        QCOMPARE(stack->currentWidget(), start ? static_cast<QWidget*>(shell_->start) : shell_->canvas);
        QVERIFY(qobject_cast<ui::CommandLine*>(region(Region::CommandLine)));
        QCOMPARE(region(Region::StatusBar), shell_->statusBar);
        QVERIFY(shell_->modes && shell_->statusBar->isAncestorOf(shell_->modes));
        for (const QWidget* w : {shell_->canvas, region(Region::LayoutTabs)}) // ô giữ chỗ của MainWindow
            QCOMPARE(w->metaObject(), &QWidget::staticMetaObject);
        int y = 0; // xếp liền nhau từ trên xuống, vùng thật có chiều cao
        for (int i = 0; i < MainWindow::kRegionCount; ++i) {
            const QWidget* w = Region(i) == Region::Canvas ? window_->dockHost() : region(Region(i));
            const QRect g(w->mapTo(window_, QPoint()), w->size());
            QVERIFY2(g.y() == y && g.height() > 0 && g.width() == window_->width(), qPrintable(regionKey(Region(i))));
            y += g.height();
        }
        QCOMPARE(y, window_->height());
        QCOMPARE(shell_->modes->button(u"ortho"_s)->isVisible(), !viewer);
        QVERIFY(shell_->ribbon->pages()->isBuilt(0) && !shell_->ribbon->pages()->isBuilt(1)); // tab khác dựng khi mở

        const double share = canvasShare();
        QStringList heights;
        for (int i = 0; i < MainWindow::kRegionCount; ++i)
            if (Region(i) != Region::Canvas)
                heights << u"%1 %2"_s.arg(regionKey(Region(i))).arg(region(Region(i))->height());
        qInfo("1920×1080 %s: vùng vẽ %d×%d = %.1f%% diện tích cửa sổ (%s)", QTest::currentDataTag(), canvas()->width(),
              canvas()->height(), share * 100, qPrintable(heights.join(u", "_s)));
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
        qInfo("a11y %s: %lld điểm dừng Tab", QTest::currentDataTag(), qlonglong(order.size()));
        QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
    }

    // Kịch bản 3 — Ctrl+0 với widget thật, vùng vẽ hiện: bật ẩn thanh tiêu đề, ribbon, tab tệp, tab layout, palette;
    // tắt trả đúng hình học từng vùng, palette, dải và ảnh cửa sổ từng pixel (trừ dòng lệnh); mỗi lần một dòng lịch sử.
    void cleanScreen()
    {
        openDocument();
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

    // Kịch bản 4 — ribbon (RibbonBar) trong cửa sổ thật: 3 mức thu gọn → cao vùng Ribbon đúng token, tỉ lệ vùng vẽ;
    // Alt vào dải tab, Esc trả focus về vùng vẽ; RIBBONCLOSE gõ ở dòng lệnh ẩn vùng (RIBBONSTATE 0, Alt không vào ribbon
    // ẩn), RIBBON hiện lại đúng mức thu gọn cũ; Clean Screen độc lập; chuột phải trên nút LINE → "Thêm vào thanh truy cập
    // nhanh" phát addToQatRequested("LINE"), QAT (UIA-101) có nút LINE.
    void ribbon()
    {
        RibbonBar* bar = shell_->ribbon;
        QCOMPARE(bar->collapse(), RibbonBar::Collapse::Full);
        openDocument();
        for (const auto& [c, name] : {std::pair{RibbonBar::Collapse::Full, "đầy đủ"},
                                      std::pair{RibbonBar::Collapse::PanelTitles, "chỉ tên panel"},
                                      std::pair{RibbonBar::Collapse::TabNames, "chỉ tên tab"}}) {
            bar->setCollapse(c);
            settle();
            QCOMPARE(region(Region::Ribbon)->height(), ribbonHeight(c));
            qInfo("1920×1080 ribbon %s: %d px, vùng vẽ %.1f%%", name, region(Region::Ribbon)->height(),
                  canvasShare() * 100);
        }
        bar->setCollapse(RibbonBar::Collapse::Full);

        QVERIFY(focusOn(shell_->canvas));
        QTest::keyClick(shell_->canvas, Qt::Key_Alt);
        QCOMPARE(QApplication::focusWidget(), bar->tabBar());
        key(Qt::Key_Escape);
        QCOMPARE(QApplication::focusWidget(), shell_->canvas);

        bar->setCollapse(RibbonBar::Collapse::PanelTitles);
        settle();
        const int height = canvas()->height();
        ui::CommandLine* cmd = shell_->commandLine;
        QVERIFY(focusOn(cmd->input()));
        type(cmd->input(), u"RIBBONCLOSE"_s);
        key(Qt::Key_Return);
        QVERIFY(!window_->isRegionVisible(Region::Ribbon));
        QCOMPARE(shell_->sysvars.get(u"RIBBONSTATE"_s), 0);
        settle();
        QCOMPARE(canvas()->height(), height + ribbonHeight(RibbonBar::Collapse::PanelTitles));
        QVERIFY(focusOn(shell_->canvas));
        QTest::keyClick(shell_->canvas, Qt::Key_Alt); // ribbon ẩn: Alt không vào
        QCOMPARE(QApplication::focusWidget(), shell_->canvas);
        QVERIFY(focusOn(cmd->input()));
        type(cmd->input(), u"RIBBON"_s);
        key(Qt::Key_Return);
        QVERIFY(window_->isRegionVisible(Region::Ribbon));
        QCOMPARE(shell_->sysvars.get(u"RIBBONSTATE"_s), 1);
        QCOMPARE(bar->collapse(), RibbonBar::Collapse::PanelTitles);
        settle();
        QCOMPARE(region(Region::Ribbon)->height(), ribbonHeight(RibbonBar::Collapse::PanelTitles));
        QCOMPARE(canvas()->height(), height);

        // Clean Screen độc lập (RIBBONCLOSE.md): đóng lúc đang bật thì tắt Clean Screen ribbon vẫn ẩn; RIBBON lúc đang
        // bật chỉ ghi nhận, tắt thì hiện.
        QTest::keyClick(window_, Qt::Key_0, Qt::ControlModifier);
        shell_->router.submit(u"RIBBONCLOSE"_s);
        QTest::keyClick(window_, Qt::Key_0, Qt::ControlModifier);
        QVERIFY(!shell_->clean.isOn() && !window_->isRegionVisible(Region::Ribbon));
        QTest::keyClick(window_, Qt::Key_0, Qt::ControlModifier);
        shell_->router.submit(u"'_RIBBON"_s); // macro trong suốt của menu
        QVERIFY(!window_->isRegionVisible(Region::Ribbon));
        QTest::keyClick(window_, Qt::Key_0, Qt::ControlModifier);
        QVERIFY(!shell_->clean.isOn() && window_->isRegionVisible(Region::Ribbon));
        bar->setCollapse(RibbonBar::Collapse::Full);
        settle();
        QCOMPARE(take(), QStringList()); // RIBBON, RIBBONCLOSE do giao diện chạy

        QSignalSpy qat(bar, &RibbonBar::addToQatRequested);
        trion::ribbon::RibbonButton* line = ribbonButton(u"LINE"_s);
        QVERIFY(line);
        QContextMenuEvent e(QContextMenuEvent::Mouse, line->rect().center(), line->mapToGlobal(line->rect().center()));
        QApplication::sendEvent(line, &e);
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        QVERIFY(menu);
        QAction* add = menu->actions().value(0);
        QVERIFY(add);
        QCOMPARE(add->text(), QCoreApplication::translate("trion::ribbon::RibbonBar", "Thêm vào thanh truy cập nhanh"));
        QTest::mouseClick(menu, Qt::LeftButton, {}, menu->actionGeometry(add).center());
        QCOMPARE(qat.count(), 1);
        QCOMPARE(qat[0][0].toString(), u"LINE"_s);
        const ui::Button* added = shell_->titleBar->qat()->button(u"LINE"_s); // nút mới trên QAT (UIA-101)
        QVERIFY(added);
        QVERIFY(spinUntil([&] { return added->isVisible(); }));
        QVERIFY(spinUntil([] { return !QApplication::activePopupWidget(); }));
        QCOMPARE(take(), QStringList());

        // F1 khi rê nút (RichTooltip phát helpRequested, test của nó kiểm phím) → '_HELP <lệnh> tới router (UIL-009).
        QVERIFY(ui::RichTooltip::instance());
        emit ui::RichTooltip::instance()->helpRequested(u"LINE"_s);
        QCOMPARE(take(), QStringList{u"'_HELP LINE"_s});
    }

    // Kịch bản 5 — Ctrl+9 ẩn/hiện dòng lệnh (CLISTATE theo vùng, vùng vẽ giãn ra phần đó, hiện lại thì focus ở ô
    // nhập); F8 đổi ORTHOMODE và nút ORTHO của thanh trạng thái ngay trong lượt phím; LWDISPLAY: nút thanh trạng thái và
    // toggle ribbon (tab Hiển thị) cùng đổi trong lượt bấm của nhau; bấm ô tọa độ: 'COORDS n qua QAction tới router.
    void commandLineModes()
    {
        openDocument();
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
        QCOMPARE(QApplication::focusWidget(), shell_->canvas);
        QTest::keyClick(window_, Qt::Key_9, Qt::ControlModifier);
        QVERIFY(window_->isRegionVisible(Region::CommandLine));
        QCOMPARE(take(), QStringList());
        QVERIFY(focusOn(canvas()));

        QToolButton* ortho = shell_->modes->button(u"ortho"_s);
        QVERIFY(ortho && shell_->statusBar->isAncestorOf(ortho) && ortho->isVisible());
        for (const bool on : {true, false}) {
            QTest::keyClick(window_, Qt::Key_F8); // không chạy vòng sự kiện giữa phím và lúc kiểm
            QCOMPARE(vars.get(u"ORTHOMODE"_s), on ? 1 : 0);
            QCOMPARE(ortho->isChecked(), on);
        }

        const auto& tabs = shell_->ribbonModel.tabs();
        const auto view = std::ranges::find(tabs, u"view"_s, &trion::ribbon::Tab::id);
        QVERIFY(view != tabs.end());
        shell_->ribbon->setCurrentTab(int(view - tabs.begin()));
        settle();
        trion::ribbon::RibbonButton* ribbonLw = ribbonButton(u"lwtgl"_s);
        QToolButton* modeLw = shell_->modes->button(u"LWDISPLAY"_s);
        QVERIFY(ribbonLw && modeLw);
        const bool lw = ribbonLw->isChecked();
        QCOMPARE(modeLw->isChecked(), lw);
        QTest::mouseClick(modeLw, Qt::LeftButton);
        QCOMPARE(ribbonLw->isChecked(), !lw);
        QCOMPARE(modeLw->isChecked(), !lw);
        QTest::mouseClick(ribbonLw, Qt::LeftButton);
        QCOMPARE(ribbonLw->isChecked(), lw);
        QCOMPARE(modeLw->isChecked(), lw);
        QCOMPARE(take(), QStringList()); // 'ORTHOMODE 1, 'LWDISPLAY … là sysvar trong suốt: router đặt Sysvars
        shell_->ribbon->setCurrentTab(0);

        // Ô tọa độ: Application đặt COORDS 1 (tuyệt đối, như AutoCAD); bấm = QAction "coord" (như Ctrl+I) → sink →
        // router: 'COORDS n là sysvar trong suốt, router đặt Sysvars, thanh đổi chữ theo nó.
        auto* coord = shell_->statusBar->findChild<QAbstractButton*>(u"coord"_s);
        QVERIFY(coord && coord->isVisible());
        QCOMPARE(vars.get(u"COORDS"_s), 1);
        for (const int next : {2, 0, 1}) {
            QCOMPARE(shell_->actions.macro(u"coord"_s), u"'COORDS %1"_s.arg(next));
            QTest::mouseClick(coord, Qt::LeftButton);
            QCOMPARE(vars.get(u"COORDS"_s), next);
            if (next == 0) // ô vẽ lại ở khung kế tiếp (tối đa một lần mỗi khung)
                QVERIFY(spinUntil([&] {
                    return shell_->statusBar->shownCoord() ==
                           QCoreApplication::translate("trion::shell::StatusBar", "Tọa độ tắt");
                }));
        }
        QCOMPARE(take(), QStringList());
    }

    // Kịch bản 6 — ô Tìm lệnh: "đường tròn" → CIRCLE đầu danh sách, dòng có vị trí ribbon từ RibbonModel; Enter chạy
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

    // Kịch bản 7 — dòng lệnh: gõ tên lệnh rồi Enter tới router (lệnh có thật: không "Ý bạn là"); prompt giả có chip:
    // bấm chip và gõ chữ tắt (cả "_" + tên toàn cục) cho cùng token; Esc khi có prompt bỏ prompt (cancelRequested →
    // clearPrompt).
    void commandLineInput()
    {
        ui::CommandLine* cmd = shell_->commandLine;
        QVERIFY(focusOn(cmd->input()));
        type(cmd->input(), u"LINE"_s);
        key(Qt::Key_Return);
        QCOMPARE(take(), QStringList{u"LINE"_s});
        QVERIFY(cmd->input()->text().isEmpty());
        QCOMPARE(cmd->historyLine(cmd->historyCount() - 1),
                 QCoreApplication::translate("trion::ui::CommandLine", "Lệnh: %1").arg(u"LINE"_s));

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
        const int lines = cmd->historyCount();
        type(cmd->input(), u"Q"_s); // trả lời prompt không phải tên lệnh: không "Ý bạn là"
        key(Qt::Key_Return);
        QCOMPARE(take(), QStringList{u"Q"_s});
        QCOMPARE(cmd->historyCount(), lines + 1);
        key(Qt::Key_Escape);
        QVERIFY(!cmd->hasPrompt());
        QCOMPARE(take(), QStringList());
    }

    // Kịch bản 8 — tên lệnh gõ sai: "LNE" + Enter tới router (chưa có lệnh), lịch sử có "Không có lệnh “LNE”. Ý bạn là:
    // LINE…" với LINE bấm được; bấm → router nhận ^C^CLINE. Viewer: gõ tên lệnh Pro, dòng tự hoàn thành có huy hiệu
    // khóa (Pro: không).
    void unknownCommand()
    {
        ui::CommandLine* cmd = shell_->commandLine;
        QVERIFY(focusOn(cmd->input()));
        type(cmd->input(), u"LNE"_s);
        key(Qt::Key_Return);
        QCOMPARE(take(), QStringList{u"LNE"_s});
        const QString line = cmd->historyLine(cmd->historyCount() - 1);
        const QString head =
            QCoreApplication::translate("trion::ui::CommandLine", "Không có lệnh “%1”.").arg(u"LNE"_s);
        QVERIFY2(line.startsWith(head) && line.contains(u"LINE"_s), qPrintable(line));

        // Gợi ý là chip trong dòng lịch sử mới nhất: tìm chỗ con trỏ thành bàn tay (như autocomplete_test), bấm.
        auto* history = cmd->findChild<QAbstractScrollArea*>(u"history"_s);
        QVERIFY(history);
        QWidget* port = history->viewport();
        port->repaint(); // chỗ của chip có sau lần vẽ
        const int h = px(tok::h_cmdl), count = cmd->historyCount();
        const int y = (count - 1 - std::max(0, count - port->height() / h)) * h + h / 2;
        const auto hover = [&](QPoint at) { QTest::mouseMove(window_->windowHandle(), port->mapTo(window_, at)); };
        int x = 0;
        for (; x < port->width(); x += 2) {
            hover({x, y});
            if (port->cursor().shape() == Qt::PointingHandCursor)
                break;
        }
        QVERIFY2(x < port->width(), "không thấy chip gợi ý");
        QTest::mouseClick(port, Qt::LeftButton, {}, {x + 4, y});
        QCOMPARE(take(), QStringList{u"^C^CLINE"_s});
        hover({1, 1});

        // Huy hiệu khóa của tự hoàn thành theo phiên bản (setLockQuery → RichTooltip::info(QAction).need).
        ui::Autocomplete* list = cmd->autocomplete();
        for (const Edition e : {Edition::Pro, Edition::Viewer}) {
            shell_->gate.setEdition(e);
            QVERIFY(focusOn(cmd->input()));
            cmd->input()->clear();
            type(cmd->input(), u"CIRCLE"_s);
            QVERIFY(spinUntil([&] { return list->isVisible(); }));
            const QModelIndex first = list->view()->model()->index(0, 0);
            QCOMPARE(first.data(Qt::DisplayRole).toString(), u"CIRCLE"_s);
            const QString said = first.data(Qt::AccessibleTextRole).toString();
            const QString badge = ui::RichTooltip::editionName(ui::Edition::Pro);
            QVERIFY2(said.endsWith(u", "_s + badge) == (e == Edition::Viewer), qPrintable(said));
            key(Qt::Key_Escape); // đóng danh sách, giữ chữ
            QVERIFY(!list->isVisible());
        }
        QCOMPARE(take(), QStringList());
    }

    // Kịch bản 9 — tab tệp: 3 tài liệu qua chỗ nối (DOC-018 chưa có); chuột giữa trên tab thứ hai: tài liệu đó thành
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
        const int notes = shell_->notes.rowCount();
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
        QCOMPARE(shell_->notes.rowCount(), notes + 1);
        QCOMPARE(take(), QStringList());
    }

    // Kịch bản 10 — tab Start (NEWTAB.md): chọn tài liệu (chỗ nối) → trang ẩn, vùng vẽ hiện và nhận focus của vùng; gõ
    // NEWTAB → tab Start, trang hiện, tài liệu giữ tab; hành động NEWTAB (^C^C_NEWTAB của menu, ribbon) như gõ; đóng tab
    // bản vẽ cuối → tab Start; bấm thẻ tệp gần đây → Application::requestOpen → openRequested đúng đường dẫn.
    void startPage()
    {
        shell::StartPage* start = shell_->start;
        shell::FileTabBar* tabs = shell_->fileTabs;
        QVERIFY(start->isVisible());
        openDocument(7);
        QVERIFY(!start->isVisible() && shell_->canvas->isVisible());
        QVERIFY(focusOn(canvas()));
        QCOMPARE(QApplication::focusWidget(), shell_->canvas);

        ui::CommandLine* cmd = shell_->commandLine;
        QVERIFY(focusOn(cmd->input()));
        type(cmd->input(), u"NEWTAB"_s);
        key(Qt::Key_Return);
        QCOMPARE(tabs->current(), shell::FileTabBar::kStart);
        QVERIFY(start->isVisible() && !shell_->canvas->isVisible());
        QCOMPARE(tabs->documents(), QList<quint64>{7});
        tabs->setCurrent(7);
        QVERIFY(!start->isVisible());
        QAction* newTab = shell_->actions.action(u"NEWTAB"_s); // ^C^C_NEWTAB của menu, ribbon
        QVERIFY(newTab);
        newTab->trigger();
        QCOMPARE(tabs->current(), shell::FileTabBar::kStart);
        QVERIFY(start->isVisible());
        tabs->setCurrent(7);
        tabs->removeDocument(7); // tab bản vẽ cuối đóng: tab Start
        QCOMPARE(tabs->current(), shell::FileTabBar::kStart);
        QVERIFY(start->isVisible());
        QCOMPARE(take(), QStringList()); // NEWTAB do giao diện chạy
        auto* openFile = start->findChild<QAbstractButton*>(u"openFile"_s); // Mở tệp…: macroRequested → router
        QVERIFY(openFile);
        QTest::mouseClick(openFile, Qt::LeftButton);
        QCOMPARE(take(), QStringList{u"^C^C_OPEN"_s});

        QVERIFY(spinUntil([&] { return startSettled(); }));
        QSignalSpy open(app_.get(), &Application::openRequested);
        QAbstractItemView* view = start->recentView();
        QVERIFY(view && view->isVisible());
        const QString path = start->filter().entry(1).path;
        QVERIFY(recent_.contains(path));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, {}, view->visualRect(view->model()->index(1, 0)).center());
        QCOMPARE(open.count(), 1);
        QCOMPARE(open[0][0].toStringList(), QStringList{path});
        QCOMPARE(take(), QStringList());
    }

    // Kịch bản 11 — đổi ngôn ngữ khi đang chạy (trang Start hiện): mọi vùng dịch lại (chữ trình đọc màn hình đọc được của
    // vùng đổi ở en, về đúng như cũ ở vi); kéo cửa sổ sang màn hình 1366×768: dòng lệnh compact, ở vi-XL không vùng nào
    // cắt chữ, kể cả ribbon mức đầy đủ (panel co theo ưu tiên) và thanh trạng thái (mục tràn vào menu »).
    void languages()
    {
        QVERIFY(shell_->start->isVisible());
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
        QCOMPARE(shell_->ribbon->collapse(), RibbonBar::Collapse::Full);
        qInfo("1366×768 vi-XL: vùng vẽ %.1f%%, thanh trạng thái tràn vào menu: %s", canvasShare() * 100,
              qPrintable(shell_->statusBar->overflowed().join(u", "_s)));
        QVERIFY2(i18n::findTruncations(window_).isEmpty(), qPrintable(truncations(window_)));

        QVERIFY(translator->setLanguage(u"vi"_s));
        QCoreApplication::processEvents();
        window_->setGeometry(kWide);
        window_->showMaximized();
        QVERIFY(spinUntil([&] { return window_->screenClass() == ui::ScreenClass::Wide; }));
        QVERIFY(!shell_->commandLine->isCompact());
        settle();
    }

    // Kịch bản 12 — Viewer: workspace viewer, thanh trạng thái còn đúng 3 nút chế độ của hồ sơ (OSNAP, lineweight, độ
    // trong suốt) và huy hiệu Viewer, lệnh Pro trong ô Tìm lệnh có huy hiệu khóa, Enter mở trang nâng cấp thay vì gửi
    // lệnh.
    void viewer()
    {
        shell_->gate.setEdition(Edition::Viewer);
        QCOMPARE(window_->workspace().current(), u"viewer"_s);
        settle();
        QCOMPARE(visibleModes(), (QStringList{u"osnap"_s, u"LWDISPLAY"_s, u"TRANSPARENCYDISPLAY"_s}));
        auto* badge = shell_->statusBar->findChild<QToolButton*>(u"edition"_s);
        QVERIFY(badge);
        QCOMPARE(badge->text(), QCoreApplication::translate("trion::shell::TitleBar", "Viewer"));

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
        settle();
        QCOMPARE(visibleModes().size(), qsizetype(10)); // 9 nút EA + Isometric (P1, hành động "iso" gắn SNAPSTYL)
        QCOMPARE(badge->text(), QCoreApplication::translate("trion::shell::TitleBar", "Pro"));
    }

    // Kịch bản 13 — tác vụ nền lỗi trên registry của ứng dụng (BackgroundJobs::instance()): ProgressPresenter (cha là
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

    // Kịch bản 14 — rảnh sau mọi kịch bản trên, trang Start hiện (ảnh tệp gần đây xong), focus ở vùng Canvas, chuột trên
    // vùng: 2 s không QTimerEvent nào tới bất kỳ đối tượng nào (timer định kỳ, polling, tooltip, toast), không việc nền.
    void idle()
    {
        QVERIFY(shell_->start->isVisible());
        QTest::mouseMove(canvas(), canvas()->rect().center());
        if (ui::RichTooltip* tips = ui::RichTooltip::instance())
            tips->leave();
        QVERIFY(focusOn(canvas()));
        QVERIFY(spinUntil([&] { return trion::jobs::BackgroundJobs::instance()->size() == 0 && startSettled(); }));
        sched::waitIdle();
        // Việc còn chờ (vẽ, bố cục, raster trước) xong; timer một lần của Qt sau lần di chuột cuối (tooltip 700 ms, rồi
        // tooltip "ngủ" 2 s của QApplication khi chuột trên thẻ của trang Start) đã chạy.
        spinFor(3s);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        const QStringList timers = timersDuring(2s);
        QVERIFY2(timers.isEmpty(), qPrintable(timers.join(u", "_s)));
    }
};

QTEST_MAIN(IntegrationVo)
#include "it_vo_test.moc"
