#include "app/shell/start/StartPage.h"

#include "app/core/edition/EditionGate.h"
#include "app/core/i18n/UiFormat.h"
#include "app/theme/ThemeManager.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/controls/Paint.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "app/widgets/views/VirtualTableView.h"
#include "core/base/settings/Settings.h"

#include <QAbstractButton>
#include <QCache>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QMimeData>
#include <QPainter>
#include <QProgressBar>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStaticText>
#include <QUrl>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace trion::shell {

using edition::Edition;
using theme::IconProvider;
using theme::IconState;
using theme::TextRole;
using theme::Weight;
using ui::Button;
using ui::Severity;

namespace {

const settings::Setting<int> kViewSetting{"start/view", 0}; // 0 lưới, 1 danh sách (prototype startView)
const settings::Setting<int> kSortSetting{"start/sort", 0}; // RecentFilter::Sort

constexpr int kThumbW = 16, kThumbH = 10; // .rf canvas aspect-ratio 16 / 10
constexpr int kTemplatePreview = 40;      // .tpl i
constexpr QSize kLearnThumb{64, 40};      // .learn .th
constexpr int kPin = 22;                  // .rf .pin
constexpr int kPinInset = 12;             // .rf .pin top, right
constexpr int kMdIcon = 20;               // .ic.md
constexpr int kSmallIcon = 12;            // .ic.s12
constexpr int kFolderMenu = 8;            // [đề xuất] số thư mục của "Mở thư mục gần đây"
constexpr int kTrialLength = 30;          // ngày dùng thử (07.1 §2)
// .rf-list colgroup: Ngày sửa 110, Kích thước 90, Phiên bản 90, ghim 36
constexpr int kListColumns[RecentFiles::ColumnCount] = {0, 110, 90, 90, 36};

// Mẫu của mục Tạo mới, theo prototype (tpls). file rỗng: Mẫu của tôi (QNEW, mẫu mặc định trong Tùy chọn).
// ponytail: tên tệp mẫu tạm; VN-008 (mẫu .dwt A0–A4) và CMD-241 (TemplateResolver) thay.
struct Template {
    const char* label;
    const char* file;
};
constexpr Template kTemplates[] = {
    {QT_TRANSLATE_NOOP("trion::shell::StartPage", "TCVN A3 ngang"), "TCVN-A3-ngang.dwt"},
    {QT_TRANSLATE_NOOP("trion::shell::StartPage", "TCVN A3 dọc"), "TCVN-A3-doc.dwt"},
    {QT_TRANSLATE_NOOP("trion::shell::StartPage", "TCVN A1 ngang"), "TCVN-A1-ngang.dwt"},
    {QT_TRANSLATE_NOOP("trion::shell::StartPage", "TCVN A2 ngang"), "TCVN-A2-ngang.dwt"},
    {QT_TRANSLATE_NOOP("trion::shell::StartPage", "TCVN A4 dọc"), "TCVN-A4-doc.dwt"},
    {QT_TRANSLATE_NOOP("trion::shell::StartPage", "Trống (mm)"), "Trong-mm.dwt"},
    {QT_TRANSLATE_NOOP("trion::shell::StartPage", "Trống (inch)"), "Trong-inch.dwt"},
    {QT_TRANSLATE_NOOP("trion::shell::StartPage", "Mẫu của tôi"), nullptr},
};

// Học tập (prototype): tiêu đề, dòng phụ, id phát qua screenRequested.
struct Lesson {
    const char* title;
    const char* detail;
    const char* id;
};
constexpr Lesson kLessons[] = {
    {QT_TRANSLATE_NOOP("trion::shell::StartPage", "Chuyển từ AutoCAD sang trong 1 ngày"),
     QT_TRANSLATE_NOOP("trion::shell::StartPage", "12 phút · các lệnh và phím tắt giữ nguyên"), "migrate"},
    {QT_TRANSLATE_NOOP("trion::shell::StartPage", "Đo diện tích phòng bằng một cú bấm"),
     QT_TRANSLATE_NOOP("trion::shell::StartPage", "2 phút · Đo nhanh"), "video-measure"},
    {QT_TRANSLATE_NOOP("trion::shell::StartPage", "Bác sĩ Font sửa chữ TCVN3, VNI thế nào"),
     QT_TRANSLATE_NOOP("trion::shell::StartPage", "3 phút"), "video-fontdoctor"},
};

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

// Màu trên nền vùng vẽ (ô ảnh): giá trị :root, không đổi theo chủ đề giao diện (07.1 §13.2).
QColor canvasCol(tok::ColorId id)
{
    return theme::toQColor(tok::color(id, tok::Mode{}));
}

// Màu chữ của QLabel; vô hiệu: --c-text-dis.
void setInk(QWidget* w, const QColor& c)
{
    QPalette p = w->palette();
    p.setColor(QPalette::Active, QPalette::WindowText, c);
    p.setColor(QPalette::Inactive, QPalette::WindowText, c);
    p.setColor(QPalette::Disabled, QPalette::WindowText, col(tok::c_text_dis));
    w->setPalette(p);
}

QLabel* textLabel(QWidget* parent = nullptr)
{
    auto* l = new QLabel(parent);
    l->setTextFormat(Qt::PlainText);
    l->setWordWrap(true);
    return l;
}

void drawGlyph(QPainter& p, const QString& name, int size, const QRect& box, const QColor& c)
{
    QRect r(0, 0, size, size);
    r.moveCenter(box.center());
    p.drawPixmap(r, ui::paint::tinted(
                        IconProvider::instance().pixmap(name, size, p.device()->devicePixelRatio(), IconState::Normal), c));
}

void fillRounded(QPainter& p, const QRectF& r, const QColor& c, qreal radius)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawRoundedRect(r, radius, radius);
    p.restore();
}

// Khung .start .card, .tpl, .big, .rf: nền --c-raised, viền 1 px --c-border (rê: --c-border-ctl), bo --r-lg.
void drawCardBox(QPainter& p, const QRect& r, bool hover)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(col(hover ? tok::c_border_ctl : tok::c_border));
    p.setBrush(col(tok::c_raised));
    const qreal radius = tm().px(tok::r_lg);
    p.drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    p.restore();
}

// Một dòng chữ đã shaping (QStaticText) theo bề rộng và chữ, cho một font: lọc, cuộn vẽ lại thẻ và dòng không shaping
// lại (~20–40 µs mỗi chuỗi; thẻ 3 chuỗi, dòng bảng 5). Đổi font (theme) thì bỏ hết.
// ponytail: QCache bỏ chuỗi lâu không dùng khi quá kTextCache mục (200 tệp × vài chuỗi, hai bề rộng thẻ/cột).
class TextCache {
public:
    static constexpr int kTextCache = 2000;

    // Căn trái, giữa theo chiều dọc trong box, bút của p; không vừa thì rút gọn theo mode. Trả bề rộng đã vẽ.
    qreal draw(QPainter& p, const QFont& font, const QRect& box, const QString& text,
               Qt::TextElideMode mode = Qt::ElideRight)
    {
        if (box.width() <= 0)
            return 0;
        if (font != font_) {
            cache_.clear();
            font_ = font;
        }
        const QFontMetricsF fm(font);
        const QString key = QString::number(box.width()) + u'|' + QString::number(int(mode)) + u'|' + text;
        QStaticText* line = cache_.object(key);
        if (!line) {
            line = new QStaticText(text);
            line->setTextFormat(Qt::PlainText);
            line->prepare(QTransform(), font);
            if (line->size().width() > box.width()) {
                line->setText(fm.elidedText(text, mode, box.width()));
                line->prepare(QTransform(), font);
            }
            cache_.insert(key, line);
        }
        p.setFont(font);
        p.drawStaticText(QPointF(box.left(), box.top() + (box.height() - fm.height()) / 2), *line);
        return line->size().width();
    }

private:
    QFont font_;
    QCache<QString, QStaticText> cache_{kTextCache};
};

bool drawing(const QString& path)
{
    const QString suffix = QFileInfo(path).suffix();
    return suffix.compare(u"dwg"_s, Qt::CaseInsensitive) == 0 || suffix.compare(u"dxf"_s, Qt::CaseInsensitive) == 0
           || suffix.compare(u"dwt"_s, Qt::CaseInsensitive) == 0;
}

void toast(Severity severity, const QString& text, ui::MessageAction action = {})
{
    if (auto* host = ui::ToastHost::instance())
        host->post(severity, text, std::move(action));
}

// .start h2: --fs-xs đậm, chữ hoa, giãn .08em, --c-text-3.
QFont headingFont()
{
    QFont f = theme::font(TextRole::Caption, Weight::Bold);
    f.setCapitalization(QFont::AllUppercase);
    f.setLetterSpacing(QFont::PercentageSpacing, 108);
    return f;
}

// .drop-hint: viền nét đứt 2 px --c-accent, nền --c-accent-soft, chữ --fs-l giữa; không nhận chuột.
class DropHint final : public QLabel {
public:
    explicit DropHint(QWidget* parent) : QLabel(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAlignment(Qt::AlignCenter);
        setWordWrap(true);
        setTextFormat(Qt::PlainText);
        setFont(theme::font(TextRole::Emphasis));
        hide();
    }

protected:
    void paintEvent(QPaintEvent* e) override
    {
        {
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QPen(col(tok::c_accent), 2, Qt::DashLine));
            p.setBrush(col(tok::c_accent_soft));
            const qreal r = tm().px(tok::r_xl);
            p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), r, r);
        }
        QLabel::paintEvent(e);
    }
};

} // namespace

// ------------------------------------------------------------------------------------------------- Card

// Nút tự vẽ của trang: .big (Mở…), .tpl (mẫu), .learn a, .news a. Chữ là QLabel con (xuống dòng khi dài, vi-XL), không
// nhận chuột; biểu tượng, giấy mẫu, ô video vẽ vào chỗ của slot_. Space, Enter bấm; viền focus khi đến từ bàn phím.
class StartPage::Card final : public QAbstractButton {
public:
    enum class Kind : std::uint8_t { Big, Template, Learn, News };

    Card(Kind kind, const QString& icon, QWidget* parent = nullptr)
        : QAbstractButton(parent), kind_(kind), icon_(icon), slot_(new QWidget(this)), title_(textLabel(this)),
          detail_(textLabel(this))
    {
        setAttribute(Qt::WA_Hover);
        setFocusPolicy(Qt::StrongFocus);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        for (QWidget* w : {slot_, static_cast<QWidget*>(title_), static_cast<QWidget*>(detail_)})
            w->setAttribute(Qt::WA_TransparentForMouseEvents);
        detail_->hide();
        auto* text = new QVBoxLayout;
        text->setContentsMargins({});
        text->setSpacing(0);
        text->addWidget(title_);
        text->addWidget(detail_);
        QBoxLayout* box = kind == Kind::Template ? static_cast<QBoxLayout*>(new QVBoxLayout(this))
                                                 : static_cast<QBoxLayout*>(new QHBoxLayout(this));
        box->addWidget(slot_, 0, kind == Kind::Template ? Qt::Alignment() : Qt::AlignVCenter);
        box->addLayout(text, 1);
        const int s1 = px(tok::sp_1), s2 = px(tok::sp_2), s3 = px(tok::sp_3), s4 = px(tok::sp_4);
        switch (kind) {
        case Kind::Big: // .big: padding 9px 10px, gap 10px; chữ --fw-m, dòng phụ --fs-xs
            box->setContentsMargins(s4 + s1, s4, s4 + s1, s4);
            box->setSpacing(s4 + s1);
            slot_->setFixedSize(kMdIcon, kMdIcon);
            title_->setFont(theme::font(TextRole::Ui, Weight::Medium));
            break;
        case Kind::Template: // .tpl: padding 6px, gap 4px, chữ --fs-xs; giấy cao 40px
            box->setContentsMargins(s3, s3, s3, s3);
            box->setSpacing(s2);
            slot_->setFixedHeight(kTemplatePreview);
            title_->setFont(theme::font(TextRole::Caption));
            break;
        case Kind::Learn: // .learn a, .news a: padding 6px, gap 10px, chữ --fs-s, dòng phụ --fs-xs
        case Kind::News:
            box->setContentsMargins(s3, s3, s3, s3);
            box->setSpacing(s4 + s1);
            slot_->setFixedSize(kind == Kind::Learn ? kLearnThumb : QSize(px(tok::ic), px(tok::ic)));
            title_->setFont(theme::font(TextRole::Label));
            break;
        }
        detail_->setFont(theme::font(TextRole::Caption));
        ink();
        connect(&tm(), &theme::ThemeManager::changed, this, [this] { ink(); });
    }

    // path: dòng phụ là đường dẫn (gần như không có chỗ xuống dòng): một dòng rút gọn giữa theo bề rộng có, đủ chữ ở
    // tooltip và tên mô tả cho trình đọc màn hình.
    void setTexts(const QString& title, const QString& detail = {}, bool path = false)
    {
        // Tên cho trình đọc màn hình; text() của nút để trống: chữ hiện là QLabel con (xuống dòng khi dài, TruncationProbe
        // đo theo nhãn).
        setAccessibleName(title);
        title_->setText(title);
        path_ = path ? detail : QString();
        detail_->setSizePolicy(path ? QSizePolicy::Ignored : QSizePolicy::Preferred, QSizePolicy::Preferred);
        detail_->setWordWrap(!path); // sau setSizePolicy: QLabel đặt heightForWidth theo wordWrap
        detail_->setText(detail);
        fitPath();
        detail_->setVisible(!detail.isEmpty());
        setToolTip(path_);
        setAccessibleDescription(detail);
    }
    QLabel* titleLabel() const { return title_; }
    QLabel* detailLabel() const { return detail_; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const bool hover = (underMouse() || isDown()) && isEnabled();
        const QRect slot = slot_->geometry();
        const qreal r = tm().px(tok::r);
        switch (kind_) {
        case Kind::Big:
            drawCardBox(p, rect(), hover);
            drawGlyph(p, icon_, kMdIcon, slot, col(isEnabled() ? tok::c_accent : tok::c_text_dis));
            break;
        case Kind::Template: { // .tpl i: tờ giấy --c-paper, khung tên lệch 4 px
            drawCardBox(p, rect(), hover);
            fillRounded(p, slot, col(tok::c_paper), r);
            const int in = px(tok::sp_2);
            p.setPen(col(tok::c_paper_print));
            p.drawRect(QRectF(slot.adjusted(in, in, -in, -in)).adjusted(0.5, 0.5, -0.5, -0.5));
            break;
        }
        case Kind::Learn:
        case Kind::News:
            if (hover)
                fillRounded(p, rect(), col(tok::c_hover), r);
            if (kind_ == Kind::Learn) { // .learn .th: ô nền vùng vẽ có nút phát
                fillRounded(p, slot, canvasCol(tok::c_canvas), r);
                drawGlyph(p, u"play"_s, kMdIcon, slot, canvasCol(tok::c_text_2));
            } else {
                drawGlyph(p, icon_, px(tok::ic), slot, col(tok::c_text_2));
            }
            break;
        }
        if (hasFocus() && ui::paint::focusVisible(this))
            ui::paint::drawRing(p, rect(), col(tok::c_focus));
    }

    void keyPressEvent(QKeyEvent* e) override
    {
        if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && !(e->modifiers() & ~Qt::KeypadModifier)) {
            click();
            return;
        }
        QAbstractButton::keyPressEvent(e);
    }

    bool event(QEvent* e) override
    {
        if (e->type() == QEvent::HoverEnter || e->type() == QEvent::HoverLeave)
            update();
        return QAbstractButton::event(e);
    }

    void resizeEvent(QResizeEvent* e) override // bố cục con đã đặt xong (QLayout nhận Resize trước)
    {
        QAbstractButton::resizeEvent(e);
        fitPath();
    }

private:
    void fitPath()
    {
        if (!path_.isEmpty() && detail_->width() > 0)
            detail_->setText(detail_->fontMetrics().elidedText(path_, Qt::ElideMiddle, detail_->width()));
    }

    void ink()
    {
        setInk(title_, col(tok::c_text));
        setInk(detail_, col(tok::c_text_3));
        update();
    }

    Kind kind_;
    QString icon_;
    QWidget* slot_;
    QLabel* title_;
    QLabel* detail_;
    QString path_; // đường dẫn đầy đủ của dòng phụ rút gọn; rỗng: dòng phụ xuống dòng như chữ thường
};

// .start .card: khung có đệm --sp-5, các dòng cách --sp-4.
class StartPage::Box final : public QWidget {
public:
    explicit Box(QWidget* parent = nullptr) : QWidget(parent)
    {
        auto* box = new QVBoxLayout(this);
        box->setContentsMargins(px(tok::sp_5), px(tok::sp_5), px(tok::sp_5), px(tok::sp_5));
        box->setSpacing(px(tok::sp_4));
    }
    QVBoxLayout* box() const { return static_cast<QVBoxLayout*>(layout()); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        drawCardBox(p, rect(), false);
    }
};

// ------------------------------------------------------------------------------------------------- lưới tệp gần đây

// .rf-grid: thẻ rộng tối thiểu kCardMinWidth, giãn đều cho kín hàng, cách nhau --sp-5. QListView chảy trái → phải,
// xuống dòng, mỗi ô lưới = thẻ + khoảng cách bên phải và dưới (thẻ đầu sát mép trái như prototype). Chỉ vẽ ô đang thấy.
class StartPage::Grid final : public QListView {
public:
    std::function<void()> laidOut;           // bố trí, cuộn, đổi DPR xong: xin ảnh cho thẻ đang thấy
    std::function<void(int row)> pinClicked; // bấm nút ghim ở góc thẻ

    explicit Grid(QWidget* parent = nullptr) : QListView(parent)
    {
        setFlow(QListView::LeftToRight);
        setWrapping(true);
        setResizeMode(QListView::Adjust);
        setMovement(QListView::Static);
        setUniformItemSizes(true);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        setFrameShape(QFrame::NoFrame);
        setMouseTracking(true);
        viewport()->setAttribute(Qt::WA_Hover);
        viewport()->setAutoFillBackground(false); // nền trang --c-base
    }

    int gap() const { return px(tok::sp_5); }
    QRect card(const QRect& cell) const { return cell.adjusted(0, 0, -gap(), -gap()); }
    QRect thumb(const QRect& card) const
    {
        const int pad = px(tok::sp_4), w = card.width() - 2 * pad;
        return {card.left() + pad, card.top() + pad, w, w * kThumbH / kThumbW};
    }
    QRect pin(const QRect& card) const
    {
        return {card.right() + 1 - kPinInset - kPin, card.top() + kPinInset, kPin, kPin};
    }
    QSize thumbPixels() const
    {
        return (QSizeF(thumb(card(QRect(QPoint(), gridSize()))).size()) * devicePixelRatio()).toSize();
    }

    // Số cột theo bề rộng (chừa thanh cuộn và khung như QListViewPrivate::prepareItemsLayout, để hàng không xuống dòng
    // sớm), thẻ giãn đều; cao theo font và token.
    void relayout()
    {
        int extent = style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, verticalScrollBar());
        if (style()->styleHint(QStyle::SH_ScrollView_FrameOnlyAroundContents)) {
            QStyleOption o;
            o.initFrom(this);
            extent += 2 * style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &o);
        }
        // − 1: QListView xuống dòng khi mép phải ô vượt bounds.right()
        const int bounds = std::max(0, maximumViewportSize().width() - extent - 1);
        const int g = gap(), cols = std::max(1, bounds / (kCardMinWidth + g));
        const int cell = std::max(kCardMinWidth + g, bounds / cols);
        // .rf: đệm 8, ảnh 16:10, cách 6, tên, cách 6, ngày · cỡ · phiên bản, cách 6 − 4, thư mục, đệm 8
        const int pad = px(tok::sp_4), in = px(tok::sp_3);
        const int name = QFontMetrics(theme::font(TextRole::Ui, Weight::Medium)).height();
        const int meta = QFontMetrics(theme::font(TextRole::Caption)).height();
        const int h = 2 * pad + (cell - g - 2 * pad) * kThumbH / kThumbW + 3 * in - px(tok::sp_2) + name + 2 * meta;
        if (const QSize size(cell, h + g); size != gridSize())
            setGridSize(size);
    }

protected:
    void resizeEvent(QResizeEvent* e) override
    {
        relayout();
        QListView::resizeEvent(e);
    }
    void updateGeometries() override
    {
        QListView::updateGeometries();
        if (laidOut)
            laidOut();
    }
    void scrollContentsBy(int dx, int dy) override
    {
        QListView::scrollContentsBy(dx, dy);
        if (laidOut)
            laidOut();
    }
    bool event(QEvent* e) override
    {
        if (e->type() == QEvent::DevicePixelRatioChange && laidOut)
            laidOut();
        return QListView::event(e);
    }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        const QPoint at = e->position().toPoint();
        const QModelIndex i = indexAt(at);
        if (e->button() == Qt::LeftButton && i.isValid() && pin(card(visualRect(i))).contains(at) && pinClicked) {
            setState(NoState); // không phát clicked: bấm ghim không mở tệp
            pinClicked(i.row());
            e->accept();
            return;
        }
        QListView::mouseReleaseEvent(e);
    }
};

// Thẻ .rf: khung, ảnh (cache của ThumbnailQueue, chưa có: ô --c-canvas; lỗi: biểu tượng tệp; không tìm thấy: biểu tượng
// cảnh báo), tên, ngày · cỡ · phiên bản (không tìm thấy: "Không tìm thấy" --c-warn), thư mục, nút ghim khi rê hoặc đã ghim.
class StartPage::GridDelegate final : public QStyledItemDelegate {
public:
    GridDelegate(Grid* grid, ThumbnailQueue& thumbs) : QStyledItemDelegate(grid), grid_(grid), thumbs_(thumbs)
    {
        connect(&tm(), &theme::ThemeManager::changed, this, [this] { background_[0] = background_[1] = {}; });
    }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return grid_->gridSize(); }

    void paint(QPainter* p, const QStyleOptionViewItem& o, const QModelIndex& i) const override
    {
        const QRect card = grid_->card(o.rect);
        const bool hover = o.state.testFlag(QStyle::State_MouseOver);
        p->save();
        // Pixmap, ảnh, biểu tượng đều 1:1 điểm ảnh thiết bị: tắt lọc mượt của view để Qt chép thẳng (có lọc: ~0,2 ms
        // mỗi thẻ thay vì ~0,02 ms).
        p->setRenderHint(QPainter::SmoothPixmapTransform, false);
        p->drawPixmap(card.topLeft(), background(card.size(), hover, p->device()->devicePixelRatio()));

        const QRect thumb = grid_->thumb(card);
        const QString path = i.data(RecentFiles::PathRole).toString();
        const bool missing = i.data(RecentFiles::MissingRole).toBool();
        if (const QImage* image = missing ? nullptr : thumbs_.image(path, grid_->thumbPixels())) {
            QRectF r(QPointF(), QSizeF(image->size()) / p->device()->devicePixelRatio()); // 1:1 điểm ảnh thiết bị
            r.moveCenter(QRectF(thumb).center());
            p->drawImage(r, *image);
        } else if (missing || thumbs_.failed(path)) {
            drawGlyph(*p, missing ? u"warn"_s : u"file"_s, kMdIcon, thumb, canvasCol(tok::c_text_3));
        }

        const int pad = px(tok::sp_4), in = px(tok::sp_3), w = card.width() - 2 * pad, x = card.left() + pad;
        const QFont nameFont = theme::font(TextRole::Ui, Weight::Medium), metaFont = theme::font(TextRole::Caption);
        const int nameH = QFontMetrics(nameFont).height(), metaH = QFontMetrics(metaFont).height();
        int y = thumb.bottom() + 1 + in;
        p->save();
        p->setPen(col(tok::c_text));
        names_.draw(*p, nameFont, QRect(x, y, w, nameH), i.data().toString());
        y += nameH + in;
        if (missing) { // không chỉ bằng màu: biểu tượng và chữ
            const QRect icon(x, y + (metaH - kSmallIcon) / 2, kSmallIcon, kSmallIcon);
            drawGlyph(*p, u"warn"_s, kSmallIcon, icon, col(tok::c_warn));
            const int tx = icon.right() + 1 + px(tok::sp_2);
            p->setPen(col(tok::c_warn));
            metas_.draw(*p, metaFont, QRect(tx, y, x + w - tx, metaH), RecentFiles::tr("Không tìm thấy"));
        } else {
            p->setPen(col(tok::c_text_3));
            metas_.draw(*p, metaFont, QRect(x, y, w, metaH), i.data(RecentFiles::MetaRole).toString());
        }
        y += metaH + in - px(tok::sp_2);
        p->setPen(col(tok::c_text_3));
        metas_.draw(*p, metaFont, QRect(x, y, w, metaH), i.data(RecentFiles::FolderRole).toString(), Qt::ElideMiddle);
        p->restore();

        if (const bool pinned = i.data(RecentFiles::PinnedRole).toBool(); pinned || hover) {
            const QRect pin = grid_->pin(card);
            fillRounded(*p, pin, col(tok::c_raised), tm().px(tok::r));
            drawGlyph(*p, u"pin"_s, px(tok::ic), pin, col(pinned ? tok::c_accent : tok::c_text_2));
        }
        if (o.state.testFlag(QStyle::State_HasFocus) && ui::paint::focusVisible(grid_))
            ui::paint::drawRing(*p, card, col(tok::c_focus));
        p->restore();
    }

private:
    // Khung thẻ và ô ảnh giữ chỗ giống nhau ở mọi thẻ: vẽ AA một lần vào pixmap (theo cỡ, rê, DPR; đổi theme thì bỏ)
    // rồi chép: vẽ thẳng ~0,2 ms mỗi thẻ, chép ~0,02 ms.
    struct Background {
        QPixmap pixmap;
        QSize size;
        qreal dpr = 0;
    };
    const QPixmap& background(QSize card, bool hover, qreal dpr) const
    {
        Background& b = background_[hover ? 1 : 0];
        if (b.size != card || b.dpr != dpr) {
            b = {QPixmap((QSizeF(card) * dpr).toSize()), card, dpr};
            b.pixmap.setDevicePixelRatio(dpr);
            b.pixmap.fill(Qt::transparent);
            QPainter q(&b.pixmap);
            const QRect box(QPoint(), card);
            drawCardBox(q, box, hover);
            fillRounded(q, grid_->thumb(box), canvasCol(tok::c_canvas), tm().px(tok::r));
        }
        return b.pixmap;
    }

    Grid* grid_;
    ThumbnailQueue& thumbs_;
    mutable Background background_[2]; // thường, rê
    mutable TextCache names_, metas_;
};

// Bảng .rf-list: cột Tên vẽ biểu tượng tệp, tên --fw-m rồi thư mục --c-text-3; tệp không tìm thấy: biểu tượng cảnh báo ở
// cột Tên và "Không tìm thấy" --c-warn thay ngày sửa; cột ghim: ngôi sao --c-accent khi đã ghim, ghim --c-text-3 khi chưa.
// Cột khác như ViewItemDelegate.
class StartPage::ListDelegate final : public ui::ViewItemDelegate {
public:
    using ui::ViewItemDelegate::ViewItemDelegate;

    void paint(QPainter* p, const QStyleOptionViewItem& o, const QModelIndex& i) const override
    {
        const bool missing = i.data(RecentFiles::MissingRole).toBool();
        const bool note = missing && i.column() == RecentFiles::Modified;
        p->save();
        p->setRenderHint(QPainter::SmoothPixmapTransform, false); // biểu tượng 1:1 điểm ảnh thiết bị: chép thẳng
        if (i.column() != RecentFiles::Name && i.column() != RecentFiles::Pin && !note) {
            // Như ViewItemDelegate (lề, màu chữ khi chọn, vô hiệu), chữ qua cache.
            const int m = o.widget ? o.widget->style()->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, o.widget) + 1
                                   : 0;
            QPalette::ColorGroup group = o.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
            if (group == QPalette::Normal && !(o.state & QStyle::State_Active))
                group = QPalette::Inactive;
            p->setPen(o.palette.color(group, o.state & QStyle::State_Selected ? QPalette::HighlightedText
                                                                                : QPalette::Text));
            p->setClipRect(o.rect);
            cells_.draw(*p, o.font, o.rect.adjusted(m, 0, -m, 0), i.data().toString(), o.textElideMode);
            p->restore();
            return;
        }
        const int m = px(tok::sp_4);
        const QRect box = o.rect.adjusted(m, 0, -m, 0);
        if (i.column() == RecentFiles::Pin) {
            const bool pinned = i.data(RecentFiles::PinnedRole).toBool();
            drawGlyph(*p, pinned ? u"star"_s : u"pin"_s, kSmallIcon, box, col(pinned ? tok::c_accent : tok::c_text_3));
            p->restore();
            return;
        }
        QRect icon(box.left(), 0, kSmallIcon, kSmallIcon);
        icon.moveTop(box.center().y() - kSmallIcon / 2);
        drawGlyph(*p, missing ? u"warn"_s : u"file"_s, kSmallIcon, icon, col(missing ? tok::c_warn : tok::c_text_2));
        int x = icon.right() + 1 + px(tok::sp_3);
        const auto rest = [&] { return QRect(x, box.top(), box.right() + 1 - x, box.height()); };
        if (note) { // không chỉ bằng màu: biểu tượng và chữ
            p->setPen(col(tok::c_warn));
            cells_.draw(*p, o.font, rest(), RecentFiles::tr("Không tìm thấy"));
        } else {
            p->setPen(col(tok::c_text));
            x += qRound(names_.draw(*p, theme::font(TextRole::Ui, Weight::Medium), rest(), i.data().toString()))
                 + px(tok::sp_3);
            if (x < box.right()) {
                p->setPen(col(tok::c_text_3));
                cells_.draw(*p, o.font, rest(), i.data(RecentFiles::FolderRole).toString(), Qt::ElideMiddle);
            }
        }
        p->restore();
    }

    // Cỡ cần của ô (TruncationProbe): biểu tượng và tên (thư mục được rút gọn giữa nên không tính); "Không tìm thấy".
    QSize sizeHint(const QStyleOptionViewItem& o, const QModelIndex& i) const override
    {
        const QSize base = ui::ViewItemDelegate::sizeHint(o, i);
        if (i.column() == RecentFiles::Pin)
            return {kListColumns[RecentFiles::Pin], base.height()};
        if (i.column() == RecentFiles::Modified && i.data(RecentFiles::MissingRole).toBool())
            return {noteWidth(o.font), base.height()};
        if (i.column() != RecentFiles::Name)
            return base;
        return {2 * px(tok::sp_4) + kSmallIcon + px(tok::sp_3)
                    + QFontMetrics(theme::font(TextRole::Ui, Weight::Medium)).horizontalAdvance(i.data().toString()),
                base.height()};
    }
    static int noteWidth(const QFont& font) // ô "Không tìm thấy": đệm, biểu tượng, chữ
    {
        return 2 * px(tok::sp_4) + kSmallIcon + px(tok::sp_3)
               + QFontMetrics(font).horizontalAdvance(RecentFiles::tr("Không tìm thấy"));
    }

private:
    mutable TextCache names_, cells_;
};

// ------------------------------------------------------------------------------------------------- StartPage

StartPage::StartPage(RecentFiles& recent, QWidget* parent)
    : QWidget(parent), recent_(recent), filter_(recent), menu_(new ui::Menu(this))
{
    setAcceptDrops(true);
    menu_->setObjectName(u"startMenu"_s);
    const settings::Store& store = settings::Store::instance();
    view_ = store.get(kViewSetting) == 1 ? View::List : View::Grid;
    filter_.setSortKey(RecentFilter::Sort(std::clamp(store.get(kSortSetting), 0, 2)));
    build();
    retranslate();
    applyTheme();
    syncFolders();
    syncEmpty();

    connect(&tm(), &theme::ThemeManager::changed, this, &StartPage::applyTheme);
    if (auto* gate = edition::EditionGate::instance())
        connect(gate, &edition::EditionGate::editionChanged, this, [this] { syncEdition(); });
    connect(&thumbs_, &ThumbnailQueue::ready, grid_->viewport(), qOverload<>(&QWidget::update));
    connect(&recent_, &RecentFiles::fileChanged, &thumbs_, &ThumbnailQueue::forget);
    for (auto signal : {&QAbstractItemModel::rowsInserted, &QAbstractItemModel::rowsRemoved})
        connect(&recent_, signal, this, &StartPage::syncFolders);
    connect(&recent_, &QAbstractItemModel::rowsMoved, this, &StartPage::syncFolders);
    for (auto signal : {&QAbstractItemModel::rowsInserted, &QAbstractItemModel::rowsRemoved})
        connect(&filter_, signal, this, &StartPage::syncEmpty);
    connect(&filter_, &QAbstractItemModel::modelReset, this, &StartPage::syncEmpty);
    connect(&filter_, &QAbstractItemModel::layoutChanged, this, &StartPage::syncEmpty);
}

StartPage::~StartPage()
{
    // View và delegate (dùng filter_, thumbs_) đi trước các thành viên. scroll_ về nullptr trước: lúc hủy, viewport và
    // view còn gửi sự kiện qua eventFilter, không được gọi vào QScrollArea đang hủy.
    delete std::exchange(scroll_, nullptr);
}

void StartPage::build()
{
    const int s3 = px(tok::sp_3), s7 = px(tok::sp_7), h2 = px(tok::sp_4) + px(tok::sp_1); // .start h2 cách 10px
    const auto heading = [this] {
        QLabel* l = textLabel();
        l->setFont(headingFont());
        headings_ << l;
        return l;
    };
    const auto section = [&](QBoxLayout* column, QLabel* title) {
        auto* box = new QVBoxLayout;
        box->setSpacing(h2);
        box->addWidget(title);
        column->addLayout(box);
        return box;
    };

    scroll_ = new QScrollArea(this);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setFocusPolicy(Qt::NoFocus); // Tab đi thẳng vào nút đầu trang
    scroll_->setWidgetResizable(true);
    scroll_->viewport()->setAutoFillBackground(false);
    scroll_->viewport()->installEventFilter(this); // đổi cỡ: lề giữa trang
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins({});
    outer->addWidget(scroll_);
    auto* page = new QWidget;
    page->setAutoFillBackground(false);
    content_ = new QHBoxLayout(page);
    content_->setSpacing(kGutter);
    scroll_->setWidget(page);

    // Cột trái: Tạo mới, Mở, thẻ phiên bản.
    auto* left = new QWidget;
    left->setFixedWidth(kSideWidth);
    auto* leftBox = new QVBoxLayout(left);
    leftBox->setContentsMargins({});
    leftBox->setSpacing(s7);
    create_ = new QWidget;
    create_->setObjectName(u"createSection"_s);
    auto* createBox = new QVBoxLayout(create_);
    createBox->setContentsMargins({});
    createBox->setSpacing(h2);
    createBox->addWidget(heading());
    auto* tiles = new QGridLayout;
    tiles->setSpacing(s3);
    for (const Template& t : kTemplates) {
        auto* c = new Card(Card::Kind::Template, {});
        c->setObjectName(u"template%1"_s.arg(templates_.size()));
        const QString macro = t.file ? u"^C^C_NEW %1"_s.arg(QLatin1StringView(t.file)) : u"^C^C_QNEW"_s;
        connect(c, &QAbstractButton::clicked, this, [this, macro] { emit macroRequested(macro); });
        c->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); // .tpl-grid 1fr 1fr: hai cột bằng nhau
        tiles->addWidget(c, int(templates_.size()) / 2, int(templates_.size()) % 2);
        templates_ << c;
    }
    tiles->setColumnStretch(0, 1);
    tiles->setColumnStretch(1, 1);
    createBox->addLayout(tiles);
    browse_ = new Button({}, Button::Kind::Ghost);
    browse_->setObjectName(u"browseTemplates"_s);
    browse_->setControlSize(Button::Size::Small);
    connect(browse_, &QPushButton::clicked, this, [this] { emit macroRequested(u"^C^C_NEW"_s); });
    createBox->addWidget(browse_, 0, Qt::AlignLeft);
    leftBox->addWidget(create_);

    QVBoxLayout* openBox = section(leftBox, heading());
    openBox->setSpacing(s3);
    openFile_ = new Card(Card::Kind::Big, u"open"_s);
    openFile_->setObjectName(u"openFile"_s);
    connect(openFile_, &QAbstractButton::clicked, this, [this] { emit macroRequested(u"^C^C_OPEN"_s); });
    folders_ = new Card(Card::Kind::Big, u"folder"_s);
    folders_->setObjectName(u"recentFolders"_s);
    connect(folders_, &QAbstractButton::clicked, this, [this] {
        menu_->clear();
        for (const QString& dir : recent_.folders(kFolderMenu)) {
            QAction* a = menu_->addAction(IconProvider::icon(u"folder"_s), QString(dir).replace(u'&', u"&&"_s));
            connect(a, &QAction::triggered, this, [this, dir] { emit browseRequested(dir); });
        }
        menu_->popup(folders_->mapToGlobal(QPoint(0, folders_->height())));
    });
    recover_ = new Card(Card::Kind::Big, u"recover"_s);
    recover_->setObjectName(u"recover"_s);
    connect(recover_, &QAbstractButton::clicked, this, [this] { emit macroRequested(u"^C^C_RECOVER"_s); });
    for (Card* c : {openFile_, folders_, recover_})
        openBox->addWidget(c);

    edition_ = new Box;
    auto* editionRow = new QHBoxLayout;
    editionRow->setSpacing(px(tok::sp_4));
    editionName_ = textLabel();
    editionName_->setObjectName(u"editionName"_s);
    editionName_->setFont(theme::font(TextRole::Ui, Weight::Bold));
    editionBadge_ = new QLabel;
    editionBadge_->setObjectName(u"editionBadge"_s);
    editionBadge_->setTextFormat(Qt::PlainText);
    editionBadge_->setWordWrap(true); // chữ dài (vi-XL) xuống dòng trong huy hiệu thay vì bị cắt
    editionBadge_->setFont(theme::font(TextRole::Badge, Weight::Medium));
    editionBadge_->setAutoFillBackground(true);
    editionBadge_->setContentsMargins(px(tok::sp_3), px(tok::sp_1), px(tok::sp_3), px(tok::sp_1));
    editionRow->addWidget(editionName_, 1);
    editionRow->addWidget(editionBadge_, 0, Qt::AlignTop);
    edition_->box()->addLayout(editionRow);
    editionText_ = textLabel();
    editionText_->setObjectName(u"editionText"_s);
    editionText_->setFont(theme::font(TextRole::Label));
    edition_->box()->addWidget(editionText_);
    trial_ = new QProgressBar;
    trial_->setObjectName(u"trialProgress"_s);
    trial_->setTextVisible(false);
    trial_->setRange(0, kTrialLength);
    trial_->setFixedHeight(px(tok::sp_3)); // .prog cao 6px
    edition_->box()->addWidget(trial_);
    editionAction_ = new Button;
    editionAction_->setObjectName(u"editionAction"_s);
    editionAction_->setControlSize(Button::Size::Small);
    connect(editionAction_, &QPushButton::clicked, this, [this] {
        auto* gate = edition::EditionGate::instance();
        const Edition e = gate ? gate->edition() : Edition::Viewer;
        if (e == Edition::Viewer || e == Edition::ViewerPro || e == Edition::Trial) {
            if (gate) // Dùng thử: trang nâng cấp UI-L04; Mua: trang mua (UpgradeRouter mở web khi chưa có UI-L04)
                gate->upgrade().open({.feature = e == Edition::Trial ? u"buy"_s : u"trial"_s,
                                      .need = ui::Edition::Pro,
                                      .name = tr("Sửa bản vẽ")});
        } else {
            emit screenRequested(u"license"_s);
        }
    });
    edition_->box()->addWidget(editionAction_, 0, Qt::AlignLeft);
    leftBox->addWidget(edition_);
    leftBox->addStretch();
    content_->addWidget(left);

    // Giữa: tệp gần đây.
    auto* middle = new QVBoxLayout;
    middle->setSpacing(h2);
    auto* tools = new QHBoxLayout;
    tools->setSpacing(s3);
    recentTitle_ = heading();
    recentTitle_->setWordWrap(false);
    tools->addWidget(recentTitle_);
    tools->addStretch();
    search_ = new ui::SearchField;
    search_->setObjectName(u"recentSearch"_s);
    search_->setFixedWidth(kSearchWidth);
    connect(search_, &QLineEdit::textChanged, this, [this](const QString& text) {
        filter_.setQuery(text);
        syncEmpty();
    });
    tools->addWidget(search_);
    sort_ = new ui::DropDown;
    sort_->setObjectName(u"recentSort"_s);
    sort_->addItems({QString(), QString(), QString()}); // chữ đặt ở retranslate
    sort_->setCurrentIndex(int(filter_.sortKey()));
    connect(sort_, &QComboBox::currentIndexChanged, this, [this](int i) {
        filter_.setSortKey(RecentFilter::Sort(i));
        settings::Store::instance().set(kSortSetting, i);
    });
    tools->addWidget(sort_);
    viewToggle_ = new ui::SegmentedGroup;
    viewToggle_->setObjectName(u"recentViewToggle"_s);
    viewToggle_->addSegment({}, u"gridview"_s);
    viewToggle_->addSegment({}, u"listview"_s);
    viewToggle_->setCurrentIndex(view_ == View::List ? 1 : 0);
    connect(viewToggle_, &ui::SegmentedGroup::currentIndexChanged, this,
            [this](int i) { setView(i == 1 ? View::List : View::Grid); });
    tools->addWidget(viewToggle_);
    middle->addLayout(tools);

    stack_ = new QStackedWidget;
    grid_ = new Grid;
    grid_->setObjectName(u"recentGrid"_s);
    grid_->setModel(&filter_);
    grid_->setItemDelegate(new GridDelegate(grid_, thumbs_));
    grid_->laidOut = [this] { requestThumbnails(); };
    grid_->pinClicked = [this](int row) {
        const RecentFiles::Entry& e = filter_.entry(row);
        recent_.setPinned(e.path, !e.pinned);
    };
    list_ = new ui::VirtualTableView;
    list_->setObjectName(u"recentList"_s);
    list_->setModel(&filter_);
    list_->setItemDelegate(new ListDelegate(list_));
    list_->header()->setSectionResizeMode(RecentFiles::Name, QHeaderView::Stretch);
    for (QAbstractItemView* v : {static_cast<QAbstractItemView*>(grid_), static_cast<QAbstractItemView*>(list_)}) {
        v->installEventFilter(this);
        v->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(v, &QWidget::customContextMenuRequested, this, [this, v](const QPoint& pos) {
            const QModelIndex i = v->indexAt(pos);
            if (!i.isValid())
                return;
            v->setCurrentIndex(i);
            showMenu(i.row(), v->viewport()->mapToGlobal(pos));
        });
        connect(v, &QAbstractItemView::clicked, this, [this, v](const QModelIndex& i) {
            if (v == list_ && i.column() == RecentFiles::Pin)
                recent_.setPinned(filter_.entry(i.row()).path, !filter_.entry(i.row()).pinned);
            else
                open(i.row());
        });
        stack_->addWidget(v);
    }
    empty_ = new ui::EmptyState(u"file"_s);
    empty_->setObjectName(u"recentEmpty"_s);
    empty_->actionButton()->setKind(Button::Kind::Primary);
    connect(empty_, &ui::EmptyState::actionTriggered, this, [this] { emit screenRequested(u"sample"_s); });
    video_ = new Button;
    video_->setObjectName(u"emptyVideo"_s);
    video_->setControlSize(Button::Size::Small);
    video_->setIconName(u"play"_s);
    connect(video_, &QPushButton::clicked, this, [this] { emit screenRequested(u"video-intro"_s); });
    // Hai nút cùng hàng như prototype (.empty .hrow): EmptyState có một nút, chuyển nút đó vào hàng cùng nút video.
    if (auto* box = qobject_cast<QBoxLayout*>(empty_->layout())) {
        box->removeWidget(empty_->actionButton());
        auto* row = new QHBoxLayout;
        row->setSpacing(s3);
        row->addStretch();
        row->addWidget(empty_->actionButton());
        row->addWidget(video_);
        row->addStretch();
        box->insertLayout(box->count() - 1, row); // trước khoảng giãn cuối
    }
    stack_->addWidget(empty_);
    middle->addWidget(stack_, 1);
    content_->addLayout(middle, 1);

    // Cột phải: Học tập, Mẹo, Thông báo.
    auto* right = new QWidget;
    right->setFixedWidth(kAsideWidth);
    auto* rightBox = new QVBoxLayout(right);
    rightBox->setContentsMargins({});
    rightBox->setSpacing(s7);
    QVBoxLayout* learnBox = section(rightBox, heading());
    learnBox->setSpacing(s3);
    for (const Lesson& l : kLessons) {
        auto* c = new Card(Card::Kind::Learn, {});
        c->setObjectName(u"lesson%1"_s.arg(learn_.size()));
        connect(c, &QAbstractButton::clicked, this, [this, id = QString::fromLatin1(l.id)] { emit screenRequested(id); });
        learnBox->addWidget(c);
        learn_ << c;
    }
    QVBoxLayout* tipBox = section(rightBox, heading());
    tip_ = new Box;
    tipText_ = textLabel();
    tipText_->setFont(theme::font(TextRole::Label));
    tip_->box()->addWidget(tipText_);
    tipBox->addWidget(tip_);
    newsSection_ = new QWidget;
    newsSection_->setObjectName(u"news"_s);
    auto* newsBox = new QVBoxLayout(newsSection_);
    newsBox->setContentsMargins({});
    newsBox->setSpacing(h2);
    newsBox->addWidget(heading());
    newsList_ = new QVBoxLayout;
    newsList_->setSpacing(s3);
    newsBox->addLayout(newsList_);
    newsSection_->hide();
    rightBox->addWidget(newsSection_);
    rightBox->addStretch();
    content_->addWidget(right);

    dropHint_ = new DropHint(this);
    dropHint_->setObjectName(u"dropHint"_s);
}

void StartPage::retranslate()
{
    const QStringList titles{tr("Tạo mới"), tr("Mở"), tr("Tệp gần đây"), tr("Học tập"), tr("Mẹo"), tr("Thông báo")};
    for (qsizetype i = 0; i < headings_.size() && i < titles.size(); ++i)
        headings_[i]->setText(titles[i]);
    for (qsizetype i = 0; i < templates_.size(); ++i) {
        const Template& t = kTemplates[i];
        templates_[i]->setTexts(tr(t.label));
        ui::RichTooltip::attach(templates_[i], {.name = tr(t.label), .command = t.file ? u"NEW"_s : u"QNEW"_s});
    }
    browse_->setText(tr("Duyệt mẫu…"));
    ui::RichTooltip::attach(browse_, {.name = tr("Duyệt mẫu…"), .command = u"NEW"_s, .keyId = u"new"_s});
    const auto* tips = ui::RichTooltip::instance();
    const QString openKey = tips ? tips->keyText(u"open"_s) : QString();
    openFile_->setTexts(tr("Mở tệp…"), openKey.isEmpty() ? u"DWG, DXF, DWT"_s : u"DWG, DXF, DWT · %1"_s.arg(openKey));
    ui::RichTooltip::attach(openFile_, {.name = tr("Mở tệp…"), .command = u"OPEN"_s, .keyId = u"open"_s});
    recover_->setTexts(tr("Phục hồi bản vẽ lỗi"), tr("Mở tệp có lỗi cấu trúc để xem"));
    ui::RichTooltip::attach(recover_, {.name = tr("Phục hồi bản vẽ lỗi"), .command = u"RECOVER"_s});
    syncFolders();
    syncEdition();

    search_->setPlaceholderText(tr("Tìm trong tệp gần đây"));
    search_->setAccessibleName(tr("Tìm trong tệp gần đây"));
    sort_->setItemText(0, tr("Ngày sửa"));
    sort_->setItemText(1, tr("Tên"));
    sort_->setItemText(2, tr("Kích thước"));
    sort_->setAccessibleName(tr("Sắp xếp"));
    viewToggle_->setAccessibleName(tr("Kiểu xem"));
    const QString grid = tr("Lưới"), list = tr("Danh sách");
    viewToggle_->segment(0)->setAccessibleName(grid);
    viewToggle_->segment(0)->setToolTip(grid);
    viewToggle_->segment(1)->setAccessibleName(list);
    viewToggle_->segment(1)->setToolTip(list);
    grid_->setAccessibleName(tr("Tệp gần đây"));
    list_->setAccessibleName(tr("Tệp gần đây"));
    emit recent_.headerDataChanged(Qt::Horizontal, 0, RecentFiles::ColumnCount - 1); // tên cột, chữ cỡ tệp theo ngôn ngữ
    for (int c = RecentFiles::Modified; c < RecentFiles::ColumnCount; ++c) // rộng như prototype, đủ chữ đầu cột (vi-XL)
        list_->header()->resizeSection(c, std::max({kListColumns[c], list_->header()->sectionSizeHint(c),
                                                    c == RecentFiles::Modified ? ListDelegate::noteWidth(list_->font())
                                                                               : 0}));
    list_->viewport()->update();
    grid_->viewport()->update();
    empty_->setAction(tr("Mở bản vẽ mẫu"));
    video_->setText(tr("Xem video 2 phút"));
    syncEmpty();

    for (qsizetype i = 0; i < learn_.size(); ++i)
        learn_[i]->setTexts(tr(kLessons[i].title), tr(kLessons[i].detail));
    tipText_->setText(tr("Gõ tiếng Việt vào dòng lệnh để tìm lệnh: “đo”, “in”, “layer”. Tên lệnh AutoCAD vẫn dùng được "
                         "như cũ."));
    dropHint_->setText(tr("Thả tệp DWG, DXF để mở"));
}

void StartPage::applyTheme()
{
    for (QLabel* h : headings_)
        setInk(h, col(tok::c_text_3));
    setInk(editionName_, col(tok::c_text));
    setInk(editionText_, col(tok::c_text_2));
    setInk(tipText_, col(tok::c_text_2));
    setInk(dropHint_, col(tok::c_text));
    grid_->relayout();
    syncEdition(); // màu huy hiệu
    update();
}

void StartPage::syncEdition()
{
    auto* gate = edition::EditionGate::instance();
    const Edition e = gate ? gate->edition() : Edition::Viewer;
    create_->setVisible(edition::EditionGate::profile(e) == edition::UiProfile::Full); // Viewer không có Tạo mới
    // Tên, huy hiệu như thanh tiêu đề (edBadge của prototype).
    QString name, badge, text, action;
    tok::ColorId bg = tok::c_ed_pro, fg = tok::c_ed_pro_t;
    bool primary = true;
    switch (e) {
    case Edition::Viewer:
        name = tr("TrionCAD Viewer");
        badge = tr("Viewer");
        bg = tok::c_ed_viewer;
        fg = tok::c_ed_viewer_t;
        text = tr("Xem, đo, in và xuất PDF miễn phí. Sửa bản vẽ cần TrionCAD Pro.");
        action = tr("Dùng thử Pro 30 ngày");
        break;
    case Edition::ViewerPro:
        name = tr("TrionCAD Viewer Pro");
        badge = tr("Viewer Pro");
        bg = tok::c_ed_vp;
        fg = tok::c_ed_vp_t;
        text = tr("Xem, in hàng loạt, so sánh bản vẽ, markup. Sửa bản vẽ cần TrionCAD Pro.");
        action = tr("Dùng thử Pro 30 ngày");
        break;
    case Edition::Trial:
        name = tr("TrionCAD Pro");
        badge = tr("Dùng thử: còn %n ngày", nullptr, trialDays_);
        bg = tok::c_ed_trial;
        fg = tok::c_ed_trial_t;
        text = tr("Còn %n ngày dùng thử. Hết hạn, phần mềm chuyển về Viewer; bản vẽ vẫn mở và in được.", nullptr,
                  trialDays_);
        action = tr("Mua TrionCAD Pro");
        break;
    case Edition::Pro:
    case Edition::Education:
    case Edition::Enterprise:
        name = e == Edition::Pro ? tr("TrionCAD Pro")
                                 : e == Edition::Education ? tr("TrionCAD Giáo dục") : tr("TrionCAD Doanh nghiệp");
        badge = e == Edition::Pro ? tr("Pro") : e == Edition::Education ? tr("Giáo dục") : tr("Doanh nghiệp");
        bg = e == Edition::Pro ? tok::c_ed_pro : e == Edition::Education ? tok::c_ed_edu : tok::c_ed_ent;
        fg = e == Edition::Pro ? tok::c_ed_pro_t : e == Edition::Education ? tok::c_ed_edu_t : tok::c_ed_ent_t;
        if (expiry_.isValid())
            text = tr("Thuê bao còn hạn đến %1.").arg(i18n::formatDate(expiry_));
        action = tr("Trạng thái giấy phép");
        primary = false;
        break;
    }
    editionName_->setText(name);
    editionBadge_->setText(badge);
    QPalette p = editionBadge_->palette();
    p.setColor(QPalette::Window, col(bg));
    p.setColor(QPalette::WindowText, col(fg));
    editionBadge_->setPalette(p);
    editionText_->setText(text);
    editionText_->setVisible(!text.isEmpty());
    trial_->setVisible(e == Edition::Trial);
    trial_->setValue(std::clamp(trialDays_, 0, kTrialLength));
    trial_->setAccessibleName(badge);
    editionAction_->setText(action);
    editionAction_->setKind(primary ? Button::Kind::Primary : Button::Kind::Normal);
}

void StartPage::syncFolders()
{
    const QStringList dirs = recent_.folders(2);
    if (dirs.isEmpty())
        folders_->setTexts(tr("Mở thư mục gần đây"), tr("Chưa có thư mục gần đây"));
    else
        folders_->setTexts(tr("Mở thư mục gần đây"), u"%1…"_s.arg(dirs.join(u", "_s)), true);
    folders_->setEnabled(!dirs.isEmpty());
}

void StartPage::syncEmpty()
{
    const bool none = filter_.rowCount() == 0;
    QWidget* want = none ? static_cast<QWidget*>(empty_) : view_ == View::List ? static_cast<QWidget*>(list_) : grid_;
    if (none) {
        const QString q = filter_.query().trimmed();
        empty_->setText(q.isEmpty() ? tr("Chưa có tệp gần đây") : tr("Không có tệp khớp “%1”").arg(q),
                        q.isEmpty() ? tr("Mở một bản vẽ, hoặc kéo-thả tệp DWG vào đây.") : QString());
    }
    if (stack_->currentWidget() != want) {
        const bool focused = stack_->currentWidget() && stack_->currentWidget()->hasFocus();
        stack_->setCurrentWidget(want);
        if (focused) // trạng thái rỗng không nhận focus: về ô tìm
            (none ? static_cast<QWidget*>(search_) : want)->setFocus(Qt::OtherFocusReason);
        requestThumbnails();
    }
    // Đổi dòng, thứ tự (gõ tìm, sắp, kiểm tệp xong): lưới bố trí lại một lần (trễ, gộp mọi tín hiệu của lượt) rồi xin
    // ảnh qua laidOut. Không xin ở đây: mỗi tín hiệu một lần bố trí ép buộc, thẻ thấy tạm thời cũng bị xin ảnh.
}

void StartPage::setView(View view)
{
    if (view == view_)
        return;
    view_ = view;
    settings::Store::instance().set(kViewSetting, view == View::List ? 1 : 0);
    viewToggle_->setCurrentIndex(view == View::List ? 1 : 0);
    syncEmpty();
}

QAbstractItemView* StartPage::recentView() const
{
    return view_ == View::List ? static_cast<QAbstractItemView*>(list_) : grid_;
}

QSize StartPage::thumbnailSize() const
{
    return grid_->thumbPixels();
}

void StartPage::setTrialDays(int days)
{
    trialDays_ = days;
    syncEdition();
}

void StartPage::setExpiry(QDate date)
{
    expiry_ = date;
    syncEdition();
}

void StartPage::setNotifications(const QList<News>& items)
{
    while (QLayoutItem* item = newsList_->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    for (const News& n : items) {
        auto* c = new Card(Card::Kind::News, n.icon.isEmpty() ? u"info"_s : n.icon);
        c->setTexts(n.title, n.detail);
        connect(c, &QAbstractButton::clicked, this, [this, id = n.id] { emit screenRequested(id); });
        newsList_->addWidget(c);
    }
    newsSection_->setVisible(!items.isEmpty());
}

void StartPage::setFolderOpener(FolderOpener opener)
{
    opener_ = std::move(opener);
}

// Ảnh cho thẻ đang thấy, theo thứ tự hiện (trái → phải, trên → dưới). Bảng, trang ẩn: hủy hết.
void StartPage::requestThumbnails()
{
    if (!isVisible() || stack_->currentWidget() != grid_) {
        thumbs_.cancelAll();
        return;
    }
    QStringList paths;
    const QRect port = grid_->viewport()->rect();
    for (int r = 0; r < filter_.rowCount(); ++r) {
        const QRect box = grid_->visualRect(filter_.index(r, 0));
        if (box.top() > port.bottom())
            break;
        if (box.intersects(port) && !filter_.entry(r).missing)
            paths << filter_.entry(r).path;
    }
    thumbs_.request(paths, grid_->thumbPixels());
}

int StartPage::currentRow() const
{
    const QModelIndex i = recentView()->currentIndex();
    return i.isValid() ? i.row() : -1;
}

void StartPage::open(int row)
{
    if (row < 0 || row >= filter_.rowCount())
        return;
    const RecentFiles::Entry& e = filter_.entry(row);
    if (!e.missing) {
        emit openRequested({e.path});
        return;
    }
    QPointer<StartPage> self(this);
    toast(Severity::Warning,
          tr("Không tìm thấy “%1”. Tệp đã bị xóa, đổi tên hoặc ổ đĩa không vào được.").arg(e.name),
          {tr("Xóa khỏi danh sách"), [self, path = e.path] {
               if (self)
                   self->removePath(path);
           }});
}

void StartPage::removePath(const QString& path)
{
    const int row = recent_.indexOf(path);
    if (row < 0)
        return;
    const RecentFiles::Entry entry = recent_.entry(row);
    recent_.remove(path);
    QPointer<StartPage> self(this);
    toast(Severity::Ok, tr("Đã xóa “%1” khỏi danh sách. Tệp trên ổ đĩa không bị xóa.").arg(entry.name),
          {tr("Hoàn tác"), [self, row, entry] {
               if (self)
                   self->recent_.restore(row, entry);
           }});
}

void StartPage::openFolder(const QString& path)
{
    const QString dir = QFileInfo(path).absolutePath();
    if (!(opener_ ? opener_(dir) : QDesktopServices::openUrl(QUrl::fromLocalFile(dir))))
        toast(Severity::Warning, tr("Không mở được thư mục %1.").arg(QDir::toNativeSeparators(dir)));
}

// Menu chuột phải của tệp (prototype host.oncontextmenu): Mở, Mở chỉ đọc, Mở thư mục chứa, Sao chép đường dẫn | Ghim,
// Xóa khỏi danh sách. Mục có id (objectName) cho test.
void StartPage::showMenu(int row, const QPoint& globalPos)
{
    if (row < 0 || row >= filter_.rowCount())
        return;
    const RecentFiles::Entry e = filter_.entry(row);
    menu_->clear();
    const auto item = [this](const QString& text, const QString& icon, const QString& id, bool enabled,
                             std::function<void()> run) {
        QAction* a = menu_->addAction(IconProvider::icon(icon), text);
        a->setObjectName(id);
        a->setEnabled(enabled);
        connect(a, &QAction::triggered, this, std::move(run));
    };
    item(tr("Mở"), u"open"_s, u"_open"_s, !e.missing, [this, p = e.path] { emit openRequested({p}); });
    item(tr("Mở chỉ đọc"), u"lock"_s, u"_openReadOnly"_s, !e.missing,
         [this, p = e.path] { emit openReadOnlyRequested(p); });
    item(tr("Mở thư mục chứa"), u"folder"_s, u"_openFolder"_s, true, [this, p = e.path] { openFolder(p); });
    item(tr("Sao chép đường dẫn"), u"copyclip"_s, u"_copyPath"_s, true, [p = e.path] {
        const QString native = QDir::toNativeSeparators(p);
        QGuiApplication::clipboard()->setText(native);
        toast(Severity::Ok, tr("Đã sao chép: %1").arg(native));
    });
    menu_->addSeparator();
    item(e.pinned ? tr("Bỏ ghim") : tr("Ghim"), u"pin"_s, u"_pin"_s, true,
         [this, p = e.path, on = !e.pinned] { recent_.setPinned(p, on); });
    item(tr("Xóa khỏi danh sách"), u"x"_s, u"_remove"_s, true, [this, p = e.path] { removePath(p); });
    menu_->popup(globalPos);
}

// ------------------------------------------------------------------------------------------------- sự kiện

void StartPage::paintEvent(QPaintEvent*)
{
    QPainter(this).fillRect(rect(), col(tok::c_base));
}

void StartPage::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    recent_.refresh(); // tệp đã xóa, đổi tên khi tab ẩn
    requestThumbnails();
}

void StartPage::hideEvent(QHideEvent* e)
{
    QWidget::hideEvent(e);
    thumbs_.cancelAll(); // rời tab: hủy ngay mọi việc tạo ảnh
}

void StartPage::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    const int in = px(tok::sp_5); // .drop-hint inset 12px
    dropHint_->setGeometry(rect().adjusted(in, in, -in, -in));
}

void StartPage::changeEvent(QEvent* e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::LanguageChange)
        retranslate();
}

bool StartPage::eventFilter(QObject* watched, QEvent* e)
{
    if (!scroll_) // đang hủy (~StartPage)
        return false;
    if (watched == scroll_->viewport() && e->type() == QEvent::Resize) {
        // .start-in: rộng tối đa kMaxWidth (gồm đệm kGutter), giữa trang; đệm dưới 40px
        const int side = std::max(kGutter, (scroll_->viewport()->width() - kMaxWidth) / 2 + kGutter);
        content_->setContentsMargins(side, kGutter, side, kGutter + px(tok::sp_5));
        return false;
    }
    auto* view = qobject_cast<QAbstractItemView*>(watched);
    if (!view || e->type() != QEvent::KeyPress)
        return false;
    const auto* k = static_cast<QKeyEvent*>(e);
    const Qt::KeyboardModifiers mods = k->modifiers() & ~Qt::KeypadModifier;
    if ((k->key() == Qt::Key_Return || k->key() == Qt::Key_Enter) && !mods) {
        open(currentRow());
        return true;
    }
    if (k->key() == Qt::Key_Menu || (k->key() == Qt::Key_F10 && mods == Qt::ShiftModifier)) {
        const int row = currentRow();
        if (row >= 0) {
            const QRect r = view->visualRect(filter_.index(row, 0));
            showMenu(row, view->viewport()->mapToGlobal(r.center()));
        }
        return true;
    }
    return false;
}

void StartPage::dragEnterEvent(QDragEnterEvent* e)
{
    // Nhận mọi tệp trên máy để thả được rồi báo tệp sai định dạng (như dải tab tệp); URL web thì không.
    const QList<QUrl> urls = e->mimeData()->urls();
    if (std::none_of(urls.begin(), urls.end(), [](const QUrl& u) { return u.isLocalFile(); }))
        return;
    e->acceptProposedAction();
    dropHint_->raise();
    dropHint_->show();
}

void StartPage::dragLeaveEvent(QDragLeaveEvent* e)
{
    dropHint_->hide();
    QWidget::dragLeaveEvent(e);
}

void StartPage::dropEvent(QDropEvent* e)
{
    dropHint_->hide();
    QStringList open, rejected;
    for (const QUrl& url : e->mimeData()->urls())
        if (url.isLocalFile()) {
            const QString path = url.toLocalFile();
            if (drawing(path))
                open << path;
            else
                rejected << u"“%1”"_s.arg(QFileInfo(path).fileName());
        }
    e->acceptProposedAction();
    if (!open.isEmpty())
        emit openRequested(open);
    if (!rejected.isEmpty())
        toast(Severity::Error, tr("TrionCAD mở được tệp DWG, DXF, DWT. %1 không phải bản vẽ.", nullptr,
                                  int(rejected.size()))
                                   .arg(rejected.join(u", "_s)));
}

} // namespace trion::shell
