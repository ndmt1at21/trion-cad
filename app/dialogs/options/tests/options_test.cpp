// UID-019: khung hộp thoại Tùy chọn và chỉ mục tìm thiết lập (OptionsDialog.h, SettingsIndex.h). Trang nội dung của các
// tab là việc của UID-020…026: test dựng trang mẫu (khóa "test/…" và chủ đề thật của ThemeManager). INT-002 (lệnh),
// INT-005 (sysvar) chưa có: macroRequested ghi lại, Sysvars tạm chỉ được đọc. Tiêu chí của task:
//   1  prototype, search, indexCoverage: TC.SETTINGS_INDEX và OPT_TABS (screens2.js nhúng) khớp bảng; gõ "autosave",
//      "tự lưu", "SAVETIME" nhảy tab Mở & Lưu, tô sáng hàng, cuộn tới; mỗi phím ≤ 16 ms CPU luồng UI (release); CI:
//      indexIssues báo hàng thiếu mục / mục không có hàng.
//   2  cancelRestoresPreview, applyAtomic: Hủy trả chủ đề (và khóa chưa đặt) như trước khi mở; Áp dụng ghi mọi khóa
//      trong một lượt — tệp trên đĩa chỉ có trạng thái cũ hoặc mới hẳn.
//   3  drawingOneUndo: sysvar bản vẽ thành một chuỗi lệnh UNDO BEGIN/SETVAR…/UNDO END; khóa hồ sơ không phát lệnh.
//   4  viewerLocks: Viewer dùng Hiển thị, Hệ thống, Dòng lệnh, Hồ sơ, Tài khoản; tab khác khóa, nút "Dùng thử Pro 30
//      ngày" mở trang nâng cấp; đổi phiên bản lúc đang mở.
//   5  openAndKeys: mở ≤ 150 ms CPU tới khung đầu (release); Ctrl+Tab, Ctrl+Shift+Tab (vòng); Enter = Lưu và đóng;
//      Esc = Hủy (cancelRestoresPreview); open(tab, hàng).
//   Còn lại: resetDefaults (xác nhận nêu số lượng, vào nháp, Hủy được), a11y (checkNames, checkFocus),
//   pseudoNoTruncation (vi-XL, hai mật độ, trang, khóa, rỗng; đổi ngôn ngữ dựng lại trang từ nháp), idle.
#include "app/dialogs/options/OptionsDialog.h"
#include "app/dialogs/options/SettingsIndex.h"

#include "app/core/a11y/AccessibilityAudit.h"
#include "app/core/actions/ActionRegistry.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/dialog/DialogFrame.h"
#include "app/widgets/message/Messages.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"
#include "tests/timing.h"

#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <memory>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using trion::options::OptionsDialog;
using trion::options::OptionsDraft;
using trion::options::OptionsPage;
using trion::options::SettingsIndex;
using trion::options::Tab;
using trion::test::kTimed;
namespace actions = trion::actions;
namespace edition = trion::edition;
namespace i18n = trion::i18n;
namespace options = trion::options;
namespace sched = trion::sched;
namespace settings = trion::settings;
namespace theme = trion::theme;
namespace ui = trion::ui;
using Edition = edition::Edition;
using Preview = OptionsDraft::Preview;

namespace {

constexpr double kKeyBudgetMs = 16;   // DoD-L: danh sách / kết quả cập nhật sau mỗi phím
constexpr double kOpenBudgetMs = 150; // DoD-L: mở hộp thoại đến khung đầu có nội dung

// Khóa của trang mẫu (trang thật do UID-020…026 làm).
const settings::Setting<bool> kTooltips{"test/tooltips", true};
const settings::Setting<int> kRibbonIcons{"test/ribbonIcons", 32};
const settings::Setting<int> kCrosshair{"test/crosshair", 5};
const settings::Setting<QString> kCmdFont{"test/cmdFont", QStringLiteral("plex")};
const settings::Setting<bool> kAutosave{"test/autosave", true};
const settings::Setting<int> kHistory{"test/historyLines", 3};
const settings::Setting<bool> kSearchVi{"test/searchVi", true};
const QStringList kTestKeys{u"test/tooltips"_s, u"test/ribbonIcons"_s,  u"test/crosshair"_s, u"test/cmdFont"_s,
                            u"test/autosave"_s, u"test/historyLines"_s, u"test/searchVi"_s};

bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 10s)
{
    if (cond())
        return true;
    QEventLoop loop;
    QTimer tick;
    QObject::connect(&tick, &QTimer::timeout, &loop, [&] {
        if (cond())
            loop.quit();
    });
    QTimer::singleShot(timeout, &loop, &QEventLoop::quit);
    tick.start(1);
    loop.exec();
    return cond();
}

void spinFor(std::chrono::milliseconds d)
{
    QEventLoop loop;
    QTimer::singleShot(d, &loop, &QEventLoop::quit);
    loop.exec();
}

double ms(std::chrono::nanoseconds d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

QString truncations(QWidget* root)
{
    QStringList out;
    for (const i18n::Truncation& t : i18n::findTruncations(root))
        out << t.toString();
    return out.join(u"; "_s);
}

QByteArray fileBytes(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

QJsonObject fileJson(const QString& path)
{
    return QJsonDocument::fromJson(fileBytes(path)).object();
}

// Mảng JS của prototype (chuỗi trong nháy đơn, không có nháy kép) → JSON.
QJsonArray protoArray(const QString& src, QStringView start)
{
    const qsizetype at = src.indexOf(start);
    const qsizetype open = at < 0 ? -1 : src.indexOf(u'[', at);
    const qsizetype end = open < 0 ? -1 : src.indexOf(u"];"_s, open);
    if (end < 0)
        return {};
    QString js = src.mid(open, end - open + 1);
    js.replace(u'\'', u'"');
    return QJsonDocument::fromJson(js.toUtf8()).array();
}

ui::Button* buttonWithText(QWidget* root, const QString& text)
{
    for (ui::Button* b : root->findChildren<ui::Button*>())
        if (b->text() == text && b->isVisible())
            return b;
    return nullptr;
}

QListView* tabList(QWidget* dlg)
{
    return dlg->findChild<QListView*>();
}

QModelIndex tabIndex(QListView* list, int row)
{
    return list->model()->index(row, 0);
}

// ------------------------------------------------------------ trang mẫu

// Hiển thị: đủ 7 mục chỉ mục của tab; chủ đề và Giao diện nhẹ xem trước (Live).
void displayPage(OptionsPage& page)
{
    OptionsDraft& d = page.draft();
    page.addGroup(u"Giao diện"_s); // i18n-ok: trang mẫu của test
    auto* themeSeg = new ui::SegmentedGroup;
    const QStringList values{u"system"_s, u"dark"_s, u"light"_s};
    themeSeg->addSegment(u"Theo hệ thống"_s); // i18n-ok: trang mẫu
    themeSeg->addSegment(u"Tối"_s);           // i18n-ok: trang mẫu
    themeSeg->addSegment(u"Sáng"_s);          // i18n-ok: trang mẫu
    themeSeg->setCurrentIndex(std::max<int>(0, values.indexOf(d.get(theme::kTheme, Preview::Live))));
    QObject::connect(themeSeg, &ui::SegmentedGroup::currentIndexChanged, &page,
                     [&d, values](int i) { d.set(theme::kTheme, values.value(i)); });
    page.addRow("theme", u"Chủ đề tối, sáng"_s, {themeSeg});                  // i18n-ok: trang mẫu
    auto* lite = new QCheckBox(u"Tắt bóng đổ, làm mờ, hiệu ứng (máy yếu)"_s); // i18n-ok: trang mẫu
    lite->setChecked(d.get(theme::kLite, Preview::Live));
    QObject::connect(lite, &QCheckBox::toggled, &page, [&d](bool on) { d.set(theme::kLite, on); });
    page.addRow("lite", u"Giao diện nhẹ"_s, {lite}); // i18n-ok: trang mẫu
    auto* icons = new ui::DropDown;
    icons->addItem(u"16 px"_s, 16); // i18n-ok: trang mẫu
    icons->addItem(u"32 px"_s, 32); // i18n-ok: trang mẫu
    icons->setCurrentIndex(icons->findData(d.get(kRibbonIcons)));
    QObject::connect(icons, &QComboBox::currentIndexChanged, &page,
                     [&d, icons](int) { d.set(kRibbonIcons, icons->currentData().toInt()); });
    page.addRow("ribbon-icons", u"Cỡ biểu tượng ribbon"_s, {icons}); // i18n-ok: trang mẫu
    auto* tips = new QCheckBox(u"Hiện phím tắt trong tooltip"_s);    // i18n-ok: trang mẫu
    tips->setObjectName(u"tooltips"_s);
    tips->setChecked(d.get(kTooltips));
    QObject::connect(tips, &QCheckBox::toggled, &page, [&d](bool on) { d.set(kTooltips, on); });
    page.addRow("tooltips", u"Tooltip mở rộng"_s, {tips}); // i18n-ok: trang mẫu
    page.addGroup(u"Vùng vẽ"_s);                           // i18n-ok: trang mẫu
    page.addRow("canvas-colors", u"Màu vùng vẽ, chữ thập, marker"_s, {new ui::Button(u"Màu…"_s)}); // i18n-ok: mẫu
    auto* cross = new ui::Slider;
    cross->setRange(1, 100);
    cross->setValue(d.get(kCrosshair));
    QObject::connect(cross, &QSlider::valueChanged, &page, [&d](int v) { d.set(kCrosshair, v); });
    page.addRow("crosshair-size", u"Cỡ chữ thập"_s, {cross}); // i18n-ok: trang mẫu
    auto* font = new ui::DropDown;
    font->addItem(u"IBM Plex Mono 12"_s, u"plex"_s); // i18n-ok: trang mẫu
    font->addItem(u"Consolas 12"_s, u"consolas"_s);  // i18n-ok: trang mẫu
    font->setCurrentIndex(std::max(0, font->findData(d.get(kCmdFont))));
    QObject::connect(font, &QComboBox::currentIndexChanged, &page,
                     [&d, font](int) { d.set(kCmdFont, font->currentData().toString()); });
    page.addRow("cmdline-font", u"Font dòng lệnh"_s, {font}); // i18n-ok: trang mẫu
}

// Mở & Lưu: mục "autosave"; filler hàng đệm phía trên để phải cuộn mới thấy (id không có trong chỉ mục).
void openSavePage(OptionsPage& page, int filler)
{
    OptionsDraft& d = page.draft();
    page.addGroup(u"An toàn tệp"_s); // i18n-ok: trang mẫu
    for (int i = 0; i < filler; ++i) {
        const QByteArray id = "filler-" + QByteArray::number(i);
        page.addRow(id.constData(), u"Hàng đệm %1"_s.arg(i), {new QCheckBox(u"Bật"_s)}); // i18n-ok: trang mẫu
    }
    auto* box = new QCheckBox(u"Tự lưu mỗi 10 phút"_s); // i18n-ok: trang mẫu
    box->setObjectName(u"autosave"_s);
    box->setChecked(d.get(kAutosave));
    QObject::connect(box, &QCheckBox::toggled, &page, [&d](bool on) { d.set(kAutosave, on); });
    page.addRow("autosave", u"Tự lưu"_s, {box}); // i18n-ok: trang mẫu
}

void commandLinePage(OptionsPage& page)
{
    OptionsDraft& d = page.draft();
    page.addGroup(u"Dòng lệnh"_s); // i18n-ok: trang mẫu
    auto* lines = new ui::DropDown;
    for (int n : {0, 3, 10})
        lines->addItem(QString::number(n), n);
    lines->setCurrentIndex(std::max(0, lines->findData(d.get(kHistory))));
    QObject::connect(lines, &QComboBox::currentIndexChanged, &page,
                     [&d, lines](int) { d.set(kHistory, lines->currentData().toInt()); });
    page.addRow("cmdline-history", u"Số dòng lịch sử dòng lệnh"_s, {lines}); // i18n-ok: trang mẫu
    auto* vi = new QCheckBox(u"Gõ “cắt” ra TRIM, “bo góc” ra FILLET"_s);     // i18n-ok: trang mẫu
    vi->setChecked(d.get(kSearchVi));
    QObject::connect(vi, &QCheckBox::toggled, &page, [&d](bool on) { d.set(kSearchVi, on); });
    page.addRow("cmdline-vietnamese", u"Tìm lệnh theo tiếng Việt"_s, {vi}); // i18n-ok: trang mẫu
}

// Hàng lưu trong bản vẽ (VIEWRES): sysvar qua nháp, không ghi Store.
void drawingPage(OptionsPage& page)
{
    OptionsDraft& d = page.draft();
    auto* res = new ui::DropDown;
    res->setObjectName(u"viewres"_s);
    for (const char* v : {"1000", "2000", "5000"})
        res->addItem(QString::fromLatin1(v), QString::fromLatin1(v));
    res->setCurrentIndex(std::max(0, res->findData(d.sysvar(u"VIEWRES"_s))));
    QObject::connect(res, &QComboBox::currentIndexChanged, &page,
                     [&d, res](int) { d.setSysvar(u"VIEWRES"_s, res->currentData().toString()); });
    page.addRow("viewres", u"Độ mịn cung tròn"_s, {res}, OptionsPage::Where::Drawing); // i18n-ok: trang mẫu
}

// Hết khung đầu của hộp: lần vẽ đầu (cả các con, cùng một lượt) xong thì timer 0 ms hẹn từ lúc đó chạy trước việc hẹn
// sau nó (dựng sẵn tab, hẹn trong paintEvent của hộp).
class FirstFrame : public QObject {
public:
    bool done = false;
    std::chrono::nanoseconds cpu{-1}; // threadCpu() lúc hết khung
    bool eventFilter(QObject*, QEvent* e) override
    {
        if (e->type() == QEvent::Paint && !std::exchange(armed_, true))
            QTimer::singleShot(0, this, [this] {
                cpu = trion::test::threadCpu();
                done = true;
            });
        return false;
    }

private:
    bool armed_ = false;
};

} // namespace

class OptionsTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<theme::ThemeManager> tm_;
    std::unique_ptr<theme::IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<edition::EditionGate> gate_;
    std::unique_ptr<actions::Sysvars> sysvars_;
    std::unique_ptr<QWidget> host_;
    QPointer<OptionsDialog> dlg_;
    QStringList macros_;
    QStringList upgrades_;

    // Hộp có trang mẫu Hiển thị, Mở & Lưu (filler hàng đệm), Dòng lệnh; chưa hiện.
    OptionsDialog* make(int filler = 0)
    {
        delete dlg_;
        dlg_ = new OptionsDialog(*gate_, *store_, host_.get());
        dlg_->addPage(Tab::Display, displayPage);
        dlg_->addPage(Tab::OpenSave, [filler](OptionsPage& p) { openSavePage(p, filler); });
        dlg_->addPage(Tab::CommandLine, commandLinePage);
        connect(dlg_, &OptionsDialog::macroRequested, this, [this](const QString& m) { macros_ << m; });
        return dlg_;
    }

    bool show(OptionsDialog* d)
    {
        d->open();
        d->activateWindow();
        return QTest::qWaitForWindowExposed(d) && spinUntil([d] { return QApplication::activeWindow() == d; });
    }

    // Chuỗi dựng sẵn tab sau khung đầu (một lần mỗi tab, tự dừng) đã xong.
    static void settle(OptionsDialog* d)
    {
        const auto busy = [d] {
            const QList<QTimer*> timers = d->findChildren<QTimer*>();
            return std::ranges::any_of(timers, [](QTimer* t) { return t->isActive(); });
        };
        QVERIFY(spinUntil([&] { return !busy(); }));
    }

    // Gõ từng ký tự vào ô tìm (từ ô rỗng); trả thời gian CPU luồng UI lớn nhất của một phím, gồm cả vẽ khung kế tiếp.
    double typeSearch(OptionsDialog* d, const QString& text)
    {
        settle(d); // người gõ sau khi hộp đã hiện xong: các tab đã dựng sẵn
        d->search()->clear();
        d->search()->setFocus(Qt::OtherFocusReason);
        QCoreApplication::processEvents();
        double worst = 0;
        for (const QChar c : text) {
            const auto t0 = trion::test::threadCpu();
            // Phím đã có chữ (như bộ gõ đã chốt, cả chữ có dấu): QTest::keyClicks chỉ gửi chữ Latin-1.
            const auto key = c.unicode() < 0x80 ? Qt::Key(c.toUpper().unicode()) : Qt::Key_unknown;
            QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, QString(c));
            QCoreApplication::sendEvent(d->search(), &press);
            QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, QString(c));
            QCoreApplication::sendEvent(d->search(), &release);
            QCoreApplication::processEvents();
            const auto t1 = trion::test::threadCpu();
            if (t0.count() >= 0)
                worst = std::max(worst, ms(t1 - t0));
        }
        return worst;
    }

    // Hàng đang tô sáng nằm trong vùng nhìn của vùng cuộn.
    static bool rowVisible(OptionsDialog* d, const QString& id)
    {
        OptionsPage* p = d->page(d->currentTab());
        QWidget* row = p ? p->row(id) : nullptr;
        QScrollArea* area = nullptr;
        for (QWidget* w = row; w && !area; w = w->parentWidget())
            area = qobject_cast<QScrollArea*>(w);
        if (!area)
            return false;
        const QRect r(row->mapTo(area->viewport(), QPoint(0, 0)), row->size());
        return area->viewport()->rect().contains(r);
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
        gate_ = std::make_unique<edition::EditionGate>();
        gate_->upgrade().setHandler([this](const edition::UpgradeRequest& r) { upgrades_ << r.feature; });
        sysvars_ = std::make_unique<actions::Sysvars>();
        QApplication::setCursorFlashTime(0);
        host_ = std::make_unique<QWidget>();
        host_->setAccessibleName(u"Cửa sổ chính"_s); // i18n-ok: cửa sổ giả của test
        host_->resize(1366, 768);
        host_->move(0, 0);
        host_->show();
        QVERIFY(QTest::qWaitForWindowExposed(host_.get()));
    }

    void init()
    {
        tm_->setTheme(theme::Theme::Dark);
        tm_->setDensity(theme::Density::Compact);
        tm_->setLite(false);
        store_->remove(QString::fromLatin1(theme::kLite.key));
        for (const QString& k : kTestKeys)
            store_->remove(k);
        gate_->setEdition(Edition::Pro);
        sysvars_->set(u"VIEWRES"_s, 1000);
        sysvars_->set(u"FILLMODE"_s, 1);
        macros_.clear();
        upgrades_.clear();
    }

    void cleanup()
    {
        delete dlg_;
        if (translator_->language() != u"vi"_s)
            QVERIFY(translator_->setLanguage(u"vi"_s));
        QCoreApplication::processEvents();
    }

    void cleanupTestCase()
    {
        delete dlg_;
        sched::waitIdle();
        host_.reset();
        sysvars_.reset();
        gate_.reset();
        translator_.reset();
        icons_.reset();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        QCoreApplication::processEvents();
    }

    // Tiêu chí 1 (một nguồn): bảng tab khớp OPT_TABS; mỗi mục TC.SETTINGS_INDEX có trong SettingsIndex (nhãn, từ khóa,
    // tab); id duy nhất, ASCII; mục nào cũng có khóa tiếng Anh.
    void prototype()
    {
        QFile js(u":/options_test/screens2.js"_s);
        QVERIFY(js.open(QIODevice::ReadOnly));
        const QString src = QString::fromUtf8(js.readAll());
        const QJsonArray tabs = protoArray(src, u"var OPT_TABS");
        QCOMPARE(tabs.size(), options::kTabCount);
        for (int i = 0; i < options::kTabCount; ++i) {
            const QJsonArray t = tabs[i].toArray();
            const options::TabInfo& info = options::tabInfo(Tab(i));
            QCOMPARE(t[0].toString(), QString::fromUtf8(info.name));
            QCOMPARE(t[1].toString(), QString::fromLatin1(info.icon));
            QCOMPARE(t[2].toString() == u"V"_s, info.need == ui::Edition::None);
            QVERIFY(options::tabFromId(QString::fromLatin1(info.id)) == Tab(i));
        }
        const QJsonArray index = protoArray(src, u"TC.SETTINGS_INDEX");
        QVERIFY(index.size() >= 19);
        for (const QJsonValue& v : index) {
            const QJsonArray e = v.toArray();
            const auto it = std::find_if(
                SettingsIndex::entries().begin(), SettingsIndex::entries().end(),
                [&](const options::SettingEntry& x) { return QString::fromUtf8(x.vi) == e[0].toString(); });
            QVERIFY2(it != SettingsIndex::entries().end(), qPrintable(e[0].toString()));
            QCOMPARE(QString::fromUtf8(it->keywords), e[1].toString());
            QCOMPARE(QString::fromUtf8(options::tabInfo(it->tab).name), e[2].toString());
        }
        QSet<QString> ids;
        for (const options::SettingEntry& e : SettingsIndex::entries()) {
            const QString id = QString::fromLatin1(e.id);
            QVERIFY2(!ids.contains(id), qPrintable(id));
            ids.insert(id);
            QVERIFY2(QRegularExpression(u"^[a-z0-9-]+$"_s).match(id).hasMatch(), qPrintable(id));
            QVERIFY2(*e.en, qPrintable(id));
            QCOMPARE(SettingsIndex::find(id), &e);
        }
        QCOMPARE(SettingsIndex::find(u"khong-co"), nullptr);
    }

    // Tiêu chí 1: tìm trên chỉ mục (không dấu, tiếng Anh, sysvar), rồi trên hộp thoại: nhảy tab, tô sáng, cuộn tới,
    // ≤ 16 ms mỗi phím.
    void search()
    {
        const auto hit = [](const QString& q) {
            const options::SettingEntry* e = SettingsIndex::search(q);
            return e ? QString::fromLatin1(e->id) : QString();
        };
        for (const QString& q : {u"autosave"_s, u"tự lưu"_s, u"tu luu"_s, u"TỰ LƯU"_s, u"SAVETIME"_s, u"savetime"_s,
                                 u"isavebak"_s, u"automatic save"_s, u"luu tu"_s})
            QCOMPARE(hit(q), u"autosave"_s);
        QCOMPARE(hit(u"chủ đề"_s), u"theme"_s);
        QCOMPARE(hit(u"dark"_s), u"theme"_s);
        QCOMPARE(hit(u"CURSORSIZE"_s), u"crosshair-size"_s);
        QCOMPARE(hit(u"font mapping"_s), u"font-mapping"_s);
        QCOMPARE(hit(u"FONTMAP"_s), u"font-mapping"_s); // trùng hẳn sysvar thắng "font" của mục trước
        QCOMPARE(hit(u"ngôn ngữ"_s), u"language"_s);
        QCOMPARE(hit(u"   "_s), QString());
        QCOMPARE(hit(u"khongcogi"_s), QString());

        OptionsDialog* d = make(16);
        QVERIFY(show(d));
        double worst = 0;
        for (const QString& q : {u"autosave"_s, u"tự lưu"_s, u"SAVETIME"_s}) {
            d->setCurrentTab(Tab::Display);
            worst = std::max(worst, typeSearch(d, q));
            QCOMPARE(d->currentTab(), Tab::OpenSave);
            QCOMPARE(d->highlightedRow(), u"autosave"_s);
            QVERIFY2(rowVisible(d, u"autosave"_s), qPrintable(q));
            QVERIFY(d->search()->hasFocus()); // gõ tiếp được
            QWidget* noMatch = nullptr;
            for (QLabel* l : d->findChildren<QLabel*>())
                if (l->text() == u"Không thấy thiết lập"_s)
                    noMatch = l;
            QVERIFY(noMatch && !noMatch->isVisible());
        }
        qInfo("tìm: phím chậm nhất %.2f ms CPU (ngưỡng %.0f ms, kiểm khi kTimed)", worst, kKeyBudgetMs);
        QVERIFY2(!kTimed || worst <= kKeyBudgetMs, qPrintable(QString::number(worst)));

        // Không khớp (không mục nào có "@"): dòng "Không thấy thiết lập", tab giữ nguyên, bỏ tô sáng.
        typeSearch(d, u"@@"_s);
        QCOMPARE(d->currentTab(), Tab::OpenSave);
        QCOMPARE(d->highlightedRow(), QString());
        QLabel* noMatch = nullptr;
        for (QLabel* l : d->findChildren<QLabel*>())
            if (l->text() == u"Không thấy thiết lập"_s)
                noMatch = l;
        QVERIFY(noMatch && noMatch->isVisible());
        // Esc trong ô có chữ: xóa chữ, hộp còn mở; dòng báo ẩn.
        QTest::keyClick(d->search(), Qt::Key_Escape);
        QVERIFY(d->isVisible());
        QVERIFY(d->search()->text().isEmpty());
        QVERIFY(!noMatch->isVisible());
        // Mục của tab chưa có trang: nhảy tới tab (trạng thái rỗng), không có hàng để tô.
        typeSearch(d, u"privacy"_s);
        QCOMPARE(d->currentTab(), Tab::System);
        QVERIFY(qobject_cast<ui::EmptyState*>(d->tabContent(Tab::System)));
        QCOMPARE(d->highlightedRow(), QString());
    }

    // Tiêu chí 1 (CI): mọi hàng của trang đã đăng ký có mục chỉ mục đúng tab, mọi mục của tab có hàng.
    void indexCoverage()
    {
        // Hộp như ứng dụng dựng: trang đăng ký trong hàm dựng (UID-020…026 thêm); kiểm chạy trên chúng.
        auto* bare = new OptionsDialog(*gate_, *store_, host_.get());
        QCOMPARE(bare->indexIssues(), QStringList());
        delete bare;
        // Trang mẫu đủ mục: đạt.
        OptionsDialog* d = make();
        QCOMPARE(d->indexIssues(), QStringList());
        // Hàng lạ, hàng sai tab, id trùng, mục thiếu hàng: báo đủ.
        d->addPage(Tab::OpenSave, [](OptionsPage& p) { openSavePage(p, 1); });
        d->addPage(Tab::Files, [](OptionsPage& p) {
            p.addRow("support-paths", u"Đường dẫn"_s, {new QCheckBox}); // i18n-ok: trang mẫu
            p.addRow("support-paths", u"Đường dẫn"_s, {new QCheckBox}); // i18n-ok: trang mẫu
            p.addRow("theme", u"Chủ đề"_s, {new QCheckBox});            // i18n-ok: trang mẫu
        });
        const QStringList issues = d->indexIssues();
        qInfo("%s", qPrintable(issues.join(u"\n"_s)));
        QCOMPARE(issues.size(), 4);
        QVERIFY(issues.contains(u"opensave/filler-0: hàng không có mục trong SettingsIndex"_s));
        QVERIFY(issues.contains(u"files/support-paths: id hàng trùng"_s));
        QVERIFY(issues.contains(u"files/theme: mục chỉ mục thuộc tab display"_s));
        QVERIFY(issues.contains(u"files/font-mapping: mục chỉ mục không có hàng trên trang"_s));
    }

    // Tiêu chí 2: xem trước chủ đề, Giao diện nhẹ; Esc trả mọi thứ như trước khi mở, kể cả khóa chưa đặt; khóa không
    // xem trước chưa từng chạm Store.
    void cancelRestoresPreview()
    {
        QCOMPARE(store_->value(u"ui/theme"_s).toString(), u"dark"_s);
        QVERIFY(store_->value(u"ui/lite"_s).isUndefined());
        store_->flush();
        const QJsonObject before = fileJson(store_->filePath());

        OptionsDialog* d = make();
        QVERIFY(show(d));
        auto* page = d->page(Tab::Display);
        QVERIFY(page);
        auto* themeSeg = page->row(u"theme")->findChild<ui::SegmentedGroup*>();
        QVERIFY(themeSeg);
        QTest::mouseClick(themeSeg->segment(2), Qt::LeftButton); // Sáng
        QCOMPARE(tm_->theme(), theme::Theme::Light);             // áp ngay
        QCOMPARE(store_->value(u"ui/theme"_s).toString(), u"light"_s);
        page->row(u"lite")->findChild<QCheckBox*>()->click();
        QVERIFY(tm_->lite());
        page->findChild<QCheckBox*>(u"tooltips"_s)->click(); // không xem trước: chỉ nháp
        QVERIFY(store_->value(u"test/tooltips"_s).isUndefined());
        QVERIFY(d->draft().isDirty());
        QVERIFY(!d->draft().get(kTooltips));

        d->search()->setFocus(Qt::OtherFocusReason);
        QTest::keyClick(d->search(), Qt::Key_Escape); // ô rỗng: Esc = Hủy
        QVERIFY(spinUntil([d] { return !d->isVisible(); }));
        QCOMPARE(d->result(), int(QDialog::Rejected));
        QCOMPARE(tm_->theme(), theme::Theme::Dark);
        QVERIFY(!tm_->mode().light);
        QVERIFY(!tm_->lite());
        QCOMPARE(store_->value(u"ui/theme"_s).toString(), u"dark"_s);
        QVERIFY(store_->value(u"ui/lite"_s).isUndefined()); // chưa đặt trước khi mở: lại chưa đặt
        QVERIFY(store_->value(u"test/tooltips"_s).isUndefined());
        QVERIFY(macros_.isEmpty());
        sched::waitIdle();
        store_->flush();
        QCOMPARE(fileJson(store_->filePath()), before);

        // Áp dụng rồi Hủy: phần đã áp dụng giữ nguyên (như AutoCAD).
        d = make();
        QVERIFY(show(d));
        QTest::mouseClick(d->page(Tab::Display)->row(u"theme")->findChild<ui::SegmentedGroup*>()->segment(2),
                          Qt::LeftButton);
        buttonWithText(d, u"Áp dụng"_s)->click();
        d->reject();
        QCOMPARE(tm_->theme(), theme::Theme::Light);
        QCOMPARE(store_->value(u"ui/theme"_s).toString(), u"light"_s);
    }

    // Tiêu chí 2: Áp dụng ghi mọi khóa hồ sơ trong một lượt. Trong lúc áp dụng tệp trên đĩa vẫn là bản cũ (không lần
    // ghi nào xen giữa các khóa); bản mới đầu tiên trên đĩa đã có đủ cả ba khóa. Không phát lệnh (không bước undo).
    void applyAtomic()
    {
        store_->set(kAutosave, true);
        store_->flush();
        const QByteArray before = fileBytes(store_->filePath());
        OptionsDialog* d = make();
        QVERIFY(show(d));
        d->page(Tab::Display)->findChild<QCheckBox*>(u"tooltips"_s)->click(); // qua trang
        d->setCurrentTab(Tab::OpenSave);
        d->page(Tab::OpenSave)->findChild<QCheckBox*>(u"autosave"_s)->click();
        d->draft().set(kHistory, 10); // qua nháp
        QVERIFY(store_->value(u"test/tooltips"_s).isUndefined());
        ui::Button* apply = buttonWithText(d, u"Áp dụng"_s);
        QVERIFY(apply && apply->isEnabled());

        QList<QByteArray> during;
        QStringList keys;
        const auto conn = connect(store_.get(), &settings::Store::changed, this, [&](const QString& k) {
            keys << k;
            during << fileBytes(store_->filePath());
        });
        apply->click();
        disconnect(conn);
        keys.sort();
        QCOMPARE(keys, (QStringList{u"test/autosave"_s, u"test/historyLines"_s, u"test/tooltips"_s}));
        for (const QByteArray& b : std::as_const(during))
            QCOMPARE(b, before);
        QCOMPARE(fileBytes(store_->filePath()), before); // ghi đĩa còn chờ: chưa có gì nửa chừng
        QSet<QByteArray> seen{before};
        QVERIFY(spinUntil([&] {
            const QByteArray now = fileBytes(store_->filePath());
            seen.insert(now);
            return now != before;
        }));
        sched::waitIdle();
        seen.insert(fileBytes(store_->filePath()));
        QCOMPARE(seen.size(), 2); // cũ, mới hẳn
        const QJsonObject after = fileJson(store_->filePath());
        QCOMPARE(after[u"test/tooltips"_s].toBool(true), false);
        QCOMPARE(after[u"test/autosave"_s].toBool(true), false);
        QCOMPARE(after[u"test/historyLines"_s].toInt(), 10);
        QVERIFY(!apply->isEnabled());
        QVERIFY(!d->draft().isDirty());
        QVERIFY(macros_.isEmpty()); // thiết lập hồ sơ không thành lệnh, không có bước undo
        QVERIFY(d->isVisible());    // Áp dụng không đóng
    }

    // Tiêu chí 3: sysvar lưu trong bản vẽ thành một chuỗi lệnh có một nhóm UNDO; Sysvars (lõi) không bị giả đổi.
    void drawingOneUndo()
    {
        OptionsDialog* d = make();
        d->addPage(Tab::User, drawingPage);
        QVERIFY(show(d));
        // Chưa có bản vẽ: hàng lưu trong bản vẽ vô hiệu, đầu hộp "Bản vẽ: —".
        d->setCurrentTab(Tab::User);
        auto* res = d->page(Tab::User)->findChild<ui::DropDown*>(u"viewres"_s);
        QVERIFY(res && !res->isEnabled());
        QVERIFY(std::ranges::any_of(d->findChildren<QLabel*>(),
                                    [](QLabel* l) { return l->text().contains(u"Bản vẽ: —"_s); }));
        d->setDrawing(u"Nha-o-2-tang.dwg"_s, sysvars_.get());
        QVERIFY(std::ranges::any_of(d->findChildren<QLabel*>(),
                                    [](QLabel* l) { return l->text().contains(u"Bản vẽ: Nha-o-2-tang.dwg"_s); }));
        res = d->page(Tab::User)->findChild<ui::DropDown*>(u"viewres"_s);
        QVERIFY(res && res->isEnabled());
        QCOMPARE(res->currentData().toString(), u"1000"_s);
        QCOMPARE(res->accessibleDescription(), u"Lưu trong bản vẽ"_s);
        QVERIFY(std::ranges::any_of(d->page(Tab::User)->row(u"viewres")->findChildren<QWidget*>(),
                                    [](QWidget* w) { return w->toolTip() == u"Lưu trong bản vẽ"_s; }));

        res->setCurrentIndex(res->findData(u"2000"_s));
        d->draft().setSysvar(u"fillmode"_s, u"0"_s);
        d->draft().setSysvar(u"ORTHOMODE"_s, u"1"_s);
        d->draft().setSysvar(u"ORTHOMODE"_s, u"0"_s); // về giá trị hiện tại: bỏ khỏi nháp
        d->draft().set(kHistory, 0);                  // khóa hồ sơ cùng lần áp dụng
        QCOMPARE(d->draft().sysvar(u"VIEWRES"_s), u"2000"_s);
        QCOMPARE(sysvars_->get(u"VIEWRES"_s), 1000);
        QVERIFY(macros_.isEmpty()); // chưa áp dụng thì chưa gửi gì

        d->search()->setFocus(Qt::OtherFocusReason);
        QTest::keyClick(d->search(), Qt::Key_Return); // Enter = Lưu và đóng
        QVERIFY(spinUntil([d] { return !d->isVisible(); }));
        QCOMPARE(d->result(), int(QDialog::Accepted));
        QCOMPARE(macros_, QStringList{u"^C^C_UNDO _BEGIN _SETVAR FILLMODE 0 _SETVAR VIEWRES 2000 _UNDO _END"_s});
        const QStringList tokens = macros_.first().split(u' ');
        QCOMPARE(tokens.count(u"_SETVAR"_s), 2);
        QCOMPARE(tokens.count(u"_BEGIN"_s), 1); // một nhóm undo
        QCOMPARE(tokens.count(u"_END"_s), 1);
        QCOMPARE(sysvars_->get(u"VIEWRES"_s), 1000); // lõi đổi khi chạy lệnh (INT-002/INT-005), không giả ở đây
        QCOMPARE(store_->get(kHistory), 0);
        QVERIFY(!macros_.first().contains(u"historyLines"_s));

        // Hủy: không gửi lệnh nào.
        macros_.clear();
        d = make();
        d->setDrawing(u"A.dwg"_s, sysvars_.get());
        QVERIFY(show(d));
        d->draft().setSysvar(u"VIEWRES"_s, u"5000"_s);
        d->reject();
        QVERIFY(macros_.isEmpty());
        QCOMPARE(OptionsDraft::setvarMacro({{u"A"_s, u"1"_s}}), u"^C^C_UNDO _BEGIN _SETVAR A 1 _UNDO _END"_s);
    }

    // Tiêu chí 4: Viewer có 5 tab dùng được; tab khác có ổ khóa, mô tả "Có trong TrionCAD Pro", trạng thái khóa với nút
    // "Dùng thử Pro 30 ngày" → trang nâng cấp. Đổi phiên bản lúc đang mở: khóa đổi theo, nội dung dựng lại.
    void viewerLocks()
    {
        gate_->setEdition(Edition::Viewer);
        OptionsDialog* d = make();
        QVERIFY(show(d));
        QListView* list = tabList(d);
        QVERIFY(list);
        QCOMPARE(list->model()->rowCount(), options::kTabCount);
        const QList<Tab> usable{Tab::Display, Tab::System, Tab::CommandLine, Tab::Profiles, Tab::Account};
        for (int i = 0; i < options::kTabCount; ++i) {
            const Tab t = Tab(i);
            QCOMPARE(d->isTabLocked(t), !usable.contains(t));
            QCOMPARE(tabIndex(list, i).data(Qt::AccessibleDescriptionRole).toString(),
                     usable.contains(t) ? QString() : u"Có trong TrionCAD Pro"_s);
            d->setCurrentTab(t);
            QCOMPARE(list->currentIndex().row(), i);
            if (usable.contains(t)) {
                QVERIFY(d->page(t) || qobject_cast<ui::EmptyState*>(d->tabContent(t)));
                QVERIFY(!buttonWithText(d, u"Dùng thử Pro 30 ngày"_s));
                continue;
            }
            QVERIFY(!d->page(t));
            QVERIFY(qobject_cast<ui::EmptyState*>(d->tabContent(t)));
            const QString title = u"Tab “%1” có trong TrionCAD Pro"_s.arg(tabIndex(list, i).data().toString());
            QVERIFY2(std::ranges::any_of(d->tabContent(t)->findChildren<QLabel*>(),
                                         [&](QLabel* l) { return l->text() == title; }),
                     qPrintable(title));
            ui::Button* trial = buttonWithText(d->tabContent(t), u"Dùng thử Pro 30 ngày"_s);
            QVERIFY(trial);
            if (t == Tab::OpenSave) {
                trial->click();
                QCOMPARE(upgrades_, QStringList{u"OPTIONS"_s});
            }
        }
        QVERIFY(d->page(Tab::Display));
        QVERIFY(d->page(Tab::CommandLine));
        // Tìm mục của tab khóa: nhảy tới, hiện trạng thái khóa.
        typeSearch(d, u"autosave"_s);
        QCOMPARE(d->currentTab(), Tab::OpenSave);
        QVERIFY(!d->page(Tab::OpenSave));
        QCOMPARE(d->highlightedRow(), QString());
        // Kích hoạt Pro khi đang mở: hết khóa, trang hiện, tìm lại thì tô sáng.
        gate_->setEdition(Edition::Pro);
        QVERIFY(!d->isTabLocked(Tab::OpenSave));
        QVERIFY(tabIndex(list, int(Tab::OpenSave)).data(Qt::AccessibleDescriptionRole).toString().isEmpty());
        QVERIFY(d->page(Tab::OpenSave));
        typeSearch(d, u"tự lưu"_s);
        QCOMPARE(d->highlightedRow(), u"autosave"_s);
    }

    // Tiêu chí 5: mở ≤ 150 ms CPU tới khung đầu; Ctrl+Tab, Ctrl+Shift+Tab; Enter; open(tab, hàng).
    void openAndKeys()
    {
        double best = 1e9;
        double worst = 0;
        for (int i = 0; i < 5; ++i) {
            delete dlg_;
            QCoreApplication::processEvents();
            const auto t0 = trion::test::threadCpu();
            OptionsDialog* d = make();
            FirstFrame frame;
            d->installEventFilter(&frame);
            d->open();
            QVERIFY(spinUntil([&] { return frame.done; }));
            d->removeEventFilter(&frame);
            if (t0.count() >= 0) {
                best = std::min(best, ms(frame.cpu - t0));
                worst = std::max(worst, ms(frame.cpu - t0));
            }
        }
        qInfo("mở: tốt nhất %.1f ms, chậm nhất %.1f ms CPU tới khung đầu (ngưỡng %.0f ms, kiểm khi kTimed)", best,
              worst, kOpenBudgetMs);
        QVERIFY2(!kTimed || worst <= kOpenBudgetMs, qPrintable(QString::number(worst)));

        OptionsDialog* d = dlg_;
        d->activateWindow();
        QVERIFY(spinUntil([d] { return QApplication::activeWindow() == d; }));
        QCOMPARE(d->currentTab(), Tab::Display);
        QVERIFY(d->search()->hasFocus()); // focus đầu: ô tìm
        const auto ctrlTab = [](bool back) {
            QTest::keyClick(QApplication::focusWidget(), back ? Qt::Key_Backtab : Qt::Key_Tab,
                            back ? Qt::ControlModifier | Qt::ShiftModifier : Qt::ControlModifier);
        };
        ctrlTab(false);
        QCOMPARE(d->currentTab(), Tab::OpenSave);
        ctrlTab(true);
        QCOMPARE(d->currentTab(), Tab::Display);
        ctrlTab(true);
        ctrlTab(true);
        QCOMPARE(d->currentTab(), Tab::Account); // vòng
        ctrlTab(false);
        QCOMPARE(d->currentTab(), Tab::Files);
        // Từ danh sách tab và từ trong trang: cũng đổi tab; focus ở trang thì về danh sách.
        tabList(d)->setFocus(Qt::TabFocusReason);
        ctrlTab(false);
        QCOMPARE(d->currentTab(), Tab::Display);
        d->page(Tab::Display)->findChild<QCheckBox*>(u"tooltips"_s)->setFocus(Qt::TabFocusReason);
        ctrlTab(false);
        QCOMPARE(d->currentTab(), Tab::OpenSave);
        QVERIFY(tabList(d)->hasFocus());
        // ↓ trong danh sách tab.
        QTest::keyClick(tabList(d), Qt::Key_Down);
        QCOMPARE(d->currentTab(), Tab::Plot);

        // open(tab, hàng): chuyển tab, tô sáng hàng.
        d->open(Tab::CommandLine, u"cmdline-history"_s);
        QCOMPARE(d->currentTab(), Tab::CommandLine);
        QCOMPARE(d->highlightedRow(), u"cmdline-history"_s);
        QVERIFY(rowVisible(d, u"cmdline-history"_s));

        // Enter (focus ở danh sách tab) = Lưu và đóng: nháp được áp dụng.
        d->draft().set(kSearchVi, false);
        tabList(d)->setFocus(Qt::TabFocusReason);
        QTest::keyClick(tabList(d), Qt::Key_Return);
        QVERIFY(spinUntil([d] { return !d->isVisible(); }));
        QCOMPARE(d->result(), int(QDialog::Accepted));
        QCOMPARE(store_->get(kSearchVi), false);

        // open(tab, hàng) khi chưa mở: hiện rồi chuyển tới hàng.
        d = make(16);
        d->open(Tab::OpenSave, u"autosave"_s);
        QVERIFY(QTest::qWaitForWindowExposed(d));
        QCOMPARE(d->currentTab(), Tab::OpenSave);
        QCOMPARE(d->highlightedRow(), u"autosave"_s);
        QVERIFY(rowVisible(d, u"autosave"_s));
    }

    // Đặt lại mặc định: xác nhận nêu số tùy chọn sẽ đổi, đặt vào nháp (khóa xem trước áp ngay), Hủy giữ như cũ, Áp dụng
    // xóa khóa (về mặc định). Không có gì để đặt lại: không hỏi.
    void resetDefaults()
    {
        store_->set(kTooltips, false);
        store_->set(kCrosshair, 50);
        tm_->setTheme(theme::Theme::Light);
        OptionsDialog* d = make();
        QVERIFY(show(d));
        buttonWithText(d, u"Đặt lại mặc định"_s)->click();
        QPointer<ui::DialogFrame> confirm;
        for (ui::DialogFrame* f : d->findChildren<ui::DialogFrame*>())
            if (f->id() == u"confirm"_s)
                confirm = f;
        QVERIFY(confirm && confirm->isVisible());
        QCOMPARE(confirm->confirmButton()->text(), u"Đặt lại 3 tùy chọn"_s); // tooltips, crosshair, chủ đề
        confirm->confirmButton()->click();
        QVERIFY(spinUntil([&] { return !confirm || !confirm->isVisible(); }));
        QCOMPARE(tm_->theme(), theme::Theme::Dark); // chủ đề xem trước ngay
        QVERIFY(d->draft().get(kTooltips));
        QCOMPARE(d->draft().get(kCrosshair), 5);
        QVERIFY(d->page(Tab::Display)->findChild<QCheckBox*>(u"tooltips"_s)->isChecked()); // trang dựng lại từ nháp
        QCOMPARE(store_->get(kTooltips), false);                                           // chưa áp dụng
        d->reject();
        QCOMPARE(tm_->theme(), theme::Theme::Light);
        QCOMPARE(store_->get(kTooltips), false);
        QCOMPARE(store_->get(kCrosshair), 50);

        d = make();
        QVERIFY(show(d));
        buttonWithText(d, u"Đặt lại mặc định"_s)->click();
        for (ui::DialogFrame* f : d->findChildren<ui::DialogFrame*>())
            if (f->id() == u"confirm"_s && f->isVisible())
                f->confirmButton()->click();
        d->accept();
        QVERIFY(store_->value(u"test/tooltips"_s).isUndefined());
        QVERIFY(store_->value(u"test/crosshair"_s).isUndefined());
        QVERIFY(store_->value(u"ui/theme"_s).isUndefined());
        QCOMPARE(tm_->theme(), theme::Theme::Dark);

        // Mọi thứ đã mặc định: không hỏi.
        d = make();
        QVERIFY(show(d));
        buttonWithText(d, u"Đặt lại mặc định"_s)->click();
        QVERIFY(std::ranges::none_of(d->findChildren<ui::DialogFrame*>(),
                                     [](ui::DialogFrame* f) { return f->isVisible(); }));
    }

    // UI0-013: tên cho trình đọc màn hình, vòng Tab có viền focus: Pro (trang), Viewer (tab khóa), tab rỗng.
    void a11y()
    {
        OptionsDialog* d = make();
        QVERIFY(show(d));
        QStringList order;
        QStringList issues = trion::a11y::checkNames(d) + trion::a11y::checkFocus(d, &order);
        QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
        qInfo("Tab: %s", qPrintable(order.join(u" → "_s)));
        QVERIFY(order.size() >= 10);
        // Thứ tự: danh sách tab → nội dung tab (dựng sau chân) → chân.
        tabList(d)->setFocus(Qt::TabFocusReason);
        QTest::keyClick(tabList(d), Qt::Key_Tab);
        QVERIFY(d->page(Tab::Display)->isAncestorOf(QApplication::focusWidget()));
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Backtab, Qt::ShiftModifier);
        QVERIFY(tabList(d)->hasFocus());
        d->page(Tab::Display)->row(u"cmdline-font")->findChild<ui::DropDown*>()->setFocus(Qt::TabFocusReason);
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), buttonWithText(d, u"Đặt lại mặc định"_s));
        d->setCurrentTab(Tab::System); // rỗng
        issues = trion::a11y::checkNames(d) + trion::a11y::checkFocus(d);
        QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
        gate_->setEdition(Edition::Viewer);
        d->setCurrentTab(Tab::Files); // khóa
        issues = trion::a11y::checkNames(d) + trion::a11y::checkFocus(d, &order);
        QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
        QCOMPARE(d->search()->accessibleName(), u"Tìm thiết lập"_s);
        QCOMPARE(tabList(d)->accessibleName(), u"Các tab tùy chọn"_s);
    }

    // vi-XL (chuỗi dài hơn 40%): 0 chữ bị cắt ở hai mật độ: danh sách tab (có ổ khóa), đầu hộp, trang, trạng thái khóa,
    // rỗng, dòng không khớp, chân. Đổi ngôn ngữ khi đang mở: trang dựng lại, giữ nháp.
    void pseudoNoTruncation()
    {
        OptionsDialog* d = make();
        d->setDrawing(u"Nha-o-2-tang-ban-ve-kien-truc.dwg"_s, sysvars_.get());
        d->setProfile(u"Công ty · Phòng Kiến trúc"_s); // i18n-ok: tên hồ sơ do người dùng đặt
        QVERIFY(show(d));
        d->page(Tab::Display)->findChild<QCheckBox*>(u"tooltips"_s)->click(); // nháp: bỏ chọn
        const QString before = tabIndex(tabList(d), 0).data().toString();
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QVERIFY(spinUntil([&] { return tabIndex(tabList(d), 0).data().toString() != before; }));
        QCoreApplication::processEvents();
        QVERIFY(!d->page(Tab::Display)->findChild<QCheckBox*>(u"tooltips"_s)->isChecked()); // dựng lại từ nháp
        for (const theme::Density density : {theme::Density::Comfortable, theme::Density::Compact}) {
            tm_->setDensity(density);
            gate_->setEdition(Edition::Pro);
            for (const Tab t : {Tab::Display, Tab::OpenSave, Tab::CommandLine, Tab::System}) {
                d->setCurrentTab(t);
                QCoreApplication::processEvents();
                QCOMPARE(truncations(d), QString());
            }
            gate_->setEdition(Edition::Viewer);
            d->setCurrentTab(Tab::Selection);
            typeSearch(d, u"@@"_s); // dòng "không thấy"
            QCoreApplication::processEvents();
            QCOMPARE(truncations(d), QString());
            d->search()->clear();
        }
        QVERIFY(d->width() < 1366); // vẫn vừa màn hình 1366 × 768
    }

    // Màu (ảnh không chữ, mọi nền tảng), Tối và Sáng (Sáng qua xem trước của chính hộp): nền danh sách tab --c-sunken,
    // nét chia --c-border, tab đang mở --c-accent-soft với vạch --c-accent, hàng kết quả tìm --c-accent-soft trên nền
    // thẻ.
    void pixels()
    {
        OptionsDialog* d = make();
        QVERIFY(show(d));
        typeSearch(d, u"lite"_s); // hàng "Giao diện nhẹ" của Hiển thị
        QCOMPARE(d->highlightedRow(), u"lite"_s);
        for (const bool light : {false, true}) {
            if (light) // Sáng: chọn đoạn "Sáng" của hàng chủ đề (xem trước)
                QTest::mouseClick(d->page(Tab::Display)->row(u"theme")->findChild<ui::SegmentedGroup*>()->segment(2),
                                  Qt::LeftButton);
            QCOMPARE(tm_->mode().light, light);
            QCoreApplication::processEvents();
            const QImage img = d->grab().toImage();
            const auto at = [&](QWidget* w, QPoint p) { return img.pixelColor(w->mapTo(d, p)).rgb(); };
            const auto over = [](const QColor& fg, const QColor& bg) {
                const int a = fg.alpha();
                const auto mix = [a](int f, int b) { return (f * a + b * (255 - a) + 127) / 255; };
                return QColor(mix(fg.red(), bg.red()), mix(fg.green(), bg.green()), mix(fg.blue(), bg.blue())).rgb();
            };
            const auto col = [this](tok::ColorId id) { return tm_->color(id); };
            QListView* list = tabList(d);
            const QRect item = list->visualRect(list->currentIndex());
            QWidget* vp = list->viewport();
            QCOMPARE(at(list, QPoint(list->width() / 2, list->height() - 3)), col(tok::c_sunken).rgb());
            QCOMPARE(at(list, QPoint(list->width(), list->height() / 2)), col(tok::c_border).rgb());
            // Phủ màu trong suốt: Qt làm tròn khác công thức trên tối đa 1 mỗi kênh.
            const auto near = [](QRgb a, QRgb b) {
                return qAbs(qRed(a) - qRed(b)) <= 1 && qAbs(qGreen(a) - qGreen(b)) <= 1 &&
                       qAbs(qBlue(a) - qBlue(b)) <= 1;
            };
            QVERIFY(near(at(vp, QPoint(item.right() - 12, item.center().y())),
                         over(col(tok::c_accent_soft), col(tok::c_sunken))));
            QCOMPARE(at(vp, QPoint(item.left() + 6, item.center().y())), col(tok::c_accent).rgb());
            QWidget* row = d->page(Tab::Display)->row(u"lite");
            QVERIFY(near(at(row, QPoint(row->width() - 4, row->height() / 2)),
                         over(col(tok::c_accent_soft), col(tok::c_base))));
        }
        d->reject();
        QVERIFY(!tm_->mode().light);
    }

    // Rảnh: không timer nào của hộp chạy định kỳ.
    void idle()
    {
        OptionsDialog* d = make();
        QVERIFY(show(d));
        typeSearch(d, u"autosave"_s);
        // Dựng sẵn các tab sau khung đầu là chuỗi hẹn một lần, tự dừng: rồi không còn timer nào chạy.
        settle(d);
        for (int i = 0; i < options::kTabCount; ++i)
            QVERIFY(d->tabContent(Tab(i))); // mọi tab đã dựng sẵn
        spinFor(50ms);
        for (QTimer* t : d->findChildren<QTimer*>())
            QVERIFY2(!t->isActive(), qPrintable(t->objectName()));
    }
};

QTEST_MAIN(OptionsTest)
#include "options_test.moc"
