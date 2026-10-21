#pragma once
// UI0-001 · Chạy một phiên (07.1 §13.6: tab tệp trong một cửa sổ). Lần chạy sau chuyển danh sách đường dẫn cho phiên
// đang chạy rồi thoát mã 0; phiên đang chạy phát received(paths) (Application đưa cửa sổ lên trước, phát
// openRequested).
//
// Một phiên cho mỗi tệp cài đặt của mỗi người dùng: khóa = băm (thư mục nhà, đường dẫn tệp cài đặt). Phiên của người
// dùng khác và test chạy song song với `--settings <tệp tạm>` không đụng nhau; hai tiến trình không cùng ghi một tệp
// cài đặt. Phiên chính giữ khóa của hệ điều hành trên <thư mục tạm>/<khóa>.lock (flock; Windows: mở độc quyền, xóa khi
// đóng): hai lần chạy cùng lúc không thành hai phiên, tiến trình chết thì hệ điều hành tự thả khóa. Không dùng
// QLockFile: nó fdatasync/FlushFileBuffers mỗi lần khóa (10–30 ms trên đường khởi động, đo ở UI0-001) và đoán khóa bỏ
// lại theo PID. Phiên chính nghe QLocalServer <khóa>, chỉ người dùng hiện tại kết nối được; socket còn sót của phiên đã
// chết bị dọn trước khi nghe. Ứng dụng bắt đầu thoát (aboutToQuit) thì ngừng nghe: lần chạy mới chờ khóa được thả rồi
// tự thành phiên chính.
//
// Giao thức, mỗi kết nối: lần chạy sau gửi một dòng JSON ["<đường dẫn tuyệt đối>", ...] kết thúc bằng '\n'; phiên chính
// đáp "\n" rồi phát received(). Dòng dài hơn kMaxMessage hoặc không phải mảng chuỗi: đóng kết nối, bỏ qua.
//
// Dùng (main):
//   SingleInstance instance(options.settingsPath);
//   if (!instance.listen()) {
//       if (instance.forward(options.paths))
//           return 0;
//       if (!instance.listen()) // phiên đang chạy không nhận: nếu nó vừa thoát thì forward() đã giành được khóa
//           return 1;
//   }
//   QObject::connect(&instance, &SingleInstance::received, &app, &Application::requestOpen);

#include <QLocalServer>
#include <QObject>
#include <QString>
#include <QStringList>

#include <chrono>

class QLocalSocket;

namespace trion::app {

class SingleInstance : public QObject {
    Q_OBJECT
public:
    static constexpr qint64 kMaxMessage = 1 << 20; // byte

    // Chỉ tính khóa từ settingsPath; chưa khóa, chưa nghe.
    explicit SingleInstance(const QString& settingsPath, QObject* parent = nullptr);
    ~SingleInstance() override; // ngừng nghe (xóa socket), rồi thả khóa

    // Luồng UI: giành vai phiên chính. true: chưa có phiên nào cho tệp cài đặt này (hoặc forward() vừa giành được
    // khóa), từ đây nhận received(); false: đã có phiên chính.
    bool listen();

    // Lần chạy sau (listen() trả false): gửi paths cho phiên chính, chờ xác nhận tới hết timeout. Phiên chính có thể
    // vừa giành khóa mà chưa nghe: thử kết nối lại; trong lúc đó phiên chính thoát hẳn thì giành lấy khóa và trả false
    // (gọi listen() để thành phiên chính). true: phiên chính đã nhận. Windows: nhường quyền đưa cửa sổ lên trước cho
    // phiên chính (AllowSetForegroundWindow), vì chỉ tiến trình người dùng vừa mở mới có quyền đó.
    bool forward(const QStringList& paths, std::chrono::milliseconds timeout = std::chrono::seconds(5));

    QString serverName() const { return name_; }

signals:
    void received(const QStringList& paths); // phiên chính, luồng UI

private:
    bool lock(); // giành khóa; đã giữ thì true
    void read(QLocalSocket* socket);

    QString name_;
    QString lockPath_;
    qintptr lock_ = -1; // fd (Unix) hoặc HANDLE (Windows) đang giữ khóa
    QLocalServer server_;
};

} // namespace trion::app
