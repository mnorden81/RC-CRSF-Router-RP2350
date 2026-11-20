#!/usr/bin/env python3
# Pruefstand WebUI + Lua: baut pruefstand/pruefstand.html aus dem Firmware-Code.
#
#   python3 pruefstand/build.py           Seite neu bauen
#   python3 pruefstand/build.py --check   nur pruefen (CI): Seite aktuell,
#                                          Lua-Menue lesbar, Simulation kennt
#                                          alle API-Routen und JSON-Felder
#
# Die Weboberflaeche ist die echte Seite aus der Firmware (webui.h bzw.
# WebServerManager.cpp), das Lua-Menue wird aus buildLuaMenu() gelesen.
# Nur die Antworten des Moduls (Status, Konfiguration ...) kommen aus der
# Simulation in pruefstand/mock_*.js. Gleiche Datei in allen Repos.
import glob, json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from luaparse import run_builder, parse_menu, preprocess, strip_comments, Env, collect, eval_init_list, Unknown, LuaParseError  # noqa: E402

BUILD_VERSION = '1.0'


def rd(p):
    with open(p, encoding='utf-8', errors='replace') as f:
        return f.read()


def files_of(spec, base):
    out = []
    for pat in spec:
        if pat.startswith('!'):
            ex = set(glob.glob(os.path.join(base, pat[1:])))
            out = [h for h in out if h not in ex]
            continue
        hits = sorted(glob.glob(os.path.join(base, pat)))
        if not hits and not any(c in pat for c in '*?['):
            raise SystemExit('Datei fehlt: ' + os.path.join(base, pat))
        out += [h for h in hits if h not in out]
    return out


def raw_string(text, var):
    """Inhalt des Rohstrings  <var>[] ... = R"X( ... )X";"""
    i = text.index(var)
    m = re.compile(r'R"([^(\s]*)\(').search(text, i)
    d = m.group(1)
    a = m.end()
    b = text.index(')' + d + '"', a)
    return text[a:b]


RC_ROLE_BAUD_CACHE = {}


def rc_common_js(rc):
    wp = os.path.join(rc, 'web_common.cpp')
    js = raw_string(rd(wp), 'RC_COMMON_JS') if os.path.exists(wp) else ''   # RP2350: ohne WebUI
    ver = re.search(r'#define\s+RC_COMMON_VERSION\s+"([^"]+)"', rd(os.path.join(rc, 'rc_common.h'))).group(1)
    ur = rd(os.path.join(rc, 'uart_roles.cpp'))
    roles = re.search(r'uartRoleOptions\(\)\s*\{\s*return\s*"([^"]*)"', ur).group(1)
    bauds = re.search(r'uartBaudOptions\(\)\s*\{\s*return\s*"([^"]*)"', ur).group(1)
    urh = rd(os.path.join(rc, 'uart_roles.h'))
    auto = int(re.search(r'UART_BAUD_AUTO\s*=\s*(\d+)', urh).group(1))
    lst = re.search(r'UART_BAUD_LIST\[[^\]]*\]\s*=\s*\{([^}]*)\}', ur).group(1)
    blist = [auto if x.strip() == 'UART_BAUD_AUTO' else int(x) for x in lst.split(',')]
    # Standard-Baudrate je Rolle (switch in uartRoleDefaultBaud)
    sw = re.search(r'uartRoleDefaultBaud\([^)]*\)\s*\{(.*?)\n\}', ur, re.S).group(1)
    names = re.findall(r'UROLE_(\w+)\s*=\s*(\d+)', urh)
    order = sorted(((int(v), k) for k, v in names if k != 'COUNT'))
    rb = []
    for _, k in order:
        m = re.search(r'case\s+UROLE_' + k + r'\s*:\s*return\s+(\w+)', sw)
        v = m.group(1) if m else re.search(r'default\s*:\s*return\s+(\w+)', sw).group(1)
        rb.append(auto if v == 'UART_BAUD_AUTO' else int(v))
    RC_ROLE_BAUD_CACHE[rc] = rb
    head = ("var RC_COMMON_VERSION='%s';var RC_ROLES='%s';var RC_BAUDS='%s';var RC_BAUD_AUTO=%d;"
            "var RC_BAUD_VALUES=[%s];var RC_ROLE_BAUD=[%s];\n") % (
        ver, roles, bauds, auto, ','.join(map(str, blist)), ','.join(map(str, rb)))
    return head + js, ver, roles, bauds


def version_text(env, spec):
    if not spec:
        return ''
    try:
        v = env.lookup(spec)
    except Unknown:
        return ''
    if isinstance(v, int):
        return '%d.%02d' % (v // 100, v % 100)
    return str(v)


def export_consts(env, names, errs):
    out = {}
    for n in names:
        opt = n.endswith('?')
        n = n.rstrip('?')
        try:
            out[n] = env.lookup(n)
        except Unknown:
            if not opt: errs.append('Konstante %s nicht lesbar' % n)
    return out


def build_firmware(fw):
    base = os.path.join(ROOT, fw['dir'])
    rc = os.path.join(base, fw.get('rc_common', 'src/RC_Common'))
    out = {'id': fw['id'], 'name': fw['name'], 'mock': fw['mock'], 'variants': [], 'warn': []}
    full = os.path.exists(os.path.join(rc, 'uart_roles.cpp'))   # RP2350: nur Teile von RC_Common
    rcjs, rcver, roles, bauds = rc_common_js(rc) if full else ('', '', '', '')
    out['rc_common'] = rcver
    # Weboberflaeche
    if fw.get('page'):
        page = raw_string(rd(os.path.join(base, fw['page']['file'])), fw['page']['var'])
        page = page.replace('<script src="/rc_common.js"></script>', '<script>' + rcjs + '</script>')
        out['page'] = page
    # Lua je Variante
    for var in fw['variants']:
        v = {'id': var['id'], 'name': var['name'], 'hint': var.get('hint', '')}
        if fw.get('lua'):
            L = fw['lua']
            files = files_of(L['files'], base)
            over = {'uartRoleOptions()': roles, 'uartBaudOptions()': bauds}
            over.update(L.get('overrides', {}))
            over.update(var.get('overrides', {}))
            menu, env = parse_menu(files, L.get('entry', 'buildLuaMenu'), var.get('defines', {}), over)
            ver = version_text(env, fw.get('version'))
            items = []
            for it in menu.items:
                d = {k: it.get(k) for k in ('id', 'parent', 'type', 'name', 'text', 'min', 'max', 'unit', 'view', 'arg', 'ro', 'get', 'set')}
                if d['text'] is None and it['type'] in ('info', 'select'):
                    d['text'] = ''
                d['runtime'] = (it['type'] in ('info', 'select') and it.get('text') is None)
                items.append(d)
            ids = [i['id'] for i in items]
            dup = sorted({x for x in ids if ids.count(x) > 1})
            gaps = sorted(set(range(max(ids) + 1)) - set(ids))
            errs = []
            if dup: errs.append('doppelte Feld-IDs %s' % dup)
            if gaps and not L.get('allow_gaps'): errs.append('Luecken in den Feld-IDs %s' % gaps)
            exp = L.get('expect_max')
            if exp:
                try:
                    e = env.lookup(exp) if isinstance(exp, str) else exp
                    if e != max(ids): errs.append('hoechste Feld-ID %d, %s = %s' % (max(ids), exp, e))
                except Unknown:
                    errs.append('%s nicht lesbar' % exp)
            allowed = set(L.get('allow_warnings', []))
            warn = sorted({w for w in menu.warn if not any(a in w for a in allowed)})
            v.update({'items': items, 'version': ver, 'lua_errors': errs + warn})
            v['consts'] = export_consts(env, fw.get('export', []), v['lua_errors'])
        else:
            v.update({'items': [], 'version': '', 'lua_errors': []})
        if not fw.get('lua'):
            # ohne Lua: Version und Konstanten direkt aus dem Code
            txts, defs = [], dict(var.get('defines', {}))
            for f in files_of(fw.get('const_files', ['*.h', '*.ino']), base):
                t, d = preprocess(rd(f), defs)
                for k2, v2 in d.items(): defs.setdefault(k2, v2)
                txts.append(t)
            consts, funcs = collect(txts)
            env = Env({}, funcs, {}, defs)
            for k2, v2 in consts.items():
                if not (v2 and v2[0][0] == 'arr'):
                    env.consts[k2] = v2
            for k2, v2 in consts.items():
                if v2 and v2[0][0] == 'arr':
                    try: env.cache[k2] = eval_init_list(env, v2[0][1])
                    except Exception: pass
            v['version'] = version_text(env, fw.get('version'))
            v['consts'] = export_consts(env, fw.get('export', []), v['lua_errors'])
        # Beispielantworten der API direkt aus dem Code (Trockenlauf der JSON-Funktionen)
        if fw.get('api'):
            afiles = files_of(fw.get('api_files', ['src/RC_Common/*.h', '*.h', '*.cpp', '*.ino']), base)
            over = {'uartRoleOptions()': roles, 'uartBaudOptions()': bauds,
                    'uartRoleDefaultBaud()': RC_ROLE_BAUD_CACHE.get(rc, []), 'webJsonEscape()': None}
            over.pop('webJsonEscape()')
            over.update(fw.get('api_overrides', {}))
            over.update(var.get('overrides', {}))
            v['api'] = {}
            for route, fn in sorted(fw['api'].items()):
                try:
                    txt = run_builder(afiles, fn, var.get('defines', {}), over, fw.get('api_init', []))
                    v['api'][route] = json.loads(txt)
                except Exception as e:
                    v['lua_errors'].append('API %s (%s): kein JSON (%s)' % (route, fn, str(e)[:80]))
        out['variants'].append(v)
    # Fuer die Pruefung: Routen und JSON-Felder der Firmware
    srcs = []
    for f in files_of(fw.get('sources', ['*.ino', '*.cpp', '*.h']), base):
        srcs.append(strip_comments(rd(f)))
    if os.path.isdir(rc):
        for f in sorted(glob.glob(os.path.join(rc, '*.cpp'))):
            srcs.append(strip_comments(rd(f)))
    allsrc = '\n'.join(srcs)
    out['routes'] = sorted(set(re.findall(r'\.on\(\s*"(/[^"]*)"', allsrc)))
    out['keys'] = sorted(set(re.findall(r'\\"([A-Za-z_][A-Za-z0-9_]*)\\":', allsrc)))
    return out


def sample_keys(o, acc):
    if isinstance(o, dict):
        for k, v in o.items():
            acc.add(k); sample_keys(v, acc)
    elif isinstance(o, list):
        for v in o: sample_keys(v, acc)
    return acc


def check_mock(fw, info, mock_text, cfg):
    errs = []
    covered = set()
    for v in info['variants']:
        for o in (v.get('api') or {}).values(): sample_keys(o, covered)
        covered |= {r.split(' ', 1)[1] for r in (v.get('api') or {})}
    ign_r = set(cfg.get('ignore_routes', [])) | set(fw.get('ignore_routes', [])) | {'/', '/rc_common.js'}
    for r in info['routes']:
        if r in ign_r: continue
        if r in covered: continue
        if ("'" + r + "'") not in mock_text and ('"' + r + '"') not in mock_text:
            errs.append('Route %s fehlt in %s' % (r, fw['mock']))
    ign_k = set(cfg.get('ignore_keys', [])) | set(fw.get('ignore_keys', []))
    for k in info['keys']:
        if k in ign_k or k in covered: continue
        if not re.search(r'\b' + re.escape(k) + r'\b', mock_text):
            errs.append('JSON-Feld "%s" fehlt in %s' % (k, fw['mock']))
    return errs


def main():
    check = '--check' in sys.argv
    cfg = json.loads(rd(os.path.join(HERE, 'config.json')))
    fws, errs, mocks = [], [], {}
    common = os.path.join(HERE, 'mock_common.js')
    ct = rd(common) if os.path.exists(common) else ''
    for fw in cfg['firmwares']:
        info = build_firmware(fw)
        mt = rd(os.path.join(HERE, fw['mock']))
        mocks[fw['mock']] = mt
        for extra in fw.get('mock_uses', []):   # weitere Simulationen, auf die diese aufbaut
            et = rd(os.path.join(HERE, extra))
            mocks[extra] = et
            mt = et + '\n' + mt
        mt = ct + '\n' + mt
        errs += ['%s: %s' % (fw['name'], e) for e in check_mock(fw, info, mt, cfg)]
        for v in info['variants']:
            errs += ['%s %s: %s' % (fw['name'], v['name'], e) for e in v['lua_errors']]
        del info['routes'], info['keys']
        fws.append(info)
    data = {'title': cfg['title'], 'repo': cfg.get('repo', ''), 'firmwares': fws, 'build': BUILD_VERSION}
    tpl = rd(os.path.join(HERE, 'template.html'))
    js_data = json.dumps(data, ensure_ascii=False, sort_keys=True).replace('</', '<\\/')
    mock_js = (ct + '\n' + '\n'.join(mocks[k] for k in sorted(mocks))).replace('</script', '<\\/script')
    html = tpl.replace('/*@DATA@*/null', js_data).replace('/*@MOCKS@*/', mock_js).replace('@TITLE@', cfg['title'])
    target = os.path.join(HERE, 'pruefstand.html')
    if check:
        old = rd(target) if os.path.exists(target) else ''
        if old != html:
            errs.append('pruefstand/pruefstand.html ist nicht aktuell: python3 pruefstand/build.py ausfuehren und committen')
        if errs:
            print('Pruefstand: %d Fehler' % len(errs))
            for e in errs: print('  - ' + e)
            sys.exit(1)
        print('Pruefstand aktuell (%s).' % ', '.join('%s: %d Lua-Felder' % (f['name'], max(0, len(f['variants'][0]['items']) - 1)) for f in fws))
        return
    with open(target, 'w', encoding='utf-8', newline='\n') as f:
        f.write(html)
    print('geschrieben: pruefstand/pruefstand.html (%d kB)' % (len(html) // 1024))
    for e in errs: print('  ! ' + e)


if __name__ == '__main__':
    main()
