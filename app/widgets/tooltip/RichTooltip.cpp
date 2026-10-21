#include "app/widgets/tooltip/RichTooltip.h"

#include "app/core/keymap/Keymap.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/ThemeManager.h"
#include "core/base/assert.h"

#include <QAction>
#include <QActionEvent>
#include <QApplication>
#include <QBoxLayout>
#include <QEnterEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QScreen>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;
using trion::theme::TextRole;
using trion::theme::ThemeManager;
using trion::theme::Weight;

namespace trion::ui {

namespace {

constexpr char kInfo[] = "trionTipInfo"; // thuộc tính động giữ TipInfo
// .tip max-width 300px của prototype (chưa có token).
constexpr int kMaxWidth = 300;

RichTooltip* g_instance = nullptr;

int px(tok::PxId id)
{
    return qRound(ThemeManager::instance().px(id));
}

QString stripMnemonic(QString s) // "&&" → "&", "&Lưu" → "Lưu"; bỏ phần sau Tab
{
    s.truncate(s.indexOf(u'\t') < 0 ? s.size() : s.indexOf(u'\t'));
    for (qsizetype i = 0; i < s.size(); ++i)
        if (s[i] == u'&')
            s.remove(i, 1); // ký tự sau '&' (kể cả '&' thứ hai) giữ lại và được bỏ qua
    return s;
}

// Huy hiệu trong dòng tên: cao bằng dòng đầu của tên, pill ở giữa (inline như .tn .ed.xs).
class Badge final : public QWidget {
public:
    Edition need = Edition::None;
    int line = 0;
    QSize sizeHint() const override
    {
        const QSize s = RichTooltip::badgeSize(need);
        return {s.width(), std::max(s.height(), line)};
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const QSize s = RichTooltip::badgeSize(need);
        RichTooltip::paintBadge(p, QRect(QPoint(0, (height() - s.height()) / 2), s), need);
    }
};

// Cửa sổ tooltip (.tip): nhãn chữ thường xuống dòng trong bề rộng tối đa (QLabel để TruncationProbe đo được);
// nền --c-dyn-bg, viền --c-dyn-b bo --r. Không nhận chuột (pointer-events:none), không lấy focus.
class TipWindow final : public QWidget {
public:
    TipWindow() : QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowTransparentForInput)
    {
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* col = new QVBoxLayout(this);
        col->setSizeConstraint(QLayout::SetNoConstraint);
        row->setContentsMargins({});
        row->addWidget(name);
        row->addWidget(badge, 0, Qt::AlignTop);
        row->addStretch();
        col->addLayout(row);
        for (QLabel* l : {name, command, description, help}) {
            l->setTextFormat(Qt::PlainText);
            l->setWordWrap(true);
            if (l != name)
                col->addWidget(l);
        }
    }

    void setContent(const QString& title, const TipInfo& info, const QString& keys, int level)
    {
        auto& t = ThemeManager::instance();
        QPalette pal = palette();
        pal.setColor(QPalette::Window, t.color(tok::c_dyn_bg));
        pal.setColor(QPalette::WindowText, t.color(tok::c_dyn_t));
        setPalette(pal);
        const auto set = [&](QLabel* l, const QString& text, const QFont& f, tok::ColorId c, int top) {
            l->setHidden(text.isEmpty());
            l->setText(text);
            l->setFont(f);
            l->setContentsMargins(0, top, 0, 0);
            QPalette lp = pal;
            lp.setColor(QPalette::WindowText, t.color(c));
            l->setPalette(lp);
        };
        // Lề .td, .tf 3 px ngoài thang token: --sp-1 cộng khoảng cách dòng --sp-1.
        const int gap = px(tok::sp_1);
        QStringList cmd{info.command, keys};
        cmd.removeAll(QString());
        set(name, title, theme::font(TextRole::Label, Weight::Bold), tok::c_dyn_t, 0);
        set(command, cmd.join(u" · "_s), theme::font(TextRole::MonoCaption), tok::c_text_2, 0);
        set(description, level >= 2 ? info.description : QString(), theme::font(TextRole::Label), tok::c_text_2, gap);
        set(help, level >= 2 && !info.command.isEmpty() ? RichTooltip::tr("Nhấn F1 để xem trợ giúp") : QString(),
            theme::font(TextRole::Caption), tok::c_text_3, gap);
        badge->need = info.need;
        badge->line = name->fontMetrics().height();
        badge->setHidden(info.need == Edition::None);
        badge->updateGeometry();
        // .tip: viền 1 px, lề 6 px dọc (--sp-3), 9 px ngang (--sp-4 + 1), các dòng cách --sp-1
        const int h = 1 + px(tok::sp_4) + 1, v = 1 + px(tok::sp_3);
        layout()->setContentsMargins(h, v, h, v);
        layout()->setSpacing(gap);
        row->setSpacing(px(tok::sp_2));

        // Rộng theo chữ dài nhất (không xuống dòng), tối đa kMaxWidth; cao theo chữ đã xuống dòng ở bề rộng đó.
        int natural = name->fontMetrics().size(0, title).width();
        if (!badge->isHidden())
            natural += row->spacing() + badge->sizeHint().width();
        for (QLabel* l : {command, description, help})
            if (!l->isHidden())
                natural = std::max(natural, l->fontMetrics().size(0, l->text()).width());
        const int w = std::min(natural + 2 * h, kMaxWidth);
        layout()->invalidate();
        resize(w, heightForWidth(w));
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        auto& t = ThemeManager::instance();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(t.color(tok::c_dyn_b));
        p.setBrush(t.color(tok::c_dyn_bg));
        const qreal r = t.px(tok::r);
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), r, r);
    }

private:
    QHBoxLayout* row = new QHBoxLayout;

public:
    QLabel* name = new QLabel;
    Badge* badge = new Badge;
    QLabel* command = new QLabel;
    QLabel* description = new QLabel;
    QLabel* help = new QLabel;
};

} // namespace

RichTooltip::RichTooltip(QObject* parent) : QObject(parent), window_(new TipWindow)
{
    timer_.setSingleShot(true);
    timer_.setTimerType(Qt::PreciseTimer); // không đến sớm: mức 2 không hiện trước 1,5 s
    connect(&timer_, &QTimer::timeout, this, &RichTooltip::advance);
    connect(&ThemeManager::instance(), &ThemeManager::changed, this, [this] {
        if (level_)
            showLevel(level_); // màu, cỡ của chủ đề, mật độ mới
    });
    g_instance = this;
}

RichTooltip::~RichTooltip()
{
    leave();
    delete window_;
    if (g_instance == this)
        g_instance = nullptr;
}

RichTooltip* RichTooltip::instance()
{
    return g_instance;
}

void RichTooltip::attach(QObject* target, const TipInfo& info)
{
    target->setProperty(kInfo, QVariant::fromValue(info));
    if (target->isWidgetType()) {
        TRION_CHECK(g_instance && "chưa tạo RichTooltip");
        target->installEventFilter(g_instance);
    } else if (auto* a = qobject_cast<QAction*>(target)) {
        for (QObject* o : a->associatedObjects()) { // như QAction::changed: Menu đo, vẽ lại mục
            QActionEvent e(QEvent::ActionChanged, a);
            if (o->isWidgetType())
                QCoreApplication::sendEvent(o, &e);
        }
    }
    if (g_instance && g_instance->target_ == target && g_instance->level_)
        g_instance->showLevel(g_instance->level_);
}

TipInfo RichTooltip::info(const QObject* target)
{
    return target ? target->property(kInfo).value<TipInfo>() : TipInfo{};
}

bool RichTooltip::hasInfo(const QObject* target)
{
    return target && target->property(kInfo).isValid();
}

QString RichTooltip::name(const QObject* target)
{
    if (QString n = info(target).name; !n.isEmpty())
        return n;
    if (const auto* a = qobject_cast<const QAction*>(target))
        return stripMnemonic(a->text());
    if (const auto* w = qobject_cast<const QWidget*>(target))
        return w->accessibleName();
    return {};
}

QString RichTooltip::editionName(Edition e)
{
    switch (e) {
    case Edition::Pro:
        return tr("Pro");
    case Edition::ViewerPro:
        return tr("Viewer Pro");
    case Edition::None:
        break;
    }
    return {};
}

QSize RichTooltip::badgeSize(Edition e) // .ed.xs: lề ngang --sp-2, chữ --fs-2xs đậm (9,5 px của prototype)
{
    if (e == Edition::None)
        return {};
    const QFontMetrics fm(theme::font(TextRole::Badge, Weight::Bold));
    return {fm.horizontalAdvance(editionName(e)) + 2 * px(tok::sp_2), fm.height()};
}

void RichTooltip::paintBadge(QPainter& p, const QRect& r, Edition e)
{
    if (e == Edition::None)
        return;
    auto& t = ThemeManager::instance();
    const bool vp = e == Edition::ViewerPro;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(t.color(vp ? tok::c_ed_vp : tok::c_ed_pro));
    const qreal radius = t.px(tok::r);
    p.drawRoundedRect(QRectF(r), radius, radius);
    p.setPen(t.color(vp ? tok::c_ed_vp_t : tok::c_ed_pro_t));
    p.setFont(theme::font(TextRole::Badge, Weight::Bold));
    p.drawText(r, Qt::AlignCenter, editionName(e));
    p.restore();
}

const keymap::Keymap* RichTooltip::keymap() const
{
    return keymap_;
}

void RichTooltip::setKeymap(const keymap::Keymap* keymap)
{
    disconnect(keymapChanged_);
    keymap_ = keymap;
    const auto changed = [this] {
        emit keysChanged();
        if (level_)
            showLevel(level_);
    };
    if (keymap)
        keymapChanged_ = connect(keymap, &keymap::Keymap::changed, this, changed);
    changed();
}

QString RichTooltip::keyText(const QString& keyId) const
{
    const keymap::Action* a = keymap_ && !keyId.isEmpty() ? keymap_->action(keyId) : nullptr;
    return a ? keymap::keyLabel(a->keys.join(u" · "_s), keymap_->platform()) : QString();
}

void RichTooltip::setDelays(std::chrono::milliseconds level1, std::chrono::milliseconds level2)
{
    level1_ = level1;
    level2_ = level2;
}

void RichTooltip::hover(QObject* target, const QRect& anchor, const QPoint& cursor)
{
    anchor_ = anchor;
    cursor_ = cursor;
    if (target == target_)
        return;
    window_->hide();
    level_ = 0;
    target_ = target;
    if (!std::exchange(filtering_, true)) // F1, bấm, phím, vị trí con trỏ khi đang rê
        QCoreApplication::instance()->installEventFilter(this);
    timer_.start(level1_);
}

void RichTooltip::leave()
{
    timer_.stop();
    if (std::exchange(filtering_, false))
        QCoreApplication::instance()->removeEventFilter(this);
    target_ = nullptr;
    level_ = 0;
    window_->hide();
}

QWidget* RichTooltip::tipWindow() const
{
    return window_;
}

void RichTooltip::advance()
{
    const TipInfo i = info(target_);
    if (!target_ || (name(target_).isEmpty() && i.command.isEmpty()))
        return leave(); // phần tử đã hủy, hoặc không có gì để hiện
    showLevel(level_ + 1);
    if (level_ == 1 && !(i.description.isEmpty() && i.command.isEmpty()))
        timer_.start(level2_);
}

void RichTooltip::showLevel(int level)
{
    level_ = level;
    const TipInfo i = info(target_);
    auto* w = static_cast<TipWindow*>(window_);
    w->setContent(name(target_), i, keyText(i.keyId), level);
    w->move(place(w->size()));
    w->show();
}

// Dưới phần tử cách --sp-3, hết chỗ thì lên trên (như showTip); trong màn hình cách mép --sp-2. Phần tử cao hơn
// chỗ trống hoặc tooltip đè lên con trỏ: dời xuống dưới con trỏ (mũi tên ~16×24 px từ điểm nóng: --sp-6 × --sp-7).
QPoint RichTooltip::place(const QSize& s) const
{
    const QScreen* screen = QGuiApplication::screenAt(cursor_);
    const QRect avail = (screen ? screen : QGuiApplication::primaryScreen())->availableGeometry();
    const int gap = px(tok::sp_3), edge = px(tok::sp_2);
    const int bottom = avail.bottom() + 1 - edge;
    const int x = std::clamp(anchor_.left(), avail.left() + edge,
                             std::max(avail.left() + edge, avail.right() + 1 - edge - s.width()));
    int y = anchor_.bottom() + 1 + gap;
    if (y + s.height() > bottom)
        y = anchor_.top() - gap - s.height();
    const QRect cursor(cursor_, QSize(px(tok::sp_6), px(tok::sp_7)));
    if (y < avail.top() + edge || QRect(QPoint(x, y), s).intersects(cursor)) {
        y = cursor.bottom() + 1 + edge;
        if (y + s.height() > bottom)
            y = cursor.top() - edge - s.height();
    }
    return {x, y};
}

bool RichTooltip::eventFilter(QObject* watched, QEvent* event)
{
    // Lọc hai tầng: widget đã attach (vào, tooltip thường của Qt) và cả ứng dụng khi đang rê. Sự kiện tới widget
    // đã attach đi qua cả hai nên mọi nhánh làm hai lần vẫn đúng.
    switch (event->type()) {
    case QEvent::Enter:
        if (watched != target_ && watched->isWidgetType() && hasInfo(watched)) {
            auto* w = static_cast<QWidget*>(watched);
            hover(w, QRect(w->mapToGlobal(QPoint()), w->size()),
                  static_cast<QEnterEvent*>(event)->globalPosition().toPoint());
        }
        break;
    case QEvent::ToolTip:
        return watched->isWidgetType() && hasInfo(watched);
    case QEvent::Leave:
    case QEvent::Hide:
        if (watched == target_)
            leave();
        break;
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    case QEvent::Wheel:
        if (target_)
            leave();
        break;
    case QEvent::MouseMove:
        if (target_)
            cursor_ = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
        break;
    case QEvent::ShortcutOverride:
    case QEvent::KeyPress: {
        if (!target_)
            break;
        // F1 khi đang rê phần tử có lệnh: nhận ở ShortcutOverride để phím tắt F1 toàn cục không chạy, phát yêu cầu
        // ở KeyPress. Phím khác (trừ phím bổ trợ): ẩn.
        const auto* k = static_cast<QKeyEvent*>(event);
        const QString command = info(target_).command;
        if (k->key() == Qt::Key_F1 && k->modifiers() == Qt::NoModifier && !command.isEmpty()) {
            event->accept();
            if (event->type() == QEvent::KeyPress) {
                leave();
                emit helpRequested(command);
            }
            return true;
        }
        if (k->key() < Qt::Key_Shift || k->key() > Qt::Key_ScrollLock) // phím bổ trợ (Ctrl, Shift…): giữ, như QToolTip
            leave();
        break;
    }
    default:
        break;
    }
    return false;
}

} // namespace trion::ui
