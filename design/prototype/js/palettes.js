/* TrionCAD prototype · palette: Thuộc tính UI-C01, Layer UI-C03/M04, Bác sĩ Font UI-J01/J02/J04, Tham chiếu ngoài UI-G10 (xem). */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic, G = TC.geo;
  var P = TC.PALETTES = {};
  TC.propsExt = TC.propsExt || {}; // loại đối tượng mới (Đợt 4): t → function(d, e, f) trả các hàng [tên, giá trị, kiểu]
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
  var TYPE = { line:'Line', pline:'Polyline', circle:'Circle', arc:'Arc', ellipse:'Ellipse', text:'Text', mtext:'MText', dim:'Kích thước', dimr:'Kích thước bán kính', dima:'Kích thước góc', insert:'Block', hatch:'Hatch', point:'Point', spline:'Spline', xline:'Xline', ray:'Ray', mleader:'Multileader', image:'Ảnh raster', vpframe:'Viewport', table:'Bảng', attdef:'Attribute definition' };
  TC.TYPE_NAMES = TYPE;
  var pstate = { filter:'all', focus:'Layer', collapsed:{}, vtx:0 };
  function lay(d, e){ return d.layerMap[e.layer] || {}; }
  function colorCell(d, e){ var c = e.color != null ? e.color : null; var lc = lay(d, e).color; var hex = TC.ACI[c != null ? c : lc] || '#FFF'; if ((c != null ? c : lc) === 7) hex = '#FFFFFF'; return '<i class="sw" style="background:' + hex + '"></i>' + (c == null ? 'ByLayer' : TC.aciName(c)); }
  function ltCell(e, d){ var lt = e.lt || 'ByLayer'; var real = e.lt || lay(d, e).lt; return '<span class="lt-s' + (real === 'CENTER' ? ' center' : real === 'DASHED' ? ' dash' : '') + '"></span>' + esc(lt); }
  function common(d, e){
    return [['Màu', colorCell(d, e), 'html'], ['Layer', esc(e.layer)], ['Linetype', ltCell(e, d), 'html'], ['Tỉ lệ linetype', '1.0000', 'num'], ['Plot style', 'ByColor'], ['Độ dày nét', '<span class="lw-s" style="height:' + Math.max(1, Math.round((e.lw || lay(d, e).lw || .25) * 4)) + 'px"></span>' + (e.lw != null ? TC.d(e.lw) + ' mm' : 'ByLayer'), 'html'], ['Độ trong suốt', 'ByLayer'], ['Độ dày đùn', TC.d(e.thick || 0), 'num'], ['Hyperlink', e.link ? esc(e.link.t || e.link.a) : '']];
  }
  function geom(d, e){
    var rows = [], f = function(v, p){ return TC.d(v, p); };
    switch (e.t){
      case 'line': { var dx = e.p2[0] - e.p1[0], dy = e.p2[1] - e.p1[1]; rows = [['Điểm đầu X', f(e.p1[0]), 'num'], ['Điểm đầu Y', f(e.p1[1]), 'num'], ['Điểm đầu Z', f(e.elev || 0), 'num'], ['Điểm cuối X', f(e.p2[0]), 'num'], ['Điểm cuối Y', f(e.p2[1]), 'num'], ['Điểm cuối Z', f(e.elev || 0), 'num'], ['Delta X', f(dx), 'ro'], ['Delta Y', f(dy), 'ro'], ['Delta Z', '0.00', 'ro'], ['Độ dài', f(Math.hypot(dx, dy)), 'ro'], ['Góc', f((Math.atan2(dy, dx) * 180 / Math.PI + 360) % 360, 0) + '°', 'ro']]; break; }
      case 'pline': { var i = Math.min(pstate.vtx, e.pts.length - 1), v = e.pts[i]; rows = [['Đỉnh hiện tại', '<span class="mono">' + (i + 1) + ' / ' + e.pts.length + '</span><span style="margin-left:auto;display:flex;gap:1px"><button class="ib sm" type="button" data-vtx="-1" aria-label="Đỉnh trước">' + ic('left', 'xs') + '</button><button class="ib sm" type="button" data-vtx="1" aria-label="Đỉnh kế">' + ic('right', 'xs') + '</button></span>', 'html'], ['Đỉnh X', f(v[0]), 'num'], ['Đỉnh Y', f(v[1]), 'num'], ['Bề rộng đầu đoạn', '0.00', 'num'], ['Bề rộng cuối đoạn', '0.00', 'num'], ['Bề rộng toàn cục', '0.00', 'num'], ['Cao độ', f(e.elev || 0), 'num'], ['Đóng', e.closed ? 'Có' : 'Không'], ['Linetype liên tục', 'Tắt']];
        if (e.closed) rows.push(['Diện tích', f(G.polyArea(e.pts), 0) + ' (' + f(G.polyArea(e.pts) / 1e6) + ' m²)', 'ro']); rows.push(['Độ dài', f(G.polyLen(e.pts, e.closed)), 'ro']); break; }
      case 'circle': rows = [['Tâm X', f(e.c[0]), 'num'], ['Tâm Y', f(e.c[1]), 'num'], ['Tâm Z', f(e.elev || 0), 'num'], ['Bán kính', f(e.r), 'num'], ['Đường kính', f(e.r * 2), 'num'], ['Chu vi', f(2 * Math.PI * e.r), 'num'], ['Diện tích', f(Math.PI * e.r * e.r, 0), 'num']]; break;
      case 'arc': { var sw = ((e.a1 - e.a0) % 360 + 360) % 360; rows = [['Tâm X', f(e.c[0]), 'num'], ['Tâm Y', f(e.c[1]), 'num'], ['Bán kính', f(e.r), 'num'], ['Góc đầu', f(e.a0, 0) + '°', 'num'], ['Góc cuối', f(e.a1, 0) + '°', 'num'], ['Tổng góc', f(sw, 0) + '°', 'ro'], ['Độ dài cung', f(e.r * sw * Math.PI / 180), 'ro']]; break; }
      case 'text': { var st = d.styleMap[e.style] || {}; rows = [['Nội dung', esc(e.s)], ['Kiểu chữ', esc(e.style)], ['Chú thích theo tỉ lệ', e.anno ? 'Có' : 'Không'], ['Điểm canh', { l:'Trái', c:'Giữa', r:'Phải', m:'Giữa tâm' }[e.al || 'l']], ['Chiều cao', f(e.h), 'num'], ['Góc xoay', f(e.rot || 0, 0) + '°', 'num'], ['Hệ số rộng', f(e.wf || 1, 4), 'num'], ['Góc nghiêng', '0°', 'num'], ['Vị trí X', f(e.p[0]), 'num'], ['Vị trí Y', f(e.p[1]), 'num'], ['Lộn ngược', 'Không'], ['Viết ngược', 'Không'],
        ['Bảng mã', '<span class="tag ' + (st.enc === 'Unicode' ? 'ok' : st.enc === 'Không rõ' ? '' : 'warn') + '">' + esc(st.enc || 'Unicode') + '</span>' + (st.enc === 'TCVN3' || st.enc === 'VNI' ? '<button class="btn sm" type="button" data-conv style="margin-left:auto;height:18px;padding:0 6px">Chuyển sang Unicode' + (TC.can('VP') ? '' : ' <span class="ed xs" data-ed="vp">VP</span>') + '</button>' : ''), 'html']]; break; }
      case 'dim': rows = [['Kiểu kích thước', esc(e.style)], ['Chú thích theo tỉ lệ', e.anno ? 'Có' : 'Không'], ['Giá trị đo', f(e._meas), 'ro'], ['Ghi đè chữ', e.over ? esc(e.over) : '<span class="faint">(không)</span>', 'html'], ['Mũi tên 1', 'Gạch nghiêng kiến trúc'], ['Mũi tên 2', 'Gạch nghiêng kiến trúc'], ['Cỡ mũi tên', '1.50', 'num'], ['Chiều cao chữ', '2.20', 'num'], ['Độ chính xác', '0'], ['Hệ số tỉ lệ đo', '1.0000', 'num']]; break;
      case 'insert': { rows = [['Tên', esc(e.name), 'ro'], ['Vị trí X', f(e.p[0]), 'num'], ['Vị trí Y', f(e.p[1]), 'num'], ['Vị trí Z', '0.00', 'num'], ['Tỉ lệ X', f(e.sx, 4), 'num'], ['Tỉ lệ Y', f(e.sy, 4), 'num'], ['Tỉ lệ Z', f(Math.abs(e.sx), 4), 'num'], ['Góc xoay', f(e.rot || 0, 0) + '°', 'num'], ['Đơn vị block', 'Milimét', 'ro'], ['Chú thích theo tỉ lệ', e.anno ? 'Có' : 'Không']]; break; }
      case 'ellipse': rows = [['Tâm X', f(e.c[0]), 'num'], ['Tâm Y', f(e.c[1]), 'num'], ['Bán trục lớn', f(e.rx), 'num'], ['Bán trục nhỏ', f(e.ry), 'num'], ['Tỉ lệ bán kính', f(e.ry / e.rx, 4), 'ro'], ['Góc xoay', f(e.rot || 0, 0) + '°', 'num'], ['Diện tích', f(Math.PI * e.rx * e.ry, 0), 'ro']]; break;
      case 'spline': rows = [['Phương pháp', 'Điểm đi qua', 'ro'], ['Số điểm đi qua', String(e.pts.length), 'ro'], ['Đóng', e.closed ? 'Có' : 'Không'], ['Độ dài', f(TC.ed ? TC.ed.pathOf(e).L : 0), 'ro']]; break;
      case 'point': rows = [['Vị trí X', f(e.p[0]), 'num'], ['Vị trí Y', f(e.p[1]), 'num'], ['Vị trí Z', '0.00', 'num']]; break;
      case 'xline': case 'ray': rows = [['Điểm gốc X', f(e.p[0]), 'num'], ['Điểm gốc Y', f(e.p[1]), 'num'], ['Vector hướng', f(e.dir[0], 4) + ', ' + f(e.dir[1], 4), 'ro']]; break;
      case 'mtext': { var st2 = d.styleMap[e.style] || {}; rows = [['Nội dung', esc(String(e.s).replace(/\n/g, ' ¶ ')) + ' <button class="ib sm" type="button" data-mte aria-label="Mở trình soạn" style="margin-left:auto">…</button>', 'html'], ['Kiểu chữ', esc(e.style)], ['Chú thích theo tỉ lệ', e.anno ? 'Có' : 'Không'], ['Điểm canh', { l:'Trái', c:'Giữa', r:'Phải' }[e.al || 'l']], ['Chiều cao', f(e.h), 'num'], ['Góc xoay', f(e.rot || 0, 0) + '°', 'num'], ['Hệ số giãn dòng', f((e.ls || 1.6667) / 1.6667, 2) + 'x', 'num'], ['Che nền', e.bg ? 'Có' : 'Không'], ['Chiều rộng khung', f(e.w), 'num'], ['Vị trí X', f(e.p[0]), 'num'], ['Vị trí Y', f(e.p[1]), 'num'], ['Bảng mã', '<span class="tag ' + (st2.enc === 'Unicode' ? 'ok' : 'warn') + '">' + esc(st2.enc || 'Unicode') + '</span>', 'html']]; break; }
      case 'mleader': rows = [['Kiểu multileader', esc(e.mlstyle || 'Standard')], ['Nội dung', esc(e.s || '')], ['Chiều cao', f(e.h), 'num'], ['Số nhánh', String(1 + (e.more || []).length), 'ro'], ['Mũi tên', 'Mũi tên kín']]; break;
      case 'dimr': rows = [['Kiểu kích thước', esc(e.style || 'TCVN-1-100')], ['Giá trị đo', (e.dia ? 'Ø' : 'R') + f(e.dia ? e.r * 2 : e.r), 'ro'], ['Ghi đè chữ', e.over ? esc(e.over) : '<span class="faint">(không)</span>', 'html']]; break;
      case 'dima': rows = [['Kiểu kích thước', esc(e.style || 'TCVN-1-100')], ['Giá trị đo', f(e._meas || 0, 1) + '°', 'ro'], ['Ghi đè chữ', e.over ? esc(e.over) : '<span class="faint">(không)</span>', 'html']]; break;
      case 'image': rows = [['Tên', esc(e.name), 'ro'], ['Vị trí X', f(e.p[0]), 'num'], ['Vị trí Y', f(e.p[1]), 'num'], ['Góc xoay', f(e.rot || 0, 0) + '°', 'num'], ['Rộng', f(e.w), 'num'], ['Cao', f(e.h), 'num'], ['Độ sáng', String(e.bright == null ? 50 : e.bright), 'num'], ['Tương phản', String(e.contrast == null ? 50 : e.contrast), 'num'], ['Mờ', String(e.fade || 0), 'num'], ['Hiện ảnh', e.show === false ? 'Không' : 'Có'], ['Hiện phần clip', e.clip ? 'Có' : 'Không', 'ro']]; break;
      case 'vpframe': { var vp = null; d.layouts.forEach(function(L){ L.vports.forEach(function(q){ if (q.id === e.vp) vp = q; }); }); if (vp) rows = [['Bật', 'Có'], ['Có clip', vp.clip ? 'Có' : 'Không', 'ro'], ['Khóa hiển thị', vp.locked ? 'Có' : 'Không'], ['Tỉ lệ chuẩn', '1:' + Math.round(vp.scale)], ['Tỉ lệ tùy chỉnh', f(1 / vp.scale, 6), 'ro'], ['Tỉ lệ chú thích', '1:' + Math.round(vp.scale), 'ro'], ['Kiểu hiển thị', '2D Wireframe'], ['Tâm X', f(vp.c[0]), 'num'], ['Tâm Y', f(vp.c[1]), 'num'], ['Rộng', f(vp.w), 'num'], ['Cao', f(vp.h), 'num']]; break; }
      case 'table': rows = [['Kiểu bảng', esc(e.tstyle || 'Standard')], ['Số hàng', String(e.rows.length), 'ro'], ['Số cột', String(e.cols.length), 'ro'], ['Rộng', f(e.cols.reduce(function(s, x){ return s + x; }, 0)), 'ro'], ['Cao', f(e.rows.reduce(function(s, x){ return s + x; }, 0)), 'ro'], ['Vị trí X', f(e.p[0]), 'num'], ['Vị trí Y', f(e.p[1]), 'num']]; break;
      case 'attdef': rows = [['Tag', esc(e.tag)], ['Prompt', esc(e.prompt || e.tag)], ['Giá trị mặc định', esc(e.s || '')], ['Kiểu chữ', esc(e.style)], ['Chiều cao', f(e.h), 'num'], ['Vị trí X', f(e.p[0]), 'num'], ['Vị trí Y', f(e.p[1]), 'num']]; break;
      case 'hatch': { var ar = e._p[0].loops.reduce(function(s, l, k){ return s + (k ? -1 : 1) * G.polyArea(l); }, 0); rows = [['Loại', e.pat === 'SOLID' ? 'Tô đặc' : 'Có sẵn'], ['Tên mẫu', e.pat], ['Chú thích theo tỉ lệ', e.anno ? 'Có' : 'Không'], ['Góc', f(e.ang || 0, 0) + '°', 'num'], ['Tỉ lệ', f(e.sc || 1, 4), 'num'], ['Gốc X', '0.00', 'num'], ['Gốc Y', '0.00', 'num'], ['Liên kết', e.assoc ? 'Có' : 'Không'], ['Nhận diện island', 'Thường'], ['Màu nền', 'Không'], ['Diện tích', f(Math.abs(ar), 0) + ' (' + f(Math.abs(ar) / 1e6) + ' m²)', 'ro']]; break; }
      default: if (TC.propsExt[e.t]) rows = TC.propsExt[e.t](d, e, f);
    }
    return rows;
  }
  TC.propRows = function(d, e){ return common(d, e).concat(geom(d, e)); };
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
      var k = r[0], kind = r[2], edk = !ro && kind !== 'ro' && TC.propEditable && TC.propEditable(k, kind), cls = 'pr' + (ro || kind === 'ro' ? ' ro' : '') + (edk ? ' edt' : '') + (pstate.focus === k ? ' is-focus' : '');
      return '<div class="' + cls + '" data-pk="' + esc(k) + '"' + (kind === 'attr' ? ' data-attr="1"' : '') + '><span class="pk">' + esc(k) + '</span><span class="pv' + (kind === 'num' || kind === 'ro' ? ' mono' : '') + '">' + (kind === 'html' ? r[1] : '<span>' + r[1] + '</span>') + (edk && TC.propIsMenu(k) ? '<i class="pdd">' + ic('down', 'xs') + '</i>' : '') + (kind !== 'html' ? '<button class="ib sm copy" type="button" data-copyv aria-label="Sao chép giá trị">' + ic('copyclip', 's12') + '</button>' : '') + '</span></div>';
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
      var cu = TC.ed && !viewer ? TC.ed.cur(d) : {}, cl = TC.ed && !viewer ? TC.ed.clayer(d) : 'KT-TUONG';
      html += groupHTML('Chung', [['Màu', (TC.screens.colorSw ? TC.screens.colorSw(cu.color, cl) + TC.screens.colorLabel(cu.color) : '<i class="sw" style="background:#FFF"></i>ByLayer'), 'html'], ['Layer', esc(cl)], ['Linetype', '<span class="lt-s"></span>' + esc(cu.lt || 'ByLayer'), 'html'], ['Tỉ lệ linetype', TC.d(cu.lts || 1, 4), 'num'], ['Độ dày nét', '<span class="lw-s" style="height:' + Math.max(1, Math.round((cu.lw || .25) * 4)) + 'px"></span>' + (cu.lw != null ? TC.d(cu.lw) + ' mm' : 'ByLayer'), 'html'], ['Độ trong suốt', cu.transp ? String(cu.transp) : 'ByLayer'], ['Độ dày đùn', '0.00', 'num']], viewer) +
        (v ? groupHTML('View', [['Tâm X', TC.d(v.cx), 'ro'], ['Tâm Y', TC.d(v.cy), 'ro'], ['Tâm Z', '0.00', 'ro'], ['Cao', TC.d(H / v.s), 'ro'], ['Rộng', TC.d(W / v.s), 'ro']], true) : '') +
        groupHTML('Khác', [['Tỉ lệ chú thích', '1:100'], ['Biểu tượng UCS bật', 'Có'], ['Biểu tượng UCS tại gốc', 'Không'], ['UCS theo viewport', 'Có'], ['Tên UCS', 'Thế giới'], ['Kiểu hiển thị', '2D Wireframe']], viewer);
    } else if (shown.length === 1 || (pstate.filter === 'all' && tkeys.length === 1 && shown.length > 1)){
      var e = shown[0], multi = shown.length > 1;
      var vary = function(rows){ if (!multi) return rows; return rows.map(function(r){ var same = shown.every(function(x){ var rr = (r[0] === 'Layer' ? [['Layer', esc(x.layer)]] : common(d, x).concat(geom(d, x))).filter(function(q){ return q[0] === r[0]; })[0]; return rr && rr[1] === r[1]; }); return same ? r : [r[0], '<span class="varies">*Khác nhau*</span>', 'html']; }); };
      html += groupHTML('Chung', vary(common(d, e)), viewer);
      var g = geom(d, e); if (g.length) html += groupHTML(e.t === 'text' ? 'Chữ' : e.t === 'dim' ? 'Kích thước' : e.t === 'insert' ? 'Hình học' : e.t === 'hatch' ? 'Mẫu' : 'Hình học', vary(g), viewer);
      var at = attrRows(d, e); if (at.length && !multi) html += groupHTML('Attribute', at.map(function(r){ return [r[0], r[1], 'attr']; }), viewer);
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
    host.innerHTML = html; pstate.shown = shown; pstate.host = host; pstate.d = d;
    $('#pp-type', host).addEventListener('click', function(){ var r = this.getBoundingClientRect(); var it = [{ label:'Tất cả (' + TC.n(sel.length) + ')', check:pstate.filter === 'all', act:function(){ pstate.filter = 'all'; P.props.render(host, d); } }].concat(tkeys.map(function(t){ return { label:TYPE[t] + ' (' + types[t] + ')', check:pstate.filter === t, act:function(){ pstate.filter = t; P.props.render(host, d); } }; })); if (!sel.length) it = [{ label:'Không chọn', check:true }]; TC.menu(it, r.left, r.bottom + 2, { minWidth:r.width }); });
    $('#pp-pick', host).addEventListener('click', function(){ $('#cv').focus(); TC.cmdline.msg('Chọn đối tượng trên bản vẽ; Shift + bấm để bớt.'); });
    $('#pp-qs', host).addEventListener('click', function(){ if (viewer) TC.screens.upgrade('Chọn nhanh', 'PRO'); else TC.cmd.run('QSELECT'); });
    host.onclick = function(ev){
      var g2 = ev.target.closest('[data-pg]'); if (g2){ pstate.collapsed[g2.dataset.pg] = !pstate.collapsed[g2.dataset.pg]; P.props.render(host, d); return; }
      var vt = ev.target.closest('[data-vtx]'); if (vt){ var pl = shown[0]; pstate.vtx = (pstate.vtx + Number(vt.dataset.vtx) + pl.pts.length) % pl.pts.length; TC.app.view.temp = [{ k:'pt', p:pl.pts[pstate.vtx], color:TC.cssv('--c-grip-hot') }]; TC.app.view.drawOver(); P.props.render(host, d); return; }
      if (ev.target.closest('[data-mte]') && !viewer && TC.cmd){ TC.cmd.run('TEXTEDIT', shown[0]); return; }
      var er = ev.target.closest('.pr.edt'); if (er && !viewer && !ev.target.closest('[data-copyv]') && TC.propEdit){ pstate.focus = er.dataset.pk; TC.propEdit(er, shown, d, host); return; }
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
    var list = d.layers.filter(function(l){ if (lstate.filter === 'used' && !l.count) return false; if (TC.layerFilterTest && !TC.layerFilterTest(d, l)) return false; return !re || re.test(l.name) || TC.fold(l.name).indexOf(TC.fold(q)) >= 0; });
    list.sort(function(a, b){ var k = lstate.sort; var x = a[k], y = b[k]; return (typeof x === 'number' ? x - y : String(x).localeCompare(String(y))) * lstate.dir; });
    return list;
  }
  function tgl(d, l, k, onIc, offIc, name, need){
    var v = L.get(d, l.name, k), dis = need && !TC.can(need), isOn = k === 'frz' ? !v : k === 'lck' ? !v : v;
    var temp = d.tempLayers[l.name] && d.tempLayers[l.name][k] != null;
    return '<button class="tgl' + (isOn ? '' : ' off') + '" type="button" data-lt="' + k + '" data-ln="' + esc(l.name) + '"' + (dis ? ' disabled aria-disabled="true"' : '') + ' aria-pressed="' + !!v + '" aria-label="' + name + ' ' + esc(l.name) + '" data-tip="' + name + (temp ? ' · đã đổi tạm thời' : '') + (dis ? ' · có trong TrionCAD Pro' : '') + '">' + ic(isOn ? onIc : offIc) + (temp ? '<span class="sr">đã đổi</span>' : '') + '</button>';
  }
  function vpTgl(d, l){ var L = d.layouts[d.space], vp = L && d.activeVp && L.vports.filter(function(q){ return q.id === d.activeVp; })[0], on = vp && (vp.frz || []).indexOf(l.name) >= 0; return '<button class="tgl' + (on ? ' off' : '') + '" type="button" data-lt="vpf" data-ln="' + esc(l.name) + '"' + (vp ? '' : ' disabled') + ' aria-label="Đóng băng trong viewport ' + esc(l.name) + '" data-tip="' + (vp ? 'Đóng băng trong viewport hiện hành' : 'Kích hoạt một viewport để dùng cột này') + '">' + ic(on ? 'snow' : 'sun') + '</button>'; }
  function swatch(c){ var hex = TC.ACI[c] || '#FFF'; return '<i class="sw" style="background:' + (c === 7 ? '#FFFFFF' : hex) + '"></i>'; }
  function layerTable(d, full){
    var rows = layerRows(d), viewer = TC.isViewer();
    var th = function(k, label, cls){ return '<th class="sortable ' + (cls || '') + '" data-sort="' + k + '" aria-sort="' + (lstate.sort === k ? (lstate.dir > 0 ? 'ascending' : 'descending') : 'none') + '">' + label + (lstate.sort === k ? (lstate.dir > 0 ? ' ▲' : ' ▼') : '') + '</th>'; };
    var cols = full ? '<colgroup><col style="width:34px"><col style="width:170px"><col style="width:34px"><col style="width:34px"><col style="width:34px"><col style="width:34px"><col style="width:84px"><col style="width:100px"><col style="width:90px"><col style="width:70px"><col style="width:70px"><col><col style="width:80px"></colgroup>'
      : '<colgroup><col style="width:26px"><col><col style="width:28px"><col style="width:28px"><col style="width:28px"><col style="width:26px"><col style="width:52px"></colgroup>';
    var vpc = full && d.space !== 'model' && !viewer;
    if (vpc) cols = cols.replace('<col style="width:84px">', '<col style="width:48px"><col style="width:56px"><col style="width:84px">');
    var head = full ? '<tr><th>TT</th>' + th('name', 'Tên') + '<th class="ctr">Bật</th><th class="ctr">Băng</th><th class="ctr">Khóa</th><th class="ctr">In</th>' + (vpc ? '<th class="ctr" title="Đóng băng trong viewport hiện hành">Băng VP</th><th class="ctr" title="Đóng băng ở viewport mới">VP mới</th>' : '') + th('color', 'Màu') + th('lt', 'Linetype') + th('lw', 'Độ dày nét', 'num') + '<th class="num">Trong suốt</th><th>Plot style</th><th>Mô tả</th>' + th('count', 'Số đối tượng', 'num') + '</tr>'
      : '<tr><th></th>' + th('name', 'Tên') + '<th class="ctr" title="Bật/Tắt">' + ic('bulb', 's12') + '</th><th class="ctr" title="Đóng băng">' + ic('snow', 's12') + '</th><th class="ctr" title="Khóa">' + ic('lock', 's12') + '</th><th class="ctr" title="Màu">' + ic('plotstyle', 's12') + '</th>' + th('count', 'Số', 'num') + '</tr>';
    var body = rows.map(function(l){
      var cur = l.name === (TC.ed && !viewer ? TC.ed.clayer(d) : 'KT-TUONG'), st = cur ? '<span class="tag acc" title="Layer hiện hành" style="height:16px;padding:0 4px">' + ic('check', 's12') + '</span>' : l.count ? '<span title="Đang dùng" style="color:var(--c-text-2)">●</span>' : '<span title="Không có đối tượng" class="faint">○</span>';
      return '<tr class="' + (lstate.sel === l.name ? 'is-sel' : '') + '" data-lrow="' + esc(l.name) + '"' + (full && !viewer ? ' draggable="true"' : '') + '><td class="ctr">' + st + '</td><td data-lname="' + esc(l.name) + '">' + esc(l.name) + (d.tempLayers[l.name] && Object.keys(d.tempLayers[l.name]).length ? ' <span class="tag info" style="height:15px;font-size:9.5px">tạm</span>' : '') + '</td>' +
        '<td class="ctr">' + tgl(d, l, 'on', 'bulb', 'bulboff', 'Bật/Tắt') + '</td><td class="ctr">' + tgl(d, l, 'frz', 'sun', 'snow', 'Đóng băng') + '</td><td class="ctr">' + tgl(d, l, 'lck', 'unlock', 'lock', 'Khóa', 'PRO') + '</td>' +
        (full ? '<td class="ctr">' + tgl(d, l, 'plot', 'print', 'x', 'In', 'PRO') + '</td>' + (d.space !== 'model' && !viewer ? '<td class="ctr">' + vpTgl(d, l) + '</td><td class="ctr">' + tgl(d, l, 'vpfrz', 'snow', 'sun', 'Đóng băng ở viewport mới', 'PRO') + '</td>' : '') + '<td data-lcol="' + esc(l.name) + '">' + swatch(l.color) + ' ' + (l.color <= 7 ? TC.aciName(l.color) : l.color) + '</td><td data-llt="' + esc(l.name) + '"><span class="lt-s' + (l.lt === 'CENTER' ? ' center' : /DASH|HIDDEN/.test(l.lt) ? ' dash' : '') + '"></span> ' + esc(l.lt) + '</td><td class="num" data-llw="' + esc(l.name) + '">' + TC.d(l.lw) + ' mm</td><td class="num">0</td><td>Color_' + l.color + '</td><td data-ldesc="' + esc(l.name) + '">' + esc(l.desc || '') + '</td>' : '<td class="ctr" data-lcol="' + esc(l.name) + '">' + swatch(l.color) + '</td>') +
        '<td class="num">' + TC.n(l.count) + '</td></tr>';
    }).join('');
    return { html:'<table class="tbl">' + cols + '<thead>' + head + '</thead><tbody>' + body + '</tbody></table>', n:rows.length };
  }
  function bindLayerTable(root, d, rerender){
    root.addEventListener('click', function(e){
      var t = e.target.closest('[data-lt]'); if (t){ e.stopPropagation(); if (t.disabled) { if (t.dataset.lt !== 'vpf') TC.screens.upgrade('Khóa layer, đổi thuộc tính layer', 'PRO'); return; } var n = t.dataset.ln, k = t.dataset.lt; if (!TC.isViewer() && TC.layerEdit){ TC.layerEdit(d, n, k, null, rerender); return; } L.set(d, n, k, !L.get(d, n, k)); A().view.draw(); rerender(); TC.emit('layers'); return; }
      var pc = e.target.closest('[data-lcol], [data-llt], [data-llw]'); if (pc){ if (TC.isViewer()){ TC.screens.upgrade('Đổi màu, linetype, độ dày nét của layer', 'PRO'); return; } if (TC.layerProp) TC.layerProp(d, pc, rerender); return; }
      var s = e.target.closest('[data-sort]'); if (s){ if (lstate.sort === s.dataset.sort) lstate.dir *= -1; else { lstate.sort = s.dataset.sort; lstate.dir = 1; } rerender(); return; }
      var r = e.target.closest('[data-lrow]'); if (r){ lstate.sel = r.dataset.lrow; rerender(); }
    });
    root.addEventListener('dblclick', function(e){ var r = e.target.closest('[data-lrow]'); if (!r) return; if (TC.isViewer()){ TC.toast('Viewer không đổi layer hiện hành.', { type:'info', log:false }); return; } var nm = e.target.closest('[data-lname]'), ds = e.target.closest('[data-ldesc]'); if ((nm || ds) && TC.layerRename){ TC.layerRename(d, nm || ds, rerender, !!ds); return; } if (TC.ed.setCurLayer(r.dataset.lrow, d)){ rerender(); TC.cmdline.msg('Layer hiện hành: <b>' + esc(r.dataset.lrow) + '</b>.'); } });
    root.addEventListener('keydown', function(e){ if (e.key === 'F2' && !TC.isViewer() && lstate.sel && TC.layerRename){ var c = $('[data-lname="' + lstate.sel.replace(/"/g, '') + '"]', root); if (c){ e.preventDefault(); TC.layerRename(d, c, rerender); } } });
    root.addEventListener('contextmenu', function(e){
      var r = e.target.closest('[data-lrow]'); if (!r) return; e.preventDefault(); var n = r.dataset.lrow; lstate.sel = n; rerender();
      var LT0 = function(k){ return function(){ if (TC.layerTool) TC.layerTool(k, d, rerender); }; };
      TC.menu([{ label:'Đặt hiện hành', need:'PRO', act:function(){ TC.ed.setCurLayer(n, d); rerender(); } }, { label:'Layer mới', need:'PRO', act:LT0('new') }, { label:'Đổi tên', need:'PRO', kbd:'F2', act:function(){ var c = $('[data-lname="' + n.replace(/"/g, '') + '"]', root); if (c && TC.layerRename) TC.layerRename(d, c, rerender); } }, { label:'Xóa', need:'PRO', act:LT0('trash') }, { label:'Sửa mô tả', need:'PRO', act:function(){ var c = $('[data-ldesc="' + n.replace(/"/g, '') + '"]', root); if (c && TC.layerRename) TC.layerRename(d, c, rerender, true); else TC.screens.ask('Mô tả layer', 'Mô tả', d.layerMap[n].desc || '', null, 'UI-C03').then(function(v){ if (v != null){ d.layerMap[n].desc = v; rerender(); } }); } }, { sep:1 },
        { label:'Chọn đối tượng trên layer này', icon:'select', act:function(){ d.sel = d.ents.filter(function(x){ return x.layer === n && TC.entVisible(d, x); }).map(function(x){ return x.id; }); TC.cad.afterSel(); TC.cmdline.msg('Đã chọn ' + d.sel.length + ' đối tượng trên layer ' + esc(n) + '.'); } },
        { label:'Cô lập layer này', icon:'isolate', act:function(){ L.isolate(d, [n]); rerender(); } }, { label:'Gộp layer này vào…', need:'PRO', act:LT0('merge') }, { sep:1 }, { label:'Lưu trạng thái layer…', need:'PRO', act:function(){ TC.screens.layerStateNew(d); } }, { label:'Trạng thái layer…', need:'PRO', act:function(){ TC.screens.layerStates(); } }, { label:'Khôi phục như tệp gốc', icon:'undo', dis:!L.changed(d), act:function(){ L.restore(d); } }], e.clientX, e.clientY);
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
  /* ================= sửa thuộc tính (UI-C01 bản Pro) ================= */
  var MENUK = { 'Màu':1, 'Layer':1, 'Linetype':1, 'Độ dày nét':1, 'Kiểu chữ':1, 'Kiểu kích thước':1, 'Kiểu multileader':1, 'Điểm canh':1, 'Đóng':1, 'Tên mẫu':1, 'Loại':1, 'Che nền':1, 'Hiện ảnh':1, 'Khóa hiển thị':1, 'Tỉ lệ chuẩn':1, 'Tên':1, 'Chú thích theo tỉ lệ':1, 'Kiểu bảng':1 };
  var NUMK = { 'Hệ số rộng':'wf', 'Điểm đầu X':['p1', 0], 'Điểm đầu Y':['p1', 1], 'Điểm cuối X':['p2', 0], 'Điểm cuối Y':['p2', 1], 'Đỉnh X':['V', 0], 'Đỉnh Y':['V', 1], 'Tâm X':['c', 0], 'Tâm Y':['c', 1], 'Vị trí X':['p', 0], 'Vị trí Y':['p', 1], 'Điểm gốc X':['p', 0], 'Điểm gốc Y':['p', 1], 'Bán kính':'r', 'Đường kính':'D', 'Chu vi':'C', 'Diện tích':'A', 'Chiều cao':'h', 'Góc xoay':'rot', 'Tỉ lệ X':'sx', 'Tỉ lệ Y':'sy', 'Tỉ lệ':'sc', 'Góc':'ang', 'Góc đầu':'a0', 'Góc cuối':'a1', 'Bề rộng toàn cục':'w', 'Bán trục lớn':'rx', 'Bán trục nhỏ':'ry', 'Chiều rộng khung':'w', 'Hệ số giãn dòng':'LS', 'Rộng':'w', 'Cao':'h', 'Độ sáng':'bright', 'Tương phản':'contrast', 'Mờ':'fade', 'Tỉ lệ linetype':'lts' };
  var TXTK = { 'Nội dung':'s', 'Ghi đè chữ':'over', 'Tag':'tag', 'Prompt':'prompt', 'Giá trị mặc định':'s' };
  TC.propEditable = function(k, kind){ return !!(MENUK[k] || (NUMK[k] && kind !== 'html') || TXTK[k] || kind === 'attr'); };
  TC.propIsMenu = function(k){ return !!MENUK[k]; };
  function vpOf(d, e){ var f = null; d.layouts.forEach(function(L){ L.vports.forEach(function(q){ if (q.id === e.vp) f = q; }); }); return f; }
  function setNum(e, k, v){ var m = NUMK[k]; if (Array.isArray(m)){ if (m[0] === 'V'){ var i = Math.min(pstate.vtx, e.pts.length - 1); e.pts[i] = e.pts[i].slice(); e.pts[i][m[1]] = v; } else if (e[m[0]]){ e[m[0]] = e[m[0]].slice(); e[m[0]][m[1]] = v; } return; } if (m === 'D') e.r = v / 2; else if (m === 'C') e.r = v / (2 * Math.PI); else if (m === 'A'){ if (e.t === 'circle') e.r = Math.sqrt(v / Math.PI); } else if (m === 'LS') e.ls = 1.6667 * v; else e[m] = v; }
  TC.propEdit = function(row, shown, d, host){
    var k = row.dataset.pk, none = !shown.length, ED = TC.ed, rr = function(){ P.props.render(host, d); };
    var apply = function(fn, label){ if (none){ fn(ED.cur(d)); rr(); A().renderRibbon(); return; } ED.run(label || 'Sửa ' + k.toLowerCase(), function(){ shown.forEach(function(e){ if (e.t === 'vpframe'){ var vp = vpOf(d, e); if (!vp) return; var done = ED.state(function(){ return vp; }, function(s2){ Object.assign(vp, s2); TC.prepEnt(d, e); A().view.draw(); }); fn(vp); TC.prepEnt(d, e); done(); } else ED.mod(e, fn); }); }); A().view.draw(); rr(); };
    if (MENUK[k]) return propMenu(k, row, shown, d, apply, none);
    var pv = $('.pv', row), raw = ($('.pv > span', row) || pv).textContent.trim(), isAng = /Góc|xoay/.test(k), val = row.dataset.attr ? raw : TXTK[k] ? (k === 'Ghi đè chữ' && raw === '(không)' ? '' : raw.replace(/ ¶ /g, '\n')) : raw.replace(/[°x]$/, '').replace(/ \(.*\)$/, '');
    var inp = TC.el('<input class="pinp' + (NUMK[k] ? ' mono' : '') + '" spellcheck="false">'); inp.value = val; pv.innerHTML = ''; pv.appendChild(inp); inp.focus(); inp.select();
    var done = false, fin = function(ok){ if (done) return; done = true; if (!ok){ rr(); return; } var v = inp.value;
      if (row.dataset.attr) return apply(function(x){ x.attrs = Object.assign({}, x.attrs || {}); x.attrs[k] = v; }, 'Sửa attribute');
      if (TXTK[k]) return apply(function(x){ var f = TXTK[k]; if (f === 'over'){ if (!v.trim() || v === '<>') delete x.over; else x.over = v; } else if (x.t === 'mtext' && f === 's') x.s = v.replace(/ ¶ /g, '\n'); else x[f] = v; });
      var n = TC.parseNum(v, isAng ? 'deg' : /Tỉ lệ|sáng|phản|Mờ|giãn/.test(k) ? 'none' : 'len'); if (n.err){ TC.ctip($('#cv'), 40, 40, n.err, true); done = false; inp.classList.add('err'); inp.focus(); return; }
      if (/^(Bán kính|Đường kính|Chu vi|Chiều cao|Rộng|Cao|Bán trục|Tỉ lệ$|Chiều rộng khung)/.test(k) && n.v <= 0){ inp.classList.add('err'); done = false; TC.ctip($('#cv'), 40, 40, k + ' phải lớn hơn 0'); return; }
      apply(function(x){ setNum(x, k, n.v); }); };
    inp.addEventListener('keydown', function(e){ e.stopPropagation(); if (e.key === 'Enter'){ e.preventDefault(); fin(true); } else if (e.key === 'Escape'){ e.preventDefault(); fin(false); } });
    inp.addEventListener('blur', function(){ setTimeout(function(){ fin(true); }, 0); });
  };
  function propMenu(k, row, shown, d, apply, none){
    var r = $('.pv', row).getBoundingClientRect(), ED = TC.ed, SC = TC.screens, e0 = shown[0] || {}, it = [];
    var pick = function(fn){ return function(){ apply(fn); }; };
    if (k === 'Màu') it = [{ label:'ByLayer', act:pick(function(x){ if (none) x.color = null; else delete x.color; }) }, { label:'ByBlock', act:pick(function(x){ x.color = 0; }) }, { sep:1 }].concat([1, 2, 3, 4, 5, 6, 7, 8, 9].map(function(n){ return { label:'<i class="sw" style="background:' + (n === 7 ? '#FFFFFF' : TC.ACI[n]) + '"></i>' + TC.aciName(n), act:pick(function(x){ x.color = n; }) }; })).concat([{ sep:1 }, { label:'Chọn màu…', icon:'plotstyle', act:function(){ SC.colorPicker(none ? ED.cur(d).color : e0.color).then(function(c){ if (c !== undefined) apply(function(x){ if (c == null && !none) delete x.color; else x.color = c; }); }); } }]);
    else if (k === 'Layer') it = d.layers.map(function(l){ return { label:'<i class="sw" style="background:' + (l.color === 7 ? '#FFFFFF' : TC.ACI[l.color]) + '"></i>' + esc(l.name) + (l.lck ? ' <span class="faint">(khóa)</span>' : ''), check:none ? l.name === ED.clayer(d) : e0.layer === l.name, act:function(){ if (none){ ED.setCurLayer(l.name, d); P.props.render(pstate.host, d); } else apply(function(x){ x.layer = l.name; }, 'Đổi layer'); } }; });
    else if (k === 'Linetype') it = [{ label:'ByLayer', act:pick(function(x){ if (none) x.lt = null; else delete x.lt; }) }, { label:'ByBlock', act:pick(function(x){ x.lt = 'ByBlock'; }) }, { sep:1 }].concat(SC.ltList(d).map(function(n){ return { label:esc(n), act:pick(function(x){ x.lt = n; }) }; })).concat([{ sep:1 }, { label:'Khác…', act:function(){ TC.cmd.run('LINETYPE'); } }]);
    else if (k === 'Độ dày nét') it = [{ label:'ByLayer', act:pick(function(x){ if (none) x.lw = null; else delete x.lw; }) }, { sep:1 }].concat(SC.LWS.filter(function(w){ return w; }).map(function(w){ return { label:'<span class="lw-s" style="height:' + Math.max(1, Math.round(w * 4)) + 'px;width:26px;display:inline-block;margin-right:6px"></span>' + TC.d(w) + ' mm', act:pick(function(x){ x.lw = w; }) }; }));
    else if (k === 'Kiểu chữ') it = d.styles.map(function(s){ return { label:esc(s.name), check:e0.style === s.name, act:pick(function(x){ x.style = s.name; }) }; }).concat([{ sep:1 }, { label:'Quản lý kiểu chữ…', act:function(){ TC.cmd.run('STYLE'); } }]);
    else if (k === 'Kiểu kích thước') it = d.dimstyles.map(function(n){ return { label:esc(n), check:e0.style === n, act:pick(function(x){ x.style = n; }) }; }).concat([{ sep:1 }, { label:'Quản lý kiểu kích thước…', act:function(){ TC.cmd.run('DIMSTYLE'); } }]);
    else if (k === 'Kiểu bảng') it = TC.screens.tableStyleNames(d).map(function(n){ return { label:esc(n), check:(e0.tstyle || 'Standard') === n, act:pick(function(x){ x.tstyle = n; }) }; }).concat([{ sep:1 }, { label:'Quản lý kiểu bảng…', act:function(){ TC.cmd.run('TABLESTYLE'); } }]);
    else if (k === 'Kiểu multileader') it = Object.keys(d.mlStyles || { Standard:1 }).map(function(n){ return { label:esc(n), act:pick(function(x){ x.mlstyle = n; }) }; });
    else if (k === 'Điểm canh') it = (e0.t === 'mtext' ? [['l', 'Trái'], ['c', 'Giữa'], ['r', 'Phải']] : [['l', 'Trái'], ['c', 'Giữa'], ['r', 'Phải'], ['m', 'Giữa tâm']]).map(function(a){ return { label:a[1], check:(e0.al || 'l') === a[0], act:pick(function(x){ x.al = a[0]; }) }; });
    else if (k === 'Đóng' || k === 'Che nền' || k === 'Hiện ảnh' || k === 'Khóa hiển thị' || k === 'Chú thích theo tỉ lệ'){ var f = { 'Đóng':'closed', 'Che nền':'bg', 'Hiện ảnh':'show', 'Khóa hiển thị':'locked', 'Chú thích theo tỉ lệ':'anno' }[k]; it = [['Có', true], ['Không', false]].map(function(o){ return { label:o[0], act:pick(function(x){ x[f] = o[1]; }) }; }); }
    else if (k === 'Tỉ lệ chuẩn') it = [20, 25, 50, 75, 100, 200, 500].map(function(n){ return { label:'1:' + n, act:pick(function(x){ x.scale = n; }) }; });
    else if (k === 'Tên mẫu') it = (SC.PATS || []).map(function(p2){ return { label:esc(p2[0]) + ' <span class="faint">· ' + esc(p2[2]) + '</span>', check:e0.pat === p2[0], act:pick(function(x){ x.pat = p2[0]; }) }; });
    else if (k === 'Loại') it = [['Có sẵn', 'ANSI31'], ['Tô đặc', 'SOLID']].map(function(o){ return { label:o[0], act:pick(function(x){ x.pat = o[1] === 'SOLID' ? 'SOLID' : (x.pat === 'SOLID' ? 'ANSI31' : x.pat); }) }; });
    else if (k === 'Tên') it = Object.keys(d.blocks).filter(function(n){ return !/^\*|^A\$/.test(n); }).map(function(n){ return { label:esc(n), check:e0.name === n, act:pick(function(x){ x.name = n; }) }; });
    TC.menu(it, r.left, r.bottom + 1, { minWidth:Math.max(160, r.width) });
  }

  /* ================= Layer bản Pro (UI-C03): sửa có hoàn tác ================= */
  function layerUndo(d, label, fn){ var ED = TC.ed; ED.run(label, function(){ var done = ED.state(function(){ return { L:d.layers.map(function(l){ return Object.assign({}, l); }), cl:d.clayer }; }, function(s2){ d.layers = s2.L.map(function(l){ return Object.assign({}, l); }); d.layerMap = {}; d.layers.forEach(function(l){ d.layerMap[l.name] = l; }); d.clayer = s2.cl; A().view.draw(); A().refreshPalettes(); A().renderRibbon(); }); fn(); done(); }); A().view.draw(); A().refreshPalettes(); A().renderRibbon(); TC.emit('layers'); }
  TC.layerEdit = function(d, n, k, v, rer){
    if (k === 'vpf'){ var L2 = d.layouts[d.space], vp = L2 && L2.vports.filter(function(q){ return q.id === d.activeVp; })[0]; if (!vp) return; vp.frz = vp.frz || []; var i = vp.frz.indexOf(n); if (i >= 0) vp.frz.splice(i, 1); else vp.frz.push(n); A().view.draw(); rer(); return; }
    var l = d.layerMap[n]; if (k === 'frz' && n === TC.ed.clayer(d) && !l.frz){ TC.toast('Không đóng băng được layer hiện hành <b>' + esc(n) + '</b>. Đặt layer khác hiện hành trước.', { type:'warn', log:false }); return; }
    layerUndo(d, 'Layer ' + n, function(){ TC.ed.layerSnap(d); l[k] = v == null ? !l[k] : v; }); rer();
  };
  TC.layerProp = function(d, cell, rer){
    var n = cell.dataset.lcol || cell.dataset.llt || cell.dataset.llw, l = d.layerMap[n], r = cell.getBoundingClientRect(), SC = TC.screens;
    if (cell.dataset.lcol) return SC.colorPicker(l.color).then(function(c){ if (c !== undefined && c != null && c !== 0) layerUndo(d, 'Màu layer', function(){ l.color = c; }), rer(); });
    if (cell.dataset.llt) return TC.menu(SC.ltList(d).map(function(t){ return { label:esc(t), check:l.lt === t, act:function(){ layerUndo(d, 'Linetype layer', function(){ l.lt = t; }); rer(); } }; }).concat([{ sep:1 }, { label:'Tải linetype…', act:function(){ TC.cmd.run('LINETYPE'); } }]), r.left, r.bottom + 1, { minWidth:r.width });
    TC.menu(SC.LWS.filter(function(w){ return w; }).map(function(w){ return { label:TC.d(w) + ' mm', check:l.lw === w, act:function(){ layerUndo(d, 'Độ dày layer', function(){ l.lw = w; }); rer(); } }; }), r.left, r.bottom + 1, { minWidth:r.width });
  };
  TC.layerRename = function(d, cell, rer, desc){
    var n = cell.dataset.lname || cell.dataset.ldesc, l = d.layerMap[n]; if (!desc && (n === '0' || n === 'Defpoints')){ TC.toast('Không đổi tên được layer <b>' + esc(n) + '</b>.', { type:'warn', log:false }); return; }
    var inp = TC.el('<input class="pinp" spellcheck="false">'); inp.value = desc ? (l.desc || '') : n; cell.innerHTML = ''; cell.appendChild(inp); inp.focus(); inp.select();
    var done = false, fin = function(ok){ if (done) return; done = true; var v = inp.value.trim(); if (!ok || (!desc && (!v || v === n))) return rer();
      if (!desc && (d.layerMap[v] || /[<>\/\\":;?*|,=`]/.test(v))){ TC.toast(d.layerMap[v] ? 'Đã có layer “' + esc(v) + '”.' : 'Tên layer không chứa < > / \\ " : ; ? * | , = `', { type:'warn', log:false }); return rer(); }
      if (desc){ layerUndo(d, 'Mô tả layer', function(){ l.desc = v; }); return rer(); }
      TC.ed.run('Đổi tên layer', function(){ layerUndo(d, 'Đổi tên layer', function(){ l.name = v; delete d.layerMap[n]; d.layerMap[v] = l; if (d.clayer === n) d.clayer = v; }); d.ents.concat.apply(d.ents, d.layouts.map(function(L){ return L.ents; })).forEach(function(e){ if (e.layer === n) TC.ed.mod(e, function(x){ x.layer = v; }); }); });
      TC.lstate.sel = v; rer(); };
    inp.addEventListener('keydown', function(e){ e.stopPropagation(); if (e.key === 'Enter'){ e.preventDefault(); fin(true); } else if (e.key === 'Escape'){ e.preventDefault(); fin(false); } });
    inp.addEventListener('blur', function(){ setTimeout(function(){ fin(true); }, 0); });
    inp.addEventListener('click', function(e){ e.stopPropagation(); });
  };
  TC.layerTool = function(k, d, rer, btn){
    var ls = TC.lstate, sel = ls.sel && d.layerMap[ls.sel], SC = TC.screens;
    if (k === 'refresh'){ rer(); return; }
    if (k === 'new' || k === 'snow'){ var i = 1; while (d.layerMap['Layer' + i]) i++; var base = sel || d.layerMap[TC.ed.clayer(d)], nl = { name:'Layer' + i, color:base.color, lt:base.lt, lw:base.lw, desc:'', on:true, frz:false, lck:false, plot:true, vpfrz:k === 'snow', count:0 }; layerUndo(d, 'Layer mới', function(){ d.layers.splice(d.layers.indexOf(base) + 1, 0, nl); d.layerMap[nl.name] = nl; }); ls.sel = nl.name; ls.q = ''; rer(); setTimeout(function(){ var c = document.querySelector('[data-lname="' + nl.name + '"]'); if (c) TC.layerRename(d, c, rer); }, 0); return; }
    if (k === 'laycur'){ if (sel){ TC.ed.setCurLayer(sel.name, d); rer(); } else TC.toast('Chọn một layer trong danh sách trước.', { type:'info', log:false }); return; }
    if (k === 'trash'){ if (!sel){ TC.toast('Chọn layer cần xóa.', { type:'info', log:false }); return; } var why = sel.name === '0' ? 'Layer 0 luôn có trong bản vẽ.' : sel.name === 'Defpoints' ? 'Defpoints là layer hệ thống của kích thước.' : sel.name === TC.ed.clayer(d) ? 'Đây là layer hiện hành.' : sel.count ? 'Layer đang có ' + TC.n(sel.count) + ' đối tượng.' : null;
      if (why){ TC.confirm({ title:'Không xóa được layer', msg:'Không xóa được <b>' + esc(sel.name) + '</b>. ' + why, detail:sel.count ? 'Gộp vào layer khác: chuyển mọi đối tượng sang layer đích rồi xóa layer này.' : '', ok:sel.count ? 'Gộp vào layer khác…' : 'Đã hiểu', cancel:'Đóng' }).then(function(y){ if (y && sel.count) TC.layerTool('merge', d, rer); }); return; }
      layerUndo(d, 'Xóa layer', function(){ d.layers = d.layers.filter(function(l){ return l !== sel; }); delete d.layerMap[sel.name]; }); ls.sel = null; rer(); TC.cmdline.msg('Đã xóa layer ' + esc(sel.name) + '.'); return; }
    if (k === 'merge'){ if (!sel) return; SC.ask('Gộp layer ' + sel.name + ' vào', 'Layer đích', TC.ed.clayer(d) === sel.name ? '0' : TC.ed.clayer(d), function(v){ return !d.layerMap[v] ? 'Không có layer “' + v + '”.' : v === sel.name ? 'Chọn layer khác.' : null; }, 'UI-C03').then(function(v){ if (!v) return; var n0 = 0; TC.ed.run('Gộp layer', function(){ d.ents.concat.apply(d.ents, d.layouts.map(function(L){ return L.ents; })).forEach(function(e){ if (e.layer === sel.name){ TC.ed.mod(e, function(x){ x.layer = v; }); n0++; } }); layerUndo(d, 'Gộp layer', function(){ d.layers = d.layers.filter(function(l){ return l !== sel; }); delete d.layerMap[sel.name]; }); }); ls.sel = v; rer(); TC.toast('Đã gộp ' + esc(sel.name) + ' vào <b>' + esc(v) + '</b>: ' + n0 + ' đối tượng.', { log:false }); }); return; }
    if (k === 'lib'){ A().showPalette('tcvnlib'); TC.toast('Chọn “Bộ layer theo bộ môn” trong Thư viện TCVN để nạp.', { type:'info', log:false }); return; }
    if (k === 'layers'){ SC.layerStates(); return; }
    if (k === 'filter'){ SC.layerFilterEdit(d, null, rer); return; }
    if (k === 'gear'){ var dlg = TC.dialog({ id:'layset', title:'Thiết lập layer', code:'UI-C03', w:480, body:'<div class="stack"><div class="grp"><div class="gh">Cô lập layer (LAYISO)</div><label class="chk"><input type="radio" name="lis" checked> Khóa và làm mờ các layer khác</label><div class="hrow" style="padding-left:20px">Độ mờ <input type="range" min="0" max="90" value="50" aria-label="Độ mờ"> <span class="mono">50%</span></div><label class="chk"><input type="radio" name="lis"> Tắt các layer khác</label><div class="gh">Trong layout</div><label class="chk"><input type="radio" name="lil" checked> Đóng băng ở viewport hiện tại</label><label class="chk"><input type="radio" name="lil"> Tắt</label></div><label class="chk"><input type="checkbox" checked> Áp bộ lọc đang chọn lên dropdown layer trên ribbon</label><label class="chk"><input type="checkbox" checked> Tô nền khác cho ô ghi đè theo viewport</label><label class="chk"><input type="checkbox" disabled> Báo khi bản vẽ có layer mới (sau v1.0)</label></div>', foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" data-x="close">Lưu thiết lập</button>' }); return; }
  };
})();
