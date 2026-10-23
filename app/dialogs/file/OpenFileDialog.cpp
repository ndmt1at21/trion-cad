#include "app/dialogs/file/OpenFileDialog.h"

#include "app/core/a11y/Announce.h"
#include "app/core/i18n/UiFormat.h"
#include "app/theme/ThemeManager.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/menu/Menu.h"
#include "app/widgets/message/Messages.h"

#include <QApplication>
#include <QBoxLayout>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QPainterPath>
#include <QSplitter>
#include <QStandardPaths>
#include <QUrl>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shobjidl.h>
#include <string>
#include <vector>
#endif

using namespace Qt::StringLiterals;

namespace trion::files {

using theme::ThemeManager;

namespace {

constexpr QSize kDialogSize{880, 540}; // prototype TC.dialog w:880 h:540

int px(tok::PxId id)
{
    return qRound(ThemeManager::instance().px(id));
}

QColor col(tok::ColorId id)
{
    return ThemeManager::instance().color(id);
}

void setInk(QWidget* w, const QColor& c)
{
    QPalette p = w->palette();
    p.setColor(QPalette::Active, QPalette::WindowText, c);
    p.setColor(QPalette::Inactive, QPalette::WindowText, c);
    p.setColor(QPalette::Disabled, QPalette::WindowText, col(tok::c_text_dis));
    w->setPalette(p);
}

QLabel* wrapping(QWidget* parent)
{
    auto* l = new QLabel(parent);
    l->setWordWrap(true);
    return l;
}

} // namespace

// ------------------------------------------------------------------------------------------------- FilePreviewPane

FilePreviewPane::FilePreviewPane(QWidget* parent) : QWidget(parent)
{
    setObjectName(u"filePreview"_s);
    setFixedWidth(kWidth);
    auto* col = new QVBoxLayout(this);
    image_ = new QWidget(this);
    image_->setObjectName(u"thumbnail"_s);
    image_->installEventFilter(this);
    auto* noImage = new QVBoxLayout(image_);
    auto* empty = wrapping(image_);
    empty->setObjectName(u"noThumbnail"_s);
    empty->setAlignment(Qt::AlignCenter);
    noImage->addWidget(empty);
    col->addWidget(image_);

    name_ = wrapping(this);
    name_->setObjectName(u"fileName"_s);
    col->addWidget(name_);

    form_ = new QWidget(this);
    auto* grid = new QGridLayout(form_);
    grid->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < 4; ++i) {
        labels_[i] = wrapping(form_);
        labels_[i]->setFixedWidth(kLabelWidth);
        values_[i] = wrapping(form_);
        values_[i]->setObjectName(u"value%1"_s.arg(i));
        grid->addWidget(labels_[i], i, 0, Qt::AlignTop);
        grid->addWidget(values_[i], i, 1, Qt::AlignTop);
    }
    grid->setColumnStretch(1, 1);
    col->addWidget(form_);

    errorRow_ = new QWidget(this);
    auto* err = new QHBoxLayout(errorRow_);
    err->setContentsMargins(0, 0, 0, 0);
    errorIcon_ = new ui::Glyph(px(tok::ic), errorRow_);
    errorIcon_->set(u"warn"_s, tok::c_warn);
    error_ = wrapping(errorRow_);
    error_->setObjectName(u"previewError"_s);
    err->addWidget(errorIcon_, 0, Qt::AlignTop);
    err->addWidget(error_, 1);
    col->addWidget(errorRow_);

    count_ = new QLabel(this);
    count_->setObjectName(u"openCount"_s);
    count_->installEventFilter(this);
    col->addWidget(count_, 0, Qt::AlignLeft);

    hint_ = wrapping(this);
    hint_->setObjectName(u"previewHint"_s);
    col->addWidget(hint_);
    col->addStretch(1);

    connect(&ThemeManager::instance(), &ThemeManager::changed, this, &FilePreviewPane::applyTheme);
    applyTheme();
    retranslate();
    showHint();
}

void FilePreviewPane::applyTheme()
{
    const int pad = px(tok::sp_5), gap = px(tok::sp_4);
    auto* col = static_cast<QVBoxLayout*>(layout());
    col->setContentsMargins(pad, pad, pad, pad);
    col->setSpacing(gap);
    image_->setFixedHeight((kWidth - 2 * pad) * 2 / 3); // canvas 420×280 của prototype
    static_cast<QGridLayout*>(form_->layout())->setHorizontalSpacing(gap);
    static_cast<QGridLayout*>(form_->layout())->setVerticalSpacing(px(tok::sp_2));
    errorRow_->layout()->setSpacing(px(tok::sp_3));
    const QFont label = theme::font(theme::TextRole::Label);
    name_->setFont(theme::font(theme::TextRole::Ui, theme::Weight::Bold));
    setInk(name_, ::trion::files::col(tok::c_text));
    for (int i = 0; i < 4; ++i) {
        labels_[i]->setFont(label);
        values_[i]->setFont(label);
        setInk(labels_[i], ::trion::files::col(tok::c_text_2));
        setInk(values_[i], ::trion::files::col(tok::c_text));
    }
    for (QLabel* l : {error_, hint_, count_, image_->findChild<QLabel*>()})
        l->setFont(label);
    setInk(error_, ::trion::files::col(tok::c_text));
    setInk(hint_, ::trion::files::col(tok::c_text_3));
    setInk(image_->findChild<QLabel*>(), ::trion::files::col(tok::c_text_3));
    count_->setContentsMargins(px(tok::sp_3), px(tok::sp_1), px(tok::sp_3), px(tok::sp_1));
    setInk(count_, ::trion::files::col(tok::c_accent));
    update();
}

void FilePreviewPane::retranslate()
{
    setAccessibleName(tr("Xem trước"));
    labels_[0]->setText(tr("Phiên bản"));
    labels_[1]->setText(tr("Lưu lần cuối bởi"));
    labels_[2]->setText(tr("Ngày sửa"));
    labels_[3]->setText(tr("Kích thước"));
    hint_->setText(tr("Chọn một tệp để xem trước. Giữ Ctrl để chọn nhiều tệp, mỗi tệp mở thành một tab."));
    image_->findChild<QLabel*>()->setText(tr("Không có ảnh xem trước"));
    setCount(countValue_);
    if (hasInfo_)
        showInfo(info_, countValue_);
}

void FilePreviewPane::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void FilePreviewPane::setCount(int count)
{
    countValue_ = count;
    count_->setText(count > 1 ? tr("%n tệp sẽ mở thành %n tab", nullptr, count) : QString());
    count_->setVisible(count > 1);
}

void FilePreviewPane::showHint()
{
    path_.clear();
    loading_ = hasInfo_ = false;
    thumbnail_ = QImage();
    for (QWidget* w : {image_, static_cast<QWidget*>(name_), form_, errorRow_})
        w->hide();
    setCount(0);
    hint_->show();
}

void FilePreviewPane::showLoading(const QString& path, int count)
{
    path_ = path;
    loading_ = true;
    hasInfo_ = false;
    thumbnail_ = QImage();
    hint_->hide();
    errorRow_->hide();
    image_->show();
    image_->findChild<QLabel*>()->hide(); // đang đọc: chỉ nền ô ảnh
    name_->setText(QFileInfo(path).fileName());
    name_->show();
    for (QLabel* v : values_)
        v->setText(u"…"_s);
    form_->show();
    setCount(count);
    image_->update();
}

QString OpenFileDialog::infoValue(const FilePreviewInfo& info, int row)
{
    switch (row) {
    case 0:
        return info.version.isEmpty() ? u"—"_s : info.version;
    case 1:
        return info.savedBy.isEmpty() ? FilePreviewPane::tr("Không rõ") : info.savedBy;
    case 2:
        return info.modified.isValid() ? i18n::formatDateTime(info.modified.toLocalTime()) : u"—"_s;
    default:
        return info.size >= 0 ? i18n::formatBytes(info.size) : u"—"_s;
    }
}

void FilePreviewPane::showInfo(const FilePreviewInfo& info, int count)
{
    path_ = info.path;
    info_ = info;
    loading_ = false;
    hasInfo_ = true;
    thumbnail_ = info.thumbnail;
    hint_->hide();
    image_->show();
    image_->findChild<QLabel*>()->setVisible(thumbnail_.isNull());
    name_->setText(QFileInfo(info.path).fileName());
    name_->show();
    for (int i = 0; i < 4; ++i)
        values_[i]->setText(OpenFileDialog::infoValue(info, i));
    form_->setVisible(info.modified.isValid());
    error_->setText(FilePreviewService::errorText(info.error));
    errorRow_->setVisible(!info.ok());
    setCount(count);
    image_->update();
    // Như aria-live="polite" của #op-prev: đọc phiên bản, cỡ hoặc lỗi
    a11y::announce(this, info.ok() ? u"%1, %2"_s.arg(values_[0]->text(), values_[3]->text()) : error_->text());
}

QString FilePreviewPane::value(int row) const
{
    return row >= 0 && row < 4 && form_->isVisible() ? values_[row]->text() : QString();
}

QString FilePreviewPane::errorText() const
{
    return errorRow_->isVisible() ? error_->text() : QString();
}

QString FilePreviewPane::countText() const
{
    return count_->isVisible() ? count_->text() : QString();
}

bool FilePreviewPane::eventFilter(QObject* watched, QEvent* e)
{
    if (e->type() != QEvent::Paint)
        return QWidget::eventFilter(watched, e);
    auto* w = static_cast<QWidget*>(watched);
    QPainter p(w);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal r = ThemeManager::instance().px(tok::r);
    QPainterPath shape;
    shape.addRoundedRect(QRectF(w->rect()), r, r);
    if (watched == count_) { // .tag.acc: nền --c-accent-soft, chữ --c-accent (QLabel vẽ chữ)
        p.fillPath(shape, ::trion::files::col(tok::c_accent_soft));
        return false;
    }
    p.fillPath(shape, ::trion::files::col(tok::c_canvas));
    if (!thumbnail_.isNull()) { // ảnh nhúng vừa ô, giữ tỉ lệ
        QSizeF s = thumbnail_.deviceIndependentSize();
        s.scale(QSizeF(w->size()), Qt::KeepAspectRatio);
        const QRectF target(QPointF((w->width() - s.width()) / 2, (w->height() - s.height()) / 2), s);
        p.setClipPath(shape);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawImage(target, thumbnail_);
    }
    return true;
}

// -------------------------------------------------------------------------------------------- Windows: hộp gốc

#if defined(Q_OS_WIN)

namespace {
constexpr DWORD kOpenDrop = 1, kItemOpen = 1, kItemReadOnly = 2, kInfoText = 10; // id điều khiển của hộp gốc

std::wstring wide(const QString& s)
{
    return s.toStdWString();
}

QString itemPath(IShellItem* item)
{
    QString path;
    PWSTR p = nullptr;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) {
        path = QDir::fromNativeSeparators(QString::fromWCharArray(p));
        CoTaskMemFree(p);
    }
    return path;
}
} // namespace

// Sự kiện của IFileOpenDialog; đời sống theo OpenFileDialog (unique_ptr), số tham chiếu COM chỉ để đúng giao thức.
struct OpenFileDialog::Native final : IFileDialogEvents, IFileDialogControlEvents {
    explicit Native(OpenFileDialog& o) : owner(o) {}
    ~Native() { release(); }

    void release()
    {
        if (custom)
            custom->Release();
        if (dialog) {
            if (cookie)
                dialog->Unadvise(cookie);
            dialog->Release();
        }
        custom = nullptr;
        dialog = nullptr;
        cookie = 0;
    }

    bool prepare()
    {
        release();
        info = QStringList(4);
        current.clear();
        bool ok = SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                             IID_IFileOpenDialog, reinterpret_cast<void**>(&dialog)));
        if (!ok)
            return false;
        FILEOPENDIALOGOPTIONS opts = 0;
        ok &= SUCCEEDED(dialog->GetOptions(&opts));
        ok &= SUCCEEDED(dialog->SetOptions(opts | FOS_ALLOWMULTISELECT | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST
                                           | FOS_FORCEFILESYSTEM));
        std::vector<std::wstring> names, specs;
        for (int f = 0; f < FilterCount; ++f) {
            names.push_back(wide(filterLabel(Filter(f))));
            specs.push_back(wide(filterPatterns(Filter(f)).join(u';')));
        }
        COMDLG_FILTERSPEC types[FilterCount];
        for (int f = 0; f < FilterCount; ++f)
            types[f] = {names[std::size_t(f)].c_str(), specs[std::size_t(f)].c_str()};
        ok &= SUCCEEDED(dialog->SetFileTypes(FilterCount, types));
        ok &= SUCCEEDED(dialog->SetFileTypeIndex(1 + Dwg));
        ok &= SUCCEEDED(dialog->SetTitle(wide(OpenFileDialog::tr("Mở bản vẽ")).c_str()));
        ok &= SUCCEEDED(dialog->SetOkButtonLabel(wide(OpenFileDialog::tr("Mở")).c_str()));
        IShellItem* folder = nullptr; // thư mục không vào được (ổ mạng ngắt): để hộp tự chọn
        if (SUCCEEDED(SHCreateItemFromParsingName(wide(QDir::toNativeSeparators(owner.startDir())).c_str(), nullptr,
                                                  IID_IShellItem, reinterpret_cast<void**>(&folder)))) {
            dialog->SetFolder(folder);
            folder->Release();
        }
        for (const QString& place : owner.places_) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(wide(QDir::toNativeSeparators(place)).c_str(), nullptr,
                                                      IID_IShellItem, reinterpret_cast<void**>(&item)))) {
                dialog->AddPlace(item, FDAP_TOP);
                item->Release();
            }
        }
        ok &= SUCCEEDED(dialog->QueryInterface(IID_IFileDialogCustomize, reinterpret_cast<void**>(&custom)));
        if (!custom)
            return false;
        if (!owner.viewer_) { // nút Mở thả xuống
            ok &= SUCCEEDED(custom->EnableOpenDropDown(kOpenDrop));
            ok &= SUCCEEDED(custom->AddControlItem(kOpenDrop, kItemOpen, wide(OpenFileDialog::tr("Mở")).c_str()));
            ok &= SUCCEEDED(custom->AddControlItem(kOpenDrop, kItemReadOnly, wide(OpenFileDialog::tr("Mở chỉ đọc")).c_str()));
        }
        for (DWORD i = 0; i < 4; ++i)
            ok &= SUCCEEDED(custom->AddText(kInfoText + i, L""));
        ok &= SUCCEEDED(dialog->Advise(static_cast<IFileDialogEvents*>(this), &cookie));
        return ok;
    }

    // Nhãn thông tin "Phiên bản: DWG 2018"…; info rỗng: xóa.
    void show(const FilePreviewInfo* i)
    {
        static const char* const kLabels[] = {QT_TRANSLATE_NOOP("trion::files::FilePreviewPane", "Phiên bản"),
                                              QT_TRANSLATE_NOOP("trion::files::FilePreviewPane", "Lưu lần cuối bởi"),
                                              QT_TRANSLATE_NOOP("trion::files::FilePreviewPane", "Ngày sửa"),
                                              QT_TRANSLATE_NOOP("trion::files::FilePreviewPane", "Kích thước")};
        for (int r = 0; r < 4; ++r) {
            QString text;
            if (i && (r == 0 || i->modified.isValid()))
                text = u"%1: %2"_s.arg(FilePreviewPane::tr(kLabels[r]),
                                       r == 0 && !i->ok() ? FilePreviewService::errorText(i->error)
                                                          : OpenFileDialog::infoValue(*i, r));
            info[r] = text;
            if (custom)
                custom->SetControlLabel(kInfoText + DWORD(r), wide(text).c_str());
        }
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (riid == IID_IUnknown || riid == IID_IFileDialogEvents)
            *ppv = static_cast<IFileDialogEvents*>(this);
        else if (riid == IID_IFileDialogControlEvents)
            *ppv = static_cast<IFileDialogControlEvents*>(this);
        else {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return ULONG(InterlockedIncrement(&refs)); }
    IFACEMETHODIMP_(ULONG) Release() override { return ULONG(InterlockedDecrement(&refs)); }
    // IFileDialogEvents: đổi lựa chọn → đọc thông tin ở nền (FilePreviewService), nhãn cập nhật khi có kết quả
    IFACEMETHODIMP OnSelectionChange(IFileDialog* d) override
    {
        IShellItem* item = nullptr;
        QString path;
        if (SUCCEEDED(d->GetCurrentSelection(&item)) && item) {
            SFGAOF attrs = 0;
            if (SUCCEEDED(item->GetAttributes(SFGAO_FOLDER, &attrs)) && !(attrs & SFGAO_FOLDER))
                path = itemPath(item);
            item->Release();
        }
        if (path == current)
            return S_OK;
        current = path;
        show(nullptr);
        if (!path.isEmpty())
            owner.previews_.requestPreview(path);
        return S_OK;
    }
    IFACEMETHODIMP OnFileOk(IFileDialog*) override { return S_OK; }
    IFACEMETHODIMP OnFolderChanging(IFileDialog*, IShellItem*) override { return S_OK; }
    IFACEMETHODIMP OnFolderChange(IFileDialog*) override { return S_OK; }
    IFACEMETHODIMP OnShareViolation(IFileDialog*, IShellItem*, FDE_SHAREVIOLATION_RESPONSE*) override { return S_OK; }
    IFACEMETHODIMP OnTypeChange(IFileDialog*) override { return S_OK; }
    IFACEMETHODIMP OnOverwrite(IFileDialog*, IShellItem*, FDE_OVERWRITE_RESPONSE*) override { return S_OK; }
    // IFileDialogControlEvents
    IFACEMETHODIMP OnItemSelected(IFileDialogCustomize*, DWORD, DWORD) override { return S_OK; }
    IFACEMETHODIMP OnButtonClicked(IFileDialogCustomize*, DWORD) override { return S_OK; }
    IFACEMETHODIMP OnCheckButtonToggled(IFileDialogCustomize*, DWORD, BOOL) override { return S_OK; }
    IFACEMETHODIMP OnControlActivating(IFileDialogCustomize*, DWORD) override { return S_OK; }

    OpenFileDialog& owner;
    IFileOpenDialog* dialog = nullptr;
    IFileDialogCustomize* custom = nullptr;
    DWORD cookie = 0;
    LONG refs = 1;
    QStringList info;
    QString current; // tệp đang chọn trong hộp gốc
};

bool OpenFileDialog::prepareNative()
{
    if (!win_)
        win_ = std::make_unique<Native>(*this);
    return win_->prepare();
}

QStringList OpenFileDialog::nativeInfo() const
{
    return win_ ? win_->info : QStringList();
}

#else
struct OpenFileDialog::Native {};
#endif

// ------------------------------------------------------------------------------------------------- OpenFileDialog

OpenFileDialog::OpenFileDialog(settings::Store& store, FilePreviewService& previews, QWidget* parent)
    : QObject(parent), store_(store), previews_(previews), parent_(parent)
{
    connect(&previews_, &FilePreviewService::ready, this, &OpenFileDialog::previewReady);
}

OpenFileDialog::~OpenFileDialog()
{
    delete qt_.data(); // con của cửa sổ cha: hủy cùng bộ điều khiển (nối tới this)
}

QFileDialog* OpenFileDialog::qtDialog() const
{
    return qt_;
}

bool OpenFileDialog::native() const
{
#if defined(Q_OS_WIN)
    return native_;
#else
    return false;
#endif
}

void OpenFileDialog::setViewer(bool viewer)
{
    viewer_ = viewer;
}

void OpenFileDialog::setPlaces(const QStringList& folders)
{
    places_ = folders;
}

QString OpenFileDialog::filterLabel(Filter f)
{
    const QString patterns = filterPatterns(f).join(u' ');
    switch (f) {
    case Dwg:
        return u"%1 (%2)"_s.arg(tr("Bản vẽ"), patterns);
    case Dxf:
        return u"DXF (%1)"_s.arg(patterns);
    case Dwt:
        return u"%1 (%2)"_s.arg(tr("Mẫu bản vẽ"), patterns);
    default:
        return u"%1 (%2)"_s.arg(tr("Mọi tệp CAD"), patterns);
    }
}

QStringList OpenFileDialog::filterPatterns(Filter f)
{
    switch (f) {
    case Dwg:
        return {u"*.dwg"_s};
    case Dxf:
        return {u"*.dxf"_s};
    case Dwt:
        return {u"*.dwt"_s};
    default:
        return {u"*.dwg"_s, u"*.dxf"_s, u"*.dwt"_s};
    }
}

bool OpenFileDialog::matches(const QString& fileName, Filter f)
{
    for (const QString& p : filterPatterns(f))
        if (fileName.endsWith(QStringView(p).sliced(1), Qt::CaseInsensitive))
            return true;
    return false;
}

QString OpenFileDialog::openMacro(const QString& path, bool readOnly)
{
    return u"^C^C_OPEN %1\"%2\""_s.arg(readOnly ? u"_ReadOnly "_s : QString(), QDir::fromNativeSeparators(path));
}

QString OpenFileDialog::startDir() const
{
    if (const QString dir = store_.get(kLastDir); !dir.isEmpty())
        return dir;
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return docs.isEmpty() ? QDir::homePath() : docs;
}

void OpenFileDialog::open(Callback done)
{
    done_ = std::move(done);
    if (open_) {
        if (qt_)
            qt_->raise();
        return;
    }
    open_ = true;
    readOnly_ = false;
    selected_ = 0;
    returnFocus_ = QApplication::focusWidget();
#if defined(Q_OS_WIN)
    if (native_) {
        OpenResult result;
        // COM của luồng UI: Qt đã khởi tạo (OleInitialize) khi có plugin windows; offscreen thì tự khởi tạo.
        const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (prepareNative()) {
            QWidget* top = parent_ ? parent_->window() : nullptr;
            if (SUCCEEDED(win_->dialog->Show(top ? reinterpret_cast<HWND>(top->winId()) : nullptr))) {
                IShellItemArray* items = nullptr;
                DWORD n = 0;
                if (SUCCEEDED(win_->dialog->GetResults(&items)) && items && SUCCEEDED(items->GetCount(&n))) {
                    for (DWORD i = 0; i < n; ++i) {
                        IShellItem* item = nullptr;
                        if (SUCCEEDED(items->GetItemAt(i, &item)) && item) {
                            if (const QString p = itemPath(item); !p.isEmpty())
                                result.paths << p;
                            item->Release();
                        }
                    }
                }
                if (items)
                    items->Release();
                DWORD chosen = kItemOpen;
                if (!viewer_ && win_->custom)
                    win_->custom->GetSelectedControlItem(kOpenDrop, &chosen);
                result.readOnly = viewer_ || chosen == kItemReadOnly;
            }
        }
        win_->release();
        if (SUCCEEDED(com))
            CoUninitialize();
        if (!result.paths.isEmpty())
            store_.set(kLastDir, QFileInfo(result.paths.first()).absolutePath());
        finish(result);
        return;
    }
#endif
    openQt();
}

void OpenFileDialog::createQt()
{
    qt_ = new QFileDialog(parent_);
    qt_->setObjectName(u"openFileDialog"_s);
    qt_->setOption(QFileDialog::DontUseNativeDialog);
    qt_->setFileMode(QFileDialog::ExistingFiles);
    qt_->setAcceptMode(QFileDialog::AcceptOpen);
    qt_->resize(kDialogSize);
    qt_->installEventFilter(this);

    pane_ = new FilePreviewPane;
    if (auto* splitter = qt_->findChild<QSplitter*>(u"splitter"_s)) { // cột thứ ba cạnh danh sách, như prototype
        splitter->addWidget(pane_);
        splitter->setCollapsible(splitter->indexOf(pane_), false);
    } else if (auto* grid = qobject_cast<QGridLayout*>(qt_->layout())) {
        grid->addWidget(pane_, 0, grid->columnCount(), grid->rowCount(), 1);
    }

    // Nút Mở: SplitButton (Mở, Mở chỉ đọc) thay nút chuẩn (ẩn, vẫn do QFileDialog bật/tắt: nút mới theo nó)
    auto* box = qt_->findChild<QDialogButtonBox*>(u"buttonBox"_s);
    auto* menu = new ui::Menu(qt_);
    QAction* openAction = menu->addAction(QString());
    QAction* readOnlyAction = menu->addAction(QString());
    openAction->setObjectName(u"open"_s);
    readOnlyAction->setObjectName(u"openReadOnly"_s);
    connect(openAction, &QAction::triggered, this, [this] { static_cast<QDialog*>(qt_)->accept(); });
    connect(readOnlyAction, &QAction::triggered, this, [this] {
        readOnly_ = true;
        static_cast<QDialog*>(qt_)->accept(); // QFileDialog::accept kiểm tên tệp
        if (qt_->isVisible()) // tên tệp sai: hộp còn mở, lần Mở sau không chỉ đọc
            readOnly_ = false;
    });
    openSplit_ = new ui::SplitButton(QString(), menu);
    openSplit_->setObjectName(u"openButton"_s);
    openSplit_->setKind(ui::Button::Kind::Primary);
    openPlain_ = new ui::Button(QString(), ui::Button::Kind::Primary);
    openPlain_->setObjectName(u"openButtonViewer"_s);
    if (box) {
        if (QPushButton* standard = box->button(QDialogButtonBox::Open)) {
            standard->hide();
            standard->installEventFilter(this);
            openSplit_->setEnabled(standard->isEnabled());
            openPlain_->setEnabled(standard->isEnabled());
        }
        box->addButton(openSplit_, QDialogButtonBox::AcceptRole);
        box->addButton(openPlain_, QDialogButtonBox::AcceptRole);
    }

    connect(qt_, &QFileDialog::currentChanged, this, &OpenFileDialog::selectionChanged);
    if (auto* list = qt_->findChild<QListView*>(u"listView"_s); list && list->selectionModel())
        connect(list->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                &OpenFileDialog::selectionChanged);
    // Tệp QFileDialog đã kiểm (tồn tại) lúc chấp nhận, theo thứ tự tên trong ô Tên tệp
    connect(qt_, &QFileDialog::filesSelected, this, [this](const QStringList& files) { accepted_ = files; });
    connect(qt_, &QDialog::finished, this, [this](int code) {
        OpenResult result;
        if (code == QDialog::Accepted) {
            for (const QString& p : accepted_)
                result.paths << QDir::fromNativeSeparators(p);
            result.readOnly = viewer_ || readOnly_;
            if (!result.paths.isEmpty())
                store_.set(kLastDir, QFileInfo(result.paths.first()).absolutePath());
        }
        finish(result);
    });
}

void OpenFileDialog::retranslateQt()
{
    qt_->setWindowTitle(tr("Mở bản vẽ"));
    qt_->setLabelText(QFileDialog::LookIn, tr("Tìm trong:"));
    qt_->setLabelText(QFileDialog::FileName, tr("Tên tệp:"));
    qt_->setLabelText(QFileDialog::FileType, tr("Loại tệp:"));
    qt_->setLabelText(QFileDialog::Accept, tr("Mở"));
    qt_->setLabelText(QFileDialog::Reject, tr("Hủy"));
    QStringList filters;
    for (int f = 0; f < FilterCount; ++f)
        filters << filterLabel(Filter(f));
    const QString current = qt_->selectedNameFilter();
    const int index = std::max(0, int(qt_->nameFilters().indexOf(current)));
    qt_->setNameFilters(filters);
    qt_->selectNameFilter(filters[open_ ? index : int(Dwg)]);
    openSplit_->setText(tr("Mở"));
    openPlain_->setText(tr("Mở"));
    openSplit_->setAccessibleName(tr("Mở"));
    openSplit_->setAccessibleDescription(tr("Mũi tên: Mở chỉ đọc"));
    const QList<QAction*> items = openSplit_->dropMenu()->actions();
    items[0]->setText(tr("Mở"));
    items[1]->setText(tr("Mở chỉ đọc"));
    // Nút công cụ của QFileDialog: tên tiếng Việt cho trình đọc màn hình và tooltip (Qt chưa có bản dịch vi).
    const struct {
        const char* name;
        QString text;
    } tools[] = {{"backButton", tr("Quay lại")},          {"forwardButton", tr("Tiến tới")},
                 {"toParentButton", tr("Thư mục cha")},  {"newFolderButton", tr("Tạo thư mục mới")},
                 {"listModeButton", tr("Dạng danh sách")}, {"detailModeButton", tr("Dạng chi tiết")}};
    for (const auto& t : tools)
        if (auto* b = qt_->findChild<QWidget*>(QString::fromLatin1(t.name))) {
            b->setToolTip(t.text);
            b->setAccessibleName(t.text);
        }
}

void OpenFileDialog::openQt()
{
    const bool first = !qt_;
    if (first)
        createQt();
    retranslateQt();
    qt_->setDirectory(startDir());
    QList<QUrl> places{QUrl(u"file:"_s), QUrl::fromLocalFile(QDir::homePath())};
    for (const QString& f : places_)
        places << QUrl::fromLocalFile(f);
    qt_->setSidebarUrls(places);
    if (auto* name = qt_->findChild<QLineEdit*>(u"fileNameEdit"_s))
        name->clear();
    if (auto* list = qt_->findChild<QListView*>(u"listView"_s))
        list->clearSelection();
    openSplit_->setVisible(!viewer_);
    openPlain_->setVisible(viewer_);
    (viewer_ ? static_cast<QPushButton*>(openPlain_) : openSplit_)->setDefault(true);
    pane_->showHint();
    accepted_.clear();
    qt_->open(); // modal với cửa sổ cha, không chặn
}

void OpenFileDialog::selectionChanged()
{
    if (!qt_ || !open_)
        return;
    auto* list = qt_->findChild<QListView*>(u"listView"_s);
    auto* model = list ? qobject_cast<QFileSystemModel*>(list->model()) : nullptr;
    QString current;
    int count = 0;
    if (model) { // từ mô hình: không chạm đĩa trên luồng UI
        for (const QModelIndex& i : list->selectionModel()->selectedRows())
            if (!model->isDir(i)) {
                ++count;
                current = model->filePath(i);
            }
        const QModelIndex at = list->selectionModel()->currentIndex();
        if (at.isValid() && list->selectionModel()->isSelected(at) && !model->isDir(at))
            current = model->filePath(at); // dòng vừa bấm, như sel[sel.length - 1] của prototype
    }
    selected_ = count;
    if (current.isEmpty()) {
        previews_.cancel();
        pane_->showHint();
        return;
    }
    if (current != pane_->path() || pane_->loading()) {
        pane_->showLoading(current, count);
        previews_.requestPreview(current);
    } else {
        pane_->setCount(count); // cùng tệp, số tệp chọn đổi
    }
}

void OpenFileDialog::previewReady(const FilePreviewInfo& info)
{
    if (!open_)
        return;
#if defined(Q_OS_WIN)
    if (win_ && win_->dialog) {
        if (info.path == win_->current)
            win_->show(&info);
        return;
    }
#endif
    if (pane_ && info.path == pane_->path())
        pane_->showInfo(info, selected_);
}

void OpenFileDialog::finish(const OpenResult& result)
{
    open_ = false;
    previews_.cancel();
    if (returnFocus_ && returnFocus_->isVisible()) {
        returnFocus_->window()->activateWindow();
        returnFocus_->setFocus(Qt::OtherFocusReason);
    }
    Callback done = std::move(done_);
    done_ = {};
    if (done)
        done(result);
    for (const QString& p : result.paths)
        emit macroRequested(openMacro(p, result.readOnly));
}

bool OpenFileDialog::eventFilter(QObject* watched, QEvent* e)
{
    if (e->type() == QEvent::EnabledChange && watched != qt_) { // nút Mở chuẩn
        const bool on = static_cast<QWidget*>(watched)->isEnabled();
        openSplit_->setEnabled(on);
        openPlain_->setEnabled(on);
    } else if (e->type() == QEvent::LanguageChange && watched == qt_) {
        retranslateQt();
    }
    return QObject::eventFilter(watched, e);
}

} // namespace trion::files
