#include "app/main/ShellAssembly.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/keymap/Keymap.h"
#include "app/shell/CleanScreen.h"
#include "app/shell/cmdline/Autocomplete.h"
#include "app/shell/cmdline/CommandLine.h"
#include "app/shell/CommandRouter.h"
#include "app/shell/filetabs/FileTabBar.h"
#include "app/shell/MainWindow.h"
#include "app/shell/ribbon/RibbonBar.h"
#include "app/shell/start/StartPage.h"
#include "app/shell/status/ModeToggles.h"
#include "app/shell/status/StatusBar.h"
#include "app/shell/titlebar/CommandSearch.h"
#include "app/shell/titlebar/Qat.h"
#include "app/shell/titlebar/TitleBar.h"
#include "app/widgets/cmdline/CommandInput.h"
#include "app/widgets/dock/PaletteHost.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/progress/ProgressPresenter.h"
#include "app/widgets/tooltip/RichTooltip.h"

#include <QApplication>
#include <QLoggingCategory>
#include <QShortcut>
#include <QStackedWidget>
#include <QTabBar>

using namespace Qt::StringLiterals;

namespace trion::app {

namespace {

Q_STATIC_LOGGING_CATEGORY(lcApp, "trion.app", QtWarningMsg)

// ponytail: tín hiệu chưa có đích (màn hình, mở chỉ đọc DOC-018, hộp Mở ở thư mục CMD-237): ghi log mức info
// (QT_LOGGING_RULES="trion.app.info=true"); task của đích nối thay.
auto logOnly(const char* signal)
{
    return [signal](const QString& value) { qCInfo(lcApp).noquote() << signal << value; };
}

} // namespace

using Region = shell::MainWindow::Region;

ShellAssembly::ShellAssembly(shell::MainWindow& window, keymap::Keymap& keymap, actions::ActionRegistry& actions,
                             actions::Sysvars& sysvars, edition::EditionGate& gate, shell::CommandRouter& router,
                             shell::CleanScreen& clean, jobs::BackgroundJobs& jobs, shell::RecentFiles& recent)
    : window(window), actions(actions), sysvars(sysvars), gate(gate), router(router), clean(clean),
      ribbonControls(actions)
{
    toasts = new ui::ToastHost(&window, notes, &window); // trước tab tệp, trang Start: tệp thả bị từ chối báo bằng toast
    presenter = new ui::ProgressPresenter(jobs, &window);
    toasts->followJobs(*presenter);

    titleBar = new shell::TitleBar(actions, gate);
    titleBar->search()->setLocator([this](const QString& command) { return ribbonModel.locate(command); });
    window.setRegion(Region::TitleBar, titleBar);
    connect(titleBar, &shell::TitleBar::screenRequested, this, logOnly("screenRequested:"));

    ribbon = new ribbon::RibbonBar(ribbonModel, ribbonControls);
    window.setRegion(Region::Ribbon, ribbon);
    connect(&ribbonControls, &ribbon::RibbonControls::macroRequested, &router, &shell::CommandRouter::submit);
    // QAT (UIA-101): chuột phải ribbon "Thêm vào thanh truy cập nhanh" → nút mới; UNDO k, MREDO k, 'MENUBAR → router.
    // ponytail: danh sách hoàn tác/làm lại rỗng tới DOC-008 (setUndoSource/setRedoSource); "Hiện bên dưới ribbon" chưa
    // có hàng dưới ribbon (QAT ở lại thanh tiêu đề), MENUBAR chưa có thanh menu (UIA-006).
    shell::Qat* qat = titleBar->qat();
    connect(ribbon, &ribbon::RibbonBar::addToQatRequested, qat, &shell::Qat::add);
    connect(qat, &shell::Qat::macroRequested, &router, &shell::CommandRouter::submit);
    // RIBBON.md, RIBBONCLOSE.md: ẩn chỉ là ẩn vùng (mức thu gọn, tab, panel ghim giữ nguyên, Alt không vào ribbon ẩn);
    // RIBBONSTATE theo lệnh. Clean Screen độc lập: lệnh lúc đang bật chỉ ghi nhận, tắt Clean Screen thì vùng theo nó.
    sysvars.set(u"RIBBONSTATE"_s, 1);
    const auto showRibbon = [this](bool on) {
        this->sysvars.set(u"RIBBONSTATE"_s, on ? 1 : 0);
        if (!this->clean.isOn())
            this->window.setRegionVisible(Region::Ribbon, on);
    };
    router.add(u"RIBBON"_s, [showRibbon] { showRibbon(true); });
    router.add(u"RIBBONCLOSE"_s, [showRibbon] { showRibbon(false); });
    connect(&clean, &shell::CleanScreen::toggled, ribbon, [this](bool on) {
        if (!on)
            this->window.setRegionVisible(Region::Ribbon, this->sysvars.get(u"RIBBONSTATE"_s) != 0);
    });
    // F1 khi rê phần tử có lệnh (ribbon, menu, thanh trạng thái…). ponytail: UIL-009 (cửa sổ trợ giúp) nhận thay.
    if (const auto* tips = ui::RichTooltip::instance())
        connect(tips, &ui::RichTooltip::helpRequested, this,
                [this](const QString& command) { this->router.submit(u"'_HELP "_s + command); });

    fileTabs = new shell::FileTabBar;
    window.setRegion(Region::FileTabs, fileTabs);
    // ponytail: chưa có phiên DOC-018: kích hoạt tài liệu = chọn tab của nó.
    connect(fileTabs, &shell::FileTabBar::activateRequested, fileTabs,
            [this](quint64 id) { fileTabs->setCurrent(id); });
    connect(fileTabs, &shell::FileTabBar::commandRequested, this, [this](quint64 id, const QString& macro) {
        fileTabs->setCurrent(id); // lệnh (CLOSE, QSAVE…) chạy trên tài liệu của tab
        this->router.submit(macro);
    });
    bindTabKeys(keymap);
    window.palettes().setKeymap(keymap); // F6 / Shift+F6 qua palette theo Keymap "pane"
    buildCanvas(recent);

    commandLine = new ui::CommandLine;
    commandLine->installEventFilter(this); // trước setRegion: lần hiện đầu đặt CLISTATE
    window.setRegion(Region::CommandLine, commandLine);
    connect(commandLine, &ui::CommandLine::submitted, &router, &shell::CommandRouter::submit);
    connect(commandLine, &ui::CommandLine::commandRequested, &router, &shell::CommandRouter::submit);
    connect(commandLine, &ui::CommandLine::cancelRequested, commandLine, &ui::CommandLine::clearPrompt);
    // Tự hoàn thành: huy hiệu khóa như tooltip của QAction (EditionGate đặt need theo phiên bản).
    commandLine->autocomplete()->setLockQuery(
        [this](const QString& command) { return ui::RichTooltip::info(this->actions.action(command)).need; });
    // Tên gõ không có (07.1 §8): tên gõ ở dấu nhắc lệnh mà router không xử lý, không phải lệnh, viết tắt, biến →
    // "Không có lệnh “LNE”. Ý bạn là: LINE…". Macro của nút (^C^C, '), trả lời prompt, biểu thức LISP không phải tên gõ.
    // ponytail: INT-002 báo lệnh không có (cả lệnh LISP, plugin) thay.
    connect(&router, &shell::CommandRouter::unhandled, commandLine, [this](const QString& macro) {
        if (commandLine->hasPrompt() || macro.startsWith(u"^C"_s) || macro.startsWith(u'\'') || macro.startsWith(u'('))
            return;
        if (commandLine->autocomplete()->resolve(macro).isEmpty())
            commandLine->reportUnknown(macro);
    });
    // COMMANDLINE.md, COMMANDLINEHIDE.md: ẩn chỉ là ẩn widget (lịch sử, prompt, chữ gõ dở giữ nguyên); hiện thì focus
    // về ô nhập; ẩn khi focus ở trong thì focus về vùng vẽ.
    router.add(u"COMMANDLINE"_s, [this] {
        this->window.setRegionVisible(Region::CommandLine, true);
        commandLine->input()->setFocus(Qt::OtherFocusReason);
    });
    router.add(u"COMMANDLINEHIDE"_s, [this] {
        if (commandLine->isAncestorOf(QApplication::focusWidget()))
            this->window.region(Region::Canvas)->setFocus(Qt::OtherFocusReason);
        this->window.setRegionVisible(Region::CommandLine, false);
    });
    // Prototype A.cleanScreen: "<Clean Screen bật · Ctrl+0 để tắt>", "<Clean Screen tắt>"; phím theo Keymap.
    connect(&clean, &shell::CleanScreen::toggled, commandLine, [this](bool on) {
        const QString key = ui::Menu::keyText(this->actions.action(u"CLEANSCREENON"_s));
        commandLine->appendHistory(!on             ? tr("<Clean Screen tắt>")
                                   : key.isEmpty() ? tr("<Clean Screen bật>")
                                                   : tr("<Clean Screen bật · %1 để tắt>").arg(key));
    });

    statusBar = new shell::StatusBar(actions, sysvars, gate, *presenter, notes);
    window.setRegion(Region::StatusBar, statusBar);
    modes = statusBar->findChild<shell::ModeToggles*>();
    connect(statusBar, &shell::StatusBar::macroRequested, &router, &shell::CommandRouter::submit);
    connect(statusBar, &shell::StatusBar::screenRequested, this, logOnly("screenRequested:"));

    // Focus đầu ở vùng vẽ (07.1 §3: vùng vẽ luôn nhận phím gõ; tab Start: trang Start); ô nhập có focus thì con trỏ chữ
    // nhấp nháy là timer.
    window.region(Region::Canvas)->setFocus(Qt::OtherFocusReason);
}

ShellAssembly::~ShellAssembly() = default;

// Vùng Canvas = ngăn xếp {vùng vẽ giữ chỗ, trang Start}. Vùng vẽ giữ nguyên đối tượng (ActionRegistry::addViewport đã
// gắn hành động phạm vi vùng vẽ vào nó): lấy khỏi dockHost trước khi setRegion hủy widget trung tâm cũ; vào ngăn xếp
// đầu tiên nên là trang hiện hành, không bị ẩn tường minh (setRegion giữ trạng thái ẩn/hiện của vùng theo nó).
void ShellAssembly::buildCanvas(shell::RecentFiles& recent)
{
    canvas = window.dockHost()->takeCentralWidget();
    auto* stack = new QStackedWidget;
    stack->addWidget(canvas);
    start = new shell::StartPage(recent);
    stack->addWidget(start);
    window.setRegion(Region::Canvas, stack);
    // NEWTAB.md: trang hiện khi tab Start đang chọn (bấm, Ctrl+Tab, NEWTAB, đóng tab bản vẽ cuối); focus của vùng (Esc
    // của palette, COMMANDLINEHIDE) tới trang đang hiện.
    const auto sync = [this, stack] {
        QWidget* page = fileTabs->current() == shell::FileTabBar::kStart ? start : canvas;
        stack->setFocusProxy(page);
        stack->setCurrentWidget(page);
    };
    connect(fileTabs->tabBar(), &QTabBar::currentChanged, stack, sync);
    sync();
    router.add(u"NEWTAB"_s, [this] { fileTabs->setCurrent(shell::FileTabBar::kStart); });
    connect(start, &shell::StartPage::macroRequested, &router, &shell::CommandRouter::submit);
    connect(start, &shell::StartPage::screenRequested, this, logOnly("screenRequested:"));
    connect(start, &shell::StartPage::openReadOnlyRequested, this, logOnly("openReadOnlyRequested:"));
    connect(start, &shell::StartPage::browseRequested, this, logOnly("browseRequested:"));
}

// Hành động "ntab" của Keymap không phải lệnh (ActionRegistry bỏ qua): khung cửa sổ giữ phím. Tổ hợp có Shift là tab
// trước, còn lại tab kế; theo Keymap khi người dùng đổi phím. Phím gắn vào cửa sổ (không vào dải tab): Clean Screen ẩn
// dải mà Ctrl+Tab vẫn đổi bản vẽ.
void ShellAssembly::bindTabKeys(keymap::Keymap& keymap)
{
    auto* next = new QShortcut(&window);
    auto* previous = new QShortcut(&window);
    for (QShortcut* s : {next, previous})
        s->setContext(Qt::ApplicationShortcut);
    connect(next, &QShortcut::activated, fileTabs, &shell::FileTabBar::next);
    connect(previous, &QShortcut::activated, fileTabs, &shell::FileTabBar::previous);
    const auto bind = [map = &keymap, next, previous] {
        QList<QKeySequence> forward, back;
        if (const keymap::Action* a = map->action(u"ntab"_s))
            for (const QKeyCombination c : a->bindings)
                (c.keyboardModifiers() & Qt::ShiftModifier ? back : forward) << QKeySequence(c);
        next->setKeys(forward);
        previous->setKeys(back);
    };
    bind();
    connect(&keymap, &keymap::Keymap::changed, next, bind);
}

bool ShellAssembly::eventFilter(QObject* watched, QEvent* e)
{
    // Vùng dòng lệnh ẩn/hiện tường minh (COMMANDLINE/HIDE, workspace áp bố cục…) → CLISTATE 1/0 (chỉ đọc trong
    // AutoCAD); ActionRegistry đọc nó để Ctrl+9 đổi vòng. Cửa sổ thu nhỏ không phát ShowToParent/HideToParent.
    if (watched == commandLine && (e->type() == QEvent::ShowToParent || e->type() == QEvent::HideToParent))
        sysvars.set(u"CLISTATE"_s, e->type() == QEvent::ShowToParent ? 1 : 0);
    return false;
}

} // namespace trion::app
