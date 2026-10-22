#pragma once
// UIA-002 · Ô Tìm lệnh trên thanh tiêu đề (07.1 §3 dòng Thanh tiêu đề; prototype TC.search trong js/ribbons.js,
// .tb-search, .sres trong css/app.css).
//
// Tìm theo tên lệnh tiếng Anh, viết tắt, nhãn và từ khóa tiếng Việt, không phân biệt dấu, hoa/thường ("duong tron"
// khớp "Đường tròn" → CIRCLE). Mỗi phím: xếp hạng lại toàn bộ chỉ mục (vài trăm lệnh, vài chục µs), hiện tối đa
// kMaxResults dòng trong popup của QCompleter (giữ focus và bộ gõ ở ô, đóng khi bấm ra ngoài). Mỗi dòng: biểu tượng,
// nhãn (phần khớp tô --c-accent), huy hiệu khóa khi lệnh bị khóa theo phiên bản, tên lệnh · viết tắt, phím tắt, vị
// trí ribbon ("Trang chủ › Vẽ"). ↑↓ chọn, Enter hoặc bấm chạy lệnh qua QAction của ActionRegistry (cổng phiên bản
// UI0-011: lệnh khóa mở trang nâng cấp thay vì chạy), Esc xóa chữ, đóng và trả focus về chỗ cũ.
//
// Dùng (TitleBar tạo; bước integration nối vị trí ribbon):
//   auto* search = new CommandSearch(actions);
//   search->setLocator([&](const QString& cmd) { return ribbonModel.location(cmd); }); // UIA-102
//
// ponytail: chỉ mục tạm thay INT-017: actions::searchTerms() (app/core/actions/SearchTerms.h: viết tắt, từ khóa sinh
// từ RAW của prototype) + nhãn của QAction; chưa có tần suất dùng, bài trợ giúp, thiết lập (nhóm "Bài trợ giúp",
// "Thiết lập" của prototype) và lệnh LISP, plugin. Dựng lần đầu khi tìm (đo < 2 ms, không cần dựng nền), dựng lại
// khi đổi ngôn ngữ.

#include "app/widgets/controls/Controls.h"

#include <QPointer>
#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

class QAbstractItemView;
class QAction;
class QCompleter;

namespace trion::actions {
class ActionRegistry;
}

namespace trion::shell {

class CommandSearch : public ui::SearchField {
    Q_OBJECT
public:
    static constexpr int kMaxResults = 6; // như prototype

    // actions sống lâu hơn ô. Cần ThemeManager, IconProvider, RichTooltip như Menu.
    explicit CommandSearch(actions::ActionRegistry& actions, QWidget* parent = nullptr);
    ~CommandSearch() override;

    // Vị trí ribbon của lệnh ("Trang chủ › Vẽ"), rỗng nếu lệnh không có trên ribbon.
    // ponytail: chỗ nối tạm; bước integration nối RibbonModel (UIA-102), INT-001 có metadata vị trí thì thay.
    using Locator = std::function<QString(const QString& command)>;
    void setLocator(Locator locator);

    // Tên lệnh khớp chuỗi gõ, xếp hạng như popup (tối đa kMaxResults). Cho test, QA.
    QStringList search(const QString& query) const;
    QAbstractItemView* popup() const; // danh sách kết quả (cửa sổ popup); cho test, QA

protected:
    bool event(QEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void changeEvent(QEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* e) override;

private:
    struct Entry;
    class Model;
    class Delegate;
    class View;

    const std::vector<Entry>& index() const; // dựng khi cần
    std::vector<int> rank(const QString& query) const;
    void refresh(); // kết quả theo chữ đang gõ
    void run(const QString& command); // QAction của lệnh; rỗng: không làm gì
    void dismiss(); // xóa chữ, đóng popup, trả focus
    bool handleKey(QKeyEvent* e);
    void retranslate();

    actions::ActionRegistry& actions_;
    Locator locator_;
    mutable std::vector<Entry> index_;
    Model* model_;
    View* view_;
    QCompleter* completer_;
    QPointer<QWidget> back_; // focus trước khi vào ô
};

} // namespace trion::shell
