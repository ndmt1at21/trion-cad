#pragma once
// UIA-001 · Bộ định tuyến lệnh tạm. ponytail: thay bằng INT-002 (cmd/Cmd.h, InputPipeline::submit) khi có; giữ cú
// pháp macro menu của AutoCAD như ActionRegistry phát (ActionRegistry.h):
//   ^C^C…        hủy lệnh đang chạy (chưa có lệnh chạy: bỏ qua), có thể lặp
//   ' _ .        tiền tố, thứ tự bất kỳ: trong suốt, tên toàn cục tiếng Anh, lệnh gốc
//   TÊN a b;c    tên lệnh (không phân biệt hoa thường), rồi đối số; khoảng trắng và ';' là Enter
// Định tuyến:
//   - lệnh do giao diện xử lý, đăng ký bằng add(tên, fn) (CLEANSCREENON/OFF của CleanScreen; COMMANDLINE… ở bước
//     integration): gọi fn, bỏ đối số;
//   - sysvar trong suốt "'TÊN số-nguyên" (hành động gắn sysvar F3, F8…; TÊN không phải lệnh của danh mục): đặt
//     Sysvars, như lệnh trong suốt đặt sysvar của AutoCAD (gõ 'ORTHOMODE rồi 1);
//   - còn lại: log "trion.cmd" mức info (QT_LOGGING_RULES="trion.cmd.info=true") và phát unhandled(macro).
// ponytail: chưa chạy được .scr, (command ...) (INT-002, LSP); chưa có sysvar chỉ đọc (CLEANSCREENSTATE đặt được qua
// đây) và miền giá trị của sysvar (INT-005/INT-107).
#include <QHash>
#include <QObject>
#include <QString>

#include <functional>

namespace trion::actions {
class Sysvars;
}

namespace trion::shell {

class CommandRouter : public QObject {
    Q_OBJECT
public:
    explicit CommandRouter(actions::Sysvars& sysvars, QObject* parent = nullptr); // sysvars sống lâu hơn router

    // Lệnh giao diện tự xử lý; đăng ký lại cùng tên thì thay hàm cũ.
    void add(const QString& name, std::function<void()> fn);
    // Chuỗi lệnh từ ActionRegistry::setSink (sau này cả dòng lệnh). Luồng UI.
    void submit(const QString& macro);

signals:
    void unhandled(const QString& macro);

private:
    actions::Sysvars& sysvars_;
    QHash<QString, std::function<void()>> commands_; // tên viết hoa
};

} // namespace trion::shell
