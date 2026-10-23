#include "app/dialogs/plot/PlotDialog.h"

#include "app/core/edition/EditionGate.h"
#include "app/core/edition/LockBadge.h"
#include "app/dialogs/plot/PlotRunner.h"
#include "app/dialogs/plot/PlotSettingsModel.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/numeric/NumericField.h"
#include "core/regen/numfmt/NumFormat.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QEvent>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QScreen>
#include <QSignalBlocker>

using namespace Qt::StringLiterals;

namespace trion::ui {

namespace {

using plot::AreaKind;
using theme::TextRole;
using Option = PlotSettingsModel::Option;

// Cỡ của prototype không có token riêng (SC.plot: hộp 700 px, mở rộng thêm cột 260 px; canvas xem trước 400 × 260).
constexpr int kMainW = 672;   // 700 trừ lề thân 2 × 14 (DialogFrame .dlg-b)
constexpr int kMoreW = 260;   // cột mở rộng
constexpr int kPreviewW = 320; // chỗ xem trước nhỏ: tỉ lệ 400 × 260 của canvas, co theo bề rộng nhóm
constexpr int kPngDpi = 300;  // ponytail: PNG của TrionCAD in 300 DPI cố định; PLT-007 cho chọn DPI

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
void tint(QWidget* w, tok::ColorId id)
{
    QPalette pal = w->palette();
    pal.setColor(QPalette::WindowText, col(id));
    w->setPalette(pal);
}

// .grp: viền --c-border, bo --r-lg.
class GroupBox final : public QWidget {
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(col(tok::c_border));
        p.setBrush(Qt::NoBrush);
        const qreal r = tm().px(tok::r_lg);
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), r, r);
    }
};

// Chỗ xem trước nhỏ (canvas #pl-mini): nền --c-sunken bo --r, cao theo tỉ lệ 260/400 của bề rộng. UIH-002 đặt
// PlotMiniPreview vào layout của nó.
class PreviewSlot final : public QWidget {
public:
    PreviewSlot()
    {
        auto* box = new QVBoxLayout(this);
        box->setContentsMargins({});
        QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sp.setHeightForWidth(true);
        setSizePolicy(sp);
    }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return w / 2; } // ponytail: thấp hơn 260/400 của prototype để hộp vừa 1366 × 768
    QSize sizeHint() const override { return {kPreviewW, heightForWidth(kPreviewW)}; }
    QSize minimumSizeHint() const override { return {0, 0}; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(col(tok::c_sunken));
        const qreal r = tm().px(tok::r);
        p.drawRoundedRect(QRectF(rect()), r, r);
    }
};

// Số của giấy, tỉ lệ: RGN-013, dấu chấm, bỏ số 0 cuối (841, 215.9).
QString number(double v, int prec)
{
    regen::UnitContext u;
    u.luprec = prec;
    u.dimzin = 8;
    const std::string s = regen::linearString(v, u);
    return QString::fromUtf8(s.data(), qsizetype(s.size()));
}

ValueSpec spec(ValueKind kind, int luprec, bool trimZeros)
{
    ValueSpec s;
    s.kind = kind;
    s.units.luprec = luprec;
    s.units.dimzin = trimZeros ? 8 : 0;
    return s;
}

// Ô chọn: dựng lại chỉ khi danh sách (chữ, dữ liệu) đổi, rồi chọn mục có dữ liệu current; không phát tín hiệu.
using Items = QList<std::pair<QString, QVariant>>;
void fill(QComboBox* box, const Items& items, const QVariant& current)
{
    const QSignalBlocker block(box);
    bool same = box->count() == items.size();
    for (int i = 0; same && i < items.size(); ++i)
        same = box->itemText(i) == items[i].first && box->itemData(i) == items[i].second;
    if (!same) {
        box->clear();
        for (const auto& [text, data] : items)
            box->addItem(text, data);
    }
    box->setCurrentIndex(box->findData(current));
}

void check(QCheckBox* box, bool on, bool enabled = true)
{
    const QSignalBlocker block(box);
    box->setChecked(on);
    box->setEnabled(enabled);
}

// Giá trị của model vào ô số; ô đang báo lỗi giữ chữ người dùng gõ (Esc trả lại).
void showValue(NumericField* f, double v, bool enabled = true)
{
    if (!f->hasError() && f->value() != v)
        f->setValue(v);
    f->setEnabled(enabled);
}

QString areaLabel(AreaKind k)
{
    switch (k) {
    case AreaKind::Layout: return PlotDialog::tr("Layout");
    case AreaKind::Extents: return PlotDialog::tr("Toàn bộ đối tượng");
    case AreaKind::Display: return PlotDialog::tr("Màn hình hiện tại");
    case AreaKind::Limits: return PlotDialog::tr("Giới hạn bản vẽ");
    case AreaKind::View: return PlotDialog::tr("View đã đặt tên");
    case AreaKind::Window: return PlotDialog::tr("Cửa sổ");
    }
    return {};
}

constexpr const char* kOptions[PlotSettingsModel::kOptionCount] = {
    QT_TRANSLATE_NOOP("trion::ui::PlotDialog", "In ở nền"),
    QT_TRANSLATE_NOOP("trion::ui::PlotDialog", "In độ dày nét"),
    QT_TRANSLATE_NOOP("trion::ui::PlotDialog", "In độ trong suốt"),
    QT_TRANSLATE_NOOP("trion::ui::PlotDialog", "In theo kiểu in"),
    QT_TRANSLATE_NOOP("trion::ui::PlotDialog", "In Paper space sau cùng"),
    QT_TRANSLATE_NOOP("trion::ui::PlotDialog", "Ẩn đối tượng Paper space"),
    QT_TRANSLATE_NOOP("trion::ui::PlotDialog", "Bật dấu in"),
    QT_TRANSLATE_NOOP("trion::ui::PlotDialog", "Lưu thay đổi vào layout"),
};

} // namespace

PlotDialog::PlotDialog(plot::PlotContext context, PlotRunner* runner, QWidget* parent)
    : DialogFrame(u"plot"_s, parent), model_(nullptr), runner_(runner)
{
    if (runner && !context.previous)
        context.previous = runner->previous();
    model_ = new PlotSettingsModel(std::move(context), this);
    setModal(true);
    setCommand(u"PLOT"_s);
    build();
    connectFields();
    connect(&tm(), &theme::ThemeManager::changed, this, &PlotDialog::restyle);
    if (auto* gate = edition::EditionGate::instance())
        connect(gate, &edition::EditionGate::editionChanged, this, &PlotDialog::syncEdition);
    restyle();
    more_->setVisible(settings::Store::instance().get(kPlotMore));
    retranslate();
    resize(fitted());

    // Máy in hệ thống ở nền (quét chậm: mạng, driver); xong thì cập nhật danh sách, khổ.
    sched::submit(sched::Priority::Normal, [rx = Receiver(this), this] {
        QList<plot::Device> printers = plot::scanPrinters();
        post(rx, [this, printers = std::move(printers)] {
            model_->setSystemPrinters(printers);
            sync();
        });
    });
}

PlotDialog::~PlotDialog() = default;

QLabel* PlotDialog::label(const char* source)
{
    auto* l = new QLabel;
    labels_.append({l, source});
    return l;
}

QLabel* PlotDialog::heading(const char* source)
{
    auto* l = new QLabel;
    headings_.append({l, source});
    return l;
}

QWidget* PlotDialog::group(const char* title, QLayout* content)
{
    auto* g = new GroupBox;
    auto* box = new QVBoxLayout(g);
    // .grp padding 10px 12px 12px; trên --sp-4 (không có token 10 px) để hộp vừa màn hình 1366 × 768
    box->setContentsMargins(px(tok::sp_5), px(tok::sp_4), px(tok::sp_5), px(tok::sp_5));
    box->setSpacing(px(tok::sp_4));
    box->addWidget(heading(title));
    content->setContentsMargins({});
    box->addLayout(content);
    return g;
}

void PlotDialog::build()
{
    const auto row = [](std::initializer_list<QWidget*> widgets, int grow = -1) { // .hrow
        auto* r = new QHBoxLayout;
        r->setSpacing(px(tok::sp_4));
        int i = 0;
        for (QWidget* w : widgets)
            r->addWidget(w, i++ == grow ? 1 : 0);
        if (grow < 0)
            r->addStretch(1);
        return r;
    };
    const auto grid = [] {
        auto* g = new QGridLayout;
        g->setHorizontalSpacing(px(tok::sp_5));
        g->setVerticalSpacing(px(tok::sp_3));
        g->setColumnStretch(1, 1);
        return g;
    };
    const auto pro = [] { return new edition::LockBadge(Edition::Pro, edition::LockBadge::Style::Tag); };
    const auto named = [](auto* w, const char* name) {
        w->setObjectName(QString::fromLatin1(name));
        return w;
    };

    // Cột chính
    setup_ = named(new DropDown, "setup");
    addSetup_ = named(new Button, "addSetup");
    QWidget* setupGroup = group(QT_TR_NOOP("Thiết lập trang"), row({setup_, addSetup_, pro()}, 0));

    device_ = named(new ComboBox, "device");
    deviceProps_ = named(new Button, "deviceProps");
    where_ = named(new QLabel, "where");
    where_->setWordWrap(true);
    toFile_ = named(new QCheckBox, "toFile");
    deviceMsg_ = named(new FieldMessage(device_), "deviceMsg");
    QGridLayout* dev = grid();
    QLabel* name = label(QT_TR_NOOP("Tên"));
    name->setBuddy(device_);
    dev->addWidget(name, 0, 0);
    dev->addLayout(row({device_, deviceProps_, pro()}, 0), 0, 1);
    dev->addWidget(deviceMsg_, 1, 1);
    dev->addWidget(label(QT_TR_NOOP("Vị trí")), 2, 0);
    dev->addLayout(row({where_, toFile_}, 0), 2, 1);
    QWidget* deviceGroup = group(QT_TR_NOOP("Máy in"), dev);

    paper_ = named(new ComboBox, "paper");
    preview_ = named(new PreviewSlot, "previewSlot");
    copies_ = named(new NumericField(ValueKind::Integer), "copies");
    copies_->setSpec(spec(ValueKind::Integer, 0, false));
    copies_->setLowerBound(0); // 1–999
    copies_->setUpperBound(plot::kMaxCopies);
    QLabel* copiesLabel = label(QT_TR_NOOP("Số bản"));
    copiesLabel->setBuddy(copies_);
    auto* paperBox = new QVBoxLayout;
    paperBox->setSpacing(px(tok::sp_4));
    paperBox->addWidget(paper_);
    paperBox->addLayout(row({copiesLabel, copies_}));
    paperBox->addWidget(preview_);
    paperBox->addStretch(1);

    area_ = named(new DropDown, "area");
    view_ = named(new ComboBox, "view");
    pickWindow_ = named(new Button, "pickWindow");
    pickWindow_->setIconName(u"zwin"_s);
    windowMsg_ = named(new FieldMessage(pickWindow_), "windowMsg");
    auto* areaBox = new QVBoxLayout;
    areaBox->setSpacing(px(tok::sp_4));
    areaBox->addLayout(row({area_, view_, pickWindow_}, 0)); // view có tên hoặc nút chọn cửa sổ cạnh ô vùng in
    areaBox->addWidget(windowMsg_);

    fit_ = named(new QCheckBox, "fit");
    scale_ = named(new DropDown, "scale");
    scaleMm_ = named(new NumericField(ValueKind::Real), "scaleMm");
    scaleUnits_ = named(new NumericField(ValueKind::Real), "scaleUnits");
    for (NumericField* f : {scaleMm_, scaleUnits_}) {
        f->setSpec(spec(ValueKind::Real, 4, true));
        f->setLowerBound(0);
    }
    scaleEq_ = new QLabel;
    scaleUnitName_ = new QLabel;
    scaleInfo_ = named(new QLabel, "scaleInfo");
    scaleInfo_->setWordWrap(true);
    scaleLw_ = named(new QCheckBox, "scaleLineweights");
    auto* scaleBox = new QVBoxLayout;
    scaleBox->setSpacing(px(tok::sp_4));
    customRow_ = named(new QWidget, "customScale"); // Tùy chỉnh: thay dòng thông tin, hộp không cao thêm
    QHBoxLayout* custom = row({scaleMm_, scaleEq_, scaleUnits_, scaleUnitName_});
    custom->setContentsMargins({});
    customRow_->setLayout(custom);
    scaleBox->addWidget(fit_);
    scaleBox->addWidget(scale_);
    scaleBox->addWidget(customRow_);
    scaleBox->addWidget(scaleInfo_);
    scaleBox->addWidget(scaleLw_);

    center_ = named(new QCheckBox, "center");
    offsetX_ = named(new NumericField(ValueKind::Real), "offsetX");
    offsetY_ = named(new NumericField(ValueKind::Real), "offsetY");
    for (NumericField* f : {offsetX_, offsetY_})
        f->setSpec(spec(ValueKind::Real, 2, false));
    offsetUnit_ = new QLabel;
    QGridLayout* off = grid();
    off->addWidget(center_, 0, 0, 1, 2);
    off->addWidget(label(QT_TR_NOOP("X / Y")), 1, 0);
    off->addLayout(row({offsetX_, offsetY_, offsetUnit_}), 1, 1);

    auto* right = new QVBoxLayout;
    right->setSpacing(px(tok::sp_5));
    right->addWidget(group(QT_TR_NOOP("Vùng in"), areaBox));
    right->addWidget(group(QT_TR_NOOP("Tỉ lệ"), scaleBox));
    right->addWidget(group(QT_TR_NOOP("Offset"), off));
    right->addStretch(1);
    auto* halves = new QHBoxLayout; // .grid2
    halves->setSpacing(px(tok::sp_5));
    QWidget* paperGroup = group(QT_TR_NOOP("Khổ giấy"), paperBox);
    halves->addWidget(paperGroup, 1, Qt::AlignTop);
    halves->addLayout(right, 1);

    auto* main = new QWidget;
    main->setMinimumWidth(kMainW);
    auto* mainBox = new QVBoxLayout(main);
    mainBox->setContentsMargins({});
    mainBox->setSpacing(px(tok::sp_5));
    mainBox->addWidget(setupGroup);
    mainBox->addWidget(deviceGroup);
    mainBox->addLayout(halves);
    mainBox->addStretch(1);

    // Cột mở rộng
    plotStyle_ = named(new DropDown, "plotStyle");
    editStyle_ = named(new Button, "editStyle");
    showStyles_ = named(new QCheckBox, "showPlotStyles");
    auto* styleBox = new QVBoxLayout;
    styleBox->setSpacing(px(tok::sp_4));
    styleBox->addLayout(row({plotStyle_, editStyle_, pro()}, 0));
    styleBox->addWidget(showStyles_);

    shadeMode_ = named(new DropDown, "shadeMode");
    shadeQuality_ = named(new DropDown, "shadeQuality");
    dpi_ = named(new NumericField(ValueKind::Integer), "dpi");
    dpi_->setSpec(spec(ValueKind::Integer, 0, false));
    dpi_->setLowerBound(0);
    dpi_->setUpperBound(plot::kMaxDpi); // dưới kMinDpi: model kẹp
    dpiUnit_ = new QLabel;
    QGridLayout* shade = grid();
    QLabel* shadeLabel = label(QT_TR_NOOP("In shade"));
    shadeLabel->setBuddy(shadeMode_);
    shade->addWidget(shadeLabel, 0, 0);
    shade->addWidget(shadeMode_, 0, 1);
    QLabel* qualityLabel = label(QT_TR_NOOP("Chất lượng"));
    qualityLabel->setBuddy(shadeQuality_);
    shade->addWidget(qualityLabel, 1, 0);
    shade->addLayout(row({shadeQuality_, dpi_, dpiUnit_}, 0), 1, 1);

    auto* optionBox = new QVBoxLayout;
    optionBox->setSpacing(px(tok::sp_2));
    for (int i = 0; i < PlotSettingsModel::kOptionCount; ++i) {
        options_[std::size_t(i)] = new QCheckBox;
        options_[std::size_t(i)]->setObjectName(u"option%1"_s.arg(i));
        if (Option(i) == Option::SaveToLayout)
            optionBox->addLayout(row({options_[std::size_t(i)], pro()}));
        else
            optionBox->addWidget(options_[std::size_t(i)]);
    }

    orientation_ = named(new SegmentedGroup, "orientation");
    orientation_->addSegment({});
    orientation_->addSegment({});
    upsideDown_ = named(new QCheckBox, "upsideDown");
    QHBoxLayout* orientBox = row({orientation_, upsideDown_});

    more_ = named(new QWidget, "more");
    more_->setMinimumWidth(kMoreW);
    auto* moreBox = new QVBoxLayout(more_);
    moreBox->setContentsMargins({});
    moreBox->setSpacing(px(tok::sp_5));
    moreBox->addWidget(group(QT_TR_NOOP("Kiểu in (Plot style)"), styleBox));
    moreBox->addWidget(group(QT_TR_NOOP("Viewport có shade"), shade));
    moreBox->addWidget(group(QT_TR_NOOP("Tùy chọn in"), optionBox));
    moreBox->addWidget(group(QT_TR_NOOP("Hướng giấy"), orientBox));
    moreBox->addStretch(1);

    auto* body = new QWidget;
    auto* columns = new QHBoxLayout(body);
    columns->setContentsMargins({});
    columns->setSpacing(px(tok::sp_6));
    columns->addWidget(main, 1);
    columns->addWidget(more_);
    setBody(body);

    // Chân: [Xem trước…][Thêm tùy chọn ▸] ··· [Áp dụng cho layout Pro][Hủy][In]
    previewButton_ = named(new Button, "preview");
    previewButton_->setIconName(u"preview"_s);
    moreButton_ = named(new Button({}, Button::Kind::Ghost), "moreOptions");
    footer()->insertWidget(0, previewButton_);
    footer()->insertWidget(1, moreButton_);
    apply_ = named(new Button, "apply");
    footer()->addWidget(apply_);
    footer()->addWidget(pro());
    addCancel();
}

void PlotDialog::connectFields()
{
    // Widget → model → đồng bộ lại mọi widget (model có thể kẹp, đổi trường liên đới, hoặc từ chối).
    const auto edit = [this](auto apply) {
        return [this, apply](auto&&... args) {
            if (syncing_)
                return;
            apply(std::forward<decltype(args)>(args)...);
            sync();
        };
    };
    const auto data = [](QComboBox* box) { return box->currentData(); };

    connect(setup_, &QComboBox::currentIndexChanged, this, edit([this, data](int) {
                customScale_ = false;
                model_->selectSetup(data(setup_).toString());
            }));
    connect(device_, &QComboBox::currentIndexChanged, this,
            edit([this, data](int) { model_->setDevice(data(device_).toString()); }));
    connect(toFile_, &QCheckBox::toggled, this, edit([this](bool on) { model_->setToFile(on); }));
    connect(paper_, &QComboBox::currentIndexChanged, this,
            edit([this, data](int) { model_->setPaper(data(paper_).toString()); }));
    connect(area_, &QComboBox::currentIndexChanged, this,
            edit([this, data](int) { model_->setArea(AreaKind(data(area_).toInt())); }));
    connect(view_, &QComboBox::currentIndexChanged, this,
            edit([this, data](int) { model_->setView(data(view_).toString()); }));
    connect(pickWindow_, &QAbstractButton::clicked, this, &PlotDialog::startWindowPick);
    connect(fit_, &QCheckBox::toggled, this, edit([this](bool on) { model_->setFit(on); }));
    connect(scale_, &QComboBox::currentIndexChanged, this, edit([this, data](int) {
                const int i = data(scale_).toInt();
                customScale_ = i < 0;
                if (i >= 0)
                    model_->setStandardScale(i);
                else
                    model_->setFit(false);
            }));
    const auto custom = edit([this](double) { model_->setCustomScale(scaleMm_->value(), scaleUnits_->value()); });
    connect(scaleMm_, &NumericField::valueCommitted, this, custom);
    connect(scaleUnits_, &NumericField::valueCommitted, this, custom);
    connect(scaleLw_, &QCheckBox::toggled, this, edit([this](bool on) { model_->setScaleLineweights(on); }));
    connect(center_, &QCheckBox::toggled, this, edit([this](bool on) { model_->setCentered(on); }));
    const auto offset = edit([this](double) { model_->setOffset(offsetX_->value(), offsetY_->value()); });
    connect(offsetX_, &NumericField::valueCommitted, this, offset);
    connect(offsetY_, &NumericField::valueCommitted, this, offset);
    connect(copies_, &NumericField::valueCommitted, this, edit([this](double v) { model_->setCopies(qRound(v)); }));
    connect(plotStyle_, &QComboBox::currentIndexChanged, this,
            edit([this, data](int) { model_->setPlotStyle(data(plotStyle_).toString()); }));
    connect(showStyles_, &QCheckBox::toggled, this, edit([this](bool on) { model_->setShowPlotStyles(on); }));
    connect(shadeMode_, &QComboBox::currentIndexChanged, this,
            edit([this, data](int) { model_->setShadeMode(plot::ShadeMode(data(shadeMode_).toInt())); }));
    connect(shadeQuality_, &QComboBox::currentIndexChanged, this,
            edit([this, data](int) { model_->setShadeQuality(plot::ShadeQuality(data(shadeQuality_).toInt())); }));
    connect(dpi_, &NumericField::valueCommitted, this, edit([this](double v) { model_->setShadeDpi(qRound(v)); }));
    for (int i = 0; i < PlotSettingsModel::kOptionCount; ++i)
        connect(options_[std::size_t(i)], &QCheckBox::toggled, this, edit([this, i](bool on) {
                    const auto o = Option(i);
                    if (o == Option::SaveToLayout && on
                        && upgradeIfLocked(u"pagesetup-save"_s, tr("Lưu thay đổi vào layout")))
                        return; // Viewer: không bật, mở trang nâng cấp
                    model_->setOption(o, on);
                }));
    connect(orientation_, &SegmentedGroup::currentIndexChanged, this, edit([this](int i) {
                model_->setOrientation(i == 0 ? plot::Orientation::Portrait : plot::Orientation::Landscape);
            }));
    connect(upsideDown_, &QCheckBox::toggled, this, edit([this](bool on) { model_->setUpsideDown(on); }));

    // Phần Pro: Viewer, Viewer Pro mở trang nâng cấp; Pro phát yêu cầu cho integration (lệnh ghi bản vẽ, màn hình khác).
    connect(addSetup_, &QAbstractButton::clicked, this, [this] {
        if (!upgradeIfLocked(u"pagesetup-add"_s, tr("Lưu thiết lập trang vào bản vẽ")))
            emit addSetupRequested(model_->settings().setup);
    });
    connect(deviceProps_, &QAbstractButton::clicked, this, [this] {
        if (!upgradeIfLocked(u"PLOTTERMANAGER"_s, tr("Cấu hình máy in (PC3)")))
            emit printerPropertiesRequested(model_->settings().setup.device);
    });
    connect(editStyle_, &QAbstractButton::clicked, this, [this] {
        if (!upgradeIfLocked(u"STYLESMANAGER"_s, tr("Sửa kiểu in (Plot style)")))
            emit editPlotStyleRequested(model_->settings().setup.plotStyle);
    });
    connect(apply_, &QAbstractButton::clicked, this, [this] {
        if (!upgradeIfLocked(u"pagesetup-apply"_s, tr("Lưu thiết lập in vào layout")))
            emit applyToLayoutRequested(model_->settings().setup);
    });
    connect(previewButton_, &QAbstractButton::clicked, this, [this] { emit previewRequested(model_->settings()); });
    connect(moreButton_, &QAbstractButton::clicked, this, [this] { setExpanded(!isExpanded()); });
}

void PlotDialog::sync()
{
    if (!apply_)
        return;
    syncing_ = true;
    const plot::PlotSettings& s = model_->settings();
    const plot::PageSetup& p = s.setup;
    const plot::PlotContext& ctx = model_->context();

    Items items{{tr("<Không>"), PlotSettingsModel::kNoSetup}};
    if (ctx.previous)
        items.append({tr("<Lần in trước>"), PlotSettingsModel::kPreviousSetup});
    for (const plot::PageSetup& n : ctx.namedSetups)
        items.append({n.name, n.name});
    fill(setup_, items, model_->setup());

    items.clear();
    for (const plot::Device& d : model_->devices())
        items.append({deviceLabel(d), d.name});
    const plot::Device* dev = model_->device();
    if (!dev)
        items.append({p.device, p.device}); // máy in vắng: vẫn hiện tên đang đặt
    fill(device_, items, p.device);
    if (!dev)
        where_->setText(u"—"_s);
    else if (dev->kind == plot::DeviceKind::Pdf)
        where_->setText(tr("Tệp trên máy · vector, giữ layer"));
    else if (dev->kind == plot::DeviceKind::Png)
        where_->setText(tr("Tệp trên máy · %1 DPI").arg(kPngDpi));
    else
        where_->setText(!dev->location.isEmpty() ? dev->location
                        : !dev->model.isEmpty()  ? dev->model
                                                 : tr("Máy in hệ thống"));
    check(toFile_, s.toFile, !model_->toFileForced());
    if (!dev && model_->printersLoaded())
        deviceMsg_->setError(tr("Không tìm thấy máy in “%1”. Chọn máy in khác hoặc kiểm tra kết nối.").arg(p.device));
    else if (!dev)
        deviceMsg_->setHint(tr("Đang tìm máy in…"));
    else
        deviceMsg_->setError({});
    deviceMsg_->setVisible(!deviceMsg_->text().isEmpty());

    items.clear();
    for (const plot::Paper& q : model_->papers())
        items.append({paperLabel(q), q.name});
    fill(paper_, items, p.paper);

    items.clear();
    for (const AreaKind k : model_->areas())
        items.append({areaLabel(k), int(k)});
    fill(area_, items, int(p.area.kind));
    items.clear();
    for (const QString& v : ctx.views)
        items.append({v, v});
    fill(view_, items, p.area.view);
    view_->setVisible(p.area.kind == AreaKind::View);
    const bool window = p.area.kind == AreaKind::Window;
    pickWindow_->setVisible(window);
    windowMsg_->setVisible(window);
    if (window && p.area.window.isEmpty()) {
        windowMsg_->setError(tr("Chưa chọn cửa sổ in."));
    } else if (window) {
        const auto xy = [&](QPointF pt) {
            const std::string x = regen::linearString(pt.x(), ctx.units), y = regen::linearString(pt.y(), ctx.units);
            return u"(%1, %2)"_s.arg(QString::fromStdString(x), QString::fromStdString(y));
        };
        const QRectF& w = p.area.window; // tọa độ bản vẽ, y lên: góc dưới trái là (left, top) của QRectF chuẩn hóa
        windowMsg_->setHint(tr("%1 – %2").arg(xy(w.topLeft()), xy(w.bottomRight())));
    } else {
        windowMsg_->setError({});
    }

    check(fit_, p.scale.fit);
    items.clear();
    const auto scales = plot::standardScales();
    for (int i = 0; i < int(scales.size()); ++i)
        items.append({u"1:%1"_s.arg(scales[std::size_t(i)]), i});
    items.append({tr("Tùy chỉnh"), -1});
    fill(scale_, items, customScale_ || model_->scaleIndex() < 0 ? -1 : model_->scaleIndex());
    scale_->setEnabled(!p.scale.fit);
    const bool custom = !p.scale.fit && (customScale_ || model_->scaleIndex() < 0);
    customRow_->setVisible(custom);
    scaleInfo_->setVisible(!custom);
    showValue(scaleMm_, p.scale.mm);
    showValue(scaleUnits_, p.scale.units);
    scaleInfo_->setText(p.scale.fit ? tr("Tỉ lệ tự tính theo vùng in và khổ giấy")
                                    : tr("1 mm = %1 đơn vị bản vẽ").arg(number(p.scale.units / p.scale.mm, 4)));
    check(scaleLw_, p.scale.scaleLineweights);
    check(center_, p.offset.centered, model_->centerEnabled());
    showValue(offsetX_, p.offset.x, !p.offset.centered);
    showValue(offsetY_, p.offset.y, !p.offset.centered);
    showValue(copies_, s.copies, model_->copiesEnabled());

    items = {{tr("Không (in theo màu đối tượng)"), QString()}};
    for (const QString& n : ctx.plotStyles)
        items.append({n, n});
    if (!p.plotStyle.isEmpty() && !ctx.plotStyles.contains(p.plotStyle))
        items.append({p.plotStyle, p.plotStyle});
    fill(plotStyle_, items, p.plotStyle);
    check(showStyles_, p.flags.showPlotStyles);
    fill(shadeMode_, {{tr("Như hiển thị"), 0}, {tr("Wireframe"), 1}, {tr("Ẩn nét"), 2}, {tr("Render"), 3}},
         int(p.shade.mode));
    fill(shadeQuality_,
         {{tr("Bản nháp"), 0}, {tr("Chất lượng thường"), 1}, {tr("Cao nhất"), 2}, {tr("Tùy chỉnh (DPI)"), 3}},
         int(p.shade.quality));
    showValue(dpi_, p.shade.dpi, p.shade.quality == plot::ShadeQuality::Custom);
    for (int i = 0; i < PlotSettingsModel::kOptionCount; ++i)
        check(options_[std::size_t(i)], model_->option(Option(i)), model_->optionEnabled(Option(i)));
    {
        const QSignalBlocker block(orientation_);
        orientation_->setCurrentIndex(p.orientation == plot::Orientation::Portrait ? 0 : 1);
    }
    check(upsideDown_, p.upsideDown);
    if (Button* confirm = confirmButton())
        confirm->setEnabled(model_->canPlot());
    syncing_ = false;
}

void PlotDialog::syncEdition()
{
    // Tooltip 2 mức có tên lệnh tiếng Anh và F1 (prototype data-tip, data-cmd); huy hiệu theo phiên bản.
    const Edition need = proLocked() ? Edition::Pro : Edition::None;
    RichTooltip::attach(deviceProps_, {tr("Thuộc tính máy in"), u"PLOTTERMANAGER"_s, {},
                                       tr("Sửa cấu hình PC3 của máy in đang chọn: cổng, khổ giấy tùy chỉnh, vùng in "
                                          "được, hiệu chỉnh tỉ lệ."),
                                       need});
    RichTooltip::attach(editStyle_, {tr("Sửa kiểu in"), u"STYLESMANAGER"_s, {},
                                     tr("Sửa bảng kiểu in đang chọn: màu in, độ dày nét, kiểu tô."), need});
    RichTooltip::attach(previewButton_, {tr("Xem trước bản in"), u"PREVIEW"_s, {},
                                         tr("Xem tờ giấy với vùng in được và độ dày nét theo kiểu in.")});
    const QString why = proLocked() ? edition::EditionGate::availableIn(Edition::Pro) : QString();
    for (QWidget* w : {static_cast<QWidget*>(addSetup_), static_cast<QWidget*>(deviceProps_),
                       static_cast<QWidget*>(editStyle_), static_cast<QWidget*>(apply_),
                       static_cast<QWidget*>(options_[std::size_t(Option::SaveToLayout)])})
        w->setAccessibleDescription(why);
}

void PlotDialog::restyle()
{
    for (const Text& t : std::as_const(labels_)) { // .frow .lab: --c-text-2, --fs-s
        t.label->setFont(theme::font(TextRole::Label));
        tint(t.label, tok::c_text_2);
    }
    for (const Text& t : std::as_const(headings_)) { // .gh: --fs-xs đậm, chữ hoa, giãn .06em, --c-text-3
        QFont f = theme::font(TextRole::Caption, theme::Weight::Bold);
        f.setCapitalization(QFont::AllUppercase);
        f.setLetterSpacing(QFont::PercentageSpacing, 106);
        t.label->setFont(f);
        tint(t.label, tok::c_text_3);
    }
    where_->setFont(theme::font(TextRole::Label)); // .muted --fs-s
    tint(where_, tok::c_text_2);
    scaleInfo_->setFont(theme::font(TextRole::Caption)); // .faint --fs-xs
    tint(scaleInfo_, tok::c_text_3);
}

void PlotDialog::retranslate()
{
    if (!apply_)
        return;
    const plot::PlotContext& ctx = model_->context();
    setWindowTitle(tr("In · %1").arg(ctx.isLayout() ? ctx.layout : u"Model"_s));
    for (const Text& t : std::as_const(labels_))
        t.label->setText(tr(t.source));
    for (const Text& t : std::as_const(headings_))
        t.label->setText(tr(t.source));
    setup_->setAccessibleName(tr("Thiết lập trang"));
    addSetup_->setText(tr("Thêm…"));
    device_->setAccessibleName(tr("Máy in"));
    deviceProps_->setText(tr("Thuộc tính…"));
    toFile_->setText(tr("In ra tệp"));
    paper_->setAccessibleName(tr("Khổ giấy"));
    preview_->setAccessibleName(tr("Xem trước vị trí in trên giấy"));
    area_->setAccessibleName(tr("Vùng in"));
    view_->setAccessibleName(tr("View đã đặt tên"));
    pickWindow_->setText(tr("Chọn cửa sổ <"));
    fit_->setText(tr("Vừa khổ giấy"));
    scale_->setAccessibleName(tr("Tỉ lệ"));
    scaleMm_->setLabel(tr("Milimét trên giấy"));
    scaleEq_->setText(tr("mm ="));
    scaleUnits_->setLabel(tr("Đơn vị bản vẽ"));
    scaleUnitName_->setText(tr("đơn vị"));
    scaleLw_->setText(tr("Co giãn độ dày nét theo tỉ lệ"));
    center_->setText(tr("Canh giữa"));
    offsetX_->setLabel(tr("Offset X"));
    offsetY_->setLabel(tr("Offset Y"));
    offsetUnit_->setText(tr("mm"));
    copies_->setLabel(tr("Số bản"));
    plotStyle_->setAccessibleName(tr("Kiểu in (Plot style)"));
    editStyle_->setText(tr("Sửa…"));
    editStyle_->setAccessibleName(tr("Sửa kiểu in"));
    showStyles_->setText(tr("Hiện kiểu in trên layout"));
    shadeMode_->setAccessibleName(tr("In shade"));
    shadeQuality_->setAccessibleName(tr("Chất lượng"));
    dpi_->setLabel(tr("DPI"));
    dpiUnit_->setText(tr("DPI"));
    for (int i = 0; i < PlotSettingsModel::kOptionCount; ++i)
        options_[std::size_t(i)]->setText(tr(kOptions[i]));
    orientation_->setAccessibleName(tr("Hướng giấy"));
    orientation_->segment(0)->setText(tr("Dọc"));
    orientation_->segment(1)->setText(tr("Ngang"));
    upsideDown_->setText(tr("In ngược"));
    previewButton_->setText(tr("Xem trước…"));
    moreButton_->setText(isExpanded() ? tr("Ít tùy chọn ◂") : tr("Thêm tùy chọn ▸"));
    apply_->setText(tr("Áp dụng cho layout"));
    setConfirm(tr("In"));
    syncEdition();
    sync();
}

void PlotDialog::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    DialogFrame::changeEvent(e);
}

bool PlotDialog::isExpanded() const
{
    return more_ && !more_->isHidden();
}

void PlotDialog::setExpanded(bool on)
{
    settings::Store::instance().set(kPlotMore, on);
    if (isExpanded() == on)
        return;
    const QPoint at = pos(); // mở rộng sang phải, mép trái đứng yên
    more_->setVisible(on);
    moreButton_->setText(on ? tr("Ít tùy chọn ◂") : tr("Thêm tùy chọn ▸"));
    if (isVisible()) {
        resize(fitted());
        move(at);
    }
}

QSize PlotDialog::fitted() const
{
    // adjustSize (show() của QDialog) giới hạn cửa sổ ở 2/3 màn hình nên hộp cao bị ép dưới sizeHint: đặt cỡ theo
    // sizeHint, chỉ giới hạn trong vùng dùng được của màn hình.
    const QScreen* s = parentWidget() ? parentWidget()->screen() : screen();
    QSize size = sizeHint();
    if (s)
        size = size.boundedTo(s->availableGeometry().size());
    return size.expandedTo(minimumSizeHint());
}

void PlotDialog::startWindowPick()
{
    if (!isVisible())
        return;
    hideForPick(true);
    emit windowPickRequested();
}

void PlotDialog::finishWindowPick(QPointF first, QPointF second)
{
    if (!isHiddenForPick())
        return;
    model_->setWindow(QRectF(first, second));
    sync();
    hideForPick(false);
}

void PlotDialog::cancelWindowPick()
{
    if (isHiddenForPick())
        hideForPick(false);
}

void PlotDialog::accept()
{
    if (!model_->canPlot())
        return;
    const plot::PlotSettings s = model_->settings();
    if (s.saveToLayout && !proLocked())
        emit applyToLayoutRequested(s.setup);
    DialogFrame::accept(); // hộp đóng ngay, in chạy nền
    if (runner_)
        runner_->start(s, model_->context().owner);
}

bool PlotDialog::proLocked() const
{
    const auto* gate = edition::EditionGate::instance();
    return gate && gate->locked(Edition::Pro);
}

bool PlotDialog::upgradeIfLocked(const QString& feature, const QString& name)
{
    if (!proLocked())
        return false;
    edition::EditionGate::instance()->upgrade().request({feature, Edition::Pro, name});
    return true;
}

QString PlotDialog::paperLabel(const plot::Paper& paper) const
{
    const QString title = paper.translatable ? tr(paper.title.toUtf8().constData()) : paper.title;
    if (!(paper.width > 0) || !(paper.height > 0))
        return title;
    return tr("%1 (%2 × %3 mm)").arg(title, number(paper.width, 2), number(paper.height, 2));
}

QString PlotDialog::deviceLabel(const plot::Device& device) const
{
    switch (device.kind) {
    case plot::DeviceKind::Pdf: return tr("PDF của TrionCAD");
    case plot::DeviceKind::Png: return tr("PNG của TrionCAD");
    case plot::DeviceKind::System: break;
    }
    return device.name;
}

} // namespace trion::ui
