/* TrionCAD prototype · dữ liệu ribbon (Viewer UI-M01, Pro UI-A05), ô Tìm lệnh UI-L12, menu công cụ prototype. */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic;
  function A(){ return TC.app; } function V(){ return TC.app.view; }
  var zoomMenu = function(){ return [
    { label:'Toàn bộ đối tượng', icon:'zext', kbd:'Z E', act:function(){ V().zoomExtents(); } }, { label:'Cửa sổ', icon:'zwin', kbd:'Z W', act:function(){ TC.cmd.run('ZOOM', 'W'); } }, { label:'Trước đó', icon:'zprev', kbd:'Z P', act:function(){ V().zoomPrev(); } },
    { label:'Thời gian thực', icon:'zoom', act:function(){ TC.cmd.run('ZOOM', 'R'); } }, { label:'Tất cả', act:function(){ V().zoomExtents(); } }, { label:'Tỉ lệ…', act:function(){ TC.cmd.run('ZOOM'); } }, { label:'Đối tượng', act:function(){ TC.cmd.run('ZOOM', 'O'); } },
    { sep:1 }, { label:'Phóng to', icon:'zoom', act:function(){ V().zoomAt(V().W / 2, V().H / 2, 2); } }, { label:'Thu nhỏ', icon:'zoomout', act:function(){ V().zoomAt(V().W / 2, V().H / 2, .5); } } ]; };
  var viewsMenu = function(){ var d = A().doc(); return (d && d.views.length ? d.views.map(function(v){ return { label:esc(v.name), icon:'views', act:function(){ if (d.space !== 'model') A().setSpace('model'); var vv = V(); vv.pushHist(); d.view = { cx:v.c[0], cy:v.c[1], s:vv.H / v.h }; vv.draw(); TC.emit('view'); } }; }) : [{ label:'Bản vẽ chưa có view đã đặt tên', dis:true }]).concat([{ sep:1 }, { label:'Trình quản lý view…', need:'PRO' }]); };
  var layoutMenu = function(){ var d = A().doc(); if (!d) return []; return [{ label:'Model', check:d.space === 'model', act:function(){ A().setSpace('model'); } }].concat(d.layouts.map(function(l, i){ return { label:esc(l.name), check:d.space === i, act:function(){ A().setSpace(i); } }; })); };
  function curLayerHTML(){ return TC.ic('bulb') + TC.ic('sun') + '<i class="sw" style="background:#FFFFFF"></i>'; }
  var layerDD = { t:'dd', id:'layerdd', label:'Layer hiện hành', w:220, html:curLayerHTML, text:function(){ return 'KT-TUONG'; }, desc:'Danh sách layer: bật/tắt, đóng băng ngay trong danh sách. Trong Viewer chỉ đổi tạm thời.', act:function(b){ TC.screens.layerDrop(b); } };
  var vstyleDD = { t:'dd', id:'vstyle', label:'Kiểu hiển thị', w:130, text:function(){ return '2D Wireframe'; }, act:function(b){ var r = b.getBoundingClientRect(); TC.menu(['2D Wireframe','Wireframe','Hidden','Shaded','Realistic'].map(function(x, i){ return { label:x, check:i === 0, act:function(){ if (i) TC.toast('Bản vẽ chỉ có đối tượng 2D; kiểu hiển thị 3D không đổi hình.', { type:'info', log:false }); } }; }), r.left, r.bottom + 2, { minWidth:r.width }); } };
  var lw = { t:'s', id:'lwtgl', label:'Độ dày nét', icon:'lw', cmd:'LWDISPLAY', toggle:true, on:function(){ return TC.S.toggles.lw; }, act:function(){ A().toggle('lw'); A().renderRibbon(); } };

  TC.RIBBON_VIEWER = { upsell:true, tabs:[
    { name:'Xem', panels:[
      { name:'Điều hướng', icon:'pan', prio:1, items:[
        { t:'lg', label:'Pan', icon:'pan', cmd:'PAN', desc:'Kéo màn hình. Giữ chuột giữa cũng là pan.' },
        { t:'lg', label:'Zoom', icon:'zoom', cmd:'ZOOM', menu:zoomMenu, desc:'Bấm phần trên để zoom thời gian thực, phần dưới để chọn kiểu zoom.' },
        { t:'col', items:[ { t:'s', label:'Toàn bộ', icon:'zext', cmd:'ZOOM', key:'Nhấp đúp chuột giữa', act:function(){ V().zoomExtents(); } }, { t:'s', label:'View đã lưu', icon:'views', cmd:'VIEW', menu:viewsMenu, menuOnly:true }, { t:'s', label:'Tạo lại hình', icon:'regen', cmd:'REGEN' } ] } ] },
      { name:'Layout', icon:'layout', prio:3, items:[ { t:'lg', label:'Chuyển\nlayout', icon:'layout', id:'lay-switch', menu:layoutMenu, menuOnly:true, desc:'Chuyển giữa Model và các layout.' },
        { t:'col', items:[ { t:'s', label:'Layout trước', icon:'left', key:'Ctrl+PgUp', act:function(){ swapLayout(-1); } }, { t:'s', label:'Layout kế', icon:'right', key:'Ctrl+PgDn', act:function(){ swapLayout(1); } } ] } ] },
      { name:'Layer', icon:'layers', prio:1, items:[ { t:'lg', label:'Quản lý\nlayer', icon:'layers', cmd:'LAYER', act:function(){ TC.screens.layerManager(); }, desc:'Danh sách layer ở chế độ xem: bật/tắt, đóng băng tạm thời.' },
        { t:'col', gap:3, items:[ layerDD, { t:'row', items:[ { t:'io', label:'Cô lập layer', icon:'isolate', cmd:'LAYISO' }, { t:'io', label:'Bỏ cô lập', icon:'unisolate', cmd:'LAYUNISO' }, { t:'io', label:'Tắt layer', icon:'bulboff', cmd:'LAYOFF' }, { t:'io', label:'Đóng băng layer', icon:'snow', cmd:'LAYFRZ' }, { t:'io', label:'Bật tất cả', icon:'bulb', cmd:'LAYON' }, { t:'io', label:'Rã đông tất cả', icon:'sun', cmd:'LAYTHW' }, { t:'io', label:'Khôi phục như tệp gốc', icon:'undo', id:'lay-restore', act:function(){ TC.layers.restore(A().doc()); } } ] } ] } ] },
      { name:'Hiển thị', icon:'vstyle', prio:3, items:[ { t:'col', gap:3, items:[ vstyleDD, lw ] } ] },
      { name:'Palette', icon:'props', prio:2, items:[ { t:'col', items:[ { t:'s', label:'Thuộc tính', icon:'props', cmd:'PROPERTIES', key:'Ctrl+1' }, { t:'s', label:'Layer', icon:'layers', id:'pal-layers', act:function(){ A().showPalette('layers', true); } }, { t:'s', label:'Bác sĩ Font', icon:'fontdoc', cmd:'FIXFONT' } ] }, { t:'col', items:[ { t:'s', label:'Tham chiếu ngoài', icon:'xref', cmd:'EXTERNALREFERENCES' }, { t:'s', label:'Đếm', icon:'count', cmd:'COUNT', need:'VP' }, { t:'s', label:'Markup', icon:'markup', id:'pal-markup', need:'VP', act:function(){ A().showPalette('markup', true); } } ] }, { t:'col', items:[ { t:'s', label:'Clean Screen', icon:'fullscreen', cmd:'CLEANSCREENON', key:'Ctrl+0' } ] } ] } ] },
    { name:'Đo', panels:[
      { name:'Đo', icon:'measure', prio:1, items:[ { t:'lg', label:'Đo nhanh', icon:'mquick', cmd:'MEASUREGEOM', act:function(){ TC.cmd.run('MEASUREGEOM', 'Q'); }, desc:'Rê chuột để thấy khoảng cách quanh con trỏ; bấm trong vùng kín để có diện tích.' }, { t:'lg', label:'Khoảng\ncách', icon:'dist', cmd:'DIST' }, { t:'lg', label:'Diện tích', icon:'area', cmd:'AREA' },
        { t:'col', items:[ { t:'s', label:'Góc', icon:'angle', cmd:'MEASUREGEOM', id:'m-ang', act:function(){ TC.cmd.run('MEASUREGEOM', 'A'); } }, { t:'s', label:'Bán kính', icon:'radius', cmd:'MEASUREGEOM', id:'m-rad', act:function(){ TC.cmd.run('MEASUREGEOM', 'R'); } }, { t:'s', label:'Tọa độ điểm', icon:'idpoint', cmd:'ID' } ] } ] },
      { name:'Thông tin', icon:'list', prio:2, items:[ { t:'lg', label:'Liệt kê', icon:'list', cmd:'LIST' }, { t:'col', items:[ { t:'s', label:'Cửa sổ văn bản', icon:'list', cmd:'TEXTSCR', key:'F2' }, { t:'s', label:'Tìm chữ', icon:'findtext', cmd:'FIND' } ] } ] },
      { name:'Tổng hợp', icon:'sigma', prio:2, items:[ { t:'lg', label:'Tổng\nchiều dài', icon:'sigma', cmd:'TCLENGTH', need:'VP' }, { t:'lg', label:'Tổng\ndiện tích', icon:'area', cmd:'TCAREA', need:'VP' }, { t:'col', items:[ { t:'s', label:'Đếm block', icon:'count', cmd:'COUNT', need:'VP' }, { t:'s', label:'Xuất ra Excel', icon:'stats', cmd:'TCATTOUT', need:'VP' } ] } ] } ] },
    { name:'Ghi chú', need:'VP', panels:[
      { name:'Đánh dấu', icon:'markup', prio:1, items:[ MK('lg', 'Đám mây', 'cloud', 'cloud', 'Đám mây sửa đổi quanh vùng cần chú ý; thêm ghi chú ngay sau khi vẽ.'),
        { t:'col', items:[ MK('s', 'Chữ nhật', 'rect', 'rect'), MK('s', 'Ellipse', 'ellipse', 'ellipse'), MK('s', 'Mũi tên', 'arrow', 'arrow') ] },
        { t:'col', items:[ MK('s', 'Đường thẳng', 'line', 'line'), MK('s', 'Nét tự do', 'freehand', 'free', 'Kéo chuột để vẽ.'), MK('s', 'Bút dạ', 'highlight', 'hl', 'Tô nổi như bút dạ quang, nét dày trong suốt.') ] } ] },
      { name:'Chữ', icon:'textnote', prio:1, items:[ MK('lg', 'Chữ\nghi chú', 'textnote', 'text'), { t:'col', items:[ MK('s', 'Ghi chú có đường dẫn', 'leader', 'leader'), MK('s', 'Dấu tick', 'tick', 'tick') ] } ] },
      { name:'Đo', icon:'dist', prio:2, items:[ { t:'col', items:[ MK('s', 'Khoảng cách', 'dist', 'dist', 'Đo và lưu kết quả thành markup.'), MK('s', 'Diện tích', 'area', 'area', 'Đo vùng nhiều điểm và lưu thành markup.') ] } ] },
      { name:'Duyệt', icon:'stamp', prio:2, items:[ { t:'lg', label:'Con dấu', icon:'stamp', cmd:'TCMARKUP', id:'mk-stamp', need:'VP', desc:'Đã duyệt, Cần sửa, Không duyệt; kèm tên người duyệt và ngày.', act:function(b){ var r = b.getBoundingClientRect(); TC.menu(['ok','fix','no'].map(function(k){ return { label:TC.markup.STAMPS[k][0].charAt(0) + TC.markup.STAMPS[k][0].slice(1).toLowerCase(), check:TC.markup.stamp === k, act:function(){ TC.cmd.run('TCMARKUP', 'stamp:' + k); } }; }), r.left, r.bottom + 2, { returnFocus:b }); } }, MK('lg', 'Chụp\nvùng', 'snapshot', 'snap', 'Chụp một vùng màn hình vào danh sách markup và clipboard.') ] },
      { name:'Thuộc tính', icon:'plotstyle', prio:3, items:[ { t:'col', gap:3, items:[
        { t:'dd', id:'mk-color', label:'Màu markup', w:118, need:'VP', html:function(){ return '<i class="sw" style="background:' + TC.markup.props.color + '"></i>'; }, text:function(){ return (TC.markup.MCOL.filter(function(c){ return c[1] === TC.markup.props.color; })[0] || ['Màu'])[0]; }, act:function(b){ mkProp(b, TC.markup.MCOL.map(function(c){ return { label:'<i class="sw" style="background:' + c[1] + '"></i> ' + c[0], check:TC.markup.props.color === c[1], v:c[1] }; }), 'color'); } },
        { t:'dd', id:'mk-w', label:'Độ dày nét', w:118, need:'VP', html:function(){ return '<span class="lw-s" style="height:' + TC.markup.props.w + 'px"></span>'; }, text:function(){ return TC.markup.props.w + ' px'; }, act:function(b){ mkProp(b, [1, 2, 3, 5].map(function(w){ return { label:w + ' px', check:TC.markup.props.w === w, v:w }; }), 'w'); } },
        { t:'dd', id:'mk-a', label:'Độ mờ', w:118, need:'VP', text:function(){ return 'Độ mờ ' + TC.markup.props.alpha + '%'; }, act:function(b){ mkProp(b, [100, 75, 50, 25].map(function(a){ return { label:a + '%', check:TC.markup.props.alpha === a, v:a }; }), 'alpha'); } } ] },
        { t:'col', gap:3, items:[ { t:'dd', id:'mk-size', label:'Cỡ chữ', w:96, need:'VP', text:function(){ return 'Chữ ' + TC.markup.props.size + ' px'; }, act:function(b){ mkProp(b, [11, 14, 18, 24].map(function(z){ return { label:z + ' px (theo zoom lúc tạo)', check:TC.markup.props.size === z, v:z }; }), 'size'); } } ] } ] },
      { name:'Markup', icon:'list', prio:1, items:[ { t:'lg', label:'Danh sách\nmarkup', icon:'list', need:'VP', id:'mk-list', cmd:'TCMARKUP', act:function(){ A().showPalette('markup', true); } },
        { t:'col', items:[ { t:'s', label:'Nhập markup…', icon:'open', need:'VP', id:'mk-import', act:function(){ A().showPalette('markup'); setTimeout(function(){ var m = $('#pal-right [data-ma="more"]'); if (m) m.click(); }, 0); } }, { t:'s', label:'Xuất PDF kèm markup', icon:'pdf', need:'VP', id:'mk-pdf', act:function(){ A().showPalette('markup'); var b2 = $('#pal-right [data-ma="pdf"]'); if (b2) b2.click(); } }, { t:'s', label:'Đưa vào bản vẽ', icon:'insert', need:'PRO', id:'mk-todwg', feature:'Đưa markup vào bản vẽ (layer TC-MARKUP)' } ] } ] } ] },
    { name:'In & Xuất', panels:[
      { name:'In', icon:'print', prio:1, items:[ { t:'lg', label:'In', icon:'print', cmd:'PLOT', key:'Ctrl+P' }, { t:'lg', label:'Xem trước', icon:'preview', cmd:'PREVIEW' },
        { t:'col', items:[ { t:'s', label:'Thiết lập trang', icon:'print', cmd:'PAGESETUP' }, { t:'s', label:'In nhiều layout', icon:'publish', cmd:'PUBLISH', need:'VP' }, { t:'s', label:'In hàng loạt theo khung tên', icon:'batchplot', cmd:'TCBATCHPLOT', need:'VP' } ] } ] },
      { name:'Xuất', icon:'pdf', prio:1, items:[ { t:'lg', label:'PDF', icon:'pdf', cmd:'EXPORTPDF', menu:function(){ return [{ label:'Layout hiện tại', act:function(){ TC.screens.exportPdf('cur'); } }, { label:'Tất cả layout', act:function(){ TC.screens.exportPdf('all'); } }, { label:'Vùng đang xem', act:function(){ TC.screens.exportPdf('view'); } }, { label:'Cửa sổ…', act:function(){ TC.screens.exportPdf('win'); } }]; } }, { t:'lg', label:'Ảnh', icon:'image', cmd:'PNGOUT' } ] } ] },
    { name:'Công cụ', panels:[
      { name:'Bản vẽ', icon:'compare', prio:1, items:[ { t:'lg', label:'So sánh', icon:'compare', cmd:'COMPARE', need:'VP' }, { t:'lg', label:'Chuyển font\nhàng loạt', icon:'fontbatch', cmd:'FIXFONTBATCH', need:'VP' },
        { t:'col', items:[ { t:'s', label:'Đếm block', icon:'count', cmd:'COUNT', need:'VP' }, { t:'s', label:'Xuất bảng thống kê', icon:'stats', cmd:'TCATTOUT', need:'VP' }, { t:'s', label:'Tìm chữ', icon:'findtext', cmd:'FIND' } ] } ] },
      { name:'Hệ thống', icon:'gpu', prio:2, items:[ { t:'col', items:[ { t:'s', label:'Thiết lập đồ họa', icon:'gpu', cmd:'GRAPHICSCONFIG' }, { t:'s', label:'Kiểm tra hiệu năng', icon:'gauge', cmd:'TRIONBENCH' }, { t:'s', label:'Gửi bản vẽ lỗi', icon:'send', cmd:'TCREPORT' } ] } ] } ] }
  ] };
  // nút công cụ markup: chạy TCMARKUP với công cụ tương ứng (UI-M02)
  function MK(t, label, icon, tool, desc){ return { t:t, label:label, icon:icon, cmd:'TCMARKUP', id:'mk-' + tool, need:'VP', desc:desc, act:function(){ TC.cmd.run('TCMARKUP', tool); } }; }
  function mkProp(b, items, key){ var r = b.getBoundingClientRect(); TC.menu(items.map(function(it){ return { label:it.label, check:it.check, act:function(){ TC.markup.props[key] = it.v; TC.save('mkprops', TC.markup.props); A().renderRibbon(); } }; }), r.left, r.bottom + 2, { minWidth:r.width, returnFocus:b }); }
  function swapLayout(dir){ var d = A().doc(); if (!d) return; var sp = ['model'].concat(d.layouts.map(function(l, i){ return i; })); var j = sp.indexOf(d.space); A().setSpace(sp[TC.clamp(j + dir, 0, sp.length - 1)]); }

  // Bản Pro: tab Trang chủ đầy đủ theo 07.1 §7.2; nội dung từng lệnh thiết kế ở Đợt 3
  var S = function(label, icon, cmd, menu){ return { t:'s', label:label, icon:icon, cmd:cmd, menu:menu ? function(){ return [{ label:'Các kiểu ' + label.toLowerCase() + ' (Đợt 3)', dis:true }]; } : null }; };
  var Lg = function(label, icon, cmd, menu){ return { t:'lg', label:label, icon:icon, cmd:cmd, menu:menu ? function(){ return [{ label:'Các kiểu (Đợt 3)', dis:true }]; } : null }; };
  var IO = function(label, icon, cmd){ return { t:'io', label:label, icon:icon, cmd:cmd }; };
  TC.RIBBON_PRO = { tabs:[
    { name:'Trang chủ', panels:[
      { name:'Vẽ', icon:'line', prio:1, more:1, items:[ Lg('Đoạn\nthẳng','line','LINE'), Lg('Polyline','pline','PLINE'), Lg('Đường\ntròn','circle','CIRCLE',1), Lg('Cung','arc','ARC',1), { t:'col', items:[S('Chữ nhật','rect','RECTANG',1), S('Hatch','hatch','HATCH',1), S('Ellipse','ellipse','ELLIPSE',1)] }, { t:'col', items:[IO('Spline','spline','SPLINE'), IO('Đường dựng hình','xline','XLINE'), IO('Đám mây sửa đổi','cloud','REVCLOUD')] } ] },
      { name:'Sửa', icon:'move', prio:1, more:1, items:[ { t:'col', items:[S('Di chuyển','move','MOVE'), S('Sao chép','copy','COPY'), S('Kéo giãn','stretch','STRETCH')] }, { t:'col', items:[S('Xoay','rotate','ROTATE'), S('Đối xứng','mirror','MIRROR'), S('Tỉ lệ','scale','SCALE')] }, { t:'col', items:[S('Cắt','trim','TRIM',1), S('Bo góc','fillet','FILLET',1), S('Mảng','array','ARRAY',1)] }, { t:'col', items:[IO('Xóa','erase','ERASE'), IO('Phá khối','explode','EXPLODE'), IO('Offset','offset','OFFSET')] } ] },
      { name:'Chú thích', icon:'mtext', prio:2, more:1, items:[ Lg('Văn bản','mtext','MTEXT',1), Lg('Kích thước','dim','DIM',1), { t:'col', items:[S('Đường dẫn','leader','MLEADER',1), S('Bảng','table','TABLE')] } ] },
      { name:'Layer', icon:'layers', prio:1, more:1, items:[ { t:'lg', label:'Thuộc tính\nlayer', icon:'layers', cmd:'LAYER', act:function(){ TC.screens.layerManager(); } }, { t:'col', gap:3, items:[ layerDD, { t:'row', items:[IO('Cô lập','isolate','LAYISO'), IO('Bỏ cô lập','unisolate','LAYUNISO'), IO('Tắt','bulboff','LAYOFF'), IO('Đóng băng','snow','LAYFRZ'), IO('Khóa','lock','LAYLCK'), IO('Đặt hiện hành','laycur','LAYMCUR'), IO('Khớp layer','laymatch','LAYMCH')] } ] } ] },
      { name:'Block', icon:'insert', prio:3, more:1, items:[ Lg('Chèn','insert','INSERT',1), { t:'col', items:[S('Tạo','bcreate','BLOCK'), S('Sửa','bedit','BEDIT'), S('Attribute','attedit','ATTEDIT',1)] } ] },
      { name:'Thuộc tính', icon:'match', prio:2, more:1, items:[ Lg('Khớp\nthuộc tính','match','MATCHPROP'), { t:'col', gap:3, items:[ { t:'dd', label:'Màu', w:132, html:function(){ return '<i class="sw" style="background:#FFF"></i>'; }, text:function(){ return 'ByLayer'; } }, { t:'dd', label:'Linetype', w:132, html:function(){ return '<span class="lt-s"></span>'; }, text:function(){ return 'ByLayer'; } }, { t:'dd', label:'Độ dày nét', w:132, html:function(){ return '<span class="lw-s" style="height:3px"></span>'; }, text:function(){ return 'ByLayer'; } } ] } ] },
      { name:'Nhóm', icon:'group', prio:4, items:[ Lg('Nhóm','group','GROUP'), { t:'col', items:[IO('Bỏ nhóm','ungroup','UNGROUP'), IO('Sửa nhóm','bedit','GROUPEDIT'), IO('Chọn theo nhóm','select','PICKSTYLE')] } ] },
      { name:'Tiện ích', icon:'measure', prio:3, items:[ { t:'lg', label:'Đo', icon:'measure', cmd:'MEASUREGEOM', menu:function(){ return [{ label:'Nhanh', act:function(){ TC.cmd.run('MEASUREGEOM','Q'); } }, { label:'Khoảng cách', act:function(){ TC.cmd.run('DIST'); } }, { label:'Bán kính', act:function(){ TC.cmd.run('MEASUREGEOM','R'); } }, { label:'Góc', act:function(){ TC.cmd.run('MEASUREGEOM','A'); } }, { label:'Diện tích', act:function(){ TC.cmd.run('AREA'); } }]; } }, { t:'col', items:[S('Chọn nhanh','qselect','QSELECT'), S('Chọn tất cả','selall','SELECTALL'), S('Tọa độ điểm','idpoint','ID')] } ] },
      { name:'Clipboard', icon:'paste', prio:4, items:[ Lg('Dán','paste','PASTECLIP',1), { t:'col', items:[IO('Cắt','cut','CUTCLIP'), IO('Sao chép','copyclip','COPYCLIP'), IO('Sao chép có điểm gốc','copybase','COPYBASE')] } ] } ] },
    { name:'Chèn', panels:[] }, { name:'Chú thích', panels:[] }, { name:'Hiển thị', panels:[] }, { name:'Quản lý', panels:[] }, { name:'Xuất', panels:[] }, { name:'Công cụ VN', panels:[] }, { name:'Mở rộng', panels:[] }
  ] };
  TC.RIBBON_PRO.tabs.slice(1).forEach(function(t){ t.panels = [{ name:t.name, icon:'more', prio:1, items:[{ t:'lg', label:'Thiết kế ở\nĐợt 3', icon:'info', id:'later-' + t.name, act:function(){ TC.toast('Tab “' + t.name + '” (07.1 §7) thiết kế ở Đợt 3 cùng bản biên tập.', { type:'info', log:false }); } }] }]; });
  TC.panelMore = function(name){ return [{ h:name + ' mở rộng' }, { label:'Nội dung panel mở rộng thiết kế ở Đợt 3', dis:true }, { sep:1 }, { label:'Ghim panel', dis:true }]; };

  /* ================= ô Tìm lệnh UI-L12 ================= */
  TC.search = { bind:function(inp){
    var box = null, hot = 0, items = [];
    function close(){ if (box) { box.remove(); box = null; } }
    function hl(s, q){ var F = TC.fold(q); if (!F) return esc(s); var fs = TC.fold(s), i = fs.indexOf(F); return i < 0 ? esc(s) : esc(s.slice(0, i)) + '<mark>' + esc(s.slice(i, i + q.length)) + '</mark>' + esc(s.slice(i + q.length)); }
    function render(){
      var q = inp.value.trim(); close(); if (!q) return;
      var res = TC.matchCmds(q).slice(0, 6), help = TC.HELP_TOPICS.filter(function(h){ return TC.fold(h.t + ' ' + h.k).indexOf(TC.fold(q)) >= 0; }).slice(0, 3), sets = TC.SETTINGS_INDEX.filter(function(s){ return TC.fold(s[0] + ' ' + s[1]).indexOf(TC.fold(q)) >= 0; }).slice(0, 3);
      items = [];
      var html = '';
      if (res.length) html += '<div class="sh">Lệnh</div>' + res.map(function(r){ var c = r.c, lock = !TC.can(c.need); items.push({ run:c.name }); return '<button class="si" type="button" data-i="' + (items.length - 1) + '">' + ic(c.icon) + '<span class="n">' + hl(c.desc, q) + '</span><span class="c">' + c.name + (c.aliases[0] && c.aliases[0] !== c.name ? ' · ' + c.aliases[0] : '') + '</span><span class="w">' + esc(c.where) + (lock ? ' · có trong ' + (c.need === 'VP' ? 'Viewer Pro' : 'Pro') : '') + '</span></button>'; }).join('');
      if (help.length) html += '<div class="sh">Bài trợ giúp</div>' + help.map(function(h){ items.push({ help:h.id }); return '<button class="si" type="button" data-i="' + (items.length - 1) + '">' + ic('book') + '<span class="n">' + hl(h.t, q) + '</span><span class="c"></span><span class="w">' + esc(h.k) + '</span></button>'; }).join('');
      if (sets.length) html += '<div class="sh">Thiết lập</div>' + sets.map(function(s){ items.push({ set:s }); return '<button class="si" type="button" data-i="' + (items.length - 1) + '">' + ic('gear') + '<span class="n">' + hl(s[0], q) + '</span><span class="c"></span><span class="w">Tùy chọn › ' + esc(s[2]) + '</span></button>'; }).join('');
      if (!items.length) html = '<div class="empty" style="padding:14px">Không tìm thấy “' + esc(q) + '”. Thử từ khóa khác, ví dụ “đo”, “in”, “layer”.</div>';
      box = TC.el('<div class="sres" role="listbox">' + html + '</div>'); document.body.appendChild(box);
      var r = inp.closest('.tb-search').getBoundingClientRect(); box.style.left = Math.max(8, r.right - 440) + 'px'; box.style.top = (r.bottom + 4) + 'px';
      hot = 0; paint();
      box.addEventListener('mousedown', function(e){ var b = e.target.closest('[data-i]'); if (!b) return; e.preventDefault(); pick(items[Number(b.dataset.i)]); });
    }
    function paint(){ if (!box) return; $$('.si', box).forEach(function(b, i){ b.classList.toggle('is-hot', i === hot); }); }
    function pick(it){ if (!it) return; close(); inp.value = ''; inp.blur(); if (it.run) TC.cmd.run(it.run); else if (it.help) TC.openHelp(it.help); else if (it.set) TC.screens.options(it.set[2], it.set[0]); }
    inp.addEventListener('input', render);
    inp.addEventListener('keydown', function(e){
      if (e.key === 'ArrowDown' || e.key === 'ArrowUp'){ e.preventDefault(); hot = TC.clamp(hot + (e.key === 'ArrowDown' ? 1 : -1), 0, items.length - 1); paint(); }
      else if (e.key === 'Enter'){ e.preventDefault(); pick(items[hot]); }
      else if (e.key === 'Escape'){ close(); inp.value = ''; inp.blur(); }
    });
    inp.addEventListener('blur', function(){ setTimeout(close, 150); });
  } };

  /* ================= menu công cụ prototype (không thuộc sản phẩm) ================= */
  TC.protoMenu = function(){
    var setEd = function(e){ return function(){ TC.S.edition = e; TC.save('edition', e); TC.emit('edition'); TC.toast('Đang xem với phiên bản <b>' + TC.EDITIONS[e].name + (e === 'trial' ? ' (dùng thử)' : '') + '</b>.', { type:'info', log:false }); }; };
    var setTheme = function(t){ return function(){ TC.S.theme = t; TC.save('theme', t); TC.applyTheme(); }; };
    return [
      { h:'Phiên bản' },
      { label:'Viewer (miễn phí)', check:TC.S.edition === 'viewer', act:setEd('viewer') }, { label:'Viewer Pro', check:TC.S.edition === 'vp', act:setEd('vp') }, { label:'Pro · dùng thử 12 ngày', check:TC.S.edition === 'trial', act:setEd('trial') }, { label:'Pro', check:TC.S.edition === 'pro', act:setEd('pro') }, { label:'Giáo dục', check:TC.S.edition === 'edu', act:setEd('edu') },
      { h:'Giao diện' },
      { label:'Chủ đề', sub:[{ label:'Theo hệ thống', check:TC.S.theme === 'auto', act:setTheme('auto') }, { label:'Tối', check:TC.S.theme === 'dark', act:setTheme('dark') }, { label:'Sáng', check:TC.S.theme === 'light', act:setTheme('light') }] },
      { label:'Mật độ thoáng', check:TC.S.density === 'comfortable', act:function(){ TC.S.density = TC.S.density === 'comfortable' ? 'compact' : 'comfortable'; TC.save('density', TC.S.density); TC.applyTheme(); A().fitRibbon(); } },
      { label:'Giao diện nhẹ (máy yếu)', check:!!TC.S.lite, act:function(){ TC.S.lite = !TC.S.lite; TC.save('lite', TC.S.lite); TC.applyTheme(); } },
      { label:'Khung màn hình', sub:[{ label:'Vừa cửa sổ trình duyệt', check:!TC.frame, act:function(){ TC.setFrame(null); } }, { label:'1366 × 768 (laptop phổ biến)', check:TC.frame === 1366, act:function(){ TC.setFrame(1366); } }, { label:'1920 × 1080', check:TC.frame === 1920, act:function(){ TC.setFrame(1920); } }] },
      { h:'Xem màn hình' },
      { label:'Danh mục 172 màn hình…', icon:'list', act:function(){ TC.screens.catalog(); } },
      { label:'Hệ thống thiết kế (styleguide)', icon:'ext', act:function(){ location.href = 'styleguide.html'; } },
      { label:'Mô phỏng tình huống', sub:[
        { label:'Lần chạy đầu (onboarding)', act:function(){ TC.screens.onboarding(); } },
        { label:'Mở tệp lớn (hiển thị dần)', act:function(){ TC.files.openLarge(); } },
        { label:'Mở tệp lỗi cấu trúc', act:function(){ TC.files.openBroken(); } },
        { label:'Khởi động lại sau sự cố', act:function(){ TC.screens.crash(); } },
        { label:'Có bản cập nhật', act:function(){ TC.screens.update(true); } },
        { label:'Trình cài đặt Windows', act:function(){ TC.screens.installer(); } },
        { label:'Gõ lệnh LINE trong Viewer', act:function(){ TC.cmd.run('LINE', null, true); } },
        { label:'Tác vụ nền xong (toast)', act:function(){ TC.toast('Đã xuất 3 tờ PDF', { act:{ label:'Mở thư mục', fn:function(){} } }); } }
      ] },
      { sep:1 },
      { label:'Đặt lại prototype', icon:'refresh', act:function(){ try { Object.keys(localStorage).filter(function(k){ return k.indexOf('tc-proto-') === 0; }).forEach(function(k){ localStorage.removeItem(k); }); } catch (e) {} location.reload(); } }
    ];
  };
})();
