#pragma once
// UI0-106 · Nút và điều khiển nhập theo prototype (styleguide.html mục Nút, Ô nhập; css/app.css .btn .ib .qsplit .seg
// .dd .search .tabs). Mỗi lớp là lớp con của widget Qt chuẩn nên giữ bàn phím, trợ năng (QAccessible) sẵn có; tự vẽ
// trong paintEvent, đọc token qua ThemeManager mỗi lần vẽ/đo (không cache, đổi chủ đề hay mật độ là đúng ngay).
// Không dùng QSS: QSS trên lớp nhiều thể hiện tốn ~1 s mỗi lần đổi chủ đề với 2.000 widget (app/theme/ThemeManager.h).
//
// Trạng thái chung (07.3 Phụ lục C):
//   rê, nhấn, vô hiệu      từ widget (underMouse, isDown, isEnabled), như prototype
//   focus                  viền 2 px --c-focus: nút khi focus đến từ bàn phím (:focus-visible), ô nhập mọi focus
//   bật                    isChecked (ToggleButton, đoạn của SegmentedGroup), tab đang mở
//   khóa theo phiên bản    setLocked(true): hình ổ khóa (không chỉ bằng màu), chữ --c-text-2, giá trị không đổi
//                          (toggle không bật, danh sách không mở, slider không kéo); nút vẫn phát clicked để
//                          UI0-011 mở hộp nâng cấp
//   lỗi                    setError(true): viền 2 px --c-danger như .inp.err; dòng thông báo .fmsg có biểu tượng
//                          do form hiện cạnh điều khiển (không chỉ bằng màu)
//
// Dùng (sau QApplication, loadFonts, ThemeManager, UiExecutor, IconProvider như main):
//   auto* print = new Button(tr("In"), Button::Kind::Primary);
//   auto* osnap = new ToggleButton({}, Button::Kind::Icon); osnap->setIconName(u"osnap"_s);
//   osnap->setAccessibleName(tr("Bắt điểm")); osnap->setToolTip(…);   // nút chỉ biểu tượng: không đặt chữ
//   auto* unit = new SegmentedGroup; unit->addSegment(u"mm"_s); unit->addSegment(u"m"_s);
//   auto* layer = new ComboBox; layer->addItems(names);               // popup có ô lọc không phân biệt dấu
#include <QComboBox>
#include <QLineEdit>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QSlider>
#include <QTabBar>

#include <cstdint>

class QAction;
class QButtonGroup;

namespace trion::ui {

// Nút: .btn (Normal), .btn.primary, .btn.danger, .btn.ghost, .ib (Icon: chỉ biểu tượng). Space và Enter kích hoạt
// (Enter cả ngoài QDialog). Chữ có '&' là phím tắt Alt như QPushButton.
class Button : public QPushButton {
    Q_OBJECT
public:
    enum class Kind : std::uint8_t {
        Normal,  // .btn
        Primary, // .btn.primary: mỗi hộp thoại tối đa một nút, mang tên hành động ("In", không "OK")
        Danger,  // .btn.danger: hành động phá hủy
        Ghost,   // .btn.ghost: nút phụ không viền ("Hủy")
        Icon,    // .ib: vuông --h-ctl, chỉ biểu tượng; tên qua setAccessibleName, setToolTip
        Segment, // .seg button: chỉ dùng trong SegmentedGroup
    };
    enum class Size : std::uint8_t { Small, Normal, Large }; // cao --h-btn, --h-ctl, --h-ctl-l

    explicit Button(const QString& text = {}, Kind kind = Kind::Normal, QWidget* parent = nullptr);

    Kind kind() const { return kind_; }
    void setKind(Kind kind);
    Size controlSize() const { return size_; }
    void setControlSize(Size size);
    // Biểu tượng của bộ theo tên (IconProvider), tô theo màu chữ của nút; khóa thì dùng bản có huy hiệu ổ khóa.
    QString iconName() const { return iconName_; }
    void setIconName(const QString& name);
    bool isLocked() const { return locked_; }
    void setLocked(bool locked);
    bool hasError() const { return error_; }
    void setError(bool error);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void nextCheckState() override; // khóa: không bật

    // SplitButton: có vùng mũi tên (rộng --ic) ở mép cuối; menu của nó đang mở.
    bool split_ = false;
    bool menuOpen_ = false;
    QRect menuZone() const;

private:
    Kind kind_;
    Size size_ = Size::Normal;
    QString iconName_;
    bool locked_ = false;
    bool error_ = false;
};

// Nút tách (.qsplit): phần chính kích hoạt như Button; vùng mũi tên ở mép cuối (hoặc Alt+↓) mở menu, Esc đóng.
// Khóa: vùng mũi tên cũng chỉ phát clicked.
class SplitButton : public Button {
    Q_OBJECT
public:
    explicit SplitButton(const QString& text = {}, QMenu* menu = nullptr, QWidget* parent = nullptr);
    // Tên khác QPushButton::setMenu (không ảo): menu của QPushButton chiếm cả nút.
    QMenu* dropMenu() const { return menu_; }
    void setDropMenu(QMenu* menu);
    void showDropMenu();

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    QPointer<QMenu> menu_;
};

// Nút bật/tắt (.ib[aria-pressed], nút trạng thái): Button có setCheckable(true); bật = nền --c-on-bg, chữ --c-on.
class ToggleButton : public Button {
    Q_OBJECT
public:
    explicit ToggleButton(const QString& text = {}, Kind kind = Kind::Normal, QWidget* parent = nullptr);
};

// Nhóm chọn một (.seg): luôn có một đoạn bật; Tab vào nhóm dừng ở đoạn đang bật, mũi tên chuyển và chọn (như nhóm
// radio của Qt, QButtonGroup độc quyền).
class SegmentedGroup : public QWidget {
    Q_OBJECT
public:
    explicit SegmentedGroup(QWidget* parent = nullptr);
    // Trả nút của đoạn để đặt tooltip, tên trợ năng, khóa. Đoạn đầu tiên được bật.
    Button* addSegment(const QString& text, const QString& iconName = {});
    Button* segment(int index) const;
    int count() const;
    int currentIndex() const; // -1 khi chưa có đoạn
    void setCurrentIndex(int index);
    bool hasError() const { return error_; }
    void setError(bool error);

signals:
    void currentIndexChanged(int index);

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    QButtonGroup* group_;
    bool error_ = false;
};

// Danh sách thả xuống (.dd, select.inp): QComboBox không sửa được, vẽ theo token. Bàn phím của QComboBox: Alt+↓,
// F4, Space mở; ↑↓ đổi giá trị; trong danh sách Enter chọn, Esc đóng không đổi giá trị.
class DropDown : public QComboBox {
    Q_OBJECT
public:
    explicit DropDown(QWidget* parent = nullptr);
    bool isLocked() const { return locked_; }
    void setLocked(bool locked);
    bool hasError() const { return error_; }
    void setError(bool error);
    void showPopup() override; // khóa: không mở
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override; // khóa: không đổi giá trị
    void wheelEvent(QWheelEvent* e) override;

private:
    bool locked_ = false;
    bool error_ = false;
};

// ComboBox có ô lọc: popup gồm SearchField và danh sách đã lọc của model(). Lọc không phân biệt dấu, hoa/thường
// (foldForSearch, NT-012: "lop" khớp "Lớp"); khóa gấp sẵn một lần cho mỗi nội dung model, mỗi phím chỉ gấp chuỗi gõ
// rồi quét. Trong popup: gõ để lọc, ↑↓ PgUp PgDn chọn, Enter hoặc nhấp chọn, Esc / Alt+↑ / F4 đóng không đổi giá trị.
// Danh sách ảo hóa (QListView, uniformItemSizes): 10.000 mục mỗi phím ≤ 16 ms (test ComboFilter).
class ComboBox : public DropDown {
    Q_OBJECT
public:
    explicit ComboBox(QWidget* parent = nullptr);
    void showPopup() override;
    void hidePopup() override;

protected:
    bool eventFilter(QObject* watched, QEvent* e) override; // phím của ô lọc
    void changeEvent(QEvent* e) override;                   // LanguageChange

private:
    class FilterModel;
    class Popup;
    void retranslate();
    void applyFilter(const QString& text);
    void activate(const QModelIndex& index);

    Popup* popup_ = nullptr;
    FilterModel* model_ = nullptr;
};

// Ô tìm (.search): kính lúp đầu ô, nút xóa khi có chữ; Esc xóa chữ (rỗng thì để hộp thoại xử lý Esc).
// Khóa: chỉ đọc, ổ khóa thay kính lúp.
class SearchField : public QLineEdit {
    Q_OBJECT
public:
    explicit SearchField(QWidget* parent = nullptr);
    bool isLocked() const { return locked_; }
    void setLocked(bool locked);
    bool hasError() const { return error_; }
    void setError(bool error);

protected:
    bool event(QEvent* e) override; // Esc là của ô khi còn chữ, không thành phím tắt của cửa sổ
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void changeEvent(QEvent* e) override; // LanguageChange: tên nút xóa

private:
    void retranslate();
    QAction* lead_;
    QAction* clear_;
    bool locked_ = false;
    bool error_ = false;
};

// Dải tab (.tabs): tab đang mở gạch chân 2 px --c-accent (TrionStyle), nét chia --c-border dưới dải; tràn thì có nút
// cuộn, không rút gọn chữ. ←→ chuyển tab (QTabBar). Focus bàn phím: viền 2 px --c-focus quanh tab đang mở.
class TabBar : public QTabBar {
    Q_OBJECT
public:
    explicit TabBar(QWidget* parent = nullptr);
    // ponytail: khóa, lỗi của tab là biểu tượng ổ khóa / lỗi thay biểu tượng của tab; tab khóa vẫn chọn được
    // (nội dung tab hiện lời mời nâng cấp, UI0-011).
    void setTabLocked(int index, bool locked);
    void setTabError(int index, bool error);

protected:
    void paintEvent(QPaintEvent* e) override; // nét chia dưới dải; tab, viền focus do TrionStyle
};

// Thanh trượt: rãnh --sp-2, phần đã chọn --c-accent, núm tròn cỡ --ic; bàn phím, chuột của QSlider.
class Slider : public QSlider {
    Q_OBJECT
public:
    explicit Slider(Qt::Orientation orientation = Qt::Horizontal, QWidget* parent = nullptr);
    bool isLocked() const { return locked_; }
    void setLocked(bool locked);
    bool hasError() const { return error_; }
    void setError(bool error);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override; // khóa: không đổi giá trị
    void keyPressEvent(QKeyEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;

private:
    bool locked_ = false;
    bool error_ = false;
};

} // namespace trion::ui
