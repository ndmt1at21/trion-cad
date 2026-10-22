#pragma once
// Integration nhóm 5 · Lắp khung ứng dụng đợt 5 vào cửa sổ chính (UIA-001) và nối lệnh của chúng vào bộ định tuyến
// tạm (CommandRouter, INT-002 thay):
//   Thanh tiêu đề   TitleBar (UIA-002); ô Tìm lệnh lấy vị trí ribbon từ RibbonModel::locate
//   Ribbon          tạm: TabBar (UI0-106) + RibbonPages của RibbonModel/RibbonControls (UIA-102), macroRequested →
//                   router. ponytail: UIA-005 (RibbonBar: khung panel, co panel theo ưu tiên) thay; tới đó trang không
//                   co, phần tràn ở màn hẹp bị cắt (không đẩy cửa sổ rộng hơn màn hình); hồ sơ Viewer vẫn thấy 8 tab
//                   2D có huy hiệu khóa (ribbon giản lược UI-M01 chưa có)
//   Tab tệp         FileTabBar (UIA-010): commandRequested → chọn tab của tài liệu rồi router; openRequested →
//                   Application::requestOpen (Application nối); Ctrl+Tab / Ctrl+Shift+Tab (Keymap "ntab") → next,
//                   previous
//   Dòng lệnh       CommandLine (UIA-107): submitted, commandRequested → router; cancelRequested → clearPrompt;
//                   COMMANDLINE / COMMANDLINEHIDE (Ctrl+9) ẩn/hiện vùng, CLISTATE theo vùng; Clean Screen bật/tắt
//                   ghi một dòng lịch sử như prototype
//   Thanh trạng thái tạm: ModeToggles (UIA-112), macroRequested → router, hồ sơ theo EditionGate::profileChanged.
//                   ponytail: UIA-009 (StatusBar) thay khung
//   Thông báo       NotificationCenter, ToastHost (cửa sổ chính), ProgressPresenter (cha là cửa sổ chính) theo
//                   BackgroundJobs; toasts->followJobs(presenter): việc xong/lỗi đọc qua toast (UI0-013)
// Vùng vẽ và tab layout vẫn là ô giữ chỗ của MainWindow (UIA-109, UIA-011). Lệnh chỉ đi qua QAction của
// ActionRegistry hoặc router, không tự chạy lệnh.
//
// ponytail: chưa có phiên tài liệu DOC-018: activateRequested chỉ chọn tab, TitleBar::setDocument chưa nối; tài liệu
// vào FileTabBar qua addDocument khi DOC-018/UIB-002 mở được bản vẽ. appMenuRequested (UIA-003), screenRequested
// (UI-L03…), deselectRequested (INT-002 vùng chọn), chú thích nâng cấp → toast (UIA-015) chưa nối. Trạng thái ẩn/hiện
// dòng lệnh chưa lưu qua lần chạy (COMMANDLINE.md mục 5; workspace người dùng 1.0).
//
// Application giữ thứ tự (Application.h): dựng sau MainWindow, CleanScreen, ActionRegistry::install, trước
// MainWindow::restore; hủy sau cửa sổ (RibbonControls sống lâu hơn widget ribbon, NotificationCenter lâu hơn
// ToastHost).
// Thành viên công khai cho bước integration sau (UIA-005, UIA-009 thay ribbon, thanh trạng thái) và test.

#include "app/shell/ribbon/controls/RibbonControls.h"
#include "app/shell/ribbon/RibbonModel.h"
#include "app/widgets/message/Messages.h"

#include <QObject>

namespace trion::keymap {
class Keymap;
}
namespace trion::actions {
class ActionRegistry;
class Sysvars;
} // namespace trion::actions
namespace trion::edition {
class EditionGate;
}
namespace trion::jobs {
class BackgroundJobs;
}
namespace trion::ui {
class CommandLine;
class ProgressPresenter;
class TabBar;
} // namespace trion::ui
namespace trion::shell {
class CleanScreen;
class CommandRouter;
class FileTabBar;
class MainWindow;
class ModeToggles;
class TitleBar;
} // namespace trion::shell

namespace trion::app {

class ShellAssembly : public QObject {
    Q_OBJECT
public:
    // Luồng UI, sau ThemeManager, IconProvider, RichTooltip, Translator. Mọi tham số sống lâu hơn ShellAssembly.
    ShellAssembly(shell::MainWindow& window, keymap::Keymap& keymap, actions::ActionRegistry& actions,
                  actions::Sysvars& sysvars, edition::EditionGate& gate, shell::CommandRouter& router,
                  shell::CleanScreen& clean, jobs::BackgroundJobs& jobs);
    ~ShellAssembly() override;

    shell::MainWindow& window;
    actions::ActionRegistry& actions;
    actions::Sysvars& sysvars;
    edition::EditionGate& gate;
    shell::CommandRouter& router;
    shell::CleanScreen& clean;

    ui::NotificationCenter notes;
    ribbon::RibbonModel ribbonModel;
    ribbon::RibbonControls ribbonControls;

    // Widget: cửa sổ sở hữu (hủy trước ShellAssembly).
    shell::TitleBar* titleBar = nullptr;
    ui::TabBar* ribbonTabs = nullptr;
    ribbon::RibbonPages* ribbonPages = nullptr;
    shell::FileTabBar* fileTabs = nullptr;
    ui::CommandLine* commandLine = nullptr;
    shell::ModeToggles* modes = nullptr;
    ui::ToastHost* toasts = nullptr;
    ui::ProgressPresenter* presenter = nullptr;

protected:
    bool eventFilter(QObject* watched, QEvent* e) override; // CLISTATE theo vùng dòng lệnh

private:
    QWidget* buildRibbon();
    QWidget* ribbonPage(int tab);
    void bindTabKeys(keymap::Keymap& keymap);
};

} // namespace trion::app
