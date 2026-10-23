#include "app/vn/fontdoctor/FontStatusChip.h"

#include "app/core/i18n/UiFormat.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Paint.h"
#include "app/widgets/tooltip/RichTooltip.h"

#include <QKeyEvent>
#include <QPainter>

#include <algorithm>
#include <climits>

using namespace Qt::StringLiterals;

namespace trion::fontdoctor {

namespace {

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

QFont chipFont(bool scanning) // .sb-chip --fs-s; .sb-task --fs-xs
{
    return theme::font(scanning ? theme::TextRole::Caption : theme::TextRole::Label);
}

int barWidth() // .sb-task .prog 80 px, như InlineProgress
{
    return 5 * px(tok::sp_6);
}

} // namespace

FontStatusChip::FontStatusChip(QWidget* parent) : QToolButton(parent)
{
    setObjectName(u"fontDoctor"_s); // id mục của StatusBar
    setFocusPolicy(Qt::TabFocus); // như các mục .sbt: cú nhấp không lấy focus khỏi vùng vẽ, dòng lệnh
    setAttribute(Qt::WA_Hover);
    retranslate();
    hide();
}

void FontStatusChip::setIssues(int missingFonts, qint64 tcvn3, qint64 vni)
{
    missingFonts = std::max(0, missingFonts);
    tcvn3 = std::max<qint64>(0, tcvn3);
    vni = std::max<qint64>(0, vni);
    if (missingFonts == 0 && tcvn3 + vni == 0) {
        clear();
        return;
    }
    if (state_ == State::Issues && missing_ == missingFonts && tcvn3_ == tcvn3 && vni_ == vni && !isHidden())
        return;
    state_ = State::Issues;
    missing_ = missingFonts;
    tcvn3_ = tcvn3;
    vni_ = vni;
    progress_ = -1;
    retranslate();
    show();
}

void FontStatusChip::setScanning(double progress)
{
    progress = progress < 0 ? -1 : std::min(progress, 1.0);
    if (state_ == State::Scanning && progress == progress_ && !isHidden())
        return;
    state_ = State::Scanning;
    progress_ = progress;
    retranslate();
    show();
}

void FontStatusChip::clear()
{
    state_ = State::Empty;
    missing_ = 0;
    tcvn3_ = vni_ = 0;
    progress_ = -1;
    hide();
}

QSize FontStatusChip::sizeHint() const
{
    const bool scanning = state_ == State::Scanning;
    int w = QFontMetrics(chipFont(scanning)).horizontalAdvance(text()) + 2 * pad();
    if (state_ == State::Issues)
        w += px(tok::ic) + px(tok::sp_2); // biểu tượng warn, gap
    if (scanning && progress_ >= 0)
        w += px(tok::sp_3) + barWidth(); // .sb-task gap 6px, thanh
    return {w, px(tok::h_btn)};
}

int FontStatusChip::pad() const
{
    return px(state_ == State::Scanning ? tok::sp_3 : tok::sp_4); // .sb-task padding 0 6px; .sb-chip 0 8px
}

void FontStatusChip::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal r = tm().px(tok::r);
    const QRectF face = rect();
    const bool hot = isEnabled() && (underMouse() || isDown());
    const bool scanning = state_ == State::Scanning;
    QColor ink = tm().color(scanning ? tok::c_text_2 : tok::c_warn);
    p.setPen(Qt::NoPen);
    if (!scanning) { // .sb-chip.warn; rê: viền trong 1 px
        p.setBrush(tm().color(tok::c_warn_soft));
        p.drawRoundedRect(face, r, r);
        if (hot)
            ui::paint::drawBox(p, face.adjusted(0.5, 0.5, -0.5, -0.5), QColor(), tm().color(tok::c_border_ctl));
    } else if (hot) { // như .sbt: rê --c-hover, nhấn --c-press
        p.setBrush(tm().color(isDown() ? tok::c_press : tok::c_hover));
        p.drawRoundedRect(face, r, r);
        ink = tm().color(tok::c_text);
    }

    QRectF area = face.adjusted(pad(), 0, -pad(), 0);
    if (!scanning) {
        const int ic = px(tok::ic);
        const QPixmap icon =
            theme::IconProvider::instance().pixmap(u"warn"_s, ic, devicePixelRatioF(), theme::IconState::Normal);
        p.drawPixmap(QPointF(area.left(), (height() - ic) / 2.0), ui::paint::tinted(icon, ink));
        area.setLeft(area.left() + ic + px(tok::sp_2));
    }
    const QFont f = chipFont(scanning);
    const int textW = QFontMetrics(f).horizontalAdvance(text());
    p.setFont(f);
    p.setPen(ink);
    p.drawText(QRectF(area.left(), 0, std::max<qreal>(textW, scanning ? 0 : area.width()), height()),
               Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, text());
    if (scanning && progress_ >= 0) { // .prog: rãnh --c-active, phần xong --c-accent, bo tròn hai đầu
        const qreal thick = tm().px(tok::sp_2);
        const QRectF track(area.left() + textW + px(tok::sp_3), (height() - thick) / 2, barWidth(), thick);
        p.setPen(Qt::NoPen);
        p.setBrush(tm().color(tok::c_active));
        p.drawRoundedRect(track, thick / 2, thick / 2);
        if (progress_ > 0) {
            QRectF done = track;
            done.setWidth(std::max(thick, track.width() * progress_));
            p.setBrush(tm().color(tok::c_accent));
            p.drawRoundedRect(done, thick / 2, thick / 2);
        }
    }
    if (ui::paint::focusVisible(this))
        ui::paint::drawRing(p, face, tm().color(tok::c_focus));
}

void FontStatusChip::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        click();
        return;
    }
    QToolButton::keyPressEvent(e);
}

void FontStatusChip::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QToolButton::changeEvent(e);
}

void FontStatusChip::retranslate()
{
    using i18n::formatCount;
    QString about;
    switch (state_) {
    case State::Issues: {
        const qint64 legacy = tcvn3_ + vni_;
        setText(missing_ > 0 ? tr("Font: %1 thay thế", nullptr, missing_).arg(formatCount(missing_))
                             : tr("Font: %1 chữ mã cũ", nullptr, plural(legacy)).arg(formatCount(legacy)));
        QStringList found;
        if (missing_ > 0)
            found << tr("%1 font đã thay thế", nullptr, missing_).arg(formatCount(missing_));
        if (legacy > 0)
            found << tr("%1 đối tượng chữ mã cũ", nullptr, plural(legacy)).arg(formatCount(legacy));
        const QString facts = found.join(u" · "_s);
        setAccessibleDescription(facts);
        about = tr("%1. Bấm để mở Bác sĩ Font.").arg(facts);
        break;
    }
    case State::Scanning: {
        setText(tr("Đang quét font"));
        // Trình đọc màn hình đọc theo bước 10% như thanh tiến trình (InlineProgress), không theo từng phần trăm.
        const int step = progress_ < 0 ? -1 : int(progress_ * 10) * 10;
        setAccessibleDescription(step < 0 ? QString() : tr("Đã quét %1%").arg(formatCount(step)));
        about = tr("Đang quét font và bảng mã chữ của bản vẽ. Bấm để mở Bác sĩ Font.");
        break;
    }
    case State::Empty:
        break;
    }
    ui::RichTooltip::attach(this, {.name = tr("Bác sĩ Font"), .command = u"FIXFONT"_s, .description = about});
    updateGeometry(); // thanh tiến trình hiện/ẩn đổi bề rộng; StatusBar bố cục lại
    update();
}

} // namespace trion::fontdoctor
