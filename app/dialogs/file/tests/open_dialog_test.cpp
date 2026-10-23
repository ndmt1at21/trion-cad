// UIB-002: hộp thoại Mở tệp (OpenFileDialog.h). Hộp của Qt (setNative(false)) chạy ở mọi nền tảng; hộp gốc Windows chỉ
// kiểm phần dựng được không cần người bấm (nativeConfig); bấm trong hộp gốc: kịch bản tests/ui/open_dialog.scenario.
// DOC-018, INT-002, CMD-240 chưa có: kết quả kiểm tới callback và macroRequested (3 tab, lịch sử dòng lệnh: Hoãn).
//   filters        Bản vẽ, DXF, Mẫu bản vẽ, Mọi tệp CAD lọc đúng đuôi, không phân biệt hoa thường (tiêu chí 4).
//   multiOpen      3 tệp (một tên tiếng Việt có dấu) → callback đủ 3 đường dẫn, mỗi tệp một macro OPEN (tiêu chí 1).
//   readOnly       mục "Mở chỉ đọc" → readOnly, macro _ReadOnly; Viewer: nút Mở thường, luôn chỉ đọc (tiêu chí 2, 5).
//   lastDirectory  nhớ thư mục cuối qua Store, mặc định thư mục Tài liệu.
//   escape         Esc đóng, callback rỗng, focus về vùng vẽ (tiêu chí 5).
//   preview        đổi lựa chọn bằng phím → khung xem trước đúng (phiên bản, lưu lần cuối bởi, ngày, cỡ; không có ảnh: chữ;
//                  lỗi: câu giải thích; nhiều tệp: số tab) ≤ 50 ms p99 (release) (tiêu chí 3).
//   firstFrame     mở hộp đến khung đầu ≤ 150 ms (release) (tiêu chí 5).
//   macros         đường dẫn tiếng Việt, UNC → macro dấu '/', ngoặc kép (tiêu chí 4).
//   pseudo         vi-XL: khung xem trước, nút Mở không cắt chữ; tên trợ năng.
//   nativeConfig   (Windows) IFileOpenDialog đủ cấu hình: bộ lọc, nút Mở thả xuống, nhãn thông tin.
#include "app/dialogs/file/OpenFileDialog.h"

#include "app/core/UiExecutor.h"
#include "app/core/i18n/Translator.h"
#include "app/core/i18n/UiFormat.h"
#include "app/core/i18n/TruncationProbe.h"
#include "app/core/keymap/Keymap.h"
#include "app/theme/ThemeManager.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/widgets/controls/Controls.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/settings/Settings.h"
#include "tests/timing.h"

#include <QApplication>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <chrono>
#include <memory>

#if defined(Q_OS_WIN)
#include <objbase.h>
#endif

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using namespace trion::files;
using trion::test::kTimed;
namespace i18n = trion::i18n;
namespace sched = trion::sched;
namespace settings = trion::settings;
namespace theme = trion::theme;
namespace ui = trion::ui;
using Clock = std::chrono::steady_clock;

namespace {

bool spinUntil(const std::function<bool()>& cond, std::chrono::milliseconds timeout = 10s)
{
    if (cond())
        return true;
    QEventLoop loop;
    QTimer tick;
    QObject::connect(&tick, &QTimer::timeout, &loop, [&] {
        if (cond())
            loop.quit();
    });
    QTimer::singleShot(timeout, &loop, &QEventLoop::quit);
    tick.start(1);
    loop.exec();
    return cond();
}

double ms(Clock::duration d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

// Header DWG 2018 hợp lệ (khối 0x6C byte mã hóa, như file_preview_test) đệm tới size.
QByteArray dwg2018(qint64 size)
{
    QByteArray h(0x100, '\0');
    h.replace(0, 6, "AC1032");
    QByteArray d(0x6C, '\0');
    d.replace(0, 12, QByteArray("AcFssFcAJMB", 12));
    const auto put = [&](int at, quint64 v, int n) {
        for (int i = 0; i < n; ++i)
            d[at + i] = char((v >> (8 * i)) & 0xFF);
    };
    put(0x10, 0x6C, 4);
    put(0x14, 4, 4);
    put(0x44, 0x20, 4);
    put(0x48, 0x80, 4);
    put(0x4C, 0x40, 4);
    put(0x54, quint64(size) - 0x200, 8);
    quint32 seed = 1;
    for (qsizetype i = 0; i < d.size(); ++i) {
        seed = seed * 0x343FDu + 0x269EC3u;
        d[i] = char(uchar(d[i]) ^ uchar(seed >> 16));
    }
    h.replace(0x80, 0x6C, d);
    return h + QByteArray(size - h.size(), 'Z');
}

const QByteArray kDxf = "  0\nSECTION\n  2\nHEADER\n  9\n$ACADVER\n  1\nAC1027\n  0\nENDSEC\n  0\nEOF\n";

// Tệp mẫu: tên → nội dung.
const QList<std::pair<QString, QByteArray>>& samples()
{
    static const QList<std::pair<QString, QByteArray>> s{
        {u"Bản vẽ nhà phố.dwg"_s, dwg2018(40'000)}, // i18n-ok: tên tệp mẫu
        {u"KET-CAU.DWG"_s, dwg2018(20'000)},
        {u"mau.Dwt"_s, dwg2018(10'000)},
        {u"thong-ke.dxf"_s, kDxf},
        {u"hong.dwg"_s, "%PDF-1.7\n"},
        {u"ghi-chu.pdf"_s, "%PDF-1.7\n"},
        {u"anh.png"_s, "\x89PNG\r\n"},
    };
    return s;
}

} // namespace

class OpenDialogTest : public QObject {
    Q_OBJECT

private:
    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<ui::UiExecutor> executor_;
    std::unique_ptr<theme::ThemeManager> tm_;
    std::unique_ptr<theme::IconProvider> icons_;
    std::unique_ptr<i18n::Translator> translator_;
    std::unique_ptr<trion::keymap::Keymap> keymap_;
    std::unique_ptr<ui::RichTooltip> tips_;
    std::unique_ptr<FilePreviewService> previews_;
    std::unique_ptr<QWidget> main_;
    QWidget* canvas_ = nullptr;

    QString files() const { return dir_.filePath(u"files"_s); }
    QString path(const QString& name) const { return files() + u'/' + name; }

    // Hộp của Qt mở ở thư mục mẫu, danh sách đã nạp.
    std::unique_ptr<OpenFileDialog> openAt(const QString& folder, OpenFileDialog::Callback done = {}, bool hasFiles = true)
    {
        store_->set(OpenFileDialog::kLastDir, folder);
        auto dlg = std::make_unique<OpenFileDialog>(*store_, *previews_, main_.get());
        dlg->setNative(false);
        dlg->open(std::move(done));
        QFileDialog* q = dlg->qtDialog();
        if (!q || !spinUntil([&] { return q->isVisible() && (!hasFiles || visibleNames(*dlg).size() > 0); }))
            qWarning("hộp thoại chưa hiện hoặc chưa nạp danh sách");
        return dlg;
    }

    static QListView* list(const OpenFileDialog& dlg) { return dlg.qtDialog()->findChild<QListView*>(u"listView"_s); }

    // Tên tệp (không thư mục) danh sách đang hiện theo bộ lọc.
    static QStringList visibleNames(const OpenFileDialog& dlg)
    {
        QListView* l = list(dlg);
        auto* model = qobject_cast<QFileSystemModel*>(l->model());
        QStringList out;
        if (!model)
            return out;
        const QModelIndex root = l->rootIndex();
        for (int r = 0; r < model->rowCount(root); ++r) {
            const QModelIndex i = model->index(r, 0, root);
            if (!model->isDir(i))
                out << model->fileName(i);
        }
        out.sort();
        return out;
    }

    static void typeNames(OpenFileDialog& dlg, const QString& text)
    {
        auto* edit = dlg.qtDialog()->findChild<QLineEdit*>(u"fileNameEdit"_s);
        QVERIFY(edit);
        edit->setText(text);
        QVERIFY(spinUntil([&] { return dlg.qtDialog()->findChild<QWidget*>(u"openButton"_s)->isEnabled()
                                       || dlg.qtDialog()->findChild<QWidget*>(u"openButtonViewer"_s)->isEnabled(); }));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        QVERIFY(theme::loadFonts());
        executor_ = std::make_unique<ui::UiExecutor>();
        tm_ = std::make_unique<theme::ThemeManager>(*store_);
        icons_ = std::make_unique<theme::IconProvider>();
        translator_ = std::make_unique<i18n::Translator>();
        keymap_ = std::make_unique<trion::keymap::Keymap>(*store_, trion::keymap::Platform::Windows);
        tips_ = std::make_unique<ui::RichTooltip>();
        tips_->setKeymap(keymap_.get());
        previews_ = std::make_unique<FilePreviewService>();
        QVERIFY(QDir().mkpath(files() + u"/con"_s));
        for (const auto& [name, content] : samples()) {
            QFile f(path(name));
            QVERIFY(f.open(QIODevice::WriteOnly));
            QCOMPARE(f.write(content), content.size());
        }
        main_ = std::make_unique<QWidget>();
        main_->resize(1366, 768);
        canvas_ = new QWidget(main_.get());
        canvas_->setObjectName(u"canvas"_s);
        canvas_->setFocusPolicy(Qt::StrongFocus);
        canvas_->setGeometry(0, 0, 400, 300);
        main_->show();
        main_->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(main_.get()));
        canvas_->setFocus();
    }

    void init() { translator_->setLanguage(u"vi"_s); }

    void cleanupTestCase()
    {
        main_.reset();
        previews_.reset();
        sched::waitIdle();
        tips_.reset();
        keymap_.reset();
        translator_.reset();
        icons_.reset();
        tm_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
        QCoreApplication::processEvents();
        executor_.reset();
    }

    // Tiêu chí 5: mở hộp đến khung đầu ≤ 150 ms (lần mở đầu, dựng QFileDialog).
    void firstFrame()
    {
        store_->set(OpenFileDialog::kLastDir, files());
        OpenFileDialog dlg(*store_, *previews_, main_.get());
        dlg.setNative(false);
        Clock::time_point painted{};
        struct Filter : QObject {
            Clock::time_point* at;
            bool eventFilter(QObject*, QEvent* e) override
            {
                if (e->type() == QEvent::Paint && *at == Clock::time_point{})
                    *at = Clock::now();
                return false;
            }
        } filter;
        filter.at = &painted;
        qApp->installEventFilter(&filter);
        const auto t0 = Clock::now();
        dlg.open();
        QVERIFY(spinUntil([&] { return painted != Clock::time_point{} && dlg.qtDialog()->isVisible(); }));
        qApp->removeEventFilter(&filter);
        const double first = ms(painted - t0);
        qInfo("mở hộp → khung đầu: %.1f ms", first);
        QVERIFY2(!kTimed || first <= 150, qPrintable(QString::number(first)));
        QVERIFY(dlg.isOpen());
        dlg.qtDialog()->reject();
        QVERIFY(!dlg.isOpen());
    }

    // Tiêu chí 4: bộ lọc đúng đuôi, không phân biệt hoa thường.
    void filters()
    {
        QCOMPARE(OpenFileDialog::filterLabel(OpenFileDialog::Dwg), u"Bản vẽ (*.dwg)"_s);
        QCOMPARE(OpenFileDialog::filterLabel(OpenFileDialog::AllCad), u"Mọi tệp CAD (*.dwg *.dxf *.dwt)"_s);
        QVERIFY(OpenFileDialog::matches(u"A.DWG"_s, OpenFileDialog::Dwg));
        QVERIFY(OpenFileDialog::matches(u"b.Dxf"_s, OpenFileDialog::AllCad));
        QVERIFY(!OpenFileDialog::matches(u"c.dwg.bak"_s, OpenFileDialog::AllCad));
        QVERIFY(!OpenFileDialog::matches(u"d.dxf"_s, OpenFileDialog::Dwt));
        auto dlg = openAt(files());
        QFileDialog* q = dlg->qtDialog();
        QVERIFY(q);
        QCOMPARE(q->selectedNameFilter(), OpenFileDialog::filterLabel(OpenFileDialog::Dwg)); // mặc định như prototype
        const QStringList dwg{u"Bản vẽ nhà phố.dwg"_s, u"KET-CAU.DWG"_s, u"hong.dwg"_s};
        QVERIFY2(spinUntil([&] { return visibleNames(*dlg) == dwg; }), qPrintable(visibleNames(*dlg).join(u", ")));
        const struct {
            OpenFileDialog::Filter f;
            QStringList names;
        } cases[] = {{OpenFileDialog::Dxf, {u"thong-ke.dxf"_s}},
                     {OpenFileDialog::Dwt, {u"mau.Dwt"_s}},
                     {OpenFileDialog::AllCad,
                      {u"Bản vẽ nhà phố.dwg"_s, u"KET-CAU.DWG"_s, u"hong.dwg"_s, u"mau.Dwt"_s, u"thong-ke.dxf"_s}}};
        for (const auto& c : cases) {
            q->selectNameFilter(OpenFileDialog::filterLabel(c.f));
            QStringList want = c.names;
            want.sort();
            QVERIFY2(spinUntil([&] { return visibleNames(*dlg) == want; }), qPrintable(visibleNames(*dlg).join(u", ")));
        }
        q->reject();
    }

    // Tiêu chí 1: chọn 3 tệp rồi Mở → 3 đường dẫn, mỗi tệp một lệnh OPEN theo thứ tự.
    void multiOpen()
    {
        OpenResult got;
        int calls = 0;
        auto dlg = openAt(files(), [&](const OpenResult& r) {
            got = r;
            ++calls;
        });
        QStringList macros;
        connect(dlg.get(), &OpenFileDialog::macroRequested, this, [&](const QString& m) { macros << m; });
        dlg->qtDialog()->selectNameFilter(OpenFileDialog::filterLabel(OpenFileDialog::AllCad));
        QVERIFY(spinUntil([&] { return visibleNames(*dlg).size() == 5; })); // tên gõ phải là tệp đang hiện
        typeNames(*dlg, u"\"Bản vẽ nhà phố.dwg\" \"thong-ke.dxf\" \"mau.Dwt\""_s); // i18n-ok: tên tệp mẫu
        auto* open = dlg->qtDialog()->findChild<ui::SplitButton*>(u"openButton"_s);
        QVERIFY(open && open->isVisible() && open->isDefault());
        QTest::mouseClick(open, Qt::LeftButton, {}, QPoint(open->height() / 2, open->height() / 2)); // phần chính
        QVERIFY(spinUntil([&] { return calls == 1; }));
        QVERIFY(!dlg->isOpen());
        const QStringList want{path(u"Bản vẽ nhà phố.dwg"_s), path(u"thong-ke.dxf"_s), path(u"mau.Dwt"_s)};
        QCOMPARE(got.paths, want);
        QVERIFY(!got.readOnly);
        QCOMPARE(macros.size(), 3);
        for (int i = 0; i < 3; ++i)
            QCOMPARE(macros[i], u"^C^C_OPEN \"%1\""_s.arg(want[i]));
    }

    // Tiêu chí 2, 5: "Mở chỉ đọc"; Viewer luôn chỉ đọc, không có mục riêng.
    void readOnly()
    {
        OpenResult got;
        auto dlg = openAt(files(), [&](const OpenResult& r) { got = r; });
        typeNames(*dlg, u"KET-CAU.DWG"_s);
        auto* open = dlg->qtDialog()->findChild<ui::SplitButton*>(u"openButton"_s);
        QAction* ro = open->dropMenu()->findChild<QAction*>(u"openReadOnly"_s);
        QVERIFY(ro);
        QCOMPARE(ro->text(), u"Mở chỉ đọc"_s);
        QStringList macros;
        connect(dlg.get(), &OpenFileDialog::macroRequested, this, [&](const QString& m) { macros << m; });
        ro->trigger();
        QVERIFY(spinUntil([&] { return !dlg->isOpen(); }));
        QCOMPARE(got.paths, QStringList{path(u"KET-CAU.DWG"_s)});
        QVERIFY(got.readOnly);
        QCOMPARE(macros, QStringList{u"^C^C_OPEN _ReadOnly \"%1\""_s.arg(path(u"KET-CAU.DWG"_s))});

        // Mở lại: không còn chỉ đọc
        dlg->open([&](const OpenResult& r) { got = r; });
        QVERIFY(spinUntil([&] { return dlg->qtDialog()->isVisible(); }));
        typeNames(*dlg, u"KET-CAU.DWG"_s);
        QTest::keyClick(dlg->qtDialog()->findChild<QLineEdit*>(u"fileNameEdit"_s), Qt::Key_Return);
        QVERIFY(spinUntil([&] { return !dlg->isOpen(); }));
        QVERIFY(!got.readOnly);

        // Viewer: nút Mở thường, DWG/DXF/DWT mở được, luôn chỉ đọc
        dlg->setViewer(true);
        dlg->open([&](const OpenResult& r) { got = r; });
        QVERIFY(spinUntil([&] { return dlg->qtDialog()->isVisible(); }));
        QVERIFY(!dlg->qtDialog()->findChild<QWidget*>(u"openButton"_s)->isVisible());
        auto* plain = dlg->qtDialog()->findChild<ui::Button*>(u"openButtonViewer"_s);
        QVERIFY(plain->isVisible() && plain->isDefault());
        dlg->qtDialog()->selectNameFilter(OpenFileDialog::filterLabel(OpenFileDialog::AllCad));
        QVERIFY(spinUntil([&] { return visibleNames(*dlg).size() == 5; }));
        typeNames(*dlg, u"\"thong-ke.dxf\" \"mau.Dwt\" \"KET-CAU.DWG\""_s);
        QTest::mouseClick(plain, Qt::LeftButton);
        QVERIFY(spinUntil([&] { return !dlg->isOpen(); }));
        QCOMPARE(got.paths.size(), 3);
        QVERIFY(got.readOnly);
    }

    void lastDirectory()
    {
        store_->reset(OpenFileDialog::kLastDir);
        const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        QVERIFY(docs.isEmpty() || QDir().mkpath(docs));
        {
            OpenFileDialog dlg(*store_, *previews_, main_.get());
            dlg.setNative(false);
            dlg.open();
            QCOMPARE(QDir(dlg.qtDialog()->directory()).canonicalPath(),
                     QDir(docs.isEmpty() ? QDir::homePath() : docs).canonicalPath());
            dlg.qtDialog()->reject();
        }
        OpenResult got;
        auto dlg = openAt(files() + u"/con"_s, [&](const OpenResult& r) { got = r; }, false);
        dlg->qtDialog()->setDirectory(files());
        typeNames(*dlg, u"KET-CAU.DWG"_s);
        static_cast<QDialog*>(dlg->qtDialog())->accept();
        QVERIFY(spinUntil([&] { return !dlg->isOpen(); }));
        QCOMPARE(store_->get(OpenFileDialog::kLastDir), files());
        dlg->open();
        QCOMPARE(QDir(dlg->qtDialog()->directory()).canonicalPath(), QDir(files()).canonicalPath());
        dlg->qtDialog()->reject();
    }

    // Tiêu chí 5: Esc đóng, không mở gì, focus về vùng vẽ.
    void escape()
    {
        main_->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(main_.get()));
        canvas_->setFocus();
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == canvas_; }));
        int calls = 0;
        OpenResult got{{u"x"_s}, true};
        auto dlg = openAt(files(), [&](const OpenResult& r) {
            got = r;
            ++calls;
        });
        QStringList macros;
        connect(dlg.get(), &OpenFileDialog::macroRequested, this, [&](const QString& m) { macros << m; });
        QVERIFY(QApplication::focusWidget() != canvas_);
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
        QVERIFY(spinUntil([&] { return calls == 1; }));
        QVERIFY(got.paths.isEmpty());
        QVERIFY(macros.isEmpty());
        QVERIFY(!dlg->qtDialog()->isVisible());
        QVERIFY(spinUntil([&] { return QApplication::focusWidget() == canvas_; }));
    }

    // Tiêu chí 3: khung xem trước theo lựa chọn ≤ 50 ms p99; không có ảnh: chữ; lỗi: câu giải thích; nhiều tệp: số tab.
    void preview()
    {
        previews_->setThumbnailSource([](const QString& p, sched::CancelToken) {
            if (!p.contains(u"KET-CAU"_s))
                return QImage(); // tệp không có ảnh nhúng
            QImage img(120, 80, QImage::Format_ARGB32_Premultiplied);
            img.fill(0xff3cc8c0u);
            return img;
        });
        auto dlg = openAt(files());
        FilePreviewPane* pane = dlg->previewPane();
        QVERIFY(pane && pane->isVisible());
        QVERIFY(pane->findChild<QLabel*>(u"previewHint"_s)->isVisible()); // chưa chọn: gợi ý
        dlg->qtDialog()->selectNameFilter(OpenFileDialog::filterLabel(OpenFileDialog::AllCad));
        QVERIFY(spinUntil([&] { return visibleNames(*dlg).size() == 5; }));
        QListView* l = list(*dlg);
        l->setFocus();
        int paints = 0;
        struct Filter : QObject {
            int* n;
            bool eventFilter(QObject*, QEvent* e) override
            {
                if (e->type() == QEvent::Paint)
                    ++*n;
                return false;
            }
        } filter;
        filter.n = &paints;
        pane->findChild<QWidget*>(u"thumbnail"_s)->installEventFilter(&filter);
        std::vector<double> times;
        auto* model = qobject_cast<QFileSystemModel*>(l->model());
        for (int i = 0; i < 40; ++i) {
            const auto t0 = Clock::now();
            QTest::keyClick(l, i % 10 < 5 ? Qt::Key_Down : Qt::Key_Up);
            const QString current = model->filePath(l->currentIndex());
            const int before = paints;
            QVERIFY(spinUntil([&] {
                return model->isDir(l->currentIndex()) || (pane->path() == current && !pane->loading() && paints > before);
            }));
            if (!model->isDir(l->currentIndex()))
                times.push_back(ms(Clock::now() - t0));
        }
        std::sort(times.begin(), times.end());
        const double p99 = times.empty() ? 0 : times[std::min(times.size() - 1, std::size_t(times.size() * 0.99))];
        qInfo("đổi lựa chọn → khung xem trước: p50 %.1f ms, p99 %.1f ms (%zu lần)", times[times.size() / 2], p99,
              times.size());
        QVERIFY(times.size() >= 20);
        QVERIFY2(!kTimed || p99 <= 50, qPrintable(QString::number(p99)));

        // Nội dung theo từng tệp
        const auto select = [&](const QString& name) { // như bấm một dòng
            l->selectionModel()->setCurrentIndex(model->index(path(name)),
                                                 QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            QVERIFY(spinUntil([&] { return pane->path() == path(name) && !pane->loading(); }));
        };
        select(u"KET-CAU.DWG"_s);
        QCOMPARE(pane->value(0), u"DWG 2018"_s);
        QCOMPARE(pane->value(1), u"Không rõ"_s); // chưa có DWG-027
        QCOMPARE(pane->value(3), i18n::formatBytes(20'000));
        QVERIFY(!pane->value(2).isEmpty());
        QVERIFY(!pane->thumbnail().isNull());
        QVERIFY(!pane->findChild<QLabel*>(u"noThumbnail"_s)->isVisible());
        select(u"thong-ke.dxf"_s);
        QCOMPARE(pane->value(0), u"DXF 2013"_s);
        QVERIFY(pane->thumbnail().isNull());
        QVERIFY(pane->findChild<QLabel*>(u"noThumbnail"_s)->isVisible()); // trạng thái rỗng có chữ
        QCOMPARE(pane->findChild<QLabel*>(u"noThumbnail"_s)->text(), u"Không có ảnh xem trước"_s);
        QVERIFY(pane->errorText().isEmpty());
        select(u"hong.dwg"_s);
        QCOMPARE(pane->errorText(), FilePreviewService::errorText(FilePreviewInfo::Error::NotDrawing));
        previews_->setSavedBySource([](const QString&, sched::CancelToken) { return u"AutoCAD 2024"_s; });
        select(u"Bản vẽ nhà phố.dwg"_s); // i18n-ok: tên tệp mẫu
        QCOMPARE(pane->value(1), u"AutoCAD 2024"_s);

        // Nhiều tệp: Shift+↓ thêm một tệp vào vùng chọn
        l->setFocus();
        QTest::keyClick(l, Qt::Key_Down, Qt::ShiftModifier);
        QVERIFY(spinUntil([&] { return !pane->countText().isEmpty(); }));
        QCOMPARE(pane->countText(), u"2 tệp sẽ mở thành 2 tab"_s);
        dlg->qtDialog()->reject();
        previews_->setThumbnailSource({});
        previews_->setSavedBySource({});
    }

    // Tiêu chí 4: đường dẫn tiếng Việt, UNC → macro dấu '/', trong ngoặc kép.
    void macros()
    {
        // Đường dẫn theo hệ điều hành (Windows: '\\', UNC "\\\\máy\\chia-sẻ") → macro luôn dấu '/'
        const QString viet = QDir::toNativeSeparators(u"D:/DuAn/Nhà phố/Bản vẽ 01.dwg"_s); // i18n-ok: đường dẫn mẫu
        QCOMPARE(OpenFileDialog::openMacro(viet, false), u"^C^C_OPEN \"D:/DuAn/Nhà phố/Bản vẽ 01.dwg\""_s); // i18n-ok
        const QString unc = QDir::toNativeSeparators(u"//SRV-01/DuAn/2026/THCS-AnPhu/MB.dwg"_s);
        QCOMPARE(OpenFileDialog::openMacro(unc, true), u"^C^C_OPEN _ReadOnly \"//SRV-01/DuAn/2026/THCS-AnPhu/MB.dwg\""_s);
        // Tên tiếng Việt đi qua hộp thoại nguyên vẹn: multiOpen. UNC thật chỉ có trên Windows (\\localhost\<ổ>$ cần
        // quyền quản trị): kịch bản open_dialog.scenario.
    }

    // vi-XL: không cắt chữ ở khung xem trước và nút Mở; tên cho trình đọc màn hình.
    void pseudo()
    {
        translator_->setLanguage(i18n::kPseudoLanguage.toString());
        QCoreApplication::processEvents(); // LanguageChange đăng hàng đợi
        auto dlg = openAt(files());
        auto* model = qobject_cast<QFileSystemModel*>(list(*dlg)->model());
        list(*dlg)->selectionModel()->setCurrentIndex(model->index(path(u"KET-CAU.DWG"_s)),
                                                      QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        FilePreviewPane* pane = dlg->previewPane();
        QVERIFY(spinUntil([&] { return pane->path() == path(u"KET-CAU.DWG"_s) && !pane->loading(); }));
        QCoreApplication::processEvents();
        QStringList cut;
        for (QWidget* root : {static_cast<QWidget*>(pane), dlg->qtDialog()->findChild<QWidget*>(u"openButton"_s)})
            for (const i18n::Truncation& t : i18n::findTruncations(root))
                cut << t.toString();
        QVERIFY2(cut.isEmpty(), qPrintable(cut.join(u"; ")));
        QVERIFY(!pane->accessibleName().isEmpty());
        QVERIFY(!dlg->qtDialog()->findChild<QWidget*>(u"openButton"_s)->accessibleName().isEmpty());
        for (const char* name : {"backButton", "toParentButton", "newFolderButton", "detailModeButton"})
            if (auto* b = dlg->qtDialog()->findChild<QWidget*>(QString::fromLatin1(name)))
                QVERIFY2(!b->accessibleName().isEmpty(), name);
        dlg->qtDialog()->reject();
        translator_->setLanguage(u"vi"_s);
        QCoreApplication::processEvents();
    }

    // Windows: hộp gốc dựng đủ cấu hình (không hiện). Bấm trong hộp gốc: kịch bản QA-011.
    void nativeConfig()
    {
#if defined(Q_OS_WIN)
        const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        {
            OpenFileDialog dlg(*store_, *previews_, main_.get());
            QVERIFY(dlg.native());
            dlg.setPlaces({files()});
            QVERIFY(dlg.prepareNative());
            QCOMPARE(dlg.nativeInfo().size(), 4);
            dlg.setViewer(true);
            QVERIFY(dlg.prepareNative());
        }
        if (SUCCEEDED(com))
            CoUninitialize();
#else
        QSKIP("hộp gốc IFileDialog chỉ có trên Windows");
#endif
    }
};

QTEST_MAIN(OpenDialogTest)
#include "open_dialog_test.moc"
