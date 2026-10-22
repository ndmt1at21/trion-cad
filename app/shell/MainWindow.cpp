#include "app/shell/MainWindow.h"

#include "app/core/i18n/Translator.h"
#include "app/shell/Workspace.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/dock/PaletteHost.h"
#include "app/widgets/message/Messages.h"
#include "core/base/assert.h"

#include <QApplication>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QStyle>
#include <QVBoxLayout>
#include <QWindow>

using namespace Qt::StringLiterals;

namespace trion::shell {

using Region = MainWindow::Region;

namespace {

const settings::Setting<QByteArray> kGeometry{"ui/window/geometry", {}};
const settings::Setting<bool> kCustomLayout{"ui/window/customLayout", false};

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
// Vùng vẽ: nền --c-canvas, nhận focus. ponytail: UIA-002 (thanh tiêu đề), UIA-005 (ribbon), UIA-010 (tab tệp),
// UIA-011 (tab layout), UIA-107 (dòng lệnh), UIA-009 (thanh trạng thái), UIA-109 (vùng vẽ) thay qua setRegion.
class Placeholder final : public QWidget {
public:
    explicit Placeholder(Region r) : region_(r)
    {
        setAttribute(Qt::WA_OpaquePaintEvent);
        setAccessibleName(MainWindow::regionName(r));
        if (r == Region::Canvas) {
            setFocusPolicy(Qt::StrongFocus);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
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

    Region region_;
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
        // Đóng, ghim: Workspace đặt bố cục khi khôi phục.
        palettes_->registerPalette({QString::fromLatin1(p.id), title,
                                    [title, icon = p.icon] { return placeholderPalette(icon, title); }, p.area, false,
                                    true});
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
    const bool hidden = slot->isHidden();
    if (r == Region::Canvas) {
        delete docks_->takeCentralWidget(); // setCentralWidget chỉ deleteLater widget cũ
        docks_->setCentralWidget(widget);
    } else {
        delete column_->replaceWidget(slot, widget);
        delete slot;
    }
    slot = widget;
    widget->setVisible(!hidden);
    if (r == Region::CommandLine)
        widget->setProperty("compact", screenClass_ != ui::ScreenClass::Wide);
}

void MainWindow::setRegionVisible(Region r, bool visible)
{
    region(r)->setVisible(visible);
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
