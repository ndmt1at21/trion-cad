#include "app/dialogs/pickers/ColorPickerDialog.h"

#include "app/core/a11y/Announce.h"
#include "app/dialogs/pickers/ColorWidgets.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Paint.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/numeric/NumericField.h"
#include "app/widgets/tooltip/RichTooltip.h"

#include <QBoxLayout>
#include <QCoreApplication>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMetaMethod>
#include <QPainter>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTimer>

#include <cmath>
#include <utility>

using namespace Qt::StringLiterals;

namespace trion::ui {

namespace {

using theme::TextRole;

// Giá trị của prototype không có token riêng (TC.dialog w 640, css .col-body, .col-foot, .tcw, .aci, .acs).
constexpr int kBodyW = 640 - 2 * 14 - 2; // trừ .dlg-b padding 14 và viền thẻ (như ConfirmDialog)
constexpr int kGap10 = 10;               // .col-body padding-top, .col-foot gap/padding-top, .tcw gap
constexpr int kLabelW = 70;              // --lab-w của hàng True Color
constexpr int kColorW = 130;             // .field ô "Màu"
constexpr int kHexW = 90;                // #col-hex
constexpr int kAciColumns = 24, kAciRows = 10;
constexpr QSize kAciCell{12, 16};    // .acs min-width 12 (giãn theo cột 1fr), .aci .acs cao 16
constexpr QSize kRowCell{20, 20};    // .hrow > .acs
constexpr QSize kRecentCell{18, 18}; // #col-rec .acs
constexpr int kCellGap = 1;          // .aci, .hrow gap 1 px
constexpr int kMaxChannel[2][3] = {{360, 100, 100}, {255, 255, 255}}; // HSL, RGB

// Nhãn các ô số theo mô hình (HSL, RGB).
constexpr const char* kFieldNames[2][3] = {
    {QT_TRANSLATE_NOOP("trion::ui::ColorPickerDialog", "Sắc độ"),
     QT_TRANSLATE_NOOP("trion::ui::ColorPickerDialog", "Bão hòa"),
     QT_TRANSLATE_NOOP("trion::ui::ColorPickerDialog", "Độ sáng")},
    {QT_TRANSLATE_NOOP("trion::ui::ColorPickerDialog", "Đỏ"),
     QT_TRANSLATE_NOOP("trion::ui::ColorPickerDialog", "Xanh lá"),
     QT_TRANSLATE_NOOP("trion::ui::ColorPickerDialog", "Xanh dương")},
};

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

void tint(QWidget* w, tok::ColorId id)
{
    QPalette pal = w->palette();
    pal.setColor(QPalette::WindowText, col(id));
    w->setPalette(pal);
}

// Nét chia trên .col-foot (border-top 1 px --c-border).
class Rule final : public QWidget {
public:
    Rule()
    {
        setFixedHeight(1);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

protected:
    void paintEvent(QPaintEvent*) override { QPainter(this).fillRect(rect(), col(tok::c_border)); }
};

} // namespace

// Ô chữ .inp nhận giá trị khi Enter hoặc rời ô (như NumericField): sai thì viền 2 px --c-danger và câu ở FieldMessage,
// Enter sai dừng ở ô (không tới nút xác nhận); đúng thì Enter đi tiếp tới nút xác nhận. Esc khi đang sửa hoặc đang lỗi:
// trả chữ cũ; không thì Esc tới hộp thoại.
class ColorPickerDialog::TextField final : public QLineEdit {
public:
    std::function<QString(const QString&)> apply; // nhận chữ đã sửa: rỗng = đúng (đã áp), khác rỗng = câu lỗi
    FieldMessage* message = nullptr;

    TextField()
    {
        setAttribute(Qt::WA_Hover);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    // Chữ của giá trị đang có; bỏ lỗi.
    void setShown(const QString& text)
    {
        shown_ = text;
        setText(text);
        setCursorPosition(0);
        clearError();
    }
    bool hasError() const { return error_; }

    bool commit()
    {
        if (text() == shown_) {
            clearError();
            return true;
        }
        const QString why = apply ? apply(text()) : QString();
        if (why.isEmpty())
            return true;
        error_ = true;
        if (message)
            message->setError(why);
        update();
        if (auto* d = qobject_cast<ColorPickerDialog*>(window()))
            d->fitHeight();
        return false;
    }

    bool revert()
    {
        if (text() == shown_ && !error_)
            return false;
        setText(shown_);
        clearError();
        return true;
    }

protected:
    void keyPressEvent(QKeyEvent* e) override
    {
        const int k = e->key();
        if (((k == Qt::Key_Return || k == Qt::Key_Enter) && !commit()) || (k == Qt::Key_Escape && revert())) {
            e->accept();
            return;
        }
        QLineEdit::keyPressEvent(e); // Enter, Esc còn lại: QLineEdit bỏ qua, tới hộp thoại
    }
    void focusOutEvent(QFocusEvent* e) override
    {
        QLineEdit::focusOutEvent(e);
        if (e->reason() != Qt::ActiveWindowFocusReason && e->reason() != Qt::PopupFocusReason)
            commit();
    }
    void paintEvent(QPaintEvent* e) override
    {
        QLineEdit::paintEvent(e); // nền, viền 1 px (TrionStyle .inp), chữ
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        if (error_) // .inp.err
            paint::drawRing(p, rect(), col(tok::c_danger));
        else if (hasFocus()) // .inp:focus: viền --c-focus + bóng 1 px
            paint::drawRing(p, rect(), col(tok::c_focus));
        else if (isEnabled() && underMouse())
            paint::drawBox(p, rect(), QColor(), col(tok::c_text_2));
    }

private:
    void clearError()
    {
        if (!error_)
            return;
        error_ = false;
        if (message)
            message->setError({});
        update();
    }

    QString shown_;
    bool error_ = false;
};

ColorPickerDialog::ColorPickerDialog(const Color& current, Mode mode, QWidget* parent)
    : DialogFrame(u"color"_s, parent), current_(current), mode_(mode)
{
    setModal(true);
    setCommand(u"COLOR"_s);
    rgb_ = toRgb(hsl_);

    auto* body = new QWidget;
    body->setMinimumWidth(kBodyW);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins({});
    column->setSpacing(0);
    tabs_ = new TabBar;
    tabs_->setObjectName(u"tabs"_s);
    for (int i = 0; i < 3; ++i)
        tabs_->addTab(QString());
    panes_ = new QStackedWidget;
    // .col-body min-height 250: trang ACI đã cao hơn (bảng 10 hàng); QStackedWidget lấy cỡ trang lớn nhất.
    panes_->addWidget(buildIndexPane());
    panes_->addWidget(buildTrueColorPane());
    auto* bookPane = new QWidget;
    auto* bookColumn = new QVBoxLayout(bookPane);
    bookColumn->setContentsMargins(0, kGap10, 0, 0);
    book_ = new EmptyState(u"book"_s);
    book_->setObjectName(u"book"_s);
    bookColumn->addWidget(book_);
    bookColumn->addStretch(1);
    panes_->addWidget(bookPane);
    connect(tabs_, &QTabBar::currentChanged, panes_, &QStackedWidget::setCurrentIndex);
    column->addWidget(tabs_);
    column->addWidget(panes_);
    column->addWidget(buildFoot());
    setBody(body);

    hint_ = new QLabel;
    hint_->setObjectName(u"hint"_s);
    footer()->insertWidget(0, hint_);
    addCancel();

    loadRecent();
    if (mode_ == Mode::TrueColorOnly) {
        tabs_->hide();
        pickButton_->hide();
    }
    // TrueColorOnly mà màu đang dùng không có RGB (ByLayer, ByBlock): bắt đầu từ màu mặc định của tab True Color.
    setPick(mode_ == Mode::TrueColorOnly && !current_.hasRgb() ? Color::rgb(rgb_) : current_);
    setTab(mode_ == Mode::TrueColorOnly           ? Tab::TrueColor
           : current_.kind() == Color::Kind::Book ? Tab::Book
           : current_.kind() == Color::Kind::True ? Tab::TrueColor
                                                  : Tab::Index);
    updatePickButton();
    connect(&tm(), &theme::ThemeManager::changed, this, &ColorPickerDialog::restyle);
    restyle();
    retranslate();
    colorEdit_->setFocus(); // focus đầu như prototype: ô "Màu" (gõ "đỏ" rồi Enter)
}

ColorPickerDialog* ColorPickerDialog::ask(QWidget* parent, const Color& current, std::function<void(const Color&)> done,
                                          Mode mode)
{
    auto* d = new ColorPickerDialog(current, mode, parent);
    d->setAttribute(Qt::WA_DeleteOnClose);
    if (done)
        connect(d, &ColorPickerDialog::colorAccepted, d, [done = std::move(done)](const Color& c) { done(c); });
    d->open();
    return d;
}

QWidget* ColorPickerDialog::buildIndexPane()
{
    auto* pane = new QWidget;
    auto* column = new QVBoxLayout(pane);
    column->setContentsMargins(0, kGap10, 0, 0);
    column->setSpacing(px(tok::sp_4)); // .stack gap 8
    aciCaption_ = new QLabel;
    column->addWidget(aciCaption_);

    const auto grid = [this](const char* name, QSize cell, bool stretch) {
        auto* g = new SwatchGrid;
        g->setObjectName(QLatin1StringView(name));
        g->setCellSize(cell, kCellGap, stretch);
        connect(g, &SwatchGrid::picked, this, [this](const Color& c) { setPick(c); });
        connect(g, &SwatchGrid::activated, this, [this](const Color& c) {
            setPick(c);
            accept();
        });
        connect(g, &SwatchGrid::hoverChanged, this, [this, g](int i) {
            if (i >= 0)
                describe(g->colorAt(i));
        });
        connect(g, &SwatchGrid::currentChanged, this, [this, g](int i) { describe(g->colorAt(i), g); });
        return g;
    };
    // .aci: 24 cột × 10 hàng; cột là sắc độ (10, 20, …, 240), hàng là chữ số cuối (sắc thái).
    QList<Color> table;
    for (int r = 0; r < kAciRows; ++r)
        for (int c = 0; c < kAciColumns; ++c)
            table << Color::aci(10 + c * 10 + r);
    aciGrid_ = grid("aci", kAciCell, true);
    aciGrid_->setColors(table, kAciColumns);
    column->addWidget(aciGrid_);

    QList<Color> standard, grays;
    for (int i = 1; i <= 9; ++i)
        standard << Color::aci(i);
    for (int i = 250; i <= 255; ++i)
        grays << Color::aci(i);
    standard_ = grid("standard", kRowCell, false);
    standard_->setColors(standard, int(standard.size()));
    grays_ = grid("grays", kRowCell, false);
    grays_->setColors(grays, int(grays.size()));
    auto* rows = new QHBoxLayout;
    rows->addWidget(standard_);
    rows->addStretch(1);
    rows->addWidget(grays_);
    column->addLayout(rows);

    auto* last = new QHBoxLayout;
    last->setSpacing(px(tok::sp_4));
    byLayer_ = new Button;
    byLayer_->setObjectName(u"byLayer"_s);
    byBlock_ = new Button;
    byBlock_->setObjectName(u"byBlock"_s);
    for (Button* b : {byLayer_, byBlock_}) {
        b->setControlSize(Button::Size::Small);
        last->addWidget(b);
    }
    connect(byLayer_, &QPushButton::clicked, this, [this] { setPick(Color::byLayer()); });
    connect(byBlock_, &QPushButton::clicked, this, [this] { setPick(Color::byBlock()); });
    last->addStretch(1);
    hover_ = new QLabel;
    hover_->setObjectName(u"hover"_s);
    last->addWidget(hover_);
    column->addLayout(last);
    column->addStretch(1);
    return pane;
}

QWidget* ColorPickerDialog::buildTrueColorPane()
{
    auto* pane = new QWidget;
    auto* row = new QHBoxLayout(pane);
    row->setContentsMargins(0, kGap10, 0, 0);
    row->setSpacing(kGap10);
    area_ = new HueSatArea;
    area_->setObjectName(u"hueSat"_s);
    connect(area_, &HueSatArea::hueSatChanged, this, [this](double h, double s) {
        hsl_.h = h;
        hsl_.s = s;
        rgb_ = toRgb(hsl_);
        trueColorEdited();
    });
    light_ = new Slider(Qt::Vertical);
    light_->setObjectName(u"lightness"_s);
    light_->setRange(0, 100);
    connect(light_, &QSlider::valueChanged, this, [this](int v) {
        if (syncing_)
            return;
        hsl_.l = v / 100.0;
        rgb_ = toRgb(hsl_);
        trueColorEdited();
    });

    auto* form = new QGridLayout;
    form->setColumnMinimumWidth(0, kLabelW);
    form->setHorizontalSpacing(kGap10);
    form->setVerticalSpacing(px(tok::sp_3)); // .stack gap 6
    modelLabel_ = new QLabel;
    models_ = new SegmentedGroup;
    models_->setObjectName(u"model"_s);
    models_->addSegment(u"HSL"_s); // tên mô hình, không dịch
    models_->addSegment(u"RGB"_s);
    modelLabel_->setBuddy(models_->segment(0));
    connect(models_, &SegmentedGroup::currentIndexChanged, this, &ColorPickerDialog::setModel);
    form->addWidget(modelLabel_, 0, 0);
    form->addWidget(models_, 0, 1, Qt::AlignLeft);
    for (int i = 0; i < 3; ++i) {
        fieldLabels_[i] = new QLabel;
        fields_[i] = new NumericField(ValueKind::Integer);
        fields_[i]->setObjectName(u"channel%1"_s.arg(i));
        fields_[i]->setPickEnabled(false);
        fields_[i]->setLowerBound(-1); // số nguyên ≥ 0
        fieldLabels_[i]->setBuddy(fields_[i]);
        connect(fields_[i], &NumericField::valueCommitted, this, [this, i](double v) { onField(i, v); });
        form->addWidget(fieldLabels_[i], i + 1, 0, Qt::AlignTop | Qt::AlignLeft);
        form->addWidget(fields_[i], i + 1, 1, Qt::AlignLeft);
    }
    hexLabel_ = new QLabel;
    hex_ = new TextField;
    hex_->setObjectName(u"hex"_s);
    hex_->setFixedWidth(kHexW);
    hexMsg_ = new FieldMessage(hex_);
    hex_->message = hexMsg_;
    hex_->apply = [this](const QString& text) -> QString {
        const std::optional<regen::Rgb> c = parseHex(text);
        if (!c)
            return tr(
                "Mã màu là # và 6 chữ số hex, ví dụ #3CC8C0."); // style-ok: ví dụ trong câu lỗi, không phải màu vẽ
        takeRgb(*c);
        trueColorEdited();
        return {};
    };
    hexLabel_->setBuddy(hex_);
    form->addWidget(hexLabel_, 4, 0);
    form->addWidget(hex_, 4, 1, Qt::AlignLeft);
    form->addWidget(hexMsg_, 5, 0, 1, 2);
    form->setRowStretch(6, 1);
    form->setColumnStretch(1, 1);

    row->addWidget(area_, 0, Qt::AlignTop);
    row->addWidget(light_, 0, Qt::AlignTop);
    row->addLayout(form, 1);
    setModel(0);
    return pane;
}

QWidget* ColorPickerDialog::buildFoot()
{
    auto* foot = new QWidget;
    auto* column = new QVBoxLayout(foot);
    column->setContentsMargins(0, px(tok::sp_4), 0, 0); // .col-foot margin-top 8, nét chia, padding-top 10
    column->setSpacing(kGap10);
    column->addWidget(new Rule);

    auto* g = new QGridLayout;
    g->setHorizontalSpacing(kGap10);
    g->setVerticalSpacing(px(tok::sp_2)); // .field gap 4 (nhãn ↔ ô), .stack gap 3 (nhãn ↔ hàng gần đây)
    recentLabel_ = new QLabel;
    recent_ = new SwatchGrid;
    recent_->setObjectName(u"recent"_s);
    recent_->setCellSize(kRecentCell, px(tok::sp_1), false); // #col-rec .hrow gap 2
    connect(recent_, &SwatchGrid::picked, this, [this](const Color& c) { setPick(c); });
    connect(recent_, &SwatchGrid::activated, this, [this](const Color& c) {
        setPick(c);
        accept();
    });
    connect(recent_, &SwatchGrid::hoverChanged, this, [this](int i) {
        if (i >= 0)
            describe(recent_->colorAt(i));
    });
    connect(recent_, &SwatchGrid::currentChanged, this, [this](int i) { describe(recent_->colorAt(i), recent_); });
    recentEmpty_ = new QLabel;
    recentEmpty_->setObjectName(u"recentEmpty"_s);
    auto* recentRow = new QHBoxLayout;
    recentRow->addWidget(recent_);
    recentRow->addWidget(recentEmpty_);
    g->addWidget(recentLabel_, 0, 0);
    g->addLayout(recentRow, 1, 0, Qt::AlignBottom | Qt::AlignLeft);
    g->setColumnStretch(1, 1);

    pickButton_ = new Button({}, Button::Kind::Icon);
    pickButton_->setObjectName(u"pick"_s);
    pickButton_->setIconName(u"idpoint"_s);
    connect(pickButton_, &QPushButton::clicked, this, &ColorPickerDialog::startPick);
    g->addWidget(pickButton_, 1, 2, Qt::AlignBottom);

    colorLabel_ = new QLabel;
    colorEdit_ = new TextField;
    colorEdit_->setObjectName(u"color"_s);
    colorEdit_->setFixedWidth(kColorW);
    colorLabel_->setBuddy(colorEdit_);
    colorMsg_ = new FieldMessage(colorEdit_);
    colorMsg_->setObjectName(u"colorMessage"_s);
    colorEdit_->message = colorMsg_;
    colorEdit_->apply = [this](const QString& text) -> QString {
        const std::optional<Color> c = parseColor(text);
        if (!c || (mode_ == Mode::TrueColorOnly && !c->hasRgb()))
            return colorError();
        setPick(*c);
        return {};
    };
    g->addWidget(colorLabel_, 0, 3);
    g->addWidget(colorEdit_, 1, 3, Qt::AlignBottom);
    compare_ = new ColorCompare;
    compare_->setObjectName(u"compare"_s);
    g->addWidget(compare_, 1, 4, Qt::AlignBottom);
    g->addWidget(colorMsg_, 2, 0, 1, 5);
    column->addLayout(g);
    return foot;
}

void ColorPickerDialog::setColor(const Color& c)
{
    setPick(c);
}

ColorPickerDialog::Tab ColorPickerDialog::tab() const
{
    return Tab(panes_->currentIndex());
}

void ColorPickerDialog::setTab(Tab t)
{
    if (mode_ == Mode::TrueColorOnly && t != Tab::TrueColor)
        return;
    tabs_->setCurrentIndex(int(t));
    panes_->setCurrentIndex(int(t)); // TabBar ẩn (TrueColorOnly) vẫn đổi trang
}

void ColorPickerDialog::setPick(const Color& c, From from)
{
    if (mode_ == Mode::TrueColorOnly) {
        if (!c.hasRgb())
            return; // màu vùng vẽ cần màu cụ thể
        pick_ = Color::rgb(c.rgb());
    } else {
        pick_ = c;
    }
    if (from != From::TrueColor && pick_.hasRgb())
        takeRgb(pick_.rgb());
    sync();
}

void ColorPickerDialog::takeRgb(regen::Rgb c)
{
    Hsl next = toHsl(c);
    if (c.r == c.g && c.g == c.b) { // xám không có sắc độ: giữ sắc độ đang có; đen, trắng: giữ cả bão hòa
        next.h = hsl_.h;
        if (c.r == 0 || c.r == 255)
            next.s = hsl_.s;
    }
    hsl_ = next;
    rgb_ = c;
}

void ColorPickerDialog::trueColorEdited()
{
    setPick(Color::rgb(rgb_), From::TrueColor);
}

void ColorPickerDialog::onField(int i, double v)
{
    if (syncing_)
        return;
    if (model_ == 0) {
        (i == 0 ? hsl_.h : i == 1 ? hsl_.s : hsl_.l) = i == 0 ? v : v / 100;
        rgb_ = toRgb(hsl_);
    } else {
        regen::Rgb c = rgb_;
        (i == 0 ? c.r : i == 1 ? c.g : c.b) = std::uint8_t(v);
        takeRgb(c);
    }
    trueColorEdited();
}

void ColorPickerDialog::setModel(int model)
{
    model_ = model == 1 ? 1 : 0;
    for (int i = 0; i < 3; ++i) {
        fields_[i]->setUpperBound(kMaxChannel[model_][i]);
        fieldLabels_[i]->setText(tr(kFieldNames[model_][i]));
        fields_[i]->setLabel(tr(kFieldNames[model_][i]));
    }
    sync();
}

void ColorPickerDialog::sync()
{
    if (!compare_)
        return; // đang dựng
    syncing_ = true;
    for (SwatchGrid* g : {aciGrid_, standard_, grays_, recent_})
        g->setSelected(pick_);
    compare_->setColors(current_, pick_);
    compare_->setAccessibleDescription(tr("Cũ: %1 · Mới: %2").arg(current_.label(), pick_.label()));
    colorEdit_->setShown(pick_.label());
    area_->setHsl(hsl_);
    area_->setAccessibleDescription(
        tr("Sắc độ %1°, bão hòa %2 %").arg(QString::number(qRound(hsl_.h)), QString::number(qRound(hsl_.s * 100))));
    light_->setValue(qRound(hsl_.l * 100));
    const int values[2][3] = {
        {int(std::lround(hsl_.h)), int(std::lround(hsl_.s * 100)), int(std::lround(hsl_.l * 100))},
        {rgb_.r, rgb_.g, rgb_.b}};
    for (int i = 0; i < 3; ++i)
        fields_[i]->setValue(values[model_][i]);
    hex_->setShown(hexName(rgb_));
    syncing_ = false;
}

void ColorPickerDialog::describe(const Color& c, QWidget* announceFrom)
{
    described_ = c;
    hasDescribed_ = true;
    const regen::Rgb v = c.rgb();
    const QString r = QString::number(v.r), g = QString::number(v.g), b = QString::number(v.b);
    QString text;
    if (c.kind() == Color::Kind::Aci && c.index() <= 7)
        text = tr("Chỉ số màu %1 (%2) · RGB %3, %4, %5").arg(QString::number(c.index()), c.label(), r, g, b);
    else if (c.kind() == Color::Kind::Aci)
        text = tr("Chỉ số màu %1 · RGB %2, %3, %4").arg(QString::number(c.index()), r, g, b);
    else
        text = tr("%1 · RGB %2, %3, %4").arg(c.label(), r, g, b);
    hover_->setText(text);
    if (announceFrom) { // đi bằng phím: trình đọc màn hình đọc ô vừa tới
        announceFrom->setAccessibleDescription(text);
        a11y::announce(announceFrom, text);
    }
}

// Câu lỗi dài xuống dòng (FieldMessage: wordWrap, cao tối thiểu 1 dòng): cửa sổ cấp cao không tự cao thêm theo
// heightForWidth của thân, nên đo thân ở bề ngang hiện tại (sau khi bố cục con cập nhật) rồi cao thêm phần thiếu.
void ColorPickerDialog::fitHeight()
{
    QWidget* b = body();
    if (!b || !isVisible())
        return;
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    if (const int h = height() - b->height() + b->heightForWidth(b->width()); h > height())
        resize(width(), h);
}

QString ColorPickerDialog::colorError() const
{
    return mode_ == Mode::TrueColorOnly
               ? tr("Không đọc được màu này. Gõ mã #RRGGBB, số 1–255 hoặc tên màu (đỏ, red).")
               : tr("Không đọc được màu này. Gõ số 1–255, tên màu (đỏ, red), ByLayer, ByBlock hoặc #RRGGBB.");
}

void ColorPickerDialog::startPick()
{
    if (picking_ || !isSignalConnected(QMetaMethod::fromSignal(&ColorPickerDialog::pickRequested)))
        return;
    picking_ = true;
    hideForPick(true);
    emit pickRequested();
}

void ColorPickerDialog::finishPick(const Color& c)
{
    if (!std::exchange(picking_, false))
        return;
    hideForPick(false);
    setPick(c); // ByLayer, ByBlock giữ nguyên; màu cụ thể đúng màu đó
}

void ColorPickerDialog::cancelPick()
{
    if (std::exchange(picking_, false))
        hideForPick(false);
}

QString ColorPickerDialog::pickPrompt()
{
    return tr("Bấm vào một đối tượng để lấy màu (Esc để quay lại).");
}

void ColorPickerDialog::updatePickButton()
{
    if (pickButton_)
        pickButton_->setEnabled(isSignalConnected(QMetaMethod::fromSignal(&ColorPickerDialog::pickRequested)));
}

// Qt gọi hai hàm này khi đang giữ khóa nội bộ của QObject: không được gọi isSignalConnected ở đây (disconnect treo).
// Cập nhật nút ở lượt sự kiện kế tiếp, trước khi người dùng kịp bấm.
void ColorPickerDialog::connectNotify(const QMetaMethod& signal)
{
    if (signal == QMetaMethod::fromSignal(&ColorPickerDialog::pickRequested))
        QTimer::singleShot(0, this, &ColorPickerDialog::updatePickButton);
    DialogFrame::connectNotify(signal);
}

void ColorPickerDialog::disconnectNotify(const QMetaMethod& signal)
{
    if (signal == QMetaMethod::fromSignal(&ColorPickerDialog::pickRequested))
        QTimer::singleShot(0, this, &ColorPickerDialog::updatePickButton);
    DialogFrame::disconnectNotify(signal);
}

void ColorPickerDialog::loadRecent()
{
    QList<Color> colors;
    for (const QString& s : settings::Store::instance().get(kRecentColors)) {
        const std::optional<Color> c = parseColor(s);
        if (c && (c->kind() == Color::Kind::Aci || c->kind() == Color::Kind::True) && !colors.contains(*c))
            colors << *c;
        if (colors.size() == kRecentMax)
            break;
    }
    recent_->setColors(colors, kRecentMax);
    recent_->setVisible(!colors.isEmpty());
    recentEmpty_->setVisible(colors.isEmpty());
}

void ColorPickerDialog::remember(const Color& c)
{
    if (c.kind() != Color::Kind::Aci && c.kind() != Color::Kind::True)
        return; // ByLayer, ByBlock, Color Book không vào danh sách
    const QString key = c.kind() == Color::Kind::Aci ? QString::number(c.index()) : hexName(c.rgb());
    settings::Store& store = settings::Store::instance();
    QStringList list = store.get(kRecentColors);
    list.removeAll(key);
    list.prepend(key);
    store.set(kRecentColors, list.mid(0, kRecentMax));
}

void ColorPickerDialog::accept()
{
    if (!colorEdit_->commit())
        return; // chữ đang gõ ở ô Màu sai: viền lỗi, không đóng
    if (!hex_->commit()) {
        setTab(Tab::TrueColor);
        hex_->setFocus(Qt::OtherFocusReason);
        return;
    }
    remember(pick_);
    emit colorAccepted(pick_);
    DialogFrame::accept();
}

void ColorPickerDialog::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    DialogFrame::changeEvent(e);
}

void ColorPickerDialog::restyle()
{
    const auto style = [](QLabel* l, TextRole role, tok::ColorId ink) {
        l->setFont(theme::font(role));
        tint(l, ink);
    };
    style(aciCaption_, TextRole::Label, tok::c_text_3); // .faint --fs-s
    style(hover_, TextRole::MonoLabel, tok::c_text_3);  // .mono.faint --fs-s
    for (QLabel* l : {modelLabel_, fieldLabels_[0], fieldLabels_[1], fieldLabels_[2], hexLabel_, colorLabel_})
        style(l, TextRole::Label, tok::c_text_2);          // .frow .lab, .field label
    style(recentLabel_, TextRole::Caption, tok::c_text_3); // .faint --fs-xs
    style(recentEmpty_, TextRole::Caption, tok::c_text_3);
    style(hint_, TextRole::Label, tok::c_text_3);
    colorEdit_->setFont(theme::font(TextRole::Ui));
    hex_->setFont(theme::font(TextRole::Mono)); // .inp.mono
    for (TextField* f : {colorEdit_, hex_})
        f->setFixedHeight(px(tok::h_ctl));
    light_->setFixedHeight(area_->sizeHint().height());
}

void ColorPickerDialog::retranslate()
{
    setWindowTitle(tr("Chọn màu"));
    setConfirm(tr("Dùng màu này"));
    tabs_->setAccessibleName(tr("Loại màu"));
    tabs_->setTabText(int(Tab::Index), tr("Màu chỉ số"));
    tabs_->setTabText(int(Tab::TrueColor), tr("True Color"));
    tabs_->setTabText(int(Tab::Book), tr("Color Book"));
    aciCaption_->setText(tr("Bảng màu chỉ số AutoCAD (ACI)"));
    aciGrid_->setAccessibleName(tr("Bảng màu chỉ số"));
    standard_->setAccessibleName(tr("Màu chuẩn 1–9"));
    grays_->setAccessibleName(tr("Màu xám 250–255"));
    byLayer_->setText(Color::byLayer().label());
    byBlock_->setText(Color::byBlock().label());
    if (hasDescribed_)
        describe(described_);
    else
        hover_->setText(tr("Rê lên ô màu để xem số và RGB"));
    area_->setAccessibleName(tr("Vùng chọn sắc độ và độ bão hòa"));
    light_->setAccessibleName(tr("Độ sáng"));
    modelLabel_->setText(tr("Mô hình"));
    hexLabel_->setText(tr("Mã màu"));
    hex_->setAccessibleName(tr("Mã màu"));
    book_->setText(tr("Color Book (Pantone, RAL)"),
                   tr("Có sau v1.0. Bản vẽ dùng màu từ Color Book vẫn hiển thị đúng màu."));
    recentLabel_->setText(tr("Dùng gần đây"));
    recent_->setAccessibleName(tr("Dùng gần đây"));
    recentEmpty_->setText(tr("Chưa có màu nào"));
    pickButton_->setAccessibleName(tr("Lấy màu từ bản vẽ"));
    const TipInfo tip{.name = tr("Lấy màu từ bản vẽ"),
                      .description = tr("Ẩn hộp thoại, bấm vào một đối tượng để lấy màu của nó.")};
    if (RichTooltip::instance())
        RichTooltip::attach(pickButton_, tip);
    else
        pickButton_->setToolTip(tip.name);
    colorLabel_->setText(tr("Màu"));
    colorEdit_->setAccessibleName(tr("Màu"));
    compare_->setAccessibleName(tr("So sánh màu cũ và mới"));
    hint_->setText(mode_ == Mode::TrueColorOnly ? tr("Gõ mã #RRGGBB, số 1–255 hoặc tên màu (đỏ, red)")
                                                : tr("Gõ số 1–255, tên màu (đỏ, red) hoặc ByLayer"));
    setModel(model_); // nhãn ô số, rồi sync(): chữ ô Màu, mô tả theo ngôn ngữ mới
}

} // namespace trion::ui
