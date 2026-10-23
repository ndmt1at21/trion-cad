#include "app/dialogs/file/FilePreviewService.h"

#include "core/base/assert.h"

#include <QCache>
#include <QFile>
#include <QFileInfo>
#include <QTimeZone>

#include <algorithm>
#include <array>
#include <charconv>
#include <mutex>

using namespace Qt::StringLiterals;

namespace trion::files {

using Format = FilePreviewInfo::Format;
using Error = FilePreviewInfo::Error;

struct FilePreviewService::Cache {
    struct Key {
        QString path;
        qint64 modified; // ms từ epoch, UTC
        qint64 size;
        friend bool operator==(const Key&, const Key&) = default;
        friend size_t qHash(const Key& k, size_t seed = 0) { return qHashMulti(seed, k.path, k.modified, k.size); }
    };
    std::mutex mutex;
    QCache<Key, FilePreviewInfo> lru{kCacheEntries}; // mỗi mục giá 1: LRU theo số mục
    quint64 generation = 0;                          // tăng khi bỏ cache: việc của nguồn cũ không ghi vào
};

namespace {

// Mã đầu tệp → tên phiên bản (Open Design Specification for .dwg, bảng mã phiên bản). Cũ hơn R9, mới hơn bảng: hiện mã.
constexpr struct {
    const char* code;
    const char* name;
} kVersions[] = {{"AC1004", "R9"},   {"AC1006", "R10"},  {"AC1009", "R12"},  {"AC1012", "R13"},
                 {"AC1014", "R14"},  {"AC1015", "2000"}, {"AC1018", "2004"}, {"AC1021", "2007"},
                 {"AC1024", "2010"}, {"AC1027", "2013"}, {"AC1032", "2018"}};

constexpr char kBinaryDxf[] = "AutoCAD Binary DXF\r\n\x1a"; // sentinel 22 byte, kể cả NUL cuối

// "AC" và 4 ký tự số hoặc '.' (AC1032, AC1.50): mã phiên bản đúng dạng.
bool isCode(QByteArrayView c)
{
    if (c.size() != 6 || !c.startsWith("AC"))
        return false;
    return std::all_of(c.begin() + 2, c.end(), [](char ch) { return (ch >= '0' && ch <= '9') || ch == '.'; });
}

// Số nguyên little-endian tại at; người gọi đã kiểm biên.
quint64 le(QByteArrayView b, qsizetype at, int bytes)
{
    quint64 v = 0;
    for (int i = bytes - 1; i >= 0; --i)
        v = (v << 8) | uchar(b[at + i]);
    return v;
}

// Header cố định của DWG đủ và đúng (h: đầu tệp, size: cỡ cả tệp). Chỉ kiểm điều chắc chắn theo đặc tả, còn lại cho
// qua: tệp sai sâu hơn do lệnh OPEN báo (UIB-007).
bool dwgHeaderOk(QByteArrayView h, QByteArrayView code, qint64 size)
{
    if (h.size() < 0x19) // mã, bảo trì, địa chỉ ảnh, trang mã, số bản ghi: phần chung từ R13
        return false;
    if (code == "AC1012" || code == "AC1014" || code == "AC1015") {
        // R13–R2000: 0x15 số bản ghi section locator (RL), từ 0x19 mỗi bản ghi 9 byte (số RC, vị trí RL, cỡ RL), rồi
        // CRC 2 byte, sentinel 16 byte. Section trỏ quá cuối tệp: tệp cụt.
        const quint64 n = le(h, 0x15, 4);
        if (n == 0 || n > 64)
            return false;
        if (h.size() < 0x19 + qsizetype(n) * 9 + 2 + 16)
            return false;
        for (qsizetype i = 0; i < qsizetype(n); ++i) {
            const qsizetype at = 0x19 + i * 9;
            if (le(h, at + 1, 4) + le(h, at + 5, 4) > quint64(size))
                return false;
        }
        return true;
    }
    if (code == "AC1018" || code == "AC1024" || code == "AC1027" || code == "AC1032") {
        // R2004 trở lên: 0x6C byte từ 0x80 mã hóa bằng dãy rand() của MSVC (hạt 1), giải ra "AcFssFcAJMB\0" và các hằng
        // 0x6C, 4, 0x20, 0x80, 0x40; địa chỉ bảng trang section (0x54, cộng 0x100) phải nằm trong tệp.
        if (h.size() < 0x100)
            return false;
        std::array<char, 0x6C> d{};
        quint32 seed = 1;
        for (qsizetype i = 0; i < qsizetype(d.size()); ++i) {
            seed = seed * 0x343FDu + 0x269EC3u;
            d[std::size_t(i)] = char(uchar(h[0x80 + i]) ^ uchar(seed >> 16));
        }
        const QByteArrayView dv(d.data(), qsizetype(d.size()));
        if (dv.first(12) != QByteArrayView("AcFssFcAJMB", 12))
            return false;
        if (le(dv, 0x10, 4) == 0x6C && le(dv, 0x14, 4) == 4 && le(dv, 0x44, 4) == 0x20 && le(dv, 0x48, 4) == 0x80
            && le(dv, 0x4C, 4) == 0x40) {
            const quint64 map = le(dv, 0x54, 8);
            if (map >= quint64(size) || quint64(size) - map <= 0x100)
                return false;
        }
        return true;
    }
    if (code == "AC1021") // R2007: 0x80 byte rõ rồi 0x400 byte mã Reed-Solomon
        return size >= 0x480;
    return true; // R12 trở về trước, mã mới hơn bảng: không biết cấu trúc
}

FilePreviewInfo parseDwg(QByteArrayView h, qint64 size)
{
    FilePreviewInfo info;
    info.format = Format::Dwg;
    info.code = h.first(6).toByteArray();
    info.version = FilePreviewService::versionName(info.code, Format::Dwg);
    if (!dwgHeaderOk(h, info.code, size))
        info.error = Error::Damaged;
    return info;
}

FilePreviewInfo parseBinaryDxf(QByteArrayView h)
{
    FilePreviewInfo info;
    info.format = Format::Dxf;
    info.version = u"DXF"_s;
    if (!h.startsWith(QByteArrayView(kBinaryDxf, sizeof kBinaryDxf))) {
        info.error = Error::Damaged; // đầu "AutoCAD Binary DXF" nhưng sentinel sai
        return info;
    }
    // Biến $ACADVER: tên kết thúc NUL, mã nhóm 1 (2 byte từ R13, 1 byte ở R12), giá trị "AC10xx" kết thúc NUL.
    qsizetype p = h.indexOf(QByteArrayView("$ACADVER", 9));
    if (p < 0)
        return info; // không có trong phần đã đọc: "DXF"
    p += 9;
    if (p + 2 <= h.size() && h[p] == 1 && h[p + 1] == 0)
        p += 2;
    else if (p < h.size() && h[p] == 1)
        p += 1;
    else
        p = -1;
    if (p < 0 || p + 7 > h.size() || h[p + 6] != 0 || !isCode(h.sliced(p, 6))) {
        info.error = Error::Damaged;
        return info;
    }
    info.code = h.sliced(p, 6).toByteArray();
    info.version = FilePreviewService::versionName(info.code, Format::Dxf);
    return info;
}

// DXF dạng chữ: từng cặp dòng (mã nhóm, giá trị). Cặp đầu sau chú thích 999 phải là (0, SECTION); $ACADVER nằm trong
// section HEADER, thường ở cặp đầu tiên của nó.
FilePreviewInfo parseTextDxf(QByteArrayView h, qint64 size)
{
    FilePreviewInfo info;
    const bool whole = h.size() >= size; // đã đọc cả tệp: dòng cuối không có '\n' vẫn là dòng
    if (h.startsWith("\xEF\xBB\xBF"))
        h = h.sliced(3);
    qsizetype pos = 0;
    const auto line = [&](QByteArrayView& out) {
        if (pos >= h.size())
            return false;
        qsizetype nl = h.indexOf('\n', pos);
        if (nl < 0) {
            if (!whole)
                return false; // dòng dở ở cuối phần đã đọc
            nl = h.size();
        }
        out = h.sliced(pos, nl - pos).trimmed();
        pos = nl + 1;
        return true;
    };
    int code = 0;
    QByteArrayView value;
    const auto pair = [&]() -> int { // 1: có cặp, 0: hết phần đã đọc, -1: mã nhóm sai
        QByteArrayView c;
        if (!line(c) || !line(value))
            return 0;
        const auto [end, ec] = std::from_chars(c.data(), c.data() + c.size(), code);
        return ec == std::errc() && end == c.data() + c.size() && code >= -5 && code <= 1071 ? 1 : -1;
    };
    int r = pair();
    while (r == 1 && code == 999)
        r = pair();
    if (r != 1 || code != 0 || value != "SECTION") {
        info.error = Error::NotDrawing;
        return info;
    }
    info.format = Format::Dxf;
    info.version = u"DXF"_s;
    while ((r = pair()) == 1) {
        if (code == 0 && value == "ENDSEC")
            return info; // hết section đầu mà không có $ACADVER: "DXF"
        if (code == 9 && value == "$ACADVER") {
            if (pair() != 1 || code != 1 || !isCode(value)) {
                info.error = Error::Damaged;
                return info;
            }
            info.code = value.toByteArray();
            info.version = FilePreviewService::versionName(info.code, Format::Dxf);
            return info;
        }
    }
    if (r < 0)
        info.error = Error::Damaged;
    return info;
}

FilePreviewInfo load(const QString& path, const QFileInfo& fi, const FilePreviewService::Sources& sources,
                     const sched::CancelToken& cancel)
{
    FilePreviewInfo info;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        info.error = Error::NoAccess;
    } else {
        const qint64 size = fi.size();
        const QByteArray head = f.read(std::min(size, FilePreviewService::kHeadBytes));
        if (size > 0 && head.isEmpty())
            info.error = Error::NoAccess; // lỗi đọc (ổ mạng rớt, khóa vùng)
        else
            info = FilePreviewService::parseHeader(head, size);
    }
    info.path = path;
    info.size = fi.size();
    info.modified = fi.lastModified(QTimeZone::UTC);
    if (!info.ok())
        return info;
    try {
        if (sources.thumbnail && !cancel.cancelled())
            info.thumbnail = sources.thumbnail(path, cancel);
    } catch (...) { // nguồn lỗi (ảnh nhúng hỏng): như không có ảnh
        info.thumbnail = QImage();
    }
    try {
        if (sources.savedBy && !cancel.cancelled())
            info.savedBy = sources.savedBy(path, cancel);
    } catch (...) {
        info.savedBy.clear();
    }
    return info;
}

FilePreviewInfo notFound(const QString& path)
{
    FilePreviewInfo info;
    info.path = path;
    info.error = Error::NotFound;
    return info;
}

} // namespace

FilePreviewService::FilePreviewService(QObject* parent)
    : QObject(parent), rx_(this), cache_(std::make_shared<Cache>())
{
}

FilePreviewService::~FilePreviewService()
{
    cancel_.cancel();
}

void FilePreviewService::setThumbnailSource(ThumbnailSource source)
{
    sources_.thumbnail = std::move(source);
    reset();
}

void FilePreviewService::setSavedBySource(SavedBySource source)
{
    sources_.savedBy = std::move(source);
    reset();
}

void FilePreviewService::reset()
{
    cancel();
    std::lock_guard lock(cache_->mutex);
    cache_->lru.clear();
    ++cache_->generation;
}

int FilePreviewService::cacheSize() const
{
    std::lock_guard lock(cache_->mutex);
    return int(cache_->lru.size());
}

void FilePreviewService::cancel()
{
    cancel_.cancel();
    pending_ = 0;
    pendingPath_.clear();
}

quint64 FilePreviewService::requestPreview(const QString& path)
{
    TRION_ASSERT_UI_THREAD();
    if (pending_ != 0 && path == pendingPath_)
        return pending_; // cùng tệp đang đọc
    cancel_.cancel();
    cancel_ = sched::CancelSource();
    pending_ = ++nextId_;
    pendingPath_ = path;
    quint64 generation = 0;
    {
        std::lock_guard lock(cache_->mutex);
        generation = cache_->generation;
    }
    sched::submit(sched::Priority::Normal, [rx = rx_, this, id = pending_, path, cancel = cancel_.token(),
                                            sources = sources_, cache = cache_, generation] {
        if (cancel.cancelled())
            return;
        const QFileInfo fi(path);
        FilePreviewInfo info;
        if (!fi.isFile()) {
            info = notFound(path);
        } else {
            const Cache::Key key{path, fi.lastModified(QTimeZone::UTC).toMSecsSinceEpoch(), fi.size()};
            bool hit = false;
            {
                std::lock_guard lock(cache->mutex);
                if (const FilePreviewInfo* cached = cache->lru.object(key)) {
                    info = *cached;
                    hit = true;
                }
            }
            if (!hit) {
                info = load(path, fi, sources, cancel);
                if (cancel.cancelled())
                    return;
                if (info.error != Error::NoAccess) {
                    std::lock_guard lock(cache->mutex);
                    if (cache->generation == generation)
                        cache->lru.insert(key, new FilePreviewInfo(info));
                }
            }
        }
        if (cancel.cancelled())
            return;
        ui::postLatest(rx, [this, id, info = std::move(info)] { finished(id, info); });
    });
    return pending_;
}

void FilePreviewService::finished(quint64 id, const FilePreviewInfo& info)
{
    if (id != pending_)
        return; // đã thay bằng yêu cầu khác hoặc đã hủy
    pending_ = 0;
    pendingPath_.clear();
    emit ready(info);
}

FilePreviewInfo FilePreviewService::read(const QString& path, const Sources& sources, sched::CancelToken cancel)
{
    const QFileInfo fi(path);
    return fi.isFile() ? load(path, fi, sources, cancel) : notFound(path);
}

FilePreviewInfo FilePreviewService::parseHeader(QByteArrayView head, qint64 size)
{
    if (size <= 0 || head.isEmpty()) {
        FilePreviewInfo info;
        info.error = Error::Damaged; // tệp trống
        return info;
    }
    if (head.size() >= 6 && isCode(head.first(6)))
        return parseDwg(head, size);
    if (head.startsWith("AC10")) { // mã của DWG nhưng không đọc được phiên bản
        FilePreviewInfo info;
        info.format = Format::Dwg;
        info.error = Error::Damaged;
        return info;
    }
    if (head.startsWith("AutoCAD Binary DXF"))
        return parseBinaryDxf(head);
    return parseTextDxf(head, size);
}

QString FilePreviewService::versionName(QByteArrayView code, Format format)
{
    if (code.size() < 6 || !isCode(code.first(6)))
        return {};
    code = code.first(6);
    for (const auto& v : kVersions)
        if (code == QByteArrayView(v.code))
            return (format == Format::Dxf ? u"DXF "_s : u"DWG "_s) + QLatin1StringView(v.name);
    return QString::fromLatin1(code); // bản mới hơn bảng, hoặc cũ hơn R9
}

QString FilePreviewService::errorText(Error error)
{
    switch (error) {
    case Error::None:
        return {};
    case Error::NotFound:
        return tr("Không tìm thấy tệp. Tệp có thể đã bị xóa, đổi tên hoặc ổ mạng đang ngắt.");
    case Error::NoAccess:
        return tr("Không đọc được tệp: không có quyền đọc hoặc tệp đang bị chương trình khác khóa.");
    case Error::NotDrawing:
        return tr("Tệp không phải bản vẽ DWG hay DXF.");
    case Error::Damaged:
        return tr("Tệp có thể bị hỏng: phần đầu tệp sai hoặc tệp bị cắt cụt.");
    }
    return {};
}

} // namespace trion::files
