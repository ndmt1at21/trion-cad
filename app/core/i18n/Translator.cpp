#include "app/core/i18n/Translator.h"

#include <QCoreApplication>
#include <QtDebug>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace trion::i18n {

namespace {
Translator* g_instance = nullptr; // chỉ luồng UI

// kPseudoLanguage: thêm ⌈40%⌉ ký tự (ít nhất 2), cụm 4 chữ có dấu chồng tầng ngăn bằng dấu cách để chuỗi dài vẫn
// xuống dòng được. Chữ hoa Ẫ cao nhất, ậ có dấu dưới: lộ cả chỗ cắt mép trên lẫn mép dưới. Chuỗi kết thúc bằng
// " (…)" chèn trước ngoặc: "Bản vẽ (*.dwg)" → "Bản vẽ Ẫễỗậ Ẫ (*.dwg)", bộ lọc của hộp thoại tệp vẫn đọc được.
QString stretch(const QString& s)
{
    static constexpr char16_t kPad[] = u" Ẫễỗậ";
    QString pad;
    const qsizetype extra = std::max<qsizetype>(2, (s.size() * 2 + 4) / 5);
    for (qsizetype i = 0; i < extra; ++i)
        pad += QChar(kPad[i % 5]);
    if (pad.endsWith(u' '))
        pad += kPad[1];
    const qsizetype at = s.endsWith(u')') ? s.lastIndexOf(u" (") : -1;
    return at < 0 ? s + pad : s.left(at) + pad + s.mid(at);
}

// Bản vi thật (trion_vi, qtbase_vi) rồi kéo dài kết quả; chuỗi không có bản dịch kéo dài chính chuỗi nguồn.
class PseudoTranslator final : public QTranslator {
public:
    explicit PseudoTranslator(std::vector<std::unique_ptr<QTranslator>> vi) : vi_(std::move(vi)) {}

    bool isEmpty() const override { return false; } // installTranslator chỉ gửi LanguageChange khi không rỗng

    QString translate(const char* context, const char* source, const char* disambiguation, int n) const override
    {
        if (!source || !*source || qstrcmp(context, "QShortcut") == 0)
            return {}; // tên phím ở vi vẫn là tiếng Anh: giữ nguyên để chuỗi phím tắt còn phân tích được
        QString s;
        for (const auto& t : vi_)
            if (s = t->translate(context, source, disambiguation, n); !s.isEmpty())
                break;
        return stretch(s.isEmpty() ? QString::fromUtf8(source) : s);
    }

private:
    std::vector<std::unique_ptr<QTranslator>> vi_;
};
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
    if (languages().contains(lang)) // ngôn ngữ giả chỉ cho QA: lần chạy sau về ngôn ngữ đã chọn
        settings::Store::instance().set(kLanguage, lang);
    emit languageChanged(lang);
    return true;
}

// Cài bộ mới trước rồi mới bỏ bộ cũ: không lúc nào thiếu bản dịch, và các LanguageChange gửi tới mỗi cửa sổ
// được Qt gộp làm một. trion_vi.qm (ngôn ngữ nguồn) chỉ chứa dạng số nhiều.
bool Translator::install(const QString& lang)
{
    const bool pseudo = lang == kPseudoLanguage;
    if (!pseudo && !languages().contains(lang))
        return false;
    const QStringList files = pseudo || lang == u"vi"_s ? QStringList{u"trion_vi"_s, u"qtbase_vi"_s}
                                                         : QStringList{u"trion_"_s + lang};
    std::vector<std::unique_ptr<QTranslator>> next;
    for (const QString& file : files) {
        next.push_back(std::make_unique<QTranslator>());
        if (!next.back()->load(u":/i18n/"_s + file + u".qm"_s)) {
            qWarning("i18n: thiếu bản dịch :/i18n/%s.qm", qUtf8Printable(file));
            return false;
        }
    }
    if (pseudo) {
        auto p = std::make_unique<PseudoTranslator>(std::move(next));
        next.clear();
        next.push_back(std::move(p));
    }
    for (const auto& t : next)
        QCoreApplication::installTranslator(t.get());
    qm_ = std::move(next);
    lang_ = lang;
    return true;
}

} // namespace trion::i18n
