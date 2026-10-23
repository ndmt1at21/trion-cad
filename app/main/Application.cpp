#include "app/main/Application.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/core/display/DisplayContext.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/i18n/Translator.h"
#include "app/core/keymap/Keymap.h"
#include "app/core/UiExecutor.h"
#include "app/main/ShellAssembly.h"
#include "app/main/StartupTrace.h"
#include "app/shell/appmenu/AppMenu.h"
#include "app/shell/CleanScreen.h"
#include "app/shell/CommandRouter.h"
#include "app/shell/filetabs/FileTabBar.h"
#include "app/shell/MainWindow.h"
#include "app/shell/start/RecentFiles.h"
#include "app/shell/start/StartPage.h"
#include "app/shell/Workspace.h"
#include "app/widgets/dock/PaletteHost.h"
#include "app/widgets/gallery/Gallery.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "core/base/assert.h"
#include "core/base/jobs/BackgroundJobs.h"
#include "core/base/settings/Settings.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMainWindow>
#include <QTimer>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace trion::app {

namespace {

Q_STATIC_LOGGING_CATEGORY(lcApp, "trion.app", QtWarningMsg)

Application* g_instance = nullptr; // chỉ luồng UI

QString absolute(const QString& path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

// Raster trước theo lô, mỗi lô một lượt sự kiện: lượt của luồng UI (xét cache, gửi việc nền cho từng tên) ngắn hơn
// 2 ms cả khi máy tải (lô 64 tên đo được 2,1 ms ở release khi tải nặng; 16 tên ~0,5 ms). context hủy (thoát) thì lô
// còn chờ bị bỏ.
void prewarmBatches(theme::IconProvider& icons, QObject* context, qreal dpr, const QStringList& names)
{
    constexpr qsizetype kBatch = 16;
    icons.prewarm(dpr, names.first(std::min(kBatch, names.size())));
    if (names.size() > kBatch)
        QMetaObject::invokeMethod(
            context, [&icons, context, dpr, rest = names.mid(kBatch)] { prewarmBatches(icons, context, dpr, rest); },
            Qt::QueuedConnection);
}

} // namespace

void Application::addOptions(QCommandLineParser& parser)
{
    parser.addHelpOption();
    parser.addPositionalArgument(
        tr("tệp"), tr("Bản vẽ cần mở; đã có phiên TrionCAD đang chạy thì chuyển cho phiên đó."), tr("[tệp...]"));
    parser.addOption({u"settings"_s,
                      tr("Dùng tệp cài đặt này thay cho tệp của người dùng (mỗi tệp cài đặt một phiên)."), tr("tệp")});
    parser.addOption({u"startup-trace"_s, tr("Ghi mốc các pha khởi động ra tệp JSON."), tr("tệp.json")});
    parser.addOption({u"ui-probe"_s, tr("Đo độ trễ giao diện; --ui-probe=<tệp.json> ghi số đo ra tệp khi thoát.")});
    parser.addOption({u"quit-after-first-frame"_s,
                      tr("Thoát ngay sau khung hình đầu của cửa sổ chính (cho test và đo khởi động).")});
    parser.addOption({u"gallery"_s, tr("Mở bảng mẫu thành phần giao diện thay cho cửa sổ chính.")});
}

Options Application::parseOptions(const QStringList& arguments)
{
    QStringList args = arguments;
    for (QString& a : args) // --ui-probe=<tệp> là của UiProbe; parser của Qt không có tùy chọn giá trị không bắt buộc
        if (a.startsWith(u"--ui-probe="))
            a = u"--ui-probe"_s;
    QCommandLineParser parser;
    addOptions(parser);
    const bool parsed = parser.parse(args);
    Options o;
    const QString settings = parser.value(u"settings"_s);
    o.settingsPath = settings.isEmpty() ? settings::Store::defaultFilePath() : absolute(settings);
    if (!parsed || parser.isSet(u"help"_s) || parser.optionNames().contains(u"help-all"_s)) {
        // Trợ giúp bằng ngôn ngữ đã chọn: nạp cài đặt (chỉ đọc) và bản dịch, dựng lại parser để chuỗi qua bản dịch.
        settings::Store store(o.settingsPath);
        settings::Store::setInstance(&store);
        const i18n::Translator translator;
        QCommandLineParser translated;
        addOptions(translated);
        translated.process(args); // in trợ giúp hoặc lỗi rồi exit()
    }
    for (const QString& path : parser.positionalArguments())
        o.paths << absolute(path);
    o.tracePath = parser.value(u"startup-trace"_s);
    o.quitAfterFirstFrame = parser.isSet(u"quit-after-first-frame"_s);
    o.gallery = parser.isSet(u"gallery"_s);
    return o;
}

Application::Application(const Options& options, StartupTrace& trace, QObject* parent) : QObject(parent), trace_(trace)
{
    TRION_CHECK(!g_instance && "chỉ một Application");
    TRION_ASSERT_UI_THREAD();
    g_instance = this;
    trace_.setOutput(options.tracePath);

    store_ = std::make_unique<settings::Store>(options.settingsPath);
    settings::Store::setInstance(store_.get());
    executor_ = std::make_unique<ui::UiExecutor>();
    trace_.mark(u"settings"_s);
    translator_ = std::make_unique<i18n::Translator>();
    trace_.mark(u"i18n"_s);
    if (const Expected<void> fonts = theme::loadFonts(); !fonts) // vẫn chạy bằng font dự phòng
        qCWarning(lcApp).noquote() << QString::fromStdString(fonts.error().message);
    trace_.mark(u"fonts"_s);
    theme_ = std::make_unique<theme::ThemeManager>(*store_);
    trace_.mark(u"theme"_s);
    icons_ = std::make_unique<theme::IconProvider>();
    trace_.mark(u"icons"_s);
    keymap_ = std::make_unique<keymap::Keymap>(*store_);
    tips_ = std::make_unique<ui::RichTooltip>(); // tooltip 2 mức cho mọi widget (UI0-107): sau ThemeManager và Keymap
    tips_->setKeymap(keymap_.get());
    trace_.mark(u"keymap"_s);
    trace_.mark(u"ready"_s);

    jobs_ = std::make_unique<jobs::BackgroundJobs>(); // BackgroundJobs::instance() của ứng dụng
    shell::MainWindow* main = nullptr;
    if (options.gallery) {
        window_ = std::make_unique<ui::GalleryWindow>();
    } else {
        // Hành động → chuỗi lệnh → bộ định tuyến tạm (INT-002 thay); cổng phiên bản chặn lệnh bị khóa (UI0-011).
        sysvars_ = std::make_unique<actions::Sysvars>();
        sysvars_->set(u"COORDS"_s, 1); // tọa độ tuyệt đối như AutoCAD. ponytail: INT-005 (mặc định của sysvar) thay
        actions_ = std::make_unique<actions::ActionRegistry>(*keymap_, *sysvars_);
        gate_ = std::make_unique<edition::EditionGate>();
        gate_->bind(*actions_);
        router_ = std::make_unique<shell::CommandRouter>(*sysvars_);
        actions_->setSink([router = router_.get()](const QString& macro) { router->submit(macro); });
        recent_ = std::make_unique<shell::RecentFiles>(*store_); // nạp từ Store; trang Start kiểm lại tệp khi hiện
        auto window = std::make_unique<shell::MainWindow>(*store_);
        main = window.get();
        auto* clean = new shell::CleanScreen(*main, *sysvars_, *router_); // con của cửa sổ; CLEANSCREENON/OFF, Ctrl+0
        actions_->install(main);
        actions_->addViewport(main->region(shell::MainWindow::Region::Canvas));
        // Hồ sơ trước khi khôi phục (bố cục của workspace đang dùng áp một lần) và trước vùng thật (RIBBONSTATE đã lưu
        // theo workspace đang dùng).
        main->workspace().setProfile(gate_->profile());
        connect(gate_.get(), &edition::EditionGate::profileChanged, &main->workspace(), &shell::Workspace::setProfile);
        // Vùng thật trước khi khôi phục: hình học, bố cục áp lên widget thật.
        shell_ = std::make_unique<ShellAssembly>(*main, *keymap_, *actions_, *sysvars_, *gate_, *router_, *clean, *jobs_,
                                                 *recent_);
        connect(shell_->fileTabs, &shell::FileTabBar::openRequested, this, &Application::requestOpen);
        connect(shell_->start, &shell::StartPage::openRequested, this, &Application::requestOpen);
        connect(shell_->appMenu, &shell::AppMenu::openRequested, this, &Application::requestOpen);
        main->restore(); // hình học (phóng to, màn hình), palette, workspace, dòng lệnh nổi đã lưu
        window_ = std::move(window);
    }
    // Raster trước bộ biểu tượng Viewer ở nền cho DPR mọi màn hình (IconProvider::prewarm). Không gọi prewarm riêng ở
    // pha icons: DisplayContext gọi lại cho cùng DPR thì 236 mặt nạ bị raster hai lần.
    display_ = std::make_unique<ui::DisplayContext>(window_.get());
    if (main) {
        main->setScreenClass(display_->screenClass());
        connect(display_.get(), &ui::DisplayContext::screenClassChanged, main, &shell::MainWindow::setScreenClass);
        main->palettes().trackDisplay(*display_);
        main->show(); // phóng to nếu lần trước phóng to (lần đầu: phóng to)
    } else {
        window_->showMaximized();
    }
    trace_.mark(u"window"_s);
    trace_.watchFirstFrame(window_.get());
    // Sau khung đầu (lượt sự kiện sau, không kéo dài lượt giao khung đầu), ở nền: mặt nạ mọi biểu tượng (tự hoàn thành
    // của dòng lệnh, menu, ribbon tab khác) ở DPR của cửa sổ, để lần hiện đầu không raster trên luồng UI.
    // ponytail: đổi DPR thì chỉ bộ Viewer được raster trước (DisplayContext).
    if (main)
        connect(
            &trace_, &StartupTrace::firstFrame, this,
            [this] { prewarmBatches(*icons_, this, window_->devicePixelRatioF(), theme::IconProvider::names()); },
            Qt::QueuedConnection);

    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] { trace_.mark(u"quit"_s); });
    if (options.quitAfterFirstFrame) // thoát sau khi khung đầu đã giao xong, không thoát ngay trong lúc giao
        connect(&trace_, &StartupTrace::firstFrame, this,
                [] { QTimer::singleShot(0, QCoreApplication::instance(), &QCoreApplication::quit); });
}

Application::~Application()
{
    TRION_ASSERT_UI_THREAD();
    // Thoát: việc nền dừng hợp tác trước khi hủy UiExecutor (UiExecutor.h).
    cancel_.cancel();
    if (auto* main = qobject_cast<shell::MainWindow*>(window_.get()))
        main->save(); // thoát không qua đóng cửa sổ (quit, phiên kết thúc): vẫn lưu hình học, bố cục
    display_.reset();
    window_.reset();
    shell_.reset(); // sau widget: RibbonControls sống lâu hơn nút ribbon, NotificationCenter lâu hơn ToastHost
    recent_.reset(); // sau trang Start; hủy lần kiểm tệp đang chạy (kết quả về sau bị bỏ)
    router_.reset();
    gate_.reset(); // gỡ cổng khỏi ActionRegistry
    actions_.reset();
    sysvars_.reset();
    jobs_.reset(); // yêu cầu hủy mọi tác vụ còn chạy; waitIdle dưới chờ chúng dừng
    tips_.reset();
    icons_.reset(); // hủy việc raster trước còn chờ
    sched::waitIdle();
    trace_.mark(u"tasks_stopped"_s);
    keymap_.reset();
    theme_.reset();
    translator_.reset();
    executor_.reset(); // không còn việc nền nào gửi kết quả
    store_.reset();    // ghi nốt cài đặt, đồng bộ; tự gỡ khỏi Store::instance()
    g_instance = nullptr;
}

Application* Application::instance()
{
    return g_instance;
}

sched::CancelToken Application::cancelToken()
{
    TRION_ASSERT_UI_THREAD();
    return g_instance ? g_instance->cancel_.token() : sched::CancelToken();
}

void Application::requestOpen(const QStringList& paths)
{
    TRION_ASSERT_UI_THREAD();
    // Windows: lần chạy sau đã AllowSetForegroundWindow cho tiến trình này (SingleInstance::forward).
    window_->setWindowState(window_->windowState() & ~Qt::WindowMinimized);
    window_->show();
    window_->raise();
    window_->activateWindow();
    if (paths.isEmpty())
        return;
    qCInfo(lcApp).noquote() << "openRequested:" << paths.join(u", "_s); // nhãn ASCII: stderr Windows theo mã trang ANSI
    emit openRequested(paths);
}

} // namespace trion::app
