#include "app/widgets/views/VirtualTreeView.h"

#include "app/theme/generated/Tokens.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace trion::ui {

namespace {

int px(tok::PxId id)
{
    return qRound(theme::ThemeManager::instance().px(id));
}

// .tree .tn (bản có mũi tên, screens10): lề trái --sp-2, ô mũi tên .tx ≈ 10 px (--sp-5 − --sp-1), cách chữ --sp-2;
// mỗi mức thụt một ô mũi tên + khoảng cách (14 px ≈ 13 px của prototype).
int lead()
{
    return px(tok::sp_2);
}
int arrow()
{
    return px(tok::sp_5) - px(tok::sp_1);
}
int step()
{
    return arrow() + px(tok::sp_2);
}

int levelOf(const QModelIndex& index)
{
    return std::max(0, index.siblingAtColumn(0).data(VirtualTreeView::DepthRole).toInt());
}

} // namespace

VirtualTreeView::VirtualTreeView(QWidget* parent) : VirtualTableView(parent)
{
    rowLines_ = false;
}

bool VirtualTreeView::isExpanded(const QModelIndex& index) const
{
    return index.siblingAtColumn(0).data(ExpandedRole).toBool();
}

void VirtualTreeView::setExpanded(const QModelIndex& index, bool expanded)
{
    const QModelIndex node = index.siblingAtColumn(0);
    const QVariant open = node.data(ExpandedRole);
    if (open.isValid() && open.toBool() != expanded)
        model()->setData(node, expanded, ExpandedRole);
}

QRect VirtualTreeView::branchRect(const QModelIndex& index) const
{
    const QRect cell = cellRect(index.siblingAtColumn(0));
    return {cell.left() + lead() + levelOf(index) * step(), cell.top(), arrow(), cell.height()};
}

// Cột 0: chữ bắt đầu sau ô mũi tên của mức mình (--sp-2 + (mức + 1) · bước từ mép ô); editor, sizeHintForColumn,
// findTruncations theo đúng vùng này.
QRect VirtualTreeView::visualRect(const QModelIndex& index) const
{
    QRect r = VirtualTableView::visualRect(index);
    if (index.column() == 0 && r.width() > 0)
        r.setLeft(std::min(r.left() + lead() + (levelOf(index) + 1) * step() - px(tok::sp_4), r.right() + 1));
    return r;
}

QModelIndex VirtualTreeView::moveCursor(CursorAction action, Qt::KeyboardModifiers modifiers)
{
    const QModelIndex current = currentIndex();
    if (!current.isValid() || (action != MoveLeft && action != MoveRight))
        return VirtualTableView::moveCursor(action, modifiers);
    const QModelIndex node = current.siblingAtColumn(0);
    const QVariant open = node.data(ExpandedRole);
    const int level = levelOf(node);
    if (action == MoveRight) { // mở nút đóng; nút đang mở: xuống con đầu
        if (open.isValid() && !open.toBool())
            setExpanded(node, true);
        else if (open.toBool()) {
            const QModelIndex child = node.siblingAtRow(node.row() + 1);
            if (child.isValid() && levelOf(child) > level)
                return child.siblingAtColumn(current.column());
        }
        return current;
    }
    if (open.toBool()) { // đóng nút mở; không thì lên cha
        setExpanded(node, false);
        return current;
    }
    for (int r = node.row() - 1; r >= 0 && level > 0; --r)
        if (const QModelIndex up = node.siblingAtRow(r); levelOf(up) < level)
            return up.siblingAtColumn(current.column());
    return current;
}

void VirtualTreeView::mousePressEvent(QMouseEvent* event)
{
    const QPoint pos = event->position().toPoint();
    const QModelIndex index = indexAt(pos);
    if (event->button() == Qt::LeftButton && index.isValid() && !isColumnHidden(0) && branchRect(index).contains(pos)) {
        if (const QVariant open = index.siblingAtColumn(0).data(ExpandedRole); open.isValid()) {
            setExpanded(index, !open.toBool()); // nhấp mũi tên: mở/đóng, không đổi chọn
            event->accept();
            return;
        }
    }
    VirtualTableView::mousePressEvent(event);
}

// Mũi tên .tx: "right" khi đóng, "down" khi mở; --c-text-2, rê hoặc chọn --c-text (như chữ của .tree .tn).
void VirtualTreeView::drawBranch(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const QVariant open = index.data(ExpandedRole);
    if (!open.isValid())
        return;
    const QRect box = branchRect(index);
    const auto state = !(option.state & QStyle::State_Enabled)                             ? theme::IconState::Disabled
                       : option.state & (QStyle::State_MouseOver | QStyle::State_Selected) ? theme::IconState::Hover
                                                                                           : theme::IconState::Normal;
    const QPixmap icon = theme::IconProvider::instance().pixmap(open.toBool() ? u"down"_s : u"right"_s, box.width(),
                                                                painter->device()->devicePixelRatio(), state);
    painter->drawPixmap(box.left(), box.top() + (box.height() - box.width()) / 2, icon);
}

} // namespace trion::ui
