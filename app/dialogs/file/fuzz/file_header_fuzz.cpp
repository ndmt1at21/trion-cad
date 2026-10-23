// Fuzz bộ đọc đầu tệp bản vẽ (UIB-001 FilePreviewService::parseHeader; DoD-C 7: tệp bản vẽ là đầu vào không tin cậy).
// Ngoài lỗi bộ nhớ, UB (ASan, UBSan), kiểm: tất định; đọc được thì có định dạng và tên phiên bản; mã luôn 6 byte "AC…";
// DWG thì mã là 6 byte đầu; NotDrawing không có phiên bản. Cỡ tệp: bằng phần đã đọc, và lớn hơn (tệp dài hơn kHeadBytes).
#include "app/dialogs/file/FilePreviewService.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>

using trion::files::FilePreviewInfo;
using trion::files::FilePreviewService;

namespace {

void check(bool ok)
{
    if (!ok)
        std::abort();
}

void run(QByteArrayView head, qint64 size)
{
    const FilePreviewInfo a = FilePreviewService::parseHeader(head, size);
    const FilePreviewInfo b = FilePreviewService::parseHeader(head, size);
    check(a.error == b.error && a.format == b.format && a.code == b.code && a.version == b.version);
    if (a.ok())
        check(a.format != FilePreviewInfo::Format::Unknown && !a.version.isEmpty());
    if (!a.code.isEmpty())
        check(a.code.size() == 6 && a.code.startsWith("AC"));
    if (a.format == FilePreviewInfo::Format::Dwg && !a.code.isEmpty())
        check(head.startsWith(a.code));
    if (a.error == FilePreviewInfo::Error::NotDrawing)
        check(a.version.isEmpty() && a.code.isEmpty());
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    const QByteArrayView all(reinterpret_cast<const char*>(data), qsizetype(size));
    const QByteArrayView head = all.first(std::min(all.size(), qsizetype(FilePreviewService::kHeadBytes)));
    run(head, qint64(size));
    run(head, qint64(size) * 7 + 0x10000); // tệp dài hơn phần đã đọc
    return 0;
}
