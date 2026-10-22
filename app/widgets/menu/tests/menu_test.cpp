// UI0-107 · Menu, tiêu chí:
//   1  openLatency: mở menu 30 mục rồi menu con 30 mục (phím →) đến khung đầu, đo bằng UiProbe Op::MenuOpen; ngưỡng
//      p99 ≤ 100 ms chỉ kiểm ở preset release (kTimed), luôn in p50/p99/max
//   3  keyboard: mũi tên bỏ qua tiêu đề nhóm, vạch chia, mục vô hiệu; → mở menu con, ← và Esc đóng từng cấp; Enter chạy
//      mục; mnemonic theo chữ của ngôn ngữ (vi "&Lưu", "&Đóng"; en "&Save")
//      accessibility: QAccessible đọc tên mục, phím theo nền tảng (Accelerator), trạng thái khóa (Description), vô
//      hiệu, dấu chọn, menu con, mục đang trỏ
//   2, 4 itemTooltip: rê mục có thông tin hiện tooltip dưới mục, không che con trỏ, rời/đóng menu thì ẩn ngay; F1 phát
//      yêu cầu trợ giúp theo lệnh của mục
//   keysFollowKeymap: phím đổi ngay khi Keymap đổi (menu đang mở đo lại), theo nền tảng; shortcut của QAction không
//      chiếm thêm chỗ
//   layout: cao --h-ctl theo mật độ, tiêu đề nhóm, vạch chia, huy hiệu chiếm chỗ, rộng tối thiểu
//   sheet: ảnh baseline menu và tooltip, Tối/Sáng × 100/150/200 % (DoD-U 1), kèm kiểm màu không phụ thuộc font
//   pseudoNoTruncation: vi-XL không cắt chữ (menu đóng và đang mở, hai mật độ)
// Ghi lại baseline: TRION_UPDATE_BASELINE=<thư mục> chạy test, chép sheet-*-<nền tảng>.png vào tests/baseline.
#include "app/widgets/menu/Menu.h"

#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/keymap/Keymap.h"
#include "app/core/perf/UiProbe.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"
#include "tests/timing.h"

#include <QAccessible>
#include <QApplication>
#include <QDir>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <functional>
#include <memory>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;
using namespace trion::theme;
using namespace trion::ui;
namespace i18n = trion::i18n;
namespace keymap = trion::keymap;
namespace probe = trion::ui::probe;
namespace sched = trion::sched;
namespace settings = trion::settings;

namespace {

using trion::test::kTimed;

// Baseline theo nền tảng: FreeType của Linux và Windows offscreen dựng chữ khác nhau. Dung sai: lệch ≤ kNoise/255 mỗi
// kênh bỏ qua; điểm lệch hơn được tối đa 0,1% ảnh.
#if defined(Q_OS_WIN)
constexpr auto kPlatform = "windows";
#elif defined(Q_OS_MACOS)
constexpr auto kPlatform = "macos";
#else
constexpr auto kPlatform = "linux";
#endif
constexpr int kNoise = 8;
constexpr double kOpenBudgetMs = 100; // tiêu chí 1, DoD-L "mở menu"

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

bool near(const QColor& a, const QColor& b)
{
    return std::abs(a.red() - b.red()) <= kNoise && std::abs(a.green() - b.green()) <= kNoise &&
           std::abs(a.blue() - b.blue()) <= kNoise;
}

int px(tok::PxId id)
{
    return qRound(ThemeManager::instance().px(id));
}

// Mục có chữ, thông tin (phím, khóa, lệnh) và biểu tượng tùy chọn.
QAction* item(QMenu& m, const QString& text, const TipInfo& info = {}, const QString& icon = {})
{
    QAction* a = m.addAction(text);
    if (!icon.isEmpty())
        a->setIcon(IconProvider::icon(icon));
    if (!info.command.isEmpty() || !info.keyId.isEmpty() || info.need != Edition::None || !info.description.isEmpty())
        RichTooltip::attach(a, info);
    return a;
}

// Menu mẫu của styleguide.html (mục Menu, tooltip), thêm mục khóa Viewer Pro có phím.
struct Sample {
    Menu menu;
    QAction* section = menu.addSection(u"Có vùng chọn"_s);
    QAction* repeat = item(menu, u"Lặp lệnh DIST"_s, {.command = u"DIST"_s});
    Menu* recent = new Menu(u"Lệnh gần đây"_s, &menu);
    QAction* recentAction = menu.addMenu(recent);
    QAction* separator = menu.addSeparator();
    QAction* endpoint = item(menu, u"Điểm cuối"_s);
    QAction* props = item(menu, u"&Thuộc tính"_s, {.command = u"PROPERTIES"_s, .keyId = u"props"_s}, u"props"_s);
    QAction* similar = item(menu, u"Chọn tương tự"_s,
                            {.command = u"SELECTSIMILAR"_s,
                             .description = u"Chọn đối tượng cùng loại, cùng layer."_s,
                             .need = Edition::Pro});
    QAction* pdf =
        item(menu, u"Xuất PDF"_s, {.command = u"EXPORTPDF"_s, .keyId = u"plot"_s, .need = Edition::ViewerPro});
    QAction* paste = item(menu, u"Dán"_s, {.command = u"PASTECLIP"_s, .keyId = u"paste"_s});

    Sample()
    {
        item(*recent, u"LINE"_s, {.command = u"LINE"_s});
        item(*recent, u"CIRCLE"_s, {.command = u"CIRCLE"_s});
        endpoint->setCheckable(true);
        endpoint->setChecked(true);
        paste->setEnabled(false);
    }
};

// Mẫu cho vi-XL: chuỗi qua tr(), đặt lại khi đổi ngôn ngữ như widget thật.
class MenuSample : public QWidget {
    Q_OBJECT
public:
    MenuSample()
    {
        section = menu->addSection({});
        props = menu->addAction(QString());
        similar = menu->addAction(QString());
        menu->addMenu(recent);
        paste = recent->addAction(QString());
        retranslate();
    }

    Menu* menu = new Menu(this);
    Menu* recent = new Menu(menu);
    QAction* section;
    QAction* props;
    QAction* similar;
    QAction* paste;

protected:
    void changeEvent(QEvent* e) override
    {
        if (e->type() == QEvent::LanguageChange)
            retranslate();
        QWidget::changeEvent(e);
    }

private:
    void retranslate()
    {
        section->setText(tr("Có vùng chọn"));
        props->setText(tr("&Thuộc tính"));
        similar->setText(tr("Chọn tương tự"));
        recent->setTitle(tr("Lệnh &gần đây"));
        paste->setText(tr("Dán"));
        RichTooltip::attach(props, {.command = u"PROPERTIES"_s, .keyId = u"props"_s});
        RichTooltip::attach(similar, {.command = u"SELECTSIMILAR"_s, .need = Edition::ViewerPro});
        RichTooltip::attach(paste, {.command = u"PASTECLIP"_s, .keyId = u"paste"_s});
    }
};

} // namespace

class MenuTest : public QObject {
    Q_OBJECT

    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<UiExecutor> executor_;
    std::unique_ptr<ThemeManager> tm_;
    std::unique_ptr<IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<keymap::Keymap> keymap_;
    std::unique_ptr<RichTooltip> tips_;

    QString tipText() const
    {
        QStringList out;
        for (QLabel* l : tips_->tipWindow()->findChildren<QLabel*>())
            if (!l->isHidden())
                out << l->text();
        return out.join(u'\n');
    }

    static void open(QMenu& m)
    {
        m.popup(QPoint(40, 40));
        QVERIFY(QTest::qWaitForWindowExposed(&m));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        QVERIFY(loadFonts()); // chữ Plex nhúng: ảnh không phụ thuộc font của máy
        executor_ = std::make_unique<UiExecutor>();
        tm_ = std::make_unique<ThemeManager>(*store_);
        icons_ = std::make_unique<IconProvider>();
        translator_ = std::make_unique<i18n::Translator>();
        keymap_ = std::make_unique<keymap::Keymap>(*store_, keymap::Platform::Windows);
        tips_ = std::make_unique<RichTooltip>();
    }

    void init()
    {
        tm_->setTheme(Theme::Dark);
        tm_->setDensity(Density::Compact);
        tips_->setKeymap(keymap_.get());
        tips_->setDelays(1ms, 1ms);
    }

    void cleanup()
    {
        probe::disable(); // openLatency hỏng giữa chừng
        while (QWidget* p = QApplication::activePopupWidget())
            p->close();
        tips_->leave();
        QCoreApplication::processEvents();
    }

    void cleanupTestCase()
    {
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

    // Tiêu chí 1.
    void openLatency()
    {
        Menu root;
        for (int i = 0; i < 30; ++i)
            item(root, u"Lệnh thứ %1"_s.arg(i), {.command = u"CMD%1"_s.arg(i), .keyId = i % 3 ? QString() : u"save"_s},
                 u"props"_s);
        auto* sub = new Menu(u"Thêm lệnh"_s, &root);
        for (int i = 0; i < 30; ++i)
            item(*sub, u"Mục con %1"_s.arg(i),
                 {.command = u"SUB%1"_s.arg(i), .keyId = u"copy"_s, .need = i % 5 ? Edition::None : Edition::Pro});
        QAction* subAction = root.addMenu(sub);

        probe::enable();
        probe::reset();
        const auto count = [] {
            return probe::toJson()["ops"_L1].toObject()["menu_open"_L1].toObject()["count"_L1].toInt();
        };
        constexpr int kRuns = 25;
        for (int i = 0; i < kRuns; ++i) {
            root.popup(QPoint(40, 40));
            QVERIFY(spinUntil([&] { return count() == 2 * i + 1; }));
            root.setActiveAction(subAction);
            QTest::keyClick(&root, Qt::Key_Right); // menu con mở ngay, đo từ phím đến khung đầu
            QVERIFY(sub->isVisible());
            QVERIFY(spinUntil([&] { return count() == 2 * i + 2; }));
            root.hide();
            QVERIFY(!sub->isVisible());
        }
        const QJsonObject op = probe::toJson()["ops"_L1].toObject()["menu_open"_L1].toObject();
        probe::disable();
        const double p50 = op["p50_ms"_L1].toDouble(), p99 = op["p99_ms"_L1].toDouble(),
                     max = op["max_ms"_L1].toDouble();
        qInfo("menu_open, %d lần (menu 30 mục và menu con 30 mục): p50 %.2f ms, p99 %.2f ms, max %.2f ms; ngưỡng %.0f "
              "ms, "
              "mục tiêu 1 khung 16,7 ms",
              2 * kRuns, p50, p99, max, kOpenBudgetMs);
        QCOMPARE(op["count"_L1].toInt(), 2 * kRuns);
        QVERIFY2(!kTimed || p99 <= kOpenBudgetMs, qPrintable(QString::number(p99)));
    }

    // Tiêu chí 3: bàn phím.
    void keyboard()
    {
        Menu m;
        m.addSection(u"Tệp"_s);
        QAction* save = item(m, u"&Lưu"_s);
        QAction* openFile = item(m, u"&Mở"_s);
        openFile->setEnabled(false);
        m.addSeparator();
        QAction* close = item(m, u"&Đóng"_s);
        auto* recent = new Menu(u"&Gần đây"_s, &m);
        QAction* one = item(*recent, u"Tệp &1"_s);
        QAction* two = item(*recent, u"Tệp &2"_s);
        QAction* recentAction = m.addMenu(recent);
        QSignalSpy triggered(&m, &QMenu::triggered);

        open(m);
        QTest::keyClick(&m, Qt::Key_Down);
        QCOMPARE(m.activeAction(), save); // tiêu đề nhóm không nhận focus
        QTest::keyClick(&m, Qt::Key_Down);
        QCOMPARE(m.activeAction(), close); // bỏ qua mục vô hiệu, vạch chia
        QTest::keyClick(&m, Qt::Key_Down);
        QCOMPARE(m.activeAction(), recentAction);
        QTest::keyClick(&m, Qt::Key_Up);
        QCOMPARE(m.activeAction(), close);
        QTest::keyClick(&m, Qt::Key_Down);
        QTest::keyClick(&m, Qt::Key_Right);
        QVERIFY(recent->isVisible());
        QCOMPARE(recent->activeAction(), one);
        QTest::keyClick(recent, Qt::Key_Down);
        QCOMPARE(recent->activeAction(), two);
        QTest::keyClick(recent, Qt::Key_Left); // ← đóng menu con
        QVERIFY(!recent->isVisible());
        QVERIFY(m.isVisible());
        QTest::keyClick(&m, Qt::Key_Right);
        QVERIFY(recent->isVisible());
        QTest::keyClick(recent, Qt::Key_Escape); // Esc đóng một cấp
        QVERIFY(!recent->isVisible());
        QVERIFY(m.isVisible());
        QCOMPARE(m.activeAction(), recentAction);
        QTest::keyClick(&m, Qt::Key_Escape);
        QVERIFY(!m.isVisible());
        QCOMPARE(triggered.count(), 0);

        // Enter chạy mục.
        open(m);
        QTest::keyClick(&m, Qt::Key_Down);
        QTest::keyClick(&m, Qt::Key_Return);
        QCOMPARE(triggered.count(), 1);
        QCOMPARE(triggered.last().at(0).value<QAction*>(), save);
        QVERIFY(!m.isVisible());

        // Mnemonic theo chữ của ngôn ngữ đang dùng: vi.
        open(m);
        QTest::keyClick(&m, Qt::Key_L);
        QCOMPARE(triggered.last().at(0).value<QAction*>(), save);
        open(m);
        QTest::sendKeyEvent(QTest::Click, &m, Qt::Key(0x0110), u"đ"_s, Qt::NoModifier); // Đ có dấu
        QCOMPARE(triggered.last().at(0).value<QAction*>(), close);
        open(m);
        QTest::keyClick(&m, Qt::Key_M); // "&Mở" vô hiệu
        QVERIFY(m.isVisible());
        QTest::keyClick(&m, Qt::Key_Escape);
        // en: chủ menu đặt lại chữ khi LanguageChange.
        save->setText(u"&Save"_s);   // i18n-ok: dữ liệu thử (bản English)
        close->setText(u"&Close"_s); // i18n-ok: dữ liệu thử (bản English)
        open(m);
        QTest::keyClick(&m, Qt::Key_L); // không còn mục "&L"
        QVERIFY(m.isVisible());
        QTest::keyClick(&m, Qt::Key_S);
        QCOMPARE(triggered.last().at(0).value<QAction*>(), save);
        QCOMPARE(triggered.count(), 4);
    }

    // Tiêu chí 3: trình đọc màn hình.
    void accessibility()
    {
        Sample s;
        QAccessibleInterface* menu = QAccessible::queryAccessibleInterface(&s.menu);
        QVERIFY(menu);
        QCOMPARE(menu->role(), QAccessible::PopupMenu);
        QCOMPARE(menu->childCount(), int(s.menu.actions().size()));
        const auto child = [&](QAction* a) { return menu->child(int(s.menu.actions().indexOf(a))); };
        const auto text = [&](QAction* a, QAccessible::Text t) { return child(a)->text(t); };

        QCOMPARE(child(s.props)->role(), QAccessible::MenuItem);
        QCOMPARE(child(s.props)->parent(), menu);
        QCOMPARE(text(s.props, QAccessible::Name), u"Thuộc tính"_s); // không đọc dấu &
        QCOMPARE(text(s.props, QAccessible::Accelerator), u"Ctrl+1"_s);
        QCOMPARE(text(s.props, QAccessible::Description), QString());
        QCOMPARE(text(s.repeat, QAccessible::Accelerator), QString());
        QCOMPARE(text(s.similar, QAccessible::Name), u"Chọn tương tự"_s);
        QCOMPARE(text(s.similar, QAccessible::Description),
                 u"Có trong TrionCAD Pro. Chọn đối tượng cùng loại, cùng layer."_s);
        QVERIFY(!child(s.similar)->state().disabled); // khóa vẫn chọn được để mở trang nâng cấp
        QCOMPARE(text(s.pdf, QAccessible::Description), u"Có trong TrionCAD Viewer Pro"_s);
        QCOMPARE(text(s.pdf, QAccessible::Accelerator), u"Ctrl+P"_s);
        QVERIFY(child(s.paste)->state().disabled);
        QCOMPARE(text(s.paste, QAccessible::Accelerator), u"Ctrl+V"_s);
        QVERIFY(child(s.paste)->actionInterface()->actionNames().isEmpty());
        QVERIFY(child(s.endpoint)->state().checkable);
        QVERIFY(child(s.endpoint)->state().checked);
        QVERIFY(child(s.recentAction)->state().hasPopup);
        QCOMPARE(child(s.recentAction)->childCount(), 1);
        QCOMPARE(child(s.recentAction)->child(0)->object(), s.recent);
        QCOMPARE(child(s.section)->role(), QAccessible::Separator);
        QCOMPARE(text(s.section, QAccessible::Name), u"Có vùng chọn"_s);
        // Mnemonic khi không có phím: như Qt.
        QAction* mnemonic = item(s.menu, u"&Gộp"_s);
        QCOMPARE(text(mnemonic, QAccessible::Accelerator), u"Alt+G"_s);

        // Phím theo nền tảng: macOS dùng ⌘ cho phím hệ thống.
        keymap::Keymap mac(*store_, keymap::Platform::Mac);
        tips_->setKeymap(&mac);
        QCOMPARE(text(s.paste, QAccessible::Accelerator), u"⌘V"_s);
        QCOMPARE(text(s.pdf, QAccessible::Accelerator), u"⌘P"_s);
        tips_->setKeymap(keymap_.get());

        // Mục đang trỏ có focus; hành động "nhấn" chạy mục.
        open(s.menu);
        QTest::keyClick(&s.menu, Qt::Key_Down);
        QCOMPARE(s.menu.activeAction(), s.repeat);
        QVERIFY(child(s.repeat)->state().focused);
        QVERIFY(!child(s.props)->state().focused);
        QVERIFY(s.menu.geometry().contains(child(s.props)->rect()));
        QSignalSpy pressed(s.props, &QAction::triggered);
        child(s.props)->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
        QCOMPARE(pressed.count(), 1);
    }

    // Tiêu chí 2, 4 trên mục menu.
    void itemTooltip()
    {
        Sample s;
        open(s.menu);
        QSignalSpy help(tips_.get(), &RichTooltip::helpRequested);
        const QRect r = s.menu.actionGeometry(s.props);
        QTest::mouseMove(&s.menu, r.center());
        QCOMPARE(tips_->target(), s.props);
        QVERIFY(spinUntil([&] { return tips_->level() == 2; }));
        QCOMPARE(tipText(), u"Thuộc tính\nPROPERTIES · Ctrl+1\nNhấn F1 để xem trợ giúp"_s);
        const QRect tip = tips_->tipWindow()->geometry();
        QVERIFY(tip.top() > s.menu.mapToGlobal(r.bottomLeft()).y()); // dưới mục
        QVERIFY(!tip.intersects(QRect(s.menu.mapToGlobal(r.center()), QSize(px(tok::sp_6), px(tok::sp_7)))));
        QTest::keyClick(&s.menu, Qt::Key_F1);
        QCOMPARE(help.count(), 1);
        QCOMPARE(help.last().at(0).toString(), u"PROPERTIES"_s);
        QVERIFY(s.menu.isVisible()); // F1 không chạy mục, không đóng menu
        QCOMPARE(tips_->level(), 0);

        // Mục khóa: tooltip có huy hiệu; mục không có thông tin, mục mở menu con: không tooltip.
        QTest::mouseMove(&s.menu, s.menu.actionGeometry(s.similar).center());
        QCOMPARE(tips_->target(), s.similar);
        QTest::mouseMove(&s.menu, s.menu.actionGeometry(s.endpoint).center());
        QCOMPARE(tips_->target(), nullptr);
        QTest::mouseMove(&s.menu, s.menu.actionGeometry(s.recentAction).center());
        QCOMPARE(tips_->target(), nullptr);
        // Chuột rời menu, đóng menu: tooltip ẩn ngay.
        tips_->setDelays(1ms, 1h);
        QTest::mouseMove(&s.menu, r.center());
        QVERIFY(spinUntil([&] { return tips_->level() == 1; }));
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(&s.menu, &leave);
        QCOMPARE(tips_->level(), 0);
        QTest::mouseMove(&s.menu, r.center() + QPoint(1, 0)); // Qt bỏ sự kiện di chuột không đổi vị trí
        QVERIFY(spinUntil([&] { return tips_->level() == 1; }));
        s.menu.hide();
        QCOMPARE(tips_->level(), 0);
        QVERIFY(!tips_->tipWindow()->isVisible());
    }

    void keysFollowKeymap()
    {
        Menu m;
        QAction* save =
            item(m, u"Lưu bản vẽ với tên khác, định dạng khác"_s, {.command = u"QSAVE"_s, .keyId = u"save"_s});
        open(m);
        const int w0 = m.width();
        QCOMPARE(Menu::keyText(save), u"Ctrl+S"_s);
        QVERIFY(!keymap_->assign(u"save"_s, u"Ctrl+Alt+K"_s));
        QTRY_COMPARE(Menu::keyText(save), u"Ctrl+S · Ctrl+Alt+K"_s);
        QTRY_VERIFY(m.width() > w0); // menu đang mở đo lại
        keymap_->reset(u"save"_s);
        QTRY_COMPARE(m.width(), w0);
        keymap::Keymap mac(*store_, keymap::Platform::Mac);
        tips_->setKeymap(&mac);
        QCOMPARE(Menu::keyText(save), u"⌘S"_s);
        tips_->setKeymap(keymap_.get());
        QCOMPARE(Menu::keyText(nullptr), QString());

        // Phím của QAction (UI0-015 gán) không hiện thêm cột của Qt.
        Menu plain, bound;
        item(plain, save->text(), RichTooltip::info(save));
        item(bound, save->text(), RichTooltip::info(save))->setShortcut(QKeySequence(u"Ctrl+S"_s));
        QCOMPARE(bound.sizeHint(), plain.sizeHint());
    }

    void layout()
    {
        for (Density d : {Density::Compact, Density::Comfortable}) {
            tm_->setDensity(d);
            Sample s;
            s.menu.ensurePolished();
            s.menu.resize(s.menu.sizeHint());
            const int ctl = qRound(tm_->px(tok::h_ctl));
            QCOMPARE(ctl, d == Density::Compact ? 24 : 26);
            for (QAction* a : {s.repeat, s.recentAction, s.endpoint, s.props, s.similar, s.pdf, s.paste})
                QCOMPARE(s.menu.actionGeometry(a).height(), ctl);
            // .mh: lề 6 + dòng --fs-xs + 3 (~23 px); .ms: 1 px giữa lề 4.
            const int header = s.menu.actionGeometry(s.section).height();
            QVERIFY2(header >= 21 && header <= 25, qPrintable(QString::number(header)));
            QCOMPARE(s.menu.actionGeometry(s.separator).height(), 1 + 2 * px(tok::sp_2));
            QVERIFY(s.menu.sizeHint().width() >= 200);
        }
        // Đổi mật độ khi menu đã dựng: đo lại.
        Sample s;
        s.menu.ensurePolished();
        (void)s.menu.sizeHint();
        tm_->setDensity(Density::Compact);
        QCOMPARE(s.menu.actionGeometry(s.props).height(), 24);
        tm_->setDensity(Density::Comfortable);
        (void)s.menu.sizeHint();
        QCOMPARE(s.menu.actionGeometry(s.props).height(), 26);

        // Huy hiệu chiếm chỗ của nó (không đè chữ): rộng thêm gap + huy hiệu.
        const QString longText = u"Chọn các đối tượng tương tự trong toàn bản vẽ"_s;
        Menu a, b;
        item(a, longText);
        QAction* locked = item(b, longText, {.need = Edition::Pro});
        QCOMPARE(b.sizeHint().width() - a.sizeHint().width(),
                 px(tok::sp_4) + RichTooltip::badgeSize(Edition::Pro).width());
        // UI0-011 đổi khóa khi menu đang mở: đo lại.
        open(b);
        const int w = b.width();
        RichTooltip::attach(locked, {});
        QTRY_VERIFY(b.width() < w);
    }

    void sheet_data()
    {
        QTest::addColumn<bool>("light");
        QTest::addColumn<qreal>("dpr");
        for (bool light : {false, true})
            for (qreal dpr : {1.0, 1.5, 2.0})
                QTest::addRow("%s-%d", light ? "light" : "dark", qRound(dpr * 100)) << light << dpr;
    }

    // DoD-U 1: menu mẫu (mục đang trỏ, dấu chọn, biểu tượng, phím, khóa Pro và Viewer Pro, vô hiệu, menu con, nhóm) và
    // tooltip mức 1, mức 2 có huy hiệu.
    void sheet()
    {
        QFETCH(bool, light);
        QFETCH(qreal, dpr);
        tm_->setTheme(light ? Theme::Light : Theme::Dark);
        QCoreApplication::processEvents(); // Qt 6.11 gửi ApplicationPaletteChange qua hàng đợi
        const QColor bg = tm_->color(tok::c_base);
        const auto render = [&](QWidget* w) {
            QImage img(w->size() * dpr, QImage::Format_ARGB32_Premultiplied);
            img.setDevicePixelRatio(dpr);
            img.fill(bg);
            QPainter p(&img);
            w->render(&p, {}, {}, QWidget::DrawWindowBackground | QWidget::DrawChildren);
            return img;
        };

        Sample s;
        s.menu.ensurePolished();
        s.menu.resize(s.menu.sizeHint());
        s.menu.setActiveAction(s.props);
        const QImage menu = render(&s.menu);
        QAction circle, calc;
        RichTooltip::attach(
            &circle,
            {.name = u"Đường tròn"_s, .command = u"CIRCLE"_s, .description = u"Vẽ đường tròn theo tâm và bán kính."_s});
        RichTooltip::attach(&calc, {.name = u"Máy tính nhanh"_s,
                                    .command = u"QUICKCALC"_s,
                                    .keyId = u"qc"_s,
                                    .description = u"Tính biểu thức, lấy số đo từ bản vẽ."_s,
                                    .need = Edition::Pro});
        QList<QImage> tips;
        for (auto [target, level] : {std::pair{&circle, 1}, std::pair{&calc, 2}}) {
            tips_->setDelays(1ms, level == 1 ? 1h : 1ms);
            tips_->hover(target, QRect(100, 100, 24, 24), QPoint(112, 112));
            QVERIFY(spinUntil([&] { return tips_->level() == level; }));
            tips << render(tips_->tipWindow());
            tips_->leave();
        }

        // Ghép: menu bên trái, tooltip xếp dọc bên phải, cách nhau 8 px.
        const auto logical = [dpr](const QImage& img) { return (QSizeF(img.size()) / dpr).toSize(); };
        const QSize m = logical(menu), t0 = logical(tips[0]), t1 = logical(tips[1]);
        const QPoint at0(8 + m.width() + 16, 8), at1(at0.x(), 8 + t0.height() + 8);
        QImage sheet(
            QSize(at0.x() + std::max(t0.width(), t1.width()) + 8, std::max(8 + m.height(), at1.y() + t1.height()) + 8) *
                dpr,
            QImage::Format_ARGB32_Premultiplied);
        sheet.setDevicePixelRatio(dpr);
        sheet.fill(bg);
        {
            QPainter p(&sheet);
            p.drawImage(QPoint(8, 8), menu);
            p.drawImage(at0, tips[0]);
            p.drawImage(at1, tips[1]);
        }

        const QString name = u"sheet-%1-%2-%3.png"_s.arg(light ? u"light"_s : u"dark"_s)
                                 .arg(qRound(dpr * 100))
                                 .arg(QLatin1StringView(kPlatform));
        const QString out = qEnvironmentVariable("TRION_UPDATE_BASELINE");
        if (!out.isEmpty())
            QVERIFY(sheet.save(out + u'/' + name, "PNG", 0)); // nén cao nhất

        // Không phụ thuộc font: nền mục đang trỏ --c-accent-soft trên --c-raised, huy hiệu Pro --c-ed-pro, Viewer Pro
        // --c-ed-vp.
        const auto pixel = [&](QPoint logicalPt) { return sheet.pixelColor((QPointF(logicalPt) * dpr).toPoint()); };
        const QRect hot = s.menu.actionGeometry(s.props).translated(8, 8);
        const QColor soft = tm_->color(tok::c_accent_soft), raised = tm_->color(tok::c_raised); // soft có alpha
        const auto mix = [&](double a, double b) { return a * soft.alphaF() + b * (1 - soft.alphaF()); };
        const QColor hotBg =
            QColor::fromRgbF(float(mix(soft.redF(), raised.redF())), float(mix(soft.greenF(), raised.greenF())),
                             float(mix(soft.blueF(), raised.blueF())));
        QVERIFY(near(pixel(QPoint(hot.left() + 2, hot.center().y())), hotBg));
        for (auto [a, c] : {std::pair{s.similar, tok::c_ed_pro}, std::pair{s.pdf, tok::c_ed_vp}}) {
            const QRect r = s.menu.actionGeometry(a).translated(8, 8);
            const int keyW = a == s.pdf
                                 ? QFontMetrics(font(TextRole::MonoCaption)).horizontalAdvance(Menu::keyText(a)) +
                                       px(tok::sp_4) + px(tok::sp_6) + px(tok::sp_1)
                                 : 0;
            const QSize b = RichTooltip::badgeSize(RichTooltip::info(a).need);
            QVERIFY2(near(pixel(QPoint(r.right() + 1 - px(tok::sp_4) - keyW - b.width() + 2, r.center().y())),
                          tm_->color(c)),
                     qPrintable(a->text()));
        }

        if (!out.isEmpty())
            QSKIP("đã ghi baseline");
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

    void pseudoNoTruncation()
    {
        MenuSample w;
        w.resize(200, 100);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        const auto report = [&w] {
            QStringList out;
            for (const i18n::Truncation& t : i18n::findTruncations(&w))
                out << t.toString();
            return out.join(u"; "_s);
        };
        QCOMPARE(report(), QString());
        const QString vi = w.props->text();
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QTRY_VERIFY(w.props->text().size() >= vi.size() + 2);
        QCOMPARE(report(), QString()); // menu đóng: cỡ sẽ mở
        w.menu->popup(w.mapToGlobal(QPoint(10, 10)));
        QVERIFY(QTest::qWaitForWindowExposed(w.menu));
        w.menu->setActiveAction(w.menu->actions().at(3));
        QTest::keyClick(w.menu, Qt::Key_Right);
        QVERIFY(w.recent->isVisible());
        for (Density d : {Density::Comfortable, Density::Compact}) {
            tm_->setDensity(d);
            QTRY_COMPARE(report(), QString());
        }
        w.menu->hide();
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QTRY_COMPARE(w.props->text(), vi);
    }
};

QTEST_MAIN(MenuTest)
#include "menu_test.moc"
