#include "app/widgets/dialog/ConfirmDialog.h"

#include "app/core/i18n/UiFormat.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/dialog/DialogFrame.h"
#include "app/widgets/dialog/SuppressedPrompts.h"
#include "core/base/assert.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QLabel>

#include <utility>

using namespace Qt::StringLiterals;

namespace trion::ui {

namespace {

constexpr int kBodyWidth = 440 - 2 * 14 - 2; // TC.confirm w 440 trừ .dlg-b padding 14 và viền thẻ
constexpr int kGap = 10;                     // .stack gap

// Chữ màu token (không có vai trò QPalette nào là --c-text-2), tô lại khi đổi chủ đề.
void ink(QWidget* w, tok::ColorId id)
{
    const auto apply = [w, id] {
        QPalette p = w->palette();
        p.setColor(QPalette::WindowText, theme::ThemeManager::instance().color(id));
        w->setPalette(p);
    };
    apply();
    QObject::connect(&theme::ThemeManager::instance(), &theme::ThemeManager::changed, w, apply);
}

} // namespace

ConfirmDialog::ConfirmDialog(QString title, QString action, qint64 count)
    : title_(std::move(title)), action_(std::move(action)), count_(count)
{
    TRION_ASSERT(count_ > 0);
    TRION_ASSERT(isActionLabel(action_));
}

ConfirmDialog& ConfirmDialog::question(QString text)
{
    question_ = std::move(text);
    return *this;
}

ConfirmDialog& ConfirmDialog::detail(QString text)
{
    detail_ = std::move(text);
    return *this;
}

ConfirmDialog& ConfirmDialog::danger(bool on)
{
    danger_ = on;
    return *this;
}

ConfirmDialog& ConfirmDialog::irreversible(bool on)
{
    irreversible_ = on;
    return *this;
}

ConfirmDialog& ConfirmDialog::dontAsk(QString id)
{
    dontAsk_ = std::move(id);
    return *this;
}

bool ConfirmDialog::isActionLabel(const QString& action)
{
    if (!action.contains(u"%1"))
        return false;
    const QString rest = QString(action).remove(u"%1"_s);
    QString verb; // chỉ giữ chữ cái: bỏ '&', khoảng trắng, dấu câu
    for (const QChar c : rest)
        if (c.isLetter())
            verb += c.toLower();
    static const QStringList generic{u"ok"_s,  u"có"_s,      u"đồngý"_s,    u"xácnhận"_s, u"tiếptục"_s,
                                     u"yes"_s, u"confirm"_s, u"continue"_s, u"accept"_s};
    return !verb.isEmpty() && !generic.contains(verb);
}

DialogFrame* ConfirmDialog::ask(QWidget* parent, std::function<void(bool accepted)> done) const
{
    if (!dontAsk_.isEmpty())
        if (const std::optional<bool> remembered = SuppressedPrompts::remembered(dontAsk_)) {
            done(*remembered);
            return nullptr;
        }
    const QString count = i18n::formatCount(count_);
    auto* f = new DialogFrame(u"confirm"_s, parent);
    f->setAttribute(Qt::WA_DeleteOnClose);
    f->setModal(true);
    f->setWindowTitle(title_);

    auto* body = new QWidget;
    body->setFixedWidth(kBodyWidth);
    auto* v = new QVBoxLayout(body);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(kGap);
    if (!question_.isEmpty()) {
        QString text = question_.toHtmlEscaped(); // chữ của người gọi không thành thẻ HTML
        if (text.contains(u"%1"))
            text = text.arg(u"<b>%1</b>"_s.arg(count));
        auto* q = new QLabel(text);
        q->setTextFormat(Qt::RichText);
        q->setWordWrap(true);
        v->addWidget(q);
    }
    if (!detail_.isEmpty()) {
        auto* d = new QLabel(detail_);
        d->setTextFormat(Qt::PlainText);
        d->setWordWrap(true);
        d->setFont(theme::font(theme::TextRole::Label)); // .muted, --fs-s
        ink(d, tok::c_text_2);
        v->addWidget(d);
    }
    QCheckBox* again = nullptr;
    if (!dontAsk_.isEmpty()) {
        again = new QCheckBox(tr("Không hỏi lại"));
        again->setObjectName(u"dontAsk"_s);
        v->addWidget(again, 0, Qt::AlignLeft);
    }
    f->setBody(body);

    Button* cancel = f->addCancel();
    cancel->setKind(Button::Kind::Normal); // TC.confirm: .btn, không ghost
    cancel->setObjectName(u"cancel"_s);
    f->setConfirm(action_.arg(count), danger_ ? Button::Kind::Danger : Button::Kind::Primary);
    if (irreversible_)
        cancel->setFocus(Qt::OtherFocusReason); // DialogFrame giữ focus đã đặt trước khi hiện

    QObject::connect(f, &QDialog::finished, f,
                     [again, id = dontAsk_, title = title_, done = std::move(done)](int result) {
                         const bool yes = result == QDialog::Accepted;
                         if (again && again->isChecked())
                             SuppressedPrompts::remember(id, title, yes);
                         done(yes);
                     });
    f->open();
    return f;
}

} // namespace trion::ui
