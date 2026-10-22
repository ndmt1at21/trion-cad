#include "app/shell/start/ThumbnailQueue.h"

#include "app/core/UiExecutor.h"

namespace trion::shell {

ThumbnailQueue::ThumbnailQueue(QObject* parent) : QObject(parent) {}

ThumbnailQueue::~ThumbnailQueue()
{
    cancelAll();
}

void ThumbnailQueue::setEmbeddedSource(EmbeddedSource source)
{
    embedded_ = std::move(source);
    reset();
}

void ThumbnailQueue::setRenderSource(RenderSource source)
{
    render_ = std::move(source);
    reset();
}

void ThumbnailQueue::reset()
{
    cancelAll();
    cache_.clear();
    failed_.clear();
}

const QImage* ThumbnailQueue::image(const QString& path, QSize size) const
{
    return cache_.object({path, size});
}

void ThumbnailQueue::request(const QStringList& paths, QSize size)
{
    const QSet<QString> wanted(paths.begin(), paths.end());
    for (auto it = running_.begin(); it != running_.end();) {
        if (wanted.contains(it.key()) && it->size == size) {
            ++it;
            continue;
        }
        it->cancel.cancel(); // rời vùng nhìn hoặc đổi cỡ
        it = running_.erase(it);
    }
    if (size.isEmpty() || (!embedded_ && !render_))
        return;
    for (const QString& path : paths)
        if (!running_.contains(path) && !failed_.contains(path) && !cache_.contains({path, size}))
            start(path, size);
}

void ThumbnailQueue::cancelAll()
{
    for (Job& job : running_)
        job.cancel.cancel();
    running_.clear();
}

void ThumbnailQueue::forget(const QString& path)
{
    failed_.remove(path);
    for (const Key& k : cache_.keys())
        if (k.path == path)
            cache_.remove(k);
}

void ThumbnailQueue::start(const QString& path, QSize size)
{
    const std::uint64_t id = ++nextId_;
    const sched::CancelToken cancel = running_.insert(path, {id, size, {}})->cancel.token();
    sched::submit(sched::Priority::Background, [rx = ui::Receiver(this), this, path, id, size, cancel,
                                                embedded = embedded_, render = render_] {
        QImage image;
        try {
            if (embedded && !cancel.cancelled())
                image = embedded(path, cancel);
            if (image.isNull() && render && !cancel.cancelled())
                image = render(path, size, cancel);
            if (!image.isNull() && !cancel.cancelled()) { // thu về cỡ ô ở nền: luồng UI vẽ 1:1
                if (image.size() != size)
                    image = image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                image.convertTo(QImage::Format_ARGB32_Premultiplied);
            }
        } catch (...) { // nguồn lỗi (tệp hỏng): như ảnh rỗng
            image = QImage();
        }
        ui::post(rx, [this, path, id, size, cancelled = cancel.cancelled(), image = std::move(image)]() mutable {
            finished(path, id, size, std::move(image), cancelled);
        });
    });
}

void ThumbnailQueue::finished(const QString& path, std::uint64_t id, QSize size, QImage image, bool cancelled)
{
    const auto it = running_.find(path);
    if (it == running_.end() || it->id != id)
        return; // đã hủy hoặc đã thay bằng việc khác
    running_.erase(it);
    if (cancelled)
        return;
    if (image.isNull()) {
        failed_.insert(path);
    } else {
        const qsizetype bytes = image.sizeInBytes();
        cache_.insert({path, size}, new QImage(std::move(image)), bytes);
    }
    emit ready(path);
}

} // namespace trion::shell
