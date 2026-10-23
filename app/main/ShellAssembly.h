#pragma once
// Integration nhóm 5, 6, 7 · Lắp khung ứng dụng vào cửa sổ chính (UIA-001) và nối lệnh của chúng vào bộ định tuyến
// tạm (CommandRouter, INT-002 thay):
//   Thanh tiêu đề   TitleBar (UIA-002); ô Tìm lệnh lấy vị trí từ RibbonBar::locate (ribbon của hồ sơ). Nút logo →
//                   menu ứng dụng AppMenu (UIA-003): macroRequested → router, openRequested → Application::requestOpen
//                   (Application nối); ảnh tệp gần đây như trang Start (ponytail: DWG-026, RND-021 đặt nguồn cho cả hai)
//   QAT             Qat (UIA-101) của thanh tiêu đề: addToQatRequested của ribbon → Qat::add, macroRequested → router;
//                   "Hiện bên dưới ribbon" → hàng dưới ribbon của MainWindow (khởi động theo isBelowRibbon), thứ tự Tab
//                   theo hàng (sau ribbon, trước tab tệp; về thanh tiêu đề: sau nút logo)
//   Ribbon          RibbonBar (UIA-005) trên RibbonModel/RibbonControls (UIA-102), theo hồ sơ của EditionGate (Viewer:
//                   ribbon UI-M01 + panel nâng cấp, UIA-017): macroRequested → router; RIBBON / RIBBONCLOSE
//                   (docs/specs/commands) ẩn/hiện vùng (mức thu gọn, tab, panel ghim giữ nguyên), RIBBONSTATE theo lệnh,
//                   lưu theo workspace qua lần chạy ("ribbon/state"; mức thu gọn RibbonBar tự lưu), áp lại khi
//                   workspace áp bố cục; Clean Screen độc lập (lệnh khi đang bật chỉ ghi nhận, tắt thì áp). Lần đầu
//                   RIBBONCLOSE trong phiên: toast "Gõ RIBBON để hiện lại" (RIBBONCLOSE.md mục 5). F1 khi rê
//                   (RichTooltip::helpRequested) → '_HELP <lệnh>. ponytail: UIL-009 (cửa sổ trợ giúp) thay
//   Tab tệp         FileTabBar (UIA-010): commandRequested → chọn tab của tài liệu rồi router; openRequested →
//                   Application::requestOpen (Application nối); Ctrl+Tab / Ctrl+Shift+Tab (Keymap "ntab") → next,
//                   previous
//   Vùng Canvas     ngăn xếp {vùng vẽ giữ chỗ của MainWindow (canvas), trang Start (UIA-004)}: trang hiện khi tab Start
//                   (FileTabBar::kStart) đang chọn, kể cả khi đóng tab bản vẽ cuối; focus của vùng (Esc của palette,
//                   COMMANDLINEHIDE) tới trang đang hiện. NEWTAB → chọn tab Start. StartPage: openRequested →
//                   Application::requestOpen (Application nối), macroRequested → router; RecentFiles do Application giữ
//   Dòng lệnh       CommandLine (UIA-107, tự hoàn thành UIA-108, UIA-007): submitted, commandRequested → router;
//                   tên lệnh gõ ở dấu nhắc lệnh mà bị khóa theo phiên bản (EditionGate::locked(ActionRegistry::need))
//                   → notifyLocked, không chạy; chip của nó → EditionGate::upgrade().open. cancelRequested →
//                   clearPrompt; tên gõ ở dấu nhắc lệnh mà router không xử lý và không phải lệnh, biến
//                   (Autocomplete::resolve) → reportUnknown ("Ý bạn là"); huy hiệu khóa của tự hoàn thành theo
//                   RichTooltip::info(QAction).need (EditionGate đặt). COMMANDLINE / COMMANDLINEHIDE (Ctrl+9) ẩn/hiện
//                   vùng, CLISTATE theo vùng; Clean Screen bật/tắt ghi một dòng lịch sử như prototype. TEXTSCR (F2,
//                   nút ^) mở HistoryWindow, INPUTSEARCHOPTIONS mở InputSearchOptions qua DialogHost (dialogs; mở lần
//                   nữa đưa bản đang mở lên). Kéo dải nắm: floatRequested/dockRequested → MainWindow::
//                   setCommandLineFloating (nổi trên vùng vẽ, lưu và khôi phục qua lần chạy); trang của vùng Canvas đổi
//                   thì dòng lệnh nổi lên lại trên cùng
//   Viewer          palette "layers" (MainWindow::setPaletteContent): hồ sơ Viewer là ViewerLayers (UIA-118),
//                   macroRequested → router; hồ sơ đầy đủ giữ ô giữ chỗ (UIC-009). ViewStateFile (viewStates, một cho
//                   cả phiên; banner trên vùng vẽ): saveViewRequested → save(tài liệu hiện hành), saved → toast, loaded
//                   → router '_-LAYER (LayerMacro). ponytail: đổi layout, view của .tcview chờ RND; load(đường dẫn) do
//                   DOC-018 gọi khi mở bản vẽ ở hồ sơ Viewer
//   Thanh trạng thái StatusBar (UIA-009, gồm ModeToggles UIA-112, tự áp hồ sơ theo EditionGate): macroRequested → router
//   Thông báo       NotificationCenter, ToastHost (cửa sổ chính), ProgressPresenter (cha là cửa sổ chính) theo
//                   BackgroundJobs; toasts->followJobs(presenter): việc xong/lỗi đọc qua toast (UI0-013)
// Vùng vẽ và tab layout vẫn là ô giữ chỗ của MainWindow (UIA-109, UIA-011). Lệnh chỉ đi qua QAction của
// ActionRegistry hoặc router, không tự chạy lệnh.
//
// ponytail: chưa có phiên tài liệu DOC-018: activateRequested chỉ chọn tab, TitleBar::setDocument chưa nối; tài liệu
// vào FileTabBar qua addDocument khi DOC-018/UIB-002 mở được bản vẽ; đường dẫn tài liệu hiện hành đọc từ tooltip của
// tab (documentPath). Chưa có màn hình, lệnh đích: screenRequested của thanh tiêu đề, thanh trạng thái, trang Start
// (UIL-001 thông báo, UIL-018 giấy phép, UI-L06 cập nhật, trang web), openReadOnlyRequested (DOC-018),
// browseRequested (CMD-237) chỉ ghi log "trion.app" mức info. deselectRequested (INT-002 vùng chọn), chú thích nâng
// cấp → toast (UIA-015) chưa nối. Trạng thái ẩn/hiện dòng lệnh chưa lưu qua lần chạy (COMMANDLINE.md mục 5;
// workspace người dùng 1.0). Chưa lắp (Integration nhóm 8): CanvasWidget (UIA-109) vào chỗ canvas của ngăn xếp,
// Onboarding (UIA-016), OptionsDialog (UID-019), ColorPickerDialog (UIC-017). 'MENUBAR là hành động sysvar của
// ActionRegistry (QAT dùng thẳng).
//
// Application giữ thứ tự (Application.h): dựng sau MainWindow, CleanScreen, ActionRegistry::install, hồ sơ của
// Workspace, trước MainWindow::restore; hủy sau cửa sổ (RibbonControls sống lâu hơn widget ribbon, NotificationCenter
// lâu hơn ToastHost, RecentFiles lâu hơn trang Start, menu ứng dụng; DialogHost sau hộp thoại).
// Thành viên công khai cho bước integration sau (UIA-109 thay canvas) và test.

#include "app/shell/ribbon/controls/RibbonControls.h"
#include "app/shell/ribbon/RibbonModel.h"
#include "app/widgets/dialog/DialogHost.h"
#include "app/widgets/message/Messages.h"

#include <QObject>
#include <QPointer>

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
class AppMenu;
class CleanScreen;
class CommandRouter;
class FileTabBar;
class MainWindow;
class ModeToggles;
class RecentFiles;
class StartPage;
class StatusBar;
class TitleBar;
class ViewerLayers;
class ViewStateFile;
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
    ui::DialogHost dialogs; // hộp thoại của cửa sổ chính theo mã: "textscr", "inputsearchoptions"

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
    shell::AppMenu* appMenu = nullptr;           // popup của cửa sổ
    ui::CanvasBanner* banner = nullptr;          // banner trên vùng vẽ (con của canvas)
    shell::ViewStateFile* viewStates = nullptr;  // .tcview; con của ShellAssembly
    QPointer<shell::ViewerLayers> viewerLayers;  // palette "layers" ở hồ sơ Viewer; dựng ở lần palette hiện đầu

    // ponytail: đường dẫn của tài liệu hiện hành (tooltip của tab đang chọn); rỗng ở tab Start hoặc bản vẽ chưa lưu.
    // DOC-018 (phiên tài liệu) thay.
    QString documentPath() const;

protected:
    bool eventFilter(QObject* watched, QEvent* e) override; // CLISTATE theo vùng dòng lệnh

private:
    void buildCanvas(shell::RecentFiles& recent);
    void bindTabKeys(keymap::Keymap& keymap);
    void buildCommandLine();
    void buildViewer();
    void placeQat(bool below);
    void applyRibbonState(); // RIBBONSTATE đã lưu của workspace đang dùng → sysvar, vùng Ribbon
    void setRibbonState(bool on);

    bool ribbonHintShown_ = false; // toast "Gõ RIBBON để hiện lại" một lần mỗi phiên
};

} // namespace trion::app
