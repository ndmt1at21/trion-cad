// UI0-006 · Bảng mẫu thành phần (Gallery.h). Chuỗi giao diện một ngữ cảnh dịch "Gallery" (gallery_en.json).
#include "app/widgets/gallery/Gallery.h"

#include "app/core/i18n/Translator.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/model/BackgroundModel.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "app/widgets/views/VirtualTreeView.h"

#include <QApplication>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QEventLoop>
#include <QGridLayout>
#include <QHeaderView>
#include <QHoverEvent>
#include <QLabel>
#include <QPainter>
#include <QPointer>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QStandardItemModel>
#include <QTimer>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <set>
#include <span>
#include <tuple>
#include <vector>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

namespace trion::ui {

namespace {

using theme::TextRole;
using theme::Weight;

theme::ThemeManager& tm()
{
    return theme::ThemeManager::instance();
}
int px(tok::PxId id)
{
    return qRound(tm().px(id));
}

// 8 trạng thái của 07.3 Phụ lục C, theo thứ tự cột của ma trận.
enum State { Default, Hover, Pressed, Focus, Disabled, On, Locked, Error, kStates };
constexpr const char* kStateIds[kStates] = {"default",  "hover", "pressed", "focus",
                                            "disabled", "on",    "locked",  "error"};

QString stateName(int s)
{
    switch (s) {
    case Default:
        return QCoreApplication::translate("Gallery", "Mặc định");
    case Hover:
        return QCoreApplication::translate("Gallery", "Rê");
    case Pressed:
        return QCoreApplication::translate("Gallery", "Nhấn");
    case Focus:
        return QCoreApplication::translate("Gallery", "Focus");
    case Disabled:
        return QCoreApplication::translate("Gallery", "Vô hiệu");
    case On:
        return QCoreApplication::translate("Gallery", "Bật");
    case Locked:
        return QCoreApplication::translate("Gallery", "Khóa");
    default:
        return QCoreApplication::translate("Gallery", "Lỗi");
    }
}

void hover(QWidget* w)
{
    w->setAttribute(Qt::WA_UnderMouse);
}

void keyboardFocus(QWidget* w)
{
    w->setFocus(Qt::TabFocusReason);
    w->window()->setAttribute(Qt::WA_KeyboardFocusChange); // như khi focus đến bằng phím Tab
}

// ------------------------------------------------------------------------------------------------- hàng ma trận

// Một hàng của ma trận và bản "Dùng thử": dựng điều khiển (chữ đã dịch), đặt trạng thái sau khi hiện; false: thành
// phần không có trạng thái này (ô "—", lý do trong docs/ui/thanh-phan.md).
struct Row {
    const char* id;   // ô: "<id>/<trạng thái>", bản thật: "live/<id>"
    const char* name; // tên thành phần (QT_TRANSLATE_NOOP, ngữ cảnh "Gallery")
    int width;        // bề rộng cố định, số lần --sp-6; 0: theo sizeHint
    QWidget* (*make)(QWidget* parent);
    bool (*apply)(QWidget* w, State s);
    TipInfo (*tip)() = nullptr; // tooltip của bản thật
    State popup = kStates;      // trạng thái mở popup: chụp sau cùng (popup lấy focus, cửa sổ active)
};

// Rê, focus, vô hiệu, khóa, lỗi: như nhau ở mọi điều khiển có setLocked/setError.
template <class T>
bool common(T* w, State s)
{
    switch (s) {
    case Hover:
        hover(w);
        break;
    case Focus:
        keyboardFocus(w);
        break;
    case Disabled:
        w->setEnabled(false);
        break;
    case Locked:
        w->setLocked(true);
        break;
    case Error:
        w->setError(true);
        break;
    default:
        break;
    }
    return true;
}

bool popupOpened()
{
    return QApplication::activePopupWidget() != nullptr;
}

bool applyButton(QWidget* w, State s)
{
    auto* b = static_cast<Button*>(w);
    if (s == Pressed)
        b->setDown(true);
    if (s != On)
        return common(b, s);
    if (auto* split = qobject_cast<SplitButton*>(b)) { // nút tách: bật = menu đang mở
        split->showDropMenu();
        return popupOpened();
    }
    if (b->kind() == Button::Kind::Primary || b->kind() == Button::Kind::Danger)
        return false; // hành động một lần, không giữ trạng thái
    b->setCheckable(true);
    b->setChecked(true);
    return true;
}

// DropDown, ComboBox: nhấn là mở danh sách; không có trạng thái bật.
bool applyDropDown(QWidget* w, State s)
{
    auto* d = static_cast<DropDown*>(w);
    if (s == Pressed) {
        d->showPopup();
        return popupOpened();
    }
    return s != On && common(d, s);
}

bool applySearch(QWidget* w, State s)
{
    auto* f = static_cast<SearchField*>(w);
    if (s == Pressed) { // đang gõ
        keyboardFocus(f);
        f->setText(u"KT"_s); // i18n-ok: dữ liệu mẫu
        return true;
    }
    if (s == Error)
        f->setText(u"KT-01?"_s); // i18n-ok: dữ liệu mẫu
    return s != On && common(f, s);
}

bool applySlider(QWidget* w, State s)
{
    auto* sl = static_cast<Slider*>(w);
    if (s == Pressed)
        sl->setSliderDown(true);
    return s != On && common(sl, s);
}

// Nhóm phân đoạn: trạng thái của từng đoạn (rê, nhấn đoạn 2; focus đoạn 1; khóa đoạn 3), bật = chọn đoạn 2.
bool applySegmented(QWidget* w, State s)
{
    auto* g = static_cast<SegmentedGroup*>(w);
    switch (s) {
    case Hover:
        hover(g->segment(1));
        break;
    case Pressed:
        g->segment(1)->setDown(true);
        break;
    case Focus:
        keyboardFocus(g->segment(0));
        break;
    case Disabled:
        g->setEnabled(false);
        break;
    case On:
        g->setCurrentIndex(1);
        break;
    case Locked:
        g->segment(2)->setLocked(true);
        break;
    case Error:
        g->setError(true);
        break;
    default:
        break;
    }
    return true;
}

// Tab: rê, khóa, lỗi ở tab 2; nhấn là mở tab ngay; tab đang mở có ở mọi ô nên không có ô bật riêng.
bool applyTabs(QWidget* w, State s)
{
    auto* t = static_cast<TabBar*>(w);
    switch (s) {
    case Hover: {
        const QPointF at = t->tabRect(1).center();
        QHoverEvent e(QEvent::HoverMove, at, t->mapToGlobal(at), QPointF(-1, -1));
        QCoreApplication::sendEvent(t, &e);
        hover(t);
        break;
    }
    case Pressed:
        t->setCurrentIndex(1);
        break;
    case Focus:
        keyboardFocus(t);
        break;
    case Disabled:
        t->setEnabled(false);
        break;
    case On:
        return false;
    case Locked:
        t->setTabLocked(1, true);
        break;
    case Error:
        t->setTabError(1, true);
        break;
    default:
        break;
    }
    return true;
}

QWidget* button(QWidget* parent, const QString& text, Button::Kind kind, const QString& icon = {})
{
    auto* b = new Button(text, kind, parent);
    if (!icon.isEmpty())
        b->setIconName(icon);
    return b;
}

constexpr Row kRows[] = {
    {"button", QT_TRANSLATE_NOOP("Gallery", "Nút"), 0,
     [](QWidget* p) { return button(p, QCoreApplication::translate("Gallery", "Xem trước…"), Button::Kind::Normal); },
     applyButton, [] { return TipInfo{.command = u"PREVIEW"_s}; }},
    {"primary", QT_TRANSLATE_NOOP("Gallery", "Nút chính"), 0,
     [](QWidget* p) {
         return button(p, QCoreApplication::translate("Gallery", "In"), Button::Kind::Primary, u"print"_s);
     },
     applyButton, [] { return TipInfo{.command = u"PLOT"_s, .keyId = u"plot"_s}; }},
    {"danger", QT_TRANSLATE_NOOP("Gallery", "Nút nguy hiểm"), 0,
     [](QWidget* p) { return button(p, QCoreApplication::translate("Gallery", "Xóa 3 layout"), Button::Kind::Danger); },
     applyButton},
    {"ghost", QT_TRANSLATE_NOOP("Gallery", "Nút phụ"), 0,
     [](QWidget* p) { return button(p, QCoreApplication::translate("Gallery", "Hủy"), Button::Kind::Ghost); },
     applyButton},
    {"icon", QT_TRANSLATE_NOOP("Gallery", "Nút biểu tượng"), 0,
     [](QWidget* p) {
         QWidget* b = button(p, {}, Button::Kind::Icon, u"osnap"_s);
         b->setAccessibleName(QCoreApplication::translate("Gallery", "Bắt điểm"));
         return b;
     },
     applyButton,
     [] {
         return TipInfo{.name = QCoreApplication::translate("Gallery", "Bắt điểm"),
                        .command = u"OSNAP"_s,
                        .keyId = u"osnap"_s,
                        .description = QCoreApplication::translate(
                            "Gallery", "Bắt vào điểm cuối, trung điểm, tâm… của đối tượng khi vẽ.")};
     }},
    {"toggle", QT_TRANSLATE_NOOP("Gallery", "Nút bật/tắt"), 0,
     [](QWidget* p) -> QWidget* { return new ToggleButton(QCoreApplication::translate("Gallery", "Lưới"), {}, p); },
     applyButton,
     [] {
         return TipInfo{.command = u"GRIDMODE"_s,
                        .keyId = u"grid"_s,
                        .description = QCoreApplication::translate("Gallery", "Hiện lưới nền trong vùng vẽ.")};
     }},
    {"split", QT_TRANSLATE_NOOP("Gallery", "Nút tách"), 0,
     [](QWidget* p) -> QWidget* {
         auto* split = new SplitButton(QCoreApplication::translate("Gallery", "In"), nullptr, p);
         auto* menu = new Menu(split);
         menu->addAction(QCoreApplication::translate("Gallery", "In hàng loạt"));
         menu->addAction(QCoreApplication::translate("Gallery", "Xuất PDF"));
         split->setDropMenu(menu);
         return split;
     },
     applyButton, [] { return TipInfo{.command = u"PLOT"_s, .keyId = u"plot"_s}; }, On},
    {"segmented", QT_TRANSLATE_NOOP("Gallery", "Nhóm phân đoạn"), 0,
     [](QWidget* p) -> QWidget* {
         auto* g = new SegmentedGroup(p);
         for (const QString& unit : {u"mm"_s, u"m"_s, u"in"_s}) // i18n-ok: đơn vị đo
             g->addSegment(unit);
         return g;
     },
     applySegmented},
    {"dropdown", QT_TRANSLATE_NOOP("Gallery", "Danh sách thả xuống"), 7,
     [](QWidget* p) -> QWidget* {
         auto* d = new DropDown(p);
         d->addItems({u"ISO A1"_s, u"ISO A3"_s, u"ISO A4"_s}); // i18n-ok: khổ giấy
         return d;
     },
     applyDropDown, nullptr, Pressed},
    {"combobox", QT_TRANSLATE_NOOP("Gallery", "Combo có ô lọc"), 7,
     [](QWidget* p) -> QWidget* {
         auto* c = new ComboBox(p);
         c->addItems({u"KT-CỬA"_s, u"KT-TƯỜNG"_s, u"KT-NỘITHẤT"_s, u"KC-DẦM"_s}); // i18n-ok: tên layer mẫu
         return c;
     },
     applyDropDown, nullptr, Pressed},
    {"search", QT_TRANSLATE_NOOP("Gallery", "Ô tìm kiếm"), 7,
     [](QWidget* p) -> QWidget* {
         auto* f = new SearchField(p);
         f->setPlaceholderText(QCoreApplication::translate("Gallery", "Tìm layer"));
         return f;
     },
     applySearch},
    {"tabs", QT_TRANSLATE_NOOP("Gallery", "Tab"), 0,
     [](QWidget* p) -> QWidget* {
         auto* t = new TabBar(p);
         t->addTab(QCoreApplication::translate("Gallery", "Chung"));
         t->addTab(QCoreApplication::translate("Gallery", "Thống kê"));
         return t;
     },
     applyTabs},
    {"slider", QT_TRANSLATE_NOOP("Gallery", "Thanh trượt"), 7,
     [](QWidget* p) -> QWidget* {
         auto* s = new Slider(Qt::Horizontal, p);
         s->setRange(0, 100);
         s->setValue(40);
         s->setAccessibleName(QCoreApplication::translate("Gallery", "Độ mờ"));
         return s;
     },
     applySlider},
};
constexpr int kRowCount = int(std::size(kRows));

// ------------------------------------------------------------------------------------------------- widget trang

QImage snap(QWidget* w, qreal dpr, QWidget::RenderFlags flags = QWidget::DrawChildren)
{
    QImage img(w->size() * dpr, QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(dpr);
    img.fill(tm().color(tok::c_base));
    QPainter p(&img);
    w->render(&p, {}, {}, flags);
    return img;
}

// Ô ảnh chụp một thành phần ở một trạng thái; ảnh rỗng: không có trạng thái này, vẽ "—".
class Shot final : public QWidget {
public:
    using QWidget::QWidget;
    const QImage& image() const { return image_; }
    void setImage(QImage image)
    {
        image_ = std::move(image);
        setAccessibleDescription(image_.isNull() ? QCoreApplication::translate("Gallery", "Không có trạng thái này")
                                                 : QString());
        updateGeometry();
        update();
    }
    QSize sizeHint() const override
    {
        return image_.isNull() ? QSize(px(tok::sp_7), px(tok::h_ctl)) : image_.deviceIndependentSize().toSize();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        if (!image_.isNull()) {
            p.drawImage(QPointF(0, (height() - image_.deviceIndependentSize().height()) / 2), image_);
            return;
        }
        p.setPen(tm().color(tok::c_text_3));
        p.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter, u"—"_s);
    }

private:
    QImage image_;
};

// Khung .card của styleguide: nền --c-base, viền --c-border, bo --r-xl, lề --sp-6.
class Card final : public QWidget {
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(tm().color(tok::c_border));
        p.setBrush(tm().color(tok::c_base));
        const qreal r = tm().px(tok::r_xl);
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), r, r);
    }
};

// Chữ phụ (.muted, --c-text-2). QLabel vẽ theo palette: đặt lại màu mỗi lần đổi chủ đề.
class Note final : public QLabel {
public:
    Note(const QString& text, QWidget* parent, TextRole role = TextRole::Ui) : QLabel(text, parent)
    {
        setFont(theme::font(role));
        recolor();
        connect(&tm(), &theme::ThemeManager::changed, this, [this] { recolor(); });
    }

private:
    void recolor()
    {
        QPalette pal = palette();
        pal.setColor(QPalette::WindowText, tm().color(tok::c_text_2));
        setPalette(pal);
    }
};

QLabel* heading(const QString& text, TextRole role, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    l->setFont(theme::font(role, Weight::Bold));
    return l;
}

// Mục của trang: tiêu đề rồi khung; trả khung.
Card* section(QVBoxLayout* page, const QString& title)
{
    page->addSpacing(px(tok::sp_7));
    page->addWidget(heading(title, TextRole::Title, page->parentWidget()));
    auto* card = new Card(page->parentWidget());
    page->addWidget(card);
    return card;
}

template <class Layout>
Layout* padded(QWidget* card)
{
    auto* l = new Layout(card);
    const int m = px(tok::sp_6);
    l->setContentsMargins(m, m, m, m);
    l->setSpacing(px(tok::sp_4));
    return l;
}

SegmentedGroup* segmented(QWidget* parent, const QString& id, const QString& name, const QStringList& segments)
{
    auto* g = new SegmentedGroup(parent);
    g->setObjectName(id);
    g->setAccessibleName(name);
    for (int i = 0; i < segments.size(); ++i)
        g->addSegment(segments[i])->setObjectName(u"%1/%2"_s.arg(id).arg(i)); // giữ focus qua lần dựng lại trang
    return g;
}

// Bảng/cây ảnh trạng thái: model đồng bộ (chụp ngay, không chờ model nền); dữ liệu mẫu.
void fillSample(QStandardItemModel& m, bool tree, bool empty)
{
    m.setColumnCount(2);
    m.setHorizontalHeaderLabels(
        {QCoreApplication::translate("Gallery", "Tên"), QCoreApplication::translate("Gallery", "Số")});
    m.setHeaderData(1, Qt::Horizontal, QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    if (empty)
        return;
    struct Item {
        const char* name;
        int count, depth, open; // open: -1 lá, 0 đóng, 1 mở
    };
    static constexpr Item kTable[] = {{"KT-CỬA", 11, 0, -1},
                                      {"KT-TƯỜNG", 182, 0, -1},
                                      {"KT-NỘITHẤT", 28, 0, -1},
                                      {"KT-KÍCHTHƯỚC", 64, 0, -1},
                                      {"KC-DẦM", 5, 0, -1}};
    static constexpr Item kTree[] = {
        {"KT", 285, 0, 1}, {"KT-TƯỜNG", 182, 1, -1}, {"KT-CỬA", 11, 1, 1}, {"CỬA-D1", 7, 2, -1}, {"KC", 5, 0, 0}};
    for (const Item& it : tree ? std::span(kTree) : std::span(kTable)) {
        auto* name = new QStandardItem(QString::fromUtf8(it.name)); // dữ liệu mẫu
        name->setData(it.depth, VirtualTreeView::DepthRole);
        if (it.open >= 0)
            name->setData(it.open == 1, VirtualTreeView::ExpandedRole);
        auto* count = new QStandardItem;
        count->setData(it.count, Qt::DisplayRole);
        count->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m.appendRow({name, count});
    }
}

// Cỡ view: rộng `spans` lần --sp-7, cao vừa đầu cột và `rows` dòng; cột tên 2/3, cột số giãn.
void sizeView(VirtualTableView* v, int spans, int rows)
{
    const int w = spans * px(tok::sp_7);
    v->setFixedSize(w, v->header()->sizeHint().height() + rows * v->rowHeight() + 2 * v->frameWidth());
    v->header()->setStretchLastSection(true);
    v->header()->resizeSection(0, w * 2 / 3);
}

// Chờ cond() trong vòng sự kiện lồng, tối đa 1 s: chỉ dùng khi chụp tooltip (RichTooltip hiện từng mức theo timer).
bool spinUntil(const std::function<bool()>& cond)
{
    QEventLoop loop;
    QTimer tick;
    QObject::connect(&tick, &QTimer::timeout, &loop, [&] {
        if (cond())
            loop.quit();
    });
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    deadline.start(1s);
    tick.start(1);
    loop.exec();
    return cond();
}

// ------------------------------------------------------------------------------------------------- dữ liệu mẫu

struct SampleRow {
    std::uint64_t key = 0;
    QString name;
    int count = 0;
    int depth = 0;
    signed char open = -1; // cây: -1 lá, 0 đóng, 1 mở
    bool operator==(const SampleRow&) const = default;
};

struct SampleSpec {
    bool tree = false;
    std::set<std::uint64_t> open; // nút cây đang mở
    int column = -1;              // cột sắp; -1: thứ tự gốc
    Qt::SortOrder order = Qt::AscendingOrder;
};

constexpr int kTableRows = 10'000, kGroups = 100, kLeaves = 100;
constexpr std::uint64_t kGroupKey = 1'000'000; // khóa nhóm cây: không trùng khóa lá

QString sampleName(int i)
{
    static const char* const words[] = {"KT-TƯỜNG", "KT-CỬA", "ĐƯỜNG-TRỤC", "KÍCH-THƯỚC", "NỘI-THẤT", "GHI-CHÚ"};
    return u"%1 %2"_s.arg(QString::fromUtf8(words[i % 6])).arg(i + 1, 5, 10, QChar(u'0')); // dữ liệu mẫu
}

// Luồng nền: bảng 10.000 dòng (sắp theo cột), hoặc cây 100 nhóm × 100 lá làm phẳng theo nút đang mở.
std::vector<SampleRow> buildSample(const SampleSpec& s, const sched::CancelToken& cancel)
{
    std::vector<SampleRow> rows;
    if (s.tree) {
        for (int g = 0; g < kGroups && !cancel.cancelled(); ++g) {
            const std::uint64_t key = kGroupKey * std::uint64_t(g + 1);
            const bool open = s.open.count(key) > 0;
            rows.push_back({key, u"Nhóm %1"_s.arg(g + 1), kLeaves, 0, static_cast<signed char>(open)});
            for (int l = 0; open && l < kLeaves; ++l)
                rows.push_back({key + std::uint64_t(l) + 1, sampleName(g * kLeaves + l), (l * 37) % 100, 1});
        }
        return rows;
    }
    rows.reserve(kTableRows);
    for (int i = 0; i < kTableRows; ++i)
        rows.push_back({std::uint64_t(i) + 1, sampleName(i), (i * 7919) % 1000});
    if (s.column >= 0 && !cancel.cancelled())
        std::sort(rows.begin(), rows.end(), [&s](const SampleRow& a, const SampleRow& b) {
            const auto& [x, y] = s.order == Qt::AscendingOrder ? std::tie(a, b) : std::tie(b, a);
            if (s.column == 0 && x.name != y.name)
                return x.name < y.name;
            if (s.column == 1 && x.count != y.count)
                return x.count < y.count;
            return x.key < y.key; // thứ tự tất định
        });
    return rows;
}

} // namespace

// Bảng, cây bản thật: dựng và sắp ở nền (BackgroundModel, UI0-014); mở/đóng nút cây làm phẳng lại ở nền.
// ponytail: dữ liệu sinh ra; palette thật dựng từ snapshot DOC-009, sửa qua lệnh INT-002.
class GalleryWindow::SampleModel final : public BackgroundModel<SampleRow> {
public:
    explicit SampleModel(bool tree)
    {
        spec_.tree = tree;
        if (tree)
            spec_.open = {kGroupKey};
        rebuild();
    }
    int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : 2; }
    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid())
            return {};
        const SampleRow& r = row(index.row());
        if (role == Qt::DisplayRole)
            return index.column() == 0 ? QVariant(r.name) : QVariant(r.count);
        if (role == Qt::TextAlignmentRole && index.column() == 1) // .tbl td.num
            return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
        if (index.column() == 0 && role == VirtualTreeView::DepthRole && spec_.tree)
            return r.depth;
        if (index.column() == 0 && role == VirtualTreeView::ExpandedRole && r.open >= 0)
            return r.open == 1;
        return {};
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation != Qt::Horizontal)
            return {};
        if (role == Qt::TextAlignmentRole && section == 1)
            return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
        if (role != Qt::DisplayRole)
            return {};
        return section == 0 ? QCoreApplication::translate("Gallery", "Tên")
                            : QCoreApplication::translate("Gallery", "Số");
    }
    bool setData(const QModelIndex& index, const QVariant& value, int role) override
    {
        if (role != VirtualTreeView::ExpandedRole || !index.isValid() || row(index.row()).open < 0)
            return false;
        const std::uint64_t key = row(index.row()).key;
        if (value.toBool())
            spec_.open.insert(key);
        else
            spec_.open.erase(key);
        rebuild();
        return true;
    }
    void sort(int column, Qt::SortOrder order) override
    {
        spec_.column = column;
        spec_.order = order;
        rebuild();
    }

private:
    void rebuild() { refresh(std::make_shared<const SampleSpec>(spec_), ++version_, buildSample); }

    SampleSpec spec_;
    std::uint64_t version_ = 0;
};

// Ô ảnh và thanh chọn của trang hiện tại (dựng lại khi đổi ngôn ngữ).
struct GalleryWindow::Page {
    std::array<std::array<Shot*, kStates>, kRowCount> cells{};
    Shot* menu = nullptr;
    std::array<Shot*, 2> tips{};                 // mức 1, mức 2
    std::array<std::array<Shot*, 3>, 2> views{}; // [bảng, cây] × [trạng thái, vô hiệu, rỗng]
    VirtualTableView* table = nullptr;           // bản thật: cao theo --h-row của mật độ hiện tại
    VirtualTreeView* tree = nullptr;
    SegmentedGroup* theme = nullptr;
    SegmentedGroup* density = nullptr;
    SegmentedGroup* language = nullptr;
    ToggleButton* lite = nullptr;
};

GalleryWindow::GalleryWindow(QWidget* parent)
    : QMainWindow(parent), scroll_(new QScrollArea(this)), table_(std::make_unique<SampleModel>(false)),
      tree_(std::make_unique<SampleModel>(true))
{
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setFocusPolicy(Qt::NoFocus); // khung cuộn không là điểm dừng Tab (không có viền focus)
    setCentralWidget(scroll_);
    page_ = buildPage();
    scroll_->setWidget(page_);
    connect(&tm(), &theme::ThemeManager::changed, this, [this] {
        syncBar();
        scheduleStage();
    });
}

GalleryWindow::~GalleryWindow() = default;

QWidget* GalleryWindow::buildPage()
{
    setWindowTitle(QCoreApplication::translate("Gallery", "Bảng mẫu thành phần"));
    auto* page = new QWidget;
    page->setObjectName(u"page"_s);
    shots_ = std::make_unique<Page>();
    Page& pg = *shots_;
    auto* col = new QVBoxLayout(page);
    col->setContentsMargins(px(tok::sp_7), px(tok::sp_7), px(tok::sp_7), px(tok::sp_9));
    col->setSpacing(px(tok::sp_5));

    // Đầu trang và thanh chọn (styleguide: "Xem ở").
    col->addWidget(heading(QCoreApplication::translate("Gallery", "Bảng mẫu thành phần"), TextRole::Display, page));
    col->addWidget(new Note(QCoreApplication::translate("Gallery", "Thành phần cơ bản ở 8 trạng thái của 07.3 Phụ lục "
                                                                   "C. Ô trạng thái là ảnh chụp từ chính widget; mục "
                                                                   "Dùng thử là widget thật."),
                            page));
    auto* bar = new QHBoxLayout;
    bar->setSpacing(px(tok::sp_5));
    bar->addWidget(new Note(QCoreApplication::translate("Gallery", "Xem ở"), page));
    pg.theme =
        segmented(page, u"theme"_s, QCoreApplication::translate("Gallery", "Chủ đề"),
                  {QCoreApplication::translate("Gallery", "Theo hệ thống"),
                   QCoreApplication::translate("Gallery", "Tối"), QCoreApplication::translate("Gallery", "Sáng")});
    pg.density =
        segmented(page, u"density"_s, QCoreApplication::translate("Gallery", "Mật độ"),
                  {QCoreApplication::translate("Gallery", "Gọn"), QCoreApplication::translate("Gallery", "Thoáng")});
    pg.lite = new ToggleButton(QCoreApplication::translate("Gallery", "Giao diện nhẹ"), {}, page);
    pg.lite->setObjectName(u"lite"_s);
    bar->addWidget(pg.theme);
    bar->addWidget(pg.density);
    bar->addWidget(pg.lite);
    if (i18n::Translator::instance()) { // tên ngôn ngữ viết theo chính ngôn ngữ đó, không dịch
        pg.language = segmented(page, u"language"_s, QCoreApplication::translate("Gallery", "Ngôn ngữ"),
                                {u"Tiếng Việt"_s, u"English"_s, i18n::kPseudoLanguage.toString()});
        bar->addWidget(pg.language);
    }
    bar->addStretch();
    col->addLayout(bar);
    connect(pg.theme, &SegmentedGroup::currentIndexChanged, this, [](int i) {
        tm().setTheme(i == 0 ? theme::Theme::System : i == 1 ? theme::Theme::Dark : theme::Theme::Light);
    });
    connect(pg.density, &SegmentedGroup::currentIndexChanged, this,
            [](int i) { tm().setDensity(i == 0 ? theme::Density::Compact : theme::Density::Comfortable); });
    connect(pg.lite, &QAbstractButton::toggled, this, [](bool on) { tm().setLite(on); });
    if (pg.language)
        connect(pg.language, &SegmentedGroup::currentIndexChanged, this, [](int i) {
            static const QString langs[] = {u"vi"_s, u"en"_s, i18n::kPseudoLanguage.toString()};
            if (auto* t = i18n::Translator::instance())
                t->setLanguage(langs[i]);
        });

    // Dùng thử: mỗi thành phần một bản thật, theo nhóm như styleguide (Nút; Ô nhập, chọn). Thứ tự tạo = thứ tự Tab.
    Card* live = section(col, QCoreApplication::translate("Gallery", "Dùng thử"));
    auto* liveRows = padded<QVBoxLayout>(live);
    const auto makeLive = [live](int r, QHBoxLayout* into) {
        QWidget* w = kRows[r].make(live);
        w->setObjectName(u"live/"_s + QLatin1StringView(kRows[r].id));
        if (kRows[r].width)
            w->setFixedWidth(kRows[r].width * px(tok::sp_6));
        if (kRows[r].tip && RichTooltip::instance())
            RichTooltip::attach(w, kRows[r].tip());
        into->addWidget(w);
    };
    for (const auto& [first, last] : {std::pair{0, 6}, std::pair{7, 10}, std::pair{11, 12}}) {
        auto* line = new QHBoxLayout;
        line->setSpacing(px(tok::sp_4));
        for (int r = first; r <= last; ++r)
            makeLive(r, line);
        if (first == 0) { // cỡ nút: nhỏ, lớn (mặc định ở trên)
            auto* small = new Button(QCoreApplication::translate("Gallery", "Nhỏ"), Button::Kind::Normal, live);
            small->setObjectName(u"live/small"_s);
            small->setControlSize(Button::Size::Small);
            auto* large = new Button(QCoreApplication::translate("Gallery", "Lớn"), Button::Kind::Primary, live);
            large->setObjectName(u"live/large"_s);
            large->setControlSize(Button::Size::Large);
            line->addWidget(small);
            line->addWidget(large);
        }
        line->addStretch();
        liveRows->addLayout(line);
    }

    // Trạng thái: ma trận ảnh chụp, thành phần × 8 trạng thái.
    Card* states = section(col, QCoreApplication::translate("Gallery", "Trạng thái"));
    auto* grid = padded<QGridLayout>(states);
    grid->setHorizontalSpacing(px(tok::sp_5));
    grid->setVerticalSpacing(px(tok::sp_3));
    for (int s = 0; s < kStates; ++s)
        grid->addWidget(new Note(stateName(s), states, TextRole::Caption), 0, s + 1);
    for (int r = 0; r < kRowCount; ++r) {
        const QString name = QCoreApplication::translate("Gallery", kRows[r].name);
        grid->addWidget(new QLabel(name, states), r + 1, 0);
        for (int s = 0; s < kStates; ++s) {
            auto* shot = new Shot(states);
            shot->setObjectName(u"%1/%2"_s.arg(QLatin1StringView(kRows[r].id), QLatin1StringView(kStateIds[s])));
            shot->setAccessibleName(u"%1, %2"_s.arg(name, stateName(s)));
            grid->addWidget(shot, r + 1, s + 1, Qt::AlignLeft | Qt::AlignVCenter);
            pg.cells[r][s] = shot;
        }
    }
    grid->setColumnStretch(kStates + 1, 1);
    grid->addWidget(new Note(QCoreApplication::translate("Gallery", "— thành phần không có trạng thái này. Khóa: tính "
                                                                    "năng ngoài phiên bản hiện hành, có ổ khóa, không "
                                                                    "chỉ đổi màu."),
                             states),
                    kRowCount + 1, 0, 1, kStates + 2);

    // Menu, tooltip: ảnh menu (mục ở các trạng thái) và tooltip mức 1, mức 2.
    Card* menus = section(col, QCoreApplication::translate("Gallery", "Menu, tooltip"));
    auto* menuRow = padded<QHBoxLayout>(menus);
    menuRow->setSpacing(px(tok::sp_7));
    pg.menu = new Shot(menus);
    pg.menu->setObjectName(u"menu"_s);
    auto* tipCol = new QVBoxLayout;
    tipCol->setSpacing(px(tok::sp_4));
    for (int i = 0; i < 2; ++i) {
        pg.tips[i] = new Shot(menus);
        pg.tips[i]->setObjectName(u"tip/%1"_s.arg(i + 1));
        tipCol->addWidget(pg.tips[i], 0, Qt::AlignLeft);
    }
    tipCol->addStretch();
    auto* menuNote = new Note(QCoreApplication::translate("Gallery", "Menu: mục đang trỏ (rê; nhấn và phím mũi tên như "
                                                                     "rê), dấu chọn (bật), huy hiệu Pro (khóa), mục vô "
                                                                     "hiệu. Lỗi: không có.\nTooltip: mức 1 sau 0,45 s; "
                                                                     "rê thêm 1,5 s ra mức 2 có mô tả và F1. Thử ở mục "
                                                                     "Dùng thử."),
                              menus);
    menuNote->setWordWrap(true);
    menuRow->addWidget(pg.menu, 0, Qt::AlignTop);
    menuRow->addLayout(tipCol);
    menuRow->addWidget(menuNote, 1, Qt::AlignTop);

    // Bảng, cây: ảnh trạng thái rồi bản thật (dữ liệu nền, sắp theo cột, mở/đóng nút).
    Card* views = section(col, QCoreApplication::translate("Gallery", "Bảng, cây"));
    auto* vgrid = padded<QGridLayout>(views);
    vgrid->setHorizontalSpacing(px(tok::sp_6));
    const QString captions[] = {QCoreApplication::translate("Gallery", "Dòng 2 rê, dòng 3 chọn (bật), dòng 4 focus"),
                                QCoreApplication::translate("Gallery", "Vô hiệu"),
                                QCoreApplication::translate("Gallery", "Rỗng")};
    for (int t = 0; t < 2; ++t)
        for (int v = 0; v < 3; ++v) {
            auto* shot = new Shot(views);
            shot->setObjectName(u"%1/%2"_s.arg(t ? u"tree"_s : u"table"_s).arg(v));
            vgrid->addWidget(shot, t * 2, v, Qt::AlignLeft | Qt::AlignTop);
            vgrid->addWidget(new Note(captions[v], views, TextRole::Caption), t * 2 + 1, v);
            pg.views[t][v] = shot;
        }
    auto* table = new VirtualTableView(views);
    table->setObjectName(u"live/table"_s);
    table->setAccessibleName(QCoreApplication::translate("Gallery", "Bảng mẫu"));
    table->setModel(table_.get());
    table->setSortingEnabled(true);
    auto* tree = new VirtualTreeView(views);
    tree->setObjectName(u"live/tree"_s);
    tree->setAccessibleName(QCoreApplication::translate("Gallery", "Cây mẫu"));
    tree->setModel(tree_.get());
    pg.table = table;
    pg.tree = tree;
    auto* liveViews = new QHBoxLayout;
    liveViews->setSpacing(px(tok::sp_6));
    liveViews->addWidget(table);
    liveViews->addWidget(tree);
    liveViews->addStretch();
    vgrid->addLayout(liveViews, 4, 0, 1, 4);
    vgrid->addWidget(new Note(QCoreApplication::translate("Gallery", "Dùng thử: bấm đầu cột để sắp, gõ chữ để nhảy "
                                                                     "dòng, ← → đóng/mở nhóm."),
                              views, TextRole::Caption),
                     5, 0, 1, 4);
    vgrid->setColumnStretch(3, 1);
    col->addStretch();
    syncBar();
    return page;
}

void GalleryWindow::syncBar()
{
    const Page& pg = *shots_;
    sizeView(pg.table, 15, 8);
    sizeView(pg.tree, 15, 8);
    const theme::Theme t = tm().theme();
    pg.theme->setCurrentIndex(t == theme::Theme::System ? 0 : t == theme::Theme::Dark ? 1 : 2);
    pg.density->setCurrentIndex(tm().density() == theme::Density::Compact ? 0 : 1);
    pg.lite->setChecked(tm().lite());
    if (pg.language) {
        const QString lang = i18n::Translator::instance()->language();
        pg.language->setCurrentIndex(lang == u"en" ? 1 : lang == i18n::kPseudoLanguage ? 2 : 0);
    }
}

void GalleryWindow::rebuild()
{
    if (staging_) // trong vòng sự kiện lồng của stage(): trang đang được chụp, stage() hẹn lại khi xong
        return;
    rebuilding_ = false;
    QWidget* f = QApplication::focusWidget();
    const QString focus = f && page_->isAncestorOf(f) ? f->objectName() : QString();
    page_ = buildPage();
    scroll_->setWidget(page_); // hủy trang cũ
    QWidget* w = focus.isEmpty() ? nullptr : page_->findChild<QWidget*>(focus);
    // Đoạn chưa bật của nhóm chọn một (ngôn ngữ đổi từ nơi khác trong lúc focus ở đoạn cũ): focus đoạn đang bật, không
    // thì Qt dời điểm dừng Tab sang đoạn chưa bật và nhóm không còn điểm dừng ở lựa chọn hiện tại.
    if (auto* b = qobject_cast<QAbstractButton*>(w); b && b->group() && b->group()->exclusive() && !b->isChecked())
        w = b->group()->checkedButton();
    if (w)
        w->setFocus(Qt::OtherFocusReason);
    scheduleStage();
}

void GalleryWindow::scheduleStage()
{
    dirty_ = true;
    if (!std::exchange(queued_, true))
        QMetaObject::invokeMethod(
            this,
            [this] {
                queued_ = false;
                stage();
            },
            Qt::QueuedConnection);
}

void GalleryWindow::changeEvent(QEvent* e)
{
    QMainWindow::changeEvent(e);
    if (e->type() == QEvent::LanguageChange && !std::exchange(rebuilding_, true))
        QMetaObject::invokeMethod(this, &GalleryWindow::rebuild, Qt::QueuedConnection); // không hủy widget đang phát
    else if (e->type() == QEvent::ActivationChange && dirty_ && isActiveWindow())
        scheduleStage();
}

bool GalleryWindow::event(QEvent* e)
{
    if (e->type() == QEvent::DevicePixelRatioChange)
        scheduleStage();
    return QMainWindow::event(e);
}

void GalleryWindow::stage(qreal dpr)
{
    if (staging_ || !isVisible() || !isActiveWindow()) {
        dirty_ = true; // đang chụp (vòng sự kiện lồng của tooltip) hoặc chưa active: chụp lại sau
        return;
    }
    const QScopedValueRollback guard(staging_, true);
    dirty_ = false;
    if (dpr <= 0)
        dpr = devicePixelRatioF();
    const QPointer<QWidget> had = QApplication::focusWidget();
    const bool keyboard = testAttribute(Qt::WA_KeyboardFocusChange);
    truncations_.clear();
    Page& pg = *shots_;
    // Vùng dựng 0×0: con của cửa sổ đang active nên nhận được focus, nhưng không chiếm chỗ nào trên màn hình.
    QWidget area(this);
    area.setGeometry({});
    area.show();

    // ponytail: chụp cả trang trong một lượt sự kiện (~180 ms bản release, chủ yếu dựng ~120 widget và mở popup); cần
    // nhanh hơn thì chia theo hàng qua nhiều lượt.
    const auto report = [this](QWidget* w) { // ghi chuỗi ngay: widget hủy sau khi chụp
        for (const i18n::Truncation& t : i18n::findTruncations(w))
            truncations_ << t.toString();
    };
    const auto shoot = [&](int r, State s) {
        const Row& row = kRows[r];
        std::unique_ptr<QWidget> w(row.make(&area));
        const auto fit = [&] {
            const QSize hint = w->sizeHint();
            w->resize(row.width ? row.width * px(tok::sp_6) : hint.width(), hint.height());
        };
        fit();
        w->show();
        const bool applies = row.apply(w.get(), s);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest); // khóa: nút rộng thêm ổ khóa
        fit();
        pg.cells[r][s]->setImage(applies ? snap(w.get(), dpr) : QImage());
        if (applies)
            report(w.get());
        if (QWidget* popup = QApplication::activePopupWidget())
            popup->close();
        setAttribute(Qt::WA_KeyboardFocusChange, false);
    };
    for (int r = 0; r < kRowCount; ++r)
        for (int s = 0; s < kStates; ++s)
            if (kRows[r].popup != s)
                shoot(r, State(s));

    // Bảng, cây: dòng rê, dòng chọn, dòng hiện hành có focus bàn phím; vô hiệu; rỗng.
    for (int t = 0; t < 2; ++t)
        for (int v = 0; v < 3; ++v) {
            QStandardItemModel items;
            fillSample(items, t == 1, v == 2);
            std::unique_ptr<VirtualTableView> view(t ? new VirtualTreeView(&area) : new VirtualTableView(&area));
            view->setModel(&items);
            view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // vừa đủ 5 dòng
            view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            sizeView(view.get(), 10, 5);
            view->show();
            // Đầu cột giãn cột cuối bằng timer (chưa chạy khi chụp ngay): đặt thẳng cột số.
            view->header()->resizeSection(1, view->viewport()->width() - view->header()->sectionSize(0));
            if (v == 0) {
                const QPointF at = view->visualRect(items.index(1, 0)).center();
                QHoverEvent ev(QEvent::HoverMove, at, view->viewport()->mapToGlobal(at), QPointF(-1, -1));
                QCoreApplication::sendEvent(view->viewport(), &ev);
                view->selectionModel()->select(items.index(2, 0),
                                               QItemSelectionModel::Select | QItemSelectionModel::Rows);
                view->selectionModel()->setCurrentIndex(items.index(3, 0), QItemSelectionModel::NoUpdate);
                keyboardFocus(view.get());
            } else if (v == 1) {
                view->setEnabled(false);
            }
            pg.views[t][v]->setImage(snap(view.get(), dpr));
            report(view.get());
            setAttribute(Qt::WA_KeyboardFocusChange, false);
        }

    // Menu ngữ cảnh như styleguide (không hiện: vẽ thẳng widget đã bố trí).
    {
        Menu menu;
        menu.addSection(QCoreApplication::translate("Gallery", "Có vùng chọn"));
        menu.addAction(QCoreApplication::translate("Gallery", "Lặp lệnh %1").arg(u"DIST"_s));
        auto* recent = new Menu(QCoreApplication::translate("Gallery", "Lệnh gần đây"), &menu);
        recent->addAction(u"LINE"_s); // i18n-ok: tên lệnh
        menu.addMenu(recent);
        menu.addSeparator();
        QAction* end = menu.addAction(QCoreApplication::translate("Gallery", "Điểm cuối"));
        end->setCheckable(true);
        end->setChecked(true);
        QAction* props =
            menu.addAction(theme::IconProvider::icon(u"props"_s), QCoreApplication::translate("Gallery", "Thuộc tính"));
        RichTooltip::attach(props, {.command = u"PROPERTIES"_s, .keyId = u"props"_s});
        RichTooltip::attach(menu.addAction(QCoreApplication::translate("Gallery", "Chọn tương tự")),
                            {.command = u"SELECTSIMILAR"_s, .need = Edition::Pro});
        QAction* paste = menu.addAction(QCoreApplication::translate("Gallery", "Dán"));
        RichTooltip::attach(paste, {.command = u"PASTECLIP"_s, .keyId = u"paste"_s});
        paste->setEnabled(false);
        menu.ensurePolished();
        menu.resize(menu.sizeHint());
        menu.setActiveAction(props);
        pg.menu->setImage(snap(&menu, dpr, QWidget::DrawWindowBackground | QWidget::DrawChildren));
    }

    // Tooltip mức 1 và mức 2 (có huy hiệu Pro): hiện thật bằng RichTooltip của ứng dụng, chụp rồi ẩn.
    if (RichTooltip* tips = RichTooltip::instance()) {
        QAction circle, calc;
        RichTooltip::attach(
            &circle, {.name = QCoreApplication::translate("Gallery", "Đường tròn"),
                      .command = u"CIRCLE"_s,
                      .description = QCoreApplication::translate("Gallery", "Vẽ đường tròn theo tâm và bán kính.")});
        RichTooltip::attach(
            &calc, {.name = QCoreApplication::translate("Gallery", "Máy tính nhanh"),
                    .command = u"QUICKCALC"_s,
                    .keyId = u"qc"_s,
                    .description = QCoreApplication::translate("Gallery", "Tính biểu thức, lấy số đo từ bản vẽ."),
                    .need = Edition::Pro});
        for (int level : {1, 2}) {
            Shot* shot = pg.tips[level - 1];
            tips->setDelays(0ms, level == 1 ? 1h : 0ms);
            const QRect at(shot->mapToGlobal(QPoint()), shot->size());
            tips->hover(level == 1 ? &circle : &calc, at, at.center());
            const bool shown = spinUntil([&] { return tips->level() == level; });
            shot->setImage(shown ? snap(tips->tipWindow(), dpr, QWidget::DrawWindowBackground | QWidget::DrawChildren)
                                 : QImage());
            tips->leave();
        }
        tips->setDelays(450ms, 1500ms); // ponytail: RichTooltip chưa có hàm đọc độ trễ: trả về mặc định (styleguide)
    }

    // Ô mở popup sau cùng: popup lấy focus (offscreen: lấy cả cửa sổ active), ô focus phải chụp trước.
    for (int r = 0; r < kRowCount; ++r)
        if (kRows[r].popup != kStates)
            shoot(r, kRows[r].popup);

    setAttribute(Qt::WA_KeyboardFocusChange, keyboard);
    if (had)
        had->setFocus(Qt::OtherFocusReason);
    activateWindow(); // popup vừa đóng (offscreen: popup lấy cửa sổ active); đang active thì không đổi gì
    if (rebuilding_) // ngôn ngữ đổi giữa lúc chụp
        QMetaObject::invokeMethod(this, &GalleryWindow::rebuild, Qt::QueuedConnection);
    else if (dirty_) // chủ đề, mật độ đổi giữa lúc chụp
        scheduleStage();
}

} // namespace trion::ui
