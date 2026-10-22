#pragma once
// UIA-001 · Clean Screen (CLEANSCREENON, CLEANSCREENOFF; docs/specs/commands/CLEANSCREENON.md): thu gọn giao diện,
// chỉ còn vùng vẽ, dòng lệnh, thanh trạng thái (prototype #app.clean, nút sb-clean). Bật: ẩn thanh tiêu đề, ribbon,
// tab tệp, tab layout, mọi palette (dock, nổi, dải tự ẩn và khung trượt) và toolbar của vùng dock. Tắt: hiện lại đúng
// những gì đã ẩn (QMainWindow giữ chỗ và cỡ của dock ẩn) và đưa tab đang chọn của mỗi nhóm lên lại, nên bố cục về
// đúng từng pixel. Không dùng QMainWindow::restoreState: Qt gửi Close cho dock đang ẩn, PaletteHost hiểu là đóng
// palette và bỏ nó khỏi dải tự ẩn.
// CLEANSCREENSTATE (Sysvars, 0/1) theo trạng thái; Ctrl+0 (Keymap "clean") gửi '_CLEANSCREENON hoặc '_CLEANSCREENOFF
// theo nó như macro CUI của AutoCAD (ActionRegistry), qua CommandRouter tới đây.
//
// Dùng (sau MainWindow, ActionRegistry nối sink vào router):
//   new shell::CleanScreen(window, sysvars, router);   // con của window; đăng ký CLEANSCREENON/OFF vào router
// Cửa sổ lưu (MainWindow::aboutToSave) hoặc đổi workspace khi đang bật: tắt trước (không lưu bố cục đã ẩn; workspace
// mới tự hiện vùng và palette của nó).
#include "app/shell/MainWindow.h"

#include <QList>
#include <QObject>
#include <QPointer>

class QDockWidget;

namespace trion::actions {
class Sysvars;
}

namespace trion::shell {

class CommandRouter;

class CleanScreen : public QObject {
    Q_OBJECT
public:
    CleanScreen(MainWindow& window, actions::Sysvars& sysvars, CommandRouter& router);

    bool isOn() const { return on_; }
    void setOn(bool on); // trùng trạng thái: không làm gì

signals:
    void toggled(bool on);

private:
    void finish(bool on);

    MainWindow& window_;
    actions::Sysvars& sysvars_;
    bool on_ = false;
    QList<MainWindow::Region> hidden_;    // vùng đã ẩn
    QList<QPointer<QWidget>> shown_;      // palette, dải, toolbar, nhóm tab nổi đã ẩn
    QList<QPointer<QDockWidget>> raised_; // tab đang chọn của các nhóm palette ở dock
};

} // namespace trion::shell
