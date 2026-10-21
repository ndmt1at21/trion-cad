#pragma once
// UI0-108 · Bảng ảo hóa cho danh sách lớn (DoD-U 4): dòng cao đều theo token --h-row, mỗi khung chỉ chạm các dòng
// đang nhìn. Không giữ gì theo từng dòng, không bố trí lại theo số dòng: mở trên 10⁶ dòng tốn cho view bằng 10⁴ dòng.
// Chọn QAbstractItemView tự viết theo số đo 10⁶ dòng (tests/views_bench.cpp): QTreeView giữ viewItems ~67 byte/dòng
// và bố trí lại O(n) mỗi lần chèn/xóa nên cây phải làm phẳng ở nền; QTableView gần như không tăng (0,25 byte/dòng)
// nhưng không có cây, rê/focus theo ô. Một lớp cho cả bảng và cây (VirtualTreeView thêm thụt lề, mũi tên), vẽ theo
// dòng như .tbl/.tree của prototype.
//
// Dùng, trên luồng UI, sau ThemeManager (và IconProvider cho VirtualTreeView):
//   auto* view = new trion::ui::VirtualTableView(parent);
//   view->setModel(&layerModel);              // BackgroundModel (UI0-014) hoặc model Qt phẳng bất kỳ
//   view->header()->resizeSection(0, 26);     // cột mặc định trước setStateKey; người dùng đổi thì lưu đè
//   view->setStateKey(u"layers"_s);           // rộng, ẩn, thứ tự cột, cột sắp: Store "views/layers"
//   view->setSortingEnabled(true);            // bấm đầu cột → model->sort(cột, chiều)
//   view->setSearchColumn(1);                 // gõ chữ nhảy tới dòng có cột 1 bắt đầu bằng chữ đã gõ
//   layerModel.keepScrollPosition(view);      // UI0-014: cập nhật nền không làm nhảy vùng nhìn
//
// Hợp đồng với model
//  - Phẳng (con của rootIndex()), ít cột: đầu cột là QHeaderView ngang dính trên vùng nhìn.
//  - sort(cột, chiều) không chặn luồng UI: dựng thứ tự mới ở nền rồi báo layoutChanged, chọn và dòng hiện hành đi theo
//    persistent index (BackgroundModel::refresh với cách sắp trong version làm đúng vậy: một hoán vị). Cột -1: thứ tự
//    gốc.
//  - Sửa tại chỗ: flags có ItemIsEditable, setData (palette thật: qua lệnh INT-002). F2 (Enter trên macOS) sửa ô
//    hiện hành; ô đó không sửa được thì ô sửa được đầu tiên của dòng. Editor do delegate tạo, đặt theo .pinp.
//
// Hành vi
//  - Chọn theo dòng, chọn nhiều (Shift/Ctrl, chuột và phím), mũi tên, PgUp/PgDn, Home/End, Ctrl+A của
//    QAbstractItemView; trái/phải đổi cột hiện hành (VirtualTreeView: đóng/mở nút).
//  - Cuộn theo dòng (ScrollPerItem: giá trị thanh cuộn dọc là dòng trên cùng), nên đổi mật độ 22 ↔ 26 px giữ dòng đầu
//    đang hiện. Nền dòng chọn --c-accent-soft, dòng rê --c-hover, đường dưới --c-border như .tbl của prototype.
//  - Gõ chữ: tìm từ dòng hiện hành, không phân biệt dấu và hoa/thường (trion::text::foldForSearch), gõ liền trong
//    keyboardInputInterval thì nối chữ, lặp cùng một phím thì đi qua các dòng bắt đầu bằng chữ đó.
//  - Bố cục cột (rộng, ẩn qua menu chuột phải của đầu cột, cột sắp) lưu ngay khi đổi, nạp lại khi setStateKey/setModel.
//  - Rảnh: không timer, không vẽ lại.
//
// ponytail: không hỗ trợ rootIndex con, ô gộp, dòng cao khác nhau, kéo thả (palette cần thì thêm). QAccessible chỉ thấy
// widget view (Qt chỉ có bảng truy cập cho QTableView/QTreeView/QListView): ô cho trình đọc màn hình thuộc UI0-013.
// Thanh cuộn tính bằng int: tối đa ~8·10⁷ dòng ở 26 px.

#include <QAbstractItemView>
#include <QBasicTimer>
#include <QElapsedTimer>
#include <QStyledItemDelegate>

class QHeaderView;

namespace trion::ui {

class VirtualTableView : public QAbstractItemView {
    Q_OBJECT
public:
    explicit VirtualTableView(QWidget* parent = nullptr);
    ~VirtualTableView() override;

    void setModel(QAbstractItemModel* model) override;
    QHeaderView* header() const { return header_; }

    // Khóa lưu bố cục cột trong settings::Store: "views/<key>". Rỗng: không lưu. Nạp ngay nếu đã có model.
    void setStateKey(const QString& key);
    QString stateKey() const { return stateKey_; }

    // Bấm đầu cột sắp xếp (gọi model->sort ở mỗi lần đổi cột/chiều). Đặt cột sắp mặc định:
    // header()->setSortIndicator(cột, chiều).
    void setSortingEnabled(bool on);
    bool isSortingEnabled() const { return sorting_; }

    // Cột so chữ khi gõ để nhảy (mặc định 0).
    void setSearchColumn(int column) { searchColumn_ = column; }
    int searchColumn() const { return searchColumn_; }

    // Ẩn/hiện cột (cũng qua menu chuột phải của đầu cột); lưu theo khóa view.
    void setColumnHidden(int column, bool hide);
    bool isColumnHidden(int column) const;

    // Cao dòng hiện tại: token --h-row (22 gọn / 26 thoáng).
    int rowHeight() const;

    QRect visualRect(const QModelIndex& index) const override;
    void scrollTo(const QModelIndex& index, ScrollHint hint = EnsureVisible) override;
    QModelIndex indexAt(const QPoint& point) const override;
    void keyboardSearch(const QString& search) override;
    int sizeHintForColumn(int column) const override; // chỉ các dòng đang nhìn
    using QAbstractItemView::edit;

protected:
    // Ô đầy đủ (gồm lề) của index trong tọa độ viewport; cột ẩn: rộng 0 tại đúng dòng.
    QRect cellRect(const QModelIndex& index) const;
    // Dòng tại y của viewport (chia làm tròn xuống, có thể ngoài [0, số dòng)).
    int rowAt(int y) const;
    void updateRow(int row);
    // Vẽ thêm cho ô cột 0 sau delegate (VirtualTreeView: mũi tên đóng/mở). option.state có Selected, MouseOver.
    virtual void drawBranch(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;

    QModelIndex moveCursor(CursorAction action, Qt::KeyboardModifiers modifiers) override;
    int horizontalOffset() const override;
    int verticalOffset() const override;
    bool isIndexHidden(const QModelIndex& index) const override;
    void setSelection(const QRect& rect, QItemSelectionModel::SelectionFlags command) override;
    QRegion visualRegionForSelection(const QItemSelection& selection) const override;
    bool edit(const QModelIndex& index, EditTrigger trigger, QEvent* event) override;
    void updateGeometries() override;
    void rowsInserted(const QModelIndex& parent, int start, int end) override;
    void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;
    void scrollContentsBy(int dx, int dy) override;
    void paintEvent(QPaintEvent* event) override;
    bool viewportEvent(QEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void timerEvent(QTimerEvent* event) override;

    bool rowLines_ = true; // đường --c-border dưới mỗi dòng (.tbl td); cây không có (.tree .tn)

private:
    void updateScrollBars();
    void setHoverRow(int row);
    void saveColumns();
    bool restoreColumns(); // true: cột sắp đổi
    void applySort();
    void columnMenu(const QPoint& pos);
    void continueSearch();

    QHeaderView* header_;
    QString stateKey_;
    bool sorting_ = false;
    int searchColumn_ = 0;
    int hoverRow_ = -1;
    QMetaObject::Connection removed_, reset_;
    struct Search {
        QElapsedTimer typed; // lần gõ gần nhất
        QString text;        // chữ đã gõ liền nhau
        QString needle;      // đã fold
        int next = 0;        // dòng xét tiếp
        int left = 0;        // số dòng còn phải xét
        QBasicTimer slice; // lát kế tiếp: timer 0, chạy sau phím, chuột, vẽ đang chờ; chỉ chạy khi đang tìm
    } search_;
};

// Delegate mặc định của hai view: như QStyledItemDelegate; ô chỉ có chữ vẽ thẳng (không dựng QTextLayout mỗi ô), editor
// theo .pinp của prototype (cao --h-row − 4, giữa dòng, mép theo lề chữ của ô). Delegate riêng của palette kế thừa lớp
// này để giữ hình editor và đường vẽ nhanh.
class ViewItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const override;
};

} // namespace trion::ui
