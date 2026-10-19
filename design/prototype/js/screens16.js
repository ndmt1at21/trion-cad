/* TrionCAD prototype · Đợt 5 (sau v1.0): dynamic block: palette tạo block, tham số và action, trạng thái hiển thị, thử block (G05, phần P2 của G04). */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic, G = TC.geo, IMPL = TC.cmd.impl, ED = TC.ed, SC = TC.screens, H = SC.h;
  function A(){ return TC.app; } function V(){ return TC.app.view; } function D(){ return TC.app.doc(); }
  var OPEN = SC.open5 = SC.open5 || {}; // mã màn hình → hàm mở (danh mục gọi khi bấm); tiện ích mở ở SC.cat (D1, PRO, MOD, RUN, plan, pick, flash)
  var T = ED.T, P = TC.PALETTES, cv = TC.cssv;

  /* ================= dữ liệu: tham số và action là đối tượng trong định nghĩa block ================= */
  // tham số: kiểu → [tên, chữ tắt (như BPARAMETER), biểu tượng, tên mặc định, mô tả]
  var KP = {
    point:['Điểm', 'O', 'dpoint', 'Vị trí', 'Vị trí X, Y của một điểm'], linear:['Thẳng', 'L', 'dlinear', 'Khoảng cách', 'Khoảng cách giữa hai điểm'],
    polar:['Cực', 'P', 'dpolar', 'Cực', 'Khoảng cách và góc'], xy:['XY', 'X', 'dxy', 'XY', 'Khoảng cách X, Y từ điểm gốc'],
    rot:['Xoay', 'R', 'drot', 'Góc', 'Góc xoay quanh điểm gốc'], align:['Gióng', 'A', 'dalign', 'Gióng', 'Gióng block theo đối tượng khác'],
    flip:['Lật', 'F', 'dflip', 'Lật', 'Lật đối xứng qua một đường'], vis:['Hiển thị', 'V', 'dvis', 'Trạng thái', 'Ẩn, hiện đối tượng theo trạng thái'],
    lookup:['Tra cứu', 'K', 'dlookup', 'Tra cứu', 'Chọn giá trị từ bảng tra cứu'], base:['Điểm gốc', 'B', 'dbase', 'Điểm gốc', 'Điểm gốc của tham chiếu'],
    table:['Bảng thuộc tính', 'T', 'table', 'Bảng thuộc tính', 'Tổ hợp giá trị tham số']
  };
  var KA = { move:['Di chuyển', 'M', 'move', 'Dời đối tượng theo điểm tham số'], scale:['Tỉ lệ', 'S', 'scale', 'Tỉ lệ đối tượng theo khoảng cách'], stretch:['Kéo giãn', 'T', 'stretch', 'Kéo giãn phần nằm trong khung'], pstretch:['Kéo giãn cực', 'P', 'dpstretch', 'Kéo giãn và xoay theo tham số cực'],
    array:['Mảng', 'A', 'array', 'Nhân bản theo khoảng cách'], rotate:['Xoay', 'R', 'rotate', 'Xoay đối tượng theo tham số xoay'], flip:['Lật', 'F', 'mirror', 'Lật đối tượng theo tham số lật'], lookup:['Tra cứu', 'L', 'dlookup', 'Gán giá trị theo bảng tra cứu'] };
  var FOR = { point:['move', 'stretch'], linear:['move', 'scale', 'stretch', 'array'], polar:['move', 'scale', 'stretch', 'pstretch', 'array'], xy:['move', 'scale', 'stretch', 'array'], rot:['rotate'], flip:['flip'], lookup:['lookup'] };
  var PKS = ['point', 'linear', 'polar', 'xy', 'rot', 'align', 'flip', 'vis', 'lookup', 'base'], PTS = ['p1', 'p2', 'p', 'lab'];
  function isPar(x){ return !!x && x.t === 'bparam'; } function isAct(x){ return !!x && x.t === 'baction'; }
  function pars(l){ return l.filter(isPar); } function acts(l){ return l.filter(isAct); }
  function isDyn(bd){ return !!bd && bd.ents.some(isPar); }
  function parBy(l, bk){ return pars(l).filter(function(p){ return p.bk === bk; })[0]; }
  function visPar(l){ return pars(l).filter(function(p){ return p.k === 'vis'; })[0]; }
  function lkAct(l, p){ return acts(l).filter(function(a){ return a.k === 'lookup' && a.par === p.bk; })[0]; }
  var add2 = function(a, b){ return [a[0] + b[0], a[1] + b[1]]; }, sub2 = function(a, b){ return [a[0] - b[0], a[1] - b[1]]; }, mul2 = function(a, k){ return [a[0] * k, a[1] * k]; }, dot2 = function(a, b){ return a[0] * b[0] + a[1] * b[1]; };
  function mid(a, b){ return [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2]; }
  function unit(v){ var L = Math.hypot(v[0], v[1]) || 1; return [v[0] / L, v[1] / L]; }
  function rect(a, b){ var x0 = Math.min(a[0], b[0]), x1 = Math.max(a[0], b[0]), y0 = Math.min(a[1], b[1]), y1 = Math.max(a[1], b[1]); return [[x0, y0], [x1, y0], [x1, y1], [x0, y1]]; }
  // ma trận tham chiếu [a, b, c, d, e, f]: x' = ax + cy + e, y' = bx + dy + f
  function mIns(e){ var r = (e.rot || 0) * Math.PI / 180, c = Math.cos(r), s = Math.sin(r), sx = e.sx || 1, sy = e.sy || sx; return [sx * c, sx * s, -sy * s, sy * c, e.p[0], e.p[1]]; }
  function mAp(M, p){ return [M[0] * p[0] + M[2] * p[1] + M[4], M[1] * p[0] + M[3] * p[1] + M[5]]; }
  function mInv(M){ var k = M[0] * M[3] - M[1] * M[2]; return [M[3] / k, -M[1] / k, -M[2] / k, M[0] / k, (M[2] * M[5] - M[3] * M[4]) / k, (M[1] * M[4] - M[0] * M[5]) / k]; }
  function sang(M, u){ var w = [M[0] * u[0] + M[2] * u[1], M[1] * u[0] + M[3] * u[1]]; return Math.atan2(-w[1], w[0]); } // góc trên màn hình (trục y hướng xuống)

  /* ---------- giá trị tham số trên tham chiếu: e.dyn[bk]; thiếu thì lấy theo định nghĩa ---------- */
  // linear [đầu, cuối] dọc theo tham số; polar [khoảng cách, góc]; xy [x, y]; point [x, y]; rot, align: góc; flip 0/1; vis: tên trạng thái
  function st0(p){ switch (p.k){ case 'linear': return [0, G.dist(p.p1, p.p2)]; case 'polar': return [G.dist(p.p1, p.p2), G.ang(p.p1, p.p2)]; case 'xy': return sub2(p.p2, p.p1); case 'point': return p.p.slice(); case 'rot': return G.ang(p.p, p.p2); case 'align': case 'flip': return 0; case 'vis': return (p.states[0] || {}).n; } return null; }
  function stOf(p, dy){ var v = dy && dy[p.bk]; return v == null ? st0(p) : v; }
  function setDyn(x, bk, v){ x.dyn = Object.assign({}, x.dyn); x.dyn[bk] = v; }
  function visSt(p, n){ var S = p.states; return S.filter(function(s){ return s.n === n; })[0] || S.filter(function(s){ return p.was && s.n === p.was[n]; })[0] || S[0]; }
  function valOf(p, s){ return p.k === 'linear' ? s[1] - s[0] : p.k === 'polar' ? s[0] : s; }
  function withVal(p, s, v){ return p.k === 'linear' ? [s[0], s[0] + v] : p.k === 'polar' ? [v, s[1]] : v; }
  function snapVs(p, v){ var vs = p.vs; if (!vs || typeof v !== 'number') return v; if (vs.list && vs.list.length) return vs.list.reduce(function(b, x){ return Math.abs(x - v) < Math.abs(b - v) ? x : b; }, vs.list[0]); if (vs.inc){ var b0 = vs.min != null ? vs.min : 0; return TC.clamp(b0 + Math.round((v - b0) / vs.inc) * vs.inc, vs.min != null ? vs.min : -Infinity, vs.max != null ? vs.max : Infinity); } return v; }
  function eqV(a, b){ return typeof a === 'number' && typeof b === 'number' ? Math.abs(a - b) < 1e-6 : String(a) === String(b); }
  function fmtV(p, v, i){ if (Array.isArray(v)) return v.map(function(x){ return TC.d(x); }).join(', '); if (p.k === 'flip') return v ? 'Lật' : 'Không lật'; if (p.k === 'rot' || p.k === 'align' || (p.k === 'polar' && i === 1)) return TC.d(v, 0) + '°'; return typeof v === 'number' ? TC.d(v) : String(v == null ? '—' : v); }
  // tra cứu: hàng có mọi cột vào khớp giá trị hiện tại; không khớp thì "Tùy chỉnh"
  function rowMatch(l, cols, r, k, dy){ return cols.every(function(bk, i){ var q = parBy(l, bk); return q && eqV(valOf(q, stOf(q, dy)), r[i + k]); }); }
  function lkVal(l, p, dy){ var a = lkAct(l, p); if (!a || !a.tbl) return null; var r = a.tbl.rows.filter(function(r){ return rowMatch(l, a.tbl.cols, r, 0, dy); })[0]; return r ? r[r.length - 1] : 'Tùy chỉnh'; }
  function tbRow(l, p, dy){ var r = (p.rows || []).filter(function(r){ return rowMatch(l, p.cols, r, 1, dy); })[0]; return r ? r[0] : 'Tùy chỉnh'; }
  // gán giá trị cho tham chiếu x (trong ED.mod); tra cứu và bảng thuộc tính gán lại các tham số nguồn
  function setVal(l, p, x, v){
    if (p.k === 'lookup'){ var a = lkAct(l, p), r = a && a.tbl.rows.filter(function(r){ return eqV(r[r.length - 1], v); })[0]; if (r) a.tbl.cols.forEach(function(bk, i){ var q = parBy(l, bk); if (q) setVal(l, q, x, r[i]); }); return; }
    if (p.k === 'table'){ var r2 = (p.rows || []).filter(function(r){ return r[0] === v; })[0]; if (r2) p.cols.forEach(function(bk, i){ var q = parBy(l, bk); if (q) setVal(l, q, x, r2[i + 1]); }); return; }
    setDyn(x, p.bk, withVal(p, stOf(p, x.dyn), snapVs(p, v)));
  }
  function curVal(l, p, dy){ return p.k === 'vis' ? visSt(p, stOf(p, dy)).n : p.k === 'lookup' ? lkVal(l, p, dy) : p.k === 'table' ? tbRow(l, p, dy) : valOf(p, stOf(p, dy)); }
  function menuVals(l, p){ if (p.k === 'vis') return p.states.map(function(s){ return s.n; }); if (p.k === 'flip') return [0, 1]; if (p.k === 'table') return (p.rows || []).map(function(r){ return r[0]; }); if (p.k === 'lookup'){ var a = lkAct(l, p); return a && a.tbl && a.tbl.rev !== false ? a.tbl.rows.map(function(r){ return r[r.length - 1]; }) : []; } return p.vs && p.vs.list ? p.vs.list.slice() : []; }
  // bảng thuộc tính bắt buộc khớp hàng: các tham số trong bảng chỉ đổi qua bảng
  function locked(l){ var o = {}; pars(l).forEach(function(p){ if (p.k === 'table' && p.strict) p.cols.forEach(function(bk){ o[bk] = 1; }); }); return o; }

  /* ================= tính hình của tham chiếu theo giá trị tham số (như block ẩn danh *U của DWG) ================= */
  function stretchGeo(x, inF, D){ var mv = function(q){ return inF(q) ? add2(q, D) : q; };
    switch (x.t){
      case 'line': x.p1 = mv(x.p1); x.p2 = mv(x.p2); break;
      case 'pline': case 'spline': x.pts = x.pts.map(mv); break;
      case 'hatch': x.loops = x.loops.map(function(l){ return l.map(mv); }); break;
      case 'arc': { var a = ED.polar(x.c, x.r, x.a0), b = ED.polar(x.c, x.r, x.a1), m = ED.polar(x.c, x.r, x.a0 + ED.sweep(x.a0, x.a1) / 2), ia = inF(a), ib = inF(b);
        if (ia && ib) ED.xform(x, T.move(D[0], D[1])); else if (ia || ib){ var r3 = ED.arc3(mv(a), add2(m, mul2(D, .5)), mv(b)); if (r3) Object.assign(x, r3); } break; }
      default: { var k = x.p || x.c || x.p1 || (x.pts && x.pts[0]); if (k && inF(k)) ED.xform(x, T.move(D[0], D[1])); }
    }
  }
  function evalDef(bd, dy){
    var E = bd.ents, ps = pars(E), as = acts(E), A0 = {}, S = {};
    var vp = visPar(E), hide = vp ? visSt(vp, stOf(vp, dy)).hide || [] : [];
    var geo = E.filter(function(x){ return !isPar(x) && !isAct(x) && !(x.bk && hide.indexOf(x.bk) >= 0); }).map(ED.clean);
    ps.forEach(function(p){ var o = {}; PTS.forEach(function(n){ if (p[n]) o[n] = p[n].slice(); }); A0[p.bk] = o; S[p.bk] = stOf(p, dy); });
    function selOf(a){ return { g:geo.filter(function(x){ return x.bk && a.sel.indexOf(x.bk) >= 0; }), q:a.sel.filter(function(k){ return A0[k]; }) }; }
    function pts(keys, own, f){ keys.forEach(function(k){ if (k === own) return; var o = A0[k]; PTS.forEach(function(n){ if (o[n]) o[n] = f(o[n]); }); }); }
    function xfSel(a, own, t){ var s = selOf(a); s.g.forEach(function(x){ ED.xform(x, t); }); pts(s.q, own, t.f); }
    function strSel(a, own, D, F){ var s = selOf(a), inF = function(q){ return G.inPoly(q, F); }; s.g.forEach(function(x){ stretchGeo(x, inF, D); }); pts(s.q, own, function(q){ return inF(q) ? add2(q, D) : q; }); }
    function copies(a, offs){ var s = selOf(a); offs.forEach(function(o){ s.g.forEach(function(x){ var c = ED.xform(ED.clean(x), T.move(o[0], o[1])); delete c.bk; geo.push(c); }); }); }
    function line(a, u, len, gap){ var o = [], n = Math.min(200, Math.max(1, Math.floor(len / gap + 1e-9))); for (var i = 1; i < n; i++) o.push(mul2(u, gap * i)); copies(a, o); }
    function run(p){
      var a0 = A0[p.bk], s = S[p.bk], my = as.filter(function(a){ return a.par === p.bk && a.sel; });
      if (p.k === 'linear'){ var L = G.dist(a0.p1, a0.p2) || 1, u = unit(sub2(a0.p2, a0.p1)), P1 = add2(a0.p1, mul2(u, s[0])), P2 = add2(a0.p1, mul2(u, s[1])), Dl = { p1:sub2(P1, a0.p1), p2:sub2(P2, a0.p2) };
        my.forEach(function(a){ var pt = a.pt || 'p2', dd = Dl[pt];
          if (a.k === 'move') xfSel(a, p.bk, T.move(dd[0], dd[1]));
          else if (a.k === 'stretch' && a.frame) strSel(a, p.bk, dd, a.frame);
          else if (a.k === 'scale') xfSel(a, p.bk, T.sc(pt === 'p2' ? P1 : P2, (s[1] - s[0]) / L));
          else if (a.k === 'array') line(a, pt === 'p2' ? u : mul2(u, -1), s[1] - s[0], a.gap || L); });
        a0.p1 = P1; a0.p2 = P2; return; }
      if (p.k === 'polar'){ var d0 = G.dist(a0.p1, a0.p2) || 1, an0 = G.ang(a0.p1, a0.p2), r = s[1] * Math.PI / 180, uN = [Math.cos(r), Math.sin(r)], E2 = add2(a0.p1, mul2(uN, s[0])), Dp = sub2(E2, a0.p2);
        my.forEach(function(a){
          if (a.k === 'move') xfSel(a, p.bk, T.move(Dp[0], Dp[1]));
          else if (a.k === 'stretch' && a.frame) strSel(a, p.bk, Dp, a.frame);
          else if (a.k === 'pstretch' && a.frame){ var tr = T.rot(a0.p1, s[1] - an0); xfSel(a, p.bk, tr); strSel(a, p.bk, mul2(uN, s[0] - d0), a.frame.map(tr.f)); }
          else if (a.k === 'scale') xfSel(a, p.bk, T.sc(a0.p1, s[0] / d0));
          else if (a.k === 'array') line(a, uN, s[0], a.gap || d0); });
        a0.p2 = E2; return; }
      if (p.k === 'xy'){ var C2 = add2(a0.p1, s), Dx = sub2(C2, a0.p2), k0 = G.dist(a0.p1, a0.p2) || 1;
        my.forEach(function(a){
          if (a.k === 'move') xfSel(a, p.bk, T.move(Dx[0], Dx[1]));
          else if (a.k === 'stretch' && a.frame) strSel(a, p.bk, Dx, a.frame);
          else if (a.k === 'scale') xfSel(a, p.bk, T.sc(a0.p1, G.dist(a0.p1, C2) / k0));
          else if (a.k === 'array'){ var gx = (a.gap && a.gap[0]) || 1, gy = (a.gap && a.gap[1]) || 1, nx = Math.min(50, Math.max(1, Math.floor(Math.abs(s[0]) / gx + 1e-9))), ny = Math.min(50, Math.max(1, Math.floor(Math.abs(s[1]) / gy + 1e-9))), o = [];
            for (var i = 0; i < nx; i++) for (var j = 0; j < ny; j++) if (i || j) o.push([i * gx * (s[0] < 0 ? -1 : 1), j * gy * (s[1] < 0 ? -1 : 1)]); copies(a, o); } });
        a0.p2 = C2; return; }
      if (p.k === 'point'){ var Dq = sub2(s, a0.p); my.forEach(function(a){ if (a.k === 'move') xfSel(a, p.bk, T.move(Dq[0], Dq[1])); else if (a.k === 'stretch' && a.frame) strSel(a, p.bk, Dq, a.frame); }); a0.p = s.slice(); if (a0.lab) a0.lab = add2(a0.lab, Dq); return; }
      if (p.k === 'rot'){ var tr2 = T.rot(a0.p, s - G.ang(a0.p, a0.p2)); my.forEach(function(a){ if (a.k === 'rotate') xfSel(a, p.bk, tr2); }); a0.p2 = tr2.f(a0.p2); if (a0.lab) a0.lab = tr2.f(a0.lab); return; }
      if (p.k === 'flip' && s) my.forEach(function(a){ if (a.k === 'flip') xfSel(a, p.bk, T.mir(a0.p1, a0.p2)); });
    }
    ps.forEach(function(p){ if (p.k !== 'flip' && p.k !== 'align') run(p); });
    ps.forEach(function(p){ if (p.k === 'flip') run(p); });
    // gióng: xoay cả block quanh điểm gốc của tham số gióng
    ps.forEach(function(p){ if (p.k !== 'align' || !S[p.bk]) return; var t = T.rot(A0[p.bk].p, S[p.bk]); geo.forEach(function(x){ ED.xform(x, t); }); pts(Object.keys(A0), null, t.f); });
    return { ents:geo, A:A0, S:S };
  }
  function dynDoc(doc, e){ var bd = e.t === 'insert' && doc.blocks && doc.blocks[e.name]; if (!isDyn(bd)) return null; var bl = Object.create(doc.blocks), pd = Object.create(doc); bl[e.name] = Object.assign({}, bd, { ents:evalDef(bd, e.dyn).ents }); pd.blocks = bl; return pd; }
  var pe0 = TC.prepEnt;
  TC.prepEnt = function(doc, e, temp){ var pd = dynDoc(doc, e); if (!pd) return pe0(doc, e, temp); pe0(pd, e, true); if (!temp) doc.byId[e.id] = e; return e; };
  var pd0 = TC.prepDoc;
  TC.prepDoc = function(doc){ pd0(doc); var n = 0; [doc.ents].concat(doc.layouts.map(function(L){ return L.ents; })).forEach(function(l){ l.forEach(function(e){ if (e.t === 'insert' && isDyn(doc.blocks[e.name])){ TC.prepEnt(doc, e); n++; } }); }); if (n) TC.docExt(doc); };

  /* ================= vẽ tham số, action trong Block Editor (không hiện ở tham chiếu) ================= */
  // ký hiệu tham số, action giữ cỡ tối thiểu trên màn hình (như BPARAMETERSIZE): tính lại khi zoom trong Block Editor
  var SZ = 0;
  function hs(e){ return Math.max(e.h || 60, SZ ? 8.5 / SZ : 0); }
  TC.on('view', function(){ var d = D(), v = V(); if (!d || !d.bedit || !v || v.space()) return; var s = v.vw().s; if (SZ && Math.abs(s - SZ) / s < .02) return; SZ = s; d.ents.forEach(function(e){ if (isPar(e) || isAct(e)) TC.prepEnt(d, e); }); v.draw(); });
  function readable(a){ a = ((a % 360) + 360) % 360; return a > 90 && a <= 270 ? a - 180 : a; }
  function offOf(p, q){ var u = unit(sub2(p.p2, p.p1)); return dot2(sub2(q, p.p1), [-u[1], u[0]]); }
  function labOf(p){ var h = hs(p);
    switch (p.k){
      case 'linear': { var u = unit(sub2(p.p2, p.p1)), n = [-u[1], u[0]], of = p.off || 0, sg = of < 0 ? -1 : 1; return { p:add2(mid(p.p1, p.p2), mul2(n, of + sg * h * 1.1)), al:'m', rot:readable(G.ang([0, 0], u)) }; }
      case 'flip': { var c = p.lab || mid(p.p1, p.p2); return { p:add2(c, [h * 2.4, -h * .35]), al:'l' }; }
      case 'xy': return { p:add2(p.p2, [h * .9, h * .4]), al:'l' };
      case 'align': return { p:add2(p.p2, [h * .9, -h * .35]), al:'l' };
      case 'polar': case 'rot': return { p:add2(p.lab || p.p2, [0, -h * .35]), al:'l' };
      default: { var o = p.lab || add2(p.p, [h * 1.5, 0]); return { p:[o[0], o[1] - h * .35], al:o[0] < p.p[0] ? 'r' : 'l' }; }
    }
  }
  function prim(x, C){ return function(pr){ x.o(pr); pr.src = C; }; }
  TC.flatExt.bparam = function(x){
    if (x.host) return; var e = x.e, h = hs(e), o = prim(x, { color:cv('--c-track'), layer:'0' }), P = x.P;
    var seg = function(a, b){ o({ k:'seg', a:P(a), b:P(b) }); }, tri = function(a, b, c){ o({ k:'tri', pts:[P(a), P(b), P(c)] }); };
    var arrow = function(q, u, z){ var n = [-u[1], u[0]], b = sub2(q, mul2(u, z)); tri(q, add2(b, mul2(n, z * .4)), sub2(b, mul2(n, z * .4))); };
    var dash = function(a, b){ var L = G.dist(a, b), u = unit(sub2(b, a)), st = h * .5; for (var t = 0; t < L; t += st * 1.9) seg(add2(a, mul2(u, t)), add2(a, mul2(u, Math.min(L, t + st)))); };
    var circ = function(c, r){ o({ k:'circ', c:P(c), r:r }); }, arc = function(c, r, a0, a1){ o({ k:'arc', c:P(c), r:r, a0:a0, a1:a1 }); };
    var lb = labOf(e); o({ k:'text', p:P(lb.p), h:h, s:e.name, rot:lb.rot || 0, al:lb.al, style:'Standard' });
    switch (e.k){
      case 'linear': { var u = unit(sub2(e.p2, e.p1)), n = [-u[1], u[0]], of = e.off || 0, q1 = add2(e.p1, mul2(n, of)), q2 = add2(e.p2, mul2(n, of)), ov = mul2(n, (of < 0 ? -1 : 1) * h * .35);
        if (Math.abs(of) > 1e-6){ seg(e.p1, add2(q1, ov)); seg(e.p2, add2(q2, ov)); } seg(q1, q2); arrow(q1, mul2(u, -1), h * .9); arrow(q2, u, h * .9); break; }
      case 'flip': { var fu = unit(sub2(e.p2, e.p1)), fn = [-fu[1], fu[0]], c = e.lab || mid(e.p1, e.p2), z = h * .75; dash(e.p1, e.p2); seg(add2(c, mul2(fn, -z * .9)), add2(c, mul2(fn, z * .9))); arrow(add2(c, mul2(fn, z * 1.7)), fn, z); arrow(add2(c, mul2(fn, -z * 1.7)), mul2(fn, -1), z); break; }
      case 'vis': { var p = e.p, r = h * 1.15; arc(add2(p, [0, -h * .62]), r, 58, 122); arc(add2(p, [0, h * .62]), r, 238, 302); circ(p, h * .28); break; }
      case 'lookup': case 'table': { var w = h * .85, hh = h * .6, q = e.p; seg([q[0] - w, q[1] - hh], [q[0] + w, q[1] - hh]); seg([q[0] + w, q[1] - hh], [q[0] + w, q[1] + hh]); seg([q[0] + w, q[1] + hh], [q[0] - w, q[1] + hh]); seg([q[0] - w, q[1] + hh], [q[0] - w, q[1] - hh]); seg([q[0] - w, q[1] + hh * .25], [q[0] + w, q[1] + hh * .25]); seg([q[0] - w * .2, q[1] - hh], [q[0] - w * .2, q[1] + hh]); if (e.k === 'table') seg([q[0] - w, q[1] - hh * .4], [q[0] + w, q[1] - hh * .4]); break; }
      case 'point': case 'base': { circ(e.p, h * (e.k === 'base' ? .5 : .32)); seg(add2(e.p, [-h * .75, 0]), add2(e.p, [h * .75, 0])); seg(add2(e.p, [0, -h * .75]), add2(e.p, [0, h * .75])); break; }
      case 'polar': { var pu = unit(sub2(e.p2, e.p1)); seg(e.p1, e.p2); arrow(e.p2, pu, h * .9); seg(e.p1, add2(e.p1, [h * 2, 0])); arc(e.p1, h * 1.6, 0, G.ang(e.p1, e.p2)); break; }
      case 'xy': { var a = e.p1, b = e.p2; dash(a, [b[0], a[1]]); dash([b[0], a[1]], b); dash(b, [a[0], b[1]]); dash([a[0], b[1]], a); arrow(b, unit(sub2(b, a)), h * .9); break; }
      case 'rot': { var R = G.dist(e.p, e.p2); for (var k = 0; k < 24; k++) arc(e.p, R, k * 15, k * 15 + 7); seg(e.p, e.p2); arrow(e.p2, unit([-(e.p2[1] - e.p[1]), e.p2[0] - e.p[0]]), h * .9); circ(e.p, h * .2); break; }
      case 'align': { var au = unit(sub2(e.p2, e.p)), an = [-au[1], au[0]]; seg(e.p, e.p2); arrow(e.p2, au, h * .9); seg(add2(e.p, mul2(an, -h * .6)), add2(e.p, mul2(an, h * .6))); break; }
    }
  };
  TC.flatExt.baction = function(x){
    if (x.host) return; var e = x.e, h = hs(e), z = h * .8, c = e.p, o = prim(x, { color:cv('--c-hl-res'), layer:'0' }), P = x.P;
    var seg = function(a, b){ o({ k:'seg', a:P(add2(c, mul2(a, z))), b:P(add2(c, mul2(b, z))) }); }, tri = function(a, b, d){ o({ k:'tri', pts:[a, b, d].map(function(q){ return P(add2(c, mul2(q, z))); }) }); };
    seg([-1, -1], [1, -1]); seg([1, -1], [1, 1]); seg([1, 1], [-1, 1]); seg([-1, 1], [-1, -1]);
    switch (e.k){
      case 'stretch': seg([-.55, -.55], [.35, .35]); tri([.6, .6], [.15, .45], [.45, .15]); seg([-.6, .1], [-.6, -.6]); seg([-.6, -.6], [.1, -.6]); break;
      case 'pstretch': seg([-.6, -.6], [.4, .4]); tri([.6, .6], [.15, .45], [.45, .15]); seg([-.6, -.6], [.5, -.6]); break;
      case 'move': seg([0, -.65], [0, .65]); seg([-.65, 0], [.65, 0]); tri([0, .75], [-.22, .45], [.22, .45]); tri([.75, 0], [.45, .22], [.45, -.22]); break;
      case 'scale': seg([-.6, -.6], [0, -.6]); seg([0, -.6], [0, 0]); seg([0, 0], [-.6, 0]); seg([-.6, 0], [-.6, -.6]); seg([0, 0], [.4, .4]); tri([.62, .62], [.18, .48], [.48, .18]); break;
      case 'rotate': for (var i = 0; i < 9; i++){ var a1 = (40 + i * 30) * Math.PI / 180, a2 = (40 + (i + 1) * 30) * Math.PI / 180; seg([.55 * Math.cos(a1), .55 * Math.sin(a1)], [.55 * Math.cos(a2), .55 * Math.sin(a2)]); } tri([.42, .35], [.68, .05], [.2, .08]); break;
      case 'flip': seg([0, -.7], [0, .7]); tri([-.7, 0], [-.2, .35], [-.2, -.35]); tri([.7, 0], [.2, .35], [.2, -.35]); break;
      case 'array': [[-.55, -.55], [.1, -.55], [-.55, .1], [.1, .1]].forEach(function(q){ seg(q, [q[0] + .45, q[1]]); seg([q[0] + .45, q[1]], [q[0] + .45, q[1] + .45]); seg([q[0] + .45, q[1] + .45], [q[0], q[1] + .45]); seg([q[0], q[1] + .45], q); }); break;
      case 'lookup': seg([-.6, .2], [.6, .2]); seg([-.6, -.25], [.6, -.25]); seg([-.1, .6], [-.1, -.6]); break;
    }
    o({ k:'text', p:P(add2(c, [z * 1.5, -h * .3])), h:h * .85, s:e.name, rot:0, al:'l', style:'Standard' });
  };
  TC.TYPE_NAMES.bparam = 'Tham số block'; TC.TYPE_NAMES.baction = 'Action block';
  // dời, xoay, tỉ lệ, đối xứng tham số và action như hình học thường
  var xf0 = ED.xform;
  ED.xform = function(e, t){ if (!isPar(e) && !isAct(e)) return xf0(e, t); PTS.forEach(function(n){ if (e[n]) e[n] = t.f(e[n]); }); if (e.frame) e.frame = e.frame.map(t.f); if (e.h) e.h *= t.k || 1; if (e.off) e.off *= (t.k || 1) * (t.mir ? -1 : 1); return e; };
  // khóa riêng (bk) để action, trạng thái hiển thị nhận ra đối tượng; bản sao mang khóa mới
  function newKey(d, pre){ var u = {}; d.ents.forEach(function(x){ if (x.bk) u[x.bk] = 1; }); for (var i = 1; ; i++) if (!u[pre + i]) return pre + i; }
  function uName(d, base){ var u = {}; d.ents.forEach(function(x){ if (isPar(x) || isAct(x)) u[x.name] = 1; }); for (var i = 1; ; i++) if (!u[base + ' ' + i]) return base + ' ' + i; }
  function keyOf(d, e){ if (!e.bk) ED.mod(e, function(x){ x.bk = newKey(d, isPar(x) ? 'p' : 'g'); }); return e.bk; }
  var add0 = ED.add;
  ED.add = function(e, list){ var d = D(); if (d && d.bedit && e.bk && d.ents.some(function(x){ return x.bk === e.bk; })){ if (isPar(e) || isAct(e)){ e.bk = newKey(d, isPar(e) ? 'p' : 'a'); e.name = uName(d, isPar(e) ? KP[e.k][3] : KA[e.k][0]); } else delete e.bk; } return add0.apply(this, arguments); };
  // xóa tham số thì xóa luôn action của nó (cùng một bước hoàn tác)
  var del0 = ED.del;
  ED.del = function(e){ var d = D(); del0.apply(this, arguments); if (isPar(e) && d && d.bedit) acts(d.ents).filter(function(a){ return a.par === e.bk; }).forEach(function(a){ del0(a); }); };

  /* ================= trạng thái hiển thị trong Block Editor ================= */
  var BV = { mode:TC.load('bvmode', false), show:false };
  function curVis(d){ var vp = d.bedit && visPar(d.ents); return vp ? visSt(vp, d.bedit.vs) : null; }
  function hidHere(d, e){ if (!d.bedit || !e.bk || isPar(e) || isAct(e)) return false; var s = curVis(d); return !!s && (s.hide || []).indexOf(e.bk) >= 0; }
  var ev0 = TC.entVisible;
  TC.entVisible = function(doc, e){ if (doc && doc.bedit && !BV.mode && !BV.show && hidHere(doc, e)) return false; return ev0(doc, e); };
  SC.bvFade = function(d, e){ return hidHere(d, e) ? .3 : 1; }; // screens10 làm mờ (Chế độ hiển thị bật)
  function setVs(d, n){ d.bedit.vs = n; V().draw(); ED.ctxRefresh(); A().refreshPalettes(); TC.cmdline.msg('Trạng thái hiển thị hiện hành: <b>' + esc(n) + '</b>.'); }

  /* ================= cảnh báo, tô sáng action trên vùng vẽ ================= */
  function actBad(l, a){ return !parBy(l, a.par) ? 'Action mất tham số' : a.k === 'lookup' ? (a.tbl && a.tbl.rows.length ? '' : 'Bảng tra cứu còn trống') : !(a.sel || []).length ? 'Action chưa có đối tượng' : ''; }
  function warns(l){ var w = []; pars(l).forEach(function(p){ if (FOR[p.k] && !acts(l).some(function(a){ return a.par === p.bk; })) w.push({ e:p, msg:'Tham số ' + p.name + ' chưa có action' }); }); acts(l).forEach(function(a){ var m = actBad(l, a); if (m) w.push({ e:a, msg:m + ': ' + a.name }); }); return w; }
  function badgePt(e){ var h = hs(e); if (isAct(e)) return add2(e.p, [h * .8, h * .8]); var lb = labOf(e), w = e.name.length * h * .75, r = (lb.rot || 0) * Math.PI / 180; return lb.al === 'm' ? add2(lb.p, mul2([Math.cos(r), Math.sin(r)], w / 2 + h * .8)) : add2(lb.p, [lb.al === 'r' ? h * .7 : w + h * .7, h * .35]); }
  function badge(ctx, s){ var x = Math.round(s[0]) + .5, y = Math.round(s[1]) + .5; ctx.beginPath(); ctx.moveTo(x, y - 7); ctx.lineTo(x + 7.5, y + 6); ctx.lineTo(x - 7.5, y + 6); ctx.closePath(); ctx.fillStyle = cv('--c-snap'); ctx.fill(); ctx.fillStyle = cv('--c-canvas'); ctx.font = '700 10px ' + cv('--f-ui'); ctx.textAlign = 'center'; ctx.textBaseline = 'middle'; ctx.fillText('!', x, y + 1.5); }
  function anchorOf(p, a){ return p.k === 'linear' ? (a.pt === 'p1' ? p.p1 : p.p2) : p.p2 && p.k !== 'flip' ? p.p2 : p.lab || p.p || mid(p.p1, p.p2); }
  function actHi(ctx, v, d, a){ var col = cv('--c-hl-res'), vw = v.vw(), S = function(q){ return v.mToS(q); };
    ctx.save(); d.ents.forEach(function(x){ if (x.bk && (a.sel || []).indexOf(x.bk) >= 0) v.drawPrims(ctx, x, vw, false, { color:col, lw:2.2 }); });
    ctx.strokeStyle = col; ctx.lineWidth = 1.2;
    if (a.frame){ ctx.setLineDash([6, 4]); ctx.beginPath(); a.frame.forEach(function(q, i){ var s = S(q); if (i) ctx.lineTo(s[0], s[1]); else ctx.moveTo(s[0], s[1]); }); ctx.closePath(); ctx.stroke(); }
    var p = parBy(d.ents, a.par); if (p){ var s1 = S(a.p), s2 = S(anchorOf(p, a)); ctx.setLineDash([2, 3]); ctx.beginPath(); ctx.moveTo(s1[0], s1[1]); ctx.lineTo(s2[0], s2[1]); ctx.stroke(); }
    ctx.restore(); }
  // vạch giá trị của bảng giá trị và giá trị đang kéo khi kéo grip động
  function dragHi(ctx, v, g){ var S = function(q){ return g.e._paper ? v.toS(q) : v.mToS(q); }, col = cv('--c-track'), tip = $('.ctip.info'); if (tip) tip.remove();
    ctx.save(); ctx.strokeStyle = col; ctx.lineWidth = 1.5;
    (g.ticks || []).forEach(function(t){ var s = S(t), n = [-Math.sin(g.sa), Math.cos(g.sa)]; if (!s) return; ctx.beginPath(); ctx.moveTo(s[0] + n[0] * 2, s[1] + n[1] * 2); ctx.lineTo(s[0] - n[0] * 11, s[1] - n[1] * 11); ctx.stroke(); }); // vạch phía trên, ô nhập động ở phía dưới con trỏ
    var tm = (v.temp || []).filter(function(x){ return x.k === 'ents' && x.list[0] && x.list[0].t === 'insert'; })[0], e = tm && tm.list[0], m = v.mouse;
    if (e && m){ var txt = g.par.name + ' = ' + fmtV(g.par, curVal(g.bd.ents, g.par, e.dyn)); ctx.font = '11px ' + cv('--f-mono'); var w = ctx.measureText(txt).width + 12, x = m[0] - w - 14, y = m[1] - 30; ctx.fillStyle = cv('--c-dyn-bg'); ctx.strokeStyle = cv('--c-dyn-b'); ctx.lineWidth = 1; ctx.fillRect(x, y, w, 18); ctx.strokeRect(x + .5, y + .5, w - 1, 17); ctx.fillStyle = cv('--c-dyn-t'); ctx.textAlign = 'left'; ctx.textBaseline = 'middle'; ctx.fillText(txt, x + 6, y + 9.5); }
    ctx.restore(); }
  TC.overlays.push(function(ctx, v){
    var d = v.doc; if (!d) return;
    if (d.bedit && !v.space()){
      warns(d.ents).forEach(function(w){ var s = v.mToS(badgePt(w.e)); if (s) badge(ctx, s); });
      var hv = v.hover && d.byId[v.hover], hot = d.sel.map(function(id){ return d.byId[id]; }).filter(isAct); if (isAct(hv) && hot.indexOf(hv) < 0) hot.push(hv);
      hot.slice(0, 4).forEach(function(a){ actHi(ctx, v, d, a); });
    }
    var g = ED.grip.hot; if (g && g.dyn) dragHi(ctx, v, g);
  });

  /* ================= grip động trên tham chiếu (07.1 §9.5): ▶ kéo giãn, ⇄ lật, ● xoay, ▼ tra cứu, trạng thái ================= */
  function gStyle(ctx, st){ ctx.fillStyle = cv(st === 'hot' ? '--c-grip-hot' : st === 'hover' ? '--c-grip-hover' : '--c-grip-cold'); ctx.strokeStyle = cv(st === 'hot' ? '--c-rubber' : '--c-grip-border'); ctx.lineWidth = st === 'cold' ? 1 : st === 'hover' ? 1.5 : 2; ctx.setLineDash([]); }
  function gPath(ctx, pts){ ctx.beginPath(); pts.forEach(function(q, i){ if (i) ctx.lineTo(q[0], q[1]); else ctx.moveTo(q[0], q[1]); }); ctx.closePath(); ctx.fill(); ctx.stroke(); }
  function gDraw(kind, ang){ return function(ctx, s, st){ var x = Math.round(s[0]) + .5, y = Math.round(s[1]) + .5, z = st === 'cold' ? 6.5 : 8, c = Math.cos(ang || 0), sn = Math.sin(ang || 0), R = function(px, py){ return [x + px * c - py * sn, y + px * sn + py * c]; };
    ctx.save(); gStyle(ctx, st);
    if (kind === 'arrow') gPath(ctx, [R(z * 1.2, 0), R(-z * .8, -z * .85), R(-z * .8, z * .85)]);
    else if (kind === 'flip'){ gPath(ctx, [R(z * 1.5, 0), R(z * .25, -z * .85), R(z * .25, z * .85)]); gPath(ctx, [R(-z * 1.5, 0), R(-z * .25, -z * .85), R(-z * .25, z * .85)]); }
    else if (kind === 'down') gPath(ctx, [[x, y + z * .9], [x - z * 1.05, y - z * .7], [x + z * 1.05, y - z * .7]]);
    else if (kind === 'circ'){ ctx.beginPath(); ctx.arc(x, y, z * .8, 0, 7); ctx.fill(); ctx.stroke(); }
    else { ctx.beginPath(); ctx.rect(x - z * .7, y - z * .7, z * 1.4, z * 1.4); ctx.fill(); ctx.stroke(); }
    ctx.restore(); }; }
  function applyVal(l, p, ents, v){ var tp = $('.ctip.info'); if (tp) tp.remove(); ED.run('Đổi ' + p.name, function(){ ents.forEach(function(e){ ED.mod(e, function(x){ setVal(l, p, x, v); }); }); }); TC.cmdline.msg(esc(p.name) + ': <b>' + esc(fmtV(p, v)) + '</b>' + (ents.length > 1 ? ' · ' + ents.length + ' block' : '') + '. Ctrl+Z để hoàn tác.'); }
  function ticks(p, from, dir, M){ var vs = p.vs, list = !vs ? [] : vs.list ? vs.list : vs.inc && vs.min != null && vs.max != null && (vs.max - vs.min) / vs.inc <= 60 ? Array.from({ length:Math.floor((vs.max - vs.min) / vs.inc) + 1 }, function(_, i){ return vs.min + i * vs.inc; }) : []; return list.map(function(v){ return mAp(M, add2(from, mul2(dir, v))); }); }
  TC.gripsExt.push(function(add, e, d){
    if (e.t !== 'insert' || TC.isViewer()) return; var bd = d.blocks[e.name]; if (!isDyn(bd)) return;
    var l = bd.ents, R = evalDef(bd, e.dyn), M = mIns(e), Mi = mInv(M), W = function(q){ return mAp(M, q); }, lock = locked(l);
    pars(l).forEach(function(p){
      var a = R.A[p.bk], s = R.S[p.bk], n = p.grips == null ? 1 : p.grips, key = 'dy:' + p.bk, o = { dyn:p.bk, par:p, bd:bd };
      if (!n || lock[p.bk] || p.k === 'base') return;
      var q = function(pw){ return mAp(Mi, pw); };
      if (p.k === 'linear'){ var u = unit(sub2(a.p2, a.p1)), len0 = s[1] - s[0];
        var mk = function(end){ var dir = end === 2 ? u : mul2(u, -1), sa = sang(M, dir); return Object.assign({}, o, { sa:sa, tip:p.name + ' = ' + TC.d(len0) + ' · kéo để đổi', draw:gDraw('arrow', sa), ticks:ticks(p, end === 2 ? a.p1 : a.p2, dir, M),
          drag:function(x, pw){ var w = q(pw), len = snapVs(p, Math.max(1, end === 2 ? dot2(sub2(w, a.p1), u) : dot2(sub2(a.p2, w), u))); setDyn(x, p.bk, end === 2 ? [s[0], s[0] + len] : [s[1] - len, s[1]]); } }); };
        add(W(a.p2), key + ':2', 'tri', mk(2)); if (n > 1) add(W(a.p1), key + ':1', 'tri', mk(1)); return; }
      if (p.k === 'polar'){ var pa = sang(M, unit(sub2(a.p2, a.p1))); add(W(a.p2), key, 'tri', Object.assign(o, { sa:pa, tip:p.name + ' = ' + TC.d(s[0]) + ' < ' + TC.d(s[1], 0) + '°', draw:gDraw('arrow', pa), drag:function(x, pw){ var w = q(pw); setDyn(x, p.bk, [snapVs(p, Math.max(1, G.dist(a.p1, w))), G.ang(a.p1, w)]); } })); return; }
      if (p.k === 'xy'){ add(W(a.p2), key, 'sq', Object.assign(o, { tip:p.name + ' X = ' + TC.d(s[0]) + ', Y = ' + TC.d(s[1]), draw:gDraw('sq'), drag:function(x, pw){ setDyn(x, p.bk, sub2(q(pw), a.p1)); } })); return; }
      if (p.k === 'point'){ add(W(a.p), key, 'sq', Object.assign(o, { tip:p.name + ' · kéo để dời', draw:gDraw('sq'), drag:function(x, pw){ setDyn(x, p.bk, q(pw)); } })); return; }
      if (p.k === 'rot'){ add(W(a.p2), key, 'circ', Object.assign(o, { tip:p.name + ' = ' + TC.d(s, 0) + '° · kéo để xoay', draw:gDraw('circ'), drag:function(x, pw){ setDyn(x, p.bk, snapVs(p, G.ang(a.p, q(pw)))); } })); return; }
      if (p.k === 'align'){ var aa = sang(M, unit(sub2(a.p2, a.p))); add(W(a.p2), key, 'tri', Object.assign(o, { tip:p.name + ' · kéo để gióng', draw:gDraw('arrow', aa), drag:function(x, pw){ setDyn(x, p.bk, G.ang(a.p, q(pw)) - G.ang(p.p, p.p2)); } })); return; }
      if (p.k === 'flip'){ var fu = unit(sub2(a.p2, a.p1)); add(W(a.lab || mid(a.p1, a.p2)), key, 'tri', Object.assign(o, { tip:p.name + ': ' + fmtV(p, s) + ' · bấm để lật', draw:gDraw('flip', sang(M, [-fu[1], fu[0]])), click:function(g){ applyVal(l, p, [g.e], stOf(p, g.e.dyn) ? 0 : 1); } })); return; }
      if (/^(vis|lookup|table)$/.test(p.k) && menuVals(l, p).length){
        var items = function(g){ var cur = curVal(l, p, g.e.dyn); return [{ h:p.name }].concat(menuVals(l, p).map(function(v){ return { label:esc(fmtV(p, v)), check:eqV(v, cur), act:function(){ applyVal(l, p, [g.e], v); } }; })); };
        add(W(a.p), key, 'tri', Object.assign(o, { multi:'dyn', draw:gDraw('down'), menu:items, click:function(g){ var r = $('#cv').getBoundingClientRect(); TC.menu(items(g), r.left + g.s[0] + 10, r.top + g.s[1] + 10, { minWidth:190 }); } }));
      }
    });
  });
  // grip của tham số, action trong Block Editor: dời từng điểm, nhãn, khung kéo giãn
  TC.gripsExt.push(function(add, e){
    if (isPar(e)){ PTS.forEach(function(n){ if (e[n]) add(e[n], 'bp:' + n, 'sq', { drag:function(x, p){ x[n] = p.slice(); } }); }); if (e.k === 'linear') add(labOf(e).p, 'bp:off', 'sq', { drag:function(x, p){ x.off = offOf(x, p); } }); }
    else if (isAct(e)){ add(e.p, 'ba:p', 'sq', { drag:function(x, p){ x.p = p.slice(); } }); (e.frame || []).forEach(function(c, i){ add(c, 'ba:f' + i, 'sq', { drag:function(x, p){ x.frame = rect(p, x.frame[(i + 2) % 4]); } }); }); }
  });
  // rê lên grip động: chú thích tên, giá trị, cách dùng
  var hg0 = TC.cad.hook.grip, tipK = null;
  TC.cad.hook.grip = function(m){ var r = hg0.apply(this, arguments), g = ED.grip.hover, k = g && g.dyn ? g.e.id + g.key : null; if (k !== tipK){ tipK = k; if (k && g.tip && !g.menu) TC.ctip($('#cv'), g.s[0], g.s[1], g.tip, true); } return r; };

  /* ================= Thuộc tính: nhóm Tùy biến (tham chiếu block động), sửa tham số và action ================= */
  // phần sửa được: [nhãn, đọc(dyn), ghi(dyn, v) → giá trị tham số, danh sách giá trị]
  function partsOf(l, p){
    var g = function(i){ return function(dy){ return stOf(p, dy)[i]; }; }, s = function(i){ return function(dy, v){ var x = stOf(p, dy).slice(); x[i] = v; return x; }; }, one = function(dy, v){ return withVal(p, stOf(p, dy), v); };
    switch (p.k){
      case 'linear': case 'rot': case 'align': return [[p.name, function(dy){ return valOf(p, stOf(p, dy)); }, one, p.vs && p.vs.list, 0]];
      case 'polar': return [[p.name, g(0), s(0), p.vs && p.vs.list, 0], [p.name + ' · góc', g(1), s(1), null, 1]];
      case 'xy': case 'point': return [[p.name + ' X', g(0), s(0), null, 0], [p.name + ' Y', g(1), s(1), null, 1]];
      case 'flip': case 'vis': case 'table': return [[p.name, function(dy){ return curVal(l, p, dy); }, null, menuVals(l, p), 0]];
      case 'lookup': return lkAct(l, p) ? [[p.name, function(dy){ return lkVal(l, p, dy); }, null, menuVals(l, p), 0]] : [];
    }
    return [];
  }
  function selIns(d){ var s = d.sel.map(function(id){ return d.byId[id]; }).filter(Boolean); if (!s.length || s.some(function(e){ return e.t !== 'insert' || e.name !== s[0].name; })) return null; var bd = d.blocks[s[0].name]; return isDyn(bd) ? { s:s, bd:bd } : null; }
  var DG = { col:false };
  function dynGroup(host, d){
    var S = selIns(d), body = $('.pal-body', host); if (!S || !body) return;
    var l = S.bd.ents, lock = locked(l), ro = TC.isViewer(), rows = [];
    pars(l).forEach(function(p){ partsOf(l, p).forEach(function(pt){ var t = S.s.map(function(e){ return fmtV(p, pt[1](e.dyn), pt[4]); }), same = t.every(function(x){ return x === t[0]; }); rows.push({ p:p, pt:pt, txt:same ? t[0] : '*Khác nhau*', ro:ro || lock[p.bk] || (!pt[2] && !(pt[3] && pt[3].length)) }); }); });
    if (!rows.length) return;
    var html = '<button class="pgh" type="button" data-dyng="h" aria-expanded="' + !DG.col + '">' + ic('down', 'xs') + 'Tùy biến</button>' + (DG.col ? '' : rows.map(function(r, i){ var menu = r.pt[3] && r.pt[3].length;
      return '<div class="pr' + (r.ro ? ' ro' : ' edt') + '" data-pk="' + esc(r.pt[0]) + '" data-dyng="' + i + '"' + (lock[r.p.bk] && !ro ? ' data-tip="Đổi qua Bảng thuộc tính (bắt buộc khớp hàng)"' : '') + '><span class="pk">' + esc(r.pt[0]) + '</span><span class="pv' + (typeof r.pt[1]({}) === 'number' && r.p.k !== 'flip' ? ' mono' : '') + '"><span' + (r.txt === '*Khác nhau*' ? ' class="varies"' : '') + '>' + esc(r.txt) + '</span>' + (!r.ro && menu ? '<i class="pdd">' + ic('down', 'xs') + '</i>' : '') + '</span></div>'; }).join(''));
    var at = $$('.pgh', body).filter(function(b){ return b.dataset.pg === 'Attribute'; })[0], box = TC.el('<div class="dyng">' + html + '</div>');
    if (at) body.insertBefore(box, at); else body.appendChild(box);
    host._dyn = { S:S, rows:rows };
  }
  // phím sửa được của tham số, action (hàng do propsExt dựng)
  var EDK = { 'Tên tham số':'name', 'Bảng giá trị':'vs', 'Số grip':'grips', 'Trạng thái':'vis', 'Bảng tra cứu':'lk', 'Số hàng':'tb', 'Tên action':'name', 'Tập đối tượng':'set', 'Khoảng cách bản sao':'gap' };
  function markRows(host, d){ var e = d.sel.length === 1 && d.byId[d.sel[0]]; if (!isPar(e) && !isAct(e) || TC.isViewer()) return; $$('.pr[data-pk]', host).forEach(function(r){ var k = EDK[r.dataset.pk]; if (!k) return; r.classList.add('edt'); r.dataset.dpe = k; if (k === 'grips'){ var pv = $('.pv', r); if (pv) pv.insertAdjacentHTML('beforeend', '<i class="pdd">' + ic('down', 'xs') + '</i>'); } }); }
  var pr0 = P.props.render;
  P.props.render = function(host, d){ pr0.apply(this, arguments); if (!d) return; if (!host._dynh){ host._dynh = 1; host.addEventListener('click', propClick, true); } dynGroup(host, d); markRows(host, d); };
  function inline(row, val, ok, mono){ var pv = $('.pv', row), inp = TC.el('<input class="pinp' + (mono ? ' mono' : '') + '" spellcheck="false">'); inp.value = val; pv.innerHTML = ''; pv.appendChild(inp); inp.focus(); inp.select();
    var done = false, fin = function(go){ if (done) return; done = true; if (!go){ A().refreshPalettes(); return; } var err = ok(inp.value.trim()); if (err){ done = false; inp.classList.add('err'); TC.ctip($('#cv'), 40, 40, err); inp.focus(); } };
    inp.addEventListener('keydown', function(e){ e.stopPropagation(); if (e.key === 'Enter'){ e.preventDefault(); fin(true); } else if (e.key === 'Escape'){ e.preventDefault(); fin(false); } });
    inp.addEventListener('blur', function(){ setTimeout(function(){ fin(true); }, 0); }); }
  function propClick(ev){
    var host = this, d = D(); if (!d) return;
    var h = ev.target.closest('[data-dyng]');
    if (h){ ev.stopPropagation(); if (h.dataset.dyng === 'h'){ DG.col = !DG.col; P.props.render(host, d); return; } var R = host._dyn && host._dyn.rows[Number(h.dataset.dyng)]; if (!R) return;
      if (R.ro){ TC.copyText && TC.copyText(R.txt); return; }
      var S = host._dyn.S, l = S.bd.ents, p = R.p, pt = R.pt, set = function(v){ ED.run('Sửa ' + pt[0].toLowerCase(), function(){ S.s.forEach(function(e){ ED.mod(e, function(x){ if (pt[2]) setDyn(x, p.bk, pt[2](x.dyn, pt[4] || p.k === 'xy' || p.k === 'point' ? v : snapVs(p, v))); else setVal(l, p, x, v); }); }); }); };
      if (pt[3] && pt[3].length){ var r = $('.pv', h).getBoundingClientRect(), cur = S.s.length === 1 ? pt[1](S.s[0].dyn) : null; TC.menu(pt[3].map(function(v){ return { label:esc(fmtV(p, v, pt[4])), check:cur != null && eqV(v, cur), act:function(){ set(v); } }; }), r.left, r.bottom + 1, { minWidth:Math.max(160, r.width) }); return; }
      var ang = p.k === 'rot' || p.k === 'align' || (p.k === 'polar' && pt[4] === 1);
      inline(h, R.txt === '*Khác nhau*' ? '' : R.txt.replace(/°$/, ''), function(v){ var n = TC.parseNum(v, ang ? 'deg' : 'len'); if (n.err) return n.err; if ((p.k === 'linear' || (p.k === 'polar' && !pt[4])) && n.v <= 0) return pt[0] + ' phải lớn hơn 0.'; set(n.v); }, true);
      return; }
    var r2 = ev.target.closest('.pr[data-dpe]'); if (!r2) return; ev.stopPropagation(); editPar(r2, d);
  }
  function editPar(row, d){
    var e = d.byId[d.sel[0]], k = row.dataset.dpe; if (!e) return;
    if (k === 'name') return inline(row, e.name, function(v){ if (!v) return 'Nhập tên.'; if (v !== e.name && d.ents.some(function(x){ return x !== e && (isPar(x) || isAct(x)) && x.name === v; })) return 'Đã có tham số hoặc action tên “' + v + '”.'; ED.run('Đổi tên', function(){ ED.mod(e, function(x){ x.name = v; }); }); });
    if (k === 'grips'){ var r = $('.pv', row).getBoundingClientRect(), mx = e.k === 'linear' ? 2 : 1; TC.menu([0, 1, 2].slice(0, mx + 1).map(function(n){ return { label:n + ' grip', check:(e.grips == null ? 1 : e.grips) === n, act:function(){ ED.run('Số grip', function(){ ED.mod(e, function(x){ x.grips = n; }); }); } }; }), r.left, r.bottom + 1, { minWidth:Math.max(140, r.width) }); return; }
    if (k === 'vs') return vsDialog(e);
    if (k === 'vis') return TC.cmd.run('BVSTATE');
    if (k === 'tb') return TC.cmd.run('BTABLE');
    if (k === 'lk'){ var a = isAct(e) ? e : lkAct(d.ents, e); if (a) lkDialog(a); else TC.cmd.run('BACTION', { par:e, k:'lookup' }); return; }
    if (k === 'set') return TC.cmd.run('BACTION', { edit:e });
    if (k === 'gap') return inline(row, TC.d(Array.isArray(e.gap) ? e.gap[0] : e.gap || 0), function(v){ var n = TC.parseNum(v, 'len'); if (n.err) return n.err; if (n.v <= 0) return 'Khoảng cách phải lớn hơn 0.'; ED.run('Khoảng cách bản sao', function(){ ED.mod(e, function(x){ x.gap = Array.isArray(x.gap) ? [n.v, x.gap[1]] : n.v; }); }); }, true);
  }
  function vsText(p){ var vs = p.vs; return !vs ? 'Không giới hạn' : vs.list ? vs.list.map(function(v){ return TC.d(v, v % 1 ? 2 : 0); }).join(', ') : 'Bước ' + TC.d(vs.inc, 0) + (vs.min != null ? ': ' + TC.d(vs.min, 0) + '–' + TC.d(vs.max, 0) : ''); }
  TC.propsExt.bparam = function(d, e, f){
    var l = d.ents, r = [['Tên tham số', esc(e.name), 'html'], ['Kiểu tham số', esc(KP[e.k][0]), 'html']], as = acts(l).filter(function(a){ return a.par === e.bk; });
    if (e.k === 'linear' || e.k === 'polar') r.push(['Khoảng cách', f(G.dist(e.p1, e.p2)), 'ro']);
    if (e.k === 'polar' || e.k === 'rot') r.push(['Góc mặc định', f(st0(e)[1] != null ? st0(e)[1] : st0(e), 0) + '°', 'ro']);
    if (/^(linear|polar|rot)$/.test(e.k)) r.push(['Bảng giá trị', esc(vsText(e)), 'html']);
    if (e.k === 'vis') r.push(['Trạng thái', esc(e.states.map(function(s){ return s.n; }).join(', ')), 'html']);
    if (e.k === 'lookup'){ var la = lkAct(l, e); r.push(['Bảng tra cứu', la && la.tbl ? la.tbl.rows.length + ' hàng' : '<span class="dwarn">' + ic('warn', 'xs') + 'Chưa có</span>', 'html']); }
    if (e.k === 'table') r.push(['Số hàng', String((e.rows || []).length), 'html'], ['Khớp hàng bắt buộc', e.strict ? 'Có' : 'Không', 'html']);
    if (e.k !== 'base') r.push(['Số grip', String(e.grips == null ? 1 : e.grips), 'html']);
    if (FOR[e.k]) r.push(['Action', as.length ? esc(as.map(function(a){ return a.name; }).join(', ')) : '<span class="dwarn">' + ic('warn', 'xs') + 'Chưa có action</span>', 'html']);
    return r;
  };
  TC.propsExt.baction = function(d, e){
    var p = parBy(d.ents, e.par), r = [['Tên action', esc(e.name), 'html'], ['Kiểu action', esc(KA[e.k][0]), 'html'], ['Tham số', p ? esc(p.name) : '<span class="dwarn">' + ic('warn', 'xs') + 'Mất tham số</span>', 'html']];
    if (p && p.k === 'linear' && e.k !== 'lookup') r.push(['Điểm tham số', e.pt === 'p1' ? 'Điểm đầu' : 'Điểm cuối', 'html']);
    if (e.k === 'lookup') r.push(['Bảng tra cứu', e.tbl ? e.tbl.rows.length + ' hàng' : 'Chưa có', 'html']);
    else r.push(['Tập đối tượng', (e.sel || []).length ? (e.sel || []).length + ' đối tượng' : '<span class="dwarn">' + ic('warn', 'xs') + 'Chưa có</span>', 'html']);
    if (e.frame) r.push(['Khung kéo giãn', 'Có', 'html']);
    if (e.k === 'array') r.push(['Khoảng cách bản sao', TC.d(Array.isArray(e.gap) ? e.gap[0] : e.gap || 0), 'html']);
    return r;
  };

  /* ================= lệnh: thêm tham số (BPARAMETER), action (BACTION) ================= */
  function inBE(ctx){ var d = D(); if (d.bedit) return d; ctx.out(d.btest ? 'Đang thử block: đóng cửa sổ thử trước.' : 'Lệnh này dùng trong Block Editor. Mở Block Editor bằng BEDIT.'); return null; }
  function symH(){ return 9.5 / (V().vw().s || 1); } // chữ nhãn cao khoảng 12 px ở mức zoom lúc tạo
  var LASTP = 'linear', LASTA = null;
  TC.on('cmdend', function(n){ if (/^(BPARAMETER|BACTION|BTABLE|BVHIDE|BVSHOW|BTESTBLOCK)$/.test(n)){ V().temp = []; V().drawOver(); } }); // bỏ hình xem trước còn lại sau lần bấm cuối
  IMPL.BPARAMETER = function(ctx, pre){
    var d = inBE(ctx); if (!d) return; var set = pre && pre.set, k0 = typeof pre === 'string' ? pre : pre && pre.k;
    if (k0 && KP[k0] && set){ ED.begin('Bộ tham số'); return addPar(ctx, d, k0, set).then(function(r){ ED.commit(); return r; }, function(err){ ED.commit(); throw err; }); } // tham số và action: một bước hoàn tác
    if (k0 && KP[k0]) return addPar(ctx, d, k0);
    return ctx.kw('Loại tham số', PKS.map(function(k){ return [KP[k][0], KP[k][1]]; }), KP[LASTP][1]).then(function(v){ var k = PKS.filter(function(x){ return KP[x][1] === String(v).toUpperCase(); })[0]; if (k) return addPar(ctx, d, k); if (v) ctx.out('<span class="err">Không có loại tham số “' + esc(v) + '”.</span>'); });
  };
  function addPar(ctx, d, k, set){
    LASTP = k; var nm = uName(d, KP[k][3]), h = symH(), o = { t:'bparam', k:k, name:nm, h:h, layer:'0' };
    if (k === 'vis' && visPar(d.ents)){ ctx.out('Block đã có tham số hiển thị. Thêm, đổi tên trạng thái bằng Quản lý trạng thái (BVSTATE).'); return; }
    var pt = function(text, op){ return ctx.point(text, Object.assign({ opts:[['Nhãn', 'L']] }, op || {})).then(function(r){ if (r.kw !== 'L') return r; return ctx.val('Nhãn tham số', nm, { allowSpace:true }).then(function(v){ if (v && v.trim()) nm = o.name = v.trim(); return pt(text, op); }); }); };
    var prev = function(f){ return function(q, temp){ f(q); temp.push(ED.temp([ED.clean(o)])); }; };
    var fin = function(){ var e; o.name = nm; ED.run('BPARAMETER', function(){ o.bk = newKey(d, 'p'); e = ED.add(o); }); if (k === 'vis') d.bedit.vs = o.states[0].n;
      ctx.out('Đã thêm tham số ' + KP[k][0].toLowerCase() + ' <b>' + esc(nm) + '</b>.' + (FOR[k] && !set ? ' Gắn action để tham số có tác dụng: Action (BACTION) hoặc palette Tạo block.' : k === 'vis' ? ' Thêm trạng thái bằng BVSTATE, ẩn hiện đối tượng bằng BVHIDE, BVSHOW.' : ''));
      if (set) return actFlow(ctx, d, e, set, true); };
    var two = function(t1, t2, t3, f){ return pt(t1).then(function(a){ if (!a.p) return; o.p1 = a.p; return ctx.point(t2, { base:a.p }).then(function(b){ if (!b.p || G.dist(a.p, b.p) < 1e-6) return; o.p2 = b.p; if (!t3) return fin();
      return ctx.point(t3, { rubber:false, onMove:prev(f) }).then(function(c){ if (!c.p) return; f(c.p); return fin(); }); }); }); };
    switch (k){
      case 'linear': o.off = 0; o.grips = 2; return two('Điểm đầu', 'Điểm cuối', 'Vị trí nhãn', function(q){ o.off = offOf(o, q); });
      case 'flip': return two('Điểm đầu của đường lật', 'Điểm cuối của đường lật', 'Vị trí nhãn và grip', function(q){ o.lab = q; });
      case 'polar': return two('Điểm gốc', 'Điểm cuối', 'Vị trí nhãn', function(q){ o.lab = q; });
      case 'xy': return two('Điểm gốc', 'Góc đối diện');
      case 'rot': return pt('Điểm gốc').then(function(a){ if (!a.p) return; o.p = a.p; return ctx.point('Bán kính và góc mặc định', { base:a.p }).then(function(b){ if (!b.p || G.dist(a.p, b.p) < 1e-6) return; o.p2 = b.p; return ctx.point('Vị trí nhãn', { rubber:false, onMove:prev(function(q){ o.lab = q; }) }).then(function(c){ if (!c.p) return; o.lab = c.p; return fin(); }); }); });
      case 'align': return pt('Điểm gốc của đường gióng').then(function(a){ if (!a.p) return; o.p = a.p; return ctx.point('Hướng gióng', { base:a.p }).then(function(b){ if (!b.p || G.dist(a.p, b.p) < 1e-6) return; o.p2 = b.p; return fin(); }); });
      case 'point': return pt('Vị trí tham số').then(function(a){ if (!a.p) return; o.p = a.p; return ctx.point('Vị trí nhãn', { base:a.p, onMove:prev(function(q){ o.lab = q; }) }).then(function(c){ if (!c.p) return; o.lab = c.p; return fin(); }); });
      default: // vis, lookup, base: một điểm
        if (k === 'vis') o.states = [{ n:'Trạng thái 1', hide:[] }];
        return pt(k === 'base' ? 'Vị trí điểm gốc' : 'Vị trí tham số', { onMove:prev(function(q){ o.p = q; }) }).then(function(a){ if (!a.p) return; o.p = a.p; return fin(); });
    }
  }
  function frameOf(ctx){ return ctx.point('Góc thứ nhất của khung kéo giãn').then(function(a){ if (!a.p) return null; return ctx.point('Góc đối diện', { base:a.p, rubber:false, onMove:function(q, temp){ temp.push({ k:'poly', pts:rect(a.p, q), closed:true, dash:[6, 4], color:cv('--c-sel-c'), fill:cv('--c-sel-cf') }); } }).then(function(b){ return b.p ? rect(a.p, b.p) : null; }); }); }
  function autoPos(d, p){ var h = hs(p), n = acts(d.ents).filter(function(a){ return a.par === p.bk; }).length, b = labOf(p).p;
    if (p.k !== 'linear') return add2(b, [h * 1.2, -h * (1.8 + n * 2.2)]); var u = unit(sub2(p.p2, p.p1)), sg = (p.off || 0) < 0 ? -1 : 1; return add2(add2(b, mul2([-u[1], u[0]], sg * h * 2)), mul2(u, (n - .5) * h * 3.6)); } // tham số thẳng: hàng action ngoài nhãn
  function actFlow(ctx, d, p, k, opt){
    var a = { t:'baction', k:k, par:p.bk, sel:[], name:uName(d, KA[k][0]), h:p.h, layer:'0' }, ids = [], chain = Promise.resolve();
    if (p.k === 'linear' && k !== 'lookup') chain = chain.then(function(){ return ctx.kw('Điểm tham số gắn với action', [['Điểm đầu', 'S'], ['Điểm cuối', 'E']], 'E').then(function(v){ a.pt = v === 'S' ? 'p1' : 'p2'; }); });
    if (k === 'stretch' || k === 'pstretch') chain = chain.then(function(){ return frameOf(ctx).then(function(F){ if (!F) throw 'cancel'; a.frame = F; }); });
    if (k !== 'lookup') chain = chain.then(function(){ TC.cad.clearSel(); return ctx.sel(opt ? 'Chọn đối tượng cho action ' + a.name + ' (Enter để chọn sau)' : 'Chọn đối tượng cho action ' + a.name).then(function(r){ ids = r.filter(function(id){ return !isAct(d.byId[id]) && id !== p.id; }); }); });
    if (k === 'array') chain = chain.then(function(){ var b = ED.bbox(ids), gx = isFinite(b[0]) ? b[2] - b[0] : 100, gy = isFinite(b[0]) ? b[3] - b[1] : 100;
      if (p.k === 'xy') return ctx.val('Khoảng cách cột', TC.d(gx)).then(function(v){ var x = TC.parseNum(v, 'len'); return ctx.val('Khoảng cách hàng', TC.d(gy)).then(function(w){ var y = TC.parseNum(w, 'len'); a.gap = [x.err || x.v <= 0 ? gx : x.v, y.err || y.v <= 0 ? gy : y.v]; }); });
      return ctx.val('Khoảng cách giữa các bản sao', TC.d(gx)).then(function(v){ var x = TC.parseNum(v, 'len'); a.gap = x.err || x.v <= 0 ? gx : x.v; }); });
    return chain.then(function(){ return ctx.point('Vị trí action', { def:'đặt cạnh tham số', rubber:false, onMove:function(q, temp){ a.p = q; temp.push(ED.temp([ED.clean(a)])); } }); }).then(function(r){
      a.p = r.p || autoPos(d, p); var made;
      ED.run('BACTION', function(){ a.sel = ids.map(function(id){ var e = d.byId[id]; return e && keyOf(d, e); }).filter(Boolean); a.bk = newKey(d, 'a'); made = ED.add(a); });
      TC.cad.clearSel(); V().temp = [];
      if (k === 'lookup'){ ctx.out('Đã thêm action tra cứu <b>' + esc(a.name) + '</b>. Điền bảng tra cứu: mỗi hàng là một tổ hợp giá trị và giá trị tra cứu.'); return lkDialog(made); }
      ctx.out('Đã thêm action ' + KA[k][0].toLowerCase() + ' <b>' + esc(a.name) + '</b> cho tham số <b>' + esc(p.name) + '</b>' + (a.sel.length ? ' · ' + a.sel.length + ' đối tượng.' : '. Chưa có đối tượng: nhấp đúp action để chọn.')); });
  }
  IMPL.BACTION = function(ctx, pre){
    var d = inBE(ctx); if (!d) return;
    if (pre && pre.edit) return editSet(ctx, d, pre.edit);
    var kind = typeof pre === 'string' ? pre : pre && pre.k, s1 = d.sel.length === 1 && d.byId[d.sel[0]], par = pre && pre.par || (isPar(s1) ? s1 : null);
    var pick = function(){ return ED.pick(ctx, 'Chọn tham số' + (kind ? ' cho action ' + KA[kind][0].toLowerCase() : ''), { types:['bparam'], bad:'Chọn một tham số (ký hiệu màu xanh lá trong Block Editor)' }).then(function(r){ if (!r || !r.e) return null; var ok = FOR[r.e.k] || [];
      if (!ok.length || (kind && ok.indexOf(kind) < 0)){ TC.ctip($('#cv'), r.s[0], r.s[1], !ok.length ? (r.e.k === 'vis' ? 'Tham số hiển thị không cần action: dùng Ẩn, Hiện đối tượng' : 'Tham số ' + KP[r.e.k][0].toLowerCase() + ' không cần action') : 'Action ' + KA[kind][0].toLowerCase() + ' không dùng với tham số ' + KP[r.e.k][0].toLowerCase()); return pick(); } return r.e; }); };
    return (par ? Promise.resolve(par) : pick()).then(function(p){ if (!p) return; var ok = FOR[p.k] || []; if (!ok.length){ ctx.out('Tham số ' + KP[p.k][0].toLowerCase() + ' không cần action.'); return; }
      var kk = kind && ok.indexOf(kind) >= 0 ? Promise.resolve(kind) : ok.length === 1 ? Promise.resolve(ok[0]) : ctx.kw('Loại action', ok.map(function(k){ return [KA[k][0], KA[k][1]]; }), KA[ok.indexOf(LASTA) >= 0 ? LASTA : ok[0]][1]).then(function(v){ return ok.filter(function(k){ return KA[k][1] === String(v).toUpperCase(); })[0]; });
      return kk.then(function(k){ if (!k) return; LASTA = k; TC.cad.clearSel(); return actFlow(ctx, d, p, k); }); });
  };
  // sửa tập đối tượng của action (nhấp đúp action): tập mới thay tập cũ
  function editSet(ctx, d, a){ var keys = a.sel || [], hi = function(){ d.found = d.ents.filter(function(x){ return x.bk && keys.indexOf(x.bk) >= 0; }).map(function(x){ return x.id; }); V().drawOver(); };
    if (a.k === 'lookup') return lkDialog(a);
    var fr = a.frame ? ctx.kw('Khung kéo giãn', [['Giữ khung', 'K'], ['Vẽ khung mới', 'N']], 'K').then(function(k){ return k === 'N' ? frameOf(ctx) : a.frame; }) : Promise.resolve(null);
    TC.cad.clearSel(); hi();
    return fr.then(function(F){ TC.cad.clearSel(); hi(); return ctx.sel('Chọn đối tượng cho action ' + a.name + ' (tập mới thay tập cũ, Enter giữ nguyên)').then(function(ids){ d.found = []; ids = ids.filter(function(id){ return !isAct(d.byId[id]) && d.byId[id] !== parBy(d.ents, a.par); });
      if (!ids.length && (!F || F === a.frame)){ ctx.out('Giữ nguyên tập đối tượng của ' + esc(a.name) + '.'); V().drawOver(); return; }
      ED.run('Sửa tập đối tượng', function(){ var sel = ids.length ? ids.map(function(id){ return keyOf(d, d.byId[id]); }) : a.sel; ED.mod(a, function(x){ x.sel = sel; if (F) x.frame = F; }); });
      TC.cad.clearSel(); ctx.out('Action <b>' + esc(a.name) + '</b>: ' + a.sel.length + ' đối tượng' + (F && F !== a.frame ? ', khung mới.' : '.')); }); });
  }

  /* ================= trạng thái hiển thị: BVSTATE, BVHIDE, BVSHOW ================= */
  IMPL.BVSTATE = function(ctx){ var d = inBE(ctx); if (!d) return; var vp = visPar(d.ents);
    if (!vp) return TC.confirm({ title:'Trạng thái hiển thị', msg:'Block <b>' + esc(d.bedit.name) + '</b> chưa có tham số hiển thị.', detail:'Thêm tham số hiển thị rồi tạo các trạng thái, ví dụ: Mở 90°, Mở 45°, Đóng.', ok:'Thêm tham số hiển thị' }).then(function(y){ if (y) setTimeout(function(){ TC.cmd.run('BPARAMETER', 'vis'); }, 0); });
    return bvDialog(d, vp); };
  function geoOf(d){ return d.ents.filter(function(x){ return !isPar(x) && !isAct(x); }); }
  function bvDialog(d, vp){
    var S = vp.states.map(function(s){ return { n:s.n, o:s.n, hide:(s.hide || []).slice() }; }), cur = visSt(vp, d.bedit.vs).n, sel = cur, all = geoOf(d);
    var B = [['new', 'plus', 'Mới…'], ['ren', 'attedit', 'Đổi tên…'], ['del', 'trash', 'Xóa'], ['up', 'up', 'Lên'], ['dn', 'down', 'Xuống'], ['cur', 'check', 'Đặt hiện hành']];
    return new Promise(function(res){
      var dlg = TC.dialog({ id:'bvstate', title:'Trạng thái hiển thị', cmd:'BVSTATE', code:'UI-G04', w:540, onClose:function(){ res(); },
        body:'<div class="stack" style="gap:8px"><div class="bvw"><div class="tblwrap bvl" id="bv-l" role="listbox" tabindex="0" aria-label="Trạng thái hiển thị"></div><div class="stack ds-btns" style="gap:6px">' + B.map(function(b){ return '<button class="btn" type="button" data-bv="' + b[0] + '">' + ic(b[1]) + b[2] + '</button>'; }).join('') + '</div></div>' +
          '<div class="note">' + ic('info') + '<span>Trạng thái đầu danh sách dùng khi chèn block. Ẩn, hiện đối tượng cho trạng thái hiện hành bằng BVHIDE, BVSHOW.</span></div></div>',
        foot:'<span class="faint" style="font-size:var(--fs-s)">' + all.length + ' đối tượng trong block</span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="bv-ok"></button>' });
      var L = $('#bv-l', dlg.el), vis = function(s){ return s.all ? 0 : all.filter(function(x){ return !(x.bk && s.hide.indexOf(x.bk) >= 0); }).length; };
      function render(){ var i = S.map(function(s){ return s.n; }).indexOf(sel);
        L.innerHTML = S.map(function(s, k){ return '<button class="bvr" type="button" role="option" data-i="' + k + '" aria-selected="' + (s.n === sel) + '"><span class="nm">' + esc(s.n) + '</span>' + (k === 0 ? '<span class="tag">Mặc định</span>' : '') + (s.n === cur ? '<span class="tag acc">Hiện hành</span>' : '') + '<span class="faint">' + vis(s) + ' hiện</span></button>'; }).join('');
        $('[data-bv="del"]', dlg.el).disabled = S.length < 2; $('[data-bv="up"]', dlg.el).disabled = i <= 0; $('[data-bv="dn"]', dlg.el).disabled = i >= S.length - 1; $('[data-bv="cur"]', dlg.el).disabled = sel === cur;
        $('#bv-ok', dlg.el).textContent = 'Lưu ' + S.length + ' trạng thái'; }
      var check = function(v, old){ return !v ? 'Nhập tên trạng thái.' : S.some(function(s){ return s.n === v && s.n !== old; }) ? 'Đã có trạng thái “' + v + '”.' : null; };
      L.addEventListener('click', function(e){ var b = e.target.closest('[data-i]'); if (!b) return; sel = S[Number(b.dataset.i)].n; render(); });
      L.addEventListener('dblclick', function(e){ if (e.target.closest('[data-i]')){ cur = sel; render(); } });
      TC.listKeys(L, '[data-i]', function(b, enter){ sel = S[Number(b.dataset.i)].n; if (enter) cur = sel; render(); var n = $('[aria-selected="true"]', L); if (n) n.focus(); });
      dlg.el.addEventListener('click', function(e){ var b = e.target.closest('[data-bv]'); if (!b || b.disabled) return; var k = b.dataset.bv, i = S.map(function(s){ return s.n; }).indexOf(sel), s = S[i];
        if (k === 'new') return bvNew(S, check, s).then(function(r){ if (!r) return; S.push(r); sel = r.n; render(); });
        if (k === 'ren') return SC.ask('Đổi tên trạng thái', 'Tên mới', s.n, function(v){ return check(v, s.n); }, 'UI-G04').then(function(v){ if (!v) return; if (cur === s.n) cur = v; s.n = sel = v; render(); });
        if (k === 'del'){ S.splice(i, 1); if (cur === s.n) cur = S[0].n; sel = S[Math.min(i, S.length - 1)].n; }
        if (k === 'up' && i > 0) S.splice(i - 1, 0, S.splice(i, 1)[0]);
        if (k === 'dn' && i < S.length - 1) S.splice(i + 1, 0, S.splice(i, 1)[0]);
        if (k === 'cur') cur = sel;
        render(); });
      $('#bv-ok', dlg.el).addEventListener('click', function(){
        ED.run('BVSTATE', function(){ var keys = S.some(function(s){ return s.all; }) ? all.map(function(x){ return keyOf(d, x); }) : null;
          ED.mod(vp, function(x){ var was = Object.assign({}, x.was); S.forEach(function(s){ if (s.o && s.o !== s.n) was[s.o] = s.n; }); x.states = S.map(function(s){ return { n:s.n, hide:s.all ? keys.slice() : s.hide }; }); x.was = was; }); });
        dlg.close(true); setVs(d, cur); TC.cmdline.msg('Đã lưu ' + S.length + ' trạng thái hiển thị; hiện hành: <b>' + esc(cur) + '</b>.'); });
      render(); setTimeout(function(){ L.focus(); }, 0);
    });
  }
  function bvNew(S, check, from){
    return new Promise(function(res){ var n = 1; while (S.some(function(s){ return s.n === 'Trạng thái ' + n; })) n++;
      var dlg = TC.dialog({ id:'bvnew', title:'Trạng thái hiển thị mới', code:'UI-G04', w:420, help:false, onClose:function(r){ if (r !== true) res(null); },
        body:'<div class="stack"><div class="field"><label for="bvn-n">Tên trạng thái</label><input class="inp" id="bvn-n" value="Trạng thái ' + n + '" spellcheck="false"><span class="fmsg" id="bvn-m"></span></div>' + H.grp('Đối tượng có sẵn trong trạng thái mới', H.rad('bvn', 'hide', 'Ẩn tất cả', false) + H.rad('bvn', 'show', 'Hiện tất cả', false) + H.rad('bvn', 'keep', 'Giữ như trạng thái “' + esc(from.n) + '”', true)) + '</div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="bvn-ok">Tạo trạng thái</button>' });
      setTimeout(function(){ $('#bvn-n', dlg.el).select(); }, 0);
      $('#bvn-ok', dlg.el).addEventListener('click', function(){ var v = $('#bvn-n', dlg.el).value.trim(), err = check(v), m = $('#bvn-m', dlg.el); if (err){ m.className = 'fmsg err'; m.innerHTML = ic('warn', 'xs') + esc(err); $('#bvn-n', dlg.el).classList.add('err'); return; }
        var o = $('[name="bvn"]:checked', dlg.el).value; dlg.close(true); res({ n:v, hide:o === 'keep' ? from.hide.slice() : [], all:o === 'hide' }); });
    });
  }
  function bvToggle(ctx, hide){
    var d = inBE(ctx); if (!d) return; var vp = visPar(d.ents); if (!vp){ ctx.out('Block chưa có tham số hiển thị. Thêm bằng Tham số › Hiển thị (BPARAMETER V).'); return; }
    var cur = visSt(vp, d.bedit.vs).n; if (!hide){ BV.show = true; V().draw(); }
    var end = function(){ BV.show = false; V().draw(); };
    return ctx.sel((hide ? 'Chọn đối tượng cần ẩn' : 'Chọn đối tượng cần hiện') + ' ở trạng thái ' + cur).then(function(ids){ ids = ids.filter(function(id){ var e = d.byId[id]; return e && !isPar(e) && !isAct(e); }); if (!ids.length){ ctx.out('Chọn hình học của block (tham số, action luôn hiện).'); return; }
      return ctx.kw((hide ? 'Ẩn' : 'Hiện') + ' trong', [['Trạng thái hiện hành', 'C'], ['Tất cả trạng thái', 'A']], 'C').then(function(k){
        ED.run(hide ? 'BVHIDE' : 'BVSHOW', function(){ var keys = ids.map(function(id){ return keyOf(d, d.byId[id]); }); ED.mod(vp, function(x){ x.states.forEach(function(s){ if (k !== 'A' && s.n !== cur) return; s.hide = (s.hide || []).filter(function(q){ return keys.indexOf(q) < 0; }); if (hide) s.hide = s.hide.concat(keys); }); }); });
        TC.cad.clearSel(); ctx.out('Đã ' + (hide ? 'ẩn ' : 'hiện ') + ids.length + ' đối tượng ở ' + (k === 'A' ? 'mọi trạng thái' : 'trạng thái <b>' + esc(cur) + '</b>') + '.'); }); }).then(end, function(err){ end(); throw err; });
  }
  IMPL.BVHIDE = function(ctx){ return bvToggle(ctx, true); };
  IMPL.BVSHOW = function(ctx){ return bvToggle(ctx, false); };

  /* ================= bảng thuộc tính block (BTABLE), bảng tra cứu: kiểm tra thiếu, trùng, sai giá trị ================= */
  function canCol(p){ return /^(linear|polar|rot|align|flip|vis)$/.test(p.k); }
  function colOf(p){ var c = { p:p, num:false, opts:null }; if (p.k === 'flip') c.opts = [[0, 'Không lật'], [1, 'Lật']]; else if (p.k === 'vis') c.opts = p.states.map(function(s){ return [s.n, s.n]; }); else if (p.vs && p.vs.list) c.opts = p.vs.list.map(function(v){ return [v, TC.d(v)]; }); else c.num = true; return c; }
  function tblDialog(o){
    var l = o.L, cols = o.cols.filter(function(bk){ return parBy(l, bk); }), nm = !!o.name, out = !!o.out, off = nm ? 1 : 0, cur = -1;
    var rows = o.rows.map(function(r){ return r.slice(); }), chk = !!(o.chk && o.chk.on);
    return new Promise(function(res){
      var dlg = TC.dialog({ id:'btbl', title:o.title, cmd:o.cmd, code:o.code, w:o.w || 660, onClose:function(){ res(); },
        body:'<div class="stack" style="gap:8px"><div class="hrow"><button class="btn sm" type="button" id="tb-col" aria-haspopup="menu">' + ic('plus') + 'Thêm cột</button><button class="btn sm" type="button" id="tb-add">' + ic('plus') + 'Thêm hàng</button><button class="btn sm" type="button" id="tb-del">' + ic('trash') + 'Xóa hàng</button><button class="ib" type="button" id="tb-up" aria-label="Chuyển hàng lên" data-tip="Chuyển hàng lên">' + ic('up') + '</button><button class="ib" type="button" id="tb-dn" aria-label="Chuyển hàng xuống" data-tip="Chuyển hàng xuống">' + ic('down') + '</button><span class="grow"></span><span class="faint" id="tb-n" style="font-size:var(--fs-s)"></span></div>' +
          '<div class="tblwrap dbt-w"><table class="tbl dbt" id="tb-t"></table></div><div id="tb-v"></div>' + (o.chk ? H.chk('tb-chk', o.chk.label, chk) : '') + (o.note ? '<p class="faint" style="font-size:var(--fs-s);margin:0">' + o.note + '</p>' : '') + '</div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="tb-ok"></button>' });
      var t = $('#tb-t', dlg.el), C = function(j){ return j - off >= 0 && j - off < cols.length ? colOf(parBy(l, cols[j - off])) : null; }, cname = function(j){ var c = C(j); return c ? c.p.name : nm && j === 0 ? 'Tên hàng' : o.out; }, width = function(){ return off + cols.length + (out ? 1 : 0); };
      function cellHTML(r, j){ var v = rows[r][j], c = C(j), a = ' data-r="' + r + '" data-j="' + j + '" aria-label="Hàng ' + (r + 1) + ', ' + esc(cname(j)) + '"';
        if (c && c.opts) return '<select class="inp sm"' + a + '><option value="">—</option>' + c.opts.map(function(x, i){ return '<option value="' + i + '"' + (v !== '' && v != null && eqV(x[0], v) ? ' selected' : '') + '>' + esc(x[1]) + '</option>'; }).join('') + '</select>';
        return '<input class="inp sm' + (c ? ' mono' : '') + '"' + a + ' value="' + esc(v == null ? '' : typeof v === 'number' ? TC.d(v, v % 1 ? 2 : 0) : v) + '" spellcheck="false">'; }
      function render(){
        t.innerHTML = '<colgroup><col style="width:34px">' + Array.from({ length:width() }, function(){ return '<col>'; }).join('') + '</colgroup><thead><tr><th class="ctr">#</th>' + (nm ? '<th>Tên hàng</th>' : '') + cols.map(function(bk, i){ var p = parBy(l, bk); return '<th>' + esc(p.name) + ' <button class="ib sm" type="button" data-rmc="' + i + '" aria-label="Bỏ cột ' + esc(p.name) + '" data-tip="Bỏ cột">' + ic('x', 'xs') + '</button></th>'; }).join('') + (out ? '<th>' + esc(o.out) + ' <span class="faint">· tra cứu</span></th>' : '') + '</tr></thead>' +
          '<tbody>' + (rows.length ? rows.map(function(r, ri){ return '<tr' + (ri === cur ? ' class="is-sel"' : '') + '><td class="num ctr" data-sr="' + ri + '" title="Chọn hàng">' + (ri + 1) + '</td>' + r.map(function(v, j){ return '<td>' + cellHTML(ri, j) + '</td>'; }).join('') + '</tr>'; }).join('') : '<tr><td colspan="' + (width() + 1) + '" style="height:auto"><div class="empty" style="padding:14px">' + ic('table') + '<b>Bảng chưa có hàng</b>Bấm Thêm hàng, rồi chọn giá trị cho từng cột.</div></td></tr>') + '</tbody>';
        check(); }
      function check(){ var errs = [], bad = {}, mark = function(r, j){ bad[r + ':' + j] = 1; };
        if (!cols.length) errs.push('Thêm ít nhất một cột tham số (Thêm cột).');
        rows.forEach(function(r, ri){ var miss = []; r.forEach(function(v, j){ var c = C(j); if (v === '' || v == null || (typeof v === 'number' && isNaN(v))){ mark(ri, j); miss.push(cname(j)); return; }
          if (c && c.num && ((c.p.k === 'linear' || c.p.k === 'polar') && v <= 0 || c.p.vs && c.p.vs.inc && snapVs(c.p, v) !== v)){ mark(ri, j); errs.push('Hàng ' + (ri + 1) + ': ' + TC.d(v) + ' không hợp lệ cho ' + c.p.name + (c.p.vs ? ' (' + vsText(c.p).toLowerCase() + ')' : ' (phải lớn hơn 0)') + '.'); } });
          if (miss.length) errs.push('Hàng ' + (ri + 1) + ': thiếu ' + miss.join(', ') + '.'); });
        var seen = {}, dupe = function(get, js, msg){ var m = {}; rows.forEach(function(r, ri){ var k = get(r); if (k == null) return; if (m[k] != null){ errs.push(msg(m[k], ri, k)); js.forEach(function(j){ mark(m[k], j); mark(ri, j); }); } else m[k] = ri; }); };
        var vj = cols.map(function(_, i){ return i + off; }), full = function(r){ return vj.some(function(j){ return r[j] === '' || r[j] == null; }) ? null : vj.map(function(j){ return String(r[j]); }).join('|'); };
        if (cols.length) dupe(full, vj, function(a, b){ return 'Hàng ' + (a + 1) + ' và ' + (b + 1) + ' trùng giá trị' + (out ? ' vào' : '') + '.'; });
        if (nm) dupe(function(r){ return r[0] || null; }, [0], function(a, b, k){ return 'Tên hàng “' + k + '” trùng (hàng ' + (a + 1) + ', ' + (b + 1) + ').'; });
        if (out && chk) dupe(function(r){ return r[r.length - 1] || null; }, [width() - 1], function(a, b, k){ return 'Giá trị tra cứu “' + k + '” trùng (hàng ' + (a + 1) + ', ' + (b + 1) + '): không tra ngược được.'; });
        if (!rows.length) errs.push('Thêm ít nhất một hàng.');
        $$('[data-r]', t).forEach(function(x){ x.parentNode.classList.toggle('err', !!bad[x.dataset.r + ':' + x.dataset.j]); });
        $('#tb-v', dlg.el).innerHTML = errs.length ? '<div class="note warn">' + ic('warn') + '<span>' + errs.slice(0, 4).map(esc).join('<br>') + (errs.length > 4 ? '<br>… và ' + (errs.length - 4) + ' lỗi khác.' : '') + '</span></div>' : '<span class="fmsg hint">' + ic('okc', 'xs') + 'Bảng hợp lệ: ' + rows.length + ' hàng, không thiếu, không trùng.</span>';
        var ok = $('#tb-ok', dlg.el); ok.textContent = 'Áp dụng ' + rows.length + ' hàng'; ok.disabled = !!errs.length; ok.dataset.tip = errs.length ? 'Sửa ' + errs.length + ' lỗi trước khi áp dụng' : '';
        $('#tb-n', dlg.el).textContent = rows.length + ' hàng · ' + cols.length + ' cột'; $('#tb-del', dlg.el).disabled = cur < 0; $('#tb-up', dlg.el).disabled = cur <= 0; $('#tb-dn', dlg.el).disabled = cur < 0 || cur >= rows.length - 1; }
      t.addEventListener('input', function(e){ var x = e.target, r = Number(x.dataset.r), j = Number(x.dataset.j), c = C(j); if (isNaN(r)) return; if (x.tagName === 'SELECT') rows[r][j] = x.value === '' ? '' : c.opts[Number(x.value)][0]; else if (c){ var n = TC.parseNum(x.value, c.p.k === 'rot' || c.p.k === 'align' ? 'deg' : 'len'); rows[r][j] = x.value.trim() === '' ? '' : n.err ? NaN : n.v; } else rows[r][j] = x.value.trim(); check(); });
            t.addEventListener('click', function(e){ var s = e.target.closest('[data-sr]'); if (s){ cur = Number(s.dataset.sr); $$('tbody tr', t).forEach(function(tr, i){ tr.classList.toggle('is-sel', i === cur); }); check(); return; } var rm = e.target.closest('[data-rmc]'); if (rm){ var i = Number(rm.dataset.rmc); cols.splice(i, 1); rows.forEach(function(r){ r.splice(i + off, 1); }); render(); } });
      $('#tb-col', dlg.el).addEventListener('click', function(){ var r = this.getBoundingClientRect(), av = pars(l).filter(function(p){ return canCol(p) && cols.indexOf(p.bk) < 0; });
        TC.menu(av.length ? av.map(function(p){ return { label:esc(p.name) + ' <span class="faint">· ' + KP[p.k][0].toLowerCase() + '</span>', icon:KP[p.k][2], act:function(){ cols.push(p.bk); rows.forEach(function(row){ row.splice(off + cols.length - 1, 0, ''); }); render(); } }; }) : [{ label:'Đã thêm mọi tham số dùng được', dis:true }], r.left, r.bottom + 2, { minWidth:220 }); });
      $('#tb-add', dlg.el).addEventListener('click', function(){ rows.push(Array.from({ length:width() }, function(){ return ''; })); cur = rows.length - 1; render(); var f = $('tbody tr:last-child [data-j]', t); if (f) f.focus(); });
      $('#tb-del', dlg.el).addEventListener('click', function(){ if (cur < 0) return; rows.splice(cur, 1); cur = Math.min(cur, rows.length - 1); render(); });
      $('#tb-up', dlg.el).addEventListener('click', function(){ if (cur > 0){ rows.splice(cur - 1, 0, rows.splice(cur, 1)[0]); cur--; render(); } });
      $('#tb-dn', dlg.el).addEventListener('click', function(){ if (cur >= 0 && cur < rows.length - 1){ rows.splice(cur + 1, 0, rows.splice(cur, 1)[0]); cur++; render(); } });
      var ck = $('#tb-chk', dlg.el); if (ck) ck.addEventListener('change', function(){ chk = ck.checked; check(); });
      $('#tb-ok', dlg.el).addEventListener('click', function(){ if (this.disabled) return; o.ok({ cols:cols, rows:rows, chk:chk }); dlg.close(true); });
      render();
    });
  }
  function lkDialog(a){ var d = D(), l = d.ents, p = parBy(l, a.par); if (!p) return; var tb = a.tbl || { cols:pars(l).filter(function(q){ return q.k === 'linear'; }).slice(0, 1).map(function(q){ return q.bk; }), rows:[], rev:true };
    return tblDialog({ L:l, title:'Bảng tra cứu: ' + esc(p.name), cmd:'BACTION', code:'UI-G05', cols:tb.cols, rows:tb.rows, out:p.name, chk:{ label:'Cho phép tra ngược: grip ▼ và Thuộc tính chọn được giá trị tra cứu', on:tb.rev !== false }, note:'Cột trái là tham số vào; khi giá trị khớp một hàng, ' + esc(p.name) + ' hiện giá trị tra cứu của hàng đó.',
      ok:function(r){ ED.run('Bảng tra cứu', function(){ ED.mod(a, function(x){ x.tbl = { cols:r.cols, rows:r.rows, rev:r.chk }; }); }); TC.cmdline.msg('Bảng tra cứu <b>' + esc(p.name) + '</b>: ' + r.rows.length + ' hàng.'); } }); }
  IMPL.BTABLE = function(ctx){ var d = inBE(ctx); if (!d) return; var tp = pars(d.ents).filter(function(p){ return p.k === 'table'; })[0];
    var go = function(at){ return tblDialog({ L:d.ents, title:'Bảng thuộc tính block', cmd:'BTABLE', code:'UI-G05', name:true, cols:tp ? tp.cols : pars(d.ents).filter(canCol).map(function(p){ return p.bk; }), rows:tp ? tp.rows : [], chk:{ label:'Giá trị tham số phải khớp một hàng trong bảng', on:!!(tp && tp.strict) }, note:'Mỗi hàng là một tổ hợp giá trị. Trên bản vẽ, grip ▼ của bảng chọn một hàng.',
      ok:function(r){ ED.run('BTABLE', function(){ if (tp) ED.mod(tp, function(x){ x.cols = r.cols; x.rows = r.rows; x.strict = r.chk; }); else ED.add({ t:'bparam', k:'table', bk:newKey(d, 'p'), name:uName(d, 'Bảng thuộc tính'), p:at, h:symH(), layer:'0', cols:r.cols, rows:r.rows, strict:r.chk }); }); ctx.out('Bảng thuộc tính block: ' + r.rows.length + ' hàng, ' + r.cols.length + ' cột.'); } }); };
    if (tp) return go(); if (!pars(d.ents).some(canCol)){ ctx.out('Block chưa có tham số dùng được cho bảng (thẳng, cực, xoay, lật, hiển thị). Thêm tham số trước.'); return; }
    return ctx.point('Vị trí bảng thuộc tính').then(function(r){ if (r.p) return go(r.p); }); };
  function vsDialog(p){ var vs = p.vs || {}, mode = vs.list ? 'list' : vs.inc ? 'inc' : 'none', ang = p.k === 'rot', cur = ang ? st0(p) : G.dist(p.p1, p.p2), unit2 = ang ? 'deg' : 'len';
    var dlg = TC.dialog({ id:'bvset', title:'Bảng giá trị: ' + esc(p.name), cmd:'BPARAMETER', code:'UI-G05', w:460,
      body:'<div class="stack" style="gap:8px">' + H.rad('vsm', 'none', 'Không giới hạn', mode === 'none') + H.rad('vsm', 'list', 'Danh sách giá trị', mode === 'list') + '<div style="padding-left:22px"><input class="inp mono" id="vs-l" style="width:100%" value="' + esc(vs.list ? vs.list.map(function(v){ return TC.d(v, v % 1 ? 2 : 0); }).join(', ') : '') + '" placeholder="ví dụ: 700, 800, 900, 1000" spellcheck="false"></div>' +
        H.rad('vsm', 'inc', 'Bước tăng', mode === 'inc') + '<div class="hrow" style="padding-left:22px"><span class="faint">Bước</span><input class="inp mono" id="vs-i" style="width:70px" value="' + (vs.inc ? TC.d(vs.inc, 0) : '') + '"><span class="faint">từ</span><input class="inp mono" id="vs-a" style="width:80px" value="' + (vs.min != null ? TC.d(vs.min, 0) : '') + '"><span class="faint">đến</span><input class="inp mono" id="vs-b" style="width:80px" value="' + (vs.max != null ? TC.d(vs.max, 0) : '') + '"></div>' +
        '<span class="fmsg hint" id="vs-m">Grip động và Thuộc tính chỉ nhận giá trị trong bảng. Giá trị của định nghĩa (' + TC.d(cur) + ') luôn có trong bảng.</span></div>',
      foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="vs-ok">Áp dụng bảng giá trị</button>' });
    $('#vs-ok', dlg.el).addEventListener('click', function(){ var m = $('[name="vsm"]:checked', dlg.el).value, msg = $('#vs-m', dlg.el), err = null, out = null, num = function(id){ var n = TC.parseNum($(id, dlg.el).value, unit2); return n.err ? NaN : n.v; };
      if (m === 'list'){ var list = $('#vs-l', dlg.el).value.split(/[;,\s]+/).filter(Boolean).map(function(s){ var n = TC.parseNum(s, unit2); return n.err ? NaN : n.v; }); if (!list.length || list.some(isNaN)) err = 'Nhập các số cách nhau bằng dấu phẩy và khoảng trắng, ví dụ: 700, 800, 900.'; else if (!ang && list.some(function(v){ return v <= 0; })) err = 'Giá trị phải lớn hơn 0.'; else { if (!list.some(function(v){ return eqV(v, cur); })) list.push(cur); out = { list:list.sort(function(a, b){ return a - b; }).filter(function(v, i, a){ return !i || !eqV(v, a[i - 1]); }) }; } }
      if (m === 'inc'){ var i = num('#vs-i'), a = num('#vs-a'), b = num('#vs-b'); if (!(i > 0)) err = 'Bước phải lớn hơn 0.'; else if (isNaN(a) || isNaN(b) || a >= b) err = 'Nhập khoảng từ nhỏ đến lớn.'; else if (cur < a - 1e-9 || cur > b + 1e-9) err = 'Giá trị của định nghĩa (' + TC.d(cur) + ') phải nằm trong khoảng.'; else out = { inc:i, min:a, max:b }; }
      if (err){ msg.className = 'fmsg err'; msg.innerHTML = ic('warn', 'xs') + esc(err); return; }
      ED.run('Bảng giá trị', function(){ ED.mod(p, function(x){ if (out) x.vs = out; else delete x.vs; }); }); dlg.close(true); TC.cmdline.msg('Bảng giá trị của <b>' + esc(p.name) + '</b>: ' + esc(vsText(p)) + '.'); });
  }

  /* ================= cửa sổ thử block (BTESTBLOCK): tham chiếu thử ở gốc, kéo grip ngay; đóng thì không lưu ================= */
  function testTab(d){ return { id:'btest', name:'Thử block', ctx:true, panels:[ { name:'Thử block', icon:'play', prio:1, items:[ { t:'lg', label:'Đặt lại\nblock', icon:'refresh', id:'bt-reset', cmd:'RESETBLOCK', desc:'Đưa tham chiếu thử về giá trị của định nghĩa.' }, { t:'lg', label:'Đóng\ncửa sổ thử', icon:'close', id:'bt-close', desc:'Quay lại Block Editor. Thay đổi khi thử không được lưu.', act:function(){ testOut(D()); } } ] } ] }; }
  function testIn(d){
    // định nghĩa đang sửa (chưa lưu) tạm thay định nghĩa cùng tên; đóng cửa sổ thử thì trả lại
    var B = d.bedit, tn = B.name, ents = d.ents.filter(function(e){ return !e.cons; }).map(function(e){ var c = ED.clean(e); c.id = ++d.seq; return c; });
    d.btest = { bedit:B, ents:d.ents, undo:d.undo, redo:d.redo, view:Object.assign({}, d.view), tn:tn, orig:d.blocks[tn] };
    d.blocks[tn] = { name:tn, desc:(d.blocks[tn] || {}).desc || '', ents:ents };
    var ins = { t:'insert', name:tn, p:[0, 0], sx:1, sy:1, rot:0, layer:'0', id:++d.seq };
    d.bedit = null; d.ents = [ins]; d.undo = []; d.redo = []; d.sel = [ins.id]; d.found = []; TC.prepDoc(d);
    A().unbanner(d, 'bedit'); document.documentElement.removeAttribute('data-bedit');
    A().banner(d, { id:'btest', icon:'play', html:'Đang thử block <b>' + esc(B.name) + '</b>: kéo grip, đổi giá trị trong Thuộc tính như trên bản vẽ. Thay đổi trong cửa sổ thử không được lưu.', acts:[{ label:'Đóng cửa sổ thử', primary:true, fn:function(){ testOut(D()); } }], onClose:function(){ testOut(D()); } });
    A().setCtxTab(testTab(d)); V().zoomTo(ins._bb); TC.emit('sel'); A().refreshPalettes();
    TC.cmdline.msg('Cửa sổ thử <b>' + esc(B.name) + '</b>: ' + (isDyn(d.blocks[tn]) ? 'kéo grip để thử; Ctrl+Z hoàn tác từng lần thử.' : 'block chưa có tham số nào.') + ' Đóng cửa sổ thử để quay lại Block Editor.');
  }
  function testOut(d){
    var X = d && d.btest; if (!X) return;
    if (X.orig) d.blocks[X.tn] = X.orig; else delete d.blocks[X.tn]; d.ents = X.ents; d.undo = X.undo; d.redo = X.redo; d.view = X.view; d.bedit = X.bedit; d.btest = null; d.sel = []; d.found = []; TC.prepDoc(d);
    A().unbanner(d, 'btest'); if (A().ctxTab && A().ctxTab.id === 'btest') A().setCtxTab(null);
    TC.emit('doc', d); V().draw(); TC.emit('sel'); A().refreshPalettes(); TC.cmdline.msg('Đã đóng cửa sổ thử. Block Editor giữ nguyên, thay đổi khi thử không được lưu.');
  }
  IMPL.BTESTBLOCK = function(ctx){ var d = D(); if (d.btest){ ctx.out('Cửa sổ thử đang mở.'); return; } if (!d.bedit){ ctx.out('Thử block dùng trong Block Editor. Mở Block Editor bằng BEDIT.'); return; } if (!d.ents.some(function(e){ return !isPar(e) && !isAct(e); })){ ctx.out('Block chưa có hình học để thử.'); return; } testIn(d); };
  var cr0 = ED.ctxRefresh;
  ED.ctxRefresh = function(){ var d = D(), app = A(); if (d && d.btest && app.active !== 'start' && !TC.isViewer()){ app.setCtxTab(testTab(d), app.ctxTab && app.ctxTab.id === 'btest'); return; } cr0.apply(this, arguments); };
  // thoát cửa sổ thử trước khi rời Block Editor, mở màn hình khác
  ['BSAVE', 'BCLOSE', 'BEDIT'].forEach(function(n){ var f0 = IMPL[n]; IMPL[n] = function(ctx){ var d = D(); if (d && d.btest) testOut(d); var r; USERCLOSE = n === 'BCLOSE'; try { r = f0.apply(this, arguments); } finally { Promise.resolve(r).then(function(){ USERCLOSE = false; }, function(){ USERCLOSE = false; }); }
    if (n === 'BSAVE' && D().bedit){ var w = warns(D().ents); if (w.length) ctx.out('Lưu ý: ' + w.slice(0, 2).map(function(x){ return esc(x.msg); }).join('; ') + (w.length > 2 ? '; và ' + (w.length - 2) + ' cảnh báo khác' : '') + '. Grip của tham số chưa có action không làm gì.'); } return r; }; });
  var USERCLOSE = false;
  addEventListener('click', function(e){ if (e.target.closest && e.target.closest('.cat-row[data-code]')) testOut(D()); }, true);
  document.addEventListener('DOMContentLoaded', function(){ var os = TC.openScreen; if (os) TC.openScreen = function(){ testOut(D()); return os.apply(this, arguments); }; });

  /* ================= RESETBLOCK, menu chuột phải, nhấp đúp, sửa tại chỗ, phá khối ================= */
  IMPL.RESETBLOCK = function(ctx){ var d = D();
    return ctx.sel('Chọn block động cần đặt lại').then(function(ids){ var list = ED.editable(ids, ctx).map(function(id){ return d.byId[id]; }).filter(function(e){ return e && e.t === 'insert' && isDyn(d.blocks[e.name]); });
      if (!list.length){ ctx.out('Vùng chọn không có block động.'); return; } var ch = list.filter(function(e){ return e.dyn && Object.keys(e.dyn).length; });
      if (!ch.length){ ctx.out(list.length + ' block động đã ở giá trị của định nghĩa.'); return; }
      return ED.run('RESETBLOCK', function(){ ch.forEach(function(e){ ED.mod(e, function(x){ delete x.dyn; }); }); ctx.out('Đã đặt lại ' + TC.n(ch.length) + ' block động về giá trị của định nghĩa. Ctrl+Z để hoàn tác.'); }); }); };
  TC.cad.ctxExt.push(function(it, first, d){
    if (TC.isViewer() || !first) return;
    if (first.t === 'insert' && isDyn(d.blocks[first.name])){ for (var i = it.length - 1; i >= 0; i--) if (it[i] && it[i].label === 'Sửa block tại chỗ') it.splice(i, 1); it.push({ label:'Đặt lại block', icon:'refresh', cmd:'RESETBLOCK', act:function(){ TC.cmd.run('RESETBLOCK'); } }); }
    if (d.bedit && isAct(first)) it.push({ label:'Sửa tập đối tượng của action…', icon:'select', act:function(){ TC.cmd.run('BACTION', { edit:first }); } });
    if (d.bedit && isPar(first) && FOR[first.k]) it.push({ label:'Thêm action cho tham số…', icon:'stretch', act:function(){ TC.cmd.run('BACTION', { par:first }); } });
    if (d.bedit && isPar(first) && /^(linear|polar|rot)$/.test(first.k)) it.push({ label:'Bảng giá trị…', icon:'list', act:function(){ vsDialog(first); } });
  });
  var dbl0 = TC.cad.hook.dbl;
  TC.cad.hook.dbl = function(e, m){ var d = D();
    if (e && d && d.bedit && !TC.isViewer()){ if (isAct(e)){ TC.cmd.run('BACTION', { edit:e }); return true; } if (isPar(e) && e.k === 'vis'){ TC.cmd.run('BVSTATE'); return true; } if (isPar(e) && e.k === 'table'){ TC.cmd.run('BTABLE'); return true; } if (isPar(e) && e.k === 'lookup'){ var a = lkAct(d.ents, e); if (a) lkDialog(a); else TC.cmd.run('BACTION', { par:e, k:'lookup' }); return true; } }
    return dbl0 ? dbl0(e, m) : false; };
  var re0 = IMPL.REFEDIT;
  IMPL.REFEDIT = function(ctx, pre){ var d = D(), e = pre && (pre.e || (pre.t === 'insert' ? pre : null)); if (!e && d.sel.length === 1) e = d.byId[d.sel[0]];
    if (e && e.t === 'insert' && isDyn(d.blocks[e.name])){ ctx.out('<b>' + esc(e.name) + '</b> là block động: sửa trong Block Editor (BEDIT) để giữ tham số và action.'); return; } return re0.apply(this, arguments); };
  // phá khối block động: thay bằng block ẩn danh mang hình hiện tại (như *U trong DWG) rồi phá như thường
  var ex0 = IMPL.EXPLODE;
  IMPL.EXPLODE = function(ctx){ var d = D();
    var conv = function(ids){ ids.forEach(function(id){ var e = d.byId[id]; if (!e || e.t !== 'insert' || !isDyn(d.blocks[e.name])) return; var i = 1; while (d.blocks['*U' + i]) i++; var nm = '*U' + i, done = ED.state(function(){ return d.blocks[nm] || null; }, function(s){ if (s) d.blocks[nm] = s; else delete d.blocks[nm]; });
      d.blocks[nm] = { name:nm, desc:'', ents:evalDef(d.blocks[e.name], e.dyn).ents.map(function(x){ x.id = ++d.seq; return x; }) }; done(); ED.mod(e, function(x){ x.name = nm; delete x.dyn; }); }); };
    var go = function(ids){ return ids.some(function(id){ var e = d.byId[id]; return e && e.t === 'insert' && isDyn(d.blocks[e.name]); }) ? ED.run('EXPLODE', function(){ conv(ids); return ex0(ctx); }) : ex0(ctx); };
    return d.sel.length ? go(d.sel.slice()) : ctx.sel('Chọn đối tượng').then(function(ids){ if (!ids.length) return; d.sel = ids; return go(ids); }); };

  /* ================= palette tạo block UI-G05 (BAUTHORPALETTE) ================= */
  var BA = { tab:TC.load('bauthorTab', 'par') }, PALPREV = null;
  var TABS = [['par', 'Tham số'], ['act', 'Action'], ['set', 'Bộ tham số'], ['con', 'Ràng buộc']];
  var SETS = [['linear', 'stretch'], ['linear', 'move'], ['linear', 'scale'], ['linear', 'array'], ['polar', 'pstretch'], ['polar', 'move'], ['xy', 'stretch'], ['xy', 'array'], ['point', 'move'], ['rot', 'rotate'], ['flip', 'flip'], ['vis', null], ['lookup', 'lookup']];
  // ràng buộc (lệnh, biểu tượng của tab Tham số, screens15.js): [lệnh, tên, biểu tượng, mô tả]
  var GCS = [['GCCOINCIDENT', 'Trùng điểm', 'gcoin', 'Ghép hai điểm trùng nhau'], ['GCCOLLINEAR', 'Thẳng hàng', 'gcoll', 'Hai đoạn nằm trên một đường'], ['GCPARALLEL', 'Song song', 'gpar', 'Hai đoạn song song'], ['GCPERPENDICULAR', 'Vuông góc', 'gperp', 'Hai đoạn vuông góc'], ['GCHORIZONTAL', 'Nằm ngang', 'ghor', 'Đoạn song song trục X'], ['GCVERTICAL', 'Thẳng đứng', 'gvert', 'Đoạn song song trục Y'], ['GCTANGENT', 'Tiếp xúc', 'gtan', 'Cung, tròn tiếp xúc đối tượng'], ['GCSMOOTH', 'Trơn (G2)', 'gsmooth', 'Spline nối trơn với đối tượng'], ['GCCONCENTRIC', 'Đồng tâm', 'gconc', 'Cung, tròn chung tâm'], ['GCSYMMETRIC', 'Đối xứng', 'gsym', 'Đối xứng qua một đường'], ['GCEQUAL', 'Bằng nhau', 'gequal', 'Cùng độ dài hoặc bán kính'], ['GCFIX', 'Cố định', 'gfix', 'Giữ điểm, đối tượng tại chỗ']];
  var DCS = [['DCLINEAR', 'Thẳng', 'dclin', 'Khoảng cách theo X hoặc Y'], ['DCHORIZONTAL', 'Ngang', 'dchor', 'Khoảng cách theo X'], ['DCVERTICAL', 'Đứng', 'dcver', 'Khoảng cách theo Y'], ['DCALIGNED', 'Nghiêng', 'dcali', 'Khoảng cách theo đường nối'], ['DCANGULAR', 'Góc', 'dcang', 'Góc giữa hai đoạn'], ['DCRADIUS', 'Bán kính', 'dcrad', 'Bán kính cung, tròn'], ['DCDIAMETER', 'Đường kính', 'dcdia', 'Đường kính cung, tròn']];
  function tpi(attr, icon, name, desc, cmd){ return '<button class="tpi" type="button" role="option" aria-selected="false" ' + attr + ' data-tip="' + esc(name) + '" data-desc="' + esc(desc) + '"' + (cmd ? ' data-cmd="' + cmd + '"' : '') + '><span class="tpic">' + ic(TC.ICONS[icon] ? icon : 'param') + '</span><span class="nm">' + esc(name) + '<small>' + esc(desc) + '</small></span></button>'; }
  P.bauthor = { name:'Tạo block', hidden:function(){ return TC.isViewer(); }, render:function(host, d){
    if (!d){ host.innerHTML = ''; return; }
    if (TC.isViewer()){ host.innerHTML = '<div class="empty">' + ic('lock') + '<b>Palette tạo block</b>Tạo dynamic block có trong TrionCAD Pro.<button class="btn primary sm" type="button" data-up style="margin-top:8px">Dùng thử Pro 30 ngày</button></div>'; host.onclick = function(e){ if (e.target.closest('[data-up]')) SC.upgrade('Tạo dynamic block', 'PRO'); }; return; }
    if (!d.bedit){ host.innerHTML = '<div class="empty">' + ic('dauthor') + (d.btest ? '<b>Đang thử block</b>Đóng cửa sổ thử để tiếp tục thêm tham số, action.<button class="btn sm" type="button" data-ba="untest" style="margin-top:8px">Đóng cửa sổ thử</button>' : '<b>Palette tạo block</b>Thêm tham số và action cho block trong Block Editor.<button class="btn sm" type="button" data-ba="bedit" style="margin-top:8px">' + ic('bedit') + 'Mở Block Editor…</button>') + '</div>';
      host.onclick = function(e){ var b = e.target.closest('[data-ba]'); if (b) b.dataset.ba === 'untest' ? testOut(d) : TC.cmd.run('BEDIT'); }; return; }
    var tab = BA.tab, list = '', w = warns(d.ents);
    if (tab === 'par') list = PKS.map(function(k){ return tpi('data-bp="' + k + '"', KP[k][2], KP[k][0], KP[k][4], 'BPARAMETER'); }).join('');
    else if (tab === 'act') list = Object.keys(KA).map(function(k){ return tpi('data-bac="' + k + '"', KA[k][2], KA[k][0], KA[k][3], 'BACTION'); }).join('') + tpi('data-btb="1"', 'table', 'Bảng thuộc tính', 'Tổ hợp giá trị, grip ▼ chọn một hàng', 'BTABLE');
    else if (tab === 'set') list = SETS.map(function(s){ return tpi('data-bset="' + s[0] + '|' + (s[1] || '') + '"', KP[s[0]][2], KP[s[0]][0] + (s[1] ? ' · ' + KA[s[1]][0] : ''), s[1] ? KP[s[0]][4] + ', chọn đối tượng ngay' : 'Kèm trạng thái đầu tiên', 'BPARAMETER'); }).join('');
    else list = '<div class="dba-h">Ràng buộc hình học</div>' + GCS.concat([0], DCS).map(function(c){ return c ? tpi('data-bcon="' + c[0] + '"', c[2], c[1], c[3], c[0]) : '<div class="dba-h">Tham số ràng buộc · có grip trên tham chiếu</div>'; }).join('');
    host.innerHTML = '<div class="tp dba"><div class="pal-tools"><span class="dba-b">' + ic('bedit') + '<b>' + esc(d.bedit.name) + '</b></span><span class="grow"></span><button class="ib" type="button" data-ba="vis" aria-label="Quản lý trạng thái hiển thị" data-tip="Quản lý trạng thái hiển thị" data-cmd="BVSTATE">' + ic('dvis') + '</button><button class="ib" type="button" data-ba="test" aria-label="Thử block" data-tip="Thử block" data-cmd="BTESTBLOCK">' + ic('play') + '</button></div>' +
      '<div class="tpm"><div class="tpl" role="listbox" tabindex="0" aria-label="' + esc(TABS.filter(function(t){ return t[0] === tab; })[0][1]) + '">' + list + '</div><div class="tpt" role="tablist" aria-orientation="vertical">' + TABS.map(function(t){ return '<button class="tpv" role="tab" type="button" data-btab="' + t[0] + '" aria-selected="' + (t[0] === tab) + '">' + t[1] + '</button>'; }).join('') + '</div></div>' +
      '<div class="dba-st"><span class="faint">' + pars(d.ents).length + ' tham số · ' + acts(d.ents).length + ' action</span>' + (w.length ? w.slice(0, 3).map(function(x){ return '<button class="dba-w" type="button" data-bw="' + x.e.id + '" data-tip="Chọn để xem">' + ic('warn', 's12') + '<span>' + esc(x.msg) + '</span></button>'; }).join('') + (w.length > 3 ? '<span class="faint">… và ' + (w.length - 3) + ' cảnh báo khác</span>' : '') : '<span class="dba-ok">' + ic('okc', 's12') + 'Mọi tham số đều đã có action</span>') + '</div></div>';
    host.onclick = function(e){ var b = e.target.closest('button'); if (!b) return; var s = b.dataset;
      if (s.btab){ BA.tab = s.btab; TC.save('bauthorTab', BA.tab); P.bauthor.render(host, d); return; }
      if (s.bp) return TC.cmd.run('BPARAMETER', s.bp); if (s.bac) return TC.cmd.run('BACTION', s.bac); if (s.btb) return TC.cmd.run('BTABLE');
      if (s.bset){ var q = s.bset.split('|'); return TC.cmd.run('BPARAMETER', { k:q[0], set:q[1] || null }); }
      if (s.bcon) return TC.cmd.run(s.bcon);
      if (s.ba === 'vis') return TC.cmd.run('BVSTATE'); if (s.ba === 'test') return TC.cmd.run('BTESTBLOCK');
      if (s.bw){ d.sel = [Number(s.bw)]; TC.cad.afterSel(); } };
    TC.listKeys($('.tpl', host), '.tpi', function(b, enter){ if (enter){ b.click(); return; } $$('.tpi', host).forEach(function(x){ x.setAttribute('aria-selected', String(x === b)); }); b.focus(); });
  } };
  function authorOn(){ var pal = A().pal; return pal.left.indexOf('bauthor') >= 0 || pal.right.indexOf('bauthor') >= 0; }
  function showPal(id){ A().showPalette(id); var f = $('.dock.flyout [data-fly="' + id + '"]'), h = f && f.closest('.dock').querySelector('.palhost'); if (f && h && h.hidden) f.click(); } // khung hẹp: mở palette tự ẩn
  function showAuthor(){ var pal = A().pal; if (!authorOn()){ PALPREV = { a:pal.leftActive, p:pal.leftPinned }; pal.left.unshift('bauthor'); } showPal('bauthor'); }
  function hideAuthor(){ var pal = A().pal; if (!authorOn()) return; ['left', 'right'].forEach(function(s){ var i = pal[s].indexOf('bauthor'); if (i < 0) return; pal[s].splice(i, 1); if (pal[s + 'Active'] === 'bauthor') pal[s + 'Active'] = s === 'left' && PALPREV ? PALPREV.a : pal[s][0] || null; }); if (PALPREV){ pal.leftPinned = PALPREV.p; PALPREV = null; } A().renderDocks(); TC.save('pal', pal); }
  IMPL.BAUTHORPALETTE = function(ctx){ var d = D(); if (!d.bedit){ ctx.out('Palette tạo block dùng trong Block Editor. Mở Block Editor bằng BEDIT.'); return; } if (authorOn() && $('.palhost:not([hidden]) .dba')) hideAuthor(); else showAuthor(); };

  /* ================= tab Block Editor: panel Tham số và Action, Visibility, Thử (07.1 §7.11) ================= */
  SC.dynPanels = function(d){
    var vp = visPar(d.ents), cur = vp ? visSt(vp, d.bedit.vs).n : null;
    var vmenu = function(b){ var r = b.getBoundingClientRect(); TC.menu(vp ? [{ h:'Trạng thái hiện hành' }].concat(vp.states.map(function(s){ return { label:esc(s.n), check:s.n === cur, act:function(){ setVs(d, s.n); } }; })).concat([{ sep:1 }, { label:'Quản lý trạng thái…', icon:'dvis', act:function(){ TC.cmd.run('BVSTATE'); } }]) : [{ label:'Thêm tham số hiển thị…', icon:'dvis', act:function(){ TC.cmd.run('BPARAMETER', 'vis'); } }], r.left, r.bottom + 2, { minWidth:r.width, returnFocus:b }); };
    return [
      { name:'Tham số và Action', icon:'dlinear', prio:2, items:[
        { t:'lg', label:'Tham số', icon:KP[LASTP][2], id:'be-par', cmd:'BPARAMETER', desc:'Thêm tham số: điểm, thẳng, cực, XY, xoay, gióng, lật, hiển thị, tra cứu, điểm gốc.', menu:function(){ return PKS.map(function(k){ return { label:KP[k][0] + ' <span class="faint">· ' + esc(KP[k][4].charAt(0).toLowerCase() + KP[k][4].slice(1)) + '</span>', icon:KP[k][2], act:function(){ TC.cmd.run('BPARAMETER', k); } }; }); } },
        { t:'lg', label:'Action', icon:'stretch', id:'be-act', cmd:'BACTION', desc:'Gắn action vào tham số: di chuyển, tỉ lệ, kéo giãn, kéo giãn cực, mảng, xoay, lật, tra cứu.', menu:function(){ return Object.keys(KA).map(function(k){ return { label:KA[k][0], icon:KA[k][2], act:function(){ TC.cmd.run('BACTION', k); } }; }); } },
        { t:'lg', label:'Bảng\nthuộc tính', icon:'table', id:'be-tbl', cmd:'BTABLE', desc:'Bảng tổ hợp giá trị tham số; trên bản vẽ grip ▼ chọn một hàng.' } ] },
      { name:'Visibility', icon:'dvis', prio:3, items:[
        { t:'col', items:[ { t:'s', label:'Chế độ hiển thị', icon:'dvmode', id:'be-vm', toggle:true, on:function(){ return !!BV.mode; }, desc:'Bật: đối tượng ẩn ở trạng thái hiện hành hiện mờ. Tắt: không hiện.', act:function(){ BV.mode = !BV.mode; TC.save('bvmode', BV.mode); V().draw(); ED.ctxRefresh(); TC.cmdline.msg('Chế độ hiển thị: ' + (BV.mode ? 'đối tượng ẩn hiện mờ.' : 'không hiện đối tượng ẩn.')); } },
          { t:'s', label:'Hiện đối tượng', icon:'eye', id:'be-show', cmd:'BVSHOW' }, { t:'s', label:'Ẩn đối tượng', icon:'eyeoff', id:'be-hide', cmd:'BVHIDE' } ] },
        { t:'col', gap:4, items:[ { t:'dd', id:'be-vs', label:'Trạng thái hiển thị', w:150, html:function(){ return ic('dvis'); }, text:function(){ return cur || 'Chưa có trạng thái'; }, act:vmenu }, { t:'s', label:'Quản lý trạng thái…', icon:'layers', id:'be-vsm', cmd:'BVSTATE' } ] } ] },
      { name:'Thử', icon:'play', prio:4, items:[ { t:'lg', label:'Thử\nblock', icon:'play', id:'be-test', cmd:'BTESTBLOCK', desc:'Mở cửa sổ thử: kéo grip như trên bản vẽ. Đóng cửa sổ thử thì không lưu gì.' } ] }
    ];
  };
  // đóng Block Editor: đóng palette tạo block; bản demo thì chọn cửa trên mặt bằng để thấy grip động
  var DEMO = false;
  TC.on('bclose', function(o){ setTimeout(function(){ var d = D(); if (!d || !d.bedit) hideAuthor(); }, 0); if (DEMO && USERCLOSE && o.name === 'CUA-DI-1C') setTimeout(function(){ doorShow(o.d); }, 60); DEMO = false; });
  // palette Blocks (screens7): đánh dấu block động, trạng thái hiển thị khi chèn
  SC.dyn = { isDyn:isDyn,
    // [[tên, giá trị]] của tham chiếu: số giữ dạng số, còn lại là chữ như nhóm Tùy biến
    props:function(d, e){ var bd = e.t === 'insert' && d.blocks[e.name]; if (!isDyn(bd)) return []; var l = bd.ents, out = []; pars(l).forEach(function(p){ partsOf(l, p).forEach(function(pt){ var v = pt[1](e.dyn); out.push([pt[0], typeof v === 'number' && p.k !== 'flip' ? v : fmtV(p, v, pt[4])]); }); }); return out; },
    visStates:function(bd){ var vp = isDyn(bd) && visPar(bd.ents); return vp ? vp.states.map(function(s){ return s.n; }) : null; },
    insDyn:function(bd, st){ var vp = isDyn(bd) && visPar(bd.ents); if (!vp || !st || st === vp.states[0].n) return undefined; var o = {}; o[vp.bk] = st; return o; } };

  /* ================= dữ liệu mẫu: cửa đi CUA-DI-1C thành block động ================= */
  // cỡ thật 900; trạng thái Mở 90° / Mở 45° / Đóng; Bề rộng 700–1000 (kéo giãn cánh đóng, tỉ lệ cánh mở), lật bản lề, tra cứu cỡ cửa
  function doorEnts(d){ var W = 900, t = 40, h = 36, out = [], add = function(o){ o.layer = o.layer || '0'; o.id = ++d.seq; out.push(o); return o; };
    var leaf = function(deg, bk){ var r = deg * Math.PI / 180, u = [Math.cos(r), Math.sin(r)], n = [u[1], -u[0]]; add({ t:'pline', pts:[[0, 0], mul2(n, t), add2(mul2(n, t), mul2(u, W)), mul2(u, W)], closed:true, bk:bk }); };
    leaf(90, 'g1'); add({ t:'arc', c:[0, 0], r:W, a0:0, a1:90, bk:'g2' }); leaf(45, 'g3'); add({ t:'arc', c:[0, 0], r:W, a0:0, a1:45, bk:'g4' }); leaf(0, 'g5');
    add({ t:'bparam', k:'linear', bk:'p1', name:'Bề rộng', p1:[0, 0], p2:[W, 0], off:-430, h:h, grips:1, vs:{ list:[700, 800, 900, 1000] } });
    add({ t:'bparam', k:'flip', bk:'p2', name:'Lật hướng mở', p1:[W / 2, -110], p2:[W / 2, 1010], lab:[W / 2, -210], h:h });
    add({ t:'bparam', k:'vis', bk:'p3', name:'Trạng thái', p:[-170, 330], lab:[-245, 330], h:h, states:[{ n:'Mở 90°', hide:['g3', 'g4', 'g5'] }, { n:'Mở 45°', hide:['g1', 'g2', 'g5'] }, { n:'Đóng', hide:['g1', 'g2', 'g3', 'g4'] }] });
    add({ t:'bparam', k:'lookup', bk:'p4', name:'Cỡ cửa', p:[W + 180, 330], h:h });
    add({ t:'baction', k:'scale', bk:'a1', name:'Tỉ lệ', par:'p1', pt:'p2', sel:['g1', 'g2', 'g3', 'g4', 'p2', 'p3', 'p4'], p:[60, -600], h:h });
    add({ t:'baction', k:'flip', bk:'a2', name:'Lật', par:'p2', sel:['g1', 'g2', 'g3', 'g4', 'g5', 'p1', 'p3', 'p4'], p:[400, -600], h:h });
    add({ t:'baction', k:'stretch', bk:'a3', name:'Kéo giãn', par:'p1', pt:'p2', frame:rect([840, -80], [960, 40]), sel:['g5'], p:[700, -600], h:h });
    add({ t:'baction', k:'lookup', bk:'a4', name:'Tra cứu', par:'p4', p:[W + 180, 140], h:h, tbl:{ cols:['p1'], rows:[[700, 'D700 · cửa WC'], [800, 'D800 · phòng ngủ'], [900, 'D900 · cửa chính'], [1000, 'D1000 · sảnh']], rev:true } });
    return out; }
  function doorConvert(d){ var nm = 'CUA-DI-1C', ins = []; [d.ents].concat(d.layouts.map(function(L){ return L.ents; })).forEach(function(l){ l.forEach(function(e){ if (e.t === 'insert' && e.name === nm) ins.push(e); }); });
    ED.run('Chuyển ' + nm + ' thành block động', function(){
      var done = ED.state(function(){ return d.blocks[nm] || null; }, function(s){ if (s) d.blocks[nm] = s; else delete d.blocks[nm]; TC.prepDoc(d); V().draw(); A().refreshPalettes(); });
      d.blocks[nm] = { name:nm, desc:'Cửa đi một cánh, block động: bề rộng 700–1000, lật bản lề, trạng thái mở', ents:doorEnts(d) }; done();
      ins.forEach(function(e){ ED.mod(e, function(x){ var w = Math.abs(x.sx) || 900, v = [700, 800, 900, 1000].reduce(function(b, q){ return Math.abs(q - w) < Math.abs(b - w) ? q : b; }, 900); x.sx = x.sx < 0 ? -1 : 1; x.sy = x.sy < 0 ? -1 : 1; if (v !== 900) x.dyn = { p1:[0, v] }; else delete x.dyn; }); }); }); }
  function doorShow(d){ var e = d.ents.filter(function(x){ return x.t === 'insert' && x.name === 'CUA-DI-1C' && Math.abs(x.p[0] - 2750) < 1 && Math.abs(x.p[1] - 110) < 1; })[0] || d.ents.filter(function(x){ return x.t === 'insert' && x.name === 'CUA-DI-1C'; })[0]; if (!e) return;
    V().zoomTo([e.p[0] - 1400, e.p[1] - 1000, e.p[0] + 2300, e.p[1] + 1500]); d.sel = [e.id]; TC.cad.afterSel(); showPal('props'); setTimeout(function(){ var g = $('[data-dyng="h"]'); if (g) g.scrollIntoView({ block:'nearest' }); }, 50);
    TC.toast('Cửa trên mặt bằng là block động: ▶ kéo đổi bề rộng 700–1000, ⇄ lật bản lề, ▼ trạng thái Mở 90° / Mở 45° / Đóng, ▼ cỡ cửa. Mỗi lần kéo là một bước hoàn tác; Thuộc tính › Tùy biến cũng sửa được.', { type:'info', log:false, ms:10000 }); }
  OPEN['UI-G05'] = function(){
    var app = A(), d1 = app.docs.filter(function(x){ return x.id === 'd1'; })[0]; if (!d1) return;
    testOut(D()); if (!d1.open) app.openDoc('d1', true); SC.cat.PRO(); if (app.active !== 'd1') app.activate('d1');
    var d = SC.cat.plan(); if (d.bedit && SC.beQuit) SC.beQuit(); TC.cad.clearSel();
    if (!isDyn(d.blocks['CUA-DI-1C'])) doorConvert(d);
    SC.bedit('CUA-DI-1C'); if (!d.bedit) return; d.bedit.vs = 'Mở 90°'; BA.tab = 'par'; showAuthor(); DEMO = true; ED.ctxRefresh(); V().draw();
    TC.toast('Block động <b>CUA-DI-1C</b>: tham số Bề rộng (kéo giãn, tỉ lệ), Lật hướng mở, Trạng thái, Cỡ cửa (tra cứu). Thêm tham số, action từ palette Tạo block; Thử block để kéo grip ngay; Đóng Block Editor rồi kéo grip của cửa trên mặt bằng.', { type:'info', log:false, ms:10000 });
  };
})();
