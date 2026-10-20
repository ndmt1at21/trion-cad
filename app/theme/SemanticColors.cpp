#include "app/theme/SemanticColors.h"

#include "core/base/assert.h"

#include <QJsonValue>

#include <algorithm>

namespace trion::theme {

namespace {

using enum Sem;
constexpr bool kCanvas = true, kUi = false;

constexpr SemEntry c(Sem s, std::string_view name, tok::ColorId id, bool canvas)
{
    return {s, name, tok::Kind::Color, id.i, canvas};
}
constexpr SemEntry n(Sem s, std::string_view name, tok::NumId id) { return {s, name, tok::Kind::Num, id.i, kCanvas}; }

// Bảng 07.1 §13.2; tên phụ (.fill, .border, .text) cho phần tử cần hai màu. Đối chiếu styleguide.html mục Màu.
constexpr SemEntry kEntries[] = {
    c(SelectionWindow, "selection.window", tok::c_sel_w, kCanvas),
    c(SelectionWindowFill, "selection.window.fill", tok::c_sel_wf, kCanvas),
    c(SelectionCrossing, "selection.crossing", tok::c_sel_c, kCanvas),
    c(SelectionCrossingFill, "selection.crossing.fill", tok::c_sel_cf, kCanvas),
    c(HighlightHover, "highlight.hover", tok::c_hl_hover, kCanvas),
    c(HighlightSelected, "highlight.selected", tok::c_hl_sel, kCanvas),
    c(HighlightResult, "highlight.result", tok::c_hl_res, kCanvas),
    c(GripCold, "grip.cold", tok::c_grip_cold, kCanvas),
    c(GripHover, "grip.hover", tok::c_grip_hover, kCanvas),
    c(GripHot, "grip.hot", tok::c_grip_hot, kCanvas),
    c(GripBorder, "grip.border", tok::c_grip_border, kCanvas),
    c(SnapMarker, "snap.marker", tok::c_snap, kCanvas),
    c(TrackingVector, "tracking.vector", tok::c_track, kCanvas),
    c(DynTooltipBg, "dyn.tooltip.bg", tok::c_dyn_bg, kCanvas),
    c(DynTooltipText, "dyn.tooltip.text", tok::c_dyn_t, kCanvas),
    c(DynTooltipBorder, "dyn.tooltip.border", tok::c_dyn_b, kCanvas),
    n(FadeXref, "fade.xref", tok::fade_xref),
    n(FadeLocked, "fade.locked", tok::fade_locked),
    c(PaperBg, "paper.bg", tok::c_paper, kCanvas),
    c(PaperPrintable, "paper.printable", tok::c_paper_print, kCanvas),
    c(PaperShadow, "paper.shadow", tok::c_paper_shadow, kCanvas),
    c(StatusOn, "status.on", tok::c_on, kUi),
    c(StatusOff, "status.off", tok::c_off, kUi),
    c(EditionViewer, "edition.badge.viewer", tok::c_ed_viewer, kUi),
    c(EditionViewerText, "edition.badge.viewer.text", tok::c_ed_viewer_t, kUi),
    c(EditionViewerPro, "edition.badge.viewerPro", tok::c_ed_vp, kUi),
    c(EditionViewerProText, "edition.badge.viewerPro.text", tok::c_ed_vp_t, kUi),
    c(EditionPro, "edition.badge.pro", tok::c_ed_pro, kUi),
    c(EditionProText, "edition.badge.pro.text", tok::c_ed_pro_t, kUi),
    c(EditionTrial, "edition.badge.trial", tok::c_ed_trial, kUi),
    c(EditionTrialText, "edition.badge.trial.text", tok::c_ed_trial_t, kUi),
    c(EditionEducation, "edition.badge.education", tok::c_ed_edu, kUi),
    c(EditionEducationText, "edition.badge.education.text", tok::c_ed_edu_t, kUi),
    c(EditionEnterprise, "edition.badge.enterprise", tok::c_ed_ent, kUi),
    c(EditionEnterpriseText, "edition.badge.enterprise.text", tok::c_ed_ent_t, kUi),
    c(CompareOnlyA, "compare.onlyA", tok::c_cmp_a, kCanvas),
    c(CompareOnlyB, "compare.onlyB", tok::c_cmp_b, kCanvas),
    c(CompareCommon, "compare.common", tok::c_cmp_common, kCanvas),
};

constexpr bool inEnumOrder()
{
    for (std::size_t i = 0; i < std::size(kEntries); ++i)
        if (std::size_t(kEntries[i].sem) != i)
            return false;
    return true;
}
static_assert(inEnumOrder() && std::size(kEntries) == std::size_t(CompareCommon) + 1);

QLatin1StringView key(Sem s)
{
    const std::string_view name = semEntry(s).name;
    return QLatin1StringView(name.data(), qsizetype(name.size()));
}

} // namespace

std::span<const SemEntry> semEntries() { return kEntries; }

const SemEntry& semEntry(Sem s) { return kEntries[std::size_t(s)]; }

std::optional<Sem> semFromName(std::string_view name)
{
    for (const SemEntry& e : kEntries)
        if (e.name == name)
            return e.sem;
    return std::nullopt;
}

QColor toQColor(tok::Rgba c) { return QColor(c.r, c.g, c.b, c.a); }

QColor SemanticColors::defaultColor(Sem s, tok::Mode ui) const
{
    const SemEntry& e = semEntry(s);
    TRION_ASSERT(e.kind == tok::Kind::Color);
    return toQColor(tok::color(tok::ColorId{e.token}, e.canvas ? tok::Mode{} : ui));
}

double SemanticColors::defaultFade(Sem s) const
{
    const SemEntry& e = semEntry(s);
    TRION_ASSERT(e.kind == tok::Kind::Num);
    return tok::num(tok::NumId{e.token});
}

QColor SemanticColors::color(Sem s, tok::Mode ui) const
{
    const QColor o = QColor::fromString(overrides_.value(key(s)).toString());
    return o.isValid() ? o : defaultColor(s, ui);
}

double SemanticColors::fade(Sem s) const
{
    const QJsonValue o = overrides_.value(key(s));
    return o.isDouble() ? std::clamp(o.toDouble(), 0.0, 1.0) : defaultFade(s);
}

bool SemanticColors::isOverridden(Sem s) const { return overrides_.contains(key(s)); }

void SemanticColors::setColor(Sem s, const QColor& c)
{
    TRION_ASSERT(semEntry(s).kind == tok::Kind::Color && c.isValid());
    put(s, c.name(QColor::HexArgb));
}

void SemanticColors::setFade(Sem s, double f)
{
    TRION_ASSERT(semEntry(s).kind == tok::Kind::Num);
    put(s, std::clamp(f, 0.0, 1.0));
}

void SemanticColors::reset(Sem s) { put(s, QJsonValue(QJsonValue::Undefined)); }

void SemanticColors::put(Sem s, const QJsonValue& v)
{
    QJsonObject o = store_.get(kSemanticOverrides);
    if (v.isUndefined())
        o.remove(key(s));
    else
        o.insert(key(s), v);
    if (o.isEmpty())
        store_.reset(kSemanticOverrides);
    else
        store_.set(kSemanticOverrides, o);
    reload();
}

} // namespace trion::theme
