#include "app/main/ShellAssembly.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/i18n/Translator.h"
#include "app/core/keymap/Keymap.h"
#include "app/shell/CleanScreen.h"
#include "app/shell/cmdline/CommandLine.h"
#include "app/shell/CommandRouter.h"
#include "app/shell/filetabs/FileTabBar.h"
#include "app/shell/MainWindow.h"
#include "app/shell/status/ModeToggles.h"
#include "app/shell/titlebar/CommandSearch.h"
#include "app/shell/titlebar/TitleBar.h"
#include "app/widgets/cmdline/CommandInput.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/progress/ProgressPresenter.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QShortcut>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace trion::app {

using Region = shell::MainWindow::Region;

ShellAssembly::ShellAssembly(shell::MainWindow& window, keymap::Keymap& keymap, actions::ActionRegistry& actions,
                             actions::Sysvars& sysvars, edition::EditionGate& gate, shell::CommandRouter& router,
                             shell::CleanScreen& clean, jobs::BackgroundJobs& jobs)
    : window(window), actions(actions), sysvars(sysvars), gate(gate), router(router), clean(clean),
      ribbonControls(actions)
{
    toasts = new ui::ToastHost(&window, notes, &window); // trước tab tệp: tệp thả bị từ chối báo bằng toast
    presenter = new ui::ProgressPresenter(jobs, &window);
    toasts->followJobs(*presenter);

    titleBar = new shell::TitleBar(actions, gate);
    titleBar->search()->setLocator([this](const QString& command) { return ribbonModel.locate(command); });
    window.setRegion(Region::TitleBar, titleBar);

    window.setRegion(Region::Ribbon, buildRibbon());
    connect(&ribbonControls, &ribbon::RibbonControls::macroRequested, &router, &shell::CommandRouter::submit);

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

    commandLine = new ui::CommandLine;
    commandLine->installEventFilter(this); // trước setRegion: lần hiện đầu đặt CLISTATE
    window.setRegion(Region::CommandLine, commandLine);
    connect(commandLine, &ui::CommandLine::submitted, &router, &shell::CommandRouter::submit);
    connect(commandLine, &ui::CommandLine::commandRequested, &router, &shell::CommandRouter::submit);
    connect(commandLine, &ui::CommandLine::cancelRequested, commandLine, &ui::CommandLine::clearPrompt);
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

    // Khung thanh trạng thái tạm. ponytail: UIA-009 (StatusBar: tọa độ, MODEL, tác vụ nền, thông báo…) thay.
    auto* status = new QWidget;
    auto* row = new QHBoxLayout(status);
    row->setContentsMargins({});
    modes = new shell::ModeToggles(actions, sysvars);
    row->addWidget(modes);
    row->addStretch();
    status->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    window.setRegion(Region::StatusBar, status);
    connect(modes, &shell::ModeToggles::macroRequested, &router, &shell::CommandRouter::submit);
    modes->setProfile(gate.profile());
    connect(&gate, &edition::EditionGate::profileChanged, modes, &shell::ModeToggles::setProfile);

    // Focus đầu ở vùng vẽ (07.1 §3: vùng vẽ luôn nhận phím gõ); ô nhập có focus thì con trỏ chữ nhấp nháy là timer.
    window.region(Region::Canvas)->setFocus(Qt::OtherFocusReason);
}

ShellAssembly::~ShellAssembly() = default;

QWidget* ShellAssembly::buildRibbon()
{
    auto* ribbon = new QWidget;
    auto* column = new QVBoxLayout(ribbon);
    column->setContentsMargins({});
    column->setSpacing(0);
    ribbonTabs = new ui::TabBar;
    const auto& tabs = ribbonModel.tabs();
    for (const ribbon::Tab& t : tabs)
        ribbonTabs->addTab(ribbon::RibbonModel::text(t.name));
    if (const auto* t = i18n::Translator::instance())
        connect(t, &i18n::Translator::languageChanged, ribbonTabs, [this] {
            for (int i = 0; i < ribbonTabs->count(); ++i)
                ribbonTabs->setTabText(i, ribbon::RibbonModel::text(ribbonModel.tabs()[size_t(i)].name));
        });
    ribbonPages = new ribbon::RibbonPages(int(tabs.size()), [this](int i) { return ribbonPage(i); });
    // Trang giữ cỡ tự nhiên (nút không bị ép nhỏ, không cắt chữ), phần tràn khỏi cửa sổ hẹp bị cắt; bề rộng tối thiểu
    // của trang không thành bề rộng tối thiểu của cửa sổ.
    auto* clip = new QScrollArea;
    clip->setFrameShape(QFrame::NoFrame);
    clip->setFocusPolicy(Qt::NoFocus);
    clip->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    clip->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    clip->setWidgetResizable(true);
    clip->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    clip->setWidget(ribbonPages);
    column->addWidget(ribbonTabs);
    column->addWidget(clip);
    connect(ribbonTabs, &QTabBar::currentChanged, ribbonPages, &ribbon::RibbonPages::setCurrentTab);
    ribbonPages->setCurrentTab(ribbonTabs->currentIndex());
    return ribbon;
}

// Thân panel nằm ngang như ribbon_test (khung panel, tên panel là của UIA-005).
QWidget* ShellAssembly::ribbonPage(int tab)
{
    auto* page = new QWidget;
    auto* row = new QHBoxLayout(page);
    for (const ribbon::Panel& p : ribbonModel.tabs()[size_t(tab)].panels)
        row->addWidget(ribbonControls.buildPanel(p), 0, Qt::AlignTop);
    row->addStretch();
    return page;
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
