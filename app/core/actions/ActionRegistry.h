#pragma once
// UI0-015 · Hành động UI → chuỗi lệnh, định tuyến phím tắt (07.1 §1 mục 1, §11; README task §4.2; DoD-U 5).
//
// Một QAction cho mỗi lệnh: nhãn tiếng Việt (dịch lại khi đổi ngôn ngữ), biểu tượng (IconProvider), tooltip 2 mức có
// tên lệnh tiếng Anh và phím (RichTooltip, keyId của Keymap), phím tắt từ Keymap (UI0-114, đổi theo Keymap). Ribbon,
// menu, toolbar, thanh trạng thái (UIA-003/005/102/105/112) dùng chung các QAction này, không tự gọi lệnh.
//
// Kích hoạt (bấm, phím, mục menu) → một chuỗi lệnh tới chỗ nối INT-002 (setSink). Cú pháp macro menu của AutoCAD:
// "^C^C" hủy lệnh đang chạy (cả lệnh lồng), "'" chạy trong suốt (không hủy), "_" tên lệnh toàn cục tiếng Anh, khoảng
// trắng là Enter, hết chuỗi là Enter:
//   ^C^C_LINE      lệnh thường (Ctrl+S → ^C^C_QSAVE)
//   '_ZOOM         lệnh trong suốt
//   'ORTHOMODE 1   hành động gắn sysvar: đặt giá trị mới tính từ giá trị hiện tại, trong suốt (F8 khi đang LINE không
//                  ngắt lệnh, giống gõ 'ORTHOMODE; UIA-112)
//   '_CLEANSCREENON / '_CLEANSCREENOFF   hành động CLEANSCREENON (Ctrl+0) đổi vòng theo CLEANSCREENSTATE như macro
//                  CUI của AutoCAD (UIA-001)
// Hành động gắn sysvar (toggle F3 OSNAP, F8 ORTHO…; F5 mặt isometric, Ctrl+I kiểu tọa độ đổi vòng): trạng thái chọn
// chỉ theo sysvar (Sysvars::changed, đồng bộ, không timer, không polling). Bấm chỉ gửi lệnh; nút đổi khi sysvar đổi.
//
// Định tuyến phím theo Keymap::Scope, bằng bảng phím tắt của Qt:
//   - Toàn cục: Qt::ApplicationShortcut trên cửa sổ chính (install). Chạy cả khi focus ở ô văn bản, trừ phím ô đó tự
//     dùng (ShortcutOverride: Ctrl+C, Ctrl+Z… của QLineEdit) và khi hộp thoại modal đang mở.
//   - Vùng vẽ: Qt::WidgetShortcut trên widget vùng vẽ (addViewport). Chỉ chạy khi chính vùng vẽ có focus: ở ô văn bản
//     (dòng lệnh, ô DYN, palette) Ctrl+C là chép chữ, Delete là xóa chữ; ở vùng vẽ Ctrl+C là COPYCLIP.
//
// Id (QAction::objectName): tên lệnh ("LINE", "QSAVE"); hành động gắn sysvar: id của Keymap ("osnap", "ortho"…) hoặc
// tên sysvar ("LWDISPLAY"). Hành động "—" của Keymap (Tab tệp, Layout kế/trước) không phải lệnh: khung cửa sổ tự xử lý.
//
// Dùng (main, luồng UI, sau ThemeManager, IconProvider, Translator, Keymap):
//   trion::actions::Sysvars sysvars;                       // ponytail: tạm thay INT-005
//   trion::actions::ActionRegistry actions(keymap, sysvars);
//   actions.setSink([&](const QString& macro) { … });      // ponytail: tạm thay INT-002
//   actions.install(&mainWindow);
//   actions.addViewport(viewport);                         // mỗi widget vùng vẽ
//   ribbonButton->setDefaultAction(actions.action(u"LINE"_s));
//   menu->addAction(actions.action(u"QSAVE"_s));
//   statusButton->setDefaultAction(actions.action(u"ortho"_s)); // checkable, theo ORTHOMODE

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include <cstdint>
#include <functional>
#include <span>
#include <utility>
#include <vector>

class QAction;
class QWidget;

namespace trion::keymap {
class Keymap;
}

namespace trion::ui {
enum class Edition : std::uint8_t; // app/widgets/tooltip/RichTooltip.h
}

namespace trion::actions {

// ponytail: metadata lệnh tạm thay INT-101/INT-001 (cmd/registry/CommandMetadata.h): danh mục RAW của prototype
// (design/prototype/js/cmd.js), sinh vào commands.cpp bằng actions_gen.mjs. Chưa có alias, từ khóa, cờ trong suốt
// (bảng tạm trong ActionRegistry.cpp); thay khi có registry lệnh.
struct CommandInfo {
    const char* name;  // "LINE"
    const char* need;  // bản tối thiểu: "V" Viewer, "VP" Viewer Pro, "PRO"
    const char* label; // nhãn tiếng Việt gốc; hiển thị qua QCoreApplication::translate("actions", label)
    const char* icon;  // tên biểu tượng (IconProvider)
};
std::span<const CommandInfo> commands(); // thứ tự của prototype

// ponytail: nguồn sysvar tạm thay INT-005 (INT-107 SysvarRegistry: kiểu, phạm vi lưu, miền giá trị, sự kiện gom lô
// kèm giá trị cũ, mới, nguồn). Chỉ số nguyên trong bộ nhớ, chưa có giá trị mặc định (biến chưa đặt đọc là 0).
class Sysvars : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    int get(const QString& name) const { return values_.value(name); }
    void set(const QString& name, int value); // đổi thì phát changed ngay (đồng bộ)

signals:
    void changed(const QString& name, int value);

private:
    QHash<QString, int> values_;
};

class ActionRegistry : public QObject {
    Q_OBJECT
public:
    // keymap, sysvars sống lâu hơn registry. Cần IconProvider; nhãn dịch lại theo Translator::instance() nếu có.
    ActionRegistry(keymap::Keymap& keymap, Sysvars& sysvars, QObject* parent = nullptr);
    ~ActionRegistry() override;

    QAction* action(const QString& id) const;                // nullptr nếu không có
    const QList<QAction*>& actions() const { return list_; } // lệnh theo danh mục, rồi lệnh và sysvar của Keymap
    QString macro(const QString& id) const;    // chuỗi lệnh nếu kích hoạt bây giờ; rỗng nếu không có
    ui::Edition need(const QString& id) const; // bản cần để dùng; UI0-011 so với giấy phép

    // ponytail: chỗ nối tạm thay INT-002 (cmd/Cmd.h; InputPipeline::submit(Token, Source::Menu)): nhận chuỗi lệnh,
    // chưa đặt thì bỏ. Không giả kết quả: lệnh chạy và sysvar đổi là việc của lõi.
    void setSink(std::function<void(const QString& macro)> sink) { sink_ = std::move(sink); }
    // Cổng phiên bản (UI0-011): gọi trước khi gửi; trả false thì không gửi (cổng tự mở trang nâng cấp).
    void setGate(std::function<bool(const QString& id)> gate) { gate_ = std::move(gate); }

    void install(QWidget* window);       // phím Toàn cục: cửa sổ chính
    void addViewport(QWidget* viewport); // phím Vùng vẽ: widget nhận focus của vùng vẽ, gọi cho từng viewport

private:
    struct Entry;
    const Entry* find(const QString& id) const;
    QString macro(const Entry& e) const;
    void activate(Entry& e);
    void applyKeys(); // phím theo Keymap
    void retranslate();
    void addTo(QWidget* w, bool viewport);

    keymap::Keymap& keymap_;
    Sysvars& sysvars_;
    std::vector<Entry> entries_;
    QHash<QString, qsizetype> index_; // id → chỉ số trong entries_
    QList<QAction*> list_;
    std::function<void(const QString&)> sink_;
    std::function<bool(const QString&)> gate_;
};

} // namespace trion::actions
