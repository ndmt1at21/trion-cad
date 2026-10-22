#include "app/core/a11y/AccessibilityAudit.h"

#include "app/theme/ThemeManager.h"

#include <QAccessible>
#include <QApplication>
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMetaEnum>
#include <QPixmap>
#include <QPointer>
#include <QScrollArea>
#include <QSet>
#include <QWidget>

#include <algorithm>
#include <cstdlib>

using namespace Qt::StringLiterals;

namespace trion::a11y {

namespace {

QString describe(const QObject* o)
{
    return u"%1 '%2'"_s.arg(QLatin1StringView(o->metaObject()->className()), o->objectName());
}

// Con của bảng, cây, danh sách là ô (QAccessibleTableCell…), không phải widget: không đi vào.
bool hasCells(QAccessible::Role r)
{
    return r == QAccessible::Table || r == QAccessible::Tree || r == QAccessible::List;
}

// Tên giống nhau trên mọi nền tảng. QAccessibleComboBox trên Unix (Linux, macOS) trả mục đang chọn làm tên (nhãn đi
// qua quan hệ Label), trên Windows trả accessibleName: lấy accessibleName, không có thì nhãn buddy (quan hệ Label).
QString nameOf(QAccessibleInterface* iface)
{
    if (iface->role() != QAccessible::ComboBox)
        return iface->text(QAccessible::Name);
    if (const auto* w = qobject_cast<const QWidget*>(iface->object()); w && !w->accessibleName().isEmpty())
        return w->accessibleName();
    const auto labels = iface->relations(QAccessible::Label);
    return labels.isEmpty() ? QString() : labels.first().first->text(QAccessible::Name);
}

// Duyệt các nút đang hiện (cha trước con).
template <class F>
void visit(QAccessibleInterface* iface, const F& f)
{
    if (!iface || !iface->isValid() || iface->state().invisible)
        return;
    f(iface);
    if (hasCells(iface->role()))
        return;
    for (int i = 0, n = iface->childCount(); i < n; ++i)
        visit(iface->child(i), f);
}

// Điểm dừng Tab theo luật của QWidget::focusNextPrevChild: nhận Tab, hiện, bật, cùng cửa sổ, không chuyển focus cho
// widget khác (focusProxy).
bool tabStop(const QWidget* w, const QWidget* window)
{
    return w->window() == window && w->isVisible() && w->isEnabled() &&
           (w->focusPolicy() & Qt::TabFocus) == Qt::TabFocus && !w->focusProxy();
}

// Vùng quanh w (±3 px) chụp từ cửa sổ: gồm cả viền mà cha vẽ quanh w (ô số :focus-within) và lớp phủ (QFocusFrame
// của ô nhiều dòng). Cuộn w vào vùng nhìn trước (ngoài vùng nhìn thì cửa sổ không vẽ ra).
QImage around(QWidget* w)
{
    for (QWidget* p = w->parentWidget(); p && !p->isWindow(); p = p->parentWidget())
        if (auto* area = qobject_cast<QScrollArea*>(p))
            area->ensureWidgetVisible(w, 3, 3);
    QWidget* window = w->window();
    return window->grab(QRect(w->mapTo(window, QPoint()), w->size()).adjusted(-3, -3, 3, 3)).toImage();
}

bool near(const QColor& a, const QColor& b) // ±24 mỗi kênh: khử răng cưa, trộn alpha
{
    return std::abs(a.red() - b.red()) <= 24 && std::abs(a.green() - b.green()) <= 24 &&
           std::abs(a.blue() - b.blue()) <= 24;
}

// Điểm ảnh đổi màu khi focus mà trước hoặc sau là màu --c-focus: viền focus hiện ra (trên nền cùng màu focus như nút
// chính thì nét trong làm viền thấy được, điểm ảnh rời màu focus cũng tính).
// ponytail: không biết viền có tương phản với nền ngay dưới không (viền --c-focus trên nền --c-accent); phần đó kiểm
// theo token ở app/theme/contrast-pairs.txt.
int ringPixels(const QImage& before, const QImage& after, const QColor& focus)
{
    int n = 0;
    for (int y = 0, h = std::min(before.height(), after.height()); y < h; ++y)
        for (int x = 0, w = std::min(before.width(), after.width()); x < w; ++x) {
            const QColor a = before.pixelColor(x, y), b = after.pixelColor(x, y);
            n += !near(a, b) && (near(a, focus) || near(b, focus));
        }
    return n;
}

// Tab như người dùng: qua QApplication::notify (ShortcutOverride trước; widget không nhận thì Tab lên cha), có chữ
// "\t" như phím thật nên ô nhiều dòng giữ Tab thì chèn ký tự tab (bẫy). Focus đổi do Tab bật WA_KeyboardFocusChange.
void pressTab(QWidget* w)
{
    QKeyEvent press(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, u"\t"_s);
    QCoreApplication::sendEvent(w, &press);
}

QJsonObject node(QAccessibleInterface* iface)
{
    static const QMetaEnum roles = QMetaEnum::fromType<QAccessible::Role>();
    QJsonObject o;
    const QAccessible::Role role = iface->role();
    const char* key = roles.valueToKey(role);
    o[u"role"_s] = key ? QString::fromLatin1(key) : QString::number(int(role));
    if (const QString s = nameOf(iface); !s.isEmpty())
        o[u"name"_s] = s;
    if (const QString s = iface->text(QAccessible::Description); !s.isEmpty())
        o[u"description"_s] = s;
    // Cờ không đổi theo lúc chạy (không focused, hotTracked, active; defaultButton đổi theo nút đang có focus).
    const QAccessible::State s = iface->state();
    const std::pair<bool, const char*> flags[] = {
        {s.checkable, "checkable"},   {s.checked, "checked"},     {s.checkStateMixed, "mixed"},
        {s.disabled, "disabled"},     {s.focusable, "focusable"}, {s.readOnly, "readOnly"},
        {s.editable, "editable"},     {s.multiLine, "multiLine"}, {s.passwordEdit, "passwordEdit"},
        {s.searchEdit, "searchEdit"}, {s.hasPopup, "hasPopup"},   {s.modal, "modal"},
        {s.expandable, "expandable"}, {s.expanded, "expanded"},   {s.collapsed, "collapsed"},
        {s.selectable, "selectable"}, {s.selected, "selected"},   {s.multiSelectable, "multiSelectable"},
    };
    QJsonArray state;
    for (const auto& [on, name] : flags)
        if (on)
            state.append(QLatin1StringView(name));
    if (!state.isEmpty())
        o[u"state"_s] = state;
    QJsonArray children;
    if (!hasCells(role))
        for (int i = 0, n = iface->childCount(); i < n; ++i)
            if (QAccessibleInterface* c = iface->child(i); c && c->isValid() && !c->state().invisible)
                children.append(node(c));
    if (!children.isEmpty())
        o[u"children"_s] = children;
    return o;
}

} // namespace

QStringList checkNames(QWidget* root)
{
    QStringList issues;
    visit(QAccessible::queryAccessibleInterface(root), [&](QAccessibleInterface* iface) {
        const QAccessible::State s = iface->state();
        const auto* w = qobject_cast<const QWidget*>(iface->object());
        if (!s.focusable || (w && w->focusProxy())) // focus đi tới proxy, proxy được kiểm riêng
            return;
        const QString where = iface->object() ? describe(iface->object()) : nameOf(iface);
        if (nameOf(iface).trimmed().isEmpty())
            issues << where + u": không có tên cho trình đọc màn hình"_s;
        if (iface->role() == QAccessible::NoRole || iface->role() == QAccessible::Client)
            issues << where + u": không có vai trò (Client)"_s;
    });
    return issues;
}

QStringList checkFocus(QWidget* window, QStringList* order)
{
    if (!window->isActiveWindow())
        return {describe(window) + u": cửa sổ chưa active, không đi Tab được"_s};
    QList<QWidget*> stops; // theo chuỗi focus
    for (QWidget* w = window->nextInFocusChain(); w && w != window; w = w->nextInFocusChain())
        if (tabStop(w, window))
            stops << w;
    if (stops.isEmpty())
        return {};

    const QPointer<QWidget> old = QApplication::focusWidget();
    const bool keyboard = window->testAttribute(Qt::WA_KeyboardFocusChange);
    if (old)
        old->clearFocus();
    QHash<const QWidget*, QImage> before; // lúc chưa focus
    for (QWidget* w : stops)
        before.insert(w, around(w));
    // Như vừa Tab tới: đặt cờ sau setFocus (setFocus ngoài phím Tab xóa cờ).
    const auto focusByTab = [window](QWidget* w) {
        w->setFocus(Qt::TabFocusReason);
        window->setAttribute(Qt::WA_KeyboardFocusChange);
    };

    QStringList issues;
    const QColor focus = theme::ThemeManager::instance().color(tok::c_focus);
    QWidget* first = stops.first();
    focusByTab(first);
    QSet<const QWidget*> seen;
    bool closed = false;
    // Vòng đúng: mỗi điểm dừng một lần rồi về điểm đầu; gặp lại điểm đã qua là vòng con không qua điểm đầu.
    while (true) {
        QWidget* cur = QApplication::focusWidget();
        if (!cur || cur->window() != window) {
            issues << u"Tab đưa focus ra khỏi %1"_s.arg(describe(window));
            break;
        }
        if (seen.contains(cur))
            break;
        seen.insert(cur);
        if (order)
            *order << describe(cur);
        if (ringPixels(before.value(cur), around(cur), focus) < 2 * std::min(cur->width(), cur->height()))
            issues << describe(cur) + u": không thấy viền --c-focus khi focus bằng Tab"_s;
        pressTab(cur);
        QWidget* next = QApplication::focusWidget();
        if (next == cur) { // bẫy: Tab không rời; đi tiếp từ điểm dừng sau nó để kiểm phần còn lại
            issues << describe(cur) + u": bẫy focus, Tab không rời khỏi"_s;
            const qsizetype i = stops.indexOf(cur);
            if (i < 0 || i + 1 >= stops.size())
                break;
            focusByTab(stops[i + 1]);
            continue;
        }
        if (next == first) {
            closed = true;
            break;
        }
    }
    if (!closed)
        issues << u"Tab không vòng về điểm đầu %1"_s.arg(describe(first));
    // Sau khi đi: nút trong nhóm chọn một chỉ giữ Tab ở nút đang bật (QAbstractButton tự bỏ TabFocus của nút khác).
    for (QWidget* w = window->nextInFocusChain(); w && w != window; w = w->nextInFocusChain())
        if (tabStop(w, window) && !seen.contains(w))
            issues << describe(w) + u": Tab không tới được"_s;

    window->setAttribute(Qt::WA_KeyboardFocusChange, keyboard);
    if (old)
        old->setFocus(Qt::OtherFocusReason);
    else if (QWidget* f = QApplication::focusWidget())
        f->clearFocus();
    return issues;
}

QStringList audit(QWidget* window)
{
    return checkNames(window) + checkFocus(window);
}

QByteArray treeJson(QWidget* root)
{
    QAccessibleInterface* iface = QAccessible::queryAccessibleInterface(root);
    return iface ? QJsonDocument(node(iface)).toJson(QJsonDocument::Indented) : QByteArray();
}

} // namespace trion::a11y
