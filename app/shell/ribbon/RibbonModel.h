#pragma once
// UIA-102 · Mô hình ribbon (07.1 §7.1): tab → panel → hàng/cột → điều khiển, đọc từ JSON. Khóa của tệp:
// resources/schema.json. Nội dung 8 tab của workspace Soạn thảo 2D (resources/<tab>.json) và 5 tab của ribbon Viewer
// UI-M01 (resources/viewer.json, một tệp nhiều tab; UIA-017) sinh từ prototype bằng ribbon_gen.mjs; UIA-006 hoàn thiện
// theo 07.1.
//
// Điều khiển trỏ tới hành động của ActionRegistry (UI0-015) bằng cmd: tên lệnh (danh mục tạm thay INT-001) hoặc id
// hành động gắn sysvar ("LWDISPLAY", "PICKSTYLE"). Ribbon chỉ tham chiếu tên biểu tượng (IconProvider), không vẽ.
// Cờ phiên bản: need của điều khiển, không có thì theo lệnh (ActionRegistry::need). Ưu tiên co của panel cho UIA-005:
// shrink (prototype data-prio), số lớn thu thành nút trước.
//
// Nhãn lưu bằng tiếng Việt gốc, dịch lúc hiển thị bằng text(): nhãn trùng nhãn lệnh dùng bản dịch có sẵn (ngữ cảnh
// "actions"), còn lại ngữ cảnh "ribbon" (ribbon_strings.cpp sinh cùng JSON để lupdate thấy).
//
// Dùng (luồng UI; JSON đọc một lần, widget của tab dựng khi mở tab lần đầu: RibbonPages trong RibbonControls.h):
//   trion::ribbon::RibbonModel model;                       // 8 tab trong resource :/trion/ribbon/
//   for (const Tab& t : model.tabs()) tabBar->addTab(RibbonModel::text(t.name));
//   searchResult->setDetail(model.locate(u"CIRCLE"_s));      // "Trang chủ › Vẽ" (ô Tìm lệnh, UIA-002)

#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>
#include <vector>

namespace trion::ribbon {

enum class ControlType : std::uint8_t {
    Button,   // nút lớn, nhỏ hoặc chỉ biểu tượng (size)
    Split,    // bấm phần chính chạy lựa chọn gần nhất, mũi tên mở danh sách (menu hoặc source)
    Toggle,   // bật/tắt theo QAction checkable của ActionRegistry (sysvar)
    DropDown, // danh sách từ model của source
    Gallery,  // lưới mẫu từ model của source, ảo hóa
    Number,   // ô số UI-C09 gắn sysvar
    Slider,   // thanh trượt gắn sysvar
};
enum class ControlSize : std::uint8_t { Large, Small, Icon };

// Mục danh sách của split và của panel mở rộng.
struct MenuItem {
    QString label{}; // nhãn tiếng Việt gốc
    QString cmd{};   // id hành động ActionRegistry
    QString args{};  // tham số lựa chọn gửi sau tên lệnh ("D" của CIRCLE Tâm, Đường kính)
    QString icon{};  // rỗng: biểu tượng của lệnh
    bool separator = false;
    QString key() const { return args.isEmpty() ? cmd : cmd + u' ' + args; } // khóa nhớ lựa chọn
};

struct Control {
    ControlType type = ControlType::Button;
    ControlSize size = ControlSize::Small; // Button, Split, Toggle
    QString id{};                          // khóa trong tab (nhớ lựa chọn, objectName); mặc định cmd
    QString label{};                       // nhãn tiếng Việt gốc; '\n' ngắt dòng ở nút lớn
    QString icon{};
    QString cmd{};
    QString args{};
    QString need{};                // "VP" | "PRO"; rỗng: theo lệnh
    QString source{};              // Split (danh sách theo bản vẽ), DropDown, Gallery: id nguồn của model
    QString sysvar{};              // Number, Slider
    std::optional<double> above{}; // Number: giá trị phải lớn hơn
    double min = 0, max = 100;     // Slider
    int width = 0;                 // px logic ở 100%: DropDown, Gallery, ô của Number, Slider; 0 = theo nội dung
    bool menuOnly = false;         // Split: cả nút mở danh sách
    std::vector<MenuItem> menu{};  // Split
};

// Phần tử của thân panel: điều khiển, hoặc hàng/cột chứa phần tử con.
struct Item {
    enum class Kind : std::uint8_t { Control, Row, Column } kind = Kind::Control;
    Control control{};
    std::vector<Item> items{};
};

struct Panel {
    QString id, name, icon;
    int shrink = 1; // ưu tiên co (UIA-005): 1 co sau cùng; số lớn thu thành nút thả xuống trước
    std::vector<Item> items;
    std::vector<MenuItem> more; // panel mở rộng (UIA-005)
};

struct Tab {
    QString id, name;
    std::vector<Panel> panels;
    QString need{}; // "VP" | "PRO": cả tab cần bản đó (huy hiệu khóa trên tab, UIA-017); rỗng: không
};

class RibbonModel {
public:
    // Đọc các tệp JSON (đường dẫn hoặc resource ":/…") theo thứ tự: tệp là một tab, hoặc {"tabs": [tab…]}. Tệp, mục
    // sai khóa hay sai kiểu: ghi vào errors() rồi bỏ qua phần đó (lint chặn ở CI), không dừng.
    explicit RibbonModel(const QStringList& files = defaultFiles());

    static QStringList defaultFiles();  // 8 tab của workspace Soạn thảo 2D
    static QStringList viewerFiles();   // 5 tab của ribbon Viewer UI-M01 (hồ sơ Viewer, UIA-017)
    static QStringList resourceFiles(); // mọi tệp tab trong :/trion/ribbon/ (lint)

    const std::vector<Tab>& tabs() const { return tabs_; }
    const QStringList& errors() const { return errors_; }

    // "Tab › Panel" đã dịch của nơi đầu tiên có lệnh (điều khiển, mục split, panel mở rộng); rỗng nếu không có.
    QString locate(const QString& cmd) const;

    // Nhãn đã dịch theo ngôn ngữ đang dùng, và ngữ cảnh dịch của nó ("actions" hoặc "ribbon").
    static QString text(const QString& label);
    static const char* context(const QString& label);
    // Nhãn trên một dòng (tooltip, menu, trình đọc màn hình): '\n' thành dấu cách.
    static QString line(const QString& label) { return text(label).replace(u'\n', u' '); }

private:
    std::vector<Tab> tabs_;
    QStringList errors_;
};

} // namespace trion::ribbon
