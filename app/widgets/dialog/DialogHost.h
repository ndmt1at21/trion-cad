#pragma once
// UI0-110 · Mở hộp thoại theo mã: một bản mỗi mã, nhớ vị trí, cỡ (NT-010), danh sách mã cho CI.
//
//   DialogHost dialogs(&mainWindow);                                    // main, sau MainWindow; giữ suốt đời ứng dụng
//   dialogs.add(u"plot"_s, [](QWidget* parent) { return new PlotDialog(parent); });
//   dialogs.open(u"plot"_s);                                            // lệnh PLOT (UI0-015)
//   connect(&dialogs, &DialogHost::helpRequested, help, &Help::show);   // UIL-009
//
// Mọi hộp thoại của ứng dụng đăng ký ở đây: CI dựng lại từng cái và kiểm nhãn nút xác nhận (confirmLabelIssues) ở
// tiếng Việt và English.
// ponytail: hộp thoại dựng theo tham số (xác nhận, lỗi của UI0-008) không qua DialogHost; builder của chúng tự kiểm.
#include "core/base/settings/Settings.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QStringList>

#include <functional>
#include <map>

class QWidget;

namespace trion::ui {

class DialogFrame;

class DialogHost : public QObject {
    Q_OBJECT
public:
    // Dựng hộp thoại (chưa hiện) với cha cho trước. Rẻ: dữ liệu chậm thì loadBody.
    using Factory = std::function<DialogFrame*(QWidget* parent)>;

    // mainWindow: cha mặc định, mốc đặt hộp thoại khi màn hình cũ đã rút. store: nơi lưu "dialog/<mã>".
    explicit DialogHost(QWidget* mainWindow, settings::Store& store = settings::Store::instance(),
                        QObject* parent = nullptr);
    ~DialogHost() override; // lưu vị trí, cỡ của hộp thoại còn mở

    // Đăng ký mã (mỗi mã một lần: TRION_CHECK).
    void add(const QString& id, Factory make);
    QStringList ids() const; // theo thứ tự chữ cái

    // Đang mở thì đưa lên, focus vào nó (không tạo bản thứ hai). Chưa thì dựng: cha là hộp thoại đang active (xếp
    // chồng: modal chặn cả nó) hoặc cửa sổ chính; khôi phục vị trí đã lưu (cỡ nếu đổi cỡ được), màn hình cũ đã rút thì
    // đặt giữa cửa sổ chính; rồi open() nếu isModal(), không thì show(). Đóng (done) thì lưu vị trí, cỡ và hủy bản đó.
    // Mã chưa đăng ký: nullptr.
    DialogFrame* open(const QString& id);
    DialogFrame* find(const QString& id) const; // bản đang mở (kể cả đang ẩn tạm), không có thì nullptr

    // Tiêu chí 5 (CI): dựng (không hiện) từng hộp thoại đã đăng ký ở ngôn ngữ hiện tại; mỗi nút xác nhận mang nhãn "OK"
    // (bỏ '&', khoảng trắng, không phân biệt hoa/thường) cho một dòng "<mã>: «nhãn»". Rỗng là đạt.
    QStringList confirmLabelIssues() const;

    // Vị trí khôi phục, hàm thuần (tọa độ màn hình, px logic). saved: thẻ đã lưu; anchor: cửa sổ chính; screens: vùng
    // dùng được của các màn hình đang cắm. Giữ màn hình chứa phần lớn nhất của saved; không màn hình nào chứa (đã rút)
    // thì lấy màn hình chứa tâm cửa sổ chính và đặt giữa cửa sổ chính. Rồi thu cỡ cho vừa và dời vào hẳn màn hình đó.
    static QRect place(const QRect& saved, const QRect& anchor, const QList<QRect>& screens);

signals:
    // Một hộp thoại đã mở xin trợ giúp (F1, nút Trợ giúp): DialogFrame::helpRequested.
    void helpRequested(const QString& topic);

private:
    void save(const DialogFrame* frame);

    QPointer<QWidget> main_;
    settings::Store& store_;
    std::map<QString, Factory> factories_;
    QHash<QString, QPointer<DialogFrame>> open_;
};

} // namespace trion::ui
