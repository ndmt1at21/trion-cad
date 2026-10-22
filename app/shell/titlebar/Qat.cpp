#include "app/shell/titlebar/Qat.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/core/edition/EditionGate.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/tooltip/RichTooltip.h"

#include <QAction>
#include <QBoxLayout>
#include <QEvent>
#include <QPainter>
#include <QSignalBlocker>

using namespace Qt::StringLiterals;

namespace trion::shell {

namespace {

// Mục của QAT bản V và của menu tùy biến (qatMenu của prototype): lệnh, nhãn trong menu, hiện sẵn.
struct Spec {
    const char* command;
    const char* label;
    bool shown;
};
constexpr Spec kItems[] = {
    {"OPEN", QT_TRANSLATE_NOOP("trion::shell::Qat", "Mở"), true},
    {"PLOT", QT_TRANSLATE_NOOP("trion::shell::Qat", "In"), true},
    {"PREVIEW", QT_TRANSLATE_NOOP("trion::shell::Qat", "Xem trước bản in"), false},
    {"FIXFONT", QT_TRANSLATE_NOOP("trion::shell::Qat", "Bác sĩ Font"), false},
    {"MATCHPROP", QT_TRANSLATE_NOOP("trion::shell::Qat", "Khớp thuộc tính"), false},
    {"TCBATCHPLOT", QT_TRANSLATE_NOOP("trion::shell::Qat", "In hàng loạt"), false},
};
// Menu của nút tách In (prototype: data-qlist="print"); mục khóa có huy hiệu, cổng phiên bản chặn khi bấm.
constexpr const char* kPrintMenu[] = {"PLOT", "PUBLISH", "TCBATCHPLOT", "PREVIEW"};

QColor col(tok::ColorId id)
{
    return theme::ThemeManager::instance().color(id);
}

// Tên biểu tượng của lệnh trong danh mục (Button::setIconName: bản khóa có huy hiệu ổ khóa, không đổi cỡ nút).
QString iconOf(const char* command)
{
    for (const actions::CommandInfo& c : actions::commands())
        if (qstrcmp(c.name, command) == 0)
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
        drawChevron(p, QRectF(rect()).center(),
                    col(!isEnabled() ? tok::c_text_dis : underMouse() || isDown() ? tok::c_text : tok::c_text_2));
    }
};

} // namespace

void drawChevron(QPainter& p, QPointF c, const QColor& color)
{
    constexpr qreal u = 9 / 16.0; // .ic.xs: lưới 16 px vẽ ở 9 px
    p.save();
    p.setPen(QPen(color, tok::num(tok::sw) * 1.5 * u, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(QList<QPointF>{c + QPointF(-4 * u, -2 * u), c + QPointF(0, 2 * u), c + QPointF(4 * u, -2 * u)});
    p.restore();
}

Qat::Qat(actions::ActionRegistry& actions, edition::EditionGate& gate, QWidget* parent)
    : QWidget(parent), menu_(new ui::Menu(this)), more_(new ChevronButton)
{
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins({});
    row->setSpacing(1); // style-ok: .qat gap 1px của prototype, chưa có token
    heading_ = menu_->addSection({});
    for (const Spec& s : kItems) {
        QAction* a = actions.action(QString::fromLatin1(s.command));
        if (!a)
            continue;
        ui::Button* b = nullptr;
        if (qstrcmp(s.command, "PLOT") == 0) {
            auto* split = new ui::SplitButton;
            split->setKind(ui::Button::Kind::Icon);
            printMenu_ = new ui::Menu(split);
            for (const char* c : kPrintMenu)
                if (QAction* m = actions.action(QString::fromLatin1(c)))
                    printMenu_->addAction(m);
            split->setDropMenu(printMenu_);
            b = split;
        } else {
            b = new ui::Button({}, ui::Button::Kind::Icon);
        }
        b->setIconName(iconOf(s.command));
        row->addWidget(b);
        b->setVisible(s.shown);
        connect(b, &QAbstractButton::clicked, a, &QAction::trigger);

        QAction* t = menu_->addAction(QString());
        t->setCheckable(true);
        t->setChecked(s.shown);
        connect(t, &QAction::toggled, this, [t, a, b](bool on) {
            if (on && ui::RichTooltip::info(a).need != ui::Edition::None) { // khóa: mở trang nâng cấp, không thêm
                const QSignalBlocker block(t);
                t->setChecked(false);
                a->trigger();
                return;
            }
            b->setVisible(on);
        });
        items_.push_back({a, b, t, s.label});
    }
    row->addWidget(more_);
    more_->setObjectName(u"qatMore"_s);
    connect(more_, &QAbstractButton::clicked, this, [this] { menu_->popup(more_->mapToGlobal(QPoint(0, more_->height()))); });
    connect(&gate, &edition::EditionGate::editionChanged, this, &Qat::sync);
    sync();
}

ui::Button* Qat::button(const QString& command) const
{
    for (const Item& i : items_)
        if (i.action->objectName() == command)
            return i.button;
    return nullptr;
}

void Qat::sync()
{
    setAccessibleName(tr("Thanh truy cập nhanh"));
    heading_->setText(tr("Thêm vào thanh truy cập nhanh"));
    const QString customize = tr("Tùy biến thanh truy cập nhanh");
    more_->setAccessibleName(customize);
    ui::RichTooltip::attach(more_, {.name = customize});
    if (printMenu_)
        printMenu_->setAccessibleName(tr("Lệnh in"));
    for (const Item& i : items_) {
        ui::TipInfo tip = ui::RichTooltip::info(i.action); // lệnh, phím, khóa do ActionRegistry, EditionGate đặt
        tip.name = i.action->text();
        i.button->setAccessibleName(tip.name);
        i.button->setLocked(tip.need != ui::Edition::None);
        ui::RichTooltip::attach(i.button, tip);
        i.toggle->setText(tr(i.label));
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
