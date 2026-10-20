#pragma once
// Bảng phím tắt một nguồn (UI0-114, README task §4.2).
// Mặc định: keymap.json nhúng trong resource, sinh từ KDEF/RESERVED/MACSYS/CMDKEYS của prototype bằng
// keymap_gen.mjs (không sửa tay). Ghi đè của người dùng: settings::Store, khóa "keymap/overrides" (NT-010).
//   - ActionRegistry (UI0-015): Action::bindings cho QAction; actionFor() để định tuyến phím, theo
//     Action::scope.
//   - Hộp Phím tắt (UI-K06): assign / unassign / reset / resetAll; "Gán lại" = unassign(chủ cũ) rồi assign.
//   - Tooltip, menu, hộp Phím tắt: nghe changed(), hiển thị bằng keyLabel().
//
// Tổ hợp viết dạng chuẩn của prototype: "Ctrl+Alt+Shift+<phím>", phím là chữ hoa, chữ số, F1…F12, Delete,
// Tab, PgUp, PgDn… Chuỗi nhiều phím nối bằng " · ". "Ctrl" là Ctrl trên Windows/Linux; trên macOS phím hệ
// thống (Mới, Mở, Lưu, In, Hoàn tác, Sao chép, Dán…) thành ⌘, phím CAD giữ ⌃ (07.1 §13.6).
// Tên, nhóm, bên giữ phím là chuỗi tiếng Việt gốc; hiển thị qua QCoreApplication::translate("keymap", …).

#include "core/base/settings/Settings.h"

#include <QHash>
#include <QKeyCombination>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

namespace trion::keymap {

enum class Platform { Windows, Linux, Mac };
Platform currentPlatform();

// Nhãn hiển thị của chuỗi phím ("F5 · Ctrl+E"), bản C++ của TC.keyLabel. Windows/Linux: giữ nguyên.
// macOS: bảng MACSYS (Ctrl+Y → ⇧⌘Z, Delete → ⌫…), phím CMDKEYS → ⌘, Ctrl còn lại → ⌃, Alt → ⌥, Shift → ⇧.
QString keyLabel(const QString& keys, Platform p = currentPlatform());

// Một tổ hợp → phím Qt thật trên nền tảng p, đúng như keyLabel hiển thị. Trên macOS Qt::ControlModifier
// là ⌘ và Qt::MetaModifier là ⌃; p = Mac nhận cả nhãn macOS ("⌃F", phím thay thế của KDEF).
// Không đọc được thì key() == Qt::Key_unknown.
QKeyCombination keyCombination(const QString& combo, Platform p = currentPlatform());

enum class Scope { Global, Viewport }; // Toàn cục / Vùng vẽ (phím chỉ chạy khi focus ở vùng vẽ)

struct Action {
    QString id;              // "osnap"; lệnh ngoài KDEF được gán phím thì id là tên lệnh
    QString group;           // "Hỗ trợ vẽ"; lệnh ngoài KDEF: "Lệnh khác"
    QString name;            // "Bật/tắt bắt điểm"
    QString command;         // "OSNAP"; "—" nếu hành động không chạy lệnh (tab, layout kế / trước)
    QStringList keys;        // phím đang dùng: mặc định hoặc ghi đè
    QStringList defaultKeys; // phím mặc định (rỗng với lệnh ngoài KDEF)
    Scope scope = Scope::Global;
    QString macAlt;          // nhãn macOS của tổ hợp thay thế phím F (laptop phải bấm Fn), ví dụ "⌃F"
    QList<QKeyCombination> bindings; // phím Qt trên nền tảng của Keymap mà hành động này thật sự giữ
};

// Lý do assign() từ chối; owner là tên chủ cũ để báo cho người dùng.
struct Rejection {
    enum Reason { None, Reserved, Taken, BareKey, Invalid } reason = None;
    QString owner;   // Reserved: bên giữ ("Windows", "bộ gõ (đổi kiểu gõ tiếng Việt)"…); Taken: tên hành động
    QString ownerId; // Taken: id hành động đang giữ phím
    explicit operator bool() const { return reason != None; }
};

class Keymap : public QObject {
    Q_OBJECT
public:
    // store phải sống lâu hơn Keymap. platform chỉ đổi trong test (nhãn và phím macOS chạy trên Linux).
    explicit Keymap(settings::Store& store = settings::Store::instance(),
                    Platform platform = currentPlatform(), QObject* parent = nullptr);

    Platform platform() const { return platform_; }
    const QList<Action>& actions() const { return actions_; } // thứ tự KDEF, rồi lệnh khác theo id
    // Con trỏ trả về hết hiệu lực khi keymap đổi (sau changed()).
    const Action* action(const QString& id) const;            // nullptr nếu không có
    // Tra phím → hành động, O(1), không cấp phát; nullptr nếu phím chưa gán. Chấp nhận nguyên QKeyEvent:
    // Shift+Tab đến dạng Key_Backtab, phím số bàn phím phụ mang KeypadModifier.
    const Action* actionFor(QKeyCombination key) const;

    // Thêm combo vào phím của id, lưu ở dạng chuẩn ("Shift+Ctrl+K" → "Ctrl+Shift+K"); id ngoài KDEF là tên
    // lệnh (nhóm "Lệnh khác"). Từ chối (không đổi gì) nếu combo là phím dành riêng của hệ điều hành
    // hoặc bộ gõ, phím đơn không kèm Ctrl/Alt/F (dành cho gõ lệnh), không đọc được, hoặc trùng phím của
    // hành động khác trên Windows/Linux hay macOS (Ctrl+Shift+Z trùng Ctrl+Y vì cùng là ⇧⌘Z trên macOS).
    Rejection assign(const QString& id, const QString& combo);
    void unassign(const QString& id, const QString& combo);
    void reset(const QString& id); // "Đặt lại": về phím mặc định
    void resetAll();

signals:
    void changed(); // phím đổi, kể cả khi "keymap/overrides" trong Store đổi từ nơi khác

private:
    void reload();
    void write(const QString& id, const QStringList& keys);

    settings::Store& store_;
    Platform platform_;
    QList<Action> actions_;
    QHash<int, qsizetype> lookup_; // QKeyCombination::toCombined() → chỉ số trong actions_
};

} // namespace trion::keymap
