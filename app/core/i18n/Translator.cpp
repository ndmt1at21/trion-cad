#include "app/core/i18n/Translator.h"

#include <QCoreApplication>
#include <QtDebug>

using namespace Qt::StringLiterals;

namespace trion::i18n {

namespace {
Translator* g_instance = nullptr; // chỉ luồng UI
} // namespace

Translator::Translator(QObject* parent) : QObject(parent)
{
    g_instance = this;
    if (!install(settings::Store::instance().get(kLanguage)))
        install(kLanguage.def);
}

Translator::~Translator()
{
    if (g_instance == this)
        g_instance = nullptr;
} // ~QTranslator tự gỡ khỏi QCoreApplication

Translator* Translator::instance() { return g_instance; }

QStringList Translator::languages() { return {u"vi"_s, u"en"_s}; }

bool Translator::setLanguage(const QString& lang)
{
    if (lang == lang_)
        return true;
    if (!install(lang))
        return false;
    settings::Store::instance().set(kLanguage, lang);
    emit languageChanged(lang);
    return true;
}

// Cài bộ mới trước rồi mới bỏ bộ cũ: không lúc nào thiếu bản dịch, và hai LanguageChange gửi tới mỗi cửa sổ
// được Qt gộp làm một. trion_vi.qm (ngôn ngữ nguồn) chỉ chứa dạng số nhiều.
bool Translator::install(const QString& lang)
{
    if (!languages().contains(lang))
        return false;
    auto next = std::make_unique<QTranslator>();
    if (!next->load(u":/i18n/trion_"_s + lang + u".qm"_s)) {
        qWarning("i18n: thiếu bản dịch :/i18n/trion_%s.qm", qUtf8Printable(lang));
        return false;
    }
    QCoreApplication::installTranslator(next.get());
    qm_ = std::move(next);
    lang_ = lang;
    return true;
}

} // namespace trion::i18n
