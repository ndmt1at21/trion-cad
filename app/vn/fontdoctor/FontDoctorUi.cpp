#include "app/vn/fontdoctor/FontDoctorUi.h"

#include "app/vn/fontdoctor/FontDoctorBanner.h"
#include "app/vn/fontdoctor/FontStatusChip.h"

namespace trion::fontdoctor {

using Clock = std::chrono::steady_clock;

FontDoctorUi::FontDoctorUi(FontDoctorBanner& banner, FontStatusChip& chip, FontDoctorUiSettings settings,
                           QObject* parent)
    : QObject(parent), banner_(&banner), chip_(&chip), settings_(settings)
{
    timer_.setSingleShot(true);
    timer_.setTimerType(Qt::PreciseTimer); // không hiện chip "Đang quét font" sớm hơn kScanChipDelay
    connect(&timer_, &QTimer::timeout, this, &FontDoctorUi::syncChip);
    connect(&banner, &FontDoctorBanner::detailsRequested, this, [this] { emit paletteRequested(Tab::Summary); });
    connect(&banner, &FontDoctorBanner::convertRequested, this, [this] { emit paletteRequested(Tab::Convert); });
    connect(&banner, &FontDoctorBanner::closeRequested, this, &FontDoctorUi::close);
    connect(&banner, &FontDoctorBanner::muteRequested, this, &FontDoctorUi::mute);
    connect(&chip, &QToolButton::clicked, this, [this] { emit paletteRequested(Tab::Summary); });
    syncBanner();
    syncChip();
}

void FontDoctorUi::scanStarted(quint64 doc, const QString& path)
{
    Doc& d = docs_[doc]; // quét lại bản vẽ đang mở: giữ "đã đóng banner"
    d.path = path;
    d.scan.reset();
    d.progress = -1;
    d.started = Clock::now();
    d.banner = false;
    if (doc == current_) {
        syncBanner();
        syncChip();
    }
}

void FontDoctorUi::scanProgress(quint64 doc, double fraction)
{
    const auto it = docs_.find(doc);
    if (it == docs_.end() || it->scan)
        return;
    it->progress = fraction;
    if (doc == current_)
        syncChip();
}

void FontDoctorUi::scanFinished(quint64 doc, const FontScan& scan)
{
    Doc& d = docs_[doc]; // chưa có scanStarted: không có đường dẫn
    d.scan = scan;
    d.progress = -1;
    d.banner = scan.hasIssues() && settings_.showMode() == ShowMode::Banner && !settings_.isMuted(d.path);
    if (doc == current_) {
        syncBanner();
        syncChip();
    }
}

void FontDoctorUi::removeDocument(quint64 doc)
{
    if (docs_.remove(doc) && doc == current_) {
        syncBanner();
        syncChip();
    }
}

void FontDoctorUi::setCurrent(quint64 doc)
{
    if (doc == current_)
        return;
    current_ = doc;
    syncBanner();
    syncChip();
}

bool FontDoctorUi::isScanning(quint64 doc) const
{
    const auto it = docs_.constFind(doc);
    return it != docs_.cend() && !it->scan;
}

std::optional<FontScan> FontDoctorUi::result(quint64 doc) const
{
    const auto it = docs_.constFind(doc);
    return it != docs_.cend() ? it->scan : std::nullopt;
}

void FontDoctorUi::syncBanner()
{
    if (!banner_)
        return;
    const auto it = docs_.constFind(current_);
    if (it != docs_.cend() && it->scan && it->banner && !it->closed)
        banner_->showScan(it->scan->missingFonts, it->scan->tcvn3, it->scan->vni, !it->path.isEmpty());
    else
        banner_->hide();
}

void FontDoctorUi::syncChip()
{
    const auto it = docs_.constFind(current_);
    const auto waited = it != docs_.cend() ? Clock::now() - it->started : Clock::duration::zero();
    if (!chip_ || it == docs_.cend() || it->scan || waited >= delay_)
        timer_.stop();
    if (!chip_)
        return;
    if (it == docs_.cend())
        chip_->clear();
    else if (it->scan)
        chip_->setIssues(it->scan->missingFonts, it->scan->tcvn3, it->scan->vni);
    else if (waited >= delay_)
        chip_->setScanning(it->progress);
    else { // quét chưa tới kScanChipDelay: chưa hiện gì, hẹn lần kiểm sau
        chip_->clear();
        timer_.start(std::chrono::ceil<std::chrono::milliseconds>(delay_ - waited));
    }
}

void FontDoctorUi::close()
{
    if (const auto it = docs_.find(current_); it != docs_.end())
        it->closed = true;
    syncBanner();
}

void FontDoctorUi::mute()
{
    const auto it = docs_.constFind(current_);
    if (it == docs_.cend() || it->path.isEmpty())
        return;
    const QString path = it->path;
    settings_.mute(path);
    const QString key = FontDoctorUiSettings::normalizedPath(path);
    for (Doc& d : docs_) // cùng tệp mở ở tab khác (nếu có): cũng thôi nhắc
        if (!d.path.isEmpty() && FontDoctorUiSettings::normalizedPath(d.path) == key)
            d.banner = false;
    syncBanner();
}

} // namespace trion::fontdoctor
