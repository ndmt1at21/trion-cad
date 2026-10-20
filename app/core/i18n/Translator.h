#pragma once
// Ngôn ngữ giao diện (UI0-104, 07.1 §13.4): tiếng Việt (ngôn ngữ nguồn của chuỗi, mặc định) và English,
// đổi khi đang chạy, không khởi động lại. Bản dịch nằm trong app/i18n/trion_<mã>.ts, build nhúng .qm vào
// resource :/i18n/ (app/i18n/i18n.cmake).
//
// Dùng (main):
//   QApplication app(argc, argv);
//   trion::i18n::Translator translator;   // đọc lựa chọn đã lưu (lần chạy đầu: "vi") và cài bản dịch
//   ...
//   Translator::instance()->setLanguage(u"en"_s);   // ví dụ từ hộp Tùy chọn
//
// Quy ước cho widget (DoD-U 6; ví dụ chạy được: Panel trong tests/translator_test.cpp):
//   class Panel : public QWidget {
//       Q_OBJECT
//   public:
//       explicit Panel(QWidget* parent = nullptr) : QWidget(parent)
//       {
//           ...                // dựng cây widget, không đặt chuỗi ở đây
//           retranslate();
//       }
//   protected:
//       void changeEvent(QEvent* e) override
//       {
//           if (e->type() == QEvent::LanguageChange)
//               retranslate();
//           QWidget::changeEvent(e);
//       }
//   private:
//       void retranslate()     // chỉ đặt chuỗi; không đụng lựa chọn, vị trí cuộn, nội dung ô nhập
//       {
//           title_->setText(tr("Thuộc tính"));
//           count_->setText(tr("%n đối tượng", nullptr, n_));          // số nhiều: %n
//           path_->setToolTip(tr("Đã lưu %1 lúc %2").arg(file, time)); // placeholder, không ghép bằng +
//           combo_->setItemText(0, tr("Mặc định"));                    // đổi chữ từng mục, giữ mục đang chọn
//       }
//   };
// - Mọi chuỗi hiển thị qua tr() (QObject) hoặc QCoreApplication::translate(); ci/lint/i18n bắt chuỗi viết
//   thẳng và chuỗi ghép bằng +. Không giữ chuỗi đã dịch trong biến sống lâu: bảng tĩnh đánh dấu bằng
//   QT_TR_NOOP / QT_TRANSLATE_NOOP, dịch bằng tr() lúc hiển thị.
// - Đối tượng không phải widget (model, delegate) không nhận LanguageChange: nối languageChanged() rồi phát
//   headerDataChanged() / dataChanged().
// - Chuỗi mới: `cmake --build <build> --target update_translations` cập nhật app/i18n/*.ts, dịch bằng Qt
//   Linguist; `ctest -L i18n` báo chuỗi thiếu bản dịch.

#include "core/base/settings/Settings.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTranslator>

#include <memory>

namespace trion::i18n {

// Lựa chọn ngôn ngữ (NT-010): mã ISO 639-1. Giá trị lạ trong tệp cài đặt coi như "vi".
inline const settings::Setting<QString> kLanguage{"ui/language", QStringLiteral("vi")};

class Translator : public QObject {
    Q_OBJECT
public:
    // Cần QCoreApplication. Đọc kLanguage từ settings::Store::instance() và cài bản dịch; không theo locale
    // của hệ điều hành (07.1 §13.4: tiếng Việt mặc định).
    explicit Translator(QObject* parent = nullptr);
    ~Translator() override; // gỡ bản dịch

    static Translator* instance();  // bộ dịch đang sống (main tạo), nullptr nếu chưa có
    static QStringList languages(); // {"vi", "en"}

    QString language() const { return lang_; }

    // Đổi ngay: thay QTranslator nên Qt gửi QEvent::LanguageChange tới mọi widget ở lượt xử lý sự kiện kế
    // tiếp (mỗi widget một lần), lưu lựa chọn vào Store, phát languageChanged. Mã lạ: trả false, giữ nguyên.
    bool setLanguage(const QString& lang);

signals:
    void languageChanged(const QString& lang);

private:
    bool install(const QString& lang);

    std::unique_ptr<QTranslator> qm_;
    QString lang_;
};

} // namespace trion::i18n
