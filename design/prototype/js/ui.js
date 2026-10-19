/* TrionCAD prototype · thành phần giao diện dùng chung: menu, tooltip, hộp thoại, xác nhận, toast, banner, ô nhập số (UI-C09). */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc;

  /* ---------- menu ngữ cảnh / thả xuống ---------- */
  var openMenus = [];
  function closeMenus(from){ while (openMenus.length > (from || 0)) { var m = openMenus.pop(); m.el.remove(); if (m.onClose) m.onClose(); } }
  TC.closeMenus = closeMenus;
  TC.menu = function(items, x, y, opts){
    opts = opts || {};
    var level = opts.level || 0; closeMenus(level);
    var el = TC.el('<div class="menu" role="menu"></div>');
    if (opts.minWidth) el.style.minWidth = opts.minWidth + 'px';
    items.forEach(function(it, i){
      if (!it) return;
      if (it.sep){ el.appendChild(TC.el('<div class="ms" role="separator"></div>')); return; }
      if (it.h){ el.appendChild(TC.el('<div class="mh">' + esc(it.h) + '</div>')); return; }
      var lockEd = it.need && !TC.can(it.need);
      var b = TC.el('<button class="mi' + (it.sub ? ' sub' : '') + '" role="menuitem" type="button"' + (it.dis ? ' aria-disabled="true"' : '') + '>' +
        (it.check != null ? '<span class="chkm">' + (it.check ? TC.ic('check', 's12') : '') + '</span>' : '<span class="ico">' + (it.icon ? TC.ic(it.icon) : '') + '</span>') +
        '<span>' + it.label + '</span>' + (lockEd ? '<span class="ed xs" data-ed="' + (it.need === 'VP' ? 'vp' : 'pro') + '">' + (it.need === 'VP' ? 'Viewer Pro' : 'Pro') + '</span>' : '') +
        (it.kbd ? '<span class="k">' + esc(TC.keyLabel ? TC.keyLabel(it.kbd) : it.kbd) + '</span>' : '') + '</button>');
      if (it.cmd) { b.dataset.tip = it.tipName || it.label.replace(/<[^>]+>/g, ''); b.dataset.cmd = it.cmd; }
      b.addEventListener('click', function(e){
        e.stopPropagation();
        if (it.dis) return;
        if (it.sub){ openSub(b, it.sub); return; }
        closeMenus(0);
        if (lockEd) { TC.screens.upgrade(it.feature || it.label.replace(/<[^>]+>/g, ''), it.need); return; }
        if (it.act) it.act();
      });
      b.addEventListener('mouseenter', function(){
        $$('.mi', el).forEach(function(n){ n.classList.toggle('is-hot', n === b); });
        if (it.sub) openSub(b, it.sub); else closeMenus(level + 1);
      });
      el.appendChild(b);
    });
    function openSub(b, sub){ var r = b.getBoundingClientRect(); TC.menu(sub, r.right - 2, r.top - 5, { level:level + 1, parentEl:b }); }
    document.body.appendChild(el);
    var w = el.offsetWidth, h = el.offsetHeight, vw = innerWidth, vh = innerHeight;
    if (opts.alignRight) x = x - w;
    if (x + w > vw - 4) x = opts.level ? Math.max(4, (opts.parentEl ? opts.parentEl.getBoundingClientRect().left : x) - w + 2) : vw - w - 4;
    if (y + h > vh - 4) y = Math.max(4, opts.up ? y - h : vh - h - 4);
    el.style.left = Math.max(4, x) + 'px'; el.style.top = Math.max(4, y) + 'px';
    var rec = { el:el, onClose:opts.onClose }; openMenus.push(rec);
    if (opts.focus !== false && !opts.level) { var f = $('.mi:not([aria-disabled])', el); if (f) setTimeout(function(){ f.focus(); }, 0); }
    el.addEventListener('keydown', function(e){
      var its = $$('.mi:not([aria-disabled])', el), i = its.indexOf(document.activeElement);
      if (e.key === 'ArrowDown' || e.key === 'ArrowUp'){ e.preventDefault(); i = (i + (e.key === 'ArrowDown' ? 1 : -1) + its.length) % its.length; its[i].focus(); }
      else if (e.key === 'Escape' || e.key === 'ArrowLeft'){ e.preventDefault(); e.stopPropagation(); closeMenus(e.key === 'Escape' && !level ? 0 : level); if (opts.parentEl) opts.parentEl.focus(); else if (opts.returnFocus) opts.returnFocus.focus(); }
      else if (e.key === 'ArrowRight' && document.activeElement.classList.contains('sub')){ document.activeElement.click(); var sm = openMenus[level + 1]; if (sm) { var f2 = $('.mi', sm.el); if (f2) f2.focus(); } }
    });
    return { el:el, close:function(){ closeMenus(level); } };
  };
  document.addEventListener('mousedown', function(e){ if (!e.target.closest('.menu')) closeMenus(0); }, true);
  window.addEventListener('blur', function(){ closeMenus(0); });

  /* ---------- tooltip 2 mức (07.1 §7.1) ---------- */
  var tipEl = null, tipT1 = 0, tipT2 = 0, tipFor = null;
  function hideTip(){ clearTimeout(tipT1); clearTimeout(tipT2); if (tipEl) { tipEl.remove(); tipEl = null; } tipFor = null; }
  TC.hideTip = hideTip;
  function showTip(t, level){
    var d = t.dataset; hideTip(); tipFor = t;
    var html = '<span class="tn">' + esc(d.tip) + (d.need && !TC.can(d.need) ? ' <span class="ed xs" data-ed="' + (d.need === 'VP' ? 'vp' : 'pro') + '">' + (d.need === 'VP' ? 'Viewer Pro' : 'Pro') + '</span>' : '') + '</span>';
    if (d.cmd || d.key) html += '<span class="tc">' + [d.cmd, d.key && TC.keyLabel ? TC.keyLabel(d.key) : d.key].filter(Boolean).map(esc).join(' · ') + '</span>';
    if (level === 2 && d.desc) html += '<span class="td">' + esc(d.desc) + '</span>';
    if (level === 2 && d.cmd) html += '<span class="tf">Nhấn F1 để xem trợ giúp</span>';
    tipEl = TC.el('<div class="tip" role="tooltip">' + html + '</div>');
    document.body.appendChild(tipEl);
    var r = t.getBoundingClientRect(), w = tipEl.offsetWidth, h = tipEl.offsetHeight;
    var x = TC.clamp(r.left, 4, innerWidth - w - 4), y = r.bottom + 6; if (y + h > innerHeight - 4) y = r.top - h - 6;
    tipEl.style.left = x + 'px'; tipEl.style.top = y + 'px';
    tipFor = t;
    if (level === 1 && (d.desc || d.cmd)) tipT2 = setTimeout(function(){ if (tipFor === t) showTip(t, 2); }, 1500);
  }
  document.addEventListener('mouseover', function(e){
    var t = e.target.closest('[data-tip]');
    if (!t || t === tipFor) return;
    hideTip(); tipFor = t;
    tipT1 = setTimeout(function(){ if (tipFor === t && document.contains(t)) showTip(t, 1); }, 450);
  });
  document.addEventListener('mouseout', function(e){ var t = e.target.closest('[data-tip]'); if (t && !t.contains(e.relatedTarget)) hideTip(); });
  document.addEventListener('mousedown', hideTip, true);
  document.addEventListener('keydown', function(e){ if (e.key === 'F1' && tipFor && tipFor.dataset.cmd){ e.preventDefault(); var c = tipFor.dataset.cmd; hideTip(); TC.openHelp && TC.openHelp(c); } });

  /* ---------- hộp thoại ---------- */
  var dlgStack = [];
  TC.dialog = function(o){
    if (o.id) { var ex = dlgStack.filter(function(d){ return d.id === o.id; })[0]; if (ex) { ex.el.focus(); return ex; } }
    TC.closeMenus(0); hideTip();
    var scrim = o.modal === false ? null : TC.el('<div class="scrim"></div>');
    var el = TC.el('<section class="dlg" role="dialog" aria-modal="' + (o.modal !== false) + '" tabindex="-1" aria-labelledby="dlgt-' + (o.id || 'x') + '">' +
      '<header class="dlg-h"><span class="t" id="dlgt-' + (o.id || 'x') + '">' + o.title + (o.cmd ? ' <span class="cmdn">' + esc(o.cmd) + '</span>' : '') + '</span>' +
      (o.code ? '<span class="tag">' + esc(o.code) + '</span>' : '') +
      (o.help !== false ? '<button class="ib" type="button" data-x="help" aria-label="Trợ giúp (F1)" data-tip="Trợ giúp" data-key="F1">' + TC.ic('help') + '</button>' : '') +
      '<button class="ib" type="button" data-x="close" aria-label="Đóng (Esc)">' + TC.ic('x') + '</button></header>' +
      '<div class="dlg-b' + (o.flush ? ' flush' : '') + '">' + (o.body || '') + '</div>' + (o.foot ? '<footer class="dlg-f">' + o.foot + '</footer>' : '') + '</section>');
    if (o.w) el.style.width = o.w + 'px';
    if (o.h) el.style.height = o.h + 'px';
    if (o.cls) el.classList.add(o.cls);
    var prev = document.activeElement;
    var api = { id:o.id, el:el, $:function(s){ return $(s, el); }, $$:function(s){ return $$(s, el); },
      close:function(result){
        if (api.closed) return; api.closed = true;
        if (o.onClose && o.onClose(result, api) === false) { api.closed = false; return; }
        el.remove(); if (scrim) scrim.remove();
        dlgStack = dlgStack.filter(function(d){ return d !== api; });
        if (prev && document.contains(prev) && !o.noRefocus) try { prev.focus(); } catch (e) {}
      },
      hide:function(on){ el.style.visibility = on ? 'hidden' : ''; if (scrim) scrim.style.visibility = on ? 'hidden' : ''; }
    };
    dlgStack.push(api);
    document.body.appendChild(scrim || el); if (scrim) document.body.appendChild(el);
    var w = el.offsetWidth, h = el.offsetHeight;
    el.style.left = Math.max(12, (innerWidth - w) / 2) + 'px'; el.style.top = Math.max(12, (innerHeight - h) / 2.4) + 'px';
    // kéo thanh tiêu đề
    $('.dlg-h', el).addEventListener('mousedown', function(e){
      if (e.target.closest('button')) return;
      var sx = e.clientX, sy = e.clientY, ox = el.offsetLeft, oy = el.offsetTop;
      function mv(ev){ el.style.left = TC.clamp(ox + ev.clientX - sx, 0, innerWidth - 60) + 'px'; el.style.top = TC.clamp(oy + ev.clientY - sy, 0, innerHeight - 34) + 'px'; }
      function up(){ removeEventListener('mousemove', mv); removeEventListener('mouseup', up); }
      addEventListener('mousemove', mv); addEventListener('mouseup', up); e.preventDefault();
    });
    el.addEventListener('click', function(e){
      var x = e.target.closest('[data-x]'); if (!x || !el.contains(x)) return;
      if (x.dataset.x === 'close' || x.dataset.x === 'cancel') api.close(false);
      else if (x.dataset.x === 'help') TC.openHelp && TC.openHelp(o.cmd || o.code);
    });
    el.addEventListener('keydown', function(e){
      if (e.key === 'Escape'){ e.preventDefault(); e.stopPropagation(); if (o.onEsc) o.onEsc(api); else api.close(false); }
      else if (e.key === 'Enter' && !e.shiftKey && e.target.tagName !== 'TEXTAREA' && e.target.tagName !== 'BUTTON' && !e.target.closest('.num, [role="listbox"], .tbl')){
        var p = $('.dlg-f .btn.primary', el); if (p && !p.disabled) { e.preventDefault(); p.click(); }
      } else if (e.key === 'Tab'){
        var f = $$('button:not([disabled]), input:not([disabled]):not([type="hidden"]), select:not([disabled]), textarea, [tabindex]:not([tabindex="-1"])', el).filter(function(n){ return n.offsetParent !== null; });
        if (!f.length) return; var i = f.indexOf(document.activeElement);
        if (e.shiftKey && i <= 0){ e.preventDefault(); f[f.length - 1].focus(); } else if (!e.shiftKey && i === f.length - 1){ e.preventDefault(); f[0].focus(); }
      } else if (e.key === 'F1'){ e.preventDefault(); TC.openHelp && TC.openHelp(o.cmd || o.code); }
    });
    if (o.onOpen) o.onOpen(api);
    setTimeout(function(){ var f = o.focus ? $(o.focus, el) : $('.dlg-b input:not([readonly]):not([type="checkbox"]):not([type="radio"]), .dlg-b select, .dlg-f .btn.primary', el); (f || el).focus(); }, 0);
    return api;
  };
  TC.topDialog = function(){ return dlgStack[dlgStack.length - 1]; };

  // hành động phá hủy hoặc khó hoàn tác: nút chính nêu rõ hành động, không dùng "OK" (07.1 §12)
  TC.confirm = function(o){
    return new Promise(function(res){
      var d = TC.dialog({ id:'confirm', title:o.title || 'Xác nhận', w:o.w || 440, help:false,
        body:'<div class="stack"><p>' + o.msg + '</p>' + (o.detail ? '<p class="muted" style="font-size:var(--fs-s)">' + o.detail + '</p>' : '') + (o.dontAsk ? '<label class="chk"><input type="checkbox" id="cf-dont"> Không hỏi lại</label>' : '') + '</div>',
        foot:'<span class="grow"></span>' + (o.alt ? '<button class="btn" type="button" data-r="alt">' + o.alt + '</button>' : '') + '<button class="btn" type="button" data-r="no">' + (o.cancel || 'Hủy') + '</button><button class="btn primary' + (o.danger ? ' danger' : '') + '" type="button" data-r="yes">' + o.ok + '</button>',
        onClose:function(r){ res(r === undefined ? false : r); } });
      d.el.addEventListener('click', function(e){ var b = e.target.closest('[data-r]'); if (!b) return; d.close(b.dataset.r === 'yes' ? true : b.dataset.r === 'alt' ? 'alt' : false); });
    });
  };

  /* ---------- toast, thông báo ---------- */
  TC.notes = TC.load('notes', null) || [];
  TC.toast = function(html, o){
    o = o || {};
    var host = $('#toasts'); if (!host) return;
    var ic = { ok:'okc', err:'error', warn:'warn', info:'info' }[o.type || 'ok'];
    var el = TC.el('<div class="toast ' + (o.type || 'ok') + '" role="' + (o.type === 'err' ? 'alert' : 'status') + '">' + TC.ic(ic, 'st') + '<p>' + html + '</p>' +
      (o.act ? '<button class="btn sm" type="button" data-a>' + o.act.label + '</button>' : '') + '<button class="ib sm" type="button" aria-label="Đóng thông báo" data-c>' + TC.ic('x') + '</button></div>');
    host.appendChild(el);
    var kill = function(){ el.remove(); };
    el.addEventListener('click', function(e){ if (e.target.closest('[data-a]')) { o.act.fn(); kill(); } else if (e.target.closest('[data-c]')) kill(); });
    if (o.type !== 'err' && !o.sticky) setTimeout(kill, o.ms || 5000);
    if (o.log !== false) { TC.notes.unshift({ t:Date.now(), html:html, type:o.type || 'ok', kind:o.kind || 'task', read:false }); TC.emit('notes'); }
    return el;
  };

  /* ---------- chú thích cạnh con trỏ (lỗi tức thời, tự ẩn sau 2 giây) ---------- */
  TC.ctip = function(cv, x, y, msg, info){
    var old = $('.ctip', cv); if (old) old.remove();
    var el = TC.el('<div class="ctip' + (info ? ' info' : '') + '" role="status">' + esc(msg) + '</div>');
    cv.appendChild(el); el.style.left = (x + 16) + 'px'; el.style.top = (y + 18) + 'px';
    setTimeout(function(){ el.remove(); }, 2000);
  };

  /* ---------- ô nhập số chuẩn UI-C09 ---------- */
  // Nhận số thường, số có đơn vị (12.5m, 250cm, 3'6"), biểu thức (=1200/2+50); dấu thập phân luôn là "."
  TC.parseNum = function(raw, unit){
    var s = String(raw).trim(); if (!s) return { err:'Nhập một giá trị.' };
    if (/^\*/.test(s)) return { err:'Nhập giá trị mới để áp dụng cho tất cả.' };
    if (/\d,\d/.test(s) && !/^=/.test(s)) return { err:'Dấu thập phân là dấu chấm. Ví dụ: 12.5', comma:true };
    if (s[0] === '=') {
      var ex = s.slice(1).replace(/\s+/g, '');
      if (!/^[\d.+\-*/()]+$/.test(ex)) return { err:'Biểu thức chỉ gồm số và + − × ÷ ( ).' };
      try { var v = Function('"use strict";return (' + ex + ')')(); if (!isFinite(v)) throw 0; return { v:v }; } catch (e) { return { err:'Biểu thức chưa đúng.' }; }
    }
    var m = s.match(/^(-?\d+(?:\.\d+)?)\s*(mm|cm|m|km|in|"|ft|')?$/i);
    var ft = s.match(/^(\d+)'\s*-?\s*(\d+(?:\.\d+)?)?"?$/);
    if (ft) return { v:(Number(ft[1]) * 12 + Number(ft[2] || 0)) * 25.4 };
    if (!m) return { err:'Chưa đọc được giá trị “' + s + '”.' };
    var k = { mm:1, cm:10, m:1000, km:1e6, in:25.4, '"':25.4, ft:304.8, "'":304.8 }[(m[2] || '').toLowerCase()] || 1;
    if (unit === 'deg' || unit === 'none') k = 1;
    return { v:Number(m[1]) * k };
  };
  TC.numField = function(o){
    return '<span class="numf' + (o.cls ? ' ' + o.cls : '') + '" data-num data-min="' + (o.min != null ? o.min : '') + '" data-step="' + (o.step || 1) + '" data-prec="' + (o.prec != null ? o.prec : 2) + '" data-unit="' + (o.unit || 'len') + '"' + (o.style ? ' style="' + o.style + '"' : '') + '>' +
      '<input id="' + o.id + '" value="' + esc(o.value) + '"' + (o.ro ? ' readonly' : '') + ' aria-label="' + esc(o.label || '') + '" autocomplete="off" spellcheck="false">' +
      // nút máy tính (UI-I10): chỉ hiện khi rê chuột hoặc focus, lấy chỗ của phần chữ nên ô không rộng thêm; screens19.js mở "tính cho ô này"
      (o.ro ? '' : '<button type="button" class="calc" data-calc tabindex="-1" aria-label="Máy tính nhanh" data-tip="Máy tính nhanh" data-cmd="QUICKCALC" data-key="Ctrl+8" data-need="PRO" data-desc="Tính giá trị cho ô này rồi bấm Áp dụng.">' + TC.ic('calc') + '</button>') +
      (o.pick !== false ? '<button type="button" class="pick" data-pick tabindex="-1" aria-label="Lấy từ bản vẽ" data-tip="Lấy từ bản vẽ" data-desc="Ẩn hộp thoại để chỉ điểm hoặc khoảng cách trên bản vẽ.">' + TC.ic('idpoint') + '</button>' : '') +
      '<span class="stepr"><button type="button" tabindex="-1" data-step="1" aria-label="Tăng">' + TC.ic('up') + '</button><button type="button" tabindex="-1" data-step="-1" aria-label="Giảm">' + TC.ic('down') + '</button></span></span>' +
      '<span class="fmsg" id="' + o.id + '-msg" aria-live="polite"></span>';
  };
  function numCommit(box){
    var inp = $('input', box), msg = document.getElementById(inp.id + '-msg');
    var r = TC.parseNum(inp.value, box.dataset.unit), min = box.dataset.min;
    if (!r.err && min !== '' && r.v <= Number(min)) r.err = (box.dataset.label || 'Giá trị') + ' phải lớn hơn ' + min + '.';
    box.classList.toggle('err', !!r.err); inp.setAttribute('aria-invalid', String(!!r.err));
    if (msg) { msg.className = 'fmsg ' + (r.err ? 'err' : 'hint'); msg.innerHTML = r.err ? TC.ic('warn', 'xs') + esc(r.err) : ''; }
    if (!r.err) { inp.value = TC.d(r.v, Number(box.dataset.prec)); box.dispatchEvent(new CustomEvent('numchange', { bubbles:true, detail:r.v })); }
    return r;
  }
  TC.numCommit = numCommit;
  document.addEventListener('keydown', function(e){
    var box = e.target.closest && e.target.closest('[data-num]'); if (!box || e.target.tagName !== 'INPUT') return;
    if (e.key === 'Enter'){ numCommit(box); }
    else if (e.key === 'ArrowUp' || e.key === 'ArrowDown'){ e.preventDefault(); step(box, e.key === 'ArrowUp' ? 1 : -1); }
  });
  document.addEventListener('focusout', function(e){ var box = e.target.closest && e.target.closest('[data-num]'); if (box && e.target.tagName === 'INPUT' && !e.target.readOnly) numCommit(box); });
  document.addEventListener('click', function(e){
    var s = e.target.closest('.numf .stepr button'); if (s) { step(s.closest('[data-num]'), Number(s.dataset.step)); return; }
    var p = e.target.closest('[data-pick]'); if (p && TC.pickValue) TC.pickValue(p.closest('[data-num]'));
  });
  // nút ⌖ (UI-C09): ẩn hộp thoại chứa ô, chỉ 2 điểm → khoảng cách (ô góc: góc của đoạn); Esc quay lại hộp thoại
  TC.pickValue = function(box){
    if (!TC.app.doc() || $('input', box).readOnly) return;
    var host = box.closest('.dlg'), api = dlgStack.filter(function(d){ return d.el === host; })[0], ang = box.dataset.unit === 'deg';
    var stop = function(e){ if (e.key === 'Escape'){ e.stopPropagation(); TC.cad.endMode(); } };
    var fin = function(v){ document.removeEventListener('keydown', stop, true); if (api) api.hide(false); var inp = $('input', box); if (v != null){ inp.value = TC.d(v, Number(box.dataset.prec)); numCommit(box); } inp.focus(); };
    if (document.activeElement) document.activeElement.blur();
    if (api) api.hide(true); document.addEventListener('keydown', stop, true);
    TC.cmdline.msg((ang ? 'Chỉ điểm đầu của đoạn đo góc' : 'Chỉ điểm thứ nhất để đo khoảng cách') + ' (Esc để quay lại).');
    TC.cad.getPoint({}).then(function(a){
      if (!a) return fin();
      TC.cmdline.msg('Chỉ điểm thứ hai.');
      TC.cad.getPoint({ base:a.p, rubber:true }).then(function(b){ fin(b ? (ang ? (TC.geo.ang(a.p, b.p) + 360) % 360 : TC.geo.dist(a.p, b.p)) : null); });
    });
  };
  function step(box, dir){
    var inp = $('input', box); if (inp.readOnly) return;
    var r = TC.parseNum(inp.value, box.dataset.unit); var v = (r.err ? 0 : r.v) + dir * Number(box.dataset.step);
    inp.value = TC.d(v, Number(box.dataset.prec)); numCommit(box);
  }
  // gõ "," thì nhắc một lần
  var commaHinted = false;
  document.addEventListener('input', function(e){
    if (commaHinted || !e.target.closest || !e.target.closest('[data-num], .cmd-in') || (e.target.closest('.cmd-in') && TC.cmd && TC.cmd.textPrompt())) return;
    if (/\d,/.test(e.target.value) && !/^=/.test(e.target.value)) { commaHinted = true; TC.toast('Trong bản vẽ, dấu thập phân luôn là dấu chấm: gõ <b>12.5</b>, không gõ 12,5.', { type:'info', log:false }); }
  });

  /* ---------- danh sách chọn một (bàn phím ↑↓) ---------- */
  TC.listKeys = function(root, sel, onPick){
    root.addEventListener('keydown', function(e){
      if (e.key !== 'ArrowDown' && e.key !== 'ArrowUp' && e.key !== 'Enter') return;
      var items = $$(sel, root); if (!items.length) return;
      var i = items.findIndex(function(n){ return n.getAttribute('aria-selected') === 'true' || n.classList.contains('is-sel'); });
      if (e.key === 'Enter'){ if (i >= 0 && onPick) onPick(items[i], true); return; }
      e.preventDefault(); i = TC.clamp(i + (e.key === 'ArrowDown' ? 1 : -1), 0, items.length - 1);
      if (onPick) onPick(items[i], false); else items[i].click();
      items[i].scrollIntoView({ block:'nearest' });
    });
  };
})();
