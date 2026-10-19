#!/usr/bin/env python3
"""Kiểm bộ task docs/08-phan-ra-task: cú pháp, phụ thuộc, chu trình, mốc, neo, độ phủ.

Chạy: python3 tools/check_tasks.py [-v] [--json tệp.json]. Mã thoát 1 nếu có LỖI."""
import re, sys, glob, os, json
from collections import defaultdict, Counter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR = os.path.join(ROOT, 'docs/08-phan-ra-task')
ID = r'[A-Z][A-Z0-9]{1,3}-\d{3}'
UICODE = r'UI-(?:[A-N]\d{2}|MAC|MR2|MR|PAR)\b'
RANK = {**{f'S{i}': 0 for i in range(1, 7)}, 'CC': 0, 'V': 1, 'VP': 2, 'EA': 3, '1.0': 4, 'Sau': 5}
P0 = ('NEW OPEN SAVE SAVEAS QSAVE CLOSE RECOVER AUDIT PURGE UNITS OPTIONS ZOOM PAN REGEN REDRAW VIEWRES '
      'LINE XLINE PLINE RECTANG POLYGON CIRCLE ARC ELLIPSE SPLINE POINT DDPTYPE DIVIDE MEASURE BOUNDARY HATCH HATCHEDIT '
      'ERASE OOPS MOVE COPY ROTATE SCALE MIRROR STRETCH OFFSET TRIM EXTEND BREAK BREAKATPOINT JOIN FILLET CHAMFER EXPLODE '
      'LENGTHEN ARRAYRECT ARRAYPOLAR MATCHPROP PEDIT DRAWORDER UNDO U REDO COPYCLIP CUTCLIP PASTECLIP SELECT QSELECT '
      'SELECTSIMILAR TEXT MTEXT STYLE TEXTEDIT FIND DIMLINEAR DIMALIGNED DIMANGULAR DIMRADIUS DIMDIAMETER DIMCONTINUE '
      'DIMBASELINE DIMSTYLE DIMEDIT DIMTEDIT MLEADER LAYER LAYISO LAYUNISO LAYOFF LAYON LAYFRZ LAYTHW LAYMCUR LAYMCH COLOR '
      'LINETYPE LTSCALE LWEIGHT PROPERTIES BLOCK INSERT WBLOCK BASE RENAME ATTDEF EATTEDIT ATTSYNC ATTDISP XREF XATTACH '
      'XBIND XCLIP IMAGEATTACH LAYOUT MVIEW VPORTS PAGESETUP PLOT PREVIEW EXPORTPDF STYLESMANAGER DIST AREA LIST ID '
      'MEASUREGEOM SETVAR SCRIPT APPLOAD FIXFONT').split()

def ui_codes():
    src = open(os.path.join(ROOT, 'design/prototype/js/catalog.js'), encoding='utf-8').read()
    raw = re.search(r"var RAW = (.*?);\n", src, re.S).group(1)
    codes = sorted(set('UI-' + c for c in re.findall(r"[';]([A-N]\d{2})\|", raw)))
    return codes + ['UI-MAC', 'UI-MR', 'UI-PAR', 'UI-MR2']

def anchors():
    txt = open(os.path.join(DIR, 'README.md'), encoding='utf-8').read()
    sec = txt.split('## 5. Task neo', 1)[1].split('\n## 6.', 1)[0]
    return re.findall(rf'^\| ({ID}) \|', sec, re.M)

def parse(path):
    tasks, errs = [], []
    lines = open(path, encoding='utf-8').read().split('\n')
    cur = None
    for n, line in enumerate(lines, 1):
        if line.startswith('### '):
            m = re.match(rf'^### ({ID}) · (.+)$', line)
            if not m:
                errs.append(f'{os.path.basename(path)}:{n}: tiêu đề sai mẫu: {line[:80]}')
                cur = None
                continue
            cur = dict(id=m.group(1), title=m.group(2), file=os.path.basename(path), line=n, body=[])
            tasks.append(cur)
        elif line.startswith('## '):
            cur = None
        elif cur is not None:
            cur['body'].append(line)
    for t in tasks:
        b = '\n'.join(t['body'])
        t['text'] = b
        m = re.search(r'^- \*\*Mốc:\*\* (.+?) · \*\*Ưu tiên:\*\* (.+?) · \*\*Cỡ:\*\* (.+?) · \*\*Phụ thuộc:\*\* (.*)$', b, re.M)
        if not m:
            errs.append(f"{t['file']}:{t['line']}: {t['id']} thiếu/sai dòng Mốc·Ưu tiên·Cỡ·Phụ thuộc")
            t.update(mmoc=None, prio=None, size=None, deps_raw='')
        else:
            t.update(mmoc=m.group(1).strip(), prio=m.group(2).strip(), size=m.group(3).strip(), deps_raw=m.group(4).strip())
            if t['mmoc'] not in RANK: errs.append(f"{t['file']}:{t['line']}: {t['id']} Mốc lạ: {t['mmoc']}")
            if t['prio'] not in ('P0', 'P1', 'P2'): errs.append(f"{t['file']}:{t['line']}: {t['id']} Ưu tiên lạ: {t['prio']}")
            if t['size'] not in ('S', 'M', 'L'): errs.append(f"{t['file']}:{t['line']}: {t['id']} Cỡ lạ: {t['size']}")
        mc = re.search(r'^- \*\*Lệnh:\*\* (.+)$', b, re.M)
        t['cmds'] = [c.strip(' `*.').upper() for c in re.split(r'[,;]', mc.group(1))] if mc else []
        t['cmds'] = [re.sub(r'\s*\(.*$', '', c) for c in t['cmds'] if c]
        ms = re.search(r'^- \*\*Màn hình:\*\* (.+)$', b, re.M)
        t['screens'] = re.findall(UICODE, ms.group(1)) if ms else []
        for f in ('Đầu vào', 'Đầu ra', 'Tiêu chí nghiệm thu'):
            if f'- **{f}:**' not in b:
                errs.append(f"{t['file']}:{t['line']}: {t['id']} thiếu trường {f}")
        ac = b.split('- **Tiêu chí nghiệm thu:**', 1)[1] if '- **Tiêu chí nghiệm thu:**' in b else ''
        ac = re.split(r'\n- \*\*', ac)[0]
        t['ac'] = ac
        d = t['deps_raw']
        t['dep_ids'] = re.findall(rf'(?<![A-Z0-9:]){ID}', d)
        t['dep_cmds'] = [x.upper() for x in re.findall(r'CMD:([A-Za-z0-9_\-\.]+)', d)]
        t['dep_uis'] = re.findall(UICODE, d)
    return tasks, errs

def main():
    files = sorted(f for f in glob.glob(os.path.join(DIR, '*.md')) if not f.endswith('README.md'))
    tasks, errs, warns = [], [], []
    for f in files:
        ts, es = parse(f); tasks += ts; errs += es
    by_id = {}
    for t in tasks:
        if t['id'] in by_id: errs.append(f"Trùng mã {t['id']}: {by_id[t['id']]['file']} và {t['file']}")
        by_id[t['id']] = t
    cmd_owner = defaultdict(list); ui_owner = defaultdict(list)
    for t in tasks:
        for c in t['cmds']: cmd_owner[c].append(t['id'])
        for s in t['screens']: ui_owner[s].append(t['id'])
    # phụ thuộc
    graph = defaultdict(set); minrank = {}
    for t in tasks:
        r = RANK.get(t['mmoc'], 9)
        for d in t['dep_ids']:
            if d not in by_id: errs.append(f"{t['id']}: phụ thuộc {d} không tồn tại"); continue
            if d == t['id']: errs.append(f"{t['id']}: tự phụ thuộc"); continue
            graph[t['id']].add(d)
            if RANK.get(by_id[d]['mmoc'], 9) > r:
                warns.append(f"MỐC {t['id']} ({t['mmoc']}) phụ thuộc {d} ({by_id[d]['mmoc']})")
        for c in t['dep_cmds']:
            own = [o for o in cmd_owner.get(c, []) if o != t['id']]
            if not own: errs.append(f"{t['id']}: CMD:{c} không task nào khai báo **Lệnh:**"); continue
            graph[t['id']].update(own)
            if min(RANK.get(by_id[o]['mmoc'], 9) for o in own) > r:
                warns.append(f"MỐC {t['id']} ({t['mmoc']}) phụ thuộc CMD:{c} (sớm nhất {min(by_id[o]['mmoc'] for o in own)})")
        for u in t['dep_uis']:
            own = [o for o in ui_owner.get(u, []) if o != t['id']]
            if not own: errs.append(f"{t['id']}: {u} không task UI nào có **Màn hình:**"); continue
            if min(RANK.get(by_id[o]['mmoc'], 9) for o in own) > r:
                warns.append(f"MỐC {t['id']} ({t['mmoc']}) phụ thuộc {u}")
    # chu trình (chỉ trên phụ thuộc theo mã và CMD)
    color = {}; cycles = []
    sys.setrecursionlimit(100000)
    def dfs(u, stack):
        color[u] = 1; stack.append(u)
        for v in sorted(graph[u]):
            if color.get(v) == 1:
                cycles.append(stack[stack.index(v):] + [v])
            elif not color.get(v):
                dfs(v, stack)
        stack.pop(); color[u] = 2
    for u in sorted(by_id):
        if not color.get(u): dfs(u, [])
    for c in cycles[:30]: errs.append('Chu trình: ' + ' → '.join(c))
    # neo
    anc = anchors()
    for a in anc:
        if a not in by_id: errs.append(f'Thiếu task neo {a}')
    # độ phủ
    uis = ui_codes()
    miss_ui = [u for u in uis if u not in ui_owner]
    miss_p0 = [c for c in P0 if c not in cmd_owner]
    # cảnh báo nội dung
    for t in tasks:
        if 'DoD-' not in t['ac']: warns.append(f"DOD {t['id']}: tiêu chí không nhắc DoD")
        if not re.search(r'\d', t['ac'].replace('DoD', '')): warns.append(f"SỐ {t['id']}: tiêu chí không có con số nào")
        if t['file'].startswith('ui-') and not t['screens']: errs.append(f"{t['id']}: task UI thiếu **Màn hình:**")
    dup_cmd = {c: o for c, o in cmd_owner.items() if len(o) > 1}
    # báo cáo
    print(f'Tệp: {len(files)} · Task: {len(tasks)} · Neo: {len(anc)}')
    per_file = Counter(t['file'] for t in tasks)
    for f in sorted(per_file): print(f'  {f}: {per_file[f]}')
    print('Theo mốc:', dict(sorted(Counter(t["mmoc"] for t in tasks).items(), key=lambda kv: RANK.get(kv[0], 9))))
    print('Theo cỡ:', dict(Counter(t['size'] for t in tasks)), '· Ưu tiên:', dict(Counter(t['prio'] for t in tasks)))
    print(f'Mã màn hình: {len(uis) - len(miss_ui)}/{len(uis)} có task' + (f' · thiếu: {" ".join(miss_ui)}' if miss_ui else ''))
    print(f'Lệnh P0 (03 Phụ lục A): {len(P0) - len(miss_p0)}/{len(P0)}' + (f' · thiếu: {" ".join(miss_p0)}' if miss_p0 else ''))
    print(f'Lệnh khai báo: {len(cmd_owner)} · khai báo ở nhiều task: {len(dup_cmd)}')
    if '-v' in sys.argv:
        for c, o in sorted(dup_cmd.items()): print(f'  LỆNH {c}: {", ".join(o)}')
    print(f'LỖI: {len(errs)}'); [print('  ' + e) for e in errs[:400]]
    wc = Counter(w.split(' ', 1)[0] for w in warns)
    print(f'CẢNH BÁO: {len(warns)} {dict(wc)}'); [print('  ' + w) for w in warns[:400] if '-v' in sys.argv or not w.startswith(('DOD', 'SỐ'))]
    if '--json' in sys.argv:
        json.dump({t['id']: dict(file=t['file'], title=t['title'], moc=t['mmoc'], prio=t['prio'], size=t['size'], deps=t['deps_raw'],
                                 cmds=t['cmds'], screens=t['screens']) for t in tasks},
                  open(sys.argv[sys.argv.index('--json') + 1], 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    sys.exit(1 if errs else 0)

main()
