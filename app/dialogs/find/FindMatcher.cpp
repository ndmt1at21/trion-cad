#include "app/dialogs/find/FindMatcher.h"

#include <QChar>

#include <algorithm>
#include <array>
#include <utility>

using namespace Qt::StringLiterals;

namespace trion::find {

namespace {

// Bảng NFD của chữ Latin có dấu: ký tự → chữ gốc + dấu kết hợp theo thứ tự chuẩn ("ệ" → e U+0323 U+0302).
constexpr char16_t kLatinA = 0x00C0, kLatinAEnd = 0x0250; // Latin-1 Supplement … Latin Extended-B
constexpr char16_t kLatinB = 0x1E00, kLatinBEnd = 0x1F00; // Latin Extended Additional (chữ Việt dựng sẵn)
constexpr std::size_t kSizeA = kLatinAEnd - kLatinA;

struct Decomp {
    std::array<char16_t, 4> c{};
    std::uint8_t n = 0; // 0: không tách
};

const Decomp* decomposition(char16_t ch)
{
    static const auto table = [] { // dựng một lần (khởi tạo static an toàn luồng), ~3 KB
        std::array<Decomp, kSizeA + (kLatinBEnd - kLatinB)> t{};
        const auto fill = [&t](char16_t from, char16_t to, std::size_t base) {
            for (char16_t c = from; c < to; ++c) {
                const QString d = QString(QChar(c)).normalized(QString::NormalizationForm_D);
                Decomp& e = t[base + std::size_t(c - from)];
                if (d.size() < 2 || d.size() > qsizetype(e.c.size()))
                    continue;
                for (qsizetype i = 0; i < d.size(); ++i)
                    e.c[std::size_t(i)] = d[i].unicode();
                e.n = std::uint8_t(d.size());
            }
        };
        fill(kLatinA, kLatinAEnd, 0);
        fill(kLatinB, kLatinBEnd, kSizeA);
        return t;
    }();
    const Decomp* e = nullptr;
    if (ch >= kLatinA && ch < kLatinAEnd)
        e = &table[std::size_t(ch - kLatinA)];
    else if (ch >= kLatinB && ch < kLatinBEnd)
        e = &table[kSizeA + std::size_t(ch - kLatinB)];
    return e && e->n ? e : nullptr;
}

// Dấu kết hợp (Mn) như foldForSearch: bỏ khi không phân biệt dấu.
bool isMark(char16_t c)
{
    return c >= 0x0300 && QChar::category(char32_t(c)) == QChar::Mark_NonSpacing;
}

int combiningClass(char16_t c)
{
    return c < 0x0300 ? 0 : QChar::combiningClass(char32_t(c));
}

char16_t foldCase(char16_t c)
{
    if (c < 0x80)
        return c >= u'A' && c <= u'Z' ? char16_t(c + (u'a' - u'A')) : c;
    return char16_t(QChar::toCaseFolded(char32_t(c))); // gấp đơn giản: BMP → BMP
}

// Khóa so sánh của s (xem FindMatcher.h). map (nếu có): vị trí trong s của từng ký tự khóa.
void makeKey(QStringView s, bool fold, bool strip, std::vector<char16_t>& key, std::vector<int>* map)
{
    key.clear();
    if (map)
        map->clear();
    const auto put = [&](char16_t c, int at) {
        key.push_back(fold ? foldCase(c) : c);
        if (map)
            map->push_back(at);
        const int cc = strip ? 0 : combiningClass(c);
        // Thứ tự chuẩn của dấu (NFD): dấu lớp kết hợp nhỏ hơn đứng trước ("ê" + U+0323 → e U+0323 U+0302).
        for (std::size_t j = key.size() - 1; cc > 0 && j > 0; --j) {
            if (combiningClass(key[j - 1]) <= cc)
                break;
            std::swap(key[j - 1], key[j]);
            if (map)
                std::swap((*map)[j - 1], (*map)[j]);
        }
    };
    for (qsizetype i = 0; i < s.size(); ++i) {
        const char16_t ch = s[i].unicode();
        const int at = int(i);
        if (ch < 0x80) {
            put(ch, at);
        } else if (strip && (ch == u'đ' || ch == u'Đ')) {
            put(ch == u'đ' ? u'd' : u'D', at);
        } else if (const Decomp* d = decomposition(ch)) {
            for (std::uint8_t k = 0; k < d->n; ++k)
                if (!strip || !isMark(d->c[k]))
                    put(d->c[k], at);
        } else if (!strip || !isMark(ch)) {
            put(ch, at);
        }
    }
}

} // namespace

KindGroup groupOf(TextKind kind)
{
    switch (kind) {
    case TextKind::Attribute:
        return kAttributes;
    case TextKind::Dimension:
    case TextKind::Leader:
        return kDimLeader;
    case TextKind::Table:
        return kTables;
    case TextKind::Hyperlink:
        return kHyperlinks;
    case TextKind::Text:
    case TextKind::MText:
        break;
    }
    return kTexts;
}

FindMatcher::FindMatcher(const QString& needle, FindOptions options) : options_(std::move(options))
{
    std::vector<char16_t> key;
    makeKey(needle, !options_.matchCase, !options_.matchAccents, key, nullptr);
    if (key.empty())
        return;
    // Mẫu trên khóa (đã gấp hoa thường, dấu theo tùy chọn): chữ thường được thoát theo đoạn (giữ cặp surrogate).
    const QString wordChar = u"[\\p{L}\\p{N}\\p{M}_]"_s;
    QString pattern;
    if (options_.wholeWord)
        pattern += u"(?<!"_s + wordChar + u')';
    QString literal;
    const auto flush = [&] {
        pattern += QRegularExpression::escape(literal);
        literal.clear();
    };
    for (const char16_t c : key) {
        if (options_.wildcard && (c == u'*' || c == u'?')) {
            flush();
            if (c == u'*')
                pattern += u".*"_s;
            else // một chữ cùng các dấu của nó (khóa bỏ dấu thì chỉ còn chữ)
                pattern += options_.matchAccents ? u"\\P{M}\\p{M}*"_s : u"."_s;
        } else {
            literal += QChar(c);
        }
    }
    flush();
    if (options_.wholeWord)
        pattern += u"(?!"_s + wordChar + u')';
    else if (options_.matchAccents)
        pattern += u"(?!\\p{M})"_s; // không dừng giữa chữ: "ba" không khớp "bả"
    re_.setPatternOptions(QRegularExpression::DotMatchesEverythingOption);
    re_.setPattern(pattern);
    valid_ = re_.isValid(); // sai chỉ khi chuỗi tìm có surrogate lẻ (UTF-16 hỏng): không khớp gì
}

std::optional<Span> FindMatcher::match(QStringView s) const
{
    if (!valid_)
        return std::nullopt;
    makeKey(s, !options_.matchCase, !options_.matchAccents, key_, &map_);
    const QRegularExpressionMatch m = re_.matchView(QStringView(key_.data(), qsizetype(key_.size())));
    if (!m.hasMatch())
        return std::nullopt;
    const auto first = std::size_t(m.capturedStart()), last = std::size_t(m.capturedEnd());
    if (first >= last) // khớp rỗng ("*" trên chữ rỗng)
        return Span{first < map_.size() ? map_[first] : int(s.size()), 0};
    int start = map_[first], end = map_[first] + 1;
    for (std::size_t k = first; k < last; ++k) { // dấu đổi chỗ khi sắp thứ tự chuẩn: lấy min, max
        start = std::min(start, map_[k]);
        end = std::max(end, map_[k] + 1);
    }
    while (end < s.size() && (isMark(s[end].unicode()) || s[end].isLowSurrogate())) // dấu đứng sau thuộc chữ cuối
        ++end;
    return Span{start, end - start};
}

bool FindMatcher::accepts(const TextItem& item) const
{
    const FindOptions& o = options_;
    if ((item.hidden && o.skipHidden) || (item.inXref && !o.inXrefs) || (item.inBlock && !o.inBlocks) ||
        !(o.kinds & groupOf(item.kind)))
        return false;
    switch (o.scope) {
    case Scope::Drawing:
        break;
    case Scope::CurrentSpace:
        return item.layout == o.space;
    case Scope::Selection:
        return o.selection && std::binary_search(o.selection->begin(), o.selection->end(), item.id);
    }
    return true;
}

bool FindMatcher::usesDecoded(const TextItem& item) const
{
    return options_.legacy && item.legacy != Legacy::None && !item.decoded.isEmpty();
}

void FindMatcher::scan(const TextList& items, int first, int last, std::vector<Hit>& out,
                       const sched::CancelToken& cancel) const
{
    if (!valid_)
        return;
    for (int i = first; i < last; ++i) {
        if ((i - first) % kCheckEvery == 0 && cancel.cancelled())
            return;
        const TextItem& t = items[std::size_t(i)];
        if (!accepts(t))
            continue;
        const bool decoded = usesDecoded(t);
        if (const std::optional<Span> span = match(decoded ? t.decoded : t.text))
            out.push_back({i, *span, decoded});
    }
}

std::vector<Hit> findAll(const TextList& items, const QString& needle, const FindOptions& options)
{
    std::vector<Hit> out;
    FindMatcher(needle, options).scan(items, 0, int(items.size()), out);
    return out;
}

} // namespace trion::find
