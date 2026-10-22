#include "app/widgets/controls/Controls.h"

#include "app/core/perf/UiProbe.h"
#include "app/widgets/controls/Paint.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "core/base/text/Fold.h"

#include <QAbstractListModel>
#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QPainter>
#include <QScreen>
#include <QStyleOptionSlider>
#include <QStyleOptionTab>
#include <QVBoxLayout>

#include <algorithm>
#include <vector>

using namespace Qt::StringLiterals;

namespace trion::ui {

namespace {

using theme::IconProvider;
using theme::IconState;

theme::ThemeManager& tm()
{
    return theme::ThemeManager::instance();
}
QColor col(tok::ColorId id)
{
    return tm().color(id);
}
int px(tok::PxId id)
{
    return qRound(tm().px(id));
}

// Giá trị của prototype không có token riêng.
constexpr int kBorder = 1;               // nét viền, nét chia 1 px (không phải khoảng cách)
constexpr qreal kDisabledOpacity = 0.45; // .btn[disabled]{opacity:.45}
constexpr int kChevron = 9;              // mũi tên .ic.xs: 9 px, nét --sw × 1,5 trên lưới 16

// Popup của QComboBox (QComboBoxPrivateContainer) và của ComboBox đều là con trực tiếp của ô.
bool popupOpen(const QComboBox* c)
{
    const QWidget* p = QApplication::activePopupWidget();
    return p && p->parentWidget() == c;
}

using paint::drawBox;
using paint::drawChevron;
using paint::drawRing;
using paint::focusVisible;
using paint::tinted;

QPixmap lockGlyph(int size, qreal dpr, const QColor& c)
{
    return tinted(IconProvider::instance().pixmap(u"lock"_s, size, dpr, IconState::Normal), c);
}

QFont::Weight weight(tok::NumId id)
{
    return QFont::Weight(qRound(tok::num(id)));
}

} // namespace

// Paint.h
namespace paint {

bool focusVisible(const QWidget* w)
{
    return w->hasFocus() && w->window()->testAttribute(Qt::WA_KeyboardFocusChange);
}

void drawBox(QPainter& p, const QRectF& r, const QColor& fill, const QColor& border)
{
    const qreal rad = tm().px(tok::r);
    p.setPen(border.isValid() ? QPen(border, kBorder) : QPen(Qt::NoPen));
    p.setBrush(fill.isValid() ? QBrush(fill) : QBrush(Qt::NoBrush));
    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), rad, rad);
}

void drawRing(QPainter& p, const QRectF& r, const QColor& c)
{
    const qreal rad = tm().px(tok::r);
    p.setPen(QPen(c, 2));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r.adjusted(1, 1, -1, -1), rad, rad);
}

void drawChevron(QPainter& p, QPointF c, const QColor& color)
{
    const qreal u = kChevron / 16.0; // một đơn vị của lưới biểu tượng 16 px
    p.setPen(QPen(color, tok::num(tok::sw) * 1.5 * u, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(QList<QPointF>{c + QPointF(-4 * u, -2 * u), c + QPointF(0, 2 * u), c + QPointF(4 * u, -2 * u)});
}

// Alpha giữ nguyên: bản Locked mờ 75% và huy hiệu ổ khóa của IconProvider vẫn còn hình.
QPixmap tinted(QPixmap pm, const QColor& c)
{
    QPainter p(&pm);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(pm.rect(), c);
    return pm;
}

} // namespace paint

// ---------------------------------------------------------------------------------------------------- Button

Button::Button(const QString& text, Kind kind, QWidget* parent) : QPushButton(text, parent), kind_(kind)
{
    setAttribute(Qt::WA_Hover);
}

void Button::setKind(Kind kind)
{
    kind_ = kind;
    updateGeometry();
    update();
}

void Button::setControlSize(Size size)
{
    size_ = size;
    updateGeometry();
    update();
}

void Button::setIconName(const QString& name)
{
    iconName_ = name;
    setIcon(name.isEmpty() ? QIcon() : IconProvider::icon(name, locked_));
}

void Button::setLocked(bool locked)
{
    if (locked_ == locked)
        return;
    locked_ = locked;
    if (!iconName_.isEmpty())
        setIcon(IconProvider::icon(iconName_, locked_));
    updateGeometry();
    update();
}

void Button::setError(bool error)
{
    error_ = error;
    update();
}

QRect Button::menuZone() const
{
    if (!split_)
        return {};
    const int w = px(tok::ic);
    return QStyle::visualRect(layoutDirection(), rect(), QRect(width() - w, 0, w, height()));
}

namespace {

// Chữ của nút: --fs (nhỏ: --fs-s), đậm --fw-m; nút chính --fw-b; đoạn chưa chọn thường. widest: cỡ đo bố cục
// (đoạn luôn đo theo bản đậm để chọn đoạn khác không làm nhóm đổi bề rộng).
QFont buttonFont(const Button* b, bool widest)
{
    QFont f = b->controlSize() == Button::Size::Small ? tm().font(tok::fs_s) : b->font();
    if (b->kind() == Button::Kind::Primary)
        f.setWeight(weight(tok::fw_b));
    else if (b->kind() != Button::Kind::Segment || b->isChecked() || widest)
        f.setWeight(weight(tok::fw_m));
    return f;
}

int padding(const Button* b)
{
    switch (b->kind()) {
    case Button::Kind::Icon:
        return px(tok::sp_2); // .ib padding 0 4px
    case Button::Kind::Segment:
        return px(tok::sp_4) + px(tok::sp_1); // .seg button padding 0 10px
    default:
        return px(b->controlSize() == Button::Size::Small   ? tok::sp_4   // .btn.sm ~9px
                  : b->controlSize() == Button::Size::Large ? tok::sp_6   // .btn.lg 16px
                                                            : tok::sp_5); // .btn 12px
    }
}

bool showsText(const Button* b)
{
    return b->kind() != Button::Kind::Icon && !b->text().isEmpty();
}

// Nội dung [ổ khóa][biểu tượng][chữ], cách nhau --sp-3 (.btn gap 6px). Ổ khóa riêng chỉ khi nút không có biểu tượng
// của bộ (có thì biểu tượng đã mang huy hiệu khóa).
int contentWidth(const Button* b, const QFont& f)
{
    int w = 0, parts = 0;
    const auto add = [&](int part) {
        w += part;
        ++parts;
    };
    if (b->isLocked() && b->iconName().isEmpty())
        add(px(tok::ic));
    if (!b->icon().isNull())
        add(px(tok::ic));
    if (showsText(b))
        add(QFontMetrics(f).size(Qt::TextShowMnemonic, b->text()).width());
    return w + px(tok::sp_3) * std::max(0, parts - 1);
}

} // namespace

QSize Button::sizeHint() const
{
    ensurePolished();
    const int h = kind_ == Kind::Segment ? px(tok::h_ctl) - 2 * kBorder // trong viền của .seg
                  : kind_ == Kind::Icon  ? px(tok::h_ctl)
                  : size_ == Size::Small ? px(tok::h_btn)
                  : size_ == Size::Large ? px(tok::h_ctl_l)
                                         : px(tok::h_ctl);
    const int w = 2 * padding(this) + contentWidth(this, buttonFont(this, true)) + menuZone().width();
    return {kind_ == Kind::Icon ? std::max(w, h) : w, h};
}

void Button::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool en = isEnabled();
    const bool hover = en && underMouse();
    const bool down = en && isDown();
    const bool on = isChecked();
    const bool flat = kind_ == Kind::Ghost || kind_ == Kind::Icon || kind_ == Kind::Segment;
    const QRectF box(rect());

    // Nền, viền, màu chữ theo app.css; nền đậm (chính, nguy hiểm) rê/nhấn phủ thêm --c-hover/--c-press.
    QColor fill, border, ink = col(tok::c_text), overlay;
    switch (kind_) {
    case Kind::Normal: // .btn; bật: phủ --c-on-bg, chữ --c-on
        fill = col(down ? tok::c_active : tok::c_raised);
        border = col(hover || down ? tok::c_text_2 : tok::c_border_ctl);
        if (on) {
            overlay = col(tok::c_on_bg);
            ink = col(tok::c_on);
        }
        break;
    case Kind::Primary:
        fill = col(hover || down ? tok::c_accent_hi : tok::c_accent);
        ink = col(tok::c_accent_t);
        break;
    case Kind::Danger: // chữ --c-accent-t: #FFF của prototype chỉ đạt 3:1 trên --c-danger ở chủ đề Tối
        fill = col(tok::c_danger);
        ink = col(tok::c_accent_t);
        if (hover && !down)
            overlay = col(tok::c_hover);
        break;
    case Kind::Ghost: // .btn.ghost, .ib: phẳng; rê --c-hover, nhấn --c-press, bật --c-on-bg
    case Kind::Icon:
        fill = on ? col(tok::c_on_bg) : down ? col(tok::c_press) : hover ? col(tok::c_hover) : QColor();
        ink = col(on ? tok::c_on : hover || down ? tok::c_text : tok::c_text_2);
        break;
    case Kind::Segment: // .seg button[aria-pressed=true]: --c-accent-soft, chữ --c-text
        fill = on ? col(tok::c_accent_soft) : down ? col(tok::c_press) : hover ? col(tok::c_hover) : QColor();
        ink = col(on || hover || down ? tok::c_text : tok::c_text_2);
        break;
    }
    if (down && (kind_ == Kind::Primary || kind_ == Kind::Danger))
        overlay = col(tok::c_press);
    if (locked_ && kind_ != Kind::Primary && kind_ != Kind::Danger && !on)
        ink = col(tok::c_text_2); // .locked
    if (!en && flat) {            // .ib[disabled]: chữ --c-text-dis, không nền
        fill = QColor();
        ink = col(tok::c_text_dis);
    }
    if (!en && !flat)
        p.setOpacity(kDisabledOpacity);

    drawBox(p, box, fill, border);
    if (overlay.isValid())
        drawBox(p, box, overlay, QColor());

    QRect cr = rect();
    if (const QRect zone = menuZone(); !zone.isNull()) { // vùng mũi tên của SplitButton
        if (menuOpen_)
            drawBox(p, zone, col(tok::c_press), QColor());
        if (kind_ == Kind::Normal)
            p.fillRect(QRect(zone.left() == 0 ? zone.right() : zone.left(), kBorder, kBorder, height() - 2 * kBorder),
                       border);
        drawChevron(p, QRectF(zone).center(), ink);
        cr = zone.left() == 0 ? cr.adjusted(zone.width(), 0, 0, 0) : cr.adjusted(0, 0, -zone.width(), 0);
    }
    if (error_)
        drawRing(p, box, col(tok::c_danger));
    if (focusVisible(this))
        drawRing(p, box, col(tok::c_focus));

    const int pad = padding(this), ic = px(tok::ic), gap = px(tok::sp_3);
    cr.adjust(pad, 0, -pad, 0);
    const QFont f = buttonFont(this, false);
    int x = cr.left() + std::max(0, (cr.width() - contentWidth(this, f)) / 2);
    const int iy = (height() - ic) / 2;
    const qreal dpr = p.device()->devicePixelRatio();
    if (locked_ && iconName_.isEmpty()) {
        p.drawPixmap(x, iy, lockGlyph(ic, dpr, ink));
        x += ic + gap;
    }
    if (!icon().isNull()) {
        p.drawPixmap(x, iy, tinted(icon().pixmap(QSize(ic, ic), dpr), ink));
        x += ic + gap;
    }
    if (showsText(this)) {
        const bool mnemonic = style()->styleHint(QStyle::SH_UnderlineShortcut, nullptr, this);
        p.setFont(f);
        p.setPen(ink);
        p.drawText(QRect(x, 0, cr.right() + 1 - x, height()),
                   Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine |
                       (mnemonic ? Qt::TextShowMnemonic : Qt::TextHideMnemonic),
                   text());
    }
}

void Button::keyPressEvent(QKeyEvent* e)
{
    if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && !(e->modifiers() & ~Qt::KeypadModifier)) {
        click(); // như Space; QPushButton chỉ nhận Enter trong QDialog
        return;
    }
    QPushButton::keyPressEvent(e);
}

void Button::nextCheckState()
{
    if (!locked_ || isChecked()) // khóa: không bật được; tắt vẫn được (nhóm độc quyền bỏ chọn đoạn cũ qua đây)
        QPushButton::nextCheckState();
}

// ---------------------------------------------------------------------------------------------------- SplitButton

SplitButton::SplitButton(const QString& text, QMenu* menu, QWidget* parent) : Button(text, Kind::Normal, parent)
{
    split_ = true;
    setDropMenu(menu);
}

void SplitButton::setDropMenu(QMenu* menu)
{
    if (menu_)
        disconnect(menu_, nullptr, this, nullptr);
    menu_ = menu;
    if (menu)
        connect(menu, &QMenu::aboutToHide, this, [this] {
            menuOpen_ = false;
            update();
        });
}

void SplitButton::showDropMenu()
{
    if (!menu_ || isLocked() || !isEnabled())
        return;
    menuOpen_ = true;
    update();
    // ponytail: menu luôn mở dưới mép trái; RTL, hết chỗ phía dưới do QMenu tự dời trong màn hình.
    menu_->popup(mapToGlobal(QPoint(0, height())));
}

void SplitButton::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && menu_ && !isLocked() && menuZone().contains(e->position().toPoint())) {
        showDropMenu();
        return;
    }
    Button::mousePressEvent(e);
}

void SplitButton::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Down && (e->modifiers() & Qt::AltModifier)) {
        showDropMenu();
        return;
    }
    Button::keyPressEvent(e);
}

ToggleButton::ToggleButton(const QString& text, Kind kind, QWidget* parent) : Button(text, kind, parent)
{
    setCheckable(true);
}

// ---------------------------------------------------------------------------------------------------- SegmentedGroup

// Tab dừng ở đoạn đang bật: Qt chỉ dời TabFocus khi một đoạn nhận focus (QAbstractButtonPrivate::fixFocusPolicy), nên
// đoạn đổi bằng mã trước lần focus đầu hoặc lúc focus ở ngoài nhóm thì Tab vào nhầm đoạn. Làm cùng việc đó mỗi khi đoạn
// bật đổi; trạng thái giống hệt sau khi Qt tự làm, nên mũi tên trong nhóm không đổi.
static void fixTabStop(QButtonGroup* group)
{
    for (QAbstractButton* b : group->buttons())
        b->setFocusPolicy(Qt::FocusPolicy(b->isChecked() ? b->focusPolicy() | Qt::TabFocus
                                                         : b->focusPolicy() & ~Qt::TabFocus));
}

SegmentedGroup::SegmentedGroup(QWidget* parent) : QWidget(parent), group_(new QButtonGroup(this))
{
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(kBorder, kBorder, kBorder, kBorder); // trong viền .seg
    row->setSpacing(kBorder);                                    // khe 1 px: nét chia --c-border
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(group_, &QButtonGroup::idToggled, this, [this](int id, bool on) {
        if (!on)
            return;
        fixTabStop(group_);
        emit currentIndexChanged(id);
    });
}

Button* SegmentedGroup::addSegment(const QString& text, const QString& iconName)
{
    auto* b = new Button(text, Button::Kind::Segment, this);
    b->setCheckable(true);
    if (!iconName.isEmpty())
        b->setIconName(iconName);
    const int id = count();
    group_->addButton(b, id);
    layout()->addWidget(b);
    if (id == 0)
        b->setChecked(true); // → fixTabStop
    else
        fixTabStop(group_);
    return b;
}

Button* SegmentedGroup::segment(int index) const
{
    return static_cast<Button*>(group_->button(index));
}

int SegmentedGroup::count() const
{
    return int(group_->buttons().size());
}

int SegmentedGroup::currentIndex() const
{
    return group_->checkedId();
}

void SegmentedGroup::setCurrentIndex(int index)
{
    if (QAbstractButton* b = group_->button(index))
        b->setChecked(true);
}

void SegmentedGroup::setError(bool error)
{
    error_ = error;
    update();
}

void SegmentedGroup::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF box(rect());
    drawBox(p, box, QColor(), col(isEnabled() ? tok::c_border_ctl : tok::c_border));
    for (int i = 1; i < count(); ++i)
        p.fillRect(QRect(segment(i)->x() - kBorder, kBorder, kBorder, height() - 2 * kBorder), col(tok::c_border));
    if (error_)
        drawRing(p, box, col(tok::c_danger));
}

// ---------------------------------------------------------------------------------------------------- DropDown

DropDown::DropDown(QWidget* parent) : QComboBox(parent)
{
    setAttribute(Qt::WA_Hover);
}

void DropDown::setLocked(bool locked)
{
    locked_ = locked;
    updateGeometry();
    update();
}

void DropDown::setError(bool error)
{
    error_ = error;
    update();
}

void DropDown::showPopup()
{
    if (!locked_)
        QComboBox::showPopup();
}

QSize DropDown::sizeHint() const
{
    QSize s = QComboBox::sizeHint(); // rộng theo mục dài nhất (Qt tính, có cache), cao --h-ctl (TrionStyle)
    if (locked_)
        s.rwidth() += px(tok::ic) + px(tok::sp_3);
    return s;
}

void DropDown::keyPressEvent(QKeyEvent* e)
{
    if (locked_)
        return e->ignore();
    QComboBox::keyPressEvent(e);
}

void DropDown::wheelEvent(QWheelEvent* e)
{
    if (locked_)
        return e->ignore();
    QComboBox::wheelEvent(e);
}

void DropDown::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool en = isEnabled(), open = popupOpen(this), hover = en && (underMouse() || open);
    const QRectF box(rect());
    // .dd: nền --c-raised, viền --c-border-ctl, rê --c-text-2; đang mở như .btn:active
    drawBox(p, box, col(open ? tok::c_active : tok::c_raised),
            col(!en     ? tok::c_border
                : hover ? tok::c_text_2
                        : tok::c_border_ctl));
    if (error_)
        drawRing(p, box, col(tok::c_danger));
    if (focusVisible(this))
        drawRing(p, box, col(tok::c_focus));

    const int ic = px(tok::ic), gap = px(tok::sp_3);
    QRect cr = rect().adjusted(px(tok::sp_4), 0, -px(tok::sp_3), 0); // .dd padding 0 6px 0 8px
    drawChevron(p, QPointF(cr.right() + 1 - kChevron / 2.0, height() / 2.0), col(en ? tok::c_text_2 : tok::c_text_dis));
    cr.setRight(cr.right() - kChevron - gap);
    const QColor ink = col(!en ? tok::c_text_dis : locked_ ? tok::c_text_2 : tok::c_text);
    const qreal dpr = p.device()->devicePixelRatio();
    const int iy = (height() - ic) / 2;
    if (locked_) {
        p.drawPixmap(cr.left(), iy, lockGlyph(ic, dpr, ink));
        cr.setLeft(cr.left() + ic + gap);
    }
    if (const QIcon icon = itemIcon(currentIndex()); !icon.isNull()) { // ô màu, biểu tượng của mục: không tô lại
        p.drawPixmap(cr.left(), iy, icon.pixmap(QSize(ic, ic), dpr, en ? QIcon::Normal : QIcon::Disabled));
        cr.setLeft(cr.left() + ic + gap);
    }
    const bool empty = currentIndex() < 0;
    p.setFont(font());
    p.setPen(empty ? col(tok::c_text_3) : ink);
    p.drawText(cr, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
               fontMetrics().elidedText(empty ? placeholderText() : currentText(), Qt::ElideRight, cr.width()));
}

// ---------------------------------------------------------------------------------------------------- ComboBox

// Các dòng của model nguồn có khóa gấp chứa chuỗi đã gõ. Khóa (foldForSearch của từng dòng) dựng lại chỉ khi model
// nguồn đổi; mỗi phím: gấp chuỗi gõ một lần, quét mảng khóa, đặt lại danh sách (một lần reset, view tự ảo hóa).
class ComboBox::FilterModel final : public QAbstractListModel {
public:
    using QAbstractListModel::QAbstractListModel;

    void setSource(QAbstractItemModel* source, int column)
    {
        if (source == source_ && column == column_)
            return;
        if (source_)
            disconnect(source_, nullptr, this, nullptr);
        source_ = source;
        column_ = column;
        keys_.clear();
        if (!source)
            return;
        // Nguồn đổi: rows_ trỏ số dòng cũ → lọc lại một lần ở lượt vòng sự kiện kế (model nền phát nhiều thao tác mỗi lô).
        const auto dirty = [this] {
            keys_.clear();
            if (refilter && !pending_) {
                pending_ = true;
                QMetaObject::invokeMethod(this, [this] { pending_ = false; refilter(); }, Qt::QueuedConnection);
            }
        };
        connect(source, &QAbstractItemModel::dataChanged, this, dirty);
        connect(source, &QAbstractItemModel::rowsInserted, this, dirty);
        connect(source, &QAbstractItemModel::rowsRemoved, this, dirty);
        connect(source, &QAbstractItemModel::rowsMoved, this, dirty);
        connect(source, &QAbstractItemModel::modelReset, this, dirty);
        connect(source, &QAbstractItemModel::layoutChanged, this, dirty);
    }

    void ensureKeys()
    {
        const int n = source_ ? source_->rowCount() : 0;
        if (qsizetype(keys_.size()) == n)
            return;
        keys_.clear();
        keys_.reserve(n);
        for (int i = 0; i < n; ++i)
            keys_.push_back(text::foldForSearch(source_->index(i, column_).data().toString()));
    }

    void setNeedle(const QString& typed)
    {
        beginResetModel();
        ensureKeys();
        const QString needle = text::foldForSearch(typed);
        rows_.clear();
        for (int i = 0; i < int(keys_.size()); ++i)
            if (keys_[i].contains(needle))
                rows_.push_back(i);
        endResetModel();
    }

    int sourceRow(int row) const { return row >= 0 && row < int(rows_.size()) ? rows_[row] : -1; }

    std::function<void()> refilter; // ComboBox đặt: lọc lại theo chuỗi đang gõ khi popup đang mở

    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : int(rows_.size()); }

    QVariant data(const QModelIndex& index, int role) const override
    {
        const int r = sourceRow(index.row());
        return source_ && r >= 0 ? source_->index(r, column_).data(role) : QVariant();
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        const int r = sourceRow(index.row());
        return source_ && r >= 0 ? source_->flags(source_->index(r, column_)) : Qt::NoItemFlags;
    }

private:
    QPointer<QAbstractItemModel> source_;
    int column_ = 0;
    std::vector<QString> keys_; // foldForSearch của từng dòng nguồn; lệch số dòng = cần dựng lại
    std::vector<int> rows_;     // dòng nguồn khớp chuỗi lọc
    bool pending_ = false;      // đã hẹn lọc lại
};

// Popup: ô lọc (SearchField), danh sách, dòng "không có mục khớp"; nền --c-raised viền --c-border-ctl như .menu.
class ComboBox::Popup final : public QWidget {
public:
    explicit Popup(ComboBox* combo)
        : QWidget(combo, Qt::Popup), filter(new SearchField(this)), list(new QListView(this)), empty(new QLabel(this))
    {
        auto* box = new QVBoxLayout(this);
        const int m = px(tok::sp_2); // ponytail: lề đọc lúc dựng; --sp-* như nhau ở mọi chế độ
        box->setContentsMargins(m, m, m, m);
        box->setSpacing(m);
        box->addWidget(filter);
        box->addWidget(list, 1);
        box->addWidget(empty, 1);
        list->setUniformItemSizes(true);
        list->setFrameShape(QFrame::NoFrame);
        list->setFocusPolicy(Qt::NoFocus); // phím ở ô lọc, ↑↓ chuyển cho danh sách
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->viewport()->setAutoFillBackground(false); // nền của popup (--c-raised)
        empty->setAlignment(Qt::AlignCenter);
        empty->setForegroundRole(QPalette::PlaceholderText);
        empty->hide();
    }

    SearchField* filter;
    QListView* list;
    QLabel* empty;

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), col(tok::c_raised));
        p.setPen(col(tok::c_border_ctl));
        p.drawRect(rect().adjusted(0, 0, -kBorder, -kBorder));
    }
    void hideEvent(QHideEvent* e) override
    {
        QWidget::hideEvent(e);
        parentWidget()->update(); // ô thôi vẽ trạng thái đang mở
    }
};

ComboBox::ComboBox(QWidget* parent) : DropDown(parent) {}

void ComboBox::showPopup()
{
    if (isLocked())
        return;
    if (!popup_) {
        model_ = new FilterModel(this);
        popup_ = new Popup(this);
        popup_->list->setModel(model_);
        popup_->filter->installEventFilter(this);
        connect(popup_->filter, &QLineEdit::textChanged, this, &ComboBox::applyFilter);
        connect(popup_->filter, &QLineEdit::returnPressed, this, [this] { activate(popup_->list->currentIndex()); });
        connect(popup_->list, &QListView::clicked, this, &ComboBox::activate);
        // ponytail: lọc lại đưa dòng chọn về mục khớp đầu tiên (không giữ mục đang trỏ bằng ↑↓); giữ được bằng
        // QPersistentModelIndex của nguồn nếu người dùng thấy phiền.
        model_->refilter = [this] {
            if (popup_->isVisible())
                applyFilter(popup_->filter->text());
        };
        retranslate();
    }
    model_->setSource(model(), modelColumn());

    // Rộng bằng ô, cao đủ maxVisibleItems() dòng; mở xuống dưới, không đủ chỗ thì lên trên.
    const int m = px(tok::sp_2);
    const int rowH = std::max(popup_->list->sizeHintForRow(0), px(tok::h_row));
    const int h = 3 * m + popup_->filter->sizeHint().height() + std::clamp(count(), 1, maxVisibleItems()) * rowH;
    QPoint at = mapToGlobal(QPoint(0, height()));
    if (const QScreen* s = screen(); s && at.y() + h > s->availableGeometry().bottom() + 1)
        at = mapToGlobal(QPoint(0, -h));
    popup_->setGeometry(QRect(at, QSize(width(), h)));
    popup_->show();
    popup_->filter->setFocus(Qt::PopupFocusReason);
    if (popup_->filter->text().isEmpty())
        applyFilter({});
    else
        popup_->filter->clear(); // → applyFilter
    update();
}

void ComboBox::hidePopup()
{
    if (popup_)
        popup_->hide();
    update();
}

void ComboBox::applyFilter(const QString& text)
{
    model_->setNeedle(text);
    const bool any = model_->rowCount() > 0;
    popup_->list->setVisible(any);
    popup_->empty->setVisible(!any);
    if (any) { // chưa gõ: mục đang chọn; đã gõ: mục khớp đầu tiên (Enter chọn ngay)
        const QModelIndex i = model_->index(text.isEmpty() ? std::max(currentIndex(), 0) : 0);
        popup_->list->setCurrentIndex(i);
        popup_->list->scrollTo(i);
    }
    if (popup_->isVisible())
        probe::untilFrame(probe::Op::Autocomplete, popup_); // DoD-L: danh sách cập nhật sau mỗi phím
}

void ComboBox::activate(const QModelIndex& index)
{
    const int row = model_->sourceRow(index.row());
    if (row < 0 || row >= count() || !(index.flags() & Qt::ItemIsEnabled))
        return;
    hidePopup();
    setCurrentIndex(row);
    emit activated(row);
    emit textActivated(itemText(row));
}

bool ComboBox::eventFilter(QObject* watched, QEvent* e)
{
    if (popup_ && watched == popup_->filter && e->type() == QEvent::KeyPress) {
        auto* k = static_cast<QKeyEvent*>(e);
        switch (k->key()) {
        case Qt::Key_Up:
            if (k->modifiers() & Qt::AltModifier) {
                hidePopup();
                return true;
            }
            [[fallthrough]];
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            QCoreApplication::sendEvent(popup_->list, k);
            return true;
        case Qt::Key_Escape:
        case Qt::Key_F4:
            hidePopup(); // không đổi giá trị
            return true;
        default:
            break;
        }
    }
    return DropDown::eventFilter(watched, e);
}

void ComboBox::retranslate()
{
    if (!popup_)
        return;
    const QString filter = tr("Lọc…");
    popup_->filter->setPlaceholderText(filter);
    popup_->filter->setAccessibleName(filter);
    popup_->empty->setText(tr("Không có mục khớp"));
}

void ComboBox::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    DropDown::changeEvent(e);
}

// ---------------------------------------------------------------------------------------------------- SearchField

SearchField::SearchField(QWidget* parent)
    : QLineEdit(parent), lead_(addAction(IconProvider::icon(u"search"_s), LeadingPosition)),
      clear_(addAction(IconProvider::icon(u"x"_s), TrailingPosition))
{
    setAttribute(Qt::WA_Hover);
    clear_->setVisible(false);
    connect(clear_, &QAction::triggered, this, &QLineEdit::clear);
    connect(this, &QLineEdit::textChanged, this,
            [this](const QString& t) { clear_->setVisible(!t.isEmpty() && !isReadOnly()); });
    retranslate();
}

void SearchField::setLocked(bool locked)
{
    locked_ = locked;
    setReadOnly(locked);
    lead_->setIcon(IconProvider::icon(locked ? u"lock"_s : u"search"_s));
    clear_->setVisible(!locked && !text().isEmpty());
    update();
}

void SearchField::setError(bool error)
{
    error_ = error;
    update();
}

bool SearchField::event(QEvent* e)
{
    if (e->type() == QEvent::ShortcutOverride && static_cast<QKeyEvent*>(e)->key() == Qt::Key_Escape &&
        !text().isEmpty() && !isReadOnly()) {
        e->accept();
        return true;
    }
    return QLineEdit::event(e);
}

void SearchField::paintEvent(QPaintEvent* e)
{
    QLineEdit::paintEvent(e); // nền, viền 1 px (TrionStyle .inp), chữ; biểu tượng là widget con
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF box(rect());
    if (error_) // .inp.err
        drawRing(p, box, col(tok::c_danger));
    else if (hasFocus()) // .search:focus-within: viền --c-focus + bóng 1 px
        drawRing(p, box, col(tok::c_focus));
    else if (isEnabled() && underMouse())
        drawBox(p, box, QColor(), col(tok::c_text_2));
}

void SearchField::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape && !text().isEmpty() && !isReadOnly()) {
        clear();
        return;
    }
    QLineEdit::keyPressEvent(e);
}

void SearchField::retranslate()
{
    clear_->setText(tr("Xóa chữ đã gõ"));
}

void SearchField::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QLineEdit::changeEvent(e);
}

// ---------------------------------------------------------------------------------------------------- TabBar

TabBar::TabBar(QWidget* parent) : QTabBar(parent)
{
    setAttribute(Qt::WA_Hover);
    setDrawBase(false);
    setExpanding(false);
    setUsesScrollButtons(true);
    setElideMode(Qt::ElideNone);
}

void TabBar::setTabLocked(int index, bool locked)
{
    setTabIcon(index, locked ? IconProvider::icon(u"lock"_s) : QIcon());
}

void TabBar::setTabError(int index, bool error)
{
    setTabIcon(index, error ? IconProvider::icon(u"error"_s) : QIcon());
}

// QCommonStyle vẽ focus của tab bằng drawPrimitive của Fusion (không qua TrionStyle): bỏ cờ, tự vẽ viền trong
// paintEvent.
void TabBar::initStyleOption(QStyleOptionTab* option, int tabIndex) const
{
    QTabBar::initStyleOption(option, tabIndex);
    option->state &= ~QStyle::State_HasFocus;
}

void TabBar::paintEvent(QPaintEvent* e)
{
    {
        QPainter p(this); // .tabs: nét chia dưới dải; gạch chân tab đang mở (TrionStyle) đè lên nó
        p.fillRect(QRect(0, height() - kBorder, width(), kBorder), col(tok::c_border));
    }
    QTabBar::paintEvent(e);
    if (focusVisible(this) && currentIndex() >= 0) {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        drawRing(p, tabRect(currentIndex()), col(tok::c_focus));
    }
}

// ---------------------------------------------------------------------------------------------------- Slider

Slider::Slider(Qt::Orientation orientation, QWidget* parent) : QSlider(orientation, parent)
{
    setAttribute(Qt::WA_Hover);
}

void Slider::setLocked(bool locked)
{
    locked_ = locked;
    update();
}

void Slider::setError(bool error)
{
    error_ = error;
    update();
}

QSize Slider::sizeHint() const
{
    QSize s = QSlider::sizeHint();
    (orientation() == Qt::Horizontal ? s.rheight() : s.rwidth()) = px(tok::h_ctl);
    return s;
}

QSize Slider::minimumSizeHint() const
{
    QSize s = QSlider::minimumSizeHint();
    (orientation() == Qt::Horizontal ? s.rheight() : s.rwidth()) = px(tok::h_ctl);
    return s;
}

void Slider::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    // Vị trí rãnh và núm theo style (QSlider dò chuột bằng chính các vùng này), hình vẽ theo token.
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    const QRectF groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
    const QPointF c = QRectF(style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this)).center();
    const bool horizontal = orientation() == Qt::Horizontal, en = isEnabled();
    const qreal t = tm().px(tok::sp_2), r = tm().px(tok::ic) / 2, rad = tm().px(tok::r);

    QRectF track = horizontal ? QRectF(groove.left(), c.y() - t / 2, groove.width(), t)
                              : QRectF(c.x() - t / 2, groove.top(), t, groove.height());
    p.setPen(Qt::NoPen);
    p.setBrush(col(en ? tok::c_border_ctl : tok::c_border));
    p.drawRoundedRect(track, rad, rad);
    // Phần đã chọn: từ đầu giá trị nhỏ nhất đến tâm núm (upsideDown: nhỏ nhất ở phải/dưới).
    if (horizontal)
        opt.upsideDown ? track.setLeft(c.x()) : track.setRight(c.x());
    else
        opt.upsideDown ? track.setTop(c.y()) : track.setBottom(c.y());
    const QColor fill = col(!en                              ? tok::c_text_dis
                            : error_                         ? tok::c_danger
                            : locked_                        ? tok::c_text_2
                            : isSliderDown() || underMouse() ? tok::c_accent_hi
                                                             : tok::c_accent);
    p.setBrush(fill);
    p.drawRoundedRect(track, rad, rad);

    if (isSliderDown()) { // quầng khi kéo
        p.setBrush(col(tok::c_accent_soft));
        p.drawEllipse(c, r + t, r + t);
    }
    if (locked_) { // núm rỗng mang ổ khóa
        p.setPen(QPen(col(tok::c_border_ctl), kBorder));
        p.setBrush(col(tok::c_raised));
        p.drawEllipse(c, r - 0.5, r - 0.5);
        const int s = px(tok::ic) - px(tok::sp_2);
        p.drawPixmap(QPointF(c.x() - s / 2.0, c.y() - s / 2.0),
                     lockGlyph(s, p.device()->devicePixelRatio(), col(tok::c_text_2)));
    } else {
        p.setBrush(fill);
        p.drawEllipse(c, r, r);
    }
    if (focusVisible(this)) {
        p.setPen(QPen(col(tok::c_focus), 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(c, r + 2, r + 2);
    }
}

void Slider::mousePressEvent(QMouseEvent* e)
{
    if (locked_)
        return e->ignore();
    QSlider::mousePressEvent(e);
}

void Slider::keyPressEvent(QKeyEvent* e)
{
    if (locked_)
        return e->ignore();
    QSlider::keyPressEvent(e);
}

void Slider::wheelEvent(QWheelEvent* e)
{
    if (locked_)
        return e->ignore();
    QSlider::wheelEvent(e);
}

} // namespace trion::ui
