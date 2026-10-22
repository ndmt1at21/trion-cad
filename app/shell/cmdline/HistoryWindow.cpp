#include "app/shell/cmdline/HistoryWindow.h"

#include "app/core/UiExecutor.h"
#include "app/shell/cmdline/CommandLine.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/views/VirtualTableView.h"
#include "core/base/text/Fold.h"

#include <QAbstractListModel>
#include <QBoxLayout>
#include <QClipboard>
#include <QGuiApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QPainter>
#include <QScrollBar>

#include <algorithm>
#include <optional>
#include <utility>

using namespace Qt::StringLiterals;

namespace trion::ui {

namespace {

constexpr int kWidth = 760, kHeight = 480; // TC.dialog w, h của prototype
constexpr int kToolsGap = 3;               // .pal-tools gap

QColor col(tok::ColorId id)
{
    return theme::ThemeManager::instance().color(id);
}

// Đoạn [from, to) của line chứa needle (đã fold). Fold từng ký tự ("ệ" → "e", dấu kết hợp rời → rỗng) nên giữ bảng vị
// trí về chữ gốc. Chỉ cho các dòng đang thấy.
std::vector<std::pair<int, int>> matches(const QString& line, const QString& needle)
{
    QString folded;
    std::vector<int> at; // at[k]: vị trí trong line của folded[k]
    for (int i = 0; i < line.size(); ++i)
        for (const QChar c : text::foldForSearch(QStringView(line).mid(i, 1))) {
            folded += c;
            at.push_back(i);
        }
    std::vector<std::pair<int, int>> out;
    for (qsizetype k = folded.indexOf(needle); k >= 0; k = folded.indexOf(needle, k + needle.size()))
        out.emplace_back(at[std::size_t(k)], at[std::size_t(k + needle.size() - 1)] + 1);
    return out;
}

// .pal-tools: đệm --sp-3, nét dưới --c-border.
class Tools final : public QWidget {
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter(this).fillRect(QRect(0, height() - 1, width(), 1), col(tok::c_border));
    }
};

} // namespace

// Các dòng đã chụp lúc mở cộng dòng mới; lọc: chỉ các dòng ở rows.
class HistoryWindow::Model final : public QAbstractListModel {
public:
    using QAbstractListModel::QAbstractListModel;

    QStringList lines;
    std::optional<std::vector<int>> rows;

    int line(int row) const { return rows ? (*rows)[std::size_t(row)] : row; }
    int rowCount(const QModelIndex& parent = {}) const override
    {
        return parent.isValid() ? 0 : int(rows ? rows->size() : std::size_t(lines.size()));
    }
    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid())
            return {};
        if (role == Qt::DisplayRole)
            return lines[line(index.row())];
        if (role == Qt::FontRole) // <pre class="mono"> cỡ --fs-s
            return theme::font(theme::TextRole::MonoLabel);
        return {};
    }
    void setRows(std::optional<std::vector<int>> r)
    {
        beginResetModel();
        rows = std::move(r);
        endResetModel();
    }
    void append(const QString& text, bool match)
    {
        const bool shown = !rows || match;
        const int n = rowCount();
        if (shown)
            beginInsertRows({}, n, n);
        lines << text;
        if (rows && match)
            rows->push_back(int(lines.size()) - 1);
        if (shown)
            endInsertRows();
    }
};

// Tô đoạn khớp (--c-warn-soft) dưới chữ của ô.
class HistoryWindow::Delegate final : public ViewItemDelegate {
public:
    using ViewItemDelegate::ViewItemDelegate;
    QString needle; // đã fold; rỗng: không tô

    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (!needle.isEmpty()) {
            const QString text = index.data().toString();
            const QFontMetricsF fm(index.data(Qt::FontRole).value<QFont>());
            const QWidget* w = option.widget;
            const int margin = w ? w->style()->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, w) + 1 : 0; // như delegate
            const QRectF box = QRectF(option.rect).adjusted(margin, 0, -margin, 0);
            const qreal h = fm.ascent() + fm.descent(), y = box.top() + (box.height() - h) / 2;
            p->save();
            p->setClipRect(box);
            for (const auto& [from, to] : matches(text, needle)) {
                const qreal x = box.left() + fm.horizontalAdvance(text.left(from));
                p->fillRect(QRectF(x, y, fm.horizontalAdvance(text.mid(from, to - from)), h), col(tok::c_warn_soft));
            }
            p->restore();
        }
        ViewItemDelegate::paint(p, option, index);
    }
};

// Danh sách: không nét giữa dòng (như <pre>), Ctrl+C sao chép các dòng chọn; đang ở cuối thì giữ ở cuối.
class HistoryWindow::List final : public VirtualTableView {
public:
    explicit List(HistoryWindow* owner) : owner_(owner)
    {
        rowLines_ = false;
        header()->hide();
        setSelectionMode(ExtendedSelection);
        connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
                [this](int v) { stick = v == verticalScrollBar()->maximum(); });
    }
    bool stick = true; // đang xem dòng cuối

    void toEnd()
    {
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
        stick = true;
    }

protected:
    bool event(QEvent* e) override
    {
        if (e->type() == QEvent::ShortcutOverride && static_cast<QKeyEvent*>(e)->matches(QKeySequence::Copy)) {
            e->accept(); // thắng phím tắt Ctrl+C (COPYCLIP) toàn cục
            return true;
        }
        return VirtualTableView::event(e);
    }
    void keyPressEvent(QKeyEvent* e) override
    {
        if (e->matches(QKeySequence::Copy))
            owner_->copy();
        else
            VirtualTableView::keyPressEvent(e);
    }
    void updateGeometries() override
    {
        const bool end = stick;
        VirtualTableView::updateGeometries();
        if (end)
            toEnd();
    }

private:
    HistoryWindow* owner_;
};

HistoryWindow::HistoryWindow(CommandLine& source, QWidget* parent) : DialogFrame(u"textscr"_s, parent)
{
    setCommand(u"TEXTSCR"_s);
    setResizable(true);
    model_ = new Model(this);
    model_->lines = source.historyLines();

    auto* tools = new Tools;
    auto* row = new QHBoxLayout(tools);
    const int pad = qRound(theme::ThemeManager::instance().px(tok::sp_3));
    row->setContentsMargins(pad, pad, pad, pad + 1);
    row->setSpacing(kToolsGap);
    field_ = new SearchField;
    field_->setObjectName(u"search"_s);
    copy_ = new Button;
    copy_->setObjectName(u"copy"_s);
    copy_->setIconName(u"copyclip"_s);
    copy_->setControlSize(Button::Size::Small);
    row->addWidget(field_, 1);
    row->addWidget(copy_);

    list_ = new List(this);
    list_->setObjectName(u"lines"_s);
    list_->setModel(model_);
    delegate_ = new Delegate(list_);
    list_->setItemDelegate(delegate_);
    empty_ = new EmptyState(u"search"_s);
    empty_->hide();

    auto* body = new QWidget;
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins({});
    column->setSpacing(0);
    column->addWidget(tools);
    column->addWidget(list_, 1);
    column->addWidget(empty_, 1);
    setBody(body, true);
    resize(QSize(kWidth, kHeight).grownBy(contentsMargins()));

    connect(field_, &QLineEdit::textChanged, this, &HistoryWindow::search);
    connect(copy_, &QAbstractButton::clicked, this, &HistoryWindow::copy);
    connect(&source, &CommandLine::historyAppended, this, &HistoryWindow::appended);
    run(warm_, {}); // khóa tìm ở nền ngay: 100.000 dòng ~0,1–0,3 s, xong trước khi người dùng kịp gõ
    retranslate();
}

HistoryWindow::~HistoryWindow()
{
    warm_.cancel();
    job_.cancel();
}

int HistoryWindow::rowCount() const
{
    return model_->rowCount();
}

QString HistoryWindow::rowText(int row) const
{
    return model_->lines[model_->line(row)];
}

std::vector<int> HistoryWindow::find(const QStringList& folded, QStringView needle)
{
    std::vector<int> rows;
    for (qsizetype i = 0; i < folded.size(); ++i)
        if (QStringView(folded[i]).contains(needle))
            rows.push_back(int(i));
    return rows;
}

void HistoryWindow::search()
{
    job_.cancel();
    const QString needle = text::foldForSearch(field_->text().trimmed());
    if (needle.isEmpty())
        filter(std::nullopt, {});
    else
        run(job_, needle);
}

// Nền: fold các dòng chưa có khóa (lượt tìm trước khi dựng khóa xong tự làm), rồi tìm nếu có needle.
void HistoryWindow::run(sched::CancelSource& source, const QString& needle)
{
    source.cancel();
    source = sched::CancelSource();
    sched::submit(sched::Priority::Interactive, [rx = Receiver(this), token = source.token(), lines = model_->lines,
                                                 folded = folded_, needle, this]() mutable {
        for (qsizetype i = folded.size(); i < lines.size(); ++i) {
            if ((i & 1023) == 0 && token.cancelled())
                return;
            folded << text::foldForSearch(lines[i]);
        }
        std::vector<int> rows = needle.isEmpty() ? std::vector<int>{} : find(folded, needle);
        if (token.cancelled())
            return;
        post(rx, [this, token, folded = std::move(folded), rows = std::move(rows), needle]() mutable {
            if (!token.cancelled())
                adopt(std::move(folded), std::move(rows), needle);
        });
    });
}

void HistoryWindow::adopt(QStringList folded, std::vector<int> rows, const QString& needle)
{
    const qsizetype done = folded.size(); // rows tính đến đây
    if (done > folded_.size())
        folded_ = std::move(folded);
    while (folded_.size() < model_->lines.size()) // dòng mới đến trong lúc chạy nền
        folded_ << text::foldForSearch(model_->lines[folded_.size()]);
    if (needle.isEmpty())
        return; // lượt dựng khóa
    for (qsizetype i = done; i < folded_.size(); ++i)
        if (folded_[i].contains(needle))
            rows.push_back(int(i));
    filter(std::move(rows), needle);
}

void HistoryWindow::filter(std::optional<std::vector<int>> rows, const QString& needle)
{
    delegate_->needle = needle;
    model_->setRows(std::move(rows));
    const bool none = rowCount() == 0 && !needle.isEmpty();
    list_->setVisible(!none);
    empty_->setVisible(none);
    list_->toEnd();
    retranslate();
    emit searched();
}

void HistoryWindow::appended(const QString& line)
{
    bool match = false;
    if (folded_.size() == model_->lines.size()) { // khóa đủ: giữ đủ (chưa đủ thì lượt nền đang chạy bù sau)
        folded_ << text::foldForSearch(line);
        match = !delegate_->needle.isEmpty() && folded_.back().contains(delegate_->needle);
    }
    const bool end = list_->stick;
    model_->append(line, match);
    if (end)
        list_->toEnd();
}

void HistoryWindow::copy()
{
    QModelIndexList picked = list_->selectionModel()->selectedRows();
    std::sort(picked.begin(), picked.end());
    QStringList out;
    if (picked.isEmpty()) {
        out.reserve(rowCount());
        for (int r = 0; r < rowCount(); ++r)
            out << rowText(r);
    } else {
        for (const QModelIndex& i : picked)
            out << rowText(i.row());
    }
    QGuiApplication::clipboard()->setText(out.join(u'\n'));
}

void HistoryWindow::retranslate()
{
    setWindowTitle(tr("Lịch sử dòng lệnh"));
    field_->setPlaceholderText(tr("Tìm trong nội dung"));
    field_->setAccessibleName(tr("Tìm trong lịch sử dòng lệnh"));
    copy_->setText(tr("Sao chép"));
    copy_->setAccessibleDescription(tr("Sao chép các dòng đang chọn; không chọn dòng nào thì mọi dòng đang hiện"));
    list_->setAccessibleName(tr("Lịch sử dòng lệnh"));
    if (empty_->isVisibleTo(this))
        empty_->setText(tr("Không có dòng nào chứa “%1”").arg(field_->text().trimmed()),
                        tr("Tìm không phân biệt dấu và chữ hoa. Xóa ô tìm để xem toàn bộ lịch sử."));
}

void HistoryWindow::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    DialogFrame::changeEvent(e);
}

} // namespace trion::ui
