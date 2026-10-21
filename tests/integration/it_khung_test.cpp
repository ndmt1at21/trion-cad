// Integration test nhóm 4 (khung giao diện): UI0-001 (thứ tự khởi động, thoát), UI0-015 (hành động → chuỗi lệnh, phím),
// UI0-011 (cổng phiên bản), UI0-107 (menu, tooltip), UI0-110 + UI0-008 (hộp thoại, xác nhận, lỗi), UI0-113 + UI0-010
// (palette, dải tự ẩn), NT-013 + UI0-112 (việc nền, tiến trình), UI0-009 (toast, trung tâm thông báo), UI0-007 (ô số),
// UIA-106 (dòng lệnh), UI0-108 (bảng ảo) trên một cửa sổ chính dựng như ứng dụng (UIA-001 chưa có): vùng vẽ giả ở giữa,
// dòng lệnh ở thanh trạng thái, palette "Tác vụ nền" bên phải, màn hình giả 1440×900 (lớp Narrow: palette tự ẩn).
// Chỗ nối của main chưa có (UIA-015, INT-002, INT-003) do test đóng vai: chỗ nhận chuỗi lệnh, chú thích nâng cấp →
// toast, việc lỗi → hộp thoại lỗi, lấy khoảng cách trên bản vẽ.

#include "app/core/actions/ActionRegistry.h"
#include "app/core/display/DisplayContext.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/keymap/Keymap.h"
#include "app/core/perf/UiProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/cmdline/CommandInput.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/dialog/ConfirmDialog.h"
#include "app/widgets/dialog/DialogFrame.h"
#include "app/widgets/dialog/DialogHost.h"
#include "app/widgets/dialog/ErrorDialog.h"
#include "app/widgets/dialog/SuppressedPrompts.h"
#include "app/widgets/dock/AutoHideStrip.h"
#include "app/widgets/dock/PaletteHost.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/numeric/NumericField.h"
#include "app/widgets/progress/InlineProgress.h"
#include "app/widgets/progress/ProgressPresenter.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "app/widgets/views/VirtualTableView.h"
#include "core/base/jobs/BackgroundJobs.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"

#include <QAbstractButton>
#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPointer>
#include <QScopeGuard>
#include <QScreen>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWindow>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <utility>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;
using trion::edition::Edition;
using trion::edition::EditionGate;
using trion::edition::UpgradeRequest;
using trion::edition::UpgradeRouter;
using trion::settings::Store;
namespace i18n = trion::i18n;
namespace jobs = trion::jobs;
namespace probe = trion::ui::probe;
namespace sched = trion::sched;
namespace ui = trion::ui;

namespace {

QString g_cwd; // thư mục chạy test; initMain đổi sang thư mục chứa cấu hình màn hình tới khi tạo lớp test

// Chữ mẫu của test (màn hình đặt chưa có): qua bộ dịch đang cài nên ở vi-XL dài ra như chuỗi thật; đọc qua biến nên
// lupdate không thu.
enum Text {
    kOffsetTitle,
    kOffsetConfirm,
    kLayoutsTitle,
    kPlotTitle,
    kPlotConfirm,
    kPropsTitle,
    kApply,
    kDistance,
    kEraseSelected,
    kDeleteTitle,
    kDeleteAction,
    kDeleteQuestion,
    kTasks,
    kSee,
    kJobPlot,
    kJobOpen,
    kJobExport,
    kJobSave,
    kOpenFailed,
    kOpenFailedWhy,
    kExported,
    TextCount
};
constexpr const char* kTexts[TextCount] = {
    "Dời song song",                                                     // i18n-ok: chữ mẫu của test
    "Dời",                                                               // i18n-ok: chữ mẫu của test
    "Quản lý layout",                                                    // i18n-ok: chữ mẫu của test
    "Xuất PDF",                                                          // i18n-ok: chữ mẫu của test
    "Xuất",                                                              // i18n-ok: chữ mẫu của test
    "Tỉ lệ in",                                                          // i18n-ok: chữ mẫu của test
    "Áp dụng",                                                           // i18n-ok: chữ mẫu của test
    "Khoảng cách",                                                       // i18n-ok: chữ mẫu của test
    "Xóa layout đã chọn",                                                // i18n-ok: chữ mẫu của test
    "Xóa layout",                                                        // i18n-ok: chữ mẫu của test
    "Xóa %1 layout",                                                     // i18n-ok: chữ mẫu của test
    "Xóa %1 layout đã chọn? Đối tượng trên các layout này cũng bị xóa.", // i18n-ok: chữ mẫu của test
    "Tác vụ nền",                                                        // i18n-ok: chữ mẫu của test
    "Xem",                                                               // i18n-ok: chữ mẫu của test
    "Đang xuất PDF",                                                     // i18n-ok: chữ mẫu của test
    "Đang mở",                                                           // i18n-ok: chữ mẫu của test
    "Đang xuất bảng thống kê",                                           // i18n-ok: chữ mẫu của test
    "Đang lưu",                                                          // i18n-ok: chữ mẫu của test
    "Không mở được bản vẽ",                                              // i18n-ok: chữ mẫu của test
    "Tệp bị hỏng. Mở bản lưu tự động hoặc chạy RECOVER.",                // i18n-ok: chữ mẫu của test
    "Đã xuất 24 tờ PDF",                                                 // i18n-ok: chữ mẫu của test
};
constexpr const char* kCorrupt = "Bảng đối tượng hỏng ở mục 3"; // i18n-ok: thông điệp kỹ thuật của lỗi giả
constexpr const char* kLogLine = "[12:00:01] dwg: đọc bảng đối tượng"; // i18n-ok: dòng log giả

QString tx(Text t)
{
    const char* source = kTexts[t];
    return QCoreApplication::translate("IntegrationKhung", source);
}

// Chạy vòng sự kiện tới khi cond đúng hoặc hết giờ. Timer trên stack: chờ xong không để lại timer nào chạy tiếp.
bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 20s)
{
    if (cond())
        return true;
    QEventLoop loop;
    QTimer tick, limit;
    QObject::connect(&tick, &QTimer::timeout, &loop, [&] {
        if (cond())
            loop.quit();
    });
    limit.setSingleShot(true);
    QObject::connect(&limit, &QTimer::timeout, &loop, &QEventLoop::quit);
    limit.start(timeout);
    tick.start(1);
    loop.exec();
    return cond();
}

void spinFor(std::chrono::milliseconds d)
{
    QEventLoop loop;
    QTimer limit;
    limit.setSingleShot(true);
    QObject::connect(&limit, &QTimer::timeout, &loop, &QEventLoop::quit);
    limit.start(d);
    loop.exec();
}

bool focusOn(QWidget* w)
{
    w->window()->activateWindow();
    w->setFocus(Qt::OtherFocusReason);
    return spinUntil([w] { return QApplication::focusWidget() == w; });
}

// Phím tới widget đang có focus (hộp thoại vừa mở tự đặt focus đầu); false nếu không có.
bool keyToFocus(Qt::Key key)
{
    QWidget* w = QApplication::focusWidget();
    if (w)
        QTest::keyClick(w, key);
    return w;
}

// Con trỏ chuột tới điểm toàn cục: plugin offscreen sinh Enter/Leave và di chuột cho cửa sổ dưới con trỏ.
void mouseTo(QPoint global)
{
    QCursor::setPos(global);
    QCoreApplication::processEvents();
}

QPoint centerOf(const QWidget* w)
{
    return w->mapToGlobal(w->rect().center());
}

// Khung trượt của dải đã mở hết (animation xong).
bool opened(const ui::AutoHideStrip* s)
{
    return s && s->current() && s->flyout()->isVisible() && s->flyout()->geometry() == s->flyoutRect();
}

QString truncations(QWidget* root)
{
    QStringList out;
    for (const i18n::Truncation& t : i18n::findTruncations(root))
        out << t.toString();
    return out.join(u"; "_s);
}

// Đếm KeyPress Esc tới một widget (dòng lệnh không được nhận Esc dành cho khung trượt).
class EscSpy final : public QObject {
public:
    explicit EscSpy(QWidget* w) { w->installEventFilter(this); }
    int count = 0;

protected:
    bool eventFilter(QObject*, QEvent* e) override
    {
        count += e->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(e)->key() == Qt::Key_Escape;
        return false;
    }
};

ui::ValueSpec mm()
{
    ui::ValueSpec s{ui::ValueKind::Distance, {}, 4}; // INSUNITS 4: milimét
    s.units.luprec = 2;
    return s;
}

// Hộp thoại mẫu như màn hình đặt (07.2): ô khoảng cách (UI0-007), một nút trong thân, Hủy, nút xác nhận đọc giá trị
// của ô lúc accept; dịch lại khi đổi ngôn ngữ theo DialogFrame.h.
class SampleDialog final : public ui::DialogFrame {
public:
    SampleDialog(const QString& id, bool modal, Text title, Text confirm, QWidget* parent)
        : DialogFrame(id, parent), title_(title), confirm_(confirm)
    {
        setModal(modal);
        setCommand(id.toUpper());
        auto* body = new QWidget;
        auto* box = new QVBoxLayout(body);
        box->setContentsMargins({});
        field = new ui::NumericField(ui::ValueKind::Distance);
        field->setSpec(mm());
        field->setValue(10);
        action = new ui::Button;
        box->addWidget(field);
        box->addWidget(action);
        setBody(body);
        cancel = addCancel();
        retranslate();
    }

    void accept() override
    {
        if (out)
            *out = field->value();
        DialogFrame::accept();
    }

    ui::NumericField* field;
    ui::Button* action;
    ui::Button* cancel;
    double* out = nullptr; // giá trị nút xác nhận đọc được

protected:
    void changeEvent(QEvent* e) override
    {
        if (e->type() == QEvent::LanguageChange)
            retranslate();
        DialogFrame::changeEvent(e);
    }

private:
    void retranslate()
    {
        setWindowTitle(tx(title_));
        setConfirm(tx(confirm_));
        field->setLabel(tx(kDistance));
        action->setText(tx(kEraseSelected));
    }

    Text title_, confirm_;
};

ui::DialogFrame* makeLayouts(QWidget* parent)
{
    return new SampleDialog(u"layouts"_s, true, kLayoutsTitle, kApply, parent);
}

ui::ConfirmDialog confirmDelete()
{
    return ui::ConfirmDialog(tx(kDeleteTitle), tx(kDeleteAction), 2)
        .question(tx(kDeleteQuestion))
        .danger()
        .dontAsk(u"layout.delete"_s);
}

} // namespace

class IntegrationKhung : public QObject {
    Q_OBJECT
public:
    // Sau QApplication (plugin đã đọc cấu hình màn hình), trước qExec: về lại thư mục chạy test.
    IntegrationKhung() { QDir::setCurrent(g_cwd); }

    // Trước QApplication (QTEST_MAIN), như main.cpp: DPR không làm tròn; một màn hình giả 1440×900 100% (lớp Narrow).
    static void initMain()
    {
        for (const char* v : {"QT_ENABLE_HIGHDPI_SCALING", "QT_SCALE_FACTOR", "QT_SCREEN_SCALE_FACTORS",
                              "QT_SCALE_FACTOR_ROUNDING_POLICY"})
            qunsetenv(v);
        ui::DisplayContext::initMain();
        const QJsonObject screen{{u"name"_s, u"n1440"_s},   {u"x"_s, 0},        {u"y"_s, 0},
                                 {u"width"_s, 1440},        {u"height"_s, 900}, {u"logicalDpi"_s, 96},
                                 {u"logicalBaseDpi"_s, 96}, {u"dpr"_s, 1}};
        // Đường dẫn tương đối: tùy chọn của plugin ngăn cách bằng ':', đường dẫn Windows "C:/…" có ':'.
        static QTemporaryDir config;
        g_cwd = QDir::currentPath();
        QFile f(config.filePath(u"screens.json"_s));
        const QByteArray json = QJsonDocument(QJsonObject{{u"screens"_s, QJsonArray{screen}}}).toJson();
        if (!config.isValid() || !f.open(QIODevice::WriteOnly) || f.write(json) != json.size() ||
            !QDir::setCurrent(config.path()))
            qFatal("không ghi được cấu hình màn hình");
        qputenv("QT_QPA_PLATFORM", "offscreen:configfile=screens.json");
    }

private:
    QTemporaryDir dir_;
    std::unique_ptr<Store> store_;
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<trion::theme::ThemeManager> theme_;
    std::unique_ptr<trion::theme::IconProvider> icons_;
    std::unique_ptr<trion::keymap::Keymap> keymap_;
    std::unique_ptr<ui::RichTooltip> tips_;
    std::unique_ptr<QMainWindow> window_;
    QWidget* canvas_ = nullptr;       // vùng vẽ giả: widget trung tâm nhận focus
    ui::CommandInput* cmd_ = nullptr; // dòng lệnh (UIA-106), chờ tên lệnh
    ui::PaletteHost* host_ = nullptr; // con của window_
    std::unique_ptr<ui::DisplayContext> display_;
    std::unique_ptr<trion::actions::Sysvars> sysvars_;
    std::unique_ptr<trion::actions::ActionRegistry> reg_;
    std::unique_ptr<EditionGate> gate_;
    std::unique_ptr<jobs::BackgroundJobs> jobs_;
    std::unique_ptr<ui::ProgressPresenter> presenter_;
    std::unique_ptr<ui::NotificationCenter> notes_;
    std::unique_ptr<ui::ToastHost> toasts_;
    std::unique_ptr<ui::DialogHost> dialogs_;
    QStringList sent_;   // chuỗi lệnh tới chỗ nối INT-002
    QList<QUrl> opened_; // trang mua đã mở (thay QDesktopServices)

    QStringList take() { return std::exchange(sent_, {}); }

    ui::Toast* toastOf(ui::Severity s) const
    {
        for (ui::Toast* t : toasts_->toasts())
            if (t->severity() == s)
                return t;
        return nullptr;
    }

    // Đóng mọi toast như bấm × (không qua chuột: cửa sổ chính có thể đang bị hộp thoại modal chặn).
    bool clearToasts()
    {
        for (ui::Toast* t : toasts_->toasts())
            t->closeButton()->click();
        return spinUntil([&] { return toasts_->toasts().isEmpty(); });
    }

    // Dòng chữ đang hiện của tooltip; huy hiệu phiên bản là widget không phải QLabel trong dòng tên.
    QStringList tipLines() const
    {
        QStringList out;
        for (QLabel* l : tips_->tipWindow()->findChildren<QLabel*>())
            if (!l->isHidden())
                out << l->text();
        return out;
    }
    bool tipBadgeShown() const
    {
        for (QWidget* w : tips_->tipWindow()->findChildren<QWidget*>(Qt::FindDirectChildrenOnly))
            if (!qobject_cast<QLabel*>(w))
                return !w->isHidden();
        return false;
    }

    ui::AutoHideStrip* strip() const { return host_->strip(Qt::RightDockWidgetArea); }
    ui::VirtualTableView* taskList() const { return qobject_cast<ui::VirtualTableView*>(host_->content(u"tasks"_s)); }

private slots:
    void initTestCase()
    {
        QCOMPARE(QGuiApplication::screens().size(), qsizetype(1));
        QCOMPARE(QGuiApplication::primaryScreen()->geometry().width(), 1440);
        QVERIFY(dir_.isValid());
        // Linux: "giảm chuyển động" của hệ điều hành (DisplayContext) đọc $XDG_CONFIG_HOME, không lấy của máy chạy
        // test.
        QVERIFY(QDir(dir_.path()).mkpath(u"config"_s));
        qputenv("XDG_CONFIG_HOME", QFile::encodeName(dir_.filePath(u"config"_s)));

        // Thứ tự của Application (UI0-001): Store → UiExecutor → Translator → loadFonts → ThemeManager → IconProvider →
        // Keymap → RichTooltip → cửa sổ chính → DisplayContext → hiện cửa sổ. Phần của nhóm 4 dựng theo phụ thuộc
        // trước khi hiện: hành động → cổng phiên bản → việc nền → tiến trình → thông báo → hộp thoại → palette.
        store_ = std::make_unique<Store>(dir_.filePath(u"user.json"_s));
        Store::setInstance(store_.get());
        executor_ = std::make_unique<ui::UiExecutor>();
        translator_ = std::make_unique<i18n::Translator>();
        QVERIFY(trion::theme::loadFonts());
        theme_ = std::make_unique<trion::theme::ThemeManager>(*store_);
        icons_ = std::make_unique<trion::theme::IconProvider>();
        keymap_ = std::make_unique<trion::keymap::Keymap>(*store_, trion::keymap::Platform::Windows);
        tips_ = std::make_unique<ui::RichTooltip>();
        tips_->setKeymap(keymap_.get());
        QCOMPARE(translator_->language(), u"vi"_s);

        window_ = std::make_unique<QMainWindow>();
        canvas_ = new QWidget;
        canvas_->setFocusPolicy(Qt::StrongFocus);
        window_->setCentralWidget(canvas_);
        auto* status = new QStatusBar;
        status->setSizeGripEnabled(false);
        cmd_ = new ui::CommandInput;
        cmd_->setAwaitingCommand(true);
        status->addWidget(cmd_, 1);
        window_->setStatusBar(status);

        sysvars_ = std::make_unique<trion::actions::Sysvars>();
        reg_ = std::make_unique<trion::actions::ActionRegistry>(*keymap_, *sysvars_);
        reg_->setSink([this](const QString& macro) { sent_ << macro; });
        reg_->install(window_.get());
        reg_->addViewport(canvas_);
        gate_ = std::make_unique<EditionGate>();
        gate_->bind(*reg_);
        gate_->upgrade().setUrlOpener([this](const QUrl& url) {
            opened_ << url;
            return true;
        });

        jobs_ = std::make_unique<jobs::BackgroundJobs>(jobs::Timing{60ms, 250ms, 20ms});
        presenter_ = std::make_unique<ui::ProgressPresenter>(*jobs_);
        notes_ = std::make_unique<ui::NotificationCenter>();
        toasts_ = std::make_unique<ui::ToastHost>(window_.get(), *notes_);
        toasts_->setTimeout(1h); // kịch bản cần tự ẩn thì rút ngắn
        toasts_->followJobs(*presenter_);
        // Chỗ nối của main (UIA-015 sẽ làm): chú thích nâng cấp → toast có liên kết "Xem" mở trang.
        connect(&gate_->upgrade(), &UpgradeRouter::noticeRequested, toasts_.get(), [this](const UpgradeRequest& r) {
            toasts_->post(ui::Severity::Info, UpgradeRouter::noticeText(r),
                          {tx(kSee), [this, r] { gate_->upgrade().open(r); }, ui::Button::Kind::Normal}, u"license"_s);
        });

        dialogs_ = std::make_unique<ui::DialogHost>(window_.get(), *store_);
        dialogs_->add(u"offset"_s,
                      [](QWidget* p) { return new SampleDialog(u"offset"_s, true, kOffsetTitle, kOffsetConfirm, p); });
        dialogs_->add(u"plot"_s,
                      [](QWidget* p) { return new SampleDialog(u"plot"_s, true, kPlotTitle, kPlotConfirm, p); });
        dialogs_->add(u"props"_s,
                      [](QWidget* p) { return new SampleDialog(u"props"_s, false, kPropsTitle, kApply, p); });
        QCOMPARE(dialogs_->confirmLabelIssues(), QStringList());

        host_ = new ui::PaletteHost(window_.get(), *store_);
        host_->registerPalette({u"tasks"_s, [] { return tx(kTasks); },
                                [this] {
                                    auto* view = new ui::VirtualTableView; // danh sách Tác vụ nền (UIA-009)
                                    view->setModel(presenter_->model());
                                    return view;
                                },
                                Qt::RightDockWidgetArea, true, true});
        QVERIFY(
            !host_->restoreLayout(ui::DisplayContext::classify(QGuiApplication::primaryScreen()->geometry().width())));

        display_ = std::make_unique<ui::DisplayContext>(window_.get());
        host_->trackDisplay(*display_);
        theme_->setReducedMotion(false); // DisplayContext áp "giảm chuyển động" của máy chạy test
        window_->showMaximized();
        QVERIFY(spinUntil([&] { return window_->isActiveWindow(); }));
        QCOMPARE(display_->screenClass(), ui::ScreenClass::Narrow);
    }

    void cleanupTestCase()
    {
        probe::disable();
        ui::ErrorDialog::setLogSource({});
        // Thoát như Application (UI0-001): còn việc nền đang chạy khi registry hủy thì việc bị hủy hợp tác; kết quả
        // việc gửi về sau khi presenter, toast đã hủy không được chạm chúng (ASan bắt).
        std::atomic<bool> running{false}, stopped{false};
        if (jobs_)
            jobs_->start(tx(kJobSave), u"save"_s, 0,
                         [&running, &stopped](jobs::Progress&, sched::CancelToken c) -> trion::Expected<void> {
                             running = true;
                             while (!c.cancelled())
                                 std::this_thread::sleep_for(1ms);
                             stopped = true;
                             return {};
                         });
        const bool started = spinUntil([&] { return running.load(); });
        dialogs_.reset();
        toasts_.reset();
        notes_.reset();
        presenter_.reset();
        jobs_.reset();
        gate_.reset();
        reg_.reset();
        sysvars_.reset();
        display_.reset();
        window_.reset();
        tips_.reset();
        icons_.reset();
        sched::waitIdle();
        QCoreApplication::processEvents(); // kết quả nền đã gửi về chạy khi đối tượng nhận đã hủy
        keymap_.reset();
        theme_.reset();
        translator_.reset();
        executor_.reset();
        Store::setInstance(nullptr);
        store_.reset();
        sched::waitIdle(); // lần ghi nền cuối của Store xong trước khi thư mục tạm bị xóa
        QVERIFY(started);
        QVERIFY(stopped);
    }

    // Sau mỗi kịch bản, kể cả khi trượt giữa chừng: trả các chỉnh tạm, đóng hộp thoại, popup còn mở để kịch bản sau
    // không bị modal chặn. Kịch bản đạt thì không còn gì để đóng.
    void cleanup()
    {
        Store::setInstance(store_.get());
        tips_->leave();
        tips_->setDelays(450ms, 1500ms);
        toasts_->setTimeout(1h);
        while (QWidget* popup = QApplication::activePopupWidget())
            popup->close();
        for (QDialog* d : window_->findChildren<QDialog*>())
            if (auto* f = qobject_cast<ui::DialogFrame*>(d); d->isVisible() || (f && f->isHiddenForPick()))
                d->reject();
        spinUntil([&] { return window_->findChildren<QDialog*>().isEmpty(); }, 5s);
    }

    // Kịch bản 1 — UI0-015 + UI0-011 + UI0-107 + UI0-009 (+ phím UI0-114, dòng lệnh UIA-106). Viewer: QSAVE (Pro) có
    // huy hiệu ở mục menu (trợ năng "khóa") và tooltip; Ctrl+S gõ khi focus ở dòng lệnh, mục menu chọn bằng Enter đều
    // không tới chỗ nối INT-002 mà thành yêu cầu nâng cấp: trang mua mở 1 lần, lần sau chú thích thành toast có "Xem"
    // (chỗ nối của main), focus không rời dòng lệnh. Kích hoạt Pro lúc menu và tooltip đang mở: cùng QAction mở khóa,
    // huy hiệu của tooltip đang hiện biến mất, mục menu và Ctrl+S gửi ^C^C_QSAVE. Unit test của UI0-011 gọi trigger()
    // trực tiếp, không qua phím của cửa sổ chính, toast hay tooltip đang hiện.
    void lockedActionUpgradeThenActivate()
    {
        gate_->setEdition(Edition::Viewer);
        QAction* save = reg_->action(u"QSAVE"_s);
        QAction* open = reg_->action(u"OPEN"_s);
        QVERIFY(save && open);
        ui::Menu menu(window_.get()); // menu Tệp (UIA-102 dựng từ cùng các QAction)
        menu.addAction(open);
        menu.addAction(save);
        QAccessibleInterface* acc = QAccessible::queryAccessibleInterface(&menu);
        QVERIFY(acc);
        const auto item = [&](QAction* a) { return acc->child(int(menu.actions().indexOf(a))); };
        const QString pro = ui::RichTooltip::editionName(ui::Edition::Pro);
        QVERIFY(item(save)->text(QAccessible::Description).contains(pro));
        QCOMPARE(item(save)->text(QAccessible::Accelerator), u"Ctrl+S"_s);
        QCOMPARE(item(open)->text(QAccessible::Description), QString()); // OPEN có ở Viewer

        QSignalSpy profiles(gate_.get(), &EditionGate::profileChanged);
        const int unread = notes_->unreadCount();
        opened_.clear();
        take();
        QVERIFY(focusOn(cmd_));
        QTest::keyClick(cmd_, Qt::Key_S, Qt::ControlModifier); // lần đầu trong phiên: trang mua kèm mã tính năng
        QCOMPARE(take(), QStringList());
        QCOMPARE(opened_.size(), 1);
        QCOMPARE(QUrlQuery(opened_[0]).queryItemValue(u"feature"_s), u"QSAVE"_s);
        QVERIFY(toasts_->toasts().isEmpty());

        QTest::keyClick(cmd_, Qt::Key_S, Qt::ControlModifier); // lần sau: chú thích → toast
        QCOMPARE(take(), QStringList());
        QCOMPARE(opened_.size(), 1);
        QCOMPARE(toasts_->toasts().size(), 1);
        ui::Toast* notice = toasts_->toasts()[0];
        QCOMPARE(notice->severity(), ui::Severity::Info);
        QCOMPARE(notice->text(),
                 UpgradeRouter::noticeText({u"QSAVE"_s, ui::Edition::Pro, ui::RichTooltip::name(save)}));
        QCOMPARE(notes_->unreadCount(), unread + 1);
        QVERIFY(cmd_->hasFocus());
        QTest::mouseClick(notice->actionButton(), Qt::LeftButton); // "Xem": mở trang dù đã mở trong phiên
        QCOMPARE(opened_.size(), 2);
        QVERIFY(toasts_->toasts().isEmpty());
        QCOMPARE(notes_->unreadCount(), unread);
        QVERIFY(cmd_->hasFocus()); // nút trên toast không lấy focus bằng cú nhấp

        // Mục menu (cùng QAction): rê → tooltip mức 1 có tên, huy hiệu, lệnh · phím; Enter → chú thích.
        tips_->setDelays(1ms, 1h);
        const auto popup = [&] {
            menu.popup(window_->mapToGlobal(QPoint(40, 40)));
            return spinUntil([&] { return menu.windowHandle() && menu.windowHandle()->isExposed(); });
        };
        const auto hover = [&](QPoint nudge) {
            QTest::mouseMove(&menu, menu.actionGeometry(save).center() + nudge);
            return spinUntil([&] { return tips_->level() == 1 && tips_->target() == save; });
        };
        QVERIFY(popup());
        QVERIFY(hover({}));
        QCOMPARE(tipLines(), (QStringList{ui::RichTooltip::name(save), u"QSAVE · Ctrl+S"_s})); // i18n-ok: lệnh · phím
        QVERIFY(tipBadgeShown());
        menu.setActiveAction(save);
        QTest::keyClick(&menu, Qt::Key_Return);
        QVERIFY(!menu.isVisible());
        QCOMPARE(take(), QStringList());
        QCOMPARE(opened_.size(), 2);
        QCOMPARE(toasts_->toasts().size(), 1);
        QVERIFY(clearToasts());

        // Giấy phép Pro được kích hoạt khi menu và tooltip đang mở: không dựng lại gì, khóa gỡ ngay tại chỗ.
        QVERIFY(popup());
        QVERIFY(hover({1, 0})); // Qt bỏ sự kiện di chuột không đổi vị trí
        QVERIFY(tipBadgeShown());
        gate_->setEdition(Edition::Pro);
        QCOMPARE(profiles.count(), 1); // Viewer → đầy đủ (ribbon, workspace của UIA-001 theo đó)
        QCOMPARE(tips_->level(), 1);
        QVERIFY(!tipBadgeShown());
        QCOMPARE(item(save)->text(QAccessible::Description), QString());
        QCOMPARE(reg_->action(u"QSAVE"_s), save);
        menu.setActiveAction(save);
        QTest::keyClick(&menu, Qt::Key_Return);
        QCOMPARE(take(), QStringList{u"^C^C_QSAVE"_s});
        QVERIFY(focusOn(cmd_));
        QTest::keyClick(cmd_, Qt::Key_S, Qt::ControlModifier);
        QCOMPARE(take(), QStringList{u"^C^C_QSAVE"_s});
        QCOMPARE(opened_.size(), 2);
        QVERIFY(toasts_->toasts().isEmpty());
    }

    // Kịch bản 2 — NT-013 + UI0-112 + UI0-110 + UI0-113 + UI0-108 + UI0-009 trên một registry: việc A theo dõi trong
    // hộp thoại modal (InlineProgress ở chân, mở qua DialogHost) và trong danh sách Tác vụ nền (VirtualTableView trên
    // ProgressItemModel, palette ghim tạm). Qua 60 ms: con trỏ bận; qua 250 ms: thanh và dòng cùng hiện. Việc B lỗi khi
    // hộp thoại đang mở: toast lỗi ở lại, mục chưa đọc, focus và cửa sổ active không đổi. Bấm Hủy trên thanh: dòng đang
    // chọn trong bảng biến mất không lỗi, thanh ẩn, không toast. Việc C xong sau mốc: toast tự ẩn, mục vẫn chưa đọc.
    // Hết việc: trả con trỏ. Unit test của UI0-112, UI0-009 dùng widget rời, từng phần một.
    void jobsAcrossDialogPaletteToasts()
    {
        QVERIFY(clearToasts());
        host_->setPinned(u"tasks"_s, true); // về dock bên phải, mở: bảng dựng ở lần hiện đầu
        ui::VirtualTableView* list = taskList();
        QVERIFY(list && list->isVisible());
        QCOMPARE(list->model(), presenter_->model());

        QPointer<SampleDialog> dlg = static_cast<SampleDialog*>(dialogs_->open(u"plot"_s));
        QVERIFY(dlg && dlg->isModal());
        QVERIFY(spinUntil([&] { return dlg->isActiveWindow(); }));
        QWidget* focused = QApplication::focusWidget();
        QVERIFY(focused && dlg->isAncestorOf(focused));

        std::atomic<bool> stop{false};
        const jobs::JobId a = jobs_->start(tx(kJobPlot), u"plot"_s, 0,
                                           [&stop](jobs::Progress& p, sched::CancelToken c) -> trion::Expected<void> {
                                               p.set(0.42);
                                               while (!c.cancelled() && !stop)
                                                   std::this_thread::sleep_for(1ms);
                                               return {};
                                           });
        const auto stopJobs = qScopeGuard([&] { // QVERIFY trượt: việc dừng trước khi stop hết hạn
            stop = true;
            sched::waitIdle();
        });
        auto* progress = new ui::InlineProgress(*presenter_, a, dlg);
        dlg->footer()->insertWidget(0, progress);
        QVERIFY(spinUntil([&] { return presenter_->busy(); }));
        QVERIFY(QGuiApplication::overrideCursor());
        QCOMPARE(QGuiApplication::overrideCursor()->shape(), Qt::BusyCursor);
        QVERIFY(spinUntil([&] { return progress->isVisible() && list->model()->rowCount() == 1; }));
        QCOMPARE(progress->current(), a);
        const QModelIndex row = list->model()->index(0, 0);
        QCOMPARE(row.data(ui::ProgressItemModel::JobIdRole).toULongLong(), quint64(a));
        QCOMPARE(row.data(ui::ProgressItemModel::PercentRole).toInt(), 42);
        QCOMPARE(row.data().toString(), ui::ProgressPresenter::label(tx(kJobPlot), 42));
        list->setCurrentIndex(row);
        QVERIFY(list->visualRect(row).intersects(list->viewport()->rect()));

        const int unread = notes_->unreadCount();
        jobs_->start(tx(kJobOpen), u"open"_s, 0, [](jobs::Progress&, sched::CancelToken) -> trion::Expected<void> {
            return trion::Error{"dwg", "corrupt", kCorrupt};
        });
        QVERIFY(spinUntil([&] { return toastOf(ui::Severity::Error) != nullptr; }));
        QVERIFY(toastOf(ui::Severity::Error)->text().contains(QString::fromUtf8(kCorrupt)));
        QVERIFY(!toastOf(ui::Severity::Error)->counting()); // lỗi ở lại tới khi đóng
        QCOMPARE(notes_->unreadCount(), unread + 1);
        QCOMPARE(QApplication::activeWindow(), dlg.data());
        QCOMPARE(QApplication::focusWidget(), focused);

        auto* cancel = progress->findChild<ui::Button*>();
        QVERIFY(cancel);
        QTest::mouseClick(cancel, Qt::LeftButton);
        QVERIFY(!cancel->isEnabled());
        QVERIFY(presenter_->cancelling(a));
        QVERIFY(spinUntil([&] { return list->model()->rowCount() == 0; }));
        QVERIFY(!list->currentIndex().isValid());
        QVERIFY(spinUntil([&] { return !progress->isVisible(); }));
        QCOMPARE(toasts_->toasts().size(), 1); // hủy: không toast

        toasts_->setTimeout(300ms); // cleanup() trả lại
        jobs_->start(tx(kJobExport), u"export"_s, 0,
                     [&stop](jobs::Progress& p, sched::CancelToken c) -> trion::Expected<void> {
                         const auto end = std::chrono::steady_clock::now() + 500ms; // quá mốc hiện tiến trình
                         while (std::chrono::steady_clock::now() < end && !c.cancelled() && !stop)
                             std::this_thread::sleep_for(1ms);
                         p.set(1);
                         return {};
                     });
        QVERIFY(spinUntil([&] { return toastOf(ui::Severity::Ok) != nullptr; }));
        QVERIFY(toastOf(ui::Severity::Ok)->counting());
        QCOMPARE(notes_->unreadCount(), unread + 2);
        QVERIFY(spinUntil([&] { return toastOf(ui::Severity::Ok) == nullptr; })); // tự ẩn
        QCOMPARE(notes_->unreadCount(), unread + 2);                              // mục vẫn chưa đọc
        QVERIFY(toastOf(ui::Severity::Error));
        QVERIFY(spinUntil([&] { return jobs_->size() == 0 && !presenter_->busy(); }));
        QCOMPARE(QGuiApplication::overrideCursor(), nullptr);

        dlg->reject();
        QVERIFY(spinUntil([&] { return !dlg; }));
        QVERIFY(clearToasts());
        host_->setPinned(u"tasks"_s, false); // về dải tự ẩn như mặc định của màn hình Narrow
        QVERIFY(strip() && strip()->contains(host_->dock(u"tasks"_s)));
    }

    // Kịch bản 3 — UI0-007 ô số trong hộp thoại modal (UI0-110 DialogHost) + UI0-015 phím Toàn cục + chỗ nối INT-003.
    // Hộp thoại modal chặn phím Toàn cục của cửa sổ chính (F8 không tới INT-002). Gõ "12,5" Enter: câu lỗi dưới ô (hộp
    // thoại cao thêm một dòng), trình đọc màn hình đọc được, Enter không tới nút xác nhận. Bấm ⌖ khi ô còn lỗi: hộp
    // thoại ẩn mà không đóng, cửa sổ chính nhận phím: F8 (ORTHO, lệnh trong suốt) tới INT-002 giữa lúc lấy điểm; test
    // đóng vai INT-003 finishPick → giá trị nhận, hết lỗi, hộp thoại hiện lại đúng chỗ, lại modal (F8 lại bị chặn),
    // focus về ô. Gõ =1200/2+50 Enter: ô nhận 650 trước, nút xác nhận đọc 650, hộp thoại đóng, DialogHost lưu chỗ. Unit
    // test của UI0-007 dùng hộp thoại rời không có ActionRegistry, ô không lỗi khi bấm ⌖.
    void numericPickFromModalDialog()
    {
        gate_->setEdition(Edition::Pro);
        sysvars_->set(u"ORTHOMODE"_s, 0);
        double applied = -1;
        QPointer<SampleDialog> dlg = static_cast<SampleDialog*>(dialogs_->open(u"offset"_s));
        QVERIFY(dlg && dlg->isModal());
        dlg->out = &applied;
        QVERIFY(spinUntil([&] { return dlg->isActiveWindow(); }));
        ui::NumericField* field = dlg->field;
        auto* edit = field->findChild<QLineEdit*>();
        QVERIFY(edit);
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == edit; })); // focus đầu: ô nhập của thân
        int picks = 0;
        connect(field, &ui::NumericField::pickRequested, dlg, [&picks] { ++picks; }); // INT-003 giả
        QSignalSpy committed(field, &ui::NumericField::valueCommitted);
        take();

        QTest::keyClick(edit, Qt::Key_F8);
        QCOMPARE(take(), QStringList()); // modal: phím Toàn cục không chạy

        edit->selectAll();
        QTest::keyClicks(edit, u"12,5"_s);
        QTest::keyClick(edit, Qt::Key_Return);
        QVERIFY(field->hasError());
        QVERIFY2(field->errorMessage().contains(u"12.5"_s), qPrintable(field->errorMessage()));
        QCOMPARE(edit->accessibleDescription(), field->errorMessage());
        QCOMPARE(committed.count(), 0);
        QVERIFY(dlg->isVisible());
        QCOMPARE(applied, -1.0);
        spinFor(50ms); // bố cục lại: dòng lỗi đã hiện
        const QRect card = dlg->cardGeometry();

        auto* pick = field->findChild<QAbstractButton*>(u"pick"_s);
        QVERIFY(pick);
        QTest::mouseClick(pick, Qt::LeftButton);
        QCOMPARE(picks, 1);
        QVERIFY(field->isPicking());
        QVERIFY(!dlg->isVisible() && dlg->isHiddenForPick());
        QCOMPARE(dialogs_->find(u"offset"_s), dlg.data()); // ẩn tạm, không đóng
        QVERIFY(!QApplication::activeModalWidget());
        QVERIFY(spinUntil([&] { return window_->isActiveWindow(); }));
        QVERIFY(focusOn(canvas_));
        QTest::keyClick(canvas_, Qt::Key_F8);
        QCOMPARE(take(), QStringList{u"'ORTHOMODE 1"_s});

        field->finishPick(321.5); // INT-003 trả khoảng cách 2 điểm
        QVERIFY(dlg->isVisible() && !dlg->isHiddenForPick());
        QCOMPARE(committed.count(), 1);
        QCOMPARE(committed[0][0].toDouble(), 321.5);
        QCOMPARE(edit->text(), u"321.50"_s);
        QVERIFY(!field->hasError());
        QCOMPARE(edit->accessibleDescription(), QString());
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == edit; }));
        QCOMPARE(QApplication::activeModalWidget(), dlg.data());
        spinFor(50ms);
        // Hộp thoại chưa từng move()/resize() tường minh (DialogHost mở lần đầu) mà cỡ đã đổi lúc đang mở (dòng lỗi):
        // hiện lại sau ⌖ vẫn đúng chỗ (test này từng bắt lỗi UI0-110: show() co theo sizeHint và canh giữa lại).
        QCOMPARE(dlg->cardGeometry().topLeft(), card.topLeft()); // đúng chỗ (cỡ theo nội dung)
        QTest::keyClick(edit, Qt::Key_F8);
        QCOMPARE(take(), QStringList()); // lại modal

        edit->selectAll();
        QTest::keyClicks(edit, u"=1200/2+50"_s);
        QTest::keyClick(edit, Qt::Key_Return);
        QCOMPARE(committed.count(), 2);
        QCOMPARE(committed[1][0].toDouble(), 650.0);
        QCOMPARE(applied, 650.0); // Enter: ô nhận trước, nút xác nhận đọc giá trị mới
        QVERIFY(spinUntil([&] { return !dlg; }));
        QVERIFY(!dialogs_->find(u"offset"_s));
        QVERIFY(!store_->value(u"dialog/offset"_s).isUndefined());
        QCOMPARE(take(), QStringList());
    }

    // Kịch bản 4 — UI0-008 xác nhận, "Không hỏi lại", hộp thoại lỗi + UI0-110 DialogHost + NT-010 Store + NT-013. Xác
    // nhận phá hủy xếp chồng trên hộp thoại modal của DialogHost: Esc chỉ đóng xác nhận, hộp thoại dưới vẫn là modal
    // đang active, focus về nút đã mở xác nhận; "Không hỏi lại" + Enter nhớ kết quả. Khởi động lại (Store mới từ cùng
    // tệp, DialogHost mới): hộp thoại về đúng chỗ đã lưu, xác nhận không hiện mà trả kết quả nhớ, danh sách Tùy chọn có
    // mục đó. Việc nền lỗi → hộp thoại lỗi (nơi nhận kết quả đóng vai UIA-015) mang mã, thông điệp của lỗi; "Sao chép
    // chi tiết" chép đủ. Unit test của UI0-008 dựng xác nhận trên cửa sổ trống, không qua DialogHost hay việc nền.
    void confirmOverDialogAcrossRestart()
    {
        const QString path = dir_.filePath(u"session.json"_s);
        QList<bool> answers;
        QPoint saved;
        // Nút "Xóa layout đã chọn" hỏi xác nhận chồng lên chính hộp thoại; bấm bằng phím Space như người dùng.
        const auto wire = [&answers](SampleDialog* d) {
            connect(d->action, &QAbstractButton::clicked, d,
                    [&answers, d] { confirmDelete().ask(d, [&answers](bool yes) { answers << yes; }); });
        };
        const auto pressAction = [](SampleDialog* d) {
            if (!focusOn(d->action))
                return false;
            QTest::keyClick(d->action, Qt::Key_Space);
            return true;
        };
        {
            Store session(path);
            Store::setInstance(&session); // ConfirmDialog đọc, ghi "Không hỏi lại" ở kho của tiến trình
            ui::DialogHost host(window_.get(), session);
            host.add(u"layouts"_s, &makeLayouts);
            QPointer<SampleDialog> dlg = static_cast<SampleDialog*>(host.open(u"layouts"_s));
            QVERIFY(dlg);
            QVERIFY(spinUntil([&] { return dlg->isActiveWindow(); }));
            saved = dlg->cardGeometry().topLeft() + QPoint(37, -23);
            dlg->setCardGeometry(QRect(saved, dlg->cardGeometry().size()));
            wire(dlg);

            QVERIFY(pressAction(dlg));
            QPointer<QWidget> confirm = QApplication::activeModalWidget();
            QVERIFY(confirm && confirm != dlg);
            QVERIFY(spinUntil([&] { return confirm->isActiveWindow(); }));
            QVERIFY(keyToFocus(Qt::Key_Escape));
            QVERIFY(spinUntil([&] { return !confirm; }));
            QCOMPARE(answers, QList<bool>{false});
            QVERIFY(dlg->isVisible());
            QCOMPARE(QApplication::activeModalWidget(), dlg.data());
            QVERIFY(spinUntil([&] { return QApplication::focusWidget() == dlg->action; }));

            QVERIFY(pressAction(dlg));
            confirm = QApplication::activeModalWidget();
            QVERIFY(confirm && confirm != dlg);
            QVERIFY(spinUntil([&] { return confirm->isActiveWindow(); }));
            auto* again = confirm->findChild<QAbstractButton*>(u"dontAsk"_s);
            QVERIFY(again);
            again->setChecked(true);
            QVERIFY(keyToFocus(Qt::Key_Return)); // focus đầu: nút chính
            QVERIFY(spinUntil([&] { return !confirm; }));
            QCOMPARE(answers, (QList<bool>{false, true}));

            QVERIFY(pressAction(dlg)); // đã nhớ: không hiện, trả kết quả ngay
            QCOMPARE(answers, (QList<bool>{false, true, true}));
            QCOMPARE(QApplication::activeModalWidget(), dlg.data());
            dlg->reject();
            QVERIFY(spinUntil([&] { return !dlg; }));
        } // ~DialogHost, ~Store ghi nốt (như thoát ứng dụng)
        sched::waitIdle(); // thoát xong: lần ghi nền cuối của Store đã xuống đĩa (Application chờ việc nền khi thoát)
        {
            Store session(path); // khởi động lại
            Store::setInstance(&session);
            ui::DialogHost host(window_.get(), session);
            host.add(u"layouts"_s, &makeLayouts);
            QPointer<SampleDialog> dlg = static_cast<SampleDialog*>(host.open(u"layouts"_s));
            QVERIFY(dlg);
            QVERIFY(spinUntil([&] { return dlg->isActiveWindow(); }));
            QCOMPARE(dlg->cardGeometry().topLeft(), saved);
            wire(dlg);
            QVERIFY(pressAction(dlg));
            QCOMPARE(answers, (QList<bool>{false, true, true, true}));
            QCOMPARE(QApplication::activeModalWidget(), dlg.data());
            ui::SuppressedPrompts hidden(session); // trang Tùy chọn (UID-019)
            QCOMPARE(hidden.rowCount(), 1);
            QCOMPARE(hidden.data(hidden.index(0, 0), ui::SuppressedPrompts::IdRole).toString(), u"layout.delete"_s);
            dlg->reject();
            QVERIFY(spinUntil([&] { return !dlg; }));
        }
        Store::setInstance(store_.get());

        // NT-013 → UI0-008: việc mở bản vẽ lỗi; nơi nhận kết quả dựng hộp thoại lỗi từ JobInfo::error.
        ui::ErrorDialog::setLogSource([](int) { return QStringList{QString::fromUtf8(kLogLine)}; });
        QPointer<ui::ErrorDialog> error;
        QObject scope;
        connect(presenter_.get(), &ui::ProgressPresenter::finished, &scope, [&](const jobs::JobInfo& j) {
            if (j.state == jobs::State::Failed && j.kind == u"open"_s)
                (error = new ui::ErrorDialog(tx(kOpenFailed), tx(kOpenFailedWhy), j.error, window_.get()))->open();
        });
        jobs_->start(tx(kJobOpen), u"open"_s, 0, [](jobs::Progress&, sched::CancelToken) -> trion::Expected<void> {
            return trion::Error{"dwg", "corrupt", kCorrupt};
        });
        QVERIFY(spinUntil([&] { return error && error->isActiveWindow(); }));
        QVERIFY(error->details().contains(u"dwg.corrupt"_s));
        auto* copy = error->findChild<ui::Button*>(u"copy"_s);
        QVERIFY(copy);
        QTest::mouseClick(copy, Qt::LeftButton);
        const QString clip = QGuiApplication::clipboard()->text();
        QCOMPARE(clip, error->details());
        QVERIFY(clip.contains(QString::fromUtf8(kCorrupt)) && clip.contains(QString::fromUtf8(kLogLine)));
        QVERIFY(toastOf(ui::Severity::Error)); // followJobs: toast lỗi cùng lúc
        QVERIFY(keyToFocus(Qt::Key_Escape));
        QVERIFY(spinUntil([&] { return !error; }));
        ui::ErrorDialog::setLogSource({});
        QVERIFY(clearToasts());
    }

    // Kịch bản 5 — UI0-010 dải tự ẩn (màn hình Narrow) + UIA-106 dòng lệnh + UI0-015 phím Vùng vẽ + UI0-108 bảng trong
    // palette. Rê tab: palette trượt ra PHỦ vùng vẽ (vùng vẽ không đổi chỗ, cỡ), dòng lệnh giữ focus và chữ gõ tiếp
    // (chờ lệnh: chữ hoa); Delete ở dòng lệnh là xóa chữ, không ERASE; Esc thu khung mà dòng lệnh không nhận Esc. Vùng
    // vẽ có focus: Delete là ERASE, cả khi khung đang trượt ra (rê không lấy focus). Bấm vào bảng trong khung: focus
    // vào palette, Delete không tới INT-002 (khung không thuộc vùng vẽ); Esc thu khung, focus về vùng vẽ, Delete lại là
    // ERASE. Unit test của UI0-010 dùng QLineEdit giả, không có ActionRegistry; của UI0-015 không có khung trượt.
    void autoHidePaletteKeepsCommandLine()
    {
        gate_->setEdition(Edition::Pro); // ERASE (Pro) tới INT-002 khi focus ở vùng vẽ
        ui::AutoHideStrip* s = strip();
        QDockWidget* dock = host_->dock(u"tasks"_s);
        QVERIFY(s && s->contains(dock));
        QVERIFY(!host_->isPinned(u"tasks"_s));
        QAbstractButton* tab = s->tab(dock);
        QVERIFY(tab && tab->isVisible());
        const QRect canvas = canvas_->geometry();
        take();

        mouseTo(centerOf(canvas_));
        QVERIFY(focusOn(cmd_));
        cmd_->clear();
        QTest::keyClicks(cmd_, u"li"_s);
        QCOMPARE(cmd_->text(), u"LI"_s);
        mouseTo(centerOf(tab)); // rê 300 ms → trượt ra
        QVERIFY(spinUntil([&] { return opened(s); }));
        QCOMPARE(canvas_->geometry(), canvas); // phủ lên, không đẩy vùng vẽ
        QVERIFY(s->flyout()->geometry().intersects(canvas));
        QVERIFY(cmd_->hasFocus());
        QTest::keyClicks(cmd_, u"ne"_s);
        QCOMPARE(cmd_->text(), u"LINE"_s);
        QTest::keyClick(cmd_, Qt::Key_Home);
        QTest::keyClick(cmd_, Qt::Key_Delete);
        QCOMPARE(cmd_->text(), u"INE"_s);
        QCOMPARE(take(), QStringList());
        EscSpy esc(cmd_);
        QTest::keyClick(cmd_, Qt::Key_Escape);
        QVERIFY(!s->current());
        QCOMPARE(esc.count, 0);
        QVERIFY(cmd_->hasFocus());
        QCOMPARE(cmd_->text(), u"INE"_s);

        mouseTo(centerOf(canvas_));
        QVERIFY(focusOn(canvas_));
        QTest::keyClick(canvas_, Qt::Key_Delete);
        QCOMPARE(take(), QStringList{u"^C^C_ERASE"_s});
        mouseTo(centerOf(tab));
        QVERIFY(spinUntil([&] { return opened(s); }));
        QVERIFY(canvas_->hasFocus());
        QTest::keyClick(canvas_, Qt::Key_Delete);
        QCOMPARE(take(), QStringList{u"^C^C_ERASE"_s});

        ui::VirtualTableView* list = taskList();
        QVERIFY(list && s->flyout()->isAncestorOf(list));
        mouseTo(centerOf(list));
        QTest::mouseClick(list->viewport(), Qt::LeftButton);
        QVERIFY(spinUntil([&] { return list->hasFocus(); }));
        QVERIFY(s->current());
        QTest::keyClick(list, Qt::Key_Delete);
        QCOMPARE(take(), QStringList());
        QTest::keyClick(list, Qt::Key_Escape);
        QVERIFY(!s->current());
        QVERIFY(canvas_->hasFocus());
        QTest::keyClick(canvas_, Qt::Key_Delete);
        QCOMPARE(take(), QStringList{u"^C^C_ERASE"_s});
        mouseTo(centerOf(canvas_));
        cmd_->clear();
    }

    // Kịch bản 6 — đổi ngôn ngữ (UI0-104, UI0-005) khi khung đang mở đủ thứ: menu (UI0-107) đang mở có mục khóa,
    // hộp thoại modeless (UI0-110) có ô số (UI0-007), palette trượt ra từ dải (UI0-010) có bảng ảo (UI0-108), toast
    // (UI0-009). vi → en → vi-XL → vi: chữ QAction (UI0-015), nút của khung hộp thoại, tiêu đề palette đổi ngay;
    // TruncationProbe không thấy chữ bị cắt ở cửa sổ chính (cả menu, dải, khung trượt, toast) và hộp thoại. Unit test
    // đổi ngôn ngữ từng thành phần riêng.
    void languageSwitchKeepsFrameReadable()
    {
        gate_->setEdition(Edition::Viewer); // mục khóa có huy hiệu: chữ của mục dài thêm
        ui::Menu menu(window_.get());
        for (const char* id : {"OPEN", "QSAVE", "PLOT", "LINE", "MEASUREGEOM"})
            menu.addAction(reg_->action(QString::fromLatin1(id)));
        QPointer<SampleDialog> dlg = static_cast<SampleDialog*>(dialogs_->open(u"props"_s));
        QVERIFY(dlg && !dlg->isModal());
        QVERIFY(spinUntil([&] { return dlg->isVisible(); }));
        host_->showPalette(u"tasks"_s);
        QVERIFY(spinUntil([&] { return opened(strip()); }));
        menu.popup(window_->mapToGlobal(QPoint(60, 60)));
        QVERIFY(spinUntil([&] { return menu.windowHandle() && menu.windowHandle()->isExposed(); }));

        QAction* save = reg_->action(u"QSAVE"_s);
        QDockWidget* dock = host_->dock(u"tasks"_s);
        const QString viSave = save->text(), viCancel = dlg->cancel->text(), viTasks = dock->windowTitle();
        QCOMPARE(viCancel, u"Hủy"_s); // i18n-ok: chữ tiếng Việt mong đợi
        for (const QString& lang : {u"en"_s, i18n::kPseudoLanguage.toString(), u"vi"_s}) {
            QVERIFY(translator_->setLanguage(lang));
            QCoreApplication::processEvents();              // LanguageChange → retranslate(), rồi bố cục lại
            toasts_->post(ui::Severity::Ok, tx(kExported)); // chữ dịch lúc đăng
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            QCoreApplication::processEvents();
            const QByteArray where = lang.toUtf8();
            QVERIFY2(menu.isVisible() && dlg->isVisible() && opened(strip()), where.constData());
            QCOMPARE(save->text(), QCoreApplication::translate("actions", "Lưu bản vẽ")); // i18n-ok: chuỗi nguồn
            QCOMPARE(dock->windowTitle(), tx(kTasks));
            if (lang == u"vi"_s) {
                QCOMPARE(save->text(), viSave);
                QCOMPARE(dlg->cancel->text(), viCancel);
                QCOMPARE(dock->windowTitle(), viTasks);
            } else {
                QVERIFY2(save->text() != viSave, where.constData());
                QVERIFY2(dlg->cancel->text() != viCancel, where.constData());
            }
            if (lang == i18n::kPseudoLanguage) {
                QVERIFY(save->text().size() > viSave.size());
                QVERIFY(dock->windowTitle().size() > viTasks.size());
            }
            QVERIFY2(i18n::findTruncations(window_.get()).isEmpty(),
                     qPrintable(lang + u": "_s + truncations(window_.get())));
            QVERIFY2(i18n::findTruncations(dlg).isEmpty(), qPrintable(lang + u": "_s + truncations(dlg)));
        }
        menu.hide();
        dlg->reject();
        QVERIFY(spinUntil([&] { return !dlg; }));
        strip()->collapse();
        QVERIFY(clearToasts());
    }

    // Kịch bản 7 — rảnh sau mọi kịch bản trên (hành động, cổng phiên bản, menu, tooltip, hộp thoại, xác nhận, việc nền,
    // toast, palette tự ẩn, đổi ngôn ngữ), focus ở vùng vẽ (focus ở dòng lệnh thì con trỏ chữ nhấp nháy là timer của
    // Qt): executor, watchdog không bị đánh thức, không sự kiện nào. Bộ đếm đọc từ luồng phụ giữa khoảng rảnh (như nhóm
    // 3).
    void idleAfterwards()
    {
        mouseTo(centerOf(canvas_));
        QVERIFY(focusOn(canvas_));
        tips_->leave();
        QVERIFY(spinUntil([&] { return jobs_->size() == 0 && !presenter_->busy() && toasts_->toasts().isEmpty(); }));
        probe::enable();
        sched::waitIdle();
        spinFor(300ms); // việc còn chờ (vẽ, bố cục, raster trước) xong
        const auto wakeups = executor_->stats().wakeups;
        probe::Counters c0, c1;
        std::thread sampler([&] {
            std::this_thread::sleep_for(200ms);
            c0 = probe::counters();
            std::this_thread::sleep_for(1000ms);
            c1 = probe::counters();
        });
        spinFor(1400ms);
        sampler.join();
        QStringList active; // chẩn đoán: timer đang chạy của các phần trong khung
        for (QObject* root : std::initializer_list<QObject*>{window_.get(), tips_.get(), jobs_.get(), presenter_.get()})
            for (const QTimer* t : root->findChildren<QTimer*>())
                if (t->isActive())
                    active << QString::fromLatin1(t->parent()->metaObject()->className());
        QVERIFY2(active.isEmpty(), qPrintable(active.join(u", "_s)));
        QCOMPARE(executor_->stats().wakeups, wakeups);
        QCOMPARE(c1.events, c0.events);
        QCOMPARE(c1.watchdogWakeups, c0.watchdogWakeups);
        probe::disable();
    }
};

QTEST_MAIN(IntegrationKhung)
#include "it_khung_test.moc"
