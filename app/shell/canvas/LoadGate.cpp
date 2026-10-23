#include "app/shell/canvas/LoadGate.h"

#include "app/core/actions/ActionRegistry.h"
#include "app/core/UiExecutor.h"
#include "app/shell/canvas/CanvasWidget.h"
#include "app/widgets/message/Messages.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QWidget>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace trion::shell {

namespace {

struct Command {
    QString name; // viết hoa, không tiền tố
    bool transparent = false;
};

// Tên lệnh của chuỗi macro, như CommandRouter::submit: bỏ ^C, tiền tố ' _ . (thứ tự bất kỳ), đến dấu cách hoặc ';'.
Command parse(QStringView m)
{
    while (m.startsWith(u"^C", Qt::CaseInsensitive))
        m = m.sliced(2);
    Command c;
    while (!m.isEmpty() && (m.front() == u'\'' || m.front() == u'_' || m.front() == u'.')) {
        c.transparent |= m.front() == u'\'';
        m = m.sliced(1);
    }
    qsizetype end = 0;
    while (end < m.size() && m[end] != u' ' && m[end] != u';')
        ++end;
    c.name = m.first(end).toString().toUpper();
    return c;
}

} // namespace

LoadGate::LoadGate(jobs::BackgroundJobs& jobs, CanvasWidget& canvas, QObject* parent)
    : QObject(parent), jobs_(jobs), canvas_(canvas)
{
    connect(&jobs_, &jobs::BackgroundJobs::updated, this, [this](const std::vector<jobs::JobInfo>& changed) {
        if (!job_)
            return;
        const auto it = std::ranges::find(changed, job_, &jobs::JobInfo::id);
        if (it != changed.end() && it->state != jobs::State::Running)
            finish(it->state);
    });
}

LoadGate::~LoadGate()
{
    if (job_)
        jobs_.cancel(job_);
    if (tip_)
        tip_->hide();
}

bool LoadGate::writesByDefault(const QString& command)
{
    // Lệnh mở tài liệu, giao diện khác: không đụng bản vẽ đang nạp.
    static constexpr QLatin1StringView other[] = {"NEW"_L1, "QNEW"_L1, "WORKSPACE"_L1, "QUICKCALC"_L1};
    if (std::ranges::any_of(other, [&](QLatin1StringView o) { return command == o; }))
        return false;
    const auto all = actions::commands();
    const auto it =
        std::ranges::find_if(all, [&](const actions::CommandInfo& c) { return command == QLatin1StringView(c.name); });
    return it != all.end() && qstrcmp(it->need, "V") != 0; // Viewer ("V") không sửa được bản vẽ
}

jobs::JobId LoadGate::start(std::uint64_t owner, LoadFn fn)
{
    if (loading()) { // một việc nạp một lúc: lô kết thúc của việc cũ tới sau, khác id nên bị bỏ qua
        jobs_.cancel(job_);
        finish(jobs::State::Cancelled);
    }
    const ui::Receiver rx(this);
    Published publish = [rx, this] { // luồng nền: chỉ chuyển về luồng UI, không chạm `this`
        ui::postLatest(rx, [this] {
            canvas_.updateScene();
            emit published();
        });
    };
    job_ = jobs_.start(tr("Đang nạp"), u"open"_s, owner,
                       [fn = std::move(fn), publish = std::move(publish)](jobs::Progress& p, sched::CancelToken c) {
                           return fn(p, c, publish);
                       });
    emit loadingChanged(true);
    emit notice(tr("Đang nạp tệp lớn ở nền. Bạn đã xem, pan, zoom được; lệnh sửa chờ đến khi nạp xong."));
    return job_;
}

bool LoadGate::cancel()
{
    return job_ && jobs_.cancel(job_);
}

void LoadGate::finish(jobs::State state)
{
    job_ = 0;
    const QString macro = std::exchange(held_, {});
    if (!macro.isEmpty())
        release();
    emit loadingChanged(false);
    emit finished(state);
    if (macro.isEmpty())
        return;
    if (state == jobs::State::Done) {
        if (sink_)
            sink_(macro);
    } else {
        emit notice(tr("Đã bỏ %1: bản vẽ chưa nạp xong.").arg(parse(macro).name));
    }
}

void LoadGate::submit(const QString& macro)
{
    const Command c = parse(macro);
    const bool wait =
        loading() && !c.transparent && !c.name.isEmpty() && (writes_ ? writes_(c.name) : writesByDefault(c.name));
    if (!wait) {
        if (sink_)
            sink_(macro);
        return;
    }
    if (held_.isEmpty())
        QCoreApplication::instance()->installEventFilter(this);
    held_ = macro;
    const QString text = tr("%1 sẽ chạy khi nạp xong. Esc để hủy.").arg(c.name);
    if (!tip_)
        tip_ = new ui::CursorTip(&canvas_);
    // Chuột ngoài vùng vẽ (gõ ở dòng lệnh): góc dưới trái vùng vẽ, cạnh dòng lệnh.
    tip_->showAt(canvas_.overlay().mouse.value_or(QPoint(0, canvas_.height())), text, ui::Severity::Info);
    emit notice(text);
    emit heldChanged(macro);
}

void LoadGate::cancelHeld()
{
    if (held_.isEmpty())
        return;
    const QString name = parse(std::exchange(held_, {})).name;
    release();
    emit notice(tr("Đã hủy %1.").arg(name));
}

void LoadGate::release()
{
    QCoreApplication::instance()->removeEventFilter(this);
    if (tip_)
        tip_->hide();
    emit heldChanged({});
}

bool LoadGate::eventFilter(QObject* watched, QEvent* e)
{
    if (e->type() != QEvent::KeyPress || held_.isEmpty())
        return false;
    const auto* k = static_cast<const QKeyEvent*>(e);
    const auto* w = qobject_cast<const QWidget*>(watched);
    if (k->key() != Qt::Key_Escape || (k->modifiers() & ~Qt::KeypadModifier) || !w || w->window() != canvas_.window())
        return false;
    cancelHeld();
    return true; // Esc này chỉ bỏ lệnh chờ: không tới dòng lệnh (bỏ vùng chọn là việc của lần Esc sau)
}

} // namespace trion::shell
