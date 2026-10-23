#include "app/shell/onboarding/Onboarding.h"

#include "app/core/edition/EditionGate.h"
#include "app/core/i18n/Translator.h"
#include "app/shell/MainWindow.h"
#include "app/shell/onboarding/QuickTour.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Paint.h"
#include "app/widgets/dock/PaletteHost.h"

#include <QAbstractButton>
#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QEvent>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionButton>

#include <algorithm>
#include <array>

using namespace Qt::StringLiterals;

namespace trion::shell {

namespace {

using theme::TextRole;
using theme::Weight;

constexpr int kBodyWidth = 640 - 2 * 14 - 2; // hộp w 640 trừ .dlg-b padding 14 và viền thẻ
constexpr int kDotsGap = 14;                 // thanh chấm bước: margin-bottom 14px
constexpr int kCardTop = 10;                 // .grp padding-top 10px (trái, phải, dưới: --sp-5)
constexpr int kStackGap = 10;                // .stack gap
constexpr int kSwatchHeight = 64;            // ô xem trước chủ đề: height 64px

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

// Chữ màu token (--c-text-2, --c-text-3 không có vai trò QPalette nào), tô lại khi đổi chủ đề.
void ink(QWidget* w, tok::ColorId id)
{
    const auto apply = [w, id] {
        QPalette p = w->palette();
        p.setColor(QPalette::Active, QPalette::WindowText, col(id));
        p.setColor(QPalette::Inactive, QPalette::WindowText, col(id));
        p.setColor(QPalette::Disabled, QPalette::WindowText, col(tok::c_text_dis));
        w->setPalette(p);
    };
    apply();
    QObject::connect(&tm(), &theme::ThemeManager::changed, w, apply);
}

QLabel* textLabel(TextRole role, Weight weight = Weight::Regular)
{
    auto* l = new QLabel;
    l->setTextFormat(Qt::PlainText);
    l->setWordWrap(true);
    l->setFont(theme::font(role, weight));
    return l;
}

// Đồng ý telemetry: mức basic hoặc full (giá trị lạ coi như tắt).
bool telemetryOn(const settings::Store& store)
{
    const QString level = store.get(kTelemetryLevel);
    return level == u"basic"_s || level == u"full"_s;
}

// Thanh chấm bước: mỗi bước một vạch cao --sp-2, cách nhau --sp-2, bo --r; đã qua và đang ở: --c-accent, còn lại
// --c-active. Số bước có ở chân ("Bước i / n"), nên màu không là tín hiệu duy nhất.
class StepDots : public QWidget {
public:
    StepDots() { setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed); }
    void set(int current, int count)
    {
        current_ = current;
        count_ = count;
        update();
    }
    QSize sizeHint() const override { return {0, px(tok::sp_2)}; }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (count_ <= 0)
            return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        const double gap = tm().px(tok::sp_2);
        const double r = tm().px(tok::r);
        const double w = (width() - gap * (count_ - 1)) / count_;
        for (int k = 0; k < count_; ++k) {
            p.setBrush(col(k <= current_ ? tok::c_accent : tok::c_active));
            p.drawRoundedRect(QRectF(k * (w + gap), 0, w, height()), r, r);
        }
    }

private:
    int current_ = 0;
    int count_ = 0;
};

// Nút radio của thẻ (input type=radio, accent-color): style vẽ (TrionStyle), trạng thái theo thẻ.
class Radio : public QWidget {
public:
    explicit Radio(QAbstractButton* card) : QWidget(card), card_(card)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    QSize sizeHint() const override
    {
        return {style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth, nullptr, this),
                style()->pixelMetric(QStyle::PM_ExclusiveIndicatorHeight, nullptr, this)};
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QStyleOptionButton o;
        o.initFrom(this);
        o.state |= card_->isChecked() ? QStyle::State_On : QStyle::State_Off;
        QPainter p(this);
        style()->drawPrimitive(QStyle::PE_IndicatorRadioButton, &o, &p, this);
    }

private:
    QAbstractButton* card_;
};

// Ô xem trước chủ đề: nền --c-base, viền trong 1 px --c-border của chính chủ đề đó, bo --r-lg.
class Swatch : public QWidget {
public:
    explicit Swatch(bool light) : light_(light)
    {
        setFixedHeight(kSwatchHeight);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        const tok::Mode m{.light = light_};
        const double r = tm().px(tok::r_lg);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(theme::toQColor(tok::color(tok::c_border, m)), 1));
        p.setBrush(theme::toQColor(tok::color(tok::c_base, m)));
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), r, r);
    }

private:
    bool light_;
};

// Thẻ chọn một (label.grp có input radio của prototype): viền 1 px --c-border, bo --r-lg, đệm 10/12/12 px, cách dòng
// 6 px; rê: viền --c-border-ctl; nhấn: nền --c-press; chọn: viền 2 px --c-accent (border + box-shadow 1 px) và nút
// radio bật (không chỉ bằng màu); focus bàn phím: viền 2 px --c-focus phía trong, cách viền thẻ 1 px (--c-focus trùng
// --c-accent: viền focus chồng lên viền của thẻ đang chọn thì không thấy). Bấm ở đâu trên thẻ cũng chọn; Space chọn;
// mũi tên chuyển và chọn trong nhóm, Tab vào nhóm dừng ở một thẻ (autoExclusive: nhóm radio của Qt). Trình đọc màn
// hình: vai trò RadioButton (QAccessibleButton), tên là tiêu đề, mô tả là câu phụ. Chữ hiện ở QLabel con (xuống dòng),
// không ở text() của nút.
class ChoiceCard : public QAbstractButton {
public:
    explicit ChoiceCard(QWidget* preview = nullptr)
    {
        setCheckable(true);
        setAutoExclusive(true);
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_Hover);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); // cột của .grid2 chia đều
        auto* v = new QVBoxLayout(this);
        v->setContentsMargins(px(tok::sp_5), kCardTop, px(tok::sp_5), px(tok::sp_5));
        v->setSpacing(px(tok::sp_3));
        auto* head = new QHBoxLayout;
        head->setSpacing(px(tok::sp_4));
        head->addWidget(new Radio(this));
        title_ = textLabel(TextRole::Ui, Weight::Bold);
        head->addWidget(title_, 1);
        v->addLayout(head);
        if (preview)
            v->addWidget(preview);
        sub_ = textLabel(TextRole::Label);
        ink(sub_, tok::c_text_2);
        v->addWidget(sub_);
    }

    // Chữ đã dịch (hoặc không dịch: tên ngôn ngữ).
    void setTexts(const QString& title, const QString& sub)
    {
        setAccessibleName(title);
        title_->setText(title);
        sub_->setText(sub);
        setAccessibleDescription(sub);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF outer(rect());
        const QRectF box = outer.adjusted(1, 1, -1, -1); // 1 px ngoài viền: box-shadow của thẻ đang chọn
        const double r = tm().px(tok::r_lg);
        if (isDown()) {
            p.setPen(Qt::NoPen);
            p.setBrush(col(tok::c_press));
            p.drawRoundedRect(box, r, r);
        }
        p.setBrush(Qt::NoBrush);
        if (isChecked()) {
            p.setPen(QPen(col(tok::c_accent), 2));
            p.drawRoundedRect(box, r, r);
        } else {
            p.setPen(QPen(col(underMouse() && isEnabled() ? tok::c_border_ctl : tok::c_border), 1));
            p.drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), r, r);
        }
        if (ui::paint::focusVisible(this))
            ui::paint::drawRing(p, outer.adjusted(3, 3, -3, -3), col(tok::c_focus)); // nét [3, 5) px tính từ mép
    }

private:
    QLabel* title_;
    QLabel* sub_;
};

// Trang của bước: tiêu đề (h3: --fs-l đậm), câu dẫn (.muted) nếu có, nội dung. Chữ đặt trong retranslate() của lớp con,
// gọi lại khi đổi ngôn ngữ.
class Page : public QWidget {
public:
    Page()
    {
        box_ = new QVBoxLayout(this);
        box_->setContentsMargins(0, 0, 0, 0);
        box_->setSpacing(0);
        heading_ = textLabel(TextRole::Emphasis, Weight::Bold);
        box_->addWidget(heading_);
    }

protected:
    // Câu dẫn: h3 margin-bottom 4px, p margin-bottom 12px. Không có câu dẫn: lớp con thêm --sp-5 sau tiêu đề.
    void addLead()
    {
        lead_ = textLabel(TextRole::Ui);
        ink(lead_, tok::c_text_2);
        box_->addSpacing(px(tok::sp_2));
        box_->addWidget(lead_);
        box_->addSpacing(px(tok::sp_5));
    }
    // Hai thẻ cạnh nhau (.grid2: hai cột bằng nhau, cách --sp-5).
    void addPair(ChoiceCard* a, ChoiceCard* b)
    {
        auto* grid = new QGridLayout;
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(px(tok::sp_5));
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);
        grid->addWidget(a, 0, 0);
        grid->addWidget(b, 0, 1);
        auto* host = new QWidget; // cha chung của nhóm autoExclusive
        host->setLayout(grid);
        box_->addWidget(host);
    }
    void changeEvent(QEvent* e) override
    {
        if (e->type() == QEvent::LanguageChange)
            retranslate();
        QWidget::changeEvent(e);
    }
    virtual void retranslate() = 0;

    QVBoxLayout* box_;
    QLabel* heading_;
    QLabel* lead_ = nullptr;
};

// Bước Ngôn ngữ: áp ngay (Translator gửi LanguageChange, không khởi động lại), lưu "ui/language".
class LanguagePage final : public Page {
public:
    LanguagePage()
    {
        addLead();
        vi_ = new ChoiceCard;
        en_ = new ChoiceCard;
        // Tên ngôn ngữ và câu tả viết bằng chính ngôn ngữ đó, không dịch (prototype).
        vi_->setTexts(u"Tiếng Việt"_s, u"Nhãn tiếng Việt; tên lệnh tiếng Anh luôn hiện kèm."_s); // i18n-ok: tên tiếng
        en_->setTexts(u"English"_s, u"Interface and prompts in English."_s); // i18n-ok: tên tiếng, không dịch
        vi_->setObjectName(u"vi"_s);
        en_->setObjectName(u"en"_s);
        addPair(vi_, en_);
        const auto* t = i18n::Translator::instance();
        (t && t->language() == u"en"_s ? en_ : vi_)->setChecked(true); // vi-XL (QA) là tiếng Việt
        for (ChoiceCard* c : {vi_, en_})
            connect(c, &QAbstractButton::toggled, this, [c](bool on) {
                if (on)
                    if (auto* t = i18n::Translator::instance())
                        t->setLanguage(c->objectName());
            });
        retranslate();
    }

private:
    void retranslate() override
    {
        heading_->setText(Onboarding::tr("Chọn ngôn ngữ"));
        lead_->setText(Onboarding::tr("Đổi được bất cứ lúc nào trong Tùy chọn › Hệ thống, không cần cài lại."));
    }
    ChoiceCard* vi_;
    ChoiceCard* en_;
};

// Bước Chủ đề: áp ngay (ThemeManager đổi palette toàn ứng dụng), lưu "ui/theme".
class ThemePage final : public Page {
public:
    ThemePage()
    {
        addLead();
        dark_ = new ChoiceCard(new Swatch(false));
        light_ = new ChoiceCard(new Swatch(true));
        dark_->setObjectName(u"dark"_s);
        light_->setObjectName(u"light"_s);
        addPair(dark_, light_);
        (tm().mode().light ? light_ : dark_)->setChecked(true); // chủ đề đang hiệu lực (kể cả Theo hệ thống)
        connect(dark_, &QAbstractButton::toggled, this, [](bool on) {
            if (on)
                tm().setTheme(theme::Theme::Dark);
        });
        connect(light_, &QAbstractButton::toggled, this, [](bool on) {
            if (on)
                tm().setTheme(theme::Theme::Light);
        });
        retranslate();
    }

private:
    void retranslate() override
    {
        heading_->setText(Onboarding::tr("Chọn chủ đề"));
        lead_->setText(Onboarding::tr("Nền vùng vẽ đặt riêng; mặc định tối cho Model, trắng cho Layout."));
        dark_->setTexts(Onboarding::tr("Tối"), Onboarding::tr("Đỡ mỏi mắt khi vẽ lâu. Mặc định."));
        light_->setTexts(Onboarding::tr("Sáng"), Onboarding::tr("Hợp phòng sáng, máy chiếu."));
    }
    ChoiceCard* dark_;
    ChoiceCard* light_;
};

// Bước Quyền riêng tư: ô đồng ý (.chk, --fs-m) mặc định tắt; đổi là ghi kTelemetryLevel ngay (off ↔ basic, giữ full).
class PrivacyPage final : public Page {
public:
    explicit PrivacyPage(settings::Store& store) : store_(store)
    {
        addLead();
        agree_ = new QCheckBox;
        agree_->setObjectName(u"telemetry"_s);
        agree_->setFont(theme::font(TextRole::Heading));
        agree_->setChecked(telemetryOn(store_));
        box_->addWidget(agree_, 0, Qt::AlignLeft);
        box_->addSpacing(px(tok::sp_4));
        note_ = textLabel(TextRole::Label);
        ink(note_, tok::c_text_3);
        box_->addWidget(note_);
        connect(agree_, &QCheckBox::toggled, this, [this](bool on) {
            const QString level = store_.get(kTelemetryLevel);
            store_.set(kTelemetryLevel, !on ? u"off"_s : level == u"full"_s ? level : u"basic"_s);
        });
        retranslate();
    }

private:
    void retranslate() override
    {
        heading_->setText(Onboarding::tr("Gửi dữ liệu sử dụng ẩn danh?"));
        lead_->setText(Onboarding::tr("Giúp TrionCAD sửa lỗi và chạy nhanh hơn trên phần cứng phổ biến ở Việt Nam. "
                                      "Không gửi nội dung bản vẽ. Máy chủ đặt tại Việt Nam."));
        agree_->setText(Onboarding::tr("Đồng ý gửi báo lỗi và số liệu hiệu năng ẩn danh"));
        note_->setText(
            Onboarding::tr("Mặc định tắt. Xem chi tiết dữ liệu thu thập ở Tùy chọn › Hệ thống › Quyền riêng tư."));
    }
    settings::Store& store_;
    QCheckBox* agree_;
    QLabel* note_;
};

// Bước Phiên bản (chỉ khi đang Viewer): ba thẻ xếp dọc (.stack); lựa chọn đọc lúc kết thúc.
class EditionPage final : public Page {
public:
    EditionPage()
    {
        box_->addSpacing(px(tok::sp_5));
        auto* host = new QWidget; // cha chung của nhóm autoExclusive
        auto* stack = new QVBoxLayout(host);
        stack->setContentsMargins(0, 0, 0, 0);
        stack->setSpacing(kStackGap);
        for (auto [card, name] :
             {std::pair{&viewer_, "viewer"}, std::pair{&trial_, "trial"}, std::pair{&key_, "key"}}) {
            *card = new ChoiceCard;
            (*card)->setObjectName(QString::fromLatin1(name));
            stack->addWidget(*card);
        }
        box_->addWidget(host);
        viewer_->setChecked(true);
        retranslate();
    }
    Onboarding::EditionChoice choice() const
    {
        return trial_->isChecked() ? Onboarding::EditionChoice::Trial
               : key_->isChecked() ? Onboarding::EditionChoice::Key
                                   : Onboarding::EditionChoice::Viewer;
    }

private:
    void retranslate() override
    {
        heading_->setText(Onboarding::tr("Bạn muốn dùng phiên bản nào?"));
        viewer_->setTexts(Onboarding::tr("TrionCAD Viewer · miễn phí"),
                          Onboarding::tr("Mở, xem, đo, in, xuất PDF. Bác sĩ Font hiển thị đúng chữ tiếng Việt."));
        trial_->setTexts(
            Onboarding::tr("Dùng thử TrionCAD Pro 30 ngày"),
            Onboarding::tr("Vẽ, sửa, lưu DWG, LISP. Cần email; hết hạn tự về Viewer, không khóa dữ liệu."));
        key_->setTexts(Onboarding::tr("Kích hoạt bằng key"),
                       Onboarding::tr("Đã mua giấy phép hoặc dùng giấy phép công ty."));
    }
    ChoiceCard* viewer_ = nullptr;
    ChoiceCard* trial_ = nullptr;
    ChoiceCard* key_ = nullptr;
};

// Bước Hướng dẫn nhanh: danh sách đánh số 5 việc (ol, chữ --c-text-2, cách dòng 4 px).
class TourPage final : public Page {
public:
    TourPage()
    {
        addLead();
        auto* list = new QGridLayout;
        list->setContentsMargins(0, 0, 0, 0);
        list->setHorizontalSpacing(px(tok::sp_3));
        list->setVerticalSpacing(px(tok::sp_2));
        list->setColumnStretch(1, 1);
        for (int k = 0; k < int(items_.size()); ++k) {
            QLabel* number = textLabel(TextRole::Ui);
            number->setWordWrap(false);
            number->setText(u"%1."_s.arg(k + 1));
            ink(number, tok::c_text_2);
            items_[size_t(k)] = textLabel(TextRole::Ui);
            ink(items_[size_t(k)], tok::c_text_2);
            list->addWidget(number, k, 0, Qt::AlignRight | Qt::AlignTop);
            list->addWidget(items_[size_t(k)], k, 1);
        }
        box_->addLayout(list);
        retranslate();
    }

private:
    void retranslate() override
    {
        heading_->setText(Onboarding::tr("Hướng dẫn nhanh 5 bước"));
        lead_->setText(Onboarding::tr("Trên bản vẽ mẫu: mở, xem, đo, đọc thuộc tính, in PDF. Mất khoảng 2 phút."));
        items_[0]->setText(Onboarding::tr("Lăn chuột để zoom, giữ chuột giữa để pan"));
        items_[1]->setText(Onboarding::tr("Bấm Đo › Đo nhanh rồi bấm vào một phòng"));
        items_[2]->setText(Onboarding::tr("Chọn một đối tượng để xem Thuộc tính"));
        items_[3]->setText(Onboarding::tr("Bác sĩ Font: chữ TCVN3 đã hiện đúng"));
        items_[4]->setText(Onboarding::tr("In & Xuất › PDF"));
    }
    std::array<QLabel*, 5> items_{};
};

// Thân hộp: rộng đúng 640 px của prototype (chữ dài hơn ở chân thì hộp rộng thêm, thân giãn theo).
class Body : public QWidget {
public:
    Body() { setMinimumWidth(kBodyWidth); }
    QSize sizeHint() const override { return {kBodyWidth, QWidget::sizeHint().height()}; }
};

} // namespace

Onboarding::Onboarding(MainWindow& window, settings::Store& store)
    : DialogFrame(u"onb"_s, &window), window_(window), store_(store)
{
    setWindowModality(Qt::WindowModal); // TC.dialog của prototype mặc định modal; hướng dẫn chạy khi hộp đã đóng
    auto* body = new Body;
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    dots_ = new StepDots;
    column->addWidget(dots_);
    column->addSpacing(kDotsGap);
    pagesBox_ = new QVBoxLayout;
    column->addLayout(pagesBox_);
    setBody(body);

    stepLabel_ = textLabel(TextRole::Caption); // .faint, --fs-xs
    stepLabel_->setWordWrap(false);            // chân chật (chữ dài) thì hộp rộng thêm
    stepLabel_->setObjectName(u"step"_s);
    ink(stepLabel_, tok::c_text_3);
    footer()->insertWidget(0, stepLabel_);
    skip_ = new ui::Button({}, ui::Button::Kind::Ghost);
    skip_->setObjectName(u"skip"_s);
    back_ = new ui::Button;
    back_->setObjectName(u"back"_s);
    footer()->addWidget(skip_);
    footer()->addWidget(back_);
    connect(skip_, &QPushButton::clicked, this, &Onboarding::skip);
    connect(back_, &QPushButton::clicked, this, &Onboarding::back);

    steps_ = {
        {u"lang"_s, [] { return tr("Ngôn ngữ"); }, [] { return new LanguagePage; }},
        {u"theme"_s, [] { return tr("Chủ đề"); }, [] { return new ThemePage; }},
        {u"privacy"_s, [] { return tr("Quyền riêng tư"); }, [this] { return new PrivacyPage(store_); }},
        {u"edition"_s, [] { return tr("Phiên bản"); }, [] { return new EditionPage; },
         [] {
             // Đã có giấy phép (Pro, dùng thử…) thì không có gì để chọn. ponytail: UIA-117 làm bước Phiên bản đủ.
             const auto* gate = edition::EditionGate::instance();
             return !gate || gate->edition() == edition::Edition::Viewer;
         }},
        {u"tour"_s, [] { return tr("Hướng dẫn nhanh"); }, [] { return new TourPage; }},
    };
    retranslate();
}

Onboarding::~Onboarding() = default;

bool Onboarding::needed(const settings::Store& store)
{
    return !store.get(kOnboarded);
}

void Onboarding::addStep(Step step, const QString& before)
{
    const auto at = std::find_if(steps_.begin(), steps_.end(), [&](const Step& s) { return s.id == before; });
    steps_.insert(at, std::move(step));
}

void Onboarding::removeStep(const QString& id)
{
    std::erase_if(steps_, [&](const Step& s) { return s.id == id; });
}

void Onboarding::start()
{
    if (tour_)
        tour_->finish();
    ids_.clear();
    for (const Step& s : steps_)
        if (!s.visible || s.visible())
            ids_ << s.id;
    qDeleteAll(pages_); // dựng lại theo cài đặt hiện tại ("Xem lại" sau khi đổi ở Tùy chọn)
    pages_.clear();
    if (ids_.isEmpty())
        return;
    go(0);
    if (isVisible()) // "Xem lại" khi hộp đang mở: focus như lúc mở (showEvent không chạy lại)
        next_->setFocus(Qt::OtherFocusReason);
    show();
    raise();
    activateWindow();
}

void Onboarding::go(int index)
{
    current_ = index;
    const QString id = ids_[index];
    QWidget* page = pages_.value(id);
    if (!page) {
        page = buildPage(id);
        page->hide();
        pages_.insert(id, page);
        pagesBox_->addWidget(page);
        // Trang dựng sau chân: điểm dừng Tab của trang trước các nút chân (Tab vòng: × → trang → chân).
        QList<QWidget*> chain;
        for (QWidget* w : page->findChildren<QWidget*>())
            if (w->focusPolicy() != Qt::NoFocus)
                chain << w;
        chain << skip_ << back_ << next_;
        for (qsizetype k = 1; k < chain.size(); ++k)
            setTabOrder(chain[k - 1], chain[k]);
    }
    // Focus đang ở trang cũ (thẻ, ô đồng ý): về nút Tiếp, không để Qt chuyển tới nút kế (Bỏ qua) khi trang cũ ẩn.
    const QWidget* focus = QApplication::focusWidget();
    bool inOld = false;
    for (const QWidget* p : std::as_const(pages_))
        inOld = inOld || (p != page && p->isAncestorOf(focus));
    page->show();
    for (QWidget* p : std::as_const(pages_))
        if (p != page)
            p->hide();
    if (inOld)
        next_->setFocus(Qt::OtherFocusReason);
    static_cast<StepDots*>(dots_)->set(index, int(ids_.size()));
    back_->setEnabled(index > 0);
    retranslate();
    fit();
}

void Onboarding::fit()
{
    // Cao theo trang của bước (hộp của prototype co theo nội dung), chữ xuống dòng theo bề rộng thật. Không dùng
    // QLayout::SetFixedSize hay adjustSize(): cả hai tính chiều cao chữ xuống dòng sai khi cửa sổ có lề (lề bóng của
    // DialogFrame) hoặc bị kẹp theo 2/3 màn hình.
    const int w = layout()->totalSizeHint().width();
    resize(w, layout()->totalHeightForWidth(w));
}

bool Onboarding::event(QEvent* e)
{
    const bool handled = DialogFrame::event(e);
    if (e->type() == QEvent::LayoutRequest) // đổi trang, đổi ngôn ngữ, mật độ: chữ đổi cỡ
        fit();
    return handled;
}

QWidget* Onboarding::buildPage(const QString& id)
{
    for (const Step& s : steps_)
        if (s.id == id)
            return s.page();
    return new QWidget;
}

void Onboarding::next()
{
    if (ids_.isEmpty() || finishing_)
        return;
    if (current_ + 1 < ids_.size())
        go(current_ + 1);
    else
        finish(true);
}

void Onboarding::back()
{
    if (current_ > 0 && !finishing_)
        go(current_ - 1);
}

void Onboarding::skip()
{
    finish(false);
}

void Onboarding::accept()
{
    next();
}

void Onboarding::reject()
{
    skip();
}

Onboarding::EditionChoice Onboarding::editionChoice() const
{
    const QWidget* p = ids_.contains(u"edition"_s) ? pages_.value(u"edition"_s) : nullptr;
    return p ? static_cast<const EditionPage*>(p)->choice() : EditionChoice::Viewer;
}

QuickTour* Onboarding::tour() const
{
    return tour_;
}

void Onboarding::finish(bool tour)
{
    if (finishing_)
        return;
    finishing_ = true;
    store_.set(kOnboarded, true);
    if (sink_ && telemetryOn(store_)) // ponytail: PLF-008 thay (Telemetry::track tự lọc theo mức)
        sink_(QJsonObject{{u"event"_s, u"onboarding"_s},
                          {u"result"_s, tour ? u"done"_s : u"skipped"_s},
                          {u"step"_s, ids_.value(current_)}});
    const EditionChoice choice = editionChoice();
    done(tour ? Accepted : Rejected); // DialogFrame: focus về widget có focus lúc mở (nếu còn hiện)
    window_.activateWindow();
    if (!window_.focusWidget()) // lần chạy đầu chưa có focus nào: vùng vẽ
        window_.palettes().focusCanvas();
    finishing_ = false;
    emit completed(!tour);
    if (choice == EditionChoice::Trial)
        emit trialRequested();
    else if (choice == EditionChoice::Key)
        emit activationRequested();
    if (tour) {
        emit sampleRequested();
        tour_ = new QuickTour(window_);
        tour_->start();
    }
}

void Onboarding::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    DialogFrame::changeEvent(e);
}

void Onboarding::retranslate()
{
    setWindowTitle(tr("Chào mừng đến với TrionCAD"));
    skip_->setText(tr("Bỏ qua, xem lại trong Tùy chọn"));
    back_->setText(tr("Quay lại"));
    const bool last = !ids_.isEmpty() && current_ == ids_.size() - 1;
    next_ = setConfirm(last ? tr("Bắt đầu hướng dẫn") : tr("Tiếp"));
    next_->setObjectName(u"next"_s);
    if (ids_.isEmpty())
        return;
    QString name;
    for (const Step& s : steps_)
        if (s.id == ids_[current_] && s.title)
            name = s.title();
    stepLabel_->setText(tr("Bước %1 / %2 · %3").arg(QString::number(current_ + 1), QString::number(ids_.size()), name));
}

} // namespace trion::shell
