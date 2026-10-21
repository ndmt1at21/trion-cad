#include "app/widgets/progress/ProgressPresenter.h"

#include "app/core/i18n/Translator.h"
#include "app/core/i18n/UiFormat.h"

#include <QCursor>
#include <QGuiApplication>
#include <QScreen>

#include <algorithm>
#include <cmath>
#include <utility>

namespace trion::ui {

using jobs::Clock;
using jobs::JobId;
using jobs::JobInfo;
using jobs::State;

namespace {

std::chrono::milliseconds until(Clock::time_point when, Clock::time_point now)
{
    return std::chrono::ceil<std::chrono::milliseconds>(when - now); // không sớm hơn mốc
}

} // namespace

ProgressItemModel::ProgressItemModel(ProgressPresenter* presenter)
    : QAbstractListModel(presenter), presenter_(presenter)
{
    // Model không nhận LanguageChange: đổi ngôn ngữ thì báo chữ của mọi hàng đổi (Translator.h).
    if (auto* t = i18n::Translator::instance())
        connect(t, &i18n::Translator::languageChanged, this, [this] {
            if (!rows_.empty())
                emit dataChanged(index(0), index(int(rows_.size()) - 1), {Qt::DisplayRole});
        });
}

int ProgressItemModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(rows_.size());
}

QVariant ProgressItemModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const JobId id = rows_[std::size_t(index.row())];
    const JobInfo* info = presenter_->job(id);
    if (!info) // lô báo kết thúc đã tới, hàng bỏ ở lần áp kế (≤ 1 khung)
        return {};
    switch (role) {
    case Qt::DisplayRole:
        return ProgressPresenter::label(info->name, ProgressPresenter::percent(info->progress));
    case JobIdRole:
        return QVariant::fromValue(quint64(id));
    case NameRole:
        return info->name;
    case KindRole:
        return info->kind;
    case PercentRole:
        return ProgressPresenter::percent(info->progress);
    case CancellingRole:
        return presenter_->cancelling(id);
    default:
        return {};
    }
}

void ProgressItemModel::sync(const std::vector<JobId>& rows, bool changed)
{
    // Cả hai danh sách tăng dần theo id: bớt hàng không còn (từ cuối), rồi chèn hàng mới đúng vị trí.
    for (int r = int(rows_.size()) - 1; r >= 0; --r)
        if (!std::binary_search(rows.begin(), rows.end(), rows_[std::size_t(r)])) {
            beginRemoveRows({}, r, r);
            rows_.erase(rows_.begin() + r);
            endRemoveRows();
        }
    for (std::size_t r = 0; r < rows.size(); ++r)
        if (r >= rows_.size() || rows_[r] != rows[r]) {
            beginInsertRows({}, int(r), int(r));
            rows_.insert(rows_.begin() + qsizetype(r), rows[r]);
            endInsertRows();
        }
    if (changed && !rows_.empty())
        emit dataChanged(index(0), index(int(rows_.size()) - 1));
}

ProgressPresenter::ProgressPresenter(jobs::BackgroundJobs& jobs, QObject* parent) : QObject(parent), jobs_(jobs)
{
    for (QTimer* t : {&mark_, &frame_}) {
        t->setSingleShot(true);
        t->setTimerType(Qt::PreciseTimer);
    }
    // Cả hai qua request(): timer của Windows có thể tới sớm dưới 1 ms, request() hẹn lại phần còn thiếu của khung.
    connect(&mark_, &QTimer::timeout, this, &ProgressPresenter::request);
    connect(&frame_, &QTimer::timeout, this, &ProgressPresenter::request);
    if (const QScreen* s = QGuiApplication::primaryScreen(); s && s->refreshRate() > 1)
        frameTime_ = std::chrono::nanoseconds(qint64(1e9 / s->refreshRate()));
    connect(&jobs_, &jobs::BackgroundJobs::updated, this, &ProgressPresenter::onBatch);
}

ProgressPresenter::~ProgressPresenter()
{
    if (busy_)
        QGuiApplication::restoreOverrideCursor();
}

const ProgressPresenter::Entry* ProgressPresenter::find(JobId id) const
{
    const auto it =
        std::lower_bound(entries_.begin(), entries_.end(), id, [](const Entry& e, JobId v) { return e.info.id < v; });
    return it != entries_.end() && it->info.id == id ? &*it : nullptr;
}

ProgressPresenter::Entry* ProgressPresenter::find(JobId id)
{
    return const_cast<Entry*>(std::as_const(*this).find(id));
}

const JobInfo* ProgressPresenter::job(JobId id) const
{
    const Entry* e = find(id);
    return e ? &e->info : nullptr;
}

bool ProgressPresenter::shown(JobId id) const
{
    const Entry* e = find(id);
    return e && e->shown && !e->ended;
}

bool ProgressPresenter::cancelling(JobId id) const
{
    const Entry* e = find(id);
    return e && (e->cancelling || e->info.cancelRequested);
}

JobId ProgressPresenter::primary() const
{
    for (const Entry& e : entries_)
        if (e.shown && !e.ended)
            return e.info.id;
    return 0;
}

int ProgressPresenter::shownCount() const
{
    return int(std::count_if(entries_.begin(), entries_.end(), [](const Entry& e) { return e.shown && !e.ended; }));
}

void ProgressPresenter::cancel(JobId id)
{
    Entry* e = find(id);
    if (!e || e->cancelling)
        return;
    e->cancelling = true;
    dataDirty_ = dataDirty_ || e->shown;
    jobs_.cancel(id);
    request();
}

QString ProgressPresenter::label(const QString& name, int percent)
{
    return tr("%1 · %2%").arg(name, i18n::formatCount(percent));
}

int ProgressPresenter::percent(double progress)
{
    if (!(progress > 0)) // cả NaN
        return 0;
    // 1e-6: 0,63 lưu theo đơn vị nguyên của NT-013 thành 0,6299999… vẫn là 63%
    return int(std::floor(std::min(progress, 1.0) * 100 + 1e-6));
}

void ProgressPresenter::onBatch(const std::vector<JobInfo>& batch)
{
    for (const JobInfo& i : batch) {
        const auto it = std::lower_bound(entries_.begin(), entries_.end(), i.id,
                                         [](const Entry& e, JobId v) { return e.info.id < v; });
        const bool known = it != entries_.end() && it->info.id == i.id;
        if (i.state != State::Running) {
            if (known) {
                dataDirty_ = dataDirty_ || it->shown;
                entries_.erase(it);
            }
            emit finished(i);
            continue;
        }
        if (known) {
            dataDirty_ = dataDirty_ || it->shown;
            it->info = i;
        } else {
            entries_.insert(it, Entry{i});
        }
    }
    request();
}

void ProgressPresenter::request()
{
    const auto now = Clock::now();
    if (now - lastApply_ >= frameTime_)
        apply();
    else if (!frame_.isActive()) // khung này đã áp: gộp vào lần áp ở khung kế
        frame_.start(until(lastApply_ + frameTime_, now));
}

void ProgressPresenter::apply()
{
    frame_.stop();
    const auto now = Clock::now();
    const jobs::Timing& t = jobs_.timing();
    auto next = Clock::time_point::max();
    bool busy = false;
    std::vector<JobId> rows;
    for (Entry& e : entries_) {
        const auto elapsed = now - e.info.startedAt;
        // Qua mốc lần đầu: hỏi lại NT-013. Việc vừa dừng mà lô báo xong chưa tới thì không hiện gì.
        if (!e.ended && ((!e.busy && elapsed >= t.busyCursor) || (!e.shown && elapsed >= t.showProgress))) {
            const auto live = jobs_.info(e.info.id);
            e.ended = !live || live->state != State::Running;
        }
        if (e.ended)
            continue;
        e.busy = e.busy || elapsed >= t.busyCursor;
        e.shown = e.shown || elapsed >= t.showProgress;
        busy = busy || e.busy;
        if (e.shown)
            rows.push_back(e.info.id);
        else
            next = std::min(next, e.info.startedAt + (e.busy ? t.showProgress : t.busyCursor));
    }
    model_.sync(rows, std::exchange(dataDirty_, false));
    setBusy(busy);
    if (next != Clock::time_point::max())
        mark_.start(until(next, now));
    else
        mark_.stop();
    emit changed();
    lastApply_ = Clock::now(); // khung tính từ lúc áp xong: hai lần áp luôn cách nhau ≥ một khung
}

void ProgressPresenter::setBusy(bool busy)
{
    if (busy == busy_)
        return;
    busy_ = busy;
    if (busy)
        QGuiApplication::setOverrideCursor(Qt::BusyCursor); // mũi tên kèm đồng hồ: vẫn thao tác được
    else
        QGuiApplication::restoreOverrideCursor();
    emit busyChanged(busy);
}

} // namespace trion::ui
