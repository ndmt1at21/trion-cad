/* TrionCAD prototype · Đợt 5 (sau v1.0): tab Tham số: ràng buộc hình học, ràng buộc kích thước, quản lý tham số. */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic, G = TC.geo, IMPL = TC.cmd.impl, ED = TC.ed, SC = TC.screens, H = SC.h;
  function A(){ return TC.app; } function V(){ return TC.app.view; } function D(){ return TC.app.doc(); }
  var OPEN = SC.open5 = SC.open5 || {}; // mã màn hình → hàm mở (danh mục gọi khi bấm); tiện ích mở ở SC.cat (D1, PRO, MOD, RUN, plan, pick, flash)
  var polar = ED.polar, sweep = ED.sweep;

  /* ================= danh mục loại ràng buộc ================= */
  // hình học: khóa → [tên, biểu tượng, lệnh, chữ tắt trong GEOMCONSTRAINT]
  var GT = { hor:['Nằm ngang', 'ghor', 'GCHORIZONTAL', 'H'], vert:['Thẳng đứng', 'gvert', 'GCVERTICAL', 'V'], perp:['Vuông góc', 'gperp', 'GCPERPENDICULAR', 'P'], par:['Song song', 'gpar', 'GCPARALLEL', 'PA'], tan:['Tiếp xúc', 'gtan', 'GCTANGENT', 'T'], smooth:['Trơn (G2)', 'gsmooth', 'GCSMOOTH', 'SM'],
    coin:['Trùng điểm', 'gcoin', 'GCCOINCIDENT', 'C'], conc:['Đồng tâm', 'gconc', 'GCCONCENTRIC', 'CON'], coll:['Thẳng hàng', 'gcoll', 'GCCOLLINEAR', 'COL'], sym:['Đối xứng', 'gsym', 'GCSYMMETRIC', 'S'], equal:['Bằng nhau', 'gequal', 'GCEQUAL', 'E'], fix:['Cố định', 'gfix', 'GCFIX', 'F'] };
  var GORD = ['coin', 'coll', 'conc', 'fix', 'par', 'perp', 'hor', 'vert', 'tan', 'smooth', 'sym', 'equal'];
  // kích thước: khóa → [tên, biểu tượng, lệnh, tiền tố tên tham số]
  var DT = { h:['Ngang', 'dchor', 'DCHORIZONTAL', 'd'], v:['Đứng', 'dcver', 'DCVERTICAL', 'd'], al:['Nghiêng', 'dcali', 'DCALIGNED', 'd'], rad:['Bán kính', 'dcrad', 'DCRADIUS', 'rad'], dia:['Đường kính', 'dcdia', 'DCDIAMETER', 'dia'], ang:['Góc', 'dcang', 'DCANGULAR', 'ang'] };
  var ENAME = { line:'Đoạn thẳng', pline:'Polyline', circle:'Đường tròn', arc:'Cung', spline:'Spline', ellipse:'Ellipse', point:'Điểm', text:'Chữ', mtext:'MText', insert:'Block' };
  var PT = { point:1, text:1, mtext:1, insert:1 };
  var TOL = 1e-6;

  /* ================= thiết lập (CONSTRAINTSETTINGS), theo người dùng ================= */
  var CS0 = { bar:{}, transp:50, barAfter:true, barSel:false, fmt:'ne', lock:true, dynSel:true, prio:['coin', 'coll', 'par', 'perp', 'tan', 'conc', 'hor', 'vert', 'equal'], off:{}, tanInt:true, perpInt:true, tolD:0.05, tolA:1 };
  var CS = Object.assign(JSON.parse(JSON.stringify(CS0)), TC.load('cset', {}));
  GORD.forEach(function(k){ if (CS.bar[k] == null) CS.bar[k] = true; });

  /* ================= dữ liệu trong bản vẽ: d.par ================= */
  // d.par = { list:[ràng buộc], users:[biến người dùng], n:bộ đếm, hide:{id đối tượng: ẩn thanh}, dyn:hiện ràng buộc động }
  // ràng buộc hình học { id, t, a, b, c?, ... }; kích thước { id, t:'dim', k, a, b?, name, expr, val, ref?, off, s? }
  // tham chiếu: điểm { id, k:'p1'|'p2'|'mid'|'c'|'a0'|'a1'|'v3'|'m3'|'p' }, đường { id, s? (đoạn của polyline) }
  function P(d){ d = d || D(); return d.par || (d.par = { list:[], users:[], n:0, hide:{}, dyn:true, v:0 }); }
  function touch(p){ p.v = (p.v || 0) + 1; }
  function parState(d){ return ED.state(function(){ return P(d); }, function(s){ d.par = s; if (d === D()){ V().draw(); A().refreshPalettes(); } }); }
  function refsOf(c){ return [c.a, c.b, c.c].filter(Boolean); }
  function entsOf(c){ var o = []; refsOf(c).forEach(function(r){ if (o.indexOf(r.id) < 0) o.push(r.id); }); return o; }
  function mkIndex(list){ var m = {}; list.forEach(function(c){ entsOf(c).forEach(function(id){ (m[id] = m[id] || []).push(c); }); }); return m; }
  var IX = { p:null };
  function index(d){ var p = P(d); if (IX.p !== p || IX.v !== p.v){ IX = { p:p, v:p.v, m:mkIndex(p.list) }; } return IX.m; }
  function live(d, c){ return entsOf(c).every(function(id){ var e = d.byId[id]; return e && okType(e) && !e._paper; }); }
  function isRef(c){ return c.t === 'dim' && !!c.ref; }
  function okType(e){ return !!e && (/^(line|circle|arc|pline|spline|ellipse)$/.test(e.t) || !!PT[e.t]); }
  function canPar(d){ return d && !d.bedit && !d.refedit && d.space === 'model'; }
  function byCid(d, id){ return P(d).list.filter(function(c){ return c.id === id; })[0]; }

  /* ================= hình học ================= */
  function sub(a, b){ return [a[0] - b[0], a[1] - b[1]]; }
  function dot(a, b){ return a[0] * b[0] + a[1] * b[1]; }
  function cross(a, b){ return a[0] * b[1] - a[1] * b[0]; }
  function len(v){ return Math.hypot(v[0], v[1]); }
  function unit(v){ var L = Math.hypot(v[0], v[1]) || 1e-12; return [v[0] / L, v[1] / L]; }
  function mid(a, b){ return [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2]; }
  function rad(a){ return a * Math.PI / 180; }
  function numStr(v){ var s = String(Math.round(v * 1e4) / 1e4); return s === '-0' ? '0' : s; }
  function fmtV(v, ang){ return isFinite(v) ? numStr(Math.round(v * 100) / 100) + (ang ? '°' : '') : '—'; }
  // tham số hình học của đối tượng (vector) và ghi ngược
  function gp(e){ switch (e.t){ case 'line': return [e.p1[0], e.p1[1], e.p2[0], e.p2[1]]; case 'circle': return [e.c[0], e.c[1], e.r]; case 'arc': return [e.c[0], e.c[1], e.r, e.a0, e.a1]; case 'pline': case 'spline': return [].concat.apply([], e.pts); case 'ellipse': return [e.c[0], e.c[1]]; default: return PT[e.t] ? [e.p[0], e.p[1]] : []; } }
  function spar(e, v){ switch (e.t){ case 'line': e.p1 = [v[0], v[1]]; e.p2 = [v[2], v[3]]; break; case 'circle': e.c = [v[0], v[1]]; e.r = v[2]; break; case 'arc': e.c = [v[0], v[1]]; e.r = v[2]; e.a0 = v[3]; e.a1 = v[4]; break; case 'pline': case 'spline': e.pts = e.pts.map(function(_, i){ return [v[2 * i], v[2 * i + 1]]; }); break; case 'ellipse': e.c = [v[0], v[1]]; break; default: if (PT[e.t]) e.p = [v[0], v[1]]; } return e; }
  // điểm ràng buộc của đối tượng: điểm cuối, trung điểm, tâm, điểm chèn
  function cpts(e){
    switch (e.t){
      case 'line': return [{ k:'p1', p:e.p1 }, { k:'p2', p:e.p2 }, { k:'mid', p:mid(e.p1, e.p2) }];
      case 'circle': case 'ellipse': return [{ k:'c', p:e.c }];
      case 'arc': return [{ k:'c', p:e.c }, { k:'a0', p:polar(e.c, e.r, e.a0) }, { k:'a1', p:polar(e.c, e.r, e.a1) }, { k:'mid', p:polar(e.c, e.r, e.a0 + sweep(e.a0, e.a1) / 2) }];
      case 'pline': { var o = [], n = e.pts.length, m = e.closed ? n : n - 1; e.pts.forEach(function(p, i){ o.push({ k:'v' + i, p:p }); }); for (var i = 0; i < m; i++) if (!(e.bulges || [])[i]) o.push({ k:'m' + i, p:mid(e.pts[i], e.pts[(i + 1) % n]) }); return o; }
      case 'spline': return [{ k:'v0', p:e.pts[0] }, { k:'v' + (e.pts.length - 1), p:e.pts[e.pts.length - 1] }];
      default: return PT[e.t] ? [{ k:'p', p:e.p }] : [];
    }
  }
  function ptOf(d, r){ var e = d.byId[r.id], q = e && cpts(e).filter(function(x){ return x.k === r.k; })[0]; return q ? q.p : null; }
  function segOf(d, r){ var e = d.byId[r.id]; if (e.t === 'line') return [e.p1, e.p2]; var n = e.pts.length, i = r.s || 0; return [e.pts[i], e.pts[(i + 1) % n]]; }
  function segs(e){ if (e.t === 'line') return [{ ref:{ id:e.id }, a:e.p1, b:e.p2 }]; if (e.t !== 'pline') return []; var n = e.pts.length, m = e.closed ? n : n - 1, o = []; for (var i = 0; i < m; i++) if (!(e.bulges || [])[i]) o.push({ ref:{ id:e.id, s:i }, a:e.pts[i], b:e.pts[(i + 1) % n] }); return o; }
  function nearSeg(e, p){ var n = e.pts.length, m = e.closed ? n : n - 1, best = 0, bd = Infinity; for (var i = 0; i < m; i++){ var q = G.segDist(p, e.pts[i], e.pts[(i + 1) % n]); if (q < bd){ bd = q; best = i; } } return best; }
  // loại hình của một tham chiếu: điểm, đường thẳng, đường tròn
  function rk(d, r){ var e = d.byId[r.id]; if (!e) return null; if (r.k) return 'pt'; if (e.t === 'line' || e.t === 'pline') return 'ln'; if (e.t === 'circle' || e.t === 'arc') return 'ci'; if (e.t === 'spline') return 'sp'; return 'ob'; }
  function refName(d, r){ var e = d.byId[r.id], n = e ? ENAME[e.t] || e.t : '?'; if (e && e.t === 'pline' && r.s != null) n += ' đoạn ' + (r.s + 1); if (e && !r.k && (e.t === 'line' || (e.t === 'pline' && r.s != null))){ var sg = segOf(d, r); n += ' ' + fmtV(G.dist(sg[0], sg[1])); } else if (e && !r.k && (e.t === 'circle' || e.t === 'arc')) n += ' R' + fmtV(e.r); if (r.k){ n += ' · ' + ({ p1:'điểm đầu', p2:'điểm cuối', mid:'trung điểm', c:'tâm', a0:'điểm đầu', a1:'điểm cuối', p:'điểm chèn' }[r.k] || (r.k[0] === 'v' ? 'đỉnh ' + (Number(r.k.slice(1)) + 1) : 'trung điểm đoạn ' + (Number(r.k.slice(1)) + 1))); } return n; }
  function consName(d, c){ return c.t === 'dim' ? c.name + ' · ' + DT[c.k][0] : GT[c.t][0]; }
  function consDesc(d, c){ return refsOf(c).map(function(r){ return refName(d, r); }).join(' ↔ '); }

  /* ================= biểu thức tham số ================= */
  // hàm lượng giác nhận và trả góc theo độ (giống cách nhập góc trong bản vẽ); d2r, r2d để đổi đơn vị
  var FN = { sin:function(a){ return Math.sin(rad(a)); }, cos:function(a){ return Math.cos(rad(a)); }, tan:function(a){ return Math.tan(rad(a)); }, asin:function(x){ return Math.asin(x) * 180 / Math.PI; }, acos:function(x){ return Math.acos(x) * 180 / Math.PI; }, atan:function(x){ return Math.atan(x) * 180 / Math.PI; },
    sqrt:Math.sqrt, abs:Math.abs, round:Math.round, floor:Math.floor, ceil:Math.ceil, min:Math.min, max:Math.max, pow:Math.pow, exp:Math.exp, ln:Math.log, log:Math.log10, d2r:rad, r2d:function(a){ return a * 180 / Math.PI; } };
  var CONST = { pi:Math.PI, e:Math.E };
  function parse(src){
    src = String(src); var i = 0;
    function ws(){ while (i < src.length && /\s/.test(src[i])) i++; }
    function err(m){ throw { pe:m }; }
    function expr(){ var l = term(); ws(); while (src[i] === '+' || src[i] === '-'){ var o = src[i++]; l = { o:o, l:l, r:term() }; ws(); } return l; }
    function term(){ var l = unary(); ws(); while (src[i] === '*' || src[i] === '/' || src[i] === '%'){ var o = src[i++]; l = { o:o, l:l, r:unary() }; ws(); } return l; }
    function unary(){ ws(); if (src[i] === '-' || src[i] === '+'){ var o = src[i++], x = unary(); return o === '-' ? { u:1, x:x } : x; } return power(); }
    function power(){ var b = prim(); ws(); if (src[i] === '^'){ i++; return { o:'^', l:b, r:unary() }; } return b; }
    function prim(){
      ws(); var m, rest = src.slice(i);
      if ((m = /^(\d+\.?\d*|\.\d+)(e[+-]?\d+)?/i.exec(rest))){ i += m[0].length; return { n:Number(m[0]) }; }
      if ((m = /^[A-Za-z_][A-Za-z0-9_]*/.exec(rest))){
        i += m[0].length; var nm = m[0], lc = nm.toLowerCase(); ws();
        if (src[i] === '('){ if (!FN[lc]) err('Không có hàm “' + nm + '”'); i++; var args = []; ws(); if (src[i] !== ')'){ args.push(expr()); ws(); while (src[i] === ','){ i++; args.push(expr()); ws(); } } if (src[i] !== ')') err('Thiếu dấu “)” sau đối số của ' + nm); i++; return { f:lc, a:args }; }
        if (CONST[lc] != null) return { n:CONST[lc] };
        return { v:nm };
      }
      if (src[i] === '('){ i++; var x = expr(); ws(); if (src[i] !== ')') err('Thiếu dấu “)”'); i++; return x; }
      if (i >= src.length) err('Biểu thức chưa hết, thiếu số hoặc tên tham số ở cuối');
      err('Ký tự “' + src[i] + '” ở vị trí ' + (i + 1) + ' không dùng được');
    }
    try {
      if (/\d,\d/.test(src)) err('Dấu thập phân là dấu chấm: gõ 12.5, không gõ 12,5');
      ws(); if (i >= src.length) err('Nhập một giá trị hoặc biểu thức');
      var ast = expr(); ws(); if (i < src.length) err(src[i] === ')' ? 'Thừa dấu “)” ở vị trí ' + (i + 1) : 'Ký tự “' + src[i] + '” ở vị trí ' + (i + 1) + ' không dùng được');
      return { ast:ast };
    } catch (x){ if (x && x.pe) return { err:x.pe }; throw x; }
  }
  function run(a, look){
    if (a.n != null) return a.n; if (a.v) return look(a.v); if (a.u) return -run(a.x, look);
    if (a.f) return FN[a.f].apply(null, a.a.map(function(z){ return run(z, look); }));
    var l = run(a.l, look), r = run(a.r, look);
    switch (a.o){ case '+': return l + r; case '-': return l - r; case '*': return l * r; case '/': if (r === 0) throw { pe:'Chia cho 0' }; return l / r; case '%': return l % r; default: return Math.pow(l, r); }
  }
  var IDRE = /[A-Za-z_][A-Za-z0-9_]*/g;
  // mọi tham số của bản vẽ: kích thước chủ động, kích thước tham chiếu, biến người dùng
  function items(p){ return p.list.filter(function(c){ return c.t === 'dim'; }).map(function(c){ return { key:c.id, name:c.name, expr:c.ref ? '' : c.expr, o:c, dim:true, ref:!!c.ref }; }).concat(p.users.map(function(u){ return { key:u.id, name:u.name, expr:u.expr, o:u, dim:false }; })); }
  function evalAll(d, p){
    var its = items(p), by = {}, v = {}, err = {}, st = {};
    its.forEach(function(it){ by[it.name.toLowerCase()] = it; });
    function get(it, path){
      if (v[it.key] != null) return v[it.key];
      if (err[it.key]) throw { pe:err[it.key] };
      if (st[it.key]){ var cyc = path.slice(path.indexOf(it.name)).concat(it.name); throw { pe:'Tham chiếu vòng: ' + cyc.join(' → '), cyc:1 }; }
      st[it.key] = 1;
      try {
        var val;
        if (it.ref) val = measure(d, it.o);
        else { var r = parse(it.expr); if (r.err) throw { pe:r.err }; val = run(r.ast, function(nm){ var t = by[nm.toLowerCase()]; if (!t) throw { pe:'Không có tham số “' + nm + '”' }; try { return get(t, path.concat(it.name)); } catch (z){ throw z.cyc ? z : { pe:'Phụ thuộc tham số “' + t.name + '” đang lỗi' }; } }); }
        if (!isFinite(val)) throw { pe:'Kết quả không xác định (ngoài miền của hàm)' };
        v[it.key] = val; return val;
      } catch (x){ if (!err[it.key]) err[it.key] = x && x.pe || String(x); throw x; } finally { st[it.key] = 0; }
    }
    its.forEach(function(it){ try { get(it, []); } catch (x){ if (!err[it.key]) err[it.key] = x && x.pe || 'Lỗi'; } });
    return { v:v, err:err, items:its };
  }
  function nameErr(p, name, key){
    if (!name) return 'Nhập tên tham số.';
    if (!/^[A-Za-z_][A-Za-z0-9_]*$/.test(name)) return 'Tên bắt đầu bằng chữ cái, chỉ gồm chữ không dấu, chữ số và “_”.';
    if (FN[name.toLowerCase()] || CONST[name.toLowerCase()] != null) return '“' + name + '” là tên hàm hoặc hằng số. Chọn tên khác.';
    var dup = items(p).filter(function(it){ return it.key !== key && it.name.toLowerCase() === name.toLowerCase(); })[0];
    return dup ? 'Đã có tham số “' + dup.name + '”. Chọn tên khác.' : null;
  }
  function renameRefs(p, from, to){ items(p).forEach(function(it){ if (!it.ref) it.o.expr = it.o.expr.replace(IDRE, function(m){ return m.toLowerCase() === from.toLowerCase() ? to : m; }); }); }
  // tham số bị xóa: biểu thức đang dùng tên đó nhận giá trị hiện tại
  function inlineRefs(p, vals){ items(p).forEach(function(it){ if (!it.ref) it.o.expr = it.o.expr.replace(IDRE, function(m){ var k = m.toLowerCase(); return vals[k] != null ? numStr(vals[k]) : m; }); }); }
  function usersOf(p, name){ var lc = name.toLowerCase(); return items(p).filter(function(it){ return !it.ref && (it.o.expr.match(IDRE) || []).some(function(m){ return m.toLowerCase() === lc; }); }); }
  function nextName(p, pre){ var mx = 0, re = new RegExp('^' + pre + '(\\d+)$', 'i'); items(p).forEach(function(it){ var m = re.exec(it.name); if (m) mx = Math.max(mx, Number(m[1])); }); return pre + (mx + 1); }

  /* ================= bộ giải ràng buộc ================= */
  // biến: tham số hình học của các đối tượng có ràng buộc; mỗi ràng buộc là các phương trình r(x) = 0 tính bằng mm.
  // Newton bước nhỏ nhất (giữ hình gần nhất), đạo hàm số; kéo grip, sửa giá trị là hàng "đích" thêm vào hệ.
  // ponytail: Jacobian dày và đạo hàm số, đủ cho vài trăm biến; chi tiết lớn hơn cần Jacobian thưa giải tích
  function ptH(S, r){
    var e = S.d.byId[r.id], o = S.off[r.id], k = r.k, f2 = function(i){ return { deps:[o + i, o + i + 1], f:function(x){ return [x[o + i], x[o + i + 1]]; } }; };
    if (e.t === 'line'){ if (k === 'p1') return f2(0); if (k === 'p2') return f2(2); return { deps:[o, o + 1, o + 2, o + 3], f:function(x){ return [(x[o] + x[o + 2]) / 2, (x[o + 1] + x[o + 3]) / 2]; } }; }
    if (e.t === 'arc'){ if (k === 'c') return f2(0); var fa = k === 'a0' ? function(x){ return x[o + 3]; } : k === 'a1' ? function(x){ return x[o + 4]; } : function(x){ return x[o + 3] + sweep(x[o + 3], x[o + 4]) / 2; }; return { deps:[o, o + 1, o + 2, o + 3, o + 4], f:function(x){ var a = rad(fa(x)); return [x[o] + x[o + 2] * Math.cos(a), x[o + 1] + x[o + 2] * Math.sin(a)]; } }; }
    if (e.t === 'pline' || e.t === 'spline'){ var i = Number(k.slice(1)); if (k[0] === 'v') return f2(2 * i); var j = (i + 1) % e.pts.length; return { deps:[o + 2 * i, o + 2 * i + 1, o + 2 * j, o + 2 * j + 1], f:function(x){ return [(x[o + 2 * i] + x[o + 2 * j]) / 2, (x[o + 2 * i + 1] + x[o + 2 * j + 1]) / 2]; } }; }
    return f2(0);
  }
  function lnH(S, r){
    var e = S.d.byId[r.id], o = S.off[r.id];
    if (e.t === 'line') return { deps:[o, o + 1, o + 2, o + 3], f:function(x){ return [[x[o], x[o + 1]], [x[o + 2], x[o + 3]]]; } };
    var i = r.s || 0, j = (i + 1) % e.pts.length; return { deps:[o + 2 * i, o + 2 * i + 1, o + 2 * j, o + 2 * j + 1], f:function(x){ return [[x[o + 2 * i], x[o + 2 * i + 1]], [x[o + 2 * j], x[o + 2 * j + 1]]]; } };
  }
  function ciH(S, r){ var o = S.off[r.id]; return { deps:[o, o + 1, o + 2], f:function(x){ return [[x[o], x[o + 1]], x[o + 2]]; } }; }
  // hướng tiếp tuyến đi vào đường cong tại điểm cuối (cho ràng buộc trơn)
  function tanH(S, r){
    var e = S.d.byId[r.id], o = S.off[r.id], k = r.k;
    if (e.t === 'line') return { deps:[o, o + 1, o + 2, o + 3], f:function(x){ return k === 'p1' ? [x[o + 2] - x[o], x[o + 3] - x[o + 1]] : [x[o] - x[o + 2], x[o + 1] - x[o + 3]]; } };
    if (e.t === 'arc') return { deps:[o + 3, o + 4], f:function(x){ var a = rad(k === 'a0' ? x[o + 3] : x[o + 4]); return k === 'a0' ? [-Math.sin(a), Math.cos(a)] : [Math.sin(a), -Math.cos(a)]; } };
    var n = e.pts.length, i = Number(k.slice(1)), j = i === 0 ? 1 : n - 2; return { deps:[o + 2 * i, o + 2 * i + 1, o + 2 * j, o + 2 * j + 1], f:function(x){ return [x[o + 2 * j] - x[o + 2 * i], x[o + 2 * j + 1] - x[o + 2 * i + 1]]; } };
  }
  // dịch một ràng buộc thành phương trình: { c, n, deps, f(x) → [r…], m(x) → giá trị đo (kích thước) }
  function comp(S, c){
    var d = S.d, A = c.a, B = c.b, ka = rk(d, A), kb = B ? rk(d, B) : null, f = null, m = null, n = 1;
    var hh = function(r, k){ return k === 'pt' ? ptH(S, r) : k === 'ln' ? lnH(S, r) : k === 'ci' ? ciH(S, r) : null; };
    var ha = hh(A, ka), hb = B ? hh(B, kb) : null, hc = c.c ? lnH(S, c.c) : null, P2 = function(x){ var p = ha.f(x), q = hb.f(x); return [p[0] - q[0], p[1] - q[1]]; };
    switch (c.t){
      case 'coin':
        if (ka === 'pt' && kb === 'pt'){ n = 2; f = P2; }
        else if (ka === 'pt' && kb === 'ln') f = function(x){ var p = ha.f(x), L = hb.f(x); return [cross(unit(sub(L[1], L[0])), sub(p, L[0]))]; };
        else if (ka === 'pt' && kb === 'ci') f = function(x){ var p = ha.f(x), C = hb.f(x); return [len(sub(p, C[0])) - C[1]]; };
        break;
      case 'conc': ha = ptH(S, { id:A.id, k:'c' }); hb = ptH(S, { id:B.id, k:'c' }); n = 2; f = P2; break;
      case 'coll': if (ka === 'ln' && kb === 'ln'){ n = 2; f = function(x){ var L = ha.f(x), M = hb.f(x), u = unit(sub(L[1], L[0])); return [cross(u, sub(M[0], L[0])), cross(u, sub(M[1], L[0]))]; }; } break;
      case 'par': case 'perp': if (ka === 'ln' && kb === 'ln') f = function(x){ var L = ha.f(x), M = hb.f(x), u = unit(sub(L[1], L[0])), w = sub(M[1], M[0]); return [c.t === 'par' ? cross(u, w) : dot(u, w)]; }; break;
      case 'hor': case 'vert': { var ax = c.t === 'hor' ? 1 : 0;
        if (ka === 'ln') f = function(x){ var L = ha.f(x); return [L[1][ax] - L[0][ax]]; };
        else if (ka === 'pt' && kb === 'pt') f = function(x){ return [hb.f(x)[ax] - ha.f(x)[ax]]; };
        break; }
      case 'tan':
        if (ka === 'ln' && kb === 'ci') f = function(x){ var L = ha.f(x), C = hb.f(x); return [cross(unit(sub(L[1], L[0])), sub(C[0], L[0])) - c.s * C[1]]; };
        else if (ka === 'ci' && kb === 'ci') f = function(x){ var C = ha.f(x), E = hb.f(x), dd = len(sub(C[0], E[0])); return [c.s > 0 ? dd - C[1] - E[1] : dd - c.s2 * (C[1] - E[1])]; };
        break;
      case 'equal':
        if (ka === 'ln' && kb === 'ln') f = function(x){ var L = ha.f(x), M = hb.f(x); return [len(sub(L[1], L[0])) - len(sub(M[1], M[0]))]; };
        else if (ka === 'ci' && kb === 'ci') f = function(x){ return [ha.f(x)[1] - hb.f(x)[1]]; };
        break;
      case 'fix':
        if (ka === 'pt'){ n = 2; f = function(x){ var p = ha.f(x); return [p[0] - c.v[0], p[1] - c.v[1]]; }; }
        else { var o = S.off[A.id], k = c.v.length, dp = []; for (var q = 0; q < k; q++) dp.push(o + q); n = k; ha = { deps:dp }; f = function(x){ var r = []; for (var i = 0; i < k; i++) r.push(x[o + i] - c.v[i]); return r; }; }
        break;
      case 'sym':
        if (!hc) break;
        if (ka === 'pt' && kb === 'pt'){ n = 2; f = function(x){ var p = ha.f(x), q = hb.f(x), L = hc.f(x), u = unit(sub(L[1], L[0])); return [cross(u, sub(mid(p, q), L[0])), dot(u, sub(q, p))]; }; }
        else if (ka === 'ln' && kb === 'ln') f = function(x){ var L = hc.f(x), u = unit(sub(L[1], L[0])), M1 = ha.f(x), M2 = hb.f(x), a = unit(sub(M1[1], M1[0])), k2 = 2 * dot(a, u); return [cross([k2 * u[0] - a[0], k2 * u[1] - a[1]], sub(M2[1], M2[0]))]; };
        else if (ka === 'ci' && kb === 'ci'){ n = 3; f = function(x){ var C = ha.f(x), E = hb.f(x), L = hc.f(x), u = unit(sub(L[1], L[0])); return [cross(u, sub(mid(C[0], E[0]), L[0])), dot(u, sub(E[0], C[0])), C[1] - E[1]]; }; }
        break;
      case 'smooth': { var ts = tanH(S, A), to = tanH(S, B); n = 3; f = function(x){ var p = ha.f(x), q = hb.f(x); return [p[0] - q[0], p[1] - q[1], cross(unit(to.f(x)), ts.f(x))]; }; hc = { deps:ts.deps.concat(to.deps) }; break; }
      case 'dim': {
        var kk = c.k, sc = 1;
        if (kk === 'h' || kk === 'v'){ var a2 = kk === 'h' ? 0 : 1; m = function(x){ return c.s * (hb.f(x)[a2] - ha.f(x)[a2]); }; }
        else if (kk === 'al') m = kb === 'ln' ? function(x){ var p = ha.f(x), L = hb.f(x); return c.s * cross(unit(sub(L[1], L[0])), sub(p, L[0])); } : function(x){ return len(sub(hb.f(x), ha.f(x))); };
        else if (kk === 'rad') m = function(x){ return ha.f(x)[1]; };
        else if (kk === 'dia') m = function(x){ return 2 * ha.f(x)[1]; };
        else if (kk === 'ang'){
          if (ka === 'ci'){ var oa = S.off[A.id]; ha = { deps:[oa + 3, oa + 4] }; m = function(x){ return sweep(x[oa + 3], x[oa + 4]); }; }
          else m = function(x){ var L = ha.f(x), M = hb.f(x), e1 = unit(sub(L[1], L[0])), e2 = unit(sub(M[1], M[0])); e1 = [e1[0] * c.s1, e1[1] * c.s1]; e2 = [e2[0] * c.s2, e2[1] * c.s2]; var th = Math.atan2(cross(e1, e2), dot(e1, e2)) * 180 / Math.PI; return c.cw ? -th : th; };
          sc = 1000 * Math.PI / 180;
        }
        if (m) f = function(x){ return [(m(x) - c.val) * sc]; };
        break; }
    }
    if (!f || !ha) return null;
    var deps = []; [ha, hb, hc].forEach(function(h){ if (h) h.deps.forEach(function(j){ if (deps.indexOf(j) < 0) deps.push(j); }); });
    return { c:c, n:n, deps:deps, f:f, m:m };
  }
  // hệ phương trình cho một nhóm ràng buộc; đối tượng trên layer khóa được giữ nguyên; đổi bán kính "đắt" hơn dời tâm (đường tròn ưu tiên di chuyển, giữ cỡ)
  function sys(d, list){
    var S = { d:d, ids:[], off:{}, x0:[], w:[] }, pins = [];
    list.forEach(function(c){ entsOf(c).forEach(function(id){ if (S.off[id] != null) return; var e = d.byId[id], v = gp(e); S.off[id] = S.x0.length; S.ids.push(id); v.forEach(function(q, i){ S.x0.push(q); S.w.push(e.t === 'arc' && i > 2 ? Math.max(1, Math.pow(e.r * Math.PI / 180, 2)) : (e.t === 'circle' || e.t === 'arc') && i === 2 ? 1e3 : 1); }); if ((d.layerMap[e.layer] || {}).lck) pins.push(id); }); });
    S.rows = list.map(function(c){ return comp(S, c); }).filter(Boolean);
    pins.forEach(function(id){ var o = S.off[id], v = gp(d.byId[id]); v.forEach(function(q, i){ S.rows.unshift({ c:null, n:1, deps:[o + i], f:function(x){ return [x[o + i] - q]; } }); }); });
    return S;
  }
  // các ràng buộc nối với các đối tượng ids (đi theo đồ thị ràng buộc)
  function compOf(d, ids, list){
    var ix = list ? mkIndex(list) : index(d), seen = {}, has = {}, out = [], q = ids.map(String);
    while (q.length){ var id = q.pop(); if (seen[id]) continue; seen[id] = 1; (ix[id] || []).forEach(function(c){ if (has[c.id] || isRef(c) || !live(d, c)) return; has[c.id] = 1; out.push(c); entsOf(c).forEach(function(j){ if (!seen[j]) q.push(String(j)); }); }); }
    return out;
  }
  function evalRows(rows, x){ var R = []; rows.forEach(function(r){ var v = r.f(x); for (var i = 0; i < v.length; i++) R.push(v[i]); }); return R; }
  function ss(R){ var s = 0; R.forEach(function(v){ s += v * v; }); return s; }
  function mx(R){ var s = 0; R.forEach(function(v){ s = Math.max(s, Math.abs(v)); }); return s; }
  // Jacobian dày (đạo hàm số trung tâm): một hàng cho mỗi phương trình
  function denseJ(rows, x, n){
    var J = [];
    rows.forEach(function(r){ var g = []; for (var i = 0; i < r.n; i++) g.push(new Float64Array(n)); r.deps.forEach(function(j){ var x0 = x[j], h = 1e-4; x[j] = x0 + h; var a = r.f(x); x[j] = x0 - h; var b = r.f(x); x[j] = x0; for (var i = 0; i < r.n; i++) g[i][j] = (a[i] - b[i]) / (2 * h); }); g.forEach(function(v){ v.row = r; J.push(v); }); });
    return J;
  }
  function gauss(M, b){
    var n = b.length, A = M.map(function(r, i){ var a = Array.prototype.slice.call(r); a.push(b[i]); return a; }), i, r, c;
    for (i = 0; i < n; i++){ var p = i; for (r = i + 1; r < n; r++) if (Math.abs(A[r][i]) > Math.abs(A[p][i])) p = r; var t = A[i]; A[i] = A[p]; A[p] = t; var v = A[i][i]; if (Math.abs(v) < 1e-300) continue; for (r = i + 1; r < n; r++){ var f = A[r][i] / v; if (!f) continue; for (c = i; c <= n; c++) A[r][c] -= f * A[i][c]; } }
    var x = new Array(n).fill(0); for (i = n - 1; i >= 0; i--){ var s = A[i][n]; for (c = i + 1; c < n; c++) s -= A[i][c] * x[c]; x[i] = Math.abs(A[i][i]) < 1e-300 ? 0 : s / A[i][i]; } return x;
  }
  // Newton bước nhỏ nhất theo trọng số W: Δ = −W⁻¹Jᵀ(JW⁻¹Jᵀ + μI)⁻¹ r, có tìm bước
  function newton(rows, x0, W, it){
    var x = x0.slice(), n = x.length, R = evalRows(rows, x), e = ss(R);
    for (var k = 0; k < (it || 30) && mx(R) > TOL; k++){
      var J = denseJ(rows, x, n), m = J.length, Am = [], dg = 0, i, j, q;
      for (i = 0; i < m; i++) Am.push(new Float64Array(m));
      for (i = 0; i < m; i++) for (j = i; j < m; j++){ var s = 0, a = J[i], b = J[j]; for (q = 0; q < n; q++) if (a[q] && b[q]) s += a[q] * b[q] / W[q]; Am[i][j] = Am[j][i] = s; if (i === j) dg = Math.max(dg, s); }
      for (i = 0; i < m; i++) Am[i][i] += 1e-10 * (1 + dg);
      var y = gauss(Am, R), dx = new Float64Array(n);
      for (i = 0; i < m; i++) for (q = 0; q < n; q++) if (J[i][q]) dx[q] -= J[i][q] * y[i] / W[q];
      for (var t = 1, ls = 0; ls < 10; ls++, t /= 2){ var xn = x.map(function(v, z){ return v + t * dx[z]; }), Rn = evalRows(rows, xn), en = ss(Rn); if (en < e || ls === 9){ x = xn; R = Rn; e = en; break; } }
    }
    return { x:x, ok:mx(R) <= TOL, err:mx(R) };
  }
  // không đạt được mọi đích: đi gần đích nhất mà vẫn giữ ràng buộc (tuyến tính hóa rồi chiếu lại về mặt ràng buộc)
  function towards(Gr, Dr, x, W){
    var best = x, be = ss(evalRows(Dr, x)), n = x.length;
    for (var k = 0; k < 5; k++){
      var Jg = denseJ(Gr, x, n), Jd = denseJ(Dr, x, n), Rd = evalRows(Dr, x), M = [], rhs = new Float64Array(n), i, j;
      for (i = 0; i < n; i++) M.push(new Float64Array(n));
      Jg.forEach(function(r){ for (var i = 0; i < n; i++) if (r[i]) for (var j = 0; j < n; j++) if (r[j]) M[i][j] += 1e4 * r[i] * r[j]; });
      Jd.forEach(function(r, z){ for (var i = 0; i < n; i++) if (r[i]){ rhs[i] -= r[i] * Rd[z]; for (var j = 0; j < n; j++) if (r[j]) M[i][j] += r[i] * r[j]; } });
      for (i = 0; i < n; i++) M[i][i] += 1e-4 * W[i];
      var dx = gauss(M, rhs), pr = newton(Gr, x.map(function(v, z){ return v + dx[z]; }), W);
      if (!pr.ok) break; var en = ss(evalRows(Dr, pr.x)); if (en < be - 1e-9){ best = pr.x; be = en; x = pr.x; } else break;
    }
    return best;
  }
  // giải: Dr = [{ j, v }] tham số cần đạt (kéo grip, neo đối tượng thứ nhất); trả { x, exact } hoặc { fail }
  function solve(S, Dr, xs){
    var x0 = xs || S.x0, D2 = (Dr || []).map(function(t){ return { n:1, deps:[t.j], f:function(x){ return [x[t.j] - t.v]; } }; });
    if (D2.length){ var a = newton(S.rows.concat(D2), x0, S.w); if (a.ok) return { x:a.x, exact:true }; }
    var p = newton(S.rows, x0, S.w); if (!p.ok) return { x:p.x, fail:true };
    return D2.length ? { x:towards(S.rows, D2, p.x, S.w), exact:false } : { x:p.x, exact:true };
  }
  // ràng buộc cuối của hệ có thừa không: các hàng Jacobian của nó nằm trong không gian sinh bởi các hàng cũ
  function redundant(S, x){
    var n = x.length, last = S.rows[S.rows.length - 1], B = [], bad = false, J = denseJ(S.rows, x.slice(), n);
    function nm(v){ var s = 0; for (var j = 0; j < n; j++) s += v[j] * v[j]; return Math.sqrt(s); }
    J.forEach(function(v){ var isNew = v.row === last, n0 = nm(v); if (n0 < 1e-12){ if (isNew) bad = true; return; } v = Float64Array.from(v);
      for (var t = 0; t < 2; t++) B.forEach(function(b){ var s = 0, j; for (j = 0; j < n; j++) s += v[j] * b[j]; for (j = 0; j < n; j++) v[j] -= s * b[j]; });
      var n1 = nm(v); if (n1 < (isNew ? 1e-6 : 1e-9) * n0){ if (isNew) bad = true; return; } for (var j = 0; j < n; j++) v[j] /= n1; B.push(v); });
    return bad;
  }
  // giữ nguyên đối tượng thứ nhất khi đổi giá trị: điểm, cả đoạn (góc), tâm (bán kính)
  function anchorOf(d, r){ var e = d.byId[r.id]; if (!e) return []; if (r.k) return [r]; if (e.t === 'circle' || e.t === 'arc') return [{ id:r.id, k:'c' }]; if (e.t === 'line') return [{ id:r.id, k:'p1' }, { id:r.id, k:'p2' }]; if (e.t === 'pline'){ var s = r.s || 0; return [{ id:r.id, k:'v' + s }, { id:r.id, k:'v' + (s + 1) % e.pts.length }]; } return []; }
  function anchorRows(S, refs){ var out = []; (refs || []).forEach(function(r){ if (!r || S.off[r.id] == null) return; var h = r.k ? ptH(S, r) : { deps:gp(S.d.byId[r.id]).map(function(_, i){ return S.off[r.id] + i; }) }; h.deps.forEach(function(j){ out.push({ j:j, v:S.x0[j] }); }); }); return out; }
  function drivenRows(S, tgt){ var out = []; Object.keys(tgt).forEach(function(id){ var o = S.off[id]; if (o == null) return; tgt[id].forEach(function(v, i){ if (Math.abs(v - S.x0[o + i]) > 1e-9) out.push({ j:o + i, v:v }); }); }); return out; }
  function changedEnts(S, x, tol){ var out = []; S.ids.forEach(function(id){ var e = S.d.byId[id], o = S.off[id], cur = gp(e), nv = cur.map(function(_, i){ return x[o + i]; }); if (nv.some(function(v, i){ return Math.abs(v - cur[i]) > (tol || 1e-9); })) out.push({ e:e, v:nv }); }); return out; }
  var inSolve = 0;
  function applySol(S, x){ var ch = changedEnts(S, x); inSolve++; try { ch.forEach(function(z){ ED.mod(z.e, function(e){ spar(e, z.v); }); }); } finally { inSolve--; } return ch.length; }
  function rowOf(d, c){ var S = sys(d, [c]); return { S:S, r:S.rows.filter(function(r){ return r.c === c; })[0] }; }
  function measure(d, c){ var q = rowOf(d, c); return q.r && q.r.m ? q.r.m(q.S.x0) : NaN; }
  function residual(d, c){ var q = rowOf(d, c); return q.r ? mx(q.r.f(q.S.x0)) : Infinity; }

  /* ================= gắn ràng buộc: kiểm tra trùng, thừa, xung đột ================= */
  function sameRef(a, b){ return !!a && !!b && a.id === b.id && (a.k || '') === (b.k || '') && (a.s || 0) === (b.s || 0); }
  function dupOf(d, c){
    return P(d).list.filter(function(o){
      if (o.t !== c.t || (c.t === 'dim' && o.k !== c.k)) return false;
      var sw = sameRef(o.a, c.b) && sameRef(o.b, c.a);
      return ((sameRef(o.a, c.a) && (!c.b || sameRef(o.b, c.b))) || sw) && (!c.c || sameRef(o.c, c.c));
    })[0] || null;
  }
  function commitAdd(d, c, S, x){
    var p = P(d), done = parState(d); c.id = 'c' + (++p.n); p.list.push(c);
    if (CS.barAfter && c.t !== 'dim') entsOf(c).forEach(function(id){ delete p.hide[id]; });
    if (c.t === 'dim') p.dyn = true;
    touch(p); done(); if (S && x) applySol(S, x);
  }
  // o.anchor: tham chiếu giữ nguyên khi giải (đối tượng thứ nhất); o.silent: không tìm ràng buộc gây xung đột
  function tryAdd(d, c, o){
    o = o || {};
    if (dupOf(d, c)) return { dup:true };
    var list = compOf(d, entsOf(c)), S = sys(d, list.concat([c])), last = S.rows[S.rows.length - 1];
    if (!last || last.c !== c) return { bad:true };
    var r = solve(S, anchorRows(S, o.anchor));
    if (r.fail || redundant(S, r.x)) return { conflict:o.silent ? [] : culprits(d, list, c), fail:!!r.fail };
    commitAdd(d, c, S, r.x);
    return { ok:true };
  }
  // ràng buộc nào gây xung đột: bỏ riêng từng cái mà hệ mới giải được và không thừa
  function culprits(d, list, c){
    var out = [];
    list.forEach(function(o){ var rest = list.filter(function(x){ return x !== o; }), S = sys(d, rest.concat([c])); if (S.rows[S.rows.length - 1].c !== c) return; var r = solve(S, []); if (!r.fail && !redundant(S, r.x)) out.push(o); });
    if (!out.length){ var ids = entsOf(c); out = list.filter(function(o){ return entsOf(o).some(function(id){ return ids.indexOf(id) >= 0; }); }); }
    var rank = function(o){ return o.t === 'dim' ? 0 : o.t === 'coin' ? 2 : 1; }; // kích thước, ràng buộc hình dạng trước; trùng điểm sau cùng
    return out.sort(function(a, b){ return rank(a) - rank(b); });
  }
  function delCons(d, cs, label){
    if (!cs.length) return 0; var p = P(d), gone = {}, vals = {}, ev = evalAll(d, p);
    cs.forEach(function(c){ gone[c.id] = 1; if (c.t === 'dim') vals[c.name.toLowerCase()] = ev.v[c.id]; });
    ED.run(label || 'Xóa ràng buộc', function(){ var done = parState(d); p.list = p.list.filter(function(c){ return !gone[c.id]; }); if (Object.keys(vals).length) inlineRefs(p, vals); touch(p); done(); });
    return cs.length;
  }
  // đổi tên, biểu thức một tham số; kiểm tra tên, cú pháp, tham chiếu vòng; dựng lại hình theo giá trị mới (một bước hoàn tác)
  function setParam(d, key, name, expr, label){
    var p = P(d), cand = JSON.parse(JSON.stringify(p)), it = items(cand).filter(function(x){ return x.key === key; })[0];
    if (!it) return { err:'Không tìm thấy tham số.' };
    name = String(name).trim(); expr = String(expr).trim();
    var bad = nameErr(cand, name, key); if (bad) return { err:bad };
    if (name !== it.name) renameRefs(cand, it.name, name);
    it.o.name = name; if (!it.ref) it.o.expr = expr;
    var ev = evalAll(d, cand); if (ev.err[key]) return { err:ev.err[key] };
    var other = Object.keys(ev.err)[0]; if (other) return { err:'Làm hỏng tham số khác: ' + ev.err[other] };
    var changed = [], edited = null;
    cand.list.forEach(function(c){ if (c.t !== 'dim' || c.ref) return; var v = ev.v[c.id];
      if ((c.k === 'ang' && (v <= 0 || v >= 180)) || (c.k !== 'ang' && v <= 0)) bad = bad || (c.name + ' = ' + fmtV(v) + ': ' + (c.k === 'ang' ? 'góc phải lớn hơn 0° và nhỏ hơn 180°.' : 'giá trị phải lớn hơn 0.'));
      if (Math.abs(v - c.val) > 1e-9){ changed.push(c); if (c.id === key) edited = c; } c.val = v; });
    if (bad) return { err:bad };
    var S = null, r = null;
    if (changed.length){
      var ids = []; changed.forEach(function(c){ entsOf(c).forEach(function(id){ if (ids.indexOf(id) < 0) ids.push(id); }); });
      S = sys(d, compOf(d, ids, cand.list)); r = solve(S, anchorRows(S, edited ? anchorOf(d, edited.a) : []));
      if (r.fail) return { err:'Không dựng được hình với ' + changed.map(function(c){ return c.name + ' = ' + fmtV(c.val, c.k === 'ang'); }).join(', ') + ': mâu thuẫn với các ràng buộc khác. Thử giá trị khác hoặc bỏ bớt ràng buộc.' };
    }
    ED.run(label || 'Sửa tham số', function(){ var done = parState(d); touch(cand); d.par = cand; done(); if (S) applySol(S, r.x); });
    return { ok:true, n:changed.length };
  }

  /* ================= móc vào bản biên tập: giữ ràng buộc khi sửa, xóa ràng buộc theo đối tượng, suy luận ================= */
  var SOLVE = { GRIPEDIT:1, MOVE:1, STRETCH:1, ROTATE:1, SCALE:1, ALIGN:1 }, INF = { LINE:1, PLINE:1, RECTANG:1, POLYGON:1, CIRCLE:1, ARC:1 };
  var depth = 0, TOUCH = {}, LIVE = { sig:null }, INFN = {};
  var begin0 = ED.begin, commit0 = ED.commit, mod0 = ED.mod, add0 = ED.add, del0 = ED.del, temp0 = ED.temp, xprev0 = ED.xprev;
  function constrained(d, id){ return !!(d && d.par && d.par.list.length && index(d)[id]); }
  ED.begin = function(){ depth++; return begin0.apply(ED, arguments); };
  ED.commit = function(){ if (depth <= 1 && !inSolve){ try { post(); } catch (err){ console.error(err); } } depth = Math.max(0, depth - 1); return commit0.apply(ED, arguments); };
  ED.mod = function(e){ var d = D(); if (!inSolve && e && e.id != null && TOUCH[e.id] == null && okType(e) && constrained(d, e.id)) TOUCH[e.id] = gp(e); return mod0.apply(ED, arguments); };
  ED.del = function(e){ var r = del0.apply(ED, arguments), d = D(); if (!inSolve && e && constrained(d, e.id)) dropEnts(d, [e.id]); return r; };
  ED.add = function(e){ var r = add0.apply(ED, arguments), d = D(); if (TC.S.toggles.infer && !inSolve && INF[TC.cmdline.current()] && canPar(d) && !r._paper && TC.can('PRO')){ try { infer(d, r); } catch (err){ console.error(err); } } return r; };
  function dropEnts(d, ids){ var cs = P(d).list.filter(function(c){ return entsOf(c).some(function(id){ return ids.indexOf(id) >= 0; }); }); if (cs.length) delCons(d, cs); }
  // cuối một bước hoàn tác: đối tượng có ràng buộc vừa bị sửa → giải lại phần nối với nó (lệnh sửa hình) hoặc bỏ ràng buộc không còn đúng (lệnh đổi cấu trúc: TRIM, FILLET…)
  function post(){
    var d = D(), ids = Object.keys(TOUCH), before = TOUCH; TOUCH = {}; if (!ids.length || !d || !d.par) return;
    var tgt = {}, n = 0, topo = false;
    ids.forEach(function(id){ var e = d.byId[id]; if (!e || !okType(e)) return; var a = gp(e), b = before[id]; if (a.length !== b.length){ topo = true; tgt[id] = a; n++; return; } if (a.some(function(v, i){ return Math.abs(v - b[i]) > 1e-9; })){ tgt[id] = a; n++; } });
    if (!n) return;
    var cmd = TC.cmdline.current();
    if (topo || (cmd && !SOLVE[cmd])) return dropBroken(d, Object.keys(tgt), cmd);
    var list = compOf(d, Object.keys(tgt)); if (!list.length) return;
    var S = sys(d, list); Object.keys(tgt).forEach(function(id){ var o = S.off[id]; if (o != null) before[id].forEach(function(v, i){ S.x0[o + i] = v; }); });
    var sig = Object.keys(tgt).sort().join(',') + '|' + S.rows.length, r = solve(S, drivenRows(S, tgt), LIVE.sig === sig && LIVE.x && LIVE.x.length === S.x0.length ? LIVE.x : null);
    if (r.fail) return dropBroken(d, Object.keys(tgt), cmd);
    applySol(S, r.x);
    if (!r.exact) TC.cmdline.msg('Ràng buộc giữ hình: chỉ phần còn tự do thay đổi theo thao tác. Bỏ bớt ràng buộc (DELCONSTRAINT) nếu cần sửa tự do.');
  }
  function dropBroken(d, ids, cmd){
    var bad = compOf(d, ids).filter(function(c){ return residual(d, c) > 1e-4; }); if (!bad.length) return;
    delCons(d, bad, 'Bỏ ràng buộc');
    TC.cmdline.msg('Đã bỏ ' + bad.length + ' ràng buộc không còn đúng sau ' + (cmd ? 'lệnh <b>' + esc(cmd) + '</b>' : 'thao tác') + ': ' + bad.map(function(c){ return esc(consName(d, c)); }).slice(0, 4).join(', ') + (bad.length > 4 ? '…' : '') + '.');
  }
  // xem trước khi kéo grip, kéo giãn, di chuyển: giải theo vị trí con trỏ, khởi đầu từ nghiệm khung trước (liên tục, không lật hình)
  function liveSolve(d, tgt){
    var ids = Object.keys(tgt), list = compOf(d, ids); if (!list.length) return null;
    var S = sys(d, list), Dr = drivenRows(S, tgt); if (!Dr.length) return null;
    var sig = ids.slice().sort().join(',') + '|' + S.rows.length, r = solve(S, Dr, LIVE.sig === sig && LIVE.x && LIVE.x.length === S.x0.length ? LIVE.x : null);
    if (r.fail) return null; if (!r.exact && !LIVE.warned && LIVE.sig === sig){ LIVE.warned = true; var m = V().mouse; if (m) TC.ctip($('#cv'), m[0], m[1], 'Đã ràng buộc: chỉ kéo được theo phần còn tự do', true); }
    LIVE = { sig:sig, x:r.x, warned:LIVE.sig === sig && LIVE.warned }; return { S:S, x:r.x };
  }
  function ghosts(sol){ return changedEnts(sol.S, sol.x, 1e-7).map(function(z){ return TC.prepEnt(sol.S.d, spar(ED.clean(z.e), z.v), true); }); }
  ED.temp = function(list){
    var r = temp0.apply(ED, arguments), d = D(), cmd = TC.cmdline.current();
    if (inSolve || !d || !d.par || !d.par.list.length || (cmd !== 'GRIPEDIT' && cmd !== 'STRETCH') || !canPar(d)) return r;
    var tgt = {}, n = 0; list.forEach(function(e){ var real = e && e.id != null && d.byId[e.id]; if (real && real !== e && okType(real) && constrained(d, e.id) && gp(e).length === gp(real).length){ tgt[e.id] = gp(e); n++; } });
    if (!n) return r;
    var sol = liveSolve(d, tgt); if (!sol) return r;
    var gh = ghosts(sol); r.list = r.list.filter(function(e){ return !tgt[e.id]; }).concat(gh); return r;
  };
  ED.xprev = function(ids, t){
    var r = xprev0.apply(ED, arguments), d = D(), cmd = TC.cmdline.current();
    if (inSolve || !d || !d.par || !d.par.list.length || !/^(MOVE|ROTATE|SCALE)$/.test(cmd || '') || ids.length > 400 || !canPar(d)) return r;
    var tgt = {}, n = 0; ids.forEach(function(id){ var e = d.byId[id]; if (e && okType(e) && constrained(d, id)){ tgt[id] = gp(ED.xform(ED.clean(e), t)); n++; } });
    if (!n) return r;
    var sol = liveSolve(d, tgt); if (!sol) return r;
    var gh = ghosts(sol).filter(function(g){ return ids.indexOf(g.id) < 0; }); if (gh.length) V().temp.push(temp0(gh)); return r;
  };
  TC.on('cmdend', function(name){
    LIVE = { sig:null }; PK.mark = null; PV = null;
    var k = Object.keys(INFN); if (k.length){ var n = k.reduce(function(s, t){ return s + INFN[t]; }, 0); TC.cmdline.msg('Suy luận ràng buộc: đã gắn ' + n + ' ràng buộc (' + k.map(function(t){ return GT[t][0] + ' ' + INFN[t]; }).join(', ') + ').'); INFN = {}; }
    if (name === 'GRIPEDIT' || name === 'MOVE' || name === 'STRETCH') V().drawOver();
  });

  /* ================= suy luận ràng buộc khi vẽ (nút trên thanh trạng thái, Ctrl+Shift+I) ================= */
  function nearPts(d, p, tol, excl){
    var out = []; d.ents.forEach(function(e){ if (e.id === excl || !okType(e) || !TC.entVisible(d, e)) return; var b = e._bb; if (!b || p[0] < b[0] - tol || p[0] > b[2] + tol || p[1] < b[1] - tol || p[1] > b[3] + tol) return; cpts(e).forEach(function(q){ if (G.dist(q.p, p) <= tol) out.push({ e:e, k:q.k }); }); });
    return out.sort(function(a, b){ return (a.k === 'mid') - (b.k === 'mid') || b.e.id - a.e.id; });
  }
  function onObj(d, p, excl){
    var hits = []; d.ents.forEach(function(e){ if (e.id === excl || !okType(e) || !TC.entVisible(d, e)) return; segs(e).forEach(function(s){ if (G.segDist(p, s.a, s.b) < 1e-6) hits.push(s.ref); }); if ((e.t === 'circle' || e.t === 'arc') && Math.abs(G.dist(p, e.c) - e.r) < 1e-6 && (e.t === 'circle' || G.inArc(G.ang(e.c, p), e.a0, e.a1))) hits.push({ id:e.id }); });
    return hits.length === 1 ? hits[0] : null;
  }
  // ponytail: suy luận theo hình khớp tuyệt đối (điểm trùng, đoạn ngang/đứng), không đọc loại bắt điểm; cần phân biệt Giao điểm, Gần nhất thì lấy loại bắt từ cad.js
  function infer(d, e){
    if (!okType(e)) return;
    var cand = [], isRect = e.t === 'pline' && e.closed && e.pts.length === 4 && !(e.bulges || []).some(Boolean) && [0, 1, 2, 3].every(function(i){ var a = e.pts[i], b = e.pts[(i + 1) % 4], c = e.pts[(i + 2) % 4]; return Math.abs(dot(unit(sub(b, a)), unit(sub(c, b)))) < 1e-9; });
    cpts(e).forEach(function(q){ if (q.k === 'mid' || /^m\d/.test(q.k)) return;
      var hit = nearPts(d, q.p, 1e-6, e.id)[0];
      if (hit){ if (q.k === 'c' && hit.k === 'c' && /^(circle|arc)$/.test(e.t) && /^(circle|arc)$/.test(hit.e.t)) cand.push({ t:'conc', a:{ id:hit.e.id }, b:{ id:e.id } }); else cand.push({ t:'coin', a:{ id:hit.e.id, k:hit.k }, b:{ id:e.id, k:q.k } }); return; }
      if (e.t === 'line'){ var on = onObj(d, q.p, e.id); if (on){ cand.push({ t:'coin', a:{ id:e.id, k:q.k }, b:on }); var oe = d.byId[on.id]; if (rk(d, on) === 'ln'){ var L = segOf(d, on); if (Math.abs(dot(unit(sub(L[1], L[0])), unit(sub(e.p2, e.p1)))) < 1e-9) cand.push({ t:'perp', a:on, b:{ id:e.id } }); } else if (oe && Math.abs(dot(unit(sub(e.p2, e.p1)), unit(sub(q.p, oe.c)))) < 1e-9) cand.push({ t:'tan', a:{ id:e.id }, b:on, s:Math.sign(cross(unit(sub(e.p2, e.p1)), sub(oe.c, e.p1))) || 1 }); } }
    });
    if (isRect && Math.abs(e.pts[1][1] - e.pts[0][1]) > 1e-9 && Math.abs(e.pts[1][0] - e.pts[0][0]) > 1e-9) cand.push({ t:'par', a:{ id:e.id, s:0 }, b:{ id:e.id, s:2 } }, { t:'par', a:{ id:e.id, s:1 }, b:{ id:e.id, s:3 } }, { t:'perp', a:{ id:e.id, s:0 }, b:{ id:e.id, s:1 } });
    else segs(e).forEach(function(s){ var dx = s.b[0] - s.a[0], dy = s.b[1] - s.a[1], L = Math.hypot(dx, dy); if (L < 1e-6) return; if (Math.abs(dy) <= 1e-9 * Math.max(1, L)) cand.push({ t:'hor', a:s.ref }); else if (Math.abs(dx) <= 1e-9 * Math.max(1, L)) cand.push({ t:'vert', a:s.ref }); });
    if (e.t === 'circle') d.ents.forEach(function(o){ if (o === e || !TC.entVisible(d, o)) return; segs(o).forEach(function(s){ var u = unit(sub(s.b, s.a)), dd = cross(u, sub(e.c, s.a)); if (Math.abs(Math.abs(dd) - e.r) < 1e-6){ var f = G.foot(e.c, s.a, s.b); if (f.t >= 0 && f.t <= 1) cand.push({ t:'tan', a:s.ref, b:{ id:e.id }, s:Math.sign(dd) || 1 }); } }); });
    cand.forEach(function(c){ var r = tryAdd(d, c, { silent:true }); if (r.ok) INFN[c.t] = (INFN[c.t] || 0) + 1; });
  }

  /* ================= tự ràng buộc (AUTOCONSTRAIN) theo dung sai và thứ tự ưu tiên ================= */
  function autoCands(d, ids, only){
    var E = ids.map(function(id){ return d.byId[id]; }).filter(function(e){ return okType(e) && !e._paper; }), tD = CS.tolD, tA = CS.tolA, sA = Math.sin(rad(tA)), out = [];
    var L = [], C = E.filter(function(e){ return e.t === 'circle' || e.t === 'arc'; }), Pt = [];
    E.forEach(function(e){ segs(e).forEach(function(s){ s.e = e; s.u = unit(sub(s.b, s.a)); s.len = G.dist(s.a, s.b); if (s.len > 1e-6) L.push(s); }); cpts(e).forEach(function(q){ if (q.k !== 'mid' && !/^m\d/.test(q.k)) Pt.push({ e:e, k:q.k, p:q.p }); }); });
    var near = function(a, b){ return G.dist(a, b) <= tD; }, touchL = function(s, t){ return [s.a, s.b].some(function(p){ return near(p, t.a) || near(p, t.b) || G.segDist(p, t.a, t.b) <= tD; }) || [t.a, t.b].some(function(p){ return G.segDist(p, s.a, s.b) <= tD; }) || !!G.segInt(s.a, s.b, t.a, t.b); };
    var gen = {
      coin:function(){ for (var i = 0; i < Pt.length; i++) for (var j = i + 1; j < Pt.length; j++){ var a = Pt[i], b = Pt[j]; if (a.e === b.e || !near(a.p, b.p) || a.k === 'c' || b.k === 'c') continue; out.push({ t:'coin', a:{ id:a.e.id, k:a.k }, b:{ id:b.e.id, k:b.k } }); } },
      coll:function(){ pairs(L, function(s, t){ if (Math.abs(cross(s.u, t.u)) <= sA && Math.abs(cross(s.u, sub(t.a, s.a))) <= tD && Math.abs(cross(s.u, sub(t.b, s.a))) <= tD) out.push({ t:'coll', a:s.ref, b:t.ref }); }); },
      par:function(){ pairs(L, function(s, t){ if (Math.abs(cross(s.u, t.u)) <= sA && !(Math.abs(cross(s.u, sub(t.a, s.a))) <= tD)) out.push({ t:'par', a:s.ref, b:t.ref }); }); },
      perp:function(){ pairs(L, function(s, t){ if (Math.abs(dot(s.u, t.u)) <= sA && (!CS.perpInt || touchL(s, t))) out.push({ t:'perp', a:s.ref, b:t.ref }); }); },
      tan:function(){ L.forEach(function(s){ C.forEach(function(e){ var dd = cross(s.u, sub(e.c, s.a)), f = G.foot(e.c, s.a, s.b); if (Math.abs(Math.abs(dd) - e.r) > tD) return; if (CS.tanInt && (f.t < -1e-6 || f.t > 1 + 1e-6 || (e.t === 'arc' && !G.inArc(G.ang(e.c, f.q), e.a0, e.a1)))) return; out.push({ t:'tan', a:s.ref, b:{ id:e.id }, s:Math.sign(dd) || 1 }); }); });
        pairs(C, function(a, b){ var dd = G.dist(a.c, b.c); if (Math.abs(dd - a.r - b.r) <= tD) out.push({ t:'tan', a:{ id:a.id }, b:{ id:b.id }, s:1 }); else if (dd > tD && Math.abs(dd - Math.abs(a.r - b.r)) <= tD) out.push({ t:'tan', a:{ id:a.id }, b:{ id:b.id }, s:-1, s2:Math.sign(a.r - b.r) || 1 }); }); },
      conc:function(){ pairs(C, function(a, b){ if (near(a.c, b.c)) out.push({ t:'conc', a:{ id:a.id }, b:{ id:b.id } }); }); },
      hor:function(){ L.forEach(function(s){ if (Math.abs(s.u[1]) <= sA) out.push({ t:'hor', a:s.ref }); }); },
      vert:function(){ L.forEach(function(s){ if (Math.abs(s.u[0]) <= sA) out.push({ t:'vert', a:s.ref }); }); },
      equal:function(){ pairs(L, function(s, t){ if (Math.abs(s.len - t.len) <= tD) out.push({ t:'equal', a:s.ref, b:t.ref }); }); pairs(C, function(a, b){ if (Math.abs(a.r - b.r) <= tD) out.push({ t:'equal', a:{ id:a.id }, b:{ id:b.id } }); }); }
    };
    function pairs(list, fn){ for (var i = 0; i < list.length; i++) for (var j = i + 1; j < list.length; j++) fn(list[i], list[j]); }
    (only || CS.prio.filter(function(k){ return !CS.off[k]; })).forEach(function(k){ if (gen[k]) gen[k](); });
    return out;
  }
  function autoRun(ctx, d, ids, only){
    var cs = autoCands(d, ids, only), n = 0, skip = 0, by = {};
    if (!cs.length){ ctx.out('Không tìm thấy quan hệ nào trong dung sai (khoảng cách ' + numStr(CS.tolD) + ', góc ' + numStr(CS.tolA) + '°). Đổi dung sai ở Thiết lập ràng buộc › Tự ràng buộc.'); return; }
    ED.run('AUTOCONSTRAIN', function(){ cs.forEach(function(c){ var r = tryAdd(d, c, { silent:true }); if (r.ok){ n++; by[c.t] = (by[c.t] || 0) + 1; } else if (!r.dup) skip++; }); });
    TC.cad.clearSel(); V().draw();
    ctx.out(n ? 'Đã gắn <b>' + n + '</b> ràng buộc cho ' + ids.length + ' đối tượng: ' + Object.keys(by).map(function(k){ return GT[k][0] + ' ' + by[k]; }).join(', ') + '.' : 'Không gắn thêm được ràng buộc: các quan hệ tìm thấy đã có' + (skip ? ' hoặc sẽ thừa ràng buộc' : '') + '.');
  }

  /* ================= chọn điểm, đối tượng trong lệnh ================= */
  var PK = { mark:null }, PV = null, HOT = null, DRAG = null;
  function nearPt(id, wp){ var d = D(), e = id && d.byId[id]; if (!okType(e) || e._paper) return null; var best = null, bd = Infinity; cpts(e).forEach(function(q){ var dd = G.dist(q.p, wp); if (dd < bd){ bd = dd; best = q; } }); return best && { e:e, k:best.k, p:best.p }; }
  function pickPt(ctx, text, o){
    o = o || {};
    return ctx.point(text, { pick:true, opts:o.opts, def:o.def, onMove:function(p, temp, hov){ var h = nearPt(hov, p); PK.mark = h ? h.p : null; } }).then(function(r){
      PK.mark = null; if (!r || r.enter || r.kw || r.num != null) return r;
      var v = V(), s = r.s || v.mToS(r.p), id = v.hitTest(s[0], s[1]), h = nearPt(id, r.p), d = D();
      var bad = !id ? 'Không có đối tượng tại đây' : !h ? 'Đối tượng này chưa ràng buộc được' : (d.layerMap[h.e.layer] || {}).lck ? 'Đối tượng nằm trên layer bị khóa' : null;
      if (bad){ TC.ctip($('#cv'), s[0], s[1], bad); return pickPt(ctx, text, o); }
      return { e:h.e, ref:{ id:h.e.id, k:h.k }, p:h.p, s:s };
    });
  }
  var KT = { ln:['line', 'pline'], ci:['circle', 'arc'], sp:['spline'] };
  function pickObj(ctx, text, kinds, o){
    o = o || {}; var types = []; kinds.forEach(function(k){ types = types.concat(KT[k] || []); });
    var hint = { ln:'đoạn thẳng, đoạn của polyline', ci:'đường tròn, cung', sp:'spline' };
    return ED.pick(ctx, text, { types:types, opts:o.opts, def:o.def, bad:'Chọn ' + kinds.map(function(k){ return hint[k]; }).join(' hoặc ') }).then(function(r){
      if (!r || !r.e) return r; var e = r.e, ref = { id:e.id };
      if (e._paper){ TC.ctip($('#cv'), r.s[0], r.s[1], 'Ràng buộc chỉ gắn cho đối tượng trong Model'); return pickObj(ctx, text, kinds, o); }
      if (e.t === 'pline'){ var s = nearSeg(e, r.p); if ((e.bulges || [])[s]){ TC.ctip($('#cv'), r.s[0], r.s[1], 'Đoạn cong của polyline: chọn đoạn thẳng'); return pickObj(ctx, text, kinds, o); } ref.s = s; }
      r.ref = ref; return r;
    });
  }
  function ready(ctx){
    var d = D(); if (!d) return false;
    if (d.bedit || d.refedit){ ctx.out('Chưa gắn ràng buộc được trong ' + (d.bedit ? 'Block Editor' : 'chế độ sửa tại chỗ') + '. Đóng chế độ này để ràng buộc đối tượng của bản vẽ.'); return false; }
    if (d.space !== 'model'){ ctx.out('Ràng buộc gắn cho đối tượng trong Model. Chuyển sang tab <b>Model</b> rồi chạy lại lệnh.'); return false; }
    return true;
  }
  function ask(ctx, text, opts, def){ return ctx.val(text, def == null ? '' : def, { opts:opts }).then(function(v){ v = String(v == null ? '' : v).trim(); var k = opts.filter(function(o){ return o[1].toUpperCase() === v.toUpperCase() || (v && TC.fold(o[0]) === TC.fold(v)); })[0]; return k ? k[1] : v; }); }

  /* ================= hộp thoại xung đột ================= */
  function conflictDlg(d, c, res){
    return new Promise(function(resolve){
      var isDim = c.t === 'dim', list = res.conflict || [], sel = list.length === 1 ? list[0].id : null;
      var row = function(o){ return '<tr data-cf="' + o.id + '" class="' + (o.id === sel ? 'is-sel' : '') + '" tabindex="-1"><td class="ctr"><input type="radio" name="pcf" value="' + o.id + '"' + (o.id === sel ? ' checked' : '') + ' aria-label="Chọn ' + esc(consName(d, o)) + '"></td><td>' + ic(o.t === 'dim' ? DT[o.k][1] : GT[o.t][1], 's12') + ' ' + esc(consName(d, o)) + '</td><td class="faint">' + esc(consDesc(d, o)) + '</td></tr>'; };
      var dlg = TC.dialog({ id:'pcf', title:res.fail ? 'Ràng buộc mâu thuẫn' : 'Thừa ràng buộc', code:'UI-PAR', cmd:isDim ? DT[c.k][2] : GT[c.t][2], w:560, help:true,
        body:'<div class="stack"><div class="hrow" style="flex-wrap:nowrap;align-items:flex-start;gap:10px">' + ic('warn', 'st') + '<p>' + (res.fail ? 'Không thể gắn ' : 'Gắn ') + '<b>' + esc(isDim ? c.name + ' (' + DT[c.k][0].toLowerCase() + ')' : GT[c.t][0]) + '</b> cho ' + esc(consDesc(d, c)) + (res.fail ? ': hình không thỏa đồng thời với các ràng buộc đang có.' : ' sẽ thừa ràng buộc: hình đã được xác định bởi các ràng buộc dưới đây.') + '</p></div>' +
          (list.length ? '<div class="tblwrap" style="max-height:180px"><table class="tbl pcf-t"><thead><tr><th class="ctr" style="width:28px"></th><th style="width:170px">Ràng buộc đang có</th><th>Đối tượng</th></tr></thead><tbody>' + list.map(row).join('') + '</tbody></table></div><p class="faint" style="font-size:var(--fs-s)">Rê lên một dòng để thấy đối tượng trên bản vẽ. Chọn một ràng buộc rồi bấm Thay để xóa nó và gắn ràng buộc mới.</p>' : '') +
          (isDim ? '<div class="note">' + ic('info') + '<span>Tham số tham chiếu chỉ đo và hiện giá trị trong ngoặc, không điều khiển hình; dùng được trong biểu thức của tham số khác.</span></div>' : '') + '</div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Không gắn</button>' + (list.length ? '<button class="btn" type="button" id="pcf-rep"' + (sel ? '' : ' disabled') + '>Thay ràng buộc đã chọn</button>' : '') + (isDim ? '<button class="btn primary" type="button" id="pcf-ref">Tạo tham số tham chiếu</button>' : ''),
        onClose:function(r){ HOT = null; V().drawOver(); resolve(r || null); } });
      dlg.el.addEventListener('click', function(e){ var tr = e.target.closest('[data-cf]'); if (tr){ sel = tr.dataset.cf; $$('[data-cf]', dlg.el).forEach(function(x){ x.classList.toggle('is-sel', x === tr); $('input', x).checked = x === tr; }); var rb = $('#pcf-rep', dlg.el); if (rb) rb.disabled = false; } });
      dlg.el.addEventListener('mouseover', function(e){ var tr = e.target.closest('[data-cf]'); var o = tr && byCid(d, tr.dataset.cf); hot(o ? [o] : null); });
      var rep = $('#pcf-rep', dlg.el); if (rep) rep.addEventListener('click', function(){ dlg.close({ k:'rep', id:sel }); });
      var rf = $('#pcf-ref', dlg.el); if (rf) rf.addEventListener('click', function(){ dlg.close({ k:'ref' }); });
    });
  }
  // gắn ràng buộc trong lệnh: báo trùng, mở hộp xung đột, thay hoặc tạo tham chiếu theo lựa chọn
  function addFlow(ctx, d, c, o){
    var r = tryAdd(d, c, o), nm = c.t === 'dim' ? c.name : GT[c.t][0];
    if (r.ok){ ctx.out('Đã gắn ràng buộc <b>' + esc(nm) + '</b>.'); V().draw(); return Promise.resolve(true); }
    if (r.dup){ var ex = dupOf(d, c); ctx.out('Đã có ràng buộc ' + esc(ex ? consName(d, ex) : nm) + ' giữa các đối tượng này.'); return Promise.resolve(false); }
    if (r.bad){ ctx.out('<span class="err">Không gắn được ' + esc(nm) + ' cho các đối tượng này.</span>'); return Promise.resolve(false); }
    ctx.out('<span class="err">' + (r.fail ? 'Mâu thuẫn' : 'Thừa ràng buộc') + ':</span> ' + esc(nm) + ' xung đột với ' + (r.conflict.length ? r.conflict.map(function(x){ return esc(consName(d, x)); }).slice(0, 4).join(', ') : 'các ràng buộc đang có') + '.');
    return conflictDlg(d, c, r).then(function(ch){
      if (!ch){ ctx.out('Không gắn ' + esc(nm) + '.'); return false; }
      if (ch.k === 'ref'){ c.ref = true; c.expr = ''; commitAdd(d, c); ctx.out('Đã tạo tham số tham chiếu <b>' + esc(c.name) + '</b> = ' + fmtV(measure(d, c), c.k === 'ang') + '.'); V().draw(); return true; }
      var old = byCid(d, ch.id); if (!old) return false;
      delCons(d, [old]); var r2 = tryAdd(d, c, o);
      if (r2.ok){ ctx.out('Đã thay <b>' + esc(consName(d, old)) + '</b> bằng <b>' + esc(nm) + '</b>.'); V().draw(); return true; }
      ED.run('Giữ ràng buộc', function(){ var done = parState(d); P(d).list.push(old); touch(P(d)); done(); }); V().draw();
      ctx.out('<span class="err">Bỏ riêng ' + esc(consName(d, old)) + ' vẫn chưa đủ</span>, đã giữ lại ràng buộc này. Bỏ thêm ràng buộc khác bằng DELCONSTRAINT rồi thử lại.'); return false;
    });
  }

  /* ================= lệnh: ràng buộc hình học ================= */
  function gcRun(t){ return function(ctx, pre){ if (!ready(ctx)) return; ctx.badge('GC'); var d = D(); return ED.run(GT[t][2], function(){ return GC[t](ctx, d, pre); }); }; }
  function both(ctx, t, ka, kb, t1, t2, build, o){
    o = o || {};
    return pickObj(ctx, t1, ka, { opts:o.opts1, def:o.def1 }).then(function(a){
      if (!a) return; if (a.kw || a.enter) return o.onKw ? o.onKw(a) : null;
      return pickObj(ctx, t2, kb).then(function(b){ if (!b || !b.e) return; if (sameRef(a.ref, b.ref)){ ctx.out('Chọn hai đối tượng khác nhau.'); return; } var c = build(a, b); if (!c) return; return addFlow(ctx, D(), c, { anchor:[a.ref] }); });
    });
  }
  var GC = {
    hor:function(ctx, d){ return hv(ctx, d, 'hor'); }, vert:function(ctx, d){ return hv(ctx, d, 'vert'); },
    par:function(ctx){ return both(ctx, 'par', ['ln'], ['ln'], 'Chọn đối tượng thứ nhất', 'Chọn đối tượng thứ hai', function(a, b){ return { t:'par', a:a.ref, b:b.ref }; }); },
    perp:function(ctx){ return both(ctx, 'perp', ['ln'], ['ln'], 'Chọn đối tượng thứ nhất', 'Chọn đối tượng thứ hai', function(a, b){ return { t:'perp', a:a.ref, b:b.ref }; }); },
    coll:function(ctx, d){ return multi(ctx, d, 'coll', ['ln']); }, equal:function(ctx, d){ return multi(ctx, d, 'equal', ['ln', 'ci']); },
    conc:function(ctx){ return both(ctx, 'conc', ['ci'], ['ci'], 'Chọn đường tròn hoặc cung thứ nhất', 'Chọn đường tròn hoặc cung thứ hai', function(a, b){ return { t:'conc', a:a.ref, b:b.ref }; }); },
    tan:function(ctx, d){ return both(ctx, 'tan', ['ln', 'ci'], ['ln', 'ci'], 'Chọn đối tượng thứ nhất', 'Chọn đối tượng thứ hai', function(a, b){
      var la = rk(d, a.ref) === 'ln', lb = rk(d, b.ref) === 'ln'; if (la && lb){ ctx.out('Tiếp xúc cần ít nhất một đường tròn hoặc cung.'); return null; }
      if (la || lb){ var L = la ? a : b, Ci = la ? b : a, s = segOf(d, L.ref), ce = Ci.e; return { t:'tan', a:L.ref, b:Ci.ref, s:Math.sign(cross(unit(sub(s[1], s[0])), sub(ce.c, s[0]))) || 1 }; }
      var dd = G.dist(a.e.c, b.e.c), inner = dd < Math.max(a.e.r, b.e.r); return { t:'tan', a:a.ref, b:b.ref, s:inner ? -1 : 1, s2:Math.sign(a.e.r - b.e.r) || 1 }; }); },
    coin:function(ctx, d){
      return pickPt(ctx, 'Chọn điểm thứ nhất hoặc', { opts:[['Đối tượng', 'O'], ['Tự ràng buộc', 'A']], def:'Đối tượng' }).then(function(a){
        if (!a) return;
        if (a.kw === 'A') return ctx.sel('Chọn đối tượng cần tự ràng buộc trùng điểm').then(function(ids){ if (ids.length) return autoRun(ctx, d, ids, ['coin']); });
        if (a.enter || a.kw === 'O') return pickObj(ctx, 'Chọn đối tượng', ['ln', 'ci']).then(function(o){ if (!o || !o.e) return; return pickPt(ctx, 'Chọn điểm đặt lên đối tượng').then(function(q){ if (!q || !q.ref) return; if (q.e.id === o.e.id){ ctx.out('Chọn điểm của đối tượng khác.'); return; } return addFlow(ctx, d, { t:'coin', a:q.ref, b:o.ref }, { anchor:[o.ref] }); }); });
        if (!a.ref) return;
        return pickPt(ctx, 'Chọn điểm thứ hai hoặc', { opts:[['Đối tượng', 'O']], def:'Đối tượng' }).then(function(b){
          if (!b) return; if (b.enter || b.kw === 'O') return pickObj(ctx, 'Chọn đối tượng', ['ln', 'ci']).then(function(o){ if (o && o.e) return addFlow(ctx, d, { t:'coin', a:a.ref, b:o.ref }, { anchor:[o.ref] }); });
          if (!b.ref) return; if (b.e.id === a.e.id){ ctx.out('Hai điểm thuộc cùng một đối tượng: chọn điểm của đối tượng khác.'); return; }
          return addFlow(ctx, d, { t:'coin', a:a.ref, b:b.ref }, { anchor:[a.ref] });
        });
      });
    },
    fix:function(ctx, d){
      return pickPt(ctx, 'Chọn điểm hoặc', { opts:[['Đối tượng', 'O']], def:'Đối tượng' }).then(function(r){
        if (!r) return;
        if (r.enter || r.kw === 'O') return ED.pick(ctx, 'Chọn đối tượng', { types:['line', 'pline', 'circle', 'arc', 'spline', 'ellipse', 'point', 'text', 'mtext', 'insert'] }).then(function(o){ if (o && o.e) return addFlow(ctx, d, { t:'fix', a:{ id:o.e.id }, v:gp(o.e) }); });
        if (r.ref) return addFlow(ctx, d, { t:'fix', a:r.ref, v:r.p.slice() });
      });
    },
    smooth:function(ctx, d){
      return pickObj(ctx, 'Chọn spline thứ nhất', ['sp']).then(function(a){ if (!a || !a.e) return; var n = a.e.pts.length, ka = G.dist(a.p, a.e.pts[0]) <= G.dist(a.p, a.e.pts[n - 1]) ? 'v0' : 'v' + (n - 1);
        return pickObj(ctx, 'Chọn đường cong thứ hai', ['ln', 'ci', 'sp']).then(function(b){ if (!b || !b.e) return; var e = b.e, ends = e.t === 'line' ? [['p1', e.p1], ['p2', e.p2]] : e.t === 'arc' ? [['a0', polar(e.c, e.r, e.a0)], ['a1', polar(e.c, e.r, e.a1)]] : e.t === 'circle' ? [] : [['v0', e.pts[0]], ['v' + (e.pts.length - 1), e.pts[e.pts.length - 1]]];
          if (!ends.length){ ctx.out('Trơn cần một đầu mút: chọn đoạn thẳng, cung, spline hoặc polyline hở.'); return; }
          var kb = ends.sort(function(x, y){ return G.dist(x[1], b.p) - G.dist(y[1], b.p); })[0][0];
          return addFlow(ctx, d, { t:'smooth', a:{ id:a.e.id, k:ka }, b:{ id:e.id, k:kb } }, { anchor:[{ id:e.id }] }); }); });
    },
    sym:function(ctx, d){
      var axis = function(fn){ return pickObj(ctx, 'Chọn đường đối xứng', ['ln']).then(function(L){ if (L && L.e) return fn(L.ref); }); };
      return pickObj(ctx, 'Chọn đối tượng thứ nhất hoặc', ['ln', 'ci'], { opts:[['2 điểm', '2P']], def:'2 điểm' }).then(function(a){
        if (!a) return;
        if (a.enter || a.kw === '2P') return pickPt(ctx, 'Chọn điểm thứ nhất').then(function(p){ if (!p || !p.ref) return; return pickPt(ctx, 'Chọn điểm thứ hai').then(function(q){ if (!q || !q.ref) return; return axis(function(L){ return addFlow(ctx, d, { t:'sym', a:p.ref, b:q.ref, c:L }, { anchor:[p.ref, L] }); }); }); });
        if (!a.e) return;
        return pickObj(ctx, 'Chọn đối tượng thứ hai', [rk(d, a.ref) === 'ln' ? 'ln' : 'ci']).then(function(b){ if (!b || !b.e) return; return axis(function(L){ if (L.id === a.ref.id || L.id === b.ref.id){ ctx.out('Đường đối xứng phải là đối tượng khác.'); return; } return addFlow(ctx, d, { t:'sym', a:a.ref, b:b.ref, c:L }, { anchor:[a.ref, L] }); }); });
      });
    }
  };
  function hv(ctx, d, t){
    var name = GT[t][0].toLowerCase();
    return pickObj(ctx, 'Chọn đối tượng hoặc', ['ln'], { opts:[['2 điểm', '2P']], def:'2 điểm' }).then(function(r){
      if (!r) return;
      if (r.enter || r.kw === '2P') return pickPt(ctx, 'Chọn điểm thứ nhất').then(function(p){ if (!p || !p.ref) return; return pickPt(ctx, 'Chọn điểm thứ hai').then(function(q){ if (!q || !q.ref) return; if (sameRef(p.ref, q.ref)){ ctx.out('Chọn hai điểm khác nhau.'); return; } return addFlow(ctx, d, { t:t, a:p.ref, b:q.ref }, { anchor:[p.ref] }); }); });
      if (!r.e) return;
      // đoạn xoay quanh đầu mút gần chỗ bấm
      var s = segOf(d, r.ref), e = r.e, k = e.t === 'line' ? (G.dist(r.p, e.p1) <= G.dist(r.p, e.p2) ? 'p1' : 'p2') : 'v' + (G.dist(r.p, s[0]) <= G.dist(r.p, s[1]) ? r.ref.s : (r.ref.s + 1) % e.pts.length);
      ctx.out('Đoạn xoay quanh đầu mút gần chỗ bấm để thành ' + name + '.', 'faint');
      return addFlow(ctx, d, { t:t, a:r.ref }, { anchor:[{ id:e.id, k:k }] });
    });
  }
  function multi(ctx, d, t, kinds){
    var label = t === 'equal' ? 'bằng' : 'thẳng hàng với';
    return pickObj(ctx, 'Chọn đối tượng thứ nhất hoặc', kinds, { opts:[['Nhiều', 'M']] }).then(function(a){
      if (!a) return;
      if (a.kw === 'M') return pickObj(ctx, 'Chọn đối tượng thứ nhất', kinds).then(function(f){ if (!f || !f.e) return; var n = 0; return (function more(){ return pickObj(ctx, 'Chọn đối tượng ' + label + ' đối tượng thứ nhất', [rk(d, f.ref) === 'ln' ? 'ln' : 'ci'], { opts:[['Xong', 'X']], def:'Xong' }).then(function(b){ if (!b || !b.e){ ctx.out(n ? 'Đã gắn ' + n + ' ràng buộc ' + GT[t][0] + '.' : ''); return; } if (sameRef(b.ref, f.ref)) return more(); return addFlow(ctx, d, { t:t, a:f.ref, b:b.ref }, { anchor:[f.ref] }).then(function(ok){ if (ok) n++; return more(); }); }); })(); });
      if (!a.e) return;
      return pickObj(ctx, 'Chọn đối tượng thứ hai', [rk(d, a.ref) === 'ln' ? 'ln' : 'ci']).then(function(b){ if (!b || !b.e) return; if (sameRef(a.ref, b.ref)){ ctx.out('Chọn hai đối tượng khác nhau.'); return; } return addFlow(ctx, d, { t:t, a:a.ref, b:b.ref }, { anchor:[a.ref] }); });
    });
  }
  Object.keys(GT).forEach(function(t){ IMPL[GT[t][2]] = gcRun(t); });
  IMPL.GEOMCONSTRAINT = function(ctx, pre){
    if (!ready(ctx)) return; var order = ['hor', 'vert', 'perp', 'par', 'tan', 'smooth', 'coin', 'conc', 'coll', 'sym', 'equal', 'fix'];
    var go = function(k){ var t = order.filter(function(x){ return GT[x][3] === String(k || '').toUpperCase(); })[0] || 'hor'; LASTGC = t; ctx.out('Ràng buộc <b>' + GT[t][0] + '</b> (' + GT[t][2] + ')', 'muted'); return gcRun(t)(ctx); };
    if (pre) return go(pre);
    return ask(ctx, 'Nhập loại ràng buộc', order.map(function(t){ return [GT[t][0].replace(' (G2)', ''), GT[t][3]]; }), GT[LASTGC][0].replace(' (G2)', '')).then(go);
  };
  var LASTGC = 'hor';

  /* ================= lệnh: ràng buộc kích thước ================= */
  function dimBuild(d, k, a, b, p, ptLine){
    var c = { t:'dim', k:k, a:a, b:b }, P0 = ptOf(d, a);
    if (k === 'lin'){ var Q0 = ptOf(d, b), x0 = Math.min(P0[0], Q0[0]), x1 = Math.max(P0[0], Q0[0]), y0 = Math.min(P0[1], Q0[1]), y1 = Math.max(P0[1], Q0[1]), oX = p[0] < x0 || p[0] > x1, oY = p[1] < y0 || p[1] > y1; c.k = oY && !oX ? 'h' : oX && !oY ? 'v' : Math.abs(Q0[0] - P0[0]) >= Math.abs(Q0[1] - P0[1]) ? 'h' : 'v'; }
    if (c.k === 'h' || c.k === 'v'){ var Q = ptOf(d, b), ax = c.k === 'h' ? 0 : 1; c.s = Math.sign(Q[ax] - P0[ax]) || 1; c.off = c.k === 'h' ? p[1] - P0[1] : p[0] - P0[0]; }
    else if (c.k === 'al' && ptLine){ var L = segOf(d, b), u = unit(sub(L[1], L[0])); c.s = Math.sign(cross(u, sub(P0, L[0]))) || 1; c.off = dot(sub(p, P0), u); }
    else if (c.k === 'al'){ var Q2 = ptOf(d, b), n = unit([-(Q2[1] - P0[1]), Q2[0] - P0[0]]); c.off = dot(sub(p, P0), n); }
    return c;
  }
  function offAt(d, c, wp){
    if (c.k === 'h' || c.k === 'v'){ var P0 = ptOf(d, c.a); return c.k === 'h' ? wp[1] - P0[1] : wp[0] - P0[0]; }
    if (c.k === 'al'){ var A0 = ptOf(d, c.a); if (rk(d, c.b) === 'ln'){ var L = segOf(d, c.b); return dot(sub(wp, A0), unit(sub(L[1], L[0]))); } var B0 = ptOf(d, c.b); return dot(sub(wp, A0), unit([-(B0[1] - A0[1]), B0[0] - A0[0]])); }
    if (c.k === 'rad' || c.k === 'dia') return G.ang(d.byId[c.a.id].c, wp);
    var g = angGeo(d, c); return g ? Math.max(1, G.dist(g.X, wp)) : c.off;
  }
  // tạo ràng buộc kích thước rồi mở ô sửa giá trị tại chỗ (Enter áp dụng, Esc giữ giá trị đo)
  function dimCreate(ctx, d, c){
    var val = measure(d, c);
    if (!(val > 1e-9)){ ctx.out('<span class="err">Không đo được:</span> hai điểm trùng nhau hoặc hai đường song song.'); return; }
    var p = P(d); c.val = val; c.expr = numStr(val); c.name = nextName(p, DT[c.k][3]);
    return addFlow(ctx, d, c).then(function(ok){
      if (!ok || c.ref) return;
      var oth = items(P(d)).filter(function(x){ return x.key !== c.id; })[0];
      ctx.out('Giá trị đo = <b class="mono">' + fmtV(val, c.k === 'ang') + '</b>. Gõ giá trị hoặc biểu thức rồi Enter, ví dụ ' + esc(c.name) + '=' + (oth ? esc(oth.name) + '/2' : numStr(Math.round(val * 1.5))) + '; Esc giữ giá trị đo.', 'faint');
      V().draw(); var ed = editor(d, c);
      return Promise.race([ed.done, ctx.val('Giá trị hoặc biểu thức', c.name + '=' + c.expr, { allowSpace:true })]).then(function(v){
        ed.close(); if (typeof v !== 'string' || !v.trim() || v.trim() === c.name + '=' + c.expr) return;
        var ne = splitNE(v, c.name), r = setParam(d, c.id, ne[0], ne[1], DT[c.k][2]); if (r.err) ctx.out('<span class="err">' + esc(r.err) + '</span> Giữ giá trị đo ' + fmtV(val, c.k === 'ang') + '. Sửa lại bằng nhấp đúp vào ' + esc(c.name) + '.');
      }, function(err){ ed.close(); throw err; });
    });
  }
  function dcLinear(k0){
    return function(ctx){
      if (!ready(ctx)) return; ctx.badge('DC'); var d = D(), cmd = k0 === 'lin' ? 'DCLINEAR' : DT[k0][2];
      function place(a, b, ptLine){
        return ctx.point('Chỉ định vị trí đường kích thước', { rubber:false, dyn:false, onMove:function(p){ PV = dimBuild(d, k0, a, b, p, ptLine); PV.id = 'pv'; PV.val = measure(d, PV); PV.name = nextName(P(d), 'd'); PV.expr = numStr(PV.val); } }).then(function(z){ PV = null; if (!z || !z.p) return; return dimCreate(ctx, d, dimBuild(d, k0, a, b, z.p, ptLine)); });
      }
      return ED.run(cmd, function(){
        return pickPt(ctx, 'Chọn điểm ràng buộc thứ nhất hoặc', { opts:k0 === 'al' ? [['Đối tượng', 'O'], ['Điểm và đường', 'P']] : [['Đối tượng', 'O']], def:'Đối tượng' }).then(function(r){
          if (!r) return;
          if (r.kw === 'P') return pickPt(ctx, 'Chọn điểm ràng buộc').then(function(q){ if (!q || !q.ref) return; return pickObj(ctx, 'Chọn đường', ['ln']).then(function(z){ if (z && z.ref) return place(q.ref, z.ref, true); }); });
          if (r.enter || r.kw === 'O') return pickObj(ctx, 'Chọn đối tượng', ['ln']).then(function(o){ if (!o || !o.e) return; var e = o.e, n = e.t === 'pline' ? e.pts.length : 0; return place(e.t === 'line' ? { id:e.id, k:'p1' } : { id:e.id, k:'v' + o.ref.s }, e.t === 'line' ? { id:e.id, k:'p2' } : { id:e.id, k:'v' + ((o.ref.s + 1) % n) }); });
          if (!r.ref) return;
          return pickPt(ctx, 'Chọn điểm ràng buộc thứ hai').then(function(q){ if (!q || !q.ref) return; if (sameRef(q.ref, r.ref)){ ctx.out('Chọn hai điểm khác nhau.'); return; } return place(r.ref, q.ref); });
        });
      });
    };
  }
  function dcRadial(k){
    return function(ctx){
      if (!ready(ctx)) return; ctx.badge('DC'); var d = D();
      return ED.run(DT[k][2], function(){ return pickObj(ctx, 'Chọn cung hoặc đường tròn', ['ci']).then(function(o){ if (!o || !o.e) return; var c0 = function(p){ return { t:'dim', k:k, a:{ id:o.e.id }, off:G.ang(o.e.c, p) }; };
        return ctx.point('Chỉ định vị trí đường kích thước', { rubber:false, dyn:false, onMove:function(p){ PV = c0(p); PV.id = 'pv'; PV.val = measure(d, PV); PV.name = nextName(P(d), DT[k][3]); PV.expr = numStr(PV.val); } }).then(function(z){ PV = null; if (z && z.p) return dimCreate(ctx, d, c0(z.p)); }); }); });
    };
  }
  function angGeo(d, c){
    if (rk(d, c.a) === 'ci'){ var e = d.byId[c.a.id]; return { X:e.c, a0:e.a0, a1:e.a1 }; }
    var L = segOf(d, c.a), M = segOf(d, c.b), X = ED.lineInt(L[0], L[1], M[0], M[1]); if (!X) return null;
    var e1 = unit(sub(L[1], L[0])), e2 = unit(sub(M[1], M[0])); e1 = [e1[0] * c.s1, e1[1] * c.s1]; e2 = [e2[0] * c.s2, e2[1] * c.s2];
    var A1 = Math.atan2(e1[1], e1[0]) * 180 / Math.PI, A2 = Math.atan2(e2[1], e2[0]) * 180 / Math.PI; return c.cw ? { X:X, a0:A2, a1:A1 } : { X:X, a0:A1, a1:A2 };
  }
  IMPL.DCANGULAR = function(ctx){
    if (!ready(ctx)) return; ctx.badge('DC'); var d = D();
    return ED.run('DCANGULAR', function(){ return pickObj(ctx, 'Chọn đoạn thẳng thứ nhất hoặc cung', ['ln', 'ci']).then(function(a){
      if (!a || !a.e) return;
      if (a.e.t === 'circle'){ ctx.out('Chọn cung hoặc đoạn thẳng; đường tròn không có góc.'); return; }
      if (a.e.t === 'arc') return ctx.point('Chỉ định vị trí cung kích thước', { rubber:false, dyn:false, onMove:function(p){ PV = { id:'pv', t:'dim', k:'ang', a:{ id:a.e.id }, off:Math.max(a.e.r * 1.15, G.dist(a.e.c, p)), name:nextName(P(d), 'ang') }; PV.val = measure(d, PV); PV.expr = numStr(PV.val); } }).then(function(z){ PV = null; if (z && z.p) return dimCreate(ctx, d, { t:'dim', k:'ang', a:{ id:a.e.id }, off:Math.max(a.e.r * 1.15, G.dist(a.e.c, z.p)) }); });
      return pickObj(ctx, 'Chọn đoạn thẳng thứ hai', ['ln']).then(function(b){
        if (!b || !b.e) return; var L = segOf(d, a.ref), M = segOf(d, b.ref), X = ED.lineInt(L[0], L[1], M[0], M[1]);
        if (!X){ ctx.out('Hai đường song song: không có góc. Dùng GCPARALLEL hoặc DCALIGNED.'); return; }
        var mk = function(p){ var u1 = unit(sub(L[1], L[0])), u2 = unit(sub(M[1], M[0])), w = sub(p, X), s1 = dot(w, u1) >= 0 ? 1 : -1, s2 = dot(w, u2) >= 0 ? 1 : -1, e1 = [u1[0] * s1, u1[1] * s1], e2 = [u2[0] * s2, u2[1] * s2]; return { t:'dim', k:'ang', a:a.ref, b:b.ref, s1:s1, s2:s2, cw:cross(e1, e2) < 0, off:Math.max(1, G.dist(X, p)) }; };
        return ctx.point('Chỉ định vị trí cung kích thước', { rubber:false, dyn:false, onMove:function(p){ PV = mk(p); PV.id = 'pv'; PV.name = nextName(P(d), 'ang'); PV.val = measure(d, PV); PV.expr = numStr(PV.val); } }).then(function(z){ PV = null; if (z && z.p) return dimCreate(ctx, d, mk(z.p)); });
      });
    }); });
  };
  IMPL.DCLINEAR = dcLinear('lin'); IMPL.DCHORIZONTAL = dcLinear('h'); IMPL.DCVERTICAL = dcLinear('v'); IMPL.DCALIGNED = dcLinear('al');
  IMPL.DCRADIUS = dcRadial('rad'); IMPL.DCDIAMETER = dcRadial('dia');
  // kích thước thường → ràng buộc: tìm điểm hình học trùng điểm gốc của kích thước
  function findPt(d, p){ var h = nearPts(d, p, 1e-3, null)[0]; return h ? { id:h.e.id, k:h.k } : null; }
  function convDim(d, e){
    if (e.t === 'dim'){ var a = findPt(d, e.p1), b = findPt(d, e.p2); if (!a || !b || sameRef(a, b)) return null; var k = e.dir === 'a' ? 'al' : e.dir, c = { t:'dim', k:k, a:a, b:b };
      if (k === 'h' || k === 'v'){ var ax = k === 'h' ? 0 : 1; c.s = Math.sign(e.p2[ax] - e.p1[ax]) || 1; c.off = e.pos - (k === 'h' ? e.p1[1] : e.p1[0]); } else c.off = e.off || 0; return c; }
    if (e.t === 'dimr'){ var ce = d.ents.filter(function(o){ return (o.t === 'circle' || o.t === 'arc') && G.dist(o.c, e.c) < 1e-3 && Math.abs(o.r - e.r) < 1e-3; })[0]; return ce ? { t:'dim', k:e.dia ? 'dia' : 'rad', a:{ id:ce.id }, off:e.ang || 45 } : null; }
    if (e.t === 'dima'){
      var ar = d.ents.filter(function(o){ return o.t === 'arc' && G.dist(o.c, e.c) < 1e-3 && Math.abs(sweep(o.a0, o.a1) - sweep(e.a0, e.a1)) < 1e-3; })[0]; if (ar) return { t:'dim', k:'ang', a:{ id:ar.id }, off:e.r };
      var thru = function(an){ var u = [Math.cos(rad(an)), Math.sin(rad(an))], best = null; d.ents.forEach(function(o){ segs(o).forEach(function(s){ var w = unit(sub(s.b, s.a)); if (!best && Math.abs(cross(w, u)) < 1e-6 && Math.abs(cross(w, sub(e.c, s.a))) < 1e-3) best = { ref:s.ref, s:dot(w, u) >= 0 ? 1 : -1 }; }); }); return best; };
      var l1 = thru(e.a0), l2 = thru(e.a1); return l1 && l2 && l1.ref.id !== l2.ref.id ? { t:'dim', k:'ang', a:l1.ref, b:l2.ref, s1:l1.s, s2:l2.s, cw:false, off:e.r } : null;
    }
    return null;
  }
  function convert(ctx, d, ids){
    var dims = ids.map(function(id){ return d.byId[id]; }).filter(function(e){ return e && /^dim[ra]?$/.test(e.t); }), n = 0, miss = 0, skip = 0;
    if (!dims.length){ ctx.out('Không có kích thước nào trong vùng chọn.'); return; }
    ED.run('DCCONVERT', function(){ dims.forEach(function(e){ var c = convDim(d, e); if (!c){ miss++; return; } c.val = measure(d, c); c.expr = numStr(c.val); c.name = nextName(P(d), DT[c.k][3]); var r = tryAdd(d, c, { silent:true }); if (r.ok){ n++; inSolve++; try { ED.del(e); } finally { inSolve--; } } else skip++; }); });
    TC.cad.clearSel(); V().draw();
    ctx.out((n ? 'Đã chuyển <b>' + n + '</b> kích thước thành ràng buộc.' : 'Chưa chuyển được kích thước nào.') + (miss ? ' ' + miss + ' kích thước không gắn với điểm của đối tượng nên giữ nguyên (ghi kích thước bằng bắt điểm rồi chuyển lại).' : '') + (skip ? ' ' + skip + ' kích thước sẽ thừa ràng buộc nên giữ nguyên; dùng DCLINEAR và chọn Tạo tham số tham chiếu nếu cần.' : ''));
  }
  IMPL.DCCONVERT = function(ctx){ if (!ready(ctx)) return; var d = D(); return ctx.sel('Chọn kích thước cần chuyển thành ràng buộc').then(function(ids){ if (ids.length) convert(ctx, d, ids); }); };
  IMPL.DIMCONSTRAINT = function(ctx, pre){
    if (!ready(ctx)) return; var d = D(), map = { LI:IMPL.DCLINEAR, H:IMPL.DCHORIZONTAL, V:IMPL.DCVERTICAL, AL:IMPL.DCALIGNED, AN:IMPL.DCANGULAR, R:IMPL.DCRADIUS, D:IMPL.DCDIAMETER };
    if (pre && map[String(pre).toUpperCase()]) return map[String(pre).toUpperCase()](ctx);
    return ED.pick(ctx, 'Chọn kích thước cần chuyển hoặc', { types:['dim', 'dimr', 'dima'], bad:'Chọn một kích thước, hoặc một tùy chọn', opts:[['Thẳng', 'LI'], ['Ngang', 'H'], ['Đứng', 'V'], ['Nghiêng', 'AL'], ['Góc', 'AN'], ['Bán kính', 'R'], ['Đường kính', 'D']], def:'Nghiêng' }).then(function(r){
      if (!r) return; if (r.enter) return IMPL.DCALIGNED(ctx); if (r.kw) return map[r.kw](ctx); if (r.e) convert(ctx, d, [r.e.id]);
    });
  };
  IMPL.DCDISPLAY = function(ctx, pre){
    var d = D(); if (!d) return;
    function all(on){ ED.run('DCDISPLAY', function(){ var done = parState(d), p = P(d); p.dyn = on; if (on) p.list.forEach(function(c){ if (c.t === 'dim') delete c.hide; }); touch(p); done(); }); V().draw(); ctx.out(on ? 'Đã hiện mọi ràng buộc động.' : 'Đã ẩn mọi ràng buộc động. Chạy lại DCDISPLAY để hiện.'); }
    if (pre === 'S' || pre === 'H') return all(pre === 'S');
    function forIds(ids){ var p = P(d), cs = p.list.filter(function(c){ return c.t === 'dim' && entsOf(c).some(function(id){ return ids.indexOf(id) >= 0; }); }); if (!cs.length){ ctx.out('Các đối tượng đã chọn không có ràng buộc kích thước.'); return; }
      return ask(ctx, 'Nhập tùy chọn', [['Hiện', 'S'], ['Ẩn', 'H']], 'Hiện').then(function(k){ var on = k !== 'H'; ED.run('DCDISPLAY', function(){ var done = parState(d), p = P(d); if (on) p.dyn = true; p.list.forEach(function(c){ if (cs.some(function(x){ return x.id === c.id; })){ if (on) delete c.hide; else c.hide = true; } }); touch(p); done(); }); TC.cad.clearSel(); V().draw(); ctx.out((on ? 'Đã hiện ' : 'Đã ẩn ') + cs.length + ' ràng buộc động.'); }); }
    if (d.sel.length) return forIds(d.sel.slice());
    ctx.out('Bấm hoặc kéo khung để chọn đối tượng rồi Enter; Enter khi chưa chọn để ' + (P(d).dyn ? 'ẩn' : 'hiện') + ' mọi ràng buộc động.', 'faint');
    return ask(ctx, 'Chọn đối tượng hoặc', [['Hiện tất cả', 'S'], ['Ẩn tất cả', 'H']]).then(function(k){ if (k === 'S' || k === 'H') return all(k === 'S'); if (d.sel.length) return forIds(d.sel.slice()); return all(!P(d).dyn); });
  };

  /* ================= lệnh: quản lý ================= */
  IMPL.AUTOCONSTRAIN = function(ctx){
    if (!ready(ctx)) return; var d = D(); ctx.badge('AC');
    if (d.sel.length) return autoRun(ctx, d, d.sel.slice());
    ctx.out('Bấm hoặc kéo khung để chọn đối tượng, Enter để tự ràng buộc.', 'faint');
    return (function step(){ return ask(ctx, 'Chọn đối tượng hoặc', [['Thiết lập', 'S']]).then(function(k){ if (k === 'S'){ settingsDlg('a'); return step(); } if (!d.sel.length){ ctx.out('Chưa chọn đối tượng nào.'); return; } return autoRun(ctx, d, d.sel.slice()); }); })();
  };
  IMPL.DELCONSTRAINT = function(ctx){
    if (!ready(ctx)) return; var d = D(); ctx.out('Xóa mọi ràng buộc hình học và kích thước của đối tượng đã chọn.', 'faint');
    return ctx.sel('Chọn đối tượng').then(function(ids){ if (!ids.length) return; var cs = P(d).list.filter(function(c){ return entsOf(c).some(function(id){ return ids.indexOf(id) >= 0; }); }), n = delCons(d, cs, 'DELCONSTRAINT'); TC.cad.clearSel(); V().draw(); ctx.out(n ? 'Đã xóa <b>' + n + '</b> ràng buộc.' : 'Các đối tượng đã chọn không có ràng buộc.'); });
  };
  IMPL.CONSTRAINTBAR = function(ctx, pre){
    var d = D(); if (!d) return;
    function set(ids, show, reset){ var p = P(d), ix = index(d), cs = ids.filter(function(id){ return ix[id]; }); if (!cs.length){ ctx.out('Các đối tượng đã chọn không có ràng buộc hình học.'); return; }
      ED.run('CONSTRAINTBAR', function(){ var done = parState(d); cs.forEach(function(id){ if (show) delete p.hide[id]; else p.hide[id] = 1; }); if (reset) p.list.forEach(function(c){ if (c.t !== 'dim' && entsOf(c).some(function(id){ return cs.indexOf(id) >= 0; })) delete c.hide; }); touch(p); done(); }); V().draw();
      ctx.out((show ? 'Đã hiện' : 'Đã ẩn') + ' thanh ràng buộc của ' + cs.length + ' đối tượng.'); }
    function all(show){ var p = P(d), ids = Object.keys(index(d)).map(Number).filter(function(id){ return d.byId[id]; }); if (!ids.length){ ctx.out('Bản vẽ chưa có ràng buộc hình học.'); return; } ED.run('CONSTRAINTBAR', function(){ var done = parState(d); if (show){ p.hide = {}; p.list.forEach(function(c){ if (c.t !== 'dim') delete c.hide; }); } else ids.forEach(function(id){ p.hide[id] = 1; }); touch(p); done(); }); V().draw(); ctx.out(show ? 'Đã hiện mọi thanh ràng buộc.' : 'Đã ẩn mọi thanh ràng buộc. Hiện lại: CONSTRAINTBAR › Hiện tất cả.'); }
    if (pre === 'S' || pre === 'H') return all(pre === 'S');
    function opt(ids){ return ask(ctx, 'Nhập tùy chọn', [['Hiện', 'S'], ['Ẩn', 'H'], ['Đặt lại', 'R']], 'Hiện').then(function(k){ set(ids, k !== 'H', k === 'R'); TC.cad.clearSel(); }); }
    if (d.sel.length) return opt(d.sel.slice());
    ctx.out('Bấm hoặc kéo khung để chọn đối tượng rồi Enter; Enter khi chưa chọn để hiện mọi thanh.', 'faint');
    return ask(ctx, 'Chọn đối tượng hoặc', [['Hiện tất cả', 'S'], ['Ẩn tất cả', 'H']]).then(function(k){ if (k === 'H') return all(false); if (k === 'S' || !d.sel.length) return all(true); return opt(d.sel.slice()); });
  };
  IMPL.PARAMETERS = function(){ A().showPalette('params'); };
  IMPL.CONSTRAINTSETTINGS = function(ctx, pre){ settingsDlg(pre === 'auto' || pre === 'a' ? 'a' : pre === 'dim' || pre === 'd' ? 'd' : 'g'); };

  /* ================= hiển thị: ràng buộc động, thanh ràng buộc (lớp nổi đi theo pan/zoom) ================= */
  function labelOf(c, v, fmt){
    var val = isFinite(v) ? fmtV(v, c.k === 'ang') : '?', f = fmt || CS.fmt;
    if (c.ref) return f === 'n' ? '(' + c.name + ')' : f === 'v' ? '(' + val + ')' : '(' + c.name + '=' + val + ')';
    var ex = c.expr === numStr(c.val) ? val : c.expr; return f === 'n' ? c.name : f === 'v' ? val : c.name + '=' + ex;
  }
  // hình của ràng buộc động trên màn hình: đường gióng, đường kích thước, mũi tên, vị trí nhãn
  function dimShape(d, c, v, off){
    var S = function(p){ return v.mToS(p); }, sc = v.modelView().v.s, k = c.k, g = { segs:[], tips:[], arc:null }, ext = function(a, b){ var dd = len(sub(b, a)); if (dd < 2) return; var u = unit(sub(b, a)); g.segs.push([[a[0] + u[0] * 3, a[1] + u[1] * 3], [b[0] + u[0] * 5, b[1] + u[1] * 5]]); };
    off = off != null ? off : c.off || 0;
    try {
      if (k === 'h' || k === 'v' || (k === 'al' && rk(d, c.b) !== 'ln')){
        var P0 = ptOf(d, c.a), Q0 = ptOf(d, c.b), P1, Q1;
        if (k === 'h'){ var y = P0[1] + off; P1 = [P0[0], y]; Q1 = [Q0[0], y]; } else if (k === 'v'){ var x = P0[0] + off; P1 = [x, P0[1]]; Q1 = [x, Q0[1]]; } else { var n = unit([-(Q0[1] - P0[1]), Q0[0] - P0[0]]); P1 = [P0[0] + n[0] * off, P0[1] + n[1] * off]; Q1 = [Q0[0] + n[0] * off, Q0[1] + n[1] * off]; }
        var a = S(P0), b = S(Q0), a1 = S(P1), b1 = S(Q1); ext(a, a1); ext(b, b1); g.segs.push([a1, b1]); var u = unit(sub(b1, a1)); g.tips.push([a1, [-u[0], -u[1]]], [b1, u]); g.lab = mid(a1, b1); return g;
      }
      if (k === 'al'){ var Pa = ptOf(d, c.a), L = segOf(d, c.b), w = unit(sub(L[1], L[0])), F = G.foot(Pa, L[0], L[1]).q, Pa1 = [Pa[0] + w[0] * off, Pa[1] + w[1] * off], F1 = [F[0] + w[0] * off, F[1] + w[1] * off], s1 = S(Pa1), s2 = S(F1); ext(S(Pa), s1); ext(S(F), s2); g.segs.push([s1, s2]); var u2 = unit(sub(s2, s1)); g.tips.push([s1, [-u2[0], -u2[1]]], [s2, u2]); g.lab = mid(s1, s2); return g; }
      if (k === 'rad' || k === 'dia'){ var e = d.byId[c.a.id], dir = [Math.cos(rad(off)), Math.sin(rad(off))], E = S([e.c[0] + dir[0] * e.r, e.c[1] + dir[1] * e.r]), C0 = S(k === 'dia' ? [e.c[0] - dir[0] * e.r, e.c[1] - dir[1] * e.r] : e.c), du = [dir[0], -dir[1]], far = [E[0] + du[0] * 18, E[1] + du[1] * 18]; g.segs.push([C0, far]); g.tips.push([E, du]); if (k === 'dia') g.tips.push([C0, [-du[0], -du[1]]]); g.lab = [far[0] + du[0] * 6, far[1] + du[1] * 6]; g.labDir = du; return g; }
      if (k === 'ang'){ var ag = angGeo(d, c); if (!ag) return null; var X = S(ag.X), R = Math.max(12, (off || 1) * sc), sw = sweep(ag.a0, ag.a1); g.arc = { c:X, r:R, a0:ag.a0, a1:ag.a0 + sw }; var t0 = rad(ag.a0), t1 = rad(ag.a0 + sw), am = rad(ag.a0 + sw / 2);
        g.tips.push([[X[0] + R * Math.cos(t0), X[1] - R * Math.sin(t0)], [Math.sin(t0), Math.cos(t0)]], [[X[0] + R * Math.cos(t1), X[1] - R * Math.sin(t1)], [-Math.sin(t1), -Math.cos(t1)]]);
        if (rk(d, c.a) === 'ln'){ [ag.a0, ag.a0 + sw].forEach(function(an){ var t = rad(an); g.segs.push([[X[0] + 4 * Math.cos(t), X[1] - 4 * Math.sin(t)], [X[0] + (R + 6) * Math.cos(t), X[1] - (R + 6) * Math.sin(t)]]); }); }
        g.lab = [X[0] + (R + 14) * Math.cos(am), X[1] - (R + 14) * Math.sin(am)]; return g; }
    } catch (err){ return null; }
    return null;
  }
  function drawShape(ctx, g, hotOn, err){
    ctx.save(); var col = TC.cssv(err ? '--c-danger' : '--c-track'); ctx.strokeStyle = ctx.fillStyle = col; ctx.lineWidth = hotOn ? 2 : 1; ctx.globalAlpha = hotOn ? 1 : .9; ctx.setLineDash([]);
    ctx.beginPath(); g.segs.forEach(function(s){ ctx.moveTo(s[0][0], s[0][1]); ctx.lineTo(s[1][0], s[1][1]); }); ctx.stroke();
    if (g.arc){ ctx.beginPath(); ctx.arc(g.arc.c[0], g.arc.c[1], g.arc.r, -rad(g.arc.a0), -rad(g.arc.a1), true); ctx.stroke(); }
    g.tips.forEach(function(t){ var p = t[0], u = t[1], n = [-u[1], u[0]]; ctx.beginPath(); ctx.moveTo(p[0], p[1]); ctx.lineTo(p[0] - u[0] * 8 + n[0] * 2.6, p[1] - u[1] * 8 + n[1] * 2.6); ctx.lineTo(p[0] - u[0] * 8 - n[0] * 2.6, p[1] - u[1] * 8 - n[1] * 2.6); ctx.closePath(); ctx.fill(); });
    ctx.restore();
  }
  function hot(cs){ var d = D(); HOT = cs && cs.length ? { d:d, ids:cs.map(function(c){ return c.id; }), ents:[].concat.apply([], cs.map(entsOf)) } : null; paintHot(); V().drawOver(); }
  function paintHot(){ if (!LAY.el) return; $$('[data-c]', LAY.el).forEach(function(el){ el.classList.toggle('is-hot', !!HOT && el.dataset.c.split(',').some(function(id){ return HOT.ids.indexOf(id) >= 0; })); }); $$('.pcb', LAY.el).forEach(function(b){ b.classList.toggle('is-hot', !!$('.is-hot', b)); }); }
  // vị trí thanh: giữa đối tượng (đoạn), lệch ra phía ngoài nhóm hình
  function barAt(d, v, e, s, cen){
    var S = function(p){ return v.mToS(p); };
    if (e.t === 'line' || e.t === 'pline'){ var a = e.t === 'line' ? e.p1 : e.pts[s], b = e.t === 'line' ? e.p2 : e.pts[(s + 1) % e.pts.length], sa = S(a), sb = S(b), m = mid(sa, sb), u = unit(sub(sb, sa)), n = [-u[1], u[0]], sc = cen ? S(cen) : null; if (sc && dot(sub(sc, m), n) > 0) n = [-n[0], -n[1]]; if (!sc && n[1] > 0) n = [-n[0], -n[1]]; return len(sub(sb, sa)) < 24 ? null : [m[0] + n[0] * 15, m[1] + n[1] * 15]; }
    if (e.t === 'circle' || e.t === 'arc'){ var an = e.t === 'arc' ? e.a0 + sweep(e.a0, e.a1) / 2 : 45, q = S(polar(e.c, e.r, an)), cc = S(e.c), w = unit(sub(q, cc)); return [q[0] + w[0] * 16, q[1] + w[1] * 16]; }
    var p = S(e.c || e.p || (e.pts && e.pts[0])); return p ? [p[0] + 18, p[1] - 14] : null;
  }
  var LAY = { el:null, html:'' }, EMPTY = { list:[], users:[], hide:{}, dyn:true };
  function layer(){ var cv = $('#cv'); if (!cv) return null; if (!LAY.el || !cv.contains(LAY.el)){ LAY.el = TC.el('<div class="pc-lay" aria-label="Ràng buộc trên vùng vẽ"></div>'); LAY.html = ''; cv.appendChild(LAY.el); bindLayer(LAY.el); } return LAY.el; }
  function setLayer(html){ var el = layer(); if (!el) return; el.style.setProperty('--pcb-o', String(1 - CS.transp / 100)); if (html !== LAY.html){ LAY.html = html; el.innerHTML = html; } paintHot(); }
  TC.overlays.push(function(ctx, v){
    var d = v.doc, main = v === V();
    if (!d || !canPar(d) || !v.modelView()){ if (main) setLayer(''); return; }
    var p = d.par || EMPTY, sel = {}, html = '', mvs = v.modelView().v; (d.sel || []).forEach(function(id){ sel[id] = 1; });
    // tô sáng đối tượng của ràng buộc đang rê
    if (HOT && HOT.d === d) HOT.ents.forEach(function(id){ var e = d.byId[id]; if (!e) return; ctx.globalAlpha = .22; v.drawPrims(ctx, e, mvs, false, { color:TC.cssv('--c-hl-hover'), lw:6 }); ctx.globalAlpha = 1; v.drawPrims(ctx, e, mvs, false, { lw:2.2 }); });
    var vis = function(c){ return live(d, c) && entsOf(c).every(function(id){ return TC.entVisible(d, d.byId[id]); }); };
    // ràng buộc động
    var dims = p.list.filter(function(c){ return c.t === 'dim' && vis(c) && ((p.dyn && !c.hide) || (CS.dynSel && entsOf(c).some(function(id){ return sel[id]; }))); });
    if (PV) dims = dims.concat([PV]);
    var ev = dims.length ? evalAll(d, p) : null, placed = [];
    dims.forEach(function(c){ var g = dimShape(d, c, v, DRAG && DRAG.id === c.id ? DRAG.off : null); if (!g || !g.lab) return; var isH = HOT && HOT.ids.indexOf(c.id) >= 0; drawShape(ctx, g, isH); if (!main) return;
      var val = c.id === 'pv' ? c.val : c.ref ? measure(d, c) : ev.v[c.id], txt = labelOf(c, val), lw = txt.length * 6.4 + (CS.lock ? 34 : 20), lab = g.labDir ? [g.lab[0] + g.labDir[0] * txt.length * 3.3, g.lab[1] + g.labDir[1] * 9] : g.lab;
      // nhãn chồng nhãn khác: dời theo đường dẫn (bán kính) hoặc dọc đường kích thước
      var dir = g.labDir || (g.segs.length ? unit(sub(g.segs[g.segs.length - 1][1], g.segs[g.segs.length - 1][0])) : [1, 0]);
      for (var t = 0; t < 4 && placed.some(function(r){ return Math.abs(r[0] - lab[0]) < (r[2] + lw) / 2 + 2 && Math.abs(r[1] - lab[1]) < 20; }); t++) lab = [lab[0] + dir[0] * (lw / 2 + 8), lab[1] + dir[1] * (Math.abs(dir[1]) > .7 ? 22 : lw / 2 + 8)];
      placed.push([lab[0], lab[1], lw]);
      html += '<button class="pcd' + (c.ref ? ' is-ref' : '') + (c.id === 'pv' ? ' is-pv' : '') + '" type="button" data-c="' + c.id + '" data-d="' + c.id + '" style="left:' + Math.round(lab[0]) + 'px;top:' + Math.round(lab[1]) + 'px" aria-label="' + esc(c.name + ' ' + DT[c.k][0] + ' ' + fmtV(val, c.k === 'ang')) + '"' + (c.id === 'pv' ? '' : ' data-tip="' + esc(c.name + ' = ' + fmtV(val, c.k === 'ang') + (c.ref ? ' · tham chiếu' : '')) + '" data-cmd="' + DT[c.k][2] + '" data-desc="' + esc(consDesc(d, c) + '. ' + (TC.can('PRO') ? 'Nhấp đúp để sửa giá trị hoặc biểu thức; kéo để dời đường kích thước; chuột phải để có tùy chọn.' : 'Chỉ xem trong bản này.')) + '"') + '>' + (CS.lock && !c.ref && c.id !== 'pv' ? ic('lock') : '') + '<span>' + esc(txt) + '</span></button>'; });
    // thanh ràng buộc và dấu trùng điểm
    if (main){
      var bars = {}, marks = {}, ents = {}, nb = 0;
      p.list.forEach(function(c){ if (c.t === 'dim' || c.hide || !CS.bar[c.t] || !vis(c)) return;
        if (c.t === 'coin'){ if (entsOf(c).every(function(id){ return p.hide[id] && !(CS.barSel && sel[id]); })) return; var q = ptOf(d, c.a), s = q && v.mToS(q); if (!s) return; var key = Math.round(s[0]) + ',' + Math.round(s[1]); (marks[key] = marks[key] || { s:s, cs:[] }).cs.push(c); return; }
        refsOf(c).forEach(function(r){ var e = d.byId[r.id]; if ((p.hide[r.id] && !(CS.barSel && sel[r.id])) || !e) return; var s = e.t === 'pline' ? (r.s != null ? r.s : r.k && r.k[0] === 'v' ? Math.min(Number(r.k.slice(1)), (e.closed ? e.pts.length : e.pts.length - 1) - 1) : r.k && r.k[0] === 'm' ? Number(r.k.slice(1)) : 0) : 0, key = r.id + ':' + s; var b = bars[key] = bars[key] || { e:e, s:s, cs:[] }; if (b.cs.indexOf(c) < 0) b.cs.push(c); ents[r.id] = 1; });
      });
      var nbr = function(e){ var acc = [0, 0], k = 0; (index(d)[e.id] || []).forEach(function(c){ entsOf(c).forEach(function(id){ var o = d.byId[id]; if (o && o._bb){ acc[0] += (o._bb[0] + o._bb[2]) / 2; acc[1] += (o._bb[1] + o._bb[3]) / 2; k++; } }); }); return k > 1 ? [acc[0] / k, acc[1] / k] : null; };
      var free = function(at, w, along){ for (var t = 0; t < 4; t++){ var hit = placed.some(function(r){ return Math.abs(r[0] - at[0]) < (r[2] + w) / 2 + 3 && Math.abs(r[1] - at[1]) < 21; }); if (!hit) break; at = [at[0] + along[0] * (w + 6), at[1] + along[1] * 22]; } placed.push([at[0], at[1], w]); return at; };
      Object.keys(bars).forEach(function(key){ if (nb > 160) return; var b = bars[key], at = barAt(d, v, b.e, b.s, nbr(b.e)); if (!at || at[0] < -20 || at[1] < -20 || at[0] > v.W + 20 || at[1] > v.H + 20) return; nb++;
        var w = b.cs.length * 16 + 4, sg = b.e.t === 'line' ? sub(v.mToS(b.e.p2), v.mToS(b.e.p1)) : [1, 0], al = Math.abs(sg[0]) >= Math.abs(sg[1]) ? [1, 0] : [0, 1]; at = free(at, w, al);
        html += '<div class="pcb" role="toolbar" aria-label="Ràng buộc của ' + esc(ENAME[b.e.t] || b.e.t) + '" style="left:' + Math.round(at[0]) + 'px;top:' + Math.round(at[1]) + 'px" data-ent="' + b.e.id + '">' + b.cs.map(function(c){ return '<button class="pci" type="button" tabindex="-1" data-c="' + c.id + '" data-ent="' + b.e.id + '" data-tip="' + esc(GT[c.t][0]) + '" data-cmd="' + GT[c.t][2] + '" data-desc="' + esc(consDesc(d, c) + '. Chuột phải: Xóa, Ẩn.') + '" aria-label="' + esc(GT[c.t][0]) + '">' + ic(GT[c.t][1]) + '</button>'; }).join('') + '<button class="pcx" type="button" tabindex="-1" data-hb="' + b.e.id + '" aria-label="Ẩn thanh ràng buộc" data-tip="Ẩn thanh này">' + ic('x') + '</button></div>'; });
      Object.keys(marks).forEach(function(key){ var m = marks[key]; html += '<button class="pcm" type="button" tabindex="-1" data-c="' + m.cs.map(function(c){ return c.id; }).join(',') + '" style="left:' + Math.round(m.s[0]) + 'px;top:' + Math.round(m.s[1]) + 'px" data-tip="' + esc(GT.coin[0] + (m.cs.length > 1 ? ' · ' + m.cs.length : '')) + '" data-cmd="GCCOINCIDENT" data-desc="' + esc(m.cs.map(function(c){ return consDesc(d, c); }).join('; ')) + '" aria-label="Trùng điểm"></button>'; });
      setLayer(html);
    }
    if (main && PK.mark) markPt(ctx, v);
  });
  function markPt(ctx, v){ var s = v.mToS(PK.mark); if (!s) return; ctx.save(); ctx.strokeStyle = TC.cssv('--c-grip-hot'); ctx.lineWidth = 1.6; ctx.beginPath(); ctx.arc(s[0], s[1], 6, 0, 7); ctx.moveTo(s[0] - 4, s[1] - 4); ctx.lineTo(s[0] + 4, s[1] + 4); ctx.moveTo(s[0] + 4, s[1] - 4); ctx.lineTo(s[0] - 4, s[1] + 4); ctx.stroke(); ctx.restore(); }
  TC.on('doc', function(){ HOT = null; PV = null; DRAG = null; });

  // tương tác trên lớp nổi: rê để tô sáng, chuột phải, nhấp đúp sửa giá trị, kéo nhãn để dời đường kích thước
  function bindLayer(el){
    el.addEventListener('mouseover', function(e){ var t = e.target.closest('[data-c]'); if (!t || t.dataset.c === 'pv') return; var d = D(), cs = t.dataset.c.split(',').map(function(id){ return byCid(d, id); }).filter(Boolean); hot(cs); });
    el.addEventListener('mouseout', function(e){ var t = e.target.closest('[data-c]'); if (t && !t.contains(e.relatedTarget) && HOT) hot(null); });
    el.addEventListener('dblclick', function(e){ var t = e.target.closest('.pcd'); if (!t) return; e.stopPropagation(); e.preventDefault(); var d = D(), c = byCid(d, t.dataset.d); if (!c) return; if (!TC.can('PRO')){ SC.upgrade('Sửa tham số ràng buộc', 'PRO'); return; } editLater(d, c); });
    el.addEventListener('contextmenu', function(e){ var t = e.target.closest('[data-c]'); e.preventDefault(); e.stopPropagation(); if (!t || t.dataset.c === 'pv') return; var d = D(); if (t.classList.contains('pcd')) dimMenu(d, byCid(d, t.dataset.d), e.clientX, e.clientY); else iconMenu(d, t.dataset.c.split(','), t.dataset.ent ? Number(t.dataset.ent) : null, e.clientX, e.clientY); });
    el.addEventListener('click', function(e){
      var hb = e.target.closest('[data-hb]'); if (hb){ if (!TC.can('PRO')){ SC.upgrade('Ẩn, hiện thanh ràng buộc', 'PRO'); return; } var d = D(), id = Number(hb.dataset.hb); ED.run('Ẩn thanh ràng buộc', function(){ var done = parState(d); P(d).hide[id] = 1; touch(P(d)); done(); }); hot(null); V().draw(); TC.cmdline.msg('Đã ẩn thanh ràng buộc. Hiện lại: CONSTRAINTBAR › Hiện tất cả.'); return; }
      var t = e.target.closest('.pci, .pcm'); if (t){ var d2 = D(), ids = []; t.dataset.c.split(',').forEach(function(cid){ var c = byCid(d2, cid); if (c) entsOf(c).forEach(function(id){ if (ids.indexOf(id) < 0) ids.push(id); }); }); if (!TC.cmdline.busy()){ d2.sel = ids; TC.cad.afterSel(); } t.focus(); }
    });
    el.addEventListener('keydown', function(e){ var t = e.target.closest('[data-c]'); if (!t) return; if (e.key === 'Delete' && TC.can('PRO') && !t.classList.contains('pcd')){ e.preventDefault(); e.stopPropagation(); var d = D(), cs = t.dataset.c.split(',').map(function(id){ return byCid(d, id); }).filter(Boolean); delCons(d, cs); hot(null); V().draw(); TC.cmdline.msg('Đã xóa ' + cs.length + ' ràng buộc.'); } else if ((e.key === 'Enter' || e.key === 'F2') && t.classList.contains('pcd') && TC.can('PRO')){ e.preventDefault(); e.stopPropagation(); var d3 = D(), c = byCid(d3, t.dataset.d); if (c) editLater(d3, c); } });
    el.addEventListener('mousedown', function(e){
      var t = e.target.closest('.pcd'); if (!t || e.button !== 0 || !TC.can('PRO') || TC.cmdline.busy()) return; var d = D(), c = byCid(d, t.dataset.d); if (!c) return; var st = [e.clientX, e.clientY], moved = false;
      function mv(ev){ if (!moved && Math.hypot(ev.clientX - st[0], ev.clientY - st[1]) < 4) return; moved = true; var r = $('#cv').getBoundingClientRect(), wp = V().sToM(ev.clientX - r.left, ev.clientY - r.top); if (wp){ DRAG = { id:c.id, off:offAt(d, c, wp) }; V().drawOver(); } }
      function up(){ removeEventListener('mousemove', mv); removeEventListener('mouseup', up); var dr = DRAG; DRAG = null; if (moved && dr){ ED.run('Dời ràng buộc kích thước', function(){ var done = parState(d); var cc = byCid(d, dr.id); if (cc){ cc.off = dr.off; touch(P(d)); } done(); }); V().draw(); } }
      addEventListener('mousemove', mv); addEventListener('mouseup', up);
    });
  }
  function iconMenu(d, cids, entId, x, y){
    var cs = cids.map(function(id){ return byCid(d, id); }).filter(Boolean); if (!cs.length) return; var c0 = cs[0], nm = GT[c0.t][0] + (cs.length > 1 ? ' · ' + cs.length : '');
    TC.menu([{ h:nm }, { label:'Xóa', icon:'erase', kbd:'Del', need:'PRO', feature:'Xóa ràng buộc', act:function(){ delCons(d, cs); hot(null); V().draw(); TC.cmdline.msg('Đã xóa ràng buộc ' + esc(nm) + '.'); } },
      { label:'Ẩn', icon:'eyeoff', need:'PRO', feature:'Ẩn ràng buộc', act:function(){ ED.run('Ẩn ràng buộc', function(){ var done = parState(d); P(d).list.forEach(function(c){ if (cids.indexOf(c.id) >= 0) c.hide = true; }); touch(P(d)); done(); }); hot(null); V().draw(); } },
      entId ? { label:'Ẩn mọi ràng buộc của đối tượng', need:'PRO', feature:'Ẩn thanh ràng buộc', act:function(){ ED.run('Ẩn thanh ràng buộc', function(){ var done = parState(d); P(d).hide[entId] = 1; touch(P(d)); done(); }); hot(null); V().draw(); } } : null,
      { sep:1 }, { label:'Hiện tất cả thanh ràng buộc', icon:'gbar', need:'PRO', act:function(){ TC.cmd.run('CONSTRAINTBAR', 'S'); } }, { label:'Thiết lập thanh ràng buộc…', icon:'gear', need:'PRO', act:function(){ TC.cmd.run('CONSTRAINTSETTINGS'); } }].filter(Boolean), x, y);
  }
  function dimMenu(d, c, x, y){
    if (!c) return; var fm = function(k, l){ return { label:l, check:CS.fmt === k, act:function(){ CS.fmt = k; TC.save('cset', CS); V().drawOver(); A().refreshPalettes(); } }; };
    TC.menu([{ h:c.name + ' · ' + DT[c.k][0] + (c.ref ? ' · tham chiếu' : '') }, { label:'Sửa giá trị, biểu thức', icon:'mtext', kbd:'Nhấp đúp', need:'PRO', dis:!!c.ref, act:function(){ editLater(d, c); } },
      { label:c.ref ? 'Chuyển thành ràng buộc chủ động' : 'Chuyển thành tham số tham chiếu', need:'PRO', act:function(){ toggleRef(d, c); } },
      { label:'Ẩn ràng buộc động này', icon:'eyeoff', need:'PRO', act:function(){ ED.run('Ẩn ràng buộc động', function(){ var done = parState(d), cc = byCid(d, c.id); cc.hide = true; touch(P(d)); done(); }); hot(null); V().draw(); } },
      { label:'Xóa ràng buộc', icon:'erase', need:'PRO', act:function(){ removeParam(d, c.id); } }, { sep:1 },
      { label:'Định dạng tên', sub:[fm('ne', 'Tên và biểu thức'), fm('n', 'Tên'), fm('v', 'Giá trị')] }, { label:'Ẩn mọi ràng buộc động', icon:'dcshow', need:'PRO', act:function(){ TC.cmd.run('DCDISPLAY', 'H'); } }, { label:'Trình quản lý tham số', icon:'params', act:function(){ A().showPalette('params'); PM.sel = c.id; A().refreshPalettes(); } }], x, y);
  }
  function toggleRef(d, c){
    if (!c.ref){ ED.run('Chuyển thành tham chiếu', function(){ var done = parState(d), cc = byCid(d, c.id); var users = usersOf(P(d), cc.name); cc.ref = true; cc.expr = ''; touch(P(d)); done(); if (users.length) TC.cmdline.msg(users.map(function(u){ return esc(u.name); }).join(', ') + ' vẫn đọc giá trị đo của ' + esc(cc.name) + '.'); }); V().draw(); return; }
    var nc = JSON.parse(JSON.stringify(c)); delete nc.id; delete nc.ref; nc.val = measure(d, c); nc.expr = numStr(nc.val);
    var p = P(d), rest = p.list.filter(function(x){ return x.id !== c.id; }), S = sys(d, compOf(d, entsOf(c), rest).concat([nc])), r = solve(S, []);
    if (r.fail || redundant(S, r.x)){ TC.toast('<b>' + esc(c.name) + '</b> không chuyển thành ràng buộc chủ động được: hình đã được xác định bởi các ràng buộc khác. Bỏ bớt một ràng buộc trước.', { type:'warn', log:false }); return; }
    ED.run('Chuyển thành ràng buộc chủ động', function(){ var done = parState(d), cc = byCid(d, c.id); delete cc.ref; cc.val = nc.val; cc.expr = nc.expr; touch(P(d)); done(); }); V().draw();
  }
  function splitNE(v, def){ v = String(v).trim(); var i = v.indexOf('='); return i > 0 ? [v.slice(0, i).trim(), v.slice(i + 1).trim()] : [def, v]; }
  function editLater(d, c){ if (TC.cmdline.busy()) TC.cmd.cancel(); setTimeout(function(){ editor(d, c); }, 0); }
  // ô sửa tại chỗ trên nhãn ràng buộc động: "tên=biểu thức"; Enter áp dụng (một bước hoàn tác), Esc bỏ
  function editor(d, c){
    var cv = $('#cv'), old = $('.pc-ed', cv); if (old) old.remove();
    var lab = LAY.el && $('.pcd[data-d="' + c.id + '"]', LAY.el), x = lab ? lab.offsetLeft : (V().mouse || [200, 200])[0], y = lab ? lab.offsetTop : (V().mouse || [200, 200])[1], closed = false, res;
    var box = TC.el('<div class="pc-ed" role="group" aria-label="Sửa ' + esc(c.name) + '"><input class="inp sm" spellcheck="false" autocomplete="off" aria-label="Tên và giá trị hoặc biểu thức" value="' + esc(c.name + '=' + (c.ref ? numStr(measure(d, c)) : c.expr)) + '"><span class="fmsg err" hidden></span></div>');
    box.style.left = x + 'px'; box.style.top = y + 'px'; cv.appendChild(box);
    var inp = $('input', box), msg = $('.fmsg', box), done = new Promise(function(r){ res = r; });
    setTimeout(function(){ inp.focus(); inp.setSelectionRange(c.name.length + 1, inp.value.length); }, 0);
    function close(v){ if (closed) return; closed = true; box.remove(); res(v); }
    function apply(){ var ne = splitNE(inp.value, c.name); if (ne[0] === c.name && ne[1] === c.expr){ close(null); return; } var r = setParam(d, c.id, ne[0], ne[1], 'Sửa ' + c.name); if (r.err){ msg.hidden = false; msg.innerHTML = ic('warn', 'xs') + esc(r.err); inp.classList.add('err'); inp.focus(); return false; } close(true); A().refreshPalettes(); return true; }
    inp.addEventListener('keydown', function(e){ if (e.key === 'Enter'){ e.preventDefault(); e.stopPropagation(); apply(); } else if (e.key === 'Escape'){ e.preventDefault(); e.stopPropagation(); close(null); } });
    inp.addEventListener('input', function(){ inp.classList.remove('err'); msg.hidden = true; });
    inp.addEventListener('blur', function(){ setTimeout(function(){ if (!closed && document.activeElement !== inp){ if (apply() === false) close(null); } }, 120); });
    return { done:done, close:function(){ close(null); } };
  }
  function removeParam(d, key){
    var p = P(d), it = items(p).filter(function(x){ return x.key === key; })[0]; if (!it) return;
    var users = usersOf(p, it.name).filter(function(u){ return u.key !== key; }), ev = evalAll(d, p);
    var go = function(){ if (it.dim) delCons(d, [it.o]); else ED.run('Xóa biến người dùng', function(){ var done = parState(d), vals = {}; vals[it.name.toLowerCase()] = ev.v[key]; p.users = p.users.filter(function(u){ return u.id !== key; }); inlineRefs(p, vals); touch(p); done(); }); if (PM.sel === key) PM.sel = null; V().draw(); A().refreshPalettes(); TC.cmdline.msg('Đã xóa tham số <b>' + esc(it.name) + '</b>.'); };
    if (!users.length) return go();
    TC.confirm({ title:'Xóa tham số ' + esc(it.name), msg:'<b>' + esc(it.name) + '</b> đang dùng trong biểu thức của ' + users.length + ' tham số: ' + users.map(function(u){ return '<b>' + esc(u.name) + '</b>'; }).join(', ') + '.', detail:'Các biểu thức đó sẽ thay ' + esc(it.name) + ' bằng giá trị hiện tại ' + fmtV(ev.v[key]) + '. Hoàn tác được bằng Ctrl+Z.', ok:'Xóa và thay bằng ' + fmtV(ev.v[key]), danger:true }).then(function(y){ if (y) go(); });
  }

  /* ================= palette Trình quản lý tham số (PARAMETERS) ================= */
  var PM = { q:'', f:'all', sort:null, dir:1, open:{ dim:true, user:true }, sel:null, edit:null, err:null };
  var KIND = { h:'Ngang', v:'Đứng', al:'Nghiêng', rad:'Bán kính', dia:'Đường kính', ang:'Góc' };
  TC.PALETTES.params = { name:'Tham số', render:function(host, d){
    if (!d){ host.innerHTML = '<div class="empty">' + ic('params') + '<b>Chưa mở bản vẽ</b>Mở một bản vẽ để xem tham số.</div>'; return; }
    var ae = document.activeElement; if (!PM.force && PM.edit && host.contains(ae) && ae.tagName === 'INPUT' && ae.closest('.pm-t')) return; // đang sửa trong bảng: không vẽ lại khi bản vẽ đổi
    PM.force = false; var rnd = function(){ PM.force = true; TC.PALETTES.params.render(host, d); };
    var ro = !TC.can('PRO'), p = P(d), ev = evalAll(d, p), F = TC.fold(PM.q), all = ev.items, used = {};
    all.forEach(function(it){ if (!it.ref) (it.expr.match(IDRE) || []).forEach(function(m){ used[m.toLowerCase()] = 1; }); });
    var shown = all.filter(function(it){ return (!F || TC.fold(it.name + ' ' + it.expr).indexOf(F) >= 0) && (PM.f === 'all' || (PM.f === 'dim' && it.dim) || (PM.f === 'user' && !it.dim) || (PM.f === 'err' && ev.err[it.key]) || (PM.f === 'used' && used[it.name.toLowerCase()])); });
    if (PM.sort) shown.sort(function(a, b){ var x = PM.sort === 'val' ? ev.v[a.key] - ev.v[b.key] : a.name.localeCompare(b.name, 'vi', { numeric:true }); return x * PM.dir; });
    var th = function(k, l, cls){ return '<th' + (cls ? ' class="' + cls + '"' : '') + '>' + (k ? '<button class="srt" type="button" data-ps="' + k + '">' + l + (PM.sort === k ? ic(PM.dir > 0 ? 'down' : 'up', 'xs') : '') + '</button>' : l) + '</th>'; };
    var row = function(it){
      var c = it.o, val = ev.v[it.key], er = ev.err[it.key], ed = PM.edit && PM.edit.key === it.key ? PM.edit.f : null, ang = it.dim && c.k === 'ang';
      var cell = function(f, txt){ return ed === f ? '<td class="ed"><input class="inp sm" data-pe="' + f + '" value="' + esc(PM.err && PM.err.key === it.key && PM.err.f === f ? PM.err.val : f === 'name' ? it.name : it.expr) + '" spellcheck="false" autocomplete="off" aria-label="' + (f === 'name' ? 'Tên' : 'Biểu thức') + ' của ' + esc(it.name) + '"></td>' : txt; };
      return '<tr data-pk="' + it.key + '" class="' + (PM.sel === it.key ? 'is-sel' : '') + (it.ref ? ' is-ref' : '') + '" tabindex="' + (PM.sel === it.key ? 0 : -1) + '" title="' + esc(it.dim ? consDesc(d, c) : 'Biến người dùng') + '">' +
        cell('name', '<td data-pf="name"><span class="pm-n">' + ic(it.dim ? DT[c.k][1] : 'params', 's12') + '<span class="mono">' + esc(it.name) + '</span></span></td>') +
        cell('expr', '<td data-pf="expr" class="mono' + (er ? ' pm-bad' : '') + '">' + (it.ref ? '<span class="faint">(tham chiếu)</span>' : esc(it.expr)) + '</td>') +
        '<td class="num">' + (er ? '<span class="pm-bad" title="' + esc(er) + '">' + ic('warn', 'xs') + '</span>' : it.ref ? '(' + fmtV(val, ang) + ')' : fmtV(val, ang)) + '</td><td class="faint">' + (it.dim ? KIND[c.k] : 'Biến') + '</td></tr>' +
        (PM.err && PM.err.key === it.key ? '<tr class="pm-err"><td colspan="4">' + ic('warn', 'xs') + ' ' + esc(PM.err.msg) + '</td></tr>' : er ? '<tr class="pm-err"><td colspan="4">' + ic('warn', 'xs') + ' ' + esc(er) + '</td></tr>' : '');
    };
    var grp = function(k, label, list, n){ return '<tr class="pm-g"><td colspan="4"><button class="pgh" type="button" data-pg="' + k + '" aria-expanded="' + !!PM.open[k] + '">' + ic('down', 'xs') + label + '<span class="faint mono" style="margin-left:auto">' + n + '</span></button></td></tr>' + (PM.open[k] ? list.map(row).join('') : ''); };
    var dims = shown.filter(function(it){ return it.dim; }), us = shown.filter(function(it){ return !it.dim; }), nerr = Object.keys(ev.err).length;
    var body = !all.length ? '<div class="empty">' + ic('params') + '<b>Bản vẽ chưa có tham số</b>Ràng buộc kích thước (DCLINEAR, DCRADIUS…) tạo tham số d1, rad1…; biến người dùng dùng chung trong biểu thức.' + (ro ? '' : '<span class="hrow" style="justify-content:center;margin-top:4px"><button class="btn sm" type="button" data-pa="dc">' + ic('dclin') + 'Ràng buộc kích thước</button><button class="btn sm" type="button" data-pa="new">' + ic('plus') + 'Biến mới</button></span>') + '</div>'
      : !shown.length ? '<div class="empty">Không có tham số khớp bộ lọc.<button class="btn sm" type="button" data-pa="clr">Bỏ lọc</button></div>'
      : '<table class="tbl pm-t" role="grid" aria-label="Tham số"><colgroup><col style="width:25%"><col style="width:28%"><col style="width:19%"><col style="width:28%"></colgroup><thead><tr>' + th('name', 'Tên') + '<th>Biểu thức</th>' + th('val', 'Giá trị', 'num') + '<th>Kiểu</th></tr></thead><tbody>' +
        (dims.length || PM.f === 'dim' ? grp('dim', 'Ràng buộc kích thước', dims, dims.length) : '') + (us.length || PM.f === 'user' ? grp('user', 'Biến người dùng', us, us.length) : '') + '</tbody></table>';
    var FL = [['all', 'Tất cả'], ['dim', 'Ràng buộc kích thước'], ['user', 'Biến người dùng'], ['used', 'Được dùng trong biểu thức'], ['err', 'Đang lỗi']];
    host.innerHTML = '<div class="pm">' + (ro ? '<div class="pal-note info">' + ic('lock') + '<span class="grow">Chỉ xem. Sửa tham số có trong TrionCAD Pro.</span><button class="btn sm" type="button" data-pa="up">Dùng thử</button></div>' : '') +
      '<div class="pal-tools"><button class="ib" type="button" data-pa="new" aria-label="Biến người dùng mới" data-tip="Biến người dùng mới"' + (ro ? ' disabled' : '') + '>' + ic('plus') + '</button><button class="ib" type="button" data-pa="del" aria-label="Xóa tham số" data-tip="Xóa tham số" data-key="Del"' + (ro || !PM.sel ? ' disabled' : '') + '>' + ic('trash') + '</button>' +
      '<label class="search grow">' + ic('search') + '<input data-pq placeholder="Lọc theo tên hoặc biểu thức" value="' + esc(PM.q) + '" aria-label="Lọc tham số"></label><button class="ib' + (PM.f !== 'all' ? ' is-on' : '') + '" type="button" data-pa="flt" aria-haspopup="menu" aria-label="Bộ lọc: ' + FL.filter(function(x){ return x[0] === PM.f; })[0][1] + '" data-tip="Bộ lọc">' + ic('filter') + '</button><button class="ib" type="button" data-pa="set" aria-label="Thiết lập ràng buộc" data-tip="Thiết lập ràng buộc" data-cmd="CONSTRAINTSETTINGS">' + ic('gear') + '</button></div>' +
      '<div class="pal-body">' + body + '</div>' +
      '<div class="pal-foot"><span>' + items(p).filter(function(x){ return x.dim; }).length + ' kích thước · ' + p.users.length + ' biến' + (nerr ? ' · <span class="pm-bad">' + nerr + ' lỗi</span>' : '') + '</span><span class="grow"></span><span>' + (ro ? '' : 'Nhấp đúp để sửa · Enter áp dụng') + '</span></div></div>';
    var inpE = $('[data-pe]', host); if (inpE){ setTimeout(function(){ inpE.focus(); if (inpE.dataset.pe === 'expr' || !PM.err) inpE.select(); }, 0); }
    else if (PM.kb){ var sr = $('tr.is-sel', host); if (sr) sr.focus(); PM.kb = 0; }
    var q = $('[data-pq]', host); q.addEventListener('input', function(){ PM.q = q.value; var pos = q.selectionStart; rnd(); var q2 = $('[data-pq]', host); q2.focus(); q2.setSelectionRange(pos, pos); });
    var start = function(key, f){ if (ro || TC.cmdline.busy()) return; var it = all.filter(function(x){ return x.key === key; })[0]; if (!it) return; if (f === 'expr' && it.ref){ TC.cmdline.msg('Tham số tham chiếu ' + esc(it.name) + ' chỉ đo, không sửa giá trị. Chuột phải nhãn trên bản vẽ › Chuyển thành ràng buộc chủ động.'); f = 'name'; } PM.sel = key; PM.edit = { key:key, f:f }; PM.err = null; rnd(); };
    var commit = function(inp, next){ var it = all.filter(function(x){ return x.key === PM.edit.key; })[0], f = inp.dataset.pe, v = inp.value.trim(); if (!it) return; var nm = f === 'name' ? v : it.name, ex = f === 'expr' ? v : it.expr;
      if (nm === it.name && (it.ref || ex === it.expr)){ PM.edit = next ? { key:it.key, f:next } : null; PM.err = null; rnd(); return; }
      var r = setParam(d, it.key, nm, it.ref ? '' : ex, 'Sửa tham số ' + it.name); if (r.err){ PM.err = { key:it.key, f:f, msg:r.err, val:inp.value }; rnd(); return; }
      PM.err = null; PM.edit = next ? { key:it.key, f:next } : null; PM.kb = 1; rnd(); };
    if (inpE){ inpE.addEventListener('keydown', function(e){ if (e.key === 'Enter'){ e.preventDefault(); e.stopPropagation(); commit(inpE); } else if (e.key === 'Escape'){ e.preventDefault(); e.stopPropagation(); PM.edit = null; PM.err = null; PM.kb = 1; rnd(); } else if (e.key === 'Tab'){ e.preventDefault(); commit(inpE, inpE.dataset.pe === 'name' ? 'expr' : 'name'); } });
      inpE.addEventListener('blur', function(){ setTimeout(function(){ if (PM.edit && document.contains(inpE) && document.activeElement !== inpE && !PM.err) commit(inpE); }, 150); }); }
    host.onclick = function(e){
      var g = e.target.closest('[data-pg]'); if (g){ PM.open[g.dataset.pg] = !PM.open[g.dataset.pg]; rnd(); return; }
      var s = e.target.closest('[data-ps]'); if (s){ if (PM.sort === s.dataset.ps) PM.dir = -PM.dir; else { PM.sort = s.dataset.ps; PM.dir = 1; } rnd(); return; }
      var b = e.target.closest('[data-pa]'); if (b){ var k = b.dataset.pa, r = b.getBoundingClientRect();
        if (k === 'up') SC.upgrade('Trình quản lý tham số', 'PRO');
        else if (k === 'new') newUser(d);
        else if (k === 'del' && PM.sel) removeParam(d, PM.sel);
        else if (k === 'dc') TC.cmd.run('DCLINEAR');
        else if (k === 'set') TC.cmd.run('CONSTRAINTSETTINGS', 'dim');
        else if (k === 'clr'){ PM.q = ''; PM.f = 'all'; rnd(); }
        else if (k === 'flt') TC.menu(FL.map(function(x){ return { label:x[1], check:PM.f === x[0], act:function(){ PM.f = x[0]; rnd(); } }; }), r.right, r.bottom + 2, { alignRight:true, returnFocus:b });
        return; }
      var tr = e.target.closest('tr[data-pk]'); if (tr && !e.target.closest('input')){ PM.sel = tr.dataset.pk; var c = byCid(d, PM.sel); hot(c ? [c] : null); rnd(); var t2 = $('tr[data-pk="' + PM.sel + '"]', host); if (t2) t2.focus(); }
    };
    host.ondblclick = function(e){ var td = e.target.closest('td[data-pf]'), tr = e.target.closest('tr[data-pk]'); if (!td || !tr) return; if (ro){ SC.upgrade('Sửa tham số ràng buộc', 'PRO'); return; } start(tr.dataset.pk, td.dataset.pf); };
    host.onkeydown = function(e){
      if (e.target.closest('input')) return; var tr = e.target.closest('tr[data-pk]'); if (!tr) return; var rows = $$('tr[data-pk]', host), i = rows.indexOf(tr);
      if (e.key === 'ArrowDown' || e.key === 'ArrowUp'){ e.preventDefault(); var n = rows[TC.clamp(i + (e.key === 'ArrowDown' ? 1 : -1), 0, rows.length - 1)]; PM.sel = n.dataset.pk; PM.kb = 1; var c = byCid(d, PM.sel); hot(c ? [c] : null); rnd(); }
      else if ((e.key === 'Enter' || e.key === 'F2') && !ro){ e.preventDefault(); e.stopPropagation(); start(tr.dataset.pk, 'expr'); }
      else if (e.key === 'Delete' && !ro){ e.preventDefault(); e.stopPropagation(); removeParam(d, tr.dataset.pk); }
      else if (e.key === 'Escape'){ PM.sel = null; hot(null); rnd(); }
    };
    host.onmouseover = function(e){ var tr = e.target.closest('tr[data-pk]'); var c = tr && byCid(d, tr.dataset.pk); if (c && (!HOT || HOT.ids[0] !== c.id)) hot([c]); };
    host.onmouseleave = function(){ if (HOT){ var c = PM.sel && byCid(d, PM.sel); hot(c ? [c] : null); } };
  } };
  function newUser(d){
    if (!TC.can('PRO')) return SC.upgrade('Biến người dùng', 'PRO');
    var p = P(d), name = nextName(p, 'user'), id;
    ED.run('Biến người dùng mới', function(){ var done = parState(d); id = 'u' + (++p.n); p.users.push({ id:id, name:name, expr:'1' }); touch(p); done(); });
    PM.open.user = true; PM.sel = id; PM.edit = { key:id, f:'name' }; PM.q = ''; PM.f = 'all'; A().showPalette('params'); A().refreshPalettes();
  }

  /* ================= hộp Thiết lập ràng buộc (CONSTRAINTSETTINGS) ================= */
  function settingsDlg(tab){
    var s = JSON.parse(JSON.stringify(CS)), selP = s.prio[0];
    var geo = '<div class="stack">' + H.grp('Loại ràng buộc hiện trên thanh', '<div class="pcs-types">' + GORD.map(function(k){ return '<label class="chk"><input type="checkbox" data-bt="' + k + '"' + (s.bar[k] ? ' checked' : '') + '> ' + ic(GT[k][1]) + ' ' + GT[k][0] + '</label>'; }).join('') + '</div><div class="hrow"><button class="btn sm" type="button" data-ball="1">Chọn tất cả</button><button class="btn sm" type="button" data-ball="0">Bỏ tất cả</button></div>') +
      H.grp('Thanh ràng buộc', H.row('Độ trong suốt', '<input type="range" id="cs-tr" min="0" max="90" step="5" value="' + s.transp + '" aria-label="Độ trong suốt thanh ràng buộc" style="width:180px;accent-color:var(--c-accent)"><span class="mono" id="cs-trv" style="width:36px">' + s.transp + '%</span>', 130) +
        '<div class="pcs-pv" aria-hidden="true"><span class="pcb" id="cs-pvb" style="position:static;transform:none;opacity:' + (1 - s.transp / 100) + '"><span class="pci">' + ic('gpar') + '</span><span class="pci">' + ic('gperp') + '</span><span class="pci">' + ic('ghor') + '</span></span><span class="faint">Thanh mờ khi không rê chuột, rõ khi rê</span></div>' +
        H.chk('cs-after', 'Hiện thanh sau khi gắn ràng buộc cho đối tượng', s.barAfter) + H.chk('cs-sel', 'Hiện thanh khi chọn đối tượng (kể cả thanh đang ẩn)', s.barSel)) + '</div>';
    var fmts = [['ne', 'Tên và biểu thức'], ['n', 'Tên'], ['v', 'Giá trị']];
    var dim = '<div class="stack">' + H.grp('Ràng buộc kích thước', H.row('Định dạng tên', '<select class="inp" id="cs-fmt" style="width:190px">' + fmts.map(function(f){ return '<option value="' + f[0] + '"' + (s.fmt === f[0] ? ' selected' : '') + '>' + f[1] + '</option>'; }).join('') + '</select>', 130) +
        H.row('Xem trước', '<span class="pcd" id="cs-pvd" style="position:static;transform:none"></span>', 130) + H.chk('cs-lock', 'Hiện biểu tượng khóa cho ràng buộc chủ động', s.lock) + H.chk('cs-dsel', 'Hiện ràng buộc động đang ẩn khi chọn đối tượng', s.dynSel)) +
      '<div class="note">' + ic('info') + '<span>Ràng buộc động không in và không xuất PDF. Tham số tham chiếu hiện trong ngoặc.</span></div></div>';
    var auto = '<div class="stack"><div class="pcs-a"><div><div class="gh" style="margin-bottom:6px">Thứ tự ưu tiên</div><div class="pcs-pri" role="listbox" tabindex="0" aria-label="Thứ tự ưu tiên tự ràng buộc"></div></div><div class="stack" style="gap:6px;align-content:start;padding-top:20px"><button class="btn sm" type="button" data-pm="-1">' + ic('up') + 'Lên</button><button class="btn sm" type="button" data-pm="1">' + ic('down') + 'Xuống</button><button class="btn sm" type="button" data-pm="all">Chọn tất cả</button><button class="btn sm" type="button" data-pm="none">Bỏ tất cả</button><button class="btn sm ghost" type="button" data-pm="reset">Đặt lại</button></div></div>' +
      H.chk('cs-tan', 'Đối tượng tiếp xúc phải có chung điểm giao', s.tanInt) + H.chk('cs-perp', 'Đối tượng vuông góc phải có chung điểm giao', s.perpInt) +
      H.grp('Dung sai', H.row('Khoảng cách', H.num('cs-td', TC.d(s.tolD, 2), { w:110, unit:'len', step:.01, prec:2 }), 130) + H.row('Góc', H.num('cs-ta', TC.d(s.tolA, 1), { w:110, unit:'deg', step:.5, prec:1 }) + '<span class="faint">°</span>', 130)) + '</div>';
    var dlg = TC.dialog({ id:'cset', title:'Thiết lập ràng buộc', cmd:'CONSTRAINTSETTINGS', code:'UI-PAR', w:560, h:560,
      body:'<div class="pcs">' + H.tabs([['g', 'Hình học'], ['d', 'Kích thước'], ['a', 'Tự ràng buộc']], tab) + H.pane('g', tab, geo) + H.pane('d', tab, dim) + H.pane('a', tab, auto) + '</div>',
      foot:'<button class="btn ghost" type="button" id="cs-def">Đặt lại mặc định</button><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="cs-ok">Áp dụng thiết lập</button>' });
    var $d = function(q){ return $(q, dlg.el); };
    function pri(){ $d('.pcs-pri').innerHTML = s.prio.map(function(k){ return '<div role="option" data-pp="' + k + '" aria-selected="' + (k === selP) + '" class="' + (k === selP ? 'is-sel' : '') + '"><input type="checkbox" data-pon="' + k + '"' + (s.off[k] ? '' : ' checked') + ' aria-label="Dùng ' + GT[k][0] + '">' + ic(GT[k][1]) + '<span>' + GT[k][0] + '</span></div>'; }).join(''); }
    function pvd(){ $d('#cs-pvd').innerHTML = (s.lock ? ic('lock') : '') + '<span>' + esc(labelOf({ name:'d2', expr:'d1/2', val:300, k:'h' }, 300, s.fmt)) + '</span>'; }
    pri(); pvd();
    dlg.el.addEventListener('input', function(e){ if (e.target.id === 'cs-tr'){ s.transp = Number(e.target.value); $d('#cs-trv').textContent = s.transp + '%'; $d('#cs-pvb').style.opacity = String(1 - s.transp / 100); } });
    dlg.el.addEventListener('change', function(e){ var t = e.target; if (t.dataset.bt) s.bar[t.dataset.bt] = t.checked; else if (t.dataset.pon){ if (t.checked) delete s.off[t.dataset.pon]; else s.off[t.dataset.pon] = 1; } else if (t.id === 'cs-fmt'){ s.fmt = t.value; pvd(); } else if (t.id === 'cs-lock'){ s.lock = t.checked; pvd(); } else if (t.id === 'cs-after') s.barAfter = t.checked; else if (t.id === 'cs-sel') s.barSel = t.checked; else if (t.id === 'cs-dsel') s.dynSel = t.checked; else if (t.id === 'cs-tan') s.tanInt = t.checked; else if (t.id === 'cs-perp') s.perpInt = t.checked; });
    function move(k){ var i = s.prio.indexOf(selP), j = i + k; if (j < 0 || j >= s.prio.length) return; s.prio.splice(i, 1); s.prio.splice(j, 0, selP); pri(); }
    dlg.el.addEventListener('click', function(e){
      var ba = e.target.closest('[data-ball]'); if (ba){ GORD.forEach(function(k){ s.bar[k] = ba.dataset.ball === '1'; }); $$('[data-bt]', dlg.el).forEach(function(c){ c.checked = s.bar[c.dataset.bt]; }); return; }
      var pp = e.target.closest('[data-pp]'); if (pp && !e.target.closest('input')){ selP = pp.dataset.pp; pri(); return; }
      var pm = e.target.closest('[data-pm]'); if (pm){ var k = pm.dataset.pm; if (k === 'all') s.off = {}; else if (k === 'none') s.prio.forEach(function(x){ s.off[x] = 1; }); else if (k === 'reset'){ s.prio = CS0.prio.slice(); s.off = {}; } else move(Number(k)); pri(); }
    });
    $d('.pcs-pri').addEventListener('keydown', function(e){ var i = s.prio.indexOf(selP); if (e.key === 'ArrowDown' || e.key === 'ArrowUp'){ e.preventDefault(); if (e.altKey) move(e.key === 'ArrowDown' ? 1 : -1); else { selP = s.prio[TC.clamp(i + (e.key === 'ArrowDown' ? 1 : -1), 0, s.prio.length - 1)]; pri(); } } else if (e.key === ' '){ e.preventDefault(); if (s.off[selP]) delete s.off[selP]; else s.off[selP] = 1; pri(); } });
    $d('#cs-def').addEventListener('click', function(){ var t = $('[data-dtabs] [aria-selected="true"]', dlg.el), cur = t ? t.dataset.dt : 'g'; dlg.close(); CS = JSON.parse(JSON.stringify(CS0)); GORD.forEach(function(k){ CS.bar[k] = true; }); TC.save('cset', CS); V().draw(); A().refreshPalettes(); settingsDlg(cur); TC.toast('Đã đặt lại thiết lập ràng buộc mặc định.', { log:false, ms:2500 }); });
    $d('#cs-ok').addEventListener('click', function(){
      var td = TC.parseNum($d('#cs-td').value, 'len'), ta = TC.parseNum($d('#cs-ta').value, 'deg');
      if (td.err || !(td.v >= 0) || td.v > 1000){ $d('[data-dt="a"]').click(); $d('#cs-td').closest('.numf').classList.add('err'); $d('#cs-td-msg').className = 'fmsg err'; $d('#cs-td-msg').innerHTML = ic('warn', 'xs') + 'Dung sai khoảng cách từ 0 đến 1000.'; return; }
      if (ta.err || !(ta.v >= 0) || ta.v > 45){ $d('[data-dt="a"]').click(); $d('#cs-ta').closest('.numf').classList.add('err'); $d('#cs-ta-msg').className = 'fmsg err'; $d('#cs-ta-msg').innerHTML = ic('warn', 'xs') + 'Dung sai góc từ 0° đến 45°.'; return; }
      s.tolD = td.v; s.tolA = ta.v; CS = s; TC.save('cset', CS); dlg.close(true); V().draw(); A().refreshPalettes(); TC.cmdline.msg('Đã áp dụng thiết lập ràng buộc.');
    });
  }

  /* ================= chuột phải: vùng chọn, nút Suy luận ràng buộc ================= */
  TC.cad.ctxExt.push(function(it, first, d){
    if (TC.isViewer() || !canPar(d)) return; var ix = d.par ? index(d) : {}, n = d.sel.filter(function(id){ return ix[id]; }).length, nc = d.par ? P(d).list.filter(function(c){ return entsOf(c).some(function(id){ return d.sel.indexOf(id) >= 0; }); }).length : 0;
    it.push({ label:'Ràng buộc', icon:'gcoin', sub:[{ label:'Tự ràng buộc', icon:'gauto', act:function(){ TC.cmd.run('AUTOCONSTRAIN'); } }, { label:'Hiện thanh ràng buộc', icon:'gbar', dis:!n, act:function(){ TC.cmd.run('CONSTRAINTBAR'); setTimeout(function(){ TC.cmdline.submit('S'); }, 0); } }, { label:'Ẩn thanh ràng buộc', dis:!n, act:function(){ TC.cmd.run('CONSTRAINTBAR'); setTimeout(function(){ TC.cmdline.submit('H'); }, 0); } }, { sep:1 }, { label:'Xóa ràng buộc' + (nc ? ' (' + nc + ')' : ''), icon:'gdel', dis:!nc, act:function(){ TC.cmd.run('DELCONSTRAINT'); } }] });
  });
  document.addEventListener('contextmenu', function(e){
    var t = e.target.closest && e.target.closest('#sb [data-tgl="infer"]'); if (!t) return; e.preventDefault(); e.stopPropagation();
    TC.menu([{ label:'Suy luận ràng buộc', check:!!TC.S.toggles.infer, act:function(){ A().toggle('infer'); } }, { sep:1 }, { label:'Thiết lập ràng buộc…', icon:'gear', need:'PRO', act:function(){ TC.cmd.run('CONSTRAINTSETTINGS'); } }], e.clientX, e.clientY, { up:true });
  }, true);

  /* ================= danh mục: chi tiết mẫu bản mã BM1 trong Model của Banve-01 (UI-PAR) ================= */
  var DEMO_AT = [64000, 3000];
  function demo(d){
    var o = DEMO_AT, X = function(x, y){ return [o[0] + x, o[1] + y]; }, old = d.ents.filter(function(e){ return e.demo === 'PAR'; }), ids = old.map(function(e){ return e.id; }), bb;
    inSolve++;
    try {
      ED.run('Chi tiết mẫu BM1', function(){
        var done = parState(d), p = P(d);
        p.list = p.list.filter(function(c){ return !entsOf(c).some(function(id){ return ids.indexOf(id) >= 0; }); }); p.users = p.users.filter(function(u){ return !/^mep$/i.test(u.name); });
        old.forEach(function(e){ ED.del(e); });
        var add = function(o2){ return ED.add(ED.make(Object.assign({ layer:'KC-COT', demo:'PAR' }, o2))); };
        var L1 = add({ t:'line', p1:X(0, 0), p2:X(600, 0) }), L2 = add({ t:'line', p1:X(600, 0), p2:X(600, 300) }), L3 = add({ t:'line', p1:X(600, 300), p2:X(0, 300) }), L4 = add({ t:'line', p1:X(0, 300), p2:X(0, 0) });
        var AX = add({ t:'line', p1:X(300, 0), p2:X(300, 300), layer:'KT-TRUC' }), AY = add({ t:'line', p1:X(0, 150), p2:X(600, 150), layer:'KT-TRUC' });
        var C = [[60, 60], [540, 60], [540, 240], [60, 240]].map(function(q){ return add({ t:'circle', c:X(q[0], q[1]), r:11 }); });
        add({ t:'text', p:X(0, -270), h:22, s:'BẢN MÃ BM1 · DÀY 20 · 4 LỖ BU LÔNG M20', style:'Standard', layer:'KC-TEXT' });
        var pt = function(e, k){ return { id:e.id, k:k }; }, id = function(e){ return { id:e.id }; }, cs = [];
        cs.push({ t:'coin', a:pt(L1, 'p2'), b:pt(L2, 'p1') }, { t:'coin', a:pt(L2, 'p2'), b:pt(L3, 'p1') }, { t:'coin', a:pt(L3, 'p2'), b:pt(L4, 'p1') }, { t:'coin', a:pt(L4, 'p2'), b:pt(L1, 'p1') });
        cs.push({ t:'hor', a:id(L1) }, { t:'par', a:id(L1), b:id(L3) }, { t:'perp', a:id(L1), b:id(L2) }, { t:'par', a:id(L2), b:id(L4) }, { t:'fix', a:pt(L1, 'p1'), v:X(0, 0) });
        cs.push({ t:'coin', a:pt(AX, 'p1'), b:pt(L1, 'mid') }, { t:'coin', a:pt(AX, 'p2'), b:pt(L3, 'mid') }, { t:'coin', a:pt(AY, 'p1'), b:pt(L4, 'mid') }, { t:'coin', a:pt(AY, 'p2'), b:pt(L2, 'mid') });
        cs.push({ t:'equal', a:id(C[0]), b:id(C[1]) }, { t:'equal', a:id(C[0]), b:id(C[2]) }, { t:'equal', a:id(C[0]), b:id(C[3]) });
        cs.push({ t:'sym', a:pt(C[0], 'c'), b:pt(C[1], 'c'), c:id(AX) }, { t:'sym', a:pt(C[3], 'c'), b:pt(C[2], 'c'), c:id(AX) }, { t:'sym', a:pt(C[0], 'c'), b:pt(C[3], 'c'), c:id(AY) });
        p.users.push({ id:'u' + (++p.n), name:'mep', expr:'60' });
        cs.forEach(function(c){ c.id = 'c' + (++p.n); p.list.push(c); });
        var dm = function(o3, ex){ var c = Object.assign({ t:'dim', id:'c' + (++p.n), expr:ex }, o3); c.name = nextName(p, DT[c.k][3]); c.val = measure(d, c); p.list.push(c); };
        dm({ k:'h', a:pt(L1, 'p1'), b:pt(L1, 'p2'), s:1, off:-150 }, '600'); dm({ k:'v', a:pt(L2, 'p1'), b:pt(L2, 'p2'), s:1, off:110 }, 'd1/2');
        dm({ k:'h', a:pt(L1, 'p1'), b:pt(C[0], 'c'), s:1, off:-80 }, 'mep'); dm({ k:'v', a:pt(L1, 'p1'), b:pt(C[0], 'c'), s:1, off:-90 }, 'mep'); dm({ k:'dia', a:id(C[0]), off:315 }, '22'); p.dyn = true;
        // các hình trên vẽ đúng sẵn; giải lại để mọi ràng buộc khớp tuyệt đối
        var S = sys(d, compOf(d, [L1.id], p.list.concat([]))), r = solve(S, []); touch(p); done(); if (!r.fail) applySol(S, r.x);
        bb = [o[0] - 220, o[1] - 290, o[0] + 780, o[1] + 360];
      });
    } finally { inSolve--; }
    return bb;
  }
  OPEN['UI-PAR'] = function(){
    var d = SC.cat.MOD(); if (TC.cmdline.busy()) TC.cmd.cancel(); TC.cad.clearSel(); var bb = demo(d);
    A().showPalette('params'); PM.sel = null; PM.edit = null; PM.err = null; PM.q = ''; PM.f = 'all';
    setTimeout(function(){
      var t = $$('.rb-tab').filter(function(b){ return b.textContent.trim() === 'Tham số'; })[0]; if (t) t.click();
      var fly = $('#dock-r.flyout [data-fly="params"]'); if (fly) fly.click(); // khung 1366: palette tự ẩn, mở ra để thấy ngay
      // vừa khung nhìn, chừa chỗ cho banner phía trên vùng vẽ
      var v = V(), bh = ($('#banners') || {}).offsetHeight || 0, w = bb[2] - bb[0], h = bb[3] - bb[1], sc = Math.min(v.W / w, (v.H - bh) / h) * .9; v.pushHist(); d.view = { cx:(bb[0] + bb[2]) / 2, cy:(bb[1] + bb[3]) / 2 + bh / 2 / sc, s:sc }; v.draw(); TC.emit('view');
      TC.toast('Bản mã BM1 đã ràng buộc đủ nên kéo grip sẽ giữ nguyên hình. Nhấp đúp <b>d1=600</b> hoặc sửa <b>mep</b> trong palette Tham số để hình đổi theo (d2 = d1/2); xóa d2 để kéo được chiều cao. Rê lên biểu tượng để thấy đối tượng liên quan; Ctrl+Z hoàn tác.', { type:'info', log:false, ms:12000 });
    }, 80);
  };
})();
