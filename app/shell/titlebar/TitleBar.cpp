#include "app/shell/titlebar/TitleBar.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/core/edition/EditionGate.h"
#include "app/shell/titlebar/CommandSearch.h"
#include "app/shell/titlebar/Qat.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/controls/Paint.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/tooltip/RichTooltip.h"

#include <QAction>
#include <QBoxLayout>
#include <QDesktopServices>
#include <QEvent>
#include <QLabel>
#include <QPainter>

using namespace Qt::StringLiterals;

namespace trion::shell {

namespace {

using edition::Edition;
using theme::IconProvider;
using theme::TextRole;
using theme::Weight;

constexpr int kSearchWidth = 300; // .tb-search width:300px (không có token)
// ponytail: thanh chật thì ô Tìm lệnh co trước tên bản vẽ (prototype: ô co theo flex), tới chỗ còn thấy vài chữ đang
// gõ (kết quả ở popup riêng): đủ cho thanh 1366 px có QAT đủ 7 mục (UIA-101), vi-XL, dùng thử kèm cảnh báo offline.
constexpr int kSearchMin = 80;
constexpr int kChevron = 9;       // mũi tên .ic.xs

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

// ponytail: trang tài khoản của prototype (thanh-toan.html#tai-khoan); địa chỉ thật do trang web UI-L16 chốt.
QUrl accountUrl()
{
    return QUrl(u"https://trioncad.vn/tai-khoan"_s);
}

} // namespace

// Nút menu ứng dụng .tb-app: logo --c-accent (cỡ --ic + --sp-1 = 18 px), "TrionCAD" đậm, mũi tên .ic.xs; cao bằng
// thanh trừ --sp-3; rê --c-hover, nhấn --c-press; viền focus khi focus đến từ bàn phím.
class TitleBar::AppButton final : public ui::Button {
public:
    AppButton() : Button({}, Kind::Ghost) {}

    QSize sizeHint() const override
    {
        return {px(tok::sp_3) + logo() + px(tok::sp_3) + brandWidth() + px(tok::sp_3) + kChevron + px(tok::sp_4),
                px(tok::h_title) - px(tok::sp_3)};
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF box(rect());
        const qreal rad = tm().px(tok::r);
        if (isDown() || underMouse()) {
            p.setPen(Qt::NoPen);
            p.setBrush(col(isDown() ? tok::c_press : tok::c_hover));
            p.drawRoundedRect(box, rad, rad);
        }
        if (ui::paint::focusVisible(this))
            ui::paint::drawRing(p, box, col(tok::c_focus));
        int x = px(tok::sp_3);
        p.drawPixmap(x, (height() - logo()) / 2,
                     IconProvider::instance().pixmap(u"logo"_s, logo(), devicePixelRatio(), theme::IconState::On));
        x += logo() + px(tok::sp_3);
        p.setFont(brandFont());
        p.setPen(col(tok::c_text));
        p.drawText(QRect(x, 0, brandWidth(), height()), Qt::AlignVCenter | Qt::TextSingleLine, brand());
        x += brandWidth() + px(tok::sp_3);
        ui::paint::drawChevron(p, QPointF(x + kChevron / 2.0, height() / 2.0), col(tok::c_text_2));
    }

private:
    static QString brand() { return u"TrionCAD"_s; } // tên sản phẩm, không dịch
    static QFont brandFont() { return theme::font(TextRole::Ui, Weight::Bold); }
    static int brandWidth() { return QFontMetrics(brandFont()).horizontalAdvance(brand()); }
    static int logo() { return px(tok::ic) + px(tok::sp_1); }
};

// Tên bản vẽ .tb-doc: tên (--c-text) liền "*" (--c-accent), rồi cờ, "—", tên phiên bản (--c-text-2), cách nhau --sp-3.
// text() là cả dòng (trình đọc màn hình, TruncationProbe); sizeHint đủ chữ; thiếu chỗ thì chỉ tên cắt giữa.
class TitleBar::DocTitle final : public QLabel {
public:
    DocTitle() { setTextFormat(Qt::PlainText); }

    void set(const QString& name, bool dirty, const QStringList& tail)
    {
        name_ = name;
        dirty_ = dirty;
        tail_ = tail;
        QStringList all = tail;
        if (!name.isEmpty())
            all.prepend(dirty ? name + u'*' : name);
        setText(all.join(u' '));
        updateGeometry();
        update();
    }

    QString shownName() const
    {
        const QFontMetrics fm(textFont());
        const int room = width() - fixedWidth();
        // So bằng cùng số đo với sizeHint: elidedText đo lẻ nên có thể cắt khi vừa khít.
        return fm.horizontalAdvance(name_) <= room ? name_ : fm.elidedText(name_, Qt::ElideMiddle, std::max(0, room));
    }

    QSize sizeHint() const override
    {
        const QFontMetrics fm(textFont());
        return {fixedWidth() + fm.horizontalAdvance(name_), fm.height()};
    }
    QSize minimumSizeHint() const override
    {
        const QFontMetrics fm(textFont());
        return {fixedWidth() + (name_.isEmpty() ? 0 : fm.horizontalAdvance(u'…')), fm.height()};
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const QFontMetrics fm(textFont());
        p.setFont(textFont());
        int x = 0;
        const auto draw = [&](const QString& s, tok::ColorId c) {
            const int w = fm.horizontalAdvance(s);
            p.setPen(col(c));
            p.drawText(QRect(x, 0, w, height()), Qt::AlignVCenter | Qt::TextSingleLine, s);
            x += w;
        };
        if (!name_.isEmpty()) {
            draw(shownName(), tok::c_text);
            if (dirty_)
                draw(u"*"_s, tok::c_accent); // .tb-dirty: dấu, không chỉ màu
            x += px(tok::sp_3);
        }
        for (const QString& t : tail_) {
            draw(t, tok::c_text_2);
            x += px(tok::sp_3);
        }
    }

private:
    static QFont textFont() { return theme::font(TextRole::Ui); }

    // Phần không rút gọn: "*", cờ, "—", tên phiên bản và khoảng cách giữa các phần.
    int fixedWidth() const
    {
        const QFontMetrics fm(textFont());
        int w = px(tok::sp_3) * std::max(0, int(tail_.size()) + (name_.isEmpty() ? 0 : 1) - 1);
        for (const QString& t : tail_)
            w += fm.horizontalAdvance(t);
        if (dirty_ && !name_.isEmpty())
            w += fm.horizontalAdvance(u'*');
        return w;
    }

    QString name_;
    bool dirty_ = false;
    QStringList tail_;
};

// Huy hiệu phiên bản .ed: --fs-xs đậm, lề ngang --sp-3, bo --r; màu theo phiên bản (--c-ed-*), cảnh báo giấy phép
// offline .ed.warn (--c-warn-soft, --c-warn). Cao 17 px của prototype ≈ chữ + 2 × --sp-1.
class TitleBar::EditionBadge final : public QLabel {
public:
    EditionBadge()
    {
        setTextFormat(Qt::PlainText);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    void set(const QString& text, tok::ColorId bg, tok::ColorId fg)
    {
        bg_ = bg;
        fg_ = fg;
        setText(text);
        updateGeometry();
        update();
    }

    QSize sizeHint() const override
    {
        const QFontMetrics fm(textFont());
        return {fm.horizontalAdvance(text()) + 2 * px(tok::sp_3), fm.height() + 2 * px(tok::sp_1)};
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal rad = tm().px(tok::r);
        p.setPen(Qt::NoPen);
        p.setBrush(col(bg_));
        p.drawRoundedRect(QRectF(rect()), rad, rad);
        p.setFont(textFont());
        p.setPen(col(fg_));
        p.drawText(rect(), Qt::AlignCenter | Qt::TextSingleLine, text());
    }

private:
    static QFont textFont() { return theme::font(TextRole::Caption, Weight::Bold); }

    tok::ColorId bg_ = tok::c_ed_viewer, fg_ = tok::c_ed_viewer_t;
};

TitleBar::TitleBar(actions::ActionRegistry& actions, edition::EditionGate& gate, QWidget* parent)
    : QWidget(parent), actions_(actions), gate_(gate), app_(new AppButton), qat_(new Qat(actions, gate)),
      title_(new DocTitle), badge_(new EditionBadge), search_(new CommandSearch(actions)),
      user_(new ui::Button({}, ui::Button::Kind::Icon)), help_(new ui::Button({}, ui::Button::Kind::Icon)),
      userMenu_(new ui::Menu(this)), helpMenu_(new ui::Menu(this))
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setUrlOpener({});
    // Tên cho test, QA (TruncationProbe ghi tên widget).
    app_->setObjectName(u"appMenuButton"_s);
    title_->setObjectName(u"docTitle"_s);
    badge_->setObjectName(u"editionBadge"_s);
    user_->setObjectName(u"accountButton"_s);
    help_->setObjectName(u"helpButton"_s);
    userMenu_->setObjectName(u"accountMenu"_s);
    helpMenu_->setObjectName(u"helpMenu"_s);
    auto* row = new QHBoxLayout(this);
    // .tb: lề trái --sp-2, khoảng cách --sp-3 (đọc lúc dựng; --sp-* như nhau ở mọi chế độ). Nút cửa sổ: MainWindow
    // đặt contentsMargins của thanh.
    row->setContentsMargins(px(tok::sp_2), 0, 0, 0);
    row->setSpacing(px(tok::sp_3));
    row->addWidget(app_);
    row->addWidget(qat_);
    row->addStretch(1);
    row->addWidget(title_);
    row->addWidget(badge_);
    row->addStretch(1);
    // Hệ số co giãn của ô: thiếu chỗ thì QBoxLayout cho ô cỡ tối thiểu trước khi rút gọn tên bản vẽ; thừa chỗ thì ô
    // giãn tới 300 px, phần còn lại chia đều hai bên tên (tên ở giữa QAT và ô).
    row->addWidget(search_, 1);
    row->addWidget(user_);
    row->addWidget(help_);
    search_->setMinimumWidth(kSearchMin);
    search_->setMaximumWidth(kSearchWidth);
    user_->setIconName(u"user"_s);
    help_->setIconName(u"help"_s);
#ifdef Q_OS_MACOS
    app_->hide(); // menu ứng dụng ở thanh menu hệ thống (07.1 §13.6)
#endif

    connect(app_, &QAbstractButton::clicked, this,
            [this] { emit appMenuRequested(app_->mapToGlobal(QPoint(0, app_->height()))); });
    connect(user_, &QAbstractButton::clicked, this, [this] { popupBelow(userMenu_, user_); });
    connect(help_, &QAbstractButton::clicked, this, [this] { popupBelow(helpMenu_, help_); });
    connect(&gate, &edition::EditionGate::editionChanged, this, &TitleBar::sync);
    // Thôi "Hiện bên dưới ribbon": lấy QAT về sau nút menu ứng dụng (cửa sổ chính đã dời nó xuống dưới ribbon).
    connect(qat_, &Qat::belowRibbonChanged, this, [this, row](bool below) {
        if (!below && qat_->parentWidget() != this)
            row->insertWidget(row->indexOf(app_) + 1, qat_);
    });
    sync();
}

void TitleBar::setDocument(const DocInfo& doc)
{
    doc_ = doc;
    sync();
}

void TitleBar::setAccount(const Account& account)
{
    account_ = account;
    sync();
}

void TitleBar::setTrialDays(int days)
{
    trialDays_ = days;
    sync();
}

void TitleBar::setOfflineDays(int days)
{
    offlineDays_ = days;
    sync();
}

void TitleBar::setUrlOpener(std::function<bool(const QUrl&)> opener)
{
    if (opener)
        openUrl_ = std::move(opener);
    else
        openUrl_ = [](const QUrl& u) { return QDesktopServices::openUrl(u); };
}

QString TitleBar::shownName() const
{
    return title_->shownName();
}

QSize TitleBar::sizeHint() const
{
    return {QWidget::sizeHint().width(), px(tok::h_title)};
}

QSize TitleBar::minimumSizeHint() const
{
    return {QWidget::minimumSizeHint().width(), px(tok::h_title)};
}

void TitleBar::sync()
{
    // Phiên bản: tên trên thanh (" — TrionCAD Pro"), chữ và màu huy hiệu (edBadge của prototype).
    QString name, badge;
    tok::ColorId bg = tok::c_ed_pro, fg = tok::c_ed_pro_t;
    switch (gate_.edition()) {
    case Edition::Viewer:
        name = tr("TrionCAD Viewer");
        badge = tr("Viewer");
        bg = tok::c_ed_viewer;
        fg = tok::c_ed_viewer_t;
        break;
    case Edition::ViewerPro:
        name = tr("TrionCAD Viewer Pro");
        badge = tr("Viewer Pro");
        bg = tok::c_ed_vp;
        fg = tok::c_ed_vp_t;
        break;
    case Edition::Pro:
        name = tr("TrionCAD Pro");
        badge = tr("Pro");
        break;
    case Edition::Trial:
        name = tr("TrionCAD Pro");
        badge = tr("Dùng thử: còn %n ngày", nullptr, trialDays_);
        bg = tok::c_ed_trial;
        fg = tok::c_ed_trial_t;
        break;
    case Edition::Education:
        name = tr("TrionCAD Giáo dục");
        badge = tr("Giáo dục");
        bg = tok::c_ed_edu;
        fg = tok::c_ed_edu_t;
        break;
    case Edition::Enterprise:
        name = tr("TrionCAD Doanh nghiệp");
        badge = tr("Doanh nghiệp");
        bg = tok::c_ed_ent;
        fg = tok::c_ed_ent_t;
        break;
    }
    if (offlineDays_ >= 0 && gate_.edition() != Edition::Viewer) { // .ed.warn: lâu chưa xác minh giấy phép
        badge = tr("%1 · còn %n ngày offline", nullptr, offlineDays_).arg(badge);
        bg = tok::c_warn_soft;
        fg = tok::c_warn;
    }
    badge_->set(badge, bg, fg);

    // Tên bản vẽ, cờ; Viewer: bản vẽ chỉ xem (07.1 §2).
    QStringList tail;
    if (!doc_.name.isEmpty()) {
        if (gate_.profile() == edition::UiProfile::Viewer)
            tail << tr("[Chỉ xem]");
        else if (doc_.readOnly)
            tail << tr("[Chỉ đọc]");
        if (doc_.recovered)
            tail << tr("[Đã phục hồi]");
        tail << u"—"_s;
    }
    tail << name;
    title_->set(doc_.name, doc_.dirty, tail);
    ui::RichTooltip::attach(title_, {.name = doc_.path.isEmpty() ? doc_.name : doc_.path}); // đủ đường dẫn

    const QString appName = tr("Menu ứng dụng");
    app_->setAccessibleName(appName);
    ui::RichTooltip::attach(app_, {.name = appName,
                                   .description = tr("Mở, xuất, in, tiện ích bản vẽ, tùy chọn và tệp gần đây.")});
    const QString userName = tr("Tài khoản"), helpName = tr("Trợ giúp");
    user_->setAccessibleName(userName);
    ui::RichTooltip::attach(user_, {.name = userName});
    help_->setAccessibleName(helpName);
    ui::RichTooltip::attach(help_, {.name = helpName, .keyId = u"help"_s});

    const auto screen = [this](ui::Menu* m, const QString& icon, const QString& text, const QString& id) {
        connect(m->addAction(IconProvider::icon(icon), text), &QAction::triggered, this,
                [this, id] { emit screenRequested(id); });
    };
    const auto command = [this](ui::Menu* m, const QString& id) {
        if (QAction* a = actions_.action(id))
            m->addAction(a); // QAction của ActionRegistry: phím, khóa, cổng phiên bản như ribbon
    };

    // Menu tài khoản (userMenu của prototype).
    userMenu_->clear();
    userMenu_->setAccessibleName(userName);
    if (account_.signedIn)
        userMenu_->addSection(u"%1 · %2"_s.arg(account_.name, account_.email));
    screen(userMenu_, u"license"_s,
           gate_.edition() == Edition::Trial ? tr("%1 · còn %n ngày", nullptr, trialDays_).arg(name) : name,
           u"license"_s);
    screen(userMenu_, u"key"_s, tr("Trạng thái giấy phép"), u"license"_s);
    screen(userMenu_, u"key"_s, account_.signedIn ? tr("Kích hoạt, nhập key…") : tr("Đăng nhập, kích hoạt…"),
           u"activate"_s);
    if (account_.signedIn) {
        connect(userMenu_->addAction(IconProvider::icon(u"ext"_s), tr("Quản lý tài khoản")), &QAction::triggered,
                this, [this] { openUrl_(accountUrl()); });
        userMenu_->addSeparator();
        connect(userMenu_->addAction(IconProvider::icon(u"exit"_s), tr("Đăng xuất")), &QAction::triggered, this,
                &TitleBar::signOutRequested);
    }

    // Menu trợ giúp (helpMenu của prototype).
    helpMenu_->clear();
    helpMenu_->setAccessibleName(helpName);
    command(helpMenu_, u"HELP"_s);
    screen(helpMenu_, u"play"_s, tr("Chuyển từ AutoCAD sang"), u"migrate"_s);
    screen(helpMenu_, u"keyboard"_s, tr("Bảng phím tắt"), u"keys"_s);
    helpMenu_->addSeparator();
    screen(helpMenu_, u"chat"_s, tr("Liên hệ hỗ trợ"), u"support"_s);
    command(helpMenu_, u"TCREPORT"_s);
    screen(helpMenu_, u"thumb"_s, tr("Góp ý"), u"feedback"_s);
    helpMenu_->addSeparator();
    screen(helpMenu_, u"update"_s, tr("Kiểm tra cập nhật"), u"update"_s);
    command(helpMenu_, u"ABOUT"_s);
}

void TitleBar::popupBelow(ui::Menu* menu, QWidget* button)
{
    // Mép phải menu theo mép phải nút, cách --sp-1 (prototype: alignRight, r.bottom + 2).
    menu->popup(button->mapToGlobal(QPoint(button->width() - menu->sizeHint().width(), button->height() + px(tok::sp_1))));
}

void TitleBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), col(tok::c_chrome));                              // .tb
    p.fillRect(QRect(0, height() - 1, width(), 1), col(tok::c_border)); // border-bottom 1px
}

void TitleBar::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        sync();
    QWidget::changeEvent(e);
}

} // namespace trion::shell
