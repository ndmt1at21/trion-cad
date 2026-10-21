# Spike S6 · Bộ gõ tiếng Việt trong ô dòng lệnh (UIA-106)

Câu hỏi (05 §21, S6): gõ tiếng Việt đúng 100% trong dòng lệnh với Unikey, EVKey, bộ gõ Windows, bộ gõ macOS,
EVKey/OpenKey macOS, ibus và fcitx5 (Bamboo, Unikey); khi **chờ tên lệnh** thì bỏ qua bộ gõ để "lm", "dd", "aa", "ow"
ra `LM`, `DD`, `AA`, `OW` (07.1 §8, §13.6). Mã: `app/core/ime/ImeBypass.{h,cpp}`, `ImeBypass_win.cpp`,
`app/widgets/cmdline/CommandInput.{h,cpp}`.

## 1. Bộ gõ đưa chữ vào ứng dụng thế nào

Có hai họ, khác hẳn nhau về cách chặn:

| Họ | Bộ gõ | Đường đi của phím | Ứng dụng nhận |
|---|---|---|---|
| **Input method của hệ điều hành** | Bộ gõ tiếng Việt Telex/VNI của Windows (TSF); bộ gõ Telex, VNI, Simple Telex của macOS (Input Sources); ibus, fcitx5 với Bamboo, Unikey (mọi chế độ: preedit, surrounding text, forward key) | Hệ điều hành → plugin input method của Qt hỏi ô `Qt::ImEnabled` → nếu bật, phím vào bộ gõ, ô nhận `QInputMethodEvent` (preedit rồi commit, hoặc commit kèm phần thay chữ quanh con trỏ) | `QInputMethodEvent` |
| **Hook / event tap chèn phím** | Unikey, EVKey, OpenKey trên Windows; EVKey, OpenKey trên macOS | Bộ gõ chặn phím ở tầng hệ thống (Windows: `WH_KEYBOARD_LL`; macOS: `CGEventTap`), **nuốt phím vật lý vừa gõ** rồi tự chèn Backspace và chữ có dấu như thể người dùng gõ | `QKeyEvent` (Backspace + chữ có dấu) |

Chi tiết họ thứ hai, đọc từ mã nguồn công khai của OpenKey (Unikey, EVKey đóng mã; cùng họ theo mô tả công khai và
hành vi, **runner xác nhận**):

- Windows (`Sources/OpenKey/win32/OpenKey/OpenKey/OpenKey.cpp`): `SetWindowsHookEx(WH_KEYBOARD_LL, …)`; phím được biến
  đổi thì hook `return -1` (nuốt); Backspace qua `SendInput` với `wVk = VK_BACK, wScan = 0`; chữ qua
  `KEYEVENTF_UNICODE` (`wVk = 0`, `wScan` = mã UTF-16), ứng dụng nhận `WM_KEYDOWN` với `wParam = VK_PACKET`; đánh dấu sự
  kiện của mình bằng `dwExtraInfo = 1`. Chế độ dán (macro, khôi phục) đặt chữ vào clipboard rồi chèn Shift+Insert.
- macOS (`Sources/OpenKey/macOS/ModernKey/OpenKey.mm`): nguồn sự kiện `CGEventSourceCreate(kCGEventSourceStatePrivate)`;
  phím biến đổi thì callback của tap `return NULL` (nuốt); chèn bằng `CGEventTapPostEvent`: Backspace keycode 51, chữ
  qua keycode 0 + `CGEventKeyboardSetUnicodeString`.
- Microsoft: `SendInput` với `KEYEVENTF_UNICODE` tạo `WM_KEYDOWN` `VK_PACKET`; hook LL thấy cờ `LLKHF_INJECTED` với sự
  kiện chèn; `SetWindowsHookEx` luôn chèn hook vào **đầu** chuỗi; hook LL quá `LowLevelHooksTimeout` bị bỏ qua (từ
  Windows 7 bị gỡ âm thầm).

Hệ quả quan trọng: với họ hook, phím vật lý gây biến đổi **không bao giờ tới ứng dụng**. Bỏ các sự kiện chèn (lọc theo
`VK_PACKET`, mã quét 0) như dự kiến ban đầu của task thì "aa" ra `A` (mất phím thứ hai), "dd" ra `D`. Muốn ra `AA` phải
thấy phím vật lý **trước** bộ gõ.

## 2. Cách lọc đã chọn

1. **Họ input method, mọi nền tảng**: khi chờ tên lệnh, `CommandInput` trả `Qt::ImEnabled = false` và tắt
   `Qt::WA_InputMethodEnabled`. Phát hiện của spike: `QLineEdit::inputMethodQuery(ImEnabled)` trả `isEnabled() &&
   !isReadOnly()`, **không** theo `WA_InputMethodEnabled`, nên chỉ tắt thuộc tính thì bộ gõ vẫn nhận phím (test đã bắt
   được); phải override `inputMethodQuery`. Đổi thuộc tính khi ô đang focus làm Qt commit phần đang soạn và gọi
   `QInputMethod::update(Qt::ImEnabled)`, plugin (windows/TSF, cocoa, ibus, fcitx5-qt) hỏi lại và thôi đưa phím cho bộ
   gõ: ô nhận thẳng phím vật lý. macOS, Linux không cần mã riêng (không có `ImeBypass_mac/linux.cpp`).
2. **Họ hook trên Windows** (`ImeBypass_win.cpp`): khi ô chờ tên lệnh **và đang có focus**, đặt hook `WH_KEYBOARD_LL` của
   ta. Đặt sau bộ gõ nên được gọi **trước** hook bộ gõ: ghi mọi phím (vk, mã quét, cờ, ký tự theo `ToUnicodeEx` cờ 0x4,
   không đụng phím chết) rồi chuyển tiếp, **không chặn**. Bộ lọc MSG (`QAbstractNativeEventFilter`) đối chiếu từng
   `WM_KEYDOWN/UP` với nhật ký (`KeyReconciler`, hàm thuần):
   - phím có chữ hook thấy mà MSG bỏ qua = bộ gõ đã nuốt → giao lại ký tự đó cho ô (`QKeyEvent` tới `QWindow`) **ngay
     trước** MSG kế tiếp, nên giữ đúng thứ tự cả khi luồng UI trễ;
   - MSG dạng chèn (`VK_PACKET`, hoặc mã quét 0) → bỏ, kể cả đợt chèn tới sau vài phím thật và chèn không nuốt (gõ tắt
     khi nhấn Space). Phím chèn có mã quét thật (TeamViewer, UltraViewer, bàn phím ảo) vẫn qua;
   - chữ của bộ gõ tới mà không có phím bị nuốt ngay trước = hook bộ gõ đã lên trước hook của ta (bộ gõ khởi động lại)
     → mất một phím, **đặt lại hook** để lần sau hook của ta đứng đầu;
   - hook chưa ghi được gì (không đặt được) → không bỏ gì, bộ gõ chạy như thường (thà thấy "Â" còn hơn mất phím).
   Hook chỉ tồn tại khi TrionCAD ở trước và ô dòng lệnh có focus ở chế độ chờ tên lệnh; mất focus là gỡ.
3. **Họ event tap trên macOS (EVKey, OpenKey)**: **chưa lọc**. Lọc theo nguồn sự kiện (nguồn private, PID khác) chỉ bỏ
   được phần chèn, phím vật lý đã bị tap nuốt nên vẫn mất phím ("dd" ra `D`), tệ hơn để bộ gõ đổi chữ. Thấy phím vật lý
   trước tap cần event tap riêng, tức quyền Input Monitoring: không xin quyền đó mặc định. Phương án cho mốc macOS alpha
   (T26–27), cần runner: (a) khi nhận đợt chèn, đọc phím đang nhấn ở tầng HID (`CGEventSourceKeyState(
   kCGEventSourceStateHIDSystemState, …)`) để dựng lại phím bị nuốt rồi dùng chung `KeyReconciler`; (b) tùy chọn bật event
   tap (người dùng tự cấp quyền); (c) hướng dẫn dùng bộ gõ Telex/VNI có sẵn của macOS (đã bỏ qua được bằng cách 1).
4. **Commit NFC, mọi nền tảng** (`ime::nfcCommit`): commit chuẩn hóa NFC; bộ gõ gửi tổ hợp NFD tách lượt (đã commit "a",
   lượt sau gửi riêng dấu U+0302) thì kéo ký tự trước con trỏ vào phần thay rồi chuẩn hóa, trong một
   `QInputMethodEvent` (giữ undo, một lần `textEdited`). Preedit hiện tại chỗ; bộ gõ không gửi định dạng thì ô tự gạch
   chân. Khi chờ tên lệnh, chữ ra hoa (`LM`).

## 3. Đã kiểm tự động (ctest: dev, asan-ubsan, release trên Linux; win chạy trên Windows)

- `ime_bypass_test`: `nfcCommit` (NFC, NFD, dấu rời, dấu chồng, phần thay sẵn); `KeyReconciler` với MSG giả: phím thật
  đi qua, "dd", "aa", "ow", "lm" (gõ tắt) theo đúng chuỗi sự kiện OpenKey (nuốt phím, Backspace mã quét 0, `VK_PACKET`)
  ra đúng phím đã gõ, chèn không nuốt, hook bộ gõ đứng trước (đặt lại hook), không có hook, phím điều khiển từ xa,
  Alt+Tab, nhật ký tràn; **10.000 chuỗi phím ngẫu nhiên ở 30 phím/s** (thời gian giả lập: nhấn mỗi 33 ± 8 ms, giữ
  40–120 ms nên phím chồng nhau; bộ gõ kiểu Telex nuốt và chèn, đợt chèn xếp sau 0–12 sự kiện đã chờ; hook và MSG xen
  kẽ ngẫu nhiên; 5% phím chèn từ xa): ô nhận đúng chuỗi đã gõ, không mất, không lặp.
- `cmdline_command_input_test`: **134 chữ** (67 thường, 67 hoa) × Telex, VNI × 4 cách đưa chữ (preedit rồi commit NFC;
  preedit rồi commit NFD; không preedit, commit NFD thay chữ trước con trỏ; dấu rời) = 1.072 ca, NFC đúng 100%; preedit
  giữa văn bản, tự gạch chân; chờ tên lệnh: "lm", "dd", "aa", "ow" ra `LM`, `DD`, `AA`, `OW` (`ImEnabled` false); tắt
  tùy chọn (áp ngay qua `Store::changed`) hoặc hết chờ tên lệnh thì ra "lm", "đ", "â", "ơ"; Ctrl+A vẫn là phím tắt;
  10.000 chuỗi ngẫu nhiên qua ô (bỏ qua bộ gõ, Telex, VNI, mọi cách đưa chữ; 1.000 chuỗi ở bản sanitizer vì QLineEdit
  chậm ~100 lần); độ trễ ký tự hiện trong ô (UiProbe `typing`, ngưỡng 16 ms p99 kiểm ở release).
- Test đóng vai plugin input method (ô `ImEnabled` thì phím vào bộ gõ giả, không thì `QKeyEvent` thẳng), như cách
  plugin thật của Qt xử lý; bộ gõ giả dựng chữ theo luật Telex/VNI từng phím.

Số đo `typing` (300 mẫu, offscreen, máy dev dùng chung đang tải): từ phím tới hết khung kế tiếp, p50 / p99.

| Bản | Phím (bỏ qua bộ gõ) | Commit của bộ gõ (`QInputMethodEvent`) |
|---|---|---|
| Linux release | 0,13 / 1,93 ms | 0,63 / 5,45 ms |
| Windows (win, không tối ưu) | 0,21 / 0,84 ms | 0,71 / 8,29 ms |

Độ trễ thêm của hook LL và bộ lọc MSG (Windows) chưa đo được ở đây: cần phím thật (runner).

## 4. Cần runner tự host (hoãn)

Máy CI không có bộ gõ thật và không được bơm phím vào desktop. Cần runner Windows/macOS/Linux có cài bộ gõ, harness bơm
phím vật lý (Windows `SendInput` với `KEYEVENTF_SCANCODE`, không cờ chèn thì phải qua driver ảo như Interception hoặc máy
ảo; macOS `CGEventPost` ở `kCGHIDEventTap`; Linux XTest/uinput), chạy ngoài ctest mặc định (task gốc dự kiến
`app/shell/tests/ime/`):

1. 134 chữ × Telex, VNI với Unikey, EVKey, bộ gõ Windows; bộ gõ macOS, EVKey, OpenKey; ibus và fcitx5 (Bamboo, Unikey).
2. "lm", "dd", "aa", "ow" khi chờ tên lệnh với từng bộ gõ trên, gồm: thứ tự hook (bộ gõ khởi động trước/sau TrionCAD),
   bộ gõ chạy quyền admin, EVKey/Unikey chế độ dán clipboard, gõ tắt, phím chết (bố cục US-International).
3. Xác nhận các giả định: MSG giữ đúng thứ tự phát của hook; Unikey, EVKey dùng `VK_PACKET` và Backspace mã quét 0 như
   OpenKey; không có bộ gõ nào chèn chữ có mã quét thật; ibus-bamboo chế độ XTest vẫn nhận phím qua ibus.
4. Độ trễ ký tự với bộ gõ thật trên máy tầm trung (QA-011).

## 5. Rủi ro

- Hook `WH_KEYBOARD_LL` toàn hệ thống: phần mềm diệt virus có thể cảnh báo (cùng loại với AutoHotkey, phím tắt
  Discord); luồng UI khựng quá `LowLevelHooksTimeout` thì Windows gỡ hook âm thầm (có đặt lại hook khi phát hiện, mất
  một phím). DoD-U giữ luồng UI không chặn quá 2 ms.
- Bộ gõ chèn chữ có mã quét thật, hoặc gửi `WM_CHAR` thẳng (không qua `SendInput`): không lọc được, chữ của bộ gõ hiện ra.
- Công cụ điều khiển từ xa chèn chữ qua `VK_PACKET` khi đang chờ tên lệnh: bị bỏ như chữ của bộ gõ.
- macOS EVKey/OpenKey: chưa bỏ qua được (mục 2.3).
- Chữ hoa khi chờ tên lệnh: biểu thức LISP gõ ở dấu nhắc lệnh ("(princ \"abc\")") phải tắt chế độ chờ tên lệnh trước
  (UIA-107 tắt khi gặp "(").
- Ô khác của Qt (QLineEdit thường) muốn bỏ qua bộ gõ cũng phải override `inputMethodQuery(ImEnabled)`, không chỉ tắt
  `WA_InputMethodEnabled`.

## 6. Kết luận cho MTEXT

Trình soạn MTEXT cần bộ gõ **bật** (gõ tiếng Việt), nên không dùng phần bỏ qua; dùng lại `ime::nfcCommit` trong
`inputMethodEvent` của trình soạn (QTextEdit hay ô tự vẽ) để mọi commit, kể cả NFD tách lượt, vào bản vẽ ở dạng NFC, và
cách tự gạch chân preedit của `CommandInput`. Trình soạn tự vẽ phải trả `Qt::ImEnabled`, `ImCursorRectangle`,
`ImSurroundingText`, `ImCursorPosition` đúng để bộ gõ kiểu surrounding text (ibus, macOS) thay được chữ quanh con trỏ.
Nếu MTEXT có ô nhập tên (tên kiểu chữ, lệnh trong trình soạn) cần bỏ qua bộ gõ thì dùng `CommandInput` hoặc cùng cách:
override `inputMethodQuery(ImEnabled)` + `ime::setHookTarget`.
