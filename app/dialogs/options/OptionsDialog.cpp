#include "app/dialogs/options/OptionsDialog.h"

#include "app/core/a11y/Announce.h"
#include "app/core/actions/ActionRegistry.h"
#include "app/core/edition/EditionGate.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/controls/Paint.h"
#include "app/widgets/dialog/ConfirmDialog.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/assert.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTimer>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace trion::options {

namespace {

using theme::TextRole;

// Giá trị của prototype không có token riêng (SC.options, css/app.css .vtabs .frow .stack .pal-tools).
constexpr int kWidth = 900;       // TC.dialog w:900
constexpr int kHeight = 600;      // h:600
constexpr int kCardBorder = 1;    // viền thẻ của DialogFrame, nằm trong contentsMargins
constexpr int kTabsWidth = 168;   // .vtabs cột danh sách tab (rộng hơn khi chữ cần)
constexpr int kTabIcon = 12;      // .ic.s12: biểu tượng tab, ổ khóa, biểu tượng bản vẽ của hàng
constexpr int kTabGap = 1;        // .vt-list gap
constexpr int kLabelWidth = 240;  // --lab-w của hàng thiết lập
constexpr int kSearchWidth = 280; // label.search
constexpr int kBodyPadV = 14;     // .vt-body padding 14px 16px (16 = --sp-6)
constexpr int kStackGap = 10;     // .stack gap
constexpr int kColumnGap = 10;    // .frow gap ngang (nhãn | điều khiển)
constexpr int kRowPadV = 5;       // hàng: padding 5px 6px (6 = --sp-3)
constexpr int kLine = 1;          // nét chia --c-border
constexpr int kLockedRole = Qt::UserRole + 1;

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

void styleLabel(QLabel* l) // .frow .lab: --fs-s, --c-text-2
{
    l->setFont(theme::font(TextRole::Label));
    setInk(l, tok::c_text_2);
}

void styleGroup(QLabel* l) // .gh: --fs-xs đậm, chữ hoa, giãn .06em, --c-text-3
{
    QFont f = theme::font(TextRole::Caption, theme::Weight::Bold);
    f.setCapitalization(QFont::AllUppercase);
    f.setLetterSpacing(QFont::PercentageSpacing, 106);
    l->setFont(f);
    setInk(l, tok::c_text_3);
}

QString tabName(Tab tab)
{
    return QCoreApplication::translate("options", tabInfo(tab).name);
}

// Thân hộp thoại: nét dưới hàng đầu, nền --c-sunken và nét phải của danh sách tab.
class Body final : public QWidget {
public:
    QWidget* head = nullptr;
    QWidget* list = nullptr;

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const QRect h = head->geometry();
        p.fillRect(QRect(0, h.bottom(), width(), kLine), col(tok::c_border)); // .pal-tools border-bottom
        const QRect l = list->geometry();
        p.fillRect(l, col(tok::c_sunken));
        p.fillRect(QRect(l.right() + 1, l.top(), kLine, l.height()), col(tok::c_border)); // .vt-list border-right
    }
};

// Mục tab (.vtabs .vt): cao --h-tab, lề --sp-4, biểu tượng 12 px, chữ --fs; rê --c-hover; đang mở --c-accent-soft với
// vạch --c-accent 2 px bên trái; ổ khóa cuối mục khi tab bị cổng; viền focus quanh mục hiện tại khi đi bằng bàn phím.
class TabDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex& index) const override
    {
        const QFontMetrics fm(theme::font(TextRole::Ui));
        int w = 2 * px(tok::sp_3) + 2 * px(tok::sp_4) + kTabIcon + px(tok::sp_3) +
                fm.horizontalAdvance(index.data().toString());
        if (index.data(kLockedRole).toBool())
            w += px(tok::sp_3) + kTabIcon;
        return {w, px(tok::h_tab) + kTabGap};
    }

    void paint(QPainter* p, const QStyleOptionViewItem& o, const QModelIndex& index) const override
    {
        const auto* view = qobject_cast<const QAbstractItemView*>(o.widget);
        const QRect r = o.rect.adjusted(px(tok::sp_3), 0, -px(tok::sp_3), -kTabGap);
        const bool on = o.state & QStyle::State_Selected;
        const bool hover = o.state & QStyle::State_MouseOver;
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        if (on || hover) {
            const qreal radius = tm().px(tok::r);
            p->setPen(Qt::NoPen);
            p->setBrush(col(on ? tok::c_accent_soft : tok::c_hover));
            p->drawRoundedRect(QRectF(r), radius, radius);
        }
        if (on)
            p->fillRect(QRect(r.left(), r.top(), px(tok::sp_1), r.height()), col(tok::c_accent)); // inset 2px
        const qreal dpr = p->device()->devicePixelRatioF();
        const theme::IconState state = on || hover ? theme::IconState::Hover : theme::IconState::Normal;
        theme::IconProvider& icons = theme::IconProvider::instance();
        const int top = r.top() + (r.height() - kTabIcon) / 2;
        int x = r.left() + px(tok::sp_4);
        p->drawPixmap(QRect(x, top, kTabIcon, kTabIcon),
                      icons.pixmap(QString::fromLatin1(tabInfo(Tab(index.row())).icon), kTabIcon, dpr, state));
        x += kTabIcon + px(tok::sp_3);
        int right = r.right() + 1 - px(tok::sp_4);
        if (index.data(kLockedRole).toBool()) {
            right -= kTabIcon;
            p->drawPixmap(QRect(right, top, kTabIcon, kTabIcon), icons.pixmap(u"lock"_s, kTabIcon, dpr, state));
            right -= px(tok::sp_3);
        }
        p->setFont(theme::font(TextRole::Ui));
        p->setPen(col(on || hover ? tok::c_text : tok::c_text_2));
        p->drawText(QRect(x, r.top(), right - x, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                    index.data().toString());
        if (view && view->hasFocus() && index == view->currentIndex() && ui::paint::focusVisible(view))
            ui::paint::drawRing(*p, r, col(tok::c_focus));
        p->restore();
    }
};

// Hàng thiết lập: tô --c-accent-soft khi là kết quả tìm.
class Row final : public QWidget {
public:
    bool lit = false;

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (!lit)
            return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(col(tok::c_accent_soft));
        const qreal radius = tm().px(tok::r);
        p.drawRoundedRect(QRectF(rect()), radius, radius);
    }
};

// Biểu tượng "lưu trong bản vẽ" cạnh nhãn hàng [đề xuất].
class DrawingMark final : public QWidget {
public:
    DrawingMark() { setFixedSize(kTabIcon, kTabIcon); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.drawPixmap(rect(), theme::IconProvider::instance().pixmap(u"file"_s, kTabIcon, devicePixelRatioF(),
                                                                    theme::IconState::Normal));
    }
};

} // namespace

// Danh sách tab dọc (.vt-list): một điểm dừng Tab, ↑↓ chuyển tab; rộng theo mục dài nhất, ít nhất 168 px.
class TabList final : public QListView {
public:
    explicit TabList(QWidget* parent) : QListView(parent), model_(new QStandardItemModel(this))
    {
        for (int i = 0; i < kTabCount; ++i) {
            auto* item = new QStandardItem;
            item->setEditable(false);
            model_->appendRow(item);
        }
        setModel(model_); // model là con của view: hủy sau view
        setItemDelegate(new TabDelegate(this));
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setUniformItemSizes(true);
        setMouseTracking(true);
        viewport()->setAttribute(Qt::WA_Hover);
        viewport()->setAutoFillBackground(false); // nền --c-sunken do Body vẽ
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        restyle();
    }

    void restyle()
    {
        setViewportMargins(0, px(tok::sp_3), 0, px(tok::sp_3)); // .vt-list padding 6px (ngang: lề trong delegate)
        updateGeometry();
    }

    QSize sizeHint() const override
    {
        return {std::max(kTabsWidth, sizeHintForColumn(0)), QListView::sizeHint().height()};
    }
    QSize minimumSizeHint() const override { return {sizeHint().width(), 0}; }

    QStandardItem* item(int row) const { return model_->item(row); }
    int currentRow() const { return currentIndex().row(); }
    void setCurrentRow(int row) { setCurrentIndex(model_->index(row, 0)); }

private:
    QStandardItemModel* model_;
};

// -------------------------------------------------------------------------------- OptionsDraft

OptionsDraft::OptionsDraft(settings::Store& store, QObject* parent) : QObject(parent), store_(store) {}

OptionsDraft::~OptionsDraft()
{
    if (!original_.isEmpty())
        revert();
}

void OptionsDraft::declare(const QString& key, const QJsonValue& def, Preview preview)
{
    Key& k = keys_[key];
    k.def = def;
    k.live = k.live || preview == Preview::Live;
}

QJsonValue OptionsDraft::value(const QString& key) const
{
    const auto it = pending_.constFind(key);
    return it != pending_.cend() ? *it : store_.value(key);
}

void OptionsDraft::setValue(const QString& key, const QJsonValue& v)
{
    const auto old = original_.constFind(key);
    const QJsonValue base = old != original_.cend() ? *old : store_.value(key);
    if (v == base)
        pending_.remove(key);
    else
        pending_.insert(key, v);
    if (keys_.value(key).live) { // xem trước: ghi Store ngay, nhớ giá trị trước khi mở để Hủy trả lại
        if (!original_.contains(key))
            original_.insert(key, store_.value(key));
        write(key, v);
        if (v == original_.value(key))
            original_.remove(key);
    }
    emit changed();
}

void OptionsDraft::setDrawing(const QString& name, actions::Sysvars* sysvars)
{
    drawing_ = name;
    vars_ = sysvars;
    sysvars_.clear();
    emit changed();
}

QString OptionsDraft::sysvar(const QString& name) const
{
    if (!hasDrawing())
        return {};
    const QString n = name.toUpper();
    const auto it = sysvars_.constFind(n);
    if (it != sysvars_.cend())
        return *it;
    return vars_ ? QString::number(vars_->get(n)) : QString();
}

void OptionsDraft::setSysvar(const QString& name, const QString& value)
{
    TRION_ASSERT(!name.isEmpty() && !value.isEmpty() && !value.contains(u' ') && !name.contains(u' '));
    if (!hasDrawing())
        return;
    const QString n = name.toUpper();
    if (vars_ && value == QString::number(vars_->get(n)))
        sysvars_.remove(n);
    else
        sysvars_.insert(n, value);
    emit changed();
}

int OptionsDraft::resetCount() const
{
    int n = 0;
    for (auto it = keys_.cbegin(); it != keys_.cend(); ++it) {
        const QJsonValue v = value(it.key());
        n += !v.isUndefined() && v != it->def;
    }
    return n;
}

void OptionsDraft::resetDefaults()
{
    for (const QString& key : keys_.keys())
        if (!value(key).isUndefined())
            setValue(key, QJsonValue(QJsonValue::Undefined));
}

void OptionsDraft::apply()
{
    // Một lượt, không quay vòng sự kiện: Store gom mọi lần đặt thành một lần ghi đĩa (QSaveFile: tệp cũ hẳn hoặc mới
    // hẳn). ponytail: nguyên tử nhờ không ai quay vòng sự kiện trong lúc nghe Store::changed; NT-010 nên có đặt theo
    // lô.
    const QMap<QString, QJsonValue> pending = std::exchange(pending_, {});
    original_.clear();
    for (auto it = pending.cbegin(); it != pending.cend(); ++it)
        write(it.key(), it.value());
    if (!sysvars_.isEmpty())
        emit macroRequested(setvarMacro(std::exchange(sysvars_, {})));
    emit changed();
}

void OptionsDraft::revert()
{
    const QMap<QString, QJsonValue> original = std::exchange(original_, {});
    pending_.clear();
    sysvars_.clear();
    for (auto it = original.cbegin(); it != original.cend(); ++it)
        write(it.key(), it.value());
    emit changed();
}

QString OptionsDraft::setvarMacro(const QMap<QString, QString>& values)
{
    QStringList parts{u"^C^C_UNDO _BEGIN"_s};
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        parts << u"_SETVAR %1 %2"_s.arg(it.key(), it.value());
    parts << u"_UNDO _END"_s;
    return parts.join(u' ');
}

void OptionsDraft::write(const QString& key, const QJsonValue& v)
{
    if (v.isUndefined())
        store_.remove(key);
    else
        store_.setValue(key, v);
}

// -------------------------------------------------------------------------------- OptionsPage

OptionsPage::OptionsPage(Tab tab, OptionsDraft& draft, QWidget* parent)
    : QWidget(parent), tab_(tab), draft_(draft), column_(new QVBoxLayout(this))
{
    column_->setContentsMargins({});
    column_->setSpacing(kStackGap);
    column_->addStretch(1); // nhóm, hàng chèn trước khoảng co giãn này
    connect(&tm(), &theme::ThemeManager::changed, this, &OptionsPage::restyle);
}

void OptionsPage::addGroup(const QString& title)
{
    if (!rows_.empty() || !groups_.empty())
        column_->insertSpacing(column_->count() - 1, px(tok::sp_4)); // .gh margin-top 8px
    auto* l = new QLabel(title);
    l->setTextFormat(Qt::PlainText);
    l->setWordWrap(true);
    styleGroup(l);
    column_->insertWidget(column_->count() - 1, l);
    groups_.push_back(l);
}

QWidget* OptionsPage::addRow(const char* id, const QString& label, std::initializer_list<QWidget*> controls,
                             Where where)
{
    TRION_ASSERT(id && *id);
    auto* row = new Row;
    row->setObjectName(QString::fromLatin1(id));
    auto* line = new QHBoxLayout(row);
    line->setContentsMargins(px(tok::sp_3), kRowPadV, px(tok::sp_3), kRowPadV);
    line->setSpacing(px(tok::sp_4)); // .hrow gap 8px giữa các điều khiển
    // Nhãn là con trực tiếp của hàng, cùng cha với điều khiển: Qt lấy nhãn buddy làm tên trợ năng của điều khiển không
    // có chữ (ô nhập, danh sách thả, thanh trượt). Nhãn chữ thường: '&' không là phím tắt.
    auto* text = new QLabel(QString(label).replace(u'&', u"&&"_s));
    text->setTextFormat(Qt::PlainText);
    text->setWordWrap(true);
    styleLabel(text);
    line->addWidget(text);
    int labelWidth = kLabelWidth;
    if (where == Where::Drawing) {
        auto* mark = new DrawingMark;
        mark->setToolTip(tr("Lưu trong bản vẽ"));
        labelWidth -= kTabIcon + px(tok::sp_4);
        line->addWidget(mark);
        for (QWidget* w : controls) {
            if (w->accessibleDescription().isEmpty())
                w->setAccessibleDescription(tr("Lưu trong bản vẽ"));
            w->setEnabled(draft_.hasDrawing());
        }
        if (!draft_.hasDrawing())
            row->setToolTip(tr("Không có bản vẽ đang mở"));
    }
    text->setFixedWidth(labelWidth); // cột nhãn 240 px (gồm biểu tượng bản vẽ); chữ dài xuống dòng
    line->addSpacing(kColumnGap - px(tok::sp_4));
    for (QWidget* w : controls)
        line->addWidget(w);
    line->addStretch(1);
    if (controls.size() > 0)
        text->setBuddy(*controls.begin());
    column_->insertWidget(column_->count() - 1, row);
    rows_.push_back({QString::fromLatin1(id), label, row, text});
    return row;
}

QWidget* OptionsPage::row(QStringView id) const
{
    for (const Entry& e : rows_)
        if (e.id == id)
            return e.row;
    return nullptr;
}

QStringList OptionsPage::rowIds() const
{
    QStringList out;
    for (const Entry& e : rows_)
        out << e.id;
    return out;
}

QString OptionsPage::rowLabel(QStringView id) const
{
    for (const Entry& e : rows_)
        if (e.id == id)
            return e.text;
    return {};
}

QString OptionsPage::rowOf(const QWidget* w) const
{
    for (; w && w != this; w = w->parentWidget())
        for (const Entry& e : rows_)
            if (e.row == w)
                return e.id;
    return {};
}

void OptionsPage::setHighlighted(QStringView id)
{
    lit_ = id.toString();
    for (const Entry& e : rows_) {
        auto* r = static_cast<Row*>(e.row);
        if (std::exchange(r->lit, e.id == id) != r->lit)
            r->update();
    }
}

void OptionsPage::restyle()
{
    for (const Entry& e : rows_)
        styleLabel(e.label);
    for (QLabel* g : groups_)
        styleGroup(g);
}

// -------------------------------------------------------------------------------- OptionsDialog

OptionsDialog::OptionsDialog(edition::EditionGate& gate, settings::Store& store, QWidget* parent)
    : DialogFrame(u"options"_s, parent), gate_(gate), draft_(new OptionsDraft(store, this))
{
    setModal(true);
    setCommand(u"OPTIONS"_s);

    auto* body = new Body;
    head_ = new QWidget(body);
    context_ = new QLabel(head_);
    context_->setTextFormat(Qt::RichText);
    context_->setWordWrap(true);
    noMatch_ = new QLabel(head_);
    noMatch_->setTextFormat(Qt::PlainText);
    noMatch_->hide();
    search_ = new ui::SearchField(head_);
    search_->setFixedWidth(kSearchWidth);
    auto* top = new QHBoxLayout(head_);
    top->addWidget(context_, 1);
    top->addWidget(noMatch_);
    top->addWidget(search_);
    tabs_ = new TabList(body);
    stack_ = new QStackedWidget(body);
    auto* split = new QHBoxLayout;
    split->setContentsMargins({});
    split->setSpacing(kLine); // nét phải của danh sách tab
    split->addWidget(tabs_);
    split->addWidget(stack_, 1);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins({});
    column->setSpacing(0);
    column->addWidget(head_);
    column->addLayout(split, 1);
    body->head = head_;
    body->list = tabs_;
    setBody(body, true);

    reset_ = new ui::Button({}, ui::Button::Kind::Ghost);
    footer()->insertWidget(0, reset_); // trái, trước khoảng co giãn
    addCancel();
    apply_ = new ui::Button;
    apply_->setEnabled(false);
    footer()->addWidget(apply_);

    connect(search_, &QLineEdit::textChanged, this, &OptionsDialog::find);
    connect(tabs_->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this](const QModelIndex& i) {
        if (i.isValid())
            showTab(Tab(i.row()));
    });
    connect(reset_, &QPushButton::clicked, this, &OptionsDialog::resetDefaults);
    connect(apply_, &QPushButton::clicked, draft_, &OptionsDraft::apply);
    connect(draft_, &OptionsDraft::changed, this, [this] { apply_->setEnabled(draft_->isDirty()); });
    connect(draft_, &OptionsDraft::macroRequested, this, &OptionsDialog::macroRequested);
    connect(&gate_, &edition::EditionGate::editionChanged, this, &OptionsDialog::updateLocks);
    connect(&tm(), &theme::ThemeManager::changed, this, &OptionsDialog::restyle);
    prebuild_ = new QTimer(this);
    prebuild_->setSingleShot(true);
    prebuild_->setInterval(0);
    connect(prebuild_, &QTimer::timeout, this, &OptionsDialog::prebuildNext);

    // Trang của từng tab: UID-020 (Hiển thị), UID-021 (Hệ thống, Dòng lệnh, Hồ sơ, Tài khoản), UID-022 (Tệp, Mở & Lưu,
    // In & Xuất), UID-023 (Người dùng, Vẽ), UID-024 (Chọn), UID-026 (Mô hình 3D) đăng ký ở đây:
    //   addPage(Tab::Display, &buildDisplayPage);

    restyle();
    retranslate();
    rebuild(); // dựng tab hiện tại
}

OptionsDialog::~OptionsDialog()
{
    draft_->revert(); // khi đối tượng còn nguyên: bộ nghe Store (ThemeManager → khung này) còn hợp lệ
}

void OptionsDialog::addPage(Tab tab, PageFactory build)
{
    factories_[int(tab)] = std::move(build);
    Content& c = content_[int(tab)];
    if (c.scroll) {
        stack_->removeWidget(c.scroll);
        c.scroll->hide();
        c.scroll->deleteLater();
        c = {};
    }
    if (tab == current_)
        showTab(tab);
    else if (painted_)
        prebuild_->start();
}

void OptionsDialog::open(Tab tab, const QString& row)
{
    if (!isVisible())
        DialogFrame::open();
    clearHighlight();
    if (row.isEmpty())
        setCurrentTab(tab);
    else
        showRow(tab, row);
}

void OptionsDialog::setCurrentTab(Tab tab)
{
    if (tabs_->currentRow() == int(tab))
        showTab(tab);
    else
        tabs_->setCurrentRow(int(tab)); // → showTab
}

bool OptionsDialog::isTabLocked(Tab tab) const
{
    return gate_.locked(tabInfo(tab).need);
}

void OptionsDialog::setProfile(const QString& name)
{
    profile_ = name;
    updateContext();
}

void OptionsDialog::setDrawing(const QString& name, actions::Sysvars* sysvars)
{
    draft_->setDrawing(name, sysvars);
    updateContext();
    rebuild(); // hàng lưu trong bản vẽ bật/tắt theo bản vẽ
}

OptionsPage* OptionsDialog::page(Tab tab) const
{
    return qobject_cast<OptionsPage*>(content_[int(tab)].inner.data());
}

QWidget* OptionsDialog::tabContent(Tab tab) const
{
    return content_[int(tab)].inner;
}

QString OptionsDialog::highlightedRow() const
{
    const OptionsPage* p = page(current_);
    return p ? p->highlighted() : QString();
}

QStringList OptionsDialog::indexIssues()
{
    QStringList out;
    for (int i = 0; i < kTabCount; ++i) {
        if (!factories_[i])
            continue;
        const Tab tab = Tab(i);
        const QString tabId = QString::fromLatin1(tabInfo(tab).id);
        OptionsPage page(tab, *draft_);
        factories_[i](page);
        QStringList seen;
        for (const QString& id : page.rowIds()) {
            if (seen.contains(id))
                out << u"%1/%2: id hàng trùng"_s.arg(tabId, id);
            seen << id;
            const SettingEntry* e = SettingsIndex::find(id);
            if (!e)
                out << u"%1/%2: hàng không có mục trong SettingsIndex"_s.arg(tabId, id);
            else if (e->tab != tab)
                out << u"%1/%2: mục chỉ mục thuộc tab %3"_s.arg(tabId, id, QString::fromLatin1(tabInfo(e->tab).id));
        }
        for (const SettingEntry& e : SettingsIndex::entries())
            if (e.tab == tab && !seen.contains(QLatin1StringView(e.id)))
                out << u"%1/%2: mục chỉ mục không có hàng trên trang"_s.arg(tabId, QLatin1StringView(e.id));
    }
    return out;
}

void OptionsDialog::accept()
{
    draft_->apply();
    DialogFrame::accept();
}

void OptionsDialog::reject()
{
    draft_->revert();
    DialogFrame::reject();
}

void OptionsDialog::keyPressEvent(QKeyEvent* e)
{
    if ((e->modifiers() & Qt::ControlModifier) && (e->key() == Qt::Key_Tab || e->key() == Qt::Key_Backtab)) {
        const bool back = e->key() == Qt::Key_Backtab || (e->modifiers() & Qt::ShiftModifier);
        const bool inside = stack_->isAncestorOf(QApplication::focusWidget());
        setCurrentTab(Tab((int(current_) + (back ? kTabCount - 1 : 1)) % kTabCount));
        if (inside) // focus đang ở trang cũ (vừa ẩn): về danh sách tab, như dải tab của hộp thoại hệ thống
            tabs_->setFocus(Qt::TabFocusReason);
        return;
    }
    DialogFrame::keyPressEvent(e);
}

void OptionsDialog::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange) {
        retranslate();
        rebuild(); // trang dịch lại bằng cách dựng lại từ nháp
    }
    DialogFrame::changeEvent(e);
}

QSize OptionsDialog::sizeHint() const
{
    const QMargins m = contentsMargins();
    const QSize card(kWidth - 2 * kCardBorder + m.left() + m.right(), kHeight - 2 * kCardBorder + m.top() + m.bottom());
    return card.expandedTo(minimumSizeHint());
}

void OptionsDialog::retranslate()
{
    setWindowTitle(tr("Tùy chọn"));
    setConfirm(tr("Lưu và đóng"));
    reset_->setText(tr("Đặt lại mặc định"));
    apply_->setText(tr("Áp dụng"));
    search_->setPlaceholderText(tr("Tìm thiết lập, ví dụ: autosave, chủ đề"));
    search_->setAccessibleName(tr("Tìm thiết lập"));
    noMatch_->setText(tr("Không thấy thiết lập"));
    tabs_->setAccessibleName(tr("Các tab tùy chọn"));
    syncTabs();
    updateContext();
}

void OptionsDialog::restyle()
{
    auto* top = static_cast<QHBoxLayout*>(head_->layout());
    top->setContentsMargins(px(tok::sp_3), px(tok::sp_3), px(tok::sp_3), px(tok::sp_3) + kLine); // .pal-tools 6px
    top->setSpacing(px(tok::sp_4));
    context_->setFont(theme::font(TextRole::Label)); // .muted --fs-s
    setInk(context_, tok::c_text_2);
    noMatch_->setFont(theme::font(TextRole::Label));
    setInk(noMatch_, tok::c_text_3);
    tabs_->restyle();
}

void OptionsDialog::paintEvent(QPaintEvent* e)
{
    DialogFrame::paintEvent(e);
    if (!std::exchange(painted_, true))
        prebuild_->start(); // sau khung đầu: không làm chậm lúc mở
}

void OptionsDialog::showTab(Tab tab)
{
    current_ = tab;
    stack_->setCurrentWidget(ensureContent(tab).scroll);
    if (tabs_->currentRow() != int(tab))
        tabs_->setCurrentRow(int(tab)); // → showTab lần nữa: nội dung đã có, chỉ đồng bộ
}

void OptionsDialog::prebuildNext()
{
    // Tìm, Ctrl+Tab, nhấp tab tới tab đã dựng sẵn thì chỉ còn vẽ (tiêu chí 1: ≤ 16 ms mỗi phím). Mỗi lượt một tab để
    // luồng UI không bận liền một mạch; xong hết thì dừng.
    for (int i = 0; i < kTabCount; ++i)
        if (!content_[i].scroll) {
            ensureContent(Tab(i)).scroll->widget()->ensurePolished();
            prebuild_->start();
            return;
        }
}

OptionsDialog::Content& OptionsDialog::ensureContent(Tab tab)
{
    Content& c = content_[int(tab)];
    if (!c.scroll) {
        c.inner = buildContent(tab);
        auto* wrap = new QWidget;
        auto* v = new QVBoxLayout(wrap);
        v->setContentsMargins(px(tok::sp_6), kBodyPadV, px(tok::sp_6), kBodyPadV); // .vt-body padding
        v->addWidget(c.inner);
        v->addStretch(1);
        c.scroll = new QScrollArea;
        c.scroll->setFrameShape(QFrame::NoFrame);
        c.scroll->setFocusPolicy(Qt::NoFocus); // điểm dừng Tab là điều khiển trong trang, cuộn theo focus
        c.scroll->setWidgetResizable(true);
        c.scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        c.scroll->viewport()->setAutoFillBackground(false);
        c.scroll->setWidget(wrap);
        wrap->setAutoFillBackground(false); // setWidget bật lại; nền là thẻ --c-base
        stack_->addWidget(c.scroll);
        // Không cuộn ngang: vùng nội dung rộng ít nhất bằng nội dung (vi-XL), hộp thoại nở theo.
        const int need = wrap->minimumSizeHint().width() + c.scroll->verticalScrollBar()->sizeHint().width();
        stack_->setMinimumWidth(std::max(stack_->minimumWidth(), need));
        // Thứ tự Tab: ô tìm → danh sách tab → nội dung tab → chân (nội dung dựng sau chân nên xếp lại).
        QWidget* prev = tabs_;
        for (QWidget* w : wrap->findChildren<QWidget*>())
            if ((w->focusPolicy() & Qt::TabFocus) && w->window() == this) { // bỏ popup của danh sách thả
                setTabOrder(prev, w);
                prev = w;
            }
    }
    return c;
}

QWidget* OptionsDialog::buildContent(Tab tab)
{
    if (isTabLocked(tab)) {
        QStringList viewer;
        for (int i = 0; i < kTabCount; ++i)
            if (!isTabLocked(Tab(i)))
                viewer << tabName(Tab(i));
        auto* s = new ui::EmptyState(u"lock"_s);
        s->setText(
            tr("Tab “%1” có trong TrionCAD %2").arg(tabName(tab), ui::RichTooltip::editionName(tabInfo(tab).need)),
            tr("Viewer có các tab %1.").arg(viewer.join(u", "_s)));
        s->setAction(tr("Dùng thử Pro 30 ngày"));
        connect(s, &ui::EmptyState::actionTriggered, this, [this, tab] {
            gate_.upgrade().open({.feature = u"OPTIONS"_s, .need = tabInfo(tab).need, .name = tr("Tùy chọn đầy đủ")});
        });
        return s;
    }
    if (const PageFactory& build = factories_[int(tab)]) {
        auto* p = new OptionsPage(tab, *draft_);
        build(*p);
        return p;
    }
    auto* s = new ui::EmptyState(u"info"_s);
    s->setText(tr("Tab “%1” chưa có thiết lập").arg(tabName(tab)),
               tr("Các thiết lập của tab này có ở bản cập nhật sau."));
    return s;
}

void OptionsDialog::rebuild()
{
    QString focusRow;
    if (const OptionsPage* p = page(current_))
        focusRow = p->rowOf(QApplication::focusWidget());
    const QString lit = highlightedRow();
    for (Content& c : content_)
        if (c.scroll) {
            stack_->removeWidget(c.scroll);
            c.scroll->hide();
            c.scroll->deleteLater(); // có thể đang trong vòng phát LanguageChange cho các con
            c = {};
        }
    stack_->setMinimumWidth(0);
    showTab(current_);
    if (painted_)
        prebuild_->start();
    OptionsPage* p = page(current_);
    if (!p)
        return;
    p->setHighlighted(lit);
    if (QWidget* r = focusRow.isEmpty() ? nullptr : p->row(focusRow))
        for (QWidget* w : r->findChildren<QWidget*>())
            if ((w->focusPolicy() & Qt::TabFocus) && w->isEnabled()) {
                w->setFocus(Qt::OtherFocusReason);
                break;
            }
}

void OptionsDialog::find(const QString& text)
{
    clearHighlight();
    const SettingEntry* e = SettingsIndex::search(text);
    const bool none = !e && !text.trimmed().isEmpty();
    noMatch_->setVisible(none);
    if (none)
        a11y::announce(search_, noMatch_->text());
    else if (e)
        showRow(e->tab, QString::fromLatin1(e->id));
}

void OptionsDialog::showRow(Tab tab, const QString& id)
{
    setCurrentTab(tab);
    OptionsPage* p = page(tab);
    QWidget* row = p ? p->row(id) : nullptr;
    if (!row) { // tab khóa, tab chưa có trang: nhảy tới tab
        a11y::announce(search_, tabName(tab));
        return;
    }
    p->setHighlighted(id);
    QScrollArea* s = content_[int(tab)].scroll;
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest); // trang vừa dựng: bố cục trước khi cuộn
    s->ensureWidgetVisible(row, 0, std::max(0, (s->viewport()->height() - row->height()) / 2)); // cuộn tới giữa
    a11y::announce(search_, u"%1 · %2"_s.arg(p->rowLabel(id), tabName(tab)));
}

void OptionsDialog::clearHighlight()
{
    for (const Content& c : content_)
        if (auto* p = qobject_cast<OptionsPage*>(c.inner.data()))
            p->setHighlighted({});
}

void OptionsDialog::resetDefaults()
{
    declareAll();
    const int n = draft_->resetCount();
    if (n == 0) {
        a11y::announce(reset_, tr("Mọi tùy chọn đang ở giá trị mặc định."));
        return;
    }
    ui::ConfirmDialog(tr("Đặt lại tùy chọn"), tr("Đặt lại %1 tùy chọn", nullptr, n), n)
        .question(tr("Đặt lại %1 tùy chọn về mặc định của TrionCAD?", nullptr, n))
        .detail(tr("Hồ sơ hiện hành, viết tắt lệnh và bảng thay thế font không đổi. Hủy hộp Tùy chọn để giữ như cũ."))
        .ask(this, [this](bool yes) {
            if (!yes)
                return;
            draft_->resetDefaults();
            rebuild(); // điều khiển hiện giá trị mặc định
        });
}

void OptionsDialog::declareAll()
{
    for (int i = 0; i < kTabCount; ++i)
        if (factories_[i]) {
            OptionsPage page(Tab(i), *draft_); // không hiện: chỉ để trang đọc (khai báo) khóa của nó
            factories_[i](page);
        }
}

void OptionsDialog::updateContext()
{
    const QString profile = profile_.isEmpty() ? tr("<<Mặc định>>") : profile_;
    const QString drawing = draft_->hasDrawing() ? draft_->drawing() : u"—"_s;
    context_->setText(tr("Hồ sơ hiện hành: %1 · Bản vẽ: %2")
                          .arg(u"<b>%1</b>"_s.arg(profile.toHtmlEscaped()), drawing.toHtmlEscaped()));
}

void OptionsDialog::syncTabs()
{
    for (int i = 0; i < kTabCount; ++i) {
        QStandardItem* item = tabs_->item(i);
        const bool locked = isTabLocked(Tab(i));
        item->setText(tabName(Tab(i)));
        item->setData(locked, kLockedRole);
        // Khóa: ổ khóa trên mục (không chỉ màu) và mô tả cho trình đọc màn hình "Có trong TrionCAD Pro".
        item->setData(locked ? edition::EditionGate::availableIn(tabInfo(Tab(i)).need) : QString(),
                      Qt::AccessibleDescriptionRole);
    }
    tabs_->updateGeometry();
}

void OptionsDialog::updateLocks()
{
    syncTabs();
    rebuild();
}

} // namespace trion::options
