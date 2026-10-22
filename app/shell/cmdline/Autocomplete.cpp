#include "app/shell/cmdline/Autocomplete.h"

#include "app/core/a11y/Announce.h"
#include "app/core/actions/ActionRegistry.h"
#include "app/core/actions/SearchTerms.h"
#include "app/core/perf/UiProbe.h"
#include "app/theme/fonts/FontRegistry.h"
#include "app/theme/icons/IconProvider.h"
#include "app/theme/ThemeManager.h"
#include "app/widgets/tooltip/RichTooltip.h"
#include "core/base/text/Fold.h"

#include <QAbstractListModel>
#include <QCoreApplication>
#include <QHash>
#include <QLabel>
#include <QListView>
#include <QPainter>
#include <QScrollBar>
#include <QSet>
#include <QStringTokenizer>
#include <QStyledItemDelegate>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <tuple>

using namespace Qt::StringLiterals;

namespace trion::ui {

namespace {

using theme::TextRole;
using theme::Weight;

theme::ThemeManager& tm()
{
    return theme::ThemeManager::instance();
}
int px(tok::PxId id)
{
    return qRound(tm().px(id));
}
QColor col(tok::ColorId id)
{
    return tm().color(id);
}

// Giá trị của prototype không có token riêng (css/app.css .ac, .ac .ai).
constexpr int kWidth = 520;     // .ac width (rộng hơn nếu dòng cần: không cắt chữ)
constexpr int kMaxHeight = 300; // .ac max-height, quá thì cuộn
constexpr int kRowH = 26;       // .ai height
constexpr int kIconW = 20;      // cột biểu tượng
constexpr int kNameW = 120;     // cột tên tối thiểu
constexpr int kAliasW = 60;     // cột viết tắt tối thiểu
constexpr int kBorder = 1;

constexpr int kExact = 100;  // mức khớp: xem Autocomplete.h
constexpr int kStrong = 55;  // từ mức này dòng đầu được chọn sẵn
constexpr int kUseBonus = 2; // điểm mỗi lần dùng
constexpr int kMaxBonus = 30;
constexpr int kMaxTypo = 63; // chữ gõ dài hơn: không tìm gợi ý gõ sai

// ponytail: tên biến hệ thống thông dụng (tên công khai của AutoCAD và biến TC… của TrionCAD), chưa có mô tả, giá trị;
// INT-005/INT-107 (SysvarRegistry) thay. Tên trùng lệnh (LTSCALE, DIMSTYLE…) chỉ hiện là lệnh.
constexpr char kSysvars[] =
    "ACADPREFIX ACADVER ANGBASE ANGDIR ANNOALLVISIBLE ANNOAUTOSCALE ANNOTATIVEDWG APBOX APERTURE AREA ATTDIA ATTDISP "
    "ATTMODE ATTREQ AUDITCTL AUNITS AUPREC AUTOCOMPLETEDELAY AUTOCOMPLETEMODE AUTOSNAP BACKGROUNDPLOT BLIPMODE "
    "BLOCKEDITOR CANNOSCALE CDATE CECOLOR CELTSCALE CELTYPE CELWEIGHT CETRANSPARENCY CHAMFERA CHAMFERB CHAMFERC "
    "CHAMFERD CHAMMODE CIRCLERAD CLAYER CLEANSCREENSTATE CLIPROMPTLINES CLISTATE CMDACTIVE CMDDIA CMDECHO "
    "CMDINPUTHISTORYMAX CMDNAMES CMLJUST CMLSCALE CMLSTYLE CONSTRAINTINFER COORDS COPYMODE CPLOTSTYLE "
    "CROSSINGAREACOLOR CTAB CTABLESTYLE CURSORSIZE CVPORT DATE DBLCLKEDIT DCTCUST DCTMAIN DEFLPLSTYLE DEFPLSTYLE "
    "DELOBJ DIMADEC DIMALT DIMASSOC DIMASZ DIMBLK DIMCEN DIMCLRD DIMCLRE DIMCLRT DIMDEC DIMDLE DIMDLI DIMEXE DIMEXO "
    "DIMGAP DIMLFAC DIMLUNIT DIMSCALE DIMSTYLE DIMTAD DIMTIH DIMTOH DIMTXSTY DIMTXT DIMUPT DISPSILH DONUTID DONUTOD "
    "DRAGMODE DRAWORDERCTL DWGCHECK DWGNAME DWGPREFIX DWGTITLED DYNMODE DYNPROMPT DYNTOOLTIPS EDGEMODE ELEVATION "
    "EXPERT EXPLMODE EXTMAX EXTMIN FACETRES FIELDDISPLAY FIELDEVAL FILEDIA FILLETRAD FILLMODE FONTALT FONTMAP FRAME "
    "GRIDDISPLAY GRIDMAJOR GRIDMODE GRIDSTYLE GRIDUNIT GRIPBLOCK GRIPCOLOR GRIPHOT GRIPHOVER GRIPOBJLIMIT GRIPS "
    "GRIPSIZE GRIPTIPS HANDLES HIDETEXT HIGHLIGHT HPANG HPASSOC HPBOUND HPDOUBLE HPDRAWORDER HPGAPTOL "
    "HPISLANDDETECTION HPLAYER HPNAME HPORIGIN HPQUICKPREVIEW HPSCALE HPSEPARATE HPSPACE HPTRANSPARENCY IMAGEFRAME "
    "IMAGEHLT INDEXCTL INPUTHISTORYMODE INSBASE INSNAME INSUNITS INSUNITSDEFSOURCE INSUNITSDEFTARGET ISAVEBAK "
    "ISAVEPERCENT ISOLINES LASTPOINT LAYERNOTIFY LAYLOCKFADECTL LENSLENGTH LIMCHECK LIMMAX LIMMIN LISPSYS LOCALE "
    "LOGFILEMODE LTSCALE LUNITS LUPREC LWDEFAULT LWDISPLAY LWUNITS MAXACTVP MAXSORT MBUTTONPAN MEASUREINIT "
    "MEASUREMENT MENUBAR MIRRTEXT MLEADERSCALE MODEMACRO MSLTSCALE MTEXTED MTEXTFIXED MTJIGSTRING NOMUTT "
    "OBSCUREDCOLOR OFFSETDIST OFFSETERASE OFFSETGAPTYPE OLEHIDE ORTHOMODE OSMODE OSNAPCOORD OSOPTIONS PALETTEOPAQUE "
    "PDMODE PDSIZE PEDITACCEPT PELLIPSE PERIMETER PICKADD PICKAUTO PICKBOX PICKDRAG PICKFIRST PICKSTYLE PLINEGEN "
    "PLINETYPE PLINEWID PLOTROTMODE PLQUIET POLARADDANG POLARANG POLARDIST POLARMODE POLYSIDES PREVIEWEFFECT "
    "PROJMODE PROXYGRAPHICS PROXYNOTICE PROXYSHOW PSLTSCALE PSTYLEMODE PSTYLEPOLICY PUCSBASE QPLOCATION QPMODE "
    "QTEXTMODE RASTERPREVIEW REGENMODE REMEMBERFOLDERS REPORTERROR ROLLOVERTIPS SAVEFILE SAVEFILEPATH SAVENAME "
    "SAVETIME SDI SELECTIONAREA SELECTIONCYCLING SELECTIONPREVIEW SHORTCUTMENU SKPOLY SNAPANG SNAPBASE SNAPISOPAIR "
    "SNAPMODE SNAPSTYL SNAPTYPE SNAPUNIT SORTENTS SPLFRAME SPLINESEGS SPLINETYPE STANDARDSVIOLATION STARTUP "
    "STATUSBAR SURFTAB1 SURFTAB2 SYSCODEPAGE TABMODE TARGET TCFONTDOC TCPROMPTLANG TDCREATE TDINDWG TDUPDATE "
    "TDUSRTIMER TEMPPREFIX TEXTFILL TEXTSIZE TEXTSTYLE THICKNESS TILEMODE TOOLTIPS TRACKPATH TRANSPARENCYDISPLAY "
    "TRIMEXTENDMODE TRIMMODE UCSFOLLOW UCSICON UCSNAME UCSORG UCSVP UCSXDIR UCSYDIR UNDOCTL UNITMODE VIEWCTR "
    "VIEWDIR VIEWSIZE VISRETAIN VTDURATION VTENABLE WINDOWAREACOLOR WIPEOUTFRAME WORLDUCS XCLIPFRAME XDWGFADECTL "
    "XFADECTL XLOADCTL XREFCTL XREFNOTIFY XREFTYPE ZOOMFACTOR ZOOMWHEEL";

// Tọa độ, số, biểu thức LISP: không phải tên lệnh, không tự hoàn thành (updateAC của prototype).
bool skipped(const QString& typed)
{
    const QStringView t = QStringView(typed).trimmed();
    return t.isEmpty() || t.contains(u',') || t.contains(u'@') || t.contains(u'<') || t.contains(u'#') ||
           t.front().isDigit() || QStringView(u".-(").contains(t.front());
}

// Lệnh đã học cho lỗi gõ (viết hoa): lệnh được chọn nhiều nhất, nếu đủ kLearnAfter lần; rỗng nếu chưa.
QString learnedFor(const QString& typo)
{
    const QJsonObject picks = settings::Store::instance().get(kAutoCorrect).value(typo).toObject();
    QString best;
    int most = Autocomplete::kLearnAfter - 1;
    for (auto it = picks.begin(); it != picks.end(); ++it)
        if (it.value().toInt() > most) {
            most = it.value().toInt();
            best = it.key();
        }
    return best;
}

// Khoảng cách Damerau–Levenshtein bản OSA (thay, thêm, bớt một ký tự, đảo hai ký tự kề: mỗi việc 1); > max thì trả
// max + 1 ngay khi chắc chắn (dừng sớm theo hàng).
int distance(QStringView a, QStringView b, int max)
{
    const auto n = int(a.size()), m = int(b.size());
    if (std::abs(n - m) > max || n > kMaxTypo || m > kMaxTypo)
        return max + 1;
    std::array<int, kMaxTypo + 1> before{}, prev{}, cur{};
    for (int j = 0; j <= m; ++j)
        prev[std::size_t(j)] = j;
    for (int i = 1; i <= n; ++i) {
        cur[0] = i;
        int low = i;
        for (int j = 1; j <= m; ++j) {
            const auto J = std::size_t(j);
            int d = std::min({prev[J] + 1, cur[J - 1] + 1, prev[J - 1] + (a[i - 1] == b[j - 1] ? 0 : 1)});
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1])
                d = std::min(d, before[J - 2] + 1);
            cur[J] = d;
            low = std::min(low, d);
        }
        if (low > max)
            return max + 1;
        before = prev;
        prev = cur;
    }
    return prev[std::size_t(m)];
}

} // namespace

struct Autocomplete::Entry {
    enum Kind : std::uint8_t { Command, Extra, Sysvar } kind;
    QString name;     // "LINE"
    QString aliases;  // ",L,": viết tắt (không gồm tên); rỗng nếu không có
    QString keywords; // ",duong thang,doan thang,": từ khóa tiếng Việt đã gấp
    QString label;    // mô tả ngắn đã dịch
    QString folded; // nhãn đã gấp để tìm giữa chuỗi; rỗng: không tìm theo nhãn (biến, mô tả chung)
    QString icon;

    // Cột viết tắt: tối đa 2 (prototype).
    QString aliasText() const
    {
        QStringList out;
        for (const QStringView a : qTokenize(aliases, u',', Qt::SkipEmptyParts))
            if (out.size() < 2)
                out << a.toString();
        return out.join(u", "_s);
    }
};

// Chữ gõ và thiết lập, chuẩn bị một lần cho mỗi phím.
struct Autocomplete::Query {
    explicit Query(const QString& typed)
    {
        QStringView t = QStringView(typed).trimmed();
        if (t.startsWith(u'\'')) // lệnh trong suốt 'ZOOM: khớp tên sau dấu '
            t = t.sliced(1);
        upper = t.toString().toUpper();
        folded = text::foldForSearch(t);
        aliasExact = u',' + upper + u',';
        aliasPrefix = u',' + upper;
        keyExact = u',' + folded + u',';
        keyPrefix = u',' + folded;
        const settings::Store& s = settings::Store::instance();
        sysvars = s.get(kSearchSysvars);
        vietnamese = s.get(kSearchVietnamese);
        usage = s.get(kUsage);
        learned = learnedFor(upper);
    }
    int uses(const QString& name) const { return usage.value(name).toInt(); }

    // Mức khớp của một dòng; 0: không khớp.
    int score(const Entry& e) const
    {
        if (upper.isEmpty() || (e.kind == Entry::Sysvar && (!sysvars || upper.size() < 2)))
            return 0;
        if (e.name == upper || e.aliases.contains(aliasExact) || e.name == learned)
            return kExact;
        if (e.name.startsWith(upper))
            return 80;
        if (e.aliases.contains(aliasPrefix))
            return 70;
        if (vietnamese && folded.size() >= 2) {
            if (e.keywords.contains(keyExact))
                return 65;
            if (e.keywords.contains(keyPrefix))
                return kStrong;
            if (e.folded.contains(folded))
                return 50;
        }
        if (upper.size() >= 3 && e.name.contains(upper))
            return 40;
        return 0;
    }

    QString upper, folded, aliasExact, aliasPrefix, keyExact, keyPrefix, learned;
    bool sysvars = true, vietnamese = true;
    QJsonObject usage;
};

// Các dòng đang hiện (vị trí trong chỉ mục).
class Autocomplete::Model final : public QAbstractListModel {
public:
    explicit Model(Autocomplete* owner) : QAbstractListModel(owner), owner_(owner) {}

    void reset()
    {
        beginResetModel();
        endResetModel();
    }
    const Entry& entry(int row) const { return owner_->index()[std::size_t(owner_->rows_[std::size_t(row)])]; }
    Edition need(const Entry& e) const
    {
        return e.kind == Entry::Command && owner_->lock_ ? owner_->lock_(e.name) : Edition::None;
    }

    int rowCount(const QModelIndex& parent = {}) const override
    {
        return parent.isValid() ? 0 : int(owner_->rows_.size());
    }
    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid() || index.row() >= rowCount())
            return {};
        const Entry& e = entry(index.row());
        switch (role) {
        case Qt::DisplayRole:
            return e.name;
        case Qt::AccessibleTextRole: { // "TRIM, TR, Cắt bớt đối tượng, Pro"
            QStringList parts{e.name, e.aliasText(), e.label, RichTooltip::editionName(need(e))};
            parts.removeAll(QString());
            return parts.join(u", "_s);
        }
        default:
            return {};
        }
    }

private:
    Autocomplete* owner_;
};

// Dòng .ai: [biểu tượng] TÊN  viết tắt  mô tả [huy hiệu]; chọn, rê: nền --c-accent-soft.
class Autocomplete::Delegate final : public QStyledItemDelegate {
public:
    explicit Delegate(Autocomplete* owner) : QStyledItemDelegate(owner), owner_(owner) {}

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return {owner_->rowW_, kRowH}; }

    void paint(QPainter* p, const QStyleOptionViewItem& o, const QModelIndex& index) const override
    {
        const Entry& e = owner_->model_->entry(index.row());
        const QRect r = o.rect;
        const int gap = px(tok::sp_4), ic = px(tok::ic);
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        if (o.state & (QStyle::State_Selected | QStyle::State_MouseOver)) { // .ai:hover, .ai.is-hot
            const qreal radius = tm().px(tok::r);
            p->setPen(Qt::NoPen);
            p->setBrush(col(tok::c_accent_soft));
            p->drawRoundedRect(QRectF(r), radius, radius);
        }
        int x = r.left() + gap;
        p->drawPixmap(x + (kIconW - ic) / 2, r.top() + (r.height() - ic) / 2,
                      theme::IconProvider::instance().pixmap(e.icon, ic, p->device()->devicePixelRatioF(),
                                                             theme::IconState::Normal));
        x += kIconW + gap;
        const bool fits = r.width() >= owner_->rowW_; // place() đo các dòng đang hiện: thường không phải rút gọn
        const auto cell = [&](const QString& s, int width, const QFont& font, tok::ColorId color) {
            p->setFont(font);
            p->setPen(col(color));
            p->drawText(QRect(x, r.top(), width, r.height()), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                        fits ? s : QFontMetrics(font).elidedText(s, Qt::ElideRight, width));
            x += width + gap;
        };
        // Biến: tên --c-info, biểu tượng bánh răng, mô tả "Biến hệ thống" (không chỉ bằng màu).
        cell(e.name, owner_->nameW_, theme::font(TextRole::Mono, Weight::Medium),
             e.kind == Entry::Sysvar ? tok::c_info : tok::c_text);
        cell(e.aliasText(), owner_->aliasW_, theme::font(TextRole::MonoCaption), tok::c_text_3);
        const Edition need = owner_->model_->need(e);
        const QSize badge = RichTooltip::badgeSize(need);
        const QFont label = theme::font(TextRole::Label);
        const int room = r.right() + 1 - gap - x - (badge.isEmpty() ? 0 : badge.width() + px(tok::sp_3));
        const int descX = x, used = std::min(room, QFontMetrics(label).horizontalAdvance(e.label));
        cell(e.label, std::max(0, room), label, tok::c_text_2);
        if (!badge.isEmpty()) // .ed.xs "Pro" | "Viewer Pro": chữ, không chỉ màu
            RichTooltip::paintBadge(
                *p, QRect(QPoint(descX + used + px(tok::sp_3), r.center().y() - badge.height() / 2), badge), need);
        p->restore();
    }

private:
    Autocomplete* owner_;
};

Autocomplete::Autocomplete(QWidget* anchor)
    : QWidget(anchor), anchor_(anchor), model_(new Model(this)), list_(new QListView(this)), footer_(new QLabel(this))
{
    hide();
    setFocusPolicy(Qt::NoFocus);
    list_->setModel(model_);
    list_->setItemDelegate(new Delegate(this));
    list_->setFocusPolicy(Qt::NoFocus); // focus, bộ gõ ở lại ô nhập của dòng lệnh
    list_->verticalScrollBar()->setFocusPolicy(Qt::NoFocus);
    list_->setFrameShape(QFrame::NoFrame);
    list_->setUniformItemSizes(true);
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    list_->setMouseTracking(true);
    list_->viewport()->setAttribute(Qt::WA_Hover); // rê dòng: nền như dòng đang chọn
    list_->viewport()->setAutoFillBackground(false);
    footer_->setWordWrap(true); // vi-XL: xuống dòng, không cắt
    connect(list_, &QAbstractItemView::clicked, this, [this](const QModelIndex& i) {
        select(i.row());
        accept(false);
    });
    connect(&settings::Store::instance(), &settings::Store::changed, this, [this](const QString& key) {
        if (live_ && (key == QLatin1StringView(kSearchSysvars.key) || key == QLatin1StringView(kSearchVietnamese.key)))
            refresh(typed_); // tắt, bật "tìm biến", "tìm theo tiếng Việt": áp ngay
    });
    const auto restyle = [this] {
        footer_->setFont(theme::font(TextRole::Caption));
        QPalette pal = footer_->palette();
        pal.setColor(QPalette::WindowText, col(tok::c_text_3));
        footer_->setPalette(pal);
        footer_->setContentsMargins(px(tok::sp_4), px(tok::sp_2), px(tok::sp_4), px(tok::sp_1)); // .af padding
        if (live_)
            refresh(typed_);
    };
    connect(&tm(), &theme::ThemeManager::changed, this, restyle);
    restyle();
    retranslate();
}

Autocomplete::~Autocomplete() = default;

void Autocomplete::setExtraCommands(const QList<Extra>& commands)
{
    extras_ = commands;
    index_.clear();
    if (live_)
        refresh(typed_);
}

void Autocomplete::setLockQuery(std::function<Edition(const QString&)> query)
{
    lock_ = std::move(query);
    list_->viewport()->update();
}

const std::vector<Autocomplete::Entry>& Autocomplete::index() const
{
    if (!index_.empty())
        return index_;
    QHash<QLatin1StringView, const actions::SearchTerms*> terms;
    for (const actions::SearchTerms& t : actions::searchTerms())
        terms.insert(QLatin1StringView(t.name), &t);
    QSet<QString> names;
    for (const actions::CommandInfo& c : actions::commands()) {
        const actions::SearchTerms* t = terms.value(QLatin1StringView(c.name));
        if (!t)
            continue; // lệnh nội bộ (HIDDEN của prototype) không gợi ý
        const QString label = QCoreApplication::translate("actions", c.label);
        index_.push_back({Entry::Command, QString::fromLatin1(c.name), QString::fromLatin1(t->aliases),
                          QString::fromLatin1(t->keywords), label, text::foldForSearch(label),
                          QString::fromLatin1(c.icon)});
        names.insert(index_.back().name);
    }
    const QString lisp = tr("Lệnh LISP hoặc plugin");
    for (const Extra& x : extras_) {
        const QString name = x.name.trimmed().toUpper();
        if (name.isEmpty() || names.contains(name))
            continue;
        names.insert(name);
        index_.push_back({Entry::Extra,
                          name,
                          {},
                          {},
                          x.description.isEmpty() ? lisp : x.description,
                          x.description.isEmpty() ? QString() : text::foldForSearch(x.description),
                          u"lisp"_s});
    }
    const QString sysvar = tr("Biến hệ thống");
    for (const QString& name : QString::fromLatin1(kSysvars).split(u' ', Qt::SkipEmptyParts))
        if (!names.contains(name))
            index_.push_back({Entry::Sysvar, name, {}, {}, sysvar, {}, u"gear"_s});
    return index_;
}

int Autocomplete::nameCount() const
{
    int n = 0;
    for (const Entry& e : index())
        n += 1 + int(e.aliases.count(u',')) - (e.aliases.isEmpty() ? 0 : 1);
    return n;
}

Autocomplete::Ranked Autocomplete::rank(const QString& typed) const
{
    const auto& all = index();
    const Query q(typed);
    struct Hit {
        int score, uses, at, tier;
        auto key() const { return std::tuple(-score, -uses, at); }
    };
    std::vector<Hit> hits;
    for (int i = 0; i < int(all.size()); ++i)
        if (const int tier = q.score(all[std::size_t(i)])) {
            const int uses = q.uses(all[std::size_t(i)].name);
            hits.push_back({tier + std::min(kMaxBonus, kUseBonus * uses), uses, i, tier});
        }
    const auto before = [](const Hit& a, const Hit& b) { return a.key() < b.key(); };
    const auto n = std::min(hits.size(), std::size_t(kMaxRows));
    std::partial_sort(hits.begin(), hits.begin() + std::ptrdiff_t(n), hits.end(), before);
    // Dòng trùng chữ gõ luôn có mặt (là dòng chọn sẵn), kể cả khi các dòng dùng nhiều hơn đẩy nó ra.
    const auto exact = [](const Hit& h) { return h.tier == kExact; };
    if (n && std::none_of(hits.begin(), hits.begin() + std::ptrdiff_t(n), exact)) {
        auto best = hits.end();
        for (auto it = hits.begin() + std::ptrdiff_t(n); it != hits.end(); ++it)
            if (exact(*it) && (best == hits.end() || before(*it, *best)))
                best = it;
        if (best != hits.end())
            hits[n - 1] = *best;
    }
    Ranked r;
    for (std::size_t k = 0; k < n; ++k) {
        r.rows.push_back(hits[k].at);
        if (r.exact < 0 && exact(hits[k]))
            r.exact = int(k);
    }
    r.current = r.exact;
    for (std::size_t k = 0; k < n && r.current < 0; ++k)
        if (hits[k].tier >= kStrong)
            r.current = int(k);
    if (r.rows.empty()) {
        r.rows = nearest(q, nullptr);
        r.typo = !r.rows.empty();
    }
    return r;
}

std::vector<int> Autocomplete::nearest(const Query& q, QStringList* terms) const
{
    std::vector<int> out;
    if (q.upper.size() < 2)
        return out;
    const int max = q.upper.size() > 4 ? 2 : 1; // như lev của prototype
    struct Near {
        int d, uses, at;
        QStringView term;
    };
    std::vector<Near> near;
    const auto& all = index();
    for (int i = 0; i < int(all.size()); ++i) {
        const Entry& e = all[std::size_t(i)];
        if (e.kind == Entry::Sysvar && !q.sysvars)
            continue;
        Near best{max + 1, 0, i, {}};
        const auto consider = [&](QStringView term) {
            if (const int d = distance(q.upper, term, max); d < best.d)
                best = {d, 0, i, term};
        };
        consider(e.name);
        for (const QStringView a : qTokenize(e.aliases, u',', Qt::SkipEmptyParts))
            consider(a);
        if (best.d <= max) {
            best.uses = q.uses(e.name);
            near.push_back(best);
        }
    }
    std::ranges::sort(near, [](const Near& a, const Near& b) {
        return std::tuple(a.d, -a.uses, a.at) < std::tuple(b.d, -b.uses, b.at);
    });
    for (const Near& n : near) {
        if (int(out.size()) == kMaxSuggestions)
            break;
        out.push_back(n.at);
        if (terms)
            *terms << n.term.toString();
    }
    return out;
}

QStringList Autocomplete::search(const QString& typed) const
{
    QStringList out;
    if (!skipped(typed))
        for (const int at : rank(typed).rows)
            out << index()[std::size_t(at)].name;
    return out;
}

QString Autocomplete::completion(const QString& typed) const
{
    if (skipped(typed))
        return {};
    const Ranked r = rank(typed);
    if (r.current < 0)
        return {};
    return r.current == r.exact ? typed.trimmed() : index()[std::size_t(r.rows[std::size_t(r.current)])].name;
}

QStringList Autocomplete::suggest(const QString& typed) const
{
    QStringList terms;
    nearest(Query(typed), &terms);
    return terms;
}

QString Autocomplete::resolve(const QString& name) const
{
    QStringView n = QStringView(name).trimmed();
    while (!n.isEmpty() && QStringView(u"'_.-").contains(n.front())) // 'ZOOM, _LINE, .LINE, -LAYER
        n = n.sliced(1);
    const QString upper = n.toString().toUpper();
    if (upper.isEmpty())
        return {};
    const QString alias = u',' + upper + u',';
    for (const Entry& e : index())
        if (e.name == upper || e.aliases.contains(alias))
            return e.name;
    return {};
}

QString Autocomplete::correction(const QString& typo) const
{
    return learnedFor(typo.trimmed().toUpper());
}

void Autocomplete::recordUse(const QString& name)
{
    const QString n = resolve(name);
    if (n.isEmpty())
        return;
    settings::Store& s = settings::Store::instance();
    QJsonObject usage = s.get(kUsage);
    usage[n] = usage.value(n).toInt() + 1;
    s.set(kUsage, usage);
}

void Autocomplete::recordPick(const QString& typo, const QString& command)
{
    const QString key = typo.trimmed().toUpper();
    if (key.isEmpty() || command.isEmpty())
        return;
    settings::Store& s = settings::Store::instance();
    QJsonObject all = s.get(kAutoCorrect), picks = all.value(key).toObject();
    const int n = picks.value(command).toInt() + 1;
    picks[command] = n;
    all[key] = picks;
    s.set(kAutoCorrect, all);
    if (n == kLearnAfter)
        emit learned(key, command);
}

QList<Autocomplete::Correction> Autocomplete::corrections()
{
    QList<Correction> out;
    for (const QString& typo : settings::Store::instance().get(kAutoCorrect).keys())
        if (const QString command = learnedFor(typo); !command.isEmpty())
            out.append({typo, command});
    return out;
}

void Autocomplete::removeCorrection(const QString& typo)
{
    settings::Store& s = settings::Store::instance();
    QJsonObject all = s.get(kAutoCorrect);
    all.remove(typo.trimmed().toUpper()); // xóa cả số lần chọn: học lại từ đầu
    s.set(kAutoCorrect, all);
}

void Autocomplete::refresh(const QString& typed)
{
    live_ = true;
    typed_ = typed;
    Ranked r = skipped(typed) ? Ranked{} : rank(typed);
    if (r.rows.empty()) {
        hide();
        return;
    }
    const QString t = typed.trimmed();
    rows_ = std::move(r.rows);
    exact_ = r.exact;
    typo_ = r.typo ? t : QString();
    model_->reset();
    QString hint = tr("↑↓ chọn · Enter hoặc Tab để chạy"); // .af
    if (r.typo)
        hint = tr("Không có lệnh “%1”. Gợi ý gần nhất ở trên: ↓ chọn rồi Enter, hoặc Tab nhận gợi ý đầu.").arg(t);
    else if (settings::Store::instance().get(kSearchVietnamese))
        hint = tr("↑↓ chọn · Enter hoặc Tab để chạy · tìm cả theo tiếng Việt: “cắt”, “bo góc”, “in”");
    footer_->setText(hint);
    place();
    show();
    raise();
    select(r.current);
    probe::untilFrame(probe::Op::Autocomplete, this); // DoD-L: danh sách cập nhật sau mỗi phím
}

void Autocomplete::place()
{
    QWidget* window = anchor_->window();
    if (parentWidget() != window && window != anchor_)
        setParent(window); // như .ac của prototype: nằm trên vùng vẽ, trong cửa sổ
    const QFontMetrics fn(theme::font(TextRole::Mono, Weight::Medium)), fa(theme::font(TextRole::MonoCaption)),
        fd(theme::font(TextRole::Label));
    const int gap = px(tok::sp_4), pad = px(tok::sp_2), inset = kBorder + pad;
    nameW_ = kNameW;
    aliasW_ = kAliasW;
    int descW = 0;
    for (const int at : rows_) {
        const Entry& e = index()[std::size_t(at)];
        nameW_ = std::max(nameW_, fn.horizontalAdvance(e.name));
        aliasW_ = std::max(aliasW_, fa.horizontalAdvance(e.aliasText()));
        const QSize badge = RichTooltip::badgeSize(model_->need(e));
        descW = std::max(descW, fd.horizontalAdvance(e.label) + (badge.isEmpty() ? 0 : px(tok::sp_3) + badge.width()));
    }
    rowW_ = gap + kIconW + gap + nameW_ + gap + aliasW_ + gap + descW + gap;

    // Rộng: 520 px hoặc vừa dòng dài nhất (thêm thanh cuộn nếu phải cuộn), không quá cửa sổ. Cao: các dòng, chú thích
    // .af (lề trên --sp-2, viền 1 px), tối đa 300 px rồi cuộn.
    const QPoint origin = anchor_->mapTo(window, QPoint(px(tok::sp_3), 0));
    const int room = std::max(kWidth, window->width() - origin.x() - px(tok::sp_3));
    const int bar = list_->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, list_);
    const int rows = int(rows_.size());
    int width = 0, footH = 0, shown = 0;
    for (const int extra : {0, bar}) {
        width = std::min(room, std::max(kWidth, rowW_ + extra + 2 * inset));
        footH = footer_->heightForWidth(width - 2 * inset);
        const int fit = std::max(1, (kMaxHeight - 2 * inset - pad - kBorder - footH) / kRowH);
        shown = std::min(rows, fit);
        if (rows <= fit)
            break;
    }
    const int listH = shown * kRowH;
    const int height = 2 * inset + listH + pad + kBorder + footH;
    const int bottom = origin.y() - px(tok::sp_1);
    setGeometry(origin.x(), std::max(0, bottom - height), width, height);
    list_->setGeometry(inset, inset, width - 2 * inset, listH);
    footer_->setGeometry(inset, inset + listH + pad + kBorder, width - 2 * inset, footH);
}

void Autocomplete::select(int row)
{
    current_ = row;
    QItemSelectionModel* sel = list_->selectionModel();
    if (row < 0) {
        sel->clear();
    } else {
        const QModelIndex i = model_->index(row);
        sel->setCurrentIndex(i, QItemSelectionModel::ClearAndSelect);
        list_->scrollTo(i);
    }
    list_->viewport()->update();
    // Focus ở ô nhập nên danh sách không tự báo dòng đang chọn: báo cho trình đọc màn hình khi dòng đổi.
    const QString say = row < 0 ? QString() : model_->index(row).data(Qt::AccessibleTextRole).toString();
    if (say != said_) {
        said_ = say;
        a11y::announce(list_, say);
    }
}

void Autocomplete::step(int delta)
{
    const int n = int(rows_.size());
    if (n && isVisible())
        select(current_ < 0 ? (delta > 0 ? 0 : n - 1) : (current_ + delta % n + n) % n);
}

bool Autocomplete::accept(bool orFirst)
{
    const int row = current_ >= 0 ? current_ : orFirst ? 0 : -1;
    if (!isVisible() || row < 0 || row >= int(rows_.size()))
        return false;
    // Dòng trùng chữ gõ: gửi đúng chữ đã gõ ("C" chứ không phải CIRCLE), như chạy cùng chuỗi bằng .scr.
    const QString name = row == exact_ ? typed_.trimmed() : index()[std::size_t(rows_[std::size_t(row)])].name;
    const QString typo = typo_;
    dismiss();
    emit accepted(name);
    if (!typo.isEmpty())
        recordPick(typo, name);
    return true;
}

void Autocomplete::dismiss()
{
    live_ = false;
    hide();
}

QAbstractItemView* Autocomplete::view() const
{
    return list_;
}

void Autocomplete::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    // .ac: nền --c-raised, viền --c-border-ctl, bo --r-lg; .af: viền trên --c-border.
    const qreal radius = tm().px(tok::r_lg);
    p.setPen(QPen(col(tok::c_border_ctl), kBorder));
    p.setBrush(col(tok::c_raised));
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    const QRect f = footer_->geometry();
    p.fillRect(QRect(f.left(), f.top() - kBorder, f.width(), kBorder), col(tok::c_border));
}

void Autocomplete::retranslate()
{
    const QString name = tr("Gợi ý lệnh");
    setAccessibleName(name);
    list_->setAccessibleName(name);
}

void Autocomplete::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange) {
        retranslate();
        index_.clear(); // nhãn của lệnh, mô tả chung dịch lại
        if (live_)
            refresh(typed_);
    }
    QWidget::changeEvent(e);
}

} // namespace trion::ui
