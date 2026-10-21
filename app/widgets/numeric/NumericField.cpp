#include "app/widgets/numeric/NumericField.h"

#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/dialog/DialogFrame.h"
#include "core/regen/numfmt/NumFormat.h"

#include <QAbstractButton>
#include <QAccessible>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMetaMethod>
#include <QPainter>
#include <QVBoxLayout>

#include <algorithm>
#include <iterator>
#include <numbers>
#include <string_view>

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

// Giá trị của prototype không có token riêng (css/app.css .numf, .fmsg).
constexpr int kBorder = 1;               // viền hộp; nét chia trước ⌖ và nút bước (border-left)
constexpr int kRing = 2;                 // viền focus/lỗi (viền 1 px + bóng 1 px), vẽ trong mép như UI0-106
constexpr int kPickW = 22;               // .numf .pick: rộng gồm nét chia; ổ khóa cũng chiếm chỗ này
constexpr int kStepW = 16;               // .numf .stepr
constexpr int kPad = 7;                  // .numf input: padding 0 7px
constexpr int kLineEditMargin = 2;       // lề chữ sẵn có của QLineEdit (QLineEditPrivate::horizontalMargin)
constexpr int kChevron = 7;              // .numf .stepr .ic: 7 px, nét 2 trên lưới 16
constexpr int kWarn = 9;                 // .fmsg: TC.ic('warn', 'xs')
constexpr qreal kDisabledOpacity = 0.55; // .numf.is-dis
constexpr int kHintDigits = 10;          // bề rộng gợi ý: 10 chữ số Mono (-1250.0000)
constexpr int kMinDigits = 4;

// Biểu tượng một màu tô lại bằng màu c (alpha giữ nguyên).
QPixmap tinted(QPixmap pm, const QColor& c)
{
    QPainter p(&pm);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(pm.rect(), c);
    return pm;
}

// Nút trong hộp (.numf .pick, .stepr button): phẳng, không nhận focus (tabindex=-1); rê --c-hover, nhấn --c-press.
class Part : public QAbstractButton {
public:
    enum Glyph { Pick, Up, Down };

    Part(Glyph glyph, QWidget* parent) : QAbstractButton(parent), glyph_(glyph)
    {
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover);
        setObjectName(glyph == Pick ? u"pick"_s : glyph == Up ? u"up"_s : u"down"_s);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const bool en = isEnabled();
        const bool hot = en && (underMouse() || isDown());
        if (hot)
            p.fillRect(rect(), col(isDown() ? tok::c_press : tok::c_hover));
        if (glyph_ == Pick) {
            const int ic = px(tok::ic);
            const IconState state = !en ? IconState::Disabled : hot ? IconState::Hover : IconState::Normal;
            p.drawPixmap((width() - ic) / 2, (height() - ic) / 2,
                         IconProvider::instance().pixmap(u"idpoint"_s, ic, p.device()->devicePixelRatio(), state));
            return;
        }
        // Mũi tên của prototype (icons.js up: M3.5 10 8 5.5l4.5 4.5, down: M3.5 6 8 10.5 12.5 6) ở 7 px, nét 2.
        const qreal u = kChevron / 16.0;
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(col(!en   ? tok::c_text_dis
                          : hot ? tok::c_text
                                : tok::c_text_2),
                      2 * u, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        const QPointF o = QRectF(rect()).center() - QPointF(8, 8) * u;
        const qreal edge = glyph_ == Up ? 10 : 6, tip = glyph_ == Up ? 5.5 : 10.5;
        p.drawPolyline(
            QList<QPointF>{o + QPointF(3.5, edge) * u, o + QPointF(8, tip) * u, o + QPointF(12.5, edge) * u});
    }

private:
    Glyph glyph_;
};

// Dòng lỗi dưới hộp (.fmsg.err): biểu tượng cảnh báo + câu --fs-xs --c-danger, xuống dòng khi dài. QLabel cho cỡ theo
// bề rộng, trợ năng (StaticText), findTruncations; tự vẽ để màu đọc token mỗi lần vẽ.
class Message : public QLabel {
public:
    explicit Message(QWidget* parent) : QLabel(parent)
    {
        setTextFormat(Qt::PlainText);
        setWordWrap(true);
        setFont(theme::font(theme::TextRole::Caption));
        setIndent(kWarn + px(tok::sp_2));               // chỗ biểu tượng + gap 4px
        setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // AlignTop thì QLabel cộng cả indent vào chiều cao
        hide();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const QColor ink = col(tok::c_danger);
        const QRect r = contentsRect().adjusted(indent(), 0, 0, 0);
        const int flags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap;
        const int top = p.fontMetrics().boundingRect(r, flags, text()).top(); // biểu tượng giữa dòng đầu
        const QPixmap warn =
            IconProvider::instance().pixmap(u"warn"_s, kWarn, p.device()->devicePixelRatio(), IconState::Normal);
        p.drawPixmap(0, top + (p.fontMetrics().height() - kWarn) / 2, tinted(warn, ink));
        p.setPen(ink);
        p.drawText(r, flags, text());
    }
};

} // namespace

// Hộp .numf: vẽ nền, viền, nét chia, ổ khóa, viền focus/lỗi; đặt ô chữ, ⌖, nút bước (không bố cục Qt: vài phép trừ).
class NumericField::Box : public QWidget {
public:
    explicit Box(NumericField* f) : QWidget(f), f_(f) { setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed); }
    QSize sizeHint() const override { return {f_->boxWidth(kHintDigits), px(tok::h_ctl)}; }
    QSize minimumSizeHint() const override { return {f_->boxWidth(kMinDigits), px(tok::h_ctl)}; }

protected:
    void resizeEvent(QResizeEvent*) override { f_->placeParts(); }

    void paintEvent(QPaintEvent*) override;

private:
    NumericField* f_;
};

// Ô chữ trong hộp: không khung, nền trong suốt (hộp vẽ nền); phím Enter, Esc, ↑ ↓; rời ô thì nhận; *Khác nhau*.
class NumericField::Edit : public QLineEdit {
public:
    explicit Edit(NumericField* f) : QLineEdit(f->box_), f_(f)
    {
        setFrame(false);
        setFont(theme::font(theme::TextRole::MonoLabel));
        // Chữ cách mép ngoài hộp kBorder + kPad (prototype); ô chữ đã lùi kRing, QLineEdit tự chừa kLineEditMargin.
        setTextMargins(kBorder + kPad - kRing - kLineEditMargin, 0, kPad - kLineEditMargin, 0);
        QPalette pal = palette();
        pal.setColor(QPalette::Base, Qt::transparent);
        setPalette(pal);
    }

protected:
    bool event(QEvent* e) override
    {
        // ↑ ↓, Esc (khi có chữ để trả lại) là của ô, không thành phím tắt của cửa sổ.
        if (e->type() == QEvent::ShortcutOverride) {
            const auto* k = static_cast<QKeyEvent*>(e);
            const bool plain = !(k->modifiers() & ~(Qt::ShiftModifier | Qt::KeypadModifier));
            if (plain && (k->key() == Qt::Key_Up || k->key() == Qt::Key_Down ||
                          (k->key() == Qt::Key_Escape && f_->isEdited()))) {
                e->accept();
                return true;
            }
        }
        return QLineEdit::event(e);
    }

    void keyPressEvent(QKeyEvent* e) override
    {
        switch (e->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            // Đúng hoặc chưa sửa: Enter đi tiếp tới hộp thoại (nút xác nhận); sai: dừng ở ô.
            f_->commit() ? e->ignore() : e->accept();
            return;
        case Qt::Key_Escape:
            f_->revert() ? e->accept() : e->ignore();
            return;
        case Qt::Key_Up:
        case Qt::Key_Down:
            f_->stepBy(e->key() == Qt::Key_Up ? 1 : -1, e->modifiers() & Qt::ShiftModifier);
            return;
        default:
            QLineEdit::keyPressEvent(e);
        }
    }

    void focusInEvent(QFocusEvent* e) override
    {
        QLineEdit::focusInEvent(e);
        f_->box_->update(); // viền focus
    }

    void focusOutEvent(QFocusEvent* e) override
    {
        QLineEdit::focusOutEvent(e);
        f_->box_->update();
        if (e->reason() != Qt::PopupFocusReason) // menu chuột phải của ô: chưa rời ô
            f_->commit();                        // cuối cùng: nơi nhận valueCommitted có thể hủy ô
    }

    void paintEvent(QPaintEvent* e) override
    {
        QLineEdit::paintEvent(e);
        if (!f_->mixed_ || !text().isEmpty())
            return;
        // *Khác nhau* (.varies): chữ giao diện nghiêng --c-text-3, ở chỗ chữ của ô.
        QPainter p(this);
        QFont italic = theme::font(theme::TextRole::Label);
        italic.setItalic(true);
        p.setFont(italic);
        p.setPen(col(tok::c_text_3));
        const QMargins m = textMargins();
        p.drawText(rect().adjusted(m.left() + kLineEditMargin, 0, -m.right() - kLineEditMargin, 0),
                   Qt::AlignLeft | Qt::AlignVCenter, NumericField::tr("*Khác nhau*"));
    }

private:
    NumericField* f_;
};

void NumericField::Box::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (!isEnabled())
        p.setOpacity(kDisabledOpacity);
    const qreal r = tm().px(tok::r);
    const QRectF box(rect());
    p.setPen(QPen(col(tok::c_border_ctl), kBorder));
    p.setBrush(col(tok::c_raised));
    p.drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), r, r);
    for (const QWidget* part : {f_->pick_, f_->up_})
        if (!part->isHidden())
            p.fillRect(QRect(part->x() - kBorder, kBorder, kBorder, height() - 2 * kBorder), col(tok::c_border));
    if (f_->locked_) { // ổ khóa ở chỗ của ⌖
        const int ic = px(tok::ic), x = f_->edit_->geometry().right() + 1;
        p.drawPixmap(x + (width() - kBorder - x - ic) / 2, (height() - ic) / 2,
                     IconProvider::instance().pixmap(u"lock"_s, ic, p.device()->devicePixelRatio(), IconState::Normal));
    }
    const QColor ring = f_->hasError() ? col(tok::c_danger) : f_->edit_->hasFocus() ? col(tok::c_focus) : QColor();
    if (ring.isValid()) {
        p.setPen(QPen(ring, kRing));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(box.adjusted(1, 1, -1, -1), r, r);
    }
}

NumericField::NumericField(ValueKind kind, QWidget* parent)
    : QWidget(parent), box_(new Box(this)), edit_(new Edit(this)), pick_(new Part(Part::Pick, box_)),
      up_(new Part(Part::Up, box_)), down_(new Part(Part::Down, box_)), msg_(new Message(this))
{
    spec_.kind = kind;
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(px(tok::sp_1)); // .nfw gap 2px
    layout->addWidget(box_);
    layout->addWidget(msg_);
    setFocusProxy(edit_);
    connect(pick_, &QAbstractButton::clicked, this, &NumericField::startPick);
    for (QAbstractButton* b : {up_, down_})
        connect(b, &QAbstractButton::clicked, this,
                [this, b] { stepBy(b == up_ ? 1 : -1, QGuiApplication::keyboardModifiers() & Qt::ShiftModifier); });
    updateParts();
    display();
    retranslate();
}

// Hủy ô chữ khi ô còn nguyên: hủy sau (~QWidget) thì ô chữ đang focus nhận FocusOut và nhận chữ vào ô đã hủy.
NumericField::~NumericField()
{
    delete box_;
}

void NumericField::setSpec(const ValueSpec& spec)
{
    spec_ = spec;
    updateParts(); // ⌖ theo loại ô
    display();
}

void NumericField::setLabel(const QString& label)
{
    label_ = label;
    updateAccessible();
}

void NumericField::setValue(double value)
{
    mixed_ = false;
    value_ = value;
    display();
}

void NumericField::setMixed()
{
    mixed_ = true;
    display();
}

void NumericField::setStep(double step)
{
    step_ = step;
}

void NumericField::setLowerBound(std::optional<double> bound)
{
    lowerBound_ = bound;
}

void NumericField::setPickEnabled(bool on)
{
    pickEnabled_ = on;
    updateParts();
}

void NumericField::setLocked(bool locked)
{
    locked_ = locked;
    if (locked)
        revert();
    edit_->setReadOnly(locked);
    updateParts();
}

void NumericField::finishPick(double value)
{
    if (endPick())
        offer(value);
}

void NumericField::cancelPick()
{
    endPick();
}

void NumericField::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

QString NumericField::format(double value) const
{
    char buf[regen::kMaxNumText];
    return QString::fromUtf8(buf, qsizetype(formatValue(value, spec_, buf)));
}

QString NumericField::check(const QString& text, double& value) const
{
    const QByteArray utf8 = text.toUtf8();
    const std::string_view s(utf8.constData(), std::size_t(utf8.size()));
    const ValueResult r = parseValue(s, spec_);
    if (!r)
        return errorText(r.error, s, spec_);
    value = r.value;
    if (!lowerBound_ || r.value > *lowerBound_)
        return {};
    const QString bound = *lowerBound_ == 0 ? u"0"_s : format(*lowerBound_);
    return label_.isEmpty() ? tr("Phải lớn hơn %1.").arg(bound) : tr("%1 phải lớn hơn %2.").arg(label_, bound);
}

// Đơn vị cho tên đọc (chữ trong ô chỉ có số, trừ feet-inch).
QString NumericField::unitName() const
{
    // INSUNITS 1–24.
    static constexpr const char* kLinear[] = {
        nullptr,
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "inch"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "feet"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "dặm"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "milimét"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "xentimét"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "mét"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "kilômét"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "microinch"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "mil"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "yard"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "angstrom"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "nanômét"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "micrômét"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "đềximét"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "đềcamét"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "héctômét"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "gigamét"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "đơn vị thiên văn"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "năm ánh sáng"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "parsec"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "feet khảo sát Mỹ"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "inch khảo sát Mỹ"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "yard khảo sát Mỹ"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "dặm khảo sát Mỹ"),
    };
    // AUNITS 0–4.
    static constexpr const char* kAngular[] = {
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "độ"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "độ phút giây"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "grad"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "radian"),
        QT_TRANSLATE_NOOP("trion::ui::NumericField", "phương vị"),
    };
    using regen::LinearUnits;
    if (spec_.kind == ValueKind::Distance) {
        if (spec_.units.lunits == LinearUnits::Engineering || spec_.units.lunits == LinearUnits::Architectural)
            return tr("feet và inch");
        return spec_.insunits > 0 && spec_.insunits < int(std::size(kLinear)) ? tr(kLinear[spec_.insunits]) : QString();
    }
    const auto a = std::size_t(spec_.units.aunits);
    return spec_.kind == ValueKind::Angle && a < std::size(kAngular) ? tr(kAngular[a]) : QString();
}

double NumericField::stepSize() const
{
    return step_ > 0 ? step_ : spec_.kind == ValueKind::Angle ? std::numbers::pi / 180 : 1.0;
}

// Bề rộng hộp đủ cho `digits` chữ số Mono, và cho *Khác nhau*.
int NumericField::boxWidth(int digits) const
{
    QFont italic = theme::font(theme::TextRole::Label);
    italic.setItalic(true);
    const int text = std::max(QFontMetrics(edit_->font()).horizontalAdvance(QString(digits, u'0')),
                              QFontMetrics(italic).horizontalAdvance(tr("*Khác nhau*")));
    const int parts = locked_ ? kPickW : (pick_->isHidden() ? 0 : kPickW) + kStepW;
    return 2 * (kBorder + kPad) + text + parts;
}

bool NumericField::isEdited() const
{
    return edit_->text() != shown_ || hasError();
}

// Enter, rời ô. false: lỗi.
bool NumericField::commit()
{
    const QString text = edit_->text();
    if (text == shown_) { // chưa sửa (*Khác nhau*: chưa gõ): không đọc lại, không phát
        clearError();
        return true;
    }
    double v = 0;
    if (const QString why = check(text, v); !why.isEmpty()) {
        showError(why);
        return false;
    }
    accept(v);
    return true;
}

// Esc. false: không có gì để trả lại (Esc thuộc về hộp thoại).
bool NumericField::revert()
{
    if (!isEdited())
        return false;
    edit_->setText(shown_);
    clearError();
    return true;
}

void NumericField::stepBy(int direction, bool fast)
{
    if (locked_)
        return;
    double base = value_, typed = 0;
    if (isEdited() && check(edit_->text(), typed).isEmpty())
        base = typed;
    else if (mixed_)
        return; // chưa có giá trị chung để bước
    offer(base + direction * stepSize() * (fast ? 10 : 1));
}

// Giá trị từ ↑ ↓ hoặc ⌖: kiểm như chữ gõ (chữ hiển thị của nó), đúng thì nhận đúng giá trị đó.
void NumericField::offer(double value)
{
    const QString text = format(value);
    double shown = 0;
    if (const QString why = check(text, shown); !why.isEmpty()) {
        edit_->setText(text);
        showError(why);
        return;
    }
    accept(value);
}

void NumericField::accept(double value)
{
    mixed_ = false;
    value_ = value;
    display();
    emit valueCommitted(value);
}

// Chữ của giá trị đã nhận vào ô; bỏ lỗi.
void NumericField::display()
{
    shown_ = mixed_ ? QString() : format(value_);
    edit_->setText(shown_);
    clearError();
    updateAccessible();
    edit_->update(); // *Khác nhau* vẽ trong paintEvent của ô chữ, chữ có thể không đổi ("" → "")
}

void NumericField::showError(const QString& why)
{
    error_ = why;
    msg_->setText(why);
    msg_->show();
    box_->update();
    updateAccessible();
    QAccessibleAnnouncementEvent said(edit_, why); // như aria-live="polite" của .fmsg
    QAccessible::updateAccessibility(&said);
}

void NumericField::clearError()
{
    if (error_.isEmpty())
        return;
    error_.clear();
    msg_->hide();
    msg_->clear();
    box_->update();
    updateAccessible();
}

void NumericField::startPick()
{
    if (picking_ || locked_ || !isSignalConnected(QMetaMethod::fromSignal(&NumericField::pickRequested)))
        return;
    picking_ = true;
    if (auto* dialog = qobject_cast<DialogFrame*>(window()))
        dialog->hideForPick(true);
    emit pickRequested();
}

bool NumericField::endPick()
{
    if (!picking_)
        return false;
    picking_ = false;
    if (auto* dialog = qobject_cast<DialogFrame*>(window()))
        dialog->hideForPick(false);
    edit_->setFocus(Qt::OtherFocusReason);
    return true;
}

void NumericField::updateParts()
{
    const bool pickable = spec_.kind == ValueKind::Distance || spec_.kind == ValueKind::Angle;
    pick_->setHidden(locked_ || !pickEnabled_ || !pickable);
    up_->setHidden(locked_);
    down_->setHidden(locked_);
    placeParts();
    box_->updateGeometry();
    box_->update();
}

// Từ phải sang: [nút bước 16][⌖ 22] mỗi phần có nét chia 1 px ở mép trái (hộp vẽ); ô chữ lấy phần còn lại. Phần sát
// mép hộp lùi kRing để không che viền focus/lỗi.
void NumericField::placeParts()
{
    const int w = box_->width(), h = box_->height() - 2 * kRing, inner = w - kRing;
    int x = w - kBorder;
    if (!up_->isHidden()) {
        x -= kStepW;
        up_->setGeometry(x + kBorder, kRing, inner - x - kBorder, h / 2);
        down_->setGeometry(x + kBorder, kRing + h / 2, inner - x - kBorder, h - h / 2);
    }
    if (!pick_->isHidden() || locked_) {
        const int end = std::min(x, inner);
        x -= kPickW;
        pick_->setGeometry(x + kBorder, kRing, end - x - kBorder, h);
    }
    edit_->setGeometry(kRing, kRing, x - kRing, h);
}

void NumericField::retranslate()
{
    pick_->setToolTip(tr("Lấy từ bản vẽ"));
    pick_->setAccessibleName(tr("Lấy từ bản vẽ"));
    up_->setAccessibleName(tr("Tăng"));
    down_->setAccessibleName(tr("Giảm"));
    if (double v = 0; hasError()) { // câu lỗi theo ngôn ngữ mới
        error_ = check(edit_->text(), v);
        msg_->setText(error_);
        msg_->setVisible(hasError());
    }
    updateAccessible();
    box_->updateGeometry(); // bề rộng của *Khác nhau*
    edit_->update();
}

void NumericField::updateAccessible()
{
    const QString unit = unitName();
    edit_->setAccessibleName(label_.isEmpty() ? unit : unit.isEmpty() ? label_ : tr("%1 (%2)").arg(label_, unit));
    edit_->setAccessibleDescription(hasError() ? error_ : mixed_ ? tr("*Khác nhau*") : QString());
}

} // namespace trion::ui
