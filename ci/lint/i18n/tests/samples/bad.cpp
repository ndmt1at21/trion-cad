// Mẫu SAI cho ci/lint/i18n/lint.py: mỗi dòng đánh dấu "lỗi (a)" / "lỗi (b)" phải bị báo đúng loại, dòng khác
// không. Không biên dịch.
void Sample::build(int n, const QString& name)
{
    label->setText("Lưu"); // lỗi (a)
    button->setToolTip(QStringLiteral("Mở bản vẽ")); // lỗi (a)
    setWindowTitle(u"TrionCAD"_s); // lỗi (a)
    edit->setPlaceholderText(QString::fromUtf8("Tìm")); // lỗi (a)
    action->setStatusTip(n > 0 ? "Bật" : "Tắt"); // lỗi (a)
    auto* save = new QAction("Lưu", this); // lỗi (a)
    QMessageBox::warning(this, tr("Lỗi"), "Không mở được tệp"); // lỗi (a)
    menu->addAction(QIcon(u":/icons/save.svg"_s), "Lưu thành"); // lỗi (a)
    QMessageBox::information(this,
                             tr("Xong"),
                             "Đã xuất các tệp"); // lỗi (a)
    combo->addItems({"Mặc định", "Tùy chỉnh"}); // lỗi (a)
    close->setText(QT_TR_NOOP("Đóng")); // lỗi (a): chỉ đánh dấu, không dịch
    QLabel title("Tên", this); // lỗi (a)
    unit->setText(name + " mm"); // lỗi (a)
    setWindowTitle(QStringLiteral("TrionCAD")); // lỗi (a): "i18n-ok" thiếu lý do thì không được miễn
    label->setText(tr("Đã chọn ") + QString::number(n)); // lỗi (b)
    tip = QString::number(n) + tr(" đối tượng"); // lỗi (b)
    tip += tr("Xong"); // lỗi (b)
    status->showMessage(tr("Lưu %1").arg(name) % u"…"_s); // lỗi (b)
    tip = QObject::tr("Kích thước") + name; // lỗi (b)
    frame->setConfirm("Xóa", Button::Kind::Danger); // lỗi (a): DialogFrame::setConfirm
}
