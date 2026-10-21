// UIA-106 · Phần thuần của ImeBypass: commit NFC (kể cả tổ hợp NFD gửi tách lượt); đối chiếu phím vật lý với MSG khi
// bộ gõ kiểu hook (Unikey, EVKey, OpenKey) nuốt phím rồi chèn Backspace + VK_PACKET; 10.000 chuỗi phím ngẫu nhiên ở
// 30 phím/s (thời gian giả lập): không mất, không lặp ký tự.
#include "app/core/ime/ImeBypass.h"

#include <QTest>
#include <QWidget>

#include <random>
#include <vector>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif

using namespace Qt::StringLiterals;
using namespace trion::ui::ime;
using namespace trion::ui::ime::win;

namespace {

// lParam của WM_KEYDOWN: mã quét ở bit 16–23, số lần lặp 1.
std::intptr_t lp(std::uint32_t scan)
{
    return std::intptr_t(scan & 0xFF) << 16 | 1;
}

// Một sự kiện phím theo thứ tự hệ thống phát (hook thấy hết), kèm việc nó có tới luồng UI không.
struct Ev {
    HookKey hook;
    bool delivered = true; // bộ gõ không nuốt
    char16_t ch = 0;       // ký tự TranslateMessage tạo từ MSG nhấn (u'\b': Backspace xóa một ký tự)
};

Ev physical(char16_t c, bool up)
{
    const auto vk = std::uint32_t(c == u' ' ? 0x20 : QChar(c).toUpper().unicode());
    return {{vk, 0x10 + vk % 0x40, up ? kUp : 0u, up ? char16_t(0) : c}, true, up ? char16_t(0) : c};
}

// Ô nhận ký tự của một MSG nhấn; Backspace xóa một ký tự.
void put(QString& s, char16_t ch)
{
    if (ch == u'\b')
        s.chop(1);
    else
        s += QChar(ch);
}

// Bộ gõ chèn bằng SendInput: Backspace có mã quét 0, chữ qua VK_PACKET (mã quét = ký tự).
void inject(std::vector<Ev>& out, int backspaces, const QString& text)
{
    for (int i = 0; i < backspaces; ++i) {
        out.push_back({{kVkBack, 0, kInjected, 0}, true, u'\b'});
        out.push_back({{kVkBack, 0, kInjected | kUp, 0}, true, 0});
    }
    for (const QChar c : text) {
        out.push_back({{kVkPacket, c.unicode(), kInjected, 0}, true, c.unicode()});
        out.push_back({{kVkPacket, c.unicode(), kInjected | kUp, 0}, true, 0});
    }
}

// Cho hook thấy toàn bộ sự kiện rồi giao MSG theo thứ tự (cách đơn giản nhất: hook của ta luôn chạy trước MSG của nó).
// Trả về chuỗi ô nhận được: ký tự giao lại + ký tự của MSG không bị bỏ.
QString run(const std::vector<Ev>& evs, KeyReconciler& rec, bool* rehook = nullptr)
{
    for (const Ev& e : evs)
        rec.onHook(e.hook);
    QString out;
    for (const Ev& e : evs) {
        if (!e.delivered)
            continue;
        const bool up = e.hook.flags & kUp;
        const Verdict v = rec.onMessage(up ? kKeyUp : kKeyDown, e.hook.vk, lp(e.hook.scan));
        out += v.replay;
        if (rehook)
            *rehook = *rehook || v.rehook;
        if (!v.drop && e.ch)
            put(out, e.ch);
    }
    return out;
}

// Gõ `first` rồi `second`: bộ gõ để `first` qua, nuốt `second` rồi chèn `bs` Backspace và `text`.
std::vector<Ev> transform(char16_t first, char16_t second, int bs, const QString& text)
{
    std::vector<Ev> evs{physical(first, false), physical(first, true), physical(second, false)};
    evs.back().delivered = false;
    inject(evs, bs, text);
    evs.push_back(physical(second, true));
    return evs;
}

} // namespace

class ImeBypassTest : public QObject {
    Q_OBJECT

private slots:
    void nfcCommit_data()
    {
        QTest::addColumn<QString>("commit");
        QTest::addColumn<int>("start");
        QTest::addColumn<int>("length");
        QTest::addColumn<QString>("before");
        QTest::addColumn<QString>("text");
        QTest::addColumn<int>("outStart");
        QTest::addColumn<int>("outLength");
        // i18n-ok: chữ mẫu tiếng Việt của test, không phải chuỗi giao diện
        QTest::newRow("nfc") << u"ấ"_s << 0 << 0 << u""_s << u"ấ"_s << 0 << 0;
        QTest::newRow("nfd") << u"ấ"_s << 0 << 0 << u"x"_s << u"ấ"_s << 0 << 0;
        QTest::newRow("dấu rời") << u"̂"_s << 0 << 0 << u"ta"_s << u"â"_s << -1 << 1;
        QTest::newRow("hai dấu rời") << u"̂́"_s << 0 << 0 << u"a"_s << u"ấ"_s << -1 << 1;
        QTest::newRow("dấu chồng") << u"́"_s << 0 << 0 << u"â"_s << u"ấ"_s << -2 << 2;
        QTest::newRow("có phần thay") << u"́"_s << -1 << 1 << u"ab"_s << u"á"_s << -2 << 2;
        QTest::newRow("đầu ô") << u"̂"_s << 0 << 0 << u""_s << u"̂"_s << 0 << 0;
        QTest::newRow("đ") << u"đ"_s << -1 << 1 << u"d"_s << u"đ"_s << -1 << 1;
    }
    void nfcCommit()
    {
        QFETCH(QString, commit);
        QFETCH(int, start);
        QFETCH(int, length);
        QFETCH(QString, before);
        const Commit c = trion::ui::ime::nfcCommit(commit, start, length, before);
        QTEST(c.text, "text");
        QTEST(c.start, "outStart");
        QTEST(c.length, "outLength");
    }

    // Không có bộ gõ: mọi MSG qua, không giao lại gì.
    void plainKeysPass()
    {
        std::vector<Ev> evs;
        for (const char16_t c : u"line 3")
            if (c) {
                evs.push_back(physical(c, false));
                evs.push_back(physical(c, true));
            }
        KeyReconciler rec;
        bool rehook = false;
        QCOMPARE(run(evs, rec, &rehook), u"line 3"_s);
        QVERIFY(!rehook);
    }

    // Tiêu chí 2 (Windows, bộ gõ kiểu hook): bộ gõ nuốt phím thứ hai và chèn chữ có dấu; ô nhận đúng phím đã gõ.
    void hookImeSwallows_data()
    {
        QTest::addColumn<QString>("keys");
        QTest::addColumn<int>("bs");
        QTest::addColumn<QString>("ime");
        // i18n-ok: chữ bộ gõ chèn, không phải chuỗi giao diện
        QTest::newRow("dd") << u"dd"_s << 1 << u"đ"_s;
        QTest::newRow("aa") << u"aa"_s << 1 << u"â"_s;
        QTest::newRow("ow") << u"ow"_s << 1 << u"ơ"_s;
        QTest::newRow("lm (gõ tắt)") << u"lm"_s << 1 << u"lầm"_s; // i18n-ok: chữ bộ gõ chèn
    }
    void hookImeSwallows()
    {
        QFETCH(QString, keys);
        QFETCH(int, bs);
        QFETCH(QString, ime);
        const auto evs = transform(keys[0].unicode(), keys[1].unicode(), bs, ime);
        KeyReconciler rec;
        bool rehook = false;
        QCOMPARE(run(evs, rec, &rehook), keys);
        QVERIFY(!rehook);
        // Không có ImeBypass thì ô nhận đúng chữ bộ gõ tạo ra (kiểm mô hình sự kiện của test).
        QString plain;
        for (const Ev& e : evs)
            if (e.delivered && e.ch && !(e.hook.flags & kUp))
                put(plain, e.ch);
        QCOMPARE(plain, ime);
    }

    // Hook của bộ gõ đứng trước hook của ta (bộ gõ khởi động lại sau khi ta đặt hook): ta không thấy phím bị nuốt nên
    // mất phím đó một lần (chữ của bộ gõ vẫn bị bỏ) và yêu cầu đặt lại hook để từ phím sau hook của ta đứng trước.
    void imeHookAheadRehooks()
    {
        const auto evs = transform(u'a', u'a', 1, u"â"_s); // i18n-ok: chữ bộ gõ chèn
        KeyReconciler rec;
        for (const Ev& e : evs)
            if (e.delivered) // phím bị nuốt không tới hook của ta
                rec.onHook(e.hook);
        QString out;
        bool rehook = false;
        for (const Ev& e : evs) {
            if (!e.delivered)
                continue;
            const Verdict v = rec.onMessage(e.hook.flags & kUp ? kKeyUp : kKeyDown, e.hook.vk, lp(e.hook.scan));
            QVERIFY(v.replay.isEmpty());
            QCOMPARE(v.drop, (e.hook.flags & kInjected) != 0);
            rehook = rehook || v.rehook;
            if (!v.drop && e.ch)
                put(out, e.ch);
        }
        QVERIFY(rehook);
        QCOMPARE(out, u"a"_s);
    }

    // Bộ gõ không nuốt phím mà chèn thêm (gõ tắt khi nhấn Space): phần chèn bị bỏ, ô giữ đúng phím đã gõ.
    void injectWithoutSwallow()
    {
        std::vector<Ev> evs{physical(u'l', false), physical(u'l', true), physical(u'm', false), physical(u'm', true),
                            physical(u' ', false)};
        inject(evs, 3, u"làm "_s); // i18n-ok: chữ bộ gõ chèn
        evs.push_back(physical(u' ', true));
        KeyReconciler rec;
        QCOMPARE(run(evs, rec), u"lm "_s);
    }

    // MSG giả, hook chưa ghi gì (không đặt được): không bỏ gì, bộ gõ chạy như thường; VK_PACKET, mã quét 0 thì đặt lại
    // hook.
    void messagesWithoutHook()
    {
        KeyReconciler rec;
        Verdict v = rec.onMessage(kKeyDown, kVkPacket, lp(0));
        QVERIFY(!v.drop && v.rehook && v.replay.isEmpty());
        v = rec.onMessage(kKeyDown, kVkBack, lp(0));
        QVERIFY(!v.drop && v.rehook);
        v = rec.onMessage(kKeyDown, 'L', lp(0x26));
        QVERIFY(!v.drop && !v.rehook);
        v = rec.onMessage(0x0102 /* WM_CHAR */, 'l', lp(0x26));
        QVERIFY(!v.drop && !v.rehook && v.replay.isEmpty());
    }

    // Sau một phím bị nuốt, MSG chèn mà hook không ghi (nhật ký tràn) vẫn nhận ra theo VK_PACKET, mã quét 0.
    void injectedAfterSwallowWithoutLog()
    {
        KeyReconciler rec;
        rec.onHook(physical(u'd', false).hook); // bị nuốt
        rec.onHook({kVkBack, 0, kInjected, 0});
        Verdict v = rec.onMessage(kKeyDown, kVkBack, lp(0));
        QCOMPARE(v.replay, u"d"_s);
        QVERIFY(v.drop);
        v = rec.onMessage(kKeyDown, kVkPacket, lp(0x11)); // không có trong nhật ký
        QVERIFY(v.drop && !v.rehook);
        v = rec.onMessage(kKeyDown, 'D', lp(physical(u'd', false).hook.scan)); // phím thật tiếp theo
        QVERIFY(!v.drop && v.replay.isEmpty());
    }

    // Công cụ điều khiển từ xa (TeamViewer, UltraViewer) chèn phím có mã quét thật: không bỏ, không đặt lại hook.
    void remoteInjectedKeysPass()
    {
        std::vector<Ev> evs;
        for (const char16_t c : u"pl") {
            if (!c)
                continue;
            Ev d = physical(c, false), u = physical(c, true);
            d.hook.flags |= kInjected;
            u.hook.flags |= kInjected;
            evs.push_back(d);
            evs.push_back(u);
        }
        KeyReconciler rec;
        bool rehook = false;
        QCOMPARE(run(evs, rec, &rehook), u"pl"_s);
        QVERIFY(!rehook);
    }

    // Alt+Tab, Win+D: hook thấy phím mà MSG không tới; phím kèm Alt/Win không có ký tự nên không bị giao lại.
    void systemKeysNotReplayed()
    {
        KeyReconciler rec;
        rec.onHook({0x12 /* VK_MENU */, 0x38, 0x20 /* LLKHF_ALTDOWN */, 0});
        rec.onHook({0x09 /* VK_TAB */, 0x0F, 0x20, 0});
        rec.onHook(physical(u'l', false).hook);
        const Verdict v = rec.onMessage(kKeyDown, 'L', lp(physical(u'l', false).hook.scan));
        QVERIFY(!v.drop && v.replay.isEmpty());
    }

    // Nhật ký giới hạn: hook thấy rất nhiều phím mà MSG không tới thì chỉ giữ phần mới nhất.
    void logBounded()
    {
        KeyReconciler rec;
        for (int i = 0; i < 1000; ++i)
            rec.onHook(physical(u'x', false).hook);
        rec.onHook({kVkBack, 0, kInjected, 0});
        const Verdict v = rec.onMessage(kKeyDown, kVkBack, lp(0));
        QVERIFY(v.drop);
        QCOMPARE(v.replay.size(), 63);
        rec.reset();
        QVERIFY(rec.onMessage(kKeyDown, kVkBack, lp(0)).rehook); // reset() xóa trạng thái "vừa nuốt"
    }

    // setHookTarget: bật, đổi ô, tắt hai lần. Windows: đặt rồi gỡ hook thật, MSG phím của luồng vẫn qua bộ lọc (hook
    // chưa ghi gì nên không bỏ). Nền tảng khác: không làm gì.
    void setHookTargetOnOff()
    {
        QWidget a, b;
        setHookTarget(&a);
        setHookTarget(&b);
#ifdef Q_OS_WIN
        QVERIFY(PostThreadMessageW(GetCurrentThreadId(), WM_KEYDOWN, VK_PACKET, 0));
        QCoreApplication::processEvents();
#endif
        setHookTarget(nullptr);
        setHookTarget(nullptr);
    }

    // Tiêu chí 3: 10.000 chuỗi phím ngẫu nhiên, 30 phím/s (thời gian giả lập), giữ phím 40–120 ms nên các phím chồng
    // nhau; bộ gõ kiểu hook biến đổi theo luật Telex, chữ nó chèn xếp sau 0–200 ms phím vật lý đã tới (luồng UI trễ);
    // hook và MSG xen kẽ ngẫu nhiên. Ô phải nhận đúng chuỗi phím đã gõ: không mất, không lặp.
    void randomTyping()
    {
        std::mt19937 rng(20261021);
        const QString keys = u"adeowsfrjxlmnu iy0123"_s;
        qint64 typed = 0;
        for (int round = 0; round < 10'000; ++round) {
            const int n = std::uniform_int_distribution<int>(1, 24)(rng);
            // Phím vật lý theo thời gian: nhấn mỗi 33 ms (± 8), nhả sau 40–120 ms.
            struct Timed {
                int at;
                Ev ev;
            };
            std::vector<Timed> phys;
            QString expect;
            for (int i = 0; i < n; ++i) {
                const char16_t c = keys[std::uniform_int_distribution<int>(0, int(keys.size()) - 1)(rng)].unicode();
                const int at = i * 33 + std::uniform_int_distribution<int>(-8, 8)(rng);
                Ev down = physical(c, false), up = physical(c, true);
                if (std::uniform_int_distribution<int>(0, 19)(rng) == 0) { // phím chèn từ xa: bộ gõ bỏ qua
                    down.hook.flags |= kInjected;
                    up.hook.flags |= kInjected;
                }
                phys.push_back({at, down});
                phys.push_back({at + std::uniform_int_distribution<int>(40, 120)(rng), up});
                expect += QChar(c);
            }
            std::stable_sort(phys.begin(), phys.end(), [](const Timed& a, const Timed& b) { return a.at < b.at; });
            std::deque<Ev> pending;
            for (const Timed& t : phys)
                pending.push_back(t.ev);
            // Hệ thống xử lý lần lượt; bộ gõ biến đổi thì nuốt phím và SendInput chèn vào sau `lag` sự kiện đang chờ.
            std::vector<Ev> rit;
            char16_t prev = 0;
            while (!pending.empty()) {
                Ev e = pending.front();
                pending.pop_front();
                const bool userDown =
                    !(e.hook.flags & (kUp | kInjected)) && e.hook.vk != kVkBack && e.hook.vk != kVkPacket;
                QString out;
                if (userDown) {
                    const char16_t c = e.ch;
                    if (c == prev && QStringView(u"adeo").contains(QChar(c)))
                        out = c == u'd' ? u"đ"_s : QString(QChar(c)) + QChar(0x0302); // i18n-ok: chữ bộ gõ chèn
                    else if (c == u'w' && QStringView(u"aou").contains(QChar(prev)))
                        out = QString(QChar(prev)) + QChar(prev == u'a' ? 0x0306 : 0x031B);
                    else if (QStringView(u"sfrxj").contains(QChar(c)) && QStringView(u"aeiouy").contains(QChar(prev)))
                        out = QString(QChar(prev)) + QChar(0x0301);
                    else if (c == u'm' && prev == u'l')
                        out = u"lầm"_s; // i18n-ok: gõ tắt của bộ gõ
                    prev = out.isEmpty() ? c : 0;
                }
                if (out.isEmpty()) {
                    rit.push_back(e);
                    continue;
                }
                e.delivered = false;
                rit.push_back(e);
                std::vector<Ev> inj;
                inject(inj, 1, out.normalized(QString::NormalizationForm_C));
                const auto lag =
                    std::min<std::size_t>(std::uniform_int_distribution<std::size_t>(0, 12)(rng), pending.size());
                pending.insert(pending.begin() + std::ptrdiff_t(lag), inj.begin(), inj.end());
            }
            // Hook của sự kiện luôn chạy trước MSG của nó; ngoài ra xen kẽ ngẫu nhiên (luồng UI lấy tin theo đợt).
            KeyReconciler rec;
            QString got;
            std::size_t h = 0;
            for (std::size_t m = 0; m < rit.size(); ++m) {
                if (!rit[m].delivered)
                    continue;
                while (h <= m || (h < rit.size() && std::uniform_int_distribution<int>(0, 2)(rng) == 0))
                    rec.onHook(rit[h++].hook);
                const Ev& e = rit[m];
                const Verdict v = rec.onMessage(e.hook.flags & kUp ? kKeyUp : kKeyDown, e.hook.vk, lp(e.hook.scan));
                got += v.replay;
                if (!v.drop && e.ch)
                    put(got, e.ch);
            }
            QVERIFY2(got == expect, qPrintable(u"vòng %1: gõ '%2', ô nhận '%3'"_s.arg(round).arg(expect, got)));
            typed += n;
        }
        qInfo("10.000 chuỗi, %lld phím: không mất, không lặp", typed);
    }
};

QTEST_MAIN(ImeBypassTest)
#include "ime_bypass_test.moc"
