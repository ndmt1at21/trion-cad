#pragma once
// UIA-106 · Ô nhập của dòng lệnh (UI-A06, .cmd-in input của prototype): QLineEdit chữ Mono --fs, không khung, nền
// trong suốt. Gõ tiếng Việt qua bộ gõ của hệ điều hành: preedit gạch chân tại chỗ, commit chuẩn hóa NFC (kể cả bộ
// gõ gửi tổ hợp NFD tách lượt). Khi chờ tên lệnh thì bỏ qua bộ gõ: "lm", "dd", "aa", "ow" ra LM, DD, AA, OW thay vì
// "lầm", "đ", "â", "ơ" (07.1 §8 "Gõ tiếng Việt"; tùy chọn kImeBypass, mặc định bật). Cách bỏ qua theo từng loại bộ
// gõ: app/core/ime/ImeBypass.h, docs/spikes/S6-ime.md.
//
// UIA-107 dựng khung .cmd-in (nền, viền, prompt, chip), lịch sử, tự hoàn thành quanh ô này và đặt
// setAwaitingCommand() theo trạng thái dòng lệnh. Lưu ý cho UIA-107: QLineEdit::setReadOnly(), setEchoMode() tự đặt
// lại Qt::WA_InputMethodEnabled; gọi setAwaitingCommand() lại sau các hàm này.

#include "core/base/settings/Settings.h"

#include <QLineEdit>

namespace trion::ui {

// Bỏ qua bộ gõ khi chờ tên lệnh; hộp thiết lập dòng lệnh (INPUTSEARCHOPTIONS) bật/tắt.
inline const settings::Setting<bool> kImeBypass{"cmdline/imeBypass", true};

class CommandInput : public QLineEdit {
    Q_OBJECT

public:
    // Cần ThemeManager (font Mono) và settings::Store::instance() (kImeBypass, đổi là áp ngay).
    explicit CommandInput(QWidget* parent = nullptr);
    ~CommandInput() override;

    // Đang chờ tên lệnh (dấu nhắc "Lệnh:", chưa có lệnh chạy).
    // ponytail: cờ do bên ngoài đặt; UIA-107 nối với INT-002/INT-003 và tắt cờ khi người dùng gõ biểu thức LISP "("
    // ở dấu nhắc lệnh (chữ trong chuỗi LISP không được viết hoa, cần gõ được tiếng Việt).
    void setAwaitingCommand(bool on);
    bool awaitingCommand() const { return awaiting_; }

    // Đang bỏ qua bộ gõ: chờ tên lệnh và kImeBypass bật. Khi đó chữ gõ vào ra chữ hoa theo phím vật lý.
    bool imeBypassed() const;

    // Qt::ImEnabled là false khi bỏ qua bộ gõ. QLineEdit trả ImEnabled theo isEnabled() && !isReadOnly(), không theo
    // Qt::WA_InputMethodEnabled, nên tắt thuộc tính thôi thì bộ gõ vẫn nhận phím.
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    using QLineEdit::inputMethodQuery;

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void inputMethodEvent(QInputMethodEvent* e) override;
    void focusInEvent(QFocusEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;

private:
    void updateBypass();

    bool awaiting_ = false;
    bool hooked_ = false; // là ô của ime::setHookTarget (Windows)
};

} // namespace trion::ui
