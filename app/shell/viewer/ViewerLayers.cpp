#include "app/shell/viewer/ViewerLayers.h"

#include "app/core/UiExecutor.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/i18n/UiFormat.h"
#include "app/theme/ThemeManager.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/controls/Paint.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/model/BackgroundModel.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "app/widgets/views/TextCache.h"
#include "app/widgets/views/VirtualTableView.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/text/Fold.h"

#include <QBoxLayout>
#include <QFileInfo>
#include <QHash>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QRegularExpression>
#include <QtMath>

#include <algorithm>
#include <optional>
#include <vector>

using namespace Qt::StringLiterals;

namespace trion::shell {

using theme::IconProvider;
using theme::IconState;
using theme::TextRole;
using ui::Button;

namespace {

constexpr int kSmallIcon = 12; // .ic.s12: dấu layer hiện hành
constexpr qreal kOffOpacity = 0.7;      // .tbl .tgl.off
constexpr qreal kDisabledOpacity = 0.35; // .tbl .tgl[disabled]

theme::ThemeManager& tm()
{
    return theme::ThemeManager::instance();
}

int px(tok::PxId id)
{
    return qRound(tm().px(id));
}

QColor col(tok::ColorId id)
{
    return tm().color(id);
}

void setInk(QWidget* w, tok::ColorId id)
{
    QPalette p = w->palette();
    p.setColor(QPalette::WindowText, col(id));
    w->setPalette(p);
}

// Dải của palette: nền (nếu có), nét --c-border 1 px ở trên hoặc dưới (.pal-note, .pal-tools, .pal-foot).
class Band final : public QWidget {
public:
    Band(std::optional<tok::ColorId> fill, bool lineOnTop, QWidget* parent)
        : QWidget(parent), fill_(fill), top_(lineOnTop)
    {
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        if (fill_)
            p.fillRect(rect(), col(*fill_));
        p.fillRect(QRect(0, top_ ? 0 : height() - 1, width(), 1), col(tok::c_border));
    }

private:
    std::optional<tok::ColorId> fill_;
    bool top_;
};

// Dòng của bảng: khóa là chỉ số trong snapshot; folded: tên đã gấp dấu (lọc, sắp).
struct LayerRow {
    std::uint64_t key = 0;
    QString folded;
    ViewerLayer layer;
    bool operator==(const LayerRow&) const = default;
};

int statusRank(const ViewerLayer& l)
{
    return l.current ? 0 : l.objects > 0 ? 1 : 2;
}

// Thứ tự theo cột rồi theo tên, khóa: tất định.
bool before(const LayerRow& a, const LayerRow& b, int column)
{
    const ViewerLayer &x = a.layer, &y = b.layer;
    switch (column) {
    case ViewerLayers::Status:
        if (statusRank(x) != statusRank(y))
            return statusRank(x) < statusRank(y);
        break;
    case ViewerLayers::On:
        if (x.on != y.on)
            return x.on;
        break;
    case ViewerLayers::Freeze:
        if (x.frozen != y.frozen)
            return !x.frozen;
        break;
    case ViewerLayers::Lock:
        if (x.locked != y.locked)
            return !x.locked;
        break;
    case ViewerLayers::Color:
        if (x.color.rgba() != y.color.rgba())
            return x.color.rgba() < y.color.rgba();
        break;
    case ViewerLayers::Count:
        if (x.objects != y.objects)
            return x.objects < y.objects;
        break;
    default:
        break;
    }
    if (const int c = a.folded.compare(b.folded); c != 0)
        return c < 0;
    if (const int c = x.name.compare(y.name); c != 0)
        return c < 0;
    return a.key < b.key;
}

} // namespace

// ------------------------------------------------------------------------------------------------- Model

class ViewerLayers::Model final : public ui::BackgroundModel<LayerRow> {
public:
    explicit Model(ViewerLayers* owner) : BackgroundModel(owner), owner_(owner) {}

    int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : ColumnCount; }

    QVariant data(const QModelIndex& i, int role) const override
    {
        if (!i.isValid())
            return {};
        const ViewerLayer& l = row(i.row()).layer;
        switch (role) {
        case Qt::DisplayRole:
            return i.column() == Name ? QVariant(l.name)
                   : i.column() == Count ? QVariant(i18n::formatCount(l.objects))
                                         : QVariant();
        case Qt::ToolTipRole:
            return tip(l, i.column());
        case Qt::TextAlignmentRole:
            return i.column() == Count ? QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter) : QVariant();
        case Qt::FontRole: // .tbl td.num
            return i.column() == Count ? QVariant::fromValue(theme::font(TextRole::MonoLabel)) : QVariant();
        default:
            return {};
        }
    }

    QVariant headerData(int section, Qt::Orientation o, int role) const override
    {
        if (o != Qt::Horizontal)
            return {};
        static constexpr const char* kIcons[ColumnCount] = {nullptr, nullptr, "bulb", "snow", "lock", "plotstyle",
                                                            nullptr};
        switch (role) {
        case Qt::DisplayRole:
            return section == Name ? ViewerLayers::tr("Tên") : section == Count ? ViewerLayers::tr("Số") : QVariant();
        case Qt::DecorationRole:
            return kIcons[section] ? QVariant::fromValue(IconProvider::icon(QLatin1StringView(kIcons[section])))
                                   : QVariant();
        case Qt::ToolTipRole:
            return headerTip(section);
        case Qt::TextAlignmentRole:
            return section == Count ? QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter)
                   : section == Name ? QVariant()
                                     : QVariant::fromValue(Qt::AlignCenter);
        default:
            return {};
        }
    }

    void sort(int column, Qt::SortOrder order) override
    {
        owner_->sortColumn_ = column;
        owner_->sortOrder_ = order;
        owner_->refresh();
    }

    void retranslate() { emit headerDataChanged(Qt::Horizontal, 0, ColumnCount - 1); }

    static QString headerTip(int column)
    {
        switch (column) {
        case Status:
            return ViewerLayers::tr("Trạng thái");
        case Name:
            return ViewerLayers::tr("Tên");
        case On:
            return ViewerLayers::tr("Bật/Tắt");
        case Freeze:
            return ViewerLayers::tr("Đóng băng");
        case Lock:
            return ViewerLayers::tr("Khóa");
        case Color:
            return ViewerLayers::tr("Màu");
        default:
            return ViewerLayers::tr("Số đối tượng");
        }
    }

    // Chú thích của ô: trạng thái bằng chữ (không chỉ bằng biểu tượng, màu), ô đổi tạm, ô khóa theo phiên bản.
    static QString tip(const ViewerLayer& l, int column)
    {
        const auto temp = [](const QString& s, bool changed) {
            return changed ? ViewerLayers::tr("%1 · đã đổi tạm thời").arg(s) : s;
        };
        const auto pro = [](const QString& s) {
            return u"%1 · %2"_s.arg(s, edition::EditionGate::availableIn(ui::Edition::Pro));
        };
        switch (column) {
        case Status:
            return l.current       ? ViewerLayers::tr("Layer hiện hành")
                   : l.objects > 0 ? ViewerLayers::tr("Đang dùng")
                                   : ViewerLayers::tr("Không có đối tượng");
        case Name:
            return l.name;
        case On:
            return temp(l.on ? ViewerLayers::tr("Đang bật") : ViewerLayers::tr("Đang tắt"), l.on != l.fileOn);
        case Freeze:
            return temp(l.frozen ? ViewerLayers::tr("Đang đóng băng") : ViewerLayers::tr("Không đóng băng"),
                        l.frozen != l.fileFrozen);
        case Lock:
            return pro(l.locked ? ViewerLayers::tr("Đang khóa") : ViewerLayers::tr("Không khóa"));
        case Color:
            return pro(ViewerLayers::tr("Màu"));
        default:
            return ViewerLayers::tr("Số đối tượng");
        }
    }

private:
    ViewerLayers* owner_;
};

// ------------------------------------------------------------------------------------------------- Delegate

// Ô theo .tbl của prototype: dấu trạng thái, tên kèm thẻ "tạm", nút bật/tắt (.tgl, tắt mờ .7, khóa mờ .35), ô màu
// (.sw); ô hiện hành của cột bấm được có viền focus khi focus đến từ bàn phím (Space, Enter bấm ô đó).
class ViewerLayers::Delegate final : public ui::ViewItemDelegate {
public:
    Delegate(Model& model, QObject* parent) : ui::ViewItemDelegate(parent), model_(model) {}

    void paint(QPainter* p, const QStyleOptionViewItem& o, const QModelIndex& i) const override
    {
        const ViewerLayer& l = model_.row(i.row()).layer;
        const int column = i.column();
        p->save();
        p->setRenderHint(QPainter::SmoothPixmapTransform, false);
        const int icon = px(tok::ic);
        switch (column) {
        case Status:
            if (l.current) { // .tag.acc với dấu chọn
                QRect tag(0, 0, kSmallIcon + 2 * px(tok::sp_2), kSmallIcon + px(tok::sp_2));
                tag.moveCenter(o.rect.center());
                fill(*p, tag, col(tok::c_accent_soft));
                glyph(*p, u"check"_s, kSmallIcon, tag, col(tok::c_accent));
            } else { // ● đang dùng, ○ không có đối tượng: hình khác nhau, không chỉ khác màu
                QRectF dot(0, 0, px(tok::sp_3), px(tok::sp_3));
                dot.moveCenter(QRectF(o.rect).center());
                p->setRenderHint(QPainter::Antialiasing);
                const QColor c = col(l.objects > 0 ? tok::c_text_2 : tok::c_text_3);
                p->setPen(l.objects > 0 ? QPen(Qt::NoPen) : QPen(c, 1));
                p->setBrush(l.objects > 0 ? QBrush(c) : QBrush(Qt::NoBrush));
                p->drawEllipse(dot.adjusted(0.5, 0.5, -0.5, -0.5));
            }
            break;
        case Name:
            name(*p, o, l);
            break;
        case On:
        case Freeze:
        case Lock: {
            const bool off = column == On ? !l.on : column == Freeze ? l.frozen : l.locked;
            const QString shape = column == On       ? (l.on ? u"bulb"_s : u"bulboff"_s)
                                  : column == Freeze ? (l.frozen ? u"snow"_s : u"sun"_s)
                                                     : (l.locked ? u"lock"_s : u"unlock"_s);
            p->setOpacity(column == Lock ? kDisabledOpacity : off ? kOffOpacity : 1.0);
            glyph(*p, shape, icon, o.rect, col(off ? tok::c_text_3 : tok::c_text_2));
            break;
        }
        case Count: // .tbl td.num: Mono, phải
            p->setPen(col(tok::c_text));
            counts_.draw(*p, theme::font(TextRole::MonoLabel), o.rect.adjusted(px(tok::sp_4), 0, -px(tok::sp_4), 0),
                         i18n::formatCount(l.objects), Qt::ElideRight, Qt::AlignRight);
            break;
        case Color: {
            QRect sw(0, 0, px(tok::sp_5), px(tok::sp_5)); // .sw
            sw.moveCenter(o.rect.center());
            fill(*p, sw.adjusted(-1, -1, 1, 1), col(tok::c_border_ctl));
            fill(*p, sw, l.color.isValid() ? l.color : col(tok::c_text));
            break;
        }
        default:
            break;
        }
        p->restore();
        const auto* view = qobject_cast<const QAbstractItemView*>(o.widget);
        if (column >= On && column <= Color && view && view->hasFocus() && view->currentIndex() == i &&
            ui::paint::focusVisible(view))
            ui::paint::drawRing(*p, o.rect.adjusted(1, 1, -1, -1), col(tok::c_focus));
    }

    QSize sizeHint(const QStyleOptionViewItem& o, const QModelIndex& i) const override
    {
        const QSize base = ui::ViewItemDelegate::sizeHint(o, i);
        if (i.column() != Name)
            return i.column() == Count ? base : QSize(iconColumn(), base.height());
        const ViewerLayer& l = model_.row(i.row()).layer;
        return {2 * px(tok::sp_4) + o.fontMetrics.horizontalAdvance(l.name) + (l.changed() ? tagWidth() : 0),
                base.height()};
    }

    static int iconColumn() { return px(tok::ic) + 2 * px(tok::sp_3); } // colgroup 28 px của cột biểu tượng

private:
    // Biểu tượng tô màu, cache theo (tên, màu, cỡ, DPR): mỗi lần vẽ bảng có ~125 ô biểu tượng, dựng lại pixmap từng ô
    // tốn vài ms. ponytail: cache không giới hạn, cỡ = số biểu tượng × số màu token × DPR (vài chục mục).
    void glyph(QPainter& p, const QString& name, int size, const QRect& box, const QColor& c) const
    {
        const qreal dpr = p.device()->devicePixelRatio();
        const QString key = u"%1 %2 %3 %4"_s.arg(name).arg(c.rgba()).arg(size).arg(dpr);
        auto it = glyphs_.constFind(key);
        if (it == glyphs_.cend())
            it = glyphs_.insert(
                key, ui::paint::tinted(IconProvider::instance().pixmap(name, size, dpr, IconState::Normal), c));
        QRect r(0, 0, size, size);
        r.moveCenter(box.center());
        p.drawPixmap(r, *it);
    }

    static void fill(QPainter& p, const QRect& r, const QColor& c)
    {
        p.save();
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawRoundedRect(r, tm().px(tok::r), tm().px(tok::r));
        p.restore();
    }

    static QString tagText() { return ViewerLayers::tr("tạm"); }
    static int tagWidth() // khoảng cách trước thẻ + thẻ
    {
        return px(tok::sp_3) + 2 * px(tok::sp_2) +
               QFontMetrics(theme::font(TextRole::Badge, theme::Weight::Medium)).horizontalAdvance(tagText());
    }

    // Tên (rút gọn "…" khi hẹp) rồi thẻ .tag.info "tạm" khi layer đổi tạm.
    void name(QPainter& p, const QStyleOptionViewItem& o, const ViewerLayer& l) const
    {
        const QRect box = o.rect.adjusted(px(tok::sp_4), 0, -px(tok::sp_4), 0);
        const int tag = l.changed() ? tagWidth() : 0;
        p.setPen(col(tok::c_text));
        const qreal drawn = names_.draw(p, o.font, box.adjusted(0, 0, -tag, 0), l.name);
        if (!tag)
            return;
        const QFont badge = theme::font(TextRole::Badge, theme::Weight::Medium);
        QRect r(box.left() + qCeil(drawn) + px(tok::sp_3), 0, tag - px(tok::sp_3),
                QFontMetrics(badge).height() + px(tok::sp_1));
        r.moveTop(box.center().y() - r.height() / 2);
        fill(p, r, col(tok::c_info_soft));
        p.setPen(col(tok::c_info));
        tags_.draw(p, badge, r.adjusted(px(tok::sp_2), 0, 0, 0), tagText());
    }

    Model& model_;
    mutable QHash<QString, QPixmap> glyphs_;
    // Chữ đã shaping, mỗi font một bản: vẽ lại bảng sau mỗi phím lọc không shaping lại các dòng.
    mutable ui::TextCache names_, counts_, tags_;
};

// ------------------------------------------------------------------------------------------------- ViewerLayers

ViewerLayers::ViewerLayers(edition::EditionGate& gate, QWidget* parent)
    : QWidget(parent), gate_(gate), snapshot_(std::make_shared<const QList<ViewerLayer>>()), model_(new Model(this))
{
    setObjectName(u"viewerLayers"_s);
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);

    // .pal-note: nền --c-warn-soft, biểu tượng info --c-warn
    noteBand_ = new Band(tok::c_warn_soft, false, this);
    auto* noteRow = new QHBoxLayout(noteBand_);
    noteIcon_ = new ui::Glyph(kSmallIcon, noteBand_);
    noteIcon_->set(u"info"_s, tok::c_warn);
    note_ = new QLabel(noteBand_);
    note_->setTextFormat(Qt::PlainText);
    note_->setWordWrap(true);
    noteRow->addWidget(noteIcon_, 0, Qt::AlignTop);
    noteRow->addWidget(note_, 1);
    column->addWidget(noteBand_);

    // .pal-tools: ô tìm, nút ⋯
    toolsBand_ = new Band(std::nullopt, false, this);
    auto* tools = new QHBoxLayout(toolsBand_);
    auto* search = new ui::SearchField(toolsBand_);
    search->setObjectName(u"layerSearch"_s);
    search_ = search;
    more_ = new Button({}, Button::Kind::Icon, toolsBand_);
    more_->setObjectName(u"layerMore"_s);
    more_->setIconName(u"more"_s);
    tools->addWidget(search_, 1);
    tools->addWidget(more_);
    column->addWidget(toolsBand_);

    table_ = new ui::VirtualTableView(this);
    table_->setObjectName(u"layerTable"_s);
    table_->setModel(model_);
    table_->setItemDelegate(new Delegate(*model_, table_));
    table_->header()->setStretchLastSection(false);
    table_->header()->setSectionResizeMode(Name, QHeaderView::Stretch);
    table_->header()->setSortIndicator(Name, Qt::AscendingOrder);
    table_->setSearchColumn(Name);
    model_->keepScrollPosition(table_);
    table_->installEventFilter(this);
    column->addWidget(table_, 1);

    empty_ = new ui::EmptyState(u"layers"_s, this);
    empty_->setObjectName(u"layerEmpty"_s);
    column->addWidget(empty_, 1);

    // .pal-foot: số layer đang hiện, nút Khôi phục (xuống dòng như flex-wrap của prototype ở bề rộng mặc định)
    footBand_ = new Band(std::nullopt, true, this);
    auto* foot = new QVBoxLayout(footBand_);
    count_ = new QLabel(footBand_);
    count_->setObjectName(u"layerCount"_s);
    restore_ = new Button({}, Button::Kind::Normal, footBand_);
    restore_->setObjectName(u"layerRestore"_s);
    restore_->setControlSize(Button::Size::Small);
    restore_->setIconName(u"undo"_s);
    foot->addWidget(count_);
    foot->addWidget(restore_, 0, Qt::AlignLeft);
    column->addWidget(footBand_);

    menu_ = new ui::Menu(this);
    menu_->setObjectName(u"layerMenu"_s);
    const auto add = [this](const char* id, const char* icon, const QString& command) {
        QAction* a = menu_->addAction(IconProvider::icon(QLatin1StringView(icon)), QString());
        a->setObjectName(QLatin1StringView(id));
        if (!command.isEmpty()) {
            ui::RichTooltip::attach(a, {.command = command});
            connect(a, &QAction::triggered, this, [this, command] { emit macroRequested(u"^C^C_"_s + command); });
        }
        return a;
    };
    QAction* used = menu_->addAction(QString());
    used->setObjectName(u"used"_s);
    used->setCheckable(true);
    connect(used, &QAction::toggled, this, [this](bool on) {
        usedOnly_ = on;
        refresh();
    });
    menu_->addSeparator();
    add("on", "bulb", u"LAYON"_s);
    add("thaw", "sun", u"LAYTHW"_s);
    add("iso", "isolate", u"LAYISO"_s);
    add("uniso", "unisolate", u"LAYUNISO"_s);
    menu_->addSeparator();
    connect(add("save", "save", {}), &QAction::triggered, this, &ViewerLayers::saveViewRequested);
    connect(add("restore", "undo", {}), &QAction::triggered, this, &ViewerLayers::restoreOriginal);
    connect(menu_, &QMenu::aboutToShow, this, [this, used] {
        used->setChecked(usedOnly_);
        menu_->findChild<QAction*>(u"restore"_s)->setEnabled(hasChanges());
        // Huy hiệu Viewer Pro khi bản Viewer; bấm vẫn phát saveViewRequested: ViewStateFile mở trang nâng cấp.
        QAction* save = menu_->findChild<QAction*>(u"save"_s);
        ui::RichTooltip::attach(save, {.need = gate_.locked(ui::Edition::ViewerPro) ? ui::Edition::ViewerPro
                                                                                   : ui::Edition::None});
    });
    connect(more_, &QAbstractButton::clicked, this, [this] { // mép phải menu thẳng mép phải nút (alignRight)
        menu_->popup(more_->mapToGlobal(QPoint(more_->width() - menu_->sizeHint().width(), more_->height())));
    });

    connect(search_, &QLineEdit::textChanged, this, &ViewerLayers::refresh);
    connect(table_, &QAbstractItemView::clicked, this, &ViewerLayers::activate);
    connect(restore_, &QAbstractButton::clicked, this, &ViewerLayers::restoreOriginal);
    connect(model_, &ui::BackgroundModelBase::settled, this, [this](quint64 version) {
        if (version != version_)
            return;
        updateFooter();
        emit filtered();
    });
    connect(&tm(), &theme::ThemeManager::changed, this, &ViewerLayers::applyTheme);

    setTabOrder(search_, more_);
    setTabOrder(more_, table_);
    setTabOrder(table_, restore_);
    retranslate();
    table_->setSortingEnabled(true); // sắp theo Tên: Model::sort → refresh() lần đầu (mọi widget đã có)
}

ViewerLayers::~ViewerLayers() = default;

void ViewerLayers::setLayers(QList<ViewerLayer> layers)
{
    snapshot_ = std::make_shared<const QList<ViewerLayer>>(std::move(layers));
    refresh();
}

QList<LayerFlags> ViewerLayers::overrides() const
{
    QList<LayerFlags> out;
    for (const ViewerLayer& l : *snapshot_)
        if (l.changed())
            out.append({l.name, l.on, l.frozen});
    return out;
}

bool ViewerLayers::hasChanges() const
{
    return std::any_of(snapshot_->cbegin(), snapshot_->cend(), [](const ViewerLayer& l) { return l.changed(); });
}

void ViewerLayers::restoreOriginal()
{
    LayerMacro m; // chỉ phần khác tệp: tắt bằng LAYISO 4.999 layer thì không kèm 4.999 tên rã đông
    for (const ViewerLayer& l : *snapshot_) {
        if (l.on != l.fileOn)
            (l.fileOn ? m.on : m.off) << l.name;
        if (l.frozen != l.fileFrozen)
            (l.fileFrozen ? m.freeze : m.thaw) << l.name;
    }
    if (const QString text = m.text(); !text.isEmpty())
        emit macroRequested(text);
}

QLineEdit* ViewerLayers::searchField() const
{
    return search_;
}

int ViewerLayers::shownCount() const
{
    return model_->rowCount();
}

void ViewerLayers::refresh()
{
    const QString query = search_->text().trimmed();
    const QString needle = text::foldForSearch(query);
    std::optional<QRegularExpression> pattern; // * ? : khớp cả tên đã gấp dấu (như wcmatch)
    if (query.contains(u'*') || query.contains(u'?'))
        pattern = QRegularExpression::fromWildcard(needle, Qt::CaseInsensitive);
    model_->refresh(snapshot_, ++version_,
                    [needle, pattern, usedOnly = usedOnly_, column = sortColumn_, order = sortOrder_](
                        const QList<ViewerLayer>& layers, const sched::CancelToken& cancel) {
                        std::vector<LayerRow> rows;
                        rows.reserve(std::size_t(layers.size()));
                        for (qsizetype i = 0; i < layers.size(); ++i) {
                            if ((i & 511) == 0 && cancel.cancelled())
                                return rows;
                            const ViewerLayer& l = layers[i];
                            if (usedOnly && l.objects == 0)
                                continue;
                            QString folded = text::foldForSearch(l.name);
                            if (pattern ? !pattern->match(folded).hasMatch() : !folded.contains(needle))
                                continue;
                            rows.push_back({std::uint64_t(i), std::move(folded), l});
                        }
                        std::sort(rows.begin(), rows.end(), [&](const LayerRow& a, const LayerRow& b) {
                            return order == Qt::AscendingOrder ? before(a, b, column) : before(b, a, column);
                        });
                        return rows;
                    });
    updateFooter();
}

void ViewerLayers::activate(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    const ViewerLayer& l = model_->row(index.row()).layer;
    switch (index.column()) {
    case On: {
        LayerMacro m;
        (l.on ? m.off : m.on) << l.name;
        emit macroRequested(m.text());
        break;
    }
    case Freeze: {
        LayerMacro m;
        (l.frozen ? m.thaw : m.freeze) << l.name;
        emit macroRequested(m.text());
        break;
    }
    case Lock: // Viewer không sửa layer (07.1 §2): trang nâng cấp như prototype TC.screens.upgrade
        gate_.upgrade().request({u"layer-lock"_s, ui::Edition::Pro, tr("Khóa layer, đổi thuộc tính layer")});
        break;
    case Color:
        gate_.upgrade().request(
            {u"layer-props"_s, ui::Edition::Pro, tr("Đổi màu, kiểu đường, độ dày nét của layer")});
        break;
    default:
        break;
    }
}

bool ViewerLayers::eventFilter(QObject* watched, QEvent* e)
{
    if (watched == table_ && e->type() == QEvent::KeyPress) {
        const auto* k = static_cast<QKeyEvent*>(e);
        const QModelIndex current = table_->currentIndex();
        const bool press = k->key() == Qt::Key_Space || k->key() == Qt::Key_Return || k->key() == Qt::Key_Enter;
        if (press && k->modifiers() == Qt::NoModifier && current.isValid() && current.column() >= On &&
            current.column() <= Color) {
            activate(current);
            return true;
        }
    }
    return QWidget::eventFilter(watched, e);
}

void ViewerLayers::updateFooter()
{
    const bool any = !snapshot_->isEmpty();
    table_->setVisible(any);
    empty_->setVisible(!any);
    count_->setText(tr("Hiện %1/%2 layer").arg(i18n::formatCount(model_->rowCount()),
                                               i18n::formatCount(snapshot_->size())));
    restore_->setVisible(hasChanges());
}

void ViewerLayers::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void ViewerLayers::showEvent(QShowEvent* e)
{
    applyTheme(); // ensurePolished đã chạy cho cả widget con: font đầu cột của TrionStyle đã đúng
    QWidget::showEvent(e);
}

void ViewerLayers::retranslate()
{
    note_->setText(tr("Bật/tắt và đóng băng là tạm thời. Thay đổi không lưu vào bản vẽ."));
    search_->setPlaceholderText(tr("Tìm layer, ví dụ KT-*"));
    search_->setAccessibleName(tr("Tìm layer"));
    more_->setAccessibleName(tr("Thao tác khác"));
    more_->setToolTip(tr("Thao tác khác"));
    table_->setAccessibleName(tr("Danh sách layer"));
    restore_->setText(tr("Khôi phục như tệp gốc"));
    empty_->setText(tr("Chưa có bản vẽ"), tr("Mở bản vẽ để xem và bật, tắt layer."));
    const auto text = [this](const char* id, const QString& s) {
        menu_->findChild<QAction*>(QLatin1StringView(id))->setText(s);
    };
    text("used", tr("Chỉ hiện layer đang dùng"));
    text("on", tr("Bật tất cả"));
    text("thaw", tr("Rã đông tất cả"));
    text("iso", tr("Cô lập layer của đối tượng…"));
    text("uniso", tr("Bỏ cô lập"));
    text("save", tr("Lưu trạng thái hiển thị"));
    text("restore", tr("Khôi phục như tệp gốc"));
    model_->retranslate();
    applyTheme(); // cột Số theo chữ đầu cột
    updateFooter();
}

void ViewerLayers::applyTheme()
{
    const int s1 = px(tok::sp_1), s3 = px(tok::sp_3), s4 = px(tok::sp_4);
    noteBand_->layout()->setContentsMargins(s4 + s1, s3, s4 + s1, s3); // .pal-note padding 6px 10px
    noteBand_->layout()->setSpacing(s3);
    toolsBand_->layout()->setContentsMargins(s3, s3, s3, s3); // .pal-tools padding 6px, gap 3px
    toolsBand_->layout()->setSpacing(s1);
    footBand_->layout()->setContentsMargins(s4 + s1, s3, s4 + s1, s3); // .pal-foot padding 6px 10px
    footBand_->layout()->setSpacing(s3);
    note_->setFont(theme::font(TextRole::Caption));
    setInk(note_, tok::c_text);
    count_->setFont(theme::font(TextRole::Caption));
    setInk(count_, tok::c_text_3);
    for (int c : {Status, On, Freeze, Lock, Color})
        table_->header()->resizeSection(c, Delegate::iconColumn());
    // .tbl td.num: đủ cho "99.999" và đầu cột "Số"
    table_->header()->resizeSection(
        Count, std::max(QFontMetrics(theme::font(TextRole::MonoLabel)).horizontalAdvance(u"99.999"_s),
                        table_->header()->sectionSizeHint(Count)) + 2 * px(tok::sp_4));
}

// ------------------------------------------------------------------------------------------------- ViewStateFile

ViewStateFile::ViewStateFile(edition::EditionGate& gate, ui::CanvasBanner& banner, QObject* parent)
    : QObject(parent), gate_(gate), banner_(&banner)
{
}

void ViewStateFile::load(const QString& drawing)
{
    if (gate_.locked(ui::Edition::ViewerPro))
        return;
    sched::submit(sched::Priority::Normal, [rx = ui::Receiver(this), this, drawing] {
        Expected<ViewState> state = readViewState(drawing); // nền: không chạm this
        ui::post(rx, [this, drawing, state = std::move(state)] {
            if (state) {
                emit loaded(drawing, *state);
                return;
            }
            const std::string& code = state.error().code;
            if (code == "missing" || !banner_)
                return;
            const QString file = QFileInfo(viewStatePath(drawing)).fileName();
            banner_->add(kBanner.toString(), ui::Severity::Warning,
                         code == "too-large"
                             ? tr("Bỏ qua trạng thái hiển thị đã lưu: %1 lớn hơn %2.")
                                   .arg(file, i18n::formatBytes(ViewState::kMaxBytes))
                             : tr("Bỏ qua trạng thái hiển thị đã lưu: %1 hỏng hoặc không đọc được.").arg(file));
        });
    });
}

bool ViewStateFile::save(const QString& drawing, const ViewState& state)
{
    if (gate_.locked(ui::Edition::ViewerPro)) {
        gate_.upgrade().request({u"viewstate"_s, ui::Edition::ViewerPro, tr("Lưu trạng thái hiển thị")});
        return false;
    }
    sched::submit(sched::Priority::Normal, [rx = ui::Receiver(this), this, drawing, state] {
        const Expected<void> written = writeViewState(drawing, state);
        const std::string code = written ? std::string() : written.error().code;
        ui::post(rx, [this, drawing, code] {
            const QString file = QFileInfo(viewStatePath(drawing)).fileName();
            emit saved(drawing, code.empty()        ? QString()
                                : code == "too-large" ? tr("Không lưu được trạng thái hiển thị: lớn hơn %1.")
                                                            .arg(i18n::formatBytes(ViewState::kMaxBytes))
                                                      : tr("Không lưu được trạng thái hiển thị vào %1.").arg(file));
        });
    });
    return true;
}

} // namespace trion::shell
