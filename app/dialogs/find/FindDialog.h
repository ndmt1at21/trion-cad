#pragma once
// UIF-001 · Tìm chữ (lệnh FIND; 07.2 UI-F09; 07.1 §7.4 panel Văn bản › Tìm và thay thế). Prototype: SC.find trong
// js/screens1.js (Viewer, "Tìm chữ") và js/screens8.js (Pro, "Tìm và thay thế"); css .frow .chk .grid3 .tbl .tag.warn
// .empty. Hộp thoại không modal trên DialogFrame (UI0-110), mã "find":
//   Tìm           ô nhập: gõ là tìm (lượt cũ hủy); Enter, nút Tìm tìm lại; ↓ vào danh sách kết quả
//   Thay bằng     Viewer, Viewer Pro: vô hiệu, huy hiệu Pro (UI0-011); Pro: nhập được (thay thật: UIF-002)
//   Phạm vi       Toàn bản vẽ | Không gian hiện tại | Vùng chọn; nút chọn đối tượng ẩn tạm hộp (hideForPick)
//   Tùy chọn tìm  thu gọn được (nhớ trong Store) — Phân biệt hoa thường, Nguyên từ, Wildcard (* ?), Phân biệt dấu tiếng
//                 Việt | Tìm trong tham chiếu ngoài (Xref), Tìm trong block, Bỏ qua đối tượng ẩn, Tìm cả chữ mã TCVN3,
//                 VNI | Loại chữ: Attribute, Chữ kích thước và đường dẫn, Text và MText, Chữ trong bảng, Hyperlink.
//                 Giá trị lưu trong Store "find/…" (FindMatcher.h: nghĩa của từng tùy chọn).
//   Kết quả       bảng ảo hóa (VirtualTableView): Vị trí | Loại (+ nhãn TCVN3/VNI) | Đoạn chữ (đoạn khớp chữ đậm trên
//   nền
//                 --c-warn-soft). Chưa gõ: trạng thái rỗng giải thích tùy chọn dấu; xong mà không có: "Không tìm thấy
//                 “…”".
//   Chân          số kết quả (vòng quay khi đang tìm) | Tạo tập chọn (mọi kết quả; menu: các dòng đang chọn) | Thay,
//                 Thay tất cả (khóa ở Viewer: mở trang nâng cấp) | Tìm
//
// Tìm ở nền (DoD-U 4, DoD-H 1): nguồn chia thành đoạn (kFirstSlice mục, rồi kSlice mục), mỗi đoạn một việc NT-005 với
// FindMatcher riêng (đoạn đầu ưu tiên Interactive). Kết quả về luồng UI (UiExecutor) và vào bảng đúng thứ tự nguồn:
// đoạn c chỉ vào khi các đoạn trước đã vào, nên kết quả đầu hiện khi đoạn đầu xong, phần còn lại đổ theo lô, số kết quả
// cập nhật. Đổi chuỗi, tùy chọn, phạm vi hay nguồn thì hủy lượt cũ (việc xét hủy mỗi FindMatcher::kCheckEvery mục) và
// bỏ kết quả cũ; bảng cũ giữ đến khi đoạn đầu của lượt mới về (không nháy rỗng giữa hai phím).
//
// Chỗ nối (quyết định chủ dự án: giao diện trước, lõi sau):
//   setTexts           nguồn chữ, snapshot — ponytail: API tìm của CMD:FIND trên snapshot DOC-009 thay (FindMatcher.h)
//   setCurrentSpace    layout hiện tại cho phạm vi "Không gian hiện tại" (khung, INT)
//   setSelection       tập chọn hiện tại cho phạm vi "Vùng chọn"; nút chọn → pickRequested, chọn xong gọi endPick
//   objectRequested    dòng hiện hành đổi (bấm, ↑↓, Enter): zoom tới đối tượng, tự chuyển layout (INT-016), tô sáng
//                      nó (RND-013)
//   highlightCleared   đóng hộp, lượt tìm mới thay kết quả: bỏ tô sáng
//   selectionRequested Tạo tập chọn: id đối tượng không trùng theo thứ tự kết quả → lệnh chọn (INT-002)
//   replaceRequested   Thay / Thay tất cả ở bản Pro — ponytail: UIF-002 làm phần thay (lệnh, một bước hoàn tác, toast)
//
// Dùng (integration, với DialogHost; FIND là lệnh có ở mọi phiên bản):
//   dialogs.add(u"find"_s, [&](QWidget* parent) {
//       auto* d = new FindDialog(gate, parent);
//       d->setTexts(findSource());                       // ponytail: CMD:FIND
//       d->setCurrentSpace(currentLayoutName());
//       connect(d, &FindDialog::objectRequested, …);     // INT-016 + RND-013
//       connect(d, &FindDialog::selectionRequested, …);  // INT-002
//       return d;
//   });
//   router.add(u"FIND"_s, [&] { dialogs.open(u"find"_s); });

#include "app/dialogs/find/FindMatcher.h"
#include "app/widgets/dialog/DialogFrame.h"

#include <QList>

#include <memory>
#include <vector>

class QAbstractItemView;
class QCheckBox;
class QLabel;
class QLineEdit;
class QStackedWidget;

namespace trion::edition {
class EditionGate;
class LockBadge;
} // namespace trion::edition

namespace trion::ui {

class EmptyState;
class Menu;
class Spinner;
class VirtualTableView;

class FindDialog : public DialogFrame {
    Q_OBJECT
public:
    enum Column { WhereColumn, KindColumn, SnippetColumn, ColumnCount };
    enum Role {
        IdRole = Qt::UserRole + 1, // quint64: TextItem::id
        LayoutRole,                // QString: TextItem::layout
        MatchRole,                 // QPoint(đầu, dài) của đoạn khớp trong chữ cột Đoạn chữ
        LegacyRole,                // QString: "TCVN3", "VNI" hoặc rỗng
    };
    // Mục mỗi việc nền: đoạn đầu nhỏ (~1 ms CPU, release) để kết quả đầu về sớm cả khi máy bận, sau đó ~8 ms mỗi đoạn.
    static constexpr int kFirstSlice = 1024;
    static constexpr int kSlice = 8192;

    // Cần ThemeManager, IconProvider, UiExecutor (như main). Đọc tùy chọn từ settings::Store::instance().
    explicit FindDialog(edition::EditionGate& gate, QWidget* parent = nullptr);
    ~FindDialog() override;

    // Nguồn chữ (snapshot bất biến; null = bản vẽ không có chữ). Đang có chuỗi tìm thì tìm lại.
    // ponytail: integration đẩy danh sách dựng sẵn; CMD:FIND (API tìm trên snapshot DOC-009) thay.
    void setTexts(std::shared_ptr<const find::TextList> texts);
    // Layout hiện tại ("Model" hoặc tên layout). Đang ở phạm vi Không gian hiện tại thì tìm lại.
    // ponytail: integration đẩy khi đổi tab layout; DOC-009 (layout hiện hành trong snapshot) thay.
    void setCurrentSpace(const QString& layout);
    // Tập chọn hiện tại của bản vẽ. Đang ở phạm vi Vùng chọn thì tìm lại.
    void setSelection(const QList<quint64>& ids);
    // Kết thúc lượt chọn đã mở bằng pickRequested: hiện lại hộp; picked (đã setSelection tập mới) thì phạm vi thành
    // Vùng chọn, không (Esc) thì giữ như cũ.
    void endPick(bool picked);

    // Tùy chọn theo các điều khiển hiện tại (gồm phạm vi).
    find::FindOptions options() const;
    // Tìm lại ngay với chuỗi, tùy chọn, nguồn hiện tại (nút Tìm, Enter). Chuỗi rỗng: về trạng thái chưa gõ.
    void search();
    bool isSearching() const;
    int resultCount() const;

    QLineEdit* queryField() const { return query_; }
    QAbstractItemView* results() const;

    void accept() override; // nút Tìm, Enter: tìm lại, không đóng (không modal)

signals:
    // ponytail: chỉ tín hiệu; INT-016 (zoom tới đối tượng, tự chuyển layout) và RND-013 (tô sáng) nối vào.
    void objectRequested(quint64 id, const QString& layout);
    void highlightCleared();
    // ponytail: chỉ tín hiệu; lệnh chọn của INT-002 (SELECT theo id) nối vào.
    void selectionRequested(const QList<quint64>& ids);
    // ponytail: hộp đã ẩn tạm; chuỗi lệnh chọn đối tượng của INT-002 trả tập chọn qua setSelection + endPick.
    void pickRequested();
    // ponytail: bản Pro chỉ phát tín hiệu; UIF-002 làm phần thay (lệnh, một bước hoàn tác, toast).
    void replaceRequested(bool all);
    void searchFinished(int count); // lượt tìm xong, mọi kết quả đã vào bảng

public slots:
    void done(int result) override; // hủy lượt tìm, bỏ tô sáng, rồi đóng

protected:
    void changeEvent(QEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* e) override; // ↓ ở ô Tìm, Enter ở bảng

private:
    class Results;
    struct Run;

    void merge(quint64 generation, int slice, std::vector<find::Hit> hits);
    void currentChanged(const QModelIndex& current);
    void makeSelection(bool selectedRowsOnly);
    void clearHighlight();
    void syncEdition();
    void updateState();
    void restyle();
    void retranslate();

    edition::EditionGate& gate_;
    // = nullptr: DialogFrame cho widget hiện (polish → eventFilter, changeEvent) ngay trong hàm dựng, trước khi gán
    QLineEdit* query_ = nullptr;
    QLineEdit* replace_ = nullptr;
    DropDown* scope_ = nullptr;
    Button* pick_ = nullptr;
    Button* optionsToggle_ = nullptr;
    QWidget* optionsBox_ = nullptr;
    QCheckBox* matchCase_ = nullptr;
    QCheckBox* wholeWord_ = nullptr;
    QCheckBox* wildcard_ = nullptr;
    QCheckBox* accents_ = nullptr;
    QCheckBox* xrefs_ = nullptr;
    QCheckBox* blocks_ = nullptr;
    QCheckBox* skipHidden_ = nullptr;
    QCheckBox* legacy_ = nullptr;
    QCheckBox* kindAttributes_ = nullptr;
    QCheckBox* kindDimLeader_ = nullptr;
    QCheckBox* kindTexts_ = nullptr;
    QCheckBox* kindTables_ = nullptr;
    QCheckBox* kindLinks_ = nullptr;
    QLabel* kindsHeading_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    VirtualTableView* view_ = nullptr;
    Results* model_ = nullptr;
    EmptyState* empty_ = nullptr;
    Spinner* spinner_ = nullptr;
    QLabel* count_ = nullptr;
    SplitButton* makeSet_ = nullptr;
    Menu* setMenu_ = nullptr;
    Button* replaceOne_ = nullptr;
    Button* replaceAll_ = nullptr;
    struct Text {
        QWidget* widget;
        const char* source;
    };
    QList<Text> labels_; // nhãn, ô đánh dấu: chữ nguồn để dịch lại

    std::shared_ptr<const find::TextList> source_;
    QString space_;
    std::shared_ptr<const std::vector<std::uint64_t>> selection_; // đã sắp
    sched::CancelSource cancel_;
    quint64 generation_ = 0;
    std::unique_ptr<Run> run_; // lượt đang tìm hoặc vừa xong; null: chưa gõ
    bool highlighted_ = false; // đã phát objectRequested từ lần bỏ tô sáng gần nhất
};

} // namespace trion::ui
