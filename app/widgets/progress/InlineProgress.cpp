#include "app/widgets/progress/InlineProgress.h"

#include "app/widgets/progress/Spinner.h"

#include "app/theme/generated/Tokens.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/controls/Controls.h"

#include <QBoxLayout>
#include <QEvent>
#include <QLabel>
#include <QPainter>
#include <QPointer>
#include <QProgressBar>

#include <algorithm>

using namespace Qt::StringLiterals;

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
    StepBar(QWidget* parent, tok::PxId thickness) : QProgressBar(parent), thickness_(thickness)
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

    QSize sizeHint() const override // .prog 80 × 6 (hộp thoại: giãn theo chỗ), .sb-task .prog 80 × 4
    {
        return {qRound(5 * tm().px(tok::sp_6)), qRound(tm().px(thickness_))};
    }
    QSize minimumSizeHint() const override { return {qRound(tm().px(tok::sp_6)), qRound(tm().px(thickness_))}; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        const QRectF track(0, (height() - tm().px(thickness_)) / 2, width(), tm().px(thickness_));
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
    tok::PxId thickness_;
    double fill_ = 0;
};

InlineProgress::InlineProgress(ProgressPresenter& presenter, jobs::JobId job, QWidget* parent, Style style)
    : QWidget(parent), presenter_(presenter), job_(job), style_(style),
      bar_(new StepBar(this, style == Style::Compact ? tok::sp_2 : tok::sp_3)), text_(new QLabel(this)),
      cancel_(new Button({}, style == Style::Compact ? Button::Kind::Icon : Button::Kind::Danger, this))
{
    auto* row = new QHBoxLayout(this);
    if (style == Style::Compact) { // .sb-task: vòng quay, nhãn, thanh 80×4, nút ×
        spin_ = new Spinner(this);
        bar_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        cancel_->setIconName(u"x"_s);
        cancel_->setControlSize(Button::Size::Small);
        row->addWidget(spin_);
        row->addWidget(text_);
        row->addWidget(bar_);
    } else {
        row->addWidget(bar_, 1);
        row->addWidget(text_);
    }
    row->addWidget(cancel_);
    setVisible(false);
    connect(cancel_, &QPushButton::clicked, this, [this] {
        if (!current_)
            return;
        // Đánh dấu đang hủy ngay. Lần áp đồng bộ của presenter có thể dựng lại danh sách chứa widget này (Tác vụ nền).
        const QPointer<InlineProgress> self(this);
        presenter_.cancel(current_);
        if (self)
            refresh(); // "Đang hủy…" ngay, không chờ lần áp của presenter (khung kế)
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
    const bool compact = style_ == Style::Compact;
    const int pad = compact ? qRound(tm().px(tok::sp_3)) : 0; // .sb-task padding 0 6px
    layout()->setContentsMargins(pad, 0, pad, 0);
    layout()->setSpacing(qRound(tm().px(compact ? tok::sp_3 : tok::sp_4))); // .sb-task gap 6px, .tx-prog gap 8px
    text_->setFont(tm().font(compact ? tok::fs_xs : tok::fs_s));
    if (compact) { // .sb: chữ --c-text-2
        QPalette pal = text_->palette();
        pal.setColor(QPalette::WindowText, tm().color(tok::c_text_2));
        text_->setPalette(pal);
    }
    updateGeometry();
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
    const bool cancelling = presenter_.cancelling(id);
    const bool compact = style_ == Style::Compact;
    const auto text = [&](int percent) {
        const QString main = compact && cancelling ? tr("%1 · đang hủy…").arg(info->name) // nút × không có chữ
                                                   : ProgressPresenter::label(info->name, percent);
        return others > 0 ? tr("%1 · %n tác vụ khác", nullptr, others).arg(main) : main;
    };
    const int percent = ProgressPresenter::percent(info->progress);
    if (const QString shown = text(percent); shown != text_->text()) {
        text_->setText(shown);
        updateGeometry(); // báo cha ngay, không chờ bố cục qua hàng đợi (thanh trạng thái đặt mục theo sizeHint)
    }
    if (const QString spoken = text(percent / 10 * 10); text_->accessibleName() != spoken)
        text_->setAccessibleName(spoken); // tên tiếp cận đặt rồi thì setText không báo NameChanged mỗi lần
    bar_->setFill(info->progress);        // trước khi hiện: giá trị đầu không thành sự kiện
    const QString action = cancelling ? tr("Đang hủy…") : tr("Hủy");
    if (compact) {
        cancel_->setAccessibleName(action);
        cancel_->setToolTip(action);
    } else {
        cancel_->setText(action);
    }
    cancel_->setEnabled(!cancelling);
    setVisible(true);
}

} // namespace trion::ui
