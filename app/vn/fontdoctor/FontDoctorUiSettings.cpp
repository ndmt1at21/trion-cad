#include "app/vn/fontdoctor/FontDoctorUiSettings.h"

#include <QDir>
#include <QFileInfo>

using namespace Qt::StringLiterals;

namespace trion::fontdoctor {

ShowMode FontDoctorUiSettings::showMode() const
{
    return store_->get(kShowMode) == u"indicator"_s ? ShowMode::Indicator : ShowMode::Banner;
}

void FontDoctorUiSettings::setShowMode(ShowMode mode)
{
    store_->set(kShowMode, mode == ShowMode::Indicator ? u"indicator"_s : u"banner"_s);
}

bool FontDoctorUiSettings::isMuted(const QString& path) const
{
    return !path.isEmpty() && store_->get(kMutedFiles).contains(normalizedPath(path));
}

void FontDoctorUiSettings::mute(const QString& path)
{
    if (path.isEmpty())
        return;
    const QString key = normalizedPath(path);
    QStringList files = store_->get(kMutedFiles);
    files.removeAll(key);
    files.append(key);
    if (files.size() > kMaxMuted)
        files.remove(0, files.size() - kMaxMuted);
    store_->set(kMutedFiles, files);
}

QString FontDoctorUiSettings::normalizedPath(const QString& path)
{
    if (path.isEmpty())
        return {};
    // absoluteFilePath chỉ ghép thư mục hiện hành với đường dẫn tương đối, không stat tệp.
    QString p = QDir::cleanPath(QFileInfo(QDir::fromNativeSeparators(path)).absoluteFilePath())
                    .normalized(QString::NormalizationForm_C);
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    p = p.toCaseFolded();
#endif
    return p;
}

} // namespace trion::fontdoctor
