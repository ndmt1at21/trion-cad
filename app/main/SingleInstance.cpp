#include "app/main/SingleInstance.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDeadlineTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QLoggingCategory>

#include <thread>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

namespace trion::app {

namespace {

Q_STATIC_LOGGING_CATEGORY(lcInstance, "trion.app", QtWarningMsg)

QString key(const QString& settingsPath)
{
    QString id = QDir::homePath() + u'\n' + QDir::cleanPath(QFileInfo(settingsPath).absoluteFilePath());
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    id = id.toCaseFolded(); // hệ tệp không phân biệt hoa thường: cùng tệp thì cùng khóa
#endif
    return u"trion-"_s +
           QString::fromLatin1(QCryptographicHash::hash(id.toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
}

} // namespace

SingleInstance::SingleInstance(const QString& settingsPath, QObject* parent)
    : QObject(parent), name_(key(settingsPath)), lockPath_(QDir::tempPath() + u'/' + name_ + u".lock"_s)
{
    server_.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&server_, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket* socket = server_.nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, this, [this, socket] { read(socket); });
        }
    });
    if (QCoreApplication* app = QCoreApplication::instance())
        connect(app, &QCoreApplication::aboutToQuit, &server_, &QLocalServer::close);
}

SingleInstance::~SingleInstance()
{
    server_.close(); // ngừng nghe trước, rồi mới thả khóa
    if (lock_ == -1)
        return;
#if defined(Q_OS_WIN)
    CloseHandle(HANDLE(lock_));
#else
    ::close(int(lock_));
#endif
}

bool SingleInstance::lock()
{
    if (lock_ != -1)
        return true;
#if defined(Q_OS_WIN)
    // Mở không chia sẻ: lần mở thứ hai (tiến trình khác hay cùng tiến trình) lỗi chia sẻ. Handle đóng (kể cả khi tiến
    // trình chết) thì tệp tự xóa.
    const HANDLE h = CreateFileW(reinterpret_cast<const wchar_t*>(QDir::toNativeSeparators(lockPath_).utf16()),
                                 GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                 FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    lock_ = qintptr(h);
#else
    // flock gắn với tệp đang mở: tiến trình chết thì hệ điều hành thả. O_CLOEXEC: tiến trình con không giữ khóa thay.
    // Không xóa tệp khi thả: xóa thì một lần chạy đang giữ fd của tệp cũ và một lần chạy tạo tệp mới cùng khóa được.
    const int fd = ::open(QFile::encodeName(lockPath_).constData(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0)
        return false;
    if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
        ::close(fd);
        return false;
    }
    lock_ = fd;
#endif
    return true;
}

bool SingleInstance::listen()
{
    if (!lock())
        return false;
    if (server_.isListening())
        return true;
    // Giữ khóa thì không còn phiên chính nào khác: socket cùng tên, nếu có, là của phiên đã chết.
    QLocalServer::removeServer(name_);
    if (!server_.listen(name_))
        qCWarning(lcInstance).noquote() << "không nghe được" << name_ << '(' << server_.errorString()
                                        << "): lần chạy sau không chuyển được tệp cho phiên này";
    return true;
}

bool SingleInstance::forward(const QStringList& paths, std::chrono::milliseconds timeout)
{
    const QDeadlineTimer deadline(timeout);
    QLocalSocket socket;
    for (;;) {
        socket.connectToServer(name_);
        if (socket.waitForConnected(int(deadline.remainingTime())))
            break;
        if (lock()) // phiên chính đã thoát hẳn: khóa giờ thuộc lần chạy này
            return false;
        if (deadline.hasExpired()) {
            qCWarning(lcInstance).noquote() << "không kết nối được phiên đang chạy:" << socket.errorString();
            return false;
        }
        std::this_thread::sleep_for(20ms); // phiên chính vừa giành khóa, chưa kịp nghe
    }
#if defined(Q_OS_WIN)
    ULONG pid = 0; // trước khi gửi: phiên chính đưa cửa sổ lên trước ngay khi nhận
    if (GetNamedPipeServerProcessId(HANDLE(socket.socketDescriptor()), &pid))
        AllowSetForegroundWindow(pid);
#endif
    socket.write(QJsonDocument(QJsonArray::fromStringList(paths)).toJson(QJsonDocument::Compact) + '\n');
    if (socket.waitForBytesWritten(int(deadline.remainingTime())) &&
        socket.waitForReadyRead(int(deadline.remainingTime())) && socket.read(1) == "\n") {
        qCInfo(lcInstance).noquote() << "đã chuyển cho phiên đang chạy:" << paths.join(u", "_s);
        return true;
    }
    qCWarning(lcInstance).noquote() << "phiên đang chạy không xác nhận:" << socket.errorString();
    return false;
}

void SingleInstance::read(QLocalSocket* socket)
{
    const auto drop = [socket] {
        socket->abort();
        socket->deleteLater();
    };
    if (socket->bytesAvailable() > kMaxMessage)
        return drop();
    if (!socket->canReadLine())
        return; // chưa đủ dòng
    const QJsonDocument doc = QJsonDocument::fromJson(socket->readLine());
    bool ok = doc.isArray();
    QStringList paths;
    for (const QJsonValue& v : doc.array()) {
        ok = ok && v.isString();
        paths << v.toString();
    }
    if (!ok)
        return drop();
    socket->write("\n");
    socket->flush();
    emit received(paths);
}

} // namespace trion::app
