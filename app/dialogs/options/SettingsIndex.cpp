#include "app/dialogs/options/SettingsIndex.h"

#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/text/Fold.h"

#include <QString>
#include <QStringList>

#include <iterator>
#include <vector>

namespace trion::options {

namespace {

using ui::Edition;

// OPT_TABS của prototype: tên, biểu tượng, bản cần ('V' → None, 'PRO' → Pro).
constexpr TabInfo kTabs[kTabCount] = {
    {"files", QT_TRANSLATE_NOOP("options", "Tệp"), "folder", Edition::Pro},
    {"display", QT_TRANSLATE_NOOP("options", "Hiển thị"), "monitor", Edition::None},
    {"opensave", QT_TRANSLATE_NOOP("options", "Mở & Lưu"), "save", Edition::Pro},
    {"plot", QT_TRANSLATE_NOOP("options", "In & Xuất"), "print", Edition::Pro},
    {"system", QT_TRANSLATE_NOOP("options", "Hệ thống"), "cpu", Edition::None},
    {"cmdline", QT_TRANSLATE_NOOP("options", "Dòng lệnh"), "keyboard", Edition::None},
    {"user", QT_TRANSLATE_NOOP("options", "Người dùng"), "user", Edition::Pro},
    {"drafting", QT_TRANSLATE_NOOP("options", "Vẽ"), "line", Edition::Pro},
    {"3d", QT_TRANSLATE_NOOP("options", "Mô hình 3D"), "vstyle", Edition::Pro},
    {"selection", QT_TRANSLATE_NOOP("options", "Chọn"), "select", Edition::Pro},
    {"profiles", QT_TRANSLATE_NOOP("options", "Hồ sơ"), "license", Edition::None},
    {"account", QT_TRANSLATE_NOOP("options", "Tài khoản"), "key", Edition::None},
};

// Mọi hàng thiết lập. vi, keywords, tab của 19 mục đầu chép từ TC.SETTINGS_INDEX (options_test so khớp); en, sysvars
// thêm cho tìm tiếng Anh và tên biến. Hàng mới (UID-020…026): thêm mục, id duy nhất.
// clang-format off
constexpr SettingEntry kEntries[] = {
    {"theme", Tab::Display, "Chủ đề tối, sáng", "theme dark light giao dien", "Color theme, dark, light", "COLORTHEME"},
    {"lite", Tab::Display, "Giao diện nhẹ", "lite may yeu hieu ung", "Lite interface, low-end machine, effects, shadows", ""},
    {"ribbon-icons", Tab::Display, "Cỡ biểu tượng ribbon", "icon", "Ribbon icon size", ""},
    {"tooltips", Tab::Display, "Tooltip mở rộng", "tooltip", "Extended tooltips, shortcut keys in tooltips, delay", "TOOLTIPS"},
    {"canvas-colors", Tab::Display, "Màu vùng vẽ, chữ thập, marker", "mau nen crosshair", "Drawing window colors, background, crosshair, snap marker", ""},
    {"crosshair-size", Tab::Display, "Cỡ chữ thập", "crosshair", "Crosshair size", "CURSORSIZE"},
    {"cmdline-font", Tab::Display, "Font dòng lệnh", "command line font", "Command line font", ""},
    {"graphics", Tab::System, "Thiết lập đồ họa", "gpu do hoa", "Graphics performance, hardware acceleration, GRAPHICSCONFIG", ""},
    {"update-channel", Tab::System, "Kênh cập nhật", "update beta", "Update channel, stable, check for updates", ""},
    {"file-associations", Tab::System, "Liên kết tệp .dwg", "file association mac dinh", "File associations, .dwg .dxf .dwt, default app", ""},
    {"privacy", Tab::System, "Quyền riêng tư, telemetry", "privacy", "Privacy, telemetry, usage data", ""},
    {"language", Tab::System, "Ngôn ngữ giao diện, prompt", "language english", "Interface language, prompt language, Vietnamese", ""},
    {"hidden-messages", Tab::System, "Khôi phục hộp thoại “Không hỏi lại”", "khong hoi lai", "Hidden messages, don't ask again, restore dialogs", ""},
    {"profiles", Tab::Profiles, "Hồ sơ người dùng, xuất .arg", "profile", "User profiles, export, import .arg", "CPROFILE"},
    {"autosave", Tab::OpenSave, "Tự lưu (autosave)", "autosave tu luu bak", "Automatic save, minutes between saves, backup copy .bak", "SAVETIME ISAVEBAK"},
    {"support-paths", Tab::Files, "Đường dẫn tìm tệp hỗ trợ", "support path", "Support file search path", ""},
    {"font-mapping", Tab::Files, "Bảng ánh xạ font", "font mapping", "Font mapping file", "FONTMAP"},
    {"cmdline-history", Tab::CommandLine, "Số dòng lịch sử dòng lệnh", "command line history", "Command line history, prompt lines", "CLIPROMPTLINES"},
    {"cmdline-vietnamese", Tab::CommandLine, "Tìm lệnh theo tiếng Việt", "vietnamese keyword", "Search commands by Vietnamese keyword, INPUTSEARCHOPTIONS", ""},
};
// clang-format on

// Khóa tìm đã gấp dấu của từng mục, cùng thứ tự kEntries.
struct Folded {
    QString text;        // nhãn, từ khóa, tiếng Anh, sysvar
    QStringList sysvars; // để xếp hạng trùng hẳn tên biến
};

const std::vector<Folded>& folded()
{
    static const std::vector<Folded> all = [] {
        std::vector<Folded> out;
        out.reserve(std::size(kEntries));
        for (const SettingEntry& e : kEntries) {
            const QString vars = QString::fromLatin1(e.sysvars);
            const QStringList keys{QString::fromUtf8(e.vi), QString::fromUtf8(e.keywords), QString::fromUtf8(e.en),
                                   vars};
            out.push_back(
                {text::foldForSearch(keys.join(u' ')), text::foldForSearch(vars).split(u' ', Qt::SkipEmptyParts)});
        }
        return out;
    }();
    return all;
}

// word có ở đầu một từ của hay (đầu chuỗi hoặc sau ký tự không phải chữ, số).
bool atWordStart(const QString& hay, const QString& word)
{
    for (qsizetype i = hay.indexOf(word); i >= 0; i = hay.indexOf(word, i + 1))
        if (i == 0 || !hay.at(i - 1).isLetterOrNumber())
            return true;
    return false;
}

} // namespace

const TabInfo& tabInfo(Tab tab)
{
    return kTabs[int(tab)];
}

std::optional<Tab> tabFromId(QStringView id)
{
    for (int i = 0; i < kTabCount; ++i)
        if (id == QLatin1StringView(kTabs[i].id))
            return Tab(i);
    return std::nullopt;
}

std::span<const SettingEntry> SettingsIndex::entries()
{
    return kEntries;
}

const SettingEntry* SettingsIndex::find(QStringView id)
{
    for (const SettingEntry& e : kEntries)
        if (id == QLatin1StringView(e.id))
            return &e;
    return nullptr;
}

const SettingEntry* SettingsIndex::search(QStringView text)
{
    const QString q = text::foldForSearch(text).simplified();
    if (q.isEmpty())
        return nullptr;
    const QStringList words = q.split(u' ');
    const std::vector<Folded>& keys = folded();
    const SettingEntry* best = nullptr;
    int bestRank = 3;
    for (std::size_t i = 0; i < keys.size() && bestRank > 0; ++i) {
        bool all = true;
        bool starts = true;
        for (const QString& w : words) {
            if (!keys[i].text.contains(w)) {
                all = false;
                break;
            }
            starts = starts && atWordStart(keys[i].text, w);
        }
        if (!all)
            continue;
        const int rank = keys[i].sysvars.contains(q) ? 0 : starts ? 1 : 2;
        if (rank < bestRank) {
            best = &kEntries[i];
            bestRank = rank;
        }
    }
    return best;
}

} // namespace trion::options
