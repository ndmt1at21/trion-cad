#include "app/shell/cmdline/InputSearchOptions.h"

#include "app/core/i18n/UiFormat.h"
#include "app/shell/cmdline/Autocomplete.h"
#include "app/shell/cmdline/CommandLine.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/cmdline/CommandInput.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/numeric/NumericField.h"
#include "app/widgets/views/VirtualTableView.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QStandardItemModel>

#include <algorithm>
#include <functional>

using namespace Qt::StringLiterals;

namespace trion::ui {

namespace {

using theme::TextRole;

constexpr int kLabelW = 240;             // --lab-w của hàng thiết lập (prototype SC.options row)
constexpr int kMaxLines = 10;            // 07.1 §8: 0–10 dòng
constexpr int kMaxTransparency = 90;     // thanh trượt 0–90 %
constexpr int kTableRows = 5;            // bảng AutoCorrect cao 5 dòng, cuộn khi nhiều hơn
constexpr int kFontSizes[] = {0, 14, 16, 18, 20}; // px; 0: theo --fs
constexpr int kDelays[] = {0, 200, 500};          // ms (prototype: 200 ms, 0 ms, 500 ms)
enum Column { Typo, Command, Picks, Remove, Columns };

theme::ThemeManager& tm()
{
    return theme::ThemeManager::instance();
}
int px(tok::PxId id)
{
    return qRound(tm().px(id));
}

void tint(QWidget* w, tok::ColorId id)
{
    QPalette pal = w->palette();
    pal.setColor(QPalette::WindowText, tm().color(id));
    w->setPalette(pal);
}

void select(DropDown* box, int value)
{
    box->setCurrentIndex(std::max(0, box->findData(value)));
}

} // namespace

InputSearchOptions::InputSearchOptions(QWidget* parent) : DialogFrame(u"inputsearchoptions"_s, parent)
{
    setModal(true);
    setCommand(u"INPUTSEARCHOPTIONS"_s);
    const settings::Store& store = settings::Store::instance();

    lines_ = new NumericField(ValueKind::Integer);
    lines_->setPickEnabled(false);
    lines_->setValue(std::clamp(store.get(kHistoryLines), 0, kMaxLines));
    connect(lines_, &NumericField::valueCommitted, this, [this](double v) {
        if (const int n = std::clamp(qRound(v), 0, kMaxLines); n != v)
            lines_->setValue(n); // NumericField chưa có chặn trên: kẹp
    });
    linesHint_ = new QLabel;
    fontSize_ = new DropDown;
    for (const int s : kFontSizes)
        fontSize_->addItem(QString(), s);
    select(fontSize_, store.get(kFontSize));
    transparency_ = new Slider;
    transparency_->setRange(0, kMaxTransparency);
    transparency_->setValue(std::clamp(store.get(kFloatTransparency), 0, kMaxTransparency));
    percent_ = new QLabel;
    connect(transparency_, &QSlider::valueChanged, this, &InputSearchOptions::retranslate);
    delay_ = new DropDown;
    for (const int d : kDelays)
        delay_->addItem(QString(), d);
    select(delay_, store.get(kCompletionDelay));
    sysvars_ = new QCheckBox;
    sysvars_->setChecked(store.get(kSearchSysvars));
    vietnamese_ = new QCheckBox;
    vietnamese_->setChecked(store.get(kSearchVietnamese));
    ime_ = new QCheckBox;
    ime_->setChecked(store.get(kImeBypass));

    model_ = new QStandardItemModel(0, Columns, this);
    for (const Autocomplete::Correction& c : Autocomplete::corrections()) {
        QList<QStandardItem*> row;
        for (const QString& s : {c.typo, c.command, i18n::formatCount(c.picks), QString()}) {
            auto* item = new QStandardItem(s);
            item->setEditable(false);
            row << item;
        }
        row[Picks]->setData(int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
        row[Remove]->setData(theme::IconProvider::icon(u"trash"_s), Qt::DecorationRole);
        model_->appendRow(row);
    }
    corrections_ = new VirtualTableView;
    corrections_->setObjectName(u"corrections"_s);
    corrections_->setModel(model_);
    corrections_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    corrections_->installEventFilter(this);
    connect(corrections_, &QAbstractItemView::clicked, this, [this](const QModelIndex& i) {
        if (i.column() == Remove)
            removeRows({i.row()});
    });
    none_ = new EmptyState(u"keyboard"_s);
    none_->setVisible(model_->rowCount() == 0);
    corrections_->setVisible(model_->rowCount() > 0);

    // Thân: nhóm (.gh) rồi các hàng nhãn | điều khiển.
    auto* body = new QWidget;
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins({});
    const auto group = [&](const char* title) {
        column->addWidget(heading(title));
        auto* grid = new QGridLayout;
        grid->setColumnMinimumWidth(0, kLabelW);
        grid->setColumnStretch(1, 1);
        column->addLayout(grid);
        return grid;
    };
    const auto addRow = [&](QGridLayout* grid, const char* name, std::initializer_list<QWidget*> controls) {
        QLabel* l = label(name);
        auto* row = new QHBoxLayout;
        for (QWidget* w : controls)
            row->addWidget(w);
        row->addStretch(1);
        const int r = grid->rowCount();
        grid->addWidget(l, r, 0);
        grid->addLayout(row, r, 1);
        if (QWidget* first = *controls.begin(); first != sysvars_ && first != vietnamese_ && first != ime_)
            l->setBuddy(first);
    };
    QGridLayout* g = group(QT_TR_NOOP("Dòng lệnh"));
    addRow(g, QT_TR_NOOP("Số dòng lịch sử dòng lệnh"), {lines_, linesHint_});
    addRow(g, QT_TR_NOOP("Cỡ chữ"), {fontSize_});
    addRow(g, QT_TR_NOOP("Độ trong suốt khi nổi"), {transparency_, percent_});
    g = group(QT_TR_NOOP("Tự hoàn thành"));
    addRow(g, QT_TR_NOOP("Độ trễ"), {delay_});
    addRow(g, QT_TR_NOOP("Tìm biến hệ thống"), {sysvars_});
    addRow(g, QT_TR_NOOP("Tìm lệnh theo tiếng Việt"), {vietnamese_});
    g = group(QT_TR_NOOP("Bộ gõ tiếng Việt"));
    addRow(g, QT_TR_NOOP("Khi đang chờ tên lệnh"), {ime_});
    column->addWidget(heading(QT_TR_NOOP("Tự sửa lỗi gõ (AutoCorrect)")));
    column->addWidget(corrections_);
    column->addWidget(none_);
    setBody(body);

    addCancel();
    connect(&tm(), &theme::ThemeManager::changed, this, &InputSearchOptions::restyle);
    restyle();
    retranslate();
}

QLabel* InputSearchOptions::label(const char* source)
{
    auto* l = new QLabel;
    labels_.append({l, source});
    return l;
}

QLabel* InputSearchOptions::heading(const char* source)
{
    auto* l = new QLabel;
    headings_.append({l, source});
    return l;
}

void InputSearchOptions::removeRows(QList<int> rows)
{
    std::sort(rows.begin(), rows.end(), std::greater<>());
    for (const int r : rows) {
        removed_ << model_->item(r, Typo)->text();
        model_->removeRow(r);
    }
    const bool empty = model_->rowCount() == 0;
    corrections_->setVisible(!empty);
    none_->setVisible(empty);
    if (empty)
        none_->setFocus(); // focus không mất theo bảng đã ẩn
}

bool InputSearchOptions::eventFilter(QObject* watched, QEvent* e)
{
    if (watched == corrections_ && e->type() == QEvent::KeyPress
        && static_cast<QKeyEvent*>(e)->key() == Qt::Key_Delete) {
        QList<int> rows;
        for (const QModelIndex& i : corrections_->selectionModel()->selectedRows())
            rows << i.row();
        if (rows.isEmpty() && corrections_->currentIndex().isValid())
            rows << corrections_->currentIndex().row();
        removeRows(rows);
        return true;
    }
    return DialogFrame::eventFilter(watched, e);
}

void InputSearchOptions::accept()
{
    settings::Store& s = settings::Store::instance();
    s.set(kHistoryLines, std::clamp(qRound(lines_->value()), 0, kMaxLines));
    s.set(kFontSize, fontSize_->currentData().toInt());
    s.set(kFloatTransparency, transparency_->value());
    s.set(kCompletionDelay, delay_->currentData().toInt());
    s.set(kSearchSysvars, sysvars_->isChecked());
    s.set(kSearchVietnamese, vietnamese_->isChecked());
    s.set(kImeBypass, ime_->isChecked());
    for (const QString& typo : std::as_const(removed_))
        Autocomplete::removeCorrection(typo);
    removed_.clear();
    DialogFrame::accept();
}

void InputSearchOptions::restyle()
{
    for (const Text& t : std::as_const(labels_)) { // .frow .lab: --c-text-2, --fs-s
        t.label->setFont(theme::font(TextRole::Label));
        tint(t.label, tok::c_text_2);
    }
    for (const Text& t : std::as_const(headings_)) { // .gh: --fs-xs đậm, chữ hoa, giãn .06em, --c-text-3
        QFont f = theme::font(TextRole::Caption, theme::Weight::Bold);
        f.setCapitalization(QFont::AllUppercase);
        f.setLetterSpacing(QFont::PercentageSpacing, 106);
        t.label->setFont(f);
        tint(t.label, tok::c_text_3);
    }
    linesHint_->setFont(theme::font(TextRole::Caption)); // .faint --fs-xs
    tint(linesHint_, tok::c_text_3);
    percent_->setFont(theme::font(TextRole::Mono));
    QHeaderView* head = corrections_->header();
    head->resizeSection(Remove, corrections_->rowHeight());
    corrections_->setFixedHeight(head->sizeHint().height() + kTableRows * corrections_->rowHeight());
    auto* column = static_cast<QVBoxLayout*>(body()->layout());
    column->setSpacing(px(tok::sp_3));
}

void InputSearchOptions::retranslate()
{
    setWindowTitle(tr("Thiết lập dòng lệnh"));
    setConfirm(tr("Lưu"));
    for (const Text& t : std::as_const(labels_))
        t.label->setText(tr(t.source));
    for (const Text& t : std::as_const(headings_))
        t.label->setText(tr(t.source));
    lines_->setLabel(tr("Số dòng lịch sử"));
    linesHint_->setText(tr("0–10 dòng; F2 mở lịch sử đầy đủ"));
    for (int i = 0; i < fontSize_->count(); ++i) {
        const int s = fontSize_->itemData(i).toInt();
        fontSize_->setItemText(i, s > 0 ? tr("%1 px").arg(s) : tr("Theo giao diện"));
    }
    fontSize_->setAccessibleName(tr("Cỡ chữ"));
    for (int i = 0; i < delay_->count(); ++i)
        delay_->setItemText(i, tr("%1 ms").arg(delay_->itemData(i).toInt()));
    delay_->setAccessibleName(tr("Độ trễ tự hoàn thành"));
    transparency_->setAccessibleName(tr("Độ trong suốt khi nổi"));
    percent_->setText(tr("%1%").arg(transparency_->value()));
    sysvars_->setText(tr("Gợi ý cả biến hệ thống"));
    vietnamese_->setText(tr("Gõ “cắt” ra TRIM, “bo góc” ra FILLET"));
    ime_->setText(tr("Bỏ qua bộ gõ để “lm” không thành “lầm”"));
    model_->setHorizontalHeaderLabels({tr("Gõ sai"), tr("Sửa thành"), tr("Số lần chọn"), QString()});
    for (const int c : {Typo, Command, Picks}) // đủ chữ đầu cột (vi-XL dài hơn 40%)
        corrections_->header()->resizeSection(c, std::max(kLabelW / 2, corrections_->header()->sectionSizeHint(c)));
    for (int r = 0; r < model_->rowCount(); ++r)
        model_->item(r, Remove)->setToolTip(tr("Xóa"));
    corrections_->setAccessibleName(tr("Tự sửa lỗi gõ"));
    corrections_->setAccessibleDescription(tr("Delete: xóa dòng đang chọn"));
    none_->setText(tr("Chưa có lỗi gõ nào được tự sửa"),
                   tr("Chọn cùng một gợi ý “Ý bạn là” 3 lần thì từ đó lỗi gõ ấy được sửa tự động."));
}

void InputSearchOptions::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    DialogFrame::changeEvent(e);
}

} // namespace trion::ui
