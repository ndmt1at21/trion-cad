#include "app/system/upgrade/UpgradeDialog.h"

#include "app/core/i18n/Translator.h"
#include "app/core/i18n/UiFormat.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Paint.h"
#include "app/widgets/message/Messages.h"

#include <QBoxLayout>
#include <QDesktopServices>
#include <QEvent>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QPainter>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace trion::upgrade {

namespace {

using theme::TextRole;
using theme::Weight;

constexpr int kWidth = 560;          // TC.dialog w:560
constexpr int kBodyPad = 14;         // .dlg-b padding
constexpr int kCardBorder = 1;       // viền thẻ của DialogFrame
constexpr int kBodyWidth = kWidth - 2 * kBodyPad - 2 * kCardBorder;
constexpr int kImage = 150;          // cột ảnh: grid-template-columns 150px, aspect-ratio 1
constexpr int kGlyph = 96;           // hình trong ảnh (prototype: SVG 110 trong ô 150; biểu tượng 16 px phóng lớn nét đậm hơn)
constexpr int kLockBox = 36;         // ô ổ khóa góc dưới phải (prototype: rect 38×38 bo 3 trong viewBox 120)
constexpr int kLock = 20;

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
    pal.setColor(QPalette::WindowText, tm().color(id));
    w->setPalette(pal);
}

ui::Edition editionOf(const QString& s)
{
    return s == "Pro"_L1 ? ui::Edition::Pro : s == "ViewerPro"_L1 ? ui::Edition::ViewerPro : ui::Edition::None;
}

// Ngôn ngữ của nội dung JSON: "en" hoặc "vi" (cả vi-XL).
QString contentLanguage()
{
    const i18n::Translator* t = i18n::Translator::instance();
    return t && t->language() == "en"_L1 ? u"en"_s : u"vi"_s;
}

// Ảnh minh họa (prototype: ô --c-canvas bo --r-xl, hình màu --c-accent, ổ khóa trên ô --c-base ở góc dưới phải).
// Trang trí: không nhận focus, không có tên tiếp cận (tên tính năng, phiên bản đã có chữ). Thuộc tính "image": tên
// biểu tượng đang vẽ (test, QA).
class Illustration final : public QWidget {
public:
    Illustration()
    {
        setObjectName(u"illustration"_s);
        setFixedSize(kImage, kImage);
    }
    void setImage(const QString& name)
    {
        setProperty("image", name);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(col(tok::c_canvas));
        const double r = tm().px(tok::r_xl);
        p.drawRoundedRect(QRectF(rect()), r, r);
        theme::IconProvider& icons = theme::IconProvider::instance();
        const qreal dpr = devicePixelRatioF();
        const QColor accent = col(tok::c_accent);
        QRect glyph(0, 0, kGlyph, kGlyph);
        glyph.moveCenter(rect().center());
        const QString name = property("image").toString();
        p.drawPixmap(glyph.topLeft(), ui::paint::tinted(icons.pixmap(name, kGlyph, dpr, theme::IconState::Normal), accent));
        QRect box(0, 0, kLockBox, kLockBox);
        box.moveBottomRight(glyph.bottomRight() + QPoint(px(tok::sp_4), px(tok::sp_4)));
        p.setBrush(col(tok::c_base));
        const double rb = tm().px(tok::r_lg);
        p.drawRoundedRect(QRectF(box), rb, rb);
        QRect lock(0, 0, kLock, kLock);
        lock.moveCenter(box.center());
        p.drawPixmap(lock.topLeft(), ui::paint::tinted(icons.pixmap(u"lock"_s, kLock, dpr, theme::IconState::Normal), accent));
    }
};

// Thẻ "Có trong TrionCAD Pro" (.tag.acc; Viewer Pro .tag.info): cao chữ + 2 × --sp-1, lề ngang --sp-3, bo --r, chữ
// --fs-xs --fw-m. Có chữ, không chỉ là màu.
class EditionTag final : public QLabel {
public:
    EditionTag() { setObjectName(u"tag"_s); }
    void setNeed(ui::Edition need)
    {
        need_ = need;
        update();
    }
    QSize sizeHint() const override
    {
        const QFontMetrics fm(font());
        return {fm.horizontalAdvance(text()) + 2 * px(tok::sp_3), fm.height() + 2 * px(tok::sp_1)};
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        const bool vp = need_ == ui::Edition::ViewerPro;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(col(vp ? tok::c_info_soft : tok::c_accent_soft));
        const double r = tm().px(tok::r);
        p.drawRoundedRect(QRectF(rect()), r, r);
        p.setPen(col(vp ? tok::c_info : tok::c_accent));
        p.setFont(font());
        p.drawText(rect(), Qt::AlignCenter, text());
    }

private:
    ui::Edition need_ = ui::Edition::Pro;
};

} // namespace

// ------------------------------------------------------------------------------------------------- UpgradeContent

Expected<UpgradeContent> UpgradeContent::parse(const QByteArray& json)
{
    const auto fail = [](const QString& what) { return Error{"upgrade", "content", what.toStdString()}; };
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject())
        return fail(u"JSON hỏng: %1 (vị trí %2)"_s.arg(pe.errorString()).arg(pe.offset));
    const auto text = [](const QJsonValue& v, Text& out) {
        out = {v["vi"_L1].toString().trimmed(), v["en"_L1].toString().trimmed()};
        return !out.vi.isEmpty() && !out.en.isEmpty();
    };
    UpgradeContent c;
    const QJsonObject editions = doc["editions"_L1].toObject();
    for (const auto& [key, out] : {std::pair{u"Pro"_s, &c.pro_}, std::pair{u"ViewerPro"_s, &c.viewerPro_}}) {
        const QJsonObject e = editions[key].toObject();
        if (e.isEmpty())
            return fail(u"editions.%1: thiếu mục"_s.arg(key));
        const QJsonObject p = e["price"_L1].toObject();
        out->image = e["image"_L1].toString();
        out->price = {p["year"_L1].toInteger(), p["month"_L1].toInteger(), p["perpetual"_L1].toInteger()};
        if (out->image.isEmpty() || out->price.year <= 0 || out->price.month < 0 || out->price.perpetual < 0)
            return fail(u"editions.%1: thiếu image hoặc giá năm không dương"_s.arg(key));
        if (!text(e["benefit"_L1], out->benefit))
            return fail(u"editions.%1.benefit: thiếu bản vi hoặc en"_s.arg(key));
    }
    const QJsonObject features = doc["features"_L1].toObject();
    for (auto it = features.begin(); it != features.end(); ++it) {
        const QJsonObject f = it.value().toObject();
        Feature x;
        x.need = editionOf(f["need"_L1].toString());
        x.image = f["image"_L1].toString();
        if (x.need == ui::Edition::None)
            return fail(u"features.%1.need: phải là Pro hoặc ViewerPro"_s.arg(it.key()));
        if (x.image.isEmpty() || !text(f["benefit"_L1], x.benefit))
            return fail(u"features.%1: thiếu image hoặc lợi ích vi/en"_s.arg(it.key()));
        c.features_.insert(it.key(), x);
    }
    return c;
}

const Expected<UpgradeContent>& UpgradeContent::builtin()
{
    static const Expected<UpgradeContent> content = []() -> Expected<UpgradeContent> {
        QFile f(u":/trion/upgrade/upgrade-content.json"_s);
        if (!f.open(QIODevice::ReadOnly))
            return Error{"upgrade", "content", "không đọc được :/trion/upgrade/upgrade-content.json"};
        return parse(f.readAll());
    }();
    return content;
}

QStringList UpgradeContent::features() const
{
    QStringList out = features_.keys();
    std::sort(out.begin(), out.end());
    return out;
}

ui::Edition UpgradeContent::need(const QString& feature, ui::Edition requested) const
{
    if (requested != ui::Edition::None)
        return requested;
    const auto it = features_.constFind(feature);
    return it != features_.cend() ? it->need : ui::Edition::Pro;
}

PageContent UpgradeContent::page(const QString& feature, ui::Edition need, QStringView language) const
{
    const EditionContent& e = need == ui::Edition::ViewerPro ? viewerPro_ : pro_;
    const bool en = language == u"en";
    PageContent p{en ? e.benefit.en : e.benefit.vi, e.image, e.price, true};
    if (const auto it = features_.constFind(feature); it != features_.cend()) {
        p.benefit = en ? it->benefit.en : it->benefit.vi;
        p.image = it->image;
        p.generic = false;
    }
    return p;
}

// -------------------------------------------------------------------------------------------------- UpgradeDialog

UpgradeDialog::UpgradeDialog(const edition::UpgradeRequest& request, UpgradeContent content, Hooks hooks,
                             QWidget* parent)
    : DialogFrame(u"upgrade"_s, parent), request_(request), content_(std::move(content)), hooks_(std::move(hooks))
{
    setModal(true);
    setAttribute(Qt::WA_DeleteOnClose);
    request_.need = content_.need(request.feature, request.need);

    // Thân: [ảnh] [thẻ phiên bản, tên, lợi ích, giá] như lưới 150px | 1fr của prototype.
    auto* body = new QWidget;
    body->setFixedWidth(kBodyWidth);
    auto* row = new QHBoxLayout(body);
    row->setContentsMargins({});
    image_ = new Illustration;
    row->addWidget(image_, 0, Qt::AlignTop);
    auto* column = new QVBoxLayout;
    tag_ = new EditionTag;
    column->addWidget(tag_, 0, Qt::AlignLeft);
    const auto paragraph = [](const QString& name) {
        auto* l = new QLabel;
        l->setObjectName(name);
        l->setTextFormat(Qt::PlainText);
        l->setWordWrap(true);
        return l;
    };
    feature_ = paragraph(u"feature"_s);
    benefit_ = paragraph(u"benefit"_s);
    price_ = paragraph(u"price"_s);
    priceNote_ = paragraph(u"priceNote"_s);
    column->addWidget(feature_);
    column->addWidget(benefit_);
    auto* prices = new QVBoxLayout; // <p><b>Từ …</b><br><span class="faint">…</span></p>: không giãn giữa hai dòng
    prices->setSpacing(0);
    prices->addWidget(price_);
    prices->addWidget(priceNote_);
    column->addLayout(prices);
    error_ = new ui::FieldMessage(nullptr);
    error_->setObjectName(u"error"_s);
    error_->hide();
    column->addWidget(error_);
    column->addStretch(1);
    row->addLayout(column, 1);
    setBody(body);

    // Chân: [Để sau] [Đã có key? Kích hoạt] ……… [Mua] [Dùng thử 30 ngày].
    later_ = new ui::Button({}, ui::Button::Kind::Ghost);
    later_->setObjectName(u"later"_s);
    footer()->insertWidget(0, later_);
    connect(later_, &QPushButton::clicked, this, &QDialog::reject);
    activate_ = new ui::Button({}, ui::Button::Kind::Ghost);
    activate_->setObjectName(u"activate"_s);
    activate_->setVisible(bool(hooks_.activate));
    footer()->insertWidget(1, activate_);
    connect(activate_, &QPushButton::clicked, this, [this] { finish(hooks_.activate); });
    if (hooks_.trial) {
        buy_ = new ui::Button({}, ui::Button::Kind::Normal);
        footer()->addWidget(buy_);
        connect(buy_, &QPushButton::clicked, this, &UpgradeDialog::buy);
    }
    connect(&tm(), &theme::ThemeManager::changed, this, &UpgradeDialog::restyle);
    restyle();
    retranslate();
}

void UpgradeDialog::setRequest(const edition::UpgradeRequest& request)
{
    request_ = request;
    request_.need = content_.need(request.feature, request.need);
    error_->setProperty("url", QVariant());
    error_->setError({});
    error_->hide();
    retranslate();
}

PageContent UpgradeDialog::content() const
{
    return content_.page(request_.feature, request_.need, contentLanguage());
}

void UpgradeDialog::accept()
{
    if (hooks_.trial)
        finish(hooks_.trial);
    else
        buy();
}

void UpgradeDialog::buy()
{
    const QUrl url = edition::UpgradeRouter::url(request_);
    if (hooks_.openUrl && hooks_.openUrl(url)) {
        DialogFrame::accept();
        return;
    }
    error_->setProperty("url", url);
    retranslate(); // dòng lỗi theo ngôn ngữ hiện tại, hộp thoại cao theo
}

void UpgradeDialog::finish(const Hook& hook)
{
    const Hook run = hook; // hook nằm trong hooks_ của hộp thoại sắp hủy (WA_DeleteOnClose: hủy sau, vẫn chép trước)
    const edition::UpgradeRequest r = request_;
    DialogFrame::accept();
    if (run)
        run(r);
}

void UpgradeDialog::restyle()
{
    tag_->setFont(theme::font(TextRole::Caption, Weight::Medium)); // .tag: --fs-xs --fw-m
    feature_->setFont(theme::font(TextRole::Title, Weight::Bold)); // h3: --fs-xl 600
    tint(feature_, tok::c_text);
    benefit_->setFont(theme::font(TextRole::Ui)); // p.muted
    tint(benefit_, tok::c_text_2);
    price_->setFont(theme::font(TextRole::Emphasis, Weight::Bold)); // b: --fs-l
    tint(price_, tok::c_text);
    priceNote_->setFont(theme::font(TextRole::Label)); // .faint --fs-s
    tint(priceNote_, tok::c_text_3);
    auto* row = static_cast<QHBoxLayout*>(body()->layout());
    row->setSpacing(px(tok::sp_6));                     // gap 16px
    static_cast<QVBoxLayout*>(row->itemAt(1)->layout())->setSpacing(px(tok::sp_4)); // .stack gap 8px
}

void UpgradeDialog::retranslate()
{
    setWindowTitle(tr("Nâng cấp TrionCAD"));
    const PageContent c = content();
    static_cast<Illustration*>(image_)->setImage(c.image);
    static_cast<EditionTag*>(tag_)->setNeed(request_.need);
    tag_->setText(edition::EditionGate::availableIn(request_.need));
    feature_->setText(request_.name.isEmpty() ? request_.feature : request_.name);
    benefit_->setText(c.benefit);
    price_->setText(tr("Từ %1 đ/năm").arg(i18n::formatCount(c.price.year)));
    QStringList other;
    if (c.price.month > 0)
        other << tr("%1 đ/tháng").arg(i18n::formatCount(c.price.month));
    if (c.price.perpetual > 0)
        other << tr("%1 đ vĩnh viễn").arg(i18n::formatCount(c.price.perpetual));
    priceNote_->setText(other.isEmpty() ? QString() : tr("hoặc %1").arg(other.join(u" · "_s)));
    priceNote_->setVisible(!other.isEmpty());
    if (const QUrl url = error_->property("url").toUrl(); !url.isEmpty()) {
        error_->setError(tr("Không mở được trình duyệt. Mở địa chỉ này để mua: %1").arg(url.toString()));
        error_->show();
    }

    later_->setText(tr("Để sau"));
    activate_->setText(tr("Đã có key? Kích hoạt"));
    if (hooks_.trial) {
        buy_->setText(tr("Mua"));
        setConfirm(tr("Dùng thử 30 ngày"))->setObjectName(u"trial"_s);
    } else {
        buy_ = setConfirm(tr("Mua"));
    }
    buy_->setObjectName(u"buy"_s);
    buy_->setIconName(u"ext"_s); // mở trang web
    buy_->setAccessibleDescription(tr("Mở trang mua trên web"));
    if (isVisible()) { // chữ dài, ngắn hơn (đổi ngôn ngữ, tính năng khác, dòng lỗi): cao theo nội dung ngay
        body()->layout()->activate();
        layout()->activate();
        adjustSize();
    }
}

void UpgradeDialog::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    DialogFrame::changeEvent(e);
}

// --------------------------------------------------------------------------------------------------- UpgradePage

UpgradePage::UpgradePage(edition::EditionGate& gate, QWidget* window, ui::ToastHost* toasts, QObject* parent)
    : QObject(parent), gate_(gate), window_(window), toasts_(toasts)
{
    setUrlOpener({});
    gate_.upgrade().setHandler([this](const edition::UpgradeRequest& r) { showUpgrade(r); });
    if (!toasts)
        return;
    // Lần sau trong phiên (UpgradeRouter đã mở trang của tính năng này): toast có liên kết "Xem" mở lại trang.
    connect(&gate_.upgrade(), &edition::UpgradeRouter::noticeRequested, this,
            [this](const edition::UpgradeRequest& r) {
                if (!toasts_)
                    return;
                const QPointer<UpgradePage> self(this);
                toasts_->post(ui::Severity::Info, edition::UpgradeRouter::noticeText(r),
                              {tr("Xem"), [self, r] {
                                   if (self)
                                       self->gate_.upgrade().open(r);
                               }},
                              u"license"_s);
            });
}

UpgradePage::~UpgradePage()
{
    gate_.upgrade().setHandler({});
    delete dialog_.data();
}

void UpgradePage::showUpgrade(const edition::UpgradeRequest& request)
{
    const Expected<UpgradeContent>& content = UpgradeContent::builtin();
    if (!content) {
        qWarning("UpgradePage: %s", content.error().message.c_str());
        opener_(edition::UpgradeRouter::url(request));
        return;
    }
    if (dialog_ && dialog_->isVisible()) { // đang đóng (chờ hủy) thì dựng bản mới
        dialog_->setRequest(request);
        dialog_->raise();
        dialog_->activateWindow();
        return;
    }
    const bool trialing = gate_.edition() == edition::Edition::Trial;
    dialog_ = new UpgradeDialog(request, *content, {opener_, activate_, trialing ? Hook() : trial_}, window_);
    dialog_->open();
}

void UpgradePage::setActivateHook(Hook hook)
{
    activate_ = std::move(hook);
}

void UpgradePage::setTrialHook(Hook hook)
{
    trial_ = std::move(hook);
}

void UpgradePage::setUrlOpener(UrlOpener opener)
{
    opener_ = opener ? std::move(opener) : UrlOpener([](const QUrl& u) { return QDesktopServices::openUrl(u); });
}

} // namespace trion::upgrade
