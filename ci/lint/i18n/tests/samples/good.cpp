// Mẫu ĐÚNG cho ci/lint/i18n/lint.py: không dòng nào bị báo. Không biên dịch.
void Sample::build(int n, const QString& path)
{
    label->setText(tr("Lưu"));
    count->setText(tr("%n đối tượng", nullptr, n));
    button->setToolTip(tr("Vẽ đường thẳng (%1)").arg(u"LINE"_s)); // tên lệnh tiếng Anh qua placeholder
    edit->setText(QString());
    edit->setText({});
    edit->setText(u""_s);
    ratio->setText(u"%1 / %2"_s.arg(n).arg(n + 1)); // không có chữ cái
    total->setText(QString::number(n));
    auto* save = new QAction(QIcon(u":/icons/save.svg"_s), tr("Lưu"), this);
    menu->addAction(tr("Mở"), QKeySequence(u"Ctrl+O"_s), this, &Sample::open);
    menu->addAction(tr("Xóa"), this, [this] { remove(u"tất cả"_s); });
    QMessageBox::warning(this, tr("Lỗi"), tr("Không mở được %1").arg(path));
    setWindowTitle(QStringLiteral("TrionCAD")); // i18n-ok: tên sản phẩm, không dịch
    setObjectName(u"saveButton"_s);
    // label->setText("Lưu");  /* setToolTip("Mở") */
    const char* doc = "setText(\"Lưu\") + tr(\"x\")";
    const auto css = R"css(QLabel { color: red; } setText("x"))css";
    char quote = '"';
    int big = 1'000'000;
    label->setText(tr("Sau ký tự %1").arg(quote));
    transform.translate(big + 1, n);
    title->setText(tr("Bước %1").arg(n + 1));
    const QStringList items{tr("Một"), tr("Hai")};
    combo->addItems(items);
    tip = tr("Đã lưu %1 lúc %2").arg(path, time);
}
