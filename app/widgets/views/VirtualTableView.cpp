#include "app/widgets/views/VirtualTableView.h"

#include "app/theme/generated/Tokens.h"
#include "app/theme/ThemeManager.h"
#include "core/base/settings/Settings.h"
#include "core/base/text/Fold.h"

#include <QApplication>
#include <QHeaderView>
#include <QHoverEvent>
#include <QJsonValue>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTimerEvent>

#include <algorithm>
#include <chrono>

using namespace Qt::StringLiterals;

namespace trion::ui {

namespace {

using theme::ThemeManager;

int px(tok::PxId id)
{
    return qRound(ThemeManager::instance().px(id));
}

// Lề chữ style tự chừa mỗi bên ô (QCommonStyle: PM_FocusFrameHMargin + 1).
int textMargin(const QWidget* w)
{
    return w->style()->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, w) + 1;
}

QString storeKey(const QString& key)
{
    return u"views/"_s + key;
}

// Gõ để nhảy đọc model trên luồng UI (model của view chỉ đọc được ở đó): quét theo lát ≤ 2 ms (DoD-U 4), giữa hai lát
// nhường vòng sự kiện.
constexpr auto kSearchSlice = std::chrono::milliseconds(2);

} // namespace

VirtualTableView::VirtualTableView(QWidget* parent)
    : QAbstractItemView(parent), header_(new QHeaderView(Qt::Horizontal, this))
{
    setSelectionBehavior(SelectRows);
    setSelectionMode(ExtendedSelection);
    setVerticalScrollMode(ScrollPerItem); // giá trị thanh cuộn dọc = dòng trên cùng
    setHorizontalScrollMode(ScrollPerPixel);
    setItemDelegate(new ViewItemDelegate(this));
    viewport()->setAttribute(Qt::WA_Hover);
    header_->setStretchLastSection(true);
    header_->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter); // .tbl th; cột số: model trả TextAlignmentRole
    header_->setSectionsClickable(false);
    header_->setSortIndicator(-1, Qt::AscendingOrder);
    header_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header_, &QHeaderView::sectionResized, this, [this] {
        updateScrollBars();
        viewport()->update();
        saveColumns();
    });
    connect(header_, &QHeaderView::sectionMoved, this, [this] {
        viewport()->update();
        saveColumns();
    });
    connect(header_, &QHeaderView::sortIndicatorChanged, this, [this] {
        applySort();
        saveColumns();
    });
    connect(header_, &QHeaderView::sectionHandleDoubleClicked, this,
            [this](int column) { header_->resizeSection(column, sizeHintForColumn(column)); });
    connect(header_, &QHeaderView::geometriesChanged, this, &VirtualTableView::updateGeometries);
    connect(header_, &QHeaderView::customContextMenuRequested, this, &VirtualTableView::columnMenu);
    // .tbl: --fs-s cho ô, đầu cột (con thừa hưởng) và editor; đổi khi chủ đề/mật độ đổi.
    auto& t = ThemeManager::instance();
    setFont(t.font(tok::fs_s));
    connect(&t, &ThemeManager::changed, this, [this] { setFont(ThemeManager::instance().font(tok::fs_s)); });
}

// Đầu cột (con) bị hủy sau phần lớp con này: nó còn phát tín hiệu thì không được gọi vào đây nữa.
VirtualTableView::~VirtualTableView()
{
    header_->disconnect(this);
}

void VirtualTableView::setModel(QAbstractItemModel* model)
{
    if (model == this->model())
        return;
    disconnect(removed_);
    disconnect(reset_);
    QAbstractItemView::setModel(model);
    {
        const QSignalBlocker block(header_); // tạo cột không ghi đè bố cục đã lưu
        header_->setModel(model);
    }
    hoverRow_ = -1;
    if (model) {
        // Số dòng đổi: dải thanh cuộn cập nhật ngay (O(1)), để BackgroundModel đặt lại vị trí cuộn trong cùng lô.
        const auto rows = [this] {
            updateScrollBars();
            viewport()->update();
        };
        removed_ = connect(model, &QAbstractItemModel::rowsRemoved, this, rows);
        reset_ = connect(model, &QAbstractItemModel::modelReset, this, rows);
    }
    restoreColumns();
    if (header_->sortIndicatorSection() >= 0)
        applySort();
}

void VirtualTableView::setStateKey(const QString& key)
{
    stateKey_ = key;
    if (restoreColumns())
        applySort();
}

void VirtualTableView::setSortingEnabled(bool on)
{
    sorting_ = on;
    header_->setSortIndicatorShown(on);
    header_->setSectionsClickable(on);
    if (header_->sortIndicatorSection() >= 0)
        applySort();
}

void VirtualTableView::applySort()
{
    if (sorting_ && model())
        model()->sort(header_->sortIndicatorSection(), header_->sortIndicatorOrder());
}

void VirtualTableView::setColumnHidden(int column, bool hide)
{
    header_->setSectionHidden(column, hide);
    updateGeometries();
    viewport()->update();
    saveColumns();
}

bool VirtualTableView::isColumnHidden(int column) const
{
    return header_->isSectionHidden(column);
}

void VirtualTableView::saveColumns()
{
    if (!stateKey_.isEmpty() && header_->model())
        settings::Store::instance().setValue(storeKey(stateKey_), QString::fromLatin1(header_->saveState().toBase64()));
}

bool VirtualTableView::restoreColumns()
{
    if (stateKey_.isEmpty() || !header_->model())
        return false;
    const QJsonValue saved = settings::Store::instance().value(storeKey(stateKey_));
    if (!saved.isString())
        return false;
    const int section = header_->sortIndicatorSection();
    const Qt::SortOrder order = header_->sortIndicatorOrder();
    {
        const QSignalBlocker block(header_); // không ghi lại, không sắp giữa chừng
        header_->restoreState(QByteArray::fromBase64(saved.toString().toLatin1()));
    }
    updateGeometries();
    viewport()->update();
    return header_->sortIndicatorSection() != section || header_->sortIndicatorOrder() != order;
}

// Menu chuột phải của đầu cột: ẩn/hiện từng cột, luôn còn ít nhất một cột. popup() không lồng vòng sự kiện (view có
// thể bị hủy khi menu đang mở).
void VirtualTableView::columnMenu(const QPoint& pos)
{
    const QAbstractItemModel* m = model();
    if (!m)
        return;
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    const int shown = header_->count() - header_->hiddenSectionCount();
    for (int v = 0; v < header_->count(); ++v) {
        const int column = header_->logicalIndex(v);
        QString name = m->headerData(column, Qt::Horizontal).toString();
        if (name.isEmpty()) // cột chỉ có biểu tượng (.tbl th[title])
            name = m->headerData(column, Qt::Horizontal, Qt::ToolTipRole).toString();
        QAction* a = menu->addAction(name);
        a->setCheckable(true);
        a->setChecked(!header_->isSectionHidden(column));
        a->setEnabled(!a->isChecked() || shown > 1);
        connect(a, &QAction::toggled, this, [this, column](bool on) { setColumnHidden(column, !on); });
    }
    menu->popup(header_->viewport()->mapToGlobal(pos));
}

int VirtualTableView::rowHeight() const
{
    return std::max(1, px(tok::h_row));
}

int VirtualTableView::rowAt(int y) const
{
    const int h = rowHeight();
    return verticalScrollBar()->value() + (y >= 0 ? y / h : (y - h + 1) / h);
}

QRect VirtualTableView::cellRect(const QModelIndex& index) const
{
    const int column = index.column();
    const int h = rowHeight();
    return {header_->sectionViewportPosition(column), (index.row() - verticalScrollBar()->value()) * h,
            header_->isSectionHidden(column) ? 0 : header_->sectionSize(column), h};
}

// Vùng delegate vẽ: ô trừ lề sao cho chữ cách mép ô --sp-4 (.tbl td: padding 0 8px). Cột ẩn: rộng 0 (không hợp lệ)
// nhưng đúng dòng, để chọn bằng Shift từ ô đó vẫn đúng.
QRect VirtualTableView::visualRect(const QModelIndex& index) const
{
    if (!index.isValid() || index.model() != model() || index.parent() != rootIndex())
        return {};
    const QRect cell = cellRect(index);
    if (cell.width() <= 0)
        return cell;
    const int pad = std::max(0, px(tok::sp_4) - textMargin(this));
    return cell.adjusted(pad, 0, -pad, 0);
}

QModelIndex VirtualTableView::indexAt(const QPoint& point) const
{
    const QAbstractItemModel* m = model();
    const int row = rowAt(point.y());
    const int column = header_->logicalIndexAt(point.x());
    if (!m || row < 0 || row >= m->rowCount(rootIndex()) || column < 0)
        return {};
    return m->index(row, column, rootIndex());
}

void VirtualTableView::scrollTo(const QModelIndex& index, ScrollHint hint)
{
    if (!index.isValid() || index.model() != model())
        return;
    QScrollBar* bar = verticalScrollBar();
    const int page = std::max(1, viewport()->height() / rowHeight());
    const int row = index.row();
    switch (hint) {
    case EnsureVisible:
        if (row < bar->value())
            bar->setValue(row);
        else if (row >= bar->value() + page)
            bar->setValue(row - page + 1);
        break;
    case PositionAtTop:
        bar->setValue(row);
        break;
    case PositionAtBottom:
        bar->setValue(row - page + 1);
        break;
    case PositionAtCenter:
        bar->setValue(row - (page - 1) / 2);
        break;
    }
    if (header_->isSectionHidden(index.column()))
        return;
    QScrollBar* across = horizontalScrollBar(); // cột của index hiện trọn nếu vừa
    const int x = header_->sectionPosition(index.column());
    const int w = header_->sectionSize(index.column());
    if (x < across->value())
        across->setValue(x);
    else if (x + w > across->value() + viewport()->width())
        across->setValue(std::min(x, x + w - viewport()->width()));
}

QModelIndex VirtualTableView::moveCursor(CursorAction action, Qt::KeyboardModifiers)
{
    const QAbstractItemModel* m = model();
    const int rows = m ? m->rowCount(rootIndex()) : 0;
    if (rows == 0 || header_->hiddenSectionCount() == header_->count())
        return {};
    const QModelIndex current = currentIndex();
    const auto shownFrom = [this](int visual, int step) { // cột hiện kế tiếp theo thứ tự nhìn thấy, -1 nếu hết
        for (; visual >= 0 && visual < header_->count(); visual += step)
            if (!header_->isSectionHidden(header_->logicalIndex(visual)))
                return visual;
        return -1;
    };
    int visual =
        current.isValid() && !isIndexHidden(current) ? header_->visualIndex(current.column()) : shownFrom(0, 1);
    if (!current.isValid())
        return m->index(0, header_->logicalIndex(visual), rootIndex());
    int row = current.row();
    const int page = std::max(1, viewport()->height() / rowHeight());
    switch (action) {
    case MoveUp:
    case MovePrevious:
        --row;
        break;
    case MoveDown:
    case MoveNext:
        ++row;
        break;
    case MovePageUp:
        row -= page;
        break;
    case MovePageDown:
        row += page;
        break;
    case MoveHome:
        row = 0;
        break;
    case MoveEnd:
        row = rows - 1;
        break;
    case MoveLeft:
    case MoveRight: {
        const int next = shownFrom(visual + (action == MoveLeft ? -1 : 1), action == MoveLeft ? -1 : 1);
        visual = next < 0 ? visual : next;
        break;
    }
    }
    return m->index(std::clamp(row, 0, rows - 1), header_->logicalIndex(visual), rootIndex());
}

int VirtualTableView::horizontalOffset() const
{
    return horizontalScrollBar()->value();
}

int VirtualTableView::verticalOffset() const
{
    return verticalScrollBar()->value() * rowHeight();
}

bool VirtualTableView::isIndexHidden(const QModelIndex& index) const
{
    return header_->isSectionHidden(index.column());
}

// rect theo tọa độ viewport, có thể vượt vùng nhìn (Shift từ dòng đã cuộn khuất): chọn trọn các dòng nó phủ.
void VirtualTableView::setSelection(const QRect& rect, QItemSelectionModel::SelectionFlags command)
{
    const QAbstractItemModel* m = model();
    if (!m || !selectionModel())
        return;
    const QRect r = rect.normalized();
    const int rows = m->rowCount(rootIndex()), columns = m->columnCount(rootIndex());
    const int first = std::max(rowAt(r.top()), 0), last = std::min(rowAt(r.bottom()), rows - 1);
    QItemSelection selection;
    if (first <= last && columns > 0)
        selection.select(m->index(first, 0, rootIndex()), m->index(last, columns - 1, rootIndex()));
    selectionModel()->select(selection, command); // rỗng vẫn áp Clear của command
}

QRegion VirtualTableView::visualRegionForSelection(const QItemSelection& selection) const
{
    QRegion region;
    const int h = rowHeight(), top = verticalScrollBar()->value();
    const int bottom = top + viewport()->height() / h + 1;
    for (const QItemSelectionRange& range : selection) {
        const int a = std::max(range.top(), top), b = std::min(range.bottom(), bottom);
        if (range.parent() == rootIndex() && a <= b)
            region += QRect(0, (a - top) * h, viewport()->width(), (b - a + 1) * h);
    }
    return region;
}

// F2 trên ô không sửa được (cột trạng thái, biểu tượng): sửa ô sửa được đầu tiên của dòng (đổi tên layer).
bool VirtualTableView::edit(const QModelIndex& index, EditTrigger trigger, QEvent* event)
{
    if (trigger == EditKeyPressed && index.isValid() && !(index.flags() & Qt::ItemIsEditable)) {
        for (int v = 0; v < header_->count(); ++v) {
            const QModelIndex cell = index.siblingAtColumn(header_->logicalIndex(v));
            if (!header_->isSectionHidden(cell.column()) && (cell.flags() & Qt::ItemIsEditable))
                return QAbstractItemView::edit(cell, trigger, event);
        }
    }
    return QAbstractItemView::edit(index, trigger, event);
}

void VirtualTableView::updateGeometries()
{
    const int head = header_->isHidden() ? 0 : header_->sizeHint().height();
    setViewportMargins(0, head, 0, 0);
    const QRect port = viewport()->geometry();
    header_->setGeometry(port.left(), port.top() - head, port.width(), head); // dính trên vùng nhìn
    updateScrollBars();
    QAbstractItemView::updateGeometries();
}

void VirtualTableView::updateScrollBars()
{
    const int rows = model() ? model()->rowCount(rootIndex()) : 0;
    const int page = std::max(1, viewport()->height() / rowHeight());
    QScrollBar* down = verticalScrollBar();
    down->setSingleStep(1);
    down->setPageStep(page);
    down->setRange(0, std::max(0, rows - page));
    QScrollBar* across = horizontalScrollBar();
    across->setSingleStep(rowHeight());
    across->setPageStep(viewport()->width());
    across->setRange(0, std::max(0, header_->length() - viewport()->width()));
}

void VirtualTableView::rowsInserted(const QModelIndex& parent, int start, int end)
{
    QAbstractItemView::rowsInserted(parent, start, end);
    updateScrollBars();
    viewport()->update();
}

// Khung focus bao cả dòng: vẽ lại hai dòng (QAbstractItemView chỉ vẽ lại hai ô).
void VirtualTableView::currentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    updateRow(previous.row());
    updateRow(current.row());
    QAbstractItemView::currentChanged(current, previous);
}

void VirtualTableView::focusInEvent(QFocusEvent* event)
{
    QAbstractItemView::focusInEvent(event);
    updateRow(currentIndex().row());
}

void VirtualTableView::focusOutEvent(QFocusEvent* event)
{
    QAbstractItemView::focusOutEvent(event);
    updateRow(currentIndex().row());
}

void VirtualTableView::scrollContentsBy(int dx, int dy)
{
    if (dx != 0)
        header_->setOffset(horizontalScrollBar()->value());
    viewport()->scroll(dx, dy * rowHeight()); // dy tính bằng dòng; editor (con của viewport) đi theo
    if (viewport()->underMouse())             // nội dung trượt dưới chuột đứng yên
        setHoverRow(rowAt(viewport()->mapFromGlobal(QCursor::pos()).y()));
}

void VirtualTableView::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == search_.slice.timerId())
        continueSearch();
    else
        QAbstractItemView::timerEvent(event);
}

void VirtualTableView::updateRow(int row)
{
    if (row >= 0)
        viewport()->update(
            QRect(0, (row - verticalScrollBar()->value()) * rowHeight(), viewport()->width(), rowHeight()));
}

void VirtualTableView::setHoverRow(int row)
{
    if (!model() || row < 0 || row >= model()->rowCount(rootIndex()))
        row = -1;
    if (row == hoverRow_)
        return;
    updateRow(hoverRow_);
    hoverRow_ = row;
    updateRow(row);
}

bool VirtualTableView::viewportEvent(QEvent* event)
{
    switch (event->type()) {
    case QEvent::HoverEnter:
    case QEvent::HoverMove:
        setHoverRow(rowAt(static_cast<QHoverEvent*>(event)->position().toPoint().y()));
        break;
    case QEvent::HoverLeave:
        setHoverRow(-1);
        break;
    default:
        break;
    }
    return QAbstractItemView::viewportEvent(event);
}

// Chỉ các dòng giao vùng cần vẽ: mỗi khung O(số dòng nhìn thấy × số cột), không phụ thuộc số dòng của model.
void VirtualTableView::paintEvent(QPaintEvent* event)
{
    const QAbstractItemModel* m = model();
    if (!m)
        return;
    const auto& t = ThemeManager::instance();
    const QRect area = event->rect();
    const int h = rowHeight(), top = verticalScrollBar()->value(), width = viewport()->width();
    const int first = std::max(rowAt(area.top()), 0);
    const int last = std::min(rowAt(area.bottom()), m->rowCount(rootIndex()) - 1);
    const QItemSelectionModel* selection = selectionModel();
    QStyleOptionViewItem base;
    initViewItemOption(&base);
    base.palette.setBrush(QPalette::Highlight, Qt::transparent); // nền dòng chọn do view vẽ trọn dòng
    QPainter p(viewport());
    for (int r = first; r <= last; ++r) {
        const QRect line(0, (r - top) * h, width, h);
        const QModelIndex head = m->index(r, 0, rootIndex());
        QStyleOptionViewItem row = base;
        if (selection && selection->isSelected(head)) { // .tbl tr.is-sel td (cả khi rê)
            row.state |= QStyle::State_Selected;
            p.fillRect(line, t.color(tok::c_accent_soft));
        } else if (r == hoverRow_) { // .tbl tbody tr:hover td
            p.fillRect(line, t.color(tok::c_hover));
        }
        if (r == hoverRow_)
            row.state |= QStyle::State_MouseOver;
        for (int v = 0; v < header_->count(); ++v) {
            const int column = header_->logicalIndex(v);
            const int x = header_->sectionViewportPosition(column);
            if (header_->isSectionHidden(column) || x > area.right() || x + header_->sectionSize(column) <= area.left())
                continue;
            const QModelIndex index = column == 0 ? head : m->index(r, column, rootIndex());
            QStyleOptionViewItem cell = row;
            cell.rect = visualRect(index);
            if (!(index.flags() & Qt::ItemIsEnabled))
                cell.state &= ~QStyle::State_Enabled;
            itemDelegateForIndex(index)->paint(&p, cell, index);
            if (column == 0)
                drawBranch(&p, cell, index);
        }
        if (rowLines_) // .tbl td: border-bottom 1px
            p.fillRect(QRect(0, line.bottom(), width, 1), t.color(tok::c_border));
    }
    const QModelIndex current = currentIndex();
    if (hasFocus() && current.isValid() && current.row() >= first && current.row() <= last) {
        QStyleOptionFocusRect focus; // :focus-visible: TrionStyle chỉ vẽ khi focus đến từ bàn phím
        focus.initFrom(this);
        focus.rect = QRect(0, (current.row() - top) * h, width, h);
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, &p, this);
    }
}

void VirtualTableView::drawBranch(QPainter*, const QStyleOptionViewItem&, const QModelIndex&) const {}

// Chỉ đo các dòng đang nhìn (QAbstractItemView đo mọi dòng): ResizeToContents, nhấp đúp mép đầu cột.
int VirtualTableView::sizeHintForColumn(int column) const
{
    const QAbstractItemModel* m = model();
    if (!m || header_->isSectionHidden(column))
        return -1;
    const int top = verticalScrollBar()->value();
    const int last = std::min(m->rowCount(rootIndex()) - 1, top + viewport()->height() / rowHeight());
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    int width = -1;
    for (int r = top; r <= last; ++r) {
        const QModelIndex index = m->index(r, column, rootIndex());
        const int pad = cellRect(index).width() - visualRect(index).width(); // lề, thụt lề của cây
        width = std::max(width, itemDelegateForIndex(index)->sizeHint(option, index).width() + pad);
    }
    return width;
}

void VirtualTableView::keyboardSearch(const QString& search)
{
    const QAbstractItemModel* m = model();
    const int rows = m ? m->rowCount(rootIndex()) : 0;
    if (search.isEmpty() || rows == 0)
        return;
    Search& s = search_;
    const bool fresh = !s.typed.isValid() || s.typed.elapsed() > QApplication::keyboardInputInterval();
    s.typed.start();
    s.text = fresh ? search : s.text + search;
    // Cùng một phím lặp lại ("lll"): đi qua các dòng bắt đầu bằng chữ đó.
    const bool repeat = s.text.size() > 1 && s.text.count(s.text.back()) == s.text.size();
    s.needle = text::foldForSearch(repeat ? QString(s.text.back()) : s.text);
    // Tìm mới, lặp phím: từ dòng sau dòng hiện hành; gõ nối: từ chính dòng hiện hành. Lần quét đang dở (nếu có) thôi.
    const QModelIndex current = currentIndex();
    s.next = current.isValid() ? (current.row() + (fresh || repeat ? 1 : 0)) % rows : 0;
    s.left = rows;
    continueSearch();
}

void VirtualTableView::continueSearch()
{
    const QAbstractItemModel* m = model();
    Search& s = search_;
    const int rows = m ? m->rowCount(rootIndex()) : 0;
    s.slice.stop();
    QElapsedTimer slice;
    slice.start();
    while (s.left > 0 && rows > 0) {
        const int row = s.next < rows ? s.next : 0;
        s.next = (row + 1) % rows;
        --s.left;
        const QModelIndex hit = m->index(row, searchColumn_, rootIndex());
        if (text::startsWithFolded(hit.data().toString(), s.needle)) {
            s.left = 0;
            const QModelIndex current = currentIndex();
            setCurrentIndex(current.isValid() ? hit.siblingAtColumn(current.column()) : hit);
            return;
        }
        if (s.left > 0 && (s.left & 63) == 0 && slice.durationElapsed() >= kSearchSlice) {
            // Timer 0 thật (không phải QTimer::singleShot(0): Qt 6 đổi thành sự kiện gửi sau, chuỗi lát chạy liền một
            // lượt vòng sự kiện, phím và vẽ không chen vào được).
            // ponytail: vị trí tính theo số dòng; model đổi giữa hai lát thì có thể bỏ sót vài dòng của lần tìm này.
            s.slice.start(0, this);
            return;
        }
    }
}

// Ô chỉ có chữ, một dòng (đa số ô của danh sách lớn): vẽ như CE_ItemViewItem của QCommonStyle (nền BackgroundRole, lề
// chữ, màu Text/HighlightedText theo nhóm màu, căn theo TextAlignmentRole, rút gọn theo textElideMode) nhưng không dựng
// QTextLayout cho mỗi ô: vẽ lại cả vùng nhanh ~1,6–1,8 lần (views_bench repaintCost). Ô khác: QStyledItemDelegate.
void ViewItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem o = option;
    initStyleOption(&o, index);
    constexpr auto kSlow =
        QStyleOptionViewItem::WrapText | QStyleOptionViewItem::HasCheckIndicator | QStyleOptionViewItem::HasDecoration;
    if ((o.features & kSlow) || o.text.contains(QChar::LineSeparator)) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    if (o.backgroundBrush.style() != Qt::NoBrush)
        painter->fillRect(o.rect, o.backgroundBrush);
    QPalette::ColorGroup group = o.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
    if (group == QPalette::Normal && !(o.state & QStyle::State_Active))
        group = QPalette::Inactive;
    const int margin = o.widget ? textMargin(o.widget) : 0;
    const QRect box = o.rect.adjusted(margin, 0, -margin, 0);
    const QFontMetricsF fm(o.font); // đặt dòng bằng số thực như QTextLayout của QCommonStyle (lệch ≤ 1 điểm ảnh)
    QString text = o.text;
    qreal width = fm.horizontalAdvance(text);
    if (width > box.width()) { // rút gọn chỉ khi không vừa: mỗi lần đo là một lần shaping
        text = fm.elidedText(text, o.textElideMode, box.width());
        width = fm.horizontalAdvance(text);
    }
    const QRect line =
        QStyle::alignedRect(o.direction, o.displayAlignment, QSizeF(width, fm.ascent() + fm.descent()).toSize(), box);
    painter->save();
    painter->setFont(o.font);
    painter->setPen(
        o.palette.color(group, o.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Text));
    painter->setClipRect(o.rect);
    painter->drawText(QPointF(line.left(), line.top() + fm.ascent()), text);
    painter->restore();
}

// .pinp có viền (--c-focus khi đang sửa): QStyledItemDelegate tạo QLineEdit không viền theo style (Fusion).
QWidget* ViewItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                        const QModelIndex& index) const
{
    QWidget* editor = QStyledItemDelegate::createEditor(parent, option, index);
    if (auto* line = qobject_cast<QLineEdit*>(editor))
        line->setFrame(true);
    return editor;
}

// .pinp: mép theo lề chữ của ô (--sp-4 từ mép ô), cao --h-row − 2·--sp-1 giữa dòng; không thấp hơn chữ cần (viền 1 px
// và lề dọc 1 px của QLineEdit mỗi phía), không cao hơn dòng.
void ViewItemDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                                            const QModelIndex& index) const
{
    QStyledItemDelegate::updateEditorGeometry(editor, option, index);
    const int margin = option.widget ? textMargin(option.widget) : 0;
    const int row = option.rect.height();
    const int h = std::min(row, std::max(row - 2 * px(tok::sp_1), editor->fontMetrics().height() + 4));
    const QRect r = editor->geometry();
    editor->setGeometry(r.left() + margin, option.rect.top() + (row - h) / 2, std::max(0, r.width() - 2 * margin), h);
}

} // namespace trion::ui
