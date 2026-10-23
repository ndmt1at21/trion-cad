#include "app/system/about/AboutDialog.h"

#include "app/core/a11y/Announce.h"
#include "app/core/edition/EditionBadge.h"
#include "app/core/edition/EditionGate.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/UiFormat.h"
#include "app/core/UiExecutor.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/message/Messages.h"
#include "app/widgets/views/VirtualTableView.h"

#include <QBoxLayout>
#include <QClipboard>
#include <QDesktopServices>
#include <QEvent>
#include <QFile>
#include <QGuiApplication>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QScreen>
#include <QStandardItemModel>
#include <QSysInfo>

#include <array>
#include <cstring>
#include <thread>
#include <utility>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(Q_OS_MACOS)
#include <sys/sysctl.h>
#include <sys/types.h>
#else
#include <unistd.h>
#endif

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#define TRION_ABOUT_CPUID 1
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#endif

#ifndef TRION_VERSION
#define TRION_VERSION "0.0.0"
#endif
#ifndef TRION_BUILD_ID
#define TRION_BUILD_ID "dev"
#endif
#ifndef TRION_CHANNEL
#define TRION_CHANNEL "internal"
#endif

using namespace Qt::StringLiterals;

namespace trion::about {

namespace {

using theme::TextRole;
using theme::Weight;

constexpr int kWidth = 600;        // TC.dialog w:600
constexpr int kBodyPad = 14;       // .dlg-b padding
constexpr int kCardBorder = 1;     // viền thẻ của DialogFrame
constexpr int kBodyWidth = kWidth - 2 * kBodyPad - 2 * kCardBorder;
constexpr int kStackGap = 10;      // .stack gap 10px
constexpr int kGroupTop = 10;      // .grp padding-top 10px (trái, phải, dưới: --sp-5)
constexpr int kTableRows = 6;      // .tblwrap max-height 150px: ~6 dòng --h-row, nhiều hơn thì cuộn
constexpr int kTextWidth = 640;    // hộp văn bản giấy phép (prototype không có): ~80 cột Mono --fs-xs
constexpr int kTextHeight = 420;
// ponytail: địa chỉ tạm trên web của sản phẩm; UIL-021 (trang web) chốt.
constexpr auto kTermsUrl = "https://trioncad.vn/dieu-khoan";
constexpr auto kPrivacyUrl = "https://trioncad.vn/quyen-rieng-tu";
constexpr auto kSupportUrl = "https://trioncad.vn/ho-tro";

theme::ThemeManager& tm()
{
    return theme::ThemeManager::instance();
}
int px(tok::PxId id)
{
    return qRound(tm().px(id));
}
QColor col(tok::ColorId id)
{
    return tm().color(id);
}

void tint(QWidget* w, tok::ColorId id)
{
    QPalette pal = w->palette();
    pal.setColor(QPalette::WindowText, tm().color(id));
    w->setPalette(pal);
}

// Đầu nhóm .gh: --fs-xs đậm, chữ hoa, giãn .06em, --c-text-3.
void styleHeading(QLabel* l)
{
    QFont f = theme::font(TextRole::Caption, Weight::Bold);
    f.setCapitalization(QFont::AllUppercase);
    f.setLetterSpacing(QFont::PercentageSpacing, 106);
    l->setFont(f);
    tint(l, tok::c_text_3);
}

// Nhóm .grp: viền 1 px --c-border, bo --r-lg, đệm 10/12/12 px, cách dòng --sp-4; dòng đầu là .gh.
class Group final : public QWidget {
public:
    explicit Group(QLabel* heading)
    {
        auto* box = new QVBoxLayout(this);
        box->addWidget(heading);
    }
    QVBoxLayout* box() const { return static_cast<QVBoxLayout*>(layout()); }
    void restyle()
    {
        box()->setContentsMargins(px(tok::sp_5), kGroupTop, px(tok::sp_5), px(tok::sp_5));
        box()->setSpacing(px(tok::sp_4));
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(col(tok::c_border), 1));
        p.setBrush(Qt::NoBrush);
        const double r = tm().px(tok::r_lg);
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), r, r);
    }
};

// Logo --ic-lg màu chữ (ic('logo', 'lg') của prototype: currentColor). Trang trí, không nhận focus.
class Logo final : public QWidget {
public:
    QSize sizeHint() const override { return {px(tok::ic_lg), px(tok::ic_lg)}; }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const int s = px(tok::ic_lg);
        p.drawPixmap(QPoint((width() - s) / 2, (height() - s) / 2),
                     theme::IconProvider::instance().pixmap(u"logo"_s, s, devicePixelRatioF(), theme::IconState::Hover));
    }
};

// Tên CPU. ponytail: NT-008 (CpuInfo của hồ sơ phần cứng) thay.
QString cpuName()
{
#ifdef TRION_ABOUT_CPUID
    std::array<unsigned, 12> brand{};
    unsigned top = 0;
#if defined(_MSC_VER)
    int r[4] = {};
    __cpuid(r, int(0x80000000u));
    top = unsigned(r[0]);
    for (unsigned i = 0; top >= 0x80000004u && i < 3; ++i) {
        __cpuid(r, int(0x80000002u + i));
        std::memcpy(&brand[4 * i], r, sizeof r);
    }
#else
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (__get_cpuid(0x80000000u, &a, &b, &c, &d))
        top = a;
    for (unsigned i = 0; top >= 0x80000004u && i < 3; ++i)
        __get_cpuid(0x80000002u + i, &brand[4 * i], &brand[4 * i + 1], &brand[4 * i + 2], &brand[4 * i + 3]);
#endif
    const QString name =
        QString::fromLatin1(reinterpret_cast<const char*>(brand.data()), qsizetype(sizeof brand)).remove(QChar(0)).simplified();
    if (!name.isEmpty())
        return name;
#endif
#if defined(Q_OS_MACOS)
    char buf[256] = {};
    size_t n = sizeof buf - 1;
    if (sysctlbyname("machdep.cpu.brand_string", buf, &n, nullptr, 0) == 0 && buf[0])
        return QString::fromUtf8(buf).simplified();
#elif !defined(Q_OS_WIN)
    QFile f(u"/proc/cpuinfo"_s);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        for (QByteArray line = f.readLine(); !line.isEmpty(); line = f.readLine())
            if (line.startsWith("model name") || line.startsWith("Hardware") || line.startsWith("Model"))
                return QString::fromUtf8(line.mid(line.indexOf(':') + 1)).simplified();
#endif
    return QSysInfo::currentCpuArchitecture();
}

// RAM vật lý, byte. ponytail: NT-008 thay.
qint64 totalRam()
{
#if defined(Q_OS_WIN)
    MEMORYSTATUSEX m{};
    m.dwLength = sizeof m;
    return GlobalMemoryStatusEx(&m) ? qint64(m.ullTotalPhys) : 0;
#elif defined(Q_OS_MACOS)
    qint64 v = 0;
    size_t n = sizeof v;
    return sysctlbyname("hw.memsize", &v, &n, nullptr, 0) == 0 ? v : 0;
#else
    const long pages = sysconf(_SC_PHYS_PAGES), size = sysconf(_SC_PAGE_SIZE);
    return pages > 0 && size > 0 ? qint64(pages) * size : 0;
#endif
}

// Tên ngôn ngữ giao diện viết bằng chính ngôn ngữ đó (không dịch).
QString languageName()
{
    const i18n::Translator* t = i18n::Translator::instance();
    const QString lang = t ? t->language() : u"vi"_s;
    if (lang == "vi"_L1)
        return u"Tiếng Việt"_s; // i18n-ok: tên ngôn ngữ viết bằng chính nó
    if (lang == "en"_L1)
        return u"English"_s; // i18n-ok: tên ngôn ngữ viết bằng chính nó
    return lang;             // vi-XL (QA)
}

QString versionLine()
{
    const BuildInfo b = BuildInfo::current();
    return AboutDialog::tr("%1 · bản build %2 · kênh %3").arg(b.version, b.build, b.channelName());
}

// Văn bản giấy phép đầy đủ của một thư viện, modal trên hộp Giới thiệu: chữ Mono --fs-xs, chỉ đọc, cuộn; Tab rời ô.
class LicenseDialog final : public ui::DialogFrame {
public:
    LicenseDialog(Component c, Licenses all, QWidget* parent)
        : DialogFrame(u"license-text"_s, parent), component_(std::move(c)), all_(std::move(all))
    {
        setModal(true);
        setAttribute(Qt::WA_DeleteOnClose);
        text_ = new QPlainTextEdit;
        text_->setObjectName(u"licenseText"_s);
        text_->setReadOnly(true);
        text_->setFixedSize(kTextWidth - 2 * kBodyPad - 2 * kCardBorder, kTextHeight);
        text_->setFont(theme::font(TextRole::MonoCaption));
        setBody(text_);
        retranslate();
    }

protected:
    void changeEvent(QEvent* e) override
    {
        if (e->type() == QEvent::LanguageChange)
            retranslate();
        DialogFrame::changeEvent(e);
    }

private:
    void retranslate()
    {
        setWindowTitle(AboutDialog::tr("Giấy phép: %1").arg(component_.name));
        setConfirm(AboutDialog::tr("Đóng"));
        text_->setPlainText(AboutDialog::licenseText(component_, all_));
        text_->setAccessibleName(windowTitle());
    }

    Component component_;
    Licenses all_;
    QPlainTextEdit* text_;
};

} // namespace

// ------------------------------------------------------------------------------------------------------ BuildInfo

BuildInfo BuildInfo::current()
{
    return {QStringLiteral(TRION_VERSION), QStringLiteral(TRION_BUILD_ID), QStringLiteral(TRION_CHANNEL)};
}

QString BuildInfo::channelName() const
{
    if (channel == "stable"_L1)
        return AboutDialog::tr("Ổn định");
    if (channel == "beta"_L1)
        return AboutDialog::tr("Beta");
    return AboutDialog::tr("Nội bộ");
}

// ------------------------------------------------------------------------------------------------------- Licenses

Expected<Licenses> Licenses::parse(const QByteArray& json)
{
    const auto fail = [](const QString& what) { return Error{"about", "licenses", what.toStdString()}; };
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject())
        return fail(u"licenses.json hỏng: %1"_s.arg(pe.errorString()));
    if (doc["format"_L1].toString() != "trion-licenses/1"_L1)
        return fail(u"licenses.json: định dạng lạ"_s);
    Licenses out;
    const QJsonObject texts = doc["texts"_L1].toObject();
    for (auto it = texts.begin(); it != texts.end(); ++it)
        out.texts.insert(it.key(), it.value().toString());
    const QJsonArray components = doc["components"_L1].toArray();
    for (const auto& v : components) {
        const QJsonObject o = v.toObject();
        Component c{o["id"_L1].toString(),   o["name"_L1].toString(), o["license"_L1].toString(),
                    o["spdx"_L1].toString(), o["link"_L1].toString(), o["url"_L1].toString(),
                    o["notice"_L1].toString(), {}};
        for (const auto& t : o["texts"_L1].toArray())
            c.texts << t.toString();
        if (c.id.isEmpty() || c.name.isEmpty() || c.license.isEmpty() || c.texts.isEmpty())
            return fail(u"licenses.json: thành phần thiếu trường hoặc văn bản: %1"_s.arg(c.id));
        for (const QString& t : std::as_const(c.texts))
            if (out.texts.value(t).trimmed().isEmpty())
                return fail(u"licenses.json: %1 thiếu văn bản giấy phép %2"_s.arg(c.id, t));
        out.components << c;
    }
    if (out.components.isEmpty())
        return fail(u"licenses.json: không có thành phần nào"_s);
    return out;
}

Expected<Licenses> Licenses::load()
{
    QFile f(u":/trion/about/licenses.json"_s);
    if (!f.open(QIODevice::ReadOnly))
        return Error{"about", "licenses", "không đọc được :/trion/about/licenses.json"};
    return parse(f.readAll());
}

// ----------------------------------------------------------------------------------------------------- SystemInfo

SystemInfo SystemInfo::read()
{
    SystemInfo s;
    s.os = u"%1 (%2)"_s.arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture());
    s.cpu = cpuName();
    s.threads = int(std::thread::hardware_concurrency());
    s.ram = totalRam();
    return s;
}

// ---------------------------------------------------------------------------------------------------- AboutDialog

AboutDialog::AboutDialog(edition::EditionGate& gate, Sources sources, QWidget* parent)
    : DialogFrame(u"about"_s, parent), gate_(gate), sources_(std::move(sources))
{
    if (!sources_.openUrl)
        sources_.openUrl = [](const QUrl& u) { return QDesktopServices::openUrl(u); };
    setCommand(u"ABOUT"_s);

    auto* body = new QWidget;
    body->setFixedWidth(kBodyWidth);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins({});

    // Đầu: logo, "TrionCAD" + huy hiệu phiên bản, dòng phiên bản · bản build · kênh.
    auto* head = new QHBoxLayout;
    logo_ = new Logo;
    head->addWidget(logo_, 0, Qt::AlignVCenter);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(0);
    auto* titleRow = new QHBoxLayout;
    product_ = new QLabel;
    product_->setObjectName(u"product"_s);
    titleRow->addWidget(product_);
    auto* badge = new edition::EditionBadge(gate_);
    badge->setObjectName(u"edition"_s);
    titleRow->addWidget(badge, 0, Qt::AlignVCenter);
    titleRow->addStretch(1);
    titles->addLayout(titleRow);
    version_ = new QLabel;
    version_->setObjectName(u"version"_s);
    version_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    titles->addWidget(version_);
    head->addLayout(titles, 1);
    column->addLayout(head);

    // Thông tin hệ thống, nút Sao chép.
    systemHead_ = new QLabel;
    auto* system = new Group(systemHead_);
    info_ = new QLabel;
    info_->setObjectName(u"system"_s);
    info_->setTextFormat(Qt::PlainText);
    info_->setWordWrap(true);
    info_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    system->box()->addWidget(info_);
    copy_ = new ui::Button;
    copy_->setObjectName(u"copy"_s);
    copy_->setControlSize(ui::Button::Size::Small);
    copy_->setEnabled(false); // tới khi thông tin đọc nền về
    system->box()->addWidget(copy_, 0, Qt::AlignLeft);
    column->addWidget(system);
    connect(copy_, &QPushButton::clicked, this, [this] {
        QGuiApplication::clipboard()->setText(systemText());
        copied_ = true;
        retranslate();
        a11y::announce(copy_, tr("Đã sao chép thông tin hệ thống"));
    });

    // Thư viện mã nguồn mở: bảng tên | giấy phép, nút xem văn bản.
    ossHead_ = new QLabel;
    auto* oss = new Group(ossHead_);
    model_ = new QStandardItemModel(0, 2, this);
    table_ = new ui::VirtualTableView;
    table_->setObjectName(u"licenses"_s);
    table_->setModel(model_);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->installEventFilter(this); // Enter mở văn bản, không tới nút Đóng
    connect(table_, &QAbstractItemView::doubleClicked, this, [this](const QModelIndex& i) { showLicense(i.row()); });
    oss->box()->addWidget(table_);
    viewLicense_ = new ui::Button;
    viewLicense_->setObjectName(u"viewLicense"_s);
    viewLicense_->setControlSize(ui::Button::Size::Small);
    viewLicense_->setEnabled(false);
    oss->box()->addWidget(viewLicense_, 0, Qt::AlignLeft);
    connect(viewLicense_, &QPushButton::clicked, this, [this] { showLicense(std::max(0, table_->currentIndex().row())); });
    column->addWidget(oss);

    // Nhãn hiệu; Điều khoản, Chính sách riêng tư, Liên hệ hỗ trợ; dòng lỗi khi không mở được trang web.
    trademark_ = new QLabel;
    trademark_->setObjectName(u"trademark"_s);
    trademark_->setTextFormat(Qt::PlainText);
    trademark_->setWordWrap(true);
    column->addWidget(trademark_);
    auto* links = new QHBoxLayout;
    const auto link = [&](const QString& name) {
        auto* b = new ui::Button({}, ui::Button::Kind::Ghost);
        b->setObjectName(name);
        b->setControlSize(ui::Button::Size::Small);
        links->addWidget(b);
        return b;
    };
    terms_ = link(u"terms"_s);
    privacy_ = link(u"privacy"_s);
    support_ = link(u"support"_s);
    links->addStretch(1);
    column->addLayout(links);
    connect(terms_, &QPushButton::clicked, this, [this] { openLink(QUrl(QString::fromLatin1(kTermsUrl))); });
    connect(privacy_, &QPushButton::clicked, this, [this] { openLink(QUrl(QString::fromLatin1(kPrivacyUrl))); });
    connect(support_, &QPushButton::clicked, this, &AboutDialog::contactSupport);
    error_ = new ui::FieldMessage(nullptr);
    error_->setObjectName(u"error"_s);
    error_->hide();
    column->addWidget(error_);
    setBody(body);

    connect(&tm(), &theme::ThemeManager::changed, this, &AboutDialog::restyle);
    restyle();
    retranslate();
}

AboutDialog::~AboutDialog()
{
    cancel_.cancel();
}

QStringList AboutDialog::systemLines() const
{
    const QString wait = tr("Đang đọc…");
    QStringList first{wait};
    if (systemReady_) {
        const QString cpu = sys_.threads > 0 ? tr("%1, %n luồng", nullptr, sys_.threads).arg(sys_.cpu) : sys_.cpu;
        const QString ram = sys_.ram > 0 ? tr("%1 RAM").arg(i18n::formatBytes(sys_.ram)) : tr("RAM chưa rõ");
        first = {sys_.os, cpu, ram};
    }
    QStringList graphics{wait};
    if (graphicsReady_) {
        graphics.clear();
        for (const QString& s : {gpu_.backend, gpu_.adapter})
            if (!s.trimmed().isEmpty())
                graphics << s.trimmed();
        if (graphics.isEmpty())
            graphics << tr("chưa rõ");
    }
    // Màn hình của hộp thoại: độ phân giải thật (điểm ảnh thiết bị) và tỉ lệ của hệ điều hành.
    if (const QScreen* s = screen()) {
        const qreal dpr = s->devicePixelRatio();
        const QSize size = s->geometry().size() * dpr;
        graphics << u"%1 × %2"_s.arg(size.width()).arg(size.height()) << u"%1%"_s.arg(qRound(dpr * 100));
    }
    const QString profile = sources_.profile.isEmpty() ? tr("<<Mặc định>>") : sources_.profile;
    return {first.join(u" · "_s), tr("Đồ họa: %1").arg(graphics.join(u" · "_s)),
            tr("Ngôn ngữ: %1 · Hồ sơ: %2").arg(languageName(), profile)};
}

QString AboutDialog::systemText() const
{
    QStringList lines{tr("TrionCAD %1 · %2").arg(edition::EditionBadge::label(gate_), versionLine())};
    lines << systemLines();
    return lines.join(u'\n');
}

QString AboutDialog::licenseText(const Component& c, const Licenses& all)
{
    QStringList parts;
    if (c.link == "dynamic"_L1) { // LGPL (05 §15): liên kết động, người dùng thay được thư viện; nơi lấy mã nguồn
#if defined(Q_OS_WIN)
        const QString how = tr("đóng TrionCAD, chép các tệp Qt6*.dll và thư mục plugins của bản Qt đó đè lên thư mục cài "
                               "TrionCAD");
#elif defined(Q_OS_MACOS)
        const QString how = tr("đóng TrionCAD, thay các thư mục Qt*.framework trong TrionCAD.app/Contents/Frameworks");
#else
        const QString how = tr("đóng TrionCAD, thay các tệp libQt6*.so.6 trong thư mục lib của TrionCAD, hoặc chạy "
                               "TrionCAD với LD_LIBRARY_PATH trỏ tới thư mục chứa bản Qt đó");
#endif
        parts << tr("%1 được dùng theo GNU Lesser General Public License phiên bản 3 (LGPL-3.0), liên kết động: "
                    "TrionCAD gọi Qt qua thư viện dùng chung và không sửa mã của Qt. Bạn có quyền thay thư viện Qt bằng "
                    "bản khác tương thích nhị phân, kể cả bản tự build từ mã nguồn đã sửa: %2. Mã nguồn Qt %3: "
                    "https://download.qt.io/archive/qt/. Văn bản LGPL-3.0 và GPL-3.0 ở dưới.")
                     .arg(c.name, how, QString::fromLatin1(qVersion()));
    }
    parts << c.notice;
    for (const QString& t : c.texts)
        parts << all.texts.value(t).trimmed();
    return parts.join(u"\n\n"_s);
}

void AboutDialog::showLicense(int row)
{
    if (!licensesReady_ || row < 0 || row >= list_.components.size())
        return;
    (new LicenseDialog(list_.components[row], list_, this))->open();
}

void AboutDialog::showEvent(QShowEvent* e)
{
    DialogFrame::showEvent(e);
    startLoading();
}

void AboutDialog::startLoading()
{
    if (std::exchange(started_, true))
        return;
    const ui::Receiver rx(this);
    const sched::CancelToken token = cancel_.token();
    // Thông tin máy và danh sách giấy phép (~75 KB JSON): đọc nền, áp một lần trên luồng UI.
    sched::submit(sched::Priority::Interactive, [rx, token, this] {
        SystemInfo s = SystemInfo::read();
        Expected<Licenses> l = Licenses::load();
        if (token.cancelled())
            return;
        ui::post(rx, [this, s = std::move(s), l = std::move(l)] {
            sys_ = s;
            systemReady_ = true;
            if (l) {
                list_ = *l;
            } else {
                loadError_ = QString::fromStdString(l.error().message);
                qWarning("AboutDialog: %s", qUtf8Printable(loadError_));
            }
            licensesReady_ = true;
            retranslate();
            if (model_->rowCount() > 0)
                table_->setCurrentIndex(model_->index(0, 0));
            if (isLoaded())
                emit loaded();
        });
    });
    // Backend và GPU (RND-001): có thể chậm (dò adapter), riêng một việc nền.
    sched::submit(sched::Priority::Interactive, [rx, token, probe = sources_.graphics, this] {
        GraphicsInfo g = probe ? probe() : GraphicsInfo{};
        if (token.cancelled())
            return;
        ui::post(rx, [this, g = std::move(g)] {
            gpu_ = g;
            graphicsReady_ = true;
            retranslate();
            if (isLoaded())
                emit loaded();
        });
    });
}

void AboutDialog::openLink(const QUrl& url)
{
    if (sources_.openUrl(url)) {
        failedUrl_.clear();
        error_->setError({});
        error_->hide();
        return;
    }
    failedUrl_ = url;
    retranslate();
}

void AboutDialog::contactSupport()
{
    if (!sources_.support) { // ponytail: UIL-006 (UI-L17) chưa nối: trang hỗ trợ trên web
        openLink(QUrl(QString::fromLatin1(kSupportUrl)));
        return;
    }
    const auto support = sources_.support; // như prototype: đóng hộp này rồi mở Liên hệ hỗ trợ
    QWidget* parent = parentWidget();
    accept();
    support(parent);
}

void AboutDialog::restyle()
{
    product_->setFont(theme::font(TextRole::Title, Weight::Bold)); // b --fs-xl
    tint(product_, tok::c_text);
    version_->setFont(theme::font(TextRole::MonoLabel)); // .muted.mono --fs-s
    tint(version_, tok::c_text_2);
    styleHeading(systemHead_);
    styleHeading(ossHead_);
    info_->setFont(theme::font(TextRole::MonoCaption)); // pre.mono --fs-xs --c-text-2
    tint(info_, tok::c_text_2);
    trademark_->setFont(theme::font(TextRole::Caption)); // p.faint --fs-xs
    tint(trademark_, tok::c_text_3);
    auto* column = static_cast<QVBoxLayout*>(body()->layout());
    column->setSpacing(kStackGap);
    static_cast<QHBoxLayout*>(column->itemAt(0)->layout())->setSpacing(px(tok::sp_5)); // .hrow gap 12px
    for (QLabel* heading : {systemHead_, ossHead_})
        static_cast<Group*>(heading->parentWidget())->restyle(); // đầu nhóm nằm trong Group của nó
    static_cast<QHBoxLayout*>(column->itemAt(4)->layout())->setSpacing(px(tok::sp_4)); // .hrow: các liên kết
    // Bảng: hai cột bằng nhau (table-layout fixed), cao kTableRows dòng.
    const int inner = kBodyWidth - 2 * px(tok::sp_5) - 2 * table_->frameWidth() - 2;
    table_->header()->resizeSection(0, inner / 2);
    table_->setFixedHeight(table_->header()->sizeHint().height() + kTableRows * table_->rowHeight() +
                           2 * table_->frameWidth());
    fitHeights();
}

// Nhãn xuống dòng: cỡ tối thiểu của cửa sổ cấp cao không tính chiều cao theo bề rộng (QLayout), và adjustSize() cắt cửa
// sổ còn 2/3 màn hình (hộp này cao hơn thế trên màn 768 px), nên chữ xuống thêm dòng sẽ bị cắt. Thân rộng cố định:
// chiều cao tối thiểu = chiều cao ở bề rộng thật của nhãn.
void AboutDialog::fitHeights()
{
    const auto fit = [](QLabel* l, int width) {
        if (const int h = l->heightForWidth(width); h > 0) // chưa có chữ: -1
            l->setMinimumHeight(h);
    };
    fit(info_, kBodyWidth - 2 * px(tok::sp_5)); // trong nhóm: đệm trái, phải (gồm viền)
    fit(trademark_, kBodyWidth);
}

void AboutDialog::retranslate()
{
    setWindowTitle(tr("Giới thiệu TrionCAD"));
    product_->setText(u"TrionCAD"_s); // i18n-ok: tên sản phẩm
    version_->setText(versionLine());
    systemHead_->setText(tr("Thông tin hệ thống"));
    info_->setText(systemLines().join(u'\n'));
    copy_->setEnabled(systemReady_ && graphicsReady_);
    copy_->setText(copied_ ? tr("Đã sao chép") : tr("Sao chép"));
    copy_->setIconName(copied_ ? u"check"_s : u"copyclip"_s);
    copy_->setAccessibleDescription(tr("Chép phiên bản và thông tin hệ thống để gửi bộ phận hỗ trợ"));

    ossHead_->setText(tr("Thư viện mã nguồn mở"));
    model_->setHorizontalHeaderLabels({tr("Thư viện"), tr("Giấy phép")});
    model_->setRowCount(int(list_.components.size()));
    for (int r = 0; r < list_.components.size(); ++r) {
        const Component& c = list_.components[r];
        const QString license = c.link == "dynamic"_L1 ? tr("%1, liên kết động").arg(c.license) : c.license;
        for (const auto& [column, text] : {std::pair{0, c.name}, std::pair{1, license}}) {
            auto* item = new QStandardItem(text);
            item->setEditable(false);
            if (column == 1)
                item->setForeground(col(tok::c_text_3)); // td.faint
            model_->setItem(r, column, item);
        }
    }
    table_->setAccessibleName(tr("Thư viện mã nguồn mở"));
    table_->setAccessibleDescription(tr("Enter: xem văn bản giấy phép"));
    viewLicense_->setText(tr("Văn bản giấy phép…"));
    viewLicense_->setEnabled(model_->rowCount() > 0);

    trademark_->setText(tr("AutoCAD® là nhãn hiệu của Autodesk, Inc. Các nhãn hiệu khác thuộc chủ sở hữu tương ứng. "
                           "TrionCAD là tên làm việc."));
    terms_->setText(tr("Điều khoản sử dụng"));
    privacy_->setText(tr("Chính sách riêng tư"));
    support_->setText(tr("Liên hệ hỗ trợ"));
    for (ui::Button* b : {terms_, privacy_})
        b->setAccessibleDescription(tr("Mở trang web"));
    if (!failedUrl_.isEmpty())
        error_->setError(tr("Không mở được trình duyệt. Địa chỉ: %1").arg(failedUrl_.toString()));
    else if (!loadError_.isEmpty())
        error_->setError(tr("Không đọc được danh sách giấy phép."));
    error_->setVisible(!failedUrl_.isEmpty() || !loadError_.isEmpty());

    setConfirm(tr("Đóng"));
    fitHeights();
    if (isVisible()) { // dòng hệ thống về, đổi ngôn ngữ, dòng lỗi: cao theo nội dung ngay
        body()->layout()->activate();
        layout()->activate();
        adjustSize();
    }
}

void AboutDialog::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    DialogFrame::changeEvent(e);
}

bool AboutDialog::eventFilter(QObject* watched, QEvent* e)
{
    if (watched == table_ && e->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent*>(e)->key();
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            showLicense(table_->currentIndex().row());
            return true;
        }
    }
    return DialogFrame::eventFilter(watched, e);
}

} // namespace trion::about
