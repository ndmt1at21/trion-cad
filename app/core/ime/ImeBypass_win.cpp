// UIA-106 · Phần Windows của ImeBypass: hook WH_KEYBOARD_LL ghi phím vật lý, bộ lọc MSG của Qt đối chiếu
// (KeyReconciler) và giao lại phím bị bộ gõ kiểu hook (Unikey, EVKey, OpenKey) nuốt.
#include "app/core/ime/ImeBypass.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QPointer>
#include <QWidget>
#include <QWindow>

#include <utility>

#include <windows.h>

namespace trion::ui::ime {
namespace {

// Ký tự của phím theo bố cục bàn phím của luồng UI (HookKey::text); 0 nếu phím nhả, kèm Ctrl/Alt/Win, không in được
// hoặc có dạng bộ gõ chèn (VK_PACKET, mã quét 0).
char16_t keyText(const KBDLLHOOKSTRUCT& k)
{
    if ((k.flags & (LLKHF_UP | LLKHF_ALTDOWN)) || k.vkCode == VK_PACKET || k.scanCode == 0)
        return 0;
    if (GetAsyncKeyState(VK_CONTROL) < 0 || GetAsyncKeyState(VK_LWIN) < 0 || GetAsyncKeyState(VK_RWIN) < 0)
        return 0;
    BYTE state[256] = {};
    if (GetAsyncKeyState(VK_SHIFT) < 0)
        state[VK_SHIFT] = 0x80;
    state[VK_CAPITAL] = GetKeyState(VK_CAPITAL) & 1;
    wchar_t buf[4];
    // Cờ 0x4: không đổi trạng thái phím chết của hệ thống (Windows 10 1607+), ứng dụng và bộ gõ vẫn thấy đúng.
    const int n = ToUnicodeEx(k.vkCode, k.scanCode, state, buf, 4, 0x4, GetKeyboardLayout(0));
    return n == 1 && buf[0] >= 0x20 && buf[0] != 0x7F ? char16_t(buf[0]) : 0;
}

class Bypass final : public QAbstractNativeEventFilter {
public:
    explicit Bypass(QWidget* t) : target(t)
    {
        hook();
        QCoreApplication::instance()->installNativeEventFilter(this); // ~QAbstractNativeEventFilter tự gỡ
    }
    ~Bypass() override
    {
        if (hook_)
            UnhookWindowsHookEx(hook_);
    }
    Bypass(const Bypass&) = delete;
    Bypass& operator=(const Bypass&) = delete;

    // SetWindowsHookEx luôn chèn vào đầu chuỗi hook: hook đặt sau hook của bộ gõ được gọi trước nó.
    void hook()
    {
        if (hook_)
            UnhookWindowsHookEx(hook_);
        hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, &Bypass::proc, GetModuleHandleW(nullptr), 0);
    }

    bool nativeEventFilter(const QByteArray& type, void* message, qintptr*) override
    {
        if ((type != "windows_generic_MSG" && type != "windows_dispatcher_MSG") || !target)
            return false;
        const auto* m = static_cast<const MSG*>(message);
        const Verdict v = rec.onMessage(m->message, m->wParam, m->lParam);
        if (!v.replay.isEmpty())
            replay(v.replay);
        if (v.rehook)
            hook();
        return v.drop;
    }

    QPointer<QWidget> target;
    KeyReconciler rec;

private:
    static LRESULT CALLBACK proc(int code, WPARAM wParam, LPARAM lParam);

    // Giao như phím thật: tới QWindow (bộ lọc của ứng dụng, UiProbe thấy), Qt chuyển cho widget đang có focus.
    void replay(const QString& s)
    {
        QWindow* w = target->window()->windowHandle();
        QObject* to = w ? static_cast<QObject*>(w) : target.data();
        for (const QChar c : s) {
            const int key = c.toUpper().unicode(); // mã phím Qt của ký tự Latin-1 in được trùng mã chữ hoa của nó
            QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, QString(c));
            QCoreApplication::sendEvent(to, &press);
            QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, QString(c));
            QCoreApplication::sendEvent(to, &release);
        }
    }

    HHOOK hook_ = nullptr;
};

Bypass* g_bypass = nullptr;

// Hook LL chạy trên luồng đã đặt nó (luồng UI) khi luồng đó lấy tin nhắn; phải trả về nhanh (LowLevelHooksTimeout).
LRESULT CALLBACK Bypass::proc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION && g_bypass && g_bypass->target) {
        const auto& k = *reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        // Chỉ khi cửa sổ của ô đang ở trước: phím khác đi tới ứng dụng khác.
        if (GetForegroundWindow() == reinterpret_cast<HWND>(g_bypass->target->window()->effectiveWinId()))
            g_bypass->rec.onHook({k.vkCode, k.scanCode, k.flags, keyText(k)});
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

} // namespace

void setHookTarget(QWidget* target)
{
    if (!target) {
        delete std::exchange(g_bypass, nullptr);
    } else if (!g_bypass) {
        g_bypass = new Bypass(target);
    } else if (g_bypass->target != target) {
        g_bypass->target = target;
        g_bypass->rec.reset();
    }
}

} // namespace trion::ui::ime
