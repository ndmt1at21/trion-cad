#include "app/shell/titlebar/Qat.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/core/edition/EditionGate.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/controls/Paint.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/tooltip/RichTooltip.h"

#include <QAction>
#include <QBoxLayout>
#include <QEvent>
#include <QPainter>

#include <algorithm>
#include <array>

using namespace Qt::StringLiterals;

namespace trion::shell {

namespace {

// Mục có sẵn của QAT và menu tùy biến (07.1 UI-A04; qatMenu của prototype): lệnh, nhãn trong menu, nhóm (vạch chia).
struct Spec {
    const char* command;
    const char* label;
    int zone;
};
constexpr Spec kItems[] = {
    {"NEW", QT_TRANSLATE_NOOP("trion::shell::Qat", "Mới"), 0},
    {"OPEN", QT_TRANSLATE_NOOP("trion::shell::Qat", "Mở"), 0},
    {"QSAVE", QT_TRANSLATE_NOOP("trion::shell::Qat", "Lưu"), 0},
    {"SAVEAS", QT_TRANSLATE_NOOP("trion::shell::Qat", "Lưu thành"), 0},
    {"U", QT_TRANSLATE_NOOP("trion::shell::Qat", "Hoàn tác"), 1},
    {"REDO", QT_TRANSLATE_NOOP("trion::shell::Qat", "Làm lại"), 1},
    {"PLOT", QT_TRANSLATE_NOOP("trion::shell::Qat", "In"), 2},
    {"MATCHPROP", QT_TRANSLATE_NOOP("trion::shell::Qat", "Khớp thuộc tính"), 2},
    {"TCBATCHPLOT", QT_TRANSLATE_NOOP("trion::shell::Qat", "In hàng loạt"), 2},
    {"PREVIEW", QT_TRANSLATE_NOOP("trion::shell::Qat", "Xem trước bản in"), 2},
    {"PROPERTIES", QT_TRANSLATE_NOOP("trion::shell::Qat", "Thuộc tính"), 2},
    {"WORKSPACE", QT_TRANSLATE_NOOP("trion::shell::Qat", "Chuyển không gian làm việc"), 2},
    {"FIXFONT", QT_TRANSLATE_NOOP("trion::shell::Qat", "Bác sĩ Font"), 2},
};
// Menu của nút tách In (prototype: data-qlist="print"); mục khóa có huy hiệu, cổng phiên bản chặn khi bấm.
constexpr const char* kPrintMenu[] = {"PLOT", "PUBLISH", "TCBATCHPLOT", "PREVIEW"};
constexpr int kListWidth = 220; // danh sách hoàn tác: minWidth 220 của prototype (chưa có token)

QColor col(tok::ColorId id)
{
    return theme::ThemeManager::instance().color(id);
}
int px(tok::PxId id)
{
    return qRound(theme::ThemeManager::instance().px(id));
}

settings::Store& store()
{
    return settings::Store::instance();
}

// Tên biểu tượng của lệnh trong danh mục (Button::setIconName: bản khóa có huy hiệu ổ khóa, không đổi cỡ nút).
QString iconOf(const QString& command)
{
    for (const actions::CommandInfo& c : actions::commands())
        if (command == QLatin1StringView(c.name))
            return QString::fromLatin1(c.icon);
    return {};
}

// Nút ▾ (.ib chỉ có mũi tên .ic.xs).
class ChevronButton final : public ui::Button {
public:
    ChevronButton() : Button({}, Kind::Icon) {}

protected:
    void paintEvent(QPaintEvent* e) override
    {
        Button::paintEvent(e); // nền rê/nhấn, viền focus
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        ui::paint::drawChevron(p, QRectF(rect()).center(),
                               col(!isEnabled() ? tok::c_text_dis : underMouse() || isDown() ? tok::c_text
                                                                                            : tok::c_text_2));
    }
};

// Vạch chia .sep: 1 px --c-border cao --ic, lề ngang --sp-2.
class Separator final : public QWidget {
public:
    QSize sizeHint() const override { return {1 + 2 * px(tok::sp_2), px(tok::ic)}; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter(this).fillRect(QRect(width() / 2, (height() - px(tok::ic)) / 2, 1, px(tok::ic)), col(tok::c_border));
    }
};

} // namespace

Qat::Qat(actions::ActionRegistry& actions, edition::EditionGate& gate, QWidget* parent)
    : QWidget(parent), actions_(actions), gate_(gate), menu_(new ui::Menu(this)), more_(new ChevronButton)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed); // nút không co; thiếu chỗ thì tên bản vẽ rút gọn
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins({});
    row->setSpacing(1); // style-ok: .qat gap 1px của prototype, chưa có token
    row->addWidget(more_); // nút và vạch chia chèn trước nó
    more_->setObjectName(u"qatMore"_s);
    heading_ = menu_->addSection({});
    itemsEnd_ = menu_->addSeparator();
    below_ = menu_->addAction(QString());
    below_->setCheckable(true);
    below_->setChecked(isBelowRibbon());
    if (QAction* m = actions.action(u"MENUBAR"_s)) // "Hiện thanh menu": gắn sysvar MENUBAR, gửi 'MENUBAR qua sink
        menu_->addAction(m);
    menu_->addSeparator();
    reset_ = menu_->addAction(QString());

    int zone = 0;
    for (const Spec& s : kItems) {
        QAction* a = actions.action(QString::fromLatin1(s.command));
        if (!a)
            continue;
        if (s.zone != zone) // vạch chia giữa hai nhóm
            row->insertWidget(row->count() - 1, seps_[s.zone - 1] = new Separator);
        zone = s.zone;
        addItem(a, s.label, s.zone);
    }

    connect(more_, &QAbstractButton::clicked, this, [this] { menu_->popup(more_->mapToGlobal(QPoint(0, more_->height()))); });
    connect(below_, &QAction::triggered, this, [this](bool on) {
        store().set(kBelowRibbon, on);
        emit belowRibbonChanged(on);
    });
    connect(reset_, &QAction::triggered, this, &Qat::reset);
    connect(&gate, &edition::EditionGate::editionChanged, this, &Qat::sync);
    connect(&gate, &edition::EditionGate::profileChanged, this, &Qat::apply);
    apply();
}

void Qat::addItem(QAction* a, const char* label, int zone)
{
    const QString id = a->objectName();
    ui::Button* b = nullptr;
    if (id == u"PLOT"_s || id == u"U"_s || id == u"REDO"_s) {
        auto* split = new ui::SplitButton;
        split->setKind(ui::Button::Kind::Icon);
        auto* m = new ui::Menu(split);
        if (id == u"PLOT"_s) {
            for (const char* c : kPrintMenu)
                if (QAction* p = actions_.action(QString::fromLatin1(c)))
                    m->addAction(p);
            printMenu_ = m;
        } else {
            const bool redo = id == u"REDO"_s;
            m->setMinimumWidth(kListWidth);
            connect(m, &QMenu::aboutToShow, this, [this, m, redo] { fillSteps(m, redo); });
            (redo ? redoMenu_ : undoMenu_) = m;
        }
        split->setDropMenu(m);
        b = split;
    } else {
        b = new ui::Button({}, ui::Button::Kind::Icon);
    }
    if (const QString icon = iconOf(id); !icon.isEmpty())
        b->setIconName(icon);
    else
        b->setIcon(a->icon()); // hành động gắn sysvar thêm từ ribbon: biểu tượng của QAction
    auto* row = static_cast<QBoxLayout*>(layout());
    row->insertWidget(row->count() - 1, b);
    connect(b, &QAbstractButton::clicked, a, &QAction::trigger);

    auto* t = new QAction(menu_);
    t->setCheckable(true);
    menu_->insertAction(itemsEnd_, t);
    connect(t, &QAction::triggered, this, [this, id](bool on) { setShown(id, on); });
    items_.push_back({a, b, t, label, zone});
}

ui::Button* Qat::button(const QString& command) const
{
    for (const Item& i : items_)
        if (i.action->objectName() == command)
            return i.button;
    return nullptr;
}

bool Qat::isBelowRibbon() const
{
    return store().get(kBelowRibbon);
}

const settings::Setting<QStringList>& Qat::shownSetting() const
{
    return gate_.profile() == edition::UiProfile::Viewer ? kViewerItems : kFullItems;
}

void Qat::add(const QString& command)
{
    setShown(command, true);
}

void Qat::reset()
{
    store().reset(shownSetting());
    apply();
}

void Qat::setShown(const QString& command, bool shown)
{
    QAction* a = actions_.action(command);
    if (!a)
        return;
    QStringList list = store().get(shownSetting());
    if (shown && ui::RichTooltip::info(a).need != ui::Edition::None)
        a->trigger(); // khóa: cổng phiên bản mở trang nâng cấp, không thêm
    else if (shown != list.contains(command)) {
        if (shown)
            list << command;
        else
            list.removeAll(command);
        store().set(shownSetting(), list);
    }
    apply(); // cả khi không đổi: trả dấu chọn của mục khóa vừa bật
}

void Qat::apply()
{
    const QStringList list = store().get(shownSetting());
    // Lệnh thêm từ ribbon: bỏ mục đã bớt (xóa sau: có thể đang trong tín hiệu của chính mục đó), tạo mục mới.
    std::erase_if(items_, [&](const Item& i) {
        if (i.label || list.contains(i.action->objectName()))
            return false;
        i.button->hide();
        i.button->deleteLater();
        menu_->removeAction(i.toggle);
        i.toggle->deleteLater();
        return true;
    });
    for (const QString& id : list)
        if (QAction* a = actions_.action(id); a && !button(id))
            addItem(a, nullptr, 2);

    std::array<bool, 3> any{};
    for (const Item& i : items_) {
        const bool on = list.contains(i.action->objectName());
        i.button->setVisible(on);
        i.toggle->setChecked(on);
        any[size_t(i.zone)] = any[size_t(i.zone)] || on;
    }
    for (int s = 0; s < 2; ++s)
        if (seps_[s])
            seps_[s]->setVisible(any[size_t(s)] && any[size_t(s) + 1]);
    // Thứ tự Tab theo thứ tự trên thanh: nút ▾ tạo trước, nút của lệnh thêm tạo sau mọi nút khác.
    QWidget* prev = nullptr;
    for (int i = 0; i < layout()->count(); ++i)
        if (QWidget* w = layout()->itemAt(i)->widget(); w && w->focusPolicy() != Qt::NoFocus) {
            if (prev)
                setTabOrder(prev, w);
            prev = w;
        }
    sync();
}

void Qat::fillSteps(ui::Menu* m, bool redo)
{
    m->clear();
    const StepSource& source = redo ? redoSource_ : undoSource_;
    std::vector<UndoStep> steps = source ? source(kListSteps) : std::vector<UndoStep>();
    steps.resize(std::min(steps.size(), size_t(kListSteps)));
    if (steps.empty()) {
        m->addAction(redo ? tr("Không có bước để làm lại") : tr("Không có bước để hoàn tác"))->setEnabled(false);
        return;
    }
    m->addSection(redo ? tr("Làm lại tới") : tr("Hoàn tác tới"));
    for (int k = 1; k <= int(steps.size()); ++k) {
        const UndoStep& s = steps[size_t(k - 1)];
        const QString name = s.description.isEmpty() ? s.command : s.description;
        connect(m->addAction(k == 1 ? name : tr("%1 (%n bước)", nullptr, k).arg(name)), &QAction::triggered, this,
                [this, redo, k] { emit macroRequested((redo ? u"^C^C_MREDO %1"_s : u"^C^C_UNDO %1"_s).arg(k)); });
    }
}

void Qat::sync()
{
    setAccessibleName(tr("Thanh truy cập nhanh"));
    heading_->setText(tr("Thêm vào thanh truy cập nhanh"));
    below_->setText(tr("Hiện bên dưới ribbon"));
    reset_->setText(tr("Đặt lại mặc định"));
    const QString customize = tr("Tùy biến thanh truy cập nhanh");
    menu_->setAccessibleName(customize);
    more_->setAccessibleName(customize);
    ui::RichTooltip::attach(more_, {.name = customize});
    if (printMenu_)
        printMenu_->setAccessibleName(tr("Lệnh in"));
    if (undoMenu_)
        undoMenu_->setAccessibleName(tr("Danh sách hoàn tác"));
    if (redoMenu_)
        redoMenu_->setAccessibleName(tr("Danh sách làm lại"));
    for (const Item& i : items_) {
        ui::TipInfo tip = ui::RichTooltip::info(i.action); // lệnh, phím, khóa do ActionRegistry, EditionGate đặt
        tip.name = i.action->text();
        i.button->setAccessibleName(tip.name);
        i.button->setLocked(tip.need != ui::Edition::None);
        ui::RichTooltip::attach(i.button, tip);
        i.toggle->setText(i.label ? tr(i.label) : i.action->text());
        ui::RichTooltip::attach(i.toggle, {.need = tip.need});
    }
}

void Qat::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        sync(); // chữ của QAction đã dịch lại (ActionRegistry theo Translator::languageChanged)
    QWidget::changeEvent(e);
}

} // namespace trion::shell
