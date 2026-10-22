// UIA-102: mô hình ribbon JSON và các loại điều khiển (RibbonModel.h, controls/RibbonControls.h).
//   2  loadEightTabs: nạp JSON 8 tab (CPU luồng, tốt nhất 5 lần; ≤ 20 ms khi kTimed), không dựng widget nào.
//      lazyPages: trang của tab dựng khi mở lần đầu, mở lại không dựng thêm.
//   3  splitRemembers: lựa chọn gần nhất chạy ở phần chính, giữ khi dựng lại với cùng Store và sau khởi động lại (Store
//      đọc lại từ tệp); splitOpens: mũi tên mở danh sách bằng chuột và ↓ (cả Alt+↓), Esc đóng; mục không tham số đi
//      qua QAction của ActionRegistry.
//   4  galleryVirtualized: 2.000 mục không tạo 2.000 widget, một bước cuộn (CPU luồng, p99 ≤ 16,6 ms khi kTimed),
//      bàn phím chọn mục; toggleFollowsSysvar: Sysvars::set đổi nút ngay trong lượt gọi, bấm chỉ gửi lệnh.
//   5  buttonStates (Tối/Sáng: điểm ảnh theo token của rê, nhấn, bật, focus, khóa; vô hiệu theo QAction), dataStates
//      (rỗng, đang tải, lỗi, có dữ liệu của dropdown, gallery, danh sách split), lockedControls (khóa theo phiên bản:
//      huy hiệu, trợ năng, bấm mở trang nâng cấp thay vì gửi lệnh), valueControls (ô số UI-C09, thanh trượt: lỗi, gửi
//      'SYSVAR giá-trị, giá trị từ sysvar không gửi lại).
//   Còn lại: locate (ngữ cảnh dịch "actions"/"ribbon"), pseudoNoTruncation (vi-XL, cả 8 tab: 0 chữ cắt; có tên cho
//   trình đọc màn hình; rảnh không timer chạy).
#include "app/core/actions/ActionRegistry.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/edition/LockBadge.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/keymap/Keymap.h"
#include "app/core/UiExecutor.h"
#include "app/shell/ribbon/controls/RibbonControls.h"
#include "app/shell/ribbon/RibbonModel.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/numeric/NumericField.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/sched/Scheduler.h"
#include "tests/timing.h"

#include <QAbstractListModel>
#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QScrollBar>
#include <QSignalSpy>
#include <QStringListModel>
#include <QTemporaryDir>
#include <QTimer>
#include <QTranslator>
#include <QtTest>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <functional>
#include <memory>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using namespace trion;
using namespace trion::ribbon;
using trion::test::kTimed;

namespace {

// Thời gian CPU của luồng gọi (Linux); nơi khác thời gian thật.
std::chrono::nanoseconds cpu()
{
#if defined(__linux__)
    timespec t{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return std::chrono::seconds(t.tv_sec) + std::chrono::nanoseconds(t.tv_nsec);
#else
    return std::chrono::steady_clock::now().time_since_epoch();
#endif
}

double ms(std::chrono::nanoseconds d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 10s)
{
    if (cond())
        return true;
    QEventLoop loop;
    QTimer tick, limit;
    QObject::connect(&tick, &QTimer::timeout, &loop, [&] {
        if (cond())
            loop.quit();
    });
    QObject::connect(&limit, &QTimer::timeout, &loop, &QEventLoop::quit);
    tick.start(5);
    limit.start(timeout);
    loop.exec();
    return cond();
}

// Model mẫu: n dòng tên + ảnh mẫu (8 màu luân phiên, như ảnh mẫu hatch đã vẽ sẵn).
class Samples final : public QAbstractListModel {
public:
    explicit Samples(int n, QObject* parent = nullptr) : QAbstractListModel(parent), n_(n)
    {
        for (int i = 0; i < 8; ++i) {
            QPixmap pm(44, 30);
            pm.fill(QColor::fromHsv(i * 45, 160, 200));
            thumbs_ << pm;
        }
    }
    int rowCount(const QModelIndex& p = {}) const override { return p.isValid() ? 0 : n_; }
    QVariant data(const QModelIndex& i, int role) const override
    {
        if (role == Qt::DisplayRole)
            return u"ANSI%1"_s.arg(i.row()); // i18n-ok: tên mẫu hatch là dữ liệu
        if (role == Qt::DecorationRole)
            return thumbs_[i.row() % 8];
        return {};
    }

private:
    int n_;
    QList<QPixmap> thumbs_;
};

// Bộ dịch đánh dấu ngữ cảnh: "ribbon" → "R:…", "actions" → "A:…".
class TagTranslator final : public QTranslator {
public:
    bool isEmpty() const override { return false; }
    QString translate(const char* context, const char* source, const char*, int) const override
    {
        const QString s = QString::fromUtf8(source);
        return qstrcmp(context, "ribbon") == 0    ? u"R:"_s + s
               : qstrcmp(context, "actions") == 0 ? u"A:"_s + s
                                                  : QString();
    }
};

} // namespace

class RibbonTest : public QObject {
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
    std::unique_ptr<RibbonModel> model_;
    std::unique_ptr<RibbonControls> controls_;
    std::unique_ptr<QWidget> window_;
    QStringList sent_;     // chỗ nối INT-002 của ActionRegistry
    QStringList macros_;   // RibbonControls::macroRequested
    QStringList upgrades_; // tính năng gửi tới trang nâng cấp (UI-L04)

    QStringList take(QStringList& l) { return std::exchange(l, {}); }

    // Điều khiển id trong tab (id mặc định là lệnh).
    const Control& control(const QString& tab, const QString& id)
    {
        static const Control none;
        std::function<const Control*(const std::vector<Item>&)> find = [&](const std::vector<Item>& items) {
            for (const Item& it : items) {
                if (it.kind == Item::Kind::Control && it.control.id == id)
                    return &it.control;
                if (const Control* c = find(it.items))
                    return c;
            }
            return static_cast<const Control*>(nullptr);
        };
        for (const Tab& t : model_->tabs())
            if (t.id == tab)
                for (const Panel& p : t.panels)
                    if (const Control* c = find(p.items))
                        return *c;
        qWarning("không thấy %s/%s", qUtf8Printable(tab), qUtf8Printable(id));
        return none;
    }

    // Dựng điều khiển trong cửa sổ mẫu, ở cỡ sizeHint.
    template <class T = QWidget>
    T* make(const Control& c, RibbonControls* owner = nullptr)
    {
        QWidget* w = (owner ? owner : controls_.get())->create(c, window_.get());
        w->setGeometry(QRect(QPoint(40, 40), w->sizeHint())); // xa con trỏ (0, 0) của offscreen: không bị rê
        w->show();
        return qobject_cast<T*>(w);
    }

    // Ảnh của riêng widget trên nền trong suốt (vẽ lại ngay, không lấy ảnh cũ của backing store).
    static QImage shot(QWidget* w)
    {
        QImage im(w->size(), QImage::Format_ARGB32_Premultiplied);
        im.fill(Qt::transparent);
        w->render(&im, {}, {}, QWidget::DrawChildren);
        return im;
    }
    static QColor pixel(QWidget* w, QPoint p) { return shot(w).pixelColor(p); }
    // Màu token (có thể trong suốt một phần: --c-hover, --c-press) phủ lên màu base (alpha thẳng).
    static QColor over(const QColor& base, const QColor& top)
    {
        const float a = top.alphaF(), b = base.alphaF() * (1 - a), out = a + b;
        if (out <= 0)
            return QColor(Qt::transparent);
        const auto mix = [&](float t, float u) { return (t * a + u * b) / out; };
        return QColor::fromRgbF(mix(top.redF(), base.redF()), mix(top.greenF(), base.greenF()),
                                mix(top.blueF(), base.blueF()), out);
    }
    // Sai số làm tròn khi trộn màu; ảnh lưu dạng premultiplied nên màu ở alpha thấp chỉ còn 255/alpha mức.
    static bool near(const QColor& a, const QColor& b)
    {
        const int tol = std::max(2, 255 / std::max(1, b.alpha()) + 1);
        return std::abs(a.red() - b.red()) <= tol && std::abs(a.green() - b.green()) <= tol &&
               std::abs(a.blue() - b.blue()) <= tol && std::abs(a.alpha() - b.alpha()) <= 2;
    }
    // Điểm p của w có màu các token phủ lần lượt lên nền trong suốt.
    bool expectToken(QWidget* w, QPoint p, std::initializer_list<tok::ColorId> layers)
    {
        QColor want(Qt::transparent);
        for (const tok::ColorId id : layers)
            want = over(want, token(id));
        const QColor got = pixel(w, p);
        if (!near(got, want))
            qWarning("%s (%d, %d): %s, cần %s", qUtf8Printable(w->objectName()), p.x(), p.y(),
                     qUtf8Printable(got.name(QColor::HexArgb)), qUtf8Printable(want.name(QColor::HexArgb)));
        return near(got, want);
    }
    QColor token(tok::ColorId id) const { return tm_->color(id); }
    int px(tok::PxId id) const { return qRound(tm_->px(id)); }

    // Trang của tab: các panel nằm ngang (khung panel là của UIA-005).
    QWidget* page(const Tab& t, RibbonControls& c, QWidget* parent = nullptr)
    {
        auto* w = new QWidget(parent);
        auto* row = new QHBoxLayout(w);
        for (const Panel& p : t.panels)
            row->addWidget(c.buildPanel(p), 0, Qt::AlignTop);
        row->addStretch();
        return w;
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
        registry_ = std::make_unique<actions::ActionRegistry>(*keymap_, *sysvars_);
        registry_->setSink([this](const QString& m) { sent_ << m; });
        gate_ = std::make_unique<edition::EditionGate>();
        gate_->bind(*registry_);
        gate_->setEdition(edition::Edition::Pro);
        gate_->upgrade().setHandler([this](const edition::UpgradeRequest& r) { upgrades_ << r.feature; });
        connect(&gate_->upgrade(), &edition::UpgradeRouter::noticeRequested, this,
                [this](const edition::UpgradeRequest& r) { upgrades_ << r.feature; });
        model_ = std::make_unique<RibbonModel>();
        controls_ = std::make_unique<RibbonControls>(*registry_, *store_);
        connect(controls_.get(), &RibbonControls::macroRequested, this, [this](const QString& m) { macros_ << m; });
        window_ = std::make_unique<QWidget>();
        window_->resize(900, 400);
        window_->show();
        QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
    }

    void init()
    {
        window_->activateWindow(); // focus bàn phím (menu popup vừa đóng có thể lấy kích hoạt)
        QVERIFY(QTest::qWaitForWindowActive(window_.get()));
        gate_->setEdition(edition::Edition::Pro);
        take(sent_);
        take(macros_);
        take(upgrades_);
    }

    void cleanup() { qDeleteAll(window_->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)); }

    void cleanupTestCase()
    {
        window_.reset();
        controls_.reset();
        model_.reset();
        gate_.reset();
        registry_.reset();
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

    void loadEightTabs()
    {
        const qsizetype widgets = QApplication::allWidgets().size();
        std::chrono::nanoseconds best = std::chrono::hours(1);
        for (int i = 0; i < 5; ++i) {
            const auto t0 = cpu();
            const RibbonModel m;
            best = std::min(best, cpu() - t0);
            QVERIFY2(m.errors().isEmpty(), qPrintable(m.errors().join(u'\n')));
        }
        qInfo("nạp JSON 8 tab: %.2f ms CPU luồng (tốt nhất 5 lần)", ms(best));
        QVERIFY2(!kTimed || best <= 20ms, qPrintable(u"%1 ms > 20 ms"_s.arg(ms(best))));
        QCOMPARE(QApplication::allWidgets().size(), widgets); // nạp không dựng widget

        QStringList ids;
        int controls = 0;
        std::function<void(const std::vector<Item>&)> count = [&](const std::vector<Item>& items) {
            for (const Item& it : items) {
                controls += it.kind == Item::Kind::Control;
                count(it.items);
            }
        };
        for (const Tab& t : model_->tabs()) {
            ids << t.id;
            for (const Panel& p : t.panels)
                count(p.items);
        }
        QCOMPARE(ids, (QStringList{u"home"_s, u"insert"_s, u"annotate"_s, u"view"_s, u"manage"_s, u"output"_s,
                                   u"vntools"_s, u"express"_s}));
        qInfo("8 tab, %d điều khiển", controls);
        QVERIFY(controls > 200);
    }

    void lazyPages()
    {
        const qsizetype before = QApplication::allWidgets().size();
        RibbonPages pages(
            int(model_->tabs().size()), [&](int i) { return page(model_->tabs()[size_t(i)], *controls_); },
            window_.get());
        QCOMPARE(QApplication::allWidgets().size(), before + 1); // chỉ khung trang, chưa tab nào
        QVERIFY(!pages.isBuilt(0));

        const auto t0 = cpu();
        pages.setCurrentTab(0);
        const auto first = cpu() - t0;
        const qsizetype home = QApplication::allWidgets().size();
        QVERIFY(pages.isBuilt(0));
        QVERIFY(!pages.isBuilt(1));
        QVERIFY(home > before + 20);
        const auto t1 = cpu();
        pages.setCurrentTab(3); // tab mở lần đầu sau khi đã có tab khác (font, biểu tượng, bảng tĩnh đã nạp)
        const auto view = cpu() - t1;
        QVERIFY(pages.isBuilt(3));
        QVERIFY(!pages.isBuilt(1));
        const qsizetype both = QApplication::allWidgets().size();
        pages.setCurrentTab(0); // mở lại: không dựng thêm
        QCOMPARE(QApplication::allWidgets().size(), both);
        QCOMPARE(pages.currentTab(), 0);
        qInfo("dựng tab lần đầu (CPU luồng): Trang chủ %.2f ms (%lld widget, tab đầu tiên của tiến trình), Hiển thị %.2f ms",
              ms(first), qlonglong(home - before - 1), ms(view));
    }

    void locate()
    {
        QCOMPARE(model_->locate(u"CIRCLE"_s), u"Trang chủ › Vẽ"_s);  // i18n-ok: nhãn của prototype
        QCOMPARE(model_->locate(u"EXTEND"_s), u"Trang chủ › Sửa"_s); // i18n-ok: mục split
        QCOMPARE(model_->locate(u"JOIN"_s), u"Trang chủ › Sửa"_s);   // i18n-ok: panel mở rộng
        QCOMPARE(model_->locate(u"TEXTSTYLE"_s), QString());
        QCOMPARE(model_->locate(u"TABLESTYLE"_s), u"Chú thích › Bảng"_s); // i18n-ok: lệnh của dropdown
        // Nhãn trùng nhãn lệnh ("Bảng" của TABLE) dịch theo ngữ cảnh "actions", còn lại "ribbon".
        QCOMPARE(RibbonModel::context(u"Bảng"_s), "actions");
        QCOMPARE(RibbonModel::context(u"Chú thích"_s), "ribbon");
        TagTranslator tag;
        QVERIFY(QCoreApplication::installTranslator(&tag));
        QCOMPARE(model_->locate(u"TABLESTYLE"_s), u"R:Chú thích › A:Bảng"_s); // i18n-ok: thử ngữ cảnh
        QCOMPARE(RibbonModel::line(u"Đoạn\nthẳng"_s), u"R:Đoạn thẳng"_s);     // i18n-ok: thử ngữ cảnh
        QVERIFY(QCoreApplication::removeTranslator(&tag));
    }

    void splitOpens()
    {
        auto* circle = make<RibbonButton>(control(u"home"_s, u"CIRCLE"_s)); // nút lớn
        QVERIFY(circle && circle->menu());
        // Nút lớn: nửa dưới là mũi tên; nút nhỏ: vùng ▾ ở mép cuối.
        QVERIFY(circle->arrowZone().top() > circle->height() / 2);
        QCOMPARE(circle->arrowZone().bottom(), circle->height() - 1);
        auto* trim = make<RibbonButton>(control(u"home"_s, u"TRIM"_s));
        QVERIFY(trim->arrowZone().left() > trim->width() / 2);
        QCOMPARE(trim->arrowZone().right(), trim->width() - 1);

        circle->setFocus(Qt::TabFocusReason);
        QTest::keyClick(circle, Qt::Key_Down);
        QTRY_VERIFY(circle->menu()->isVisible());
        QCOMPARE(QApplication::activePopupWidget(), circle->menu());
        QCOMPARE(circle->menu()->actions().size(), 6); // Tâm–Bán kính … Tiếp–Tiếp–Tiếp
        QTest::keyClick(circle->menu(), Qt::Key_Escape);
        QTRY_VERIFY(!circle->menu()->isVisible());
        QTest::keyClick(circle, Qt::Key_Down, Qt::AltModifier);
        QTRY_VERIFY(circle->menu()->isVisible());
        circle->menu()->close();

        QTest::mousePress(trim, Qt::LeftButton, {}, trim->arrowZone().center());
        QTRY_VERIFY(trim->menu()->isVisible());
        QCOMPARE(trim->menu()->actions().size(), 2); // Cắt / Kéo dài
        trim->menu()->actions().at(1)->trigger();    // mục không tham số: QAction của registry
        trim->menu()->close();
        QCOMPARE(take(sent_), QStringList{u"^C^C_EXTEND"_s});
        QVERIFY(take(macros_).isEmpty());
        QTest::mouseClick(trim, Qt::LeftButton, {}, QPoint(px(tok::sp_2) + 2, trim->height() / 2)); // phần chính
        QCOMPARE(take(sent_), QStringList{u"^C^C_EXTEND"_s});
        QTest::keyClick(trim, Qt::Key_Return); // Enter như bấm
        QCOMPARE(take(sent_), QStringList{u"^C^C_EXTEND"_s});
        QVERIFY(QApplication::activePopupWidget() == nullptr);
    }

    void splitRemembers()
    {
        const Control& c = control(u"home"_s, u"CIRCLE"_s);
        auto* b = make<RibbonButton>(c);
        QTest::mouseClick(b, Qt::LeftButton, {}, QPoint(b->width() / 2, 4)); // chưa chọn: lệnh của nút
        QCOMPARE(take(sent_), QStringList{u"^C^C_CIRCLE"_s});
        b->showMenu();
        QTRY_VERIFY(b->menu()->isVisible());
        b->menu()->actions().at(1)->trigger(); // Tâm, Đường kính
        b->menu()->close();
        QCOMPARE(take(macros_), QStringList{u"^C^C_CIRCLE D"_s});
        QCOMPARE(b->mainItem().key(), u"CIRCLE D"_s);
        QVERIFY(ui::RichTooltip::name(b).contains(RibbonModel::line(c.menu[1].label)));
        QTest::mouseClick(b, Qt::LeftButton, {}, QPoint(b->width() / 2, 4));
        QCOMPARE(take(macros_), QStringList{u"^C^C_CIRCLE D"_s});
        delete b;

        // Dựng lại với cùng Store: vẫn nhớ.
        auto* again = make<RibbonButton>(c);
        again->click();
        QCOMPARE(take(macros_), QStringList{u"^C^C_CIRCLE D"_s});

        // Khởi động lại: Store mới đọc từ tệp.
        store_->flush();
        settings::Store reopened(store_->filePath());
        RibbonControls fresh(*registry_, reopened);
        QStringList got;
        connect(&fresh, &RibbonControls::macroRequested, this, [&](const QString& m) { got << m; });
        auto* restarted = make<RibbonButton>(c, &fresh);
        restarted->click();
        QCOMPARE(got, QStringList{u"^C^C_CIRCLE D"_s});
        delete restarted;
        delete again;
    }

    void galleryVirtualized()
    {
        Samples samples(2000);
        const Control g{.type = ControlType::Gallery,
                        .id = u"pat"_s,
                        .label = u"Mẫu hatch"_s, // i18n-ok: thử
                        .source = u"patterns"_s,
                        .width = 296};
        controls_->setModel(u"patterns"_s, &samples);
        controls_->setCurrent(u"patterns"_s, 0);
        QWidget* gallery = make(g);
        QVERIFY(gallery);
        QCOMPARE(gallery->sizeHint(), QSize(296, 3 * px(tok::h_btn) + 2));
        auto* view = gallery->findChild<QListView*>();
        QVERIFY(view);
        QCOMPARE(view->model()->rowCount(), 2000);
        const qsizetype widgets = gallery->findChildren<QWidget*>().size();
        QVERIFY2(widgets < 20, qPrintable(QString::number(widgets))); // không có widget cho từng mục

        // Một bước cuộn = bấm "Hàng sau" rồi vẽ lại vùng nhìn, đo CPU luồng.
        const QList<ui::Button*> nav = gallery->findChildren<ui::Button*>();
        QCOMPARE(nav.size(), 3);
        QScrollBar* bar = view->verticalScrollBar();
        std::vector<double> steps;
        for (int i = 0; i < 100; ++i) {
            const int before = bar->value();
            const auto t0 = cpu();
            nav[1]->click();
            view->viewport()->repaint();
            steps.push_back(ms(cpu() - t0));
            QVERIFY(bar->value() > before);
        }
        std::ranges::sort(steps);
        const double p99 = steps[98];
        qInfo("gallery 2.000 mục, %lld widget; bước cuộn p50 %.3f ms, p99 %.3f ms CPU luồng", qlonglong(widgets),
              steps[50], p99);
        QVERIFY2(!kTimed || p99 <= 16.6, qPrintable(QString::number(p99)));
        nav[0]->click();
        QVERIFY(bar->value() < bar->maximum());

        // Bàn phím: mũi tên đổi mục, Enter chọn; nút "toàn bộ gallery".
        QSignalSpy picked(controls_.get(), &RibbonControls::picked);
        QSignalSpy more(controls_.get(), &RibbonControls::moreRequested);
        view->setFocus(Qt::TabFocusReason);
        view->setCurrentIndex(view->model()->index(0, 0));
        QTest::keyClick(view, Qt::Key_Right);
        QTest::keyClick(view, Qt::Key_Return);
        QCOMPARE(picked.size(), 1);
        QCOMPARE(picked[0], (QVariantList{u"patterns"_s, 1}));
        nav[2]->click();
        QCOMPARE(more.size(), 1);
        delete gallery;
        controls_->setModel(u"patterns"_s, nullptr);
    }

    void toggleFollowsSysvar()
    {
        auto* lw = make<RibbonButton>(control(u"view"_s, u"lwtgl"_s));
        auto* pick = make<RibbonButton>(control(u"home"_s, u"PICKSTYLE"_s));
        QVERIFY(lw->isCheckable() && pick->isCheckable());
        QVERIFY(!lw->isChecked());
        sysvars_->set(u"LWDISPLAY"_s, 1); // từ dòng lệnh, LISP: cùng lượt gọi, không chờ vòng sự kiện
        QVERIFY(lw->isChecked());
        sysvars_->set(u"PICKSTYLE"_s, 3);
        QVERIFY(pick->isChecked());
        sysvars_->set(u"LWDISPLAY"_s, 0);
        QVERIFY(!lw->isChecked());

        lw->click(); // chỉ gửi lệnh; nút đổi khi sysvar đổi
        QCOMPARE(take(sent_), QStringList{u"'LWDISPLAY 1"_s});
        QVERIFY(!lw->isChecked());
        QTest::keyClick(lw, Qt::Key_Space);
        QCOMPARE(take(sent_), QStringList{u"'LWDISPLAY 1"_s});
        sysvars_->set(u"PICKSTYLE"_s, 0);
    }

    void buttonStates()
    {
        // Mỗi loại nút ở Tối và Sáng: nền theo token của trạng thái (điểm ảnh ở lề, xa chữ và biểu tượng).
        for (const theme::Theme th : {theme::Theme::Dark, theme::Theme::Light}) {
            tm_->setTheme(th);
            QCoreApplication::processEvents();
            for (const QString& id :
                 {u"LINE"_s, u"MOVE"_s, u"LAYMCUR"_s, u"CIRCLE"_s, u"TRIM"_s}) { // lớn, nhỏ, biểu tượng, split
                const Control& c = control(u"home"_s, id);
                auto* b = make<RibbonButton>(c);
                const QPoint edge(2, b->height() / 2);
                b->setAttribute(Qt::WA_UnderMouse, false);
                const QImage normal = shot(b);
                b->setAttribute(Qt::WA_UnderMouse, true);
                QCOMPARE(normal.pixelColor(edge), QColor(Qt::transparent)); // mặc định: không nền
                QVERIFY(expectToken(b, edge, {tok::c_hover}));
                b->setDown(true);
                QVERIFY(expectToken(b, edge, {tok::c_hover, tok::c_press}));
                b->setDown(false);
                b->setAttribute(Qt::WA_UnderMouse, false);
                QCOMPARE(shot(b), normal);
                b->setFocus(Qt::TabFocusReason);
                QTRY_VERIFY(b->hasFocus());
                window_->setAttribute(Qt::WA_KeyboardFocusChange); // như khi focus đến bằng phím Tab
                QVERIFY(expectToken(b, QPoint(1, b->height() / 2), {tok::c_focus}));
                b->clearFocus();

                // Vô hiệu theo QAction của lệnh.
                registry_->action(c.cmd)->setEnabled(false);
                QVERIFY(!b->isEnabled());
                QVERIFY(shot(b) != normal);
                registry_->action(c.cmd)->setEnabled(true);
                QVERIFY(b->isEnabled());

                // Khóa theo phiên bản: huy hiệu .lkb (nền --c-ed-pro), mô tả cho trình đọc màn hình.
                gate_->setEdition(edition::Edition::Viewer);
                QVERIFY2(b->isLocked(), qPrintable(id));
                b->resize(b->sizeHint());
                const QImage locked = shot(b);
                const QSize badge = edition::LockBadge::size(ui::Edition::Pro, edition::LockBadge::Style::Ribbon);
                const QPoint inBadge =
                    c.size == ControlSize::Large
                        ? QPoint(b->width() - badge.width() + 1, 1 + badge.height() / 2)
                        : QPoint(b->width() - px(c.size == ControlSize::Icon ? tok::sp_2 : tok::sp_3) - badge.width() +
                                     1,
                                 b->height() / 2);
                QVERIFY(locked != normal);
                QVERIFY(expectToken(b, inBadge, {tok::c_ed_pro}));
                QCOMPARE(b->accessibleDescription(), edition::EditionGate::availableIn(ui::Edition::Pro));
                gate_->setEdition(edition::Edition::Pro);
                QVERIFY(!b->isLocked());
                delete b;
            }
            // Bật (toggle): --c-on-bg.
            auto* lw = make<RibbonButton>(control(u"view"_s, u"lwtgl"_s));
            sysvars_->set(u"LWDISPLAY"_s, 1);
            QVERIFY(expectToken(lw, QPoint(2, lw->height() / 2), {tok::c_on_bg}));
            sysvars_->set(u"LWDISPLAY"_s, 0);
            delete lw;
        }
        tm_->setTheme(theme::Theme::Dark);
        QCoreApplication::processEvents();
    }

    void dataStates()
    {
        // Dropdown
        auto* dd = make<ui::DropDown>(control(u"annotate"_s, u"rb-ts"_s));
        QVERIFY(dd);
        const QString name = RibbonModel::line(u"Kiểu chữ"_s); // i18n-ok: nhãn của prototype
        QCOMPARE(dd->placeholderText(), name);                 // nguồn chưa nối: tên dropdown
        QCOMPARE(dd->accessibleName(), name);
        QStringListModel later; // model nền: nối trước, dòng đến sau
        controls_->setModel(u"textstyles"_s, &later);
        QCOMPARE(dd->placeholderText(), RibbonControls::tr("Không có mục")); // rỗng
        later.setStringList({u"Standard"_s});
        QCOMPARE(dd->placeholderText(), name);
        controls_->setDataState(u"textstyles"_s, DataState::Loading);
        QCOMPARE(dd->placeholderText(), RibbonControls::tr("Đang tải…"));
        dd->showPopup();
        QVERIFY(QApplication::activePopupWidget() == nullptr);
        controls_->setDataState(u"textstyles"_s, DataState::Error);
        QVERIFY(dd->hasError());
        QCOMPARE(pixel(dd, QPoint(1, dd->height() / 2)), token(tok::c_danger));
        QCOMPARE(dd->accessibleDescription(), RibbonControls::tr("Không tải được"));
        Samples styles(3);
        controls_->setModel(u"textstyles"_s, &styles);
        controls_->setDataState(u"textstyles"_s, DataState::Ready);
        QVERIFY(!dd->hasError());
        QCOMPARE(dd->currentIndex(), -1);
        QCOMPARE(dd->placeholderText(), name);
        controls_->setCurrent(u"textstyles"_s, 1);
        QCOMPARE(dd->currentText(), u"ANSI1"_s);
        QSignalSpy picked(controls_.get(), &RibbonControls::picked);
        dd->setFocus(Qt::TabFocusReason);
        QTest::keyClick(dd, Qt::Key_Down);
        QCOMPARE(picked.size(), 1);
        QCOMPARE(picked[0], (QVariantList{u"textstyles"_s, 2}));
        QCOMPARE(dd->height(), px(tok::h_btn));

        // Gallery: chữ thay lưới khi rỗng, đang tải, lỗi; ô đang chọn viền --c-accent.
        const Control g{.type = ControlType::Gallery,
                        .id = u"pat"_s,
                        .label = u"Mẫu hatch"_s, // i18n-ok: thử
                        .source = u"patterns"_s,
                        .width = 296};
        QWidget* gallery = make(g);
        auto* view = gallery->findChild<QListView*>();
        QCOMPARE(view->accessibleDescription(), RibbonControls::tr("Không có mục"));
        const QImage empty = shot(gallery);
        controls_->setDataState(u"patterns"_s, DataState::Loading);
        QCOMPARE(view->accessibleDescription(), RibbonControls::tr("Đang tải…"));
        QVERIFY(shot(gallery) != empty);
        controls_->setDataState(u"patterns"_s, DataState::Error);
        QCOMPARE(view->accessibleDescription(), RibbonControls::tr("Không tải được"));
        Samples samples(40);
        controls_->setModel(u"patterns"_s, &samples);
        controls_->setDataState(u"patterns"_s, DataState::Ready);
        controls_->setCurrent(u"patterns"_s, 0);
        QVERIFY(view->accessibleDescription().isEmpty());
        const QRect cell = view->visualRect(samples.index(0, 0)).translated(view->mapTo(gallery, QPoint()));
        QCOMPARE(pixel(gallery, QPoint(cell.left() + 1, cell.center().y())), token(tok::c_accent));

        // Split theo source (Chèn block): danh sách theo model, chọn → picked.
        auto* insert = make<RibbonButton>(control(u"insert"_s, u"INSERT"_s));
        insert->showMenu();
        QTRY_VERIFY(insert->menu()->isVisible());
        QCOMPARE(insert->menu()->actions().size(), 1);
        QVERIFY(!insert->menu()->actions().at(0)->isEnabled());
        QCOMPARE(insert->menu()->actions().at(0)->text(), RibbonControls::tr("Không có mục"));
        insert->menu()->close();
        Samples blocks(5);
        controls_->setModel(u"blocks"_s, &blocks);
        insert->showMenu();
        QTRY_VERIFY(insert->menu()->isVisible());
        QCOMPARE(insert->menu()->actions().size(), 5);
        picked.clear();
        insert->menu()->actions().at(2)->trigger();
        insert->menu()->close();
        QCOMPARE(picked[0], (QVariantList{u"blocks"_s, 2}));
        insert->click(); // phần chính: lệnh của nút
        QCOMPARE(take(sent_), QStringList{u"^C^C_INSERT"_s});

        delete insert;
        delete gallery;
        delete dd;
        for (const QString& s : {u"textstyles"_s, u"patterns"_s, u"blocks"_s}) {
            controls_->setModel(s, nullptr);
            controls_->setCurrent(s, -1);
        }
    }

    void lockedControls()
    {
        auto* move = make<RibbonButton>(control(u"home"_s, u"MOVE"_s));
        auto* circle = make<RibbonButton>(control(u"home"_s, u"CIRCLE"_s));
        auto* dd = make<ui::DropDown>(control(u"annotate"_s, u"rb-ts"_s)); // lệnh STYLE: Pro
        Samples styles(3);
        controls_->setModel(u"textstyles"_s, &styles);
        gate_->setEdition(edition::Edition::Viewer);
        QVERIFY(move->isLocked() && circle->isLocked() && dd->isLocked());
        QCOMPARE(ui::RichTooltip::info(move).need, ui::Edition::Pro);

        move->click();
        circle->showMenu(); // mũi tên của nút khóa: trang nâng cấp, không mở danh sách
        QVERIFY(!circle->menu()->isVisible());
        dd->showPopup();
        QVERIFY(QApplication::activePopupWidget() == nullptr);
        QVERIFY(take(sent_).isEmpty());
        QCOMPARE(take(upgrades_), (QStringList{u"MOVE"_s, u"CIRCLE"_s, u"STYLE"_s}));

        // Mục có tham số đã nhớ: phần chính qua cổng phiên bản, không phát macro.
        gate_->setEdition(edition::Edition::Pro);
        circle->showMenu();
        QTRY_VERIFY(circle->menu()->isVisible());
        circle->menu()->actions().at(2)->trigger(); // 2 điểm
        circle->menu()->close();
        QCOMPARE(take(macros_), QStringList{u"^C^C_CIRCLE 2P"_s});
        gate_->setEdition(edition::Edition::Viewer);
        circle->click();
        QVERIFY(take(macros_).isEmpty());
        QCOMPARE(take(upgrades_), QStringList{u"CIRCLE"_s});

        // Ô số, thanh trượt có cờ riêng: khóa, nhận giá trị mở trang nâng cấp.
        const Control num{.type = ControlType::Number,
                          .id = u"h"_s,
                          .label = u"Chiều cao"_s, // i18n-ok: thử
                          .need = u"PRO"_s,
                          .sysvar = u"TEXTSIZE"_s};
        QWidget* n = make(num);
        QVERIFY(n->findChild<ui::NumericField*>()->isLocked());
        const Control sl{.type = ControlType::Slider,
                         .id = u"t"_s,
                         .label = u"Trong suốt"_s, // i18n-ok: thử
                         .need = u"PRO"_s,
                         .sysvar = u"CETRANSPARENCY"_s,
                         .min = 0,
                         .max = 90};
        QWidget* s = make(sl);
        QVERIFY(s->findChild<ui::Slider*>()->isLocked());
        controls_->commit(sl, 10);
        QVERIFY(take(macros_).isEmpty());
        QCOMPARE(take(upgrades_), QStringList{u"t"_s});
        gate_->setEdition(edition::Edition::Pro);
        QVERIFY(!s->findChild<ui::Slider*>()->isLocked());
        QVERIFY(!move->isLocked() && !dd->isLocked());
        controls_->setModel(u"textstyles"_s, nullptr);
        delete dd;
    }

    void valueControls()
    {
        QWidget* n = make(control(u"annotate"_s, u"rb-th"_s)); // Chiều cao chữ: TEXTSIZE
        auto* field = n->findChild<ui::NumericField*>();
        QVERIFY(field && !field->isPickEnabled());
        controls_->setValue(u"TEXTSIZE"_s, 3.5); // từ sysvar: hiện, không gửi lại
        QCOMPARE(field->value(), 3.5);
        QVERIFY(take(macros_).isEmpty());
        auto* edit = field->findChild<QLineEdit*>();
        edit->setFocus();
        edit->selectAll();
        QTest::keyClicks(edit, u"abc"_s);
        QTest::keyClick(edit, Qt::Key_Return);
        QVERIFY(field->hasError()); // lỗi: viền, câu dưới ô (UI-C09), không gửi
        QVERIFY(take(macros_).isEmpty());
        edit->selectAll();
        QTest::keyClicks(edit, u"2.5"_s);
        QTest::keyClick(edit, Qt::Key_Return);
        QVERIFY(!field->hasError());
        QCOMPARE(take(macros_), QStringList{u"'TEXTSIZE 2.5"_s});

        QWidget* s = make(control(u"home"_s, u"rb-transp"_s)); // độ trong suốt: thanh trượt 0–90
        auto* slider = s->findChild<ui::Slider*>();
        QVERIFY(slider);
        QCOMPARE(slider->maximum(), 90);
        controls_->setValue(u"CETRANSPARENCY"_s, 40);
        QCOMPARE(slider->value(), 40);
        QVERIFY(take(macros_).isEmpty());
        slider->setFocus(Qt::TabFocusReason);
        QTest::keyClick(slider, Qt::Key_Right);
        QCOMPARE(take(macros_), QStringList{u"'CETRANSPARENCY 41"_s});
        QVERIFY(!slider->accessibleName().isEmpty());
    }

    // Ảnh 8 tab ở Tối và Sáng để xem so với prototype (QA-011, DoD-U 1): TRION_RIBBON_SHOTS=<thư mục>.
    void shots()
    {
        const QString out = qEnvironmentVariable("TRION_RIBBON_SHOTS");
        if (out.isEmpty())
            QSKIP("đặt TRION_RIBBON_SHOTS=<thư mục> để ghi ảnh");
        for (const theme::Theme th : {theme::Theme::Dark, theme::Theme::Light}) {
            tm_->setTheme(th);
            for (const Tab& t : model_->tabs()) {
                QWidget* w = page(t, *controls_, window_.get());
                w->setAutoFillBackground(true);
                w->adjustSize();
                w->show();
                QCoreApplication::processEvents();
                QVERIFY(w->grab().save(
                    u"%1/%2-%3.png"_s.arg(out, t.id, th == theme::Theme::Dark ? u"dark"_s : u"light"_s)));
                delete w;
            }
        }
        tm_->setTheme(theme::Theme::Dark);
    }

    void pseudoNoTruncation()
    {
        // Tab Trang chủ dựng ở "vi" rồi đổi sang vi-XL (dịch lại tại chỗ); 7 tab còn lại dựng ở vi-XL.
        QWidget* home = page(model_->tabs()[0], *controls_, window_.get());
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        const QString longer = RibbonModel::line(u"Đoạn\nthẳng"_s); // i18n-ok: nhãn của prototype
        QVERIFY(spinUntil([&] { return home->findChild<RibbonButton*>(u"LINE"_s)->text() == longer; }));
        int checked = 0;
        for (size_t i = 0; i < model_->tabs().size(); ++i) {
            QWidget* w = i == 0 ? home : page(model_->tabs()[i], *controls_, window_.get());
            w->adjustSize();
            w->show();
            QCoreApplication::processEvents();
            const QList<i18n::Truncation> cut = i18n::findTruncations(w);
            for (const i18n::Truncation& t : cut)
                qWarning("%s: %s", qUtf8Printable(model_->tabs()[i].id), qUtf8Printable(t.toString()));
            QVERIFY(cut.isEmpty());
            // Tên cho trình đọc màn hình; rảnh không timer chạy.
            for (RibbonButton* b : w->findChildren<RibbonButton*>())
                QVERIFY2(!b->text().isEmpty(), qPrintable(b->objectName()));
            for (QComboBox* c : w->findChildren<QComboBox*>())
                QVERIFY2(!c->accessibleName().isEmpty(), qPrintable(c->objectName()));
            for (ui::Slider* s : w->findChildren<ui::Slider*>())
                QVERIFY(!s->accessibleName().isEmpty());
            for (ui::NumericField* f : w->findChildren<ui::NumericField*>())
                QVERIFY(!f->label().isEmpty());
            for (const QTimer* t : w->findChildren<QTimer*>())
                QVERIFY2(!t->isActive(), qPrintable(t->objectName()));
            checked += int(w->findChildren<QWidget*>().size());
            delete w;
        }
        qInfo("vi-XL: 8 tab, %d widget, 0 chữ bị cắt", checked);
        QVERIFY(translator_->setLanguage(u"vi"_s));
        QCoreApplication::processEvents();
    }
};

QTEST_MAIN(RibbonTest)
#include "ribbon_test.moc"
