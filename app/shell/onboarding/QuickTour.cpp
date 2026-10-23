#include "app/shell/onboarding/QuickTour.h"

#include "app/core/a11y/Announce.h"
#include "app/shell/MainWindow.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/dock/AutoHideStrip.h"
#include "app/widgets/dock/PaletteHost.h"

#include <QBoxLayout>
#include <QDockWidget>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace trion::shell {

namespace {

using Region = MainWindow::Region;
using theme::TextRole;
using theme::Weight;

constexpr int kCardWidth = 300; // .mcard width:300px
constexpr int kCardPadX = 10;   // .mcard padding: 8px 10px
constexpr int kRingGap = 3;     // viền: hộp quanh vùng rộng thêm 3 px mỗi phía, nét 2 px phía ngoài
constexpr int kOffset = 10;     // thẻ cách vùng 10 px
constexpr int kInset = 20;      // vùng cao hơn nửa cửa sổ: thẻ cách góc trên trái của vùng 20 px
constexpr int kMargin = 8;      // thẻ cách mép cửa sổ ít nhất 8 px

// Câu của từng bước, theo thứ tự (prototype SC.tour).
const char* const kText[QuickTour::kStops] = {
    QT_TRANSLATE_NOOP("trion::shell::QuickTour",
                      "Lăn chuột để zoom quanh con trỏ, giữ chuột giữa để pan. Nhấp đúp chuột giữa để xem toàn bộ."),
    QT_TRANSLATE_NOOP("trion::shell::QuickTour",
                      "Tab Đo › Đo nhanh: rê chuột thấy khoảng cách, bấm trong một phòng để có diện tích."),
    QT_TRANSLATE_NOOP("trion::shell::QuickTour",
                      "Chọn một đối tượng: palette Thuộc tính hiện thông tin, bấm vào giá trị để sao chép."),
    QT_TRANSLATE_NOOP("trion::shell::QuickTour", "Chỉ báo Bác sĩ Font: chữ TCVN3 và font thiếu đã được hiển thị đúng."),
    QT_TRANSLATE_NOOP("trion::shell::QuickTour", "Dòng lệnh nhận mọi phím gõ. Gõ “in” hoặc PLOT để in, xuất PDF."),
};

theme::ThemeManager& tm()
{
    return theme::ThemeManager::instance();
}

int px(tok::PxId id)
{
    return qRound(tm().px(id));
}

// Lớp phủ: --c-scrim ngoài vùng tô sáng (box-shadow 0 0 0 9999px của prototype), viền 2 px --c-accent quanh vùng, bo
// --r-xl. Không nhận chuột, không nhận focus.
class Scrim : public QWidget {
public:
    explicit Scrim(QWidget* window) : QWidget(window)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
    }
    void setHole(const QRect& hole)
    {
        if (hole == hole_)
            return;
        hole_ = hole;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath dim;
        dim.addRect(rect());
        if (hole_.isEmpty()) {
            p.fillPath(dim, tm().color(tok::c_scrim));
            return;
        }
        const QRectF ring = QRectF(hole_).adjusted(-kRingGap, -kRingGap, kRingGap, kRingGap);
        const double r = tm().px(tok::r_xl);
        dim.addRoundedRect(ring, r, r); // OddEvenFill: lỗ
        p.fillPath(dim, tm().color(tok::c_scrim));
        p.setPen(QPen(tm().color(tok::c_accent), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(ring.adjusted(1, 1, -1, -1), r - 1, r - 1);
    }

private:
    QRect hole_;
};

// Thẻ hướng dẫn (.mcard): nền --c-raised, viền 1 px --c-border-ctl, bo --r-lg, đệm 8/10 px, các dòng cách 6 px.
// ponytail: chưa có bóng --sh-pop (như toast, menu: viền tách khỏi nền); UI0-109 thêm bóng chung cho thẻ nổi.
class Card : public QWidget {
public:
    explicit Card(QWidget* window) : QWidget(window) {}

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const double r = tm().px(tok::r_lg);
        p.setPen(QPen(tm().color(tok::c_border_ctl), 1));
        p.setBrush(tm().color(tok::c_raised));
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), r, r);
    }
};

// Chữ màu token, tô lại khi đổi chủ đề.
void ink(QWidget* w, tok::ColorId id)
{
    const auto apply = [w, id] {
        QPalette p = w->palette();
        p.setColor(QPalette::WindowText, tm().color(id));
        w->setPalette(p);
    };
    apply();
    QObject::connect(&tm(), &theme::ThemeManager::changed, w, apply);
}

} // namespace

QuickTour::QuickTour(MainWindow& window) : QObject(&window), window_(window)
{
    overlay_ = new Scrim(&window);
    overlay_->hide();
    card_ = new Card(&window);
    card_->setObjectName(u"quickTour"_s);
    card_->hide();
    auto* v = new QVBoxLayout(card_);
    v->setContentsMargins(kCardPadX, px(tok::sp_4), kCardPadX, px(tok::sp_4));
    v->setSpacing(px(tok::sp_3));
    title_ = new QLabel;
    title_->setTextFormat(Qt::PlainText);
    title_->setFont(theme::font(TextRole::Label, Weight::Bold));
    text_ = new QLabel;
    text_->setTextFormat(Qt::PlainText);
    text_->setWordWrap(true);
    text_->setFont(theme::font(TextRole::Label));
    ink(text_, tok::c_text_2);
    end_ = new ui::Button({}, ui::Button::Kind::Ghost);
    end_->setObjectName(u"end"_s);
    next_ = new ui::Button({}, ui::Button::Kind::Primary);
    next_->setObjectName(u"next"_s);
    for (ui::Button* b : {end_, next_}) {
        b->setControlSize(ui::Button::Size::Small);
        b->installEventFilter(this);
    }
    auto* acts = new QHBoxLayout;
    acts->setSpacing(px(tok::sp_3));
    acts->addWidget(end_);
    acts->addStretch(1);
    acts->addWidget(next_);
    v->addWidget(title_);
    v->addWidget(text_);
    v->addLayout(acts);
    connect(end_, &QPushButton::clicked, this, [this] { finish(false); });
    connect(next_, &QPushButton::clicked, this, &QuickTour::next);
    card_->installEventFilter(this);
    window.installEventFilter(this);
}

QuickTour::~QuickTour()
{
    delete card_;
    delete overlay_;
}

void QuickTour::start()
{
    for (int stop = 0; stop < kStops; ++stop)
        if (resolve(stop)) {
            overlay_->setGeometry(window_.rect());
            overlay_->show();
            overlay_->raise();
            card_->show();
            card_->raise();
            show(stop);
            return;
        }
    current_ = 0;
    finish(true); // không vùng nào đang hiện
}

void QuickTour::next()
{
    if (current_ < 0)
        return;
    for (int stop = current_ + 1; stop < kStops; ++stop)
        if (resolve(stop)) {
            show(stop);
            return;
        }
    finish(true);
}

void QuickTour::previous()
{
    for (int stop = current_ - 1; stop >= 0; --stop)
        if (resolve(stop)) {
            show(stop);
            return;
        }
}

void QuickTour::finish(bool completed)
{
    if (current_ < 0)
        return;
    current_ = -1;
    if (target_)
        target_->removeEventFilter(this);
    target_ = nullptr;
    if (!window_.focusWidget() || card_->isAncestorOf(window_.focusWidget()))
        window_.palettes().focusCanvas(); // trước khi ẩn thẻ: Qt không chuyển focus sang widget kế tiếp bất kỳ
    overlay_->hide();
    card_->hide();
    emit finished(completed);
    deleteLater();
}

QString QuickTour::targetName() const
{
    switch (current_) {
    case 0:
        return MainWindow::regionName(Region::Canvas);
    case 1:
        return MainWindow::regionName(Region::Ribbon);
    case 2:
        return tr("Palette Thuộc tính");
    case 3:
        return MainWindow::regionName(Region::StatusBar);
    case 4:
        return MainWindow::regionName(Region::CommandLine);
    default:
        return {};
    }
}

QWidget* QuickTour::resolve(int stop) const
{
    const auto shown = [this](QWidget* w) -> QWidget* {
        return w && w->isVisible() && w->window() == &window_ ? w : nullptr;
    };
    switch (stop) {
    case 0:
        return shown(window_.region(Region::Canvas));
    case 1:
        return shown(window_.region(Region::Ribbon));
    case 2: { // palette Thuộc tính: dock đang mở; thu ở dải (màn hình hẹp): tab của nó trên dải
        QDockWidget* dock = window_.palettes().dock(u"props"_s);
        if (!dock)
            return nullptr;
        if (!dock->isFloating() && shown(dock))
            return dock;
        for (const Qt::DockWidgetArea side : {Qt::RightDockWidgetArea, Qt::LeftDockWidgetArea})
            if (const ui::AutoHideStrip* strip = window_.palettes().strip(side); strip && strip->contains(dock))
                return shown(strip->tab(dock));
        return nullptr;
    }
    case 3: { // thanh trạng thái; chỉ báo Bác sĩ Font (#sb-font) khi đang hiện
        QWidget* bar = shown(window_.region(Region::StatusBar));
        QWidget* font = bar ? shown(bar->findChild<QWidget*>(u"fontDoctor"_s)) : nullptr;
        return font ? font : bar;
    }
    case 4:
        return shown(window_.region(Region::CommandLine));
    default:
        return nullptr;
    }
}

void QuickTour::show(int stop)
{
    if (target_)
        target_->removeEventFilter(this);
    current_ = stop;
    target_ = resolve(stop);
    if (target_)
        target_->installEventFilter(this);
    retranslate();
    next_->setFocus(Qt::OtherFocusReason);
    a11y::announce(card_, next_->accessibleDescription());
    emit stepChanged(stop);
}

void QuickTour::retranslate()
{
    if (current_ < 0)
        return;
    bool last = true;
    for (int stop = current_ + 1; stop < kStops && last; ++stop)
        last = !resolve(stop);
    const QString step = QString::number(current_ + 1);
    const QString count = QString::number(kStops);
    const QString text = tr(kText[current_]);
    title_->setText(tr("Bước %1 / %2").arg(step, count));
    text_->setText(text);
    end_->setText(tr("Kết thúc"));
    next_->setText(last ? tr("Xong") : tr("Tiếp"));
    card_->setAccessibleName(tr("Hướng dẫn nhanh"));
    overlay_->setAccessibleName(targetName());
    next_->setAccessibleDescription(tr("Hướng dẫn nhanh, bước %1 / %2: %3. %4").arg(step, count, targetName(), text));
    place();
}

void QuickTour::place()
{
    if (current_ < 0)
        return;
    overlay_->setGeometry(window_.rect());
    const QRect r =
        target_ && target_->isVisible() ? QRect(target_->mapTo(&window_, QPoint()), target_->size()) : QRect();
    static_cast<Scrim*>(overlay_)->setHole(r);
    const int w = kCardWidth;
    const int h = card_->heightForWidth(w);
    const QSize area = window_.size();
    const int maxX = std::max(kMargin, area.width() - w - kMargin);
    QPoint at;
    if (r.isEmpty()) {
        at = QPoint((area.width() - w) / 2, (area.height() - h) / 2);
    } else if (r.height() > area.height() / 2) { // vùng vẽ, palette cao: thẻ nằm trong vùng
        at = QPoint(std::clamp(r.left() + kInset, kMargin, maxX), r.top() + kInset);
    } else { // dưới vùng, không đủ chỗ thì trên
        const int below = r.top() + r.height() + kOffset;
        at = QPoint(std::clamp(r.center().x() - w / 2, kMargin, maxX),
                    below + h > area.height() ? r.top() - h - kOffset : below);
    }
    card_->setGeometry(QRect(at, QSize(w, h)));
}

bool QuickTour::eventFilter(QObject* watched, QEvent* e)
{
    const QEvent::Type type = e->type();
    if (watched == &window_ || watched == target_) {
        if (type == QEvent::Resize || type == QEvent::Move || type == QEvent::Show || type == QEvent::Hide)
            place();
        return false;
    }
    if (watched == card_) {
        if (type == QEvent::LanguageChange)
            retranslate();
        return false;
    }
    if (type != QEvent::ShortcutOverride && type != QEvent::KeyPress)
        return false;
    auto* k = static_cast<QKeyEvent*>(e);
    if (k->modifiers() & ~Qt::KeypadModifier)
        return false;
    const int key = k->key();
    if (key != Qt::Key_Escape && key != Qt::Key_Left && key != Qt::Key_Right && key != Qt::Key_Up &&
        key != Qt::Key_Down)
        return false;
    e->accept(); // ShortcutOverride: phím tắt của cửa sổ (Esc của lệnh…) không lấy phím này
    if (type == QEvent::KeyPress) {
        if (key == Qt::Key_Escape)
            finish(false);
        else if (key == Qt::Key_Right || key == Qt::Key_Down)
            next();
        else
            previous();
    }
    return true;
}

} // namespace trion::shell
