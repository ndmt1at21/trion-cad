/* TrionCAD prototype · Đợt 4: các lệnh v1.0 chạy trên dòng lệnh theo bảng prompt UI-E10 (vẽ, sửa, kích thước, nhóm, chữ). */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic, G = TC.geo, IMPL = TC.cmd.impl, ED = TC.ed, SC = TC.screens, H = SC.h;
  function A(){ return TC.app; } function V(){ return TC.app.view; } function D(){ return TC.app.doc(); }
  var OPEN = SC.open4 = SC.open4 || {}; // mã màn hình → hàm mở (danh mục gọi khi bấm); tiện ích mở ở SC.cat (D1, PRO, MOD, RUN, plan, pick, flash)
  var T = ED.T, polar = ED.polar, sweep = ED.sweep, X = TC.flatExt, PX = TC.propsExt, LAST = ED.last;

  /* ================= tiện ích ================= */
  function sub(a, b){ return [a[0] - b[0], a[1] - b[1]]; }
  function add2(a, v, k){ return [a[0] + v[0] * k, a[1] + v[1] * k]; }
  function dot(a, b){ return a[0] * b[0] + a[1] * b[1]; }
  function unit(v){ var L = Math.hypot(v[0], v[1]) || 1; return [v[0] / L, v[1] / L]; }
  function mid(a, b){ return [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2]; }
  function lerp(a, b, t){ return [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t]; }
  function rad(a){ return a * Math.PI / 180; }
  function prev(e){ return ED.temp([e], { real:true }); }
  function ents(ids, re){ var d = D(); return ids.map(function(id){ return d.byId[id]; }).filter(function(e){ return e && (!re || re.test(e.t)); }); }
  function selEdit(ctx, text){ return ctx.sel(text).then(function(ids){ return ED.editable(ids, ctx); }); }
  function ctip(r, msg){ var s = r && r.s || V().mouse || [100, 100]; TC.ctip($('#cv'), s[0], s[1], msg); }
  function dimStyle(){ return ED.cur().dimstyle; }
  function upr(a){ a = (a % 360 + 360) % 360; return a > 90 && a <= 270 ? a - 180 : a; } // góc chữ luôn đọc được từ dưới lên hoặc từ trái sang
  // prompt có chip: Enter trả mặc định; gõ chữ tắt kiểu AutoCAD hoặc nhãn tiếng Việt đều được
  function ask(ctx, text, opts, def){ return ctx.val(text, def == null ? null : def, { opts:opts }).then(function(v){ v = String(v == null ? '' : v).trim(); var k = opts.filter(function(o){ return o[1].toUpperCase() === v.toUpperCase() || (v && TC.fold(o[0]) === TC.fold(v)); })[0]; return k ? k[1] : v; }); }

  /* ================= màu: dải gradient (True Color), nền cho vùng che ================= */
  function hexOf(c){ return typeof c === 'string' ? c.toUpperCase() : c == null || c === 7 ? '#FFFFFF' : TC.ACI[c] || '#FFFFFF'; }
  function rgbOf(h){ var n = parseInt(h.slice(1), 16); return [n >> 16 & 255, n >> 8 & 255, n & 255]; }
  function mixHex(a, b, t){ var p = rgbOf(a), q = rgbOf(b); return '#' + [0, 1, 2].map(function(i){ return ('0' + Math.round(p[i] + (q[i] - p[i]) * t).toString(16)).slice(-2); }).join('').toUpperCase(); }
  // ponytail: View.color (canvas.js) chỉ tra TC.ACI theo khóa, nên màu '#RRGGBB' được đăng ký làm khóa của TC.ACI; bỏ khi canvas.js vẽ thẳng chuỗi màu
  function tcol(h){ TC.ACI[h] = h; return h; }
  Object.defineProperty(TC.ACI, 'bg', { get:function(){ var d = TC.app.doc && TC.app.doc(); return TC.cssv(d && d.space !== 'model' ? '--c-paper' : '--c-canvas'); } });

  /* ================= hình học ================= */
  // cắt đa giác, giữ phần dot(p, n) >= c (Sutherland–Hodgman); đa giác lõm có cạnh thừa trùng nhau, tô evenodd vẫn đúng
  function clipH(poly, n, c){ var out = [], k = poly.length; for (var i = 0; i < k; i++){ var a = poly[i], b = poly[(i + 1) % k], da = dot(a, n) - c, db = dot(b, n) - c; if (da >= 0) out.push(a); if ((da >= 0) !== (db >= 0)){ var t = da / (da - db); out.push(lerp(a, b, t)); } } return out; }
  function bulgePts(a, b, B, n){ var th = 4 * Math.atan(B), L = G.dist(a, b), r = L / (2 * Math.sin(Math.abs(th) / 2)), m = mid(a, b), u = [(b[0] - a[0]) / L, (b[1] - a[1]) / L], h = r * Math.cos(th / 2) * (B > 0 ? 1 : -1), c = [m[0] - u[1] * h, m[1] + u[0] * h], a0 = Math.atan2(a[1] - c[1], a[0] - c[0]), out = []; for (var i = 1; i < n; i++) out.push([c[0] + r * Math.cos(a0 + th * i / n), c[1] + r * Math.sin(a0 + th * i / n)]); return out; }
  // các điểm của một đường kín (vòng của region, gradient)
  function ringOf(e){
    var N = function(n, f){ return Array.from({ length:n }, function(_, i){ return f(i); }); };
    if (e.t === 'circle') return N(72, function(i){ return polar(e.c, e.r, i * 5); });
    if (e.t === 'ellipse' && e.a0 == null){ var r = rad(e.rot || 0); return N(72, function(i){ var a = i * Math.PI / 36, x = e.rx * Math.cos(a), y = e.ry * Math.sin(a); return [e.c[0] + x * Math.cos(r) - y * Math.sin(r), e.c[1] + x * Math.sin(r) + y * Math.cos(r)]; }); }
    if (e.t === 'pline' && e.closed && e.pts.length > 2){ var P = e.pts, B = e.bulges || [], out = []; P.forEach(function(p, i){ out.push(p); if (B[i]) Array.prototype.push.apply(out, bulgePts(p, P[(i + 1) % P.length], B[i], 8)); }); return out; }
    if (e.t === 'spline' && e.closed) return TC.splinePts(e.pts, true).slice(0, -1);
    return null;
  }
  function areaOf(loops){ return Math.abs(loops.reduce(function(s, l, i){ return s + (i ? -1 : 1) * G.polyArea(l); }, 0)); }
  function bbOf(pts){ var b = [Infinity, Infinity, -Infinity, -Infinity]; pts.forEach(function(p){ b[0] = Math.min(b[0], p[0]); b[1] = Math.min(b[1], p[1]); b[2] = Math.max(b[2], p[0]); b[3] = Math.max(b[3], p[1]); }); return b; }
  // offset đường gấp khúc theo khoảng k (dương: lệch sang trái chiều vẽ), góc nối vát nhọn
  function offPts(P, k, closed){ var n = P.length, nrm = function(a, b){ var u = unit(sub(b, a)); return [-u[1], u[0]]; }; return P.map(function(b, i){ var n1 = closed || i > 0 ? nrm(P[(i - 1 + n) % n], b) : null, n2 = closed || i < n - 1 ? nrm(b, P[(i + 1) % n]) : null; if (!n1 || !n2) return add2(b, n1 || n2, k); var m = unit([n1[0] + n2[0], n1[1] + n2[1]]); return add2(b, m, k / Math.max(.25, dot(m, n1))); }); }
  // biến đổi gộp: xoay quanh b một góc ang (độ) rồi dời b tới q
  function xfm(b, ang, q){ var r = T.rot(b, ang), dx = q[0] - b[0], dy = q[1] - b[1]; return { f:function(p){ var z = r.f(p); return [z[0] + dx, z[1] + dy]; }, rot:ang, k:1, m:[r.m[0], r.m[1], r.m[2], r.m[3], r.m[4] + dx, r.m[5] + dy] }; }

  /* ================= Gradient: tab ngữ cảnh Hatch ở chế độ gradient (UI-E01 phần 1.0) ================= */
  var GP = [['GR_LINEAR', 'Tuyến tính'], ['GR_CYLINDER', 'Trụ'], ['GR_INVCYLINDER', 'Trụ ngược'], ['GR_SPHERICAL', 'Cầu'], ['GR_INVSPHERICAL', 'Cầu ngược'], ['GR_HEMISPHERICAL', 'Bán cầu'], ['GR_INVHEMISPHERICAL', 'Bán cầu ngược'], ['GR_CURVED', 'Cong'], ['GR_INVCURVED', 'Cong ngược']];
  var GS = Object.assign({ name:'GR_LINEAR', one:false, c1:5, c2:2, lum:80, ang:0, ctr:true, tr:0 }, TC.load('gset', {}));
  var GC = { cmd:false, made:[], edit:null, hv:null };
  function gName(n){ var p = GP.filter(function(x){ return x[0] === n; })[0]; return p ? p[1] : n; }
  // tỉ lệ màu 2 tại vị trí u (dọc hướng gradient) hoặc r (khoảng cách tới tâm), cả hai trong 0..1
  function gProf(name, u, r){ var t = /SPHER/.test(name) ? 1 - r : /CYL/.test(name) ? Math.sin(Math.PI * u) : /CURV/.test(name) ? 1 - (1 - u) * (1 - u) : u; return /INV/.test(name) ? 1 - t : t; }
  // một màu: màu 2 là sắc độ của màu 1 (0 % tối hẳn, 100 % sáng hẳn)
  function gCols(g){ var c1 = hexOf(g.c1); return [c1, g.one ? mixHex(c1, g.lum >= 50 ? '#FFFFFF' : '#000000', Math.abs(g.lum - 50) / 50) : hexOf(g.c2)]; }
  // ponytail: tô bằng 2 lớp × 24 dải hatch SOLID lệch nhau nửa dải để che vệt nối; canvas.js có nguyên tố gradient thật thì mịn hơn
  var NB = 24;
  function gBands(e){
    var g = e.g, all = [].concat.apply([], e.loops || []); if (!all.length || !g) return [];
    var du = [Math.cos(rad(e.ang || 0)), Math.sin(rad(e.ang || 0))], dv = [-du[1], du[0]], us = all.map(function(p){ return dot(p, du); }), vs = all.map(function(p){ return dot(p, dv); });
    var u0 = Math.min.apply(null, us), W = Math.max.apply(null, us) - u0 || 1, v0 = Math.min.apply(null, vs), Hh = Math.max.apply(null, vs) - v0 || 1, sh = g.ctr ? 0 : .25, cols = gCols(g), out = [];
    var band = function(loops, t){ if (loops.length) out.push({ t:'hatch', loops:loops, pat:'SOLID', color:tcol(mixHex(cols[0], cols[1], TC.clamp(t, 0, 1))), layer:e.layer }); };
    var each = function(fn){ [0, .5].forEach(function(o){ for (var k = o ? -1 : 0; k < NB; k++){ var a = Math.max(0, (k + o) / NB), b = Math.min(1, (k + 1 + o) / NB); if (b > a) fn(a, b); } }); };
    if (/SPHER/.test(g.name)){
      var c = add2(add2([0, 0], du, u0 + W * (.5 - sh)), dv, /HEMI/.test(g.name) ? v0 : v0 + Hh * (.5 + sh)), R = Math.max.apply(null, all.map(function(p){ return G.dist(p, c); })) || 1, memo = {};
      var disk = function(r){ if (memo[r]) return memo[r]; var res = []; e.loops.forEach(function(l){ var q = l; for (var j = 0; j < 48 && q.length > 2; j++){ var u = [Math.cos(j * Math.PI / 24), Math.sin(j * Math.PI / 24)]; q = clipH(q, [-u[0], -u[1]], -(r * R + dot(c, u))); } if (q.length > 2) res.push(q); }); return (memo[r] = res); };
      each(function(a, b){ band(disk(b).concat(a > 0 ? disk(a) : []), gProf(g.name, 0, (a + b) / 2)); });
    } else each(function(a, b){ var ls = []; e.loops.forEach(function(l){ var q = clipH(clipH(l, du, u0 + W * a), [-du[0], -du[1]], -(u0 + W * b)); if (q.length > 2) ls.push(q); }); band(ls, gProf(g.name, TC.clamp((a + b) / 2 + sh, 0, 1), 0)); });
    return out;
  }
  X.gradient = function(x){ gBands(x.e).forEach(function(b){ x.sub(b); }); };
  // ảnh mẫu trong gallery: vẽ từng điểm ảnh theo cùng công thức
  function gThumb(c, name, g){
    var x = c.getContext('2d'), w = c.width, h = c.height, im = x.createImageData(w, h), cols = gCols(g), p = rgbOf(cols[0]), q = rgbOf(cols[1]), sh = g.ctr ? 0 : .25, cx = (.5 - sh) * w, cy = (/HEMI/.test(name) ? 1 : .5 - sh) * h, R = Math.max(Math.hypot(cx, cy), Math.hypot(w - cx, cy), Math.hypot(cx, h - cy), Math.hypot(w - cx, h - cy));
    for (var j = 0; j < h; j++) for (var i = 0; i < w; i++){ var t = TC.clamp(gProf(name, TC.clamp((i + .5) / w + sh, 0, 1), Math.hypot(i + .5 - cx, j + .5 - cy) / R), 0, 1), o = (j * w + i) * 4; im.data[o] = p[0] + (q[0] - p[0]) * t; im.data[o + 1] = p[1] + (q[1] - p[1]) * t; im.data[o + 2] = p[2] + (q[2] - p[2]) * t; im.data[o + 3] = 255; }
    x.putImageData(im, 0, 0);
  }
  function gTargets(){ return (GC.cmd ? GC.made : GC.edit || []).filter(function(e){ return D().byId[e.id]; }); }
  function gv(k){ var e = !GC.cmd && GC.edit && GC.edit[0]; return e ? (k === 'ang' ? e.ang || 0 : e.g[k]) : GS[k]; }
  function gset(){ return { name:GS.name, one:GS.one, c1:GS.c1, c2:GS.c2, lum:GS.lum, ctr:GS.ctr, tr:GS.tr }; }
  // đổi một thiết lập: khi đang tạo thì áp vào gradient vừa tạo (cùng bước hoàn tác của lệnh); khi sửa thì mỗi lần đổi là một bước
  function setG(k, v){
    var list = gTargets(); if (!GC.edit || GC.cmd){ GS[k] = v; TC.save('gset', GS); }
    if (list.length){ var fn = function(){ list.forEach(function(e){ ED.mod(e, function(x){ if (k === 'ang') x.ang = v; else x.g[k] = v; }); }); }; if (GC.cmd) fn(); else ED.run('HATCHEDIT', fn); V().draw(); }
    A().setCtxTab(gradTab(GC.edit), true);
  }
  // đổi lệnh đang chạy: hủy trước để nhóm hoàn tác của lệnh cũ đóng lại rồi mới chạy lệnh mới
  function swapCmd(c){ TC.cmd.cancel(); setTimeout(function(){ TC.cmd.run(c); }, 0); }
  function toHatch(type){
    var HS = ED.hset, list = gTargets(); HS.type = type; if (type === 'Tô đặc') HS.pat = 'SOLID'; else if (HS.pat === 'SOLID') HS.pat = 'ANSI31'; TC.save('hset', HS);
    if (GC.cmd){ swapCmd('HATCH'); return; }
    ED.run('HATCHEDIT', function(){ list.forEach(function(e){ ED.mod(e, function(x){ x.t = 'hatch'; x.pat = HS.pat; x.sc = HS.sc; x.ang = HS.ang; delete x.g; }); }); });
    A().setCtxTab(null); ED.ctxRefresh();
  }
  // gọi từ tab Hatch (screens6.js, dropdown Loại › Gradient): đang tạo thì chuyển sang GRADIENT; đang sửa thì đổi các hatch đã chọn thành gradient
  SC.toGradient = function(edit){
    if (!edit || !edit.length){ swapCmd('GRADIENT'); return; }
    var g = gset(); ED.run('HATCHEDIT', function(){ edit.forEach(function(e){ if (D().byId[e.id]) ED.mod(e, function(x){ x.t = 'gradient'; x.g = JSON.parse(JSON.stringify(g)); x.ang = GS.ang; delete x.pat; delete x.sc; }); }); });
    A().setCtxTab(null); ED.ctxRefresh();
  };
  function gradTab(edit){
    if (!GC.cmd && edit) GC.edit = edit;
    var cmd = GC.cmd, HS = ED.hset, re = function(){ A().setCtxTab(gradTab(GC.edit), true); };
    var tog = function(label, icon, id, k, x){ return Object.assign({ t:'s', label:label, icon:icon, id:id, toggle:true, on:function(){ return !!gv(k); }, act:function(){ setG(k, !gv(k)); } }, x || {}); };
    var col = function(id, label, k){ return { t:'dd', id:id, label:label, w:130, html:function(){ return SC.colorSw(gv(k)); }, text:function(){ return SC.colorLabel(gv(k)); }, act:function(){ SC.colorPicker(gv(k)).then(function(c){ if (c != null && c !== 0) setG(k, c); else if (c !== undefined) TC.toast('Màu gradient cần một màu cụ thể (chỉ số màu hoặc True Color), không dùng ByLayer, ByBlock.', { type:'info', log:false }); }); } }; };
    var dd = function(id, label, w, list, cur, fn, html){ return { t:'dd', id:id, label:label, w:w, html:html, text:cur, act:function(b){ var r = b.getBoundingClientRect(); TC.menu(list.map(function(x){ return { label:x, check:cur() === x, act:function(){ fn(x); } }; }), r.left, r.bottom + 2, { minWidth:r.width }); } }; };
    var N = function(id, label, k, lo, hi){ return { t:'num', id:id, label:label, w:56, get:function(){ return String(gv(k)); }, set:function(v){ var n = TC.parseNum(v, 'none'); if (n.err){ re(); return; } setG(k, k === 'ang' ? (Math.round(n.v) % 360 + 360) % 360 : TC.clamp(Math.round(n.v), lo, hi)); } }; };
    var hs = function(k){ return { t:'s', label:k === 'sep' ? 'Tạo các hatch riêng' : 'Liên kết', icon:k === 'sep' ? 'copy' : 'group', id:'g-' + k, toggle:true, on:function(){ return !!HS[k]; }, act:function(){ HS[k] = !HS[k]; TC.save('hset', HS); re(); } }; };
    return { id:cmd ? 'gradc' : 'grad', name:cmd ? 'Tạo hatch' : 'Sửa hatch', ctx:true, panels:[
      { name:'Biên', icon:'hatch', prio:2, items:[ { t:'lg', label:'Chọn\nđiểm', icon:'idpoint', id:'g-pick', desc:'Bấm vào trong vùng kín để tô gradient. Rê chuột để xem trước.', act:function(){ if (cmd) TC.cmdline.msg('Bấm vào trong vùng kín để tô gradient.'); else TC.cmd.run('GRADIENT'); } },
        { t:'col', items:[ { t:'s', label:'Chọn đối tượng', icon:'select', id:'g-sel', act:function(){ if (cmd) TC.cmdline.submit('S'); else TC.cmd.run('GRADIENT'); } },
          { t:'s', label:'Tạo lại biên', icon:'refresh', id:'g-rec', act:function(){ var l = gTargets(); if (!l.length) return; ED.run('Tạo lại biên', function(){ l.forEach(function(e){ e.loops.forEach(function(lp){ ED.add(ED.make({ t:'pline', pts:lp.slice(), closed:true, layer:e.layer })); }); }); }); TC.cmdline.msg('Đã tạo lại đường biên cho ' + l.length + ' gradient.'); } },
          { t:'s', label:'Hiện đối tượng biên', icon:'eye', id:'g-show', act:function(){ V().temp = [].concat.apply([], gTargets().map(function(e){ return e.loops.map(function(l){ return { k:'poly', pts:l, closed:true, color:TC.cssv('--c-accent'), w:2 }; }); })); V().drawOver(); } } ] } ] },
      { name:'Mẫu', icon:'gradient', prio:1, items:[ { t:'gal', id:'g-gal', label:'Mẫu gradient', w:272, items:GP, cur:function(){ return GP.map(function(x){ return x[0]; }).indexOf(gv('name')); },
        paint:function(c, it){ gThumb(c, it[0], { one:gv('one'), c1:gv('c1'), c2:gv('c2'), lum:gv('lum'), ctr:gv('ctr') }); }, tip:function(it){ return it[1] + ' · ' + it[0]; },
        hover:function(it){ gTargets().forEach(function(e){ if (GC.hv == null) e._g0 = e.g.name; e.g.name = it ? it[0] : e._g0; TC.prepEnt(D(), e); }); GC.hv = it ? it[0] : null; V().draw(); },
        pick:function(it){ gTargets().forEach(function(e){ if (e._g0){ e.g.name = e._g0; TC.prepEnt(D(), e); } delete e._g0; }); GC.hv = null; setG('name', it[0]); },
        more:function(b){ var r = b.getBoundingClientRect(); TC.menu(GP.map(function(p){ return { label:p[1] + ' <span class="faint">· ' + p[0] + '</span>', check:gv('name') === p[0], act:function(){ setG('name', p[0]); } }; }), r.left, r.bottom + 2); } } ] },
      { name:'Thuộc tính', icon:'props', prio:1, items:[ { t:'col', gap:3, items:[ dd('g-type', 'Loại', 130, ['Mẫu', 'Tô đặc', 'Người dùng', 'Gradient'], function(){ return 'Gradient'; }, function(x){ if (x !== 'Gradient') toHatch(x); }, function(){ return ic('gradient'); }), col('g-c1', 'Màu gradient 1', 'c1'), gv('one') ? N('g-lum', 'Sắc độ (%)', 'lum', 0, 100) : col('g-c2', 'Màu gradient 2', 'c2') ] },
        { t:'col', gap:3, items:[ N('g-tr', 'Trong suốt', 'tr', 0, 90), N('g-ang', 'Góc', 'ang', 0, 359), tog('Một màu', 'plotstyle', 'g-one', 'one', { desc:'Một màu chuyển sang sắc độ sáng hoặc tối của chính nó; tắt để dùng hai màu.' }) ] } ] },
      { name:'Gốc', icon:'gcenter', prio:3, items:[ tog('Canh\ngiữa', 'gcenter', 'g-ctr', 'ctr', { t:'lg', desc:'Bật: gradient đối xứng. Tắt: dời lên trên, sang trái như có nguồn sáng ở bên trái.' }) ] },
      { name:'Tùy chọn', icon:'gear', prio:2, items:[ { t:'col', items:[ hs('assoc'), hs('sep'), dd('g-ord', 'Thứ tự hiển thị', 150, ['Sau đường biên', 'Xuống dưới cùng', 'Trước đường biên', 'Lên trên cùng'], function(){ return /Trước|trên/.test(HS.order || '') ? HS.order : HS.order === 'Xuống dưới cùng' ? HS.order : 'Sau đường biên'; }, function(x){ HS.order = x; TC.save('hset', HS); re(); }) ] } ] },
      { name:'Đóng', icon:'close', prio:1, items:[ { t:'lg', label:cmd ? 'Đóng tạo\nhatch' : 'Đóng sửa\nhatch', icon:'close', id:'g-close', act:function(){ if (cmd) TC.cmdline.submit(''); else TC.cad.clearSel(); } } ] }
    ] };
  }
  IMPL.GRADIENT = function(ctx){
    ctx.badge('GD'); var HS = ED.hset, marks = [];
    GC.cmd = true; GC.made = []; A().setCtxTab(gradTab());
    ctx.out('Thiết lập hiện tại: gradient ' + gName(GS.name).toLowerCase() + ', ' + (GS.one ? 'một màu' : 'hai màu') + ', góc ' + GS.ang + '°' + (GS.ctr ? ', canh giữa' : '') + '. Rê chuột vào vùng kín để xem trước.');
    function make(loops){ return ED.make({ t:'gradient', loops:loops, g:gset(), ang:GS.ang, assoc:!!HS.assoc }); }
    function addG(loops){ marks.push(ED.mark()); if (!HS.sep && GC.made.length) ED.mod(GC.made[0], function(x){ x.loops = x.loops.concat(loops); }); else { var e = ED.add(make(loops)); GC.made.push(e); if (!/Trước|trên/.test(HS.order || '')) ED.order(e, 0); } V().draw(); }
    function loop(){
      return ctx.point('Chọn điểm bên trong', { pick:true, dyn:false, opts:[['Chọn đối tượng', 'S'], ['Hoàn tác', 'U'], ['Thiết lập', 'T']], onMove:function(p, temp){
        var g = ED.regionAt(p); if (!g) return; var d = D(), b = bbOf(g.outer);
        // xem trước đúng như kết quả: gradient nằm dưới, nội dung trong vùng vẽ lại ở trên
        temp.push(prev(make(g.loops)), ED.temp(ED.spaceList(d).filter(function(e){ var q = e._bb; return e.t !== 'hatch' && e.t !== 'gradient' && TC.entVisible(d, e) && q && q[2] >= b[0] && q[0] <= b[2] && q[3] >= b[1] && q[1] <= b[3]; }).slice(0, 400), { real:true }), { k:'poly', pts:g.outer, closed:true, color:TC.cssv('--c-accent'), w:1.8 }); } }).then(function(r){
        if (r.enter) return;
        if (r.kw === 'U'){ if (marks.length){ ED.back(marks.pop()); GC.made = GC.made.filter(function(e){ return D().byId[e.id]; }); } else ctx.out('Không còn bước để hoàn tác.'); return loop(); }
        if (r.kw === 'T'){ TC.toast('Mọi thiết lập gradient có trên tab ngữ cảnh Tạo hatch: mẫu, hai màu hoặc một màu, góc, canh giữa.', { type:'info', log:false }); return loop(); }
        if (r.kw === 'S') return ctx.sel('Chọn đối tượng kín').then(function(ids){ var n = 0; ents(ids).forEach(function(e){ var l = ringOf(e); if (l){ addG([l]); n++; } }); TC.cad.clearSel(); ctx.out(n ? 'Đã tô gradient ' + n + ' vùng.' : 'Không có đối tượng kín (polyline đóng, đường tròn, ellipse) trong vùng chọn.'); return loop(); });
        var g = ED.regionAt(r.p); if (!g){ ctip(r, 'Không tìm thấy vùng kín tại đây. Tăng Dung sai khe hở hoặc chọn đối tượng.'); return loop(); }
        addG(g.loops); ctx.out('Đã tô gradient ' + gName(GS.name).toLowerCase() + ', diện tích ' + TC.d(g.area / 1e6) + ' m²' + (g.loops.length > 1 ? ' (trừ ' + (g.loops.length - 1) + ' island)' : '') + '.');
        return loop();
      });
    }
    function fin(){ GC.cmd = false; GC.made = []; if (A().ctxTab && A().ctxTab.id === 'gradc') A().setCtxTab(null); setTimeout(ED.ctxRefresh, 0); }
    return ED.run('GRADIENT', loop).then(fin, function(err){ fin(); throw err; });
  };
  // chọn gradient thì hiện tab Sửa hatch ở chế độ gradient; tab Tạo hatch giữ nguyên khi đang chạy lệnh
  var refresh0 = ED.ctxRefresh;
  ED.ctxRefresh = function(){
    var app = A(), d = D(); if (!d || app.active === 'start') return;
    var cur = app.ctxTab && app.ctxTab.id; if (cur === 'gradc') return;
    if (!TC.isViewer() && !TC.cmdline.busy()){ var sel = ents(d.sel); if (sel.length && sel.length <= 50 && sel.every(function(e){ return e.t === 'gradient'; })){ app.setCtxTab(gradTab(sel), cur === 'grad'); return; } }
    refresh0();
  };

  /* ================= Region, vùng che, vành khuyên ================= */
  X.region = function(x){ x.e.loops.forEach(function(l){ l.forEach(function(p, i){ x.o({ k:'seg', a:x.P(p), b:x.P(l[(i + 1) % l.length]) }); }); }); };
  IMPL.REGION = function(ctx){
    ctx.badge('REG');
    return selEdit(ctx).then(function(ids){ if (!ids.length) return; var d = D(), es = ents(ids), found = [], rest = [];
      es.forEach(function(e){ var l = ringOf(e); if (l) found.push({ l:l, src:[e] }); else if (/^(line|arc|pline)$/.test(e.t)) rest.push(e); });
      // các đoạn rời có điểm cuối trùng nhau: nối thành vòng kín
      for (var guard = 0; rest.length > 1 && guard < 100; guard++){ var j = null; for (var i = 0; i < rest.length && !(j && j.e.closed); i++) j = ED.join([rest[i]].concat(rest.filter(function(x, k){ return k !== i; }))); if (!j || !j.e.closed) break; found.push({ l:ringOf(j.e), src:j.used }); rest = rest.filter(function(x){ return j.used.indexOf(x) < 0; }); }
      var skip = es.length - found.reduce(function(s, f){ return s + f.src.length; }, 0);
      if (!found.length){ ctx.out('Không có vòng kín. Region cần polyline đóng, đường tròn, ellipse hoặc các đoạn có điểm cuối nối kín nhau.'); return; }
      return ED.run('REGION', function(){ found.forEach(function(f){ var s = f.src[0], r = { t:'region', loops:[f.l], layer:s.layer }; if (s.color != null) r.color = s.color; ED.add(r, ED.listOf(d, s)); f.src.forEach(function(e){ ED.del(e); }); }); TC.cad.clearSel();
        ctx.out('Đã trích ' + found.length + ' vòng kín. Đã tạo ' + found.length + ' region' + (skip ? '; ' + skip + ' đối tượng không tạo được vòng kín' : '') + '.'); }); }); };

  X.wipeout = function(x){ var fm = x.doc.wipeFrame == null ? 1 : x.doc.wipeFrame; [0, 1].forEach(function(){ x.sub({ t:'hatch', loops:x.e.loops, pat:'SOLID', color:'bg', layer:x.e.layer }); }); if (fm) X.region(x); };
  function reWipe(d){ [d.ents].concat(d.layouts.map(function(L){ return L.ents; })).forEach(function(l){ l.forEach(function(e){ if (e.t === 'wipeout') TC.prepEnt(d, e); }); }); V().draw(); }
  IMPL.WIPEOUT = function(ctx){
    ctx.badge('WO'); var d = D();
    var mk = function(pts){ return ED.make({ t:'wipeout', loops:[pts] }); };
    var done = function(pts, src){ return ED.run('WIPEOUT', function(){ ED.add(mk(pts)); if (src) ED.del(src); ctx.out('Đã tạo vùng che ' + TC.d(G.polyArea(pts) / 1e6) + ' m². Đối tượng vẽ sau, hoặc đưa lên trên (TEXTTOFRONT, DRAWORDER), hiện trên vùng che.'); }); };
    function start(){ return ctx.point('Điểm đầu', { opts:[['Khung', 'F'], ['Polyline', 'P']], def:'Polyline' }).then(function(r){
      if (r.kw === 'F') return ctx.kw('Khung vùng che', [['Bật', 'ON'], ['Tắt', 'OFF'], ['Hiện nhưng không in', 'D']], ['OFF', 'ON', 'D'][d.wipeFrame == null ? 1 : d.wipeFrame]).then(function(k){ var v = { ON:1, OFF:0, D:2 }[String(k).toUpperCase()]; if (v == null) return start(); ED.run('WIPEOUT', function(){ var st = ED.state(function(){ return { f:d.wipeFrame == null ? 1 : d.wipeFrame }; }, function(s){ d.wipeFrame = s.f; reWipe(d); }); d.wipeFrame = v; reWipe(d); st(); }); ctx.out('Khung vùng che: ' + ['tắt', 'bật', 'hiện nhưng không in'][v] + ' (áp dụng cho mọi vùng che của bản vẽ).'); });
      if (r.kw === 'P' || r.enter) return ED.pick(ctx, 'Chọn polyline kín', { types:['pline'], bad:'Chọn polyline kín' }).then(function(q){ if (!q.e) return; var e = q.e; if (!e.closed){ ctx.out('Polyline chưa đóng. Đóng bằng PEDIT › Đóng rồi chạy lại WIPEOUT.'); return; } if ((e.bulges || []).some(Boolean)){ ctx.out('Polyline có đoạn cung. Vùng che chỉ dùng polyline gồm các đoạn thẳng.'); return; } return ctx.kw('Xóa polyline?', [['Có', 'Y'], ['Không', 'N']], 'N').then(function(k){ return done(e.pts.slice(), k === 'Y' ? e : null); }); });
      if (!r.p) return; var pts = [r.p];
      return (function next(){ return ctx.point('Điểm tiếp theo', { base:pts[pts.length - 1], opts:pts.length > 1 ? [['Hoàn tác', 'U']] : null, onMove:function(p, temp){ temp.push(prev(mk(pts.concat([p])))); } }).then(function(q){ if (q.kw === 'U'){ if (pts.length > 1) pts.pop(); return next(); } if (q.p){ pts.push(q.p); return next(); } if (pts.length >= 3) return done(pts); ctx.out('Vùng che cần ít nhất 3 điểm.'); }); })();
    }); }
    return start();
  };

  // vành khuyên: polyline kín hai cung có bề rộng (như AutoCAD); đường kính trong 0 cho chấm tròn đặc
  IMPL.DONUT = function(ctx){
    ctx.badge('DO'); if (LAST.dnIn == null){ LAST.dnIn = 100; LAST.dnOut = 300; }
    function dia(text, def){ return ctx.point(text, { def:TC.d(def), num:true, dyn:false }).then(function(r){ if (r.num != null) return r.num; if (r.p) return ctx.point('Điểm thứ hai', { base:r.p }).then(function(q){ return q.p ? G.dist(r.p, q.p) : def; }); return def; }); }
    function mk(c, di, dout){ var rc = (di + dout) / 4; return ED.make({ t:'pline', pts:[[c[0] - rc, c[1]], [c[0] + rc, c[1]]], bulges:[1, 1], closed:true, w:(dout - di) / 2 }); }
    return dia('Đường kính trong', LAST.dnIn).then(function(di){ if (!(di >= 0)){ ctx.out('Đường kính trong không được âm.'); return; } LAST.dnIn = di;
      return dia('Đường kính ngoài', Math.max(LAST.dnOut, di + 1)).then(function(dout){ if (!(dout > di)){ ctx.out('Đường kính ngoài phải lớn hơn đường kính trong.'); return; } LAST.dnOut = dout; var n = 0;
        return ED.run('DONUT', function again(){ return ctx.point('Tâm vành khuyên', { def:'thoát', onMove:function(p, temp){ temp.push(prev(mk(p, di, dout))); } }).then(function(r){ if (!r.p) return; ED.add(mk(r.p, di, dout)); n++; V().draw(); return again(); }); })
          .then(function(){ if (n) ctx.out('Đã tạo ' + n + ' vành khuyên Ø' + TC.d(di, 0) + '/Ø' + TC.d(dout, 0) + '.'); }); }); });
  };

  /* ================= Đa tuyến song song (MLINE) theo kiểu MLSTYLE (UI-D07) ================= */
  // cùng bộ kiểu mặc định với hộp thoại MLSTYLE (screens5.js)
  function mlStyles(d){ return (d.mlineStyles = d.mlineStyles || { STANDARD:{ desc:'Hai nét cách 1 đơn vị', els:[[0.5, 'BYLAYER', 'ByLayer'], [-0.5, 'BYLAYER', 'ByLayer']], cap:{ ls:false, le:false, ao:false }, fill:false, joints:false }, 'TUONG-110':{ desc:'Tường 110 (TCVN): hai nét cách 110', els:[[55, 'BYLAYER', 'ByLayer'], [-55, 'BYLAYER', 'ByLayer']], cap:{ ls:true, le:true, ao:false }, fill:false, joints:false }, 'TUONG-220':{ desc:'Tường 220 (TCVN): hai nét, hatch gạch', els:[[110, 'BYLAYER', 'ByLayer'], [-110, 'BYLAYER', 'ByLayer']], cap:{ ls:true, le:true, ao:false }, fill:true, joints:false } }); }
  X.mline = function(x){
    var e = x.e, SS = mlStyles(x.doc), S = SS[e.style] || SS.STANDARD, P = e.pts, n = P.length, cl = !!e.closed; if (n < 2) return;
    var offs = S.els.map(function(el){ return el[0] * (e.sc || 1); }), hi = Math.max.apply(null, offs), lo = Math.min.apply(null, offs), sh = e.just === 'T' ? -hi : e.just === 'B' ? -lo : 0;
    var top = offPts(P, hi + sh, cl), bot = offPts(P, lo + sh, cl), seg = function(a, b){ x.o({ k:'seg', a:x.P(a), b:x.P(b) }); };
    if (S.fill) x.o({ k:'hatch', loops:(cl ? [top, bot] : [top.concat(bot.slice().reverse())]).map(function(l){ return l.map(x.P); }), pat:'TCVN-GACH', sc:15, ang:0 });
    offs.forEach(function(o){ var L = offPts(P, o + sh, cl); for (var i = 0; i < L.length - (cl ? 0 : 1); i++) seg(L[i], L[(i + 1) % L.length]); });
    if (!cl){ if (S.cap.ls) seg(top[0], bot[0]); if (S.cap.le) seg(top[n - 1], bot[n - 1]); }
  };
  var ML = { just:'Z', sc:1 }, JN = { T:'Trên', Z:'Không', B:'Dưới' };
  IMPL.MLINE = function(ctx){
    ctx.badge('ML'); var d = D(), S = mlStyles(d), c = ED.cur(d), pts = [], made = false;
    if (!S[c.mlstyle]) c.mlstyle = S['TUONG-220'] ? 'TUONG-220' : Object.keys(S)[0];
    ctx.out('Thiết lập hiện tại: Canh lề = ' + JN[ML.just] + ', Tỉ lệ = ' + TC.d(ML.sc) + ', Kiểu = ' + esc(c.mlstyle));
    var mk = function(p, cl){ return ED.make({ t:'mline', pts:p ? pts.concat([p]) : pts.slice(), style:c.mlstyle, just:ML.just, sc:ML.sc, closed:!!cl }); };
    function first(){ return ctx.point('Điểm đầu', { opts:[['Canh lề', 'J'], ['Tỉ lệ', 'S'], ['Kiểu', 'ST']] }).then(function(r){
      if (r.kw === 'J') return ctx.kw('Kiểu canh lề', [['Trên', 'T'], ['Không', 'Z'], ['Dưới', 'B']], ML.just).then(function(k){ if (JN[k]) ML.just = k; return first(); });
      if (r.kw === 'S') return ctx.val('Tỉ lệ đa tuyến song song', TC.d(ML.sc)).then(function(v){ var n = Number(v); if (n > 0) ML.sc = n; else ctx.out('Tỉ lệ phải lớn hơn 0.'); return first(); });
      if (r.kw === 'ST') return ctx.val('Tên kiểu đa tuyến song song hoặc [?]', c.mlstyle).then(function(v){ var u = String(v).toUpperCase(); if (v === '?') ctx.out('Kiểu đã nạp: ' + Object.keys(S).map(esc).join(', ') + '.'); else if (S[v] || S[u]) c.mlstyle = S[v] ? v : u; else ctx.out('Không có kiểu “' + esc(v) + '”. Tạo kiểu mới bằng MLSTYLE.'); return first(); });
      if (!r.p) return; pts.push(r.p); return next(); }); }
    function next(){ var n = pts.length; return ctx.point('Điểm tiếp theo', { base:pts[n - 1], opts:n >= 3 ? [['Đóng', 'C'], ['Hoàn tác', 'U']] : n >= 2 ? [['Hoàn tác', 'U']] : null, onMove:function(p, temp){ temp.push(prev(mk(p))); } }).then(function(r){
      if (r.kw === 'U'){ if (pts.length > 1) pts.pop(); else ctx.out('Không còn đoạn để hoàn tác.'); return next(); }
      if (r.kw === 'C') return fin(true);
      if (r.p){ pts.push(r.p); return next(); }
      return fin(false); }); }
    function fin(cl){ if (made || pts.length < 2) return; made = true; return ED.run('MLINE', function(){ ED.add(mk(null, cl)); ctx.out('Đã vẽ đa tuyến song song ' + esc(c.mlstyle) + ', ' + (pts.length - (cl ? 0 : 1)) + ' đoạn, dài ' + TC.d(G.polyLen(pts, cl)) + '.'); }); }
    // Esc giữ phần đã vẽ (như PLINE)
    return first().then(null, function(err){ fin(false); throw err; });
  };
  // MLSTYLE: nút Hiện hành đặt kiểu cho MLINE
  var mls0 = SC.mlStyle;
  SC.mlStyle = function(){ mls0.apply(this, arguments); var dlg = TC.topDialog(), b = dlg && $('#mls-cur', dlg.el); if (!b) return;
    b.addEventListener('click', function(){ var t = $('[data-mls][aria-selected="true"]', dlg.el); if (!t) return; ED.cur().mlstyle = t.dataset.mls; TC.cmdline.msg('Kiểu đa tuyến song song hiện hành: <b>' + esc(t.dataset.mls) + '</b>. Gõ MLINE để vẽ.'); TC.toast('<b>' + esc(t.dataset.mls) + '</b> là kiểu đa tuyến song song hiện hành.', { log:false, ms:2500 }); }); };

  /* ================= Dấu tâm, đường tâm ================= */
  var CEXT = 350; // phần kéo dài qua đường tròn: CENTEREXE 3,5 × DIMSCALE 100
  // dấu chữ thập ở tâm (0,1 R) và bốn đường kéo dài qua đường tròn, cách chữ thập một khe 0,05 R
  X.cmark = function(x){ var e = x.e, c = e.c, s = e.r * .1, gp = e.r * .05, R = e.r + Math.min(CEXT, e.r * .5), seg = function(a, b){ x.o({ k:'seg', a:x.P(a), b:x.P(b) }); };
    seg([c[0] - s, c[1]], [c[0] + s, c[1]]); seg([c[0], c[1] - s], [c[0], c[1] + s]);
    [[1, 0], [0, 1], [-1, 0], [0, -1]].forEach(function(u){ seg(add2(c, u, s + gp), add2(c, u, R)); }); };
  function cmark(e){ return ED.make({ t:'cmark', c:e.c.slice(), r:e.r }); }
  IMPL.CENTERMARK = function(ctx){
    ctx.badge('CM'); var n = 0;
    return ED.run('CENTERMARK', function loop(){ return ED.pick(ctx, n ? 'Chọn đường tròn hoặc cung tiếp theo' : 'Chọn đường tròn hoặc cung để thêm dấu tâm', { types:['circle', 'arc'], bad:'Chọn đường tròn hoặc cung', def:n ? 'thoát' : null, onMove:function(p, temp, hov){ var e = hov && D().byId[hov]; if (e && (e.t === 'circle' || e.t === 'arc')) temp.push(prev(cmark(e))); } }).then(function(r){ if (!r.e) return; ED.add(cmark(r.e)); n++; V().draw(); return loop(); }); })
      .then(function(){ if (n) ctx.out('Đã thêm ' + n + ' dấu tâm.'); });
  };
  IMPL.CENTERLINE = function(ctx){
    ctx.badge('CL');
    // nối trung điểm hai đầu tương ứng của hai đoạn, kéo dài mỗi phía CEXT; linetype CENTER
    function cl(a, b){ var A1 = a.p1, A2 = a.p2, B1 = b.p1, B2 = b.p2; if (dot(sub(A2, A1), sub(B2, B1)) < 0){ var t = B1; B1 = B2; B2 = t; } var s = mid(A1, B1), e = mid(A2, B2), u = unit(sub(e, s)); return ED.make({ t:'line', p1:add2(s, u, -CEXT), p2:add2(e, u, CEXT), lt:'CENTER' }); }
    return ED.pick(ctx, 'Chọn đoạn thẳng thứ nhất', { types:['line'], bad:'Chọn đoạn thẳng' }).then(function(a){ if (!a.e) return;
      return ED.pick(ctx, 'Chọn đoạn thẳng thứ hai', { types:['line'], bad:'Chọn đoạn thẳng', onMove:function(p, temp, hov){ var e = hov && D().byId[hov]; if (e && e.t === 'line' && e !== a.e) temp.push(prev(cl(a.e, e))); } }).then(function(b){ if (!b.e) return; if (b.e === a.e){ ctx.out('Chọn một đoạn thẳng khác đoạn thứ nhất.'); return; }
        return ED.run('CENTERLINE', function(){ var e = ED.add(cl(a.e, b.e)); ctx.out('Đã tạo đường tâm dài ' + TC.d(G.dist(e.p1, e.p2)) + ' (linetype CENTER).'); }); }); });
  };

  /* ================= Mảng theo đường (ARRAYPATH, tab ngữ cảnh Mảng UI-E02) ================= */
  var AP = null;
  function tanA(P, s){ var h = Math.max(P.L / 2000, 1e-3), a = P.at(Math.max(0, s - h)), b = P.at(Math.min(P.L, s + h)); return G.ang(a, b); }
  function nearS(P, p){ var best = 0, bd = Infinity; for (var i = 0; i <= 400; i++){ var s = P.L * i / 400, q = P.at(s), dd = q ? G.dist(q, p) : Infinity; if (dd < bd){ bd = dd; best = s; } } return best; }
  function apN(R){ return R.method === 'D' ? R.n : Math.max(1, Math.min(R.n, Math.floor(R.P.L / R.sp + 1e-6) + 1)); }
  function apStep(R){ var n = apN(R); return R.method === 'D' ? R.P.L / Math.max(1, R.closed ? n : n - 1) : R.sp; }
  function apXf(R){ var P = R.P, n = apN(R), st = apStep(R), b = R.base || P.at(0), a0 = tanA(P, 0), out = [];
    for (var j = 0; j < R.rows; j++) for (var i = 0; i < n; i++){ var s = Math.min(P.L, i * st), ta = tanA(P, s), q = add2(P.at(s), [-Math.sin(rad(ta)), Math.cos(rad(ta))], j * R.dr); out.push(xfm(b, R.align ? ta - a0 : 0, q)); }
    return out; }
  function apGrips(R){ var P = R.P, st = apStep(R), n = apN(R), g = []; if (n > 1) g.push({ p:P.at(Math.min(P.L, st)), kind:'sq', k:'sp', prompt:'Khoảng cách giữa các phần tử (kéo dọc đường dẫn)' }); g.push({ p:P.at(Math.min(P.L, st * (n - 1))), kind:'tri', k:'n', prompt:'Số phần tử (kéo để thêm, bớt)' }); return g; }
  TC.overlays.push(function(ctx, v){ if (!AP) return; apGrips(AP).forEach(function(g){ g.s = v.mToS(g.p) || v.toS(g.p); ED.drawGrip(ctx, g, 'cold'); }); });
  function apTab(){
    var R = AP, ref = function(){ A().setCtxTab(apTab(), true); R.pv(); }, num = function(v, d){ var n = TC.parseNum(v, 'none'); return n.err ? d : n.v; };
    var N = function(id, label, get, set){ return { t:'num', id:id, label:label, w:64, get:get, set:function(v){ set(v); ref(); } }; };
    return { id:'arr', name:'Mảng', ctx:true, panels:[
      { name:'Phần tử', icon:'array', prio:1, items:[ { t:'col', gap:3, items:[ N('ap-n', 'Số phần tử', function(){ return String(apN(R)); }, function(v){ R.n = TC.clamp(Math.round(num(v, R.n)), 1, 2000); if (R.method === 'M' && apN(R) < R.n) R.sp = R.P.L / Math.max(1, R.n - 1); }), N('ap-s', 'Khoảng cách', function(){ return TC.d(apStep(R)); }, function(v){ var x = num(v, 0); if (x > 0){ R.n = apN(R); R.sp = x; R.method = 'M'; } }), N('ap-t', 'Tổng', function(){ return TC.d(apStep(R) * (apN(R) - 1)); }, function(v){ var x = num(v, 0), n = apN(R); if (x > 0 && n > 1){ R.n = n; R.sp = Math.min(x, R.P.L) / (n - 1); R.method = 'M'; } }) ] } ] },
      { name:'Hàng', icon:'array', prio:2, items:[ { t:'col', gap:3, items:[ N('ap-r', 'Số hàng', function(){ return String(R.rows); }, function(v){ R.rows = TC.clamp(Math.round(num(v, 1)), 1, 100); }), N('ap-rd', 'Khoảng cách', function(){ return TC.d(R.dr); }, function(v){ R.dr = num(v, R.dr); }), N('ap-rt', 'Tổng', function(){ return TC.d(R.dr * (R.rows - 1)); }, function(v){ if (R.rows > 1) R.dr = num(v, 0) / (R.rows - 1); }) ] } ] },
      { name:'Thuộc tính', icon:'props', prio:2, items:[ { t:'lg', label:'Liên kết', icon:'group', id:'ap-as', toggle:true, on:function(){ return false; }, desc:'Prototype tạo mảng theo đường dạng không liên kết.', act:function(){ TC.cmdline.submit('AS'); } }, { t:'lg', label:'Điểm\ngốc', icon:'idpoint', id:'ap-b', act:function(){ TC.cmdline.submit('B'); } },
        { t:'col', items:[ { t:'s', label:R.method === 'D' ? 'Chia đều' : 'Theo khoảng', icon:'measure', id:'ap-m', menuOnly:true, menu:function(){ return [['D', 'Chia đều'], ['M', 'Theo khoảng']].map(function(o){ return { label:o[1], check:R.method === o[0], act:function(){ apMethod(R, o[0]); ref(); } }; }); } }, { t:'s', label:'Canh phần tử', icon:'rotate', id:'ap-al', toggle:true, on:function(){ return R.align; }, act:function(){ R.align = !R.align; ref(); } }, { t:'s', label:'Hướng Z', icon:'iso', id:'ap-z', toggle:true, on:function(){ return true; }, act:function(){ TC.toast('Hướng Z dùng cho mảng 3D: có sau v1.0.', { type:'info', log:false }); } } ] } ] },
      { name:'Đóng', icon:'close', prio:1, items:[ { t:'lg', label:'Đóng\nmảng', icon:'close', id:'ap-x', act:function(){ TC.cmdline.submit('X'); } } ] }
    ] };
  }
  function apMethod(R, m){ if (m === R.method) return; if (m === 'D') R.n = apN(R); else { R.sp = apStep(R); R.n = apN(R); } R.method = m; }
  IMPL.ARRAYPATH = function(ctx){
    ctx.badge('AR');
    return selEdit(ctx).then(function(ids){ if (!ids.length) return;
      return ED.pick(ctx, 'Chọn đường dẫn', { types:['line', 'pline', 'arc', 'circle', 'spline'], bad:'Đường dẫn phải là line, polyline, cung, đường tròn hoặc spline' }).then(function(r){ if (!r.e) return;
        if (ids.indexOf(r.e.id) >= 0){ ctx.out('Đường dẫn không được nằm trong các đối tượng làm mảng.'); return; }
        var P = ED.pathOf(r.e), bb = ED.bbox(ids), size = Math.max(bb[2] - bb[0], bb[3] - bb[1]) || 500, sp = Math.round(size * 1.5) || 1000;
        if (!(P.L > 0)){ ctx.out('Đường dẫn có độ dài bằng 0.'); return; }
        var R = AP = { ids:ids, P:P, closed:r.e.t === 'circle' || !!r.e.closed, method:'M', sp:sp, n:Math.floor(P.L / sp) + 1, align:true, rows:1, dr:Math.round(size * 1.5) || 1000, base:null };
        ctx.out('Kiểu = Theo đường  Liên kết = Không  Phương pháp = Theo khoảng');
        R.pv = function(){ V().temp = apXf(R).map(function(t){ return ED.xprev(ids, t); }); V().drawOver(); };
        A().setCtxTab(apTab());
        var near = function(q){ var best = null, bd = 9; apGrips(R).forEach(function(g){ var s = V().mToS(g.p) || V().toS(g.p), dd = Math.hypot(s[0] - q.s[0], s[1] - q.s[1]); if (dd < bd){ bd = dd; best = g; } }); return best; };
        var ref = function(){ A().setCtxTab(apTab(), true); R.pv(); };
        var grip = function(g, p){ var s = nearS(P, p); if (g.k === 'sp'){ if (R.method === 'D') apMethod(R, 'M'); R.n = apN(R); R.sp = Math.max(1, s); } else { var st = apStep(R); if (R.method === 'D') R.n = Math.max(2, Math.round(s / (P.L / Math.max(1, R.n - 1))) + 1); else R.n = Math.max(1, Math.round(s / st) + 1); } };
        function loop(){
          return ctx.point('Chọn grip để sửa mảng hoặc', { pick:true, dyn:false, def:'Thoát', opts:[['Liên kết', 'AS'], ['Phương pháp', 'M'], ['Điểm gốc', 'B'], ['Hướng tiếp tuyến', 'T'], ['Phần tử', 'I'], ['Hàng', 'R'], ['Cấp', 'L'], ['Canh phần tử', 'A'], ['Hướng Z', 'Z'], ['Thoát', 'X']], onMove:function(p, temp){ apXf(R).forEach(function(t){ temp.push(ED.xprev(ids, t)); }); } }).then(function(q){
            if (q.enter || q.kw === 'X') return true;
            if (q.kw === 'AS'){ ctx.out('Prototype tạo mảng theo đường dạng không liên kết (sửa lại bằng cách chạy lại lệnh).'); return loop(); }
            if (q.kw === 'L' || q.kw === 'Z'){ ctx.out(q.kw === 'L' ? 'Cấp dùng cho mảng 3D: có sau v1.0.' : 'Hướng Z dùng cho mảng 3D: có sau v1.0.'); return loop(); }
            if (q.kw === 'M') return ctx.kw('Phương pháp', [['Chia đều', 'D'], ['Theo khoảng', 'M']], R.method).then(function(k){ apMethod(R, k === 'D' ? 'D' : 'M'); ref(); return loop(); });
            if (q.kw === 'B') return ctx.point('Điểm gốc của phần tử', { base:R.base || P.at(0) }).then(function(z){ if (z.p) R.base = z.p; ref(); return loop(); });
            if (q.kw === 'T'){ ctx.out('Hướng tiếp tuyến: phần tử giữ hướng như khi đặt ở đầu đường dẫn. Bật, tắt Canh phần tử để đổi.'); return loop(); }
            if (q.kw === 'A') return ctx.kw('Canh phần tử theo đường dẫn?', [['Có', 'Y'], ['Không', 'N']], R.align ? 'Y' : 'N').then(function(k){ R.align = k !== 'N'; ref(); return loop(); });
            if (q.kw === 'I') return (R.method === 'M' ? ctx.val('Khoảng cách giữa các phần tử dọc đường dẫn', TC.d(apStep(R))).then(function(v){ var x = Number(v); if (x > 0) R.sp = x; }) : Promise.resolve()).then(function(){ return ctx.val('Số phần tử', String(apN(R))); }).then(function(v){ var n = Math.round(Number(v)); if (n >= 1) R.n = Math.min(2000, n); ref(); return loop(); });
            if (q.kw === 'R') return ctx.val('Số hàng', String(R.rows)).then(function(v){ R.rows = TC.clamp(Math.round(Number(v)) || 1, 1, 100); return ctx.val('Khoảng cách giữa các hàng', TC.d(R.dr)); }).then(function(v){ R.dr = Number(v) || R.dr; ref(); return loop(); });
            var g = q.s && near(q);
            if (!g){ ctip(q, 'Bấm vào một grip của mảng, hoặc Enter để kết thúc'); return loop(); }
            return ctx.point(g.prompt, { base:g.p, onMove:function(p, temp){ var keep = [R.n, R.sp, R.method]; grip(g, p); apXf(R).forEach(function(t){ temp.push(ED.xprev(ids, t)); }); R.n = keep[0]; R.sp = keep[1]; R.method = keep[2]; } }).then(function(z){ if (z.p) grip(g, z.p); ref(); return loop(); });
          });
        }
        var clean = function(){ AP = null; V().temp = []; V().drawOver(); var app = A(); if (app.ctxTab && app.ctxTab.id === 'arr') app.setCtxTab(null); setTimeout(ED.ctxRefresh, 0); };
        R.pv();
        return loop().then(function(){ var xs = apXf(R), made = 0; clean();
          return ED.run('ARRAYPATH', function(){ xs.forEach(function(t, i){ if (i === 0 && !R.base) return; if (i === 0) ED.apply(ids, t); else { ED.apply(ids, t, true); made++; } }); TC.cad.clearSel(); ctx.out('Đã tạo mảng theo đường ' + apN(R) + ' phần tử' + (R.rows > 1 ? ' × ' + R.rows + ' hàng' : '') + ', khoảng cách ' + TC.d(apStep(R)) + ' (' + made * ids.length + ' đối tượng mới, không liên kết).'); }); }, function(err){ clean(); throw err; });
      }); });
  };

  /* ================= Nối cong, căn chỉnh, sửa spline, đảo chiều ================= */
  var BL = { cont:'T' };
  // điểm cuối gần p và hướng đi ra khỏi đối tượng tại đó
  function endInfo(e, p){
    if (e.t === 'arc'){ var s = polar(e.c, e.r, e.a0), t = polar(e.c, e.r, e.a1); return G.dist(p, s) <= G.dist(p, t) ? { p:s, u:[Math.sin(rad(e.a0)), -Math.cos(rad(e.a0))] } : { p:t, u:[-Math.sin(rad(e.a1)), Math.cos(rad(e.a1))] }; }
    var P = e.t === 'line' ? [e.p1, e.p2] : e.t === 'spline' ? TC.splinePts(e.pts, false) : e.pts, n = P.length;
    return G.dist(p, P[0]) <= G.dist(p, P[n - 1]) ? { p:P[0], u:unit(sub(P[0], P[1])) } : { p:P[n - 1], u:unit(sub(P[n - 1], P[n - 2])) };
  }
  function blendE(a, b){ var k = G.dist(a.p, b.p) * (BL.cont === 'S' ? .45 : .33); return ED.make({ t:'spline', pts:[a.p, add2(a.p, a.u, k), add2(b.p, b.u, k), b.p] }); }
  IMPL.BLEND = function(ctx){
    ctx.badge('BL'); var ok = function(e){ return /^(line|arc|spline|pline)$/.test(e.t) && !e.closed; };
    ctx.out('Liên tục = ' + (BL.cont === 'S' ? 'Mượt' : 'Tiếp tuyến'));
    function first(){ return ED.pick(ctx, 'Chọn đối tượng thứ nhất', { opts:[['Liên tục', 'CON']], types:['line', 'arc', 'spline', 'pline'], bad:'Chọn line, cung, polyline hở hoặc spline hở' }).then(function(r){
      if (r.kw === 'CON') return ctx.kw('Liên tục', [['Tiếp tuyến', 'T'], ['Mượt', 'S']], BL.cont).then(function(k){ BL.cont = k === 'S' ? 'S' : 'T'; return first(); });
      if (!r.e) return; if (!ok(r.e)){ ctip(r, 'Đối tượng kín không nối cong được'); return first(); } var a = endInfo(r.e, r.p);
      return ED.pick(ctx, 'Chọn đối tượng thứ hai', { types:['line', 'arc', 'spline', 'pline'], bad:'Chọn line, cung, polyline hở hoặc spline hở', onMove:function(p, temp, hov){ var e = hov && D().byId[hov]; if (e && e !== r.e && ok(e)) temp.push(prev(blendE(a, endInfo(e, p)))); } }).then(function(q){ if (!q.e) return; if (q.e === r.e || !ok(q.e)){ ctx.out('Chọn đối tượng hở khác đối tượng thứ nhất.'); return; } var b = endInfo(q.e, q.p);
        if (G.dist(a.p, b.p) < 1e-6){ ctx.out('Hai điểm cuối trùng nhau, không cần nối.'); return; }
        return ED.run('BLEND', function(){ var e = ED.add(blendE(a, b)); e.layer = r.e.layer; ctx.out('Đã tạo spline nối cong dài ' + TC.d(ED.pathOf(e).L) + ' (liên tục ' + (BL.cont === 'S' ? 'mượt' : 'tiếp tuyến') + ').'); }); }); }); }
    return first();
  };
  // căn chỉnh 2D: một cặp điểm thì dời; hai cặp thì dời và xoay, có thể co giãn theo cặp điểm
  function alXf(s1, s2, d1, d2, sc){ var th = G.ang(d1, d2) - G.ang(s1, s2), k = sc ? G.dist(d1, d2) / (G.dist(s1, s2) || 1) : 1, c = Math.cos(rad(th)) * k, s = Math.sin(rad(th)) * k, tx = d1[0] - (c * s1[0] - s * s1[1]), ty = d1[1] - (s * s1[0] + c * s1[1]); return { f:function(p){ return [c * p[0] - s * p[1] + tx, s * p[0] + c * p[1] + ty]; }, rot:th, k:k, m:[c, s, -s, c, tx, ty] }; }
  IMPL.ALIGN = function(ctx){
    ctx.badge('AL');
    return selEdit(ctx).then(function(ids){ if (!ids.length) return;
      var go = function(t, msg){ return ED.run('ALIGN', function(){ ED.apply(ids, t); TC.cad.clearSel(); ctx.out('Đã căn chỉnh ' + TC.n(ids.length) + ' đối tượng: ' + msg + '.'); }); };
      return ctx.point('Điểm nguồn thứ nhất').then(function(s1){ if (!s1.p) return;
        return ctx.point('Điểm đích thứ nhất', { base:s1.p, onMove:function(p, temp){ temp.push(ED.xprev(ids, T.move(p[0] - s1.p[0], p[1] - s1.p[1]))); } }).then(function(d1){ if (!d1.p) return; var mv = T.move(d1.p[0] - s1.p[0], d1.p[1] - s1.p[1]), guide = { k:'line', a:s1.p, b:d1.p, color:TC.cssv('--c-accent'), dash:[4, 3] };
          return ctx.point('Điểm nguồn thứ hai', { def:'tiếp tục', temp:[ED.xprev(ids, mv), guide] }).then(function(s2){
            if (!s2.p) return go(mv, 'dời ' + TC.d(G.dist(s1.p, d1.p)));
            return ctx.point('Điểm đích thứ hai', { base:s2.p, temp:[guide], onMove:function(p, temp){ if (G.dist(d1.p, p) > 1e-6) temp.push(ED.xprev(ids, alXf(s1.p, s2.p, d1.p, p, false))); } }).then(function(d2){ if (!d2.p) return;
              return ctx.point('Điểm nguồn thứ ba', { def:'tiếp tục' }).then(function(s3){ if (s3.p) ctx.out('Căn chỉnh 3D có sau v1.0; dùng hai cặp điểm đầu.', 'faint');
                return ctx.kw('Đổi tỉ lệ đối tượng theo các điểm căn chỉnh?', [['Có', 'Y'], ['Không', 'N']], 'N').then(function(k){ var t = alXf(s1.p, s2.p, d1.p, d2.p, k === 'Y'); return go(t, 'xoay ' + TC.d(((t.rot % 360) + 360) % 360, 1) + '°' + (k === 'Y' ? ', tỉ lệ ' + TC.d(t.k, 4) : '')); }); }); }); }); }); }); });
  };
  IMPL.SPLINEDIT = function(ctx, pre){
    ctx.badge('SPE'); var d = D(), s0 = pre && pre.t === 'spline' ? pre : d.sel.length === 1 && d.byId[d.sel[0]] && d.byId[d.sel[0]].t === 'spline' ? d.byId[d.sel[0]] : null;
    var start = s0 ? Promise.resolve({ e:s0 }) : ED.pick(ctx, 'Chọn spline', { types:['spline'], bad:'Chọn spline' });
    return start.then(function(r){ if (!r.e) return; var e = r.e, marks = [];
      if ((d.layerMap[e.layer] || {}).lck){ ctx.out('Spline nằm trên layer bị khóa, không sửa được.'); return; }
      TC.cad.clearSel();
      var dots = function(){ return e.pts.map(function(p){ return { k:'pt', p:p, color:TC.cssv('--c-grip-cold') }; }); };
      var nearI = function(p){ var bi = 0; e.pts.forEach(function(q, i){ if (G.dist(q, p) < G.dist(e.pts[bi], p)) bi = i; }); return bi; };
      var mod = function(fn){ marks.push(ED.mark()); ED.mod(e, fn); V().draw(); };
      function menu(){
        V().temp = dots(); V().drawOver();
        return ask(ctx, 'Nhập một tùy chọn', [[e.closed ? 'Mở' : 'Đóng', e.closed ? 'O' : 'C'], ['Nối', 'J'], ['Khớp dữ liệu', 'F'], ['Sửa đỉnh', 'E'], ['Chuyển thành polyline', 'P'], ['Đảo chiều', 'R'], ['Hoàn tác', 'U'], ['Thoát', 'X']], 'Thoát').then(function(k){
          if (!k || k === 'X') return;
          if (k === 'U'){ if (marks.length) ED.back(marks.pop()); else ctx.out('Không còn bước để hoàn tác.'); return menu(); }
          if (k === 'C' || k === 'O'){ mod(function(x){ x.closed = k === 'C'; }); ctx.out(k === 'C' ? 'Đã đóng spline.' : 'Đã mở spline.'); return menu(); }
          if (k === 'R'){ mod(function(x){ x.pts.reverse(); }); ctx.out('Đã đảo chiều spline.'); return menu(); }
          if (k === 'P') return ctx.val('Độ chính xác (số đoạn trên mỗi nhịp)', '10').then(function(){ var pts = TC.splinePts(e.pts, e.closed); if (e.closed) pts.pop(); var pl = Object.assign(ED.clean(e), { t:'pline', pts:pts, closed:!!e.closed, bulges:[] }); delete pl.id; ED.add(pl, ED.listOf(d, e)); ED.del(e); V().temp = []; V().draw(); ctx.out('Đã chuyển spline thành polyline ' + pts.length + ' đỉnh.'); });
          if (k === 'J') return ctx.sel('Chọn spline, đoạn thẳng, polyline hở cần nối').then(function(ids){ var cand = ents(ED.editable(ids, ctx)).filter(function(x){ return x !== e && /^(line|spline|pline)$/.test(x.t) && !x.closed; }), pts = e.pts.slice(), used = [], grew = true;
            while (grew){ grew = false; cand.forEach(function(x){ if (used.indexOf(x) >= 0) return; var q = x.t === 'line' ? [x.p1, x.p2] : x.pts, a = q[0], b = q[q.length - 1], s = pts[0], t = pts[pts.length - 1], tol = 1;
              if (G.dist(t, a) < tol) pts = pts.concat(q.slice(1)); else if (G.dist(t, b) < tol) pts = pts.concat(q.slice(0, -1).reverse()); else if (G.dist(s, b) < tol) pts = q.slice(0, -1).concat(pts); else if (G.dist(s, a) < tol) pts = q.slice(1).reverse().concat(pts); else return; used.push(x); grew = true; }); }
            TC.cad.clearSel(); if (!used.length){ ctx.out('0 đối tượng được nối: các đối tượng phải chạm điểm đầu hoặc cuối của spline.'); return menu(); }
            marks.push(ED.mark()); ED.mod(e, function(x){ x.pts = pts; }); used.forEach(function(x){ ED.del(x); }); V().draw(); ctx.out(used.length + ' đối tượng đã nối vào spline.'); return menu(); });
          if (k === 'F' || k === 'E') return pts(k === 'F' ? 'Khớp dữ liệu' : 'Sửa đỉnh', 0);
          return menu();
        }); }
      // thêm, xóa, di chuyển điểm khớp; spline mới hiện tạm khi rê chuột
      function pts(label, i){
        return ask(ctx, label, [['Thêm', 'A'], ['Xóa', 'D'], ['Di chuyển', 'M'], ['Thoát', 'X']], 'Thoát').then(function(k){
          if (!k || k === 'X') return menu();
          if (k === 'D') return ctx.point('Chọn điểm khớp cần xóa', { temp:dots() }).then(function(q){ if (!q.p) return pts(label, i); if (e.pts.length <= 2){ ctx.out('Spline cần ít nhất 2 điểm.'); return pts(label, i); } var j = nearI(q.p); mod(function(x){ x.pts.splice(j, 1); }); return pts(label, i); });
          if (k === 'A') return ctx.point('Chọn điểm khớp có sẵn (điểm mới chèn sau điểm này)', { temp:dots() }).then(function(q){ if (!q.p) return pts(label, i); var j = nearI(q.p);
            return ctx.point('Vị trí điểm khớp mới', { base:e.pts[j], temp:dots(), onMove:function(p, temp){ var x = ED.clean(e); x.pts.splice(j + 1, 0, p); temp.push(prev(x)); } }).then(function(z){ if (z.p) mod(function(x){ x.pts.splice(j + 1, 0, z.p); }); return pts(label, i); }); });
          return (function move(i){ var n = e.pts.length; i = (i + n) % n;
            return ctx.point('Vị trí mới của điểm ' + (i + 1) + '/' + n, { base:e.pts[i], opts:[['Kế', 'N'], ['Trước', 'P'], ['Chọn điểm', 'S'], ['Thoát', 'X']], def:'Kế', temp:dots().concat([{ k:'pt', p:e.pts[i], color:TC.cssv('--c-grip-hot') }]), onMove:function(p, temp){ var x = ED.clean(e); x.pts[i] = p; temp.push(prev(x)); } }).then(function(q){
              if (q.enter || q.kw === 'N') return move(i + 1); if (q.kw === 'P') return move(i - 1); if (q.kw === 'X') return pts(label, i);
              if (q.kw === 'S') return ctx.point('Chọn điểm khớp', { temp:dots() }).then(function(z){ return move(z.p ? nearI(z.p) : i); });
              if (q.p) mod(function(x){ x.pts[i] = q.p; }); return move(i); }); })(i);
        }); }
      return ED.run('SPLINEDIT', menu).then(function(){ V().temp = []; V().drawOver(); }, function(err){ V().temp = []; throw err; });
    });
  };
  function revEnt(x){ if (x.t === 'line'){ var t = x.p1; x.p1 = x.p2; x.p2 = t; return; } var n = x.pts.length, b = x.bulges; x.pts.reverse(); if (b && b.length){ var nb = []; for (var i = 0; i < n - 1; i++) nb[i] = -(b[n - 2 - i] || 0); if (x.closed) nb[n - 1] = -(b[n - 1] || 0); x.bulges = nb; } if (x.t === 'mline') x.just = { T:'B', B:'T' }[x.just] || x.just; }
  IMPL.REVERSE = function(ctx){
    return selEdit(ctx, 'Chọn line, polyline, spline hoặc đa tuyến song song cần đảo chiều').then(function(ids){ if (!ids.length) return; var es = ents(ids), ok = es.filter(function(e){ return /^(line|pline|spline|mline)$/.test(e.t); });
      if (!ok.length){ ctx.out('Không có line, polyline, spline hoặc đa tuyến song song trong vùng chọn.'); return; }
      return ED.run('REVERSE', function(){ ok.forEach(function(e){ ED.mod(e, revEnt); }); TC.cad.clearSel();
        // mũi tên chỉ chiều mới ở điểm đầu, tự ẩn
        var mv = V().modelView(), z = 14 / (mv ? mv.v.s : 1); V().temp = ok.map(function(e){ var P = e.t === 'line' ? [e.p1, e.p2] : e.t === 'spline' ? TC.splinePts(e.pts, false) : e.pts, u = unit(sub(P[1], P[0])), w = [-u[1], u[0]], tip = add2(P[0], u, z * 1.6); return { k:'poly', pts:[add2(add2(tip, u, -z), w, z * .55), tip, add2(add2(tip, u, -z), w, -z * .55)], color:TC.cssv('--c-accent'), w:2 }; }); V().drawOver(); setTimeout(function(){ if (!TC.cmdline.busy()){ V().temp = []; V().drawOver(); } }, 2500);
        ctx.out('Đã đảo chiều ' + ok.length + ' đối tượng.' + (es.length > ok.length ? ' ' + (es.length - ok.length) + ' đối tượng không đảo chiều được (chỉ line, polyline, spline, đa tuyến song song).' : '')); }); }); };

  /* ================= Dán đặc biệt (PASTESPEC) ================= */
  IMPL.PASTESPEC = function(ctx){
    var C = ED.clip, has = !!(C && C.ents && C.ents.length), n = has ? C.ents.length : 0, texts = has ? C.ents.filter(function(e){ return e.t === 'text' || e.t === 'mtext'; }) : [], cur = 'obj';
    var F = [['obj', 'Đối tượng TrionCAD', 'Dán thành đối tượng gốc, giữ layer, màu và thuộc tính. Giống Ctrl+V.', 'paste'], ['blk', 'Block', 'Dán thành một block mới đặt tên tự động (A$C…); đổi tên bằng RENAME.', 'insert'], ['img', 'Ảnh (PNG)', 'Dán thành ảnh raster nhúng vào bản vẽ. Ảnh không sửa được hình học.', 'image'], ['txt', 'Văn bản Unicode', 'Dán nội dung ' + texts.length + ' đối tượng chữ thành một MText.', 'mtext']].filter(function(f){ return f[0] !== 'txt' || texts.length; });
    var count = function(){ var c = {}; C.ents.forEach(function(e){ var k = TC.TYPE_NAMES[e.t] || e.t; c[k] = (c[k] || 0) + 1; }); return Object.keys(c).map(function(k){ return TC.n(c[k]) + ' ' + esc(k); }).join(', '); };
    var label = function(){ return { obj:'Dán ' + TC.n(n) + ' đối tượng', blk:'Dán thành block', img:'Dán thành ảnh', txt:'Dán thành MText' }[cur]; };
    return new Promise(function(res){
      var top = TC.topDialog(); if (top && top.id === 'pspec') top.close(false); // chạy lại lệnh khi hộp thoại cũ còn mở
      var dlg = TC.dialog({ id:'pspec', title:'Dán đặc biệt', cmd:'PASTESPEC', w:560, cls:'pspec', focus:has ? '#ps-l' : '[data-x="cancel"]', onClose:function(r){ if (r !== true) res(); },
        body:has ? '<div class="stack"><div class="frow" style="--lab-w:64px"><span class="lab">Nguồn</span><span><b>' + esc(C.from || 'TrionCAD') + '</b> · ' + count() + '</span></div>' +
            '<div class="psw"><div class="stack" style="gap:8px">' + H.rad('ps-m', 'paste', 'Dán', true) + '<label class="chk" aria-disabled="true" data-tip="Dán liên kết dùng cho nguồn OLE (Excel, Word). Clipboard đang chứa đối tượng TrionCAD."><input type="radio" name="ps-m" value="link" disabled> Dán liên kết</label></div>' +
            '<div class="stack" style="gap:4px"><span class="gh">Dạng</span><div class="tree psl" id="ps-l" role="listbox" aria-label="Dạng dán" tabindex="0">' + F.map(function(f){ return '<button class="tn" type="button" role="option" data-pf="' + f[0] + '" aria-selected="' + (f[0] === cur) + '">' + ic(f[3], 's12') + esc(f[1]) + '</button>'; }).join('') + '</div></div></div>' +
            H.chk('ps-icon', 'Hiện dạng biểu tượng', false, true) + '<div class="note" id="ps-r"></div></div>'
          : '<div class="empty">' + ic('paste') + '<b>Clipboard trống</b>Chọn đối tượng rồi bấm Ctrl+C (COPYCLIP) hoặc Ctrl+X, sau đó mở lại Dán đặc biệt.</div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="ps-ok"' + (has ? '' : ' disabled') + '>' + (has ? label() : 'Dán') + '</button>' });
      if (!has) return;
      function sync(){ $$('[data-pf]', dlg.el).forEach(function(b){ b.setAttribute('aria-selected', String(b.dataset.pf === cur)); }); var f = F.filter(function(x){ return x[0] === cur; })[0]; $('#ps-r', dlg.el).innerHTML = ic('info') + '<span><b>Kết quả:</b> ' + esc(f[2]) + '</span>'; $('#ps-ok', dlg.el).textContent = label(); }
      dlg.el.addEventListener('click', function(e){ var b = e.target.closest('[data-pf]'); if (b){ cur = b.dataset.pf; sync(); } });
      TC.listKeys($('#ps-l', dlg.el), '[data-pf]', function(b, enter){ cur = b.dataset.pf; sync(); if (enter) $('#ps-ok', dlg.el).click(); });
      $('#ps-ok', dlg.el).addEventListener('click', function(){ dlg.close(true); res(place()); });
      sync();
    });
    // điểm chèn có xem trước theo dạng đã chọn
    function place(){
      var d = D(), src = C.ents.map(function(e){ return TC.prepEnt(d, ED.clean(e), true); }), bb = [Infinity, Infinity, -Infinity, -Infinity]; src.forEach(function(e){ var b = e._bb; bb = [Math.min(bb[0], b[0]), Math.min(bb[1], b[1]), Math.max(bb[2], b[2]), Math.max(bb[3], b[3])]; });
      var mv = function(p){ return T.move(p[0] - C.base[0], p[1] - C.base[1]); }, w = Math.max(100, bb[2] - bb[0]), h = Math.max(100, bb[3] - bb[1]), th = texts.length ? texts[0].h : ED.cur().th;
      var mt = function(p){ return ED.make({ t:'mtext', p:p, w:Math.max(th * 12, w), h:th, s:texts.map(function(e){ return e.s; }).join('\n'), style:texts.length ? texts[0].style : ED.cur().tstyle, al:'l' }); };
      var img = function(p){ return ED.make({ t:'image', p:p, w:w, h:h, name:'clipboard-' + new Date().toTimeString().slice(0, 5).replace(':', '') + '.png', bright:50, contrast:50, fade:0 }); };
      return ctx.point(cur === 'img' ? 'Góc dưới trái của ảnh' : 'Điểm chèn', { onMove:function(p, temp){ if (cur === 'img') temp.push(prev(img(p))); else if (cur === 'txt') temp.push(prev(mt(p))); else temp.push({ k:'xents', list:src, m:mv(p).m }); } }).then(function(r){ if (!r.p) return;
        return ED.run('PASTESPEC', function(){
          if (cur === 'obj'){ var ids = C.ents.map(function(e){ return ED.add(ED.xform(ED.clean(e), mv(r.p))).id; }); d.sel = ids; TC.cad.afterSel(); ctx.out('Đã dán ' + TC.n(ids.length) + ' đối tượng dạng Đối tượng TrionCAD.'); return; }
          if (cur === 'blk'){ var nm = 'A$C' + Math.random().toString(16).slice(2, 8).toUpperCase(); var done = ED.state(function(){ return { b:d.blocks[nm] || null }; }, function(s){ if (s.b) d.blocks[nm] = s.b; else delete d.blocks[nm]; }); d.blocks[nm] = { name:nm, desc:'Dán đặc biệt dạng block', ents:C.ents.map(function(e){ return ED.xform(ED.clean(e), T.move(-C.base[0], -C.base[1])); }) }; done(); ED.add(ED.make({ t:'insert', name:nm, p:r.p, sx:1, sy:1, rot:0 })); ctx.out('Đã dán thành block <b>' + nm + '</b>. Đổi tên block bằng RENAME.'); return; }
          if (cur === 'img'){ var e = ED.add(img(r.p)); ctx.out('Đã dán thành ảnh ' + esc(e.name) + ' (' + TC.d(w, 0) + ' × ' + TC.d(h, 0) + ').'); return; }
          ED.add(mt(r.p)); ctx.out('Đã dán ' + texts.length + ' dòng chữ thành MText.'); }); });
    }
  };

  /* ================= Nhóm: GROUP, UNGROUP, GROUPEDIT, PICKSTYLE (UI-E05 dạng dòng lệnh) ================= */
  TC.S.pickstyle = TC.load('pickstyle', 1);
  var NOGRP = false;
  function groups(d){ return (d.groups = d.groups || []); }
  function live(d, g){ return g.ids.filter(function(id){ return d.byId[id]; }); }
  // sửa danh sách nhóm trong một bước hoàn tác
  function grpEdit(d, fn){ var st = ED.state(function(){ return groups(d); }, function(s){ d.groups = s; V().drawOver(); }); fn(); st(); }
  function grpOf(d, id){ return groups(d).filter(function(g){ return g.ids.indexOf(id) >= 0; }).sort(function(a, b){ return a.ids.length - b.ids.length; })[0] || null; }
  function grpLabel(g){ return g.anon ? 'nhóm không tên ' + esc(g.name) : 'nhóm “' + esc(g.name) + '”'; }
  // chọn một nhóm: chọn sẵn, bấm vào một đối tượng của nhóm, hoặc gõ tên
  function pickGroup(ctx, text){
    var d = D(); for (var i = 0; i < d.sel.length; i++){ var g0 = grpOf(d, d.sel[i]); if (g0) return Promise.resolve(g0); }
    return ctx.point(text, { pick:true, dyn:false, opts:[['Tên', 'N']] }).then(function(r){
      if (r.kw === 'N') return ctx.val('Tên nhóm hoặc [?]', groups(d)[0] ? groups(d)[0].name : '').then(function(v){ if (v === '?'){ ctx.out(groups(d).length ? 'Nhóm trong bản vẽ: ' + groups(d).map(function(g){ return esc(g.name) + ' (' + live(d, g).length + ')'; }).join(', ') + '.' : 'Bản vẽ chưa có nhóm nào.'); return pickGroup(ctx, text); } var g = groups(d).filter(function(x){ return x.name.toUpperCase() === String(v).toUpperCase(); })[0]; if (!g){ ctx.out('Không có nhóm “' + esc(v) + '”.'); return pickGroup(ctx, text); } return g; });
      if (!r.s) return null; var id = V().hitTest(r.s[0], r.s[1]), g = id && grpOf(d, id);
      if (!g){ ctip(r, id ? 'Đối tượng không thuộc nhóm nào' : 'Không có đối tượng tại đây'); return pickGroup(ctx, text); }
      return g; });
  }
  IMPL.GROUP = function(ctx){
    ctx.badge('G'); var d = D(), name = null, desc = '';
    var make = function(ids){ ids = ED.editable(ids.filter(function(id){ return d.byId[id]; }), ctx); if (!ids.length){ ctx.out('Chưa chọn đối tượng nào, nhóm chưa được tạo.'); return; } var gs = groups(d), k = 1, nm = name; if (!nm){ while (gs.some(function(g){ return g.name === '*A' + k; })) k++; nm = '*A' + k; }
      return ED.run('GROUP', function(){ grpEdit(d, function(){ groups(d).push({ name:nm, desc:desc, sel:true, ids:ids.slice(), anon:!name }); }); TC.cad.clearSel(); ctx.out('Đã tạo ' + (name ? 'nhóm “' + esc(nm) + '”' : 'nhóm không tên ' + nm) + ' với ' + TC.n(ids.length) + ' đối tượng.' + (TC.S.pickstyle & 1 ? '' : ' Chọn theo nhóm đang tắt (PICKSTYLE = 0).')); }); };
    if (d.sel.length) return make(d.sel.slice());
    ctx.out('Bấm hoặc kéo khung để chọn đối tượng, Enter để tạo nhóm.', 'faint');
    // trong lúc chờ, vùng vẽ ở chế độ chọn bình thường (bấm, Window, Crossing)
    function step(){ return ask(ctx, 'Chọn đối tượng hoặc', [['Tên', 'N'], ['Mô tả', 'D']]).then(function(k){
      if (k === 'N') return ctx.val('Tên nhóm hoặc [?]', 'NHOM-' + (groups(d).filter(function(g){ return !g.anon; }).length + 1)).then(function(v){ v = String(v).trim();
        if (v === '?'){ ctx.out(groups(d).length ? 'Nhóm trong bản vẽ: ' + groups(d).map(function(g){ return esc(g.name); }).join(', ') + '.' : 'Bản vẽ chưa có nhóm nào.'); return step(); }
        if (!v || /\s/.test(v) || v[0] === '*'){ ctx.out('Tên nhóm không được trống, không chứa khoảng trắng và không bắt đầu bằng *.'); return step(); }
        if (groups(d).some(function(g){ return g.name.toUpperCase() === v.toUpperCase(); })){ ctx.out('Đã có nhóm “' + esc(v) + '”. Chọn tên khác.'); return step(); }
        name = v; return step(); });
      if (k === 'D') return ctx.val('Mô tả nhóm', desc || '').then(function(v){ desc = String(v || ''); return step(); });
      if (k){ ctx.out('Gõ N để đặt tên, D để thêm mô tả, hoặc chọn đối tượng rồi Enter.'); return step(); }
      return make(d.sel.slice()); }); }
    return step();
  };
  IMPL.UNGROUP = function(ctx){
    ctx.badge('UG'); var d = D(), gs = [];
    d.sel.forEach(function(id){ var g = grpOf(d, id); if (g && gs.indexOf(g) < 0) gs.push(g); });
    var drop = function(list){ return ED.run('UNGROUP', function(){ grpEdit(d, function(){ d.groups = groups(d).filter(function(g){ return list.indexOf(g) < 0; }); }); TC.cad.clearSel(); ctx.out(list.length > 1 ? 'Đã rã ' + list.length + ' nhóm.' : 'Đã rã ' + grpLabel(list[0]) + ' (' + live(d, list[0]).length + ' đối tượng).'); }); };
    if (gs.length) return drop(gs);
    if (!groups(d).length){ ctx.out('Bản vẽ chưa có nhóm nào.'); return; }
    return pickGroup(ctx, 'Chọn nhóm').then(function(g){ if (g) return drop([g]); });
  };
  IMPL.GROUPEDIT = function(ctx){
    ctx.badge('GE'); var d = D();
    if (!groups(d).length){ ctx.out('Bản vẽ chưa có nhóm nào. Tạo nhóm bằng GROUP.'); return; }
    return pickGroup(ctx, 'Chọn nhóm').then(function(g){ if (!g) return; TC.cad.clearSel();
      var hl = function(){ d.found = live(d, g); V().drawOver(); }, end = function(){ d.found = []; NOGRP = false; V().drawOver(); };
      ctx.out('Đang sửa ' + grpLabel(g) + ' · ' + live(d, g).length + ' đối tượng (tô sáng).'); hl();
      return ED.run('GROUPEDIT', function menu(){ return ask(ctx, 'Nhập một tùy chọn', [['Thêm đối tượng', 'A'], ['Bớt đối tượng', 'R'], ['Đổi tên', 'REN']]).then(function(k){
        if (k === 'A' || k === 'R'){ NOGRP = true; return ctx.sel(k === 'A' ? 'Chọn đối tượng cần thêm vào nhóm' : 'Chọn đối tượng cần bớt khỏi nhóm').then(function(ids){ NOGRP = false; ids = ED.editable(ids, ctx); var n0 = live(d, g).length; grpEdit(d, function(){ var x = groups(d).filter(function(y){ return y.name === g.name; })[0] || g; if (k === 'A') ids.forEach(function(id){ if (x.ids.indexOf(id) < 0) x.ids.push(id); }); else x.ids = x.ids.filter(function(id){ return ids.indexOf(id) < 0; }); g = x; }); TC.cad.clearSel(); var n1 = live(d, g).length; ctx.out((k === 'A' ? 'Đã thêm ' + (n1 - n0) : 'Đã bớt ' + (n0 - n1)) + ' đối tượng. Nhóm có ' + n1 + ' đối tượng.'); hl(); return menu(); }); }
        if (k === 'REN') return ctx.val('Tên mới của nhóm', g.anon ? 'NHOM-' + (groups(d).length + 1) : g.name).then(function(v){ v = String(v).trim(); if (!v || /\s/.test(v) || v[0] === '*'){ ctx.out('Tên nhóm không được trống, không chứa khoảng trắng và không bắt đầu bằng *.'); return menu(); } if (groups(d).some(function(x){ return x !== g && x.name.toUpperCase() === v.toUpperCase(); })){ ctx.out('Đã có nhóm “' + esc(v) + '”.'); return menu(); } var old = g.name; grpEdit(d, function(){ var x = groups(d).filter(function(y){ return y.name === old; })[0] || g; x.name = v; x.anon = false; g = x; }); ctx.out('Đã đổi tên nhóm ' + esc(old) + ' thành “' + esc(v) + '”.'); return menu(); });
        if (k){ ctx.out('Tùy chọn “' + esc(k) + '” không có. Chọn Thêm đối tượng · A, Bớt đối tượng · R hoặc Đổi tên · REN.'); return menu(); }
      }); }).then(end, function(err){ end(); throw err; }); });
  };
  // PICKSTYLE: bấm nút (ribbon, menu) thì bật/tắt ngay; gõ lệnh hoặc chạy script thì hỏi giá trị như AutoCAD
  IMPL.PICKSTYLE = function(ctx, pre){
    var ev = window.event;
    var set = function(v){ TC.S.pickstyle = v; TC.save('pickstyle', v); ctx.out('PICKSTYLE = ' + v + ' · chọn theo nhóm: <b>' + (v & 1 ? 'bật' : 'tắt') + '</b>' + (v & 2 ? ', chọn hatch kèm đường biên liên kết' : '') + '.'); V().drawOver(); A().renderRibbon(); };
    if (pre == null && ev && ev.type === 'click') return set(TC.S.pickstyle & 1 ? TC.S.pickstyle & 2 : TC.S.pickstyle | 1);
    if (pre != null && /^[0-3]$/.test(pre)) return set(Number(pre));
    return ctx.kw('Giá trị mới cho PICKSTYLE', [['Tắt', '0'], ['Chọn theo nhóm', '1'], ['Hatch liên kết', '2'], ['Cả hai', '3']], String(TC.S.pickstyle)).then(function(k){ if (/^[0-3]$/.test(k)) set(Number(k)); else ctx.out('PICKSTYLE nhận giá trị từ 0 đến 3.'); });
  };
  // chọn theo nhóm: bấm hoặc kéo khung trúng một đối tượng của nhóm thì chọn cả nhóm; Shift bớt cả nhóm
  function grpSel(before){
    var d = D(); if (!d || NOGRP || !(TC.S.pickstyle & 1) || !d.groups || !d.groups.length) return;
    var add = d.sel.filter(function(id){ return before.indexOf(id) < 0; }), rem = before.filter(function(id){ return d.sel.indexOf(id) < 0; }); if (!add.length && !rem.length) return;
    d.groups.forEach(function(g){ if (g.sel === false) return; var ids = live(d, g); if (ids.some(function(id){ return add.indexOf(id) >= 0; })) ids.forEach(function(id){ if (d.sel.indexOf(id) < 0) d.sel.push(id); }); if (ids.some(function(id){ return rem.indexOf(id) >= 0; })) d.sel = d.sel.filter(function(id){ return ids.indexOf(id) < 0; }); });
    TC.cad.afterSel(); if (TC.cad.mode() === 'select') TC.cmdline.status('Đã chọn ' + TC.n(d.sel.length) + ' · Enter để kết thúc chọn');
  }
  var click0 = TC.cad.click, box0 = TC.cad.boxDone;
  TC.cad.click = function(p, e){ var d = D(), b = d ? d.sel.slice() : [], r = click0.apply(this, arguments); if (r) grpSel(b); return r; };
  TC.cad.boxDone = function(a, b, e){ var d = D(), s = d ? d.sel.slice() : [], r = box0.apply(this, arguments); grpSel(s); return r; };
  // khung đứt nét quanh nhóm đang được chọn trọn
  TC.overlays.push(function(ctx, v){ var d = v.doc; if (!(TC.S.pickstyle & 1) || !d || !d.groups || !d.sel.length || TC.cmdline.busy()) return; d.groups.forEach(function(g){ var ids = live(d, g); if (g.sel === false || ids.length < 2 || !ids.every(function(id){ return d.sel.indexOf(id) >= 0; })) return; var b = ED.bbox(ids), a = v.mToS([b[0], b[1]]), c = v.mToS([b[2], b[3]]); if (!a || !c) return; ctx.save(); ctx.strokeStyle = TC.cssv('--c-hl-sel'); ctx.lineWidth = 1; ctx.setLineDash([5, 4]); ctx.strokeRect(Math.round(Math.min(a[0], c[0])) - 5.5, Math.round(Math.min(a[1], c[1])) - 5.5, Math.round(Math.abs(c[0] - a[0])) + 11, Math.round(Math.abs(c[1] - a[1])) + 11); ctx.restore(); }); });

  /* ================= Kích thước: cung, gãy khúc, tọa độ, ghi nhanh, ngắt, giãn cách ================= */
  function dimText(e, v){ return e.over ? e.over.replace('<>', v) : v; }
  X.dimarc = function(x){
    var e = x.e, P = x.P, k = x.t ? x.t.k : 1, sw = sweep(e.a0, e.a1) || 360, L = e.r * rad(sw), out = e.rd >= e.r ? 1 : -1, am = e.a0 + sw / 2, raw = ((am - 90) % 360 + 360) % 360, flip = raw > 90 && raw <= 270, rt = flip ? raw - 180 : raw, s = dimText(e, String(Math.round(L)));
    e._meas = L;
    x.o({ k:'arc', c:P(e.c), r:e.rd * k, a0:e.a0, a1:e.a1 });
    [e.a0, e.a1].forEach(function(an){ x.o({ k:'seg', a:P(polar(e.c, e.r + out * 60, an)), b:P(polar(e.c, e.rd + out * 160, an)) }); x.o({ k:'tick', p:P(polar(e.c, e.rd, an)) }); });
    var tp = polar(e.c, e.rd + 230 + (flip ? 200 : 0), am), up = rt + 90, cs = polar(tp, 67, up);
    x.o({ k:'text', p:P(tp), h:220, s:s, rot:rt, al:'m', style:'Standard' });
    x.o({ k:'arc', c:P(cs), r:130 * k, a0:up - 55, a1:up + 55 }); // ký hiệu độ dài cung trên chữ
  };
  X.dimj = function(x){
    var e = x.e, P = x.P, u = [Math.cos(rad(e.ang)), Math.sin(rad(e.ang))], v = [-u[1], u[0]], A1 = add2(e.c, u, e.r), jp = e.jp || mid(e.oc, A1), J2 = add2(e.c, u, dot(sub(jp, e.c), u)), J1 = add2(e.oc, u, dot(sub(jp, e.oc), u)), s = dimText(e, 'R' + Math.round(e.r));
    e._meas = e.r;
    [[e.oc, J1], [J1, J2], [J2, A1]].forEach(function(q){ if (G.dist(q[0], q[1]) > 1e-6) x.o({ k:'seg', a:P(q[0]), b:P(q[1]) }); });
    x.o({ k:'tick', p:P(A1) });
    var sd = dot(sub(J1, J2), v) > 0 ? -1 : 1; x.o({ k:'text', p:P(add2(mid(J2, A1), v, sd * 210)), h:220, s:s, rot:upr(e.ang), al:'m', style:'Standard' });
  };
  X.dimo = function(x){
    var e = x.e, P = x.P, p = e.p, q = e.q, ox = e.axis === 'x', val = ox ? p[0] : p[1], s = dimText(e, String(Math.round(val))), w = 220 * .62 * s.length, i = ox ? 1 : 0, j = 1 - i, sg = q[i] >= p[i] ? 1 : -1, pt = function(a, b){ var z = [0, 0]; z[i] = a; z[j] = b; return z; };
    e._meas = val;
    var a = pt(p[i] + sg * 80, p[j]), m1 = pt(p[i] + (q[i] - p[i]) * .4, p[j]), m2 = pt(p[i] + (q[i] - p[i]) * .6, q[j]);
    [[a, m1], [m1, m2], [m2, q]].forEach(function(z){ if (G.dist(z[0], z[1]) > 1e-6 && sg * (z[1][i] - z[0][i]) >= -1e-6) x.o({ k:'seg', a:P(z[0]), b:P(z[1]) }); });
    x.o({ k:'text', p:P(pt(q[i] + sg * (w / 2 + 90), q[j])), h:220, s:s, rot:ox ? 90 : 0, al:'m', style:'Standard' });
  };
  // cung (hoặc đoạn cung của polyline) gần điểm bấm
  function arcAt(e, p){ if (e.t === 'arc') return { c:e.c, r:e.r, a0:e.a0, a1:e.a1 }; var best = null, bd = Infinity; (e._p || []).forEach(function(pr){ if (pr.k !== 'arc') return; var dd = Math.abs(G.dist(p, pr.c) - pr.r); if (dd < bd && G.inArc(G.ang(pr.c, p), pr.a0, pr.a1)){ bd = dd; best = pr; } }); return best && { c:best.c, r:best.r, a0:best.a0, a1:best.a1 }; }
  function dimOpts(ctx, st, more){ return function(q){ if (q.kw === 'M' || q.kw === 'T') return ctx.val('Chữ kích thước (<> là giá trị đo)', st.txt || '<>').then(function(v){ st.txt = v && v !== '<>' ? v : null; }); if (q.kw === 'A') return ctx.val('Góc của chữ kích thước', '0'); return more ? more(q) : Promise.resolve(); }; }
  IMPL.DIMARC = function(ctx){
    ctx.badge('DAR');
    return ED.pick(ctx, 'Chọn cung hoặc đoạn cung của polyline', { types:['arc', 'pline'], bad:'Chọn cung' }).then(function(r){ if (!r.e) return; var a = arcAt(r.e, r.p); if (!a){ ctip(r, 'Đoạn polyline này không phải cung'); return IMPL.DIMARC(ctx); }
      var st = { txt:null }, o = { a0:a.a0, a1:a.a1 };
      var mk = function(p){ return ED.make({ t:'dimarc', c:a.c, r:a.r, a0:o.a0, a1:o.a1, rd:Math.max(a.r * .2, G.dist(a.c, p)), style:dimStyle(), over:st.txt || undefined }); };
      var more = function(q){ if (q.kw === 'P') return ctx.point('Điểm đầu của phần cung cần ghi', { base:a.c }).then(function(s){ if (!s.p) return; return ctx.point('Điểm cuối của phần cung cần ghi', { base:a.c }).then(function(t){ if (t.p){ o.a0 = G.ang(a.c, s.p); o.a1 = G.ang(a.c, t.p); if (sweep(o.a0, o.a1) > sweep(a.a0, a.a1)){ var z = o.a0; o.a0 = o.a1; o.a1 = z; } } }); }); if (q.kw === 'L'){ ctx.out('Đường dẫn: dùng cho cung lớn hơn 90°; prototype đặt chữ trên cung kích thước.'); } return Promise.resolve(); };
      ctx.out('Độ dài cung = ' + TC.d(a.r * rad(sweep(a.a0, a.a1)), 0));
      return (function place(){ return ctx.point('Vị trí kích thước độ dài cung', { rubber:false, dyn:false, opts:[['Mtext', 'M'], ['Chữ', 'T'], ['Góc', 'A'], ['Một phần', 'P'], ['Đường dẫn', 'L']], onMove:function(p, temp){ temp.push(prev(mk(p))); } }).then(function(q){
        if (q.kw) return dimOpts(ctx, st, more)(q).then(place);
        if (q.p) return ED.run('DIMARC', function(){ var e = ED.add(mk(q.p)); D().lastDim = e.id; ctx.out('Chữ kích thước = ⌒' + TC.d(e._meas, 0)); }); }); })(); });
  };
  IMPL.DIMJOGGED = function(ctx){
    ctx.badge('DJO');
    return ED.pick(ctx, 'Chọn cung hoặc đường tròn', { types:['arc', 'circle'], bad:'Chọn cung hoặc đường tròn' }).then(function(r){ if (!r.e) return; var e = r.e, st = { txt:null };
      ctx.out('Chữ kích thước = R' + Math.round(e.r));
      var mk = function(oc, ang, jp){ return ED.make({ t:'dimj', c:e.c.slice(), r:e.r, oc:oc, ang:ang, jp:jp, style:dimStyle(), over:st.txt || undefined }); };
      return ctx.point('Vị trí tâm thay thế', { onMove:function(p, temp){ temp.push({ k:'line', a:e.c, b:p, color:TC.cssv('--c-track'), dash:[3, 3] }); } }).then(function(o){ if (!o.p) return; var oc = o.p;
        return (function pos(){ return ctx.point('Vị trí đường kích thước', { rubber:false, dyn:false, opts:[['Mtext', 'M'], ['Chữ', 'T'], ['Góc', 'A']], onMove:function(p, temp){ temp.push(prev(mk(oc, G.ang(e.c, p), null))); } }).then(function(q){
          if (q.kw) return dimOpts(ctx, st)(q).then(pos);
          if (!q.p) return; var an = G.ang(e.c, q.p);
          return ctx.point('Vị trí gãy', { rubber:false, onMove:function(p, temp){ temp.push(prev(mk(oc, an, p))); } }).then(function(z){ if (!z.p) return; return ED.run('DIMJOGGED', function(){ var de = ED.add(mk(oc, an, z.p)); D().lastDim = de.id; ctx.out('Chữ kích thước = ' + esc(dimText(de, 'R' + Math.round(e.r)))); }); }); }); })(); }); });
  };
  IMPL.DIMORDINATE = function(ctx){
    ctx.badge('DOR');
    return ctx.point('Vị trí điểm cần ghi').then(function(r){ if (!r.p) return; var p = r.p, force = null, st = { txt:null };
      var axisOf = function(q){ return force || (Math.abs(q[0] - p[0]) >= Math.abs(q[1] - p[1]) ? 'y' : 'x'); };
      var mk = function(q){ return ED.make({ t:'dimo', p:p, q:q, axis:axisOf(q), style:dimStyle(), over:st.txt || undefined }); };
      return (function pos(){ return ctx.point('Vị trí đầu đường dẫn', { base:p, rubber:false, dyn:false, opts:[['Mốc X', 'X'], ['Mốc Y', 'Y'], ['Mtext', 'M'], ['Chữ', 'T'], ['Góc', 'A']], onMove:function(q, temp){ temp.push(prev(mk(q))); } }).then(function(q){
        if (q.kw === 'X' || q.kw === 'Y'){ force = q.kw.toLowerCase(); return pos(); }
        if (q.kw) return dimOpts(ctx, st)(q).then(pos);
        if (q.p) return ED.run('DIMORDINATE', function(){ var e = ED.add(mk(q.p)); D().lastDim = e.id; ctx.out('Chữ kích thước = ' + esc(dimText(e, String(Math.round(e._meas)))) + ' (' + (e.axis === 'x' ? 'mốc X' : 'mốc Y') + ').'); }); }); })(); });
  };
  var QD = { mode:'C' }, QN = { C:'Nối tiếp', S:'So le', B:'Cùng gốc', O:'Tọa độ', R:'Bán kính', D:'Đường kính' }, DSP = 600; // DIMDLI 6 mm × DIMSCALE 100
  IMPL.QDIM = function(ctx){
    ctx.badge('QD');
    return ctx.sel('Chọn hình học cần ghi kích thước').then(function(ids){ var es = ents(ids); TC.cad.clearSel(); if (!es.length) return;
      var pts = [], arcs = es.filter(function(e){ return e.t === 'circle' || e.t === 'arc'; }), datum = null;
      es.forEach(function(e){ if (e.t === 'line') pts.push(e.p1, e.p2); else if (e.t === 'pline') Array.prototype.push.apply(pts, e.pts); else if (e.t === 'arc') pts.push(polar(e.c, e.r, e.a0), polar(e.c, e.r, e.a1)); else if (e.t === 'circle' || e.t === 'ellipse') pts.push(e.c); else if (e.p && /^(insert|point)$/.test(e.t)) pts.push(e.p); });
      if (pts.length < 2 && !arcs.length){ ctx.out('Không đủ điểm để ghi kích thước. Chọn line, polyline, cung, đường tròn hoặc block.'); return; }
      var bb = bbOf(pts);
      function series(p){
        var m = QD.mode;
        if (m === 'R' || m === 'D') return arcs.map(function(a){ return ED.make({ t:'dimr', c:a.c, r:a.r, ang:G.ang(a.c, p), dia:m === 'D', style:dimStyle() }); });
        var hz = p[1] > bb[3] || p[1] < bb[1] ? true : p[0] > bb[2] || p[0] < bb[0] ? false : bb[2] - bb[0] >= bb[3] - bb[1], k = hz ? 0 : 1, at = hz ? p[1] : p[0], u = [];
        pts.slice().sort(function(a, b){ return a[k] - b[k]; }).forEach(function(q){ if (!u.length || q[k] - u[u.length - 1][k] > 1) u.push(q); });
        var sg = at >= (hz ? (bb[1] + bb[3]) / 2 : (bb[0] + bb[2]) / 2) ? 1 : -1, mk = function(a, b, off){ return ED.dimEnt(a, b, hz ? [0, at + sg * off] : [at + sg * off, 0], hz ? 'h' : 'v'); };
        if (m === 'O') return u.map(function(q){ return ED.make({ t:'dimo', p:q, q:hz ? [q[0], at] : [at, q[1]], axis:hz ? 'x' : 'y', style:dimStyle() }); });
        if (u.length < 2) return [];
        if (m === 'B'){ var b0 = datum ? u.reduce(function(s, q){ return Math.abs(q[k] - datum[k]) < Math.abs(s[k] - datum[k]) ? q : s; }) : u[0]; return u.filter(function(q){ return q !== b0; }).sort(function(a, b){ return Math.abs(a[k] - b0[k]) - Math.abs(b[k] - b0[k]); }).map(function(q, i){ return mk(b0, q, i * DSP); }); }
        if (m === 'S'){ var out = [], h = Math.floor(u.length / 2); for (var i = 0; i < h; i++) out.push(mk(u[i], u[u.length - 1 - i], (h - 1 - i) * DSP)); return out; }
        return u.slice(1).map(function(q, i){ return mk(u[i], q, 0); });
      }
      function loop(){ return ctx.point('Vị trí đường kích thước', { rubber:false, dyn:false, def:QN[QD.mode], opts:[['Nối tiếp', 'C'], ['So le', 'S'], ['Cùng gốc', 'B'], ['Tọa độ', 'O'], ['Bán kính', 'R'], ['Đường kính', 'D'], ['Điểm mốc', 'P'], ['Sửa', 'E'], ['Thiết lập', 'T']], onMove:function(p, temp){ temp.push(ED.temp(series(p).slice(0, 80), { real:true })); } }).then(function(r){
        if (QN[r.kw]){ QD.mode = r.kw; if ((r.kw === 'R' || r.kw === 'D') && !arcs.length) ctx.out('Vùng chọn không có cung hoặc đường tròn để ghi ' + QN[r.kw].toLowerCase() + '.'); return loop(); }
        if (r.kw === 'P') return ctx.point('Điểm mốc mới').then(function(q){ if (q.p){ datum = q.p; ctx.out('Điểm mốc: ' + TC.d(q.p[0]) + ', ' + TC.d(q.p[1]) + ' (dùng cho Cùng gốc).'); } return loop(); });
        if (r.kw === 'E'){ ctx.out('Sửa dãy: prototype ghi mọi điểm cuối, đỉnh và tâm của hình đã chọn; muốn bớt điểm thì chọn lại hình.'); return loop(); }
        if (r.kw === 'T') return ctx.kw('Ưu tiên điểm khi ghi', [['Điểm cuối', 'E'], ['Giao điểm', 'I']], 'E').then(loop);
        if (!r.p) return; var list = series(r.p);
        if (!list.length){ ctx.out('Không đủ điểm khác nhau theo phương này. Đổi phương bằng cách đặt đường kích thước ở phía khác.'); return loop(); }
        return ED.run('QDIM', function(){ list.forEach(function(o){ ED.add(o); }); V().draw(); ctx.out('Đã tạo ' + list.length + ' kích thước ' + QN[QD.mode].toLowerCase() + '.'); }); }); }
      return loop(); });
  };
  // ngắt kích thước: cắt nguyên tố đường của kích thước tại chỗ giao với đối tượng khác (tự động, theo đối tượng chọn, hoặc thủ công)
  var BRK = 300, DIMT = ['dim', 'dimr', 'dima', 'dimarc', 'dimj', 'dimo']; // khe ngắt: DIMBREAK 3 mm × DIMSCALE 100
  function segCuts(a, b, pr){ if (pr.k === 'seg'){ var q = G.segInt(a, b, pr.a, pr.b); return q ? [G.dist(a, q) / (G.dist(a, b) || 1)] : []; } var dx = b[0] - a[0], dy = b[1] - a[1], fx = a[0] - pr.c[0], fy = a[1] - pr.c[1], A2 = dx * dx + dy * dy, B2 = 2 * (fx * dx + fy * dy), C2 = fx * fx + fy * fy - pr.r * pr.r, D2 = B2 * B2 - 4 * A2 * C2; if (D2 < 0 || !A2) return []; var sq = Math.sqrt(D2); return [(-B2 - sq) / (2 * A2), (-B2 + sq) / (2 * A2)].filter(function(t){ return t >= 0 && t <= 1 && (pr.k !== 'arc' || G.inArc(G.ang(pr.c, [a[0] + dx * t, a[1] + dy * t]), pr.a0, pr.a1)); }); }
  function cutBreaks(d, e){
    var b = e.brk, cut = [], eb = e._bb, out = [];
    if (b.auto || b.ids) (ED.listOf(d, e) || d.ents).forEach(function(x){ if (x === e || !x._p || !x._bb || !TC.entVisible(d, x) || (b.ids && b.ids.indexOf(x.id) < 0) || /^(text|mtext|point|hatch|gradient|wipeout)$/.test(x.t)) return; var q = x._bb; if (q[2] < eb[0] || q[0] > eb[2] || q[3] < eb[1] || q[1] > eb[3]) return; x._p.forEach(function(pr){ if ((pr.k === 'seg' && !pr.inf) || pr.k === 'arc' || pr.k === 'circ') cut.push(pr); }); });
    e._p.forEach(function(pr){ if (pr.k !== 'seg'){ out.push(pr); return; } var L = G.dist(pr.a, pr.b) || 1, g = BRK / 2 / L, gaps = [];
      cut.forEach(function(c){ segCuts(pr.a, pr.b, c).forEach(function(t){ if (t > .002 && t < .998) gaps.push([t - g, t + g]); }); });
      (b.man || []).forEach(function(m){ if (G.segDist(m[0], pr.a, pr.b) < 150 && G.segDist(m[1], pr.a, pr.b) < 150){ var t0 = G.foot(m[0], pr.a, pr.b).t, t1 = G.foot(m[1], pr.a, pr.b).t; gaps.push([Math.min(t0, t1), Math.max(t0, t1)]); } });
      if (!gaps.length){ out.push(pr); return; }
      gaps.sort(function(x, y){ return x[0] - y[0]; }); var t = 0;
      gaps.forEach(function(gp){ if (gp[0] > t) out.push(Object.assign({}, pr, { a:lerp(pr.a, pr.b, t), b:lerp(pr.a, pr.b, gp[0]) })); t = Math.max(t, gp[1]); });
      if (t < 1) out.push(Object.assign({}, pr, { a:lerp(pr.a, pr.b, t), b:pr.b })); });
    e._p = out;
  }
  // ponytail: nguyên tố của kích thước do canvas.js dựng, nên ngắt dim là bước hậu xử lý gắn vào prepEnt/prepDoc; ngắt tự động chỉ tính lại khi kích thước được dựng lại
  var prep0 = TC.prepEnt; TC.prepEnt = function(doc, e, temp){ prep0(doc, e, temp); if (e.brk) cutBreaks(doc, e); return e; };
  var pdoc0 = TC.prepDoc; TC.prepDoc = function(doc){ pdoc0(doc); [doc.ents].concat(doc.layouts.map(function(L){ return L.ents; })).forEach(function(l){ l.forEach(function(e){ if (e.brk) cutBreaks(doc, e); }); }); };
  IMPL.DIMBREAK = function(ctx){
    ctx.badge('DBR');
    var apply = function(ds, brk){ return ED.run('DIMBREAK', function(){ ds.forEach(function(e){ ED.mod(e, function(x){ if (brk) x.brk = JSON.parse(JSON.stringify(brk)); else delete x.brk; }); }); V().draw(); TC.cad.clearSel(); ctx.out(brk ? 'Đã ngắt ' + ds.length + ' kích thước' + (brk.auto ? ' tại mọi chỗ giao với đối tượng khác' : '') + '.' : 'Đã bỏ ngắt ở ' + ds.length + ' kích thước.'); }); };
    return ED.pick(ctx, 'Chọn kích thước cần thêm hoặc bỏ ngắt', { opts:[['Nhiều', 'M']], types:DIMT, bad:'Chọn kích thước' }).then(function(r){
      if (r.kw === 'M') return ctx.sel('Chọn các kích thước').then(function(ids){ var ds = ents(ED.editable(ids, ctx)).filter(function(e){ return DIMT.indexOf(e.t) >= 0; }); if (!ds.length){ ctx.out('Không có kích thước trong vùng chọn.'); return; } return ctx.kw('Ngắt kích thước', [['Tự động', 'A'], ['Bỏ', 'R']], 'A').then(function(k){ return apply(ds, k === 'R' ? null : { auto:true }); }); });
      if (!r.e) return; var e = r.e;
      return ED.pick(ctx, 'Chọn đối tượng cắt kích thước hoặc', { opts:[['Tự động', 'A'], ['Thủ công', 'M'], ['Bỏ', 'R']], def:'Tự động', locked:true }).then(function(q){
        if (q.enter || q.kw === 'A') return apply([e], { auto:true });
        if (q.kw === 'R') return apply([e], null);
        if (q.kw === 'M') return ctx.point('Điểm ngắt thứ nhất').then(function(a){ if (!a.p) return; return ctx.point('Điểm ngắt thứ hai', { base:a.p }).then(function(b){ if (b.p) return apply([e], Object.assign({}, e.brk || {}, { man:((e.brk || {}).man || []).concat([[a.p, b.p]]) })); }); });
        var ids = [q.e.id];
        return (function more(){ return ED.pick(ctx, 'Chọn đối tượng cắt kích thước tiếp theo', { def:'thoát', locked:true }).then(function(z){ if (z.e){ if (ids.indexOf(z.e.id) < 0) ids.push(z.e.id); return more(); } return apply([e], { ids:ids }); }); })(); }); });
  };
  IMPL.DIMSPACE = function(ctx){
    ctx.badge('DSP');
    return ED.pick(ctx, 'Chọn kích thước gốc', { types:['dim'], bad:'Chọn kích thước thẳng' }).then(function(r){ if (!r.e) return; var base = r.e;
      return ctx.sel('Chọn các kích thước cần giãn cách').then(function(ids){ var ds = ents(ED.editable(ids, ctx)).filter(function(e){ return e.t === 'dim' && e !== base && e.dir === base.dir; });
        if (!ds.length){ ctx.out('Chọn ít nhất một kích thước thẳng cùng phương với kích thước gốc.'); return; }
        return ctx.kw('Nhập khoảng cách hoặc', [['Tự động', 'A']], 'Tự động').then(function(v){ var gap = v === 'A' || v === 'Tự động' ? 440 : Number(v); if (!(gap >= 0)){ ctx.out('Khoảng cách phải là số không âm.'); return; }
          var pv = function(e){ return e.dir === 'a' ? e.off || 0 : e.pos; }, b0 = pv(base), def = base.dir === 'a' ? Math.sign(base.off || 1) : Math.sign(base.pos - (base.dir === 'h' ? base.p1[1] : base.p1[0])) || 1, cnt = { 1:0, '-1':0 };
          ds.sort(function(a, b){ return Math.abs(pv(a) - b0) - Math.abs(pv(b) - b0); });
          return ED.run('DIMSPACE', function(){ ds.forEach(function(e){ var sg = Math.sign(pv(e) - b0) || def, i = ++cnt[sg]; ED.mod(e, function(x){ var nv = b0 + sg * gap * (gap ? i : 0); if (x.dir === 'a') x.off = nv; else x.pos = nv; }); }); TC.cad.clearSel(); ctx.out(gap ? 'Đã giãn cách ' + ds.length + ' kích thước, khoảng cách ' + TC.d(gap) + '.' : 'Đã gióng thẳng hàng ' + ds.length + ' kích thước với kích thước gốc.'); }); }); }); });
  };

  /* ================= Multileader: gióng hàng, gom nhóm ================= */
  var MLA = { mode:'U', sp:1000 };
  function mlLay(ms, ref, b, p){
    var u = unit(sub(p, b)); if (!(G.dist(p, b) > 1e-6)) u = [0, -1];
    var pr = ms.map(function(e){ return dot(sub(e.pts[1], b), u); }), ord = ms.map(function(e, i){ return i; }).sort(function(x, y){ return pr[x] - pr[y]; }), lo = Math.min.apply(null, pr), hi = Math.max.apply(null, pr), ri = ms.indexOf(ref), rk = ord.indexOf(ri), dref = sub(ref.pts[1], ref.pts[0]);
    return ms.map(function(e, i){ var x = ED.clean(e), t = pr[i], k = ord.indexOf(i);
      if (MLA.mode === 'D') t = ms.length > 1 ? lo + (hi - lo) * k / (ms.length - 1) : t;
      if (MLA.mode === 'S') t = (k - rk) * MLA.sp;
      var q = add2(b, u, t);
      if (MLA.mode === 'P' && e !== ref){ var X2 = ED.lineInt(b, add2(b, u, 1), e.pts[0], add2(e.pts[0], dref, 1)); if (X2) q = X2; }
      x.pts = [e.pts[0].slice(), q]; return x; });
  }
  IMPL.MLEADERALIGN = function(ctx){
    ctx.badge('MLA');
    return ctx.sel('Chọn multileader').then(function(ids){ var ms = ents(ED.editable(ids, ctx), /^mleader$/); TC.cad.clearSel(); if (ms.length < 2){ ctx.out('Chọn ít nhất 2 multileader.'); return; }
      ctx.out('Tùy chọn hiện tại: ' + { U:'Dùng khoảng cách hiện tại', D:'Phân bố', P:'Làm đường dẫn song song', S:'Khoảng cách ' + TC.d(MLA.sp) }[MLA.mode]);
      function pickRef(){ return ED.pick(ctx, 'Chọn multileader để gióng theo hoặc', { types:['mleader'], opts:[['Tùy chọn', 'O']], bad:'Chọn multileader' }).then(function(r){
        if (r.kw === 'O') return ctx.kw('Nhập một tùy chọn', [['Phân bố', 'D'], ['Làm đường dẫn song song', 'P'], ['Khoảng cách', 'S'], ['Dùng khoảng cách hiện tại', 'U']], MLA.mode).then(function(k){ if (k === 'S') return ctx.val('Khoảng cách giữa các multileader', TC.d(MLA.sp)).then(function(v){ var n = Number(v); if (n > 0) MLA.sp = n; MLA.mode = 'S'; return pickRef(); }); if (/^[DPU]$/.test(k)) MLA.mode = k; return pickRef(); });
        if (!r.e) return; var ref = ms.indexOf(r.e) >= 0 ? r.e : ms[0], b = r.e.pts[1];
        return ctx.point('Hướng', { base:b, onMove:function(p, temp){ temp.push(ED.temp(mlLay(ms, ref, b, p), { real:true })); } }).then(function(q){ if (!q.p) return; var L = mlLay(ms, ref, b, q.p);
          return ED.run('MLEADERALIGN', function(){ ms.forEach(function(e, i){ ED.mod(e, function(x){ x.pts = L[i].pts; }); }); ctx.out('Đã gióng hàng ' + ms.length + ' multileader.'); }); }); }); }
      return pickRef(); });
  };
  IMPL.MLEADERCOLLECT = function(ctx){
    ctx.badge('MLC');
    return ctx.sel('Chọn multileader').then(function(ids){ var ms = ents(ED.editable(ids, ctx), /^mleader$/); TC.cad.clearSel(); if (ms.length < 2){ ctx.out('Chọn ít nhất 2 multileader để gom.'); return; }
      var how = 'V', wrapW = 3000, f = ms[0];
      var text = function(){ var parts = ms.map(function(e){ return String(e.s || '').replace(/\n/g, ' ').trim(); }).filter(Boolean); if (how === 'H') return parts.join(', '); if (how !== 'W') return parts.join('\n'); var per = Math.max(8, Math.floor(wrapW / (f.h * .62))), lines = [], cur = ''; parts.join(', ').split(' ').forEach(function(w){ if (cur && (cur + ' ' + w).length > per){ lines.push(cur); cur = w; } else cur = cur ? cur + ' ' + w : w; }); lines.push(cur); return lines.join('\n'); };
      var mk = function(p){ var x = ED.clean(f); x.pts = [f.pts[0].slice(), p]; x.s = text(); delete x.more; return x; };
      function place(){ return ctx.point('Vị trí multileader đã gom', { base:f.pts[0], opts:[['Ngang', 'H'], ['Dọc', 'V'], ['Gói', 'W']], def:{ H:'Ngang', V:'Dọc', W:'Gói' }[how], onMove:function(p, temp){ temp.push(prev(mk(p))); } }).then(function(r){
        if (r.kw === 'H' || r.kw === 'V'){ how = r.kw; return place(); }
        if (r.kw === 'W') return ctx.val('Bề rộng gói chữ', TC.d(wrapW)).then(function(v){ wrapW = Number(v) > 0 ? Number(v) : wrapW; how = 'W'; return place(); });
        if (!r.p) return; var nx = mk(r.p);
        return ED.run('MLEADERCOLLECT', function(){ ED.mod(f, function(x){ x.pts = nx.pts; x.s = nx.s; delete x.more; }); ms.slice(1).forEach(function(e){ ED.del(e); }); ctx.out('Đã gom ' + ms.length + ' multileader thành 1.'); }); }); }
      return place(); });
  };

  /* ================= Chữ: đổi tỉ lệ, đổi điểm canh, gộp thành MText ================= */
  var TXT = /^(text|mtext|attdef)$/;
  // đo chữ bằng đúng font của vùng vẽ: đo ở cỡ 100 px rồi quy ra đơn vị bản vẽ (cỡ font = 1,32 × chiều cao chữ, như canvas.js)
  var MC = document.createElement('canvas').getContext('2d');
  function tMet(e){ var s = String(e.t === 'attdef' ? e.tag : e.s), k = e.h * 1.32 / 100; MC.font = V().textFont({ style:e.style }, 100); MC.textBaseline = 'alphabetic'; var a = MC.measureText(s); MC.textBaseline = 'middle'; var b = MC.measureText(s); return { w:a.width * k, mid:(a.actualBoundingBoxAscent - b.actualBoundingBoxAscent) * k }; }
  // khung chữ một dòng: điểm trái trên đường chân chữ (lb), hướng u, pháp tuyến v, bề rộng w, độ cao đường giữa so với chân chữ (mid)
  function tFrame(e){ var r = rad(e.rot || 0), u = [Math.cos(r), Math.sin(r)], v = [-u[1], u[0]], al = e.al || 'l', m = tMet(e), lb = add2(e.p, u, al === 'c' || al === 'm' ? -m.w / 2 : al === 'r' ? -m.w : 0); if (al === 'm') lb = add2(lb, v, -m.mid); return { lb:lb, u:u, v:v, w:m.w, h:e.h, mid:m.mid }; }
  function tAt(f, al){ var p = add2(f.lb, f.u, al === 'c' || al === 'm' ? f.w / 2 : al === 'r' ? f.w : 0); return al === 'm' ? add2(p, f.v, f.mid) : p; }
  // điểm neo theo mã canh (L, C, R, M, TL…BR) của chữ hoặc MText
  function anchor(e, k){
    if (e.t === 'mtext'){ var r = rad(e.rot || 0), u = [Math.cos(r), Math.sin(r)], v = [-u[1], u[0]], Hm = TC.mtextLines(e).length * e.h * (e.ls || 1.6667); k = { L:'BL', C:'BC', R:'BR', M:'MC' }[k] || k; var p = add2(e.p, u, /C$/.test(k) ? e.w / 2 : /R$/.test(k) ? e.w : 0); return add2(p, v, /^M/.test(k) ? -Hm / 2 : /^B/.test(k) ? -Hm : 0); }
    var f = tFrame(e); if (/^[LCRM]$/.test(k)) return tAt(f, k.toLowerCase()); var q = add2(f.lb, f.u, /C$/.test(k) ? f.w / 2 : /R$/.test(k) ? f.w : 0); return add2(q, f.v, /^T/.test(k) ? f.h : /^M/.test(k) ? f.h / 2 : 0);
  }
  var JUST = [['Trái', 'L'], ['Giữa', 'C'], ['Phải', 'R'], ['Giữa tâm', 'M'], ['Trên trái', 'TL'], ['Trên giữa', 'TC'], ['Trên phải', 'TR']], JALL = /^(L|C|R|M|TL|TC|TR|ML|MC|MR|BL|BC|BR)$/;
  IMPL.SCALETEXT = function(ctx){
    ctx.badge('ST');
    return selEdit(ctx, 'Chọn đối tượng chữ').then(function(ids){ var ts = ents(ids, TXT); if (!ids.length) return; if (!ts.length){ ctx.out('Không có chữ (Text, MText, attribute) trong vùng chọn.'); return; }
      return ask(ctx, 'Điểm gốc cho tỉ lệ', [['Hiện có', 'E']].concat(JUST.slice(0, 4)), 'Hiện có').then(function(k){ k = String(k).toUpperCase(); if (k !== 'E' && !JALL.test(k)){ ctx.out('Điểm gốc “' + esc(k) + '” không hợp lệ. Dùng E, L, C, R, M hoặc TL…BR.'); return; }
        var go = function(hOf, what){ return ED.run('SCALETEXT', function(){ ts.forEach(function(e){ var h = hOf(e); if (!(h > 0)) return; var f = h / e.h, a = k === 'E' ? null : anchor(e, k); ED.mod(e, function(x){ x.h = h; if (a){ x.p = add2(a, sub(x.p, a), f); if (x.t === 'mtext') x.w *= f; } }); }); TC.cad.clearSel(); ctx.out('Đã đổi chiều cao ' + TC.n(ts.length) + ' đối tượng chữ ' + what + '.'); }); };
        return ctx.val('Chiều cao mới hoặc', TC.d(ts[0].h), { opts:[['Khớp đối tượng', 'M'], ['Hệ số tỉ lệ', 'S']] }).then(function(v){ var o = String(v).trim().toUpperCase();
          if (o === 'M' || TC.fold(o) === 'khop doi tuong') return ED.pick(ctx, 'Chọn đối tượng chữ có chiều cao cần khớp', { types:['text', 'mtext', 'attdef'], locked:true }).then(function(r){ if (r.e) return go(function(){ return r.e.h; }, 'thành ' + TC.d(r.e.h)); });
          if (o === 'S' || TC.fold(o) === 'he so ti le') return ctx.val('Hệ số tỉ lệ', '2').then(function(s){ var f = Number(s); if (!(f > 0)){ ctx.out('Hệ số tỉ lệ phải lớn hơn 0.'); return; } return go(function(e){ return e.h * f; }, 'theo hệ số ' + TC.d(f, 4)); });
          var h = Number(v); if (!(h > 0)){ ctx.out('Chiều cao phải là số lớn hơn 0.'); return; } return go(function(){ return h; }, 'thành ' + TC.d(h)); }); }); });
  };
  IMPL.JUSTIFYTEXT = function(ctx){
    ctx.badge('JT');
    return selEdit(ctx, 'Chọn đối tượng chữ').then(function(ids){ var ts = ents(ids, TXT); if (!ids.length) return; if (!ts.length){ ctx.out('Không có chữ (Text, MText, attribute) trong vùng chọn.'); return; }
      return ask(ctx, 'Điểm canh mới', JUST, 'Trái').then(function(k){ k = String(k).toUpperCase(); var al = { L:'l', C:'c', R:'r', M:'m', MC:'m', TL:'l', ML:'l', BL:'l', TC:'c', BC:'c', TR:'r', MR:'r', BR:'r' }[k];
        if (!al){ ctx.out('Điểm canh “' + esc(k) + '” không hợp lệ. Dùng L, C, R, M hoặc TL…BR.'); return; }
        var name = (JUST.filter(function(j){ return j[1] === k; })[0] || [k])[0];
        // chữ đứng yên: tính lại điểm chèn theo điểm canh mới
        return ED.run('JUSTIFYTEXT', function(){ ts.forEach(function(e){ ED.mod(e, function(x){ if (x.t === 'mtext'){ x.al = al === 'm' ? 'c' : al; x.just = k.length === 2 ? k : { L:'TL', C:'TC', R:'TR', M:'MC' }[k]; return; } var f = tFrame(x); x.al = al; x.p = tAt(f, al); }); }); TC.cad.clearSel(); ctx.out('Đã đổi điểm canh ' + TC.n(ts.length) + ' đối tượng chữ sang ' + esc(name) + '; vị trí chữ giữ nguyên.'); }); }); });
  };
  var T2M = { order:'T' };
  IMPL.TXT2MTXT = function(ctx){
    ctx.badge('T2M'); var d = D();
    var go = function(ids){ var ts = ents(ED.editable(ids, ctx), /^(text|mtext)$/); if (!ts.length){ ctx.out('Không có chữ trong vùng chọn.'); return; } if (ts.length === 1 && ts[0].t === 'mtext'){ ctx.out('Đối tượng đã là MText. Chọn thêm chữ để gộp.'); return; }
      if (T2M.order === 'T') ts.sort(function(a, b){ var ya = a.t === 'mtext' ? a.p[1] : tFrame(a).lb[1], yb = b.t === 'mtext' ? b.p[1] : tFrame(b).lb[1]; return yb - ya || a.p[0] - b.p[0]; });
      // chiều cao chung là chiều cao gặp nhiều nhất; khung đủ rộng để không tự ngắt dòng (canvas ngắt theo 0,62 × cao mỗi ký tự)
      var f = ts[0], hs = {}; ts.forEach(function(e){ hs[e.h] = (hs[e.h] || 0) + 1; }); var h = Number(Object.keys(hs).sort(function(a, b){ return hs[b] - hs[a]; })[0]);
      var lbs = ts.map(function(e){ return e.t === 'mtext' ? [e.p[0], e.p[1] - e.h] : tFrame(e).lb; }), x0 = Math.min.apply(null, lbs.map(function(p){ return p[0]; })), top = lbs[0][1] + h;
      var w = (Math.max.apply(null, ts.map(function(e){ return Math.max.apply(null, String(e.s).split('\n').map(function(l){ return l.length; })); })) + 1) * h * .62, ls = ts.length > 1 ? TC.clamp(Math.abs(lbs[0][1] - lbs[lbs.length - 1][1]) / (ts.length - 1) / h, 1, 3) : 1.6667;
      return ED.run('TXT2MTXT', function(){ var m = Object.assign(ED.make({ t:'mtext', p:[x0, top], w:w, h:h, s:ts.map(function(e){ return e.s; }).join('\n'), ls:ls, style:f.style, al:'l', rot:0 }), { layer:f.layer }); if (f.color != null) m.color = f.color; m = ED.add(m, ED.listOf(d, f)); ts.forEach(function(e){ ED.del(e); }); d.sel = [m.id]; TC.cad.afterSel(); ctx.out('Đã gộp ' + ts.length + ' đối tượng chữ thành 1 MText (' + (T2M.order === 'T' ? 'từ trên xuống' : 'theo thứ tự chọn') + ').'); }); };
    if (d.sel.length) return go(d.sel.slice());
    ctx.out('Bấm hoặc kéo khung để chọn chữ, Enter để gộp.', 'faint');
    return (function step(){ return ask(ctx, 'Chọn đối tượng chữ hoặc', [['Thiết lập', 'SE']]).then(function(k){ if (k === 'SE') return ctx.kw('Thứ tự gộp', [['Từ trên xuống', 'T'], ['Theo thứ tự chọn', 'S']], T2M.order).then(function(o){ T2M.order = o === 'S' ? 'S' : 'T'; ctx.out('Thứ tự gộp: ' + (T2M.order === 'S' ? 'theo thứ tự chọn' : 'từ trên xuống') + '.'); return step(); }); if (k){ ctx.out('Gõ SE để đổi thiết lập, hoặc chọn chữ rồi Enter.'); return step(); } return go(d.sel.slice()); }); })();
  };

  /* ================= gắn các loại đối tượng mới vào lõi biên tập ================= */
  function angT(a, t){ return t.mir ? 2 * t.th - a : a + t.rot; }
  var xf0 = ED.xform;
  ED.xform = function(e, t){
    var f = t.f;
    switch (e.t){
      case 'gradient': case 'region': case 'wipeout': e.loops = e.loops.map(function(l){ return l.map(f); }); if (e.t === 'gradient') e.ang = angT(e.ang || 0, t); return e;
      case 'mline': e.pts = e.pts.map(f); e.sc = (e.sc || 1) * t.k; if (t.mir) e.just = { T:'B', B:'T' }[e.just] || e.just; return e;
      case 'cmark': e.c = f(e.c); e.r *= t.k; return e;
      case 'dimarc': e.c = f(e.c); e.r *= t.k; e.rd *= t.k; if (t.mir){ var a0 = angT(e.a1, t), a1 = angT(e.a0, t); e.a0 = a0; e.a1 = a1; } else { e.a0 += t.rot; e.a1 += t.rot; } return e;
      case 'dimj': e.c = f(e.c); e.oc = f(e.oc); if (e.jp) e.jp = f(e.jp); e.r *= t.k; e.ang = angT(e.ang, t); return e;
      case 'dimo': e.p = f(e.p); e.q = f(e.q); return e;
    }
    return xf0(e, t);
  };
  Object.assign(TC.TYPE_NAMES, { gradient:'Hatch gradient', region:'Region', wipeout:'Vùng che', mline:'Đa tuyến song song', cmark:'Dấu tâm', dimarc:'Kích thước cung', dimj:'Kích thước gãy khúc', dimo:'Kích thước tọa độ' });
  function swc(c){ return SC.colorSw(c) + esc(SC.colorLabel(c)); }
  function dimRows(e, f, v){ return [['Kiểu kích thước', esc(e.style || 'TCVN-1-100')], ['Giá trị đo', v, 'ro'], ['Ghi đè chữ', e.over ? esc(e.over) : '<span class="faint">(không)</span>', 'html'], ['Ngắt kích thước', e.brk ? (e.brk.auto ? 'Tự động' : 'Có') : 'Không', 'ro']]; }
  PX.gradient = function(d, e, f){ var g = e.g || {}, a = areaOf(e.loops); return [['Loại', 'Gradient', 'ro'], ['Mẫu gradient', esc(gName(g.name)) + ' (' + g.name + ')', 'ro'], ['Màu gradient 1', swc(g.c1), 'html'], g.one ? ['Sắc độ', g.lum + ' %', 'ro'] : ['Màu gradient 2', swc(g.c2), 'html'], ['Canh giữa', g.ctr ? 'Có' : 'Không', 'ro'], ['Góc', f(e.ang || 0, 0) + '°', 'num'], ['Độ trong suốt hatch', String(g.tr || 0), 'ro'], ['Liên kết', e.assoc ? 'Có' : 'Không', 'ro'], ['Diện tích', f(a, 0) + ' (' + f(a / 1e6) + ' m²)', 'ro']]; };
  PX.region = function(d, e, f){ var a = areaOf(e.loops); return [['Diện tích', f(a, 0) + ' (' + f(a / 1e6) + ' m²)', 'ro'], ['Chu vi', f(e.loops.reduce(function(s, l){ return s + G.polyLen(l, true); }, 0)), 'ro']]; };
  PX.wipeout = function(d, e, f){ var a = areaOf(e.loops); return [['Diện tích', f(a, 0) + ' (' + f(a / 1e6) + ' m²)', 'ro'], ['Hiện khung', ['Tắt', 'Bật', 'Hiện, không in'][d.wipeFrame == null ? 1 : d.wipeFrame], 'ro']]; };
  PX.mline = function(d, e, f){ return [['Kiểu', esc(e.style), 'ro'], ['Canh lề', JN[e.just] || 'Không', 'ro'], ['Tỉ lệ', f(e.sc || 1, 2), 'num'], ['Số đỉnh', String(e.pts.length), 'ro'], ['Đóng', e.closed ? 'Có' : 'Không'], ['Độ dài', f(G.polyLen(e.pts, e.closed)), 'ro']]; };
  PX.cmark = function(d, e, f){ return [['Tâm X', f(e.c[0]), 'num'], ['Tâm Y', f(e.c[1]), 'num'], ['Bán kính', f(e.r), 'num'], ['Kéo dài qua đường tròn', f(Math.min(CEXT, e.r * .5)), 'ro']]; };
  PX.dimarc = function(d, e, f){ return dimRows(e, f, '⌒' + f(e._meas || 0)); };
  PX.dimj = function(d, e, f){ return dimRows(e, f, 'R' + f(e.r)); };
  PX.dimo = function(d, e, f){ return dimRows(e, f, f(e._meas || 0) + ' (mốc ' + (e.axis === 'x' ? 'X' : 'Y') + ')'); };
  // nhấp đúp theo loại đối tượng (07.1 §9.10)
  var dbl0 = TC.cad.hook.dbl;
  TC.cad.hook.dbl = function(e, m){
    if (e && !TC.isViewer() && !(D().layerMap[e.layer] || {}).lck){
      if (e.t === 'gradient'){ D().sel = [e.id]; TC.cad.afterSel(); return true; }
      if (e.t === 'spline'){ TC.cmd.run('SPLINEDIT', e); return true; }
      if (e.t === 'mline'){ TC.cmd.run('MLEDIT'); return true; }
      if (e.t === 'dimarc' || e.t === 'dimj' || e.t === 'dimo'){ TC.cmd.run('TEXTEDIT', e); return true; }
    }
    return dbl0 ? dbl0(e, m) : false;
  };
  // menu chuột phải khi có vùng chọn
  TC.cad.ctxExt.push(function(it, first, d){
    if (!first || TC.isViewer()) return; var sel = ents(d.sel), inG = d.sel.some(function(id){ return grpOf(d, id); });
    if (inG) it.push({ label:'Nhóm', sub:[{ label:'Bỏ nhóm', act:function(){ TC.cmd.run('UNGROUP'); } }, { label:'Sửa nhóm', act:function(){ TC.cmd.run('GROUPEDIT'); } }] });
    else if (sel.length > 1) it.push({ label:'Tạo nhóm', icon:'group', act:function(){ TC.cmd.run('GROUP'); } });
    if (sel.length > 1 && sel.every(function(e){ return e.t === 'text' || e.t === 'mtext'; })) it.push({ label:'Gộp thành MText', act:function(){ TC.cmd.run('TXT2MTXT'); } });
    if (first.t === 'spline') it.push({ label:'Sửa spline', act:function(){ TC.cmd.run('SPLINEDIT', first); } });
    if (first.t === 'gradient') it.push({ label:'Sửa gradient', act:function(){ ED.ctxRefresh(); } });
  });
})();
