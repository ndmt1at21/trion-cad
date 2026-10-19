/* TrionCAD prototype · Đợt 5 (sau v1.0): tùy biến giao diện CUI (K07), bảng điều khiển LISP (K09), quản lý plugin (K10). */
(function(){
  'use strict';
  var TC = window.TC, $ = TC.$, $$ = TC.$$, esc = TC.esc, ic = TC.ic, G = TC.geo, IMPL = TC.cmd.impl, ED = TC.ed, SC = TC.screens, H = SC.h;
  function A(){ return TC.app; } function V(){ return TC.app.view; } function D(){ return TC.app.doc(); }
  var OPEN = SC.open5 = SC.open5 || {}; // mã màn hình → hàm mở (danh mục gọi khi bấm); tiện ích mở ở SC.cat (D1, PRO, MOD, RUN, plan, pick, flash)
  function clone(o){ return JSON.parse(JSON.stringify(o)); }
  function uid(){ return Math.random().toString(36).slice(2, 8); }
  function sleep(ms){ return new Promise(function(r){ setTimeout(r, ms); }); }
  function fmtMs(ms){ return ms >= 1000 ? TC.n(Math.round(ms / 100) / 10) + ' s' : TC.n(ms < 10 ? Math.round(ms * 10) / 10 : Math.round(ms)) + ' ms'; }
  function showPal(id){ A().showPalette(id); var f = $('.dock.flyout [data-fly="' + id + '"]'), h = f && f.closest('.dock').querySelector('.palhost'); if (f && h && h.hidden) f.click(); }

  /* ================= LISP: bộ thông dịch AutoLISP thu gọn (UI-K09) =================
     INT là số JS, REAL là Re, STR là chuỗi, LIST là mảng (cặp chấm: mảng có .dot), nil là null. Liên kết động như AutoLISP.
     Hàm chạy bất đồng bộ để (command …), (getpoint …) chờ người dùng trên vùng vẽ. */
  function Sym(n){ this.n = n; } function Re(v){ this.v = v; } function En(id){ this.id = id; } function Ss(ids, n){ this.ids = ids; this.n = n; } function Fn(o){ for (var k in o) this[k] = o[k]; }
  var SYMS = {}; function sym(n){ n = String(n).toUpperCase(); return SYMS[n] || (SYMS[n] = new Sym(n)); }
  var T = sym('T'), QT = sym('QUOTE'), NOP = new Sym(''), PAUSE = '\\';
  var GV = { PI:new Re(Math.PI), PAUSE:PAUSE }, BI = {}, SF = {}, ST = [], CUR = { ctx:null }, LIVE = null, SSN = 2, INIT = null, OUT = null, CLB = '';
  var L = TC.lisp = { busy:false, stop:false, src:null, defs:null, loaded:{} };
  // lỗi LISP: chụp vết gọi hàm (trong cùng trước) và biến cục bộ của từng khung lúc lỗi
  function LErr(vi, en, x){ this.vi = vi; this.en = en; if (arguments.length > 2) this.x = x; this.tr = ST.map(function(f){ return { f:f.f, a:f.a, form:f.form, vars:f.f.js ? null : f.f.args.concat(f.f.loc).map(function(n){ return [n, GV[n]]; }) }; }).reverse(); }
  function E(vi, en, x){ return arguments.length > 2 ? new LErr(vi, en, x) : new LErr(vi, en); }
  function bad(p, x){ return E('kiểu đối số sai: ' + p, 'bad argument type', x); }
  function R(v){ return new Re(v); }
  function isN(x){ return typeof x === 'number' || x instanceof Re; }
  function N(x, p){ if (typeof x === 'number') return x; if (x instanceof Re) return x.v; throw bad(p || 'numberp', x); }
  function mk(v, r){ return r ? R(v) : v | 0; }
  function anyR(a){ return a.some(function(x){ return x instanceof Re; }); }
  function S(x, p){ if (typeof x === 'string') return x; throw bad(p || 'stringp', x); }
  function lst(x, p){ if (x == null) return []; if (Array.isArray(x)) return x; throw bad(p || 'listp', x); }
  function nl(a){ return a.length ? a : null; }
  function dp(k, v){ var r = [k, v]; r.dot = true; return r; }

  // đọc mã nguồn thành các biểu thức; danh sách nhớ tệp, số dòng để báo vết gọi hàm
  var TOK = /[^\s()'";]+/y;
  function read(src, file){
    var i = 0, ln = 1, n = src.length, out = [];
    function fail(vi, en){ var e = E(vi, en); e.file = file; e.ln = ln; throw e; }
    function ws(){ for (;;){ var c = src[i]; if (c === '\n'){ ln++; i++; } else if (c === ' ' || c === '\t' || c === '\r' || c === '\f') i++; else if (c === ';'){ if (src[i + 1] === '|'){ var j = src.indexOf('|;', i + 2); j = j < 0 ? n : j + 2; ln += (src.slice(i, j).match(/\n/g) || []).length; i = j; } else while (i < n && src[i] !== '\n') i++; } else return; } }
    function form(){
      ws(); if (i >= n) fail('thiếu biểu thức sau dấu nháy', 'malformed input');
      var c = src[i];
      if (c === '('){ var l = [], at = ln; i++;
        for (;;){ ws(); if (i >= n) fail('thiếu dấu ngoặc đóng', 'malformed list on input');
          if (src[i] === ')'){ i++; break; }
          if (src[i] === '.' && l.length === 1 && /[\s()]/.test(src[i + 1] || ' ')){ i++; var cd = form(); ws(); if (src[i] !== ')') fail('cặp chấm sai cú pháp', 'malformed dotted pair'); i++; if (Array.isArray(cd) && !cd.dot) l = l.concat(cd); else if (cd != null){ l.push(cd); l.dot = true; } break; }
          l.push(form()); }
        if (!l.length) return null; l.ln = at; l.file = file; return l; }
      if (c === ')'){ i++; fail('dư dấu ngoặc đóng', 'extra right paren on input'); }
      if (c === "'"){ i++; return [QT, form()]; }
      if (c === '"'){ var s = ''; i++; while (i < n && src[i] !== '"'){ var ch = src[i++]; if (ch === '\\'){ ch = src[i++]; ch = ch === 'n' ? '\n' : ch === 't' ? '\t' : ch === 'r' ? '\r' : ch === 'e' ? '\x1b' : ch; } else if (ch === '\n') ln++; s += ch; } if (i >= n) fail('thiếu dấu nháy đóng chuỗi', 'malformed string on input'); i++; return s; }
      TOK.lastIndex = i; var t = TOK.exec(src)[0]; i += t.length;
      if (/^[+-]?\d+$/.test(t)) return Number(t) | 0;
      if (/^[+-]?(\d+\.\d*|\.\d+|\d+)(e[+-]?\d+)?$/i.test(t)) return R(Number(t));
      return /^nil$/i.test(t) ? null : sym(t);
    }
    for (;;){ ws(); if (i >= n) return out; out.push(form()); }
  }
  // in giá trị theo kiểu AutoLISP: số thực 6 chữ số có nghĩa, chuỗi có nháy (raw = princ)
  function fmtR(v){ if (!isFinite(v)) return v > 0 ? '1.#INF' : v < 0 ? '-1.#INF' : '-1.#IND'; var a = Math.abs(v);
    if (a && (a >= 1e6 || a < 1e-4)){ var p = v.toExponential(5).split('e'), m = p[0].replace(/0+$/, '').replace(/\.$/, '.0'), x = Number(p[1]); return m + 'e' + (x < 0 ? '-' : '+') + ('0' + Math.abs(x)).slice(-2); }
    var t = String(Number(v.toPrecision(6))); return /\./.test(t) ? t : t + '.0'; }
  function hex(id){ return (0x7ff4a3c00000 + id * 16).toString(16); }
  function show(x, raw){
    if (x == null) return 'nil'; if (typeof x === 'number') return String(x); if (x instanceof Re) return fmtR(x.v);
    if (typeof x === 'string') return raw ? x : '"' + x.replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\n/g, '\\n').replace(/\t/g, '\\t') + '"';
    if (x instanceof Sym) return x.n; if (x instanceof En) return '<Entity name: ' + hex(x.id) + '>'; if (x instanceof Ss) return '<Selection set: ' + x.n.toString(16) + '>';
    if (x instanceof Fn) return '#<' + (x.js ? 'SUBR' : 'USUBR') + ' @' + hex(x.name.length * 977) + ' ' + x.name + '>';
    if (Array.isArray(x)) return '(' + (x.dot ? show(x[0], raw) + ' . ' + show(x[1], raw) : x.map(function(e){ return show(e, raw); }).join(' ')) + ')';
    return String(x);
  }
  function typ(x){ return x == null ? null : typeof x === 'number' ? 'INT' : x instanceof Re ? 'REAL' : typeof x === 'string' ? 'STR' : Array.isArray(x) ? 'LIST' : x instanceof En ? 'ENAME' : x instanceof Ss ? 'PICKSET' : x instanceof Fn ? (x.js ? 'SUBR' : 'USUBR') : 'SYM'; }

  // tính giá trị; cứ 1500 bước nhường trình duyệt một nhịp để nút Dừng, Esc có tác dụng với vòng lặp dài
  var steps = 0;
  function look(n){ return n in GV ? GV[n] : BI[n] || null; }
  async function ev(x){
    if (++steps % 1500 === 0){ await sleep(0); if (L.stop) throw E('hàm bị hủy', 'Function cancelled'); }
    if (x instanceof Sym) return x === T ? T : look(x.n);
    if (!Array.isArray(x) || x.dot) return x;
    var h = x[0];
    if (h instanceof Sym && SF[h.n]) return SF[h.n](x);
    var f = h instanceof Sym ? look(h.n) : await ev(h);
    if (!(f instanceof Fn)) throw E('hàm chưa định nghĩa', 'no function definition', h instanceof Sym ? h : f);
    var a = []; for (var i = 1; i < x.length; i++) a.push(await ev(x[i]));
    return call(f, a, x);
  }
  async function call(f, a, form){
    ST.push({ f:f, a:a, form:form });
    try {
      if (f.js){ if (a.length < f.min) throw E('quá ít đối số', 'too few arguments'); if (f.max != null && a.length > f.max) throw E('quá nhiều đối số', 'too many arguments'); return await f.js(a); }
      if (a.length < f.args.length) throw E('quá ít đối số', 'too few arguments'); if (a.length > f.args.length) throw E('quá nhiều đối số', 'too many arguments');
      var ns = f.args.concat(f.loc), sv = ns.map(function(n){ return n in GV ? [GV[n]] : null; });
      ns.forEach(function(n, i){ GV[n] = i < a.length ? a[i] : null; });
      try { var r = null; for (var k = 0; k < f.body.length; k++) r = await ev(f.body[k]); return r; }
      finally { ns.forEach(function(n, i){ if (sv[i]) GV[n] = sv[i][0]; else delete GV[n]; }); }
    } finally { ST.pop(); }
  }
  SF.QUOTE = function(x){ return x[1]; };
  SF.SETQ = async function(x){ if (x.length % 2 === 0) throw E('quá ít đối số', 'too few arguments'); var v = null; for (var i = 1; i < x.length; i += 2){ if (!(x[i] instanceof Sym) || x[i] === T) throw bad('symbolp', x[i]); v = await ev(x[i + 1]); GV[x[i].n] = v; } return v; };
  SF.IF = async function(x){ if (x.length < 3) throw E('quá ít đối số', 'too few arguments'); return (await ev(x[1])) != null ? ev(x[2]) : x.length > 3 ? ev(x[3]) : null; };
  SF.COND = async function(x){ for (var i = 1; i < x.length; i++){ var c = lst(x[i]), t = await ev(c[0]); if (t != null){ for (var k = 1; k < c.length; k++) t = await ev(c[k]); return t; } } return null; };
  SF.PROGN = async function(x){ var r = null; for (var i = 1; i < x.length; i++) r = await ev(x[i]); return r; };
  SF.WHILE = async function(x){ var r = null; while ((await ev(x[1])) != null) for (var i = 2; i < x.length; i++) r = await ev(x[i]); return r; };
  SF.REPEAT = async function(x){ var n = await ev(x[1]), r = null; if (typeof n !== 'number') throw bad('fixnump', n); while (n-- > 0) for (var i = 2; i < x.length; i++) r = await ev(x[i]); return r; };
  SF.FOREACH = async function(x){ if (!(x[1] instanceof Sym)) throw bad('symbolp', x[1]); var v = x[1].n, l = lst(await ev(x[2])), had = v in GV, old = GV[v], r = null; try { for (var k = 0; k < l.length; k++){ GV[v] = l[k]; for (var i = 3; i < x.length; i++) r = await ev(x[i]); } } finally { if (had) GV[v] = old; else delete GV[v]; } return r; };
  SF.AND = async function(x){ for (var i = 1; i < x.length; i++) if ((await ev(x[i])) == null) return null; return T; };
  SF.OR = async function(x){ for (var i = 1; i < x.length; i++) if ((await ev(x[i])) != null) return T; return null; };
  SF.LAMBDA = function(x){ return lam(x.slice(1), 'LAMBDA', x); };
  SF.FUNCTION = function(x){ return ev(x[1]); };
  SF.DEFUN = function(x){ var s = x[1]; if (!(s instanceof Sym)) throw bad('symbolp', s); var f = lam(x.slice(2), s.n, x); GV[s.n] = f; if (L.defs) L.defs.push(s.n); if (/^C:./.test(s.n)) regCmd(s.n, f); return s; };
  function lam(p, name, x){ var args = [], loc = [], cur = args; lst(p[0]).forEach(function(s){ if (!(s instanceof Sym)) throw bad('symbolp', s); if (s.n === '/') cur = loc; else cur.push(s.n); }); return new Fn({ name:name, args:args, loc:loc, body:p.slice(1), file:x.file, ln:x.ln }); }

  /* ---------- hàm có sẵn ---------- */
  function bi(names, min, max, fn){ names.split(' ').forEach(function(k){ BI[k] = new Fn({ name:k, js:fn, min:min, max:max }); }); }
  function cmpv(x){ return typeof x === 'string' ? x : N(x); }
  function chain(t){ return function(a){ for (var i = 0; i < a.length - 1; i++) if (!t(cmpv(a[i]), cmpv(a[i + 1]))) return null; return T; }; }
  function equal(x, y, fz){ if (isN(x) && isN(y)) return Math.abs(N(x) - N(y)) <= fz; if (Array.isArray(x) && Array.isArray(y)) return x.length === y.length && !!x.dot === !!y.dot && x.every(function(e, i){ return equal(e, y[i], fz); }); if (x instanceof En && y instanceof En) return x.id === y.id; return x === y; }
  function fnOf(x){ if (x instanceof Fn) return x; if (x instanceof Sym && look(x.n) instanceof Fn) return look(x.n); if (Array.isArray(x) && x[0] === sym('LAMBDA')) return SF.LAMBDA(x); throw E('hàm chưa định nghĩa', 'no function definition', x); }
  function car(x){ var l = lst(x); return l.length ? l[0] : null; }
  function cdr(x){ var l = lst(x); return l.dot ? l[1] : nl(l.slice(1)); }
  function P2(x){ var l = lst(x, 'pointp'); if (l.length < 2) throw bad('pointp', x); return [N(l[0], 'pointp'), N(l[1], 'pointp')]; }
  function P3(p){ return [R(p[0]), R(p[1]), R(0)]; }
  bi('+', 0, null, function(a){ var s = 0; a.forEach(function(x){ s += N(x); }); return mk(s, anyR(a)); });
  bi('*', 0, null, function(a){ var s = 1; a.forEach(function(x){ s *= N(x); }); return mk(s, anyR(a)); });
  bi('-', 0, null, function(a){ if (!a.length) return 0; var s = N(a[0]); if (a.length === 1) s = -s; for (var i = 1; i < a.length; i++) s -= N(a[i]); return mk(s, anyR(a)); });
  bi('/', 0, null, function(a){ if (!a.length) return 0; var r = anyR(a), s = N(a[0]); for (var i = 1; i < a.length; i++){ var dv = N(a[i]); if (dv === 0) throw E('chia cho 0', 'divide by zero'); s = r ? s / dv : Math.trunc(s / dv); } return mk(s, r); });
  bi('1+', 1, 1, function(a){ return mk(N(a[0]) + 1, a[0] instanceof Re); }); bi('1-', 1, 1, function(a){ return mk(N(a[0]) - 1, a[0] instanceof Re); });
  bi('ABS', 1, 1, function(a){ return mk(Math.abs(N(a[0])), a[0] instanceof Re); });
  bi('MAX', 1, null, function(a){ return mk(Math.max.apply(null, a.map(function(x){ return N(x); })), anyR(a)); }); bi('MIN', 1, null, function(a){ return mk(Math.min.apply(null, a.map(function(x){ return N(x); })), anyR(a)); });
  bi('REM', 1, null, function(a){ var s = N(a[0]); for (var i = 1; i < a.length; i++){ var dv = N(a[i]); if (dv === 0) throw E('chia cho 0', 'divide by zero'); s %= dv; } return mk(s, anyR(a)); });
  bi('FIX', 1, 1, function(a){ return Math.trunc(N(a[0])) | 0; }); bi('FLOAT', 1, 1, function(a){ return R(N(a[0])); });
  bi('SQRT', 1, 1, function(a){ return R(Math.sqrt(N(a[0]))); }); bi('SIN', 1, 1, function(a){ return R(Math.sin(N(a[0]))); }); bi('COS', 1, 1, function(a){ return R(Math.cos(N(a[0]))); });
  bi('ATAN', 1, 2, function(a){ return R(a.length > 1 ? Math.atan2(N(a[0]), N(a[1])) : Math.atan(N(a[0]))); }); bi('EXP', 1, 1, function(a){ return R(Math.exp(N(a[0]))); }); bi('LOG', 1, 1, function(a){ return R(Math.log(N(a[0]))); });
  bi('EXPT', 2, 2, function(a){ return mk(Math.pow(N(a[0]), N(a[1])), anyR(a)); });
  bi('=', 1, null, chain(function(x, y){ return x === y; })); bi('<', 1, null, chain(function(x, y){ return x < y; })); bi('>', 1, null, chain(function(x, y){ return x > y; })); bi('<=', 1, null, chain(function(x, y){ return x <= y; })); bi('>=', 1, null, chain(function(x, y){ return x >= y; }));
  bi('/=', 1, null, function(a){ for (var i = 0; i < a.length - 1; i++) if (cmpv(a[i]) === cmpv(a[i + 1])) return null; return T; });
  bi('EQ', 2, 2, function(a){ return a[0] === a[1] || (a[0] instanceof En && a[1] instanceof En && a[0].id === a[1].id) ? T : null; });
  bi('EQUAL', 2, 3, function(a){ return equal(a[0], a[1], a.length > 2 ? N(a[2]) : 0) ? T : null; });
  bi('NOT NULL', 1, 1, function(a){ return a[0] == null ? T : null; });
  bi('ZEROP', 1, 1, function(a){ return N(a[0]) === 0 ? T : null; }); bi('MINUSP', 1, 1, function(a){ return N(a[0]) < 0 ? T : null; }); bi('NUMBERP', 1, 1, function(a){ return isN(a[0]) ? T : null; });
  bi('ATOM', 1, 1, function(a){ return Array.isArray(a[0]) ? null : T; }); bi('LISTP', 1, 1, function(a){ return a[0] == null || Array.isArray(a[0]) ? T : null; }); bi('VL-CONSP', 1, 1, function(a){ return Array.isArray(a[0]) ? T : null; });
  bi('BOUNDP', 1, 1, function(a){ return a[0] instanceof Sym && GV[a[0].n] != null ? T : null; });
  bi('TYPE', 1, 1, function(a){ var t = typ(a[0]); return t ? sym(t) : null; });
  bi('LIST', 0, null, function(a){ return nl(a.slice()); });
  bi('CONS', 2, 2, function(a){ var d = a[1]; if (d == null) return [a[0]]; if (Array.isArray(d) && !d.dot) return [a[0]].concat(d); return dp(a[0], d); });
  bi('CAR', 1, 1, function(a){ return car(a[0]); }); bi('CDR', 1, 1, function(a){ return cdr(a[0]); });
  bi('CADR', 1, 1, function(a){ return car(cdr(a[0])); }); bi('CDDR', 1, 1, function(a){ return cdr(cdr(a[0])); }); bi('CAAR', 1, 1, function(a){ return car(car(a[0])); }); bi('CDAR', 1, 1, function(a){ return cdr(car(a[0])); }); bi('CADDR', 1, 1, function(a){ return car(cdr(cdr(a[0]))); }); bi('CADAR', 1, 1, function(a){ return car(cdr(car(a[0]))); });
  bi('NTH', 2, 2, function(a){ var l = lst(a[1]), k = N(a[0], 'fixnump'); return l[k] !== undefined && !(l.dot && k > 0) ? l[k] : null; });
  bi('LENGTH', 1, 1, function(a){ return lst(a[0]).length; }); bi('REVERSE', 1, 1, function(a){ return nl(lst(a[0]).slice().reverse()); });
  bi('APPEND', 0, null, function(a){ return nl([].concat.apply([], a.map(function(x){ return lst(x); }))); });
  bi('LAST', 1, 1, function(a){ var l = lst(a[0]); return l.length ? l[l.length - 1] : null; });
  bi('MEMBER', 2, 2, function(a){ var l = lst(a[1]); for (var i = 0; i < l.length; i++) if (equal(a[0], l[i], 0)) return l.slice(i); return null; });
  bi('ASSOC', 2, 2, function(a){ var l = lst(a[1]); for (var i = 0; i < l.length; i++) if (Array.isArray(l[i]) && equal(l[i][0], a[0], 0)) return l[i]; return null; });
  bi('SUBST', 3, 3, function(a){ return nl(lst(a[2]).map(function(x){ return equal(x, a[1], 0) ? a[0] : x; })); });
  bi('MAPCAR', 2, null, async function(a){ var f = fnOf(a[0]), ls = a.slice(1).map(function(x){ return lst(x); }), n = Math.min.apply(null, ls.map(function(l){ return l.length; })), out = []; for (var i = 0; i < n; i++) out.push(await call(f, ls.map(function(l){ return l[i]; }))); return nl(out); });
  bi('APPLY', 2, 2, function(a){ return call(fnOf(a[0]), lst(a[1]).slice()); });
  bi('STRCAT', 0, null, function(a){ return a.map(function(x){ return S(x); }).join(''); });
  bi('STRLEN', 0, null, function(a){ return a.reduce(function(s, x){ return s + S(x).length; }, 0); });
  bi('SUBSTR', 2, 3, function(a){ var s = S(a[0]), b = N(a[1], 'fixnump') - 1; return a.length > 2 ? s.substr(b, N(a[2], 'fixnump')) : s.slice(b); });
  bi('STRCASE', 1, 2, function(a){ return a[1] != null ? S(a[0]).toLowerCase() : S(a[0]).toUpperCase(); });
  bi('ITOA', 1, 1, function(a){ if (typeof a[0] !== 'number') throw bad('fixnump', a[0]); return String(a[0]); });
  bi('ATOI', 1, 1, function(a){ return parseInt(S(a[0]), 10) | 0; }); bi('ATOF', 1, 1, function(a){ return R(parseFloat(S(a[0])) || 0); });
  bi('RTOS', 1, 3, function(a){ return N(a[0]).toFixed(TC.clamp(a.length > 2 ? N(a[2]) : Number(TC.SV ? TC.SV.get('LUPREC') : 2), 0, 8)); });
  bi('ANGTOS', 1, 3, function(a){ return (N(a[0]) * 180 / Math.PI).toFixed(a.length > 2 ? N(a[2]) : 0); });
  bi('CHR', 1, 1, function(a){ return String.fromCharCode(N(a[0])); }); bi('ASCII', 1, 1, function(a){ return S(a[0]).charCodeAt(0) || 0; });
  bi('VL-PRINC-TO-STRING', 1, 1, function(a){ return show(a[0], true); }); bi('VL-PRIN1-TO-STRING', 1, 1, function(a){ return show(a[0]); });
  bi('VL-STRING-SEARCH', 2, 3, function(a){ var k = S(a[1]).indexOf(S(a[0]), a.length > 2 ? N(a[2]) : 0); return k < 0 ? null : k; });
  bi('READ', 1, 1, function(a){ var f = read(S(a[0])); return f.length ? f[0] : null; });
  bi('PRINC', 0, 2, function(a){ if (!a.length) return NOP; emit(show(a[0], true)); return a[0]; });
  bi('PRIN1', 0, 2, function(a){ if (!a.length) return NOP; emit(show(a[0])); return a[0]; });
  bi('PRINT', 1, 2, function(a){ emit('\n' + show(a[0]) + ' '); return a[0]; });
  bi('PROMPT', 1, 1, function(a){ emit(S(a[0])); return null; }); bi('TERPRI', 0, 0, function(){ emit('\n'); return null; });
  bi('ALERT', 1, 1, function(a){ return new Promise(function(res){ TC.dialog({ id:'lspalert', title:'TrionCAD', help:false, w:420, body:'<p style="margin:0;white-space:pre-wrap">' + esc(S(a[0])) + '</p>', foot:'<span class="grow"></span><button class="btn primary" type="button" data-x="close">Đóng</button>', onClose:function(){ res(null); } }); }); });
  bi('EXIT QUIT', 0, 0, function(){ throw E('thoát theo yêu cầu của chương trình', 'quit / exit abort'); });
  bi('VER', 0, 0, function(){ return 'Visual LISP 2.0 (vi) · TrionCAD'; });
  bi('VL-LOAD-COM', 0, 0, function(){ return null; });
  bi('LOAD', 1, 2, function(a){ var f = findF(S(a[0])); if (!f){ if (a.length > 1) return a[1]; throw E('không tìm thấy tệp', 'LOAD failed', a[0]); } return loadF(f); });
  // DCL: dựng hộp thoại của thongkethep.dcl như APPLOAD (UI-K11)
  bi('LOAD_DIALOG', 1, 1, function(){ return 1; }); bi('NEW_DIALOG', 2, 4, function(){ return T; }); bi('START_DIALOG', 0, 0, function(){ SC.dclDemo(); return 1; });
  bi('DONE_DIALOG UNLOAD_DIALOG ACTION_TILE SET_TILE MODE_TILE', 0, null, function(){ return null; }); bi('GET_TILE', 1, 1, function(){ return ''; });

  /* ---------- bản vẽ: biến hệ thống, đối tượng, vùng chọn, nhập từ người dùng ---------- */
  function need(){ var d = D(); if (!d) throw E('chưa mở bản vẽ nào', 'no drawing'); return d; }
  function ent(x){ if (!(x instanceof En)) throw bad('lentityp', x); var e = need().byId[x.id]; if (!e) throw E('đối tượng không còn trong bản vẽ', 'bad argument type: lentityp', x); return e; }
  function g(l, k){ var p = lst(l).filter(function(x){ return Array.isArray(x) && x[0] === k; })[0]; return p ? (p.dot ? p[1] : nl(p.slice(1))) : null; }
  function getvar(n){ n = n.toUpperCase(); var d = D(), t = new Date(), z = function(x){ return ('0' + x).slice(-2); };
    var x = { DWGNAME:d ? d.name : null, DWGPREFIX:d ? String(d.path || '').replace(/[^\\]+$/, '') : null, ACADVER:'24.3s (TrionCAD 1.0)', PRODUCT:'TrionCAD', CDATE:R(Number('' + t.getFullYear() + z(t.getMonth() + 1) + z(t.getDate()) + '.' + z(t.getHours()) + z(t.getMinutes()) + z(t.getSeconds()))) }[n];
    if (x !== undefined) return x;
    var df = TC.SV && TC.SV.def(n); if (!df) return null; var v = TC.SV.get(n); return df.t === 'r' ? R(Number(v)) : df.t === 's' ? String(v) : Number(v) | 0; }
  function setvar(n, v){ n = n.toUpperCase(); var df = TC.SV && TC.SV.def(n);
    if (!df || df.ro) throw E('không đặt được biến ' + n + (df ? ' (chỉ đọc)' : ' (không có biến này)'), 'AutoCAD variable setting rejected', v);
    var p = TC.SV.parse(n, df.t === 's' ? S(v) : String(N(v))); if (p.err) throw E('giá trị bị từ chối: ' + n + '. ' + p.err, 'AutoCAD variable setting rejected', v);
    var r = TC.SV.set(n, p.v, L.src || 'Bảng điều khiển LISP'); if (r !== true) throw E(String(r), 'AutoCAD variable setting rejected', v); A().renderStatus(); return v; }
  bi('GETVAR', 1, 1, function(a){ return getvar(S(a[0])); }); bi('SETVAR', 2, 2, function(a){ return setvar(S(a[0]), a[1]); });
  var TY0 = { line:'LINE', pline:'LWPOLYLINE', circle:'CIRCLE', arc:'ARC', ellipse:'ELLIPSE', text:'TEXT', mtext:'MTEXT', insert:'INSERT', hatch:'HATCH', dim:'DIMENSION', dimr:'DIMENSION', dima:'DIMENSION', point:'POINT', spline:'SPLINE', xline:'XLINE', ray:'RAY', mleader:'MULTILEADER', image:'IMAGE', table:'ACAD_TABLE', attdef:'ATTDEF', mline:'MLINE', vpframe:'VIEWPORT' };
  function rad(a){ return R((a || 0) * Math.PI / 180); }
  function dxf(e){ var r = [dp(-1, new En(e.id)), dp(0, TY0[e.t] || e.t.toUpperCase()), dp(5, (e.id * 4 + 672).toString(16).toUpperCase()), dp(8, e.layer)], P = function(k, p){ if (p) r.push([k].concat(P3(p))); };
    if (e.color != null) r.push(dp(62, e.color));
    if (e.t === 'line'){ P(10, e.p1); P(11, e.p2); }
    else if (e.t === 'circle' || e.t === 'arc'){ P(10, e.c); r.push(dp(40, R(e.r))); if (e.t === 'arc') r.push(dp(50, rad(e.a0)), dp(51, rad(e.a1))); }
    else if (e.t === 'text' || e.t === 'mtext'){ P(10, e.p); r.push(dp(40, R(e.h || 0)), dp(1, String(e.s || '')), dp(7, e.style || 'Standard'), dp(50, rad(e.rot))); }
    else if (e.t === 'insert'){ r.push(dp(2, e.name)); P(10, e.p); r.push(dp(41, R(e.sx || 1)), dp(42, R(e.sy || e.sx || 1)), dp(43, R(1)), dp(50, rad(e.rot))); }
    else if (e.t === 'pline'){ r.push(dp(90, e.pts.length), dp(70, e.closed ? 1 : 0)); e.pts.forEach(function(p){ r.push([10, R(p[0]), R(p[1])]); }); }
    else if (e.t === 'hatch') r.push(dp(2, e.pat || 'SOLID'), dp(41, R(e.sc || 1)));
    else if (e.p) P(10, e.p);
    return r; }
  function mkLayer(d, n){ ED.run('Layer mới', function(){ var done = ED.state(function(){ return d.layers.map(function(l){ return Object.assign({}, l); }); }, function(s){ d.layers = s.map(function(l){ return Object.assign({}, l); }); d.layerMap = {}; d.layers.forEach(function(l){ d.layerMap[l.name] = l; }); A().refreshPalettes(); A().renderRibbon(); }); var l = { name:n, color:7, lt:'Continuous', lw:.25, desc:'Tạo bằng LISP', on:true, frz:false, lck:false, plot:true, vpfrz:false, count:0 }; d.layers.push(l); d.layerMap[n] = l; done(); }); TC.emit('layers'); }
  function entmod(l){ var e = ent(g(l, -1)), d = need();
    ED.run('ENTMOD', function(){ ED.mod(e, function(x){ var v;
      if ((v = g(l, 8)) != null){ if (!d.layerMap[S(v)]) mkLayer(d, v); x.layer = v; }
      if ((v = g(l, 62)) != null){ if (N(v) === 256) delete x.color; else x.color = N(v); }
      if ((v = g(l, 1)) != null && 's' in x) x.s = S(v);
      if ((v = g(l, 40)) != null){ if ('r' in x) x.r = N(v); else if ('h' in x) x.h = N(v); }
      if ((v = g(l, 10)) != null){ var p = P2(v); if (x.p1) x.p1 = p; else if (x.c) x.c = p; else if (x.p) x.p = p; }
      if ((v = g(l, 11)) != null && x.p2) x.p2 = P2(v);
      if ((v = g(l, 50)) != null && (x.t === 'text' || x.t === 'insert' || x.t === 'mtext')) x.rot = N(v) * 180 / Math.PI;
      if ((v = g(l, 7)) != null && 'style' in x) x.style = S(v); }); });
    V().draw(); A().refreshPalettes(); return l; }
  function entmake(l){ var ty = String(g(l, 0) || '').toUpperCase(), d = need(), o = {}, ly = g(l, 8);
    if (ly != null){ if (!d.layerMap[S(ly)]) mkLayer(d, ly); o.layer = ly; } if (g(l, 62) != null) o.color = N(g(l, 62));
    if (ty === 'LINE') Object.assign(o, { t:'line', p1:P2(g(l, 10)), p2:P2(g(l, 11)) });
    else if (ty === 'CIRCLE') Object.assign(o, { t:'circle', c:P2(g(l, 10)), r:N(g(l, 40)) });
    else if (ty === 'TEXT') Object.assign(o, { t:'text', p:P2(g(l, 10)), h:N(g(l, 40) || 250), s:S(g(l, 1) || ''), rot:g(l, 50) != null ? N(g(l, 50)) * 180 / Math.PI : 0, style:g(l, 7) || ED.cur(d).tstyle, al:'l' });
    else if (ty === 'POINT') Object.assign(o, { t:'point', p:P2(g(l, 10)) });
    else throw E('entmake: loại “' + ty + '” chưa hỗ trợ trong prototype (LINE, CIRCLE, TEXT, POINT)', 'entmake', l);
    ED.run('ENTMAKE', function(){ ED.add(ED.make(o)); }); V().draw(); return l; }
  bi('ENTLAST', 0, 0, function(){ var d = need(), e = d.lastEnt && d.byId[d.lastEnt] || d.ents[d.ents.length - 1]; return e ? new En(e.id) : null; });
  bi('ENTGET', 1, 2, function(a){ return dxf(ent(a[0])); });
  bi('ENTMOD', 1, 1, function(a){ return entmod(a[0]); }); bi('ENTMAKE', 0, 1, function(a){ return a.length ? entmake(a[0]) : null; });
  bi('ENTDEL', 1, 1, function(a){ var e = ent(a[0]); ED.run('ENTDEL', function(){ ED.del(e); }); V().draw(); return a[0]; });
  // ssget: "X" cả bản vẽ, "I" vùng chọn sẵn, "L" đối tượng cuối, "P" vùng chọn trước; danh sách lọc theo mã 0, 2, 8, 62, 70
  function wc(s, p){ return S(p).split(',').some(function(q){ return new RegExp('^' + q.trim().replace(/[.+^${}()|[\]\\]/g, '\\$&').replace(/\*/g, '.*').replace(/\?/g, '.') + '$', 'i').test(s || ''); }); }
  function match(e, fl){ return fl.every(function(gp){ var k = gp[0], v = gp.dot ? gp[1] : gp[1]; return k === 0 ? wc(TY0[e.t] || e.t, v) : k === 8 ? wc(e.layer, v) : k === 2 ? wc(e.name || e.pat, v) : k === 62 ? (e.color != null ? e.color : 256) === N(v) : k === 70 ? (e.closed ? 1 : 0) === N(v) : true; }); }
  async function ssget(a){
    var d = need(), m = typeof a[0] === 'string' ? a[0].toUpperCase().replace(/^_/, '') : '', fl = a.filter(function(x){ return Array.isArray(x) && Array.isArray(x[0]); })[0], ids;
    if (m === 'X') ids = d.ents.concat.apply(d.ents, d.layouts.map(function(L0){ return L0.ents; })).map(function(e){ return e.id; });
    else if (m === 'I') ids = d.sel.slice(); else if (m === 'L') ids = d.lastEnt ? [d.lastEnt] : []; else if (m === 'P') ids = (d.prevSel || []).slice();
    else if (m && m !== ':L') throw E('ssget: chế độ chọn chưa hỗ trợ trong prototype', 'invalid selection mode', a[0]);
    else { ids = await ask(function(ctx){ return ctx.sel(fl ? 'Chọn đối tượng (lọc: ' + fl.map(function(gp){ return show(gp[1], true); }).join(', ') + ')' : 'Chọn đối tượng'); }); TC.cad.clearSel(); }
    if (fl) ids = ids.filter(function(id){ var e = d.byId[id]; return e && match(e, fl); });
    ids = ids.filter(function(id){ return d.byId[id]; });
    return ids.length ? new Ss(ids, ++SSN) : null;
  }
  bi('SSGET', 0, 3, ssget);
  bi('SSLENGTH', 1, 1, function(a){ if (!(a[0] instanceof Ss)) throw bad('lselsetp', a[0]); return a[0].ids.length; });
  bi('SSNAME', 2, 2, function(a){ if (!(a[0] instanceof Ss)) throw bad('lselsetp', a[0]); var id = a[0].ids[N(a[1], 'fixnump')]; return id ? new En(id) : null; });
  // nhập từ người dùng: trong lệnh LISP dùng chính dòng lệnh của lệnh đó; gọi từ bảng điều khiển thì mở một lượt hỏi trên dòng lệnh
  function msg(s, def){ return typeof s === 'string' && s.replace(/^\s+|[\s:]+$/g, '') || def; }
  function ask(fn){ need(); flushCL(); if (CUR.ctx) return fn(CUR.ctx); return new Promise(function(res, rej){ TC.cmd.run('LISPCONSOLE', { ask:function(ctx){ return fn(ctx).then(res, rej); } }); }); }
  bi('ENTSEL', 0, 1, async function(a){ var r = await ask(function(ctx){ return ED.pick(ctx, msg(a[0], 'Chọn đối tượng')); }); return r && r.e ? [new En(r.e.id), P3(r.p)] : null; });
  bi('INITGET', 0, 2, function(a){ var s = a.filter(function(x){ return typeof x === 'string'; })[0]; INIT = s ? s.split(/\s+/).filter(Boolean) : null; return null; });
  async function getp(a, kind){ var base = null, m = null, kw = INIT; INIT = null; a.forEach(function(x){ if (typeof x === 'string') m = x; else if (Array.isArray(x)) base = P2(x); });
    var r = await ask(function(ctx){ return ctx.point(msg(m, kind === 'dist' ? 'Khoảng cách' : 'Chỉ định điểm'), { base:base, num:kind === 'dist', opts:kw ? kw.map(function(k){ return [k, k]; }) : null }); });
    if (r.enter) return null; if (r.kw) return r.kw; if (r.num != null) return R(r.num);
    if (kind !== 'dist') return P3(r.p); if (base) return R(G.dist(base, r.p));
    var p0 = r.p, r2 = await ask(function(ctx){ return ctx.point('Điểm thứ hai', { base:p0 }); }); return r2.p ? R(G.dist(p0, r2.p)) : null; }
  async function getv(a, kind){ var m = a.filter(function(x){ return typeof x === 'string'; }).pop(), kw = INIT; INIT = null;
    for (;;){ var v = await ask(function(ctx){ return ctx.val(msg(m, kind === 'str' ? 'Nhập chữ' : kind === 'kw' ? '[' + (kw || []).join('/') + ']' : 'Nhập số'), null, kind === 'str' && a.length > 1 && a[0] != null ? { allowSpace:true } : null); });
      if (v == null || v === '') return null;
      if (kind === 'str') return v;
      if (kind === 'kw'){ var k = (kw || []).filter(function(x){ return x.toUpperCase().indexOf(v.toUpperCase()) === 0; })[0]; if (k) return k; }
      else if (kind === 'int' ? /^[+-]?\d+$/.test(v) : /^[+-]?(\d+\.?\d*|\.\d+)$/.test(v)) return kind === 'int' ? Number(v) | 0 : R(Number(v));
      TC.cmdline.msg(kind === 'int' ? 'Cần một số nguyên.' : kind === 'kw' ? 'Cần một trong: ' + esc((kw || []).join(', ')) + '.' : 'Cần một số. Dấu thập phân là dấu chấm.'); } }
  bi('GETPOINT GETCORNER', 0, 2, function(a){ return getp(a, 'pt'); }); bi('GETDIST', 0, 2, function(a){ return getp(a, 'dist'); });
  bi('GETINT', 0, 1, function(a){ return getv(a, 'int'); }); bi('GETREAL', 0, 1, function(a){ return getv(a, 'real'); }); bi('GETSTRING', 0, 2, function(a){ return getv(a, 'str'); }); bi('GETKWORD', 0, 1, function(a){ return getv(a, 'kw'); });
  bi('DISTANCE', 2, 2, function(a){ return R(G.dist(P2(a[0]), P2(a[1]))); });
  bi('ANGLE', 2, 2, function(a){ var p = P2(a[0]), q = P2(a[1]), r = Math.atan2(q[1] - p[1], q[0] - p[0]); return R(r < 0 ? r + 2 * Math.PI : r); });
  bi('POLAR', 3, 3, function(a){ var p = P2(a[0]), an = N(a[1]), dd = N(a[2]); return [R(p[0] + dd * Math.cos(an)), R(p[1] + dd * Math.sin(an)), R(0)]; });
  bi('VLAX-CURVE-GETAREA', 1, 1, function(a){ var e = ent(a[0]); if (e.t === 'circle') return R(Math.PI * e.r * e.r); if (e.t === 'pline') return R(G.polyArea(e.pts)); throw bad('curvep', a[0]); });

  /* ---------- (command …): chạy lệnh thật của TrionCAD, đưa từng đối số vào prompt ---------- */
  // trong lệnh LISP (c:…) lệnh con chạy lồng (như lệnh trong suốt) để lệnh LISP vẫn giữ dòng lệnh cho getpoint sau đó
  function running(){ return !!LIVE && (LIVE.nest ? !LIVE.done : TC.cmdline.busy() && TC.cmdline.current() === LIVE.name); }
  function isSel(p){ return !!(p && p.opts && p.opts.some(function(o){ return o[1] === 'ALL'; })); }
  function inp(v){ var f = function(x){ return String(Number(N(x).toFixed(6))); };
    if (v == null || v === '') return ''; if (typeof v === 'string') return /^_[A-Za-z]/.test(v) ? v.slice(1) : v; if (isN(v)) return f(v);
    if (Array.isArray(v) && !v.dot && v.length >= 2 && isN(v[0]) && isN(v[1])) return f(v[0]) + ',' + f(v[1]); if (v === T) return 'Y'; throw bad('lệnh không nhận giá trị này', v); }
  async function until(f){ while (!f()){ if (L.stop) throw E('hàm bị hủy', 'Function cancelled'); await sleep(30); } }
  async function waitP(last, lv){ for (var t = 0; ; t += 20){
    if (L.stop) throw E('hàm bị hủy', 'Function cancelled');
    if (!running()) return null;
    var p = TC.cmdline.prompt(); if (p && (p !== last || isSel(p))) return p;
    if (p && t > 600){ var tx = p.text; TC.cmd.cancel(); throw E('giá trị “' + lv + '” không hợp lệ cho “' + tx + '”', 'invalid input'); }
    await sleep(20); } }
  function layerCmd(q){ var d = need();
    while (q.length){ var o = q.shift(); if (o === '' || o == null) return; o = String(o).replace(/^_/, '').toUpperCase();
      if (!/^(S|SET|M|MAKE)$/.test(o)) throw E('-LAYER: tùy chọn chưa hỗ trợ trong prototype (dùng S, M)', 'invalid option', o);
      var n = S(q.shift()); if (!d.layerMap[n]){ if (o[0] === 'S') throw E('không có layer này', 'Cannot find layer', n); mkLayer(d, n); }
      ED.setCurLayer(n, d); log({ k:'cmd', s:'-LAYER: layer hiện hành ' + n }); } }
  async function command(a){
    var q = a.slice(), CL = TC.cmdline; need();
    if (!q.length){ if (running()) TC.cmd.cancel(); LIVE = null; return; }
    while (q.length){
      if (!running()){
        var raw = q.shift(); if (raw === '' || raw == null) continue; if (typeof raw !== 'string') throw bad('stringp', raw);
        var nm = raw.replace(/^[_.]+/, ''), dash = nm[0] === '-'; nm = nm.replace(/^-+/, '').toUpperCase();
        if (dash && nm === 'LAYER'){ layerCmd(q); continue; }
        var c = TC.cmd.def(nm); if (!c) throw E('lệnh không có', 'Unknown command', raw);
        if (!TC.can(c.need)) throw E('lệnh ' + c.name + ' có trong ' + (c.need === 'VP' ? 'TrionCAD Viewer Pro' : 'TrionCAD Pro'), 'command not available');
        // vùng chọn đưa vào ngay sau tên lệnh: chọn trước rồi gọi lệnh (PICKFIRST), bỏ một Enter kết thúc chọn
        if (q[0] instanceof Ss || q[0] instanceof En){ var s0 = q.shift(); D().sel = s0 instanceof Ss ? s0.ids.slice() : [s0.id]; TC.cad.afterSel(); if (q[0] === '') q.shift(); }
        if (Number(getvar('CMDECHO')) !== 0) log({ k:'cmd', s:'Lệnh: ' + c.name });
        var nest = !!(CUR.ctx && CL.busy()), job = LIVE = { name:c.name, nest:nest, done:false }, pr = TC.cmd.run((nest ? "'" : '') + c.name);
        if (nest) Promise.resolve(pr).then(function(){ job.done = true; });
        await sleep(20);
      }
      var last = null, lv = '';
      while (q.length){
        var p = await waitP(last, lv); if (!p) break;
        var v = q.shift();
        if (v === PAUSE){ await until(function(){ return !running() || CL.prompt() !== p; }); last = p; continue; }
        lv = inp(v); CL.submit(lv); last = p; await sleep(20);
      }
    }
  }
  bi('COMMAND VL-CMDF COMMAND-S', 0, null, async function(a){ await command(a); return null; });

  /* ---------- lệnh LISP (defun c:…) thành lệnh gõ được trên dòng lệnh ---------- */
  function regCmd(n, f){ var c = n.slice(2), ex = TC.CMDS[c];
    if (ex && !ex.lisp && ex.where !== 'LISP'){ log({ k:'info', s:'Hàm ' + n + ' trùng tên lệnh có sẵn ' + c + ': gõ ' + c + ' vẫn chạy lệnh của TrionCAD; gọi hàm bằng (' + n.toLowerCase() + ').' }); return; }
    TC.CMDS[c] = { name:c, aliases:[], need:'PRO', wave:5, desc:'Lệnh LISP' + (f.file ? ' · ' + f.file : ''), vi:'lisp', icon:'lisp', where:'LISP · ' + (f.file || 'Bảng điều khiển LISP'), lisp:true };
    IMPL[c] = function(ctx){ return runCmd(n, ctx); }; }
  function runCmd(n, ctx){
    if (L.busy){ ctx.out('LISP đang chạy một biểu thức khác. Dừng ở Bảng điều khiển LISP (Esc) rồi thử lại.'); return; }
    return boot().then(function(){ var f = GV[n]; if (!(f instanceof Fn)){ ctx.out('Hàm ' + esc(n) + ' không còn được định nghĩa.'); return; }
      CUR.ctx = ctx; log({ k:'in', s:'(' + n.toLowerCase() + ')' });
      return exec(function(){ return call(f, [], [sym(n)]); }, { cl:true, quiet:true }).then(function(){ CUR.ctx = null; }); });
  }

  /* ---------- tệp LISP mẫu (cùng tên, thư mục với APPLOAD UI-K01) ---------- */
  var LF = [
    { name:'layer-tcvn.lsp', dir:'D:\\Tai-nguyen\\LISP', size:'4 KB', date:'02/06/2026', suite:true, src:[';;; layer-tcvn.lsp · bộ layer TCVN cho bản vẽ kiến trúc · nạp qua Startup Suite', '(setq *lop-tcvn* \'(("KT-TUONG" 7) ("KT-CUA" 4) ("KT-DIM" 3) ("KT-TEXT" 7) ("KT-GHICHU" 7)))', '(defun lop-dat (ten)', '  (if (assoc ten *lop-tcvn*)', '    (command "_.-LAYER" "_M" ten "")', '    (princ (strcat "\\nKhông có layer " ten " trong bộ TCVN.")))', '  (princ))', '(princ "\\nlayer-tcvn.lsp: (lop-dat \\"KT-TUONG\\") đặt layer hiện hành theo bộ TCVN.")', '(princ)'] },
    { name:'danhso.lsp', dir:'D:\\Tai-nguyen\\LISP', size:'6 KB', date:'01/08/2026', src:[';;; danhso.lsp · đánh số tăng dần (phòng, cấu kiện) tại các điểm chọn', '(defun c:DANHSO (/ so cao p dem)', '  (setq so (getint "\\nSố bắt đầu <1>: "))', '  (if (null so) (setq so 1))', '  (setq cao (getreal "\\nChiều cao chữ <250>: "))', '  (if (null cao) (setq cao 250.0))', '  (setq dem 0)', '  (while (setq p (getpoint (strcat "\\nVị trí số " (itoa so) " (Enter để kết thúc): ")))', '    (entmake (list \'(0 . "TEXT") (cons 10 p) (cons 40 cao) (cons 1 (itoa so)) \'(8 . "KT-TEXT")))', '    (setq so (1+ so) dem (1+ dem)))', '  (princ (strcat "\\nĐã đánh " (itoa dem) " số."))', '  (princ))'] },
    { name:'tinh-dien-tich.lsp', dir:'D:\\Tai-nguyen\\LISP', size:'9 KB', date:'15/07/2026', src:[';;; tinh-dien-tich.lsp · tổng và trung bình diện tích các vùng kín đã chọn (m²)', '(vl-load-com)', '(defun trung-binh (tong n) (/ tong n))', '(defun dt-phong (ds / tong)', '  (setq tong 0.0)', '  (foreach a ds (setq tong (+ tong a)))', '  (list tong (trung-binh tong (length ds))))', '(defun c:TDT (/ ss i ds kq)', '  (setq ss (ssget \'((0 . "LWPOLYLINE,CIRCLE"))) i 0)', '  (repeat (sslength ss)', '    (setq ds (cons (/ (vlax-curve-getarea (ssname ss i)) 1e6) ds) i (1+ i)))', '  (setq kq (dt-phong ds))', '  (princ (strcat "\\nTổng: " (rtos (car kq) 2 2) " m², trung bình " (rtos (cadr kq) 2 2) " m²."))', '  (princ))'] },
    { name:'thongkethep.lsp', dir:'D:\\Tai-nguyen\\LISP', size:'24 KB', date:'12/09/2026', src:[';;; thongkethep.lsp · thống kê thép qua hộp thoại thongkethep.dcl', '(defun kg-thanh (d l) (* 0.00617 d d (/ l 1000.0)))   ; khối lượng một thanh: d, l tính bằng mm', '(defun c:TKT (/ id)', '  (setq id (load_dialog "thongkethep.dcl"))', '  (if (not (new_dialog "thongkethep" id)) (exit))', '  (start_dialog)', '  (unload_dialog id)', '  (princ))'] },
    { name:'ve-cot.lsp', dir:'C:\\Users\\An\\Downloads', size:'11 KB', date:'18/10/2026', src:[';;; ve-cot.lsp · vẽ cột vuông tại các điểm chọn (tải từ diễn đàn)', '(defun c:VECOT (/ a p)', '  (setq a (getdist "\\nCạnh cột <220>: "))', '  (if (null a) (setq a 220.0))', '  (while (setq p (getpoint "\\nTâm cột (Enter để kết thúc): "))', '    (command "_.RECTANG" (polar p (* 1.25 pi) (* a (sqrt 0.5))) (polar p (* 0.25 pi) (* a (sqrt 0.5)))))', '  (princ))'] }
  ];
  LF.forEach(function(f){ f.src = f.src.join('\n'); });
  function findF(n){ n = String(n).replace(/^.*[\\\/]/, '').toLowerCase(); if (!/\.lsp$/.test(n)) n += '.lsp'; return LF.filter(function(f){ return f.name === n; })[0] || null; }
  // nạp tệp: kiểm tra thư mục tin cậy theo Tùy chọn bảo mật (UI-K03), cảnh báo như APPLOAD (UI-K02)
  async function loadF(f, how){
    var s = TC.load('secopts', { level:1, trusted:['C:\\TrionCAD\\Support', 'D:\\Tai-nguyen\\LISP'] }), ok = s.level === 0 || s.trusted.some(function(t){ return f.dir.toLowerCase().indexOf(t.toLowerCase()) === 0; });
    if (!ok && s.level === 2) throw E('chỉ nạp từ thư mục tin cậy (SECURELOAD = 2)', 'LOAD failed', f.name);
    if (!ok && (await SC.secWarn([f.name, f.dir, f.size, f.date])) === 'no') throw E('không nạp vì cảnh báo bảo mật', 'LOAD failed', f.name);
    var forms = read(f.src, f.name), keep = [L.src, L.defs], defs = [], t0 = performance.now(), r = null; L.src = f.name; L.defs = defs;
    try { for (var i = 0; i < forms.length; i++) r = await ev(forms[i]); } finally { L.src = keep[0]; L.defs = keep[1]; }
    var cmds = defs.filter(function(n){ return /^C:/.test(n); }).map(function(n){ return n.slice(2); });
    L.loaded[f.name] = { f:f, t:new Date(), defs:defs, how:how || 'Bảng điều khiển LISP' };
    log({ k:'info', s:'Đã nạp ' + f.name + (how ? ' (' + how + ')' : '') + ' · ' + defs.length + ' hàm' + (cmds.length ? ', lệnh ' + cmds.join(', ') : '') + ' · ' + fmtMs(performance.now() - t0) });
    return r;
  }

  /* ---------- chạy, đo thời gian, bắt lỗi ---------- */
  function norm(e){ return e instanceof LErr ? e : e === 'cancel' ? E('hàm bị hủy', 'Function cancelled') : E('lỗi nội bộ của prototype: ' + (e && e.message || e), 'internal error'); }
  function emit(s){ if (!OUT) log({ k:'out', s:'' }); OUT.s += s; if (CUR.ctx) CLB += s; }
  function flushCL(){ if (CLB){ CLB.split('\n').forEach(function(l){ if (l.trim()) TC.cmdline.msg(esc(l)); }); CLB = ''; } }
  async function exec(fn, o){
    o = o || {}; L.busy = true; L.stop = false; L.t0 = performance.now(); paintState();
    var r, err = null; try { r = await fn(); } catch (e){ err = norm(e); }
    var ms = performance.now() - L.t0; flushCL(); OUT = null;
    if (err){ if (running()) TC.cmd.cancel(); LIVE = null; INIT = null; await onErr(err, o); }
    else if (!o.quiet && r !== NOP) log({ k:'res', v:r, ms:ms });
    L.busy = false; L.stop = false; paintState(); paintSide();
    return err;
  }
  async function onErr(e, o){ var h = look('*ERROR*');
    log({ k:'err', e:e, h:h instanceof Fn, fi:0 });
    if (o.cl) TC.cmdline.msg('<span class="err">; lỗi: ' + esc(e.vi) + ('x' in e ? ': ' + esc(show(e.x)) : '') + '</span>');
    if (h instanceof Fn){ try { await call(h, [e.vi]); } catch (e2){ log({ k:'err', e:norm(e2), fi:0 }); } flushCL(); OUT = null; } }
  // Startup Suite và tệp LISP đi kèm tệp tùy biến (CUI) nạp một lần, lúc dùng LISP lần đầu
  var booted = null;
  async function loadMissing(){ var list = LF.filter(function(f){ return f.suite; }).concat((CU.lisp || []).map(findF).filter(Boolean));
    for (var i = 0; i < list.length; i++){ var f = list[i]; if (!L.loaded[f.name] && !L.busy) await exec(function(){ return loadF(f, f.suite ? 'Startup Suite' : 'đi kèm trion.tcui'); }, { quiet:true }); } }
  function boot(){ return booted || (booted = loadMissing()); }
  async function repl(src){
    src = src.trim(); if (!src || L.busy) return;
    CON.hist = [src].concat(CON.hist.filter(function(h){ return h !== src; })).slice(0, 60); TC.save('lisphist', CON.hist); CON.hi = -1;
    await boot(); log({ k:'in', s:src });
    var forms; try { forms = read(src, null); } catch (e){ log({ k:'err', e:norm(e), fi:0 }); return; }
    for (var i = 0; i < forms.length; i++){ var f = forms[i]; if (await exec(function(){ return ev(f); })) break; }
  }
  TC.lisp.eval = repl; TC.lisp.run = function(src){ return boot().then(function(){ return read(src).reduce(function(p, f){ return p.then(function(){ return ev(f); }); }, Promise.resolve()); }).then(function(v){ return show(v); }); }; TC.lisp.load = function(name){ var f = findF(name); return f && !L.busy ? exec(function(){ return loadF(f); }, { quiet:true }) : Promise.resolve(); };
  TC.lisp.has = function(n){ return look(String(n).toUpperCase()) instanceof Fn; };
  // Esc khi LISP đang chạy (không có hộp thoại mở): dừng ở nhịp kế tiếp
  document.addEventListener('keydown', function(e){ if (e.key === 'Escape' && L.busy && !TC.topDialog()) L.stop = true; }, true);

  /* ================= Bảng điều khiển LISP UI-K09 (LISPCONSOLE): palette REPL ================= */
  var CON = { log:[], hist:TC.load('lisphist', []), hi:-1, tab:'con', vq:'', brk:true, tm:true, draft:'' }, ROOT = null, raf = 0, tick = 0;
  function log(o){ CON.log.push(o); if (CON.log.length > 400) CON.log.splice(0, CON.log.length - 400); OUT = o.k === 'out' ? o : null; paintSoon(); return o; }
  function paintSoon(){ if (!raf) raf = requestAnimationFrame(function(){ raf = 0; paintLog(); }); }
  function depth(s){ var d = 0, q = false; for (var i = 0; i < s.length; i++){ var c = s[i]; if (q){ if (c === '\\') i++; else if (c === '"') q = false; } else if (c === '"') q = true; else if (c === ';'){ while (i < s.length && s[i] !== '\n') i++; } else if (c === '(') d++; else if (c === ')') d--; } return q ? 1 : d; }
  // nhãn kiểu của kết quả; T và nil ghi riêng cho dễ đọc (type của T vẫn là SYM)
  function tyTag(v, bare){ var t = v === T ? 'T' : typ(v) || 'nil'; return '<span class="lty" data-ty="' + t + '"' + (v === T ? ' title="Kiểu SYM: ký hiệu T (đúng)"' : '') + '>' + t + (v instanceof Ss && !bare ? ' · ' + TC.n(v.ids.length) + ' đối tượng' : '') + '</span>'; }
  function callStr(f){ var s = '(' + f.f.name.toLowerCase() + (f.a.length ? ' ' + f.a.map(function(x){ var t = show(x); return t.length > 28 ? t.slice(0, 26) + '…' : t; }).join(' ') : '') + ')'; return s; }
  function errHTML(o, i){ var e = o.e, tr = e.tr || [], fr = tr[o.fi || 0], src = tr.filter(function(f){ return f.form && f.form.file; })[0], at = src ? src.form.file + ':' + src.form.ln : e.file ? e.file + ':' + e.ln : '';
    var h = '<div class="lse err"><div class="lerr">' + ic('error', 's12') + '<span><b>lỗi: ' + esc(e.vi) + ('x' in e ? ': <span class="mono">' + esc(show(e.x)) + '</span>' : '') + '</b> <span class="faint">· ' + esc(e.en) + '</span>' + (o.h ? ' <span class="faint">· đã gọi *error*</span>' : '') + '</span></div>';
    if (CON.brk && tr.length){ h += '<div class="ltr"><div class="ltr-h">Dừng khi lỗi · vết gọi hàm (trong cùng trước)</div>' + tr.map(function(f, k){ return '<button type="button" class="ltf' + (k === (o.fi || 0) ? ' on' : '') + '" data-lfr="' + i + ':' + k + '"><span class="mono">' + esc(callStr(f)) + '</span><span class="faint">' + (f.form && f.form.file ? esc(f.form.file) + ':' + f.form.ln : 'nhập tay') + '</span></button>'; }).join('');
      if (fr) h += '<div class="lvars">' + (fr.vars ? (fr.vars.length ? fr.vars.map(function(v){ return '<span class="mono">' + esc(v[0].toLowerCase()) + ' = ' + esc(show(v[1])) + '</span>'; }).join('') : '<span class="faint">Hàm không có biến cục bộ.</span>') : '<span class="faint">Hàm có sẵn · đối số: </span><span class="mono">' + esc(fr.a.map(function(x){ return show(x); }).join(' ') || '(không có)') + '</span>') + '</div>';
      h += '</div>'; }
    return h + (at ? '<div class="hrow lerr-a"><button class="btn sm ghost" type="button" data-lcode="' + esc(at) + '">' + ic('ext') + 'Mở ' + esc(at) + ' trong VS Code</button></div>' : '') + '</div>'; }
  function eHTML(o, i){
    if (o.k === 'in') return '<div class="lse in"><span class="lspr">_$</span><span>' + esc(o.s) + '</span></div>';
    if (o.k === 'res') return '<div class="lse res"><span class="lv">' + esc(show(o.v)) + '</span>' + tyTag(o.v) + (CON.tm ? '<span class="lms">' + fmtMs(o.ms) + '</span>' : '') + '</div>';
    if (o.k === 'out'){ var s = o.s.replace(/^\n+|\n+$/g, ''); return s ? '<div class="lse out">' + esc(s) + '</div>' : ''; }
    if (o.k === 'cmd') return '<div class="lse cmd">' + esc(o.s) + '</div>';
    if (o.k === 'info') return '<div class="lse info">' + ic('info', 's12') + '<span>' + esc(o.s) + '</span></div>';
    return errHTML(o, i);
  }
  function paintLog(){ if (!ROOT || !ROOT.isConnected) return; var out = $('.lsout', ROOT);
    out.innerHTML = CON.log.length ? CON.log.map(eHTML).join('') : '<div class="empty lsempty">' + ic('lisp') + '<b>Bảng điều khiển LISP</b>Gõ biểu thức AutoLISP rồi Enter, ví dụ <span class="mono">(+ 1 2)</span> hoặc <span class="mono">(getvar "OSMODE")</span>. Kết quả ghi kiểu dữ liệu và thời gian chạy.</div>';
    out.scrollTop = out.scrollHeight; }
  function paintState(){ if (!ROOT || !ROOT.isConnected) return; var b = L.busy, p = b && TC.cmdline.prompt();
    $('[data-ls="stop"]', ROOT).disabled = !b; $('[data-ls="run"]', ROOT).disabled = b; $('textarea', ROOT).classList.toggle('is-busy', b);
    var nf = Object.keys(GV).filter(function(n){ return GV[n] instanceof Fn; }).length;
    $('.lsst', ROOT).innerHTML = b ? '<span class="lsrun">' + ic('clock', 's12') + 'Đang chạy ' + fmtMs(performance.now() - L.t0) + (p ? ' · chờ nhập trên dòng lệnh: ' + esc(p.text) : '') + '</span><span class="grow"></span><span>Esc để dừng</span>'
      : '<span>Sẵn sàng · ' + nf + ' hàm, ' + Object.keys(L.loaded).length + ' tệp đã nạp</span><span class="grow"></span><span>Enter chạy · ↑↓ lịch sử</span>';
    clearInterval(tick); if (b) tick = setInterval(paintState, 250); }
  function varRows(){ var q = TC.fold(CON.vq.trim()); return Object.keys(GV).filter(function(n){ return n !== 'PI' && n !== 'PAUSE' && GV[n] != null && (!q || TC.fold(n).indexOf(q) >= 0); }).sort(); }
  function paintSide(){ if (!ROOT || !ROOT.isConnected) return;
    var all = Object.keys(GV).filter(function(n){ return n !== 'PI' && n !== 'PAUSE' && GV[n] != null; }), nv = all.filter(function(n){ return !(GV[n] instanceof Fn); }).length;
    $('[data-lt="var"] .cnt', ROOT).textContent = nv; $('[data-lt="file"] .cnt', ROOT).textContent = Object.keys(L.loaded).length;
    $$('[data-lt]', ROOT).forEach(function(t){ t.setAttribute('aria-selected', String(t.dataset.lt === CON.tab)); }); $$('[data-lp]', ROOT).forEach(function(p){ p.hidden = p.dataset.lp !== CON.tab; });
    if (CON.tab === 'var'){ var ks = varRows(), fn = ks.filter(function(n){ return GV[n] instanceof Fn; }), vs = ks.filter(function(n){ return !(GV[n] instanceof Fn); });
      var row = function(n){ var v = GV[n]; return '<tr data-lvn="' + esc(n) + '" title="Nhấp đúp để in giá trị ra Console"><td class="mono">' + esc(n.toLowerCase()) + '</td><td>' + tyTag(v, 1) + '</td><td class="mono">' + esc(v instanceof Fn ? (v.file ? v.file + ':' + v.ln : 'Bảng điều khiển') + ' · ' + v.args.length + ' tham số' : show(v)) + '</td></tr>'; };
      $('[data-lvl]', ROOT).innerHTML = ks.length ? '<table class="tbl lsvt"><colgroup><col style="width:32%"><col style="width:76px"><col></colgroup><thead><tr><th>Tên</th><th>Kiểu</th><th>Giá trị</th></tr></thead><tbody>' + (vs.length ? '<tr class="lsg"><td colspan="3">Biến toàn cục · ' + vs.length + '</td></tr>' + vs.map(row).join('') : '') + (fn.length ? '<tr class="lsg"><td colspan="3">Hàm do người dùng định nghĩa · ' + fn.length + '</td></tr>' + fn.map(row).join('') : '') + '</tbody></table>'
        : '<div class="empty">' + ic('search') + (CON.vq ? '<b>Không có biến khớp “' + esc(CON.vq) + '”</b>Thử tên khác, ví dụ “so-tang”.' : '<b>Chưa có biến toàn cục</b>Gõ (setq ten gia-tri) hoặc nạp một tệp LISP.') + '</div>'; }
    if (CON.tab === 'file'){ var ks2 = Object.keys(L.loaded);
      $('[data-lfl]', ROOT).innerHTML = ks2.length ? ks2.map(function(n){ var x = L.loaded[n], cm = x.defs.filter(function(c){ return /^C:/.test(c); }); return '<div class="lsf"><div class="lsf-h">' + ic('file', 's12') + '<b>' + esc(n) + '</b>' + (x.how !== 'Bảng điều khiển LISP' ? '<span class="tag">' + esc(x.how) + '</span>' : '') + '<span class="grow"></span><span class="faint mono">' + ('0' + x.t.getHours()).slice(-2) + ':' + ('0' + x.t.getMinutes()).slice(-2) + '</span></div><div class="mono faint lsf-p">' + esc(x.f.dir + '\\' + n) + '</div><div class="lsf-d">' + x.defs.length + ' hàm' + (cm.length ? ' · lệnh <b class="mono">' + esc(cm.map(function(c){ return c.slice(2); }).join(', ')) + '</b>' : '') + '</div><div class="hrow"><button class="btn sm" type="button" data-lre="' + esc(n) + '">' + ic('refresh') + 'Nạp lại</button><button class="btn sm ghost" type="button" data-lcode="' + esc(n) + '">' + ic('ext') + 'Mở trong VS Code</button></div></div>'; }).join('')
        : '<div class="empty">' + ic('file') + '<b>Chưa nạp tệp LISP nào</b>Nạp tệp để thêm hàm và lệnh. Tệp ngoài thư mục tin cậy sẽ hỏi trước khi nạp.<button class="btn sm" type="button" data-ls="load" style="margin-top:6px">' + ic('open') + 'Nạp tệp…</button></div>'; }
  }
  function paintAll(){ paintLog(); paintState(); paintSide(); }
  function vscode(at){
    if (!at){ var e = CON.log.slice().reverse().filter(function(o){ return o.k === 'err'; })[0], fr = e && (e.e.tr || []).filter(function(f){ return f.form && f.form.file; })[0]; at = fr ? fr.form.file + ':' + fr.form.ln : Object.keys(L.loaded).slice(-1)[0]; }
    var m = /^(.*):(\d+)$/.exec(at || ''), file = m ? m[1] : at;
    TC.toast(at ? 'Đang mở <b class="mono">' + esc(file) + '</b>' + (m ? ' tại dòng ' + m[2] : '') + ' trong VS Code. Tiện ích <b>TrionCAD LISP</b> nối trình gỡ lỗi: đặt điểm dừng, chạy từng bước, xem biến.' : 'Mở VS Code với phiên LISP hiện tại. Chưa có tệp nào được nạp.', { type:'info', log:false, ms:7000 });
    log({ k:'info', s:'Đã nối trình gỡ lỗi VS Code (tiện ích TrionCAD LISP 1.0) qua cổng 5717' + (file ? ' · ' + file : '') + '.' }); }
  function pickFile(){
    var sel = (LF.filter(function(f){ return !L.loaded[f.name]; })[0] || LF[0]).name, sec = TC.load('secopts', { level:1, trusted:['C:\\TrionCAD\\Support', 'D:\\Tai-nguyen\\LISP'] });
    var tr = function(f){ return sec.level === 0 || sec.trusted.some(function(t){ return f.dir.toLowerCase().indexOf(t.toLowerCase()) === 0; }); };
    var dlg = TC.dialog({ id:'lspfile', title:'Nạp tệp LISP', code:'UI-K09', w:620,
      body:'<div class="stack"><div class="tblwrap" style="height:190px"><table class="tbl" id="lf-t"><colgroup><col><col style="width:200px"><col style="width:88px"><col style="width:64px"></colgroup><thead><tr><th>Tệp</th><th>Thư mục</th><th>Ngày sửa</th><th class="num">Cỡ</th></tr></thead><tbody>' + LF.map(function(f){ return '<tr data-lf="' + esc(f.name) + '" tabindex="-1"><td>' + ic('file', 's12') + ' ' + esc(f.name) + (L.loaded[f.name] ? ' <span class="tag ok">đã nạp</span>' : '') + (tr(f) ? '' : ' <span class="tag warn">ngoài thư mục tin cậy</span>') + '</td><td class="mono faint" style="font-size:var(--fs-xs)">' + esc(f.dir) + '</td><td>' + f.date + '</td><td class="num">' + f.size + '</td></tr>'; }).join('') + '</tbody></table></div><p class="faint" style="margin:0;font-size:var(--fs-s)">Tệp ngoài thư mục tin cậy hỏi trước khi nạp (Tùy chọn bảo mật). Tệp đã nạp thì nạp lại, ghi đè hàm cùng tên.</p></div>',
      foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="lf-ok">Nạp tệp</button>' });
    function paint(){ $$('[data-lf]', dlg.el).forEach(function(r){ r.classList.toggle('is-sel', r.dataset.lf === sel); }); $('#lf-ok', dlg.el).textContent = L.loaded[sel] ? 'Nạp lại ' + sel : 'Nạp ' + sel; }
    function go(){ var f = findF(sel); dlg.close(true); CON.tab = 'con'; paintSide(); boot().then(function(){ log({ k:'in', s:'(load "' + f.name + '")' }); return exec(function(){ return loadF(f); }); }); }
    dlg.el.addEventListener('click', function(e){ var r = e.target.closest('[data-lf]'); if (r){ sel = r.dataset.lf; paint(); } });
    dlg.el.addEventListener('dblclick', function(e){ if (e.target.closest('[data-lf]')) go(); });
    TC.listKeys($('#lf-t', dlg.el), '[data-lf]', function(r, enter){ sel = r.dataset.lf; paint(); if (enter) go(); });
    $('#lf-ok', dlg.el).addEventListener('click', go); paint();
  }
  function mkRoot(){
    var r = TC.el('<div class="lspc">' +
      '<div class="pal-tools"><button class="btn sm primary" type="button" data-ls="run" data-tip="Chạy biểu thức" data-key="Enter">' + ic('play') + 'Chạy</button><button class="ib" type="button" data-ls="stop" aria-label="Dừng" data-tip="Dừng" data-key="Esc" disabled>' + ic('stop') + '</button><span class="grow"></span>' +
        '<button class="btn sm ghost" type="button" data-ls="load" data-tip="Nạp tệp LISP" data-desc="Nạp tệp .lsp vào phiên này, như (load &quot;ten-tep&quot;). Tệp ngoài thư mục tin cậy hỏi trước khi nạp.">' + ic('open') + 'Nạp tệp…</button><button class="btn sm ghost" type="button" data-ls="code" data-tip="Mở trong VS Code" data-desc="Mở tệp LISP (tại dòng lỗi gần nhất nếu có) trong VS Code; tiện ích TrionCAD LISP nối trình gỡ lỗi với phiên này.">' + ic('ext') + 'Mở trong VS Code</button>' +
        '<button class="ib" type="button" data-ls="clear" aria-label="Xóa màn hình" data-tip="Xóa màn hình" data-key="Ctrl+L">' + ic('trash') + '</button><button class="ib" type="button" data-ls="more" aria-label="Tùy chọn bảng điều khiển" data-tip="Tùy chọn">' + ic('more') + '</button></div>' +
      '<div class="tabs ptabs lstabs" role="tablist" aria-label="Bảng điều khiển LISP"><button class="tab" role="tab" type="button" data-lt="con">Console</button><button class="tab" role="tab" type="button" data-lt="var">Biến <span class="cnt mono"></span></button><button class="tab" role="tab" type="button" data-lt="file">Tệp đã nạp <span class="cnt mono"></span></button></div>' +
      '<div class="lsp-p" data-lp="con"><div class="lsout" role="log" aria-live="polite" aria-label="Kết quả LISP" tabindex="0"></div><div class="lsin"><span class="lspr" aria-hidden="true">_$</span><textarea class="inp mono" rows="3" spellcheck="false" aria-label="Biểu thức LISP" placeholder="(+ 1 2)   ·   Shift+Enter để xuống dòng"></textarea></div></div>' +
      '<div class="lsp-p" data-lp="var" hidden><div class="pal-tools"><label class="search grow">' + ic('search') + '<input data-lv placeholder="Tìm biến, hàm" aria-label="Tìm biến"></label></div><div class="pal-body" data-lvl></div><div class="pal-foot">Biến cục bộ (sau dấu / trong defun) chỉ có khi hàm đang chạy; xem trong vết gọi hàm khi lỗi.</div></div>' +
      '<div class="lsp-p" data-lp="file" hidden><div class="pal-body" data-lfl></div><div class="pal-foot"><span class="grow">Startup Suite và tệp đi kèm tệp tùy biến tự nạp khi dùng LISP lần đầu.</span><button class="btn sm" type="button" data-ls="load">' + ic('open') + 'Nạp tệp…</button></div></div>' +
      '<div class="pal-foot lsst" aria-live="polite"></div></div>');
    var ta = $('textarea', r);
    function submit(){ if (L.busy){ TC.toast('LISP đang chạy. Bấm Dừng hoặc Esc trước khi chạy biểu thức mới.', { type:'info', log:false }); return; } var s = ta.value; if (!s.trim()) return; ta.value = ''; CON.draft = ''; repl(s); }
    function stop(){ L.stop = true; var c = TC.cmdline.current(); if (c === 'LISPCONSOLE' || running() || (CUR.ctx && c)) TC.cmd.cancel(); }
    ta.addEventListener('keydown', function(e){
      if (e.key === 'Enter' && !e.shiftKey && (e.ctrlKey || depth(ta.value) <= 0)){ e.preventDefault(); submit(); return; }
      var up = e.key === 'ArrowUp' && !/\n/.test(ta.value.slice(0, ta.selectionStart)), dn = e.key === 'ArrowDown' && !/\n/.test(ta.value.slice(ta.selectionEnd));
      if ((up || dn) && CON.hist.length){ e.preventDefault(); if (CON.hi < 0) CON.draft = ta.value; CON.hi = TC.clamp(CON.hi + (up ? 1 : -1), -1, CON.hist.length - 1); ta.value = CON.hi < 0 ? CON.draft : CON.hist[CON.hi]; return; }
      if (e.key === 'Escape'){ e.preventDefault(); e.stopPropagation(); if (L.busy) stop(); else ta.value = ''; return; }
      if ((e.ctrlKey || e.metaKey) && (e.key === 'l' || e.key === 'L')){ e.preventDefault(); CON.log = []; paintLog(); }
    });
    $('[data-lv]', r).addEventListener('input', function(){ CON.vq = this.value; paintSide(); });
    r.addEventListener('dblclick', function(e){ var v = e.target.closest('[data-lvn]'); if (v && !L.busy){ CON.tab = 'con'; paintSide(); repl(v.dataset.lvn.toLowerCase()); } });
    r.addEventListener('click', function(e){
      var t = e.target.closest('[data-lt]'); if (t){ CON.tab = t.dataset.lt; paintSide(); if (CON.tab === 'con') ta.focus(); return; }
      var fr = e.target.closest('[data-lfr]'); if (fr){ var k = fr.dataset.lfr.split(':'); CON.log[Number(k[0])].fi = Number(k[1]); paintLog(); return; }
      var cd = e.target.closest('[data-lcode]'); if (cd){ vscode(cd.dataset.lcode); return; }
      var re = e.target.closest('[data-lre]'); if (re){ if (L.busy) return; var f = findF(re.dataset.lre); log({ k:'in', s:'(load "' + f.name + '")' }); exec(function(){ return loadF(f); }); return; }
      var b = e.target.closest('[data-ls]'); if (!b) return; var k2 = b.dataset.ls;
      if (k2 === 'run') submit(); else if (k2 === 'stop') stop(); else if (k2 === 'load') pickFile(); else if (k2 === 'code') vscode();
      else if (k2 === 'clear'){ CON.log = []; paintLog(); ta.focus(); }
      else if (k2 === 'more'){ var rc = b.getBoundingClientRect(); TC.menu([{ label:'Dừng khi lỗi, hiện vết gọi hàm', check:CON.brk, act:function(){ CON.brk = !CON.brk; paintLog(); } }, { label:'Hiện thời gian chạy', check:CON.tm, act:function(){ CON.tm = !CON.tm; paintLog(); } }, { sep:1 },
        { label:'Sao chép nội dung Console', icon:'copyclip', act:function(){ TC.copyText(CON.log.map(function(o){ return o.k === 'in' ? '_$ ' + o.s : o.k === 'res' ? show(o.v) : o.k === 'err' ? '; lỗi: ' + o.e.vi : o.s; }).join('\n')); } },
        { label:'Xóa lịch sử nhập (' + CON.hist.length + ')', dis:!CON.hist.length, act:function(){ CON.hist = []; TC.save('lisphist', []); } }, { sep:1 },
        { label:'Nạp lại tất cả tệp', icon:'refresh', dis:L.busy || !Object.keys(L.loaded).length, act:function(){ var ns = Object.keys(L.loaded); (function nx(i){ if (i < ns.length) exec(function(){ return loadF(L.loaded[ns[i]].f); }, { quiet:true }).then(function(){ nx(i + 1); }); })(0); } }], rc.right, rc.bottom + 2, { alignRight:true, returnFocus:b, minWidth:240 }); }
    });
    return r;
  }
  TC.PALETTES.lisp = { name:'LISP', hidden:function(){ return TC.isViewer(); }, render:function(host){
    if (TC.isViewer()){ host.innerHTML = '<div class="empty">' + ic('lisp') + '<b>Bảng điều khiển LISP</b>Chạy thử biểu thức, xem biến, nạp lại tệp LISP có trong TrionCAD Pro.<button class="btn primary sm" type="button" data-up style="margin-top:8px">Dùng thử Pro 30 ngày</button></div>'; host.onclick = function(e){ if (e.target.closest('[data-up]')) SC.upgrade('Bảng điều khiển LISP', 'PRO'); }; return; }
    host.onclick = null; if (!ROOT) ROOT = mkRoot();
    if (ROOT.parentNode !== host){ host.innerHTML = ''; host.appendChild(ROOT); }
    paintAll(); boot();
  } };
  IMPL.LISPCONSOLE = function(ctx, pre){ if (pre && pre.ask) return pre.ask(ctx); showPal('lisp'); setTimeout(function(){ var t = ROOT && $('textarea', ROOT); if (t && ROOT.isConnected){ CON.tab = 'con'; paintSide(); t.focus(); } }, 30); };
  var DEMO = ['(setq so-tang 2 cao-tang 3300)', '(* so-tang cao-tang 1.0)', '(strcat "Tầng " (itoa so-tang) ", cao " (rtos (/ cao-tang 1000.0) 2 1) " m")', '(setq ss (ssget "X" \'((0 . "INSERT") (8 . "KT-CUA"))))', '(setq e (ssname ss 0))', '(entget e)', '(type ss)', '(> (sslength ss) 10)', '(member "KT-CUA" \'("KT-TUONG" "KT-DIM"))', '(command "_.ZOOM" "0.9X")', '(dt-phong \'(15.07 35.39 17.31))', '(dt-phong nil)'];
  var demoing = false;
  OPEN['UI-K09'] = function(){ SC.cat.plan(); CON.tab = 'con'; if (!demoing && !L.busy) CON.log = []; showPal('lisp'); if (demoing) return; demoing = true;
    var ld = function(n){ return function(){ return L.loaded[n] || L.busy ? 0 : exec(function(){ return loadF(findF(n)); }, { quiet:true }); }; };
    boot().then(ld('tinh-dien-tich.lsp')).then(ld('danhso.lsp')).then(function(){ var i = 0; return (function nx(){ if (i >= DEMO.length || L.busy) return; var s = DEMO[i++]; log({ k:'in', s:s }); var f = read(s)[0]; return exec(function(){ return ev(f); }).then(nx); })(); })
      .then(function(){ demoing = false; }, function(){ demoing = false; });
  };

  /* ================= Tùy biến giao diện UI-K07 (CUI) =================
     trion.tcui = giao diện gốc + bản vá CU (lưu 'cui1'). Áp dụng: dựng lại ribbon, thanh menu, toolbar từ bản gốc (chụp lúc khởi động)
     và bản vá. Bản vá lỗi khi tải lại thì về giao diện mặc định, ribbon không hỏng. */
  function CU0(){ return { tabs:[], panels:[], add:{}, mod:{}, qat:[], tb:[], menus:[], madd:{}, ctx:[], dbl:{}, mouse:{}, cmds:[], lisp:[], part:[] }; }
  function cuNorm(c){ var b = CU0(); if (c && typeof c === 'object') Object.keys(b).forEach(function(k){ if (c[k] && typeof c[k] === 'object' && Array.isArray(c[k]) === Array.isArray(b[k])) b[k] = c[k]; }); return b; }
  var CU = cuNorm(TC.load('cui1', null)), ORIG = null, REG = {}, DRAFT = null;
  function grab(){ return ORIG || (ORIG = { tabs:TC.RIBBON_PRO.tabs.slice(), menus:TC.CLASSIC_MENUS.slice(), tb:TC.CLASSIC_TB.slice() }); }
  function cinfo(n){ var x = DRAFT && DRAFT.cmds.filter(function(c){ return c.name === n; })[0]; return x ? { name:x.name, desc:x.label, icon:x.icon || 'play', aliases:[], where:'Lệnh tùy biến', cu:x } : TC.CMDS[n] || null; }
  // nhãn mặc định của nút: phần đầu mô tả lệnh; nút lớn chia hai dòng như nút có sẵn ("Đoạn\nthẳng")
  function short(s){ return String(s || '').split(/[,(:]/)[0].trim(); }
  function twoLines(s){ var w = s.split(' '), best = s, m = 1e9; if (w.length < 2 || /\n/.test(s)) return s; for (var i = 1; i < w.length; i++){ var a = w.slice(0, i).join(' '), b = w.slice(i).join(' '), d = Math.max(a.length, b.length); if (d < m){ m = d; best = a + '\n' + b; } } return best; }
  function ctlOf(it){ var c = cinfo(it.cmd) || {}, t = it.t || 's', l = it.label || short(c.desc) || it.cmd; return { t:t, label:t === 'lg' ? twoLines(l) : l, icon:it.icon || c.icon || 'play', cmd:it.cmd, id:'cu-' + it.uid, _ck:'cu:' + it.uid, _cu:it }; }
  function lay(items){ var out = [], col = null; items.forEach(function(it){ var c = ctlOf(it); if (c.t === 'lg'){ out.push(c); col = null; return; } if (!col || col.items.length >= 3){ col = { t:'col', items:[] }; out.push(col); } col.items.push(c); }); return out; }
  function cpanel(x){ return { name:x.name, icon:x.icon || 'star', prio:x.prio || 1, items:lay(x.items), _pk:'cu:' + x.uid, _cu:x }; }
  // điều khiển có sẵn mang khóa “Tab/Panel/lệnh” (trùng thì thêm #2) để đổi nhãn, biểu tượng
  function cloneItems(items, pre, mod, seen){ return items.map(function(c){ if ((c.t === 'col' || c.t === 'row') && c.items) return Object.assign({}, c, { items:cloneItems(c.items, pre, mod, seen) }); var b = c.id || c.cmd || c.label, n = seen[b] = (seen[b] || 0) + 1, k = pre + '/' + b + (n > 1 ? '#' + n : ''), m = mod[k] || {}; return Object.assign({}, c, { _ck:k, _ol:c.label, _oi:c.icon }, m.label != null ? { label:m.label } : {}, m.icon ? { icon:m.icon } : {}); }); }
  function build(cu){ return grab().tabs.map(function(t){ var mt = cu.mod[t.name] || {}, ps = t.panels.map(function(p){ var k = t.name + '/' + p.name, mp = cu.mod[k] || {}, items = cloneItems(p.items, k, cu.mod, {}), add = cu.add[k] || []; return Object.assign({}, p, { name:mp.label || p.name, icon:mp.icon || p.icon, prio:mp.prio || p.prio, items:add.length ? items.concat(lay(add)) : items, _pk:k, _on:p.name, _hide:!!mp.hide }); });
      cu.panels.forEach(function(x){ if (x.tab === t.name) ps.push(cpanel(x)); });
      return Object.assign({}, t, { name:mt.label || t.name, panels:ps, _tk:t.name, _on:t.name, _hide:!!mt.hide }); }).concat(cu.tabs.map(function(x){ return { name:x.name, panels:x.panels.map(cpanel), _tk:'cu:' + x.uid, _cu:x }; })); }
  function live(cu){ var b = build(cu), l = b.filter(function(t){ return !t._hide; }); return (l.length ? l : b.slice(0, 1)).map(function(t){ return Object.assign({}, t, { panels:t.panels.filter(function(p){ return !p._hide; }) }); }); }
  // tệp tùy biến phụ: của plugin đang chạy và tệp gắn thêm (chỉ đọc)
  function parts(cu){ return plugParts().concat((cu.part || []).map(function(p){ var s = XSRC[p.id]; return s ? { id:p.id, name:s.name, path:s.path, src:'gắn thêm', tb:s.tb, menus:s.menus, cmds:s.cmds.map(function(c){ return Object.assign({ file:s.name }, c); }) } : null; }).filter(Boolean)); }
  function mItem(i){ var n = i.cmd || i, c = cinfo(n); return [i.label || (c ? c.desc : n), n]; }
  function tbItem(i){ var n = i.cmd || i, c = cinfo(n) || {}; return [i.icon || c.icon || 'play', n]; }
  // menu tùy biến đứng trước menu Trợ giúp (giữ Trợ giúp ở cuối như quen dùng)
  function menusOf(cu){ var b = grab().menus.map(function(m){ var a = cu.madd[m[0]] || []; return a.length ? [m[0], m[1].concat(['|'], a.map(mItem))] : m; }), c = cu.menus.map(function(m){ return [m.name, m.items.map(mItem)]; }).concat([].concat.apply([], parts(cu).map(function(p){ return (p.menus || []).map(function(m){ return [m.name, m.items.map(mItem)]; }); }))); return b.slice(0, -1).concat(c, b.slice(-1)); }
  function tbsOf(cu){ return grab().tb.concat(cu.tb.map(function(t){ return [t.name, t.pos || 1, t.items.map(tbItem)]; }), [].concat.apply([], parts(cu).map(function(p){ return (p.tb || []).map(function(t){ return [t.name, 1, t.items.map(tbItem)]; }); }))); }
  // lệnh tùy biến thành lệnh của TrionCAD: bấm nút, gõ tên đều chạy macro
  function regCmds(cu){ Object.keys(REG).forEach(function(n){ if (TC.CMDS[n] && TC.CMDS[n].cu){ delete TC.CMDS[n]; delete IMPL[n]; } }); REG = {};
    cu.cmds.concat([].concat.apply([], parts(cu).map(function(p){ return p.cmds || []; }))).forEach(function(c){ if (TC.CMDS[c.name] && !TC.CMDS[c.name].cu) return;
      TC.CMDS[c.name] = { name:c.name, aliases:[], need:'PRO', wave:5, desc:c.label, vi:TC.fold(c.label), icon:c.icon || 'play', where:'Lệnh tùy biến · ' + (c.file || 'trion.tcui'), cu:1 };
      IMPL[c.name] = function(){ setTimeout(function(){ runMacro(c.macro, c.name); }, 0); }; REG[c.name] = 1; }); }
  function applyCU(cu, quiet){ var R = TC.RIBBON_PRO, M = TC.CLASSIC_MENUS, B = TC.CLASSIC_TB; grab(); regCmds(cu);
    [].splice.apply(R.tabs, [0, R.tabs.length].concat(live(cu))); [].splice.apply(M, [0, M.length].concat(menusOf(cu))); [].splice.apply(B, [0, B.length].concat(tbsOf(cu)));
    CU = cu; if (booted) booted.then(loadMissing); if (!quiet && A().active !== 'start'){ A().renderRibbon(); A().renderTitle(); } }
  // macro kiểu AutoCAD: ^C^C hủy lệnh, ; hoặc dấu cách là Enter, \ chờ người dùng nhập; (…) là LISP; $M= là DIESEL
  function macroToks(m){ var s = String(m || '').trim().replace(/^(\^C)+/i, ''); if (/\$M=|\$\(/i.test(s)) return { diesel:true }; if (/^\(/.test(s)) return { lisp:s };
    var out = [], cur = ''; for (var i = 0; i < s.length; i++){ var ch = s[i]; if (ch === ';' || ch === ' '){ out.push(cur); cur = ''; } else if (ch === '\\'){ if (cur) out.push(cur); cur = ''; out.push(PAUSE); } else cur += ch; } if (cur) out.push(cur); return { toks:out }; }
  function macroCheck(m){ if (!String(m || '').replace(/\^C/gi, '').trim()) return ['err', 'Macro trống: nút sẽ không làm gì.']; var t = macroToks(m);
    if (t.diesel) return ['warn', 'Macro dùng DIESEL ($M=…). TrionCAD chưa chạy được biểu thức DIESEL phức tạp: thay bằng lệnh hoặc LISP tương đương.'];
    if (t.lisp){ try { read(t.lisp); } catch (e){ return ['err', 'Biểu thức LISP lỗi: ' + e.vi + '.']; } var f = /\(c:([\w-]+)\)/i.exec(t.lisp); return f && !TC.lisp.has('C:' + f[1]) ? ['warn', 'Hàm c:' + f[1] + ' chưa được nạp. Thêm tệp LISP chứa hàm này vào “Tệp LISP đi kèm”.'] : ['hint', 'Bấm nút sẽ chạy biểu thức LISP.']; }
    var first = (t.toks.filter(function(x){ return x && x !== PAUSE; })[0] || '').replace(/^[_.]+/, ''), nm = first.replace(/^-/, '').toUpperCase();
    if (!(nm === 'LAYER' && first[0] === '-') && !cinfo(nm) && !TC.cmd.def(nm)) return ['warn', 'Không có lệnh “' + first + '”. Kiểm tra tên lệnh hoặc viết tắt.'];
    return ['hint', 'Bấm nút sẽ chạy: ' + t.toks.map(function(x){ return x === PAUSE ? '[chờ nhập]' : x === '' ? '↵' : x.replace(/^_/, ''); }).join(' › ')]; }
  function runMacro(m, name){ var t = macroToks(m), fn = t.lisp && /^\s*\(c:([\w-]+)\)\s*$/i.exec(t.lisp);
    if (t.diesel){ TC.toast('Lệnh <b>' + esc(name) + '</b> dùng macro DIESEL; TrionCAD chưa chạy được biểu thức này. Sửa macro trong Tùy biến giao diện.', { type:'warn', log:false, act:{ label:'Mở CUI', fn:function(){ SC.cui('cmd:' + name); } } }); return; }
    if (fn){ var c = TC.CMDS[fn[1].toUpperCase()]; if (c && c.lisp) return TC.cmd.run(c.name); TC.toast('Hàm <b class="mono">c:' + esc(fn[1]) + '</b> chưa được nạp. Nạp tệp LISP chứa hàm (APPLOAD hoặc Bảng điều khiển LISP), hoặc thêm vào “Tệp LISP đi kèm” trong CUI.', { type:'warn', log:false, act:{ label:'Bảng điều khiển LISP', fn:function(){ TC.cmd.run('LISPCONSOLE'); } } }); return; }
    if (L.busy){ TC.cmdline.msg('LISP đang chạy một biểu thức khác; thử lại sau.'); return; }
    boot().then(function(){ return exec(t.lisp ? function(){ return read(t.lisp).reduce(function(p, f){ return p.then(function(){ return ev(f); }); }, Promise.resolve()); } : function(){ return command(t.toks); }, { quiet:true, cl:true }); });
  }
  // nạp bản vá lúc khởi động (trước lần vẽ ribbon đầu); nút thêm vào thanh truy cập nhanh
  var cuBooted = false;
  TC.on('title', function(){
    if (!cuBooted){ cuBooted = true; try { applyCU(CU, true); } catch (e){ CU = CU0(); try { applyCU(CU, true); } catch (e2){} TC.save('cui1', null); setTimeout(function(){ TC.toast('Tệp tùy biến trion.tcui bị lỗi nên TrionCAD dùng giao diện mặc định. Mở Tùy biến giao diện để làm lại.', { type:'warn', act:{ label:'Mở CUI', fn:function(){ TC.cmd.run('CUI'); } } }); }, 900); } }
    var more = $('#qat-more'), list = CU.qat.filter(function(n){ return TC.CMDS[n]; }); if (!more || TC.isViewer() || !list.length) return;
    more.insertAdjacentHTML('beforebegin', '<span class="sep"></span>' + list.map(function(n){ var c = TC.CMDS[n]; return '<button class="ib" type="button" data-run="' + esc(n) + '" aria-label="' + esc(c.desc) + '" data-tip="' + esc(c.desc) + '" data-cmd="' + esc(n) + '">' + ic(c.icon || 'play') + '</button>'; }).join(''));
  });
  // nhấp đúp, nút chuột, menu chuột phải theo tệp tùy biến
  var DBL = [['text', 'Chữ: Text, MText, multileader, kích thước', 'Sửa chữ tại chỗ'], ['blkatt', 'Block có attribute', 'Trình sửa attribute (EATTEDIT)'], ['blk', 'Block không có attribute', 'Hỏi mở Block Editor (BEDIT)'], ['hatch', 'Hatch', 'Sửa hatch (HATCHEDIT)'], ['pline', 'Polyline', 'Sửa polyline (PEDIT)'], ['spline', 'Spline', 'Sửa spline (SPLINEDIT)'], ['mline', 'Đa tuyến song song', 'Sửa đa tuyến (MLEDIT)'], ['table', 'Ô bảng', 'Sửa nội dung ô'], ['geom', 'Đường thẳng, cung, tròn, điểm', 'Thuộc tính nhanh'], ['image', 'Ảnh, PDF underlay', 'Chọn, mở tab ngữ cảnh']];
  var DBLO = [['', 'Mặc định'], ['PROPERTIES', 'Palette Thuộc tính'], ['LIST', 'Liệt kê thông tin (LIST)'], ['NONE', 'Không làm gì']];
  var MOUSE = [['rbtn', 'Chuột phải trên vùng vẽ (không lệnh, chưa chọn)', [['', 'Menu theo ngữ cảnh'], ['enter', 'Lặp lệnh vừa dùng (Enter)']]], ['rcmd', 'Chuột phải khi đang trong lệnh', [['', 'Menu tùy chọn của lệnh'], ['enter', 'Enter']]], ['mbtn', 'Chuột giữa (giữ)', [['', 'Pan (MBUTTONPAN = 1)'], ['snap', 'Menu ghi đè bắt điểm (MBUTTONPAN = 0)']]]];
  var MFIX = [['Shift + chuột phải', 'Menu ghi đè bắt điểm'], ['Nhấp đúp chuột giữa', 'Zoom toàn bộ đối tượng'], ['Lăn chuột', 'Zoom tại con trỏ (ZOOMFACTOR)']];
  function dblKey(e){ return /^(text|mtext|mleader|dim|dimr|dima|attdef)$/.test(e.t) ? 'text' : e.t === 'insert' ? (((D().blocks[e.name] || {}).ents || []).some(function(x){ return x.t === 'attdef'; }) ? 'blkatt' : 'blk') : /^(line|arc|circle|ellipse|point|xline|ray)$/.test(e.t) ? 'geom' : e.t; }
  var dbl0 = TC.cad.hook.dbl;
  TC.cad.hook.dbl = function(e, m){ var a = e && !TC.isViewer() && CU.dbl[dblKey(e)];
    if (a){ if (a !== 'NONE'){ var d = D(); d.sel = [e.id]; TC.cad.afterSel(); if (a === 'PROPERTIES') A().showPalette('props'); else TC.cmd.run(a); } return true; }
    return dbl0 ? dbl0(e, m) : false; };
  var ctx0 = TC.cad.context;
  TC.cad.context = function(m, e){ var mo = CU.mouse || {}, d = D(), busy = TC.cmdline.busy();
    if (d && !TC.isViewer()){ if (busy && mo.rcmd === 'enter'){ TC.cmdline.enter(); return; } if (!busy && !d.sel.length && mo.rbtn === 'enter' && TC.cmd.last()){ TC.cmd.run(TC.cmd.last()); return; } }
    return ctx0.apply(this, arguments); };
  TC.cad.ctxExt.push(function(it){ var l = CU.ctx.filter(function(i){ return TC.CMDS[i.cmd]; }); if (!l.length || TC.isViewer()) return; it.push({ sep:1 }); l.forEach(function(i){ var c = TC.CMDS[i.cmd]; it.push({ label:esc(i.label || c.desc), icon:i.icon || c.icon, cmd:i.cmd, act:function(){ TC.cmd.run(i.cmd); } }); }); });

  // tệp để chuyển, gắn: CUIX của AutoCAD (đọc một phần như TCMIGRATE UI-K12) và tệp TrionCAD dùng chung
  var XSRC = {
    cuix:{ id:'cuix', name:'acad.cuix', note:'AutoCAD 2024 · hồ sơ VN-KT', path:'C:\\Users\\An\\AppData\\Roaming\\Autodesk\\AutoCAD 2024\\R24.3\\enu\\Support\\acad.cuix',
      cmds:[{ name:'VT220', label:'Vẽ tường 220', macro:'^C^C_-LAYER;_S;KT-TUONG;;_MLINE;_ST;TUONG-220;_S;1', icon:'pline', tip:'Layer KT-TUONG, MLINE kiểu TUONG-220' }, { name:'VT110', label:'Vẽ tường 110', macro:'^C^C_-LAYER;_S;KT-TUONG;;_MLINE;_ST;TUONG-110;_S;1', icon:'pline' }, { name:'COT220', label:'Cột 220×220', macro:'^C^C_RECTANG;\\@220,220', icon:'rect' },
        { name:'LTUONG', label:'Layer tường', macro:'^C^C_-LAYER;_S;KT-TUONG;;', icon:'layers' }, { name:'LCUA', label:'Layer cửa', macro:'^C^C_-LAYER;_S;KT-CUA;;', icon:'layers' }, { name:'LGHICHU', label:'Layer ghi chú', macro:'^C^C_-LAYER;_M;KT-GHICHU;;', icon:'layers' },
        { name:'GHICD', label:'Ghi cao độ', macro:'^C^C(c:CD)', icon:'elev' }, { name:'DANHSOP', label:'Đánh số phòng', macro:'^C^C(c:DANHSO)', icon:'mtext' }, { name:'DIMKT', label:'Kích thước kiến trúc', macro:'^C^C_DIMLINEAR', icon:'dim' }, { name:'XOATRUNG', label:'Xóa trùng lặp', macro:'^C^C_OVERKILL', icon:'erase' },
        { name:'INA3', label:'In A3 theo khung', macro:'$M=$(if,$(eq,$(getvar,tilemode),1),^C^C_TCBATCHPLOT,^C^C_PLOT)', icon:'print' }, { name:'DOILAYOUT', label:'Đổi Model, Layout', macro:'$M=$(if,$(getvar,tilemode),^C^C_tilemode;0,^C^C_tilemode;1)', icon:'layout' }],
      tabs:[{ name:'KT Công ty', panels:[{ name:'Tường, cột', icon:'pline', items:[['VT220', 'lg'], ['VT110'], ['COT220']] }, { name:'Layer', icon:'layers', items:[['LTUONG'], ['LCUA'], ['LGHICHU']] }, { name:'Ghi chú', icon:'mtext', items:[['GHICD'], ['DANHSOP'], ['TCAREA']] }, { name:'In', icon:'print', items:[['INA3', 'lg', 'In A3']] }] }],
      tb:[{ name:'KT Vẽ nhanh', items:['VT220', 'VT110', 'COT220'] }, { name:'KT Kích thước', items:['DIMKT', 'DIMCONTINUE', 'QDIM'] }, { name:'KT Layer', items:['LTUONG', 'LCUA', 'LGHICHU'] }],
      menus:[{ name:'KT Tiện ích', items:['VT220', 'VT110', 'COT220', 'DANHSOP', 'GHICD', 'XOATRUNG', 'INA3', 'DOILAYOUT'] }],
      skip:[['Tab ngữ cảnh “Hatch Editor”', 'TrionCAD có tab ngữ cảnh Hatch riêng'], ['Tool palette “Ky hieu KT” (2 nhóm)', 'chuyển bằng Tool Palettes › Nhập'], ['Menu ảnh “Cửa đi, cửa sổ”', 'menu ảnh (image tile) không còn dùng'], ['Nhấp đúp “Block KT”', 'đặt lại ở Hành động nhấp đúp']] },
    kcn:{ id:'kcn', name:'ketcau-nhom.tcui', note:'TrionCAD · dùng chung nhóm Kết cấu', path:'\\\\SRV-01\\CAD\\ketcau-nhom.tcui',
      cmds:[{ name:'COT300', label:'Cột 300×300', macro:'^C^C_RECTANG;\\@300,300', icon:'rect' }, { name:'LTHEP', label:'Layer thép', macro:'^C^C_-LAYER;_M;KC-THEP;;', icon:'layers' }],
      tabs:[], tb:[{ name:'Kết cấu', items:['COT300', 'LTHEP', 'TCREBAR', 'COUNT'] }], menus:[{ name:'Kết cấu', items:['COT300', 'LTHEP', 'TCREBAR', 'TCLENGTH'] }], skip:[] }
  };
  var QATD = [['new', 'NEW'], ['open', 'OPEN'], ['save', 'QSAVE'], ['saveas', 'SAVEAS'], ['undo', 'U'], ['redo', 'REDO'], ['print', 'PLOT']];
  var CTXS = [['sel', 'Có vùng chọn', ['Lặp lệnh', 'Lệnh gần đây ▸', 'Clipboard ▸', 'Cô lập ▸', 'Xóa', 'Di chuyển', 'Sao chép', 'Tỉ lệ', 'Xoay', 'Thứ tự hiển thị ▸', 'Tạo đối tượng cùng loại', 'Chọn tương tự', 'Chọn nhanh…', 'Bỏ chọn tất cả', 'Liệt kê thông tin', 'Thuộc tính']], ['idle', 'Không lệnh, không chọn', ['Lặp lệnh', 'Lệnh gần đây ▸', 'Clipboard ▸', 'Cô lập ▸', 'Hoàn tác', 'Làm lại', 'Pan', 'Zoom ▸', 'Đo ▸', 'Chọn tất cả', 'Tìm…', 'Tùy chọn…']], ['cmd', 'Đang trong lệnh', ['Enter', 'Hủy', 'Tùy chọn của lệnh', 'Ghi đè bắt điểm ▸', 'Pan', 'Zoom']], ['grip', 'Trên grip nóng', ['Kéo giãn', 'Di chuyển', 'Xoay', 'Tỉ lệ', 'Đối xứng', 'Điểm gốc', 'Sao chép', 'Hoàn tác', 'Thoát']]];
  var WSB = [['2d', 'Soạn thảo 2D', 'Ribbon đủ tab; palette Thuộc tính, Layer bên phải.'], ['classic', 'Cổ điển', 'Thanh menu và toolbar như AutoCAD cũ, không có ribbon.'], ['viewer', 'Viewer', 'Ribbon rút gọn của bản xem: Xem, Đo, In và xuất, Công cụ.']];
  // đơn vị thay đổi để đếm “Áp dụng N thay đổi”
  function units(c){ var u = {}; Object.keys(c.mod).forEach(function(k){ u['mod:' + k] = c.mod[k]; }); c.tabs.forEach(function(x){ u['tab:' + x.uid] = x; }); c.panels.forEach(function(x){ u['pnl:' + x.uid] = x; });
    Object.keys(c.add).forEach(function(k){ c.add[k].forEach(function(i){ u['add:' + i.uid] = [k, i]; }); }); c.tb.forEach(function(x){ u['tb:' + x.uid] = x; }); c.menus.forEach(function(x){ u['mn:' + x.uid] = x; });
    Object.keys(c.madd).forEach(function(k){ c.madd[k].forEach(function(i){ u['madd:' + i.uid] = [k, i]; }); }); c.cmds.forEach(function(x){ u['cmd:' + x.uid] = x; });
    ['qat', 'ctx', 'lisp', 'part'].forEach(function(k){ if (c[k].length) u[k] = c[k]; }); ['dbl', 'mouse'].forEach(function(k){ Object.keys(c[k]).forEach(function(j){ if (c[k][j]) u[k + ':' + j] = c[k][j]; }); });
    return u; }
  function diff(a, b){ var ua = units(a), ub = units(b); return Object.keys(ua).concat(Object.keys(ub)).filter(function(k, i, l){ return l.indexOf(k) === i && JSON.stringify(ua[k]) !== JSON.stringify(ub[k]); }); }

  SC.cui = function(sel){
    if (!TC.can('PRO')){ SC.upgrade('Tùy biến giao diện (CUI)', 'PRO'); return; }
    if (TC.topDialog() && TC.topDialog().id === 'cui'){ TC.topDialog().el.focus(); return; }
    grab();
    var dr = DRAFT = clone(CU), cs = sel && sel.indexOf('cmd:') === 0 ? sel.slice(4) : null;
    var U = { sel:cs ? 'f' : sel || 't:Trang chủ', it:null, cmd:cs, foc:cs ? 'cmd' : 'node', q:cs || '', grp:'Tất cả lệnh', open:{ f:1, rb:1, 't:Trang chủ':1 }, xs:'cuix', xc:{}, xmsg:'', nk:{}, help:null };
    var dlg = TC.dialog({ id:'cui', title:'Tùy biến giao diện', cmd:'CUI', code:'UI-K07', w:1180, h:684, flush:true,
      onClose:function(r){ var n = diff(dr, CU).length; if (r === 'ok' || r === 'force' || !n){ DRAFT = null; return; } TC.confirm({ title:'Đóng Tùy biến giao diện', msg:'Bỏ <b>' + n + '</b> thay đổi chưa áp dụng?', detail:'Giao diện giữ như lần áp dụng trước.', ok:'Bỏ ' + n + ' thay đổi', cancel:'Quay lại', danger:true }).then(function(y){ if (y) dlg.close('force'); }); return false; },
      body:'<div class="cuw">' + H.tabs([['cust', 'Tùy biến'], ['xfer', 'Chuyển giữa hai tệp']], 'cust', 'cut') +
        '<div class="dpane cumain" data-dp="cust"><section class="cul"><div class="cu-h"><b>Cây tùy biến</b><span class="grow"></span><button class="btn sm ghost" type="button" data-cu="new" aria-haspopup="menu">' + ic('plus') + 'Mới</button><button class="btn sm ghost" type="button" data-cu="imp" data-tip="Nhập CUIX của AutoCAD" data-desc="Đọc tab ribbon, panel, toolbar, menu và macro từ tệp .cuix; chọn mục cần chép ở tab Chuyển.">' + ic('open') + 'Nhập CUIX…</button></div>' +
          '<div class="tree cu-tree" id="cu-tree" role="tree" aria-label="Cây tùy biến" tabindex="0"></div>' +
          '<div class="cu-h"><b>Danh sách lệnh</b><span class="faint mono" id="cu-cn"></span><span class="grow"></span><button class="btn sm ghost" type="button" data-cu="newcmd">' + ic('plus') + 'Lệnh mới</button></div>' +
          '<div class="cu-cf"><label class="search">' + ic('search') + '<input id="cu-q" placeholder="Tìm: “cắt”, TRIM, CO…" aria-label="Tìm lệnh" spellcheck="false" value="' + esc(U.q) + '"></label><select class="inp" id="cu-g" aria-label="Lọc theo nhóm"></select></div>' +
          '<div class="cu-cl" id="cu-cl" role="listbox" aria-label="Danh sách lệnh" tabindex="0"></div></section>' +
          '<section class="cuc" id="cu-c" aria-label="Xem trước"></section>' +
          '<section class="cur"><div class="cu-h"><b>Thuộc tính</b></div><div class="cu-props" id="cu-p"></div><div class="pal-desc" id="cu-d" aria-live="polite"></div></section></div>' +
        '<div class="dpane cux" data-dp="xfer" hidden></div></div>',
      foot:'<button class="btn ghost" type="button" id="cu-rs">' + ic('undo') + 'Khôi phục mặc định…</button><button class="btn sm ghost cu-n" type="button" id="cu-n" aria-haspopup="menu" hidden></button><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="cu-ok" disabled>Áp dụng</button>' });
    var el = dlg.el;

    /* ---- tra cứu trong bản nháp ---- */
    function V0(){ return build(dr); }
    function vPanel(pk){ var r = null; V0().forEach(function(t){ t.panels.forEach(function(p){ if (p._pk === pk) r = { t:t, p:p }; }); }); return r; }
    function cPanel(id){ return dr.panels.concat([].concat.apply([], dr.tabs.map(function(t){ return t.panels; }))).filter(function(x){ return x.uid === id; })[0] || null; }
    function cTab(id){ return dr.tabs.filter(function(x){ return x.uid === id; })[0] || null; }
    function leaves(items){ var o = []; (function w(l){ l.forEach(function(c){ if ((c.t === 'col' || c.t === 'row') && c.items) w(c.items); else o.push(c); }); })(items); return o; }
    function setMod(k, f, v){ var m = Object.assign({}, dr.mod[k]); if (v == null || v === '' || v === false) delete m[f]; else m[f] = v; if (Object.keys(m).length) dr.mod[k] = m; else delete dr.mod[k]; }
    function partsList(kind){ return [].concat.apply([], parts(dr).map(function(p){ return (p[kind] || []).map(function(t){ return { k:(kind === 'tb' ? 'tb:' : 'mn:') + 'pt:' + p.id + ':' + t.name, name:t.name, part:p, pt:t }; }); })); }
    function tbList(){ return grab().tb.map(function(t){ return { k:'tb:' + t[0], name:t[0], items:Array.isArray(t[2]) ? t[2] : null }; }).concat(dr.tb.map(function(t){ return { k:'tb:cu:' + t.uid, name:t.name, x:t }; }), partsList('tb')); }
    function mnList(){ return grab().menus.map(function(m){ return { k:'mn:' + m[0], name:m[0], m:m }; }).concat(dr.menus.map(function(m){ return { k:'mn:cu:' + m.uid, name:m.name, x:m }; }), partsList('menus')); }
    function wsNames(){ return WSB.map(function(w){ return w[1]; }).concat(((TC.WS || {}).list || []).map(function(u){ return u.name; })); }
    function ci(n){ return cinfo(n) || TC.cmd.def(n) || { name:n, desc:n, icon:'play', aliases:[], where:'' }; }
    function cont(k){
      if (k === 'qat') return { list:dr.qat, q:1 }; if (k === 'ctx:sel') return { list:dr.ctx };
      if (/^p:/.test(k)){ var pk = k.slice(2); if (/^cu:/.test(pk)){ var x = cPanel(pk.slice(3)); return x && { list:x.items, pnl:1, cu:1 }; } return vPanel(pk) ? { list:dr.add[pk] || (dr.add[pk] = []), pnl:1 } : null; }
      if (/^tb:cu:/.test(k)){ var t = dr.tb.filter(function(x){ return x.uid === k.slice(6); })[0]; return t && { list:t.items }; }
      if (/^mn:cu:/.test(k)){ var m = dr.menus.filter(function(x){ return x.uid === k.slice(6); })[0]; return m && { list:m.items }; }
      if (/^mn:/.test(k) && grab().menus.some(function(m){ return 'mn:' + m[0] === k; })){ var n = k.slice(3); return { list:dr.madd[n] || (dr.madd[n] = []) }; }
      return null; }
    function cname(k){ if (k === 'qat') return 'Thanh truy cập nhanh'; if (k === 'ctx:sel') return 'menu chuột phải'; if (/^p:/.test(k)){ var vp = vPanel(k.slice(2)); return vp ? 'panel ' + vp.p.name : ''; } var t = tbList().concat(mnList()).filter(function(x){ return x.k === k; })[0]; return t ? (k[0] === 't' ? 'toolbar ' : 'menu ') + t.name : ''; }
    function addTo(k, name){ var c = cont(k); if (!c){ TC.toast('Chọn một panel, toolbar, menu hoặc thanh truy cập nhanh ở cây bên trái rồi thêm lệnh.', { type:'info', log:false }); return false; }
      if (c.q){ if (dr.qat.indexOf(name) < 0) dr.qat.push(name); U.it = 'q:' + name; }
      else { var it = { uid:uid(), cmd:name }; if (c.pnl) it.t = c.cu && !c.list.length ? 'lg' : 's'; c.list.push(it); U.it = 'cu:' + it.uid; }
      U.sel = k; U.foc = 'item'; openPath(k); render(); return true; }
    // các mục của vùng chứa đang chọn (nút panel, toolbar, menu…): bi = nút có sẵn (đổi nhãn, biểu tượng), cu = mục tùy biến, ro = chỉ xem
    function rows(k){
      var cu = function(list){ return function(x){ var c = ci(x.cmd); return { key:'cu:' + x.uid, label:x.label || c.desc, icon:x.icon || c.icon, cmd:x.cmd, t:x.t, cu:x, list:list }; }; }, roR = function(src){ return function(n, i){ var c = ci(n); return { key:'b:' + i, label:c.desc, icon:c.icon, cmd:n, ro:src }; }; };
      if (k === 'qat') return QATD.map(function(q){ return { key:'qd:' + q[1], label:ci(q[1]).desc, icon:q[0], cmd:q[1], ro:'mặc định' }; }).concat(dr.qat.map(function(n){ var c = ci(n); return { key:'q:' + n, label:c.desc, icon:c.icon, cmd:n, q:1 }; }));
      if (/^ctx:/.test(k)){ var cx = CTXS.filter(function(x){ return 'ctx:' + x[0] === k; })[0]; return cx ? cx[2].map(function(l, i){ return { key:'b:' + i, label:l, ro:'có sẵn' }; }).concat(k === 'ctx:sel' ? dr.ctx.map(cu(dr.ctx)) : []) : null; }
      if (/^p:/.test(k)){ var vp = vPanel(k.slice(2)), c0 = cont(k); return vp ? leaves(vp.p.items).map(function(c){ return c._cu ? Object.assign(cu(c0.list)(c._cu), { t:c._cu.t || 's' }) : { key:c._ck, label:String(c.label || '').replace(/\n/g, ' '), icon:c.icon, cmd:c.cmd, t:c.t, bi:c }; }) : null; }
      var tb = tbList().filter(function(t){ return t.k === k; })[0];
      if (tb) return tb.x ? tb.x.items.map(cu(tb.x.items)) : tb.pt ? tb.pt.items.map(roR(tb.part.name)) : (tb.items || []).filter(function(b){ return b !== '|'; }).map(function(b, i){ return { key:'b:' + i, label:ci(b[1]).desc, icon:b[0], cmd:b[1], ro:'có sẵn' }; });
      var mn = mnList().filter(function(m){ return m.k === k; })[0];
      if (mn) return mn.x ? mn.x.items.map(cu(mn.x.items)) : mn.pt ? mn.pt.items.map(roR(mn.part.name)) : mn.m[1].filter(function(x){ return x !== '|' && x[1] && !x._cu; }).map(function(x, i){ var c = TC.CMDS[x[1]]; return { key:'b:' + i, label:x[0], icon:c && c.icon, cmd:x[1], ro:'có sẵn' }; }).concat((dr.madd[mn.name] || []).map(cu(dr.madd[mn.name])));
      return null; }
    function selRow(){ return U.foc === 'item' && (rows(U.sel) || []).filter(function(r){ return r.key === U.it; })[0] || null; }
    function usedIn(n){ var o = []; V0().forEach(function(t){ t.panels.forEach(function(p){ if (leaves(p.items).some(function(c){ return c.cmd === n; })) o.push(['p:' + p._pk, t.name + ' › ' + p.name]); }); }); if (dr.qat.indexOf(n) >= 0) o.push(['qat', 'Thanh truy cập nhanh']);
      tbList().concat(mnList()).forEach(function(x){ var r = rows(x.k) || []; if (r.some(function(i){ return i.cmd === n; })) o.push([x.k, (x.k[0] === 't' ? 'Toolbar ' : 'Menu ') + x.name]); }); if (dr.ctx.some(function(i){ return i.cmd === n; })) o.push(['ctx:sel', 'Menu chuột phải']); return o; }
    function openPath(k){ U.open.f = 1; if (/^(t|p):/.test(k)){ U.open.rb = 1; var tk = /^t:/.test(k) ? k.slice(2) : (vPanel(k.slice(2)) || { t:{} }).t._tk; if (tk) U.open['t:' + tk] = 1; } if (/^tb:/.test(k)) U.open.tbs = 1; if (/^mn:/.test(k)) U.open.mns = 1; if (/^ctx:/.test(k)) U.open.ctx = 1; if (/^ws:/.test(k)) U.open.ws = 1; if (/^part:/.test(k)) U.open.part = 1; }

    /* ---- cây tùy biến ---- */
    function nodes(){ var out = [], V = V0(), add = function(k, d, label, icon, o){ o = o || {}; out.push(Object.assign({ k:k, d:d, label:label, icon:icon }, o)); return !!(o.kids && U.open[k]); }, tg = function(o){ return o._cu ? 'tùy biến' : o._hide ? 'ẩn' : null; };
      if (add('f', 0, 'trion.tcui', 'file', { kids:1, tag:'tệp chính' })){
        if (add('ws', 1, 'Workspace', 'ws', { kids:1, cnt:wsNames().length })) wsNames().forEach(function(n){ add('ws:' + n, 2, n, 'ws'); });
        add('qat', 1, 'Thanh truy cập nhanh', 'star', { cnt:QATD.length + dr.qat.length, drop:1 });
        if (add('rb', 1, 'Ribbon', 'gridview', { kids:1, cnt:V.length })) V.forEach(function(t){ if (add('t:' + t._tk, 2, t.name, 'layout', { kids:1, cnt:t.panels.length, tag:tg(t), dim:t._hide })) t.panels.forEach(function(p){ var a = !p._cu && (dr.add[p._pk] || []).length; add('p:' + p._pk, 3, p.name, p.icon || 'gridview', { tag:tg(p) || (a ? '+' + a + ' nút' : null), dim:p._hide || t._hide, drop:1 }); }); });
        var tbs = tbList(); if (add('tbs', 1, 'Toolbar', 'listview', { kids:1, cnt:tbs.length })) tbs.forEach(function(t){ add(t.k, 2, t.name, 'listview', { tag:t.x ? 'tùy biến' : t.part ? 'tệp phụ' : null, drop:t.x ? 1 : 0 }); });
        var mns = mnList(); if (add('mns', 1, 'Menu (workspace Cổ điển)', 'menu', { kids:1, cnt:mns.length })) mns.forEach(function(m){ var a = !m.x && !m.pt && (dr.madd[m.name] || []).length; add(m.k, 2, m.name, 'menu', { tag:m.x ? 'tùy biến' : m.part ? 'tệp phụ' : a ? '+' + a : null, drop:m.pt ? 0 : 1 }); });
        if (add('ctx', 1, 'Menu chuột phải', 'mouse', { kids:1, cnt:CTXS.length })) CTXS.forEach(function(c){ add('ctx:' + c[0], 2, c[1], 'menu', { drop:c[0] === 'sel' ? 1 : 0, tag:c[0] === 'sel' && dr.ctx.length ? '+' + dr.ctx.length : null }); });
        add('keys', 1, 'Phím tắt', 'keyboard', { cnt:SC.keyRows ? SC.keyRows().length : null });
        add('dbl', 1, 'Hành động nhấp đúp', 'mouse', { tag:Object.keys(dr.dbl).length ? Object.keys(dr.dbl).length + ' đổi' : null });
        add('mouse', 1, 'Nút chuột', 'mouse', { tag:Object.keys(dr.mouse).length ? Object.keys(dr.mouse).length + ' đổi' : null });
        add('lisp', 1, 'Tệp LISP đi kèm', 'lisp', { cnt:dr.lisp.length });
        var ps = parts(dr); if (add('part', 1, 'Tệp tùy biến phụ', 'file', { kids:1, cnt:ps.length })) ps.forEach(function(p){ add('part:' + p.id, 2, p.name, 'file', { tag:p.plug ? 'plugin' : 'chỉ đọc' }); });
      }
      return out; }
    function rTree(){ var tr = $('#cu-tree', el); tr.innerHTML = nodes().map(function(n){ var on = U.sel === n.k && U.foc !== 'cmd'; return '<button class="tn' + (n.dim ? ' dim' : '') + '" type="button" role="treeitem" tabindex="-1" data-tn="' + esc(n.k) + '"' + (n.drop ? ' data-drop="' + esc(n.k) + '"' : '') + ' aria-selected="' + on + '"' + (n.kids ? ' aria-expanded="' + !!U.open[n.k] + '"' : '') + ' style="padding-left:' + (4 + n.d * 14) + 'px"><span class="tg" data-tg="' + (n.kids ? esc(n.k) : '') + '">' + (n.kids ? ic(U.open[n.k] ? 'down' : 'right', 'xs') : '') + '</span>' + ic(n.icon || 'file', 's12') + '<span class="nm">' + esc(n.label) + '</span>' + (U.nk[n.k] ? '<span class="tag ok">mới</span>' : n.tag ? '<span class="tag">' + esc(n.tag) + '</span>' : '') + (n.cnt != null ? '<span class="cnt">' + n.cnt + '</span>' : '') + '</button>'; }).join('');
      var s = $('.tn[aria-selected="true"]', tr); if (s) s.scrollIntoView({ block:'nearest' }); }

    /* ---- danh sách lệnh: tìm, lọc theo nhóm, kéo vào panel, toolbar, menu ---- */
    function allCmds(){ var o = Object.keys(TC.CMDS).filter(function(n){ return !TC.CMDS[n].cu && n !== 'GRIPEDIT'; }).map(function(n){ return TC.CMDS[n]; });
      dr.cmds.forEach(function(x){ o.push({ name:x.name, desc:x.label, icon:x.icon || 'play', aliases:[], vi:'', where:'Lệnh tùy biến', cu:x }); });
      parts(dr).forEach(function(p){ (p.cmds || []).forEach(function(x){ o.push({ name:x.name, desc:x.label, icon:x.icon || 'play', aliases:[], vi:'', where:'Lệnh tùy biến', cu:x, pt:p.name }); }); }); return o; }
    function grpOf(c){ var w = String(c.where || '').split(' › ')[0]; return c.cu ? 'Lệnh tùy biến' : c.lisp ? 'Lệnh LISP' : c.plug ? 'Plugin' : grab().tabs.some(function(t){ return t.name === w; }) ? w : 'Khác'; }
    function rCmds(){ var q = TC.fold(U.q.trim()), l = allCmds().filter(function(c){ return (U.grp === 'Tất cả lệnh' || grpOf(c) === U.grp) && (!q || TC.fold(c.name + ' ' + c.desc + ' ' + (c.vi || '') + ' ' + c.aliases.join(' ')).indexOf(q) >= 0); }).sort(function(a, b){ return (b.cu ? 1 : 0) - (a.cu ? 1 : 0) || a.desc.localeCompare(b.desc, 'vi'); });
      var gs = ['Tất cả lệnh', 'Lệnh tùy biến', 'Lệnh LISP', 'Plugin'].concat(grab().tabs.map(function(t){ return t.name; }), ['Khác']), g = $('#cu-g', el);
      if (!g.options.length) g.innerHTML = gs.map(function(x){ return '<option>' + esc(x) + '</option>'; }).join(''); g.value = U.grp;
      $('#cu-cn', el).textContent = TC.n(l.length);
      $('#cu-cl', el).innerHTML = l.length ? l.slice(0, 400).map(function(c){ var w = c.cu && c.cu.macro != null && macroCheck(c.cu.macro)[0] !== 'hint', on = U.foc === 'cmd' && U.cmd === c.name; return '<div class="cu-ci' + (on ? ' is-sel' : '') + '" role="option" draggable="true" data-cn="' + esc(c.name) + '" aria-selected="' + on + '" title="' + esc(c.desc + ' · ' + c.name) + '">' + ic(c.icon || 'play') + '<span class="n">' + esc(c.desc) + '</span>' + (w ? ic('warn', 's12') : '') + (c.cu ? '<span class="tag acc">' + (c.pt ? 'tệp phụ' : 'tùy biến') + '</span>' : c.lisp ? '<span class="tag">LISP</span>' : c.plug ? '<span class="tag">plugin</span>' : '') + '<span class="mono c">' + esc(c.name) + '</span></div>'; }).join('')
        : '<div class="empty">' + ic('search') + '<b>Không có lệnh khớp “' + esc(U.q) + '”</b>Thử tên tiếng Anh (TRIM) hoặc từ khóa tiếng Việt (“cắt”).</div>';
      var s = $('.cu-ci.is-sel', el); if (s) s.scrollIntoView({ block:'nearest' }); }

    /* ---- xem trước: panel ribbon, thanh truy cập nhanh, toolbar, menu ---- */
    function txt(c){ try { return c.text ? c.text() : c.label; } catch (e){ return c.label; } }
    function pvCtl(c){ var on = U.foc === 'item' && U.it === c._ck ? ' is-sel' : '', a = ' data-ci="' + esc(c._ck || '') + '"';
      if ((c.t === 'col' || c.t === 'row') && c.items) return '<div class="rb-' + c.t + '"' + (c.gap ? ' style="gap:' + c.gap + 'px"' : '') + '>' + c.items.map(pvCtl).join('') + '</div>';
      if (c.t === 'lg') return '<button class="rb-lg' + on + '" type="button" tabindex="-1"' + a + '>' + ic(c.icon, 'lg') + '<span>' + esc(c.label).replace(/\n/g, '<br>') + (c.menu ? ic('down', 'xs') : '') + '</span></button>';
      if (c.t === 's' || c.t === 'io') return '<button class="rb-s' + (c.t === 'io' ? ' io' : '') + on + '" type="button" tabindex="-1"' + a + '>' + ic(c.icon) + (c.t === 's' ? '<span class="t">' + esc(c.label) + '</span>' : '') + (c.menu ? ic('down', 'xs') : '') + '</button>';
      if (c.t === 'dd') return '<span class="rb-dd' + on + '"' + a + ' style="width:' + (c.w || 160) + 'px"><span class="grow">' + esc(txt(c)) + '</span>' + ic('down', 'xs') + '</span>';
      if (c.t === 'num') return '<span class="rb-num' + on + '"' + a + '><span>' + esc(c.label) + '</span><input class="mono" disabled style="width:' + (c.w || 60) + 'px" aria-label="' + esc(c.label) + '"></span>';
      if (c.t === 'gal') return '<span class="rb-gal cu-gal' + on + '"' + a + ' style="width:' + (c.w || 220) + 'px">' + esc(c.label) + '</span>';
      return ''; }
    function pvPanel(p){ return '<div class="rb-p"><div class="rb-c">' + p.items.map(pvCtl).join('') + '</div><div class="rb-l">' + esc(p.name) + '</div></div>'; }
    function pvIcons(rs, cls){ return '<div class="' + cls + '">' + (cls === 'tbar cu-tbar' ? '<span class="tb-grip"></span>' : '') + rs.map(function(r, i){ return (cls === 'cu-qat' && i === QATD.length ? '<span class="sep"></span>' : '') + '<button class="ib' + (U.it === r.key ? ' is-sel' : '') + '" type="button" tabindex="-1" data-ci="' + esc(r.key) + '" data-tip="' + esc(r.label) + '">' + ic(r.icon || 'play') + '</button>'; }).join('') + (cls === 'cu-qat' ? '<span class="ib">' + ic('down', 'xs') + '</span>' : '') + '</div>'; }
    function pvMenu(rs, title){ return '<div class="cu-mn"><div class="cu-mnh">' + esc(title) + '</div>' + rs.map(function(r){ return '<button class="mi' + (U.it === r.key ? ' is-hot' : '') + (r.ro && !r.cmd ? ' ro' : '') + '" type="button" tabindex="-1" data-ci="' + esc(r.key) + '"><span class="ico">' + (r.icon ? ic(r.icon) : '') + '</span><span>' + esc(r.label) + '</span>' + (r.cmd ? '<span class="k">' + esc(r.cmd) + '</span>' : '') + '</button>'; }).join('') + '</div>'; }
    function head(t, sub, acts){ return '<div class="cu-ch"><div class="cu-cht"><b>' + esc(t) + '</b><span class="faint">' + sub + '</span></div><span class="grow"></span>' + (acts || '') + '</div>'; }
    function sub(t, n, acts){ return '<div class="cu-sub"><b>' + t + '</b>' + (n != null ? '<span class="faint mono">' + n + '</span>' : '') + '<span class="grow"></span>' + (acts || '') + '</div>'; }
    function itBtns(){ var r = selRow(), ok = r && (r.cu || r.q); return '<button class="btn sm" type="button" data-cu="up"' + (ok ? '' : ' disabled') + '>' + ic('up') + 'Lên</button><button class="btn sm" type="button" data-cu="down"' + (ok ? '' : ' disabled') + '>' + ic('down') + 'Xuống</button><button class="btn sm" type="button" data-cu="del"' + (ok ? '' : ' disabled') + '>' + ic('trash') + 'Bỏ</button>'; }
    function itTable(rs, kind){ return rs.length ? '<div class="tblwrap cu-it"><table class="tbl"><colgroup><col style="width:30px"><col><col style="width:150px"><col style="width:' + (kind === 'pnl' ? 92 : 104) + 'px"></colgroup><thead><tr><th></th><th>Nhãn</th><th>Lệnh</th><th>' + (kind === 'pnl' ? 'Cỡ' : 'Nguồn') + '</th></tr></thead><tbody>' + rs.map(function(r){ return '<tr data-ci="' + esc(r.key) + '" class="' + (U.foc === 'item' && U.it === r.key ? 'is-sel' : '') + (r.ro ? ' ro' : '') + '"><td class="ctr">' + (r.icon ? ic(r.icon) : '') + '</td><td>' + esc(r.label) + (r.cu ? ' <span class="tag acc">tùy biến</span>' : r.bi && dr.mod[r.key] ? ' <span class="tag info">đã đổi</span>' : '') + '</td><td class="mono faint">' + esc(r.cmd || (r.bi ? { dd:'danh sách', num:'ô số', gal:'gallery' }[r.t] || '—' : '—')) + '</td><td>' + (kind === 'pnl' ? ({ lg:'Lớn', s:'Nhỏ', io:'Biểu tượng', dd:'Thả xuống', num:'Ô số', gal:'Gallery' }[r.t || 's'] || '') : r.ro ? '<span class="faint">' + esc(r.ro) + '</span>' : 'tùy biến') + '</td></tr>'; }).join('') + '</tbody></table></div>' : '<div class="empty cu-empty">' + ic('drag') + '<b>Chưa có lệnh nào</b>Kéo lệnh từ Danh sách lệnh vào đây, hoặc chọn lệnh rồi bấm “Thêm vào”.</div>'; }
    function hint(){ return '<p class="cu-hint">' + ic('drag', 's12') + 'Kéo lệnh từ Danh sách lệnh thả vào đây; hoặc nhấp đúp lệnh, Enter trên lệnh để thêm.</p>'; }
    function gone(){ return '<div class="empty">' + ic('info') + '<b>Mục này không còn</b>Chọn mục khác ở cây bên trái.</div>'; }
    function kv(rs){ return '<dl class="kv cu-kv">' + rs.map(function(r){ return '<dt>' + r[0] + '</dt><dd>' + r[1] + '</dd>'; }).join('') + '</dl>'; }

    /* ---- vùng giữa: nội dung, xem trước của mục đang chọn ---- */
    function center(){ var k = U.sel, V = V0(), rs;
      if (k === 'f'){ var nu = Object.keys(units(CU)).length, np = diff(dr, CU).length; return head('trion.tcui', 'Tệp tùy biến chính') + kv([['Đường dẫn', '<span class="mono">C:\\TrionCAD\\Support\\trion.tcui</span>'], ['Đang dùng', nu ? nu + ' tùy biến so với mặc định' : 'Giao diện mặc định của TrionCAD'], ['Chưa áp dụng', np ? np + ' thay đổi' : 'Không có'], ['Tệp phụ', parts(dr).length + ' tệp']]) + '<div class="hrow"><button class="btn sm" type="button" data-cu="exp">' + ic('saveas') + 'Xuất tệp .tcui…</button><button class="btn sm ghost" type="button" data-cu="folder">' + ic('folder') + 'Mở thư mục</button></div><div class="note">' + ic('info', 'xs') + '<span>Mọi tùy biến ghi vào trion.tcui. Tệp phụ (của plugin, của nhóm) gắn thêm, chỉ đọc. Phím tắt, viết tắt lệnh và workspace có trình riêng, CUI mở được từ đây.</span></div>'; }
      if (k === 'ws' || /^ws:/.test(k)){ var cur = (TC.WS && TC.WS.cur) || (WSB.filter(function(w){ return w[0] === A().ws; })[0] || WSB[0])[1];
        if (k === 'ws') return head('Workspace', wsNames().length + ' workspace', '<button class="btn sm" type="button" data-cu="wssave">' + ic('save') + 'Lưu workspace hiện tại…</button><button class="btn sm ghost" type="button" data-cu="wsset">' + ic('gear') + 'Thiết lập…</button>') + '<div class="tblwrap cu-it"><table class="tbl"><tbody>' + wsNames().map(function(n){ var b = WSB.filter(function(w){ return w[1] === n; })[0]; return '<tr data-tn="ws:' + esc(n) + '"><td>' + ic('ws') + ' ' + esc(n) + (n === cur ? ' <span class="tag ok">đang dùng</span>' : '') + '</td><td class="faint">' + (b ? 'có sẵn' : 'của bạn') + '</td></tr>'; }).join('') + '</tbody></table></div>';
        var n0 = k.slice(3), b0 = WSB.filter(function(w){ return w[1] === n0; })[0], u = ((TC.WS || {}).list || []).filter(function(x){ return x.name === n0; })[0], pn = function(l){ return (l || []).map(function(p){ return TC.PALETTES[p] ? TC.PALETTES[p].name : p; }).join(', ') || 'không'; };
        return head(n0, b0 ? 'Workspace có sẵn' : 'Workspace của bạn', n0 === cur ? '<span class="tag ok">đang dùng</span>' : '<button class="btn sm" type="button" data-cu="wsgo" data-ws="' + esc(n0) + '">Chuyển sang workspace này</button>') + kv(b0 ? [['Nội dung', esc(b0[2])]] : u ? [['Giao diện', u.base === 'classic' ? 'Thanh menu, toolbar' : 'Ribbon'], ['Palette phải', esc(pn(u.pal.right))], ['Palette trái', esc(pn(u.pal.left))], ['Tab tệp, layout', (u.ft ? 'hiện' : 'ẩn') + ', ' + (u.lt ? 'hiện' : 'ẩn')], ['Dòng lệnh', u.cmd ? 'hiện' : 'ẩn']] : []) + '<p class="cu-hint">' + ic('info', 's12') + 'Ribbon, menu, toolbar của mọi workspace lấy từ trion.tcui; workspace chỉ chọn cái nào hiện và vị trí palette.</p>'; }
      if (k === 'qat'){ rs = rows(k); return head('Thanh truy cập nhanh', 'Quick Access Toolbar · ' + rs.length + ' nút') + '<div class="cu-pvw cu-drop" data-drop="qat">' + pvIcons(rs, 'cu-qat') + '</div>' + sub('Nút', rs.length, itBtns()) + itTable(rs, 'src') + hint(); }
      if (k === 'rb') return head('Ribbon', V.length + ' tab', '<button class="btn sm" type="button" data-cu="newtab">' + ic('plus') + 'Tab mới…</button>') + '<div class="tblwrap cu-it"><table class="tbl"><tbody>' + V.map(function(t){ return '<tr data-tn="t:' + esc(t._tk) + '"' + (t._hide ? ' class="ro"' : '') + '><td>' + ic('layout') + ' ' + esc(t.name) + (t._cu ? ' <span class="tag acc">tùy biến</span>' : t._hide ? ' <span class="tag">ẩn</span>' : '') + '</td><td class="faint">' + t.panels.length + ' panel</td></tr>'; }).join('') + '</tbody></table></div>';
      if (/^t:/.test(k)){ var t = V.filter(function(x){ return 't:' + x._tk === k; })[0]; if (!t) return gone();
        return head(t.name, 'Tab ribbon · ' + t.panels.length + ' panel' + (t._hide ? ' · đang ẩn' : '') + (t._cu ? ' · tùy biến' : ''), '<button class="btn sm" type="button" data-cu="newpnl">' + ic('plus') + 'Panel mới…</button>') + '<div class="cu-pvw cu-pvtab">' + t.panels.map(function(p){ return '<div class="cu-pvp' + (p._hide ? ' is-hide' : '') + '" data-tn="p:' + esc(p._pk) + '" title="' + esc(p.name) + '">' + pvPanel(p) + '</div>'; }).join('') + '</div>' + sub('Panel', t.panels.length) + '<div class="tblwrap cu-it"><table class="tbl"><tbody>' + t.panels.map(function(p){ return '<tr data-tn="p:' + esc(p._pk) + '"' + (p._hide ? ' class="ro"' : '') + '><td>' + ic(p.icon || 'gridview') + ' ' + esc(p.name) + (p._cu ? ' <span class="tag acc">tùy biến</span>' : p._hide ? ' <span class="tag">ẩn</span>' : dr.mod[p._pk] ? ' <span class="tag info">đã đổi</span>' : '') + '</td><td class="faint">' + leaves(p.items).length + ' nút</td></tr>'; }).join('') + '</tbody></table></div><p class="cu-hint">' + ic('info', 's12') + 'Bấm vào một panel để sửa. Ribbon tự thu gọn panel theo ưu tiên khi màn hình hẹp.</p>'; }
      if (/^p:/.test(k)){ var vp = vPanel(k.slice(2)); if (!vp) return gone(); rs = rows(k);
        return head(vp.p.name, 'Panel ribbon · tab ' + esc(vp.t.name) + (vp.p._hide ? ' · đang ẩn' : '') + (vp.p._cu ? ' · tùy biến' : ''), vp.p._cu ? '' : '<span class="faint cu-note0">Nút có sẵn: đổi nhãn, biểu tượng</span>') + '<div class="cu-pvw cu-drop" data-drop="' + esc(k) + '">' + pvPanel(vp.p) + '</div>' + sub('Nút', rs.length, itBtns()) + itTable(rs, 'pnl') + hint(); }
      if (k === 'tbs' || k === 'mns'){ var isT = k === 'tbs', ls = isT ? tbList() : mnList(); return head(isT ? 'Toolbar' : 'Menu', 'Workspace Cổ điển · ' + ls.length, '<button class="btn sm" type="button" data-cu="' + (isT ? 'newtb' : 'newmn') + '">' + ic('plus') + (isT ? 'Toolbar mới…' : 'Menu mới…') + '</button>') + '<div class="tblwrap cu-it"><table class="tbl"><tbody>' + ls.map(function(x){ return '<tr data-tn="' + esc(x.k) + '"><td>' + ic(isT ? 'listview' : 'menu') + ' ' + esc(x.name) + (x.x ? ' <span class="tag acc">tùy biến</span>' : x.part ? ' <span class="tag">' + esc(x.part.name) + '</span>' : '') + '</td><td class="faint">' + ((rows(x.k) || []).length || (isT ? 'danh sách thả xuống' : '')) + (rows(x.k) && rows(x.k).length ? ' mục' : '') + '</td></tr>'; }).join('') + '</tbody></table></div>' + (isT ? '<p class="cu-hint">' + ic('info', 's12') + 'Toolbar mới hiện trong workspace Cổ điển; bật, tắt bằng chuột phải trên vùng toolbar.</p>' : ''); }
      if (/^tb:/.test(k)){ var tb = tbList().filter(function(x){ return x.k === k; })[0]; if (!tb) return gone(); rs = rows(k) || [];
        return head(tb.name, 'Toolbar' + (tb.x ? ' tùy biến' : tb.part ? ' của ' + esc(tb.part.name) + ' (chỉ đọc)' : ' có sẵn (chỉ đọc)'), tb.x ? '' : '<button class="btn sm" type="button" data-cu="copytb">' + ic('copy') + 'Sao chép thành toolbar mới</button>') + '<div class="cu-pvw' + (tb.x ? ' cu-drop" data-drop="' + esc(k) : '') + '">' + (tb.items || tb.x || tb.pt ? pvIcons(rs, 'tbar cu-tbar') : '<span class="faint">Toolbar gồm các danh sách thả xuống (kiểu, layer, thuộc tính).</span>') + '</div>' + sub('Nút', rs.length, tb.x ? itBtns() : '') + itTable(rs, 'src') + (tb.x ? hint() : ''); }
      if (/^mn:/.test(k)){ var mn = mnList().filter(function(x){ return x.k === k; })[0]; if (!mn) return gone(); rs = rows(k) || [];
        return head(mn.name, 'Menu' + (mn.x ? ' tùy biến' : mn.part ? ' của ' + esc(mn.part.name) + ' (chỉ đọc)' : ' có sẵn: thêm mục vào cuối menu')) + '<div class="cu-pvw' + (mn.pt ? '' : ' cu-drop" data-drop="' + esc(k)) + '">' + pvMenu(rs, mn.name) + '</div>' + sub('Mục', rs.length, mn.pt ? '' : itBtns()) + itTable(rs, 'src') + (mn.pt ? '' : hint()); }
      if (k === 'ctx') return head('Menu chuột phải', 'Theo ngữ cảnh trên vùng vẽ (07.1 §9.9)') + '<div class="tblwrap cu-it"><table class="tbl"><tbody>' + CTXS.map(function(c){ return '<tr data-tn="ctx:' + c[0] + '"><td>' + ic('menu') + ' ' + esc(c[1]) + '</td><td class="faint">' + (c[0] === 'sel' ? 'thêm được lệnh' : 'cố định') + '</td></tr>'; }).join('') + '</tbody></table></div>';
      if (/^ctx:/.test(k)){ var cx = CTXS.filter(function(x){ return 'ctx:' + x[0] === k; })[0]; if (!cx) return gone(); rs = rows(k);
        return head(cx[1], 'Menu chuột phải' + (k === 'ctx:sel' ? ' · mục thêm hiện ở cuối menu' : ' · cố định')) + '<div class="cu-pvw' + (k === 'ctx:sel' ? ' cu-drop" data-drop="ctx:sel' : '') + '">' + pvMenu(rs, cx[1]) + '</div>' + (k === 'ctx:sel' ? sub('Mục thêm', dr.ctx.length, itBtns()) + itTable(rs.filter(function(r){ return r.cu; }), 'src') + hint() : '<p class="cu-hint">' + ic('info', 's12') + 'Ngữ cảnh này giữ cố định để thao tác quen tay. Thêm lệnh vào ngữ cảnh “Có vùng chọn”.</p>'); }
      if (k === 'keys'){ var kr = SC.keyRows ? SC.keyRows() : null; return head('Phím tắt', 'Một bảng dùng chung cho tooltip, menu và bảng in', '<button class="btn sm" type="button" data-cu="keys">' + ic('keyboard') + 'Mở trình Phím tắt…</button>') + (kr ? '<div class="tblwrap cu-it"><table class="tbl"><colgroup><col><col style="width:140px"><col style="width:150px"></colgroup><thead><tr><th>Lệnh hoặc hành động</th><th>Tên lệnh</th><th>Phím tắt</th></tr></thead><tbody>' + kr.map(function(r){ return '<tr data-key="' + esc(r.id) + '"><td>' + esc(r.name) + '</td><td class="mono faint">' + esc(r.cmd) + '</td><td>' + String(r.keys).split(' · ').map(function(x){ return '<kbd class="kc">' + esc(TC.keyLabel(x)) + '</kbd>'; }).join(' ') + '</td></tr>'; }).join('') + '</tbody></table></div>' : '') + '<div class="note">' + ic('info', 'xs') + '<span>Gán, gỡ phím ở trình Phím tắt (UI-K06): bấm tổ hợp phím để gán, báo trùng, nhập, xuất, in bảng. CUI và trình Phím tắt dùng chung một bảng.</span></div>'; }
      if (k === 'dbl') return head('Hành động nhấp đúp', 'Theo loại đối tượng (07.1 §9.10)') + '<div class="tblwrap cu-it"><table class="tbl"><colgroup><col style="width:40%"><col><col style="width:190px"></colgroup><thead><tr><th>Đối tượng</th><th>Mặc định</th><th>Khi nhấp đúp</th></tr></thead><tbody>' + DBL.map(function(r){ var v = dr.dbl[r[0]] || ''; return '<tr><td>' + esc(r[1]) + '</td><td class="faint">' + esc(r[2]) + '</td><td><select class="inp sm" data-dbl="' + r[0] + '" aria-label="Nhấp đúp vào ' + esc(r[1]) + '">' + DBLO.map(function(o){ return '<option value="' + o[0] + '"' + (o[0] === v ? ' selected' : '') + '>' + esc(o[1]) + '</option>'; }).join('') + '</select></td></tr>'; }).join('') + '</tbody></table></div><p class="cu-hint">' + ic('info', 's12') + 'Nhấp đúp vào vùng trống, viewport, xref giữ như mặc định.</p>';
      if (k === 'mouse') return head('Nút chuột', 'Trên vùng vẽ') + '<div class="tblwrap cu-it"><table class="tbl"><colgroup><col style="width:46%"><col></colgroup><thead><tr><th>Thao tác</th><th>Hành động</th></tr></thead><tbody>' + MOUSE.map(function(r){ var v = dr.mouse[r[0]] || ''; return '<tr><td>' + esc(r[1]) + '</td><td><select class="inp sm" data-mo="' + r[0] + '" aria-label="' + esc(r[1]) + '">' + r[2].map(function(o){ return '<option value="' + o[0] + '"' + (o[0] === v ? ' selected' : '') + '>' + esc(o[1]) + '</option>'; }).join('') + '</select></td></tr>'; }).join('') + MFIX.map(function(r){ return '<tr class="ro"><td>' + esc(r[0]) + '</td><td class="faint">' + esc(r[1]) + ' · cố định</td></tr>'; }).join('') + '</tbody></table></div><p class="cu-hint">' + ic('info', 's12') + 'Chuột phải theo thời gian (bấm nhanh là Enter, giữ lâu là menu) đặt ở Tùy chọn › Người dùng.</p>';
      if (k === 'lisp') return head('Tệp LISP đi kèm', 'Nạp cùng trion.tcui khi dùng LISP lần đầu', '<button class="btn sm" type="button" data-cu="lispadd" aria-haspopup="menu">' + ic('plus') + 'Thêm tệp LISP…</button>') + (dr.lisp.length ? '<div class="tblwrap cu-it"><table class="tbl"><colgroup><col><col style="width:200px"><col style="width:110px"><col style="width:58px"></colgroup><tbody>' + dr.lisp.map(function(n, i){ var f = findF(n); return '<tr><td>' + ic('lisp') + ' ' + esc(n) + '</td><td class="mono faint" style="font-size:var(--fs-xs)">' + esc(f ? f.dir : '') + '</td><td>' + (L.loaded[n] ? '<span class="tag ok">đã nạp</span>' : '<span class="tag">chưa nạp</span>') + '</td><td class="ctr"><button class="ib sm" type="button" data-cu="lispdel" data-i="' + i + '" aria-label="Bỏ ' + esc(n) + '" data-tip="Bỏ khỏi danh sách">' + ic('x') + '</button></td></tr>'; }).join('') + '</tbody></table></div>' : '<div class="empty cu-empty">' + ic('lisp') + '<b>Chưa có tệp LISP đi kèm</b>Thêm tệp chứa hàm mà macro gọi, ví dụ (c:DANHSO) cần danhso.lsp.</div>') + '<div class="note">' + ic('info', 'xs') + '<span>Tệp ngoài thư mục tin cậy vẫn hỏi trước khi nạp (Tùy chọn bảo mật).</span></div>';
      if (k === 'part' || /^part:/.test(k)){ var ps = parts(dr);
        if (k === 'part') return head('Tệp tùy biến phụ', ps.length + ' tệp gắn thêm, chỉ đọc', '<button class="btn sm" type="button" data-cu="partadd" aria-haspopup="menu">' + ic('plus') + 'Gắn tệp…</button>') + (ps.length ? '<div class="tblwrap cu-it"><table class="tbl"><tbody>' + ps.map(function(p){ return '<tr data-tn="part:' + esc(p.id) + '"><td>' + ic('file') + ' ' + esc(p.name) + '</td><td class="faint">' + (p.plug ? 'của plugin ' + esc(p.plug) : 'gắn thêm') + '</td></tr>'; }).join('') + '</tbody></table></div>' : '<div class="empty cu-empty">' + ic('file') + '<b>Chưa gắn tệp phụ</b>Tệp phụ thêm toolbar, menu, lệnh dùng chung của nhóm hoặc của plugin.</div>');
        var p = ps.filter(function(x){ return 'part:' + x.id === k; })[0]; if (!p) return gone();
        return head(p.name, p.plug ? 'Tệp phụ của plugin ' + esc(p.plug) + ' · chỉ đọc' : 'Tệp phụ gắn thêm · chỉ đọc', p.plug ? '' : '<button class="btn sm" type="button" data-cu="partdel" data-id="' + esc(p.id) + '">' + ic('x') + 'Gỡ tệp</button>') + kv([['Đường dẫn', '<span class="mono">' + esc(p.path) + '</span>'], ['Toolbar', esc((p.tb || []).map(function(t){ return t.name; }).join(', ') || 'không')], ['Menu', esc((p.menus || []).map(function(m){ return m.name; }).join(', ') || 'không')], ['Lệnh', esc((p.cmds || []).map(function(c){ return c.name; }).join(', ') || 'không')]]) + '<p class="cu-hint">' + ic('info', 's12') + (p.plug ? 'Tắt plugin thì tệp này cũng gỡ khỏi giao diện.' : 'Muốn sửa thì chép mục sang trion.tcui ở tab Chuyển.') + '</p>'; }
      return gone(); }

    /* ---- khung thuộc tính ---- */
    function pr(lab, ctl, h){ return '<div class="cu-pr" data-h="' + esc(h || '') + '"><span class="k">' + lab + '</span><span class="v">' + ctl + '</span></div>'; }
    function ttl(t, s){ return '<div class="cu-pt"><b>' + esc(t) + '</b><span class="faint">' + s + '</span></div>'; }
    function icb(name, act){ return '<button class="btn sm cu-icb" type="button" data-cu="' + act + '">' + ic(name || 'play') + '<span>' + esc(name || 'play') + '</span><span class="faint">Đổi…</span></button>'; }
    function keyOf(n){ var kr = SC.keyRows ? SC.keyRows().filter(function(r){ return r.cmd === n; })[0] : null; return kr ? kr.keys : ''; }
    function itemProps(r){ var c = r.cmd ? ci(r.cmd) : null, h;
      if (r.ro) return ttl(r.label, r.ro === 'mặc định' ? 'Nút mặc định' : 'Mục có sẵn' + (r.ro !== 'có sẵn' ? ' của ' + esc(r.ro) : '')) + (r.cmd ? pr('Lệnh', '<span class="mono">' + esc(r.cmd) + '</span>') : '') + '<p class="cu-note">' + ic('info', 's12') + (r.ro === 'mặc định' ? 'Nút mặc định của thanh truy cập nhanh; ẩn, hiện bằng menu ▾ cuối thanh.' : 'Chỉ xem. Muốn khác thì thêm lệnh vào panel, toolbar, menu của bạn.') + '</p>';
      var ed = !r.bi || /^(lg|s|io)$/.test(r.t), lab = r.bi ? r.bi.label : r.cu ? r.cu.label || '' : r.label;
      h = ttl(String(r.label), r.bi ? 'Nút có sẵn' + (dr.mod[r.key] ? ' · đã đổi' : '') : r.q ? 'Nút thêm vào thanh truy cập nhanh' : 'Mục tùy biến');
      if (!r.q && ed) h += pr('Nhãn', r.t === 'lg' ? '<textarea class="inp sm" data-pf="label" rows="2" spellcheck="false" placeholder="' + esc(c ? c.desc : '') + '">' + esc(lab) + '</textarea>' : '<input class="inp sm" data-pf="label" value="' + esc(lab) + '" spellcheck="false" placeholder="' + esc(c ? c.desc : '') + '">', 'Chữ trên nút hoặc mục menu. Nút lớn: Enter để xuống dòng. Để trống thì dùng tên lệnh.');
      if (!r.q && ed && !/^mn:|^ctx:/.test(U.sel)) h += pr('Biểu tượng', icb(r.icon, 'icon'), 'Chọn trong bộ biểu tượng TrionCAD (lưới 16 px, có bản 32 px cho nút lớn).');
      if (r.cu && /^p:/.test(U.sel)) h += pr('Cỡ nút', '<span class="seg">' + [['lg', 'Lớn'], ['s', 'Nhỏ'], ['io', 'Biểu tượng']].map(function(o){ return '<button type="button" data-pt="' + o[0] + '" aria-pressed="' + ((r.t || 's') === o[0]) + '">' + o[1] + '</button>'; }).join('') + '</span>', 'Nút lớn đứng riêng một cột; nút nhỏ và nút chỉ có biểu tượng xếp ba nút một cột.');
      if (r.bi && !ed) h += pr('Loại', esc({ dd:'Danh sách thả xuống', num:'Ô nhập số', gal:'Gallery' }[r.t] || r.t), 'Điều khiển đặc biệt, chỉ xem.');
      h += pr('Lệnh', r.cmd ? '<span class="mono">' + esc(r.cmd) + '</span>' + (c && c.cu ? '<span class="tag acc">tùy biến</span>' : '') : '<span class="faint">—</span>', 'Lệnh chạy khi bấm. Đổi lệnh: bỏ mục này rồi kéo lệnh khác vào.');
      if (c && r.cmd) h += pr('Tooltip', esc(c.desc) + (keyOf(r.cmd) ? ' · <kbd class="kc">' + esc(TC.keyLabel(keyOf(r.cmd))) + '</kbd>' : ''), 'Tooltip mức 1: tên, tên lệnh tiếng Anh, phím tắt. Mức 2 lấy mô tả của lệnh.');
      if (r.bi && dr.mod[r.key]) h += '<div class="hrow"><button class="btn sm" type="button" data-cu="resetit">' + ic('undo') + 'Đặt lại nút này</button></div>';
      if (r.cu || r.q) h += '<div class="hrow">' + itBtns() + '</div>';
      return h; }
    function nodeProps(k){
      if (/^p:/.test(k)){ var vp = vPanel(k.slice(2)); if (!vp) return ''; var p = vp.p, x = p._cu, pk = p._pk, m = dr.mod[pk] || {};
        return ttl(p.name, x ? 'Panel tùy biến' : 'Panel có sẵn' + (dr.mod[pk] ? ' · đã đổi' : '')) + pr('Tên panel', '<input class="inp sm" data-pf="pname" value="' + esc(p.name) + '" spellcheck="false">', 'Tên dưới panel và trong menu khi panel thu gọn.') +
          (x ? '' : pr('Hiện panel', H.chk('', 'Hiện trong tab ' + esc(vp.t.name), !p._hide).replace('<input type="checkbox"', '<input type="checkbox" data-pf="phide"'), 'Ẩn panel không dùng để ribbon gọn hơn. Khôi phục mặc định để hiện lại tất cả.')) +
          pr('Biểu tượng', icb(p.icon, 'picon'), 'Biểu tượng khi panel thu gọn thành một nút ở màn hình hẹp.') + pr('Ưu tiên thu gọn', '<select class="inp sm" data-pf="pprio">' + [1, 2, 3, 4, 5].map(function(n){ return '<option value="' + n + '"' + (n === (p.prio || 1) ? ' selected' : '') + '>' + n + (n === 1 ? ' · thu gọn sau cùng' : n === 5 ? ' · thu gọn trước' : '') + '</option>'; }).join('') + '</select>', 'Màn hình hẹp: panel ưu tiên lớn thu gọn thành một nút trước.') +
          (x ? pr('Tab', '<select class="inp sm" data-pf="ptab">' + grab().tabs.map(function(t){ return '<option' + (t.name === x.tab ? ' selected' : '') + '>' + esc(t.name) + '</option>'; }).join('') + '</select>', 'Chuyển panel sang tab khác.') : '') +
          pr('Số nút', String(leaves(p.items).length)) + '<div class="hrow">' + (x ? '<button class="btn sm" type="button" data-cu="delpnl">' + ic('trash') + 'Xóa panel</button>' : m.label || m.icon || m.prio || m.hide || (dr.add[pk] || []).length ? '<button class="btn sm" type="button" data-cu="resetp">' + ic('undo') + 'Đặt lại panel</button>' : '') + '</div>'; }
      if (/^t:/.test(k)){ var t = V0().filter(function(y){ return 't:' + y._tk === k; })[0]; if (!t) return '';
        return ttl(t.name, t._cu ? 'Tab tùy biến' : 'Tab có sẵn') + pr('Tên tab', '<input class="inp sm" data-pf="tname" value="' + esc(t.name) + '" spellcheck="false">', 'Tên tab trên ribbon.') + (t._cu ? '' : pr('Hiện tab', H.chk('', 'Hiện trên ribbon', !t._hide).replace('<input type="checkbox"', '<input type="checkbox" data-pf="thide"'), 'Tab ẩn không hiện trên ribbon; lệnh vẫn gõ được.')) + pr('Số panel', String(t.panels.length)) + '<div class="hrow"><button class="btn sm" type="button" data-cu="newpnl">' + ic('plus') + 'Panel mới…</button>' + (t._cu ? '<button class="btn sm" type="button" data-cu="deltab">' + ic('trash') + 'Xóa tab</button>' : '') + '</div>'; }
      if (/^tb:cu:/.test(k) || /^mn:cu:/.test(k)){ var isT = k[0] === 't', o = (isT ? dr.tb : dr.menus).filter(function(y){ return y.uid === k.slice(6); })[0]; if (!o) return '';
        return ttl(o.name, isT ? 'Toolbar tùy biến' : 'Menu tùy biến') + pr('Tên', '<input class="inp sm" data-pf="oname" value="' + esc(o.name) + '" spellcheck="false">', isT ? 'Tên trong menu bật, tắt toolbar.' : 'Tên trên thanh menu.') + (isT ? pr('Vị trí', '<select class="inp sm" data-pf="tpos">' + [['1', 'Trên, dưới thanh menu'], ['L', 'Dọc bên trái vùng vẽ'], ['R', 'Dọc bên phải vùng vẽ']].map(function(q){ return '<option value="' + q[0] + '"' + (String(o.pos || 1) === q[0] ? ' selected' : '') + '>' + q[1] + '</option>'; }).join('') + '</select>', 'Vị trí mặc định khi bật toolbar.') : '') + pr('Số mục', String(o.items.length)) + '<div class="hrow"><button class="btn sm" type="button" data-cu="delobj">' + ic('trash') + (isT ? 'Xóa toolbar' : 'Xóa menu') + '</button></div>'; }
      return ttl(nodes().filter(function(n){ return n.k === k; }).map(function(n){ return n.label; })[0] || '', 'Chọn một mục ở cây hoặc trong vùng xem trước để sửa thuộc tính.'); }
    function cmdProps(n){ var x = dr.cmds.filter(function(c){ return c.name === n; })[0], c = ci(n), pt = !x && allCmds().filter(function(a){ return a.name === n && a.pt; })[0], used = usedIn(n), h, ck, kc = cont(U.sel);
      if (x){ ck = macroCheck(x.macro); h = ttl(x.label, 'Lệnh tùy biến' + (x.src ? ' · chép từ ' + esc(x.src) : '') + (REG[x.name] ? '' : ' · chưa áp dụng'));
        h += pr('Tên hiển thị', '<input class="inp sm" data-pf="clabel" value="' + esc(x.label) + '" spellcheck="false">', 'Tên trên nút, trong menu và Danh sách lệnh.') + pr('Tên lệnh', '<input class="inp sm mono" data-pf="cname" value="' + esc(x.name) + '" spellcheck="false" aria-describedby="cu-cnm">', 'Tên tiếng Anh viết hoa, gõ được trên dòng lệnh như lệnh thường.') + '<span class="fmsg" id="cu-cnm"></span>' +
          pr('Macro', '<textarea class="inp sm mono" data-pf="cmacro" rows="3" spellcheck="false" aria-describedby="cu-mm">' + esc(x.macro) + '</textarea>', 'Như AutoCAD: ^C^C hủy lệnh đang chạy, dấu ; hoặc dấu cách là Enter, \\ chờ người dùng nhập. Biểu thức (…) chạy LISP, ví dụ ^C^C(c:DANHSO).') + '<span class="fmsg ' + (ck[0] === 'err' ? 'err' : ck[0] === 'warn' ? 'warn' : 'hint') + ' cu-mm" id="cu-mm">' + (ck[0] !== 'hint' ? ic('warn', 'xs') : '') + esc(ck[1]) + '</span>' +
          pr('Biểu tượng', icb(x.icon, 'cicon'), 'Biểu tượng trên nút, trong menu, Danh sách lệnh.') + pr('Tooltip', '<input class="inp sm" data-pf="ctip" value="' + esc(x.tip || '') + '" spellcheck="false" placeholder="' + esc(x.label) + '">', 'Tooltip mức 1 khi rê chuột lên nút.') + pr('Mô tả', '<textarea class="inp sm" data-pf="cdesc" rows="2" spellcheck="false" placeholder="Mô tả dài, hiện ở tooltip mức 2">' + esc(x.desc || '') + '</textarea>', 'Tooltip mức 2 (rê lâu hơn 1,5 giây).'); }
      else { h = ttl(c.desc, pt ? 'Lệnh của ' + esc(pt.pt) + ' (chỉ đọc)' : c.lisp ? 'Lệnh LISP' : c.plug ? 'Lệnh của plugin' : 'Lệnh có sẵn');
        h += pr('Tên lệnh', '<span class="mono">' + esc(c.name) + '</span>') + (c.aliases && c.aliases.filter(function(a){ return a !== c.name; }).length ? pr('Viết tắt', '<span class="mono">' + esc(c.aliases.filter(function(a){ return a !== c.name; }).join(', ')) + '</span>') : '') + pr('Macro', '<span class="mono">' + esc(pt ? pt.cu.macro : '^C^C_' + c.name) + '</span>') + pr('Biểu tượng', ic(c.icon || 'play') + '<span class="mono faint">' + esc(c.icon || 'play') + '</span>') + (c.where ? pr('Vị trí', esc(c.where)) : '') + (keyOf(c.name) ? pr('Phím tắt', '<kbd class="kc">' + esc(TC.keyLabel(keyOf(c.name))) + '</kbd>') : ''); }
      h += pr('Dùng ở', used.length ? '<span class="cu-chips">' + used.map(function(u){ return '<button class="chip" type="button" data-chip="' + esc(u[0]) + '">' + esc(u[1]) + '</button>'; }).join('') + '</span>' : '<span class="faint">Chưa dùng ở đâu</span>', 'Bấm để mở chỗ đang dùng lệnh.');
      h += '<div class="hrow cu-acts">' + (kc ? '<button class="btn sm primary" type="button" data-cu="addsel">' + ic('plus') + 'Thêm vào ' + esc(cname(U.sel)) + '</button>' : '') + (x ? '<button class="btn sm" type="button" data-cu="delcmd">' + ic('trash') + 'Xóa lệnh</button>' : pt ? '' : '<button class="btn sm" type="button" data-cu="dupcmd">' + ic('copy') + 'Tạo lệnh tùy biến từ lệnh này</button>') + '</div>';
      return h; }
    function props(){ if (U.foc === 'cmd' && U.cmd && (cinfo(U.cmd) || TC.cmd.def(U.cmd))) return cmdProps(U.cmd); var r = selRow(); return r ? itemProps(r) : nodeProps(U.sel); }
    var HELP = { f:'trion.tcui: tệp tùy biến chính. Chọn một mục ở cây để xem và sửa.', qat:'Nút thêm vào thanh truy cập nhanh hiện sau nút In, ở mọi workspace có ribbon.', rb:'Ribbon: tab, panel, nút. Thêm panel “Của tôi” để gom lệnh hay dùng.', keys:'Phím tắt: một bảng dùng chung với trình Phím tắt.', dbl:'Đổi hành động khi nhấp đúp vào từng loại đối tượng.', mouse:'Đổi hành động nút chuột trên vùng vẽ.', lisp:'Tệp LISP nạp cùng tệp tùy biến, để macro gọi được hàm trong tệp.', part:'Tệp tùy biến phụ: thêm toolbar, menu, lệnh dùng chung; chỉ đọc.' };
    function help(){ var k = U.sel; return U.help || (U.foc === 'cmd' ? 'Kéo lệnh vào panel, toolbar, menu; hoặc nhấp đúp để thêm vào mục đang chọn ở cây.' : HELP[k] || (/^p:/.test(k) ? 'Panel ribbon: kéo lệnh vào để thêm nút; bấm một nút ở vùng xem trước để đổi nhãn, biểu tượng.' : /^t:/.test(k) ? 'Tab ribbon: đổi tên, ẩn tab, thêm panel.' : /^(tb|mn):/.test(k) ? 'Toolbar, menu của workspace Cổ điển.' : /^ctx/.test(k) ? 'Menu chuột phải trên vùng vẽ.' : /^ws/.test(k) ? 'Workspace chọn phần nào của giao diện hiện ra.' : '')); }

    /* ---- tab Chuyển: chép mục từ tệp khác (CUIX của AutoCAD, tệp dùng chung) sang trion.tcui ---- */
    function xNeed(){ var s = XSRC[U.xs], need = {}; s.tabs.forEach(function(t, i){ if (U.xc['tab:' + i]) t.panels.forEach(function(p){ p.items.forEach(function(c){ need[c[0]] = 1; }); }); }); s.tb.forEach(function(t, i){ if (U.xc['tb:' + i]) t.items.forEach(function(n){ need[n] = 1; }); }); s.menus.forEach(function(m, i){ if (U.xc['mn:' + i]) m.items.forEach(function(n){ need[n] = 1; }); }); return need; }
    function xList(){ var s = XSRC[U.xs], need = xNeed(); return Object.keys(U.xc).filter(function(k){ return U.xc[k] && !/^cmd:/.test(k); }).concat(s.cmds.filter(function(c){ return U.xc['cmd:' + c.name] || need[c.name]; }).map(function(c){ return 'cmd:' + c.name; })); }
    function rX(){ var s = XSRC[U.xs], need = xNeed(), n = xList().length, chk = function(k, on, dis){ return '<input type="checkbox" data-xk="' + k + '"' + (on ? ' checked' : '') + (dis ? ' disabled' : '') + '>'; }, mine = dr.tabs.length + dr.tb.length + dr.menus.length + dr.cmds.length;
      $('.cux', el).innerHTML = '<div class="cux-c"><div class="cu-h"><b>trion.tcui</b><span class="faint">tệp chính · đích</span></div><div class="cux-l">' +
          '<div class="cux-g">Tab ribbon</div>' + V0().map(function(t){ return '<div class="cux-i' + (t._cu ? ' cu' : '') + '">' + ic('layout', 's12') + esc(t.name) + (U.nk['t:' + t._tk] ? '<span class="tag ok">mới</span>' : t._cu ? '<span class="tag acc">tùy biến</span>' : '') + '</div>'; }).join('') +
          '<div class="cux-g">Toolbar, menu tùy biến</div>' + (dr.tb.length + dr.menus.length ? dr.tb.map(function(t){ return '<div class="cux-i cu">' + ic('listview', 's12') + esc(t.name) + (U.nk['tb:cu:' + t.uid] ? '<span class="tag ok">mới</span>' : '') + '</div>'; }).join('') + dr.menus.map(function(m){ return '<div class="cux-i cu">' + ic('menu', 's12') + esc(m.name) + (U.nk['mn:cu:' + m.uid] ? '<span class="tag ok">mới</span>' : '') + '</div>'; }).join('') : '<div class="cux-i faint">Chưa có</div>') +
          '<div class="cux-g">Lệnh tùy biến · ' + dr.cmds.length + '</div>' + (dr.cmds.length ? dr.cmds.map(function(c){ return '<div class="cux-i cu">' + ic(c.icon || 'play', 's12') + esc(c.label) + '<span class="mono faint">' + esc(c.name) + '</span></div>'; }).join('') : '<div class="cux-i faint">Chưa có</div>') + '</div><div class="cux-f faint">' + mine + ' mục tùy biến · chưa áp dụng cho tới khi bấm Áp dụng</div></div>' +
        '<div class="cux-m"><button class="btn primary" type="button" data-cu="xcopy"' + (n ? '' : ' disabled') + '>' + ic('arrowL') + 'Chép ' + n + ' mục</button><span class="faint">sang tệp chính</span></div>' +
        '<div class="cux-c"><div class="cu-h"><select class="inp" id="cux-s" aria-label="Tệp nguồn">' + Object.keys(XSRC).map(function(k){ return '<option value="' + k + '"' + (k === U.xs ? ' selected' : '') + '>' + esc(XSRC[k].name + ' · ' + XSRC[k].note) + '</option>'; }).join('') + '</select><button class="btn sm ghost" type="button" data-cu="xopen">' + ic('folder') + 'Mở tệp…</button></div><div class="cux-p mono faint" title="' + esc(s.path) + '">' + esc(s.path) + '</div>' +
          (U.xmsg ? '<div class="note acc">' + ic('okc', 'xs') + '<span>' + U.xmsg + '</span></div>' : U.xs === 'cuix' ? '<div class="note">' + ic('info', 'xs') + '<span>Đọc được tab ribbon, panel, toolbar, menu và macro. Tab ngữ cảnh, tool palette, menu ảnh, macro DIESEL phức tạp chưa chuyển được.</span></div>' : '') +
          '<div class="cux-l">' + (s.tabs.length ? '<div class="cux-g">Tab ribbon</div>' + s.tabs.map(function(t, i){ return '<label class="cux-i chk">' + chk('tab:' + i, U.xc['tab:' + i]) + ic('layout', 's12') + '<b>' + esc(t.name) + '</b><span class="faint">' + t.panels.map(function(p){ return esc(p.name); }).join(', ') + '</span></label>'; }).join('') : '') +
            '<div class="cux-g">Toolbar</div>' + s.tb.map(function(t, i){ return '<label class="cux-i chk">' + chk('tb:' + i, U.xc['tb:' + i]) + ic('listview', 's12') + esc(t.name) + '<span class="faint">' + t.items.length + ' nút</span></label>'; }).join('') +
            '<div class="cux-g">Menu</div>' + s.menus.map(function(m, i){ return '<label class="cux-i chk">' + chk('mn:' + i, U.xc['mn:' + i]) + ic('menu', 's12') + esc(m.name) + '<span class="faint">' + m.items.length + ' mục</span></label>'; }).join('') +
            '<div class="cux-g">Lệnh, macro · ' + s.cmds.length + '</div>' + s.cmds.map(function(c){ var w = macroCheck(c.macro), dup = dr.cmds.some(function(x){ return x.name === c.name; }); return '<label class="cux-i chk">' + chk('cmd:' + c.name, U.xc['cmd:' + c.name] || need[c.name], need[c.name] || dup) + ic(c.icon || 'play', 's12') + esc(c.label) + '<span class="mono faint">' + esc(c.name) + '</span>' + (dup ? '<span class="tag">đã có</span>' : need[c.name] ? '<span class="tag">đi kèm</span>' : '') + (w[0] === 'warn' && /DIESEL/.test(w[1]) ? '<span class="tag warn">' + ic('warn') + 'DIESEL</span>' : '') + '</label>'; }).join('') +
            (s.skip.length ? '<div class="cux-g">Không chuyển được · ' + s.skip.length + '</div>' + s.skip.map(function(x){ return '<div class="cux-i dis">' + ic('x', 's12') + '<span>' + esc(x[0]) + '</span><span class="faint">' + esc(x[1]) + '</span></div>'; }).join('') : '') + '</div>' +
          '<div class="hrow cux-f"><button class="btn sm ghost" type="button" data-cu="xall">Chọn tất cả mục chuyển được</button><button class="btn sm ghost" type="button" data-cu="xnone"' + (n ? '' : ' disabled') + '>Bỏ chọn</button></div></div>'; }
    function xAll(){ var s = XSRC[U.xs]; U.xc = {}; s.tabs.forEach(function(t, i){ U.xc['tab:' + i] = 1; }); s.tb.forEach(function(t, i){ U.xc['tb:' + i] = 1; }); s.menus.forEach(function(m, i){ U.xc['mn:' + i] = 1; }); s.cmds.forEach(function(c){ U.xc['cmd:' + c.name] = 1; }); }
    function uniq(name, taken){ var n = name, i = 2; while (taken.indexOf(n) >= 0) n = name + ' (' + i++ + ')'; return n; }
    function xCopy(){ var s = XSRC[U.xs], need = xNeed(), c = { tab:0, tb:0, mn:0, cmd:0, dup:0, die:0 }, it = function(n, t, l){ var o = { uid:uid(), cmd:n }; if (t) o.t = t; if (l) o.label = l; return o; };
      s.tabs.forEach(function(t, i){ if (!U.xc['tab:' + i]) return; var x = { uid:uid(), name:uniq(t.name, V0().map(function(v){ return v.name; })), panels:t.panels.map(function(p){ return { uid:uid(), name:p.name, icon:p.icon, items:p.items.map(function(q){ return it(q[0], q[1] || 's', q[2]); }) }; }) }; dr.tabs.push(x); U.nk['t:cu:' + x.uid] = 1; c.tab++; });
      s.tb.forEach(function(t, i){ if (!U.xc['tb:' + i]) return; var x = { uid:uid(), name:uniq(t.name, tbList().map(function(v){ return v.name; })), pos:1, items:t.items.map(function(n){ return it(n); }) }; dr.tb.push(x); U.nk['tb:cu:' + x.uid] = 1; c.tb++; });
      s.menus.forEach(function(m, i){ if (!U.xc['mn:' + i]) return; var x = { uid:uid(), name:uniq(m.name, mnList().map(function(v){ return v.name; })), items:m.items.map(function(n){ return it(n); }) }; dr.menus.push(x); U.nk['mn:cu:' + x.uid] = 1; c.mn++; });
      s.cmds.forEach(function(q){ if (!(U.xc['cmd:' + q.name] || need[q.name])) return; if (dr.cmds.some(function(x){ return x.name === q.name; }) || (TC.CMDS[q.name] && !TC.CMDS[q.name].cu)){ c.dup++; return; } dr.cmds.push(Object.assign(clone(q), { uid:uid(), src:s.name })); c.cmd++; if (/DIESEL/.test(macroCheck(q.macro)[1])) c.die++; });
      U.xc = {}; U.open.rb = U.open.tbs = U.open.mns = 1;
      U.xmsg = 'Đã chép ' + [c.tab && c.tab + ' tab', c.tb && c.tb + ' toolbar', c.mn && c.mn + ' menu', c.cmd && c.cmd + ' lệnh'].filter(Boolean).join(', ') + ' vào trion.tcui.' + (c.dup ? ' ' + c.dup + ' lệnh đã có, giữ bản hiện tại.' : '') + (c.die ? ' ' + c.die + ' lệnh dùng DIESEL cần sửa macro (đánh dấu ' + ic('warn', 's12') + ' ở Danh sách lệnh).' : '') + ' Bấm Áp dụng để dùng.';
      render(); }

    /* ---- vẽ lại ---- */
    function rFoot(){ var n = diff(dr, CU).length, ok = $('#cu-ok', el), nb = $('#cu-n', el); ok.disabled = !n; ok.textContent = n ? 'Áp dụng ' + n + ' thay đổi' : 'Áp dụng'; nb.hidden = !n; nb.innerHTML = n ? ic('info', 's12') + n + ' thay đổi chưa áp dụng' + ic('down', 'xs') : ''; }
    function live2(){ rTree(); rCmds(); $('#cu-c', el).innerHTML = center(); rFoot(); }
    function render(){ live2(); $('#cu-p', el).innerHTML = props(); $('#cu-d', el).textContent = help(); if (!$('.cux', el).hidden) rX(); $$('.cu-pvw .is-sel, .cu-pvw .is-hot, .cu-it tr.is-sel', el).forEach(function(x){ x.scrollIntoView({ block:'nearest' }); }); }
    function human(k){ var t = k.split(':')[0], id = k.slice(t.length + 1), nm = function(l){ var x = dr[l].concat(CU[l]).filter(function(y){ return y.uid === id; })[0]; return x ? ' “' + (x.name || x.label) + '”' : ''; };
      if (t === 'mod'){ var s = id.split('/'); return s.length === 1 ? 'Tab “' + s[0] + '”' : s.length === 2 ? 'Panel “' + s[1] + '” · ' + s[0] : 'Nút ' + s.slice(2).join('/') + ' · ' + s[0] + ' › ' + s[1]; }
      return { tab:'Tab tùy biến' + nm('tabs'), pnl:'Panel tùy biến' + nm('panels'), add:'Nút thêm vào panel có sẵn', tb:'Toolbar' + nm('tb'), mn:'Menu' + nm('menus'), madd:'Mục thêm vào menu có sẵn', cmd:'Lệnh' + nm('cmds'), qat:'Thanh truy cập nhanh', ctx:'Menu chuột phải', lisp:'Tệp LISP đi kèm', part:'Tệp tùy biến phụ', dbl:'Nhấp đúp · ' + ((DBL.filter(function(r){ return r[0] === id; })[0] || [])[1] || id), mouse:'Nút chuột · ' + ((MOUSE.filter(function(r){ return r[0] === id; })[0] || [])[1] || id) }[t] || k; }
    function nameOk(v, taken, what){ return !v ? 'Nhập tên ' + what + '.' : taken.indexOf(v) >= 0 ? 'Đã có ' + what + ' “' + v + '”.' : null; }
    function renameCmd(o, n){ var fix = function(l){ (l || []).forEach(function(i){ if (i.cmd === o) i.cmd = n; }); }; dr.panels.forEach(function(p){ fix(p.items); }); dr.tabs.forEach(function(t){ t.panels.forEach(function(p){ fix(p.items); }); }); Object.keys(dr.add).forEach(function(k){ fix(dr.add[k]); }); dr.tb.forEach(function(t){ fix(t.items); }); dr.menus.forEach(function(m){ fix(m.items); }); Object.keys(dr.madd).forEach(function(k){ fix(dr.madd[k]); }); fix(dr.ctx); dr.qat = dr.qat.map(function(q){ return q === o ? n : q; }); }
    function dropCmd(n){ var f = function(l){ return (l || []).filter(function(i){ return i.cmd !== n; }); }; dr.panels.forEach(function(p){ p.items = f(p.items); }); dr.tabs.forEach(function(t){ t.panels.forEach(function(p){ p.items = f(p.items); }); }); Object.keys(dr.add).forEach(function(k){ dr.add[k] = f(dr.add[k]); }); dr.tb.forEach(function(t){ t.items = f(t.items); }); dr.menus.forEach(function(m){ m.items = f(m.items); }); Object.keys(dr.madd).forEach(function(k){ dr.madd[k] = f(dr.madd[k]); }); dr.ctx = f(dr.ctx); dr.qat = dr.qat.filter(function(q){ return q !== n; }); dr.cmds = dr.cmds.filter(function(c){ return c.name !== n; }); }
    function curTab(){ var k = U.sel; if (/^t:/.test(k)) return k.slice(2); if (/^p:/.test(k)){ var vp = vPanel(k.slice(2)); if (vp) return vp.t._tk; } return 'Trang chủ'; }
    function pickIcon(cur){ return new Promise(function(res){ var names = Object.keys(TC.ICONS).filter(function(n){ return n !== 'logo'; }), sel = cur;
      var d2 = TC.dialog({ id:'cuicon', title:'Chọn biểu tượng', code:'UI-K07', w:540, help:false, onClose:function(r){ if (r !== true) res(null); },
        body:'<div class="stack"><label class="search">' + ic('search') + '<input id="ci-q" placeholder="Tìm theo tên: line, layer, print…" aria-label="Tìm biểu tượng" spellcheck="false"></label><div class="cu-icg" id="ci-g" role="listbox" aria-label="Biểu tượng"></div><p class="faint" style="margin:0;font-size:var(--fs-xs)">Bộ biểu tượng TrionCAD: lưới 16 px, nét 1,25; nút lớn dùng bản 32 px.</p></div>',
        foot:'<span class="faint" id="ci-n" style="font-size:var(--fs-s)"></span><span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="ci-ok">Chọn biểu tượng</button>', focus:'#ci-q' });
      function paint(){ var q = $('#ci-q', d2.el).value.trim().toLowerCase(), l = names.filter(function(n){ return !q || n.indexOf(q) >= 0; }); $('#ci-g', d2.el).innerHTML = l.length ? l.map(function(n){ return '<button type="button" role="option" class="cu-ic' + (n === sel ? ' on' : '') + '" data-ic="' + n + '" aria-selected="' + (n === sel) + '" title="' + n + '" aria-label="' + n + '">' + ic(n) + '</button>'; }).join('') : '<div class="empty">' + ic('search') + '<b>Không có biểu tượng khớp</b></div>'; $('#ci-n', d2.el).textContent = sel ? 'Đã chọn: ' + sel : 'Chưa chọn'; $('#ci-ok', d2.el).disabled = !sel; }
      $('#ci-q', d2.el).addEventListener('input', paint);
      d2.el.addEventListener('click', function(e){ var b = e.target.closest('[data-ic]'); if (b){ sel = b.dataset.ic; paint(); var nb = $('[data-ic="' + sel + '"]', d2.el); if (nb) nb.focus(); } });
      d2.el.addEventListener('dblclick', function(e){ if (e.target.closest('[data-ic]')){ d2.close(true); res(sel); } });
      $('#ci-ok', d2.el).addEventListener('click', function(){ d2.close(true); res(sel); }); paint(); }); }
    function askName(title, label, def, taken, what){ return SC.ask(title, label, def, function(v){ return nameOk(v, taken, what); }, 'UI-K07'); }

    /* ---- hành động ---- */
    function act(a, b){ var r = selRow(), rc = b && b.getBoundingClientRect();
      if (a === 'new') return TC.menu([{ h:'Tạo mới trong trion.tcui' }, { label:'Panel trong tab “' + esc((V0().filter(function(t){ return t._tk === curTab(); })[0] || {}).name || 'Trang chủ') + '”…', icon:'gridview', act:function(){ act('newpnl'); } }, { label:'Tab ribbon…', icon:'layout', act:function(){ act('newtab'); } }, { label:'Toolbar…', icon:'listview', act:function(){ act('newtb'); } }, { label:'Menu…', icon:'menu', act:function(){ act('newmn'); } }, { sep:1 }, { label:'Lệnh mới', icon:'play', act:function(){ act('newcmd'); } }], rc.left, rc.bottom + 2, { returnFocus:b, minWidth:230 });
      if (a === 'newpnl'){ var tk = curTab(), ct = /^cu:/.test(tk) ? cTab(tk.slice(3)) : null, vt = V0().filter(function(t){ return t._tk === tk; })[0] || V0()[0];
        return askName('Panel mới trong tab ' + vt.name, 'Tên panel', 'Của tôi', vt.panels.map(function(p){ return p.name; }), 'panel').then(function(v){ if (!v) return; var x = { uid:uid(), name:v, icon:'star', items:[] }; if (ct) ct.panels.push(x); else { x.tab = vt._tk; dr.panels.push(x); } U.sel = 'p:cu:' + x.uid; U.foc = 'node'; U.nk[U.sel] = 1; openPath(U.sel); U.open['t:' + vt._tk] = 1; render(); }); }
      if (a === 'newtab') return askName('Tab ribbon mới', 'Tên tab', 'Của tôi', V0().map(function(t){ return t.name; }), 'tab').then(function(v){ if (!v) return; var x = { uid:uid(), name:v, panels:[{ uid:uid(), name:'Lệnh hay dùng', icon:'star', items:[] }] }; dr.tabs.push(x); U.nk['t:cu:' + x.uid] = 1; U.sel = 'p:cu:' + x.panels[0].uid; U.foc = 'node'; U.open.rb = 1; U.open['t:cu:' + x.uid] = 1; render(); });
      if (a === 'newtb' || a === 'newmn'){ var isT = a === 'newtb'; return askName(isT ? 'Toolbar mới' : 'Menu mới', isT ? 'Tên toolbar' : 'Tên menu', isT ? 'Toolbar của tôi' : 'Của tôi', (isT ? tbList() : mnList()).map(function(x){ return x.name; }), isT ? 'toolbar' : 'menu').then(function(v){ if (!v) return; var x = { uid:uid(), name:v, items:[] }; if (isT){ x.pos = 1; dr.tb.push(x); } else dr.menus.push(x); U.sel = (isT ? 'tb:cu:' : 'mn:cu:') + x.uid; U.nk[U.sel] = 1; U.foc = 'node'; openPath(U.sel); render(); }); }
      if (a === 'newcmd'){ var i = 1; while (ci('LENH' + i).desc !== 'LENH' + i || TC.CMDS['LENH' + i]) i++; var x = { uid:uid(), name:'LENH' + i, label:'Lệnh mới ' + i, macro:'^C^C', icon:'play', tip:'', desc:'' }; dr.cmds.push(x); U.cmd = x.name; U.foc = 'cmd'; U.q = ''; $('#cu-q', el).value = ''; U.grp = 'Tất cả lệnh'; render(); setTimeout(function(){ var f = $('[data-pf="clabel"]', el); if (f){ f.focus(); f.select(); } }, 0); return; }
      if (a === 'dupcmd'){ var c = ci(U.cmd), j = 1, nn = c.name + '-2'; while (ci(nn).desc !== nn || TC.CMDS[nn]) nn = c.name + '-' + (++j + 1); var y = { uid:uid(), name:nn.slice(0, 24), label:c.desc + ' (của tôi)', macro:'^C^C_' + c.name, icon:c.icon || 'play', tip:'', desc:'' }; dr.cmds.push(y); U.cmd = y.name; render(); return; }
      if (a === 'delcmd'){ var u = usedIn(U.cmd), n0 = U.cmd; return TC.confirm({ title:'Xóa lệnh tùy biến', msg:'Xóa lệnh <b>' + esc(ci(n0).desc) + '</b> (' + esc(n0) + ')' + (u.length ? ' và bỏ khỏi <b>' + u.length + '</b> chỗ đang dùng?' : '?'), ok:'Xóa lệnh', danger:true }).then(function(ok){ if (!ok) return; dropCmd(n0); U.cmd = null; U.foc = 'node'; render(); }); }
      if (a === 'addsel'){ addTo(U.sel, U.cmd); return; }
      if (a === 'up' || a === 'down'){ if (!r) return; var l = r.q ? dr.qat : r.list, o = r.q ? r.cmd : r.cu, p = l.indexOf(o), q = p + (a === 'up' ? -1 : 1); if (p < 0 || q < 0 || q >= l.length) return; l.splice(p, 1); l.splice(q, 0, o); render(); return; }
      if (a === 'del'){ if (!r) return; var l2 = r.q ? dr.qat : r.list, o2 = r.q ? r.cmd : r.cu; l2.splice(l2.indexOf(o2), 1); U.it = null; U.foc = 'node'; render(); return; }
      if (a === 'resetit'){ if (r) delete dr.mod[r.key]; render(); return; }
      if (a === 'resetp'){ var pk = U.sel.slice(2), na = (dr.add[pk] || []).length; return (na ? TC.confirm({ title:'Đặt lại panel', msg:'Đặt lại panel về mặc định và bỏ <b>' + na + '</b> nút đã thêm?', ok:'Đặt lại panel' }) : Promise.resolve(true)).then(function(ok){ if (!ok) return; delete dr.mod[pk]; delete dr.add[pk]; Object.keys(dr.mod).forEach(function(k){ if (k.indexOf(pk + '/') === 0) delete dr.mod[k]; }); render(); }); }
      if (a === 'delpnl'){ var x2 = cPanel(U.sel.slice(5)); if (!x2) return; return TC.confirm({ title:'Xóa panel', msg:'Xóa panel <b>' + esc(x2.name) + '</b>' + (x2.items.length ? ' cùng <b>' + x2.items.length + '</b> nút?' : '?'), ok:'Xóa panel', danger:true }).then(function(ok){ if (!ok) return; dr.panels = dr.panels.filter(function(p){ return p !== x2; }); dr.tabs.forEach(function(t){ t.panels = t.panels.filter(function(p){ return p !== x2; }); }); U.sel = 't:' + (x2.tab || 'Trang chủ'); U.foc = 'node'; render(); }); }
      if (a === 'deltab'){ var t2 = cTab(U.sel.slice(5)); if (!t2) return; return TC.confirm({ title:'Xóa tab', msg:'Xóa tab <b>' + esc(t2.name) + '</b> cùng <b>' + t2.panels.length + '</b> panel?', ok:'Xóa tab', danger:true }).then(function(ok){ if (!ok) return; dr.tabs = dr.tabs.filter(function(t){ return t !== t2; }); U.sel = 'rb'; U.foc = 'node'; render(); }); }
      if (a === 'delobj'){ var isT2 = U.sel[0] === 't', l3 = isT2 ? 'tb' : 'menus', o3 = dr[l3].filter(function(y){ return y.uid === U.sel.slice(6); })[0]; if (!o3) return; return TC.confirm({ title:isT2 ? 'Xóa toolbar' : 'Xóa menu', msg:'Xóa ' + (isT2 ? 'toolbar' : 'menu') + ' <b>' + esc(o3.name) + '</b>' + (o3.items.length ? ' cùng <b>' + o3.items.length + '</b> mục?' : '?'), ok:isT2 ? 'Xóa toolbar' : 'Xóa menu', danger:true }).then(function(ok){ if (!ok) return; dr[l3] = dr[l3].filter(function(y){ return y !== o3; }); U.sel = isT2 ? 'tbs' : 'mns'; U.foc = 'node'; render(); }); }
      if (a === 'copytb'){ var src = tbList().filter(function(x){ return x.k === U.sel; })[0], it0 = rows(U.sel) || []; if (!src) return; var x3 = { uid:uid(), name:uniq(src.name + ' của tôi', tbList().map(function(v){ return v.name; })), pos:1, items:it0.filter(function(rr){ return rr.cmd; }).map(function(rr){ return { uid:uid(), cmd:rr.cmd, icon:rr.icon }; }) }; dr.tb.push(x3); U.sel = 'tb:cu:' + x3.uid; U.nk[U.sel] = 1; U.foc = 'node'; render(); return; }
      if (a === 'icon' || a === 'picon' || a === 'cicon'){ var cur = a === 'cicon' ? (dr.cmds.filter(function(c2){ return c2.name === U.cmd; })[0] || {}).icon : a === 'picon' ? (vPanel(U.sel.slice(2)) || { p:{} }).p.icon : r && r.icon; return pickIcon(cur).then(function(nm){ if (!nm) return;
        if (a === 'cicon'){ var cx = dr.cmds.filter(function(c2){ return c2.name === U.cmd; })[0]; if (cx) cx.icon = nm; }
        else if (a === 'picon'){ var pk2 = U.sel.slice(2), cp = /^cu:/.test(pk2) && cPanel(pk2.slice(3)); if (cp) cp.icon = nm; else { var vp2 = vPanel(pk2), orig = grab().tabs.filter(function(t){ return t.name === vp2.t._on; })[0].panels.filter(function(p){ return p.name === vp2.p._on; })[0]; setMod(pk2, 'icon', orig && orig.icon === nm ? null : nm); } }
        else if (r && r.cu) r.cu.icon = nm; else if (r && r.bi) setMod(r.key, 'icon', nm === r.bi._oi ? null : nm);
        render(); }); }
      if (a === 'imp'){ U.xs = 'cuix'; U.xmsg = ''; xAll(); $('[data-dt="xfer"]', el).click(); return; }
      if (a === 'xall'){ xAll(); rX(); return; } if (a === 'xnone'){ U.xc = {}; rX(); return; } if (a === 'xcopy'){ xCopy(); return; }
      if (a === 'xopen') return TC.menu(Object.keys(XSRC).map(function(k){ return { label:esc(XSRC[k].name) + ' <span class="faint">· ' + esc(XSRC[k].note) + '</span>', icon:'file', check:k === U.xs, act:function(){ U.xs = k; U.xmsg = ''; U.xc = {}; rX(); } }; }).concat([{ sep:1 }, { label:'Tệp khác (.cuix, .tcui)…', icon:'folder', act:function(){ TC.toast('Prototype: có sẵn hai tệp mẫu để chuyển. Bản thật mở hộp thoại chọn tệp .cuix, .tcui.', { type:'info', log:false }); } }]), rc.left, rc.bottom + 2, { returnFocus:b });
      if (a === 'keys'){ SC.shortcuts(); return; }
      if (a === 'wssave'){ SC.wsSave().then(render); return; } if (a === 'wsset'){ SC.wsSettings(); return; } if (a === 'wsgo'){ SC.wsSwitch(b.dataset.ws); render(); return; }
      if (a === 'lispadd') return TC.menu(LF.filter(function(f){ return dr.lisp.indexOf(f.name) < 0; }).map(function(f){ return { label:esc(f.name) + ' <span class="faint">· ' + esc(f.dir) + '</span>', icon:'lisp', act:function(){ dr.lisp.push(f.name); render(); } }; }).concat(dr.lisp.length === LF.length ? [{ label:'Đã thêm mọi tệp mẫu', dis:true }] : []), rc.left, rc.bottom + 2, { returnFocus:b });
      if (a === 'lispdel'){ dr.lisp.splice(Number(b.dataset.i), 1); render(); return; }
      if (a === 'partadd') return TC.menu(Object.keys(XSRC).filter(function(k){ return k !== 'cuix'; }).map(function(k){ var on = dr.part.some(function(p){ return p.id === k; }); return { label:esc(XSRC[k].name) + ' <span class="faint">· ' + esc(on ? 'đã gắn' : XSRC[k].note) + '</span>', icon:'file', dis:on, act:function(){ dr.part.push({ id:k }); U.sel = 'part:' + k; U.open.part = 1; U.nk[U.sel] = 1; render(); } }; }).concat([{ label:'Tệp tùy biến doanh nghiệp…' + (TC.can('ENT') ? '' : ' <span class="ed xs" data-ed="ent">Doanh nghiệp</span>'), icon:'lock', dis:!TC.can('ENT'), act:function(){ TC.toast('Tệp tùy biến doanh nghiệp do quản trị viên đặt ở Cổng quản trị (UI-L15); máy trạm nhận về, chỉ đọc.', { type:'info', log:false }); } }, { sep:1 }, { label:'Tệp khác (.tcui)…', icon:'folder', act:function(){ TC.toast('Prototype: có sẵn một tệp mẫu để gắn. Bản thật mở hộp thoại chọn tệp .tcui.', { type:'info', log:false }); } }]), rc.left, rc.bottom + 2, { returnFocus:b, minWidth:320 });
      if (a === 'partdel'){ var pid = b.dataset.id; return TC.confirm({ title:'Gỡ tệp tùy biến phụ', msg:'Gỡ <b>' + esc(XSRC[pid].name) + '</b> khỏi giao diện?', detail:'Toolbar, menu, lệnh của tệp này không còn hiện. Tệp trên máy chủ không bị xóa.', ok:'Gỡ tệp' }).then(function(ok){ if (!ok) return; dr.part = dr.part.filter(function(p){ return p.id !== pid; }); U.sel = 'part'; render(); }); }
      if (a === 'exp'){ TC.toast('Đã xuất <span class="mono">trion-' + TC.date().replace(/\//g, '') + '.tcui</span> (' + Object.keys(units(CU)).length + ' tùy biến đã áp dụng).', { log:false, act:{ label:'Mở thư mục', fn:function(){} } }); return; }
      if (a === 'folder'){ TC.toast('Mở thư mục C:\\TrionCAD\\Support trong File Explorer.', { type:'info', log:false }); return; }
    }

    /* ---- sự kiện ---- */
    el.addEventListener('click', function(e){
      var tg = e.target.closest('[data-tg]'); if (tg && tg.dataset.tg){ U.open[tg.dataset.tg] = !U.open[tg.dataset.tg]; rTree(); return; }
      var tn = e.target.closest('[data-tn]'); if (tn){ U.sel = tn.dataset.tn; U.foc = 'node'; U.it = null; U.help = null; if (!U.open[U.sel] && tn.hasAttribute('aria-expanded')) U.open[U.sel] = 1; openPath(U.sel); render(); if (tn.closest('#cu-tree')) $('#cu-tree', el).focus(); return; }
      var cix = e.target.closest('[data-ci]'); if (cix && cix.dataset.ci){ U.it = cix.dataset.ci; U.foc = 'item'; U.help = null; render(); return; }
      var cn = e.target.closest('[data-cn]'); if (cn){ U.cmd = cn.dataset.cn; U.foc = 'cmd'; U.help = null; render(); $('#cu-cl', el).focus(); return; }
      var pt = e.target.closest('[data-pt]'); if (pt){ var r = selRow(); if (r && r.cu){ r.cu.t = pt.dataset.pt; render(); } return; }
      var ch = e.target.closest('[data-chip]'); if (ch){ U.sel = ch.dataset.chip; U.foc = 'node'; U.it = null; openPath(U.sel); render(); return; }
      var ky = e.target.closest('[data-key]'); if (ky && ky.closest('#cu-c')){ SC.shortcuts(ky.dataset.key); return; }
      var b = e.target.closest('[data-cu]'); if (b && !b.disabled) act(b.dataset.cu, b);
    });
    el.addEventListener('dblclick', function(e){ var cn = e.target.closest('[data-cn]'); if (cn){ addTo(U.sel, cn.dataset.cn); return; } var tn = e.target.closest('#cu-tree [data-tn][aria-expanded]'); if (tn){ U.open[tn.dataset.tn] = tn.getAttribute('aria-expanded') !== 'true'; rTree(); } });
    el.addEventListener('input', function(e){ var t = e.target, f = t.dataset.pf, r = selRow(), v = t.value;
      if (t.id === 'cu-q'){ U.q = v; rCmds(); return; }
      if (!f) return;
      if (f === 'label' && r){ if (r.bi) setMod(r.key, 'label', v === r.bi._ol ? null : v); else if (r.cu){ if (v.trim()) r.cu.label = v; else delete r.cu.label; } }
      else if (f === 'pname' || f === 'tname'){ var isP = f === 'pname', k = U.sel.slice(2), x = isP ? /^cu:/.test(k) && cPanel(k.slice(3)) : /^cu:/.test(k) && cTab(k.slice(3)); if (!v.trim()) return; if (x) x.name = v.trim(); else setMod(k, 'label', v.trim() === (isP ? vPanel(k).p._on : k) ? null : v.trim()); }
      else if (f === 'oname'){ var o = (U.sel[0] === 't' ? dr.tb : dr.menus).filter(function(y){ return y.uid === U.sel.slice(6); })[0]; if (o && v.trim()) o.name = v.trim(); }
      else if (/^c/.test(f)){ var c = dr.cmds.filter(function(y){ return y.name === U.cmd; })[0]; if (!c) return;
        if (f === 'clabel'){ if (v.trim()) c.label = v.trim(); }
        else if (f === 'cname'){ var nv = v.trim().toUpperCase(), m = $('#cu-cnm', el), bad = !/^[A-Z][A-Z0-9_-]{1,23}$/.test(nv) ? 'Tên lệnh: chữ cái đầu, chỉ A–Z, 0–9, - và _, tối đa 24 ký tự.' : nv !== c.name && (TC.CMDS[nv] && !TC.CMDS[nv].cu || dr.cmds.some(function(y){ return y.name === nv; })) ? 'Đã có lệnh ' + nv + '. Chọn tên khác.' : null;
          t.classList.toggle('err', !!bad); m.className = 'fmsg ' + (bad ? 'err' : 'hint'); m.innerHTML = bad ? ic('warn', 'xs') + esc(bad) : ''; if (bad || nv === c.name) return; renameCmd(c.name, nv); c.name = nv; U.cmd = nv; }
        else if (f === 'cmacro'){ c.macro = v; var ck = macroCheck(v), mm = $('#cu-mm', el); mm.className = 'fmsg cu-mm ' + (ck[0] === 'err' ? 'err' : ck[0] === 'warn' ? 'warn' : 'hint'); mm.innerHTML = (ck[0] !== 'hint' ? ic('warn', 'xs') : '') + esc(ck[1]); }
        else if (f === 'ctip') c.tip = v; else if (f === 'cdesc') c.desc = v; }
      live2(); var pt = $('.cu-pt b', el); if (pt && /^(label|pname|tname|oname|clabel)$/.test(f) && v.trim()) pt.textContent = v.replace(/\n/g, ' '); });
    el.addEventListener('change', function(e){ var t = e.target, f = t.dataset.pf;
      if (t.id === 'cu-g'){ U.grp = t.value; rCmds(); return; }
      if (t.id === 'cux-s'){ U.xs = t.value; U.xc = {}; U.xmsg = ''; rX(); return; }
      if (t.dataset.xk){ U.xc[t.dataset.xk] = t.checked; rX(); return; }
      if (t.dataset.dbl){ if (t.value) dr.dbl[t.dataset.dbl] = t.value; else delete dr.dbl[t.dataset.dbl]; render(); return; }
      if (t.dataset.mo){ if (t.value) dr.mouse[t.dataset.mo] = t.value; else delete dr.mouse[t.dataset.mo]; render(); return; }
      var k = U.sel.slice(2);
      if (f === 'phide') setMod(k, 'hide', !t.checked);
      else if (f === 'thide'){ if (!t.checked && V0().filter(function(x){ return !x._hide; }).length < 2){ t.checked = true; TC.toast('Ribbon cần ít nhất một tab hiện.', { type:'info', log:false }); return; } setMod(k, 'hide', !t.checked); }
      else if (f === 'pprio'){ var cp = /^cu:/.test(k) && cPanel(k.slice(3)), vp = vPanel(k); if (cp) cp.prio = Number(t.value); else setMod(k, 'prio', Number(t.value) === (grab().tabs.filter(function(x){ return x.name === vp.t._on; })[0].panels.filter(function(p){ return p.name === vp.p._on; })[0].prio || 1) ? null : Number(t.value)); }
      else if (f === 'ptab'){ var cp2 = cPanel(k.slice(3)); if (cp2){ cp2.tab = t.value; U.open['t:' + t.value] = 1; } }
      else if (f === 'tpos'){ var o = dr.tb.filter(function(y){ return y.uid === U.sel.slice(6); })[0]; if (o) o.pos = t.value === '1' ? 1 : t.value; }
      else return;
      render(); });
    el.addEventListener('dtab', function(e){ if (e.detail === 'xfer') rX(); else render(); });
    // mô tả thuộc tính ở đáy khung (như CUI của AutoCAD)
    el.addEventListener('mouseover', function(e){ var p = e.target.closest('.cu-pr[data-h]'); if (p && p.dataset.h) $('#cu-d', el).textContent = p.dataset.h; });
    el.addEventListener('focusin', function(e){ var p = e.target.closest('.cu-pr[data-h]'); if (p && p.dataset.h) $('#cu-d', el).textContent = p.dataset.h; });
    // kéo lệnh thả vào panel, toolbar, menu, thanh truy cập nhanh
    el.addEventListener('dragstart', function(e){ var c = e.target.closest('[data-cn]'); if (!c) return; e.dataTransfer.setData('text/plain', c.dataset.cn); e.dataTransfer.effectAllowed = 'copy'; el.classList.add('cu-dragging'); });
    el.addEventListener('dragend', function(){ el.classList.remove('cu-dragging'); $$('.is-drop', el).forEach(function(x){ x.classList.remove('is-drop'); }); });
    el.addEventListener('dragover', function(e){ var z = e.target.closest('[data-drop]'); if (!z) return; e.preventDefault(); e.dataTransfer.dropEffect = 'copy'; $$('.is-drop', el).forEach(function(x){ if (x !== z) x.classList.remove('is-drop'); }); z.classList.add('is-drop'); });
    el.addEventListener('dragleave', function(e){ var z = e.target.closest('[data-drop]'); if (z && !z.contains(e.relatedTarget)) z.classList.remove('is-drop'); });
    el.addEventListener('drop', function(e){ var z = e.target.closest('[data-drop]'); if (!z) return; e.preventDefault(); el.classList.remove('cu-dragging'); var n = e.dataTransfer.getData('text/plain'); if (n && (cinfo(n) || TC.cmd.def(n))) addTo(z.dataset.drop, n); });
    // bàn phím: ↑↓ trong cây và danh sách lệnh, ←→ thu gọn, mở nhánh, Enter thêm lệnh vào mục đang chọn
    $('#cu-tree', el).addEventListener('keydown', function(e){ var ns = nodes(), i = ns.map(function(n){ return n.k; }).indexOf(U.sel), n = ns[i]; if (!n) return;
      if (e.key === 'ArrowDown' || e.key === 'ArrowUp'){ e.preventDefault(); var m = ns[TC.clamp(i + (e.key === 'ArrowDown' ? 1 : -1), 0, ns.length - 1)]; U.sel = m.k; U.foc = 'node'; U.it = null; render(); }
      else if (e.key === 'ArrowRight' && n.kids && !U.open[n.k]){ e.preventDefault(); U.open[n.k] = 1; rTree(); }
      else if (e.key === 'ArrowLeft'){ e.preventDefault(); if (n.kids && U.open[n.k]){ U.open[n.k] = 0; rTree(); } else { for (var j = i - 1; j >= 0; j--) if (ns[j].d < n.d){ U.sel = ns[j].k; U.foc = 'node'; render(); break; } } }
      else if (e.key === 'Enter' && n.kids){ e.preventDefault(); U.open[n.k] = !U.open[n.k]; rTree(); } });
    $('#cu-cl', el).addEventListener('keydown', function(e){ var cs = $$('[data-cn]', el), i = cs.map(function(c){ return c.dataset.cn; }).indexOf(U.cmd);
      if (e.key === 'ArrowDown' || e.key === 'ArrowUp'){ e.preventDefault(); var c = cs[TC.clamp(i + (e.key === 'ArrowDown' ? 1 : -1), 0, cs.length - 1)]; if (c){ U.cmd = c.dataset.cn; U.foc = 'cmd'; render(); } }
      else if (e.key === 'Enter' && U.cmd){ e.preventDefault(); e.stopPropagation(); addTo(U.sel, U.cmd); } });
    $('#cu-ok', el).addEventListener('click', function(){ var n = diff(dr, CU).length; if (!n) return; var prev = CU, next = clone(dr), first = diff(dr, CU).map(function(k){ return /^mod:/.test(k) ? k.slice(4).split('/')[0] : /^pnl:/.test(k) ? (dr.panels.filter(function(p){ return 'pnl:' + p.uid === k; })[0] || {}).tab : /^add:/.test(k) ? (units(dr)[k] || [''])[0].split('/')[0] : /^tab:/.test(k) ? (dr.tabs.filter(function(t){ return 'tab:' + t.uid === k; })[0] || {}).name : null; }).filter(Boolean)[0];
      try { applyCU(next); } catch (err){ applyCU(prev); TC.toast('Không áp dụng được tùy biến: ' + esc(err.message) + '. Giao diện giữ như cũ.', { type:'err' }); return; }
      TC.save('cui1', next); dlg.close('ok');
      if (first && A().active !== 'start'){ var tabName = (live(next).filter(function(t){ return t._tk === first || t._on === first || t.name === first; })[0] || {}).name, tb = tabName && $$('.rb-tab').filter(function(x){ return x.textContent.trim() === tabName; })[0]; if (tb) tb.click(); SC.cat.flash('#rb'); }
      TC.toast('Đã áp dụng ' + n + ' thay đổi giao diện vào trion.tcui.', { act:{ label:'Hoàn tác', fn:function(){ applyCU(prev); TC.save('cui1', prev); TC.toast('Đã trả giao diện về như trước.', { log:false }); } } }); });
    $('#cu-rs', el).addEventListener('click', function(){ var n = Object.keys(units(dr)).length; if (!n){ TC.toast('Giao diện đang là mặc định của TrionCAD.', { type:'info', log:false }); return; }
      TC.confirm({ title:'Khôi phục mặc định', msg:'Khôi phục giao diện mặc định của TrionCAD và bỏ <b>' + n + '</b> tùy biến trong trion.tcui?', detail:'TrionCAD lưu bản sao trion.tcui.bak cạnh tệp để lấy lại khi cần. Phím tắt, viết tắt lệnh và workspace không đổi.', ok:'Khôi phục mặc định', danger:true }).then(function(ok){ if (!ok) return; var prev = CU; dr = DRAFT = CU0(); applyCU(CU0()); TC.save('cui1', null); U.nk = {}; U.sel = 't:Trang chủ'; U.foc = 'node'; U.it = null; render();
        TC.toast('Đã khôi phục giao diện mặc định (bỏ ' + n + ' tùy biến).', { act:{ label:'Hoàn tác', fn:function(){ applyCU(prev); TC.save('cui1', prev); if (!dlg.closed){ dr = DRAFT = clone(prev); render(); } } } }); }); });
    // danh sách thay đổi chưa áp dụng; bấm một mục để mở chỗ đã đổi
    function goKey(k){ var t = k.split(':')[0], id = k.slice(t.length + 1), u = units(dr)[k] || units(CU)[k], s = id.split('/'), f = function(l){ return (dr[l].filter(function(x){ return x.uid === id; })[0] || {}); };
      U.foc = 'node'; U.it = null;
      if (t === 'mod'){ U.sel = s.length === 1 ? 't:' + s[0] : 'p:' + s[0] + '/' + s[1]; if (s.length > 2){ U.it = id; U.foc = 'item'; } }
      else if (t === 'tab') U.sel = 't:cu:' + id; else if (t === 'pnl') U.sel = 'p:cu:' + id; else if (t === 'add' && u) U.sel = 'p:' + u[0];
      else if (t === 'tb') U.sel = 'tb:cu:' + id; else if (t === 'mn') U.sel = 'mn:cu:' + id; else if (t === 'madd' && u) U.sel = 'mn:' + u[0];
      else if (t === 'cmd'){ U.cmd = f('cmds').name; U.foc = U.cmd ? 'cmd' : 'node'; } else U.sel = { qat:'qat', ctx:'ctx:sel', lisp:'lisp', part:'part', dbl:'dbl', mouse:'mouse' }[t] || 'f';
      openPath(U.sel); if ($('.cux', el).hidden === false) $('[data-dt="cust"]', el).click(); render(); }
    $('#cu-n', el).addEventListener('click', function(){ var rc = this.getBoundingClientRect(), ks = diff(dr, CU); TC.menu([{ h:ks.length + ' thay đổi chưa áp dụng' }].concat(ks.slice(0, 16).map(function(k){ return { label:esc(human(k)), icon:/^mod:/.test(k) ? 'bedit' : 'plus', act:function(){ goKey(k); } }; })).concat(ks.length > 16 ? [{ label:'… và ' + (ks.length - 16) + ' thay đổi khác', dis:true }] : []), rc.left, rc.top - 4, { up:true, minWidth:300, returnFocus:this }); });
    if (U.foc === 'cmd'){ U.grp = 'Tất cả lệnh'; } openPath(U.sel); render();
    return dlg;
  };
  IMPL.CUI = function(ctx, pre){ SC.cui(pre); };
  OPEN['UI-K07'] = function(){ SC.cat.PRO(); var t = TC.topDialog(); if (t && t.id === 'cui') t.close('force'); SC.cui('p:Trang chủ/Vẽ'); };

  /* ================= Quản lý plugin UI-K10 (PLUGINS) =================
     Plugin dựng bằng SDK C API của TrionCAD (05 §12), gói .tcplugin; quyền khai báo trong gói; chưa ký số thì hỏi tin cậy. */
  var PDEF = [
    { id:'damsan', name:'Dầm sàn VN', dev:'Công ty TNHH Phần mềm Kết Cấu Việt', ver:'2.3.1', up:'2.4.0', upNote:'thêm dầm công-xôn, sửa thống kê thép Ø32, nhanh hơn 30% với bản vẽ trên 5.000 dầm', signed:true, cert:'Kết Cấu Việt · chứng thư mã nguồn còn hạn tới 03/2028', on:true, st:'ok', size:'8,4 MB', date:'12/08/2026', sdk:'1.0', ms:120, tcui:'dam-san-vn.tcui', tb:'Dầm sàn VN', dir:'DamSanVN',
      desc:'Vẽ dầm, sàn bê tông cốt thép theo lưới trục và thống kê thép theo TCVN 5574:2018.', perm:{ read:'Thư mục dự án đang mở', write:'Thư mục dự án đang mở', net:'api.ketcauviet.vn · kiểm tra bản quyền', cmd:'Chạy lệnh, thêm và sửa đối tượng' }, cmds:[['DAMSAN', 'Vẽ dầm sàn theo lưới trục', 'pline'], ['TKDAM', 'Thống kê thép dầm', 'stats'], ['BANGTHEP', 'Chèn bảng thống kê thép', 'table']] },
    { id:'diahinh', name:'Địa hình 2D', dev:'GeoVN Software', ver:'1.4.0', signed:true, cert:'GeoVN Software · chứng thư còn hạn tới 11/2027', on:true, st:'ok', size:'5,1 MB', date:'03/05/2026', sdk:'1.0', ms:85, dir:'DiaHinh2D',
      desc:'Nhập điểm đo, ghi cao độ, vẽ đường đồng mức và mặt cắt dọc cho bản vẽ san nền.', perm:{ read:'Tệp điểm đo (.txt, .csv) bạn chọn', write:'', net:'', cmd:'Chạy lệnh, thêm đối tượng' }, cmds:[['CAODO', 'Ghi cao độ điểm đo', 'elev'], ['DUONGDONGMUC', 'Vẽ đường đồng mức', 'spline'], ['MATCATDOC', 'Vẽ mặt cắt dọc', 'cutsym']] },
    { id:'anphu', name:'Khung tên An Phú', dev:'Phòng Kỹ thuật, Công ty CP Xây dựng An Phú', ver:'3.0.0', signed:true, cert:'An Phú · chứng thư còn hạn tới 06/2027', on:true, st:'err', size:'1,2 MB', date:'20/09/2026', sdk:'1.0', dir:'KhungTenAnPhu',
      err:'Không nạp được: thiếu tệp an-phu-core.dll trong thư mục plugin. Phần mềm diệt virus có thể đã cách ly tệp này.', logl:['18/10/2026 08:12:03  nạp an-phu-core.dll: không tìm thấy tệp (0x8007007E)', '18/10/2026 08:12:03  dừng nạp plugin; 0/1 lệnh được đăng ký'],
      desc:'Chèn khung tên, bảng thống kê bản vẽ theo mẫu của công ty An Phú.', perm:{ read:'\\\\SRV-01\\Mau (mẫu khung tên)', write:'', net:'', cmd:'Chạy lệnh, thêm đối tượng' }, cmds:[['KTAP', 'Chèn khung tên An Phú', 'layout']] },
    { id:'capdien', name:'Cáp điện tòa nhà', dev:'ElecSoft VN', ver:'0.9.2', signed:true, cert:'ElecSoft VN · chứng thư còn hạn tới 01/2027', on:false, st:'incompat', size:'3,7 MB', date:'14/01/2026', sdk:'0.9', dir:'CapDienToaNha',
      inc:'Viết cho SDK 0.9 (bản thử nghiệm). TrionCAD này dùng SDK 1.0, cần bản plugin dựng lại với SDK 1.0.', desc:'Vẽ tuyến cáp, máng cáp và thống kê chiều dài cáp theo tầng.', perm:{ read:'Thư mục dự án đang mở', write:'Thư mục dự án đang mở', net:'', cmd:'Chạy lệnh, thêm đối tượng' }, cmds:[['TUYENCAP', 'Vẽ tuyến cáp', 'pline'], ['TKCAP', 'Thống kê chiều dài cáp', 'stats']] }
  ];
  var PKG = [{ file:'xuat-khoi-luong-1.0.3.tcplugin', size:'640 KB', date:'17/10/2026', p:{ id:'xkl', name:'Xuất khối lượng Excel', dev:'Nguyễn Văn Hùng', ver:'1.0.3', signed:false, on:true, st:'ok', size:'640 KB', date:TC.date(), sdk:'1.0', ms:40, dir:'XuatKhoiLuong', desc:'Đếm block, đo chiều dài theo layer rồi xuất bảng khối lượng ra Excel.', perm:{ read:'Mọi tệp trên máy', write:'Mọi tệp trên máy', net:'Mọi địa chỉ (gửi thống kê sử dụng)', cmd:'Chạy lệnh TrionCAD' }, cmds:[['XKL', 'Xuất khối lượng ra Excel', 'stats']] } },
    { file:'dam-san-vn-2.4.0.tcplugin', size:'8,6 MB', date:'15/10/2026', upd:'damsan', ver:'2.4.0' },
    { file:'ban-ve-mau-khung-ten.tcplugin', size:'12 KB', date:'02/10/2026', broken:'Gói hỏng: thiếu tệp manifest.json. Tải lại gói từ nhà phát triển.' }];
  var PLUG = TC.load('plugins1', null) || clone(PDEF);
  function plugSave(){ TC.save('plugins1', PLUG); }
  function plugOf(id){ return PLUG.filter(function(p){ return p.id === id; })[0] || null; }
  function plugRun(p){ return p.on && p.st === 'ok'; }
  function plugParts(){ return PLUG.filter(function(p){ return plugRun(p) && p.tb; }).map(function(p){ return { id:'plug-' + p.id, name:p.tcui, path:'C:\\ProgramData\\TrionCAD\\Plugins\\' + p.dir + '\\' + p.tcui, plug:p.name, tb:[{ name:p.tb, items:p.cmds.map(function(c){ return c[0]; }) }], menus:[], cmds:[] }; }); }
  // lệnh của plugin: luôn đăng ký để gõ tên khi plugin tắt hay lỗi thì báo rõ lý do và cách bật
  function plugReg(){ Object.keys(TC.CMDS).forEach(function(n){ if (TC.CMDS[n].plug){ delete TC.CMDS[n]; delete IMPL[n]; } });
    PLUG.forEach(function(p){ p.cmds.forEach(function(c){ if (TC.CMDS[c[0]]) return; TC.CMDS[c[0]] = { name:c[0], aliases:[], need:'PRO', wave:5, desc:c[1], vi:TC.fold(c[1]), icon:c[2] || 'pkg', where:'Plugin · ' + p.name, plug:p.id };
      IMPL[c[0]] = function(ctx){ var q = plugOf(p.id); if (!q){ ctx.out('Plugin của lệnh này đã gỡ.'); return; }
        if (!plugRun(q)){ ctx.out('Lệnh <b>' + c[0] + '</b> thuộc plugin <b>' + esc(q.name) + '</b> ' + (q.st === 'err' ? 'đang lỗi, chưa nạp được' : q.st === 'incompat' ? 'không tương thích với bản TrionCAD này' : 'đang tắt') + '. <button class="chip" type="button" data-run="PLUGINS">Quản lý plugin</button>'); return; }
        ctx.out('<b>' + esc(q.name) + '</b> ' + q.ver + ' · ' + esc(c[1]) + '. Giao diện của lệnh do nhà phát triển plugin thiết kế; prototype dừng ở đây.', 'faint'); }; }); }); }
  plugReg();
  function plugApply(){ plugSave(); plugReg(); applyCU(CU, A().active === 'start'); }
  var PERM = [['read', 'open', 'Đọc tệp'], ['write', 'save', 'Ghi tệp'], ['net', 'globe', 'Truy cập mạng'], ['cmd', 'play', 'Chạy lệnh, sửa bản vẽ']];
  function permList(p, risk){ return PERM.map(function(q){ var v = p.perm[q[0]], wide = /^Mọi/.test(v || ''); return '<div class="plp' + (v ? '' : ' off') + '">' + ic(q[1], 's12') + '<span class="pln">' + q[2] + '</span><span class="plv">' + (v ? esc(v) : 'Không dùng') + '</span>' + (risk && wide ? '<span class="tag warn">rộng</span>' : '') + '</div>'; }).join(''); }
  function stTag(p){ return p.st === 'err' ? '<span class="tag danger">' + ic('error') + 'Lỗi tải</span>' : p.st === 'incompat' ? '<span class="tag warn">' + ic('warn') + 'Không tương thích</span>' : p.on ? '<span class="tag ok">' + ic('check') + 'Đang chạy</span>' : '<span class="tag">Đã tắt</span>'; }
  function sigTag(p){ return p.signed ? '<span class="tag ok">' + ic('shield') + 'Đã ký số</span>' : '<span class="tag warn">' + ic('warn') + 'Chưa ký</span>'; }
  // hỏi tin cậy khi cài: chưa ký thì nêu rủi ro, nhà phát hành tự khai, quyền yêu cầu; Không cài là nút mặc định (như UI-K02)
  function plugTrust(p, file, upd){ return new Promise(function(res){
    if (!p.signed && TC.S.edition === 'ent'){ TC.dialog({ id:'pltrust', title:'Chặn plugin chưa ký số', code:'UI-K10', w:520, help:false, onClose:function(){ res(false); }, body:'<div class="hrow pltw">' + ic('lock', 'st') + '<div class="stack" style="gap:6px"><p style="margin:0"><b>' + esc(p.name) + '</b> ' + p.ver + ' chưa có chữ ký số.</p><p style="margin:0;font-size:var(--fs-s)">Chính sách của công ty (TrionCAD Doanh nghiệp) chỉ cho cài plugin đã ký. Gửi gói <span class="mono">' + esc(file) + '</span> cho quản trị viên để duyệt.</p></div></div>', foot:'<span class="grow"></span><button class="btn primary" type="button" data-x="close">Đóng</button>' }); return; }
    var dlg = TC.dialog({ id:'pltrust', title:p.signed ? (upd ? 'Cập nhật plugin' : 'Cài plugin') : 'Plugin chưa ký số', code:'UI-K10', w:580, help:false, onClose:function(r){ if (r !== true) res(false); },
      body:'<div class="hrow pltw">' + ic(p.signed ? 'pkg' : 'warn', 'st pl-st' + (p.signed ? '' : ' warn')) + '<div class="stack" style="gap:8px"><p style="margin:0">' + (upd ? 'Cập nhật' : 'Cài') + ' <b>' + esc(p.name) + '</b> ' + esc(upd ? upd.ver + ' (đang dùng ' + upd.from + ')' : p.ver) + '?</p>' +
        (p.signed ? '<p style="margin:0;font-size:var(--fs-s)">' + ic('okc', 's12') + ' Chữ ký số hợp lệ: <b>' + esc(p.dev) + '</b>. Gói không bị sửa sau khi phát hành.</p>' : '<p style="margin:0;font-size:var(--fs-s)">Gói <span class="mono">' + esc(file) + '</span> không có chữ ký số: TrionCAD không xác minh được ai làm ra gói và gói có bị sửa sau khi phát hành hay không.</p>') +
        '<div class="frow" style="--lab-w:104px;font-size:var(--fs-s)"><span class="lab">Nhà phát hành</span><span>' + esc(p.dev) + (p.signed ? ' <span class="tag ok">' + ic('shield') + 'đã xác minh</span>' : ' <span class="faint">(tự khai, chưa xác minh)</span>') + '</span><span class="lab">Quyền yêu cầu</span><span class="stack" style="gap:2px">' + permList(p, !p.signed) + '</span><span class="lab">Lệnh thêm vào</span><span class="mono">' + esc(p.cmds.map(function(c){ return c[0]; }).join(', ')) + '</span></div>' +
        (p.signed ? '' : '<div class="note warn">' + ic('warn', 'xs') + '<span>Plugin chạy cùng quyền với TrionCAD: có thể đọc, sửa bản vẽ và gửi tệp ra ngoài. Chỉ cài khi bạn biết rõ người gửi gói.</span></div>' + H.chk('pt-ok', 'Tôi biết nguồn của gói này và chấp nhận rủi ro', false)) + '</div></div>',
      foot:'<span class="grow"></span>' + (p.signed ? '<button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="pt-yes">' + (upd ? 'Cập nhật lên ' + esc(upd.ver) : 'Cài plugin') + '</button>' : '<button class="btn danger" type="button" id="pt-yes" disabled>Cài và tin cậy</button><button class="btn primary" type="button" data-x="cancel">Không cài</button>'), focus:p.signed ? '#pt-yes' : '[data-x="cancel"]' });
    var c = $('#pt-ok', dlg.el); if (c) c.addEventListener('change', function(){ $('#pt-yes', dlg.el).disabled = !c.checked; });
    $('#pt-yes', dlg.el).addEventListener('click', function(){ dlg.close(true); res(true); }); }); }
  // tiến trình cài, cập nhật: giải nén, kiểm tra, đăng ký lệnh; có nút Hủy
  function plugProg(title, p, done){ var k = 0, steps = ['Giải nén gói', 'Kiểm tra chữ ký và tương thích SDK ' + (p.sdk || '1.0'), 'Đăng ký ' + p.cmds.length + ' lệnh, tệp tùy biến'], stop = false;
    var dlg = TC.dialog({ id:'plprog', title:title, code:'UI-K10', w:460, help:false, onClose:function(){ stop = true; }, body:'<div class="stack"><b>' + esc(p.name) + ' ' + esc(p.ver) + '</b><div class="prog"><i id="pg-b" style="width:0%"></i></div><span class="faint" id="pg-t" style="font-size:var(--fs-s)"></span></div>', foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button>' });
    (function tick(){ if (stop || dlg.closed){ TC.toast('Đã hủy. Không có thay đổi nào.', { type:'info', log:false }); return; } $('#pg-b', dlg.el).style.width = (k / steps.length * 100) + '%'; $('#pg-t', dlg.el).textContent = k < steps.length ? steps[k] + '…' : 'Xong'; if (k++ < steps.length) setTimeout(tick, 480); else { dlg.close('done'); stop = true; done(); } })(); }
  SC.plugins = function(selId){
    if (!TC.can('PRO')){ SC.upgrade('Quản lý plugin', 'PRO'); return; }
    var U = { sel:selId || (PLUG[0] || {}).id, q:'', f:'all', sort:'name', busy:{} };
    var dlg = TC.dialog({ id:'plug', title:'Quản lý plugin', cmd:'PLUGINS', code:'UI-K10', w:980, h:620, flush:true,
      body:'<div class="plw"><div class="pal-tools"><label class="search" style="width:250px">' + ic('search') + '<input id="pl-q" placeholder="Tìm plugin, nhà phát triển, lệnh" aria-label="Tìm plugin" spellcheck="false"></label><span class="seg" id="pl-f" role="group" aria-label="Lọc"></span><span class="grow"></span><button class="btn sm" type="button" data-pl="check">' + ic('update') + 'Kiểm tra cập nhật</button><button class="btn sm primary" type="button" data-pl="install">' + ic('pkg') + 'Cài từ tệp gói…</button></div><div class="plmain"><div class="tblwrap" id="pl-t"></div><div class="pld" id="pl-d"></div></div></div>',
      foot:'<span class="faint pl-ft">' + ic('shield', 's12') + 'Plugin chạy trong TrionCAD với quyền khai báo trong gói. Chỉ cài plugin từ nguồn bạn tin cậy.</span><span class="grow"></span><button class="btn primary" type="button" data-x="close">Đóng</button>' });
    var el = dlg.el;
    function att(p){ return p.st !== 'ok' || p.up || !p.signed; }
    function list(){ var q = TC.fold(U.q.trim()); return PLUG.filter(function(p){ return (U.f === 'all' || (U.f === 'on' ? plugRun(p) : att(p))) && (!q || TC.fold(p.name + ' ' + p.dev + ' ' + p.cmds.map(function(c){ return c[0] + ' ' + c[1]; }).join(' ')).indexOf(q) >= 0); }).sort(function(a, b){ return U.sort === 'st' ? (att(b) ? 1 : 0) - (att(a) ? 1 : 0) || a.name.localeCompare(b.name, 'vi') : a.name.localeCompare(b.name, 'vi'); }); }
    function rT(){ var l = list();
      $('#pl-f', el).innerHTML = [['all', 'Tất cả', PLUG.length], ['on', 'Đang chạy', PLUG.filter(plugRun).length], ['att', 'Cần chú ý', PLUG.filter(att).length]].map(function(f){ return '<button type="button" data-plf="' + f[0] + '" aria-pressed="' + (U.f === f[0]) + '">' + f[1] + ' (' + f[2] + ')</button>'; }).join('');
      $('#pl-t', el).innerHTML = !PLUG.length ? '<div class="empty plempty">' + ic('pkg') + '<b>Chưa cài plugin nào</b>Plugin thêm lệnh, công cụ chuyên ngành cho TrionCAD: kết cấu, giao thông, điện nước, dự toán…<div class="hrow" style="justify-content:center;margin-top:6px"><button class="btn sm primary" type="button" data-pl="install">' + ic('pkg') + 'Cài từ tệp gói…</button><button class="btn sm" type="button" data-pl="web">' + ic('ext') + 'Tìm plugin trên trang TrionCAD</button></div></div>'
        : !l.length ? '<div class="empty">' + ic('search') + '<b>Không có plugin khớp</b>Thử tên khác, hoặc chọn bộ lọc “Tất cả”.</div>'
        : '<table class="tbl plt"><colgroup><col style="width:44px"><col><col style="width:82px"><col style="width:112px"><col style="width:150px"></colgroup><thead><tr><th class="ctr">Bật</th><th class="sortable" data-pls="name" aria-sort="' + (U.sort === 'name' ? 'ascending' : 'none') + '">Plugin' + (U.sort === 'name' ? ' ↑' : '') + '</th><th>Phiên bản</th><th>Chữ ký</th><th class="sortable" data-pls="st" aria-sort="' + (U.sort === 'st' ? 'descending' : 'none') + '">Trạng thái' + (U.sort === 'st' ? ' ↓' : '') + '</th></tr></thead><tbody>' + l.map(function(p){ return '<tr data-plr="' + p.id + '" class="' + (p.id === U.sel ? 'is-sel' : '') + '"><td class="ctr"><input type="checkbox" data-plon="' + p.id + '"' + (p.on ? ' checked' : '') + (p.st === 'incompat' ? ' disabled title="Không tương thích: không bật được"' : '') + ' aria-label="Bật ' + esc(p.name) + '"></td><td class="two"><b>' + esc(p.name) + '</b><span class="faint">' + esc(p.dev) + '</span></td><td class="mono">' + esc(p.ver) + '</td><td>' + sigTag(p) + '</td><td>' + (U.busy[p.id] ? '<span class="tag info">' + ic('clock') + esc(U.busy[p.id]) + '</span>' : stTag(p) + (p.up ? ' <span class="tag info">' + ic('update') + p.up + '</span>' : '')) + '</td></tr>'; }).join('') + '</tbody></table>';
      rD(); }
    function rD(){ var p = plugOf(U.sel), box = $('#pl-d', el);
      if (!p){ box.innerHTML = '<div class="empty">' + ic('pkg') + '<b>Plugin cho TrionCAD</b>Dựng bằng SDK C/C++ của TrionCAD, đóng gói .tcplugin. Chọn một plugin để xem quyền và lệnh.</div>'; return; }
      var b = U.busy[p.id], blk = p.st === 'err' ? '<div class="note err">' + ic('error', 'xs') + '<div class="stack" style="gap:6px"><b>Không nạp được plugin</b><span>' + esc(p.err) + '</span><div class="hrow"><button class="btn sm" type="button" data-pl="retry"' + (b ? ' disabled' : '') + '>' + ic('refresh') + 'Thử nạp lại</button><button class="btn sm ghost" type="button" data-pl="folder">' + ic('folder') + 'Mở thư mục plugin</button></div><details><summary>Nhật ký nạp</summary><pre class="mono">' + esc((p.logl || []).join('\n')) + '</pre></details></div></div>'
        : p.st === 'incompat' ? '<div class="note warn">' + ic('warn', 'xs') + '<div class="stack" style="gap:6px"><b>Không tương thích với bản TrionCAD này</b><span>' + esc(p.inc) + '</span><div class="hrow"><button class="btn sm" type="button" data-pl="findup">' + ic('update') + 'Tìm bản cập nhật</button></div></div></div>'
        : !p.on ? '<div class="note">' + ic('info', 'xs') + '<span>Đã tắt: lệnh của plugin không chạy, toolbar đi kèm ẩn. Bật lại không cần khởi động lại TrionCAD.</span></div>'
        : '<div class="note acc">' + ic('okc', 'xs') + '<span>Đang chạy · nạp trong ' + p.ms + ' ms, ' + p.cmds.length + ' lệnh sẵn sàng.</span></div>';
      if (p.up && p.st !== 'incompat') blk += '<div class="note">' + ic('update', 'xs') + '<div class="stack" style="gap:6px"><span><b>Có bản ' + esc(p.up) + '</b>: ' + esc(p.upNote || 'sửa lỗi, cải thiện') + '.</span><div class="hrow"><button class="btn sm primary" type="button" data-pl="update"' + (b ? ' disabled' : '') + '>' + ic('update') + 'Cập nhật lên ' + esc(p.up) + '</button></div></div></div>';
      box.innerHTML = '<div class="plh">' + ic('pkg', 'lg') + '<div><b>' + esc(p.name) + '</b> <span class="mono faint">' + esc(p.ver) + '</span><div class="faint">' + esc(p.dev) + '</div></div></div><p class="pldesc">' + esc(p.desc) + '</p>' + blk +
        '<div class="gh">Quyền</div><div class="stack" style="gap:2px">' + permList(p, !p.signed) + '</div>' +
        '<div class="gh">Lệnh thêm vào · ' + p.cmds.length + '</div><div class="stack" style="gap:2px">' + p.cmds.map(function(c){ return '<div class="plc"><span class="mono">' + esc(c[0]) + '</span><span class="grow">' + esc(c[1]) + '</span>' + (plugRun(p) ? '<button class="btn sm ghost" type="button" data-plrun="' + esc(c[0]) + '">Chạy</button>' : '') + '</div>'; }).join('') + (p.tb && plugRun(p) ? '<div class="faint" style="font-size:var(--fs-xs)">Toolbar “' + esc(p.tb) + '” (workspace Cổ điển) từ tệp ' + esc(p.tcui) + '.</div>' : '') + '</div>' +
        '<div class="gh">Thông tin</div><dl class="kv pl-kv"><dt>Chữ ký số</dt><dd>' + (p.signed ? esc(p.cert || 'Hợp lệ') : '<span style="color:var(--c-warn)">Không có · bạn đã chọn tin cậy khi cài</span>') + '</dd><dt>SDK</dt><dd>' + esc(p.sdk) + (p.sdk !== '1.0' ? ' <span class="faint">(cần 1.0)</span>' : '') + '</dd><dt>Cài ngày</dt><dd>' + esc(p.date) + '</dd><dt>Dung lượng</dt><dd>' + esc(p.size) + '</dd><dt>Thư mục</dt><dd class="mono" style="font-size:var(--fs-xs)">C:\\ProgramData\\TrionCAD\\Plugins\\' + esc(p.dir) + '</dd></dl>' +
        '<div class="hrow pl-acts">' + (p.st === 'incompat' ? '' : '<button class="btn sm" type="button" data-pl="toggle">' + ic(p.on ? 'pause' : 'play') + (p.on ? 'Tắt plugin' : 'Bật plugin') + '</button>') + '<button class="btn sm" type="button" data-pl="remove">' + ic('trash') + 'Gỡ…</button></div>'; }
    function setOn(p, on){ p.on = on; plugApply(); rT(); TC.toast((on ? 'Đã bật ' : 'Đã tắt ') + '<b>' + esc(p.name) + '</b>: ' + p.cmds.length + ' lệnh ' + (on ? (p.st === 'ok' ? 'chạy được' : 'chờ plugin nạp được') : 'ngừng chạy') + '.', { log:false }); }
    function install(){ var sel = PKG[0].file;
      var d2 = TC.dialog({ id:'plfile', title:'Cài plugin từ tệp gói', code:'UI-K10', w:600,
        body:'<div class="stack">' + H.row('Thư mục', '<span class="mono">C:\\Users\\An\\Downloads</span>', 70) + '<div class="tblwrap" style="height:150px"><table class="tbl" id="pf-t"><colgroup><col><col style="width:96px"><col style="width:76px"></colgroup><thead><tr><th>Tệp gói (.tcplugin)</th><th>Ngày</th><th class="num">Cỡ</th></tr></thead><tbody>' + PKG.map(function(k){ return '<tr data-pf="' + esc(k.file) + '" tabindex="-1"><td>' + ic('pkg', 's12') + ' ' + esc(k.file) + '</td><td>' + k.date + '</td><td class="num">' + k.size + '</td></tr>'; }).join('') + '</tbody></table></div><span class="fmsg" id="pf-m"></span></div>',
        foot:'<span class="grow"></span><button class="btn ghost" type="button" data-x="cancel">Hủy</button><button class="btn primary" type="button" id="pf-ok">Cài gói</button>' });
      function paint(){ $$('[data-pf]', d2.el).forEach(function(r){ r.classList.toggle('is-sel', r.dataset.pf === sel); }); var m = $('#pf-m', d2.el); m.className = 'fmsg'; m.innerHTML = ''; }
      function go(){ var k = PKG.filter(function(x){ return x.file === sel; })[0], m = $('#pf-m', d2.el);
        if (k.broken){ m.className = 'fmsg err'; m.innerHTML = ic('warn', 'xs') + esc(k.broken); return; }
        if (k.upd){ var cur = plugOf(k.upd); if (!cur){ m.className = 'fmsg err'; m.innerHTML = ic('warn', 'xs') + 'Gói cập nhật cho plugin chưa cài. Cài bản đầy đủ trước.'; return; } if (cur.ver === k.ver){ m.className = 'fmsg hint'; m.innerHTML = 'Đã cài ' + esc(cur.name) + ' ' + esc(k.ver) + '.'; return; } d2.close(true); doUpdate(cur, k.file); return; }
        if (plugOf(k.p.id)){ m.className = 'fmsg hint'; m.innerHTML = 'Đã cài ' + esc(k.p.name) + ' ' + esc(k.p.ver) + '.'; return; }
        d2.close(true); plugTrust(k.p, k.file).then(function(ok){ if (!ok) return; plugProg('Đang cài plugin', k.p, function(){ var np = Object.assign(clone(k.p), { date:TC.date() }); PLUG.push(np); plugApply(); U.sel = np.id; U.f = 'all'; if (!dlg.closed) rT(); TC.toast('Đã cài <b>' + esc(np.name) + '</b> ' + esc(np.ver) + '. Lệnh mới: <b class="mono">' + np.cmds.map(function(c){ return c[0]; }).join(', ') + '</b>.', { act:{ label:'Chạy ' + np.cmds[0][0], fn:function(){ if (!dlg.closed) dlg.close(); TC.cmd.run(np.cmds[0][0]); } } }); }); }); }
      d2.el.addEventListener('click', function(e){ var r = e.target.closest('[data-pf]'); if (r && r.tagName === 'TR'){ sel = r.dataset.pf; paint(); } });
      d2.el.addEventListener('dblclick', function(e){ if (e.target.closest('tr[data-pf]')) go(); });
      TC.listKeys($('#pf-t', d2.el), 'tr[data-pf]', function(r, enter){ sel = r.dataset.pf; paint(); if (enter) go(); });
      $('#pf-ok', d2.el).addEventListener('click', go); paint(); }
    function doUpdate(p, file){ var nv = p.up || '2.4.0'; plugTrust(p, file || (PKG.filter(function(k){ return k.upd === p.id; })[0] || {}).file || p.dir.toLowerCase() + '-' + nv + '.tcplugin', { ver:nv, from:p.ver }).then(function(ok){ if (!ok) return; plugProg('Đang cập nhật plugin', Object.assign({}, p, { ver:nv }), function(){ var from = p.ver; p.ver = nv; delete p.up; delete p.upNote; p.date = TC.date(); plugApply(); if (!dlg.closed) rT(); TC.toast('Đã cập nhật <b>' + esc(p.name) + '</b> từ ' + esc(from) + ' lên ' + esc(nv) + '.', { log:false }); }); }); }
    el.addEventListener('click', function(e){
      var f = e.target.closest('[data-plf]'); if (f){ U.f = f.dataset.plf; rT(); return; }
      var s = e.target.closest('[data-pls]'); if (s){ U.sort = s.dataset.pls; rT(); return; }
      var on = e.target.closest('[data-plon]'); if (on) return;
      var r = e.target.closest('[data-plr]'); if (r){ U.sel = r.dataset.plr; rT(); return; }
      var rn = e.target.closest('[data-plrun]'); if (rn){ dlg.close(); TC.cmd.run(rn.dataset.plrun); return; }
      var b = e.target.closest('[data-pl]'); if (!b || b.disabled) return; var k = b.dataset.pl, p = plugOf(U.sel);
      if (k === 'install') install();
      else if (k === 'web') TC.toast('Mở trang Plugin cho TrionCAD trên trình duyệt (danh sách plugin đã kiểm duyệt).', { type:'info', log:false });
      else if (k === 'check'){ b.disabled = true; b.innerHTML = ic('clock') + 'Đang kiểm tra…'; setTimeout(function(){ if (dlg.closed) return; b.disabled = false; b.innerHTML = ic('update') + 'Kiểm tra cập nhật'; var ups = PLUG.filter(function(x){ return x.up; }); TC.toast(ups.length ? 'Đã kiểm tra ' + PLUG.length + ' plugin: ' + ups.map(function(x){ return '<b>' + esc(x.name) + '</b> có bản ' + esc(x.up); }).join(', ') + '.' : 'Đã kiểm tra ' + PLUG.length + ' plugin: tất cả là bản mới nhất.', { type:'info', log:false }); if (ups.length){ U.f = 'att'; rT(); } }, 1100); }
      else if (!p) return;
      else if (k === 'toggle') setOn(p, !p.on);
      else if (k === 'update') doUpdate(p);
      else if (k === 'retry'){ U.busy[p.id] = 'Đang nạp…'; rT(); setTimeout(function(){ delete U.busy[p.id]; if (dlg.closed) return; p.logl = (p.logl || []).concat([TC.date() + ' ' + new Date().toTimeString().slice(0, 8) + '  thử lại: vẫn không tìm thấy an-phu-core.dll']); plugSave(); rT(); TC.toast('Vẫn thiếu <span class="mono">an-phu-core.dll</span>. Khôi phục tệp từ phần mềm diệt virus hoặc cài lại plugin.', { type:'warn', log:false }); }, 1000); }
      else if (k === 'folder') TC.toast('Mở thư mục C:\\ProgramData\\TrionCAD\\Plugins\\' + esc(p.dir) + ' trong File Explorer.', { type:'info', log:false });
      else if (k === 'findup') TC.toast(esc(p.dev) + ' chưa phát hành bản cho SDK 1.0. TrionCAD sẽ báo khi có bản mới.', { type:'info', log:false });
      else if (k === 'remove') TC.confirm({ title:'Gỡ plugin', msg:'Gỡ <b>' + esc(p.name) + '</b> ' + esc(p.ver) + ' và ' + p.cmds.length + ' lệnh (' + esc(p.cmds.map(function(c){ return c[0]; }).join(', ')) + ')?', detail:'Xóa thư mục plugin' + (p.tcui ? ' và tệp tùy biến ' + esc(p.tcui) : '') + '. Bản vẽ đã tạo bằng plugin không bị ảnh hưởng.', ok:'Gỡ plugin', danger:true }).then(function(ok){ if (!ok) return; var i = PLUG.indexOf(p); PLUG.splice(i, 1); plugApply(); U.sel = (PLUG[Math.min(i, PLUG.length - 1)] || {}).id; if (!dlg.closed) rT(); TC.toast('Đã gỡ <b>' + esc(p.name) + '</b>.', { act:{ label:'Hoàn tác', fn:function(){ PLUG.splice(i, 0, p); plugApply(); if (!dlg.closed){ U.sel = p.id; rT(); } } } }); });
    });
    el.addEventListener('change', function(e){ var c = e.target.closest('[data-plon]'); if (c){ var p = plugOf(c.dataset.plon); U.sel = p.id; setOn(p, c.checked); } });
    $('#pl-q', el).addEventListener('input', function(){ U.q = this.value; rT(); });
    TC.listKeys($('#pl-t', el), '[data-plr]', function(r){ U.sel = r.dataset.plr; rT(); var n = $('[data-plr="' + U.sel + '"]', el); if (n) n.scrollIntoView({ block:'nearest' }); });
    rT(); return dlg;
  };
  IMPL.PLUGINS = function(){ SC.plugins(); };
  OPEN['UI-K10'] = function(){ SC.cat.PRO(); SC.plugins('damsan'); };
})();
