#pragma once
// UID-019 · Khung hộp thoại Tùy chọn (07.2 UI-D10; prototype SC.options, OPT_TABS, TC.SETTINGS_INDEX trong
// design/prototype/js/screens2.js; css/app.css .vtabs .frow .pal-tools).
//
// Bố cục (thẻ 900 × 600):
//   đầu  "Hồ sơ hiện hành: <<Mặc định>> · Bản vẽ: <tên>" (setProfile, setDrawing) … ô "Tìm thiết lập"
//   thân danh sách 12 tab dọc (biểu tượng; ổ khóa cho tab bị cổng) | nội dung tab: trang đã đăng ký (addPage), trạng
//        thái khóa "Tab … có trong TrionCAD Pro" + [Dùng thử Pro 30 ngày] (EditionGate::upgrade), hoặc trạng thái rỗng
//        khi tab chưa có trang
//   chân [Đặt lại mặc định] … [Hủy] [Áp dụng] [Lưu và đóng]
// Phím: Ctrl+Tab / Ctrl+Shift+Tab đổi tab (vòng); ↑↓ trong danh sách tab; Enter = Lưu và đóng; Esc = Hủy (ô tìm còn
// chữ thì Esc xóa chữ trước, như SearchField). Ô tìm: mỗi phím nhảy tới tab của mục khớp nhất (SettingsIndex::search),
// tô sáng hàng, cuộn tới giữa; tab khóa vẫn nhảy tới (hiện trạng thái khóa, prototype thì đứng yên); không khớp: dòng
// "Không thấy thiết lập". Nội dung tab đang mở dựng ngay; các tab khác dựng sẵn (ẩn) sau khung đầu, mỗi lượt vòng sự
// kiện một tab, xong thì dừng: nhảy tab khi tìm chỉ còn vẽ.
//
// Giao dịch (OptionsDraft): sửa trong hộp là bản nháp. Khóa đọc với Preview::Live (chủ đề…) được xem trước: ghi Store
// ngay để ứng dụng đổi theo. Hủy, Esc, nút Đóng trả mọi khóa đã xem trước về giá trị trước khi mở (cả khóa chưa đặt) và
// bỏ nháp. Áp dụng / Lưu và đóng: mọi khóa hồ sơ ghi Store trong một lượt (Store gom thành một lần ghi đĩa QSaveFile:
// tệp trên đĩa hoặc cũ hẳn hoặc mới hẳn), không tạo bước undo; sysvar lưu trong bản vẽ thành một chuỗi lệnh
// "^C^C_UNDO _BEGIN _SETVAR TÊN giá-trị … _UNDO _END" (một bước undo) qua macroRequested.
// ponytail: chỗ nối tạm. Hồ sơ hiện hành chỉ là tên (NT-010 chưa có phạm vi profile); sysvar bản vẽ đọc từ
// actions::Sysvars (số nguyên, tạm thay INT-005/INT-107), ghi bằng macro qua CommandRouter (tạm thay INT-002); xem
// trước đi qua Store (ThemeManager chỉ nghe Store) nên bị ngắt tiến trình khi đang xem trước thì lần chạy sau giữ giá
// trị đang xem trước; sysvar bản vẽ chưa xem trước được (cần giá trị tạm không undo của INT-005).
//
// Dùng (main, sau EditionGate, CommandRouter, DialogHost):
//   dialogs.add(u"options"_s, [&](QWidget* parent) {
//       auto* d = new options::OptionsDialog(gate, store, parent);
//       d->setDrawing(docName, &sysvars);                                // bản vẽ hiện hành (rỗng: chưa mở bản vẽ)
//       connect(d, &options::OptionsDialog::macroRequested, &router, &shell::CommandRouter::submit);
//       return d;
//   });
//   router.add(u"OPTIONS"_s, [&] { dialogs.open(u"options"_s); });     // OP, menu ứng dụng, chuột phải vùng vẽ
//   auto* d = static_cast<options::OptionsDialog*>(dialogs.open(u"options"_s));
//   d->open(options::Tab::CommandLine, u"cmdline-history"_s);          // mở ở tab, tô sáng hàng
// Trang của tab (UID-020…026), đăng ký trong hàm dựng của OptionsDialog:
//   addPage(Tab::OpenSave, [](OptionsPage& page) {
//       page.addGroup(OptionsPage::tr("An toàn tệp"));
//       auto* box = new QCheckBox(OptionsPage::tr("Tự lưu mỗi"));
//       box->setChecked(page.draft().get(kAutosave));
//       QObject::connect(box, &QCheckBox::toggled, &page, [&page](bool on) { page.draft().set(kAutosave, on); });
//       page.addRow("autosave", OptionsPage::tr("Tự lưu"), {box});         // id có trong SettingsIndex
//   });
#include "app/dialogs/options/SettingsIndex.h"
#include "app/widgets/dialog/DialogFrame.h"
#include "core/base/settings/Settings.h"

#include <QJsonValue>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <array>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <vector>

class QLabel;
class QScrollArea;
class QStackedWidget;
class QTimer;
class QVBoxLayout;

namespace trion::actions {
class Sysvars;
}
namespace trion::edition {
class EditionGate;
}
namespace trion::ui {
class SearchField;
}

namespace trion::options {

class TabList;

// Bản nháp của hộp Tùy chọn: khóa hồ sơ (NT-010) và sysvar lưu trong bản vẽ (INT-005) đã sửa, chưa áp dụng.
class OptionsDraft : public QObject {
    Q_OBJECT
public:
    enum class Preview : std::uint8_t {
        None, // chỉ nháp; Áp dụng mới ghi
        Live, // xem trước: ghi Store ngay để ứng dụng đổi theo (chủ đề, mật độ); Hủy trả giá trị cũ
    };

    explicit OptionsDraft(settings::Store& store, QObject* parent = nullptr);
    ~OptionsDraft() override; // còn xem trước chưa áp dụng thì trả về (revert)

    settings::Store& store() const { return store_; }

    // Đọc khóa hồ sơ: nháp nếu đã sửa, không thì Store (chưa đặt: mặc định của Setting). Lần đọc đầu khai báo khóa (Đặt
    // lại mặc định đặt lại các khóa đã khai báo) cùng cách xem trước của nó.
    template <class T>
    T get(const settings::Setting<T>& s, Preview preview = Preview::None)
    {
        const QString key = QString::fromLatin1(s.key);
        declare(key, settings::Codec<T>::encode(s.def), preview);
        return settings::Codec<T>::decode(value(key), s.def);
    }
    template <class T>
    void set(const settings::Setting<T>& s, const T& v)
    {
        setValue(QString::fromLatin1(s.key), settings::Codec<T>::encode(v));
    }
    void declare(const QString& key, const QJsonValue& def, Preview preview = Preview::None);
    QJsonValue value(const QString& key) const;             // Undefined: chưa đặt (mặc định)
    void setValue(const QString& key, const QJsonValue& v); // Undefined: về mặc định (xóa khóa khi áp dụng)

    // Bản vẽ hiện hành. name rỗng: không có bản vẽ (hàng lưu trong bản vẽ vô hiệu). sysvars: giá trị hiện tại.
    QString drawing() const { return drawing_; }
    bool hasDrawing() const { return !drawing_.isEmpty(); }
    void setDrawing(const QString& name, actions::Sysvars* sysvars);
    QString sysvar(const QString& name) const; // nháp, không thì giá trị của bản vẽ; rỗng nếu không có bản vẽ
    // Giá trị như gõ ở SETVAR ("1000", "0.5"), không có khoảng trắng (khoảng trắng là Enter trong macro).
    void setSysvar(const QString& name, const QString& value);

    bool isDirty() const { return !pending_.isEmpty() || !sysvars_.isEmpty(); }
    // ponytail: khóa biết được qua lần đọc của trang (Đặt lại mặc định dựng tạm mọi trang đã đăng ký); NT-010 có phạm
    // vi profile thì đặt lại theo danh sách khóa của hồ sơ.
    int resetCount() const; // số khóa đã khai báo đang khác mặc định (tính cả nháp)
    void resetDefaults();   // nháp: mọi khóa đã khai báo về mặc định (khóa Live xem trước ngay)
    // Áp dụng: nháp khóa hồ sơ ghi Store trong một lượt, sysvar bản vẽ thành một chuỗi lệnh có một bước undo.
    void apply();
    void revert(); // trả khóa đã xem trước về giá trị trước đó, bỏ nháp

    // Chuỗi lệnh của các sysvar (sắp theo tên): "^C^C_UNDO _BEGIN _SETVAR A 1 _SETVAR B 2 _UNDO _END".
    static QString setvarMacro(const QMap<QString, QString>& values);

signals:
    void changed(); // nháp đổi (nút Áp dụng)
    void macroRequested(const QString& macro);

private:
    struct Key {
        QJsonValue def;
        bool live = false;
    };
    void write(const QString& key, const QJsonValue& v); // Store: đặt, hoặc xóa nếu Undefined

    settings::Store& store_;
    QMap<QString, Key> keys_;           // khóa đã khai báo
    QMap<QString, QJsonValue> pending_; // nháp khác giá trị nền (Store, hoặc giá trị trước xem trước)
    QMap<QString, QJsonValue> original_; // giá trị Store trước khi xem trước (Undefined: chưa đặt)
    QMap<QString, QString> sysvars_;     // nháp sysvar bản vẽ
    QString drawing_;
    QPointer<actions::Sysvars> vars_;
};

// Trang nội dung của một tab (UID-020…026 dựng): nhóm (.gh) và hàng thiết lập (.frow: nhãn rộng 240 px xuống dòng khi
// dài, rồi các điều khiển cách --sp-4). Hộp dựng trang khi tab mở lần đầu (hoặc dựng sẵn sau khung đầu), dựng lại từ
// nháp khi đổi ngôn ngữ, đặt lại mặc định, đổi phiên bản: trang không cần retranslate, chỉ đọc/ghi giá trị qua draft().
class OptionsPage : public QWidget {
    Q_OBJECT
public:
    enum class Where : std::uint8_t {
        Profile, // khóa hồ sơ (NT-010) hoặc sysvar phạm vi hồ sơ
        Drawing, // lưu trong bản vẽ: biểu tượng bản vẽ cạnh nhãn [đề xuất]; vô hiệu khi không có bản vẽ
    };

    OptionsPage(Tab tab, OptionsDraft& draft, QWidget* parent = nullptr);

    Tab tab() const { return tab_; }
    OptionsDraft& draft() const { return draft_; }

    void addGroup(const QString& title);
    // Hàng thiết lập. id: mục của SettingsIndex (tìm thấy thì hộp nhảy tới hàng này và tô sáng; CI kiểm mọi hàng có
    // mục). controls: điều khiển trái sang phải, trang nhận sở hữu; nhãn là buddy của điều khiển đầu (tên cho trình đọc
    // màn hình khi điều khiển không có chữ).
    QWidget* addRow(const char* id, const QString& label, std::initializer_list<QWidget*> controls,
                    Where where = Where::Profile);
    QWidget* row(QStringView id) const; // nullptr nếu không có
    QStringList rowIds() const;
    QString rowLabel(QStringView id) const;
    QString rowOf(const QWidget* w) const; // id của hàng chứa w, rỗng nếu không
    void setHighlighted(QStringView id);   // tô sáng hàng (--c-accent-soft); rỗng: bỏ
    QString highlighted() const { return lit_; }

private:
    void restyle();

    struct Entry {
        QString id;
        QString text; // nhãn như truyền vào (QLabel giữ bản đã thoát '&')
        QWidget* row;
        QLabel* label;
    };
    Tab tab_;
    OptionsDraft& draft_;
    QVBoxLayout* column_;
    std::vector<Entry> rows_;
    std::vector<QLabel*> groups_;
    QString lit_;
};

class OptionsDialog : public ui::DialogFrame {
    Q_OBJECT
public:
    // Dựng nội dung trang: thêm nhóm, hàng vào page. Gọi khi tab mở lần đầu và mỗi lần dựng lại.
    using PageFactory = std::function<void(OptionsPage& page)>;

    // gate: khóa tab, nút dùng thử. store: kho hồ sơ (NT-010). Modal (áp dụng khi bấm nút), mở ở tab Hiển thị.
    explicit OptionsDialog(edition::EditionGate& gate, settings::Store& store = settings::Store::instance(),
                           QWidget* parent = nullptr);
    ~OptionsDialog() override; // trả xem trước nếu đóng mà chưa Hủy/Áp dụng

    // Trang của tab (một trang mỗi tab; gọi lại thì thay). Tab đang mở thì dựng lại ngay.
    void addPage(Tab tab, PageFactory build);
    bool hasPage(Tab tab) const { return bool(factories_[int(tab)]); }

    // Mở ở tab, tô sáng hàng (id của SettingsIndex; rỗng: không). Đang mở thì chỉ chuyển tab.
    using DialogFrame::open;
    void open(Tab tab, const QString& row = {});

    Tab currentTab() const { return current_; }
    void setCurrentTab(Tab tab);
    bool isTabLocked(Tab tab) const;

    void setProfile(const QString& name); // hồ sơ hiện hành; rỗng: "<<Mặc định>>"
    void setDrawing(const QString& name, actions::Sysvars* sysvars);

    OptionsDraft& draft() const { return *draft_; }
    ui::SearchField* search() const { return search_; }
    OptionsPage* page(Tab tab) const; // trang đã dựng của tab (không khóa), nullptr nếu không
    QWidget* tabContent(Tab tab) const; // nội dung đã dựng của tab: trang, trạng thái khóa hoặc rỗng; nullptr nếu chưa
    QString highlightedRow() const; // hàng đang tô sáng ở tab hiện tại

    // CI (tiêu chí 1): dựng (không hiện) trang của mọi tab đã đăng ký; mỗi dòng một lỗi: hàng không có mục trong
    // SettingsIndex, mục thuộc tab khác, id hàng trùng, mục của tab không có hàng. Rỗng là đạt.
    QStringList indexIssues();

    void accept() override; // Lưu và đóng, Enter: áp dụng rồi đóng
    void reject() override; // Hủy, Esc, nút Đóng: trả xem trước, bỏ nháp

signals:
    // Chuỗi lệnh sysvar bản vẽ khi áp dụng → CommandRouter::submit (INT-002).
    void macroRequested(const QString& macro);

protected:
    void keyPressEvent(QKeyEvent* e) override; // Ctrl+Tab, Ctrl+Shift+Tab
    void changeEvent(QEvent* e) override;      // LanguageChange: dịch lại, dựng lại trang
    void paintEvent(QPaintEvent* e) override;  // khung đầu xong: bắt đầu dựng sẵn các tab khác
    QSize sizeHint() const override;           // thẻ 900 × 600 như prototype (lớn hơn nếu nội dung cần)

private:
    void retranslate();
    void restyle();

    struct Content {
        QPointer<QScrollArea> scroll; // vùng cuộn trong stack_
        QPointer<QWidget> inner;      // trang, trạng thái khóa hoặc rỗng
    };

    void showTab(Tab tab);
    Content& ensureContent(Tab tab); // dựng nội dung (ẩn trong stack_) nếu chưa có
    QWidget* buildContent(Tab tab);
    void prebuildNext(); // dựng sẵn một tab chưa dựng mỗi lượt vòng sự kiện, sau khung đầu
    void rebuild();      // bỏ mọi nội dung đã dựng, dựng lại tab hiện tại (focus về cùng hàng)
    void find(const QString& text);
    void showRow(Tab tab, const QString& id);
    void clearHighlight();
    void resetDefaults();
    void declareAll(); // dựng tạm mọi trang đã đăng ký để khai báo khóa của chúng
    void updateContext();
    void syncTabs();    // chữ, cờ khóa, mô tả trợ năng của danh sách tab
    void updateLocks(); // phiên bản đổi: syncTabs rồi dựng lại

    edition::EditionGate& gate_;
    OptionsDraft* draft_;
    std::array<PageFactory, kTabCount> factories_;
    std::array<Content, kTabCount> content_{}; // nội dung đã dựng theo tab
    Tab current_ = Tab::Display;
    QString profile_;
    QWidget* head_;
    QLabel* context_;
    QLabel* noMatch_;
    ui::SearchField* search_;
    TabList* tabs_;
    QStackedWidget* stack_;
    ui::Button* reset_;
    ui::Button* apply_;
    QTimer* prebuild_; // một lần, 0 ms; tự hẹn lại tới khi mọi tab đã dựng (không chạy định kỳ khi rảnh)
    bool painted_ = false; // đã có khung đầu
};

} // namespace trion::options
