#include "app/dialogs/find/FindDialog.h"

#include "app/core/a11y/Announce.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/edition/LockBadge.h"
#include "app/core/i18n/UiFormat.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/progress/Spinner.h"
#include "app/widgets/views/VirtualTableView.h"
#include "core/base/settings/Settings.h"

#include <QAbstractTableModel>
#include <QBoxLayout>
#include <QCheckBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QStackedWidget>

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <utility>

using namespace Qt::StringLiterals;

namespace trion::ui {

namespace {

using theme::TextRole;

constexpr int kWidth = 680;         // TC.dialog w:680 (prototype bản Pro)
constexpr int kLabelW = 80;         // --lab-w: 80px của .frow
constexpr int kResultsHeight = 220; // .tblwrap height:220px
constexpr int kWhereW = 110;        // cột Vị trí, Loại: col width của prototype
constexpr int kKindW = 130;
constexpr int kContext = 24;     // chữ giữ trước đoạn khớp trong cột Đoạn chữ
constexpr int kTail = 160;       // chữ giữ sau đoạn khớp (phần sau view tự rút gọn)
constexpr int kNeedleShown = 40; // chuỗi tìm dài hơn thì rút gọn trong "Không tìm thấy “…”"

const settings::Setting<bool> kOptionsOpen{"find/optionsOpen", false};

theme::ThemeManager& tm()
{
    return theme::ThemeManager::instance();
}

int px(tok::PxId id)
{
    return qRound(tm().px(id));
}

void tint(QWidget* w, tok::ColorId id)
{
    QPalette pal = w->palette();
    pal.setColor(QPalette::WindowText, tm().color(id));
    w->setPalette(pal);
}

// Nguồn có thể đang chỉ còn ở đây (FIND dựng riêng cho hộp): giải phóng 10⁵ chữ mất vài ms, nên thả tham chiếu ở
// worker. Còn chủ khác thì việc nền chỉ giảm bộ đếm.
void dispose(std::shared_ptr<const find::TextList> texts)
{
    if (texts)
        sched::submit(sched::Priority::Background, [texts = std::move(texts)] {});
}

bool isMark(QChar c)
{
    return c.category() == QChar::Mark_NonSpacing;
}

// Lề chữ style tự chừa mỗi bên ô (như ViewItemDelegate: QCommonStyle PM_FocusFrameHMargin + 1).
int textMargin(const QWidget* w)
{
    return w ? w->style()->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, w) + 1 : 0;
}

QString legacyName(find::Legacy l)
{
    switch (l) {
    case find::Legacy::Tcvn3:
        return u"TCVN3"_s;
    case find::Legacy::Vni:
        return u"VNI"_s;
    case find::Legacy::None:
        break;
    }
    return {};
}

// Nhãn .tag.warn (TCVN3, VNI): chữ --fs-xs --fw-m, lề ngang --sp-3, bo --r.
QFont tagFont()
{
    return theme::font(TextRole::Caption, theme::Weight::Medium);
}

QSize tagSize(const QString& tag)
{
    const QFontMetrics fm(tagFont());
    return {fm.horizontalAdvance(tag) + 2 * px(tok::sp_3), fm.height() + 2 * px(tok::sp_1)};
}

void paintTag(QPainter* p, const QRect& r, const QString& tag)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(Qt::NoPen);
    p->setBrush(tm().color(tok::c_warn_soft));
    const qreal radius = tm().px(tok::r);
    p->drawRoundedRect(QRectF(r), radius, radius);
    p->setPen(tm().color(tok::c_warn));
    p->setFont(tagFont());
    p->drawText(r, Qt::AlignCenter, tag);
    p->restore();
}

// Cột Đoạn chữ: cửa sổ quanh đoạn khớp, một dòng (xuống dòng của MTEXT thành dấu cách), "…" nơi đã cắt.
struct Snippet {
    QString text;
    int start = 0;
    int length = 0;
};

Snippet snippet(const QString& s, find::Span span)
{
    qsizetype from = std::max(0, span.start - kContext);
    while (from > 0 && (isMark(s[from]) || s[from].isLowSurrogate())) // không cắt giữa một chữ
        --from;
    const qsizetype to = std::min(s.size(), qsizetype(span.start) + span.length + kTail);
    Snippet out;
    out.text.reserve(to - from + 2);
    if (from > 0)
        out.text += u'…';
    for (qsizetype i = from; i < to; ++i) {
        const QChar c = s[i];
        out.text +=
            c == u'\n' || c == u'\r' || c == u'\t' || c == QChar::LineSeparator || c == QChar::ParagraphSeparator
                ? QChar(u' ')
                : c;
    }
    if (to < s.size())
        out.text += u'…';
    out.start = int(span.start - from) + (from > 0 ? 1 : 0);
    out.length = span.length;
    return out;
}

// Vẽ cột Loại (chữ + nhãn chữ mã cũ) và Đoạn chữ (đoạn khớp chữ đậm trên nền --c-warn-soft, như <mark>); cột Vị trí
// vẽ như mọi bảng.
class ResultDelegate final : public ViewItemDelegate {
public:
    using ViewItemDelegate::ViewItemDelegate;

    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (index.column() == FindDialog::WhereColumn) {
            ViewItemDelegate::paint(p, option, index);
            return;
        }
        QStyleOptionViewItem o = option;
        initStyleOption(&o, index);
        QPalette::ColorGroup group = o.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
        if (group == QPalette::Normal && !(o.state & QStyle::State_Active))
            group = QPalette::Inactive;
        const QColor ink =
            o.palette.color(group, o.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Text);
        const int margin = textMargin(o.widget);
        const QRect box = o.rect.adjusted(margin, 0, -margin, 0);
        const QFontMetricsF fm(o.font);
        const qreal baseline = box.top() + (box.height() - fm.ascent() - fm.descent()) / 2 + fm.ascent();
        p->save();
        p->setClipRect(o.rect);
        p->setPen(ink);
        p->setFont(o.font);
        if (index.column() == FindDialog::KindColumn) {
            const QString tag = index.data(FindDialog::LegacyRole).toString();
            const QSize t = tag.isEmpty() ? QSize() : tagSize(tag);
            const int gap = tag.isEmpty() ? 0 : px(tok::sp_3);
            QString text = o.text;
            qreal w = fm.horizontalAdvance(text);
            if (w + gap + t.width() > box.width()) {
                text = fm.elidedText(text, Qt::ElideRight, std::max(0, box.width() - gap - t.width()));
                w = fm.horizontalAdvance(text);
            }
            p->drawText(QPointF(box.left(), baseline), text);
            if (!tag.isEmpty())
                paintTag(
                    p,
                    QRect(QPoint(box.left() + int(std::ceil(w)) + gap, box.top() + (box.height() - t.height()) / 2), t),
                    tag);
        } else {
            QFont bold = o.font;
            bold.setWeight(QFont::Weight(qRound(tok::num(tok::fw_b))));
            const QFontMetricsF bm(bold);
            const QPoint m = index.data(FindDialog::MatchRole).toPoint();
            const struct {
                QString text;
                bool mark;
            } runs[] = {
                {o.text.left(m.x()), false}, {o.text.mid(m.x(), m.y()), true}, {o.text.mid(m.x() + m.y()), false}};
            qreal x = box.left();
            const qreal right = box.left() + box.width();
            for (const auto& run : runs) {
                if (run.text.isEmpty())
                    continue;
                const QFontMetricsF& rm = run.mark ? bm : fm;
                QString s = run.text;
                qreal w = rm.horizontalAdvance(s);
                const bool cut = x + w > right;
                if (cut) { // rút gọn chỉ khi không vừa
                    s = rm.elidedText(s, Qt::ElideRight, right - x);
                    w = rm.horizontalAdvance(s);
                }
                if (run.mark && !s.isEmpty()) {
                    p->fillRect(QRectF(x, box.top() + (box.height() - rm.height()) / 2, w, rm.height()),
                                tm().color(tok::c_warn_soft));
                    p->setPen(tm().color(tok::c_text));
                }
                p->setFont(run.mark ? bold : o.font);
                p->drawText(QPointF(x, baseline), s);
                p->setPen(ink);
                x += w;
                if (cut)
                    break;
            }
        }
        p->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QSize size = ViewItemDelegate::sizeHint(option, index);
        if (index.column() == FindDialog::WhereColumn)
            return size;
        QStyleOptionViewItem o = option;
        initStyleOption(&o, index);
        const QFontMetrics fm(o.font);
        int w = 2 * textMargin(o.widget);
        if (index.column() == FindDialog::KindColumn) {
            const QString tag = index.data(FindDialog::LegacyRole).toString();
            w += fm.horizontalAdvance(o.text) + (tag.isEmpty() ? 0 : px(tok::sp_3) + tagSize(tag).width());
        } else {
            QFont bold = o.font;
            bold.setWeight(QFont::Weight(qRound(tok::num(tok::fw_b))));
            const QPoint m = index.data(FindDialog::MatchRole).toPoint();
            w += fm.horizontalAdvance(o.text.left(m.x())) +
                 QFontMetrics(bold).horizontalAdvance(o.text.mid(m.x(), m.y())) +
                 fm.horizontalAdvance(o.text.mid(m.x() + m.y()));
        }
        size.setWidth(w);
        return size;
    }
};

} // namespace

// Kết quả: chỉ số vào snapshot nguồn (16 byte mỗi dòng), chữ hiện dựng khi view hỏi (chỉ các dòng đang nhìn).
class FindDialog::Results final : public QAbstractTableModel {
public:
    using QAbstractTableModel::QAbstractTableModel;

    struct Labels {
        QStringList kinds;   // theo find::TextKind
        QString withDetail;  // "%1 · %2": loại, chi tiết (tag, ô bảng, block)
        QStringList headers; // theo Column
    };

    void setLabels(Labels labels)
    {
        labels_ = std::move(labels);
        emit headerDataChanged(Qt::Horizontal, 0, ColumnCount - 1);
        if (!hits_.empty())
            emit dataChanged(index(0, KindColumn), index(rowCount() - 1, KindColumn), {Qt::DisplayRole});
    }

    void reset(std::shared_ptr<const find::TextList> texts)
    {
        beginResetModel();
        dispose(std::exchange(texts_, std::move(texts)));
        hits_ = {};
        endResetModel();
    }

    void append(std::vector<find::Hit> hits)
    {
        if (hits.empty())
            return;
        const int n = rowCount();
        beginInsertRows({}, n, n + int(hits.size()) - 1);
        hits_.insert(hits_.end(), hits.begin(), hits.end());
        endInsertRows();
    }

    const find::TextItem& item(int row) const { return (*texts_)[std::size_t(hits_[std::size_t(row)].item)]; }

    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : int(hits_.size()); }
    int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : ColumnCount; }

    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemNeverHasChildren : Qt::NoItemFlags;
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 &&
            section < labels_.headers.size())
            return labels_.headers[section];
        return {};
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid() || index.row() >= rowCount())
            return {};
        const find::Hit& h = hits_[std::size_t(index.row())];
        const find::TextItem& t = (*texts_)[std::size_t(h.item)];
        const QString& shown = h.decoded ? t.decoded : t.text;
        switch (role) {
        case Qt::DisplayRole:
            switch (index.column()) {
            case WhereColumn:
                return t.layout;
            case KindColumn: {
                const QString kind = labels_.kinds.value(int(t.kind));
                return t.detail.isEmpty() ? kind : labels_.withDetail.arg(kind, t.detail);
            }
            default:
                return snippet(shown, h.span).text;
            }
        case Qt::ToolTipRole: // chữ đầy đủ (giới hạn để tooltip không phủ màn hình)
            return index.column() == SnippetColumn ? shown.left(kTail * 4) : QVariant();
        case MatchRole: {
            const Snippet s = snippet(shown, h.span);
            return QPoint(s.start, s.length);
        }
        case LegacyRole:
            return index.column() == KindColumn ? legacyName(t.legacy) : QString();
        case IdRole:
            return QVariant::fromValue<quint64>(t.id);
        case LayoutRole:
            return t.layout;
        default:
            return {};
        }
    }

private:
    std::shared_ptr<const find::TextList> texts_;
    std::vector<find::Hit> hits_;
    Labels labels_;
};

// Một lượt tìm: các đoạn đã xong chờ đến lượt vào bảng (đúng thứ tự nguồn).
struct FindDialog::Run {
    QString needle;
    std::shared_ptr<const find::TextList> texts;
    std::vector<std::vector<find::Hit>> ready;
    std::vector<char> done; // theo đoạn
    int next = 0;           // đoạn kế tiếp vào bảng
    bool stale = true;      // bảng còn kết quả của lượt trước (thay khi đoạn đầu về)
    bool finished() const { return next == int(done.size()); }
};

FindDialog::FindDialog(edition::EditionGate& gate, QWidget* parent) : DialogFrame(u"find"_s, parent), gate_(gate)
{
    setCommand(u"FIND"_s);
    setResizable(true);
    settings::Store& store = settings::Store::instance();
    const auto named = [](auto* w, const char* name) {
        w->setObjectName(QString::fromLatin1(name));
        return w;
    };
    const auto label = [this](const char* source, QWidget* buddy) {
        auto* l = new QLabel;
        l->setBuddy(buddy);
        labels_.append({l, source});
        return l;
    };

    // Tìm, Thay bằng, Phạm vi (.frow)
    query_ = named(new QLineEdit, "query");
    query_->installEventFilter(this);
    replace_ = named(new QLineEdit, "replace");
    scope_ = named(new DropDown, "scope");
    for (const find::Scope s : {find::Scope::Drawing, find::Scope::CurrentSpace, find::Scope::Selection})
        scope_->addItem(QString(), int(s));
    pick_ = named(new Button({}, Button::Kind::Icon), "pick");
    pick_->setIconName(u"select"_s);
    auto* form = new QGridLayout;
    form->setColumnMinimumWidth(0, kLabelW);
    form->setColumnStretch(1, 1);
    const auto row = [](std::initializer_list<QWidget*> widgets, bool stretch) {
        auto* r = new QHBoxLayout;
        r->setSpacing(px(tok::sp_4));
        for (QWidget* w : widgets)
            r->addWidget(w, w == *widgets.begin() && !stretch ? 1 : 0);
        if (stretch)
            r->addStretch(1);
        return r;
    };
    form->addWidget(label(QT_TR_NOOP("Tìm"), query_), 0, 0);
    form->addWidget(query_, 0, 1);
    form->addWidget(label(QT_TR_NOOP("Thay bằng"), replace_), 1, 0);
    form->addLayout(row({replace_, new edition::LockBadge(ui::Edition::Pro, edition::LockBadge::Style::Tag)}, false), 1,
                    1);
    form->addWidget(label(QT_TR_NOOP("Phạm vi"), scope_), 2, 0);
    form->addLayout(row({scope_, pick_}, true), 2, 1);

    // Tùy chọn tìm (<details> của prototype): ba cột .grid3
    optionsToggle_ = named(new Button({}, Button::Kind::Ghost), "optionsToggle");
    optionsToggle_->setControlSize(Button::Size::Small);
    optionsBox_ = named(new QWidget, "options");
    auto* grid = new QGridLayout(optionsBox_);
    grid->setContentsMargins({});
    struct Def {
        QCheckBox* FindDialog::*box;
        settings::Setting<bool> key;
        const char* source;
        const char* name;
        int column;
    };
    static const Def defs[] = {
        {&FindDialog::matchCase_, {"find/matchCase", false}, QT_TR_NOOP("Phân biệt hoa thường"), "matchCase", 0},
        {&FindDialog::wholeWord_, {"find/wholeWord", false}, QT_TR_NOOP("Nguyên từ"), "wholeWord", 0},
        {&FindDialog::wildcard_, {"find/wildcard", false}, QT_TR_NOOP("Wildcard (* ?)"), "wildcard", 0},
        {&FindDialog::accents_, {"find/matchAccents", false}, QT_TR_NOOP("Phân biệt dấu tiếng Việt"), "accents", 0},
        {&FindDialog::xrefs_, {"find/xrefs", false}, QT_TR_NOOP("Tìm trong tham chiếu ngoài (Xref)"), "xrefs", 1},
        {&FindDialog::blocks_, {"find/blocks", true}, QT_TR_NOOP("Tìm trong block"), "blocks", 1},
        {&FindDialog::skipHidden_, {"find/skipHidden", true}, QT_TR_NOOP("Bỏ qua đối tượng ẩn"), "skipHidden", 1},
        {&FindDialog::legacy_, {"find/legacy", true}, QT_TR_NOOP("Tìm cả chữ mã TCVN3, VNI"), "legacy", 1},
        {&FindDialog::kindAttributes_, {"find/kindAttributes", true}, QT_TR_NOOP("Attribute"), "kindAttributes", 2},
        {&FindDialog::kindDimLeader_, {"find/kindDims", true}, QT_TR_NOOP("Chữ kích thước, đường dẫn"), "kindDims", 2},
        {&FindDialog::kindTexts_, {"find/kindTexts", true}, QT_TR_NOOP("Text, MText"), "kindTexts", 2},
        {&FindDialog::kindTables_, {"find/kindTables", true}, QT_TR_NOOP("Chữ trong bảng"), "kindTables", 2},
        {&FindDialog::kindLinks_, {"find/kindLinks", true}, QT_TR_NOOP("Hyperlink"), "kindLinks", 2},
    };
    kindsHeading_ = named(new QLabel, "kindsHeading");
    labels_.append({kindsHeading_, QT_TR_NOOP("Loại chữ")});
    grid->addWidget(kindsHeading_, 0, 2);
    int rows[3] = {0, 0, 1};
    for (const Def& d : defs) {
        auto* box = named(new QCheckBox, d.name);
        box->setChecked(store.get(d.key));
        this->*d.box = box;
        labels_.append({box, d.source});
        grid->addWidget(box, rows[d.column]++, d.column);
        connect(box, &QCheckBox::toggled, this, [this, key = d.key](bool on) {
            settings::Store::instance().set(key, on);
            search();
        });
    }
    grid->setRowStretch(grid->rowCount(), 1);
    for (int c = 0; c < 3; ++c)
        grid->setColumnStretch(c, 1);
    if (!store.get(kOptionsOpen))
        optionsBox_->hide(); // chưa có cha: chỉ ẩn, hiện thì để bố cục (không tự mở thành cửa sổ)

    // Kết quả: bảng ảo hóa hoặc trạng thái rỗng
    model_ = new Results(this);
    view_ = named(new VirtualTableView, "results");
    view_->setModel(model_);
    view_->setItemDelegate(new ResultDelegate(view_));
    view_->installEventFilter(this);
    empty_ = named(new EmptyState(u"findtext"_s), "empty");
    stack_ = named(new QStackedWidget, "resultsArea");
    stack_->addWidget(view_);
    stack_->addWidget(empty_);
    stack_->setMinimumHeight(kResultsHeight);

    auto* content = new QWidget;
    auto* column = new QVBoxLayout(content);
    column->setContentsMargins({});
    column->addLayout(form);
    column->addWidget(optionsToggle_, 0, Qt::AlignLeft);
    column->addWidget(optionsBox_);
    column->addWidget(stack_, 1);
    setBody(content);

    // Chân: số kết quả | Tạo tập chọn | Thay, Thay tất cả | Tìm (nút xác nhận, retranslate)
    spinner_ = named(new Spinner, "spinner");
    spinner_->hide();
    count_ = named(new QLabel, "count");
    footer()->insertWidget(0, spinner_);
    footer()->insertWidget(1, count_);
    setMenu_ = new Menu(this);
    QAction* fromAll = setMenu_->addAction(QString());
    QAction* fromRows = setMenu_->addAction(QString());
    makeSet_ = named(new SplitButton({}, setMenu_), "makeSet");
    replaceOne_ = named(new Button, "replaceOne");
    replaceAll_ = named(new Button, "replaceAll");
    footer()->addWidget(makeSet_);
    footer()->addWidget(replaceOne_);
    footer()->addWidget(new edition::LockBadge(ui::Edition::Pro, edition::LockBadge::Style::Tag));
    footer()->addWidget(replaceAll_);
    footer()->addWidget(new edition::LockBadge(ui::Edition::Pro, edition::LockBadge::Style::Tag));

    connect(query_, &QLineEdit::textChanged, this, &FindDialog::search);
    connect(scope_, &QComboBox::currentIndexChanged, this, &FindDialog::search);
    connect(pick_, &QPushButton::clicked, this, [this] {
        hideForPick(true);
        emit pickRequested();
    });
    connect(optionsToggle_, &QPushButton::clicked, this, [this] {
        const bool open = optionsBox_->isHidden();
        settings::Store::instance().set(kOptionsOpen, open);
        const int before = sizeHint().height();
        optionsBox_->setVisible(open);
        optionsToggle_->setIconName(open ? u"down"_s : u"right"_s);
        body()->layout()->activate(); // cỡ mới lên tới khung ngay (thu gọn lại được)
        layout()->activate();
        resize(width(), std::max(minimumSizeHint().height(), height() + sizeHint().height() - before));
    });
    connect(view_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex& current) { currentChanged(current); });
    connect(view_, &QAbstractItemView::doubleClicked, this, &FindDialog::currentChanged); // zoom lại dòng hiện hành
    connect(makeSet_, &QPushButton::clicked, this, [this] { makeSelection(false); });
    connect(fromAll, &QAction::triggered, this, [this] { makeSelection(false); });
    connect(fromRows, &QAction::triggered, this, [this] { makeSelection(true); });
    connect(setMenu_, &QMenu::aboutToShow, this, [this, fromAll, fromRows] {
        qint64 selected = 0;
        for (const QItemSelectionRange& r : view_->selectionModel()->selection())
            selected += r.height();
        fromAll->setText(tr("Từ mọi kết quả (%1)").arg(i18n::formatCount(model_->rowCount())));
        fromRows->setText(tr("Từ các dòng đang chọn (%1)").arg(i18n::formatCount(selected)));
        fromRows->setEnabled(selected > 0);
    });
    for (Button* b : {replaceOne_, replaceAll_})
        connect(b, &QPushButton::clicked, this, [this, all = b == replaceAll_] {
            if (gate_.locked(ui::Edition::Pro)) // Viewer, Viewer Pro: trang nâng cấp (UI0-011)
                gate_.upgrade().request(
                    {.feature = u"FINDREPLACE"_s, .need = ui::Edition::Pro, .name = tr("Tìm và thay thế chữ")});
            else
                emit replaceRequested(all);
        });
    connect(&gate_, &edition::EditionGate::editionChanged, this, &FindDialog::syncEdition);
    connect(&tm(), &theme::ThemeManager::changed, this, &FindDialog::restyle);

    optionsToggle_->setIconName(optionsBox_->isHidden() ? u"right"_s : u"down"_s);
    restyle();
    retranslate();
    const QSize hint = sizeHint();
    resize(std::max(hint.width(), kWidth + contentsMargins().left() + contentsMargins().right()), hint.height());
}

FindDialog::~FindDialog()
{
    cancel_.cancel();
    model_->reset({}); // view còn sống: các con hủy sau thân hàm hủy
    dispose(std::move(source_));
    if (run_)
        dispose(std::move(run_->texts));
}

void FindDialog::setTexts(std::shared_ptr<const find::TextList> texts)
{
    dispose(std::exchange(source_, std::move(texts)));
    if (run_)
        search();
}

void FindDialog::setCurrentSpace(const QString& layout)
{
    space_ = layout;
    if (run_ && options().scope == find::Scope::CurrentSpace)
        search();
}

void FindDialog::setSelection(const QList<quint64>& ids)
{
    auto sorted = std::make_shared<std::vector<std::uint64_t>>(ids.begin(), ids.end());
    std::sort(sorted->begin(), sorted->end());
    sorted->erase(std::unique(sorted->begin(), sorted->end()), sorted->end());
    selection_ = std::move(sorted);
    if (run_ && options().scope == find::Scope::Selection)
        search();
}

void FindDialog::endPick(bool picked)
{
    hideForPick(false);
    if (!picked)
        return;
    const int at = scope_->findData(int(find::Scope::Selection));
    if (scope_->currentIndex() != at)
        scope_->setCurrentIndex(at); // currentIndexChanged → search
    else
        search();
}

find::FindOptions FindDialog::options() const
{
    find::FindOptions o;
    o.matchCase = matchCase_->isChecked();
    o.wholeWord = wholeWord_->isChecked();
    o.wildcard = wildcard_->isChecked();
    o.matchAccents = accents_->isChecked();
    o.inXrefs = xrefs_->isChecked();
    o.inBlocks = blocks_->isChecked();
    o.skipHidden = skipHidden_->isChecked();
    o.legacy = legacy_->isChecked();
    o.kinds = std::uint8_t(
        (kindAttributes_->isChecked() ? find::kAttributes : 0) | (kindDimLeader_->isChecked() ? find::kDimLeader : 0) |
        (kindTexts_->isChecked() ? find::kTexts : 0) | (kindTables_->isChecked() ? find::kTables : 0) |
        (kindLinks_->isChecked() ? find::kHyperlinks : 0));
    o.scope = find::Scope(scope_->currentData().toInt());
    o.space = space_;
    o.selection = selection_;
    return o;
}

void FindDialog::search()
{
    if (!replaceAll_) // tín hiệu của điều khiển khi hộp đang dựng
        return;
    cancel_.cancel();
    cancel_ = sched::CancelSource();
    ++generation_;
    if (run_)
        dispose(std::move(run_->texts));
    const QString needle = query_->text();
    if (needle.isEmpty()) {
        run_.reset();
        model_->reset({});
        clearHighlight();
        updateState();
        return;
    }
    auto texts = source_ ? source_ : std::make_shared<const find::TextList>();
    const int n = int(texts->size());
    const int slices = n <= kFirstSlice ? 1 : 1 + (n - kFirstSlice + kSlice - 1) / kSlice;
    run_ = std::make_unique<Run>();
    run_->needle = needle;
    run_->texts = texts;
    run_->ready.resize(std::size_t(slices));
    run_->done.assign(std::size_t(slices), 0);
    struct Job {
        std::shared_ptr<const find::TextList> texts;
        QString needle;
        find::FindOptions options;
    };
    const auto job = std::make_shared<const Job>(Job{std::move(texts), needle, options()});
    const Receiver rx(this);
    for (int s = 0; s < slices; ++s)
        sched::submit(s == 0 ? sched::Priority::Interactive : sched::Priority::Normal,
                      [rx, job, s, generation = generation_, token = cancel_.token(), this] {
                          if (token.cancelled())
                              return;
                          const find::FindMatcher matcher(job->needle, job->options); // nền: không chạm `this`
                          std::vector<find::Hit> hits;
                          const int first = s == 0 ? 0 : kFirstSlice + (s - 1) * kSlice;
                          const int last = s == 0 ? kFirstSlice : first + kSlice;
                          matcher.scan(*job->texts, first, std::min(last, int(job->texts->size())), hits, token);
                          if (token.cancelled())
                              return;
                          post(rx, [this, generation, s, hits = std::move(hits)]() mutable {
                              merge(generation, s, std::move(hits));
                          });
                      });
    updateState();
}

void FindDialog::merge(quint64 generation, int slice, std::vector<find::Hit> hits)
{
    if (!run_ || generation != generation_)
        return; // lượt cũ
    Run& r = *run_;
    r.ready[std::size_t(slice)] = std::move(hits);
    r.done[std::size_t(slice)] = 1;
    while (!r.finished() && r.done[std::size_t(r.next)]) {
        if (std::exchange(r.stale, false)) {
            clearHighlight();
            model_->reset(r.texts);
        }
        model_->append(std::exchange(r.ready[std::size_t(r.next)], {}));
        ++r.next;
    }
    updateState();
    if (r.finished() && !r.ready.empty()) {
        r.ready = {}; // xong: lần merge sau (không có) không lặp lại thông báo
        a11y::announce(count_, count_->text());
        emit searchFinished(model_->rowCount());
    }
}

bool FindDialog::isSearching() const
{
    return run_ && !run_->finished();
}

int FindDialog::resultCount() const
{
    return run_ && !run_->stale ? model_->rowCount() : 0;
}

QAbstractItemView* FindDialog::results() const
{
    return view_;
}

void FindDialog::accept()
{
    search();
}

void FindDialog::done(int result)
{
    cancel_.cancel();
    clearHighlight();
    DialogFrame::done(result);
}

void FindDialog::currentChanged(const QModelIndex& current)
{
    if (!current.isValid())
        return;
    highlighted_ = true;
    emit objectRequested(current.data(IdRole).value<quint64>(), current.data(LayoutRole).toString());
}

void FindDialog::makeSelection(bool selectedRowsOnly)
{
    std::vector<int> rows;
    if (selectedRowsOnly) {
        for (const QItemSelectionRange& r : view_->selectionModel()->selection())
            for (int row = r.top(); row <= r.bottom(); ++row)
                rows.push_back(row);
        std::sort(rows.begin(), rows.end());
        rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    } else {
        rows.resize(std::size_t(model_->rowCount()));
        for (std::size_t i = 0; i < rows.size(); ++i)
            rows[i] = int(i);
    }
    std::vector<quint64> ids;
    ids.reserve(rows.size());
    for (const int row : rows)
        ids.push_back(model_->item(row).id);
    // Bỏ trùng (nhiều chữ của một đối tượng: attribute của một block, ô của một bảng) ở nền: 10⁵ dòng ~ vài ms.
    sched::submit(sched::Priority::Interactive, [rx = Receiver(this), this, ids = std::move(ids)] {
        QList<quint64> out;
        out.reserve(qsizetype(ids.size()));
        std::unordered_set<quint64> seen(ids.size());
        for (const quint64 id : ids)
            if (seen.insert(id).second)
                out.append(id);
        post(rx, [this, out = std::move(out)] { emit selectionRequested(out); });
    });
}

void FindDialog::clearHighlight()
{
    if (std::exchange(highlighted_, false))
        emit highlightCleared();
}

void FindDialog::syncEdition()
{
    const bool locked = gate_.locked(ui::Edition::Pro);
    setWindowTitle(locked ? tr("Tìm chữ") : tr("Tìm và thay thế"));
    replace_->setEnabled(!locked);
    replace_->setPlaceholderText(locked ? tr("Thay chữ cần TrionCAD Pro") : tr("Để trống để xóa đoạn chữ tìm được"));
    replace_->setAccessibleDescription(locked ? edition::EditionGate::availableIn(ui::Edition::Pro) : QString());
    for (Button* b : {replaceOne_, replaceAll_}) {
        b->setLocked(locked);
        b->setAccessibleDescription(locked ? edition::EditionGate::availableIn(ui::Edition::Pro) : QString());
    }
    updateState();
}

void FindDialog::updateState()
{
    const int n = model_->rowCount();
    const bool typed = run_ != nullptr;
    const bool running = isSearching();
    const bool fresh = typed && !run_->stale; // bảng là kết quả của lượt này
    if (!typed) {
        empty_->setIconName(u"findtext"_s);
        empty_->setText(tr("Gõ chữ cần tìm"),
                        accents_->isChecked()
                            ? tr("Đang phân biệt dấu tiếng Việt: “duong” không khớp “đường”. Tắt tùy chọn này để tìm "
                                 "không dấu.")
                            : tr("Không phân biệt dấu: gõ “duong” cũng ra “đường”, “ĐƯỜNG”."));
    } else if (!running && n == 0) {
        QString needle = run_->needle;
        if (needle.size() > kNeedleShown)
            needle = needle.left(kNeedleShown - 1) + u'…';
        const find::FindOptions o = options();
        empty_->setIconName(u"search"_s);
        empty_->setText(tr("Không tìm thấy “%1”").arg(needle),
                        o.matchAccents ? tr("Thử tắt “Phân biệt dấu tiếng Việt” hoặc mở rộng phạm vi.")
                        : o.scope == find::Scope::Selection && (!o.selection || o.selection->empty())
                            ? tr("Vùng chọn đang trống: bấm nút chọn cạnh Phạm vi để chọn đối tượng.")
                            : tr("Thử mở rộng phạm vi hoặc bật thêm loại chữ trong Tùy chọn tìm."));
    }
    stack_->setCurrentWidget(typed && (n > 0 || running) ? static_cast<QWidget*>(view_) : empty_);
    spinner_->setVisible(running);
    count_->setText(!typed    ? QString()
                    : !fresh  ? tr("Đang tìm…")
                    : running ? tr("Đang tìm… %1 kết quả", nullptr, n).arg(i18n::formatCount(n))
                              : tr("%1 kết quả", nullptr, n).arg(i18n::formatCount(n)));
    const bool any = fresh && n > 0;
    makeSet_->setEnabled(any);
    const bool locked = gate_.locked(ui::Edition::Pro);
    for (Button* b : {replaceOne_, replaceAll_})
        b->setEnabled(locked || any); // nút khóa vẫn bấm được: mở trang nâng cấp
}

bool FindDialog::eventFilter(QObject* watched, QEvent* e)
{
    if (e->type() == QEvent::KeyPress) {
        const auto* k = static_cast<QKeyEvent*>(e);
        if (watched == query_ && k->key() == Qt::Key_Down && k->modifiers() == Qt::NoModifier &&
            stack_->currentWidget() == view_ && model_->rowCount() > 0) { // ↓: xuống bảng, dòng kế tiếp
            const QModelIndex cur = view_->currentIndex();
            view_->setFocus(Qt::TabFocusReason);
            view_->setCurrentIndex(
                model_->index(cur.isValid() ? std::min(cur.row() + 1, model_->rowCount() - 1) : 0, 0));
            return true;
        }
        if (watched == view_ && (k->key() == Qt::Key_Return || k->key() == Qt::Key_Enter)) {
            if (view_->currentIndex().isValid()) // Enter: zoom lại tới dòng hiện hành, không chạy nút Tìm
                currentChanged(view_->currentIndex());
            return true;
        }
    }
    return DialogFrame::eventFilter(watched, e);
}

void FindDialog::restyle()
{
    for (const Text& t : std::as_const(labels_)) {
        auto* l = qobject_cast<QLabel*>(t.widget);
        if (!l || l == kindsHeading_)
            continue;
        l->setFont(theme::font(TextRole::Label)); // .frow > .lab: --fs-s, --c-text-2
        tint(l, tok::c_text_2);
    }
    QFont heading = theme::font(TextRole::Caption, theme::Weight::Bold); // .gh
    heading.setCapitalization(QFont::AllUppercase);
    heading.setLetterSpacing(QFont::PercentageSpacing, 106);
    kindsHeading_->setFont(heading);
    tint(kindsHeading_, tok::c_text_3);
    count_->setFont(theme::font(TextRole::Label)); // .faint --fs-s
    tint(count_, tok::c_text_3);
    auto* column = static_cast<QVBoxLayout*>(body()->layout());
    column->setSpacing(px(tok::sp_5));
    auto* form = static_cast<QGridLayout*>(column->itemAt(0)->layout());
    form->setHorizontalSpacing(px(tok::sp_5));
    form->setVerticalSpacing(px(tok::sp_3));
    auto* grid = static_cast<QGridLayout*>(optionsBox_->layout());
    grid->setHorizontalSpacing(px(tok::sp_5));
    grid->setVerticalSpacing(px(tok::sp_2));
}

void FindDialog::retranslate()
{
    for (const Text& t : std::as_const(labels_)) {
        if (auto* l = qobject_cast<QLabel*>(t.widget))
            l->setText(tr(t.source));
        else if (auto* b = qobject_cast<QAbstractButton*>(t.widget))
            b->setText(tr(t.source));
    }
    query_->setPlaceholderText(tr("Ví dụ: tường, cua di, KT-12"));
    query_->setAccessibleName(tr("Tìm"));
    replace_->setAccessibleName(tr("Thay bằng"));
    scope_->setItemText(0, tr("Toàn bản vẽ"));
    scope_->setItemText(1, tr("Không gian hiện tại"));
    scope_->setItemText(2, tr("Vùng chọn"));
    scope_->setAccessibleName(tr("Phạm vi"));
    pick_->setToolTip(tr("Chọn đối tượng"));
    pick_->setAccessibleName(tr("Chọn đối tượng"));
    optionsToggle_->setText(tr("Tùy chọn tìm"));
    view_->setAccessibleName(tr("Kết quả tìm"));
    view_->setAccessibleDescription(tr("Enter: zoom tới đối tượng của dòng"));
    model_->setLabels({{tr("Text"), tr("MText"), tr("Attribute"), tr("Chữ kích thước"), tr("Multileader"), tr("Ô bảng"),
                        tr("Hyperlink")},
                       tr("%1 · %2"),
                       {tr("Vị trí"), tr("Loại"), tr("Đoạn chữ")}});
    makeSet_->setText(tr("Tạo tập chọn"));
    makeSet_->setToolTip(tr("Chọn đối tượng của mọi kết quả; mũi tên: chỉ các dòng đang chọn"));
    replaceOne_->setText(tr("Thay"));
    replaceAll_->setText(tr("Thay tất cả"));
    setConfirm(tr("Tìm"));
    syncEdition();
    // Cột Vị trí, Loại: rộng như prototype, đủ chữ đầu cột và nhãn loại dài nhất kèm nhãn TCVN3 (vi-XL dài hơn 40%).
    QHeaderView* head = view_->header();
    const QFontMetrics fm(view_->font());
    int kind = 0;
    for (const QString& k : {tr("Chữ kích thước"), tr("Multileader"), tr("Attribute"), tr("Ô bảng")})
        kind = std::max(kind, fm.horizontalAdvance(k));
    const int pad = 2 * px(tok::sp_4);
    head->resizeSection(WhereColumn, std::max(kWhereW, head->sectionSizeHint(WhereColumn)));
    head->resizeSection(KindColumn, std::max({kKindW, head->sectionSizeHint(KindColumn),
                                              kind + px(tok::sp_3) + tagSize(u"TCVN3"_s).width() + pad}));
}

void FindDialog::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange && replaceAll_)
        retranslate();
    DialogFrame::changeEvent(e);
}

} // namespace trion::ui
