#include "app/shell/ribbon/controls/RibbonControls.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/edition/LockBadge.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/numeric/NumericField.h"
#include "app/widgets/tooltip/RichTooltip.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QFontInfo>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QPainter>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyledItemDelegate>

#include <algorithm>
#include <climits>

using namespace Qt::StringLiterals;

namespace trion::ribbon {

namespace {

using edition::EditionGate;
using edition::LockBadge;
using theme::IconProvider;
using theme::IconState;
using theme::TextRole;

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

// Giá trị của prototype không có token riêng (như app/widgets/controls).
constexpr int kBorder = 1;             // nét viền, nét chia 1 px
constexpr int kChevron = 9;            // mũi tên .ic.xs
constexpr qreal kLineHeight = 1.18;    // .rb-lg line-height
constexpr qreal kLockedOpacity = 0.75; // .locked .ic
constexpr qreal kArrowSplit = 0.55;    // nút lớn: phần chính là 55% trên (prototype ctlRun)
constexpr int kMenuRows = 200;         // danh sách split theo source: QMenu không ảo hóa (DoD-U 4)

// Focus từ bàn phím (:focus-visible), như app/widgets/controls.
bool focusVisible(const QWidget* w)
{
    return w->hasFocus() && w->window()->testAttribute(Qt::WA_KeyboardFocusChange);
}

void drawRing(QPainter& p, const QRectF& r, const QColor& c)
{
    const qreal rad = tm().px(tok::r);
    p.setPen(QPen(c, 2));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r.adjusted(1, 1, -1, -1), rad, rad);
}

void drawChevron(QPainter& p, QPointF c, const QColor& color)
{
    const qreal u = kChevron / 16.0;
    p.setPen(QPen(color, tok::num(tok::sw) * 1.5 * u, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(QList<QPointF>{c + QPointF(-4 * u, -2 * u), c + QPointF(0, 2 * u), c + QPointF(4 * u, -2 * u)});
}

// Biểu tượng một màu tô bằng màu chữ (currentColor của prototype).
QPixmap glyph(const QString& name, int size, qreal dpr, const QColor& c)
{
    QPixmap pm = IconProvider::instance().pixmap(name, size, dpr, IconState::Normal);
    if (pm.isNull())
        return pm;
    QPainter p(&pm);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(pm.rect(), c);
    return pm;
}

// Lệnh trong danh mục (tạm thay INT-001); nullptr nếu không có (hành động gắn sysvar).
const actions::CommandInfo* commandInfo(const QString& cmd)
{
    static const QHash<QString, const actions::CommandInfo*> index = [] {
        QHash<QString, const actions::CommandInfo*> h;
        for (const actions::CommandInfo& c : actions::commands())
            h.insert(QString::fromLatin1(c.name), &c);
        return h;
    }();
    return index.value(cmd);
}

QString keyId(const actions::ActionRegistry& r, const QString& cmd)
{
    const QAction* a = r.action(cmd);
    return a ? ui::RichTooltip::info(a).keyId : QString();
}

QIcon toIcon(const QVariant& v)
{
    switch (v.typeId()) {
    case QMetaType::QIcon:
        return v.value<QIcon>();
    case QMetaType::QPixmap:
        return QIcon(v.value<QPixmap>());
    case QMetaType::QImage:
        return QIcon(QPixmap::fromImage(v.value<QImage>()));
    default:
        return {};
    }
}

// Chữ của danh sách theo source khi không có mục để hiện; rỗng khi có.
QString dataMessage(const RibbonControls& o, const QString& source)
{
    switch (o.dataState(source)) {
    case DataState::Loading:
        return RibbonControls::tr("Đang tải…");
    case DataState::Error:
        return RibbonControls::tr("Không tải được");
    case DataState::Ready:
        break;
    }
    const QAbstractItemModel* m = o.model(source);
    return m && m->rowCount() > 0 ? QString() : RibbonControls::tr("Không có mục");
}

// Model thêm, bớt dòng (BackgroundModel chèn dòng sau khi setModel): gọi f để chữ rỗng/có mục đúng.
template <class F>
void onRows(QAbstractItemModel* m, QObject* w, F f)
{
    QObject::connect(m, &QAbstractItemModel::modelReset, w, f);
    QObject::connect(m, &QAbstractItemModel::rowsInserted, w, f);
    QObject::connect(m, &QAbstractItemModel::rowsRemoved, w, f);
}

// Theo dõi đổi phiên bản (khóa) cho widget w.
template <class F>
void onEdition(QWidget* w, F f)
{
    if (EditionGate* g = EditionGate::instance())
        QObject::connect(g, &EditionGate::editionChanged, w, f);
}

void tip(QWidget* w, const RibbonControls& o, const Control& c, const QString& name)
{
    const ui::Edition need = o.need(c);
    ui::RichTooltip::attach(w, {.name = name,
                                .command = c.cmd,
                                .keyId = keyId(o.registry(), c.cmd),
                                .description = RibbonControls::describe(c.cmd, name),
                                .need = RibbonControls::locked(need) ? need : ui::Edition::None});
}

// ------------------------------------------------------------------------------------------- DropDown (.rb-dd)

class RibbonDropDown final : public ui::DropDown {
public:
    RibbonDropDown(const Control& c, RibbonControls& owner, QWidget* parent) : DropDown(parent), c_(c), owner_(owner)
    {
        setFocusPolicy(Qt::TabFocus); // bấm chuột không lấy focus của vùng vẽ
        setFont(theme::font(TextRole::Label));
        connect(&owner, &RibbonControls::sourceChanged, this, [this](const QString& s) {
            if (s == c_.source)
                syncData();
        });
        connect(this, &QComboBox::activated, this, [this](int row) { emit owner_.picked(c_.source, row); });
        onEdition(this, [this] { syncText(); });
        syncData();
    }

    QSize sizeHint() const override
    {
        const int chrome =
            px(tok::sp_4) + px(tok::sp_3) * 2 + kChevron + (isLocked() ? px(tok::ic) + px(tok::sp_3) : 0);
        return {std::max(c_.width, fontMetrics().horizontalAdvance(placeholderText()) + chrome), px(tok::h_btn)};
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

    void showPopup() override
    {
        if (isLocked())
            owner_.upgrade(c_);
        else if (dataMessage(owner_, c_.source).isEmpty())
            DropDown::showPopup();
    }

protected:
    void changeEvent(QEvent* e) override
    {
        if (e->type() == QEvent::LanguageChange)
            syncText();
        DropDown::changeEvent(e);
    }

private:
    void syncData()
    {
        syncText(); // placeholder trước setModel: QComboBox có placeholder thì không tự chọn dòng đầu
        if (QAbstractItemModel* m = owner_.model(c_.source); m && model() != m) {
            setModel(m);
            onRows(m, this, [this] { syncText(); });
        }
        setCurrentIndex(owner_.dataState(c_.source) == DataState::Ready ? owner_.current(c_.source) : -1);
        syncText();
    }

    void syncText()
    {
        // Chưa có model (nguồn chưa nối): chữ chờ là tên dropdown; có model rỗng mới là "Không có mục".
        const bool wired = owner_.model(c_.source) || owner_.dataState(c_.source) != DataState::Ready;
        const QString name = RibbonModel::line(c_.label), msg = wired ? dataMessage(owner_, c_.source) : QString();
        const ui::Edition need = owner_.need(c_);
        setPlaceholderText(msg.isEmpty() ? name : msg);
        setError(owner_.dataState(c_.source) == DataState::Error);
        setLocked(RibbonControls::locked(need));
        setAccessibleName(name);
        setAccessibleDescription(isLocked() ? EditionGate::availableIn(need) : msg);
        tip(this, owner_, c_, name);
        updateGeometry();
    }

    Control c_;
    RibbonControls& owner_;
};

// ------------------------------------------------------------------------------------------- Gallery (.rb-gal)

// Ô mẫu .rb-gi 48×34 (3 × --ic, 2 × --ic + --sp-1), ảnh mẫu 44×30 (trừ --sp-1 mỗi bên), khe --sp-1.
QSize cellSize()
{
    return {3 * px(tok::ic), 2 * px(tok::ic) + px(tok::sp_1)};
}

class GalleryDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return cellSize(); }

    void paint(QPainter* p, const QStyleOptionViewItem& o, const QModelIndex& i) const override
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const QRect inner = o.rect.adjusted(px(tok::sp_1), px(tok::sp_1), -px(tok::sp_1), -px(tok::sp_1));
        if (!(o.state & QStyle::State_Enabled))
            p->setOpacity(0.45); // như nút vô hiệu
        toIcon(i.data(Qt::DecorationRole)).paint(p, inner);
        const qreal rad = tm().px(tok::r);
        const QRectF box = QRectF(o.rect).adjusted(0.5, 0.5, -0.5, -0.5);
        if (o.state & QStyle::State_Selected) { // .rb-gi.on: viền --c-accent và viền trong 1 px
            p->setPen(QPen(col(tok::c_accent), 2 * kBorder));
            p->drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), rad, rad);
        } else if (o.state & QStyle::State_MouseOver) {
            p->setPen(QPen(col(tok::c_border_ctl), kBorder));
            p->drawRoundedRect(box, rad, rad);
        }
        if ((o.state & QStyle::State_HasFocus) && focusVisible(static_cast<const QWidget*>(o.widget)))
            drawRing(*p, o.rect, col(tok::c_focus));
        p->restore();
    }
};

class GalleryView final : public QListView {
public:
    GalleryView(const Control& c, RibbonControls& owner, QWidget* parent) : QListView(parent), c_(c), owner_(owner)
    {
        setFlow(LeftToRight);
        setWrapping(true);
        setResizeMode(Adjust);
        setUniformItemSizes(true); // ảo hóa: chỉ vẽ ô trong vùng nhìn, không dựng widget cho từng mục
        setSpacing(px(tok::sp_1) / 2);
        setVerticalScrollMode(ScrollPerPixel);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setFrameShape(NoFrame);
        setEditTriggers(NoEditTriggers);
        setSelectionMode(SingleSelection);
        setMouseTracking(true);
        viewport()->setAttribute(Qt::WA_Hover);
        viewport()->setAutoFillBackground(false);
        setFocusPolicy(Qt::TabFocus);
        setItemDelegate(new GalleryDelegate(this));
        connect(this, &QAbstractItemView::clicked, this, &GalleryView::pick);
    }

    int rowStep() const { return cellSize().height() + 2 * spacing(); }

    void pick(const QModelIndex& i)
    {
        if (!i.isValid())
            return;
        if (RibbonControls::locked(owner_.need(c_)))
            return owner_.upgrade(c_);
        selectionModel()->setCurrentIndex(i, QItemSelectionModel::ClearAndSelect);
        emit owner_.picked(c_.source, i.row());
    }

protected:
    void keyPressEvent(QKeyEvent* e) override
    {
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_Space)
            return pick(currentIndex());
        QListView::keyPressEvent(e);
    }

    void paintEvent(QPaintEvent* e) override
    {
        const QString msg = dataMessage(owner_, c_.source);
        if (msg.isEmpty())
            return QListView::paintEvent(e);
        QPainter p(viewport()); // rỗng, đang tải, lỗi: chữ thay lưới (không chỉ bằng màu)
        p.setFont(theme::font(TextRole::Label));
        p.setPen(col(owner_.dataState(c_.source) == DataState::Error ? tok::c_danger : tok::c_text_3));
        p.drawText(viewport()->rect(), Qt::AlignCenter | Qt::TextWordWrap, msg);
    }

private:
    const Control& c_;
    RibbonControls& owner_;
};

class RibbonGallery final : public QWidget {
public:
    RibbonGallery(const Control& c, RibbonControls& owner, QWidget* parent)
        : QWidget(parent), c_(c), owner_(owner), view_(new GalleryView(c_, owner, this))
    {
        auto* nav = new QVBoxLayout;
        nav->setContentsMargins({});
        nav->setSpacing(0);
        const auto navButton = [&](const QString& icon) {
            auto* b = new ui::Button({}, ui::Button::Kind::Icon, this);
            b->setIconName(icon);
            b->setFocusPolicy(Qt::NoFocus); // bàn phím: mũi tên trong lưới cuộn theo
            nav->addWidget(b);
            return b;
        };
        up_ = navButton(u"up"_s);
        down_ = navButton(u"down"_s);
        more_ = navButton(u"more"_s);
        connect(up_, &QAbstractButton::clicked, this, [this] { scrollRows(-1); });
        connect(down_, &QAbstractButton::clicked, this, [this] { scrollRows(1); });
        connect(more_, &QAbstractButton::clicked, this, [this] { emit owner_.moreRequested(c_.source); });
        placeNav(sizeHint().height());

        auto* box = new QHBoxLayout(this);
        box->setContentsMargins(kBorder, kBorder, kBorder, kBorder);
        box->setSpacing(kBorder); // nét chia trước cột điều hướng
        box->addWidget(view_, 1);
        box->addLayout(nav);
        badge_ = new LockBadge(owner.need(c_), LockBadge::Style::Ribbon, this); // tự hiện/ẩn theo EditionGate

        connect(&owner, &RibbonControls::sourceChanged, this, [this](const QString& s) {
            if (s == c_.source)
                syncData();
        });
        onEdition(this, [this] { syncText(); });
        syncData();
    }

    QSize sizeHint() const override
    {
        const int w = c_.width > 0 ? c_.width : 6 * (cellSize().width() + px(tok::sp_1)) + px(tok::ic);
        return {w, 3 * px(tok::h_btn) + 2 * kBorder};
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal rad = tm().px(tok::r);
        p.setPen(QPen(col(tok::c_border_ctl), kBorder));
        p.setBrush(col(tok::c_sunken));
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), rad, rad);
        p.fillRect(QRect(up_->x() - kBorder, kBorder, kBorder, height() - 2 * kBorder), col(tok::c_border_ctl));
    }

    void resizeEvent(QResizeEvent* e) override
    {
        QWidget::resizeEvent(e);
        placeNav(height());
        badge_->adjustSize();
        badge_->move(up_->x() - kBorder - badge_->width(), kBorder);
    }

    void changeEvent(QEvent* e) override
    {
        if (e->type() == QEvent::LanguageChange)
            syncText();
        QWidget::changeEvent(e);
    }

private:
    void placeNav(int h) // .rb-gal-nav: cột rộng --ic, ba nút chia đều chiều cao
    {
        for (ui::Button* b : {up_, down_, more_})
            b->setFixedSize(px(tok::ic), (h - 2 * kBorder) / 3);
    }

    void scrollRows(int n)
    {
        QScrollBar* bar = view_->verticalScrollBar();
        bar->setValue(bar->value() + n * view_->rowStep());
    }

    void syncData()
    {
        QAbstractItemModel* m = owner_.model(c_.source);
        if (m && view_->model() != m) {
            view_->setModel(m);
            onRows(m, this, [this] { syncText(); });
        }
        if (m && owner_.current(c_.source) >= 0)
            view_->selectionModel()->setCurrentIndex(m->index(owner_.current(c_.source), 0),
                                                     QItemSelectionModel::ClearAndSelect);
        view_->viewport()->update();
        syncText();
    }

    void syncText()
    {
        const QString name = RibbonModel::line(c_.label);
        const ui::Edition need = owner_.need(c_);
        view_->setAccessibleName(name);
        view_->setAccessibleDescription(RibbonControls::locked(need) ? EditionGate::availableIn(need)
                                                                     : dataMessage(owner_, c_.source));
        up_->setAccessibleName(RibbonControls::tr("Hàng trước"));
        down_->setAccessibleName(RibbonControls::tr("Hàng sau"));
        more_->setAccessibleName(RibbonControls::tr("Mở toàn bộ gallery"));
        for (ui::Button* b : {up_, down_, more_})
            ui::RichTooltip::attach(b, {.name = b->accessibleName()});
        tip(view_, owner_, c_, name);
    }

    Control c_;
    RibbonControls& owner_;
    GalleryView* view_;
    ui::Button* up_ = nullptr;
    ui::Button* down_ = nullptr;
    ui::Button* more_ = nullptr;
    LockBadge* badge_ = nullptr;
};

// ------------------------------------------------------------------------------- Number, Slider (.rb-num)

class RibbonValue final : public QWidget {
public:
    RibbonValue(const Control& c, RibbonControls& owner, QWidget* parent)
        : QWidget(parent), c_(c), owner_(owner), label_(new QLabel(this))
    {
        auto* box = new QHBoxLayout(this);
        box->setContentsMargins(px(tok::sp_1), 0, px(tok::sp_1), 0);
        box->setSpacing(px(tok::sp_3));
        label_->setFont(theme::font(TextRole::Label));
        box->addWidget(label_);
        if (c.type == ControlType::Number) {
            field_ = new ui::NumericField(ui::ValueKind::Distance, this);
            field_->setPickEnabled(false);
            field_->setLowerBound(c.above);
            field_->setMinimumWidth(c.width);
            box->addWidget(field_);
            connect(field_, &ui::NumericField::valueCommitted, this, [this](double v) { owner_.commit(c_, v); });
        } else {
            slider_ = new ui::Slider(Qt::Horizontal, this);
            slider_->setRange(qRound(c.min), qRound(c.max));
            slider_->setFocusPolicy(Qt::TabFocus);
            slider_->setMinimumWidth(c.width);
            shown_ = new QLabel(this);
            shown_->setFont(theme::font(TextRole::Label));
            shown_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            shown_->setMinimumWidth(shown_->fontMetrics().horizontalAdvance(QString::number(qRound(c.max))));
            box->addWidget(slider_);
            box->addWidget(shown_);
            // Kéo: gửi khi thả; bàn phím, lăn, bấm rãnh: gửi ngay.
            connect(slider_, &QSlider::valueChanged, this, [this](int v) {
                shown_->setNum(v);
                if (!slider_->isSliderDown())
                    owner_.commit(c_, v);
            });
            connect(slider_, &QSlider::sliderReleased, this, [this] { owner_.commit(c_, slider_->value()); });
        }
        connect(&owner, &RibbonControls::valueChanged, this, [this](const QString& s, double v) {
            if (s == c_.sysvar)
                display(v);
        });
        onEdition(this, [this] { syncText(); });
        display(owner.value(c.sysvar));
        syncText();
        applyColor();
    }

    QSize sizeHint() const override
    {
        QSize s = QWidget::sizeHint();
        s.setHeight(std::max(s.height(), px(tok::h_btn)));
        return s;
    }

protected:
    void changeEvent(QEvent* e) override
    {
        if (e->type() == QEvent::LanguageChange)
            syncText();
        else if (e->type() == QEvent::PaletteChange)
            applyColor();
        QWidget::changeEvent(e);
    }

private:
    void display(double v)
    {
        if (field_) {
            field_->setValue(v);
        } else {
            const QSignalBlocker block(slider_);
            slider_->setValue(qRound(v));
            shown_->setNum(slider_->value());
        }
    }

    void syncText()
    {
        const QString name = RibbonModel::line(c_.label);
        const ui::Edition need = owner_.need(c_);
        const bool locked = RibbonControls::locked(need);
        label_->setText(name);
        if (field_) {
            field_->setLabel(name);
            field_->setLocked(locked);
        } else {
            slider_->setAccessibleName(name);
            slider_->setLocked(locked);
        }
        QWidget* input = field_ ? static_cast<QWidget*>(field_) : slider_;
        input->setAccessibleDescription(locked ? EditionGate::availableIn(need) : QString());
        for (QWidget* w : {static_cast<QWidget*>(label_), input})
            ui::RichTooltip::attach(w, {.name = name, .command = c_.sysvar});
    }

    void applyColor() // .rb-num: nhãn --c-text-2
    {
        QPalette p = label_->palette();
        p.setColor(QPalette::WindowText, col(tok::c_text_2));
        label_->setPalette(p);
    }

    Control c_;
    RibbonControls& owner_;
    QLabel* label_;
    ui::NumericField* field_ = nullptr;
    ui::Slider* slider_ = nullptr;
    QLabel* shown_ = nullptr;
};

// Thân panel: hàng (.rb-row, khe 1 px), cột (.rb-col, canh trên, khe 1 px; cột có ô nhập khe --sp-1 như col(…, 3)).
QWidget* build(RibbonControls& o, const Item& it, QWidget* parent)
{
    if (it.kind == Item::Kind::Control)
        return o.create(it.control, parent);
    auto* w = new QWidget(parent);
    QBoxLayout* box = it.kind == Item::Kind::Row ? static_cast<QBoxLayout*>(new QHBoxLayout(w)) : new QVBoxLayout(w);
    box->setContentsMargins({});
    const bool inputs = std::ranges::any_of(it.items, [](const Item& x) {
        return x.kind == Item::Kind::Control && x.control.type != ControlType::Button &&
               x.control.type != ControlType::Split && x.control.type != ControlType::Toggle;
    });
    box->setSpacing(it.kind == Item::Kind::Column && inputs ? px(tok::sp_1) : kBorder);
    for (const Item& x : it.items)
        box->addWidget(build(o, x, w), 0, it.kind == Item::Kind::Column ? Qt::AlignLeft : Qt::Alignment());
    if (it.kind == Item::Kind::Column)
        box->addStretch();
    return w;
}

} // namespace

// ---------------------------------------------------------------------------------------------------- RibbonControls

RibbonControls::RibbonControls(actions::ActionRegistry& registry, settings::Store& store, QObject* parent)
    : QObject(parent), registry_(registry), store_(store)
{
}

QWidget* RibbonControls::create(const Control& c, QWidget* parent)
{
    QWidget* w = nullptr;
    switch (c.type) {
    case ControlType::Button:
    case ControlType::Split:
    case ControlType::Toggle:
        w = new RibbonButton(c, *this, parent);
        break;
    case ControlType::DropDown:
        w = new RibbonDropDown(c, *this, parent);
        break;
    case ControlType::Gallery:
        w = new RibbonGallery(c, *this, parent);
        break;
    case ControlType::Number:
    case ControlType::Slider:
        w = new RibbonValue(c, *this, parent);
        break;
    }
    w->setObjectName(c.id);
    return w;
}

QWidget* RibbonControls::buildPanel(const Panel& p, QWidget* parent)
{
    auto* w = new QWidget(parent);
    w->setObjectName(p.id);
    auto* box = new QHBoxLayout(w);
    box->setContentsMargins({});
    box->setSpacing(px(tok::sp_1)); // .rb-c gap 2px
    for (const Item& it : p.items)
        box->addWidget(build(*this, it, w), 0, Qt::AlignTop);
    return w;
}

void RibbonControls::setModel(const QString& source, QAbstractItemModel* model)
{
    models_.insert(source, model);
    emit sourceChanged(source);
}

void RibbonControls::setCurrent(const QString& source, int row)
{
    current_.insert(source, row);
    emit sourceChanged(source);
}

void RibbonControls::setDataState(const QString& source, DataState state)
{
    states_.insert(source, state);
    emit sourceChanged(source);
}

void RibbonControls::setValue(const QString& sysvar, double value)
{
    values_.insert(sysvar, value);
    emit valueChanged(sysvar, value);
}

ui::Edition RibbonControls::need(const Control& c) const
{
    if (c.need == u"PRO")
        return ui::Edition::Pro;
    if (c.need == u"VP")
        return ui::Edition::ViewerPro;
    return c.cmd.isEmpty() ? ui::Edition::None : registry_.need(c.cmd);
}

bool RibbonControls::locked(ui::Edition need)
{
    const EditionGate* g = EditionGate::instance();
    return g && g->locked(need);
}

void RibbonControls::run(const QString& cmd, const QString& args, const QString& name)
{
    QAction* a = registry_.action(cmd);
    if (!a)
        return; // ribbon_lint_test: mọi lệnh của ribbon có trong registry
    if (args.isEmpty())
        return a->trigger(); // cổng phiên bản, chỗ nối INT-002 của ActionRegistry
    if (const ui::Edition need = registry_.need(cmd); locked(need)) {
        EditionGate::instance()->upgrade().request({cmd, need, name});
        return;
    }
    // ponytail: tham số là mã lựa chọn của prototype ghép sau tên lệnh ("^C^C_CIRCLE D"); INT-001 thay bằng macro
    // của từng biến thể (từ khóa lựa chọn thật, "\" chờ nhập điểm).
    emit macroRequested(registry_.macro(cmd) + u' ' + args);
}

void RibbonControls::commit(const Control& c, double value)
{
    if (locked(need(c)))
        return upgrade(c);
    emit macroRequested(u"'%1 %2"_s.arg(c.sysvar, QString::number(value, 'g', 15))); // dấu chấm thập phân
}

void RibbonControls::upgrade(const Control& c)
{
    if (EditionGate* g = EditionGate::instance())
        g->upgrade().request({c.cmd.isEmpty() ? c.id : c.cmd, need(c), RibbonModel::line(c.label)});
}

QString RibbonControls::choice(const QString& id) const
{
    return store_.get(kSplitChoice).value(id).toString();
}

void RibbonControls::remember(const QString& id, const QString& key)
{
    QJsonObject o = store_.get(kSplitChoice);
    if (o.value(id).toString() == key)
        return;
    o.insert(id, key);
    store_.set(kSplitChoice, o);
}

QString RibbonControls::commandIcon(const QString& cmd)
{
    const actions::CommandInfo* c = commandInfo(cmd);
    return c ? QString::fromLatin1(c->icon) : QString();
}

// ponytail: mô tả là nhãn lệnh của danh mục; INT-001 thay bằng nội dung trợ giúp riêng kèm ảnh minh họa (07.1 §7.1).
QString RibbonControls::describe(const QString& cmd, const QString& name)
{
    const actions::CommandInfo* c = commandInfo(cmd);
    const QString d = c ? QCoreApplication::translate("actions", c->label) : QString();
    return d == name ? QString() : d;
}

// ---------------------------------------------------------------------------------------------------- RibbonButton

namespace {

QFont labelFont(ControlSize s) // .rb-lg span --fs-s; .rb-s chữ thân --fs
{
    return theme::font(s == ControlSize::Large ? TextRole::Label : TextRole::Ui);
}

// Nhãn nút lớn: theo '\n' của nhãn; bản dịch không có '\n' mà rộng hơn --lg-w thì chia ở dấu cách cho hai dòng
// cân nhất (.rb-lg max-width của prototype xuống dòng).
QStringList largeLines(const QString& s, const QFontMetrics& fm)
{
    if (s.contains(u'\n'))
        return s.split(u'\n');
    if (fm.horizontalAdvance(s) <= px(tok::lg_w) - 2 * px(tok::sp_2) || !s.contains(u' '))
        return {s};
    qsizetype best = -1;
    int bestW = INT_MAX;
    for (qsizetype i = s.indexOf(u' '); i >= 0; i = s.indexOf(u' ', i + 1))
        if (const int w = std::max(fm.horizontalAdvance(s.left(i)), fm.horizontalAdvance(s.mid(i + 1))); w < bestW) {
            bestW = w;
            best = i;
        }
    return {s.left(best), s.mid(best + 1)};
}

int lineStep(const QFont& f)
{
    return qRound(QFontInfo(f).pixelSize() * kLineHeight);
}

// Khoảng của .rb-s: lề trái --sp-2, phải 6 px (--sp-3; .io 4 px), khe 5 px (--sp-2 + 1).
int gap()
{
    return px(tok::sp_2) + kBorder;
}

} // namespace

RibbonButton::RibbonButton(const Control& c, RibbonControls& owner, QWidget* parent)
    : QAbstractButton(parent), c_(c), owner_(owner), action_(owner.registry().action(c.cmd))
{
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::TabFocus); // bấm chuột không lấy focus của vùng vẽ, dòng lệnh
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    if (action_) {
        setEnabled(action_->isEnabled());
        connect(action_, &QAction::enabledChanged, this, &QWidget::setEnabled);
        if (c.type == ControlType::Toggle) {
            setCheckable(true);
            setChecked(action_->isChecked());
            connect(action_, &QAction::toggled, this, &QAbstractButton::setChecked); // Sysvars::changed, đồng bộ
        }
    } else {
        setEnabled(false);
    }
    if (c.type == ControlType::Split && c.source.isEmpty())
        choice_ = owner.choice(c.id);
    connect(this, &QAbstractButton::clicked, this, &RibbonButton::activate);
    onEdition(this, [this] { sync(); });
    sync();
}

QMenu* RibbonButton::menu()
{
    if (!menu_ && c_.type == ControlType::Split) { // dựng khi mở lần đầu: mở tab nhanh hơn
        menu_ = new ui::Menu(this);
        connect(menu_, &QMenu::aboutToShow, this, &RibbonButton::fillMenu);
        connect(menu_, &QMenu::aboutToHide, this, [this] {
            menuOpen_ = false;
            update();
        });
    }
    return menu_;
}

MenuItem RibbonButton::mainItem() const
{
    const MenuItem own{.cmd = c_.cmd, .args = c_.args};
    if (c_.type != ControlType::Split || !c_.source.isEmpty())
        return own;
    const auto find = [&](const QString& key) {
        return std::ranges::find_if(c_.menu, [&](const MenuItem& m) { return !m.separator && m.key() == key; });
    };
    if (auto it = find(choice_); !choice_.isEmpty() && it != c_.menu.end())
        return *it;
    if (auto it = find(own.key()); it != c_.menu.end())
        return *it;
    return own;
}

QRect RibbonButton::arrowZone() const
{
    if (c_.type != ControlType::Split)
        return {};
    if (c_.menuOnly)
        return rect();
    if (c_.size == ControlSize::Large) {
        const int top = qRound(height() * kArrowSplit);
        return {0, top, width(), height() - top};
    }
    const int w = kChevron + px(drawSize(compact_) == ControlSize::Icon ? tok::sp_2 : tok::sp_3) + gap() / 2;
    return QStyle::visualRect(layoutDirection(), rect(), QRect(width() - w, 0, w, height()));
}

QRect RibbonButton::mainZone() const
{
    const QRect a = arrowZone();
    if (a.isNull())
        return rect();
    if (a == rect())
        return {};
    return c_.size == ControlSize::Large
               ? QRect(0, 0, width(), a.top())
               : QStyle::visualRect(layoutDirection(), rect(), QRect(0, 0, width() - a.width(), height()));
}

ControlSize RibbonButton::drawSize(bool compact) const
{
    return compact && c_.size == ControlSize::Small ? ControlSize::Icon : c_.size;
}

void RibbonButton::setCompact(bool compact)
{
    if (compact == compact_)
        return;
    compact_ = compact;
    updateGeometry();
    update();
}

QSize RibbonButton::sizeFor(bool compact) const
{
    ensurePolished();
    const ControlSize sz = drawSize(compact);
    const QFont f = labelFont(c_.size);
    const QFontMetrics fm(f);
    const bool split = c_.type == ControlType::Split;
    const ui::Edition need = owner_.need(c_);
    const QSize badge = locked_ ? LockBadge::size(need, LockBadge::Style::Ribbon) : QSize();
    if (c_.size == ControlSize::Large) {
        const QStringList lines = largeLines(RibbonModel::text(c_.label), fm);
        int w = 0;
        for (qsizetype i = 0; i < lines.size(); ++i)
            w = std::max(w, fm.horizontalAdvance(lines[i]) +
                                (split && i == lines.size() - 1 ? px(tok::sp_1) + kChevron : 0));
        const int pad = px(tok::sp_2), top = px(tok::sp_1) + kBorder; // .rb-lg padding 3px 4px 0, khe 3px
        const int h = top + px(tok::ic_lg) + top + int(lines.size() - 1) * lineStep(f) + fm.height();
        return {std::max(px(tok::lg_w), w + 2 * pad), std::max(h, 3 * px(tok::h_btn) + 2 * kBorder)};
    }
    int w = px(tok::sp_2) + px(tok::ic) + px(sz == ControlSize::Icon ? tok::sp_2 : tok::sp_3);
    if (sz == ControlSize::Small)
        w += gap() + fm.horizontalAdvance(text());
    if (split)
        w += gap() + kChevron;
    if (locked_)
        w += px(tok::sp_1) + badge.width();
    return {w, px(tok::h_btn)};
}

void RibbonButton::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool en = isEnabled(), hover = en && underMouse(), on = isChecked();
    const qreal rad = tm().px(tok::r);
    const auto fill = [&](const QRect& r, tok::ColorId c) {
        p.setPen(Qt::NoPen);
        p.setBrush(col(c));
        p.drawRoundedRect(QRectF(r), rad, rad);
    };

    // Nền: bật --c-on-bg; rê --c-hover; nhấn, danh sách đang mở --c-press ở phần đang bấm (.rb-lg/.rb-s).
    if (on)
        fill(rect(), tok::c_on_bg);
    else if (hover || menuOpen_)
        fill(rect(), tok::c_hover);
    if (menuOpen_)
        fill(arrowZone(), tok::c_press);
    else if (en && isDown())
        fill(mainZone(), tok::c_press);
    if (hover && !mainZone().isEmpty() && !arrowZone().isNull()) { // split: nét chia giữa hai phần khi rê
        const QRect a = arrowZone();
        p.fillRect(c_.size == ControlSize::Large
                       ? QRect(0, a.top(), width(), kBorder)
                       : QRect(a.left(), px(tok::sp_1), kBorder, height() - 2 * px(tok::sp_1)),
                   col(tok::c_border));
    }
    if (focusVisible(this))
        drawRing(p, rect(), col(tok::c_focus));

    const QColor ink = col(!en ? tok::c_text_dis : locked_ ? tok::c_text_2 : on ? tok::c_on : tok::c_text);
    const QColor faint = col(en ? tok::c_text_3 : tok::c_text_dis); // .rb-s .ic.xs
    const qreal dpr = p.device()->devicePixelRatio();
    const MenuItem main = mainItem(); // split: biểu tượng theo lựa chọn gần nhất
    QString icon = main.icon;
    if (icon.isEmpty() && main.cmd != c_.cmd)
        icon = RibbonControls::commandIcon(main.cmd);
    if (icon.isEmpty())
        icon = c_.icon.isEmpty() ? RibbonControls::commandIcon(c_.cmd) : c_.icon;
    const ui::Edition need = owner_.need(c_);
    const QSize badge = locked_ ? LockBadge::size(need, LockBadge::Style::Ribbon) : QSize();
    const QFont f = labelFont(c_.size);
    const QFontMetrics fm(f);
    p.setFont(f);

    const auto drawIcon = [&](int x, int y, int size) {
        p.save();
        if (locked_)
            p.setOpacity(kLockedOpacity);
        p.drawPixmap(x, y, glyph(icon, size, dpr, ink));
        p.restore();
    };

    if (c_.size == ControlSize::Large) {
        const int top = px(tok::sp_1) + kBorder, ic = px(tok::ic_lg);
        drawIcon((width() - ic) / 2, top, ic);
        const QStringList lines = largeLines(RibbonModel::text(c_.label), fm);
        int y = top + ic + top;
        p.setPen(ink);
        for (qsizetype i = 0; i < lines.size(); ++i, y += lineStep(f)) {
            const bool last = i == lines.size() - 1, chevron = last && c_.type == ControlType::Split;
            const int tw = fm.horizontalAdvance(lines[i]);
            const int x = (width() - tw - (chevron ? px(tok::sp_1) + kChevron : 0)) / 2;
            p.drawText(QRect(x, y, tw, fm.height()), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, lines[i]);
            if (chevron)
                drawChevron(p, QPointF(x + tw + px(tok::sp_1) + kChevron / 2.0, y + fm.height() / 2.0), faint);
        }
        if (locked_) // .rb-lg .lkb: góc trên phải
            LockBadge::paint(p, QRect(QPoint(width() - badge.width(), kBorder), badge), need, LockBadge::Style::Ribbon);
        return;
    }

    const int ic = px(tok::ic);
    int x = px(tok::sp_2);
    drawIcon(x, (height() - ic) / 2, ic);
    x += ic;
    if (drawSize(compact_) == ControlSize::Small) {
        x += gap();
        const int tw = fm.horizontalAdvance(text());
        p.setPen(ink);
        p.drawText(QRect(x, 0, tw, height()), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text());
        x += tw;
    }
    if (c_.type == ControlType::Split) {
        x += gap();
        drawChevron(p, QPointF(x + kChevron / 2.0, height() / 2.0), faint);
        x += kChevron;
    }
    if (locked_) // .rb-s .lkb: sau nhãn, cách --sp-1
        LockBadge::paint(p, QRect(QPoint(x + px(tok::sp_1), (height() - badge.height()) / 2), badge), need,
                         LockBadge::Style::Ribbon);
}

void RibbonButton::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && arrowZone().contains(e->position().toPoint()))
        return showMenu();
    QAbstractButton::mousePressEvent(e);
}

void RibbonButton::keyPressEvent(QKeyEvent* e)
{
    const Qt::KeyboardModifiers mods = e->modifiers() & ~Qt::KeypadModifier;
    if (c_.type == ControlType::Split && e->key() == Qt::Key_Down &&
        (mods == Qt::NoModifier || mods == Qt::AltModifier))
        return showMenu();
    if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && mods == Qt::NoModifier)
        return click(); // như Space
    QAbstractButton::keyPressEvent(e);
}

void RibbonButton::changeEvent(QEvent* e)
{
    switch (e->type()) {
    case QEvent::LanguageChange:
        sync();
        break;
    case QEvent::StyleChange: // mật độ: --h-btn
    case QEvent::FontChange:
        updateGeometry();
        update();
        break;
    case QEvent::PaletteChange:
    case QEvent::EnabledChange:
        update();
        break;
    default:
        break;
    }
    QAbstractButton::changeEvent(e);
}

void RibbonButton::nextCheckState()
{
    // Toggle: trạng thái chỉ theo QAction (sysvar); bấm chỉ gửi lệnh.
}

void RibbonButton::showMenu()
{
    if (c_.type != ControlType::Split || !isEnabled())
        return;
    if (locked_)
        return click(); // khóa: mũi tên cũng mở trang nâng cấp
    menuOpen_ = true;
    update();
    // ponytail: mở dưới mép trái; RTL, hết chỗ phía dưới do QMenu tự dời trong màn hình.
    menu()->popup(mapToGlobal(QPoint(0, height())));
}

void RibbonButton::activate()
{
    if (c_.type == ControlType::Toggle) {
        if (action_)
            action_->trigger(); // ActionRegistry: cổng phiên bản, 'SYSVAR giá-trị-mới tới sink
        return;
    }
    if (c_.menuOnly)
        return locked_ ? owner_.upgrade(c_) : showMenu();
    const MenuItem m = mainItem();
    owner_.run(m.cmd, m.args, text());
}

void RibbonButton::fillMenu()
{
    menu_->clear();
    if (!c_.source.isEmpty()) { // danh sách theo bản vẽ (block, view): mục chọn → picked
        const QString msg = dataMessage(owner_, c_.source);
        if (!msg.isEmpty())
            return menu_->addAction(msg)->setEnabled(false);
        QAbstractItemModel* m = owner_.model(c_.source);
        for (int r = 0; r < std::min(m->rowCount(), kMenuRows); ++r) {
            const QModelIndex i = m->index(r, 0);
            connect(menu_->addAction(toIcon(i.data(Qt::DecorationRole)), i.data().toString()), &QAction::triggered,
                    this, [this, r] { emit owner_.picked(c_.source, r); });
        }
        return;
    }
    for (const MenuItem& m : c_.menu) {
        if (m.separator) {
            menu_->addSeparator();
            continue;
        }
        const QString icon = m.icon.isEmpty() ? RibbonControls::commandIcon(m.cmd) : m.icon;
        QAction* a = menu_->addAction(icon.isEmpty() ? QIcon() : IconProvider::icon(icon), RibbonModel::line(m.label));
        const ui::Edition need = owner_.registry().need(m.cmd);
        ui::RichTooltip::attach(a, {.command = m.cmd,
                                    .keyId = keyId(owner_.registry(), m.cmd),
                                    .description = RibbonControls::describe(m.cmd, a->text()),
                                    .need = RibbonControls::locked(need) ? need : ui::Edition::None});
        connect(a, &QAction::triggered, this, [this, m, need] {
            if (!RibbonControls::locked(need)) {
                choice_ = m.key();
                owner_.remember(c_.id, choice_);
            }
            owner_.run(m.cmd, m.args, RibbonModel::line(m.label));
            sync();
        });
    }
}

void RibbonButton::sync()
{
    const QString name = RibbonModel::line(c_.label);
    setText(name);
    const ui::Edition need = owner_.need(c_);
    locked_ = RibbonControls::locked(need);
    const MenuItem m = mainItem();
    const QString detail = m.label.isEmpty() ? QString() : RibbonModel::line(m.label);
    setAccessibleDescription(locked_ ? EditionGate::availableIn(need) : detail);
    const QString shown = detail.isEmpty() ? name : u"%1: %2"_s.arg(name, detail);
    ui::RichTooltip::attach(this, {.name = shown,
                                   .command = m.cmd,
                                   .keyId = keyId(owner_.registry(), m.cmd),
                                   .description = RibbonControls::describe(m.cmd, detail.isEmpty() ? name : detail),
                                   .need = locked_ ? need : ui::Edition::None});
    updateGeometry();
    update();
}

// ---------------------------------------------------------------------------------------------------- RibbonPages

RibbonPages::RibbonPages(int count, std::function<QWidget*(int)> build, QWidget* parent)
    : QStackedWidget(parent), build_(std::move(build)), pages_(count, nullptr)
{
}

void RibbonPages::setCurrentTab(int index)
{
    if (index < 0 || index >= pages_.size())
        return;
    if (!pages_[index]) {
        pages_[index] = build_(index);
        addWidget(pages_[index]);
    }
    setCurrentWidget(pages_[index]);
    current_ = index;
}

} // namespace trion::ribbon
