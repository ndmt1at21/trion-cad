#include "app/core/i18n/Translator.h"

#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QErrorMessage>
#include <QHBoxLayout>
#include <QHash>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QVBoxLayout>

#include <algorithm>
#include <memory>

using namespace Qt::StringLiterals;
using trion::i18n::Translator;
using trion::settings::Store;

// Ví dụ quy ước retranslate() (Translator.h): dựng cây widget một lần, mọi chuỗi đặt trong retranslate(),
// gọi từ constructor và changeEvent(LanguageChange). Mỗi Panel có 4 widget mang chữ.
class Panel : public QWidget {
    Q_OBJECT
public:
    explicit Panel(int n, QWidget* parent = nullptr) : QWidget(parent), n_(n)
    {
        auto* row = new QHBoxLayout(this);
        row->setContentsMargins(0, 0, 0, 0);
        row->addWidget(label);
        row->addWidget(edit);
        row->addWidget(button);
        row->addWidget(combo);
        combo->addItems({QString(), QString()}); // chữ của mục đặt trong retranslate()
        retranslate();
    }

    QLabel* label = new QLabel(this);
    QLineEdit* edit = new QLineEdit(this);
    QPushButton* button = new QPushButton(this);
    QComboBox* combo = new QComboBox(this);

protected:
    void changeEvent(QEvent* e) override
    {
        if (e->type() == QEvent::LanguageChange)
            retranslate();
        QWidget::changeEvent(e);
    }

private:
    void retranslate() // chỉ đặt chuỗi: không đụng mục đang chọn, nội dung ô nhập, vị trí cuộn
    {
        label->setText(tr("Hàng %1").arg(n_));
        edit->setPlaceholderText(tr("Nhập tên"));
        button->setText(tr("%n đối tượng", nullptr, n_));
        button->setToolTip(tr("Chọn các đối tượng của hàng này"));
        combo->setItemText(0, tr("Mặc định"));
        combo->setItemText(1, tr("Tùy chỉnh"));
    }

    int n_;
};

class TranslatorTest : public QObject {
    Q_OBJECT
private slots:
    void firstRunIsVietnamese()
    {
        QTemporaryDir dir;
        Store s(dir.filePath(u"user.json"_s));
        Store::setInstance(&s);
        {
            Translator t;
            QCOMPARE(Translator::instance(), &t);
            QCOMPARE(t.language(), u"vi"_s);
            QCOMPARE(tr("Đã lưu %1 vào %2").arg(u"A.dwg"_s, u"D:"_s), u"Đã lưu A.dwg vào D:"_s);
            QVERIFY(!t.setLanguage(u"fr"_s)); // mã lạ: giữ nguyên
            QCOMPARE(t.language(), u"vi"_s);
        }
        QCOMPARE(Translator::instance(), nullptr);
        s.set(trion::i18n::kLanguage, u"xx"_s); // tệp cài đặt sửa tay sai
        Translator t;
        QCOMPARE(t.language(), u"vi"_s);
    }

    void choiceSurvivesRestart()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"user.json"_s);
        {
            Store s(path);
            Store::setInstance(&s);
            Translator t;
            QSignalSpy changed(&t, &Translator::languageChanged);
            QVERIFY(t.setLanguage(u"en"_s));
            QVERIFY(t.setLanguage(u"en"_s)); // đã là en: không phát lại
            QCOMPARE(changed.count(), 1);
            QCOMPARE(changed.at(0).at(0).toString(), u"en"_s);
        } // "thoát": ~Store ghi nốt xuống tệp
        Store s(path); // "khởi động lại": kho mới, cùng tệp
        Store::setInstance(&s);
        Translator t;
        QCOMPARE(t.language(), u"en"_s);
        QCOMPARE(tr("Đã lưu %1 vào %2").arg(u"A.dwg"_s, u"D:"_s), u"Saved A.dwg to D:"_s);
    }

    void pluralsAndPlaceholders()
    {
        QTemporaryDir dir;
        Store s(dir.filePath(u"user.json"_s));
        Store::setInstance(&s);
        Translator t;
        QCOMPARE(tr("%n đối tượng được chọn", nullptr, 3), u"3 đối tượng được chọn"_s);
        QVERIFY(t.setLanguage(u"en"_s));
        QCOMPARE(tr("%n đối tượng được chọn", nullptr, 1), u"1 object selected"_s);
        QCOMPARE(tr("%n đối tượng được chọn", nullptr, 3), u"3 objects selected"_s);
        QCOMPARE(tr("Đã lưu %1 vào %2").arg(u"A.dwg"_s, u"D:"_s), u"Saved A.dwg to D:"_s);
        QVERIFY(t.setLanguage(u"vi"_s));
        QCOMPARE(tr("%n đối tượng được chọn", nullptr, 1), u"1 đối tượng được chọn"_s);
    }

    // UI0-005 tiêu chí 5: chuỗi có sẵn của Qt (menu chuột phải ô nhập, nút chuẩn) hiện tiếng Việt ở vi, đổi
    // theo ngôn ngữ khi đang chạy.
    void qtStringsInVietnamese()
    {
        QTemporaryDir dir;
        Store s(dir.filePath(u"user.json"_s));
        Store::setInstance(&s);
        Translator t;
        QLineEdit edit;
        QDialogButtonBox box(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply
                             | QDialogButtonBox::RestoreDefaults);
        QMessageBox ask;
        ask.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Discard);
        const auto menu = [&] {
            const std::unique_ptr<QMenu> m(edit.createStandardContextMenu());
            QStringList texts;
            for (const QAction* a : m->actions())
                if (!a->isSeparator())
                    texts << a->text().section(u'\t', 0, 0); // bỏ phím tắt sau tab
            return texts;
        };
        const auto button = [&](QDialogButtonBox::StandardButton b) { return box.button(b)->text(); };

        QCOMPARE(menu(), (QStringList{u"&Hoàn tác"_s, u"&Làm lại"_s, u"Cắ&t"_s, u"Sao &chép"_s, u"&Dán"_s, u"Xóa"_s,
                                      u"Chọn tất cả"_s}));
        QCOMPARE(button(QDialogButtonBox::Ok), u"OK"_s);
        QCOMPARE(button(QDialogButtonBox::Cancel), u"Hủy"_s);
        QCOMPARE(button(QDialogButtonBox::Apply), u"Áp dụng"_s);
        QCOMPARE(button(QDialogButtonBox::RestoreDefaults), u"Khôi phục mặc định"_s);
        QCOMPARE(ask.button(QMessageBox::Yes)->text(), u"&Có"_s);
        QCOMPARE(ask.button(QMessageBox::No)->text(), u"&Không"_s);
        QCOMPARE(ask.button(QMessageBox::Discard)->text(), u"Không lưu"_s);

        QVERIFY(t.setLanguage(u"en"_s));
        QCoreApplication::processEvents(); // LanguageChange → QDialogButtonBox, QMessageBox dịch lại nút
        QCOMPARE(menu().constFirst(), u"&Undo"_s);
        QCOMPARE(button(QDialogButtonBox::Cancel), u"Cancel"_s);
        QCOMPARE(ask.button(QMessageBox::Yes)->text(), u"&Yes"_s);
        QVERIFY(t.setLanguage(u"vi"_s));
        QCoreApplication::processEvents();
        QCOMPARE(button(QDialogButtonBox::Cancel), u"Hủy"_s);
        QCOMPARE(menu().constFirst(), u"&Hoàn tác"_s);
        trion::sched::waitIdle(); // Store ghi nền xong trước khi xóa thư mục tạm
    }

    // UI0-005: vi-XL kéo dài bản vi ⌈40%⌉ ký tự (ít nhất 2) bằng chữ có dấu chồng tầng; không lưu vào Store;
    // tên phím (QShortcut) giữ nguyên.
    void pseudoLanguage()
    {
        QTemporaryDir dir;
        Store s(dir.filePath(u"user.json"_s));
        Store::setInstance(&s);
        const QString xl = trion::i18n::kPseudoLanguage.toString();
        QLineEdit edit;
        QDialogButtonBox box(QDialogButtonBox::Cancel);
        QErrorMessage err; // chuỗi Qt không có trong qtbase_vi: kéo dài chính chuỗi tiếng Anh
        {
            Translator t;
            const QString keys = QKeySequence(Qt::CTRL | Qt::Key_S).toString(QKeySequence::NativeText);
            QSignalSpy changed(&t, &Translator::languageChanged);
            QVERIFY(!Translator::languages().contains(xl));
            QVERIFY(t.setLanguage(xl));
            QCOMPARE(changed.count(), 1);
            QCOMPARE(t.language(), xl);
            QCOMPARE(s.get(trion::i18n::kLanguage), u"vi"_s); // không lưu
            QCoreApplication::processEvents();

            // ca vàng: placeholder và số nhiều còn nguyên; cụm "Ẫễỗậ" ngăn bằng dấu cách, không kết thúc bằng dấu cách
            QCOMPARE(tr("Đã lưu %1 vào %2").arg(u"A.dwg"_s, u"D:"_s), u"Đã lưu A.dwg vào D: Ẫễỗậ Ẫ"_s);
            QCOMPARE(tr("%n đối tượng được chọn", nullptr, 3), u"3 đối tượng được chọn Ẫễỗậ Ẫễỗ"_s);
            QCOMPARE(tr("Bản vẽ (*.dwg)"), u"Bản vẽ Ẫễỗậ Ẫ (*.dwg)"_s); // bộ lọc tệp còn dùng được
            QCOMPARE(box.button(QDialogButtonBox::Cancel)->text(), u"Hủy Ẫ"_s); // qtbase_vi rồi kéo dài
            const std::unique_ptr<QMenu> menu(edit.createStandardContextMenu());
            QCOMPARE(menu->actions().constFirst()->text().section(u'\t', 0, 0), u"&Hoàn tác Ẫễỗ"_s);
            const QString again = err.findChild<QCheckBox*>()->text();
            const QString english = u"&Show this message again"_s;
            QVERIFY2(again.startsWith(english + u" Ẫễỗậ"_s) && again.size() >= english.size() * 14 / 10,
                     qPrintable(again));
            QCOMPARE(QKeySequence(Qt::CTRL | Qt::Key_S).toString(QKeySequence::NativeText), keys);

            QVERIFY(t.setLanguage(u"vi"_s));
            QCOMPARE(tr("%n đối tượng được chọn", nullptr, 3), u"3 đối tượng được chọn"_s);
        }
        s.set(trion::i18n::kLanguage, xl); // QA mở thẳng ở vi-XL: sửa tay tệp cài đặt
        Translator t;
        QCOMPARE(t.language(), xl);
    }

    // Tiêu chí 1: 2.000 widget mang chữ đang hiện đổi vi ↔ en ≤ 300 ms, giữ trạng thái.
    void switchLiveKeepsState()
    {
        QTemporaryDir dir;
        Store s(dir.filePath(u"user.json"_s));
        Store::setInstance(&s);
        Translator t;

        QScrollArea window;
        auto* page = new QWidget;
        auto* rows = new QVBoxLayout(page);
        QList<Panel*> panels;
        for (int i = 1; i <= 500; ++i) {
            panels << new Panel(i, page);
            rows->addWidget(panels.back());
        }
        window.setWidgetResizable(true);
        window.setWidget(page);
        window.resize(1366, 768);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(panels.back()->combo->isVisible());

        QScrollBar* bar = window.verticalScrollBar();
        bar->setValue(bar->maximum() / 2);
        const int scroll = bar->value();
        QVERIFY(scroll > 0);
        for (int i = 0; i < panels.size(); ++i) {
            const QString typed = u"bản vẽ %1"_s.arg(i + 1); // nội dung người dùng gõ
            panels[i]->edit->setText(typed);
            panels[i]->edit->setCursorPosition(i % 6);
            panels[i]->combo->setCurrentIndex(i % 2);
        }
        QLineEdit* focused = panels[250]->edit; // chỉ ô đang focus giữ vùng chọn chữ
        focused->setFocus();
        focused->setSelection(0, 6);
        QCoreApplication::processEvents();
        QCOMPARE(focused->selectedText(), u"bản vẽ"_s);

        // Đo tới khi mọi widget đã đổi chuỗi (tiêu chí), tốt nhất của 3 lượt mỗi chiều để bớt nhiễu khi máy bận;
        // thời gian tới khi bố cục + vẽ xong chỉ in ra (phụ thuộc tải máy, QA-011 đo riêng).
        QHash<QString, qint64> best;
        const auto check = [&](const QString& lang) {
            QElapsedTimer clock;
            clock.start();
            QVERIFY(t.setLanguage(lang));
            QCoreApplication::processEvents(); // LanguageChange → retranslate() trên cả cây
            const qint64 strings = clock.elapsed();
            QCoreApplication::sendPostedEvents(); // bố cục
            window.repaint();                     // vẽ đồng bộ
            const qint64 painted = clock.elapsed();
            qInfo("đổi sang %s: %lld ms đổi chuỗi, %lld ms tới khi vẽ xong, %lld widget", qPrintable(lang),
                  static_cast<long long>(strings), static_cast<long long>(painted),
                  static_cast<long long>(window.findChildren<QWidget*>().size()));
            best[lang] = std::min(best.value(lang, strings), strings);

            const bool en = lang == u"en"_s;
            for (int i = 0; i < panels.size(); ++i) {
                const Panel* p = panels[i];
                const int n = i + 1;
                QCOMPARE(p->label->text(), (en ? u"Row %1"_s : u"Hàng %1"_s).arg(n));
                QCOMPARE(p->edit->placeholderText(), en ? u"Enter a name"_s : u"Nhập tên"_s);
                QCOMPARE(p->button->text(),
                         en ? (n == 1 ? u"1 object"_s : u"%1 objects"_s.arg(n)) : u"%1 đối tượng"_s.arg(n));
                QCOMPARE(p->combo->itemText(1), en ? u"Custom"_s : u"Tùy chỉnh"_s);
                // trạng thái người dùng không đổi
                QCOMPARE(p->edit->text(), u"bản vẽ %1"_s.arg(n));
                if (p->edit != focused)
                    QCOMPARE(p->edit->cursorPosition(), i % 6);
                QCOMPARE(p->combo->currentIndex(), i % 2);
            }
            QCOMPARE(focused->selectedText(), u"bản vẽ"_s);
            QCOMPARE(QApplication::focusWidget(), focused);
            QCOMPARE(bar->value(), scroll);
        };
        for (int round = 0; round < 3 && !QTest::currentTestFailed(); ++round) {
            check(u"en"_s);
            if (!QTest::currentTestFailed())
                check(u"vi"_s);
        }
        for (auto it = best.cbegin(); it != best.cend(); ++it)
            QVERIFY2(it.value() <= 300, qPrintable(u"đổi chuỗi sang %1: %2 ms"_s.arg(it.key()).arg(it.value())));
    }
};

QTEST_MAIN(TranslatorTest)
#include "translator_test.moc"
