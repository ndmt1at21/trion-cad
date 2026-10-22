#pragma once
// UIA-001 · Workspace dựng sẵn (07.1 §3.1): "2d" Soạn thảo 2D (mặc định), "viewer" Viewer. Mô tả JSON nhúng qrc
// (app/shell/resources/workspaces/<id>.json): vùng hiện ("regions": tên MainWindow::Region; vùng vẽ luôn hiện) và
// palette mở theo thứ tự ("palettes": id, cạnh dock left|right|bottom, ghim). Palette đầu tiên của mỗi cạnh là tab
// đang mở; palette không có trong mô tả thì đóng. Ghim chỉ ở lớp màn hình Wide; hẹp hơn thì mọi palette mở nằm ở dải
// tự ẩn (07.1 §3.2).
//
// Hồ sơ giao diện (EditionGate::profile, UI0-011): Viewer ép workspace "viewer" (không đổi lựa chọn đã lưu); Full dùng
// lựa chọn của người dùng ("ui/window/workspace", mặc định "2d"). Nối: connect(&gate, &EditionGate::profileChanged,
// &window.workspace(), &Workspace::setProfile) cùng setProfile(gate.profile()) lúc dựng.
//
// API cho UIA-105 (Cổ điển: thêm <id>.json và tên), UIA-017 (đổi workspace từ thanh trạng thái): apply(id), current(),
// changed. ponytail: đổi workspace áp lại bố cục mặc định của nó, không nhớ bố cục người dùng đã đổi riêng cho từng
// workspace (WSAUTOSAVE của AutoCAD; "Workspace người dùng" 1.0 thêm khi có).
#include "app/core/edition/EditionGate.h"
#include "core/base/settings/Settings.h"

#include <QObject>
#include <QStringList>

namespace trion::shell {

class MainWindow;

class Workspace : public QObject {
    Q_OBJECT
public:
    // Con của window; store sống lâu hơn. Chưa áp gì tới khi MainWindow::restore() (gọi applyLayout).
    Workspace(MainWindow& window, settings::Store& store);

    static QStringList ids();               // {"2d", "viewer"}
    static QString name(const QString& id); // tên đã dịch; rỗng nếu id lạ

    QString chosen() const;  // lựa chọn đã lưu của người dùng
    QString current() const; // workspace đang dùng: "viewer" khi hồ sơ Viewer, không thì chosen()

    // Người dùng chọn workspace: lưu, áp bố cục mặc định của workspace đang dùng, phát changed nếu current() đổi.
    // id lạ: bỏ qua.
    void apply(const QString& id);
    // Hồ sơ của phiên bản; chưa restore() thì chỉ ghi nhận.
    void setProfile(trion::edition::UiProfile profile);

    // MainWindow::restore: keepSaved (bố cục palette đã khôi phục là của người dùng) thì giữ, không thì applyLayout().
    // Từ đây setProfile áp lại bố cục khi workspace đổi.
    void restore(bool keepSaved);
    // Áp mô tả của current(): vùng hiện, palette mở, cạnh, ghim theo lớp màn hình của cửa sổ; ghi nhận làm bố cục mặc
    // định. MainWindow gọi khi khôi phục và khi đổi lớp màn hình.
    void applyLayout();
    // Bố cục palette hiện tại còn đúng bố cục mặc định vừa áp (cùng palette mở, cạnh, ghim, nổi, dải).
    bool isDefaultLayout() const;

signals:
    void changed(const QString& id);

private:
    QString signature() const;

    MainWindow& window_;
    settings::Store& store_;
    bool viewer_ = false;
    bool restored_ = false;
    QString defaultSignature_;
};

} // namespace trion::shell
