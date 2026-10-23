// UIA-017: Viewer — ribbon Viewer UI-M01 (RibbonBar::setProfile, ribbon/resources/viewer.json), panel nâng cấp cuối
// ribbon, thanh trạng thái giản lược (StatusBar ở hồ sơ Viewer), chế độ chỉ xem tới chỗ nối INT-002. Tiêu chí:
//   ribbonProfile      5 tab UI-M01 (tab Ghi chú cần VP); locate theo hồ sơ; đổi hồ sơ qua lại giữ mức thu gọn, không
//                      dựng lại trang đã dựng.
//   viewerProUnlocks   tiêu chí 3: mục VP (nút, tab) có huy hiệu khóa ở Viewer, bấm mở trang nâng cấp; setEdition(Viewer
//                      Pro) mở ngay trên chính các widget đó (không dựng lại, không khởi động lại).
//   upsell             panel "Sửa bản vẽ?" chỉ ở hồ sơ Viewer, cuối thân; bấm → trang nâng cấp (handler giả); mức chỉ
//                      tên panel: một hàng, không cắt chữ.
//   editBlocked        tiêu chí 2: mọi lệnh của danh mục (ActionRegistry::commands) có need ≠ V kích hoạt ở Viewer: sink
//                      không nhận, UpgradeRouter nhận (đếm); lệnh V tới sink. "0 thay đổi bản vẽ": Hoãn (cần INT-020).
//   noWrites           tiêu chí 1 tới chỗ nối: bấm mọi nút, mục split của 5 tab ở Viewer và Viewer Pro (xem, đo, in…):
//                      không lệnh ghi bản vẽ (SAVE, QSAVE, SAVEAS, WBLOCK…) và không lệnh ngoài bản đang dùng tới sink.
//                      1.000 tệp QA-001 mở ở Viewer không bị ghi: Hoãn (cần DOC-018, QA-001).
//   statusBar          thanh trạng thái UI-M01: mục hiện, Bắt điểm (khi đo), tỉ lệ chỉ xem (Model/Paper/viewport), bấm
//                      tỉ lệ → toast, không lệnh.
//   pseudoNoTruncation 1366×768 vi-XL: ribbon Viewer mức đầy đủ (mọi tab) và thanh trạng thái 0 chữ cắt;
//                      a11y::checkNames, checkFocus 0 vi phạm.
//   baseline           tiêu chí 4: ảnh ribbon + thanh trạng thái Viewer 1920 px, Tối/Sáng, theo nền tảng (ghi lại:
//                      TRION_UPDATE_BASELINE=<thư mục>).
#include "app/core/a11y/AccessibilityAudit.h"
#include "app/core/actions/ActionRegistry.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/edition/LockBadge.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/keymap/Keymap.h"
#include "app/core/UiExecutor.h"
#include "app/shell/ribbon/controls/RibbonControls.h"
#include "app/shell/ribbon/RibbonBar.h"
#include "app/shell/ribbon/RibbonModel.h"
#include "app/shell/status/ModeToggles.h"
#include "app/shell/status/StatusBar.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/progress/ProgressPresenter.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/jobs/BackgroundJobs.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QTabBar>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using namespace trion;
using edition::Edition;
using edition::UiProfile;
using ribbon::RibbonBar;
using ribbon::RibbonButton;
using ribbon::RibbonModel;
using shell::StatusBar;

namespace {

#if defined(Q_OS_WIN)
constexpr auto kPlatform = "windows";
#elif defined(Q_OS_MACOS)
constexpr auto kPlatform = "macos";
#else
constexpr auto kPlatform = "linux";
#endif
constexpr int kNoise = 8; // baseline: lệch ≤ 8/255 mỗi kênh bỏ qua, điểm lệch hơn được tối đa 0,1% ảnh

// Lệnh ghi bản vẽ ra đĩa (03 Phụ lục A, nhóm Tệp): không bao giờ được tới sink ở Viewer.
const QStringList kWrites = {u"SAVE"_s,    u"QSAVE"_s,    u"SAVEAS"_s,  u"SAVEALL"_s, u"WBLOCK"_s,
                             u"DXFOUT"_s,  u"EXPORT"_s,   u"ETRANSMIT"_s, u"RECOVER"_s, u"AUDIT"_s,
                             u"PURGE"_s,   u"OVERKILL"_s, u"TCMARKUP2DWG"_s};

void settle()
{
    for (int i = 0; i < 3; ++i) {
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    }
}

// "^C^C_LINE", "'_ZOOM", "'LWDISPLAY 1", "^C^C_MEASUREGEOM Q" → tên lệnh/hành động.
QString commandName(QString macro)
{
    for (const QString& p : {u"^C^C"_s, u"'"_s, u"_"_s})
        if (macro.startsWith(p))
            macro.remove(0, p.size());
    return macro.section(u' ', 0, 0);
}

void closePopups()
{
    while (QWidget* p = QApplication::activePopupWidget())
        p->close();
}

} // namespace

class ViewerTest : public QObject {
    Q_OBJECT

    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<theme::ThemeManager> tm_;
    std::unique_ptr<theme::IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<keymap::Keymap> keymap_;
    std::unique_ptr<ui::RichTooltip> tips_;
    std::unique_ptr<actions::Sysvars> sysvars_;
    std::unique_ptr<actions::ActionRegistry> registry_;
    std::unique_ptr<edition::EditionGate> gate_;
    std::unique_ptr<jobs::BackgroundJobs> jobs_;
    std::unique_ptr<ui::ProgressPresenter> progress_;
    std::unique_ptr<ui::NotificationCenter> notes_;
    std::unique_ptr<RibbonModel> model_;
    std::unique_ptr<ribbon::RibbonControls> controls_;
    std::unique_ptr<QWidget> window_;
    std::unique_ptr<ui::ToastHost> toasts_;
    RibbonBar* ribbon_ = nullptr;
    StatusBar* status_ = nullptr;
    QStringList sent_;   // chỗ nối INT-002 (ActionRegistry::setSink)
    QStringList macros_; // macroRequested của RibbonControls, StatusBar
    QList<edition::UpgradeRequest> upgrades_; // trang nâng cấp đã mở (handler giả UI-L04)
    QStringList notices_;                     // UpgradeRouter::noticeRequested (lần sau trong phiên)

    int px(tok::PxId id) const { return qRound(tm_->px(id)); }
    qsizetype upgradeCount() const { return upgrades_.size() + notices_.size(); }

    RibbonButton* button(const QString& id) const
    {
        for (RibbonButton* b : ribbon_->pages()->currentWidget()->findChildren<RibbonButton*>(id))
            if (b->isVisible())
                return b;
        return nullptr;
    }

    // Bấm mọi nút của 5 tab và kích hoạt mọi mục split (menu thật của nút), ở phiên bản hiện tại.
    void clickEverything()
    {
        for (int i = 0; i < int(ribbon_->model().tabs().size()); ++i) {
            ribbon_->setCurrentTab(i);
            settle();
            for (RibbonButton* b : ribbon_->pages()->currentWidget()->findChildren<RibbonButton*>()) {
                if (!b->isVisible())
                    continue;
                b->click();
                closePopups();
                if (QMenu* m = b->menu()) {
                    emit m->aboutToShow();
                    for (QAction* a : m->actions())
                        if (!a->isSeparator() && a->isEnabled() && !a->menu())
                            a->trigger();
                }
                closePopups();
            }
        }
        ribbon_->setCurrentTab(0);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        executor_ = std::make_unique<ui::UiExecutor>();
        QVERIFY(theme::loadFonts());
        tm_ = std::make_unique<theme::ThemeManager>(*store_);
        icons_ = std::make_unique<theme::IconProvider>();
        translator_ = std::make_unique<i18n::Translator>();
        keymap_ = std::make_unique<keymap::Keymap>(*store_, keymap::Platform::Windows);
        tips_ = std::make_unique<ui::RichTooltip>();
        tips_->setKeymap(keymap_.get());
        sysvars_ = std::make_unique<actions::Sysvars>();
        sysvars_->set(u"COORDS"_s, 1);
        registry_ = std::make_unique<actions::ActionRegistry>(*keymap_, *sysvars_);
        registry_->setSink([this](const QString& m) { sent_ << m; });
        gate_ = std::make_unique<edition::EditionGate>();
        gate_->bind(*registry_);
        gate_->upgrade().setHandler([this](const edition::UpgradeRequest& r) { upgrades_ << r; });
        connect(&gate_->upgrade(), &edition::UpgradeRouter::noticeRequested, this,
                [this](const edition::UpgradeRequest& r) { notices_ << r.feature; });
        jobs_ = std::make_unique<jobs::BackgroundJobs>(jobs::Timing{20ms, 50ms, 20ms});
        progress_ = std::make_unique<ui::ProgressPresenter>(*jobs_);
        notes_ = std::make_unique<ui::NotificationCenter>();
        model_ = std::make_unique<RibbonModel>();
        controls_ = std::make_unique<ribbon::RibbonControls>(*registry_, *store_);
        connect(controls_.get(), &ribbon::RibbonControls::macroRequested, this,
                [this](const QString& m) { macros_ << m; });

        // Cửa sổ mẫu như MainWindow: ribbon, vùng vẽ, thanh trạng thái; integration nối hồ sơ như dưới đây.
        window_ = std::make_unique<QWidget>();
        auto* column = new QVBoxLayout(window_.get());
        column->setContentsMargins({});
        column->setSpacing(0);
        ribbon_ = new RibbonBar(*model_, *controls_, *store_, window_.get());
        ribbon_->setProfile(gate_->profile());
        connect(gate_.get(), &edition::EditionGate::profileChanged, ribbon_, &RibbonBar::setProfile);
        auto* canvas = new QWidget(window_.get());
        canvas->setObjectName(u"canvas"_s);
        canvas->setFocusPolicy(Qt::StrongFocus);
        status_ = new StatusBar(*registry_, *sysvars_, *gate_, *progress_, *notes_, *store_, window_.get());
        connect(status_, &StatusBar::macroRequested, this, [this](const QString& m) { macros_ << m; });
        column->addWidget(ribbon_);
        column->addWidget(canvas, 1);
        column->addWidget(status_);
        toasts_ = std::make_unique<ui::ToastHost>(window_.get(), *notes_);
        registry_->install(window_.get());
        window_->resize(1920, 400);
        window_->show();
        QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
    }

    void init()
    {
        gate_->setEdition(Edition::Viewer);
        ribbon_->setCollapse(RibbonBar::Collapse::Full);
        ribbon_->setCurrentTab(0);
        window_->resize(1920, 400);
        window_->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(window_.get()));
        settle();
        sent_.clear();
        macros_.clear();
    }

    void cleanup() { closePopups(); }

    void cleanupTestCase()
    {
        toasts_.reset();
        window_.reset();
        controls_.reset();
        model_.reset();
        notes_.reset();
        progress_.reset();
        jobs_.reset();
        gate_.reset();
        registry_.reset();
        sysvars_.reset();
        tips_.reset();
        keymap_.reset();
        translator_.reset();
        icons_.reset();
        tm_.reset();
        executor_.reset();
        sched::waitIdle();
        settings::Store::setInstance(nullptr);
        store_.reset();
    }

    void ribbonProfile()
    {
        QCOMPARE(ribbon_->profile(), UiProfile::Viewer);
        QStringList ids, needs;
        for (const ribbon::Tab& t : ribbon_->model().tabs()) {
            ids << t.id;
            needs << t.need;
        }
        QCOMPARE(ids, (QStringList{u"viewer-view"_s, u"viewer-measure"_s, u"viewer-markup"_s, u"viewer-output"_s,
                                   u"viewer-tools"_s}));
        QCOMPARE(needs, (QStringList{{}, {}, u"VP"_s, {}, {}})); // tab Ghi chú (VP)
        QCOMPARE(ribbon_->tabBar()->count(), 5);
        QVERIFY(ribbon_->tabBar()->tabText(3).contains(u"&&"_s)); // "In & Xuất": "&" là chữ, không phải phím tắt
        QVERIFY(ribbon_->model().errors().isEmpty());
        const ribbon::Tab& measure = ribbon_->model().tabs()[1];
        QCOMPARE(ribbon_->locate(u"DIST"_s),
                 u"%1 › %2"_s.arg(RibbonModel::line(measure.name), RibbonModel::line(measure.panels[0].name)));
        QVERIFY(ribbon_->locate(u"LINE"_s).isEmpty()); // lệnh sửa không có trên ribbon Viewer

        ribbon_->setCollapse(RibbonBar::Collapse::PanelTitles);
        QWidget* page = ribbon_->pages()->currentWidget();
        gate_->setEdition(Edition::Pro); // profileChanged → ribbon đầy đủ
        settle();
        QCOMPARE(ribbon_->profile(), UiProfile::Full);
        QCOMPARE(ribbon_->tabBar()->count(), int(model_->tabs().size()));
        QCOMPARE(ribbon_->collapse(), RibbonBar::Collapse::PanelTitles);
        QCOMPARE(ribbon_->locate(u"CIRCLE"_s), model_->locate(u"CIRCLE"_s));
        QVERIFY(!ribbon_->locate(u"CIRCLE"_s).isEmpty());
        QVERIFY(!ribbon_->upsell());
        gate_->setEdition(Edition::ViewerPro);
        settle();
        QCOMPARE(ribbon_->tabBar()->count(), 5);
        QCOMPARE(ribbon_->pages()->currentWidget(), page); // trang đã dựng giữ lại
        QCOMPARE(ribbon_->collapse(), RibbonBar::Collapse::PanelTitles);
        QCOMPARE(ribbon_->height(), ribbon_->sizeHint().height());
    }

    void viewerProUnlocks()
    {
        QTabBar* tabs = ribbon_->tabBar();
        auto* badge = qobject_cast<edition::LockBadge*>(tabs->tabButton(2, QTabBar::RightSide));
        QVERIFY(badge && badge->isVisible());
        QVERIFY(!tabs->tabButton(0, QTabBar::RightSide));
        QVERIFY(tabs->accessibleTabName(2).contains(edition::EditionGate::availableIn(ui::Edition::ViewerPro)));
        QVERIFY(i18n::findTruncations(tabs).isEmpty()); // chữ tab không bị huy hiệu đè

        QWidget* page = ribbon_->pages()->currentWidget();
        RibbonButton* count = button(u"COUNT"_s); // Xem › Palette › Đếm (VP)
        QVERIFY(count && count->isLocked());
        QVERIFY(!button(u"PAN"_s)->isLocked());
        const qsizetype before = upgradeCount();
        count->click();
        QVERIFY(sent_.isEmpty());
        QCOMPARE(upgradeCount(), before + 1);

        gate_->setEdition(Edition::ViewerPro); // giấy phép thành VP lúc đang chạy
        settle();
        QCOMPARE(ribbon_->profile(), UiProfile::Viewer);
        QCOMPARE(ribbon_->pages()->currentWidget(), page);
        QCOMPARE(button(u"COUNT"_s), count); // cùng widget: không dựng lại
        QVERIFY(!count->isLocked());
        QVERIFY(!tabs->tabButton(2, QTabBar::RightSide));
        count->click();
        QCOMPARE(sent_, QStringList{registry_->macro(u"COUNT"_s)});
        QCOMPARE(upgradeCount(), before + 1);
        ribbon_->setCurrentTab(2); // Ghi chú: mọi nút mở
        settle();
        for (RibbonButton* b : ribbon_->pages()->currentWidget()->findChildren<RibbonButton*>())
            QVERIFY2(!b->isLocked(), qPrintable(b->objectName()));
        gate_->setEdition(Edition::Viewer);
        settle();
        QVERIFY(tabs->tabButton(2, QTabBar::RightSide));
        QVERIFY(ribbon_->pages()->currentWidget()->findChild<RibbonButton*>(u"mk-cloud"_s)->isLocked());
    }

    void upsell()
    {
        QWidget* up = ribbon_->upsell();
        QVERIFY(up && up->isVisible());
        const QRect r(up->mapTo(ribbon_, QPoint()), up->size()); // cuối thân (.rb-up margin-left auto)
        QCOMPARE(r.right(), ribbon_->width() - 1 - px(tok::sp_1));
        for (ribbon::RibbonPanel* p : ribbon_->panels())
            if (p->isVisible())
                QVERIFY(QRect(p->mapTo(ribbon_, QPoint()), p->size()).right() < r.left());
        auto* trial = up->findChild<QPushButton*>(u"trial"_s);
        QVERIFY(trial && trial->isVisible() && !trial->text().isEmpty());
        trial->click();
        QVERIFY(!upgrades_.isEmpty());
        QCOMPARE(upgrades_.last().feature, u"trial"_s);
        QCOMPARE(upgrades_.last().need, ui::Edition::Pro);
        QVERIFY(sent_.isEmpty() && macros_.isEmpty());

        ribbon_->setCollapse(RibbonBar::Collapse::PanelTitles); // một hàng: tên + nút, câu phụ ẩn
        settle();
        QVERIFY(up->isVisible());
        QVERIFY(!up->findChild<QLabel*>(u"upsellText"_s)->isVisible());
        for (QWidget* w : {static_cast<QWidget*>(up->findChild<QLabel*>(u"upsellTitle"_s)), static_cast<QWidget*>(trial)})
            QVERIFY(up->rect().contains(QRect(w->mapTo(up, QPoint()), w->size())));
        QVERIFY(i18n::findTruncations(ribbon_).isEmpty());
        ribbon_->setCollapse(RibbonBar::Collapse::Full);
        settle();
        QVERIFY(up->findChild<QLabel*>(u"upsellText"_s)->isVisible());
        QVERIFY(i18n::findTruncations(ribbon_).isEmpty());
    }

    void editBlocked()
    {
        int blocked = 0, view = 0;
        for (const actions::CommandInfo& c : actions::commands()) {
            const QString name = QString::fromLatin1(c.name);
            QAction* a = registry_->action(name);
            QVERIFY2(a, c.name);
            const qsizetype before = upgradeCount();
            sent_.clear();
            a->trigger();
            if (qstrcmp(c.need, "V") == 0) {
                QVERIFY2(sent_.size() == 1 && commandName(sent_.front()) == name, c.name);
                QCOMPARE(upgradeCount(), before);
                ++view;
            } else {
                QVERIFY2(sent_.isEmpty(), c.name);           // không tới INT-002
                QVERIFY2(upgradeCount() == before + 1, c.name); // thông báo nâng cấp
                ++blocked;
            }
        }
        qInfo("Viewer: %d lệnh cần Viewer Pro/Pro bị chặn kèm thông báo nâng cấp, %d lệnh xem tới sink", blocked, view);
        QVERIFY(blocked > 0 && view > 0);
    }

    void noWrites_data()
    {
        QTest::addColumn<Edition>("edition");
        QTest::addRow("viewer") << Edition::Viewer;
        QTest::addRow("viewer-pro") << Edition::ViewerPro;
    }

    void noWrites()
    {
        QFETCH(Edition, edition);
        gate_->setEdition(edition);
        settle();
        clickEverything();
        QStringList all = sent_ + macros_;
        QVERIFY(!all.isEmpty());
        for (const QString& m : std::as_const(all)) {
            const QString name = commandName(m);
            QVERIFY2(!kWrites.contains(name), qPrintable(m));
            QVERIFY2(!gate_->locked(registry_->need(name)), qPrintable(m)); // chỉ lệnh của bản đang dùng
            QVERIFY2(registry_->need(name) != ui::Edition::Pro, qPrintable(m)); // không lệnh sửa bản vẽ
        }
        qInfo("%s: %lld lệnh xem, đo, in tới sink; 0 lệnh ghi", QTest::currentDataTag(), qint64(all.size()));
    }

    void statusBar()
    {
        status_->setSpace(StatusBar::Space::Model);
        settle();
        QStringList shown;
        for (QWidget* w : status_->findChildren<QWidget*>(Qt::FindDirectChildrenOnly))
            if (w->isVisible() && !w->objectName().isEmpty())
                shown << w->objectName();
        for (QToolButton* b : status_->findChild<shell::ModeToggles*>()->findChildren<QToolButton*>(
                 Qt::FindDirectChildrenOnly))
            if (b->isVisible())
                shown << b->defaultAction()->objectName();
        shown.sort();
        QStringList want{u"coord"_s,         u"space"_s,   u"scale"_s,       u"graphics"_s,  u"tasks"_s,
                         u"notifications"_s, u"edition"_s, u"cleanScreen"_s, u"customize"_s, u"osnap"_s,
                         u"LWDISPLAY"_s};
        want.sort();
        QCOMPARE(shown, want); // UI-M01: không Lưới, Ortho…, không độ trong suốt
        const auto* osnap = status_->findChild<shell::ModeToggles*>()->button(u"osnap"_s);
        QCOMPARE(osnap->accessibleName(),
                 QCoreApplication::translate("trion::shell::ModeToggles", "Bắt điểm (khi đo)")); // i18n-ok: nguồn

        auto* scale = status_->findChild<QToolButton*>(u"scale"_s);
        auto* modes = status_->findChild<shell::ModeToggles*>();
        QVERIFY(scale->x() > modes->x() + modes->width()); // | dải nút | tỉ lệ
        QCOMPARE(scale->text(), u"1:1"_s);
        status_->setAnnotationScale(u"1:100"_s);
        QCOMPARE(scale->text(), u"1:100"_s);
        status_->setSpace(StatusBar::Space::Paper);
        QCOMPARE(scale->text(), u"Paper"_s);
        status_->setViewportScale(u"1:50"_s);
        status_->setSpace(StatusBar::Space::ModelInViewport);
        QCOMPARE(scale->text(), u"1:50"_s);
        QVERIFY(scale->accessibleName().contains(u"1:50"_s));
        QTest::mouseClick(scale, Qt::LeftButton); // chỉ xem: toast, không lệnh
        QVERIFY(sent_.isEmpty() && macros_.isEmpty());
        QCOMPARE(toasts_->toasts().size(), 1);
        QCOMPARE(toasts_->toasts().front()->text(),
                 QCoreApplication::translate("trion::shell::StatusBar",
                                             "Viewer chỉ xem tỉ lệ. Đổi tỉ lệ có trong TrionCAD Pro.")); // i18n-ok
        toasts_->toasts().front()->closeButton()->click();
        status_->setSpace(StatusBar::Space::Model);
        status_->setViewportScale({});
        status_->setAnnotationScale(u"1:1"_s);
    }

    void pseudoNoTruncation()
    {
        window_->resize(1366, 768);
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        settle();
        QStringList found;
        for (int i = 0; i < int(ribbon_->model().tabs().size()); ++i) {
            ribbon_->setCurrentTab(i);
            settle();
            QCOMPARE(ribbon_->collapse(), RibbonBar::Collapse::Full);
            for (const i18n::Truncation& t : i18n::findTruncations(window_.get()))
                found << u"tab %1: %2"_s.arg(i).arg(t.toString());
        }
        QVERIFY2(status_->overflowed().isEmpty(), qPrintable(status_->overflowed().join(u", "_s)));
        QCOMPARE(a11y::checkNames(ribbon_), QStringList());
        QCOMPARE(a11y::checkNames(status_), QStringList());
        QWidget* canvas = window_->findChild<QWidget*>(u"canvas"_s);
        canvas->setFocusPolicy(Qt::NoFocus); // vùng vẽ mẫu không có viền focus: chỉ đi vòng Tab của ribbon, thanh
        const QStringList focus = a11y::checkFocus(window_.get());
        canvas->setFocusPolicy(Qt::StrongFocus);
        QVERIFY2(focus.isEmpty(), qPrintable(focus.join(u'\n')));
        QVERIFY(translator_->setLanguage(u"vi"_s));
        settle();
        QVERIFY2(found.isEmpty(), qPrintable(found.join(u'\n')));
    }

    void baseline_data()
    {
        QTest::addColumn<bool>("light");
        QTest::addRow("dark") << false;
        QTest::addRow("light") << true;
    }

    void baseline()
    {
        QFETCH(bool, light);
        tm_->setTheme(light ? theme::Theme::Light : theme::Theme::Dark);
        QCoreApplication::processEvents(); // Qt 6.11 gửi ApplicationPaletteChange qua hàng đợi
        status_->setAnnotationScale(u"1:100"_s);
        QTest::mouseMove(window_.get(), QPoint(window_->width() / 2, window_->height() / 2)); // không rê nút nào
        settle();
        const int pad = 10;
        QImage sheet(QSize(ribbon_->width(), ribbon_->height() + status_->height() + pad),
                     QImage::Format_ARGB32_Premultiplied);
        sheet.fill(tm_->color(tok::c_canvas));
        QPainter p(&sheet);
        ribbon_->render(&p, QPoint());
        status_->render(&p, QPoint(0, ribbon_->height() + pad));
        p.end();
        status_->setAnnotationScale(u"1:1"_s);
        tm_->setTheme(theme::Theme::Dark);

        const QString name = u"viewer-%1-%2.png"_s.arg(light ? u"light"_s : u"dark"_s, QLatin1StringView(kPlatform));
        const QString out = qEnvironmentVariable("TRION_UPDATE_BASELINE");
        if (!out.isEmpty()) {
            QVERIFY2(sheet.save(out + u'/' + name, "PNG", 0), qPrintable(out));
            QSKIP("đã ghi baseline");
        }
        const QImage want = QImage(u":/baseline/"_s + name).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QVERIFY2(!want.isNull(), qPrintable(u"thiếu baseline "_s + name));
        QCOMPARE(sheet.size(), want.size());
        qsizetype off = 0;
        int maxDiff = 0;
        for (int y = 0; y < sheet.height(); ++y) {
            const auto* a = reinterpret_cast<const QRgb*>(sheet.constScanLine(y));
            const auto* b = reinterpret_cast<const QRgb*>(want.constScanLine(y));
            for (int x = 0; x < sheet.width(); ++x) {
                const int d = std::max({std::abs(qRed(a[x]) - qRed(b[x])), std::abs(qGreen(a[x]) - qGreen(b[x])),
                                        std::abs(qBlue(a[x]) - qBlue(b[x])), std::abs(qAlpha(a[x]) - qAlpha(b[x]))});
                maxDiff = std::max(maxDiff, d);
                off += d > kNoise;
            }
        }
        qInfo("%s: %lld điểm ảnh lệch > %d/255, lệch lớn nhất %d/255", qPrintable(name), qint64(off), kNoise, maxDiff);
        if (off > sheet.width() * sheet.height() / 1000)
            sheet.save(QDir::temp().filePath(u"got-"_s + name)); // để so bằng mắt
        QVERIFY(off <= sheet.width() * sheet.height() / 1000);
    }
};

QTEST_MAIN(ViewerTest)
#include "viewer_test.moc"
