#include "app/vn/fontdoctor/FontDoctorBanner.h"

#include "app/core/a11y/Announce.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/edition/LockBadge.h"
#include "app/core/i18n/UiFormat.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/message/Messages.h"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>

#include <algorithm>
#include <array>
#include <climits>

using namespace Qt::StringLiterals;

namespace trion::fontdoctor {

namespace {

constexpr int kBorder = 1; // .banner border-bottom 1px

theme::ThemeManager& tm()
{
    return theme::ThemeManager::instance();
}

int px(tok::PxId id)
{
    return qRound(tm().px(id));
}

int plural(qint64 n) // tr(…, n) chỉ dùng n để chọn dạng số nhiều
{
    return int(std::min<qint64>(n, INT_MAX));
}

// Vai trò StatusBar (role="status" của prototype): QAccessibleWidget mặc định chỉ có Client.
QAccessibleInterface* accessibleBanner(const QString& className, QObject* object)
{
    return className == QLatin1StringView(FontDoctorBanner::staticMetaObject.className())
               ? new QAccessibleWidget(static_cast<QWidget*>(object), QAccessible::StatusBar)
               : nullptr;
}

} // namespace

FontDoctorBanner::FontDoctorBanner(QWidget* canvas, edition::EditionGate& gate)
    : QWidget(canvas), gate_(gate), glyph_(new ui::Glyph(px(tok::ic), this)), title_(new QLabel(this)),
      text_(new QLabel(this)), details_(new ui::Button({}, ui::Button::Kind::Normal, this)),
      convert_(new ui::Button({}, ui::Button::Kind::Primary, this)),
      badge_(new edition::LockBadge(ui::Edition::ViewerPro, edition::LockBadge::Style::Tag, this)),
      mute_(new ui::Button({}, ui::Button::Kind::Ghost, this)), close_(new ui::Button({}, ui::Button::Kind::Icon, this))
{
    static const bool factory = (QAccessible::installFactory(accessibleBanner), true);
    Q_UNUSED(factory);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_OpaquePaintEvent); // paintEvent phủ kín: khỏi vẽ vùng vẽ bên dưới mỗi lần banner vẽ lại
    glyph_->set(u"fontdoc"_s, tok::c_accent);
    for (QLabel* l : {title_, text_}) {
        l->setTextFormat(Qt::PlainText);
        l->setTextInteractionFlags(Qt::NoTextInteraction);
    }
    text_->setWordWrap(true);
    for (ui::Button* b : {details_, convert_, mute_})
        b->setControlSize(ui::Button::Size::Small);
    close_->setIconName(u"x"_s);
    for (ui::Button* b : {details_, convert_, mute_, close_})
        b->setFocusPolicy(Qt::TabFocus); // cú nhấp không lấy focus khỏi vùng vẽ, dòng lệnh

    connect(details_, &QPushButton::clicked, this, &FontDoctorBanner::detailsRequested);
    connect(convert_, &QPushButton::clicked, this, [this] {
        if (gate_.locked(ui::Edition::ViewerPro))
            gate_.upgrade().request({.feature = u"fontconvert"_s,
                                     .need = ui::Edition::ViewerPro,
                                     .name = tr("Chuyển chữ mã cũ sang Unicode")});
        else
            emit convertRequested();
    });
    connect(mute_, &QPushButton::clicked, this, &FontDoctorBanner::muteRequested);
    connect(close_, &QPushButton::clicked, this, &FontDoctorBanner::closeRequested);
    connect(&gate, &edition::EditionGate::editionChanged, this, [this] {
        syncLock();
        place();
    });
    connect(&tm(), &theme::ThemeManager::changed, this, [this] {
        applyTheme();
        place();
    });
    // Tab vào banner: nhớ chỗ có focus trước đó (dòng lệnh, vùng vẽ) để trả lại khi banner ẩn.
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget* old, QWidget* now) {
        if (isAncestorOf(now) && !isAncestorOf(old))
            back_ = old;
    });
    canvas->installEventFilter(this);
    applyTheme();
    retranslate();
    hide();
}

QString FontDoctorBanner::compose(int missingFonts, qint64 tcvn3, qint64 vni)
{
    using i18n::formatCount;
    // Mỗi số một chuỗi riêng để chọn đúng dạng số nhiều; có cả hai bảng mã: "250 đối tượng chữ mã TCVN3, 46 chữ VNI".
    QStringList parts;
    if (tcvn3 > 0)
        parts << tr("%1 đối tượng chữ mã TCVN3", nullptr, plural(tcvn3)).arg(formatCount(tcvn3));
    if (vni > 0)
        parts << (tcvn3 > 0 ? tr("%1 chữ VNI", nullptr, plural(vni)).arg(formatCount(vni))
                            : tr("%1 đối tượng chữ mã VNI", nullptr, plural(vni)).arg(formatCount(vni)));
    const QString legacy = parts.join(u", "_s);
    if (missingFonts <= 0)
        return legacy.isEmpty() ? QString() : tr("Bản vẽ có %1. TrionCAD đã tự hiển thị đúng.").arg(legacy);
    const QString fonts = tr("%1 font không có trên máy", nullptr, missingFonts).arg(formatCount(missingFonts));
    return legacy.isEmpty() ? tr("Bản vẽ dùng %1. TrionCAD đã tự hiển thị đúng.").arg(fonts)
                            : tr("Bản vẽ dùng %1 và %2. TrionCAD đã tự hiển thị đúng.").arg(fonts, legacy);
}

void FontDoctorBanner::showScan(int missingFonts, qint64 tcvn3, qint64 vni, bool canMute)
{
    if (!isHidden() && missingFonts == missing_ && tcvn3 == tcvn3_ && vni == vni_ && canMute == !mute_->isHidden())
        return;
    missing_ = missingFonts;
    tcvn3_ = tcvn3;
    vni_ = vni;
    mute_->setVisible(canMute); // bản vẽ chưa lưu: không có tệp để nhớ
    retranslate();              // câu, tên tiếp cận, đặt chỗ
    show();
    raise();
    a11y::announce(this, accessibleName());
}

void FontDoctorBanner::stackBelow(QWidget* other)
{
    if (above_)
        above_->removeEventFilter(this);
    above_ = other;
    if (other)
        other->installEventFilter(this);
    place();
}

void FontDoctorBanner::setVisible(bool visible)
{
    // Ẩn khi đang giữ focus: Qt sẽ chuyển focus sang widget kế tiếp bất kỳ; trả về chỗ cũ trước.
    if (!visible && back_ && isAncestorOf(QApplication::focusWidget()))
        back_->setFocus(Qt::OtherFocusReason);
    QWidget::setVisible(visible);
}

bool FontDoctorBanner::event(QEvent* e)
{
    switch (e->type()) {
    case QEvent::LayoutRequest: // nút đổi chữ, huy hiệu hiện/ẩn
        place();
        break;
    case QEvent::ShortcutOverride: { // Esc trong banner là Đóng, không phải phím tắt hủy lệnh của cửa sổ
        const auto* k = static_cast<QKeyEvent*>(e);
        if (k->key() == Qt::Key_Escape && k->modifiers() == Qt::NoModifier) {
            e->accept();
            return true;
        }
        break;
    }
    default:
        break;
    }
    return QWidget::event(e);
}

bool FontDoctorBanner::eventFilter(QObject* watched, QEvent* e)
{
    const QEvent::Type t = e->type();
    if ((watched == parent() && t == QEvent::Resize) ||
        (watched == above_.data() &&
         (t == QEvent::Resize || t == QEvent::Move || t == QEvent::Show || t == QEvent::Hide)))
        place();
    return false;
}

void FontDoctorBanner::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void FontDoctorBanner::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape && e->modifiers() == Qt::NoModifier) {
        emit closeRequested();
        return;
    }
    QWidget::keyPressEvent(e);
}

void FontDoctorBanner::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), tm().color(tok::c_banner));
    p.fillRect(QRect(0, height() - kBorder, width(), kBorder), tm().color(tok::c_border));
}

int FontDoctorBanner::arrange(int width)
{
    // .banner: padding 4px 6px 4px 12px, gap 10px giữa biểu tượng, câu, nhóm nút, ×; .acts gap 6px; viền đáy 1 px.
    const int left = px(tok::sp_5), right = px(tok::sp_3), pad = px(tok::sp_2);
    const int gap = px(tok::sp_4) + px(tok::sp_1), between = px(tok::sp_3), tagGap = px(tok::sp_2), ic = px(tok::ic);
    const QSize closeSize = close_->sizeHint();

    // Nhóm nút: mỗi đơn vị là nút (và huy hiệu đi liền sau nó), không tách khi xuống hàng.
    struct Unit {
        QWidget* main = nullptr;
        QWidget* tag = nullptr;
        QSize size;
    };
    std::array<Unit, 3> units;
    int count = 0, actions = 0;
    const auto add = [&](QWidget* main, QWidget* tag) {
        if (main->isHidden())
            return;
        QSize s = main->sizeHint();
        if (tag && !tag->isHidden()) {
            const QSize t = tag->sizeHint();
            s = {s.width() + tagGap + t.width(), std::max(s.height(), t.height())};
        } else {
            tag = nullptr;
        }
        actions += (count ? between : 0) + s.width();
        units[count++] = {main, tag, s};
    };
    add(details_, nullptr);
    add(convert_, badge_);
    add(mute_, nullptr);

    // Khối chữ: tiêu đề đậm rồi câu (thụt theo tiêu đề). Một hàng khi câu còn chỗ cho ≤ 2 dòng, không thì nút xuống
    // hàng dưới.
    const QSize titleSize = title_->sizeHint();
    const QFontMetrics fm(text_->font());
    const int lead = titleSize.width() + fm.horizontalAdvance(u' ');
    const int oneLine = lead + fm.horizontalAdvance(message_);
    const int inner = width - left - right;
    const int besideActions = inner - ic - actions - closeSize.width() - 3 * gap;
    const bool oneRow = besideActions > lead && 2 * besideActions >= oneLine;
    const int textX = left + ic + gap;
    const int blockW = oneRow ? besideActions : inner - ic - closeSize.width() - 2 * gap;
    const int textW = std::max(1, blockW - lead);
    const int textH = text_->heightForWidth(textW);
    const int blockH = std::max(textH, titleSize.height());

    int rowH = std::max({blockH, ic, closeSize.height()});
    if (oneRow) {
        for (int i = 0; i < count; ++i)
            rowH = std::max(rowH, units[i].size.height());
        rowH = std::max(rowH, kMinHeight - 2 * pad - kBorder); // min-height: nội dung căn giữa dải 38 px
    }
    glyph_->setGeometry(left, pad + (rowH - ic) / 2, ic, ic);
    const int blockY = pad + (rowH - blockH) / 2; // tiêu đề thẳng hàng dòng đầu của câu
    title_->setGeometry(textX, blockY, titleSize.width(), titleSize.height());
    text_->setGeometry(textX + lead, blockY, textW, textH);
    close_->setGeometry(width - right - closeSize.width(), pad + (rowH - closeSize.height()) / 2, closeSize.width(),
                        closeSize.height());

    int x = oneRow ? textX + blockW + gap : textX;
    int y = oneRow ? pad : pad + rowH + between;
    int lineH = oneRow ? rowH : 0;
    for (int i = 0; i < count; ++i) {
        const Unit& u = units[i];
        if (!oneRow && x > textX && x + u.size.width() > width - right) { // hết chỗ: hàng nút kế tiếp
            y += lineH + between;
            x = textX;
            lineH = 0;
        }
        if (!oneRow)
            lineH = std::max(lineH, u.size.height());
        const int top = oneRow ? pad + (rowH - u.size.height()) / 2 : y;
        const QSize m = u.main->sizeHint();
        u.main->setGeometry(x, top + (u.size.height() - m.height()) / 2, m.width(), m.height());
        if (u.tag) {
            const QSize t = u.tag->sizeHint();
            u.tag->setGeometry(x + m.width() + tagGap, top + (u.size.height() - t.height()) / 2, t.width(), t.height());
        }
        x += u.size.width() + between;
    }
    const int bottom = oneRow ? pad + rowH : y + lineH;
    return std::max(kMinHeight, bottom + pad + kBorder);
}

void FontDoctorBanner::place()
{
    QWidget* canvas = parentWidget();
    if (!canvas)
        return;
    const int y = above_ && above_->isVisibleTo(canvas) ? above_->geometry().bottom() + 1 : 0;
    const int w = canvas->width();
    setGeometry(0, y, w, arrange(w)); // chỗ của các con chỉ theo bề rộng: đặt và đo cùng một lượt
}

void FontDoctorBanner::retranslate()
{
    details_->setText(tr("Xem chi tiết"));
    convert_->setText(tr("Chuyển sang Unicode"));
    mute_->setText(tr("Không nhắc lại với tệp này"));
    close_->setAccessibleName(tr("Đóng banner"));
    title_->setText(tr("Bác sĩ Font:"));
    syncLock();
    if (missing_ >= 0) {
        message_ = compose(missing_, tcvn3_, vni_);
        text_->setText(message_);
        setAccessibleName(tr("Bác sĩ Font: %1").arg(message_));
    }
    place();
}

void FontDoctorBanner::syncLock()
{
    const bool locked = gate_.locked(ui::Edition::ViewerPro);
    convert_->setLocked(locked);
    convert_->setAccessibleDescription(locked ? edition::EditionGate::availableIn(ui::Edition::ViewerPro) : QString());
}

void FontDoctorBanner::applyTheme()
{
    title_->setFont(theme::font(theme::TextRole::Label, theme::Weight::Bold));
    text_->setFont(theme::font(theme::TextRole::Label));
    for (QLabel* l : {title_, text_}) {
        QPalette p = l->palette();
        p.setColor(QPalette::WindowText, tm().color(tok::c_text));
        l->setPalette(p);
    }
}

} // namespace trion::fontdoctor
