/* TrionCAD prototype · Đợt 4: block và tham chiếu (G04 Block Editor, G08 quản lý attribute, G13 gắn PDF, G14 nhập PDF, G16 thay block, G17 DesignCenter, G18 Tool Palettes, G19 sửa tại chỗ) và tab Mở rộng P1. */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic, G = TC.geo, IMPL = TC.cmd.impl, ED = TC.ed, SC = TC.screens, H = SC.h;
  function A(){ return TC.app; } function V(){ return TC.app.view; } function D(){ return TC.app.doc(); }
  var OPEN = SC.open4 = SC.open4 || {}; // mã màn hình → hàm mở (danh mục gọi khi bấm); tiện ích mở ở SC.cat (D1, PRO, MOD, RUN, plan, pick, flash)
  var T = ED.T, P = TC.PALETTES;
  function num(v, d){ var n = TC.parseNum(v, 'none'); return n.err ? d : n.v; }
  function rp(x0, y0, x1, y1){ return [[x0, y0], [x1, y0], [x1, y1], [x0, y1]]; }
  function later(what){ TC.toast('<b>' + esc(what) + '</b> có ở sau v1.0.', { type:'info', log:false }); }
  function bad(msg){ var m = V().mouse || [140, 140]; TC.ctip($('#cv'), m[0], m[1], msg); }
  function nowStr(){ return TC.date() + ' ' + new Date().toTimeString().slice(0, 5); }
  var BADNAME = /[<>\/\\":;?*|,=`]/, BADMSG = 'Tên không chứa < > / \\ " : ; ? * | , = `';

  /* ---------- biến đổi của tham chiếu: ma trận [a, b, c, d, e, f] (x' = ax + cy + e, y' = bx + dy + f) ---------- */
  function mIns(e){ var r = (e.rot || 0) * Math.PI / 180, c = Math.cos(r), s = Math.sin(r), sx = e.sx || 1, sy = e.sy || sx; return [sx * c, sx * s, -sy * s, sy * c, e.p[0], e.p[1]]; }
  function mMul(M, N){ return [M[0] * N[0] + M[2] * N[1], M[1] * N[0] + M[3] * N[1], M[0] * N[2] + M[2] * N[3], M[1] * N[2] + M[3] * N[3], M[0] * N[4] + M[2] * N[5] + M[4], M[1] * N[4] + M[3] * N[5] + M[5]]; }
  function mInv(M){ var k = M[0] * M[3] - M[1] * M[2]; return [M[3] / k, -M[1] / k, -M[2] / k, M[0] / k, (M[2] * M[5] - M[3] * M[4]) / k, (M[1] * M[4] - M[0] * M[5]) / k]; }
  function mApply(M, p){ return [M[0] * p[0] + M[2] * p[1] + M[4], M[1] * p[0] + M[3] * p[1] + M[5]]; }
  // dạng biến đổi cho ED.xform; định thức âm là đối xứng (cửa đi lật chiều…)
  function tOf(M){ var k = M[0] * M[3] - M[1] * M[2], a = Math.atan2(M[1], M[0]) * 180 / Math.PI, sx = Math.hypot(M[0], M[1]); return { f:function(p){ return mApply(M, p); }, k:Math.sqrt(Math.abs(k)), rot:a, mir:k < 0, th:a / 2, nonuni:Math.abs(sx - Math.hypot(M[2], M[3])) > 1e-6 * sx, m:[M[0], M[1], M[2], M[3], M[4], M[5]] }; }
  // biến đổi một bản sao đối tượng; tỉ lệ không đều thì cung, tròn thành polyline (như EXPLODE)
  function xfEnt(c, t){
    if (t.nonuni && (c.t === 'arc' || c.t === 'circle')){ var cir = c.t === 'circle', sw = cir ? 360 : ED.sweep(c.a0, c.a1), a0 = cir ? 0 : c.a0, n = cir ? 24 : 25, pts = []; for (var i = 0; i < n; i++) pts.push(t.f(ED.polar(c.c, c.r, a0 + sw * i / 24))); var o = { t:'pline', pts:pts, closed:cir, layer:c.layer }; if (c.color != null) o.color = c.color; return o; }
    return ED.xform(c, t);
  }

  /* ---------- tiện ích bản vẽ ---------- */
  function modelEnts(d){ return d.bedit ? d.bedit.ents : d.ents; }
  function allIns(d, name){ var out = []; [modelEnts(d)].concat(d.layouts.map(function(L){ return L.ents; })).forEach(function(l){ l.forEach(function(e){ if (e.t === 'insert' && e.name === name) out.push(e); }); }); return out; }
  function userBlocks(d){ return Object.keys(d.blocks).filter(function(n){ return !/^\*|^A\$/.test(n) && !d.blocks[n].xref; }); }
  function attDefs(bd){ return bd ? bd.ents.filter(function(x){ return x.t === 'attdef'; }) : []; }
  function cleanDef(b){ return b ? JSON.parse(JSON.stringify(Object.assign({}, b, { ents:b.ents.map(ED.clean), cons:b.cons ? b.cons.map(ED.clean) : undefined }))) : null; }
  // định nghĩa block là dữ liệu ngoài danh sách đối tượng: chụp trước/sau để hoàn tác (ED.state)
  function defState(d, names){ return ED.state(function(){ var o = {}; names.forEach(function(n){ o[n] = cleanDef(d.blocks[n]); }); return o; }, function(s){ Object.keys(s).forEach(function(n){ if (s[n]) d.blocks[n] = s[n]; else delete d.blocks[n]; }); TC.prepDoc(d); V().draw(); A().refreshPalettes(); }); }
  function layerState(d){ return ED.state(function(){ return { L:d.layers.map(function(l){ return Object.assign({}, l); }), cl:d.clayer }; }, function(s){ d.layers = s.L; d.layerMap = {}; d.layers.forEach(function(l){ d.layerMap[l.name] = l; }); d.clayer = s.cl; A().refreshPalettes(); A().renderRibbon(); }); }
  function addLayer(d, name, src){ if (d.layerMap[name]) return false; var l = Object.assign({ color:7, lt:'Continuous', lw:.25, desc:'', on:true, frz:false, lck:false, plot:true, vpfrz:false }, src || {}, { name:name, count:0 }); d.layers.push(l); d.layerMap[name] = l; return true; }
  function thumbDoc(){ var d = D() || A().docs[0]; return { blocks:{}, layerMap:d.layerMap, styleMap:d.styleMap, layers:d.layers, seq:0, byId:{}, frame:2 }; } // vẽ thumbnail block thư viện mà không thêm vào bản vẽ
  var TD = null; function libThumb(c, name, src){ if (src && src.blocks && src.blocks[name]) return SC.blockThumb(c, src, name); if (D() && D().blocks[name]) return SC.blockThumb(c, D(), name); TD = TD || thumbDoc(); SC.blockThumb(c, TD, name); }
  // chép định nghĩa block (kèm block lồng) từ bản vẽ khác hoặc thư viện; gọi trong ED.run, trả tên các định nghĩa mới
  function copyDef(d, name, src){
    if (d.blocks[name]) return [];
    var made = [], go = function(n){ if (d.blocks[n] || made.indexOf(n) >= 0) return; var b = src && src.blocks[n]; if (!b){ var done = defState(d, [n]); if (SC.ensureBlock(d, n)){ made.push(n); done(); } return; } b.ents.forEach(function(x){ if (x.t === 'insert') go(x.name); }); var done2 = defState(d, [n]); d.blocks[n] = Object.assign(cleanDef(b), { ents:b.ents.map(function(x){ var c = ED.clean(x); c.id = ++d.seq; return c; }) }); delete d.blocks[n].xref; made.push(n); done2(); };
    go(name); return made;
  }

  /* ================= Block Editor UI-G04 (BEDIT, BSAVE, BCLOSE) ================= */
  // môi trường sửa: danh sách đối tượng của bản vẽ đổi sang bản sao định nghĩa block (gốc tại 0,0); lệnh vẽ, sửa chạy như thường
  function snapEnts(list){ return JSON.stringify(list.map(function(e){ var c = ED.clean(e); delete c.id; return c; })); }
  function modeAttr(){ var d = D(); document.documentElement.toggleAttribute('data-bedit', !!(d && d.bedit && A().active !== 'start')); }
  function beEnter(d, name){
    if (d.refedit){ TC.cmdline.msg('Đang sửa tại chỗ: lưu hoặc hủy thay đổi trước khi mở Block Editor.'); return; }
    if (d.space !== 'model') A().setSpace('model');
    var bd = d.blocks[name], src = bd ? bd.ents.concat(bd.cons || []) : [];
    d.bedit = { name:name, ents:d.ents, undo:d.undo || [], redo:d.redo || [], view:d.view, hist:d.viewHist.slice(), origs:{} };
    d.bedit.origs[name] = cleanDef(bd);
    d.ents = src.map(function(x){ var c = ED.clean(x); c.id = ++d.seq; return c; }); d.undo = []; d.redo = []; d.sel = []; d.found = []; TC.prepDoc(d);
    d.bedit.saved = snapEnts(d.ents);
    beChrome(d); fitView(d, d.ents.length ? d.ext : [-2000, -2000, 2000, 2000]); TC.emit('sel');
    TC.cmdline.msg('Block Editor: <b>' + esc(name) + '</b>' + (bd ? ' · ' + d.ents.length + ' đối tượng' : ' · block mới') + '. Vẽ, sửa như bình thường; BSAVE để lưu, BCLOSE để đóng.');
  }
  SC.bedit = function(name){ var d = D(); if (d && !TC.isViewer()) beEnter(d, name); };
  // vừa khung nhìn với vùng b, chừa chỗ cho các banner phía trên vùng vẽ
  function fitView(d, b){ var v = V(), bh = ($('#banners') || {}).offsetHeight || 0, w = Math.max(b[2] - b[0], 1e-6), h = Math.max(b[3] - b[1], 1e-6), s = Math.min(v.W / (w * 1.3), (v.H - bh) / (h * 1.3)); v.pushHist(); d.view = { cx:(b[0] + b[2]) / 2, cy:(b[1] + b[3]) / 2 + bh / 2 / s, s:s }; v.draw(); TC.emit('view'); }
  function beChrome(d){
    var B = d.bedit; if (!B) return; var n = allIns(d, B.name).length;
    A().banner(d, { id:'bedit', icon:'bedit', html:'Đang sửa block <b>' + esc(B.name) + '</b>' + (B.origs[B.name] ? ' · ' + n + ' tham chiếu sẽ cập nhật khi lưu' : ' · block mới, tạo khi lưu') + '. Gốc tọa độ là điểm gốc của block.', acts:[{ label:'Lưu block', primary:true, fn:function(){ TC.cmd.run('BSAVE'); } }, { label:'Đóng Block Editor', fn:function(){ TC.cmd.run('BCLOSE'); } }], onClose:function(){ TC.cmd.run('BCLOSE'); } });
    modeAttr(); A().setCtxTab(beTab(d), A().ctxTab && A().ctxTab.id === 'bedit'); V().draw(); A().refreshPalettes();
  }
  function p2(id, label, icon, t){ return { t:t || 's', label:label, icon:icon, id:'be-p2-' + id, desc:'Dynamic block (UI-G05): có ở sau v1.0.', act:function(){ later(label.replace(/\n/g, ' ')); } }; }
  function beTab(d){ return { id:'bedit', name:'Block Editor', ctx:true, panels:[
    { name:'Mở/Lưu', icon:'save', prio:1, items:[ { t:'lg', label:'Lưu\nblock', icon:'save', id:'be-save', cmd:'BSAVE', desc:'Lưu định nghĩa block. Mọi tham chiếu cập nhật khi đóng Block Editor.' }, { t:'col', items:[ { t:'s', label:'Sửa block khác', icon:'bedit', id:'be-other', cmd:'BEDIT' }, { t:'s', label:'Lưu thành…', icon:'saveas', id:'be-saveas', act:function(){ beSaveAs(D()); } } ] } ] },
    { name:'Hình học', icon:'line', prio:3, items:[ { t:'col', items:[ { t:'s', label:'Đường thẳng', icon:'line', id:'be-line', cmd:'LINE' }, { t:'s', label:'Polyline', icon:'pline', id:'be-pl', cmd:'PLINE' }, { t:'s', label:'Đường tròn', icon:'circle', id:'be-c', cmd:'CIRCLE' } ] }, { t:'col', items:[ { t:'s', label:'Đặt điểm gốc', icon:'idpoint', id:'be-base', cmd:'BASE', desc:'Chỉ một điểm: điểm đó thành gốc của block. Tham chiếu giữ điểm chèn, hình học dịch theo.' }, p2('auto', 'Tự ràng buộc', 'snap'), p2('geo', 'Ràng buộc hình học', 'osnap') ] } ] },
    { name:'Quản lý', icon:'gear', prio:2, items:[ { t:'col', items:[ { t:'s', label:'Xóa', icon:'erase', id:'be-erase', cmd:'ERASE' }, { t:'s', label:'Đối tượng dựng hình', icon:'construct', id:'be-cons', desc:'Chọn đối tượng rồi bấm: đối tượng dựng hình chỉ hiện trong Block Editor, không hiện ở tham chiếu. Bấm lần nữa để bỏ.', act:beCons }, { t:'s', label:'Định nghĩa attribute', icon:'attedit', id:'be-att', cmd:'ATTDEF' } ] }, { t:'col', items:[ p2('cs', 'Hiện ràng buộc', 'eye'), p2('pm', 'Quản lý tham số', 'list'), p2('pal', 'Palette tạo block', 'blocks') ] } ] },
    { name:'Tham số, Action · sau v1.0', icon:'param', prio:4, items:[ p2('par', 'Tham số', 'param', 'lg'), p2('act', 'Action', 'move', 'lg'), p2('tbl', 'Bảng\nthuộc tính', 'table', 'lg') ] },
    { name:'Visibility · sau v1.0', icon:'eye', prio:5, items:[ { t:'col', items:[ p2('vm', 'Chế độ hiển thị', 'eye'), p2('vs', 'Trạng thái hiển thị', 'layers'), p2('vh', 'Hiện, ẩn theo trạng thái', 'eyeoff') ] } ] },
    { name:'Thử · sau v1.0', icon:'play', prio:5, items:[ p2('test', 'Thử\nblock', 'play', 'lg') ] },
    { name:'Đóng', icon:'close', prio:1, items:[ { t:'lg', label:'Đóng\nBlock Editor', icon:'close', id:'be-close', cmd:'BCLOSE', desc:'Đóng Block Editor; hỏi lưu nếu có thay đổi.' } ] }
  ] }; }
  function beCons(){ var d = D(), ids = ED.editable(d.sel.slice()); if (!d.bedit) return; if (!ids.length){ TC.cmdline.msg('Chọn đối tượng trước, rồi bấm <b>Đối tượng dựng hình</b>.'); return; }
    var on = !ids.every(function(id){ return d.byId[id] && d.byId[id].cons; }); ED.run('BCONSTRUCTION', function(){ ids.forEach(function(id){ ED.mod(d.byId[id], function(x){ if (on) x.cons = true; else delete x.cons; }); }); }); TC.cad.clearSel(); V().draw();
    TC.cmdline.msg(ids.length + ' đối tượng ' + (on ? 'thành đối tượng dựng hình (hiện mờ, không hiện ở tham chiếu).' : 'trở lại đối tượng thường.')); }
  // block không được chứa chính nó (kể cả qua block lồng)
  function selfRef(d, name, ents){ var seen = {}; function has(list){ return list.some(function(e){ if (e.t !== 'insert') return false; if (e.name === name) return true; if (seen[e.name]) return false; seen[e.name] = 1; var b = d.blocks[e.name]; return !!b && has(b.ents); }); } return has(ents); }
  function beSave(d, asName){
    var B = d.bedit, nm = asName || B.name;
    if (selfRef(d, nm, d.ents)){ TC.toast('Block <b>' + esc(nm) + '</b> không thể chứa chính nó. Xóa tham chiếu ' + esc(nm) + ' trong Block Editor rồi lưu lại.', { type:'err', log:false }); return false; }
    var list = d.ents.map(function(e){ var c = ED.clean(e); c.id = ++d.seq; return c; }), cons = list.filter(function(x){ return x.cons; });
    if (!(nm in B.origs)) B.origs[nm] = cleanDef(d.blocks[nm]);
    d.blocks[nm] = Object.assign({ name:nm, desc:'' }, d.blocks[nm] || (asName ? { desc:(d.blocks[B.name] || {}).desc || '' } : {}), { name:nm, ents:list.filter(function(x){ return !x.cons; }), cons:cons.length ? cons : undefined });
    B.name = nm; B.saved = snapEnts(d.ents); var n = allIns(d, nm).length;
    TC.cmdline.msg('Đã lưu block <b>' + esc(nm) + '</b>' + (n ? ': ' + n + ' tham chiếu cập nhật khi đóng Block Editor.' : '.'));
    TC.toast('Đã lưu block <b>' + esc(nm) + '</b>' + (n ? ' · ' + n + ' tham chiếu' : ''), { log:false, ms:2500 }); beChrome(d); return true;
  }
  function beSaveAs(d){ var B = d && d.bedit; if (!B) return; SC.ask('Lưu block thành', 'Tên block mới', B.name + '-2', function(v){ return BADNAME.test(v) ? BADMSG : d.blocks[v] ? 'Đã có block “' + v + '”. Chọn tên khác.' : null; }, 'UI-G04').then(function(v){ if (v && beSave(d, v)) TC.cmdline.msg('Block Editor đang sửa block mới <b>' + esc(v) + '</b>; block cũ giữ nguyên.'); }); }
  function beClose(d){
    var B = d && d.bedit; if (!B) return Promise.resolve(true);
    var ch = snapEnts(d.ents) !== B.saved;
    var ask = ch ? TC.confirm({ title:'Đóng Block Editor', msg:'Block <b>' + esc(B.name) + '</b> có thay đổi chưa lưu. Lưu trước khi đóng?', detail:B.origs[B.name] ? 'Lưu sẽ cập nhật ' + allIns(d, B.name).length + ' tham chiếu trong bản vẽ.' : 'Block mới chỉ được tạo khi lưu.', ok:'Lưu và đóng', alt:'Bỏ thay đổi', cancel:'Tiếp tục sửa' }) : Promise.resolve('close');
    return ask.then(function(r){ if (!r){ beChrome(d); return false; } if (r === true && !beSave(d)) return false; beExit(d); return true; });
  }
  function beExit(d){
    var B = d.bedit; if (!B) return;
    d.ents = B.ents; d.undo = B.undo; d.redo = B.redo; d.view = B.view; d.viewHist = B.hist; d.bedit = null; d.sel = []; TC.prepDoc(d);
    // một bước hoàn tác cho cả phiên sửa block: định nghĩa trước và sau
    var ops = Object.keys(B.origs).map(function(n){ var a = cleanDef(d.blocks[n]); return JSON.stringify(a) === JSON.stringify(B.origs[n]) ? null : { k:'st', b:{ n:n, def:B.origs[n] }, a:{ n:n, def:a }, set:function(s){ if (s.def) d.blocks[s.n] = s.def; else delete d.blocks[s.n]; TC.prepDoc(d); V().draw(); A().refreshPalettes(); } }; }).filter(Boolean);
    if (ops.length){ d.undo.push({ label:'BEDIT', ops:ops }); d.redo = []; }
    A().unbanner(d, 'bedit'); modeAttr(); if (A().ctxTab && A().ctxTab.id === 'bedit') A().setCtxTab(null); ED.ctxRefresh();
    if (ops.length) ED.changed(d); else { V().draw(); TC.emit('sel'); } A().refreshPalettes();
    TC.cmdline.msg(ops.length ? 'Đã đóng Block Editor: ' + ops.map(function(o){ return '<b>' + esc(o.a.n) + '</b> (' + allIns(d, o.a.n).length + ' tham chiếu)'; }).join(', ') + ' đã cập nhật. Ctrl+Z để hoàn tác cả phiên sửa.' : 'Đã đóng Block Editor.');
  }
  IMPL.BSAVE = function(ctx){ var d = D(); if (!d.bedit){ ctx.out('BSAVE dùng trong Block Editor. Mở Block Editor bằng BEDIT.'); return; } if (!d.ents.length) return TC.confirm({ title:'Lưu block rỗng', msg:'Block <b>' + esc(d.bedit.name) + '</b> không còn đối tượng nào. Vẫn lưu?', detail:'Các tham chiếu của block sẽ không còn hiện gì.', ok:'Lưu block rỗng' }).then(function(y){ if (y) beSave(d); }); beSave(d); };
  IMPL.BCLOSE = function(ctx){ var d = D(); if (!d.bedit){ ctx.out('Block Editor đang không mở.'); return; } return beClose(d); };
  // BASE trong Block Editor: dời gốc block (ngoài Block Editor: điểm gốc chèn của bản vẽ như cũ)
  var base0 = IMPL.BASE;
  IMPL.BASE = function(ctx){ var d = D(); if (!d.bedit) return base0(ctx);
    return ctx.point('Điểm gốc mới của block').then(function(r){ if (!r.p) return; var p = r.p, ids = d.ents.map(function(e){ return e.id; }); ED.run('BASE', function(){ ED.apply(ids, T.move(-p[0], -p[1])); }); var v = d.view; v.cx -= p[0]; v.cy -= p[1]; V().draw(); ctx.out('Đã dời gốc block tới ' + TC.d(p[0]) + ', ' + TC.d(p[1]) + '. Tham chiếu giữ điểm chèn, hình học dịch theo.'); }); };

  // hộp chọn block để sửa: danh sách, tìm, sắp xếp, xem trước; gõ tên chưa có để tạo block mới
  function beDialog(pre, ins){
    var d = D(), cur = pre && d.blocks[pre] ? pre : null, q = '', sortBy = 'Tên A–Z';
    return new Promise(function(res){
      var dlg = TC.dialog({ id:'bedit', title:'Sửa định nghĩa block', cmd:'BEDIT', code:'UI-G04', w:660, onClose:function(r){ res(r === true ? cur : null); },
        body:'<div class="bed"><div class="stack" style="gap:6px"><div class="hrow" style="flex-wrap:nowrap"><label class="search grow">' + ic('search') + '<input id="be-q" placeholder="Tìm, hoặc nhập tên block mới" spellcheck="false" aria-label="Tìm block"></label>' + H.sel('be-sort', ['Tên A–Z', 'Dùng nhiều'], sortBy, 118) + '</div><div class="tblwrap bel" id="be-l" role="listbox" tabindex="0" aria-label="Block trong bản vẽ"></div><span class="fmsg hint" id="be-m"></span></div>' +
          '<div class="stack" style="gap:8px">' + H.cv('be-pv', 300, 200) + '<div class="frow bedet" id="be-det"></div></div></div>',
        foot:(ins ? '<button class="btn ghost" type="button" id="be-ref">' + ic('bedit') + 'Sửa tại chỗ (REFEDIT)</button>' : '') + '<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="be-ok">Sửa block</button>' });
      function names(){ var F = TC.fold(q), cnt = {}, list = userBlocks(d).filter(function(n){ return !F || TC.fold(n + ' ' + (d.blocks[n].desc || '')).indexOf(F) >= 0; }); list.forEach(function(n){ cnt[n] = allIns(d, n).length; }); return list.sort(function(a, b){ return sortBy === 'Dùng nhiều' ? cnt[b] - cnt[a] || a.localeCompare(b) : a.localeCompare(b); }).map(function(n){ return [n, cnt[n]]; }); }
      function render(){
        var list = names(), typed = q.trim(), isNew = typed && !d.blocks[typed] && !list.length;
        if (cur && !list.some(function(x){ return x[0] === cur; })) cur = list.length ? list[0][0] : null;
        if (!cur && list.length && !isNew) cur = list[0][0];
        $('#be-l', dlg.el).innerHTML = list.length ? list.map(function(x){ var bd = d.blocks[x[0]]; return '<button class="bei" type="button" role="option" data-bn="' + esc(x[0]) + '" aria-selected="' + (x[0] === cur) + '"><span class="nm">' + esc(x[0]) + '</span>' + (attDefs(bd).length ? '<span class="tag">attribute</span>' : '') + (bd.lib ? '<span class="tag">thư viện</span>' : '') + '<span class="mono" data-tip="Số tham chiếu">' + x[1] + '</span></button>'; }).join('')
          : '<div class="empty">' + ic('blocks') + (typed ? '<b>Chưa có block “' + esc(typed) + '”</b>Bấm Tạo block để vẽ block mới trong Block Editor.' : '<b>Bản vẽ chưa có block</b>Gõ tên để tạo block mới, hoặc mở Thư viện TCVN.') + '</div>';
        var bad = isNew && BADNAME.test(typed), m = $('#be-m', dlg.el), ok = $('#be-ok', dlg.el);
        m.className = 'fmsg ' + (bad ? 'err' : 'hint'); m.innerHTML = bad ? ic('warn', 'xs') + BADMSG : list.length + ' block · ↑↓ để chọn, Enter để sửa';
        ok.textContent = isNew ? 'Tạo block “' + typed + '”' : 'Sửa block'; ok.disabled = !!bad || (!isNew && !cur);
        if (isNew) cur = null;
        detail(isNew ? typed : cur, isNew);
      }
      function detail(n, isNew){
        var c = $('#be-pv', dlg.el), x = c.getContext('2d');
        if (!n || isNew){ x.fillStyle = TC.cssv('--c-canvas'); x.fillRect(0, 0, c.width, c.height); x.fillStyle = '#8C949E'; x.font = '12px ' + TC.cssv('--f-ui'); x.textAlign = 'center'; x.fillText(isNew ? 'Block mới: vẽ trong Block Editor rồi Lưu block' : 'Chọn một block để xem trước', c.width / 2, c.height / 2); $('#be-det', dlg.el).innerHTML = ''; return; }
        SC.blockThumb(c, d, n); var bd = d.blocks[n], ins2 = allIns(d, n), inM = ins2.filter(function(e){ return !e._paper; }).length, at = attDefs(bd);
        $('#be-det', dlg.el).innerHTML = '<span class="lab">Mô tả</span><span>' + esc(bd.desc || '—') + '</span><span class="lab">Tham chiếu</span><span>' + ins2.length + (ins2.length ? ' (Model ' + inM + ', layout ' + (ins2.length - inM) + ')' : '') + '</span><span class="lab">Đối tượng</span><span>' + bd.ents.length + (bd.cons ? ' · ' + bd.cons.length + ' dựng hình' : '') + '</span><span class="lab">Attribute</span><span>' + (at.length ? at.length + ' tag: <span class="mono">' + esc(at.slice(0, 4).map(function(a){ return a.tag; }).join(', ')) + (at.length > 4 ? '…' : '') + '</span>' : 'Không') + '</span><span class="lab">Đơn vị</span><span>Milimét</span>';
      }
      var lst = $('#be-l', dlg.el);
      lst.addEventListener('click', function(e){ var b = e.target.closest('[data-bn]'); if (!b) return; cur = b.dataset.bn; $$('[data-bn]', lst).forEach(function(x){ x.setAttribute('aria-selected', String(x === b)); }); detail(cur); $('#be-ok', dlg.el).disabled = false; });
      lst.addEventListener('dblclick', function(e){ if (e.target.closest('[data-bn]')) $('#be-ok', dlg.el).click(); });
      TC.listKeys(lst, '[data-bn]', function(b, enter){ if (enter){ $('#be-ok', dlg.el).click(); return; } b.click(); });
      $('#be-q', dlg.el).addEventListener('input', function(){ q = this.value; render(); });
      $('#be-q', dlg.el).addEventListener('keydown', function(e){ if (e.key === 'ArrowDown'){ e.preventDefault(); lst.focus(); } });
      $('#be-sort', dlg.el).addEventListener('change', function(){ sortBy = this.value; render(); });
      $('#be-ok', dlg.el).addEventListener('click', function(){ var typed = q.trim(); if (!cur && typed && !d.blocks[typed] && !BADNAME.test(typed)) cur = typed; if (cur) dlg.close(true); });
      var rf = $('#be-ref', dlg.el); if (rf) rf.addEventListener('click', function(){ dlg.close(false); setTimeout(function(){ TC.cmd.run('REFEDIT', ins); }, 0); });
      render(); setTimeout(function(){ var s = $('[aria-selected="true"]', lst); if (s) s.scrollIntoView({ block:'nearest' }); }, 0);
    });
  }
  IMPL.BEDIT = function(ctx, pre){
    var d = D(), ins = pre && pre.t === 'insert' ? pre : null, name = ins ? ins.name : typeof pre === 'string' ? pre.toUpperCase() : null;
    if (d.refedit){ ctx.out('Đang sửa tại chỗ (REFEDIT): lưu hoặc hủy thay đổi trước.'); return; }
    if (!name && d.sel.length === 1 && d.byId[d.sel[0]] && d.byId[d.sel[0]].t === 'insert' && !d.byId[d.sel[0]].xref) name = d.byId[d.sel[0]].name;
    return beDialog(name, ins).then(function(n){ if (!n) return; if (!d.bedit) return beEnter(d, n); if (n === d.bedit.name) return; return beClose(d).then(function(ok){ if (ok) beEnter(d, n); }); });
  };

  /* ================= hiển thị theo chế độ: tab ngữ cảnh, UCS tại gốc block, mờ đối tượng ================= */
  var ctxR0 = ED.ctxRefresh;
  ED.ctxRefresh = function(){
    var app = A(), d = D(); if (!d || app.active === 'start') return ctxR0();
    var cur = app.ctxTab && app.ctxTab.id;
    if (!TC.isViewer() && !/^(mte|cmp|arr|hatchc)$/.test(cur)){
      if (d.bedit){ app.setCtxTab(beTab(d), cur === 'bedit'); return; }
      if (d.refedit){ app.setCtxTab(reTab(d), cur === 'refe'); return; }
      if (!TC.cmdline.busy() && d.sel.length === 1){ var e = d.byId[d.sel[0]]; if (e && e.t === 'pdfu'){ app.setCtxTab(pdfTab(e), cur === 'pdfu'); return; } }
    }
    ctxR0();
  };
  TC.on('doc', function(d){ modeAttr(); if (d && d.bedit && !d.banners.some(function(b){ return b.id === 'bedit'; })) beChrome(d); if (d && d.refedit && !d.banners.some(function(b){ return b.id === 'refedit'; })) reChrome(d); });
  // UCS đặt tại gốc block khi sửa block (gốc ngoài màn hình thì hiện ở góc dưới trái)
  TC.overlays.push(function(ctx, v){
    var d = v.doc; if (!d || !d.bedit || v.space()) return; var s = v.mToS([0, 0]); if (!s) return; var out = s[0] < 20 || s[1] > v.H - 20 || s[0] > v.W - 60 || s[1] < 60;
    if (out) s = [26, v.H - 22]; var x = Math.round(s[0]) + .5, y = Math.round(s[1]) + .5, L = 44;
    ctx.save(); ctx.strokeStyle = ctx.fillStyle = TC.cssv('--c-cross'); ctx.lineWidth = 1.5; ctx.setLineDash([]); ctx.beginPath(); ctx.moveTo(x, y - L); ctx.lineTo(x, y); ctx.lineTo(x + L, y); ctx.moveTo(x - 4, y - L + 7); ctx.lineTo(x, y - L); ctx.lineTo(x + 4, y - L + 7); ctx.moveTo(x + L - 7, y - 4); ctx.lineTo(x + L, y); ctx.lineTo(x + L - 7, y + 4); ctx.stroke();
    ctx.strokeRect(x, y - 8, 8, 8); ctx.font = '11px ' + TC.cssv('--f-mono'); ctx.fillText('X', x + L + 4, y + 4); ctx.fillText('Y', x - 3, y - L - 5); if (!out){ ctx.font = '10.5px ' + TC.cssv('--f-ui'); ctx.globalAlpha = .8; ctx.fillText('gốc block', x + 12, y + 15); } ctx.restore();
  });
  // mờ: đối tượng ngoài tập làm việc khi REFEDIT, đối tượng dựng hình trong Block Editor, PDF underlay (nhạt theo Mờ và Tương phản)
  function alphaCtx(ctx, k){ var fn = {}; return new Proxy(ctx, { get:function(t, p){ var v = t[p]; return typeof v === 'function' ? fn[p] || (fn[p] = v.bind(t)) : v; }, set:function(t, p, v){ t[p] = p === 'globalAlpha' ? v * k : v; return true; } }); }
  function pdfAlpha(e){ return (1 - (e.fade == null ? 25 : e.fade) / 100) * (.5 + (e.contrast == null ? 20 : e.contrast) / 200); }
  function refFade(){ return (TC.S.refFade == null ? 50 : TC.S.refFade) / 100; }
  var dp0 = TC.View.prototype.drawPrims;
  TC.View.prototype.drawPrims = function(ctx, e, v, onPaper, style){
    var d = this.doc, k = 1;
    if (e.t === 'pdfu' && style){ var all = e._p, fr = all.filter(function(pr){ return pr.fr; }); if (fr.length){ e._p = fr; try { dp0.call(this, ctx, e, v, onPaper, style); } finally { e._p = all; } return; } }
    if (!style){ if (e.t === 'pdfu') k = pdfAlpha(e); if (d && d.refedit && e.id && d.byId[e.id] === e && !d.refedit.ws[e.id]) k *= 1 - refFade(); if (d && d.bedit && e.cons) k *= .45; }
    if (k === 1) return dp0.apply(this, arguments);
    var g = ctx.globalAlpha; ctx.globalAlpha = g * k; dp0.call(this, alphaCtx(ctx, k), e, v, onPaper, style); ctx.globalAlpha = g;
  };

  /* ================= nhấp đúp, menu chuột phải theo loại (07.1 §9.10) ================= */
  var LASTPT = null;
  var dbl0 = TC.cad.hook.dbl;
  TC.cad.hook.dbl = function(e, m){
    var d = D(); if (TC.isViewer() || !e || !d || (d.layerMap[e.layer] || {}).lck) return dbl0 ? dbl0(e, m) : false;
    LASTPT = m ? V().sToM(m[0], m[1]) : null;
    if (e.t === 'pdfu'){ d.sel = [e.id]; TC.cad.afterSel(); return true; }
    if (e.t === 'insert' && !d.refedit){ if (e.xref){ TC.cmd.run('REFEDIT', { e:e, p:LASTPT }); return true; } if (!attDefs(d.blocks[e.name]).length){ TC.cmd.run('BEDIT', e); return true; } }
    return dbl0 ? dbl0(e, m) : false;
  };
  TC.cad.ctxExt.push(function(it, first, d){
    if (TC.isViewer() || !first) return; var m = V().mouse, wp = m ? V().sToM(m[0], m[1]) : null;
    if (d.refedit){ it.push({ label:'Thêm vào tập làm việc', icon:'pickadd', act:function(){ TC.cmd.run('REFSET', 'A'); } }, { label:'Bớt khỏi tập làm việc', icon:'pickrem', act:function(){ TC.cmd.run('REFSET', 'R'); } }); return; }
    if (first.t === 'insert' && !d.bedit){ it.push(first.xref ? { label:'Sửa xref tại chỗ', icon:'bedit', act:function(){ TC.cmd.run('REFEDIT', { e:first, p:wp }); } } : { label:'Sửa block tại chỗ', icon:'bedit', act:function(){ TC.cmd.run('REFEDIT', { e:first, p:wp }); } }); if (!first.xref) it.push({ label:'Block Editor', icon:'bedit', act:function(){ TC.cmd.run('BEDIT', first); } }, { label:'Thay block…', icon:'breplace', act:function(){ TC.cmd.run('BLOCKREPLACE'); } }); }
    if (first.t === 'pdfu') it.push({ label:'Layer của PDF…', icon:'layers', act:function(){ TC.cmd.run('ULAYERS', first); } }, { label:'Nhập PDF thành đối tượng…', icon:'pdf', act:function(){ TC.cmd.run('PDFIMPORT', first); } });
  });

  /* ================= Quản lý attribute UI-G08 (BATTMAN), ATTSYNC ================= */
  var MODES = [['hidden', 'Ẩn', 'I'], ['const', 'Hằng số', 'C'], ['verify', 'Xác nhận', 'V'], ['preset', 'Đặt sẵn', 'P'], ['lockpos', 'Khóa vị trí', 'L'], ['multi', 'Nhiều dòng', 'M']];
  var BM = TC.load('battman', { cols:{ tag:1, prompt:1, def:1, mode:1, style:0, h:0, layer:0 }, dup:true, apply:true });
  var BMCOLS = [['tag', 'Tag'], ['prompt', 'Prompt'], ['def', 'Mặc định'], ['mode', 'Chế độ'], ['style', 'Kiểu chữ'], ['h', 'Chiều cao'], ['layer', 'Layer']];
  function modeStr(a){ var m = MODES.filter(function(x){ return a[x[0]]; }); return m.length ? m.map(function(x){ return x[1]; }).join(', ') : '—'; }
  // đồng bộ giá trị attribute của tham chiếu theo định nghĩa (giữ giá trị cũ theo tag, tag đổi tên thì theo tag cũ); gọi trong ED.run
  function syncRefs(d, name, defs){ var n = 0; defs = defs || attDefs(d.blocks[name]); allIns(d, name).forEach(function(e){ var old = e.attrs || {}, at = {}; defs.forEach(function(a){ var v = old[a._old || a.tag]; at[a.tag] = v != null && !a.const ? v : a.s; }); if (JSON.stringify(at) !== JSON.stringify(old)){ ED.mod(e, function(x){ x.attrs = at; }); n++; } }); return n; }
  function attBlocks(d){ return userBlocks(d).filter(function(n){ return attDefs(d.blocks[n]).length; }); }
  IMPL.BATTMAN = function(ctx, pre){
    var d = D(), names = attBlocks(d);
    if (!names.length){ ctx.out('Bản vẽ chưa có block nào có attribute. Tạo attribute bằng ATTDEF rồi gom vào block bằng BLOCK.'); return; }
    var sel0 = d.sel.length === 1 && d.byId[d.sel[0]], name = typeof pre === 'string' && names.indexOf(pre) >= 0 ? pre : sel0 && sel0.t === 'insert' && names.indexOf(sel0.name) >= 0 ? sel0.name : names[0];
    var defs, cur = 0, dirty = false;
    function load(){ defs = attDefs(d.blocks[name]).map(function(a){ var c = ED.clean(a); c._old = a.tag; return c; }); cur = 0; dirty = false; }
    load();
    return new Promise(function(res){
      var dlg = TC.dialog({ id:'battman', title:'Quản lý attribute block', cmd:'BATTMAN', code:'UI-G08', w:760, onClose:function(r){ if (r !== true) res(); },
        body:'<div class="stack"><div class="hrow"><span class="faint">Block</span><select class="inp" id="bm-b" style="width:220px" aria-label="Block"></select><button class="btn sm" type="button" id="bm-pick">' + ic('select') + 'Chọn block</button><span class="grow"></span><span class="faint" id="bm-n" style="font-size:var(--fs-s)"></span></div>' +
          '<div class="bmw"><div class="tblwrap bmt"><table class="tbl" id="bm-t"></table></div><div class="stack ds-btns" style="gap:6px"><button class="btn" type="button" id="bm-sync">' + ic('refresh') + 'Đồng bộ</button><button class="btn" type="button" id="bm-up">' + ic('up') + 'Lên</button><button class="btn" type="button" id="bm-dn">' + ic('down') + 'Xuống</button><button class="btn" type="button" id="bm-ed">' + ic('attedit') + 'Sửa…</button><button class="btn" type="button" id="bm-rm">' + ic('trash') + 'Gỡ</button></div></div>' +
          '<div id="bm-note"></div>' + H.chk('bm-apply', 'Áp thay đổi cho tham chiếu có sẵn', BM.apply) + '</div>',
        foot:'<button class="btn ghost" type="button" id="bm-set">' + ic('gear') + 'Thiết lập…</button><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn" type="button" id="bm-ap" disabled>Áp dụng</button><button class="btn primary" type="button" id="bm-ok"></button>' });
      function opts(){ $('#bm-b', dlg.el).innerHTML = names.map(function(n){ return '<option' + (n === name ? ' selected' : '') + '>' + esc(n) + '</option>'; }).join(''); }
      function render(){
        var cols = BMCOLS.filter(function(c){ return BM.cols[c[0]]; }), tags = defs.map(function(a){ return a.tag; }), dup = tags.filter(function(t, i){ return tags.indexOf(t) !== i; }), ins = allIns(d, name), inM = ins.filter(function(e){ return !e._paper; }).length;
        $('#bm-n', dlg.el).textContent = 'Tìm thấy trong bản vẽ: ' + ins.length + ' (Model ' + inM + ', layout ' + (ins.length - inM) + ')';
        $('#bm-t', dlg.el).innerHTML = '<thead><tr>' + cols.map(function(c){ return '<th>' + c[1] + '</th>'; }).join('') + '</tr></thead><tbody>' + (defs.length ? defs.map(function(a, i){ var dp = BM.dup && dup.indexOf(a.tag) >= 0; return '<tr data-ai="' + i + '" class="' + (i === cur ? 'is-sel' : '') + (dp ? ' dup' : '') + '">' + cols.map(function(c){ var v = c[0] === 'tag' ? '<span class="mono">' + esc(a.tag) + '</span>' + (dp ? ' <span class="tag warn">trùng</span>' : '') : c[0] === 'prompt' ? esc(a.prompt || a.tag) : c[0] === 'def' ? esc(a.s || '') : c[0] === 'mode' ? modeStr(a) : c[0] === 'style' ? esc(a.style || 'Standard') : c[0] === 'h' ? '<span class="mono">' + TC.d(a.h) + '</span>' : esc(a.layer || '0'); return '<td>' + v + '</td>'; }).join('') + '</tr>'; }).join('') : '<tr><td colspan="' + cols.length + '"><div class="empty" style="padding:14px">Block không còn attribute nào.</div></td></tr>') + '</tbody>';
        $('#bm-note', dlg.el).innerHTML = BM.dup && dup.length ? '<div class="note warn">' + ic('warn') + '<span>Tag trùng: <b class="mono">' + esc(dup.filter(function(t, i){ return dup.indexOf(t) === i; }).join(', ')) + '</b>. Khi nhập và đồng bộ chỉ tag đầu tiên nhận giá trị: đổi tên một tag bằng Sửa….</span></div>' : '';
        ['bm-up', 'bm-dn', 'bm-ed', 'bm-rm'].forEach(function(id){ $('#' + id, dlg.el).disabled = !defs.length; }); $('#bm-up', dlg.el).disabled = !defs.length || cur === 0; $('#bm-dn', dlg.el).disabled = !defs.length || cur >= defs.length - 1;
        $('#bm-ap', dlg.el).disabled = !dirty; var ap = $('#bm-apply', dlg.el).checked; $('#bm-ok', dlg.el).textContent = dirty ? (ap && ins.length ? 'Cập nhật ' + ins.length + ' tham chiếu' : 'Lưu định nghĩa') : 'Đóng';
      }
      function apply(force){ var refs = force || $('#bm-apply', dlg.el).checked, nRef = 0, list = defs.slice();
        ED.run(force ? 'ATTSYNC' : 'BATTMAN', function(){ var done = defState(d, [name]), bd = d.blocks[name]; bd.ents = bd.ents.filter(function(x){ return x.t !== 'attdef'; }).concat(list.map(function(a){ var c = ED.clean(a); delete c._old; c.id = c.id || ++d.seq; return c; })); done(); if (refs) nRef = syncRefs(d, name, list); });
        TC.prepDoc(d); V().draw(); load(); render(); TC.cmdline.msg((force ? 'Đã đồng bộ attribute của block <b>' : 'Đã cập nhật định nghĩa <b>') + esc(name) + '</b>' + (refs ? ' · ' + nRef + ' tham chiếu đổi giá trị hoặc thứ tự.' : '; tham chiếu có sẵn giữ nguyên (dùng Đồng bộ khi cần).')); }
      function change(fn){ fn(); dirty = true; render(); }
      dlg.el.addEventListener('click', function(e){ var tr = e.target.closest('[data-ai]'); if (tr){ cur = Number(tr.dataset.ai); render(); } });
      dlg.el.addEventListener('dblclick', function(e){ if (e.target.closest('[data-ai]')) $('#bm-ed', dlg.el).click(); });
      TC.listKeys($('.bmt', dlg.el), 'tr[data-ai]', function(tr, enter){ if (enter){ $('#bm-ed', dlg.el).click(); return; } cur = Number(tr.dataset.ai); render(); });
      $('.bmt', dlg.el).tabIndex = 0; $('.bmt', dlg.el).addEventListener('keydown', function(e){ if (e.key === 'Delete'){ e.preventDefault(); $('#bm-rm', dlg.el).click(); } });
      $('#bm-b', dlg.el).addEventListener('change', function(){ var v = this.value, go = function(){ name = v; load(); render(); }; if (dirty) TC.confirm({ title:'Đổi block', msg:'Thay đổi của block <b>' + esc(name) + '</b> chưa áp dụng.', ok:'Áp dụng rồi đổi', alt:'Bỏ thay đổi' }).then(function(r){ if (r === true){ apply(false); go(); } else if (r === 'alt') go(); else opts(); }); else go(); });
      $('#bm-pick', dlg.el).addEventListener('click', function(){ dlg.hide(true); ED.pick(ctx, 'Chọn block có attribute', { types:['insert'] }).then(function(r){ dlg.hide(false); if (r.e && names.indexOf(r.e.name) >= 0){ name = r.e.name; load(); opts(); render(); } else if (r.e) TC.ctip(dlg.el, 120, 60, 'Block “' + r.e.name + '” không có attribute'); }, function(){ dlg.hide(false); }); });
      $('#bm-up', dlg.el).addEventListener('click', function(){ if (cur > 0) change(function(){ var a = defs.splice(cur, 1)[0]; defs.splice(--cur, 0, a); }); });
      $('#bm-dn', dlg.el).addEventListener('click', function(){ if (cur < defs.length - 1) change(function(){ var a = defs.splice(cur, 1)[0]; defs.splice(++cur, 0, a); }); });
      $('#bm-rm', dlg.el).addEventListener('click', function(){ var a = defs[cur]; if (!a) return; var n = allIns(d, name).length; TC.confirm({ title:'Gỡ attribute', msg:'Gỡ tag <b class="mono">' + esc(a.tag) + '</b> khỏi block <b>' + esc(name) + '</b>?', detail:n ? n + ' tham chiếu sẽ mất giá trị của tag này khi áp dụng. Hoàn tác được bằng Ctrl+Z.' : '', ok:'Gỡ attribute', danger:true }).then(function(y){ if (y) change(function(){ defs.splice(cur, 1); cur = Math.max(0, Math.min(cur, defs.length - 1)); }); }); });
      $('#bm-ed', dlg.el).addEventListener('click', function(){ if (defs[cur]) attDefEdit(d, defs, cur).then(function(ok){ if (ok) change(function(){}); }); });
      $('#bm-sync', dlg.el).addEventListener('click', function(){ apply(true); });
      $('#bm-apply', dlg.el).addEventListener('change', function(){ BM.apply = this.checked; TC.save('battman', BM); render(); });
      $('#bm-ap', dlg.el).addEventListener('click', function(){ apply(false); });
      $('#bm-ok', dlg.el).addEventListener('click', function(){ if (dirty) apply(false); dlg.close(true); res(); });
      $('#bm-set', dlg.el).addEventListener('click', function(){ var sd = TC.dialog({ id:'bmset', title:'Thiết lập quản lý attribute', code:'UI-G08', w:420, help:false,
          body:'<div class="stack">' + H.grp('Cột hiển thị', '<div class="at-tags">' + BMCOLS.map(function(c){ return H.chk('bc-' + c[0], c[1], !!BM.cols[c[0]], c[0] === 'tag'); }).join('') + '</div>') + H.chk('bs-dup', 'Tô nổi tag trùng', BM.dup) + H.chk('bs-ap', 'Áp thay đổi cho tham chiếu có sẵn', $('#bm-apply', dlg.el).checked) + '</div>', foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="bs-ok">Lưu thiết lập</button>' });
        $('#bs-ok', sd.el).addEventListener('click', function(){ BMCOLS.forEach(function(c){ BM.cols[c[0]] = c[0] === 'tag' || $('#bc-' + c[0], sd.el).checked ? 1 : 0; }); BM.dup = $('#bs-dup', sd.el).checked; BM.apply = $('#bs-ap', sd.el).checked; $('#bm-apply', dlg.el).checked = BM.apply; TC.save('battman', BM); sd.close(true); render(); }); });
      opts(); render();
    });
  };
  // sửa một attribute definition (các tab như UI-G07); kiểm tra tag rỗng, có khoảng trắng, trùng
  function attDefEdit(d, defs, i){
    var a = defs[i];
    return new Promise(function(res){
      var dlg = TC.dialog({ id:'bmedit', title:'Sửa attribute: ' + esc(a.tag), code:'UI-G08', w:560, onClose:function(r){ if (r !== true) res(false); },
        body:'<div class="stack">' + H.tabs([['a', 'Attribute'], ['t', 'Tùy chọn chữ'], ['p', 'Thuộc tính']], 'a', 'bme') +
          H.pane('a', 'a', '<div class="grid2"><div class="grp"><div class="gh">Chế độ</div>' + MODES.map(function(m){ return H.chk('be-' + m[0], m[1], !!a[m[0]]); }).join('') + '</div><div class="stack">' + H.row('Tag', '<input class="inp mono" id="bme-tag" value="' + esc(a.tag) + '" style="width:180px" spellcheck="false">', 70) + H.row('Prompt', '<input class="inp" id="bme-pr" value="' + esc(a.prompt || '') + '" style="width:180px">', 70) + H.row('Mặc định', '<input class="inp" id="bme-def" value="' + esc(a.s || '') + '" style="width:180px">', 70) + '<span class="fmsg" id="bme-m"></span></div></div>') +
          H.pane('t', 'a', '<div class="grid2"><div class="stack">' + H.row('Kiểu chữ', H.sel('bme-st', d.styles.map(function(s){ return s.name; }), a.style || 'Standard', 150), 90) + H.row('Canh lề', H.sel('bme-al', ['Trái', 'Giữa', 'Phải', 'Giữa tâm'], { l:'Trái', c:'Giữa', r:'Phải', m:'Giữa tâm' }[a.al || 'l'], 150), 90) + H.row('Chiều cao', H.num('bme-h', TC.d(a.h), { w:110 }), 90) + H.row('Góc xoay', H.num('bme-r', TC.d(a.rot || 0, 0), { prec:0, unit:'deg', w:110 }), 90) + '</div><div class="stack">' + H.row('Hệ số rộng', H.num('bme-wf', '1.0000', { prec:4, unit:'none', w:90, pick:false }), 90) + H.row('Góc nghiêng', H.num('bme-ob', '0', { prec:0, unit:'deg', w:90, pick:false }), 90) + H.chk('', 'Chú thích theo tỉ lệ', false) + H.chk('', 'Viết ngược', false) + H.chk('', 'Lộn ngược', false) + '</div></div>') +
          H.pane('p', 'a', '<div class="stack">' + H.row('Layer', H.sel('bme-ly', d.layers.map(function(l){ return l.name; }), a.layer || '0', 200), 90) + H.row('Linetype', H.sel('', ['ByLayer', 'ByBlock', 'Continuous'], 'ByLayer', 200), 90) + H.row('Màu', H.sel('', ['ByLayer', 'ByBlock', 'Đỏ', 'Vàng'], 'ByLayer', 200), 90) + H.row('Độ dày nét', H.sel('', ['ByLayer', 'ByBlock', '0,25 mm'], 'ByLayer', 200), 90) + '</div>') + '</div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="bme-ok">Lưu attribute</button>', focus:'#bme-tag' });
      $('#bme-ok', dlg.el).addEventListener('click', function(){
        var tag = $('#bme-tag', dlg.el).value.trim().toUpperCase(), m = $('#bme-m', dlg.el), err = !tag ? 'Nhập tag cho attribute.' : /\s/.test(tag) ? 'Tag không chứa khoảng trắng.' : defs.some(function(x, j){ return j !== i && x.tag === tag; }) ? 'Block đã có tag “' + tag + '”.' : null;
        if (err){ m.className = 'fmsg err'; m.innerHTML = ic('warn', 'xs') + esc(err); $('#bme-tag', dlg.el).classList.add('err'); var t = $('[data-dt="a"]', dlg.el); if (t) t.click(); $('#bme-tag', dlg.el).focus(); return; }
        a.tag = tag; a.prompt = $('#bme-pr', dlg.el).value || tag; a.s = $('#bme-def', dlg.el).value; MODES.forEach(function(x){ if ($('#be-' + x[0], dlg.el).checked) a[x[0]] = true; else delete a[x[0]]; });
        a.style = $('#bme-st', dlg.el).value; a.al = { 'Trái':'l', 'Giữa':'c', 'Phải':'r', 'Giữa tâm':'m' }[$('#bme-al', dlg.el).value]; a.h = num($('#bme-h', dlg.el).value, a.h) || a.h; a.rot = num($('#bme-r', dlg.el).value, 0); a.layer = $('#bme-ly', dlg.el).value;
        dlg.close(true); res(true); });
    });
  }
  IMPL.ATTSYNC = function(ctx, pre){
    var d = D(), find = function(v){ v = String(v || '').trim().toUpperCase(); return Object.keys(d.blocks).filter(function(n){ return n.toUpperCase() === v; })[0]; };
    var go = function(nm){ var bd = d.blocks[nm]; if (!bd){ ctx.out('Không có block “' + esc(nm || '') + '” trong bản vẽ.'); return; } if (!attDefs(bd).length){ ctx.out('Block <b>' + esc(nm) + '</b> không có attribute; không có gì để đồng bộ.'); return; } var n = allIns(d, nm).length;
      return ctx.kw('Đồng bộ attribute của ' + n + ' block ' + nm + '?', [['Có', 'Y'], ['Không', 'N']], 'Y').then(function(k){ if (k !== 'Y') return; var c = 0; ED.run('ATTSYNC', function(){ c = syncRefs(d, nm); }); V().draw(); A().refreshPalettes(); ctx.out('Đã đồng bộ ' + n + ' block <b>' + esc(nm) + '</b> theo định nghĩa: ' + c + ' block đổi attribute (thêm tag mới, bỏ tag đã gỡ, giữ giá trị đã nhập).'); }); };
    if (pre) return go(find(pre) || pre);
    return ctx.kw('Chọn block cần đồng bộ', [['Liệt kê', '?'], ['Tên', 'N'], ['Chọn', 'S']], 'S').then(function(k){
      if (k === '?'){ ctx.out('Block có attribute: ' + attBlocks(d).map(function(n){ return '<b>' + esc(n) + '</b> (' + allIns(d, n).length + ')'; }).join(', ')); return IMPL.ATTSYNC(ctx); }
      if (k === 'N') return ctx.val('Tên block cần đồng bộ').then(function(v){ return go(find(v) || v); });
      return ED.pick(ctx, 'Chọn block', { types:['insert'], bad:'Chọn một block có attribute' }).then(function(r){ if (r.e) return go(r.e.name); });
    });
  };

  /* ================= PDF underlay: tệp mô phỏng, nội dung vector theo trang (mm trên giấy, tỉ lệ 1:100) ================= */
  var PDFS = [
    { name:'MB-nha-pho-doi-tac.pdf', dir:'D:\\DuAn\\NhaO-2T\\Doi-tac', size:'2,4 MB', date:'14/10/2026', w:420, h:297, vec:true, pages:['Mặt bằng tầng 1', 'Mặt bằng tầng 2', 'Mặt đứng trục 1-4'] },
    { name:'Ho-so-khao-sat-scan.pdf', dir:'D:\\DuAn\\NhaO-2T\\Khao-sat', size:'8,6 MB', date:'02/10/2026', w:297, h:210, vec:false, pages:['Hiện trạng khu đất', 'Ảnh chụp hiện trạng'] },
    { name:'Ban-ve-ky-duyet.pdf', dir:'\\\\SRV-01\\Chung\\Ky-duyet', size:'1,1 MB', date:'16/10/2026', w:420, h:297, vec:true, pass:true, pages:['Mặt bằng tầng 1 (đã ký)'] }
  ];
  var PDFL = { 'Tường':[7, .5], 'Cửa':[4, .25], 'Cột':[8, .35], 'Trục':[1, .18], 'Kích thước':[3, .18], 'Chữ':[7, .25], 'Nội thất':[8, .18], 'Ghi chú':[7, .25], 'Khung tên':[7, .35], 'Ảnh quét':[7, .18] };
  var PDFC = {};
  function pdfFile(n){ return PDFS.filter(function(f){ return f.name === n; })[0]; }
  function pdfItems(f, pg){
    var key = f.name + '#' + pg; if (PDFC[key]) return PDFC[key];
    var out = PDFC[key] = [], add = function(pl, o){ o.pl = pl; if (o.color == null) o.color = PDFL[pl][0]; o.plw = PDFL[pl][1]; out.push(o); return o; };
    var L = function(pl, a, b, ax){ return add(pl, { t:'line', p1:a, p2:b, ax:ax }); }, R = function(pl, x0, y0, x1, y1){ L(pl, [x0, y0], [x1, y0]); L(pl, [x1, y0], [x1, y1]); L(pl, [x1, y1], [x0, y1]); L(pl, [x0, y1], [x0, y0]); };
    var Tx = function(pl, p, h, s, al, shx, rot){ return add(pl, { t:'text', p:p, h:h, s:s, al:al || 'l', rot:rot || 0, style:shx ? 'ROMANS' : 'Standard', shx:!!shx }); };
    var W = f.w, Hh = f.h; R('Khung tên', 10, 10, W - 10, Hh - 10);
    if (!f.vec){ add('Ảnh quét', { t:'image', p:[14, 14], w:W - 28, h:Hh - 28, name:'trang-quet-' + pg + '.png', frame:false, raster:true }); return out; }
    var tx = W - 190, ty = 10; R('Khung tên', tx, ty, W - 10, ty + 40); L('Khung tên', [tx, ty + 20], [tx + 110, ty + 20]); L('Khung tên', [tx + 110, ty], [tx + 110, ty + 40]);
    Tx('Khung tên', [tx + 4, ty + 29], 3, 'CÔNG TY TNHH TƯ VẤN THIẾT KẾ AN PHÚ'); Tx('Khung tên', [tx + 4, ty + 7], 4.2, f.pages[pg - 1].toUpperCase()); Tx('Khung tên', [tx + 114, ty + 30], 2.5, 'TL 1:100', 'l', true); Tx('Khung tên', [tx + 114, ty + 6], 5, 'KT-0' + pg, 'l', true);
    add('Khung tên', { t:'image', p:[tx + 150, ty + 22], w:26, h:14, name:'logo-an-phu.png', frame:false, raster:true });
    [[60, 'GHI CHÚ:'], [54.5, '1. Kích thước ghi bằng mm, cao độ ghi bằng m.'], [49, '2. Cao độ ±0.000 lấy tại mặt sàn tầng 1.'], [43.5, '3. Bản vẽ của đối tác, dùng để tham khảo.']].forEach(function(r, i){ Tx('Ghi chú', [30, r[0]], i ? 2.5 : 2.8, r[1]); });
    var X = [60, 105, 170, 220], Y = [110, 160];
    if (pg === 3){ // mặt đứng
      L('Tường', [40, 100], [240, 100]); R('Tường', 60, 100, 220, 168); L('Tường', [52, 168], [140, 190]); L('Tường', [140, 190], [228, 168]);
      [[78, 115, 98, 140], [122, 115, 142, 140], [186, 115, 206, 140]].forEach(function(r){ R('Cửa', r[0], r[1], r[2], r[3]); L('Cửa', [(r[0] + r[2]) / 2, r[1]], [(r[0] + r[2]) / 2, r[3]]); }); R('Cửa', 150, 100, 166, 128);
      [['±0.000', 100], ['+3.600', 134], ['+6.800', 168]].forEach(function(c){ L('Kích thước', [222, c[1]], [236, c[1]]); add('Kích thước', { t:'hatch', loops:[[[224, c[1]], [222, c[1] + 2.2], [226, c[1] + 2.2]]], pat:'SOLID' }); Tx('Kích thước', [227, c[1] + 1.2], 2.5, c[0], 'l', true); });
      X.forEach(function(x, i){ for (var y = 96; y > 82; y -= 8) L('Trục', [x, y], [x, y - 6], 'v' + i); add('Trục', { t:'circle', c:[x, 78], r:4 }); Tx('Trục', [x, 78], 3.5, String(i + 1), 'm', true); });
      return out;
    }
    var P2 = pg === 2, px = P2 ? [120, 175] : [105, 170];
    R('Tường', 60, 110, 220, 160); R('Tường', 62.2, 112.2, 217.8, 157.8);
    px.forEach(function(x, k){ var g0 = k ? 115 : 140, g1 = g0 + 9; [x - .55, x + .55].forEach(function(xx){ L('Tường', [xx, 112.2], [xx, g0]); L('Tường', [xx, g1], [xx, 157.8]); }); L('Tường', [x - .55, g0], [x + .55, g0]); L('Tường', [x - .55, g1], [x + .55, g1]); add('Cửa', { t:'arc', c:[x + .55, g0], r:9, a0:0, a1:90 }); L('Cửa', [x + .55, g0], [x + .55, g1]); });
    if (P2){ [134.45, 135.55].forEach(function(y){ L('Tường', [175.55, y], [217.8, y]); }); add('Cửa', { t:'arc', c:[190, 135.55], r:8, a0:0, a1:90 }); }
    [[120, 135], [180, 195]].forEach(function(w){ L('Cửa', [w[0], 158.9], [w[1], 158.9]); L('Cửa', [w[0], 157.8], [w[0], 160]); L('Cửa', [w[1], 157.8], [w[1], 160]); });
    X.forEach(function(x){ Y.forEach(function(y){ add('Cột', { t:'hatch', loops:[rp(x - 1.6, y - 1.6, x + 1.6, y + 1.6)], pat:'SOLID' }); }); });
    X.forEach(function(x, i){ for (var y = 104; y < 168; y += 8) L('Trục', [x, y], [x, y + 6], 'v' + i); add('Trục', { t:'circle', c:[x, 174], r:4 }); Tx('Trục', [x, 174], 3.5, String(i + 1), 'm', true); });
    Y.forEach(function(y, i){ for (var x = 50; x < 228; x += 8) L('Trục', [x, y], [x + 6, y], 'h' + i); add('Trục', { t:'circle', c:[42, y], r:4 }); Tx('Trục', [42, y], 3.5, 'AB'[i], 'm', true); });
    var dim = function(y, xs){ L('Kích thước', [xs[0] - 2, y], [xs[xs.length - 1] + 2, y]); xs.forEach(function(x, i){ L('Kích thước', [x, y - 2], [x, y + 4]); L('Kích thước', [x - 1, y - 1], [x + 1, y + 1]); if (i) Tx('Kích thước', [(x + xs[i - 1]) / 2, y + 1.2], 2.5, String((x - xs[i - 1]) * 100), 'c', true); }); };
    dim(98, X); dim(91, [60, 220]); L('Kích thước', [50, 108], [50, 162]); [110, 160].forEach(function(y){ L('Kích thước', [48, y], [56, y]); L('Kích thước', [49, y - 1], [51, y + 1]); }); Tx('Kích thước', [48.8, 135], 2.5, '5000', 'c', true, 90);
    var rooms = P2 ? [['P. NGỦ 2', 90, 20.6], ['P. SINH HOẠT', 147.5, 25.4], ['P. NGỦ 3', 196.5, 9.8], ['WC', 196.5, 8.5]] : [['PHÒNG KHÁCH', 82.5, 20.4], ['BẾP + ĂN', 137.5, 30.2], ['P. NGỦ', 195, 22.3]];
    rooms.forEach(function(r, i){ var y = P2 && i === 3 ? 122 : P2 && i === 2 ? 148 : 137; Tx('Chữ', [r[1], y], 3, r[0], 'm'); Tx('Chữ', [r[1], y - 5.5], 2.2, 'S = ' + r[2].toFixed(1) + ' m²', 'm'); });
    if (!P2){ R('Nội thất', 68, 120, 88, 128); R('Nội thất', 68, 128, 74, 148); add('Nội thất', { t:'circle', c:[137.5, 125], r:6 }); R('Nội thất', 182, 141, 208, 155); }
    else { R('Nội thất', 72, 140, 92, 155); R('Nội thất', 182, 113, 192, 119); }
    return out;
  }
  function pdfM(e){ var r = (e.rot || 0) * Math.PI / 180, s = e.sc || 1; return [s * Math.cos(r), s * Math.sin(r), -s * Math.sin(r), s * Math.cos(r), e.p[0], e.p[1]]; }
  function clipSeg(a, b, c){ var t0 = 0, t1 = 1, dx = b[0] - a[0], dy = b[1] - a[1], P2 = [-dx, dx, -dy, dy], Q = [a[0] - c[0], c[2] - a[0], a[1] - c[1], c[3] - a[1]]; for (var i = 0; i < 4; i++){ if (!P2[i]){ if (Q[i] < 0) return null; continue; } var r = Q[i] / P2[i]; if (P2[i] < 0){ if (r > t1) return null; if (r > t0) t0 = r; } else { if (r < t0) return null; if (r < t1) t1 = r; } } return [[a[0] + dx * t0, a[1] + dy * t0], [a[0] + dx * t1, a[1] + dy * t1]]; }
  function clipItem(it, c){ var inR = function(p){ return p[0] >= c[0] && p[0] <= c[2] && p[1] >= c[1] && p[1] <= c[3]; };
    if (it.t === 'line'){ var s = clipSeg(it.p1, it.p2, c); return s && Object.assign({}, it, { p1:s[0], p2:s[1] }); }
    var p = it.c || it.p || (it.loops && it.loops[0][0]); return p && inR(p) ? it : null; }
  // loại đối tượng PDF underlay: nội dung vector ghép từ nguyên tố có sẵn, có khung (FRAME), clip, tắt layer PDF
  TC.flatExt.pdfu = function(x){
    var e = x.e, f = pdfFile(e.name) || PDFS[0], M = pdfM(e), tt = { m:function(p){ return mApply(M, p); }, rot:e.rot || 0, sx:e.sc || 1, sy:e.sc || 1, k:e.sc || 1 }, off = e.off || [], cl = e.clip, bg = e.bgadj !== false;
    if (e.show !== false) pdfItems(f, e.page || 1).forEach(function(it){ if (off.indexOf(it.pl) >= 0) return; var c = cl ? clipItem(it, cl) : it; if (!c) return; var col = e.mono ? (bg ? 7 : 250) : c.color === 7 && !bg ? 250 : c.color; x.sub(Object.assign({}, c, { color:col, layer:e.layer }), tt, e); });
    if (x.doc.frame === 0 || e.frame === false) return; var fr = cl ? rp(cl[0], cl[1], cl[2], cl[3]) : rp(0, 0, f.w, f.h);
    for (var i = 0; i < 4; i++) x.o({ k:'seg', a:tt.m(fr[i]), b:tt.m(fr[(i + 1) % 4]), fr:1 });
  };
  TC.TYPE_NAMES.pdfu = 'PDF underlay';
  TC.propsExt.pdfu = function(d, e, f){ var pf = pdfFile(e.name) || {}; return [['Tệp', esc(e.name), 'ro'], ['Trang', (e.page || 1) + ' · ' + esc((pf.pages || [])[(e.page || 1) - 1] || ''), 'ro'], ['Vị trí X', f(e.p[0]), 'num'], ['Vị trí Y', f(e.p[1]), 'num'], ['Tỉ lệ', f(e.sc, 4), 'num'], ['Góc xoay', f(e.rot || 0, 0) + '°', 'num'], ['Tương phản', String(e.contrast == null ? 20 : e.contrast), 'num'], ['Mờ', String(e.fade == null ? 25 : e.fade), 'num'], ['Đơn sắc', e.mono ? 'Có' : 'Không', 'ro'], ['Hiện underlay', e.show === false ? 'Không' : 'Có', 'ro'], ['Layer PDF đang tắt', String((e.off || []).length), 'ro'], ['Có clip', e.clip ? 'Có' : 'Không', 'ro'], ['Đường dẫn', esc((pf.dir || '') + '\\' + e.name), 'ro']]; };
  // bắt điểm vào underlay (UOSNAP): tắt thì bỏ underlay khỏi tập điểm bắt
  var snap0 = TC.View.prototype.snapAt;
  TC.View.prototype.snapAt = function(){ var d = this.doc; if (!d || d.uosnap !== 0 || !d.ents.some(function(e){ return e.t === 'pdfu'; })) return snap0.apply(this, arguments); var keep = d.ents; d.ents = keep.filter(function(e){ return e.t !== 'pdfu'; }); try { return snap0.apply(this, arguments); } finally { d.ents = keep; } };
  var frame0 = IMPL.FRAME; IMPL.FRAME = function(ctx){ return Promise.resolve(frame0(ctx)).then(function(){ var d = D(); d.ents.forEach(function(e){ if (e.t === 'pdfu') TC.prepEnt(d, e); }); V().draw(); }); };
  // thumbnail một trang: giấy trắng, nét đen
  function pdfThumb(c, f, pg){ var x = c.getContext('2d'), W = c.width, Hh = c.height, s = Math.min((W - 6) / f.w, (Hh - 6) / f.h), ox = (W - f.w * s) / 2, oy = (Hh + f.h * s) / 2, S = function(p){ return [ox + p[0] * s, oy - p[1] * s]; };
    x.fillStyle = TC.cssv('--c-layout-bg'); x.fillRect(0, 0, W, Hh); x.fillStyle = '#FFFFFF'; x.fillRect(ox, oy - f.h * s, f.w * s, f.h * s); x.strokeStyle = x.fillStyle = '#2B2F35'; x.lineWidth = .8;
    pdfItems(f, pg).forEach(function(it){ x.beginPath(); if (it.t === 'line'){ var a = S(it.p1), b = S(it.p2); x.moveTo(a[0], a[1]); x.lineTo(b[0], b[1]); x.stroke(); } else if (it.t === 'circle' || it.t === 'arc'){ var cc = S(it.c); x.arc(cc[0], cc[1], it.r * s, it.t === 'arc' ? -it.a1 * Math.PI / 180 : 0, it.t === 'arc' ? -it.a0 * Math.PI / 180 : 7); x.stroke(); } else if (it.t === 'hatch'){ it.loops[0].forEach(function(q, i){ var p = S(q); if (i) x.lineTo(p[0], p[1]); else x.moveTo(p[0], p[1]); }); x.fill(); } else if (it.t === 'image'){ var p0 = S(it.p); x.fillStyle = it.w > 100 ? '#9AA1A9' : '#C7CCD2'; x.fillRect(p0[0], p0[1] - it.h * s, it.w * s, it.h * s); x.fillStyle = '#2B2F35'; } else if (it.t === 'text' && it.h * s > 1.2){ var tp = S(it.p); x.fillRect(tp[0] - (it.al === 'm' || it.al === 'c' ? it.s.length * it.h * s * .3 : 0), tp[1] - it.h * s * .5, it.s.length * it.h * s * .6, Math.max(.8, it.h * s * .5)); } }); }
  function pdfRec(d, f){ if (!d.xrefs.some(function(x){ return x.name === f.name; })) d.xrefs.push({ name:f.name, type:'PDF', status:'Đã nạp', path:'..\\' + f.dir.split('\\').pop() + '\\' + f.name, found:f.dir + '\\' + f.name, size:f.size, date:f.date }); }
  function mkPdfu(f, pg, p, sc, rot){ return ED.make({ t:'pdfu', name:f.name, page:pg, p:p, sc:sc, rot:rot || 0, fade:25, contrast:20, mono:false, bgadj:true, off:[] }); }

  /* ================= Gắn PDF UI-G13 (PDFATTACH) ================= */
  IMPL.PDFATTACH = function(ctx){
    var d = D(), f = PDFS[0], sel = [1], open = {};
    return new Promise(function(res){
      var dlg = TC.dialog({ id:'pdfatt', title:'Gắn PDF', cmd:'PDFATTACH', code:'UI-G13', w:720, onClose:function(r){ if (r !== true) res(); },
        body:'<div class="stack">' + H.row('Tệp', '<select class="inp" id="pa-f" style="width:340px">' + PDFS.map(function(x){ return '<option>' + esc(x.name) + '</option>'; }).join('') + '</select><button class="btn sm" type="button" id="pa-br">Duyệt…</button>', 40) +
          '<div class="pam"><div class="stack" style="gap:6px"><div class="hrow"><span class="gh">Trang</span><span class="faint" id="pa-n" style="font-size:var(--fs-xs)"></span><span class="grow"></span><button class="btn sm ghost" type="button" id="pa-all">Chọn tất cả</button></div><div class="pdp" id="pa-pages" aria-label="Các trang PDF"></div><span class="fmsg" id="pa-m"></span></div>' +
          '<div class="stack">' + H.grp('Đường dẫn', H.sel('pa-pt', ['Đường dẫn tương đối', 'Đường dẫn đầy đủ', 'Không đường dẫn'], 'Đường dẫn tương đối', 210)) + H.grp('Chi tiết', '<div class="frow" style="--lab-w:100px;font-size:var(--fs-s)" id="pa-det"></div>') + '</div></div>' +
          '<div class="grid3">' + H.grp('Điểm chèn', H.chk('pa-ps', 'Chỉ định trên màn hình', true) + H.row('X', '<input class="inp mono sm" id="pa-x" value="0.00" style="width:96px" disabled>', 20) + H.row('Y', '<input class="inp mono sm" id="pa-y" value="0.00" style="width:96px" disabled>', 20)) + H.grp('Tỉ lệ', H.chk('pa-ss', 'Chỉ định trên màn hình', false) + H.row('Hệ số', H.num('pa-s', '100.0000', { prec:4, unit:'none', w:112, pick:false }), 46)) + H.grp('Góc xoay', H.chk('pa-rs', 'Chỉ định trên màn hình', false) + H.row('Góc', H.num('pa-r', '0', { prec:0, unit:'deg', w:96, pick:false }), 34)) + '</div>' +
          '<div class="note acc">' + ic('info') + '<span>Muốn chuyển thành đối tượng sửa được? <button class="lnk" type="button" id="pa-imp">Nhập PDF →</button> (PDFIMPORT)</span></div></div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="pa-ok">Gắn 1 trang</button>' });
      function locked(){ return f.pass && !open[f.name]; }
      function render(){
        var host = $('#pa-pages', dlg.el), sc = num($('#pa-s', dlg.el).value, 100) || 100;
        if (locked()) host.innerHTML = '<div class="pdlock">' + ic('lock') + '<b>Tệp được bảo vệ bằng mật khẩu</b><span class="faint">Nhập mật khẩu mở tệp để xem và gắn trang.</span><div class="hrow"><input class="inp" type="password" id="pa-pw" style="width:180px" aria-label="Mật khẩu PDF" placeholder="Mật khẩu"><button class="btn sm" type="button" id="pa-un">Mở khóa</button></div><span class="fmsg" id="pa-pm"></span></div>';
        else { host.innerHTML = f.pages.map(function(t, i){ var on = sel.indexOf(i + 1) >= 0; return '<button class="pdpi" type="button" data-pg="' + (i + 1) + '" aria-pressed="' + on + '"><canvas width="132" height="94"></canvas><span><b class="mono">' + (i + 1) + '</b> ' + esc(t) + '</span>' + (on ? '<i class="pdck">' + ic('check', 's12') + '</i>' : '') + '</button>'; }).join(''); $$('[data-pg] canvas', host).forEach(function(c){ pdfThumb(c, f, Number(c.parentNode.dataset.pg)); }); }
        $('#pa-n', dlg.el).textContent = locked() ? '' : f.pages.length + ' trang · đã chọn ' + sel.length;
        var m = $('#pa-m', dlg.el); m.className = 'fmsg ' + (!locked() && !sel.length ? 'err' : 'hint'); m.innerHTML = locked() ? '' : !sel.length ? ic('warn', 'xs') + 'Chọn ít nhất một trang.' : 'Bấm để chọn hoặc bỏ chọn trang; các trang sau đặt cạnh nhau bên phải.';
        $('#pa-det', dlg.el).innerHTML = '<span class="lab">Khổ trang</span><span>' + (f.w === 420 ? 'A3' : 'A4') + ' ngang · ' + f.w + ' × ' + f.h + ' mm</span><span class="lab">Nội dung</span><span>' + (f.vec ? 'Vector · ' + Object.keys(PDFL).length + ' layer PDF' : 'Ảnh quét, không có vector') + '</span><span class="lab">Tệp</span><span>' + esc(f.size) + ' · ' + esc(f.date) + '</span><span class="lab">Tìm thấy tại</span><span class="mono" style="font-size:var(--fs-xs);word-break:break-all">' + esc(f.dir + '\\' + f.name) + '</span><span class="lab">Trong bản vẽ</span><span class="mono">' + TC.d(f.w * sc, 0) + ' × ' + TC.d(f.h * sc, 0) + '</span>';
        var ok = $('#pa-ok', dlg.el); ok.disabled = locked() || !sel.length; ok.textContent = 'Gắn ' + Math.max(1, sel.length) + ' trang';
        var un = $('#pa-un', dlg.el); if (un){ var pw = $('#pa-pw', dlg.el); un.addEventListener('click', function(){ if (!pw.value){ $('#pa-pm', dlg.el).className = 'fmsg err'; $('#pa-pm', dlg.el).innerHTML = ic('warn', 'xs') + 'Nhập mật khẩu do người gửi tệp cung cấp.'; pw.classList.add('err'); pw.focus(); return; } open[f.name] = 1; render(); }); pw.addEventListener('keydown', function(e){ if (e.key === 'Enter'){ e.preventDefault(); e.stopPropagation(); un.click(); } }); setTimeout(function(){ pw.focus(); }, 0); }
      }
      $('#pa-f', dlg.el).addEventListener('change', function(){ f = pdfFile(this.value); sel = [1]; render(); });
      $('#pa-br', dlg.el).addEventListener('click', function(){ TC.toast('Mở hộp thoại chọn tệp PDF của hệ điều hành.', { type:'info', log:false }); });
      $('#pa-pages', dlg.el).addEventListener('click', function(e){ var b = e.target.closest('[data-pg]'); if (!b) return; var pg = Number(b.dataset.pg), i = sel.indexOf(pg); if (i >= 0) sel.splice(i, 1); else sel.push(pg); sel.sort(); render(); var nb = $('[data-pg="' + pg + '"]', dlg.el); if (nb) nb.focus(); });
      $('#pa-all', dlg.el).addEventListener('click', function(){ if (locked()) return; sel = sel.length === f.pages.length ? [] : f.pages.map(function(_, i){ return i + 1; }); render(); });
      $('#pa-ps', dlg.el).addEventListener('change', function(){ $('#pa-x', dlg.el).disabled = $('#pa-y', dlg.el).disabled = this.checked; });
      dlg.el.addEventListener('numchange', render);
      $('#pa-imp', dlg.el).addEventListener('click', function(){ var pg = sel[0] || 1; dlg.close(false); setTimeout(function(){ TC.cmd.run('PDFIMPORT', { file:f.name, page:pg }); }, 0); });
      $('#pa-ok', dlg.el).addEventListener('click', function(){
        if (locked() || !sel.length) return;
        var sc = num($('#pa-s', dlg.el).value, 100) || 100, rot = num($('#pa-r', dlg.el).value, 0), onScr = $('#pa-ps', dlg.el).checked, scScr = $('#pa-ss', dlg.el).checked, rScr = $('#pa-rs', dlg.el).checked, pages = sel.slice(); dlg.close(true);
        var pt = onScr ? ctx.point('Điểm chèn (góc dưới trái trang ' + pages[0] + ')', { onMove:function(p, temp){ temp.push(ED.temp([mkPdfu(f, pages[0], p, sc, rot)], { real:true })); } }) : Promise.resolve({ p:[num($('#pa-x', dlg.el).value, 0), num($('#pa-y', dlg.el).value, 0)] });
        pt.then(function(r){ if (!r.p) return res(); var p = r.p;
          var s2 = scScr ? ctx.point('Hệ số tỉ lệ', { base:p, num:true, dyn:false, def:TC.d(sc, 4), onMove:function(q, temp){ temp.push(ED.temp([mkPdfu(f, pages[0], p, Math.max(1, Math.abs(q[0] - p[0]) / f.w), rot)], { real:true })); } }).then(function(z){ if (z.num != null) sc = z.num; else if (z.p) sc = Math.max(1, Math.abs(z.p[0] - p[0]) / f.w); }) : Promise.resolve();
          return s2.then(function(){ return rScr ? ctx.point('Góc xoay', { base:p, num:true, dyn:'ang', onMove:function(q, temp){ temp.push(ED.temp([mkPdfu(f, pages[0], p, sc, G.ang(p, q))], { real:true })); } }).then(function(z){ rot = z.num != null ? z.num : z.p ? G.ang(p, z.p) : rot; }) : null; }).then(function(){
            var ids = []; ED.run('PDFATTACH', function(){ pages.forEach(function(pg, i){ var q = [p[0] + i * (f.w + 20) * sc * Math.cos(rot * Math.PI / 180), p[1] + i * (f.w + 20) * sc * Math.sin(rot * Math.PI / 180)]; ids.push(ED.add(mkPdfu(f, pg, q, sc, rot)).id); }); });
            pdfRec(d, f); d.sel = ids.slice(0, 1); TC.cad.afterSel(); A().refreshPalettes(); V().draw();
            ctx.out('Đã gắn ' + pages.length + ' trang của <b>' + esc(f.name) + '</b> làm PDF underlay (tỉ lệ ' + TC.d(sc, 2) + '). Chỉnh độ mờ, layer PDF, clip trên tab ngữ cảnh PDF underlay.'); res(); }); }, function(){ res(); });
      });
      render();
    });
  };

  /* ================= Tab ngữ cảnh PDF underlay (UI-G15), ULAYERS, UOSNAP ================= */
  function pdfTab(e){
    var set = function(k, v, label){ ED.run(label || 'Điều chỉnh PDF', function(){ ED.mod(e, function(x){ if (v === undefined) delete x[k]; else x[k] = v; }); }); V().draw(); A().setCtxTab(pdfTab(e), true); A().refreshPalettes(); };
    return { id:'pdfu', name:'PDF underlay', ctx:true, panels:[
      { name:'Điều chỉnh', icon:'contrast', prio:1, items:[ { t:'col', gap:3, items:[ { t:'num', id:'pu-c', label:'Tương phản', w:52, get:function(){ return String(e.contrast == null ? 20 : e.contrast); }, set:function(v){ set('contrast', TC.clamp(Math.round(num(v, 20)), 0, 100)); } }, { t:'num', id:'pu-f', label:'Mờ', w:52, get:function(){ return String(e.fade == null ? 25 : e.fade); }, set:function(v){ set('fade', TC.clamp(Math.round(num(v, 25)), 0, 80)); } }, { t:'s', label:'Đơn sắc', icon:'contrast', id:'pu-mono', toggle:true, on:function(){ return !!e.mono; }, act:function(){ set('mono', !e.mono); } } ] } ] },
      { name:'Clip', icon:'zwin', prio:2, items:[ { t:'lg', label:'Tạo đường\nclip', icon:'zwin', id:'pu-clip', act:function(){ TC.cmd.run('CLIP', e); } }, { t:'lg', label:'Bỏ clip', icon:'erase', id:'pu-unclip', act:function(){ if (e.clip) set('clip', undefined, 'Bỏ clip PDF'); else TC.cmdline.msg('Underlay chưa có đường clip.'); } } ] },
      { name:'Tùy chọn', icon:'gear', prio:2, items:[ { t:'col', items:[ { t:'s', label:'Hiện underlay', icon:'eye', id:'pu-show', toggle:true, on:function(){ return e.show !== false; }, act:function(){ set('show', e.show === false ? undefined : false); } }, { t:'s', label:'Bắt điểm vào PDF', icon:'osnap', id:'pu-snap', toggle:true, on:function(){ return D().uosnap !== 0; }, act:function(){ TC.cmd.run('UOSNAP', D().uosnap === 0 ? 'ON' : 'OFF'); setTimeout(function(){ A().setCtxTab(pdfTab(e), true); }, 0); } }, { t:'s', label:'Chỉnh màu theo nền', icon:'transp', id:'pu-bg', toggle:true, desc:'Nét đen của PDF hiện sáng trên nền vùng vẽ tối.', on:function(){ return e.bgadj !== false; }, act:function(){ set('bgadj', e.bgadj === false ? undefined : false); } } ] } ] },
      { name:'Layer PDF', icon:'layers', prio:1, items:[ { t:'lg', label:'Layer\ncủa PDF', icon:'layers', id:'pu-lay', cmd:'ULAYERS', act:function(){ TC.cmd.run('ULAYERS', e); } } ] },
      { name:'Nhập', icon:'pdf', prio:1, items:[ { t:'lg', label:'Nhập thành\nđối tượng', icon:'pdf', id:'pu-imp', cmd:'PDFIMPORT', act:function(){ TC.cmd.run('PDFIMPORT', e); } } ] },
      { name:'Tham chiếu ngoài', icon:'xref', prio:3, items:[ { t:'lg', label:'Mở\npalette', icon:'xref', id:'pu-xr', act:function(){ A().showPalette('xref'); } } ] }
    ] };
  }
  // CLIP cho PDF underlay: khung chữ nhật lưu theo tọa độ trang
  var clip0 = IMPL.CLIP;
  IMPL.CLIP = function(ctx, pre){ var u = pre && pre.t === 'pdfu' ? pre : null, d = D(); if (!u && d.sel.length === 1 && d.byId[d.sel[0]] && d.byId[d.sel[0]].t === 'pdfu') u = d.byId[d.sel[0]]; if (!u) return clip0(ctx, pre);
    return ctx.kw('Tùy chọn clip', [['Biên mới', 'N'], ['Xóa', 'D']], 'N').then(function(k){ if (k === 'D'){ if (u.clip){ ED.run('CLIP', function(){ ED.mod(u, function(x){ delete x.clip; }); }); V().draw(); } ctx.out(u.clip ? 'Đã bỏ biên clip.' : 'Underlay chưa có biên clip.'); return; }
      return ctx.point('Góc thứ nhất').then(function(a){ if (!a.p) return; return ctx.point('Góc đối', { base:a.p, rubber:false, dyn:'wh', onMove:function(p, temp){ temp.push({ k:'poly', pts:rp(a.p[0], a.p[1], p[0], p[1]), closed:true, dash:[6, 4], color:TC.cssv('--c-accent') }); } }).then(function(q){ if (!q.p) return; var Mi = mInv(pdfM(u)), pa = mApply(Mi, a.p), pb = mApply(Mi, q.p), c = [Math.min(pa[0], pb[0]), Math.min(pa[1], pb[1]), Math.max(pa[0], pb[0]), Math.max(pa[1], pb[1])]; ED.run('CLIP', function(){ ED.mod(u, function(x){ x.clip = c; }); }); V().draw(); A().setCtxTab(pdfTab(u), true); ctx.out('Đã clip PDF underlay theo khung chữ nhật. Bỏ clip trên tab ngữ cảnh.'); }); }); }); };
  IMPL.UOSNAP = function(ctx, pre){ var d = D(); var go = function(k){ d.uosnap = k === 'OFF' ? 0 : 1; ctx.out('UOSNAP = ' + d.uosnap + (d.uosnap ? ': bắt điểm vào hình học của PDF underlay.' : ': không bắt điểm vào PDF underlay.')); var t = A().ctxTab; if (t && t.id === 'pdfu') ED.ctxRefresh(); }; if (pre) return go(String(pre).toUpperCase()); return ctx.kw('Bắt điểm vào underlay', [['Bật', 'ON'], ['Tắt', 'OFF']], d.uosnap === 0 ? 'OFF' : 'ON').then(go); };
  // bật/tắt layer của PDF: xem trước ngay trên vùng vẽ; áp dụng là một bước hoàn tác
  IMPL.ULAYERS = function(ctx, pre){
    var d = D(), pick = pre && pre.t === 'pdfu' ? Promise.resolve({ e:pre }) : d.sel.length === 1 && d.byId[d.sel[0]] && d.byId[d.sel[0]].t === 'pdfu' ? Promise.resolve({ e:d.byId[d.sel[0]] }) : ED.spaceList(d).some(function(e){ return e.t === 'pdfu'; }) ? ED.pick(ctx, 'Chọn PDF underlay', { types:['pdfu'], bad:'Chọn một PDF underlay' }) : Promise.resolve({});
    return pick.then(function(r){ var u = r.e; if (!u){ ctx.out('Bản vẽ chưa có PDF underlay. Gắn PDF bằng PDFATTACH.'); return; }
      var f = pdfFile(u.name) || PDFS[0], items = pdfItems(f, u.page || 1), cnt = {}, off0 = (u.off || []).slice(), off = off0.slice(), q = ''; items.forEach(function(it){ cnt[it.pl] = (cnt[it.pl] || 0) + 1; }); var layers = Object.keys(cnt);
      return new Promise(function(res){
        var dlg = TC.dialog({ id:'ulayers', title:'Layer của underlay', cmd:'ULAYERS', code:'UI-G15', w:460, onClose:function(rr){ if (rr !== true){ u.off = off0; TC.prepEnt(d, u); V().draw(); } res(); },
          body:'<div class="stack"><div class="hrow"><span>' + ic('pdf') + ' <b>' + esc(u.name) + '</b> · trang ' + (u.page || 1) + '</span></div><div class="hrow"><label class="search grow">' + ic('search') + '<input id="ul-q" placeholder="Tìm layer" aria-label="Tìm layer"></label><button class="btn sm" type="button" id="ul-on">Bật tất cả</button><button class="btn sm" type="button" id="ul-off">Tắt tất cả</button></div><div class="tblwrap" style="height:236px" role="listbox"><table class="tbl"><thead><tr><th class="ctr" style="width:36px">Bật</th><th>Layer PDF</th><th class="num" style="width:90px">Đối tượng</th></tr></thead><tbody id="ul-l"></tbody></table></div><span class="faint" style="font-size:var(--fs-s)">Xem trước ngay trên vùng vẽ. Chỉ đổi hiển thị trong bản vẽ này, tệp PDF không đổi.</span></div>',
          foot:'<span class="faint" id="ul-n" style="font-size:var(--fs-s)"></span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="ul-ok">Áp dụng</button>' });
        function render(){ var F = TC.fold(q), list = layers.filter(function(n){ return !F || TC.fold(n).indexOf(F) >= 0; }); $('#ul-l', dlg.el).innerHTML = list.length ? list.map(function(n){ var on = off.indexOf(n) < 0; return '<tr><td class="ctr"><button class="tgl' + (on ? '' : ' off') + '" type="button" data-ul="' + esc(n) + '" aria-pressed="' + on + '" aria-label="' + (on ? 'Tắt' : 'Bật') + ' layer ' + esc(n) + '">' + ic(on ? 'bulb' : 'bulboff') + '</button></td><td>' + esc(n) + '</td><td class="num">' + cnt[n] + '</td></tr>'; }).join('') : '<tr><td colspan="3"><div class="empty" style="padding:12px">Không có layer khớp “' + esc(q) + '”.</div></td></tr>';
          $('#ul-n', dlg.el).textContent = (layers.length - off.length) + '/' + layers.length + ' layer đang bật'; u.off = off.slice(); TC.prepEnt(d, u); V().draw(); }
        dlg.el.addEventListener('click', function(e){ var b = e.target.closest('[data-ul]'); if (b){ var n = b.dataset.ul, i = off.indexOf(n); if (i >= 0) off.splice(i, 1); else off.push(n); render(); var nb = $('[data-ul="' + n + '"]', dlg.el); if (nb) nb.focus(); } });
        $('#ul-q', dlg.el).addEventListener('input', function(){ q = this.value; render(); });
        $('#ul-on', dlg.el).addEventListener('click', function(){ off = []; render(); }); $('#ul-off', dlg.el).addEventListener('click', function(){ off = layers.slice(); render(); });
        $('#ul-ok', dlg.el).addEventListener('click', function(){ var fin = off.slice(); u.off = off0; ED.run('ULAYERS', function(){ ED.mod(u, function(x){ x.off = fin; }); }); V().draw(); A().refreshPalettes(); dlg.close(true); ctx.out(fin.length ? 'Đã tắt ' + fin.length + ' layer của PDF: ' + fin.map(esc).join(', ') + '.' : 'Mọi layer của PDF đang bật.'); });
        render();
      });
    });
  };

  /* ================= Nhập PDF thành đối tượng UI-G14 (PDFIMPORT) ================= */
  var PI = TC.load('pdfimp', { geo:true, fill:true, ttf:true, img:false, layer:'pdf', block:false, join:true, hatch:true, lw:true, ltype:true, shxOn:true, fonts:['romans.shx', 'txt.shx'], thr:75, merge:true, detach:true, pos:'scr' });
  function shxConf(s){ var h = 7; for (var i = 0; i < s.length; i++) h = (h * 31 + s.charCodeAt(i)) % 997; return 60 + h % 40; } // độ tin cậy mô phỏng của nhận dạng chữ SHX
  // tạo đối tượng thật từ nội dung trang; M: trang → bản vẽ; gọi trong ED.run
  function pdfImport(d, f, pg, M, o){
    var tt = tOf(M), items = pdfItems(f, pg), rep = { geo:0, arc:0, fill:0, ttf:0, shx:0, shxOk:0, img:0, lt:0, mt:0, joined:0, layers:[] }, made = [];
    var lay = function(it, kind){ return o.layer === 'pdf' ? it.pl : o.layer === 'type' ? { geo:'PDF_Geometry', fill:'PDF_Solid Fills', img:'PDF_Images', text:'PDF_Text' }[kind] : ED.clayer(d); };
    var mk = function(it, kind, extra){ var c = Object.assign({}, it, extra || {}); ['pl', 'plw', 'shx', 'ax', 'raster'].forEach(function(k){ delete c[k]; }); c.layer = lay(it, kind); if (o.layer === 'pdf' || it.color === 7) delete c.color; if (o.lw && kind !== 'text' && kind !== 'img') c.lw = it.plw; return xfEnt(c, tt); };
    var lines = [], axes = {};
    if (o.geo) items.forEach(function(it){ if (it.t !== 'line') return; if (o.ltype && it.ax) (axes[it.ax] = axes[it.ax] || []).push(it); else lines.push(mk(it, 'geo')); });
    Object.keys(axes).forEach(function(k){ var g = axes[k], pts = []; g.forEach(function(it){ pts.push(it.p1, it.p2); }); var hz = /^h/.test(k); pts.sort(function(a, b){ return hz ? a[0] - b[0] : a[1] - b[1]; }); made.push(mk(g[0], 'geo', { p1:pts[0], p2:pts[pts.length - 1], lt:'CENTER' })); rep.lt++; });
    if (o.join){ var byL = {}; lines.forEach(function(e){ (byL[e.layer] = byL[e.layer] || []).push(e); }); lines = []; Object.keys(byL).forEach(function(k){ var pool = byL[k]; while (pool.length){ var j = pool.length > 1 ? ED.join(pool) : null; if (j && j.used.length > 1){ Object.keys(j.e).forEach(function(kk){ if (j.e[kk] === undefined) delete j.e[kk]; }); delete j.e.id; lines.push(j.e); rep.joined += j.used.length; pool = pool.filter(function(x){ return j.used.indexOf(x) < 0; }); } else lines.push(pool.shift()); } }); }
    made = made.concat(lines); rep.geo = made.length;
    items.forEach(function(it){
      if (it.t === 'circle' || it.t === 'arc'){ if (o.geo){ made.push(mk(it, 'geo')); rep.arc++; } }
      else if (it.t === 'hatch'){ if (!o.fill) return; made.push(o.hatch ? mk(it, 'fill') : mk({ t:'pline', pts:it.loops[0], closed:true, pl:it.pl, plw:it.plw, color:it.color }, 'fill')); rep.fill++; }
      else if (it.t === 'image'){ if (o.img){ made.push(mk(it, 'img', { frame:true, bright:50, contrast:50, fade:0 })); rep.img++; } }
      else if (it.t === 'text'){
        if (!it.shx){ if (o.ttf){ made.push(mk(it, 'text')); rep.ttf++; } return; }
        rep.shx++; if (o.shxOn && shxConf(it.s) >= o.thr){ made.push(mk(it, 'text')); rep.shxOk++; return; }
        if (o.geo){ var w = mk(it, 'geo'); textLoops(d, w).forEach(function(l){ made.push({ t:'pline', pts:l, closed:true, layer:w.layer, color:w.color }); rep.geo++; }); }
      }
    });
    // gộp các dòng chữ thẳng cột thành MText (TXT2MTXT)
    if (o.merge){ var tx = made.filter(function(e){ return e.t === 'text' && (e.al || 'l') === 'l' && !(e.rot % 360); }).sort(function(a, b){ return b.p[1] - a.p[1]; }), used = [];
      tx.forEach(function(a){ if (used.indexOf(a) >= 0) return; var grp = [a], last = a; tx.forEach(function(b){ if (b === a || used.indexOf(b) >= 0 || b.layer !== a.layer || b.style !== a.style) return; if (Math.abs(b.p[0] - a.p[0]) < a.h * .5 && last.p[1] - b.p[1] > 0 && last.p[1] - b.p[1] <= a.h * 2.6){ grp.push(b); last = b; } });
        if (grp.length < 2) return; used = used.concat(grp); var gap = (grp[0].p[1] - grp[grp.length - 1].p[1]) / (grp.length - 1);
        made = made.filter(function(e){ return grp.indexOf(e) < 0; }); var mt = { t:'mtext', p:[a.p[0], a.p[1] + a.h], w:Math.max.apply(null, grp.map(function(e){ return e.s.length * e.h * .64; })), h:grp[1].h, ls:gap / grp[1].h, s:grp.map(function(e){ return e.s; }).join('\n'), style:a.style, layer:a.layer }; if (a.color != null) mt.color = a.color; made.push(mt); rep.mt++; rep.ttf -= grp.length; }); }
    var need = {}; made.forEach(function(e){ need[e.layer] = 1; }); var done = layerState(d);
    Object.keys(need).forEach(function(n){ if (addLayer(d, n, o.layer === 'pdf' && PDFL[n] ? { color:PDFL[n][0], lw:PDFL[n][1], desc:'Layer của PDF ' + f.name } : { color:n === 'PDF_Solid Fills' ? 8 : 7, desc:'Nhập từ ' + f.name })) rep.layers.push(n); }); done();
    var ids = [];
    if (o.block){ var base = [M[4], M[5]], bn = 'PDF-' + f.name.replace(/\.pdf$/i, '') + '-T' + pg, k = 2; while (d.blocks[bn + (k > 2 ? '-' + (k - 1) : '')]) k++; bn += k > 2 ? '-' + (k - 1) : ''; var dd = defState(d, [bn]); d.blocks[bn] = { name:bn, desc:'Nhập từ ' + f.name + ', trang ' + pg, ents:made.map(function(e){ var c = ED.xform(e, T.move(-base[0], -base[1])); c.id = ++d.seq; return c; }) }; dd(); ids.push(ED.add(ED.make({ t:'insert', name:bn, p:base, sx:1, sy:1, rot:0 })).id); rep.block = bn; }
    else made.forEach(function(e){ ids.push(ED.add(e).id); });
    rep.n = made.length; return { ids:ids, rep:rep };
  }
  function pdfProgress(steps, fin){
    var i = 0, t = null, dlg = TC.dialog({ id:'pdfprog', title:'Đang nhập PDF', code:'UI-G14', w:440, help:false, onClose:function(r){ clearTimeout(t); if (r !== true) TC.toast('Đã hủy nhập PDF. Bản vẽ không thay đổi.', { type:'info', log:false }); },
      body:'<div class="stack"><div class="prog"><i id="pp-b" style="width:0%"></i></div><div class="pps">' + steps.map(function(s, k){ return '<div class="ppi" data-k="' + k + '"><span class="ppk"></span>' + esc(s) + '</div>'; }).join('') + '</div></div>', foot:'<span class="faint" id="pp-t" style="font-size:var(--fs-s)"></span><span class="grow"></span><button class="btn" type="button" data-x="cancel">Hủy</button>', focus:'[data-x="cancel"]' });
    function tick(){ $$('.ppi', dlg.el).forEach(function(r, k){ r.classList.toggle('done', k < i); r.classList.toggle('cur', k === i); $('.ppk', r).innerHTML = k < i ? ic('check', 's12') : k === i ? '<i class="spin"></i>' : ''; }); $('#pp-b', dlg.el).style.width = Math.round(i / steps.length * 100) + '%'; $('#pp-t', dlg.el).textContent = i < steps.length ? 'Bước ' + (i + 1) + '/' + steps.length + ' · còn khoảng ' + Math.max(1, Math.round((steps.length - i) * .45)) + ' giây' : 'Xong';
      if (i >= steps.length){ t = setTimeout(function(){ dlg.close(true); fin(); }, 250); return; } i++; t = setTimeout(tick, 450); }
    tick();
  }
  IMPL.PDFIMPORT = function(ctx, pre){
    var d = D(), unders = ED.spaceList(d).filter(function(e){ return e.t === 'pdfu'; }), u = pre && pre.t === 'pdfu' ? pre : d.sel.length === 1 && d.byId[d.sel[0]] && d.byId[d.sel[0]].t === 'pdfu' ? d.byId[d.sel[0]] : null;
    var src = u ? 'u' : 'f', f = pre && pre.file ? pdfFile(pre.file) : u ? pdfFile(u.name) : PDFS[0], pg = pre && pre.page || (u ? u.page : 1) || 1, o = JSON.parse(JSON.stringify(PI));
    return new Promise(function(res){
      var c3 = function(id, label, k){ return H.chk(id, label, !!o[k]); };
      var dlg = TC.dialog({ id:'pdfimp', title:'Nhập PDF thành đối tượng', cmd:'PDFIMPORT', code:'UI-G14', w:880, onClose:function(r){ if (r !== true) res(); },
        body:'<div class="pim"><div class="stack">' + H.grp('Nguồn', '<div class="hrow">' + H.rad('pis', 'u', 'PDF underlay', src === 'u') + '<button class="btn sm" type="button" id="pi-pick"' + (unders.length ? '' : ' disabled') + '>' + ic('select') + 'Chọn underlay</button></div><span class="faint" id="pi-u" style="font-size:var(--fs-s);padding-left:20px"></span>' + H.rad('pis', 'f', 'Tệp PDF', src === 'f') + '<div class="hrow" style="padding-left:20px"><select class="inp" id="pi-f" style="width:230px" aria-label="Tệp PDF">' + PDFS.filter(function(x){ return !x.pass; }).map(function(x){ return '<option' + (x === f ? ' selected' : '') + '>' + esc(x.name) + '</option>'; }).join('') + '</select><button class="btn sm" type="button" id="pi-br">Duyệt…</button></div>' + H.row('Trang', '<select class="inp" id="pi-pg" style="width:230px"></select>', 40)) +
          H.grp('Vị trí, tỉ lệ, góc', H.rad('pip', 'scr', 'Chỉ định trên màn hình', o.pos === 'scr') + H.rad('pip', 'org', 'Gốc tọa độ (0,0)', o.pos === 'org') + H.row('Tỉ lệ', H.num('pi-s', '100.0000', { prec:4, unit:'none', w:110, pick:false }), 46) + H.row('Góc', H.num('pi-r', '0', { prec:0, unit:'deg', w:110, pick:false }), 46) + '<span class="faint" id="pi-pn" style="font-size:var(--fs-xs)"></span>') + '</div>' +
          '<div class="stack">' + H.grp('Dữ liệu nhập', c3('pi-geo', 'Hình học vector', 'geo') + c3('pi-fill', 'Vùng tô đặc', 'fill') + c3('pi-ttf', 'Chữ TrueType', 'ttf') + c3('pi-img', 'Ảnh raster', 'img')) +
            H.grp('Layer', H.rad('pil', 'pdf', 'Dùng layer của PDF', o.layer === 'pdf') + H.rad('pil', 'type', 'Tạo layer theo loại đối tượng', o.layer === 'type') + H.rad('pil', 'cur', 'Layer hiện hành', o.layer === 'cur')) +
            H.grp('Tùy chọn', c3('pi-block', 'Nhập thành block', 'block') + c3('pi-join', 'Nối đoạn thẳng và cung', 'join') + c3('pi-hatch', 'Chuyển vùng tô đặc thành hatch', 'hatch') + c3('pi-lw', 'Áp độ dày nét', 'lw') + c3('pi-lt', 'Suy ra linetype từ nét đứt thẳng hàng', 'ltype') + '<div id="pi-dw">' + c3('pi-det', 'Gỡ underlay sau khi nhập', 'detach') + '</div>') + '</div>' +
          '<div class="stack"><canvas id="pi-pv" width="232" height="164" class="pvc"></canvas>' + H.grp('Nhận dạng chữ SHX', c3('pi-shx', 'Nhận dạng chữ SHX', 'shxOn') + '<div class="pishx">' + ['romans.shx', 'txt.shx', 'simplex.shx', 'isocp.shx'].map(function(n){ return H.chk('', '<span class="mono">' + n + '</span>', o.fonts.indexOf(n) >= 0).replace('<input', '<input data-font="' + n + '"'); }).join('') + '</div>' + H.row('Ngưỡng tin cậy', '<input type="range" id="pi-thr" min="50" max="100" value="' + o.thr + '" style="width:96px;accent-color:var(--c-accent)" aria-label="Ngưỡng tin cậy"><span class="mono" id="pi-thv">' + o.thr + '%</span>', 96) + c3('pi-mt', 'Gộp chữ thành đoạn (TXT2MTXT)', 'merge')) + '<div id="pi-msg"></div></div></div>',
        foot:'<span class="faint" style="font-size:var(--fs-s)">Toàn bộ lần nhập là một bước hoàn tác.</span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="pi-ok">Nhập PDF</button>' });
      function read(){ ['geo', 'fill', 'ttf', 'img', 'block', 'join', 'hatch', 'lw', 'det', 'shx', 'mt'].forEach(function(k){ var el = $('#pi-' + k, dlg.el); if (el) o[{ lt:'ltype', det:'detach', shx:'shxOn', mt:'merge' }[k] || k] = el.checked; }); o.ltype = $('#pi-lt', dlg.el).checked; o.layer = $('[name="pil"]:checked', dlg.el).value; o.pos = $('[name="pip"]:checked', dlg.el).value; o.thr = Number($('#pi-thr', dlg.el).value); o.fonts = $$('[data-font]', dlg.el).filter(function(c){ return c.checked; }).map(function(c){ return c.dataset.font; }); src = $('[name="pis"]:checked', dlg.el).value; }
      function render(){
        read(); var isU = src === 'u' && u; if (isU){ f = pdfFile(u.name); pg = u.page || 1; }
        $('#pi-u', dlg.el).innerHTML = u ? esc(u.name) + ' · trang ' + (u.page || 1) : unders.length ? 'Bấm Chọn underlay để chọn trên bản vẽ' : 'Bản vẽ chưa có PDF underlay';
        $('[name="pis"][value="u"]', dlg.el).disabled = !u; $('#pi-f', dlg.el).disabled = $('#pi-pg', dlg.el).disabled = !!isU;
        $('#pi-pg', dlg.el).innerHTML = f.pages.map(function(t, i){ return '<option value="' + (i + 1) + '"' + (i + 1 === pg ? ' selected' : '') + '>' + (i + 1) + ' · ' + esc(t) + '</option>'; }).join('');
        $$('[name="pip"], #pi-s input, #pi-r input', dlg.el).forEach(function(x){ x.disabled = !!isU; }); $('#pi-pn', dlg.el).textContent = isU ? 'Dùng vị trí, tỉ lệ, góc của underlay.' : ''; $('#pi-dw', dlg.el).hidden = !isU;
        $$('.pishx input, #pi-thr', dlg.el).forEach(function(x){ x.disabled = !o.shxOn; }); $('#pi-thv', dlg.el).textContent = o.thr + '%';
        pdfThumb($('#pi-pv', dlg.el), f, pg);
        var msg = !f.vec && !o.img ? ['warn', 'Trang này là ảnh quét, không có hình học vector. Bật <b>Ảnh raster</b> để nhập ảnh, hoặc gắn PDF làm underlay để vẽ đè.'] : !(o.geo || o.fill || o.ttf || o.img) ? ['err', 'Chọn ít nhất một loại dữ liệu để nhập.'] : o.shxOn && !o.fonts.length ? ['err', 'Chọn ít nhất một font SHX để so khớp, hoặc tắt nhận dạng chữ SHX.'] : null;
        $('#pi-msg', dlg.el).innerHTML = msg ? '<div class="note ' + msg[0] + '">' + ic('warn') + '<span>' + msg[1] + '</span></div>' : ''; $('#pi-ok', dlg.el).disabled = !!msg && msg[0] === 'err';
      }
      dlg.el.addEventListener('change', function(e){ if (e.target.id === 'pi-f'){ f = pdfFile(e.target.value); pg = 1; } if (e.target.id === 'pi-pg') pg = Number(e.target.value); render(); });
      $('#pi-thr', dlg.el).addEventListener('input', function(){ $('#pi-thv', dlg.el).textContent = this.value + '%'; });
      $('#pi-br', dlg.el).addEventListener('click', function(){ TC.toast('Mở hộp thoại chọn tệp PDF của hệ điều hành.', { type:'info', log:false }); });
      $('#pi-pick', dlg.el).addEventListener('click', function(){ dlg.hide(true); ED.pick(ctx, 'Chọn PDF underlay', { types:['pdfu'], bad:'Chọn một PDF underlay' }).then(function(r){ dlg.hide(false); if (r.e){ u = r.e; $('[name="pis"][value="u"]', dlg.el).checked = true; } render(); }, function(){ dlg.hide(false); }); });
      $('#pi-ok', dlg.el).addEventListener('click', function(){
        read(); var sc = num($('#pi-s', dlg.el).value, 100) || 100, rot = num($('#pi-r', dlg.el).value, 0), isU = src === 'u' && u; PI = JSON.parse(JSON.stringify(o)); TC.save('pdfimp', PI); dlg.close(true);
        var place = isU ? Promise.resolve(pdfM(u)) : o.pos === 'org' ? Promise.resolve(pdfM({ p:[0, 0], sc:sc, rot:rot })) : ctx.point('Điểm chèn (góc dưới trái trang)', { onMove:function(p, temp){ temp.push(ED.temp([mkPdfu(f, pg, p, sc, rot)], { real:true })); } }).then(function(r){ return r.p ? pdfM({ p:r.p, sc:sc, rot:rot }) : null; });
        place.then(function(M){ if (!M) return res();
          var steps = ['Đọc trang ' + pg + ' của ' + f.name, 'Hình học vector', 'Vùng tô đặc', 'Chữ TrueType', 'Nhận dạng chữ SHX (' + (o.fonts.join(', ') || 'tắt') + ')', 'Gộp đoạn, tạo layer'];
          pdfProgress(steps, function(){ var out; ED.run('PDFIMPORT', function(){ out = pdfImport(d, f, pg, M, o); if (isU && o.detach) ED.del(u); }); TC.cad.clearSel(); V().draw(); A().refreshPalettes(); pdfReport(f, pg, out, o, isU); res(); });
        }, function(){ res(); });
      });
      render();
    });
  };
  function pdfReport(f, pg, out, o, isU){
    var r = out.rep, d = D(), row = function(a, b){ return '<tr><td>' + a + '</td><td class="num">' + b + '</td></tr>'; };
    if (!out.ids.length){ TC.toast('Không có đối tượng nào để nhập với các lựa chọn hiện tại.', { type:'warn', log:false }); return; }
    var dlg = TC.dialog({ id:'pdfrep', title:'Kết quả nhập PDF', code:'UI-G14', w:520,
      body:'<div class="stack"><div class="hrow" style="flex-wrap:nowrap;align-items:flex-start">' + ic('okc', 'st') + '<span>Đã nhập trang ' + pg + ' của <b>' + esc(f.name) + '</b> thành <b>' + TC.n(r.n) + '</b> đối tượng' + (r.block ? ' trong block <b>' + esc(r.block) + '</b>' : '') + (isU && o.detach ? '; đã gỡ underlay' : '') + '.</span></div>' +
        '<div class="tblwrap"><table class="tbl"><tbody>' + row('Đường thẳng, polyline' + (r.joined ? ' <span class="faint">(nối từ ' + r.joined + ' đoạn)</span>' : ''), r.geo) + row('Cung, đường tròn', r.arc) + row(o.hatch ? 'Hatch (từ vùng tô đặc)' : 'Vùng tô đặc (polyline)', r.fill) + row('Chữ TrueType' + (r.mt ? ' <span class="faint">(' + r.mt + ' đoạn MText)</span>' : ''), r.ttf + r.mt) + row('Chữ SHX nhận dạng được', o.shxOn ? r.shxOk + '/' + r.shx : 'tắt') + row('Ảnh raster', r.img) + row('Đường có linetype suy ra', r.lt) + '</tbody></table></div>' +
        (o.shxOn && r.shxOk < r.shx ? '<div class="note warn">' + ic('warn') + '<span>' + (r.shx - r.shxOk) + ' chữ SHX dưới ngưỡng ' + o.thr + '% được giữ dạng hình học. Hạ ngưỡng hoặc thêm font SHX nếu cần nhận dạng hết.</span></div>' : '') +
        (r.layers.length ? '<div class="faint" style="font-size:var(--fs-s)">Layer mới: ' + r.layers.map(esc).join(', ') + '</div>' : '') + '</div>',
      foot:'<button class="btn" type="button" id="pr-sel">' + ic('select') + 'Chọn ' + TC.n(out.ids.length) + ' đối tượng</button><span class="grow"></span><button class="btn primary" type="button" data-x="close">Đóng</button>' });
    $('#pr-sel', dlg.el).addEventListener('click', function(){ d.sel = out.ids.filter(function(id){ return d.byId[id]; }); TC.cad.afterSel(); V().zoomTo(ED.bbox(d.sel)); dlg.close(true); });
    TC.cmdline.msg('PDFIMPORT: ' + TC.n(r.n) + ' đối tượng từ trang ' + pg + ' của ' + esc(f.name) + '. Ctrl+Z để hoàn tác cả lần nhập.');
  }

  /* ================= Thay block UI-G16 (BLOCKREPLACE); Tìm & chuyển đổi (TCBLOCKMATCH) sau v1.0 ================= */
  function libList(){ var out = []; (SC.LIB || []).forEach(function(g){ g[2].forEach(function(x){ out.push({ name:x[0], label:x[1], desc:x[2], anno:!!x[4] }); }); }); return out; }
  function ghostIns(d, name, e, keep){ var src = d.blocks[name] ? d : (TD = TD || thumbDoc(), SC.ensureBlock(TD, name), TD), li = libList().filter(function(x){ return x.name === name; })[0], k = li && li.anno ? 100 : 1; return TC.prepEnt(src, { t:'insert', name:name, p:e.p, sx:keep ? e.sx : k, sy:keep ? e.sy : k, rot:keep ? e.rot || 0 : 0, layer:e.layer }, true); }
  IMPL.BLOCKREPLACE = function(ctx, pre){
    var d = D(), names = userBlocks(d).filter(function(n){ return allIns(d, n).length; }).sort(), selIns = d.sel.map(function(id){ return d.byId[id]; }).filter(function(e){ return e && e.t === 'insert' && !e.xref; });
    if (!names.length){ ctx.out('Bản vẽ chưa có block nào được chèn để thay.'); return; }
    var mode = selIns.length && !(pre && pre.from) ? 'sel' : 'all', from = pre && pre.from || (selIns[0] && selIns[0].name) || names[0], to = pre && pre.to || null, src = 'dwg', q = '', picked = selIns.slice(), tab = 'r';
    return new Promise(function(res){
      var dlg = TC.dialog({ id:'brep', title:'Thay block', cmd:'BLOCKREPLACE', code:'UI-G16', w:760, modal:false, onClose:function(r){ d.found = []; V().temp = []; V().drawOver(); if (r !== true) res(); },
        body:'<div class="stack"><div class="tabs" role="tablist" data-dtabs="br"><button class="tab" role="tab" type="button" data-dt="r" aria-selected="true">Thay block</button><button class="tab" role="tab" type="button" data-dt="m" aria-selected="false">' + ic('lock', 's12') + 'Tìm và chuyển đổi <span class="tag">sau v1.0</span></button></div>' +
          H.pane('r', 'r', '<div class="brw"><div class="stack">' + H.grp('Block cần thay', H.rad('brm', 'all', 'Mọi block cùng tên', mode === 'all') + '<div style="padding-left:20px"><select class="inp" id="br-from" style="width:100%" aria-label="Block cần thay">' + names.map(function(n){ return '<option value="' + esc(n) + '"' + (n === from ? ' selected' : '') + '>' + esc(n) + ' (' + allIns(d, n).length + ')</option>'; }).join('') + '</select></div>' + H.rad('brm', 'sel', 'Chỉ các block đã chọn', mode === 'sel') + '<div class="hrow" style="padding-left:20px"><button class="btn sm" type="button" id="br-pick">' + ic('select') + 'Chọn trên bản vẽ</button><span class="faint" id="br-sn" style="font-size:var(--fs-s)"></span></div>') +
            H.grp('Tùy chọn', H.chk('br-keep', 'Giữ tỉ lệ và góc', true) + H.chk('br-map', 'Ánh xạ attribute theo tag', true) + H.chk('br-purge', 'Dọn định nghĩa block cũ', false) + '<span class="fmsg hint" id="br-ph"></span>') + '</div>' +
            '<div class="stack" style="gap:6px"><div class="gh">Block thay thế</div>' + SC.seg('brs', ['Trong bản vẽ', 'Thư viện'], 'Trong bản vẽ') + '<label class="search">' + ic('search') + '<input id="br-q" placeholder="Tìm block" aria-label="Tìm block thay thế"></label><div class="tblwrap brl" id="br-l" role="listbox" tabindex="0"></div></div>' +
            '<div class="stack" style="gap:6px">' + H.cv('br-pv', 200, 150) + '<div id="br-d" class="faint" style="font-size:var(--fs-s)"></div><div class="note" id="br-hint">' + ic('info') + '<span>Bản xem trước hiện trên vùng vẽ: block bị thay tô cam, block mới vẽ nét đứt.</span></div></div></div>') +
          H.pane('m', 'r', '<div class="empty" style="padding:36px 16px">' + ic('lock') + '<b>Tìm và chuyển đổi (TCBLOCKMATCH) có ở sau v1.0</b>Chọn một cụm hình học mẫu; TrionCAD tìm các cụm tương tự (kể cả khi xoay, đổi tỉ lệ) để chuyển thành block, đếm hoặc chọn.</div>') + '</div>',
        foot:'<span class="faint" id="br-sum" style="font-size:var(--fs-s)"></span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="br-ok">Thay block</button>' });
      dlg.el.style.left = Math.max(12, innerWidth - 760 - 300) + 'px';
      function targets(){ return mode === 'all' ? allIns(d, from) : picked.filter(function(e){ return d.byId[e.id]; }); }
      function cands(){ var F = TC.fold(q); return (src === 'dwg' ? userBlocks(d).sort().map(function(n){ return { name:n, label:d.blocks[n].desc || '' }; }) : libList()).filter(function(x){ return !F || TC.fold(x.name + ' ' + x.label).indexOf(F) >= 0; }); }
      function list(){ var c = cands(); $('#br-l', dlg.el).innerHTML = c.length ? c.map(function(x){ return '<button class="bri" type="button" role="option" data-bt="' + esc(x.name) + '" aria-selected="' + (x.name === to) + '"><canvas width="40" height="28"></canvas><span class="nm">' + esc(x.name) + '<i>' + esc(x.label) + '</i></span></button>'; }).join('') : '<div class="empty" style="padding:14px">Không có block khớp “' + esc(q) + '”.</div>'; $$('[data-bt] canvas', dlg.el).forEach(function(cv){ libThumb(cv, cv.parentNode.dataset.bt); }); }
      function render(){
        var tg = targets(), keep = $('#br-keep', dlg.el).checked, same = to && tg.some(function(e){ return e.name === to; }), left = mode === 'sel' ? allIns(d, from).length - tg.filter(function(e){ return e.name === from; }).length : 0, msg = !tg.length ? 'Chưa có block nào để thay. Chọn trên bản vẽ.' : !to ? 'Chọn block thay thế ở danh sách giữa.' : same ? 'Block thay thế trùng với block cần thay.' : null;
        $('#br-from', dlg.el).disabled = mode !== 'all'; $('#br-pick', dlg.el).disabled = mode !== 'sel'; $('#br-sn', dlg.el).textContent = mode === 'sel' ? picked.length + ' block đã chọn' : '';
        var pg = $('#br-purge', dlg.el); pg.disabled = left > 0; if (left > 0) pg.checked = false; $('#br-ph', dlg.el).innerHTML = left > 0 ? 'Còn ' + left + ' tham chiếu ' + esc(from) + ' khác: chưa dọn được.' : '';
        var c = $('#br-pv', dlg.el); if (to) libThumb(c, to); else { var x = c.getContext('2d'); x.fillStyle = TC.cssv('--c-canvas'); x.fillRect(0, 0, c.width, c.height); }
        var li = to && libList().filter(function(z){ return z.name === to; })[0]; $('#br-d', dlg.el).innerHTML = to ? '<b style="color:var(--c-text)">' + esc(to) + '</b> · ' + esc((d.blocks[to] && d.blocks[to].desc) || (li && li.desc) || '') + (d.blocks[to] ? '' : ' · <span class="tag">thư viện</span>') : 'Chưa chọn block thay thế.';
        $('#br-sum', dlg.el).innerHTML = msg ? '<span class="' + (same ? 'danger' : '') + '">' + ic('warn', 'xs') + ' ' + msg + '</span>' : 'Thay ' + tg.length + ' block · ' + (keep ? 'giữ tỉ lệ, góc' : 'tỉ lệ 1, góc 0') + '. Một bước hoàn tác.';
        var ok = $('#br-ok', dlg.el); ok.disabled = !!msg || tab !== 'r'; ok.textContent = 'Thay ' + tg.length + ' block';
        d.found = tg.map(function(e){ return e.id; }); V().temp = to && !same ? [ED.temp(tg.map(function(e){ return ghostIns(d, to, e, keep); }), { color:TC.cssv('--c-accent'), w:1.5 })] : []; V().drawOver();
      }
      dlg.el.addEventListener('change', function(e){ if (e.target.name === 'brm') mode = e.target.value; if (e.target.id === 'br-from') from = e.target.value; render(); });
      dlg.el.addEventListener('segchange', function(e){ src = e.detail === 'Thư viện' ? 'lib' : 'dwg'; list(); });
      dlg.el.addEventListener('dtab', function(e){ tab = e.detail; render(); });
      $('#br-q', dlg.el).addEventListener('input', function(){ q = this.value; list(); });
      var bl = $('#br-l', dlg.el); bl.addEventListener('click', function(e){ var b = e.target.closest('[data-bt]'); if (!b) return; to = b.dataset.bt; $$('[data-bt]', bl).forEach(function(x){ x.setAttribute('aria-selected', String(x === b)); }); render(); });
      TC.listKeys(bl, '[data-bt]', function(b, enter){ if (enter) $('#br-ok', dlg.el).click(); else b.click(); });
      $('#br-pick', dlg.el).addEventListener('click', function(){ dlg.hide(true); TC.cad.clearSel(); ctx.sel('Chọn các block cần thay').then(function(ids){ picked = ids.map(function(id){ return d.byId[id]; }).filter(function(e){ return e && e.t === 'insert' && !e.xref; }); TC.cad.clearSel(); dlg.hide(false); render(); }, function(){ dlg.hide(false); }); });
      $('#br-ok', dlg.el).addEventListener('click', function(){
        var tg = targets(), keep = $('#br-keep', dlg.el).checked, map = $('#br-map', dlg.el).checked, purge = $('#br-purge', dlg.el).checked && !$('#br-purge', dlg.el).disabled, old = {}, n = tg.length; if (!n || !to) return;
        tg.forEach(function(e){ old[e.name] = 1; }); d.found = []; V().temp = []; dlg.close(true);
        ED.run('BLOCKREPLACE', function(){ copyDef(d, to); var defs = attDefs(d.blocks[to]), li = libList().filter(function(z){ return z.name === to; })[0], k = li && li.anno ? 100 : 1;
          tg.forEach(function(e){ ED.mod(e, function(x){ var at = {}; defs.forEach(function(a){ at[a.tag] = map && x.attrs && x.attrs[a.tag] != null ? x.attrs[a.tag] : a.s; }); x.name = to; x.attrs = at; if (!keep){ x.sx = x.sy = k; x.rot = 0; } }); });
          if (purge) Object.keys(old).forEach(function(nm){ if (!allIns(d, nm).length){ var dd = defState(d, [nm]); delete d.blocks[nm]; dd(); } }); });
        V().draw(); A().refreshPalettes(); ctx.out('Đã thay ' + n + ' block ' + Object.keys(old).map(esc).join(', ') + ' bằng <b>' + esc(to) + '</b>' + (purge ? '; đã dọn định nghĩa block cũ' : '') + '. Ctrl+Z để hoàn tác.'); res();
      });
      list(); render();
    });
  };

  /* ================= DesignCenter UI-G17 (ADCENTER, Ctrl+2) ================= */
  var DCAT = [['Block', 'insert'], ['Kiểu kích thước', 'dim'], ['Layer', 'layers'], ['Layout', 'layout'], ['Linetype', 'line'], ['Kiểu multileader', 'leader'], ['Kiểu bảng', 'table'], ['Kiểu chữ', 'mtext'], ['Xref', 'xref']];
  var DCFILES = { // tệp chưa mở: nội dung mô phỏng, block lấy từ thư viện
    'Cua.dwg':{ date:'02/10/2026', Block:['CUA-DI-1C', 'CUA-DI-2C', 'CUA-TRUOT', 'CUA-SO'], Layer:[['KT-CUA', 2], ['KT-CUASO', 4]] },
    'Noi-that.dwg':{ date:'28/09/2026', Block:['SOFA-L', 'BAN-TRA', 'BAN-AN-6', 'GIUONG-1600', 'GIUONG-1800', 'TU-AO', 'NGUOI', 'CAY-XANH', 'XE-O-TO'], Layer:[['KT-NOITHAT', 8], ['KT-CAYXANH', 3]] },
    'Ve-sinh.dwg':{ date:'25/09/2026', Block:['BON-CAU', 'LAVABO', 'VOI-SEN'], Layer:[['KT-TBVS', 140]] },
    'Dien-nuoc.dwg':{ date:'20/09/2026', Block:['O-CAM', 'CONG-TAC', 'DEN-TRAN', 'VAN-KHOA', 'PHEU-THU'], Layer:[['DN-DIEN', 1], ['DN-CAPNUOC', 5], ['DN-THOATNUOC', 30]] },
    'Ky-hieu-TCVN.dwg':{ date:'15/09/2026', Block:['TCVN-CAO-DO', 'TCVN-TRUC', 'TCVN-HUONG-BAC', 'TCVN-MAT-CAT', 'TCVN-CHI-TIET'], Layer:[['KT-KYHIEU', 2]], 'Kiểu chữ':['Standard', 'ROMANS'] },
    'Mat-dung.dwg':{ date:'28/09/2026', Block:['CUA-SO', 'NGUOI', 'CAY-XANH'], Layer:[['KT-MATDUNG', 7], ['KT-TRUC', 1]], Layout:['A3-Mat dung'] },
    'Chi-tiet-cau-thang.dwg':{ date:'25/09/2026', Block:['TCVN-CHI-TIET', 'TCVN-MAT-CAT'], Layer:[['KT-THANG', 9], ['KT-DIM', 3]], 'Kiểu kích thước':['Standard', 'TCVN-1-20'] }
  };
  var DCDIRS = [['D:\\DuAn\\NhaO-2T\\KienTruc', ['Banve-01.dwg', 'Mat-dung.dwg', 'Chi-tiet-cau-thang.dwg']], ['D:\\DuAn\\NhaO-2T\\KetCau', ['Ketcau-T2.dwg']], ['D:\\Tai-nguyen\\Block', ['Cua.dwg', 'Noi-that.dwg', 'Ve-sinh.dwg', 'Dien-nuoc.dwg', 'Ky-hieu-TCVN.dwg']]];
  var DC = { tab:'open', file:null, cat:'Block', item:null, view:'lg', tree:true, pv:true, desc:true, back:[], fav:TC.load('adcfav', []), exp:{ 'd:D:\\Tai-nguyen\\Block':1 }, fv:false, se:null };
  function dcDoc(file){ return A().docs.filter(function(x){ return x.open && x.name === file; })[0]; }
  function dcDir(file){ var r = DCDIRS.filter(function(x){ return x[1].indexOf(file) >= 0; })[0]; return r ? r[0] : ''; }
  function dcContent(file){ var d = dcDoc(file);
    if (d) return { doc:d, Block:userBlocks(d).sort(), 'Kiểu kích thước':d.dimstyles.slice(), Layer:d.layers.map(function(l){ return [l.name, l.color]; }), Layout:d.layouts.map(function(L){ return L.name; }), Linetype:['Continuous', 'CENTER', 'DASHED', 'HIDDEN'], 'Kiểu multileader':['Standard', 'TCVN-1-100'], 'Kiểu bảng':['Standard', 'TCVN-Thong-ke'], 'Kiểu chữ':d.styles.map(function(s){ return s.name; }), Xref:d.xrefs.filter(function(x){ return x.type !== 'PDF'; }).map(function(x){ return x.name; }) };
    return Object.assign({ Block:[], 'Kiểu kích thước':['Standard', 'TCVN-1-100'], Layer:[['0', 7]], Layout:[], Linetype:['Continuous', 'CENTER'], 'Kiểu multileader':['Standard'], 'Kiểu bảng':['Standard'], 'Kiểu chữ':['Standard'], Xref:[] }, DCFILES[file] || {}); }
  function dcName(x){ return Array.isArray(x) ? x[0] : x; }
  function dcGo(file, cat){ if (DC.file !== file || DC.cat !== cat) DC.back.push([DC.file, DC.cat]); DC.file = file; DC.cat = cat; DC.item = null; DC.fv = false; DC.se = null; if (file) DC.exp['f:' + file] = 1; }
  function dcItemHTML(it, c){ var n = dcName(it.name), lg = DC.view === 'lg', key = esc(JSON.stringify({ file:it.file, cat:it.cat, name:n })), on = DC.item && DC.item.name === n && DC.item.cat === it.cat && DC.item.file === it.file;
    var vis = it.cat === 'Block' ? '<canvas width="' + (lg ? 76 : 30) + '" height="' + (lg ? 52 : 22) + '"></canvas>' : it.cat === 'Layer' ? '<i class="sw" style="background:' + (it.name[1] === 7 ? '#FFFFFF' : TC.ACI[it.name[1]] || '#FFFFFF') + '"></i>' : ic((DCAT.filter(function(x){ return x[0] === it.cat; })[0] || [0, 'file'])[1], lg ? 'md' : '');
    return '<button class="dci' + (lg ? '' : ' ls') + '" type="button" draggable="true" data-di="' + key + '" aria-selected="' + !!on + '" data-tip="' + esc(n) + '">' + vis + '<span class="nm">' + esc(n) + '</span>' + (c ? '<span class="faint">' + esc(c) + '</span>' : '') + '</button>'; }
  P.adc = { name:'DesignCenter', render:function(host, d){
    if (!d){ host.innerHTML = ''; return; }
    if (TC.isViewer()){ host.innerHTML = '<div class="empty">' + ic('dcenter') + '<b>DesignCenter</b>Lấy block, layer, kiểu từ bản vẽ khác có trong TrionCAD Pro.<button class="btn primary sm" type="button" data-up style="margin-top:8px">Dùng thử Pro 30 ngày</button></div>'; host.onclick = function(e){ if (e.target.closest('[data-up]')) SC.upgrade('DesignCenter', 'PRO'); }; return; }
    if (!DC.file) DC.file = d.name;
    var opens = A().docs.filter(function(x){ return x.open; }), tb = function(k, icn, tip, on, dis){ return '<button class="ib" type="button" data-dt2="' + k + '" data-tip="' + tip + '" aria-label="' + tip + '"' + (on != null ? ' aria-pressed="' + on + '"' : '') + (dis ? ' disabled' : '') + '>' + ic(icn) + '</button>'; };
    var tn = function(key, lbl, icn, depth, sel, cnt, exp){ return '<button class="tn" type="button" data-tk="' + esc(key) + '" aria-selected="' + !!sel + '" style="padding-left:' + (4 + depth * 13) + 'px">' + (exp != null ? '<span class="tx">' + ic(exp ? 'down' : 'right', 'xs') + '</span>' : '<span class="tx"></span>') + ic(icn, 's12') + '<span class="tl">' + esc(lbl) + '</span>' + (cnt != null ? '<span class="cnt">' + cnt + '</span>' : '') + '</button>'; };
    var fileNodes = function(file, depth){ var on = DC.exp['f:' + file], s = tn('f:' + file, file, 'file', depth, !DC.fv && !DC.se && DC.file === file && !DC.cat, null, !!on); if (on){ var c = dcContent(file); DCAT.forEach(function(k){ s += tn('c:' + file + '|' + k[0], k[0], k[1], depth + 1, !DC.fv && !DC.se && DC.file === file && DC.cat === k[0], (c[k[0]] || []).length); }); } return s; };
    var tree = DC.tab === 'open' ? opens.map(function(x){ return fileNodes(x.name, 0); }).join('') : DC.tab === 'dir' ? DCDIRS.map(function(r){ var on = DC.exp['d:' + r[0]]; return tn('d:' + r[0], r[0], 'folder', 0, false, null, !!on) + (on ? r[1].map(function(f){ return fileNodes(f, 1); }).join('') : ''); }).join('') : TC.RECENT.map(function(r){ return '<button class="tn dch" type="button" data-hk="' + esc(r.name) + '"><span class="tx"></span>' + ic('clock', 's12') + '<span class="tl">' + esc(r.name) + '</span><span class="cnt">' + esc(r.date || '') + '</span></button>'; }).join('');
    var c = DC.file ? dcContent(DC.file) : null, body = '', items = [], path = DC.file ? (dcDoc(DC.file) ? dcDoc(DC.file).path : dcDir(DC.file) + '\\' + DC.file) : '';
    if (DC.se){ var S = DC.se; body = '<div class="dcs"><div class="frow" style="--lab-w:62px"><span class="lab">Tìm</span>' + H.sel('dcs-c', DCAT.map(function(x){ return x[0]; }), S.cat, 0) + '<span class="lab">Trong</span>' + H.sel('dcs-w', ['Mọi thư mục', 'Bản vẽ đang mở'].concat(DCDIRS.map(function(r){ return r[0]; })), S.where, 0) + '<span class="lab">Tên chứa</span><input class="inp" id="dcs-q" value="' + esc(S.q) + '" placeholder="ví dụ: CUA" spellcheck="false"><span class="lab">Sửa đổi</span>' + H.sel('dcs-t', ['Bất kỳ lúc nào', 'Trong 7 ngày', 'Trong 30 ngày'], S.when, 0) + '</div><div class="hrow"><button class="btn sm primary" type="button" id="dcs-go">' + ic('search') + 'Tìm</button><button class="btn sm ghost" type="button" data-dt2="seoff">Đóng tìm kiếm</button><span class="faint" style="font-size:var(--fs-xs)">' + (S.res ? S.res.length + ' kết quả' : '') + '</span></div></div>';
      items = S.res || []; body += S.res ? (items.length ? '<div class="dcg ls">' + items.map(function(it){ return dcItemHTML(it, it.file); }).join('') + '</div>' : '<div class="empty">' + ic('search') + '<b>Không tìm thấy “' + esc(S.q) + '”</b>Thử từ khóa ngắn hơn hoặc tìm ở Mọi thư mục.</div>') : ''; }
    else if (DC.fv){ items = DC.fav.slice(); body = items.length ? '<div class="dcg ls">' + items.map(function(it){ return dcItemHTML(it, it.file); }).join('') + '</div>' : '<div class="empty">' + ic('star') + '<b>Chưa có mục yêu thích</b>Chuột phải lên block, layer hoặc kiểu › Thêm vào Yêu thích.</div>'; }
    else if (c && !DC.cat){ body = '<div class="dcg">' + DCAT.map(function(k){ return '<button class="dci" type="button" data-cat="' + esc(k[0]) + '">' + ic(k[1], 'md') + '<span class="nm">' + esc(k[0]) + '</span><span class="faint">' + (c[k[0]] || []).length + '</span></button>'; }).join('') + '</div>'; }
    else if (c){ items = (c[DC.cat] || []).map(function(n){ return { file:DC.file, cat:DC.cat, name:n }; }); body = items.length ? '<div class="dcg' + (DC.view === 'lg' ? '' : ' ls') + '">' + items.map(function(it){ return dcItemHTML(it); }).join('') + '</div>' : '<div class="empty">' + ic((DCAT.filter(function(x){ return x[0] === DC.cat; })[0] || [0, 'file'])[1]) + '<b>' + esc(DC.file) + ' không có ' + esc(DC.cat.toLowerCase()) + '</b></div>'; }
    var it = DC.item, bd = it && it.cat === 'Block' ? ((dcDoc(it.file) || {}).blocks || {})[it.name] : null, li = it && it.cat === 'Block' && libList().filter(function(z){ return z.name === it.name; })[0], desc = !it ? (DC.cat ? 'Chọn một mục để xem mô tả. Kéo thả vào vùng vẽ để thêm vào bản vẽ hiện hành; nhấp đúp để chèn.' : 'Chọn loại nội dung.') : it.cat === 'Block' ? (bd && bd.desc) || (li && li.label + ' · ' + li.desc) || 'Block ' + it.name : it.cat === 'Layer' ? 'Layer ' + dcName(it.name) + ' · màu ' + it.name[1] + (D().layerMap[dcName(it.name)] ? ' · đã có trong bản vẽ hiện hành' : ' · chưa có trong bản vẽ hiện hành') : it.cat + ' “' + dcName(it.name) + '”' + (it.file ? ' trong ' + it.file : '');
    host.innerHTML = '<div class="dc"><div class="pal-tools">' + tb('load', 'open', 'Tải tệp…') + tb('back', 'left', 'Lùi', null, !DC.back.length) + tb('up', 'up', 'Lên trên', null, !DC.cat && !DC.fv && !DC.se) + tb('find', 'search', 'Tìm kiếm', !!DC.se) + tb('fav', 'star', 'Yêu thích', DC.fv) + tb('home', 'home', 'Trang chủ: D:\\Tai-nguyen\\Block') + '<span class="sep"></span>' + tb('tree', 'listview', 'Cây thư mục', DC.tree) + tb('pv', 'preview', 'Xem trước', DC.pv) + tb('desc', 'info', 'Mô tả', DC.desc) + tb('view', 'gridview', 'Kiểu xem: ' + (DC.view === 'lg' ? 'biểu tượng lớn' : 'danh sách')) + '</div>' +
      '<div class="tabs ptabs dct" role="tablist">' + [['dir', 'Thư mục'], ['open', 'Bản vẽ đang mở'], ['hist', 'Lịch sử']].map(function(t){ return '<button class="tab" role="tab" type="button" data-dtab="' + t[0] + '" aria-selected="' + (DC.tab === t[0]) + '">' + t[1] + '</button>'; }).join('') + '</div>' +
      (DC.tree ? '<div class="dctree tree">' + tree + '</div>' : '') + '<div class="dcpath mono" title="' + esc(path) + '">' + (DC.se ? 'Tìm kiếm' : DC.fv ? 'Yêu thích' : esc(path + (DC.cat ? ' › ' + DC.cat : ''))) + '</div>' +
      '<div class="dcc" id="dc-c">' + body + '</div>' + (DC.pv ? '<div class="dcpv"><canvas width="236" height="96"></canvas></div>' : '') + (DC.desc ? '<div class="pal-desc dcdesc">' + esc(desc) + '</div>' : '') + '</div>';
    $$('[data-di] canvas', host).forEach(function(cv){ var o = JSON.parse(cv.parentNode.dataset.di); libThumb(cv, o.name, dcDoc(o.file)); });
    var pv = $('.dcpv canvas', host); if (pv){ if (it && it.cat === 'Block') libThumb(pv, it.name, dcDoc(it.file)); else { var x = pv.getContext('2d'); x.fillStyle = TC.cssv('--c-canvas'); x.fillRect(0, 0, pv.width, pv.height); x.fillStyle = '#8C949E'; x.font = '11px ' + TC.cssv('--f-ui'); x.textAlign = 'center'; x.fillText(it ? 'Không có xem trước cho ' + it.cat.toLowerCase() : 'Xem trước', pv.width / 2, pv.height / 2 + 4); } }
    var rr = function(){ P.adc.render(host, D()); };
    host.onclick = function(e){
      var t = e.target.closest('[data-dtab]'); if (t){ DC.tab = t.dataset.dtab; rr(); return; }
      var k = e.target.closest('[data-tk]'); if (k){ var key = k.dataset.tk, onTx = e.target.closest('.tx'); if (key[0] === 'd'){ DC.exp[key] = !DC.exp[key]; rr(); return; } if (key[0] === 'f'){ var f = key.slice(2); if (onTx && DC.file === f) DC.exp[key] = !DC.exp[key]; else dcGo(f, null); rr(); return; } var p = key.slice(2).split('|'); dcGo(p[0], p[1]); rr(); return; }
      var h = e.target.closest('[data-hk]'); if (h){ var nm = h.dataset.hk; if (dcDoc(nm) || DCFILES[nm] || dcDir(nm)){ dcGo(nm, null); DC.tab = dcDoc(nm) ? 'open' : 'dir'; if (dcDir(nm)) DC.exp['d:' + dcDir(nm)] = 1; rr(); } else TC.toast('<b>' + esc(nm) + '</b> nằm ở thư mục mạng hoặc ổ đĩa đang không kết nối. Kết nối lại rồi mở từ Lịch sử.', { type:'warn', log:false }); return; }
      var cb = e.target.closest('[data-cat]'); if (cb){ dcGo(DC.file, cb.dataset.cat); rr(); return; }
      var di = e.target.closest('[data-di]'); if (di){ DC.item = JSON.parse(di.dataset.di); rr(); return; }
      var b = e.target.closest('[data-dt2]'); if (!b) return; var a = b.dataset.dt2, rb = b.getBoundingClientRect();
      if (a === 'load') TC.toast('Mở hộp thoại chọn tệp DWG, DWT, DXF để xem nội dung trong DesignCenter.', { type:'info', log:false });
      else if (a === 'back'){ var pv2 = DC.back.pop(); if (pv2){ DC.file = pv2[0]; DC.cat = pv2[1]; DC.item = null; DC.fv = false; DC.se = null; } }
      else if (a === 'up'){ if (DC.se || DC.fv){ DC.se = null; DC.fv = false; } else if (DC.cat) dcGo(DC.file, null); }
      else if (a === 'find') DC.se = DC.se ? null : { cat:'Block', where:'Mọi thư mục', q:'', when:'Bất kỳ lúc nào', res:null };
      else if (a === 'seoff') DC.se = null;
      else if (a === 'fav'){ DC.fv = !DC.fv; DC.se = null; }
      else if (a === 'home'){ DC.tab = 'dir'; DC.exp['d:D:\\Tai-nguyen\\Block'] = 1; dcGo('Noi-that.dwg', 'Block'); }
      else if (a === 'tree' || a === 'pv' || a === 'desc') DC[a] = !DC[a];
      else if (a === 'view'){ TC.menu([{ label:'Biểu tượng lớn', check:DC.view === 'lg', act:function(){ DC.view = 'lg'; rr(); } }, { label:'Danh sách', check:DC.view === 'ls', act:function(){ DC.view = 'ls'; rr(); } }], rb.left, rb.bottom + 2); return; }
      rr();
    };
    var go = $('#dcs-go', host); if (go){ var run = function(){ var S = DC.se; S.cat = $('#dcs-c', host).value; S.where = $('#dcs-w', host).value; S.q = $('#dcs-q', host).value.trim(); S.when = $('#dcs-t', host).value; var F = TC.fold(S.q), days = S.when === 'Trong 7 ngày' ? 7 : S.when === 'Trong 30 ngày' ? 30 : 1e9, out = [];
        var files = S.where === 'Bản vẽ đang mở' ? A().docs.filter(function(x){ return x.open; }).map(function(x){ return x.name; }) : S.where === 'Mọi thư mục' ? DCDIRS.reduce(function(a, r){ return a.concat(r[1]); }, []) : (DCDIRS.filter(function(r){ return r[0] === S.where; })[0] || [0, []])[1];
        files.forEach(function(f){ var dd = dcDoc(f), dt = dd ? (dd.modified || '').split(' ')[0] : (DCFILES[f] || {}).date || '', pp = dt.split('/'), age = pp.length === 3 ? (new Date(2026, 9, 19) - new Date(Number(pp[2]), Number(pp[1]) - 1, Number(pp[0]))) / 864e5 : 0; if (age > days) return; (dcContent(f)[S.cat] || []).forEach(function(n){ if (!F || TC.fold(dcName(n)).indexOf(F) >= 0) out.push({ file:f, cat:S.cat, name:n }); }); });
        S.res = out; rr(); setTimeout(function(){ var q2 = $('#dcs-q', host); if (q2) q2.focus(); }, 0); }; go.addEventListener('click', run); $('#dcs-q', host).addEventListener('keydown', function(e){ if (e.key === 'Enter'){ e.preventDefault(); run(); } }); }
    host.ondblclick = function(e){ var di = e.target.closest('[data-di]'); if (di) dcAdd(JSON.parse(di.dataset.di)); };
    host.oncontextmenu = function(e){ var di = e.target.closest('[data-di]'); if (!di) return; e.preventDefault(); var o = JSON.parse(di.dataset.di), fi = DC.fav.findIndex(function(x){ return x.file === o.file && x.cat === o.cat && dcName(x.name) === dcName(o.name); });
      TC.menu([{ label:o.cat === 'Block' ? 'Chèn block…' : o.cat === 'Xref' ? 'Gắn xref…' : 'Thêm vào bản vẽ', icon:o.cat === 'Block' ? 'insert' : 'plus', act:function(){ dcAdd(o); } }, { label:fi >= 0 ? 'Bỏ khỏi Yêu thích' : 'Thêm vào Yêu thích', icon:'star', act:function(){ if (fi >= 0) DC.fav.splice(fi, 1); else DC.fav.push(o); TC.save('adcfav', DC.fav); rr(); } }, { label:'Tạo công cụ trên Tool Palette', icon:'tpal', dis:o.cat !== 'Block', act:function(){ tpAdd({ t:'block', name:'Block ' + dcName(o.name), block:dcName(o.name) }); A().showPalette('tools'); } }, { sep:1 }, { label:'Mở ' + esc(o.file), icon:'open', dis:!o.file || !!dcDoc(o.file) || !/^(Banve-01|Ketcau-T2)\.dwg$/.test(o.file), act:function(){ A().openDoc(o.file === 'Ketcau-T2.dwg' ? 'd2' : 'd1'); } }], e.clientX, e.clientY); };
    host.ondragstart = function(e){ var di = e.target.closest('[data-di]'); if (!di) return; e.dataTransfer.setData('text/x-tc-adc', di.dataset.di); e.dataTransfer.effectAllowed = 'copy'; };
    TC.listKeys(host, '[data-di]', function(b, enter){ if (enter) dcAdd(JSON.parse(b.dataset.di)); else { DC.item = JSON.parse(b.dataset.di); rr(); var nb = $('[aria-selected="true"][data-di]', host); if (nb) nb.focus(); } });
  } };
  // thêm nội dung DesignCenter vào bản vẽ hiện hành; p: điểm thả trên vùng vẽ (block)
  function dcAdd(o, p){
    var d = D(), src = dcDoc(o.file), n = dcName(o.name), c = o.cat; if (!d) return; if (TC.isViewer()){ SC.upgrade('DesignCenter', 'PRO'); return; }
    if (c === 'Block'){
      if (d.blocks[n] && !p){ TC.cmd.run('INSERT', n); return; } var li = libList().filter(function(z){ return z.name === n; })[0], k = li && li.anno ? 100 : 1, had = !!d.blocks[n];
      ED.run('ADCENTER', function(){ copyDef(d, n, src && src !== d ? src : null); });
      if (!d.blocks[n]){ TC.toast('Không đọc được block <b>' + esc(n) + '</b> từ ' + esc(o.file) + '.', { type:'err' }); return; }
      if (!p){ TC.cmdline.msg((had ? '' : 'Đã chép định nghĩa <b>' + esc(n) + '</b> từ ' + esc(o.file) + '. ') + 'Chỉ điểm chèn.'); TC.cmd.run('INSERT', n); return; }
      SC.askAttrs(null, d.blocks[n]).then(function(at){ ED.run('INSERT', function(){ ED.add(ED.make({ t:'insert', name:n, p:p, sx:k, sy:k, rot:0, attrs:at })); }); V().draw(); TC.cmdline.msg('Đã chèn <b>' + esc(n) + '</b> từ ' + esc(o.file) + ' tại vị trí thả' + (had ? ' (dùng định nghĩa có sẵn trong bản vẽ).' : '.')); });
      return;
    }
    var exists = c === 'Layer' ? !!d.layerMap[n] : c === 'Kiểu chữ' ? !!d.styleMap[n] : c === 'Kiểu kích thước' ? d.dimstyles.indexOf(n) >= 0 : c === 'Layout' ? d.layouts.some(function(L){ return L.name === n; }) : null;
    if (c === 'Xref'){ TC.cmd.run('XATTACH'); return; }
    if (exists){ TC.toast(c + ' <b>' + esc(n) + '</b> đã có trong bản vẽ: giữ thiết lập hiện có, không ghi đè.', { type:'info', log:false }); return; }
    if (exists === null){ TC.toast('Đã nạp ' + c.toLowerCase() + ' <b>' + esc(n) + '</b> vào bản vẽ.', { log:false }); return; }
    ED.run('ADCENTER', function(){
      if (c === 'Layer'){ var done = layerState(d), sl = src && src.layerMap[n]; addLayer(d, n, sl ? { color:sl.color, lt:sl.lt, lw:sl.lw, desc:sl.desc } : { color:Array.isArray(o.name) ? o.name[1] : 7 }); done(); }
      else if (c === 'Kiểu chữ'){ var ds = ED.state(function(){ return d.styles; }, function(s){ d.styles = s; d.styleMap = {}; s.forEach(function(x){ d.styleMap[x.name] = x; }); A().refreshPalettes(); }), st = src && src.styleMap[n]; var ns = Object.assign({ font:'Arial', kind:'TTF', enc:'Unicode', h:0 }, st ? JSON.parse(JSON.stringify(st)) : { name:n, font:/ROMANS/i.test(n) ? 'romans.shx' : 'Arial', kind:/ROMANS/i.test(n) ? 'SHX' : 'TTF' }, { name:n }); d.styles.push(ns); d.styleMap[n] = ns; ds(); }
      else if (c === 'Kiểu kích thước'){ var dd = ED.state(function(){ return d.dimstyles; }, function(s){ d.dimstyles = s; A().renderRibbon(); }); d.dimstyles.push(n); dd(); }
      else if (c === 'Layout'){ var SL = src && src.layouts.filter(function(L){ return L.name === n; })[0], L = { name:n, paper:SL ? JSON.parse(JSON.stringify(SL.paper)) : { w:420, h:297, name:'ISO A3 (420,00 × 297,00 mm)' }, margin:[5, 5, 5, 5], ents:[], vports:[], setup:SL ? SL.setup : '' }, dl = ED.state(function(){ return { n:d.layouts.length }; }, function(s){ if (s.n < d.layouts.length) d.layouts.splice(s.n); else if (d.layouts.indexOf(L) < 0) d.layouts.push(L); A().renderLayoutTabs(); });
        d.layouts.push(L); dl(); if (SL){ SL.ents.forEach(function(e){ if (e.t !== 'vpframe'){ if (e.t === 'insert') copyDef(d, e.name, src); ED.add(ED.clean(e), L.ents); } }); SL.vports.forEach(function(v){ var cp = JSON.parse(JSON.stringify(v)); cp.id = 'vp' + (++d.seq); L.vports.push(cp); ED.add({ t:'vpframe', vp:cp.id, layer:d.layerMap['KT-VIEWPORT'] ? 'KT-VIEWPORT' : '0' }, L.ents); }); } A().renderLayoutTabs(); }
    });
    A().refreshPalettes(); TC.cmdline.msg('Đã thêm ' + c.toLowerCase() + ' <b>' + esc(n) + '</b> từ ' + esc(o.file) + ' vào bản vẽ hiện hành.');
  }
  document.addEventListener('dragover', function(e){ if (e.target.closest && e.target.closest('#cv') && Array.prototype.indexOf.call(e.dataTransfer.types, 'text/x-tc-adc') >= 0){ e.preventDefault(); e.dataTransfer.dropEffect = 'copy'; } });
  document.addEventListener('drop', function(e){ if (!e.target.closest || !e.target.closest('#cv')) return; var raw = e.dataTransfer.getData('text/x-tc-adc'); if (!raw) return; e.preventDefault(); e.stopPropagation(); var r = $('#cv').getBoundingClientRect(), v = V(), p = v.sToM(e.clientX - r.left, e.clientY - r.top) || v.toW(e.clientX - r.left, e.clientY - r.top); dcAdd(JSON.parse(raw), p); }, true);
  IMPL.ADCENTER = function(ctx, pre){ A().showPalette('adc', pre === 'toggle' || undefined); };

  /* ================= Tool Palettes UI-G18 (TOOLPALETTES, Ctrl+3) ================= */
  var TPDEF = { cur:'kt', group:'Tất cả', groups:[['Tất cả', null], ['Kiến trúc', ['kt', 'hatch', 'lenh']], ['Điện nước', ['dien', 'lenh', 'lisp']]], pals:[
    { id:'kt', name:'Kiến trúc', tools:[ { t:'block', name:'Cửa đi một cánh 900', block:'CUA-DI-1C', sc:900, layer:'KT-CUA', askRot:true }, { t:'block', name:'Cửa đi hai cánh 1400', block:'CUA-DI-2C', sc:1400, layer:'KT-CUA', askRot:true }, { t:'block', name:'Giường đôi 1600', block:'GIUONG-1600', layer:'KT-NOITHAT' }, { t:'block', name:'Sofa góc chữ L', block:'SOFA-L', layer:'KT-NOITHAT' }, { t:'block', name:'Bồn cầu', block:'BON-CAU', layer:'KT-TBVS', askRot:true }, { t:'block', name:'Lavabo', block:'LAVABO', layer:'KT-TBVS' }, { t:'block', name:'Cây xanh Ø2400', block:'CAY-XANH', layer:'KT-NOITHAT' } ] },
    { id:'dien', name:'Điện nước', tools:[ { t:'block', name:'Ổ cắm đôi', block:'O-CAM', layer:'KT-KYHIEU' }, { t:'block', name:'Công tắc đơn', block:'CONG-TAC', layer:'KT-KYHIEU' }, { t:'block', name:'Đèn trần', block:'DEN-TRAN', layer:'KT-KYHIEU' }, { t:'block', name:'Van khóa', block:'VAN-KHOA', layer:'KT-KYHIEU' }, { t:'block', name:'Phễu thu sàn', block:'PHEU-THU', layer:'KT-KYHIEU' } ] },
    { id:'hatch', name:'Hatch', tools:[ { t:'hatch', name:'Bê tông', pat:'TCVN-BETONG', sc:40 }, { t:'hatch', name:'Bê tông cốt thép', pat:'TCVN-BTCT', sc:40 }, { t:'hatch', name:'Gạch xây', pat:'TCVN-GACH', sc:40, layer:'KT-HATCH-TUONG' }, { t:'hatch', name:'Đất tự nhiên', pat:'TCVN-DAT', sc:40 }, { t:'hatch', name:'Gỗ', pat:'TCVN-GO', sc:30 }, { t:'hatch', name:'Tô đặc', pat:'SOLID' } ] },
    { id:'lenh', name:'Lệnh', tools:[ { t:'cmd', name:'Kích thước thông minh', cmd:'DIM', layer:'KT-DIM' }, { t:'cmd', name:'Đám mây sửa đổi', cmd:'REVCLOUD' }, { t:'cmd', name:'Multileader', cmd:'MLEADER', layer:'KT-GHICHU' }, { t:'cmd', name:'Đo nhanh', cmd:'MEASUREGEOM', pre:'Q' }, { t:'cmd', name:'Tổng diện tích', cmd:'TCAREA' } ] },
    { id:'lisp', name:'LISP', tools:[ { t:'lisp', name:'Vẽ tường trên KT-TUONG', code:'(command "_.-LAYER" "_S" "KT-TUONG" "")\n(command "_.PLINE")' }, { t:'lisp', name:'Ký hiệu cao độ', code:'(command "_.INSERT" "TCVN-CAO-DO")' }, { t:'lisp', name:'Đánh số trục tự động', code:'(c:DANHTRUC)' } ] }
  ] };
  var TP = TC.load('toolpal1', null) || JSON.parse(JSON.stringify(TPDEF)), PEND = null;
  function tpSave(){ TC.save('toolpal1', TP); }
  function tpPals(){ var g = TP.groups.filter(function(x){ return x[0] === TP.group; })[0], ids = g && g[1]; return TP.pals.filter(function(p){ return !ids || ids.indexOf(p.id) >= 0; }); }
  function tpCur(){ var l = tpPals(); return l.filter(function(p){ return p.id === TP.cur; })[0] || l[0] || null; }
  function tpAdd(tool, pal){ pal = pal || tpCur() || TP.pals[0]; pal.tools.push(tool); TP.cur = pal.id; tpSave(); A().refreshPalettes(); TC.toast('Đã tạo công cụ “' + esc(tool.name) + '” trên palette ' + esc(pal.name) + '.', { log:false }); }
  TC.on('cmdend', function(n){ if (PEND && n === PEND.cmd){ var f = PEND.fn; PEND = null; f(); } });
  // dùng công cụ: chạy lệnh thật với thuộc tính của công cụ (layer, màu ghi đè; tỉ lệ, góc của block; mẫu hatch)
  function tpRun(tool){
    var d = D(); if (!d) return; if (TC.isViewer()){ SC.upgrade('Tool Palettes', 'PRO'); return; }
    var go = function(){ var lay = tool.layer && d.layerMap[tool.layer] ? tool.layer : null, ov = {}; if (lay) ov.layer = lay; if (tool.color != null) ov.color = tool.color;
      if (tool.layer && !lay) TC.cmdline.msg('Layer ' + esc(tool.layer) + ' chưa có trong bản vẽ: dùng layer hiện hành.', 'faint');
      if (tool.t === 'block'){ var BO = SC.blockOpts, keep = { scale:BO.scale, rot:BO.rot, rotScreen:BO.rotScreen, explode:BO.explode, repeat:BO.repeat }; Object.assign(BO, { scale:tool.sc || 1, rot:tool.rot || 0, rotScreen:!!tool.askRot, explode:!!tool.explode, repeat:false }); PEND = { cmd:'INSERT', fn:function(){ Object.assign(BO, keep); } }; ED.override = ov; TC.cmd.run('INSERT', tool.block); }
      else if (tool.t === 'hatch'){ var HS = ED.hset, k2 = { pat:HS.pat, sc:HS.sc, ang:HS.ang, layer:HS.layer, type:HS.type }; Object.assign(HS, { pat:tool.pat, sc:tool.sc || HS.sc, ang:tool.ang || 0, type:tool.pat === 'SOLID' ? 'Tô đặc' : 'Mẫu', layer:lay || 'Dùng hiện hành' }); PEND = { cmd:'HATCH', fn:function(){ Object.assign(HS, k2); } }; ED.override = ov; TC.cmd.run('HATCH'); }
      else if (tool.t === 'cmd'){ if (!TC.cmd.def(tool.cmd)){ TC.toast('Không có lệnh “' + esc(tool.cmd) + '”. Sửa công cụ trong Thuộc tính công cụ.', { type:'warn', log:false }); return; } TC.cmd.run(tool.cmd, tool.pre); ED.override = ov; }
      else lispRun(tool);
    };
    if (TC.cmdline.busy()){ TC.cmd.cancel(); setTimeout(go, 40); } else go();
  }
  // macro LISP của công cụ: hiểu (command "…" …) và (c:TEN); hàm chưa nạp thì báo cách nạp
  function lispRun(tool){ var code = tool.code || '', d = D(), lay = code.match(/"_?\.?-LAYER"\s+"_?S(?:et)?"\s+"([^"]+)"/i), cm = null, fn = code.match(/\(c:([\w-]+)\)/i);
    TC.cmdline.msg('<span class="mono">' + esc(code.replace(/\n/g, ' ')) + '</span>', 'muted');
    if (lay){ if (d.layerMap[lay[1]]) ED.setCurLayer(lay[1]); else TC.cmdline.msg('Layer ' + esc(lay[1]) + ' chưa có trong bản vẽ.'); }
    code.replace(/\(command\s+"_?\.?([A-Za-z]+)"(?:\s+"([^"]*)")?/g, function(m, c, a){ if (!cm && !/LAYER/i.test(c)) cm = [c.toUpperCase(), a]; return m; });
    if (cm && TC.cmd.def(cm[0])) TC.cmd.run(cm[0], cm[1] || null); else if (fn) TC.toast('Hàm <b class="mono">c:' + esc(fn[1]) + '</b> chưa được nạp. Nạp tệp .lsp chứa hàm này bằng APPLOAD rồi dùng lại công cụ.', { type:'warn', log:false, act:{ label:'Nạp ứng dụng…', fn:function(){ TC.cmd.run('APPLOAD'); } } }); else if (!lay) TC.cmdline.msg('Macro không có lệnh nào để chạy.'); }
  function tpThumb(c, tool){ if (tool.t === 'block') return libThumb(c, tool.block); if (tool.t === 'hatch') return SC.patThumb(c, tool.pat); }
  P.tools = { name:'Tool Palettes', render:function(host, d){
    if (!d){ host.innerHTML = ''; return; }
    if (TC.isViewer()){ host.innerHTML = '<div class="empty">' + ic('tpal') + '<b>Tool Palettes</b>Công cụ chèn block, hatch, chạy lệnh và macro LISP có trong TrionCAD Pro.<button class="btn primary sm" type="button" data-up style="margin-top:8px">Dùng thử Pro 30 ngày</button></div>'; host.onclick = function(e){ if (e.target.closest('[data-up]')) SC.upgrade('Tool Palettes', 'PRO'); }; return; }
    var pals = tpPals(), pal = tpCur(), TYPE = { block:'Block', hatch:'Hatch', cmd:'Lệnh', lisp:'LISP' }, ICN = { cmd:'play', lisp:'pkg' };
    if (pal) TP.cur = pal.id;
    host.innerHTML = '<div class="tp"><div class="pal-tools"><button class="btn sm" type="button" data-ta="group" aria-haspopup="menu">' + ic('layers') + esc(TP.group) + ic('down', 'xs') + '</button><span class="grow"></span><button class="ib" type="button" data-ta="add" data-tip="Tạo công cụ từ đối tượng đang chọn" aria-label="Tạo công cụ từ đối tượng đang chọn">' + ic('plus') + '</button><button class="ib" type="button" data-ta="menu" aria-label="Tùy chọn Tool Palettes">' + ic('more') + '</button></div>' +
      '<div class="tpm"><div class="tpl" role="listbox" tabindex="0" aria-label="Công cụ">' + (pal && pal.tools.length ? pal.tools.map(function(t, i){ return '<button class="tpi" type="button" role="option" data-ti="' + i + '" aria-selected="false" data-tip="' + esc(t.name) + '" data-desc="' + esc(TYPE[t.t] + (t.t === 'block' ? ' ' + t.block + (t.layer ? ' · layer ' + t.layer : '') : t.t === 'hatch' ? ' ' + t.pat : t.t === 'cmd' ? ' ' + t.cmd : ' · ' + (t.code || '').split('\n')[0])) + '">' + (t.t === 'block' || t.t === 'hatch' ? '<canvas width="40" height="30"></canvas>' : '<span class="tpic">' + ic(t.t === 'cmd' && TC.cmd.def(t.cmd) ? TC.cmd.def(t.cmd).icon : ICN[t.t]) + '</span>') + '<span class="nm">' + esc(t.name) + '</span><span class="tag">' + TYPE[t.t] + '</span></button>'; }).join('')
        : '<div class="empty">' + ic('tpal') + '<b>' + (pal ? 'Palette ' + esc(pal.name) + ' chưa có công cụ' : 'Nhóm chưa có palette') + '</b>Kéo đối tượng từ bản vẽ vào đây, hoặc chuột phải lên block trong DesignCenter.</div>') + '</div>' +
      '<div class="tpt" role="tablist" aria-orientation="vertical">' + pals.map(function(p){ return '<button class="tpv" role="tab" type="button" data-tp="' + p.id + '" aria-selected="' + (pal && p.id === pal.id) + '">' + esc(p.name) + '</button>'; }).join('') + '</div></div><div class="pal-foot">' + ic('drag', 's12') + 'Kéo đối tượng đã chọn từ bản vẽ vào đây để tạo công cụ.</div></div>';
    $$('[data-ti] canvas', host).forEach(function(c){ tpThumb(c, pal.tools[Number(c.parentNode.dataset.ti)]); });
    var rr = function(){ P.tools.render(host, D()); };
    host.onclick = function(e){
      var t = e.target.closest('[data-tp]'); if (t){ TP.cur = t.dataset.tp; tpSave(); rr(); return; }
      var ti = e.target.closest('[data-ti]'); if (ti){ tpRun(pal.tools[Number(ti.dataset.ti)]); return; }
      var a = e.target.closest('[data-ta]'); if (!a) return; var r = a.getBoundingClientRect();
      if (a.dataset.ta === 'group') TC.menu([{ h:'Nhóm palette' }].concat(TP.groups.map(function(g){ return { label:esc(g[0]), check:g[0] === TP.group, act:function(){ TP.group = g[0]; tpSave(); rr(); } }; })).concat([{ sep:1 }, { label:'Nhóm mới…', icon:'plus', act:function(){ SC.ask('Nhóm palette mới', 'Tên nhóm', 'Nhóm ' + TP.groups.length, function(v){ return TP.groups.some(function(g){ return g[0] === v; }) ? 'Đã có nhóm “' + v + '”.' : null; }, 'UI-G18').then(function(v){ if (v){ TP.groups.push([v, pal ? [pal.id] : []]); TP.group = v; tpSave(); rr(); } }); } }]), r.left, r.bottom + 2, { returnFocus:a });
      else if (a.dataset.ta === 'add') tpFromSel();
      else TC.menu([{ label:'Palette mới…', icon:'plus', act:function(){ tpNewPal(); } }, { label:'Khôi phục palette mặc định', icon:'refresh', act:function(){ TC.confirm({ title:'Khôi phục Tool Palettes', msg:'Thay ' + TP.pals.length + ' palette hiện có bằng bộ mặc định?', detail:'Công cụ bạn tự tạo sẽ mất.', ok:'Khôi phục mặc định', danger:true }).then(function(y){ if (y){ TP = JSON.parse(JSON.stringify(TPDEF)); tpSave(); rr(); } }); } }, { sep:1 }, { label:'Nhập, xuất .xtp (sau v1.0)', dis:true }], r.right, r.bottom + 2, { alignRight:true, returnFocus:a });
    };
    host.oncontextmenu = function(e){
      var ti = e.target.closest('[data-ti]'), tv = e.target.closest('[data-tp]'); if (!ti && !tv) return; e.preventDefault();
      if (ti){ var i = Number(ti.dataset.ti), tool = pal.tools[i]; TC.menu([{ label:'Dùng công cụ', icon:'play', act:function(){ tpRun(tool); } }, { label:'Thuộc tính công cụ…', icon:'props', act:function(){ tpProps(pal, i); } }, { label:'Sao chép công cụ', icon:'copy', act:function(){ pal.tools.splice(i + 1, 0, Object.assign(JSON.parse(JSON.stringify(tool)), { name:tool.name + ' (2)' })); tpSave(); rr(); } }, { label:'Lên', icon:'up', dis:!i, act:function(){ pal.tools.splice(i - 1, 0, pal.tools.splice(i, 1)[0]); tpSave(); rr(); } }, { label:'Xuống', icon:'down', dis:i >= pal.tools.length - 1, act:function(){ pal.tools.splice(i + 1, 0, pal.tools.splice(i, 1)[0]); tpSave(); rr(); } }, { sep:1 }, { label:'Xóa công cụ', icon:'trash', act:function(){ TC.confirm({ title:'Xóa công cụ', msg:'Xóa công cụ “' + esc(tool.name) + '” khỏi palette ' + esc(pal.name) + '?', ok:'Xóa công cụ', danger:true }).then(function(y){ if (y){ pal.tools.splice(i, 1); tpSave(); rr(); } }); } }], e.clientX, e.clientY); return; }
      var p = TP.pals.filter(function(x){ return x.id === tv.dataset.tp; })[0];
      TC.menu([{ label:'Palette mới…', icon:'plus', act:tpNewPal }, { label:'Đổi tên palette…', act:function(){ SC.ask('Đổi tên palette', 'Tên palette', p.name, null, 'UI-G18').then(function(v){ if (v){ p.name = v; tpSave(); rr(); } }); } }, { label:'Xóa palette', icon:'trash', dis:TP.pals.length < 2, act:function(){ TC.confirm({ title:'Xóa palette', msg:'Xóa palette <b>' + esc(p.name) + '</b> cùng ' + p.tools.length + ' công cụ?', ok:'Xóa palette', danger:true }).then(function(y){ if (y){ TP.pals = TP.pals.filter(function(x){ return x !== p; }); TP.groups.forEach(function(g){ if (g[1]) g[1] = g[1].filter(function(id){ return id !== p.id; }); }); tpSave(); rr(); } }); } }, { sep:1 }, { label:'Thuộc nhóm', sub:TP.groups.filter(function(g){ return g[1]; }).map(function(g){ var on = g[1].indexOf(p.id) >= 0; return { label:esc(g[0]), check:on, act:function(){ if (on) g[1] = g[1].filter(function(id){ return id !== p.id; }); else g[1].push(p.id); tpSave(); rr(); } }; }) }], e.clientX, e.clientY);
    };
    TC.listKeys($('.tpl', host), '[data-ti]', function(b, enter){ if (enter) tpRun(pal.tools[Number(b.dataset.ti)]); else { $$('[data-ti]', host).forEach(function(x){ x.setAttribute('aria-selected', String(x === b)); }); b.focus(); } });
  } };
  function tpNewPal(){ SC.ask('Palette mới', 'Tên palette', 'Palette ' + (TP.pals.length + 1), function(v){ return TP.pals.some(function(p){ return p.name === v; }) ? 'Đã có palette “' + v + '”.' : null; }, 'UI-G18').then(function(v){ if (!v) return; var id = 'p' + Date.now().toString(36); TP.pals.push({ id:id, name:v, tools:[] }); var g = TP.groups.filter(function(x){ return x[0] === TP.group; })[0]; if (g && g[1]) g[1].push(id); TP.cur = id; tpSave(); A().refreshPalettes(); }); }
  // tạo công cụ từ đối tượng đã chọn: block, hatch, hoặc lệnh vẽ cùng loại với layer, màu của đối tượng
  function toolFrom(e){ var base = { layer:e.layer }; if (e.color != null) base.color = e.color;
    if (e.t === 'insert' && !e.xref) return Object.assign(base, { t:'block', name:'Block ' + e.name, block:e.name, sc:Math.abs(e.sx) === Math.abs(e.sy) ? Math.abs(e.sx) : 1, rot:e.rot || 0 });
    if (e.t === 'hatch') return Object.assign(base, { t:'hatch', name:'Hatch ' + e.pat + ' · ' + e.layer, pat:e.pat, sc:e.sc || 1, ang:e.ang || 0 });
    var c = { line:'LINE', pline:'PLINE', circle:'CIRCLE', arc:'ARC', ellipse:'ELLIPSE', spline:'SPLINE', text:'TEXT', mtext:'MTEXT', dim:'DIM', mleader:'MLEADER', table:'TABLE' }[e.t]; return c ? Object.assign(base, { t:'cmd', name:TC.CMDS[c].desc + ' · ' + e.layer, cmd:c }) : null; }
  function tpFromSel(ids){ var d = D(), es = (ids || d.sel).map(function(id){ return d.byId[id]; }).filter(Boolean); if (!es.length){ TC.cmdline.msg('Chọn một đối tượng trên bản vẽ (block, hatch, line, chữ…) rồi bấm + hoặc kéo vào palette.'); return; } var tool = toolFrom(es[0]); if (!tool){ TC.toast('Chưa tạo được công cụ từ ' + (TC.TYPE_NAMES[es[0].t] || es[0].t) + '. Dùng block, hatch, line, polyline, chữ, kích thước.', { type:'warn', log:false }); return; } tpAdd(tool); }
  // kéo đối tượng đang chọn từ vùng vẽ thả vào palette (07.1 §9.11)
  var DR = null;
  document.addEventListener('mousedown', function(e){ DR = null; var d = D(); if (e.button || !d || TC.isViewer() || !d.sel.length || TC.cad.mode() !== 'idle' || TC.cmdline.busy() || !e.target.closest || !e.target.closest('#cv') || !$('.tp')) return; var r = $('#cv').getBoundingClientRect(), id = V().hitTest(e.clientX - r.left, e.clientY - r.top); if (id && d.sel.indexOf(id) >= 0) DR = { ids:d.sel.slice(), x:e.clientX, y:e.clientY, on:false }; }, true);
  document.addEventListener('mousemove', function(e){ if (!DR) return; if (!(e.buttons & 1)){ tpDragEnd(); return; } if (!DR.on && Math.hypot(e.clientX - DR.x, e.clientY - DR.y) > 6){ DR.on = true; DR.ghost = TC.el('<div class="tpdrag">' + ic('tpal', 's12') + 'Tạo công cụ từ ' + DR.ids.length + ' đối tượng</div>'); document.body.appendChild(DR.ghost); } if (!DR.on) return; e.stopPropagation(); DR.ghost.style.left = (e.clientX + 14) + 'px'; DR.ghost.style.top = (e.clientY + 12) + 'px'; var over = document.elementFromPoint(e.clientX, e.clientY), tp = over && over.closest('.tp'); $$('.tp').forEach(function(x){ x.classList.toggle('drop', x === tp); }); DR.ghost.classList.toggle('ok', !!tp); }, true);
  function tpDragEnd(){ if (DR && DR.ghost) DR.ghost.remove(); $$('.tp.drop').forEach(function(x){ x.classList.remove('drop'); }); DR = null; }
  addEventListener('mouseup', function(e){ if (!DR) return; var dr = DR, over = document.elementFromPoint(e.clientX, e.clientY); tpDragEnd(); if (dr.on && over && over.closest('.tp')) tpFromSel(dr.ids); }, true);
  function tpProps(pal, i){
    var tool = pal.tools[i], d = D(), t = tool.t, lays = ['Dùng hiện hành'].concat(d.layers.map(function(l){ return l.name; })), cols = [['ByLayer', null], ['Đỏ', 1], ['Vàng', 2], ['Xanh lá', 3], ['Xanh lơ', 4], ['Xanh dương', 5], ['Tím', 6], ['Trắng', 7]];
    var colSel = '<select class="inp" id="tq-col" style="width:160px">' + cols.map(function(c){ return '<option value="' + (c[1] == null ? '' : c[1]) + '"' + (c[1] === (tool.color == null ? null : tool.color) ? ' selected' : '') + '>' + c[0] + '</option>'; }).join('') + '</select>', layRow = H.row('Layer', H.sel('tq-lay', lays, tool.layer || 'Dùng hiện hành', 200), 110) + H.row('Màu', colSel, 110);
    var spec = t === 'block' ? H.row('Block', '<input class="inp mono" id="tq-b" value="' + esc(tool.block) + '" style="width:200px" list="tq-bl" spellcheck="false"><datalist id="tq-bl">' + userBlocks(d).concat(libList().map(function(x){ return x.name; })).filter(function(n, k, a){ return a.indexOf(n) === k; }).map(function(n){ return '<option value="' + esc(n) + '">'; }).join('') + '</datalist>', 110) + layRow + H.row('Tỉ lệ', H.num('tq-sc', TC.d(tool.sc || 1, 4), { prec:4, unit:'none', w:120, pick:false }), 110) + H.row('Góc xoay', H.num('tq-rot', TC.d(tool.rot || 0, 0), { prec:0, unit:'deg', w:120, pick:false }), 110) + H.row('', H.chk('tq-ask', 'Hỏi góc khi chèn', !!tool.askRot) + H.chk('tq-exp', 'Phá khối khi chèn', !!tool.explode), 110)
      : t === 'hatch' ? H.row('Mẫu', H.sel('tq-pat', SC.PATS.map(function(p){ return p[0]; }), tool.pat, 200), 110) + H.row('Tỉ lệ', H.num('tq-sc', TC.d(tool.sc || 1, 2), { unit:'none', w:120, pick:false }), 110) + H.row('Góc', H.num('tq-rot', TC.d(tool.ang || 0, 0), { prec:0, unit:'deg', w:120, pick:false }), 110) + layRow
      : t === 'cmd' ? H.row('Lệnh', '<input class="inp mono" id="tq-cmd" value="' + esc(tool.cmd) + '" style="width:200px" spellcheck="false">', 110) + layRow : H.row('Macro LISP', '<textarea class="inp mono" id="tq-code" rows="4" style="width:100%;height:auto;padding:4px 7px" spellcheck="false">' + esc(tool.code || '') + '</textarea>', 110);
    var dlg = TC.dialog({ id:'tprops', title:'Thuộc tính công cụ', code:'UI-G18', w:480, body:'<div class="stack">' + H.row('Tên', '<input class="inp" id="tq-n" value="' + esc(tool.name) + '" style="width:260px">', 110) + H.row('Loại', '<span class="tag">' + { block:'Block', hatch:'Hatch', cmd:'Lệnh', lisp:'Macro LISP' }[t] + '</span><span class="faint" style="font-size:var(--fs-s)">Palette ' + esc(pal.name) + '</span>', 110) + '<div class="hsep"></div>' + spec + '<span class="fmsg" id="tq-m"></span></div>',
      foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="tq-ok">Lưu công cụ</button>', focus:'#tq-n' });
    $('#tq-ok', dlg.el).addEventListener('click', function(){
      var g = function(id){ var el = $('#' + id, dlg.el); return el ? el.value : null; }, m = $('#tq-m', dlg.el), nm = g('tq-n').trim(), err = !nm ? 'Nhập tên công cụ.' : t === 'block' && !d.blocks[g('tq-b').trim()] && !libList().some(function(x){ return x.name === g('tq-b').trim(); }) ? 'Không có block “' + g('tq-b').trim() + '” trong bản vẽ hoặc thư viện.' : t === 'cmd' && !TC.cmd.def(g('tq-cmd')) ? 'Không có lệnh “' + g('tq-cmd') + '”.' : null;
      if (err){ m.className = 'fmsg err'; m.innerHTML = ic('warn', 'xs') + esc(err); return; }
      tool.name = nm; if (g('tq-lay') != null){ var l = g('tq-lay'); if (l === 'Dùng hiện hành') delete tool.layer; else tool.layer = l; var cv = g('tq-col'); if (cv === '') delete tool.color; else tool.color = Number(cv); }
      if (t === 'block'){ tool.block = g('tq-b').trim(); tool.sc = num(g('tq-sc'), 1) || 1; tool.rot = num(g('tq-rot'), 0); tool.askRot = $('#tq-ask', dlg.el).checked; tool.explode = $('#tq-exp', dlg.el).checked; }
      if (t === 'hatch'){ tool.pat = g('tq-pat'); tool.sc = num(g('tq-sc'), 1) || 1; tool.ang = num(g('tq-rot'), 0); }
      if (t === 'cmd') tool.cmd = TC.cmd.def(g('tq-cmd')).name; if (t === 'lisp') tool.code = g('tq-code');
      tpSave(); dlg.close(true); A().refreshPalettes(); });
  }
  IMPL.TOOLPALETTES = function(ctx, pre){ A().showPalette('tools', pre === 'toggle' || undefined); };

})();
