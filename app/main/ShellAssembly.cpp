#include "app/main/ShellAssembly.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/keymap/Keymap.h"
#include "app/shell/appmenu/AppMenu.h"
#include "app/shell/CleanScreen.h"
#include "app/shell/cmdline/Autocomplete.h"
#include "app/shell/cmdline/CommandLine.h"
#include "app/shell/cmdline/HistoryWindow.h"
#include "app/shell/cmdline/InputSearchOptions.h"
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
#include "app/shell/viewer/ViewerLayers.h"
#include "app/shell/viewer/ViewState.h"
#include "app/shell/Workspace.h"
#include "app/widgets/cmdline/CommandInput.h"
#include "app/widgets/dock/PaletteHost.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/progress/ProgressPresenter.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/settings/Settings.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QShortcut>
#include <QStackedWidget>
#include <QTabBar>

#include <utility>

using namespace Qt::StringLiterals;

namespace trion::app {

namespace {

Q_STATIC_LOGGING_CATEGORY(lcApp, "trion.app", QtWarningMsg)

// RIBBONSTATE theo workspace (RIBBONCLOSE.md mục 3, NT-010): {"2d": 0, "viewer": 1}; chưa có: 1 (ribbon hiện).
const settings::Setting<QJsonObject> kRibbonState{"ribbon/state", {}};

// ponytail: tín hiệu chưa có đích (màn hình, mở chỉ đọc DOC-018, hộp Mở ở thư mục CMD-237): ghi log mức info
// (QT_LOGGING_RULES="trion.app.info=true"); task của đích nối thay.
auto logOnly(const char* signal)
{
    return [signal](const QString& value) { qCInfo(lcApp).noquote() << signal << value; };
}

// Điểm dừng Tab của w (theo chuỗi focus hiện tại, giữ thứ tự với nhau) đứng ngay sau after: Tab theo hàng trên màn
// hình khi w đổi chỗ (Qt giữ chỗ cũ của widget trong chuỗi focus khi đổi cha trong cùng cửa sổ).
void tabAfter(QWidget* after, QWidget* w)
{
    QList<QWidget*> stops;
    for (QWidget* n = w->nextInFocusChain(); n != w; n = n->nextInFocusChain())
        if (w->isAncestorOf(n) && n->focusPolicy() != Qt::NoFocus)
            stops << n;
    for (QWidget* s : std::as_const(stops)) {
        QWidget::setTabOrder(after, s);
        after = s;
    }
}

} // namespace

using Region = shell::MainWindow::Region;

ShellAssembly::ShellAssembly(shell::MainWindow& window, keymap::Keymap& keymap, actions::ActionRegistry& actions,
                             actions::Sysvars& sysvars, edition::EditionGate& gate, shell::CommandRouter& router,
                             shell::CleanScreen& clean, jobs::BackgroundJobs& jobs, shell::RecentFiles& recent)
    : window(window), actions(actions), sysvars(sysvars), gate(gate), router(router), clean(clean),
      ribbonControls(actions), dialogs(&window)
{
    toasts = new ui::ToastHost(&window, notes, &window); // trước tab tệp, trang Start: tệp thả bị từ chối báo bằng toast
    presenter = new ui::ProgressPresenter(jobs, &window);
    toasts->followJobs(*presenter);

    titleBar = new shell::TitleBar(actions, gate);
    window.setRegion(Region::TitleBar, titleBar);
    connect(titleBar, &shell::TitleBar::screenRequested, this, logOnly("screenRequested:"));
    // Menu ứng dụng (UIA-003): nút logo; mục có QAction chạy qua ActionRegistry (sink → router, cổng phiên bản), mục
    // còn lại (Lưu thành theo phiên bản, Xuất DXF, Đóng tất cả) phát macro. ponytail: ảnh tệp gần đây chưa có nguồn
    // (DWG-026, RND-021 đặt cho appMenu->thumbnails() như trang Start).
    appMenu = new shell::AppMenu(actions, recent, *titleBar->search(), &window);
    connect(titleBar, &shell::TitleBar::appMenuRequested, appMenu, &shell::AppMenu::popupAt);
    connect(appMenu, &shell::AppMenu::macroRequested, &router, &shell::CommandRouter::submit);

    ribbon = new ribbon::RibbonBar(ribbonModel, ribbonControls);
    // Hồ sơ Viewer: ribbon giản lược UI-M01 + panel "Dùng thử Pro 30 ngày" (UIA-017); ô Tìm lệnh chỉ đường theo ribbon
    // đang dùng.
    ribbon->setProfile(gate.profile());
    connect(&gate, &edition::EditionGate::profileChanged, ribbon, &ribbon::RibbonBar::setProfile);
    titleBar->search()->setLocator([this](const QString& command) { return ribbon->locate(command); });
    window.setRegion(Region::Ribbon, ribbon);
    connect(&ribbonControls, &ribbon::RibbonControls::macroRequested, &router, &shell::CommandRouter::submit);
    // QAT (UIA-101): chuột phải ribbon "Thêm vào thanh truy cập nhanh" → nút mới; UNDO k, MREDO k, 'MENUBAR → router;
    // "Hiện bên dưới ribbon" → hàng dưới ribbon (TitleBar tự lấy lại khi thôi; nối trước nên chạy trước placeQat).
    // ponytail: danh sách hoàn tác/làm lại rỗng tới DOC-008 (setUndoSource/setRedoSource); 'MENUBAR là hành động sysvar
    // của ActionRegistry, thanh menu chưa có (UIA-006).
    shell::Qat* qat = titleBar->qat();
    connect(ribbon, &ribbon::RibbonBar::addToQatRequested, qat, &shell::Qat::add);
    connect(qat, &shell::Qat::macroRequested, &router, &shell::CommandRouter::submit);
    connect(qat, &shell::Qat::belowRibbonChanged, this, &ShellAssembly::placeQat);
    // RIBBON.md, RIBBONCLOSE.md: ẩn chỉ là ẩn vùng (mức thu gọn, tab, panel ghim giữ nguyên, Alt không vào ribbon ẩn);
    // RIBBONSTATE theo lệnh, lưu theo workspace; workspace áp bố cục (hiện mọi vùng của mô tả) thì áp lại. Clean
    // Screen độc lập: lệnh lúc đang bật chỉ ghi nhận, tắt Clean Screen thì vùng theo nó.
    applyRibbonState();
    connect(&window.workspace(), &shell::Workspace::layoutApplied, this, &ShellAssembly::applyRibbonState);
    router.add(u"RIBBON"_s, [this] { setRibbonState(true); });
    router.add(u"RIBBONCLOSE"_s, [this] {
        if (this->sysvars.get(u"RIBBONSTATE"_s) == 0)
            return; // đã ẩn: không làm gì
        setRibbonState(false);
        if (!std::exchange(ribbonHintShown_, true)) // RIBBONCLOSE.md mục 5: lần đầu trong phiên
            toasts->post(ui::Severity::Info, tr("Đã ẩn ribbon. Gõ RIBBON để hiện lại."));
    });
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
    buildCommandLine();

    statusBar = new shell::StatusBar(actions, sysvars, gate, *presenter, notes);
    window.setRegion(Region::StatusBar, statusBar);
    modes = statusBar->findChild<shell::ModeToggles*>();
    connect(statusBar, &shell::StatusBar::macroRequested, &router, &shell::CommandRouter::submit);
    connect(statusBar, &shell::StatusBar::screenRequested, this, logOnly("screenRequested:"));

    buildViewer();
    if (qat->isBelowRibbon()) // sau tab tệp: thứ tự Tab tính theo khối của tab tệp
        placeQat(true);

    // Focus đầu ở vùng vẽ (07.1 §3: vùng vẽ luôn nhận phím gõ; tab Start: trang Start); ô nhập có focus thì con trỏ chữ
    // nhấp nháy là timer.
    window.region(Region::Canvas)->setFocus(Qt::OtherFocusReason);
}

ShellAssembly::~ShellAssembly() = default;

void ShellAssembly::buildCommandLine()
{
    commandLine = new ui::CommandLine;
    commandLine->installEventFilter(this); // trước setRegion: lần hiện đầu đặt CLISTATE
    window.setRegion(Region::CommandLine, commandLine);
    // Tên lệnh gõ ở dấu nhắc lệnh mà phiên bản chưa có (07.1 §2): "Lệnh LINE có trong TrionCAD Pro." kèm 2 chip, không
    // chạy. Macro của nút (^C^C, ') qua QAction đã có cổng của EditionGate. ponytail: INT-020 (cổng lệnh) thay.
    connect(commandLine, &ui::CommandLine::submitted, this, [this](const QString& token) {
        if (!commandLine->hasPrompt()) {
            const QString name = commandLine->autocomplete()->resolve(token);
            if (!name.isEmpty() && this->actions.action(name) && this->gate.locked(this->actions.need(name))) {
                commandLine->notifyLocked(name, this->actions.need(name));
                return;
            }
        }
        this->router.submit(token);
    });
    connect(commandLine, &ui::CommandLine::upgradeRequested, this, [this](const QString& command) {
        this->gate.upgrade().open(
            {command, this->actions.need(command), ui::RichTooltip::name(this->actions.action(command))});
    });
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

    // UIA-007: lịch sử đầy đủ (TEXTSCR: F2 qua QAction của Keymap "hist", nút ^ của dòng lệnh) và thiết lập
    // (INPUTSEARCHOPTIONS: menu chuột phải dòng lệnh); DialogHost mở lần nữa thì đưa bản đang mở lên.
    dialogs.add(u"textscr"_s, [this](QWidget* parent) { return new ui::HistoryWindow(*commandLine, parent); });
    dialogs.add(u"inputsearchoptions"_s, [](QWidget* parent) { return new ui::InputSearchOptions(parent); });
    router.add(u"TEXTSCR"_s, [this] { dialogs.open(u"textscr"_s); });
    router.add(u"INPUTSEARCHOPTIONS"_s, [this] { dialogs.open(u"inputsearchoptions"_s); });

    // Dạng nổi (07.1 §8): kéo dải nắm → nổi trên vùng vẽ; thả sát mép dưới vùng vẽ → về cột. MainWindow dời widget,
    // lưu và khôi phục; CommandLine đặt dạng của nó.
    connect(commandLine, &ui::CommandLine::floatRequested, &window,
            [this](QPoint at) { this->window.setCommandLineFloating(true, at); });
    connect(commandLine, &ui::CommandLine::dockRequested, &window,
            [this] { this->window.setCommandLineFloating(false); });
    connect(&window, &shell::MainWindow::commandLineFloatChanged, commandLine, [this](bool floating, QPoint at) {
        commandLine->setFloating(floating, floating ? std::optional<QPoint>(at) : std::nullopt);
    });
}

// Palette "layers" (UIA-118): hồ sơ Viewer là ViewerLayers, hồ sơ đầy đủ giữ ô giữ chỗ tới UIC-009; đổi theo
// EditionGate::profileChanged. ViewStateFile lưu, nạp .tcview; banner của nó trên vùng vẽ.
// ponytail: nguồn layer (setLayers) chờ DOC-004, CMD-501, DOC-009: palette ở trạng thái rỗng "Chưa có bản vẽ".
void ShellAssembly::buildViewer()
{
    banner = new ui::CanvasBanner(canvas);
    viewStates = new shell::ViewStateFile(gate, *banner, this);
    window.setPaletteContent(u"layers"_s, [this](QWidget* placeholder) {
        auto* stack = new QStackedWidget;
        stack->addWidget(placeholder);
        auto* layers = new shell::ViewerLayers(gate);
        viewerLayers = layers;
        stack->addWidget(layers);
        connect(layers, &shell::ViewerLayers::macroRequested, &router, &shell::CommandRouter::submit);
        connect(layers, &shell::ViewerLayers::saveViewRequested, this, [this, layers] {
            const QString path = documentPath();
            if (path.isEmpty() && !this->gate.locked(ui::Edition::ViewerPro))
                return; // chưa có bản vẽ đã lưu: không có gì để ghi (bản Viewer vẫn mở trang nâng cấp)
            // ponytail: layout, VIEWCTR, VIEWSIZE của view đang xem chờ RND; nay "Model" và view mặc định.
            viewStates->save(path, {.layout = u"Model"_s, .layers = layers->overrides()});
        });
        const auto sync = [this, stack] {
            stack->setCurrentIndex(this->gate.profile() == edition::UiProfile::Viewer ? 1 : 0);
        };
        sync();
        connect(&gate, &edition::EditionGate::profileChanged, stack, sync);
        return stack;
    });
    // ponytail: đổi layout, view theo s.layout, s.centerX/Y, s.height chờ RND (CTAB, VIEW); nay chỉ áp layer.
    connect(viewStates, &shell::ViewStateFile::loaded, this, [this](const QString&, const shell::ViewState& s) {
        if (const QString macro = shell::LayerMacro::to(s.layers).text(); !macro.isEmpty())
            router.submit(macro);
    });
    connect(viewStates, &shell::ViewStateFile::saved, this, [this](const QString& drawing, const QString& error) {
        if (error.isEmpty())
            toasts->post(ui::Severity::Ok, tr("Đã lưu trạng thái hiển thị vào %1.")
                                               .arg(QFileInfo(shell::viewStatePath(drawing)).fileName()));
        else
            toasts->post(ui::Severity::Error, error);
    });
}

QString ShellAssembly::documentPath() const
{
    const quint64 id = fileTabs->current();
    const QTabBar* tabs = fileTabs->tabBar();
    for (int i = 0; id != shell::FileTabBar::kStart && i < tabs->count(); ++i)
        if (tabs->tabData(i).toULongLong() == id)
            return QDir::fromNativeSeparators(tabs->tabToolTip(i));
    return {};
}

// QAT dưới ribbon: vào hàng dưới ribbon, Tab sau ribbon (điểm dừng cuối trước tab tệp) và trước tab tệp; thôi thì
// TitleBar đã lấy lại về sau nút logo, Tab theo đó.
void ShellAssembly::placeQat(bool below)
{
    shell::Qat* qat = titleBar->qat();
    QWidget* after = nullptr;
    if (below) {
        window.addBelowRibbon(qat);
        for (QWidget* w = fileTabs->previousInFocusChain(); w != fileTabs && !after; w = w->previousInFocusChain())
            if (w->focusPolicy() != Qt::NoFocus && w != qat && !qat->isAncestorOf(w))
                after = w;
    } else {
        after = titleBar->findChild<QWidget*>(u"appMenuButton"_s);
    }
    if (after)
        tabAfter(after, qat);
}

void ShellAssembly::applyRibbonState()
{
    const QJsonObject states = settings::Store::instance().get(kRibbonState);
    const bool on = states.value(window.workspace().current()).toInt(1) != 0;
    sysvars.set(u"RIBBONSTATE"_s, on ? 1 : 0);
    window.setRegionVisible(Region::Ribbon, on);
}

void ShellAssembly::setRibbonState(bool on)
{
    settings::Store& store = settings::Store::instance();
    QJsonObject states = store.get(kRibbonState);
    states.insert(window.workspace().current(), on ? 1 : 0);
    store.set(kRibbonState, states);
    sysvars.set(u"RIBBONSTATE"_s, on ? 1 : 0);
    if (!clean.isOn())
        window.setRegionVisible(Region::Ribbon, on);
}

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
        if (commandLine && commandLine->isFloating()) // trang mới được đưa lên trên: dòng lệnh nổi lên lại trên cùng
            commandLine->setFloating(true);
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
