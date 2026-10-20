#include "core/base/text/Fold.h"

namespace trion::text {

QString foldForSearch(QStringView s)
{
    const QString nfd = s.toString().normalized(QString::NormalizationForm_D);
    QString out;
    out.reserve(nfd.size());
    for (QChar c : nfd) {
        if (c.category() == QChar::Mark_NonSpacing)
            continue;
        if (c == u'đ' || c == u'Đ')
            c = u'd';
        out.append(c);
    }
    return out.toCaseFolded();
}

bool containsFolded(QStringView haystack, QStringView foldedNeedle)
{
    return foldedNeedle.isEmpty() || foldForSearch(haystack).contains(foldedNeedle);
}

bool startsWithFolded(QStringView haystack, QStringView foldedNeedle)
{
    return foldedNeedle.isEmpty() || foldForSearch(haystack).startsWith(foldedNeedle);
}

} // namespace trion::text
