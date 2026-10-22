#pragma once
// Integration nhóm 5, 6 · Lắp khung ứng dụng vào cửa sổ chính (UIA-001) và nối lệnh của chúng vào bộ định tuyến tạm
// (CommandRouter, INT-002 thay):
//   Thanh tiêu đề   TitleBar (UIA-002); ô Tìm lệnh lấy vị trí ribbon từ RibbonModel::locate
//   Ribbon          RibbonBar (UIA-005) trên RibbonModel/RibbonControls (UIA-102): macroRequested → router; RIBBON /
//                   RIBBONCLOSE (docs/specs/commands) ẩn/hiện vùng (mức thu gọn, tab, panel ghim giữ nguyên), RIBBONSTATE
//                   theo lệnh, Clean Screen độc lập (lệnh khi đang bật chỉ ghi nhận, tắt thì áp). addToQatRequested
//                   → Qat::add (UIA-101), Qat::macroRequested → router. F1 khi rê (RichTooltip::helpRequested) →
//                   '_HELP <lệnh>. ponytail: UIL-009 (cửa sổ trợ giúp) thay; RIBBONSTATE chưa lưu theo workspace qua lần
//                   chạy (NT-010), chưa có toast "Gõ RIBBON để hiện lại" [đề xuất] của RIBBONCLOSE.md mục 5
//   Tab tệp         FileTabBar (UIA-010): commandRequested → chọn tab của tài liệu rồi router; openRequested →
//                   Application::requestOpen (Application nối); Ctrl+Tab / Ctrl+Shift+Tab (Keymap "ntab") → next,
//                   previous
//   Vùng Canvas     ngăn xếp {vùng vẽ giữ chỗ của MainWindow (canvas), trang Start (UIA-004)}: trang hiện khi tab Start
//                   (FileTabBar::kStart) đang chọn, kể cả khi đóng tab bản vẽ cuối; focus của vùng (Esc của palette,
//                   COMMANDLINEHIDE) tới trang đang hiện. NEWTAB → chọn tab Start. StartPage: openRequested →
//                   Application::requestOpen (Application nối), macroRequested → router; RecentFiles do Application giữ
//   Dòng lệnh       CommandLine (UIA-107, tự hoàn thành UIA-108): submitted, commandRequested → router;
//                   cancelRequested → clearPrompt; tên gõ ở dấu nhắc lệnh mà router không xử lý và không phải lệnh, biến
//                   (Autocomplete::resolve) → reportUnknown ("Ý bạn là"); huy hiệu khóa của tự hoàn thành theo
//                   RichTooltip::info(QAction).need (EditionGate đặt). COMMANDLINE / COMMANDLINEHIDE (Ctrl+9) ẩn/hiện
//                   vùng, CLISTATE theo vùng; Clean Screen bật/tắt ghi một dòng lịch sử như prototype
//   Thanh trạng thái StatusBar (UIA-009, gồm ModeToggles UIA-112, tự áp hồ sơ theo EditionGate): macroRequested → router
//   Thông báo       NotificationCenter, ToastHost (cửa sổ chính), ProgressPresenter (cha là cửa sổ chính) theo
//                   BackgroundJobs; toasts->followJobs(presenter): việc xong/lỗi đọc qua toast (UI0-013)
// Vùng vẽ và tab layout vẫn là ô giữ chỗ của MainWindow (UIA-109, UIA-011). Lệnh chỉ đi qua QAction của
// ActionRegistry hoặc router, không tự chạy lệnh.
//
// ponytail: chưa có phiên tài liệu DOC-018: activateRequested chỉ chọn tab, TitleBar::setDocument chưa nối; tài liệu
// vào FileTabBar qua addDocument khi DOC-018/UIB-002 mở được bản vẽ. Chưa có màn hình, lệnh đích: screenRequested
// của thanh tiêu đề, thanh trạng thái, trang Start (UIL-001 thông báo, UIL-018 giấy phép, UI-L06 cập nhật, trang web),
// openReadOnlyRequested (DOC-018), browseRequested (CMD-237) chỉ ghi log "trion.app" mức info. appMenuRequested
// (UIA-003), deselectRequested (INT-002 vùng chọn), chú thích nâng cấp → toast (UIA-015) chưa nối. Trạng thái ẩn/hiện
// dòng lệnh chưa lưu qua lần chạy (COMMANDLINE.md mục 5; workspace người dùng 1.0).
//
// Application giữ thứ tự (Application.h): dựng sau MainWindow, CleanScreen, ActionRegistry::install, trước
// MainWindow::restore; hủy sau cửa sổ (RibbonControls sống lâu hơn widget ribbon, NotificationCenter lâu hơn
// ToastHost, RecentFiles lâu hơn trang Start).
// Thành viên công khai cho bước integration sau (UIA-109 thay canvas, hàng QAT dưới ribbon) và test.

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
namespace trion::ribbon {
class RibbonBar;
}
namespace trion::ui {
class CommandLine;
class ProgressPresenter;
} // namespace trion::ui
namespace trion::shell {
class CleanScreen;
class CommandRouter;
class FileTabBar;
class MainWindow;
class ModeToggles;
class RecentFiles;
class StartPage;
class StatusBar;
class TitleBar;
} // namespace trion::shell

namespace trion::app {

class ShellAssembly : public QObject {
    Q_OBJECT
public:
    // Luồng UI, sau ThemeManager, IconProvider, RichTooltip, Translator. Mọi tham số sống lâu hơn ShellAssembly.
    ShellAssembly(shell::MainWindow& window, keymap::Keymap& keymap, actions::ActionRegistry& actions,
                  actions::Sysvars& sysvars, edition::EditionGate& gate, shell::CommandRouter& router,
                  shell::CleanScreen& clean, jobs::BackgroundJobs& jobs, shell::RecentFiles& recent);
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
    ribbon::RibbonBar* ribbon = nullptr;
    shell::FileTabBar* fileTabs = nullptr;
    QWidget* canvas = nullptr;          // vùng vẽ giữ chỗ của MainWindow trong ngăn xếp của vùng Canvas
    shell::StartPage* start = nullptr;  // trang Start, cùng ngăn xếp
    ui::CommandLine* commandLine = nullptr;
    shell::StatusBar* statusBar = nullptr;
    shell::ModeToggles* modes = nullptr; // của statusBar
    ui::ToastHost* toasts = nullptr;
    ui::ProgressPresenter* presenter = nullptr;

protected:
    bool eventFilter(QObject* watched, QEvent* e) override; // CLISTATE theo vùng dòng lệnh

private:
    void buildCanvas(shell::RecentFiles& recent);
    void bindTabKeys(keymap::Keymap& keymap);
};

} // namespace trion::app
