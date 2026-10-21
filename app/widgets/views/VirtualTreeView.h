#pragma once
// UI0-108 · Cây ảo hóa: VirtualTableView trên danh sách phẳng các dòng đang hiện (cây đã làm phẳng ở nền), cột 0 thụt
// lề theo mức và có mũi tên đóng/mở (.tree .tn, .tx của prototype). Mọi tính chất của bảng giữ nguyên: dòng cao đều,
// bộ nhớ không theo số dòng, sắp ở nền, lưu bố cục cột, gõ để nhảy, sửa tại chỗ.
//
// Model phẳng, mỗi dòng ở cột 0 trả:
//   DepthRole     int: mức, 0 là gốc.
//   ExpandedRole  bool: nút đang mở (true) / đóng (false); QVariant rỗng: lá.
// Mở/đóng: view gọi model->setData(index cột 0, bool, ExpandedRole); model làm phẳng lại ở nền với tập nút mở mới
// (BackgroundModel::refresh: dòng con chèn/xóa ngay dưới dòng cha, chọn và dòng hiện hành giữ nhờ persistent index).
// Con của một nút là các dòng liền sau nó có mức lớn hơn.
//
// Chuột: nhấp mũi tên mở/đóng (không đổi chọn). Phím: phải mở nút đóng / xuống con đầu; trái đóng nút mở / lên cha.
//
// ponytail: lên cha quét ngược từng dòng (O(số dòng giữa con và cha)); nhóm > 10⁵ con thì model trả thêm dòng cha.
// Chưa có mở/đóng tất cả, đường nối cây, nhấp đúp để mở (palette cần thì thêm).

#include "app/widgets/views/VirtualTableView.h"

namespace trion::ui {

class VirtualTreeView : public VirtualTableView {
    Q_OBJECT
public:
    enum Role {
        DepthRole = Qt::UserRole + 0x7108,
        ExpandedRole,
    };

    explicit VirtualTreeView(QWidget* parent = nullptr);

    bool isExpanded(const QModelIndex& index) const;
    void setExpanded(const QModelIndex& index, bool expanded); // không đổi nếu là lá

    QRect visualRect(const QModelIndex& index) const override;

protected:
    QModelIndex moveCursor(CursorAction action, Qt::KeyboardModifiers modifiers) override;
    void mousePressEvent(QMouseEvent* event) override;
    void drawBranch(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    QRect branchRect(const QModelIndex& index) const; // ô mũi tên trong cột 0, tọa độ viewport
};

} // namespace trion::ui
