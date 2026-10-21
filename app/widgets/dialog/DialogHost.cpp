#include "app/widgets/dialog/DialogHost.h"

#include "app/widgets/dialog/DialogFrame.h"
#include "core/base/assert.h"

#include <QApplication>
#include <QJsonObject>
#include <QScreen>

#include <algorithm>
#include <memory>

using namespace Qt::StringLiterals;

namespace trion::ui {

namespace {

QString geometryKey(const QString& id)
{
    return u"dialog/"_s + id;
}

} // namespace

DialogHost::DialogHost(QWidget* mainWindow, settings::Store& store, QObject* parent)
    : QObject(parent), main_(mainWindow), store_(store)
{
    TRION_CHECK(mainWindow);
}

DialogHost::~DialogHost()
{
    for (const QPointer<DialogFrame>& f : std::as_const(open_))
        if (f)
            save(f);
}

void DialogHost::add(const QString& id, Factory make)
{
    TRION_CHECK(make && !factories_.contains(id));
    factories_.emplace(id, std::move(make));
}

QStringList DialogHost::ids() const
{
    QStringList out;
    for (const auto& entry : factories_)
        out << entry.first;
    return out;
}

DialogFrame* DialogHost::find(const QString& id) const
{
    return open_.value(id);
}

DialogFrame* DialogHost::open(const QString& id)
{
    if (DialogFrame* f = find(id)) {
        if (!f->isHiddenForPick()) {
            f->raise();
            f->activateWindow();
        }
        return f;
    }
    const auto it = factories_.find(id);
    if (it == factories_.end() || !main_)
        return nullptr;
    QWidget* parent = main_;
    if (auto* active = qobject_cast<QDialog*>(QApplication::activeWindow()); active && active->isVisible())
        parent = active; // xếp chồng: modal chặn cả hộp thoại mở nó, nằm trên nó
    DialogFrame* f = it->second(parent);
    TRION_CHECK(f && f->id() == id);
    f->setAttribute(Qt::WA_DeleteOnClose);
    open_.insert(id, f);
    connect(f, &DialogFrame::helpRequested, this, &DialogHost::helpRequested);
    connect(f, &QDialog::finished, this, [this, f, id] {
        save(f);
        open_.remove(id);
    });

    // Vị trí đã lưu; cỡ chỉ khi đổi cỡ được (cỡ cố định theo nội dung: đổi ngôn ngữ, mật độ thì theo nội dung mới).
    const QJsonObject g = store_.value(geometryKey(id)).toObject();
    QRect card(g[u"x"_s].toInt(), g[u"y"_s].toInt(), g[u"w"_s].toInt(), g[u"h"_s].toInt());
    if (!g.isEmpty() && !f->isResizable()) {
        f->adjustSize();
        card.setSize(f->cardGeometry().size());
    }
    if (!g.isEmpty() && card.isValid()) {
        QList<QRect> screens;
        for (const QScreen* s : QGuiApplication::screens())
            screens << s->availableGeometry();
        f->setCardGeometry(place(card, main_->frameGeometry(), screens));
    }
    if (f->isModal())
        f->open();
    else
        f->show();
    return f;
}

QStringList DialogHost::confirmLabelIssues() const
{
    QStringList out;
    for (const auto& [id, make] : factories_) {
        const std::unique_ptr<DialogFrame> f(make(main_));
        const Button* confirm = f ? f->confirmButton() : nullptr;
        if (!confirm)
            continue;
        QString label = confirm->text();
        label.remove(u'&');
        if (label.trimmed().compare("OK"_L1, Qt::CaseInsensitive) == 0)
            out << u"%1: «%2»"_s.arg(id, confirm->text());
    }
    return out;
}

QRect DialogHost::place(const QRect& saved, const QRect& anchor, const QList<QRect>& screens)
{
    if (screens.isEmpty() || saved.isEmpty())
        return saved;
    const auto area = [](const QRect& r) { return r.isEmpty() ? 0 : qint64(r.width()) * r.height(); };
    const QRect* screen = nullptr;
    qint64 best = 0;
    for (const QRect& s : screens)
        if (const qint64 a = area(s & saved); a > best) {
            best = a;
            screen = &s;
        }
    const bool gone = !screen; // màn hình cũ đã rút: màn hình chứa tâm cửa sổ chính
    if (gone) {
        screen = &screens.front();
        for (const QRect& s : screens)
            if (s.contains(anchor.center())) {
                screen = &s;
                break;
            }
    }
    QRect r(saved.topLeft(), saved.size().boundedTo(screen->size()));
    if (gone)
        r.moveCenter(anchor.center());
    r.moveLeft(std::clamp(r.left(), screen->left(), screen->right() - r.width() + 1));
    r.moveTop(std::clamp(r.top(), screen->top(), screen->bottom() - r.height() + 1));
    return r;
}

void DialogHost::save(const DialogFrame* frame)
{
    const QRect r = frame->cardGeometry();
    store_.setValue(geometryKey(frame->id()),
                    QJsonObject{{u"x"_s, r.x()}, {u"y"_s, r.y()}, {u"w"_s, r.width()}, {u"h"_s, r.height()}});
}

} // namespace trion::ui
