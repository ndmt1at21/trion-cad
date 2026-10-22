#include "app/core/actions/ActionRegistry.h"

#include "app/core/i18n/Translator.h"
#include "app/core/keymap/Keymap.h"
#include "app/theme/icons/IconProvider.h"
#include "app/widgets/tooltip/RichTooltip.h"

#include <QAction>
#include <QCoreApplication>
#include <QKeySequence>
#include <QWidget>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <utility>

using namespace Qt::StringLiterals;

namespace trion::actions {

void Sysvars::set(const QString& name, int value)
{
    if (std::exchange(values_[name], value) != value)
        emit changed(name, value);
}

namespace {

// Hành động gắn sysvar: bấm gửi 'TÊN giá-trị-mới (trong suốt). 07.1 §10; prototype renderStatus (shell.js), LIVE
// (screens14.js); giá trị theo AutoCAD.
// ponytail: bảng tạm thay metadata sysvar của INT-108 (miền giá trị); thêm ISODRAFT (P1), ANNOALLVISIBLE,
// ANNOAUTOSCALE khi thanh trạng thái (UIA-112) dùng.
struct SysvarAction {
    const char* id; // id của Keymap, tên lệnh trong danh mục, hoặc tên sysvar
    const char* sysvar;
    enum Kind : std::uint8_t { Bit, Sign, Cycle } kind;
    int arg; // Bit: mặt nạ, âm khi bit bật nghĩa là tắt (OSMODE 16384); Sign: bật lại từ 0 thì đặt arg,
             // tắt là đổi dấu (giữ lựa chọn cũ); Cycle: đổi vòng 0 … arg-1, không phải toggle
    const char* need;  // như CommandInfo::need
    const char* icon;  // "" nếu không có
    const char* label; // nhãn khi không có trong Keymap lẫn danh mục (ngữ cảnh "actions")
};

constexpr SysvarAction kSysvarActions[] = {
    {"osnap", "OSMODE", SysvarAction::Bit, -16384, "V", "osnap", nullptr},
    {"grid", "GRIDMODE", SysvarAction::Bit, 1, "PRO", "grid", nullptr},
    {"snap", "SNAPMODE", SysvarAction::Bit, 1, "PRO", "snap", nullptr},
    {"ortho", "ORTHOMODE", SysvarAction::Bit, 1, "PRO", "ortho", nullptr},
    {"polar", "AUTOSNAP", SysvarAction::Bit, 8, "PRO", "polar", nullptr},
    {"otrack", "AUTOSNAP", SysvarAction::Bit, 16, "PRO", "otrack", nullptr},
    {"dyn", "DYNMODE", SysvarAction::Sign, 3, "PRO", "dyn", nullptr},
    {"qp", "QPMODE", SysvarAction::Sign, 1, "PRO", "qprops", nullptr},
    {"infer", "CONSTRAINTINFER", SysvarAction::Bit, 1, "PRO", "infer", nullptr},
    {"cycle", "SELECTIONCYCLING", SysvarAction::Sign, 2, "PRO", "cycle", nullptr},
    {"isop", "SNAPISOPAIR", SysvarAction::Cycle, 3, "PRO", "iso", nullptr},
    {"coord", "COORDS", SysvarAction::Cycle, 3, "V", "", nullptr},
    {"PICKSTYLE", "PICKSTYLE", SysvarAction::Bit, 1, "PRO", "select", nullptr},
    {"LWDISPLAY", "LWDISPLAY", SysvarAction::Bit, 1, "V", "lw", QT_TRANSLATE_NOOP("actions", "Hiển thị độ dày nét")},
    {"TRANSPARENCYDISPLAY", "TRANSPARENCYDISPLAY", SysvarAction::Bit, 1, "V", "transp",
     QT_TRANSLATE_NOOP("actions", "Hiển thị độ trong suốt")},
};

bool isOn(const SysvarAction& s, int v)
{
    return s.kind == SysvarAction::Sign ? v > 0 : ((v & std::abs(s.arg)) != 0) == (s.arg > 0);
}

int next(const SysvarAction& s, int v)
{
    switch (s.kind) {
    case SysvarAction::Bit:
        return v ^ std::abs(s.arg);
    case SysvarAction::Sign:
        return v != 0 ? -v : s.arg;
    case SysvarAction::Cycle:
        break;
    }
    return v >= 0 && v < s.arg - 1 ? v + 1 : 0;
}

const SysvarAction* sysvarAction(QStringView id)
{
    for (const SysvarAction& s : kSysvarActions)
        if (id == QLatin1StringView(s.id))
            return &s;
    return nullptr;
}

// Lệnh chạy trong suốt ("'", không hủy lệnh đang chạy).
// ponytail: tạm thay cờ "trong suốt" của metadata INT-101: lệnh AutoCAD cho dùng trong suốt, cộng lệnh chỉ đổi giao
// diện có phím tắt (palette Ctrl+1…4, 8; dòng lệnh Ctrl+9; Clean Screen Ctrl+0; viewport kích hoạt Ctrl+R) để bấm
// giữa lệnh không ngắt lệnh. INT-101 quyết định khi có.
constexpr const char* kTransparent[] = {
    "ATTDISP",     "BASE",       "COLOR",    "DDPTYPE",      "DIST",   "DSETTINGS", "HELP",
    "ID",          "LAYER",      "LINETYPE", "LWEIGHT",      "PAN",    "QUICKCALC", "SCRIPT",
    "SETVAR",      "STYLE",      "TEXTSCR",  "UNITS",        "ZOOM",   "ADCENTER",  "CLEANSCREENON",
    "COMMANDLINE", "PROPERTIES", "SHEETSET", "TOOLPALETTES", "VPNEXT",
};

ui::Edition edition(const char* need)
{
    return std::strcmp(need, "PRO") == 0  ? ui::Edition::Pro
           : std::strcmp(need, "VP") == 0 ? ui::Edition::ViewerPro
                                          : ui::Edition::None;
}

} // namespace

struct ActionRegistry::Entry {
    QAction* action = nullptr;     // con của registry
    QString command;               // tên lệnh; hành động gắn sysvar: lệnh hay sysvar hiện ở tooltip
    QString keyId;                 // id trong Keymap
    const char* context = nullptr; // ngữ cảnh dịch của label: "actions" hoặc "keymap"
    QByteArray label;              // nhãn tiếng Việt gốc
    const SysvarAction* sysvar = nullptr;
    ui::Edition need = ui::Edition::None;
    bool transparent = false;
    bool viewport = false; // phạm vi Vùng vẽ
};

ActionRegistry::ActionRegistry(keymap::Keymap& keymap, Sysvars& sysvars, QObject* parent)
    : QObject(parent), keymap_(keymap), sysvars_(sysvars)
{
    const auto add = [this](const QString& id, const QString& command, const QString& keyId, const char* context,
                            QByteArray label, const char* need, const char* icon, const SysvarAction* s) {
        index_.insert(id, qsizetype(entries_.size()));
        auto* a = new QAction(this);
        a->setObjectName(id);
        if (*icon)
            a->setIcon(theme::IconProvider::icon(QString::fromLatin1(icon)));
        if (s && s->kind != SysvarAction::Cycle) {
            a->setCheckable(true);
            a->setChecked(isOn(*s, sysvars_.get(QString::fromLatin1(s->sysvar))));
            a->setAutoRepeat(false); // giữ F8 không nhấp nháy ORTHO
        }
        const bool transparent =
            std::ranges::any_of(kTransparent, [&](const char* t) { return command == QLatin1StringView(t); });
        entries_.push_back({a, command, keyId, context, std::move(label), s, edition(need), transparent});
        list_ << a;
    };

    // Lệnh của danh mục (thay INT-001)
    for (const CommandInfo& c : commands()) {
        const QString name = QString::fromLatin1(c.name);
        add(name, name, name, "actions", c.label, c.need, c.icon, sysvarAction(name));
    }
    // Hành động của Keymap (KDEF): phím cho lệnh của danh mục; hành động gắn sysvar; lệnh ngoài danh mục. Bỏ hành
    // động "—" (không phải lệnh) và lệnh khác người dùng tự gán phím (id là tên lệnh: đã có nếu thuộc danh mục).
    for (const keymap::Action& k : keymap.actions()) {
        if (k.defaultKeys.isEmpty() || k.command == u"—"_s)
            continue;
        if (const SysvarAction* s = sysvarAction(k.id))
            add(k.id, k.command, k.id, "keymap", k.name.toUtf8(), s->need, s->icon, s);
        else if (const qsizetype i = index_.value(k.command, -1); i >= 0)
            entries_[size_t(i)].keyId = k.id;
        else
            add(k.command, k.command, k.id, "keymap", k.name.toUtf8(), "V", "", nullptr);
    }
    // Sysvar không có trong Keymap lẫn danh mục
    for (const SysvarAction& s : kSysvarActions) {
        const QString id = QString::fromLatin1(s.id);
        if (s.label && !index_.contains(id))
            add(id, QString::fromLatin1(s.sysvar), id, "actions", s.label, s.need, s.icon, &s);
    }

    for (size_t i = 0; i < entries_.size(); ++i) {
        Entry& e = entries_[i];
        const keymap::Action* k = keymap.action(e.keyId);
        e.viewport = k && k->scope == keymap::Scope::Viewport; // phạm vi theo KDEF; lệnh người dùng gán là Toàn cục
        e.action->setShortcutContext(e.viewport ? Qt::WidgetShortcut : Qt::ApplicationShortcut);
        ui::RichTooltip::attach(e.action, {.command = e.command, .keyId = e.keyId});
        connect(e.action, &QAction::triggered, this, [this, i] { activate(entries_[i]); });
    }
    applyKeys();
    retranslate();

    connect(&keymap_, &keymap::Keymap::changed, this, &ActionRegistry::applyKeys);
    connect(&sysvars_, &Sysvars::changed, this, [this](const QString& name, int value) {
        for (Entry& e : entries_)
            if (e.sysvar && e.action->isCheckable() && name == QLatin1StringView(e.sysvar->sysvar))
                e.action->setChecked(isOn(*e.sysvar, value));
    });
    if (const auto* t = i18n::Translator::instance())
        connect(t, &i18n::Translator::languageChanged, this, &ActionRegistry::retranslate);
}

ActionRegistry::~ActionRegistry() = default;

const ActionRegistry::Entry* ActionRegistry::find(const QString& id) const
{
    const qsizetype i = index_.value(id, -1);
    return i < 0 ? nullptr : &entries_[size_t(i)];
}

QAction* ActionRegistry::action(const QString& id) const
{
    const Entry* e = find(id);
    return e ? e->action : nullptr;
}

QString ActionRegistry::macro(const QString& id) const
{
    const Entry* e = find(id);
    return e ? macro(*e) : QString();
}

QString ActionRegistry::macro(const Entry& e) const
{
    if (const SysvarAction* s = e.sysvar) {
        const QString name = QString::fromLatin1(s->sysvar);
        return u"'%1 %2"_s.arg(name).arg(next(*s, sysvars_.get(name)));
    }
    // Clean Screen (Ctrl+0, nút thanh trạng thái) đổi vòng như macro CUI của AutoCAD: DIESEL $M=$(if,…) chọn
    // _cleanscreenoff khi CLEANSCREENSTATE bật, _cleanscreenon khi tắt (UIA-001).
    const QString command =
        e.command == u"CLEANSCREENON"_s && sysvars_.get(u"CLEANSCREENSTATE"_s) != 0 ? u"CLEANSCREENOFF"_s : e.command;
    return (e.transparent ? u"'_"_s : u"^C^C_"_s) + command;
}

ui::Edition ActionRegistry::need(const QString& id) const
{
    const Entry* e = find(id);
    return e ? e->need : ui::Edition::None;
}

void ActionRegistry::activate(Entry& e)
{
    if ((!gate_ || gate_(e.action->objectName())) && sink_)
        sink_(macro(e));
    if (e.sysvar && e.action->isCheckable()) // Qt đã đảo trạng thái khi bấm; trạng thái chỉ theo sysvar
        e.action->setChecked(isOn(*e.sysvar, sysvars_.get(QString::fromLatin1(e.sysvar->sysvar))));
}

void ActionRegistry::applyKeys()
{
    for (Entry& e : entries_) {
        QList<QKeySequence> keys;
        if (const keymap::Action* k = keymap_.action(e.keyId))
            for (const QKeyCombination c : k->bindings)
                keys << QKeySequence(c);
        e.action->setShortcuts(keys);
    }
}

void ActionRegistry::retranslate()
{
    for (Entry& e : entries_)
        e.action->setText(QCoreApplication::translate(e.context, e.label.constData()));
}

void ActionRegistry::install(QWidget* window)
{
    addTo(window, false);
}

void ActionRegistry::addViewport(QWidget* viewport)
{
    addTo(viewport, true);
}

void ActionRegistry::addTo(QWidget* w, bool viewport)
{
    for (const Entry& e : entries_)
        if (e.viewport == viewport)
            w->addAction(e.action);
}

} // namespace trion::actions
