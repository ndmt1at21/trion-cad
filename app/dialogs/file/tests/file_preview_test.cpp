// UIB-001: đọc nhanh thông tin xem trước của tệp bản vẽ (FilePreviewService.h). DWG-026, DWG-027 chưa có: nguồn ảnh và
// "lưu lần cuối bởi" giả (đếm lời gọi, chậm theo ý, thấy CancelToken, ném lỗi). Bộ tệp mẫu tự sinh trong test theo Open
// Design Specification for .dwg; bộ 1.000 tệp QA-001: Hoãn.
//   versions        header DWG R10 → 2018 (R13–R2000: bảng section locator; R2004+: khối mã hóa), mã lạ, DXF chữ (BOM,
//                   CRLF, chú thích 999, không HEADER) và nhị phân (mã nhóm 2 byte và 1 byte) → mã, tên phiên bản.
//   errors          tệp trống, cụt, khối mã hóa sai, section quá cuối tệp, mã hỏng, $ACADVER sai, mã nhóm sai → Damaged;
//                   PDF, PNG, ZIP, chữ thường mang đuôi .dwg/.dxf → NotDrawing; không có tệp, thư mục → NotFound; không
//                   quyền đọc (Linux chmod 000, Windows mở độc quyền) → NoAccess. Tiêu chí 3.
//   robustness      6.000 đầu tệp ngẫu nhiên và biến dị của tệp mẫu: không crash, kết quả tất định, bất biến của
//                   FilePreviewInfo (fuzz dài: fuzz/file_header_fuzz.cpp).
//   asyncResult     requestPreview → ready trên luồng UI với ngày sửa, dung lượng, ảnh, "lưu lần cuối bởi"; nguồn không
//                   được gọi cho tệp lỗi; nguồn ném lỗi → không ảnh, không crash; tên tệp tiếng Việt có dấu.
//   latestWins      yêu cầu mới hủy yêu cầu cũ: nguồn chậm của yêu cầu cũ thấy hủy ≤ 50 ms (release), chỉ kết quả mới phát;
//                   cùng tệp đang đọc thì giữ việc; cancel() không phát; hủy dịch vụ khi việc đang chạy. Tiêu chí 4.
//   uiThread        mỗi requestPreview và mỗi lượt phát kết quả ≤ 2 ms trên luồng UI (release). Tiêu chí 4.
//   cache           LRU 64 mục theo (đường dẫn, giờ sửa, kích thước): chọn lại không đọc lại, tệp đổi thì đọc lại, mục
//                   vừa dùng không bị đẩy ra; đổi nguồn bỏ cache.
//   bigFile         tệp thưa 100 MB (DWG 2018, DXF) trong thư mục build: đọc ≤ 30 ms p99 (release). Tiêu chí 1.
#include "app/dialogs/file/FilePreviewService.h"

#include "app/core/UiExecutor.h"
#include "tests/timing.h"

#include <QDir>
#include <QFile>
#include <QRandomGenerator>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtTest>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using trion::files::FilePreviewInfo;
using trion::files::FilePreviewService;
using trion::test::kTimed;
using Error = FilePreviewInfo::Error;
using Format = FilePreviewInfo::Format;
namespace sched = trion::sched;
namespace ui = trion::ui;
using Clock = std::chrono::steady_clock;

namespace {

bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 10s)
{
    if (cond())
        return true;
    QEventLoop loop;
    QTimer tick;
    QObject::connect(&tick, &QTimer::timeout, &loop, [&] {
        if (cond())
            loop.quit();
    });
    QTimer::singleShot(timeout, &loop, &QEventLoop::quit);
    tick.start(1);
    loop.exec();
    return cond();
}

void spinFor(std::chrono::milliseconds d)
{
    QEventLoop loop;
    QTimer::singleShot(d, &loop, &QEventLoop::quit);
    loop.exec();
}

double ms(Clock::duration d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

double p99(std::vector<double> v)
{
    std::sort(v.begin(), v.end());
    return v.empty() ? 0 : v[std::min(v.size() - 1, std::size_t(double(v.size()) * 0.99))];
}

void put(QByteArray& b, qsizetype at, quint64 v, int bytes)
{
    for (int i = 0; i < bytes; ++i)
        b[at + i] = char((v >> (8 * i)) & 0xFF);
}

// Khối 0x6C byte của header R2004+: mã hóa bằng dãy rand() của MSVC hạt 1 (cùng phép XOR để giải).
QByteArray crypt2004(QByteArray d)
{
    quint32 seed = 1;
    for (qsizetype i = 0; i < d.size(); ++i) {
        seed = seed * 0x343FDu + 0x269EC3u;
        d[i] = char(uchar(d[i]) ^ uchar(seed >> 16));
    }
    return d;
}

// Header DWG R2004 trở lên (AC1018, AC1024, AC1027, AC1032) của tệp cỡ size: bảng trang section ở map + 0x100.
QByteArray dwg2004(const char* code, qint64 size, quint64 map = 0)
{
    QByteArray h(0x100, '\0');
    h.replace(0, 6, code);
    h[0x0C] = 3;
    QByteArray d(0x6C, '\0');
    d.replace(0, 12, QByteArray("AcFssFcAJMB", 12));
    put(d, 0x10, 0x6C, 4);
    put(d, 0x14, 4, 4);
    put(d, 0x24, 1, 4);
    put(d, 0x44, 0x20, 4);
    put(d, 0x48, 0x80, 4);
    put(d, 0x4C, 0x40, 4);
    put(d, 0x54, map ? map : quint64(size) - 0x200, 8);
    h.replace(0x80, 0x6C, crypt2004(d));
    return h;
}

// Header DWG R13–R2000 (AC1012, AC1014, AC1015): n bản ghi section locator chia đều tệp cỡ size.
QByteArray dwg2000(const char* code, qint64 size, int n = 6)
{
    static const char kSentinel[] = "\x95\xA0\x4E\x28\x99\x82\x1A\xE5\x5E\x41\xE0\x5F\x9D\x3A\x4D";
    QByteArray h(0x19 + n * 9 + 2 + 16, '\0');
    h.replace(0, 6, code);
    h[0x0C] = 1;
    put(h, 0x15, quint64(n), 4);
    const qint64 start = h.size(), part = (size - start) / n;
    for (int i = 0; i < n; ++i) {
        h[0x19 + i * 9] = char(i);
        put(h, 0x19 + i * 9 + 1, quint64(start + i * part), 4);
        put(h, 0x19 + i * 9 + 5, quint64(part), 4);
    }
    h.replace(h.size() - 16, 16, QByteArray(kSentinel, 16));
    return h;
}

// Nội dung đủ cỡ size: header rồi byte đệm.
QByteArray padded(QByteArray head, qint64 size)
{
    head.append(QByteArray(qsizetype(size) - head.size(), '\x5A'));
    return head;
}

const QByteArray kDxfText = "  0\r\nSECTION\r\n  2\r\nHEADER\r\n  9\r\n$ACADVER\r\n  1\r\nAC1027\r\n  9\r\n$ACADMAINTVER\r\n"
                            " 70\r\n   105\r\n  0\r\nENDSEC\r\n  0\r\nEOF\r\n";

QByteArray binaryDxf(const char* code, bool twoByteCodes)
{
    QByteArray b("AutoCAD Binary DXF\r\n\x1a", 22);
    const auto group = [&](int c, const char* value) {
        b.append(char(c));
        if (twoByteCodes)
            b.append('\0');
        b.append(value);
        b.append('\0');
    };
    group(0, "SECTION");
    group(2, "HEADER");
    group(9, "$ACADVER");
    group(1, code);
    group(0, "ENDSEC");
    group(0, "EOF");
    return b;
}

FilePreviewInfo parse(const QByteArray& content)
{
    return FilePreviewService::parseHeader(QByteArrayView(content).first(std::min<qsizetype>(
                                               content.size(), FilePreviewService::kHeadBytes)),
                                           content.size());
}

// Nguồn giả của DWG-026 (ảnh) và DWG-027 (ứng dụng lưu cuối).
struct FakeSources {
    std::atomic<int> thumbCalls{0};
    std::atomic<int> savedByCalls{0};
    std::atomic<int> delayMs{0};      // nguồn ảnh chậm: xét hủy mỗi 1 ms
    std::atomic<bool> fail{false};    // nguồn ảnh ném lỗi
    std::atomic<int> started{0};      // số lần nguồn ảnh bắt đầu
    std::atomic<int> cancelled{0};    // số lần nguồn ảnh thấy hủy
    std::atomic<qint64> cancelSeenNs{0};

    void reset()
    {
        thumbCalls = 0;
        savedByCalls = 0;
        delayMs = 0;
        fail = false;
        started = 0;
        cancelled = 0;
        cancelSeenNs = 0;
    }
};

qint64 nowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();
}

} // namespace

class FilePreviewTest : public QObject {
    Q_OBJECT

private:
    QTemporaryDir dir_;
    std::unique_ptr<ui::UiExecutor> executor_;
    FakeSources fake_;

    QString file(const QString& name) const { return dir_.filePath(name); }

    bool write(const QString& path, const QByteArray& content)
    {
        QFile f(path);
        return f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(content) == content.size();
    }

    void install(FilePreviewService& svc)
    {
        FakeSources* f = &fake_;
        svc.setThumbnailSource([f](const QString&, sched::CancelToken cancel) {
            ++f->thumbCalls;
            ++f->started;
            const auto t0 = Clock::now();
            const std::chrono::milliseconds delay(f->delayMs.load());
            while (Clock::now() - t0 < delay) {
                if (cancel.cancelled()) {
                    f->cancelSeenNs = nowNs();
                    ++f->cancelled;
                    return QImage();
                }
                std::this_thread::sleep_for(1ms);
            }
            if (f->fail)
                throw std::runtime_error("ảnh nhúng hỏng"); // i18n-ok: lỗi giả của nguồn
            QImage img(64, 48, QImage::Format_ARGB32_Premultiplied);
            img.fill(0xff3cc8c0u);
            return img;
        });
        svc.setSavedBySource([f](const QString&, sched::CancelToken) {
            ++f->savedByCalls;
            return u"AutoCAD 2024"_s;
        });
    }

    // Đợi ready của dịch vụ; trả kết quả cuối.
    static FilePreviewInfo await(FilePreviewService& svc, quint64 id)
    {
        FilePreviewInfo out;
        bool got = false;
        const QMetaObject::Connection c = connect(&svc, &FilePreviewService::ready, &svc,
                                                  [&](const FilePreviewInfo& info) {
                                                      out = info;
                                                      got = true;
                                                  });
        const bool ok = spinUntil([&] { return got || svc.pending() != id; });
        disconnect(c);
        if (!ok || !got)
            qWarning("không có kết quả cho yêu cầu %llu", static_cast<unsigned long long>(id));
        return out;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        executor_ = std::make_unique<ui::UiExecutor>();
    }

    void init() { fake_.reset(); }

    void cleanup() { sched::waitIdle(); } // nguồn giả là thành viên: việc nền phải xong trước ca sau

    void cleanupTestCase()
    {
        sched::waitIdle();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    void versions()
    {
        // Tên phiên bản của mã
        QCOMPARE(FilePreviewService::versionName("AC1032", Format::Dwg), u"DWG 2018"_s);
        QCOMPARE(FilePreviewService::versionName("AC1027", Format::Dwg), u"DWG 2013"_s);
        QCOMPARE(FilePreviewService::versionName("AC1024", Format::Dwg), u"DWG 2010"_s);
        QCOMPARE(FilePreviewService::versionName("AC1021", Format::Dwg), u"DWG 2007"_s);
        QCOMPARE(FilePreviewService::versionName("AC1018", Format::Dwg), u"DWG 2004"_s);
        QCOMPARE(FilePreviewService::versionName("AC1015", Format::Dwg), u"DWG 2000"_s);
        QCOMPARE(FilePreviewService::versionName("AC1014", Format::Dwg), u"DWG R14"_s);
        QCOMPARE(FilePreviewService::versionName("AC1012", Format::Dwg), u"DWG R13"_s);
        QCOMPARE(FilePreviewService::versionName("AC1009", Format::Dwg), u"DWG R12"_s);
        QCOMPARE(FilePreviewService::versionName("AC1018", Format::Dxf), u"DXF 2004"_s);
        QCOMPARE(FilePreviewService::versionName("AC1040", Format::Dwg), u"AC1040"_s); // mới hơn bảng
        QCOMPARE(FilePreviewService::versionName("AC1032xyz", Format::Dwg), u"DWG 2018"_s);
        QCOMPARE(FilePreviewService::versionName("AC10", Format::Dwg), QString());
        QCOMPARE(FilePreviewService::versionName("ACAD10", Format::Dwg), QString());

        // DWG R14 → 2018 đủ cấu trúc header, kể cả tệp lớn hơn phần đọc (16 KB)
        const struct {
            const char* code;
            const char* name;
            qint64 size;
        } dwgs[] = {{"AC1014", "DWG R14", 40'000}, {"AC1015", "DWG 2000", 3'000'000}, {"AC1018", "DWG 2004", 9'000},
                    {"AC1021", "DWG 2007", 60'000}, {"AC1024", "DWG 2010", 70'000}, {"AC1027", "DWG 2013", 80'000},
                    {"AC1032", "DWG 2018", 1'000'000}, {"AC1012", "DWG R13", 5'000}};
        for (const auto& d : dwgs) {
            const QByteArray code(d.code);
            QByteArray head = dwg2004(d.code, d.size);
            if (code == "AC1021")
                head = code.leftJustified(0x480, '\0');
            else if (code == "AC1012" || code == "AC1014" || code == "AC1015")
                head = dwg2000(d.code, d.size);
            const FilePreviewInfo info = parse(padded(head, d.size));
            QVERIFY2(info.ok(), d.code);
            QCOMPARE(info.format, Format::Dwg);
            QCOMPARE(info.code, code);
            QCOMPARE(info.version, QString::fromLatin1(d.name));
        }
        // R12, R10, mã mới hơn bảng: chỉ kiểm phần chung
        for (const char* code : {"AC1009", "AC1006", "AC1040"}) {
            const FilePreviewInfo info = parse(padded(code, 4096));
            QVERIFY2(info.ok(), code);
            QCOMPARE(info.code, QByteArray(code));
        }
        QCOMPARE(parse(padded("AC1009", 4096)).version, u"DWG R12"_s);
        QCOMPARE(parse(padded("AC1040", 4096)).version, u"AC1040"_s);

        // DXF chữ: CRLF; BOM + chú thích 999 + LF; không có HEADER; nhị phân R13+ (mã nhóm 2 byte) và R12 (1 byte)
        FilePreviewInfo info = parse(kDxfText);
        QVERIFY(info.ok());
        QCOMPARE(info.format, Format::Dxf);
        QCOMPARE(info.code, QByteArray("AC1027"));
        QCOMPARE(info.version, u"DXF 2013"_s);
        info = parse("\xEF\xBB\xBF" "999\ndxfrw 0.6.3\n  0\nSECTION\n  2\nHEADER\n  9\n$ACADVER\n  1\nAC1015  \n  0\nENDSEC\n");
        QVERIFY(info.ok());
        QCOMPARE(info.version, u"DXF 2000"_s);
        info = parse("0\nSECTION\n2\nENTITIES\n0\nLINE\n8\n0\n0\nENDSEC\n0\nEOF");
        QVERIFY(info.ok());
        QCOMPARE(info.version, u"DXF"_s);
        QVERIFY(info.code.isEmpty());
        info = parse(binaryDxf("AC1032", true));
        QVERIFY(info.ok());
        QCOMPARE(info.version, u"DXF 2018"_s);
        info = parse(binaryDxf("AC1009", false));
        QVERIFY(info.ok());
        QCOMPARE(info.version, u"DXF R12"_s);
        info = parse(QByteArray("AutoCAD Binary DXF\r\n\x1a", 22) + QByteArray(64, '\x01'));
        QVERIFY(info.ok());
        QCOMPARE(info.version, u"DXF"_s);
        // $ACADVER sau phần đọc (16 KB chú thích): "DXF", không lỗi
        QByteArray longComment = "999\n" + QByteArray(20'000, 'x') + "\n  0\nSECTION\n";
        info = parse("  0\nSECTION\n  2\nHEADER\n" + QByteArray("  9\n$EXTMIN\n 10\n0.0\n").repeated(2000) + kDxfText);
        QVERIFY(info.ok());
        QCOMPARE(info.version, u"DXF"_s);
        QCOMPARE(parse(longComment).error, Error::NotDrawing); // cặp đầu không trong phần đọc

        // Đọc qua tệp: DWT là DWG, đuôi không quyết định định dạng
        QVERIFY(write(file(u"mau.DWT"_s), padded(dwg2004("AC1032", 20'000), 20'000)));
        QVERIFY(write(file(u"that-ra-dxf.dwg"_s), kDxfText));
        QCOMPARE(FilePreviewService::read(file(u"mau.DWT"_s)).version, u"DWG 2018"_s);
        QCOMPARE(FilePreviewService::read(file(u"that-ra-dxf.dwg"_s)).version, u"DXF 2013"_s);
    }

    // Tiêu chí 3: tệp hỏng, không phải bản vẽ, không có quyền đọc: mã lỗi, không crash.
    void errors()
    {
        const auto error = [](const QByteArray& content) { return parse(content).error; };
        // Hỏng
        QCOMPARE(error({}), Error::Damaged);                                                  // trống
        QCOMPARE(error("AC1032"), Error::Damaged);                                            // chỉ có mã
        QCOMPARE(error(dwg2004("AC1032", 1'000'000).first(0x90)), Error::Damaged);           // cụt giữa header
        QCOMPARE(error(padded(dwg2004("AC1032", 1'000'000), 400'000)), Error::Damaged);     // cụt: bảng trang ở sau
        QByteArray bad = padded(dwg2004("AC1027", 50'000), 50'000);
        bad[0x85] = char(bad[0x85] ^ 0x40); // lật bit trong khối mã hóa
        QCOMPARE(error(bad), Error::Damaged);
        QCOMPARE(error(padded(dwg2000("AC1015", 90'000), 60'000)), Error::Damaged); // section quá cuối tệp
        QByteArray zero = padded(dwg2000("AC1015", 90'000), 90'000);
        put(zero, 0x15, 0, 4); // 0 bản ghi
        QCOMPARE(error(zero), Error::Damaged);
        QCOMPARE(error(dwg2000("AC1014", 90'000).first(0x30)), Error::Damaged);
        QCOMPARE(error(padded("AC1021", 0x300)), Error::Damaged);
        QCOMPARE(error(padded(QByteArray("AC10\0\0", 6), 4096)), Error::Damaged); // mã phiên bản hỏng
        QCOMPARE(error(padded("AC10xy", 4096)), Error::Damaged);
        QCOMPARE(error("  0\nSECTION\n  2\nHEADER\n  9\n$ACADVER\n  1\nXYZ\n"), Error::Damaged);
        QCOMPARE(error("  0\nSECTION\n  2\nHEADER\n  9\n$ACADVER\nabc\nAC1027\n"), Error::Damaged);
        QCOMPARE(error("  0\nSECTION\n  2\nHEADER\nkhông phải mã\n$ACADVER\n"), Error::Damaged);
        QCOMPARE(error(binaryDxf("XYZ123", true)), Error::Damaged);
        QCOMPARE(error(QByteArray("AutoCAD Binary DXF\r\nxx", 22)), Error::Damaged);
        QCOMPARE(parse(padded(dwg2004("AC1032", 1'000'000).first(0x90), 0x90)).format, Format::Dwg);
        // Không phải bản vẽ
        QCOMPARE(error("%PDF-1.7\n%\xE2\xE3\xCF\xD3\n1 0 obj\n"), Error::NotDrawing);
        QCOMPARE(error(QByteArray("\x89PNG\r\n\x1a\n\0\0\0\rIHDR", 16)), Error::NotDrawing);
        QCOMPARE(error(QByteArray("PK\x03\x04\x14\0\0\0", 8)), Error::NotDrawing);
        QCOMPARE(error("Ghi chu cong trinh\nTang 2\n"), Error::NotDrawing);
        QCOMPARE(error("123\nabc\n"), Error::NotDrawing);
        QCOMPARE(error("ACME report"), Error::NotDrawing);
        QCOMPARE(error(QByteArray(300, '\0')), Error::NotDrawing);

        // Qua tệp: lỗi, không gọi nguồn, có ngày sửa và cỡ khi tệp có
        QVERIFY(write(file(u"tai-lieu.dwg"_s), "%PDF-1.7\n"));
        FilePreviewInfo info = FilePreviewService::read(file(u"tai-lieu.dwg"_s));
        QCOMPARE(info.error, Error::NotDrawing);
        QCOMPARE(info.size, 9);
        QVERIFY(info.modified.isValid());
        QVERIFY(info.version.isEmpty());
        QVERIFY(!FilePreviewService::errorText(info.error).isEmpty());
        QVERIFY(FilePreviewService::errorText(Error::None).isEmpty());
        info = FilePreviewService::read(file(u"khong-co.dwg"_s));
        QCOMPARE(info.error, Error::NotFound);
        QCOMPARE(info.size, -1);
        QVERIFY(!info.modified.isValid());
        QVERIFY(QDir(dir_.path()).mkpath(u"thu-muc.dwg"_s));
        QCOMPARE(FilePreviewService::read(file(u"thu-muc.dwg"_s)).error, Error::NotFound);
        QVERIFY(write(file(u"trong.dxf"_s), {}));
        QCOMPARE(FilePreviewService::read(file(u"trong.dxf"_s)).error, Error::Damaged);

        // Không quyền đọc
        const QString locked = file(u"khoa.dwg"_s);
        QVERIFY(write(locked, padded(dwg2004("AC1032", 4096), 4096)));
#if defined(Q_OS_WIN)
        // Mở độc quyền (share mode 0) như chương trình khác đang khóa tệp
        const HANDLE h = CreateFileW(reinterpret_cast<const wchar_t*>(QDir::toNativeSeparators(locked).utf16()),
                                     GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        QVERIFY(h != INVALID_HANDLE_VALUE);
        info = FilePreviewService::read(locked);
        CloseHandle(h);
        QCOMPARE(info.error, Error::NoAccess);
        QCOMPARE(info.size, 4096);
#else
        QVERIFY(QFile::setPermissions(locked, QFileDevice::Permissions{}));
        if (QFile f(locked); f.open(QIODevice::ReadOnly)) {
            QFile::setPermissions(locked, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            QSKIP("tiến trình đọc được tệp chmod 000 (root)");
        }
        info = FilePreviewService::read(locked);
        QFile::setPermissions(locked, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        QCOMPARE(info.error, Error::NoAccess);
        QCOMPARE(info.size, 4096);
#endif
        QVERIFY(!FilePreviewService::errorText(Error::NoAccess).isEmpty());
    }

    // Đầu tệp ngẫu nhiên và biến dị của tệp mẫu: không crash, tất định, bất biến.
    void robustness()
    {
        QRandomGenerator rng(20261023);
        const QList<QByteArray> seeds{padded(dwg2004("AC1032", 70'000), 70'000), padded(dwg2000("AC1015", 9'000), 9'000),
                                      kDxfText, binaryDxf("AC1027", true), binaryDxf("AC1009", false)};
        int ok = 0;
        for (int i = 0; i < 6000; ++i) {
            QByteArray b;
            if (i % 3 == 0) { // ngẫu nhiên hoàn toàn
                b.resize(rng.bounded(0, 2048));
                for (char& c : b)
                    c = char(rng.bounded(256));
                if (i % 2 == 0 && b.size() >= 6)
                    b.replace(0, 6, "AC1032");
            } else { // biến dị: lật byte, cắt, lặp
                b = seeds[rng.bounded(int(seeds.size()))];
                for (int k = rng.bounded(1, 6); k > 0 && !b.isEmpty(); --k)
                    b[rng.bounded(int(b.size()))] = char(rng.bounded(256));
                if (rng.bounded(4) == 0)
                    b.truncate(rng.bounded(int(b.size()) + 1));
            }
            const qint64 size = qint64(b.size()) + (i % 5 == 0 ? rng.bounded(1, 100'000) : 0);
            const FilePreviewInfo a = FilePreviewService::parseHeader(b, size);
            const FilePreviewInfo again = FilePreviewService::parseHeader(b, size);
            QCOMPARE(a.error, again.error);
            QCOMPARE(a.code, again.code);
            QCOMPARE(a.version, again.version);
            if (a.ok()) {
                ++ok;
                QVERIFY(a.format != Format::Unknown);
                QVERIFY(!a.version.isEmpty());
            }
            if (!a.code.isEmpty()) {
                QCOMPARE(a.code.size(), 6);
                QVERIFY(a.code.startsWith("AC"));
            }
            if (a.error == Error::NotDrawing)
                QVERIFY(a.version.isEmpty());
        }
        qInfo("6.000 đầu tệp: %d đọc được", ok);
    }

    // ready trên luồng UI đủ thông tin; nguồn chỉ cho tệp đọc được; nguồn ném lỗi; tên tiếng Việt có dấu.
    void asyncResult()
    {
        FilePreviewService svc;
        install(svc);
        const QString viet = file(u"Bản vẽ nhà phố – tầng 2 (sửa đổi).dwg"_s);
        QVERIFY(write(viet, padded(dwg2004("AC1032", 30'000), 30'000)));
        const QDateTime when = QDateTime(QDate(2026, 10, 5), QTime(14, 5), QTimeZone::UTC);
        {
            QFile f(viet);
            QVERIFY(f.open(QIODevice::ReadWrite));
            QVERIFY(f.setFileTime(when, QFileDevice::FileModificationTime));
        }
        QSignalSpy spy(&svc, &FilePreviewService::ready);
        const quint64 id = svc.requestPreview(viet);
        QVERIFY(id != 0);
        QCOMPARE(svc.pending(), id);
        QVERIFY(spinUntil([&] { return spy.count() == 1; }));
        QCOMPARE(svc.pending(), quint64(0));
        auto info = spy.at(0).at(0).value<FilePreviewInfo>();
        QCOMPARE(info.path, viet);
        QVERIFY(info.ok());
        QCOMPARE(info.version, u"DWG 2018"_s);
        QCOMPARE(info.savedBy, u"AutoCAD 2024"_s);
        QCOMPARE(info.size, 30'000);
        QCOMPARE(info.modified, when);
        QCOMPARE(info.thumbnail.size(), QSize(64, 48));
        QCOMPARE(fake_.thumbCalls.load(), 1);

        // Tệp lỗi: không gọi nguồn
        QVERIFY(write(file(u"rac.dxf"_s), "rác\n"));
        info = await(svc, svc.requestPreview(file(u"rac.dxf"_s)));
        QCOMPARE(info.error, Error::NotDrawing);
        info = await(svc, svc.requestPreview(file(u"mat.dwg"_s)));
        QCOMPARE(info.error, Error::NotFound);
        QCOMPARE(fake_.thumbCalls.load(), 1);
        QCOMPARE(fake_.savedByCalls.load(), 1);

        // Nguồn ảnh ném lỗi: không ảnh, vẫn có phần còn lại
        fake_.fail = true;
        QVERIFY(write(file(u"anh-hong.dwg"_s), padded(dwg2004("AC1024", 9'000), 9'000)));
        info = await(svc, svc.requestPreview(file(u"anh-hong.dwg"_s)));
        QVERIFY(info.ok());
        QVERIFY(info.thumbnail.isNull());
        QCOMPARE(info.savedBy, u"AutoCAD 2024"_s);
        QCOMPARE(info.version, u"DWG 2010"_s);
    }

    // Tiêu chí 4: yêu cầu mới hủy yêu cầu cũ ≤ 50 ms; chỉ kết quả mới phát.
    void latestWins()
    {
        auto svc = std::make_unique<FilePreviewService>();
        install(*svc);
        const QString a = file(u"a.dwg"_s), b = file(u"b.dxf"_s);
        QVERIFY(write(a, padded(dwg2004("AC1032", 9'000), 9'000)));
        QVERIFY(write(b, kDxfText));
        QSignalSpy spy(svc.get(), &FilePreviewService::ready);

        fake_.delayMs = 5000;
        const quint64 first = svc->requestPreview(a);
        QVERIFY(spinUntil([&] { return fake_.started.load() == 1; }));
        QCOMPARE(svc->requestPreview(a), first); // cùng tệp đang đọc: giữ việc
        fake_.delayMs = 0;
        const qint64 t0 = nowNs();
        const quint64 second = svc->requestPreview(b);
        QVERIFY(second > first);
        QVERIFY(spinUntil([&] { return fake_.cancelled.load() == 1; }));
        const double latency = double(fake_.cancelSeenNs.load() - t0) / 1e6;
        qInfo("yêu cầu mới → việc cũ thấy hủy: %.2f ms", latency);
        QVERIFY2(latency <= 1000, qPrintable(QString::number(latency)));
        QVERIFY2(!kTimed || latency <= 50, qPrintable(QString::number(latency)));
        QVERIFY(spinUntil([&] { return spy.count() == 1; }));
        sched::waitIdle();
        spinFor(20ms);
        QCOMPARE(spy.count(), 1); // kết quả của a không bao giờ phát
        QCOMPARE(spy.at(0).at(0).value<FilePreviewInfo>().path, b);
        QCOMPARE(fake_.thumbCalls.load(), 2); // a (bị hủy) và b

        // cancel(): không phát
        fake_.delayMs = 5000;
        const QString c = file(u"c.dwg"_s);
        QVERIFY(write(c, padded(dwg2004("AC1027", 9'000), 9'000)));
        svc->requestPreview(c);
        QVERIFY(spinUntil([&] { return fake_.started.load() == 3; }));
        svc->cancel();
        QCOMPARE(svc->pending(), quint64(0));
        QVERIFY(spinUntil([&] { return fake_.cancelled.load() == 2; }));
        sched::waitIdle();
        spinFor(20ms);
        QCOMPARE(spy.count(), 1);

        // Hủy dịch vụ khi việc đang chạy: việc thấy hủy, không chạm đối tượng đã hủy
        const QString d = file(u"d.dwg"_s);
        QVERIFY(write(d, padded(dwg2004("AC1018", 9'000), 9'000)));
        svc->requestPreview(d);
        QVERIFY(spinUntil([&] { return fake_.started.load() == 4; }));
        svc.reset();
        QVERIFY(spinUntil([&] { return fake_.cancelled.load() == 3; }));
        sched::waitIdle();
        spinFor(20ms);
    }

    // Tiêu chí 4: luồng UI không bị chặn quá 2 ms (gửi yêu cầu, phát kết quả).
    void uiThread()
    {
        FilePreviewService svc;
        install(svc);
        QStringList paths;
        for (int i = 0; i < 20; ++i) {
            paths << file(u"ui-%1.dwg"_s.arg(i));
            QVERIFY(write(paths.last(), padded(dwg2004("AC1032", 9'000), 9'000)));
        }
        int delivered = 0;
        double longestEmit = 0;
        connect(&svc, &FilePreviewService::ready, this, [&] { ++delivered; });
        std::vector<double> calls, cpu; // đồng hồ thật (báo), CPU của luồng UI (kiểm: máy dùng chung)
        for (int round = 0; round < 10; ++round) {
            for (const QString& p : paths) { // đổi lựa chọn liên tục: mỗi yêu cầu hủy yêu cầu trước
                const auto t0 = Clock::now();
                const auto c0 = trion::test::threadCpu();
                svc.requestPreview(p);
                cpu.push_back(ms(trion::test::threadCpu() - c0));
                calls.push_back(ms(Clock::now() - t0));
            }
            QVERIFY(spinUntil([&] { return svc.pending() == 0; }));
        }
        const ui::UiExecutor::Stats after = executor_->stats();
        longestEmit = ms(after.longest);
        const double worst = *std::max_element(calls.begin(), calls.end());
        const double worstCpu = *std::max_element(cpu.begin(), cpu.end());
        qInfo("requestPreview: p99 %.3f ms, tối đa %.3f ms (CPU tối đa %.3f ms); lượt dài nhất của executor %.3f ms; "
              "%d kết quả phát",
              p99(calls), worst, worstCpu, longestEmit, delivered);
        QCOMPARE(delivered, 10); // mỗi lượt chỉ kết quả của yêu cầu cuối
        QVERIFY2(!kTimed || worstCpu <= 2.0, qPrintable(QString::number(worstCpu))); // threadCpu < 0: không đo được
        QVERIFY2(!kTimed || longestEmit <= 2.0, qPrintable(QString::number(longestEmit)));
    }

    // LRU 64 mục theo (đường dẫn, giờ sửa, kích thước).
    void cache()
    {
        FilePreviewService svc;
        install(svc);
        QStringList paths;
        for (int i = 0; i <= FilePreviewService::kCacheEntries; ++i) {
            paths << file(u"lru-%1.dwg"_s.arg(i));
            QVERIFY(write(paths.last(), padded(dwg2004("AC1032", 9'000), 9'000)));
        }
        const auto view = [&](const QString& p) { return await(svc, svc.requestPreview(p)); };
        QVERIFY(view(paths[0]).ok());
        QCOMPARE(fake_.thumbCalls.load(), 1);
        QVERIFY(view(paths[0]).ok()); // chọn lại: từ cache
        QCOMPARE(fake_.thumbCalls.load(), 1);
        QCOMPARE(svc.cacheSize(), 1);

        // Tệp đổi (cỡ, giờ sửa) → đọc lại
        QVERIFY(write(paths[0], padded(dwg2004("AC1027", 12'000), 12'000)));
        FilePreviewInfo info = view(paths[0]);
        QCOMPARE(info.version, u"DWG 2013"_s);
        QCOMPARE(fake_.thumbCalls.load(), 2);
        {
            QFile f(paths[0]);
            QVERIFY(f.open(QIODevice::ReadWrite));
            QVERIFY(f.setFileTime(QDateTime::currentDateTimeUtc().addSecs(-3600), QFileDevice::FileModificationTime));
        }
        QVERIFY(view(paths[0]).ok());
        QCOMPARE(fake_.thumbCalls.load(), 3);

        // 64 tệp: đủ chỗ; xem lại tệp 0 (vừa dùng) rồi tệp 64: tệp ít dùng nhất (1) bị đẩy ra, tệp 0 còn
        for (int i = 1; i < FilePreviewService::kCacheEntries; ++i)
            QVERIFY(view(paths[i]).ok());
        QCOMPARE(svc.cacheSize(), FilePreviewService::kCacheEntries);
        const int calls = fake_.thumbCalls.load();
        QVERIFY(view(paths[0]).ok());
        QCOMPARE(fake_.thumbCalls.load(), calls);
        QVERIFY(view(paths[FilePreviewService::kCacheEntries]).ok());
        QCOMPARE(fake_.thumbCalls.load(), calls + 1);
        QCOMPARE(svc.cacheSize(), FilePreviewService::kCacheEntries);
        QVERIFY(view(paths[0]).ok());
        QCOMPARE(fake_.thumbCalls.load(), calls + 1);
        QVERIFY(view(paths[1]).ok());
        QCOMPARE(fake_.thumbCalls.load(), calls + 2);

        // Lỗi đọc được nội dung (không phải bản vẽ) cache; không có tệp thì không
        QVERIFY(write(file(u"lru-rac.dwg"_s), "abc"));
        QCOMPARE(view(file(u"lru-rac.dwg"_s)).error, Error::NotDrawing);
        QCOMPARE(view(file(u"lru-mat.dwg"_s)).error, Error::NotFound);

        // Đổi nguồn: bỏ cache
        svc.setSavedBySource({});
        QCOMPARE(svc.cacheSize(), 0);
        info = view(paths[0]);
        QVERIFY(info.savedBy.isEmpty());
        QCOMPARE(fake_.thumbCalls.load(), calls + 3);
    }

    // Tiêu chí 1: tệp 100 MB (thưa) trong thư mục build: đọc ≤ 30 ms p99.
    void bigFile()
    {
        QTemporaryDir big(QDir::currentPath() + u"/file_preview-XXXXXX"_s);
        QVERIFY(big.isValid());
        constexpr qint64 kSize = 100LL * 1024 * 1024;
        const QString dwg = big.filePath(u"BENCH-100MB.dwg"_s), dxf = big.filePath(u"BENCH-100MB.dxf"_s);
        for (const auto& [path, head] : {std::pair{dwg, dwg2004("AC1032", kSize)}, std::pair{dxf, kDxfText}}) {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            QCOMPARE(f.write(head), head.size());
            QVERIFY(f.resize(kSize)); // thưa: không ghi 100 MB thật
        }
        std::vector<double> times;
        for (int i = 0; i < 200; ++i) {
            const auto t0 = Clock::now();
            const FilePreviewInfo info = FilePreviewService::read(i % 2 ? dxf : dwg);
            times.push_back(ms(Clock::now() - t0));
            QVERIFY(info.ok());
            QCOMPARE(info.size, kSize);
        }
        // Qua dịch vụ (nền → luồng UI), không cache
        FilePreviewService svc;
        std::vector<double> async;
        for (int i = 0; i < 40; ++i) {
            svc.setThumbnailSource({}); // bỏ cache
            const auto t0 = Clock::now();
            const FilePreviewInfo info = await(svc, svc.requestPreview(i % 2 ? dxf : dwg));
            async.push_back(ms(Clock::now() - t0));
            QCOMPARE(info.version, i % 2 ? u"DXF 2013"_s : u"DWG 2018"_s);
        }
        qInfo("tệp 100 MB: read p99 %.3f ms; requestPreview → ready p99 %.3f ms", p99(times), p99(async));
        QVERIFY2(!kTimed || p99(times) <= 30, qPrintable(QString::number(p99(times))));
        QVERIFY2(!kTimed || p99(async) <= 30, qPrintable(QString::number(p99(async))));
    }
};

QTEST_GUILESS_MAIN(FilePreviewTest)
#include "file_preview_test.moc"
