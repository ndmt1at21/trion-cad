#include "app/core/ime/ImeBypass.h"

#include <algorithm>

namespace trion::ui::ime {

Commit nfcCommit(QString text, int start, int length, QStringView beforeCursor)
{
    // at: đầu phần thay thế trong văn bản. Commit mở đầu bằng dấu (Mn, Mc): gộp ký tự đứng trước vào phần thay.
    for (qsizetype at = beforeCursor.size() + start;
         !text.isEmpty() && text.front().isMark() && at > 0 && at <= beforeCursor.size(); --at) {
        text.prepend(beforeCursor[at - 1]);
        --start;
        ++length;
    }
    return {text.normalized(QString::NormalizationForm_C), start, length};
}

namespace {
// Sự kiện hook thấy mà MSG không bao giờ tới (đi tới cửa sổ khác, hệ thống giữ như Alt+Tab): bỏ bớt cái cũ.
constexpr std::size_t kMaxLog = 64;
} // namespace

void KeyReconciler::onHook(const HookKey& key)
{
    hookAlive_ = true;
    if (log_.size() == kMaxLog)
        log_.pop_front();
    log_.push_back(key);
}

Verdict KeyReconciler::onMessage(unsigned message, std::uintptr_t wParam, std::intptr_t lParam)
{
    using namespace win;
    Verdict v;
    const bool down = message == kKeyDown || message == kSysKeyDown;
    if (!down && message != kKeyUp && message != kSysKeyUp)
        return v;
    const auto vk = std::uint32_t(wParam);
    const auto scan = std::uint32_t(lParam >> 16) & 0xFF; // MSG chỉ giữ 8 bit mã quét
    // VK_PACKET mang ký tự trong mã quét của hook, MSG không giữ đủ: chỉ so vk.
    const auto it = std::find_if(log_.begin(), log_.end(), [&](const HookKey& k) {
        return k.vk == vk && !(k.flags & kUp) == down && (vk == kVkPacket || (k.scan & 0xFF) == scan);
    });
    const bool found = it != log_.end();
    // Không khớp (hook chưa chạy, hoặc hook bộ gõ đứng trước nên ta không thấy gì): đoán theo dấu hiệu của SendInput.
    const bool injected = found ? (it->flags & (kInjected | kLowerIlInjected)) != 0 : vk == kVkPacket || scan == 0;
    // Dạng bộ gõ chèn: Backspace mã quét 0, chữ qua VK_PACKET. Công cụ điều khiển từ xa chèn phím có mã quét thật.
    const bool imeLike = injected && (vk == kVkPacket || scan == 0);
    if (!injected)
        swallowed_ = false;
    if (found)
        for (auto k = log_.begin(); k != it; ++k) // hook thấy phím có chữ mà MSG không tới: bộ gõ đã nuốt
            if (k->text && !(k->flags & kUp)) {
                v.replay += QChar(k->text);
                swallowed_ = true;
            }
    // Bỏ mọi sự kiện dạng chèn, kể cả đợt chèn tới sau vài phím thật (SendInput xếp sau các phím đã vào hàng đợi
    // của hệ thống) hay bộ gõ chèn mà không nuốt phím (gõ tắt khi nhấn Space). Hook chưa ghi gì (không đặt được) thì
    // để bộ gõ chạy như thường: bỏ chữ của nó mà không giao lại được phím thì mất phím.
    v.drop = imeLike && hookAlive_;
    // Chữ của bộ gõ tới mà không có phím bị nuốt ngay trước: hook của bộ gõ đứng trước hook của ta (phím vừa gõ đã
    // mất), đặt lại hook để hook của ta lên đầu chuỗi cho lần gõ sau.
    v.rehook = imeLike && !swallowed_;
    if (found)
        log_.erase(log_.begin(), it + 1);
    return v;
}

void KeyReconciler::reset()
{
    log_.clear();
    swallowed_ = hookAlive_ = false;
}

#ifndef Q_OS_WIN
// Bộ gõ kiểu hook chỉ có trên Windows; macOS, Linux bỏ qua bộ gõ bằng Qt::ImEnabled của ô.
void setHookTarget(QWidget*) {}
#endif

} // namespace trion::ui::ime
