#include "app/core/i18n/TruncationProbe.h"

#include <QAbstractItemView>
#include <QAction>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QScreen>
#include <QStyleOptionButton>
#include <QTabBar>
#include <QToolButton>
#include <QTreeView>

#include <algorithm>
#include <functional>

using namespace Qt::StringLiterals;

namespace trion::i18n {

namespace {

// QTabBar::tabSizeHint là protected: gọi qua con trỏ thành viên đặt tên bằng lớp con (hợp lệ, không ép kiểu đối
// tượng sang lớp con).
struct TabBarAccess : QTabBar {
    static QSize hint(const QTabBar* bar, int i) { return (bar->*&TabBarAccess::tabSizeHint)(i); }
};

// Không ai thấy dù isVisible(): cha cắt hết hoặc anh em đục che hết (trang tab nền của nhóm dock: Qt dời ra ngoài cửa
// sổ với cỡ cũ). Vùng cuộn (view, đầu cột) xét viewport: viewport đục phủ khung nên visibleRegion của khung rỗng.
bool unseen(const QWidget* w)
{
    const auto* area = qobject_cast<const QAbstractScrollArea*>(w);
    return (area ? area->viewport() : w)->visibleRegion().isEmpty();
}

void add(QList<Truncation>& out, QWidget* w, const QString& text, QSize need, QSize have)
{
    if (!text.isEmpty() && (need.width() > have.width() || need.height() > have.height()))
        out.append({w, text, need, have});
}

// Không so với sizeHint: style đặt bề rộng tối thiểu (Fusion 80 px) cả khi chữ ngắn. Chữ, biểu tượng và mũi
// tên menu vẽ trong SE_PushButtonContents (QCommonStyle CE_PushButtonLabel; khoảng cách 4 px như sizeHint).
void pushButton(QList<Truncation>& out, QPushButton* b)
{
    QStyleOptionButton opt;
    opt.initFrom(b);
    opt.text = b->text();
    opt.icon = b->icon();
    opt.iconSize = b->iconSize();
    if (b->isFlat())
        opt.features |= QStyleOptionButton::Flat;
    if (b->autoDefault())
        opt.features |= QStyleOptionButton::AutoDefaultButton;
    if (b->isDefault())
        opt.features |= QStyleOptionButton::DefaultButton;
    if (b->menu())
        opt.features |= QStyleOptionButton::HasMenu;
    QSize need = b->fontMetrics().size(Qt::TextShowMnemonic, b->text());
    if (!b->icon().isNull())
        need.rwidth() += b->iconSize().width() + 4;
    if (b->menu())
        need.rwidth() += b->style()->pixelMetric(QStyle::PM_MenuButtonIndicator, &opt, b);
    add(out, b, b->text(), need, b->style()->subElementRect(QStyle::SE_PushButtonContents, &opt, b).size());
}

void otherButton(QList<Truncation>& out, QAbstractButton* b)
{
    if (auto* tb = qobject_cast<QToolButton*>(b); tb && !tb->icon().isNull()) {
        const Qt::ToolButtonStyle st = tb->toolButtonStyle() == Qt::ToolButtonFollowStyle
            ? Qt::ToolButtonStyle(tb->style()->styleHint(QStyle::SH_ToolButtonStyle, nullptr, tb))
            : tb->toolButtonStyle();
        if (st == Qt::ToolButtonIconOnly)
            return; // chữ không vẽ (chỉ làm tooltip/tên cho trình đọc màn hình)
    }
    // QCheckBox, QRadioButton, QToolButton: style không đặt bề rộng tối thiểu, so thẳng với sizeHint
    add(out, b, b->text(), b->sizeHint(), b->size());
}

// Tab hẹp hơn tabSizeHint (bị nén, rút gọn "…"), hoặc tràn ra ngoài thanh khi không có nút cuộn.
void tabs(QList<Truncation>& out, QTabBar* bar)
{
    for (int i = 0; i < bar->count(); ++i) {
        const QRect r = bar->tabRect(i);
        if (bar->isTabVisible(i))
            add(out, bar, bar->tabText(i), TabBarAccess::hint(bar, i),
                bar->usesScrollButtons() ? r.size() : r.intersected(bar->rect()).size());
    }
}

void header(QList<Truncation>& out, QHeaderView* h)
{
    const bool horizontal = h->orientation() == Qt::Horizontal;
    const int extent = horizontal ? h->viewport()->width() : h->viewport()->height();
    for (int i = 0; i < h->count(); ++i) {
        const int pos = h->sectionViewportPosition(i), have = h->sectionSize(i);
        if (h->isSectionHidden(i) || pos + have <= 0 || pos >= extent)
            continue; // ẩn hoặc ngoài vùng nhìn: không đo (bảng nhiều dòng)
        const int need = h->sectionSizeHint(i);
        add(out, h, h->model()->headerData(i, h->orientation()).toString(),
            horizontal ? QSize(need, 0) : QSize(0, need), horizontal ? QSize(have, 0) : QSize(0, have));
    }
}

// Ô trong vùng nhìn, theo thứ tự hiện (cây: vào nhánh đang mở); dừng ở dòng đầu tiên nằm dưới vùng nhìn.
// ponytail: bảng có dòng bị kéo đổi thứ tự (QHeaderView::moveSection) có thể dừng sớm.
void cells(QList<Truncation>& out, QAbstractItemView* v)
{
    const QAbstractItemModel* m = v->model();
    if (!m)
        return;
    const auto* tree = qobject_cast<QTreeView*>(v);
    const QRect port = v->viewport()->rect();
    bool below = false;
    const std::function<void(const QModelIndex&)> walk = [&](const QModelIndex& parent) {
        for (int r = 0; r < m->rowCount(parent) && !below; ++r) {
            for (int c = 0; c < m->columnCount(parent); ++c) {
                const QModelIndex i = m->index(r, c, parent);
                const QRect box = v->visualRect(i);
                if (c == 0 && box.isValid() && box.top() > port.bottom()) {
                    below = true;
                    return;
                }
                if (box.isValid() && box.intersects(port))
                    add(out, v, i.data().toString(), v->sizeHintForIndex(i), box.size());
            }
            if (const QModelIndex first = m->index(r, 0, parent); tree && tree->isExpanded(first))
                walk(first);
        }
    };
    walk(v->rootIndex());
}

void menu(QList<Truncation>& out, QMenu* m)
{
    const QSize need = m->sizeHint();
    const int room =
        m->isVisible() ? m->width() : std::min(m->maximumWidth(), m->screen()->availableGeometry().width());
    if (need.width() <= room)
        return;
    const QAction* longest = nullptr;
    int best = -1;
    for (const QAction* a : m->actions()) {
        const int w = m->fontMetrics().horizontalAdvance(a->text());
        if (a->isVisible() && !a->isSeparator() && w > best) {
            best = w;
            longest = a;
        }
    }
    if (longest)
        add(out, m, longest->text(), need, QSize(room, need.height()));
}

} // namespace

QString Truncation::toString() const
{
    const auto size = [](QSize s) { return u"%1×%2"_s.arg(s.width()).arg(s.height()); };
    return u"%1 '%2': «%3» cần %4, có %5"_s.arg(QString::fromLatin1(widget->metaObject()->className()),
                                                 widget->objectName(), text, size(need), size(have));
}

QList<Truncation> findTruncations(QWidget* root)
{
    QList<Truncation> out;
    QList<QWidget*> all = root->findChildren<QWidget*>();
    all.prepend(root);
    for (QWidget* w : std::as_const(all)) {
        if (auto* m = qobject_cast<QMenu*>(w))
            menu(out, m); // menu đóng vẫn đo: sẽ mở ra với đúng cỡ này
        else if (!w->isVisible() || unseen(w))
            continue;
        else if (auto* l = qobject_cast<QLabel*>(w))
            add(out, l, l->text(), l->wordWrap() ? QSize(l->width(), l->heightForWidth(l->width())) : l->sizeHint(),
                l->size());
        else if (auto* b = qobject_cast<QPushButton*>(w))
            pushButton(out, b);
        else if (auto* b = qobject_cast<QAbstractButton*>(w))
            otherButton(out, b);
        else if (auto* bar = qobject_cast<QTabBar*>(w))
            tabs(out, bar);
        else if (auto* h = qobject_cast<QHeaderView*>(w))
            header(out, h);
        else if (auto* v = qobject_cast<QAbstractItemView*>(w))
            cells(out, v);
    }
    return out;
}

} // namespace trion::i18n
