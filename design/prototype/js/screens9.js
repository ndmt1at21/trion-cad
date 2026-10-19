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
      'TCVN-dien-tich':{ tcvn:true, desc:'Bảng thống kê diện tích phòng: tên phòng canh trái, diện tích canh phải, tiêu đề 3.5 mm, viền ngoài 0.5 mm.', dir:'down', cells:{ title:cst({ th:3.5, b:true, mv:1.5, merge:true }, { t:0, l:0, r:0, b:.5 }), header:cst({ th:2.5, b:true }, T), data:cst({ al:'auto', kind:'Dữ liệu' }, { l:.5, r:.5, b:.5 }) } }
    };
  }
  function tstyles(d){ return d.tableStyles || (d.tableStyles = defStyles()); }
  function tsOf(d, e){ var S = tstyles(d); return (e && e._ts) || S[e && e.tstyle] || S.Standard; }
  // tên kiểu bảng cho dropdown trên ribbon (ribbons.js) và kiểu hiện hành (ED.cur(d).bstyle)
  SC.tableStyleNames = function(d){ return Object.keys(tstyles(d || D())); };
  SC.curTableStyle = function(d){ d = d || D(); var n = ED.cur(d).bstyle; return n && tstyles(d)[n] ? n : 'Standard'; };
  SC.setTableStyle = function(d, n){ ED.cur(d || D()).bstyle = n; };
  function reprep(d){ allEnts(d).forEach(function(e){ if (e.t === 'table' || (e.fld && e.fld.length)) TC.prepEnt(d, e); }); V().draw(); }
  function annoK(d){ return d.space === 'model' || d.activeVp ? 100 : 1; } // 1:100 trong Model, 1:1 trên giấy
  function hkOf(ts, e){ return (e.th || 250) / ((ts.cells.data || {}).th || 2.5); }
  function rowH(cs, lines, hk){ return Math.round(hk * (cs.th * 1.5 * (lines || 1) + 2 * cs.mv)); }

  /* ================= Bảng: hình học, công thức, định dạng, vẽ ================= */
  function tgeo(d, e){
    var X = [e.p[0]], R = [], up = tsOf(d, e).dir === 'up', y = e.p[1], n = e.rows.length;
    e.cols.forEach(function(w){ X.push(X[X.length - 1] + w); });
    for (var k = 0; k < n; k++){ var i = up ? n - 1 - k : k; R[i] = [y, y - e.rows[i]]; y -= e.rows[i]; }
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
  function wrapLines(s, w, h){ var per = Math.max(2, Math.floor(w / (h * .62))), out = []; String(s).split('\n').forEach(function(par){ var cur = ''; par.split(' ').forEach(function(x){ if (cur && (cur + ' ' + x).length > per){ out.push(cur); cur = x; } else cur = cur ? cur + ' ' + x : x; }); out.push(cur); }); return out; }
  function hasField(e, r, c){ return (e.fld || []).some(function(f){ return f.cell && f.cell[0] === r && f.cell[1] === c; }); }
  // vẽ bảng thành nguyên tố (canvas.js gọi trong case 'table'): nền ô, viền theo kiểu ô, chữ canh theo kiểu ô, ô gộp, block trong ô, nền xám cho field và công thức
  TC.flatTable = function(x){
    var d = x.doc, e = x.e, P = x.P, t = x.t, ts = tsOf(d, e), g = tgeo(d, e), hk = hkOf(ts, e), kS = t ? t.k : 1, rS = t ? t.rot : 0, fills = [], lines = [], texts = [];
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
      var ls = cs.ang ? [txt.replace(/\n/g, ' ')] : wrapLines(txt, q.x1 - q.x0 - 2 * mh, h), lh = h * 1.5, n = ls.length, V0 = al[0], H0 = al[1];
      var xx = cs.ang ? (q.x0 + q.x1) / 2 : H0 === 'L' ? q.x0 + mh : H0 === 'R' ? q.x1 - mh : (q.x0 + q.x1) / 2, pal = cs.ang ? 'm' : H0 === 'L' ? 'l' : H0 === 'R' ? 'r' : 'c';
      var y0 = cs.ang ? (q.top + q.bot) / 2 : V0 === 'T' ? q.top - mv - h : V0 === 'B' ? q.bot + mv + (n - 1) * lh : (q.top + q.bot) / 2 + (h + (n - 1) * lh) / 2 - h;
      // nền xám của field và công thức (FIELDDISPLAY), không in
      if (TC.S.fieldBg !== false && (raw(e, r, c).trim()[0] === '=' || hasField(e, r, c))){ var wmax = Math.max.apply(null, ls.map(function(l){ return tw(d, l, h, style, cs.b); })), fx0 = pal === 'l' ? xx : pal === 'r' ? xx - wmax : xx - wmax / 2; fills.push(src({ k:'hatch', loops:[rect(fx0 - h * .15, y0 + h * 1.1, fx0 + wmax + h * .15, y0 - (n - 1) * lh - h * .3)], pat:'SOLID', sc:1, ang:0, fbg:1 }, 8)); }
      ls.forEach(function(l, li){ texts.push(src({ k:'text', p:P([xx, y0 - li * lh]), h:h * kS, s:l, rot:rS + (cs.ang || 0), al:pal, style:style, bold:cs.b, cell:[r, c] }, cs.tc)); });
    }
    fills.concat(lines, texts).forEach(function(pr){ var s0 = pr.src; x.o(pr); if (s0) pr.src = s0; });
  };

  /* ================= Kiểu bảng: hộp thoại (UI-D04) ================= */
  var SAMPLE = { 'TCVN-thong-ke-thep':[['BẢNG THỐNG KÊ CỐT THÉP'], ['Số hiệu', 'Đường kính', 'Dài 1 thanh', 'Số lượng', 'Khối lượng (kg)'], ['1', 'Φ12', '4250', '24', '90.6'], ['2', 'Φ16', '3800', '12', '72.0'], ['3', 'Φ8', '1200', '86', '40.7'], ['', '', '', 'Tổng', '=Sum(E3:E5)']], 'TCVN-dien-tich':[['BẢNG THỐNG KÊ DIỆN TÍCH'], ['STT', 'Tên phòng', 'Diện tích (m²)'], ['1', 'Phòng khách + ăn', '35.39'], ['2', 'Bếp', '17.31'], ['3', 'P. Ngủ 1', '15.07'], ['', 'Tổng cộng', '=Sum(C3:C5)']] };
  function sampleTable(d, name, ts){
    if (ts.start) return Object.assign(clone(ts.start), { t:'table', p:[0, 0], th:250, tstyle:name, _ts:ts, layer:'0' });
    var rowsT = SAMPLE[name] || [['Tiêu đề'], ['Đầu cột', 'Đầu cột', 'Đầu cột'], ['Dữ liệu', '120', '3.50'], ['Dữ liệu', '85', '12.00'], ['Dữ liệu', '240', '7.25']], nc = rowsT[1].length, hk = 100;
    var cols = rowsT[1].map(function(h, j){ return Math.max(1000, 420 + 170 * Math.max.apply(null, rowsT.slice(1).map(function(r){ return String(r[j] || '').length; }))); }), rs =rowsT.map(function(r, i){ return i === 0 ? 'title' : i === 1 ? 'header' : 'data'; });
    return { t:'table', p:[0, 0], cols:cols, rows:rs.map(function(k){ return rowH(ts.cells[k] || ts.cells.data, 1, hk); }), cells:rowsT.map(function(r){ return r.length < nc ? r.concat(new Array(nc - r.length).join('.').split('.')) : r; }), merge:[[0, 0, nc - 1]], rs:rs, head:2, th:250, tstyle:name, _ts:ts, layer:'0' };
  }
  function pvTable(c, d, e){
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
      pvTable($('#tbs-pv', dlg.el), d, sampleTable(d, sel, S[sel]));
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
        pvTable($d('#te-cpv'), d, cpv); pvTable($d('#te-pv'), d, sampleTable(d, name, C));
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
        if (t.id === 'te-pick') pickTable(dlg).then(function(e){ if (!e) return; C.start = { name:'Bảng ' + e.cols.length + ' cột × ' + e.rows.length + ' hàng', cols:e.cols.slice(), rows:e.rows.slice(), cells:clone(e.cells || []), merge:clone(e.merge || []), rs:e.rs ? e.rs.slice() : e.rows.map(function(x, i){ return rowKey(e, i); }), head:e.head || 0 }; pv(); });
      });
      $d('#te-ok').addEventListener('click', function(){ ED.run('TABLESTYLE', function(){ var done = ED.state(function(){ return d.tableStyles; }, function(s){ d.tableStyles = s; reprep(d); }); d.tableStyles[name] = C; done(); }); reprep(d); dlg.close(true); TC.toast('Đã lưu kiểu bảng <b>' + esc(name) + '</b>' + (tablesUsing(d, name) ? '; ' + tablesUsing(d, name) + ' bảng đã cập nhật.' : '.'), { log:false }); });
      all();
    });
  };
  // ẩn hộp thoại, chọn một bảng trên vùng vẽ (Esc để quay lại)
  function pickTable(dlg){
    dlg.hide(true); TC.cmdline.msg('Chọn bảng làm bảng khởi đầu. Esc để quay lại hộp thoại.');
    var esc1 = function(ev){ if (ev.key === 'Escape'){ ev.stopPropagation(); ev.preventDefault(); TC.cad.cancelPoint(); } };
    document.addEventListener('keydown', esc1, true);
    return TC.cad.getPoint({ pick:true }).then(function(r){
      document.removeEventListener('keydown', esc1, true); dlg.hide(false);
      if (!r) return null; var v = V(), id = v.hitTest(r.s[0], r.s[1]), e = id && D().byId[id];
      if (!e || e.t !== 'table'){ TC.toast('Đối tượng đã chọn không phải bảng. Bấm Chọn bảng… và chọn đúng một bảng.', { type:'warn', log:false }); return null; }
      return e;
    });
  }
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
        if (ok) pvTable($d('#ti-pv'), d, e);
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
    var ta = TC.el('<textarea class="tbce' + (e._paper ? ' paper' : '') + '" spellcheck="false" aria-label="Nội dung ô ' + colName(c) + (r + 1) + '"></textarea>');
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
    var d = D(), e = g.e, snap = ED.clean(e), p0 = wpt(m, e); if (!p0) return; TG.drag = g;
    function mv(ev){ var m2 = local(ev), p = wpt(m2, e); if (!p) return; var dx = p[0] - p0[0], dy = p[1] - p0[1];
      if (g.k === 'col') e.cols[g.i] = Math.max(e.th * 1.2, Math.round(snap.cols[g.i] + dx)); else if (g.k === 'row') e.rows[g.i] = Math.max(e.th * 1.4, Math.round(snap.rows[g.i] - dy)); else e.p = [snap.p[0] + dx, snap.p[1] + dy];
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
})();
