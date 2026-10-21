#pragma once
// UIA-106 · Bộ gõ tiếng Việt trong ô dòng lệnh (spike S6, kết luận ở docs/spikes/S6-ime.md).
//
// Bỏ qua bộ gõ khi chờ tên lệnh ("lm" không thành "lầm"), theo cách bộ gõ đưa chữ vào ứng dụng:
//  - Bộ gõ đi qua input method của hệ điều hành (TSF/IMM: bộ gõ tiếng Việt của Windows; bộ gõ Telex/VNI của macOS;
//    ibus, fcitx5 với Bamboo, Unikey): ô trả Qt::ImEnabled = false là đủ, plugin của Qt không đưa phím cho bộ gõ mà
//    giao thẳng phím vật lý cho ô. CommandInput override inputMethodQuery (QLineEdit trả ImEnabled theo isReadOnly(),
//    không theo Qt::WA_InputMethodEnabled) và tắt thuộc tính để Qt báo bộ gõ. macOS, Linux không cần gì thêm.
//  - Bộ gõ kiểu hook của Windows (Unikey, EVKey, OpenKey): hook WH_KEYBOARD_LL của nó nuốt phím vật lý vừa gõ rồi
//    dùng SendInput chèn Backspace (VK_BACK, mã quét 0) và chữ có dấu (VK_PACKET). Thuộc tính của ô không chặn được,
//    còn bỏ các sự kiện chèn thì mất luôn phím đã bị nuốt ("aa" ra "A"). setHookTarget() đặt hook WH_KEYBOARD_LL của
//    ta (đặt sau nên được gọi trước hook bộ gõ) để ghi phím vật lý, rồi đối chiếu với MSG tới luồng UI: phím vật lý
//    hook thấy mà MSG không tới là bị bộ gõ nuốt, giao lại ký tự của nó cho ô; sự kiện dạng chèn (Backspace mã quét 0,
//    VK_PACKET) thì bỏ.
//    Phần quyết định là KeyReconciler (thuần, test được trên mọi nền tảng); phần gắn Windows ở ImeBypass_win.cpp.
//
// Chuẩn hóa commit (nfcCommit): mọi nền tảng, cho ô dòng lệnh và trình soạn MTEXT.

#include <QString>
#include <QStringView>

#include <cstdint>
#include <deque>

class QWidget;

namespace trion::ui::ime {

// Commit của bộ gõ ở dạng NFC. Bộ gõ gửi tổ hợp NFD tách lượt (đã commit "a", lượt sau commit dấu rời U+0302) thì
// kéo các ký tự đứng ngay trước phần thay thế vào commit đến khi commit không còn mở đầu bằng dấu, rồi chuẩn hóa.
// start, length như QInputMethodEvent::setCommitString (start tính từ con trỏ); beforeCursor: văn bản của ô trước
// con trỏ (không gồm preedit).
struct Commit {
    QString text;
    int start = 0;
    int length = 0;
};
Commit nfcCommit(QString text, int start, int length, QStringView beforeCursor);

// Hằng của winuser.h, để phần thuần biên dịch và test trên mọi nền tảng.
namespace win {
inline constexpr unsigned kKeyDown = 0x0100, kKeyUp = 0x0101, kSysKeyDown = 0x0104, kSysKeyUp = 0x0105; // WM_*
inline constexpr std::uint32_t kVkBack = 0x08, kVkPacket = 0xE7;
inline constexpr std::uint32_t kLowerIlInjected = 0x02, kInjected = 0x10, kUp = 0x80; // LLKHF_*
} // namespace win

// Một sự kiện phím hook WH_KEYBOARD_LL thấy (các trường của KBDLLHOOKSTRUCT), theo thứ tự hệ thống phát.
struct HookKey {
    std::uint32_t vk = 0;
    std::uint32_t scan = 0;
    std::uint32_t flags = 0; // LLKHF_*
    // Ký tự của phím nhấn xuống không kèm Ctrl/Alt/Win, có mã quét thật (phím vật lý, phím công cụ điều khiển từ xa
    // chèn); 0 nếu không có, và luôn 0 với dạng bộ gõ chèn (VK_PACKET, mã quét 0).
    char16_t text = 0;
};

struct Verdict {
    bool drop = false; // bỏ MSG: không TranslateMessage, không DispatchMessage
    bool rehook = false; // chữ của bộ gõ tới mà hook của ta không thấy phím bị nuốt: hook bộ gõ đang đứng trước
    QString replay; // ký tự của các phím vật lý bộ gõ đã nuốt, giao cho ô trước MSG này
};

// Đối chiếu phím vật lý (hook) với MSG tới luồng UI khi đang bỏ qua bộ gõ. Chỉ dùng trên một luồng: hook LL chạy trên
// luồng đã đặt nó (luồng UI), lúc luồng đó lấy tin nhắn.
// MSG khớp với sự kiện hook đầu tiên cùng phím (vk, mã quét, nhấn/nhả). Khớp nhầm chỉ xảy ra giữa các sự kiện của
// cùng một phím nên không đổi chuỗi ký tự ra.
class KeyReconciler {
public:
    void onHook(const HookKey& key);
    // message, wParam, lParam của MSG. Không phải WM_KEYDOWN/UP, WM_SYSKEYDOWN/UP thì trả Verdict rỗng.
    Verdict onMessage(unsigned message, std::uintptr_t wParam, std::intptr_t lParam);
    void reset();

private:
    std::deque<HookKey> log_; // sự kiện hook đã thấy, MSG chưa tới
    bool swallowed_ = false;  // đã thấy phím bị nuốt từ sau phím thật gần nhất
    bool hookAlive_ = false;  // hook đã ghi ít nhất một sự kiện
};

// Windows: bật lọc bộ gõ kiểu hook cho ô `target` (đang có focus), nullptr thì tắt và gỡ hook. Nền tảng khác: không
// làm gì. Luồng UI. Phím bị nuốt được giao lại cho cửa sổ của ô dưới dạng QKeyEvent không modifier.
void setHookTarget(QWidget* target);

} // namespace trion::ui::ime
