#include "app/shell/filetabs/FileTabBar.h"

#include "app/core/UiExecutor.h"
#include "app/core/edition/EditionGate.h"
#include "app/theme/ThemeManager.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/tooltip/RichTooltip.h"

#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHoverEvent>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QUrl>
#include <QtMath>

#include <algorithm>
#include <utility>
#include <vector>

using namespace Qt::StringLiterals;

namespace trion::shell {

using theme::IconProvider;
using theme::IconState;
using theme::TextRole;
using ui::Button;
using ui::Edition;
using ui::Menu;
using ui::RichTooltip;
using ui::Severity;

namespace {

constexpr int kSmallIcon = 12; // .ic.s12: home của Start, ổ khóa .ic.ro, × của tab
constexpr int kClose = 18;     // .ft-tab .x
constexpr int kDot = 6;        // .ft-dot

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

// Nền vùng vẽ, layout không đổi theo chủ đề giao diện (07.1 §13.2): giá trị :root.
QColor canvasCol(tok::ColorId id)
{
    return theme::toQColor(tok::color(id, tok::Mode{}));
}

// Biểu tượng của bộ tô màu bất kỳ (--c-text-3 của ổ khóa, × không là trạng thái của IconProvider), giữa box.
void drawGlyph(QPainter& p, const QString& name, const QRect& box, const QColor& color, qreal dpr)
{
    QPixmap pm = IconProvider::instance().pixmap(name, kSmallIcon, dpr, IconState::Normal);
    if (pm.isNull())
        return;
    {
        QPainter tint(&pm);
        tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
        tint.fillRect(QRect(QPoint(), pm.size()), color);
    }
    QRect r(0, 0, kSmallIcon, kSmallIcon);
    r.moveCenter(box.center());
    p.drawPixmap(r, pm);
}

void toast(Severity severity, const QString& text)
{
    if (auto* host = ui::ToastHost::instance())
        host->post(severity, text);
}

// Mục cần bản cao hơn mà đang khóa: mở trang nâng cấp thay vì chạy (cổng UI0-011).
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

} // namespace

// Ô ảnh của popup (.ft-thumb button): ảnh kThumbSize trên chữ --fs-xs; rê nền --c-accent-soft. Ảnh xong là icon của
// nút; chưa có: ô giữ chỗ màu nền vùng vẽ (Model) hoặc layout.
class FileTabBar::Cell final : public QAbstractButton {
public:
    Cell(const QString& layout, const QString& label, QWidget* parent) : QAbstractButton(parent), layout(layout)
    {
        setText(label);
        setAccessibleName(label);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover);
    }

    const QString layout;

    QSize sizeHint() const override
    {
        const QFontMetrics fm(theme::font(TextRole::Caption));
        const int pad = px(tok::sp_2);
        return {FileTabBar::kThumbSize.width() + 2 * pad,
                FileTabBar::kThumbSize.height() + 2 * pad + px(tok::sp_2) + fm.height()};
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal r = tm().px(tok::r);
        const bool hot = underMouse();
        if (hot) {
            p.setPen(Qt::NoPen);
            p.setBrush(col(tok::c_accent_soft));
            p.drawRoundedRect(QRectF(rect()), r, r);
        }
        const int pad = px(tok::sp_2);
        const QRect image(QPoint(pad, pad), FileTabBar::kThumbSize);
        if (icon().isNull()) {
            p.setPen(Qt::NoPen);
            p.setBrush(canvasCol(layout == FileTabBar::kModel ? tok::c_canvas : tok::c_layout_bg));
            p.drawRoundedRect(QRectF(image), r, r);
        } else {
            p.drawPixmap(image, icon().pixmap(FileTabBar::kThumbSize, devicePixelRatioF()));
        }
        const QFont f = theme::font(TextRole::Caption);
        p.setFont(f);
        p.setPen(col(hot ? tok::c_text : tok::c_text_2));
        const QRect label(pad, image.bottom() + 1 + pad, width() - 2 * pad, QFontMetrics(f).height());
        p.drawText(label, Qt::AlignHCenter | Qt::AlignTop,
                   QFontMetrics(f).elidedText(text(), Qt::ElideRight, label.width()));
    }
};

// ------------------------------------------------------------------------------------------------- Tabs

// QTabBar giữ chọn tab, ←→ (trừ phần tự xử lý dưới đây), tên trợ năng từng tab; tab do lớp này đo và vẽ theo .ft-tab.
// Không dùng setMovable của Qt: ảnh tab bay theo chuột do style vẽ (sai kiểu tab), và tab Start phải đứng yên.
class FileTabBar::Tabs final : public QTabBar {
public:
    explicit Tabs(FileTabBar* owner) : QTabBar(owner), owner_(owner)
    {
        setDrawBase(false);
        setExpanding(false);
        setUsesScrollButtons(false);
        setElideMode(Qt::ElideNone); // tab không co: tab không vừa thì ẩn (fit)
        setDocumentMode(true);
        setAttribute(Qt::WA_Hover);
        setMouseTracking(true);
    }

    // Phần tử của tab (.ft-tab: padding 0 6px 0 10px, cách nhau 7px ~ --sp-3), cao --h-ftab - 4px sát đáy dải.
    struct Parts {
        QRect shape, icon, text, dot, close; // rỗng: không có
    };

    Parts parts(int index) const
    {
        Parts t;
        const QRect r = tabRect(index);
        if (r.isEmpty())
            return t;
        const int h = px(tok::h_ftab) - px(tok::sp_2), gap = px(tok::sp_3);
        t.shape = QRect(r.left(), r.bottom() + 1 - h, r.width(), h);
        const int cy = t.shape.center().y();
        int x = t.shape.left() + px(tok::sp_4) + px(tok::sp_1), right = t.shape.right() + 1 - px(tok::sp_3);
        if (!owner_->iconName(index).isEmpty()) {
            t.icon = QRect(x, cy - kSmallIcon / 2, kSmallIcon, kSmallIcon);
            x += kSmallIcon + gap;
        }
        const quint64 id = owner_->idAt(index);
        if (id != kStart) {
            t.close = QRect(right - kClose, cy - kClose / 2, kClose, kClose);
            right -= kClose + gap;
            if (owner_->docs_.value(id).dirty) {
                t.dot = QRect(right - kDot, cy - kDot / 2, kDot, kDot);
                right -= kDot + gap;
            }
        }
        t.text = QRect(x, t.shape.top(), std::max(0, right - x), h);
        return t;
    }

    QSize tabSizeHint(int index) const override
    {
        const int gap = px(tok::sp_3);
        const qreal text = QFontMetricsF(theme::font(TextRole::Ui)).horizontalAdvance(owner_->label(index));
        int w = px(tok::sp_4) + px(tok::sp_1) + qCeil(text) + px(tok::sp_3);
        if (!owner_->iconName(index).isEmpty())
            w += kSmallIcon + gap;
        if (const quint64 id = owner_->idAt(index); id != kStart)
            w += gap + kClose + (owner_->docs_.value(id).dirty ? gap + kDot : 0);
        return {std::min(w, kMaxTabWidth), px(tok::h_ftab) - 1};
    }
    QSize minimumTabSizeHint(int index) const override { return tabSizeHint(index); }

    int hover = -1;        // tab dưới chuột
    bool closeHot = false; // chuột trên × của tab đó

protected:
    bool event(QEvent* e) override
    {
        switch (e->type()) {
        case QEvent::HoverEnter:
        case QEvent::HoverMove: {
            const QPoint pos = static_cast<QHoverEvent*>(e)->position().toPoint();
            const int i = tabAt(pos);
            const bool hot = i > 0 && parts(i).close.contains(pos);
            if (i != hover || hot != closeHot) {
                hover = i;
                closeHot = hot;
                update();
            }
            owner_->hovered(i);
            break;
        }
        case QEvent::HoverLeave:
            hover = -1;
            closeHot = false;
            update();
            owner_->hovered(-1);
            break;
        case QEvent::ToolTip: // có ảnh khi rê thì không chồng thêm tooltip đường dẫn
            if (owner_->source_)
                return true;
            break;
        default:
            break;
        }
        return QTabBar::event(e);
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QFont font = theme::font(TextRole::Ui);
        const QFontMetrics fm(font);
        p.setFont(font);
        const qreal dpr = devicePixelRatioF(), radius = tm().px(tok::r_lg);
        for (int i = 0; i < count(); ++i) {
            if (!isTabVisible(i))
                continue;
            const Parts t = parts(i);
            const bool cur = i == currentIndex(), hot = i == hover && isEnabled();
            if (cur || hot) { // bo hai góc trên; đang mở: nền --c-base, vạch trên 2 px --c-accent
                QPainterPath shape;
                shape.addRoundedRect(QRectF(t.shape).adjusted(0, 0, 0, radius), radius, radius);
                p.save();
                p.setClipRect(t.shape);
                p.fillPath(shape, col(cur ? tok::c_base : tok::c_hover));
                if (cur) {
                    p.setClipPath(shape, Qt::IntersectClip);
                    p.fillRect(QRect(t.shape.topLeft(), QSize(t.shape.width(), 2)), col(tok::c_accent));
                }
                p.restore();
            }
            const QColor ink = col(!isEnabled() ? tok::c_text_dis : cur || hot ? tok::c_text : tok::c_text_2);
            const QString icon = owner_->iconName(i);
            if (!t.icon.isEmpty())
                drawGlyph(p, icon, t.icon, icon == u"home" ? ink : col(tok::c_text_3), dpr);
            p.setPen(ink);
            p.drawText(t.text, Qt::AlignVCenter | Qt::AlignLeft,
                       fm.elidedText(owner_->label(i), Qt::ElideRight, t.text.width()));
            if (!t.dot.isEmpty()) {
                p.setPen(Qt::NoPen);
                p.setBrush(col(tok::c_accent));
                p.drawEllipse(t.dot);
            }
            if (!t.close.isEmpty()) {
                const bool closeHover = hot && closeHot;
                if (closeHover) {
                    p.setPen(Qt::NoPen);
                    p.setBrush(col(tok::c_press));
                    p.drawRoundedRect(QRectF(t.close), tm().px(tok::r), tm().px(tok::r));
                }
                drawGlyph(p, u"x"_s, t.close, col(closeHover ? tok::c_text : tok::c_text_3), dpr);
            }
            if (cur && hasFocus()) { // focus bàn phím (QTabBar chỉ nhận focus bằng Tab): viền 2 px --c-focus
                p.setPen(QPen(col(tok::c_focus), 2));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(QRectF(t.shape).adjusted(1, 1, -1, -1), radius, radius);
            }
        }
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        owner_->hideThumbnails(); // như prototype: nhấn chuột thì ẩn ảnh, không hiện lại tới khi sang tab khác
        owner_->showTimer_.stop();
        const QPoint pos = e->position().toPoint();
        const int i = tabAt(pos);
        if (e->button() == Qt::MiddleButton) { // đóng khi nhả
            e->accept();
            return;
        }
        if (e->button() == Qt::LeftButton && i > 0 && parts(i).close.contains(pos)) { // × không chọn tab
            pressedClose_ = i;
            e->accept();
            return;
        }
        if (e->button() == Qt::LeftButton) {
            dragFrom_ = dragOrigin_ = i > 0 ? i : -1;
            dragStart_ = pos;
            dragged_ = false;
        }
        QTabBar::mousePressEvent(e);
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        const QPoint pos = e->position().toPoint();
        if ((e->buttons() & Qt::LeftButton) && dragFrom_ > 0
            && (dragged_ || (pos - dragStart_).manhattanLength() >= QApplication::startDragDistance())) {
            dragged_ = true;
            const int to = tabAt(QPoint(pos.x(), height() / 2)); // con trỏ lệch khỏi dải theo chiều dọc vẫn kéo
            if (to > 0 && to != dragFrom_) {                      // Start (0) đứng yên
                moveTab(dragFrom_, to);
                dragFrom_ = to;
            }
        }
        QTabBar::mouseMoveEvent(e);
    }

    void mouseReleaseEvent(QMouseEvent* e) override
    {
        const QPoint pos = e->position().toPoint();
        const int i = tabAt(pos);
        if (e->button() == Qt::MiddleButton) {
            if (i > 0)
                owner_->command(owner_->idAt(i), u"CLOSE"_s);
            e->accept();
            return;
        }
        if (e->button() == Qt::LeftButton && pressedClose_ >= 0) {
            const int pressed = std::exchange(pressedClose_, -1);
            if (i == pressed && parts(i).close.contains(pos))
                owner_->command(owner_->idAt(i), u"CLOSE"_s);
            e->accept();
            return;
        }
        if (e->button() == Qt::LeftButton) {
            if (dragged_ && dragFrom_ > 0 && dragFrom_ != dragOrigin_)
                emit owner_->moved(owner_->idAt(dragFrom_), dragFrom_ - 1);
            dragFrom_ = -1;
            dragged_ = false;
        }
        QTabBar::mouseReleaseEvent(e);
    }

    void contextMenuEvent(QContextMenuEvent* e) override
    {
        const bool keyboard = e->reason() == QContextMenuEvent::Keyboard;
        const int i = keyboard ? currentIndex() : tabAt(e->pos());
        if (i >= 0) // vùng trống của dải: không có menu (prototype)
            owner_->showMenu(i, keyboard ? mapToGlobal(parts(i).shape.bottomLeft()) : e->globalPos());
    }

    void keyPressEvent(QKeyEvent* e) override
    {
        if (e->key() == Qt::Key_Menu || (e->key() == Qt::Key_F10 && e->modifiers() == Qt::ShiftModifier)) {
            owner_->showMenu(currentIndex(), mapToGlobal(parts(currentIndex()).shape.bottomLeft()));
            return;
        }
        // ←→ đi qua cả tab đang ẩn (QTabBar bỏ qua chúng): tab mới mở hiện ra nhờ fit().
        if ((e->key() == Qt::Key_Left || e->key() == Qt::Key_Right) && !(e->modifiers() & ~Qt::KeypadModifier)) {
            const int i = currentIndex() + (e->key() == Qt::Key_Right ? 1 : -1);
            if (i >= 0 && i < count())
                setCurrentIndex(i);
            return;
        }
        QTabBar::keyPressEvent(e);
    }

private:
    FileTabBar* owner_;
    int pressedClose_ = -1; // tab có × đang nhấn
    int dragFrom_ = -1;     // tab bản vẽ đang kéo, chỗ hiện tại
    int dragOrigin_ = -1;   // chỗ lúc nhấn
    QPoint dragStart_;
    bool dragged_ = false;
};

// ------------------------------------------------------------------------------------------------- Popup

// Hàng ảnh dưới tab (.ft-thumb): nền --c-raised, viền --c-border-ctl, bo --r-lg, lề và khoảng cách --sp-3; hết bề
// rộng màn hình thì xuống hàng. Cửa sổ ToolTip riêng: không lấy focus, không kích hoạt.
// ponytail: chưa có bóng --sh-pop (như toast, chú thích của UI0-009).
class FileTabBar::Popup final : public QWidget {
public:
    explicit Popup(FileTabBar* owner) : QWidget(owner, Qt::ToolTip | Qt::FramelessWindowHint), owner_(owner)
    {
        setAttribute(Qt::WA_ShowWithoutActivating);
        setFocusPolicy(Qt::NoFocus);
    }

    quint64 id = kStart;
    std::vector<Cell*> cells;

    void place(int columns)
    {
        const int pad = px(tok::sp_3), gap = px(tok::sp_3);
        QSize cell;
        for (Cell* c : cells)
            cell = cell.expandedTo(c->sizeHint());
        for (int k = 0; k < int(cells.size()); ++k)
            cells[k]->setGeometry(pad + (k % columns) * (cell.width() + gap), pad + (k / columns) * (cell.height() + gap),
                                  cell.width(), cell.height());
        const int rows = (int(cells.size()) + columns - 1) / columns;
        const int cols = std::min(columns, int(cells.size()));
        resize(2 * pad + cols * cell.width() + (cols - 1) * gap, 2 * pad + rows * cell.height() + (rows - 1) * gap);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(col(tok::c_border_ctl));
        p.setBrush(col(tok::c_raised));
        const qreal r = tm().px(tok::r_lg);
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), r, r);
    }
    void enterEvent(QEnterEvent*) override { owner_->hideTimer_.stop(); }
    void leaveEvent(QEvent*) override { owner_->hideTimer_.start(); }

private:
    FileTabBar* owner_;
};

// ------------------------------------------------------------------------------------------------- FileTabBar

FileTabBar::FileTabBar(QWidget* parent)
    : QWidget(parent), tabs_(new Tabs(this)), add_(new Button({}, Button::Kind::Icon, this)),
      more_(new Button({}, Button::Kind::Icon, this)), menu_(new Menu(this)), list_(new Menu(this)), showTimer_(this),
      hideTimer_(this)
{
    setAcceptDrops(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    menu_->setObjectName(u"fileTabMenu"_s);
    list_->setObjectName(u"fileTabList"_s);
    add_->setIconName(u"plus"_s);
    more_->setIconName(u"down"_s);
    more_->hide();
    tabs_->addTab({}); // Start: dữ liệu rỗng = kStart
    showTimer_.setSingleShot(true);
    showTimer_.setInterval(kThumbDelay);
    hideTimer_.setSingleShot(true);
    hideTimer_.setInterval(kThumbGrace);
    connect(&showTimer_, &QTimer::timeout, this, &FileTabBar::showThumbnails);
    connect(&hideTimer_, &QTimer::timeout, this, [this] {
        if (!popup_ || !popup_->underMouse())
            hideThumbnails();
    });
    connect(tabs_, &QTabBar::currentChanged, this, [this](int index) {
        fit(); // tab mới mở luôn hiện
        const quint64 id = idAt(index);
        if (index < 0 || id == currentId_)
            return;
        currentId_ = id;
        if (!silent_)
            emit activateRequested(id, {});
    });
    connect(add_, &QAbstractButton::clicked, this, [this] { command(current(), viewer() ? u"OPEN"_s : u"NEW"_s); });
    connect(more_, &QAbstractButton::clicked, this, &FileTabBar::showList);
    if (auto* gate = edition::EditionGate::instance()) { // EditionGate tạo trước dải (main)
        connect(gate, &edition::EditionGate::profileChanged, this, &FileTabBar::retranslate);
        connect(gate, &QObject::destroyed, this, &FileTabBar::retranslate, Qt::QueuedConnection);
    }
    retranslate();
}

FileTabBar::~FileTabBar()
{
    cancel_.cancel();
}

QTabBar* FileTabBar::tabBar() const
{
    return tabs_;
}

QWidget* FileTabBar::thumbnailPopup() const
{
    return popup_;
}

int FileTabBar::indexOf(quint64 id) const
{
    for (int i = 0; i < tabs_->count(); ++i)
        if (idAt(i) == id)
            return i;
    return -1;
}

quint64 FileTabBar::idAt(int index) const
{
    return tabs_->tabData(index).toULongLong(); // Start: dữ liệu rỗng → 0
}

QString FileTabBar::label(int index) const
{
    const quint64 id = idAt(index);
    return id == kStart ? tr("Start") : docs_.value(id).name;
}

QString FileTabBar::iconName(int index) const
{
    const quint64 id = idAt(index);
    if (id == kStart)
        return u"home"_s;
    return docs_.value(id).readOnly || viewer() ? u"lock"_s : QString();
}

bool FileTabBar::viewer() const
{
    auto* gate = edition::EditionGate::instance();
    return gate && gate->profile() == edition::UiProfile::Viewer;
}

void FileTabBar::addDocument(const DocTab& doc)
{
    TRION_ASSERT(doc.id != kStart && doc.id != kNone);
    if (indexOf(doc.id) >= 0)
        return updateDocument(doc);
    docs_.insert(doc.id, doc);
    const int i = tabs_->addTab({});
    tabs_->setTabData(i, QVariant::fromValue(doc.id));
    syncTab(i);
    fit();
}

void FileTabBar::updateDocument(const DocTab& doc)
{
    const int i = indexOf(doc.id);
    if (i <= 0)
        return;
    docs_.insert(doc.id, doc);
    dropThumbnails(doc.id);
    syncTab(i);
    fit();
}

void FileTabBar::dropThumbnails(quint64 id)
{
    ++epoch_;
    for (const ThumbKey& k : cache_.keys())
        if (k.id == id)
            cache_.remove(k);
    if (popup_ && popup_->id == id)
        hideThumbnails();
}

void FileTabBar::removeDocument(quint64 id)
{
    const int i = indexOf(id);
    if (i <= 0)
        return;
    dropThumbnails(id);
    if (hoverId_ == id) {
        hoverId_ = kNone;
        showTimer_.stop();
    }
    docs_.remove(id);
    silent_ = true;
    tabs_->removeTab(i);
    silent_ = false;
    fit();
}

void FileTabBar::setCurrent(quint64 id)
{
    if (const int i = indexOf(id); i >= 0) {
        silent_ = true;
        tabs_->setCurrentIndex(i);
        silent_ = false;
    }
}

quint64 FileTabBar::current() const
{
    return idAt(tabs_->currentIndex());
}

QList<quint64> FileTabBar::documents() const
{
    QList<quint64> ids;
    for (int i = 1; i < tabs_->count(); ++i)
        ids << idAt(i);
    return ids;
}

void FileTabBar::setThumbnailSource(ThumbnailSource source)
{
    source_ = std::move(source);
    if (!source_)
        hideThumbnails();
}

void FileTabBar::setFolderOpener(FolderOpener opener)
{
    opener_ = std::move(opener);
}

void FileTabBar::next()
{
    tabs_->setCurrentIndex((tabs_->currentIndex() + 1) % tabs_->count());
}

void FileTabBar::previous()
{
    tabs_->setCurrentIndex((tabs_->currentIndex() + tabs_->count() - 1) % tabs_->count());
}

void FileTabBar::command(quint64 id, const QString& name)
{
    emit commandRequested(id, u"^C^C_"_s + name);
}

void FileTabBar::syncTab(int index)
{
    const DocTab doc = docs_.value(idAt(index));
    QString text = doc.name;
    tabs_->setTabText(index, text.replace(u'&', u"&&"_s)); // tên tệp có "&": không thành phím tắt Alt
    QString name = doc.name; // trình đọc màn hình: trạng thái bằng chữ, không chỉ chấm, ổ khóa
    if (doc.dirty)
        name = tr("%1, chưa lưu").arg(name);
    if (doc.readOnly || viewer())
        name = tr("%1, chỉ đọc").arg(name);
    tabs_->setAccessibleTabName(index, name);
    tabs_->setTabToolTip(index, QDir::toNativeSeparators(doc.path));
}

void FileTabBar::retranslate()
{
    tabs_->setTabText(0, tr("Start"));
    setAccessibleName(tr("Tab tệp")); // QAccessibleTabBar lấy tên của tab đang mở làm tên dải: đặt ở khung chứa
    const bool v = viewer();
    const QString add = v ? tr("Mở bản vẽ") : tr("Bản vẽ mới");
    const QString all = tr("Mọi tab tệp");
    add_->setAccessibleName(add);
    more_->setAccessibleName(all);
    if (RichTooltip::instance()) {
        RichTooltip::attach(add_, {.name = add, .command = v ? u"OPEN"_s : u"NEW"_s, .keyId = v ? u"open"_s : u"new"_s});
        RichTooltip::attach(more_, {.name = all});
    } else {
        add_->setToolTip(add);
        more_->setToolTip(all);
    }
    for (int i = 1; i < tabs_->count(); ++i)
        syncTab(i);
    tabs_->update();
    fit();
}

void FileTabBar::fit()
{
    // .ft: lề --sp-3 hai bên, cách nhau 2 px (--sp-1), nút cao --h-ctl sát đáy (align-items: flex-end); nét đáy 1 px.
    const int n = tabs_->count(), cur = tabs_->currentIndex(), pad = px(tok::sp_3), gap = px(tok::sp_1);
    const int h = height() - 1;
    const QSize addSize = add_->sizeHint(), moreSize = more_->sizeHint();
    std::vector<int> w(size_t(n), 0);
    int total = 0;
    for (int i = 0; i < n; ++i)
        total += w[size_t(i)] = tabs_->tabSizeHint(i).width();
    int room = width() - 2 * pad - gap - addSize.width();
    const bool overflow = total > room;
    // Không đủ chỗ: giữ Start, tab đang mở và các tab đầu còn vừa; ẩn phần sau (fitTabs của prototype ẩn từ cuối).
    int used = 0;
    if (overflow)
        room -= gap + moreSize.width();
    for (int i = 0; i < n; ++i)
        used += i == 0 || i == cur ? w[size_t(i)] : 0;
    bool full = !overflow ? false : used > room;
    for (int i = 1; i < n; ++i) {
        if (i == cur)
            continue;
        full = full || (overflow && used + w[size_t(i)] > room);
        if (!full)
            used += w[size_t(i)];
        tabs_->setTabVisible(i, !full);
    }
    if (cur > 0)
        tabs_->setTabVisible(cur, true);
    tabs_->setGeometry(pad, 0, used, h);
    add_->setGeometry(pad + used + gap, h - addSize.height(), addSize.width(), addSize.height());
    more_->setVisible(overflow);
    more_->setGeometry(width() - pad - moreSize.width(), h - moreSize.height(), moreSize.width(), moreSize.height());
}

QSize FileTabBar::sizeHint() const
{
    int w = 2 * px(tok::sp_3) + px(tok::sp_1) + add_->sizeHint().width();
    for (int i = 0; i < tabs_->count(); ++i)
        w += tabs_->tabSizeHint(i).width();
    return {w, px(tok::h_ftab)};
}

QSize FileTabBar::minimumSizeHint() const
{
    return {2 * px(tok::sp_3) + 2 * px(tok::sp_1) + tabs_->tabSizeHint(0).width() + add_->sizeHint().width()
                + more_->sizeHint().width(),
            px(tok::h_ftab)};
}

void FileTabBar::paintEvent(QPaintEvent*)
{
    QPainter p(this); // .ft: nền --c-chrome, nét đáy --c-border
    p.fillRect(rect(), col(tok::c_chrome));
    p.fillRect(QRect(0, height() - 1, width(), 1), col(tok::c_border));
}

void FileTabBar::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    fit();
}

void FileTabBar::changeEvent(QEvent* e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    else if (e->type() == QEvent::StyleChange || e->type() == QEvent::FontChange) { // mật độ: --h-ftab, chữ đổi
        updateGeometry();
        fit();
    }
}

void FileTabBar::hideEvent(QHideEvent* e)
{
    QWidget::hideEvent(e);
    hideThumbnails();
    showTimer_.stop();
}

// ------------------------------------------------------------------------------------------------- menu

void FileTabBar::showMenu(int index, const QPoint& globalPos)
{
    hideThumbnails();
    showTimer_.stop();
    const quint64 id = idAt(index);
    const DocTab doc = docs_.value(id);
    const bool isDoc = id != kStart, hasPath = isDoc && !doc.path.isEmpty();
    menu_->clear();
    // Mục: chữ, biểu tượng, lệnh (id của mục, tooltip), id phím trong Keymap, bản cần, bật, việc làm.
    const auto item = [&](const QString& text, const QString& icon, const QString& feature, const QString& keyId,
                          Edition need, bool enabled, std::function<void()> run) {
        QAction* a = icon.isEmpty() ? menu_->addAction(text) : menu_->addAction(IconProvider::icon(icon), text);
        a->setObjectName(feature);
        a->setEnabled(enabled);
        RichTooltip::attach(a, {.command = feature.startsWith(u'_') ? QString() : feature,
                                .keyId = keyId,
                                .need = lockedNeed(need)});
        connect(a, &QAction::triggered, this, [feature, need, text, run = std::move(run)] {
            if (!gated(feature, need, text))
                run();
        });
    };
    const auto each = [this](auto pick, const QString& name) { // lệnh cho từng tab bản vẽ được chọn, theo thứ tự tab
        for (const quint64 doc : documents())
            if (pick(docs_.value(doc)))
                command(doc, name);
    };
    item(tr("Mới"), u"new"_s, u"NEW"_s, u"new"_s, Edition::Pro, true, [this, id] { command(id, u"NEW"_s); });
    item(tr("Mở…"), u"open"_s, u"OPEN"_s, u"open"_s, Edition::None, true, [this, id] { command(id, u"OPEN"_s); });
    item(tr("Lưu"), u"save"_s, u"QSAVE"_s, u"save"_s, Edition::Pro, isDoc, [this, id] { command(id, u"QSAVE"_s); });
    item(tr("Lưu tất cả"), {}, u"SAVEALL"_s, {}, Edition::Pro, true, [this, each] {
        const QList<quint64> ids = documents();
        if (std::none_of(ids.begin(), ids.end(), [this](quint64 d) { return docs_.value(d).dirty; }))
            return toast(Severity::Info, tr("Không có bản vẽ nào cần lưu."));
        each([](const DocTab& d) { return d.dirty; }, u"QSAVE"_s);
    });
    menu_->addSeparator();
    item(tr("Đóng"), u"fileclose"_s, u"CLOSE"_s, u"close"_s, Edition::None, isDoc, [this, id] { command(id, u"CLOSE"_s); });
    item(tr("Đóng tất cả"), {}, u"CLOSEALL"_s, {}, Edition::None, tabs_->count() > 1,
         [this, id] { command(id, u"CLOSEALL"_s); });
    item(tr("Đóng các tab khác"), {}, u"_closeOthers"_s, {}, Edition::None, isDoc,
         [each, id] { each([id](const DocTab& d) { return d.id != id; }, u"CLOSE"_s); });
    menu_->addSeparator();
    item(tr("Sao chép đường dẫn"), u"copyclip"_s, u"_copyPath"_s, {}, Edition::None, hasPath, [path = doc.path] {
        const QString native = QDir::toNativeSeparators(path);
        QGuiApplication::clipboard()->setText(native);
        toast(Severity::Ok, tr("Đã sao chép: %1").arg(native));
    });
    item(tr("Mở thư mục chứa"), u"folder"_s, u"_openFolder"_s, {}, Edition::None, hasPath, [this, path = doc.path] {
        const QString dir = QFileInfo(path).absolutePath();
        if (!(opener_ ? opener_(dir) : QDesktopServices::openUrl(QUrl::fromLocalFile(dir))))
            toast(Severity::Warning, tr("Không mở được thư mục %1.").arg(QDir::toNativeSeparators(dir)));
    });
    menu_->popup(globalPos);
}

void FileTabBar::showList()
{
    hideThumbnails();
    list_->clear();
    for (int i = 0; i < tabs_->count(); ++i) {
        const quint64 id = idAt(i);
        QString text = label(i);
        QAction* a = list_->addAction(text.replace(u'&', u"&&"_s));
        a->setCheckable(true);
        a->setChecked(i == tabs_->currentIndex());
        connect(a, &QAction::triggered, this, [this, id] {
            if (const int at = indexOf(id); at >= 0)
                tabs_->setCurrentIndex(at);
        });
    }
    // .ft ▾: mép phải menu thẳng mép phải nút (như menu » của palette trong prototype)
    const QPoint corner = more_->mapToGlobal(QPoint(more_->width(), more_->height() + px(tok::sp_1)));
    list_->popup(corner - QPoint(list_->sizeHint().width(), 0));
}

// ------------------------------------------------------------------------------------------------- ảnh khi rê

void FileTabBar::hovered(int index)
{
    const quint64 id = index < 0 ? kNone : idAt(index);
    if (popup_ && popup_->isVisible() && popup_->id == id) { // quay lại tab đang có ảnh
        hoverId_ = id;
        hideTimer_.stop();
        return;
    }
    if (id == hoverId_)
        return;
    hoverId_ = id;
    showTimer_.stop();
    if (index < 0) { // rời dải: chờ một chút, chuột có thể đang xuống hàng ảnh
        if (popup_ && popup_->isVisible())
            hideTimer_.start();
        return;
    }
    hideThumbnails();
    if (id != kStart && source_)
        showTimer_.start();
}

void FileTabBar::showThumbnails()
{
    const int index = indexOf(hoverId_);
    if (index <= 0 || !source_ || !tabs_->isTabVisible(index))
        return;
    const DocTab doc = docs_.value(hoverId_);
    if (!popup_)
        popup_ = new Popup(this);
    for (Cell* c : popup_->cells)
        delete c;
    popup_->cells.clear();
    popup_->id = doc.id;
    popup_->setAccessibleName(tr("Xem trước %1").arg(doc.name));
    cancel_.cancel();
    cancel_ = sched::CancelSource{};
    const qreal dpr = devicePixelRatioF();
    const QSize size = (QSizeF(kThumbSize) * dpr).toSize();
    const ui::Receiver rx(this);
    QStringList layouts{kModel.toString()};
    layouts += doc.layouts;
    for (const QString& layout : layouts) {
        auto* cell = new Cell(layout, layout == kModel ? tr("Model") : layout, popup_);
        popup_->cells.push_back(cell);
        connect(cell, &QAbstractButton::clicked, this, [this, id = doc.id, layout] {
            hideThumbnails();
            setCurrent(id);
            emit activateRequested(id, layout);
        });
        if (const QImage* image = cache_.object({doc.id, layout, size})) { // có sẵn: hiện cùng lúc với popup
            cell->setIcon(QPixmap::fromImage(*image));
            continue;
        }
        sched::submit(sched::Priority::Background, [rx, this, source = source_, id = doc.id, layout, size,
                                                    epoch = epoch_, token = cancel_.token()] {
            if (token.cancelled())
                return;
            QImage image = source(id, layout, size, token); // luồng nền: không chạm `this`
            ui::post(rx, [this, id, layout, size, epoch, image = std::move(image)]() mutable {
                if (epoch == epoch_)
                    thumbnailReady(id, layout, size, std::move(image));
            });
        });
    }
    // Hàng ảnh dưới tab cách --sp-2; không tràn mép phải màn hình (prototype: innerWidth - rộng - 8).
    const QRect screen = this->screen() ? this->screen()->availableGeometry() : QRect(QPoint(), QSize(1920, 1080));
    const int cell = popup_->cells.front()->sizeHint().width(), gap = px(tok::sp_3);
    popup_->place(std::max(1, (screen.width() - 2 * gap + gap) / (cell + gap)));
    const QRect tab = tabs_->tabRect(index);
    QPoint at = tabs_->mapToGlobal(QPoint(tab.left(), tab.bottom() + 1 + px(tok::sp_2)));
    at.setX(std::max(screen.left(), std::min(at.x(), screen.right() + 1 - popup_->width() - px(tok::sp_4))));
    popup_->move(at);
    popup_->show();
}

void FileTabBar::hideThumbnails()
{
    hideTimer_.stop();
    cancel_.cancel();
    if (popup_)
        popup_->hide();
}

void FileTabBar::thumbnailReady(quint64 id, const QString& layout, QSize size, QImage image)
{
    if (image.isNull() || indexOf(id) <= 0) // hủy, lỗi, hoặc tài liệu đã đóng
        return;
    image.setDevicePixelRatio(qreal(size.width()) / kThumbSize.width());
    if (popup_ && popup_->isVisible() && popup_->id == id)
        for (Cell* c : popup_->cells)
            if (c->layout == layout)
                c->setIcon(QPixmap::fromImage(image));
    const qsizetype bytes = image.sizeInBytes();
    cache_.insert({id, layout, size}, new QImage(std::move(image)), bytes);
}

// ------------------------------------------------------------------------------------------------- thả tệp

FileTabBar::Drop FileTabBar::readDrop(const QMimeData& data)
{
    Drop d;
    QStringList rejected;
    for (const QUrl& url : data.urls())
        if (url.isLocalFile()) {
            const QString path = url.toLocalFile();
            const QString suffix = QFileInfo(path).suffix();
            if (suffix.compare(u"dwg"_s, Qt::CaseInsensitive) == 0 || suffix.compare(u"dxf"_s, Qt::CaseInsensitive) == 0
                || suffix.compare(u"dwt"_s, Qt::CaseInsensitive) == 0)
                d.drawings << path;
            else
                rejected << u"“%1”"_s.arg(QFileInfo(path).fileName());
        }
    if (!rejected.isEmpty())
        d.rejection = tr("TrionCAD mở được tệp DWG, DXF, DWT. %1 không phải bản vẽ.", nullptr, int(rejected.size()))
                          .arg(rejected.join(u", "_s));
    return d;
}

void FileTabBar::dragEnterEvent(QDragEnterEvent* e)
{
    // Nhận mọi tệp trên máy để thả được rồi báo tệp sai định dạng (prototype); URL web thì không nhận.
    if (!readDrop(*e->mimeData()).empty())
        e->acceptProposedAction();
}

void FileTabBar::dropEvent(QDropEvent* e)
{
    const Drop d = readDrop(*e->mimeData());
    e->acceptProposedAction();
    if (!d.drawings.isEmpty())
        emit openRequested(d.drawings);
    if (!d.rejection.isEmpty())
        toast(Severity::Error, d.rejection);
}

} // namespace trion::shell
