#include "app/core/i18n/TruncationProbe.h"

#include "app/core/i18n/Translator.h"
#include "core/base/sched/Scheduler.h"
#include "core/base/settings/Settings.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QRadioButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QStyle>
#include <QTabBar>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;
using trion::i18n::Translator;
using trion::settings::Store;

// Bộ widget thử: mỗi loại một ca "cut…" (cỡ cố định vừa khít chữ tiếng Việt: ở vi-XL phải bị báo) và một ca còn
// chỗ (trong bố cục, hoặc không vẽ chữ: không được báo). Chuỗi đặt trong retranslate() theo quy ước Translator.h.
class ProbeScreen : public QWidget {
    Q_OBJECT
public:
    ProbeScreen()
    {
        auto* rows = new QVBoxLayout(this);
        const auto row = [rows](std::initializer_list<QWidget*> ws) {
            auto* line = new QHBoxLayout;
            for (QWidget* w : ws)
                line->addWidget(w);
            line->addStretch();
            rows->addLayout(line);
        };
        row({cutLabel, label, hiddenLabel});
        row({cutButton, button});
        row({cutCheck, check});
        row({cutTool, iconTool});
        row({cutTabs, cutTabsOverflow, tabs});
        row({table});
        row({buttons});
        hiddenLabel->setFixedWidth(4);
        hiddenLabel->hide();
        iconTool->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
        iconTool->setToolButtonStyle(Qt::ToolButtonIconOnly);
        iconTool->setFixedWidth(24);
        cutTabs->addTab(QString());
        cutTabs->setElideMode(Qt::ElideRight);
        cutTabs->setUsesScrollButtons(false);
        cutTabs->setExpanding(false);
        cutTabsOverflow->addTab(QString()); // không rút gọn, không nút cuộn: tab tràn khỏi thanh
        cutTabsOverflow->setElideMode(Qt::ElideNone);
        cutTabsOverflow->setUsesScrollButtons(false);
        cutTabsOverflow->setExpanding(false);
        tabs->addTab(QString());
        tabs->addTab(QString());
        table->setColumnCount(3);
        table->setRowCount(2);
        table->setMinimumWidth(900);
        for (int c = 0; c < 3; ++c)
            table->setHorizontalHeaderItem(c, new QTableWidgetItem);
        table->setItem(0, 1, new QTableWidgetItem);
        table->setItem(0, 2, new QTableWidgetItem);
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        for (int i = 0; i < 3; ++i) {
            cutMenu->addAction(QString());
            menu->addAction(QString());
        }
        for (QObject* o : std::initializer_list<QObject*>{cutLabel, cutButton, cutCheck, cutTool, cutTabs, cutMenu,
                                                          table, label, button, check, iconTool, tabs, menu})
            o->setObjectName(QString::fromLatin1(o->metaObject()->className()));
        cutLabel->setObjectName(u"cutLabel"_s);
        cutButton->setObjectName(u"cutButton"_s);
        cutCheck->setObjectName(u"cutCheck"_s);
        cutTool->setObjectName(u"cutTool"_s);
        cutTabs->setObjectName(u"cutTabs"_s);
        cutTabsOverflow->setObjectName(u"cutTabsOverflow"_s);
        cutMenu->setObjectName(u"cutMenu"_s);
        table->horizontalHeader()->setObjectName(u"header"_s);
        retranslate();
    }

    // Gọi khi đang ở vi, sau khi hiện: cố định cỡ các ca "cut…" vừa đúng chữ tiếng Việt.
    void fitToVietnamese()
    {
        for (QWidget* w :
             std::initializer_list<QWidget*>{cutLabel, cutButton, cutCheck, cutTool, cutTabs, cutTabsOverflow})
            w->setFixedWidth(w->sizeHint().width());
        table->setColumnWidth(0, table->horizontalHeader()->sectionSizeHint(0)); // đầu cột vừa khít
        const QAbstractItemView* view = table; // QTableView::sizeHintForColumn là protected, ở lớp gốc public
        table->setColumnWidth(1, view->sizeHintForColumn(1));                    // ô vừa khít
        cutMenu->setMaximumWidth(cutMenu->sizeHint().width());
    }

    QLabel* cutLabel = new QLabel(this);
    QLabel* label = new QLabel(this);
    QLabel* hiddenLabel = new QLabel(this);
    QPushButton* cutButton = new QPushButton(this);
    QPushButton* button = new QPushButton(this);
    QCheckBox* cutCheck = new QCheckBox(this);
    QCheckBox* check = new QCheckBox(this);
    QToolButton* cutTool = new QToolButton(this);
    QToolButton* iconTool = new QToolButton(this);
    QTabBar* cutTabs = new QTabBar(this);
    QTabBar* cutTabsOverflow = new QTabBar(this);
    QTabBar* tabs = new QTabBar(this);
    QTableWidget* table = new QTableWidget(this);
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QMenu* cutMenu = new QMenu(this);
    QMenu* menu = new QMenu(this);

protected:
    void changeEvent(QEvent* e) override
    {
        if (e->type() == QEvent::LanguageChange)
            retranslate();
        QWidget::changeEvent(e);
    }

private:
    void retranslate()
    {
        cutLabel->setText(tr("Độ dày nét theo layer"));
        label->setText(tr("Thuộc tính"));
        hiddenLabel->setText(tr("Kiểu đường"));
        cutButton->setText(tr("Xuất ra tệp PDF"));
        button->setText(tr("Áp dụng"));
        cutCheck->setText(tr("Hiện độ dày nét"));
        check->setText(tr("Khóa layer"));
        cutTool->setText(tr("Thiết lập trang"));
        iconTool->setText(tr("Bắt điểm"));
        cutTabs->setTabText(0, tr("Mặt bằng tầng 1"));
        cutTabsOverflow->setTabText(0, tr("Bản vẽ"));
        tabs->setTabText(0, tr("Model"));
        tabs->setTabText(1, tr("Bản vẽ"));
        table->horizontalHeaderItem(0)->setText(tr("Độ dày nét"));
        table->horizontalHeaderItem(1)->setText(tr("Mô tả"));
        table->horizontalHeaderItem(2)->setText(tr("Ghi chú"));
        table->item(0, 1)->setText(tr("Nét liền mảnh cho đường bao"));
        table->item(0, 2)->setText(tr("Nét đứt"));
        const QList<QAction*> cut = cutMenu->actions(), room = menu->actions();
        cut[0]->setText(tr("Mở"));
        cut[1]->setText(tr("Lưu bản vẽ thành tệp mới"));
        cut[2]->setText(tr("Đóng"));
        room[0]->setText(tr("Sao chép"));
        room[1]->setText(tr("Dán"));
        room[2]->setText(tr("Chọn tất cả"));
    }
};

class TruncationTest : public QObject {
    Q_OBJECT

    QTemporaryDir dir_;
    std::unique_ptr<Store> store_;
    std::unique_ptr<Translator> translator_;

    // LanguageChange → retranslate(), bố cục, đổi cỡ cột (hẹn giờ 0): mỗi bước đăng việc cho lượt kế. Máy tải nặng,
    // qWait hết giờ khi chuỗi đó chưa xong: thêm các lượt không tính giờ, mỗi lượt giao mọi sự kiện đã đăng và hẹn giờ
    // đã tới (đủ cho chuỗi sâu 10 bước), nên kết quả không phụ thuộc tải.
    static void settle()
    {
        QTest::qWait(20);
        for (int i = 0; i < 10; ++i) {
            QCoreApplication::sendPostedEvents();
            QCoreApplication::processEvents();
        }
    }

    // "tên đối tượng: chữ" của mọi chỗ bị cắt, đã sắp xếp
    static QStringList found(QWidget* root)
    {
        QStringList out;
        for (const auto& t : trion::i18n::findTruncations(root))
            out << t.widget->objectName() + u": "_s + t.text;
        out.sort();
        return out;
    }

private slots:
    void initTestCase()
    {
        store_ = std::make_unique<Store>(dir_.filePath(u"user.json"_s));
        Store::setInstance(store_.get());
        translator_ = std::make_unique<Translator>();
    }

    void cleanupTestCase()
    {
        translator_.reset();
        Store::setInstance(nullptr);
        store_.reset();
        trion::sched::waitIdle(); // Store ghi nền xong trước khi xóa thư mục tạm
    }

    // Tiêu chí 3: với vi-XL, probe tìm đủ 9 ca cắt cài sẵn (nhãn, nút, ô chọn, nút công cụ, tab bị nén, tab tràn,
    // đầu cột, ô bảng, mục menu) và không báo gì khác; ở vi không báo gì.
    void findsPlantedCutsOnly()
    {
        ProbeScreen s;
        s.show();
        QVERIFY(QTest::qWaitForWindowExposed(&s));
        s.fitToVietnamese();
        settle();
        QCOMPARE(found(&s), QStringList());

        QVERIFY(translator_->setLanguage(trion::i18n::kPseudoLanguage.toString()));
        settle();
        QVERIFY(s.label->text().startsWith(u"Thuộc tính Ẫ"_s)); // bộ thử thật sự đang ở vi-XL
        QStringList want{
            u"cutLabel: "_s + s.cutLabel->text(),
            u"cutButton: "_s + s.cutButton->text(),
            u"cutCheck: "_s + s.cutCheck->text(),
            u"cutTool: "_s + s.cutTool->text(),
            u"cutTabs: "_s + s.cutTabs->tabText(0),
            u"cutTabsOverflow: "_s + s.cutTabsOverflow->tabText(0),
            u"header: "_s + s.table->horizontalHeaderItem(0)->text(),
            u"QTableWidget: "_s + s.table->item(0, 1)->text(),
            u"cutMenu: "_s + s.cutMenu->actions().at(1)->text(),
        };
        want.sort();
        QCOMPARE(found(&s), want);

        QVERIFY(translator_->setLanguage(u"vi"_s));
        settle();
        QCOMPARE(found(&s), QStringList());
    }

    // 0 báo nhầm ngoài bộ thử: cửa sổ bố cục bình thường (menu, thanh công cụ, tab, form, nhóm, cây, danh sách, nút
    // chuẩn) không bị báo ở vi lẫn vi-XL, vì bố cục giãn theo chữ. Chuỗi dùng lại của ProbeScreen.
    void typicalWindowHasNoFalsePositives()
    {
        const auto tx = [](const char* s) { return QCoreApplication::translate("ProbeScreen", s); };
        QMainWindow w;
        QMenu* file = w.menuBar()->addMenu(QString());
        QToolBar* tools = w.addToolBar(QString());
        tools->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        QAction* open = tools->addAction(w.style()->standardIcon(QStyle::SP_DialogOpenButton), QString());
        file->addAction(open);
        QAction* save = file->addAction(QString());
        auto* pages = new QTabWidget;
        auto* form = new QWidget;
        auto* fl = new QFormLayout(form);
        auto* nameLabel = new QLabel;
        auto* combo = new QComboBox;
        combo->addItems({QString(), QString()});
        auto* group = new QGroupBox;
        auto* gl = new QVBoxLayout(group);
        auto* lock = new QCheckBox;
        auto* radio = new QRadioButton;
        gl->addWidget(lock);
        gl->addWidget(radio);
        fl->addRow(nameLabel, new QLineEdit);
        fl->addRow(new QLabel, combo);
        fl->addRow(new QLabel, new QSpinBox);
        fl->addRow(group);
        auto* tree = new QTreeWidget;
        tree->setColumnCount(2);
        tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
        auto* item = new QTreeWidgetItem(tree);
        auto* list = new QListWidget;
        list->addItem(QString());
        auto* lists = new QWidget;
        auto* ll = new QHBoxLayout(lists);
        ll->addWidget(tree);
        ll->addWidget(list);
        pages->addTab(form, QString());
        pages->addTab(lists, QString());
        auto* central = new QWidget;
        auto* cl = new QVBoxLayout(central);
        cl->addWidget(pages);
        cl->addWidget(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply));
        w.setCentralWidget(central);
        auto* status = new QLabel;
        w.statusBar()->addWidget(status);
        const auto texts = [&] { // như retranslate(): đặt lại chữ sau mỗi lần đổi ngôn ngữ
            file->setTitle(tx("Mở"));
            open->setText(tx("Mở"));
            save->setText(tx("Lưu bản vẽ thành tệp mới"));
            nameLabel->setText(tx("Mô tả"));
            static_cast<QLabel*>(fl->itemAt(1, QFormLayout::LabelRole)->widget())->setText(tx("Kiểu đường"));
            static_cast<QLabel*>(fl->itemAt(2, QFormLayout::LabelRole)->widget())->setText(tx("Độ dày nét"));
            combo->setItemText(0, tx("Nét liền mảnh cho đường bao"));
            combo->setItemText(1, tx("Nét đứt"));
            group->setTitle(tx("Thuộc tính"));
            lock->setText(tx("Khóa layer"));
            radio->setText(tx("Hiện độ dày nét"));
            tree->setHeaderLabels({tx("Độ dày nét theo layer"), tx("Ghi chú")});
            item->setText(0, tx("Nét liền mảnh cho đường bao"));
            item->setText(1, tx("Nét đứt"));
            list->item(0)->setText(tx("Mặt bằng tầng 1"));
            pages->setTabText(0, tx("Thuộc tính"));
            pages->setTabText(1, tx("Bản vẽ"));
            status->setText(tx("Xuất ra tệp PDF"));
        };
        texts();
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        for (const QString& lang : {u"vi"_s, trion::i18n::kPseudoLanguage.toString(), u"vi"_s}) {
            QVERIFY(translator_->setLanguage(lang));
            texts();
            settle();
            pages->setCurrentIndex(1); // trang ẩn không đo: đo cả hai trang
            settle();
            QCOMPARE(found(&w), QStringList());
            pages->setCurrentIndex(0);
            settle();
            QCOMPARE(found(&w), QStringList());
        }
    }

    // Cắt theo chiều dọc (nhãn thấp hơn dòng chữ, nhãn xuống dòng thiếu chỗ); widget ẩn hoặc không ai thấy (ngoài cha,
    // bị che) không đo, view không viền vẫn đo; toString cho QA.
    void verticalCutsAndHiddenWidgets()
    {
        QWidget root;
        auto* rows = new QVBoxLayout(&root);
        auto* low = new QLabel(tr("Chú thích"), &root);
        low->setObjectName(u"low"_s);
        low->setFixedHeight(low->fontMetrics().height() / 2);
        auto* wrapped = new QLabel(tr("Bản vẽ này có nhiều layer đang khóa"), &root);
        wrapped->setObjectName(u"wrapped"_s);
        wrapped->setWordWrap(true);
        const QFontMetrics fm = wrapped->fontMetrics();
        wrapped->setFixedSize(fm.horizontalAdvance(u"layer đang"_s), fm.height()); // một dòng hẹp
        auto* hidden = new QLabel(tr("Chú thích"), &root);
        hidden->setFixedHeight(1);
        hidden->hide();
        rows->addWidget(low);
        rows->addWidget(wrapped);
        rows->addWidget(hidden);
        // isVisible() mà không ai thấy: ngoài cha (trang tab nền của nhóm dock Qt dời ra ngoài cửa sổ), bị anh em đục
        // che hết. View không viền vẫn quét: viewport đục phủ hết khung.
        auto* outside = new QLabel(tr("Bản vẽ này có nhiều layer đang khóa"), &root);
        outside->setGeometry(-400, 0, 20, 20);
        auto* covered = new QLabel(tr("Bản vẽ này có nhiều layer đang khóa"), &root);
        covered->setGeometry(300, 250, 20, 20);
        auto* cover = new QWidget(&root); // tạo sau: nằm trên
        cover->setAutoFillBackground(true);
        cover->setGeometry(290, 240, 40, 40);
        auto* table = new QTableWidget(1, 1, &root);
        table->setObjectName(u"frameless"_s);
        table->setFrameShape(QFrame::NoFrame);
        table->horizontalHeader()->hide();
        table->verticalHeader()->hide();
        table->setItem(0, 0, new QTableWidgetItem(tr("Bản vẽ này có nhiều layer đang khóa")));
        table->setColumnWidth(0, 30);
        table->setFixedSize(40, 40);
        rows->addWidget(table);
        root.resize(400, 300);
        root.show();
        QVERIFY(QTest::qWaitForWindowExposed(&root));
        settle();
        QVERIFY(outside->isVisible() && covered->isVisible());
        QVERIFY(table->visibleRegion().isEmpty()); // khung bị viewport che: phải xét viewport

        const QList<trion::i18n::Truncation> cuts = trion::i18n::findTruncations(&root);
        QCOMPARE(found(&root), (QStringList{u"frameless: "_s + table->item(0, 0)->text(), u"low: "_s + low->text(),
                                            u"wrapped: "_s + wrapped->text()}));
        for (const auto& t : cuts) {
            if (t.widget == table)
                continue;
            QVERIFY(t.need.height() > t.have.height());
            QVERIFY2(t.toString().startsWith(u"QLabel '%1': «%2» cần "_s.arg(t.widget->objectName(), t.text)),
                     qPrintable(t.toString()));
        }
    }
};

QTEST_MAIN(TruncationTest)
#include "truncation_test.moc"
