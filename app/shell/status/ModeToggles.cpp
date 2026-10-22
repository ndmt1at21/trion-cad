#include "app/shell/status/ModeToggles.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/tooltip/RichTooltip.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QStyleOptionFocusRect>
#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QToolButton>

#include <cstdlib>
#include <span>

using namespace Qt::StringLiterals;

namespace trion::shell {

using theme::ThemeManager;

// Mục menu gắn sysvar: dấu chọn theo Sysvars, bấm gửi 'TÊN giá-trị (trong suốt).
struct ModeToggles::Choice {
    const char* label; // chữ gốc, ngữ cảnh dịch trion::shell::ModeToggles
    const char* sysvar;
    bool bit; // true: đảo bit arg, giữ dấu (DYNMODE âm là DYN tắt mà vẫn nhớ chế độ); false: đặt giá trị arg
    int arg;
};

struct ModeToggles::Spec {
    const char* id;     // QAction gắn sysvar của ActionRegistry
    const char* sysvar; // tên ở tooltip; F1 mở trợ giúp của nó
    const char* keyId;  // phím ở tooltip (Keymap); "" nếu không có
    int tab;            // "Thiết lập…": tab của +DSETTINGS, kLweight hoặc kNoSettings (không có menu)
    std::span<const Choice> choices;
    bool viewer; // còn ở hồ sơ Viewer
};

namespace {

using Choice = ModeToggles::Choice;

constexpr int kLweight = -1;     // Spec::tab: "Thiết lập…" là LWEIGHT
constexpr int kNoSettings = -2;  // Spec::tab: nút không có menu
constexpr int kOsnapOff = 16384; // OSMODE: bit bật là OSNAP tắt (F3); đổi chế độ giữ nguyên bit này
constexpr int kOsnapAll = 16383; // mọi chế độ của kOsnap

// Bit OSMODE của AutoCAD, thứ tự của prototype (osnapMenu).
constexpr Choice kOsnap[] = {
    {QT_TRANSLATE_NOOP("trion::shell::ModeToggles", "Điểm cuối"), "OSMODE", true, 1},
    {QT_TRANSLATE_NOOP("trion::shell::ModeToggles", "Trung điểm"), "OSMODE", true, 2},
    {QT_TRANSLATE_NOOP("trion::shell::ModeToggles", "Tâm"), "OSMODE", true, 4},
    {QT_TRANSLATE_NOOP("trion::shell::ModeToggles", "Tâm hình học"), "OSMODE", true, 1024},
    {QT_TRANSLATE_NOOP("trion::shell::ModeToggles", "Điểm"), "OSMODE", true, 8},
    {QT_TRANSLATE_NOOP("trion::shell::ModeToggles", "Góc phần tư"), "OSMODE", true, 16},
    {QT_TRANSLATE_NOOP("trion::shell::ModeToggles", "Giao điểm"), "OSMODE", true, 32},
    {QT_TRANSLATE_NOOP("trion::shell::ModeToggles", "Kéo dài"), "OSMODE", true, 4096},
    {QT_TRANSLATE_NOOP("trion::shell::ModeToggles", "Điểm chèn"), "OSMODE", true, 64},
    {QT_TRANSLATE_NOOP("trion::shell::ModeToggles", "Vuông góc"), "OSMODE", true, 128},
    {QT_TRANSLATE_NOOP("trion::shell::ModeToggles", "Tiếp xúc"), "OSMODE", true, 256},
    {QT_TRANSLATE_NOOP("trion::shell::ModeToggles", "Gần nhất"), "OSMODE", true, 512},
    {QT_TRANSLATE_NOOP("trion::shell::ModeToggles", "Giao biểu kiến"), "OSMODE", true, 2048},
    {QT_TRANSLATE_NOOP("trion::shell::ModeToggles", "Song song"), "OSMODE", true, 8192},
};
constexpr Choice kSnap[] = {
    {QT_TRANSLATE_NOOP("trion::shell::ModeToggles", "Snap lưới"), "SNAPTYPE", false, 0},
    {QT_TRANSLATE_NOOP("trion::shell::ModeToggles", "Snap polar"), "SNAPTYPE", false, 1},
};
constexpr Choice kDyn[] = {
    {QT_TRANSLATE_NOOP("trion::shell::ModeToggles", "Nhập con trỏ"), "DYNMODE", true, 1},
    {QT_TRANSLATE_NOOP("trion::shell::ModeToggles", "Nhập kích thước"), "DYNMODE", true, 2},
    {QT_TRANSLATE_NOOP("trion::shell::ModeToggles", "Prompt động"), "DYNPROMPT", true, 1},
};
constexpr Choice kIso[] = {
    {QT_TRANSLATE_NOOP("trion::shell::ModeToggles", "Mặt trái"), "SNAPISOPAIR", false, 0},
    {QT_TRANSLATE_NOOP("trion::shell::ModeToggles", "Mặt trên"), "SNAPISOPAIR", false, 1},
    {QT_TRANSLATE_NOOP("trion::shell::ModeToggles", "Mặt phải"), "SNAPISOPAIR", false, 2},
};
constexpr const char* kAngles[] = {"90", "45", "30", "22.5", "18", "15", "10", "5"}; // góc tăng polar (07.1 §10)

// Thứ tự prototype (renderStatus). Tab +DSETTINGS của AutoCAD: 0 Snap và lưới, 1 Polar, 2 Bắt điểm, 4 Nhập động;
// nút nào mở tab nào theo prototype (tabOf).
constexpr ModeToggles::Spec kSpecs[] = {
    {"grid", "GRIDMODE", "grid", 0, {}, false},
    {"snap", "SNAPMODE", "snap", 0, kSnap, false},
    {"dyn", "DYNMODE", "dyn", 4, kDyn, false},
    {"ortho", "ORTHOMODE", "ortho", 0, {}, false},
    {"polar", "AUTOSNAP", "polar", 1, {}, false}, // góc: addAngles
    {"iso", "SNAPSTYL", "isop", 0, kIso, false},  // chờ ActionRegistry có "iso" (ModeToggles.h)
    {"otrack", "AUTOSNAP", "otrack", 2, {}, false},
    {"osnap", "OSMODE", "osnap", 2, kOsnap, true},
    {"LWDISPLAY", "LWDISPLAY", "", kLweight, {}, true},
    {"TRANSPARENCYDISPLAY", "TRANSPARENCYDISPLAY", "", kNoSettings, {}, true},
};

bool is(const ModeToggles::Spec* s, const char* id)
{
    return qstrcmp(s->id, id) == 0;
}

int px(tok::PxId id)
{
    return qRound(ThemeManager::instance().px(id));
}

QString settingsCommand(int tab)
{
    return tab == kLweight ? u"LWEIGHT"_s : u"DSETTINGS"_s;
}

} // namespace

StatusButton::StatusButton(QWidget* parent) : QToolButton(parent)
{
    setAutoRaise(true);
}

QSize StatusButton::sizeHint() const
{
    return {px(tok::h_btn) + px(tok::sp_2), px(tok::h_btn)}; // 25 × 21 ở mật độ gọn
}

void StatusButton::popupAbove(QMenu* m, const QWidget* anchor, bool alignRight)
{
    const QSize s = m->sizeHint();
    m->popup(anchor->mapToGlobal(QPoint(alignRight ? anchor->width() - s.width() : 0, -s.height())));
}

void StatusButton::paintEvent(QPaintEvent*)
{
    QStylePainter p(this);
    QStyleOptionToolButton o;
    initStyleOption(&o);
    o.state &= ~QStyle::State_HasFocus; // vòng focus của style lùi vào 3 px, đè biểu tượng: vẽ sát mép ở dưới
    p.drawComplexControl(QStyle::CC_ToolButton, o);
    const ThemeManager& tm = ThemeManager::instance();
    if (isChecked()) {
        const int inset = px(tok::sp_2), h = px(tok::sp_1);
        p.fillRect(QRect(inset, height() - h, width() - 2 * inset, h), tm.color(tok::c_on));
    }
    if (dot) { // .has-dot::after: chấm --sp-3 cách mép trên --sp-1, mép phải --sp-2, viền nền thanh tách khỏi biểu tượng
        const qreal d = tm.px(tok::sp_3), ring = tm.px(tok::sp_1) * 0.75;
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(tm.color(tok::c_chrome), ring));
        p.setBrush(tm.color(tok::c_danger));
        p.drawEllipse(QRectF(width() - tm.px(tok::sp_2) - d, tm.px(tok::sp_1), d, d));
    }
    if (hasFocus()) { // TrionStyle chỉ vẽ khi focus đến từ bàn phím (:focus-visible)
        QStyleOptionFocusRect f;
        f.initFrom(this);
        p.drawPrimitive(QStyle::PE_FrameFocusRect, f);
    }
}

void StatusButton::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
        click();
    else if (menu && (e->key() == Qt::Key_Menu || e->keyCombination() == QKeyCombination(Qt::ShiftModifier, Qt::Key_F10)))
        popupAbove(menu, this);
    else
        QToolButton::keyPressEvent(e);
}

void StatusButton::contextMenuEvent(QContextMenuEvent* e)
{
    if (menu)
        menu->popup(e->globalPos() - QPoint(0, menu->sizeHint().height())); // mở lên trên (prototype)
}

ModeToggles::ModeToggles(actions::ActionRegistry& actions, actions::Sysvars& sysvars, QWidget* parent)
    : QWidget(parent), actions_(actions), sysvars_(sysvars)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins({});
    for (const Spec& s : kSpecs) {
        QAction* action = actions.action(QString::fromLatin1(s.id));
        if (!action)
            continue;
        auto* b = new StatusButton(this);
        b->setDefaultAction(action); // checkable theo sysvar; bấm gửi lệnh qua sink (và cổng phiên bản) của registry
        row->addWidget(b);
        buttons_.emplace_back(&s, b);
        if (s.tab == kNoSettings)
            continue;

        auto* m = new ui::Menu(b);
        b->menu = m;
        const bool osnap = is(&s, "osnap"), polar = is(&s, "polar");
        if (osnap) {
            labels_.emplace_back(m->addSection(QString()), QT_TR_NOOP("Chế độ bắt điểm"));
            m->installEventFilter(this);
        }
        for (const Choice& c : s.choices)
            addChoice(m, c);
        if (polar)
            addAngles(m);
        if (!m->isEmpty())
            m->addSeparator();
        if (osnap)
            for (const bool all : {true, false}) {
                QAction* a = m->addAction(QString());
                labels_.emplace_back(a, all ? QT_TR_NOOP("Chọn tất cả") : QT_TR_NOOP("Bỏ tất cả"));
                connect(a, &QAction::triggered, this, [this, all] {
                    const int off = sysvars_.get(u"OSMODE"_s) & kOsnapOff;
                    setSysvar(u"OSMODE"_s, all ? off | kOsnapAll : off);
                });
            }
        QAction* settings = m->addAction(QString());
        labels_.emplace_back(settings, osnap   ? QT_TR_NOOP("Thiết lập bắt điểm…")
                                       : polar ? QT_TR_NOOP("Thiết lập theo dõi…")
                                               : QT_TR_NOOP("Thiết lập…"));
        connect(settings, &QAction::triggered, this, [this, tab = s.tab] { openSettings(tab); });
        // Huy hiệu khóa theo QAction của lệnh (EditionGate::bind đặt TipInfo::need khi phiên bản không có lệnh)
        connect(m, &QMenu::aboutToShow, this, [this, settings, tab = s.tab] {
            const QString command = settingsCommand(tab);
            ui::RichTooltip::attach(settings,
                                    {.command = command, .need = ui::RichTooltip::info(actions_.action(command)).need});
        });
    }

    const auto metrics = [this, row] {  // token đọc lại khi đổi chủ đề, mật độ
        row->setSpacing(px(tok::sp_1)); // .sb gap
        for (const auto& [s, b] : buttons_) {
            b->setIconSize(QSize(px(tok::ic), px(tok::ic)));
            b->updateGeometry();
        }
    };
    metrics();
    connect(&ThemeManager::instance(), &ThemeManager::changed, this, metrics);
    connect(&sysvars_, &actions::Sysvars::changed, this, &ModeToggles::sync);
    sync();
    retranslate();
}

void ModeToggles::addChoice(QMenu* menu, const Choice& c)
{
    QAction* a = menu->addAction(QString());
    a->setCheckable(true);
    labels_.emplace_back(a, c.label);
    choices_.emplace_back(a, &c);
    connect(a, &QAction::triggered, this, [this, &c] {
        const QString name = QString::fromLatin1(c.sysvar);
        const int v = sysvars_.get(name);
        setSysvar(name, !c.bit ? c.arg : v < 0 ? -(-v ^ c.arg) : v ^ c.arg);
    });
}

void ModeToggles::addAngles(QMenu* menu)
{
    const auto add = [this, menu](const QString& angle) {
        QAction* a = menu->addAction(u"%1°"_s.arg(angle));
        a->setCheckable(true);
        a->setData(angle);
        angles_.push_back(a);
        connect(a, &QAction::triggered, this, [this, a] {
            emit macroRequested(u"'POLARANG %1"_s.arg(a->data().toString()));
            if (QAction* polar = actions_.action(u"polar"_s); !polar->isChecked())
                polar->trigger(); // chọn góc thì bật Polar (prototype, AutoCAD)
            sync();
        });
    };
    for (const char* angle : kAngles)
        add(QString::fromLatin1(angle));
    add(QString()); // góc người dùng: hiện khi POLARANG không thuộc danh sách (sync)
    QAction* other = menu->addAction(QString());
    labels_.emplace_back(other, QT_TR_NOOP("Góc khác…"));
    // Không kèm giá trị: dòng lệnh hỏi "POLARANG <90>:" như gõ 'POLARANG
    connect(other, &QAction::triggered, this, [this] { emit macroRequested(u"'POLARANG"_s); });
}

void ModeToggles::setSysvar(const QString& name, int value)
{
    emit macroRequested(u"'%1 %2"_s.arg(name).arg(value));
    sync(); // Qt đã đảo dấu chọn khi bấm; dấu chọn chỉ theo Sysvars (lõi áp lệnh thì Sysvars::changed gọi lại)
}

void ModeToggles::openSettings(int tab)
{
    if (QAction* a = actions_.action(settingsCommand(tab)); a && ui::RichTooltip::info(a).need != ui::Edition::None)
        a->trigger(); // khóa theo phiên bản: cổng UI0-011 mở trang nâng cấp, không gửi lệnh
    else
        emit macroRequested(tab == kLweight ? u"'_LWEIGHT"_s : u"'_+DSETTINGS %1"_s.arg(tab));
}

void ModeToggles::sync()
{
    for (const auto& [a, c] : choices_) {
        const int v = sysvars_.get(QString::fromLatin1(c->sysvar));
        a->setChecked(c->bit ? (std::abs(v) & c->arg) != 0 : v == c->arg);
    }
    if (angles_.empty())
        return;
    // ponytail: Sysvars tạm (INT-005) chỉ có số nguyên: POLARANG 22.5 không đọc được nên không đánh dấu; INT-005 có
    // số thực thì so số thực.
    const int value = sysvars_.get(u"POLARANG"_s);
    const QString angle = QString::number(value);
    QAction* user = angles_.back();
    bool preset = false;
    for (QAction* a : angles_)
        if (a != user) {
            a->setChecked(a->data().toString() == angle);
            preset |= a->isChecked();
        }
    user->setVisible(!preset && value != 0); // 0: chưa đặt
    user->setData(angle);
    user->setText(u"%1°"_s.arg(angle));
    user->setChecked(true);
}

void ModeToggles::setProfile(edition::UiProfile profile)
{
    viewer_ = profile == edition::UiProfile::Viewer;
    for (const auto& [s, b] : buttons_)
        b->setVisible(!viewer_ || s->viewer);
    retranslate(); // tên OSNAP
}

QToolButton* ModeToggles::button(const QString& id) const
{
    for (const auto& [s, b] : buttons_)
        if (id == QLatin1StringView(s->id))
            return b;
    return nullptr;
}

void ModeToggles::retranslate()
{
    for (const auto& [s, b] : buttons_) {
        const QString sysvar = QString::fromLatin1(s->sysvar);
        const QString name = !is(s, "osnap") ? b->defaultAction()->text()
                             : viewer_       ? tr("Bắt điểm (khi đo)")
                                             : tr("Bắt điểm");
        const QString about = s->tab == kNoSettings ? tr("Biến hệ thống %1.").arg(sysvar)
                                                    : tr("Biến hệ thống %1. Chuột phải để xem thiết lập.").arg(sysvar);
        b->setAccessibleName(name);
        b->setAccessibleDescription(about);
        ui::RichTooltip::attach(b, {.command = sysvar, .keyId = QString::fromLatin1(s->keyId), .description = about});
    }
    for (const auto& [a, label] : labels_)
        a->setText(tr(label));
}

void ModeToggles::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

bool ModeToggles::eventFilter(QObject* watched, QEvent* e)
{
    // Menu OSNAP: bấm hoặc Enter/Space trên một chế độ chỉ bật/tắt nó, menu mở tiếp (prototype mở lại menu) để đổi
    // nhiều chế độ liền; mục khác đóng menu như thường.
    auto* menu = qobject_cast<QMenu*>(watched);
    QAction* a = nullptr;
    if (menu && e->type() == QEvent::MouseButtonRelease)
        a = menu->actionAt(static_cast<QMouseEvent*>(e)->position().toPoint());
    else if (menu && e->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent*>(e)->key();
        if (key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Space)
            a = menu->activeAction();
    }
    if (!a || !a->isCheckable() || !a->isEnabled())
        return QWidget::eventFilter(watched, e);
    a->trigger();
    return true;
}

} // namespace trion::shell
