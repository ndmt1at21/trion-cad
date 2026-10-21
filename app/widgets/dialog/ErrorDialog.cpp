#include "app/widgets/dialog/ErrorDialog.h"

#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/generated/Tokens.h"
#include "app/theme/ThemeManager.h"

#include <QBoxLayout>
#include <QClipboard>
#include <QGuiApplication>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSysInfo>
#include <QTextDocument>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace trion::ui {

namespace {

constexpr int kBodyWidth = 440 - 2 * 14 - 2; // như ConfirmDialog (TC.confirm w 440): prototype không có hộp lỗi riêng
constexpr int kGap = 10;                     // .stack gap
constexpr int kDetailsHeight = 160;          // khối chi tiết (pre.mono): ~10 dòng --fs-xs, còn lại cuộn

ErrorDialog::LogSource& logSource()
{
    static ErrorDialog::LogSource source;
    return source;
}

ErrorDialog::ReportHandler& reportHandler()
{
    static ErrorDialog::ReportHandler handler;
    return handler;
}

} // namespace

void ErrorDialog::setLogSource(LogSource source)
{
    logSource() = std::move(source);
}

void ErrorDialog::setReportHandler(ReportHandler handler)
{
    reportHandler() = std::move(handler);
}

ErrorDialog::ErrorDialog(const QString& title, const QString& explanation, const Error& error, QWidget* parent)
    : DialogFrame(u"error"_s, parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(true);
    setWindowTitle(title);

    const QString code = QString::fromStdString(error.module + '.' + error.code);
    QStringList lines{
        title,
        tr("Mã lỗi: %1").arg(code),
        tr("Thông điệp: %1").arg(QString::fromStdString(error.message)),
        tr("Phiên bản: %1").arg(QStringLiteral("TrionCAD " TRION_VERSION)),
        tr("Hệ điều hành: %1 · %2").arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture()),
    };
    if (const LogSource& source = logSource()) {
        QStringList log = source(kLogLines);
        log = log.mid(std::max<qsizetype>(0, log.size() - kLogLines)); // nguồn trả thừa: vẫn chỉ dòng cuối
        lines << tr("%n dòng log cuối:", nullptr, int(log.size())) << log;
    } else {
        lines << tr("Chưa có log.");
    }
    details_ = lines.join(u'\n');

    auto* body = new QWidget;
    body->setFixedWidth(kBodyWidth);
    auto* v = new QVBoxLayout(body);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(kGap);
    auto* text = new QLabel(explanation);
    text->setTextFormat(Qt::PlainText);
    text->setWordWrap(true);
    v->addWidget(text);
    auto* codeLine = new QLabel(tr("Mã lỗi: %1").arg(code));
    codeLine->setTextFormat(Qt::PlainText);
    codeLine->setFont(theme::font(theme::TextRole::Label));
    codeLine->setForegroundRole(QPalette::PlaceholderText); // .faint, --c-text-3
    v->addWidget(codeLine);
    auto* toggle = new Button(tr("Chi tiết kỹ thuật"), Button::Kind::Ghost); // <details><summary> của prototype
    toggle->setObjectName(u"detailsToggle"_s);
    toggle->setIconName(u"right"_s);
    toggle->setControlSize(Button::Size::Small);
    v->addWidget(toggle, 0, Qt::AlignLeft);
    auto* box = new QPlainTextEdit(details_); // pre.mono: --fs-xs, nền --c-sunken
    box->setObjectName(u"details"_s);
    box->setAccessibleName(toggle->text());
    box->setReadOnly(true);
    box->setFrameShape(QFrame::NoFrame);
    box->setFont(theme::font(theme::TextRole::MonoCaption));
    box->document()->setDocumentMargin(theme::ThemeManager::instance().px(tok::sp_4)); // padding 8px
    box->viewport()->setBackgroundRole(QPalette::AlternateBase);
    box->setFixedHeight(kDetailsHeight);
    box->hide(); // thu gọn mặc định
    v->addWidget(box);
    setBody(body);
    connect(toggle, &QPushButton::clicked, this, [this, toggle, box, body] {
        box->setVisible(box->isHidden());
        toggle->setIconName(box->isHidden() ? u"right"_s : u"down"_s);
        body->layout()->activate(); // cỡ mới lên tới khung ngay, không chờ LayoutRequest: thu gọn lại được
        layout()->activate();
        adjustSize();
    });

    auto* copy = new Button(tr("Sao chép chi tiết"));
    copy->setObjectName(u"copy"_s);
    copy->setIconName(u"copyclip"_s);
    footer()->insertWidget(0, copy); // bên trái chân
    connect(copy, &QPushButton::clicked, this, [this, copy] {
        QGuiApplication::clipboard()->setText(details_);
        copy->setText(tr("Đã sao chép"));
        copy->setIconName(u"check"_s);
    });
    if (reportHandler()) {
        auto* report = new Button(tr("Gửi báo lỗi"));
        report->setObjectName(u"report"_s);
        footer()->addWidget(report);
        connect(report, &QPushButton::clicked, this, [this] {
            if (const ReportHandler& send = reportHandler()) // có thể đã gỡ trong lúc hộp thoại mở
                send(details_, this);
        });
    }
    setConfirm(tr("Đóng"));
}

} // namespace trion::ui
