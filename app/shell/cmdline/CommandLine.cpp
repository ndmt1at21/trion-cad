#include "app/shell/cmdline/CommandLine.h"

#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/cmdline/CommandInput.h"
#include "app/widgets/menu/Menu.h"

#include <QAbstractScrollArea>
#include <QAccessible>
#include <QBoxLayout>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>

#include <algorithm>
#include <climits>
#include <deque>
#include <utility>
#include <vector>

using namespace Qt::StringLiterals;

namespace trion::ui {

namespace {

using theme::TextRole;
using theme::Weight;

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

// Giá trị của prototype không có token riêng (css/app.css .cmd, .cmd-hist, .cmd-in, .chip, .ic.xs).
constexpr int kBorder = 1;    // viền trên của .cmd, viền .cmd-in, gạch chân chữ tắt
constexpr int kTight = 3;     // .cmd-hist padding-top, .cmd-in margin-top, khoảng trong .chip, gạch chân cách chân chữ 2 px
constexpr int kItemGap = 5;   // gap của .cmd-pr và .cmd-hist p
constexpr int kChipH = 20;    // .chip height
constexpr int kChevron = 9;   // mũi tên .ic.xs: 9 px, nét --sw × 1,5 trên lưới 16
constexpr int kInputMin = 60; // .cmd-in input min-width
constexpr int kTextInset = 2; // QLineEdit lùi chữ 2 px (QLineEditPrivate::horizontalMargin): placeholder thẳng chữ gõ
constexpr int kMaxLines = 10; // 07.1 §8: lịch sử 0–10 dòng
// prototype: 12 lệnh, 20 giá trị gần đây; menu chuột phải hiện 8.
constexpr int kRecentMax = 12;
constexpr int kEnteredMax = 20;
constexpr int kMenuItems = 8;
// ponytail: giữ 100.000 dòng cuối trong bộ nhớ (F2 của UIA-007 mở 100.000 dòng); INT-017 History thay.
constexpr std::size_t kHistoryMax = 100000;

struct Line {
    QString text;
    CommandLine::Kind kind = CommandLine::Kind::Message;
    int boldFrom = 0, boldTo = 0; // đoạn in đậm --c-text (tên lệnh)
    int monoFrom = -1;            // từ đây tới hết là chữ Mono (giá trị đã nhập); -1: không có
};

// Đưa lên đầu, bỏ trùng, giữ tối đa max mục.
void remember(QStringList& list, const QString& value, int max)
{
    list.removeAll(value);
    list.prepend(value);
    while (list.size() > max)
        list.removeLast();
}

void announce(QObject* source, const QString& text, QAccessible::AnnouncementPoliteness politeness)
{
    QAccessibleAnnouncementEvent ev(source, text);
    ev.setPoliteness(politeness);
    QAccessible::updateAccessibility(&ev);
}

// Lời nhắc dạng chữ cho trình đọc màn hình: "CIRCLE Chỉ định bán kính hoặc [Đường kính · D] <10.0000>:".
QString describe(const CommandLine::Prompt& p)
{
    QString s = p.command.isEmpty() ? p.text : p.command + u' ' + p.text;
    QStringList options;
    for (const CommandLine::Keyword& k : p.keywords)
        if (!k.hidden)
            options << k.label + u" · "_s + k.key;
    if (!options.isEmpty())
        s += u" ["_s + options.join(u" / "_s) + u']';
    if (!p.defaultValue.isEmpty())
        s += u" <"_s + p.defaultValue + u'>';
    return s + u':';
}

} // namespace

// .cmd-hist: tự vẽ các dòng đang thấy; không widget, không item theo dòng; thêm dòng O(1).
class CommandLine::HistoryView final : public QAbstractScrollArea {
public:
    explicit HistoryView(QWidget* parent) : QAbstractScrollArea(parent)
    {
        setObjectName(u"history"_s);
        setFrameShape(QFrame::NoFrame);
        setFocusPolicy(Qt::NoFocus);
        // Như prototype: không thanh cuộn, lăn chuột xem dòng cũ; lịch sử đầy đủ có tìm, sao chép là F2 (UIA-007).
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        viewport()->setAutoFillBackground(false); // nền --c-base của CommandLine
    }

    std::deque<Line> lines;

    void append(Line line)
    {
        lines.push_back(std::move(line));
        if (lines.size() > kHistoryMax)
            lines.pop_front();
        updateRange();
    }

    // Giá trị thanh cuộn = dòng trên cùng. Thêm dòng, đổi cỡ: về cuối, dòng mới nhất luôn hiện.
    void updateRange()
    {
        const int rows = std::max(1, viewport()->height() / std::max(1, px(tok::h_cmdl)));
        QScrollBar* bar = verticalScrollBar();
        bar->setRange(0, std::max(0, int(lines.size()) - rows));
        bar->setPageStep(rows);
        bar->setValue(bar->maximum());
        viewport()->update();
    }

protected:
    void resizeEvent(QResizeEvent* e) override
    {
        QAbstractScrollArea::resizeEvent(e);
        updateRange();
    }
    void scrollContentsBy(int, int) override { viewport()->update(); }
    void paintEvent(QPaintEvent* e) override;
};

void CommandLine::HistoryView::paintEvent(QPaintEvent*)
{
    QPainter p(viewport());
    const int h = px(tok::h_cmdl);
    const QFont ui = theme::font(TextRole::Ui), bold = theme::font(TextRole::Ui, Weight::Bold),
                mono = theme::font(TextRole::Mono);
    const QFontMetrics fu(ui), fb(bold), fm(mono);
    const bool enabled = isEnabled();
    int y = 0;
    for (auto i = std::size_t(verticalScrollBar()->value()); i < lines.size() && y < viewport()->height(); ++i, y += h) {
        const Line& l = lines[i];
        const bool error = l.kind == Kind::Error;
        const QColor ink = !enabled ? col(tok::c_text_dis) : error ? col(tok::c_danger) : col(tok::c_text_2);
        const int base = y + (h + fu.ascent() - fu.descent()) / 2; // chung một đường chân chữ cho mọi font
        int x = px(tok::sp_5);
        if (error) { // biểu tượng lỗi: không chỉ bằng màu
            const int ic = px(tok::ic);
            QPixmap icon = theme::IconProvider::instance().pixmap(u"error"_s, ic, devicePixelRatioF(),
                                                                   theme::IconState::Normal);
            QPainter tint(&icon);
            tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
            tint.fillRect(icon.rect(), ink);
            tint.end();
            p.drawPixmap(x, y + (h - ic) / 2, icon);
            x += ic + kItemGap;
        }
        const auto run = [&](int from, int to, const QFont& font, const QFontMetrics& metrics, const QColor& color) {
            if (from >= to)
                return;
            const QString s = l.text.mid(from, to - from);
            p.setFont(font);
            p.setPen(color);
            p.drawText(QPoint(x, base), s);
            x += metrics.horizontalAdvance(s);
        };
        const int end = int(l.text.size()), mono0 = l.monoFrom < 0 ? end : l.monoFrom;
        run(0, l.boldFrom, ui, fu, ink);
        run(l.boldFrom, l.boldTo, bold, fb, enabled && !error ? col(tok::c_text) : ink);
        run(l.boldTo, mono0, ui, fu, ink);
        run(mono0, end, mono, fm, ink);
    }
}

// Dấu nhắc trong .cmd-in: ">>", mũi tên, tên lệnh, lời nhắc, chip, <mặc định>, ":" (renderPrompt của prototype).
class CommandLine::PromptView final : public QWidget {
public:
    explicit PromptView(CommandLine* owner) : QWidget(owner), owner_(owner)
    {
        setObjectName(u"prompt"_s);
        setMouseTracking(true);
        // Ngang: co lại khi hẹp (lời nhắc rút gọn "…", ô nhập giữ ≥ 60 px); dọc: cao bằng khung.
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    }

    struct Hit {
        QRect rect;
        QString key;
    };

    // Đặt các phần từ trái sang trong bề rộng width; vẽ nếu có p; ô chip vào hits nếu có. Trả bề rộng cần để hiện đủ.
    int place(QPainter* p, int width, std::vector<Hit>* hits) const;

    QSize sizeHint() const override { return {place(nullptr, INT_MAX, nullptr), kChipH}; }
    QSize minimumSizeHint() const override { return {kChevron, kChipH}; }

    int hot = -1, down = -1; // chip đang rê, đang nhấn (thứ tự chip)

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        place(&p, width(), nullptr);
    }
    void mouseMoveEvent(QMouseEvent* e) override { setHot(chipAt(e->position().toPoint())); }
    void leaveEvent(QEvent*) override { setHot(-1); }
    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton) {
            down = chipAt(e->position().toPoint());
            update();
        }
    }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        QString key;
        const int at = chipAt(e->position().toPoint(), &key);
        const bool click = e->button() == Qt::LeftButton && down >= 0 && at == down;
        down = -1;
        update();
        if (click)
            owner_->submitToken(key); // như gõ chữ tắt rồi Enter
    }

private:
    int chipAt(QPoint pos, QString* key = nullptr) const
    {
        std::vector<Hit> hits;
        place(nullptr, width(), &hits);
        for (std::size_t i = 0; i < hits.size(); ++i)
            if (hits[i].rect.contains(pos)) {
                if (key)
                    *key = hits[i].key;
                return int(i);
            }
        return -1;
    }
    void setHot(int chip)
    {
        if (chip != hot) {
            hot = chip;
            update();
        }
    }

    CommandLine* owner_;
};

int CommandLine::PromptView::place(QPainter* p, int width, std::vector<Hit>* hits) const
{
    struct Piece {
        enum Type : std::uint8_t { Text, Chevron, Chip } type;
        QString text; // Chip: nhãn
        QString key;  // Chip: chữ tắt
        QFont font;
        QColor color;
        int gap;      // khoảng trước phần này
        int width;
    };
    const std::optional<Prompt>& pr = owner_->prompt_;
    const bool enabled = isEnabled();
    const QColor ink = col(enabled ? tok::c_text : tok::c_text_dis), faint = col(enabled ? tok::c_text_3 : tok::c_text_dis);
    const QFont ui = theme::font(TextRole::Ui), label = theme::font(TextRole::Label),
                keyFont = theme::font(TextRole::MonoCaption);
    const QFontMetrics fu(ui), fl(label), fk(keyFont);
    const int gap = px(tok::sp_3), pad = px(tok::sp_4);
    const QString dot = u"·"_s;

    std::vector<Piece> v;
    const auto text = [&](const QString& s, const QFont& f, const QColor& c, int before) {
        v.push_back({Piece::Text, s, {}, f, c, before, QFontMetrics(f).horizontalAdvance(s)});
    };
    if (pr && pr->nested)
        text(u">>"_s, ui, faint, gap);
    v.push_back({Piece::Chevron, {}, {}, {}, enabled ? col(tok::c_accent) : faint, gap, kChevron});
    int textAt = -1;
    if (pr) {
        if (!pr->command.isEmpty())
            text(pr->command, theme::font(TextRole::Ui, Weight::Bold), ink, gap);
        bool first = true; // phần đầu của .cmd-pr cách phần trước --sp-3, các phần sau 5 px
        const auto next = [&] { return std::exchange(first, false) ? gap : kItemGap; };
        const bool chips = std::any_of(pr->keywords.cbegin(), pr->keywords.cend(), [](const Keyword& k) { return !k.hidden; });
        const bool def = !pr->defaultValue.isEmpty();
        if (!pr->text.isEmpty()) {
            textAt = int(v.size());
            text(chips || def ? pr->text : pr->text + u':', ui, ink, next());
        }
        for (const Keyword& k : pr->keywords)
            if (!k.hidden)
                v.push_back({Piece::Chip, k.label, k.key, label, ink, next(),
                             2 * pad + fl.horizontalAdvance(k.label) + kTight + fl.horizontalAdvance(dot) + kTight
                                 + fk.horizontalAdvance(k.key)});
        if (def)
            text(u'<' + pr->defaultValue + u'>', theme::font(TextRole::Mono), ink, next());
        if (chips || def)
            text(u":"_s, ui, ink, v.back().type == Piece::Chip ? kItemGap : 0);
    }
    v.front().gap = 0;
    int need = 0;
    for (const Piece& pc : v)
        need += pc.gap + pc.width;
    if (!p && !hits)
        return need;
    if (need > width && textAt >= 0) { // thiếu chỗ: rút gọn lời nhắc trước, chip và mặc định giữ nguyên
        Piece& t = v[std::size_t(textAt)];
        t.width = std::max(0, t.width - (need - width));
        t.text = QFontMetrics(t.font).elidedText(t.text, Qt::ElideRight, t.width);
    }

    const int h = height(), base = (h + fu.ascent() - fu.descent()) / 2;
    const qreal radius = tm().px(tok::r);
    int x = 0, chip = 0;
    for (const Piece& pc : v) {
        x += pc.gap;
        if (pc.type == Piece::Chip) {
            const QRect r(x, (h - kChipH) / 2, pc.width, kChipH);
            if (hits)
                hits->push_back({r, pc.key});
            if (p) { // .chip: nền --c-active; rê/nhấn --c-press viền trong --c-border-ctl
                const bool lit = enabled && (chip == hot || chip == down);
                p->setPen(lit ? QPen(col(tok::c_border_ctl), kBorder) : QPen(Qt::NoPen));
                p->setBrush(col(lit ? tok::c_press : tok::c_active));
                p->drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
                const int cb = r.top() + (kChipH + fl.ascent() - fl.descent()) / 2;
                int cx = r.left() + pad;
                p->setFont(label);
                p->setPen(pc.color);
                p->drawText(QPoint(cx, cb), pc.text);
                cx += fl.horizontalAdvance(pc.text) + kTight;
                p->setPen(faint);
                p->drawText(QPoint(cx, cb), dot);
                cx += fl.horizontalAdvance(dot) + kTight;
                p->setFont(keyFont);
                p->setPen(pc.color);
                p->drawText(QPoint(cx, cb), pc.key);
                p->fillRect(QRect(cx, cb + kTight - 1, fk.horizontalAdvance(pc.key), kBorder), pc.color); // gạch chân
            }
            ++chip;
        } else if (p && pc.type == Piece::Chevron) {
            const QPointF c(x + kChevron / 2.0, h / 2.0);
            const qreal u = kChevron / 16.0; // một đơn vị của lưới biểu tượng 16 px
            p->setPen(QPen(pc.color, tok::num(tok::sw) * 1.5 * u, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
            p->setBrush(Qt::NoBrush);
            p->drawPolyline(QList<QPointF>{c + QPointF(-2 * u, -4 * u), c + QPointF(2 * u, 0), c + QPointF(-2 * u, 4 * u)});
        } else if (p) {
            p->setFont(pc.font);
            p->setPen(pc.color);
            p->drawText(QPoint(x, base), pc.text);
        }
        x += pc.width;
    }
    return need;
}

CommandLine::CommandLine(QWidget* parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    history_ = new HistoryView(this);
    promptView_ = new PromptView(this);
    input_ = new CommandInput(this);
    input_->setContextMenuPolicy(Qt::NoContextMenu); // menu của dòng lệnh thay menu sửa chữ (prototype: cả vùng .cmd)
    input_->installEventFilter(this);
    // Nhấp vào lịch sử, khung, chip: focus về ô nhập; setFocus() của cửa sổ chính cũng vào ô nhập.
    setFocusPolicy(Qt::ClickFocus);
    setFocusProxy(input_);

    frame_ = new QHBoxLayout;
    frame_->addWidget(promptView_);
    frame_->addWidget(input_, 1);
    auto* column = new QVBoxLayout(this);
    column->addWidget(history_);
    column->addLayout(frame_);

    connect(input_, &QLineEdit::textChanged, this, &CommandLine::textChanged);
    connect(&settings::Store::instance(), &settings::Store::changed, this, [this](const QString& key) {
        if (key == QLatin1StringView(kHistoryLines.key))
            relayout();
    });
    connect(&tm(), &theme::ThemeManager::changed, this, &CommandLine::relayout); // mật độ: cao dòng, cao khung
    relayout();
    retranslate();
    updateAwaiting();
    appendHistory(tr("TrionCAD đã sẵn sàng. Gõ lệnh như AutoCAD, hoặc gõ tiếng Việt (ví dụ “đo”, “in”) để tìm lệnh."));
}

CommandLine::~CommandLine() = default;

void CommandLine::setPrompt(const Prompt& prompt)
{
    if (!prompt.nested && !prompt.command.isEmpty()) {
        // Lệnh vừa gõ ở dấu nhắc ("L") chạy thành prompt.command ("LINE"): danh sách gần đây giữ tên lệnh. Lệnh từ
        // ribbon, menu cũng vào đây (lặp bằng Enter). ponytail: suy từ prompt; INT-017 ghi lệnh đã chạy từ mọi nguồn.
        if (!typedCommand_.isEmpty())
            recent_.removeAll(typedCommand_);
        remember(recent_, prompt.command, kRecentMax);
    }
    typedCommand_.clear();
    prompt_ = prompt;
    promptChanged();
}

void CommandLine::clearPrompt()
{
    typedCommand_.clear();
    prompt_.reset();
    promptChanged();
}

void CommandLine::promptChanged()
{
    browse_ = -1;
    promptView_->hot = promptView_->down = -1;
    promptView_->updateGeometry();
    promptView_->update();
    const QString say = prompt_ ? describe(*prompt_) : QString();
    promptView_->setAccessibleName(say);
    if (prompt_)
        announce(input_, say, QAccessible::AnnouncementPoliteness::Polite);
    retranslate();
    updateAwaiting();
}

void CommandLine::appendHistory(const QString& text, Kind kind)
{
    history_->append({text, kind});
    if (kind != Kind::Echo)
        announce(history_, text,
                 kind == Kind::Error ? QAccessible::AnnouncementPoliteness::Assertive
                                     : QAccessible::AnnouncementPoliteness::Polite);
}

int CommandLine::historyCount() const
{
    return int(history_->lines.size());
}

QString CommandLine::historyLine(int index) const
{
    return index >= 0 && index < historyCount() ? history_->lines[std::size_t(index)].text : QString();
}

QString CommandLine::historyText() const
{
    QStringList all;
    all.reserve(historyCount());
    for (const Line& l : history_->lines)
        all << l.text;
    return all.join(u'\n');
}

void CommandLine::setCompact(bool compact)
{
    compact_ = compact;
    relayout();
}

QRect CommandLine::keywordRect(const QString& key) const
{
    std::vector<PromptView::Hit> hits;
    promptView_->place(nullptr, promptView_->width(), &hits);
    for (const PromptView::Hit& h : hits)
        if (h.key == key)
            return h.rect.translated(promptView_->pos());
    return {};
}

QString CommandLine::resolve(const Prompt& prompt, const QString& text)
{
    const auto bare = [](const QString& s) { return QStringView(s).startsWith(u'_') ? QStringView(s).mid(1) : QStringView(s); };
    const QStringView typed = bare(text);
    if (typed.isEmpty())
        return text;
    for (const Keyword& k : prompt.keywords) {
        const QStringView global = bare(k.global);
        if (typed.compare(k.key, Qt::CaseInsensitive) == 0
            || (!global.isEmpty() && typed.compare(global, Qt::CaseInsensitive) == 0))
            return k.key;
    }
    return text;
}

bool CommandLine::spaceIsText() const
{
    return prompt_ ? prompt_->allowSpace : input_->text().startsWith(u'(');
}

bool CommandLine::ownsKey(const QKeyEvent* e) const
{
    if (e->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        return false;
    switch (e->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return true;
    case Qt::Key_Space:
        return !spaceIsText();
    case Qt::Key_Escape:
    case Qt::Key_Up:
    case Qt::Key_Down:
        return true;
    default:
        return false;
    }
}

bool CommandLine::eventFilter(QObject* watched, QEvent* e)
{
    if (watched == input_) {
        switch (e->type()) {
        case QEvent::FocusIn:
        case QEvent::FocusOut:
            update(frameRect()); // .cmd-in:focus-within
            break;
        case QEvent::InputMethod: // đang soạn chữ bằng bộ gõ (tắt bỏ qua bộ gõ): ẩn placeholder như QLineEdit
            composing_ = !static_cast<QInputMethodEvent*>(e)->preeditString().isEmpty();
            break;
        case QEvent::ShortcutOverride: // phím của dòng lệnh thắng phím tắt toàn cục (Esc, Space…)
            if (ownsKey(static_cast<QKeyEvent*>(e))) {
                e->accept();
                return true;
            }
            break;
        case QEvent::KeyPress: {
            const auto* k = static_cast<QKeyEvent*>(e);
            if (!ownsKey(k))
                break;
            if (k->key() == Qt::Key_Escape)
                escape();
            else if (k->key() == Qt::Key_Up || k->key() == Qt::Key_Down)
                browse(k->key() == Qt::Key_Up ? 1 : -1);
            else
                submit();
            return true;
        }
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, e);
}

void CommandLine::submit()
{
    const QString text = input_->text();
    if (prompt_) {
        submitToken(prompt_->allowSpace ? text : text.trimmed());
        return;
    }
    if (const QString name = text.trimmed(); !name.isEmpty())
        submitToken(name);
    else if (!recent_.isEmpty()) // ponytail: lặp lệnh ở đây như prototype CL.enter; INT-002 thay (lệnh vừa chạy)
        submitToken(recent_.front());
}

void CommandLine::submitToken(const QString& typed)
{
    input_->clear();
    browse_ = -1;
    Line echo;
    QString token = typed;
    if (prompt_) {
        token = resolve(*prompt_, typed);
        // "<b>LINE</b> Chỉ định điểm đầu: <mono>100,200</mono>" (CL.submit của prototype)
        const QString head = prompt_->command.isEmpty() ? prompt_->text : prompt_->command + u' ' + prompt_->text;
        echo = {head + u": "_s, Kind::Echo, 0, int(prompt_->command.size())};
        echo.monoFrom = int(echo.text.size());
        echo.text += token;
        if (!token.isEmpty())
            remember(entered_, token, kEnteredMax);
    } else {
        // "Lệnh: <b>LINE</b>"
        const QString pattern = tr("Lệnh: %1");
        echo.text = pattern.arg(token);
        echo.kind = Kind::Echo;
        echo.boldFrom = int(std::max<qsizetype>(pattern.indexOf(u"%1"), 0));
        echo.boldTo = echo.boldFrom + int(token.size());
        if (!token.startsWith(u'(')) { // biểu thức LISP không phải lệnh để lặp
            typedCommand_ = token.toUpper();
            remember(recent_, typedCommand_, kRecentMax);
        }
    }
    history_->append(std::move(echo));
    emit submitted(token);
}

void CommandLine::escape()
{
    input_->clear();
    browse_ = -1;
    if (prompt_)
        emit cancelRequested();
    else
        emit deselectRequested();
}

void CommandLine::browse(int step)
{
    const QStringList& list = prompt_ ? entered_ : recent_;
    if (list.isEmpty())
        return;
    browse_ = std::clamp(browse_ + step, -1, int(list.size()) - 1);
    input_->setText(browse_ < 0 ? QString() : list[browse_]);
}

void CommandLine::textChanged(const QString& text)
{
    updateAwaiting();
    if (commaHinted_ || (prompt_ && prompt_->allowSpace))
        return;
    for (qsizetype i = 1; i < text.size(); ++i)
        if (text[i] == u',' && text[i - 1].isDigit()) { // 07.1 §8: nhắc một lần
            commaHinted_ = true;
            appendHistory(tr("Trong bản vẽ, dấu thập phân luôn là dấu chấm: gõ 12.5, không gõ 12,5."));
            return;
        }
}

void CommandLine::updateAwaiting()
{
    const bool awaiting = !prompt_ && !input_->text().startsWith(u'(');
    if (awaiting != input_->awaitingCommand())
        input_->setAwaitingCommand(awaiting);
}

void CommandLine::relayout()
{
    const int lines = std::clamp(settings::Store::instance().get(kHistoryLines), 0, kMaxLines);
    const int shown = compact_ ? std::min(lines, 1) : lines;
    const int gap = px(tok::sp_3);
    history_->setFixedHeight(shown * px(tok::h_cmdl));
    history_->setVisible(shown > 0);
    auto* column = static_cast<QVBoxLayout*>(layout());
    column->setContentsMargins(0, kBorder + kTight, 0, gap);
    column->setSpacing(kTight);
    frame_->setContentsMargins(gap + kBorder + px(tok::sp_4), kBorder, gap + kBorder + px(tok::sp_2), kBorder);
    frame_->setSpacing(gap);
    input_->setFixedHeight(px(tok::h_cmdin) - 2 * kBorder);
    input_->setMinimumWidth(kInputMin);
}

QString CommandLine::placeholder() const
{
    return tr("Gõ lệnh hoặc từ khóa tiếng Việt, ví dụ: đo, in, layer");
}

void CommandLine::retranslate()
{
    history_->setAccessibleName(tr("Lịch sử dòng lệnh"));
    input_->setAccessibleDescription(prompt_ ? describe(*prompt_) : placeholder());
    update(input_->geometry());
}

void CommandLine::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

QRect CommandLine::frameRect() const
{
    const int gap = px(tok::sp_3);
    return frame_->geometry().adjusted(gap, 0, -gap, 0);
}

void CommandLine::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), col(tok::c_base));
    p.fillRect(QRect(0, 0, width(), kBorder), col(tok::c_border));
    // .cmd-in: nền --c-raised, viền --c-border-ctl; ô nhập có focus: viền --c-focus
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(col(input_->hasFocus() ? tok::c_focus : tok::c_border_ctl), kBorder));
    p.setBrush(col(tok::c_raised));
    const qreal radius = tm().px(tok::r);
    p.drawRoundedRect(QRectF(frameRect()).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    if (!prompt_ && input_->text().isEmpty() && !composing_) {
        // .cmd-in input::placeholder: --f-ui --c-text-3, vẽ dưới ô nhập trong suốt (QLineEdit vẽ placeholder bằng
        // font Mono của ô). Ô nhập vẽ lại (chữ, preedit đổi) thì Qt vẽ lại cả phần này bên dưới nó.
        const QRect r = input_->geometry().adjusted(kTextInset, 0, 0, 0);
        p.setFont(theme::font(TextRole::Ui));
        p.setPen(col(isEnabled() ? tok::c_text_3 : tok::c_text_dis));
        p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(p.font()).elidedText(placeholder(), Qt::ElideRight, r.width()));
    }
}

void CommandLine::contextMenuEvent(QContextMenuEvent* e)
{
    auto* menu = new Menu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    const auto sub = [&](const QString& title, const QStringList& items, auto pick) {
        auto* m = new Menu(title, menu);
        for (const QString& item : items.mid(0, kMenuItems))
            connect(m->addAction(item), &QAction::triggered, this, [pick, item] { pick(item); });
        if (items.isEmpty())
            m->addAction(tr("Chưa có"))->setEnabled(false);
        menu->addMenu(m);
    };
    sub(tr("Lệnh gần đây"), recent_, [this](const QString& name) { emit commandRequested(u"^C^C"_s + name); });
    sub(tr("Giá trị đã nhập gần đây"), entered_, [this](const QString& value) {
        input_->setText(value);
        input_->setFocus(Qt::OtherFocusReason);
    });
    connect(menu->addAction(theme::IconProvider::icon(u"copyclip"_s), tr("Sao chép lịch sử")), &QAction::triggered,
            this, [this] { QGuiApplication::clipboard()->setText(historyText()); });
    menu->addSeparator();
    connect(menu->addAction(theme::IconProvider::icon(u"gear"_s), tr("Tùy chọn dòng lệnh…")), &QAction::triggered,
            this, [this] { emit commandRequested(u"^C^C_INPUTSEARCHOPTIONS"_s); });
    menu->popup(e->globalPos());
}

} // namespace trion::ui
