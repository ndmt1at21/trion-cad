#pragma once
// Màu ngữ nghĩa 07.1 §13.2 (UI0-002): tên → token của tokens.css, ghi đè của người dùng (UI-D10) lưu qua Store.
//
// Đọc theo chế độ hiện tại: ThemeManager::instance().color(Sem::GripHot), .fade(Sem::FadeXref)
// UI-D10: auto& s = ThemeManager::instance().semantic();
//   s.setColor(Sem::GripHot, c); s.reset(Sem::GripHot); s.resetAll(); s.isOverridden(…); s.defaultColor(…)
//   Mọi thay đổi ghi vào Store; ThemeManager nạp lại và phát changed().
//
// Tên vùng vẽ (canvas = true) không đổi theo chủ đề giao diện (07.1 §13.2: nền vùng vẽ đặt riêng, mặc định tối):
// luôn lấy giá trị :root của tokens.css, bỏ qua ghi đè của khối Sáng (grip.*, dyn.tooltip.*, paper.shadow).
// ponytail: một bộ cho mọi ngữ cảnh; màu theo ngữ cảnh Model/Layout/Block Editor và theo nền vùng vẽ sáng
// (UI-D10 "Màu…") thêm khi làm UI-D10.

#include "app/theme/generated/Tokens.h"
#include "core/base/settings/Settings.h"

#include <QColor>
#include <QJsonObject>

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace trion::theme {

enum class Sem : std::uint8_t {
    SelectionWindow, SelectionWindowFill, SelectionCrossing, SelectionCrossingFill,
    HighlightHover, HighlightSelected, HighlightResult,
    GripCold, GripHover, GripHot, GripBorder,
    SnapMarker, TrackingVector,
    DynTooltipBg, DynTooltipText, DynTooltipBorder,
    FadeXref, FadeLocked, // hệ số làm nhạt 0..1 (token số), không phải màu
    PaperBg, PaperPrintable, PaperShadow,
    StatusOn, StatusOff,
    EditionViewer, EditionViewerText, EditionViewerPro, EditionViewerProText, EditionPro, EditionProText,
    EditionTrial, EditionTrialText, EditionEducation, EditionEducationText, EditionEnterprise, EditionEnterpriseText,
    CompareOnlyA, CompareOnlyB, CompareCommon,
};

struct SemEntry {
    Sem sem;
    std::string_view name; // tên 07.1 §13.2, cũng là khóa lưu ghi đè: "grip.hot"
    tok::Kind kind;        // Color, hoặc Num cho fade.*
    std::uint16_t token;   // chỉ số trong loại, như tok::ColorId::i
    bool canvas;           // vẽ trên vùng vẽ: không đổi theo chủ đề giao diện
};

std::span<const SemEntry> semEntries(); // đủ mọi tên, theo thứ tự enum
const SemEntry& semEntry(Sem s);
std::optional<Sem> semFromName(std::string_view name);

QColor toQColor(tok::Rgba c);

// Ghi đè: {"grip.hot": "#AARRGGBB", "fade.xref": 0.6}; giá trị hỏng trong tệp bị bỏ qua (dùng token).
inline const settings::Setting<QJsonObject> kSemanticOverrides{"ui/semanticColors", {}};

class SemanticColors {
public:
    explicit SemanticColors(settings::Store& store) : store_(store) { reload(); }

    // Giá trị hiệu lực: ghi đè nếu có, không thì token. ui = chế độ giao diện (tên vùng vẽ bỏ qua nó).
    QColor color(Sem s, tok::Mode ui) const;
    double fade(Sem s) const;
    // Giá trị tokens.css: "Đặt lại" trả về đây.
    QColor defaultColor(Sem s, tok::Mode ui) const;
    double defaultFade(Sem s) const;

    bool isOverridden(Sem s) const;
    void setColor(Sem s, const QColor& c);
    void setFade(Sem s, double f); // kẹp vào 0..1
    void reset(Sem s);
    void resetAll()
    {
        store_.reset(kSemanticOverrides);
        reload();
    }

    void reload() { overrides_ = store_.get(kSemanticOverrides); }

private:
    void put(Sem s, const QJsonValue& v); // Undefined = xóa

    settings::Store& store_;
    QJsonObject overrides_;
};

} // namespace trion::theme
