#include "app/widgets/progress/InlineProgress.h"

#include "app/theme/generated/Tokens.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"

#include <QBoxLayout>
#include <QEvent>
#include <QLabel>
#include <QPainter>
#include <QProgressBar>

#include <algorithm>

namespace trion::ui {

namespace {

theme::ThemeManager& tm()
{
    return theme::ThemeManager::instance();
}

} // namespace

// Thanh .prog: vẽ phần đã xong chính xác (fill_), còn giá trị của QProgressBar, cái trình đọc màn hình đọc, chỉ
// đổi theo bước 10%: QProgressBar::setValue phát QAccessibleValueChangeEvent mỗi lần giá trị đổi.
class StepBar final : public QProgressBar {
public:
    explicit StepBar(QWidget* parent) : QProgressBar(parent)
    {
        setRange(0, 100);
        setTextVisible(false);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setFill(double fill)
    {
        if (fill != fill_) {
            fill_ = fill;
            update();
        }
        setValue(ProgressPresenter::percent(fill) / 10 * 10);
    }

    QSize sizeHint() const override // .prog 80×6 (.sb-task), giãn theo chỗ trong hộp thoại
    {
        return {qRound(5 * tm().px(tok::sp_6)), qRound(tm().px(tok::sp_3))};
    }
    QSize minimumSizeHint() const override { return {qRound(tm().px(tok::sp_6)), qRound(tm().px(tok::sp_3))}; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        const QRectF track(0, (height() - tm().px(tok::sp_3)) / 2, width(), tm().px(tok::sp_3));
        const qreal radius = track.height() / 2; // .prog bo tròn hai đầu
        p.setBrush(tm().color(tok::c_active));
        p.drawRoundedRect(track, radius, radius);
        if (fill_ > 0) {
            QRectF chunk = track;
            chunk.setWidth(std::max(track.height(), track.width() * std::min(fill_, 1.0)));
            p.setBrush(tm().color(tok::c_accent));
            p.drawRoundedRect(chunk, radius, radius);
        }
    }

private:
    double fill_ = 0;
};

InlineProgress::InlineProgress(ProgressPresenter& presenter, jobs::JobId job, QWidget* parent)
    : QWidget(parent), presenter_(presenter), job_(job), bar_(new StepBar(this)), text_(new QLabel(this)),
      cancel_(new Button({}, Button::Kind::Danger, this))
{
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins({});
    row->addWidget(bar_, 1);
    row->addWidget(text_);
    row->addWidget(cancel_);
    setVisible(false);
    connect(cancel_, &QPushButton::clicked, this, [this] {
        if (!current_)
            return;
        cancel_->setText(tr("Đang hủy…")); // ngay, không chờ lần áp của presenter (khung kế)
        cancel_->setEnabled(false);
        presenter_.cancel(current_);
    });
    connect(&presenter_, &ProgressPresenter::changed, this, &InlineProgress::refresh);
    connect(&tm(), &theme::ThemeManager::changed, this, &InlineProgress::applyTheme);
    applyTheme();
    refresh();
}

void InlineProgress::setJob(jobs::JobId job)
{
    job_ = job;
    refresh();
}

void InlineProgress::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        refresh();
    QWidget::changeEvent(e);
}

void InlineProgress::applyTheme()
{
    layout()->setSpacing(qRound(tm().px(tok::sp_4))); // .tx-prog gap 8px
    text_->setFont(tm().font(tok::fs_s));
}

void InlineProgress::refresh()
{
    const jobs::JobId id = job_ ? (presenter_.shown(job_) ? job_ : 0) : presenter_.primary();
    const jobs::JobInfo* info = id ? presenter_.job(id) : nullptr;
    if (!info) {
        current_ = 0;
        setVisible(false);
        return;
    }
    if (id != current_) {
        current_ = id;
        bar_->setAccessibleName(info->name);
        cancel_->setAccessibleDescription(info->name);
    }
    const int others = job_ ? 0 : presenter_.shownCount() - 1;
    const auto text = [&](int percent) {
        const QString main = ProgressPresenter::label(info->name, percent);
        return others > 0 ? tr("%1 · %n tác vụ khác", nullptr, others).arg(main) : main;
    };
    const int percent = ProgressPresenter::percent(info->progress);
    text_->setText(text(percent));
    if (const QString spoken = text(percent / 10 * 10); text_->accessibleName() != spoken)
        text_->setAccessibleName(spoken); // tên tiếp cận đặt rồi thì setText không báo NameChanged mỗi lần
    bar_->setFill(info->progress);        // trước khi hiện: giá trị đầu không thành sự kiện
    const bool cancelling = presenter_.cancelling(id);
    cancel_->setText(cancelling ? tr("Đang hủy…") : tr("Hủy"));
    cancel_->setEnabled(!cancelling);
    setVisible(true);
}

} // namespace trion::ui
