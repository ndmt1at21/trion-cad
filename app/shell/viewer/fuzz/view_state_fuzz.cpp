// Fuzz bộ đọc .tcview (UIA-118, DoD-C 7). Ngoài lỗi bộ nhớ, UB (ASan, UBSan), kiểm:
// - fromJson tất định (hai lần gọi cùng kết quả); đọc được thì đúng miền: layout 1–255 ký tự, tâm hữu hạn, cao > 0,
//   tên layer hợp lệ (isLayerName), và toJson rồi fromJson cho đúng trạng thái ấy;
// - LayerMacro từ trạng thái đọc được và từ chính đầu vào cắt theo dòng (tên bất kỳ): chỉ một ';' (cuối chuỗi, thoát
//   -LAYER), không ký tự điều khiển, ngoặc kép chẵn: tên lạ không chèn được lệnh.
#include "app/shell/viewer/ViewState.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>

using namespace trion::shell;

namespace {

void check(bool ok)
{
    if (!ok)
        std::abort();
}

void checkMacro(const QString& text)
{
    if (text.isEmpty())
        return;
    check(text.startsWith(u"'_-LAYER ") && text.endsWith(u';') && text.count(u';') == 1 && text.count(u'"') % 2 == 0);
    for (QChar c : text)
        check(c.unicode() >= 0x20 && c.unicode() != 0x7F);
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    const QByteArrayView bytes(reinterpret_cast<const char*>(data), qsizetype(size));
    const trion::Expected<ViewState> s = fromJson(bytes);
    const trion::Expected<ViewState> again = fromJson(bytes);
    check(s.has_value() == again.has_value());
    if (s) {
        check(*s == *again);
        check(!s->layout.isEmpty() && s->layout.size() <= 255);
        check(std::isfinite(s->centerX) && std::isfinite(s->centerY) && std::isfinite(s->height) && s->height > 0);
        for (const LayerFlags& l : s->layers)
            check(isLayerName(l.name));
        const trion::Expected<ViewState> back = fromJson(toJson(*s));
        check(back && *back == *s);
        checkMacro(LayerMacro::to(s->layers).text());
    } else {
        check(s.error().code == again.error().code);
    }
    LayerMacro raw;
    raw.off = QString::fromUtf8(bytes).split(u'\n');
    checkMacro(raw.text());
    return 0;
}
