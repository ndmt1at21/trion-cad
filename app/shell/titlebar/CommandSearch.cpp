#include "app/shell/titlebar/CommandSearch.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/perf/UiProbe.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/text/Fold.h"

#include <QAbstractListModel>
#include <QAction>
#include <QApplication>
#include <QCompleter>
#include <QKeyEvent>
#include <QListView>
#include <QPainter>
#include <QStyledItemDelegate>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace trion::shell {

namespace {

using theme::TextRole;
using theme::Weight;

constexpr int kEntryRole = Qt::UserRole + 1; // vị trí trong chỉ mục; -1: dòng "không tìm thấy"
constexpr int kPopupWidth = 440;             // .sres width:440px (không có token)
constexpr int kCompleterPad = 3; // QCompleter cộng 3 px trên, 3 px dưới vào chiều cao các dòng của popup

const theme::ThemeManager& tm()
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

// Chuỗi đã gấp → " ve duong tron": dấu câu thành dấu cách, thêm dấu cách đầu; khớp đầu từ bằng contains(" tu").
QString wordsOf(QStringView folded)
{
    QString out(folded.size() + 1, u' ');
    for (qsizetype i = 0; i < folded.size(); ++i)
        if (folded[i].isLetterOrNumber())
            out[i + 1] = folded[i];
    return out;
}

} // namespace

// Một lệnh trong chỉ mục: nhãn gấp sẵn khi dựng; viết tắt, từ khóa là chuỗi tĩnh của search_terms.cpp (không tách,
// không cấp phát). Mỗi phím chỉ gấp chuỗi gõ (Query).
struct CommandSearch::Entry {
    QLatin1StringView name;     // "CIRCLE"
    QLatin1StringView aliases;  // ",C,"
    QLatin1StringView keywords; // ",duong tron,hinh tron,vong tron,"
    QString label;              // nhãn đã dịch (chữ của QAction)
    QString folded;             // nhãn đã gấp: "ve duong tron"
    QString words;              // " ve duong tron"
    QAction* action;
};

namespace {

// Chuỗi gõ, chuẩn bị một lần cho mỗi phím.
struct Query {
    struct Word {
        QString space, comma; // " tu", ",tu": đầu từ trong nhãn, đầu từ khóa
    };
    explicit Query(const QString& typed)
        : upper(typed.trimmed().toUpper()), folded(text::foldForSearch(typed.trimmed())),
          aliasExact(u',' + upper + u','), aliasPrefix(u',' + upper), keyExact(u',' + folded + u','),
          keyPrefix(u',' + folded)
    {
        for (const QString& w : wordsOf(folded).split(u' ', Qt::SkipEmptyParts))
            words.push_back({u' ' + w, u',' + w});
    }
    QString upper, folded, aliasExact, aliasPrefix, keyExact, keyPrefix;
    std::vector<Word> words;
};

// Điểm khớp; 0: không khớp. Bằng điểm thì giữ thứ tự danh mục (lệnh thông dụng đứng trước). 50 truy vấn mẫu của
// tiêu chí 1 trong tests/titlebar_test.cpp.
//   100 tên lệnh hoặc viết tắt trùng chuỗi gõ: "C", "line"
//    90 trùng một từ khóa hoặc cả nhãn: "đường tròn", "in" (PLOT đứng trước INSERT)
//    80 tên lệnh bắt đầu bằng chuỗi gõ: "circ"
//    70 viết tắt bắt đầu bằng chuỗi gõ
//    60 từ khóa bắt đầu bằng chuỗi gõ: "duong t"
//    50 mọi từ gõ là đầu một từ của nhãn hoặc từ khóa: "tron duong", "ve duong thang"
//    40 tên lệnh chứa chuỗi gõ (từ 3 ký tự)
// ponytail: chưa theo tần suất dùng, chưa sửa lỗi gõ; INT-017 thay.
template <class Entry>
int score(const Entry& e, const Query& q)
{
    if (q.folded.isEmpty())
        return 0;
    if (e.name == q.upper || e.aliases.contains(q.aliasExact))
        return 100;
    if (e.keywords.contains(q.keyExact) || e.folded == q.folded)
        return 90;
    if (e.name.startsWith(q.upper))
        return 80;
    if (e.aliases.contains(q.aliasPrefix))
        return 70;
    if (q.folded.size() < 2)
        return 0;
    if (e.keywords.contains(q.keyPrefix))
        return 60;
    if (!q.words.empty() && std::ranges::all_of(q.words, [&](const Query::Word& w) {
            return e.words.contains(w.space) || e.keywords.contains(w.comma) || e.keywords.contains(w.space);
        }))
        return 50;
    if (q.upper.size() > 2 && e.name.contains(q.upper))
        return 40;
    return 0;
}

} // namespace

// Kết quả đang hiện: vị trí trong chỉ mục; rỗng thì một dòng "không tìm thấy".
class CommandSearch::Model final : public QAbstractListModel {
public:
    explicit Model(CommandSearch* owner) : QAbstractListModel(owner), owner_(owner) {}

    void set(std::vector<int> rows)
    {
        beginResetModel();
        rows_ = std::move(rows);
        endResetModel();
    }

    int rowCount(const QModelIndex& parent = {}) const override
    {
        return parent.isValid() ? 0 : std::max(1, int(rows_.size()));
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        return entry(index.row()) ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        const Entry* e = entry(index.row());
        switch (role) {
        case kEntryRole:
            return e ? rows_[size_t(index.row())] : -1;
        case Qt::UserRole:
            return e ? QString(e->name) : QString();
        case Qt::DisplayRole:
            return e ? e->label
                     : CommandSearch::tr("Không tìm thấy “%1”. Thử từ khóa khác, ví dụ “đo”, “in”, “layer”.")
                           .arg(owner_->text().trimmed());
        case Qt::AccessibleTextRole: { // trình đọc màn hình: nhãn, lệnh, phím, vị trí, khóa
            if (!e)
                return data(index, Qt::DisplayRole);
            QStringList parts{e->label, e->name, ui::Menu::keyText(e->action)};
            if (owner_->locator_)
                parts << owner_->locator_(e->name);
            parts << edition::EditionGate::availableIn(ui::RichTooltip::info(e->action).need);
            parts.removeAll(QString());
            return parts.join(u", "_s);
        }
        default:
            return {};
        }
    }

private:
    const Entry* entry(int row) const
    {
        return row >= 0 && row < int(rows_.size()) ? &owner_->index()[size_t(rows_[size_t(row)])] : nullptr;
    }

    CommandSearch* owner_;
    std::vector<int> rows_;
};

// Dòng kết quả .sres .si: [biểu tượng] nhãn [huy hiệu khóa] … TÊN · viết tắt  phím / vị trí ribbon.
class CommandSearch::Delegate final : public QStyledItemDelegate {
public:
    explicit Delegate(CommandSearch* owner) : QStyledItemDelegate(owner), owner_(owner) {}

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        // Rộng 0: dòng rộng bằng popup (QListView). Cao: lề --sp-2, dòng nhãn, --sp-1, dòng vị trí.
        return {0, 2 * px(tok::sp_2) + QFontMetrics(theme::font(TextRole::Ui, Weight::Medium)).height() + px(tok::sp_1) +
                       QFontMetrics(theme::font(TextRole::Caption)).height()};
    }

    void paint(QPainter* p, const QStyleOptionViewItem& o, const QModelIndex& index) const override
    {
        const int pad = px(tok::sp_2), side = px(tok::sp_4), gap = px(tok::sp_4), ic = px(tok::ic);
        const QRect r = o.rect;
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const int pos = index.data(kEntryRole).toInt();
        if (pos < 0) { // .empty
            p->setFont(theme::font(TextRole::Ui));
            p->setPen(col(tok::c_text_2));
            p->drawText(r.adjusted(side, pad, -side, -pad), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                        index.data().toString());
            p->restore();
            return;
        }
        const Entry& e = owner_->index()[size_t(pos)];
        if (o.state & (QStyle::State_Selected | QStyle::State_MouseOver)) { // .si:hover, .si.is-hot
            const qreal rad = tm().px(tok::r);
            p->setPen(Qt::NoPen);
            p->setBrush(col(tok::c_accent_soft));
            p->drawRoundedRect(QRectF(r), rad, rad);
        }
        const QFont label = theme::font(TextRole::Ui, Weight::Medium), hot = theme::font(TextRole::Ui, Weight::Bold),
                    mono = theme::font(TextRole::MonoCaption), caption = theme::font(TextRole::Caption);
        const QFontMetrics lfm(label), mfm(mono), cfm(caption);
        const QRect line1(r.left() + side + ic + gap, r.top() + pad, r.width() - 2 * side - ic - gap, lfm.height());
        const QRect line2(line1.left(), line1.bottom() + 1 + px(tok::sp_1), line1.width(), cfm.height());
        const qreal dpr = p->device()->devicePixelRatio();
        p->drawPixmap(r.left() + side, line1.top() + (line1.height() - ic) / 2,
                      e.action->icon().pixmap(QSize(ic, ic), dpr)); // .ic --c-text-2 (IconProvider, Normal)

        // Phải: TÊN · viết tắt (Mono --c-text-2), phím (Mono --c-text-3, như .menu .k).
        int right = line1.right() + 1;
        const auto drawRight = [&](const QString& s, tok::ColorId color) {
            const int w = mfm.horizontalAdvance(s);
            p->setFont(mono);
            p->setPen(col(color));
            p->drawText(QRect(right - w, line1.top(), w, line1.height()), Qt::AlignVCenter | Qt::TextSingleLine, s);
            right -= w + gap;
        };
        if (const QString keys = ui::Menu::keyText(e.action); !keys.isEmpty())
            drawRight(keys, tok::c_text_3);
        QString code = e.name; // "CIRCLE · C": viết tắt đầu tiên
        if (!e.aliases.isEmpty())
            code += u" · "_s + e.aliases.sliced(1, e.aliases.indexOf(u',', 1) - 1);
        drawRight(code, tok::c_text_2);

        // Nhãn, phần khớp chuỗi gõ tô --c-accent đậm (.sres mark); thiếu chỗ thì rút gọn, không tô. Gấp dấu giữ độ dài
        // với chữ tiếng Việt NFC (một chữ có dấu → một chữ cái); khác độ dài thì không tô.
        const ui::Edition need = ui::RichTooltip::info(e.action).need;
        const QSize badge = ui::RichTooltip::badgeSize(need);
        const int room = right - line1.left() - (badge.isEmpty() ? 0 : badge.width() + px(tok::sp_3));
        const QString q = text::foldForSearch(owner_->text().trimmed());
        const qsizetype at = q.isEmpty() || e.folded.size() != e.label.size() ? -1 : e.folded.indexOf(q);
        const QString parts[3] = {at < 0 ? e.label : e.label.first(at), at < 0 ? QString() : e.label.sliced(at, q.size()),
                                  at < 0 ? QString() : e.label.sliced(at + q.size())};
        const int full = lfm.horizontalAdvance(parts[0]) + QFontMetrics(hot).horizontalAdvance(parts[1]) +
                         lfm.horizontalAdvance(parts[2]);
        int x = line1.left();
        const auto drawPart = [&](const QString& s, bool match) {
            p->setFont(match ? hot : label);
            p->setPen(col(match ? tok::c_accent : tok::c_text));
            const int w = QFontMetrics(p->font()).horizontalAdvance(s);
            p->drawText(QRect(x, line1.top(), w, line1.height()), Qt::AlignVCenter | Qt::TextSingleLine, s);
            x += w;
        };
        if (full <= room) {
            for (int i = 0; i < 3; ++i)
                drawPart(parts[i], i == 1);
        } else {
            drawPart(lfm.elidedText(e.label, Qt::ElideRight, room), false);
        }
        if (!badge.isEmpty()) // .ed.xs "Pro" | "Viewer Pro": chữ, không chỉ màu
            ui::RichTooltip::paintBadge(*p, QRect(QPoint(x + px(tok::sp_3), line1.center().y() - badge.height() / 2),
                                                  badge), need);

        // Dòng 2: vị trí ribbon (--fs-xs --c-text-3).
        if (owner_->locator_) {
            p->setFont(caption);
            p->setPen(col(tok::c_text_3));
            p->drawText(line2, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                        cfm.elidedText(owner_->locator_(e.name), Qt::ElideRight, line2.width()));
        }
        p->restore();
    }

private:
    CommandSearch* owner_;
};

// Popup .sres: nền --c-raised, viền 1 px --c-border-ctl như popup của ComboBox.
// ponytail: chưa bo góc --r-lg, chưa bóng --sh-pop (cần cửa sổ trong suốt), như popup của ComboBox (UI0-106).
class CommandSearch::View final : public QListView {
public:
    View()
    {
        setUniformItemSizes(true);
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setMouseTracking(true);
        viewport()->setAttribute(Qt::WA_Hover); // rê dòng: nền như dòng đang chọn
        viewport()->setAutoFillBackground(false);
        setViewportMargins(px(tok::sp_2), kCompleterPad, px(tok::sp_2), kCompleterPad);
    }

protected:
    bool event(QEvent* e) override
    {
        if (e->type() != QEvent::Paint)
            return QListView::event(e);
        QPainter p(this); // khung quanh viewport (viewport tự vẽ các dòng)
        p.fillRect(rect(), col(tok::c_raised));
        p.setPen(col(tok::c_border_ctl));
        p.drawRect(rect().adjusted(0, 0, -1, -1));
        return true;
    }
};

CommandSearch::CommandSearch(actions::ActionRegistry& actions, QWidget* parent)
    : SearchField(parent), actions_(actions), model_(new Model(this)), view_(new View), completer_(new QCompleter(this))
{
    completer_->setModel(model_);
    completer_->setCompletionMode(QCompleter::UnfilteredPopupCompletion); // model đã xếp hạng, không lọc lại
    completer_->setMaxVisibleItems(kMaxResults);
    completer_->setWrapAround(false); // ↑ ở dòng đầu giữ dòng đầu: Enter luôn có dòng để chạy
    completer_->setWidget(this);      // không setCompleter: QLineEdit sẽ thay chữ gõ bằng dòng đang chọn
    completer_->setPopup(view_);      // QCompleter sở hữu view, giữ focus và bộ gõ ở ô
    view_->setItemDelegate(new Delegate(this));
    view_->installEventFilter(this); // sau bộ lọc của QCompleter: Enter, Esc tới đây trước
    connect(completer_, qOverload<const QModelIndex&>(&QCompleter::activated), this,
            [this](const QModelIndex& i) { run(i.data(Qt::UserRole).toString()); });
    connect(this, &QLineEdit::textEdited, this, &CommandSearch::refresh);
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget* old, QWidget* now) {
        if (now == this && old && old != view_ && old->window() == window())
            back_ = old;
    });
    retranslate();
}

CommandSearch::~CommandSearch()
{
    delete completer_; // popup trước model, delegate
}

void CommandSearch::setLocator(Locator locator)
{
    locator_ = std::move(locator);
    view_->viewport()->update();
}

const std::vector<CommandSearch::Entry>& CommandSearch::index() const
{
    if (!index_.empty())
        return index_;
    const auto terms = searchTerms();
    index_.reserve(terms.size());
    for (const SearchTerms& t : terms) {
        const QLatin1StringView name(t.name);
        QAction* a = actions_.action(name);
        if (!a)
            continue; // danh mục lệch registry: ctest titlebar_prototype_check, actions_prototype_check bắt
        const QString label = a->text(), folded = text::foldForSearch(label);
        index_.push_back({name, QLatin1StringView(t.aliases), QLatin1StringView(t.keywords), label, folded,
                          wordsOf(folded), a});
    }
    return index_;
}

std::vector<int> CommandSearch::rank(const QString& query) const
{
    const auto& all = index();
    const Query q(query);
    std::vector<std::pair<int, int>> hits; // (−điểm, vị trí): sắp tăng = điểm cao trước, rồi thứ tự danh mục
    for (int i = 0; i < int(all.size()); ++i)
        if (const int s = score(all[size_t(i)], q))
            hits.emplace_back(-s, i);
    const auto n = std::min(hits.size(), size_t(kMaxResults));
    std::partial_sort(hits.begin(), hits.begin() + ptrdiff_t(n), hits.end());
    std::vector<int> out;
    for (size_t k = 0; k < n; ++k)
        out.push_back(hits[k].second);
    return out;
}

QStringList CommandSearch::search(const QString& query) const
{
    QStringList out;
    for (int i : rank(query))
        out << index()[size_t(i)].name;
    return out;
}

QAbstractItemView* CommandSearch::popup() const
{
    return view_;
}

void CommandSearch::refresh()
{
    if (text().trimmed().isEmpty()) {
        view_->hide();
        return;
    }
    model_->set(rank(text()));
    // .sres: rộng 440 px, mép phải theo mép phải ô, cách ô --sp-2 (QCompleter đặt popup dưới đáy hình chữ nhật này).
    completer_->complete(QRect(width() - kPopupWidth, 0, kPopupWidth, height() + px(tok::sp_2)));
    view_->setCurrentIndex(view_->model()->index(0, 0)); // dòng đầu: Enter chạy kết quả tốt nhất
    ui::probe::untilFrame(ui::probe::Op::Autocomplete, view_); // DoD-L: danh sách cập nhật sau mỗi phím
}

void CommandSearch::run(const QString& command)
{
    QAction* a = command.isEmpty() ? nullptr : actions_.action(command);
    if (!a)
        return; // dòng "không tìm thấy"
    dismiss();
    a->trigger(); // ActionRegistry: cổng phiên bản (lệnh khóa → trang nâng cấp), rồi chuỗi lệnh tới INT-002
}

void CommandSearch::dismiss()
{
    view_->hide();
    clear();
    if (back_)
        back_->setFocus(Qt::OtherFocusReason);
    else
        clearFocus();
}

bool CommandSearch::handleKey(QKeyEvent* e)
{
    switch (e->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (view_->isVisible())
            run(view_->currentIndex().data(Qt::UserRole).toString());
        else if (const auto rows = rank(text()); !rows.empty())
            run(index()[size_t(rows.front())].name);
        return true;
    case Qt::Key_Escape:
        dismiss();
        return true;
    case Qt::Key_Down: // popup đã đóng (bấm ra ngoài) mà còn chữ: mở lại
        if (view_->isVisible() || text().trimmed().isEmpty())
            return false;
        refresh();
        return true;
    default:
        return false;
    }
}

bool CommandSearch::event(QEvent* e)
{
    // Esc là của ô (đóng, trả focus) cả khi ô trống: không thành phím tắt của cửa sổ.
    if (e->type() == QEvent::ShortcutOverride && static_cast<QKeyEvent*>(e)->key() == Qt::Key_Escape) {
        e->accept();
        return true;
    }
    return SearchField::event(e);
}

void CommandSearch::keyPressEvent(QKeyEvent* e)
{
    if (!handleKey(e))
        SearchField::keyPressEvent(e);
}

bool CommandSearch::eventFilter(QObject* watched, QEvent* e)
{
    // Phím khi popup mở tới popup trước (QCompleter chuyển phần còn lại về ô); ↑↓ PgUp PgDn để QCompleter chọn dòng.
    if (watched == view_ && e->type() == QEvent::KeyPress && handleKey(static_cast<QKeyEvent*>(e)))
        return true;
    return SearchField::eventFilter(watched, e);
}

void CommandSearch::retranslate()
{
    const QString name = tr("Tìm lệnh");
    setAccessibleName(name);
    setPlaceholderText(tr("Tìm lệnh: “đường tròn”, CIRCLE, C…"));
    view_->setAccessibleName(tr("Kết quả tìm lệnh"));
}

void CommandSearch::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange) {
        retranslate();
        index_.clear(); // nhãn của QAction đã dịch lại (ActionRegistry theo Translator::languageChanged)
        if (view_->isVisible())
            refresh();
    }
    SearchField::changeEvent(e);
}

} // namespace trion::shell
