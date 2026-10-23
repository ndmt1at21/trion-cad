#include "app/widgets/dialog/DialogFrame.h"

#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/progress/Spinner.h"

#include <QAbstractButton>
#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QtMath>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace Qt::StringLiterals;

namespace trion::ui {

namespace {

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

// Giá trị của prototype không có token riêng (css/app.css .dlg*).
constexpr int kBorder = 1;    // viền thẻ, nét chia đầu/chân
constexpr int kHeaderH = 34;  // .dlg-h height (gồm nét chia dưới)
constexpr int kBodyPad = 14;  // .dlg-b padding
constexpr int kFootPadV = 10; // .dlg-f padding: 10px 14px
constexpr int kFootPadH = 14;

// Làm mờ Gauss độ lệch sigma (px thiết bị) cho kênh alpha w×h, ngoài ảnh là 0: 3 lượt box blur mỗi chiều, bề rộng
// hộp theo "boxes for Gauss" (Kovesi).
void blur(std::vector<float>& a, int w, int h, double sigma)
{
    const int n = 3;
    int lo = int(std::sqrt(12 * sigma * sigma / n + 1));
    if (lo % 2 == 0)
        --lo;
    const int m = int(std::lround((12 * sigma * sigma - n * lo * lo - 4 * n * lo - 3 * n) / (-4.0 * lo - 4)));
    std::vector<float> tmp(a.size());
    // Một lượt trên `lines` đường dài `len`, phần tử cách nhau `step`, đường cách nhau `lineStep`.
    const auto pass = [](const float* src, float* dst, int len, int step, int lines, int lineStep, int r) {
        const float inv = 1.0f / float(2 * r + 1);
        for (int l = 0; l < lines; ++l) {
            const float* s = src + std::ptrdiff_t(l) * lineStep;
            float* d = dst + std::ptrdiff_t(l) * lineStep;
            float sum = 0;
            for (int k = 0; k < std::min(r, len); ++k)
                sum += s[k * step];
            for (int x = 0; x < len; ++x) {
                if (x + r < len)
                    sum += s[(x + r) * step];
                d[x * step] = sum * inv;
                if (x - r >= 0)
                    sum -= s[(x - r) * step];
            }
        }
    };
    for (int i = 0; i < n; ++i) {
        const int r = ((i < m ? lo : lo + 2) - 1) / 2;
        pass(a.data(), tmp.data(), w, 1, h, w, r); // ngang
        pass(tmp.data(), a.data(), h, w, w, 1, r); // dọc
    }
}

// Bóng box-shadow của thẻ dạng 9 mảnh: tile vuông dựng một lần cho mỗi (bóng, bo góc, DPR) rồi nằm trong QPixmapCache.
// Hình (bo góc r) cách mép tile e = độ loang; góc c = 2e + r px thiết bị (e ngoài, e trong đã bão hòa, bo góc) vẽ 1:1,
// cạnh và phần giữa kéo giãn hàng/cột thứ c.
QPixmap shadowTile(const tok::Shadow& s, double radius, qreal dpr, int* corner)
{
    const int e = qCeil(s.blur * dpr);
    const int r = qCeil((radius + s.spread) * dpr);
    const int c = 2 * e + r;
    const int t = 2 * c + 1;
    *corner = c;
    const QString key = u"trion/dlg-shadow/%1/%2/%3/%4/%5"_s.arg(s.blur).arg(s.spread).arg(radius).arg(dpr).arg(
        qRgba(s.color.r, s.color.g, s.color.b, s.color.a));
    QPixmap tile;
    if (QPixmapCache::find(key, &tile))
        return tile;
    QImage shape(t, t, QImage::Format_Alpha8);
    shape.fill(0);
    {
        QPainter p(&shape);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::black); // style-ok: mặt nạ, chỉ dùng kênh alpha
        p.drawRoundedRect(QRectF(e, e, t - 2 * e, t - 2 * e), r, r);
    }
    std::vector<float> a(std::size_t(t) * t);
    for (int y = 0; y < t; ++y)
        for (int x = 0; x < t; ++x)
            a[std::size_t(y) * t + x] = shape.constScanLine(y)[x] / 255.0f;
    blur(a, t, t, s.blur * dpr / 2); // CSS: độ lệch Gauss = blur / 2
    QImage img(t, t, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < t; ++y) {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < t; ++x) {
            const float k = std::clamp(a[std::size_t(y) * t + x], 0.0f, 1.0f) * float(s.color.a) / 255.0f;
            line[x] = qRgba(qRound(s.color.r * k), qRound(s.color.g * k), qRound(s.color.b * k), qRound(255 * k));
        }
    }
    tile = QPixmap::fromImage(img);
    QPixmapCache::insert(key, tile);
    return tile;
}

// --sh-dlg quanh thẻ `card` (px logic). Giao diện nhẹ: token không mờ, spread 1 → viền 1 px.
void drawShadow(QPainter& p, const QRectF& card, double radius)
{
    const tok::Shadow s = tm().shadow(tok::sh_dlg);
    const QColor color(s.color.r, s.color.g, s.color.b, s.color.a);
    const QRectF shape = card.translated(s.x, s.y).adjusted(-s.spread, -s.spread, s.spread, s.spread);
    if (s.blur <= 0) {
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(shape, radius + s.spread, radius + s.spread);
        return;
    }
    const qreal dpr = p.device()->devicePixelRatio();
    int c = 0;
    const QPixmap tile = shadowTile(s, radius, dpr, &c);
    const double e = qCeil(s.blur * dpr) / dpr;
    const double cl = c / dpr;
    const QRectF out = shape.adjusted(-e, -e, e, e);
    const double xs[] = {out.left(), out.left() + cl, out.right() - cl, out.right()};
    const double ys[] = {out.top(), out.top() + cl, out.bottom() - cl, out.bottom()};
    const double ss[] = {0, double(c), double(c + 1), double(2 * c + 1)};
    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            const QRectF target(QPointF(xs[i], ys[j]), QPointF(xs[i + 1], ys[j + 1]));
            if (target.width() > 0 && target.height() > 0) // ponytail: thẻ nhỏ hơn 2 góc thì góc chồng nhau
                p.drawPixmap(target, tile, QRectF(QPointF(ss[i], ss[j]), QPointF(ss[i + 1], ss[j + 1])));
        }
    p.restore();
}

} // namespace

DialogFrame::DialogFrame(const QString& id, QWidget* parent) : QDialog(parent), id_(id)
{
    TRION_CHECK(!id.isEmpty() && !id.contains(u'/'));
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground); // bóng và góc bo vẽ trên nền trong suốt
    setMouseTracking(true);                     // con trỏ đổi cỡ ở mép

    header_ = new QWidget(this);
    header_->setFixedHeight(kHeaderH);
    header_->setCursor(Qt::SizeAllCursor); // .dlg-h cursor: move
    header_->installEventFilter(this);
    title_ = new QLabel(header_);
    title_->setTextFormat(Qt::PlainText);
    title_->setFont(theme::font(theme::TextRole::Ui, theme::Weight::Bold));
    commandLabel_ = new QLabel(header_);
    commandLabel_->setTextFormat(Qt::PlainText);
    commandLabel_->setFont(theme::font(theme::TextRole::MonoCaption));
    commandLabel_->setForegroundRole(QPalette::PlaceholderText); // --c-text-3
    commandLabel_->hide();
    help_ = new Button({}, Button::Kind::Icon, header_);
    help_->setIconName(u"help"_s);
    help_->setCursor(Qt::ArrowCursor);
    help_->hide();
    close_ = new Button({}, Button::Kind::Icon, header_);
    close_->setIconName(u"x"_s);
    close_->setCursor(Qt::ArrowCursor);
    connect(help_, &QPushButton::clicked, this, &DialogFrame::requestHelp);
    connect(close_, &QPushButton::clicked, this, &QDialog::reject);
    auto* titles = new QHBoxLayout;
    titles->setSpacing(px(tok::sp_2));
    titles->addWidget(title_);
    titles->addWidget(commandLabel_);
    auto* head = new QHBoxLayout(header_);
    head->setContentsMargins(px(tok::sp_5), 0, px(tok::sp_2), kBorder); // .dlg-h padding: 0 4px 0 12px; nét chia dưới
    head->setSpacing(px(tok::sp_4));
    head->addLayout(titles);
    head->addStretch(1);
    head->addWidget(help_);
    head->addWidget(close_);

    bodyBox_ = new QVBoxLayout;
    bodyBox_->setContentsMargins(kBodyPad, kBodyPad, kBodyPad, kBodyPad);
    auto* card = new QVBoxLayout(this);
    card->setContentsMargins(0, 0, 0, 0);
    card->setSpacing(0);
    card->addWidget(header_);
    card->addLayout(bodyBox_, 1);
    applyMargins();
    connect(&tm(), &theme::ThemeManager::changed, this, &DialogFrame::applyMargins);
    retranslate();
}

DialogFrame::~DialogFrame()
{
    load_.cancel();
}

void DialogFrame::setCommand(const QString& command)
{
    command_ = command;
    commandLabel_->setText(command);
    commandLabel_->setVisible(!command.isEmpty());
    help_->setVisible(!command.isEmpty());
}

void DialogFrame::setBody(QWidget* body, bool flush)
{
    const bool wasLoading = isLoading();
    if (body_ != body)
        delete body_; // kể cả "Đang tải…"
    loading_ = nullptr;
    body_ = body;
    const int pad = flush ? 0 : kBodyPad;
    bodyBox_->setContentsMargins(pad, pad, pad, pad);
    if (body) {
        bodyBox_->addWidget(body);
        // Tab vòng trong hộp thoại: ô nhiều dòng chuyển focus thay vì chèn ký tự tab (như textarea).
        QList<QWidget*> all = body->findChildren<QWidget*>();
        all << body;
        for (QWidget* w : std::as_const(all))
            if (auto* plain = qobject_cast<QPlainTextEdit*>(w))
                plain->setTabChangesFocus(true);
            else if (auto* rich = qobject_cast<QTextEdit*>(w))
                rich->setTabChangesFocus(true);
    }
    if (wasLoading && confirm_)
        confirm_->setEnabled(true);
}

void DialogFrame::setLoading()
{
    auto* box = new QWidget;
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(px(tok::sp_6), px(tok::sp_7), px(tok::sp_6), px(tok::sp_7)); // .empty padding: 24px 16px
    v->setSpacing(px(tok::sp_4));
    v->addWidget(new Spinner(box), 0, Qt::AlignHCenter);
    auto* text = new QLabel(box);
    text->setFont(theme::font(theme::TextRole::Ui, theme::Weight::Bold));
    v->addWidget(text, 0, Qt::AlignHCenter);
    setBody(box);
    loading_ = box;
    retranslate();
    if (confirm_)
        confirm_->setEnabled(false);
}

bool DialogFrame::isLoading() const
{
    return loading_ && body_ == loading_;
}

QHBoxLayout* DialogFrame::footer()
{
    if (!footer_) {
        footer_ = new QWidget(this);
        auto* h = new QHBoxLayout(footer_);
        h->setContentsMargins(kFootPadH, kFootPadV + kBorder, kFootPadH, kFootPadV); // nét chia trên
        h->setSpacing(px(tok::sp_4));
        h->addStretch(1);
        static_cast<QVBoxLayout*>(layout())->addWidget(footer_);
    }
    return static_cast<QHBoxLayout*>(footer_->layout());
}

Button* DialogFrame::addCancel()
{
    if (!cancel_) {
        cancel_ = new Button({}, Button::Kind::Ghost);
        connect(cancel_, &QPushButton::clicked, this, &QDialog::reject);
        QHBoxLayout* f = footer();
        f->insertWidget(confirm_ ? f->indexOf(confirm_) : f->count(), cancel_);
        retranslate();
    }
    return cancel_;
}

Button* DialogFrame::setConfirm(const QString& action, Button::Kind kind)
{
    if (!confirm_) {
        confirm_ = new Button(action, kind);
        footer()->addWidget(confirm_);
        confirm_->setDefault(true); // sau khi vào hộp thoại: QDialog nhận nó làm nút mặc định chính của Enter
        confirm_->setEnabled(!isLoading());
        connect(confirm_, &QPushButton::clicked, this, &QDialog::accept);
    } else {
        confirm_->setText(action);
        confirm_->setKind(kind);
    }
    return confirm_;
}

void DialogFrame::setResizable(bool on)
{
    resizable_ = on;
    applyMargins();
}

void DialogFrame::hideForPick(bool hidden)
{
    if (hidden == pickHidden_ || (hidden && !isVisible()))
        return;
    pickHidden_ = hidden;
    if (hidden) {
        QWidget* f = QApplication::focusWidget();
        pickFocus_ = f && isAncestorOf(f) ? f : nullptr;
        pickGeometry_ = geometry();
        hide();
        if (QWidget* p = parentWidget())
            p->activateWindow(); // phím, chuột về cửa sổ mở nó (vùng vẽ); hiện lại thì active đổi về đây
        return;
    }
    reshowing_ = true;
    // Hộp thoại chưa từng move/resize tường minh thì show() của QDialog co theo sizeHint và đặt lại giữa cửa sổ cha
    // (integration nhóm 4 bắt: lệch 16 px, cao thêm 15 px sau khi dòng lỗi của ô số hiện): đặt lại hình học lúc ẩn.
    setGeometry(pickGeometry_);
    show();
    raise();
    activateWindow();
    if (pickFocus_)
        pickFocus_->setFocus(Qt::OtherFocusReason);
}

QRect DialogFrame::cardRect() const
{
    return rect().marginsRemoved(shadow_);
}

QRect DialogFrame::cardGeometry() const
{
    return cardRect().translated(geometry().topLeft());
}

void DialogFrame::setCardGeometry(const QRect& card)
{
    setGeometry(card.marginsAdded(shadow_));
}

void DialogFrame::done(int result)
{
    load_.cancel();
    pickHidden_ = false;
    const QPointer<QWidget> prev = prevFocus_;
    QDialog::done(result);
    if (prev && prev->isVisible()) {
        prev->activateWindow();
        prev->setFocus(Qt::OtherFocusReason);
    }
}

bool DialogFrame::event(QEvent* e)
{
    if (e->type() == QEvent::ShortcutOverride) {
        const auto* k = static_cast<QKeyEvent*>(e);
        if (k->key() == Qt::Key_F1 && k->modifiers() == Qt::NoModifier) {
            e->accept(); // phím tới keyPressEvent, không thành phím tắt F1 toàn cục (HELP)
            return true;
        }
    }
    return QDialog::event(e);
}

void DialogFrame::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_F1 && e->modifiers() == Qt::NoModifier) {
        requestHelp();
        return;
    }
    QDialog::keyPressEvent(e); // Enter: nút mặc định (xác nhận) nếu bật; Esc: reject()
}

void DialogFrame::showEvent(QShowEvent* e)
{
    QDialog::showEvent(e);
    if (e->spontaneous() || std::exchange(reshowing_, false))
        return; // hiện lại sau khi thu nhỏ, sau khi lấy từ bản vẽ: giữ focus như cũ
    QWidget* prev = QApplication::focusWidget();
    prevFocus_ = prev && prev->window() != this ? prev : nullptr;
    if (focusWidget())
        return;
    // Focus đầu như prototype: ô nhập đầu tiên của thân (theo thứ tự Tab, không phải nút), không có thì nút xác nhận.
    QWidget* first = nullptr;
    for (QWidget* w = nextInFocusChain(); w != this && !first; w = w->nextInFocusChain())
        if (body_ && body_->isAncestorOf(w) && (w->focusPolicy() & Qt::TabFocus) && w->isEnabled() &&
            w->isVisibleTo(this) && !qobject_cast<QAbstractButton*>(w))
            first = w;
    if (!first && confirm_ && confirm_->isEnabled() && confirm_->isVisibleTo(this))
        first = confirm_;
    if (first)
        first->setFocus(Qt::OtherFocusReason);
}

void DialogFrame::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF card = cardRect();
    const double r = tm().px(tok::r_xl);
    drawShadow(p, card, r);
    p.setPen(Qt::NoPen);
    p.setBrush(col(tok::c_border_ctl)); // viền: thẻ tô màu viền, phần trong tô nền
    p.drawRoundedRect(card, r, r);
    QPainterPath inner;
    inner.addRoundedRect(card.adjusted(kBorder, kBorder, -kBorder, -kBorder), r - kBorder, r - kBorder);
    p.setBrush(col(tok::c_base));
    p.drawPath(inner);
    p.setClipPath(inner);
    const QRect head = header_->geometry();
    p.fillRect(head, col(tok::c_chrome));
    p.fillRect(QRect(head.left(), head.bottom(), head.width(), kBorder), col(tok::c_border));
    if (footer_) {
        const QRect foot = footer_->geometry();
        p.fillRect(QRect(foot.left(), foot.top(), foot.width(), kBorder), col(tok::c_border));
    }
}

void DialogFrame::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::WindowTitleChange)
        title_->setText(windowTitle());
    else if (e->type() == QEvent::LanguageChange)
        retranslate();
    QDialog::changeEvent(e);
}

void DialogFrame::retranslate()
{
    help_->setAccessibleName(tr("Trợ giúp (F1)"));
    help_->setToolTip(help_->accessibleName());
    close_->setAccessibleName(tr("Đóng (Esc)"));
    close_->setToolTip(close_->accessibleName());
    if (cancel_)
        cancel_->setText(tr("Hủy"));
    if (loading_)
        loading_->findChild<QLabel*>()->setText(tr("Đang tải…"));
}

void DialogFrame::applyMargins()
{
    const tok::Shadow s = tm().shadow(tok::sh_dlg);
    const int band = resizable_ ? px(tok::sp_3) : 0; // dải kéo đổi cỡ ngoài mép thẻ
    const auto out = [&](double shift) { return std::max(qCeil(s.blur + s.spread + shift), band); };
    const QMargins m(out(-s.x), out(-s.y), out(s.x), out(s.y));
    if (m == shadow_)
        return;
    const QRect card = cardGeometry();
    shadow_ = m;
    setContentsMargins(m + QMargins(kBorder, kBorder, kBorder, kBorder));
    if (isVisible() || pickHidden_)
        setCardGeometry(card); // thẻ đứng yên khi lề bóng đổi (bật/tắt Giao diện nhẹ)
}

Qt::Edges DialogFrame::edgesAt(QPoint pos) const
{
    const QRect card = cardRect();
    const int band = px(tok::sp_3);
    if (!resizable_ || card.contains(pos) || !card.adjusted(-band, -band, band, band).contains(pos))
        return {};
    Qt::Edges edges;
    if (pos.x() < card.left())
        edges |= Qt::LeftEdge;
    else if (pos.x() > card.right())
        edges |= Qt::RightEdge;
    if (pos.y() < card.top())
        edges |= Qt::TopEdge;
    else if (pos.y() > card.bottom())
        edges |= Qt::BottomEdge;
    return edges;
}

// Kéo đầu (di chuyển) hoặc mép (đổi cỡ): để hệ điều hành kéo nếu được (Wayland chỉ có cách này), không thì tự dời.
void DialogFrame::beginDrag(Qt::Edges edges, QPoint global)
{
    QWindow* w = windowHandle();
    if (w && (edges ? w->startSystemResize(edges) : w->startSystemMove()))
        return;
    drag_ = {true, edges, global, geometry()};
}

void DialogFrame::mousePressEvent(QMouseEvent* e)
{
    const Qt::Edges edges = edgesAt(e->position().toPoint());
    if (e->button() == Qt::LeftButton && edges)
        beginDrag(edges, e->globalPosition().toPoint());
    else
        QDialog::mousePressEvent(e);
}

void DialogFrame::mouseMoveEvent(QMouseEvent* e)
{
    if (!drag_.on) {
        const Qt::Edges edges = edgesAt(e->position().toPoint());
        const bool h = edges & (Qt::LeftEdge | Qt::RightEdge);
        const bool v = edges & (Qt::TopEdge | Qt::BottomEdge);
        if (!edges)
            unsetCursor();
        else if (h && v)
            setCursor(edges == (Qt::LeftEdge | Qt::TopEdge) || edges == (Qt::RightEdge | Qt::BottomEdge)
                          ? Qt::SizeFDiagCursor
                          : Qt::SizeBDiagCursor);
        else
            setCursor(h ? Qt::SizeHorCursor : Qt::SizeVerCursor);
        return;
    }
    const QPoint d = e->globalPosition().toPoint() - drag_.start;
    QRect g = drag_.geometry;
    const QSize min = minimumSize();
    if (!drag_.edges)
        g.translate(d);
    if (drag_.edges & Qt::LeftEdge)
        g.setLeft(std::min(g.left() + d.x(), g.right() + 1 - min.width()));
    if (drag_.edges & Qt::RightEdge)
        g.setRight(std::max(g.right() + d.x(), g.left() + min.width() - 1));
    if (drag_.edges & Qt::TopEdge)
        g.setTop(std::min(g.top() + d.y(), g.bottom() + 1 - min.height()));
    if (drag_.edges & Qt::BottomEdge)
        g.setBottom(std::max(g.bottom() + d.y(), g.top() + min.height() - 1));
    setGeometry(g);
}

void DialogFrame::mouseReleaseEvent(QMouseEvent* e)
{
    drag_.on = false;
    QDialog::mouseReleaseEvent(e);
}

bool DialogFrame::eventFilter(QObject* watched, QEvent* e)
{
    const QEvent::Type type = e->type();
    if (watched == header_ &&
        (type == QEvent::MouseButtonPress || type == QEvent::MouseMove || type == QEvent::MouseButtonRelease)) {
        auto* m = static_cast<QMouseEvent*>(e);
        if (type == QEvent::MouseButtonPress && m->button() == Qt::LeftButton) {
            beginDrag({}, m->globalPosition().toPoint());
            return true;
        }
        if (type == QEvent::MouseMove && drag_.on) {
            mouseMoveEvent(m);
            return true;
        }
        if (type == QEvent::MouseButtonRelease && drag_.on) {
            mouseReleaseEvent(m);
            return true;
        }
    }
    return QDialog::eventFilter(watched, e);
}

void DialogFrame::requestHelp()
{
    emit helpRequested(command_.isEmpty() ? id_ : command_);
}

} // namespace trion::ui
