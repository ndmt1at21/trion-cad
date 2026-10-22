// UIA-102 tiêu chí 1 · Lint nội dung ribbon (chạy trong CI, chặn merge): mọi tệp JSON tab trong :/trion/ribbon/
//   - đọc được, đúng khóa và kiểu của resources/schema.json (RibbonModel::errors rỗng);
//   - điều khiển, mục split, mục panel mở rộng trỏ tới hành động có trong ActionRegistry (danh mục lệnh tạm thay
//     INT-001, hành động gắn sysvar của Keymap);
//   - tên biểu tượng có trong bộ biểu tượng UI0-103 (IconProvider::names, manifest.json);
//   - mọi nhãn (tab, panel, điều khiển, mục) có bản English đã dịch trong app/i18n/trion_en.ts, đúng ngữ cảnh
//     RibbonModel::context ("actions" nếu trùng nhãn lệnh, còn lại "ribbon").
// Mỗi lỗi in một dòng (QWARN) rồi test hỏng với số lỗi.
#include "app/core/actions/ActionRegistry.h"
#include "app/core/keymap/Keymap.h"
#include "app/shell/ribbon/RibbonModel.h"
#include "app/theme/icons/IconProvider.h"

#include <QAction>
#include <QFile>
#include <QHash>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>
#include <QXmlStreamReader>

#include <functional>
#include <memory>

using namespace Qt::StringLiterals;
using namespace trion;
using namespace trion::ribbon;

namespace {

// (ngữ cảnh, nguồn) đã có bản dịch xong trong tệp .ts.
QSet<std::pair<QString, QString>> translated(const QString& ts)
{
    QSet<std::pair<QString, QString>> out;
    QFile f(ts);
    if (!f.open(QIODevice::ReadOnly))
        return out;
    QXmlStreamReader x(&f);
    QString context, source;
    while (!x.atEnd()) {
        if (x.readNext() != QXmlStreamReader::StartElement)
            continue;
        if (x.name() == u"name")
            context = x.readElementText();
        else if (x.name() == u"source")
            source = x.readElementText();
        else if (x.name() == u"translation") {
            const QString type = x.attributes().value(u"type").toString();
            if (const QString t = x.readElementText(QXmlStreamReader::IncludeChildElements);
                !t.isEmpty() && type.isEmpty())
                out.insert({context, source});
        }
    }
    return out;
}

} // namespace

class RibbonLintTest : public QObject {
    Q_OBJECT

    QTemporaryDir dir_;
    std::unique_ptr<settings::Store> store_;
    std::unique_ptr<keymap::Keymap> keymap_;
    std::unique_ptr<actions::Sysvars> sysvars_;
    std::unique_ptr<actions::ActionRegistry> registry_;

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        store_ = std::make_unique<settings::Store>(dir_.filePath(u"user.json"_s));
        settings::Store::setInstance(store_.get());
        keymap_ = std::make_unique<keymap::Keymap>(*store_, keymap::Platform::Windows);
        sysvars_ = std::make_unique<actions::Sysvars>();
        registry_ = std::make_unique<actions::ActionRegistry>(*keymap_, *sysvars_);
    }

    void cleanupTestCase()
    {
        registry_.reset();
        sysvars_.reset();
        keymap_.reset();
        settings::Store::setInstance(nullptr);
        store_.reset();
    }

    void lint()
    {
        const QStringList files = RibbonModel::resourceFiles();
        QVERIFY2(files.size() >= 8, qPrintable(files.join(u", ")));
        const RibbonModel model(files);
        QStringList bad = model.errors();

        const QStringList iconList = theme::IconProvider::names();
        const QSet<QString> icons(iconList.begin(), iconList.end());
        const auto en = translated(u":/ribbon_lint/trion_en.ts"_s);
        QVERIFY2(!en.isEmpty(), "không đọc được trion_en.ts");
        QStringList missing; // nhãn thiếu bản English, mỗi nhãn một lần

        for (const Tab& t : model.tabs()) {
            const auto label = [&](const QString& s, const QString& where) {
                const QString ctx = QString::fromLatin1(RibbonModel::context(s));
                if (!en.contains({ctx, s}) && !missing.contains(s)) {
                    missing << s;
                    bad << u"%1: nhãn \"%2\" thiếu bản English (ngữ cảnh %3) trong app/i18n/trion_en.ts"_s.arg(
                        where, QString(s).replace(u'\n', u"\\n"_s), ctx);
                }
            };
            const auto command = [&](const QString& cmd, const QString& where) {
                if (!registry_->action(cmd))
                    bad << u"%1: lệnh \"%2\" không có trong ActionRegistry (INT-001)"_s.arg(where, cmd);
            };
            const auto icon = [&](const QString& name, const QString& where) {
                if (!name.isEmpty() && !icons.contains(name))
                    bad << u"%1: biểu tượng \"%2\" không có trong bộ biểu tượng UI0-103"_s.arg(where, name);
            };
            const auto menu = [&](const std::vector<MenuItem>& items, const QString& where) {
                for (const MenuItem& m : items) {
                    if (m.separator)
                        continue;
                    const QString at = u"%1 › %2"_s.arg(where, m.label);
                    label(m.label, at);
                    command(m.cmd, at);
                    icon(m.icon, at);
                }
            };
            std::function<void(const std::vector<Item>&, const QString&)> walk;
            walk = [&](const std::vector<Item>& items, const QString& where) {
                for (const Item& it : items) {
                    walk(it.items, where);
                    if (it.kind != Item::Kind::Control)
                        continue;
                    const Control& c = it.control;
                    const QString at = u"%1 › %2"_s.arg(where, c.id);
                    label(c.label, at);
                    if (!c.cmd.isEmpty())
                        command(c.cmd, at);
                    icon(c.icon, at);
                    menu(c.menu, at);
                }
            };
            label(t.name, t.id);
            for (const Panel& p : t.panels) {
                const QString at = u"%1/%2"_s.arg(t.id, p.id);
                label(p.name, at);
                icon(p.icon, at);
                walk(p.items, at);
                menu(p.more, at + u" (mở rộng)"_s);
            }
        }
        for (const QString& b : std::as_const(bad))
            qWarning("%s", qUtf8Printable(b));
        if (!missing.isEmpty()) // danh sách gọn cho người điều phối: chuỗi cần thêm vào trion_en.ts
            qInfo("%lld nhãn thiếu bản English", qlonglong(missing.size()));
        QVERIFY2(bad.isEmpty(), qPrintable(u"%1 lỗi nội dung ribbon (xem các dòng trên)"_s.arg(bad.size())));
    }
};

QTEST_MAIN(RibbonLintTest)
#include "ribbon_lint_test.moc"
