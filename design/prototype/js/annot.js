/* TrionCAD prototype · chú thích của bản biên tập (Đợt 3): TEXT và sửa tại chỗ UI-F12, MTEXT với tab Soạn văn bản UI-F01, kích thước và dim thông minh UI-F05, multileader, nhấp đúp theo loại đối tượng (07.1 §9.10). */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic, G = TC.geo, IMPL = TC.cmd.impl, ED = TC.ed, polar = ED.polar, sweep = ED.sweep;
  function A(){ return TC.app; } function V(){ return TC.app.view; } function D(){ return TC.app.doc(); }
  function prev(e){ return ED.temp([e], { real:true }); }
  function scrS(e, p){ var v = V(); return e && e._paper ? v.toS(p) : (v.mToS(p) || v.toS(p)); }
  function viewS(e){ var v = V(), mv = v.modelView(); return (e && e._paper) || !mv ? v.vw().s : mv.v.s; }
  function cssFont(style){ var st = D().styleMap[style] || {}; return st.css || 'Arial, "Helvetica Neue", sans-serif'; }
  function colorOf(c, layer){ var d = D(), n = c != null ? c : (d.layerMap[layer] || {}).color || 7; return n === 7 ? (d.space === 'model' ? '#FFFFFF' : '#000000') : TC.ACI[n] || '#FFFFFF'; }
  function ctipAt(s, msg){ TC.ctip($('#cv'), s[0], s[1], msg); }

  /* ================= sửa chữ một dòng tại chỗ (UI-F12) ================= */
  ED.textEdit = function(o){
    return new Promise(function(res){
      var v = V(), cv = $('#cv'), fin = false, inp = TC.el('<input class="ite" spellcheck="false" autocomplete="off" aria-label="Nội dung chữ">');
      var meas = document.createElement('canvas').getContext('2d');
      function place(){ if (fin) return; var s = scrS(o.e, o.p), px = Math.max(12, o.h * viewS(o.e) * 1.32); inp.style.font = v.textFont({ style:o.style }, px); meas.font = inp.style.font; var w = Math.max(70, meas.measureText(inp.value || 'Mẫu').width + 26); inp.style.width = w + 'px'; inp.style.height = Math.round(px * 1.32) + 'px';
        var dx = o.al === 'c' || o.al === 'm' ? -w / 2 : o.al === 'r' ? -w + 6 : -6, dy = o.al === 'm' ? -px * .66 : -px * 1.08; inp.style.left = Math.round(s[0] + dx) + 'px'; inp.style.top = Math.round(s[1] + dy) + 'px'; inp.style.transformOrigin = (-dx) + 'px ' + (-dy) + 'px'; inp.style.transform = o.rot ? 'rotate(' + (-o.rot) + 'deg)' : ''; }
      inp.value = o.s || ''; cv.appendChild(inp); place(); inp.focus(); if (o.s) inp.select();
      TC.on('viewchg', place); inp.addEventListener('input', place);
      inp.addEventListener('keydown', function(ev){
        ev.stopPropagation();
        if (ev.key === 'Enter'){ ev.preventDefault(); var val = inp.value; if (o.multi && !val.trim()){ end(); return; } var nx = o.commit(val); if (o.multi && nx){ o.p = nx; inp.value = ''; place(); } else end(); }
        else if (ev.key === 'Escape'){ ev.preventDefault(); end(true); }
      });
      function outside(ev){ if (ev.target === inp) return; ev.stopImmediatePropagation(); ev.preventDefault(); if (inp.value.trim() || !o.multi) o.commit(inp.value, true); end(); }
      setTimeout(function(){ if (!fin) cv.addEventListener('mousedown', outside, true); }, 0);
      function end(cancel){ if (fin) return; fin = true; cv.removeEventListener('mousedown', outside, true); inp.remove(); $('#cv').focus({ preventScroll:true }); res(!cancel); }
    });
  };
  IMPL.TEXT = function(ctx){
    ctx.badge('DT');
    var c = ED.cur(), al = 'l';
    ctx.out('Kiểu chữ hiện tại: “' + esc(c.tstyle) + '”  Chiều cao: ' + TC.d(c.th) + '  Chú thích theo tỉ lệ: Không  Canh lề: Trái');
    function start(){ return ctx.point('Điểm đầu của chữ', { opts:[['Canh lề', 'J'], ['Kiểu', 'S']] }).then(function(r){
      if (r.kw === 'J') return ctx.kw('Canh lề', [['Trái', 'L'], ['Giữa', 'C'], ['Phải', 'R'], ['Giữa tâm', 'M']], 'L').then(function(k){ al = { L:'l', C:'c', R:'r', M:'m' }[k] || 'l'; return start(); });
      if (r.kw === 'S') return ctx.val('Tên kiểu chữ hoặc [?]', c.tstyle).then(function(v){ if (v === '?'){ ctx.out('Kiểu chữ: ' + D().styles.map(function(s){ return s.name; }).join(', ')); } else if (D().styleMap[v]) c.tstyle = v; else ctx.out('Không có kiểu chữ “' + esc(v) + '”.'); return start(); });
      if (!r.p) return; var p = r.p;
      return ctx.point('Chiều cao', { base:p, def:TC.d(c.th), num:true, dyn:false }).then(function(h){ var H = h.num != null ? h.num : h.p ? G.dist(p, h.p) : c.th; if (!(H > 0)) H = c.th; c.th = H;
        return ctx.point('Góc xoay của chữ', { base:p, def:'0', num:true, dyn:'ang' }).then(function(a){ var rot = a.num != null ? a.num : a.p ? G.ang(p, a.p) : 0, n = 0;
          ctx.out('Gõ chữ ngay trên bản vẽ. Enter để xuống dòng (một đối tượng TEXT mới), Enter ở dòng trống hoặc bấm ra ngoài để kết thúc.', 'faint');
          return ED.run('TEXT', function(){ return ED.textEdit({ p:p, h:H, rot:rot, al:al, style:c.tstyle, multi:true, commit:function(val){ if (!val.trim()) return null; var e = ED.add(ED.make({ t:'text', p:this.p, h:H, s:val, rot:rot, al:al, style:c.tstyle })); n++; V().draw(); var dy = H * 1.6667, r2 = rot * Math.PI / 180; return [e.p[0] + dy * Math.sin(r2), e.p[1] - dy * Math.cos(r2)]; } }).then(function(){ if (n) ctx.out('Đã tạo ' + n + ' dòng chữ.'); }); }); }); });
    }); }
    return start();
  };

  /* ================= trình soạn MText tại chỗ + tab ngữ cảnh Soạn văn bản (UI-F01) ================= */
  var SYM = [['°', 'Độ', '%%d'], ['±', 'Cộng trừ', '%%p'], ['⌀', 'Đường kính', '%%c'], ['≈', 'Xấp xỉ'], ['∠', 'Góc'], ['Δ', 'Delta'], ['Ω', 'Ohm'], ['²', 'Mũ 2'], ['³', 'Mũ 3'], ['≠', 'Không bằng'], ['≤', 'Nhỏ hơn hoặc bằng'], ['≥', 'Lớn hơn hoặc bằng'], ['‰', 'Phần nghìn'], [' ', 'Khoảng trắng không ngắt'], ['m²', 'Mét vuông'], ['m³', 'Mét khối']];
  var MTE = null;
  ED.mtextEdit = function(e, o){
    o = o || {};
    if (MTE) MTE.done(true);
    return new Promise(function(res){
      var v = V(), cv = $('#cv'), F = { bold:!!e.bold, ital:!!e.ital, und:!!e.und, over:!!e.over2, strike:!!e.strike, caps:!!e.caps, al:e.al || 'l', just:e.just || 'TL', color:e.color, style:e.style || ED.cur().tstyle, h:e.h, ls:e.ls || 1.6667, bg:!!e.bg, anno:!!e.anno, obl:0, track:1, wf:1, ruler:true, auto:false }; if (TC.screens.mtInit) TC.screens.mtInit(F, e);
      var box = TC.el('<div class="mte" role="group" aria-label="Trình soạn MText"><div class="mte-ruler" aria-hidden="true"><i class="i1"></i><i class="i2"></i><i class="tbs"></i></div><textarea spellcheck="false" aria-label="Nội dung MText"></textarea><span class="mte-w" role="separator" aria-label="Kéo để đổi bề rộng khung" title="Kéo để đổi bề rộng khung"></span></div>');
      var ta = $('textarea', box), orig = e.s || '', W = o.w ? o.w() : e.w, fin = false;
      function anchor(){ return o.anchor ? o.anchor() : e.p; }
      function place(){ if (fin) return; var s = scrS(e, anchor()), k = viewS(e), px = Math.max(12, F.h * k * 1.32); box.style.left = Math.round(s[0]) + 'px'; box.style.top = Math.round(s[1] - (F.ruler ? 18 : 2)) + 'px'; box.style.width = Math.max(140, W * k + 8) + 'px';
        ta.style.font = (F.ital ? 'italic ' : '') + (F.bold ? '600 ' : '') + px + 'px ' + cssFont(F.style); ta.style.lineHeight = Math.max(px * 1.15, F.h * F.ls * k) + 'px'; ta.style.textDecoration = [F.und ? 'underline' : '', F.over ? 'overline' : '', F.strike ? 'line-through' : ''].join(' ').trim() || 'none'; ta.style.textTransform = F.caps ? 'uppercase' : 'none'; ta.style.textAlign = { l:'left', c:'center', r:'right', j:'justify', d:'justify' }[F.al] || 'left'; ta.style.color = colorOf(F.color, e.layer); ta.style.background = F.bg ? 'var(--c-canvas)' : ''; ta.style.letterSpacing = F.track !== 1 ? ((F.track - 1) * px * .5) + 'px' : ''; box.classList.toggle('no-ruler', !F.ruler); ta.style.height = 'auto'; ta.style.height = Math.max(ta.scrollHeight, px * 1.6) + 'px'; }
      ta.value = orig; cv.appendChild(box); place(); ta.focus(); ta.setSelectionRange(ta.value.length, ta.value.length);
      MTE = { e:e, F:F, ta:ta, apply:function(){ place(); A().setCtxTab(tab(), true); setTimeout(function(){ if (!fin) ta.focus(); }, 0); }, done:done, insert:function(t){ ta.setRangeText(t, ta.selectionStart, ta.selectionEnd, 'end'); place(); ta.focus(); }, w:function(v){ if (v){ W = v; place(); } return W; } };
      A().setCtxTab(tab());
      TC.on('viewchg', place); ta.addEventListener('input', function(){ if (F.auto && /(^|[.!?]\s+|\n)([a-zà-ỹđ])$/.test(ta.value.slice(0, ta.selectionStart))){ var p0 = ta.selectionStart; ta.value = ta.value.slice(0, p0 - 1) + ta.value[p0 - 1].toUpperCase() + ta.value.slice(p0); ta.setSelectionRange(p0, p0); } place(); });
      ta.addEventListener('keydown', function(ev){ ev.stopPropagation(); if (ev.key === 'Enter' && (ev.ctrlKey || ev.metaKey)){ ev.preventDefault(); done(true); } else if (ev.key === 'Escape'){ ev.preventDefault(); if (ta.value !== orig) TC.confirm({ title:'Đóng trình soạn chữ', msg:'Bỏ các thay đổi trong MText?', ok:'Bỏ thay đổi', cancel:'Tiếp tục sửa', alt:'Lưu và đóng' }).then(function(y){ if (y === true) done(false); else if (y === 'alt') done(true); else ta.focus(); }); else done(false); } });
      // kéo tay nắm bên phải để đổi bề rộng khung
      $('.mte-w', box).addEventListener('mousedown', function(ev){ ev.preventDefault(); ev.stopPropagation(); var x0 = ev.clientX, w0 = W, k = viewS(e); function mv(m){ W = Math.max(F.h * 4, w0 + (m.clientX - x0) / k); place(); } function up(){ removeEventListener('mousemove', mv); removeEventListener('mouseup', up); ta.focus(); } addEventListener('mousemove', mv); addEventListener('mouseup', up); });
      function outside(ev){ if (box.contains(ev.target)) return; ev.stopImmediatePropagation(); ev.preventDefault(); done(true); }
      setTimeout(function(){ if (!fin) cv.addEventListener('mousedown', outside, true); }, 0);
      function done(ok){ if (fin) return; fin = true; cv.removeEventListener('mousedown', outside, true); box.remove(); MTE = null; A().setCtxTab(null); $('#cv').focus({ preventScroll:true }); res(ok ? { text:ta.value, F:F, w:W } : null); }
    });
  };
  function applyF(x, F, w){ ['bold', 'ital', 'und', 'caps', 'bg', 'anno'].forEach(function(k){ if (F[k]) x[k] = true; else delete x[k]; }); if (F.over) x.over2 = true; else delete x.over2; if (F.strike) x.strike = true; else delete x.strike; x.al = F.al; x.just = F.just; x.style = F.style; x.h = F.h; x.ls = F.ls; if (F.color != null) x.color = F.color; else delete x.color; if (w && x.t === 'mtext') x.w = w; if (TC.screens.mtApply) TC.screens.mtApply(x, F); return x; }
  function tab(){
    var F = MTE.F, d = D(), st = function(fn){ return function(){ fn(); MTE.apply(); }; }, tog = function(k){ return { toggle:true, on:function(){ return !!F[k]; }, act:st(function(){ F[k] = !F[k]; }) }; };
    var IO = function(label, icon, id, x){ return Object.assign({ t:'io', label:label, icon:icon, id:id }, x || {}); };
    var sel = function(fn){ var ta = MTE.ta, a = ta.selectionStart, b = ta.selectionEnd; if (a === b){ a = 0; b = ta.value.length; } ta.setRangeText(fn(ta.value.slice(a, b)), a, b, 'select'); MTE.apply(); };
    var menuAt = function(b, items){ var r = b.getBoundingClientRect(); TC.menu(items, r.left, r.bottom + 2, { returnFocus:b, minWidth:Math.max(r.width, 170) }); };
    var colorMenu = [{ label:'ByLayer', check:F.color == null, act:st(function(){ F.color = null; }) }, { label:'ByBlock', act:st(function(){ F.color = 0; }) }].concat([1, 2, 3, 4, 5, 6, 7, 8, 9].map(function(n){ return { label:'<i class="sw" style="background:' + (n === 7 ? '#FFFFFF' : TC.ACI[n]) + '"></i>' + TC.aciName(n), check:F.color === n, act:st(function(){ F.color = n; }) }; })).concat([{ sep:1 }, { label:'Chọn màu khác…', act:function(){ TC.screens.colorPicker(F.color).then(function(c){ if (c !== undefined){ F.color = c; MTE.apply(); } }); } }]);
    var JUST = [['TL', 'Trên trái'], ['TC', 'Trên giữa'], ['TR', 'Trên phải'], ['ML', 'Giữa trái'], ['MC', 'Giữa tâm'], ['MR', 'Giữa phải'], ['BL', 'Dưới trái'], ['BC', 'Dưới giữa'], ['BR', 'Dưới phải']];
    return { id:'mte', name:'Soạn văn bản', ctx:true, panels:[
      { name:'Kiểu', icon:'mtext', prio:2, items:[ { t:'col', gap:3, items:[
        { t:'dd', id:'mte-style', label:'Kiểu chữ', w:150, html:function(){ return ic('mtext'); }, text:function(){ return F.style; }, act:function(b){ menuAt(b, d.styles.map(function(s){ return { label:'<span style="font-family:' + esc(s.css || 'Arial') + '">' + esc(s.name) + '</span> <span class="faint">· ' + esc(s.font) + '</span>', check:F.style === s.name, act:st(function(){ F.style = s.name; }) }; }).concat([{ sep:1 }, { label:'Quản lý kiểu chữ…', act:function(){ TC.cmd.run("'STYLE"); } }])); } },
        { t:'num', id:'mte-h', label:'Chiều cao', w:70, get:function(){ return TC.d(F.h); }, set:function(v){ var n = TC.parseNum(v); if (!n.err && n.v > 0){ F.h = n.v; MTE.apply(); } } },
        Object.assign({ t:'s', label:'Chú thích theo tỉ lệ', icon:'anno', id:'mte-anno' }, tog('anno')) ] } ] },
      { name:'Định dạng', icon:'bold', prio:1, items:[ { t:'col', gap:3, items:[
        { t:'row', items:[ IO('Đậm', 'bold', 'mte-b', Object.assign({ key:'Ctrl+B' }, tog('bold'))), IO('Nghiêng', 'italic', 'mte-i', Object.assign({ key:'Ctrl+I' }, tog('ital'))), IO('Gạch chân', 'underline', 'mte-u', Object.assign({ key:'Ctrl+U' }, tog('und'))), IO('Gạch trên', 'overline', 'mte-o', tog('over')), IO('Gạch ngang', 'strike', 'mte-s', tog('strike')) ] },
        { t:'row', items:[ { t:'dd', id:'mte-font', label:'Font', w:122, text:function(){ return (d.styleMap[F.style] || {}).font || 'Arial'; }, act:function(b){ menuAt(b, ['Arial', 'Times New Roman', 'TrionVN-Times.shx', 'romans.shx', 'IBM Plex Sans'].map(function(f){ return { label:esc(f), act:function(){ TC.toast('Font chỉ áp dụng cho phần chữ đang chọn. Prototype: đổi kiểu chữ để đổi font cả đối tượng.', { type:'info', log:false }); } }; })); } },
          { t:'dd', id:'mte-color', label:'Màu chữ', w:96, html:function(){ return '<i class="sw" style="background:' + colorOf(F.color, MTE.e.layer) + '"></i>'; }, text:function(){ return F.color == null ? 'ByLayer' : F.color === 0 ? 'ByBlock' : TC.aciName(F.color); }, act:function(b){ menuAt(b, colorMenu); } } ] },
        { t:'row', items:[ IO('Khớp định dạng', 'match', 'mte-mf', { act:function(){ TC.cmdline.msg('Khớp định dạng: chọn đoạn chữ nguồn rồi đoạn chữ đích trong trình soạn.'); } }), IO('Xóa định dạng', 'erase', 'mte-clr', { menu:function(){ return [['Xóa định dạng ký tự', 'c'], ['Xóa định dạng đoạn', 'p'], ['Xóa tất cả', 'a']].map(function(x){ return { label:x[0], act:st(function(){ if (x[1] !== 'p'){ F.bold = F.ital = F.und = F.over = F.strike = F.caps = false; F.color = null; } if (x[1] !== 'c'){ F.al = 'l'; F.ls = 1.6667; } }) }; }); }, menuOnly:true }),
          IO('Chữ hoa, chữ thường', 'case', 'mte-case', { menuOnly:true, menu:function(){ return [{ label:'CHỮ HOA', kbd:'Ctrl+Shift+U', act:function(){ sel(function(s){ return s.toUpperCase(); }); } }, { label:'chữ thường', kbd:'Ctrl+Shift+L', act:function(){ sel(function(s){ return s.toLowerCase(); }); } }, { label:'Viết Hoa Chữ Đầu', act:function(){ sel(function(s){ return s.toLowerCase().replace(/(^|\s)(\S)/g, function(m, a, b){ return a + b.toUpperCase(); }); }); } }, { label:'Viết hoa đầu câu', act:function(){ sel(function(s){ return s.toLowerCase().replace(/(^\s*|[.!?]\s+)(\S)/g, function(m, a, b){ return a + b.toUpperCase(); }); }); } }, { sep:1 }, { h:'Giữ đúng dấu tiếng Việt: “đường” → “ĐƯỜNG”' }]; } }),
          IO('Che nền', 'bgmask', 'mte-bg', tog('bg')), IO('Xếp chồng', 'stack', 'mte-stk', { menuOnly:true, menu:function(){ return [{ label:'Xếp chồng / bỏ xếp chồng', act:function(){ TC.screens.mtStack(MTE); } }, { label:'Thuộc tính xếp chồng…', act:function(){ TC.screens.mtStack(MTE, 'props'); } }, { label:'Thiết lập AutoStack…', act:function(){ TC.screens.mtStack(MTE, 'auto'); } }]; } }) ] } ] },
        { t:'col', gap:3, items:[ { t:'num', id:'mte-obl', label:'Nghiêng', w:52, get:function(){ return TC.d(F.obl, 0); }, set:function(v){ F.obl = Number(v) || 0; MTE.apply(); } }, { t:'num', id:'mte-trk', label:'Giãn ký tự', w:52, get:function(){ return TC.d(F.track, 2); }, set:function(v){ F.track = TC.clamp(Number(v) || 1, .75, 4); MTE.apply(); } }, { t:'num', id:'mte-wf', label:'Hệ số rộng', w:52, get:function(){ return TC.d(F.wf, 2); }, set:function(v){ F.wf = TC.clamp(Number(v) || 1, .1, 10); MTE.apply(); } } ] } ] },
      { name:'Đoạn', icon:'alignl', prio:1, items:[
        { t:'lg', label:'Điểm\ncanh', icon:'justify', id:'mte-just', menuOnly:true, menu:function(){ return JUST.map(function(j){ return { label:j[1] + ' <span class="faint">· ' + j[0] + '</span>', check:F.just === j[0], act:st(function(){ F.just = j[0]; F.al = { L:'l', C:'c', R:'r' }[j[0][1]]; }) }; }); } },
        { t:'col', gap:3, items:[ { t:'row', items:[ IO('Canh trái', 'alignl', 'mte-al', { toggle:true, on:function(){ return F.al === 'l'; }, act:st(function(){ F.al = 'l'; }) }), IO('Canh giữa', 'alignc', 'mte-ac', { toggle:true, on:function(){ return F.al === 'c'; }, act:st(function(){ F.al = 'c'; }) }), IO('Canh phải', 'alignr', 'mte-ar', { toggle:true, on:function(){ return F.al === 'r'; }, act:st(function(){ F.al = 'r'; }) }), IO('Canh đều', 'alignj', 'mte-aj', { toggle:true, on:function(){ return F.al === 'j'; }, act:st(function(){ F.al = 'j'; }) }), IO('Phân bố', 'alignd', 'mte-ad', { toggle:true, on:function(){ return F.al === 'd'; }, act:st(function(){ F.al = 'd'; }) }) ] },
          { t:'s', label:'Bullet và đánh số', icon:'bullet', id:'mte-bul', menuOnly:true, menu:function(){ var pre = function(fn){ return function(){ sel(function(s){ return s.split('\n').map(fn).join('\n'); }); }; }, strip = function(l){ return l.replace(/^(\d+\.|[a-z]\.|•)\s+/, ''); }; return [{ label:'Không', act:pre(strip) }, { label:'Số: 1. 2. 3.', act:function(){ var i = 0; sel(function(s){ return s.split('\n').map(function(l){ return (++i) + '. ' + strip(l); }).join('\n'); }); } }, { label:'Chữ cái: a. b. c.', act:function(){ var i = 0; sel(function(s){ return s.split('\n').map(function(l){ return String.fromCharCode(97 + (i++ % 26)) + '. ' + strip(l); }).join('\n'); }); } }, { label:'Ký hiệu: •', act:pre(function(l){ return '• ' + strip(l); }) }, { sep:1 }, { label:'Tự động đánh số khi gõ', check:true }]; } },
          { t:'s', label:'Giãn dòng', icon:'lspace', id:'mte-ls', menuOnly:true, menu:function(){ return [1, 1.5, 2, 2.5].map(function(x){ return { label:TC.d(x, 1) + 'x', check:Math.abs(F.ls - 1.6667 * x) < .01, act:st(function(){ F.ls = 1.6667 * x; }) }; }).concat([{ sep:1 }, { label:'Khác… (đoạn văn)', act:function(){ TC.screens.mtPara(MTE); } }]); } } ] } ] },
      { name:'Chèn', icon:'symbol', prio:2, items:[ { t:'lg', label:'Ký hiệu', icon:'symbol', id:'mte-sym', menuOnly:true, menu:function(){ return SYM.map(function(s){ return { label:'<span class="mono" style="display:inline-block;min-width:22px">' + (s[0] === ' ' ? '·' : esc(s[0])) + '</span>' + esc(s[1]) + (s[2] ? ' <span class="faint">' + s[2] + '</span>' : ''), act:function(){ MTE.insert(s[0]); } }; }).concat([{ sep:1 }, { label:'Khác… (bảng ký tự)', act:function(){ MTE.insert('∅'); } }]); } },
        { t:'col', items:[ { t:'s', label:'Cột', icon:'columns', id:'mte-col', menuOnly:true, menu:function(){ return TC.screens.mtColMenu(MTE); } }, { t:'s', label:'Field', icon:'field', id:'mte-field', act:function(){ TC.screens.mtField(MTE); } } ] } ] },
      { name:'Chính tả', icon:'spell', prio:4, items:[ { t:'lg', label:'Kiểm tra\nchính tả', icon:'spell', id:'mte-spell', act:function(){ TC.toast('Kiểm tra chính tả tiếng Việt (UI-F10) có sau v1.0.', { type:'info', log:false }); } } ] },
      { name:'Công cụ', icon:'findtext', prio:3, items:[ { t:'col', items:[ { t:'s', label:'Tìm và thay thế', icon:'findtext', id:'mte-find', key:'Ctrl+R', act:function(){ var ta = MTE.ta, dlg = TC.dialog({ id:'mtefind', title:'Tìm và thay thế trong MText', code:'UI-F09', w:440, modal:false, help:false, body:'<div class="stack"><div class="field"><label for="mf-q">Tìm</label><input class="inp" id="mf-q" value="' + esc(ta.value.slice(ta.selectionStart, ta.selectionEnd)) + '"></div><div class="field"><label for="mf-r">Thay bằng</label><input class="inp" id="mf-r"></div><div class="hrow"><label class="chk"><input type="checkbox" id="mf-c"> Phân biệt hoa thường</label><label class="chk"><input type="checkbox" id="mf-d" checked> Phân biệt dấu tiếng Việt</label></div><span class="faint" id="mf-n" style="font-size:var(--fs-s)"></span></div>', foot:'<span class="grow"></span><button class="btn" type="button" id="mf-all">Thay tất cả</button><button class="btn primary" type="button" data-x="close">Đóng</button>' });
          $('#mf-all', dlg.el).addEventListener('click', function(){ var q = $('#mf-q', dlg.el).value, r = $('#mf-r', dlg.el).value; if (!q) return; var flags = $('#mf-c', dlg.el).checked ? 'g' : 'gi', esc2 = q.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), n = (ta.value.match(new RegExp(esc2, flags)) || []).length; ta.value = ta.value.replace(new RegExp(esc2, flags), r); MTE.apply(); $('#mf-n', dlg.el).textContent = 'Đã thay ' + n + ' chỗ.'; }); } }, { t:'s', label:'Nhập văn bản', icon:'open', id:'mte-imp', act:function(){ TC.screens.mtImport(MTE); } }, Object.assign({ t:'s', label:'Tự viết hoa', icon:'case', id:'mte-auto' }, tog('auto')) ] } ] },
      { name:'Tùy chọn', icon:'gear', prio:3, items:[ { t:'col', items:[ { t:'s', label:'Hoàn tác', icon:'undo', id:'mte-undo', key:'Ctrl+Z', act:function(){ MTE.ta.focus(); document.execCommand('undo'); } }, { t:'s', label:'Làm lại', icon:'redo', id:'mte-redo', key:'Ctrl+Y', act:function(){ MTE.ta.focus(); document.execCommand('redo'); } }, Object.assign({ t:'s', label:'Thước', icon:'ruler', id:'mte-ruler' }, tog('ruler')) ] },
        { t:'io', label:'Thiết lập trình soạn', icon:'more', id:'mte-set', menuOnly:true, menu:function(){ return [{ label:'Luôn hiển thị như trên bản vẽ (WYSIWYG)', check:true }, { label:'Hiện thanh công cụ', check:false }, { label:'Nền trình soạn đục', check:false }, { label:'Màu tô chữ đang chọn…' }]; } } ] },
      { name:'Đóng', icon:'close', prio:1, items:[ { t:'lg', label:'Đóng trình\nsoạn chữ', icon:'close', id:'mte-close', key:'Ctrl+Enter', act:function(){ MTE.done(true); } } ] }
    ] };
  }
  ED.isEditing = function(){ return !!MTE; }; ED.mte = function(){ return MTE; };
  IMPL.MTEXT = function(ctx){
    ctx.badge('MT');
    var c = ED.cur(), H = c.th, ls = 1.6667, rot = 0, al = 'l';
    ctx.out('Kiểu chữ hiện tại: “' + esc(c.tstyle) + '”  Chiều cao: ' + TC.d(H) + '  Chú thích theo tỉ lệ: Không');
    return ctx.point('Góc thứ nhất').then(function(r){ if (!r.p) return; var a = r.p;
      function frame(p){ return [a, [p[0], a[1]], p, [a[0], p[1]]]; }
      function corner(){ return ctx.point('Góc đối', { base:a, rubber:false, dyn:'wh', opts:[['Chiều cao', 'H'], ['Canh lề', 'J'], ['Giãn dòng', 'L'], ['Xoay', 'R'], ['Kiểu', 'S'], ['Rộng', 'W'], ['Cột', 'C']], onMove:function(p, temp){ temp.push({ k:'poly', pts:frame(p), closed:true, dash:[4, 3] }); var y = Math.max(a[1], p[1]) - H * 1.2, x0 = Math.min(a[0], p[0]); temp.push({ k:'line', a:[x0 + H * .3, y], b:[x0 + Math.min(Math.abs(p[0] - a[0]) * .6, H * 10), y], w:1, color:TC.cssv('--c-accent') }); } }).then(function(q){
        if (q.kw === 'H') return ctx.val('Chiều cao chữ', TC.d(H)).then(function(v){ H = Number(v) || H; c.th = H; return corner(); });
        if (q.kw === 'L') return ctx.kw('Kiểu giãn dòng', [['Tối thiểu', 'A'], ['Chính xác', 'E']], 'A').then(function(){ return ctx.val('Hệ số giãn dòng (vd 1x)', '1x').then(function(v){ ls = 1.6667 * (parseFloat(v) || 1); return corner(); }); });
        if (q.kw === 'R') return ctx.point('Góc xoay', { base:a, num:true, dyn:'ang' }).then(function(z){ rot = z.num != null ? z.num : z.p ? G.ang(a, z.p) : 0; return corner(); });
        if (q.kw === 'S') return ctx.val('Tên kiểu chữ', c.tstyle).then(function(v){ if (D().styleMap[v]) c.tstyle = v; else ctx.out('Không có kiểu chữ “' + esc(v) + '”.'); return corner(); });
        if (q.kw === 'J') return ctx.kw('Điểm canh', [['Trên trái', 'TL'], ['Trên giữa', 'TC'], ['Trên phải', 'TR'], ['Giữa trái', 'ML'], ['Giữa tâm', 'MC'], ['Giữa phải', 'MR'], ['Dưới trái', 'BL'], ['Dưới giữa', 'BC'], ['Dưới phải', 'BR']], 'TL').then(function(k){ al = { L:'l', C:'c', R:'r' }[k[1]] || 'l'; return corner(); });
        if (q.kw === 'C') return ctx.kw('Kiểu cột', [['Động', 'D'], ['Tĩnh', 'S'], ['Không cột', 'N']], 'N').then(corner);
        if (q.kw === 'W') return ctx.point('Bề rộng khung', { base:a, num:true, dyn:false }).then(function(z){ var w = z.num != null ? z.num : z.p ? Math.abs(z.p[0] - a[0]) : 3000; return open(a[0], a[0] + w, a[1]); });
        if (q.p) return open(Math.min(a[0], q.p[0]), Math.max(a[0], q.p[0]), Math.max(a[1], q.p[1])); }); }
      function open(x0, x1, y1){ var e = ED.make({ t:'mtext', p:[x0, y1], w:Math.max(H * 4, x1 - x0), h:H, s:'', ls:ls, style:c.tstyle, rot:rot, al:al });
        ctx.out('Gõ nội dung. Ctrl+Enter hoặc bấm ra ngoài khung để kết thúc; Esc để hủy.', 'faint');
        return ED.mtextEdit(e).then(function(r){ if (!r || !r.text.trim()) return; e.s = r.text; applyF(e, r.F, r.w); return ED.run('MTEXT', function(){ ED.add(e); }); }); }
      return corner(); });
  };

  /* ================= sửa chú thích (TEXTEDIT, nhấp đúp) ================= */
  function editText(e){ return ED.textEdit({ e:e, p:e.p, h:e.h, rot:e.rot || 0, al:e.al || 'l', style:e.style, s:e.s, commit:function(val){ if (val !== e.s && val.trim()) ED.run('TEXTEDIT', function(){ ED.mod(e, function(x){ x.s = val; }); }); } }); }
  function editMtext(e){ return ED.mtextEdit(e).then(function(r){ if (!r) return; if (!r.text.trim()) return ED.run('TEXTEDIT', function(){ ED.del(e); }); return ED.run('TEXTEDIT', function(){ ED.mod(e, function(x){ x.s = r.text; applyF(x, r.F, r.w); }); }); }); }
  function mlAnchor(e){ var b = e.pts[1], dir = b[0] >= e.pts[0][0] ? 1 : -1; return [b[0] + dir * e.h * 1.6, b[1] + e.h * 1.35]; }
  function editMl(e, isNew){ var proxy = { t:'mtext', p:mlAnchor(e), w:e.h * 16, h:e.h, s:e.s || '', style:e.style, layer:e.layer, _paper:e._paper }; return ED.mtextEdit(proxy, { anchor:function(){ return mlAnchor(e); } }).then(function(r){ if (!r) return isNew ? null : undefined; if (isNew){ if (!r.text.trim()) return; e.s = r.text; return ED.run('MLEADER', function(){ ED.add(e); }); } return ED.run('TEXTEDIT', function(){ ED.mod(e, function(x){ x.s = r.text; x.h = r.F.h; x.style = r.F.style; }); }); }); }
  function editDim(e){ var tp = (e._p || []).filter(function(x){ return x.k === 'text'; })[0]; if (!tp) return Promise.resolve(); var meas = String(Math.round(e._meas || 0)); return ED.textEdit({ e:e, p:tp.p, h:tp.h, rot:tp.rot || 0, al:'m', style:'Standard', s:e.over || '<>', commit:function(val){ val = val.trim(); ED.run('TEXTEDIT', function(){ ED.mod(e, function(x){ if (!val || val === '<>' || val === meas) delete x.over; else x.over = val; }); }); } }); }
  IMPL.TEXTEDIT = function(ctx, pre){
    var go = function(e){ ctx.badge('ED'); if (e.t === 'text' || e.t === 'attdef') return editText(e); if (e.t === 'mtext') return editMtext(e); if (e.t === 'mleader') return editMl(e); if (/^dim/.test(e.t)) return editDim(e); };
    if (pre && pre.t) return go(pre);
    return ED.pick(ctx, 'Chọn chú thích', { opts:[['Hoàn tác', 'U'], ['Chế độ', 'M']], types:['text', 'mtext', 'mleader', 'dim', 'dimr', 'dima', 'attdef'], bad:'Chọn chữ, MText, kích thước hoặc multileader' }).then(function(r){
      if (r.kw === 'U'){ ED.undo(1); return IMPL.TEXTEDIT(ctx); }
      if (r.kw === 'M') return ctx.kw('Chế độ', [['Một', 'S'], ['Nhiều', 'M']], 'M').then(function(){ return IMPL.TEXTEDIT(ctx); });
      if (r.e) return Promise.resolve(go(r.e)).then(function(){ return IMPL.TEXTEDIT(ctx); }); });
  };
  // nhấp đúp theo loại đối tượng (07.1 §9.10)
  TC.cad.hook.dbl = function(e){
    if (TC.isViewer() || !e) return false;
    if ((D().layerMap[e.layer] || {}).lck){ var s = V().mouse || [100, 100]; ctipAt(s, 'Đối tượng nằm trên layer bị khóa'); return true; }
    if (/^(text|mtext|mleader|attdef|dim|dimr|dima)$/.test(e.t)){ TC.cmd.run('TEXTEDIT', e); return true; }
    if (e.t === 'insert'){ var bd = D().blocks[e.name]; if (bd && bd.ents.some(function(x){ return x.t === 'attdef'; })) TC.cmd.run('EATTEDIT', e); else TC.confirm({ title:'Sửa block', msg:'Mở <b>' + esc(e.name) + '</b> trong Block Editor?', detail:'Block Editor (UI-G04) có từ v1.0. Hoặc sửa tại chỗ bằng REFEDIT.', ok:'Mở Block Editor' }).then(function(y){ if (y) TC.cmd.run('BEDIT'); }); return true; }
    if (e.t === 'hatch'){ D().sel = [e.id]; TC.cad.afterSel(); TC.cmd.run('HATCHEDIT', e); return true; }
    if (e.t === 'pline'){ TC.cmd.run('PEDIT', e); return true; }
    if (e.t === 'image'){ D().sel = [e.id]; TC.cad.afterSel(); return true; }
    if (e.t === 'spline'){ TC.cmd.run('SPLINEDIT'); return true; }
    return false;
  };

  /* ================= multileader ================= */
  IMPL.MLEADER = function(ctx){
    ctx.badge('MLD');
    var c = ED.cur(), H = 250, order = LASTML.order;
    function opts(){ return ctx.kw('Nhập một tùy chọn', [['Kiểu đường dẫn', 'L'], ['Đoạn ngang', 'A'], ['Loại nội dung', 'C'], ['Số điểm tối đa', 'M'], ['Góc thứ nhất', 'F'], ['Góc thứ hai', 'S'], ['Thoát tùy chọn', 'X']], 'X').then(function(k){ if (k === 'X' || !k) return start(); if (k === 'L') return ctx.kw('Kiểu đường dẫn', [['Thẳng', 'S'], ['Spline', 'P'], ['Không', 'N']], 'S').then(opts); if (k === 'A') return ctx.kw('Dùng đoạn ngang?', [['Có', 'Y'], ['Không', 'N']], 'Y').then(opts); if (k === 'C') return ctx.kw('Loại nội dung', [['Block', 'B'], ['MText', 'M'], ['Không', 'N']], 'M').then(opts); return ctx.val(k === 'M' ? 'Số điểm tối đa của đường dẫn' : 'Góc ràng buộc', k === 'M' ? '2' : '0').then(opts); }); }
    function start(){ return ctx.point(order === 'L' ? 'Vị trí đoạn ngang' : 'Vị trí đầu mũi tên', { opts:[['Đầu mũi tên trước', 'H'], ['Đoạn ngang trước', 'L'], ['Nội dung trước', 'C'], ['Tùy chọn', 'O']].filter(function(o){ return o[1] !== (order || 'H'); }), def:'Tùy chọn' }).then(function(r){
      if (r.enter || r.kw === 'O') return opts();
      if (r.kw === 'H' || r.kw === 'L' || r.kw === 'C'){ order = LASTML.order = r.kw; if (r.kw === 'C') ctx.out('Nội dung trước: chỉ khung chữ, gõ chữ, rồi chỉ đầu mũi tên. Prototype vẽ theo thứ tự đầu mũi tên trước.'); return start(); }
      if (!r.p) return; var a = r.p;
      return ctx.point(order === 'L' ? 'Vị trí đầu mũi tên' : 'Vị trí đoạn ngang', { base:a, onMove:function(p, temp){ var pts = order === 'L' ? [p, a] : [a, p]; temp.push(prev(ED.make({ t:'mleader', pts:pts, s:'', h:H, style:c.tstyle }))); } }).then(function(q){ if (!q.p) return; var e = ED.make({ t:'mleader', pts:order === 'L' ? [q.p, a] : [a, q.p], s:'', h:H, style:c.tstyle }); TC.prepEnt(D(), e, true); V().temp = [prev(e)]; V().drawOver(); return editMl(e, true).then(function(){ V().temp = []; V().drawOver(); }); }); }); }
    return start();
  };
  var LASTML = { order:'H' };
  IMPL.MLEADEREDIT = function(ctx){ return ED.pick(ctx, 'Chọn multileader', { types:['mleader'] }).then(function(r){ if (!r.e) return; var e = r.e;
    return ctx.kw('Tùy chọn', [['Thêm đường dẫn', 'A'], ['Bớt đường dẫn', 'R']], 'A').then(function(k){
      if (k === 'R'){ if (!(e.more || []).length){ ctx.out('Multileader chỉ có một đường dẫn.'); return; } return ED.run('MLEADEREDIT', function(){ ED.mod(e, function(x){ x.more.pop(); }); }); }
      return ED.run('MLEADEREDIT', function again(){ return ctx.point('Vị trí đầu mũi tên', { base:e.pts[1], onMove:function(p, temp){ var x = ED.clean(e); x.more = (x.more || []).concat([p]); temp.push(prev(x)); } }).then(function(q){ if (!q.p) return; ED.mod(e, function(x){ x.more = (x.more || []).concat([q.p]); }); V().draw(); return again(); }); }); }); }); };

  /* ================= kích thước ================= */
  function dimStyle(){ return ED.cur().dimstyle; }
  function dimEnt(p1, p2, dl, dir){
    var o = ED.make({ t:'dim', p1:p1, p2:p2, style:dimStyle() });
    if (!dir){ var x0 = Math.min(p1[0], p2[0]), x1 = Math.max(p1[0], p2[0]), y0 = Math.min(p1[1], p2[1]), y1 = Math.max(p1[1], p2[1]), outX = dl[0] < x0 || dl[0] > x1, outY = dl[1] < y0 || dl[1] > y1;
      dir = outY && !outX ? 'h' : outX && !outY ? 'v' : Math.abs(p2[0] - p1[0]) >= Math.abs(p2[1] - p1[1]) ? 'h' : 'v'; }
    o.dir = dir; ED.moveDimLine(o, dl); return o;
  }
  ED.dimEnt = dimEnt;
  function endsOf(e, p){
    if (e.t === 'line') return [e.p1, e.p2];
    if (e.t === 'arc') return [polar(e.c, e.r, e.a0), polar(e.c, e.r, e.a1)];
    if (e.t === 'circle') return [[e.c[0] - e.r, e.c[1]], [e.c[0] + e.r, e.c[1]]];
    if (e.t === 'pline'){ var n = e.pts.length, m = e.closed ? n : n - 1, best = 0, bd = Infinity; for (var i = 0; i < m; i++){ var q = G.segDist(p, e.pts[i], e.pts[(i + 1) % n]); if (q < bd){ bd = q; best = i; } } return [e.pts[best], e.pts[(best + 1) % n]]; }
    return null;
  }
  function addDim(label, o, ctx){ return ED.run(label, function(){ var e = ED.add(o); D().lastDim = e.id; V().draw(); ctx.out('Chữ kích thước = ' + (e.t === 'dimr' ? (e.dia ? 'Ø' + Math.round(e.r * 2) : 'R' + Math.round(e.r)) : TC.d(e._meas, e.t === 'dima' ? 1 : 0) + (e.t === 'dima' ? '°' : ''))); return e; }); }
  function placeLinear(ctx, p1, p2, force, label){
    var dir = force, txt = null;
    function ask(){ return ctx.point('Vị trí đường kích thước', { rubber:false, dyn:false, opts:force === 'a' ? [['Mtext', 'M'], ['Chữ', 'T'], ['Góc', 'A']] : [['Mtext', 'M'], ['Chữ', 'T'], ['Góc', 'A'], ['Ngang', 'H'], ['Dọc', 'V'], ['Xoay', 'R']], onMove:function(p, temp){ var o = dimEnt(p1, p2, p, dir); if (txt) o.over = txt; temp.push(prev(o)); } }).then(function(q){
      if (q.kw === 'H' || q.kw === 'V'){ dir = q.kw.toLowerCase(); return ask(); }
      if (q.kw === 'R'){ dir = 'a'; return ask(); }
      if (q.kw === 'T' || q.kw === 'M') return ctx.val('Chữ kích thước (<> là giá trị đo)', txt || '<>').then(function(v){ txt = v && v !== '<>' ? v : null; return ask(); });
      if (q.kw === 'A') return ctx.val('Góc của chữ kích thước', '0').then(ask);
      if (q.p){ var o = dimEnt(p1, p2, q.p, dir); if (txt) o.over = txt; return addDim(label, o, ctx); } }); }
    return ask();
  }
  function linear(ctx, force, label){
    ctx.badge('D');
    return ctx.point('Gốc đường gióng thứ nhất', { def:'chọn đối tượng' }).then(function(r){
      if (r.enter) return ED.pick(ctx, 'Chọn đối tượng cần ghi kích thước', { types:['line', 'pline', 'arc', 'circle'] }).then(function(q){ if (!q.e) return; var ab = endsOf(q.e, q.p); return placeLinear(ctx, ab[0], ab[1], force, label); });
      if (!r.p) return; var p1 = r.p;
      return ctx.point('Gốc đường gióng thứ hai', { base:p1 }).then(function(q){ if (q.p) return placeLinear(ctx, p1, q.p, force, label); });
    });
  }
  IMPL.DIMLINEAR = function(ctx){ return linear(ctx, null, 'DIMLINEAR'); };
  IMPL.DIMALIGNED = function(ctx){ return linear(ctx, 'a', 'DIMALIGNED'); };
  function radial(ctx, dia){
    ctx.badge(dia ? 'DDI' : 'DRA');
    return ED.pick(ctx, 'Chọn cung hoặc đường tròn', { types:['arc', 'circle'] }).then(function(r){ if (!r.e) return; var e = r.e, mk = function(p){ return ED.make({ t:'dimr', c:e.c, r:e.r, ang:G.ang(e.c, p), dia:dia, style:dimStyle() }); };
      ctx.out('Chữ kích thước = ' + (dia ? 'Ø' + Math.round(e.r * 2) : 'R' + Math.round(e.r)));
      return ctx.point('Vị trí đường kích thước', { rubber:false, dyn:false, opts:[['Mtext', 'M'], ['Chữ', 'T'], ['Góc', 'A']], onMove:function(p, temp){ temp.push(prev(mk(p))); } }).then(function(q){ if (q.p) return addDim(dia ? 'DIMDIAMETER' : 'DIMRADIUS', mk(q.p), ctx); }); });
  }
  IMPL.DIMRADIUS = function(ctx){ return radial(ctx, false); };
  IMPL.DIMDIAMETER = function(ctx){ return radial(ctx, true); };
  function angDim(l1, l2, p){
    var X = ED.lineInt(l1.p1, l1.p2, l2.p1, l2.p2); if (!X) return null;
    var a1 = G.ang(l1.p1, l1.p2), a2 = G.ang(l2.p1, l2.p2), ac = G.ang(X, p), cand = [[a1, a2], [a2, a1 + 180], [a1 + 180, a2 + 180], [a2 + 180, a1]].map(function(c){ return sweep(c[0], c[1]) > 180 ? [c[1], c[0]] : c; });
    var pick = cand.filter(function(c){ return G.inArc(ac, c[0], c[1]); })[0] || cand[0];
    return ED.make({ t:'dima', c:X, a0:pick[0], a1:pick[1], r:Math.max(300, G.dist(X, p)), style:dimStyle() });
  }
  IMPL.DIMANGULAR = function(ctx){
    ctx.badge('DAN');
    return ED.pick(ctx, 'Chọn cung, đường tròn, đoạn thẳng', { def:'chỉ định đỉnh', types:['line', 'arc'] }).then(function(r){ if (!r.e) return;
      if (r.e.t === 'arc'){ var e = r.e; return ctx.point('Vị trí cung kích thước', { rubber:false, onMove:function(p, temp){ temp.push(prev(ED.make({ t:'dima', c:e.c, a0:e.a0, a1:e.a1, r:Math.max(e.r * 1.1, G.dist(e.c, p)) }))); } }).then(function(q){ if (q.p) return addDim('DIMANGULAR', ED.make({ t:'dima', c:e.c, a0:e.a0, a1:e.a1, r:Math.max(e.r * 1.1, G.dist(e.c, q.p)), style:dimStyle() }), ctx); }); }
      var l1 = r.e;
      return ED.pick(ctx, 'Chọn đoạn thẳng thứ hai', { types:['line'] }).then(function(q){ if (!q.e) return; if (!ED.lineInt(l1.p1, l1.p2, q.e.p1, q.e.p2)){ ctx.out('Hai đoạn thẳng song song.'); return; }
        return ctx.point('Vị trí cung kích thước', { rubber:false, opts:[['Mtext', 'M'], ['Chữ', 'T'], ['Góc', 'A'], ['Góc phần tư', 'Q']], onMove:function(p, temp){ var o = angDim(l1, q.e, p); if (o) temp.push(prev(o)); } }).then(function(z){ if (z.p) return addDim('DIMANGULAR', angDim(l1, q.e, z.p), ctx); }); }); });
  };
  // dim thông minh: rê lên đối tượng để xem trước loại kích thước phù hợp (UI-F05)
  IMPL.DIM = function(ctx){
    ctx.badge('DIM');
    function preview(e, p){
      if (e.t === 'line'){ var u = [e.p2[0] - e.p1[0], e.p2[1] - e.p1[1]], L = Math.hypot(u[0], u[1]) || 1, n = [-u[1] / L, u[0] / L], s = (p[0] - e.p1[0]) * n[0] + (p[1] - e.p1[1]) * n[1], off = (s >= 0 ? 1 : -1) * Math.max(600, Math.abs(s)), m = [(e.p1[0] + e.p2[0]) / 2 + n[0] * off, (e.p1[1] + e.p2[1]) / 2 + n[1] * off], hv = Math.abs(u[0]) < 1e-6 || Math.abs(u[1]) < 1e-6; return dimEnt(e.p1, e.p2, m, hv ? null : 'a'); }
      if (e.t === 'circle') return ED.make({ t:'dimr', c:e.c, r:e.r, ang:G.ang(e.c, p), dia:true });
      if (e.t === 'arc') return ED.make({ t:'dimr', c:e.c, r:e.r, ang:G.ang(e.c, p), dia:false });
      if (e.t === 'pline'){ var ab = endsOf(e, p); return dimEnt(ab[0], ab[1], p, null); }
      return null;
    }
    function loop(){
      return ctx.point('Chọn đối tượng hoặc chỉ định gốc đường gióng thứ nhất', { pick:false, opts:[['Góc', 'A'], ['Cùng gốc', 'B'], ['Nối tiếp', 'C'], ['Tọa độ', 'O'], ['Gióng thẳng', 'G'], ['Phân bố', 'D'], ['Layer', 'L'], ['Hoàn tác', 'U']],
        onMove:function(p, temp){ var s = V().mToS(p) || V().toS(p), id = V().hitTest(s[0], s[1]), e = id && D().byId[id]; V().hover = id; if (e){ var o = preview(e, p); if (o) temp.push(ED.temp([o], { color:TC.cssv('--c-accent') })); } } }).then(function(r){
        if (r.enter) return;
        if (r.kw === 'A') return IMPL.DIMANGULAR(ctx).then(loop);
        if (r.kw === 'B') return dimChain(ctx, true).then(loop);
        if (r.kw === 'C') return dimChain(ctx, false).then(loop);
        if (r.kw === 'U'){ ED.undo(1); return loop(); }
        if (r.kw === 'L') return ctx.val('Layer cho kích thước mới', 'KT-DIM').then(function(v){ if (D().layerMap[v]){ ED.cur().dimLayer = v; ctx.out('Kích thước mới đặt trên layer ' + esc(v) + '.'); } return loop(); });
        if (r.kw){ ctx.out('Tùy chọn “' + esc(r.kw) + '” của DIM: prompt giống AutoCAD; prototype minh họa Góc, Cùng gốc, Nối tiếp.'); return loop(); }
        var s = r.s || V().mToS(r.p), id = s && V().hitTest(s[0], s[1]), e = id && D().byId[id];
        if (e && (e.t === 'circle' || e.t === 'arc')) return ctx.point('Vị trí kích thước ' + (e.t === 'circle' ? 'đường kính' : 'bán kính'), { rubber:false, onMove:function(p, temp){ temp.push(prev(preview(e, p))); } }).then(function(q){ if (q.p){ var o = preview(e, q.p); o.style = dimStyle(); return addDim('DIM', o, ctx); } }).then(loop);
        if (e && (e.t === 'line' || e.t === 'pline')){ var ab = endsOf(e, r.p); return placeLinear(ctx, ab[0], ab[1], e.t === 'line' && Math.abs(ab[0][0] - ab[1][0]) > 1e-6 && Math.abs(ab[0][1] - ab[1][1]) > 1e-6 ? 'a' : null, 'DIM').then(loop); }
        var p1 = r.p; return ctx.point('Gốc đường gióng thứ hai', { base:p1 }).then(function(q){ if (q.p) return placeLinear(ctx, p1, q.p, null, 'DIM'); }).then(loop);
      });
    }
    return loop();
  };
  function dimChain(ctx, base){
    var d = D(), last = d.lastDim && d.byId[d.lastDim];
    var start = last && last.t === 'dim' ? Promise.resolve(last) : ED.pick(ctx, base ? 'Chọn kích thước gốc' : 'Chọn kích thước nối tiếp', { types:['dim'] }).then(function(r){ return r.e; });
    return start.then(function(first){ if (!first) return; var ref = first, n = 0, step = 0;
      function next(p){ var o = ED.clean(ref); delete o.id; delete o.over; delete o.tdx; delete o.tdy;
        if (base){ o.p1 = first.p1; o.p2 = p; step++; if (o.dir === 'a') o.off = (first.off || 0) + Math.sign(first.off || 1) * 500 * (n + 1); else { var dl = ED.dimPts(first).d1, sg = o.dir === 'h' ? Math.sign(dl[1] - first.p1[1]) || 1 : Math.sign(dl[0] - first.p1[0]) || 1; o.pos = first.pos + sg * 500 * (n + 1); } }
        else o.p1 = G.dist(ref.p1, p) < G.dist(ref.p2, p) ? ref.p1 : ref.p2, o.p2 = p;
        return o; }
      return ED.run(base ? 'DIMBASELINE' : 'DIMCONTINUE', function loop(){ return ctx.point('Gốc đường gióng tiếp theo', { opts:[['Chọn', 'S'], ['Hoàn tác', 'U']], def:'Chọn', onMove:function(p, temp){ temp.push(prev(next(p))); } }).then(function(q){
        if (q.enter) return;
        if (q.kw === 'S') return ED.pick(ctx, 'Chọn kích thước', { types:['dim'] }).then(function(z){ if (z.e){ first = ref = z.e; n = 0; } return loop(); });
        if (q.kw === 'U'){ ED.undo(1); return loop(); }
        if (q.p){ var e = ED.add(next(q.p)); n++; ref = e; D().lastDim = e.id; V().draw(); ctx.out('Chữ kích thước = ' + TC.d(e._meas, 0)); return loop(); } }); });
    });
  }
  IMPL.DIMCONTINUE = function(ctx){ ctx.badge('DCO'); return dimChain(ctx, false); };
  IMPL.DIMBASELINE = function(ctx){ ctx.badge('DBA'); return dimChain(ctx, true); };
  IMPL.DIMEDIT = function(ctx){ return ctx.kw('Kiểu sửa kích thước', [['Về gốc', 'H'], ['Chữ mới', 'N'], ['Xoay chữ', 'R'], ['Nghiêng', 'O']], 'H').then(function(k){
    var get = k === 'N' ? ctx.val('Chữ kích thước mới (<> là giá trị đo)', '<>') : k === 'R' || k === 'O' ? ctx.val(k === 'R' ? 'Góc của chữ kích thước' : 'Góc nghiêng đường gióng', '0') : Promise.resolve(null);
    return get.then(function(v){ return ctx.sel('Chọn kích thước').then(function(ids){ var d = D(), dims = ids.map(function(id){ return d.byId[id]; }).filter(function(e){ return e && /^dim/.test(e.t); }); if (!dims.length){ ctx.out('Không có kích thước nào trong vùng chọn.'); return; }
      return ED.run('DIMEDIT', function(){ dims.forEach(function(e){ ED.mod(e, function(x){ if (k === 'H'){ delete x.tdx; delete x.tdy; } if (k === 'N'){ if (!v || v === '<>') delete x.over; else x.over = v; } if (k === 'R') x.trot = Number(v) || 0; if (k === 'O') x.obl = Number(v) || 0; }); }); TC.cad.clearSel(); ctx.out('Đã sửa ' + dims.length + ' kích thước.'); }); }); }); }); };
  IMPL.DIMTEDIT = function(ctx){ return ED.pick(ctx, 'Chọn kích thước', { types:['dim'] }).then(function(r){ if (!r.e) return; var e = r.e, tp = (e._p || []).filter(function(x){ return x.k === 'text'; })[0], base = tp ? [tp.p[0] - (e.tdx || 0), tp.p[1] - (e.tdy || 0)] : r.p;
    return ctx.point('Vị trí mới của chữ kích thước', { rubber:false, opts:[['Trái', 'L'], ['Phải', 'R'], ['Giữa', 'C'], ['Về gốc', 'H'], ['Góc', 'A']], onMove:function(p, temp){ var x = ED.clean(e); x.tdx = p[0] - base[0]; x.tdy = p[1] - base[1]; temp.push(prev(x)); } }).then(function(q){
      return ED.run('DIMTEDIT', function(){ ED.mod(e, function(x){ var q0 = ED.dimPts(x), half = G.dist(q0.d1, q0.d2) / 2 - 300, h = x.dir === 'h';
        if (q.kw === 'H' || q.kw === 'C'){ delete x.tdx; delete x.tdy; } else if (q.kw === 'L' || q.kw === 'R'){ var s = q.kw === 'L' ? -1 : 1; x.tdx = h ? s * half : 0; x.tdy = h ? 0 : s * half; } else if (q.kw === 'A'){ x.trot = 0; } else if (q.p){ x.tdx = q.p[0] - base[0]; x.tdy = q.p[1] - base[1]; } }); }); }); }); };
})();
