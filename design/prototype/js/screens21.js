/* TrionCAD prototype · Đợt 5 (sau v1.0): tab Mở rộng phần P2 (Express Tools còn lại, 07.1 §7.10). */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic, G = TC.geo, IMPL = TC.cmd.impl, ED = TC.ed, SC = TC.screens, H = SC.h;
  function A(){ return TC.app; } function V(){ return TC.app.view; } function D(){ return TC.app.doc(); }
  var OPEN = SC.open5 = SC.open5 || {}; // mã màn hình → hàm mở (danh mục gọi khi bấm); tiện ích mở ở SC.cat (D1, PRO, MOD, RUN, plan, pick, flash)
  var T = ED.T, polar = ED.polar, sweep = ED.sweep, X = TC.flatExt, PX = TC.propsExt, NM = TC.TYPE_NAMES;

  /* ================= tiện ích ================= */
  function sub(a, b){ return [a[0] - b[0], a[1] - b[1]]; }
  function add2(a, v, k){ k = k == null ? 1 : k; return [a[0] + v[0] * k, a[1] + v[1] * k]; }
  function dot(a, b){ return a[0] * b[0] + a[1] * b[1]; }
  function unit(v){ var L = Math.hypot(v[0], v[1]) || 1; return [v[0] / L, v[1] / L]; }
  function rad(a){ return a * Math.PI / 180; } function deg(r){ return r * 180 / Math.PI; }
  function rot2(v, a){ var c = Math.cos(rad(a)), s = Math.sin(rad(a)); return [v[0] * c - v[1] * s, v[0] * s + v[1] * c]; }
  function angT(a, t){ return t.mir ? 2 * t.th - a : a + t.rot; }
  function an360(a){ return (a % 360 + 360) % 360; }
  function num(v, def){ v = String(v == null ? '' : v).trim(); var n = Number(v); return v !== '' && isFinite(n) ? n : def; }
  function prev(e, o){ return ED.temp([e], Object.assign({ real:true }, o || {})); }
  function ents(ids, re){ var d = D(); return ids.map(function(id){ return d.byId[id]; }).filter(function(e){ return e && (!re || re.test(e.t)); }); }
  function selEdit(ctx, text){ return ctx.sel(text).then(function(ids){ return ED.editable(ids, ctx); }); }
  function ctip(r, msg){ var s = r && r.s || V().mouse || [100, 100]; TC.ctip($('#cv'), s[0], s[1], msg); }
  // prompt có chip: Enter trả mặc định; gõ chữ tắt kiểu AutoCAD hoặc nhãn tiếng Việt đều được
  function ask(ctx, text, opts, def, o){ return ctx.val(text, def == null ? null : def, Object.assign({ opts:opts }, o || {})).then(function(v){ v = String(v == null ? '' : v).trim(); var k = opts.filter(function(x){ return x[1].toUpperCase() === v.toUpperCase() || (v && TC.fold(x[0]) === TC.fold(v)); })[0]; return k ? k[1] : v; }); }
  function bbOf(pts){ var b = [Infinity, Infinity, -Infinity, -Infinity]; pts.forEach(function(p){ if (p[0] < b[0]) b[0] = p[0]; if (p[1] < b[1]) b[1] = p[1]; if (p[0] > b[2]) b[2] = p[0]; if (p[1] > b[3]) b[3] = p[1]; }); return b; }
  function centroid(l){ var x = 0, y = 0; l.forEach(function(q){ x += q[0]; y += q[1]; }); return [x / l.length, y / l.length]; }
  function allEnts(d){ return d.layouts.reduce(function(a, L){ return a.concat(L.ents); }, d.ents.slice()); }
  function base(p){ return String(p || '').replace(/^.*[\\/]/, ''); }
  function dirOf(p){ return String(p || '').replace(/[\\/][^\\/]*$/, ''); }
  function swc(c){ return SC.colorSw(c) + esc(SC.colorLabel(c)); }
  function lbl(t){ return '<span class="mono">' + esc(t) + '</span>'; }
  function qty(n, w){ return TC.n(n) + ' ' + w; }
  // đo chữ bằng đúng font của vùng vẽ (cỡ font = 1,32 × chiều cao chữ, như canvas.js)
  function fontOf(d, style, px, o){ var st = d.styleMap[style] || {}; return (o && o.ital ? 'italic ' : '') + (st.bold || (o && o.bold) ? '600 ' : '') + px + 'px ' + (st.css || 'Arial, "Helvetica Neue", sans-serif'); }
  var MC = document.createElement('canvas').getContext('2d');
  function tw(d, s, h, style, o){ MC.font = fontOf(d, style, 100, o); return MC.measureText(String(s)).width * h * 1.32 / 100; }
  function midOf(d, e){ MC.font = fontOf(d, e.style, 100, e); MC.textBaseline = 'alphabetic'; var a = MC.measureText(String(e.s)); MC.textBaseline = 'middle'; var b = MC.measureText(String(e.s)); MC.textBaseline = 'alphabetic'; return (a.actualBoundingBoxAscent - b.actualBoundingBoxAscent) * e.h * 1.32 / 100; }
  // khung chữ một dòng: lb trái-chân chữ, hướng u, pháp tuyến v, rộng w (đã nhân hệ số rộng), tâm c
  function frameOf(d, e){ var u = rot2([1, 0], e.rot || 0), v = [-u[1], u[0]], al = e.al || 'l', w = tw(d, e.s, e.h, e.style, e) * (e.wf || 1), lb = add2(e.p, u, al === 'c' || al === 'm' ? -w / 2 : al === 'r' ? -w : 0); if (al === 'm') lb = add2(lb, v, -midOf(d, e)); return { lb:lb, u:u, v:v, w:w, h:e.h, c:add2(add2(lb, u, w / 2), v, e.h / 2) }; }
  // tâm nhìn thấy của chữ, MText, RText (để xoay quanh tâm)
  function tCenter(d, e){ var u = rot2([1, 0], e.rot || 0), v = [-u[1], u[0]];
    if (e.t === 'mtext'){ var n = TC.mtextLines(e).length; return add2(add2(e.p, u, e.w / 2), v, -(e.h * (1 + (n - 1) * (e.ls || 1.6)) + e.h * .24) / 2); }
    if (e.t === 'rtext'){ var ls = rtLines(d, e), W = Math.max.apply(null, ls.map(function(s){ return tw(d, s, e.h, e.style); }).concat([e.h])); return add2(add2(e.p, u, W / 2), v, (e.h - (ls.length - 1) * e.h * 1.6667 - e.h * .24) / 2); }
    return frameOf(d, e).c; }
  function vpsOf(d){ var out = []; d.layouts.forEach(function(L, li){ L.vports.forEach(function(vp, i){ out.push({ L:L, li:li, i:i, vp:vp }); }); }); return out; }
  // ảnh chụp mọi viewport cho hoàn tác (tâm, tỉ lệ, khóa)
  function vpSnap(d){ return ED.state(function(){ var o = {}; vpsOf(d).forEach(function(x){ o[x.vp.id] = { c:x.vp.c.slice(), scale:x.vp.scale, locked:!!x.vp.locked }; }); return o; }, function(s){ vpsOf(d).forEach(function(x){ var q = s[x.vp.id]; if (q){ x.vp.c = q.c; x.vp.scale = q.scale; x.vp.locked = q.locked; } }); V().draw(); A().renderStatus(); A().refreshPalettes(); }); }
  function kFmt(k){ return Math.abs(k - Math.round(k)) < 1e-6 ? String(Math.round(k)) : TC.d(k, 2); }

  /* ================= tệp ngoài mô phỏng (RTEXT, REDIR, DIMEX/DIMIM, BLOCKTOXREF) ================= */
  var FS = {
    'D:\\DuAn\\NhaO-2T\\Ghi-chu\\ghi-chu-chung.txt':'GHI CHÚ CHUNG:\n1. Kích thước ghi bằng mm, cao độ ghi bằng m.\n2. Tường bao xây gạch đặc dày 220, vữa xi măng M75.\n3. Bê tông cột, dầm, sàn cấp độ bền B20.',
    'D:\\DuAn\\NhaO-2T\\Ghi-chu\\thong-tin-du-an.txt':'CÔNG TRÌNH: NHÀ Ở GIA ĐÌNH 2 TẦNG\nĐỊA ĐIỂM: LÔ B7, KDC AN PHÚ, TP. THỦ ĐỨC\nCHỦ ĐẦU TƯ: ÔNG NGUYỄN VĂN AN',
    '\\\\SRV-01\\Chung\\tieu-chuan-ap-dung.txt':'TIÊU CHUẨN ÁP DỤNG:\n- TCVN 5571:2012 Khung tên bản vẽ xây dựng\n- TCVN 5705:1993 Quy tắc ghi kích thước\n- TCVN 4455:1987 Quy tắc ghi kích thước, chữ',
    '\\\\SRV-01\\Tai-nguyen\\logo-cong-ty.png':'.', '\\\\SRV-01\\Tai-nguyen\\Shape\\ky-hieu-dien.shx':'.', 'D:\\DuAn\\NhaO-2T\\KetCau\\KC-luoi-truc.dwg':'.', 'D:\\DuAn\\NhaO-2T\\Hien-trang\\Hien-trang.dwg':'.',
    'D:\\DuAn\\NhaO-2T\\Thu-vien\\BAN-AN-6.dwg':'.', 'D:\\DuAn\\NhaO-2T\\Thu-vien\\SOFA-L.dwg':'.',
    'D:\\Tai-nguyen\\Kieu-kich-thuoc\\Cong-ty-AB.dim':'; Kiểu kích thước của Công ty Kiến trúc AB (xuất bằng DIMEX)\n*AB-1-100\nDIMASZ 1.2\nDIMBLK _DOT\nDIMCEN 2\nDIMDEC 0\nDIMDLI 4\nDIMEXE 1\nDIMEXO 1\nDIMGAP 0.8\nDIMSCALE 100\nDIMTXSTY Standard\nDIMTXT 2\n*AB-1-50\nDIMASZ 1.2\nDIMBLK _DOT\nDIMCEN 2\nDIMDEC 0\nDIMDLI 4\nDIMEXE 1\nDIMEXO 1\nDIMGAP 0.8\nDIMSCALE 50\nDIMTXSTY Standard\nDIMTXT 2\n*TCVN-1-100\nDIMASZ 2\nDIMBLK _OBLIQUE\nDIMCEN 2.5\nDIMDEC 0\nDIMDLI 4\nDIMEXE 2\nDIMEXO 1\nDIMGAP 1\nDIMSCALE 100\nDIMTXSTY Standard\nDIMTXT 2.5',
    'D:\\Tai-nguyen\\Kieu-kich-thuoc\\ISO-25.dim':'; ISO-25 gốc\n*ISO-25\nDIMASZ 2.5\nDIMBLK _CLOSEDFILLED\nDIMCEN 2.5\nDIMDEC 2\nDIMDLI 3.75\nDIMEXE 1.25\nDIMEXO 0.625\nDIMGAP 0.625\nDIMSCALE 1\nDIMTXSTY Standard\nDIMTXT 2.5'
  };
  function fsKey(p){ var k = String(p || '').toLowerCase(); return Object.keys(FS).filter(function(x){ return x.toLowerCase() === k; })[0] || null; }
  // đường dẫn tương đối tính từ thư mục bản vẽ
  function resolve(d, p){ p = String(p || ''); if (!/^\.\.?\\/.test(p)) return p; var parts = dirOf(d.path).split('\\'); p.split('\\').forEach(function(s){ if (s === '..') parts.pop(); else if (s !== '.') parts.push(s); }); return parts.join('\\'); }
  function exists(d, p){ return !!fsKey(resolve(d, p)); }
  function fsRead(d, p){ var k = fsKey(resolve(d, p)); return k ? FS[k] : null; }

  /* ================= shape: định nghĩa trong tệp .shx đã nạp (MKSHAPE, SHAPE, SHP2BLK, linetype) ================= */
  function ring(cx, cy, r, n, a0, a1){ var out = [], s = a0 == null ? 0 : a0, e = a1 == null ? 360 : a1; for (var i = 0; i < n; i++){ var A1 = rad(s + (e - s) * i / n), A2 = rad(s + (e - s) * (i + 1) / n); out.push([[cx + r * Math.cos(A1), cy + r * Math.sin(A1)], [cx + r * Math.cos(A2), cy + r * Math.sin(A2)]]); } return out; }
  var SHX0 = { 'O-CAM':{ segs:ring(0, 0, .5, 16, 0, 180).concat([[[-.5, 0], [.5, 0]], [[-.18, 0], [-.18, -.32]], [[.18, 0], [.18, -.32]]]), res:128, desc:'Ổ cắm điện' },
    'CONG-TAC':{ segs:ring(0, 0, .3, 16).concat([[[.21, .21], [.62, .62]], [[.62, .62], [.78, .46]]]), res:128, desc:'Công tắc một cực' },
    'DEN':{ segs:ring(0, 0, .5, 16).concat([[[-.35, -.35], [.35, .35]], [[-.35, .35], [.35, -.35]]]), res:128, desc:'Đèn trần' } };
  function shapeFiles(d){ return (d.shapeFiles = d.shapeFiles || []); }
  function shapeDef(d, name){ var r = null; shapeFiles(d).forEach(function(f){ if (!r && f.defs[name]) r = Object.assign({ file:f.name }, f.defs[name]); }); return r; }
  function shapeNames(d){ var out = []; shapeFiles(d).forEach(function(f){ Object.keys(f.defs).forEach(function(n){ out.push([n, f.name]); }); }); return out; }

  /* ================= vẽ: che nền chữ, hệ số rộng, linetype tự tạo, SuperHatch (bọc View.drawPrims) ================= */
  function bgCol(view, onPaper){ return TC.cssv(view.print || onPaper ? '--c-paper' : '--c-canvas'); }
  function colHex(c, dark){ return typeof c === 'string' ? c : c === 7 || c == null ? (dark ? '#FFFFFF' : '#000000') : TC.ACI[c] || '#FFFFFF'; }
  // vùng che vẽ trước chữ, trong không gian màn hình, theo đúng bề rộng font đang vẽ
  function drawMask(view, ctx, e, v, onPaper){
    var m = e.mask || { type:'B', off:.35 }, s = v.s, g = m.off == null ? .35 : m.off, wf = e.wf || 1;
    ctx.save(); ctx.fillStyle = m.type === 'C' ? colHex(m.color, !(view.print || onPaper)) : bgCol(view, onPaper);
    e._p.forEach(function(pr){ if (pr.k !== 'text' || !String(pr.s).trim()) return; var px = pr.h * s * 1.32; if (px < 2.2) return; var tp = view.toS(pr.p, v), h = pr.h * s, al = pr.al || 'l';
      ctx.save(); ctx.translate(tp[0], tp[1]); if (pr.rot) ctx.rotate(-rad(pr.rot)); ctx.font = view.textFont(pr, px);
      var w = ctx.measureText(view.textStr(pr)).width * wf, x0 = al === 'm' || al === 'c' ? -w / 2 : al === 'r' || al === 'rm' ? -w : 0, md = al === 'm' || al === 'lm' || al === 'rm', y0 = md ? -h * .62 : -h, y1 = md ? h * .62 : h * .24;
      ctx.fillRect(x0 - g * h, y0 - g * h, w + 2 * g * h, y1 - y0 + 2 * g * h); ctx.restore(); });
    ctx.restore();
  }
  // hệ số rộng (TEXTFIT, ARCTEXT): co chữ theo phương ngang khi vẽ
  function wfCtx(ctx, k){ var fn = {}; return new Proxy(ctx, { get:function(t, p){ if (p === 'fillText') return function(s, x, y){ t.save(); t.scale(k, 1); t.fillText(s, x / k, y); t.restore(); }; var v = t[p]; return typeof v === 'function' ? fn[p] || (fn[p] = v.bind(t)) : v; }, set:function(t, p, v){ t[p] = v; return true; } }); }
  // linetype tạo bằng MKLTYPE: nét, khoảng hở, chữ và shape nhúng; mẫu chạy liên tục qua các đỉnh
  function ltDef(d, e){ var n = e.lt && !/^by/i.test(e.lt) ? e.lt : (d.layerMap[e.layer] || {}).lt; return n && d.ltdefs && d.ltdefs[n] || null; }
  function primLen(pr){ return pr.k === 'seg' ? G.dist(pr.a, pr.b) : pr.r * rad(pr.k === 'circ' ? 360 : sweep(pr.a0, pr.a1) || 360); }
  function arcSw(pr){ return pr.k === 'circ' ? 360 : sweep(pr.a0, pr.a1) || 360; }
  function primAt(pr, s, dir){ if (pr.k === 'seg'){ var u = unit(sub(pr.b, pr.a)); return { p:add2(pr.a, u, s), a:deg(Math.atan2(u[1], u[0])) }; } var a0 = pr.k === 'circ' ? 0 : pr.a0, an = dir < 0 ? a0 + arcSw(pr) - deg(s / pr.r) : a0 + deg(s / pr.r); return { p:polar(pr.c, pr.r, an), a:an + (dir < 0 ? -90 : 90) }; }
  function primPiece(pr, s0, s1, dir){ if (pr.k === 'seg'){ var u = unit(sub(pr.b, pr.a)); return Object.assign({}, pr, { a:add2(pr.a, u, s0), b:add2(pr.a, u, s1) }); } var a0 = pr.k === 'circ' ? 0 : pr.a0, sw = arcSw(pr); return Object.assign({}, pr, { k:'arc', a0:dir < 0 ? a0 + sw - deg(s1 / pr.r) : a0 + deg(s0 / pr.r), a1:dir < 0 ? a0 + sw - deg(s0 / pr.r) : a0 + deg(s1 / pr.r) }); }
  function ltMarks(d, m, q, k, pr0, out){ var a = q.a + (m.rot || 0), n = rot2([0, 1], q.a), p = add2(q.p, n, (m.dy || 0) * k);
    if (m.k === 't'){ var r = an360(a); if (r > 90 && r <= 270) r -= 180; out.push({ k:'text', p:p, h:m.h * k, s:m.s, rot:r, al:'m', style:m.style || 'Standard', e:pr0.e, src:pr0.src }); return; }
    var sd = shapeDef(d, m.name); if (!sd) return; sd.segs.forEach(function(sg){ out.push({ k:'seg', a:add2(p, rot2([sg[0][0] * m.sz * k, sg[0][1] * m.sz * k], a)), b:add2(p, rot2([sg[1][0] * m.sz * k, sg[1][1] * m.sz * k], a)), e:pr0.e, src:pr0.src }); }); }
  function ltWalk(d, chain, def, k, out){
    var P = def.L * k, tot = chain.reduce(function(s, c){ return s + c.len; }, 0), S = 0;
    if (!(P > 0) || tot / P > 4000){ chain.forEach(function(c){ out.push(c.pr); }); return; } // ponytail: mẫu quá dày so với đối tượng thì vẽ liền, như AutoCAD khi vượt giới hạn nét
    chain.forEach(function(c){ var L = c.len, n0 = Math.floor(S / P) - 1, n1 = Math.ceil((S + L) / P);
      for (var n = n0; n <= n1; n++){ var b0 = n * P;
        def.dash.forEach(function(dv){ var a = Math.max(b0 + dv[0] * k, S), b = Math.min(b0 + dv[1] * k, S + L); if (dv[1] - dv[0] < 1e-9){ if (a >= S && a < S + L) out.push({ k:'pt', p:primAt(c.pr, a - S, c.dir).p, e:c.pr.e, src:c.pr.src }); } else if (b - a > 1e-9) out.push(primPiece(c.pr, a - S, b - S, c.dir)); });
        (def.marks || []).forEach(function(m){ var at = b0 + m.at * k; if (at >= S && at < S + L) ltMarks(d, m, primAt(c.pr, at - S, c.dir), k, c.pr, out); }); }
      S += L; });
  }
  function ltExpand(d, e, def){
    var all = e._p, k = e.lts || 1, c0 = e._ltc; if (c0 && c0.p === all && c0.def === def && c0.k === k) return c0.out;
    var out = [], chain = [], last = null, flush = function(){ if (chain.length) ltWalk(d, chain, def, k, out); chain = []; last = null; };
    all.forEach(function(pr){
      if (pr.src !== e || !/^(seg|arc|circ)$/.test(pr.k)){ flush(); out.push(pr); return; }
      var dir = 1, a = pr.k === 'seg' ? pr.a : pr.k === 'arc' ? polar(pr.c, pr.r, pr.a0) : null, b = pr.k === 'seg' ? pr.b : pr.k === 'arc' ? polar(pr.c, pr.r, pr.a1) : null;
      if (pr.k === 'arc' && last && G.dist(b, last) < G.dist(a, last)){ dir = -1; var t = a; a = b; b = t; }
      if (!a || (last && G.dist(a, last) > 1e-3)) flush();
      chain.push({ pr:pr, dir:dir, len:primLen(pr) }); last = b; if (!b) flush(); });
    flush(); e._ltc = { p:all, def:def, k:k, out:out }; return out;
  }
  // vật liệu ảnh của SuperHatch: vẽ thủ tục từng viên trong không gian màn hình
  var MAT = { 'gach-the-300.png':{ n:'Gạch thẻ 300 × 300', w:300, h:300, base:'#A65A37', line:'#E6D3BC', kind:'tile' }, 'da-granite-600.jpg':{ n:'Đá granite 600 × 600', w:600, h:600, base:'#7C8086', line:'#A9ADB3', dot:'#C9CCD1', dot2:'#4E5258', kind:'speck' },
    'san-go-150x900.jpg':{ n:'Sàn gỗ 900 × 150', w:900, h:150, base:'#93623A', line:'#5F3E22', kind:'grain' }, 'co-nhan-tao.jpg':{ n:'Cỏ nhân tạo 1000 × 1000', w:1000, h:1000, base:'#3E7A34', line:'#6EAA55', kind:'grass' } };
  function rnd(n){ var x = Math.sin(n * 12.9898 + 4.1) * 43758.5453; return x - Math.floor(x); }
  function paintTile(ctx, M, q, i, big){
    var P = function(u, v){ return [q[0][0] + (q[1][0] - q[0][0]) * u + (q[3][0] - q[0][0]) * v, q[0][1] + (q[1][1] - q[0][1]) * u + (q[3][1] - q[0][1]) * v]; }, k, a, b;
    ctx.beginPath(); ctx.moveTo(q[0][0], q[0][1]); for (k = 1; k < 4; k++) ctx.lineTo(q[k][0], q[k][1]); ctx.closePath(); ctx.fillStyle = M.base; ctx.fill();
    if (big){ ctx.lineWidth = 1; ctx.strokeStyle = M.line; ctx.fillStyle = M.dot || M.line;
      if (M.kind === 'speck') for (k = 0; k < 14; k++){ a = P(rnd(i * 31 + k), rnd(i * 17 + k * 3)); ctx.fillStyle = k % 3 ? M.dot : M.dot2; ctx.fillRect(a[0], a[1], 1.6, 1.6); }
      if (M.kind === 'grain') for (k = 1; k < 4; k++){ ctx.beginPath(); for (var j = 0; j <= 8; j++){ a = P(j / 8, k / 4 + Math.sin(j * 1.3 + i + k) * .06); if (j) ctx.lineTo(a[0], a[1]); else ctx.moveTo(a[0], a[1]); } ctx.stroke(); }
      if (M.kind === 'grass') for (k = 0; k < 18; k++){ a = P(rnd(i * 13 + k), rnd(i * 7 + k * 5)); b = [a[0] + 1.5, a[1] - 4]; ctx.beginPath(); ctx.moveTo(a[0], a[1]); ctx.lineTo(b[0], b[1]); ctx.stroke(); } }
    ctx.beginPath(); ctx.moveTo(q[0][0], q[0][1]); for (k = 1; k < 4; k++) ctx.lineTo(q[k][0], q[k][1]); ctx.closePath(); ctx.strokeStyle = M.kind === 'tile' || M.kind === 'speck' ? M.line : 'rgba(0,0,0,.35)'; ctx.lineWidth = 1; ctx.stroke();
  }
  function loopPath(view, ctx, loops, v){ ctx.beginPath(); loops.forEach(function(l){ l.forEach(function(p, i){ var s = view.toS(p, v); if (i) ctx.lineTo(s[0], s[1]); else ctx.moveTo(s[0], s[1]); }); ctx.closePath(); }); }
  function drawTiles(view, ctx, e, v, onPaper, hp){
    var wp = e._p.filter(function(pr){ return pr.k === 'wipe'; })[0]; if (wp){ ctx.fillStyle = bgCol(view, onPaper); loopPath(view, ctx, wp.loops, v); ctx.fill('evenodd'); return; }
    var tiles = e._p.filter(function(pr){ return pr.k === 'tile'; }); if (!tiles.length) return;
    var M = MAT[e.src.name] || MAT['gach-the-300.png'], S = function(p){ return view.toS(p, v); }, q0 = tiles[0].pts.map(S), tsz = Math.min(Math.hypot(q0[1][0] - q0[0][0], q0[1][1] - q0[0][1]), Math.hypot(q0[3][0] - q0[0][0], q0[3][1] - q0[0][1]));
    if (tsz < 4){ ctx.save(); ctx.globalAlpha *= 1 - (e.fade || 0) / 100; ctx.fillStyle = M.base; loopPath(view, ctx, hp.loops, v); ctx.fill('evenodd'); ctx.restore(); return; }
    ctx.save(); ctx.globalAlpha *= 1 - (e.fade || 0) / 100; tiles.forEach(function(t){ paintTile(ctx, M, t.pts.map(S), t.i, tsz > 14); }); ctx.restore();
  }
  var dp1 = TC.View.prototype.drawPrims;
  TC.View.prototype.drawPrims = function(ctx, e, v, onPaper, style){
    var all = e._p, d = this.doc; if (!all || !d) return dp1.apply(this, arguments); if (e._edit && !style) return; // đang sửa trong hộp thoại: chỉ hiện bản xem trước
    var sh = e.t === 'shatch', mk = !style && (e.mask || (e.t === 'mtext' && e.bg)), wf = e.wf && e.wf !== 1, lt = !style && !sh && d.ltdefs && ltDef(d, e);
    if (!sh && !mk && !wf && !lt) return dp1.apply(this, arguments);
    if (mk) drawMask(this, ctx, e, v, onPaper);
    var hp = sh && all.filter(function(pr){ return pr.hit; })[0];
    if (hp){ ctx.save(); loopPath(this, ctx, hp.loops, v); ctx.clip('evenodd'); if (!style) drawTiles(this, ctx, e, v, onPaper, hp); }
    e._p = sh ? all.filter(function(pr){ return pr.k !== 'tile' && pr.k !== 'wipe' && (style || !pr.hit); }) : lt ? ltExpand(d, e, lt) : all;
    try { dp1.call(this, wf ? wfCtx(ctx, e.wf) : ctx, e, v, onPaper, style); } finally { e._p = all; if (hp) ctx.restore(); }
  };

  /* ================= liên kết: chữ theo cung đi theo cung, đường dẫn đi theo chú thích (bọc ED.mod) ================= */
  function sameArc(x, o){ return G.dist(x.c, o.c) < 1e-6 && Math.abs(x.r - o.r) < 1e-6 && Math.abs(an360(x.a0) - an360(o.a0)) < 1e-6 && Math.abs(an360(x.a1) - an360(o.a1)) < 1e-6; }
  var mod0 = ED.mod;
  ED.mod = function(e, fn){
    var d = D(), old = e.t === 'arc' && d && d._atLink ? { c:e.c.slice(), r:e.r, a0:e.a0, a1:e.a1 } : null, r = mod0.apply(this, arguments);
    if (old) allEnts(d).forEach(function(x){ if (x.t === 'arctext' && x.arc === e.id && sameArc(x, old)) mod0(x, function(y){ y.c = e.c.slice(); y.r = e.r; y.a0 = e.a0; y.a1 = e.a1; }); });
    if (d && d._qlAtt && e.t !== 'qleader') allEnts(d).forEach(function(x){ if (x.t === 'qleader' && x.att === e.id) mod0(x, function(y){ var r = attachPts(d, y, e); y.pts = r.pts; y.atp = r.at; }); });
    return r;
  };

  /* ================= loại đối tượng mới ================= */
  var AL = { F:'Vừa cung', L:'Trái', C:'Giữa', R:'Phải' };
  // vị trí từng ký tự: 'out' đọc từ đầu a1 theo chiều kim đồng hồ (đỉnh chữ ra ngoài), 'in' từ a0 ngược chiều (đỉnh chữ vào tâm)
  function atLayout(d, e){
    var chars = Array.from(String(e.s || '')); if (e.rev) chars.reverse();
    var h = e.h, n = chars.length, ws = chars.map(function(ch){ return tw(d, ch, h, e.style, e) * (e.wf || 1); }), sumW = ws.reduce(function(s, w){ return s + w; }, 0);
    var R = Math.max(h * .5, e.side === 'in' ? e.r - (e.off || 0) - h / 2 : e.r + (e.off || 0) + h / 2), sw = rad(sweep(e.a0, e.a1) || 360), L = R * sw, offL = e.offL || 0, offR = e.offR || 0, avail = Math.max(0, L - offL - offR);
    var fit = e.al === 'F' && n > 1, gap = fit ? (avail - sumW) / (n - 1) : e.sp || 0, W = sumW + gap * Math.max(0, n - 1), s = e.al === 'R' ? L - offR - W : e.al === 'C' || (e.al === 'F' && !fit) ? offL + (avail - W) / 2 : offL, inw = e.dir === 'in', out = [];
    chars.forEach(function(ch, i){ var m = s + ws[i] / 2, an = inw ? e.a0 + deg(m / R) : e.a0 + deg(sw - m / R); out.push({ p:polar(e.c, R, an), rot:inw ? an + 90 : an - 90, s:ch }); s += ws[i] + gap; });
    out.need = fit ? sumW : W; out.avail = avail; out.gap = gap; return out;
  }
  X.arctext = function(x){ var e = x.e, k = x.t ? x.t.k : 1, r0 = x.t ? x.t.rot : 0; atLayout(x.doc, e).forEach(function(ch){ x.o({ k:'text', p:x.P(ch.p), h:e.h * k, s:ch.s, rot:ch.rot + r0, al:'m', style:e.style, bold:e.bold, ital:e.ital }); }); };
  // RText: nội dung đọc lại từ tệp (hoặc biểu thức DIESEL) mỗi lần tạo lại hình
  function rtLines(d, e){ if (e.src === 'diesel') return [diesel(d, e.expr)]; var t = fsRead(d, e.path); return t == null ? ['[RText: không tìm thấy ' + base(e.path) + ']'] : String(t).split('\n'); }
  X.rtext = function(x){ var e = x.e, k = x.t ? x.t.k : 1, r0 = x.t ? x.t.rot : 0, u = rot2([0, -1], e.rot || 0); rtLines(x.doc, e).forEach(function(s, i){ x.o({ k:'text', p:x.P(add2(e.p, u, i * e.h * 1.6667)), h:e.h * k, s:s, rot:(e.rot || 0) + r0, al:'l', style:e.style }); }); };
  X.shape = function(x){ var e = x.e, sd = shapeDef(x.doc, e.name), f = function(q){ return x.P(add2(e.p, rot2([q[0] * e.h * (e.mir ? -1 : 1), q[1] * e.h], e.rot || 0))); };
    if (!sd){ x.o({ k:'text', p:x.P(e.p), h:e.h * .4, s:'?' + e.name, rot:e.rot || 0, al:'l', style:'Standard' }); return; }
    sd.segs.forEach(function(s){ x.o({ k:'seg', a:f(s[0]), b:f(s[1]) }); }); x.o({ k:'ins', p:x.P(e.p) }); };
  // SuperHatch: lưới ô (ảnh, block, xref) theo gốc, góc, khe, so le; cắt theo vùng khi vẽ
  function shBlockBB(d, name){ return d.blocks[name] ? TC.prepEnt(d, { t:'insert', name:name, p:[0, 0], sx:1, sy:1, rot:0, layer:'0' }, true)._bb : null; }
  function quadHits(q, poly){ var c = [(q[0][0] + q[2][0]) / 2, (q[0][1] + q[2][1]) / 2]; if (G.inPoly(c, poly) || q.some(function(p){ return G.inPoly(p, poly); }) || poly.some(function(p){ return G.inPoly(p, q); })) return true; for (var k = 0; k < 4; k++) for (var m = 0; m < poly.length; m++) if (G.segInt(q[k], q[(k + 1) % 4], poly[m], poly[(m + 1) % poly.length])) return true; return false; }
  function shSize(d, e){ if (e.src.kind === 'image') return [e.tw, e.th]; var b = shBlockBB(d, e.src.name), k = e.src.sc || 1; return b && isFinite(b[0]) ? [(b[2] - b[0]) * k, (b[3] - b[1]) * k, b] : null; }
  function shTiles(d, e){
    var outer = e.loops[0], bb = bbOf(outer), a = e.ang || 0, u = rot2([1, 0], a), v = rot2([0, 1], a), o = e.org || [bb[0], bb[1]], gap = e.gap || 0, sz = shSize(d, e); if (!sz) return [];
    var pw = sz[0] + gap, ph = sz[1] + gap, k = e.src.sc || 1; if (!(pw > 0 && ph > 0)) return [];
    var us = outer.map(function(p){ return dot(sub(p, o), u); }), vs = outer.map(function(p){ return dot(sub(p, o), v); }), i0 = Math.floor(Math.min.apply(null, us) / pw) - 1, i1 = Math.ceil(Math.max.apply(null, us) / pw), j0 = Math.floor(Math.min.apply(null, vs) / ph) - 1, j1 = Math.ceil(Math.max.apply(null, vs) / ph);
    if ((i1 - i0 + 1) * (j1 - j0 + 1) > 4000) return { over:true, n:(i1 - i0 + 1) * (j1 - j0 + 1) };
    var out = [];
    for (var j = j0; j <= j1; j++) for (var i = i0; i <= i1; i++){ var c0 = add2(add2(o, u, i * pw + (e.stag && j % 2 ? pw / 2 : 0)), v, j * ph), q = [c0, add2(c0, u, sz[0]), add2(add2(c0, u, sz[0]), v, sz[1]), add2(c0, v, sz[1])];
      if (quadHits(q, outer)) out.push({ pts:q, i:out.length, sc:k, ip:sz[2] ? sub(c0, rot2([sz[2][0] * k, sz[2][1] * k], a)) : null }); }
    return out.length > 1500 ? { over:true, n:out.length } : out;
  }
  X.shatch = function(x){ var e = x.e, L = e.loops.map(function(l){ return l.map(x.P); }); x.o({ k:'hatch', loops:L, pat:'SOLID', hit:1 });
    if (e.src.kind === 'wipe'){ x.o({ k:'wipe', loops:L }); return; }
    var T0 = shTiles(x.doc, e); if (T0.over){ e._over = T0.n; return; } delete e._over;
    T0.forEach(function(t){ if (e.src.kind === 'image') x.o({ k:'tile', pts:t.pts.map(x.P), i:t.i }); else x.sub({ t:'insert', name:e.src.name, p:t.ip, sx:t.sc, sy:t.sc, rot:e.ang || 0, layer:e.layer }); }); };

  var xf1 = ED.xform;
  ED.xform = function(e, t){
    var f = t.f;
    switch (e.t){
      case 'arctext': e.c = f(e.c); ['r', 'h', 'off', 'sp', 'offL', 'offR'].forEach(function(k){ if (e[k]) e[k] *= t.k; }); if (t.mir){ var a0 = angT(e.a1, t), a1 = angT(e.a0, t); e.a0 = a0; e.a1 = a1; } else { e.a0 += t.rot; e.a1 += t.rot; } return e;
      case 'rtext': e.p = f(e.p); e.h *= t.k; if (!t.mir) e.rot = (e.rot || 0) + t.rot; return e;
      case 'shape': e.p = f(e.p); e.h *= t.k; if (t.mir){ e.rot = 2 * t.th - (e.rot || 0); e.mir = !e.mir; } else e.rot = (e.rot || 0) + t.rot; return e;
      case 'qleader': if (e.atp) e.atp = f(e.atp); break;
      case 'shatch': e.loops = e.loops.map(function(l){ return l.map(f); }); if (e.org) e.org = f(e.org); e.tw *= t.k; e.th *= t.k; e.gap = (e.gap || 0) * t.k; if (e.src.sc) e.src.sc *= t.k; e.ang = angT(e.ang || 0, t); return e;
    }
    return xf1(e, t);
  };
  Object.assign(NM, { arctext:'Chữ theo cung', rtext:'RText (chữ từ tệp)', shape:'Shape', shatch:'SuperHatch' });
  var SRCN = { image:'Ảnh', block:'Block', xref:'Xref', wipe:'Vùng che' };
  PX.arctext = function(d, e, f){ var a = e.arc != null && d.byId[e.arc]; return [['Nội dung', esc(e.s)], ['Kiểu chữ', esc(e.style)], ['Chiều cao', f(e.h), 'num'], ['Hệ số rộng', f(e.wf || 1, 2), 'ro'], ['Canh chữ', AL[e.al] || 'Vừa cung', 'ro'], ['Vị trí', e.side === 'in' ? 'Phía lõm' : 'Phía lồi', 'ro'], ['Hướng chữ', e.dir === 'in' ? 'Hướng vào tâm' : 'Hướng ra ngoài', 'ro'], ['Tâm X', f(e.c[0]), 'num'], ['Tâm Y', f(e.c[1]), 'num'], ['Bán kính', f(e.r), 'num'], ['Cách cung', f(e.off || 0), 'ro'], ['Gắn với cung', a && a.t === 'arc' && sameArc(e, a) ? 'Có' : 'Không', 'ro']]; };
  PX.rtext = function(d, e, f){ var fl = e.src !== 'diesel', ok = !fl || exists(d, e.path); return [['Nguồn', fl ? 'Tệp chữ' : 'Biểu thức DIESEL', 'ro'], [fl ? 'Đường dẫn' : 'Biểu thức', '<span class="mono">' + esc(fl ? e.path : e.expr) + '</span>', 'html'], ['Trạng thái', ok ? '<span class="tag ok">Đã nạp</span>' : '<span class="tag warn">Không tìm thấy</span>', 'html'], ['Kiểu chữ', esc(e.style)], ['Chiều cao', f(e.h), 'num'], ['Góc xoay', f(e.rot || 0, 0) + '°', 'num'], ['Vị trí X', f(e.p[0]), 'num'], ['Vị trí Y', f(e.p[1]), 'num'], ['Số dòng', String(rtLines(d, e).length), 'ro']]; };
  PX.shape = function(d, e, f){ var sd = shapeDef(d, e.name); return [['Tên shape', esc(e.name), 'ro'], ['Tệp shape', sd ? esc(sd.file) : '<span class="tag warn">Chưa nạp</span>', 'html'], ['Chiều cao', f(e.h), 'num'], ['Góc xoay', f(e.rot || 0, 0) + '°', 'num'], ['Vị trí X', f(e.p[0]), 'num'], ['Vị trí Y', f(e.p[1]), 'num']]; };
  PX.shatch = function(d, e, f){ var a = e.loops.reduce(function(s, l, i){ return s + (i ? -1 : 1) * G.polyArea(l); }, 0), sz = shSize(d, e), n = e._p.filter(function(pr){ return pr.k === 'tile'; }).length || (e.src.kind === 'image' || e.src.kind === 'wipe' ? 0 : shTiles(d, e).length || 0);
    return [['Kiểu nguồn', SRCN[e.src.kind], 'ro'], ['Nguồn', esc(e.src.kind === 'image' ? (MAT[e.src.name] || {}).n || e.src.name : e.src.kind === 'wipe' ? 'Màu nền vùng vẽ' : e.src.name), 'ro']].concat(e.src.kind === 'wipe' ? [] : [['Kích thước ô', sz ? f(sz[0], 0) + ' × ' + f(sz[1], 0) : '—', 'ro'], ['Góc', f(e.ang || 0, 0) + '°', 'num'], ['Khe giữa ô', f(e.gap || 0), 'ro'], ['Kiểu xếp', e.stag ? 'So le' : 'Thẳng hàng', 'ro']].concat(e.src.kind === 'image' ? [['Mờ', String(e.fade || 0), 'num']] : []).concat([['Số ô', e._over ? '<span class="tag warn">' + TC.n(e._over) + ' ô, quá nhiều</span>' : TC.n(n), 'html']])).concat([['Diện tích', f(Math.abs(a), 0) + ' (' + f(Math.abs(a) / 1e6) + ' m²)', 'ro']]); };
  function baseOf(e, o){ var c = Object.assign({ layer:e.layer }, o); if (e.color != null) c.color = e.color; return c; }
  TC.explodeExt.arctext = function(d, e){ return e._p.filter(function(pr){ return pr.k === 'text'; }).map(function(pr){ return baseOf(e, { t:'text', p:pr.p, h:pr.h, s:pr.s, rot:pr.rot, al:'m', style:e.style }); }); };
  TC.explodeExt.rtext = function(d, e){ var ls = rtLines(d, e), n = Math.max.apply(null, ls.map(function(s){ return s.length; }).concat([4])); return [baseOf(e, { t:'mtext', p:add2(e.p, rot2([0, e.h], e.rot || 0)), w:(n + 1) * e.h * .62, h:e.h, s:ls.join('\n'), style:e.style, rot:e.rot || 0, al:'l', ls:1.6667 })]; };
  TC.explodeExt.shape = function(d, e){ return e._p.filter(function(pr){ return pr.k === 'seg'; }).map(function(pr){ return baseOf(e, { t:'line', p1:pr.a, p2:pr.b }); }); };
  // SuperHatch bằng block, xref: phá thành nét đã cắt theo vùng; vùng che thành WIPEOUT; ảnh không phá được
  TC.explodeExt.shatch = function(d, e){
    if (e.src.kind === 'wipe') return [baseOf(e, { t:'wipeout', loops:e.loops })]; if (e.src.kind === 'image') return null;
    var inR = function(p){ return e.loops.filter(function(l){ return G.inPoly(p, l); }).length % 2 === 1; }, edges = [], out = [];
    e.loops.forEach(function(l){ l.forEach(function(p, i){ edges.push([p, l[(i + 1) % l.length]]); }); });
    var segOut = function(a, b){ var ts = [0, 1]; edges.forEach(function(ed){ var q = G.segInt(a, b, ed[0], ed[1]); if (q) ts.push(G.dist(a, q) / (G.dist(a, b) || 1)); }); ts.sort(function(x, y){ return x - y; }); for (var i = 0; i < ts.length - 1; i++){ var p = add2(a, sub(b, a), ts[i]), q2 = add2(a, sub(b, a), ts[i + 1]); if (G.dist(p, q2) > 1e-6 && inR(add2(a, sub(b, a), (ts[i] + ts[i + 1]) / 2))) out.push(baseOf(e, { t:'line', p1:p, p2:q2 })); } };
    e._p.forEach(function(pr){ if (pr.k === 'seg') segOut(pr.a, pr.b); else if (pr.k === 'arc' || pr.k === 'circ'){ var sw = arcSw(pr), a0 = pr.k === 'circ' ? 0 : pr.a0; for (var i = 0; i < 16; i++) segOut(polar(pr.c, pr.r, a0 + sw * i / 16), polar(pr.c, pr.r, a0 + sw * (i + 1) / 16)); } else if (pr.k === 'text' && inR(pr.p)) out.push(baseOf(e, { t:'text', p:pr.p, h:pr.h, s:pr.s, rot:pr.rot, al:pr.al, style:pr.style })); });
    return out;
  };
  TC.gripsExt.push(function(add, e, d){
    if (e.t === 'rtext' || e.t === 'shape') add(e.p, 'p', 'sq', { drag:function(x, p){ x.p = p; } });
    if (e.t === 'arctext'){ var L = atLayout(d, e), m = L[Math.floor(L.length / 2)]; if (m) add(m.p, 'aoff', 'sq', { drag:function(x, p){ x.off = Math.max(0, x.side === 'in' ? x.r - G.dist(x.c, p) - x.h / 2 : G.dist(x.c, p) - x.r - x.h / 2); } }); }
    if (e.t === 'shatch' && e.src.kind !== 'wipe') add(centroid(e.loops[0]), 'org', 'circ', { drag:function(x, p, g){ var bb = bbOf(x.loops[0]), o = x.org || [bb[0], bb[1]]; x.org = [o[0] + p[0] - g.p[0], o[1] + p[1] - g.p[1]]; } });
  });
  // nhấp đúp theo loại đối tượng (07.1 §9.10): mở lại lệnh tạo ra đối tượng ở chế độ sửa
  var dbl1 = TC.cad.hook.dbl;
  TC.cad.hook.dbl = function(e, m){
    if (e && !TC.isViewer() && !(D().layerMap[e.layer] || {}).lck){ var c = { arctext:'ARCTEXT', rtext:'RTEXT', shatch:'SUPERHATCH' }[e.t]; if (c){ TC.cmd.run(c, e); return true; } }
    return dbl1 ? dbl1(e, m) : false;
  };
  TC.cad.ctxExt.push(function(it, first, d){
    if (!first || TC.isViewer()) return; var sel = ents(d.sel), tx = sel.filter(function(e){ return /^(text|mtext)$/.test(e.t); });
    if (first.t === 'arctext') it.push({ label:'Sửa chữ theo cung…', icon:'arctext', act:function(){ TC.cmd.run('ARCTEXT', first); } });
    if (first.t === 'rtext') it.push({ label:'Sửa RText…', icon:'rtext', act:function(){ TC.cmd.run('RTEXT', first); } }, { label:'Nạp lại chữ từ tệp', icon:'refresh', act:function(){ var n = 0; allEnts(d).forEach(function(x){ if (x.t === 'rtext'){ TC.prepEnt(d, x); n++; } }); V().draw(); TC.cmdline.msg('Đã nạp lại ' + n + ' RText từ tệp nguồn.'); } });
    if (first.t === 'shatch') it.push({ label:'Sửa SuperHatch…', icon:'superhatch', act:function(){ TC.cmd.run('SUPERHATCH', first); } });
    if (first.t === 'shape') it.push({ label:'Đổi shape thành block', icon:'shp2blk', act:function(){ TC.cmd.run('SHP2BLK', first); } });
    if (first.t === 'insert' && !first.xref) it.push({ label:'Tỉ lệ tại điểm chèn…', icon:'scale', act:function(){ TC.cmd.run('BSCALE'); } });
    if (tx.length) it.push(tx.some(function(e){ return e.mask || e.bg; }) ? { label:'Bỏ che nền chữ', icon:'bgmask', act:function(){ TC.cmd.run('TEXTUNMASK'); } } : { label:'Che nền chữ', icon:'bgmask', act:function(){ TC.cmd.run('TEXTMASK'); } });
  });

  // xem trước một nhóm đối tượng tạm trong canvas của hộp thoại (luôn nền vùng vẽ)
  function pvDraw(c, d, list, pad, bb0){
    var x = c.getContext('2d'), W = c.width, Hh = c.height; x.setTransform(1, 0, 0, 1, 0, 0); x.fillStyle = TC.cssv('--c-canvas'); x.fillRect(0, 0, W, Hh);
    list = list.map(function(e){ return TC.prepEnt(d, e, true); }); var b = bb0 || [Infinity, Infinity, -Infinity, -Infinity];
    if (!bb0) list.forEach(function(e){ var q = e._bb; if (!q || !isFinite(q[0])) return; b = [Math.min(b[0], q[0]), Math.min(b[1], q[1]), Math.max(b[2], q[2]), Math.max(b[3], q[3])]; }); if (!isFinite(b[0])) return null;
    var f = Object.create(TC.View.prototype); f.W = W; f.H = Hh; f.doc = d; f.dpr = 1;
    var v = { cx:(b[0] + b[2]) / 2, cy:(b[1] + b[3]) / 2, s:Math.min((W - (pad || 24)) / Math.max(b[2] - b[0], 1), (Hh - (pad || 24)) / Math.max(b[3] - b[1], 1)) };
    list.forEach(function(e){ f.drawPrims(x, e, v, false); }); return { f:f, v:v };
  }
  function fieldErr(dlg, id, msg){ var inp = $('#' + id, dlg.el), m = $('#' + id + '-msg', dlg.el); if (inp) inp.classList.toggle('err', !!msg); var box = inp && inp.closest('.numf'); if (box) box.classList.toggle('err', !!msg); if (m){ m.className = 'fmsg ' + (msg ? 'err' : 'hint'); m.innerHTML = msg ? ic('warn', 'xs') + esc(msg) : ''; } return !msg; }
  function nv(dlg, id, def){ var i = $('#' + id, dlg.el); var r = i ? TC.parseNum(i.value, 'none') : { err:1 }; return r.err ? def : r.v; }

  /* ================= Block: XLIST, BLOCKTOXREF, BSCALE ================= */
  function nestPath(d, name, target){ var bd = d.blocks[name]; if (!bd) return null; for (var i = 0; i < bd.ents.length; i++){ var x = bd.ents[i]; if (x === target) return [name]; if (x.t === 'insert'){ var r = nestPath(d, x.name, target); if (r) return [name].concat(r); } } return null; }
  // đối tượng lồng dưới con trỏ: nguyên tố gần nhất của block (hoặc xref) có src khác chính block
  function nestPick(d, s){
    var v = V(), id = v.hitTest(s[0], s[1], 7), e = id && d.byId[id]; if (!e || e.t !== 'insert' || !d.blocks[e.name]) return null;
    var w = e._paper ? v.toW(s[0], s[1]) : v.sToM(s[0], s[1]) || v.toW(s[0], s[1]), mv = v.modelView(), tol = 8 / (e._paper || !mv ? v.vw().s : mv.v.s), best = null, bd = Infinity;
    e._p.forEach(function(pr){ var dd = pr.k === 'seg' ? G.segDist(w, pr.a, pr.b) : pr.k === 'circ' ? Math.abs(G.dist(w, pr.c) - pr.r) : pr.k === 'arc' ? (G.inArc(G.ang(pr.c, w), pr.a0, pr.a1) ? Math.abs(G.dist(w, pr.c) - pr.r) : Infinity) : pr.k === 'ell' ? Math.abs(G.dist(w, pr.c) - (pr.rx + pr.ry) / 2) : pr.k === 'text' ? Math.max(0, G.dist(w, pr.p) - pr.h * String(pr.s).length * .35) : pr.k === 'hatch' ? (pr.loops.some(function(l){ return G.inPoly(w, l); }) ? tol * .9 : Infinity) : Infinity; if (dd < bd){ bd = dd; best = pr; } });
    if (!best || bd > tol * 2 || best.src === e) return null;
    return { ins:e, src:best.src, path:nestPath(d, e.name, best.src) || [e.name], prims:e._p.filter(function(pr){ return pr.src === best.src; }) };
  }
  function xlistRows(d, n){
    var s = n.src, host = n.ins, L0 = s.layer === '0', lay = L0 ? host.layer : s.layer, L = d.layerMap[lay] || {}, c = s.color;
    var col = c == null ? 'ByLayer · ' + esc(TC.aciName(L.color || 7)) : c === 0 ? 'ByBlock · ' + esc(SC.colorLabel(host.color)) : esc(SC.colorLabel(c)), lt = s.lt || 'ByLayer', ltr = lt === 'ByLayer' ? L.lt || 'Continuous' : lt;
    return [['Loại đối tượng', esc(NM[s.t] || s.t)], [host.xref ? 'Tên xref' : 'Tên block', esc(n.path[n.path.length - 1])], ['Tham chiếu ngoài cùng', esc(host.xref || host.name) + (host.xref ? ' <span class="tag info">xref</span>' : '')], ['Đường lồng', esc(n.path.join(' › ')) + ' <span class="faint">(' + n.path.length + ' cấp)</span>'],
      ['Layer', '<i class="sw" style="background:' + colHex(L.color, true) + '"></i>' + esc(s.layer) + (L0 ? ' <span class="faint">→ hiện theo ' + esc(lay) + '</span>' : '')], ['Màu', SC.colorSw(c == null ? L.color : c === 0 ? host.color : c) + col], ['Linetype', esc(lt) + (lt === 'ByLayer' ? ' <span class="faint">(' + esc(ltr) + ')</span>' : '')], ['Độ dày nét', s.lw != null ? TC.d(s.lw) + ' mm' : 'ByLayer <span class="faint">(' + TC.d(L.lw || .25) + ' mm)</span>']];
  }
  IMPL.XLIST = function(ctx){
    var d = D(); ctx.badge('XL');
    var hl = function(n){ return { k:'ents', list:[{ _p:n.prims, layer:n.ins.layer }], color:TC.cssv('--c-accent'), w:2.4 }; };
    return (function pick(){ return ctx.point('Chọn đối tượng lồng trong block hoặc xref', { pick:true, dyn:false, onMove:function(p, temp){ var n = V().mouse && nestPick(d, V().mouse); if (n) temp.push(hl(n)); } }).then(function(r){
      if (!r || r.enter || r.kw) return; var s = r.s || V().mToS(r.p) || V().toS(r.p), n = s && nestPick(d, s); if (!n){ ctip(r, 'Không có đối tượng lồng trong block hoặc xref tại đây'); return pick(); }
      V().temp = [hl(n)]; V().drawOver(); var rows = xlistRows(d, n); ctx.out('XLIST · ' + esc(NM[n.src.t] || n.src.t) + ' trong ' + esc(n.path.join(' › ')) + ' · layer ' + esc(n.src.layer) + '.');
      return new Promise(function(res){ var ex = TC.topDialog(); if (ex && ex.id === 'xlist') ex.close(); // lần chọn trước còn mở (lệnh đã bị hủy)
        var dlg = TC.dialog({ cls:'mr2', id:'xlist', title:'Đối tượng lồng', cmd:'XLIST', code:'UI-MR2', w:470, modal:false, onClose:function(r2){ V().temp = []; V().drawOver(); res(r2 === 'again'); },
          body:'<div class="stack"><dl class="mr2-kv">' + rows.map(function(x){ return '<dt>' + x[0] + '</dt><dd>' + x[1] + '</dd>'; }).join('') + '</dl><p class="faint mr2-s">Đối tượng tô sáng trên vùng vẽ. Thuộc tính layer 0 hiện theo block cha.</p></div>',
          foot:'<button class="btn ghost" type="button" id="xl-copy">' + ic('copyclip') + 'Sao chép</button><span class="grow"></span><button class="btn" type="button" id="xl-again">Chọn đối tượng khác</button><button class="btn primary" type="button" data-x="close">Đóng</button>' });
        $('#xl-copy', dlg.el).addEventListener('click', function(){ TC.copyText(rows.map(function(x){ return x[0] + ': ' + x[1].replace(/<[^>]+>/g, '').replace(/\s+/g, ' ').trim(); }).join('\n')); });
        $('#xl-again', dlg.el).addEventListener('click', function(){ dlg.close('again'); });
      }).then(function(again){ if (again) return pick(); }); }); })();
  };

  // BLOCKTOXREF: ghi block ra tệp (hoặc dùng tệp có sẵn) rồi thay mọi tham chiếu bằng xref
  var B2X = { mode:'new', type:'Attach', rel:true, purge:true };
  IMPL.BLOCKTOXREF = function(ctx){
    var d = D(); ctx.badge('BX');
    return ED.pick(ctx, 'Chọn block cần đổi thành xref', { types:['insert'], bad:'Chọn một tham chiếu block' }).then(function(r){
      if (!r || !r.e) return; var e = r.e, nm = e.name; if (e.xref){ ctx.out('Đối tượng này đã là xref <b>' + esc(e.xref) + '</b>.'); return; } if (!d.blocks[nm]) return;
      var refs = allEnts(d).filter(function(x){ return x.t === 'insert' && x.name === nm && !x.xref; }), locked = refs.filter(function(x){ return (d.layerMap[x.layer] || {}).lck; }), nested = Object.keys(d.blocks).filter(function(b){ return d.blocks[b].ents.some(function(x){ return x.t === 'insert' && x.name === nm; }); });
      refs = refs.filter(function(x){ return locked.indexOf(x) < 0; });
      var dir = dirOf(d.path).replace(/\\[^\\]+$/, '') + '\\Thu-vien', libs = Object.keys(FS).filter(function(k){ return /\\Thu-vien\\.+\.dwg$/i.test(k); }); B2X.mode = libs.some(function(k){ return base(k).toUpperCase() === nm.toUpperCase() + '.DWG'; }) ? 'lib' : 'new';
      return new Promise(function(res){
        var dlg = TC.dialog({ cls:'mr2', id:'b2x', title:'Đổi block thành xref', cmd:'BLOCKTOXREF', code:'UI-MR2', w:640, onClose:function(v){ if (v !== true) res(); },
          body:'<div class="mr2-b2x"><div class="stack"><canvas id="bx-pv" width="180" height="140" class="pvc"></canvas><div class="faint mr2-s" style="text-align:center">' + esc(nm) + '</div></div><div class="stack">' +
            '<p>' + qty(refs.length, 'tham chiếu') + ' block <b>' + esc(nm) + '</b> sẽ được thay bằng xref, giữ nguyên điểm chèn, tỉ lệ, góc xoay.' + (locked.length ? ' <span class="tag warn">' + locked.length + ' trên layer khóa, bỏ qua</span>' : '') + '</p>' +
            H.grp('Tệp bản vẽ của xref', H.rad('bx-m', 'new', 'Ghi block ra tệp mới (WBLOCK)', B2X.mode === 'new') + '<div class="frow" style="--lab-w:22px"><span></span><input class="inp mono" id="bx-path" value="' + esc(dir + '\\' + nm + '.dwg') + '" spellcheck="false"></div><span class="fmsg" id="bx-path-msg"></span>' +
              H.rad('bx-m', 'lib', 'Dùng tệp có sẵn', B2X.mode === 'lib') + '<div class="frow" style="--lab-w:22px"><span></span>' + H.sel('bx-lib', libs.map(function(k){ return k; }), libs.filter(function(k){ return base(k).toUpperCase() === nm + '.DWG'; })[0] || libs[0]) + '</div>') +
            '<div class="grid2">' + H.grp('Kiểu tham chiếu', H.rad('bx-t', 'Attach', 'Attach', B2X.type === 'Attach') + H.rad('bx-t', 'Overlay', 'Overlay (không lồng)', B2X.type === 'Overlay')) + H.grp('Đường dẫn lưu', H.rad('bx-r', '1', 'Tương đối', B2X.rel) + H.rad('bx-r', '0', 'Đầy đủ', !B2X.rel)) + '</div>' +
            H.chk('bx-purge', 'Dọn định nghĩa block ' + esc(nm) + ' sau khi đổi', B2X.purge, !!nested.length) + (nested.length ? '<div class="note">' + ic('info') + '<span>Block nằm lồng trong ' + nested.map(esc).join(', ') + ': giữ định nghĩa để các block đó vẫn đúng.</span></div>' : '') + '</div></div>',
          foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="bx-ok"' + (refs.length ? '' : ' disabled') + '>Đổi ' + qty(refs.length, 'block') + ' thành xref</button>' });
        pvDraw($('#bx-pv', dlg.el), d, [{ t:'insert', name:nm, p:[0, 0], sx:1, sy:1, rot:0, layer:e.layer }]);
        function sync(){ var m = $('[name="bx-m"]:checked', dlg.el).value; $('#bx-path', dlg.el).disabled = m !== 'new'; $('#bx-lib', dlg.el).disabled = m !== 'lib'; }
        dlg.el.addEventListener('change', sync); sync();
        $('#bx-ok', dlg.el).addEventListener('click', function(){
          var m = $('[name="bx-m"]:checked', dlg.el).value, full = m === 'new' ? $('#bx-path', dlg.el).value.trim() : $('#bx-lib', dlg.el).value;
          if (m === 'new' && !fieldErr(dlg, 'bx-path', !/^([a-z]:\\|\\\\).+\.dwg$/i.test(full) ? 'Cần đường dẫn đầy đủ tới tệp .dwg, ví dụ D:\\DuAn\\Thu-vien\\' + nm + '.dwg' : exists(d, full) ? 'Tệp đã có. Đổi tên tệp hoặc chọn “Dùng tệp có sẵn”.' : null)) return;
          B2X = { mode:m, type:$('[name="bx-t"]:checked', dlg.el).value, rel:$('[name="bx-r"]:checked', dlg.el).value === '1', purge:$('#bx-purge', dlg.el).checked };
          dlg.close(true); res({ full:full });
        });
      }).then(function(o){
        if (!o) return; var xn = base(o.full).replace(/\.dwg$/i, ''), same = xn.toUpperCase() === nm.toUpperCase(), purge = !nested.length && (B2X.purge || same), save = Object.assign({}, d.blocks[nm]);
        if (same && nested.length){ ctx.out('Không đổi được: block ' + esc(nm) + ' nằm lồng trong block khác và xref sẽ trùng tên. Ghi ra tệp tên khác.'); return; }
        var rel = B2X.rel && o.full.toLowerCase().indexOf(dirOf(d.path).replace(/\\[^\\]+$/, '').toLowerCase()) === 0 ? '..' + o.full.slice(dirOf(d.path).replace(/\\[^\\]+$/, '').length) : o.full;
        var keys = [nm, xn], get = function(){ var b = {}; keys.forEach(function(k){ b[k] = d.blocks[k] || null; }); return { b:b, x:d.xrefs }; }, set = function(s){ keys.forEach(function(k){ if (s.b[k]) d.blocks[k] = s.b[k]; else delete d.blocks[k]; }); d.xrefs = s.x; allEnts(d).forEach(function(x){ if (x.t === 'insert' && keys.indexOf(x.name) >= 0) TC.prepEnt(d, x); }); V().draw(); A().refreshPalettes(); };
        ED.run('BLOCKTOXREF', function(){ var st = ED.state(get, set);
          if (purge) delete d.blocks[nm]; d.blocks[xn] = { name:xn, desc:'Xref ' + xn, ents:JSON.parse(JSON.stringify(save.ents)), xref:true };
          d.xrefs = d.xrefs.filter(function(x){ return x.name !== xn; }).concat([{ name:xn, type:B2X.type, status:'Đã nạp', path:rel, found:o.full, size:'38 KB', date:TC.date() }]);
          refs.forEach(function(x){ ED.mod(x, function(y){ y.name = xn; y.xref = xn; }); }); st(); });
        if (!fsKey(o.full)) FS[o.full] = '.';
        TC.cad.clearSel(); V().draw(); A().refreshPalettes();
        ctx.out('Đã đổi ' + qty(refs.length, 'tham chiếu') + ' block ' + esc(nm) + ' thành xref <b>' + esc(xn) + '</b> (' + B2X.type + ', ' + lbl(rel) + ')' + (purge ? '; đã dọn định nghĩa block.' : '.'));
        TC.toast('Đã đổi ' + refs.length + ' block <b>' + esc(nm) + '</b> thành xref.', { log:false, act:{ label:'Mở palette Xref', fn:function(){ A().showPalette('xref'); } } });
      });
    });
  };

  IMPL.BSCALE = function(ctx){
    var d = D(); ctx.badge('BSC');
    return selEdit(ctx, 'Chọn block').then(function(ids){ if (!ids.length) return; var es = ents(ids, /^insert$/);
      if (!es.length){ ctx.out('Không có block nào trong vùng chọn.'); return; } if (es.length < ids.length) ctx.out((ids.length - es.length) + ' đối tượng không phải block được bỏ qua.', 'faint');
      var e0 = es[0], b = e0._bb, ref = Math.max(1, Math.hypot(b[2] - b[0], b[3] - b[1]) / 2), mk = function(k){ return es.slice(0, 300).map(function(e){ var c = ED.clean(e); c.sx *= k; c.sy *= k; return c; }); };
      var go = function(k, how){ if (!(k > 0)){ ctx.out('Hệ số tỉ lệ phải lớn hơn 0.'); return; } return ED.run('BSCALE', function(){ es.forEach(function(e){ ED.mod(e, function(x){ x.sx *= k; x.sy *= k; }); }); TC.cad.clearSel(); ctx.out('Đã đổi tỉ lệ ' + qty(es.length, 'block') + ' quanh điểm chèn của từng block, hệ số ' + TC.d(k, 4) + how + '.'); }); };
      return ctx.point('Hệ số tỉ lệ hoặc', { base:e0.p, num:true, def:'1.0000', opts:[['Tham chiếu', 'R']], onMove:function(p, temp){ var k = G.dist(e0.p, p) / ref; temp.push(ED.temp(mk(k))); TC.cad.clearLabels(); var s = V().mToS(p) || V().toS(p); if (s) TC.cad.label('× ' + TC.d(k, 3), s[0] + 14, s[1] - 24); } }).then(function(r){ TC.cad.clearLabels();
        if (r.num != null) return go(r.num, '');
        if (r.p) return go(G.dist(e0.p, r.p) / ref, ' (theo con trỏ)');
        if (r.kw === 'R') return ctx.val('Độ dài tham chiếu', TC.d(ref)).then(function(a){ var A1 = num(a, 0); if (!(A1 > 0)){ ctx.out('Độ dài tham chiếu phải lớn hơn 0.'); return; } return ctx.val('Độ dài mới', TC.d(A1)).then(function(b2){ return go(num(b2, 0) / A1, ' (tham chiếu ' + TC.d(A1) + ' → ' + TC.d(num(b2, 0)) + ')'); }); });
        ctx.out('Giữ nguyên tỉ lệ.', 'faint'); }); });
  };

  /* ================= Văn bản: ARCTEXT, TEXTMASK, TEXTUNMASK, TEXTFIT, TORIENT, RTEXT ================= */
  var AT = Object.assign({ al:'F', side:'out', dir:'out', wf:1, sp:0, offL:0, offR:0, rev:false, bold:false, ital:false }, TC.load('mr2at', {}));
  // hộp thoại chữ theo cung (kiểu ArcAlignedText): xem trước trong hộp và trên vùng vẽ
  function atDlg(d, arc, e0){
    return new Promise(function(res){
      var o = Object.assign({ s:'ĐƯỜNG NỘI BỘ RỘNG 7 M', h:ED.cur(d).th || 250, style:ED.cur(d).tstyle || 'Standard' }, AT, e0 ? ED.clean(e0) : {}), edit = !!e0;
      if (!e0) o.off = o.h * .5;
      var seg = function(n, list, cur){ return '<span class="seg" role="group" data-seg="' + n + '">' + list.map(function(x){ return '<button type="button" data-v="' + x[0] + '" aria-pressed="' + (x[0] === cur) + '">' + x[1] + '</button>'; }).join('') + '</span>'; };
      var dlg = TC.dialog({ cls:'mr2', id:'arctext', title:edit ? 'Sửa chữ theo cung' : 'Chữ theo cung', cmd:'ARCTEXT', code:'UI-MR2', w:820, onClose:function(v){ if (e0){ delete e0._edit; V().draw(); } V().temp = []; V().drawOver(); if (v !== true) res(null); },
        body:'<div class="mr2-at"><div class="stack">' +
          '<div class="field"><label for="at-s">Nội dung</label><input class="inp" id="at-s" value="' + esc(o.s) + '" spellcheck="false"><span class="fmsg" id="at-s-msg"></span></div>' +
          '<div class="grid2">' + H.row('Kiểu chữ', H.sel('at-st', d.styles.map(function(s){ return s.name; }), o.style), 84) + H.row('Chiều cao', H.num('at-h', TC.d(o.h), { w:110 }), 84) + H.row('Hệ số rộng', H.num('at-wf', TC.d(o.wf || 1), { unit:'none', pick:false, step:.05, w:110 }), 84) + H.row('Giãn ký tự', H.num('at-sp', TC.d(o.sp || 0), { w:110 }), 84) + '</div>' +
          H.row('Canh chữ', seg('at-al', [['F', 'Vừa cung'], ['L', 'Trái'], ['C', 'Giữa'], ['R', 'Phải']], o.al), 84) +
          H.row('Vị trí', seg('at-side', [['out', 'Phía lồi'], ['in', 'Phía lõm']], o.side), 84) +
          H.row('Hướng chữ', seg('at-dir', [['out', 'Đỉnh chữ ra ngoài'], ['in', 'Đỉnh chữ vào tâm']], o.dir), 84) +
          '<div class="grid3">' + H.row('Cách cung', H.num('at-off', TC.d(o.off || 0), { pick:false, w:88 }), 64) + H.row('Lề trái', H.num('at-ol', TC.d(o.offL || 0), { pick:false, w:88 }), 46) + H.row('Lề phải', H.num('at-or', TC.d(o.offR || 0), { pick:false, w:88 }), 46) + '</div>' +
          '<div class="hrow">' + H.chk('at-rev', 'Đảo thứ tự chữ', o.rev) + H.chk('at-b', 'Đậm', o.bold) + H.chk('at-i', 'Nghiêng', o.ital) + '</div>' +
          '</div><div class="stack"><span class="gh">Xem trước</span><canvas id="at-pv" width="280" height="210" class="pvc"></canvas><div id="at-info" class="mr2-s"></div></div></div>',
        foot:'<span class="faint mr2-s">' + (edit ? 'Chữ vẫn gắn với cung: sửa cung thì chữ đi theo.' : 'Chữ gắn với cung đã chọn: sửa cung thì chữ đi theo.') + '</span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="at-ok">' + (edit ? 'Cập nhật chữ theo cung' : 'Đặt chữ theo cung') + '</button>' });
      function read(){ o.s = $('#at-s', dlg.el).value; o.style = $('#at-st', dlg.el).value; o.h = nv(dlg, 'at-h', o.h); o.wf = nv(dlg, 'at-wf', o.wf); o.sp = nv(dlg, 'at-sp', o.sp); o.off = nv(dlg, 'at-off', o.off); o.offL = nv(dlg, 'at-ol', o.offL); o.offR = nv(dlg, 'at-or', o.offR); o.rev = $('#at-rev', dlg.el).checked; o.bold = $('#at-b', dlg.el).checked; o.ital = $('#at-i', dlg.el).checked; }
      function mk(){ return Object.assign({ t:'arctext', c:arc.c.slice(), r:arc.r, a0:arc.a0, a1:arc.a1, layer:e0 ? e0.layer : ED.clayer(d) }, o, { t:'arctext' }); }
      function paint(){ read(); var e = mk(), L = atLayout(d, e), over = L.need - L.avail, info = $('#at-info', dlg.el);
        var pts = []; for (var i = 0; i <= 24; i++) pts.push(polar(arc.c, arc.r, arc.a0 + sweep(arc.a0, arc.a1) * i / 24)); L.forEach(function(ch){ pts.push(add2(ch.p, [o.h, o.h]), add2(ch.p, [-o.h, -o.h])); });
        pvDraw($('#at-pv', dlg.el), d, [{ t:'arc', c:arc.c, r:arc.r, a0:arc.a0, a1:arc.a1, layer:arc.layer || '0', color:8 }, e], 30, bbOf(pts));
        info.className = 'mr2-s ' + (over > 1e-6 && o.al !== 'F' ? 'warn' : 'faint');
        info.innerHTML = o.al === 'F' && L.gap < 0 ? ic('warn', 'xs') + ' Chữ dài hơn cung ' + TC.d(-L.gap * (Array.from(o.s).length - 1), 0) + ': ký tự sẽ chồng nhau. Giảm chiều cao hoặc lề.' : over > 1e-6 && o.al !== 'F' ? ic('warn', 'xs') + ' Chữ dài hơn đoạn cung ' + TC.d(over, 0) + ': phần thừa vượt ra ngoài cung.' : 'Cung dài ' + TC.d(L.avail, 0) + ' · chữ cần ' + TC.d(L.need, 0) + (o.al === 'F' && L.gap >= 0 ? ' · giãn ' + TC.d(L.gap, 0) + ' giữa các ký tự' : '');
        V().temp = [prev(e)]; V().drawOver(); }
      if (e0){ e0._edit = true; V().draw(); }
      if (innerWidth >= 1500){ dlg.el.style.left = (innerWidth - dlg.el.offsetWidth - 24) + 'px'; } // vùng vẽ bên trái vẫn thấy xem trước
      dlg.el.addEventListener('input', function(ev){ if (ev.target.id === 'at-s') paint(); });
      dlg.el.addEventListener('change', paint); dlg.el.addEventListener('numchange', paint);
      dlg.el.addEventListener('segchange', function(ev){ var g = ev.target.closest('[data-seg]').dataset.seg.slice(3); o[g] = ev.detail; paint(); });
      $('#at-ok', dlg.el).addEventListener('click', function(){ read();
        var ok = fieldErr(dlg, 'at-s', !o.s.trim() ? 'Nhập nội dung chữ.' : null) & fieldErr(dlg, 'at-h', !(o.h > 0) ? 'Chiều cao phải lớn hơn 0.' : null) & fieldErr(dlg, 'at-wf', !(o.wf > 0 && o.wf <= 10) ? 'Hệ số rộng từ 0,01 đến 10.' : null);
        if (!ok) return; AT = { al:o.al, side:o.side, dir:o.dir, wf:o.wf, sp:o.sp, offL:o.offL, offR:o.offR, rev:o.rev, bold:o.bold, ital:o.ital }; TC.save('mr2at', AT); var e = mk(); dlg.close(true); res(e); });
      paint();
    });
  }
  IMPL.ARCTEXT = function(ctx, pre){
    var d = D(); ctx.badge('AT');
    var edit = function(e){ if ((d.layerMap[e.layer] || {}).lck){ ctx.out('Chữ nằm trên layer bị khóa, không sửa được.'); return; } var a = e.arc != null && d.byId[e.arc], arc = a && a.t === 'arc' && sameArc(e, a) ? a : { c:e.c, r:e.r, a0:e.a0, a1:e.a1, layer:e.layer };
      return atDlg(d, arc, e).then(function(n){ if (!n) return; ED.run('ARCTEXT', function(){ ED.mod(e, function(x){ Object.keys(n).forEach(function(k){ if (k !== 'id' && k[0] !== '_') x[k] = n[k]; }); }); }); A().refreshPalettes(); ctx.out('Đã cập nhật chữ theo cung “' + esc(n.s) + '”.'); }); };
    var create = function(arc){ return atDlg(d, arc, null).then(function(n){ if (!n) return; n.arc = arc.id; ED.run('ARCTEXT', function(){ ED.add(ED.make(n)); }); d._atLink = true;
      ctx.out('Đã đặt chữ theo cung “' + esc(n.s) + '” (' + Array.from(n.s).length + ' ký tự, cao ' + TC.d(n.h) + ', ' + AL[n.al].toLowerCase() + '). Nhấp đúp để sửa; sửa cung thì chữ đi theo.'); }); };
    if (pre && pre.t === 'arctext') return edit(pre);
    if (pre && pre.t === 'arc') return create(pre);
    return ED.pick(ctx, 'Chọn cung hoặc chữ theo cung', { types:['arc', 'arctext'], bad:'Chọn một cung (ARC) hoặc chữ theo cung' }).then(function(r){ if (r && r.e) return r.e.t === 'arctext' ? edit(r.e) : create(r.e); });
  };

  var TM = Object.assign({ type:'W', off:.35, color:253 }, TC.load('mr2tm', {})), TMN = { W:'Vùng che (màu nền)', C:'Tô màu', B:'Nền MText' };
  function tmDesc(){ return TMN[TM.type] + (TM.type === 'C' ? ' ' + SC.colorLabel(TM.color) : '') + ', khoảng hở ' + TC.d(TM.off) + ' × chiều cao chữ'; }
  function tmDlg(){
    return new Promise(function(res){
      var o = Object.assign({}, TM), d = D();
      var dlg = TC.dialog({ cls:'mr2', id:'tmset', title:'Thiết lập che nền chữ', cmd:'TEXTMASK', code:'UI-MR2', w:600, onClose:function(v){ res(v === true); },
        body:'<div class="mr2-tm"><div class="stack">' + H.grp('Kiểu che', H.rad('tm-t', 'W', 'Vùng che: màu nền vùng vẽ, in ra giấy trắng', o.type === 'W') + H.rad('tm-t', 'C', 'Tô màu', o.type === 'C') + '<div class="hrow" style="padding-left:20px"><button class="dd" type="button" id="tm-col" style="width:150px"><span class="lbl" id="tm-coll"></span>' + ic('down', 'xs') + '</button></div>' + H.rad('tm-t', 'B', 'Nền MText (chữ một dòng dùng Vùng che)', o.type === 'B')) +
          H.row('Khoảng hở', H.num('tm-off', TC.d(o.off), { unit:'none', pick:false, step:.05, w:100 }) + '<span class="faint mr2-s">× chiều cao chữ</span>', 84) + '<p class="faint mr2-s">Vùng che là thuộc tính của chữ: di chuyển, sửa nội dung, đổi chiều cao thì vùng che đi theo. Lưu DWG thành wipeout và nhóm như Express Tools.</p></div>' +
          '<div class="stack"><span class="gh">Xem trước</span><canvas id="tm-pv" width="240" height="150" class="pvc"></canvas></div></div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="tm-ok">Lưu thiết lập</button>' });
      function paint(){ o.type = $('[name="tm-t"]:checked', dlg.el).value; o.off = nv(dlg, 'tm-off', o.off); $('#tm-coll', dlg.el).innerHTML = swc(o.color); $('#tm-col', dlg.el).disabled = o.type !== 'C';
        var L = [[0, 0], [3000, 0], [3000, 1800], [0, 1800]];
        pvDraw($('#tm-pv', dlg.el), d, [{ t:'hatch', loops:[L], pat:'ANSI31', sc:40, layer:'KT-HATCH-TUONG' }, { t:'line', p1:[0, 900], p2:[3000, 900], layer:'KT-TRUC' }, { t:'text', p:[1500, 900], h:300, s:'PHÒNG NGỦ', al:'m', style:'Standard', layer:'KT-TEXT', mask:{ type:o.type === 'B' ? 'W' : o.type, off:o.off, color:o.color } }], 16, [0, 0, 3000, 1800]); }
      dlg.el.addEventListener('change', paint); dlg.el.addEventListener('numchange', paint);
      $('#tm-col', dlg.el).addEventListener('click', function(){ SC.colorPicker(o.color).then(function(c){ if (c != null && c !== 0){ o.color = c; paint(); } }); });
      $('#tm-ok', dlg.el).addEventListener('click', function(){ o.off = nv(dlg, 'tm-off', -1); if (!fieldErr(dlg, 'tm-off', !(o.off >= 0 && o.off <= 5) ? 'Khoảng hở từ 0 đến 5 lần chiều cao chữ.' : null)) return; TM = o; TC.save('mr2tm', TM); dlg.close(true); });
      paint();
    });
  }
  IMPL.TEXTMASK = function(ctx){
    var d = D(); ctx.badge('TM'); ctx.out('Thiết lập hiện tại: ' + tmDesc() + '.', 'faint');
    var go = function(ids){ var ts = ents(ED.editable(ids, ctx), /^(text|mtext)$/); if (!ts.length){ ctx.out('Không có chữ (TEXT, MTEXT) trong vùng chọn.'); return; }
      var m = { type:TM.type, off:TM.off }; if (TM.type === 'C') m.color = TM.color; var one = ts.filter(function(e){ return e.t === 'text'; }).length;
      return ED.run('TEXTMASK', function(){ ts.forEach(function(e){ ED.mod(e, function(x){ x.mask = Object.assign({}, m, x.t === 'text' && m.type === 'B' ? { type:'W' } : {}); if (x.t === 'mtext' && m.type === 'B') x.bg = true; }); }); TC.cad.clearSel();
        ctx.out('Đã che nền ' + qty(ts.length, 'chữ') + ' (' + tmDesc() + ').' + (TM.type === 'B' && one ? ' ' + one + ' chữ một dòng dùng Vùng che.' : '') + ' Bỏ bằng TEXTUNMASK.'); }); };
    if (d.sel.length) return go(d.sel.slice());
    ctx.out('Bấm hoặc kéo khung để chọn chữ, Enter để che nền.', 'faint');
    return (function step(){ return ask(ctx, 'Chọn chữ cần che nền hoặc', [['Kiểu che', 'M'], ['Khoảng hở', 'O'], ['Thiết lập', 'S']]).then(function(k){
      if (k === 'M') return ctx.kw('Kiểu che', [['Vùng che', 'W'], ['Tô màu', 'C'], ['Nền MText', 'B']], TM.type).then(function(t){ if (TMN[t]){ TM.type = t; TC.save('mr2tm', TM); } return step(); });
      if (k === 'O') return ctx.val('Khoảng hở (× chiều cao chữ)', TC.d(TM.off)).then(function(v){ var n = num(v, -1); if (n >= 0 && n <= 5){ TM.off = n; TC.save('mr2tm', TM); } else ctx.out('Khoảng hở từ 0 đến 5 lần chiều cao chữ.'); return step(); });
      if (k === 'S') return tmDlg().then(function(){ ctx.out('Thiết lập: ' + tmDesc() + '.', 'faint'); return step(); });
      if (k){ ctx.out('Gõ M, O hoặc S để đổi thiết lập; chọn chữ rồi Enter để che nền.'); return step(); }
      if (!d.sel.length){ ctx.out('Chưa chọn chữ nào.'); return; } return go(d.sel.slice()); }); })();
  };
  IMPL.TEXTUNMASK = function(ctx){
    ctx.badge('TU');
    return selEdit(ctx, 'Chọn chữ cần bỏ che nền').then(function(ids){ if (!ids.length) return; var ts = ents(ids, /^(text|mtext)$/).filter(function(e){ return e.mask || e.bg; });
      if (!ts.length){ ctx.out('Không có chữ nào đang được che nền trong vùng chọn.'); return; }
      return ED.run('TEXTUNMASK', function(){ ts.forEach(function(e){ ED.mod(e, function(x){ delete x.mask; delete x.bg; }); }); TC.cad.clearSel(); ctx.out('Đã bỏ che nền ' + qty(ts.length, 'chữ') + '.'); }); });
  };

  // TEXTFIT: kéo điểm cuối để chữ vừa khoảng, đổi hệ số rộng, giữ chiều cao
  IMPL.TEXTFIT = function(ctx){
    var d = D(); ctx.badge('TF');
    return ED.pick(ctx, 'Chọn chữ cần co giãn', { types:['text'], bad:'Chọn chữ một dòng (TEXT)' }).then(function(r){ if (!r || !r.e) return; var e = r.e, f = frameOf(d, e), w1 = f.w / (e.wf || 1), start = f.lb;
      var fit = function(p){ var L = dot(sub(p, start), f.u); if (!(L > e.h * .2)) return null; var wf = L / w1, al = e.al || 'l', x = ED.clean(e); x.wf = Math.round(wf * 1e4) / 1e4; x.p = add2(start, f.u, al === 'c' || al === 'm' ? L / 2 : al === 'r' ? L : 0); if (al === 'm') x.p = add2(x.p, f.v, midOf(d, e)); return x; };
      return (function at(){ return ctx.point('Điểm cuối hoặc', { base:start, opts:[['Điểm đầu', 'S']], onMove:function(p, temp){ var x = fit(p); TC.cad.clearLabels(); if (!x) return; temp.push(prev(x)); var s = V().mToS(p) || V().toS(p); if (s) TC.cad.label('Hệ số rộng ' + TC.d(x.wf, 2), s[0] + 14, s[1] - 24); } }).then(function(q){ TC.cad.clearLabels();
        if (q.kw === 'S') return ctx.point('Điểm đầu mới', { base:f.lb }).then(function(s){ if (s.p) start = add2(f.lb, f.u, dot(sub(s.p, f.lb), f.u)); return at(); });
        if (!q.p) return; var x = fit(q.p); if (!x){ ctip(q, 'Điểm cuối phải nằm về phía cuối dòng chữ'); return at(); }
        return ED.run('TEXTFIT', function(){ ED.mod(e, function(y){ y.wf = x.wf; y.p = x.p; }); ctx.out('Đã co giãn chữ vừa khoảng ' + TC.d(w1 * x.wf) + ': hệ số rộng ' + TC.d(x.wf, 3) + ', chiều cao giữ ' + TC.d(e.h) + '.'); }); }); })(); });
  };

  // TORIENT: chữ quay ngược được lật 180° quanh tâm; hoặc đặt góc tuyệt đối cho mọi chữ đã chọn
  IMPL.TORIENT = function(ctx){
    var d = D(); ctx.badge('TO');
    return selEdit(ctx, 'Chọn chữ, MText').then(function(ids){ if (!ids.length) return; var ts = ents(ids, /^(text|mtext|rtext)$/);
      if (!ts.length){ ctx.out('Không có chữ (TEXT, MTEXT, RTEXT) trong vùng chọn.'); return; }
      var turn = function(e, a){ var c = tCenter(d, e), x = ED.clean(e), dl = a - (e.rot || 0); x.p = add2(c, rot2(sub(e.p, c), dl)); x.rot = an360(a); return x; };
      var bad = ts.filter(function(e){ var a = an360(e.rot || 0); return a > 90 + 1e-6 && a <= 270 + 1e-6; });
      V().temp = bad.length ? [ED.temp(bad.map(function(e){ return turn(e, (e.rot || 0) - 180); }), { real:true })] : []; V().drawOver();
      ctx.out(bad.length ? bad.length + ' / ' + ts.length + ' chữ đang quay ngược; xem trước kết quả trên vùng vẽ.' : 'Mọi chữ đã chọn đều đọc xuôi.', 'faint');
      return ctx.val('Góc tuyệt đối mới', 'dễ đọc nhất').then(function(v){ V().temp = []; V().drawOver(); var s = String(v).trim();
        if (s === 'dễ đọc nhất' || !s){ if (!bad.length){ ctx.out('Không có chữ nào cần xoay.'); return; } return ED.run('TORIENT', function(){ bad.forEach(function(e){ var x = turn(e, (e.rot || 0) - 180); ED.mod(e, function(y){ y.p = x.p; y.rot = x.rot; }); }); TC.cad.clearSel(); ctx.out('Đã xoay ' + qty(bad.length, 'chữ') + ' cho dễ đọc (lật 180° quanh tâm chữ); ' + (ts.length - bad.length) + ' chữ đã đọc xuôi, giữ nguyên.'); }); }
        var a = num(s.replace('°', ''), NaN); if (!isFinite(a)){ ctx.out('Nhập góc bằng số (độ), hoặc Enter để chọn hướng dễ đọc nhất.'); return; }
        return ED.run('TORIENT', function(){ ts.forEach(function(e){ var x = turn(e, a); ED.mod(e, function(y){ y.p = x.p; y.rot = x.rot; }); }); TC.cad.clearSel(); ctx.out('Đã đặt góc ' + TC.d(an360(a), 0) + '° cho ' + qty(ts.length, 'chữ') + ' (xoay quanh tâm chữ).'); }); },
      function(err){ V().temp = []; V().drawOver(); throw err; }); });
  };

  // DIESEL tối thiểu cho RTEXT: getvar, edtime, upper, lower, strlen
  function gvar(d, n){ n = String(n).trim().toUpperCase(); if (n === 'DWGNAME') return d.name; if (n === 'DWGPREFIX') return dirOf(d.path) + '\\'; if (n === 'CTAB') return d.space === 'model' ? 'Model' : d.layouts[d.space].name; if (n === 'DATE') return 'NOW'; var v = TC.SV && TC.SV.def(n) ? TC.SV.get(n) : null; return v == null ? '' : String(v); }
  function edtime(fmt){ var t = new Date(), p = function(x){ return (x < 10 ? '0' : '') + x; }; return String(fmt || 'DD/MO/YYYY').replace(/YYYY|YY|MO|DD|HH|MM|SS/g, function(k){ return String({ YYYY:t.getFullYear(), YY:String(t.getFullYear()).slice(2), MO:p(t.getMonth() + 1), DD:p(t.getDate()), HH:p(t.getHours()), MM:p(t.getMinutes()), SS:p(t.getSeconds()) }[k]); }); }
  function diesel(d, expr){ var s = String(expr || ''), f, guard = 0, FN = { getvar:function(a){ return gvar(d, a[0]); }, edtime:function(a){ return edtime(a[1]); }, upper:function(a){ return String(a[0]).toUpperCase(); }, lower:function(a){ return String(a[0]).toLowerCase(); }, strlen:function(a){ return String(String(a[0]).length); } };
    while ((f = /\$\(([^$()]*)\)/.exec(s)) && guard++ < 60){ var parts = f[1].split(','), fn = FN[parts.shift().trim().toLowerCase()]; s = s.slice(0, f.index) + (fn ? fn(parts.map(function(x){ return x.trim(); })) : '$?') + s.slice(f.index + f[0].length); }
    return s; }
  var RT = { style:null, h:null, rot:0 }, DSL = ['$(getvar,dwgname)', 'Ngày in: $(edtime,$(getvar,date),DD/MO/YYYY)', 'Tỉ lệ $(getvar,cannoscale)', 'Layout: $(getvar,ctab)'];
  function txtFiles(){ return Object.keys(FS).filter(function(k){ return /\.txt$/i.test(k); }); }
  function rtPick(d, cur){
    return new Promise(function(res){
      var files = txtFiles(), sel = cur && fsKey(cur) || files[0];
      var dlg = TC.dialog({ cls:'mr2', id:'rtpick', title:'Chọn tệp chữ cho RText', cmd:'RTEXT', code:'UI-MR2', w:700, onClose:function(v){ if (v !== true) res(null); },
        body:'<div class="mr2-rtp"><div class="stack"><div class="tblwrap" tabindex="0" style="height:220px"><table class="tbl"><thead><tr><th>Tệp</th><th style="width:96px">Bảng mã</th><th class="num" style="width:58px">Dòng</th></tr></thead><tbody id="rp-rows"></tbody></table></div><button class="btn sm" type="button" id="rp-br" style="justify-self:start">' + ic('open') + 'Duyệt…</button></div>' +
          '<div class="stack"><span class="gh">Nội dung</span><pre class="mr2-code" id="rp-pv"></pre><span class="faint mr2-s mono" id="rp-path"></span></div></div>',
        foot:'<span class="faint mr2-s">RText đọc lại tệp mỗi lần tạo lại hình: sửa tệp thì chữ trong bản vẽ cập nhật.</span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="rp-ok">Chọn tệp</button>' });
      function rows(){ $('#rp-rows', dlg.el).innerHTML = files.map(function(k){ return '<tr data-f="' + esc(k) + '" class="' + (k === sel ? 'is-sel' : '') + '" tabindex="-1"><td>' + ic('file', 's12') + ' ' + esc(base(k)) + ' <span class="faint">· ' + esc(dirOf(k)) + '</span></td><td>UTF-8</td><td class="num">' + String(FS[k]).split('\n').length + '</td></tr>'; }).join(''); $('#rp-pv', dlg.el).textContent = FS[sel] || ''; $('#rp-path', dlg.el).textContent = sel || ''; }
      dlg.el.addEventListener('click', function(e){ var r = e.target.closest('[data-f]'); if (r){ sel = r.dataset.f; rows(); } });
      dlg.el.addEventListener('dblclick', function(e){ if (e.target.closest('[data-f]')) $('#rp-ok', dlg.el).click(); });
      TC.listKeys($('.tblwrap', dlg.el), '[data-f]', function(r, enter){ sel = r.dataset.f; rows(); if (enter) $('#rp-ok', dlg.el).click(); });
      $('#rp-br', dlg.el).addEventListener('click', function(){ TC.toast('Mở hộp thoại chọn tệp .txt của hệ điều hành.', { type:'info', log:false }); });
      $('#rp-ok', dlg.el).addEventListener('click', function(){ if (!sel) return; dlg.close(true); res(sel); });
      rows();
    });
  }
  // sửa RText (RTEDIT): đổi nguồn, sửa nội dung tệp (ghi thẳng ra tệp), kiểu, chiều cao, góc
  function rtEdit(ctx, e){
    var d = D(); if ((d.layerMap[e.layer] || {}).lck){ ctx.out('RText nằm trên layer bị khóa, không sửa được.'); return; }
    return new Promise(function(res){
      var o = { src:e.src || 'file', path:e.path || txtFiles()[0], expr:e.expr || DSL[0] };
      var dlg = TC.dialog({ cls:'mr2', id:'rtedit', title:'Sửa RText', cmd:'RTEXT', code:'UI-MR2', w:660, onClose:function(){ res(); },
        body:'<div class="stack">' + H.row('Nguồn', SC.seg('rt-src', ['Tệp chữ', 'Biểu thức DIESEL'], o.src === 'file' ? 'Tệp chữ' : 'Biểu thức DIESEL'), 80) +
          '<div id="rt-f" class="stack">' + H.row('Tệp', '<input class="inp mono" id="rt-path" value="' + esc(o.path) + '" readonly style="flex:1"><button class="btn sm" type="button" id="rt-pick">Chọn tệp…</button>', 80) + '<div class="field"><label for="rt-txt">Nội dung tệp</label><textarea class="inp mr2-ta" id="rt-txt" rows="5" spellcheck="false"></textarea></div><div class="note warn">' + ic('warn') + '<span>Sửa nội dung là ghi thẳng vào tệp .txt: mọi RText dùng tệp này cập nhật, Ctrl+Z không hoàn tác phần này.</span></div></div>' +
          '<div id="rt-d" class="stack">' + H.row('Biểu thức', '<input class="inp mono" id="rt-expr" value="' + esc(o.expr) + '" spellcheck="false" style="flex:1">', 80) + H.row('Kết quả', '<span class="mono" id="rt-res"></span>', 80) + H.row('Mẫu', DSL.map(function(x, i){ return '<button class="chip" type="button" data-dsl="' + i + '">' + esc(x.replace(/\$\(.*\)/, '…')) + '</button>'; }).join(' '), 80) + '</div>' +
          '<div class="grid3">' + H.row('Kiểu chữ', H.sel('rt-st', d.styles.map(function(s){ return s.name; }), e.style, 120), 60) + H.row('Chiều cao', H.num('rt-h', TC.d(e.h), { w:96 }), 60) + H.row('Góc', H.num('rt-r', TC.d(e.rot || 0, 0), { unit:'deg', prec:0, pick:false, w:80 }), 34) + '</div></div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="rt-ok">Cập nhật RText</button>' });
      function sync(){ $('#rt-f', dlg.el).hidden = o.src !== 'file'; $('#rt-d', dlg.el).hidden = o.src === 'file'; $('#rt-path', dlg.el).value = o.path; var t = fsRead(d, o.path); $('#rt-txt', dlg.el).value = t == null ? '' : t; $('#rt-txt', dlg.el).placeholder = t == null ? 'Không tìm thấy tệp. Chọn tệp khác hoặc đổi đường dẫn bằng REDIR.' : ''; $('#rt-res', dlg.el).textContent = diesel(d, $('#rt-expr', dlg.el).value); }
      dlg.el.addEventListener('segchange', function(ev){ o.src = ev.detail === 'Tệp chữ' ? 'file' : 'diesel'; sync(); });
      $('#rt-expr', dlg.el).addEventListener('input', function(){ $('#rt-res', dlg.el).textContent = diesel(d, this.value); });
      dlg.el.addEventListener('click', function(ev){ var c = ev.target.closest('[data-dsl]'); if (c){ $('#rt-expr', dlg.el).value = DSL[Number(c.dataset.dsl)]; sync(); } });
      $('#rt-pick', dlg.el).addEventListener('click', function(){ rtPick(d, o.path).then(function(p){ if (p){ o.path = p; sync(); } }); });
      $('#rt-ok', dlg.el).addEventListener('click', function(){ var h = nv(dlg, 'rt-h', 0), rot = nv(dlg, 'rt-r', 0); if (!fieldErr(dlg, 'rt-h', h > 0 ? null : 'Chiều cao phải lớn hơn 0.')) return;
        var expr = $('#rt-expr', dlg.el).value.trim(); if (o.src === 'diesel' && !expr){ TC.ctip(dlg.el, 120, 120, 'Nhập biểu thức DIESEL'); return; }
        var k = o.src === 'file' && fsKey(resolve(d, o.path)), txt = $('#rt-txt', dlg.el).value, changed = k && FS[k] !== txt;
        if (changed) FS[k] = txt;
        ED.run('RTEXT', function(){ ED.mod(e, function(x){ x.src = o.src; if (o.src === 'file'){ x.path = o.path; delete x.expr; } else { x.expr = expr; delete x.path; } x.h = h; x.rot = rot; x.style = $('#rt-st', dlg.el).value; }); });
        if (changed) allEnts(d).forEach(function(x){ if (x.t === 'rtext' && x !== e && x.path && fsKey(resolve(d, x.path)) === k) TC.prepEnt(d, x); });
        V().draw(); A().refreshPalettes(); dlg.close(true); ctx.out('Đã cập nhật RText' + (changed ? '; đã lưu nội dung vào ' + lbl(base(k)) + '.' : '.')); });
      sync();
    });
  }
  IMPL.RTEXT = function(ctx, pre){
    var d = D(); ctx.badge('RT'); if (pre && pre.t === 'rtext') return rtEdit(ctx, pre);
    var c = ED.cur(d), st = RT.style || c.tstyle || 'Standard', h = RT.h || c.th || 250;
    ctx.out('Thiết lập hiện tại: Kiểu chữ = ' + esc(st) + ', Chiều cao = ' + TC.d(h) + ', Góc = ' + TC.d(RT.rot, 0) + '°', 'faint');
    var place = function(o){ var mk = function(p){ return Object.assign(ED.make({ t:'rtext', p:p, h:h, style:st, rot:RT.rot }), o); };
      return ctx.point('Điểm đầu của RText', { onMove:function(p, temp){ temp.push(prev(mk(p))); } }).then(function(r){ if (!r.p) return; var e; ED.run('RTEXT', function(){ e = ED.add(mk(r.p)); }); RT.style = st; RT.h = h;
        ctx.out('Đã chèn RText ' + (o.src === 'file' ? 'từ ' + lbl(base(o.path)) + ' (' + qty(rtLines(d, e).length, 'dòng') + ')' : 'biểu thức ' + lbl(o.expr) + ' = “' + esc(diesel(d, o.expr)) + '”') + '. Nhấp đúp để sửa nguồn.'); }); };
    return (function menu(){ return ctx.kw('Nguồn chữ hoặc', [['Tệp', 'F'], ['Biểu thức DIESEL', 'D'], ['Kiểu chữ', 'S'], ['Chiều cao', 'H'], ['Góc', 'R']], 'F').then(function(k){
      if (k === 'S') return ctx.val('Kiểu chữ', st).then(function(v){ if (d.styleMap[v]) st = v; else ctx.out('Không có kiểu chữ “' + esc(v) + '”.'); return menu(); });
      if (k === 'H') return ctx.val('Chiều cao', TC.d(h)).then(function(v){ var n = num(v, 0); if (n > 0) h = n; else ctx.out('Chiều cao phải lớn hơn 0.'); return menu(); });
      if (k === 'R') return ctx.val('Góc xoay', TC.d(RT.rot, 0)).then(function(v){ var n = num(v, NaN); if (isFinite(n)) RT.rot = n; return menu(); });
      if (k === 'D') return ctx.val('Biểu thức DIESEL', DSL[0], { allowSpace:true }).then(function(x){ x = String(x).trim(); if (!/\$\(/.test(x)){ ctx.out('Biểu thức DIESEL bắt đầu bằng $(, ví dụ $(getvar,dwgname).'); return menu(); } return place({ src:'diesel', expr:x }); });
      if (k !== 'F'){ ctx.out('Chọn F, D, S, H hoặc R.'); return menu(); }
      return rtPick(d).then(function(p){ if (p) return place({ src:'file', path:p }); }); }); })();
  };

  /* ================= Sửa: MOCORO, MSTRETCH ================= */
  // MOCORO: một điểm gốc, lần lượt dời, xoay, đổi tỉ lệ; Sao chép · C bật chế độ tạo bản sao; cả lệnh là một bước hoàn tác
  IMPL.MOCORO = function(ctx){
    var d = D(); ctx.badge('MC');
    return selEdit(ctx).then(function(ids){ if (!ids.length) return;
      return ctx.point('Điểm gốc').then(function(r0){ if (!r0.p) return; var base = r0.p, copy = false, marks = [], cur = ids.slice(), cnt = { mv:0, ro:0, sc:0, cp:0 };
        var apply = function(t, what){ marks.push({ mk:ED.mark(), cur:cur.slice(), base:base, what:copy ? 'cp' : what }); var out = ED.apply(cur, t, copy); if (!copy) cur = out; cnt[copy ? 'cp' : what]++; V().draw(); };
        var refR = function(){ var b = ED.bbox(cur); return Math.max(1, Math.hypot(b[2] - b[0], b[3] - b[1]) / 2); };
        var rot = function(){ return ctx.point('Góc xoay hoặc', { base:base, num:true, dyn:'ang', opts:[['Tham chiếu', 'R']], onMove:function(p, temp){ temp.push(ED.xprev(cur, T.rot(base, G.ang(base, p)))); } }).then(function(r){
          if (r.num != null) return apply(T.rot(base, r.num), 'ro'); if (r.p) return apply(T.rot(base, G.ang(base, r.p)), 'ro');
          if (r.kw === 'R') return ctx.val('Góc tham chiếu', '0').then(function(a){ return ctx.val('Góc mới', '90').then(function(b){ apply(T.rot(base, num(b, 0) - num(a, 0)), 'ro'); }); }); }); };
        var scl = function(){ var ref = refR(); return ctx.point('Hệ số tỉ lệ hoặc', { base:base, num:true, opts:[['Tham chiếu', 'R']], onMove:function(p, temp){ temp.push(ED.xprev(cur, T.sc(base, Math.max(1e-3, G.dist(base, p) / ref)))); } }).then(function(r){
          if (r.kw === 'R') return ctx.val('Độ dài tham chiếu', TC.d(ref)).then(function(a){ return ctx.val('Độ dài mới', TC.d(ref)).then(function(b){ var A1 = num(a, 0), B1 = num(b, 0); if (A1 > 0 && B1 > 0) apply(T.sc(base, B1 / A1), 'sc'); else ctx.out('Độ dài phải lớn hơn 0.'); }); });
          var k = r.num != null ? r.num : r.p ? G.dist(base, r.p) / ref : null; if (k > 0) apply(T.sc(base, k), 'sc'); else if (k != null) ctx.out('Hệ số tỉ lệ phải lớn hơn 0.'); }); };
        var loop = function(){ return ctx.point('Điểm thứ hai hoặc', { base:base, def:'thoát', opts:[[copy ? 'Di chuyển' : 'Sao chép', 'C'], ['Điểm gốc', 'B'], ['Xoay', 'R'], ['Tỉ lệ', 'S'], ['Hoàn tác', 'U']], onMove:function(p, temp){ temp.push(ED.xprev(cur, T.move(p[0] - base[0], p[1] - base[1]))); } }).then(function(r){
          if (r.kw === 'C'){ copy = !copy; ctx.out(copy ? 'Chế độ sao chép: mỗi điểm, góc xoay hoặc hệ số tạo một bản sao; bản gốc giữ nguyên.' : 'Chế độ di chuyển.', 'faint'); return loop(); }
          if (r.kw === 'B') return ctx.point('Điểm gốc mới').then(function(q){ if (q.p) base = q.p; return loop(); });
          if (r.kw === 'R') return rot().then(loop);
          if (r.kw === 'S') return scl().then(loop);
          if (r.kw === 'U'){ var m = marks.pop(); if (m){ ED.back(m.mk); cur = m.cur; base = m.base; cnt[m.what]--; V().draw(); } else ctx.out('Không còn bước để hoàn tác.'); return loop(); }
          if (r.p){ apply(T.move(r.p[0] - base[0], r.p[1] - base[1]), 'mv'); if (!copy) base = r.p; return loop(); } }); };
        return ED.run('MOCORO', loop).then(function(){ TC.cad.clearSel(); var s = [cnt.mv ? cnt.mv + ' lần dời' : '', cnt.ro ? cnt.ro + ' lần xoay' : '', cnt.sc ? cnt.sc + ' lần đổi tỉ lệ' : '', cnt.cp ? qty(cnt.cp, 'bản sao') : ''].filter(Boolean);
          ctx.out(s.length ? 'MOCORO: ' + s.join(', ') + ' trên ' + qty(ids.length, 'đối tượng') + '. Ctrl+Z hoàn tác cả lệnh.' : 'Không có thay đổi.'); }); }); });
  };

  // MSTRETCH: nhiều khung, đa giác Crossing; đỉnh nằm trong khung thì dời, đối tượng nằm trọn thì di chuyển
  function inAny(wins, p){ return wins.some(function(w){ return G.inPoly(p, w); }); }
  function crossIds(d, wins){ var out = [], edges = [], wbb = wins.map(bbOf);
    wins.forEach(function(w){ w.forEach(function(p, i){ edges.push([p, w[(i + 1) % w.length]]); }); });
    ED.spaceList(d).forEach(function(e){ if (!TC.entVisible(d, e) || e.t === 'vpframe') return; var b = e._bb; if (!b || !wbb.some(function(q){ return !(b[2] < q[0] || b[0] > q[2] || b[3] < q[1] || b[1] > q[3]); })) return;
      var hit = e._p.some(function(pr){ if (pr.k === 'seg') return inAny(wins, pr.a) || inAny(wins, pr.b) || edges.some(function(ed){ return G.segInt(pr.a, pr.b, ed[0], ed[1]); });
        if (pr.k === 'arc' || pr.k === 'circ'){ var sw = arcSw(pr), a0 = pr.k === 'circ' ? 0 : pr.a0; for (var i = 0; i <= 16; i++) if (inAny(wins, polar(pr.c, pr.r, a0 + sw * i / 16))) return true; return false; }
        if (pr.k === 'text' || pr.k === 'pt' || pr.k === 'tick' || pr.k === 'ins') return inAny(wins, pr.p); if (pr.k === 'ell') return inAny(wins, pr.c);
        if (pr.k === 'hatch') return pr.loops.some(function(l){ return l.some(function(p){ return inAny(wins, p); }); }); if (pr.k === 'img' || pr.k === 'tri' || pr.k === 'tile') return pr.pts.some(function(p){ return inAny(wins, p); }); return false; });
      if (hit) out.push(e.id); });
    return out; }
  function mst(x, wins, dx, dy){ var mv = function(p){ return inAny(wins, p) ? [p[0] + dx, p[1] + dy] : p; };
    switch (x.t){
      case 'line': x.p1 = mv(x.p1); x.p2 = mv(x.p2); break;
      case 'pline': case 'spline': case 'mleader': case 'qleader': case 'mline': x.pts = x.pts.map(mv); if (x.atp) x.atp = mv(x.atp); break;
      case 'hatch': case 'region': case 'wipeout': case 'gradient': case 'shatch': x.loops = x.loops.map(function(l){ return l.map(mv); }); break;
      case 'dim': { var a = inAny(wins, x.p1), b = inAny(wins, x.p2); if (a && b) ED.xform(x, T.move(dx, dy)); else { x.p1 = mv(x.p1); x.p2 = mv(x.p2); } break; }
      default: { var k = x.p || x.c || (x.pts && x.pts[0]) || (x.loops && x.loops[0][0]); if (k && inAny(wins, k)) ED.xform(x, T.move(dx, dy)); }
    }
    return x; }
  IMPL.MSTRETCH = function(ctx){
    var d = D(), wins = []; ctx.badge('MS'); TC.cad.clearSel();
    var wp = function(pts, closed){ return { k:'poly', pts:pts, closed:closed !== false, color:TC.cssv('--c-sel-c'), fill:TC.cssv('--c-sel-cf'), dash:[6, 4], w:1 }; }, wt = function(){ return wins.map(function(w){ return wp(w); }); }, rect = function(a, b){ return [[a[0], a[1]], [b[0], a[1]], [b[0], b[1]], [a[0], b[1]]]; };
    function next(){ var n = wins.length; return ctx.point(n ? 'Góc thứ nhất của khung Crossing tiếp theo hoặc' : 'Góc thứ nhất của khung Crossing hoặc', { temp:wt(), rubber:false, def:n ? 'xong' : null, opts:n ? [['Đa giác Crossing', 'CP'], ['Hoàn tác', 'U']] : [['Đa giác Crossing', 'CP']] }).then(function(r){
      if (r.kw === 'U'){ wins.pop(); return next(); } if (r.kw === 'CP') return poly([]);
      if (r.p) return ctx.point('Góc đối diện', { base:r.p, rubber:false, temp:wt(), onMove:function(p, temp){ temp.push(wp(rect(r.p, p))); } }).then(function(q){ if (q.p && Math.abs(q.p[0] - r.p[0]) > 1e-6 && Math.abs(q.p[1] - r.p[1]) > 1e-6) wins.push(rect(r.p, q.p)); return next(); });
      if (!n){ ctx.out('Cần ít nhất một khung Crossing.'); return next(); }
      var ids = ED.editable(crossIds(d, wins), ctx); if (!ids.length){ ctx.out('Không có đối tượng nào cắt qua ' + qty(n, 'khung') + '.'); return; }
      d.sel = ids; TC.cad.afterSel(); ctx.out(qty(ids.length, 'đối tượng') + ' cắt qua ' + qty(n, 'khung') + '.', 'faint'); return baseStep(); }); }
    function poly(pts){ return ctx.point(pts.length ? 'Điểm tiếp theo của đa giác Crossing' : 'Điểm đầu của đa giác Crossing', { base:pts[pts.length - 1], temp:wt(), rubber:false, def:pts.length > 2 ? 'đóng đa giác' : null, opts:pts.length ? [['Hoàn tác', 'U']] : null, onMove:function(p, temp){ if (pts.length) temp.push(wp(pts.concat([p]), pts.length > 1)); } }).then(function(r){
      if (r.kw === 'U'){ pts.pop(); return poly(pts); } if (r.p){ pts.push(r.p); return poly(pts); } if (pts.length > 2) wins.push(pts); else if (pts.length) ctx.out('Đa giác Crossing cần ít nhất 3 điểm.'); return next(); }); }
    function rem(){ return ctx.point('Chọn đối tượng cần bớt (Enter để xong)', { pick:true, dyn:false, temp:wt() }).then(function(r){ if (!r.p) return; var s = r.s || V().mToS(r.p) || V().toS(r.p), id = V().hitTest(s[0], s[1]);
      if (id && d.sel.indexOf(id) >= 0){ d.sel = d.sel.filter(function(x){ return x !== id; }); TC.cad.afterSel(); ctx.out('Còn ' + qty(d.sel.length, 'đối tượng') + '.', 'faint'); } else ctip(r, 'Đối tượng này không nằm trong vùng chọn'); return rem(); }); }
    function baseStep(){ return ctx.point('Điểm gốc hoặc', { temp:wt(), opts:[['Bớt đối tượng', 'R']] }).then(function(r){
      if (r.kw === 'R') return rem().then(function(){ if (d.sel.length) return baseStep(); ctx.out('Không còn đối tượng nào để kéo giãn.'); });
      if (!r.p) return; var b = r.p, ids = d.sel.slice();
      return ctx.point('Điểm thứ hai', { base:b, temp:wt(), onMove:function(p, temp){ temp.push(ED.temp(ids.slice(0, 300).map(function(id){ return mst(ED.clean(d.byId[id]), wins, p[0] - b[0], p[1] - b[1]); }))); } }).then(function(q){ if (!q.p) return; var dx = q.p[0] - b[0], dy = q.p[1] - b[1];
        return ED.run('MSTRETCH', function(){ ids.forEach(function(id){ var e = d.byId[id]; if (e) ED.mod(e, function(x){ mst(x, wins, dx, dy); }); }); TC.cad.clearSel(); ctx.out('Đã kéo giãn ' + qty(ids.length, 'đối tượng') + ' qua ' + qty(wins.length, 'khung') + ', dời ' + TC.d(Math.hypot(dx, dy)) + '.'); }); }); }); }
    return next();
  };

  /* ================= Vẽ: BREAKLINE, SUPERHATCH, MKSHAPE, MKLTYPE ================= */
  var BK = Object.assign({ sym:'Z', size:0, ext:0 }, TC.load('mr2bk', {})), BKN = { Z:'zic zắc', S:'lượn sóng' };
  function annoK(d){ var m = /^(\d+(?:\.\d+)?):(\d+(?:\.\d+)?)$/.exec(String(d.annoScale || '1:100')); return m ? Number(m[2]) / Number(m[1]) : 100; }
  function bkSize(d){ return BK.size || 5 * annoK(d); }
  // đường ngắt: polyline đi qua hai điểm, ký hiệu ngắt ở vị trí chọn (mặc định trung điểm), kéo dài hai đầu
  function bkMake(a, b, m, z, ext, sym){ var u = unit(sub(b, a)), n = [-u[1], u[0]], L = G.dist(a, b), t = TC.clamp(dot(sub(m || [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2], a), u), z / 2, L - z / 2), c = add2(a, u, t), P = function(s, h){ return add2(add2(c, u, s * z), n, h * z); }, A1 = add2(a, u, -ext), B1 = add2(b, u, ext);
    if (sym === 'S') return { t:'pline', pts:[A1, P(-.5, 0), P(0, 0), P(.5, 0), B1], bulges:[0, .9, -.9, 0] };
    return { t:'pline', pts:[A1, P(-.5, 0), P(-.17, .62), P(.17, -.62), P(.5, 0), B1], bulges:[] }; }
  IMPL.BREAKLINE = function(ctx){
    var d = D(); ctx.badge('BL');
    ctx.out('Ký hiệu = ' + BKN[BK.sym] + ', Cỡ = ' + TC.d(bkSize(d)) + (BK.size ? '' : ' (5 mm giấy × tỉ lệ 1:' + kFmt(annoK(d)) + ')') + ', Kéo dài = ' + TC.d(BK.ext), 'faint');
    return (function first(){ return ctx.point('Điểm đầu đường ngắt hoặc', { opts:[['Ký hiệu', 'B'], ['Cỡ', 'S'], ['Kéo dài', 'E']] }).then(function(r){
      if (r.kw === 'B') return ctx.kw('Ký hiệu ngắt', [['Zic zắc', 'Z'], ['Lượn sóng', 'S']], BK.sym).then(function(k){ if (BKN[k]){ BK.sym = k; TC.save('mr2bk', BK); } return first(); });
      if (r.kw === 'S') return ctx.val('Cỡ ký hiệu (đơn vị bản vẽ)', TC.d(bkSize(d))).then(function(v){ var n = num(v, 0); if (n > 0){ BK.size = n; TC.save('mr2bk', BK); } else ctx.out('Cỡ ký hiệu phải lớn hơn 0.'); return first(); });
      if (r.kw === 'E') return ctx.val('Độ kéo dài qua hai điểm', TC.d(BK.ext)).then(function(v){ var n = num(v, -1); if (n >= 0){ BK.ext = n; TC.save('mr2bk', BK); } else ctx.out('Độ kéo dài không được âm.'); return first(); });
      if (!r.p) return; var a = r.p, z = bkSize(d);
      return ctx.point('Điểm cuối đường ngắt', { base:a, onMove:function(p, temp){ if (G.dist(a, p) > z) temp.push(prev(ED.make(bkMake(a, p, null, z, BK.ext, BK.sym)))); } }).then(function(q){ if (!q.p) return; var b = q.p;
        if (G.dist(a, b) < z * 1.2){ ctx.out('Đường ngắt ngắn hơn ký hiệu (cỡ ' + TC.d(z) + '). Chọn điểm xa hơn hoặc giảm cỡ bằng tùy chọn Cỡ · S.'); return first(); }
        return ctx.point('Vị trí ký hiệu ngắt', { def:'trung điểm', rubber:false, onMove:function(p, temp){ temp.push(prev(ED.make(bkMake(a, b, p, z, BK.ext, BK.sym)))); } }).then(function(m){
          ED.run('BREAKLINE', function(){ ED.add(ED.make(bkMake(a, b, m.p || null, z, BK.ext, BK.sym))); }); ctx.out('Đã vẽ đường ngắt dài ' + TC.d(G.dist(a, b) + 2 * BK.ext) + ', ký hiệu ' + BKN[BK.sym] + ' cỡ ' + TC.d(z) + ' (polyline).'); }); }); }); })();
  };

  var SH = Object.assign({ kind:'image', name:'gach-the-300.png', tw:300, th:300, ang:0, gap:0, stag:false, sc:1, fade:40 }, TC.load('mr2sh', {}));
  function shSources(d, kind){ if (kind === 'image') return Object.keys(MAT).map(function(k){ return { name:k, label:MAT[k].n }; }); if (kind === 'wipe') return []; return Object.keys(d.blocks).filter(function(n){ var b = d.blocks[n]; return !/^[*]|^A\$|^KHUNG/.test(n) && (kind === 'xref' ? !!b.xref : !b.xref); }).map(function(n){ return { name:n, label:d.blocks[n].desc || n }; }); }
  function shDesc(o){ return o.kind === 'image' ? 'ảnh ' + ((MAT[o.name] || {}).n || o.name).toLowerCase() : o.kind === 'wipe' ? 'vùng che' : o.kind + ' ' + o.name + ' tỉ lệ ' + TC.d(o.sc, 2); }
  function loopsArea(L){ return Math.abs(L.reduce(function(s, l, i){ return s + (i ? -1 : 1) * G.polyArea(l); }, 0)); }
  function shDlg(d, o0, edit){
    return new Promise(function(res){
      var o = Object.assign({}, SH, o0 || {}), K = [['image', 'Ảnh'], ['block', 'Block'], ['xref', 'Xref'], ['wipe', 'Vùng che']];
      var seg = function(n, list, cur){ return '<span class="seg" role="group" data-seg="' + n + '">' + list.map(function(x){ return '<button type="button" data-v="' + x[0] + '" aria-pressed="' + (x[0] === cur) + '">' + x[1] + '</button>'; }).join('') + '</span>'; };
      var dlg = TC.dialog({ cls:'mr2', id:'shatch', title:edit ? 'Sửa SuperHatch' : 'SuperHatch', cmd:'SUPERHATCH', code:'UI-MR2', w:800, onClose:function(v){ if (v !== true) res(null); },
        body:'<div class="mr2-sh"><div class="stack">' + seg('sh-k', K, o.kind) + '<div class="mr2-src" id="sh-list" role="listbox" tabindex="0" aria-label="Nguồn tô"></div></div>' +
          '<div class="stack"><div class="grp" id="sh-set"><div class="gh">Ô lát</div><div id="sh-img" class="stack" style="gap:6px">' + H.row('Rộng ô', H.num('sh-w', TC.d(o.tw), { w:110 }), 84) + H.row('Cao ô', H.num('sh-h', TC.d(o.th), { w:110 }), 84) + H.row('Độ mờ ảnh', H.num('sh-f', String(o.fade || 0), { unit:'none', prec:0, step:10, pick:false, w:110 }) + '<span class="faint mr2-s">%</span>', 84) + '</div><div id="sh-blk">' + H.row('Tỉ lệ', H.num('sh-sc', TC.d(o.sc, 3), { unit:'none', prec:3, step:.1, pick:false, w:110 }), 84) + '</div>' +
            H.row('Góc', H.num('sh-a', TC.d(o.ang, 0), { unit:'deg', prec:0, pick:false, w:110 }), 84) + H.row('Khe giữa ô', H.num('sh-g', TC.d(o.gap), { w:110 }), 84) + H.row('Kiểu xếp', seg('sh-st', [['0', 'Thẳng hàng'], ['1', 'So le']], o.stag ? '1' : '0'), 84) + '</div>' +
            '<div id="sh-wipe" class="note">' + ic('info') + '<span>Vùng che tô bằng màu nền vùng vẽ, in ra giấy trắng; che mọi đối tượng nằm dưới. Không có ô lát.</span></div>' +
            '<canvas id="sh-pv" width="300" height="140" class="pvc"></canvas><div class="mr2-s faint" id="sh-info"></div></div></div>',
        foot:'<span class="faint mr2-s">' + (edit ? 'Kéo grip tròn giữa vùng để dời gốc lưới ô.' : 'Bước sau: chỉ điểm bên trong từng vùng cần tô.') + '</span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="sh-ok">' + (edit ? 'Cập nhật SuperHatch' : 'Chọn vùng tô') + '</button>' });
      function list(){ var L = shSources(d, o.kind), host = $('#sh-list', dlg.el); if (L.length && !L.some(function(x){ return x.name === o.name; })){ o.name = L[0].name; if (o.kind === 'image'){ o.tw = MAT[o.name].w; o.th = MAT[o.name].h; } }
        host.innerHTML = o.kind === 'wipe' ? '<div class="empty">' + ic('bgmask') + '<b>Vùng che</b>Không cần chọn nguồn.</div>' : !L.length ? '<div class="empty">' + ic(o.kind === 'xref' ? 'xref' : 'insert') + '<b>' + (o.kind === 'xref' ? 'Bản vẽ chưa gắn xref' : 'Bản vẽ chưa có block') + '</b>' + (o.kind === 'xref' ? 'Gắn bằng XATTACH hoặc BLOCKTOXREF rồi mở lại SuperHatch.' : 'Tạo block bằng BLOCK.') + '</div>' :
          L.map(function(x){ return '<button type="button" role="option" class="mr2-srci" data-src="' + esc(x.name) + '" aria-selected="' + (x.name === o.name) + '"><canvas width="64" height="44"></canvas><span><b>' + esc(o.kind === 'image' ? x.label : x.name) + '</b><span class="faint">' + esc(o.kind === 'image' ? x.name : x.label) + '</span></span></button>'; }).join('');
        $$('[data-src]', host).forEach(function(b){ var c = $('canvas', b), x = c.getContext('2d'); if (o.kind === 'image'){ x.fillStyle = TC.cssv('--c-canvas'); x.fillRect(0, 0, 64, 44); var M = MAT[b.dataset.src]; for (var i = 0; i < 2; i++) for (var j = 0; j < 2; j++) paintTile(x, M, [[2 + i * 30, 42 - j * 20], [32 + i * 30, 42 - j * 20], [32 + i * 30, 22 - j * 20], [2 + i * 30, 22 - j * 20]], i * 2 + j, true); } else pvDraw(c, d, [{ t:'insert', name:b.dataset.src, p:[0, 0], sx:1, sy:1, rot:0, layer:'0' }], 8); });
        $('#sh-img', dlg.el).hidden = o.kind !== 'image'; $('#sh-blk', dlg.el).hidden = o.kind === 'image' || o.kind === 'wipe'; $('#sh-set', dlg.el).hidden = o.kind === 'wipe'; $('#sh-wipe', dlg.el).hidden = o.kind !== 'wipe'; $('#sh-ok', dlg.el).disabled = o.kind !== 'wipe' && !L.length; if (o.kind === 'image'){ $('#sh-w', dlg.el).value = TC.d(o.tw); $('#sh-h', dlg.el).value = TC.d(o.th); } paint(); }
      function read(){ o.tw = nv(dlg, 'sh-w', o.tw); o.th = nv(dlg, 'sh-h', o.th); o.fade = TC.clamp(Math.round(nv(dlg, 'sh-f', o.fade)), 0, 90); o.sc = nv(dlg, 'sh-sc', o.sc); o.ang = nv(dlg, 'sh-a', o.ang); o.gap = nv(dlg, 'sh-g', o.gap); }
      function paint(){ read(); var c = $('#sh-pv', dlg.el), x = c.getContext('2d'), ok = o.kind === 'wipe' || shSources(d, o.kind).length; x.fillStyle = TC.cssv('--c-canvas'); x.fillRect(0, 0, c.width, c.height); if (!ok){ $('#sh-info', dlg.el).textContent = ''; return; }
        var sz = o.kind === 'image' ? [o.tw, o.th] : o.kind === 'wipe' ? [1000, 1000] : shSize(d, { src:{ kind:o.kind, name:o.name, sc:o.sc } }) || [1000, 1000], W = Math.max(sz[0] + (o.gap || 0), sz[1] + (o.gap || 0)) * 5, Hh = W * 140 / 300, L = [[0, 0], [W, 0], [W, Hh], [W * .3, Hh], [0, Hh * .6]];
        pvDraw(c, d, [{ t:'shatch', loops:[L], src:{ kind:o.kind, name:o.name, sc:o.sc }, tw:o.tw, th:o.th, ang:o.ang, gap:o.gap, stag:o.stag, fade:o.fade, org:[0, 0], layer:'0' }, { t:'pline', pts:L, closed:true, layer:'KT-TUONG' }], 10, [0, 0, W, Hh]);
        $('#sh-info', dlg.el).textContent = o.kind === 'wipe' ? 'Vùng che, không có ô lát.' : 'Ô ' + TC.d(sz[0], 0) + ' × ' + TC.d(sz[1], 0) + ' mm' + (o.gap ? ' · khe ' + TC.d(o.gap, 0) : '') + (o.ang ? ' · góc ' + TC.d(o.ang, 0) + '°' : '') + (o.stag ? ' · so le' : '') + (o.kind === 'image' && o.fade ? ' · mờ ' + o.fade + ' %' : '') + ' · hình mẫu cạnh ' + TC.d(W / 1000, 1) + ' m'; }
      dlg.el.addEventListener('click', function(ev){ var b = ev.target.closest('[data-src]'); if (!b) return; o.name = b.dataset.src; if (o.kind === 'image'){ o.tw = MAT[o.name].w; o.th = MAT[o.name].h; } $$('[data-src]', dlg.el).forEach(function(x){ x.setAttribute('aria-selected', String(x === b)); }); if (o.kind === 'image'){ $('#sh-w', dlg.el).value = TC.d(o.tw); $('#sh-h', dlg.el).value = TC.d(o.th); } paint(); });
      dlg.el.addEventListener('dblclick', function(ev){ if (ev.target.closest('[data-src]')) $('#sh-ok', dlg.el).click(); });
      TC.listKeys($('#sh-list', dlg.el), '[data-src]', function(b, enter){ if (enter) $('#sh-ok', dlg.el).click(); else b.click(); });
      dlg.el.addEventListener('segchange', function(ev){ var g = ev.target.closest('[data-seg]').dataset.seg; if (g === 'sh-k'){ read(); o.kind = ev.detail; list(); } else { o.stag = ev.detail === '1'; paint(); } });
      dlg.el.addEventListener('numchange', paint);
      $('#sh-ok', dlg.el).addEventListener('click', function(){ read();
        if (o.kind !== 'wipe' && !shSources(d, o.kind).length){ TC.ctip(dlg.el, 160, 120, 'Chưa có nguồn để tô. Chọn loại khác.'); return; }
        var ok = o.kind !== 'image' || (fieldErr(dlg, 'sh-w', o.tw > 0 ? null : 'Phải lớn hơn 0.') & fieldErr(dlg, 'sh-h', o.th > 0 ? null : 'Phải lớn hơn 0.')); ok = ok & fieldErr(dlg, 'sh-sc', o.kind === 'image' || o.kind === 'wipe' || o.sc > 0 ? null : 'Phải lớn hơn 0.') & fieldErr(dlg, 'sh-g', o.gap >= 0 ? null : 'Không được âm.');
        if (!ok) return; dlg.close(true); res(o); });
      list();
    });
  }
  function entLoops(e){ if (e.t === 'pline' && e.closed){ var out = []; e._p.forEach(function(pr){ if (pr.k === 'seg') out.push(pr.a); else if (pr.k === 'arc'){ var sw = arcSw(pr); for (var i = 0; i < 12; i++) out.push(polar(pr.c, pr.r, pr.a0 + sw * i / 12)); } }); return [out]; }
    if (e.t === 'circle') return [Array.from({ length:48 }, function(_, i){ return polar(e.c, e.r, i * 7.5); })]; if (e.t === 'region' || e.t === 'wipeout' || e.t === 'hatch') return e.loops; return null; }
  IMPL.SUPERHATCH = function(ctx, pre){
    var d = D(); ctx.badge('SH');
    if (pre && pre.t === 'shatch'){ if ((d.layerMap[pre.layer] || {}).lck){ ctx.out('SuperHatch nằm trên layer bị khóa, không sửa được.'); return; }
      return shDlg(d, { kind:pre.src.kind, name:pre.src.name, sc:pre.src.sc || 1, tw:pre.tw, th:pre.th, ang:pre.ang || 0, gap:pre.gap || 0, stag:!!pre.stag, fade:pre.fade || 0 }, true).then(function(o){ if (!o) return;
        ED.run('SUPERHATCH', function(){ ED.mod(pre, function(x){ x.src = { kind:o.kind, name:o.name, sc:o.sc }; x.tw = o.tw; x.th = o.th; x.ang = o.ang; x.gap = o.gap; x.stag = o.stag; if (o.kind === 'image') x.fade = o.fade; else delete x.fade; }); }); V().draw(); A().refreshPalettes(); ctx.out('Đã cập nhật SuperHatch: ' + shDesc(o) + '.'); }); }
    TC.cad.clearSel();
    return shDlg(d, null, false).then(function(o){ if (!o) return; SH = o; TC.save('mr2sh', SH); var n = 0, area = 0, used = {};
      var mk = function(loops){ var e = ED.make({ t:'shatch', loops:loops.map(function(l){ return l.slice(); }), src:{ kind:o.kind, name:o.name, sc:o.sc }, tw:o.tw, th:o.th, ang:o.ang, gap:o.gap, stag:o.stag }); if (o.kind === 'image') e.fade = o.fade; return e; };
      var put = function(loops, r){ var t = mk(loops); if (o.kind !== 'wipe'){ var T0 = shTiles(d, t); if (T0.over){ ctip(r, 'Vùng cần ' + TC.n(T0.n) + ' ô, quá nhiều để hiển thị. Tăng kích thước ô (T).'); return; } } var e = ED.add(t); ED.order(e, 0); n++; area += loopsArea(loops); used[shDesc(o)] = 1; V().draw(); };
      function loop(){ return ctx.point('Chỉ điểm bên trong vùng cần tô hoặc', { pick:true, dyn:false, def:n ? 'xong' : null, opts:[['Chọn đối tượng', 'S'], ['Thiết lập', 'T']], onMove:function(p, temp){ var g = ED.regionAt(p); if (g){ temp.push(prev(mk(g.loops))); temp.push({ k:'poly', pts:g.outer, closed:true, color:TC.cssv('--c-accent'), w:1.6 }); } } }).then(function(r){
        if (r.kw === 'T') return shDlg(d, o, false).then(function(o2){ if (o2){ o = o2; SH = o; TC.save('mr2sh', SH); } return loop(); });
        if (r.kw === 'S') return ctx.sel('Chọn polyline kín, đường tròn hoặc region làm biên').then(function(ids){ var k = 0; ents(ids).forEach(function(e){ var L = entLoops(e); if (L){ put(L, r); k++; } }); TC.cad.clearSel(); if (!k) ctx.out('Không có polyline kín, đường tròn hoặc region trong vùng chọn.'); return loop(); });
        if (!r.p) return; var g = ED.regionAt(r.p); if (!g){ ctip(r, 'Không tìm thấy vùng kín tại đây. Dùng Chọn đối tượng · S'); return loop(); }
        put(g.loops, r); return loop(); }); }
      return ED.run('SUPERHATCH', loop).then(function(){ if (n) ctx.out('Đã tô ' + qty(n, 'vùng') + ' bằng ' + Object.keys(used).join(', ') + ', tổng ' + TC.d(area / 1e6) + ' m². Nhấp đúp để sửa; kéo grip tròn để dời gốc lưới.'); }); });
  };

  // MKSHAPE: nét đã chọn → vector chuẩn hóa theo cạnh lớn nhất, làm tròn theo độ phân giải; nạp ngay vào bản vẽ
  var MKS = { file:'trion-shapes', lin:'trion' }, LASTSHP = null;
  function entSegs(es){ var out = []; es.forEach(function(e){ (e._p || []).forEach(function(pr){ if (pr.k === 'seg') out.push([pr.a, pr.b]); else if (pr.k === 'arc' || pr.k === 'circ'){ var sw = arcSw(pr), a0 = pr.k === 'circ' ? 0 : pr.a0, n = Math.max(4, Math.ceil(sw / 15)); for (var i = 0; i < n; i++) out.push([polar(pr.c, pr.r, a0 + sw * i / n), polar(pr.c, pr.r, a0 + sw * (i + 1) / n)]); } else if (pr.k === 'ell'){ var f = function(t){ return add2(pr.c, rot2([pr.rx * Math.cos(rad(t)), pr.ry * Math.sin(rad(t))], pr.rot || 0)); }; for (var j = 0; j < 24; j++) out.push([f(15 * j), f(15 * (j + 1))]); } }); }); return out; }
  function shapePlace(ctx, nm, h0){
    var h = h0 || 300, n = 0, mk = function(p, hh, rr){ return ED.make({ t:'shape', name:nm, p:p, h:hh, rot:rr || 0 }); };
    return ED.run('SHAPE', function again(){ return ctx.point('Điểm chèn shape ' + nm, { def:n ? 'xong' : null, onMove:function(p, temp){ temp.push(prev(mk(p, h))); } }).then(function(r){ if (!r.p) return;
      return ctx.point('Chiều cao', { base:r.p, num:true, def:TC.d(h), onMove:function(p, temp){ temp.push(prev(mk(r.p, Math.max(1, G.dist(r.p, p))))); } }).then(function(q){ if (q.num > 0) h = q.num; else if (q.p) h = Math.max(1, G.dist(r.p, q.p));
        return ctx.point('Góc xoay', { base:r.p, num:true, dyn:'ang', def:'0', onMove:function(p, temp){ temp.push(prev(mk(r.p, h, G.ang(r.p, p)))); } }).then(function(a){ ED.add(mk(r.p, h, a.num != null ? a.num : a.p ? G.ang(r.p, a.p) : 0)); n++; V().draw(); return again(); }); }); }); })
      .then(function(){ if (n) ctx.out('Đã chèn ' + qty(n, 'shape') + ' ' + nm + '. Đổi thành block bằng SHP2BLK.'); });
  }
  IMPL.SHAPE = function(ctx){
    var d = D(), L = shapeNames(d); ctx.badge('SHP'); if (!L.length){ ctx.out('Chưa nạp tệp shape nào. Tạo shape bằng MKSHAPE.'); return; }
    return ctx.val('Tên shape hoặc [?]', LASTSHP || L[0][0]).then(function(v){ v = String(v).trim().toUpperCase();
      if (v === '?'){ ctx.out('Shape đã nạp: ' + L.map(function(x){ return '<b>' + esc(x[0]) + '</b> <span class="faint">(' + esc(x[1]) + ')</span>'; }).join(', ') + '.'); return IMPL.SHAPE(ctx); }
      var sd = shapeDef(d, v); if (!sd){ ctx.out('Không có shape “' + esc(v) + '”. Gõ ? để xem danh sách.'); return; } LASTSHP = v; return shapePlace(ctx, v, sd.H || 300); });
  };
  IMPL.MKSHAPE = function(ctx){
    var d = D(); ctx.badge('MKS');
    return ctx.val('Tên tệp shape (.shx)', MKS.file).then(function(f){ f = String(f).trim().replace(/\.(shx|shp)$/i, ''); if (!/^[\w\-]+$/.test(f)){ ctx.out('Tên tệp chỉ gồm chữ không dấu, số và gạch nối.'); return; }
      return ctx.val('Tên shape').then(function(nm){ nm = String(nm || '').trim().toUpperCase(); if (!/^[A-Z0-9_\-]{1,31}$/.test(nm)){ ctx.out('Tên shape gồm chữ cái không dấu, số, gạch nối, tối đa 31 ký tự.'); return; }
        var had = shapeDef(d, nm);
        return (had ? ctx.kw('Đã có shape ' + nm + ' trong ' + had.file + '. Ghi đè?', [['Có', 'Y'], ['Không', 'N']], 'N') : Promise.resolve('Y')).then(function(k){ if (k !== 'Y') return;
          return ctx.val('Độ phân giải', '128').then(function(rv){ var res = Math.round(num(rv, 0)); if (!(res >= 8 && res <= 32767)){ ctx.out('Độ phân giải từ 8 đến 32767.'); return; }
            return ctx.point('Điểm gốc chèn').then(function(b){ if (!b.p) return;
              return ctx.sel('Chọn đối tượng tạo shape').then(function(ids){ var es = ents(ids, /^(line|pline|arc|circle|ellipse|spline)$/), segs = entSegs(es); TC.cad.clearSel();
                if (!segs.length){ ctx.out('Không có line, polyline, cung, đường tròn trong vùng chọn. Shape chỉ gồm nét.'); return; }
                var pts = []; segs.forEach(function(s){ pts.push(s[0], s[1]); }); var bb = bbOf(pts), Hs = Math.max(bb[2] - bb[0], bb[3] - bb[1]) || 1, q = function(p){ return [Math.round((p[0] - b.p[0]) / Hs * res) / res, Math.round((p[1] - b.p[1]) / Hs * res) / res]; };
                var def = { segs:segs.map(function(s){ return [q(s[0]), q(s[1])]; }).filter(function(s){ return s[0][0] !== s[1][0] || s[0][1] !== s[1][1]; }), res:res, H:Hs, desc:'Tạo bằng MKSHAPE' };
                ED.run('MKSHAPE', function(){ var st = ED.state(function(){ return shapeFiles(d); }, function(s){ d.shapeFiles = s; allEnts(d).forEach(function(x){ if (x.t === 'shape') TC.prepEnt(d, x); }); V().draw(); });
                  shapeFiles(d).forEach(function(x){ delete x.defs[nm]; }); var file = shapeFiles(d).filter(function(x){ return x.name.toLowerCase() === f.toLowerCase() + '.shx'; })[0]; if (!file){ file = { name:f + '.shx', path:'D:\\Tai-nguyen\\Shape\\' + f + '.shx', defs:{} }; d.shapeFiles.push(file); } file.defs[nm] = def; st(); });
                allEnts(d).forEach(function(x){ if (x.t === 'shape' && x.name === nm) TC.prepEnt(d, x); }); V().draw(); MKS.file = f; LASTSHP = nm;
                ctx.out('Đã biên dịch shape <b>' + nm + '</b> (' + qty(def.segs.length, 'vector') + ', độ phân giải ' + res + ') vào ' + lbl(f + '.shx') + ' và nạp vào bản vẽ' + (es.length < ids.length ? '; bỏ qua ' + qty(ids.length - es.length, 'đối tượng') + ' không phải nét' : '') + '. Dùng được trong MKLTYPE.');
                return ctx.kw('Chèn shape vừa tạo?', [['Chèn', 'I'], ['Xong', 'X']], 'X').then(function(k2){ if (k2 === 'I') return shapePlace(ctx, nm, Hs); }); }); }); }); }); }); });
  };

  // MKLTYPE: nét trùng đoạn mẫu thành nét gạch, điểm thành chấm, chữ và shape trên đoạn mẫu thành phần tử nhúng
  function ltBuild(d, a, b, es){
    var u = unit(sub(b, a)), n = [-u[1], u[0]], L = G.dist(a, b), tol = Math.max(L * .01, 1), dash = [], marks = [], skip = 0, ang = G.ang(a, b), along = function(p){ return dot(sub(p, a), u); }, off = function(p){ return dot(sub(p, a), n); };
    es.forEach(function(e){
      if (e.t === 'line' || e.t === 'pline'){ var segs = e.t === 'line' ? [[e.p1, e.p2]] : e._p.filter(function(pr){ return pr.k === 'seg'; }).map(function(pr){ return [pr.a, pr.b]; }), ok = false;
        segs.forEach(function(s){ if (Math.abs(off(s[0])) <= tol && Math.abs(off(s[1])) <= tol){ var t0 = TC.clamp(Math.min(along(s[0]), along(s[1])), 0, L), t1 = TC.clamp(Math.max(along(s[0]), along(s[1])), 0, L); if (t1 > t0){ dash.push([t0, t1]); ok = true; } } }); if (!ok) skip++; return; }
      if (e.t === 'point'){ var t = along(e.p); if (t >= 0 && t <= L && Math.abs(off(e.p)) <= tol) dash.push([t, t]); else skip++; return; }
      if (e.t === 'text'){ var c = e.al === 'm' ? e.p : frameOf(d, e).c, tt = along(c); if (tt < 0 || tt > L){ skip++; return; } marks.push({ k:'t', at:tt, s:e.s, h:e.h, style:e.style, dy:off(c), rot:an360((e.rot || 0) - ang) }); return; }
      if (e.t === 'shape'){ var ts = along(e.p); if (ts < 0 || ts > L){ skip++; return; } marks.push({ k:'s', at:ts, name:e.name, sz:e.h, dy:off(e.p), rot:an360((e.rot || 0) - ang) }); return; }
      skip++; });
    dash.sort(function(x, y){ return x[0] - y[0]; }); var m = []; dash.forEach(function(dv){ var l = m[m.length - 1]; if (l && dv[1] > dv[0] && dv[0] <= l[1] + 1e-6) l[1] = Math.max(l[1], dv[1]); else m.push(dv.slice()); });
    if (!m.length && !marks.length) return { err:'Không có nét nào nằm trên đoạn mẫu. Vẽ các đoạn thẳng trùng đoạn mẫu (bắt điểm End, Nearest), thêm chữ hoặc shape vào khoảng hở rồi chạy lại.' };
    if (m.length === 1 && m[0][0] <= tol && m[0][1] >= L - tol && !marks.length) return { err:'Đoạn mẫu chỉ có một nét liền: linetype sẽ giống Continuous. Chừa khoảng hở giữa các nét.' };
    return { L:L, dash:m, marks:marks.sort(function(x, y){ return x.at - y.at; }), skip:skip };
  }
  // dòng định nghĩa .lin (đơn vị bản vẽ chia LTSCALE); phần đầu là khoảng hở thì mở đầu bằng chấm 0
  function linStr(nm, def, file){ var k = 1 / ((TC.SV && TC.SV.get('LTSCALE')) || 1), r = function(x){ return String(Math.round(x * k * 1000) / 1000); }, parts = [], pos = 0;
    var ev = def.dash.map(function(v){ return { at:v[0], end:v[1] }; }).concat(def.marks.map(function(m){ return { at:m.at, m:m }; })).sort(function(x, y){ return x.at - y.at; });
    ev.forEach(function(x){ if (x.at > pos + 1e-9) parts.push('-' + r(x.at - pos)); if (x.m){ parts.push(x.m.k === 't' ? '["' + x.m.s + '",' + String(x.m.style || 'Standard').toUpperCase() + ',S=' + r(x.m.h) + ',R=' + Math.round(x.m.rot || 0) + ',X=0,Y=' + r(x.m.dy || 0) + ']' : '[' + x.m.name + ',' + file.replace(/\.lin$/, '') + '.shx,S=' + r(x.m.sz) + ',R=' + Math.round(x.m.rot || 0) + ']'); pos = Math.max(pos, x.at); } else { parts.push(x.end - x.at < 1e-9 ? '0' : r(x.end - x.at)); pos = Math.max(pos, x.end); } });
    if (def.L > pos + 1e-9) parts.push('-' + r(def.L - pos)); if (/^[-[]/.test(parts[0] || '')) parts.unshift('0');
    return '*' + nm + ',' + def.desc + '\nA,' + parts.join(','); }
  var lp0 = SC.ltPaint;
  SC.ltPaint = function(c, name, color){
    var d = D(), def = d && d.ltdefs && d.ltdefs[name]; if (!def) return lp0.apply(this, arguments);
    var x = c.getContext('2d'), w = c.width, h = c.height, k = (w - 4) / (def.L * 2); x.clearRect(0, 0, w, h); x.strokeStyle = x.fillStyle = color || TC.cssv('--c-text'); x.lineWidth = 1.5;
    for (var n = 0; n < 2; n++){ var o = 2 + n * def.L * k;
      def.dash.forEach(function(dv){ x.beginPath(); if (dv[1] - dv[0] < 1e-9){ x.arc(o + dv[0] * k, h / 2, 1, 0, 7); x.fill(); } else { x.moveTo(o + dv[0] * k, h / 2 + .5); x.lineTo(o + dv[1] * k, h / 2 + .5); x.stroke(); } });
      def.marks.forEach(function(m){ var cx = o + m.at * k; if (m.k === 't'){ x.save(); x.font = Math.max(7, Math.min(h - 2, m.h * k * 1.32)) + 'px "Arial Narrow", Arial, sans-serif'; x.textAlign = 'center'; x.textBaseline = 'middle'; x.fillText(m.s, cx, h / 2 + .5); x.restore(); return; } var sd = shapeDef(d, m.name); if (!sd) return; var z = Math.min(h - 2, m.sz * k); x.beginPath(); sd.segs.forEach(function(s){ x.moveTo(cx + s[0][0] * z, h / 2 - s[0][1] * z); x.lineTo(cx + s[1][0] * z, h / 2 - s[1][1] * z); }); x.stroke(); }); }
  };
  IMPL.MKLTYPE = function(ctx){
    var d = D(); ctx.badge('MKL');
    return ctx.val('Tên tệp linetype (.lin)', MKS.lin).then(function(f){ f = String(f).trim().replace(/\.lin$/i, ''); if (!/^[\w\-]+$/.test(f)){ ctx.out('Tên tệp chỉ gồm chữ không dấu, số và gạch nối.'); return; }
      return ctx.val('Tên linetype').then(function(nm){ nm = String(nm || '').trim().toUpperCase(); if (!/^[A-Z0-9_\-]{1,31}$/.test(nm)){ ctx.out('Tên linetype gồm chữ cái không dấu, số, gạch nối, tối đa 31 ký tự.'); return; }
        if (SC.ltList(d).some(function(x){ return x.toUpperCase() === nm; })){ ctx.out('Bản vẽ đã có linetype ' + nm + '. Chọn tên khác, hoặc xóa linetype cũ trong LINETYPE.'); return; }
        return ctx.val('Mô tả', nm.charAt(0) + nm.slice(1).toLowerCase() + ' ── · ──', { allowSpace:true }).then(function(desc){
          return ctx.point('Điểm đầu đoạn mẫu').then(function(a){ if (!a.p) return;
            return ctx.point('Điểm cuối đoạn mẫu', { base:a.p }).then(function(b){ if (!b.p) return; if (G.dist(a.p, b.p) < 1){ ctx.out('Đoạn mẫu quá ngắn.'); return; }
              V().temp = [{ k:'line', a:a.p, b:b.p, color:TC.cssv('--c-accent'), w:2, dash:[6, 4] }]; V().drawOver();
              return ctx.sel('Chọn đối tượng tạo linetype (nét, điểm, chữ, shape trên đoạn mẫu)').then(function(ids){ V().temp = []; TC.cad.clearSel(); var def = ltBuild(d, a.p, b.p, ents(ids));
                if (def.err){ ctx.out(def.err); return; } def.desc = String(desc).trim() || nm; def.file = f + '.lin';
                ED.run('MKLTYPE', function(){ var st = ED.state(function(){ return { l:SC.ltList(d).slice(), x:d.ltdefs || {} }; }, function(s){ d.linetypes = s.l; d.ltdefs = s.x; V().draw(); A().renderRibbon(); A().refreshPalettes(); }); d.ltdefs = Object.assign({}, d.ltdefs || {}); d.ltdefs[nm] = def; SC.ltList(d).push(nm); st(); });
                SC.LTS[nm] = [def.desc, [12, 4]]; MKS.lin = f;
                var nt = def.marks.filter(function(m){ return m.k === 't'; }).length, ns = def.marks.length - nt;
                ctx.out('Đã tạo linetype <b>' + nm + '</b> trong ' + lbl(f + '.lin') + ' và nạp vào bản vẽ: ' + [def.dash.length ? qty(def.dash.length, 'nét') : '', nt ? qty(nt, 'chữ') : '', ns ? qty(ns, 'shape') : ''].filter(Boolean).join(', ') + ', chu kỳ ' + TC.d(def.L) + (def.skip ? '; bỏ qua ' + qty(def.skip, 'đối tượng') + ' không nằm trên đoạn mẫu' : '') + '.');
                ctx.out('<span class="mono">' + esc(linStr(nm, def, def.file)).replace(/\n/g, '<br>') + '</span>', 'faint');
                return ctx.kw('Đặt ' + nm + ' làm linetype hiện hành?', [['Có', 'Y'], ['Không', 'N']], 'Y').then(function(k){ if (k !== 'Y') return; ED.cur(d).lt = nm; A().renderRibbon(); A().refreshPalettes(); ctx.out('Linetype hiện hành: <b>' + nm + '</b>. Đối tượng vẽ mới dùng linetype này; đổi cho đối tượng có sẵn trong palette Thuộc tính.'); }); },
              function(err){ V().temp = []; V().drawOver(); throw err; }); }); }); }); }); });
  };

  /* ================= Layout: ALIGNSPACE, VPSYNC, VPSCALE ================= */
  function vpAt(L, p){ return L.vports.filter(function(q){ return p[0] >= q.x && p[0] <= q.x + q.w && p[1] >= q.y && p[1] <= q.y + q.h; })[0] || null; }
  function vpRect(vp){ return [[vp.x, vp.y], [vp.x + vp.w, vp.y], [vp.x + vp.w, vp.y + vp.h], [vp.x, vp.y + vp.h]]; }
  function vpName(d, vp){ var x = vpsOf(d).filter(function(q){ return q.vp === vp; })[0]; return x ? 'viewport #' + (x.i + 1) + ' (' + esc(x.L.name) + ')' : 'viewport'; }
  // lệnh layout: đang ở Model thì chuyển sang layout đầu tiên có đủ viewport rồi chạy lại
  function needLayout(ctx, cmd, min){ var d = D(); min = min || 1; if (d.space !== 'model' && d.layouts[d.space].vports.length >= min) return d.layouts[d.space];
    var li = d.layouts.map(function(L, i){ return L.vports.length >= min ? i : -1; }).filter(function(i){ return i >= 0; })[0];
    if (li == null){ ctx.out(cmd + ' cần một layout có ' + (min > 1 ? 'ít nhất ' + min + ' viewport' : 'viewport') + '. Tạo viewport bằng MVIEW rồi chạy lại.'); return null; }
    ctx.out(cmd + ' chạy trong layout: chuyển sang layout <b>' + esc(d.layouts[li].name) + '</b>.', 'faint'); setTimeout(function(){ A().setSpace(li); TC.cmd.run(cmd); }, 0); return null; }
  function vpHl(vp, col, dash){ return { k:'poly', pts:vpRect(vp), closed:true, paper:true, color:col || TC.cssv('--c-accent'), w:2.5, dash:dash }; }
  function pickVp(ctx, L, text, skip, extra){ return ctx.point(text, { pick:true, dyn:false, temp:extra || [], def:extra && extra.length > 1 ? 'áp dụng' : null, onMove:function(p, temp){ var vp = vpAt(L, p); if (vp && vp !== skip) temp.push(vpHl(vp)); } }).then(function(r){ if (!r.p) return r.enter ? 'enter' : null; var vp = vpAt(L, r.p); if (!vp || vp === skip){ ctip(r, vp ? 'Đây là viewport chính' : 'Bấm vào bên trong một viewport'); return pickVp(ctx, L, text, skip, extra); } return vp; }); }
  function unlockAsk(ctx, vps){ var lk = vps.filter(function(v){ return v.locked; }); if (!lk.length) return Promise.resolve(true);
    return ctx.kw((lk.length > 1 ? lk.length + ' viewport' : 'Viewport') + ' đang khóa hiển thị. Vẫn thay đổi (giữ khóa sau đó)?', [['Có', 'Y'], ['Không', 'N']], 'Y').then(function(k){ if (k === 'Y') return true; ctx.out('Giữ nguyên viewport đang khóa.', 'faint'); return false; }); }
  // ALIGNSPACE: hai điểm trong Model trùng hai điểm trên Paper (dời tâm view, đổi tỉ lệ); xem trước khi chỉ điểm trên Paper
  IMPL.ALIGNSPACE = function(ctx){
    var d = D(); ctx.badge('AS'); var L = needLayout(ctx, 'ALIGNSPACE'); if (!L) return;
    var vp0 = d.activeVp && L.vports.filter(function(q){ return q.id === d.activeVp; })[0], sv = null;
    var restore = function(){ if (sv){ sv.vp.c = sv.c; sv.vp.scale = sv.scale; sv = null; } d.activeVp = null; V().draw(); A().renderStatus(); };
    var setVp = function(vp, m, p, k){ vp.scale = k; vp.c = [m[0] - (p[0] - (vp.x + vp.w / 2)) * k, m[1] - (p[1] - (vp.y + vp.h / 2)) * k]; };
    if (!vp0){ d.activeVp = null; V().draw(); }
    return (vp0 ? Promise.resolve(vp0) : pickVp(ctx, L, 'Chọn viewport cần gióng')).then(function(vp){ if (!vp || vp === 'enter') return;
      return unlockAsk(ctx, [vp]).then(function(ok){ if (!ok) return; sv = { vp:vp, c:vp.c.slice(), scale:vp.scale }; d.activeVp = vp.id; V().draw(); A().renderStatus();
        return ctx.point('Điểm gióng thứ nhất trong MODEL').then(function(m1){ if (!m1.p){ restore(); return; }
          return ctx.point('Điểm gióng thứ hai trong MODEL', { base:m1.p, def:'chỉ dời' }).then(function(m2){ d.activeVp = null; V().draw(); A().renderStatus();
            var mark = [{ k:'pt', p:m1.p, color:TC.cssv('--c-warn') }];
            return ctx.point('Điểm gióng thứ nhất trên PAPER', { rubber:false, onMove:function(p){ setVp(vp, m1.p, p, sv.scale); V().draw(); } }).then(function(p1){ if (!p1.p){ restore(); return; }
              var two = m2.p && G.dist(m1.p, m2.p) > 1e-6, kOf = function(p){ var dd = G.dist(p1.p, p); return dd > 1e-6 ? G.dist(m1.p, m2.p) / dd : sv.scale; };
              setVp(vp, m1.p, p1.p, sv.scale); V().draw();
              return (two ? ctx.point('Điểm gióng thứ hai trên PAPER', { base:p1.p, temp:mark.length ? [] : [], onMove:function(p){ setVp(vp, m1.p, p1.p, kOf(p)); V().draw(); } }) : Promise.resolve({})).then(function(p2){
                var k = two && p2.p ? kOf(p2.p) : sv.scale; vp.c = sv.c; vp.scale = sv.scale; sv = null;
                ED.run('ALIGNSPACE', function(){ var st = vpSnap(d); setVp(vp, m1.p, p1.p, k); st(); }); V().draw(); A().renderStatus(); A().refreshPalettes();
                var dA = two && p2.p ? an360(G.ang(p1.p, p2.p) - G.ang(m1.p, m2.p) + 180) - 180 : 0;
                ctx.out('Đã gióng ' + vpName(d, vp) + ': điểm Model ' + TC.d(m1.p[0], 0) + ',' + TC.d(m1.p[1], 0) + ' nằm tại ' + TC.d(p1.p[0], 1) + ',' + TC.d(p1.p[1], 1) + ' trên giấy, tỉ lệ 1:' + kFmt(k) + '.' + (Math.abs(k - Math.round(k)) > 1e-6 ? ' Tỉ lệ lẻ: đặt tỉ lệ chuẩn bằng VPSCALE.' : '') + (Math.abs(dA) > .5 ? ' Hai đoạn lệch ' + TC.d(Math.abs(dA), 1) + '°: viewport chưa xoay view, chỉ gióng vị trí và tỉ lệ.' : '')); }); }); }); }); }); }).then(null, function(err){ restore(); throw err; });
  };
  // VPSYNC: viewport được chọn lấy tỉ lệ của viewport chính và dời để Model liền mạch qua các khung (cả hai phương, hoặc chỉ gióng hàng, cột)
  IMPL.VPSYNC = function(ctx){
    var d = D(); ctx.badge('VS'); var L = needLayout(ctx, 'VPSYNC', 2); if (!L) return; d.activeVp = null; V().draw(); A().renderStatus();
    var orig = {}; L.vports.forEach(function(v){ orig[v.id] = { c:v.c.slice(), scale:v.scale }; });
    var back = function(){ L.vports.forEach(function(v){ v.c = orig[v.id].c.slice(); v.scale = orig[v.id].scale; }); V().draw(); };
    var sync = function(m, s, mode){ var c = [m.c[0] + (s.x + s.w / 2 - m.x - m.w / 2) * m.scale, m.c[1] + (s.y + s.h / 2 - m.y - m.h / 2) * m.scale]; s.scale = m.scale; s.c = [mode === 'H' ? orig[s.id].c[0] : c[0], mode === 'V' ? orig[s.id].c[1] : c[1]]; };
    var MODE = { B:'cả hai phương', H:'gióng hàng ngang', V:'gióng cột dọc' };
    return pickVp(ctx, L, 'Chọn viewport chính').then(function(m){ if (!m || m === 'enter') return;
      return ask(ctx, 'Đồng bộ theo', [['Cả hai phương', 'B'], ['Gióng hàng ngang', 'H'], ['Gióng cột dọc', 'V']], 'B').then(function(mode){ mode = MODE[mode] ? mode : 'B'; var picked = [];
        var loop = function(){ return pickVp(ctx, L, 'Chọn viewport cần đồng bộ' + (picked.length ? ' (đã chọn ' + picked.length + ', Enter để áp dụng)' : ''), m, [vpHl(m, TC.cssv('--c-warn'))].concat(picked.map(function(v){ return vpHl(v, null, [6, 3]); }))).then(function(v){
          if (v === 'enter' || !v){ if (!picked.length){ ctx.out('Chưa chọn viewport nào.'); back(); return; } return commit(); }
          if (picked.indexOf(v) < 0){ picked.push(v); sync(m, v, mode); V().draw(); } return loop(); }); };
        var commit = function(){ return unlockAsk(ctx, picked).then(function(ok){ var use = ok ? picked : picked.filter(function(v){ return !v.locked; }); back(); if (!use.length){ ctx.out('Không viewport nào được đồng bộ.'); return; }
          ED.run('VPSYNC', function(){ var st = vpSnap(d); use.forEach(function(v){ sync(m, v, mode); }); st(); }); V().draw(); A().renderStatus(); A().refreshPalettes();
          ctx.out('Đã đồng bộ ' + qty(use.length, 'viewport') + ' theo ' + vpName(d, m) + ' (tỉ lệ 1:' + kFmt(m.scale) + ', ' + MODE[mode] + ').' + (use.length < picked.length ? ' ' + (picked.length - use.length) + ' viewport khóa giữ nguyên.' : '')); }); };
        return loop(); }); }).then(null, function(err){ back(); throw err; });
  };
  IMPL.VPSCALE = function(ctx){
    var d = D(), all = vpsOf(d); ctx.badge('VSC'); if (!all.length){ ctx.out('Bản vẽ chưa có viewport nào. Tạo viewport bằng MVIEW trong layout.'); return; }
    ctx.out('Tỉ lệ viewport: ' + all.map(function(x){ return esc(x.L.name) + ' #' + (x.i + 1) + ' 1:' + kFmt(x.vp.scale); }).join(' · ') + '.', 'faint');
    var pre = d.sel.map(function(id){ return d.byId[id]; }).filter(function(e){ return e && e.t === 'vpframe'; }).map(function(e){ return e.vp; }), cur = d.space !== 'model' ? d.layouts[d.space] : null, on = {};
    all.forEach(function(x){ on[x.vp.id] = pre.length ? pre.indexOf(x.vp.id) >= 0 : cur ? x.L === cur : true; });
    var scales = (d.scales || ['1:1', '1:2', '1:5', '1:10', '1:20', '1:25', '1:50', '1:75', '1:100', '1:125', '1:200', '1:250', '1:500', '1:1000']).filter(function(s){ return /^1:/.test(s); });
    return new Promise(function(res){
      var dlg = TC.dialog({ cls:'mr2', id:'vpscale', title:'Tỉ lệ nhiều viewport', cmd:'VPSCALE', code:'UI-MR2', w:660, onClose:function(){ res(); },
        body:'<div class="stack"><div class="hrow">' + H.row('Tỉ lệ mới', H.sel('vs-k', scales.concat(['Tùy chỉnh…']), scales.indexOf('1:100') >= 0 ? '1:100' : scales[0], 120) + '<span id="vs-cw" class="hrow" hidden style="gap:4px">1 :' + H.num('vs-c', '100', { unit:'none', prec:2, pick:false, w:96 }) + '</span>', 72) + '</div>' +
          '<div class="tblwrap" style="max-height:250px"><table class="tbl"><colgroup><col style="width:32px"><col><col style="width:120px"><col style="width:100px"><col style="width:56px"><col style="width:110px"></colgroup><thead><tr><th class="ctr"><input type="checkbox" id="vs-all" aria-label="Chọn tất cả"></th><th>Layout</th><th>Viewport</th><th class="num">Tỉ lệ hiện tại</th><th class="ctr">Khóa</th><th class="num">Tỉ lệ mới</th></tr></thead><tbody id="vs-rows"></tbody></table></div>' +
          H.chk('vs-ul', 'Đổi cả viewport đang khóa hiển thị (giữ khóa sau khi đổi)', true) + '<p class="faint mr2-s">Tâm view của từng viewport giữ nguyên; nội dung co giãn quanh tâm. Như VPSCALE của Express Tools, dòng lệnh liệt kê tỉ lệ hiện tại.</p></div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="vs-ok">Đặt tỉ lệ</button>' });
      var k = function(){ var v = $('#vs-k', dlg.el).value; return v === 'Tùy chỉnh…' ? nv(dlg, 'vs-c', 0) : Number(v.split(':')[1]); };
      function rows(){ var kk = k(), ul = $('#vs-ul', dlg.el).checked, n = 0;
        $('#vs-rows', dlg.el).innerHTML = all.map(function(x){ var skip = x.vp.locked && !ul, ch = on[x.vp.id]; if (ch && !skip) n++; return '<tr class="' + (ch ? 'is-sel' : '') + '"><td class="ctr"><input type="checkbox" data-vp="' + x.vp.id + '"' + (ch ? ' checked' : '') + ' aria-label="Chọn viewport #' + (x.i + 1) + ' của ' + esc(x.L.name) + '"></td><td>' + esc(x.L.name) + '</td><td>#' + (x.i + 1) + ' <span class="faint">' + TC.d(x.vp.w, 0) + ' × ' + TC.d(x.vp.h, 0) + '</span></td><td class="num">1:' + kFmt(x.vp.scale) + '</td><td class="ctr">' + (x.vp.locked ? ic('lock', 's12') : '') + '</td><td class="num">' + (ch ? (skip ? '<span class="faint">giữ (khóa)</span>' : '<b>1:' + kFmt(kk) + '</b>') : '') + '</td></tr>'; }).join('');
        var b = $('#vs-ok', dlg.el); b.disabled = !(kk > 0 && n); b.textContent = n && kk > 0 ? 'Đặt tỉ lệ 1:' + kFmt(kk) + ' cho ' + qty(n, 'viewport') : 'Đặt tỉ lệ'; $('#vs-all', dlg.el).checked = all.every(function(x){ return on[x.vp.id]; }); }
      dlg.el.addEventListener('change', function(ev){ var c = ev.target.closest('[data-vp]'); if (c) on[c.dataset.vp] = c.checked; if (ev.target.id === 'vs-all') all.forEach(function(x){ on[x.vp.id] = ev.target.checked; }); if (ev.target.id === 'vs-k') $('#vs-cw', dlg.el).hidden = ev.target.value !== 'Tùy chỉnh…'; rows(); });
      dlg.el.addEventListener('numchange', rows);
      $('#vs-ok', dlg.el).addEventListener('click', function(){ var kk = k(), ul = $('#vs-ul', dlg.el).checked; if (!fieldErr(dlg, 'vs-c', kk > 0 ? null : 'Tỉ lệ phải lớn hơn 0.')) return;
        var use = all.filter(function(x){ return on[x.vp.id] && (!x.vp.locked || ul); }), skip = all.filter(function(x){ return on[x.vp.id] && x.vp.locked && !ul; }).length;
        ED.run('VPSCALE', function(){ var st = vpSnap(d); use.forEach(function(x){ x.vp.scale = kk; }); st(); }); V().draw(); A().renderStatus(); A().refreshPalettes(); dlg.close(true);
        ctx.out('Đã đặt tỉ lệ 1:' + kFmt(kk) + ' cho ' + qty(use.length, 'viewport') + (skip ? '; ' + skip + ' viewport khóa giữ nguyên' : '') + '. PS:MS = 1:' + kFmt(kk) + ', hệ số ' + TC.d(1 / kk, 4) + '.'); });
      rows();
    });
  };

  /* ================= Kích thước: DIMEX, DIMIM, QLATTACH ================= */
  var BLK = { 'Gạch nghiêng kiến trúc':'_ARCHTICK', 'Gạch nghiêng':'_OBLIQUE', 'Mũi tên kín':'_CLOSEDFILLED', 'Mũi tên kín rỗng':'_CLOSEDBLANK', 'Mũi tên hở':'_OPEN', 'Chấm':'_DOT', 'Chấm nhỏ':'_DOTSMALL', 'Góc vuông':'_OPEN90', 'Tích':'_INTEGRAL', 'Không':'_NONE' };
  function dimToTxt(name, df, ts){ return '*' + name + '\nDIMASZ ' + df.asz + '\nDIMBLK ' + (BLK[df.arrow] || '_CLOSEDFILLED') + '\nDIMCEN ' + (df.cmark === 'Không' ? 0 : df.cmark === 'Đường' ? -df.csz : df.csz) + '\nDIMDEC ' + (String(df.unit.prec).split('.')[1] || '').length + '\nDIMDLI ' + df.dl.base + '\nDIMEXE ' + df.ext.ov + '\nDIMEXO ' + df.ext.off + '\nDIMGAP ' + df.txt.gap + (df.unit.pre || df.unit.suf ? '\nDIMPOST "' + df.unit.pre + '<>' + df.unit.suf + '"' : '') + '\nDIMSCALE ' + df.scale + (ts ? '\nDIMTXSTY ' + df.txt.style : '') + '\nDIMTXT ' + df.txt.h; }
  function dimParse(txt){ var out = [], cur = null; String(txt).split('\n').forEach(function(l){ l = l.trim(); if (!l) return; if (l[0] === ';'){ if (cur) cur.desc = l.slice(1).trim(); return; } if (l[0] === '*'){ cur = { name:l.slice(1).trim(), v:{} }; out.push(cur); return; } var m = /^(\w+)\s+(.*)$/.exec(l); if (cur && m) cur.v[m[1].toUpperCase()] = m[2].replace(/^"|"$/g, ''); }); return out; }
  function dimFrom(p, b0){ var df = JSON.parse(JSON.stringify(b0)), v = p.v, n = function(k, def){ var x = Number(v[k]); return v[k] != null && isFinite(x) ? x : def; }, ar = Object.keys(BLK).filter(function(k){ return BLK[k] === v.DIMBLK; })[0], cen = n('DIMCEN', df.csz), dec = n('DIMDEC', 0);
    if (ar) df.arrow = ar; df.asz = n('DIMASZ', df.asz); df.cmark = cen === 0 ? 'Không' : cen < 0 ? 'Đường' : 'Dấu'; df.csz = Math.abs(cen) || df.csz; df.unit.prec = dec ? '0.' + new Array(dec + 1).join('0') : '0';
    df.dl.base = n('DIMDLI', df.dl.base); df.ext.ov = n('DIMEXE', df.ext.ov); df.ext.off = n('DIMEXO', df.ext.off); df.txt.gap = n('DIMGAP', df.txt.gap); df.scale = n('DIMSCALE', df.scale); df.txt.h = n('DIMTXT', df.txt.h); if (v.DIMTXSTY) df.txt.style = v.DIMTXSTY; df.desc = p.desc || 'Nhập bằng DIMIM'; df.from = 'DIMIM'; return df; }
  IMPL.DIMEX = function(ctx){
    var d = D(), defs = SC.dimDefs(d), names = Object.keys(defs), on = {}; ctx.badge('DX'); names.forEach(function(n){ on[n] = true; });
    var used = function(n){ return allEnts(d).filter(function(e){ return /^dim/.test(e.t) && (e.style || 'TCVN-1-100') === n; }).length; };
    return new Promise(function(res){
      var dlg = TC.dialog({ cls:'mr2', id:'dimex', title:'Xuất kiểu kích thước', cmd:'DIMEX', code:'UI-MR2', w:760, onClose:function(){ res(); },
        body:'<div class="mr2-dx"><div class="stack"><div class="tblwrap" style="height:200px"><table class="tbl"><colgroup><col style="width:32px"><col><col style="width:84px"></colgroup><thead><tr><th class="ctr"><input type="checkbox" id="dx-all" checked aria-label="Chọn tất cả"></th><th>Kiểu kích thước</th><th class="num">Đang dùng</th></tr></thead><tbody>' + names.map(function(n){ return '<tr><td class="ctr"><input type="checkbox" data-ds="' + esc(n) + '" checked aria-label="Xuất ' + esc(n) + '"></td><td>' + esc(n) + (n === ED.cur(d).dimstyle ? ' <span class="tag acc">hiện hành</span>' : '') + '</td><td class="num">' + TC.n(used(n)) + '</td></tr>'; }).join('') + '</tbody></table></div>' +
          '<div class="field"><label for="dx-f">Tệp .dim</label><div class="hrow" style="flex-wrap:nowrap"><input class="inp mono" id="dx-f" value="' + esc('D:\\Tai-nguyen\\Kieu-kich-thuoc\\' + d.name.replace(/\.\w+$/, '') + '.dim') + '" spellcheck="false" style="flex:1"><button class="btn sm" type="button" id="dx-br">Duyệt…</button></div><span class="fmsg" id="dx-f-msg"></span></div>' + H.chk('dx-ts', 'Ghi kèm kiểu chữ của kích thước (DIMTXSTY)', true) + '</div>' +
          '<div class="stack"><span class="gh">Nội dung tệp</span><pre class="mr2-code" id="dx-pv" style="height:262px"></pre></div></div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="dx-ok">Xuất</button>' });
      var sel = function(){ return names.filter(function(n){ return on[n]; }); }, txt = function(){ return '; TrionCAD · kiểu kích thước xuất bằng DIMEX từ ' + d.name + ' · ' + TC.date() + '\n' + sel().map(function(n){ return dimToTxt(n, defs[n], $('#dx-ts', dlg.el).checked) + (defs[n].desc ? '\n; ' + defs[n].desc : ''); }).join('\n'); };
      function upd(){ var n = sel().length; $('#dx-pv', dlg.el).textContent = txt(); $('#dx-ok', dlg.el).disabled = !n; $('#dx-ok', dlg.el).textContent = n ? 'Xuất ' + qty(n, 'kiểu kích thước') : 'Xuất'; $('#dx-all', dlg.el).checked = n === names.length; }
      dlg.el.addEventListener('change', function(ev){ var c = ev.target.closest('[data-ds]'); if (c) on[c.dataset.ds] = c.checked; if (ev.target.id === 'dx-all'){ names.forEach(function(n){ on[n] = ev.target.checked; }); $$('[data-ds]', dlg.el).forEach(function(x){ x.checked = ev.target.checked; }); } upd(); });
      $('#dx-br', dlg.el).addEventListener('click', function(){ TC.toast('Mở hộp thoại lưu tệp .dim của hệ điều hành.', { type:'info', log:false }); });
      $('#dx-ok', dlg.el).addEventListener('click', function(){ var f = $('#dx-f', dlg.el).value.trim(); if (!fieldErr(dlg, 'dx-f', /^([a-z]:\\|\\\\).+\.dim$/i.test(f) ? null : 'Cần đường dẫn đầy đủ tới tệp .dim, ví dụ D:\\Tai-nguyen\\kich-thuoc.dim')) return;
        var go = function(){ var n = sel().length, names2 = sel(); FS[fsKey(f) || f] = txt(); dlg.close(true); ctx.out('Đã xuất ' + qty(n, 'kiểu kích thước') + ' (' + names2.map(esc).join(', ') + ') ra ' + lbl(f) + '. Nhập vào bản vẽ khác bằng DIMIM.'); TC.toast('Đã xuất ' + n + ' kiểu kích thước ra <b>' + esc(base(f)) + '</b>.', { log:false }); };
        if (fsKey(f)) TC.confirm({ title:'Ghi đè tệp', msg:'<b>' + esc(base(f)) + '</b> đã có. Thay bằng ' + qty(sel().length, 'kiểu kích thước') + ' đang chọn?', detail:'Các kiểu trong tệp cũ sẽ mất.', ok:'Ghi đè tệp', danger:true }).then(function(y){ if (y) go(); }); else go(); });
      upd();
    });
  };
  IMPL.DIMIM = function(ctx){
    var d = D(), defs = SC.dimDefs(d), files = Object.keys(FS).filter(function(k){ return /\.dim$/i.test(k); }), f = files[0], mode = 'K', on = {}, P = []; ctx.badge('DI');
    var b0 = defs.Standard || defs[Object.keys(defs)[0]];
    return new Promise(function(res){
      var dlg = TC.dialog({ cls:'mr2', id:'dimim', title:'Nhập kiểu kích thước', cmd:'DIMIM', code:'UI-MR2', w:740, onClose:function(){ res(); },
        body:'<div class="stack">' + H.row('Tệp .dim', H.sel('di-f', files, f, 440) + '<button class="btn sm" type="button" id="di-br">Duyệt…</button>', 90) + H.row('Khi trùng tên', SC.seg('di-m', ['Giữ kiểu hiện có', 'Ghi đè'], 'Giữ kiểu hiện có'), 90) +
          '<div class="tblwrap" style="max-height:240px"><table class="tbl"><colgroup><col style="width:32px"><col><col style="width:170px"><col style="width:150px"><col style="width:80px"><col style="width:80px"></colgroup><thead><tr><th class="ctr"></th><th>Kiểu trong tệp</th><th>Trạng thái</th><th>Mũi tên</th><th class="num">Chữ cao</th><th class="num">Tỉ lệ</th></tr></thead><tbody id="di-rows"></tbody></table></div><p class="faint mr2-s" id="di-n"></p></div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="di-ok">Nhập</button>' });
      var plan = function(){ return P.filter(function(p){ return on[f + '|' + p.name]; }).map(function(p){ var ex = !!defs[p.name]; return { p:p, act:ex ? mode : 'N' }; }); };
      function rows(){ var pl = plan(), nN = pl.filter(function(x){ return x.act === 'N'; }).length, nO = pl.filter(function(x){ return x.act === 'O'; }).length, nK = pl.filter(function(x){ return x.act === 'K'; }).length;
        $('#di-rows', dlg.el).innerHTML = P.map(function(p){ var ex = !!defs[p.name], df = dimFrom(p, b0); return '<tr><td class="ctr"><input type="checkbox" data-dp="' + esc(p.name) + '"' + (on[f + '|' + p.name] ? ' checked' : '') + ' aria-label="Nhập ' + esc(p.name) + '"></td><td>' + esc(p.name) + '</td><td>' + (ex ? (mode === 'O' ? '<span class="tag warn">Trùng tên · ghi đè</span>' : '<span class="tag">Trùng tên · giữ</span>') : '<span class="tag ok">Mới</span>') + '</td><td>' + esc(df.arrow) + '</td><td class="num">' + TC.d(df.txt.h) + '</td><td class="num">' + kFmt(df.scale) + '</td></tr>'; }).join('') || '<tr><td colspan="6" class="faint">Tệp không có kiểu kích thước nào.</td></tr>';
        $('#di-n', dlg.el).textContent = P.length + ' kiểu trong tệp · ' + nN + ' mới' + (nO ? ' · ' + nO + ' ghi đè' : '') + (nK ? ' · ' + nK + ' trùng tên giữ nguyên' : '');
        var n = nN + nO, b = $('#di-ok', dlg.el); b.disabled = !n; b.textContent = n ? 'Nhập ' + qty(n, 'kiểu kích thước') : 'Nhập'; }
      function load(){ P = dimParse(FS[f] || ''); P.forEach(function(p){ if (on[f + '|' + p.name] == null) on[f + '|' + p.name] = true; }); rows(); }
      $('#di-f', dlg.el).addEventListener('change', function(){ f = this.value; load(); });
      dlg.el.addEventListener('change', function(ev){ var c = ev.target.closest('[data-dp]'); if (c){ on[f + '|' + c.dataset.dp] = c.checked; rows(); } });
      dlg.el.addEventListener('segchange', function(ev){ mode = ev.detail === 'Ghi đè' ? 'O' : 'K'; rows(); });
      $('#di-br', dlg.el).addEventListener('click', function(){ TC.toast('Mở hộp thoại chọn tệp .dim của hệ điều hành.', { type:'info', log:false }); });
      $('#di-ok', dlg.el).addEventListener('click', function(){ var pl = plan(), go = pl.filter(function(x){ return x.act !== 'K'; }), kept = pl.filter(function(x){ return x.act === 'K'; }); if (!go.length) return;
        ED.run('DIMIM', function(){ var st = ED.state(function(){ return { n:d.dimstyles.slice(), x:SC.dimDefs(d) }; }, function(s){ d.dimstyles = s.n; d.dimDefs = s.x; A().renderRibbon(); A().refreshPalettes(); }); var D0 = SC.dimDefs(d); go.forEach(function(x){ D0[x.p.name] = dimFrom(x.p, b0); if (d.dimstyles.indexOf(x.p.name) < 0) d.dimstyles.push(x.p.name); }); st(); });
        A().renderRibbon(); dlg.close(true);
        ctx.out('Đã nhập ' + qty(go.length, 'kiểu kích thước') + ' từ ' + lbl(base(f)) + ': ' + go.map(function(x){ return esc(x.p.name) + (x.act === 'O' ? ' (ghi đè)' : ''); }).join(', ') + '.' + (kept.length ? ' Giữ nguyên ' + kept.map(function(x){ return esc(x.p.name); }).join(', ') + ' vì trùng tên.' : '')); });
      load();
    });
  };
  // điểm gắn: giữa cạnh trái hoặc phải của chú thích (MText: giữa dòng đầu); đoạn cuối xiên thì chừa đoạn móc ngang
  function annBox(d, a){
    if (a.t === 'text'){ var f = frameOf(d, a), b = bbOf([f.lb, add2(f.lb, f.u, f.w), add2(add2(f.lb, f.u, f.w), f.v, f.h), add2(f.lb, f.v, f.h)]); return { b:b, y:(b[1] + b[3]) / 2 }; }
    if (a.t === 'mtext'){ var ls = TC.mtextLines(a), W = Math.max.apply(null, ls.map(function(s){ return tw(d, s, a.h, a.style); }).concat([a.h])), x0 = a.al === 'c' ? a.p[0] + a.w / 2 - W / 2 : a.al === 'r' ? a.p[0] + a.w - W : a.p[0]; return { b:[x0, a.p[1] - a.h * (1 + (ls.length - 1) * (a.ls || 1.6)) - a.h * .3, x0 + W, a.p[1]], y:a.p[1] - a.h / 2 }; }
    var bb = a._bb; return { b:bb, y:(bb[1] + bb[3]) / 2 }; }
  // chữ nằm phía đối diện mũi tên; điểm cuối của đường dẫn là đầu đoạn móc ngang, atp là điểm gắn (vẽ thêm đoạn móc khi tạo hình)
  function attachPts(d, L, a){ var pts = L.pts.slice(), n = pts.length, B = annBox(d, a), h = L.h || 220, left = pts[0][0] <= (B.b[0] + B.b[2]) / 2, at = [left ? B.b[0] - h * .45 : B.b[2] + h * .45, B.y];
    pts[n - 1] = [at[0] - (left ? 1 : -1) * h, at[1]]; return { pts:pts, at:at }; }
  var ql0 = X.qleader;
  if (ql0) X.qleader = function(x){ var e = x.e; if (e.att == null || !e.atp || (e.ann && e.ann !== 'none')) return ql0(x); return ql0(Object.assign({}, x, { e:Object.assign({}, e, { pts:e.pts.concat([e.atp]) }) })); };
  IMPL.QLATTACH = function(ctx){
    var d = D(); ctx.badge('QLA');
    return ED.pick(ctx, 'Chọn đường dẫn', { types:['qleader'], bad:'Chọn đường dẫn tạo bằng QLEADER' }).then(function(r){ if (!r || !r.e) return; var L = r.e;
      if ((L.ann || 'mtext') !== 'none' && (L.s || (L.ann && L.ann !== 'mtext'))){ ctx.out('Đường dẫn này đã có chú thích riêng. Chọn đường dẫn không có chú thích (QLEADER, Loại chú thích: Không).'); return; }
      return ED.pick(ctx, 'Chọn chú thích (MText, chữ, dung sai, block)', { types:['mtext', 'text', 'tol', 'insert'], bad:'Chọn MText, chữ, khung dung sai hoặc block', locked:true, onMove:function(p, temp, hov){ var a = hov && d.byId[hov]; if (a && a !== L && /^(mtext|text|tol|insert)$/.test(a.t)){ var x = ED.clean(L), q = attachPts(d, x, a); x.pts = q.pts; x.atp = q.at; x.att = a.id; x.ann = 'none'; temp.push(ED.temp([x], { color:TC.cssv('--c-accent'), w:1.8 })); } } }).then(function(q){ if (!q || !q.e) return; var a = q.e;
        ED.run('QLATTACH', function(){ ED.mod(L, function(x){ var q = attachPts(d, x, a); x.att = a.id; x.ann = 'none'; x.pts = q.pts; x.atp = q.at; }); }); d._qlAtt = true; TC.cad.clearSel();
        ctx.out('Đã gắn đường dẫn vào ' + esc(NM[a.t] || a.t) + '. Di chuyển hoặc sửa chú thích thì đường dẫn đi theo.'); }); });
  };

  /* ================= Tệp: REVERT, QQUIT, MOVEBAK, REDIR ================= */
  TC.on('dirty', function(d){ if (d && d.dirty === false) d._savedAt = (d.undo || []).length; });
  function unsaved(d){ return d.dirty ? Math.max(1, Math.abs((d.undo || []).length - (d._savedAt || 0))) : 0; }
  IMPL.REVERT = function(ctx){
    var d = D(); ctx.badge('RV');
    if (d.isNew && !d.savedOnce){ ctx.out('Bản vẽ chưa lưu lần nào nên không có bản đã lưu để mở lại. Lưu bằng Ctrl+S.'); return; }
    if (d.bedit || d.refedit){ ctx.out('Đóng Block Editor hoặc kết thúc sửa tại chỗ (REFEDIT) trước khi mở lại bản đã lưu.'); return; }
    var n = unsaved(d), others = A().docs.filter(function(x){ return x.open && x !== d; }).length; if (!n){ ctx.out('<b>' + esc(d.name) + '</b> không có thay đổi chưa lưu, không cần mở lại.'); return; }
    return TC.confirm({ title:'Mở lại bản đã lưu', msg:'Bỏ <b>' + qty(n, 'thay đổi') + '</b> chưa lưu của <b>' + esc(d.name) + '</b> và mở lại bản đã lưu?', detail:'Bản đã lưu lúc ' + esc(d.modified || '') + '. Lịch sử hoàn tác của bản vẽ này sẽ xóa, không lấy lại được.' + (others ? ' ' + qty(others, 'bản vẽ') + ' khác đang mở không bị ảnh hưởng.' : ''), ok:'Bỏ ' + qty(n, 'thay đổi') + ' và mở lại', danger:true }).then(function(y){
      if (!y){ ctx.out('Đã hủy, bản vẽ giữ nguyên.'); return; }
      var k = (d.undo || []).length - (d._savedAt || 0), msg = TC.cmdline.msg; TC.cmdline.msg = function(){};
      try { if (k > 0) ED.undo(k); else if (k < 0) ED.redo(-k); } finally { TC.cmdline.msg = msg; } // ponytail: lùi theo lịch sử hoàn tác tới điểm lưu; bản thật đọc lại tệp từ đĩa
      d.undo = []; d.redo = []; d.dirty = false; d.sel = []; TC.emit('dirty', d); A().renderFileTabs(); A().renderTitle(); V().draw(); A().refreshPalettes(); TC.emit('sel');
      ctx.out('Đã mở lại <b>' + esc(d.name) + '</b> từ bản lưu lúc ' + esc(d.modified || '') + '; đã bỏ ' + qty(n, 'thay đổi') + '.'); TC.toast('Đã mở lại <b>' + esc(d.name) + '</b> từ bản đã lưu.', { log:false }); });
  };
  IMPL.QQUIT = function(ctx){
    var docs = A().docs.filter(function(x){ return x.open; }), dirty = docs.filter(function(x){ return x.dirty; }), ch = dirty.reduce(function(s, x){ return s + unsaved(x); }, 0); ctx.badge('QQ');
    return TC.confirm({ title:'Đóng tất cả và thoát', w:500, msg:'Đóng <b>' + qty(docs.length, 'bản vẽ') + '</b> và thoát TrionCAD' + (dirty.length ? ' mà <b>không lưu ' + qty(ch, 'thay đổi') + '</b>?' : '?'), detail:docs.map(function(x){ return esc(x.name) + ': ' + (x.dirty ? '<b>' + qty(unsaved(x), 'thay đổi chưa lưu') + '</b>' : 'đã lưu'); }).join(' · ') + (dirty.length ? '. Thay đổi chưa lưu sẽ mất, không hoàn tác được.' : '.'), ok:dirty.length ? 'Đóng ' + docs.length + ' bản vẽ, bỏ ' + qty(ch, 'thay đổi') : 'Đóng ' + docs.length + ' bản vẽ và thoát', danger:!!dirty.length }).then(function(y){
      if (!y){ ctx.out('Đã hủy, không đóng bản vẽ nào.'); return; }
      docs.forEach(function(x){ x.dirty = false; A().closeDoc(x.id, true); });
      TC.toast('Đã đóng ' + qty(docs.length, 'bản vẽ') + (dirty.length ? ', bỏ ' + qty(ch, 'thay đổi chưa lưu') : '') + '. Bản thật thoát TrionCAD tại bước này; prototype giữ lại màn hình Start.', { type:'info', log:false, ms:8000 }); });
  };
  // MOVEBAK: thư mục chứa tệp .bak (biến MOVEBAK, lưu theo người dùng)
  function mbGet(){ return TC.SV && TC.SV.def('MOVEBAK') ? TC.SV.get('MOVEBAK') : TC.load('movebak', ''); }
  function mbSet(v){ if (TC.SV && TC.SV.def('MOVEBAK')) TC.SV.set('MOVEBAK', v, 'MOVEBAK'); else TC.save('movebak', v); }
  var FOLD = ['D:\\Backup', 'D:\\Backup\\TrionCAD', 'D:\\DuAn\\NhaO-2T\\_bak', '\\\\SRV-01\\Backup', '\\\\SRV-01\\Backup\\TrionCAD', '\\\\SRV-01\\Tai-nguyen', '\\\\SRV-01\\Tai-nguyen\\Shape', 'C:\\Users\\An\\Documents\\TrionCAD'];
  function folderPick(cur, title){
    return new Promise(function(res){ var sel = FOLD.indexOf(cur) >= 0 ? cur : FOLD[1];
      var dlg = TC.dialog({ cls:'mr2', id:'fpick', title:title || 'Chọn thư mục', code:'UI-MR2', w:480, help:false, onClose:function(v){ if (v !== true) res(null); },
        body:'<div class="stack"><span class="gh">Thư mục gợi ý</span><div class="tree mr2-fold" id="fp-t" tabindex="0" role="listbox" aria-label="Thư mục gợi ý">' + FOLD.map(function(f){ return '<button class="tn" type="button" role="option" data-fd="' + esc(f) + '" aria-selected="' + (f === sel) + '">' + ic('folder', 's12') + '<span class="mono">' + esc(f) + '</span></button>'; }).join('') + '</div>' + H.row('Thư mục', '<input class="inp mono" id="fp-p" value="' + esc(sel) + '" spellcheck="false" style="flex:1">', 64) + '</div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="fp-ok">Chọn thư mục này</button>' });
      var pick = function(b){ $$('[data-fd]', dlg.el).forEach(function(x){ x.setAttribute('aria-selected', String(x === b)); }); $('#fp-p', dlg.el).value = b.dataset.fd; };
      dlg.el.addEventListener('click', function(ev){ var b = ev.target.closest('[data-fd]'); if (b) pick(b); });
      dlg.el.addEventListener('dblclick', function(ev){ if (ev.target.closest('[data-fd]')) $('#fp-ok', dlg.el).click(); });
      TC.listKeys($('#fp-t', dlg.el), '[data-fd]', function(b, enter){ pick(b); if (enter) $('#fp-ok', dlg.el).click(); });
      $('#fp-ok', dlg.el).addEventListener('click', function(){ var v = $('#fp-p', dlg.el).value.trim().replace(/\\$/, ''); if (!/^([a-z]:\\|\\\\)./i.test(v)){ TC.ctip(dlg.el, 120, 300, 'Cần đường dẫn đầy đủ, ví dụ D:\\Backup'); return; } dlg.close(true); res(v); });
    });
  }
  IMPL.MOVEBAK = function(ctx){
    var cur = mbGet(); ctx.badge('MB');
    var set = function(v){ mbSet(v); ctx.out(v ? 'Từ lần lưu sau, tệp .bak chuyển vào ' + lbl(v) + ' (MOVEBAK). Bản vẽ trùng tên ở các thư mục khác nhau sẽ ghi đè .bak của nhau.' : 'Tệp .bak lưu cạnh bản vẽ như mặc định (MOVEBAK = "").'); };
    ctx.out('Thư mục tệp .bak hiện tại: ' + (cur ? lbl(cur) : 'cùng thư mục bản vẽ') + '.', 'faint');
    return ask(ctx, 'Thư mục mới cho tệp .bak, hoặc . để lưu cạnh bản vẽ', [['Duyệt', 'B']], cur || '.', { allowSpace:true }).then(function(v){ v = String(v).trim().replace(/\\$/, '');
      if (v === 'B') return folderPick(cur, 'Chọn thư mục chứa tệp .bak').then(function(f){ if (f) set(f); });
      if (v === '.' || v === '') return set('');
      if (!/^([a-z]:\\|\\\\)./i.test(v)){ ctx.out('Đường dẫn chưa đúng. Ví dụ: D:\\Backup\\TrionCAD hoặc \\\\SRV-01\\Backup.'); return; }
      set(v); });
  };
  // REDIR: tìm phần đầu đường dẫn (hoặc * cho mọi thư mục) trong xref, ảnh, font, shape, RText; xem trước tệp có tìm thấy không
  function pathItems(d){ var out = [];
    d.xrefs.forEach(function(x){ out.push({ k:x.type === 'PDF' ? 'PDF' : 'Xref', name:x.name, path:x.path, ref:x }); });
    (d.images || []).forEach(function(x){ out.push({ k:'Ảnh', name:x.name, path:x.path, ref:x }); });
    d.styles.forEach(function(s){ if (/[\\/]/.test(s.font || '')) out.push({ k:'Font', name:s.name, path:s.font, ref:s }); });
    shapeFiles(d).forEach(function(f){ out.push({ k:'Shape', name:f.name, path:f.path, ref:f }); });
    allEnts(d).forEach(function(e){ if (e.t === 'rtext' && e.src !== 'diesel') out.push({ k:'RText', name:base(e.path), path:e.path, e:e }); });
    return out; }
  function redirMap(p, from, to){ to = to.replace(/\\$/, ''); if (from === '*') return to + '\\' + base(p); var f = from.replace(/\\$/, ''); return p.toLowerCase().indexOf(f.toLowerCase()) === 0 && (p.length === f.length || p[f.length] === '\\') ? to + p.slice(f.length) : null; }
  var RD = { from:'D:\\Tai-nguyen', to:'\\\\SRV-01\\Tai-nguyen', kinds:{ Xref:true, 'Ảnh':true, PDF:true, Font:true, Shape:true, RText:true } };
  IMPL.REDIR = function(ctx){
    var d = D(), items = pathItems(d), off = {}, K = ['Xref', 'Ảnh', 'PDF', 'Font', 'Shape', 'RText']; ctx.badge('RD');
    if (!items.length){ ctx.out('Bản vẽ không có đường dẫn ngoài nào (xref, ảnh, font, shape, RText).'); return; }
    return new Promise(function(res){
      var dlg = TC.dialog({ cls:'mr2', id:'redir', title:'Đổi đường dẫn hàng loạt', cmd:'REDIR', code:'UI-MR2', w:960, onClose:function(){ res(); },
        body:'<div class="stack"><div class="grid2">' + H.row('Tìm', '<input class="inp mono" id="rd-from" value="' + esc(RD.from) + '" spellcheck="false" style="flex:1">', 70) + H.row('Thay bằng', '<input class="inp mono" id="rd-to" value="' + esc(RD.to) + '" spellcheck="false" style="flex:1"><button class="btn sm" type="button" id="rd-br">Duyệt…</button>', 70) + '</div>' +
          '<div class="hrow"><span class="faint mr2-s">Loại</span>' + K.map(function(k){ var n = items.filter(function(x){ return x.k === k; }).length; return H.chk('', esc(k) + ' <span class="faint">(' + n + ')</span>', RD.kinds[k] && n > 0, !n).replace('<input', '<input data-rk="' + k + '"'); }).join('') + '<span class="grow"></span><span class="faint mr2-s">Gõ * ở ô Tìm để thay mọi thư mục</span></div>' +
          '<div class="tblwrap" style="height:250px"><table class="tbl"><colgroup><col style="width:32px"><col style="width:62px"><col style="width:150px"><col><col><col style="width:120px"></colgroup><thead><tr><th class="ctr"></th><th>Loại</th><th>Tên</th><th>Đường dẫn hiện tại</th><th>Đường dẫn mới</th><th>Tệp</th></tr></thead><tbody id="rd-rows"></tbody></table></div><p class="mr2-s" id="rd-n"></p></div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="rd-ok">Đổi đường dẫn</button>' });
      var plan = function(){ var from = $('#rd-from', dlg.el).value.trim(), to = $('#rd-to', dlg.el).value.trim(); return items.map(function(x, i){ var np = from && to && RD.kinds[x.k] && !off[i] ? redirMap(x.path, from, to) : null; np = np && np !== x.path ? np : null; return { x:x, i:i, np:np, ok:exists(d, np || x.path) }; }); };
      function rows(){ var pl = plan(), ch = pl.filter(function(r){ return r.np; }), found = ch.filter(function(r){ return r.ok; }).length;
        $('#rd-rows', dlg.el).innerHTML = pl.map(function(r){ var x = r.x; return '<tr class="' + (r.np ? '' : 'mr2-dim') + '"><td class="ctr"><input type="checkbox" data-ri="' + r.i + '"' + (off[r.i] ? '' : ' checked') + (RD.kinds[x.k] ? '' : ' disabled') + ' aria-label="Đổi đường dẫn của ' + esc(x.name) + '"></td><td>' + esc(x.k) + '</td><td title="' + esc(x.name) + '">' + esc(x.name) + '</td><td class="mono mr2-path" title="' + esc(x.path) + '">' + esc(x.path) + '</td><td class="mono mr2-path' + (r.np ? ' mr2-new' : '') + '" title="' + esc(r.np || '') + '">' + esc(r.np || '—') + '</td><td>' + (r.np ? (r.ok ? '<span class="tag ok">Tìm thấy</span>' : '<span class="tag warn">Không tìm thấy</span>') : r.ok ? '<span class="tag">Đang dùng</span>' : '<span class="tag danger">Đang thiếu</span>') + '</td></tr>'; }).join('');
        $('#rd-n', dlg.el).innerHTML = ch.length ? qty(ch.length, 'đường dẫn') + ' sẽ đổi · <span class="ok">' + found + ' tìm thấy tệp ở đường dẫn mới</span>' + (ch.length - found ? ' · <span class="warn">' + (ch.length - found) + ' vẫn thiếu</span>' : '') : '<span class="faint">Không có đường dẫn nào bắt đầu bằng “' + esc($('#rd-from', dlg.el).value) + '”. Gõ * để thay mọi thư mục.</span>';
        var b = $('#rd-ok', dlg.el); b.disabled = !ch.length; b.textContent = ch.length ? 'Đổi ' + qty(ch.length, 'đường dẫn') : 'Đổi đường dẫn'; }
      dlg.el.addEventListener('input', function(ev){ if (ev.target.matches('#rd-from, #rd-to')) rows(); });
      dlg.el.addEventListener('change', function(ev){ var c = ev.target.closest('[data-ri]'); if (c) off[c.dataset.ri] = !c.checked; var k = ev.target.closest('[data-rk]'); if (k) RD.kinds[k.dataset.rk] = k.checked; rows(); });
      $('#rd-br', dlg.el).addEventListener('click', function(){ folderPick($('#rd-to', dlg.el).value, 'Chọn thư mục mới').then(function(f){ if (f){ $('#rd-to', dlg.el).value = f; rows(); } }); });
      $('#rd-ok', dlg.el).addEventListener('click', function(){ var ch = plan().filter(function(r){ return r.np; }); if (!ch.length) return; RD.from = $('#rd-from', dlg.el).value.trim(); RD.to = $('#rd-to', dlg.el).value.trim();
        ED.run('REDIR', function(){ var st = ED.state(function(){ return { x:d.xrefs, i:d.images, s:d.styles.map(function(s){ return s.font; }), f:shapeFiles(d) }; }, function(s){ d.xrefs = s.x; d.images = s.i; d.styles.forEach(function(x, k){ x.font = s.s[k]; }); d.shapeFiles = s.f; allEnts(d).forEach(function(x){ if (x.t === 'shape') TC.prepEnt(d, x); }); V().draw(); A().refreshPalettes(); });
          ch.forEach(function(r){ var x = r.x; if (x.e) ED.mod(x.e, function(y){ y.path = r.np; }); else if (x.k === 'Font') x.ref.font = r.np; else { x.ref.path = r.np; if (x.k !== 'Shape'){ x.ref.status = r.ok ? 'Đã nạp' : 'Không tìm thấy'; if (x.k !== 'Ảnh') x.ref.found = r.ok ? resolve(d, r.np) : ''; } } }); st(); });
        V().draw(); A().refreshPalettes(); dlg.close(true); var miss = ch.filter(function(r){ return !r.ok; });
        ctx.out('Đã đổi ' + qty(ch.length, 'đường dẫn') + ': ' + (ch.length - miss.length) + ' tìm thấy tệp' + (miss.length ? ', ' + miss.length + ' vẫn thiếu (' + miss.map(function(r){ return esc(r.x.name); }).join(', ') + ')' : '') + '. Ctrl+Z để hoàn tác.'); });
      rows();
    });
  };

  /* ================= Công cụ: XDATA, XDLIST, EDITTIME, SHP2BLK ================= */
  var XT = [['1000', 'Chuỗi'], ['1040', 'Số thực'], ['1041', 'Khoảng cách'], ['1042', 'Hệ số tỉ lệ'], ['1070', 'Số nguyên'], ['1071', 'Số nguyên dài'], ['1010', 'Điểm'], ['1005', 'Handle']];
  function xtName(c){ return (XT.filter(function(x){ return x[0] === String(c); })[0] || [c, 'Giá trị'])[1]; }
  function xdCheck(c, v){ v = String(v).trim(); c = String(c); if (c === '1000') return v.length > 255 ? 'Chuỗi tối đa 255 ký tự.' : null; if (v === '') return 'Nhập giá trị.'; if (/^104/.test(c)) return /^-?\d+(\.\d+)?$/.test(v) ? null : 'Cần một số, dấu thập phân là dấu chấm.'; if (c === '1070') return /^-?\d+$/.test(v) && Math.abs(Number(v)) <= 32767 ? null : 'Số nguyên từ -32768 đến 32767.'; if (c === '1071') return /^-?\d+$/.test(v) ? null : 'Cần một số nguyên.'; if (c === '1010') return /^-?\d+(\.\d+)?,-?\d+(\.\d+)?(,-?\d+(\.\d+)?)?$/.test(v.replace(/\s/g, '')) ? null : 'Điểm dạng x,y hoặc x,y,z.'; if (c === '1005') return /^[0-9A-F]{1,16}$/i.test(v) ? null : 'Handle là số hệ 16, ví dụ 2A4.'; return null; }
  function xdBytes(xd){ var n = 0; Object.keys(xd || {}).forEach(function(a){ n += 4 + a.length; (xd[a] || []).forEach(function(it){ var c = String(it[0]); n += 2 + (c === '1000' ? new TextEncoder().encode(String(it[1])).length + 2 : c === '1010' ? 24 : /^104/.test(c) ? 8 : c === '1070' ? 2 : c === '1005' ? 8 : 4); }); }); return n; }
  function handle(e){ return (0x200 + (e.id || 0)).toString(16).toUpperCase(); }
  function regapps(d){ return (d.regapps = d.regapps || ['ACAD']); }
  IMPL.XDATA = function(ctx){
    var d = D(); ctx.badge('XD');
    return ED.pick(ctx, 'Chọn đối tượng cần gắn xdata').then(function(r){ if (!r || !r.e) return; var e = r.e, apps = regapps(d).filter(function(a){ return !/^ACAD/.test(a); }), app = Object.keys(e.xdata || {})[0] || apps[0] || 'TRIONCAD', rows = [];
      var load = function(){ rows = ((e.xdata || {})[app] || [[1000, '']]).map(function(it){ return [String(it[0]), Array.isArray(it[1]) ? it[1].join(',') : String(it[1])]; }); };
      load();
      return new Promise(function(res){
        var dlg = TC.dialog({ cls:'mr2', id:'xdata', title:'Gắn xdata', cmd:'XDATA', code:'UI-MR2', w:660, onClose:function(){ res(); },
          body:'<div class="stack"><div class="hrow mr2-s"><span class="tag">' + esc(NM[e.t] || e.t) + '</span><span class="faint">handle</span><span class="mono">' + handle(e) + '</span><span class="faint">· layer</span>' + esc(e.layer) + '</div>' +
            '<div class="field"><label for="xd-app">Tên ứng dụng (regapp)</label><input class="inp mono" id="xd-app" list="xd-apps" value="' + esc(app) + '" spellcheck="false" style="width:260px"><datalist id="xd-apps">' + regapps(d).filter(function(a){ return !/^ACAD/.test(a); }).map(function(a){ return '<option value="' + esc(a) + '">'; }).join('') + '</datalist><span class="fmsg" id="xd-app-msg"></span></div>' +
            '<div class="tblwrap" style="max-height:220px"><table class="tbl mr2-xd"><colgroup><col style="width:190px"><col><col style="width:36px"></colgroup><thead><tr><th>Loại · mã DXF</th><th>Giá trị</th><th></th></tr></thead><tbody id="xd-rows"></tbody></table></div>' +
            '<div class="hrow"><button class="btn sm" type="button" id="xd-add">' + ic('plus') + 'Thêm giá trị</button><span class="grow"></span><span class="faint mr2-s" id="xd-b"></span></div></div>',
          foot:'<button class="btn ghost" type="button" id="xd-del">Gỡ xdata của ứng dụng này</button><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="xd-ok">Gắn xdata</button>' });
        function upd(){ var bad = rows.filter(function(it){ return xdCheck(it[0], it[1]); }).length, xd = Object.assign({}, e.xdata || {}); xd[app] = rows; var b = xdBytes(xd), ok = $('#xd-ok', dlg.el);
          $('#xd-b', dlg.el).textContent = 'Dung lượng xdata ' + TC.n(b) + ' / ' + TC.n(16383) + ' byte'; ok.textContent = 'Gắn ' + qty(rows.length, 'giá trị') + ' xdata'; ok.disabled = !rows.length || !!bad || b > 16383; $('#xd-del', dlg.el).hidden = !(e.xdata && e.xdata[app]); }
        function paint(){ $('#xd-rows', dlg.el).innerHTML = rows.map(function(it, i){ var err = xdCheck(it[0], it[1]); return '<tr><td><select class="inp" data-xc="' + i + '" aria-label="Loại giá trị ' + (i + 1) + '">' + XT.map(function(x){ return '<option value="' + x[0] + '"' + (x[0] === it[0] ? ' selected' : '') + '>' + x[1] + ' · ' + x[0] + '</option>'; }).join('') + '</select></td><td><input class="inp mono' + (err && it[1] !== '' ? ' err' : '') + '" data-xv="' + i + '" value="' + esc(it[1]) + '" spellcheck="false" aria-label="Giá trị ' + (i + 1) + '" title="' + esc(err || '') + '"></td><td class="ctr"><button class="ib sm" type="button" data-xr="' + i + '" aria-label="Xóa giá trị ' + (i + 1) + '">' + ic('x') + '</button></td></tr>'; }).join('') || '<tr><td colspan="3" class="faint">Chưa có giá trị. Bấm Thêm giá trị.</td></tr>'; upd(); }
        dlg.el.addEventListener('input', function(ev){ var v = ev.target.closest('[data-xv]'); if (!v) return; var i = Number(v.dataset.xv), err = xdCheck(rows[i][0], v.value); rows[i][1] = v.value; v.classList.toggle('err', !!err && v.value !== ''); v.title = err || ''; upd(); });
        dlg.el.addEventListener('change', function(ev){ var c = ev.target.closest('[data-xc]'); if (c){ rows[Number(c.dataset.xc)][0] = c.value; paint(); return; }
          if (ev.target.id === 'xd-app'){ var a = ev.target.value.trim().toUpperCase(); if (fieldErr(dlg, 'xd-app', !/^[A-Z0-9_\-$]{1,31}$/.test(a) ? 'Gồm chữ không dấu, số, _ - $, tối đa 31 ký tự.' : /^ACAD/.test(a) ? 'Tên bắt đầu bằng ACAD dành cho AutoCAD.' : null)){ app = a; ev.target.value = a; load(); paint(); } } });
        dlg.el.addEventListener('click', function(ev){ var x = ev.target.closest('[data-xr]'); if (x){ rows.splice(Number(x.dataset.xr), 1); paint(); } });
        $('#xd-add', dlg.el).addEventListener('click', function(){ rows.push(['1000', '']); paint(); var l = $$('[data-xv]', dlg.el).pop(); if (l) l.focus(); });
        $('#xd-del', dlg.el).addEventListener('click', function(){ ED.run('XDATA', function(){ ED.mod(e, function(x){ delete x.xdata[app]; if (!Object.keys(x.xdata).length) delete x.xdata; }); }); dlg.close(true); ctx.out('Đã gỡ xdata của ứng dụng ' + lbl(app) + ' khỏi ' + esc(NM[e.t] || e.t) + ' (handle ' + handle(e) + ').'); });
        $('#xd-ok', dlg.el).addEventListener('click', function(){ var bad = rows.map(function(it){ return xdCheck(it[0], it[1]); }).filter(Boolean)[0]; if (bad || !rows.length) return;
          var val = rows.map(function(it){ var c = it[0], v = it[1].trim(); return [Number(c), c === '1000' || c === '1005' ? v : c === '1010' ? v.replace(/\s/g, '').split(',').map(Number) : Number(v)]; });
          ED.run('XDATA', function(){ var st = ED.state(function(){ return regapps(d).slice(); }, function(s){ d.regapps = s; }); if (regapps(d).indexOf(app) < 0) d.regapps.push(app); ED.mod(e, function(x){ x.xdata = Object.assign({}, x.xdata || {}); x.xdata[app] = val; }); st(); });
          dlg.close(true); ctx.out('Đã gắn ' + qty(val.length, 'giá trị') + ' xdata của ứng dụng ' + lbl(app) + ' vào ' + esc(NM[e.t] || e.t) + ' (handle ' + handle(e) + '). Xem lại bằng XDLIST.'); });
        paint();
      }); });
  };
  function xdLines(e, app){ var xd = e.xdata || {}, apps = Object.keys(xd).filter(function(a){ return app === '*' || a === app; }), out = ['Đối tượng: ' + (NM[e.t] || e.t) + ' · handle ' + handle(e) + ' · layer ' + e.layer];
    apps.forEach(function(a){ out.push('* Ứng dụng: ' + a + ' (1001)'); xd[a].forEach(function(it){ out.push('  (' + it[0] + ') ' + xtName(it[0]) + ': ' + (Array.isArray(it[1]) ? it[1].map(function(x){ return TC.d(x); }).join(', ') : it[1])); }); });
    out.push('Tổng: ' + qty(apps.length, 'ứng dụng') + ', ' + qty(apps.reduce(function(s, a){ return s + xd[a].length; }, 0), 'giá trị') + ', ' + TC.n(xdBytes(xd)) + ' byte (giới hạn ' + TC.n(16383) + ' byte).'); return out; }
  IMPL.XDLIST = function(ctx){
    ctx.badge('XDL');
    return ED.pick(ctx, 'Chọn đối tượng', { locked:true }).then(function(r){ if (!r || !r.e) return; var e = r.e, apps = Object.keys(e.xdata || {});
      if (!apps.length){ ctx.out(esc(NM[e.t] || e.t) + ' (handle ' + handle(e) + ') không có xdata. Gắn bằng XDATA.'); return; }
      return (apps.length > 1 ? ask(ctx, 'Tên ứng dụng', apps.map(function(a, i){ return [a, String(i + 1)]; }).concat([['Tất cả', '*']]), '*') : Promise.resolve(apps[0])).then(function(app){
        if (/^\d+$/.test(app)) app = apps[Number(app) - 1] || app; app = app === '*' ? '*' : String(app).toUpperCase();
        if (app !== '*' && apps.indexOf(app) < 0){ ctx.out('Đối tượng không có xdata của ứng dụng “' + esc(app) + '”.'); return; }
        var L = xdLines(e, app); L.forEach(function(l, i){ ctx.out(i ? '<span class="mono">' + esc(l) + '</span>' : esc(l), i && i < L.length - 1 ? 'faint' : null); });
        if (L.length > 3) SC.textWindow(L.join('\n'), 'XDLIST · ' + (NM[e.t] || e.t) + ' ' + handle(e)); }); });
  };

  // EDITTIME: đếm thời gian sửa thật (bỏ khoảng không thao tác dài hơn ngưỡng), theo từng bản vẽ đang hoạt động
  var ET = Object.assign({ on:true, idle:5 }, TC.load('mr2et', {})), ACT = {};
  function etTick(){ var d = TC.app && TC.app.docs && TC.app.doc(); if (!d || !ET.on) return; var now = Date.now(), r = ACT[d.id] || (ACT[d.id] = { a:0, last:now, open:now }), gap = now - r.last; if (gap < ET.idle * 60000) r.a += gap; r.last = now; }
  ['mousemove', 'mousedown', 'keydown', 'wheel'].forEach(function(t){ document.addEventListener(t, etTick, { capture:true, passive:true }); });
  function hm(sec){ sec = Math.max(0, Math.round(sec)); var h = Math.floor(sec / 3600), m = Math.floor(sec % 3600 / 60), s = sec % 60, p = function(x){ return (x < 10 ? '0' : '') + x; }; return h ? h + ' giờ ' + p(m) + ' phút' : m + ' phút ' + p(s) + ' giây'; }
  function etOf(d){ var t = d.etime || (d.etime = { created:d.modified || TC.date(), total:0, active:0, days:[0, 0, 0, 0, 0, 0, 0] }), r = ACT[d.id] || { a:0, open:Date.now() }; return { t:t, sess:(Date.now() - r.open) / 1000, act:r.a / 1000 }; }
  IMPL.EDITTIME = function(ctx){
    var d = D(); ctx.badge('ET'); etTick(); var x0 = etOf(d);
    ctx.out('EDITTIME · ' + esc(d.name) + ': sửa thực ' + hm(x0.t.active + x0.act) + ', tổng thời gian mở ' + hm(x0.t.total + x0.sess) + (ET.on ? '' : ' (đồng hồ đang tạm dừng)') + '.');
    return new Promise(function(res){
      var timer = 0, dlg = TC.dialog({ cls:'mr2', id:'edittime', title:'Thời gian sửa bản vẽ', cmd:'EDITTIME', code:'UI-MR2', w:560, onClose:function(){ clearInterval(timer); res(); },
        body:'<div class="stack"><dl class="mr2-kv" id="et-kv"></dl><div class="grp"><div class="gh">7 ngày gần đây · thời gian sửa thực</div><div class="mr2-bars" id="et-bars"></div></div>' + H.row('Ngưỡng nghỉ', H.num('et-idle', String(ET.idle), { unit:'none', prec:0, pick:false, w:84 }) + '<span class="faint mr2-s">phút</span>', 92) + '<p class="faint mr2-s">Khoảng không thao tác dài hơn ngưỡng nghỉ không tính là thời gian sửa thực.</p></div>',
        foot:'<button class="btn ghost" type="button" id="et-reset">Đặt lại đồng hồ</button><span class="grow"></span><button class="btn" type="button" id="et-on"></button><button class="btn primary" type="button" data-x="close">Đóng</button>' });
      function paint(){ etTick(); var x = etOf(d), days = x.t.days.slice(); days[6] += x.act / 60; var mx = Math.max.apply(null, days.concat([60]));
        $('#et-kv', dlg.el).innerHTML = [['Bản vẽ', esc(d.name)], ['Tạo lúc', esc(x.t.created)], ['Lưu lần cuối', esc(d.modified || '—')], ['Tổng thời gian mở (TDINDWG)', hm(x.t.total + x.sess)], ['Thời gian sửa thực', '<b>' + hm(x.t.active + x.act) + '</b> ' + (ET.on ? '<span class="tag ok">Đang đếm</span>' : '<span class="tag warn">Tạm dừng</span>')], ['Phiên này', 'mở ' + hm(x.sess) + ' · sửa thực ' + hm(x.act)]].map(function(r){ return '<dt>' + r[0] + '</dt><dd>' + r[1] + '</dd>'; }).join('');
        $('#et-bars', dlg.el).innerHTML = days.map(function(m, i){ var dt = new Date(Date.now() - (6 - i) * 864e5); return '<div class="mr2-bar"><span class="faint">' + (i === 6 ? 'Hôm nay' : TC.date(dt).slice(0, 5)) + '</span><i><b style="width:' + Math.round(m / mx * 100) + '%"></b></i><span>' + (m >= 60 ? Math.floor(m / 60) + ' giờ ' + Math.round(m % 60) + ' p' : Math.round(m) + ' phút') + '</span></div>'; }).join('');
        $('#et-on', dlg.el).textContent = ET.on ? 'Tạm dừng đồng hồ' : 'Bật đồng hồ'; }
      $('#et-on', dlg.el).addEventListener('click', function(){ ET.on = !ET.on; TC.save('mr2et', ET); if (ET.on && ACT[d.id]) ACT[d.id].last = Date.now(); paint(); ctx.out(ET.on ? 'Đồng hồ EDITTIME: bật.' : 'Đồng hồ EDITTIME: tạm dừng.', 'faint'); });
      dlg.el.addEventListener('numchange', function(){ var v = Math.round(nv(dlg, 'et-idle', ET.idle)); if (fieldErr(dlg, 'et-idle', v >= 1 && v <= 120 ? null : 'Từ 1 đến 120 phút.')){ ET.idle = v; TC.save('mr2et', ET); } });
      $('#et-reset', dlg.el).addEventListener('click', function(){ var x = etOf(d); TC.confirm({ title:'Đặt lại đồng hồ', msg:'Đặt thời gian sửa thực của <b>' + esc(d.name) + '</b> về 0?', detail:'Bỏ ' + hm(x.t.active + x.act) + ' đã đếm. Tổng thời gian mở (TDINDWG) giữ nguyên. Ctrl+Z để hoàn tác.', ok:'Đặt lại về 0', danger:true }).then(function(y){ if (!y) return;
        ED.run('EDITTIME', function(){ var st = ED.state(function(){ return d.etime; }, function(s){ d.etime = s; }); d.etime.active = 0; d.etime.days = [0, 0, 0, 0, 0, 0, 0]; if (ACT[d.id]) ACT[d.id].a = 0; st(); }); paint(); ctx.out('Đã đặt lại đồng hồ thời gian sửa của ' + esc(d.name) + '.'); }); });
      paint(); timer = setInterval(function(){ if (document.contains(dlg.el)) paint(); else clearInterval(timer); }, 1000);
    });
  };

  // SHP2BLK: mọi shape cùng tên thành tham chiếu block (định nghĩa block gồm các nét của shape, đơn vị = chiều cao 1)
  IMPL.SHP2BLK = function(ctx, pre){
    var d = D(); ctx.badge('S2B');
    if (!allEnts(d).some(function(e){ return e.t === 'shape'; })){ ctx.out('Bản vẽ chưa có shape nào. Chèn shape bằng SHAPE hoặc tạo bằng MKSHAPE (chọn Chèn ở bước cuối).'); return; }
    return (pre && pre.t === 'shape' ? Promise.resolve({ e:pre }) : ED.pick(ctx, 'Chọn shape cần đổi thành block', { types:['shape'], bad:'Chọn một shape' })).then(function(r){ if (!r || !r.e) return; var nm = r.e.name, sd = shapeDef(d, nm);
      if (!sd){ ctx.out('Shape ' + esc(nm) + ' chưa nạp tệp .shx nên không đổi được. Nạp tệp hoặc đổi đường dẫn bằng REDIR.'); return; }
      return ctx.val('Tên block mới', nm).then(function(bn){ bn = String(bn).trim(); if (!bn || /[<>\/\\":;?*|,=`]/.test(bn)){ ctx.out('Tên block không được trống hoặc chứa < > / \\ " : ; ? * | , = `'); return; }
        return (d.blocks[bn] ? ctx.kw('Đã có block ' + bn + '. Thay định nghĩa block?', [['Có', 'Y'], ['Không', 'N']], 'N') : Promise.resolve('Y')).then(function(k){ if (k !== 'Y') return;
          var list = allEnts(d).filter(function(e){ return e.t === 'shape' && e.name === nm; }), lk = list.filter(function(e){ return (d.layerMap[e.layer] || {}).lck; }); list = list.filter(function(e){ return lk.indexOf(e) < 0; });
          ED.run('SHP2BLK', function(){ var st = ED.state(function(){ var b = {}; b[bn] = d.blocks[bn] || null; return b; }, function(s){ if (s[bn]) d.blocks[bn] = s[bn]; else delete d.blocks[bn]; allEnts(d).forEach(function(x){ if (x.t === 'insert' && x.name === bn) TC.prepEnt(d, x); }); V().draw(); A().refreshPalettes(); });
            d.blocks[bn] = { name:bn, desc:'Từ shape ' + nm + ' (' + sd.file + ')', ents:sd.segs.map(function(s){ return { t:'line', p1:s[0].slice(), p2:s[1].slice(), layer:'0' }; }) }; st();
            list.forEach(function(e){ var lst = ED.listOf(d, e), b = { t:'insert', name:bn, p:e.p.slice(), sx:e.h * (e.mir ? -1 : 1), sy:e.h, rot:e.rot || 0, layer:e.layer }; if (e.color != null) b.color = e.color; ED.del(e); ED.add(b, lst); }); });
          TC.cad.clearSel(); V().draw(); A().refreshPalettes();
          ctx.out('Đã đổi ' + qty(list.length, 'shape') + ' ' + esc(nm) + ' thành block <b>' + esc(bn) + '</b>' + (lk.length ? '; ' + lk.length + ' shape trên layer khóa giữ nguyên' : '') + '.'); }); }); });
  };

  /* ================= dữ liệu mẫu, danh mục ================= */
  // Banve-01: xdata tên phòng và cửa (XDLIST), tệp shape ký hiệu điện, đồng hồ EDITTIME; không đổi hình bản vẽ
  document.addEventListener('DOMContentLoaded', function(){ (A().docs || []).forEach(function(d){
    if (d.id !== 'd1'){ d.etime = d.etime || { created:d.modified || TC.date(), total:22040, active:15360, days:[0, 35, 64, 0, 0, 22, 0] }; return; }
    d.shapeFiles = [{ name:'ky-hieu-dien.shx', path:'D:\\Tai-nguyen\\Shape\\ky-hieu-dien.shx', defs:JSON.parse(JSON.stringify(SHX0)) }]; d.regapps = ['ACAD', 'ACAD_PSEXT', 'TRIONCAD', 'TCVN-PHONG'];
    d.ents.filter(function(e){ return e.t === 'text' && e.layer === 'KT-TEXT' && e.style === 'VNI' && e.p[0] < 20000; }).forEach(function(e, i){ var ar = d.ents.filter(function(x){ return x.t === 'text' && /^S = /.test(x.s) && Math.abs(x.p[0] - e.p[0]) < 1 && x.p[1] < e.p[1] && e.p[1] - x.p[1] < 500; })[0];
      e.xdata = { 'TCVN-PHONG':[[1000, e.s], [1040, ar ? Number(ar.s.replace(/[^\d.]/g, '')) : 0], [1070, 1], [1000, i % 2 ? 'Sàn gạch 600 × 600' : 'Sàn gỗ công nghiệp']] }; });
    d.ents.filter(function(e){ return e.t === 'insert' && e.name === 'CUA-DI-1C' && e.p[0] < 20000; }).forEach(function(e, i){ e.xdata = { TRIONCAD:[[1000, 'D' + (i + 1)], [1041, Math.round(Math.abs(e.sx))], [1000, 'Cửa gỗ công nghiệp'], [1070, 2026]] }; });
    d.etime = { created:'02/09/2026 08:15', total:149225, active:96530, days:[95, 142, 0, 188, 236, 61, 0] };
  }); });
  function closeMine(){ for (var i = 0; i < 8; i++){ var t = TC.topDialog(); if (!t) break; t.close(false); } }
  OPEN['UI-MR2'] = function(){
    closeMine(); var d = SC.cat.plan(); TC.cad.clearSel();
    var arc = d.ents.filter(function(e){ return e.t === 'arc' && e.mr2 === 'demo'; })[0];
    if (!arc) ED.run('ARC', function(){ arc = ED.add(ED.make({ t:'arc', c:[19500, -8200], r:6000, a0:58, a1:122, layer:'KT-GHICHU', mr2:'demo' })); });
    A().view.zoomTo([16000, -3600, 23000, -1300]);
    setTimeout(function(){ var t = $$('.rb-tab').filter(function(b){ return b.textContent.trim() === 'Mở rộng'; })[0]; if (t) t.click(); SC.cat.flash('#rb'); TC.cmd.run('ARCTEXT', arc);
      // hộp thoại nằm bên phải: đưa cung vào giữa phần vùng vẽ còn trống bên trái
      var dl = TC.topDialog(), cv = $('#cv').getBoundingClientRect(); if (dl){ var free = dl.el.getBoundingClientRect().left - cv.left - 16; if (free > 420){ var v = V(), vw = v.vw(); vw.s = Math.min(free / 8200, (cv.height - 60) / 3600); vw.cx = 19500 + (cv.width / 2 - free / 2) / vw.s; vw.cy = -2500; v.draw(); TC.emit('view'); } }
      TC.toast('Tab Mở rộng phần P2: liệt kê lồng, block thành xref, chữ theo cung, che nền chữ, chữ từ tệp, MOCORO, MSTRETCH, đường ngắt, SuperHatch, tạo shape và linetype, gióng và đồng bộ viewport, xuất nhập kiểu kích thước, mở lại bản đã lưu, đổi đường dẫn, xdata, thời gian sửa.', { type:'info', log:false, ms:9000 }); }, 80);
  };
})();
