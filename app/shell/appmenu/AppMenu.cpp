#include "app/shell/appmenu/AppMenu.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/perf/UiProbe.h"
#include "app/shell/titlebar/CommandSearch.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/controls/Paint.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/tooltip/RichTooltip.h"

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QCursor>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QPainter>
#include <QScreen>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTimer>

#include <algorithm>
#include <functional>
#include <utility>

using namespace Qt::StringLiterals;

namespace trion::shell {

namespace {

using theme::IconProvider;
using theme::TextRole;
using theme::Weight;
using ui::Edition;
using ui::RichTooltip;

constexpr int kItemPad = 10;                 // .amenu .ai: padding 0 10px, gap 10px (không có token)
constexpr int kRowPadV = 5;                  // .amenu .rfi: padding 5px 8px
constexpr int kSmallIcon = 12;               // .ic.s12: ghim
constexpr int kChevron = 9;                  // mũi tên .ic.xs của mục có menu con
constexpr int kLine = 1;                     // nét viền, vạch chia (độ dày nét, không phải khoảng cách)
constexpr int kIndexRole = Qt::UserRole + 1; // mục trái: AppMenu::Item
constexpr int kCommandRole = Qt::UserRole + 2; // lệnh tìm được: tên lệnh

const theme::ThemeManager& tm()
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

enum class Sub : std::uint8_t { None, SaveAs, Export, Print, Utilities, Close };

struct ItemSpec {
    const char* label; // nhãn gốc (dịch trong ngữ cảnh AppMenu)
    const char* icon;  // IconProvider
    const char* command; // id trong ActionRegistry: chạy khi không có menu con; cần Pro thì ẩn ở hồ sơ Viewer
    Sub sub;
};
// prototype SC.appMenu: items (Mới … Đóng); 07.1 UI-A03: menu con của Lưu thành, Xuất, In, Tiện ích bản vẽ, Đóng.
constexpr std::array<ItemSpec, AppMenu::ItemCount> kItems{{
    {QT_TRANSLATE_NOOP("trion::shell::AppMenu", "Mới"), "new", "NEW", Sub::None},
    {QT_TRANSLATE_NOOP("trion::shell::AppMenu", "Mở"), "open", "OPEN", Sub::None},
    {QT_TRANSLATE_NOOP("trion::shell::AppMenu", "Lưu"), "save", "QSAVE", Sub::None},
    {QT_TRANSLATE_NOOP("trion::shell::AppMenu", "Lưu thành"), "saveas", "SAVEAS", Sub::SaveAs},
    {QT_TRANSLATE_NOOP("trion::shell::AppMenu", "Xuất"), "pdf", "", Sub::Export},
    {QT_TRANSLATE_NOOP("trion::shell::AppMenu", "In"), "print", "", Sub::Print},
    {QT_TRANSLATE_NOOP("trion::shell::AppMenu", "Tiện ích bản vẽ"), "wrench", "", Sub::Utilities},
    {QT_TRANSLATE_NOOP("trion::shell::AppMenu", "Đóng"), "fileclose", "", Sub::Close},
}};

// Phiên bản lưu được (07.1 UI-B02: DWG 2018 khuyến nghị, 2013, 2010, 2007, 2004, 2000; DXF cùng các phiên bản đó).
constexpr const char* kVersions[] = {"2018", "2013", "2010", "2007", "2004", "2000"};

const ItemSpec& spec(const QModelIndex& i)
{
    return kItems[size_t(std::clamp(i.data(kIndexRole).toInt(), 0, int(AppMenu::ItemCount) - 1))];
}

// Mục cần bản cao hơn mà đang khóa: mở trang nâng cấp thay vì chạy (cổng UI0-011, như FileTabBar).
bool gated(const QString& feature, Edition need, const QString& name)
{
    auto* gate = edition::EditionGate::instance();
    if (need == Edition::None || !gate || !gate->locked(need))
        return false;
    gate->upgrade().request({.feature = feature, .need = need, .name = name});
    return true;
}

Edition lockedNeed(Edition need)
{
    auto* gate = edition::EditionGate::instance();
    return gate && gate->locked(need) ? need : Edition::None;
}

void drawGlyph(QPainter& p, const QString& name, int size, const QRect& box, const QColor& c)
{
    QRect r(0, 0, size, size);
    r.moveCenter(box.center());
    p.drawPixmap(r, ui::paint::tinted(IconProvider::instance().pixmap(name, size, p.device()->devicePixelRatio(),
                                                                      theme::IconState::Normal),
                                      c));
}

void setInk(QWidget* w, const QColor& c)
{
    QPalette p = w->palette();
    p.setColor(QPalette::Active, QPalette::WindowText, c);
    p.setColor(QPalette::Inactive, QPalette::WindowText, c);
    w->setPalette(p);
}

// .amenu .ar .rh: --fs-xs đậm, chữ hoa, giãn chữ .05em.
QFont headingFont()
{
    QFont f = theme::font(TextRole::Caption, Weight::Bold);
    f.setCapitalization(QFont::AllUppercase);
    f.setLetterSpacing(QFont::PercentageSpacing, 105);
    return f;
}

QLabel* heading()
{
    auto* l = new QLabel;
    l->setTextFormat(Qt::PlainText);
    l->setFont(headingFont());
    return l;
}

} // namespace

// ------------------------------------------------------------------------------------------------------------- List

// Danh sách của menu: không khung, nền trong (AppMenu vẽ nền), rê có màu; không có dòng thì vẽ chữ empty.
class AppMenu::List final : public QListView {
public:
    explicit List(const QString& name)
    {
        setObjectName(name);
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setSelectionMode(NoSelection);
        setEditTriggers(NoEditTriggers);
        setVerticalScrollMode(ScrollPerPixel);
        setMouseTracking(true);
        viewport()->setAttribute(Qt::WA_Hover);
        viewport()->setAutoFillBackground(false);
    }

    QString empty;              // chữ khi không có dòng
    QPoint lastRelease;         // điểm nhả chuột gần nhất (viewport): bấm ô ghim
    std::function<void()> left; // chuột rời danh sách

protected:
    void focusInEvent(QFocusEvent* e) override
    {
        // Vào bằng bàn phím mà chưa có dòng hiện hành: dòng đầu (QAbstractItemView chỉ tự chọn khi chưa từng
        // setCurrentIndex, popupAt thì đặt lại mỗi lần mở).
        if (e->reason() != Qt::MouseFocusReason && !currentIndex().isValid() && model() && model()->rowCount() > 0)
            setCurrentIndex(model()->index(0, 0));
        QListView::focusInEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        lastRelease = e->position().toPoint();
        QListView::mouseReleaseEvent(e); // phát clicked
    }
    bool viewportEvent(QEvent* e) override
    {
        if (e->type() == QEvent::Leave && left)
            left();
        if (e->type() == QEvent::ToolTip)
            return true; // đường dẫn đã ở dòng và ảnh xem trước; không hiện tooltip thường của Qt
        return QListView::viewportEvent(e);
    }
    void paintEvent(QPaintEvent* e) override
    {
        QListView::paintEvent(e);
        if (model() && model()->rowCount() > 0)
            return;
        QPainter p(viewport());
        p.setFont(theme::font(TextRole::Label));
        p.setPen(col(tok::c_text_3));
        p.drawText(viewport()->rect().adjusted(px(tok::sp_4), kRowPadV, -px(tok::sp_4), 0),
                   Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, empty);
        if (ui::paint::focusVisible(this))
            ui::paint::drawRing(p, viewport()->rect(), col(tok::c_focus));
    }
};

// --------------------------------------------------------------------------------------------------------- Delegate

// Mục trái (.ai), lệnh tìm được và tệp gần đây (.rfi). Rê: nền; dòng hiện hành khi danh sách có focus: nền và viền
// focus (bàn phím).
class AppMenu::Delegate final : public QStyledItemDelegate {
public:
    enum class Kind : std::uint8_t { Items, Commands, Files };

    Delegate(AppMenu* owner, Kind kind, QObject* parent) : QStyledItemDelegate(parent), owner_(owner), kind_(kind) {}

    static int fileHeight()
    {
        return 2 * kRowPadV + QFontMetrics(theme::font(TextRole::Ui)).height() +
               QFontMetrics(theme::font(TextRole::Caption)).height();
    }
    // Ô ghim cuối dòng 2 của tệp (hiện khi rê; bấm: ghim/bỏ ghim).
    static QRect pinRect(const QRect& row)
    {
        const int y = row.top() + kRowPadV + QFontMetrics(theme::font(TextRole::Ui)).height();
        const int h = QFontMetrics(theme::font(TextRole::Caption)).height();
        return {row.right() - px(tok::sp_4) - kSmallIcon + 1, y + (h - kSmallIcon) / 2, kSmallIcon, kSmallIcon};
    }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex& i) const override
    {
        switch (kind_) {
        case Kind::Items: {
            int w = kItemPad + px(tok::ic) + kItemPad +
                    QFontMetrics(theme::font(TextRole::Ui)).horizontalAdvance(i.data().toString()) + kItemPad;
            if (spec(i).sub != Sub::None)
                w += kChevron + kItemPad;
            return {w, kItemHeight};
        }
        case Kind::Commands: {
            const int w = kRecentWidth - 2 * px(tok::sp_4); // cột phải trừ lề; danh sách lệnh không cuộn
            return {w, command(QRect(0, 0, w, 0), i).height};
        }
        case Kind::Files: // tên, thư mục là dữ liệu (cắt "…" khi dài); phần cố định: ngày hoặc "Không tìm thấy", ghim
            return {2 * px(tok::sp_4) + QFontMetrics(theme::font(TextRole::Caption)).horizontalAdvance(sideText(i)) +
                        px(tok::sp_4) + kSmallIcon,
                    fileHeight()};
        }
        return {};
    }

    void paint(QPainter* p, const QStyleOptionViewItem& o, const QModelIndex& i) const override
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const auto* view = qobject_cast<const QAbstractItemView*>(o.widget);
        const bool current = o.state & QStyle::State_HasFocus;
        const bool hot = (o.state & QStyle::State_MouseOver) || current;
        const QRectF box(o.rect);
        const qreal rad = tm().px(tok::r);
        if (hot && enabled(i)) {
            p->setPen(Qt::NoPen);
            p->setBrush(col(kind_ == Kind::Items ? tok::c_accent_soft : tok::c_hover)); // .ai:hover | .rfi:hover
            p->drawRoundedRect(box, rad, rad);
        }
        switch (kind_) {
        case Kind::Items:
            paintItem(*p, o.rect, i);
            break;
        case Kind::Commands:
            paintCommand(*p, o.rect, i);
            break;
        case Kind::Files:
            paintFile(*p, o.rect, i, o.state & QStyle::State_MouseOver);
            break;
        }
        if (current && view && ui::paint::focusVisible(view))
            ui::paint::drawRing(*p, box, col(tok::c_focus));
        p->restore();
    }

private:
    struct Command {
        QRect icon, label, badge, name;
        Edition need = Edition::None;
        QAction* action = nullptr;
        int height = 0;
    };

    // Dòng lệnh: biểu tượng, nhãn (xuống dòng khi dài: vi-XL không cắt), huy hiệu khóa, tên lệnh tiếng Anh bên phải.
    Command command(const QRect& r, const QModelIndex& i) const
    {
        Command c;
        const QString name = i.data(kCommandRole).toString();
        c.action = owner_->actions_.action(name);
        c.need = c.action ? RichTooltip::info(c.action).need : Edition::None;
        const QFontMetrics fu(theme::font(TextRole::Ui)), fm(theme::font(TextRole::MonoCaption));
        const int pad = px(tok::sp_4), gap = px(tok::sp_3), ic = px(tok::ic), top = r.top() + kRowPadV;
        int right = r.right() + 1 - pad - fm.horizontalAdvance(name);
        c.name = QRect(right, top, fm.horizontalAdvance(name), fu.height());
        if (c.need != Edition::None) {
            const QSize b = RichTooltip::badgeSize(c.need);
            right -= gap + b.width();
            c.badge = QRect(right, top + (fu.height() - b.height()) / 2, b.width(), b.height());
        }
        const int x = r.left() + pad + ic + gap;
        const int w = std::max(0, right - gap - x);
        const int h = fu.boundingRect(QRect(x, top, w, 1 << 16), Qt::TextWordWrap, i.data().toString()).height();
        c.icon = QRect(r.left() + pad, top + (fu.height() - ic) / 2, ic, ic);
        c.label = QRect(x, top, w, h);
        c.height = 2 * kRowPadV + std::max(h, ic);
        return c;
    }

    // Chữ bên phải dòng 1 của tệp: ngày sửa, hoặc "Không tìm thấy" (chữ, không chỉ màu).
    QString sideText(const QModelIndex& i) const
    {
        return owner_->filter_.entry(i.row()).missing ? RecentFiles::tr("Không tìm thấy")
                                                      : i.siblingAtColumn(RecentFiles::Modified).data().toString();
    }

    bool enabled(const QModelIndex& i) const
    {
        if (kind_ != Kind::Items || spec(i).sub != Sub::None)
            return true;
        const QAction* a = owner_->actions_.action(QString::fromLatin1(spec(i).command));
        return a && a->isEnabled();
    }

    void paintItem(QPainter& p, const QRect& r, const QModelIndex& i) const
    {
        const bool on = enabled(i);
        const int ic = px(tok::ic);
        drawGlyph(p, QString::fromLatin1(spec(i).icon), ic, QRect(r.left() + kItemPad, r.top(), ic, r.height()),
                  col(on ? tok::c_text_2 : tok::c_text_dis));
        const int x = r.left() + kItemPad + ic + kItemPad;
        int right = r.right() + 1 - kItemPad;
        if (spec(i).sub != Sub::None) { // mũi tên › (drawChevron xoay)
            p.save();
            p.translate(right - kChevron / 2.0, r.center().y() + 0.5);
            p.rotate(-90);
            ui::paint::drawChevron(p, QPointF(0, 0), col(tok::c_text_2));
            p.restore();
            right -= kChevron + kItemPad;
        }
        p.setFont(theme::font(TextRole::Ui));
        p.setPen(col(on ? tok::c_text : tok::c_text_dis));
        p.drawText(QRect(x, r.top(), right - x, r.height()), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                   i.data().toString());
    }

    void paintCommand(QPainter& p, const QRect& r, const QModelIndex& i) const
    {
        const Command c = command(r, i);
        if (c.action)
            c.action->icon().paint(&p, c.icon);
        p.setFont(theme::font(TextRole::Ui));
        p.setPen(col(tok::c_text));
        p.drawText(c.label, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, i.data().toString());
        if (c.need != Edition::None)
            RichTooltip::paintBadge(p, c.badge, c.need);
        p.setFont(theme::font(TextRole::MonoCaption));
        p.setPen(col(tok::c_text_3));
        p.drawText(c.name, Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine, i.data(kCommandRole).toString());
    }

    void paintFile(QPainter& p, const QRect& r, const QModelIndex& i, bool hover) const
    {
        const RecentFiles::Entry& e = owner_->filter_.entry(i.row());
        const QFont ui = theme::font(TextRole::Ui), cap = theme::font(TextRole::Caption);
        const QFontMetrics fu(ui), fc(cap);
        const int pad = px(tok::sp_4), gap = px(tok::sp_2), y1 = r.top() + kRowPadV, y2 = y1 + fu.height();
        const QString side = sideText(i);
        const int sw = fc.horizontalAdvance(side);
        p.setFont(cap);
        p.setPen(col(tok::c_text_3));
        p.drawText(QRect(r.right() + 1 - pad - sw, y1, sw, fu.height()), Qt::AlignRight | Qt::AlignVCenter, side);
        int x = r.left() + pad;
        if (e.pinned) { // prototype: ghim trước tên
            drawGlyph(p, u"pin"_s, kSmallIcon, QRect(x, y1, kSmallIcon, fu.height()), col(tok::c_text_2));
            x += kSmallIcon + gap;
        }
        const int nameW = r.right() + 1 - pad - sw - pad - x;
        p.setFont(ui);
        p.setPen(col(e.missing ? tok::c_text_3 : tok::c_text));
        p.drawText(QRect(x, y1, nameW, fu.height()), Qt::AlignLeft | Qt::AlignVCenter,
                   fu.elidedText(e.name, Qt::ElideRight, nameW));
        const QRect pin = pinRect(r);
        const int pathW = (hover ? pin.left() - gap : r.right() + 1 - pad) - (r.left() + pad);
        p.setFont(cap);
        p.setPen(col(tok::c_text_3));
        p.drawText(QRect(r.left() + pad, y2, pathW, fc.height()), Qt::AlignLeft | Qt::AlignVCenter,
                   fc.elidedText(i.data(RecentFiles::FolderRole).toString(), Qt::ElideRight, pathW));
        if (hover) // ô ghim/bỏ ghim (đậm khi đã ghim)
            drawGlyph(p, u"pin"_s, kSmallIcon, pin, col(e.pinned ? tok::c_text : tok::c_text_3));
    }

    AppMenu* owner_;
    Kind kind_;
};

// ---------------------------------------------------------------------------------------------------------- Preview

// Ảnh xem trước của tệp đang rê, cạnh menu: ảnh (ThumbnailQueue; đang tạo: ô --c-canvas; lỗi: biểu tượng tệp), tên,
// "ngày · cỡ · phiên bản" hoặc "Không tìm thấy". Không nhận chuột, không lấy focus.
class AppMenu::Preview final : public QWidget {
public:
    explicit Preview(AppMenu* owner) : QWidget(owner, Qt::ToolTip | Qt::FramelessWindowHint), owner_(owner)
    {
        setObjectName(u"appMenuPreview"_s);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
    }

    QString path, name, meta;
    bool missing = false;

    static QSize image() { return {kPreviewWidth, kPreviewWidth * 10 / 16}; }
    QSize pixels() const { return image() * devicePixelRatioF(); } // cỡ xin ThumbnailQueue

    QSize sizeHint() const override
    {
        const int pad = px(tok::sp_3);
        return {kPreviewWidth + 2 * pad, 2 * pad + image().height() + px(tok::sp_2) +
                                             QFontMetrics(theme::font(TextRole::Ui, Weight::Bold)).height() +
                                             QFontMetrics(theme::font(TextRole::Caption)).height()};
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), col(tok::c_raised));
        p.setPen(col(tok::c_border_ctl));
        p.drawRect(rect().adjusted(0, 0, -1, -1));
        const int pad = px(tok::sp_3);
        const QRect slot(QPoint(pad, pad), image());
        if (const QImage* shot = missing ? nullptr : owner_->thumbs_.image(path, pixels())) {
            QRect fit(QPoint(), shot->size() / devicePixelRatioF()); // ảnh đã thu giữ tỉ lệ ở việc nền
            fit.moveCenter(slot.center());
            p.drawImage(fit, *shot);
        } else {
            p.fillRect(slot, col(tok::c_canvas));
            if (missing || owner_->thumbs_.failed(path))
                drawGlyph(p, u"file"_s, px(tok::ic_lg), slot, col(tok::c_text_3));
        }
        const QFont bold = theme::font(TextRole::Ui, Weight::Bold), cap = theme::font(TextRole::Caption);
        const QFontMetrics fb(bold), fc(cap);
        const int y = slot.bottom() + 1 + px(tok::sp_2), w = slot.width();
        p.setFont(bold);
        p.setPen(col(tok::c_text));
        p.drawText(QRect(pad, y, w, fb.height()), Qt::AlignLeft | Qt::AlignVCenter,
                   fb.elidedText(name, Qt::ElideRight, w));
        p.setFont(cap);
        p.setPen(col(tok::c_text_3));
        p.drawText(QRect(pad, y + fb.height(), w, fc.height()), Qt::AlignLeft | Qt::AlignVCenter,
                   fc.elidedText(missing ? RecentFiles::tr("Không tìm thấy") : meta, Qt::ElideRight, w));
    }

private:
    AppMenu* owner_;
};

// ---------------------------------------------------------------------------------------------------------- AppMenu

AppMenu::AppMenu(actions::ActionRegistry& actions, RecentFiles& recent, const CommandSearch& commands, QWidget* parent)
    : QFrame(parent, Qt::Popup), actions_(actions), recent_(recent), commands_(commands), filter_(recent)
{
    setObjectName(u"appMenu"_s);
    build();
    retranslate();
    relayout();
}

AppMenu::~AppMenu()
{
    // Danh sách dùng filter_ (model), delegate đọc thành viên, sự kiện lúc hủy (Leave) gọi lại menu: hủy trước các
    // thành viên.
    delete std::exchange(body_, nullptr);
}

QAbstractItemView* AppMenu::items() const
{
    return items_;
}
QAbstractItemView* AppMenu::commands() const
{
    return cmds_;
}
QAbstractItemView* AppMenu::files() const
{
    return files_;
}
QWidget* AppMenu::preview() const
{
    return preview_;
}

void AppMenu::build()
{
    const int s1 = px(tok::sp_1), s2 = px(tok::sp_2), s3 = px(tok::sp_3), s4 = px(tok::sp_4);
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(kLine, kLine, kLine, kLine); // viền (paintEvent)
    column->setSpacing(0);

    auto* head = new QHBoxLayout; // .ah: padding --sp-4, vạch dưới
    head->setContentsMargins(s4, s4, s4, s4);
    search_ = new ui::SearchField;
    search_->setObjectName(u"appMenuSearch"_s);
    head->addWidget(search_);
    column->addLayout(head);

    body_ = new QWidget;
    auto* row = new QHBoxLayout(body_);
    row->setContentsMargins(0, kLine, 0, kLine); // vạch trên, dưới
    row->setSpacing(kLine);                      // vạch giữa hai cột
    left_ = new QWidget;
    left_->setFixedWidth(kItemsWidth);
    auto* lc = new QVBoxLayout(left_); // .al: padding --sp-3
    lc->setContentsMargins(s3, s3, s3, s3);
    items_ = new List(u"appMenuItems"_s);
    lc->addWidget(items_);
    right_ = new QWidget;
    right_->setFixedWidth(kRecentWidth);
    auto* rc = new QVBoxLayout(right_); // .ar: padding --sp-4, gap 2 px
    rc->setContentsMargins(s4, s4, s4, s4);
    rc->setSpacing(s1);
    cmdTitle_ = heading();
    cmdTitle_->setContentsMargins(s2, s1, s2, s3); // .rh: padding 2px 4px 6px
    cmds_ = new List(u"appMenuCommands"_s);
    cmds_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* fileHead = new QHBoxLayout;
    fileTitle_ = heading();
    fileTitle_->setContentsMargins(s2, s1, s2, s3);
    sort_ = new ui::DropDown;
    sort_->setObjectName(u"appMenuSort"_s);
    fileHead->addWidget(fileTitle_, 1);
    fileHead->addWidget(sort_);
    files_ = new List(u"appMenuFiles"_s);
    files_->setContextMenuPolicy(Qt::CustomContextMenu);
    rc->addWidget(cmdTitle_);
    rc->addWidget(cmds_);
    rc->addLayout(fileHead);
    rc->addWidget(files_, 1); // phần còn lại của cột: phần Lệnh (khi gõ) lấy chỗ của tệp, menu không đổi cỡ
    row->addWidget(left_);
    row->addWidget(right_);
    column->addWidget(body_);

    footer_ = new QWidget; // .af: nút bên phải, padding --sp-4, gap 6 px
    auto* foot = new QHBoxLayout(footer_);
    foot->setContentsMargins(s4, s4, s4, s4);
    foot->setSpacing(s3);
    foot->addStretch(1);
    const auto button = [&](const QString& id, const QString& icon) {
        auto* b = new ui::Button;
        b->setObjectName(id);
        b->setControlSize(ui::Button::Size::Small);
        b->setIconName(icon);
        foot->addWidget(b);
        if (QAction* a = actions_.action(id)) { // chữ, tooltip theo QAction của registry (dịch lại khi đổi ngôn ngữ)
            const auto sync = [b, a] {
                b->setText(a->text());
                ui::TipInfo tip = RichTooltip::info(a);
                tip.name = a->text();
                RichTooltip::attach(b, tip);
            };
            sync();
            connect(a, &QAction::changed, b, sync);
            connect(b, &QAbstractButton::clicked, this, [this, a] { run(a); });
        }
        return b;
    };
    options_ = button(u"OPTIONS"_s, u"gear"_s);
    quit_ = button(u"QUIT"_s, u"exit"_s);
    column->addWidget(footer_);

    itemModel_ = new QStandardItemModel(this);
    cmdModel_ = new QStandardItemModel(this);
    items_->setModel(itemModel_);
    cmds_->setModel(cmdModel_);
    files_->setModel(&filter_);
    items_->setItemDelegate(new Delegate(this, Delegate::Kind::Items, items_));
    cmds_->setItemDelegate(new Delegate(this, Delegate::Kind::Commands, cmds_));
    files_->setItemDelegate(new Delegate(this, Delegate::Kind::Files, files_));
    for (List* v : {items_, cmds_, files_}) {
        connect(v, &QAbstractItemView::clicked, this, [this, v](const QModelIndex& i) { activate(v, i, false); });
        connect(v, &QAbstractItemView::entered, this, [this, v](const QModelIndex& i) { hover(v, i); });
        connect(v, &QAbstractItemView::viewportEntered, this, &AppMenu::unhover);
        v->left = [this] { unhover(); };
        v->installEventFilter(this);
    }
    search_->installEventFilter(this);
    connect(search_, &QLineEdit::textEdited, this, &AppMenu::refreshResults);
    connect(sort_, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (i >= 0)
            filter_.setSortKey(RecentFilter::Sort(i));
    });
    connect(files_, &QWidget::customContextMenuRequested, this, &AppMenu::showContextMenu);
    setTabOrder(search_, items_);
    setTabOrder(items_, cmds_);
    setTabOrder(cmds_, sort_);
    setTabOrder(sort_, files_);
    setTabOrder(files_, options_);
    setTabOrder(options_, quit_);

    for (int i = 0; i < ItemCount; ++i) {
        if (kItems[size_t(i)].sub == Sub::None)
            continue;
        auto* m = sub_[size_t(i)] = new ui::Menu(this);
        m->setObjectName(u"appMenuSub%1"_s.arg(i));
        connect(m, &QMenu::aboutToShow, this, [this, i] { fill(Item(i)); });
        m->installEventFilter(this);
        fill(Item(i)); // TruncationProbe, test đọc được cả khi chưa mở
    }
    context_ = new ui::Menu(this);
    context_->setObjectName(u"appMenuContext"_s);
    context_->installEventFilter(this);
    preview_ = new Preview(this);

    connect(&thumbs_, &ThumbnailQueue::ready, this, [this](const QString& path) {
        if (preview_->isVisible() && path == preview_->path)
            preview_->update();
    });
    connect(&recent_, &RecentFiles::fileChanged, &thumbs_, &ThumbnailQueue::forget);
    connect(&theme::ThemeManager::instance(), &theme::ThemeManager::changed, this, &AppMenu::relayout);
    if (auto* gate = edition::EditionGate::instance()) // EditionGate tạo trước menu (main)
        connect(gate, &edition::EditionGate::profileChanged, this, &AppMenu::rebuildItems);
}

void AppMenu::retranslate()
{
    setAccessibleName(tr("Menu ứng dụng"));
    search_->setAccessibleName(tr("Tìm trong menu ứng dụng"));
    search_->setPlaceholderText(tr("Tìm lệnh hoặc tệp gần đây"));
    items_->setAccessibleName(tr("Menu ứng dụng"));
    cmdTitle_->setText(tr("Lệnh"));
    cmds_->setAccessibleName(cmdTitle_->text());
    fileTitle_->setText(tr("Tệp gần đây"));
    files_->setAccessibleName(fileTitle_->text());
    {
        const QSignalBlocker block(sort_);
        const int sort = int(filter_.sortKey()); // RecentFilter::Sort: Date, Name, Size
        sort_->clear();
        sort_->addItems({tr("Ngày"), tr("Tên"), tr("Kích thước")});
        sort_->setCurrentIndex(sort);
    }
    sort_->setAccessibleName(tr("Sắp xếp tệp gần đây"));
    rebuildItems();
    refreshResults();
}

void AppMenu::relayout()
{
    for (QLabel* h : {cmdTitle_, fileTitle_}) {
        h->setFont(headingFont());
        setInk(h, col(tok::c_text_3));
    }
    // Cột phải cao đủ kVisibleFiles tệp; cỡ menu không đổi khi gõ (không vẽ lại cả cửa sổ mỗi phím).
    const int fileH = Delegate::fileHeight(), s1 = px(tok::sp_1), s4 = px(tok::sp_4);
    const int head = std::max(fileTitle_->sizeHint().height(), sort_->sizeHint().height());
    right_->setFixedHeight(2 * s4 + head + s1 + kVisibleFiles * fileH);
    files_->setMinimumHeight(2 * fileH);
    for (List* v : {items_, cmds_, files_})
        v->doItemsLayout();
    refreshResults(); // chiều cao danh sách lệnh theo cỡ chữ mới
    if (isVisible())
        adjustSize();
    update();
}

void AppMenu::rebuildItems()
{
    const auto* gate = edition::EditionGate::instance();
    const bool viewer = gate && gate->profile() == edition::UiProfile::Viewer;
    itemModel_->clear();
    for (int i = 0; i < ItemCount; ++i) {
        const ItemSpec& s = kItems[size_t(i)];
        // 07.1 §2: Viewer, Viewer Pro không sửa, không lưu bản vẽ → bản rút gọn bỏ mục cần Pro.
        if (viewer && *s.command && actions_.need(QString::fromLatin1(s.command)) == Edition::Pro)
            continue;
        auto* row = new QStandardItem(tr(s.label));
        row->setData(i, kIndexRole);
        row->setEditable(false);
        itemModel_->appendRow(row);
    }
    items_->setMinimumHeight(itemModel_->rowCount() * kItemHeight); // cột trái không cuộn
}

void AppMenu::fill(Item item)
{
    ui::Menu* m = sub_[item];
    m->clear();
    // Mục chạy QAction của registry: QAction riêng của menu mang chữ, biểu tượng, phím, khóa của nó lúc mở.
    const auto command = [this, m](const char* id) {
        QAction* target = actions_.action(QString::fromLatin1(id));
        if (!target)
            return; // danh mục lệch registry: test bảng đối chiếu bắt
        QAction* a = m->addAction(target->icon(), target->text());
        a->setObjectName(QString::fromLatin1(id));
        a->setEnabled(target->isEnabled());
        RichTooltip::attach(a, RichTooltip::info(target)); // lệnh, phím, khóa (EditionGate::bind)
        connect(a, &QAction::triggered, this, [this, m, target] {
            m->hide();
            close(); // trước khi chạy: hộp thoại của lệnh không mở dưới popup
            target->trigger();
        });
    };
    // Mục không có QAction trong registry: chuỗi lệnh qua macroRequested; feature là lệnh (cờ phiên bản theo registry).
    const auto macro = [this, m](const QString& text, const QString& feature, const QString& chain) {
        const Edition need = actions_.need(feature);
        QAction* a = m->addAction(text);
        a->setObjectName(feature);
        a->setData(chain);
        RichTooltip::attach(a, {.command = feature, .need = lockedNeed(need)});
        connect(a, &QAction::triggered, this, [this, m, text, feature, chain, need] {
            m->hide();
            close();
            if (!gated(feature, need, text))
                emit macroRequested(chain);
        });
    };
    switch (kItems[item].sub) {
    case Sub::SaveAs: { // 07.1 UI-B02: hộp thoại Lưu thành có sẵn định dạng chọn ở đây
        // ponytail: cú pháp đề xuất (định dạng như prompt SAVEAS khi FILEDIA=0, DXF thêm phiên bản, "~" mở hộp thoại
        // tệp); CMD-244 chốt trong SAVEAS.md thì sửa bảng này và bảng đối chiếu của test.
        command("SAVEAS");
        m->addSeparator();
        for (const char* v : kVersions) {
            const QString dwg = u"DWG %1"_s.arg(QLatin1StringView(v));
            macro(v == kVersions[0] ? tr("%1 (khuyến nghị)").arg(dwg) : dwg, u"SAVEAS"_s,
                  u"^C^C_SAVEAS _%1 ~"_s.arg(QLatin1StringView(v)));
        }
        m->addSeparator();
        for (const char* v : kVersions)
            macro(u"DXF %1"_s.arg(QLatin1StringView(v)), u"SAVEAS"_s,
                  u"^C^C_SAVEAS _DXF _%1 ~"_s.arg(QLatin1StringView(v)));
        m->addSeparator();
        macro(tr("Mẫu bản vẽ (DWT)"), u"SAVEAS"_s, u"^C^C_SAVEAS _Template ~"_s);
        break;
    }
    case Sub::Export: // prototype: PDF, Ảnh (PNG, JPG, TIF, BMP), DXF
        command("EXPORTPDF");
        command("PNGOUT");
        macro(tr("Xuất DXF"), u"SAVEAS"_s, u"^C^C_SAVEAS _DXF _2018 ~"_s);
        break;
    case Sub::Print: // prototype: In, In hàng loạt theo khung tên, In nhiều layout, Thiết lập trang, Xem trước bản in
        command("PLOT");
        command("TCBATCHPLOT");
        command("PUBLISH");
        command("PAGESETUP");
        command("PREVIEW");
        break;
    case Sub::Utilities: // 07.1: Kiểm tra lỗi, Phục hồi, Dọn dẹp, Thuộc tính bản vẽ, Đơn vị
        command("AUDIT");
        command("RECOVER");
        command("PURGE");
        command("DWGPROPS");
        command("UNITS");
        break;
    case Sub::Close: // 07.1: bản vẽ này / tất cả
        command("CLOSE");
        macro(tr("Đóng tất cả bản vẽ"), u"CLOSEALL"_s, u"^C^C_CLOSEALL"_s);
        break;
    case Sub::None:
        break;
    }
}

void AppMenu::refreshResults()
{
    const QString q = search_->text();
    filter_.setQuery(q);
    cmdModel_->clear();
    if (!q.trimmed().isEmpty()) {
        // Chỉ mục của ô Tìm lệnh (viết tắt, từ khóa tiếng Việt), cùng xếp hạng.
        for (const QString& name : commands_.search(q).mid(0, kMaxCommands)) {
            const QAction* a = actions_.action(name);
            auto* row = new QStandardItem(a->text());
            row->setData(name, kCommandRole);
            row->setEditable(false);
            if (const Edition need = RichTooltip::info(a).need; need != Edition::None)
                row->setData(edition::EditionGate::availableIn(need), Qt::AccessibleDescriptionRole);
            cmdModel_->appendRow(row);
        }
    }
    const bool any = cmdModel_->rowCount() > 0;
    int h = 0;
    for (int r = 0; r < cmdModel_->rowCount(); ++r)
        h += cmds_->sizeHintForRow(r);
    cmdTitle_->setVisible(any);
    cmds_->setVisible(any);
    cmds_->setFixedHeight(h);
    if (const QString empty = recent_.size() == 0 ? tr("Chưa có tệp gần đây.") : tr("Không có tệp nào khớp.");
        empty != files_->empty) {
        files_->empty = empty;
        files_->viewport()->update();
    }
    if (isVisible())
        ui::probe::untilFrame(ui::probe::Op::Autocomplete, this); // DoD-L: kết quả cập nhật sau mỗi phím
}

void AppMenu::popupAt(const QPoint& globalPos)
{
    if (isVisible()) { // bấm lại nút logo (bằng phím): đóng như prototype
        close();
        return;
    }
    search_->clear();
    refreshResults();
    for (List* v : {items_, cmds_, files_})
        v->setCurrentIndex({});
    files_->scrollToTop();
    recent_.refresh(); // tệp đã xóa, đổi tên từ lần trước: "Không tìm thấy" (kiểm ở nền)
    adjustSize();
    const QScreen* screen = QGuiApplication::screenAt(globalPos);
    const QRect avail = (screen ? screen : this->screen())->availableGeometry();
    QPoint at(globalPos.x(), globalPos.y() + px(tok::sp_1)); // prototype: top = đáy nút + 2
    at.setX(std::clamp(at.x(), avail.left(), std::max(avail.left(), avail.right() + 1 - width())));
    at.setY(std::clamp(at.y(), avail.top(), std::max(avail.top(), avail.bottom() + 1 - height())));
    move(at);
    show();
    search_->setFocus(Qt::PopupFocusReason);
    ui::probe::untilFrame(ui::probe::Op::MenuOpen, this);
}

void AppMenu::run(QAction* action)
{
    if (!action || !action->isEnabled())
        return;
    close();           // focus về chỗ cũ trước khi lệnh chạy
    action->trigger(); // ActionRegistry: cổng phiên bản, rồi chuỗi lệnh tới INT-002
}

void AppMenu::openFile(const QString& path)
{
    close();
    emit openRequested({path});
}

void AppMenu::runFirst()
{
    if (search_->text().trimmed().isEmpty())
        return;
    if (cmdModel_->rowCount() > 0)
        activate(cmds_, cmdModel_->index(0, 0), true);
    else if (filter_.rowCount() > 0)
        activate(files_, filter_.index(0, 0), true);
}

void AppMenu::activate(List* view, const QModelIndex& index, bool keyboard)
{
    if (!index.isValid())
        return;
    if (view == items_) {
        const ItemSpec& s = spec(index);
        if (s.sub != Sub::None)
            openSubmenu(index.row(), keyboard);
        else
            run(actions_.action(QString::fromLatin1(s.command)));
    } else if (view == cmds_) {
        run(actions_.action(index.data(kCommandRole).toString()));
    } else {
        const RecentFiles::Entry& e = filter_.entry(index.row());
        const QRect pin = Delegate::pinRect(files_->visualRect(index))
                              .adjusted(-px(tok::sp_1), -px(tok::sp_1), px(tok::sp_1), px(tok::sp_1));
        if (!keyboard && pin.contains(files_->lastRelease))
            recent_.setPinned(e.path, !e.pinned); // RecentFilter đưa lên/xuống nhóm ghim
        else
            openFile(e.path);
    }
}

void AppMenu::openSubmenu(int row, bool keyboard)
{
    const QModelIndex index = itemModel_->index(row, 0);
    ui::Menu* m = sub_[size_t(index.data(kIndexRole).toInt())];
    if (!m)
        return;
    unhover();
    items_->setCurrentIndex(index);
    const QRect r = items_->visualRect(index);
    m->popup(items_->viewport()->mapToGlobal(QPoint(r.right() + 1, r.top()))); // prototype: TC.menu(br.right, br.top)
    if (keyboard && !m->actions().isEmpty())
        m->setActiveAction(m->actions().constFirst());
}

void AppMenu::showContextMenu(const QPoint& viewportPos)
{
    const QModelIndex i = files_->indexAt(viewportPos);
    if (!i.isValid())
        return;
    const RecentFiles::Entry& e = filter_.entry(i.row());
    context_->clear();
    connect(context_->addAction(IconProvider::icon(u"open"_s), tr("Mở")), &QAction::triggered, this,
            [this, path = e.path] { openFile(path); });
    QAction* pin = context_->addAction(IconProvider::icon(u"pin"_s),
                                       e.pinned ? RecentFiles::tr("Bỏ ghim") : RecentFiles::tr("Ghim"));
    connect(pin, &QAction::triggered, this, [this, path = e.path, on = !e.pinned] { recent_.setPinned(path, on); });
    unhover();
    context_->popup(files_->viewport()->mapToGlobal(viewportPos));
}

void AppMenu::hover(List* view, const QModelIndex& index)
{
    auto* tips = RichTooltip::instance();
    if (view == files_) { // ảnh xem trước thay tooltip
        if (tips)
            tips->leave();
        const RecentFiles::Entry& e = filter_.entry(index.row());
        preview_->path = e.path;
        preview_->name = e.name;
        preview_->meta = index.data(RecentFiles::MetaRole).toString();
        preview_->missing = e.missing;
        const QSize size = preview_->sizeHint();
        preview_->resize(size);
        const QRect avail = screen()->availableGeometry(), menu = geometry();
        const int top = files_->viewport()->mapToGlobal(files_->visualRect(index).topLeft()).y();
        int x = menu.right() + 1 + px(tok::sp_2); // bên phải menu; hết chỗ thì bên trái, vẫn hết thì đè lên menu
        if (x + size.width() > avail.right() + 1)
            x = menu.left() - px(tok::sp_2) - size.width();
        x = std::clamp(x, avail.left(), std::max(avail.left(), avail.right() + 1 - size.width()));
        preview_->move(x, std::clamp(top, avail.top(), std::max(avail.top(), avail.bottom() + 1 - size.height())));
        preview_->show();
        preview_->update();
        if (!e.missing && !thumbs_.image(e.path, preview_->pixels()) && !thumbs_.failed(e.path))
            thumbs_.request({e.path}, preview_->pixels()); // xong: ready → vẽ lại
        return;
    }
    preview_->hide();
    if (!tips)
        return;
    QAction* a = nullptr;
    if (view == cmds_)
        a = actions_.action(index.data(kCommandRole).toString());
    else if (spec(index).sub == Sub::None)
        a = actions_.action(QString::fromLatin1(spec(index).command));
    if (!a) {
        tips->leave();
        return;
    }
    const QRect r = view->visualRect(index);
    tips->hover(a, QRect(view->viewport()->mapToGlobal(r.topLeft()), r.size()), QCursor::pos());
}

void AppMenu::unhover()
{
    if (auto* tips = RichTooltip::instance())
        tips->leave();
    preview_->hide();
}

void AppMenu::focusRow(List* view, int row)
{
    if (row < 0 || row >= view->model()->rowCount())
        return;
    view->setCurrentIndex(view->model()->index(row, 0));
    view->setFocus(Qt::ShortcutFocusReason); // từ bàn phím: viền focus
}

void AppMenu::focusRight()
{
    for (List* v : {cmds_, files_})
        if (v->isVisible() && v->model()->rowCount() > 0)
            return focusRow(v, std::max(0, v->currentIndex().row()));
    sort_->setFocus(Qt::ShortcutFocusReason); // không có kết quả: ô sắp xếp vẫn ở cột phải
}

bool AppMenu::searchKey(QKeyEvent* k)
{
    switch (k->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        runFirst();
        return true;
    case Qt::Key_Down:
        if (search_->text().trimmed().isEmpty())
            focusRow(items_, 0);
        else
            focusRight();
        return true;
    default:
        return false;
    }
}

bool AppMenu::listKey(List* view, QKeyEvent* k)
{
    const int row = view->currentIndex().row(), last = view->model()->rowCount() - 1;
    switch (k->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        activate(view, view->currentIndex(), true);
        return true;
    case Qt::Key_Up:
        if (row > 0)
            return false; // QListView chuyển dòng
        if (view == files_ && cmds_->isVisible())
            focusRow(cmds_, cmdModel_->rowCount() - 1);
        else
            search_->setFocus(Qt::ShortcutFocusReason);
        return true;
    case Qt::Key_Down:
        if (row < last)
            return false;
        if (view == cmds_)
            focusRow(files_, 0);
        return true; // dòng cuối: đứng yên
    case Qt::Key_Right:
        if (view == items_) {
            if (row >= 0 && spec(view->currentIndex()).sub != Sub::None)
                openSubmenu(row, true);
            else
                focusRight();
        }
        return true;
    case Qt::Key_Left:
        if (view != items_)
            focusRow(items_, std::max(0, items_->currentIndex().row()));
        return true;
    default:
        return false;
    }
}

bool AppMenu::eventFilter(QObject* watched, QEvent* e)
{
    if (auto* m = qobject_cast<QMenu*>(watched)) { // menu con, menu chuột phải
        if (e->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(e)->key() == Qt::Key_Left) {
            m->hide(); // ← về mục của menu ứng dụng
            return true;
        }
        if (e->type() == QEvent::MouseButtonPress) {
            const auto* me = static_cast<QMouseEvent*>(e);
            if (!m->rect().contains(me->position().toPoint()) && !geometry().contains(me->globalPosition().toPoint())) {
                // Bấm ra ngoài cả hai: đóng cả menu ứng dụng, nuốt cú bấm như mousePressEvent.
                m->setAttribute(Qt::WA_NoMouseReplay);
                setAttribute(Qt::WA_NoMouseReplay);
                QTimer::singleShot(0, this, &QWidget::close);
            }
        }
        return false;
    }
    if (e->type() != QEvent::KeyPress)
        return QFrame::eventFilter(watched, e);
    auto* k = static_cast<QKeyEvent*>(e);
    if (k->key() == Qt::Key_Escape) { // cả khi ô tìm còn chữ (SearchField tự xóa chữ): Esc đóng menu
        close();
        return true;
    }
    if (watched == search_)
        return searchKey(k);
    for (List* v : {items_, cmds_, files_})
        if (watched == v)
            return listKey(v, k);
    return QFrame::eventFilter(watched, e);
}

bool AppMenu::event(QEvent* e)
{
    if (e->type() == QEvent::ShortcutOverride && static_cast<QKeyEvent*>(e)->key() == Qt::Key_Escape) {
        e->accept();
        return true;
    }
    return QFrame::event(e);
}

void AppMenu::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape)
        close();
    else
        QFrame::keyPressEvent(e);
}

void AppMenu::mousePressEvent(QMouseEvent* e)
{
    // Bấm ra ngoài: đóng và nuốt cú bấm (như menu của Windows), nên bấm lại nút logo không mở lại menu.
    if (!rect().contains(e->position().toPoint()))
        setAttribute(Qt::WA_NoMouseReplay);
    QFrame::mousePressEvent(e);
}

void AppMenu::paintEvent(QPaintEvent*)
{
    // .amenu: nền --c-raised, viền --c-border-ctl; vạch --c-border dưới ô tìm, giữa hai cột, trên hai nút.
    // ponytail: chưa bo góc dưới --r-lg, chưa bóng --sh-dlg (cần cửa sổ trong suốt), như popup của ô Tìm lệnh.
    QPainter p(this);
    p.fillRect(rect(), col(tok::c_raised));
    p.setPen(col(tok::c_border));
    const QRect b = body_->geometry();
    p.drawLine(1, b.top(), width() - 2, b.top());
    p.drawLine(1, b.bottom(), width() - 2, b.bottom());
    const int x = b.left() + left_->geometry().right() + 1;
    p.drawLine(x, b.top(), x, b.bottom());
    p.setPen(col(tok::c_border_ctl));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

void AppMenu::hideEvent(QHideEvent* e)
{
    QFrame::hideEvent(e);
    thumbs_.cancelAll();
    unhover();
    for (ui::Menu* m : sub_)
        if (m && m->isVisible())
            m->hide();
}

void AppMenu::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QFrame::changeEvent(e);
}

} // namespace trion::shell
