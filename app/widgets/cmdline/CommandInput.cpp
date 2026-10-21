#include "app/widgets/cmdline/CommandInput.h"

#include "app/core/ime/ImeBypass.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/ThemeManager.h"

#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QTextCharFormat>

#include <algorithm>

namespace trion::ui {

CommandInput::CommandInput(QWidget* parent) : QLineEdit(parent)
{
    setFrame(false); // khung, nền, viền thuộc .cmd-in (UIA-107)
    QPalette pal = palette();
    pal.setBrush(QPalette::Base, Qt::transparent);
    setPalette(pal);
    setAccessibleName(tr("Dòng lệnh"));
    setFont(theme::font(theme::TextRole::Mono));
    connect(&theme::ThemeManager::instance(), &theme::ThemeManager::changed, this,
            [this] { setFont(theme::font(theme::TextRole::Mono)); });
    connect(&settings::Store::instance(), &settings::Store::changed, this, [this](const QString& key) {
        if (key == QLatin1StringView(kImeBypass.key))
            updateBypass();
    });
    updateBypass();
}

CommandInput::~CommandInput()
{
    if (hooked_)
        ime::setHookTarget(nullptr);
}

void CommandInput::setAwaitingCommand(bool on)
{
    awaiting_ = on;
    updateBypass();
}

bool CommandInput::imeBypassed() const
{
    return awaiting_ && settings::Store::instance().get(kImeBypass);
}

QVariant CommandInput::inputMethodQuery(Qt::InputMethodQuery query) const
{
    if (query == Qt::ImEnabled && imeBypassed())
        return false;
    return QLineEdit::inputMethodQuery(query);
}

void CommandInput::updateBypass()
{
    const bool on = imeBypassed();
    // Ô đang có focus thì Qt commit phần đang soạn rồi báo bộ gõ (QInputMethod::update(Qt::ImEnabled)); plugin hỏi
    // lại ImEnabled (inputMethodQuery ở trên) và thôi đưa phím cho TSF/IMM, bộ gõ macOS, ibus, fcitx5.
    setAttribute(Qt::WA_InputMethodEnabled, !on);
    const bool hook = on && hasFocus(); // bộ gõ kiểu hook của Windows chỉ cần lọc khi ô đang nhận phím
    if (hook != hooked_) {
        hooked_ = hook;
        ime::setHookTarget(hook ? this : nullptr);
    }
}

void CommandInput::focusInEvent(QFocusEvent* e)
{
    QLineEdit::focusInEvent(e);
    updateBypass();
}

void CommandInput::focusOutEvent(QFocusEvent* e)
{
    QLineEdit::focusOutEvent(e);
    updateBypass();
}

void CommandInput::keyPressEvent(QKeyEvent* e)
{
    const QString upper = e->text().toUpper();
    if (!imeBypassed() || upper == e->text() || (e->modifiers() & (Qt::ControlModifier | Qt::MetaModifier))) {
        QLineEdit::keyPressEvent(e);
        return;
    }
    QKeyEvent up(e->type(), e->key(), e->modifiers(), e->nativeScanCode(), e->nativeVirtualKey(), e->nativeModifiers(),
                 upper, e->isAutoRepeat(), e->count(), e->device());
    QLineEdit::keyPressEvent(&up);
    e->setAccepted(up.isAccepted());
}

void CommandInput::inputMethodEvent(QInputMethodEvent* e)
{
    QList<QInputMethodEvent::Attribute> attrs = e->attributes();
    const QString& pre = e->preeditString();
    // Preedit gạch chân tại chỗ: bộ gõ (hoặc plugin của Qt) không gửi định dạng thì tự gạch.
    if (!pre.isEmpty() && std::none_of(attrs.cbegin(), attrs.cend(), [](const QInputMethodEvent::Attribute& a) {
            return a.type == QInputMethodEvent::TextFormat;
        })) {
        QTextCharFormat f;
        f.setFontUnderline(true);
        attrs.append({QInputMethodEvent::TextFormat, 0, int(pre.size()), f});
    }
    const ime::Commit c = ime::nfcCommit(e->commitString(), e->replacementStart(), e->replacementLength(),
                                         QStringView(text()).left(cursorPosition()));
    QInputMethodEvent ev(pre, attrs);
    ev.setCommitString(c.text, c.start, c.length);
    QLineEdit::inputMethodEvent(&ev);
    e->setAccepted(ev.isAccepted());
}

} // namespace trion::ui
