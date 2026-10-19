/* TrionCAD prototype · Đợt 4: chú thích và bảng (D04 kiểu bảng, F02–F04 MText, F06 chèn bảng, F07 tab Ô bảng, F08 field, F11 tỉ lệ chú thích, F13 leader nhanh, F14 dung sai hình học). */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic, G = TC.geo, IMPL = TC.cmd.impl, ED = TC.ed, SC = TC.screens, H = SC.h;
  function A(){ return TC.app; } function V(){ return TC.app.view; } function D(){ return TC.app.doc(); }
  var OPEN = SC.open4 = SC.open4 || {}; // mã màn hình → hàm mở (danh mục gọi khi bấm); tiện ích mở ở SC.cat (D1, PRO, MOD, RUN, plan, pick, flash)
  function clone(o){ return JSON.parse(JSON.stringify(o)); }
  function num(v, d){ var n = TC.parseNum(v, 'none'); return n.err ? d : n.v; }
  function ctip(msg, info){ var s = (V() && V().mouse) || [120, 120]; TC.ctip($('#cv'), s[0], s[1], msg, info); }
  function allEnts(d){ var l = d.ents.slice(); d.layouts.forEach(function(L){ l = l.concat(L.ents); }); return l; }
  // điểm màn hình ↔ tọa độ của không gian chứa đối tượng (Model qua viewport đang kích hoạt, hoặc giấy)
  function spt(p, e){ var v = V(); return e && e._paper ? v.toS(p) : v.mToS(p); }
  function wpt(m, e){ var v = V(); return e && e._paper ? v.toW(m[0], m[1]) : v.sToM(m[0], m[1]); }
  function scaleOf(e){ var v = V(), mv = v.modelView(); return (e && e._paper) || !mv ? v.vw().s : mv.v.s; }
  function local(ev){ var r = $('#cv').getBoundingClientRect(); return [ev.clientX - r.left, ev.clientY - r.top]; }
  // bề rộng chữ theo đơn vị bản vẽ, đo bằng font hiển thị (khớp View.textFont)
  var MC = null;
  function tw(d, s, h, style, bold){ MC = MC || document.createElement('canvas').getContext('2d'); var st = d.styleMap[style] || {}; MC.font = ((st.bold || bold) ? '600 ' : '') + '100px ' + (st.css || 'Arial, "Helvetica Neue", sans-serif'); return MC.measureText(String(s)).width / 100 * h * 1.32; }
  // ô màu dùng trong hộp thoại: nút thả xuống có mẫu màu
  function swHex(c){ return c == null ? 'transparent' : c === 7 ? '#FFFFFF' : TC.ACI[c] || '#FFFFFF'; }
  function colLbl(c, none){ return c == null ? (none || 'ByBlock') : TC.aciName(c); }
  function colorBtn(id, c, none){ return '<button class="dd" type="button" id="' + id + '" data-none="' + esc(none || '') + '" style="width:150px"><span class="lbl"><i class="sw" style="background:' + swHex(c) + '"></i>' + esc(colLbl(c, none)) + '</span>' + ic('down', 'xs') + '</button>'; }
  function colorMenu(b, cur, none, set){ var r = b.getBoundingClientRect(); TC.menu([{ label:esc(none || 'ByBlock'), check:cur == null, act:function(){ set(null); } }, { sep:1 }].concat([1, 2, 3, 4, 5, 6, 7, 8, 9, 251, 253, 254].map(function(n){ return { label:'<i class="sw" style="background:' + swHex(n) + '"></i>' + TC.aciName(n), check:cur === n, act:function(){ set(n); } }; })).concat([{ sep:1 }, { label:'Chọn màu khác…', act:function(){ SC.colorPicker(cur).then(function(c){ if (c !== undefined && typeof c === 'number') set(c); }); } }]), r.left, r.bottom + 2, { minWidth:r.width, returnFocus:b }); }

  /* ================= Kiểu bảng: dữ liệu (UI-D04) ================= */
  var CSN = { title:'Tiêu đề', header:'Đầu cột', data:'Dữ liệu' };
  var ALN = [['TL', 'Trên trái'], ['TC', 'Trên giữa'], ['TR', 'Trên phải'], ['ML', 'Giữa trái'], ['MC', 'Giữa tâm'], ['MR', 'Giữa phải'], ['BL', 'Dưới trái'], ['BC', 'Dưới giữa'], ['BR', 'Dưới phải']];
  var DTS = ['Chung', 'Số thập phân', 'Số nguyên', 'Phần trăm', 'Tiền tệ', 'Ngày', 'Góc', 'Điểm', 'Chữ'];
  var LWS = [0.13, 0.18, 0.25, 0.35, 0.5, 0.7];
  function bdr(lw){ return { on:lw > 0, lw:lw || .25, c:null, lt:'ByBlock', dbl:false, sp:.5 }; }
  // kiểu ô: nền, canh lề, định dạng, lề (mm trên giấy), chữ, viền 6 cạnh (trên, dưới, trái, phải, trong ngang, trong dọc)
  function cst(o, bd){ var c = Object.assign({ bg:null, al:'MC', dt:'Chung', prec:2, kind:'Nhãn', mh:1.5, mv:1, merge:false, ts:'Standard', th:2.5, tc:null, ang:0, b:false, bd:{} }, o || {}); ['t', 'b', 'l', 'r', 'h', 'v'].forEach(function(k){ c.bd[k] = bdr(bd && bd[k] != null ? bd[k] : .25); }); return c; }
  function defStyles(){
    var T = { t:.5, l:.5, r:.5, b:.5 };
    return {
      Standard:{ desc:'Kiểu mặc định của bản vẽ: chữ 2.5 mm, dữ liệu số canh phải, chữ canh trái.', dir:'down', cells:{ title:cst({ th:2.75, b:true, merge:true }), header:cst({ th:2.75, b:true }), data:cst({ al:'auto', kind:'Dữ liệu', mh:1.25 }) } },
      'TCVN-thong-ke-thep':{ tcvn:true, desc:'Bảng thống kê cốt thép, mẫu theo TCVN 5572: tiêu đề 3.5 mm không khung, chữ 2.5 mm, viền ngoài 0.5 mm, viền trong 0.25 mm.', dir:'down', cells:{ title:cst({ th:3.5, b:true, mv:1.5, merge:true }, { t:0, l:0, r:0, b:.5 }), header:cst({ th:2.5, b:true }, T), data:cst({ kind:'Dữ liệu' }, { l:.5, r:.5, b:.5 }) } },
      'TCVN-thong-ke':{ tcvn:true, desc:'Bảng thống kê chung (cửa, thiết bị, vật liệu): tiêu đề 3.5 mm không khung, chữ 2.5 mm, cột số canh phải, viền ngoài 0.5 mm, viền trong 0.25 mm.', dir:'down', cells:{ title:cst({ th:3.5, b:true, mv:1.5, merge:true }, { t:0, l:0, r:0, b:.5 }), header:cst({ th:2.5, b:true }, T), data:cst({ al:'auto', kind:'Dữ liệu' }, { l:.5, r:.5, b:.5 }) } },
      'TCVN-dien-tich':{ tcvn:true, desc:'Bảng thống kê diện tích phòng: tên phòng canh trái, diện tích canh phải, tiêu đề 3.5 mm, viền ngoài 0.5 mm.', dir:'down', cells:{ title:cst({ th:3.5, b:true, mv:1.5, merge:true }, { t:0, l:0, r:0, b:.5 }), header:cst({ th:2.5, b:true }, T), data:cst({ al:'auto', kind:'Dữ liệu' }, { l:.5, r:.5, b:.5 }) } }
    };
  }
  function tstyles(d){ return d.tableStyles || (d.tableStyles = defStyles()); }
  function tsOf(d, e){ var S = tstyles(d); return (e && e._ts) || S[e && e.tstyle] || S.Standard; }
  // tên kiểu bảng cho dropdown trên ribbon (ribbons.js) và kiểu hiện hành (ED.cur(d).bstyle)
  SC.tableStyleNames = function(d){ d = d || D(); return d ? Object.keys(tstyles(d)) : ['Standard']; };
  SC.curTableStyle = function(d){ d = d || D(); var n = d && ED.cur(d).bstyle; return n && tstyles(d)[n] ? n : 'Standard'; };
  SC.setTableStyle = function(d, n){ d = d || D(); if (d) ED.cur(d).bstyle = n; };
  function reprep(d){ allEnts(d).forEach(function(e){ if (e.t === 'table' || (e.fld && e.fld.length)) TC.prepEnt(d, e); }); V().draw(); }
  function annoK(d){ return d.space === 'model' || d.activeVp ? 100 : 1; } // 1:100 trong Model, 1:1 trên giấy
  function hkOf(ts, e){ return (e.th || 250) / ((ts.cells.data || {}).th || 2.5); }
  function rowH(cs, lines, hk){ return Math.round(hk * (cs.th * 1.5 * (lines || 1) + 2 * cs.mv)); }

  /* ================= Bảng: hình học, công thức, định dạng, vẽ ================= */
  function tgeo(d, e){
    var X = [e.p[0]], R = [], up = tsOf(d, e).dir === 'up', y = e.p[1], n = e.rows.length, hs = e._rh && e._rh.length === n ? e._rh : e.rows;
    e.cols.forEach(function(w){ X.push(X[X.length - 1] + w); });
    for (var k = 0; k < n; k++){ var i = up ? n - 1 - k : k; R[i] = [y, y - hs[i]]; y -= hs[i]; }
    return { X:X, R:R, nc:e.cols.length, nr:n, top:e.p[1], bot:y };
  }
  function mergeAt(e, r, c){ return (e.merge || []).filter(function(m){ var r1 = m[3] != null ? m[3] : m[0]; return r >= m[0] && r <= r1 && c >= m[1] && c <= m[2]; })[0]; }
  function span(e, r, c){ var m = mergeAt(e, r, c); return m ? { r0:m[0], c0:m[1], c1:m[2], r1:m[3] != null ? m[3] : m[0] } : { r0:r, c0:c, r1:r, c1:c }; }
  function rectOf(g, s){ var top = -Infinity, bot = Infinity; for (var i = s.r0; i <= s.r1; i++){ top = Math.max(top, g.R[i][0]); bot = Math.min(bot, g.R[i][1]); } return { x0:g.X[s.c0], x1:g.X[s.c1 + 1], top:top, bot:bot }; }
  function cellAtP(g, p){ var c = -1, r = -1, j, i; if (p[0] < g.X[0] || p[0] > g.X[g.nc]) return null; for (j = 0; j < g.nc; j++) if (p[0] <= g.X[j + 1]){ c = j; break; } for (i = 0; i < g.nr; i++) if (p[1] <= g.R[i][0] && p[1] >= g.R[i][1]){ r = i; break; } return r < 0 || c < 0 ? null : { r:r, c:c }; }
  function cfOf(e, r, c){ return e.cf && e.cf[r + ',' + c]; }
  function rowKey(e, r){ if (e.rs && e.rs[r]) return e.rs[r]; var h = e.head || 0; return r < h ? (h >= 2 && r === 0 ? 'title' : 'header') : 'data'; }
  // kiểu ô thực của một ô: kiểu ô theo hàng (hoặc gán riêng) + định dạng ghi đè của ô
  function csOf(d, e, r, c, ts){ ts = ts || tsOf(d, e); var f = cfOf(e, r, c) || {}, key = f.st || rowKey(e, r), base = ts.cells[key] || ts.cells.data, o = Object.assign({}, base, { key:key, ov:f.bd || null, blk:f.blk || null, lock:f.lock || '' }); ['bg', 'al', 'dt', 'prec', 'tc'].forEach(function(k){ if (f[k] !== undefined) o[k] = f[k]; }); return o; }
  function raw(e, r, c){ var v = ((e.cells || [])[r] || [])[c]; return v == null ? '' : String(v); }
  function colName(c){ var s = ''; for (c++; c > 0; c = Math.floor((c - 1) / 26)) s = String.fromCharCode(65 + (c - 1) % 26) + s; return s; }
  function refRC(s){ var m = /^([A-Za-z]+)(\d+)$/.exec(s); if (!m) return null; var c = 0; m[1].toUpperCase().split('').forEach(function(ch){ c = c * 26 + ch.charCodeAt(0) - 64; }); return { r:Number(m[2]) - 1, c:c - 1 }; }
  function cellNum(d, e, r, c, dep){ var s = raw(e, r, c).trim(); if (s[0] === '=') return evalF(d, e, s, dep + 1); var n = parseFloat(s.replace(/\s/g, '')); return isNaN(n) ? null : n; }
  // công thức ô (UI-F07): + − × ÷ ^ ( ), ô A1, vùng A1:B3, Sum, Average, Count; lỗi thì ném ra
  function evalF(d, e, src, dep){
    if ((dep || 0) > 30) throw 'Tham chiếu vòng';
    var tk = String(src).replace(/^=/, '').match(/[A-Za-z]+\d+:[A-Za-z]+\d+|[A-Za-z]+\d+|[A-Za-z]+|\d*\.?\d+|\S/g) || [], i = 0;
    function peek(){ return tk[i]; } function next(){ return tk[i++]; }
    function range(a){ var p = a.split(':').map(refRC), out = []; for (var r = Math.min(p[0].r, p[1].r); r <= Math.max(p[0].r, p[1].r); r++) for (var c = Math.min(p[0].c, p[1].c); c <= Math.max(p[0].c, p[1].c); c++) out.push(cellNum(d, e, r, c, dep)); return out; }
    function prim(){ var t = next(); if (t == null) throw 'Thiếu toán hạng';
      if (t === '(') { var v = add(); if (next() !== ')') throw 'Thiếu dấu )'; return v; }
      if (t === '-') return -prim();
      if (/^\d*\.?\d+$/.test(t)) return Number(t);
      if (/^[A-Za-z]+$/.test(t) && peek() === '('){ next(); var list = [];
        if (peek() !== ')') do { var a = peek(); if (/:/.test(a || '')){ next(); list = list.concat(range(a)); } else list.push(add()); } while (peek() === ',' && next());
        if (next() !== ')') throw 'Thiếu dấu )';
        var fn = t.toLowerCase(), nums = list.filter(function(v){ return v != null; }), sum = nums.reduce(function(s, v){ return s + v; }, 0);
        if (fn === 'sum') return sum; if (fn === 'average' || fn === 'avg'){ if (!nums.length) throw 'Vùng không có số'; return sum / nums.length; } if (fn === 'count') return nums.length;
        throw 'Không có hàm ' + t; }
      var rc = refRC(t); if (rc){ var v2 = cellNum(d, e, rc.r, rc.c, dep); return v2 == null ? 0 : v2; }
      throw 'Không hiểu “' + t + '”'; }
    function pow(){ var v = prim(); while (peek() === '^'){ next(); v = Math.pow(v, prim()); } return v; }
    function mul(){ var v = pow(); while (peek() === '*' || peek() === '/'){ var op = next(), b = pow(); if (op === '/' && !b) throw 'Chia cho 0'; v = op === '*' ? v * b : v / b; } return v; }
    function add(){ var v = mul(); while (peek() === '+' || peek() === '-'){ var op = next(), b = mul(); v = op === '+' ? v + b : v - b; } return v; }
    var v = add(); if (i < tk.length) throw 'Thừa “' + tk[i] + '”'; return v;
  }
  // định dạng số theo kiểu dữ liệu của ô; số trong bản vẽ luôn dùng dấu chấm thập phân
  function fmtNum(v, dt, prec){
    var p = prec == null ? 2 : prec, grp = function(x){ return String(x).replace(/\B(?=(\d{3})+(?!\d))/g, ' '); };
    if (dt === 'Số thập phân') return TC.d(v, p);
    if (dt === 'Số nguyên') return String(Math.round(v));
    if (dt === 'Phần trăm') return TC.d(v * 100, Math.min(p, 2)) + '%';
    if (dt === 'Tiền tệ') return grp(Math.round(v)) + ' đ';
    if (dt === 'Góc') return TC.d(v, Math.min(p, 2)) + '°';
    return Number.isInteger(v) ? String(v) : String(Math.round(v * 1e4) / 1e4);
  }
  function cellShow(d, e, r, c, cs){
    var s = raw(e, r, c), dt = cs.dt, t = s.trim();
    if (dt === 'Chữ') return s;
    if (t[0] === '='){ try { return fmtNum(evalF(d, e, t, 0), dt === 'Chung' ? 'Chung' : dt, cs.prec); } catch (x){ return '#LỖI'; } }
    if (dt !== 'Chung' && /^-?\d+(\.\d+)?$/.test(t)) return dt === 'Ngày' ? s : fmtNum(Number(t), dt, cs.prec);
    if (dt === 'Ngày'){ var m = /^(\d{4})-(\d{1,2})-(\d{1,2})$/.exec(t); if (m) return ('0' + m[3]).slice(-2) + '/' + ('0' + m[2]).slice(-2) + '/' + m[1]; }
    if (dt === 'Điểm'){ var q = t.split(/\s*,\s*/).map(Number); if (q.length >= 2 && q.every(function(x){ return !isNaN(x); })) return q.map(function(x){ return TC.d(x); }).join(', '); }
    return s;
  }
  SC.tableEval = function(e, r, c){ var d = D(); return cellShow(d, e, r, c, csOf(d, e, r, c)); };
  // ngắt dòng theo bề rộng đo bằng font hiển thị
  function wrapM(d, s, w, h, style, bold){ var out = []; String(s).split('\n').forEach(function(par){ var cur = ''; par.split(' ').forEach(function(x){ var t = cur ? cur + ' ' + x : x; if (cur && tw(d, t, h, style, bold) > w){ out.push(cur); cur = x; } else cur = t; }); out.push(cur); }); return out; }
  function cellLines(d, e, r, c, cs, hk, w){ var txt = cellShow(d, e, r, c, cs), style = cs.ts !== 'Standard' ? cs.ts : (e.style || 'Standard'); return txt === '' ? [] : cs.ang ? [txt.replace(/\n/g, ' ')] : wrapM(d, txt, w - 2 * cs.mh * hk, cs.th * hk, style, cs.b); }
  function hasField(e, r, c){ return (e.fld || []).some(function(f){ return f.cell && f.cell[0] === r && f.cell[1] === c; }); }
  // vẽ bảng thành nguyên tố (canvas.js gọi trong case 'table'): nền ô, viền theo kiểu ô, chữ canh theo kiểu ô, ô gộp, block trong ô, nền xám cho field và công thức
  TC.flatTable = function(x){
    var d = x.doc, e = x.e, P = x.P, t = x.t, ts = tsOf(d, e), hk = hkOf(ts, e), kS = t ? t.k : 1, rS = t ? t.rot : 0, fills = [], lines = [], texts = [], r0, c0;
    // hàng tự cao thêm khi chữ trong ô xuống nhiều dòng (e.rows là chiều cao tối thiểu)
    e._rh = e.rows.slice();
    for (r0 = 0; r0 < e.rows.length; r0++) for (c0 = 0; c0 < e.cols.length; c0++){ var s0 = span(e, r0, c0); if (s0.r0 !== r0 || s0.c0 !== c0 || s0.r1 !== r0) continue; var cs0 = csOf(d, e, r0, c0, ts), w0 = 0; for (var j0 = s0.c0; j0 <= s0.c1; j0++) w0 += e.cols[j0]; var nl = cs0.blk ? 0 : cellLines(d, e, r0, c0, cs0, hk, w0).length; if (nl) e._rh[r0] = Math.max(e._rh[r0], Math.round(cs0.th * hk * (1.35 + 1.5 * (nl - 1)) + 2 * cs0.mv * hk)); }
    var g = tgeo(d, e);
    var src = function(pr, col){ if (col != null){ pr.src = { color:col, layer:e.layer }; } return pr; };
    var rect = function(x0, y0, x1, y1){ return [[x0, y0], [x1, y0], [x1, y1], [x0, y1]].map(P); };
    // cạnh chung của hai ô: viền gán riêng cho ô (Sửa viền) thắng viền của kiểu ô; cùng loại thì nét bật thắng nét tắt, nét dày thắng nét mảnh
    var strong = function(a, b){ if (!b) return a; if (!a) return b; if (!!a.x !== !!b.x) return a.x ? a : b; if (!a.on) return b; if (!b.on) return a; return b.lw > a.lw ? b : a; };
    var own = function(cs, k, same){ return cs.ov && cs.ov[k] ? Object.assign({ x:1 }, cs.ov[k]) : same ? cs.bd[k === 't' || k === 'b' ? 'h' : 'v'] : cs.bd[k]; };
    var nbCs = function(p){ var q = cellAtP(g, p); if (!q) return null; var s = span(e, q.r, q.c); return csOf(d, e, s.r0, s.c0, ts); };
    function edge(a, b, rec){ if (!rec || !rec.on) return; var thick = rec.lw >= .35 ? rec.lw * hk : 0, push = function(p, q){ lines.push(src({ k:'seg', a:P(p), b:P(q), w:thick }, rec.c)); };
      if (rec.dbl){ var o = (rec.sp || .5) * hk / 2, hz = a[1] === b[1]; push([a[0] - (hz ? 0 : o), a[1] - (hz ? o : 0)], [b[0] - (hz ? 0 : o), b[1] - (hz ? o : 0)]); push([a[0] + (hz ? 0 : o), a[1] + (hz ? o : 0)], [b[0] + (hz ? 0 : o), b[1] + (hz ? o : 0)]); } else push(a, b); }
    for (var r = 0; r < g.nr; r++) for (var c = 0; c < g.nc; c++){
      var s = span(e, r, c); if (s.r0 !== r || s.c0 !== c) continue;
      var cs = csOf(d, e, r, c, ts), q = rectOf(g, s), eps = 1e-6 + (q.top - q.bot) * 1e-4, j, i;
      if (cs.bg != null) fills.push(src({ k:'hatch', loops:[rect(q.x0, q.top, q.x1, q.bot)], pat:'SOLID', sc:1, ang:0 }, cs.bg));
      // viền: mỗi ô vẽ cạnh trên và cạnh trái theo từng đoạn cột/hàng; ô cuối vẽ thêm cạnh phải, cạnh dưới
      for (j = s.c0; j <= s.c1; j++){ var xm = (g.X[j] + g.X[j + 1]) / 2, up = nbCs([xm, q.top + eps]), dn = nbCs([xm, q.bot - eps]);
        edge([g.X[j], q.top], [g.X[j + 1], q.top], up ? strong(own(cs, 't', up.key === cs.key), own(up, 'b', up.key === cs.key)) : own(cs, 't'));
        if (!dn) edge([g.X[j], q.bot], [g.X[j + 1], q.bot], own(cs, 'b')); }
      for (i = s.r0; i <= s.r1; i++){ var ym = (g.R[i][0] + g.R[i][1]) / 2, lf = nbCs([q.x0 - eps, ym]);
        edge([q.x0, g.R[i][0]], [q.x0, g.R[i][1]], lf ? strong(own(cs, 'l', lf.key === cs.key), own(lf, 'r', lf.key === cs.key)) : own(cs, 'l'));
        if (s.c1 === g.nc - 1) edge([q.x1, g.R[i][0]], [q.x1, g.R[i][1]], own(cs, 'r')); }
      // block trong ô: co vừa ô
      if (cs.blk && d.blocks[cs.blk] && x.sub){ var bi = TC.prepEnt(d, { t:'insert', name:cs.blk, p:[0, 0], sx:1, sy:1, rot:0, layer:e.layer }, true), bb = bi._bb, bw = Math.max(bb[2] - bb[0], 1), bh = Math.max(bb[3] - bb[1], 1), k = Math.min((q.x1 - q.x0 - 2 * cs.mh * hk) / bw, (q.top - q.bot - 2 * cs.mv * hk) / bh), cx = (q.x0 + q.x1) / 2, cy = (q.top + q.bot) / 2;
        if (k > 0) x.sub({ t:'insert', name:cs.blk, p:[cx - (bb[0] + bb[2]) / 2 * k, cy - (bb[1] + bb[3]) / 2 * k], sx:k, sy:k, rot:0, layer:e.layer }); continue; }
      var txt = cellShow(d, e, r, c, cs); if (txt === '') continue;
      var h = cs.th * hk, mh = cs.mh * hk, mv = cs.mv * hk, al = cs.al, style = cs.ts !== 'Standard' ? cs.ts : (e.style || 'Standard');
      if (al === 'auto') al = /^[-+(]?[\d.,\s]+[)%°]?$/.test(txt.trim()) ? 'MR' : 'ML';
      var ls = cellLines(d, e, r, c, cs, hk, q.x1 - q.x0), lh = h * 1.5, n = ls.length, V0 = al[0], H0 = al[1];
      var xx = cs.ang ? (q.x0 + q.x1) / 2 : H0 === 'L' ? q.x0 + mh : H0 === 'R' ? q.x1 - mh : (q.x0 + q.x1) / 2, pal = cs.ang ? 'm' : H0 === 'L' ? 'l' : H0 === 'R' ? 'r' : 'c';
      var y0 = cs.ang ? (q.top + q.bot) / 2 : V0 === 'T' ? q.top - mv - h : V0 === 'B' ? q.bot + mv + (n - 1) * lh : (q.top + q.bot) / 2 + (h + (n - 1) * lh) / 2 - h;
      // nền xám của field và công thức (FIELDDISPLAY), không in
      if (TC.S.fieldBg !== false && (raw(e, r, c).trim()[0] === '=' || hasField(e, r, c))){ var wmax = Math.max.apply(null, ls.map(function(l){ return tw(d, l, h, style, cs.b); })), fx0 = pal === 'l' ? xx : pal === 'r' ? xx - wmax : xx - wmax / 2; fills.push(src({ k:'hatch', loops:[rect(fx0 - h * .15, y0 + h * 1.1, fx0 + wmax + h * .15, y0 - (n - 1) * lh - h * .3)], pat:'SOLID', sc:1, ang:0, fbg:1 }, 8)); }
      ls.forEach(function(l, li){ texts.push(src({ k:'text', p:P([xx, y0 - li * lh]), h:h * kS, s:l, rot:rS + (cs.ang || 0), al:pal, style:style, bold:cs.b, cell:[r, c] }, cs.tc)); });
    }
    fills.concat(lines, texts).forEach(function(pr){ var s0 = pr.src; x.o(pr); if (s0) pr.src = s0; });
  };

  /* ================= Kiểu bảng: hộp thoại (UI-D04) ================= */
  var SAMPLE = { 'TCVN-thong-ke':[['BẢNG THỐNG KÊ CỬA'], ['STT', 'Ký hiệu', 'Loại cửa', 'Kích thước (mm)', 'Số lượng'], ['1', 'D1', 'Cửa đi 1 cánh', '900 × 2200', '6'], ['2', 'D2', 'Cửa đi WC', '700 × 2100', '2'], ['3', 'S1', 'Cửa sổ 2 cánh', '1400 × 1500', '8'], ['', '', '', 'Tổng cộng', '=Sum(E3:E5)']], 'TCVN-thong-ke-thep':[['BẢNG THỐNG KÊ CỐT THÉP'], ['Số hiệu', 'Đường kính', 'Dài 1 thanh', 'Số lượng', 'Khối lượng (kg)'], ['1', 'Φ12', '4250', '24', '90.6'], ['2', 'Φ16', '3800', '12', '72.0'], ['3', 'Φ8', '1200', '86', '40.7'], ['', '', '', 'Tổng', '=Sum(E3:E5)']], 'TCVN-dien-tich':[['BẢNG THỐNG KÊ DIỆN TÍCH'], ['STT', 'Tên phòng', 'Diện tích (m²)'], ['1', 'Phòng khách + ăn', '35.39'], ['2', 'Bếp', '17.31'], ['3', 'P. Ngủ 1', '15.07'], ['', 'Tổng cộng', '=Sum(C3:C5)']] };
  function sampleTable(d, name, ts){
    if (ts.start) return Object.assign(clone(ts.start), { t:'table', p:[0, 0], th:250, tstyle:name, _ts:ts, layer:'0' });
    var rowsT = SAMPLE[name] || [['Tiêu đề'], ['Đầu cột', 'Đầu cột', 'Đầu cột'], ['Dữ liệu', '120', '3.50'], ['Dữ liệu', '85', '12.00'], ['Dữ liệu', '240', '7.25']], nc = rowsT[1].length, hk = 100;
    var cols = rowsT[1].map(function(h, j){ return Math.max(1000, 420 + 205 * Math.max.apply(null, rowsT.slice(1).map(function(r){ return String(r[j] || '').length; }))); }), rs =rowsT.map(function(r, i){ return i === 0 ? 'title' : i === 1 ? 'header' : 'data'; });
    return { t:'table', p:[0, 0], cols:cols, rows:rs.map(function(k){ return rowH(ts.cells[k] || ts.cells.data, 1, hk); }), cells:rowsT.map(function(r){ return r.length < nc ? r.concat(new Array(nc - r.length).join('.').split('.')) : r; }), merge:[[0, 0, nc - 1]], rs:rs, head:2, th:250, tstyle:name, _ts:ts, layer:'0' };
  }
  function pvEnt(c, d, e){
    var x = c.getContext('2d'), W = c.width, Hh = c.height; x.fillStyle = TC.cssv('--c-canvas'); x.fillRect(0, 0, W, Hh);
    TC.prepEnt(d, e, true); var b = e._bb, s = Math.min((W - 24) / Math.max(b[2] - b[0], 1), (Hh - 24) / Math.max(b[3] - b[1], 1));
    var f = Object.create(TC.View.prototype); f.W = W; f.H = Hh; f.doc = d; f.dpr = 1; f.drawPrims(x, e, { cx:(b[0] + b[2]) / 2, cy:(b[1] + b[3]) / 2, s:s }, false);
  }
  function tablesUsing(d, n){ return allEnts(d).filter(function(e){ return e.t === 'table' && (e.tstyle || 'Standard') === n; }).length; }
  SC.tableStyle = function(sel0){
    var d = D(), S = tstyles(d), sel = sel0 && S[sel0] ? sel0 : SC.curTableStyle(d), filt = 'Tất cả kiểu';
    var dlg = TC.dialog({ id:'tbstyle', title:'Kiểu bảng', cmd:'TABLESTYLE', code:'UI-D04', w:780,
      body:'<div class="dsm tbsm"><div class="stack" style="gap:6px"><span class="gh">Kiểu</span><div class="tree" id="tbs-l" role="listbox" aria-label="Danh sách kiểu bảng" tabindex="0" style="min-height:236px"></div>' + H.sel('tbs-f', ['Tất cả kiểu', 'Kiểu đang dùng'], filt) + '</div><div class="stack"><span class="gh">Xem trước: <span id="tbs-n" style="text-transform:none;letter-spacing:0"></span></span>' + H.cv('tbs-pv', 320, 196) + '<div class="grp"><div class="gh">Mô tả</div><p id="tbs-d" style="font-size:var(--fs-s);margin:0"></p></div></div><div class="stack ds-btns"><button class="btn" type="button" id="tbs-set">Đặt hiện hành</button><button class="btn" type="button" id="tbs-new">Mới…</button><button class="btn" type="button" id="tbs-edit">Sửa…</button><button class="btn" type="button" id="tbs-del">Xóa</button></div></div>',
      foot:'<span class="faint" style="font-size:var(--fs-s)">Kiểu hiện hành: <b id="tbs-cur" style="color:var(--c-text)"></b> · kiểu TCVN có sẵn trong bản vẽ mẫu</span><span class="grow"></span><button class="btn primary" type="button" data-x="close">Đóng</button>' });
    function list(){
      var cur = SC.curTableStyle(d), names = Object.keys(S).filter(function(n){ return filt === 'Tất cả kiểu' || tablesUsing(d, n) || n === cur; });
      $('#tbs-l', dlg.el).innerHTML = names.map(function(n){ var u = tablesUsing(d, n); return '<button class="tn" type="button" role="option" data-tbs="' + esc(n) + '" aria-selected="' + (n === sel) + '">' + ic('table', 's12') + '<span>' + esc(n) + '</span>' + (S[n].tcvn ? '<span class="tag acc" style="height:15px;margin-left:4px">TCVN</span>' : '') + '<span class="cnt">' + (n === cur ? 'hiện hành' : u ? u + ' bảng' : '') + '</span></button>'; }).join('') || '<div class="empty" style="padding:16px 8px">' + ic('table') + '<span>Chưa bảng nào dùng kiểu bảng.</span></div>';
      if (!S[sel]) sel = 'Standard';
      $('#tbs-n', dlg.el).textContent = sel; $('#tbs-cur', dlg.el).textContent = cur;
      var u2 = tablesUsing(d, sel); $('#tbs-d', dlg.el).innerHTML = esc(S[sel].desc || 'Tạo từ ' + (S[sel].from || 'Standard') + '.') + '<br><span class="faint">' + (u2 ? 'Đang được ' + u2 + ' bảng dùng.' : 'Chưa bảng nào dùng.') + (S[sel].start ? ' Có bảng khởi đầu.' : '') + '</span>';
      var del = $('#tbs-del', dlg.el), why = sel === 'Standard' ? 'Không xóa được kiểu Standard' : sel === cur ? 'Kiểu đang là hiện hành' : u2 ? 'Kiểu đang được ' + u2 + ' bảng dùng' : ''; del.disabled = !!why; del.dataset.tip = why;
      pvEnt($('#tbs-pv', dlg.el), d, sampleTable(d, sel, S[sel]));
    }
    dlg.el.addEventListener('click', function(ev){ var t = ev.target.closest('[data-tbs]'); if (t){ sel = t.dataset.tbs; list(); $('#tbs-l', dlg.el).focus(); } });
    dlg.el.addEventListener('dblclick', function(ev){ if (ev.target.closest('[data-tbs]')) $('#tbs-edit', dlg.el).click(); });
    TC.listKeys($('#tbs-l', dlg.el), '[data-tbs]', function(it, enter){ sel = it.dataset.tbs; list(); if (enter) $('#tbs-edit', dlg.el).click(); else $('#tbs-l', dlg.el).focus(); });
    $('#tbs-f', dlg.el).addEventListener('change', function(){ filt = this.value; list(); });
    $('#tbs-set', dlg.el).addEventListener('click', function(){ SC.setTableStyle(d, sel); list(); A().renderRibbon(); TC.cmdline.msg('Kiểu bảng hiện hành: <b>' + esc(sel) + '</b>.'); });
    $('#tbs-edit', dlg.el).addEventListener('click', function(){ SC.tableStyleEdit(sel).then(list); });
    $('#tbs-new', dlg.el).addEventListener('click', function(){ newStyle(d, sel).then(function(n){ if (!n) return; sel = n; list(); SC.tableStyleEdit(n).then(list); }); });
    $('#tbs-del', dlg.el).addEventListener('click', function(){ TC.confirm({ title:'Xóa kiểu bảng', msg:'Xóa kiểu bảng <b>' + esc(sel) + '</b> khỏi bản vẽ?', detail:'Không bảng nào đang dùng kiểu này. Hoàn tác được bằng Ctrl+Z.', ok:'Xóa kiểu bảng', danger:true }).then(function(y){ if (!y) return; var n = sel; ED.run('TABLESTYLE', function(){ var done = ED.state(function(){ return d.tableStyles; }, function(s){ d.tableStyles = s; }); delete d.tableStyles[n]; done(); }); S = tstyles(d); sel = 'Standard'; list(); }); });
    list(); return dlg;
  };
  function newStyle(d, from){
    return new Promise(function(res){ var S = tstyles(d), n = 1; while (S['Kiểu bảng ' + n]) n++;
      var dlg = TC.dialog({ id:'tbsnew', title:'Tạo kiểu bảng mới', code:'UI-D04', w:440, help:false, onClose:function(r){ if (r !== true) res(null); }, body:'<div class="stack">' + H.row('Tên kiểu mới', '<input class="inp" id="tn-n" value="Kiểu bảng ' + n + '" style="width:220px" spellcheck="false">', 110) + H.row('Bắt đầu từ', H.sel('tn-f', Object.keys(S), from, 220), 110) + '<span class="fmsg" id="tn-m" aria-live="polite"></span></div>', foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="tn-ok">Tạo kiểu bảng</button>' });
      $('#tn-ok', dlg.el).addEventListener('click', function(){ var nm = $('#tn-n', dlg.el).value.trim(), m = $('#tn-m', dlg.el), err = !nm ? 'Nhập tên kiểu bảng.' : S[nm] ? 'Đã có kiểu “' + nm + '”. Chọn tên khác.' : /[<>\/\\":;?*|,=`]/.test(nm) ? 'Tên không chứa ký tự < > / \\ " : ; ? * | , = `' : null;
        if (err){ m.className = 'fmsg err'; m.innerHTML = ic('warn', 'xs') + esc(err); $('#tn-n', dlg.el).classList.add('err'); return; }
        var f = $('#tn-f', dlg.el).value; ED.run('TABLESTYLE', function(){ var done = ED.state(function(){ return d.tableStyles; }, function(s){ d.tableStyles = s; }); var c = clone(S[f]); c.from = f; c.desc = ''; delete c.tcvn; d.tableStyles[nm] = c; done(); }); dlg.close(true); res(nm); });
    });
  }
  var BDB = [['all', 'Tất cả viền', 'bdall', 'tblrhv'], ['out', 'Viền ngoài', 'bdout', 'tblr'], ['in', 'Viền trong', 'bdin', 'hv'], ['bot', 'Viền dưới', 'bdbot', 'b'], ['left', 'Viền trái', 'bdleft', 'l'], ['top', 'Viền trên', 'bdtop', 't'], ['right', 'Viền phải', 'bdright', 'r'], ['none', 'Không viền', 'bdnone', '']];
  SC.tableStyleEdit = function(name){
    return new Promise(function(res){
      var d = D(), S = tstyles(d), C = clone(S[name]), ck = 'data', brush = Object.assign({}, C.cells.data.bd.t);
      var dlg = TC.dialog({ id:'tbsedit', title:'Sửa kiểu bảng: ' + esc(name), code:'UI-D04', w:880, onClose:function(){ res(); },
        body:'<div class="dse"><div class="stack" style="gap:10px">' +
          '<div class="grid2">' + H.grp('Bảng khởi đầu', '<div class="hrow"><span id="te-st" style="font-size:var(--fs-s)"></span><span class="grow"></span><button class="btn sm" type="button" id="te-pick" data-tip="Chọn một bảng có sẵn trên bản vẽ làm mẫu cho bảng mới">' + ic('select') + 'Chọn bảng…</button><button class="btn sm ghost" type="button" id="te-unpick">Bỏ</button></div>') + H.grp('Chung', H.row('Hướng bảng', H.sel('te-dir', ['Xuống', 'Lên'], C.dir === 'up' ? 'Lên' : 'Xuống', 120), 96)) + '</div>' +
          '<div class="grp"><div class="gh">Kiểu ô</div><div class="hrow"><select class="inp" id="te-cs" style="width:170px" aria-label="Kiểu ô"></select><button class="btn sm" type="button" id="te-csnew">Tạo kiểu ô…</button><button class="btn sm ghost" type="button" id="te-csdel">Xóa kiểu ô</button></div>' + H.tabs([['g', 'Chung'], ['t', 'Chữ'], ['b', 'Viền']], 'g', 'te') + H.pane('g', 'g', '', 'tep') + H.pane('t', 'g', '', 'tep') + H.pane('b', 'g', '', 'tep') + '</div>' +
          '</div><div class="stack"><span class="gh">Xem trước kiểu ô</span>' + H.cv('te-cpv', 300, 92) + '<span class="gh">Xem trước bảng</span>' + H.cv('te-pv', 300, 214) + '<p class="faint" style="font-size:var(--fs-s);margin:0">Các bảng đang dùng kiểu này cập nhật khi lưu. Hoàn tác bằng Ctrl+Z.</p></div></div>',
        foot:'<span class="faint" style="font-size:var(--fs-s)" id="te-u"></span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="te-ok">Lưu kiểu bảng</button>' });
      var $d = function(s){ return $(s, dlg.el); };
      function cs(){ return C.cells[ck]; }
      function csList(){ $d('#te-cs').innerHTML = Object.keys(C.cells).map(function(k){ return '<option value="' + esc(k) + '"' + (k === ck ? ' selected' : '') + '>' + esc(CSN[k] || k) + '</option>'; }).join(''); $d('#te-csdel').disabled = !!CSN[ck]; }
      function panes(){
        var c = cs(), precOn = /thập phân|Phần trăm|Góc/.test(c.dt);
        $d('[data-dp="g"]').innerHTML = H.row('Màu nền', colorBtn('te-bg', c.bg, 'Không'), 110) + H.row('Canh lề', H.sel('te-al', [['auto', 'Tự động: chữ trái, số phải']].concat(ALN).map(function(a){ return a[1]; }), a2l(c.al), 210), 110) + H.row('Định dạng', H.sel('te-dt', DTS, c.dt, 150) + H.sel('te-pr', ['0', '0.0', '0.00', '0.000', '0.0000'], ['0', '0.0', '0.00', '0.000', '0.0000'][c.prec] || '0.00', 90).replace('<select', '<select aria-label="Độ chính xác"' + (precOn ? '' : ' disabled')), 110) + H.row('Loại', H.sel('te-kind', ['Dữ liệu', 'Nhãn'], c.kind, 150), 110) + H.row('Lề ngang', H.num('te-mh', TC.d(c.mh), { w:110, pick:false }), 110) + H.row('Lề dọc', H.num('te-mv', TC.d(c.mv), { w:110, pick:false }), 110) + H.row('', H.chk('te-mg', 'Gộp ô khi tạo hàng, cột', c.merge), 110);
        $d('[data-dp="t"]').innerHTML = H.row('Kiểu chữ', H.sel('te-ts', d.styles.map(function(s){ return s.name; }), c.ts, 170) + '<button class="ib" type="button" id="te-tsm" data-tip="Quản lý kiểu chữ (STYLE)" aria-label="Quản lý kiểu chữ">' + ic('more') + '</button>', 110) + H.row('Chiều cao chữ', H.num('te-th', TC.d(c.th), { w:110, pick:false }) + '<span class="faint" style="font-size:var(--fs-xs)">mm trên giấy</span>', 110) + H.row('Màu chữ', colorBtn('te-tc', c.tc), 110) + H.row('Góc chữ', H.sel('te-ang', ['0°', '90°', '180°', '270°'], (c.ang || 0) + '°', 110), 110) + H.row('', H.chk('te-b', 'Chữ đậm', c.b), 110);
        $d('[data-dp="b"]').innerHTML = H.row('Độ dày', H.sel('te-lw', LWS.map(function(w){ return TC.d(w) + ' mm'; }), TC.d(brush.lw) + ' mm', 150), 110) + H.row('Linetype', H.sel('te-lt', ['ByBlock', 'Continuous', 'DASHED', 'HIDDEN'], brush.lt, 150), 110) + H.row('Màu', colorBtn('te-bc', brush.c), 110) + H.row('', H.chk('te-dbl', 'Đường đôi', brush.dbl), 110) + H.row('Khoảng cách', H.num('te-sp', TC.d(brush.sp), { w:110, pick:false }).replace('<input', '<input' + (brush.dbl ? '' : ' disabled')), 110) +
          '<div class="tbbd" role="group" aria-label="Áp dụng cho cạnh viền">' + BDB.map(function(b){ return '<button class="ib" type="button" data-bd="' + b[0] + '" data-tip="' + b[1] + '" aria-label="' + b[1] + '">' + ic(b[2]) + '</button>'; }).join('') + '</div><p class="faint" style="font-size:var(--fs-xs);margin:0">Chọn độ dày, linetype, màu ở trên rồi bấm một nút để áp dụng cho cạnh viền của kiểu ô.</p>';
      }
      function a2l(al){ return al === 'auto' ? 'Tự động: chữ trái, số phải' : (ALN.filter(function(a){ return a[0] === al; })[0] || ALN[4])[1]; }
      function pv(){
        var cpv = { t:'table', p:[0, 0], cols:[1400, 1400, 1400], rows:[700, 700], cells:[['Chữ', 'Mẫu', '12.50'], ['Chữ', 'Mẫu', '8.00']], rs:[ck, ck], th:250, _ts:C, layer:'0' };
        pvEnt($d('#te-cpv'), d, cpv); pvEnt($d('#te-pv'), d, sampleTable(d, name, C));
        $d('#te-st').innerHTML = C.start ? '<b>' + esc(C.start.name) + '</b>' : '<span class="faint">Không dùng</span>'; $d('#te-unpick').disabled = !C.start;
        var u = tablesUsing(d, name); $d('#te-u').textContent = u ? u + ' bảng đang dùng kiểu này.' : '';
      }
      function all(){ csList(); panes(); pv(); }
      dlg.el.addEventListener('change', function(ev){ var t = ev.target, id = t.id, c = cs(), v = t.value;
        if (id === 'te-cs'){ ck = v; brush = Object.assign({}, cs().bd.t); all(); return; }
        if (id === 'te-dir') C.dir = v === 'Lên' ? 'up' : 'down';
        if (id === 'te-al') c.al = v === a2l('auto') ? 'auto' : (ALN.filter(function(a){ return a[1] === v; })[0] || ALN[4])[0];
        if (id === 'te-dt'){ c.dt = v; panes(); }
        if (id === 'te-pr') c.prec = v.length > 1 ? v.length - 2 : 0;
        if (id === 'te-kind') c.kind = v; if (id === 'te-mg') c.merge = t.checked; if (id === 'te-ts') c.ts = v; if (id === 'te-b') c.b = t.checked;
        if (id === 'te-ang') c.ang = parseInt(v, 10) || 0;
        if (id === 'te-lw') brush.lw = parseFloat(v); if (id === 'te-lt') brush.lt = v; if (id === 'te-dbl'){ brush.dbl = t.checked; panes(); }
        pv(); });
      dlg.el.addEventListener('numchange', function(ev){ var inp = $('input', ev.target), id = inp && inp.id, c = cs(), v = ev.detail; if (!(v >= 0)) return;
        if (id === 'te-mh') c.mh = v; if (id === 'te-mv') c.mv = v; if (id === 'te-th' && v > 0) c.th = v; if (id === 'te-sp') brush.sp = v; pv(); });
      dlg.el.addEventListener('click', function(ev){ var t = ev.target.closest('button'); if (!t) return; var c = cs();
        if (t.id === 'te-bg') colorMenu(t, c.bg, 'Không', function(v){ c.bg = v; panes(); pv(); });
        if (t.id === 'te-tc') colorMenu(t, c.tc, null, function(v){ c.tc = v; panes(); pv(); });
        if (t.id === 'te-bc') colorMenu(t, brush.c, null, function(v){ brush.c = v; panes(); });
        if (t.id === 'te-tsm') TC.cmd.run("'STYLE");
        if (t.dataset.bd){ var b = BDB.filter(function(x){ return x[0] === t.dataset.bd; })[0]; ['t', 'b', 'l', 'r', 'h', 'v'].forEach(function(k){ if (b[0] === 'none') c.bd[k].on = false; else if (b[3].indexOf(k) >= 0) c.bd[k] = Object.assign({}, brush, { on:true }); }); pv(); TC.cmdline.msg('Kiểu ô ' + esc(CSN[ck] || ck) + ': ' + b[1].toLowerCase() + '.'); }
        if (t.id === 'te-csnew') SC.ask('Tạo kiểu ô', 'Tên kiểu ô mới', 'Ô ' + (Object.keys(C.cells).length - 2), function(v){ return C.cells[v] || CSN[v] || Object.keys(CSN).some(function(k){ return CSN[k] === v; }) ? 'Đã có kiểu ô “' + v + '”.' : null; }, 'UI-D04').then(function(v){ if (!v) return; C.cells[v] = clone(cs()); ck = v; all(); });
        if (t.id === 'te-csdel' && !CSN[ck]){ delete C.cells[ck]; ck = 'data'; all(); }
        if (t.id === 'te-unpick'){ delete C.start; pv(); }
        if (t.id === 'te-pick') pickTable().then(function(e){ if (!e) return; C.start = { name:'Bảng ' + e.cols.length + ' cột × ' + e.rows.length + ' hàng', cols:e.cols.slice(), rows:e.rows.slice(), cells:clone(e.cells || []), merge:clone(e.merge || []), rs:e.rs ? e.rs.slice() : e.rows.map(function(x, i){ return rowKey(e, i); }), head:e.head || 0, cf:clone(e.cf || {}) }; pv(); });
      });
      $d('#te-ok').addEventListener('click', function(){ ED.run('TABLESTYLE', function(){ var done = ED.state(function(){ return d.tableStyles; }, function(s){ d.tableStyles = s; reprep(d); }); d.tableStyles[name] = C; done(); }); reprep(d); dlg.close(true); TC.toast('Đã lưu kiểu bảng <b>' + esc(name) + '</b>' + (tablesUsing(d, name) ? '; ' + tablesUsing(d, name) + ' bảng đã cập nhật.' : '.'), { log:false }); });
      all();
    });
  };
  // ẩn mọi hộp thoại đang mở để chọn một đối tượng trên vùng vẽ (Esc để quay lại)
  function pickEnt(msg){
    var hid = $$('.dlg, .scrim').filter(function(x){ return x.style.visibility !== 'hidden'; }), esc1 = function(ev){ if (ev.key === 'Escape'){ ev.stopPropagation(); ev.preventDefault(); TC.cad.cancelPoint(); } };
    hid.forEach(function(x){ x.style.visibility = 'hidden'; }); TC.cmdline.msg(msg); document.addEventListener('keydown', esc1, true);
    return TC.cad.getPoint({ pick:true }).then(function(r){ document.removeEventListener('keydown', esc1, true); hid.forEach(function(x){ x.style.visibility = ''; }); if (!r) return undefined; var id = V().hitTest(r.s[0], r.s[1]); return id ? D().byId[id] : null; });
  }
  function pickTable(){ return pickEnt('Chọn bảng làm bảng khởi đầu. Esc để quay lại hộp thoại.').then(function(e){ if (e && e.t === 'table') return e; if (e !== undefined) TC.toast('Đối tượng đã chọn không phải bảng. Bấm Chọn bảng… rồi bấm vào một đường kẻ của bảng.', { type:'warn', log:false }); return null; }); }
  IMPL.TABLESTYLE = function(ctx, pre){ SC.tableStyle(pre && typeof pre === 'string' ? pre : null); };

  /* ================= Chèn bảng UI-F06 ================= */
  function makeTable(d, O, p, wh){
    var S = tstyles(d), ts = S[O.st] || S.Standard, hk = annoK(d), e, i;
    if (ts.start){ e = Object.assign(clone(ts.start), { t:'table', p:p, th:hk * ts.cells.data.th, tstyle:O.st }); delete e.name; return e; }
    var nc = Math.max(1, Math.round(O.nc)), cw = O.cw, nr = Math.max(1, Math.round(O.nr));
    var H1 = function(key){ return rowH(ts.cells[key] || ts.cells.data, key === 'data' ? O.rl : 1, hk); };
    if (wh){ cw = Math.max(200, wh[0] / nc); nr = Math.max(1, Math.floor((wh[1] - H1(O.r1) - H1(O.r2)) / H1(O.r3))); }
    var rs = [O.r1, O.r2], merge = []; for (i = 0; i < nr; i++) rs.push(O.r3);
    rs.forEach(function(key, i){ if ((ts.cells[key] || {}).merge && nc > 1) merge.push([i, 0, nc - 1]); });
    var head = 0; while (head < rs.length && rs[head] !== 'data') head++;
    var row = function(v){ var a = []; for (var j = 0; j < nc; j++) a.push(v); return a; };
    return { t:'table', p:p, cols:row(cw), rows:rs.map(H1), cells:rs.map(function(){ return row(''); }), merge:merge, head:head, rs:rs, th:hk * ts.cells.data.th, tstyle:O.st };
  }
  IMPL.TABLE = function(ctx){
    var d = D(), O = Object.assign({ st:SC.curTableStyle(d), from:'empty', ins:'pt', nc:5, cw:2500, nr:5, rl:1, r1:'title', r2:'header', r3:'data' }, TC.load('tblins', {}));
    if (!tstyles(d)[O.st]) O.st = 'Standard';
    ctx.badge('TB');
    return new Promise(function(res){
      var CS3 = function(){ return Object.keys((tstyles(d)[O.st] || tstyles(d).Standard).cells); };
      var dlg = TC.dialog({ id:'tbins', title:'Chèn bảng', cmd:'TABLE', code:'UI-F06', w:800, onClose:function(r){ if (r !== true) res(); },
        body:'<div class="tbin"><div class="stack">' +
          H.grp('Kiểu bảng', '<div class="hrow"><select class="inp" id="ti-st" style="flex:1" aria-label="Kiểu bảng"></select><button class="ib" type="button" id="ti-stm" data-tip="Mở Kiểu bảng (TABLESTYLE)" aria-label="Mở Kiểu bảng">' + ic('gear') + '</button></div>') +
          H.grp('Cách bắt đầu', H.rad('ti-from', 'empty', 'Bảng trống', O.from === 'empty') + '<label class="chk dis"><input type="radio" name="ti-from" value="link" disabled> Từ liên kết dữ liệu (Excel) <span class="tag" style="height:15px">sau v1.0</span></label>' + H.rad('ti-from', 'data', 'Từ dữ liệu đối tượng (Trích xuất dữ liệu)', O.from === 'data')) +
          '<span class="gh">Xem trước</span>' + H.cv('ti-pv', 330, 200) + '</div><div class="stack">' +
          H.grp('Cách chèn', '<div class="hrow">' + H.rad('ti-ins', 'pt', 'Chỉ điểm chèn', O.ins === 'pt') + H.rad('ti-ins', 'win', 'Chỉ khung', O.ins === 'win') + '</div>') +
          H.grp('Cột và hàng', H.row('Số cột', H.num('ti-nc', String(O.nc), { prec:0, unit:'none', w:110, pick:false }), 120) + H.row('Rộng cột', H.num('ti-cw', TC.d(O.cw, 0), { prec:0, w:110, pick:false }), 120) + H.row('Số hàng dữ liệu', H.num('ti-nr', String(O.nr), { prec:0, unit:'none', w:110, pick:false }), 120) + H.row('Cao hàng (dòng)', H.num('ti-rl', String(O.rl), { prec:0, unit:'none', w:110, pick:false }), 120) + '<span class="fmsg" id="ti-m" aria-live="polite"></span>') +
          H.grp('Kiểu ô', '<div id="ti-cs"></div>') + '<div id="ti-start"></div>' +
          '</div></div>',
        foot:'<span class="faint mono" style="font-size:var(--fs-s)" id="ti-sum"></span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="ti-ok">Chèn bảng</button>' });
      var $d = function(s){ return $(s, dlg.el); };
      function styles(){ $d('#ti-st').innerHTML = SC.tableStyleNames(d).map(function(n){ return '<option' + (n === O.st ? ' selected' : '') + '>' + esc(n) + '</option>'; }).join(''); }
      function cellSel(){ var ks = CS3(), lab = function(k){ return CSN[k] || k; }; $d('#ti-cs').innerHTML = [['r1', 'Hàng đầu'], ['r2', 'Hàng hai'], ['r3', 'Các hàng còn lại']].map(function(x){ return H.row(x[1], '<select class="inp" id="ti-' + x[0] + '" style="width:170px">' + ks.map(function(k){ return '<option value="' + esc(k) + '"' + (O[x[0]] === k ? ' selected' : '') + '>' + esc(lab(k)) + '</option>'; }).join('') + '</select>', 120); }).join(''); }
      function check(){ var bad = !(O.nc >= 1 && O.nc <= 100) ? 'Số cột từ 1 đến 100.' : !(O.nr >= 1 && O.nr <= 500) ? 'Số hàng dữ liệu từ 1 đến 500.' : !(O.cw > 0) ? 'Rộng cột phải lớn hơn 0.' : !(O.rl >= 1 && O.rl <= 20) ? 'Cao hàng từ 1 đến 20 dòng.' : null, m = $d('#ti-m'); m.className = 'fmsg' + (bad ? ' err' : ''); m.innerHTML = bad ? ic('warn', 'xs') + esc(bad) : ''; $d('#ti-ok').disabled = !!bad || O.from === 'link'; return !bad; }
      function update(){
        var ts = tstyles(d)[O.st] || tstyles(d).Standard, win = O.ins === 'win', st = !!ts.start;
        ['#ti-nc', '#ti-cw', '#ti-nr', '#ti-rl'].forEach(function(s){ var i = $d(s); i.disabled = st || (win && (s === '#ti-cw' || s === '#ti-nr')); i.closest('.numf').classList.toggle('is-dis', i.disabled); });
        if (win && !st){ $d('#ti-cw').value = 'Tự động'; $d('#ti-nr').value = 'Tự động'; } else { $d('#ti-cw').value = TC.d(O.cw, 0); $d('#ti-nr').value = String(O.nr); }
        $$('#ti-cs select', dlg.el).forEach(function(s){ s.disabled = st; });
        $d('#ti-start').innerHTML = st ? '<div class="note">' + ic('info', 'xs') + '<span>Kiểu <b>' + esc(O.st) + '</b> có bảng khởi đầu (' + esc(ts.start.name) + '): bảng mới lấy cột, hàng và nội dung cố định từ bảng đó.</span></div>' : '';
        var ok = check(), e = makeTable(d, Object.assign({}, O, { nr:Math.min(O.nr, 12) }), [0, 0]); e.layer = '0';
        if (!st) e.cells = e.rs.map(function(k){ return e.cols.map(function(w, j){ return k === 'title' ? (j ? '' : 'Tiêu đề') : CSN[k] || k; }); }); // chữ giữ chỗ chỉ để xem trước
        if (ok) pvEnt($d('#ti-pv'), d, e);
        var full = makeTable(d, O, [0, 0]); $d('#ti-sum').textContent = O.from === 'data' ? 'Mở Trích xuất dữ liệu để tạo bảng từ thuộc tính đối tượng' : full.cols.length + ' cột × ' + full.rows.length + ' hàng · ' + (win && !st ? 'kích thước theo khung' : TC.d(full.cols.reduce(function(s, x){ return s + x; }, 0), 0) + ' × ' + TC.d(full.rows.reduce(function(s, x){ return s + x; }, 0), 0));
        $d('#ti-ok').textContent = O.from === 'data' ? 'Mở Trích xuất dữ liệu' : 'Chèn bảng';
      }
      dlg.el.addEventListener('change', function(ev){ var t = ev.target;
        if (t.id === 'ti-st'){ O.st = t.value; var ks = CS3(); ['r1', 'r2', 'r3'].forEach(function(k, i){ if (ks.indexOf(O[k]) < 0) O[k] = ['title', 'header', 'data'][i]; }); cellSel(); }
        if (t.name === 'ti-from') O.from = t.value; if (t.name === 'ti-ins') O.ins = t.value; if (/^ti-r[123]$/.test(t.id)) O[t.id.slice(3)] = t.value;
        update(); });
      dlg.el.addEventListener('numchange', function(ev){ var inp = $('input', ev.target), k = inp && inp.id.slice(3); if (k && k in O){ O[k] = ev.detail; update(); } });
      $d('#ti-stm').addEventListener('click', function(){ var sd = SC.tableStyle(O.st); var wait = setInterval(function(){ if (!document.body.contains(sd.el)){ clearInterval(wait); if (!tstyles(d)[O.st]) O.st = 'Standard'; styles(); cellSel(); update(); } }, 250); });
      $d('#ti-ok').addEventListener('click', function(){
        if (O.from === 'data'){ dlg.close(true); res(); setTimeout(function(){ TC.cmd.run('DATAEXTRACTION'); }, 60); return; }
        if (!check()) return; TC.save('tblins', O); dlg.close(true);
        var place = function(e){ var tb; ED.run('TABLE', function(){ tb = ED.add(ED.make(e)); }); V().temp = []; ctx.out('Đã chèn bảng ' + tb.cols.length + ' cột × ' + tb.rows.length + ' hàng. Gõ nội dung ô; Tab, Enter để sang ô kế tiếp; Esc để thôi.'); setTimeout(function(){ startCell(tb, 0, 0); editCell(); }, 30); };
        if (O.ins === 'pt' || tstyles(d)[O.st].start) return ctx.point('Điểm chèn (góc trên trái của bảng)', { rubber:false, onMove:function(p, temp){ temp.push(ED.temp([makeTable(d, O, p)], { real:true })); } }).then(function(r){ if (r.p) place(makeTable(d, O, r.p)); res(); }, function(){ res(); });
        return ctx.point('Góc thứ nhất của khung bảng').then(function(a){ if (!a.p) return res();
          var fit = function(q){ return makeTable(d, O, [Math.min(a.p[0], q[0]), Math.max(a.p[1], q[1])], [Math.abs(q[0] - a.p[0]), Math.abs(q[1] - a.p[1])]); };
          return ctx.point('Góc đối của khung bảng', { base:a.p, rubber:false, dyn:'wh', onMove:function(p, temp){ temp.push({ k:'poly', pts:[a.p, [p[0], a.p[1]], p, [a.p[0], p[1]]], closed:true, dash:[4, 3] }); if (Math.abs(p[0] - a.p[0]) > 100 && Math.abs(p[1] - a.p[1]) > 100) temp.push(ED.temp([fit(p)], { real:true })); } }).then(function(q){ if (q.p) place(fit(q.p)); res(); }); }).catch(function(){ res(); });
      });
      styles(); cellSel(); update();
    });
  };

  /* ================= Ô bảng UI-F07: chọn ô trên lưới, phím, sửa tại chỗ, grip, tab ngữ cảnh ================= */
  var CELL = null, EDIT = null, CLIP = null, TG = { list:[], hover:null, drag:null }, pendPaste = 0;
  var FX = [['Tổng', 'Sum'], ['Trung bình', 'Average'], ['Đếm', 'Count'], ['Ô', 'cell'], ['Biểu thức', 'eq']];
  var NUMRE = /^[-+(]?[\d.,\s]+[)%°]?$/;
  function live(){ var d = D(); return !!(CELL && d && d.byId[CELL.e.id] === CELL.e); }
  function clampSel(){ var e = CELL.e; CELL.a = { r:TC.clamp(CELL.a.r, 0, e.rows.length - 1), c:TC.clamp(CELL.a.c, 0, e.cols.length - 1) }; CELL.b = { r:TC.clamp(CELL.b.r, 0, e.rows.length - 1), c:TC.clamp(CELL.b.c, 0, e.cols.length - 1) }; }
  // vùng chọn mở rộng cho trọn các ô gộp cắt qua
  function rng(){ clampSel(); var a = CELL.a, b = CELL.b, s = { r0:Math.min(a.r, b.r), c0:Math.min(a.c, b.c), r1:Math.max(a.r, b.r), c1:Math.max(a.c, b.c) }, grow = true;
    while (grow){ grow = false; (CELL.e.merge || []).forEach(function(m){ var r1 = m[3] != null ? m[3] : m[0]; if (m[0] <= s.r1 && r1 >= s.r0 && m[1] <= s.c1 && m[2] >= s.c0 && (m[0] < s.r0 || r1 > s.r1 || m[1] < s.c0 || m[2] > s.c1)){ s.r0 = Math.min(s.r0, m[0]); s.r1 = Math.max(s.r1, r1); s.c0 = Math.min(s.c0, m[1]); s.c1 = Math.max(s.c1, m[2]); grow = true; } }); }
    return s; }
  function anchors(s){ var out = []; for (var r = s.r0; r <= s.r1; r++) for (var c = s.c0; c <= s.c1; c++){ var p = span(CELL.e, r, c); if (p.r0 === r && p.c0 === c) out.push([r, c]); } return out; }
  function refName(s){ return colName(s.c0) + (s.r0 + 1) + (s.r1 > s.r0 || s.c1 > s.c0 ? ':' + colName(s.c1) + (s.r1 + 1) : ''); }
  function status(){ if (!CELL || EDIT || CELL.pick || CELL.match) return; var s = rng(), n = anchors(s).length, a = CELL.a, t = raw(CELL.e, a.r, a.c); TC.cmdline.status('Ô <b>' + refName(s) + '</b>' + (n > 1 ? ' · ' + n + ' ô' : t.trim()[0] === '=' ? ' · <span class="mono">' + esc(t.trim()) + '</span>' : '') + ' · F2 hoặc nhấp đúp để sửa · Tab, Enter để di chuyển · Esc để thôi chọn ô'); }
  function refreshTab(){ if (!CELL) return; var app = A(); if (app.ctxTab && app.ctxTab.id === 'cell') app.setCtxTab(cellTab(), true); else ED.ctxRefresh(); status(); }
  function startCell(e, r, c){ if (EDIT) EDIT.fin(true); var s = span(e, r, c); CELL = { e:e, a:{ r:s.r0, c:s.c0 }, b:{ r:s.r0, c:s.c0 } }; var d = D(); if (d.sel.length) TC.cad.clearSel(); else { V().drawOver(); TC.emit('sel'); } status(); }
  function endCell(){ if (!CELL) return; if (EDIT) EDIT.fin(true); CELL = null; TG.hover = null; V().drawOver(); ED.ctxRefresh(); }
  SC.tableCell = function(e, r, c, edit){ startCell(e, r || 0, c || 0); if (edit) editCell(); };
  SC.tableCellEnd = endCell;
  // bảng dưới con trỏ: ô chứa điểm, và con trỏ có nằm trên đường kẻ không
  function tableAt(m){
    var d = D(), list = V().spaceEnts();
    for (var i = list.length - 1; i >= 0; i--){ var e = list[i]; if (e.t !== 'table' || !e._p || !TC.entVisible(d, e)) continue; var p = wpt(m, e); if (!p) continue;
      var b = e._bb; if (p[0] < b[0] || p[0] > b[2] || p[1] < b[1] || p[1] > b[3]) continue;
      var g = tgeo(d, e), q = cellAtP(g, p); if (!q) continue; var tol = 4 / scaleOf(e);
      return { e:e, q:q, g:g, line:e._p.some(function(pr){ return pr.k === 'seg' && G.segDist(p, pr.a, pr.b) < tol; }) }; }
    return null;
  }
  function moveSel(dr, dc, ext){ var e = CELL.e, from = ext ? CELL.b : CELL.a, s = span(e, from.r, from.c); if (tsOf(D(), e).dir === 'up') dr = -dr;
    var r = TC.clamp(dr > 0 ? s.r1 + 1 : dr < 0 ? s.r0 - 1 : from.r, 0, e.rows.length - 1), c = TC.clamp(dc > 0 ? s.c1 + 1 : dc < 0 ? s.c0 - 1 : from.c, 0, e.cols.length - 1);
    if (ext) CELL.b = { r:r, c:c }; else { var s2 = span(e, r, c); CELL.a = { r:s2.r0, c:s2.c0 }; CELL.b = { r:s2.r0, c:s2.c0 }; }
    V().drawOver(); refreshTab(); }
  // Tab: sang phải, hết hàng thì xuống đầu hàng sau; Enter: xuống hàng
  function nextCell(dr, dc){ var e = CELL.e, s = span(e, CELL.a.r, CELL.a.c), nc = e.cols.length, nr = e.rows.length, r, c;
    if (dc){ c = dc > 0 ? s.c1 + 1 : s.c0 - 1; r = s.r0; if (c >= nc){ c = 0; r = s.r1 + 1; } if (c < 0){ c = nc - 1; r = s.r0 - 1; } }
    else { if (tsOf(D(), e).dir === 'up') dr = -dr; r = dr > 0 ? s.r1 + 1 : s.r0 - 1; c = s.c0; }
    if (r < 0 || r >= nr) return false; var s2 = span(e, r, c); CELL.a = { r:s2.r0, c:s2.c0 }; CELL.b = { r:s2.r0, c:s2.c0 }; V().drawOver(); refreshTab(); return true; }
  function setRaw(x, r, c, v){ x.cells = x.cells || []; while (x.cells.length <= r) x.cells.push([]); var row = x.cells[r]; while (row.length < x.cols.length) row.push(''); row[c] = v; }
  function dropFields(x, r, c, v){ if (!x.fld) return; x.fld = x.fld.filter(function(f){ return !(f.cell && f.cell[0] === r && f.cell[1] === c) || (v != null && v.indexOf(f.v) >= 0); }); if (!x.fld.length) delete x.fld; }
  function commit(e, r, c, v, flds){
    if (v === raw(e, r, c) && !(flds && flds.length)) return false;
    ED.run('Sửa ô ' + colName(c) + (r + 1), function(){ ED.mod(e, function(x){ setRaw(x, r, c, v); var f = x.cf && x.cf[r + ',' + c]; if (f && f.blk && v){ delete f.blk; } dropFields(x, r, c, v); (flds || []).forEach(function(fl){ if (v.indexOf(fl.v) >= 0) x.fld = (x.fld || []).concat([Object.assign({}, fl, { cell:[r, c] })]); }); }); });
    if (v.trim()[0] === '='){ try { evalF(D(), e, v.trim(), 0); } catch (x){ TC.toast('Công thức ô ' + colName(c) + (r + 1) + ' chưa đúng: ' + esc(String(x)) + '. Ô hiện #LỖI; bấm F2 để sửa.', { type:'warn', log:false }); } }
    return true;
  }
  function addRowEnd(){ var e = CELL.e; ED.run('Thêm hàng', function(){ ED.mod(e, function(x){ if (!x.rs) x.rs = x.rows.map(function(h, i){ return rowKey(x, i); }); x.rows.push(x.rows[x.rows.length - 1]); x.rs.push('data'); x.cells.push(x.cols.map(function(){ return ''; })); }); }); }
  // sửa nội dung ô tại chỗ: Enter xuống ô dưới, Tab sang ô phải (ô cuối thì thêm hàng), Alt+Enter xuống dòng, Ctrl+Enter xong, Esc bỏ
  function editCell(seed){
    if (!live()) return; if (EDIT) EDIT.fin(true);
    var d = D(), e = CELL.e, s = span(e, CELL.a.r, CELL.a.c), r = s.r0, c = s.c0, cs = csOf(d, e, r, c);
    CELL.a = { r:r, c:c }; CELL.b = { r:r, c:c };
    if (/c/.test(cs.lock)){ ctip('Ô ' + colName(c) + (r + 1) + ' đang khóa nội dung. Mở khóa ở Định dạng ô › Khóa.'); return; }
    var q = rectOf(tgeo(d, e), s), orig = raw(e, r, c), st = d.styleMap[cs.ts !== 'Standard' ? cs.ts : (e.style || 'Standard')] || {}, hk = hkOf(tsOf(d, e), e), done = false, flds = [];
    var al = cs.al === 'auto' ? (NUMRE.test(orig.trim()) ? 'MR' : 'ML') : cs.al, cv = $('#cv');
    var ta = TC.el('<textarea class="tbce' + (e._paper ? ' paper' : '') + '" rows="1" spellcheck="false" aria-label="Nội dung ô ' + colName(c) + (r + 1) + '"></textarea>');
    function place(){ if (done) return; var a = spt([q.x0, q.top], e), b = spt([q.x1, q.bot], e); if (!a || !b) return; var px = Math.max(11, cs.th * hk * scaleOf(e) * 1.32);
      ta.style.left = Math.round(a[0]) + 'px'; ta.style.top = Math.round(a[1]) + 'px'; ta.style.width = Math.max(64, Math.round(b[0] - a[0])) + 'px'; ta.style.font = ((cs.b || st.bold) ? '600 ' : '') + px + 'px ' + (st.css || 'Arial, "Helvetica Neue", sans-serif'); ta.style.height = 'auto'; ta.style.height = Math.max(Math.round(b[1] - a[1]), ta.scrollHeight) + 'px'; }
    ta.value = seed != null ? seed : orig; ta.style.textAlign = { L:'left', C:'center', R:'right' }[al[1]] || 'left';
    cv.appendChild(ta); place(); ta.focus(); ta.setSelectionRange(ta.value.length, ta.value.length);
    TC.on('viewchg', place); ta.addEventListener('input', place);
    function fin(ok, dr, dc){ if (done) return; done = true; cv.removeEventListener('mousedown', outside, true); ta.remove(); EDIT = null; var v = ta.value;
      if (ok && live()) commit(e, r, c, v, flds);
      if (live() && (dr || dc)){ if (nextCell(dr, dc)) return editCell(); if (dc > 0 && ok){ addRowEnd(); if (nextCell(0, 1)) return editCell(); } }
      cv.focus({ preventScroll:true }); V().drawOver(); refreshTab(); }
    function outside(ev){ if (ev.target === ta) return; ev.stopImmediatePropagation(); ev.preventDefault(); fin(true); }
    setTimeout(function(){ if (!done) cv.addEventListener('mousedown', outside, true); }, 0);
    ta.addEventListener('keydown', function(ev){ ev.stopPropagation(); var k = ev.key;
      if (k === 'Enter' && ev.altKey){ ev.preventDefault(); ta.setRangeText('\n', ta.selectionStart, ta.selectionEnd, 'end'); place(); return; }
      if (k === 'Enter' && (ev.ctrlKey || ev.metaKey)){ ev.preventDefault(); fin(true); return; }
      if (k === 'Enter'){ ev.preventDefault(); fin(true, ev.shiftKey ? -1 : 1, 0); return; }
      if (k === 'Tab'){ ev.preventDefault(); fin(true, 0, ev.shiftKey ? -1 : 1); return; }
      if (k === 'Escape'){ ev.preventDefault(); fin(false); } });
    EDIT = { ta:ta, fin:fin, insert:function(t, f){ ta.setRangeText(t, ta.selectionStart, ta.selectionEnd, 'end'); if (f) flds.push(f); place(); ta.focus(); } };
    V().drawOver(); refreshTab(); TC.cmdline.status('Đang sửa ô <b>' + colName(c) + (r + 1) + '</b> · Enter: ô dưới · Tab: ô phải · Alt+Enter: xuống dòng · Ctrl+Enter: xong · Esc: bỏ');
  }
  // sao chép, cắt, dán vùng ô; dán chuỗi có tab từ Excel thành nhiều ô
  function gridOf(s, disp){ var d = D(), e = CELL.e, out = []; for (var r = s.r0; r <= s.r1; r++){ var row = []; for (var c = s.c0; c <= s.c1; c++) row.push(disp ? cellShow(d, e, r, c, csOf(d, e, r, c)) : raw(e, r, c)); out.push(row); } return out; }
  function tsv(grid){ return grid.map(function(row){ return row.map(function(v){ return /[\t\n"]/.test(v) ? '"' + v.replace(/"/g, '""') + '"' : v; }).join('\t'); }).join('\r\n'); }
  function parseTSV(t){ var rows = [[]], cur = '', q = false, i, ch; t = String(t).replace(/\r\n?/g, '\n'); for (i = 0; i < t.length; i++){ ch = t[i]; if (q){ if (ch === '"'){ if (t[i + 1] === '"'){ cur += '"'; i++; } else q = false; } else cur += ch; } else if (ch === '"' && cur === '') q = true; else if (ch === '\t'){ rows[rows.length - 1].push(cur); cur = ''; } else if (ch === '\n'){ rows[rows.length - 1].push(cur); cur = ''; rows.push([]); } else cur += ch; } rows[rows.length - 1].push(cur); if (rows.length > 1 && rows[rows.length - 1].length === 1 && rows[rows.length - 1][0] === '') rows.pop(); return rows; }
  function copyCells(cut){ var s = rng(), e = CELL.e, cf = []; for (var r = s.r0; r <= s.r1; r++){ var row = []; for (var c = s.c0; c <= s.c1; c++) row.push(clone(cfOf(e, r, c) || {})); cf.push(row); }
    CLIP = { raw:gridOf(s), cf:cf, tsv:tsv(gridOf(s, true)) }; try { navigator.clipboard.writeText(CLIP.tsv).catch(function(){}); } catch (x){}
    if (cut) clearCells(true); TC.cmdline.msg((cut ? 'Đã cắt ' : 'Đã sao chép ') + anchors(s).length + ' ô (' + refName(s) + '). Ctrl+V để dán vào ô khác, hoặc dán sang Excel.'); }
  function pasteGrid(grid, cf, from){
    var e = CELL.e, s = rng(), R = grid.length, C = Math.max.apply(null, grid.map(function(r){ return r.length; })), addR = Math.max(0, s.r0 + R - e.rows.length), addC = Math.max(0, s.c0 + C - e.cols.length), skip = 0;
    ED.run('Dán ô', function(){ ED.mod(e, function(x){
      if (!x.rs) x.rs = x.rows.map(function(h, i){ return rowKey(x, i); });
      for (var i = 0; i < addR; i++){ x.rows.push(x.rows[x.rows.length - 1]); x.rs.push('data'); x.cells.push(x.cols.map(function(){ return ''; })); }
      for (var j = 0; j < addC; j++){ x.cols.push(x.cols[x.cols.length - 1]); x.cells.forEach(function(row){ row.push(''); }); }
      grid.forEach(function(row, i){ row.forEach(function(v, j){ var r = s.r0 + i, c = s.c0 + j, f = x.cf && x.cf[r + ',' + c]; if (f && /c/.test(f.lock || '')){ skip++; return; } setRaw(x, r, c, v); dropFields(x, r, c, null); if (cf && cf[i] && cf[i][j]){ var nf = clone(cf[i][j]); delete nf.lock; x.cf = x.cf || {}; if (Object.keys(nf).length) x.cf[r + ',' + c] = nf; else delete x.cf[r + ',' + c]; } }); });
    }); });
    CELL.a = { r:s.r0, c:s.c0 }; CELL.b = { r:s.r0 + R - 1, c:s.c0 + C - 1 }; V().drawOver(); refreshTab();
    TC.cmdline.msg('Đã dán ' + R + ' × ' + C + ' ô' + (from ? ' từ ' + from : '') + ' vào ' + refName(rng()) + '.' + (addR || addC ? ' Đã thêm ' + [addR ? addR + ' hàng' : '', addC ? addC + ' cột' : ''].filter(Boolean).join(', ') + ' để đủ chỗ.' : '') + (skip ? ' ' + skip + ' ô khóa nội dung được giữ nguyên.' : ''));
  }
  SC.tablePaste = function(text){ if (!live()) return false; if (CLIP && text === CLIP.tsv) pasteGrid(CLIP.raw, CLIP.cf); else pasteGrid(parseTSV(text), null, /\t/.test(text) ? 'Excel' : null); return true; };
  function pasteSoon(){ pendPaste = 1; setTimeout(function(){ if (!pendPaste || !live()) return; pendPaste = 0; if (CLIP) pasteGrid(CLIP.raw, CLIP.cf); else TC.cmdline.msg('Clipboard chưa có vùng ô. Sao chép ô (Ctrl+C) hoặc vùng ô trong Excel rồi dán lại.'); }, 150); }
  function inField(t){ var g = t && t.tagName; return g === 'INPUT' || g === 'TEXTAREA' || g === 'SELECT' || (t && t.isContentEditable); }
  document.addEventListener('paste', function(ev){ if (!live() || EDIT || inField(ev.target)) return; var t = ev.clipboardData && ev.clipboardData.getData('text/plain'); if (!t) return; pendPaste = 0; ev.preventDefault(); SC.tablePaste(t); });
  ['copy', 'cut'].forEach(function(k){ document.addEventListener(k, function(ev){ if (!live() || EDIT || !CLIP || inField(ev.target) || !ev.clipboardData) return; ev.clipboardData.setData('text/plain', CLIP.tsv); ev.preventDefault(); }); });
  function clearCells(quiet){ var s = rng(), e = CELL.e, d = D(), list = anchors(s).filter(function(a){ return !/c/.test(csOf(d, e, a[0], a[1]).lock) && (raw(e, a[0], a[1]) !== '' || (cfOf(e, a[0], a[1]) || {}).blk); });
    if (!list.length){ if (!quiet) TC.cmdline.msg('Không có nội dung để xóa, hoặc ô đang khóa nội dung.'); return; }
    ED.run('Xóa nội dung ô', function(){ ED.mod(e, function(x){ list.forEach(function(a){ setRaw(x, a[0], a[1], ''); var f = x.cf && x.cf[a[0] + ',' + a[1]]; if (f && f.blk) delete f.blk; dropFields(x, a[0], a[1], null); }); }); }); refreshTab(); }
  // chèn, xóa hàng/cột: dời định dạng ô, ô gộp, field theo chỉ số mới (ax 0 = hàng, 1 = cột)
  function shiftI(i, at, n){ if (n > 0) return i >= at ? i + n : i; return i < at ? i : i < at - n ? null : i + n; }
  function remap(x, ax, at, n){
    if (x.cf){ var cf = {}; Object.keys(x.cf).forEach(function(k){ var p = k.split(',').map(Number), v = shiftI(p[ax], at, n); if (v == null) return; p[ax] = v; cf[p.join(',')] = x.cf[k]; }); x.cf = cf; }
    x.merge = (x.merge || []).map(function(m){ var lo = ax ? m[1] : m[0], hi = ax ? m[2] : (m[3] != null ? m[3] : m[0]), keep = []; for (var i = lo; i <= hi; i++){ var v = shiftI(i, at, n); if (v != null) keep.push(v); } if (!keep.length) return null; var a = Math.min.apply(null, keep), b = Math.max.apply(null, keep); return ax ? [m[0], a, b, m[3]] : [a, m[1], m[2], b]; })
      .filter(function(m){ return m && (m[2] > m[1] || (m[3] != null && m[3] > m[0])); }).map(function(m){ return m[3] == null || m[3] === m[0] ? [m[0], m[1], m[2]] : m; });
    if (x.fld){ x.fld = x.fld.filter(function(f){ if (!f.cell) return true; var v = shiftI(f.cell[ax], at, n); if (v == null) return false; f.cell[ax] = v; return true; }); }
    (x.cells || []).forEach(function(row){ row.forEach(function(v, j){ if (typeof v === 'string' && v.trim()[0] === '=') row[j] = shiftRefs(v, ax, at, n); }); });
  }
  // công thức theo hàng/cột mới; ô bị xóa thành #REF, vùng co lại khi xóa đầu mút
  function shiftRefs(src, ax, at, n){
    var one = function(t, end){ var rc = refRC(t), i, v; if (!rc) return t; i = ax ? rc.c : rc.r; v = shiftI(i, at, n); if (v == null){ if (end == null) return '#REF'; v = end ? at - 1 : at; } if (ax) rc.c = v; else rc.r = v; return colName(rc.c) + (rc.r + 1); };
    return src.replace(/([A-Za-z]+\d+):([A-Za-z]+\d+)|([A-Za-z]+\d+)/g, function(m, a, b, c){ if (c) return one(c); var A = one(a, false), B = one(b, true), ra = refRC(A), rb = refRC(B); return ra && rb && (ax ? ra.c <= rb.c : ra.r <= rb.r) ? A + ':' + B : '#REF'; });
  }
  function headOf(x){ var h = 0; while (h < x.rs.length && x.rs[h] !== 'data') h++; return h; }
  function insRC(ax, after){
    if (EDIT) EDIT.fin(true); var s = rng(), e = CELL.e, n = ax ? s.c1 - s.c0 + 1 : s.r1 - s.r0 + 1, ref = after ? (ax ? s.c1 : s.r1) : (ax ? s.c0 : s.r0), at = after ? ref + 1 : ref;
    ED.run(ax ? 'Chèn cột' : 'Chèn hàng', function(){ ED.mod(e, function(x){
      if (!x.rs) x.rs = x.rows.map(function(h, i){ return rowKey(x, i); });
      var w0 = ax ? x.cols[ref] : x.rows[ref], k0 = x.rs[ref] === 'title' ? 'data' : x.rs[ref]; remap(x, ax, at, n);
      for (var i = 0; i < n; i++){ if (ax){ x.cols.splice(at, 0, w0); x.cells.forEach(function(row){ while (row.length < at) row.push(''); row.splice(at, 0, ''); }); } else { x.rows.splice(at, 0, w0); x.rs.splice(at, 0, k0); x.cells.splice(at, 0, x.cols.map(function(){ return ''; })); } }
      x.head = headOf(x); }); });
    if (!after){ CELL.a = { r:CELL.a.r + (ax ? 0 : n), c:CELL.a.c + (ax ? n : 0) }; CELL.b = { r:CELL.b.r + (ax ? 0 : n), c:CELL.b.c + (ax ? n : 0) }; }
    V().drawOver(); refreshTab(); TC.cmdline.msg('Đã chèn ' + n + (ax ? ' cột ' : ' hàng ') + (after ? (ax ? 'bên phải.' : 'phía dưới.') : (ax ? 'bên trái.' : 'phía trên.')));
  }
  function delRC(ax){
    if (EDIT) EDIT.fin(true); var s = rng(), e = CELL.e, lo = ax ? s.c0 : s.r0, hi = ax ? s.c1 : s.r1, n = hi - lo + 1, filled = 0;
    if (n >= (ax ? e.cols.length : e.rows.length)){ TC.toast('Bảng cần ít nhất một ' + (ax ? 'cột' : 'hàng') + '. Xóa cả bảng bằng lệnh Xóa (ERASE).', { type:'warn', log:false }); return; }
    (e.cells || []).forEach(function(row, r){ row.forEach(function(v, c){ if (String(v == null ? '' : v).trim() && (ax ? c >= lo && c <= hi : r >= lo && r <= hi)) filled++; }); });
    var what = n + (ax ? ' cột' : ' hàng'), go = function(){ ED.run(ax ? 'Xóa cột' : 'Xóa hàng', function(){ ED.mod(e, function(x){ if (!x.rs) x.rs = x.rows.map(function(h, i){ return rowKey(x, i); }); remap(x, ax, lo, -n); if (ax){ x.cols.splice(lo, n); x.cells.forEach(function(row){ row.splice(lo, n); }); } else { x.rows.splice(lo, n); x.rs.splice(lo, n); x.cells.splice(lo, n); } x.head = headOf(x); }); });
      CELL.a = ax ? { r:s.r0, c:lo } : { r:lo, c:s.c0 }; CELL.b = { r:CELL.a.r, c:CELL.a.c }; clampSel(); V().drawOver(); refreshTab(); TC.cmdline.msg('Đã xóa ' + what + '.'); };
    if (filled) TC.confirm({ title:'Xóa ' + (ax ? 'cột' : 'hàng'), msg:'Xóa ' + what + ' (' + (ax ? colName(lo) + (n > 1 ? '–' + colName(hi) : '') : (lo + 1) + (n > 1 ? '–' + (hi + 1) : '')) + ') khỏi bảng?', detail:filled + ' ô có nội dung sẽ bị xóa. Hoàn tác được bằng Ctrl+Z.', ok:'Xóa ' + what, danger:true }).then(function(y){ if (y) go(); });
    else go();
  }
  function doMerge(mode){
    if (EDIT) EDIT.fin(true); var s = rng(), e = CELL.e, groups = [], r, c;
    if (mode === 'all') groups.push(s); else if (mode === 'row') for (r = s.r0; r <= s.r1; r++) groups.push({ r0:r, r1:r, c0:s.c0, c1:s.c1 }); else for (c = s.c0; c <= s.c1; c++) groups.push({ r0:s.r0, r1:s.r1, c0:c, c1:c });
    groups = groups.filter(function(g){ return g.r1 > g.r0 || g.c1 > g.c0; });
    if (!groups.length){ TC.cmdline.msg('Chọn từ hai ô trở lên ' + (mode === 'row' ? 'trên một hàng ' : mode === 'col' ? 'trên một cột ' : '') + 'để gộp. Kéo chuột hoặc Shift + mũi tên để chọn nhiều ô.'); return; }
    var lost = 0; groups.forEach(function(g){ for (var r2 = g.r0; r2 <= g.r1; r2++) for (var c2 = g.c0; c2 <= g.c1; c2++) if ((r2 !== g.r0 || c2 !== g.c0) && raw(e, r2, c2).trim()) lost++; });
    var go = function(){ ED.run('Gộp ô', function(){ ED.mod(e, function(x){ x.merge = (x.merge || []).filter(function(m){ var r1 = m[3] != null ? m[3] : m[0]; return !(m[0] >= s.r0 && r1 <= s.r1 && m[1] >= s.c0 && m[2] <= s.c1); });
      groups.forEach(function(g){ x.merge.push(g.r1 > g.r0 ? [g.r0, g.c0, g.c1, g.r1] : [g.r0, g.c0, g.c1]); for (var r3 = g.r0; r3 <= g.r1; r3++) for (var c3 = g.c0; c3 <= g.c1; c3++) if (r3 !== g.r0 || c3 !== g.c0){ setRaw(x, r3, c3, ''); dropFields(x, r3, c3, null); } }); }); });
      CELL.a = { r:s.r0, c:s.c0 }; CELL.b = { r:s.r1, c:s.c1 }; V().drawOver(); refreshTab(); TC.cmdline.msg('Đã gộp thành ' + groups.length + ' ô.'); };
    if (lost) TC.confirm({ title:'Gộp ô', msg:'Gộp ' + anchors(s).length + ' ô thành ' + groups.length + ' ô. Mỗi ô gộp chỉ giữ nội dung của ô trên trái.', detail:lost + ' ô khác có nội dung sẽ bị xóa. Hoàn tác được bằng Ctrl+Z.', ok:'Gộp ' + anchors(s).length + ' ô' }).then(function(y){ if (y) go(); });
    else go();
  }
  function unmerge(){ if (EDIT) EDIT.fin(true); var s = rng(), e = CELL.e, hit = (e.merge || []).filter(function(m){ var r1 = m[3] != null ? m[3] : m[0]; return m[0] <= s.r1 && r1 >= s.r0 && m[1] <= s.c1 && m[2] >= s.c0; });
    if (!hit.length){ TC.cmdline.msg('Vùng chọn không có ô gộp.'); return; }
    ED.run('Tách ô', function(){ ED.mod(e, function(x){ x.merge = x.merge.filter(function(m){ return !hit.some(function(h){ return JSON.stringify(h) === JSON.stringify(m); }); }); }); }); V().drawOver(); refreshTab(); TC.cmdline.msg('Đã tách ' + hit.length + ' ô gộp.'); }
  // đổi định dạng các ô đang chọn (bỏ qua ô khóa định dạng)
  function setFmt(label, fn, lockOp){
    if (EDIT) EDIT.fin(true); var s = rng(), e = CELL.e, d = D(), list = anchors(s), ok = list.filter(function(a){ return lockOp || !/f/.test(csOf(d, e, a[0], a[1]).lock); });
    if (!ok.length){ TC.toast('Các ô đã chọn đang khóa định dạng. Mở khóa ở Định dạng ô › Khóa.', { type:'warn', log:false }); return; }
    ED.run(label, function(){ ED.mod(e, function(x){ x.cf = x.cf || {}; ok.forEach(function(a){ var k = a[0] + ',' + a[1], f = x.cf[k] || {}; fn(f, a); Object.keys(f).forEach(function(z){ if (f[z] === undefined || (f[z] === null && z !== 'bg')) delete f[z]; }); if (Object.keys(f).length) x.cf[k] = f; else delete x.cf[k]; }); if (!Object.keys(x.cf).length) delete x.cf; }); });
    if (ok.length < list.length) TC.cmdline.msg((list.length - ok.length) + ' ô đang khóa định dạng, giữ nguyên.');
    V().drawOver(); refreshTab();
  }
  function menuAt(b, items){ var r = b.getBoundingClientRect(); TC.menu(items, r.left, r.bottom + 2, { returnFocus:b, minWidth:Math.max(r.width, 180) }); }
  function curCs(){ return csOf(D(), CELL.e, CELL.a.r, CELL.a.c); }
  function alignItems(){ var cs = curCs(); return ALN.map(function(a){ return { label:a[1] + ' <span class="faint">· ' + a[0] + '</span>', check:cs.al === a[0], act:function(){ setFmt('Canh lề ô', function(f){ f.al = a[0]; }); } }; }).concat([{ sep:1 }, { label:'Theo kiểu ô', act:function(){ setFmt('Canh lề ô', function(f){ delete f.al; }); } }]); }
  function lockItems(){ var cs = curCs(); return [['Mở khóa', ''], ['Khóa nội dung', 'c'], ['Khóa định dạng', 'f'], ['Khóa nội dung và định dạng', 'cf']].map(function(l){ return { label:l[0], check:cs.lock === l[1], act:function(){ setFmt(l[1] ? 'Khóa ô' : 'Mở khóa ô', function(f){ f.lock = l[1] || undefined; }, true); } }; }); }
  function dtItems(){ var cs = curCs(), P = ['0', '0.0', '0.00', '0.000', '0.0000']; return DTS.map(function(dt){ var set = function(p){ return function(){ setFmt('Kiểu dữ liệu ô', function(f){ f.dt = dt; if (p != null) f.prec = p; }); }; };
    return /thập phân|Phần trăm|Góc/.test(dt) ? { label:dt, check:cs.dt === dt, sub:P.map(function(x, i){ return { label:'<span class="mono">' + x + '</span>', check:cs.dt === dt && cs.prec === i, act:set(i) }; }) } : { label:dt, check:cs.dt === dt, act:set(null) }; }).concat([{ sep:1 }, { label:'Theo kiểu ô', act:function(){ setFmt('Kiểu dữ liệu ô', function(f){ delete f.dt; delete f.prec; }); } }]); }
  function bgItems(){ var cs = curCs(), f0 = cfOf(CELL.e, CELL.a.r, CELL.a.c) || {}, set = function(v){ return function(){ setFmt('Màu nền ô', function(f){ if (v === undefined) delete f.bg; else f.bg = v; }); }; };
    return [{ label:'Theo kiểu ô', check:f0.bg === undefined, act:set(undefined) }, { label:'Không', check:f0.bg === null, act:set(null) }, { sep:1 }].concat([1, 2, 3, 4, 5, 6, 8, 9, 251, 252, 253, 254].map(function(n){ return { label:'<i class="sw" style="background:' + swHex(n) + '"></i>' + TC.aciName(n), check:cs.bg === n, act:set(n) }; })).concat([{ sep:1 }, { label:'Chọn màu khác…', act:function(){ SC.colorPicker(cs.bg).then(function(c){ if (typeof c === 'number') set(c)(); }); } }]); }
  function csItems(){ var d = D(), e = CELL.e, ts = tsOf(d, e), cs = curCs(); return Object.keys(ts.cells).map(function(k){ return { label:esc(CSN[k] || k), check:cs.key === k, act:function(){ setFmt('Kiểu ô', function(f){ if (k === rowKey(e, CELL.a.r)) delete f.st; else f.st = k; }); } }; }).concat([{ sep:1 }, { label:'Quản lý kiểu ô (Kiểu bảng)…', act:function(){ TC.cmd.run('TABLESTYLE', e.tstyle || 'Standard'); } }]); }
  function fxItems(){ return FX.map(function(f){ return { label:f[0] + (f[1] !== 'cell' && f[1] !== 'eq' ? ' <span class="faint mono">' + f[1] + '()</span>' : ''), act:function(){ if (f[1] === 'eq') editCell('='); else startPick(f[1]); } }; }); }
  // khớp ô: lấy định dạng ô hiện tại, bấm các ô đích để áp dụng
  function startMatch(){ if (EDIT) EDIT.fin(true); var d = D(), e = CELL.e, a = CELL.a, f = clone(cfOf(e, a.r, a.c) || {}); delete f.lock; delete f.blk; f.st = csOf(d, e, a.r, a.c).key; CELL.match = f; CELL.pick = null; TC.cmdline.msg('Khớp ô: đã lấy định dạng ô <b>' + colName(a.c) + (a.r + 1) + '</b>. Bấm hoặc kéo trên các ô đích. Esc để kết thúc.'); V().drawOver(); }
  // công thức: chọn vùng ô bằng chuột (gợi ý sẵn dãy số liền phía trên), Enter để dùng gợi ý
  function suggest(e, t){ var d = D(), r = t.r - 1, r1 = r; while (r >= 0 && cellNum(d, e, r, t.c, 0) != null) r--; if (r1 > r) return { r0:r + 1, r1:r1, c0:t.c, c1:t.c }; var c = t.c - 1, c1 = c; while (c >= 0 && cellNum(d, e, t.r, c, 0) != null) c--; return c1 > c ? { r0:t.r, r1:t.r, c0:c + 1, c1:c1 } : null; }
  function startPick(fn){
    if (EDIT) EDIT.fin(true); var e = CELL.e, s = rng(), t = { r:s.r0, c:s.c0 }; if (/c/.test(curCs().lock)){ ctip('Ô đang khóa nội dung.'); return; }
    var sug = fn === 'cell' ? null : (anchors(s).length > 1 ? null : suggest(e, t)), name = (FX.filter(function(f){ return f[1] === fn; })[0] || [fn])[0];
    if (anchors(s).length > 1 && fn !== 'cell'){ t = { r:s.r1 + 1 <= e.rows.length - 1 ? s.r1 + 1 : s.r1, c:s.c0 }; sug = s; }
    CELL.pick = { fn:fn, t:t, sug:sug }; CELL.match = null; if (sug){ CELL.a = { r:sug.r0, c:sug.c0 }; CELL.b = { r:sug.r1, c:sug.c1 }; }
    TC.cmdline.msg(name + ' cho ô <b>' + colName(t.c) + (t.r + 1) + '</b>: ' + (fn === 'cell' ? 'bấm ô cần tham chiếu' : 'bấm ô đầu rồi kéo tới ô cuối của vùng') + (sug ? '; Enter để dùng vùng gợi ý <b>' + refName(sug) + '</b>' : '') + '. Esc để hủy.'); V().drawOver();
  }
  function finishPick(){ var P0 = CELL.pick, s = rng(), e = CELL.e; CELL.pick = null; if (!P0) return;
    var f = P0.fn === 'cell' ? '=' + colName(s.c0) + (s.r0 + 1) : '=' + P0.fn + '(' + refName(s) + ')';
    if (s.r0 <= P0.t.r && s.r1 >= P0.t.r && s.c0 <= P0.t.c && s.c1 >= P0.t.c){ CELL.a = P0.t; CELL.b = { r:P0.t.r, c:P0.t.c }; TC.toast('Vùng ' + refName(s) + ' chứa chính ô ' + colName(P0.t.c) + (P0.t.r + 1) + '. Chọn vùng khác để tránh tham chiếu vòng.', { type:'warn', log:false }); V().drawOver(); return; }
    CELL.a = { r:P0.t.r, c:P0.t.c }; CELL.b = { r:P0.t.r, c:P0.t.c }; commit(e, P0.t.r, P0.t.c, f);
    TC.cmdline.msg('Ô <b>' + colName(P0.t.c) + (P0.t.r + 1) + '</b> = <span class="mono">' + esc(f.slice(1)) + '</span> = <b>' + esc(SC.tableEval(e, P0.t.r, P0.t.c)) + '</b>'); V().drawOver(); refreshTab(); }
  // bấm trên lưới bảng: chọn ô, kéo để chọn nhiều ô, Shift để mở rộng; đang khớp ô/chọn vùng công thức thì dùng cho việc đó
  function cellDown(m, ev){
    var d = D(); if (!d || TC.isViewer() || TC.cmdline.busy() || d.cmp) return false;
    var hit = tableAt(m);
    if (!hit || (hit.line && !(CELL && CELL.e === hit.e))){ if (CELL) endCell(); return false; }
    if ((d.layerMap[hit.e.layer] || {}).lck){ ctip('Bảng nằm trên layer bị khóa'); return true; }
    var e = hit.e, q = hit.q, mode = CELL && CELL.e === e ? (CELL.pick ? 'pick' : CELL.match ? 'match' : ev.shiftKey ? 'ext' : '') : '';
    if (!mode) startCell(e, q.r, q.c); else if (mode === 'ext') CELL.b = { r:q.r, c:q.c }; else { CELL.a = { r:q.r, c:q.c }; CELL.b = { r:q.r, c:q.c }; }
    V().drawOver();
    if (mode === 'pick' && CELL.pick.fn === 'cell'){ finishPick(); return true; }
    var mv = function(ev2){ var h2 = tableAt(local(ev2)); if (h2 && h2.e === e && CELL && (h2.q.r !== CELL.b.r || h2.q.c !== CELL.b.c)){ CELL.b = { r:h2.q.r, c:h2.q.c }; V().drawOver(); } };
    var up = function(){ removeEventListener('mousemove', mv); removeEventListener('mouseup', up); if (!CELL) return; if (mode === 'pick') finishPick(); else if (mode === 'match'){ var f0 = CELL.match; setFmt('Khớp ô', function(f){ Object.keys(f).forEach(function(k){ if (k !== 'lock') delete f[k]; }); Object.assign(f, clone(f0)); if (f.st === rowKey(e, CELL.a.r)) delete f.st; }); CELL.match = f0; } else refreshTab(); };
    addEventListener('mousemove', mv); addEventListener('mouseup', up);
    return true;
  }
  // grip: cạnh phải, cạnh dưới của vùng ô (đổi rộng cột, cao hàng); khi chọn cả bảng: grip dời bảng và grip đầu mỗi cột
  function startDrag(g, m){
    var d = D(), e = g.e, snap = ED.clean(e), hs0 = (e._rh || e.rows).slice(), p0 = wpt(m, e); if (!p0) return; TG.drag = g;
    function mv(ev){ var m2 = local(ev), p = wpt(m2, e); if (!p) return; var dx = p[0] - p0[0], dy = p[1] - p0[1];
      if (g.k === 'col') e.cols[g.i] = Math.max(e.th * 1.2, Math.round(snap.cols[g.i] + dx)); else if (g.k === 'row') e.rows[g.i] = Math.max(e.th * 1.4, Math.round(hs0[g.i] - dy)); else e.p = [snap.p[0] + dx, snap.p[1] + dy];
      TC.prepEnt(d, e); V().draw(); TC.cad.clearLabels(); TC.cad.label(g.k === 'col' ? 'Rộng cột ' + colName(g.i) + ': ' + TC.d(e.cols[g.i], 0) : g.k === 'row' ? 'Cao hàng ' + (g.i + 1) + ': ' + TC.d(e.rows[g.i], 0) : 'Dời bảng: ' + TC.d(dx, 0) + ', ' + TC.d(dy, 0), m2[0] + 16, m2[1] + 14); }
    function up(){ removeEventListener('mousemove', mv); removeEventListener('mouseup', up); TC.cad.clearLabels(); TG.drag = null; var fin = ED.clean(e), changed = JSON.stringify(fin) !== JSON.stringify(snap);
      Object.keys(e).forEach(function(k){ if (k[0] !== '_' && k !== 'id') delete e[k]; }); Object.assign(e, clone(snap)); TC.prepEnt(d, e);
      if (changed) ED.run(g.k === 'col' ? 'Đổi rộng cột' : g.k === 'row' ? 'Đổi cao hàng' : 'Dời bảng', function(){ ED.mod(e, function(x){ x.cols = fin.cols; x.rows = fin.rows; x.p = fin.p; }); });
      V().draw(); }
    addEventListener('mousemove', mv); addEventListener('mouseup', up);
  }
  function drawCells(ctx){
    var d = D(), e = CELL.e, g = tgeo(d, e), s = rng(), q = rectOf(g, s), A0 = spt([q.x0, q.top], e), B0 = spt([q.x1, q.bot], e); if (!A0 || !B0) return;
    var box = function(x, y, w, h, t, on){ ctx.fillStyle = TC.cssv(on ? '--c-sel-w' : '--c-dyn-bg'); ctx.fillRect(x, y, w, h); ctx.strokeStyle = TC.cssv('--c-dyn-b'); ctx.lineWidth = 1; ctx.strokeRect(x + .5, y + .5, w - 1, h - 1); ctx.fillStyle = TC.cssv(on ? '--c-canvas' : '--c-dyn-t'); ctx.fillText(t, x + w / 2, y + h / 2 + .5); };
    ctx.save(); ctx.font = '500 10px ' + TC.cssv('--f-mono'); ctx.textAlign = 'center'; ctx.textBaseline = 'middle'; ctx.setLineDash([]);
    // chỉ số cột (A, B, C…) phía trên và số hàng bên trái, như bảng tính: dùng cho công thức
    for (var j = 0; j < g.nc; j++){ var a = spt([g.X[j], g.top], e), b = spt([g.X[j + 1], g.top], e); if (b[0] - a[0] >= 8) box(a[0], a[1] - 17, b[0] - a[0], 15, colName(j), j >= s.c0 && j <= s.c1); }
    for (var i = 0; i < g.nr; i++){ var a2 = spt([g.X[0], g.R[i][0]], e), b2 = spt([g.X[0], g.R[i][1]], e); if (b2[1] - a2[1] >= 8) box(a2[0] - 26, a2[1], 24, b2[1] - a2[1], String(i + 1), i >= s.r0 && i <= s.r1); }
    ctx.fillStyle = TC.cssv('--c-sel-wf'); ctx.fillRect(A0[0], A0[1], B0[0] - A0[0], B0[1] - A0[1]);
    ctx.strokeStyle = TC.cssv(CELL.match ? '--c-grip-hover' : '--c-sel-w'); ctx.lineWidth = 2; ctx.setLineDash(CELL.pick ? [6, 3] : []); ctx.strokeRect(A0[0] + 1, A0[1] + 1, B0[0] - A0[0] - 2, B0[1] - A0[1] - 2); ctx.setLineDash([]);
    if (CELL.pick){ var t = rectOf(g, span(e, CELL.pick.t.r, CELL.pick.t.c)), ta = spt([t.x0, t.top], e), tb = spt([t.x1, t.bot], e); ctx.strokeStyle = TC.cssv('--c-snap'); ctx.lineWidth = 2; ctx.strokeRect(ta[0] + 1, ta[1] + 1, tb[0] - ta[0] - 2, tb[1] - ta[1] - 2); ctx.fillStyle = TC.cssv('--c-snap'); ctx.fillText('=', ta[0] + 8, ta[1] + 9); }
    // ô khóa: dấu khóa nhỏ ở góc
    anchors(s).forEach(function(a3){ var cs = csOf(d, e, a3[0], a3[1]); if (!cs.lock) return; var rr = rectOf(g, span(e, a3[0], a3[1])), p = spt([rr.x1, rr.top], e), x = p[0] - 13, y = p[1] + 4; ctx.strokeStyle = ctx.fillStyle = TC.cssv('--c-grip-hover'); ctx.lineWidth = 1.3; ctx.beginPath(); ctx.arc(x + 4, y + 4, 2.6, Math.PI, 0); ctx.stroke(); ctx.fillRect(x, y + 4, 8, 6); });
    ctx.restore();
    if (!EDIT && !CELL.pick && !CELL.match){ var up = tsOf(d, e).dir === 'up'; TG.list.push({ s:[B0[0], (A0[1] + B0[1]) / 2], kind:'sq', k:'col', i:s.c1, e:e }, { s:[(A0[0] + B0[0]) / 2, B0[1]], kind:'sq', k:'row', i:up ? s.r0 : s.r1, e:e }); }
  }
  function tableGrips(e){ var g = tgeo(D(), e), push = function(p, k, i, kind){ var s = spt(p, e); if (s) TG.list.push({ s:s, k:k, i:i, e:e, kind:kind }); }; push([g.X[0], g.top], 'move', -1, 'sq'); for (var j = 1; j <= g.nc; j++) push([g.X[j], g.top], 'col', j - 1, 'tri'); }
  TC.overlays.push(function(ctx){
    TG.list = []; var d = D(); if (!d || TC.isViewer()) return;
    if (CELL && !live()){ CELL = null; EDIT && EDIT.fin(false); setTimeout(function(){ if (!CELL) ED.ctxRefresh(); }, 0); }
    if (CELL) drawCells(ctx);
    else if (!TC.cmdline.busy() && d.sel.length === 1){ var e = d.byId[d.sel[0]]; if (e && e.t === 'table' && !(d.layerMap[e.layer] || {}).lck) tableGrips(e); }
    TG.list.forEach(function(g){ var on = TG.hover && TG.hover.k === g.k && TG.hover.i === g.i && TG.hover.e === g.e; ED.drawGrip(ctx, g, on ? (TG.drag ? 'hot' : 'hover') : 'cold'); });
  });
  var cad = TC.cad, hg0 = cad.hook.grip, hd0 = cad.hook.gripDown, hx0 = cad.hook.ctx, dbl0 = cad.dbl;
  cad.hook.grip = function(m){ if (hg0 && hg0(m)){ TG.hover = null; return true; } var best = null, bd = 8; TG.list.forEach(function(g){ var dd = Math.hypot(g.s[0] - m[0], g.s[1] - m[1]); if (dd < bd){ bd = dd; best = g; } }); var was = TG.hover; TG.hover = best; if (best !== was) V().drawOver(); return !!best; };
  cad.hook.gripDown = function(m, ev){ if (TG.hover && !TC.cmdline.busy() && !TC.isViewer()){ startDrag(TG.hover, m); return true; } if (hd0 && hd0(m, ev)) return true; return cellDown(m, ev); };
  cad.hook.ctx = function(m, ev){ if (hx0 && hx0(m, ev)) return true; if (!CELL || TC.cmdline.busy()) return false; var hit = tableAt(m); if (!hit || hit.e !== CELL.e) return false; var s = rng(); if (!(hit.q.r >= s.r0 && hit.q.r <= s.r1 && hit.q.c >= s.c0 && hit.q.c <= s.c1)) startCell(hit.e, hit.q.r, hit.q.c); cellMenu(ev.clientX, ev.clientY); return true; };
  // nhấp đúp vào ô bảng: sửa nội dung ô tại chỗ (07.1 §9.10)
  cad.dbl = function(m, ev){ var d = D(); if (d && !TC.isViewer() && cad.mode() === 'idle' && !TC.cmdline.busy() && !d.cmp){ var hit = tableAt(m); if (hit && (!hit.line || (CELL && CELL.e === hit.e)) && !(d.layerMap[hit.e.layer] || {}).lck){ var s = span(hit.e, hit.q.r, hit.q.c); if (!CELL || CELL.e !== hit.e || CELL.a.r !== s.r0 || CELL.a.c !== s.c0) startCell(hit.e, hit.q.r, hit.q.c); editCell(); return; } } return dbl0(m, ev); };
  var ctxR0 = ED.ctxRefresh;
  ED.ctxRefresh = function(){ var app = A(); if (live() && app.active !== 'start' && !(app.ctxTab && /^(mte|cmp|arr|hatchc)$/.test(app.ctxTab.id))){ app.setCtxTab(cellTab(), !!(app.ctxTab && app.ctxTab.id === 'cell')); return; } ctxR0(); };
  TC.on('sel', function(){ var d = D(); if (CELL && d && d.sel.length) endCell(); });
  ['doc', 'space', 'edition'].forEach(function(k){ TC.on(k, function(){ if (CELL) endCell(); }); });
  var run0 = TC.cmd.run;
  TC.cmd.run = function(input){ if (CELL && String(input)[0] !== "'") endCell(); return run0.apply(this, arguments); };
  document.addEventListener('keydown', function(ev){
    if (!CELL || EDIT || ev.defaultPrevented || inField(ev.target) || TC.topDialog() || $('.menu') || A().active === 'start') return;
    if (!live()){ endCell(); return; }
    var k = ev.key, ctrl = ev.ctrlKey || ev.metaKey, how = 1, e = CELL.e;
    if (k === 'Escape'){ if (CELL.pick || CELL.match){ CELL.pick = CELL.match = null; TC.cmdline.msg('Đã thôi.'); V().drawOver(); status(); } else endCell(); }
    else if (/^Arrow/.test(k) && !ctrl) moveSel(k === 'ArrowUp' ? -1 : k === 'ArrowDown' ? 1 : 0, k === 'ArrowLeft' ? -1 : k === 'ArrowRight' ? 1 : 0, ev.shiftKey);
    else if (k === 'Tab') nextCell(0, ev.shiftKey ? -1 : 1);
    else if (k === 'Enter'){ if (CELL.pick){ if (CELL.pick.sug){ CELL.a = { r:CELL.pick.sug.r0, c:CELL.pick.sug.c0 }; CELL.b = { r:CELL.pick.sug.r1, c:CELL.pick.sug.c1 }; } finishPick(); } else nextCell(ev.shiftKey ? -1 : 1, 0); }
    else if (k === 'F2') editCell();
    else if (k === 'Delete' || k === 'Backspace') clearCells();
    else if (ctrl && /^[cx]$/i.test(k)){ copyCells(k.toLowerCase() === 'x'); how = 2; }
    else if (ctrl && /^v$/i.test(k)){ pasteSoon(); how = 2; }
    else if (ctrl && /^[zy]$/i.test(k)){ if (k.toLowerCase() === 'z') ED.undo(1); else ED.redo(1); if (live()){ clampSel(); V().drawOver(); refreshTab(); } }
    else if (ctrl && /^a$/i.test(k)){ CELL.a = { r:0, c:0 }; CELL.b = { r:e.rows.length - 1, c:e.cols.length - 1 }; V().drawOver(); refreshTab(); }
    else if (!ctrl && !ev.altKey && k.length === 1) editCell(k);
    else how = 0;
    if (how) ev.stopPropagation(); if (how === 1) ev.preventDefault(); // Ctrl+C, X, V: để trình duyệt phát sự kiện copy, cut, paste
  }, true);
  function cellMenu(x, y){ var s = rng(), e = CELL.e, hasM = (e.merge || []).some(function(m){ var r1 = m[3] != null ? m[3] : m[0]; return m[0] <= s.r1 && r1 >= s.r0 && m[1] <= s.c1 && m[2] >= s.c0; });
    TC.menu([{ label:'Cắt', icon:'cut', kbd:'Ctrl+X', act:function(){ copyCells(true); } }, { label:'Sao chép', icon:'copyclip', kbd:'Ctrl+C', act:function(){ copyCells(false); } }, { label:'Dán', icon:'paste', kbd:'Ctrl+V', dis:!CLIP, act:function(){ pasteGrid(CLIP.raw, CLIP.cf); } }, { sep:1 },
      { label:'Sửa nội dung ô', kbd:'F2', act:function(){ editCell(); } }, { label:'Xóa nội dung', kbd:'Del', act:function(){ clearCells(); } }, { sep:1 },
      { label:'Hàng', sub:[{ label:'Chèn phía trên', icon:'rowabove', act:function(){ insRC(0, false); } }, { label:'Chèn phía dưới', icon:'rowbelow', act:function(){ insRC(0, true); } }, { label:'Xóa hàng', icon:'rowdel', act:function(){ delRC(0); } }] },
      { label:'Cột', sub:[{ label:'Chèn bên trái', icon:'colleft', act:function(){ insRC(1, false); } }, { label:'Chèn bên phải', icon:'colright', act:function(){ insRC(1, true); } }, { label:'Xóa cột', icon:'coldel', act:function(){ delRC(1); } }] },
      { label:'Gộp ô', sub:[['Gộp tất cả', 'all'], ['Gộp theo hàng', 'row'], ['Gộp theo cột', 'col']].map(function(m){ return { label:m[0], act:function(){ doMerge(m[1]); } }; }) }, { label:'Tách ô', dis:!hasM, act:unmerge }, { sep:1 },
      { label:'Canh lề', sub:alignItems() }, { label:'Màu nền', sub:bgItems() }, { label:'Khóa', sub:lockItems() }, { label:'Kiểu dữ liệu', sub:dtItems() }, { label:'Khớp ô', icon:'match', act:startMatch }, { sep:1 },
      { label:'Chèn', sub:[{ label:'Block…', icon:'insert', act:insBlockDlg }, { label:'Field…', icon:'field', act:cellField }, { label:'Công thức', sub:fxItems() }] }, { sep:1 },
      { label:'Kiểu bảng…', icon:'table', act:function(){ TC.cmd.run('TABLESTYLE', e.tstyle || 'Standard'); } }, { label:'Xuất bảng ra Excel', icon:'stats', act:function(){ var id = e.id; endCell(); D().sel = [id]; TC.cmd.run('TCEXCELOUT'); } }], x, y);
  }
  function borderDlg(){
    if (EDIT) EDIT.fin(true); var n = anchors(rng()).length, B = { lw:.25, c:null, dbl:false, sp:.5, edges:'out' };
    var dlg = TC.dialog({ id:'tbbd', title:'Viền ô', code:'UI-F07', w:470, body:'<div class="stack">' + H.row('Độ dày', H.sel('bd-lw', LWS.map(function(w){ return TC.d(w) + ' mm'; }), '0.25 mm', 150), 96) + H.row('Màu', colorBtn('bd-c', null), 96) + H.row('', H.chk('bd-dbl', 'Đường đôi', false), 96) + H.row('Khoảng cách', H.num('bd-sp', '0.50', { w:110, pick:false }), 96) + '<div class="gh">Cạnh áp dụng</div><div class="tbbd" role="group" aria-label="Cạnh áp dụng">' + BDB.map(function(b){ return '<button class="ib" type="button" data-bde="' + b[0] + '" aria-pressed="' + (b[0] === 'out') + '" data-tip="' + b[1] + '" aria-label="' + b[1] + '">' + ic(b[2]) + '</button>'; }).join('') + '</div><p class="faint" id="bd-h" style="font-size:var(--fs-xs);margin:0">Viền ngoài của vùng ' + refName(rng()) + '.</p></div>',
      foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="bd-ok">Áp dụng cho ' + n + ' ô</button>' });
    dlg.el.addEventListener('click', function(ev){ var b = ev.target.closest('[data-bde]'); if (b){ B.edges = b.dataset.bde; $$('[data-bde]', dlg.el).forEach(function(x){ x.setAttribute('aria-pressed', String(x === b)); }); $('#bd-h', dlg.el).textContent = BDB.filter(function(x){ return x[0] === B.edges; })[0][1] + ' của vùng ' + refName(rng()) + '.'; } var cb = ev.target.closest('#bd-c'); if (cb) colorMenu(cb, B.c, null, function(v){ B.c = v; cb.outerHTML = colorBtn('bd-c', v); }); });
    dlg.el.addEventListener('change', function(ev){ if (ev.target.id === 'bd-lw') B.lw = parseFloat(ev.target.value); if (ev.target.id === 'bd-dbl') B.dbl = ev.target.checked; });
    dlg.el.addEventListener('numchange', function(ev){ if (ev.detail > 0) B.sp = ev.detail; });
    $('#bd-ok', dlg.el).addEventListener('click', function(){ dlg.close(true); var s = rng(), rec = { on:B.edges !== 'none', lw:B.lw, c:B.c, lt:'ByBlock', dbl:B.dbl, sp:B.sp }, ks = BDB.filter(function(b){ return b[0] === B.edges; })[0][3];
      setFmt('Sửa viền ô', function(f, a){ var sp = span(CELL.e, a[0], a[1]), side = { t:sp.r0 === s.r0, b:sp.r1 === s.r1, l:sp.c0 === s.c0, r:sp.c1 === s.c1 }; f.bd = f.bd || {};
        ['t', 'b', 'l', 'r'].forEach(function(k){ if (B.edges === 'none' || ks.indexOf(side[k] ? k : (k === 't' || k === 'b' ? 'h' : 'v')) >= 0) f.bd[k] = clone(rec); }); }); });
  }
  function insBlockDlg(){
    if (EDIT) EDIT.fin(true); var d = D(), names = Object.keys(d.blocks).filter(function(n){ return !/^\*|^A\$/.test(n) && !/^KHUNG/.test(n); }).sort();
    if (!names.length){ TC.toast('Bản vẽ chưa có block. Tạo block bằng BLOCK hoặc chèn từ Thư viện TCVN.', { type:'info', log:false }); return; }
    var cur = names.indexOf('CUA-DI-1C') >= 0 ? 'CUA-DI-1C' : names[0], n = anchors(rng()).length;
    var dlg = TC.dialog({ id:'tbblk', title:'Chèn block vào ô', code:'UI-F07', w:520, body:'<div class="grid2"><div class="stack">' + H.row('Tên block', H.sel('bk-n', names, cur, 190), 74) + H.row('', H.chk('bk-fit', 'Tự co giãn vừa ô', true, true), 74) + '<p class="faint" style="font-size:var(--fs-s);margin:0">Block canh giữa ô, giữ tỉ lệ, chừa lề của kiểu ô. Gõ chữ vào ô sẽ thay block.</p></div><div class="stack"><span class="gh">Xem trước</span><canvas id="bk-pv" width="210" height="140" class="pvc"></canvas></div></div>', foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="bk-ok">Chèn block vào ' + n + ' ô</button>' });
    var pv = function(){ SC.blockThumb($('#bk-pv', dlg.el), d, $('#bk-n', dlg.el).value); }; pv();
    $('#bk-n', dlg.el).addEventListener('change', pv);
    $('#bk-ok', dlg.el).addEventListener('click', function(){ var nm = $('#bk-n', dlg.el).value; dlg.close(true); setFmt('Chèn block vào ô', function(f, a){ f.blk = nm; }); });
  }
  function cellField(){
    if (!CELL) return; var e = CELL.e, a = { r:CELL.a.r, c:CELL.a.c }, ed = EDIT;
    if (/c/.test(curCs().lock)){ ctip('Ô đang khóa nội dung.'); return; }
    SC.fieldDlg({ title:'Chèn field vào ô ' + colName(a.c) + (a.r + 1) }).then(function(f){ if (!f) return;
      if (ed && EDIT === ed){ ed.insert(f.v, f); return; }
      ED.run('Chèn field vào ô', function(){ ED.mod(e, function(x){ setRaw(x, a.r, a.c, f.v); dropFields(x, a.r, a.c, null); x.fld = (x.fld || []).concat([Object.assign({}, f, { cell:[a.r, a.c] })]); }); }); refreshTab(); });
  }
  function cellTab(){
    var d = D(), e = CELL.e, a = CELL.a, cs = csOf(d, e, a.r, a.c), f0 = cfOf(e, a.r, a.c) || {};
    var S_ = function(label, icon, id, act, x){ return Object.assign({ t:'s', label:label, icon:icon, id:id, act:act }, x || {}); };
    var later = function(w){ return function(){ TC.toast('<b>' + w + '</b> (liên kết dữ liệu với Excel) có sau v1.0.', { type:'info', log:false }); }; };
    return { id:'cell', name:'Ô bảng', ctx:true, panels:[
      { name:'Hàng', icon:'rowabove', prio:1, items:[ { t:'col', items:[ S_('Chèn phía trên', 'rowabove', 'cl-ra', function(){ insRC(0, false); }), S_('Chèn phía dưới', 'rowbelow', 'cl-rb', function(){ insRC(0, true); }), S_('Xóa hàng', 'rowdel', 'cl-rd', function(){ delRC(0); }) ] } ] },
      { name:'Cột', icon:'colleft', prio:1, items:[ { t:'col', items:[ S_('Chèn bên trái', 'colleft', 'cl-cl', function(){ insRC(1, false); }), S_('Chèn bên phải', 'colright', 'cl-cr', function(){ insRC(1, true); }), S_('Xóa cột', 'coldel', 'cl-cd', function(){ delRC(1); }) ] } ] },
      { name:'Gộp', icon:'merge', prio:2, items:[ { t:'lg', label:'Gộp ô', icon:'merge', id:'cl-mg', menuOnly:true, menu:function(){ return [['Gộp tất cả', 'all'], ['Gộp theo hàng', 'row'], ['Gộp theo cột', 'col']].map(function(m){ return { label:m[0], act:function(){ doMerge(m[1]); } }; }); } }, { t:'lg', label:'Tách ô', icon:'unmerge', id:'cl-um', act:unmerge } ] },
      { name:'Kiểu ô', icon:'table', prio:2, items:[ { t:'col', gap:3, items:[
        { t:'row', items:[ { t:'io', label:'Khớp ô', icon:'match', id:'cl-mt', act:startMatch, desc:'Lấy định dạng ô đang chọn, rồi bấm các ô đích để áp dụng.' }, { t:'dd', id:'cl-cs', label:'Kiểu ô', w:120, text:function(){ return CSN[cs.key] || cs.key; }, act:function(b){ menuAt(b, csItems()); } } ] },
        { t:'dd', id:'cl-bg', label:'Màu nền', w:148, html:function(){ return '<i class="sw" style="background:' + swHex(cs.bg) + '"></i>'; }, text:function(){ return f0.bg === undefined ? 'Theo kiểu ô' : colLbl(cs.bg, 'Không'); }, act:function(b){ menuAt(b, bgItems()); } },
        S_('Sửa viền', 'borders', 'cl-bd', borderDlg) ] } ] },
      { name:'Định dạng ô', icon:'justify', prio:2, items:[ { t:'col', items:[ S_('Canh lề', 'justify', 'cl-al', null, { menuOnly:true, menu:alignItems }), S_(cs.lock ? 'Đang khóa' : 'Khóa', cs.lock ? 'lock' : 'unlock', 'cl-lk', null, { menuOnly:true, menu:lockItems }), S_('Kiểu dữ liệu', 'pct', 'cl-dt', null, { menuOnly:true, menu:dtItems }) ] } ] },
      { name:'Chèn', icon:'insert', prio:3, items:[ { t:'lg', label:'Block', icon:'insert', id:'cl-blk', act:insBlockDlg }, { t:'lg', label:'Field', icon:'field', id:'cl-fld', act:cellField }, { t:'lg', label:'Công\nthức', icon:'fx', id:'cl-fx', menuOnly:true, menu:fxItems } ] },
      { name:'Dữ liệu', icon:'ext', prio:4, items:[ { t:'col', items:[ S_('Liên kết ô', 'ext', 'cl-dl', later('Liên kết ô'), { desc:'Có sau v1.0.' }), S_('Tải lại từ nguồn', 'refresh', 'cl-dr', later('Tải lại từ nguồn'), { desc:'Có sau v1.0.' }) ] } ] }
    ] };
  }
  TC.cad.ctxExt.push(function(it, first){ if (!first || TC.isViewer() || first.t !== 'table') return; it.push({ label:'Sửa ô bảng', act:function(){ startCell(first, 0, 0); } }, { label:'Kiểu bảng…', icon:'table', act:function(){ TC.cmd.run('TABLESTYLE', first.tstyle || 'Standard'); } }); });

  /* ================= Field UI-F08, UPDATEFIELD ================= */
  TC.S.fieldBg = TC.load('fieldbg', true);
  var FG = ['Tất cả', 'Ngày giờ', 'Tài liệu', 'Liên kết', 'Đối tượng', 'Khác', 'In', 'Bộ bản vẽ'];
  // [mã AutoCAD, tên, nhóm, loại giá trị]
  var FLD = [['Author', 'Tác giả', 'Tài liệu', 'txt'], ['Comments', 'Ghi chú', 'Tài liệu', 'txt'], ['CreateDate', 'Ngày tạo', 'Ngày giờ', 'date'], ['CustomDP', 'Thuộc tính tùy biến', 'Tài liệu', 'cp'], ['Date', 'Ngày', 'Ngày giờ', 'date'], ['Filename', 'Tên tệp', 'Tài liệu', 'file'], ['Filesize', 'Dung lượng', 'Tài liệu', 'size'], ['Formula', 'Công thức', 'Khác', 'fx'], ['Hyperlink', 'Hyperlink', 'Liên kết', 'hl'], ['Keywords', 'Từ khóa', 'Tài liệu', 'txt'], ['LastSavedBy', 'Người lưu cuối', 'Tài liệu', 'txt'], ['NamedObject', 'Đối tượng có tên', 'Đối tượng', 'named'], ['Object', 'Đối tượng', 'Đối tượng', 'obj'], ['PageSetupName', 'Tên thiết lập trang', 'In', 'txt'], ['PaperSize', 'Khổ giấy', 'In', 'txt'], ['PlotDate', 'Ngày in', 'In', 'date'], ['PlotOrientation', 'Hướng in', 'In', 'txt'], ['PlotScale', 'Tỉ lệ in', 'In', 'txt'], ['PlotStyleTable', 'Plot style', 'In', 'txt'], ['SaveDate', 'Ngày lưu', 'Ngày giờ', 'date'], ['SheetNumber', 'Số tờ', 'Bộ bản vẽ', 'txt'], ['SheetTotal', 'Tổng số tờ', 'Bộ bản vẽ', 'txt'], ['SheetNumTotal', 'Số tờ / Tổng số tờ', 'Bộ bản vẽ', 'txt'], ['SheetTitle', 'Tên tờ', 'Bộ bản vẽ', 'txt'], ['SheetSetName', 'Tên bộ bản vẽ', 'Bộ bản vẽ', 'txt'], ['Subject', 'Chủ đề', 'Tài liệu', 'txt'], ['SystemVariable', 'Biến hệ thống', 'Khác', 'sv'], ['Title', 'Tiêu đề', 'Tài liệu', 'txt']];
  var CASES = ['(không)', 'CHỮ HOA', 'chữ thường', 'Viết Hoa Chữ Đầu', 'Viết hoa đầu câu'];
  var DATEF = ['dd/MM/yyyy', 'd/M/yyyy', 'dd/MM/yy', 'dd-MM-yyyy', 'yyyy-MM-dd', "'ngày' dd 'tháng' MM 'năm' yyyy", 'dd/MM/yyyy HH:mm', 'HH:mm'];
  var UNITS = { area:[['m²', 1e-6], ['mm²', 1], ['cm²', 1e-2]], len:[['mm', 1], ['m', 1e-3], ['cm', .1]] };
  function info(d){ return d.info || (d.info = { title:'Nhà ở gia đình 2 tầng', subject:'Hồ sơ thiết kế kiến trúc', author:'Nguyễn Minh An', kw:'nhà ở, 2 tầng, An Phú', notes:'', base:'', custom:[['DUAN', 'NHÀ Ở GIA ĐÌNH 2 TẦNG'], ['CHUDAUTU', 'Ông Trần Văn Hùng'], ['GIAIDOAN', 'Thiết kế kỹ thuật'], ['SOHD', 'HĐ-2026/118']] }); }
  function pdate(s){ var m = /(\d{1,2})\/(\d{1,2})\/(\d{4})(?:\s+(\d{1,2}):(\d{2}))?/.exec(s || ''); return m ? new Date(+m[3], m[2] - 1, +m[1], +(m[4] || 0), +(m[5] || 0)) : new Date(); }
  function fmtDate(dt, pat){ var p2 = function(x){ return (x < 10 ? '0' : '') + x; }; return String(pat || 'dd/MM/yyyy').split(/('[^']*')/).map(function(part){ if (/^'.*'$/.test(part)) return part.slice(1, -1); return part.replace(/yyyy|yy|MM|M|dd|d|HH|H|mm/g, function(t){ return { yyyy:dt.getFullYear(), yy:String(dt.getFullYear()).slice(2), MM:p2(dt.getMonth() + 1), M:dt.getMonth() + 1, dd:p2(dt.getDate()), d:dt.getDate(), HH:p2(dt.getHours()), H:dt.getHours(), mm:p2(dt.getMinutes()) }[t]; }); }).join(''); }
  function fcase(s, f){ s = String(s); return f === 'CHỮ HOA' ? s.toUpperCase() : f === 'chữ thường' ? s.toLowerCase() : f === 'Viết Hoa Chữ Đầu' ? s.toLowerCase().replace(/(^|\s)(\S)/g, function(m, a, b){ return a + b.toUpperCase(); }) : f === 'Viết hoa đầu câu' ? s.toLowerCase().replace(/^\s*\S/, function(m){ return m.toUpperCase(); }) : s; }
  // tờ hiện hành trong bộ bản vẽ: số hiệu, tên lấy từ khung tên của layout (bộ bản vẽ mẫu 12 tờ)
  function sheetOf(d){ var L = d.space !== 'model' ? d.layouts[d.space] : d.layouts[0] || null, k = L && L.ents.filter(function(e){ return e.t === 'insert' && /^KHUNG/.test(e.name); })[0], a = (k && k.attrs) || {}, ps = (L && d.pageSetups.filter(function(p){ return p.name === L.setup; })[0]) || d.pageSetups[0] || {};
    return { L:L, num:a.KYHIEU || 'KT-01', title:a.TENBV || (L ? L.name : ''), total:12, set:'Hồ sơ kiến trúc ' + d.name.replace(/\.\w+$/, ''), ps:ps }; }
  function objProps(e){ var L = ['layer', 'Layer', 'txt']; switch (e.t){
      case 'pline': return (e.closed ? [['area', 'Diện tích', 'area']] : []).concat([['len', 'Độ dài', 'len'], L]);
      case 'circle': return [['area', 'Diện tích', 'area'], ['r', 'Bán kính', 'len'], ['dia', 'Đường kính', 'len'], ['len', 'Chu vi', 'len'], L];
      case 'hatch': return [['area', 'Diện tích', 'area'], ['pat', 'Tên mẫu', 'txt'], L];
      case 'line': case 'arc': return [['len', 'Độ dài', 'len'], L];
      case 'text': case 'mtext': case 'attdef': return [['s', 'Nội dung', 'txt'], ['h', 'Chiều cao', 'len'], ['style', 'Kiểu chữ', 'txt'], L];
      case 'insert': return [['name', 'Tên block', 'txt']].concat(Object.keys(e.attrs || {}).map(function(t){ return ['attr:' + t, 'Attribute ' + t, 'txt']; })).concat([L]);
      case 'dim': return [['meas', 'Giá trị đo', 'len'], L];
      default: return [L]; } }
  function objRaw(d, e, p){ if (p === 'area') return e.t === 'circle' ? Math.PI * e.r * e.r : e.t === 'hatch' ? Math.abs(e._p[0].loops.reduce(function(s, l, k){ return s + (k ? -1 : 1) * G.polyArea(l); }, 0)) : G.polyArea(e.pts || []);
    if (p === 'len') return e.t === 'line' ? G.dist(e.p1, e.p2) : e.t === 'circle' ? 2 * Math.PI * e.r : e.t === 'arc' ? e.r * ((((e.a1 - e.a0) % 360) + 360) % 360) * Math.PI / 180 : G.polyLen(e.pts || [], e.closed);
    if (p === 'r') return e.r; if (p === 'dia') return e.r * 2; if (p === 'h') return e.h; if (p === 'meas') return e._meas || 0;
    if (/^attr:/.test(p)) return (e.attrs || {})[p.slice(5)] || ''; return e[p] == null ? '' : e[p]; }
  function fval(d, f){
    var o = f.o || {}, I = info(d), sh = sheetOf(d), v, dt = function(s){ return fmtDate(s ? pdate(s) : new Date(), f.f && f.f !== '(không)' ? f.f : 'dd/MM/yyyy'); };
    switch (f.k){
      case 'Author': v = I.author; break; case 'Comments': v = I.notes; break; case 'Keywords': v = I.kw; break; case 'Subject': v = I.subject; break; case 'Title': v = I.title; break; case 'LastSavedBy': v = I.lastBy || I.author; break;
      case 'Hyperlink': v = o.link ? o.link.t || o.link.a : ''; break;
      case 'CustomDP': v = ((I.custom || []).filter(function(c){ return c[0] === o.cp; })[0] || [])[1]; break;
      case 'Date': return dt(null); case 'CreateDate': return dt(d.created || '02/09/2026 08:15'); case 'SaveDate': return dt(d.modified); case 'PlotDate': return dt(d.plotted || '18/10/2026 16:05');
      case 'Filename': v = o.path ? d.path : d.name; if (!o.ext) v = v.replace(/\.(dwg|dxf|dwt)$/i, ''); break;
      case 'Filesize': v = o.unit === 'KB' ? Math.round(d.bytes / 1024) + ' KB' : TC.d(d.bytes / 1048576, 1) + ' MB'; break;
      case 'PageSetupName': v = sh.ps.name || ''; break; case 'PaperSize': v = String(sh.L ? sh.L.paper.name : sh.ps.paper || '').replace(/(\d),(\d)/g, '$1.$2'); break; case 'PlotOrientation': v = sh.L && sh.L.paper.w < sh.L.paper.h ? 'Dọc' : 'Ngang'; break; case 'PlotScale': v = sh.ps.scale || '1:1'; break; case 'PlotStyleTable': v = sh.ps.ctb || 'TCVN.ctb'; break;
      case 'SheetNumber': v = sh.num; break; case 'SheetTotal': v = String(sh.total); break; case 'SheetNumTotal': v = (sh.num.match(/\d+$/) || ['01'])[0] + '/' + sh.total; break; case 'SheetTitle': v = sh.title; break; case 'SheetSetName': v = sh.set; break;
      case 'NamedObject': v = o.nm || ''; break;
      case 'SystemVariable': v = ((TC.SYSVARS || []).filter(function(s){ return s[0] === o.sv; })[0] || [])[2] || ''; break;
      case 'Formula': try { v = TC.d(evalF(d, { cells:[] }, o.ex || '0', 0), o.prec != null ? o.prec : 2); } catch (x){ v = '#LỖI'; } break;
      case 'Object': { var e = d.byId[o.id]; if (!e) return '####'; var p = (objProps(e).filter(function(x){ return x[0] === o.prop; })[0] || [])[2], r = objRaw(d, e, o.prop);
        if (p === 'area' || p === 'len'){ var u = (UNITS[p].filter(function(x){ return x[0] === o.u; })[0] || UNITS[p][0]); v = (o.pre || '') + TC.d(r * u[1], o.prec != null ? o.prec : 2) + (o.suf != null ? o.suf : ' ' + u[0]); } else v = r; break; }
      default: v = '';
    }
    return fcase(v == null ? '' : v, f.f);
  }
  function fcode(f){ var o = f.o || {}, fm = f.f && f.f !== '(không)' ? ' \\f "' + f.f + '"' : '';
    if (f.k === 'Object') return '%<\\AcObjProp Object(%<\\_ObjId ' + (o.id || 0) + '>%).' + (o.prop || '') + (o.u ? ' \\f "%lu2%pr' + (o.prec != null ? o.prec : 2) + (o.u === 'm²' ? '%ct8[1E-06]' : '') + '"' : '') + '>%';
    if (f.k === 'CustomDP') return '%<\\AcVar CustomDP.' + (o.cp || '') + fm + '>%'; if (f.k === 'Formula') return '%<\\AcExpr (' + (o.ex || '') + ') \\f "%lu2%pr' + (o.prec != null ? o.prec : 2) + '">%';
    if (f.k === 'SystemVariable') return '%<\\AcVar ' + (o.sv || '') + '>%'; if (f.k === 'NamedObject') return '%<\\AcObjProp NamedObject(' + (o.ty || 'Layer') + ').Name "' + (o.nm || '') + '">%';
    if (/^Sheet/.test(f.k)) return '%<\\AcSm ' + { SheetNumber:'Sheet.Number', SheetTotal:'SheetSet.SheetCount', SheetNumTotal:'Sheet.Number/SheetSet.SheetCount', SheetTitle:'Sheet.Title', SheetSetName:'SheetSet.Name' }[f.k] + fm + '>%';
    if (f.k === 'Filename') return '%<\\AcVar Filename \\f "' + (o.path ? '%fn2' : '%fn6') + (o.ext ? '' : '%fn7') + '">%';
    return '%<\\AcVar ' + f.k + fm + '>%'; }
  var NAMED = { 'Layer':function(d){ return d.layers.map(function(l){ return l.name; }); }, 'Block':function(d){ return Object.keys(d.blocks).filter(function(n){ return !/^\*/.test(n); }); }, 'Kiểu chữ':function(d){ return d.styles.map(function(s){ return s.name; }); }, 'Kiểu kích thước':function(d){ return d.dimstyles.slice(); }, 'Kiểu bảng':function(d){ return SC.tableStyleNames(d); }, 'View':function(d){ return d.views.map(function(v){ return v.name; }); }, 'Layout':function(d){ return d.layouts.map(function(l){ return l.name; }); } };
  // hộp thoại Field: trả về { k, f, o, v } hoặc null
  SC.fieldDlg = function(o0){
    o0 = o0 || {};
    return new Promise(function(res){
      var d = D(), grp = 'Tất cả', q = '', F = Object.assign({ k:'Date', f:'dd/MM/yyyy', o:{} }, clone(o0.preset || TC.load('lastfield', {}) || {})); if (!FLD.some(function(x){ return x[0] === F.k; })) F = { k:'Date', f:'dd/MM/yyyy', o:{} };
      if (F.k === 'Object' && !d.byId[F.o.id]) F.o = {};
      var dlg = TC.dialog({ id:'field', title:o0.title || 'Chèn field', cmd:'FIELD', code:'UI-F08', w:880, onClose:function(r){ if (r !== true) res(null); },
        body:'<div class="fdw"><div class="stack fdl" style="gap:6px"><div class="field"><label for="fd-g">Nhóm field</label>' + H.sel('fd-g', FG, grp) + '</div><label class="search">' + ic('search') + '<input id="fd-q" placeholder="Tìm tên field" aria-label="Tìm tên field" spellcheck="false"></label><div class="tree fdlist" id="fd-l" role="listbox" tabindex="0" aria-label="Tên field"></div></div>' +
          '<div class="stack fdm"><div class="stack" id="fd-o" style="gap:8px"></div><div class="grid2 fdbot"><div class="field"><label>Xem trước</label><div class="fdpv" id="fd-pv"></div></div><div class="field"><label>Biểu thức field (chỉ đọc)</label><div class="fdx mono" id="fd-x" aria-live="polite"></div></div></div></div></div>',
        foot:'<label class="chk" style="font-size:var(--fs-s)"><input type="checkbox" id="fd-bg"' + (TC.S.fieldBg !== false ? ' checked' : '') + '> Hiện nền xám cho field trên bản vẽ (không in, FIELDDISPLAY)</label><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="fd-ok">' + esc(o0.ok || 'Chèn field') + '</button>' });
      var $d = function(s){ return $(s, dlg.el); }, def = function(){ return FLD.filter(function(x){ return x[0] === F.k; })[0]; };
      function list(){ var items = FLD.filter(function(x){ return (grp === 'Tất cả' || x[2] === grp) && (!q || TC.fold(x[1] + ' ' + x[0]).indexOf(TC.fold(q)) >= 0); }).sort(function(a, b){ return a[1].localeCompare(b[1], 'vi'); });
        $d('#fd-l').innerHTML = items.map(function(x){ return '<button class="tn" type="button" role="option" data-fk="' + x[0] + '" aria-selected="' + (x[0] === F.k) + '">' + esc(x[1]) + (x[2] === 'Bộ bản vẽ' ? '<span class="tag acc" style="height:15px;margin-left:auto">VN</span>' : '') + '</button>'; }).join('') || '<div class="empty" style="padding:14px 6px">' + ic('search') + '<span>Không có field khớp “' + esc(q) + '”.</span></div>'; }
      var tree = function(id, rows, cur){ return '<div class="tree fdopt" id="' + id + '" role="listbox" tabindex="0">' + rows.map(function(r){ return '<button class="tn" type="button" role="option" data-v="' + esc(r[0]) + '" aria-selected="' + (r[0] === cur) + '"><span>' + esc(r[1]) + '</span>' + (r[2] != null ? '<span class="cnt">' + esc(r[2]) + '</span>' : '') + '</button>'; }).join('') + '</div>'; };
      function opts(){ var x = def(), t = x[3], o = F.o, h = '', e = o.id && d.byId[o.id];
        if (t === 'date') h = H.row('Định dạng ngày', '<input class="inp mono" id="fd-df" value="' + esc(F.f || 'dd/MM/yyyy') + '" style="width:260px" spellcheck="false">', 116) + '<div class="field"><label>Mẫu (ngày kiểu Việt Nam ở đầu danh sách)</label>' + tree('fd-dl', DATEF.map(function(p){ return [p, fmtDate(new Date(2026, 9, 19, 9, 5), p), p]; }), F.f) + '</div>';
        else if (t === 'obj') h = '<div class="hrow"><span class="faint" style="font-size:var(--fs-s)">Đối tượng:</span><b id="fd-on">' + (e ? esc(TC.TYPE_NAMES[e.t] || e.t) + ' <span class="faint mono" style="font-weight:400">#' + e.id + ' · ' + esc(e.layer) + '</span>' : '<span class="faint" style="font-weight:400">chưa chọn</span>') + '</b><span class="grow"></span><button class="btn sm" type="button" id="fd-pick">' + ic('select') + (e ? 'Chọn lại' : 'Chọn đối tượng') + '</button></div>' +
          (e ? '<div class="field"><label>Thuộc tính</label>' + tree('fd-pl', objProps(e).map(function(p){ var r = objRaw(d, e, p[0]); return [p[0], p[1], typeof r === 'number' ? TC.d(r, 2) : String(r).slice(0, 24)]; }), o.prop) + '</div>' + (/area|len/.test((objProps(e).filter(function(p){ return p[0] === o.prop; })[0] || [])[2] || '') ? '<div class="grid2">' + H.row('Đơn vị', H.sel('fd-u', UNITS[(objProps(e).filter(function(p){ return p[0] === o.prop; })[0])[2]].map(function(u){ return u[0]; }), o.u, 100), 84) + H.row('Độ chính xác', H.sel('fd-p', ['0', '0.0', '0.00', '0.000'], ['0', '0.0', '0.00', '0.000'][o.prec != null ? o.prec : 2], 100), 84) + H.row('Tiền tố', '<input class="inp" id="fd-pre" value="' + esc(o.pre || '') + '" style="width:100px" placeholder="vd: S = ">', 84) + H.row('Hậu tố', '<input class="inp" id="fd-suf" value="' + esc(o.suf != null ? o.suf : '') + '" style="width:100px" placeholder="theo đơn vị">', 84) + '</div>' : '') : '<div class="empty" style="padding:14px 8px">' + ic('select') + '<span>Bấm <b>Chọn đối tượng</b> rồi chọn trên bản vẽ, ví dụ polyline bao phòng để lấy diện tích. Giá trị tự cập nhật bằng UPDATEFIELD khi đối tượng đổi.</span></div>');
        else if (t === 'cp') h = '<div class="field"><label>Thuộc tính tùy biến (Thuộc tính bản vẽ › Tùy biến)</label>' + tree('fd-cl', (info(d).custom || []).map(function(c){ return [c[0], c[0], c[1]]; }), o.cp) + '</div>';
        else if (t === 'named') h = H.row('Loại', H.sel('fd-ty', Object.keys(NAMED), o.ty || 'Layer', 180), 60) + '<div class="field"><label>Tên</label>' + tree('fd-nl', NAMED[o.ty || 'Layer'](d).map(function(n){ return [n, n]; }), o.nm) + '</div>';
        else if (t === 'sv') h = '<div class="field"><label>Biến hệ thống</label>' + tree('fd-sl', (TC.SYSVARS || []).map(function(s){ return [s[0], s[0] + ' · ' + s[1], s[2]]; }), o.sv) + '</div>';
        else if (t === 'fx') h = H.row('Biểu thức', '<input class="inp mono" id="fd-ex" value="' + esc(o.ex || '') + '" style="width:280px" placeholder="vd: (3900+3900+3900)/1000" spellcheck="false">', 80) + H.row('Độ chính xác', H.sel('fd-p', ['0', '0.0', '0.00', '0.000'], ['0', '0.0', '0.00', '0.000'][o.prec != null ? o.prec : 2], 100), 80) + '<p class="faint" style="font-size:var(--fs-xs);margin:0">Dùng + − * / ^ ( ). Công thức tham chiếu ô bảng: chọn ô rồi dùng Ô bảng › Công thức.</p>';
        else if (t === 'file') h = '<div class="stack" style="gap:4px">' + H.chk('fd-fp', 'Đường dẫn đầy đủ', !!o.path) + H.chk('fd-fe', 'Có phần mở rộng .dwg', !!o.ext) + '</div>';
        else if (t === 'size') h = H.row('Đơn vị', H.sel('fd-su', ['MB', 'KB'], o.unit || 'MB', 100), 60);
        else if (t === 'hl') h = '<div class="hrow"><span class="faint" style="font-size:var(--fs-s)">Liên kết:</span><b id="fd-hn">' + (o.link ? esc(o.link.t || o.link.a) + ' <span class="faint" style="font-weight:400">· ' + esc(o.link.a) + '</span>' : '<span class="faint" style="font-weight:400">chưa chọn</span>') + '</b><span class="grow"></span><button class="btn sm" type="button" id="fd-hl">' + ic('link') + (o.link ? 'Sửa liên kết…' : 'Chọn liên kết…') + '</button></div><p class="faint" style="font-size:var(--fs-xs);margin:0">Chữ field là chữ hiển thị của liên kết. ' + (TC.S.platform === 'mac' ? '⌘' : 'Ctrl') + ' + bấm chữ trên bản vẽ để mở.</p>';
        if (t !== 'date' && t !== 'fx' && !(t === 'obj' && /area|len/.test((objProps(e || { t:'' }).filter(function(p){ return p[0] === o.prop; })[0] || [])[2] || ''))) h += '<div class="field"><label>Định dạng</label>' + tree('fd-cf', CASES.map(function(c){ return [c, c]; }), F.f && CASES.indexOf(F.f) >= 0 ? F.f : '(không)') + '</div>';
        $d('#fd-o').innerHTML = h; pv(); }
      function pv(){ var x = def(), v = fval(d, F), miss = (x[3] === 'hl' && !F.o.link) || (x[3] === 'obj' && (!F.o.id || !F.o.prop)) || (x[3] === 'cp' && !F.o.cp) || (x[3] === 'named' && !F.o.nm) || (x[3] === 'sv' && !F.o.sv);
        $d('#fd-pv').innerHTML = miss ? '<span class="faint">Chọn ' + (x[3] === 'obj' ? 'đối tượng và thuộc tính' : x[3] === 'hl' ? 'liên kết' : 'một mục ở trên') + '</span>' : v === '' ? '<span class="faint">(trống)</span>' : '<span class="fdv' + (TC.S.fieldBg !== false ? ' bg' : '') + '">' + esc(v) + '</span>';
        $d('#fd-x').textContent = fcode(F); $d('#fd-ok').disabled = miss; }
      function pickDef(k){ F = { k:k, f:def0(k), o:k === F.k ? F.o : {} }; list(); opts(); }
      function def0(k){ var t = FLD.filter(function(x){ return x[0] === k; })[0][3]; return t === 'date' ? 'dd/MM/yyyy' : '(không)'; }
      dlg.el.addEventListener('click', function(ev){ var b = ev.target.closest('[data-fk]'); if (b){ pickDef(b.dataset.fk); return; }
        var it = ev.target.closest('.fdopt [data-v]'); if (it){ var id = it.parentNode.id, v = it.dataset.v; if (id === 'fd-dl'){ F.f = v; } if (id === 'fd-cf') F.f = v; if (id === 'fd-pl'){ F.o.prop = v; var ty = (objProps(d.byId[F.o.id]).filter(function(p){ return p[0] === v; })[0] || [])[2]; if (ty === 'area' && !F.o.pre) F.o.pre = 'S = '; if (/area|len/.test(ty)) F.o.u = UNITS[ty][0][0]; else { delete F.o.u; delete F.o.pre; delete F.o.suf; } } if (id === 'fd-cl') F.o.cp = v; if (id === 'fd-nl') F.o.nm = v; if (id === 'fd-sl') F.o.sv = v; opts(); var nb = $('#' + id, dlg.el); if (nb) nb.focus(); return; }
        if (ev.target.closest('#fd-hl')) SC.hyperlink([{ link:F.o.link }]).then(function(r){ if (r && r.link) F.o.link = r.link; else if (r && r.remove) delete F.o.link; opts(); });
        if (ev.target.closest('#fd-pick')) pickEnt('Chọn đối tượng cho field. Esc để quay lại hộp thoại.').then(function(e){ if (e === undefined) return; if (!e){ TC.toast('Không có đối tượng tại điểm đã bấm. Bấm Chọn đối tượng rồi thử lại.', { type:'warn', log:false }); return; } F.o = { id:e.id }; var ps = objProps(e); F.o.prop = ps[0][0]; if (ps[0][2] === 'area'){ F.o.u = 'm²'; F.o.pre = 'S = '; } else if (ps[0][2] === 'len') F.o.u = 'mm'; opts(); }); });
      dlg.el.addEventListener('change', function(ev){ var t = ev.target;
        if (t.id === 'fd-g'){ grp = t.value; list(); } if (t.id === 'fd-ty'){ F.o = { ty:t.value }; opts(); return; } if (t.id === 'fd-u') F.o.u = t.value; if (t.id === 'fd-p') F.o.prec = t.value.length > 1 ? t.value.length - 2 : 0; if (t.id === 'fd-su') F.o.unit = t.value; if (t.id === 'fd-fp') F.o.path = t.checked; if (t.id === 'fd-fe') F.o.ext = t.checked;
        if (t.id === 'fd-bg'){ TC.S.fieldBg = t.checked; TC.save('fieldbg', t.checked); A().docs.forEach(function(x){ if (x.ents) reprep(x); }); }
        pv(); });
      dlg.el.addEventListener('input', function(ev){ var t = ev.target; if (t.id === 'fd-q'){ q = t.value; list(); return; } if (t.id === 'fd-df') F.f = t.value; if (t.id === 'fd-pre') F.o.pre = t.value; if (t.id === 'fd-suf') F.o.suf = t.value === '' ? undefined : t.value; if (t.id === 'fd-ex') F.o.ex = t.value; pv(); });
      TC.listKeys($d('#fd-l'), '[data-fk]', function(it){ pickDef(it.dataset.fk); var b = $d('#fd-l [aria-selected="true"]'); if (b) b.scrollIntoView({ block:'nearest' }); $d('#fd-l').focus(); });
      $d('#fd-ok').addEventListener('click', function(){ var f = { k:F.k, f:F.f, o:clone(F.o), v:fval(d, F) }; TC.save('lastfield', { k:f.k, f:f.f, o:f.k === 'Object' ? {} : f.o }); dlg.close(true); res(f); });
      list(); opts(); var sb = $d('#fd-l [aria-selected="true"]'); if (sb) setTimeout(function(){ sb.scrollIntoView({ block:'nearest' }); }, 0);
    });
  };
  IMPL.FIELD = function(ctx, pre){
    ctx.badge('FLD');
    return SC.fieldDlg({ preset:pre && pre.k ? pre : null }).then(function(f){ if (!f) return; var c = ED.cur(), h = c.th, mk = function(p){ return { t:'mtext', p:p, w:Math.max(h * 4, tw(D(), f.v, h, c.tstyle) * 1.05 + h), h:h, s:f.v, style:c.tstyle, fld:[f], link:f.o.link ? clone(f.o.link) : undefined }; };
      return ctx.point('Góc trên trái của chữ field', { rubber:false, onMove:function(p, temp){ temp.push(ED.temp([mk(p)], { real:true })); } }).then(function(r){ if (!r.p) return; ED.run('FIELD', function(){ ED.add(ED.make(mk(r.p))); }); ctx.out('Đã chèn field <b>' + esc((FLD.filter(function(x){ return x[0] === f.k; })[0] || [])[1]) + '</b> = ' + esc(f.v) + '. Cập nhật bằng UPDATEFIELD.'); }); });
  };
  // cập nhật giá trị field trong chữ, MText, attribute và ô bảng; công thức ô bảng tự tính lại
  function updFields(d, ents){ var n = 0, ch = 0, tb = 0;
    ents.forEach(function(e){ if (e.t === 'table'){ tb++; if (!e.fld) { TC.prepEnt(d, e); return; } }
      if (!e.fld || !e.fld.length) return; var nv = e.fld.map(function(f){ return fval(d, f); }), diff = e.fld.some(function(f, i){ return nv[i] !== f.v; }); n += e.fld.length;
      if (!diff){ TC.prepEnt(d, e); return; }
      ED.mod(e, function(x){ x.fld.forEach(function(f, i){ if (nv[i] === f.v) return; ch++; if (f.cell){ var s0 = raw(x, f.cell[0], f.cell[1]); setRaw(x, f.cell[0], f.cell[1], s0.indexOf(f.v) >= 0 ? s0.replace(f.v, nv[i]) : nv[i]); } else if (f.tag){ x.attrs = Object.assign({}, x.attrs); x.attrs[f.tag] = String(x.attrs[f.tag] || '').replace(f.v, nv[i]); } else x.s = String(x.s).indexOf(f.v) >= 0 ? String(x.s).replace(f.v, nv[i]) : String(x.s) + nv[i]; f.v = nv[i]; }); }); });
    return { n:n, ch:ch, tb:tb }; }
  IMPL.UPDATEFIELD = function(ctx){
    return ctx.sel('Chọn đối tượng chứa field').then(function(ids){ var d = D(), ents = ids.map(function(id){ return d.byId[id]; }).filter(Boolean); if (!ents.length) return;
      var r; ED.run('UPDATEFIELD', function(){ r = updFields(d, ents); }); V().draw(); TC.cad.clearSel();
      ctx.out(r.n || r.tb ? 'Đã tìm thấy ' + r.n + ' field' + (r.tb ? ' và ' + r.tb + ' bảng có công thức' : '') + '. Đã cập nhật ' + r.ch + ' field có giá trị mới.' : 'Không có field nào trong ' + ents.length + ' đối tượng đã chọn.'); });
  };
  TC.cad.ctxExt.push(function(it, first, d){ if (!first || TC.isViewer() || !d.sel.some(function(id){ var e = d.byId[id]; return e && e.fld && e.fld.length; })) return; it.push({ label:'Cập nhật field', icon:'refresh', act:function(){ TC.cmd.run('UPDATEFIELD'); } }, { label:(TC.S.fieldBg !== false ? 'Tắt' : 'Bật') + ' nền xám của field', act:function(){ TC.S.fieldBg = TC.S.fieldBg === false; TC.save('fieldbg', TC.S.fieldBg); reprep(d); } }); });

  /* ================= MText: xếp chồng UI-F02, đoạn văn UI-F03, cột UI-F04, nhập văn bản ================= */
  // chữ xếp chồng ghi bằng ký tự Unicode trên/dưới nên mở được ở mọi trình xem; phân số thông dụng dùng ký tự ghép sẵn
  var SUP = { 0:'⁰', 1:'¹', 2:'²', 3:'³', 4:'⁴', 5:'⁵', 6:'⁶', 7:'⁷', 8:'⁸', 9:'⁹', '+':'⁺', '-':'⁻', '.':'·', '(':'⁽', ')':'⁾' }, SUB = { 0:'₀', 1:'₁', 2:'₂', 3:'₃', 4:'₄', 5:'₅', 6:'₆', 7:'₇', 8:'₈', 9:'₉', '+':'₊', '-':'₋', '.':'.', '(':'₍', ')':'₎' };
  var VF = { '1/2':'½', '1/3':'⅓', '2/3':'⅔', '1/4':'¼', '3/4':'¾', '1/5':'⅕', '2/5':'⅖', '3/5':'⅗', '4/5':'⅘', '1/6':'⅙', '5/6':'⅚', '1/8':'⅛', '3/8':'⅜', '5/8':'⅝', '7/8':'⅞' };
  var SK = [['h', 'Phân số ngang', '/'], ['d', 'Phân số chéo', '#'], ['t', 'Dung sai', '^'], ['dec', 'Thập phân', '^']];
  function mapc(s, T){ return String(s).split('').map(function(c){ return T[c] || c; }).join(''); }
  function stackStr(u, l, k){ if (k === 'h' && VF[u + '/' + l]) return VF[u + '/' + l]; return mapc(u, SUP) + (k === 't' || k === 'dec' ? '' : '⁄') + mapc(l, SUB); }
  function unstack(s){ var inv = function(T){ var o = {}; Object.keys(T).forEach(function(k){ o[T[k]] = k; }); return o; }, IS = inv(SUP), IB = inv(SUB), vf = Object.keys(VF).filter(function(k){ return VF[k] === s; })[0];
    if (vf) return { u:vf.split('/')[0], l:vf.split('/')[1], k:'h' }; var m = /^([⁰¹²³⁴⁵⁶⁷⁸⁹⁺⁻·⁽⁾]+)(⁄?)([₀₁₂₃₄₅₆₇₈₉₊₋.₍₎]+)$/.exec(s); if (!m) return null;
    return { u:m[1].split('').map(function(c){ return IS[c]; }).join(''), l:m[3].split('').map(function(c){ return c === '.' ? '.' : IB[c]; }).join(''), k:m[2] ? 'd' : 't' }; }
  function parseStack(s){ var m = /^\s*([^\/#^]+?)\s*([\/#^])\s*([^\/#^]+?)\s*$/.exec(s); return m ? { u:m[1], l:m[3], k:{ '/':'h', '#':'d', '^':'t' }[m[2]] } : unstack(s.trim()); }
  var AS = Object.assign({ on:true, trim:true, diag:false, ask:true }, TC.load('autostack', {}));
  function asDlg(settings){
    return new Promise(function(res){
      var dlg = TC.dialog({ id:'autostack', title:'Thiết lập AutoStack', code:'UI-F02', w:460, onClose:function(r){ if (r !== true) res(false); },
        body:'<div class="stack" style="gap:8px">' + (settings ? '' : '<p style="margin:0">Vừa gõ một số dạng phân số. Tự động xếp chồng khi gõ?</p>') + H.chk('as-on', 'Bật AutoStack (tự xếp chồng khi gõ x/y, x#y rồi khoảng trắng)', AS.on) + H.chk('as-trim', 'Bỏ khoảng trắng giữa phần nguyên và phân số', AS.trim) + '<div class="grp" style="gap:4px"><div class="gh">Khi gõ x/y</div>' + H.rad('as-k', 'h', 'Chuyển thành phân số ngang', !AS.diag) + H.rad('as-k', 'd', 'Chuyển thành phân số chéo', AS.diag) + '</div>' + H.chk('as-ask', 'Không hiện hộp thoại này nữa', !AS.ask) + '</div>',
        foot:'<span class="grow"></span>' + (settings ? '<button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="as-ok">Lưu thiết lập</button>' : '<button class="btn" type="button" data-x="cancel">Không xếp chồng</button><button class="btn primary" type="button" id="as-ok">Xếp chồng</button>') });
      $('#as-ok', dlg.el).addEventListener('click', function(){ AS.on = $('#as-on', dlg.el).checked; AS.trim = $('#as-trim', dlg.el).checked; AS.diag = $('[name="as-k"]:checked', dlg.el).value === 'd'; AS.ask = !$('#as-ask', dlg.el).checked; TC.save('autostack', AS); dlg.close(true); res(AS.on); });
    });
  }
  function autoStack(ta){ if (!AS.on) return; var a = ta.selectionStart, pre = ta.value.slice(0, a), m = /(^|[^\d.\/#^])(\d+(?:\.\d+)?)([\/#])(\d+(?:\.\d+)?)(\s)$/.exec(pre); if (!m) return;
    var go = function(){ var start = a - m[0].length + m[1].length, lead = AS.trim && /\d $/.test(ta.value.slice(0, start)) ? 1 : 0; ta.setRangeText(stackStr(m[2], m[4], m[3] === '#' || AS.diag ? 'd' : 'h') + m[5], start - lead, a, 'end'); ta.dispatchEvent(new Event('input')); };
    if (AS.ask) asDlg(false).then(function(ok){ if (ok && ta.isConnected) go(); if (ta.isConnected) ta.focus(); }); else go(); }
  function selText(ta){ return ta.value.slice(ta.selectionStart, ta.selectionEnd); }
  SC.mtStack = function(M, op){
    var ta = M.ta, s = selText(ta), p = s && parseStack(s);
    if (op === 'auto') return asDlg(true).then(function(){ ta.focus(); });
    if (op === 'props') return stackDlg(M, p && s ? p : { u:'1', l:'2', k:'h' }, !!(p && s));
    if (!p){ TC.toast('Chọn chữ dạng <b>1/2</b> (phân số ngang), <b>1#2</b> (phân số chéo) hoặc <b>+0.1^-0.2</b> (dung sai) rồi bấm Xếp chồng.', { type:'info', log:false }); ta.focus(); return; }
    var stacked = unstack(s.trim()); ta.setRangeText(stacked ? stacked.u + (stacked.k === 'd' ? '#' : stacked.k === 't' ? '^' : '/') + stacked.l : stackStr(p.u, p.l, p.k), ta.selectionStart, ta.selectionEnd, 'select'); M.apply();
  };
  function stackDlg(M, p, hasSel){
    var ta = M.ta, a = ta.selectionStart, b = ta.selectionEnd, P = Object.assign({ pos:'Giữa', size:70 }, TC.load('stackfmt', {}), p);
    var dlg = TC.dialog({ id:'stackp', title:'Thuộc tính xếp chồng', code:'UI-F02', w:560,
      body:'<div class="skw"><div class="stack" style="gap:8px">' + H.grp('Chữ', H.row('Trên', '<input class="inp mono" id="sk-u" value="' + esc(P.u) + '" style="width:150px">', 50) + H.row('Dưới', '<input class="inp mono" id="sk-l" value="' + esc(P.l) + '" style="width:150px">', 50)) +
        H.grp('Giao diện', '<div class="stack" style="gap:3px">' + SK.map(function(k){ return H.rad('sk-k', k[0], k[1] + ' <span class="faint mono">' + (k[0] === 'dec' ? 'x^y canh dấu chấm' : 'x' + k[2] + 'y') + '</span>', P.k === k[0]); }).join('') + '</div>' + H.row('Vị trí', H.sel('sk-pos', ['Trên', 'Giữa', 'Dưới'], P.pos, 110), 60) + H.row('Cỡ chữ', H.sel('sk-sz', ['50%', '60%', '70%', '80%', '90%', '100%'], P.size + '%', 110), 60)) + '</div>' +
        '<div class="stack"><span class="gh">Xem trước</span><div class="skpv" id="sk-pv"></div><p class="faint" style="font-size:var(--fs-xs);margin:0">Trên bản vẽ ghi bằng ký tự Unicode trên/dưới (mở được ở mọi nơi); phân số thông dụng như ½, ¾ dùng ký tự ghép sẵn.</p><button class="btn sm" type="button" id="sk-as" style="justify-self:start">' + ic('gear') + 'Thiết lập AutoStack…</button></div></div>',
      foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="sk-ok">' + (hasSel ? 'Xếp chồng' : 'Chèn phân số') + '</button>' });
    function pv(){ P.u = $('#sk-u', dlg.el).value; P.l = $('#sk-l', dlg.el).value; P.k = $('[name="sk-k"]:checked', dlg.el).value; P.pos = $('#sk-pos', dlg.el).value; P.size = parseInt($('#sk-sz', dlg.el).value, 10);
      var sz = 'font-size:' + P.size + '%', va = P.pos === 'Trên' ? 'super' : P.pos === 'Dưới' ? 'sub' : 'middle', u = esc(P.u), l = esc(P.l);
      $('#sk-pv', dlg.el).innerHTML = '<span>Ống ⌀ 1</span><span class="skf ' + P.k + '" style="vertical-align:' + va + ';' + sz + '">' + (P.k === 'd' ? '<sup>' + u + '</sup>⁄<sub>' + l + '</sub>' : '<span>' + u + '</span><span>' + l + '</span>') + '</span><span>&nbsp;in</span><span class="skc">Ghi trên bản vẽ: <b>Ống ⌀ 1' + esc(stackStr(P.u, P.l, P.k)) + ' in</b></span>'; }
    dlg.el.addEventListener('input', pv); dlg.el.addEventListener('change', pv);
    $('#sk-as', dlg.el).addEventListener('click', function(){ asDlg(true); });
    $('#sk-ok', dlg.el).addEventListener('click', function(){ pv(); if (!P.u.trim() || !P.l.trim()){ TC.toast('Nhập chữ trên và chữ dưới.', { type:'warn', log:false }); return; } TC.save('stackfmt', { pos:P.pos, size:P.size }); dlg.close(true); ta.setSelectionRange(a, b); ta.setRangeText(stackStr(P.u.trim(), P.l.trim(), P.k), a, b, 'end'); M.apply(); });
    pv();
  }
  // đoạn văn: tab, thụt lề, canh đoạn, khoảng cách đoạn, giãn dòng (đơn vị bản vẽ)
  var LSK = [['m', 'Bội số'], ['e', 'Chính xác'], ['a', 'Tối thiểu']];
  SC.mtPara = function(M){
    var F = M.F, pa = Object.assign({ fi:0, hg:0, ri:0, sb:0, sa:0, tabs:[], lk:'m' }, clone(F.para || {})), lsv = pa.lk === 'm' ? F.ls / 1.6667 : F.ls * F.h, tsel = -1;
    var AL = [['l', 'Trái'], ['c', 'Giữa'], ['r', 'Phải'], ['j', 'Đều'], ['d', 'Phân bố']], TK = [['L', 'Trái'], ['C', 'Giữa'], ['R', 'Phải'], ['D', 'Thập phân']];
    var n = function(id, v, o){ return H.num(id, TC.d(v, o && o.p != null ? o.p : 2), Object.assign({ w:100, pick:false }, o || {})); };
    var dlg = TC.dialog({ id:'mpara', title:'Đoạn văn', code:'UI-F03', w:780,
      body:'<div class="mpar"><div class="stack" style="gap:10px">' + H.grp('Tab dừng', '<div class="hrow">' + TK.map(function(t){ return H.rad('pa-tk', t[0], t[1], t[0] === 'L'); }).join('') + '</div><div class="hrow" style="flex-wrap:nowrap">' + n('pa-tp', 0, { w:88 }) + '<button class="btn sm" type="button" id="pa-ta">Thêm</button><button class="btn sm ghost" type="button" id="pa-td">Xóa</button></div><div class="tree mtabs" id="pa-tl" role="listbox" tabindex="0" aria-label="Tab dừng"></div>') +
        H.grp('Giãn dòng', '<div class="hrow">' + H.sel('pa-lk', LSK.map(function(x){ return x[1]; }), LSK.filter(function(x){ return x[0] === pa.lk; })[0][1], 120) + n('pa-lv', lsv, { p:pa.lk === 'm' ? 2 : 0 }) + '</div><span class="faint" id="pa-lh" style="font-size:var(--fs-xs)"></span>') + '</div>' +
        '<div class="stack" style="gap:10px">' + H.grp('Thụt lề trái', H.row('Dòng đầu', n('pa-fi', pa.fi), 80) + H.row('Treo', n('pa-hg', pa.hg), 80)) + H.grp('Thụt lề phải', H.row('Phải', n('pa-ri', pa.ri), 80)) +
        H.grp('Canh đoạn', '<div class="hrow">' + AL.map(function(x){ return H.rad('pa-al', x[0], x[1], F.al === x[0]); }).join('') + '</div>') + H.grp('Khoảng cách đoạn', H.row('Trước', n('pa-sb', pa.sb), 80) + H.row('Sau', n('pa-sa', pa.sa), 80)) + '</div>' +
        '<div class="stack"><span class="gh">Xem trước</span><div class="mppv" id="pa-pv"></div></div></div>',
      foot:'<span class="faint" style="font-size:var(--fs-s)">Đơn vị bản vẽ (mm). Chiều cao chữ ' + TC.d(F.h, 0) + '.</span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="pa-ok">Áp dụng đoạn văn</button>' });
    function tabs(){ pa.tabs.sort(function(x, y){ return x.p - y.p; }); $('#pa-tl', dlg.el).innerHTML = pa.tabs.length ? pa.tabs.map(function(t, i){ return '<button class="tn" type="button" role="option" data-ti="' + i + '" aria-selected="' + (i === tsel) + '"><span class="mono">' + TC.d(t.p, 0) + '</span><span class="cnt">' + TK.filter(function(x){ return x[0] === t.k; })[0][1] + '</span></button>'; }).join('') : '<span class="faint" style="font-size:var(--fs-xs);padding:4px 6px">Chưa có tab dừng. Nhập vị trí rồi bấm Thêm.</span>'; pv(); }
    function pv(){ var k = 150 / Math.max(F.h * 30, 1), al = { l:'left', c:'center', r:'right', j:'justify', d:'justify' }[$('[name="pa-al"]:checked', dlg.el).value] || 'left', lh = pa.lk === 'm' ? lsv * 1.6667 : pa.lk === 'e' ? lsv / F.h : Math.max(1.6667, lsv / F.h), p = '<p style="text-indent:' + ((pa.fi - pa.hg) * k) + 'px;padding-left:' + (pa.hg * k) + 'px;padding-right:' + (pa.ri * k) + 'px;margin:' + (pa.sb * k) + 'px 0 ' + (pa.sa * k) + 'px;line-height:' + Math.max(1, lh * .78) + ';text-align:' + al + '">';
      $('#pa-pv', dlg.el).innerHTML = p + 'Tường bao xây gạch dày 220, tường ngăn dày 110, trát vữa xi măng mác 75.</p>' + p + 'Cao độ ±0.000 lấy theo mặt sàn hoàn thiện tầng 1.</p>';
      $('#pa-lh', dlg.el).textContent = 'Khoảng cách giữa hai dòng: ' + TC.d(F.h * lh, 0) + ' (' + TC.d(lh / 1.6667, 2) + ' lần dòng đơn)'; }
    dlg.el.addEventListener('numchange', function(ev){ var id = ($('input', ev.target) || {}).id, v = ev.detail; if (id === 'pa-lv') lsv = Math.max(.1, v); else if (id && id !== 'pa-tp') pa[id.slice(3)] = v; pv(); });
    dlg.el.addEventListener('change', function(ev){ if (ev.target.id === 'pa-lk'){ var nk = LSK.filter(function(x){ return x[1] === ev.target.value; })[0][0]; lsv = nk === 'm' ? (pa.lk === 'm' ? lsv : lsv / F.h / 1.6667) : (pa.lk === 'm' ? lsv * 1.6667 * F.h : lsv); pa.lk = nk; var i = $('#pa-lv', dlg.el); i.value = TC.d(lsv, nk === 'm' ? 2 : 0); } pv(); });
    dlg.el.addEventListener('click', function(ev){ var t = ev.target.closest('[data-ti]'); if (t){ tsel = Number(t.dataset.ti); tabs(); }
      if (ev.target.closest('#pa-ta')){ var v = num($('#pa-tp', dlg.el).value, 0); if (!(v > 0)){ TC.toast('Vị trí tab phải lớn hơn 0.', { type:'warn', log:false }); return; } if (pa.tabs.some(function(x){ return Math.abs(x.p - v) < 1e-6; })){ TC.toast('Đã có tab dừng tại ' + TC.d(v, 0) + '.', { type:'warn', log:false }); return; } pa.tabs.push({ p:v, k:$('[name="pa-tk"]:checked', dlg.el).value }); tsel = -1; tabs(); }
      if (ev.target.closest('#pa-td') && tsel >= 0){ pa.tabs.splice(tsel, 1); tsel = -1; tabs(); } });
    $('#pa-ok', dlg.el).addEventListener('click', function(){ F.al = $('[name="pa-al"]:checked', dlg.el).value; F.ls = pa.lk === 'm' ? lsv * 1.6667 : pa.lk === 'e' ? lsv / F.h : Math.max(1.6667, lsv / F.h); F.para = pa.fi || pa.hg || pa.ri || pa.sb || pa.sa || pa.tabs.length || pa.lk !== 'm' ? pa : null; dlg.close(true); M.apply(); paraCss(M); });
    tabs();
  };
  // thụt lề và tab dừng trong khung soạn: chữ thụt theo dòng đầu/treo, mốc trên thước
  function paraCss(M){ var ta = M.ta, box = ta.parentNode, pa = M.F.para || {}, k = (box.offsetWidth - 8) / Math.max(M.w ? M.w() : M.e.w, 1);
    ta.style.textIndent = ((pa.fi || 0) - (pa.hg || 0)) * k + 'px'; ta.style.paddingLeft = (4 + (pa.hg || 0) * k) + 'px'; ta.style.paddingRight = (4 + (pa.ri || 0) * k) + 'px';
    var i1 = $('.mte-ruler .i1', box), i2 = $('.mte-ruler .i2', box); if (i1) i1.style.left = (pa.fi || 0) * k - 1 + 'px'; if (i2) i2.style.left = (pa.hg || 0) * k - 1 + 'px';
    $$('.mte-ruler .tbs', box).forEach(function(t, i){ if (i) t.remove(); }); var tb = $('.mte-ruler .tbs', box); if (tb){ tb.style.display = (pa.tabs || []).length ? '' : 'none'; (pa.tabs || []).forEach(function(t, i){ var el = i ? tb.cloneNode() : tb; el.style.left = t.p * k + 'px'; if (i) tb.parentNode.appendChild(el); }); } }
  // cột: không cột / động (tự cao hoặc cao thủ công) / tĩnh; chữ chảy sang cột kế khi đầy chiều cao
  SC.mtColMenu = function(M){ var c = M.F.col || { k:'n' }, set = function(o){ return function(){ M.F.col = o.k === 'n' ? null : Object.assign({ n:2, h:M.F.h * 12, gap:M.F.h * 4, auto:true }, c.k !== 'n' ? c : {}, o); M.apply(); TC.cmdline.msg(o.k === 'n' ? 'MText không chia cột.' : 'MText chia ' + (o.k === 's' ? (o.n || c.n || 2) + ' cột tĩnh.' : 'cột động.') + ' Thiết lập chi tiết ở Cột › Thiết lập cột.'); }; };
    return [{ label:'Không cột', check:c.k === 'n', act:set({ k:'n' }) }, { label:'Cột động', check:c.k === 'd', sub:[{ label:'Tự cao', check:c.k === 'd' && c.auto !== false, act:set({ k:'d', auto:true }) }, { label:'Cao thủ công', check:c.k === 'd' && c.auto === false, act:set({ k:'d', auto:false }) }] }, { label:'Cột tĩnh', check:c.k === 's', sub:[2, 3, 4, 5, 6].map(function(n){ return { label:n + ' cột', check:c.k === 's' && c.n === n, act:set({ k:'s', n:n }) }; }) }, { sep:1 }, { label:'Thiết lập cột…', act:function(){ colDlg(M); } }]; };
  function colDlg(M){
    var F = M.F, C = Object.assign({ k:'n', n:2, h:F.h * 12, gap:F.h * 4, auto:true }, clone(F.col || {})), W = M.w ? M.w() : M.e.w;
    var n = function(id, v){ return H.num(id, TC.d(v, id === 'co-n' ? 0 : 2), { w:110, pick:false, prec:id === 'co-n' ? 0 : 2, unit:id === 'co-n' ? 'none' : 'len' }); };
    var dlg = TC.dialog({ id:'mcol', title:'Thiết lập cột', code:'UI-F04', w:620,
      body:'<div class="mcol"><div class="stack" style="gap:10px">' + H.grp('Kiểu cột', H.rad('co-k', 'n', 'Không cột', C.k === 'n') + H.rad('co-k', 'd', 'Cột động', C.k === 'd') + '<div class="stack" style="gap:3px;padding-left:20px">' + H.rad('co-a', 'a', 'Tự cao', C.auto !== false) + H.rad('co-a', 'm', 'Cao thủ công', C.auto === false) + '</div>' + H.rad('co-k', 's', 'Cột tĩnh', C.k === 's')) +
        H.grp('Số cột, chiều cao', H.row('Số cột', n('co-n', C.n), 90) + H.row('Chiều cao', n('co-h', C.h), 90)) + H.grp('Bề rộng', H.row('Cột', n('co-w', W), 90) + H.row('Khoảng giữa', n('co-g', C.gap), 90) + H.row('Tổng', '<span class="mono" id="co-t"></span>', 90)) + '</div>' +
        '<div class="stack"><span class="gh">Xem trước</span><div class="mcpv" id="co-pv"></div><p class="faint" id="co-m" style="font-size:var(--fs-xs);margin:0"></p></div></div>',
      foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="co-ok">Áp dụng cột</button>' });
    function upd(){ C.k = $('[name="co-k"]:checked', dlg.el).value; C.auto = $('[name="co-a"]:checked', dlg.el).value === 'a';
      $$('[name="co-a"]', dlg.el).forEach(function(i){ i.disabled = C.k !== 'd'; }); var dis = function(id, on){ var i = $('#' + id, dlg.el); i.disabled = !on; i.closest('.numf').classList.toggle('is-dis', !on); }; dis('co-n', C.k === 's'); dis('co-h', C.k === 's' || (C.k === 'd' && !C.auto)); dis('co-g', C.k !== 'n');
      var cols = C.k === 'n' ? 1 : C.k === 's' ? C.n : Math.max(1, Math.ceil(TC.mtextLines(Object.assign({}, M.e, { s:M.ta.value, w:W, h:F.h })).length / Math.max(1, Math.floor((C.h - F.h) / (F.h * F.ls)) + 1)));
      $('#co-t', dlg.el).textContent = TC.d(cols * W + (cols - 1) * (C.k === 'n' ? 0 : C.gap));
      $('#co-pv', dlg.el).innerHTML = new Array(cols + 1).join('x').split('').map(function(){ return '<div class="mcc"><i></i><i></i><i></i><i></i><i></i><i class="s"></i></div>'; }).join('');
      $('#co-m', dlg.el).textContent = C.k === 'n' ? 'Chữ chạy một cột theo bề rộng khung.' : C.k === 's' ? 'Chữ chia đều ' + C.n + ' cột; cột đầy thì chảy sang cột sau.' : C.auto ? 'Cột cao ' + TC.d(C.h, 0) + '; chữ dài thêm tự sinh cột mới (' + cols + ' cột với nội dung hiện tại).' : 'Kéo tay nắm dưới mỗi cột để chỉnh chiều cao từng cột (prototype dùng một chiều cao chung).'; }
    dlg.el.addEventListener('change', upd);
    dlg.el.addEventListener('numchange', function(ev){ var id = ($('input', ev.target) || {}).id, v = ev.detail; if (!(v > 0)) return; if (id === 'co-n') C.n = TC.clamp(Math.round(v), 2, 20); if (id === 'co-h') C.h = v; if (id === 'co-g') C.gap = v; if (id === 'co-w') W = Math.max(F.h * 4, v); upd(); });
    $('#co-ok', dlg.el).addEventListener('click', function(){ upd(); F.col = C.k === 'n' ? null : { k:C.k, n:C.n, h:C.h, gap:C.gap, auto:C.auto }; if (M.w) M.w(W); dlg.close(true); M.apply(); });
    upd();
  }
  // nhập văn bản .txt, .rtf vào trình soạn; tệp mã TCVN3/VNI được chuyển sang Unicode
  var TXTS = [['Ghi-chu-chung.txt', 'UTF-8', '1 KB', 'GHI CHÚ:\n1. Tường bao xây gạch dày 220, tường ngăn dày 110.\n2. Cao độ ±0.000 lấy theo mặt sàn hoàn thiện tầng 1.\n3. Kích thước ghi bằng mm, cao độ ghi bằng m.'], ['Thuyet-minh-hoan-thien.rtf', 'RTF', '3 KB', 'Sàn lát gạch ceramic 600×600, vữa lót mác 75 dày 20.\nTrần thạch cao khung chìm, sơn nước 1 lớp lót 2 lớp phủ.'], ['Ghi-chu-cu-2015.txt', 'TCVN3', '1 KB', 'Cửa đi, cửa sổ xem bảng thống kê bản vẽ KT-12.\nVật liệu chống thấm: màng khò nóng dày 3 mm.']];
  SC.mtImport = function(M){
    var cur = 0, uni = true, extra = null;
    var dlg = TC.dialog({ id:'mtimp', title:'Nhập văn bản vào MText', code:'UI-F01', w:640,
      body:'<div class="grid2" style="grid-template-columns:240px minmax(0,1fr)"><div class="stack" style="gap:6px"><span class="gh">Tệp gần đây · D:\\DuAn\\NhaO-2T\\Ghi-chu</span><div class="tree" id="ti-l" role="listbox" tabindex="0" aria-label="Tệp văn bản"></div><button class="btn sm" type="button" id="ti-br" style="justify-self:start">' + ic('open') + 'Duyệt…</button><input type="file" id="ti-f" accept=".txt,.rtf,text/plain" hidden></div><div class="stack" style="gap:6px"><span class="gh">Nội dung</span><pre class="tipv" id="ti-pv"></pre><span id="ti-m"></span></div></div>',
      foot:'<span class="faint" style="font-size:var(--fs-s)">Chèn tại vị trí con trỏ, giữ định dạng của MText.</span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="ti-ok">Chèn văn bản</button>' });
    var files = function(){ return TXTS.concat(extra ? [extra] : []); };
    function text(){ var f = files()[cur]; return f[1] === 'TCVN3' && !uni ? TC.tcvn3(f[3]) : f[3]; }
    function render(){ $('#ti-l', dlg.el).innerHTML = files().map(function(f, i){ return '<button class="tn" type="button" role="option" data-fi="' + i + '" aria-selected="' + (i === cur) + '">' + ic('file', 's12') + '<span>' + esc(f[0]) + '</span><span class="cnt">' + esc(f[2]) + '</span></button>'; }).join('');
      var f = files()[cur]; $('#ti-pv', dlg.el).textContent = text(); $('#ti-m', dlg.el).innerHTML = f[1] === 'TCVN3' ? '<div class="note warn">' + ic('warn', 'xs') + '<span>Tệp dùng bảng mã <b>TCVN3</b>. ' + H.chk('ti-u', 'Chuyển sang Unicode khi chèn (khuyên dùng)', uni) + '</span></div>' : '<span class="faint" style="font-size:var(--fs-xs)">Bảng mã: ' + esc(f[1]) + (f[1] === 'RTF' ? ' · giữ chữ, bỏ định dạng phức tạp' : '') + '</span>'; }
    dlg.el.addEventListener('click', function(ev){ var b = ev.target.closest('[data-fi]'); if (b){ cur = Number(b.dataset.fi); render(); } if (ev.target.closest('#ti-br')) $('#ti-f', dlg.el).click(); });
    dlg.el.addEventListener('change', function(ev){ if (ev.target.id === 'ti-u'){ uni = ev.target.checked; render(); } if (ev.target.id === 'ti-f' && ev.target.files[0]){ var file = ev.target.files[0], rd = new FileReader(); rd.onload = function(){ var t = String(rd.result); if (/\.rtf$/i.test(file.name)) t = t.replace(/\\par[d]?/g, '\n').replace(/\{\\\*[^{}]*\}|\\[a-z]+-?\d* ?|[{}]/g, '').trim(); extra = [file.name, /\.rtf$/i.test(file.name) ? 'RTF' : 'UTF-8', TC.size(file.size), t]; cur = files().length - 1; render(); }; rd.readAsText(file); } });
    TC.listKeys($('#ti-l', dlg.el), '[data-fi]', function(it){ cur = Number(it.dataset.fi); render(); $('#ti-l', dlg.el).focus(); });
    $('#ti-ok', dlg.el).addEventListener('click', function(){ var t = text(); dlg.close(true); M.ta.focus(); M.insert(t); TC.cmdline.msg('Đã chèn ' + t.split('\n').length + ' dòng từ ' + esc(files()[cur][0]) + '.'); });
    render();
  };
  SC.mtField = function(M){ var a = M.ta.selectionStart, b = M.ta.selectionEnd; SC.fieldDlg({}).then(function(f){ M.ta.focus(); if (!f) return; M.ta.setSelectionRange(a, b); M.insert(f.v); M.F.fld = (M.F.fld || []).concat([f]); }); };
  // annot.js gọi khi mở và khi lưu MText: mang đoạn văn, cột, field qua trình soạn
  SC.mtInit = function(F, e){ F.para = e.para ? clone(e.para) : null; F.col = e.col ? clone(e.col) : null; F.fld = e.fld ? clone(e.fld) : []; };
  SC.mtApply = function(x, F){ ['para', 'col'].forEach(function(k){ if (F[k]) x[k] = F[k]; else delete x[k]; }); var fl = (F.fld || []).filter(function(f){ return String(x.s || '').indexOf(f.v) >= 0; }); if (fl.length) x.fld = fl; else delete x.fld; };
  var mte0 = ED.mtextEdit;
  ED.mtextEdit = function(e, o){ var r = mte0(e, o), ta = $('#cv .mte textarea'); if (ta){ ta.addEventListener('input', function(){ autoStack(ta); }); } return r; };

  /* ================= hậu xử lý nguyên tố: cột và thụt lề MText, nền xám của field ================= */
  // khung bao theo bề rộng chữ đo thật (canvas.js ước lượng 0.62 × chiều cao mỗi ký tự)
  function bboxOf(list, d){ var b = [Infinity, Infinity, -Infinity, -Infinity], add = function(x, y){ if (x < b[0]) b[0] = x; if (y < b[1]) b[1] = y; if (x > b[2]) b[2] = x; if (y > b[3]) b[3] = y; };
    list.forEach(function(p){ if (p.k === 'seg'){ add(p.a[0], p.a[1]); add(p.b[0], p.b[1]); } else if (p.k === 'text'){ var W = d ? tw(d, p.s, p.h, p.style, p.bold) : p.h * .62 * String(p.s).length, al = p.al || 'l', x0 = al === 'c' || al === 'm' ? -W / 2 : al === 'r' || al === 'rm' ? -W : 0, mid = /m$/.test(al); if (p.rot){ add(p.p[0] - W, p.p[1] - W); add(p.p[0] + W, p.p[1] + W); } else { add(p.p[0] + x0, p.p[1] - (mid ? p.h * .7 : p.h * .3)); add(p.p[0] + x0 + W, p.p[1] + (mid ? p.h * .7 : p.h * 1.1)); } } else if (p.k === 'hatch') p.loops.forEach(function(l){ l.forEach(function(q){ add(q[0], q[1]); }); }); else if (p.k === 'circ' || p.k === 'arc'){ add(p.c[0] - p.r, p.c[1] - p.r); add(p.c[0] + p.r, p.c[1] + p.r); } else if (p.p) add(p.p[0], p.p[1]); else if (p.pts) p.pts.forEach(function(q){ add(q[0], q[1]); }); });
    return b; }
  // cột (UI-F04), thụt lề và khoảng cách đoạn (UI-F03): ngắt dòng lại theo bề rộng đo thật rồi xếp vào cột
  function mtLayout(d, e){
    var col = e.col && e.col.k && e.col.k !== 'n' ? e.col : null, pa = e.para || {}, old = e._p.filter(function(pr){ return pr.k === 'text' && pr.src === e; });
    if (!old.length || (!col && !pa.fi && !pa.hg && !pa.ri && !pa.sb && !pa.sa)) return false;
    var tp = old[0], lh = e.h * (e.ls || 1.6), rr = (e.rot || 0) * Math.PI / 180, cs = Math.cos(rr), sn = Math.sin(rr), mal = e.al === 'c' ? .5 : e.al === 'r' ? 1 : 0, gap = col ? (col.gap || e.h * 4) : 0, L = [], y = 0;
    String(e.s || '').split('\n').forEach(function(par){ if (e.caps) par = par.toUpperCase(); var cur = '', k = 0, w = function(){ return e.w - (k ? (pa.hg || 0) : (pa.fi || 0)) - (pa.ri || 0); }, push = function(t){ L.push({ s:t, f:k === 0 }); k++; };
      par.split(' ').forEach(function(x){ var t = cur ? cur + ' ' + x : x; if (cur && tw(d, t, e.h, tp.style, tp.bold) > w()){ push(cur); cur = x; } else cur = t; }); push(cur); });
    L.forEach(function(l, i){ if (i && l.f) y += (pa.sa || 0) + (pa.sb || 0); l.y = y; y += lh; });
    var n = col && col.k === 's' ? Math.max(1, col.n || 2) : 0, colH = !col ? Infinity : n ? (y - lh + e.h) / n + 1 : (col.h || e.h * 12), ci = 0, y0 = 0, out = e._p.filter(function(pr){ return !(pr.k === 'text' && pr.src === e); });
    L.forEach(function(l, i){ if (col && i && l.y - y0 + e.h > colH && (!n || ci < n - 1)){ ci++; y0 = l.y; }
      var xo = ci * (e.w + gap) + (e.w - (pa.ri || 0)) * mal + (mal ? 0 : l.f ? (pa.fi || 0) : (pa.hg || 0)), yo = -(e.h + l.y - y0);
      out.push({ k:'text', p:[e.p[0] + xo * cs - yo * sn, e.p[1] + xo * sn + yo * cs], h:tp.h, s:l.s, rot:tp.rot, al:tp.al, style:tp.style, bold:tp.bold, ital:tp.ital, e:e, src:e }); });
    e._p = out; return true;
  }
  function fieldBg(d, e){ var add = [];
    (e.fld || []).forEach(function(f){ if (f.cell || !f.v) return; for (var i = 0; i < e._p.length; i++){ var pr = e._p[i]; if (pr.k !== 'text' || (f.tag ? pr.tag !== f.tag : pr.src !== e)) continue; var at = pr.s.indexOf(f.v); if (at < 0) continue;
      var W = tw(d, pr.s, pr.h, pr.style, pr.bold), w0 = tw(d, pr.s.slice(0, at), pr.h, pr.style, pr.bold), w1 = tw(d, f.v, pr.h, pr.style, pr.bold), h = pr.h, al = pr.al || 'l', mid = /m$/.test(al);
      var x0 = (al === 'c' || al === 'm' ? -W / 2 : al === 'r' || al === 'rm' ? -W : 0) + w0, y0 = mid ? -h * .66 : -h * .3, y1 = mid ? h * .66 : h * 1.1, pad = h * .12, r = (pr.rot || 0) * Math.PI / 180, cs = Math.cos(r), sn = Math.sin(r);
      var Pt = function(x, y){ return [pr.p[0] + x * cs - y * sn, pr.p[1] + x * sn + y * cs]; };
      add.push({ k:'hatch', loops:[[Pt(x0 - pad, y0), Pt(x0 + w1 + pad, y0), Pt(x0 + w1 + pad, y1), Pt(x0 - pad, y1)]], pat:'SOLID', sc:1, ang:0, e:e, src:{ color:8, layer:e.layer }, fbg:1 }); break; } });
    if (add.length) Array.prototype.unshift.apply(e._p, add); return add.length > 0; }
  function post(d, e){ var ch = false; if (!e._p) return false; if (e.t === 'mtext' && (e.col || e.para)) ch = mtLayout(d, e) || ch; if (e.fld && e.fld.length && e.t !== 'table' && TC.S.fieldBg !== false) ch = fieldBg(d, e) || ch; if (ch || /^(table|tol|qleader)$/.test(e.t)) e._bb = bboxOf(e._p, d); return ch; }
  var prep0 = TC.prepEnt;
  TC.prepEnt = function(doc, e, temp){ var r = prep0(doc, e, temp); post(doc, e); return r; };
  var prepDoc0 = TC.prepDoc;
  TC.prepDoc = function(doc){ prepDoc0(doc); allEnts(doc).forEach(function(e){ post(doc, e); }); };
  SC.mtCols = colDlg;
  // dời, sao chép, tỉ lệ cho bảng, dung sai, leader nhanh (bảng và khung dung sai luôn nằm ngang)
  var xf0 = ED.xform;
  ED.xform = function(e, t){
    if (e.t === 'table'){ e.p = t.f(e.p); if (t.k !== 1){ e.cols = e.cols.map(function(w){ return w * t.k; }); e.rows = e.rows.map(function(h){ return h * t.k; }); e.th = (e.th || 250) * t.k; } return e; }
    if (e.t === 'tol'){ e.p = t.f(e.p); e.h = (e.h || 220) * t.k; return e; }
    if (e.t === 'qleader'){ e.pts = e.pts.map(t.f); e.h = (e.h || 220) * t.k; if (e.w) e.w *= t.k; return e; }
    return xf0(e, t);
  };
  function dimTextH(d){ var defs = SC.dimDefs ? SC.dimDefs(d) : null, def = defs && defs[ED.cur(d).dimstyle]; return def ? def.txt.h * (def.scale || 1) * (d.space === 'model' || d.activeVp ? 1 : 1 / Math.max(def.scale || 1, 1)) : 220; }
  function boxSegs(o, P, x0, y0, w, h){ o({ k:'seg', a:P([x0, y0]), b:P([x0 + w, y0]) }); o({ k:'seg', a:P([x0 + w, y0]), b:P([x0 + w, y0 - h]) }); o({ k:'seg', a:P([x0 + w, y0 - h]), b:P([x0, y0 - h]) }); o({ k:'seg', a:P([x0, y0 - h]), b:P([x0, y0]) }); }

  /* ================= Dung sai hình học UI-F14 (đối tượng 'tol') ================= */
  // 14 ký hiệu đặc tính theo ISO 1101, vẽ trong ô đơn vị tâm (0, 0), cạnh 1, trục y hướng lên; dùng chung cho bản vẽ và giao diện
  var GDT = [['str', 'Độ thẳng', [['l', -.42, 0, .42, 0]]], ['flat', 'Độ phẳng', [['p', -.46, -.22, .2, -.22, .46, .22, -.2, .22]]], ['circ', 'Độ tròn', [['c', 0, 0, .36]]], ['cyl', 'Độ trụ', [['c', 0, 0, .22], ['l', -.41, -.27, .03, .49], ['l', -.03, -.49, .41, .27]]],
    ['lprof', 'Profile đường', [['a', 0, -.2, .4, 0, 180]]], ['sprof', 'Profile mặt', [['a', 0, -.2, .4, 0, 180], ['l', -.4, -.2, .4, -.2]]], ['ang', 'Độ nghiêng', [['l', -.42, -.32, .44, -.32], ['l', -.42, -.32, .36, .34]]], ['perp', 'Độ vuông góc', [['l', -.42, -.36, .42, -.36], ['l', 0, -.36, 0, .42]]],
    ['par', 'Độ song song', [['l', -.34, -.4, -.04, .4], ['l', .04, -.4, .34, .4]]], ['pos', 'Vị trí', [['c', 0, 0, .26], ['l', -.46, 0, .46, 0], ['l', 0, -.46, 0, .46]]], ['conc', 'Đồng tâm, đồng trục', [['c', 0, 0, .4], ['c', 0, 0, .2]]], ['sym', 'Đối xứng', [['l', -.46, 0, .46, 0], ['l', -.26, .25, .26, .25], ['l', -.26, -.25, .26, -.25]]],
    ['run', 'Độ đảo hướng kính', [['l', -.3, -.38, .24, .34], ['h', .3, .42, -.3, -.38]]], ['trun', 'Độ đảo toàn phần', [['l', -.44, -.38, .1, -.38], ['l', -.44, -.38, -.1, .34], ['l', .1, -.38, .44, .34], ['h', -.06, .42, -.44, -.38], ['h', .48, .42, .1, -.38]]]];
  var MCS = [['', 'Không'], ['M', 'Vật liệu tối đa (MMC)'], ['L', 'Vật liệu tối thiểu (LMC)'], ['S', 'Không phụ thuộc kích thước (RFS)']];
  function gdtOf(k){ return GDT.filter(function(g){ return g[0] === k; })[0]; }
  function ahead(p){ var dx = p[1] - p[3], dy = p[2] - p[4], L = Math.hypot(dx, dy) || 1, ux = dx / L, uy = dy / L, bx = p[1] - ux * .17, by = p[2] - uy * .17; return [[p[1], p[2]], [bx - uy * .075, by + ux * .075], [bx + uy * .075, by - ux * .075]]; }
  function gdtSvg(k){ var g = gdtOf(k), f = function(v){ return Math.round(v * 1000) / 1000; };
    return '<svg class="gdts" viewBox="-.5 -.5 1 1" aria-hidden="true"><g transform="scale(1,-1)">' + (g ? g[2].map(function(p){
      if (p[0] === 'l') return '<line x1="' + p[1] + '" y1="' + p[2] + '" x2="' + p[3] + '" y2="' + p[4] + '"/>';
      if (p[0] === 'c') return '<circle cx="' + p[1] + '" cy="' + p[2] + '" r="' + p[3] + '"/>';
      if (p[0] === 'p') return '<polygon points="' + p.slice(1).join(' ') + '"/>';
      if (p[0] === 'a'){ var a0 = p[4] * Math.PI / 180, a1 = p[5] * Math.PI / 180; return '<path d="M' + f(p[1] + p[3] * Math.cos(a0)) + ' ' + f(p[2] + p[3] * Math.sin(a0)) + 'A' + p[3] + ' ' + p[3] + ' 0 ' + (p[5] - p[4] > 180 ? 1 : 0) + ' 1 ' + f(p[1] + p[3] * Math.cos(a1)) + ' ' + f(p[2] + p[3] * Math.sin(a1)) + '"/>'; }
      return '<polygon class="fl" points="' + ahead(p).map(function(q){ return f(q[0]) + ' ' + f(q[1]); }).join(' ') + '"/>'; }).join('') : '') + '</g></svg>'; }
  function gdtPrims(k, cx, cy, s, P, o, kS){ var g = gdtOf(k); if (!g) return; var Q = function(x, y){ return P([cx + x * s, cy + y * s]); };
    g[2].forEach(function(p){ if (p[0] === 'l') o({ k:'seg', a:Q(p[1], p[2]), b:Q(p[3], p[4]) }); else if (p[0] === 'c') o({ k:'circ', c:Q(p[1], p[2]), r:p[3] * s * kS }); else if (p[0] === 'a') o({ k:'arc', c:Q(p[1], p[2]), r:p[3] * s * kS, a0:p[4], a1:p[5] });
      else if (p[0] === 'p'){ for (var i = 1; i < p.length; i += 2){ var j = i + 2 < p.length ? i + 2 : 1; o({ k:'seg', a:Q(p[i], p[i + 1]), b:Q(p[j], p[j + 1]) }); } } else o({ k:'tri', pts:ahead(p).map(function(q){ return Q(q[0], q[1]); }) }); }); }
  function tolHas(c){ return !!(c && (c.v || c.dia || c.mc)); }
  // khung dung sai: điểm chèn ở giữa cạnh trái hàng đầu; ô ký hiệu vuông, ô dung sai và chuẩn rộng theo chữ
  TC.flatExt.tol = function(x){
    var d = x.doc, e = x.e, P = x.P, o = x.o, h = e.h || 220, H = h * 2, pad = h * .5, kS = x.t ? x.t.k : 1, rS = x.t ? x.t.rot : 0, X0 = e.p[0], top = e.p[1] + H / 2, maxX = X0, rowsN = 0, mcw = h * 1.1;
    var txt = function(s, x0, y){ o({ k:'text', p:P([x0, y]), h:h * kS, s:s, rot:rS, al:'lm', style:'Standard' }); return tw(d, s, h, 'Standard'); };
    var mc = function(m, x0, y){ o({ k:'circ', c:P([x0 + h * .5, y]), r:h * .5 * kS }); o({ k:'text', p:P([x0 + h * .5, y]), h:h * .62 * kS, s:m, rot:rS, al:'m', style:'Standard' }); };
    var cw = function(c){ return !tolHas(c) ? 0 : pad * 2 + (c.dia ? tw(d, '⌀', h, 'Standard') + h * .12 : 0) + tw(d, c.v || '', h, 'Standard') + (c.mc ? mcw + (c.v ? h * .2 : 0) : 0); };
    (e.rows || []).slice(0, 2).forEach(function(R, i){ if (!R || !(R.sym || ['t1', 't2', 'd1', 'd2', 'd3'].some(function(k){ return tolHas(R[k]); }))) return;
      var y0 = top - i * H, ym = y0 - H / 2, xx = X0; boxSegs(o, P, xx, y0, H, H); if (R.sym) gdtPrims(R.sym, xx + H / 2, ym, H * .6, P, o, kS); xx += H;
      ['t1', 't2', 'd1', 'd2', 'd3'].forEach(function(k){ var c = R[k], w = cw(c); if (!w) return; boxSegs(o, P, xx, y0, w, H); var cx = xx + pad; if (c.dia) cx += txt('⌀', cx, ym) + h * .12; if (c.v) cx += txt(c.v, cx, ym); if (c.mc) mc(c.mc, cx + (c.v ? h * .2 : 0), ym); xx += w; });
      maxX = Math.max(maxX, xx); rowsN = i + 1; });
    if (e.ph || e.pz){ var y1 = top - rowsN * H, w = pad * 2 + (e.ph ? tw(d, e.ph, h, 'Standard') : 0) + (e.pz ? mcw + (e.ph ? h * .2 : 0) : 0), cx2 = X0 + H + pad; boxSegs(o, P, X0 + H, y1, w, H); if (e.ph) cx2 += txt(e.ph, cx2, y1 - H / 2) + h * .2; if (e.pz) mc('P', cx2, y1 - H / 2); }
    if (e.did){ var xd = maxX + h * .8, w2 = Math.max(H, pad * 2 + tw(d, e.did, h, 'Standard')); boxSegs(o, P, xd, top, w2, H); o({ k:'text', p:P([xd + w2 / 2, top - H / 2]), h:h * kS, s:e.did, rot:rS, al:'m', style:'Standard' }); }
    if (!rowsN && !e.did) o({ k:'pt', p:P(e.p) });
  };
  function tolSum(R){ if (!R) return ''; var c = function(z){ return tolHas(z) ? (z.dia ? '⌀' : '') + (z.v || '') + (z.mc ? ' (' + z.mc + ')' : '') : ''; }; return [(gdtOf(R.sym) || [0, ''])[1], c(R.t1), c(R.t2), [R.d1, R.d2, R.d3].map(c).filter(Boolean).join(' | ')].filter(Boolean).join(' · '); }
  TC.TYPE_NAMES.tol = 'Dung sai hình học';
  TC.propsExt.tol = function(d, e, f){ return [['Kiểu kích thước', esc(ED.cur(d).dimstyle)], ['Hàng 1', esc(tolSum((e.rows || [])[0]) || '(trống)') + ' <button class="ib sm" type="button" data-tol-edit aria-label="Sửa dung sai" style="margin-left:auto">…</button>', 'html'], ['Hàng 2', esc(tolSum((e.rows || [])[1]) || '(trống)'), 'ro'], ['Ký hiệu chuẩn', esc(e.did || '(không)'), 'ro'], ['Chiều cao', f(e.h || 220), 'num'], ['Vị trí X', f(e.p[0]), 'num'], ['Vị trí Y', f(e.p[1]), 'num']]; };
  document.addEventListener('click', function(ev){ if (!ev.target.closest('[data-tol-edit]')) return; var d = D(), e = d && d.sel.length === 1 && d.byId[d.sel[0]]; if (e && e.t === 'tol') editTol(e); });
  function tolBlank(){ var c = function(){ return { dia:false, v:'', mc:'' }; }, r = function(){ return { sym:'', t1:c(), t2:c(), d1:{ v:'', mc:'' }, d2:{ v:'', mc:'' }, d3:{ v:'', mc:'' } }; }; return { rows:[r(), r()], ph:'', pz:false, did:'' }; }
  SC.tolDlg = function(T0, edit){
    return new Promise(function(res){
      var d = D(), T = Object.assign(tolBlank(), clone(T0 ? { rows:T0.rows, ph:T0.ph, pz:T0.pz, did:T0.did } : {})), h = edit && T0.h || dimTextH(d);
      while (T.rows.length < 2) T.rows.push(tolBlank().rows[0]);
      var cell = function(i, k, datum){ var c = T.rows[i][k]; return '<div class="gtc">' + (datum ? '' : '<button class="gtd" type="button" data-dia="' + i + '.' + k + '" aria-pressed="' + !!c.dia + '" data-tip="Ký hiệu đường kính ⌀" aria-label="Đường kính">⌀</button>') + '<input class="inp mono" data-gv="' + i + '.' + k + '" value="' + esc(c.v) + '" maxlength="' + (datum ? 3 : 12) + '" spellcheck="false" aria-label="' + (datum ? 'Chuẩn ' + k[1] : 'Dung sai ' + k[1]) + ' hàng ' + (i + 1) + '"' + (datum ? ' style="width:44px;text-transform:uppercase"' : ' style="width:76px"') + '><button class="gtm" type="button" data-mc="' + i + '.' + k + '" aria-label="Điều kiện vật liệu" data-tip="Điều kiện vật liệu">' + (c.mc ? '<i>' + c.mc + '</i>' : '·') + '</button></div>'; };
      var row = function(i){ return '<tr><th scope="row" class="faint">' + (i + 1) + '</th><td><button class="gts" type="button" data-sym="' + i + '" aria-label="Ký hiệu đặc tính hàng ' + (i + 1) + '" data-tip="' + esc((gdtOf(T.rows[i].sym) || [0, 'Chọn ký hiệu đặc tính'])[1]) + '">' + gdtSvg(T.rows[i].sym) + '</button></td><td>' + cell(i, 't1') + '</td><td>' + cell(i, 't2') + '</td><td>' + cell(i, 'd1', 1) + '</td><td>' + cell(i, 'd2', 1) + '</td><td>' + cell(i, 'd3', 1) + '</td></tr>'; };
      var dlg = TC.dialog({ id:'tol', title:edit ? 'Sửa dung sai hình học' : 'Dung sai hình học', cmd:'TOLERANCE', code:'UI-F14', w:800, onClose:function(r){ if (r !== true) res(null); },
        body:'<div class="stack"><table class="gtt"><thead><tr><th></th><th>Ký hiệu</th><th>Dung sai 1</th><th>Dung sai 2</th><th>Chuẩn 1</th><th>Chuẩn 2</th><th>Chuẩn 3</th></tr></thead><tbody id="gt-b"></tbody></table>' +
          '<div class="grid2" style="grid-template-columns:300px minmax(0,1fr)"><div class="stack" style="gap:8px">' + H.row('Chiều cao vùng chiếu', '<input class="inp mono" id="gt-ph" value="' + esc(T.ph) + '" style="width:90px" spellcheck="false">', 150) + H.row('Vùng dung sai chiếu', '<button class="gtm" type="button" id="gt-pz" aria-pressed="' + !!T.pz + '" aria-label="Ký hiệu vùng dung sai chiếu">' + (T.pz ? '<i>P</i>' : '·') + '</button>', 150) + H.row('Ký hiệu chuẩn', '<input class="inp mono" id="gt-did" value="' + esc(T.did) + '" maxlength="3" style="width:56px;text-transform:uppercase" spellcheck="false">', 150) + '<span class="fmsg" id="gt-m" aria-live="polite"></span></div>' +
          '<div class="stack" style="gap:4px"><span class="gh">Xem trước</span>' + H.cv('gt-pv', 420, 132) + '</div></div></div>',
        foot:'<span class="faint" style="font-size:var(--fs-s)">Chữ cao ' + TC.d(h, 0) + ' theo kiểu kích thước ' + esc(ED.cur(d).dimstyle) + '.</span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="gt-ok">' + (edit ? 'Cập nhật dung sai' : 'Chèn dung sai') + '</button>' });
      function render(){ $('#gt-b', dlg.el).innerHTML = row(0) + row(1); pv(); }
      function pv(){ var e = { t:'tol', p:[0, 0], h:220, rows:T.rows, ph:T.ph, pz:T.pz, did:T.did, layer:'0' }; pvEnt($('#gt-pv', dlg.el), d, e); }
      var at = function(s){ var p = s.split('.'); return T.rows[Number(p[0])][p[1]]; };
      dlg.el.addEventListener('input', function(ev){ var t = ev.target; if (t.dataset.gv){ var c = at(t.dataset.gv); c.v = /^d/.test(t.dataset.gv.split('.')[1]) ? t.value.toUpperCase().replace(/[^A-Z]/g, '') : t.value; if (c.v !== t.value) t.value = c.v; } if (t.id === 'gt-ph') T.ph = t.value; if (t.id === 'gt-did'){ T.did = t.value.toUpperCase().replace(/[^A-Z]/g, ''); t.value = T.did; } pv(); });
      dlg.el.addEventListener('click', function(ev){ var b = ev.target.closest('button'); if (!b) return; var r = b.getBoundingClientRect();
        if (b.dataset.sym != null){ var i = Number(b.dataset.sym); TC.menu(GDT.map(function(g){ return { label:'<span class="gtmi">' + gdtSvg(g[0]) + '</span>' + esc(g[1]), check:T.rows[i].sym === g[0], act:function(){ T.rows[i].sym = g[0]; render(); $('[data-sym="' + i + '"]', dlg.el).focus(); } }; }).concat([{ sep:1 }, { label:'Không có ký hiệu', act:function(){ T.rows[i].sym = ''; render(); } }]), r.left, r.bottom + 2, { returnFocus:b, minWidth:220 }); }
        if (b.dataset.dia){ var c = at(b.dataset.dia); c.dia = !c.dia; b.setAttribute('aria-pressed', String(c.dia)); pv(); }
        if (b.dataset.mc){ var c2 = at(b.dataset.mc); TC.menu(MCS.map(function(m){ return { label:(m[0] ? '<span class="gtmc">' + m[0] + '</span>' : '<span class="gtmc off">·</span>') + esc(m[1]), check:c2.mc === m[0], act:function(){ c2.mc = m[0]; b.innerHTML = m[0] ? '<i>' + m[0] + '</i>' : '·'; pv(); b.focus(); } }; }), r.left, r.bottom + 2, { returnFocus:b, minWidth:240 }); }
        if (b.id === 'gt-pz'){ T.pz = !T.pz; b.setAttribute('aria-pressed', String(T.pz)); b.innerHTML = T.pz ? '<i>P</i>' : '·'; pv(); } });
      $('#gt-ok', dlg.el).addEventListener('click', function(){ var R0 = T.rows[0], ok = R0.sym || ['t1', 't2'].some(function(k){ return tolHas(R0[k]); }) || T.did, m = $('#gt-m', dlg.el);
        if (!ok){ m.className = 'fmsg err'; m.innerHTML = ic('warn', 'xs') + 'Chọn ký hiệu đặc tính hoặc nhập dung sai cho hàng 1.'; return; }
        if (R0.sym && !tolHas(R0.t1)){ m.className = 'fmsg err'; m.innerHTML = ic('warn', 'xs') + 'Nhập giá trị Dung sai 1 cho ký hiệu ' + esc(gdtOf(R0.sym)[1].toLowerCase()) + '.'; $('[data-gv="0.t1"]', dlg.el).focus(); return; }
        dlg.close(true); res(T); });
      render();
    });
  };
  function editTol(e){ SC.tolDlg(e, true).then(function(T){ if (!T) return; ED.run('Sửa dung sai', function(){ ED.mod(e, function(x){ x.rows = T.rows; x.ph = T.ph; x.pz = T.pz; x.did = T.did; }); }); V().draw(); A().refreshPalettes(); }); }
  IMPL.TOLERANCE = function(ctx, pre){
    ctx.badge('TOL');
    return SC.tolDlg(pre && pre.rows ? pre : null).then(function(T){ if (!T) return; var h = dimTextH(D()), mk = function(p){ return { t:'tol', p:p, h:h, rows:T.rows, ph:T.ph, pz:T.pz, did:T.did }; };
      return ctx.point('Vị trí dung sai (giữa cạnh trái khung)', { rubber:false, onMove:function(p, temp){ temp.push(ED.temp([mk(p)], { real:true })); } }).then(function(r){ if (!r.p) return; ED.run('TOLERANCE', function(){ ED.add(ED.make(mk(r.p))); }); ctx.out('Đã chèn khung dung sai: ' + esc(tolSum(T.rows[0])) + '. Nhấp đúp để sửa.'); }); });
  };

  /* ================= Leader nhanh UI-F13 (QLEADER, gõ LE; đối tượng 'qleader') ================= */
  var QA = ['Mũi tên kín', 'Mũi tên kín rỗng', 'Mũi tên hở', 'Gạch nghiêng kiến trúc', 'Chấm', 'Chấm nhỏ', 'Góc vuông', 'Không'];
  var ATT = [['t1', 'Đỉnh dòng đầu'], ['m1', 'Giữa dòng đầu'], ['mb', 'Giữa khối chữ'], ['mn', 'Giữa dòng cuối'], ['bn', 'Đáy dòng cuối']];
  var ANG = ['Tự do', 'Ngang', '90°', '45°', '30°', '15°'];
  var QK = [['mtext', 'MText'], ['copy', 'Sao chép một đối tượng'], ['tol', 'Dung sai'], ['block', 'Tham chiếu block'], ['none', 'Không']];
  var QS = Object.assign({ kind:'mtext', askW:true, left:false, frame:false, reuse:'none', sp:false, ar:'Mũi tên kín', unlim:false, max:3, a1:'Tự do', a2:'Tự do', attL:'m1', attR:'m1', ul:false }, TC.load('qlset', {})), LASTQ = null;
  function qArrow(o, P, tip, from, kind, h, kS){ var a = Math.atan2(from[1] - tip[1], from[0] - tip[0]), z = h * .9, c = Math.cos(a), s = Math.sin(a), pt = function(l, w){ return [tip[0] + c * l - s * w, tip[1] + s * l + c * w]; };
    if (/kín/.test(kind)){ var tri = [tip, pt(z, z * .3), pt(z, -z * .3)]; if (/rỗng/.test(kind)) for (var i = 0; i < 3; i++) o({ k:'seg', a:P(tri[i]), b:P(tri[(i + 1) % 3]) }); else o({ k:'tri', pts:tri.map(P) }); }
    else if (kind === 'Mũi tên hở'){ o({ k:'seg', a:P(tip), b:P(pt(z, z * .32)) }); o({ k:'seg', a:P(tip), b:P(pt(z, -z * .32)) }); }
    else if (/Gạch nghiêng/.test(kind)) o({ k:'seg', a:P(pt(-z * .4, -z * .4)), b:P(pt(z * .4, z * .4)) });
    else if (/Chấm/.test(kind)){ var r = kind === 'Chấm nhỏ' ? z * .14 : z * .26, ring = []; for (var k = 0; k < 16; k++) ring.push(P([tip[0] + r * Math.cos(k * Math.PI / 8), tip[1] + r * Math.sin(k * Math.PI / 8)])); o({ k:'hatch', loops:[ring], pat:'SOLID', sc:1, ang:0 }); o({ k:'circ', c:P(tip), r:r * kS }); }
    else if (kind === 'Góc vuông'){ o({ k:'seg', a:P(pt(z * .5, z * .5)), b:P(tip) }); o({ k:'seg', a:P(tip), b:P(pt(z * .5, -z * .5)) }); } }
  function qLines(d, e, h){ return e.w > 0 ? wrapM(d, e.s || '', e.w, h, e.style || 'Standard') : String(e.s || '').split('\n'); }
  // đường dẫn thẳng hoặc spline, mũi tên, đoạn móc ngang, chữ gắn theo thiết lập (đỉnh, giữa dòng đầu, giữa khối, giữa/đáy dòng cuối), gạch chân, khung
  TC.flatExt.qleader = function(x){
    var d = x.doc, e = x.e, P = x.P, o = x.o, h = e.h || 220, kS = x.t ? x.t.k : 1, rS = x.t ? x.t.rot : 0, src = e.pts, pts = e.sp && src.length > 2 ? TC.splinePts(src, false) : src, n = pts.length, i;
    for (i = 0; i < n - 1; i++) o({ k:'seg', a:P(pts[i]), b:P(pts[i + 1]) });
    if (n >= 2) qArrow(o, P, pts[0], pts[1], e.ar || 'Mũi tên kín', h, kS);
    if (n < 2) return; var L = src[src.length - 1], Lp = src[src.length - 2], dir = L[0] >= Lp[0] ? 1 : -1, an = Math.abs(G.ang(Lp, L)) % 180, A0 = L;
    if (an > 3 && an < 177){ A0 = [L[0] + dir * h, L[1]]; o({ k:'seg', a:P(L), b:P(A0) }); }
    var gap = h * .45, ax = A0[0] + dir * gap, ay = A0[1], ann = e.ann || 'mtext';
    if (ann === 'mtext' && e.s){ var ls = qLines(d, e, h), N = ls.length, lh = h * 1.6667, att = dir > 0 ? (e.attR || 'm1') : (e.attL || 'm1'), y1 = att === 't1' ? ay - h : att === 'm1' ? ay - h / 2 : att === 'mb' ? ay + (h + (N - 1) * lh) / 2 - h : att === 'mn' ? ay - h / 2 + (N - 1) * lh : ay + h * .3 + (N - 1) * lh, st = e.style || 'Standard';
      if (e.ul) y1 = ay + h * .4 + (N - 1) * lh;
      var W = Math.max.apply(null, ls.map(function(l){ return tw(d, l, h, st); }).concat([h])), xl = dir > 0 || e.left ? (dir > 0 ? ax : ax - W) : ax;
      ls.forEach(function(l, k){ o({ k:'text', p:P([xl, y1 - k * lh]), h:h * kS, s:l, rot:rS, al:dir > 0 || e.left ? 'l' : 'r', style:st }); });
      var x0 = dir > 0 || e.left ? xl : ax - W, x1 = x0 + W;
      if (e.ul) o({ k:'seg', a:P([A0[0], ay]), b:P([dir > 0 ? x1 : x0, ay]) });
      if (e.fr){ var m = h * .35; boxSegs(o, P, x0 - m, y1 + h + m, W + 2 * m, h + (N - 1) * lh + h * .3 + 2 * m); } }
    else if (ann === 'tol' && e.tol){ var te = TC.prepEnt(d, { t:'tol', p:[0, 0], h:h, rows:e.tol.rows, ph:e.tol.ph, pz:e.tol.pz, did:e.tol.did }, true), bw = te._bb[2] - te._bb[0]; x.sub({ t:'tol', p:[dir > 0 ? ax : ax - bw, ay], h:h, rows:e.tol.rows, ph:e.tol.ph, pz:e.tol.pz, did:e.tol.did }); }
    else if (ann === 'block' && e.blk && d.blocks[e.blk]){ var bi = TC.prepEnt(d, { t:'insert', name:e.blk, p:[0, 0], sx:1, sy:1, rot:0, layer:e.layer }, true), bb = bi._bb; x.sub({ t:'insert', name:e.blk, p:[dir > 0 ? ax - bb[0] : ax - bb[2], ay - (bb[1] + bb[3]) / 2], sx:1, sy:1, rot:0, layer:e.layer }); }
    else if (ann === 'copy' && e.cp){ var c = clone(e.cp); if (c.p){ var bb2 = TC.prepEnt(d, clone(c), true)._bb, dx = (dir > 0 ? ax - bb2[0] : ax - bb2[2]), dy = ay - (bb2[1] + bb2[3]) / 2; c = ED.xform(c, { f:function(p){ return [p[0] + dx, p[1] + dy]; }, k:1, rot:0 }); } x.sub(c); }
  };
  TC.TYPE_NAMES.qleader = 'Leader';
  TC.propsExt.qleader = function(d, e, f){ return [['Nội dung', esc(String(e.s || '').replace(/\n/g, ' ¶ '))], ['Loại chú thích', (QK.filter(function(q){ return q[0] === (e.ann || 'mtext'); })[0] || QK[0])[1], 'ro'], ['Kiểu đường dẫn', e.sp ? 'Spline' : 'Thẳng', 'ro'], ['Mũi tên', esc(e.ar || 'Mũi tên kín'), 'ro'], ['Số điểm', String(e.pts.length), 'ro'], ['Chiều cao', f(e.h || 220), 'num'], ['Gạch chân dòng cuối', e.ul ? 'Có' : 'Không', 'ro']]; };
  function qSample(S, d){ return { t:'qleader', pts:[[0, 0], [1100, 900], [2300, 900]], sp:S.sp, ar:S.ar, h:220, s:S.kind === 'mtext' ? 'Tường xây gạch\nchống thấm 2 lớp' : '', ann:S.kind, ul:S.ul, fr:S.frame, left:S.left, attL:S.attL, attR:S.attR, w:0, tol:S.kind === 'tol' ? { rows:[{ sym:'pos', t1:{ dia:true, v:'0.05', mc:'M' }, d1:{ v:'A', mc:'' } }] } : null, blk:'TCVN-CAO-DO', cp:S.kind === 'copy' ? { t:'text', p:[0, 0], h:220, s:'Ghi chú sao chép', al:'l', style:'Standard', layer:'0' } : null, layer:'0' }; }
  SC.qlSettings = function(){
    return new Promise(function(res){
      var d = D(), S = clone(QS);
      var rad = function(n, list, cur){ return list.map(function(x){ return H.rad(n, x[0], x[1], x[0] === cur); }).join(''); };
      var dlg = TC.dialog({ id:'qlset', title:'Thiết lập leader nhanh', cmd:'QLEADER', code:'UI-F13', w:820, onClose:function(r){ res(r === true); },
        body:'<div class="qlw"><div>' + H.tabs([['a', 'Chú thích'], ['l', 'Đường dẫn và mũi tên'], ['t', 'Gắn chữ']], 'a', 'qlt') +
          H.pane('a', 'a', '<div class="grid2">' + H.grp('Loại chú thích', rad('ql-k', QK, S.kind)) + '<div class="stack" style="gap:10px">' + H.grp('Tùy chọn MText', H.chk('ql-w', 'Hỏi bề rộng chữ', S.askW) + H.chk('ql-left', 'Luôn canh trái', S.left) + H.chk('ql-fr', 'Có khung chữ', S.frame), 'qlmt') + H.grp('Dùng lại chú thích', rad('ql-re', [['none', 'Không'], ['next', 'Dùng lại cho lần sau'], ['cur', 'Dùng lại chú thích hiện tại']], S.reuse)) + '</div></div>', 'tep') +
          H.pane('l', 'a', '<div class="grid2">' + H.grp('Đường dẫn', rad('ql-sp', [['0', 'Thẳng'], ['1', 'Spline']], S.sp ? '1' : '0')) + H.grp('Mũi tên', H.sel('ql-ar', QA, S.ar, 200)) + H.grp('Số điểm', H.chk('ql-un', 'Không giới hạn', S.unlim) + H.row('Tối đa', H.num('ql-mx', String(S.max), { prec:0, unit:'none', w:90, pick:false, min:1 }), 60)) + H.grp('Ràng buộc góc', H.row('Đoạn thứ nhất', H.sel('ql-a1', ANG, S.a1, 110), 110) + H.row('Đoạn thứ hai', H.sel('ql-a2', ANG, S.a2, 110), 110)) + '</div>', 'tep') +
          H.pane('t', 'a', H.grp('Gắn MText', '<div class="qlatt"><span></span><span class="gh">Chữ bên trái</span><span class="gh">Chữ bên phải</span>' + ATT.map(function(a){ return '<span>' + a[1] + '</span><span><input type="radio" name="ql-al" value="' + a[0] + '" aria-label="Chữ bên trái: ' + a[1] + '"' + (S.attL === a[0] ? ' checked' : '') + '></span><span><input type="radio" name="ql-ar2" value="' + a[0] + '" aria-label="Chữ bên phải: ' + a[1] + '"' + (S.attR === a[0] ? ' checked' : '') + '></span>'; }).join('') + '</div>' + H.chk('ql-ul', 'Gạch chân dòng cuối', S.ul)), 'tep') +
          '</div><div class="stack"><span class="gh">Xem trước</span>' + H.cv('ql-pv', 300, 190) + '<p class="faint" style="font-size:var(--fs-s);margin:0" id="ql-n"></p></div></div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="ql-ok">Lưu thiết lập</button>' });
      function upd(){ var mt = S.kind === 'mtext'; $$('.qlmt input', dlg.el).forEach(function(i){ i.disabled = !mt; }); $('.qlmt', dlg.el).classList.toggle('is-dim', !mt); var mx = $('#ql-mx', dlg.el); mx.disabled = S.unlim; mx.closest('.numf').classList.toggle('is-dis', S.unlim); $$('[name="ql-al"], [name="ql-ar2"], #ql-ul', dlg.el).forEach(function(i){ i.disabled = !mt; });
        pvEnt($('#ql-pv', dlg.el), d, qSample(S, d)); $('#ql-n', dlg.el).textContent = 'LE: ' + (S.unlim ? 'không giới hạn số điểm' : 'tối đa ' + S.max + ' điểm') + ' · ' + (QK.filter(function(q){ return q[0] === S.kind; })[0][1]) + (S.kind === 'mtext' && S.askW ? ' · hỏi bề rộng chữ' : '') + '.'; }
      dlg.el.addEventListener('change', function(ev){ var t = ev.target, n = t.name, id = t.id;
        if (n === 'ql-k') S.kind = t.value; if (n === 'ql-re') S.reuse = t.value; if (n === 'ql-sp') S.sp = t.value === '1'; if (n === 'ql-al') S.attL = t.value; if (n === 'ql-ar2') S.attR = t.value;
        if (id === 'ql-w') S.askW = t.checked; if (id === 'ql-left') S.left = t.checked; if (id === 'ql-fr') S.frame = t.checked; if (id === 'ql-un') S.unlim = t.checked; if (id === 'ql-ul') S.ul = t.checked; if (id === 'ql-ar') S.ar = t.value; if (id === 'ql-a1') S.a1 = t.value; if (id === 'ql-a2') S.a2 = t.value; upd(); });
      dlg.el.addEventListener('numchange', function(ev){ if (ev.detail >= 2) S.max = Math.min(50, Math.round(ev.detail)); else { var i = $('#ql-mx', dlg.el); i.value = String(S.max); TC.toast('Leader cần ít nhất 2 điểm.', { type:'warn', log:false }); } upd(); });
      $('#ql-ok', dlg.el).addEventListener('click', function(){ Object.assign(QS, S); TC.save('qlset', QS); if (QS.reuse === 'none') LASTQ = null; dlg.close(true); });
      upd();
    });
  };
  function qCons(prev, p, a){ if (!a || a === 'Tự do') return p; if (a === 'Ngang') return [p[0], prev[1]]; var dx = p[0] - prev[0], dy = p[1] - prev[1], L = Math.hypot(dx, dy), st = parseInt(a, 10), an = Math.round(Math.atan2(dy, dx) * 180 / Math.PI / st) * st * Math.PI / 180; return [prev[0] + L * Math.cos(an), prev[1] + L * Math.sin(an)]; }
  function qEdit(e){ var h = e.h || 220, L = e.pts[e.pts.length - 1], Lp = e.pts[e.pts.length - 2] || L, dir = L[0] >= Lp[0] ? 1 : -1, pr = { t:'mtext', p:[L[0] + dir * h * (dir > 0 ? 1.5 : -10), L[1] + h * .9], w:e.w || h * 16, h:h, s:e.s || '', style:e.style || 'Standard', layer:e.layer, _paper:e._paper };
    return ED.mtextEdit(pr).then(function(r){ if (!r) return null; return r.text; }); }
  IMPL.QLEADER = function(ctx){
    ctx.badge('LE'); var d = D(), S = QS, h = dimTextH(d), pts = [];
    var mk = function(list, extra){ return Object.assign({ t:'qleader', pts:list, sp:S.sp, ar:S.ar, h:h, ann:'none', ul:S.ul, fr:S.frame, left:S.left, attL:S.attL, attR:S.attR, w:0, style:ED.cur(d).tstyle }, extra || {}); };
    function first(){ return ctx.point('Điểm đầu đường dẫn', { opts:[['Thiết lập', 'S']], def:'Thiết lập' }).then(function(r){ if (r.enter || r.kw === 'S') return SC.qlSettings().then(function(){ S = QS; return first(); }); if (!r.p) return; pts = [r.p]; return next(); }); }
    function next(){ var max = S.unlim ? Infinity : Math.max(2, S.max), seg = pts.length === 1 ? S.a1 : pts.length === 2 ? S.a2 : 'Tự do', last = pts[pts.length - 1];
      if (pts.length >= max) return annot();
      return ctx.point('Điểm tiếp theo', { base:last, rubber:false, def:pts.length > 1 ? 'chú thích' : null, onMove:function(p, temp){ temp.push(ED.temp([mk(pts.concat([qCons(last, p, seg)]))], { real:true })); } }).then(function(r){
        if (r.enter){ if (pts.length < 2){ ctx.out('Leader cần ít nhất 2 điểm.'); return next(); } return annot(); }
        if (!r.p) return; pts.push(qCons(last, r.p, seg)); return next(); }); }
    function make(extra, msg){ var e; ED.run('QLEADER', function(){ e = ED.add(ED.make(mk(pts, extra))); }); V().temp = []; V().draw(); ctx.out('Đã tạo leader ' + pts.length + ' điểm' + (msg ? ': ' + msg : '') + '.'); return e; }
    function annot(){
      if (S.kind === 'none') return make({}, 'không có chú thích');
      if (S.kind === 'tol') return SC.tolDlg(null).then(function(T){ if (T) make({ ann:'tol', tol:{ rows:T.rows, ph:T.ph, pz:T.pz, did:T.did } }, 'dung sai ' + esc(tolSum(T.rows[0]))); });
      if (S.kind === 'block') return ctx.val('Tên block', QS.blk || 'TCVN-CAO-DO').then(function(v){ if (!d.blocks[v] && !(SC.ensureBlock && SC.ensureBlock(d, v))){ ctx.out('Không có block “' + esc(v) + '” trong bản vẽ. Chèn từ palette Blocks hoặc Thư viện TCVN trước.'); return; } QS.blk = v; TC.save('qlset', QS); make({ ann:'block', blk:v }, 'block ' + esc(v)); });
      if (S.kind === 'copy') return ED.pick(ctx, 'Chọn chữ, MText, dung sai hoặc block cần sao chép', { types:['text', 'mtext', 'tol', 'insert'] }).then(function(r){ if (r && r.e){ var c = ED.clean(r.e); delete c.id; make({ ann:'copy', cp:c }, 'sao chép ' + esc(TC.TYPE_NAMES[r.e.t] || r.e.t)); } });
      var w = 0, ask = S.askW ? ctx.point('Bề rộng chữ', { base:pts[pts.length - 1], num:true, def:'0', dyn:false }).then(function(r){ w = r.num != null ? Math.max(0, r.num) : r.p ? Math.abs(r.p[0] - pts[pts.length - 1][0]) : 0; }) : Promise.resolve();
      return ask.then(function(){
        if (S.reuse !== 'none' && LASTQ != null){ ctx.out('Dùng lại chú thích: ' + esc(LASTQ.replace(/\n/g, ' ¶ '))); make({ ann:'mtext', s:LASTQ, w:w }); return; }
        var lines = [];
        return ctx.val('Dòng chữ đầu tiên của chú thích', 'MText', { allowSpace:true }).then(function(v){
          var keep = function(s){ if (S.reuse !== 'none') LASTQ = s; make({ ann:'mtext', s:s, w:w }); };
          if (v === 'MText'){ var tmp = ED.temp([mk(pts)], { real:true }); V().temp = [tmp]; V().drawOver(); return qEdit(mk(pts, { w:w })).then(function(s){ V().temp = []; if (s && s.trim()) keep(s); else ctx.out('Không có chữ, leader không được tạo.'); }); }
          lines.push(v); var more = function(){ return ctx.val('Dòng chữ tiếp theo', '', { allowSpace:true }).then(function(v2){ if (!v2) return keep(lines.join('\n')); lines.push(v2); return more(); }); }; return more(); }); });
    }
    return first();
  };

  /* ================= Tỉ lệ chú thích của đối tượng UI-F11 (OBJECTSCALE) ================= */
  var SCL0 = ['1:1', '1:2', '1:5', '1:10', '1:20', '1:25', '1:50', '1:75', '1:100', '1:125', '1:200', '1:250', '1:500', '1:1000', '1:2000', '1:5000'];
  var ANNT = /^(text|mtext|dim|dimr|dima|mleader|qleader|tol|hatch|insert|attdef|table)$/;
  function ratio(s){ var p = String(s).split(':').map(Number); return p[1] ? p[0] / p[1] : 0; }
  SC.objScale = function(ids){
    var d = D(), ents = ids.map(function(id){ return d.byId[id]; }).filter(Boolean), cur = '1:100', can = ents.filter(function(e){ return ANNT.test(e.t) && !e.anno; });
    var ann = function(){ return ents.filter(function(e){ return e.anno; }); }, S = {}, mode = 'all', sel = null;
    ann().forEach(function(e){ S[e.id] = (e.scales || [cur]).slice(); });
    var dlg = TC.dialog({ id:'oscale', title:'Tỉ lệ chú thích của đối tượng', cmd:'OBJECTSCALE', code:'UI-F11', w:600, body:'<div id="os-b"></div>', foot:'<span class="faint" style="font-size:var(--fs-s)">Tỉ lệ chú thích hiện hành: <b class="mono" style="color:var(--c-text)">' + cur + '</b></span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="os-ok"></button>' });
    function list(){ var A0 = ann(), all = {}, cnt = {}; A0.forEach(function(e){ S[e.id].forEach(function(s){ all[s] = 1; cnt[s] = (cnt[s] || 0) + 1; }); });
      return Object.keys(all).filter(function(s){ return mode === 'all' || cnt[s] === A0.length; }).sort(function(a, b){ return ratio(b) - ratio(a); }).map(function(s){ return [s, cnt[s]]; }); }
    function render(){ var A0 = ann(), b = $('#os-b', dlg.el), ok = $('#os-ok', dlg.el);
      if (!A0.length){ b.innerHTML = '<div class="empty">' + ic('anno') + '<b>Không có đối tượng chú thích theo tỉ lệ</b><span>' + ents.length + ' đối tượng đã chọn chưa bật “Chú thích theo tỉ lệ”, nên chưa gắn tỉ lệ nào.</span>' + (can.length ? '<button class="btn sm" type="button" id="os-on">Bật chú thích theo tỉ lệ cho ' + can.length + ' đối tượng</button>' : '<span class="faint">Chọn chữ, kích thước, multileader, hatch hoặc block rồi chạy lại OBJECTSCALE.</span>') + '</div>'; ok.disabled = true; ok.textContent = 'Lưu tỉ lệ'; return; }
      var L = list(); if (sel && !L.some(function(x){ return x[0] === sel; })) sel = null;
      b.innerHTML = '<div class="osw"><div class="stack" style="gap:8px"><p style="margin:0">' + A0.length + ' đối tượng chú thích theo tỉ lệ' + (A0.length < ents.length ? ' (trong ' + ents.length + ' đối tượng đã chọn)' : '') + '.</p><div class="tblwrap" style="height:214px"><table class="tbl"><colgroup><col style="width:156px"><col><col><col style="width:118px"></colgroup><thead><tr><th>Tỉ lệ</th><th class="num">Đơn vị giấy</th><th class="num">Đơn vị bản vẽ</th><th>Gắn với</th></tr></thead><tbody>' +
        (L.length ? L.map(function(x){ var p = x[0].split(':'); return '<tr data-osr="' + x[0] + '" class="' + (x[0] === sel ? 'is-sel' : '') + '"><td class="mono">' + x[0] + (x[0] === cur ? ' <span class="tag acc" style="height:15px">hiện hành</span>' : '') + '</td><td class="num">' + p[0] + '</td><td class="num">' + p[1] + '</td><td>' + x[1] + '/' + A0.length + ' đối tượng</td></tr>'; }).join('') : '<tr><td colspan="4" class="faint">Các đối tượng không có tỉ lệ chung. Chọn “Mọi tỉ lệ” để xem từng tỉ lệ.</td></tr>') + '</tbody></table></div>' +
        '<div class="hrow">' + H.rad('os-m', 'all', 'Mọi tỉ lệ gắn với các đối tượng', mode === 'all') + H.rad('os-m', 'common', 'Chỉ tỉ lệ chung', mode === 'common') + '</div></div><div class="stack ds-btns"><button class="btn" type="button" id="os-add">Thêm…</button><button class="btn" type="button" id="os-del"' + (sel ? '' : ' disabled') + '>Xóa</button></div></div>';
      ok.disabled = false; ok.textContent = 'Lưu tỉ lệ cho ' + A0.length + ' đối tượng'; }
    function addDlg(){ var have = {}; ann().forEach(function(e){ S[e.id].forEach(function(s){ have[s] = (have[s] || 0) + 1; }); }); var pool = (d.scales || SCL0).filter(function(s){ return have[s] !== ann().length; }), pick = {}, q = '';
      var a = TC.dialog({ id:'osadd', title:'Thêm tỉ lệ cho đối tượng', code:'UI-F11', w:420, help:false, body:'<div class="stack" style="gap:6px"><label class="search">' + ic('search') + '<input id="oa-q" placeholder="Tìm tỉ lệ, ví dụ 1:50" aria-label="Tìm tỉ lệ"></label><div class="tree osadd" id="oa-l"></div><span class="faint" style="font-size:var(--fs-xs)">Danh sách theo Danh sách tỉ lệ (SCALELISTEDIT).</span></div>', foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="oa-ok" disabled>Thêm tỉ lệ</button>' });
      function l2(){ var items = pool.filter(function(s){ return !q || s.indexOf(q) >= 0; }); $('#oa-l', a.el).innerHTML = items.length ? items.map(function(s){ return '<label class="chk osi"><input type="checkbox" data-oa="' + s + '"' + (pick[s] ? ' checked' : '') + '> <span class="mono">' + s + '</span>' + (have[s] ? '<span class="faint" style="margin-left:auto;font-size:var(--fs-xs)">đã có ở ' + have[s] + ' đối tượng</span>' : '') + '</label>'; }).join('') : '<div class="empty" style="padding:12px">Không có tỉ lệ khớp.</div>'; var n = Object.keys(pick).length; $('#oa-ok', a.el).disabled = !n; $('#oa-ok', a.el).textContent = n ? 'Thêm ' + n + ' tỉ lệ' : 'Thêm tỉ lệ'; }
      a.el.addEventListener('change', function(ev){ var s = ev.target.dataset.oa; if (s){ if (ev.target.checked) pick[s] = 1; else delete pick[s]; l2(); } });
      $('#oa-q', a.el).addEventListener('input', function(){ q = this.value.trim(); l2(); });
      $('#oa-ok', a.el).addEventListener('click', function(){ ann().forEach(function(e){ Object.keys(pick).forEach(function(s){ if (S[e.id].indexOf(s) < 0) S[e.id].push(s); }); }); a.close(true); render(); });
      l2(); }
    dlg.el.addEventListener('click', function(ev){ var r = ev.target.closest('[data-osr]'); if (r){ sel = r.dataset.osr; render(); return; }
      if (ev.target.closest('#os-add')) addDlg();
      if (ev.target.closest('#os-on')){ ED.run('Chú thích theo tỉ lệ', function(){ can.forEach(function(e){ ED.mod(e, function(x){ x.anno = true; x.scales = [cur]; }); }); }); can.forEach(function(e){ S[e.id] = [cur]; }); can = []; A().refreshPalettes(); render(); }
      if (ev.target.closest('#os-del') && sel){ var stuck = ann().filter(function(e){ return S[e.id].length === 1 && S[e.id][0] === sel; }).length; if (stuck){ TC.toast('Mỗi đối tượng chú thích cần ít nhất một tỉ lệ: ' + stuck + ' đối tượng chỉ còn ' + sel + '. Thêm tỉ lệ khác trước khi xóa.', { type:'warn', log:false }); return; } ann().forEach(function(e){ S[e.id] = S[e.id].filter(function(s){ return s !== sel; }); }); sel = null; render(); } });
    dlg.el.addEventListener('change', function(ev){ if (ev.target.name === 'os-m'){ mode = ev.target.value; render(); } });
    $('#os-ok', dlg.el).addEventListener('click', function(){ var A0 = ann(); ED.run('OBJECTSCALE', function(){ A0.forEach(function(e){ ED.mod(e, function(x){ x.scales = S[e.id].slice(); }); }); }); dlg.close(true); A().refreshPalettes(); TC.cmdline.msg('Đã lưu tỉ lệ chú thích cho ' + A0.length + ' đối tượng.'); });
    render(); return dlg;
  };
  IMPL.OBJECTSCALE = function(ctx){ return ctx.sel('Chọn đối tượng chú thích').then(function(ids){ if (ids.length) SC.objScale(ids); }); };

  /* ================= nhấp đúp, menu chuột phải cho đối tượng mới ================= */
  var hdbl0 = TC.cad.hook.dbl;
  TC.cad.hook.dbl = function(e, m){
    if (e && !TC.isViewer() && (e.t === 'tol' || e.t === 'qleader') && !(D().layerMap[e.layer] || {}).lck){
      if (e.t === 'tol') editTol(e); else if ((e.ann || 'mtext') === 'tol' && e.tol) SC.tolDlg(e.tol, true).then(function(T){ if (T) ED.run('Sửa dung sai', function(){ ED.mod(e, function(x){ x.tol = { rows:T.rows, ph:T.ph, pz:T.pz, did:T.did }; }); }); }); else qEdit(e).then(function(s){ if (s != null && s !== e.s) ED.run('TEXTEDIT', function(){ ED.mod(e, function(x){ x.s = s; x.ann = 'mtext'; }); }); });
      return true; }
    return hdbl0 ? hdbl0(e, m) : false;
  };
  TC.cad.ctxExt.push(function(it, first, d){ if (!first || TC.isViewer()) return;
    if (first.t === 'tol') it.push({ label:'Sửa dung sai…', act:function(){ editTol(first); } });
    if (first.t === 'qleader') it.push({ label:'Sửa chữ leader', act:function(){ TC.cad.hook.dbl(first); } });
    if (d.sel.some(function(id){ var e = d.byId[id]; return e && ANNT.test(e.t); })) it.push({ label:'Tỉ lệ chú thích của đối tượng…', icon:'anno', act:function(){ TC.cmd.run('OBJECTSCALE'); } }); });

  /* ================= danh mục màn hình (Đợt 4) ================= */
  function demoTable(d){
    var e = d.ents.filter(function(x){ return x.t === 'table' && x.demo === 'F07'; })[0]; if (e) return e;
    var rooms = [['Phòng khách + ăn', '35.39'], ['Bếp', '17.31'], ['P. Ngủ 1', '15.07'], ['P. Ngủ 2', '15.07'], ['Hành lang, cầu thang', '7.54'], ['WC', '3.27'], ['Kho', '3.85']], ts = tstyles(d)['TCVN-dien-tich'];
    var cells = [['BẢNG THỐNG KÊ DIỆN TÍCH', '', ''], ['STT', 'Tên phòng', 'Diện tích (m²)']].concat(rooms.map(function(r, i){ return [String(i + 1), r[0], r[1]]; })).concat([['', 'Tổng cộng', '=Sum(C3:C9)']]), rs = ['title', 'header'].concat(rooms.map(function(){ return 'data'; })).concat(['data']);
    ED.run('TABLE', function(){ e = ED.add(ED.make({ t:'table', p:[13600, 17000], cols:[1000, 4600, 2800], rows:rs.map(function(k){ return rowH(ts.cells[k], 1, 100); }), cells:cells, merge:[[0, 0, 2]], rs:rs, head:2, th:250, tstyle:'TCVN-dien-tich', layer:'KT-TEXT', demo:'F07', cf:{ '9,1':{ al:'MR' }, '9,2':{ dt:'Số thập phân', prec:2 } } })); });
    return e;
  }
  function demoMtext(d){
    var e = d.ents.filter(function(x){ return x.t === 'mtext' && x.demo === 'F02'; })[0]; if (e) return e;
    ED.run('MTEXT', function(){ e = ED.add(ED.make({ t:'mtext', p:[-11500, 16800], w:7600, h:250, s:'THUYẾT MINH HOÀN THIỆN\nSàn lát gạch ceramic 600×600, vữa lót mác 75 dày 20. Ống thoát sàn ⌀ 3/4 in.\nTrần thạch cao khung chìm, sơn nước 1 lớp lót, 2 lớp phủ.\nTường trát vữa xi măng mác 75, bả matit, sơn hoàn thiện.', style:'Standard', ls:1.6667, layer:'KT-GHICHU', demo:'F02' })); });
    return e;
  }
  function withEditor(fn){ var d = SC.cat.plan(), e = demoMtext(d); V().zoomTo([-12600, 11800, -1200, 17600]); setTimeout(function(){ TC.cmd.run('TEXTEDIT', e); setTimeout(function(){ var M = ED.mte && ED.mte(); if (M) fn(M); }, 200); }, 80); }
  OPEN['UI-D04'] = function(){ SC.cat.plan(); TC.cmd.run('TABLESTYLE', 'TCVN-thong-ke-thep'); };
  OPEN['UI-F06'] = function(){ SC.cat.plan(); V().zoomTo([4000, 9000, 26400, 18400]); setTimeout(function(){ TC.cmd.run('TABLE'); }, 60); };
  OPEN['UI-F07'] = function(){ var d = SC.cat.plan(), e = demoTable(d); V().zoomTo([11800, 9800, 24200, 17900]); setTimeout(function(){ startCell(e, 9, 2); TC.toast('Bấm vào ô để chọn, kéo để chọn nhiều ô; nhấp đúp hoặc F2 để sửa; Tab, Enter để di chuyển. Ô C10 là công thức Sum(C3:C9).', { type:'info', log:false, ms:8000 }); }, 80); };
  OPEN['UI-F08'] = function(){ var d = SC.cat.plan(), b = d.ents.filter(function(x){ return x.t === 'pline' && x.demo === 'F08'; })[0];
    if (!b) ED.run('BOUNDARY', function(){ b = ED.add(ED.make({ t:'pline', pts:[[110, 4855], [3845, 4855], [3845, 8890], [110, 8890]], closed:true, layer:'Defpoints', demo:'F08' })); });
    TC.cmd.run('FIELD', { k:'Object', f:'(không)', o:{ id:b.id, prop:'area', u:'m²', pre:'S = ', prec:2 } }); };
  OPEN['UI-F02'] = function(){ withEditor(function(M){ var i = M.ta.value.indexOf('3/4'); if (i >= 0) M.ta.setSelectionRange(i, i + 3); SC.mtStack(M, 'props'); }); };
  OPEN['UI-F03'] = function(){ withEditor(function(M){ SC.mtPara(M); }); };
  OPEN['UI-F04'] = function(){ withEditor(function(M){ M.w(3700); M.F.col = { k:'s', n:2, h:2600, gap:500, auto:true }; M.apply(); SC.mtCols(M); }); };
  OPEN['UI-F11'] = function(){ var d = SC.cat.plan(), ts = d.ents.filter(function(x){ return x.t === 'text' && x.layer === 'KT-TEXT' && x.p[0] < 13000 && /^(P\. NGỦ|BẾP|PHÒNG KHÁCH)/.test(x.s); });
    ts.forEach(function(e, i){ if (!e.anno){ e.anno = true; e.scales = i ? ['1:100', '1:50'] : ['1:100', '1:50', '1:20']; } }); d.sel = ts.map(function(e){ return e.id; }); TC.cad.afterSel(); setTimeout(function(){ TC.cmd.run('OBJECTSCALE'); }, 60); };
  OPEN['UI-F13'] = function(){ SC.cat.plan(); TC.cmd.run('QLEADER'); setTimeout(function(){ TC.cmdline.submit('S'); }, 150); };
  OPEN['UI-F14'] = function(){ SC.cat.plan(); TC.cmd.run('TOLERANCE', { rows:[{ sym:'pos', t1:{ dia:true, v:'0.05', mc:'M' }, t2:{ dia:false, v:'', mc:'' }, d1:{ v:'A', mc:'' }, d2:{ v:'B', mc:'M' }, d3:{ v:'', mc:'' } }, { sym:'perp', t1:{ dia:false, v:'0.02', mc:'' }, t2:{ dia:false, v:'', mc:'' }, d1:{ v:'A', mc:'' }, d2:{ v:'', mc:'' }, d3:{ v:'', mc:'' } }], ph:'', pz:false, did:'' }); };
})();
