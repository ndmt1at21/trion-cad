#pragma once
// UIA-005 · Khung ribbon (07.1 §7.1; §3 ba mức thu gọn; §3.2 co theo ưu tiên). Prototype: renderRibbon, fitRibbon,
// ctlRun trong js/shell.js, TC.panelMore trong js/ribbons.js; css/app.css .rb .rb-tabs .rb-tab .rb-body .rb-p .rb-c
// .rb-l .rb-coll .is-collapsed .is-compact .min-panels .min-tabs.
//
//   [Trang chủ][Chèn][Chú thích]…                                                [⌃]   ← dải tab (--h-tab, --c-chrome)
//   ┌──────────────┬────────────┬──────┬                                                ← thân (3 hàng nút, --c-base)
//   │ thân panel   │            │ [▣]  │                                                   panel thu: nút thả xuống
//   │    Vẽ ▾      │    Sửa ▾   │Block▾│                                                ← tên panel / panel mở rộng
//
// - Dải tab: các tab của RibbonModel (UIA-103 thêm tab ngữ cảnh sau các tab thường). Thân tab dựng khi mở lần đầu
//   (RibbonPages, RibbonControls::buildPanel) trong khung RibbonPanel.
// - Co theo ưu tiên khi hẹp (fitRibbon): panel shrink > 1 thu thành nút thả xuống trước (shrink lớn trước, cùng mức
//   thì panel bên phải trước); còn tràn thì nút nhỏ bỏ chữ (.is-compact) từ phải sang; còn tràn nữa thì thu nốt từ
//   phải sang. Bề rộng tính từ sizeHint của điều khiển, không chờ layout.
// - Nút thả xuống của panel đã thu (và tên panel ở mức "chỉ tên panel") mở menu các điều khiển của panel: nút là mục
//   lệnh, split là menu con (rê để mở, không tốn lần bấm), dropdown/gallery/ô số/thanh trượt là chính điều khiển đó
//   trong menu, mục panel mở rộng ở cuối. Mọi lệnh tới được trong ≤ 2 lần bấm.
// - Panel mở rộng: nút "tên ▾" ở hàng tên mở vùng nổi dưới panel (các mục more); ghim thì giữ mở cả khi bấm ra
//   ngoài, chạy lệnh, đổi tab rồi quay lại, khởi động lại (kPinnedPanels).
// - Thu gọn 3 mức (nút ⌃ cuối dải tab đổi vòng, menu chuột phải): đầy đủ → chỉ tên panel → chỉ tên tab. Mức chỉ tên
//   tab: bấm tab thì thân hiện nổi trên vùng vẽ; bấm lại tab đó, bấm ra ngoài, Esc hoặc chạy lệnh thì đóng.
// - Vùng nổi (thân nổi, panel mở rộng) là widget con của cửa sổ, không phải cửa sổ popup: cú bấm đóng nó vẫn tới vùng
//   vẽ. Bộ lọc sự kiện của ứng dụng chỉ cài khi vùng nổi không ghim đang mở và khi đang giữ Alt.
// - Chuột phải: Thêm vào thanh truy cập nhanh (điều khiển có lệnh → addToQatRequested, UIA-101), hiện/ẩn tab, hiện/ẩn
//   panel của tab đang mở, mức thu gọn. Mức thu gọn, tab ẩn, panel ẩn, panel ghim lưu qua Store (NT-010).
// - Bàn phím: Alt (nhấn rồi thả, không kèm phím khác) vào dải tab, Alt lần nữa trả focus; ←→ đổi tab (QTabBar); ↓ hoặc
//   Enter vào thân (mức chỉ tên tab: mở thân nổi); Tab/Shift+Tab qua các điều khiển theo thứ tự trên màn hình; Esc ở
//   thân về dải tab, Esc ở dải tab trả focus nơi trước đó. F10 là POLAR (keymap.json, 07.1 §11) nên không dùng.
// - Tooltip 2 mức cho mọi điều khiển qua RichTooltip (RibbonControls; nút của khung đặt ở đây): mức 1 tên, tên lệnh
//   tiếng Anh, phím; mức 2 sau 1,5 s mô tả, "Nhấn F1 để xem trợ giúp"; F1 khi rê → RichTooltip::helpRequested(lệnh).
// Màu, cỡ đọc token mỗi lần vẽ, đo. Rảnh không timer. Luồng UI.
//
// Dùng (bước integration, sau ThemeManager, IconProvider, RichTooltip, ActionRegistry, EditionGate):
//   auto* ribbon = new trion::ribbon::RibbonBar(model, controls);       // model, controls sống lâu hơn ribbon
//   mainWindow.setRegion(MainWindow::Region::Ribbon, ribbon);
//   connect(&controls, &RibbonControls::macroRequested, router, &CommandRouter::submit);
//   connect(ribbon, &RibbonBar::addToQatRequested, titleBar->qat(), …);  // UIA-101
//   router.add(u"RIBBON"_s, [&] { mainWindow.setRegionVisible(Region::Ribbon, true); });       // RIBBON.md
//   router.add(u"RIBBONCLOSE"_s, [&] { mainWindow.setRegionVisible(Region::Ribbon, false); }); // RIBBONCLOSE.md
//
// ponytail: tab đang mở có chữ thường (prototype --fw-m), gạch chân lùi --sp-4 như .tabs (prototype 10 px); vùng nổi
// chưa có bóng đổ; không có Keytips (P2) và hiệu ứng trượt khi mở thân nổi.

#include "app/shell/ribbon/RibbonModel.h"
#include "core/base/settings/Settings.h"

#include <QAbstractButton>
#include <QPointer>
#include <QWidget>

#include <array>
#include <cstdint>

class QLabel;
class QMenu;
class QTabBar;

namespace trion::ui {
class Button;
}

namespace trion::ribbon {

class Floating;
class PanelButton;
class RibbonControls;
class RibbonPages;

// Trạng thái lưu: mức thu gọn (RibbonBar::Collapse); id tab ẩn; panel ẩn và panel mở rộng đã ghim ("tab/panel").
inline const settings::Setting<int> kCollapse{"ribbon/collapse", 0};
inline const settings::Setting<QStringList> kHiddenTabs{"ribbon/hiddenTabs", {}};
inline const settings::Setting<QStringList> kHiddenPanels{"ribbon/hiddenPanels", {}};
inline const settings::Setting<QStringList> kPinnedPanels{"ribbon/pinned", {}};

// Khung một panel (.rb-p): thân (RibbonControls::buildPanel), hàng tên (.rb-l: tên, hoặc nút "tên ▾" mở panel mở rộng
// khi panel có more), nút thả xuống khi thu (.rb-coll); nét chia bên phải.
class RibbonPanel : public QWidget {
    Q_OBJECT
public:
    enum class State : std::uint8_t {
        Full,      // thân đầy đủ
        Compact,   // nút nhỏ bỏ chữ (.is-compact)
        Collapsed, // nút lớn: biểu tượng của panel, tên ▾ (.is-collapsed); mở menu()
        Title,     // mức "chỉ tên panel": nút "tên ▾" cao --h-lbl; mở menu()
    };

    // panel, controls, store sống lâu hơn khung. key: "tab/panel".
    RibbonPanel(const Panel& panel, const QString& key, RibbonControls& controls, settings::Store& store,
                QWidget* parent = nullptr);
    ~RibbonPanel() override;

    const Panel& panel() const { return panel_; }
    const QString& key() const { return key_; }
    State state() const { return state_; }
    void setState(State s);
    // Bề rộng khung ở trạng thái s (cả nét chia), tính từ sizeHint của điều khiển; nhớ tới invalidate().
    int widthFor(State s) const;
    void invalidate(); // chữ, cỡ, khóa của điều khiển đổi

    QWidget* body() const { return body_; }
    QAbstractButton* moreButton() const; // nullptr nếu panel không có more
    QAbstractButton* dropButton() const; // nút của trạng thái Collapsed, Title
    QMenu* menu();                       // menu của nút thả xuống; điền lại mỗi lần mở

    // Panel mở rộng (vùng nổi dưới panel). Đóng bằng tay (nút "tên ▾", Esc) thì bỏ ghim.
    bool isExpanded() const;
    void setExpanded(bool open, bool focus = false);
    QWidget* expansion() const; // nullptr nếu chưa mở lần nào
    bool isPinned() const { return pinned_; }
    void setPinned(bool pinned);
    void syncExpansion(); // sau khi khung đổi chỗ, ẩn/hiện: đặt lại chỗ; nút "tên ▾" ẩn thì ẩn theo, ghim thì hiện lại

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void changeEvent(QEvent* e) override;
    void paintEvent(QPaintEvent* e) override;

private:
    void retranslate();
    void applyStyle(); // hàng tên: màu, cao theo token
    void fillMenu();
    QWidget* buildExpansion();

    const Panel& panel_;
    const QString key_;
    RibbonControls& controls_;
    settings::Store& store_;
    QWidget* body_;
    QLabel* label_ = nullptr;      // hàng tên khi không có more
    PanelButton* more_ = nullptr;  // hàng tên khi có more
    PanelButton* drop_;            // Collapsed, Title
    QMenu* menu_ = nullptr;
    QPointer<Floating> expansion_; // con của cửa sổ: hủy cùng khung
    State state_ = State::Full;
    bool pinned_ = false;
    mutable std::array<int, 4> widths_{-1, -1, -1, -1};
};

class RibbonBar : public QWidget {
    Q_OBJECT
public:
    enum class Collapse : std::uint8_t { Full, PanelTitles, TabNames };

    // model, controls, store sống lâu hơn RibbonBar. Tab đầu tiên chưa ẩn dựng ngay.
    RibbonBar(const RibbonModel& model, RibbonControls& controls, settings::Store& store = settings::Store::instance(),
              QWidget* parent = nullptr);
    ~RibbonBar() override;

    QTabBar* tabBar() const;
    QAbstractButton* collapseButton() const;
    RibbonPages* pages() const { return pages_; }
    QList<RibbonPanel*> panels() const;           // của tab đang mở, kể cả panel ẩn
    RibbonPanel* panel(const QString& key) const; // "tab/panel" trong các tab đã dựng; nullptr nếu không có

    int currentTab() const; // chỉ số trong RibbonModel::tabs()
    void setCurrentTab(int index);
    Collapse collapse() const { return collapse_; }
    void setCollapse(Collapse c);
    bool isTabHidden(const QString& tab) const;
    void setTabHidden(const QString& tab, bool hidden); // không ẩn tab cuối cùng còn hiện
    bool isPanelHidden(const QString& key) const;
    void setPanelHidden(const QString& key, bool hidden);

    bool isFloating() const; // thân đang hiện nổi (chỉ ở mức TabNames)
    void setFloating(bool open);
    QWidget* floatingBody() const; // vùng nổi chứa thân; nullptr nếu chưa mở lần nào

    void enter(); // focus vào dải tab, nhớ focus cũ để trả (Alt)
    // Menu chuột phải tại widget at của ribbon (nullptr: chỗ trống); tự hủy khi đóng. contextMenuEvent mở nó.
    QMenu* contextMenu(QWidget* at);

    QSize sizeHint() const override;

signals:
    void addToQatRequested(const QString& cmd); // "Thêm vào thanh truy cập nhanh" (UIA-101)

protected:
    bool eventFilter(QObject* watched, QEvent* e) override;
    void changeEvent(QEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void showEvent(QShowEvent* e) override;

private:
    QWidget* buildPage(int index);
    void showTab(int index);
    void fit();
    void scheduleFit(bool remeasure);
    void applyCollapse();
    void syncExpansions();
    void closeTransient(); // lệnh vừa chạy: đóng thân nổi, panel mở rộng chưa ghim
    void enterBody();
    void leave();
    bool hasFocusInside() const;
    void retranslate();
    QStringList hiddenPanels() const;

    const RibbonModel& model_;
    RibbonControls& controls_;
    settings::Store& store_;
    QTabBar* tabs_;
    ui::Button* collapseButton_;
    RibbonPages* pages_;
    QPointer<Floating> float_; // con của cửa sổ: hủy cùng ribbon
    QPointer<QWidget> returnFocus_;
    QPointer<QWidget> filtered_; // cửa sổ đang cài bộ lọc phím Alt
    Collapse collapse_ = Collapse::Full;
    bool fitPending_ = false;
    bool remeasure_ = false;
    bool altDown_ = false;
};

} // namespace trion::ribbon
