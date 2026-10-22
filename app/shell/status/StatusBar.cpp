#include "app/shell/status/StatusBar.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/i18n/UiFormat.h"
#include "app/shell/status/ModeToggles.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Paint.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/progress/InlineProgress.h"
#include "app/widgets/progress/ProgressPresenter.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/regen/numfmt/NumFormat.h"

#include <QAction>
#include <QBoxLayout>
#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QTimer>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

using namespace Qt::StringLiterals;

namespace trion::shell {

namespace {

using edition::Edition;
using theme::TextRole;
using theme::Weight;
using Clock = std::chrono::steady_clock;

// Tùy biến: khóa các mục người dùng tắt (mục mới mặc định hiện).
const settings::Setting<QStringList> kHidden{"ui/statusbar/hidden", {}};

// Nhãn menu Tùy biến theo khóa (nút chế độ dùng tên của nút). Thứ tự = thứ tự trên thanh.
constexpr std::pair<const char*, const char*> kLabels[] = {
    {"coord", QT_TRANSLATE_NOOP("trion::shell::StatusBar", "Tọa độ")},
    {"space", QT_TRANSLATE_NOOP("trion::shell::StatusBar", "Model / Paper")},
    {"tasks", QT_TRANSLATE_NOOP("trion::shell::StatusBar", "Tác vụ nền")},
    {"fontDoctor", QT_TRANSLATE_NOOP("trion::shell::StatusBar", "Chỉ báo Bác sĩ Font")},
    {"graphics", QT_TRANSLATE_NOOP("trion::shell::StatusBar", "Chỉ báo đồ họa")},
    {"notifications", QT_TRANSLATE_NOOP("trion::shell::StatusBar", "Thông báo")},
    {"edition", QT_TRANSLATE_NOOP("trion::shell::StatusBar", "Huy hiệu phiên bản")},
    {"cleanScreen", QT_TRANSLATE_NOOP("trion::shell::StatusBar", "Clean Screen")},
};

// Thiếu chỗ: mục rank thấp vào menu tràn trước. Tọa độ, MODEL, OSNAP, tác vụ đang chạy giữ lâu nhất; nút chế độ chưa
// có trong bảng (ModeToggles thêm sau) ở giữa.
constexpr std::pair<const char*, int> kRanks[] = {
    {"coord", 100},      {"space", 90},     {"osnap", 85},         {"taskProgress", 80},
    {"ortho", 75},       {"polar", 70},     {"tasks", 65},         {"notifications", 60},
    {"otrack", 55},      {"fontDoctor", 50}, {"dyn", 45},           {"snap", 40},
    {"grid", 35},        {"edition", 30},   {"LWDISPLAY", 25},     {"TRANSPARENCYDISPLAY", 20},
    {"cleanScreen", 15}, {"graphics", 10},
};

int rankOf(const QString& id)
{
    for (const auto& [name, rank] : kRanks)
        if (id == QLatin1StringView(name))
            return rank;
    return 30;
}

const theme::ThemeManager& tm()
{
    return theme::ThemeManager::instance();
}

int px(tok::PxId id)
{
    return qRound(tm().px(id));
}

// Thời gian một khung theo tần số màn hình chứa w (60/120/144 Hz); chưa biết: 60 Hz.
std::chrono::nanoseconds frameTime(const QWidget* w)
{
    const QScreen* s = w->screen();
    return std::chrono::nanoseconds(qint64(1e9 / (s && s->refreshRate() > 1 ? s->refreshRate() : 60)));
}

} // namespace

struct StatusBar::Item {
    QString id;        // objectName của widget, mã ở overflowed()
    QString key;       // khóa Tùy biến; "tasks" chung cho tác vụ chính và nút Tác vụ nền
    int rank;          // giữ lại khi thiếu chỗ: lớn hơn giữ lâu hơn
    bool right;        // nhóm phải (sau khoảng giãn)
    QWidget* widget;   // nút chế độ: nút trong ModeToggles
    QAction* spill;    // mục ở menu tràn
    bool toggle;       // nút trong ModeToggles
    bool inProfile = true; // nút chế độ có trong hồ sơ (ModeToggles::setProfile quyết)
    bool wanted = false;   // relayout: muốn hiện (Tùy biến, hồ sơ, có nội dung)
    bool shown = false;    // relayout: đang hiện trên thanh
    int width = 0;         // relayout: sizeHint, đo một lần mỗi lượt
};

// Nút chữ: .sb-txt (MODEL/PAPER), .sb-chip.warn (Bác sĩ Font), .ed (huy hiệu phiên bản), .sb-coord (CoordButton). Cao
// --h-btn; rê --c-hover, nhấn --c-press (chip: viền trong --c-border-ctl; huy hiệu: như prototype, không đổi nền); focus
// bàn phím: viền --c-focus. Enter như Space; menu (con của nút): chuột phải, phím Menu, Shift+F10.
class StatusBar::TextButton : public QToolButton {
public:
    enum class Look : std::uint8_t { Coord, Space, Chip, Edition };

    TextButton(Look look, QWidget* parent) : QToolButton(parent), look_(look)
    {
        setFocusPolicy(Qt::TabFocus); // như .sbt: bấm chuột không lấy focus khỏi vùng vẽ, dòng lệnh
        setAttribute(Qt::WA_Hover);
    }

    ui::Menu* menu = nullptr;
    tok::ColorId bg = tok::c_ed_pro, fg = tok::c_ed_pro_t; // .ed theo phiên bản

    virtual QString label() const { return text(); }

    QSize sizeHint() const override { return {widthFor(label()), px(tok::h_btn)}; }
    QSize minimumSizeHint() const override { return sizeHint(); }

    int widthFor(const QString& s) const
    {
        int w = QFontMetrics(textFont()).horizontalAdvance(s) + 2 * pad();
        if (look_ == Look::Chip)
            w += px(tok::ic) + px(tok::sp_2); // biểu tượng warn, gap
        if (look_ == Look::Edition)
            w += 2 * px(tok::sp_2); // .sb .ed margin 0 4px
        return w;
    }

protected:
    QFont textFont() const
    {
        switch (look_) {
        case Look::Coord:
            return theme::font(TextRole::MonoCaption);
        case Look::Chip:
            return theme::font(TextRole::Label);
        case Look::Space:
        case Look::Edition:
            break;
        }
        return theme::font(TextRole::Caption, Weight::Bold);
    }

    int pad() const { return px(look_ == Look::Coord || look_ == Look::Chip ? tok::sp_4 : tok::sp_3); }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal r = tm().px(tok::r);
        QRectF face = rect();
        const bool hot = isEnabled() && (underMouse() || isDown());
        tok::ColorId ink = tok::c_text_2;
        p.setPen(Qt::NoPen);
        switch (look_) {
        case Look::Chip:
            p.setBrush(tm().color(tok::c_warn_soft));
            p.drawRoundedRect(face, r, r);
            if (hot) // .sb-chip:hover: viền trong 1 px
                ui::paint::drawBox(p, face.adjusted(0.5, 0.5, -0.5, -0.5), QColor(), tm().color(tok::c_border_ctl));
            ink = tok::c_warn;
            break;
        case Look::Edition: {
            const qreal h = QFontMetrics(textFont()).height() + 2 * tm().px(tok::sp_1);
            face = QRectF(px(tok::sp_2), (height() - h) / 2, width() - 2 * px(tok::sp_2), h);
            p.setBrush(tm().color(bg));
            p.drawRoundedRect(face, r, r);
            ink = fg;
            break;
        }
        case Look::Coord:
        case Look::Space:
            if (hot) {
                p.setBrush(tm().color(isDown() ? tok::c_press : tok::c_hover));
                p.drawRoundedRect(face, r, r);
            }
            ink = look_ == Look::Coord ? tok::c_text : tok::c_text_2;
            break;
        }
        QRectF text = face.adjusted(pad(), 0, -pad(), 0);
        if (look_ == Look::Chip) {
            const int ic = px(tok::ic);
            const QPixmap icon = theme::IconProvider::instance().pixmap(u"warn"_s, ic, devicePixelRatioF(),
                                                                        theme::IconState::Normal);
            p.drawPixmap(QPointF(text.left(), (height() - ic) / 2.0), ui::paint::tinted(icon, tm().color(ink)));
            text.setLeft(text.left() + ic + px(tok::sp_2));
        }
        p.setFont(textFont());
        p.setPen(tm().color(ink));
        p.drawText(text, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, label());
        if (ui::paint::focusVisible(this))
            ui::paint::drawRing(p, rect(), tm().color(tok::c_focus));
    }

    void keyPressEvent(QKeyEvent* e) override
    {
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
            click();
        else if (menu && (e->key() == Qt::Key_Menu ||
                          e->keyCombination() == QKeyCombination(Qt::ShiftModifier, Qt::Key_F10)))
            StatusButton::popupAbove(menu, this);
        else
            QToolButton::keyPressEvent(e);
    }

    void contextMenuEvent(QContextMenuEvent* e) override
    {
        if (menu)
            menu->popup(e->globalPos() - QPoint(0, menu->sizeHint().height()));
    }

private:
    Look look_;
};

// Ô tọa độ .sb-coord: Mono --fs-xs, --c-text, lề --sp-4. Điểm mới chỉ hẹn một lần làm mới (timer một lần, PreciseTimer):
// ngay nếu đã qua một khung từ lần trước, không thì ở mốc khung kế; mọi điểm tới trước mốc gộp vào lần đó (đọc điểm
// mới nhất). Chữ không đổi thì không vẽ. Bề rộng chỉ nới: giữ chỗ cho mẫu 10000, 10000 theo đơn vị hiện tại.
class StatusBar::CoordButton final : public TextButton {
public:
    explicit CoordButton(QWidget* parent) : TextButton(Look::Coord, parent)
    {
        timer_.setSingleShot(true);
        timer_.setTimerType(Qt::PreciseTimer);
        QObject::connect(&timer_, &QTimer::timeout, this, [this] { refresh(); });
    }

    QPointF point;
    std::optional<QPointF> base;
    int coords = 0;
    regen::UnitContext units;

    QString label() const override { return text_; }
    QSize sizeHint() const override
    {
        return {std::max(reserved_, widthFor(coordText({1e4, 1e4}, {}, 1, units))), px(tok::h_btn)};
    }

    void schedule()
    {
        if (timer_.isActive())
            return; // lần làm mới đã hẹn đọc điểm mới nhất
        const auto wait = std::max(Clock::duration::zero(), last_ + frameTime(this) - Clock::now());
        timer_.start(std::chrono::ceil<std::chrono::milliseconds>(wait));
    }

    void refresh()
    {
        timer_.stop();
        last_ = Clock::now();
        QString s = coordText(point, base, coords, units);
        if (s == text_)
            return;
        text_ = std::move(s);
        if (const int w = widthFor(text_); w > reserved_) {
            reserved_ = w;
            updateGeometry();
        }
        update();
    }

    void resetWidth() // đơn vị đổi: chữ mẫu đổi bề rộng
    {
        reserved_ = 0;
        updateGeometry();
        schedule();
    }

private:
    QTimer timer_;
    Clock::time_point last_{};
    QString text_;
    int reserved_ = 0;
};

// Danh sách Tác vụ nền (prototype SC.tasks; 07.1 §10): popup trên nút, mép phải thẳng nút, nền --c-raised viền
// --c-border-ctl như .menu. Mỗi việc đang hiện tiến trình (hàng của ProgressItemModel) một InlineProgress (thanh %,
// nhãn, nút Hủy); việc dừng thì hàng của nó bỏ ngay trong lần áp của presenter. Rỗng: câu giải thích. Esc đóng.
class StatusBar::TaskList final : public QWidget {
public:
    TaskList(ui::ProgressPresenter& progress, QWidget* parent)
        : QWidget(parent, Qt::Popup), progress_(progress), title_(new QLabel(this)), empty_(new QLabel(this)),
          hint_(new QLabel(this)), list_(new QVBoxLayout)
    {
        auto* box = new QVBoxLayout(this);
        box->addWidget(title_);
        box->addLayout(list_);
        box->addWidget(empty_);
        box->addWidget(hint_);
        hint_->setWordWrap(true);
        hint_->setForegroundRole(QPalette::PlaceholderText);
        title_->setTextFormat(Qt::PlainText);
        ui::ProgressItemModel* m = progress.model();
        for (auto signal : {&QAbstractItemModel::rowsInserted, &QAbstractItemModel::rowsRemoved})
            QObject::connect(m, signal, this, [this] { rebuild(); });
        QObject::connect(m, &QAbstractItemModel::modelReset, this, [this] { rebuild(); });
        retranslate();
        rebuild();
    }

    QWidget* anchor = nullptr;
    std::vector<ui::InlineProgress*> rows; // theo hàng của model

    void retranslate()
    {
        const QString name = StatusBar::tr("Tác vụ nền");
        setAccessibleName(name);
        title_->setText(name);
        empty_->setText(StatusBar::tr("Không có tác vụ đang chạy"));
        hint_->setText(StatusBar::tr("Tự lưu, in hàng loạt, chuyển font, nạp tệp hiện ở đây kèm tiến trình và nút hủy."));
        place();
    }

    void place()
    {
        const int m = px(tok::sp_4);
        layout()->setContentsMargins(m, m, m, m);
        layout()->setSpacing(px(tok::sp_3));
        title_->setFont(theme::font(TextRole::Ui, Weight::Bold));
        const int w = std::max(px(tok::pal_w), layout()->sizeHint().width());
        hint_->setFixedWidth(w - 2 * m);
        resize(w, layout()->sizeHint().height());
        if (anchor)
            move(anchor->mapToGlobal(QPoint(anchor->width(), 0)) - QPoint(width(), height()));
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), tm().color(tok::c_raised));
        p.setPen(tm().color(tok::c_border_ctl));
        p.drawRect(rect().adjusted(0, 0, -1, -1));
    }

    void keyPressEvent(QKeyEvent* e) override
    {
        if (e->key() == Qt::Key_Escape)
            close();
        else
            QWidget::keyPressEvent(e);
    }

private:
    void rebuild()
    {
        for (ui::InlineProgress* r : rows)
            delete r; // bỏ ngay, trong lần áp này: khung kế không còn hàng
        rows.clear();
        const ui::ProgressItemModel* m = progress_.model();
        for (int i = 0; i < m->rowCount(); ++i) {
            const auto id = m->index(i).data(ui::ProgressItemModel::JobIdRole).value<jobs::JobId>();
            rows.push_back(new ui::InlineProgress(progress_, id, this));
            list_->addWidget(rows.back());
        }
        empty_->setVisible(rows.empty());
        hint_->setVisible(rows.empty());
        if (isVisible())
            place();
    }

    ui::ProgressPresenter& progress_;
    QLabel* title_;
    QLabel* empty_;
    QLabel* hint_;
    QVBoxLayout* list_;
};

StatusBar::StatusBar(actions::ActionRegistry& actions, actions::Sysvars& sysvars, edition::EditionGate& gate,
                     ui::ProgressPresenter& progress, ui::NotificationCenter& notes, settings::Store& store,
                     QWidget* parent)
    : QWidget(parent), actions_(actions), sysvars_(sysvars), gate_(gate), progress_(progress), notes_(notes),
      store_(store), hidden_(store.get(kHidden)), coord_(new CoordButton(this)),
      spaceButton_(new TextButton(TextButton::Look::Space, this)), modes_(new ModeToggles(actions, sysvars, this)),
      taskBox_(new QWidget(this)), task_(new ui::InlineProgress(progress, 0, taskBox_)),
      font_(new TextButton(TextButton::Look::Chip, this)), gpu_(new StatusButton(this)), tasks_(new StatusButton(this)),
      bell_(new StatusButton(this)), edition_(new TextButton(TextButton::Look::Edition, this)),
      clean_(new StatusButton(this)), overflow_(new StatusButton(this)), custom_(new StatusButton(this)),
      coordMenu_(new ui::Menu(coord_)), overflowMenu_(new ui::Menu(overflow_)), customMenu_(new ui::Menu(custom_)),
      spaceAction_(new QAction(this)), tasksAction_(new QAction(this)), notesAction_(new QAction(this)),
      licenseAction_(new QAction(this))
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* box = new QHBoxLayout(taskBox_);
    box->setContentsMargins({});
    box->addWidget(task_);

    // Lệnh qua QAction của registry (cổng phiên bản, sink); nút biểu tượng lấy biểu tượng của lệnh.
    const auto command = [&](StatusButton* b, const char* id) {
        if (QAction* a = actions.action(QString::fromLatin1(id)))
            b->setDefaultAction(a);
    };
    command(gpu_, "GRAPHICSCONFIG");
    command(clean_, "CLEANSCREENON");
    tasks_->setIcon(theme::IconProvider::icon(u"tasks"_s));
    bell_->setIcon(theme::IconProvider::icon(u"bell"_s));
    overflow_->setIcon(theme::IconProvider::icon(u"more"_s));
    custom_->setIcon(theme::IconProvider::icon(u"menu"_s));
    gpu_->installEventFilter(this); // rê: thông tin đồ họa mới (eventFilter)

    connect(coord_, &QToolButton::clicked, this, [this] {
        if (QAction* a = actions_.action(u"coord"_s))
            a->trigger(); // như Ctrl+I: 'COORDS n qua sink
    });
    connect(spaceButton_, &QToolButton::clicked, this, &StatusBar::toggleSpace);
    connect(spaceAction_, &QAction::triggered, this, &StatusBar::toggleSpace);
    connect(font_, &QToolButton::clicked, this, [this] {
        if (QAction* a = actions_.action(u"FIXFONT"_s))
            a->trigger();
    });
    connect(tasks_, &QToolButton::clicked, this, [this] { openTasks(tasks_); });
    connect(tasksAction_, &QAction::triggered, this, [this] { openTasks(overflow_); });
    connect(bell_, &QToolButton::clicked, this, [this] { emit screenRequested(u"notifications"_s); });
    connect(notesAction_, &QAction::triggered, this, [this] { emit screenRequested(u"notifications"_s); });
    connect(edition_, &QToolButton::clicked, this, [this] { emit screenRequested(u"license"_s); });
    connect(licenseAction_, &QAction::triggered, this, [this] { emit screenRequested(u"license"_s); });
    connect(overflow_, &QToolButton::clicked, this, [this] { StatusButton::popupAbove(overflowMenu_, overflow_, true); });
    connect(custom_, &QToolButton::clicked, this, [this] { StatusButton::popupAbove(customMenu_, custom_, true); });
    connect(modes_, &ModeToggles::macroRequested, this, &StatusBar::macroRequested);

    // Mục theo thứ tự trên thanh: tọa độ, MODEL, nút chế độ | tác vụ chính, Bác sĩ Font, đồ họa, Tác vụ nền, chuông,
    // huy hiệu, Clean Screen. Mục tràn, Tùy biến luôn ở cuối.
    const auto add = [this](const QString& id, const QString& key, bool right, QWidget* w, QAction* spill) {
        w->setObjectName(id);
        items_.push_back({id, key, rankOf(id), right, w, spill, false});
    };
    add(u"coord"_s, u"coord"_s, false, coord_, actions.action(u"coord"_s));
    add(u"space"_s, u"space"_s, false, spaceButton_, spaceAction_);
    for (QToolButton* b : modes_->findChildren<QToolButton*>(Qt::FindDirectChildrenOnly)) {
        const QString id = b->defaultAction()->objectName(); // id hành động của registry ("ortho")
        items_.push_back({id, id, rankOf(id), false, b, b->defaultAction(), true});
    }
    add(u"taskProgress"_s, u"tasks"_s, true, taskBox_, tasksAction_);
    add(u"fontDoctor"_s, u"fontDoctor"_s, true, font_, actions.action(u"FIXFONT"_s));
    add(u"graphics"_s, u"graphics"_s, true, gpu_, gpu_->defaultAction());
    add(u"tasks"_s, u"tasks"_s, true, tasks_, tasksAction_);
    add(u"notifications"_s, u"notifications"_s, true, bell_, notesAction_);
    add(u"edition"_s, u"edition"_s, true, edition_, licenseAction_);
    add(u"cleanScreen"_s, u"cleanScreen"_s, true, clean_, clean_->defaultAction());
    overflow_->setObjectName(u"overflow"_s);
    custom_->setObjectName(u"customize"_s);

    // Tọa độ: kiểu theo COORDS; menu chuột phải chọn kiểu.
    coord_->menu = coordMenu_;
    coordMenu_->addSection(QString());
    for (const int v : {1, 2, 0}) { // tuyệt đối, tương đối, tắt
        QAction* a = coordMenu_->addAction(QString());
        a->setCheckable(true);
        a->setData(v);
        connect(a, &QAction::triggered, this, [this, v] { setCoords(v); });
    }
    coord_->coords = sysvars.get(u"COORDS"_s);
    connect(&sysvars_, &actions::Sysvars::changed, this, [this](const QString& name, int value) {
        if (name != u"COORDS"_s)
            return;
        coord_->coords = value;
        coord_->schedule();
        syncCoordMenu();
    });

    // Tùy biến: một mục cho mỗi khóa, theo thứ tự trên thanh; bật/tắt nhiều mục liền: menu mở tiếp như menu OSNAP
    // (ModeToggles::eventFilter giữ menu mở khi kích hoạt mục có ô chọn).
    customMenu_->addSection(QString());
    QStringList keys;
    for (const Item& it : items_)
        if (!keys.contains(it.key)) {
            keys << it.key;
            QAction* a = customMenu_->addAction(QString());
            a->setCheckable(true);
            a->setData(it.key);
            connect(a, &QAction::triggered, this, [this, key = it.key](bool on) {
                hidden_.removeAll(key);
                if (!on)
                    hidden_ << key;
                store_.set(kHidden, hidden_);
                relayout();
            });
        }
    customMenu_->installEventFilter(modes_);
    connect(customMenu_, &QMenu::aboutToShow, this, [this] {
        for (QAction* a : customMenu_->actions()) {
            const QString key = a->data().toString();
            if (key.isEmpty())
                continue;
            a->setChecked(!hidden_.contains(key));
            for (const Item& it : items_)
                if (it.key == key && it.toggle) { // tên nút chế độ theo hồ sơ ("Bắt điểm (khi đo)")
                    a->setText(it.widget->accessibleName());
                    a->setVisible(it.inProfile);
                }
        }
    });
    connect(overflowMenu_, &QMenu::aboutToShow, this, [this] {
        overflowMenu_->clear();
        for (const Item& it : items_)
            if (it.wanted && !it.shown && it.spill && !overflowMenu_->actions().contains(it.spill))
                overflowMenu_->addAction(it.spill);
    });

    // Tác vụ nền, thông báo, phiên bản, hồ sơ: theo nguồn, đồng bộ.
    connect(&progress_, &ui::ProgressPresenter::changed, this, &StatusBar::relayout);
    connect(&notes_, &ui::NotificationCenter::unreadChanged, this, &StatusBar::retranslate);
    connect(&gate_, &edition::EditionGate::editionChanged, this, &StatusBar::syncEdition);
    connect(&gate_, &edition::EditionGate::profileChanged, this, &StatusBar::applyProfile);
    connect(&theme::ThemeManager::instance(), &theme::ThemeManager::changed, this, [this] {
        for (StatusButton* b : {gpu_, tasks_, bell_, clean_, overflow_, custom_})
            b->setIconSize(QSize(px(tok::ic), px(tok::ic)));
        for (const Item& it : items_)
            it.widget->updateGeometry();
        coord_->resetWidth();
        update();
    });
    for (StatusButton* b : {gpu_, tasks_, bell_, clean_, overflow_, custom_})
        b->setIconSize(QSize(px(tok::ic), px(tok::ic)));

    setSpace(Space::Model);
    applyProfile();
    retranslate();
    coord_->refresh();
}

StatusBar::~StatusBar() = default;

QString StatusBar::coordText(QPointF point, std::optional<QPointF> base, int coords, const regen::UnitContext& units)
{
    if (coords == 0)
        return tr("Tọa độ tắt");
    std::array<char, regen::kMaxNumText> buf;
    const auto text = [&buf](std::size_t n) { return QString::fromUtf8(buf.data(), qsizetype(std::min(n, buf.size()))); };
    const auto linear = [&](double v) { return text(regen::formatLinear(v, units, buf)); };
    if (coords == 2 && base) { // tương đối cực: khoảng cách<góc theo ANGBASE/ANGDIR
        const QPointF d = point - *base;
        regen::UnitContext angular = units;
        angular.useAngbase = true;
        const QString angle = text(regen::formatAngle(std::atan2(d.y(), d.x()), angular, buf));
        return u"%1<%2, %3"_s.arg(linear(std::hypot(d.x(), d.y())), angle, linear(0));
    }
    return u"%1, %2, %3"_s.arg(linear(point.x()), linear(point.y()), linear(0));
}

void StatusBar::setCursorPoint(double x, double y)
{
    coord_->point = {x, y};
    coord_->schedule();
}

void StatusBar::setBasePoint(std::optional<QPointF> base)
{
    coord_->base = base;
    coord_->schedule();
}

void StatusBar::setUnits(const regen::UnitContext& units)
{
    coord_->units = units;
    coord_->resetWidth();
}

QString StatusBar::shownCoord() const
{
    return coord_->label();
}

void StatusBar::setSpace(Space space)
{
    space_ = space;
    spaceButton_->setText(space == Space::Paper ? u"PAPER"_s : u"MODEL"_s); // i18n-ok: tên không gian của AutoCAD
    spaceButton_->setEnabled(space != Space::Model);
    spaceAction_->setText(spaceButton_->text());
    spaceAction_->setEnabled(spaceButton_->isEnabled());
}

void StatusBar::toggleSpace()
{
    if (space_ != Space::Model) // lệnh thường: hủy lệnh đang chạy như nút MODEL/PAPER của AutoCAD
        emit macroRequested(space_ == Space::Paper ? u"^C^C_MSPACE"_s : u"^C^C_PSPACE"_s);
}

void StatusBar::setCoords(int value)
{
    emit macroRequested(u"'COORDS %1"_s.arg(value));
    syncCoordMenu(); // Qt đã đảo dấu chọn khi bấm; dấu chọn chỉ theo Sysvars
}

void StatusBar::syncCoordMenu()
{
    const int v = sysvars_.get(u"COORDS"_s);
    for (QAction* a : coordMenu_->actions())
        if (a->isCheckable())
            a->setChecked(a->data().toInt() == v);
}

void StatusBar::setGraphicsSource(std::function<GraphicsInfo()> source)
{
    graphics_ = std::move(source);
    syncGraphicsTip();
}

void StatusBar::syncGraphicsTip()
{
    const GraphicsInfo g = graphics_ ? graphics_() : GraphicsInfo{};
    QStringList device;
    for (const QString& s : {g.backend, g.gpu})
        if (!s.isEmpty())
            device << s;
    const QString name = device.isEmpty() ? tr("Chỉ báo đồ họa") : tr("Đồ họa: %1").arg(device.join(u" · "_s));
    const QString about = g.fps > 0 ? tr("Khung hình hiện tại %1 FPS. Bấm để mở Thiết lập đồ họa.")
                                          .arg(i18n::formatCount(qRound(g.fps)))
                                    : tr("Bấm để mở Thiết lập đồ họa.");
    ui::RichTooltip::attach(gpu_, {.name = name, .command = u"GRAPHICSCONFIG"_s, .description = about});
}

void StatusBar::setFontIssues(int substitutedFonts, int legacyObjects)
{
    fontSubstituted_ = std::max(0, substitutedFonts);
    fontLegacy_ = std::max(0, legacyObjects);
    retranslate();
}

void StatusBar::setTrialDays(int days)
{
    trialDays_ = days;
    syncEdition();
}

void StatusBar::setOfflineDays(int days)
{
    offlineDays_ = days;
    syncEdition();
}

void StatusBar::syncEdition()
{
    // Như huy hiệu của thanh tiêu đề (edBadge của prototype); chữ dùng chung bản dịch của TitleBar.
    // ponytail: hai nơi tính huy hiệu; gộp thành một widget ở app/core/edition khi có PLF-002 (đề nghị UIA-009).
    QString badge;
    edition_->bg = tok::c_ed_pro;
    edition_->fg = tok::c_ed_pro_t;
    switch (gate_.edition()) {
    case Edition::Viewer:
        badge = QCoreApplication::translate("trion::shell::TitleBar", "Viewer");
        edition_->bg = tok::c_ed_viewer;
        edition_->fg = tok::c_ed_viewer_t;
        break;
    case Edition::ViewerPro:
        badge = QCoreApplication::translate("trion::shell::TitleBar", "Viewer Pro");
        edition_->bg = tok::c_ed_vp;
        edition_->fg = tok::c_ed_vp_t;
        break;
    case Edition::Pro:
        badge = QCoreApplication::translate("trion::shell::TitleBar", "Pro");
        break;
    case Edition::Trial:
        badge = QCoreApplication::translate("trion::shell::TitleBar", "Dùng thử: còn %n ngày", nullptr, trialDays_);
        edition_->bg = tok::c_ed_trial;
        edition_->fg = tok::c_ed_trial_t;
        break;
    case Edition::Education:
        badge = QCoreApplication::translate("trion::shell::TitleBar", "Giáo dục");
        edition_->bg = tok::c_ed_edu;
        edition_->fg = tok::c_ed_edu_t;
        break;
    case Edition::Enterprise:
        badge = QCoreApplication::translate("trion::shell::TitleBar", "Doanh nghiệp");
        edition_->bg = tok::c_ed_ent;
        edition_->fg = tok::c_ed_ent_t;
        break;
    }
    if (offlineDays_ >= 0 && gate_.edition() != Edition::Viewer) { // .ed.warn: lâu chưa xác minh giấy phép
        badge = QCoreApplication::translate("trion::shell::TitleBar", "%1 · còn %n ngày offline", nullptr, offlineDays_).arg(badge);
        edition_->bg = tok::c_warn_soft;
        edition_->fg = tok::c_warn;
    }
    edition_->setText(badge);
    edition_->update();
}

void StatusBar::applyProfile()
{
    modes_->setProfile(gate_.profile());
    for (Item& it : items_)
        if (it.toggle)
            it.inProfile = !it.widget->isHidden(); // ModeToggles vừa đặt theo hồ sơ
    syncEdition();
    relayout();
}

void StatusBar::openTasks(QWidget* anchor)
{
    if (!taskList_)
        taskList_ = new TaskList(progress_, this);
    taskList_->anchor = anchor;
    taskList_->place();
    taskList_->show();
    if (!taskList_->rows.empty()) // bàn phím: Tab giữa các nút Hủy, Esc đóng
        taskList_->rows.front()->findChild<QPushButton*>()->setFocus(Qt::PopupFocusReason);
}

bool StatusBar::wanted(const Item& it) const
{
    if (hidden_.contains(it.key))
        return false;
    if (it.toggle)
        return it.inProfile;
    if (it.widget == taskBox_)
        return task_->current() != 0;
    if (it.widget == font_)
        return fontSubstituted_ + fontLegacy_ > 0;
    return true;
}

int StatusBar::required(bool all) const
{
    const int gap = px(tok::sp_1);
    int w = 2 * px(tok::sp_2), n = 0, toggles = 0, strip = 0;
    const auto add = [&](int x) {
        w += x;
        ++n;
    };
    bool spilled = false;
    for (const Item& it : items_) {
        if (!(all ? it.wanted : it.shown)) {
            spilled |= it.wanted;
            continue;
        }
        if (it.toggle) {
            strip += it.width;
            ++toggles;
        } else {
            add(it.width);
        }
    }
    if (toggles) { // | dải nút | : .sb-sep rộng 1 px, lề --sp-2 mỗi bên
        add(1 + 2 * px(tok::sp_2));
        add(strip + (toggles - 1) * modes_->layout()->spacing());
        add(1 + 2 * px(tok::sp_2));
    }
    if (spilled)
        add(overflow_->sizeHint().width());
    add(custom_->sizeHint().width());
    return w + gap * (n - 1);
}

void StatusBar::relayout()
{
    for (Item& it : items_) {
        it.shown = it.wanted = wanted(it);
        it.width = it.widget->sizeHint().width();
    }
    std::vector<Item*> byRank;
    for (Item& it : items_)
        byRank.push_back(&it);
    std::ranges::stable_sort(byRank, {}, &Item::rank);
    for (Item* it : byRank) { // thiếu chỗ: mục ưu tiên thấp vào menu tràn trước, không nén mục nào dưới cỡ của nó
        if (required() <= width())
            break;
        it->shown = false;
    }

    const int gap = px(tok::sp_1), top = 1; // viền trên 1 px
    const auto place = [&](QWidget* w, int x, int width) {
        const int h = w->sizeHint().height();
        w->setGeometry(x, top + (height() - top - h) / 2, width, h);
    };
    int x = px(tok::sp_2), toggles = 0, strip = 0;
    bool spilled = false;
    for (Item& it : items_) {
        spilled |= it.wanted && !it.shown;
        it.widget->setVisible(it.shown);
        if (it.toggle && it.shown) {
            strip += it.width;
            ++toggles;
        } else if (!it.right && it.shown) {
            place(it.widget, x, it.width);
            x += it.width + gap;
        }
    }
    std::array<int, 2> seps{-1, -1};
    modes_->setVisible(toggles > 0);
    if (toggles) {
        seps[0] = x + px(tok::sp_2);
        x += 1 + 2 * px(tok::sp_2) + gap;
        const int w = strip + (toggles - 1) * modes_->layout()->spacing();
        place(modes_, x, w);
        x += w + gap;
        seps[1] = x + px(tok::sp_2);
    }
    int r = width() - px(tok::sp_2);
    const auto right = [&](QWidget* w, int width) {
        r -= width;
        place(w, r, width);
        r -= gap;
    };
    right(custom_, custom_->sizeHint().width());
    overflow_->setVisible(spilled);
    if (spilled)
        right(overflow_, overflow_->sizeHint().width());
    for (auto it = items_.rbegin(); it != items_.rend(); ++it)
        if (it->right && it->shown)
            right(it->widget, it->width);
    if (seps != seps_) {
        seps_ = seps;
        update();
    }
}

QStringList StatusBar::overflowed() const
{
    QStringList out;
    for (const Item& it : items_)
        if (it.wanted && !it.shown)
            out << it.id;
    return out;
}

QSize StatusBar::sizeHint() const
{
    return {required(true), px(tok::h_status)}; // mọi mục muốn hiện, không tràn (bề rộng đo ở lần relayout gần nhất)
}

QSize StatusBar::minimumSizeHint() const
{
    return {custom_->sizeHint().width() + overflow_->sizeHint().width() + 2 * px(tok::sp_2), px(tok::h_status)};
}

bool StatusBar::event(QEvent* e)
{
    if (e->type() == QEvent::LayoutRequest) { // mục đổi cỡ, hiện/ẩn
        relayout();
        return true;
    }
    return QWidget::event(e);
}

void StatusBar::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    relayout();
}

void StatusBar::paintEvent(QPaintEvent*)
{
    QPainter p(this); // .sb: nền --c-chrome, viền trên --c-border; .sb-sep 1 px cao --sp-6 - --sp-1 (14 px)
    p.fillRect(rect(), tm().color(tok::c_chrome));
    p.fillRect(QRect(0, 0, width(), 1), tm().color(tok::c_border));
    const int h = px(tok::sp_6) - px(tok::sp_1);
    for (const int x : seps_)
        if (x >= 0)
            p.fillRect(QRect(x, 1 + (height() - 1 - h) / 2, 1, h), tm().color(tok::c_border));
}

void StatusBar::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

bool StatusBar::eventFilter(QObject* watched, QEvent* e)
{
    if (watched == gpu_ && e->type() == QEvent::Enter)
        syncGraphicsTip(); // RichTooltip đọc thông tin lúc hiện (sau 0,45 s): backend, GPU, FPS lúc rê
    return QWidget::eventFilter(watched, e);
}

void StatusBar::retranslate()
{
    const auto tip = [](QWidget* w, const QString& name, const ui::TipInfo& info) {
        w->setAccessibleName(name);
        w->setAccessibleDescription(info.description);
        ui::TipInfo i = info;
        i.name = name;
        ui::RichTooltip::attach(w, i);
    };
    tip(coord_, tr("Tọa độ con trỏ"),
        {.command = u"COORDS"_s, .keyId = u"coord"_s,
         .description = tr("Bấm để đổi kiểu hiển thị: tuyệt đối, tương đối, tắt. Chuột phải để chọn kiểu.")});
    tip(spaceButton_, tr("Model / Paper"),
        {.description = tr("Trong layout: chuyển giữa Paper và Model trong viewport.")});
    spaceButton_->setAccessibleName({}); // trình đọc màn hình đọc chữ MODEL/PAPER
    const auto coordActions = coordMenu_->actions();
    coordActions[0]->setText(tr("Kiểu hiển thị"));
    coordActions[1]->setText(tr("Tuyệt đối"));
    coordActions[2]->setText(tr("Tương đối"));
    coordActions[3]->setText(tr("Tắt"));
    syncCoordMenu();
    coord_->schedule(); // "Tọa độ tắt"

    // Bác sĩ Font: "Font: 3 thay thế"; rê: "3 font đã thay thế · 250 đối tượng TCVN3".
    QStringList found;
    if (fontSubstituted_)
        found << tr("%1 font đã thay thế", nullptr, fontSubstituted_).arg(i18n::formatCount(fontSubstituted_));
    if (fontLegacy_)
        found << tr("%1 đối tượng TCVN3", nullptr, fontLegacy_).arg(i18n::formatCount(fontLegacy_));
    font_->setText(fontSubstituted_ ? tr("Font: %1 thay thế", nullptr, fontSubstituted_)
                                          .arg(i18n::formatCount(fontSubstituted_))
                                    : tr("Font: %1 TCVN3", nullptr, fontLegacy_).arg(i18n::formatCount(fontLegacy_)));
    ui::RichTooltip::attach(font_, {.name = tr("Bác sĩ Font"), .command = u"FIXFONT"_s,
                                    .description = tr("%1. Bấm để mở Bác sĩ Font.").arg(found.join(u" · "_s))});
    font_->setAccessibleDescription(found.join(u" · "_s));

    syncGraphicsTip();
    gpu_->setAccessibleName(tr("Chỉ báo đồ họa"));
    tip(tasks_, tr("Tác vụ nền"),
        {.description = tr("Tự lưu, in hàng loạt, chuyển font, nạp tệp; có tiến trình và nút hủy.")});
    tasksAction_->setText(tr("Tác vụ nền"));
    const int unread = notes_.unreadCount();
    bell_->dot = unread > 0; // chấm, kèm số chưa đọc ở mô tả: không chỉ là màu
    bell_->update();
    tip(bell_, tr("Thông báo"),
        {.description = unread ? tr("%1 thông báo chưa đọc", nullptr, unread).arg(i18n::formatCount(unread)) : QString()});
    ui::RichTooltip::attach(bell_, {.name = tr("Trung tâm thông báo"), .description = bell_->accessibleDescription()});
    notesAction_->setText(tr("Thông báo"));
    ui::RichTooltip::attach(edition_, {.name = tr("Trạng thái giấy phép")});
    edition_->setAccessibleDescription(tr("Trạng thái giấy phép"));
    licenseAction_->setText(tr("Trạng thái giấy phép"));
    tip(clean_, tr("Clean Screen"),
        {.command = u"CLEANSCREENON"_s, .keyId = u"clean"_s,
         .description = tr("Thu gọn toàn bộ giao diện, chỉ còn vùng vẽ, dòng lệnh, thanh trạng thái.")});
    tip(overflow_, tr("Mục khác"), {.description = tr("Các mục không đủ chỗ trên thanh trạng thái.")});
    tip(custom_, tr("Tùy biến thanh trạng thái"), {.description = tr("Chọn mục hiện trên thanh trạng thái.")});
    customMenu_->actions().front()->setText(tr("Hiện trên thanh trạng thái"));
    for (QAction* a : customMenu_->actions())
        for (const auto& [key, label] : kLabels)
            if (a->data().toString() == QLatin1StringView(key))
                a->setText(tr(label));
    if (taskList_)
        taskList_->retranslate();
    syncEdition();
    relayout();
}

} // namespace trion::shell
