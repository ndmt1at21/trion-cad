/* TrionCAD prototype · palette: Thuộc tính UI-C01, Layer UI-C03/M04, Bác sĩ Font UI-J01/J02/J04, Tham chiếu ngoài UI-G10 (xem). */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic, G = TC.geo;
  var P = TC.PALETTES = {};
  function A(){ return TC.app; }

  /* ================= Thuộc tính UI-C01 ================= */
  var PD = { // mô tả ở đáy palette: tên → [giải thích, mã DXF/biến hệ thống]
    'Màu':['Màu của đối tượng. ByLayer: lấy theo màu của layer.','DXF 62 · CECOLOR'], 'Layer':['Layer chứa đối tượng. Khi không chọn gì: layer hiện hành cho đối tượng vẽ mới.','DXF 8 · CLAYER'],
    'Linetype':['Kiểu đường của đối tượng.','DXF 6 · CELTYPE'], 'Tỉ lệ linetype':['Tỉ lệ riêng của đối tượng, nhân với tỉ lệ toàn cục.','DXF 48 · CELTSCALE'], 'Plot style':['Kiểu in theo màu (CTB) hoặc theo tên (STB).','DXF 390'],
    'Độ dày nét':['Độ dày khi in và khi bật hiển thị độ dày nét.','DXF 370 · CELWEIGHT'], 'Độ trong suốt':['Độ trong suốt từ 0 đến 90.','DXF 440 · CETRANSPARENCY'], 'Độ dày đùn':['Chiều cao đùn theo trục Z.','DXF 39 · THICKNESS'],
    'Độ dài':['Chỉ đọc. Tính từ hình học; bấm để sao chép.',''], 'Diện tích':['Chỉ đọc. Tính từ hình học; bấm để sao chép.',''], 'Góc':['Chỉ đọc. Góc trong mặt XY, đo từ trục X.',''],
    'Nội dung':['Chuỗi chữ hiển thị.','DXF 1'], 'Kiểu chữ':['Kiểu chữ quyết định font và hiệu ứng.','DXF 7 · TEXTSTYLE'], 'Chiều cao':['Chiều cao chữ hoa theo đơn vị bản vẽ.','DXF 40 · TEXTSIZE'],
    'Bảng mã':['TrionCAD nhận diện bảng mã từ tên font và nội dung chữ. Chữ mã TCVN3/VNI vẫn hiển thị đúng; chuyển sang Unicode để mở được ở mọi nơi.','Bác sĩ Font'],
    'Tên':['Tên định nghĩa block.','DXF 2'], 'Kiểu kích thước':['Kiểu kích thước quyết định chữ, mũi tên, đơn vị.','DXF 3 · DIMSTYLE'], 'Giá trị đo':['Chỉ đọc. Giá trị hình học thật của kích thước.',''],
    'Tên mẫu':['Mẫu hatch.','DXF 2'], 'Tỉ lệ':['Tỉ lệ mẫu hatch.','DXF 41'], 'Tâm X':['Tọa độ X của tâm.','DXF 10'], 'Bán kính':['Bán kính.','DXF 40'], 'Tỉ lệ chú thích':['Tỉ lệ chú thích hiện hành.','CANNOSCALE'], 'Kiểu hiển thị':['Kiểu hiển thị của viewport hiện hành.','VSCURRENT']
  };
  var TYPE = { line:'Line', pline:'Polyline', circle:'Circle', arc:'Arc', ellipse:'Ellipse', text:'Text', mtext:'MText', dim:'Kích thước', insert:'Block', hatch:'Hatch', point:'Point' };
  TC.TYPE_NAMES = TYPE;
  var pstate = { filter:'all', focus:'Layer', collapsed:{}, vtx:0 };
  function lay(d, e){ return d.layerMap[e.layer] || {}; }
  function colorCell(d, e){ var c = e.color != null ? e.color : null; var lc = lay(d, e).color; var hex = TC.ACI[c != null ? c : lc] || '#FFF'; if ((c != null ? c : lc) === 7) hex = '#FFFFFF'; return '<i class="sw" style="background:' + hex + '"></i>' + (c == null ? 'ByLayer' : TC.aciName(c)); }
  function ltCell(e, d){ var lt = e.lt || 'ByLayer'; var real = e.lt || lay(d, e).lt; return '<span class="lt-s' + (real === 'CENTER' ? ' center' : real === 'DASHED' ? ' dash' : '') + '"></span>' + esc(lt); }
  function common(d, e){
    return [['Màu', colorCell(d, e), 'html'], ['Layer', esc(e.layer)], ['Linetype', ltCell(e, d), 'html'], ['Tỉ lệ linetype', '1.0000', 'num'], ['Plot style', 'ByColor'], ['Độ dày nét', '<span class="lw-s" style="height:' + Math.max(1, Math.round((e.lw || lay(d, e).lw || .25) * 4)) + 'px"></span>' + (e.lw != null ? TC.d(e.lw) + ' mm' : 'ByLayer'), 'html'], ['Độ trong suốt', 'ByLayer'], ['Độ dày đùn', '0.00', 'num']];
  }
  function geom(d, e){
    var rows = [], f = function(v, p){ return TC.d(v, p); };
    switch (e.t){
      case 'line': { var dx = e.p2[0] - e.p1[0], dy = e.p2[1] - e.p1[1]; rows = [['Điểm đầu X', f(e.p1[0]), 'num'], ['Điểm đầu Y', f(e.p1[1]), 'num'], ['Điểm đầu Z', '0.00', 'num'], ['Điểm cuối X', f(e.p2[0]), 'num'], ['Điểm cuối Y', f(e.p2[1]), 'num'], ['Điểm cuối Z', '0.00', 'num'], ['Delta X', f(dx), 'ro'], ['Delta Y', f(dy), 'ro'], ['Delta Z', '0.00', 'ro'], ['Độ dài', f(Math.hypot(dx, dy)), 'ro'], ['Góc', f((Math.atan2(dy, dx) * 180 / Math.PI + 360) % 360, 0) + '°', 'ro']]; break; }
      case 'pline': { var i = Math.min(pstate.vtx, e.pts.length - 1), v = e.pts[i]; rows = [['Đỉnh hiện tại', '<span class="mono">' + (i + 1) + ' / ' + e.pts.length + '</span><span style="margin-left:auto;display:flex;gap:1px"><button class="ib sm" type="button" data-vtx="-1" aria-label="Đỉnh trước">' + ic('left', 'xs') + '</button><button class="ib sm" type="button" data-vtx="1" aria-label="Đỉnh kế">' + ic('right', 'xs') + '</button></span>', 'html'], ['Đỉnh X', f(v[0]), 'num'], ['Đỉnh Y', f(v[1]), 'num'], ['Bề rộng đầu đoạn', '0.00', 'num'], ['Bề rộng cuối đoạn', '0.00', 'num'], ['Bề rộng toàn cục', '0.00', 'num'], ['Cao độ', '0.00', 'num'], ['Đóng', e.closed ? 'Có' : 'Không'], ['Linetype liên tục', 'Tắt']];
        if (e.closed) rows.push(['Diện tích', f(G.polyArea(e.pts), 0) + ' (' + f(G.polyArea(e.pts) / 1e6) + ' m²)', 'ro']); rows.push(['Độ dài', f(G.polyLen(e.pts, e.closed)), 'ro']); break; }
      case 'circle': rows = [['Tâm X', f(e.c[0]), 'num'], ['Tâm Y', f(e.c[1]), 'num'], ['Tâm Z', '0.00', 'num'], ['Bán kính', f(e.r), 'num'], ['Đường kính', f(e.r * 2), 'num'], ['Chu vi', f(2 * Math.PI * e.r), 'num'], ['Diện tích', f(Math.PI * e.r * e.r, 0), 'num']]; break;
      case 'arc': { var sw = ((e.a1 - e.a0) % 360 + 360) % 360; rows = [['Tâm X', f(e.c[0]), 'num'], ['Tâm Y', f(e.c[1]), 'num'], ['Bán kính', f(e.r), 'num'], ['Góc đầu', f(e.a0, 0) + '°', 'num'], ['Góc cuối', f(e.a1, 0) + '°', 'num'], ['Tổng góc', f(sw, 0) + '°', 'ro'], ['Độ dài cung', f(e.r * sw * Math.PI / 180), 'ro']]; break; }
      case 'text': { var st = d.styleMap[e.style] || {}; rows = [['Nội dung', esc(e.s)], ['Kiểu chữ', esc(e.style)], ['Chú thích theo tỉ lệ', 'Không'], ['Điểm canh', { l:'Trái', c:'Giữa', r:'Phải', m:'Giữa tâm' }[e.al || 'l']], ['Chiều cao', f(e.h), 'num'], ['Góc xoay', f(e.rot || 0, 0) + '°', 'num'], ['Hệ số rộng', '1.0000', 'num'], ['Góc nghiêng', '0°', 'num'], ['Vị trí X', f(e.p[0]), 'num'], ['Vị trí Y', f(e.p[1]), 'num'], ['Lộn ngược', 'Không'], ['Viết ngược', 'Không'],
        ['Bảng mã', '<span class="tag ' + (st.enc === 'Unicode' ? 'ok' : st.enc === 'Không rõ' ? '' : 'warn') + '">' + esc(st.enc || 'Unicode') + '</span>' + (st.enc === 'TCVN3' || st.enc === 'VNI' ? '<button class="btn sm" type="button" data-conv style="margin-left:auto;height:18px;padding:0 6px">Chuyển sang Unicode' + (TC.can('VP') ? '' : ' <span class="ed xs" data-ed="vp">VP</span>') + '</button>' : ''), 'html']]; break; }
      case 'dim': rows = [['Kiểu kích thước', esc(e.style)], ['Chú thích theo tỉ lệ', 'Không'], ['Giá trị đo', f(e._meas), 'ro'], ['Ghi đè chữ', e.over ? esc(e.over) : '<span class="faint">(không)</span>', 'html'], ['Mũi tên 1', 'Gạch nghiêng kiến trúc'], ['Mũi tên 2', 'Gạch nghiêng kiến trúc'], ['Cỡ mũi tên', '1.50', 'num'], ['Chiều cao chữ', '2.20', 'num'], ['Độ chính xác', '0'], ['Hệ số tỉ lệ đo', '1.0000', 'num']]; break;
      case 'insert': { rows = [['Tên', esc(e.name), 'ro'], ['Vị trí X', f(e.p[0]), 'num'], ['Vị trí Y', f(e.p[1]), 'num'], ['Vị trí Z', '0.00', 'num'], ['Tỉ lệ X', f(e.sx, 4), 'num'], ['Tỉ lệ Y', f(e.sy, 4), 'num'], ['Tỉ lệ Z', f(Math.abs(e.sx), 4), 'num'], ['Góc xoay', f(e.rot || 0, 0) + '°', 'num'], ['Đơn vị block', 'Milimét', 'ro'], ['Chú thích theo tỉ lệ', 'Không']]; break; }
      case 'hatch': { var ar = e._p[0].loops.reduce(function(s, l, k){ return s + (k ? -1 : 1) * G.polyArea(l); }, 0); rows = [['Loại', e.pat === 'SOLID' ? 'Tô đặc' : 'Có sẵn'], ['Tên mẫu', e.pat], ['Chú thích theo tỉ lệ', 'Không'], ['Góc', f(e.ang || 0, 0) + '°', 'num'], ['Tỉ lệ', f(e.sc || 1, 4), 'num'], ['Gốc X', '0.00', 'num'], ['Gốc Y', '0.00', 'num'], ['Liên kết', e.assoc ? 'Có' : 'Không'], ['Nhận diện island', 'Thường'], ['Màu nền', 'Không'], ['Diện tích', f(Math.abs(ar), 0) + ' (' + f(Math.abs(ar) / 1e6) + ' m²)', 'ro']]; break; }
    }
    return rows;
  }
  function attrRows(d, e){ if (e.t !== 'insert') return []; var bd = d.blocks[e.name]; if (!bd) return []; return bd.ents.filter(function(x){ return x.t === 'attdef'; }).map(function(a){ return [a.tag, esc(e.attrs && e.attrs[a.tag] != null ? e.attrs[a.tag] : a.s)]; }); }
  TC.describe = function(d, e){
    var out = [(TYPE[e.t] || e.t).toUpperCase() + '   Layer: "' + e.layer + '"   Không gian: ' + (e._paper ? 'Paper' : 'Model'), '   Handle = ' + (0x2A0 + e.id).toString(16).toUpperCase()];
    geom(d, e).forEach(function(r){ out.push('   ' + r[0] + ' = ' + String(r[1]).replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim()); });
    attrRows(d, e).forEach(function(r){ out.push('   Attribute ' + r[0] + ' = ' + r[1]); });
    return out.join('\n');
  };
  function groupHTML(name, rows, ro){
    var col = pstate.collapsed[name];
    return '<button class="pgh" type="button" data-pg="' + esc(name) + '" aria-expanded="' + !col + '">' + ic('down', 'xs') + esc(name) + '</button>' + (col ? '' : rows.map(function(r){
      var k = r[0], kind = r[2], cls = 'pr' + (ro || kind === 'ro' ? ' ro' : '') + (pstate.focus === k ? ' is-focus' : '');
      return '<div class="' + cls + '" data-pk="' + esc(k) + '"><span class="pk">' + esc(k) + '</span><span class="pv' + (kind === 'num' || kind === 'ro' ? ' mono' : '') + '">' + (kind === 'html' ? r[1] : '<span>' + r[1] + '</span>') + (kind !== 'html' ? '<button class="ib sm copy" type="button" data-copyv aria-label="Sao chép giá trị">' + ic('copyclip', 's12') + '</button>' : '') + '</span></div>';
    }).join(''));
  }
  P.props = { name:'Thuộc tính', render:function(host, d){
    if (!d){ host.innerHTML = '<div class="empty">' + ic('props') + '<b>Chưa mở bản vẽ</b>Mở một bản vẽ để xem thuộc tính.</div>'; return; }
    var sel = d.sel.map(function(id){ return d.byId[id]; }).filter(Boolean), viewer = TC.isViewer();
    var types = {}; sel.forEach(function(e){ types[e.t] = (types[e.t] || 0) + 1; });
    var tkeys = Object.keys(types); if (pstate.filter !== 'all' && !types[pstate.filter]) pstate.filter = 'all';
    var shown = pstate.filter === 'all' ? sel : sel.filter(function(e){ return e.t === pstate.filter; });
    var label = !sel.length ? 'Không chọn' : pstate.filter === 'all' ? (tkeys.length === 1 ? TYPE[tkeys[0]] + ' (' + sel.length + ')' : 'Tất cả (' + TC.n(sel.length) + ')') : TYPE[pstate.filter] + ' (' + types[pstate.filter] + ')';
    var html = (viewer ? '<div class="pal-note">' + ic('lock') + '<span>Chỉ đọc trong Viewer. Bấm một giá trị để sao chép.</span></div>' : '') +
      '<div class="pal-tools"><button class="dd grow" type="button" id="pp-type" aria-haspopup="menu"><span class="lbl">' + esc(label) + '</span>' + ic('down', 'xs') + '</button>' +
      '<button class="ib is-on" type="button" aria-pressed="true" aria-label="Cộng dồn vùng chọn (PICKADD)" data-tip="Cộng dồn vùng chọn" data-cmd="PICKADD">' + ic('pickadd') + '</button>' +
      '<button class="ib" type="button" id="pp-pick" aria-label="Chọn đối tượng" data-tip="Chọn đối tượng">' + ic('select') + '</button>' +
      '<button class="ib" type="button" id="pp-qs" aria-label="Chọn nhanh" data-tip="Chọn nhanh" data-cmd="QSELECT" data-need="PRO">' + ic('qselect') + '</button></div><div class="pal-body">';
    if (!sel.length){
      var v = d.space === 'model' ? d.view : null, W = TC.app.view.W, H = TC.app.view.H;
      html += groupHTML('Chung', [['Màu', '<i class="sw" style="background:#FFF"></i>ByLayer', 'html'], ['Layer', 'KT-TUONG'], ['Linetype', '<span class="lt-s"></span>ByLayer', 'html'], ['Tỉ lệ linetype', '1.0000', 'num'], ['Độ dày nét', '<span class="lw-s" style="height:1px"></span>ByLayer', 'html'], ['Độ trong suốt', 'ByLayer'], ['Độ dày đùn', '0.00', 'num']], viewer) +
        (v ? groupHTML('View', [['Tâm X', TC.d(v.cx), 'ro'], ['Tâm Y', TC.d(v.cy), 'ro'], ['Tâm Z', '0.00', 'ro'], ['Cao', TC.d(H / v.s), 'ro'], ['Rộng', TC.d(W / v.s), 'ro']], true) : '') +
        groupHTML('Khác', [['Tỉ lệ chú thích', '1:100'], ['Biểu tượng UCS bật', 'Có'], ['Biểu tượng UCS tại gốc', 'Không'], ['UCS theo viewport', 'Có'], ['Tên UCS', 'Thế giới'], ['Kiểu hiển thị', '2D Wireframe']], viewer);
    } else if (shown.length === 1 || (pstate.filter === 'all' && tkeys.length === 1 && shown.length > 1)){
      var e = shown[0], multi = shown.length > 1;
      var vary = function(rows){ if (!multi) return rows; return rows.map(function(r){ var same = shown.every(function(x){ var rr = (r[0] === 'Layer' ? [['Layer', esc(x.layer)]] : common(d, x).concat(geom(d, x))).filter(function(q){ return q[0] === r[0]; })[0]; return rr && rr[1] === r[1]; }); return same ? r : [r[0], '<span class="varies">*Khác nhau*</span>', 'html']; }); };
      html += groupHTML('Chung', vary(common(d, e)), viewer);
      var g = geom(d, e); if (g.length) html += groupHTML(e.t === 'text' ? 'Chữ' : e.t === 'dim' ? 'Kích thước' : e.t === 'insert' ? 'Hình học' : e.t === 'hatch' ? 'Mẫu' : 'Hình học', vary(g), viewer);
      var at = attrRows(d, e); if (at.length && !multi) html += groupHTML('Attribute', at, viewer);
    } else {
      // nhiều loại: chỉ thuộc tính chung, *Khác nhau* khi khác giá trị
      var rows = common(d, shown[0]).map(function(r){ var same = shown.every(function(x){ return common(d, x).filter(function(q){ return q[0] === r[0]; })[0][1] === r[1]; }); return same ? r : [r[0], '<span class="varies">*Khác nhau*</span>', 'html']; });
      html += groupHTML('Chung', rows, viewer);
      var ln = shown.reduce(function(s, x){ return s + (x.t === 'line' ? G.dist(x.p1, x.p2) : x.t === 'pline' ? G.polyLen(x.pts, x.closed) : 0); }, 0);
      if (ln) html += groupHTML('Tổng hợp', [['Tổng độ dài (line, polyline)', TC.d(ln), 'ro']], true);
    }
    html += '</div>';
    var pd = PD[pstate.focus] || PD[pstate.focus.replace(/ [XYZ]$/, '')] || ['Thuộc tính của đối tượng đang chọn.', ''];
    html += '<div class="pal-desc"><b>' + esc(pstate.focus) + '</b><p>' + esc(pd[0]) + '</p>' + (pd[1] ? '<code>' + esc(pd[1]) + '</code>' : '') + '</div>';
    host.innerHTML = html;
    $('#pp-type', host).addEventListener('click', function(){ var r = this.getBoundingClientRect(); var it = [{ label:'Tất cả (' + TC.n(sel.length) + ')', check:pstate.filter === 'all', act:function(){ pstate.filter = 'all'; P.props.render(host, d); } }].concat(tkeys.map(function(t){ return { label:TYPE[t] + ' (' + types[t] + ')', check:pstate.filter === t, act:function(){ pstate.filter = t; P.props.render(host, d); } }; })); if (!sel.length) it = [{ label:'Không chọn', check:true }]; TC.menu(it, r.left, r.bottom + 2, { minWidth:r.width }); });
    $('#pp-pick', host).addEventListener('click', function(){ $('#cv').focus(); TC.cmdline.msg('Chọn đối tượng trên bản vẽ; Shift + bấm để bớt.'); });
    $('#pp-qs', host).addEventListener('click', function(){ TC.screens.upgrade('Chọn nhanh', 'PRO'); });
    host.onclick = function(ev){
      var g2 = ev.target.closest('[data-pg]'); if (g2){ pstate.collapsed[g2.dataset.pg] = !pstate.collapsed[g2.dataset.pg]; P.props.render(host, d); return; }
      var vt = ev.target.closest('[data-vtx]'); if (vt){ var pl = shown[0]; pstate.vtx = (pstate.vtx + Number(vt.dataset.vtx) + pl.pts.length) % pl.pts.length; TC.app.view.temp = [{ k:'pt', p:pl.pts[pstate.vtx], color:TC.cssv('--c-grip-hot') }]; TC.app.view.drawOver(); P.props.render(host, d); return; }
      if (ev.target.closest('[data-conv]')){ if (TC.can('VP')) TC.app.showPalette('fontdoc'); else TC.screens.upgrade('Chuyển chữ mã cũ sang Unicode', 'VP'); return; }
      var row = ev.target.closest('.pr'); if (!row) return;
      pstate.focus = row.dataset.pk;
      if (ev.target.closest('[data-copyv]') || viewer){ var val = $('.pv', row).textContent.trim(); TC.copyText(val); }
      P.props.render(host, d);
    };
  } };
  TC.on('sel', function(){ pstate.vtx = 0; if (A().pal.rightActive === 'props' || A().pal.leftActive === 'props') A().refreshPalettes(); });
  TC.on('viewchg', function(){ var d = A().doc(); if (d && !d.sel.length && A().pal.rightActive === 'props') A().refreshPalettes(); });

  /* ================= Layer: thao tác tạm thời trong Viewer ================= */
  var L = TC.layers = {};
  L.get = function(d, n, k){ var l = d.layerMap[n], t = d.tempLayers[n] || {}; return t[k] != null ? t[k] : l[k]; };
  L.set = function(d, n, k, v){ if (TC.isViewer()){ d.tempLayers[n] = d.tempLayers[n] || {}; if (d.layerMap[n][k] === v) delete d.tempLayers[n][k]; else d.tempLayers[n][k] = v; } else d.layerMap[n][k] = v; };
  L.changed = function(d){ return Object.keys(d.tempLayers).some(function(n){ return Object.keys(d.tempLayers[n]).length; }); };
  L.restore = function(d){ d.tempLayers = {}; d.iso = null; A().view.draw(); A().refreshPalettes(); TC.cmdline.msg('Đã khôi phục trạng thái layer như tệp gốc.'); };
  L.isolate = function(d, keep){ d.iso = { prev:JSON.parse(JSON.stringify(d.tempLayers)) }; d.layers.forEach(function(l){ if (keep.indexOf(l.name) < 0) L.set(d, l.name, 'on', false); }); A().view.draw(); A().refreshPalettes(); };
  L.endIso = function(){ var d = A().doc(); if (!d) return; if (d.iso){ d.tempLayers = d.iso.prev; d.iso = null; } d.hidden = null; A().view.draw(); A().refreshPalettes(); };
  L.isolateSel = function(){ var d = A().doc(); var keep = {}; d.sel.forEach(function(id){ keep[d.byId[id].layer] = 1; }); L.isolate(d, Object.keys(keep)); TC.cad.clearSel(); TC.cmdline.msg('Đã cô lập. Chuột phải › Cô lập › Kết thúc cô lập để hiện lại.'); };
  L.hideSel = function(){ var d = A().doc(); var names = {}; d.sel.forEach(function(id){ names[d.byId[id].layer] = 1; }); d.iso = { prev:JSON.parse(JSON.stringify(d.tempLayers)) }; Object.keys(names).forEach(function(n){ L.set(d, n, 'on', false); }); TC.cad.clearSel(); A().view.draw(); A().refreshPalettes(); };
  var lstate = { q:'', sort:'name', dir:1, sel:null, filter:'all' };
  function layerRows(d, compact){
    var q = lstate.q.trim(), re = q ? new RegExp('^' + q.replace(/[.+^${}()|[\]\\]/g, '\\$&').replace(/\*/g, '.*').replace(/\?/g, '.') + (q.indexOf('*') < 0 ? '' : '$'), 'i') : null;
    var list = d.layers.filter(function(l){ if (lstate.filter === 'used' && !l.count) return false; return !re || re.test(l.name) || TC.fold(l.name).indexOf(TC.fold(q)) >= 0; });
    list.sort(function(a, b){ var k = lstate.sort; var x = a[k], y = b[k]; return (typeof x === 'number' ? x - y : String(x).localeCompare(String(y))) * lstate.dir; });
    return list;
  }
  function tgl(d, l, k, onIc, offIc, name, need){
    var v = L.get(d, l.name, k), dis = need && !TC.can(need), isOn = k === 'frz' ? !v : k === 'lck' ? !v : v;
    var temp = d.tempLayers[l.name] && d.tempLayers[l.name][k] != null;
    return '<button class="tgl' + (isOn ? '' : ' off') + '" type="button" data-lt="' + k + '" data-ln="' + esc(l.name) + '"' + (dis ? ' disabled aria-disabled="true"' : '') + ' aria-pressed="' + !!v + '" aria-label="' + name + ' ' + esc(l.name) + '" data-tip="' + name + (temp ? ' · đã đổi tạm thời' : '') + (dis ? ' · có trong TrionCAD Pro' : '') + '">' + ic(isOn ? onIc : offIc) + (temp ? '<span class="sr">đã đổi</span>' : '') + '</button>';
  }
  function swatch(c){ var hex = TC.ACI[c] || '#FFF'; return '<i class="sw" style="background:' + (c === 7 ? '#FFFFFF' : hex) + '"></i>'; }
  function layerTable(d, full){
    var rows = layerRows(d), viewer = TC.isViewer();
    var th = function(k, label, cls){ return '<th class="sortable ' + (cls || '') + '" data-sort="' + k + '" aria-sort="' + (lstate.sort === k ? (lstate.dir > 0 ? 'ascending' : 'descending') : 'none') + '">' + label + (lstate.sort === k ? (lstate.dir > 0 ? ' ▲' : ' ▼') : '') + '</th>'; };
    var cols = full ? '<colgroup><col style="width:34px"><col style="width:170px"><col style="width:34px"><col style="width:34px"><col style="width:34px"><col style="width:34px"><col style="width:84px"><col style="width:100px"><col style="width:90px"><col style="width:70px"><col style="width:70px"><col><col style="width:80px"></colgroup>'
      : '<colgroup><col style="width:26px"><col><col style="width:28px"><col style="width:28px"><col style="width:28px"><col style="width:26px"><col style="width:52px"></colgroup>';
    var head = full ? '<tr><th>TT</th>' + th('name', 'Tên') + '<th class="ctr">Bật</th><th class="ctr">Băng</th><th class="ctr">Khóa</th><th class="ctr">In</th>' + th('color', 'Màu') + th('lt', 'Linetype') + th('lw', 'Độ dày nét', 'num') + '<th class="num">Trong suốt</th><th>Plot style</th><th>Mô tả</th>' + th('count', 'Số đối tượng', 'num') + '</tr>'
      : '<tr><th></th>' + th('name', 'Tên') + '<th class="ctr" title="Bật/Tắt">' + ic('bulb', 's12') + '</th><th class="ctr" title="Đóng băng">' + ic('snow', 's12') + '</th><th class="ctr" title="Khóa">' + ic('lock', 's12') + '</th><th class="ctr" title="Màu">' + ic('plotstyle', 's12') + '</th>' + th('count', 'Số', 'num') + '</tr>';
    var body = rows.map(function(l){
      var cur = l.name === 'KT-TUONG', st = cur ? '<span class="tag acc" title="Layer hiện hành" style="height:16px;padding:0 4px">' + ic('check', 's12') + '</span>' : l.count ? '<span title="Đang dùng" style="color:var(--c-text-2)">●</span>' : '<span title="Không có đối tượng" class="faint">○</span>';
      return '<tr class="' + (lstate.sel === l.name ? 'is-sel' : '') + '" data-lrow="' + esc(l.name) + '"><td class="ctr">' + st + '</td><td>' + esc(l.name) + (d.tempLayers[l.name] && Object.keys(d.tempLayers[l.name]).length ? ' <span class="tag info" style="height:15px;font-size:9.5px">tạm</span>' : '') + '</td>' +
        '<td class="ctr">' + tgl(d, l, 'on', 'bulb', 'bulboff', 'Bật/Tắt') + '</td><td class="ctr">' + tgl(d, l, 'frz', 'sun', 'snow', 'Đóng băng') + '</td><td class="ctr">' + tgl(d, l, 'lck', 'unlock', 'lock', 'Khóa', 'PRO') + '</td>' +
        (full ? '<td class="ctr">' + tgl(d, l, 'plot', 'print', 'x', 'In', 'PRO') + '</td><td>' + swatch(l.color) + ' ' + (l.color <= 7 ? TC.aciName(l.color) : l.color) + '</td><td><span class="lt-s' + (l.lt === 'CENTER' ? ' center' : '') + '"></span> ' + esc(l.lt) + '</td><td class="num">' + TC.d(l.lw) + ' mm</td><td class="num">0</td><td>Color_' + l.color + '</td><td>' + esc(l.desc || '') + '</td>' : '<td class="ctr">' + swatch(l.color) + '</td>') +
        '<td class="num">' + TC.n(l.count) + '</td></tr>';
    }).join('');
    return { html:'<table class="tbl">' + cols + '<thead>' + head + '</thead><tbody>' + body + '</tbody></table>', n:rows.length };
  }
  function bindLayerTable(root, d, rerender){
    root.addEventListener('click', function(e){
      var t = e.target.closest('[data-lt]'); if (t){ e.stopPropagation(); if (t.disabled) { TC.screens.upgrade('Khóa layer, đổi thuộc tính layer', 'PRO'); return; } var n = t.dataset.ln, k = t.dataset.lt; L.set(d, n, k, !L.get(d, n, k)); A().view.draw(); rerender(); TC.emit('layers'); return; }
      var s = e.target.closest('[data-sort]'); if (s){ if (lstate.sort === s.dataset.sort) lstate.dir *= -1; else { lstate.sort = s.dataset.sort; lstate.dir = 1; } rerender(); return; }
      var r = e.target.closest('[data-lrow]'); if (r){ lstate.sel = r.dataset.lrow; rerender(); }
    });
    root.addEventListener('dblclick', function(e){ var r = e.target.closest('[data-lrow]'); if (r && !TC.isViewer()) TC.toast('Đặt hiện hành: có trong bản biên tập.', { type:'info', log:false }); else if (r) TC.toast('Viewer không đổi layer hiện hành.', { type:'info', log:false }); });
    root.addEventListener('contextmenu', function(e){
      var r = e.target.closest('[data-lrow]'); if (!r) return; e.preventDefault(); var n = r.dataset.lrow; lstate.sel = n; rerender();
      TC.menu([{ label:'Đặt hiện hành', need:'PRO' }, { label:'Layer mới', need:'PRO' }, { label:'Đổi tên', need:'PRO', kbd:'F2' }, { label:'Xóa', need:'PRO' }, { label:'Sửa mô tả', need:'PRO' }, { sep:1 },
        { label:'Chọn đối tượng trên layer này', icon:'select', act:function(){ d.sel = d.ents.filter(function(x){ return x.layer === n && TC.entVisible(d, x); }).map(function(x){ return x.id; }); TC.cad.afterSel(); TC.cmdline.msg('Đã chọn ' + d.sel.length + ' đối tượng trên layer ' + esc(n) + '.'); } },
        { label:'Cô lập layer này', icon:'isolate', act:function(){ L.isolate(d, [n]); rerender(); } }, { label:'Gộp các layer đã chọn vào…', need:'PRO' }, { sep:1 }, { label:'Lưu trạng thái layer', need:'PRO' }, { label:'Khôi phục như tệp gốc', icon:'undo', dis:!L.changed(d), act:function(){ L.restore(d); } }], e.clientX, e.clientY);
    });
  }
  P.layers = { name:'Layer', render:function(host, d){
    if (!d){ host.innerHTML = ''; return; }
    var viewer = TC.isViewer(), t = layerTable(d, false);
    host.innerHTML = (viewer ? '<div class="pal-note">' + ic('info') + '<span>Bật/tắt và đóng băng là tạm thời. Thay đổi không lưu vào bản vẽ.</span></div>' : '') +
      '<div class="pal-tools"><label class="search grow">' + ic('search') + '<input id="ly-q" placeholder="Tìm layer, ví dụ KT-*" value="' + esc(lstate.q) + '" aria-label="Tìm layer"></label>' +
      '<button class="ib" type="button" id="ly-full" aria-label="Mở Quản lý layer đầy đủ" data-tip="Quản lý layer" data-cmd="LAYER" data-desc="Mở dạng palette lớn với đủ cột, cây bộ lọc.">' + ic('fullscreen') + '</button>' +
      '<button class="ib" type="button" id="ly-more" aria-label="Thao tác khác">' + ic('more') + '</button></div>' +
      '<div class="pal-body" id="ly-body">' + t.html + '</div>' +
      '<div class="pal-foot"><span>Hiện ' + t.n + '/' + d.layers.length + ' layer</span><span class="grow"></span>' + (L.changed(d) ? '<button class="btn sm" type="button" id="ly-restore">' + ic('undo') + 'Khôi phục như tệp gốc</button>' : '') + '</div>';
    var rer = function(){ P.layers.render(host, d); var q = $('#ly-q', host); if (q && document.activeElement !== q && lstate.focusQ) { q.focus(); q.setSelectionRange(q.value.length, q.value.length); } };
    $('#ly-q', host).addEventListener('input', function(){ lstate.q = this.value; lstate.focusQ = true; rer(); });
    $('#ly-q', host).addEventListener('blur', function(){ lstate.focusQ = false; });
    $('#ly-full', host).addEventListener('click', function(){ TC.screens.layerManager(); });
    $('#ly-more', host).addEventListener('click', function(){ var r = this.getBoundingClientRect(); TC.menu([{ label:'Chỉ hiện layer đang dùng', check:lstate.filter === 'used', act:function(){ lstate.filter = lstate.filter === 'used' ? 'all' : 'used'; rer(); } }, { label:'Bật tất cả', icon:'bulb', act:function(){ TC.cmd.run('LAYON'); } }, { label:'Rã đông tất cả', icon:'sun', act:function(){ TC.cmd.run('LAYTHW'); } }, { label:'Cô lập layer của đối tượng…', icon:'isolate', act:function(){ TC.cmd.run('LAYISO'); } }, { label:'Bỏ cô lập', icon:'unisolate', act:function(){ TC.cmd.run('LAYUNISO'); } }, { sep:1 }, { label:'Nạp bộ layer chuẩn TCVN…', need:'PRO' }, { label:'Khôi phục như tệp gốc', icon:'undo', dis:!L.changed(d), act:function(){ L.restore(d); } }], r.right, r.bottom + 2, { alignRight:true }); });
    var rs = $('#ly-restore', host); if (rs) rs.addEventListener('click', function(){ L.restore(d); });
    bindLayerTable($('#ly-body', host), d, rer);
  } };
  TC.layerTable = layerTable; TC.bindLayerTable = bindLayerTable; TC.lstate = lstate;
  TC.on('layers', function(){ A().refreshPalettes(); });

  /* ================= Bác sĩ Font UI-J01, J02, J04 ================= */
  var FD = TC.fontDoctor = {};
  FD.settings = TC.load('fdset', { autoScan:true, threshold:80, show:'banner', table:'Mặc định TrionCAD', trusted:'C:\\TrionCAD\\Fonts' });
  FD.summary = function(d){
    var miss = d.styles.filter(function(s){ return s.missing && s.count; });
    var enc = { 'Unicode':0, 'TCVN3':0, 'VNI':0, 'Không rõ':0 };
    d.styles.forEach(function(s){ enc[s.enc] = (enc[s.enc] || 0) + (s.count || 0); });
    return { missing:miss.length, legacy:enc.TCVN3 + enc.VNI, enc:enc, issues:miss.length + enc.TCVN3 + enc.VNI > 0, styles:d.styles.filter(function(s){ return s.count; }).length };
  };
  FD.onOpen = function(d){
    var s = FD.summary(d); if (!s.issues || d.fdMuted || FD.settings.show === 'indicator') return;
    A().banner(d, { id:'fontdoc', icon:'fontdoc', html:'<b>Bác sĩ Font:</b> bản vẽ dùng ' + s.missing + ' font không có trên máy và ' + TC.n(s.enc.TCVN3) + ' đối tượng chữ mã TCVN3' + (s.enc.VNI ? ', ' + TC.n(s.enc.VNI) + ' chữ VNI' : '') + '. TrionCAD đã tự hiển thị đúng.',
      acts:[{ label:'Xem chi tiết', fn:function(){ TC.app.showPalette('fontdoc'); } }, { label:'Chuyển sang Unicode', primary:true, need:'VP', feature:'Chuyển chữ mã cũ sang Unicode', fn:function(){ TC.app.showPalette('fontdoc'); } }, { label:'Không nhắc lại với tệp này', ghost:true, fn:function(){ d.fdMuted = true; A().unbanner(d, 'fontdoc'); } }] });
  };
  var fstate = { tab:'sum', prev:'fixed' };
  function encBar(enc){
    var tot = Object.keys(enc).reduce(function(s, k){ return s + enc[k]; }, 0) || 1, col = { 'Unicode':'var(--c-ok)', 'TCVN3':'var(--c-warn)', 'VNI':'#C79CFF', 'Không rõ':'var(--c-text-3)' };
    return '<div style="display:flex;height:10px;border-radius:2px;overflow:hidden;background:var(--c-active)" role="img" aria-label="Tỉ lệ bảng mã">' + Object.keys(enc).map(function(k){ return enc[k] ? '<i style="width:' + (enc[k] / tot * 100) + '%;background:' + col[k] + '"></i>' : ''; }).join('') + '</div>' +
      '<div style="display:grid;grid-template-columns:1fr 1fr;gap:3px 10px;margin-top:6px;font-size:var(--fs-xs)">' + Object.keys(enc).map(function(k){ return '<span style="display:flex;gap:5px;align-items:center"><i style="width:9px;height:9px;border-radius:2px;background:' + col[k] + '"></i>' + k + '<b class="mono" style="margin-left:auto;font-weight:500">' + TC.n(enc[k]) + '</b></span>'; }).join('') + '</div>';
  }
  function samples(d){
    var out = [], seen = {};
    function walk(list, host){ list.forEach(function(e){ if (out.length > 9) return; if (e.t === 'text' || e.t === 'attdef'){ var st = d.styleMap[e.style]; if (st && (st.enc === 'TCVN3' || st.enc === 'VNI') && !seen[e.s]){ seen[e.s] = 1; out.push({ e:host || e, s:e.t === 'attdef' && host && host.attrs && host.attrs[e.tag] ? host.attrs[e.tag] : e.s, st:st }); } } if (e.t === 'insert' && d.blocks[e.name]) walk(d.blocks[e.name].ents, e); }); }
    walk(d.ents); return out;
  }
  P.fontdoc = { name:'Bác sĩ Font', render:function(host, d){
    if (!d){ host.innerHTML = ''; return; }
    var s = FD.summary(d), canConv = TC.can('VP'), canEdit = TC.can('PRO');
    var head = '<div class="pal-tools" style="padding:0 4px 0 0"><div class="tabs ptabs" role="tablist" aria-label="Khu vực">' + [['sum','Tóm tắt'],['map','Thay thế'],['prev','Xem trước'],['conv','Chuyển']].map(function(t){ return '<button class="tab" role="tab" type="button" data-ft="' + t[0] + '" aria-selected="' + (fstate.tab === t[0]) + '">' + t[1] + '</button>'; }).join('') + '</div><button class="ib" type="button" id="fd-set" aria-label="Thiết lập Bác sĩ Font" data-tip="Thiết lập Bác sĩ Font">' + ic('gear') + '</button></div>';
    var body = '';
    if (!s.issues){ body = '<div class="empty">' + ic('okc') + '<b>Bản vẽ dùng Unicode và đủ font ✓</b>Không có chữ mã cũ, không thiếu font.</div>'; }
    else if (fstate.tab === 'sum'){
      body = '<div class="stack" style="padding:10px">' +
        '<div class="hrow" style="gap:16px"><div><div class="faint" style="font-size:var(--fs-xs)">Font thiếu</div><div style="font-size:var(--fs-xl);font-weight:600">' + s.missing + '</div></div><div><div class="faint" style="font-size:var(--fs-xs)">Kiểu chữ</div><div style="font-size:var(--fs-xl);font-weight:600">' + s.styles + '</div></div><div><div class="faint" style="font-size:var(--fs-xs)">Chữ mã cũ</div><div style="font-size:var(--fs-xl);font-weight:600">' + TC.n(s.legacy) + '</div></div></div>' +
        '<div><div class="gh" style="margin-bottom:6px">Số đối tượng theo bảng mã</div>' + encBar(s.enc) + '</div>' +
        '<div class="note acc">' + ic('okc') + '<span>Độ tin cậy nhận diện: <b>cao</b>. Tên font và đặc trưng âm tiết tiếng Việt khớp ở 98% đối tượng.</span></div>' +
        '<div class="gh">Kiểu chữ</div><div class="tblwrap"><table class="tbl"><colgroup><col><col style="width:62px"><col style="width:46px"></colgroup><thead><tr><th>Kiểu · font</th><th>Bảng mã</th><th class="num">Số</th></tr></thead><tbody>' +
        d.styles.filter(function(x){ return x.count; }).map(function(x){ return '<tr data-style="' + esc(x.name) + '" title="Bấm để tô sáng các đối tượng dùng kiểu này"><td><b style="font-weight:500">' + esc(x.name) + '</b> <span class="faint">' + esc(x.font) + (x.missing ? ' · thiếu' : '') + '</span></td><td><span class="tag ' + (x.enc === 'Unicode' ? 'ok' : x.enc === 'Không rõ' ? '' : 'warn') + '">' + esc(x.enc) + '</span></td><td class="num">' + TC.n(x.count) + '</td></tr>'; }).join('') + '</tbody></table></div></div>';
    } else if (fstate.tab === 'map'){
      body = '<div class="stack" style="padding:10px">' + d.styles.filter(function(x){ return x.missing && x.count; }).map(function(x){
        return '<div class="grp" style="padding:8px 10px;gap:6px"><div class="hrow"><b class="mono" style="font-weight:500">' + esc(x.font) + '</b><span class="tag">' + x.kind + '</span><span class="tag danger">Thiếu</span><span class="tag warn">' + x.enc + '</span></div>' +
          '<div class="frow" style="--lab-w:84px"><span class="lab">Thay bằng</span><button class="dd" type="button" data-sub="' + esc(x.name) + '"><span class="lbl">' + esc(x.sub) + ' <span class="faint">· gợi ý</span></span>' + ic('down', 'xs') + '</button>' +
          '<span class="lab">Áp dụng cho</span><span class="seg"><button type="button" aria-pressed="true">Phiên này</button><button type="button" aria-pressed="false">Mọi bản vẽ</button></span></div></div>';
      }).join('') + '<div class="hrow"><button class="btn sm" type="button" data-io="in">' + ic('open') + 'Nhập bảng…</button><button class="btn sm" type="button" data-io="out">' + ic('saveas') + 'Xuất bảng…</button></div><p class="faint" style="font-size:var(--fs-xs)">Lưu bảng vào hồ sơ người dùng. Nhập/xuất để cả công ty dùng chung một bảng thay thế.</p></div>';
    } else if (fstate.tab === 'prev'){
      body = '<div class="stack" style="padding:10px"><span class="seg" role="group" aria-label="Kiểu hiển thị trên vùng vẽ"><button type="button" data-pm="fixed" aria-pressed="' + (fstate.prev === 'fixed') + '">Hiển thị đúng</button><button type="button" data-pm="raw" aria-pressed="' + (fstate.prev === 'raw') + '">Hiển thị gốc</button></span>' +
        '<p class="faint" style="font-size:var(--fs-xs)">“Hiển thị gốc” là chữ lỗi như AutoCAD thấy khi thiếu font. Áp dụng cho cả vùng vẽ để so sánh.</p>' +
        samples(d).map(function(x, i){ var raw = TC.rawText(d, { s:x.s, style:x.st.name }).replace(/\u00AD/g, '-'); return '<button class="grp" type="button" data-zoom="' + x.e.id + '" style="padding:7px 9px;gap:3px;text-align:left;width:100%"><span class="hrow" style="justify-content:space-between"><span class="tag warn">' + x.st.enc + '</span><span class="faint" style="font-size:var(--fs-xs)">tin cậy ' + (96 - i % 4) + '% · ' + esc(x.st.font) + '</span></span><span class="mono" style="color:var(--c-danger);font-size:var(--fs-s)">' + esc(raw) + '</span><span style="font-family:\'Times New Roman\', serif">' + esc(x.s) + '</span></button>'; }).join('') + '</div>';
    } else {
      var lock = !canConv;
      body = '<div class="stack" style="padding:10px">' + (lock ? '<div class="note">' + ic('lock') + '<span>Chuyển đổi có trong <b>Viewer Pro</b> (lưu bản sao) và <b>Pro</b> (sửa trực tiếp). Viewer vẫn hiển thị đúng chữ.</span></div>' : !canEdit ? '<div class="note">' + ic('info') + '<span>Viewer Pro lưu kết quả thành bản sao, không sửa tệp gốc.</span></div>' : '') +
        '<div class="field"><span class="lab">Phạm vi</span><span class="seg"><button type="button" aria-pressed="true">Cả bản vẽ</button><button type="button" aria-pressed="false">Vùng chọn</button><button type="button" aria-pressed="false">Layout này</button></span></div>' +
        '<div class="stack" style="gap:4px">' + ['Block lồng','Attribute','Chữ kích thước','Bảng','Field'].map(function(x){ return '<label class="chk' + (lock ? ' dis' : '') + '"><input type="checkbox" checked' + (lock ? ' disabled' : '') + '> Gồm ' + x.toLowerCase() + '</label>'; }).join('') + '</div>' +
        '<div class="field"><span class="lab">Bảng mã nguồn</span><select class="inp"' + (lock ? ' disabled' : '') + '><option>Tự nhận diện</option><option>Ép TCVN3</option><option>Ép VNI</option></select></div>' +
        '<div class="frow" style="--lab-w:90px"><span class="lab">TCVN3 →</span><select class="inp"' + (lock ? ' disabled' : '') + '><option>TrionVN-Times</option><option>Times New Roman</option><option>Arial</option></select><span class="lab">VNI →</span><select class="inp"' + (lock ? ' disabled' : '') + '><option>TrionVN-Arial</option><option>Arial</option><option>Times New Roman</option></select></div>' +
        '<label class="chk' + (lock ? ' dis' : '') + '"><input type="checkbox" checked' + (lock ? ' disabled' : '') + '> Cập nhật kiểu chữ sang font Unicode</label><label class="chk' + (lock ? ' dis' : '') + '"><input type="checkbox" checked' + (lock ? ' disabled' : '') + '> Xử lý chữ hoa (.VnTimeH)</label><label class="chk' + (lock ? ' dis' : '') + '"><input type="checkbox" checked' + (lock ? ' disabled' : '') + '> Giữ định dạng MTEXT</label>' +
        '<div class="field"><span class="lab">Đối tượng có độ tin cậy thấp</span><select class="inp"' + (lock ? ' disabled' : '') + '><option>Hỏi từng cái</option><option>Bỏ qua</option><option>Vẫn chuyển</option></select></div>' +
        '<div class="hrow"><button class="btn sm" type="button" data-conv="prev">Xem trước thay đổi</button><button class="btn sm primary" type="button" data-conv="go">' + (canEdit ? 'Chuyển đổi' : 'Chuyển và lưu bản sao…') + (lock ? ' <span class="ed xs" data-ed="vp">VP</span>' : '') + '</button><button class="btn sm ghost" type="button" data-conv="csv">Xuất báo cáo CSV</button></div>' +
        '<button class="btn sm ghost" type="button" data-conv="batch" style="justify-self:start">' + ic('fontbatch') + 'Chuyển hàng loạt nhiều tệp…' + (lock ? ' <span class="ed xs" data-ed="vp">VP</span>' : '') + '</button></div>';
    }
    host.innerHTML = head + '<div class="pal-body">' + body + '</div>';
    host.onclick = function(e){
      var t = e.target.closest('[data-ft]'); if (t){ fstate.tab = t.dataset.ft; P.fontdoc.render(host, d); return; }
      if (e.target.closest('#fd-set')){ TC.screens.fontDoctorSettings(); return; }
      var pm = e.target.closest('[data-pm]'); if (pm){ fstate.prev = pm.dataset.pm; TC.app.view.textMode = fstate.prev; TC.app.view.draw(); P.fontdoc.render(host, d); TC.cmdline.msg(fstate.prev === 'raw' ? 'Vùng vẽ đang hiện chữ gốc (như AutoCAD khi thiếu font).' : 'Vùng vẽ hiện chữ đúng.'); return; }
      var st = e.target.closest('[data-style]'); if (st){ var n = st.dataset.style; d.found = []; d.ents.forEach(function(x){ if ((x.t === 'text' && x.style === n) || (x.t === 'insert' && d.blocks[x.name] && d.blocks[x.name].ents.some(function(b){ return (b.t === 'text' || b.t === 'attdef') && b.style === n; }))) d.found.push(x.id); }); TC.app.view.drawOver(); TC.cmdline.msg('Tô sáng ' + d.found.length + ' đối tượng dùng kiểu chữ ' + esc(n) + '.'); return; }
      var z = e.target.closest('[data-zoom]'); if (z){ var en = d.byId[Number(z.dataset.zoom)]; if (en){ if (d.space !== 'model') TC.app.setSpace('model'); d.found = [en.id]; TC.app.view.zoomTo(en._bb); } return; }
      var sb = e.target.closest('[data-sub]'); if (sb){ var r = sb.getBoundingClientRect(); TC.menu(['TrionVN-Times.shx','TrionVN-Arial.shx','Times New Roman','Arial','simplex.shx'].map(function(f){ return { label:f, act:function(){ d.styleMap[sb.dataset.sub].sub = f; P.fontdoc.render(host, d); } }; }), r.left, r.bottom + 2, { minWidth:r.width }); return; }
      var io = e.target.closest('[data-io]'); if (io){ TC.toast(io.dataset.io === 'in' ? 'Chọn tệp bảng thay thế (.tcfm) để nhập.' : 'Đã xuất bảng thay thế font ra <span class="mono">bang-thay-font.tcfm</span>.', { type:'info', log:false }); return; }
      var cv = e.target.closest('[data-conv]'); if (cv){ if (!canConv){ TC.screens.upgrade('Chuyển chữ mã cũ sang Unicode', 'VP'); return; } var k = cv.dataset.conv; if (k === 'prev') TC.screens.fontPreview(d); else if (k === 'go') TC.screens.fontConvert(d); else if (k === 'batch') TC.cmd.run('FIXFONTBATCH'); else TC.toast('Đã xuất báo cáo <span class="mono">' + esc(d.name.replace(/\.\w+$/, '')) + '_font.csv</span>: ' + FD.summary(d).legacy + ' đối tượng, bảng mã nguồn và font đích.', { act:{ label:'Mở tệp', fn:function(){} }, log:false }); }
    };
  } };

  /* ================= Thiếu tham chiếu UI-B06: banner không chặn ================= */
  TC.missingRefs = { onOpen:function(d){
    var x = d.xrefs.filter(function(r){ return r.status === 'Không tìm thấy'; }).length, im = d.images.filter(function(r){ return r.status === 'Không tìm thấy'; }).length, f = FD.summary(d).missing;
    if (!x && !im) return;
    var parts = []; if (x) parts.push(x + ' xref'); if (f) parts.push(f + ' font'); if (im) parts.push(im + ' ảnh');
    A().banner(d, { id:'missing', type:'warn', icon:'warn', html:'Thiếu ' + parts.join(', ') + '. Phần còn lại của bản vẽ vẫn xem được.', acts:[{ label:'Xem chi tiết', fn:function(){ TC.screens.missingRefs(); } }, { label:'Bỏ qua', ghost:true, fn:function(){ A().unbanner(d, 'missing'); } }] });
  } };

  /* ================= Tham chiếu ngoài (chế độ xem) ================= */
  P.xref = { name:'Tham chiếu', hidden:function(){ return false; }, render:function(host, d){
    if (!d){ host.innerHTML = ''; return; }
    var rows = [{ name:d.name.replace(/\.dwg$/i, ''), type:'Bản vẽ chính', status:'Đã mở', size:TC.size(d.bytes), date:d.modified.split(' ')[0], path:d.path }].concat(d.xrefs, d.images.map(function(i){ return { name:i.name, type:'Ảnh', status:i.status, path:i.path, size:'', date:'' }; }));
    host.innerHTML = '<div class="pal-tools"><button class="btn sm" type="button" data-xa="attach">' + ic('xref') + 'Gắn ▾' + (TC.can('PRO') ? '' : ' <span class="ed xs" data-ed="pro">Pro</span>') + '</button><button class="ib" type="button" data-xa="refresh" aria-label="Làm mới" data-tip="Làm mới">' + ic('refresh') + '</button><span class="grow"></span><button class="ib" type="button" data-xa="path" aria-label="Đổi đường dẫn" data-tip="Đổi đường dẫn" data-need="PRO">' + ic('folder') + '</button></div>' +
      '<div class="pal-body"><table class="tbl"><colgroup><col><col style="width:96px"></colgroup><thead><tr><th>Tên</th><th>Trạng thái</th></tr></thead><tbody>' + rows.map(function(r, i){ return '<tr data-xr="' + i + '"><td>' + ic(r.type === 'Ảnh' ? 'image' : i === 0 ? 'file' : 'xref', 's12') + ' ' + esc(r.name) + '</td><td><span class="tag ' + (r.status === 'Không tìm thấy' ? 'danger' : r.status === 'Đã nạp' ? 'ok' : '') + '">' + r.status + '</span></td></tr>'; }).join('') + '</tbody></table>' +
      '<div class="stack" style="padding:10px;font-size:var(--fs-s)" id="xr-det"><span class="faint">Chọn một tham chiếu để xem chi tiết.</span></div></div>';
    host.onclick = function(e){
      var tr = e.target.closest('[data-xr]'); if (tr){ $$('[data-xr]', host).forEach(function(x){ x.classList.toggle('is-sel', x === tr); }); var r = rows[Number(tr.dataset.xr)]; $('#xr-det', host).innerHTML = '<div class="frow" style="--lab-w:96px"><span class="lab">Kiểu</span><span>' + esc(r.type || 'Attach') + '</span><span class="lab">Đường dẫn lưu</span><span class="mono" style="font-size:var(--fs-xs);word-break:break-all">' + esc(r.path || '') + '</span><span class="lab">Tìm thấy tại</span><span class="mono" style="font-size:var(--fs-xs);word-break:break-all">' + esc(r.found || (r.status === 'Không tìm thấy' ? '—' : r.path)) + '</span><span class="lab">Kích thước</span><span>' + esc(r.size || '—') + '</span><span class="lab">Ngày</span><span>' + esc(r.date || '—') + '</span></div>' + (r.status === 'Không tìm thấy' ? '<div class="hrow"><button class="btn sm" type="button" data-xa="find">' + ic('search') + 'Tìm tệp…</button></div>' : ''); return; }
      var a = e.target.closest('[data-xa]'); if (!a) return;
      if (a.dataset.xa === 'refresh') TC.toast('Đã nạp lại tham chiếu. Không có thay đổi mới.', { log:false });
      else if (a.dataset.xa === 'find') TC.screens.missingRefs();
      else if (!TC.can('PRO')) TC.screens.upgrade('Gắn và sửa đường dẫn tham chiếu ngoài', 'PRO');
    };
  } };

  // palette dock trái của bản Pro (thiết kế ở Đợt 3)
  P.tcvnlib = { name:'Thư viện TCVN', render:function(host){ host.innerHTML = '<div class="empty">' + ic('lib') + '<b>Thư viện TCVN</b>UI-J13 thiết kế ở Đợt 3 (bản biên tập).</div>'; } };
  P.blocks = { name:'Blocks', render:function(host){ host.innerHTML = '<div class="empty">' + ic('blocks') + '<b>Palette Blocks</b>UI-G01 thiết kế ở Đợt 3 (bản biên tập).</div>'; } };
})();
