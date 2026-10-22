#pragma once
// UIA-102 · Điều khiển ribbon (07.1 §7.1; prototype ctlHTML, ctlRun, paintGal trong js/shell.js; css/app.css .rb-lg
// .rb-s .rb-dd .rb-num .rb-gal .lkb) dựng từ mô tả JSON của RibbonModel:
//   Button   nút lớn (.rb-lg: biểu tượng --ic-lg trên, nhãn ≤ 2 dòng), nhỏ (.rb-s), chỉ biểu tượng (.rb-s.io)
//   Split    phần chính chạy lựa chọn gần nhất (nhớ qua Store, giữ sau khởi động lại); mũi tên (nút lớn: nửa dưới,
//            nút nhỏ: vùng ▾ cuối nút), phím ↓ hoặc Alt+↓ mở danh sách; menuOnly: cả nút mở danh sách. Nhãn giữ
//            nguyên, biểu tượng và tooltip theo lựa chọn gần nhất (như "SplitFollowStaticText" của AutoCAD: đổi lựa
//            chọn không làm panel đổi bề rộng)
//   Toggle   trạng thái chỉ theo QAction checkable của ActionRegistry (sysvar): đổi từ dòng lệnh, LISP hiện ngay
//            trong lượt gọi Sysvars::set; bấm chỉ gửi lệnh
//   DropDown ui::DropDown trên model của source; Gallery: lưới mẫu ảo hóa (QListView, không widget cho từng mục)
//   Number   nhãn + ô số UI-C09 (NumericField); Slider: nhãn + ui::Slider + giá trị. Nhận giá trị → 'SYSVAR giá-trị
// Trạng thái (07.3 Phụ lục C): rê, nhấn, vô hiệu (QAction của lệnh vô hiệu thì nút vô hiệu), bật, focus bàn phím,
// khóa theo phiên bản (EditionGate::instance(): huy hiệu LockBadge Style::Ribbon, chữ --c-text-2; bấm mở trang nâng
// cấp qua cổng của ActionRegistry), đang tải/rỗng/lỗi của danh sách theo source (setDataState). Màu, cỡ đọc token mỗi
// lần vẽ, đo. Biểu tượng chỉ qua IconProvider theo tên.
//
// Lệnh: nút, mục không tham số gọi QAction của ActionRegistry (cổng phiên bản, chỗ nối INT-002 của nó); mục có tham số,
// ô số, thanh trượt phát macroRequested (bước integration nối vào bộ định tuyến tạm của UIA-001). Không tự chạy lệnh.
//
// Dùng (luồng UI, sau ThemeManager, IconProvider, RichTooltip, ActionRegistry, EditionGate):
//   trion::ribbon::RibbonModel model;
//   trion::ribbon::RibbonControls controls(actions);
//   connect(&controls, &RibbonControls::macroRequested, router, &Router::submit);    // như ActionRegistry::setSink
//   controls.setModel(u"textstyles"_s, textStyleModel);                             // UIA-104, INT-… (nguồn động)
//   auto* pages = new RibbonPages(int(model.tabs().size()), [&](int i) { return buildTabPage(model.tabs()[i]); });
//   pages->setCurrentTab(0);  // UIA-005: buildTabPage dựng khung panel quanh controls.buildPanel(panel)

#include "app/shell/ribbon/RibbonModel.h"
#include "core/base/settings/Settings.h"

#include <QAbstractButton>
#include <QAbstractItemModel>
#include <QHash>
#include <QPointer>
#include <QStackedWidget>

#include <cstdint>
#include <functional>

class QMenu;

namespace trion::actions {
class ActionRegistry;
}
namespace trion::ui {
enum class Edition : std::uint8_t;
}

namespace trion::ribbon {

// Lựa chọn gần nhất của split: {"<id điều khiển>": "<khóa mục>"} (MenuItem::key, "CIRCLE D").
inline const settings::Setting<QJsonObject> kSplitChoice{"ribbon/split", {}};

// Trạng thái danh sách của một source; Ready mà model không có dòng là rỗng.
enum class DataState : std::uint8_t { Ready, Loading, Error };

class RibbonControls : public QObject {
    Q_OBJECT
public:
    // registry, store sống lâu hơn RibbonControls; RibbonControls sống lâu hơn widget nó dựng.
    explicit RibbonControls(actions::ActionRegistry& registry, settings::Store& store = settings::Store::instance(),
                            QObject* parent = nullptr);

    QWidget* create(const Control& c, QWidget* parent = nullptr);
    // Thân panel: hàng/cột như JSON, cột canh trên. Khung panel (tên, mở rộng, thu gọn) là của UIA-005.
    QWidget* buildPanel(const Panel& p, QWidget* parent = nullptr);

    // Dữ liệu theo source cho DropDown, Gallery, danh sách của Split: áp cho điều khiển đã dựng và dựng sau.
    // ponytail: chỗ nối tạm cho UIA-104 (dropdown kiểu, layer), INT-… (block, view của bản vẽ); model không sở hữu,
    // phải sống lâu hơn điều khiển dùng nó. Dòng: DisplayRole (tên), DecorationRole (ảnh mẫu, biểu tượng), ToolTipRole.
    void setModel(const QString& source, QAbstractItemModel* model);
    void setCurrent(const QString& source, int row); // mục đang chọn (giá trị hiện hành); -1 không có
    void setDataState(const QString& source, DataState state);
    // Giá trị sysvar cho Number, Slider; không phát tín hiệu. ponytail: chỗ nối tạm thay sự kiện sysvar của INT-005
    // (Sysvars của ActionRegistry chỉ có số nguyên): bước integration gọi khi sysvar đổi.
    void setValue(const QString& sysvar, double value);

    QAbstractItemModel* model(const QString& source) const { return models_.value(source); }
    int current(const QString& source) const { return current_.value(source, -1); }
    DataState dataState(const QString& source) const { return states_.value(source, DataState::Ready); }
    double value(const QString& sysvar) const { return values_.value(sysvar); }

    // Bản cần của điều khiển: need riêng ("VP", "PRO"), không có thì theo lệnh; khóa ở phiên bản hiện tại.
    ui::Edition need(const Control& c) const;
    static bool locked(ui::Edition need);

    actions::ActionRegistry& registry() const { return registry_; }

    // Chạy lệnh: không tham số → QAction của registry (cổng phiên bản, sink); có tham số → cổng rồi macroRequested.
    void run(const QString& cmd, const QString& args, const QString& name);
    // Đặt sysvar (Number, Slider): cổng theo need rồi macroRequested("'SYSVAR giá-trị").
    void commit(const Control& c, double value);
    // Mở trang nâng cấp cho điều khiển khóa không có QAction (dropdown, gallery).
    void upgrade(const Control& c);

    QString choice(const QString& id) const; // khóa mục đã chọn gần nhất của split; rỗng nếu chưa
    void remember(const QString& id, const QString& key);

    // Tên biểu tượng (UI0-103) và mô tả của lệnh trong danh mục (tạm thay INT-001); rỗng nếu không có. describe: mức 2
    // của tooltip (UIA-005), rỗng khi trùng name ("Đoạn thẳng" → "Vẽ đoạn thẳng").
    static QString commandIcon(const QString& cmd);
    static QString describe(const QString& cmd, const QString& name);

signals:
    // Chuỗi lệnh không có QAction riêng: "^C^C_CIRCLE D", "'_ZOOM W", "'TEXTSIZE 2.5".
    void macroRequested(const QString& macro);
    void picked(const QString& source, int row); // chọn mục của dropdown, gallery, danh sách split theo source
    void moreRequested(const QString& source);   // nút "Mở toàn bộ gallery"
    // Cho các điều khiển đã dựng (không cần nối từ ngoài).
    void sourceChanged(const QString& source);
    void valueChanged(const QString& sysvar, double value);

private:
    actions::ActionRegistry& registry_;
    settings::Store& store_;
    QHash<QString, QPointer<QAbstractItemModel>> models_;
    QHash<QString, int> current_;
    QHash<QString, DataState> states_;
    QHash<QString, double> values_;
};

// Nút ribbon: Button, Split, Toggle.
class RibbonButton : public QAbstractButton {
    Q_OBJECT
public:
    RibbonButton(const Control& c, RibbonControls& owner, QWidget* parent = nullptr);

    const Control& control() const { return c_; }
    QMenu* menu(); // Split: danh sách (dựng khi mở lần đầu); nullptr nếu không phải split
    void showMenu();
    bool isLocked() const { return locked_; }
    // Split: mục phần chính sẽ chạy (lựa chọn gần nhất, chưa có thì mục đầu trùng lệnh của nút); {} nếu không có.
    MenuItem mainItem() const;
    // Split: vùng mũi tên (nửa dưới nút lớn, vùng ▾ cuối nút nhỏ); cả nút nếu menuOnly.
    QRect arrowZone() const;
    // Panel hẹp (UIA-005, .rb-p.is-compact): nút nhỏ chỉ còn biểu tượng; chữ vẫn là tên cho trình đọc màn hình và
    // tooltip. Nút lớn, nút chỉ biểu tượng không đổi.
    void setCompact(bool compact);
    bool isCompact() const { return compact_; }
    QSize sizeFor(bool compact) const; // cỡ ở dạng đó: khung ribbon tính bề rộng panel trước khi đổi dạng

    QSize sizeHint() const override { return sizeFor(compact_); }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void changeEvent(QEvent* e) override;
    void nextCheckState() override; // Toggle: trạng thái chỉ theo QAction

private:
    void activate();
    void fillMenu();
    void sync(); // chữ, khóa, tooltip, trợ năng theo ngôn ngữ, phiên bản, lựa chọn gần nhất
    QRect mainZone() const;
    ControlSize drawSize(bool compact) const; // nút nhỏ ở dạng gọn vẽ như nút chỉ biểu tượng

    Control c_;
    RibbonControls& owner_;
    QAction* action_ = nullptr; // QAction của lệnh trong registry
    QMenu* menu_ = nullptr;
    QString choice_; // Split: khóa mục chọn gần nhất (đọc từ Store khi dựng)
    bool locked_ = false;
    bool menuOpen_ = false;
    bool compact_ = false;
};

// Trang các tab: trang của tab i dựng (build(i)) khi mở lần đầu, giữ lại cho lần sau.
class RibbonPages : public QStackedWidget {
    Q_OBJECT
public:
    RibbonPages(int count, std::function<QWidget*(int)> build, QWidget* parent = nullptr);
    void setCurrentTab(int index);
    int currentTab() const { return current_; }
    bool isBuilt(int index) const { return pages_.value(index) != nullptr; }

private:
    std::function<QWidget*(int)> build_;
    QList<QWidget*> pages_;
    int current_ = -1;
};

} // namespace trion::ribbon
