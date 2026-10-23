// UIJ-001: banner Bác sĩ Font khi mở bản vẽ và chỉ báo trên thanh trạng thái (FontDoctorBanner, FontStatusChip,
// FontDoctorUi, FontDoctorUiSettings). VN-004 (quét font), vùng vẽ thật (RND), StatusBar nhận chip: chưa có. Cửa sổ mẫu
// 1366×768 với vùng vẽ giả (đếm cú nhấp), dòng lệnh giả (QLineEdit), dải trạng thái giả chứa chip; kết quả quét đưa vào
// qua chỗ nối scanStarted/scanProgress/scanFinished. Mỗi tiêu chí nghiệm thu một nhóm hàm test:
//   1  message: banner chỉ hiện khi quét xong; câu theo mẫu 07.3 (", 46 chữ VNI"), số dạng 1.250; phủ mép trên vùng vẽ.
//   2  actions: Xem chi tiết → paletteRequested(Summary); Chuyển sang Unicode ở Viewer: ổ khóa, huy hiệu Viewer Pro, mô
//      tả tiếp cận, bấm mở trang nâng cấp (UpgradeRouter), không phát Convert; Viewer Pro, Pro → Convert.
//   3  notBlocking: hiện banner không đổi focus, cửa sổ vẫn kích hoạt; bấm nút banner không lấy focus; vùng vẽ dưới
//      banner vẫn nhận chuột; phần việc của banner trên luồng UI mỗi lần mở bản vẽ (chỗ nối, cập nhật, bố cục banner
//      và chip) ≤ 2 ms, cộng cả vẽ khung đầu ≤ 1 khung (CPU luồng, release). Pan/zoom ≥ 60 fps trên BENCH-STD: Hoãn
//      (cần RND, vùng vẽ thật).
//   4  chipAfterClose: đóng banner (×), chế độ "chỉ hiện chỉ báo": chip "Font: 3 thay thế", tooltip nêu số font thay
//      thế và số chữ mã cũ, bấm chip → paletteRequested(Summary).
//   5  muteFile: "Không nhắc lại với tệp này" ghi đường dẫn chuẩn hóa ("/./", "..", NFD đều khớp), còn sau khởi động
//      lại (Store mới đọc cùng tệp); tệp đó vẫn có chip; đổi tên, chuyển thư mục: banner hiện lại; bản vẽ chưa lưu:
//      không có nút đó.
//   6  cleanAndScanning: không vấn đề: không banner, không chip; quét lâu: chip "Đang quét font" + tiến trình sau
//      kScanChipDelay, banner chờ quét xong; quét nhanh: chip không nháy.
//   7  perTab: mỗi tab banner riêng (đóng ở tab này không ảnh hưởng tab kia); chuyển tab đổi banner, chip ≤ 100 ms.
//   8  matchesScan (tới chỗ nối; kho 1.000 tệp QA-001: Hoãn, cần VN-004): 1.000 kết quả quét ngẫu nhiên, số trên banner
//      và chip khớp đúng số đưa vào.
//   DoD-U: pseudoNoTruncation (vi-XL ở 1366 px, vùng vẽ cả bề rộng và kẹp giữa hai palette dock, hai mật độ: không cắt
//   chữ, câu xuống dòng, nút xuống hàng), accessibility (vai trò StatusBar, tên, thông báo Polite; checkNames +
//   checkFocus 0 vi phạm; Tab tới nút; Esc đóng banner, focus về chỗ cũ), themes (nền, viền theo token Tối/Sáng),
//   stacking (ngay dưới ui::CanvasBanner), idle (rảnh không timer).
#include "app/vn/fontdoctor/FontDoctorBanner.h"
#include "app/vn/fontdoctor/FontDoctorUi.h"
#include "app/vn/fontdoctor/FontDoctorUiSettings.h"
#include "app/vn/fontdoctor/FontStatusChip.h"

#include "app/core/a11y/AccessibilityAudit.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/edition/LockBadge.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/i18n/UiFormat.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/sched/Scheduler.h"
#include "tests/timing.h"

#include <QAccessible>
#include <QApplication>
#include <QBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <functional>
#include <memory>
#include <random>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using namespace trion::fontdoctor;
using namespace trion::theme;
namespace edition = trion::edition;
namespace i18n = trion::i18n;
namespace sched = trion::sched;
namespace settings = trion::settings;
namespace ui = trion::ui;
namespace a11y = trion::a11y;
using trion::test::kTimed;
using trion::test::threadCpu;
using Clock = std::chrono::steady_clock;
using Tab = FontDoctorUi::Tab;

namespace {

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

// Vùng vẽ giả: đếm cú nhấp (banner không chặn thao tác ở phần còn lại của vùng vẽ).
class Canvas final : public QWidget {
public:
    using QWidget::QWidget;
    int presses = 0;

protected:
    void mousePressEvent(QMouseEvent*) override { ++presses; }
};

// Đếm lần widget hiện lên trong lúc điều kiện when() đúng.
class ShowWatch final : public QObject {
public:
    ShowWatch(QWidget* w, std::function<bool()> when) : when_(std::move(when)) { w->installEventFilter(this); }
    int count = 0;

protected:
    bool eventFilter(QObject*, QEvent* e) override
    {
        if (e->type() == QEvent::Show && when_())
            ++count;
        return false;
    }

private:
    std::function<bool()> when_;
};

// Bộ bắt sự kiện tiếp cận: thông báo (Announcement) của banner.
struct Announcement {
    QObject* source;
    QString message;
    QAccessible::AnnouncementPoliteness politeness;
};
std::vector<Announcement> g_said;

void recordA11y(QAccessibleEvent* e)
{
    if (e->type() == QAccessible::Announcement) {
        const auto* say = static_cast<QAccessibleAnnouncementEvent*>(e);
        g_said.push_back({e->object(), say->message(), say->politeness()});
    }
}

// Các số đếm (dạng 1.250) trong câu theo thứ tự; bỏ chữ số dính chữ cái như "TCVN3".
QList<qint64> numbersIn(const QString& s)
{
    static const QRegularExpression number(uR"((?<![\p{L}\d.])\d{1,3}(?:\.\d{3})*(?![\d\p{L}]))"_s);
    QList<qint64> out;
    for (auto it = number.globalMatch(s); it.hasNext();)
        out << it.next().captured().remove(u'.').toLongLong();
    return out;
}

// Nhãn câu của banner (QLabel xuống dòng; nhãn còn lại là tiêu đề "Bác sĩ Font:", huy hiệu phiên bản).
QLabel* sentence(QWidget* banner)
{
    for (QLabel* l : banner->findChildren<QLabel*>())
        if (l->wordWrap())
            return l;
    return nullptr;
}

// Các widget con đang hiện nằm trọn trong cha (không tràn ra ngoài banner).
bool childrenInside(QWidget* w)
{
    for (QWidget* c : w->findChildren<QWidget*>(Qt::FindDirectChildrenOnly))
        if (!c->isHidden() && !w->rect().contains(c->geometry()))
            return false;
    return true;
}

// Điểm ảnh của widget tự vẽ khớp màu token (so ở dạng nhân trước alpha: --c-warn-soft trong suốt 13%).
bool paints(QWidget* w, QPoint at, const QColor& token)
{
    QImage img(w->size(), QImage::Format_ARGB32_Premultiplied); // không nền palette (grab() vẽ nền trước)
    img.fill(Qt::transparent);
    w->render(&img, {}, {}, QWidget::DrawChildren);
    const QRgb got = img.pixel(at);
    const QRgb want = qPremultiply(token.rgba());
    return std::abs(qRed(got) - qRed(want)) <= 2 && std::abs(qGreen(got) - qGreen(want)) <= 2 &&
           std::abs(qBlue(got) - qBlue(want)) <= 2 && std::abs(qAlpha(got) - qAlpha(want)) <= 2;
}

} // namespace

class FontDoctorBannerTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<ThemeManager> tm_;
    std::unique_ptr<IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<ui::RichTooltip> tips_;
    std::unique_ptr<edition::EditionGate> gate_;
    std::unique_ptr<QMainWindow> window_; // vùng vẽ giả giữa hai chỗ palette dock, dòng lệnh, dải trạng thái giả
    Canvas* canvas_ = nullptr;
    QWidget* leftDock_ = nullptr;
    QWidget* rightDock_ = nullptr;
    QLineEdit* cmd_ = nullptr;
    QHBoxLayout* status_ = nullptr;
    QPointer<FontDoctorBanner> banner_;
    QPointer<FontStatusChip> chip_;
    std::unique_ptr<FontDoctorUi> fd_;
    QList<edition::UpgradeRequest> upgrades_; // trang nâng cấp đã mở (UpgradeRouter handler)

    QString path(const QString& name) const { return dir_.filePath(name); }

    void make(FontDoctorUiSettings settings)
    {
        banner_ = new FontDoctorBanner(canvas_, *gate_);
        chip_ = new FontStatusChip;
        status_->addWidget(chip_);
        fd_ = std::make_unique<FontDoctorUi>(*banner_, *chip_, settings);
    }

    void drop()
    {
        fd_.reset();
        delete banner_;
        delete chip_;
    }

    // Mở bản vẽ id (tab hiện hành) rồi VN-004 báo quét xong.
    void open(quint64 id, const QString& file, FontScan scan)
    {
        fd_->setCurrent(id);
        fd_->scanStarted(id, file);
        fd_->scanFinished(id, scan);
    }

    void focusCommandLine()
    {
        window_->activateWindow();
        cmd_->setFocus(Qt::OtherFocusReason);
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == cmd_; }));
    }

    void showDocks(bool on)
    {
        leftDock_->setVisible(on);
        rightDock_->setVisible(on);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        QVERIFY(loadFonts()); // chữ Plex nhúng: đo chữ không phụ thuộc font của máy
        executor_ = std::make_unique<ui::UiExecutor>();
        tm_ = std::make_unique<ThemeManager>(*store_);
        icons_ = std::make_unique<IconProvider>();
        translator_ = std::make_unique<i18n::Translator>();
        tips_ = std::make_unique<ui::RichTooltip>();
        gate_ = std::make_unique<edition::EditionGate>();
        gate_->upgrade().setHandler([this](const edition::UpgradeRequest& r) { upgrades_ << r; });

        window_ = std::make_unique<QMainWindow>();
        auto* central = new QWidget;
        auto* col = new QVBoxLayout(central);
        col->setContentsMargins({});
        col->setSpacing(0);
        auto* row = new QHBoxLayout;
        row->setSpacing(0);
        leftDock_ = new QWidget;
        rightDock_ = new QWidget;
        canvas_ = new Canvas;
        for (QWidget* dock : {leftDock_, rightDock_})
            dock->setFixedWidth(qRound(tm_->px(tok::pal_w))); // palette dock --pal-w mỗi bên
        row->addWidget(leftDock_);
        row->addWidget(canvas_, 1);
        row->addWidget(rightDock_);
        cmd_ = new QLineEdit;
        cmd_->setAccessibleName(u"Dòng lệnh"_s); // i18n-ok: dữ liệu thử
        auto* bar = new QWidget;
        status_ = new QHBoxLayout(bar);
        status_->setContentsMargins({});
        status_->addStretch(1);
        col->addLayout(row, 1);
        col->addWidget(cmd_);
        col->addWidget(bar);
        window_->setCentralWidget(central);
        window_->resize(1366, 768);
        window_->show();
        QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
        focusCommandLine();
    }

    void init()
    {
        tm_->setTheme(Theme::Dark);
        tm_->setDensity(Density::Compact);
        gate_->setEdition(edition::Edition::Viewer);
        store_->reset(kShowMode);
        store_->reset(kMutedFiles);
        upgrades_.clear();
        showDocks(false);
        make(FontDoctorUiSettings(*store_));
    }

    void cleanup()
    {
        drop();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        if (translator_->language() != u"vi"_s)
            QVERIFY(translator_->setLanguage(u"vi"_s));
        QCoreApplication::processEvents();
    }

    void cleanupTestCase()
    {
        window_.reset();
        gate_.reset();
        tips_.reset();
        translator_.reset();
        icons_.reset();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        sched::waitIdle();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    // Tiêu chí 1: banner hiện khi VN-004 báo quét xong, câu theo mẫu, số dạng 1.250, phủ mép trên vùng vẽ.
    void message()
    {
        fd_->setCurrent(1);
        fd_->scanStarted(1, path(u"a.dwg"_s));
        QVERIFY(banner_->isHidden()); // đang quét: banner chờ
        fd_->scanFinished(1, {3, 250, 0});
        QVERIFY(banner_->isVisible());
        QCOMPARE(banner_->message(), u"Bản vẽ dùng 3 font không có trên máy và 250 đối tượng chữ mã TCVN3. " // i18n-ok
                                     "TrionCAD đã tự hiển thị đúng."_s); // i18n-ok: mẫu 07.3 §1 UI-J01
        fd_->scanFinished(1, {3, 250, 46});
        QCOMPARE(banner_->message(), u"Bản vẽ dùng 3 font không có trên máy và 250 đối tượng chữ mã TCVN3, " // i18n-ok
                                     "46 chữ VNI. TrionCAD đã tự hiển thị đúng."_s); // i18n-ok: mẫu 07.3 §1 UI-J01
        fd_->scanFinished(1, {12, 1250, 1250000});
        QVERIFY2(banner_->message().contains(u"1.250 đối tượng chữ mã TCVN3, 1.250.000 chữ VNI"_s), // i18n-ok: mẫu
                 qPrintable(banner_->message()));
        QCOMPARE(FontDoctorBanner::compose(2, 0, 0),
                 u"Bản vẽ dùng 2 font không có trên máy. TrionCAD đã tự hiển thị đúng."_s); // i18n-ok: mẫu
        QCOMPARE(FontDoctorBanner::compose(0, 1250, 0),
                 u"Bản vẽ có 1.250 đối tượng chữ mã TCVN3. TrionCAD đã tự hiển thị đúng."_s); // i18n-ok: mẫu
        QCOMPARE(FontDoctorBanner::compose(0, 0, 46),
                 u"Bản vẽ có 46 đối tượng chữ mã VNI. TrionCAD đã tự hiển thị đúng."_s); // i18n-ok: mẫu
        QCOMPARE(FontDoctorBanner::compose(0, 0, 0), QString());

        QCOMPARE(banner_->geometry().topLeft(), QPoint(0, 0));
        QCOMPARE(banner_->width(), canvas_->width());
        QVERIFY(banner_->height() >= FontDoctorBanner::kMinHeight);
        QVERIFY(childrenInside(banner_));
        QVERIFY(banner_->accessibleName().endsWith(banner_->message()));
        QVERIFY(banner_->accessibleName().startsWith(u"Bác sĩ Font:"_s)); // i18n-ok: tiền tố đậm của prototype
    }

    // Tiêu chí 2: bốn nút; Chuyển sang Unicode theo phiên bản (Viewer: huy hiệu Viewer Pro, trang nâng cấp).
    void actions()
    {
        QSignalSpy palette(fd_.get(), &FontDoctorUi::paletteRequested);
        open(1, path(u"a.dwg"_s), {3, 250, 46});
        QCOMPARE(banner_->detailsButton()->text(), u"Xem chi tiết"_s);            // i18n-ok: nhãn 07.3
        QCOMPARE(banner_->convertButton()->text(), u"Chuyển sang Unicode"_s);     // i18n-ok: nhãn 07.3
        QCOMPARE(banner_->muteButton()->text(), u"Không nhắc lại với tệp này"_s); // i18n-ok: nhãn 07.3
        QCOMPARE(banner_->closeButton()->accessibleName(), u"Đóng banner"_s);     // i18n-ok: nhãn 07.3
        for (ui::Button* b :
             {banner_->detailsButton(), banner_->convertButton(), banner_->muteButton(), banner_->closeButton()})
            QVERIFY(b->isVisible());
        QCOMPARE(banner_->convertButton()->kind(), ui::Button::Kind::Primary);
        QCOMPARE(banner_->muteButton()->kind(), ui::Button::Kind::Ghost);

        QTest::mouseClick(banner_->detailsButton(), Qt::LeftButton);
        QCOMPARE(palette.size(), 1);
        QCOMPARE(palette.takeFirst().at(0).value<Tab>(), Tab::Summary);
        QVERIFY(banner_->isVisible()); // xem chi tiết không đóng banner

        // Viewer: khóa, huy hiệu "Viewer Pro", bấm mở trang nâng cấp, không mở tab Chuyển.
        QVERIFY(banner_->convertButton()->isLocked());
        QVERIFY(banner_->convertBadge()->isVisible());
        QCOMPARE(banner_->convertBadge()->need(), ui::Edition::ViewerPro);
        QCOMPARE(banner_->convertButton()->accessibleDescription(),
                 edition::EditionGate::availableIn(ui::Edition::ViewerPro));
        QTest::mouseClick(banner_->convertButton(), Qt::LeftButton);
        QCOMPARE(upgrades_.size(), 1);
        QCOMPARE(upgrades_[0].need, ui::Edition::ViewerPro);
        QCOMPARE(upgrades_[0].feature, u"fontconvert"_s);
        QCOMPARE(palette.size(), 0);

        // Viewer Pro, Pro: mở tab Chuyển của palette; huy hiệu ẩn, ổ khóa bỏ.
        for (edition::Edition e : {edition::Edition::ViewerPro, edition::Edition::Pro}) {
            gate_->setEdition(e);
            QVERIFY(!banner_->convertButton()->isLocked());
            QVERIFY(banner_->convertBadge()->isHidden());
            QVERIFY(banner_->convertButton()->accessibleDescription().isEmpty());
            QVERIFY(childrenInside(banner_));
            QTest::mouseClick(banner_->convertButton(), Qt::LeftButton);
            QCOMPARE(palette.size(), 1);
            QCOMPARE(palette.takeFirst().at(0).value<Tab>(), Tab::Convert);
        }
        QCOMPARE(upgrades_.size(), 1);

        // Đóng: banner ẩn, chip còn.
        QTest::mouseClick(banner_->closeButton(), Qt::LeftButton);
        QVERIFY(banner_->isHidden());
        QVERIFY(chip_->isVisible());
    }

    // Tiêu chí 3: không chặn thao tác; phần việc của banner trên luồng UI mỗi lần mở bản vẽ ≤ 2 ms.
    void notBlocking()
    {
        focusCommandLine();
        open(1, path(u"a.dwg"_s), {3, 250, 46});
        QVERIFY(banner_->isVisible());
        QCOMPARE(QApplication::focusWidget(), cmd_);
        QVERIFY(window_->isActiveWindow());
        QTest::mouseClick(banner_->detailsButton(), Qt::LeftButton); // nút nhận focus bằng Tab, không bằng cú nhấp
        QCOMPARE(QApplication::focusWidget(), cmd_);
        const int before = canvas_->presses;
        QTest::mouseClick(canvas_, Qt::LeftButton, {}, QPoint(canvas_->width() / 2, banner_->geometry().bottom() + 40));
        QCOMPARE(canvas_->presses, before + 1);

        // Mở bản vẽ: scanStarted, scanFinished, setCurrent, bố cục banner và chip (phần việc của Bác sĩ Font trên luồng
        // UI, ≤ 2 ms); vẽ khung đầu của banner và chip đo riêng (vẽ của Qt, ≤ 1 khung). Tốt nhất của 5 lần, CPU luồng.
        std::chrono::nanoseconds best = std::chrono::hours(1), bestPaint = best;
        for (quint64 id = 100; id < 105; ++id) {
            fd_->setCurrent(0);
            QVERIFY(banner_->isHidden());
            QCoreApplication::processEvents();
            const auto cpu0 = threadCpu();
            fd_->setCurrent(id);
            fd_->scanStarted(id, path(u"b%1.dwg"_s.arg(id)));
            fd_->scanFinished(id, {int(id % 7) + 1, qint64(id) * 13, qint64(id % 3) * 1000});
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            const auto cpu1 = threadCpu();
            banner_->repaint();
            chip_->repaint();
            const auto cpu2 = threadCpu();
            best = std::min(best, cpu1 - cpu0);
            bestPaint = std::min(bestPaint, cpu2 - cpu1);
            QVERIFY(banner_->isVisible() && chip_->isVisible());
        }
        qInfo("mở bản vẽ: cập nhật, bố cục banner + chip %.3f ms CPU; vẽ khung đầu %.3f ms CPU (tốt nhất của 5)",
              ms(best), ms(bestPaint));
        if (threadCpu().count() >= 0) {
            QVERIFY2(!kTimed || best <= 2ms, qPrintable(QString::number(ms(best))));
            QVERIFY2(!kTimed || best + bestPaint <= 16ms, qPrintable(QString::number(ms(best + bestPaint))));
        }
    }

    // Tiêu chí 4: đóng banner hoặc chế độ chỉ báo: chip "Font: 3 thay thế", tooltip, bấm mở palette.
    void chipAfterClose()
    {
        QSignalSpy palette(fd_.get(), &FontDoctorUi::paletteRequested);
        open(1, path(u"a.dwg"_s), {3, 250, 46});
        QVERIFY(chip_->isVisible());
        QTest::mouseClick(banner_->closeButton(), Qt::LeftButton);
        QVERIFY(banner_->isHidden());
        QVERIFY(chip_->isVisible());
        QCOMPARE(chip_->text(), u"Font: 3 thay thế"_s); // i18n-ok: mẫu 07.1 §10
        const ui::TipInfo tip = ui::RichTooltip::info(chip_);
        QCOMPARE(tip.name, u"Bác sĩ Font"_s); // i18n-ok: mẫu 07.1 §10
        QCOMPARE(tip.command, u"FIXFONT"_s);
        QCOMPARE(tip.description,
                 u"3 font đã thay thế · 296 đối tượng chữ mã cũ. Bấm để mở Bác sĩ Font."_s);         // i18n-ok: mẫu
        QCOMPARE(chip_->accessibleDescription(), u"3 font đã thay thế · 296 đối tượng chữ mã cũ"_s); // i18n-ok: mẫu
        QVERIFY(chip_->width() >= chip_->sizeHint().width());
        QTest::mouseClick(chip_, Qt::LeftButton);
        QCOMPARE(palette.size(), 1);
        QCOMPARE(palette.takeFirst().at(0).value<Tab>(), Tab::Summary);
        chip_->setFocus(Qt::TabFocusReason); // Enter, Space như bấm
        QTest::keyClick(chip_, Qt::Key_Return);
        QTest::keyClick(chip_, Qt::Key_Space);
        QCOMPARE(palette.size(), 2);

        // "Chỉ hiện chỉ báo trên thanh trạng thái" (UI-J04): không banner, vẫn chip; chỉ chữ mã cũ: đếm chữ.
        FontDoctorUiSettings(*store_).setShowMode(ShowMode::Indicator);
        open(2, path(u"b.dwg"_s), {2, 0, 0});
        QVERIFY(banner_->isHidden());
        QCOMPARE(chip_->text(), u"Font: 2 thay thế"_s); // i18n-ok: mẫu 07.1 §10
        QCOMPARE(ui::RichTooltip::info(chip_).description, u"2 font đã thay thế. Bấm để mở Bác sĩ Font."_s); // i18n-ok
        open(3, path(u"c.dwg"_s), {0, 1250, 0});
        QCOMPARE(chip_->text(), u"Font: 1.250 chữ mã cũ"_s); // i18n-ok: mẫu
        QCOMPARE(FontDoctorUiSettings(*store_).showMode(), ShowMode::Indicator);
        store_->set(kShowMode, u"lạ"_s); // i18n-ok: giá trị hỏng trong tệp
        QCOMPARE(FontDoctorUiSettings(*store_).showMode(), ShowMode::Banner);
    }

    // Tiêu chí 5: không nhắc lại theo đường dẫn chuẩn hóa, qua khởi động lại; vẫn có chip; đổi tên, chuyển: hiện lại.
    void muteFile()
    {
        QVERIFY(QDir(dir_.path()).mkpath(u"sub"_s));
        const QString file = path(u"Nhà phố.dwg"_s); // i18n-ok: tên tệp thử
        open(1, file, {3, 250, 0});
        QVERIFY(banner_->muteButton()->isVisible());
        QTest::mouseClick(banner_->muteButton(), Qt::LeftButton);
        QVERIFY(banner_->isHidden());
        QVERIFY(chip_->isVisible());
        const FontDoctorUiSettings s(*store_);
        QVERIFY(s.isMuted(file));
        QCOMPARE(store_->get(kMutedFiles), QStringList{FontDoctorUiSettings::normalizedPath(file)});
        // Cùng tệp viết khác: "/./", "x/..", "//", NFD (macOS) đều là một tệp.
        QVERIFY(s.isMuted(dir_.path() + u"/./sub/..//Nhà phố.dwg"_s)); // i18n-ok: tên tệp thử
        QVERIFY(s.isMuted(file.normalized(QString::NormalizationForm_D)));
        QVERIFY(!s.isMuted(QString()));

        // Mở lại trong phiên: không banner, vẫn chip; tệp đổi tên, chuyển thư mục: banner hiện lại.
        open(2, file, {3, 250, 0});
        QVERIFY(banner_->isHidden());
        QCOMPARE(chip_->text(), u"Font: 3 thay thế"_s);   // i18n-ok: mẫu 07.1 §10
        open(3, path(u"Nhà phố (2).dwg"_s), {3, 250, 0}); // i18n-ok: tên tệp thử
        QVERIFY(banner_->isVisible());
        open(4, path(u"sub/Nhà phố.dwg"_s), {3, 250, 0}); // i18n-ok: tên tệp thử
        QVERIFY(banner_->isVisible());
        // Bản vẽ chưa lưu: không có tệp để nhớ.
        open(5, QString(), {3, 250, 0});
        QVERIFY(banner_->isVisible());
        QVERIFY(banner_->muteButton()->isHidden());
        QVERIFY(childrenInside(banner_));

        // Khởi động lại: Store mới đọc cùng tệp, FontDoctorUi mới.
        store_->flush();
        drop();
        settings::Store reread(store_->filePath());
        make(FontDoctorUiSettings(reread));
        open(1, file, {3, 250, 0});
        QVERIFY(banner_->isHidden());
        QVERIFY(chip_->isVisible());
        open(2, path(u"khac.dwg"_s), {3, 250, 0});
        QVERIFY(banner_->isVisible());
        drop(); // trước khi reread hủy
        make(FontDoctorUiSettings(*store_));

        // Giới hạn danh sách: giữ kMaxMuted tệp mới nhất.
        for (int i = 0; i < FontDoctorUiSettings::kMaxMuted + 5; ++i)
            FontDoctorUiSettings(*store_).mute(path(u"t%1.dwg"_s.arg(i)));
        const QStringList kept = store_->get(kMutedFiles);
        QCOMPARE(kept.size(), FontDoctorUiSettings::kMaxMuted);
        QVERIFY(!FontDoctorUiSettings(*store_).isMuted(file)); // cũ nhất bị bỏ trước
        QCOMPARE(kept.last(), FontDoctorUiSettings::normalizedPath(path(u"t%1.dwg"_s.arg(1004))));
    }

    // Tiêu chí 6: không vấn đề thì không gì cả; quét lâu: chip "Đang quét font" + tiến trình, banner chờ.
    void cleanAndScanning()
    {
        open(1, path(u"sach.dwg"_s), {0, 0, 0});
        QVERIFY(banner_->isHidden());
        QVERIFY(chip_->isHidden());
        QVERIFY(!chip_->hasContent());

        // Bản vẽ nhỏ quét xong trước kScanChipDelay: chip không hiện "Đang quét font" (không nháy).
        ShowWatch scanningShows(chip_, [&] { return chip_->isScanning(); });
        fd_->setCurrent(2);
        fd_->scanStarted(2, path(u"nho.dwg"_s));
        QVERIFY(chip_->isHidden());
        QVERIFY(fd_->waiting());
        fd_->scanFinished(2, {1, 0, 0});
        QVERIFY(!fd_->waiting());
        QCOMPARE(chip_->text(), u"Font: 1 thay thế"_s); // i18n-ok: mẫu 07.1 §10
        QCOMPARE(scanningShows.count, 0);

        // Bản vẽ lớn: hiện chip đang quét sau kScanChipDelay (rút ngắn), không sớm hơn.
        const auto delay = 150ms;
        fd_->setScanChipDelay(delay);
        const auto started = Clock::now();
        fd_->setCurrent(3);
        fd_->scanStarted(3, path(u"lon.dwg"_s));
        QVERIFY(chip_->isHidden());
        QVERIFY(fd_->isScanning(3));
        QVERIFY(spinUntil([&] { return chip_->isVisible(); }));
        QVERIFY(Clock::now() - started >= delay);
        QCOMPARE(chip_->text(), u"Đang quét font"_s); // i18n-ok: nhãn chip
        QVERIFY(chip_->isScanning());
        QCOMPARE(chip_->progress(), -1.0);
        const int textOnly = chip_->sizeHint().width();
        fd_->scanProgress(3, 0.42);
        QCOMPARE(chip_->progress(), 0.42);
        QCOMPARE(chip_->accessibleDescription(), u"Đã quét 40%"_s); // i18n-ok: theo bước 10%
        QVERIFY(chip_->sizeHint().width() > textOnly);              // thanh tiến trình
        QVERIFY(banner_->isHidden());                               // banner chờ quét xong
        QVERIFY(!fd_->result(3));
        fd_->scanFinished(3, {3, 250, 0});
        QVERIFY(banner_->isVisible());
        QCOMPARE(chip_->text(), u"Font: 3 thay thế"_s); // i18n-ok: mẫu 07.1 §10
        QVERIFY(fd_->result(3).has_value());
        QVERIFY(!fd_->waiting());

        // Quét lâu rồi không vấn đề: chip đang quét biến mất.
        fd_->setScanChipDelay(0ms);
        fd_->setCurrent(4);
        fd_->scanStarted(4, path(u"lon2.dwg"_s));
        QVERIFY(chip_->isScanning() && chip_->isVisible());
        fd_->scanFinished(4, {});
        QVERIFY(chip_->isHidden());
        QVERIFY(banner_->isHidden());
    }

    // Tiêu chí 7: mỗi tab banner riêng; chuyển tab đổi banner và chip ≤ 100 ms.
    void perTab()
    {
        fd_->setScanChipDelay(0ms);
        open(1, path(u"a.dwg"_s), {3, 250, 0});
        open(2, path(u"b.dwg"_s), {});
        open(3, path(u"c.dwg"_s), {1, 0, 46});
        QTest::mouseClick(banner_->closeButton(), Qt::LeftButton); // đóng banner của tab 3
        fd_->setCurrent(4);
        fd_->scanStarted(4, path(u"d.dwg"_s));

        struct Want {
            quint64 tab;
            bool banner;
            QString chip; // rỗng: không chip
        };
        const Want wants[] = {
            {1, true, u"Font: 3 thay thế"_s},  // i18n-ok: mẫu 07.1 §10
            {2, false, {}},                    // không vấn đề
            {3, false, u"Font: 1 thay thế"_s}, // i18n-ok: banner đã đóng ở tab này
            {1, true, u"Font: 3 thay thế"_s},  // i18n-ok: tab 1 vẫn còn banner
            {4, false, u"Đang quét font"_s},   // i18n-ok: đang quét
            {0, false, {}},                    // tab Start
            {3, false, u"Font: 1 thay thế"_s}, // i18n-ok: quay lại vẫn đã đóng
        };
        std::chrono::nanoseconds worst{0};
        for (const Want& w : wants) {
            const auto cpu0 = threadCpu();
            const auto wall0 = Clock::now();
            fd_->setCurrent(w.tab);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            window_->repaint();
            const std::chrono::nanoseconds spent = threadCpu().count() >= 0 ? threadCpu() - cpu0 : Clock::now() - wall0;
            worst = std::max(worst, spent);
            QCOMPARE(banner_->isVisible(), w.banner);
            QCOMPARE(chip_->isVisible(), !w.chip.isEmpty());
            if (!w.chip.isEmpty())
                QCOMPARE(chip_->text(), w.chip);
            if (w.banner)
                QCOMPARE(numbersIn(banner_->message()), (QList<qint64>{3, 250}));
        }
        qInfo("chuyển tab: banner + chip, lâu nhất %.3f ms", ms(worst));
        QVERIFY2(worst <= 100ms, qPrintable(QString::number(ms(worst))));

        fd_->scanStarted(3, path(u"c.dwg"_s)); // quét lại tab 3: banner đã đóng vẫn không hiện lại
        fd_->scanFinished(3, {1, 0, 46});
        QVERIFY(banner_->isHidden());
        fd_->removeDocument(1);
        fd_->setCurrent(1); // tab đã đóng: không còn trạng thái
        QVERIFY(banner_->isHidden() && chip_->isHidden());
    }

    // Tiêu chí 8 (tới chỗ nối): số trên banner và chip khớp đúng kết quả quét đưa vào.
    void matchesScan()
    {
        std::mt19937_64 rng(20261023);
        std::uniform_int_distribution<int> fonts(0, 40);
        std::uniform_int_distribution<qint64> count(0, 3'000'000);
        std::bernoulli_distribution zero(0.3);
        int checked = 0;
        for (int i = 0; i < 1000; ++i) {
            const FontScan s{zero(rng) ? 0 : fonts(rng), zero(rng) ? 0 : count(rng), zero(rng) ? 0 : count(rng)};
            if (!s.hasIssues())
                continue;
            QList<qint64> want;
            for (qint64 n : {qint64(s.missingFonts), s.tcvn3, s.vni})
                if (n > 0)
                    want << n;
            const QString msg = FontDoctorBanner::compose(s.missingFonts, s.tcvn3, s.vni);
            QCOMPARE(numbersIn(msg), want);
            chip_->setIssues(s.missingFonts, s.tcvn3, s.vni);
            QCOMPARE(numbersIn(chip_->text()), QList<qint64>{s.missingFonts > 0 ? s.missingFonts : s.legacy()});
            QList<qint64> facts;
            for (qint64 n : {qint64(s.missingFonts), s.legacy()})
                if (n > 0)
                    facts << n;
            QCOMPARE(numbersIn(chip_->accessibleDescription()), facts);
            ++checked;
        }
        QVERIFY(checked > 800);
    }

    // DoD-U: vi-XL ở 1366 px không cắt chữ (câu xuống dòng, nút xuống hàng khi hẹp), hai mật độ.
    void pseudoNoTruncation()
    {
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QVERIFY(spinUntil([&] { return banner_->detailsButton()->text().size() > 14; }));
        open(1, path(u"a.dwg"_s), {3, 1250, 46});
        open(2, path(u"b.dwg"_s), {0, 0, 0}); // chip đang quét ở dưới
        fd_->setScanChipDelay(0ms);
        fd_->setCurrent(3);
        fd_->scanStarted(3, path(u"c.dwg"_s));
        fd_->scanProgress(3, 0.5);
        QVERIFY(chip_->isScanning());
        const auto check = [&](const char* where) {
            QVERIFY2(spinUntil([&] { return truncations(window_.get()).isEmpty(); }, 5s),
                     qPrintable(QString::fromLatin1(where) + u": "_s + truncations(window_.get())));
            QVERIFY2(childrenInside(banner_), where);
            QVERIFY(banner_->y() + banner_->height() < canvas_->height());
        };
        fd_->setCurrent(1);
        QVERIFY(sentence(banner_));
        const int lineHeight = sentence(banner_)->fontMetrics().height();
        for (bool docks : {false, true}) {
            showDocks(docks);
            for (Density d : {Density::Comfortable, Density::Compact}) {
                tm_->setDensity(d);
                fd_->setCurrent(1);
                check(docks ? "docks" : "full");
                QVERIFY(banner_->width() == canvas_->width());
                if (docks) { // 1366 − 2 × --pal-w: câu xuống dòng, nút xuống hàng dưới câu
                    const QLabel* text = sentence(banner_);
                    QVERIFY(text->height() > lineHeight);
                    QVERIFY(banner_->detailsButton()->y() > text->geometry().top());
                }
                fd_->setCurrent(3);
                check("scanning chip");
            }
        }
    }

    // DoD-U 7: vai trò, tên, thông báo; tên và thứ tự Tab (a11y); Tab tới nút; Esc đóng, focus về chỗ cũ.
    void accessibility()
    {
        QAccessible::installUpdateHandler(recordA11y);
        QAccessible::setActive(true);
        g_said.clear();
        open(1, path(u"a.dwg"_s), {3, 250, 0});
        QAccessibleInterface* ai = QAccessible::queryAccessibleInterface(banner_);
        QVERIFY(ai);
        QCOMPARE(ai->role(), QAccessible::StatusBar);
        QVERIFY(ai->text(QAccessible::Name).contains(banner_->message()));
        QCOMPARE(g_said.size(), size_t(1));
        QCOMPARE(g_said[0].source, banner_.data());
        QCOMPARE(g_said[0].message, banner_->accessibleName());
        QCOMPARE(g_said[0].politeness, QAccessible::AnnouncementPoliteness::Polite);
        fd_->scanFinished(1, {3, 250, 0}); // VN-004 báo lại cùng số liệu: không đọc lại
        QCOMPARE(g_said.size(), size_t(1));
        fd_->scanFinished(1, {3, 250, 46}); // số liệu đổi: đọc câu mới
        QCOMPARE(g_said.size(), size_t(2));
        QVERIFY(g_said[1].message.contains(u"46"_s));
        QAccessible::installUpdateHandler(nullptr);
        QAccessible::setActive(false);

        // Cửa sổ riêng chỉ có widget của UIJ-001 và một nút giữ focus ban đầu (dòng lệnh thật là UIA-107).
        QWidget w;
        auto* col = new QVBoxLayout(&w);
        auto* canvas = new QWidget;
        canvas->setMinimumHeight(200);
        auto* origin = new ui::Button(u"OK"_s); // i18n-ok: chữ mẫu của test
        auto* chip = new FontStatusChip;
        col->addWidget(canvas, 1);
        col->addWidget(origin);
        col->addWidget(chip);
        auto* banner = new FontDoctorBanner(canvas, *gate_);
        FontDoctorUi fd(*banner, *chip, FontDoctorUiSettings(*store_));
        w.resize(900, 400);
        w.show();
        w.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        fd.setCurrent(1);
        fd.scanStarted(1, path(u"a.dwg"_s));
        fd.scanFinished(1, {3, 250, 46});
        QVERIFY(banner->isVisible() && chip->isVisible());
        const QStringList names = a11y::checkNames(&w);
        QVERIFY2(names.isEmpty(), qPrintable(names.join(u'\n')));
        QStringList order;
        const QStringList focus = a11y::checkFocus(&w, &order);
        QVERIFY2(focus.isEmpty(), qPrintable(focus.join(u'\n')));
        QCOMPARE(order.size(), 6); // OK, chip, 4 nút banner (thứ tự theo chuỗi focus của cửa sổ)

        origin->setFocus(Qt::OtherFocusReason);
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == origin; }));
        QWidget* before = origin; // chỗ có focus ngay trước khi Tab vào banner
        for (int i = 0; i < 6 && !banner->isAncestorOf(QApplication::focusWidget()); ++i) {
            before = QApplication::focusWidget();
            QTest::keyClick(before, Qt::Key_Tab);
        }
        QVERIFY(banner->isAncestorOf(QApplication::focusWidget()));
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
        QVERIFY(banner->isHidden());
        QVERIFY(chip->isVisible());
        QCOMPARE(QApplication::focusWidget(), before);
        w.close();
        focusCommandLine();
    }

    // DoD-U 2: nền, viền banner và nền chip theo token ở Tối và Sáng.
    void themes()
    {
        open(1, path(u"a.dwg"_s), {3, 250, 0});
        for (Theme t : {Theme::Dark, Theme::Light}) {
            tm_->setTheme(t);
            QVERIFY(paints(banner_, {1, 1}, tm_->color(tok::c_banner)));
            QVERIFY(paints(banner_, {banner_->width() / 2, banner_->height() - 1}, tm_->color(tok::c_border)));
            QVERIFY(paints(chip_, {chip_->width() - 3, chip_->height() / 2}, tm_->color(tok::c_warn_soft)));
        }
    }

    // Banner chung (ui::CanvasBanner) hiện thì banner Bác sĩ Font nằm ngay dưới, ẩn thì về mép trên.
    void stacking()
    {
        auto* common = new ui::CanvasBanner(canvas_);
        banner_->stackBelow(common);
        open(1, path(u"a.dwg"_s), {3, 250, 0});
        QCOMPARE(banner_->y(), 0);
        common->add(u"readonly"_s, ui::Severity::Warning, u"Bản vẽ chỉ đọc"_s); // i18n-ok: dữ liệu thử
        QVERIFY(spinUntil([&] { return banner_->y() == common->geometry().bottom() + 1; }));
        QVERIFY(!banner_->geometry().intersects(common->geometry()));
        common->remove(u"readonly"_s);
        QVERIFY(spinUntil([&] { return banner_->y() == 0; }));
        delete common;
        QCOMPARE(banner_->y(), 0);
    }

    // DoD-U: rảnh không timer (timer một lần chỉ khi bản vẽ hiện hành đang quét chưa tới kScanChipDelay).
    void idle()
    {
        open(1, path(u"a.dwg"_s), {3, 250, 46});
        QCoreApplication::processEvents();
        QVERIFY(!fd_->waiting());
        const auto timers = window_->findChildren<QTimer*>();
        QVERIFY(std::none_of(timers.begin(), timers.end(), [](const QTimer* t) { return t->isActive(); }));
    }
};

QTEST_MAIN(FontDoctorBannerTest)
#include "font_doctor_banner_test.moc"
