#include "app/widgets/message/Messages.h"

#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/progress/ProgressPresenter.h"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QApplication>
#include <QBoxLayout>
#include <QEvent>
#include <QKeyEvent>
#include <QPainter>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace trion::ui {

using theme::IconProvider;
using theme::IconState;
using theme::Sem;
using theme::TextRole;
using theme::Weight;

namespace {

constexpr int kBorder = 1;
constexpr int kToastMaxWidth = 420;  // .toast max-width
constexpr int kBannerMinHeight = 38; // .banner min-height
constexpr double kBannerTint = 0.14; // .banner.warn, .err: color-mix(in srgb, màu 14%, --c-base)
constexpr int kTipIcon = 12; // .ic.s12: prototype .ctip chỉ có viền màu; thêm biểu tượng để mức không chỉ là màu
constexpr int kEmptyIcon = 28;      // .empty .ic
constexpr int kFieldIcon = 9;       // .fmsg: TC.ic('warn', 'xs')
constexpr int kFieldMinHeight = 14; // .fmsg min-height
constexpr char kRoleProperty[] = "_trion_a11y_role";

ToastHost* g_toasts = nullptr; // chỉ luồng UI

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

// Màu của token trên vùng vẽ: nền vùng vẽ không đổi theo chủ đề giao diện (07.1 §13.2), dùng giá trị :root.
QColor canvasCol(tok::ColorId id)
{
    return theme::toQColor(tok::color(id, tok::Mode{}));
}

tok::ColorId ink(Severity s)
{
    switch (s) {
    case Severity::Ok:
        return tok::c_ok;
    case Severity::Info:
        return tok::c_info;
    case Severity::Warning:
        return tok::c_warn;
    case Severity::Error:
        break;
    }
    return tok::c_danger;
}

// Biểu tượng một màu tô lại bằng màu c (alpha giữ nguyên).
QPixmap tinted(QPixmap pm, const QColor& c)
{
    QPainter p(&pm);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(pm.rect(), c);
    return pm;
}

// color-mix(in srgb, a t, b)
QColor mix(const QColor& a, const QColor& b, double t)
{
    return QColor::fromRgbF(float(a.redF() * t + b.redF() * (1 - t)), float(a.greenF() * t + b.greenF() * (1 - t)),
                            float(a.blueF() * t + b.blueF() * (1 - t)));
}

void setInk(QWidget* w, const QColor& c)
{
    QPalette p = w->palette();
    p.setColor(QPalette::WindowText, c);
    w->setPalette(p);
}

// Áp ngay và mỗi khi chủ đề, mật độ đổi (ThemeManager::changed): lề, khoảng cách, font, màu chữ của QLabel.
template <class F>
void themed(QObject* owner, F apply)
{
    apply();
    QObject::connect(&tm(), &theme::ThemeManager::changed, owner, apply);
}

// Vai trò tiếp cận theo thuộc tính của widget (Toast, banner, CursorTip): QAccessibleWidget mặc định chỉ có Client.
QAccessibleInterface* roleFactory(const QString&, QObject* object)
{
    auto* w = qobject_cast<QWidget*>(object);
    const QVariant role = w ? w->property(kRoleProperty) : QVariant();
    return role.isValid() ? new QAccessibleWidget(w, QAccessible::Role(role.toInt())) : nullptr;
}

void setRole(QWidget* w, QAccessible::Role role)
{
    static const bool installed = (QAccessible::installFactory(roleFactory), true);
    Q_UNUSED(installed);
    w->setProperty(kRoleProperty, int(role));
}

// Như role="alert" (urgent: QAccessible::Alert rồi thông báo Assertive) hoặc role="status" (thông báo Polite).
void announce(QWidget* w, const QString& text, bool urgent)
{
    if (!QAccessible::isActive())
        return; // không có trình đọc màn hình: khỏi dựng giao diện tiếp cận cho mỗi thông điệp
    if (urgent) {
        QAccessibleEvent alert(w, QAccessible::Alert);
        QAccessible::updateAccessibility(&alert);
    }
    QAccessibleAnnouncementEvent say(w, text);
    say.setPoliteness(urgent ? QAccessible::AnnouncementPoliteness::Assertive
                             : QAccessible::AnnouncementPoliteness::Polite);
    QAccessible::updateAccessibility(&say);
}

// Focus vào thông điệp (Tab tới nút của toast, banner): nhớ widget có focus trước đó vào back, để khi thông điệp đóng
// trả focus về đó (dòng lệnh, vùng vẽ) thay vì để Qt chuyển sang widget kế tiếp bất kỳ.
void rememberFocus(QWidget* owner, QPointer<QWidget>* back)
{
    QObject::connect(qApp, &QApplication::focusChanged, owner, [owner, back](QWidget* old, QWidget* now) {
        if (owner->isAncestorOf(now) && !owner->isAncestorOf(old))
            *back = old;
    });
}

void returnFocus(QWidget* owner, const QPointer<QWidget>& back)
{
    if (back && owner->isAncestorOf(QApplication::focusWidget()))
        back->setFocus(Qt::OtherFocusReason);
}

// Chữ một dòng, hẹp thì rút gọn "…" (.banner p: nowrap, ellipsis). Vẫn là QLabel đủ chữ: trình đọc màn hình đọc cả
// câu, findTruncations báo khi bị rút gọn; tooltip hiện đủ câu khi đang rút gọn.
class ElidedLabel final : public QLabel {
public:
    explicit ElidedLabel(QWidget* parent) : QLabel(parent)
    {
        setTextFormat(Qt::PlainText);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); // nhường chỗ cho nút hành động
    }

protected:
    void resizeEvent(QResizeEvent* e) override
    {
        QLabel::resizeEvent(e);
        setToolTip(fontMetrics().horizontalAdvance(text()) > contentsRect().width() ? text() : QString());
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setPen(col(tok::c_text));
        const QRect r = contentsRect();
        p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, fontMetrics().elidedText(text(), Qt::ElideRight, r.width()));
    }
};

} // namespace

QString iconName(Severity s)
{
    switch (s) {
    case Severity::Ok:
        return u"okc"_s;
    case Severity::Info:
        return u"info"_s;
    case Severity::Warning:
        return u"warn"_s;
    case Severity::Error:
        break;
    }
    return u"error"_s;
}

// -------------------------------------------------------------------------------- Glyph

Glyph::Glyph(int size, QWidget* parent) : QWidget(parent), ink_(tok::c_text_2), size_(size)
{
    setFixedSize(size, size);
}

void Glyph::set(const QString& name, tok::ColorId ink, bool canvas)
{
    name_ = name;
    ink_ = ink;
    canvas_ = canvas;
    update();
}

void Glyph::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    const qreal dpr = p.device()->devicePixelRatio();
    const QColor c = canvas_ ? canvasCol(ink_) : col(ink_);
    p.drawPixmap(0, 0, tinted(IconProvider::instance().pixmap(name_, size_, dpr, IconState::Normal), c));
}

// -------------------------------------------------------------------------------- NotificationCenter

NotificationCenter::NotificationCenter(QObject* parent) : QAbstractListModel(parent) {}

NotificationCenter::Id NotificationCenter::add(const QString& text, Severity severity, const QString& kind)
{
    if (entries_.size() >= kMaxEntries) {
        const int last = int(entries_.size()) - 1;
        beginRemoveRows({}, last, last);
        const bool unread = !entries_.takeLast().read;
        endRemoveRows();
        if (unread)
            setUnread(unread_ - 1);
    }
    beginInsertRows({}, 0, 0);
    entries_.prepend({next_, text, severity, kind, QDateTime::currentDateTime()});
    endInsertRows();
    setUnread(unread_ + 1);
    return next_++;
}

int NotificationCenter::row(Id id) const
{
    for (int i = 0; i < entries_.size(); ++i)
        if (entries_[i].id == id)
            return i;
    return -1;
}

void NotificationCenter::markRead(Id id)
{
    const int i = row(id);
    if (i < 0 || entries_[i].read)
        return;
    entries_[i].read = true;
    emit dataChanged(index(i), index(i), {ReadRole});
    setUnread(unread_ - 1);
}

void NotificationCenter::markAllRead()
{
    if (!unread_)
        return;
    for (Entry& e : entries_)
        e.read = true;
    emit dataChanged(index(0), index(int(entries_.size()) - 1), {ReadRole});
    setUnread(0);
}

void NotificationCenter::remove(Id id)
{
    const int i = row(id);
    if (i < 0)
        return;
    beginRemoveRows({}, i, i);
    const bool unread = !entries_.takeAt(i).read;
    endRemoveRows();
    if (unread)
        setUnread(unread_ - 1);
}

void NotificationCenter::setUnread(int count)
{
    if (count == unread_)
        return;
    unread_ = count;
    emit unreadChanged(count);
}

int NotificationCenter::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(entries_.size());
}

QVariant NotificationCenter::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const Entry& e = entries_[index.row()];
    switch (role) {
    case Qt::DisplayRole:
        return e.text;
    case IdRole:
        return e.id;
    case SeverityRole:
        return int(e.severity);
    case KindRole:
        return e.kind;
    case TimeRole:
        return e.time;
    case ReadRole:
        return e.read;
    default:
        return {};
    }
}

// -------------------------------------------------------------------------------- Toast

Toast::Toast(Severity severity, const QString& text, MessageAction action, QWidget* window)
    : QWidget(window), severity_(severity), glyph_(new Glyph(px(tok::ic), this)), text_(new QLabel(text, this)),
      close_(new Button({}, Button::Kind::Icon, this)), timer_(this)
{
    setFocusPolicy(Qt::NoFocus);
    setRole(this, severity == Severity::Error ? QAccessible::AlertMessage : QAccessible::StatusBar);
    setAccessibleName(text);
    glyph_->set(iconName(severity), ink(severity));
    text_->setTextFormat(Qt::PlainText);
    text_->setWordWrap(true);
    text_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    auto* row = new QHBoxLayout(this);
    row->addWidget(glyph_);
    row->addWidget(text_, 1); // chỗ thừa về chữ, nút giữ cỡ của nó
    if (action.run) {
        action_ = new Button(action.label, Button::Kind::Normal, this);
        action_->setControlSize(Button::Size::Small);
        action_->setFocusPolicy(Qt::TabFocus);
        row->addWidget(action_);
        connect(action_, &QPushButton::clicked, this, [this, run = std::move(action.run)] {
            run();
            emit dismissed(true);
        });
    }
    close_->setIconName(u"x"_s);
    close_->setFocusPolicy(Qt::TabFocus);
    close_->setAccessibleName(tr("Đóng thông báo"));
    row->addWidget(close_);
    connect(close_, &QPushButton::clicked, this, [this] { emit dismissed(true); });
    timer_.setSingleShot(true);
    timer_.setTimerType(Qt::PreciseTimer);
    connect(&timer_, &QTimer::timeout, this, [this] { emit dismissed(false); });
    connect(qApp, &QApplication::focusChanged, this, &Toast::hold);
    rememberFocus(this, &back_);
    themed(this, [this, row] {
        // .toast: padding 9px 8px 9px 12px trong viền 1 px, gap 10px, chữ --fs-s
        const int left = kBorder + px(tok::sp_5), right = kBorder + px(tok::sp_4);
        const int vertical = kBorder + px(tok::sp_4) + 1;
        row->setContentsMargins(left, vertical, right, vertical);
        row->setSpacing(px(tok::sp_4) + px(tok::sp_1));
        text_->setFont(theme::font(TextRole::Label));
        setInk(text_, col(tok::c_text));
    });
}

QString Toast::text() const
{
    return text_->text();
}

void Toast::start(std::chrono::milliseconds timeout)
{
    if (severity_ == Severity::Error)
        return; // lỗi ở lại đến khi đóng
    timer_.start(timeout);
    hold(); // hiện ngay dưới con trỏ
}

void Toast::hold()
{
    if (severity_ == Severity::Error || (!timer_.isActive() && left_ < 0))
        return;
    const bool held = hovered_ || isAncestorOf(QApplication::focusWidget());
    if (held && timer_.isActive()) {
        left_ = std::max(0, timer_.remainingTime());
        timer_.stop();
    } else if (!held && left_ >= 0) {
        timer_.start(left_);
        left_ = -1;
    }
}

QSize Toast::fit(int maxWidth) const
{
    // Rộng theo chữ trên một dòng, tối đa maxWidth; cao theo chữ đã xuống dòng ở bề rộng đó.
    const int natural =
        sizeHint().width() - text_->sizeHint().width() + text_->fontMetrics().size(0, text_->text()).width();
    const int w = std::min(natural, maxWidth);
    const int h = heightForWidth(w);
    return {w, h > 0 ? h : sizeHint().height()};
}

bool Toast::event(QEvent* e)
{
    switch (e->type()) {
    case QEvent::Enter:
    case QEvent::Leave:
        hovered_ = e->type() == QEvent::Enter;
        hold();
        break;
    case QEvent::LanguageChange:
        close_->setAccessibleName(tr("Đóng thông báo"));
        break;
    case QEvent::ShortcutOverride: // Esc khi focus trong toast là của toast, không thành phím tắt (hủy lệnh)
    case QEvent::KeyPress:         // nút không nhận Esc: tới toast
        if (static_cast<QKeyEvent*>(e)->key() == Qt::Key_Escape) {
            e->accept();
            if (e->type() == QEvent::KeyPress)
                emit dismissed(true);
            return true;
        }
        break;
    default:
        break;
    }
    return QWidget::event(e);
}

void Toast::paintEvent(QPaintEvent*)
{
    // .toast: nền --c-raised, viền --c-border-ctl, bo --r-lg
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(col(tok::c_border_ctl), kBorder));
    p.setBrush(col(tok::c_raised));
    const qreal r = tm().px(tok::r_lg);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), r, r);
}

// -------------------------------------------------------------------------------- ToastHost

ToastHost::ToastHost(QWidget* window, NotificationCenter& center, QObject* parent)
    : QObject(parent), window_(window), center_(center)
{
    window->installEventFilter(this);
    connect(&tm(), &theme::ThemeManager::changed, this, &ToastHost::place); // --h-status theo mật độ
    g_toasts = this;
}

ToastHost::~ToastHost()
{
    if (g_toasts == this)
        g_toasts = nullptr;
    qDeleteAll(std::exchange(toasts_, {}));
}

ToastHost* ToastHost::instance()
{
    return g_toasts;
}

Toast* ToastHost::post(Severity severity, const QString& text, MessageAction action, const QString& kind)
{
    if (!window_)
        return nullptr;
    const NotificationCenter::Id entry = center_.add(text, severity, kind);
    while (toasts_.size() >= kMaxVisible)
        dismiss(toasts_.front(), false); // cũ nhất ra; mục của nó vẫn chưa đọc trong center
    auto* toast = new Toast(severity, text, std::move(action), window_);
    toast->entry_ = entry;
    connect(toast, &Toast::dismissed, this, [this, toast](bool byUser) { dismiss(toast, byUser); });
    connect(toast, &QObject::destroyed, this, [this, toast] { toasts_.removeOne(toast); }); // cùng window
    toasts_.append(toast);
    place();
    toast->show();
    toast->raise();
    announce(toast, text, severity == Severity::Error);
    toast->start(timeout_);
    return toast;
}

void ToastHost::followJobs(ProgressPresenter& presenter)
{
    // ponytail: câu chung theo tên việc; UIA-015 (event_map) thay bằng câu riêng theo loại, có hành động ("Xem").
    connect(&presenter, &ProgressPresenter::finished, this, [this](const jobs::JobInfo& job) {
        if (job.state == jobs::State::Failed) {
            const QString why = QString::fromStdString(job.error.message);
            post(Severity::Error, why.isEmpty() ? tr("%1 — lỗi").arg(job.name) : tr("%1 — lỗi: %2").arg(job.name, why));
        } else if (job.state == jobs::State::Done && job.showProgress) {
            post(Severity::Ok, tr("%1 — xong").arg(job.name));
        }
    });
}

void ToastHost::dismiss(Toast* toast, bool byUser)
{
    if (!toasts_.removeOne(toast))
        return; // đã đóng (vd. hành động vừa đăng toast mới đẩy nó ra)
    if (byUser)
        center_.markRead(toast->entry());
    toast->timer_.stop();
    returnFocus(toast, toast->back_);
    toast->hide();
    toast->deleteLater(); // có thể đang trong tín hiệu clicked của nút trên toast
    place();
}

void ToastHost::place()
{
    // .toasts: right 12px, bottom calc(--h-status + 12px), gap 8px; mới nhất ở dưới cùng
    if (!window_)
        return;
    const int margin = px(tok::sp_5), gap = px(tok::sp_4);
    const int maxWidth = std::min(kToastMaxWidth, window_->width() - 2 * margin);
    int bottom = window_->height() - px(tok::h_status) - margin;
    for (auto it = toasts_.crbegin(); it != toasts_.crend(); ++it) {
        const QSize s = (*it)->fit(maxWidth);
        (*it)->setGeometry(window_->width() - margin - s.width(), bottom - s.height(), s.width(), s.height());
        bottom -= s.height() + gap;
    }
}

bool ToastHost::eventFilter(QObject* watched, QEvent* e)
{
    if (watched == window_ && e->type() == QEvent::Resize)
        place();
    return false;
}

// -------------------------------------------------------------------------------- CanvasBanner

// Một banner (.banner): nền theo mức, nét --c-border ở đáy; [biểu tượng] chữ [+n thông báo khác] [hành động…] [×].
class BannerRow final : public QWidget {
    Q_OBJECT
public:
    BannerRow(QWidget* owner, const QString& id)
        : QWidget(owner), id(id), glyph_(new Glyph(px(tok::ic), this)), text_(new ElidedLabel(this)),
          more_(new QLabel(this)), actions_(new QHBoxLayout), close_(new Button({}, Button::Kind::Icon, this))
    {
        setFocusPolicy(Qt::NoFocus);
        setRole(this, QAccessible::StatusBar);
        setMinimumHeight(kBannerMinHeight);
        more_->hide();
        close_->setIconName(u"x"_s);
        close_->setFocusPolicy(Qt::TabFocus);
        auto* row = new QHBoxLayout(this);
        row->addWidget(glyph_);
        row->addWidget(text_, 1);
        row->addWidget(more_);
        row->addLayout(actions_);
        row->addWidget(close_);
        connect(close_, &QPushButton::clicked, this, &BannerRow::closeRequested);
        rememberFocus(this, &back);
        themed(this, [this, row] {
            // .banner: padding 4px 6px 4px 12px, viền đáy 1 px, gap 10px, chữ --fs-s; .acts gap 6px
            const int vertical = px(tok::sp_2);
            row->setContentsMargins(px(tok::sp_5), vertical, px(tok::sp_3), vertical + kBorder);
            row->setSpacing(px(tok::sp_4) + px(tok::sp_1));
            actions_->setSpacing(px(tok::sp_3));
            text_->setFont(theme::font(TextRole::Label));
            more_->setFont(theme::font(TextRole::Label));
            setInk(more_, col(tok::c_text_2));
        });
        retranslate();
    }

    void set(Severity s, const QString& text, const QList<MessageAction>& actions, const QString& icon)
    {
        severity_ = s;
        glyph_->set(icon.isEmpty() ? iconName(s) : icon, s == Severity::Info ? tok::c_accent : ink(s));
        text_->setText(text);
        setAccessibleName(text);
        for (Button* b : std::exchange(buttons_, {})) {
            b->hide();
            b->deleteLater(); // có thể đang trong clicked của chính nó
        }
        for (const MessageAction& a : actions) {
            auto* b = new Button(a.label, a.kind, this);
            b->setControlSize(Button::Size::Small);
            b->setFocusPolicy(Qt::TabFocus);
            connect(b, &QPushButton::clicked, this, [run = a.run] {
                if (run)
                    run();
            });
            actions_->addWidget(b);
            buttons_.push_back(b);
        }
        update();
    }

    void setMore(int count)
    {
        more_count_ = count;
        retranslate();
    }

    const QString id;
    bool announced = false; // đã đọc cho trình đọc màn hình (lúc hiện lần đầu)
    QPointer<QWidget> back; // focus trước khi Tab vào banner

signals:
    void closeRequested(); // bấm ×

protected:
    void changeEvent(QEvent* e) override
    {
        if (e->type() == QEvent::LanguageChange)
            retranslate();
        QWidget::changeEvent(e);
    }

    void paintEvent(QPaintEvent*) override
    {
        // .banner nền --c-banner; .warn, .err pha 14% màu mức với --c-base
        QPainter p(this);
        const bool tint = severity_ == Severity::Warning || severity_ == Severity::Error;
        p.fillRect(rect(), tint ? mix(col(ink(severity_)), col(tok::c_base), kBannerTint) : col(tok::c_banner));
        p.fillRect(QRect(0, height() - kBorder, width(), kBorder), col(tok::c_border));
    }

private:
    void retranslate()
    {
        close_->setAccessibleName(CanvasBanner::tr("Đóng banner"));
        more_->setText(CanvasBanner::tr("+%n thông báo khác", nullptr, more_count_));
        more_->setVisible(more_count_ > 0);
    }

    Glyph* glyph_;
    ElidedLabel* text_;
    QLabel* more_;
    QHBoxLayout* actions_;
    Button* close_;
    std::vector<Button*> buttons_;
    Severity severity_ = Severity::Info;
    int more_count_ = 0;
};

CanvasBanner::CanvasBanner(QWidget* canvas) : QWidget(canvas), rows_(new QVBoxLayout(this))
{
    setFocusPolicy(Qt::NoFocus);
    rows_->setContentsMargins({});
    rows_->setSpacing(0);
    canvas->installEventFilter(this);
    connect(&tm(), &theme::ThemeManager::changed, this, &CanvasBanner::refresh);
    hide();
}

void CanvasBanner::add(const QString& id, Severity severity, const QString& text, const QList<MessageAction>& actions,
                       const QString& icon)
{
    auto it = std::find_if(list_.begin(), list_.end(), [&](const BannerRow* r) { return r->id == id; });
    BannerRow* row = it != list_.end() ? *it : nullptr;
    const bool replaced = row && row->announced; // đang hiện mà đổi chữ: đọc lại
    if (!row) {
        row = list_.emplace_back(new BannerRow(this, id));
        rows_->addWidget(row);
        connect(row, &BannerRow::closeRequested, this, [this, row] {
            const QString closedId = row->id; // drop() hủy hàng (deleteLater)
            drop(row);
            emit closed(closedId);
        });
    }
    row->set(severity, text, actions, icon);
    refresh();
    if (replaced)
        announce(row, text, false);
}

void CanvasBanner::remove(const QString& id)
{
    if (QWidget* r = row(id))
        drop(static_cast<BannerRow*>(r));
}

void CanvasBanner::drop(BannerRow* row)
{
    std::erase(list_, row);
    returnFocus(row, row->back);
    row->hide();
    row->deleteLater();
    refresh();
}

bool CanvasBanner::contains(const QString& id) const
{
    return row(id);
}

QStringList CanvasBanner::ids() const
{
    QStringList out;
    for (const BannerRow* r : list_)
        out << r->id;
    return out;
}

int CanvasBanner::visibleCount() const
{
    return std::min(int(list_.size()), kMaxVisible);
}

QWidget* CanvasBanner::row(const QString& id) const
{
    auto it = std::find_if(list_.begin(), list_.end(), [&](const BannerRow* r) { return r->id == id; });
    return it != list_.end() ? *it : nullptr;
}

void CanvasBanner::refresh()
{
    const int n = int(list_.size()), shown = visibleCount();
    for (int i = 0; i < n; ++i) {
        BannerRow* r = list_[i];
        r->setVisible(i < shown);
        r->setMore(i == shown - 1 ? n - shown : 0);
        if (i < shown && !std::exchange(r->announced, true)) // mới, hoặc hết chờ vì banner trước đã đóng
            announce(r, r->accessibleName(), false);
    }
    setVisible(n > 0);
    if (QWidget* canvas = parentWidget()) {
        rows_->invalidate(); // hàng vừa hiện/ẩn: tính lại cao
        setGeometry(0, 0, canvas->width(), sizeHint().height());
    }
    raise();
}

bool CanvasBanner::eventFilter(QObject* watched, QEvent* e)
{
    if (watched == parentWidget() && e->type() == QEvent::Resize)
        refresh();
    return false;
}

// -------------------------------------------------------------------------------- CursorTip

CursorTip::CursorTip(QWidget* canvas)
    : QWidget(canvas, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowTransparentForInput), canvas_(canvas),
      glyph_(new Glyph(kTipIcon, this)), text_(new QLabel(this)), timer_(this)
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    setRole(this, QAccessible::StatusBar);
    text_->setTextFormat(Qt::PlainText);
    auto* row = new QHBoxLayout(this);
    row->addWidget(glyph_);
    row->addWidget(text_);
    timer_.setSingleShot(true);
    timer_.setTimerType(Qt::PreciseTimer);
    connect(&timer_, &QTimer::timeout, this, &QWidget::hide);
}

QString CursorTip::text() const
{
    return text_->text();
}

void CursorTip::showAt(const QPoint& pos, const QString& text, Severity severity)
{
    if (!canvas_)
        return;
    // .ctip: padding 3px 8px trong viền 1 px, chữ --fs-s; nền, chữ dyn.tooltip.* của vùng vẽ (07.1 §13.2)
    severity_ = severity;
    const int horizontal = kBorder + px(tok::sp_4), vertical = kBorder + px(tok::sp_1) + 1;
    layout()->setContentsMargins(horizontal, vertical, horizontal, vertical);
    layout()->setSpacing(px(tok::sp_2));
    QPalette pal = palette();
    pal.setColor(QPalette::Window, tm().color(Sem::DynTooltipBg)); // góc ngoài nét bo của cửa sổ
    pal.setColor(QPalette::WindowText, tm().color(Sem::DynTooltipText));
    setPalette(pal);
    text_->setFont(theme::font(TextRole::Label));
    text_->setText(text);
    setAccessibleName(text);
    glyph_->set(iconName(severity), ink(severity), true);
    adjustSize();

    // Lệch kOffset sang phải, xuống dưới; tràn mép canvas thì sang trái, lên trên, vẫn cách điểm ≥ kOffset.
    const QRect area(canvas_->mapToGlobal(QPoint(0, 0)), canvas_->size());
    const QPoint at = canvas_->mapToGlobal(pos);
    QPoint p = at + kOffset;
    if (p.x() + width() > area.right() + 1)
        p.setX(at.x() - kOffset.x() - width());
    if (p.y() + height() > area.bottom() + 1)
        p.setY(at.y() - kOffset.y() - height());
    move(p);
    show(); // cửa sổ ToolTip luôn ở trên
    timer_.start(timeout_);
    announce(this, text, false);
}

void CursorTip::paintEvent(QPaintEvent*)
{
    // .ctip: viền --c-danger (lỗi) hoặc --c-dyn-b (.ctip.info), bo --r; màu của vùng vẽ như nền
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(
        QPen(severity_ == Severity::Error ? canvasCol(tok::c_danger) : tm().color(Sem::DynTooltipBorder), kBorder));
    p.setBrush(tm().color(Sem::DynTooltipBg));
    const qreal r = tm().px(tok::r);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), r, r);
}

// -------------------------------------------------------------------------------- EmptyState

EmptyState::EmptyState(const QString& icon, QWidget* parent)
    : QWidget(parent), glyph_(new Glyph(kEmptyIcon, this)), title_(new QLabel(this)), explanation_(new QLabel(this)),
      action_(new Button({}, Button::Kind::Normal, this))
{
    auto* box = new QVBoxLayout(this);
    box->addWidget(glyph_, 0, Qt::AlignHCenter);
    box->addWidget(title_);
    box->addWidget(explanation_);
    box->addWidget(action_, 0, Qt::AlignHCenter);
    box->addStretch(); // .empty nằm đầu vùng chứa
    for (QLabel* l : {title_, explanation_}) {
        l->setTextFormat(Qt::PlainText);
        l->setWordWrap(true);
        l->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        l->hide();
    }
    action_->setControlSize(Button::Size::Small);
    action_->hide();
    connect(action_, &QPushButton::clicked, this, &EmptyState::actionTriggered);
    glyph_->set(icon, tok::c_text_3);
    themed(this, [this, box] {
        // .empty: padding 24px 16px, gap 8px; tiêu đề đậm --c-text, giải thích --c-text-2
        box->setContentsMargins(px(tok::sp_6), px(tok::sp_7), px(tok::sp_6), px(tok::sp_7));
        box->setSpacing(px(tok::sp_4));
        title_->setFont(theme::font(TextRole::Ui, Weight::Bold));
        explanation_->setFont(theme::font(TextRole::Ui));
        setInk(title_, col(tok::c_text));
        setInk(explanation_, col(tok::c_text_2));
    });
}

void EmptyState::setIconName(const QString& icon)
{
    glyph_->set(icon, tok::c_text_3);
}

void EmptyState::setText(const QString& title, const QString& explanation)
{
    title_->setText(title);
    title_->setVisible(!title.isEmpty());
    explanation_->setText(explanation);
    explanation_->setVisible(!explanation.isEmpty());
}

void EmptyState::setAction(const QString& label)
{
    action_->setText(label);
    action_->setVisible(!label.isEmpty());
}

// -------------------------------------------------------------------------------- FieldMessage

FieldMessage::FieldMessage(QWidget* field, QWidget* parent) : QLabel(parent), field_(field)
{
    setTextFormat(Qt::PlainText);
    setWordWrap(true);
    setMinimumHeight(kFieldMinHeight);
    setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // AlignTop thì QLabel cộng cả indent vào chiều cao
    themed(this, [this] { setFont(theme::font(TextRole::Caption)); });
}

void FieldMessage::setError(const QString& text)
{
    set(text, true);
}

void FieldMessage::setHint(const QString& text)
{
    set(text, false);
}

void FieldMessage::set(const QString& text, bool error)
{
    const bool fresh = error && !text.isEmpty() && (!error_ || text != this->text());
    error_ = error && !text.isEmpty();
    setIndent(error_ ? kFieldIcon + px(tok::sp_2) : 0); // chỗ biểu tượng + gap 4px
    setText(text);
    if (field_)
        field_->setAccessibleDescription(text);
    if (fresh)
        announce(field_ ? field_.data() : static_cast<QWidget*>(this), text, false); // aria-live="polite"
    update();
}

void FieldMessage::paintEvent(QPaintEvent*)
{
    if (text().isEmpty())
        return;
    QPainter p(this);
    const QColor c = col(error_ ? tok::c_danger : tok::c_text_3);
    const QRect r = contentsRect().adjusted(indent(), 0, 0, 0);
    const int flags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap;
    if (error_) { // biểu tượng giữa dòng đầu
        const int top = p.fontMetrics().boundingRect(r, flags, text()).top();
        const qreal dpr = p.device()->devicePixelRatio();
        p.drawPixmap(contentsRect().left(), top + (p.fontMetrics().height() - kFieldIcon) / 2,
                     tinted(IconProvider::instance().pixmap(u"warn"_s, kFieldIcon, dpr, IconState::Normal), c));
    }
    p.setPen(c);
    p.drawText(r, flags, text());
}

} // namespace trion::ui

#include "Messages.moc"
