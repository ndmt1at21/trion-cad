// UIF-001: Tìm chữ (FindDialog.h, FindMatcher.h). Lõi chưa có (CMD:FIND, DOC-009, INT-016, RND-013, VN-001): nguồn chữ
// là bản vẽ mẫu VN tự dựng (sampleTexts), tín hiệu chỗ nối được ghi lại. Tiêu chí của task:
//   1  matcher (45 ca, đáp án viết tay: có/không dấu, hoa thường, nguyên từ, wildcard, TCVN3/VNI qua chữ giải mã của
//      nguồn, trong block, xref, bảng, chữ kích thước, đối tượng ẩn, phạm vi), spans; tắt "Phân biệt dấu" thì "duong"
//      khớp "đường". Giải mã TCVN3/VNI thật: Hoãn (VN-001) — nguồn mẫu cấp sẵn chữ giải mã.
//   2  firstResults, cancelLatency, staleDropped: ≥ 200.000 chữ (release): kết quả đầu ≤ 200 ms, đổ theo lô, số kết
//      quả cập nhật; đổi chuỗi thì lượt cũ hủy ≤ 50 ms và kết quả cũ không lẫn vào.
//   3  navigation: bấm dòng, ↑↓, Enter → objectRequested(id, layout) (zoom INT-016, tô sáng RND-013: tới chỗ nối).
//   4  selectionSet: Tạo tập chọn từ mọi dòng hoặc các dòng đang chọn, id không trùng.
//   5  emptyStates: chưa gõ (giải thích tùy chọn dấu); "Không tìm thấy “…”".
//   6  edition: Viewer, Viewer Pro: Thay bằng, Thay, Thay tất cả có huy hiệu Pro, bấm → trang nâng cấp; Pro: mở khóa.
//   Còn lại (DoD-U, DoD-L): scopeAndPick, optionsPersist, highlightPixels, openTime, a11y, pseudoNoTruncation, idle.
#include "app/dialogs/find/FindDialog.h"
#include "app/dialogs/find/FindMatcher.h"

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
#include "app/widgets/progress/Spinner.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"
#include "tests/timing.h"

#include <QApplication>
#include <QCheckBox>
#include <QElapsedTimer>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using trion::test::kTimed;
using trion::ui::FindDialog;
namespace edition = trion::edition;
namespace find = trion::find;
namespace i18n = trion::i18n;
namespace sched = trion::sched;
namespace settings = trion::settings;
namespace theme = trion::theme;
namespace ui = trion::ui;
using Edition = edition::Edition;
using K = find::TextKind;

namespace {

bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 20s)
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

// Bản vẽ mẫu VN: mục thứ n (đánh số từ 1) có id n·10, trừ attribute cùng một block khung tên (7, 8: id 70) và ô cùng
// một bảng (13, 14: id 130). Chữ mã cũ (17 TCVN3, 18 VNI): byte gốc minh họa, chữ giải mã do nguồn cấp (VN-001 sau
// này).
std::shared_ptr<const find::TextList> sampleTexts()
{
    auto list = std::make_shared<find::TextList>();
    const auto add = [&](K kind, const QString& layout, const QString& text, const QString& detail = {}) {
        find::TextItem t;
        t.id = (list->size() + 1) * 10;
        t.kind = kind;
        t.layout = layout;
        t.text = text;
        t.detail = detail;
        list->push_back(t);
        return &list->back();
    };
    const QString model = u"Model"_s, mb = u"A1-MB"_s, md = u"A1-MD"_s;
    add(K::Text, model, u"Đường ống cấp nước D25"_s);                         // 1
    add(K::Text, model, u"DUONG ONG THOAT NUOC"_s);                           // 2
    add(K::MText, model, u"Phòng ngủ 1\nDiện tích 12,5 m²"_s);                // 3
    add(K::MText, model, u"Tường xây gạch 220"_s);                            // 4
    add(K::Text, model, u"tường"_s);                                          // 5
    add(K::Text, mb, u"Tường rào"_s);                                         // 6
    add(K::Attribute, mb, u"MẶT BẰNG TẦNG 1"_s, u"TEN_BV"_s)->inBlock = true; // 7
    find::TextItem* t = add(K::Attribute, mb, u"KT-12"_s, u"SO_BV"_s);        // 8
    t->inBlock = true;
    t->id = 70;
    add(K::Text, model, u"CỬA ĐI D1"_s, u"CUA_DI"_s)->inBlock = true;           // 9
    add(K::Dimension, model, u"1250"_s);                                        // 10
    add(K::Dimension, model, u"Cao độ +3.600"_s);                               // 11
    add(K::Leader, model, u"Ống thoát nước mưa D90"_s);                         // 12
    add(K::Table, mb, u"Cửa đi"_s, u"B3"_s);                                    // 13
    add(K::Table, mb, u"Đường kính"_s, u"C3"_s)->id = 130;                      // 14
    add(K::Text, model, u"Hiện trạng đường"_s, u"HIEN_TRANG"_s)->inXref = true; // 15
    add(K::Text, model, u"Ghi chú ẩn: đường cũ"_s)->hidden = true;              // 16
    t = add(K::Text, model, u"§­êng giao th«ng"_s);                             // 17
    t->legacy = find::Legacy::Tcvn3;
    t->decoded = u"Đường giao thông"_s;
    t = add(K::MText, model, u"Ñöôøng ñoâi"_s); // 18
    t->legacy = find::Legacy::Vni;
    t->decoded = u"Đường đôi"_s;
    add(K::Hyperlink, model, u"Tiêu chuẩn TCVN 2737"_s);                         // 19
    add(K::Text, model, u"tuong"_s);                                             // 20
    add(K::Text, model, u"Tượng đài"_s);                                         // 21
    add(K::Text, mb, u"KT-13"_s);                                                // 22
    add(K::Text, mb, u"KT-1A"_s);                                                // 23
    add(K::Text, model, u"đường bộ"_s.normalized(QString::NormalizationForm_D)); // 24: chữ tổ hợp (NFD)
    add(K::Text, model, u"Bản vẽ số 3"_s);                                       // 25
    add(K::Text, model, u"ĐƯỜNG ĐỎ"_s);                                          // 26
    add(K::Text, md, u"Mặt đứng trục 1-5"_s);                                    // 27
    add(K::Text, model, u"bàn làm việc"_s);                                      // 28
    add(K::Attribute, model, u"ban công"_s, u"GHI_CHU"_s)->inBlock = true;       // 29
    add(K::Text, model, u"Cao độ sàn +0.000"_s);                                 // 30
    return list;
}

// Bản vẽ chữ dày (BENCH-STD text dày tự dựng, tất định): chữ 3–6 từ, loại, layout xoay vòng.
std::shared_ptr<const find::TextList> denseTexts(int n)
{
    static const char16_t* const words[] = {u"Đường", u"ống",   u"cấp",  u"nước", u"Tường", u"xây", u"gạch", u"Phòng",
                                            u"ngủ",   u"khách", u"Cửa",  u"đi",   u"sổ",    u"Cao", u"độ",   u"sàn",
                                            u"Mặt",   u"bằng",  u"tầng", u"Trục", u"thép",  u"bê",  u"tông", u"cốt",
                                            u"D25",   u"KT-12", u"1250", u"móng", u"cột",   u"dầm"};
    const QString layouts[] = {u"Model"_s, u"A1-MB"_s, u"A1-MD"_s, u"A1-MC"_s};
    std::uint32_t seed = 20261023;
    const auto next = [&seed] {
        seed = seed * 1664525u + 1013904223u;
        return seed >> 8;
    };
    auto list = std::make_shared<find::TextList>();
    list->reserve(std::size_t(n));
    for (int i = 0; i < n; ++i) {
        find::TextItem t;
        t.id = std::uint64_t(i) + 1;
        t.kind = K(next() % 7);
        t.inBlock = t.kind == K::Attribute;
        t.layout = layouts[i % 4];
        const int words_ = 3 + int(next() % 4);
        for (int w = 0; w < words_; ++w) {
            if (w)
                t.text += u' ';
            t.text += QString::fromUtf16(words[next() % std::size(words)]);
        }
        list->push_back(std::move(t));
    }
    return list;
}

// "case word wild accents xref noblock hidden nolegacy kinds=<adtbh> space=<layout> sel=<id,…>"
find::FindOptions parseOptions(const QString& spec)
{
    find::FindOptions o;
    for (const QString& t : spec.split(u' ', Qt::SkipEmptyParts)) {
        if (t == u"case"_s)
            o.matchCase = true;
        else if (t == u"word"_s)
            o.wholeWord = true;
        else if (t == u"wild"_s)
            o.wildcard = true;
        else if (t == u"accents"_s)
            o.matchAccents = true;
        else if (t == u"xref"_s)
            o.inXrefs = true;
        else if (t == u"noblock"_s)
            o.inBlocks = false;
        else if (t == u"hidden"_s)
            o.skipHidden = false;
        else if (t == u"nolegacy"_s)
            o.legacy = false;
        else if (t.startsWith(u"kinds="_s)) {
            o.kinds = 0;
            for (const QChar c : t.mid(6))
                o.kinds |= c == u'a'   ? find::kAttributes
                           : c == u'd' ? find::kDimLeader
                           : c == u't' ? find::kTexts
                           : c == u'b' ? find::kTables
                                       : find::kHyperlinks;
        } else if (t.startsWith(u"space="_s)) {
            o.scope = find::Scope::CurrentSpace;
            o.space = t.mid(6);
        } else if (t.startsWith(u"sel="_s)) {
            auto ids = std::make_shared<std::vector<std::uint64_t>>();
            for (const QString& id : t.mid(4).split(u','))
                ids->push_back(id.toULongLong());
            std::sort(ids->begin(), ids->end());
            o.scope = find::Scope::Selection;
            o.selection = ids;
        } else {
            qFatal("tùy chọn lạ: %s", qPrintable(t));
        }
    }
    return o;
}

QList<int> numbers(const std::vector<find::Hit>& hits)
{
    QList<int> out;
    for (const find::Hit& h : hits)
        out << h.item + 1;
    return out;
}

QList<quint64> rowIds(const QAbstractItemView* v)
{
    QList<quint64> out;
    for (int r = 0; r < v->model()->rowCount(); ++r)
        out << v->model()->index(r, 0).data(FindDialog::IdRole).value<quint64>();
    return out;
}

QList<quint64> ids(const find::TextList& texts, const std::vector<find::Hit>& hits)
{
    QList<quint64> out;
    for (const find::Hit& h : hits)
        out << texts[std::size_t(h.item)].id;
    return out;
}

QString emptyText(FindDialog* d)
{
    QStringList out;
    for (const QLabel* l : d->findChild<ui::EmptyState*>(u"empty"_s)->findChildren<QLabel*>())
        out << l->text();
    return out.join(u'\n');
}

// Màu src (có alpha) phủ lên nền bg, như QPainter SourceOver.
QColor over(const QColor& bg, const QColor& src)
{
    const double a = src.alphaF();
    return QColor::fromRgbF(float(src.redF() * a + bg.redF() * (1 - a)),
                            float(src.greenF() * a + bg.greenF() * (1 - a)),
                            float(src.blueF() * a + bg.blueF() * (1 - a)));
}

int countColor(const QImage& img, const QRect& r, const QColor& c)
{
    int n = 0;
    for (int y = r.top(); y <= r.bottom(); ++y)
        for (int x = r.left(); x <= r.right(); ++x) {
            const QColor p = img.pixelColor(x, y);
            if (std::abs(p.red() - c.red()) <= 2 && std::abs(p.green() - c.green()) <= 2 &&
                std::abs(p.blue() - c.blue()) <= 2)
                ++n;
        }
    return n;
}

} // namespace

class FindDialogTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<theme::ThemeManager> tm_;
    std::unique_ptr<theme::IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<edition::EditionGate> gate_;
    std::unique_ptr<QWidget> host_;
    std::shared_ptr<const find::TextList> sample_;
    std::shared_ptr<const find::TextList> dense_;
    QPointer<FindDialog> dlg_;
    QStringList upgrades_;

    FindDialog* make()
    {
        delete dlg_;
        dlg_ = new FindDialog(*gate_, host_.get());
        dlg_->setTexts(sample_);
        dlg_->setCurrentSpace(u"Model"_s);
        return dlg_;
    }

    static bool show(FindDialog* d)
    {
        d->show();
        d->activateWindow();
        return QTest::qWaitForWindowExposed(d) && spinUntil([d] { return QApplication::activeWindow() == d; });
    }

    // Gõ chuỗi tìm (như dán: một lần đổi chữ) rồi chờ lượt tìm xong.
    static void type(FindDialog* d, const QString& text)
    {
        d->queryField()->setText(text);
        QVERIFY(spinUntil([d] { return !d->isSearching(); }));
    }

    static QCheckBox* box(FindDialog* d, const char* name)
    {
        return d->findChild<QCheckBox*>(QString::fromLatin1(name));
    }

    static ui::Button* button(FindDialog* d, const char* name)
    {
        return d->findChild<ui::Button*>(QString::fromLatin1(name));
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
        QApplication::setCursorFlashTime(0);
        host_ = std::make_unique<QWidget>();
        host_->setAccessibleName(u"Cửa sổ chính"_s); // i18n-ok: cửa sổ giả của test
        host_->resize(1366, 768);
        host_->move(0, 0);
        host_->show();
        QVERIFY(QTest::qWaitForWindowExposed(host_.get()));
        sample_ = sampleTexts();
        // Release: 200.000 chữ như tiêu chí; debug 50.000, sanitizer 20.000 (chậm 5–20 lần): chỉ báo số đo.
        dense_ = denseTexts(kTimed ? 200'000 : trion::test::kSanitized ? 20'000 : 50'000);
    }

    void init()
    {
        tm_->setTheme(theme::Theme::Dark);
        tm_->setDensity(theme::Density::Compact);
        gate_->setEdition(Edition::Pro);
        for (const QString& k : store_->keys())
            if (k.startsWith(u"find/"_s))
                store_->remove(k);
        upgrades_.clear();
    }

    void cleanup()
    {
        delete dlg_;
        if (translator_->language() != u"vi"_s)
            QVERIFY(translator_->setLanguage(u"vi"_s));
        sched::waitIdle();
        QCoreApplication::processEvents();
    }

    void cleanupTestCase()
    {
        delete dlg_;
        sched::waitIdle();
        QCoreApplication::processEvents();
        host_.reset();
        gate_.reset();
        translator_.reset();
        icons_.reset();
        tm_.reset();
        executor_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
    }

    // Tiêu chí 1: bộ ca trên bản vẽ mẫu VN (số thứ tự mục của sampleTexts). Mặc định như hộp thoại: không phân biệt
    // hoa thường và dấu, tìm trong block, bỏ đối tượng ẩn, không tìm trong xref, tìm cả chữ mã cũ, mọi loại chữ.
    void matcher_data()
    {
        QTest::addColumn<QString>("needle");
        QTest::addColumn<QString>("spec");
        QTest::addColumn<QList<int>>("expected");
        // i18n-ok: chuỗi tìm và đáp án là dữ liệu của test
        const auto row = [](const char* name, const QString& needle, const char* spec, QList<int> expected) {
            QTest::newRow(name) << needle << QString::fromLatin1(spec) << expected;
        };
        row("01 không dấu khớp có dấu", u"duong"_s, "", {1, 2, 14, 17, 18, 24, 26});
        row("02 có dấu, tắt phân biệt dấu", u"đường"_s, "", {1, 2, 14, 17, 18, 24, 26});
        row("03 phân biệt dấu (NFC, NFD, hoa)", u"đường"_s, "accents", {1, 14, 17, 18, 24, 26});
        row("04 phân biệt dấu, gõ không dấu", u"duong"_s, "accents", {2});
        row("05 hoa thường, bỏ dấu", u"DUONG"_s, "case", {2, 26});
        row("06 hoa thường và dấu", u"Đường"_s, "case accents", {1, 14, 17, 18});
        row("07 tuong", u"tuong"_s, "", {4, 5, 6, 20, 21});
        row("08 tường có dấu", u"tường"_s, "accents", {4, 5, 6});
        row("09 nguyên từ, dấu", u"tuong"_s, "word accents", {20});
        row("10 nguyên từ", u"ban"_s, "word", {25, 28, 29});
        row("11 giữa từ", u"ban"_s, "", {7, 25, 28, 29});
        row("12 bản có dấu", u"bản"_s, "accents", {25});
        row("13 không dừng giữa chữ", u"ba"_s, "accents", {29});
        row("14 wildcard ?", u"KT-1?"_s, "wild", {8, 22, 23});
        row("15 wildcard * qua dòng MTEXT", u"D*5"_s, "wild", {1, 3, 27});
        row("16 tắt wildcard: * là chữ", u"D*5"_s, "", {});
        row("17 nguyên từ số", u"12"_s, "word", {3, 8});
        row("18 số giữa từ", u"12"_s, "", {3, 8, 10});
        row("19 chữ kích thước", u"1250"_s, "kinds=d", {10});
        row("20 bỏ loại kích thước", u"1250"_s, "kinds=atbh", {});
        row("21 dim và text", u"cao do"_s, "", {11, 30});
        row("22 chỉ text", u"cao do"_s, "kinds=t", {30});
        row("23 trong block và bảng", u"cua di"_s, "", {9, 13});
        row("24 không tìm trong block", u"cua di"_s, "noblock", {13});
        row("25 chỉ chữ trong bảng", u"cua di"_s, "kinds=b", {13});
        row("26 attribute khi tắt block", u"mat bang"_s, "noblock", {});
        row("27 attribute", u"mat bang"_s, "", {7});
        row("28 xref tắt", u"hien trang"_s, "", {});
        row("29 xref bật", u"hien trang"_s, "xref", {15});
        row("30 bỏ qua ẩn", u"ghi chu"_s, "", {});
        row("31 cả đối tượng ẩn", u"ghi chu"_s, "hidden", {16});
        row("32 TCVN3 qua chữ giải mã", u"giao thong"_s, "", {17});
        row("33 tắt chữ mã cũ", u"giao thong"_s, "nolegacy", {});
        row("34 chữ mã cũ nguyên byte", u"th«ng"_s, "nolegacy", {17});
        row("35 VNI qua chữ giải mã", u"duong doi"_s, "", {18});
        row("36 hyperlink", u"tcvn"_s, "kinds=h", {19});
        row("37 không gian hiện tại", u"duong"_s, "space=A1-MB", {14});
        row("38 vùng chọn (xref vẫn bị lọc)", u"duong"_s, "sel=10,150,240", {1, 24});
        row("39 NFC gõ, NFD trong bản vẽ", u"đường bộ"_s, "accents", {24});
        row("40 hoa có dấu, nguyên từ", u"ĐƯỜNG ĐỎ"_s, "word", {26});
        row("41 hoa có dấu giữa từ", u"ĐƯỜNG ĐỎ"_s, "", {18, 26});
        row("42 ký tự đặc biệt + wildcard", u"+?.600"_s, "wild", {11});
        row("43 wildcard đầu chuỗi", u"*.000"_s, "wild", {30});
        row("44 ký hiệu m²", u"m²"_s, "", {3});
        row("45 đường dẫn (multileader)", u"mưa"_s, "kinds=d", {12});
    }

    void matcher()
    {
        QFETCH(QString, needle);
        QFETCH(QString, spec);
        QFETCH(QList<int>, expected);
        const std::vector<find::Hit> hits = find::findAll(*sample_, needle, parseOptions(spec));
        QCOMPARE(numbers(hits), expected);
        // Cùng kết quả khi chia đoạn (như FindDialog): mỗi đoạn một matcher.
        const find::FindMatcher m(needle, parseOptions(spec));
        std::vector<find::Hit> sliced;
        for (int first = 0; first < int(sample_->size()); first += 7)
            m.scan(*sample_, first, std::min(first + 7, int(sample_->size())), sliced);
        QVERIFY(sliced == hits);
    }

    // Đoạn khớp theo vị trí trong chữ gốc (để tô): chữ dựng sẵn, chữ tổ hợp, dấu đứng sau, wildcard tham lam.
    void spans()
    {
        const auto span = [](const QString& needle, const QString& text, const char* spec = "") {
            return find::FindMatcher(needle, parseOptions(QString::fromLatin1(spec))).match(text);
        };
        QCOMPARE(span(u"duong"_s, u"Đường ống"_s), (find::Span{0, 5}));
        QCOMPARE(span(u"ong"_s, u"Đường ống"_s), (find::Span{2, 3})); // "ờng" trong "Đường": lần khớp đầu
        QCOMPARE(span(u"ong"_s, u"Đường ống"_s, "word"), (find::Span{6, 3}));
        const QString nfd = u"đường bộ"_s.normalized(QString::NormalizationForm_D);
        QCOMPARE(nfd.size(), 13);
        QCOMPARE(span(u"duong"_s, nfd), (find::Span{0, 8}));
        QCOMPARE(span(u"duo"_s, nfd), (find::Span{0, 6})); // phủ cả hai dấu của "ờ"
        QCOMPARE(span(u"bộ"_s, nfd, "accents"), (find::Span{9, 4}));
        QCOMPARE(span(u"ệ"_s, u"Nghệ"_s, "accents"), (find::Span{3, 1}));
        // "ê" + dấu nặng rời (bộ gõ tổ hợp) cùng nghĩa với "ệ" dựng sẵn: thứ tự chuẩn của dấu
        QCOMPARE(span(u"ệ"_s, u"Nghệ"_s, "accents"), (find::Span{3, 2}));
        QCOMPARE(span(u"a*o"_s, u"Cao độ sàn +0.000"_s, "wild"), (find::Span{1, 5})); // tham lam: tới "o" cuối ("ộ")
        QCOMPARE(span(u"?"_s, u"ộ"_s, "wild accents"), (find::Span{0, 1}));
        QVERIFY(!span(u"ba"_s, u"bả"_s, "accents"));
        QVERIFY(!span(u"ă"_s, u"ắ"_s, "accents"));
        QVERIFY(span(u"ă"_s, u"ắ"_s)); // tắt phân biệt dấu: cùng chữ a
        QVERIFY(find::FindMatcher(QString(), {}).isEmpty());
        QVERIFY(!find::FindMatcher(u" "_s, {}).isEmpty());
        QCOMPARE(span(u" "_s, u"a b"_s), (find::Span{1, 1}));
        // Ngoài BMP (cặp surrogate) giữ nguyên, đoạn khớp không cắt đôi cặp
        QCOMPARE(span(u"😀b"_s, u"a😀b"_s), (find::Span{1, 3}));
        QCOMPARE(span(u"?"_s, u"😀"_s, "wild"), (find::Span{0, 2}));
    }

    // Tiêu chí 2: kết quả đầu ≤ 200 ms, phần còn lại đổ theo lô (nhiều lần chèn dòng), số kết quả cập nhật; đúng thứ
    // tự và đủ như tìm một luồng.
    void firstResults()
    {
        FindDialog* d = make();
        d->setTexts(dense_);
        QVERIFY(show(d));
        const QString needle = u"duong"_s;
        const std::vector<find::Hit> reference = find::findAll(*dense_, needle, d->options());
        QVERIFY(reference.size() > dense_->size() / 20); // ~14 % chữ có "đường"
        QAbstractItemModel* m = d->results()->model();
        double bestFirst = 1e9, bestAll = 1e9;
        int inserts = 0;
        QList<int> counts; // số dòng sau mỗi lần chèn
        for (int round = 0; round < 3; ++round) {
            d->queryField()->clear();
            QCoreApplication::processEvents();
            QElapsedTimer clock;
            double first = -1;
            inserts = 0;
            counts.clear();
            const auto c = connect(m, &QAbstractItemModel::rowsInserted, this, [&] {
                if (first < 0)
                    first = clock.nsecsElapsed() / 1e6;
                ++inserts;
                counts << m->rowCount();
            });
            clock.start();
            d->queryField()->setText(needle);
            QVERIFY(spinUntil([d] { return !d->isSearching(); }));
            const double all = clock.nsecsElapsed() / 1e6;
            disconnect(c);
            bestFirst = std::min(bestFirst, first);
            bestAll = std::min(bestAll, all);
        }
        QCOMPARE(rowIds(d->results()), ids(*dense_, reference));
        QCOMPARE(d->resultCount(), int(reference.size()));
        QVERIFY2(inserts >= 2 && counts.first() < counts.last(), "kết quả phải vào theo lô");
        QCOMPARE(d->findChild<QLabel*>(u"count"_s)->text(),
                 u"%1 kết quả"_s.arg(i18n::formatCount(qint64(reference.size()))));
        // Việc của đoạn đầu (CPU luồng này, không phụ thuộc tải) và cả nguồn một luồng: chỉ để báo.
        const auto c0 = trion::test::threadCpu();
        std::vector<find::Hit> hits;
        find::FindMatcher(needle, d->options()).scan(*dense_, 0, FindDialog::kFirstSlice, hits);
        const auto c1 = trion::test::threadCpu();
        find::findAll(*dense_, needle, d->options());
        const auto c2 = trion::test::threadCpu();
        qInfo("%zu chữ, %zu kết quả: kết quả đầu %.1f ms, xong %.1f ms (tốt nhất của 3, %d lần chèn); đoạn đầu %.2f ms "
              "CPU, cả nguồn một luồng %.1f ms CPU",
              dense_->size(), reference.size(), bestFirst, bestAll, inserts, ms(c1 - c0), ms(c2 - c1));
        QVERIFY2(!kTimed || bestFirst <= 200, qPrintable(u"kết quả đầu %1 ms"_s.arg(bestFirst)));
        QVERIFY2(!kTimed || c0.count() < 0 || ms(c1 - c0) <= 200, "đoạn đầu quá 200 ms CPU");
    }

    // Tiêu chí 2: lượt cũ dừng ≤ 50 ms sau khi hủy (việc xét hủy mỗi kCheckEvery mục).
    void cancelLatency()
    {
        find::FindOptions o;
        o.wildcard = true;
        double best = 1e9;
        for (int round = 0; round < 3; ++round) {
            sched::CancelSource cancel;
            std::atomic<std::int64_t> ended{0};
            std::atomic<bool> started{false};
            const auto texts = dense_;
            sched::submit(sched::Priority::Normal, [&ended, &started, texts, o, token = cancel.token()] {
                const find::FindMatcher m(u"d*g"_s, o);
                std::vector<find::Hit> hits;
                started = true;
                for (int pass = 0; pass < 50 && !token.cancelled(); ++pass) {
                    hits.clear();
                    m.scan(*texts, 0, int(texts->size()), hits, token);
                }
                ended = std::chrono::steady_clock::now().time_since_epoch().count();
            });
            while (!started)
                std::this_thread::yield();
            std::this_thread::sleep_for(20ms);
            const auto t0 = std::chrono::steady_clock::now().time_since_epoch().count();
            cancel.cancel();
            QElapsedTimer guard;
            guard.start();
            while (!ended && guard.elapsed() < 10'000)
                std::this_thread::yield();
            sched::waitIdle(); // việc nền xong trước khi biến cục bộ hết hạn
            QVERIFY(ended);
            best = std::min(best, ms(std::chrono::nanoseconds(ended - t0)));
        }
        // CPU giữa hai lần xét hủy: kCheckEvery mục (không phụ thuộc tải)
        const find::FindMatcher m(u"d*g"_s, o);
        std::vector<find::Hit> hits;
        const auto c0 = trion::test::threadCpu();
        m.scan(*dense_, 0, find::FindMatcher::kCheckEvery, hits);
        const auto c1 = trion::test::threadCpu();
        qInfo("hủy: dừng sau %.2f ms (tốt nhất của 3); %d mục giữa hai lần xét hủy: %.3f ms CPU", best,
              find::FindMatcher::kCheckEvery, ms(c1 - c0));
        QVERIFY2(!kTimed || best <= 50, qPrintable(u"hủy sau %1 ms"_s.arg(best)));
        QVERIFY2(c0.count() < 0 || ms(c1 - c0) <= 50, "một lát giữa hai lần xét hủy quá 50 ms");
    }

    // Tiêu chí 2: đổi chuỗi giữa chừng — kết quả cũ không lẫn vào, bảng là đúng kết quả của chuỗi mới.
    void staleDropped()
    {
        FindDialog* d = make();
        d->setTexts(dense_);
        QSignalSpy finished(d, &FindDialog::searchFinished);
        const QString wide = u"d"_s; // lượt dài (rất nhiều kết quả)
        for (const QString& next : {u"duong ong"_s, u"tuong"_s, u"kt-12"_s}) {
            d->queryField()->setText(wide);
            spinFor(2ms);
            d->queryField()->setText(next);
            QVERIFY(spinUntil([d] { return !d->isSearching(); }));
            const std::vector<find::Hit> reference = find::findAll(*dense_, next, d->options());
            QCOMPARE(rowIds(d->results()), ids(*dense_, reference));
            QCOMPARE(finished.last().first().toInt(), int(reference.size()));
        }
        QCOMPARE(finished.size(), 3); // lượt bị thay không báo xong
    }

    // Tiêu chí 3: bấm dòng, ↑↓, Enter → objectRequested(id, layout); ↓ từ ô Tìm vào bảng; Esc đóng, bỏ tô sáng.
    void navigation()
    {
        FindDialog* d = make();
        QVERIFY(show(d));
        type(d, u"duong"_s);
        QAbstractItemView* v = d->results();
        QAbstractItemModel* m = v->model();
        QCOMPARE(m->rowCount(), 7);
        QSignalSpy object(d, &FindDialog::objectRequested);
        QSignalSpy cleared(d, &FindDialog::highlightCleared);
        QSignalSpy finished(d, &FindDialog::searchFinished);
        const auto check = [&](int row) {
            QCOMPARE(object.last().at(0).value<quint64>(), m->index(row, 0).data(FindDialog::IdRole).value<quint64>());
            QCOMPARE(object.last().at(1).toString(), m->index(row, 0).data(FindDialog::LayoutRole).toString());
        };
        QElapsedTimer clock;
        clock.start();
        QTest::mouseClick(v->viewport(), Qt::LeftButton, {}, v->visualRect(m->index(2, 0)).center()); // "Đường kính"
        const double clickMs = clock.nsecsElapsed() / 1e6;
        QCOMPARE(object.size(), 1);
        check(2);
        QCOMPARE(object.last().at(1).toString(), u"A1-MB"_s); // dòng ở layout khác: tự chuyển layout
        QTest::keyClick(v, Qt::Key_Down);
        QCOMPARE(object.size(), 2);
        check(3);
        QTest::keyClick(v, Qt::Key_Up);
        check(2);
        QTest::keyClick(v, Qt::Key_Return); // zoom lại, không chạy nút Tìm
        QCOMPARE(object.size(), 4);
        check(2);
        QVERIFY(!d->isSearching());
        QCOMPARE(finished.size(), 0);
        d->queryField()->setFocus();
        QTest::keyClick(d->queryField(), Qt::Key_Down); // ô Tìm → bảng, dòng kế tiếp
        QVERIFY(v->hasFocus());
        check(3);
        QTest::keyClick(d->queryField(), Qt::Key_Return); // Enter ở ô Tìm: tìm lại
        QVERIFY(spinUntil([d] { return !d->isSearching(); }));
        QCOMPARE(finished.size(), 1);
        QCOMPARE(cleared.size(), 1); // bảng thay: bỏ tô sáng
        QTest::mouseClick(v->viewport(), Qt::LeftButton, {}, v->visualRect(m->index(0, 0)).center());
        QTest::keyClick(d, Qt::Key_Escape);
        QVERIFY(!d->isVisible());
        QCOMPARE(cleared.size(), 2);
        qInfo("bấm dòng → objectRequested: %.2f ms", clickMs);
    }

    // Tiêu chí 4: Tạo tập chọn — mọi dòng hoặc các dòng đang chọn, id không trùng (7, 8 cùng block khung tên).
    void selectionSet()
    {
        FindDialog* d = make();
        QVERIFY(show(d));
        type(d, u"1"_s);
        QAbstractItemView* v = d->results();
        const QList<quint64> rows = rowIds(v);
        QCOMPARE(rows, (QList<quint64>{30, 70, 70, 90, 100, 220, 230, 270}));
        QSignalSpy spy(d, &FindDialog::selectionRequested);
        auto* set = d->findChild<ui::SplitButton*>(u"makeSet"_s);
        QVERIFY(set->isEnabled());
        set->click();
        QVERIFY(spinUntil([&] { return spy.size() == 1; }));
        QCOMPARE(spy.last().first().value<QList<quint64>>(), (QList<quint64>{30, 70, 90, 100, 220, 230, 270}));
        // Các dòng đang chọn (Ctrl+bấm): dòng 1, 2 (cùng id 70) và 5
        QAbstractItemModel* m = v->model();
        QTest::mouseClick(v->viewport(), Qt::LeftButton, {}, v->visualRect(m->index(1, 0)).center());
        QTest::mouseClick(v->viewport(), Qt::LeftButton, Qt::ControlModifier, v->visualRect(m->index(2, 0)).center());
        QTest::mouseClick(v->viewport(), Qt::LeftButton, Qt::ControlModifier, v->visualRect(m->index(5, 0)).center());
        QMenu* menu = set->dropMenu();
        emit menu->aboutToShow();
        QVERIFY(menu->actions().at(1)->isEnabled());
        QVERIFY(menu->actions().at(1)->text().contains(u"(3)"_s));
        menu->actions().at(1)->trigger();
        QVERIFY(spinUntil([&] { return spy.size() == 2; }));
        QCOMPARE(spy.last().first().value<QList<quint64>>(), (QList<quint64>{70, 220}));
        menu->actions().at(0)->trigger();
        QVERIFY(spinUntil([&] { return spy.size() == 3; }));
        QCOMPARE(spy.last().first().value<QList<quint64>>().size(), 7);
        type(d, u"khong co chu nay"_s);
        QVERIFY(!set->isEnabled());
    }

    // Tiêu chí 5: chưa gõ → giải thích tùy chọn dấu (theo trạng thái ô); không có → "Không tìm thấy “…”".
    void emptyStates()
    {
        FindDialog* d = make();
        QVERIFY(show(d));
        auto* empty = d->findChild<ui::EmptyState*>(u"empty"_s);
        QVERIFY(empty->isVisible());
        QVERIFY(!d->results()->isVisible());
        QString text = emptyText(d);
        QVERIFY2(text.contains(u"Gõ chữ cần tìm"_s) && text.contains(u"“duong”"_s) && text.contains(u"“đường”"_s),
                 qPrintable(text));
        box(d, "accents")->click(); // bật phân biệt dấu: câu giải thích đổi
        QVERIFY2(emptyText(d) != text && emptyText(d).contains(u"Tắt tùy chọn này"_s), qPrintable(emptyText(d)));
        type(d, u"duong"_s);
        QCOMPARE(d->resultCount(), 1); // phân biệt dấu: chỉ "DUONG ONG THOAT NUOC"
        box(d, "accents")->click();    // tắt: tìm lại, "duong" khớp cả "đường"
        QVERIFY(spinUntil([d] { return !d->isSearching(); }));
        QCOMPARE(d->resultCount(), 7);
        QVERIFY(d->results()->isVisible() && !empty->isVisible());
        type(d, u"khong co chu nay"_s);
        QVERIFY(empty->isVisible());
        QVERIFY2(emptyText(d).contains(u"Không tìm thấy “khong co chu nay”"_s), qPrintable(emptyText(d)));
        QCOMPARE(d->findChild<QLabel*>(u"count"_s)->text(), u"0 kết quả"_s);
        type(d, QString());
        QVERIFY(emptyText(d).contains(u"Gõ chữ cần tìm"_s));
        QCOMPARE(d->findChild<QLabel*>(u"count"_s)->text(), QString());
    }

    // Tiêu chí 6: Viewer, Viewer Pro: Thay bằng vô hiệu, Thay và Thay tất cả khóa, huy hiệu Pro; bấm → trang nâng cấp.
    // Pro (đổi lúc đang mở): mở khóa, nút bật khi có kết quả, phát replaceRequested (phần thay: UIF-002).
    void edition()
    {
        FindDialog* d = make();
        QVERIFY(show(d));
        QSignalSpy replace(d, &FindDialog::replaceRequested);
        QSignalSpy notice(&gate_->upgrade(), &edition::UpgradeRouter::noticeRequested);
        auto* field = d->findChild<QLineEdit*>(u"replace"_s);
        const auto badges = [d] {
            int n = 0;
            for (const edition::LockBadge* b : d->findChildren<edition::LockBadge*>())
                n += b->isVisibleTo(d) && b->text() == u"Pro"_s;
            return n;
        };
        for (const Edition e : {Edition::Viewer, Edition::ViewerPro}) {
            gate_->setEdition(e);
            QCOMPARE(d->windowTitle(), u"Tìm chữ"_s);
            QVERIFY(!field->isEnabled());
            QCOMPARE(field->placeholderText(), u"Thay chữ cần TrionCAD Pro"_s);
            QCOMPARE(badges(), 3);
            for (const char* name : {"replaceOne", "replaceAll"}) {
                QVERIFY(button(d, name)->isLocked());
                QVERIFY(button(d, name)->isEnabled());
                QCOMPARE(button(d, name)->accessibleDescription(), u"Có trong TrionCAD Pro"_s);
            }
            button(d, "replaceAll")->click();
            // lần đầu trong phiên: trang nâng cấp; lần sau: chú thích có liên kết (UpgradeRouter, mỗi tính năng một
            // lần)
            QCOMPARE(upgrades_.size() + notice.size(), e == Edition::Viewer ? 1 : 2);
            QCOMPARE(upgrades_.value(0), u"FINDREPLACE"_s);
            QCOMPARE(replace.size(), 0);
        }
        gate_->setEdition(Edition::Pro);
        QCOMPARE(d->windowTitle(), u"Tìm và thay thế"_s);
        QVERIFY(field->isEnabled());
        QCOMPARE(badges(), 0);
        QVERIFY(!button(d, "replaceAll")->isLocked());
        QVERIFY(!button(d, "replaceAll")->isEnabled()); // chưa có kết quả
        type(d, u"duong"_s);
        QVERIFY(button(d, "replaceAll")->isEnabled());
        button(d, "replaceAll")->click();
        button(d, "replaceOne")->click();
        QCOMPARE(replace.size(), 2);
        QCOMPARE(replace.at(0).first().toBool(), true);
        QCOMPARE(replace.at(1).first().toBool(), false);
        QCOMPARE(upgrades_.size() + notice.size(), 2); // Pro: không còn yêu cầu nâng cấp
    }

    // Phạm vi: Không gian hiện tại theo setCurrentSpace; Vùng chọn theo setSelection; nút chọn ẩn tạm hộp, endPick hiện
    // lại (picked: phạm vi thành Vùng chọn).
    void scopeAndPick()
    {
        FindDialog* d = make();
        QVERIFY(show(d));
        auto* scope = d->findChild<ui::DropDown*>(u"scope"_s);
        type(d, u"duong"_s);
        QCOMPARE(d->resultCount(), 7);
        d->setCurrentSpace(u"A1-MB"_s);
        scope->setCurrentIndex(1);
        QVERIFY(spinUntil([d] { return !d->isSearching(); }));
        QCOMPARE(rowIds(d->results()), (QList<quint64>{130}));
        d->setCurrentSpace(u"Model"_s); // đang ở phạm vi này: tìm lại
        QVERIFY(spinUntil([d] { return !d->isSearching(); }));
        QCOMPARE(d->resultCount(), 6);
        d->setSelection({240, 10, 10});
        scope->setCurrentIndex(2);
        QVERIFY(spinUntil([d] { return !d->isSearching(); }));
        QCOMPARE(rowIds(d->results()), (QList<quint64>{10, 240}));
        scope->setCurrentIndex(0);
        QVERIFY(spinUntil([d] { return !d->isSearching(); }));
        QSignalSpy pick(d, &FindDialog::pickRequested);
        button(d, "pick")->click();
        QCOMPARE(pick.size(), 1);
        QVERIFY(d->isHiddenForPick());
        d->endPick(false); // Esc: giữ phạm vi
        QVERIFY(!d->isHiddenForPick() && d->isVisible());
        QCOMPARE(scope->currentIndex(), 0);
        button(d, "pick")->click();
        d->setSelection({20});
        d->endPick(true);
        QVERIFY(d->isVisible());
        QCOMPARE(scope->currentIndex(), 2);
        QVERIFY(spinUntil([d] { return !d->isSearching(); }));
        QCOMPARE(rowIds(d->results()), (QList<quint64>{20}));
        d->setSelection({});
        QVERIFY(spinUntil([d] { return !d->isSearching(); }));
        QVERIFY2(emptyText(d).contains(u"Vùng chọn đang trống"_s), qPrintable(emptyText(d)));
    }

    // Tùy chọn lưu trong Store "find/…", hộp sau đọc lại; khung Tùy chọn tìm thu gọn/mở và nhớ.
    void optionsPersist()
    {
        FindDialog* d = make();
        QVERIFY(show(d));
        auto* options = d->findChild<QWidget*>(u"options"_s);
        QVERIFY(!options->isVisible()); // mặc định thu gọn như <details> của prototype
        const int collapsed = d->height();
        button(d, "optionsToggle")->click();
        QVERIFY(options->isVisible());
        QVERIFY(d->height() > collapsed);
        box(d, "wildcard")->click();
        box(d, "blocks")->click();
        QCOMPARE(store_->get(settings::Setting<bool>{"find/wildcard", false}), true);
        QCOMPARE(store_->get(settings::Setting<bool>{"find/blocks", true}), false);
        QVERIFY(d->options().wildcard && !d->options().inBlocks);
        d = make();
        QVERIFY(show(d));
        QVERIFY(d->findChild<QWidget*>(u"options"_s)->isVisible());
        QVERIFY(box(d, "wildcard")->isChecked() && !box(d, "blocks")->isChecked());
        button(d, "optionsToggle")->click();
        QVERIFY(!d->findChild<QWidget*>(u"options"_s)->isVisible());
        QVERIFY(d->height() < collapsed + d->findChild<QWidget*>(u"options"_s)->sizeHint().height());
    }

    // Đoạn khớp tô nền --c-warn-soft (cùng chữ đậm), nhãn TCVN3 .tag.warn; cột Vị trí không tô. Ảnh không chữ: đếm điểm
    // ảnh đúng màu nền tô (mọi nền tảng).
    void highlightPixels()
    {
        FindDialog* d = make();
        QVERIFY(show(d));
        type(d, u"giao thong"_s);
        QAbstractItemView* v = d->results();
        QAbstractItemModel* m = v->model();
        QCOMPARE(m->rowCount(), 1);
        QCOMPARE(m->index(0, FindDialog::KindColumn).data(FindDialog::LegacyRole).toString(), u"TCVN3"_s);
        QCOMPARE(m->index(0, FindDialog::SnippetColumn).data().toString(), u"Đường giao thông"_s);
        QCOMPARE(m->index(0, FindDialog::SnippetColumn).data(FindDialog::MatchRole).toPoint(), QPoint(6, 10));
        QCOMPARE(m->index(0, FindDialog::WhereColumn).data().toString(), u"Model"_s);
        QCOMPARE(m->index(0, FindDialog::KindColumn).data().toString(), u"Text"_s);
        for (const theme::Theme t : {theme::Theme::Dark, theme::Theme::Light}) {
            tm_->setTheme(t);
            QCoreApplication::processEvents();
            const QImage img = v->viewport()->grab().toImage();
            const QRect where = v->visualRect(m->index(0, FindDialog::WhereColumn));
            const QRect kind = v->visualRect(m->index(0, FindDialog::KindColumn));
            const QRect text = v->visualRect(m->index(0, FindDialog::SnippetColumn));
            const QColor bg = img.pixelColor(text.right() - 2, text.center().y()); // sau chữ ngắn: nền dòng
            const QColor mark = over(bg, tm_->color(tok::c_warn_soft));
            QVERIFY2(countColor(img, text, mark) > 20, qPrintable(mark.name()));
            QVERIFY(countColor(img, kind, mark) > 20); // nhãn TCVN3
            QCOMPARE(countColor(img, where, mark), 0);
        }
        // Đoạn chữ dài: cửa sổ quanh đoạn khớp, "…" ở chỗ cắt, xuống dòng thành dấu cách
        auto list = std::make_shared<find::TextList>();
        find::TextItem item;
        item.id = 1;
        item.kind = K::MText;
        item.layout = u"Model"_s;
        item.text = QString(60, u'x') + u"\nđường cuối"_s;
        list->push_back(item);
        d->setTexts(list);
        QVERIFY(spinUntil([d] { return !d->isSearching(); }));
        type(d, u"duong"_s);
        const QString shown = m->index(0, FindDialog::SnippetColumn).data().toString();
        const QPoint match = m->index(0, FindDialog::SnippetColumn).data(FindDialog::MatchRole).toPoint();
        QVERIFY2(shown.startsWith(u'…') && !shown.contains(u'\n'), qPrintable(shown));
        QCOMPARE(shown.mid(match.x(), match.y()), u"đường"_s);
    }

    // DoD-L: mở hộp tới khung đầu ≤ 150 ms (CPU luồng UI, release).
    void openTime()
    {
        double best = 1e9;
        for (int round = 0; round < 3; ++round) {
            delete dlg_;
            QCoreApplication::processEvents();
            const auto t0 = trion::test::threadCpu();
            FindDialog* d = make();
            QVERIFY(show(d));
            QCoreApplication::processEvents();
            const auto t1 = trion::test::threadCpu();
            if (t0.count() >= 0)
                best = std::min(best, ms(t1 - t0));
        }
        qInfo("mở hộp Tìm chữ tới khung đầu: %.1f ms CPU (tốt nhất của 3)", best);
        QVERIFY2(!kTimed || best <= 150, qPrintable(u"%1 ms"_s.arg(best)));
    }

    // DoD-U 7: tên cho trình đọc màn hình, Tab đi hết vòng, có viền focus — thu gọn, mở tùy chọn có kết quả, Viewer.
    void a11y()
    {
        FindDialog* d = make();
        QVERIFY(show(d));
        QStringList issues = trion::a11y::checkNames(d) + trion::a11y::checkFocus(d);
        QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
        button(d, "optionsToggle")->click();
        type(d, u"kt-1"_s);
        QStringList order;
        issues = trion::a11y::checkNames(d) + trion::a11y::checkFocus(d, &order);
        QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
        const auto at = [&order](const char* name) { // mục: "<lớp> '<objectName>'"
            for (int i = 0; i < order.size(); ++i)
                if (order[i].endsWith(u"'%1'"_s.arg(QLatin1StringView(name))))
                    return i;
            return -1;
        };
        QVERIFY2(at("query") >= 0 && at("results") > at("query") && at("matchCase") > at("query"),
                 qPrintable(order.join(u", "_s)));
        gate_->setEdition(Edition::Viewer);
        issues = trion::a11y::checkNames(d) + trion::a11y::checkFocus(d);
        QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
    }

    // vi-XL (chuỗi dài hơn 40%): 0 chữ bị cắt ở hai mật độ, Pro và Viewer, chưa gõ / có kết quả / không tìm thấy.
    void pseudoNoTruncation()
    {
        FindDialog* d = make();
        QVERIFY(show(d));
        button(d, "optionsToggle")->click();
        const QString before = d->windowTitle();
        QVERIFY(translator_->setLanguage(i18n::kPseudoLanguage.toString()));
        QVERIFY(spinUntil([&] { return d->windowTitle() != before; }));
        QCoreApplication::processEvents();
        for (const theme::Density density : {theme::Density::Comfortable, theme::Density::Compact}) {
            tm_->setDensity(density);
            for (const Edition e : {Edition::Pro, Edition::Viewer}) {
                gate_->setEdition(e);
                for (const QString& q : {QString(), u"kt-1"_s, u"khong co"_s}) {
                    type(d, q);
                    QCoreApplication::processEvents();
                    QCOMPARE(truncations(d), QString());
                }
            }
        }
        QVERIFY(d->width() < 1366); // vẫn vừa màn hình 1366 × 768
    }

    // Rảnh (DoD-H 6): tìm xong thì vòng quay ẩn, đứng; không timer nào chạy.
    void idle()
    {
        FindDialog* d = make();
        QVERIFY(show(d));
        d->setTexts(dense_);
        type(d, u"duong"_s);
        spinFor(50ms);
        auto* spinner = d->findChild<ui::Spinner*>(u"spinner"_s);
        QVERIFY(!spinner->isVisible() && !spinner->spinning());
        for (QTimer* t : d->findChildren<QTimer*>())
            QVERIFY2(!t->isActive(), qPrintable(t->objectName()));
    }
};

QTEST_MAIN(FindDialogTest)
#include "find_dialog_test.moc"
