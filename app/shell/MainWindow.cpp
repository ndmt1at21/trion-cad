#include "app/shell/MainWindow.h"

#include "app/core/i18n/Translator.h"
#include "app/shell/Workspace.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Paint.h"
#include "app/widgets/dock/PaletteHost.h"
#include "app/widgets/message/Messages.h"
#include "core/base/assert.h"

#include <QAccessibleWidget>
#include <QApplication>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QStyle>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace trion::shell {

using Region = MainWindow::Region;

namespace {

const settings::Setting<QByteArray> kGeometry{"ui/window/geometry", {}};
const settings::Setting<bool> kCustomLayout{"ui/window/customLayout", false};
// Dòng lệnh nổi: {"x", "y", "width"} theo tọa độ vùng vẽ; rỗng: ở cột.
const settings::Setting<QJsonObject> kCommandLineFloat{"ui/window/commandLineFloat", {}};

// Số prototype viết thẳng (app.css), không có token: nét chia 1 px; .cmd-hist padding-top 3 px; .cmd-in margin-top
// 3 px (margin-bottom 6 px là --sp-3).
constexpr int kLine = 1;
constexpr int kHistoryPad = 3;
constexpr int kInputTop = 3;

// Cao của vùng theo token (mật độ gọn/thoáng), như #app của prototype; 0: vùng vẽ (giãn).
int rowHeight(Region r, bool compact)
{
    const auto& t = theme::ThemeManager::instance();
    const auto px = [&t](tok::PxId id) { return qRound(t.px(id)); };
    switch (r) {
    case Region::TitleBar: // .tb
        return px(tok::h_title);
    case Region::Ribbon: // .rb-tabs + .rb-body (3 hàng nút, 8 px, nhãn panel) + nét dưới
        return px(tok::h_tab) + 3 * px(tok::h_btn) + px(tok::sp_4) + px(tok::h_lbl) + kLine;
    case Region::FileTabs: // .ft
        return px(tok::h_ftab);
    case Region::LayoutTabs: // .lt
        return px(tok::h_ltab);
    case Region::CommandLine: // nét trên + .cmd-hist (3 dòng; hẹp: 1 dòng, #app.narrow) + .cmd-in và lề
        return kLine + (compact ? 1 : 3) * px(tok::h_cmdl) + kHistoryPad + kInputTop + px(tok::h_cmdin) + px(tok::sp_3);
    case Region::StatusBar: // .sb
        return px(tok::h_status);
    case Region::Canvas:
        break;
    }
    return 0;
}

// Ô giữ chỗ của vùng chưa có widget thật: cao đúng token, nền và nét theo prototype, tên cho trình đọc màn hình.
// Vùng vẽ: nền --c-canvas, nhận focus (viền --c-focus khi Tab tới), vai trò Canvas (QWidget thường là Client: trình
// đọc màn hình không biết là gì). ponytail: UIA-002 (thanh tiêu đề), UIA-005 (ribbon), UIA-010 (tab tệp), UIA-011 (tab
// layout), UIA-107 (dòng lệnh), UIA-009 (thanh trạng thái), UIA-109 (vùng vẽ) thay qua setRegion.
class Placeholder final : public QWidget {
public:
    explicit Placeholder(Region r) : region_(r)
    {
        setAttribute(Qt::WA_OpaquePaintEvent);
        setAccessibleName(MainWindow::regionName(r));
        if (r == Region::Canvas) {
            setFocusPolicy(Qt::StrongFocus);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            QAccessible::installFactory(&accessible); // Qt bỏ qua lần cài lại
        } else {
            setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        }
    }

    QSize sizeHint() const override
    {
        return region_ == Region::Canvas ? QWidget::sizeHint() : QSize(0, rowHeight(region_, compact()));
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    bool event(QEvent* e) override
    {
        if (e->type() == QEvent::DynamicPropertyChange) // "compact" của dòng lệnh
            updateGeometry();
        else if (e->type() == QEvent::LanguageChange)
            setAccessibleName(MainWindow::regionName(region_));
        return QWidget::event(e);
    }

    void paintEvent(QPaintEvent*) override
    {
        const auto& t = theme::ThemeManager::instance();
        QPainter p(this);
        const QRect r = rect();
        switch (region_) {
        case Region::Canvas:
            p.fillRect(r, t.color(tok::c_canvas));
            if (ui::paint::focusVisible(this)) {
                p.setRenderHint(QPainter::Antialiasing);
                ui::paint::drawRing(p, r, t.color(tok::c_focus));
            }
            return;
        case Region::Ribbon: // hàng tab --c-chrome trên thân --c-base
            p.fillRect(r, t.color(tok::c_base));
            p.fillRect(r.adjusted(0, 0, 0, qRound(t.px(tok::h_tab)) - r.height()), t.color(tok::c_chrome));
            break;
        case Region::CommandLine:
            p.fillRect(r, t.color(tok::c_base));
            break;
        default:
            p.fillRect(r, t.color(tok::c_chrome));
            break;
        }
        // Nét chia --c-border: dưới các hàng trên vùng vẽ, trên các hàng dưới vùng vẽ.
        const bool above = region_ < Region::Canvas;
        p.fillRect(QRect(0, above ? r.bottom() : 0, r.width(), kLine), t.color(tok::c_border));
    }

private:
    bool compact() const { return property("compact").toBool(); }

    static QAccessibleInterface* accessible(const QString&, QObject* o)
    {
        auto* p = dynamic_cast<Placeholder*>(o);
        return p && p->region_ == Region::Canvas ? new QAccessibleWidget(p, QAccessible::Canvas) : nullptr;
    }

    Region region_;
};

// Hàng dưới ribbon (QAT "Hiện bên dưới ribbon"): nền --c-chrome, nét dưới --c-border như các hàng trên vùng vẽ; lề
// trái --sp-2 như .tb, trên/dưới --sp-1 (đọc lúc dựng: --sp-* như nhau ở mọi chế độ). ponytail: prototype không vẽ
// hàng này (chỉ có mục menu "Hiện bên dưới ribbon"); theo .tb.
class BelowRibbon final : public QWidget {
public:
    explicit BelowRibbon(QWidget* parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_OpaquePaintEvent);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        const auto& t = theme::ThemeManager::instance();
        const int sp1 = qRound(t.px(tok::sp_1)), sp2 = qRound(t.px(tok::sp_2));
        auto* row = new QHBoxLayout(this);
        row->setContentsMargins(sp2, sp1, sp2, sp1 + kLine);
        row->setSpacing(0);
        row->addStretch(1); // widget chèn trước stretch: xếp từ trái
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        const auto& t = theme::ThemeManager::instance();
        QPainter p(this);
        p.fillRect(rect(), t.color(tok::c_chrome));
        p.fillRect(QRect(0, height() - kLine, width(), kLine), t.color(tok::c_border));
    }
};

// Palette giữ chỗ trong bố cục mặc định của prototype (core.js TC.S.pal: phải Thuộc tính, Layer, Bác sĩ Font; trái
// Thư viện TCVN, Blocks). ponytail: nội dung EmptyState; UIC-002 (Thuộc tính), UIC-009 (Layer), UIJ-002 (Bác sĩ Font),
// UIJ-011 (Thư viện TCVN), UIG-015 (Blocks) thay hàm dựng.
struct PaletteInfo {
    const char* id;
    const char* title;
    const char* icon;
    Qt::DockWidgetArea area;
};
constexpr PaletteInfo kPalettes[] = {
    {"props", QT_TRANSLATE_NOOP("trion::shell::MainWindow", "Thuộc tính"), "props", Qt::RightDockWidgetArea},
    {"layers", QT_TRANSLATE_NOOP("trion::shell::MainWindow", "Layer"), "layers", Qt::RightDockWidgetArea},
    {"fontdoc", QT_TRANSLATE_NOOP("trion::shell::MainWindow", "Bác sĩ Font"), "fontdoc", Qt::RightDockWidgetArea},
    {"tcvnlib", QT_TRANSLATE_NOOP("trion::shell::MainWindow", "Thư viện TCVN"), "lib", Qt::LeftDockWidgetArea},
    {"blocks", QT_TRANSLATE_NOOP("trion::shell::MainWindow", "Blocks"), "blocks", Qt::LeftDockWidgetArea},
};

QWidget* placeholderPalette(const char* icon, const std::function<QString()>& title)
{
    auto* e = new ui::EmptyState(QString::fromLatin1(icon));
    const auto text = [e, title] { e->setText(title(), MainWindow::tr("Nội dung palette có ở bản sau.")); };
    text();
    if (const auto* t = i18n::Translator::instance())
        QObject::connect(t, &i18n::Translator::languageChanged, e, text);
    return e;
}

} // namespace

MainWindow::MainWindow(settings::Store& store, QWidget* parent) : QMainWindow(parent), store_(store)
{
    auto* frame = new QWidget(this);
    column_ = new QVBoxLayout(frame);
    column_->setContentsMargins({});
    column_->setSpacing(0);
    docks_ = new QMainWindow(frame);
    docks_->setWindowFlags(Qt::Widget); // QMainWindow con làm vùng dock giữa tab tệp và tab layout
    for (int i = 0; i < kRegionCount; ++i) {
        auto* w = new Placeholder(Region(i));
        regions_[size_t(i)] = w;
        if (Region(i) == Region::Canvas) {
            docks_->setCentralWidget(w);
            column_->addWidget(docks_, 1);
        } else {
            column_->addWidget(w);
        }
    }
    belowRibbon_ = new BelowRibbon(frame);
    column_->insertWidget(column_->indexOf(regions_[size_t(Region::Ribbon)]) + 1, belowRibbon_);
    belowRibbon_->hide();
    belowRibbon_->installEventFilter(this);
    regions_[size_t(Region::Canvas)]->installEventFilter(this);
    regions_[size_t(Region::CommandLine)]->installEventFilter(this);
    setCentralWidget(frame);
    palettes_ = new ui::PaletteHost(docks_, store_);
    registerPalettes();
    workspace_ = new Workspace(*this, store_);
}

MainWindow::~MainWindow() = default;

void MainWindow::registerPalettes()
{
    for (const PaletteInfo& p : kPalettes) {
        const std::function<QString()> title = [t = p.title] { return tr(t); };
        const QString id = QString::fromLatin1(p.id);
        // Đóng, ghim: Workspace đặt bố cục khi khôi phục. Nội dung thật (setPaletteContent) nếu có.
        palettes_->registerPalette({id, title,
                                    [this, id, title, icon = p.icon] {
                                        QWidget* placeholder = placeholderPalette(icon, title);
                                        const auto it = contents_.constFind(id);
                                        return it == contents_.cend() ? placeholder : (*it)(placeholder);
                                    },
                                    p.area, false, true});
    }
}

QString MainWindow::regionName(Region r)
{
    switch (r) { // aria-label của prototype (A.mount) nếu có
    case Region::TitleBar:
        return tr("Thanh tiêu đề");
    case Region::Ribbon:
        return tr("Ribbon");
    case Region::FileTabs:
        return tr("Bản vẽ đang mở");
    case Region::Canvas:
        return tr("Vùng vẽ");
    case Region::LayoutTabs:
        return tr("Model và layout");
    case Region::CommandLine:
        return tr("Dòng lệnh");
    case Region::StatusBar:
        return tr("Thanh trạng thái");
    }
    return {};
}

void MainWindow::setRegion(Region r, QWidget* widget)
{
    TRION_CHECK(widget);
    QWidget*& slot = regions_[size_t(r)];
    if (!widget || widget == slot)
        return;
    if (r == Region::CommandLine)
        setCommandLineFloating(false); // widget cũ về ô của nó trong cột rồi mới thay
    const bool hidden = slot->isHidden();
    if (r == Region::Canvas) {
        // Dòng lệnh nổi là con của vùng vẽ cũ: thôi nổi (gom lịch sử về widget), giữ tạm ở vùng dock (cùng cửa sổ: giữ
        // chỗ trong chuỗi focus), rồi nổi lại trên vùng vẽ mới ở chỗ đã chọn; đổi cỡ của vùng vẽ mới kẹp lại theo đó.
        QWidget* cmd = floating_ ? region(Region::CommandLine) : nullptr;
        const bool cmdHidden = cmd && cmd->isHidden();
        if (cmd) {
            emit commandLineFloatChanged(false, {});
            cmd->setParent(docks_);
        }
        delete docks_->takeCentralWidget(); // setCentralWidget chỉ deleteLater widget cũ
        docks_->setCentralWidget(widget);
        slot = widget;
        widget->setVisible(!hidden);
        if (cmd) {
            cmd->setParent(widget);
            placeFloating();
            cmd->setVisible(!cmdHidden);
        }
        widget->installEventFilter(this);
        return;
    }
    delete column_->replaceWidget(slot, widget);
    delete slot;
    slot = widget;
    widget->setVisible(!hidden);
    if (r == Region::CommandLine) {
        widget->setProperty("compact", screenClass_ != ui::ScreenClass::Wide);
        widget->installEventFilter(this);
    }
}

void MainWindow::setRegionVisible(Region r, bool visible)
{
    region(r)->setVisible(visible);
    if (r == Region::TitleBar)
        syncBelowRibbon();
}

void MainWindow::setCommandLineFloating(bool floating, QPoint at)
{
    QWidget* cmd = region(Region::CommandLine);
    if (floating) {
        floatCommandLine(QRect(region(Region::Canvas)->mapFromGlobal(at), QSize(floating_ ? cmd->width() : 0, 0)));
        floatAt_ = QRect(cmd->pos(), QSize(cmd->width(), 0)); // chỗ đã kẹp, rộng thật
    } else if (floating_) {
        const bool hidden = cmd->isHidden();
        floating_ = false;
        emit commandLineFloatChanged(false, {});
        column_->insertWidget(column_->indexOf(region(Region::LayoutTabs)) + 1, cmd);
        cmd->setVisible(!hidden);
    }
}

void MainWindow::floatCommandLine(const QRect& want)
{
    QWidget* cmd = region(Region::CommandLine);
    const bool entering = !floating_;
    const bool hidden = cmd->isHidden();
    if (entering) {
        column_->removeWidget(cmd);
        cmd->setParent(region(Region::Canvas)); // setParent ẩn widget: hiện lại theo trạng thái của vùng
        floating_ = true;
    }
    floatAt_ = want;
    placeFloating();
    if (entering)
        cmd->setVisible(!hidden);
}

void MainWindow::placeFloating()
{
    QWidget* cmd = region(Region::CommandLine);
    const QPoint at = region(Region::Canvas)->mapToGlobal(floatAt_.topLeft());
    placing_ = true;
    emit commandLineFloatChanged(true, at); // vừa rời cột: rộng mặc định (CommandLine: 2/3 vùng vẽ)
    if (floatAt_.width() > 0 && cmd->width() != floatAt_.width()) {
        cmd->resize(floatAt_.width(), cmd->height());
        emit commandLineFloatChanged(true, at); // kẹp lại với rộng đã chọn
    }
    placing_ = false;
    placed_ = cmd->pos();
}

void MainWindow::addBelowRibbon(QWidget* w)
{
    auto* row = static_cast<QHBoxLayout*>(belowRibbon_->layout());
    row->insertWidget(row->count() - 1, w); // trước stretch cuối; hàng đang ẩn: w hiện cùng hàng
    syncBelowRibbon();
}

void MainWindow::syncBelowRibbon()
{
    const QObjectList& kids = belowRibbon_->children();
    const bool used = std::any_of(kids.cbegin(), kids.cend(), [](const QObject* o) { return o->isWidgetType(); });
    belowRibbon_->setVisible(used && isRegionVisible(Region::TitleBar));
}

void MainWindow::setPaletteContent(const QString& id, std::function<QWidget*(QWidget* placeholder)> create)
{
    contents_.insert(id, std::move(create));
}

bool MainWindow::eventFilter(QObject* watched, QEvent* e)
{
    if (watched == belowRibbon_) {
        if (e->type() == QEvent::ChildRemoved) // layout khác lấy widget đi (QAT về thanh tiêu đề)
            syncBelowRibbon();
    } else if (floating_ && !placing_) {
        // Vùng vẽ đổi cỡ (cả lúc khôi phục: cửa sổ hiện, phóng to): về chỗ đã chọn, kẹp trong vùng vẽ. Dòng lệnh dời tới
        // chỗ khác chỗ vừa đặt: người dùng kéo dải nắm, đó là chỗ đã chọn mới.
        QWidget* cmd = region(Region::CommandLine);
        if (watched == region(Region::Canvas) && e->type() == QEvent::Resize)
            placeFloating();
        else if (watched == cmd && e->type() == QEvent::Move && cmd->pos() != placed_)
            floatAt_.moveTopLeft(placed_ = cmd->pos());
    }
    return QMainWindow::eventFilter(watched, e);
}

bool MainWindow::isRegionVisible(Region r) const
{
    return !region(r)->isHidden();
}

void MainWindow::setScreenClass(ui::ScreenClass c)
{
    if (c == screenClass_)
        return;
    const bool reapply = restored_ && workspace_->isDefaultLayout(); // trước khi đổi lớp
    screenClass_ = c;
    region(Region::CommandLine)->setProperty("compact", c != ui::ScreenClass::Wide);
    if (reapply)
        workspace_->applyLayout();
}

void MainWindow::restore()
{
    if (!restoreGeometry(store_.get(kGeometry))) {
        // Lần đầu (hoặc dữ liệu hỏng): phóng to trên màn hình chính; khôi phục kích thước thì còn 4/5 vùng làm việc.
        const QRect area = QGuiApplication::primaryScreen()->availableGeometry();
        setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, area.size() * 4 / 5, area));
        setWindowState(windowState() | Qt::WindowMaximized);
    }
    screenClass_ = ui::DisplayContext::classify(screen()->geometry().width());
    region(Region::CommandLine)->setProperty("compact", screenClass_ != ui::ScreenClass::Wide);
    // Bố cục đã lưu mà người dùng chưa từng đổi: áp lại mặc định theo lớp màn hình lúc này (có thể khác lần trước).
    workspace_->restore(palettes_->restoreLayout(screenClass_) && store_.get(kCustomLayout));
    // Dòng lệnh nổi: rời cột ngay (khung đầu đúng bố cục); chỗ, rộng đã lưu áp lại mỗi lần vùng vẽ đổi cỡ (hiện,
    // phóng to) tới khi người dùng kéo.
    if (const QJsonObject f = store_.get(kCommandLineFloat); !f.isEmpty())
        floatCommandLine({f[u"x"_s].toInt(), f[u"y"_s].toInt(), std::max(0, f[u"width"_s].toInt()), 0});
    restored_ = true;
    region(Region::Canvas)->setFocus(); // vùng vẽ nhận phím gõ (07.1 §3)
}

void MainWindow::save()
{
    if (!restored_)
        return;
    emit aboutToSave();
    store_.set(kGeometry, saveGeometry());
    palettes_->saveLayout();
    store_.set(kCustomLayout, !workspace_->isDefaultLayout());
    QJsonObject f;
    if (floating_)
        f = {{u"x"_s, floatAt_.x()}, {u"y"_s, floatAt_.y()}, {u"width"_s, floatAt_.width()}};
    store_.set(kCommandLineFloat, f);
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    save();
    QMainWindow::closeEvent(e);
}

bool MainWindow::onTitleBar(QPoint p) const
{
    const QWidget* t = region(Region::TitleBar);
    return t->isVisible() && QRect(t->mapTo(this, QPoint()), t->size()).contains(p);
}

void MainWindow::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && onTitleBar(e->position().toPoint())) {
        pressAt_ = e->position().toPoint(); // kéo đủ xa mới di chuyển: nhấp đúp vẫn tới mouseDoubleClickEvent
        e->accept();
        return;
    }
    QMainWindow::mousePressEvent(e);
}

void MainWindow::mouseMoveEvent(QMouseEvent* e)
{
    if (pressAt_ && (e->buttons() & Qt::LeftButton) &&
        (e->position().toPoint() - *pressAt_).manhattanLength() >= QApplication::startDragDistance()) {
        pressAt_.reset();
        if (QWindow* w = windowHandle())
            w->startSystemMove(); // hệ điều hành kéo cửa sổ (kể cả dính cạnh màn hình)
        e->accept();
        return;
    }
    QMainWindow::mouseMoveEvent(e);
}

void MainWindow::mouseReleaseEvent(QMouseEvent* e)
{
    pressAt_.reset();
    QMainWindow::mouseReleaseEvent(e);
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && onTitleBar(e->position().toPoint())) {
        pressAt_.reset();
        setWindowState(windowState() ^ Qt::WindowMaximized);
        e->accept();
        return;
    }
    QMainWindow::mouseDoubleClickEvent(e);
}

} // namespace trion::shell
