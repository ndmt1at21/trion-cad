/* TrionCAD prototype · Đợt 2 (Viewer Pro), phần 2: in hàng loạt, chuyển font hàng loạt, attribute với Excel, giấy phép.
   UI-H04 In nhiều layout, UI-H06 In hàng loạt theo khung tên, UI-J02 (chuyển đổi), UI-J03, UI-J15, UI-L01, UI-L02, UI-L03. */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic;
  var SC = TC.screens, IMPL = TC.cmd.impl, opts = SC.opts, seg = SC.seg;
  function A(){ return TC.app; } function V(){ return TC.app.view; } function D(){ return TC.app.doc(); }
  var proLock = function(){ return TC.can('PRO') ? '' : ' <span class="ed xs" data-ed="pro">Pro</span>'; };
  var base = function(n){ return n.replace(/\.\w+$/, ''); };
  // tên tệp gửi qua Zalo, email: bỏ dấu tiếng Việt, khoảng trắng thành “-”
  function ascii(s){ return String(s).normalize('NFD').replace(/[\u0300-\u036f]/g, '').replace(/đ/g, 'd').replace(/Đ/g, 'D').replace(/[^\w.\-]+/g, '-').replace(/-+/g, '-').replace(/^-|-$/g, ''); }
  function fill(tpl, vars){ return tpl.replace(/\{(\w+)\}/g, function(m, k){ return vars[k] != null ? vars[k] : m; }); }
  function attrsOf(d, e){ var o = {}, b = d.blocks[e.name]; if (b) b.ents.forEach(function(a){ if (a.t === 'attdef') o[a.tag] = e.attrs && e.attrs[a.tag] != null ? e.attrs[a.tag] : a.s; }); return o; }
  function hasAtt(d, n){ var b = d.blocks[n]; return !!(b && b.ents.some(function(x){ return x.t === 'attdef'; })); }
  function handle(e){ return (0x2A0 + e.id).toString(16).toUpperCase(); }
  function where(d, sp){ return sp === 'model' ? 'Model' : d.layouts[sp].name; }
  // mô phỏng tác vụ nền: chạy lần lượt (hoặc song song conc tệp), có tạm dừng, hủy
  function simulate(list, conc, onStep, onEnd){
    var job = { paused:false, stop:false, done:false }, run = [], next = 0;
    var t = setInterval(function(){
      if (job.stop){ clearInterval(t); list.forEach(function(it){ if (it.st !== 'done') it.st = 'cancel'; }); job.done = true; onEnd(true); return; }
      if (job.paused) return;
      while (run.length < conc && next < list.length){ var it = list[next++]; it.p = 0; it.st = 'run'; run.push(it); }
      run.forEach(function(it){ it.p = Math.min(100, it.p + it.speed); if (it.failAt && it.p >= it.failAt){ it.st = 'done'; it.res = 'err'; } else if (it.p >= 100) it.st = 'done'; });
      run = run.filter(function(it){ return it.st === 'run'; });
      onStep();
      if (!run.length && next >= list.length){ clearInterval(t); job.done = true; onEnd(false); }
    }, 140);
    list.forEach(function(it){ it.st = 'wait'; it.p = 0; });
    return job;
  }
  function stCell(it, okText){
    if (it.st === 'wait') return '<span class="faint">Chờ</span>';
    if (it.st === 'cancel') return '<span class="faint">Đã hủy</span>';
    if (it.st === 'run') return '<span class="cellprog"><span class="prog"><i style="width:' + it.p + '%"></i></span><span class="mono">' + Math.round(it.p) + '%</span></span>';
    if (it.res === 'err') return '<span class="tag danger" title="' + esc(it.msg || '') + '">' + ic('error', 's12') + esc(it.short || 'Lỗi') + '</span>';
    if (it.res === 'skip') return '<span class="faint" title="' + esc(it.msg || '') + '">' + esc(it.short || 'Bỏ qua') + '</span>';
    if (it.res === 'warn') return '<span class="tag warn" title="' + esc(it.msg || '') + '">' + ic('warn', 's12') + esc(it.short || 'Xong, có cảnh báo') + '</span>';
    return '<span class="tag ok">' + ic('check', 's12') + esc(okText || it.short || 'Xong') + '</span>';
  }
  function task(id, label, list, job){
    var n = list.filter(function(x){ return x.st === 'done'; }).length, p = list.reduce(function(s, x){ return s + (x.st === 'done' ? 100 : x.p || 0); }, 0) / list.length;
    A().setTask(job.done ? '' : '<span class="sb-task">' + (job.paused ? ic('pause', 's12') : '<span class="spin"></span>') + esc(label) + ' ' + n + '/' + list.length + '<span class="prog"><i style="width:' + p.toFixed(0) + '%"></i></span><button class="ib sm" type="button" id="' + id + '-x" aria-label="Hủy ' + esc(label.toLowerCase()) + '">' + ic('x') + '</button></span>');
    var x = $('#' + id + '-x'); if (x) x.onclick = function(){ job.stop = true; };
  }
  function tokens(id, list){ return '<span class="tokens" role="group" aria-label="Chèn trường vào tên tệp">' + list.map(function(t){ return '<button class="chip" type="button" data-tok="' + t + '" data-for="' + id + '">{' + t + '}</button>'; }).join('') + '</span>'; }
  document.addEventListener('click', function(e){ var t = e.target.closest('[data-tok]'); if (!t) return; var inp = document.getElementById(t.dataset.for); if (!inp) return; var s = inp.selectionStart != null ? inp.selectionStart : inp.value.length; inp.value = inp.value.slice(0, s) + '{' + t.dataset.tok + '}' + inp.value.slice(inp.selectionEnd || s); inp.dispatchEvent(new Event('input', { bubbles:true })); inp.focus(); });
  function recenter(dlg){ var el = dlg.el; el.style.left = Math.max(12, (innerWidth - el.offsetWidth) / 2) + 'px'; el.style.top = Math.max(12, (innerHeight - el.offsetHeight) / 2.4) + 'px'; }
  // phóng tới khung ở phần vùng vẽ không bị hộp thoại không modal che
  function zoomBeside(bb, el){
    var v = V(), cv = $('#cv').getBoundingClientRect(), r = el.getBoundingClientRect(), L0 = 0, R0 = cv.width;
    if (cv.right - r.right >= r.left - cv.left) L0 = Math.max(0, r.right - cv.left); else R0 = Math.max(0, r.left - cv.left);
    var rw = Math.max(160, R0 - L0), pv = v.vw(); v.pushHist();
    pv.s = Math.min(rw / ((bb[2] - bb[0]) * 1.12), (v.H - 24) / ((bb[3] - bb[1]) * 1.12)); pv.cx = (bb[0] + bb[2]) / 2 - (L0 + rw / 2 - v.W / 2) / pv.s; pv.cy = (bb[1] + bb[3]) / 2;
    v.draw(); TC.emit('view');
  }
  function paperShort(p){ return p.split(' (')[0].replace('ISO ', ''); }

  /* ================= In hàng loạt theo khung tên UI-H06 ================= */
  var BP = { step:1, how:'block', names:null, layer:'KT-KHUNG', sample:null, scope:{ model:true, layouts:false }, view:'grid', order:'lr', dev:'PDF của TrionCAD', pdf:'each', ctb:'TCVN.ctb', tpl:'{KYHIEU}_{TENBV}', ascii:true, center:true, lw:true, tplName:'<Không>', cur:-1 };
  var BP_TPL = { 'A3 PDF · công ty':{ dev:'PDF của TrionCAD', pdf:'each', ctb:'TCVN.ctb', tpl:'{KYHIEU}_{TENBV}' }, 'A1 máy HP DesignJet':{ dev:'HP DesignJet T650 (khổ A1)', pdf:'each', ctb:'monochrome.ctb', tpl:'{KYHIEU}' }, 'Hồ sơ nộp (1 tệp)':{ dev:'PDF của TrionCAD', pdf:'one', ctb:'TCVN.ctb', tpl:'{TenBanVe}_ho-so' } };
  function frameBlocks(d){
    var c = {}; function walk(list, k){ list.forEach(function(e){ if (e.t === 'insert' && hasAtt(d, e.name)){ c[e.name] = c[e.name] || { name:e.name, model:0, lay:0 }; c[e.name][k]++; } }); }
    walk(d.ents, 'model'); d.layouts.forEach(function(L){ walk(L.ents, 'lay'); });
    return Object.keys(c).map(function(k){ return c[k]; });
  }
  function paperOf(w, h){ var a = Math.max(w, h), b = Math.min(w, h); var p = SC.PAPERS.filter(function(x){ return Math.abs(Math.max(x[1], x[2]) - a) / a < .02 && Math.abs(Math.min(x[1], x[2]) - b) / b < .02; })[0]; return p ? p[0] : 'Tự chọn (' + Math.round(a) + ' × ' + Math.round(b) + ' mm)'; }
  function findFrames(d){
    var out = [];
    function ok(e){ if (BP.how === 'block') return e.t === 'insert' && BP.names[e.name]; if (BP.how === 'layer') return e.layer === BP.layer && ((e.t === 'insert' && hasAtt(d, e.name)) || (e.t === 'pline' && e.closed)); return e.t === 'insert' && e.name === BP.sample; }
    if (BP.scope.model) d.ents.forEach(function(e){ if (ok(e)) out.push({ e:e, sp:'model' }); });
    if (BP.scope.layouts) d.layouts.forEach(function(L, i){ L.ents.forEach(function(e){ if (ok(e)) out.push({ e:e, sp:i }); }); });
    out.forEach(function(f, i){ var bb = f.e._bb, k = f.e.t === 'insert' ? Math.abs(f.e.sx) : 100; f.at = f.e.t === 'insert' ? attrsOf(d, f.e) : {}; f.scale = k; f.w = (bb[2] - bb[0]) / k; f.h = (bb[3] - bb[1]) / k; f.paper = paperOf(f.w, f.h); f.orient = f.w >= f.h ? 'Ngang' : 'Dọc'; f.fit = false; f.on = true; f.name = ((f.at.KYHIEU ? f.at.KYHIEU + ' ' : '') + (f.at.TENBV || 'Khung ' + (i + 1))).trim(); f.where = where(d, f.sp); });
    var key = { lr:function(f){ return [-Math.round(f.e._bb[3] / 5000), f.e._bb[0]]; }, tb:function(f){ return [Math.round(f.e._bb[0] / 5000), -f.e._bb[3]]; } };
    out.sort(function(a, b){ if (a.sp !== b.sp) return a.sp === 'model' ? -1 : b.sp === 'model' ? 1 : a.sp - b.sp; if (BP.order === 'attr') return String(a.at.KYHIEU || '').localeCompare(String(b.at.KYHIEU || '')); var x = key[BP.order](a), y = key[BP.order](b); return x[0] - y[0] || x[1] - y[1]; });
    return out;
  }
  function bpName(d, f, i){ if (BP.pdf === 'one' && BP.dev.indexOf('PDF') >= 0) return (BP.ascii ? ascii(base(d.name)) : base(d.name)) + '_bo-ban-ve.pdf'; var n = fill(BP.tpl, Object.assign({}, f.at, { TenBanVe:base(d.name), STT:('0' + (i + 1)).slice(-2), Ngay:TC.date().replace(/\//g, '-') })); return (BP.ascii ? ascii(n) : n) + (BP.dev.indexOf('PNG') >= 0 ? '.png' : '.pdf'); }
  SC.batchPlot = function(){
    var d = D(); if (!d){ TC.toast('Mở một bản vẽ có khung tên để in hàng loạt.', { type:'warn', log:false }); return; }
    var ex = $('#bp-body'); if (ex) { ex.closest('.dlg').focus(); return; }
    if (!BP.names || BP.doc !== d.id){ BP.names = {}; frameBlocks(d).forEach(function(b){ if (b.model) BP.names[b.name] = true; }); if (!Object.keys(BP.names).length){ frameBlocks(d).forEach(function(b){ BP.names[b.name] = true; }); BP.scope = { model:true, layouts:true }; } BP.doc = d.id; BP.step = 1; BP.job = null; }
    BP.frames = findFrames(d); BP.dir = d.path.replace(/\\[^\\]+$/, '\\PDF');
    var STEPS = ['Nhận diện khung', 'Danh sách khung', 'Thiết lập in', 'Chạy'];
    var dlg = TC.dialog({ id:'bp', title:'In hàng loạt theo khung tên', cmd:'TCBATCHPLOT', code:'UI-H06', w:820, h:640, modal:false, flush:true,
      body:'<div class="wz"><ol class="wz-steps" id="bp-steps" aria-label="Các bước"></ol><div class="wz-bar" id="bp-bar"></div><div class="wz-body" id="bp-body"></div></div>', foot:'<span></span>',
      onClose:function(){ d.found = []; V().drawOver(); if (BP.job && !BP.job.done) TC.toast('In hàng loạt vẫn chạy ở nền. Theo dõi ở thanh trạng thái.', { type:'info', log:false }); } });
    var cr0 = $('#cv').getBoundingClientRect(); dlg.el.style.left = (cr0.left + 12) + 'px'; dlg.el.style.top = Math.max(12, Math.min(cr0.top + 4, innerHeight - 652)) + 'px'; // đặt bên trái vùng vẽ, chừa chỗ xem khung
    var $d = function(s){ return $(s, dlg.el); }, on = function(f){ return f.on; };
    function maxStep(){ return BP.job ? 4 : BP.frames.some(on) ? 3 : 1; }
    function render(){
      if (!document.contains(dlg.el)) return;
      $d('#bp-steps').innerHTML = STEPS.map(function(s, i){ var n = i + 1, st = n < BP.step ? 'done' : n === BP.step ? 'cur' : ''; return '<li><button type="button" class="wz-s ' + st + '" data-step="' + n + '"' + (n > maxStep() || (BP.job && !BP.job.done && n < 4) ? ' disabled' : '') + (n === BP.step ? ' aria-current="step"' : '') + '><span class="wz-n">' + (st === 'done' ? ic('check', 's12') : n) + '</span>' + s + '</button></li>'; }).join('');
      var sel = BP.frames.filter(on);
      $d('#bp-bar').innerHTML = '<span class="lab">Mẫu in</span><select class="inp" id="bp-tplname" aria-label="Mẫu in" style="width:200px">' + opts(['<Không>'].concat(Object.keys(BP_TPL)), BP.tplName) + '</select><button class="btn sm ghost" type="button" data-bp="savetpl">Lưu mẫu in…</button><span class="grow"></span><span class="faint" style="font-size:var(--fs-s)">' + esc(d.name) + ' · ' + sel.length + ' khung sẽ in' + (sel.length ? ' · ' + esc(BP.dev) : '') + '</span>';
      $d('#bp-body').innerHTML = [s1, s2, s3, s4][BP.step - 1]();
      var f = $('.dlg-f', dlg.el), running = BP.job && !BP.job.done;
      f.innerHTML = BP.step === 1 ? '<span class="grow"></span><button class="btn ghost" type="button" data-x="close">Hủy</button><button class="btn primary" type="button" data-bp="next"' + (BP.frames.length ? '' : ' disabled') + '>Tiếp ' + ic('right', 'xs') + '</button>'
        : BP.step < 4 ? '<button class="btn" type="button" data-bp="back">' + ic('left', 'xs') + 'Quay lại</button><span class="grow"></span>' + (BP.step === 2 ? '<button class="btn primary" type="button" data-bp="next"' + (sel.length ? '' : ' disabled') + '>Tiếp ' + ic('right', 'xs') + '</button>' : '<button class="btn primary" type="button" data-bp="run">' + ic('print') + (BP.dev.indexOf('PDF') >= 0 ? 'Xuất ' + sel.length + ' tờ PDF' : 'In ' + sel.length + ' tờ') + '</button>')
        : running ? '<button class="btn ghost" type="button" data-x="close">Chạy ở nền</button><span class="grow"></span><button class="btn" type="button" data-bp="pause">' + (BP.job.paused ? ic('play') + 'Tiếp tục' : ic('pause') + 'Tạm dừng') + '</button><button class="btn danger" type="button" data-bp="stop">Hủy</button>'
        : '<button class="btn" type="button" data-bp="report">' + ic('list') + 'Xem báo cáo</button><button class="btn" type="button" data-bp="folder">' + ic('folder') + 'Mở thư mục</button><span class="grow"></span><button class="btn" type="button" data-bp="again">Thiết lập lại</button><button class="btn primary" type="button" data-x="close">Đóng</button>';
      $$('canvas[data-ft]', dlg.el).forEach(function(c){ var fr = BP.frames[Number(c.dataset.ft)]; if (fr.sp === 'model') TC.regionThumb(c, d, fr.e._bb); else TC.thumb(c, d, fr.sp); });
    }
    function card(k, title, desc, inner){ var sel = BP.how === k; return '<div class="wz-card' + (sel ? ' on' : '') + '"><label class="chk"><input type="radio" name="bp-how" value="' + k + '"' + (sel ? ' checked' : '') + '><b>' + title + '</b></label><span class="faint">' + desc + '</span>' + (sel ? '<div class="wz-in">' + inner + '</div>' : '') + '</div>'; }
    function s1(){
      var fb = frameBlocks(d);
      return '<div class="wz-cols"><div class="stack"><span class="gh">Nhận diện khung theo</span>' +
        card('block', 'Tên block khung tên', 'Chọn một hoặc nhiều block. Đề xuất: block có attribute số hiệu, tên bản vẽ.', fb.length ? '<div class="tblwrap"><table class="tbl"><colgroup><col style="width:28px"><col><col style="width:64px"><col style="width:64px"><col style="width:74px"></colgroup><thead><tr><th></th><th>Block</th><th class="num">Model</th><th class="num">Layout</th><th></th></tr></thead><tbody>' + fb.map(function(b){ return '<tr><td class="ctr"><input type="checkbox" data-bname="' + esc(b.name) + '"' + (BP.names[b.name] ? ' checked' : '') + ' aria-label="Dùng block ' + esc(b.name) + '"></td><td>' + esc(b.name) + '</td><td class="num">' + b.model + '</td><td class="num">' + b.lay + '</td><td>' + (b.model ? '<span class="tag acc">đề xuất</span>' : '') + '</td></tr>'; }).join('') + '</tbody></table></div>' : '<span class="faint">Bản vẽ không có block nào có attribute.</span>') +
        card('layer', 'Layer của hình chữ nhật viền khung', 'Dùng khi khung vẽ bằng polyline kín, không phải block.', '<select class="inp" id="bp-layer" aria-label="Layer viền khung">' + opts(d.layers.map(function(l){ return l.name; }), BP.layer) + '</select>') +
        card('sample', 'Đối tượng mẫu', 'Chọn một khung trên bản vẽ; TrionCAD tìm các khung giống nó.', '<span class="hrow"><button class="btn" type="button" data-bp="pick">' + ic('select') + 'Chọn khung mẫu &lt;</button>' + (BP.sample ? '<span class="tag acc">' + esc(BP.sample) + '</span>' : '<span class="faint">Chưa chọn</span>') + '</span>') +
        '</div><div class="stack"><span class="gh">Phạm vi</span><label class="chk"><input type="checkbox" data-scope="model"' + (BP.scope.model ? ' checked' : '') + '> Model</label><label class="chk"><input type="checkbox" data-scope="layouts"' + (BP.scope.layouts ? ' checked' : '') + '> Các layout (' + d.layouts.length + ')</label><label class="chk dis" data-tip="Có từ bản 1.0" data-desc="Quét nhiều tệp DWG cùng lúc, in thành một bộ."><input type="checkbox" disabled> Nhiều tệp <span class="tag">bản 1.0</span></label>' +
        '<div class="wz-found"><b class="mono">' + BP.frames.length + '</b><span>khung tìm thấy</span></div>' +
        '<div class="stack" style="gap:3px;font-size:var(--fs-s)">' + BP.frames.slice(0, 6).map(function(f){ return '<span class="hrow" style="gap:6px;flex-wrap:nowrap">' + ic('layout', 's12') + '<span>' + esc(f.name) + '</span><span class="faint">· ' + paperShort(f.paper) + ' · ' + esc(f.where) + '</span></span>'; }).join('') + (BP.frames.length > 6 ? '<span class="faint">và ' + (BP.frames.length - 6) + ' khung khác</span>' : '') + (BP.frames.length ? '' : '<span class="faint">Chưa tìm thấy khung. Thử chọn block khác hoặc bật phạm vi Các layout.</span>') + '</div></div></div>';
    }
    function s2(){
      var fr = BP.frames, sel = fr.filter(on).length;
      return '<div class="stack"><div class="hrow">' + seg('bpview', ['Lưới', 'Bảng'], BP.view === 'grid' ? 'Lưới' : 'Bảng') + '<span class="lab" style="margin-left:6px">Thứ tự</span><select class="inp" id="bp-order" style="width:auto">' + [['lr','Trái → phải, trên → dưới'],['tb','Trên → dưới, trái → phải'],['attr','Theo số hiệu (KYHIEU)']].map(function(o){ return '<option value="' + o[0] + '"' + (BP.order === o[0] ? ' selected' : '') + '>' + o[1] + '</option>'; }).join('') + '</select><button class="btn sm ghost" type="button" data-bp="all">Chọn tất cả</button><button class="btn sm ghost" type="button" data-bp="none">Bỏ chọn</button><span class="grow"></span><span class="faint" style="font-size:var(--fs-s)">' + sel + '/' + fr.length + ' khung sẽ in · bấm một khung để xem trên bản vẽ</span></div>' +
        (BP.view === 'grid' ? '<div class="bp-grid">' + fr.map(function(f, i){ return '<div class="bp-card' + (f.on ? '' : ' off') + (i === BP.cur ? ' is-sel' : '') + '" data-fi="' + i + '" role="button" tabindex="0"><canvas width="300" height="212" data-ft="' + i + '" aria-hidden="true"></canvas><label class="chk"><input type="checkbox" data-fon="' + i + '"' + (f.on ? ' checked' : '') + '><b>' + (i + 1) + '. ' + esc(f.name) + '</b></label><span class="faint">' + paperShort(f.paper) + ' ' + f.orient.toLowerCase() + ' · ' + (f.fit ? 'vừa khổ' : '1:' + f.scale) + ' · ' + esc(f.where) + '</span></div>'; }).join('') + '</div>'
        : '<div class="tblwrap"><table class="tbl"><colgroup><col style="width:30px"><col style="width:34px"><col><col style="width:190px"><col style="width:96px"><col style="width:150px"><col style="width:110px"></colgroup><thead><tr><th></th><th class="num">#</th><th>Tên tờ (từ attribute)</th><th>Khổ giấy</th><th>Hướng</th><th>Tỉ lệ</th><th>Ở</th></tr></thead><tbody>' + fr.map(function(f, i){ return '<tr data-fi="' + i + '" class="' + (i === BP.cur ? 'is-sel' : '') + '"><td class="ctr"><input type="checkbox" data-fon="' + i + '"' + (f.on ? ' checked' : '') + ' aria-label="In khung ' + (i + 1) + '"></td><td class="num">' + (i + 1) + '</td><td>' + esc(f.name) + '</td><td><select class="inp sm" data-fp="' + i + '">' + opts(SC.PAPERS.map(function(p){ return p[0]; }), f.paper) + '</select></td><td><select class="inp sm" data-fo="' + i + '">' + opts(['Ngang','Dọc'], f.orient) + '</select></td><td><select class="inp sm" data-fs="' + i + '"><option' + (f.fit ? '' : ' selected') + '>1:' + f.scale + ' (tự suy ra)</option><option' + (f.fit ? ' selected' : '') + '>Vừa khổ giấy</option></select></td><td>' + esc(f.where) + '</td></tr>'; }).join('') + '</tbody></table></div>') +
        '<p class="faint" style="font-size:var(--fs-xs);margin:0">Khổ giấy suy ra từ kích thước khung chia cho tỉ lệ chèn block (ví dụ khung 42000 × 29700 chèn tỉ lệ 100 là A3). Sửa được ở chế độ Bảng.</p></div>';
    }
    function s3(){
      var pdf = BP.dev.indexOf('PDF') >= 0, prn = !pdf && BP.dev.indexOf('PNG') < 0, fr = BP.frames.filter(on);
      return '<div class="wz-cols"><div class="stack">' +
        '<div class="grp"><span class="gh">Máy in, tệp</span><select class="inp" id="bp-dev" aria-label="Máy in">' + opts(SC.DEVICES, BP.dev) + '</select>' + (pdf ? '<label class="chk"><input type="radio" name="bp-pdf" value="one"' + (BP.pdf === 'one' ? ' checked' : '') + '> Một tệp PDF nhiều trang</label><label class="chk"><input type="radio" name="bp-pdf" value="each"' + (BP.pdf === 'each' ? ' checked' : '') + '> Mỗi khung một tệp PDF</label>' : '') + (prn ? '<span class="faint" style="font-size:var(--fs-s)">Mỗi khung in một tờ theo khổ đã suy ra; khay giấy chọn tự động.</span>' : '') + '</div>' +
        '<div class="grp"><span class="gh">Plot style và nét</span><select class="inp" id="bp-ctb" aria-label="Plot style">' + opts(SC.CTBS, BP.ctb) + '</select><label class="chk"><input type="checkbox" id="bp-lw"' + (BP.lw ? ' checked' : '') + '> In độ dày nét</label></div>' +
        '<div class="grp"><span class="gh">Vị trí trên giấy</span><label class="chk"><input type="checkbox" id="bp-center"' + (BP.center ? ' checked' : '') + '> Canh giữa</label><div class="frow" style="--lab-w:60px"><span class="lab">X / Y</span><span class="hrow">' + TC.numField({ id:'bp-ox', value:'0.00', label:'Offset X', pick:false, style:'width:90px' }) + TC.numField({ id:'bp-oy', value:'0.00', label:'Offset Y', pick:false, style:'width:90px' }) + '</span></div></div></div>' +
        '<div class="stack"><div class="grp' + (prn ? ' is-dim' : '') + '"><span class="gh">Tên tệp</span><input class="inp mono" id="bp-tpl" value="' + esc(BP.tpl) + '" aria-label="Mẫu tên tệp"' + (BP.pdf === 'one' && pdf ? ' disabled' : '') + '>' + tokens('bp-tpl', ['KYHIEU','TENBV','TILE','CONGTRINH','TenBanVe','STT','Ngay']) + '<label class="chk"><input type="checkbox" id="bp-ascii"' + (BP.ascii ? ' checked' : '') + '> Bỏ dấu tiếng Việt, thay khoảng trắng bằng “-”</label>' +
          '<div class="names" id="bp-names">' + (BP.pdf === 'one' && pdf ? '<span class="mono">' + esc(bpName(d, fr[0] || {}, 0)) + '</span>' : fr.slice(0, 4).map(function(f, i){ return '<span class="mono">' + esc(bpName(d, f, i)) + '</span>'; }).join('') + (fr.length > 4 ? '<span class="faint">và ' + (fr.length - 4) + ' tệp khác</span>' : '')) + '</div></div>' +
          '<div class="grp"><span class="gh">Thư mục đích</span><span class="hrow" style="flex-wrap:nowrap"><input class="inp grow mono" id="bp-dir" value="' + esc(BP.dir) + '" style="font-size:var(--fs-xs)" aria-label="Thư mục đích"><button class="btn" type="button" data-bp="browse">Duyệt…</button></span><label class="chk"><input type="checkbox" checked> Mở thư mục khi xong</label></div></div></div>';
    }
    function s4(){
      var R = BP.run || [], ok = R.filter(function(x){ return x.st === 'done' && x.res !== 'err'; }).length, warn = R.filter(function(x){ return x.res === 'warn' && x.st === 'done'; }).length, done = BP.job && BP.job.done, p = R.length ? R.reduce(function(s, x){ return s + (x.st === 'done' ? 100 : x.p || 0); }, 0) / R.length : 0;
      return '<div class="stack"><div class="bp-sum"><div><span class="faint">Số tờ</span><b>' + R.length + '</b></div><div><span class="faint">Thiết bị</span><b>' + esc(BP.dev) + '</b></div><div><span class="faint">Tệp</span><b>' + (BP.dev.indexOf('PDF') >= 0 ? (BP.pdf === 'one' ? 'Một tệp nhiều trang' : 'Mỗi khung một tệp') : '—') + '</b></div><div><span class="faint">Plot style</span><b>' + esc(BP.ctb) + '</b></div><div><span class="faint">Thư mục</span><b class="mono" style="font-size:var(--fs-xs)">' + esc(BP.dir) + '</b></div></div>' +
        '<div class="hrow"><span class="prog grow"><i style="width:' + p.toFixed(0) + '%"></i></span><span class="mono" style="font-size:var(--fs-s)">' + R.filter(function(x){ return x.st === 'done'; }).length + '/' + R.length + '</span></div>' +
        (done ? '<div class="note ' + (BP.job.stop ? 'warn' : warn ? 'warn' : 'acc') + '">' + ic(BP.job.stop ? 'warn' : 'okc') + '<span>' + (BP.job.stop ? 'Đã hủy. ' + ok + '/' + R.length + ' tờ đã in xong trước khi hủy.' : 'Xong ' + ok + '/' + R.length + ' tờ' + (warn ? ', ' + warn + ' cảnh báo font: <b>.VnTimeH</b> được thay bằng TrionVN-Times (hoa). Bản in giống trên màn hình.' : '.')) + '</span></div>' : '<div class="note">' + ic('info') + '<span>Đang chạy ở nền. Có thể đóng hộp thoại và tiếp tục xem bản vẽ; tiến trình hiện ở thanh trạng thái.</span></div>') +
        '<div class="tblwrap"><table class="tbl"><colgroup><col style="width:34px"><col><col style="width:290px"><col style="width:190px"></colgroup><thead><tr><th class="num">#</th><th>Tên tờ</th><th>Tệp</th><th>Trạng thái</th></tr></thead><tbody>' + R.map(function(x, i){ return '<tr><td class="num">' + (i + 1) + '</td><td>' + esc(x.f.name) + '</td><td class="mono" style="font-size:var(--fs-xs)">' + esc(x.file) + '</td><td>' + stCell(x, 'Xong') + '</td></tr>'; }).join('') + '</tbody></table></div></div>';
    }
    function refind(){ BP.frames = findFrames(d); BP.cur = -1; render(); }
    BP.ui = render;
    function start(){
      var fr = BP.frames.filter(on);
      BP.run = fr.map(function(f, i){ return { f:f, file:bpName(d, f, i), speed:12 + (i % 3) * 4, res:f.e.t === 'insert' ? 'warn' : 'ok', short:'Xong · font thay thế', msg:'Font .VnTimeH thay bằng TrionVN-Times (hoa)' }; });
      BP.step = 4;
      BP.job = simulate(BP.run, 1, function(){ BP.ui(); task('bp', 'In hàng loạt', BP.run, BP.job); }, function(cancel){ BP.ui(); task('bp', 'In hàng loạt', BP.run, BP.job); var ok = BP.run.filter(function(x){ return x.st === 'done'; }).length; TC.toast(cancel ? 'Đã hủy in hàng loạt sau ' + ok + ' tờ.' : 'In hàng loạt xong <b>' + ok + ' tờ</b> từ ' + esc(d.name) + (BP.dev.indexOf('PDF') >= 0 ? ' vào ' + esc(BP.dir) : ' tới ' + esc(BP.dev)), { type:cancel ? 'warn' : 'ok', act:{ label:'Mở thư mục', fn:function(){} }, kind:'task' }); TC.cmdline.msg('TCBATCHPLOT: ' + ok + '/' + BP.run.length + ' tờ' + (cancel ? ' (đã hủy)' : '') + '.'); });
      render();
    }
    dlg.el.addEventListener('click', function(e){
      var st = e.target.closest('[data-step]'); if (st && !st.disabled){ BP.step = Number(st.dataset.step); render(); return; }
      var vw = e.target.closest('[data-seg="bpview"] button'); if (vw){ BP.view = vw.dataset.v === 'Lưới' ? 'grid' : 'table'; render(); return; }
      var card2 = e.target.closest('[data-fi]'); if (card2 && !e.target.closest('input, select, label')){ var i = Number(card2.dataset.fi), f = BP.frames[i]; BP.cur = i; if (f.sp !== d.space) A().setSpace(f.sp); d.found = [f.e.id]; zoomBeside(f.e._bb, dlg.el); render(); return; }
      var b = e.target.closest('[data-bp]'); if (!b) return; var k = b.dataset.bp;
      if (k === 'next'){ BP.step = Math.min(3, BP.step + 1); render(); }
      else if (k === 'back'){ BP.step = Math.max(1, BP.step - 1); render(); }
      else if (k === 'run') start();
      else if (k === 'pause'){ BP.job.paused = !BP.job.paused; render(); task('bp', 'In hàng loạt', BP.run, BP.job); }
      else if (k === 'stop') BP.job.stop = true;
      else if (k === 'again'){ BP.job = null; BP.run = null; BP.step = 3; render(); }
      else if (k === 'all' || k === 'none'){ BP.frames.forEach(function(f){ f.on = k === 'all'; }); render(); }
      else if (k === 'report') SC.textWindow(['TrionCAD · Báo cáo in hàng loạt theo khung tên', 'Bản vẽ: ' + d.path, 'Thiết bị: ' + BP.dev + ' · ' + BP.ctb, ''].concat(BP.run.map(function(x, i){ return (i + 1) + '. ' + x.f.name + ' → ' + x.file + ' · ' + (x.st === 'cancel' ? 'đã hủy' : x.res === 'warn' ? 'xong, cảnh báo: ' + x.msg : 'xong'); })).join('\n'), 'Báo cáo TCBATCHPLOT');
      else if (k === 'folder' || k === 'browse') TC.toast(k === 'folder' ? 'Mở ' + esc(BP.dir) + ' trong File Explorer.' : 'Hộp thoại chọn thư mục của Windows (bản thật).', { type:'info', log:false });
      else if (k === 'savetpl') TC.confirm({ title:'Lưu mẫu in', msg:'Lưu thiết lập hiện tại thành mẫu “' + esc(paperShort((BP.frames[0] || { paper:'A3' }).paper)) + ' ' + esc(BP.dev.indexOf('PDF') >= 0 ? 'PDF' : 'máy in') + ' · ' + esc(base(d.name)) + '”?', detail:'Mẫu gồm cách nhận diện khung, thứ tự, máy in, plot style, mẫu tên tệp. Dùng lại cho các bộ bản vẽ sau.', ok:'Lưu mẫu in' }).then(function(y){ if (y) TC.toast('Đã lưu mẫu in.', { log:false }); });
      else if (k === 'pick'){
        dlg.hide(true); TC.cmdline.msg('Bấm vào viền một khung tên trên bản vẽ. Esc để quay lại.');
        var esc1 = function(ev){ if (ev.key === 'Escape'){ ev.stopPropagation(); TC.cad.endSel(); } }; document.addEventListener('keydown', esc1, true);
        TC.cad.getSel({ onPick:function(h){ if (h) TC.cad.endSel(); } }).then(function(ids){ document.removeEventListener('keydown', esc1, true); var e2 = (ids || []).map(function(id){ return d.byId[id]; }).filter(function(x){ return x && x.t === 'insert'; })[0]; TC.cad.clearSel(); dlg.hide(false); if (e2){ BP.sample = e2.name; BP.how = 'sample'; refind(); TC.cmdline.msg('Khung mẫu: block ' + esc(e2.name) + '. Tìm thấy ' + BP.frames.length + ' khung giống.'); } else if (ids && ids.length) TC.toast('Đối tượng vừa chọn không phải block khung tên. Bấm vào viền khung.', { type:'warn', log:false }); });
      }
    });
    dlg.el.addEventListener('change', function(e){
      var t = e.target;
      if (t.name === 'bp-how'){ BP.how = t.value; refind(); }
      else if (t.dataset.bname){ BP.names[t.dataset.bname] = t.checked; refind(); }
      else if (t.dataset.scope){ BP.scope[t.dataset.scope] = t.checked; refind(); }
      else if (t.id === 'bp-layer'){ BP.layer = t.value; refind(); }
      else if (t.dataset.fon){ BP.frames[Number(t.dataset.fon)].on = t.checked; render(); }
      else if (t.dataset.fp){ BP.frames[Number(t.dataset.fp)].paper = t.value; }
      else if (t.dataset.fo){ BP.frames[Number(t.dataset.fo)].orient = t.value; }
      else if (t.dataset.fs){ BP.frames[Number(t.dataset.fs)].fit = t.selectedIndex === 1; }
      else if (t.id === 'bp-order'){ BP.order = t.value; refind(); }
      else if (t.id === 'bp-dev'){ BP.dev = t.value; render(); }
      else if (t.name === 'bp-pdf'){ BP.pdf = t.value; render(); }
      else if (t.id === 'bp-ctb') BP.ctb = t.value;
      else if (t.id === 'bp-lw') BP.lw = t.checked;
      else if (t.id === 'bp-center') BP.center = t.checked;
      else if (t.id === 'bp-ascii'){ BP.ascii = t.checked; render(); }
      else if (t.id === 'bp-dir') BP.dir = t.value;
      else if (t.id === 'bp-tplname'){ BP.tplName = t.value; if (BP_TPL[t.value]) Object.assign(BP, BP_TPL[t.value]); render(); }
    });
    dlg.el.addEventListener('input', function(e){ if (e.target.id === 'bp-tpl'){ BP.tpl = e.target.value; var fr = BP.frames.filter(on); $d('#bp-names').innerHTML = fr.slice(0, 4).map(function(f, i){ return '<span class="mono">' + esc(bpName(d, f, i)) + '</span>'; }).join('') + (fr.length > 4 ? '<span class="faint">và ' + (fr.length - 4) + ' tệp khác</span>' : ''); } });
    render();
  };
  IMPL.TCBATCHPLOT = function(){ SC.batchPlot(); };

  /* ================= In nhiều layout, nhiều bản vẽ UI-H04 ================= */
  SC.publish = function(){
    var d0 = D(); if (!d0){ TC.toast('Mở bản vẽ để in nhiều layout.', { type:'warn', log:false }); return; }
    var st = { inc:'layouts', dest:'PDF', multi:true, tpl:'{TenBanVe}-{Layout}', ascii:true, dir:d0.path.replace(/\\[^\\]+$/, '\\PDF'), stamp:false, bg:true, open:true, sel:-1, list:[], run:null, job:null };
    function from(d){ var out = []; if (st.inc !== 'layouts') out.push({ d:d, sp:'model', setup:'' }); if (st.inc !== 'model') d.layouts.forEach(function(L, i){ out.push({ d:d, sp:i, setup:L.setup }); }); return out; }
    function rebuild(){ st.list = A().docs.filter(function(x){ return x.open; }).reduce(function(a, x){ return a.concat(from(x)); }, []); st.list.forEach(function(s){ s.on = true; }); }
    rebuild();
    function nm(s){ return base(s.d.name) + ' – ' + where(s.d, s.sp); }
    function ps(s){ return s.d.pageSetups.filter(function(p){ return p.name === s.setup; })[0] || null; }
    function check(s){ var p = ps(s); if (!p && s.sp === 'model') return ['warn', 'Chưa có thiết lập: in vùng đang xem, vừa khổ A3']; if (!p) return ['err', 'Không tìm thấy thiết lập trang']; if (st.dest === 'Máy in' && p.device.indexOf('PDF') >= 0) return ['info', 'Thiết lập in ra PDF']; return ['ok', 'Sẵn sàng']; }
    function file(s, i){ var n = fill(st.tpl, { TenBanVe:base(s.d.name), Layout:where(s.d, s.sp), STT:('0' + (i + 1)).slice(-2), KYHIEU:(function(){ var L = s.sp === 'model' ? null : s.d.layouts[s.sp]; var f = L && L.ents.filter(function(e){ return e.t === 'insert' && hasAtt(s.d, e.name); })[0]; return f ? attrsOf(s.d, f).KYHIEU : ''; })() }); return (st.ascii ? ascii(n) : n) + (st.dest === 'PNG' ? '.png' : '.pdf'); }
    var dlg = TC.dialog({ id:'pub', title:'In nhiều layout, nhiều bản vẽ', cmd:'PUBLISH', code:'UI-H04', w:940, body:'<div class="stack" id="pb-b"></div>', foot:'<span></span>',
      onClose:function(){ if (st.job && !st.job.done) TC.toast('In nhiều layout vẫn chạy ở nền.', { type:'info', log:false }); } });
    function render(){
      if (!document.contains(dlg.el)) return;
      var on = st.list.filter(function(s){ return s.on; }), running = st.job && !st.job.done, R = st.run;
      $('#pb-b', dlg.el).innerHTML =
        '<div class="pub-tools"><button class="btn sm" type="button" data-pb="add" aria-haspopup="menu"' + (running ? ' disabled' : '') + '>' + ic('plus') + 'Thêm tờ' + ic('down', 'xs') + '</button><span class="lab">Gồm</span><select class="inp sm" id="pb-inc" style="width:auto"' + (running ? ' disabled' : '') + '>' + [['layouts','Chỉ layout'],['model','Chỉ Model'],['both','Model và layout']].map(function(o){ return '<option value="' + o[0] + '"' + (st.inc === o[0] ? ' selected' : '') + '>' + o[1] + '</option>'; }).join('') + '</select><span class="sep"></span><button class="ib" type="button" data-pb="rm" aria-label="Bỏ tờ" data-tip="Bỏ tờ khỏi danh sách"' + (st.sel < 0 || running ? ' disabled' : '') + '>' + ic('minus') + '</button><button class="ib" type="button" data-pb="up" aria-label="Lên" data-tip="Lên"' + (st.sel < 1 || running ? ' disabled' : '') + '>' + ic('up') + '</button><button class="ib" type="button" data-pb="down" aria-label="Xuống" data-tip="Xuống"' + (st.sel < 0 || st.sel >= st.list.length - 1 || running ? ' disabled' : '') + '>' + ic('down') + '</button><span class="grow"></span><button class="btn sm ghost" type="button" data-pb="load">' + ic('open') + 'Nạp danh sách…</button><button class="btn sm ghost" type="button" data-pb="save">' + ic('save') + 'Lưu danh sách…</button></div>' +
        '<div class="tblwrap" style="height:208px"><table class="tbl"><colgroup><col style="width:30px"><col><col style="width:200px"><col style="width:210px"><col style="width:220px"></colgroup><thead><tr><th></th><th>Tên tờ</th><th>Thiết lập trang</th><th>Khổ, thiết bị</th><th>Trạng thái</th></tr></thead><tbody>' +
          st.list.map(function(s, i){ var p = ps(s), c = check(s), r = R && R.filter(function(x){ return x.s === s; })[0]; return '<tr data-pi="' + i + '" class="' + (i === st.sel ? 'is-sel' : '') + (s.on ? '' : ' faint') + '"><td class="ctr"><input type="checkbox" data-pon="' + i + '"' + (s.on ? ' checked' : '') + (running ? ' disabled' : '') + ' aria-label="In ' + esc(nm(s)) + '"></td><td>' + ic(s.sp === 'model' ? 'model' : 'layout', 's12') + ' ' + esc(nm(s)) + '</td><td><select class="inp sm" data-pps="' + i + '"' + (running ? ' disabled' : '') + ' aria-label="Thiết lập trang">' + '<option value="">' + (s.sp === 'model' ? '&lt;Vùng đang xem&gt;' : '&lt;Không&gt;') + '</option>' + s.d.pageSetups.map(function(q){ return '<option' + (q.name === s.setup ? ' selected' : '') + '>' + esc(q.name) + '</option>'; }).join('') + '</select></td><td class="faint" title="' + esc(p ? p.paper + ' · ' + p.device : '') + '">' + (p ? esc(paperShort(p.paper)) + ' · ' + esc(p.device) : '—') + '</td><td>' + (r ? stCell(r, 'Xong · ' + r.file) : '<span class="tag ' + (c[0] === 'ok' ? 'ok' : c[0] === 'err' ? 'danger' : c[0] === 'warn' ? 'warn' : 'info') + '" title="' + esc(c[1]) + '">' + esc(c[1]) + '</span>') + '</td></tr>'; }).join('') +
          (st.list.length ? '' : '<tr><td colspan="5" class="faint">Danh sách trống. Thêm tờ từ bản vẽ đang mở hoặc từ tệp.</td></tr>') + '</tbody></table></div>' +
        '<div class="grid2"><div class="grp"><span class="gh">Đích</span>' + seg('pbdest', ['Máy in', 'PDF', 'PNG'], st.dest) + '<span class="faint" style="font-size:var(--fs-s)">' + (st.dest === 'Máy in' ? 'Mỗi tờ in theo máy in và khổ giấy trong thiết lập trang của tờ đó.' : st.dest === 'PDF' ? 'PDF vector, giữ layer; preset theo Tùy chọn xuất PDF (UI-H05).' : 'Mỗi tờ một ảnh PNG 300 DPI, nền trắng.') + '</span>' +
          (st.dest === 'PDF' ? '<label class="chk"><input type="radio" name="pb-multi" value="1"' + (st.multi ? ' checked' : '') + '> Một tệp nhiều trang</label><label class="chk"><input type="radio" name="pb-multi" value="0"' + (st.multi ? '' : ' checked') + '> Mỗi tờ một tệp</label>' : '') + (st.dest === 'Máy in' ? '<div class="frow" style="--lab-w:60px"><span class="lab">Số bản</span>' + TC.numField({ id:'pb-copies', value:'1', label:'Số bản', pick:false, prec:0, min:0, unit:'none', style:'width:90px' }) + '</div>' : '') + '</div>' +
          '<div class="grp' + (st.dest === 'Máy in' ? ' is-dim' : '') + '"><span class="gh">Tên tệp và thư mục</span><input class="inp mono" id="pb-tpl" value="' + esc(st.multi && st.dest === 'PDF' ? '{TenBanVe}_bo-ban-ve' : st.tpl) + '" aria-label="Mẫu tên tệp"' + (st.dest === 'Máy in' ? ' disabled' : '') + '>' + tokens('pb-tpl', ['TenBanVe','Layout','KYHIEU','STT']) + '<span class="names" id="pb-names">' + (st.dest === 'Máy in' ? '' : (st.multi && st.dest === 'PDF' ? '<span class="mono">' + esc((st.ascii ? ascii(base(d0.name)) : base(d0.name)) + '_bo-ban-ve.pdf') + '</span>' : on.slice(0, 3).map(function(s, i){ return '<span class="mono">' + esc(file(s, i)) + '</span>'; }).join(''))) + '</span><span class="hrow" style="flex-wrap:nowrap"><input class="inp grow mono" id="pb-dir" value="' + esc(st.dir) + '" style="font-size:var(--fs-xs)" aria-label="Thư mục"' + (st.dest === 'Máy in' ? ' disabled' : '') + '><button class="btn" type="button" data-pb="browse"' + (st.dest === 'Máy in' ? ' disabled' : '') + '>Duyệt…</button></span></div></div>' +
        '<div class="hrow" style="gap:6px 16px"><label class="chk"><input type="checkbox" id="pb-stamp"' + (st.stamp ? ' checked' : '') + '> In dấu in</label><button class="btn sm ghost" type="button" data-pb="stampset">Thiết lập dấu in… <span class="tag">P2</span></button><label class="chk"><input type="checkbox" checked> In ở nền</label><label class="chk"><input type="checkbox" checked> Mở thư mục khi xong</label></div>';
      var f = $('.dlg-f', dlg.el), n = on.length, label = st.dest === 'PDF' ? (st.multi ? 'Xuất ' + n + ' tờ thành 1 PDF' : 'Xuất ' + n + ' tờ PDF') : st.dest === 'PNG' ? 'Xuất ' + n + ' ảnh PNG' : 'In ' + n + ' tờ';
      f.innerHTML = running ? '<button class="btn ghost" type="button" data-x="close">Chạy ở nền</button><span class="grow"></span><button class="btn" type="button" data-pb="pause">' + (st.job.paused ? ic('play') + 'Tiếp tục' : ic('pause') + 'Tạm dừng') + '</button><button class="btn danger" type="button" data-pb="stop">Hủy</button>'
        : '<button class="btn" type="button" data-pb="preview"' + (st.sel < 0 ? ' disabled title="Chọn một tờ để xem trước"' : '') + '>' + ic('preview') + 'Xem trước</button><button class="btn ghost" type="button" data-pb="log">' + ic('list') + 'Nhật ký</button><span class="grow"></span><button class="btn ghost" type="button" data-x="close">' + (R ? 'Đóng' : 'Hủy') + '</button><button class="btn primary" type="button" data-pb="go"' + (n ? '' : ' disabled') + '>' + ic(st.dest === 'Máy in' ? 'print' : st.dest === 'PNG' ? 'image' : 'pdf') + label + '</button>';
    }
    dlg.el.addEventListener('click', function(e){
      var ds = e.target.closest('[data-seg="pbdest"] button'); if (ds){ st.dest = ds.dataset.v; render(); return; }
      var row = e.target.closest('[data-pi]'); if (row && !e.target.closest('input, select')){ st.sel = Number(row.dataset.pi); render(); }
      var b = e.target.closest('[data-pb]'); if (!b || b.disabled) return; var k = b.dataset.pb;
      if (k === 'add'){ var r = b.getBoundingClientRect(); TC.menu([{ h:'Từ bản vẽ đang mở' }].concat(A().docs.filter(function(x){ return x.open; }).map(function(x){ return { label:esc(x.name) + ' <span class="faint">· Model và ' + x.layouts.length + ' layout</span>', act:function(){ var inc = st.inc; st.inc = 'both'; from(x).forEach(function(s){ if (!st.list.some(function(q){ return q.d === s.d && q.sp === s.sp; })){ s.on = true; st.list.push(s); } }); st.inc = inc; render(); } }; })).concat([{ sep:1 }, { label:'Từ tệp…', icon:'open', act:function(){ TC.toast('Hộp thoại chọn nhiều tệp DWG (UI-B01); tờ của tệp chưa mở được thêm mà không cần mở tệp.', { type:'info', log:false }); } }]), r.left, r.bottom + 2, { minWidth:220 }); }
      else if (k === 'rm'){ st.list.splice(st.sel, 1); st.sel = Math.min(st.sel, st.list.length - 1); render(); }
      else if (k === 'up' || k === 'down'){ var j = st.sel + (k === 'up' ? -1 : 1), tmp = st.list[j]; st.list[j] = st.list[st.sel]; st.list[st.sel] = tmp; st.sel = j; render(); }
      else if (k === 'save') TC.toast('Đã lưu danh sách tờ thành <span class="mono">' + esc(base(d0.name)) + '.dsd</span> (đọc được bằng AutoCAD).', { log:false });
      else if (k === 'load') TC.toast('Chọn tệp danh sách tờ .dsd để nạp.', { type:'info', log:false });
      else if (k === 'browse') TC.toast('Hộp thoại chọn thư mục của Windows (bản thật).', { type:'info', log:false });
      else if (k === 'stampset') TC.toast('Thiết lập dấu in (tên bản vẽ, layout, ngày giờ, người in…) là tính năng P2.', { type:'info', log:false });
      else if (k === 'preview'){ var s = st.list[st.sel]; if (s.d !== D()) A().activate(s.d.id); A().setSpace(s.sp); SC.preview(); }
      else if (k === 'log') SC.textWindow(st.run ? st.run.map(function(x){ return nm(x.s) + ' → ' + x.file + ' · ' + (x.res === 'warn' ? 'xong, cảnh báo: ' + x.msg : x.st === 'cancel' ? 'đã hủy' : 'xong'); }).join('\n') : 'Chưa chạy lần nào. Nhật ký ghi từng tờ: thời gian, thiết bị, tệp kết quả, cảnh báo và lỗi.', 'Nhật ký PUBLISH');
      else if (k === 'pause'){ st.job.paused = !st.job.paused; render(); }
      else if (k === 'stop') st.job.stop = true;
      else if (k === 'go'){
        var on = st.list.filter(function(s){ return s.on; });
        st.run = on.map(function(s, i){ var c = check(s), font = TC.fontDoctor.summary(s.d).missing; return { s:s, file:st.dest === 'Máy in' ? ps(s) && ps(s).device || 'máy in mặc định' : st.multi && st.dest === 'PDF' ? (st.ascii ? ascii(base(d0.name)) : base(d0.name)) + '_bo-ban-ve.pdf · trang ' + (i + 1) : file(s, i), speed:14 + (i % 2) * 6, res:c[0] === 'err' ? 'err' : font || c[0] === 'warn' ? 'warn' : 'ok', short:c[0] === 'err' ? 'Lỗi thiết lập' : font ? 'Xong · ' + font + ' font thay thế' : c[0] === 'warn' ? 'Xong · vùng đang xem' : '', msg:c[0] === 'err' ? c[1] : font ? font + ' font thay thế' : c[1], failAt:c[0] === 'err' ? 20 : 0 }; });
        st.job = simulate(st.run, 1, function(){ render(); task('pb', 'In nhiều layout', st.run, st.job); }, function(cancel){ render(); task('pb', 'In nhiều layout', st.run, st.job); var ok = st.run.filter(function(x){ return x.st === 'done' && x.res !== 'err'; }).length; TC.toast((cancel ? 'Đã hủy. ' : '') + 'In nhiều layout: ' + ok + '/' + st.run.length + ' tờ xong.', { type:cancel ? 'warn' : 'ok', act:{ label:'Mở thư mục', fn:function(){} }, kind:'task' }); });
        render();
      }
    });
    dlg.el.addEventListener('change', function(e){
      var t = e.target;
      if (t.id === 'pb-inc'){ st.inc = t.value; rebuild(); st.sel = -1; render(); }
      else if (t.dataset.pon){ st.list[Number(t.dataset.pon)].on = t.checked; render(); }
      else if (t.dataset.pps){ st.list[Number(t.dataset.pps)].setup = t.value; render(); }
      else if (t.name === 'pb-multi'){ st.multi = t.value === '1'; render(); }
      else if (t.id === 'pb-stamp') st.stamp = t.checked;
      else if (t.id === 'pb-dir') st.dir = t.value;
    });
    render(); recenter(dlg);
    dlg.el.addEventListener('input', function(e){ if (e.target.id === 'pb-tpl' && !(st.multi && st.dest === 'PDF')){ st.tpl = e.target.value; var on = st.list.filter(function(s){ return s.on; }); $('#pb-names', dlg.el).innerHTML = on.slice(0, 3).map(function(s, i){ return '<span class="mono">' + esc(file(s, i)) + '</span>'; }).join(''); } });
  };
  IMPL.PUBLISH = function(){ SC.publish(); };

  /* ================= Chuyển font hàng loạt UI-J03 và Xử lý tệp hàng loạt UI-B14: chung một màn hình, lệnh quyết định thao tác bật sẵn; Viewer Pro chỉ có chuyển font ================= */
  // tên, thư mục, dung lượng, chữ mã cũ, kết quả chuyển font, tốc độ mô phỏng, phiên bản, số lỗi sửa được, số mục dọn được, số layout
  var FB_FILES = [
    ['Banve-01.dwg', 'D:\\DuAn\\NhaO-2T\\KienTruc', '4,8 MB', { TCVN3:113, VNI:13 }, 'ok', 9, 'DWG 2018', 0, 46, 2],
    ['Ketcau-T2.dwg', 'D:\\DuAn\\NhaO-2T\\KetCau', '1,9 MB', {}, 'skip', 100, 'DWG 2013', 2, 8, 0],
    ['Chi-tiet-cau-thang.dwg', 'D:\\DuAn\\NhaO-2T\\KienTruc', '2,2 MB', { TCVN3:96 }, 'ok', 14, 'DWG 2018', 0, 21, 1],
    ['Thong-ke-thep-mong.dxf', 'D:\\DuAn\\NhaO-2T\\KetCau', '0,9 MB', { TCVN3:57, low:3 }, 'warn', 18, 'DXF 2010', 1, 5, 1],
    ['MB-Truong-THCS-An-Phu.dwg', '\\\\SRV-01\\DuAn\\2026\\THCS-AnPhu', '38,2 MB', { TCVN3:2410 }, 'ok', 2.6, 'DWG 2010', 7, 312, 6],
    ['Dien-nuoc-T1-T3.dwg', 'D:\\DuAn\\Biet-thu-Thao-Dien\\ME', '6,4 MB', { VNI:388 }, 'ok', 7, 'DWG 2018', 0, 64, 3],
    ['Hien-trang-khu-dat.dwg', 'D:\\Khao-sat\\Lo-B7', '12,1 MB', null, 'err', 6, 'DWG 2004', 0, 0, 1]
  ];
  var FB_OPS = [['audit', 'Kiểm tra và sửa lỗi'], ['purge', 'Dọn dẹp (kể cả regapps)'], ['font', 'Chuyển font sang Unicode'], ['ver', 'Chuyển phiên bản'], ['prn', 'Đặt máy in của layout về “Không”']];
  var FB = SC.FB = { out:'dir', dir:'D:\\DuAn\\_Unicode', suf:'_unicode', fmt:'Giữ nguyên', regapp:false, low:'skip', files:null, job:null, mode:'font', ops:{ font:true }, ver:'DWG 2013', tpl:'',
    tpls:{ 'Gửi đối tác · DWG 2013':{ ops:{ audit:true, purge:true, font:true, ver:true }, ver:'DWG 2013', out:'dir', dir:'D:\\DuAn\\_Gui-doi-tac', suf:'_2013', low:'skip' }, 'Lưu trữ cuối dự án':{ ops:{ audit:true, purge:true, prn:true }, ver:'DWG 2018', out:'over', dir:'D:\\DuAn\\_Da-xu-ly', suf:'_xu-ly', low:'skip' } } };
  SC.fbFiles = function(){ return FB_FILES.map(function(f){ return { name:f[0], dir:f[1], size:f[2], enc:f[3], res0:f[4], speed:f[5], ver:f[6], aud:f[7], pur:f[8], lay:f[9], on:true }; }); };
  // kết quả theo từng thao tác của một tệp (bản Pro, khi bật thao tác ngoài chuyển font)
  function fbRes(f){
    var o = FB.ops, L = [], n = f.enc ? Object.keys(f.enc).filter(function(x){ return x !== 'low'; }).reduce(function(a, x){ return a + f.enc[x]; }, 0) : 0, low = o.font && f.enc && f.enc.low && FB.low === 'skip';
    if (f.res0 === 'err'){ f.res = 'err'; f.short = 'Lỗi cấu trúc tệp'; f.msg = 'Kiểm tra lỗi: không sửa được tự động. Mở bằng Phục hồi (RECOVER) rồi chạy lại.'; return; }
    if (o.audit) L.push(f.aud ? 'Kiểm tra: sửa ' + f.aud + ' lỗi' : 'Kiểm tra: không có lỗi');
    if (o.purge) L.push('Dọn dẹp: ' + TC.n(f.pur) + ' mục');
    if (o.font) L.push(n ? 'Font: ' + TC.n(n - (low ? f.enc.low : 0)) + ' đối tượng sang Unicode' + (low ? ', bỏ qua ' + f.enc.low + ' độ tin cậy thấp' : '') : 'Font: đã là Unicode');
    if (o.ver) L.push(f.ver === FB.ver ? 'Phiên bản: đã là ' + FB.ver : 'Phiên bản: ' + f.ver + ' → ' + FB.ver);
    if (o.prn) L.push('Máy in: ' + f.lay + ' layout về “Không”');
    f.res = low ? 'warn' : 'ok'; f.short = low ? 'Xong, ' + f.enc.low + ' bỏ qua' : 'Xong · ' + L.length + ' thao tác'; f.msg = L.join(' · ');
  }
  function fbReport(){
    var R = FB.files.filter(function(f){ return f.on && f.st; });
    return ['TrionCAD · Báo cáo ' + (FB.mode === 'batch' ? 'xử lý tệp hàng loạt' : 'chuyển font hàng loạt') + ' · ' + TC.date(), 'Thao tác (theo thứ tự): ' + FB_OPS.filter(function(o){ return FB.ops[o[0]]; }).map(function(o){ return o[1] + (o[0] === 'ver' ? ' (' + FB.ver + ')' : ''); }).join(', '),
      'Đầu ra: ' + (FB.out === 'over' ? 'ghi đè tệp gốc, tạo .bak' : FB.out === 'dir' ? 'lưu sang ' + FB.dir + ', giữ cấu trúc thư mục' : 'thêm hậu tố ' + FB.suf + ' vào tên tệp'), '']
      .concat(R.map(function(f, i){ return (i + 1) + '. ' + f.dir + '\\' + f.name + ' · ' + (f.st === 'cancel' ? 'đã hủy' : f.res === 'err' ? 'lỗi' : f.res === 'warn' ? 'xong, có cảnh báo' : f.res === 'skip' ? 'bỏ qua' : 'xong') + (f.msg ? '\n   ' + f.msg : ''); })).join('\n');
  }
  function fbSaveTpl(){
    var dlg = TC.dialog({ id:'fbtpl', title:'Lưu thiết lập thành mẫu', code:'UI-B14', w:420, help:false,
      body:'<div class="field"><label for="ft-n">Tên mẫu</label><input class="inp" id="ft-n" value="' + esc(FB.mode === 'batch' ? 'Xử lý hàng loạt ' + (Object.keys(FB.tpls).length + 1) : 'Chuyển font · thư mục _Unicode') + '" spellcheck="false"><span class="fmsg" id="ft-m"></span><span class="faint" style="font-size:var(--fs-xs)">Mẫu gồm các thao tác, phiên bản đích, đầu ra và tùy chọn chuyển font.</span></div>',
      foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="ft-ok">Lưu mẫu</button>' });
    $('#ft-ok', dlg.el).addEventListener('click', function(){ var inp = $('#ft-n', dlg.el), v = inp.value.trim(), m = $('#ft-m', dlg.el), err = !v ? 'Nhập tên mẫu.' : FB.tpls[v] ? 'Đã có mẫu tên này. Đặt tên khác.' : '';
      inp.classList.toggle('err', !!err); m.className = 'fmsg' + (err ? ' err' : ''); m.innerHTML = err ? ic('warn', 'xs') + esc(err) : ''; if (err) return;
      FB.tpls[v] = { ops:Object.assign({}, FB.ops), ver:FB.ver, out:FB.out, dir:FB.dir, suf:FB.suf, low:FB.low }; FB.tpl = v; dlg.close(true); if (FB.ui) FB.ui(); TC.toast('Đã lưu mẫu “' + esc(v) + '”. Chọn lại ở ô Mẫu thiết lập.', { log:false }); });
  }
  SC.fontBatch = function(mode){
    mode = mode === 'batch' ? 'batch' : 'font';
    var running0 = FB.job && !FB.job.done;
    if ($('#fb-b')){ var old = TC.dialog({ id:'fb' }); if (FB.mode === mode || running0) return; old.close(); }
    if (FB.mode !== mode && !running0){ FB.mode = mode; FB.job = null; FB.tpl = ''; FB.out = 'dir'; FB.ops = mode === 'batch' ? { audit:true, purge:true } : { font:true }; FB.dir = mode === 'batch' ? 'D:\\DuAn\\_Da-xu-ly' : 'D:\\DuAn\\_Unicode'; FB.suf = mode === 'batch' ? '_xu-ly' : '_unicode'; (FB.files || []).forEach(function(f){ delete f.st; delete f.p; delete f.msg; }); }
    if (!FB.files) FB.files = SC.fbFiles();
    var P = TC.can('PRO'), B = FB.mode === 'batch', label = B ? 'Xử lý hàng loạt' : 'Chuyển font';
    var dlg = TC.dialog({ id:'fb', title:B ? 'Xử lý tệp hàng loạt' : 'Chuyển font hàng loạt', cmd:B ? 'TCBATCH' : 'FIXFONTBATCH', code:B ? 'UI-B14' : 'UI-J03', w:P ? 1040 : 980, h:640, modal:false, flush:true, body:'<div class="fb" id="fb-b"></div>', foot:'<span></span>',
      onClose:function(){ if (FB.job && !FB.job.done) TC.toast(label + ' vẫn chạy ở nền.', { type:'info', log:false }); } });
    FB.ui = function(){ render(); };
    function encTxt(f){ if (!f.enc) return '<span class="faint">Chưa đọc được</span>'; var k = Object.keys(f.enc).filter(function(x){ return x !== 'low'; }); return k.length ? k.map(function(x){ return '<span class="tag warn">' + x + ' ' + TC.n(f.enc[x]) + '</span>'; }).join(' ') : '<span class="tag ok">Unicode</span>'; }
    function more(){ return P && FB_OPS.some(function(o){ return o[0] !== 'font' && FB.ops[o[0]]; }); }
    function render(){
      if (!document.contains(dlg.el)) return;
      var running = FB.job && !FB.job.done, R = FB.files.filter(function(f){ return f.on; }), done = FB.job && FB.job.done, dis = running ? ' disabled' : '', fon = !P || FB.ops.font, anyOp = !P || FB_OPS.some(function(o){ return FB.ops[o[0]]; });
      var conv = R.reduce(function(s, f){ return s + (f.st === 'done' && f.res !== 'err' && f.enc ? Object.keys(f.enc).filter(function(x){ return x !== 'low'; }).reduce(function(a, x){ return a + f.enc[x]; }, 0) * (f.res === 'skip' ? 0 : 1) - (FB.low === 'skip' ? f.enc.low || 0 : 0) : 0); }, 0);
      $('#fb-b', dlg.el).innerHTML =
        '<div class="fb-ops"><span class="gh">' + (P ? 'Thao tác · chạy theo thứ tự' : 'Thao tác') + '</span>' + (P ? FB_OPS.map(function(o){ return '<span class="hrow" style="gap:6px;flex-wrap:nowrap"><label class="chk"><input type="checkbox" data-fbop="' + o[0] + '"' + (FB.ops[o[0]] ? ' checked' : '') + dis + '> ' + o[1] + '</label>' + (o[0] === 'ver' ? '<select class="inp sm" id="fb-ver" aria-label="Phiên bản đích" style="width:96px"' + (FB.ops.ver && !running ? '' : ' disabled') + '>' + opts(['DWG 2018', 'DWG 2013', 'DWG 2010', 'DWG 2007', 'DWG 2004', 'DWG 2000', 'DXF 2018'], FB.ver) + '</select>' : '') + '</span>'; }).join('') + '<label class="chk dis" data-tip="Chạy script .scr trên từng tệp" data-desc="Có sau v1.0. Hiện chạy script trên bản vẽ đang mở bằng lệnh SCRIPT."><input type="checkbox" disabled> Chạy script .scr <span class="tag">Sau v1.0</span></label>'
          : '<label class="chk"><input type="checkbox" checked disabled> Chuyển font sang Unicode</label>' + ['Kiểm tra và sửa lỗi', 'Dọn dẹp (kể cả regapps)', 'Chuyển phiên bản DWG', 'Đặt máy in về “Không”'].map(function(x){ return '<label class="chk dis" data-tip="' + esc(x) + '" data-desc="Thao tác của Xử lý tệp hàng loạt (TCBATCH, UI-B14), có trong TrionCAD Pro từ bản 1.0."><input type="checkbox" disabled> ' + x + ' <span class="ed xs" data-ed="pro">Pro</span></label>'; }).join('')) + '</div>' +
        '<div class="fb-main"><div class="fb-left"><div class="pal-tools"><button class="btn sm" type="button" data-fb="addf"' + dis + '>' + ic('file') + 'Thêm tệp…</button><button class="btn sm" type="button" data-fb="addd"' + dis + '>' + ic('folder') + 'Thêm thư mục…</button><label class="chk" style="margin-left:6px"><input type="checkbox" checked' + dis + '> Gồm thư mục con</label><span class="faint mono" style="font-size:var(--fs-xs);margin-left:6px">' + (P ? '*.dwg; *.dxf; *.dwt' : '*.dwg; *.dxf') + '</span><span class="grow"></span><button class="btn sm ghost" type="button" data-fb="clear"' + dis + '>Xóa danh sách</button></div>' +
          (FB.job ? '<div class="fb-prog"><span class="prog grow"><i style="width:' + (R.reduce(function(s, f){ return s + (f.st === 'done' ? 100 : f.p || 0); }, 0) / R.length).toFixed(0) + '%"></i></span><span class="mono">' + R.filter(function(f){ return f.st === 'done'; }).length + '/' + R.length + ' tệp' + (fon ? ' · ' + TC.n(conv) + ' đối tượng' : '') + '</span>' + (running ? '<span class="faint">· ' + (FB.job.paused ? 'đã tạm dừng' : '6 luồng') + '</span>' : '') + '</div>' : '') +
          '<div class="pal-body"><table class="tbl"><colgroup><col style="width:30px"><col><col style="width:70px">' + (P ? '<col style="width:84px">' : '') + (fon ? '<col style="width:150px">' : '') + '<col style="width:170px"></colgroup><thead><tr><th></th><th>Tệp</th><th class="num">Dung lượng</th>' + (P ? '<th>Phiên bản</th>' : '') + (fon ? '<th>Chữ mã cũ</th>' : '') + '<th>Trạng thái</th></tr></thead><tbody>' +
            FB.files.map(function(f, i){ return '<tr class="' + (f.on ? '' : 'faint') + '"><td class="ctr"><input type="checkbox" data-fbon="' + i + '"' + (f.on ? ' checked' : '') + dis + ' aria-label="' + (B ? 'Xử lý ' : 'Chuyển ') + esc(f.name) + '"></td><td class="two"><b style="font-weight:500">' + esc(f.name) + '</b><span class="faint mono">' + esc(f.dir) + '</span></td><td class="num">' + f.size + '</td>' + (P ? '<td class="mono" style="font-size:var(--fs-xs)">' + f.ver + '</td>' : '') + (fon ? '<td>' + encTxt(f) + '</td>' : '') + '<td' + (f.on && FB.job && f.msg ? ' title="' + esc(f.msg) + '"' : '') + '>' + (f.on && FB.job ? stCell(f, f.short) : '<span class="faint">' + (f.res0 === 'err' ? 'Có thể lỗi khi đọc' : f.res0 === 'skip' && !more() ? 'Sẽ bỏ qua: đã là Unicode' : 'Sẵn sàng') + '</span>') + '</td></tr>'; }).join('') + '</tbody></table></div>' +
          (done ? '<div class="note ' + (FB.job.stop ? 'warn' : 'acc') + '" style="margin:8px">' + ic(FB.job.stop ? 'warn' : 'okc') + '<span>' + (FB.job.stop ? 'Đã hủy. ' : '') + 'Xong ' + R.filter(function(f){ return f.st === 'done' && f.res !== 'err' && f.res !== 'skip'; }).length + '/' + R.length + ' tệp' + (fon ? ': <b>' + TC.n(conv) + '</b> đối tượng chữ chuyển sang Unicode. ' : '. ') + R.filter(function(f){ return f.st === 'done' && f.res === 'err'; }).length + ' lỗi, ' + R.filter(function(f){ return f.st === 'done' && f.res === 'warn'; }).length + ' cảnh báo, ' + R.filter(function(f){ return f.st === 'done' && f.res === 'skip'; }).length + ' bỏ qua.' + (P ? ' Chi tiết từng tệp ở Xem báo cáo.' : '') + '</span></div>' : '') + '</div>' +
        '<div class="fb-right">' + (P ? '<div class="frow" style="--lab-w:92px"><label class="lab" for="fb-tpl">Mẫu thiết lập</label><select class="inp" id="fb-tpl"' + dis + '><option value="">&lt;Không&gt;</option>' + Object.keys(FB.tpls).map(function(k){ return '<option' + (k === FB.tpl ? ' selected' : '') + '>' + esc(k) + '</option>'; }).join('') + '</select></div>' : '') +
          (fon ? '<div class="grp"><span class="gh">Tùy chọn chuyển</span><div class="frow" style="--lab-w:92px"><span class="lab">Bảng mã nguồn</span><select class="inp"' + dis + '><option>Tự nhận diện</option><option>Ép TCVN3</option><option>Ép VNI</option></select><span class="lab">TCVN3 →</span><select class="inp"' + dis + '><option>TrionVN-Times</option><option>Times New Roman</option><option>Arial</option></select><span class="lab">VNI →</span><select class="inp"' + dis + '><option>TrionVN-Arial</option><option>Arial</option><option>Times New Roman</option></select></div>' +
          '<div class="stack" style="gap:4px">' + ['Gồm block lồng, attribute', 'Gồm chữ kích thước, bảng', 'Cập nhật kiểu chữ sang font Unicode', 'Xử lý chữ hoa (.VnTimeH)', 'Giữ định dạng MTEXT'].map(function(x){ return '<label class="chk"><input type="checkbox" checked' + dis + '> ' + x + '</label>'; }).join('') + '</div>' +
          '<div class="field"><span class="lab">Đối tượng độ tin cậy thấp</span><select class="inp" id="fb-low"' + dis + '><option value="skip"' + (FB.low === 'skip' ? ' selected' : '') + '>Bỏ qua, ghi vào báo cáo</option><option value="go"' + (FB.low === 'go' ? ' selected' : '') + '>Vẫn chuyển</option></select><span class="faint" style="font-size:var(--fs-xs)">Chạy hàng loạt ở nền nên không hỏi từng cái như trong Bác sĩ Font.</span></div></div>' : '') +
          '<div class="grp"><span class="gh">Đầu ra</span>' +
            (P ? '<label class="chk"><input type="radio" name="fb-out" value="over"' + (FB.out === 'over' ? ' checked' : '') + dis + '> Ghi đè tệp gốc, tạo .bak</label>' : '<label class="chk dis" data-tip="Ghi đè tệp gốc" data-desc="Sửa trực tiếp tệp gốc (có .bak). Viewer Pro chỉ lưu bản sao."><input type="radio" name="fb-out" value="over" disabled> Ghi đè tệp gốc, tạo .bak' + proLock() + '</label>') +
            '<label class="chk"><input type="radio" name="fb-out" value="dir"' + (FB.out === 'dir' ? ' checked' : '') + dis + '> Lưu sang thư mục khác, giữ cấu trúc</label>' + (FB.out === 'dir' ? '<span class="hrow" style="flex-wrap:nowrap;padding-left:20px"><input class="inp grow mono" id="fb-dir" value="' + esc(FB.dir) + '" style="font-size:var(--fs-xs)" aria-label="Thư mục đích"' + dis + '><button class="btn" type="button" data-fb="browse"' + dis + '>Duyệt…</button></span>' : '') +
            '<label class="chk"><input type="radio" name="fb-out" value="suf"' + (FB.out === 'suf' ? ' checked' : '') + dis + '> Thêm hậu tố vào tên tệp</label>' + (FB.out === 'suf' ? '<span class="hrow" style="padding-left:20px"><input class="inp mono" id="fb-suf" value="' + esc(FB.suf) + '" style="width:120px" aria-label="Hậu tố"' + dis + '><span class="faint mono" style="font-size:var(--fs-xs)">Banve-01' + esc(FB.suf) + '.dwg</span></span>' : '') +
            (P ? '' : '<div class="frow" style="--lab-w:92px"><span class="lab">Định dạng lưu</span><select class="inp" id="fb-fmt"' + dis + '>' + opts(['Giữ nguyên','DWG 2018','DWG 2013','DWG 2010','DWG 2007','DWG 2004'], FB.fmt) + '</select></div><label class="chk"><input type="checkbox" id="fb-reg"' + (FB.regapp ? ' checked' : '') + dis + '> Dọn regapps (giảm dung lượng)</label>') + '</div>' +
          '<p class="faint" style="font-size:var(--fs-xs);margin:0">Chạy ở nền với 6 luồng (theo số nhân CPU). Đóng hộp thoại thì vẫn chạy tiếp; tiến trình ở thanh trạng thái.</p></div></div>';
      var n = R.length, f = $('.dlg-f', dlg.el);
      f.innerHTML = running ? '<button class="btn ghost" type="button" data-x="close">Chạy ở nền</button><span class="grow"></span><button class="btn" type="button" data-fb="pause">' + (FB.job.paused ? ic('play') + 'Tiếp tục' : ic('pause') + 'Tạm dừng') + '</button><button class="btn danger" type="button" data-fb="stop">Hủy</button>'
        : done ? (P ? '<button class="btn" type="button" data-fb="report">' + ic('list') + 'Xem báo cáo</button>' : '') + '<button class="btn" type="button" data-fb="csv">' + ic('stats') + 'Báo cáo CSV</button><button class="btn" type="button" data-fb="folder">' + ic('folder') + 'Mở thư mục kết quả</button><span class="grow"></span><button class="btn" type="button" data-fb="reset">Chạy lại</button><button class="btn primary" type="button" data-x="close">Đóng</button>'
        : '<button class="btn ghost" type="button" data-fb="tpl">Lưu thiết lập thành mẫu…</button><span class="grow"></span><button class="btn ghost" type="button" data-x="close">Hủy</button><button class="btn primary" type="button" data-fb="go"' + (n && anyOp ? '' : ' disabled') + '>' + ic('fontbatch') + (B ? 'Xử lý ' : 'Chuyển ') + n + ' tệp</button>';
    }
    dlg.el.addEventListener('click', function(e){
      var b = e.target.closest('[data-fb]'); if (!b || b.disabled) return; var k = b.dataset.fb;
      if (k === 'go'){
        var R = FB.files.filter(function(f){ return f.on; }), M = more();
        R.forEach(function(f){ f.res = f.res0; f.failAt = f.res0 === 'err' ? 34 : 0; f.short = f.res0 === 'skip' ? 'Bỏ qua: đã là Unicode' : f.res0 === 'err' ? 'Lỗi cấu trúc tệp' : f.res0 === 'warn' ? TC.n(f.enc.TCVN3 - f.enc.low) + ' · ' + f.enc.low + ' bỏ qua' : TC.n(Object.keys(f.enc).reduce(function(a, x){ return a + f.enc[x]; }, 0)) + ' đối tượng'; f.msg = f.res0 === 'err' ? 'Tệp bị lỗi cấu trúc. Mở bằng Phục hồi (RECOVER) rồi chạy lại.' : f.res0 === 'warn' ? f.enc.low + ' đối tượng độ tin cậy thấp đã bỏ qua, xem báo cáo' : ''; if (FB.low === 'go' && f.res0 === 'warn'){ f.res = 'ok'; f.short = TC.n(f.enc.TCVN3) + ' đối tượng'; } if (M) fbRes(f); });
        FB.job = simulate(R, 3, function(){ FB.ui(); task('fb', label, R, FB.job); }, function(cancel){ FB.ui(); task('fb', label, R, FB.job); var ok = R.filter(function(f){ return f.st === 'done' && f.res !== 'err' && f.res !== 'skip'; }).length, er = R.filter(function(f){ return f.st === 'done' && f.res === 'err'; }).length; TC.toast((cancel ? 'Đã hủy. ' : '') + (B ? 'Xử lý xong ' : 'Chuyển font xong ') + ok + '/' + R.length + ' tệp' + (er ? ': ' + er + ' lỗi' : ''), { type:er || cancel ? 'err' : 'ok', act:{ label:'Xem', fn:function(){ SC.fontBatch(FB.mode); } }, kind:'task' }); });
        render();
      }
      else if (k === 'pause'){ FB.job.paused = !FB.job.paused; render(); }
      else if (k === 'stop') FB.job.stop = true;
      else if (k === 'reset'){ FB.job = null; FB.files.forEach(function(f){ delete f.st; delete f.p; delete f.msg; }); render(); }
      else if (k === 'clear'){ FB.files = []; render(); }
      else if (k === 'addf' || k === 'addd'){ if (!FB.files.length) FB.files = SC.fbFiles(); TC.toast(k === 'addd' ? 'Đã quét thư mục D:\\DuAn: thêm 7 tệp *.dwg, *.dxf (gồm thư mục con) và nhận diện bảng mã.' : 'Hộp thoại chọn nhiều tệp (UI-B01).', { type:'info', log:false }); render(); }
      else if (k === 'csv') TC.toast('Đã xuất <span class="mono">' + (B ? 'Xu-ly-hang-loat_' : 'Chuyen-font_') + TC.date().replace(/\//g, '-') + '.csv</span>: mỗi tệp một dòng' + (B ? ', mỗi thao tác một cột.' : ', kèm đối tượng bỏ qua.'), { act:{ label:'Mở tệp', fn:function(){} }, log:false });
      else if (k === 'report') SC.textWindow(fbReport(), B ? 'Báo cáo xử lý tệp hàng loạt' : 'Báo cáo chuyển font hàng loạt');
      else if (k === 'folder' || k === 'browse') TC.toast(k === 'folder' ? 'Mở ' + esc(FB.dir) + ' trong File Explorer.' : 'Hộp thoại chọn thư mục của Windows.', { type:'info', log:false });
      else if (k === 'tpl'){ if (P) fbSaveTpl(); else TC.toast('Đã lưu thiết lập thành mẫu “Chuyển font · thư mục _Unicode”.', { log:false }); }
    });
    dlg.el.addEventListener('change', function(e){
      var t = e.target;
      if (t.dataset.fbon){ FB.files[Number(t.dataset.fbon)].on = t.checked; render(); }
      else if (t.dataset.fbop){ FB.ops[t.dataset.fbop] = t.checked; render(); }
      else if (t.id === 'fb-ver') FB.ver = t.value;
      else if (t.id === 'fb-tpl'){ var tp = FB.tpls[t.value]; FB.tpl = t.value; if (tp) Object.assign(FB, JSON.parse(JSON.stringify(tp))); render(); }
      else if (t.name === 'fb-out'){ FB.out = t.value; render(); }
      else if (t.id === 'fb-dir') FB.dir = t.value; else if (t.id === 'fb-suf'){ FB.suf = t.value; render(); } else if (t.id === 'fb-fmt') FB.fmt = t.value; else if (t.id === 'fb-reg') FB.regapp = t.checked; else if (t.id === 'fb-low') FB.low = t.value;
    });
    render();
  };
  IMPL.FIXFONTBATCH = function(){ SC.fontBatch(); };

  /* ================= Bác sĩ Font: xem trước và chuyển, lưu bản sao (UI-J02 khu vực 5, Viewer Pro) ================= */
  function legacyRows(d){
    var out = [];
    function st(n){ return d.styleMap[n] || {}; }
    function walk(list, sp){ list.forEach(function(e){
      if (e.t === 'text' && (st(e.style).enc === 'TCVN3' || st(e.style).enc === 'VNI')) out.push({ e:e, sp:sp, kind:'Text', s:e.s, style:e.style });
      if (e.t === 'insert' && d.blocks[e.name]) d.blocks[e.name].ents.forEach(function(b){ if ((b.t === 'attdef' || b.t === 'text') && (st(b.style).enc === 'TCVN3' || st(b.style).enc === 'VNI')) out.push({ e:e, sp:sp, kind:b.t === 'attdef' ? 'Attribute ' + b.tag : 'Text trong block', s:b.t === 'attdef' && e.attrs && e.attrs[b.tag] != null ? e.attrs[b.tag] : b.s, style:b.style, blk:e.name }); });
    }); }
    walk(d.ents, 'model'); d.layouts.forEach(function(L, i){ walk(L.ents, i); });
    return out;
  }
  SC.fontPreview = function(d){
    var rows = legacyRows(d), q = '';
    var dlg = TC.dialog({ id:'fdprev', title:'Xem trước chuyển đổi · ' + esc(d.name), cmd:'FIXFONT', code:'UI-J02', w:820, h:560, flush:true,
      body:'<div style="display:flex;flex-direction:column;height:100%;min-height:0"><div class="pal-tools"><span class="muted" style="font-size:var(--fs-s)"><b>' + TC.n(rows.length) + '</b> đối tượng sẽ đổi · 0 độ tin cậy thấp · bảng mã tự nhận diện</span><span class="grow"></span><label class="search" style="width:240px">' + ic('search') + '<input id="fp-q" placeholder="Tìm chữ" aria-label="Tìm chữ"></label></div><div style="flex:1;overflow:auto" id="fp-l"></div></div>',
      foot:'<span class="faint" style="font-size:var(--fs-s)">Bấm một dòng để zoom tới đối tượng.</span><span class="grow"></span><button class="btn ghost" type="button" data-x="close">Đóng</button><button class="btn primary" type="button" id="fp-go">' + (TC.can('PRO') ? 'Chuyển đổi' : 'Chuyển và lưu bản sao…') + '</button>' });
    function render(){ var list = rows.filter(function(r){ return !q || TC.fold(r.s).indexOf(TC.fold(q)) >= 0; });
      $('#fp-l', dlg.el).innerHTML = '<table class="tbl"><colgroup><col style="width:150px"><col style="width:70px"><col><col><col style="width:64px"></colgroup><thead><tr><th>Đối tượng</th><th>Bảng mã</th><th>Trước (chữ gốc)</th><th>Sau (Unicode)</th><th class="num">Tin cậy</th></tr></thead><tbody>' +
        list.slice(0, 300).map(function(r, i){ var enc = (d.styleMap[r.style] || {}).enc; return '<tr data-fz="' + rows.indexOf(r) + '"><td title="' + esc(r.blk || '') + '">' + esc(r.kind) + (r.blk ? ' <span class="faint">· ' + esc(r.blk) + '</span>' : '') + '</td><td><span class="tag warn">' + enc + '</span></td><td class="mono" style="color:var(--c-danger);font-size:var(--fs-xs)">' + esc(TC.rawText(d, { s:r.s, style:r.style }).replace(/\u00AD/g, '-')) + '</td><td style="font-family:\'Times New Roman\', serif">' + esc(r.s) + '</td><td class="num">' + (99 - i % 4) + '%</td></tr>'; }).join('') + '</tbody></table>'; }
    $('#fp-q', dlg.el).addEventListener('input', function(){ q = this.value; render(); });
    $('#fp-l', dlg.el).addEventListener('click', function(e){ var r = e.target.closest('[data-fz]'); if (!r) return; var x = rows[Number(r.dataset.fz)]; if (x.sp !== d.space) A().setSpace(x.sp); d.found = [x.e.id]; TC.zoomBB(x.e._bb); });
    $('#fp-go', dlg.el).addEventListener('click', function(){ dlg.close(); SC.fontConvert(d); });
    render();
  };
  SC.fontConvert = function(d){
    var n = legacyRows(d).length;
    if (TC.can('PRO')){ TC.toast('Đã chuyển ' + TC.n(n) + ' đối tượng sang Unicode trong bản vẽ (một bước hoàn tác).', { log:false }); return; }
    var dlg = TC.dialog({ id:'fdconv', title:'Chuyển sang Unicode và lưu bản sao', cmd:'FIXFONT', code:'UI-J02', w:560,
      body:'<div class="stack"><div class="note">' + ic('info') + '<span>Viewer Pro không sửa tệp gốc. Bản sao dùng font Unicode, mở đúng trên mọi máy, kể cả AutoCAD không có font TCVN3, VNI.</span></div>' +
        '<div class="frow" style="--lab-w:110px"><span class="lab">Chuyển</span><span><b>' + TC.n(n) + '</b> đối tượng chữ · bảng mã tự nhận diện · TCVN3 → TrionVN-Times, VNI → TrionVN-Arial</span>' +
        '<label class="lab" for="fc-path">Lưu bản sao</label><span class="hrow" style="flex-wrap:nowrap"><input class="inp grow mono" id="fc-path" value="' + esc(d.path.replace(/\.(\w+)$/, '_unicode.$1')) + '" style="font-size:var(--fs-xs)"><button class="btn" type="button" id="fc-br">Duyệt…</button></span>' +
        '<span class="lab">Định dạng</span><select class="inp" style="width:220px">' + opts(['Giữ nguyên (' + d.ver + ')','DWG 2018','DWG 2013','DWG 2010','DWG 2007','DWG 2004'], 'Giữ nguyên (' + d.ver + ')') + '</select>' +
        '<span></span><label class="chk"><input type="checkbox" id="fc-open" checked> Mở bản sao sau khi chuyển</label></div><div class="prog" id="fc-p" hidden><i style="width:0%"></i></div></div>',
      foot:'<button class="btn ghost" type="button" id="fc-prev">Xem trước thay đổi</button><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="fc-go">Chuyển và lưu bản sao</button>' });
    $('#fc-br', dlg.el).addEventListener('click', function(){ TC.toast('Hộp thoại Lưu thành của Windows.', { type:'info', log:false }); });
    $('#fc-prev', dlg.el).addEventListener('click', function(){ dlg.close(); SC.fontPreview(d); });
    $('#fc-go', dlg.el).addEventListener('click', function(){
      var path = $('#fc-path', dlg.el).value, open = $('#fc-open', dlg.el).checked, p = $('#fc-p', dlg.el), k = 0; p.hidden = false; this.disabled = true;
      var t = setInterval(function(){ k += 20; $('i', p).style.width = k + '%'; if (k < 100) return; clearInterval(t); dlg.close(true);
        var nm = path.split('\\').pop();
        TC.toast('Đã lưu bản sao <b>' + esc(nm) + '</b>: ' + TC.n(n) + ' đối tượng chữ sang Unicode. Tệp gốc giữ nguyên.', { act:open ? null : { label:'Mở bản sao', fn:function(){ openCopy(d, nm, path); } }, kind:'task' });
        if (open) openCopy(d, nm, path);
      }, 160);
    });
  };
  function openCopy(d, nm, path){
    var ex = A().docs.filter(function(x){ return x.name === nm; })[0]; if (ex){ A().openDoc(ex.id); return; }
    var nd = TC.buildDocs()[d.id === 'd2' ? 1 : 0];
    nd.id = d.id + 'u' + Date.now() % 1000; nd.name = nm; nd.path = path; nd.modified = TC.date() + ' (bản sao)'; nd.savedBy = 'TrionCAD Viewer Pro';
    nd.styles.forEach(function(s){ if (s.enc === 'TCVN3' || s.enc === 'VNI'){ s.font = s.enc === 'VNI' ? 'TrionVN-Arial' : 'TrionVN-Times'; s.kind = 'TTF'; s.enc = 'Unicode'; s.missing = false; } });
    TC.prepDoc(nd); A().docs.push(nd); A().openDoc(nd.id);
    TC.cmdline.msg('Đã mở bản sao ' + esc(nm) + '. Bác sĩ Font: bản vẽ dùng Unicode và đủ font.');
  }

  /* ================= Attribute với Excel UI-J15 ================= */
  function attInstances(d, scope){
    var out = [];
    function walk(list, sp){ list.forEach(function(e){ if (e.t === 'insert' && hasAtt(d, e.name)) out.push({ e:e, sp:sp }); }); }
    if (scope === 'cur'){ if (d.space === 'model') walk(d.ents, 'model'); else walk(d.layouts[d.space].ents, d.space); }
    else { walk(d.ents, 'model'); d.layouts.forEach(function(L, i){ walk(L.ents, i); }); }
    return out;
  }
  function tagsOf(d, names){ var t = []; Object.keys(names).forEach(function(n){ if (!names[n] || !d.blocks[n]) return; d.blocks[n].ents.forEach(function(a){ if (a.t === 'attdef' && t.indexOf(a.tag) < 0) t.push(a.tag); }); }); return t; }
  SC.attOut = function(){
    var d = D(); if (!d) return;
    var blks = Object.keys(d.blocks).filter(function(n){ return hasAtt(d, n); });
    var st = { by:'name', names:{}, scope:'all', tags:null, fmt:'XLSX', hide:true, open:true, picked:null };
    blks.forEach(function(n){ st.names[n] = true; });
    var dlg = TC.dialog({ id:'attout', title:'Xuất attribute ra Excel', cmd:'TCATTOUT', code:'UI-J15', w:960, body:SC.attTabs('out') + '<div class="att" id="at-b"></div>',
      foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" data-at="go" id="at-go">Xuất</button>' });
    function rows(){ var list = st.by === 'sel' && st.picked ? st.picked : attInstances(d, st.scope).filter(function(it){ return st.names[it.e.name]; }); return list; }
    function render(){
      var R = rows(), tags = tagsOf(d, st.by === 'sel' && st.picked ? st.picked.reduce(function(o, it){ o[it.e.name] = true; return o; }, {}) : st.names);
      if (!st.tags) { st.tags = {}; tags.forEach(function(t){ st.tags[t] = true; }); }
      var useT = tags.filter(function(t){ return st.tags[t] !== false; }), cols = (st.hide ? [] : ['Handle']).concat(['Block', 'Layout']).concat(useT);
      $('#at-b', dlg.el).innerHTML = '<div class="stack">' +
        '<div class="grp"><span class="gh">Block</span><label class="chk"><input type="radio" name="at-by" value="name"' + (st.by === 'name' ? ' checked' : '') + '> Theo tên block</label>' + (st.by === 'name' ? '<div class="stack" style="gap:4px;padding-left:20px">' + blks.map(function(n){ var c = attInstances(d, st.scope).filter(function(it){ return it.e.name === n; }).length; return '<label class="chk"><input type="checkbox" data-atb="' + esc(n) + '"' + (st.names[n] ? ' checked' : '') + '> ' + esc(n) + ' <span class="faint">' + c + ' block · ' + tagsOf(d, (function(){ var o = {}; o[n] = true; return o; })()).length + ' tag</span></label>'; }).join('') + '</div>' : '') +
          '<label class="chk"><input type="radio" name="at-by" value="sel"' + (st.by === 'sel' ? ' checked' : '') + '> Theo vùng chọn</label>' + (st.by === 'sel' ? '<span class="hrow" style="padding-left:20px"><button class="btn sm" type="button" data-at="pick">' + ic('select') + 'Chọn trên bản vẽ &lt;</button><span class="faint" style="font-size:var(--fs-s)">' + (st.picked ? st.picked.length + ' block có attribute' : 'Chưa chọn') + '</span></span>' : '') + '</div>' +
        '<div class="grp"><span class="gh">Phạm vi</span>' + seg('atscope', ['Cả bản vẽ', 'Layout hiện tại'], st.scope === 'all' ? 'Cả bản vẽ' : 'Layout hiện tại') + '<span class="faint" style="font-size:var(--fs-xs)">Hiện tại: ' + esc(where(d, d.space)) + '</span></div>' +
        '<div class="grp"><span class="gh">Tag cần xuất</span><div class="at-tags">' + tags.map(function(t){ return '<label class="chk"><input type="checkbox" data-att="' + t + '"' + (st.tags[t] !== false ? ' checked' : '') + '> <span class="mono">' + t + '</span></label>'; }).join('') + '</div><span class="hrow"><button class="btn sm ghost" type="button" data-at="tall">Chọn tất cả</button><button class="btn sm ghost" type="button" data-at="tnone">Bỏ chọn</button></span></div></div>' +
        '<div class="stack"><div class="grp" style="min-width:0"><span class="gh">Xem trước tệp Excel · ' + R.length + ' dòng × ' + (cols.length + (st.hide ? 1 : 0)) + ' cột</span><div class="tblwrap at-prev"><table class="tbl" style="width:auto;min-width:100%"><thead><tr>' + (st.hide ? '<th class="faint" title="Cột Handle bị ẩn trong Excel, vẫn giữ để nhập lại đúng đối tượng">Handle (ẩn)</th>' : '') + cols.map(function(c){ return '<th>' + c + '</th>'; }).join('') + '</tr></thead><tbody>' +
          R.map(function(it){ var a = attrsOf(d, it.e); return '<tr>' + (st.hide ? '<td class="faint mono">' + handle(it.e) + '</td>' : '') + cols.map(function(c){ return '<td' + (c === 'Handle' ? ' class="mono"' : '') + '>' + esc(c === 'Handle' ? handle(it.e) : c === 'Block' ? it.e.name : c === 'Layout' ? where(d, it.sp) : a[c]) + '</td>'; }).join('') + '</tr>'; }).join('') + (R.length ? '' : '<tr><td colspan="' + cols.length + '" class="faint">Không có block nào khớp.</td></tr>') + '</tbody></table></div></div>' +
          '<div class="grp"><span class="gh">Tệp</span><div class="frow" style="--lab-w:84px"><span class="lab">Định dạng</span>' + seg('atfmt', ['XLSX', 'CSV (UTF-8)'], st.fmt) + '<label class="lab" for="at-file">Tên tệp</label><input class="inp mono" id="at-file" value="' + esc('Khung-ten_' + ascii(base(d.name)) + (st.fmt === 'XLSX' ? '.xlsx' : '.csv')) + '"></div>' +
          '<label class="chk"><input type="checkbox" id="at-hide"' + (st.hide ? ' checked' : '') + '> Ẩn cột Handle trong Excel (vẫn giữ để nhập lại đúng đối tượng)</label><label class="chk"><input type="checkbox" id="at-open"' + (st.open ? ' checked' : '') + '> Mở tệp sau khi xuất</label></div></div>';
      $('#at-go', dlg.el).textContent = 'Xuất ' + R.length + ' dòng'; $('#at-go', dlg.el).disabled = !R.length;
    }
    setTimeout(function(){ recenter(dlg); }, 0);
    dlg.el.addEventListener('click', function(e){
      var s = e.target.closest('[data-seg="atscope"] button'); if (s){ st.scope = s.dataset.v === 'Cả bản vẽ' ? 'all' : 'cur'; render(); return; }
      var f = e.target.closest('[data-seg="atfmt"] button'); if (f){ st.fmt = f.dataset.v; render(); return; }
      var b = e.target.closest('[data-at]'); if (!b) return; var k = b.dataset.at;
      if (k === 'go'){ var nm = $('#at-file', dlg.el).value, n = rows().length; SC.attLast = nm; dlg.close(true); TC.toast('Đã xuất <span class="mono">' + esc(nm) + '</span>: ' + n + ' dòng. ' + (TC.can('PRO') ? 'Sửa trong Excel rồi dùng Nhập từ Excel để cập nhật bản vẽ.' : 'Nhập lại vào bản vẽ có trong TrionCAD Pro.'), { act:{ label:'Mở tệp', fn:function(){} }, kind:'task' }); TC.cmdline.msg('TCATTOUT: ' + n + ' block → ' + esc(nm)); }
      else if (k === 'tall' || k === 'tnone'){ Object.keys(st.tags).forEach(function(t){ st.tags[t] = k === 'tall'; }); render(); }
      else if (k === 'pick'){ dlg.hide(true); TC.cmdline.msg('Chọn các khung tên (block có attribute), Enter để xong. Esc để quay lại.');
        var fin = function(ids){ document.removeEventListener('keydown', keyF, true); st.picked = (ids || []).map(function(id){ return d.byId[id]; }).filter(function(x){ return x && x.t === 'insert' && hasAtt(d, x.name); }).map(function(x){ return { e:x, sp:x._paper ? d.space : 'model' }; }); TC.cad.clearSel(); dlg.hide(false); st.tags = null; render(); };
        var keyF = function(ev){ if (ev.key === 'Enter' || ev.key === 'Escape'){ ev.preventDefault(); ev.stopPropagation(); TC.cad.endSel(); } };
        document.addEventListener('keydown', keyF, true); TC.cad.getSel({}).then(fin); }
    });
    dlg.el.addEventListener('change', function(e){
      var t = e.target;
      if (t.name === 'at-by'){ st.by = t.value; st.tags = null; render(); }
      else if (t.dataset.atb){ st.names[t.dataset.atb] = t.checked; st.tags = null; render(); }
      else if (t.dataset.att){ st.tags[t.dataset.att] = t.checked; render(); }
      else if (t.id === 'at-hide'){ st.hide = t.checked; render(); }
      else if (t.id === 'at-open') st.open = t.checked;
    });
    render();
  };
  // lối Xuất ↔ Nhập liền mạch: tab đầu hộp thoại, giữ vị trí khi chuyển; Nhập (TCATTIN, screens12.js) cần Pro
  SC.attTabs = function(cur){ return '<div class="tabs at-tabs" role="tablist" aria-label="Attribute với Excel">' + [['out', 'stats', 'Xuất ra Excel', ''], ['in', 'open', 'Nhập từ Excel', proLock()]].map(function(t){ return '<button class="tab" role="tab" type="button" data-attab="' + t[0] + '" aria-selected="' + (t[0] === cur) + '">' + ic(t[1]) + t[2] + t[3] + '</button>'; }).join('') + '</div>'; };
  document.addEventListener('click', function(e){ var t = e.target.closest('[data-attab]'); if (!t || t.getAttribute('aria-selected') === 'true') return; if (t.dataset.attab === 'in' && !TC.can('PRO')){ SC.upgrade('Nhập attribute từ Excel', 'PRO'); return; }
    var old = TC.topDialog(), x = old.el.style.left, y = old.el.style.top; old.close(); (t.dataset.attab === 'in' ? SC.attIn : SC.attOut)(); var nw = TC.topDialog(); if (nw && nw !== old) setTimeout(function(){ nw.el.style.left = x; nw.el.style.top = Math.max(12, Math.min(parseFloat(y), innerHeight - nw.el.offsetHeight - 12)) + 'px'; }, 0); });
  SC.att = { inst:attInstances, tags:tagsOf, attrsOf:attrsOf, hasAtt:hasAtt, handle:handle, where:where, ascii:ascii, base:base };
  SC.zoomBeside = zoomBeside;
  IMPL.TCATTOUT = function(){ SC.attOut(); };
  IMPL.TCATTIN = function(){ SC.attIn(); };

  /* ================= Giấy phép: kích hoạt UI-L01, offline UI-L02, trạng thái UI-L03 ================= */
  var DEV = 'LAPTOP-AN-KT';
  var ACT_TABS = [['acc','Tài khoản TrionCAD','user'],['key','Nhập key','key'],['srv','Máy chủ giấy phép','globe'],['edu','Giáo dục','book'],['off','Kích hoạt offline','qr']];
  var SIM = [['ok','Thành công'],['key','Key sai'],['limit','Hết lượt kích hoạt'],['net','Không có mạng'],['seats','Hết ghế floating']];
  function qrSVG(seed, size){
    var n = 25, s = seed >>> 0, cells = '', rnd = function(){ s = (s * 1664525 + 1013904223) >>> 0; return s / 4294967296; };
    var finder = function(x, y){ return x < 7 && y < 7 || x >= n - 7 && y < 7 || x < 7 && y >= n - 7; };
    for (var y = 0; y < n; y++) for (var x = 0; x < n; x++){
      var on; if (finder(x, y)){ var fx = x < 7 ? x : x - (n - 7), fy = y < 7 ? y : y - (n - 7); on = fx === 0 || fx === 6 || fy === 0 || fy === 6 || (fx >= 2 && fx <= 4 && fy >= 2 && fy <= 4); } else on = rnd() > .52;
      if (on) cells += 'M' + x + ' ' + y + 'h1v1h-1z';
    }
    return '<svg class="qr" viewBox="-2 -2 ' + (n + 4) + ' ' + (n + 4) + '" width="' + size + '" height="' + size + '" role="img" aria-label="Mã QR kích hoạt offline"><rect x="-2" y="-2" width="' + (n + 4) + '" height="' + (n + 4) + '" fill="#FFFFFF"/><path d="' + cells + '" fill="#101214"/></svg>';
  }
  SC.qrSVG = qrSVG;
  SC.activate = function(tab){
    var st = { tab:ACT_TABS.some(function(t){ return t[0] === tab; }) ? tab : 'acc', sim:'ok', res:null, off:0, edu:0, key:'' };
    var dlg = TC.dialog({ id:'act', title:'Kích hoạt TrionCAD', code:'UI-L01', w:760, h:520, flush:true,
      body:'<div class="vtabs" style="height:100%"><div class="vt-list" role="tablist" id="ac-tabs"></div><div class="vt-body" id="ac-body"></div></div>', foot:'<span></span>' });
    function edOf(){ var k = st.key.slice(0, 4); return st.tab === 'edu' ? 'edu' : st.tab === 'srv' ? 'ent' : k === 'TCPR' ? 'pro' : k === 'TCEN' ? 'ent' : 'vp'; }
    function render(){
      $('#ac-tabs', dlg.el).innerHTML = ACT_TABS.map(function(t){ return '<button class="vt" role="tab" type="button" data-act="' + t[0] + '" aria-selected="' + (st.tab === t[0]) + '"' + (st.res === 'ok' ? ' disabled' : '') + '>' + ic(t[2]) + t[1] + '</button>'; }).join('') + '<div class="faint" style="font-size:var(--fs-xs);padding:10px 8px;line-height:1.5">Máy này: <b class="mono">' + DEV + '</b><br>Windows 11 · TrionCAD 1.0.2</div>';
      var B = $('#ac-body', dlg.el), f = $('.dlg-f', dlg.el), err = st.res && st.res !== 'ok' ? errBox(st.res) : '';
      if (st.res === 'ok'){ var ed = TC.EDITIONS[edOf()];
        B.innerHTML = '<div class="act-ok">' + ic('okc') + '<h3>Đã kích hoạt ' + esc(ed.name) + '</h3><div class="frow" style="--lab-w:150px;text-align:left"><span class="lab">Phiên bản</span><span>' + esc(ed.name) + ' <span class="ed" data-ed="' + edOf() + '">' + esc(ed.badge) + '</span></span><span class="lab">Thời hạn</span><span>' + (edOf() === 'edu' ? 'Giáo dục · 1 năm, đến 19/10/2027' : edOf() === 'ent' ? 'Ghế floating · máy chủ license.congty.local' : 'Thuê bao năm · đến 19/10/2027') + '</span><span class="lab">Thiết bị</span><span class="mono">' + DEV + ' (1/2 máy)</span><span class="lab">Quyền cập nhật</span><span>Đến 19/10/2027</span></div></div>';
        f.innerHTML = '<span class="grow"></span><button class="btn primary" type="button" data-ac="start">Bắt đầu dùng</button>'; return; }
      var simSel = '<label class="proto-sel" data-tip="Chỉ có trong prototype" data-desc="Chọn kết quả máy chủ trả về để xem từng trạng thái."><span>Mô phỏng</span><select id="ac-sim" aria-label="Mô phỏng kết quả">' + SIM.filter(function(s){ return st.tab === 'srv' ? s[0] !== 'limit' && s[0] !== 'key' : s[0] !== 'seats'; }).map(function(s){ return '<option value="' + s[0] + '"' + (st.sim === s[0] ? ' selected' : '') + '>' + s[1] + '</option>'; }).join('') + '</select></label>';
      var main = { acc:'Đăng nhập và kích hoạt', key:'Kích hoạt', srv:'Kết nối máy chủ', edu:st.edu ? 'Xác minh' : 'Gửi mã xác minh', off:'' }[st.tab];
      if (st.tab === 'acc') B.innerHTML = '<div class="stack act-form"><h3>Đăng nhập tài khoản TrionCAD</h3><p class="muted">Giấy phép đã mua bằng tài khoản này được kích hoạt cho máy hiện tại.</p>' + err +
        '<div class="field"><label for="ac-mail">Email</label><input class="inp" id="ac-mail" type="email" autocomplete="email" value="an.nguyen@congty.vn"></div><div class="field"><label for="ac-pw">Mật khẩu</label><input class="inp" id="ac-pw" type="password" autocomplete="current-password" value="••••••••••"></div>' +
        '<div class="hrow"><a href="#" data-ac="forgot">Quên mật khẩu?</a><span class="faint">·</span><a href="#" data-ac="browser">Đăng nhập qua trình duyệt</a></div><p class="faint" style="font-size:var(--fs-s)">Chưa có tài khoản? <a href="#" data-ac="buy">Mua hoặc dùng thử</a></p></div>';
      else if (st.tab === 'key') B.innerHTML = '<div class="stack act-form"><h3>Nhập key</h3><p class="muted">Key có trong email mua hàng hoặc hóa đơn, dạng <span class="mono">TCVP-XXXX-XXXX-XXXX</span>.</p>' + err + '<div class="field"><label for="ac-key">Key</label><input class="inp mono act-key" id="ac-key" value="' + esc(st.key) + '" placeholder="TCVP-7K2Q-M9XD-4HFA" maxlength="19" autocomplete="off" spellcheck="false" aria-describedby="ac-keyh"><span class="fmsg hint" id="ac-keyh">Chữ hoa, không phân biệt có gạch nối hay không. Dán cả key một lần được.</span></div></div>';
      else if (st.tab === 'srv') B.innerHTML = '<div class="stack act-form"><h3>Máy chủ giấy phép <span class="ed" data-ed="ent">Doanh nghiệp</span></h3><p class="muted">Dùng ghế floating của công ty. Hỏi bộ phận IT địa chỉ máy chủ.</p>' + err + '<div class="field"><label for="ac-srv">Địa chỉ máy chủ</label><span class="hrow" style="flex-wrap:nowrap"><input class="inp mono grow" id="ac-srv" value="license.congty.local:27100"><button class="btn" type="button" data-ac="scan">Tự tìm trong mạng</button></span></div><label class="chk"><input type="checkbox" checked> Kết nối lại tự động khi mở TrionCAD</label></div>';
      else if (st.tab === 'edu') B.innerHTML = '<div class="stack act-form"><h3>Giáo dục <span class="ed" data-ed="edu">Miễn phí</span></h3><p class="muted">Cho sinh viên, giảng viên; chỉ dùng phi thương mại. Hạn 1 năm, gia hạn khi vẫn đang học hoặc dạy. Không chèn dấu vào bản vẽ.</p>' + err +
        (st.edu ? '<div class="field"><label for="ac-code">Mã xác minh 6 số đã gửi tới <b>' + esc(st.mail) + '</b></label><input class="inp mono" id="ac-code" inputmode="numeric" maxlength="6" placeholder="000000" style="width:140px;letter-spacing:.3em"></div><a href="#" data-ac="resend">Gửi lại mã</a>'
          : '<div class="field"><label for="ac-edu">Email của trường</label><input class="inp" id="ac-edu" type="email" placeholder="ten@sv.huce.edu.vn" value="' + esc(st.mail || '') + '"><span class="fmsg hint">Đuôi .edu.vn hoặc tên miền trường đã đăng ký với TrionCAD.</span></div><div class="field"><label for="ac-scode">Hoặc mã do trường cấp</label><input class="inp mono" id="ac-scode" placeholder="HUCE-2026-XXXX" style="width:220px"></div>') + '</div>';
      else B.innerHTML = offHTML();
      f.innerHTML = simSel + '<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button>' + (main ? '<button class="btn primary" type="button" data-ac="go">' + main + '</button>' : '');
    }
    function errBox(k){
      var M = { key:['err', 'Key không đúng. Kiểm tra các ký tự dễ nhầm: số 0 và chữ O, số 1 và chữ I.', ''],
        limit:['err', 'Key đã kích hoạt trên <b>2/2 máy</b>. Hủy kích hoạt một máy cũ ở trang Quản lý thiết bị rồi thử lại.', '<button class="btn sm" type="button" data-ac="devices">' + ic('ext') + 'Quản lý thiết bị</button><button class="btn sm" type="button" data-ac="go">Thử lại</button>'],
        net:['warn', 'Không kết nối được máy chủ kích hoạt <span class="mono">trioncad.vn</span>. Kiểm tra mạng, proxy của công ty, hoặc dùng kích hoạt offline.', '<button class="btn sm" type="button" data-ac="go">Thử lại</button><button class="btn sm" type="button" data-act="off">' + ic('qr') + 'Kích hoạt offline</button>'],
        seats:['warn', '<b>Đã dùng hết 10/10 ghế.</b> Mọi ghế floating của công ty đang có người dùng.', '<button class="btn sm" type="button" data-ac="go">Thử lại</button><button class="btn sm" type="button" data-ac="notify">' + ic('bell') + 'Báo khi có ghế trống</button><button class="btn sm" type="button" data-ac="viewer">Mở ở chế độ Viewer</button>'] }[k];
      return '<div class="note ' + M[0] + '" role="alert">' + ic(M[0] === 'err' ? 'error' : 'warn') + '<span class="stack" style="gap:6px"><span>' + M[1] + '</span>' + (M[2] ? '<span class="hrow">' + M[2] + '</span>' : '') + '</span></div>';
    }
    function offHTML(){
      var s = st.off;
      return '<div class="stack act-form"><h3>Kích hoạt offline</h3><p class="muted">Cho máy không có mạng (công trường, phòng máy nội bộ). Làm 3 bước, mỗi bước một lần.</p><ol class="off-steps">' +
        '<li class="' + (s > 0 ? 'done' : 'cur') + '"><b>Tạo tệp yêu cầu</b>' + (s > 0 ? '<div class="off-req"><div class="stack" style="gap:6px"><span class="hrow">' + ic('file') + '<span class="mono">' + DEV + '.req</span><span class="faint">2 KB · ' + TC.date() + '</span></span><span class="hrow"><button class="btn sm" type="button" data-ac="savereq">' + ic('save') + 'Lưu tệp…</button><button class="btn sm ghost" type="button" data-ac="copyreq">' + ic('copyclip') + 'Sao chép mã máy</button></span><span class="faint" style="font-size:var(--fs-xs)">Hoặc quét mã QR bằng điện thoại: trang kích hoạt mở sẵn mã máy này.</span></div>' + qrSVG(7331, 116) + '</div>' : '<span class="hrow"><button class="btn sm primary" type="button" data-ac="mkreq">Tạo tệp yêu cầu</button><span class="faint" style="font-size:var(--fs-xs)">Tệp chứa mã máy, không chứa dữ liệu bản vẽ.</span></span>') + '</li>' +
        '<li class="' + (s > 1 ? 'done' : s === 1 ? 'cur' : '') + '"><b>Lấy tệp phản hồi trên máy có mạng</b><span class="muted" style="font-size:var(--fs-s)">Mở <span class="mono">trioncad.vn/kich-hoat</span>, đăng nhập, tải tệp .req lên (hoặc quét QR), rồi tải về tệp phản hồi <span class="mono">.lic</span>.</span>' + (s === 1 ? '<span class="hrow"><button class="btn sm" type="button" data-ac="gotlic">Đã có tệp .lic</button></span>' : '') + '</li>' +
        '<li class="' + (s > 1 ? 'cur' : '') + '"><b>Nhập tệp phản hồi</b>' + (s > 1 ? '<button class="drop" type="button" data-ac="lic">' + ic('open') + '<span>Chọn tệp <span class="mono">.lic</span> hoặc kéo thả vào đây</span></button>' : '<span class="faint" style="font-size:var(--fs-s)">Sau bước 2.</span>') + '</li></ol></div>';
    }
    function go(){
      if (st.tab === 'key'){ var v = $('#ac-key', dlg.el).value.toUpperCase().replace(/[^A-Z0-9]/g, ''); if (!/^TC(VP|PR|EN)[A-Z0-9]{12}$/.test(v) && st.sim === 'ok'){ var i = $('#ac-key', dlg.el); i.classList.add('err'); i.setAttribute('aria-invalid', 'true'); $('#ac-keyh', dlg.el).className = 'fmsg err'; $('#ac-keyh', dlg.el).innerHTML = ic('warn', 'xs') + 'Key cần 16 ký tự, bắt đầu bằng TCVP, TCPR hoặc TCEN.'; return; } st.key = v.replace(/(.{4})(?=.)/g, '$1-'); }
      if (st.tab === 'edu' && !st.edu){ var m = $('#ac-edu', dlg.el).value.trim(), c = $('#ac-scode', dlg.el).value.trim(); if (!c && !/\.edu\.vn$/i.test(m)){ st.res = null; render(); var e2 = $('#ac-edu', dlg.el); e2.classList.add('err'); e2.nextElementSibling.className = 'fmsg err'; e2.nextElementSibling.innerHTML = ic('warn', 'xs') + 'Email cần đuôi .edu.vn, hoặc nhập mã do trường cấp.'; return; } st.mail = m || 'mã ' + c; st.edu = 1; st.res = null; render(); return; }
      var btn = $('[data-ac="go"]', dlg.el); if (btn){ btn.disabled = true; btn.innerHTML = '<span class="spin"></span>Đang kích hoạt…'; }
      setTimeout(function(){ st.res = st.sim; render(); }, 700);
    }
    dlg.el.addEventListener('click', function(e){
      var t = e.target.closest('[data-act]'); if (t && !t.disabled){ st.tab = t.dataset.act; st.res = null; if (st.tab === 'srv' && (st.sim === 'key' || st.sim === 'limit')) st.sim = 'ok'; if (st.tab !== 'srv' && st.sim === 'seats') st.sim = 'ok'; render(); return; }
      var a = e.target.closest('[data-ac]'); if (!a) return; e.preventDefault(); var k = a.dataset.ac;
      if (k === 'go') go();
      else if (k === 'start'){ var ed = edOf(); dlg.close(true); TC.S.edition = ed; TC.S.licSim = 'ok'; TC.save('edition', ed); TC.emit('edition'); TC.toast('Đã kích hoạt <b>' + esc(TC.EDITIONS[ed].name) + '</b> trên ' + DEV + '.', { kind:'license' }); }
      else if (k === 'mkreq'){ st.off = 1; render(); }
      else if (k === 'gotlic'){ st.off = 2; render(); }
      else if (k === 'lic'){ a.innerHTML = '<span class="spin"></span>Đang đọc ' + DEV + '.lic…'; setTimeout(function(){ st.res = 'ok'; render(); }, 700); }
      else if (k === 'savereq') TC.toast('Đã lưu <span class="mono">' + DEV + '.req</span>. Chép sang USB hoặc gửi qua email cho máy có mạng.', { log:false });
      else if (k === 'copyreq') TC.copyText('TRN-' + DEV + '-8F3A-21C9-7D04');
      else if (k === 'devices' || k === 'buy') TC.openWeb('thanh-toan.html' + (k === 'devices' ? '#tai-khoan' : ''));
      else if (k === 'browser') TC.toast('Mở trình duyệt để đăng nhập (Google, Zalo, email); TrionCAD tự nhận kết quả.', { type:'info', log:false });
      else if (k === 'forgot') TC.toast('Đã gửi liên kết đặt lại mật khẩu tới an.nguyen@congty.vn.', { type:'info', log:false });
      else if (k === 'scan') { $('#ac-srv', dlg.el).value = 'license.congty.local:27100'; TC.toast('Tìm thấy 1 máy chủ giấy phép trong mạng nội bộ.', { log:false }); }
      else if (k === 'resend') TC.toast('Đã gửi lại mã tới ' + esc(st.mail) + '.', { log:false });
      else if (k === 'notify'){ dlg.close(); TC.toast('Sẽ báo khi có ghế trống. Trong lúc chờ, TrionCAD mở ở chế độ Viewer.', { type:'info', kind:'license' }); }
      else if (k === 'viewer'){ dlg.close(); TC.S.edition = 'viewer'; TC.emit('edition'); TC.toast('Đang dùng chế độ Viewer: xem, đo, in vẫn đầy đủ.', { type:'info', log:false }); }
    });
    dlg.el.addEventListener('change', function(e){ if (e.target.id === 'ac-sim'){ st.sim = e.target.value; st.res = null; render(); } });
    dlg.el.addEventListener('input', function(e){ if (e.target.id === 'ac-key'){ var i = e.target, raw = i.value.toUpperCase().replace(/[^A-Z0-9]/g, '').slice(0, 16); i.value = raw.replace(/(.{4})(?=.)/g, '$1-'); st.key = i.value; i.classList.remove('err'); } });
    render();
  };
  // UI-L03: thông tin, thiết bị, floating, các trạng thái cần báo
  var LIC_SIM = [['ok','Bình thường'],['expiring','Sắp hết hạn (7 ngày)'],['expired','Đã hết hạn'],['offline','Lâu chưa kết nối']];
  SC.license = function(){
    var dlg = TC.dialog({ id:'lic', title:'Trạng thái giấy phép', code:'UI-L03', w:680, body:'<div class="stack" id="li-b"></div>', foot:'<span></span>' });
    var devs = [[DEV + ' (máy này)', 'Windows 11', '18/10/2026'], ['PC-VANPHONG-02', 'Windows 10', '02/09/2026']];
    function render(){
      var e = TC.S.edition, s = TC.S.licSim, prev = TC.S.prevEd, shown = s === 'expired' && prev ? prev : e, sed = TC.EDITIONS[shown];
      var paid = shown !== 'viewer';
      var notice = s === 'expiring' && paid ? '<div class="note warn">' + ic('warn') + '<span class="stack" style="gap:6px"><span>Thuê bao còn <b>7 ngày</b> (hết hạn 26/10/2026). Hết hạn thì TrionCAD chuyển về Viewer: bản vẽ vẫn mở, xem và in được.</span><span class="hrow"><button class="btn sm primary" type="button" data-li="renew">Gia hạn</button></span></span></div>'
        : s === 'expired' && paid ? '<div class="note err">' + ic('error') + '<span class="stack" style="gap:6px"><span>Thuê bao ' + esc(sed.name) + ' đã hết hạn ngày 12/10/2026. Đã chuyển về <b>Viewer</b>; dữ liệu an toàn, bản vẽ vẫn mở và in được.</span><span class="hrow"><button class="btn sm primary" type="button" data-li="renew">Gia hạn để dùng lại</button></span></span></div>'
        : s === 'offline' && paid ? '<div class="note warn">' + ic('warn') + '<span class="stack" style="gap:6px"><span>Đã 25 ngày chưa kết nối máy chủ giấy phép. Giấy phép cần xác minh 30 ngày một lần: còn <b>' + TC.S.offDays + ' ngày</b> dùng offline, sau đó tạm chuyển về Viewer tới khi kết nối lại.</span><span class="hrow"><button class="btn sm" type="button" data-li="reconnect">' + ic('refresh') + 'Kết nối lại</button><button class="btn sm ghost" type="button" data-li="offline">Kích hoạt offline…</button></span></span></div>' : '';
      var info = !paid ? '<div class="frow" style="--lab-w:170px"><span class="lab">Loại</span><span>Miễn phí, không cần kích hoạt</span><span class="lab">Gồm</span><span>Mở, xem, đo, in, xuất PDF; Bác sĩ Font hiển thị đúng chữ</span><span class="lab">Cập nhật</span><span>Luôn miễn phí</span></div>'
        : '<div class="frow" style="--lab-w:170px"><span class="lab">Loại</span><span>' + (shown === 'trial' ? 'Dùng thử 30 ngày' : shown === 'edu' ? 'Giáo dục, phi thương mại' : shown === 'ent' ? 'Ghế floating, thuê bao năm' : 'Thuê bao năm') + '</span><span class="lab">Key</span><span class="hrow"><span class="mono">' + (shown === 'pro' ? 'TCPR' : shown === 'ent' ? 'TCEN' : 'TCVP') + '-••••-••••-4HFA</span><button class="ib sm" type="button" data-li="copykey" aria-label="Sao chép key" data-tip="Sao chép key">' + ic('copyclip') + '</button></span><span class="lab">Hạn dùng</span><span>' + (shown === 'trial' ? 'Còn ' + TC.S.trialDays + ' ngày (đến 30/10/2026)' : s === 'expired' ? '<span style="color:var(--c-danger)">Đã hết hạn 12/10/2026</span>' : s === 'expiring' ? '26/10/2026 · còn 7 ngày' : '18/10/2027') + '</span><span class="lab">Quyền cập nhật đến</span><span>' + (s === 'expired' ? '12/10/2026' : '18/10/2027') + '</span><span class="lab">Tài khoản</span><span>an.nguyen@congty.vn</span></div>';
      var devT = paid && shown !== 'ent' ? '<div class="gh">Thiết bị đã kích hoạt (' + devs.length + '/2)</div><div class="tblwrap"><table class="tbl"><colgroup><col><col style="width:110px"><col style="width:100px"><col style="width:120px"></colgroup><thead><tr><th>Tên máy</th><th>Hệ điều hành</th><th>Kích hoạt</th><th></th></tr></thead><tbody>' + devs.map(function(dv, i){ return '<tr><td>' + esc(dv[0]) + '</td><td>' + dv[1] + '</td><td>' + dv[2] + '</td><td><button class="btn sm ghost" type="button" data-li="deact" data-i="' + i + '">Hủy kích hoạt</button></td></tr>'; }).join('') + '</tbody></table></div><p class="faint" style="font-size:var(--fs-xs);margin:0">Đổi máy: hủy kích hoạt máy cũ rồi kích hoạt máy mới. Giấy phép vĩnh viễn tự kích hoạt lại được 2 lần mỗi năm.</p>' : '';
      var fl = shown === 'ent' ? '<div class="grp"><span class="gh">Máy chủ giấy phép</span><div class="frow" style="--lab-w:150px"><span class="lab">Máy chủ</span><span class="hrow"><span class="mono">license.congty.local:27100</span><span class="tag ok">Đã kết nối</span></span><span class="lab">Ghế đang dùng</span><span class="hrow"><span class="prog" style="width:160px"><i style="width:70%"></i></span><span class="mono">7/10</span></span><span class="lab">Mượn offline</span><span class="hrow"><select class="inp" disabled style="width:110px"><option>7 ngày</option></select><button class="btn sm" type="button" disabled>Mượn giấy phép</button><span class="tag">P2</span></span></div></div>' : '';
      $('#li-b', dlg.el).innerHTML = '<div class="hrow" style="justify-content:space-between"><span class="hrow"><b style="font-size:var(--fs-l)">' + esc(sed.name) + '</b>' + A().edBadge() + '</span>' + (s === 'expired' && paid ? '<span class="faint" style="font-size:var(--fs-s)">Đang chạy: TrionCAD Viewer</span>' : '') + '</div>' + notice + info + fl + devT;
      $('.dlg-f', dlg.el).innerHTML = '<label class="proto-sel" data-tip="Chỉ có trong prototype"><span>Mô phỏng</span><select id="li-sim" aria-label="Mô phỏng trạng thái giấy phép">' + LIC_SIM.map(function(x){ return '<option value="' + x[0] + '"' + (s === x[0] ? ' selected' : '') + '>' + x[1] + '</option>'; }).join('') + '</select></label><span class="grow"></span>' +
        (paid ? '<button class="btn ghost" type="button" data-li="key">Đổi key…</button><button class="btn ghost" type="button" data-li="out">Đăng xuất</button>' : '<button class="btn" type="button" data-li="act">' + ic('key') + 'Kích hoạt, nhập key…</button>') + '<button class="btn" type="button" data-li="renew">' + ic('ext') + (paid ? 'Gia hạn, nâng cấp' : 'Mua Viewer Pro, Pro') + '</button><button class="btn primary" type="button" data-x="close">Đóng</button>';
    }
    dlg.el.addEventListener('click', function(e){
      var b = e.target.closest('[data-li]'); if (!b) return; var k = b.dataset.li;
      if (k === 'renew') TC.openWeb('thanh-toan.html' + (TC.S.edition === 'viewer' && !TC.S.prevEd ? '' : '#gia-han'));
      else if (k === 'key' || k === 'act' || k === 'offline'){ dlg.close(); SC.activate(k === 'offline' ? 'off' : k === 'key' ? 'key' : 'acc'); }
      else if (k === 'out'){ dlg.close(); SC.signOut(); }
      else if (k === 'copykey') TC.copyText((TC.S.edition === 'pro' ? 'TCPR' : 'TCVP') + '-7K2Q-M9XD-4HFA');
      else if (k === 'reconnect'){ b.innerHTML = '<span class="spin"></span>Đang kết nối…'; setTimeout(function(){ TC.S.licSim = 'ok'; TC.emit('edition'); render(); TC.toast('Đã xác minh giấy phép. Dùng offline được thêm 30 ngày.', { kind:'license' }); }, 700); }
      else if (k === 'deact'){ var i = Number(b.dataset.i); TC.confirm({ title:'Hủy kích hoạt thiết bị', msg:'Hủy kích hoạt <b>' + esc(devs[i][0]) + '</b>?', detail:i === 0 ? 'Máy này chuyển về Viewer ngay. Bản vẽ đang mở không bị đóng; kích hoạt lại bất cứ lúc nào.' : 'Máy đó chuyển về Viewer khi kết nối mạng lần tới. Bạn có thể kích hoạt một máy khác.', ok:'Hủy kích hoạt', danger:true }).then(function(y){ if (!y) return; devs.splice(i, 1); if (i === 0){ dlg.close(); TC.S.edition = 'viewer'; TC.emit('edition'); TC.toast('Đã hủy kích hoạt máy này. TrionCAD chạy ở chế độ Viewer.', { kind:'license' }); } else { render(); TC.toast('Đã hủy kích hoạt PC-VANPHONG-02. Còn trống 1 lượt kích hoạt.', { kind:'license' }); } }); }
    });
    dlg.el.addEventListener('change', function(e){ if (e.target.id !== 'li-sim') return; setLic(e.target.value); render(); });
    render(); recenter(dlg);
  };
  // chuyển trạng thái mô phỏng; hết hạn thì về Viewer, không khóa dữ liệu (04 §6.3)
  function setLic(v){
    var was = TC.S.licSim; TC.S.licSim = v;
    if (v === 'expired' && TC.S.edition !== 'viewer'){ TC.S.prevEd = TC.S.edition; TC.S.edition = 'viewer'; TC.toast('Thuê bao <b>' + esc(TC.EDITIONS[TC.S.prevEd].name) + '</b> đã hết hạn. TrionCAD chuyển về Viewer: bản vẽ vẫn mở, xem và in được.', { type:'warn', act:{ label:'Gia hạn', fn:function(){ TC.openWeb('thanh-toan.html#gia-han'); } }, kind:'license', sticky:true }); }
    else if (was === 'expired' && v !== 'expired' && TC.S.prevEd){ TC.S.edition = TC.S.prevEd; TC.S.prevEd = null; }
    if (v === 'expiring') TC.toast('Thuê bao còn 7 ngày. Gia hạn để không bị chuyển về Viewer.', { type:'warn', act:{ label:'Gia hạn', fn:function(){ TC.openWeb('thanh-toan.html#gia-han'); } }, kind:'license' });
    TC.emit('edition');
  }
  SC.signOut = function(){
    if (TC.S.edition === 'viewer'){ TC.toast('Viewer không cần đăng nhập.', { type:'info', log:false }); return; }
    TC.confirm({ title:'Đăng xuất', msg:'Đăng xuất tài khoản <b>an.nguyen@congty.vn</b> trên máy này?', detail:'Giấy phép được giải phóng để dùng ở máy khác. TrionCAD chuyển về Viewer; bản vẽ đang mở không bị đóng.', ok:'Đăng xuất' }).then(function(y){ if (!y) return; TC.S.edition = 'viewer'; TC.save('edition', 'viewer'); TC.emit('edition'); TC.toast('Đã đăng xuất. Đang dùng TrionCAD Viewer.', { type:'info', kind:'license' }); });
  };
})();
