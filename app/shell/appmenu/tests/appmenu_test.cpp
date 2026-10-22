// UIA-003: menu ứng dụng (AppMenu.h). DOC-018 (mở tệp), INT-002 (chạy lệnh), DWG-026/RND-021 (ảnh) chưa có: sink giả
// của ActionRegistry, macroRequested, openRequested ghi lại; nguồn ảnh giả; tệp tạm trên đĩa; phiên bản đặt qua
// EditionGate::setEdition. Tiêu chí của task:
//   1  openTime: 50 tệp gần đây, popupAt → khung đầu có tệp ≤ 100 ms CPU luồng UI (release).
//   2  mapping: bảng đối chiếu mọi mục (trái, menu con, hai nút) → chuỗi lệnh; menu đóng trước khi lệnh chạy.
//   3  search: mỗi phím ≤ 16 ms CPU (release), kết quả lệnh (chỉ mục của ô Tìm lệnh) và tệp; Enter chạy kết quả đầu.
//   4  viewer: Viewer bỏ Mới, Lưu, Lưu thành; mục Pro, Viewer Pro có huy hiệu, mở trang nâng cấp, không phát lệnh.
//   5  escape: Esc đóng (cả khi ô tìm có chữ), focus về vùng vẽ; Esc trong menu con chỉ đóng menu con.
//   Còn lại: keyboard (↑↓ trong danh sách, ←→ giữa cột), outsideClick, pinAndSort, preview, a11y (checkNames,
//   checkFocus), pseudoNoTruncation (vi-XL), pixels (màu không có chữ, Tối và Sáng), tooltip, idle.
#include "app/shell/appmenu/AppMenu.h"

#include "app/core/a11y/AccessibilityAudit.h"
#include "app/core/actions/ActionRegistry.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/keymap/Keymap.h"
#include "app/core/UiExecutor.h"
#include "app/shell/titlebar/CommandSearch.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/settings/Settings.h"
#include "tests/timing.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QBoxLayout>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtTest>

#include <atomic>
#include <memory>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using trion::shell::AppMenu;
using trion::shell::CommandSearch;
using trion::shell::RecentFiles;
using trion::shell::ThumbnailQueue;
using trion::test::kTimed;
namespace actions = trion::actions;
namespace edition = trion::edition;
namespace i18n = trion::i18n;
namespace keymap = trion::keymap;
namespace sched = trion::sched;
namespace settings = trion::settings;
namespace theme = trion::theme;
namespace ui = trion::ui;
using Clock = std::chrono::steady_clock;
using Edition = edition::Edition;

namespace {

constexpr QRgb kThumbColor = qRgb(60, 200, 192); // ảnh của nguồn render giả

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

int px(tok::PxId id)
{
    return qRound(theme::ThemeManager::instance().px(id));
}

QColor col(tok::ColorId id)
{
    return theme::ThemeManager::instance().color(id);
}

// Màu trong suốt fg phủ lên nền đặc bg (như trình duyệt vẽ --c-accent-soft trên --c-raised).
QColor over(const QColor& fg, const QColor& bg)
{
    const int a = fg.alpha();
    const auto mix = [a](int f, int b) { return (f * a + b * (255 - a) + 127) / 255; };
    return QColor(mix(fg.red(), bg.red()), mix(fg.green(), bg.green()), mix(fg.blue(), bg.blue()));
}

QString truncations(QWidget* root)
{
    QStringList out;
    for (const i18n::Truncation& t : i18n::findTruncations(root))
        out << t.toString();
    return out.join(u"; "_s);
}

// Bảng đối chiếu (tiêu chí 2): mục → chuỗi lệnh. item = ItemCount: nút dưới (Tùy chọn, Thoát). Chữ của mục chạy QAction
// của ActionRegistry là nhãn của registry; mục có menu con trên cột trái không phát lệnh (mở menu con).
struct Expect {
    AppMenu::Item item;
    const char* text;
    const char* macro;
};
const Expect kTable[] = {
    {AppMenu::New, "Mới", "^C^C_NEW"},
    {AppMenu::Open, "Mở", "^C^C_OPEN"},
    {AppMenu::Save, "Lưu", "^C^C_QSAVE"},
    {AppMenu::SaveAs, "Lưu thành", "^C^C_SAVEAS"},
    {AppMenu::SaveAs, "DWG 2018 (khuyến nghị)", "^C^C_SAVEAS _2018 ~"},
    {AppMenu::SaveAs, "DWG 2013", "^C^C_SAVEAS _2013 ~"},
    {AppMenu::SaveAs, "DWG 2010", "^C^C_SAVEAS _2010 ~"},
    {AppMenu::SaveAs, "DWG 2007", "^C^C_SAVEAS _2007 ~"},
    {AppMenu::SaveAs, "DWG 2004", "^C^C_SAVEAS _2004 ~"},
    {AppMenu::SaveAs, "DWG 2000", "^C^C_SAVEAS _2000 ~"},
    {AppMenu::SaveAs, "DXF 2018", "^C^C_SAVEAS _DXF _2018 ~"},
    {AppMenu::SaveAs, "DXF 2013", "^C^C_SAVEAS _DXF _2013 ~"},
    {AppMenu::SaveAs, "DXF 2010", "^C^C_SAVEAS _DXF _2010 ~"},
    {AppMenu::SaveAs, "DXF 2007", "^C^C_SAVEAS _DXF _2007 ~"},
    {AppMenu::SaveAs, "DXF 2004", "^C^C_SAVEAS _DXF _2004 ~"},
    {AppMenu::SaveAs, "DXF 2000", "^C^C_SAVEAS _DXF _2000 ~"},
    {AppMenu::SaveAs, "Mẫu bản vẽ (DWT)", "^C^C_SAVEAS _Template ~"},
    {AppMenu::Export, "Xuất PDF", "^C^C_EXPORTPDF"},
    {AppMenu::Export, "Xuất ảnh", "^C^C_PNGOUT"},
    {AppMenu::Export, "Xuất DXF", "^C^C_SAVEAS _DXF _2018 ~"},
    {AppMenu::Print, "In", "^C^C_PLOT"},
    {AppMenu::Print, "In hàng loạt theo khung tên", "^C^C_TCBATCHPLOT"},
    {AppMenu::Print, "In nhiều layout, nhiều bản vẽ", "^C^C_PUBLISH"},
    {AppMenu::Print, "Quản lý thiết lập trang", "^C^C_PAGESETUP"},
    {AppMenu::Print, "Xem trước bản in", "^C^C_PREVIEW"},
    {AppMenu::Utilities, "Kiểm tra lỗi", "^C^C_AUDIT"},
    {AppMenu::Utilities, "Phục hồi bản vẽ lỗi", "^C^C_RECOVER"},
    {AppMenu::Utilities, "Dọn dẹp bản vẽ", "^C^C_PURGE"},
    {AppMenu::Utilities, "Thuộc tính bản vẽ", "^C^C_DWGPROPS"},
    {AppMenu::Utilities, "Đơn vị bản vẽ", "'_UNITS"},
    {AppMenu::Close, "Đóng bản vẽ", "^C^C_CLOSE"},
    {AppMenu::Close, "Đóng tất cả bản vẽ", "^C^C_CLOSEALL"},
    {AppMenu::ItemCount, "Tùy chọn", "^C^C_OPTIONS"},
    {AppMenu::ItemCount, "Thoát TrionCAD", "^C^C_QUIT"},
};

// Chữ của mục trái theo AppMenu::Item.
const char* const kLabels[] = {"Mới", "Mở", "Lưu", "Lưu thành", "Xuất", "In", "Tiện ích bản vẽ", "Đóng"};

QStringList texts(const QAbstractItemModel* m)
{
    QStringList out;
    for (int r = 0; r < m->rowCount(); ++r)
        out << m->index(r, 0).data().toString();
    return out;
}

QList<QAction*> entries(const QMenu* m)
{
    QList<QAction*> out;
    for (QAction* a : m->actions())
        if (!a->isSeparator())
            out << a;
    return out;
}

QAction* entry(const QMenu* m, const QString& text)
{
    for (QAction* a : entries(m))
        if (a->text() == text)
            return a;
    return nullptr;
}

} // namespace

class AppMenuTest : public QObject {
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
    std::unique_ptr<edition::EditionGate> gate_;
    std::unique_ptr<CommandSearch> search_; // chỉ mục của ô Tìm lệnh (TitleBar giữ trong ứng dụng)
    std::unique_ptr<QWidget> host_;         // cửa sổ chính giả: nút logo, vùng vẽ
    std::unique_ptr<RecentFiles> recent_;
    QPushButton* logo_ = nullptr;
    QLineEdit* canvas_ = nullptr; // vùng vẽ: có focus trước khi mở menu
    AppMenu* menu_ = nullptr;
    QStringList sent_;          // chuỗi lệnh tới sink của ActionRegistry (và macroRequested)
    QList<QStringList> opened_; // openRequested
    QStringList upgrades_;      // tính năng đã yêu cầu nâng cấp (trang hoặc chú thích)
    bool openAtRun_ = false;    // menu còn hiện lúc lệnh tới sink
    std::atomic<int> renders_{0};

    QString files() const { return dir_.filePath(u"files"_s); }

    // n tệp DWG thật tên theo name(i), ngày sửa giảm dần, cỡ tăng dần theo i; pinned: các dòng ghim.
    QStringList seed(int n, const std::function<QString(int)>& name, const QList<int>& pinned = {})
    {
        QStringList paths;
        QJsonArray list;
        QDir().mkpath(files());
        for (int i = 0; i < n; ++i) {
            const QString path = files() + u'/' + name(i);
            QFile f(path);
            if (!f.open(QIODevice::WriteOnly))
                return {};
            f.write(QByteArray("AC1032") + QByteArray(122 + 100 * i, '\0'));
            f.close();
            const QDateTime t =
                QDateTime::fromMSecsSinceEpoch(1'770'000'000'000 - qint64(i) * 86'400'000, QTimeZone::UTC);
            QFile g(path);
            if (!g.open(QIODevice::ReadWrite) || !g.setFileTime(t, QFileDevice::FileModificationTime))
                return {};
            QJsonObject o{{"path"_L1, path},
                          {"size"_L1, double(128 + 100 * i)},
                          {"version"_L1, u"DWG 2018"_s},
                          {"modified"_L1, double(t.toMSecsSinceEpoch())}};
            if (pinned.contains(i))
                o.insert("pinned"_L1, true);
            list.append(o);
            paths << path;
        }
        store_->setValue(u"recent/files"_s, list);
        return paths;
    }
    QStringList seed(int n)
    {
        return seed(n, [](int i) { return u"Ban-ve-%1.dwg"_s.arg(i, 3, 10, QChar(u'0')); });
    }

    void destroyMenu()
    {
        while (QWidget* p = QApplication::activePopupWidget())
            p->close();
        sched::waitIdle(); // việc nền dùng recent_, nguồn ảnh
        delete menu_;
        menu_ = nullptr;
        QCoreApplication::processEvents();
        recent_.reset();
    }

    // Menu mới trên danh sách tệp gần đây đang có trong Store.
    void makeMenu()
    {
        destroyMenu();
        recent_ = std::make_unique<RecentFiles>(*store_);
        menu_ = new AppMenu(*reg_, *recent_, *search_, host_.get());
        menu_->thumbnails().setRenderSource([this](const QString&, QSize size, sched::CancelToken) {
            ++renders_;
            QImage image(size, QImage::Format_ARGB32_Premultiplied);
            image.fill(kThumbColor);
            return image;
        });
        connect(menu_, &AppMenu::macroRequested, this, [this](const QString& m) {
            openAtRun_ = openAtRun_ || menu_->isVisible();
            sent_ << m;
        });
        connect(menu_, &AppMenu::openRequested, this, [this](const QStringList& p) {
            openAtRun_ = openAtRun_ || menu_->isVisible();
            opened_ << p;
        });
    }

    QPoint anchor() const { return logo_->mapToGlobal(QPoint(0, logo_->height())); }

    // Mở như TitleBar khi vùng vẽ đang có focus.
    bool open()
    {
        host_->activateWindow();
        canvas_->setFocus(Qt::OtherFocusReason);
        menu_->popupAt(anchor());
        return QTest::qWaitForWindowExposed(menu_) && spinUntil([&] { return menu_->search()->hasFocus(); });
    }
    bool closed()
    {
        return spinUntil([&] { return !menu_->isVisible(); });
    }

    // Phím như bàn phím thật: tới popup đang mở (menu con, menu ứng dụng) hoặc widget có focus.
    static QWidget* target()
    {
        if (QWidget* p = QApplication::activePopupWidget())
            return p->focusWidget() ? p->focusWidget() : p;
        return QApplication::focusWidget();
    }
    static void key(Qt::Key k, Qt::KeyboardModifiers m = Qt::NoModifier) { QTest::keyClick(target(), k, m); }

    QAbstractItemView* items() const { return menu_->items(); }
    QAbstractItemView* cmds() const { return menu_->commands(); }
    QAbstractItemView* fileList() const { return menu_->files(); }
    int row(QAbstractItemView* v) const { return v->currentIndex().row(); }
    int itemRow(const QString& text) const { return int(texts(items()->model()).indexOf(text)); }
    QStringList shownFiles() const
    {
        QStringList out;
        for (int r = 0; r < menu_->filter().rowCount(); ++r)
            out << menu_->filter().entry(r).name;
        return out;
    }

    // Focus vào mục trái có chữ text (bằng bàn phím: ↓ từ ô tìm rồi ↓ tới dòng).
    bool focusItem(const QString& text)
    {
        const int r = itemRow(text);
        if (r < 0)
            return false;
        menu_->search()->setFocus(Qt::OtherFocusReason);
        key(Qt::Key_Down);
        for (int i = 0; i < r; ++i)
            key(Qt::Key_Down);
        return items()->hasFocus() && row(items()) == r;
    }
    // Mở menu con của mục bằng →; trả menu con đang hiện.
    QMenu* openSub(AppMenu::Item item)
    {
        if (!focusItem(QString::fromUtf8(kLabels[item])))
            return nullptr;
        key(Qt::Key_Right);
        QMenu* m = menu_->submenu(item);
        return m && spinUntil([m] { return m->isVisible(); }) ? m : nullptr;
    }

    // Tâm ô ghim của dòng tệp r (tọa độ menu): cuối dòng 2 (AppMenu::Delegate::pinRect).
    QPoint pinCenter(int r) const
    {
        const QRect rect = fileList()->visualRect(menu_->filter().index(r, 0));
        const int y = rect.top() + 5 + QFontMetrics(theme::font(theme::TextRole::Ui)).height() +
                      QFontMetrics(theme::font(theme::TextRole::Caption)).height() / 2;
        return fileList()->viewport()->mapTo(menu_, QPoint(rect.right() - px(tok::sp_4) - 6, y));
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
        reg_ = std::make_unique<actions::ActionRegistry>(*keymap_, *sysvars_);
        reg_->setSink([this](const QString& m) {
            openAtRun_ = openAtRun_ || (menu_ && menu_->isVisible());
            sent_ << m;
        });
        gate_ = std::make_unique<edition::EditionGate>();
        gate_->bind(*reg_);
        gate_->upgrade().setHandler([this](const edition::UpgradeRequest& r) { upgrades_ << r.feature; });
        connect(&gate_->upgrade(), &edition::UpgradeRouter::noticeRequested, this,
                [this](const edition::UpgradeRequest& r) { upgrades_ << r.feature; });
        search_ = std::make_unique<CommandSearch>(*reg_);

        host_ = std::make_unique<QWidget>();
        auto* col = new QVBoxLayout(host_.get());
        logo_ = new QPushButton(u"TrionCAD"_s); // i18n-ok: tên sản phẩm
        logo_->setFocusPolicy(Qt::TabFocus); // như nút trên thanh: bấm chuột không lấy focus của vùng vẽ
        canvas_ = new QLineEdit;
        canvas_->setAccessibleName(u"Vùng vẽ"_s); // i18n-ok: tên widget giả trong test
        col->addWidget(logo_);
        col->addWidget(canvas_);
        col->addStretch(1);
        host_->resize(1366, 768);
        host_->move(0, 0);
        host_->show();
        QVERIFY(QTest::qWaitForWindowExposed(host_.get()));
    }

    void init()
    {
        tm_->setTheme(theme::Theme::Dark);
        tm_->setDensity(theme::Density::Compact);
        gate_->setEdition(Edition::Pro);
        for (const char* k : {"recent/files", "recent/limit"})
            store_->remove(QString::fromLatin1(k));
        QDir(files()).removeRecursively();
        sent_.clear();
        opened_.clear();
        upgrades_.clear();
        openAtRun_ = false;
        renders_ = 0;
    }

    void cleanup()
    {
        destroyMenu();
        if (translator_->language() != u"vi"_s)
            QVERIFY(translator_->setLanguage(u"vi"_s));
    }

    void cleanupTestCase()
    {
        destroyMenu();
        sched::waitIdle();
        host_.reset();
        search_.reset();
        gate_.reset();
        reg_.reset();
        sysvars_.reset();
        tips_.reset();
        keymap_.reset();
        translator_.reset();
        icons_.reset();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    // Tiêu chí 1: 50 tệp gần đây; từ popupAt đến lần vẽ đầu của danh sách đã có tệp ≤ 100 ms CPU luồng UI (release).
    // Đo lần mở đầu của một menu mới (cửa sổ popup tạo lúc này), sau một menu khác đã mở (font, style, biểu tượng ấm
    // như sau khởi động).
    void openTime()
    {
        seed(50);
        makeMenu();
        QVERIFY(open());
        menu_->close();
        QVERIFY(closed());
        makeMenu();
        int paints = 0;
        struct Counter final : QObject {
            int* n;
            QAbstractItemView* view;
            Counter(int* p, QAbstractItemView* v) : n(p), view(v) {}
            bool eventFilter(QObject*, QEvent* e) override
            {
                if (e->type() == QEvent::Paint && view->model()->rowCount() > 0)
                    ++*n;
                return false;
            }
        } counter(&paints, fileList());
        fileList()->viewport()->installEventFilter(&counter);
        host_->activateWindow();
        canvas_->setFocus(Qt::OtherFocusReason);
        const auto cpu = trion::test::threadCpu();
        const auto start = Clock::now();
        menu_->popupAt(anchor());
        QVERIFY(spinUntil([&] { return paints > 0; }));
        const double wall = ms(Clock::now() - start), used = ms(trion::test::threadCpu() - cpu);
        fileList()->viewport()->removeEventFilter(&counter);
        QCOMPARE(fileList()->model()->rowCount(), 50);
        QVERIFY(fileList()->visualRect(menu_->filter().index(0, 0)).intersects(fileList()->viewport()->rect()));
        qInfo("mở menu (50 tệp) đến khung có nội dung: %.1f ms, CPU luồng UI %.1f ms", wall, used);
        QVERIFY2(!kTimed || trion::test::threadCpu().count() < 0 || used <= 100, qPrintable(u"CPU %1 ms"_s.arg(used)));
    }

    // Tiêu chí 2: mỗi mục phát đúng chuỗi lệnh (UI0-015: QAction của registry; còn lại macroRequested), menu đóng
    // trước khi lệnh tới sink; bảng phủ 100% mục đang có (mục trái chạy lệnh, mọi mục menu con, hai nút).
    void mapping()
    {
        seed(3);
        makeMenu();
        for (const Expect& x : kTable) {
            const QString text = QString::fromUtf8(x.text);
            sent_.clear();
            openAtRun_ = false;
            QVERIFY(open());
            if (x.item == AppMenu::ItemCount) { // nút dưới: bấm chuột
                auto* b = menu_->findChild<QPushButton*>(text == u"Tùy chọn"_s ? u"OPTIONS"_s : u"QUIT"_s);
                QVERIFY(b);
                QCOMPARE(b->text(), text);
                QTest::mouseClick(b, Qt::LeftButton);
            } else if (!menu_->submenu(x.item)) { // mục trái chạy lệnh: Enter
                QVERIFY2(focusItem(text), x.text);
                key(Qt::Key_Return);
            } else {
                QMenu* m = openSub(x.item);
                QVERIFY2(m, x.text);
                QAction* a = entry(m, text);
                QVERIFY2(a, x.text);
                a->trigger();
            }
            QVERIFY2(closed(), x.text);
            QCOMPARE(sent_, QStringList{QString::fromUtf8(x.macro)});
            QVERIFY2(!openAtRun_, x.text);
        }
        // Phủ 100%: mọi mục đang có (mục trái chạy lệnh, mục của menu con, hai nút) nằm trong bảng và ngược lại.
        QSet<QString> table;
        for (const Expect& x : kTable)
            table.insert(QString::fromUtf8(x.text));
        int count = 2; // hai nút
        for (int i = 0; i < AppMenu::ItemCount; ++i) {
            if (const QMenu* m = menu_->submenu(AppMenu::Item(i))) {
                for (const QAction* a : entries(m)) {
                    QVERIFY2(table.contains(a->text()), qPrintable(a->text()));
                    ++count;
                }
            } else {
                const QString label = QString::fromUtf8(kLabels[i]);
                QVERIFY2(texts(items()->model()).contains(label) && table.contains(label), kLabels[i]);
                ++count;
            }
        }
        QCOMPARE(count, int(std::size(kTable)));

        // Đường chuột thật của QMenu: bấm mục của menu con.
        sent_.clear();
        QVERIFY(open());
        QMenu* m = openSub(AppMenu::Print);
        QVERIFY(m);
        QAction* plot = entry(m, u"In"_s);
        QTest::mouseClick(m->windowHandle(), Qt::LeftButton, {}, m->actionGeometry(plot).center());
        QVERIFY(closed());
        QVERIFY(!m->isVisible());
        QCOMPARE(sent_, QStringList{u"^C^C_PLOT"_s});
        QVERIFY(!openAtRun_);
        QCOMPARE(QApplication::focusWidget(), canvas_);
    }

    // Tiêu chí 3: ô tìm lọc tệp và tìm lệnh mỗi phím ≤ 16 ms CPU (release); Enter chạy lệnh đầu, không có lệnh thì mở
    // tệp đầu.
    void search()
    {
        const QStringList paths = seed(50, [](int i) {
            return i % 5 == 0 ? u"Duong-ong-%1.dwg"_s.arg(i)
                   : i == 7   ? u"zqx-bang-ke.dwg"_s
                              : u"Ban-ve-%1.dwg"_s.arg(i);
        });
        makeMenu();
        QVERIFY(open());
        QCOMPARE(cmds()->isVisible(), false);
        QVERIFY(spinUntil([&] { return !recent_->refreshing(); })); // kết quả kiểm tệp ở nền đã áp
        // Phím đầu tiên của phiên còn dựng chỉ mục lệnh, nạp glyph, biểu tượng: đo riêng (chỉ báo), rồi xóa chữ.
        const auto first = trion::test::threadCpu();
        QTest::keyClicks(menu_->search(), u"lu"_s);
        QCoreApplication::processEvents();
        const double cold = ms(trion::test::threadCpu() - first);
        QTest::keyClick(menu_->search(), Qt::Key_A, Qt::ControlModifier);
        QTest::keyClick(menu_->search(), Qt::Key_Backspace);
        QCoreApplication::processEvents();
        QVERIFY(!cmds()->isVisible());
        double worst = 0;
        QStringList each;
        for (const QChar c : u"duong"_s) {
            const auto cpu = trion::test::threadCpu();
            QTest::keyClick(menu_->search(), c.toLatin1());
            QCoreApplication::processEvents(); // vẽ lại
            const double t = ms(trion::test::threadCpu() - cpu);
            worst = std::max(worst, t);
            each << QString::number(t, 'f', 2);
        }
        qInfo("CPU mỗi phím (ms): %s; hai phím đầu của phiên (dựng chỉ mục, glyph): %.2f ms",
              qPrintable(each.join(u' ')), cold);
        const QStringList expected = search_->search(u"duong"_s).mid(0, AppMenu::kMaxCommands);
        QVERIFY(!expected.isEmpty());
        QVERIFY(cmds()->isVisible());
        QCOMPARE(cmds()->model()->rowCount(), expected.size());
        QCOMPARE(cmds()->model()->index(0, 0).data().toString(), reg_->action(expected[0])->text());
        QCOMPARE(menu_->filter().rowCount(), 10);
        for (const QString& n : shownFiles())
            QVERIFY(n.startsWith(u"Duong-ong-"_s));
        qInfo("ô tìm: 50 tệp, %lld lệnh khớp; mỗi phím ≤ %.2f ms CPU luồng UI", qint64(expected.size()), worst);
        QVERIFY2(!kTimed || trion::test::threadCpu().count() < 0 || worst <= 16, qPrintable(u"CPU %1 ms"_s.arg(worst)));
        key(Qt::Key_Return); // kết quả đầu: lệnh
        QVERIFY(closed());
        QCOMPARE(sent_, QStringList{reg_->macro(expected[0])});
        QVERIFY(!openAtRun_);

        QVERIFY(open()); // chỉ tệp khớp: Enter mở tệp đầu
        QTest::keyClicks(menu_->search(), u"zqx"_s);
        QVERIFY(!cmds()->isVisible());
        QCOMPARE(shownFiles(), QStringList{u"zqx-bang-ke.dwg"_s});
        key(Qt::Key_Return);
        QVERIFY(closed());
        QCOMPARE(opened_, QList<QStringList>{{paths[7]}});

        QVERIFY(open()); // không gì khớp: chữ trống, Enter không làm gì
        QTest::keyClicks(menu_->search(), u"khong co gi"_s);
        QCOMPARE(menu_->filter().rowCount(), 0);
        QVERIFY(!cmds()->isVisible());
        key(Qt::Key_Return);
        QVERIFY(menu_->isVisible());
        QCOMPARE(sent_.size(), 1);
        QCOMPARE(opened_.size(), 1);
        // Mở lại: ô tìm trống, đủ tệp.
        key(Qt::Key_Escape);
        QVERIFY(closed());
        QVERIFY(open());
        QCOMPARE(menu_->search()->text(), QString());
        QCOMPARE(menu_->filter().rowCount(), 50);
    }

    // Tiêu chí 4: hồ sơ Viewer chỉ có mục 07.1 §2 cho phép (không Mới, Lưu, Lưu thành); mục Pro / Viewer Pro trong menu
    // con và kết quả tìm có huy hiệu (TipInfo::need, Menu vẽ), bấm thì mở trang nâng cấp, không phát lệnh. Đổi phiên
    // bản lúc đang chạy.
    void viewer()
    {
        seed(3);
        gate_->setEdition(Edition::Viewer);
        makeMenu();
        QVERIFY(open());
        QCOMPARE(texts(items()->model()), (QStringList{u"Mở"_s, u"Xuất"_s, u"In"_s, u"Tiện ích bản vẽ"_s, u"Đóng"_s}));
        const struct {
            AppMenu::Item item;
            const char* text;
            ui::Edition need;
            const char* feature;
        } locked[] = {{AppMenu::Export, "Xuất DXF", ui::Edition::Pro, "SAVEAS"},
                      {AppMenu::Print, "In hàng loạt theo khung tên", ui::Edition::ViewerPro, "TCBATCHPLOT"},
                      {AppMenu::Print, "In nhiều layout, nhiều bản vẽ", ui::Edition::ViewerPro, "PUBLISH"},
                      {AppMenu::Utilities, "Kiểm tra lỗi", ui::Edition::Pro, "AUDIT"},
                      {AppMenu::Utilities, "Dọn dẹp bản vẽ", ui::Edition::Pro, "PURGE"},
                      {AppMenu::Utilities, "Đơn vị bản vẽ", ui::Edition::Pro, "UNITS"}};
        for (const auto& l : locked) {
            QVERIFY(menu_->isVisible() || open());
            QMenu* m = openSub(l.item);
            QVERIFY2(m, l.text);
            QAction* a = entry(m, QString::fromUtf8(l.text));
            QVERIFY2(a, l.text);
            QCOMPARE(ui::RichTooltip::info(a).need, l.need); // huy hiệu "Pro" / "Viewer Pro"
            upgrades_.clear();
            a->trigger();
            QVERIFY(closed());
            QCOMPARE(upgrades_, QStringList{QString::fromLatin1(l.feature)});
            QVERIFY(sent_.isEmpty());
        }
        QVERIFY(open()); // mục được phép chạy bình thường
        QMenu* m = openSub(AppMenu::Export);
        QVERIFY(m);
        QCOMPARE(ui::RichTooltip::info(entry(m, u"Xuất PDF"_s)).need, ui::Edition::None);
        entry(m, u"Xuất PDF"_s)->trigger();
        QVERIFY(closed());
        QCOMPARE(sent_, QStringList{u"^C^C_EXPORTPDF"_s});

        // Kết quả tìm: lệnh khóa có mô tả trợ năng "Có trong TrionCAD Pro"; Enter mở trang nâng cấp.
        sent_.clear();
        upgrades_.clear();
        QVERIFY(open());
        QTest::keyClicks(menu_->search(), u"line"_s);
        QCOMPARE(cmds()->model()->index(0, 0).data(Qt::AccessibleDescriptionRole).toString(),
                 edition::EditionGate::availableIn(ui::Edition::Pro));
        key(Qt::Key_Return);
        QVERIFY(closed());
        QCOMPARE(upgrades_, QStringList{u"LINE"_s});
        QVERIFY(sent_.isEmpty());

        // Viewer Pro: in hàng loạt mở khóa; Pro (đổi lúc đang chạy): đủ 8 mục.
        gate_->setEdition(Edition::ViewerPro);
        QVERIFY(open());
        m = openSub(AppMenu::Print);
        QVERIFY(m);
        QCOMPARE(ui::RichTooltip::info(entry(m, u"In hàng loạt theo khung tên"_s)).need, ui::Edition::None);
        key(Qt::Key_Escape);
        key(Qt::Key_Escape);
        QVERIFY(closed());
        gate_->setEdition(Edition::Pro);
        QCOMPARE(items()->model()->rowCount(), int(AppMenu::ItemCount));
        gate_->setEdition(Edition::Trial);
        QCOMPARE(items()->model()->rowCount(), int(AppMenu::ItemCount));
    }

    // Tiêu chí 4 (Esc): Esc đóng menu cả khi ô tìm có chữ, focus về vùng vẽ (widget có focus trước khi mở); từ danh
    // sách cũng vậy; Esc trong menu con chỉ đóng menu con (focus về mục), Esc nữa đóng menu.
    void escape()
    {
        seed(3);
        makeMenu();
        QVERIFY(open());
        QTest::keyClicks(menu_->search(), u"abc"_s);
        key(Qt::Key_Escape);
        QVERIFY(closed());
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == canvas_; }));

        QVERIFY(open());
        key(Qt::Key_Down);
        QVERIFY(items()->hasFocus());
        key(Qt::Key_Escape);
        QVERIFY(closed());
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == canvas_; }));

        QVERIFY(open());
        QMenu* m = openSub(AppMenu::SaveAs);
        QVERIFY(m);
        key(Qt::Key_Escape);
        QVERIFY(spinUntil([&] { return !m->isVisible(); }));
        QVERIFY(menu_->isVisible());
        QVERIFY(spinUntil([&] { return items()->hasFocus(); }));
        key(Qt::Key_Escape);
        QVERIFY(closed());
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == canvas_; }));
        QVERIFY(sent_.isEmpty());
    }

    // ↑↓ trong danh sách (đầu danh sách lên ô tìm), ←→ giữa hai cột, → ở mục có menu con mở menu con (mục đầu đang
    // chọn), ← trong menu con đóng nó; kết quả tìm: ↓ qua lệnh cuối sang tệp, ↑ ngược lại; Enter, Space mở tệp.
    void keyboard()
    {
        const QStringList paths = seed(5);
        makeMenu();
        QVERIFY(open());
        key(Qt::Key_Down); // ô tìm trống → mục trái
        QVERIFY(items()->hasFocus());
        QCOMPARE(row(items()), 0);
        key(Qt::Key_Down);
        key(Qt::Key_Down);
        QCOMPARE(row(items()), 2);
        key(Qt::Key_Up);
        key(Qt::Key_Up);
        QCOMPARE(row(items()), 0);
        key(Qt::Key_Up);
        QVERIFY(menu_->search()->hasFocus());
        key(Qt::Key_Down);
        key(Qt::Key_Right); // Mới không có menu con → cột phải
        QVERIFY(fileList()->hasFocus());
        QCOMPARE(row(fileList()), 0);
        key(Qt::Key_Down);
        QCOMPARE(row(fileList()), 1);
        key(Qt::Key_Left);
        QVERIFY(items()->hasFocus());
        QCOMPARE(row(items()), 0);
        key(Qt::Key_Right);
        QVERIFY(fileList()->hasFocus());
        QCOMPARE(row(fileList()), 1); // giữ dòng
        key(Qt::Key_Left);
        for (int i = 0; i < 3; ++i)
            key(Qt::Key_Down);
        QCOMPARE(items()->currentIndex().data().toString(), u"Lưu thành"_s);
        key(Qt::Key_Right);
        QMenu* m = menu_->submenu(AppMenu::SaveAs);
        QVERIFY(spinUntil([m] { return m->isVisible(); }));
        QCOMPARE(m->activeAction(), m->actions().constFirst());
        QVERIFY(m->geometry().left() >= menu_->geometry().left() + AppMenu::kItemsWidth - 2 * px(tok::sp_3));
        key(Qt::Key_Left);
        QVERIFY(spinUntil([m] { return !m->isVisible(); }));
        QVERIFY(spinUntil([&] { return items()->hasFocus(); }));
        QVERIFY(sent_.isEmpty());

        menu_->search()->setFocus(Qt::OtherFocusReason); // có kết quả lệnh và tệp
        QTest::keyClicks(menu_->search(), u"ban"_s);
        const int nCmd = cmds()->model()->rowCount();
        QVERIFY(nCmd > 0);
        QVERIFY(menu_->filter().rowCount() > 0);
        key(Qt::Key_Down);
        QVERIFY(cmds()->hasFocus());
        QCOMPARE(row(cmds()), 0);
        for (int i = 1; i < nCmd; ++i)
            key(Qt::Key_Down);
        QCOMPARE(row(cmds()), nCmd - 1);
        key(Qt::Key_Down);
        QVERIFY(fileList()->hasFocus());
        QCOMPARE(row(fileList()), 0);
        key(Qt::Key_Up);
        QVERIFY(cmds()->hasFocus());
        QCOMPARE(row(cmds()), nCmd - 1);
        key(Qt::Key_Left);
        QVERIFY(items()->hasFocus());
        QCOMPARE(items()->currentIndex().data().toString(), u"Lưu thành"_s); // giữ dòng; → ở đây mở menu con
        key(Qt::Key_Up);
        key(Qt::Key_Right); // "Lưu" không có menu con → cột phải, dòng cũ
        QVERIFY(cmds()->hasFocus());
        QCOMPARE(row(cmds()), nCmd - 1);
        key(Qt::Key_Down);
        QVERIFY(fileList()->hasFocus());
        key(Qt::Key_Space);
        QVERIFY(closed());
        QCOMPARE(opened_, QList<QStringList>{{menu_->filter().files().entry(0).path}});
        QVERIFY(paths.contains(opened_[0][0]));
        QVERIFY(sent_.isEmpty());
    }

    // Bấm ra ngoài: đóng, cú bấm không tới widget bên dưới; bấm lại nút logo đóng mà không mở lại.
    void outsideClick()
    {
        seed(3);
        makeMenu();
        int logoClicks = 0;
        connect(logo_, &QPushButton::clicked, menu_, [&] {
            ++logoClicks;
            menu_->popupAt(anchor());
        });
        QWindow* win = host_->windowHandle();
        const QPoint logo = logo_->mapTo(host_.get(), logo_->rect().center());
        host_->activateWindow();
        canvas_->setFocus(Qt::OtherFocusReason);
        QTest::mouseClick(win, Qt::LeftButton, {}, logo);
        QVERIFY(spinUntil([&] { return menu_->isVisible(); }));
        QCOMPARE(logoClicks, 1);
        QTest::mouseClick(win, Qt::LeftButton, {}, logo);
        QVERIFY(closed());
        // Qt không phát lại cú bấm đã đóng popup cho widget bên dưới (Windows phát lại nếu không có cờ; plugin
        // offscreen không phát lại nên kiểm thêm cờ).
        QVERIFY(menu_->testAttribute(Qt::WA_NoMouseReplay));
        spinFor(50ms);
        QVERIFY(!menu_->isVisible());
        QCOMPARE(logoClicks, 1);

        int presses = 0;
        struct Count final : QObject {
            int* n;
            explicit Count(int* p) : n(p) {}
            bool eventFilter(QObject*, QEvent* e) override
            {
                *n += e->type() == QEvent::MouseButtonPress;
                return false;
            }
        } count(&presses);
        canvas_->installEventFilter(&count);
        QVERIFY(open());
        QTest::mouseClick(win, Qt::LeftButton, {}, canvas_->mapTo(host_.get(), canvas_->rect().center()));
        QVERIFY(closed());
        spinFor(20ms);
        canvas_->removeEventFilter(&count);
        QCOMPARE(presses, 0);
        QVERIFY(sent_.isEmpty());
        QVERIFY(opened_.isEmpty());

        // Menu con đang mở, bấm ra ngoài cả hai: đóng hết.
        QVERIFY(open());
        QMenu* m = openSub(AppMenu::Close);
        QVERIFY(m);
        QTest::mouseClick(m->windowHandle(), Qt::LeftButton, {},
                          m->mapFromGlobal(canvas_->mapToGlobal(canvas_->rect().center())));
        QVERIFY(spinUntil([&] { return !m->isVisible() && !menu_->isVisible(); }));
        spinFor(20ms);
        QVERIFY(!menu_->isVisible());
        QCOMPARE(logoClicks, 1);
    }

    // Ghim lên đầu; sắp theo ngày (mới trước), tên (A→Z), kích thước (lớn trước); bấm ô ghim cuối dòng ghim/bỏ ghim mà
    // không mở tệp, menu vẫn mở; menu chuột phải (bàn phím) Ghim/Bỏ ghim.
    void pinAndSort()
    {
        // Ngày giảm, cỡ tăng theo i; tên đặt để ba cách sắp cho ba thứ tự khác nhau; c-ba ghim.
        const QStringList names{u"b-moi-nhat.dwg"_s, u"d-hai.dwg"_s, u"c-ba.dwg"_s, u"a-cu-nhat.dwg"_s};
        seed(4, [&](int i) { return names[i]; }, {2});
        makeMenu();
        QVERIFY(open());
        QVERIFY(spinUntil([&] { return !recent_->refreshing(); }));
        QCOMPARE(shownFiles(), (QStringList{u"c-ba.dwg"_s, u"b-moi-nhat.dwg"_s, u"d-hai.dwg"_s, u"a-cu-nhat.dwg"_s}));
        auto* sort = menu_->findChild<QComboBox*>(u"appMenuSort"_s);
        QVERIFY(sort);
        QCOMPARE(sort->count(), 3);
        sort->setCurrentIndex(1); // tên
        QCOMPARE(shownFiles(), (QStringList{u"c-ba.dwg"_s, u"a-cu-nhat.dwg"_s, u"b-moi-nhat.dwg"_s, u"d-hai.dwg"_s}));
        sort->setCurrentIndex(2); // kích thước
        QCOMPARE(shownFiles(), (QStringList{u"c-ba.dwg"_s, u"a-cu-nhat.dwg"_s, u"d-hai.dwg"_s, u"b-moi-nhat.dwg"_s}));
        sort->setCurrentIndex(0);
        QCOMPARE(shownFiles().constFirst(), u"c-ba.dwg"_s);

        // Ô ghim của "d-hai" (dòng 2): ghim, lên nhóm ghim (mới hơn c-ba), không mở, menu vẫn mở.
        const QPoint pin = pinCenter(2);
        QTest::mouseMove(menu_->windowHandle(), pin);
        QTest::mouseClick(menu_->windowHandle(), Qt::LeftButton, {}, pin);
        QVERIFY(spinUntil([&] { return recent_->entry(recent_->indexOf(files() + u"/d-hai.dwg"_s)).pinned; }));
        QVERIFY(menu_->isVisible());
        QVERIFY(opened_.isEmpty());
        QCOMPARE(shownFiles(), (QStringList{u"d-hai.dwg"_s, u"c-ba.dwg"_s, u"b-moi-nhat.dwg"_s, u"a-cu-nhat.dwg"_s}));
        // Menu chuột phải từ bàn phím (Shift+F10 / phím Menu: QContextMenuEvent tại dòng hiện hành) → Bỏ ghim.
        fileList()->setFocus(Qt::OtherFocusReason);
        fileList()->setCurrentIndex(menu_->filter().index(0, 0));
        const QPoint at = fileList()->visualRect(fileList()->currentIndex()).center();
        QContextMenuEvent e(QContextMenuEvent::Keyboard, at, fileList()->viewport()->mapToGlobal(at));
        QApplication::sendEvent(fileList()->viewport(), &e);
        auto* context = menu_->findChild<QMenu*>(u"appMenuContext"_s);
        QVERIFY(spinUntil([&] { return context->isVisible(); }));
        QAction* unpin = entry(context, RecentFiles::tr("Bỏ ghim"));
        QVERIFY(unpin);
        unpin->trigger();
        context->hide();
        QVERIFY(!recent_->entry(recent_->indexOf(files() + u"/d-hai.dwg"_s)).pinned);
        QVERIFY(menu_->isVisible());
        QCOMPARE(shownFiles().constFirst(), u"c-ba.dwg"_s);
        // Bấm giữa dòng: mở tệp.
        const QPoint middle =
            fileList()->viewport()->mapTo(menu_, fileList()->visualRect(menu_->filter().index(1, 0)).center());
        QTest::mouseClick(menu_->windowHandle(), Qt::LeftButton, {}, middle);
        QVERIFY(closed());
        QCOMPARE(opened_, QList<QStringList>{{files() + u"/b-moi-nhat.dwg"_s}});
    }

    // Rê một tệp: ảnh xem trước bên phải menu (ThumbnailQueue, nguồn giả), điền khi ảnh xong; tệp không còn trên đĩa:
    // "Không tìm thấy", không xin ảnh; rời danh sách, đóng menu: ẩn. Rê mục lệnh: tooltip 2 mức của QAction.
    void preview()
    {
        const QStringList paths = seed(3);
        QVERIFY(QFile::remove(paths[2]));
        makeMenu();
        QVERIFY(open());
        QVERIFY(spinUntil([&] { return !recent_->refreshing(); }));
        QWidget* pv = menu_->preview();
        const auto rowCenter = [&](int r) {
            return fileList()->viewport()->mapTo(menu_, fileList()->visualRect(menu_->filter().index(r, 0)).center());
        };
        QTest::mouseMove(menu_->windowHandle(), rowCenter(0));
        QVERIFY(spinUntil([&] { return pv->isVisible(); }));
        // Bên phải menu khi đủ chỗ (màn hình offscreen 800 px: không đủ, nằm trong màn hình).
        const QRect avail = menu_->screen()->availableGeometry();
        if (menu_->geometry().right() + px(tok::sp_2) + pv->width() < avail.right())
            QVERIFY(pv->geometry().left() > menu_->geometry().right());
        QVERIFY(avail.contains(pv->geometry()));
        QVERIFY(spinUntil([&] { return renders_ == 1 && !menu_->thumbnails().busy(); }));
        const QImage shot = pv->grab().toImage();
        const QPoint mid(pv->width() / 2, px(tok::sp_3) + AppMenu::kPreviewWidth * 10 / 32);
        QCOMPARE(QColor(shot.pixel(mid * shot.devicePixelRatio())), QColor(kThumbColor));

        QTest::mouseMove(menu_->windowHandle(), rowCenter(2)); // tệp đã xóa
        QVERIFY(spinUntil([&] { return menu_->filter().entry(2).missing; }));
        spinFor(20ms);
        QCOMPARE(renders_.load(), 1);
        QVERIFY(pv->isVisible());

        // Mục "Mới": tooltip của QAction NEW (tên lệnh tiếng Anh, phím); ảnh xem trước ẩn.
        const QPoint item =
            items()->viewport()->mapTo(menu_, items()->visualRect(items()->model()->index(0, 0)).center());
        QTest::mouseMove(menu_->windowHandle(), item);
        QVERIFY(spinUntil([&] { return !pv->isVisible(); }));
        QCOMPARE(tips_->target(), static_cast<QObject*>(reg_->action(u"NEW"_s)));
        QTest::mouseMove(menu_->windowHandle(), rowCenter(1));
        QVERIFY(spinUntil([&] { return pv->isVisible(); }));
        QVERIFY(tips_->target() == nullptr);
        menu_->close();
        QVERIFY(closed());
        QVERIFY(!pv->isVisible());
        QVERIFY(!menu_->thumbnails().busy());
    }

    // UI0-013: tên cho trình đọc màn hình, vòng Tab đủ điểm dừng có viền focus (khi có và không có kết quả lệnh).
    void a11y()
    {
        seed(5);
        makeMenu();
        QVERIFY(open());
        QStringList order;
        QStringList issues = trion::a11y::checkNames(menu_) + trion::a11y::checkFocus(menu_, &order);
        QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
        qInfo("Tab: %s", qPrintable(order.join(u" → "_s)));
        QCOMPARE(order.size(), 6); // ô tìm, mục, sắp xếp, tệp, Tùy chọn, Thoát
        QTest::keyClicks(menu_->search(), u"ban"_s);
        QVERIFY(cmds()->isVisible());
        order.clear();
        issues = trion::a11y::checkNames(menu_) + trion::a11y::checkFocus(menu_, &order);
        QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
        QCOMPARE(order.size(), 7);
        QCOMPARE(menu_->accessibleName(), u"Menu ứng dụng"_s);
        // Danh sách trống vẫn là điểm dừng có viền.
        QTest::keyClicks(menu_->search(), u"khong khop"_s);
        QCOMPARE(menu_->filter().rowCount(), 0);
        issues = trion::a11y::checkFocus(menu_);
        QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
    }

    // vi-XL (chuỗi dài hơn 40%): không chữ nào bị cắt ở cả hai mật độ: mục trái, tiêu đề, nút, kết quả lệnh dài (xuống
    // dòng), menu con.
    void pseudoNoTruncation()
    {
        seed(5);
        makeMenu();
        const QString before = items()->model()->index(0, 0).data().toString();
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QVERIFY(spinUntil([&] { return items()->model()->index(0, 0).data().toString() != before; }));
        for (const theme::Density d : {theme::Density::Comfortable, theme::Density::Compact}) {
            tm_->setDensity(d);
            QVERIFY(open());
            QCOMPARE(truncations(menu_), QString());
            QTest::keyClicks(menu_->search(), u"do"_s); // "Đo nhanh, khoảng cách, bán kính, góc, diện tích"…
            QVERIFY(cmds()->isVisible());
            QCoreApplication::processEvents();
            QCOMPARE(truncations(menu_), QString());
            for (int item = 0; item < AppMenu::ItemCount; ++item)
                if (QMenu* m = menu_->submenu(AppMenu::Item(item))) {
                    m->popup(menu_->mapToGlobal(QPoint(AppMenu::kItemsWidth, 0)));
                    QVERIFY(spinUntil([m] { return m->isVisible(); }));
                    QCOMPARE(truncations(m), QString());
                    m->hide();
                }
            menu_->close();
            QVERIFY(closed());
        }
    }

    // Màu (ảnh không chữ, mọi nền tảng): nền --c-raised, viền --c-border-ctl, vạch --c-border, mục đang chọn
    // --c-accent-soft và viền --c-focus khi đi bằng bàn phím; Tối và Sáng.
    void pixels()
    {
        seed(2);
        makeMenu();
        for (const theme::Theme t : {theme::Theme::Dark, theme::Theme::Light}) {
            tm_->setTheme(t);
            QVERIFY(open());
            key(Qt::Key_Down); // mục "Mới" đang chọn, focus bàn phím
            const QImage img = menu_->grab().toImage();
            const qreal dpr = img.devicePixelRatio();
            const auto at = [&](QPoint p) { return QColor(img.pixel((QPointF(p) * dpr).toPoint())); };
            QCOMPARE(at(QPoint(0, 0)), col(tok::c_border_ctl));
            const QRect files = QRect(fileList()->viewport()->mapTo(menu_, QPoint()), fileList()->viewport()->size());
            QCOMPARE(at(files.bottomLeft() - QPoint(-2, 2)), col(tok::c_raised)); // dưới tệp cuối: nền
            const QRect body =
                menu_->findChild<QWidget*>(u"appMenuItems"_s)->parentWidget()->parentWidget()->geometry();
            QCOMPARE(at(QPoint(body.center().x(), body.top())), col(tok::c_border));
            const QRect row0 = items()->visualRect(items()->model()->index(0, 0));
            const QPoint inRow = items()->viewport()->mapTo(menu_, QPoint(row0.left() + 4, row0.center().y()));
            QCOMPARE(at(inRow), over(col(tok::c_accent_soft), col(tok::c_raised)));
            const QPoint ring = items()->viewport()->mapTo(menu_, QPoint(row0.center().x(), row0.top()));
            QCOMPARE(at(ring), col(tok::c_focus));
            key(Qt::Key_Escape);
            QVERIFY(closed());
        }
    }

    // Rảnh không timer: đóng menu thì không QTimer nào của menu chạy; mở lại nhiều lần không thêm widget.
    void idle()
    {
        seed(3);
        makeMenu();
        QVERIFY(open());
        menu_->close();
        QVERIFY(closed());
        const qsizetype children = menu_->findChildren<QObject*>().size();
        for (int i = 0; i < 3; ++i) {
            QVERIFY(open());
            QMenu* m = openSub(AppMenu::SaveAs);
            QVERIFY(m);
            key(Qt::Key_Escape);
            QTest::keyClicks(menu_->search(), u"lu"_s);
            key(Qt::Key_Escape);
            QVERIFY(closed());
        }
        spinFor(20ms);
        for (const QTimer* t : menu_->findChildren<QTimer*>())
            QVERIFY2(!t->isActive(), qPrintable(t->objectName()));
        QCOMPARE(menu_->findChildren<QObject*>().size(), children);
    }
};

QTEST_MAIN(AppMenuTest)
#include "appmenu_test.moc"
