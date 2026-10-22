#pragma once
// UIA-002, UIA-101 · Quick Access Toolbar (07.1 UI-A04; design/README.md §Quy ước: nút tách hoàn tác 12 bước;
// prototype renderTitle, qatMenu trong js/shell.js, ED.undoMenu trong js/editor.js; .qat .qsplit .qdd .sep trong
// css/app.css):
//   [Mới][Mở][Lưu][Lưu thành] | [Hoàn tác▾][Làm lại▾] | [In▾][lệnh thêm…][▾]
//
// - Mục hiện theo bộ giao diện (EditionGate::profile), mỗi bộ một danh sách lưu riêng qua Store (NT-010): đầy đủ mặc
//   định 7 mục như trên (kFullItems); Viewer, Viewer Pro chỉ Mở, In (kViewerItems). Hạ cấp về Viewer không mất tùy
//   biến của bản đầy đủ. Thứ tự: các mục có sẵn theo thứ tự cố định, rồi lệnh thêm từ ribbon theo thứ tự thêm; vạch
//   chia quanh nhóm Hoàn tác/Làm lại khi nhóm đó và nhóm bên cạnh cùng có nút.
// - Nút dùng QAction của ActionRegistry: bấm = trigger (cổng phiên bản UI0-011 rồi chuỗi lệnh tới INT-002); tooltip,
//   phím, khóa theo QAction. Lệnh bị khóa: nút có ổ khóa; bật mục khóa (menu tùy biến, "Thêm vào QAT") mở trang nâng
//   cấp thay vì thêm nút.
// - Hoàn tác, Làm lại: nút tách; mũi tên (Alt+↓) mở danh sách tối đa 12 bước gần nhất, mới nhất trên cùng, lấy từ
//   chỗ nối setUndoSource/setRedoSource lúc mở. Chọn bước thứ k phát đúng một chuỗi lệnh "^C^C_UNDO k" (Làm lại:
//   "^C^C_MREDO k") qua macroRequested. Danh sách chỉ hỏi nguồn 12 bước nên mở tốn như nhau với stack 10 hay 10.000
//   bước; 12 dòng không cần ảo hóa (DoD-U 4: > 200 dòng).
// - Menu tùy biến (nút ▾): bật/tắt các mục có sẵn (7 mục mặc định, Khớp thuộc tính, In hàng loạt, Xem trước bản in,
//   Thuộc tính, Chuyển không gian làm việc, Bác sĩ Font) và lệnh đã thêm từ ribbon; "Hiện bên dưới ribbon"; "Hiện
//   thanh menu" ('MENUBAR 1|0 qua macroRequested); "Đặt lại mặc định" (danh sách của bộ giao diện đang dùng).
// - "Hiện bên dưới ribbon": QAT không tự dời; lưu lựa chọn rồi phát belowRibbonChanged. Cửa sổ chính đặt widget này
//   vào hàng dưới ribbon khi true; khi false TitleBar lấy lại về chỗ cũ trên thanh tiêu đề.
//
// Dùng (bước integration, luồng UI; TitleBar tạo Qat, Store::instance() là kho của ứng dụng):
//   Qat* qat = titleBar->qat();
//   connect(qat, &Qat::macroRequested, &router, &CommandRouter::submit);        // UNDO k, MREDO k, 'MENUBAR
//   connect(ribbon, &RibbonBar::addToQatRequested, qat, &Qat::add);             // chuột phải ribbon
//   qat->setUndoSource([&](int n) { return steps(history.undo(n)); });         // DOC-008, CMD-232 UndoHistory
//   qat->setRedoSource([&](int n) { return steps(history.redo(n)); });
//   connect(&sysvars, &Sysvars::changed, qat, [qat](const QString& n, int v) {
//       if (n == u"MENUBAR"_s) qat->setMenuBarShown(v != 0); });                // UIA-006 hiện thanh menu theo MENUBAR
//   const auto place = [=](bool below) { if (below) strip->layout()->addWidget(qat); strip->setVisible(below); };
//   connect(qat, &Qat::belowRibbonChanged, strip, place);                       // strip: hàng dưới ribbon
//   place(qat->isBelowRibbon());

#include "core/base/settings/Settings.h"

#include <QWidget>

#include <functional>
#include <utility>
#include <vector>

class QAction;

namespace trion::actions {
class ActionRegistry;
}
namespace trion::edition {
class EditionGate;
}
namespace trion::ui {
class Button;
class Menu;
} // namespace trion::ui

namespace trion::shell {

// Lệnh đang hiện (id của ActionRegistry) theo bộ giao diện; vị trí trên/dưới ribbon.
inline const settings::Setting<QStringList> kFullItems{
    "ui/qat/items",
    {QStringLiteral("NEW"), QStringLiteral("OPEN"), QStringLiteral("QSAVE"), QStringLiteral("SAVEAS"),
     QStringLiteral("U"), QStringLiteral("REDO"), QStringLiteral("PLOT")}};
inline const settings::Setting<QStringList> kViewerItems{"ui/qat/viewerItems",
                                                         {QStringLiteral("OPEN"), QStringLiteral("PLOT")}};
inline const settings::Setting<bool> kBelowRibbon{"ui/qat/belowRibbon", false};

// Một bước hoàn tác/làm lại trong danh sách của nút tách.
struct UndoStep {
    QString command{};     // tên lệnh toàn cục: "MOVE"
    QString description{}; // nhãn hiện trong danh sách: "Di chuyển 3 đối tượng"; rỗng thì hiện command
};

class Qat : public QWidget {
    Q_OBJECT
public:
    static constexpr int kListSteps = 12; // design/README.md §Quy ước: nút tách liệt kê 12 bước
    // Tối đa limit bước gần nhất, mới nhất trước (bước 1 = bước Ctrl+Z sẽ hoàn tác).
    using StepSource = std::function<std::vector<UndoStep>(int limit)>;

    // actions, gate sống lâu hơn QAT. Cần ThemeManager, IconProvider, RichTooltip.
    Qat(actions::ActionRegistry& actions, edition::EditionGate& gate, QWidget* parent = nullptr);

    ui::Button* button(const QString& command) const; // nút của lệnh (kể cả đang ẩn); nullptr nếu không có
    ui::Menu* menu() const { return menu_; }          // menu tùy biến của nút ▾

    // ponytail: chỗ nối tạm thay API lịch sử hoàn tác của DOC-008/CMD-232 (UndoHistory); chưa đặt: danh sách rỗng.
    // Nút chưa mờ đi khi stack rỗng (prototype: disabled) vì chưa có sự kiện đổi stack.
    void setUndoSource(StepSource source) { undoSource_ = std::move(source); }
    void setRedoSource(StepSource source) { redoSource_ = std::move(source); }

    bool isBelowRibbon() const;

public slots:
    void add(const QString& command);   // "Thêm vào thanh truy cập nhanh" của ribbon; đã có hay không có lệnh: bỏ qua
    void reset();                       // danh sách của bộ giao diện đang dùng về mặc định
    void setMenuBarShown(bool shown);   // dấu chọn "Hiện thanh menu" theo sysvar MENUBAR; không phát lệnh

signals:
    void macroRequested(const QString& macro); // "^C^C_UNDO 3", "^C^C_MREDO 1", "'MENUBAR 1" → CommandRouter
    void belowRibbonChanged(bool below);

protected:
    void changeEvent(QEvent* e) override;

private:
    struct Item {
        QAction* action;    // QAction của ActionRegistry
        ui::Button* button; // nút trên QAT
        QAction* toggle;    // mục bật/tắt trong menu tùy biến
        const char* label;  // nhãn của mục (nguồn tiếng Việt); nullptr: lệnh thêm từ ribbon, nhãn theo QAction
        int zone;           // 0 trước nhóm Hoàn tác, 1 nhóm Hoàn tác, 2 sau
    };
    void addItem(QAction* action, const char* label, int zone);
    void setShown(const QString& command, bool shown); // thêm/bớt, lưu, áp
    void apply();                                     // nút, dấu chọn, vạch chia theo danh sách đã lưu
    void fillSteps(ui::Menu* menu, bool redo);         // danh sách của nút tách lúc mở
    const settings::Setting<QStringList>& shownSetting() const; // danh sách của bộ giao diện đang dùng
    void sync(); // nhãn, tooltip, khóa theo QAction (đổi ngôn ngữ, phiên bản)

    actions::ActionRegistry& actions_;
    edition::EditionGate& gate_;
    std::vector<Item> items_;
    ui::Menu* menu_;
    QAction* heading_;
    QAction* itemsEnd_; // vạch chia sau các mục bật/tắt; mục của lệnh thêm chèn trước nó
    QAction* below_;
    QAction* menuBar_;
    QAction* reset_;
    QWidget* seps_[2] = {}; // vạch chia trước và sau nhóm Hoàn tác
    ui::Menu* printMenu_ = nullptr;
    ui::Menu* undoMenu_ = nullptr;
    ui::Menu* redoMenu_ = nullptr;
    ui::Button* more_;
    StepSource undoSource_;
    StepSource redoSource_;
};

} // namespace trion::shell
