// UI0-011 · Cổng phiên bản trong UI (EditionGate.h, LockBadge.h). Mỗi tiêu chí nghiệm thu một nhóm hàm test:
//   1  capabilityTable: bảng khả năng khớp 07.1 §2 chép tay ở đây (6 phiên bản × 11 dòng), minimumEdition, UiProfile.
//      actionLocks: với 6 phiên bản, mọi QAction của ActionRegistry: locked(), TipInfo::need (huy hiệu mục menu,
//      tooltip, trợ năng), bấm có tới chỗ nối INT-002 hay thành yêu cầu nâng cấp. Bảng kiểm sinh từ bảng khả năng
//      theo cờ phiên bản của hành động, không liệt kê tay từng lệnh.
//   2  upgradeOncePerSession: bấm mục khóa không gửi lệnh, gửi UpgradeRequest kèm mã tính năng; trang mở tối đa 1 lần
//      mỗi tính năng mỗi phiên, lần sau chú thích; open() bỏ giới hạn; UI-L04 đăng ký; mở trang web thất bại.
//   3  liveChange: dùng thử hết hạn → Viewer, kích hoạt Pro, lên Doanh nghiệp: QAction, mục menu (trợ năng), thanh
//      công cụ, LockBadge đổi ngay trên cùng đối tượng; profileChanged đúng 1 lần mỗi lần đổi hồ sơ; đổi ≤ 500 ms
//      (chỉ kiểm ở release, tests/timing.h).
//   4  accessibility: huy hiệu có chữ và ổ khóa (điểm ảnh màu chữ trên nền huy hiệu), trình đọc màn hình đọc
//      "Có trong TrionCAD Pro".
//   DoD-U: badges (ảnh baseline Tối/Sáng × 100/150/200% theo nền tảng; ghi lại: TRION_UPDATE_BASELINE=<thư mục>
//   chạy test, chép badges-*-<nền tảng>.png vào tests/baseline), pseudoNoTruncation (vi-XL), commandNotice.
//   bindLifetime, orphanBadge: gỡ cổng khi EditionGate hủy; huy hiệu chưa có cha không tự mở thành cửa sổ.
//   editionBadge: huy hiệu phiên bản EditionBadge (chữ, màu theo phiên bản, offline, theo cổng, vi-XL).
#include "app/core/edition/EditionBadge.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/edition/LockBadge.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/keymap/Keymap.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"
#include "tests/timing.h"

#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QElapsedTimer>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QTemporaryDir>
#include <QToolBar>
#include <QtTest>
#include <QUrlQuery>

#include <algorithm>
#include <iterator>
#include <memory>

using namespace Qt::StringLiterals;
using namespace trion;
using namespace trion::edition;
using trion::test::kTimed;

namespace {

constexpr const char* kEditionNames[] = {"Viewer", "ViewerPro", "Pro", "Trial", "Education", "Enterprise"};
constexpr Edition kEditions[] = {Edition::Viewer, Edition::ViewerPro, Edition::Pro,
                                 Edition::Trial,  Edition::Education, Edition::Enterprise};

// 07.1 §2 chép tay, không suy từ mã. Cột theo bảng: Viewer, Viewer Pro, Pro, Doanh nghiệp, Giáo dục; Dùng thử = Pro
// (PLF-002 tiêu chí 1). 'x' có, '-' không.
struct SpecRow {
    Capability cap;
    const char* cols;
};
constexpr SpecRow kSpec[] = {
    {Capability::View, "xxxxx"},        // mở, xem DWG/DXF; layout, viewport; zoom/pan
    {Capability::Measure, "xxxxx"},     // đo
    {Capability::LayerToggle, "xxxxx"}, // bật/tắt layer tạm thời
    {Capability::Print, "xxxxx"},       // in và xuất PDF từng layout
    {Capability::FontView, "xxxxx"},    // Bác sĩ Font: hiển thị đúng
    {Capability::FontConvert, "-xxxx"}, // Bác sĩ Font: chuyển đổi và lưu bản sao, chạy hàng loạt
    {Capability::Markup, "-xxxx"},      // markup lưu ra tệp ghi chú riêng
    {Capability::BatchTools, "-xxxx"}, // in hàng loạt theo khung tên; so sánh; đếm block; xuất bảng thống kê
    {Capability::Edit, "--xxx"},       // sửa bản vẽ, lưu DWG/DXF
    {Capability::Scripting, "--xxx"}, // LISP, script, plugin
    {Capability::Fleet, "---x-"},     // giấy phép floating, cổng quản trị, cài đặt hàng loạt
};

bool specHas(Edition e, Capability c)
{
    int col = 0;
    switch (e) {
    case Edition::Viewer:
        col = 0;
        break;
    case Edition::ViewerPro:
        col = 1;
        break;
    case Edition::Pro:
    case Edition::Trial:
        col = 2;
        break;
    case Edition::Enterprise:
        col = 3;
        break;
    case Edition::Education:
        col = 4;
        break;
    }
    for (const SpecRow& r : kSpec)
        if (r.cap == c)
            return r.cols[col] == 'x';
    return false;
}

// Dòng bảng mà cờ phiên bản của hành động đại diện (danh mục lệnh: V xem, đo, in; VP công cụ Viewer Pro; PRO sửa,
// LISP). Hành động khóa khi phiên bản thiếu các dòng đó; các dòng của một cờ phải cùng cột, không thì cờ mơ hồ.
QList<Capability> rowsOf(ui::Edition need)
{
    switch (need) {
    case ui::Edition::ViewerPro:
        return {Capability::FontConvert, Capability::Markup, Capability::BatchTools};
    case ui::Edition::Pro:
        return {Capability::Edit, Capability::Scripting};
    case ui::Edition::None:
        break;
    }
    return {Capability::View, Capability::Measure, Capability::LayerToggle, Capability::Print, Capability::FontView};
}

double ms(qint64 ns)
{
    return double(ns) / 1e6;
}

// Sai khác ≤ tol/255 mỗi kênh coi là trùng (khử răng cưa).
bool sameColor(QColor a, QColor b, int tol = 8)
{
    return std::abs(a.red() - b.red()) <= tol && std::abs(a.green() - b.green()) <= tol &&
           std::abs(a.blue() - b.blue()) <= tol;
}

// Màu token (có thể trong suốt, như --c-warn-soft) phủ lên nền.
QColor over(QColor bg, QColor fg)
{
    const double a = fg.alphaF();
    return QColor::fromRgbF(float(fg.redF() * a + bg.redF() * (1 - a)), float(fg.greenF() * a + bg.greenF() * (1 - a)),
                            float(fg.blueF() * a + bg.blueF() * (1 - a)));
}

#if defined(Q_OS_WIN)
constexpr auto kPlatform = "windows";
#elif defined(Q_OS_MACOS)
constexpr auto kPlatform = "macos";
#else
constexpr auto kPlatform = "linux";
#endif
constexpr int kNoise = 8;
constexpr int kCellW = 80, kCellH = 20, kInset = 4;
constexpr LockBadge::Style kStyles[] = {LockBadge::Style::Tag, LockBadge::Style::Ribbon};
constexpr ui::Edition kNeeds[] = {ui::Edition::Pro, ui::Edition::ViewerPro};

} // namespace

class EditionTest : public QObject {
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
    std::unique_ptr<actions::ActionRegistry> reg_;
    QStringList sent_; // chỗ nối INT-002

    // Huy hiệu vẽ trên nền --c-base, ô (kiểu, bản cần) như tấm ảnh baseline.
    QImage sheet(qreal dpr) const
    {
        QImage img(QSize(kCellW * 2, kCellH * 2) * dpr, QImage::Format_ARGB32_Premultiplied);
        img.setDevicePixelRatio(dpr);
        img.fill(tm_->color(tok::c_base));
        QPainter p(&img);
        for (int s = 0; s < 2; ++s)
            for (int n = 0; n < 2; ++n) {
                const QSize size = LockBadge::size(kNeeds[n], kStyles[s]);
                LockBadge::paint(p, QRect(QPoint(n * kCellW + kInset, s * kCellH + (kCellH - size.height()) / 2), size),
                                 kNeeds[n], kStyles[s]);
            }
        return img;
    }

    static QRect cell(int style, int need, const QImage& img)
    {
        const QSize s = LockBadge::size(kNeeds[need], kStyles[style]);
        const qreal dpr = img.devicePixelRatio();
        const QRect logical(QPoint(need * kCellW + kInset, style * kCellH + (kCellH - s.height()) / 2), s);
        return {QPoint(qRound(logical.x() * dpr), qRound(logical.y() * dpr)), logical.size() * dpr};
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        executor_ = std::make_unique<ui::UiExecutor>();
        QVERIFY(theme::loadFonts()); // chữ Plex nhúng: ảnh không phụ thuộc font của máy
        tm_ = std::make_unique<theme::ThemeManager>(*store_);
        icons_ = std::make_unique<theme::IconProvider>();
        translator_ = std::make_unique<i18n::Translator>();
        keymap_ = std::make_unique<keymap::Keymap>(*store_, keymap::Platform::Windows);
        tips_ = std::make_unique<ui::RichTooltip>();
        tips_->setKeymap(keymap_.get());
        sysvars_ = std::make_unique<actions::Sysvars>();
        reg_ = std::make_unique<actions::ActionRegistry>(*keymap_, *sysvars_);
        reg_->setSink([this](const QString& m) { sent_ << m; });
    }

    void init()
    {
        tm_->setTheme(theme::Theme::Dark);
        sent_.clear();
    }

    void cleanupTestCase()
    {
        reg_.reset();
        sysvars_.reset();
        tips_.reset();
        keymap_.reset();
        translator_.reset();
        icons_.reset();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        sched::waitIdle();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    // Tiêu chí 1: dữ liệu khả năng (tạm thay PLF-002) khớp từng ô của bảng 07.1 §2.
    void capabilityTable()
    {
        QCOMPARE(int(std::size(kSpec)), kCapabilityCount);
        for (Edition e : kEditions)
            for (const SpecRow& r : kSpec)
                QVERIFY2(EditionGate::has(e, r.cap) == specHas(e, r.cap),
                         qPrintable(u"phiên bản %1, dòng %2"_s.arg(int(e)).arg(int(r.cap))));
        for (const SpecRow& r : kSpec) { // bản thấp nhất có khả năng
            const Edition min = *std::find_if(std::begin(kEditions), std::end(kEditions), [&](Edition e) {
                return e != Edition::Trial && e != Edition::Education && specHas(e, r.cap);
            });
            QCOMPARE(EditionGate::minimumEdition(r.cap), min);
        }
        for (Edition e : kEditions) // Viewer, Viewer Pro không sửa bản vẽ → giao diện Viewer
            QCOMPARE(EditionGate::profile(e), specHas(e, Capability::Edit) ? UiProfile::Full : UiProfile::Viewer);

        EditionGate gate;
        QCOMPARE(EditionGate::instance(), &gate);
        QCOMPARE(gate.edition(), Edition::Viewer); // chưa có giấy phép: Viewer
        QCOMPARE(gate.profile(), UiProfile::Viewer);
        QVERIFY(!gate.has(Capability::Edit));
        QVERIFY(!gate.locked(ui::Edition::None));
        gate.setEdition(Edition::Education);
        QVERIFY(gate.has(Capability::Scripting));
        QVERIFY(!gate.has(Capability::Fleet));
    }

    // Tiêu chí 1: 6 phiên bản × mọi QAction.
    void actionLocks()
    {
        EditionGate gate;
        gate.bind(*reg_);
        QList<UpgradeRequest> requests; // trang (UI-L04 giả) và chú thích đều là yêu cầu nâng cấp
        gate.upgrade().setHandler([&](const UpgradeRequest& r) { requests << r; });
        connect(&gate.upgrade(), &UpgradeRouter::noticeRequested, &gate,
                [&](const UpgradeRequest& r) { requests << r; });

        qsizetype cells = 0;
        QStringList summary;
        for (Edition e : kEditions) {
            gate.setEdition(e);
            int locked = 0;
            for (QAction* a : reg_->actions()) {
                const QString id = a->objectName();
                const ui::Edition need = reg_->need(id);
                const QList<Capability> rows = rowsOf(need);
                const bool has = specHas(e, rows.first());
                for (Capability c : rows)
                    QVERIFY2(specHas(e, c) == has, "cờ phiên bản mơ hồ: các dòng của cờ khác cột");
                const bool want = !has;
                const QByteArray where = (id + u" @ "_s + QString::number(int(e))).toUtf8();
                QVERIFY2(gate.locked(need) == want, where.constData());
                QVERIFY2(ui::RichTooltip::info(a).need == (want ? need : ui::Edition::None), where.constData());

                sent_.clear();
                requests.clear();
                a->trigger();
                QVERIFY2(sent_.size() == (want ? 0 : 1), where.constData()); // khóa: không tới INT-002
                QVERIFY2(requests.size() == (want ? 1 : 0), where.constData());
                if (want) {
                    QCOMPARE(requests[0].feature, id);
                    QCOMPARE(requests[0].need, need);
                    QCOMPARE(requests[0].name, ui::RichTooltip::name(a));
                }
                locked += want;
                ++cells;
            }
            summary << u"%1 %2"_s.arg(QLatin1StringView(kEditionNames[int(e)])).arg(locked);
        }
        QCOMPARE(cells, 6 * reg_->actions().size());
        qInfo("bảng kiểm %lld ô (6 phiên bản × %lld hành động); số hành động khóa: %s", qlonglong(cells),
              qlonglong(reg_->actions().size()), qPrintable(summary.join(u", "_s)));
    }

    // Tiêu chí 2.
    void upgradeOncePerSession()
    {
        EditionGate gate; // phiên mới: chưa mở trang nào; Viewer
        gate.bind(*reg_);
        UpgradeRouter& up = gate.upgrade();
        QList<QUrl> opened; // trang web giả thay QDesktopServices
        up.setUrlOpener([&](const QUrl& u) {
            opened << u;
            return true;
        });
        QList<UpgradeRequest> notices;
        connect(&up, &UpgradeRouter::noticeRequested, &gate, [&](const UpgradeRequest& r) { notices << r; });

        // Bấm LINE (Pro) lần đầu: không gửi lệnh, mở trang mua kèm mã tính năng
        QAction* line = reg_->action(u"LINE"_s);
        line->trigger();
        QCOMPARE(sent_, QStringList());
        QCOMPARE(opened.size(), 1);
        QCOMPARE(QUrlQuery(opened[0]).queryItemValue(u"feature"_s), u"LINE"_s);
        QCOMPARE(opened[0].fragment(), u"pro"_s);
        QCOMPARE(opened[0], UpgradeRouter::url({u"LINE"_s, ui::Edition::Pro, {}}));
        QVERIFY(notices.isEmpty());

        // Lần sau trong phiên (kể cả phím tắt, mục menu: cùng QAction): chú thích có liên kết, không mở trang
        line->trigger();
        line->trigger();
        QCOMPARE(opened.size(), 1);
        QCOMPARE(notices.size(), 2);
        QCOMPARE(notices[0].feature, u"LINE"_s);
        QCOMPARE(notices[0].need, ui::Edition::Pro);
        QCOMPARE(notices[0].name, u"Vẽ đoạn thẳng"_s); // i18n-ok: nhãn gốc của LINE trong danh mục
        QCOMPARE(UpgradeRouter::noticeText(notices[0]), u"Vẽ đoạn thẳng có trong TrionCAD Pro."_s); // i18n-ok: mẫu
        QCOMPARE(sent_, QStringList());

        // Tính năng khác: trang mở lần đầu của nó; Viewer Pro mở gói vp
        reg_->action(u"COUNT"_s)->trigger();
        QCOMPARE(opened.size(), 2);
        QCOMPARE(opened[1].fragment(), u"vp"_s);
        QCOMPARE(QUrlQuery(opened[1]).queryItemValue(u"feature"_s), u"COUNT"_s);

        // Gọi thẳng: request lần đầu Page, lần sau Notice; open() (liên kết "Xem") luôn mở
        const UpgradeRequest qselect{u"QSELECT"_s, ui::Edition::Pro, u"QSELECT"_s};
        QCOMPARE(up.request(qselect), UpgradeRouter::Outcome::Page);
        QCOMPARE(up.request(qselect), UpgradeRouter::Outcome::Notice);
        QVERIFY(up.open(qselect));
        QCOMPARE(opened.size(), 4);

        // UI-L04 đăng ký: nhận yêu cầu thay trang web, cùng giới hạn mỗi phiên
        QList<UpgradeRequest> pages;
        up.setHandler([&](const UpgradeRequest& r) { pages << r; });
        const UpgradeRequest calc{u"QUICKCALC"_s, ui::Edition::Pro, u"QUICKCALC"_s};
        QCOMPARE(up.request(calc), UpgradeRouter::Outcome::Page);
        QCOMPARE(up.request(calc), UpgradeRouter::Outcome::Notice);
        QCOMPARE(pages.size(), 1);
        QCOMPARE(pages[0].feature, u"QUICKCALC"_s);
        QCOMPARE(opened.size(), 4);
        up.setHandler({});

        // Mở trang web thất bại: chú thích; chưa tính là đã mở nên lần sau thử lại
        up.setUrlOpener([&](const QUrl& u) {
            opened << u;
            return false;
        });
        const UpgradeRequest markup{u"TCMARKUP"_s, ui::Edition::ViewerPro, u"TCMARKUP"_s};
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"^UpgradeRouter: .*TCMARKUP"_s));
        QCOMPARE(up.request(markup), UpgradeRouter::Outcome::Notice);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"^UpgradeRouter: .*TCMARKUP"_s));
        QVERIFY(!up.open(markup));
        QCOMPARE(opened.size(), 6);
        up.setUrlOpener([&](const QUrl& u) {
            opened << u;
            return true;
        });
        QCOMPARE(up.request(markup), UpgradeRouter::Outcome::Page);

        // Pro: không khóa, lệnh tới INT-002, không yêu cầu nâng cấp
        const qsizetype before = notices.size();
        gate.setEdition(Edition::Pro);
        line->trigger();
        QCOMPARE(sent_, QStringList{u"^C^C_LINE"_s});
        QCOMPARE(notices.size(), before);
        QCOMPARE(opened.size(), 7);
    }

    // Tiêu chí 3.
    void liveChange()
    {
        EditionGate gate;
        gate.upgrade().setHandler([](const UpgradeRequest&) {});
        gate.setEdition(Edition::Trial);
        gate.bind(*reg_);

        // Khung: mọi hành động trong một Menu và một thanh công cụ (đo, vẽ lại khi QAction đổi), huy hiệu trong bố cục
        QWidget window;
        auto* box = new QVBoxLayout(&window);
        auto* bar = new QToolBar(&window);
        ui::Menu menu(&window); // hủy trước window
        for (QAction* a : reg_->actions()) {
            menu.addAction(a);
            bar->addAction(a);
        }
        auto* badge = new LockBadge(ui::Edition::Pro, LockBadge::Style::Tag, &window);
        box->addWidget(bar);
        box->addWidget(badge);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(badge->isHidden()); // dùng thử = Pro

        QList<UiProfile> profiles;
        QList<Edition> editions;
        connect(&gate, &EditionGate::profileChanged, &window, [&](UiProfile p) { profiles << p; });
        connect(&gate, &EditionGate::editionChanged, &window, [&](Edition e) { editions << e; });

        QAction* line = reg_->action(u"LINE"_s);
        QAccessibleInterface* menuAcc = QAccessible::queryAccessibleInterface(&menu);
        QVERIFY(menuAcc);
        const auto lineDescription = [&] {
            return menuAcc->child(int(menu.actions().indexOf(line)))->text(QAccessible::Description);
        };
        QCOMPARE(lineDescription(), QString());

        const auto change = [&](Edition e) {
            QElapsedTimer clock;
            clock.start();
            gate.setEdition(e);
            QCoreApplication::processEvents(); // vẽ lại thanh công cụ, huy hiệu
            return ms(clock.nsecsElapsed());
        };

        // Dùng thử hết hạn → Viewer: khóa, huy hiệu, trợ năng; hồ sơ Viewer (bản vẽ chỉ xem)
        const double toViewer = change(Edition::Viewer);
        QCOMPARE(editions, QList<Edition>{Edition::Viewer});
        QCOMPARE(profiles, QList<UiProfile>{UiProfile::Viewer});
        QCOMPARE(ui::RichTooltip::info(line).need, ui::Edition::Pro);
        QVERIFY(lineDescription().contains(ui::RichTooltip::editionName(ui::Edition::Pro))); // "Khóa, cần bản Pro"
        QVERIFY(badge->isVisible());
        QCOMPARE(reg_->action(u"LINE"_s), line); // cùng đối tượng, không dựng lại

        // Kích hoạt Pro: mở khóa ngay, hồ sơ đầy đủ
        const double toPro = change(Edition::Pro);
        QCOMPARE(profiles, (QList<UiProfile>{UiProfile::Viewer, UiProfile::Full}));
        QCOMPARE(ui::RichTooltip::info(line).need, ui::Edition::None);
        QCOMPARE(lineDescription(), QString());
        QVERIFY(badge->isHidden());
        sent_.clear();
        line->trigger();
        QCOMPARE(sent_, QStringList{u"^C^C_LINE"_s});

        // Pro → Doanh nghiệp: phiên bản đổi, hồ sơ không; đặt lại cùng phiên bản: không phát gì
        change(Edition::Enterprise);
        change(Edition::Enterprise);
        QCOMPARE(editions, (QList<Edition>{Edition::Viewer, Edition::Pro, Edition::Enterprise}));
        QCOMPARE(profiles.size(), 2);

        qInfo("đổi hồ sơ (%lld QAction, Menu + thanh công cụ): Dùng thử → Viewer %.2f ms, Viewer → Pro %.2f ms",
              qlonglong(reg_->actions().size()), toViewer, toPro);
        QVERIFY2(!kTimed || std::max(toViewer, toPro) <= 500, "đổi hồ sơ quá 500 ms");
    }

    // Tiêu chí 4.
    void accessibility()
    {
        EditionGate gate; // Viewer: huy hiệu hiện
        QWidget host;
        auto* box = new QHBoxLayout(&host);
        auto* pro = new LockBadge(ui::Edition::Pro, LockBadge::Style::Ribbon, &host);
        auto* vp = new LockBadge(ui::Edition::ViewerPro, LockBadge::Style::Tag, &host);
        box->addWidget(pro);
        box->addWidget(vp);
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        QVERIFY(pro->isVisible() && vp->isVisible());

        QAccessibleInterface* a = QAccessible::queryAccessibleInterface(pro);
        QVERIFY(a);
        QCOMPARE(a->role(), QAccessible::StaticText);
        QCOMPARE(a->text(QAccessible::Name), u"Có trong TrionCAD Pro"_s); // i18n-ok: câu đọc của 07.1 §2
        QCOMPARE(QAccessible::queryAccessibleInterface(vp)->text(QAccessible::Name),
                 u"Có trong TrionCAD Viewer Pro"_s); // i18n-ok: câu đọc
        QCOMPARE(pro->need(), ui::Edition::Pro);
        QCOMPARE(pro->style(), LockBadge::Style::Ribbon);

        // Không chỉ màu: trên nền huy hiệu có điểm ảnh màu chữ (chữ "Pro", "VP"…) và, ở kiểu ribbon, ổ khóa trước chữ
        for (qreal dpr : {1.0, 2.0}) {
            const QImage img = sheet(dpr);
            for (int s = 0; s < 2; ++s)
                for (int n = 0; n < 2; ++n) {
                    const bool isVp = kNeeds[n] == ui::Edition::ViewerPro;
                    const QColor ink = tm_->color(isVp ? tok::c_ed_vp_t : tok::c_ed_pro_t);
                    const QRect r = cell(s, n, img);
                    QVERIFY(sameColor(img.pixelColor(r.left() + qRound(dpr), r.center().y()),
                                      tm_->color(isVp ? tok::c_ed_vp : tok::c_ed_pro)));
                    const int lockEnd = kStyles[s] == LockBadge::Style::Ribbon
                                            ? r.left() + qRound((tm_->px(tok::sp_1) + tm_->px(tok::sp_4)) * dpr)
                                            : r.left();
                    int glyph = 0, text = 0;
                    for (int y = r.top(); y <= r.bottom(); ++y)
                        for (int x = r.left(); x <= r.right(); ++x)
                            if (sameColor(img.pixelColor(x, y), ink))
                                ++(x < lockEnd ? glyph : text);
                    QVERIFY2(text >= 4 * dpr * dpr, "huy hiệu thiếu chữ");
                    if (kStyles[s] == LockBadge::Style::Ribbon)
                        QVERIFY2(glyph >= 4 * dpr * dpr, "huy hiệu ribbon thiếu ổ khóa");
                }
        }
    }

    // DoD-U 1: ảnh baseline theo nền tảng.
    void badges_data()
    {
        QTest::addColumn<bool>("light");
        QTest::addColumn<qreal>("dpr");
        for (bool light : {false, true})
            for (qreal dpr : {1.0, 1.5, 2.0})
                QTest::addRow("%s-%d", light ? "light" : "dark", qRound(dpr * 100)) << light << dpr;
    }

    void badges()
    {
        QFETCH(bool, light);
        QFETCH(qreal, dpr);
        tm_->setTheme(light ? theme::Theme::Light : theme::Theme::Dark);
        const QImage img = sheet(dpr);
        // Hai kiểu cùng cao (dòng chữ huy hiệu); không khóa thì không có huy hiệu
        for (ui::Edition need : kNeeds)
            QCOMPARE(LockBadge::size(need, LockBadge::Style::Ribbon).height(),
                     LockBadge::size(need, LockBadge::Style::Tag).height());
        QCOMPARE(LockBadge::size(ui::Edition::None, LockBadge::Style::Ribbon), QSize());

        const QString name = u"badges-%1-%2-%3.png"_s.arg(light ? u"light"_s : u"dark"_s)
                                 .arg(qRound(dpr * 100))
                                 .arg(QLatin1StringView(kPlatform));
        if (const QString out = qEnvironmentVariable("TRION_UPDATE_BASELINE"); !out.isEmpty()) {
            QVERIFY(img.save(out + u'/' + name, "PNG", 0));
            QSKIP("đã ghi baseline");
        }
        const QImage want = QImage(u":/baseline/"_s + name).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QVERIFY2(!want.isNull(), qPrintable(u"thiếu baseline "_s + name));
        QCOMPARE(img.size(), want.size());
        qsizetype off = 0;
        int maxDiff = 0;
        for (int y = 0; y < img.height(); ++y) {
            const auto* a = reinterpret_cast<const QRgb*>(img.constScanLine(y));
            const auto* b = reinterpret_cast<const QRgb*>(want.constScanLine(y));
            for (int x = 0; x < img.width(); ++x) {
                const int d = std::max({std::abs(qRed(a[x]) - qRed(b[x])), std::abs(qGreen(a[x]) - qGreen(b[x])),
                                        std::abs(qBlue(a[x]) - qBlue(b[x])), std::abs(qAlpha(a[x]) - qAlpha(b[x]))});
                maxDiff = std::max(maxDiff, d);
                off += d > kNoise;
            }
        }
        qInfo("%s: %lld điểm ảnh lệch > %d/255, lệch lớn nhất %d/255", qPrintable(name), qint64(off), kNoise, maxDiff);
        if (off > img.width() * img.height() / 1000)
            img.save(QDir::temp().filePath(u"got-"_s + name)); // để so bằng mắt
        QVERIFY(off <= img.width() * img.height() / 1000);
    }

    // DoD-U 3: vi-XL không cắt chữ; huy hiệu dịch lại khi đổi ngôn ngữ.
    void pseudoNoTruncation()
    {
        EditionGate gate; // Viewer: mọi huy hiệu hiện
        auto* early = new LockBadge(ui::Edition::ViewerPro, LockBadge::Style::Tag);
        const std::unique_ptr<LockBadge> keep(early);
        const int viWidth = early->sizeHint().width();
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QCoreApplication::processEvents();
        QVERIFY(early->sizeHint().width() > viWidth); // LanguageChange: chữ, cỡ, tên trợ năng mới
        QCOMPARE(early->text(), LockBadge::label(ui::Edition::ViewerPro, LockBadge::Style::Tag));
        QCOMPARE(early->accessibleName(), EditionGate::availableIn(ui::Edition::ViewerPro));

        QWidget host;
        auto* box = new QVBoxLayout(&host);
        auto* row = new QHBoxLayout;
        for (LockBadge::Style s : kStyles)
            for (ui::Edition n : kNeeds)
                row->addWidget(new LockBadge(n, s, &host));
        row->addStretch();
        box->addLayout(row);
        const EditionGate::CommandNotice notice = EditionGate::commandNotice(u"LINE"_s, ui::Edition::Pro);
        auto* cmd = new QHBoxLayout; // dòng lệnh: câu và hai chip (UIA-007)
        cmd->addWidget(new QLabel(notice.text, &host));
        cmd->addWidget(new QPushButton(notice.trial, &host));
        cmd->addWidget(new QPushButton(notice.learn, &host));
        cmd->addStretch();
        box->addLayout(cmd);
        box->addWidget(new QLabel(UpgradeRouter::noticeText({u"LINE"_s, ui::Edition::Pro, notice.learn}), &host));
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        const QList<i18n::Truncation> cut = i18n::findTruncations(&host);
        for (const i18n::Truncation& t : cut)
            qWarning("%s", qUtf8Printable(t.toString()));
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QCoreApplication::processEvents();
        QCOMPARE(cut.size(), 0);
        QCOMPARE(early->sizeHint().width(), viWidth);
    }

    // Huy hiệu phiên bản (EditionBadge): chữ và nền theo phiên bản (.ed[data-ed]); lâu chưa xác minh giấy phép: thêm số
    // ngày offline, màu .ed.warn, trừ Viewer; đổi ngay theo cổng; trình đọc màn hình đọc chữ; vi-XL không cắt chữ.
    void editionBadge()
    {
        EditionGate gate;
        QSignalSpy status(&gate, &EditionGate::statusChanged);
        gate.setTrialDays(12);
        gate.setTrialDays(12); // trùng: không phát
        QCOMPARE(status.count(), 1);

        QWidget host;
        auto* row = new QHBoxLayout(&host);
        auto* badge = new EditionBadge(gate, &host);
        row->addWidget(badge);
        row->addStretch();
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));

        // Nền ở mép trái giữa chiều cao (token trong suốt phủ lên --c-base của cửa sổ); có chữ: điểm ảnh gần màu chữ
        // hơn hẳn màu nền (nét chữ mảnh như "Viewer" ít điểm ảnh phủ kín) — không chỉ là màu.
        const auto check = [&](tok::ColorId bg, tok::ColorId fg) {
            QCoreApplication::processEvents(); // LayoutRequest: cỡ theo chữ mới
            QCOMPARE(badge->size(), EditionBadge::sizeFor(gate));
            const QImage img = badge->grab().toImage();
            const QColor got = img.pixelColor(2, img.height() / 2);
            const QColor want = over(tm_->color(tok::c_base), tm_->color(bg));
            QVERIFY2(sameColor(got, want), qPrintable(badge->text() + u": "_s + got.name() + u" ≠ "_s + want.name()));
            const QColor ink = tm_->color(fg);
            const int contrast = std::max({std::abs(ink.red() - want.red()), std::abs(ink.green() - want.green()),
                                           std::abs(ink.blue() - want.blue())});
            int strokes = 0;
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x)
                    strokes += sameColor(img.pixelColor(x, y), ink, contrast / 3);
            QVERIFY2(strokes >= 4, qPrintable(badge->text() + u": thiếu chữ màu "_s + ink.name()));
        };
        struct Case {
            Edition edition;
            QString text;
            tok::ColorId bg, fg;
        };
        const Case cases[] = {
            {Edition::Viewer, u"Viewer"_s, tok::c_ed_viewer, tok::c_ed_viewer_t},
            {Edition::ViewerPro, u"Viewer Pro"_s, tok::c_ed_vp, tok::c_ed_vp_t},
            {Edition::Pro, u"Pro"_s, tok::c_ed_pro, tok::c_ed_pro_t},
            {Edition::Trial, u"Dùng thử: còn 12 ngày"_s, tok::c_ed_trial, tok::c_ed_trial_t}, // i18n-ok: chữ huy hiệu
            {Edition::Education, u"Giáo dục"_s, tok::c_ed_edu, tok::c_ed_edu_t},                // i18n-ok: chữ huy hiệu
            {Edition::Enterprise, u"Doanh nghiệp"_s, tok::c_ed_ent, tok::c_ed_ent_t},           // i18n-ok: chữ huy hiệu
        };
        for (const theme::Theme t : {theme::Theme::Dark, theme::Theme::Light}) {
            tm_->setTheme(t);
            for (const Case& c : cases) {
                gate.setEdition(c.edition);
                QCOMPARE(badge->text(), c.text);
                QCOMPARE(QAccessible::queryAccessibleInterface(badge)->text(QAccessible::Name), c.text);
                check(c.bg, c.fg);
                gate.setOfflineDays(3);
                if (c.edition == Edition::Viewer) { // Viewer không cần xác minh giấy phép
                    QCOMPARE(badge->text(), c.text);
                } else {
                    QCOMPARE(badge->text(), c.text + u" · còn 3 ngày offline"_s); // i18n-ok: .ed.warn
                    check(tok::c_warn_soft, tok::c_warn);
                }
                gate.setOfflineDays(-1);
            }
        }
        QCOMPARE(status.count(), 1 + 2 * 2 * kEditionCount);

        gate.setEdition(Edition::Trial);
        gate.setOfflineDays(3);
        const QString vi = badge->text();
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QCoreApplication::processEvents(); // LanguageChange đăng hàng đợi
        QCOMPARE(badge->text(), EditionBadge::label(gate));
        QVERIFY(badge->text() != vi);
        QCoreApplication::processEvents();
        const QList<i18n::Truncation> cut = i18n::findTruncations(&host);
        for (const i18n::Truncation& c : cut)
            qWarning("%s", qUtf8Printable(c.toString()));
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QCoreApplication::processEvents();
        QCOMPARE(cut.size(), 0);
        QCOMPARE(badge->text(), vi);
    }

    void commandNotice()
    {
        const EditionGate::CommandNotice n = EditionGate::commandNotice(u"LINE"_s, ui::Edition::Pro);
        QCOMPARE(n.text, u"Lệnh LINE có trong TrionCAD Pro."_s); // i18n-ok: câu của 07.1 §2
        QCOMPARE(n.trial, u"Dùng thử 30 ngày"_s);                // i18n-ok: chip
        QCOMPARE(n.learn, u"Tìm hiểu"_s);                        // i18n-ok: chip
        QCOMPARE(EditionGate::commandNotice(u"COUNT"_s, ui::Edition::ViewerPro).text,
                 u"Lệnh COUNT có trong TrionCAD Viewer Pro."_s); // i18n-ok: câu của 07.1 §2
        QCOMPARE(EditionGate::availableIn(ui::Edition::None), QString());
        QCOMPARE(LockBadge::label(ui::Edition::ViewerPro, LockBadge::Style::Ribbon), u"VP"_s);
        QCOMPARE(LockBadge::label(ui::Edition::ViewerPro, LockBadge::Style::Tag), u"Viewer Pro"_s);
        QCOMPARE(LockBadge::label(ui::Edition::Pro, LockBadge::Style::Ribbon), u"Pro"_s);
    }

    void bindLifetime()
    {
        {
            EditionGate gate;
            gate.upgrade().setHandler([](const UpgradeRequest&) {});
            gate.bind(*reg_);
            gate.bind(*reg_); // bind lại: vẫn một cổng
            reg_->action(u"LINE"_s)->trigger();
            QCOMPARE(sent_, QStringList());
            {
                EditionGate inner;
                QCOMPARE(EditionGate::instance(), &inner);
            }
            QCOMPARE(EditionGate::instance(), nullptr); // gần nhất đã hủy
        }
        reg_->action(u"LINE"_s)->trigger(); // EditionGate đã hủy: cổng gỡ khỏi registry
        QCOMPARE(sent_, QStringList{u"^C^C_LINE"_s});
    }

    void orphanBadge()
    {
        EditionGate gate; // Viewer: khóa
        QWidget host;
        auto own = std::make_unique<LockBadge>(ui::Edition::Pro, LockBadge::Style::Tag);
        LockBadge* orphan = own.get();
        QVERIFY(!orphan->isVisible()); // chưa có cha: không tự mở thành cửa sổ
        gate.setEdition(Edition::Pro);
        gate.setEdition(Edition::Viewer);
        QVERIFY(!orphan->isVisible());
        (new QVBoxLayout(&host))->addWidget(own.release()); // vào bố cục: hiện cùng cha
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        QVERIFY(orphan->isVisible());
        gate.setEdition(Edition::Pro);
        QVERIFY(orphan->isHidden());
    }
};

QTEST_MAIN(EditionTest)
#include "edition_test.moc"
