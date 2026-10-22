#include "app/shell/cmdline/CommandLine.h"

#include "app/core/edition/EditionGate.h"
#include "app/shell/cmdline/Autocomplete.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/cmdline/CommandInput.h"
#include "app/widgets/controls/Paint.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/assert.h"

#include <QAbstractButton>
#include <QAbstractScrollArea>
#include <QAccessible>
#include <QApplication>
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
#include <functional>
#include <memory>
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
constexpr int kIconButton = 20; // .ib.sm
constexpr int kMaxLines = 10; // 07.1 §8: lịch sử 0–10 dòng
// .tb-grip của prototype: chấm --c-text-3 bán kính 1 px, cách 4 px, chấm đầu ở y = 5, mờ .7
constexpr qreal kGripPitch = 4, kGripFirst = 5, kGripDot = 1, kGripAlpha = 0.7;
constexpr int kMaxTransparency = 90; // % (prototype: thanh trượt 0–90)
// prototype: 12 lệnh, 20 giá trị gần đây; menu chuột phải hiện 8.
constexpr int kRecentMax = 12;
constexpr int kEnteredMax = 20;
constexpr int kMenuItems = 8;
// ponytail: giữ 100.000 dòng cuối trong bộ nhớ (F2 của UIA-007 mở 100.000 dòng); INT-017 History thay.
constexpr std::size_t kHistoryMax = 100000;

// Chữ của dòng lệnh theo cỡ chữ người dùng chọn (kFontSize): font của vai trò nhân tỉ lệ z.
QFont zoomed(TextRole role, double z, Weight weight = Weight::Regular)
{
    QFont f = theme::font(role, weight);
    if (z != 1.0)
        f.setPointSizeF(f.pointSizeF() * z);
    return f;
}
int scaled(tok::PxId id, double z)
{
    return qRound(tm().px(id) * z);
}

// Nền khi nổi: --c-base trong suốt theo kFloatTransparency.
QColor floatBase()
{
    QColor c = col(tok::c_base);
    c.setAlphaF(1.0 - std::clamp(settings::Store::instance().get(kFloatTransparency), 0, kMaxTransparency) / 100.0);
    return c;
}

// Dòng có chip bấm được (.cmd-hist .chip): "Không có lệnh “LNE”. Ý bạn là: LINE, LEN?" (reportUnknown, câu dẫn màu lỗi),
// "Lệnh LINE có trong TrionCAD Pro. Dùng thử 30 ngày Tìm hiểu" (notifyLocked). Câu dẫn [0, headTo) màu theo Kind, phần
// sau --c-text-2.
struct Chips {
    int headTo = 0;
    struct Chip {
        int from, to;              // chữ của chip trong Line::text
        std::function<void()> run; // bấm
    };
    std::vector<Chip> chips;
};

struct Line {
    QString text;
    CommandLine::Kind kind = CommandLine::Kind::Message;
    int boldFrom = 0, boldTo = 0; // đoạn in đậm --c-text (tên lệnh)
    int monoFrom = -1;            // từ đây tới hết là chữ Mono (giá trị đã nhập); -1: không có
    std::shared_ptr<const Chips> chips{}; // dòng có chip; còn lại rỗng
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
    explicit HistoryView(CommandLine* owner) : QAbstractScrollArea(owner), owner_(owner)
    {
        setObjectName(u"history"_s);
        setFrameShape(QFrame::NoFrame);
        setFocusPolicy(Qt::NoFocus);
        // Như prototype: không thanh cuộn, lăn chuột xem dòng cũ; lịch sử đầy đủ có tìm, sao chép là F2 (UIA-007).
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        viewport()->setAutoFillBackground(false); // nền --c-base của CommandLine
        viewport()->setMouseTracking(true);       // rê gợi ý "Ý bạn là"
    }

    std::deque<Line> lines;
    // Chip gợi ý đang thấy, theo lần vẽ gần nhất; chip rê, nhấn theo (dòng, thứ tự chip).
    struct Hit {
        QRect rect;
        std::shared_ptr<const Chips> line;
        int chip;
    };
    using Key = std::pair<const Chips*, int>;
    std::vector<Hit> hits;
    Key hot{nullptr, -1}, down{nullptr, -1};

    void append(Line line)
    {
        lines.push_back(std::move(line));
        if (lines.size() > kHistoryMax)
            lines.pop_front();
        updateRange();
        emit owner_->historyAppended(lines.back().text);
    }

    // Giá trị thanh cuộn = dòng trên cùng. Thêm dòng, đổi cỡ: về cuối, dòng mới nhất luôn hiện.
    void updateRange()
    {
        const int rows = std::max(1, viewport()->height() / std::max(1, scaled(tok::h_cmdl, owner_->zoom_)));
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
    void mouseMoveEvent(QMouseEvent* e) override { setHot(keyAt(e->position().toPoint())); }
    void leaveEvent(QEvent*) override { setHot({nullptr, -1}); }
    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton) {
            down = keyAt(e->position().toPoint());
            viewport()->update();
        }
    }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        const Key at = keyAt(e->position().toPoint());
        const bool click = e->button() == Qt::LeftButton && at.first && at == down;
        down = {nullptr, -1};
        viewport()->update();
        if (click) { // như prototype: chip chạy lệnh (chép hàm: lịch sử có thể đổi trong lúc chạy)
            const std::function<void()> run = at.first->chips[std::size_t(at.second)].run;
            run();
        }
    }

private:
    Key keyAt(QPoint pos) const
    {
        for (const Hit& h : hits)
            if (h.rect.contains(pos))
                return {h.line.get(), h.chip};
        return {nullptr, -1};
    }
    void setHot(Key key)
    {
        if (key == hot)
            return;
        hot = key;
        if (key.first)
            viewport()->setCursor(Qt::PointingHandCursor);
        else
            viewport()->unsetCursor();
        viewport()->update();
    }

    CommandLine* owner_;
};

void CommandLine::HistoryView::paintEvent(QPaintEvent*)
{
    QPainter p(viewport());
    const double z = owner_->zoom_;
    if (owner_->floating_) // nổi: nền trong suốt; dưới là vùng vẽ
        p.fillRect(viewport()->rect(), floatBase());
    const int h = scaled(tok::h_cmdl, z);
    const QFont ui = zoomed(TextRole::Ui, z), bold = zoomed(TextRole::Ui, z, Weight::Bold),
                mono = zoomed(TextRole::Mono, z), label = zoomed(TextRole::Label, z);
    const QFontMetrics fu(ui), fb(bold), fm(mono), fl(label);
    const bool enabled = isEnabled();
    const qreal radius = tm().px(tok::r);
    hits.clear();
    int y = 0;
    for (auto i = std::size_t(verticalScrollBar()->value()); i < lines.size() && y < viewport()->height(); ++i, y += h) {
        const Line& l = lines[i];
        const bool error = l.kind == Kind::Error;
        const QColor ink = !enabled ? col(tok::c_text_dis) : error ? col(tok::c_danger) : col(tok::c_text_2);
        const int base = y + (h + fu.ascent() - fu.descent()) / 2; // chung một đường chân chữ cho mọi font
        int x = px(tok::sp_5);
        if (error) { // biểu tượng lỗi: không chỉ bằng màu
            const int ic = scaled(tok::ic, z);
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
        if (l.chips) { // câu dẫn (tên lệnh đậm), phần sau --c-text-2, chip
            const Chips& u = *l.chips;
            const QColor rest = enabled ? col(tok::c_text_2) : ink;
            run(0, l.boldFrom, ui, fu, ink);
            run(l.boldFrom, l.boldTo, bold, fb, enabled && !error ? col(tok::c_text) : ink);
            run(l.boldTo, u.headTo, ui, fu, ink);
            int at = u.headTo;
            for (int c = 0; c < int(u.chips.size()); ++c) {
                const Chips::Chip& chip = u.chips[std::size_t(c)];
                run(at, chip.from, ui, fu, rest);
                const QString s = l.text.mid(chip.from, chip.to - chip.from);
                // .cmd-hist .chip: cao bằng dòng, nền --c-active; rê, nhấn --c-press viền trong --c-border-ctl
                const QRect r(x, y, 2 * px(tok::sp_4) + fl.horizontalAdvance(s), h);
                const bool lit = enabled && (Key{&u, c} == hot || Key{&u, c} == down);
                p.setRenderHint(QPainter::Antialiasing);
                p.setPen(lit ? QPen(col(tok::c_border_ctl), kBorder) : QPen(Qt::NoPen));
                p.setBrush(col(lit ? tok::c_press : tok::c_active));
                p.drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
                p.setFont(label);
                p.setPen(enabled ? col(tok::c_text) : ink);
                p.drawText(r, Qt::AlignCenter | Qt::TextSingleLine, s);
                hits.push_back({r, l.chips, c});
                x = r.right() + 1;
                at = chip.to;
            }
            run(at, end, ui, fu, rest);
            continue;
        }
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

    QSize sizeHint() const override { return {place(nullptr, INT_MAX, nullptr), chipH()}; }
    QSize minimumSizeHint() const override { return {kChevron, chipH()}; }
    int chipH() const { return qRound(kChipH * owner_->zoom_); }

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
    const double z = owner_->zoom_;
    const int chipH = qRound(kChipH * z);
    const QFont ui = zoomed(TextRole::Ui, z), label = zoomed(TextRole::Label, z),
                keyFont = zoomed(TextRole::MonoCaption, z);
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
            text(pr->command, zoomed(TextRole::Ui, z, Weight::Bold), ink, gap);
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
            text(u'<' + pr->defaultValue + u'>', zoomed(TextRole::Mono, z), ink, next());
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
            const QRect r(x, (h - chipH) / 2, pc.width, chipH);
            if (hits)
                hits->push_back({r, pc.key});
            if (p) { // .chip: nền --c-active; rê/nhấn --c-press viền trong --c-border-ctl
                const bool lit = enabled && (chip == hot || chip == down);
                p->setPen(lit ? QPen(col(tok::c_border_ctl), kBorder) : QPen(Qt::NoPen));
                p->setBrush(col(lit ? tok::c_press : tok::c_active));
                p->drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
                const int cb = r.top() + (chipH + fl.ascent() - fl.descent()) / 2;
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

// .cmd-r .ib.sm (#cmd-histbtn của prototype): mũi tên lên cỡ .ic.xs, mở lịch sử đầy đủ. Không nhận Tab (bàn phím: F2,
// ghi trong tên và tooltip); rê --c-hover chữ --c-text, nhấn --c-press, vô hiệu --c-text-dis.
class CommandLine::HistoryButton final : public QAbstractButton {
public:
    explicit HistoryButton(QWidget* parent) : QAbstractButton(parent)
    {
        setObjectName(u"historyButton"_s);
        setFocusPolicy(Qt::NoFocus);
        setFixedSize(kIconButton, kIconButton);
    }

protected:
    void enterEvent(QEnterEvent* e) override
    {
        update();
        QAbstractButton::enterEvent(e);
    }
    void leaveEvent(QEvent* e) override
    {
        update();
        QAbstractButton::leaveEvent(e);
    }
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const bool en = isEnabled(), lit = en && (isDown() || underMouse());
        if (lit) {
            const qreal radius = tm().px(tok::r);
            p.setPen(Qt::NoPen);
            p.setBrush(col(isDown() ? tok::c_press : tok::c_hover));
            p.drawRoundedRect(QRectF(rect()), radius, radius);
        }
        const QColor ink = col(!en ? tok::c_text_dis : lit ? tok::c_text : tok::c_text_2);
        const QPixmap icon = theme::IconProvider::instance().pixmap(u"up"_s, kChevron, devicePixelRatioF(),
                                                                     theme::IconState::Normal);
        p.drawPixmap(QRect((width() - kChevron) / 2, (height() - kChevron) / 2, kChevron, kChevron),
                     paint::tinted(icon, ink));
    }
};

// Dải nắm bên trái (.tb-grip của prototype), rộng --sp-3: kéo để cho dòng lệnh nổi, di chuyển khi nổi, bám lại.
class CommandLine::Grip final : public QWidget {
public:
    explicit Grip(CommandLine* owner) : QWidget(owner), owner_(owner)
    {
        setObjectName(u"grip"_s);
        setCursor(Qt::SizeAllCursor);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QColor dot = col(isEnabled() ? tok::c_text_3 : tok::c_text_dis);
        dot.setAlphaF(kGripAlpha);
        p.setPen(Qt::NoPen);
        p.setBrush(dot);
        for (qreal y = kGripFirst; y + kGripDot <= height(); y += kGripPitch)
            p.drawEllipse(QPointF(width() / 2.0, y), kGripDot, kGripDot);
    }
    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton)
            owner_->drag_ = Drag{e->globalPosition().toPoint(), owner_->pos()};
    }
    void mouseMoveEvent(QMouseEvent* e) override { owner_->dragTo(e->globalPosition().toPoint(), false); }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton)
            owner_->dragTo(e->globalPosition().toPoint(), true);
    }

private:
    CommandLine* owner_;
};

CommandLine::CommandLine(QWidget* parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    history_ = new HistoryView(this);
    promptView_ = new PromptView(this);
    input_ = new CommandInput(this);
    autocomplete_ = new Autocomplete(this);
    input_->setContextMenuPolicy(Qt::NoContextMenu); // menu của dòng lệnh thay menu sửa chữ (prototype: cả vùng .cmd)
    input_->installEventFilter(this);
    // Nhấp vào lịch sử, khung, chip: focus về ô nhập; setFocus() của cửa sổ chính cũng vào ô nhập.
    setFocusPolicy(Qt::ClickFocus);
    setFocusProxy(input_);

    histButton_ = new HistoryButton(this);
    frame_ = new QHBoxLayout;
    frame_->addWidget(promptView_);
    frame_->addWidget(input_, 1);
    frame_->addWidget(histButton_);
    grip_ = new Grip(this);
    auto* column = new QVBoxLayout(this);
    column->addWidget(history_);
    column->addLayout(frame_);

    connect(input_, &QLineEdit::textChanged, this, &CommandLine::textChanged);
    connect(input_, &QLineEdit::textEdited, this, [this](const QString& text) { // gõ, dán, bộ gõ; không phải ↑↓
        if (prompt_)
            return;
        if (const int delay = settings::Store::instance().get(kCompletionDelay); delay > 0)
            pending_.start(delay, this); // danh sách cũ giữ tới khi hết trễ (không nháy)
        else
            autocomplete_->refresh(text);
    });
    connect(histButton_, &QAbstractButton::clicked, this, [this] { emit commandRequested(u"'_TEXTSCR"_s); });
    connect(autocomplete_, &Autocomplete::accepted, this, &CommandLine::submitToken);
    connect(autocomplete_, &Autocomplete::learned, this, [this](const QString& typo, const QString& command) {
        appendHistory(
            tr("Từ nay gõ “%1” sẽ chạy %2 (AutoCorrect; xem, xóa trong Tùy chọn dòng lệnh).").arg(typo, command));
    });
    connect(&settings::Store::instance(), &settings::Store::changed, this, [this](const QString& key) {
        if (key == QLatin1StringView(kHistoryLines.key) || key == QLatin1StringView(kFontSize.key))
            relayout();
        else if (key == QLatin1StringView(kFloatTransparency.key) && floating_) {
            update();
            history_->viewport()->update();
        }
    });
    connect(&tm(), &theme::ThemeManager::changed, this, &CommandLine::relayout); // mật độ: cao dòng, cao khung
    relayout();
    retranslate();
    updateAwaiting();
    appendHistory(tr("TrionCAD đã sẵn sàng. Gõ lệnh như AutoCAD, hoặc gõ tiếng Việt (ví dụ “đo”, “in”) để tìm lệnh."));
}

CommandLine::~CommandLine()
{
    delete autocomplete_.data(); // có thể đang là con của cửa sổ
    if (floating_)
        delete history_.data(); // anh em trên vùng vẽ
}

void CommandLine::setPrompt(const Prompt& prompt)
{
    if (!prompt.nested && !prompt.command.isEmpty()) {
        // Lệnh vừa gõ ở dấu nhắc ("L") chạy thành prompt.command ("LINE"): danh sách gần đây giữ tên lệnh. Lệnh từ
        // ribbon, menu cũng vào đây (lặp bằng Enter). ponytail: suy từ prompt; INT-017 ghi lệnh đã chạy từ mọi nguồn.
        if (!typedCommand_.isEmpty())
            recent_.removeAll(typedCommand_);
        else if (!prompt_ || prompt_->command != prompt.command)
            autocomplete_->recordUse(prompt.command); // lệnh bắt đầu từ ribbon, menu…; lệnh gõ đã tính khi gửi
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
    dismissCompletion(); // lệnh đang chạy: không tự hoàn thành tên lệnh
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

QStringList CommandLine::historyLines() const
{
    QStringList all;
    all.reserve(historyCount());
    for (const Line& l : history_->lines)
        all << l.text;
    return all;
}

QString CommandLine::historyText() const
{
    return historyLines().join(u'\n');
}

void CommandLine::setCompact(bool compact)
{
    compact_ = compact;
    relayout();
}

Autocomplete* CommandLine::autocomplete() const
{
    return autocomplete_; // QPointer → con trỏ cần kiểu đầy đủ: thân ở .cpp
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
            if (e->type() == QEvent::FocusOut)
                dismissCompletion(); // bấm ra ngoài, đổi cửa sổ
            update(frameRect());       // .cmd-in:focus-within
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
            if (completionKey(k))
                return true;
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

bool CommandLine::completionKey(const QKeyEvent* e)
{
    if (e->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        return false;
    if (pending_.isActive()) { // độ trễ chưa hết: phím của danh sách dùng danh sách tính ngay; Esc chỉ bỏ lượt chờ
        static constexpr int kListKeys[] = {Qt::Key_Up,     Qt::Key_Down,  Qt::Key_Tab,  Qt::Key_Return,
                                            Qt::Key_Enter,  Qt::Key_Space, Qt::Key_Escape};
        if (std::find(std::begin(kListKeys), std::end(kListKeys), e->key()) != std::end(kListKeys)) {
            pending_.stop();
            if (e->key() != Qt::Key_Escape)
                autocomplete_->refresh(input_->text());
        }
    }
    if (!autocomplete_->isVisible())
        return false;
    switch (e->key()) {
    case Qt::Key_Up:
    case Qt::Key_Down:
        autocomplete_->step(e->key() == Qt::Key_Up ? -1 : 1);
        return true;
    case Qt::Key_Tab: // nhận dòng đang chọn, chưa chọn thì dòng đầu
        return autocomplete_->accept(true);
    case Qt::Key_Escape: // đóng danh sách, giữ chữ đã gõ; Esc lần nữa mới xóa chữ, bỏ vùng chọn
        dismissCompletion();
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space: // không có dòng chọn sẵn (chỉ khớp giữa chuỗi, gợi ý gõ sai): gửi chữ đã gõ
        return ownsKey(e) && autocomplete_->accept(false);
    default:
        return false;
    }
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
    dismissCompletion();
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
        if (const QString fixed = autocomplete_->correction(token); !fixed.isEmpty())
            token = fixed; // AutoCorrect: lỗi gõ đã học (chọn cùng gợi ý 3 lần)
        // "Lệnh: <b>LINE</b>"
        const QString pattern = tr("Lệnh: %1");
        echo.text = pattern.arg(token);
        echo.kind = Kind::Echo;
        echo.boldFrom = int(std::max<qsizetype>(pattern.indexOf(u"%1"), 0));
        echo.boldTo = echo.boldFrom + int(token.size());
        if (!token.startsWith(u'(')) { // biểu thức LISP không phải lệnh để lặp
            typedCommand_ = token.toUpper();
            remember(recent_, typedCommand_, kRecentMax);
            autocomplete_->recordUse(token); // tần suất cho tự hoàn thành (tên lạ bỏ qua)
        }
    }
    history_->append(std::move(echo));
    emit submitted(token);
}

void CommandLine::reportUnknown(const QString& typed)
{
    const QString name = typed.trimmed(), typo = name.toUpper(); // typo: khóa AutoCorrect
    auto u = std::make_shared<Chips>();
    const QString head = tr("Không có lệnh “%1”.").arg(name);
    u->headTo = int(head.size());
    const QStringList terms = autocomplete_->suggest(name);
    const QChar mark(0xE000); // chỗ của các gợi ý trong câu đã dịch (ký tự dùng riêng)
    Line line{u"%1 %2"_s.arg(head, terms.isEmpty() ? tr("Gõ ? hoặc mở Trợ giúp (F1) để xem danh sách lệnh.")
                                                   : tr("Ý bạn là: %1?").arg(mark)),
              Kind::Error};
    if (const qsizetype at = line.text.indexOf(mark, head.size()); at >= 0) {
        QString chips;
        for (const QString& term : terms) {
            if (!chips.isEmpty())
                chips += u", "_s;
            const auto from = int(at + chips.size());
            chips += term;
            u->chips.push_back({from, int(at + chips.size()),
                                [this, typo, command = autocomplete_->resolve(term)] { pickSuggestion(typo, command); }});
        }
        line.text.replace(at, 1, chips);
    }
    line.chips = std::move(u);
    const QString say = line.text;
    history_->append(std::move(line));
    announce(history_, say, QAccessible::AnnouncementPoliteness::Assertive);
}

void CommandLine::notifyLocked(const QString& command, Edition need)
{
    const edition::EditionGate::CommandNotice n = edition::EditionGate::commandNotice(command, need);
    Line line{n.text, Kind::Message};
    if (const qsizetype at = n.text.indexOf(command); at >= 0) // tên lệnh đậm như prototype
        line.boldFrom = int(at), line.boldTo = int(at + command.size());
    auto c = std::make_shared<Chips>();
    c->headTo = int(n.text.size());
    for (const QString& label : {n.trial, n.learn}) {
        line.text += u' ';
        const int from = int(line.text.size());
        line.text += label;
        c->chips.push_back({from, int(line.text.size()), [this, command] { emit upgradeRequested(command); }});
    }
    line.chips = std::move(c);
    const QString say = line.text;
    history_->append(std::move(line));
    announce(history_, say, QAccessible::AnnouncementPoliteness::Assertive);
}

void CommandLine::pickSuggestion(const QString& typo, const QString& command)
{
    if (command.isEmpty())
        return;
    autocomplete_->recordPick(typo, command);
    emit commandRequested(u"^C^C"_s + command); // như "Lệnh gần đây": hủy lệnh đang chạy (nếu có) rồi chạy
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
    const settings::Store& store = settings::Store::instance();
    const int fs = store.get(kFontSize);
    zoom_ = fs > 0 ? fs / tm().px(tok::fs) : 1.0;
    const int lines = std::clamp(store.get(kHistoryLines), 0, kMaxLines);
    const int shown = compact_ ? std::min(lines, 1) : lines;
    const int gap = px(tok::sp_3);
    history_->setFixedHeight(shown * scaled(tok::h_cmdl, zoom_));
    history_->setVisible(shown > 0 && (!floating_ || isVisible())); // nổi: theo hàng ô nhập (showEvent)
    history_->updateRange();
    auto* column = static_cast<QVBoxLayout*>(layout());
    column->setContentsMargins(0, kBorder + kTight, 0, gap);
    column->setSpacing(kTight);
    frame_->setContentsMargins(gap + kBorder + px(tok::sp_4), kBorder, gap + kBorder + px(tok::sp_2), kBorder);
    frame_->setSpacing(gap);
    input_->setFixedHeight(scaled(tok::h_cmdin, zoom_) - 2 * kBorder);
    input_->setMinimumWidth(kInputMin);
    input_->setFont(zoomed(TextRole::Mono, zoom_)); // sau CommandInput (nối ThemeManager::changed trước)
    promptView_->updateGeometry();
    fitHistoryButton();
    if (floating_) { // không nằm trong bố cục nào: tự đặt cao
        layout()->activate();
        resize(width(), sizeHint().height());
        placeHistory();
    }
}

void CommandLine::dismissCompletion()
{
    pending_.stop();
    autocomplete_->dismiss();
}

void CommandLine::timerEvent(QTimerEvent* e)
{
    if (e->timerId() != pending_.timerId()) {
        QWidget::timerEvent(e);
        return;
    }
    pending_.stop();
    if (!prompt_)
        autocomplete_->refresh(input_->text());
}

void CommandLine::setFloating(bool on, std::optional<QPoint> at)
{
    QWidget* host = parentWidget();
    TRION_CHECK(!on || host);
    if (on != floating_) {
        floating_ = on;
        dismissCompletion();
        if (on) {
            history_->setParent(host); // bố cục tự bỏ mục của nó
            history_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            resize(std::min(width(), host->width() * 2 / 3), height());
        } else {
            history_->setAttribute(Qt::WA_TransparentForMouseEvents, false);
            history_->setParent(this);
            static_cast<QVBoxLayout*>(layout())->insertWidget(0, history_);
        }
        relayout();
    }
    if (on) {
        move(clampFloat(at ? host->mapFromGlobal(*at) : pos()));
        raise();
        placeHistory();
    }
    update();
}

QPoint CommandLine::clampFloat(QPoint topLeft) const
{
    const QWidget* host = parentWidget();
    if (!host)
        return topLeft;
    const int top = history_->height(); // lịch sử nằm trên, cũng trong cha
    return {std::clamp(topLeft.x(), 0, std::max(0, host->width() - width())),
            std::clamp(topLeft.y(), top, std::max(top, host->height() - height()))};
}

void CommandLine::placeHistory()
{
    if (!floating_ || !history_)
        return;
    history_->setGeometry(x(), y() - history_->height(), width(), history_->height());
    history_->raise();
}

void CommandLine::dragTo(QPoint global, bool done)
{
    if (!drag_)
        return;
    const QPoint d = global - drag_->start;
    drag_->moved = drag_->moved || d.manhattanLength() >= QApplication::startDragDistance();
    if (drag_->moved && floating_)
        move(clampFloat(drag_->origin + d));
    if (!done)
        return;
    const Drag was = *std::exchange(drag_, std::nullopt);
    if (!was.moved)
        return;
    if (!floating_)
        emit floatRequested(mapToGlobal(QPoint()) + d);
    else if (parentWidget()->height() - (geometry().bottom() + 1) <= px(tok::h_cmdl)) // thả sát mép dưới vùng vẽ
        emit dockRequested();
}

void CommandLine::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    grip_->setGeometry(0, kBorder, px(tok::sp_3), height() - kBorder);
    grip_->raise(); // trên lịch sử khi dock
    fitHistoryButton();
    placeHistory();
}

void CommandLine::moveEvent(QMoveEvent* e)
{
    QWidget::moveEvent(e);
    placeHistory();
}

void CommandLine::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    if (floating_) {
        history_->setVisible(history_->height() > 0);
        placeHistory();
    }
}

void CommandLine::hideEvent(QHideEvent* e)
{
    QWidget::hideEvent(e);
    if (floating_)
        history_->hide();
}

QString CommandLine::placeholder() const
{
    return tr("Gõ lệnh hoặc từ khóa tiếng Việt, ví dụ: đo, in, layer");
}

void CommandLine::retranslate()
{
    history_->setAccessibleName(tr("Lịch sử dòng lệnh"));
    histButton_->setAccessibleName(tr("Lịch sử dòng lệnh (F2)"));
    if (RichTooltip::instance()) { // có tooltip 2 mức (main tạo); test không có thì thôi
        RichTooltip::attach(histButton_, {tr("Lịch sử đầy đủ"), u"TEXTSCR"_s, u"hist"_s});
        RichTooltip::attach(grip_, {tr("Kéo để dòng lệnh nổi trên vùng vẽ; thả sát mép dưới để bám lại")});
    }
    input_->setAccessibleDescription(prompt_ ? describe(*prompt_) : placeholder());
    update(input_->geometry());
    fitHistoryButton();
}

// Nút lịch sử nhường chỗ cho prompt: hàng hẹp (1366 px, chữ dịch dài) thì ẩn nút; F2, menu chuột phải vẫn mở lịch sử.
// Gọi trước khi bố cục chạy (prompt, chữ, cỡ đổi) để lần bố cục kế tiếp đã tính không có nút.
void CommandLine::fitHistoryButton()
{
    const QMargins m = frame_->contentsMargins();
    const int room = width() - m.left() - m.right() - 2 * frame_->spacing() - kInputMin - kIconButton;
    histButton_->setVisible(promptView_->sizeHint().width() <= room);
}

void CommandLine::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    if (e->type() == QEvent::EnabledChange && !isEnabled())
        dismissCompletion(); // danh sách nằm ở cửa sổ, không tự vô hiệu theo dòng lệnh
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
    p.fillRect(rect(), floating_ ? floatBase() : col(tok::c_base));
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
        p.setFont(zoomed(TextRole::Ui, zoom_));
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
