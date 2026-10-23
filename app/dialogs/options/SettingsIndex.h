#pragma once
// UID-019 · Chỉ mục thiết lập của hộp Tùy chọn (07.2 UI-D10: "gõ autosave sẽ nhảy tới đúng ô"; prototype
// TC.SETTINGS_INDEX, OPT_TABS trong design/prototype/js/screens2.js).
//
// 12 tab theo thứ tự, biểu tượng và bản cần của OPT_TABS (Viewer dùng Hiển thị, Hệ thống, Dòng lệnh, Hồ sơ, Tài khoản).
// Mỗi hàng thiết lập một mục: id ổn định, tab, khóa tìm tiếng Việt (nhãn và từ khóa của prototype), tiếng Anh, tên
// sysvar. Tìm không phân biệt dấu, hoa/thường (foldForSearch, NT-012): "autosave", "tự lưu", "tu luu", "SAVETIME" cùng
// ra mục "autosave" của tab Mở & Lưu.
//
// Một nguồn: bảng kEntries trong SettingsIndex.cpp, chứa đủ các mục của TC.SETTINGS_INDEX (options_test đọc screens2.js
// và so từng mục, cả bảng tab với OPT_TABS). Trang thêm hàng (UID-020…026) thì thêm mục vào bảng: OptionsPage::addRow
// nhận id của mục; OptionsDialog::indexIssues (CI) báo hàng không có mục, mục sai tab, mục không có hàng.
// ponytail: tên sysvar điền tay theo tài liệu AutoCAD tới khi có bảng sysvar INT-108 (cmd/data/sysvars.tsv).
#include <QStringView>

#include <cstdint>
#include <optional>
#include <span>

namespace trion::ui {
enum class Edition : std::uint8_t; // app/widgets/tooltip/RichTooltip.h
}

namespace trion::options {

// Thứ tự của OPT_TABS (danh sách tab dọc từ trên xuống).
enum class Tab : std::uint8_t {
    Files,
    Display,
    OpenSave,
    Plot,
    System,
    CommandLine,
    User,
    Drafting,
    Modeling,
    Selection,
    Profiles,
    Account,
};
inline constexpr int kTabCount = 12;

struct TabInfo {
    const char* id; // "opensave": tham số mở hộp (lệnh, liên kết từ màn hình khác), ASCII, ổn định
    const char* name; // "Mở & Lưu": nhãn gốc tiếng Việt; hiển thị qua QCoreApplication::translate("options", name)
    const char* icon; // tên biểu tượng (IconProvider)
    ui::Edition need; // None: Viewer dùng được; Pro: khóa ở Viewer, Viewer Pro (07.2 UI-D10 "Phát hành")
};
const TabInfo& tabInfo(Tab tab);
std::optional<Tab> tabFromId(QStringView id); // "opensave" → Tab::OpenSave

struct SettingEntry {
    const char* id;       // "autosave": OptionsPage::addRow, OptionsDialog::open(tab, row); ASCII, ổn định
    Tab tab;              // tab chứa hàng
    const char* vi;       // "Tự lưu (autosave)": nhãn tiếng Việt (khóa tìm)
    const char* keywords; // "autosave tu luu bak": từ khóa của prototype (tiếng Việt không dấu, tiếng Anh)
    const char* en;       // "Automatic save, backup file": khóa tìm tiếng Anh
    const char* sysvars;  // "SAVETIME ISAVEBAK": sysvar của hàng, cách bằng dấu cách; rỗng nếu không có
};

class SettingsIndex {
public:
    static std::span<const SettingEntry> entries();  // thứ tự của bảng
    static const SettingEntry* find(QStringView id); // nullptr nếu không có

    // Mục khớp tốt nhất với chuỗi gõ; nullptr nếu rỗng hay không mục nào khớp. Khớp: mọi từ của chuỗi (đã gấp dấu)
    // nằm trong khóa tìm của mục (nhãn, từ khóa, tiếng Anh, sysvar). Hạng: trùng hẳn tên sysvar, rồi mọi từ khớp
    // đầu một từ của khóa, rồi khớp giữa từ; cùng hạng thì theo thứ tự bảng. Khóa gấp sẵn một lần (lần gọi đầu),
    // mỗi phím chỉ gấp chuỗi gõ rồi quét: vài µs với vài trăm mục.
    static const SettingEntry* search(QStringView text);
};

} // namespace trion::options
