#pragma once
// UI0-010 · Dải tự ẩn (prototype .strip/.vtab, .dock.flyout .palhost; js/shell.js dock()). Palette bỏ ghim nằm ở dải
// 22 px (--strip) ở cạnh cửa sổ, mỗi palette một tab chữ dọc (dải dưới: chữ ngang), nét chia 1 px phía vùng vẽ.
//   - Rê tab 300 ms hoặc bấm: palette trượt ra PHỦ lên vùng vẽ (không đẩy vùng vẽ) trong --t (120 ms); Giao diện nhẹ
//     hoặc giảm chuyển động (--t = 0) thì hiện ngay. Thu lại thì tức thì.
//   - Rời chuột khỏi dải và palette (khi focus không ở trong palette, không có popup), focus rời palette, Esc, hoặc bấm
//     lại tab: thu lại. Esc khi focus ở ngoài palette (dòng lệnh) chỉ thu lại, ô đó không nhận Esc này; focus ở trong
//     palette thì widget dùng Esc trước (ô tìm còn chữ thì xóa chữ), không ai dùng thì thu lại và về vùng vẽ.
//   - Mở bằng chuột không lấy focus (dòng lệnh giữ focus) cho tới khi bấm vào palette. Tab tới tab của dải (chỉ nhận
//     focus bằng phím) rồi Space: mở và đưa focus vào palette (tín hiệu focusRequested).
// Là QToolBar của QMainWindow (vùng toolbar nằm ngoài vùng dock nên dải ở sát mép cửa sổ, giữ chỗ trong bố cục),
// không di chuyển, không nổi; ẩn khi không còn palette. Khi palette vào dải, đầu palette (titleBarWidget) và thân
// (widget) của QDockWidget chuyển sang khung trượt; dock ẩn suốt thời gian đó nên QMainWindow vẫn giữ chỗ, nhóm tab,
// cỡ của nó (ghim lại về đúng chỗ cũ) và saveState ghi nó như palette ẩn.
// PaletteHost (UI0-113) dùng: bỏ ghim → addPalette; ghim, đóng, chuyển chỗ → removePalette; showPalette → open.
//
// ponytail: thu lại không có animation (prototype ẩn ngay); khung trượt chỉ có viền 1 px --c-border-ctl như Giao diện
// nhẹ, chưa vẽ bóng --sh-pop (widget con không vẽ ra ngoài rect: thêm lề vẽ bóng nếu ảnh QA-011 đòi); chưa kéo mép
// khung trượt để đổi cỡ (cỡ = sizeHint của palette, ít nhất --pal-w). Kéo palette từ khung trượt ra nổi: chưa có.
#include <QPointer>
#include <QTimer>
#include <QToolBar>

class QAbstractButton;
class QDockWidget;
class QMainWindow;
class QPropertyAnimation;

namespace trion::ui {

class AutoHideStrip : public QToolBar {
    Q_OBJECT
public:
    // Thời gian rê tab trước khi mở (07.1 chưa có token; [đề xuất] của UI0-010).
    static constexpr int kHoverMs = 300;

    // side: Qt::LeftDockWidgetArea, RightDockWidgetArea hoặc BottomDockWidgetArea. Tự thêm vào window (ẩn tới khi có
    // palette); window sở hữu dải và khung trượt.
    AutoHideStrip(Qt::DockWidgetArea side, QMainWindow* window);
    ~AutoHideStrip() override; // hủy cả khung trượt (con của window)

    Qt::DockWidgetArea side() const { return side_; }
    QList<QDockWidget*> palettes() const; // theo thứ tự tab
    bool contains(const QDockWidget* dock) const { return find(dock) >= 0; }

    // Palette vào dải (tab cuối): ẩn dock, chuyển đầu và thân của dock sang khung trượt. Đã có thì bỏ qua.
    void addPalette(QDockWidget* dock);
    // Palette ra khỏi dải: thu lại nếu đang mở, trả đầu và thân về dock. Dock vẫn ẩn, người gọi hiện nếu cần.
    void removePalette(QDockWidget* dock);

    // Trượt palette (đã ở trong dải) ra; đang mở palette khác thì đổi ngay. focus: phát focusRequested sau khi hiện.
    void open(QDockWidget* dock, bool focus = false);
    void collapse();
    QDockWidget* current() const; // palette đang mở, nullptr nếu đã thu

    QWidget* flyout() const { return flyout_; } // khung trượt: con của window, nằm trên vùng vẽ
    QRect flyoutRect() const; // chỗ của khung khi mở hết (tọa độ window); rỗng nếu đã thu
    QAbstractButton* tab(const QDockWidget* dock) const;

    QSize sizeHint() const override;

signals:
    void aboutToOpen(QDockWidget* dock);    // khung đã hiện, trước khi đo cỡ: dựng nội dung ở đây
    void focusRequested(QDockWidget* dock); // mở bằng bàn phím: đưa focus vào palette

protected:
    bool event(QEvent* e) override; // LayoutRequest: tab đổi cỡ (đổi ngôn ngữ, mật độ) → xếp lại
    bool eventFilter(QObject* watched, QEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void moveEvent(QMoveEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void changeEvent(QEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override; // không mở menu toolbar/dock của QMainWindow

private:
    class Flyout;
    struct Entry {
        QDockWidget* dock;
        QAbstractButton* tab;
        QWidget* sheet; // khung của palette trong khung trượt: viền, đầu, thân
        QWidget* header;
        QWidget* body;
    };
    qsizetype find(const QDockWidget* dock) const;
    void relayout();
    void place(); // đang mở: khung về đúng chỗ (dải đổi cỡ, đổi chỗ; đổi DPI, màn hình)
    void maybeCollapse();
    void escape();
    bool hovered() const; // con trỏ trên dải hoặc trên chỗ khung trượt khi mở hết

    QMainWindow* window_;
    Qt::DockWidgetArea side_;
    QList<Entry> entries_;
    QPointer<QWidget> flyout_; // Flyout; window hủy con theo thứ tự nên có thể đã hủy trước dải
    QPropertyAnimation* slide_;
    QTimer hover_; // chỉ chạy khi đang rê một tab
    QPointer<QDockWidget> pending_;
};

} // namespace trion::ui
