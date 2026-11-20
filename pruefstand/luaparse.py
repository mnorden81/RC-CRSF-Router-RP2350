# Liest das Lua-Menue (RC_Common lua_menu) direkt aus dem Firmware-Code.
# Kleiner C-Interpreter fuer genau das, was in buildLuaMenu() vorkommt:
# m.folder/info/u8/select, Konstanten, for-Schleifen, #if, snprintf in
# Namenspuffer und Aufrufe weiterer Menue-Funktionen (z. B. soundLuaMotor).
import re
import warnings
warnings.filterwarnings("ignore", category=SyntaxWarning)   # #if-Ausdruecke werden mit eval() geprueft

class LuaParseError(Exception):
    pass

# ── Kommentare entfernen (Strings bleiben) ─────────────────────────────
def strip_comments(s):
    out, i, n = [], 0, len(s)
    while i < n:
        c = s[i]
        if c == '"' or c == "'":
            # R"X(...)X" Rohstrings
            if c == '"' and i > 0 and s[i - 1] == 'R':
                m = re.match(r'"([^(\s]*)\(', s[i:])
                if m:
                    end = s.index(')' + m.group(1) + '"', i)
                    out.append('""'); i = end + len(m.group(1)) + 2; continue
            j = i + 1
            while j < n and s[j] != c:
                j += 2 if s[j] == '\\' else 1
            out.append(s[i:j + 1]); i = j + 1
        elif s.startswith('//', i):
            j = s.find('\n', i); i = n if j < 0 else j
        elif s.startswith('/*', i):
            j = s.find('*/', i + 2); seg = s[i:j + 2]
            out.append('\n' * seg.count('\n')); i = j + 2
        else:
            out.append(c); i += 1
    return ''.join(out)

# ── Tokenizer ──────────────────────────────────────────────────────────
TOK = re.compile(r'''\s*(?:
    (?P<str>"(?:\\.|[^"\\])*")|
    (?P<chr>'(?:\\.|[^'\\])*')|
    (?P<num>0[xX][0-9a-fA-F]+[uUlL]*|\d+\.\d*f?|\d+[uUlL]*)|
    (?P<id>[A-Za-z_]\w*)|
    (?P<op>->|\+\+|--|<<=|>>=|<<|>>|<=|>=|==|!=|&&|\|\||\+=|-=|\*=|/=|%=|&=|\|=|::|[-+*/%<>=!&|^~?:;,.(){}\[\]\#])
    )''', re.X)

def tokenize(s):
    toks, i = [], 0
    s = s.rstrip()
    while i < len(s):
        m = TOK.match(s, i)
        if not m:
            raise LuaParseError('Unbekanntes Zeichen: %r' % s[i:i + 20])
        i = m.end()
        for k in ('str', 'chr', 'num', 'id', 'op'):
            v = m.group(k)
            if v is not None:
                toks.append((k, v)); break
    return toks

def unescape(lit):
    body = lit[1:-1]
    return bytes(body, 'utf-8').decode('unicode_escape').encode('latin-1').decode('utf-8')

# ── Praeprozessor ──────────────────────────────────────────────────────
def preprocess(text, defines):
    text = strip_comments(text).replace('\\\n', ' ')
    out, stack = [], []          # stack: (aktiv, schon_genommen, elternaktiv)
    active = True
    defs = dict(defines)
    for line in text.split('\n'):
        st = line.strip()
        m = re.match(r'#\s*(\w+)\s*(.*)', st)
        if m:
            d, rest = m.group(1), m.group(2).strip()
            if d in ('if', 'ifdef', 'ifndef'):
                if d == 'ifdef': cond = rest.split()[0] in defs
                elif d == 'ifndef': cond = rest.split()[0] not in defs
                else: cond = pp_eval(rest, defs)
                stack.append((active, cond, active))
                active = active and cond
                out.append(''); continue
            if d == 'elif':
                parent_act, taken, par = stack[-1]
                cond = (not taken) and pp_eval(rest, defs)
                stack[-1] = (parent_act, taken or cond, par)
                active = par and cond
                out.append(''); continue
            if d == 'else':
                parent_act, taken, par = stack[-1]
                active = par and not taken
                stack[-1] = (parent_act, True, par)
                out.append(''); continue
            if d == 'endif':
                a, _, _ = stack.pop(); active = a
                out.append(''); continue
            if d == 'define' and active:
                mm = re.match(r'(\w+)(\([^)]*\))?\s*(.*)', rest)
                if mm and not mm.group(2):
                    defs.setdefault(mm.group(1), mm.group(3).strip())
                out.append('#define ' + rest if active else ''); continue
            if d == 'undef' and active:
                defs.pop(rest.split()[0], None)
            out.append(''); continue
        out.append(line if active else '')
    return '\n'.join(out), defs

def pp_eval(expr, defs):
    expr = re.sub(r'defined\s*\(\s*(\w+)\s*\)', lambda m: '1' if m.group(1) in defs else '0', expr)
    expr = re.sub(r'defined\s+(\w+)', lambda m: '1' if m.group(1) in defs else '0', expr)
    def ident(m):
        k = m.group(0)
        v = defs.get(k)
        if v is None: return '0'
        v = str(v).strip() or '1'
        return '(' + v + ')' if re.fullmatch(r'[\w\s()+\-*/<>=!&|]+', v) else '0'
    for _ in range(5):
        expr2 = re.sub(r'\b[A-Za-z_]\w*\b', ident, expr)
        if expr2 == expr: break
        expr = expr2
    expr = expr.replace('&&', ' and ').replace('||', ' or ')
    expr = re.sub(r'!(?!=)', ' not ', expr)
    expr = re.sub(r'(\d+)[uUlL]+', r'\1', expr)
    try:
        return bool(eval(expr, {}, {}))
    except Exception:
        return False

# ── Ausdruecke ─────────────────────────────────────────────────────────
class Unknown(Exception):
    pass

TYPES = {'uint8_t', 'uint16_t', 'uint32_t', 'int8_t', 'int16_t', 'int32_t', 'int', 'unsigned', 'char',
         'size_t', 'bool', 'long', 'short', 'float', 'double', 'auto', 'byte', 'String'}

class Expr:
    def __init__(self, toks, env):
        self.t, self.i, self.env = toks, 0, env
    def peek(self, k=0):
        j = self.i + k
        return self.t[j] if j < len(self.t) else (None, None)
    def take(self, v=None):
        tok = self.peek()
        if v is not None and tok[1] != v:
            raise LuaParseError('Erwartet %s, gefunden %s' % (v, tok[1]))
        self.i += 1
        return tok
    def parse(self):
        v = self.ternary()
        return v
    def ternary(self):
        c = self.lor()
        if self.peek()[1] == '?':
            self.take('?'); a = self.ternary(); self.take(':'); b = self.ternary()
            return a if truth(c) else b
        return c
    def lor(self):
        v = self.land()
        while self.peek()[1] == '||':
            self.take(); w = self.land(); v = 1 if (truth(v) or truth(w)) else 0
        return v
    def land(self):
        v = self.bor()
        while self.peek()[1] == '&&':
            self.take(); w = self.bor(); v = 1 if (truth(v) and truth(w)) else 0
        return v
    def bor(self):
        v = self.band()
        while self.peek()[1] == '|' :
            self.take(); v = num(v) | num(self.band())
        return v
    def band(self):
        v = self.eq()
        while self.peek()[1] == '&':
            self.take(); v = num(v) & num(self.eq())
        return v
    def eq(self):
        v = self.rel()
        while self.peek()[1] in ('==', '!='):
            op = self.take()[1]; w = self.rel()
            v = 1 if ((v == w) == (op == '==')) else 0
        return v
    def rel(self):
        v = self.shift()
        while self.peek()[1] in ('<', '>', '<=', '>='):
            op = self.take()[1]; w = self.shift()
            v = 1 if {'<': num(v) < num(w), '>': num(v) > num(w), '<=': num(v) <= num(w), '>=': num(v) >= num(w)}[op] else 0
        return v
    def shift(self):
        v = self.add()
        while self.peek()[1] in ('<<', '>>'):
            op = self.take()[1]; w = self.add()
            v = num(v) << num(w) if op == '<<' else num(v) >> num(w)
        return v
    def add(self):
        v = self.mul()
        while self.peek()[1] in ('+', '-'):
            op = self.take()[1]; w = self.mul()
            if op == '+' and (isinstance(v, str) or isinstance(w, str)):
                v = fmtval(v) + fmtval(w)
            else:
                v = num(v) + num(w) if op == '+' else num(v) - num(w)
        return v
    def mul(self):
        v = self.unary()
        while self.peek()[1] in ('*', '/', '%'):
            op = self.take()[1]; w = self.unary()
            v = num(v) * num(w) if op == '*' else (num(v) // num(w) if op == '/' else num(v) % num(w))
        return v
    def unary(self):
        k, v = self.peek()
        if v == '!':
            self.take(); return 0 if truth(self.unary()) else 1
        if v == '-':
            self.take(); return -num(self.unary())
        if v == '+':
            self.take(); return num(self.unary())
        if v == '~':
            self.take(); return ~num(self.unary())
        if v == '(':
            # Cast?
            j = 1; names = []
            while self.peek(j)[1] in TYPES or self.peek(j)[1] in ('const', '*', 'unsigned'):
                names.append(self.peek(j)[1]); j += 1
            if names and self.peek(j)[1] == ')':
                self.i += j + 1
                return self.unary()
        if v == 'sizeof':
            self.take(); self.skip_parens(); return 0
        return self.postfix()
    def skip_parens(self):
        self.take('('); d = 1
        while d:
            v = self.take()[1]
            if v == '(': d += 1
            elif v == ')': d -= 1
    def postfix(self):
        v = self.primary()
        while True:
            t = self.peek()[1]
            if t == '[':
                self.take(); idx = self.ternary(); self.take(']')
                if isinstance(v, (list, tuple)):
                    try:
                        v = v[num(idx)]
                    except (IndexError, TypeError):
                        if not getattr(self.env, 'lenient', False): raise Unknown('Index')
                        v = 0
                elif isinstance(v, Struct):
                    v = v.get(num(idx), 0)
                elif isinstance(v, dict):
                    v = v.get(num(idx), Unknown)
                    if v is Unknown: raise Unknown('Index')
                elif getattr(self.env, 'lenient', False):
                    v = 0
                else:
                    raise Unknown('Index auf %r' % (v,))
            elif t in ('.', '->'):
                self.take(); name = self.take()[1]
                if self.peek()[1] == '(':
                    args = self.args()
                    if name == 'c_str' and isinstance(v, str): continue
                    if getattr(self.env, 'lenient', False):
                        # Preferences: prefs.getUChar("key", vorgabe) -> Vorgabe (Werkseinstellung)
                        if re.match(r'^get(U?Char|U?Short|U?Int|U?Long|Bool|Float|String)$', name) and len(args) == 2:
                            v = args[1]; continue
                        v = v if name in ('c_str', 'toString') else 0; continue
                    raise Unknown('Methode ' + name)
                if isinstance(v, Struct):
                    v = v.get(name, 0); continue
                if getattr(self.env, 'lenient', False):
                    v = 0; continue
                raise Unknown('Member ' + name)
            elif t in ('++', '--'):
                raise Unknown('postfix')
            else:
                return v
    def args(self):
        self.take('('); out = []
        if self.peek()[1] == ')':
            self.take(); return out
        while True:
            out.append(self.ternary())
            if self.peek()[1] == ',':
                self.take(); continue
            self.take(')'); return out
    def primary(self):
        k, v = self.take()
        if k == 'num':
            v2 = v.rstrip('uUlLf')
            return int(v2, 16) if v2.lower().startswith('0x') else (float(v2) if '.' in v2 else int(v2))
        if k == 'str':
            s = unescape(v)
            # benachbarte Literale/Makros zusammenhaengen
            while True:
                k2, v2 = self.peek()
                if k2 == 'str':
                    self.take(); s += unescape(v2)
                elif k2 == 'id' and isinstance(self.env.lookup_soft(v2), str) and self.peek(1)[1] not in ('(',):
                    self.take(); s += self.env.lookup_soft(v2)
                else:
                    break
            return s
        if k == 'chr':
            return ord(unescape(v))
        if v == '(':
            r = self.ternary(); self.take(')'); return r
        if k == 'id' and v in getattr(self.env, 'mv', ()) and self.peek()[1] == '.' and self.peek(1)[1] in ADD:
            self.take('.'); kind = self.take()[1]
            c = find_close(self.t, self.i, '(', ')')
            args = split_args(self.t[self.i + 1:c]); self.i = c + 1
            return make_item(self.env, self.env.menu, kind, args)
        if k == 'id':
            if v in ('true',): return 1
            if v in ('false',): return 0
            if v in ('nullptr', 'NULL'): return None
            if self.peek()[1] == '(':
                args = self.args()
                return self.env.call(v, args)
            if self.peek()[1] == '::':
                self.take(); return self.primary()
            if self.peek()[1] in ('++', '--'):
                op = self.take()[1]; val = num(self.env.lookup(v))
                self.env.assign(v, val + (1 if op == '++' else -1)); return val
            if self.env.lenient and self.peek()[1] in ('.', '->', '['):
                val = self.env.lookup_soft(v)
                if val is None or val == 0:
                    val = Struct(); self.env.scopes[0][v] = val
            else:
                val = self.env.lookup(v)
            if isinstance(val, str):
                while self.peek()[0] == 'str':
                    val += unescape(self.take()[1])
            return val
        raise Unknown('Ausdruck bei %r' % v)

def fmtval(v):
    if isinstance(v, str): return v
    return str(num(v))

def truth(v):
    if isinstance(v, (str, dict, list, FnRef)): return True
    return bool(num(v))

def num(v):
    if isinstance(v, bool): return int(v)
    if isinstance(v, (int, float)): return v
    if v is None: return 0
    raise Unknown('keine Zahl: %r' % (v,))

class Struct(dict):
    """Struktur/Feld im Trockenlauf: Mitglieder und Indizes nach Bedarf."""
    pass


class FnRef:
    def __init__(self, name): self.name = name
    def __repr__(self): return self.name

# ── Umgebung: Konstanten, Funktionen ───────────────────────────────────
class Env:
    lenient = False
    def __init__(self, consts, funcs, overrides, defines):
        self.strings = {}
        self.consts, self.funcs, self.over, self.defines = consts, funcs, overrides, defines
        self.scopes = [{}]
        self.cache = {}
    def push(self, d=None): self.scopes.append(d or {})
    def pop(self): self.scopes.pop()
    def set(self, k, v):
        self.note(k, v); self.scopes[-1][k] = v
    def note(self, k, v):
        if self.lenient and isinstance(v, str) and v[:1] in '{[':
            if len(v) >= len(self.strings.get(k, '')): self.strings[k] = v
    def assign(self, k, v):
        self.note(k, v)
        for s in reversed(self.scopes):
            if k in s:
                s[k] = v; return
        self.scopes[0][k] = v
    def lookup_soft(self, k):
        try:
            return self.lookup(k)
        except Unknown:
            return None
    def lookup(self, k):
        if k in self.over: return self.over[k]
        for s in reversed(self.scopes):
            if k in s: return s[k]
        if k in self.cache: return self.cache[k]
        if k in self.consts:
            self.cache[k] = Unknown          # Schutz vor Rekursion
            try:
                v = Expr(self.consts[k], self).parse()
            except (Unknown, LuaParseError, IndexError):
                del self.cache[k]
                raise Unknown(k)
            self.cache[k] = v
            return v
        if k in self.defines:
            d = str(self.defines[k]).strip()
            if d == '': return 1
            try:
                v = Expr(tokenize(d), self).parse(); self.cache[k] = v; return v
            except (Unknown, LuaParseError):
                raise Unknown(k)
        if k in self.funcs:
            return FnRef(k)
        if re.match(r'^[a-z]\w*$', k) and (k.startswith('lua') or k.startswith('snd')) and not self.lenient:
            return FnRef(k)
        if self.lenient: return 0
        raise Unknown(k)
    def call(self, name, args):
        if name + '()' in self.over:
            o = self.over[name + '()']
            if isinstance(o, list) and args:
                try: return o[num(args[0])]
                except (IndexError, Unknown): return 0
            return o
        if name in ('min', 'max', 'constrain'):
            a = [num(x) for x in args]
            return min(a) if name == 'min' else (max(a) if name == 'max' else max(a[1], min(a[0], a[2])))
        if name == 'String': return fmtval(args[0]) if args else ''
        st = getattr(self, 'structs', {}).get(name)
        if st is not None and not args:
            o = Struct()
            for k, v in st.items():
                try: o[k] = Expr(v, self).parse()
                except Exception: o[k] = 0
            return o
        f = self.funcs.get(name)
        if f and f['ret']:
            # einfache Funktion: nur "if (c) return x;" und "return x;"
            m = f['body_toks']; i = 0
            self.push(dict(zip(f['params'], args)))
            try:
                while i < len(m):
                    if m[i][1] == 'return':
                        e = stmt_end(m, i, len(m))
                        return Expr(m[i + 1:e], self).parse()
                    if m[i][1] == 'if':
                        c = find_close(m, i + 1, '(', ')')
                        cond = truth(Expr(m[i + 2:c], self).parse())
                        if m[c + 1][1] != 'return': break
                        e = stmt_end(m, c + 1, len(m))
                        if cond:
                            return Expr(m[c + 2:e], self).parse()
                        i = e + 1; continue
                    break
            except Unknown:
                if not self.lenient: raise
            except (LuaParseError, IndexError, KeyError, TypeError, AttributeError, ValueError):
                if not self.lenient: raise
            finally:
                self.pop()
        if self.lenient:
            # Hilfsfunktionen wie jsonEscape(text) geben den Text zurueck
            return args[0] if args and isinstance(args[0], str) else 0
        raise Unknown(name + '()')

# ── Quellen einsammeln ─────────────────────────────────────────────────
def collect(texts):
    consts, funcs = {}, {}
    for src in texts:
        toks = tokenize(src.replace('#define', ' '))  # define-Zeilen stoeren nicht
        # Konstanten:  [static] [constexpr|const] TYPE [const] [*] NAME [ [N] ] = ... ;
        n = len(toks); i = 0
        depth = 0
        while i < n:
            v = toks[i][1]
            if v == '{': depth += 1
            elif v == '}': depth -= 1
            if depth == 0 and (v in ('constexpr', 'const') or (v in TYPES and i + 2 < n and toks[i + 1][0] == 'id' and toks[i + 2][1] == '[' and (i == 0 or toks[i - 1][1] in (';', '}', 'static')))):
                j = i + 1
                while j < n and (toks[j][1] in TYPES or toks[j][1] in ('const', '*', 'unsigned', 'constexpr', 'static', 'char')):
                    j += 1
                if j < n and toks[j][0] == 'id' and j + 1 < n:
                    name = toks[j][1]; k = j + 1
                    while k < n and toks[k][1] == '[':
                        while toks[k][1] != ']': k += 1
                        k += 1
                    if k < n and toks[k][1] == '=':
                        e = k + 1; d = 0
                        while e < n and not (toks[e][1] in (';', ',') and d == 0):
                            if toks[e][1] in '({[': d += 1
                            if toks[e][1] in ')}]': d -= 1
                            e += 1
                        val = toks[k + 1:e]
                        if val and val[0][1] == '{':
                            consts[name] = [('arr', val)]
                        else:
                            consts.setdefault(name, val)
                        i = e; continue
            # globale Variable mit festem Startwert:  uint16_t Version = 14;
            if depth == 0 and v in TYPES and i + 4 < n and toks[i + 1][0] == 'id' and toks[i + 2][1] == '=' \
               and toks[i + 3][0] in ('num', 'str') and toks[i + 4][1] == ';' and (i == 0 or toks[i - 1][1] in (';', '}', 'static')):
                consts.setdefault(toks[i + 1][1], [toks[i + 3]])
                i += 5; continue
            # Funktionen:  TYPE NAME ( params ) {   (auf oberster Ebene)
            if depth == 0 and toks[i][0] == 'id' and i + 1 < n and toks[i + 1][1] == '(' and i > 0 and \
               (toks[i - 1][1] in TYPES or toks[i - 1][1] in ('void', '*', '&', '::') or toks[i - 1][0] == 'id'):
                k = i + 2; d = 1
                while k < n and d:
                    if toks[k][1] == '(': d += 1
                    elif toks[k][1] == ')': d -= 1
                    k += 1
                if k < n and toks[k][1] == '{':
                    params_t = toks[i + 2:k - 1]
                    params, cur = [], []
                    for t in params_t + [('op', ',')]:
                        if t[1] == ',':
                            ids = [x[1] for x in cur if x[0] == 'id' and x[1] not in TYPES and x[1] != 'const']
                            if '=' in [x[1] for x in cur]:
                                eqi = [x[1] for x in cur].index('=')
                                ids = [x[1] for x in cur[:eqi] if x[0] == 'id' and x[1] not in TYPES and x[1] != 'const']
                            if ids: params.append(ids[-1])
                            cur = []
                        else:
                            cur.append(t)
                    b = k + 1; d = 1
                    while b < n and d:
                        if toks[b][1] == '{': d += 1
                        elif toks[b][1] == '}': d -= 1
                        b += 1
                    ret = toks[i - 1][1] != 'void'
                    funcs.setdefault(toks[i][1], {'params': params, 'body_toks': toks[k + 1:b - 1], 'ret': ret,
                                                 'menu': any(x[1] == 'LuaMenu' for x in params_t)})
                    i = b; continue
            i += 1
    # Arrays auswerten lassen wir spaeter (brauchen Env)
    return consts, funcs

# ── Interpreter fuer Menue-Funktionen ──────────────────────────────────
class Menu:
    def __init__(self):
        self.items = []
        self.warn = []

def run_function(env, menu, fname, args, menu_vars):
    f = env.funcs[fname]
    env.push(dict(zip(f['params'], args)))
    mv = set(menu_vars)
    toks = f['body_toks']
    # Parameter vom Typ LuaMenu&
    try:
        exec_block(env, menu, toks, 0, len(toks), mv)
    except _Return:
        pass
    env.pop()

class _Return(Exception):
    def __init__(self, toks=None):
        self.toks = toks or []

def find_close(toks, i, o, c):
    d = 0
    while i < len(toks):
        if toks[i][1] == o: d += 1
        elif toks[i][1] == c:
            d -= 1
            if d == 0: return i
        i += 1
    raise LuaParseError('Klammer fehlt')

def stmt_end(toks, i, end):
    d = 0
    while i < end:
        v = toks[i][1]
        if v in '([{': d += 1
        elif v in ')]}': d -= 1
        elif v == ';' and d == 0: return i
        i += 1
    return end

def exec_block(env, menu, toks, i, end, mv):
    env.mv, env.menu = mv, menu
    while i < end:
        i = exec_stmt(env, menu, toks, i, end, mv)

def exec_stmt(env, menu, toks, i, end, mv):
    k, v = toks[i]
    if v == ';':
        return i + 1
    if v == '{':
        c = find_close(toks, i, '{', '}')
        env.push(); exec_block(env, menu, toks, i + 1, c, mv); env.pop()
        return c + 1
    if v == 'return':
        e = stmt_end(toks, i, end)
        raise _Return(toks[i + 1:e])
    if v in ('while', 'switch'):
        c = find_close(toks, i + 1, '(', ')')
        return stmt_span(toks, c + 1, end)
    if v == 'do':
        e = stmt_span(toks, i + 1, end)
        return stmt_end(toks, e, end) + 1
    if v == 'for':
        c = find_close(toks, i + 1, '(', ')')
        head = toks[i + 2:c]
        parts, cur, d = [], [], 0
        for t in head:
            if t[1] == ';' and d == 0:
                parts.append(cur); cur = []
            else:
                if t[1] in '([': d += 1
                if t[1] in ')]': d -= 1
                cur.append(t)
        parts.append(cur)
        if len(parts) != 3:
            return stmt_span(toks, c + 1, end)
        body_s = c + 1
        if toks[body_s][1] == '{':
            body_e = find_close(toks, body_s, '{', '}'); nxt = body_e + 1
            bs, be = body_s + 1, body_e
        else:
            body_e = stmt_end_any(toks, body_s, end); nxt = body_e
            bs, be = body_s, body_e
        env.push()
        try:
            do_decl_or_assign(env, parts[0])
            guard = 0
            while truth(Expr(parts[1], env).parse()):
                env.push(); exec_block(env, menu, toks, bs, be, mv); env.pop()
                do_incr(env, parts[2])
                guard += 1
                if guard > 300: raise LuaParseError('Schleife zu lang')
        except Unknown as e:
            menu.warn.append('Schleife nicht auswertbar (%s) – uebersprungen' % e)
        env.pop()
        return nxt
    if v == 'if':
        c = find_close(toks, i + 1, '(', ')')
        try:
            cond = truth(Expr(toks[i + 2:c], env).parse())
        except Unknown as e:
            menu.warn.append('if nicht auswertbar (%s), Zweig genommen' % e); cond = True
        s = c + 1
        e1 = stmt_span(toks, s, end)
        nxt = e1
        if nxt < end and toks[nxt][1] == 'else':
            e2 = stmt_span(toks, nxt + 1, end)
            if cond: exec_block(env, menu, toks, s, e1, mv)
            else: exec_block(env, menu, toks, nxt + 1, e2, mv)
            return e2
        if cond: exec_block(env, menu, toks, s, e1, mv)
        return e1
    e = stmt_end(toks, i, end)
    try:
        simple(env, menu, toks[i:e], mv)
    except (LuaParseError, IndexError, ZeroDivisionError, TypeError, ValueError) as ex:
        if not env.lenient: raise
    except Unknown as ex:
        menu.warn.append('Anweisung nicht auswertbar (%s): %s' % (ex, ' '.join(x[1] for x in toks[i:e])[:120]))
    return e + 1

def stmt_span(toks, s, end):
    if toks[s][1] == '{':
        return find_close(toks, s, '{', '}') + 1
    if toks[s][1] in ('if', 'for'):
        c = find_close(toks, s + 1, '(', ')')
        e = stmt_span(toks, c + 1, end)
        if toks[s][1] == 'if' and e < end and toks[e][1] == 'else':
            e = stmt_span(toks, e + 1, end)
        return e
    return stmt_end(toks, s, end) + 1

def stmt_end_any(toks, s, end):
    return stmt_span(toks, s, end)

def do_decl_or_assign(env, t):
    j = 0
    while j < len(t) and (t[j][1] in TYPES or t[j][1] in ('const', 'static', 'unsigned', 'constexpr')):
        j += 1
    if j + 1 < len(t) and t[j + 1][1] == '=':
        env.set(t[j][1], Expr(t[j + 2:], env).parse())

def do_incr(env, t):
    if len(t) == 2 and t[1][1] in ('++', '--'):
        name = t[0][1]; env.assign(name, num(env.lookup(name)) + (1 if t[1][1] == '++' else -1))
    elif len(t) == 2 and t[0][1] in ('++', '--'):
        name = t[1][1]; env.assign(name, num(env.lookup(name)) + (1 if t[0][1] == '++' else -1))
    elif len(t) >= 3 and t[1][1] in ('+=', '-='):
        name = t[0][1]; w = num(Expr(t[2:], env).parse())
        env.assign(name, num(env.lookup(name)) + (w if t[1][1] == '+=' else -w))
    else:
        raise Unknown('Schleifenschritt')

ADD = {'folder': 3, 'info': 4, 'u8': 9, 'select': 7}

def simple(env, menu, t, mv):
    vals = [x[1] for x in t]
    if not t: return
    # Referenz/Zeiger auf ein Feld:  [const] Typ& name = cfg.common;
    if env.lenient:
        k0 = 1 if vals[:1] == ['const'] else 0
        if len(t) > k0 + 4 and t[k0][0] == 'id' and vals[k0 + 1] in ('&', '*') and t[k0 + 2][0] == 'id' and vals[k0 + 3] == '=':
            rhs = t[k0 + 4:]
            if rhs and rhs[0][1] == '&': rhs = rhs[1:]
            obj = path_obj(env, rhs)
            if obj is not None:
                env.set(t[k0 + 2][1], obj); return
    # Deklaration lokaler Variablen
    j = 0
    while j < len(t) and (t[j][1] in TYPES or t[j][1] in ('const', 'static', 'unsigned', 'constexpr', 'LuaItem', 'LuaMenu', '*', '&')):
        j += 1
    is_decl = j > 0 and j < len(t) and t[j][0] == 'id'
    target = None
    if is_decl:
        name = t[j][1]; k = j + 1
        if vals[0:1] == ['LuaMenu'] and k < len(t) and t[k][1] == '=':
            mv.add(name); return
        arr = None
        if k < len(t) and t[k][1] == '[':
            k = find_close(t, k, '[', ']') + 1; arr = True
        if k >= len(t):
            env.set(name, {} if arr else None); return
        if t[k][1] != '=':
            return
        rhs = t[k + 1:]
        if rhs and rhs[0][1] == '{':
            try:
                env.set(name, eval_init_list(env, rhs))
            except Unknown:
                env.set(name, None)
            return
        if vals[:j].count('LuaItem') or (rhs and rhs[0][1] in mv):
            target = name; t = rhs; vals = [x[1] for x in t]
        else:
            try:
                env.set(name, Expr(rhs, env).parse())
            except Unknown:
                env.set(name, None)
            return
    # Menue-Aufruf m.xxx(...)
    if len(t) >= 4 and t[0][1] in mv and t[1][1] == '.' and t[2][1] in ADD:
        kind = t[2][1]
        c = find_close(t, 3, '(', ')')
        args = split_args(t[4:c])
        item = make_item(env, menu, kind, args)
        rest = t[c + 1:]
        apply_chain(env, menu, item, rest)
        if target: env.set(target, item)
        return
    if len(t) >= 4 and t[0][1] in mv and t[1][1] == '.':
        return  # m.clear() usw.
    # it->arg = x;  it->view = fn;
    if len(t) >= 4 and t[1][1] == '->' and isinstance(env.lookup_soft(t[0][1]), dict) and 'id' in env.lookup_soft(t[0][1]):
        apply_chain(env, menu, env.lookup(t[0][1]), t[1:])
        return
    if env.lenient and vals[0] in ('strcpy', 'strncpy', 'strlcpy') and len(t) > 3:
        c = find_close(t, 1, '(', ')')
        a = split_args(t[2:c])
        if len(a) >= 2:
            try:
                if len(a[0]) > 1: assign_lvalue(env, a[0], '=', a[1])
                else: env.assign(a[0][0][1], Expr(a[1], env).parse())
            except Unknown:
                pass
        return
    # puffer[0] = '\0';
    if len(t) == 6 and t[1][1] == '[' and t[2][1] == '0' and t[3][1] == ']' and t[4][1] == '=':
        env.assign(t[0][1], ''); return
    # snprintf(puffer[x], sizeof(...), "fmt", ...)  bzw.  len += snprintf(puffer + len, ...)
    if 'snprintf' in vals:
        si = vals.index('snprintf')
        c = find_close(t, si + 1, '(', ')')
        args = split_args(t[si + 2:c])
        dest = args[0]
        if len(dest) >= 3 and dest[1][1] == '+':
            try:
                fmt = Expr(args[2], env).parse()
                fa = [Expr(a, env).parse() for a in args[3:]]
                cur = env.lookup_soft(dest[0][1])
                env.assign(dest[0][1], (cur if isinstance(cur, str) else '') + cfmt(fmt, fa))
            except Unknown:
                env.assign(dest[0][1], Unknown)
            return
        try:
            fmt = Expr(args[2], env).parse()
            fa = [Expr(a, env).parse() for a in args[3:]]
            s = cfmt(fmt, fa)
            if len(dest) == 1:
                env.assign(dest[0][1], s)
            elif len(dest) >= 4 and dest[1][1] == '[' and dest[-1][1] == ']':
                idx = num(Expr(dest[2:-1], env).parse())
                d = env.lookup_soft(dest[0][1])
                if not isinstance(d, dict):
                    d = {}; env.assign(dest[0][1], d)
                d[idx] = s
        except Unknown:
            pass
        return
    # Zuweisung an Feld:  a.b[i] = x;  a[i] = x;
    if env.lenient and len(t) >= 4 and t[0][0] == 'id' and t[1][1] in ('.', '->', '['):
        d = 0; eq = None
        for k2, x in enumerate(t):
            if x[1] in '([{': d += 1
            elif x[1] in ')]}': d -= 1
            elif d == 0 and x[1] in ('=', '+=', '-=', '|=', '&='):
                eq = k2; break
        if eq:
            assign_lvalue(env, t[:eq], t[eq][1], t[eq + 1:])
            return
    # Zuweisungen  x += ausdruck;  x = ausdruck;
    if len(t) >= 3 and t[0][0] == 'id' and t[1][1] in ('+=', '=', '-=') and not mv & {t[0][1]}:
        try:
            w = Expr(t[2:], env).parse()
            if t[1][1] == '=':
                env.assign(t[0][1], w)
            else:
                cur = env.lookup_soft(t[0][1])
                if isinstance(cur, str) and len(t) == 3 and t[2][0] == 'chr':
                    w = unescape(t[2][1])   # String += 'x' haengt ein Zeichen an
                if isinstance(cur, str) or isinstance(w, str):
                    env.assign(t[0][1], fmtval(cur if cur is not None else '') + fmtval(w))
                else:
                    env.assign(t[0][1], num(cur) + (num(w) if t[1][1] == '+=' else -num(w)))
        except Unknown:
            pass
        return
    # Aufruf einer weiteren Menue-Funktion
    if t[0][0] == 'id' and len(t) >= 3 and t[1][1] == '(' and t[0][1] in env.funcs and env.funcs[t[0][1]]['menu']:
        c = find_close(t, 1, '(', ')')
        raw = split_args(t[2:c])
        args = []
        for a in raw:
            if len(a) == 1 and a[0][1] in mv:
                args.append('__menu__')
            else:
                try: args.append(Expr(a, env).parse())
                except Unknown: args.append(None)
        f = env.funcs[t[0][1]]
        mparams = [p for p, a in zip(f['params'], args) if a == '__menu__']
        run_function(env, menu, t[0][1], args, set(mparams) | mv)
        return
    # Trockenlauf: void-Funktion, die Text in einen Parameter schreibt
    #   soundMapDefaultName(i, config.Sound_File[i]);
    if env.lenient and t[0][0] == 'id' and len(t) >= 3 and t[1][1] == '(' and t[0][1] in env.funcs \
       and getattr(env, 'depth', 0) < 4:
        f = env.funcs[t[0][1]]
        body = [x[1] for x in f['body_toks']]
        outs = [p for p in f['params'] if any(body[k] in ('snprintf', 'strcpy', 'strncpy', 'strlcpy') and
                                               k + 2 < len(body) and body[k + 2] == p for k in range(len(body)))]
        if not outs: return
        c = find_close(t, 1, '(', ')')
        raw = split_args(t[2:c])
        args = []
        for p, a in zip(f['params'], raw):
            if p in outs: args.append(None); continue
            try: args.append(Expr(a, env).parse())
            except Exception: args.append(0)
        env.depth = getattr(env, 'depth', 0) + 1
        env.push(dict(zip(f['params'], args)))
        scope = env.scopes[-1]
        try:
            exec_block(env, menu, f['body_toks'], 0, len(f['body_toks']), set())
        except _Return:
            pass
        except Exception:
            pass
        env.scopes.remove(scope) if scope in env.scopes else None
        env.depth -= 1
        for p, a in zip(f['params'], raw):
            if p in outs and isinstance(scope.get(p), str):
                if len(a) > 1: assign_lvalue(env, a, '=', [('str', '"' + scope[p].replace('\\', '\\\\').replace('"', '\\"') + '"')])
                elif a: env.assign(a[0][1], scope[p])
        return

def path_obj(env, toks):
    """Objekt hinter a.b[c] (fehlende Teile werden als Struct angelegt), sonst None."""
    if not toks or toks[0][0] != 'id': return None
    base = env.lookup_soft(toks[0][1])
    if not isinstance(base, (Struct, list)):
        if len(toks) == 1: return None
        base = Struct(); env.assign(toks[0][1], base)
    obj = base; i = 1
    while i < len(toks):
        if toks[i][1] in ('.', '->') and i + 1 < len(toks):
            k = toks[i + 1][1]; i += 2
        elif toks[i][1] == '[':
            c = find_close(toks, i, '[', ']')
            try: k = num(Expr(toks[i + 1:c], env).parse())
            except Exception: return None
            i = c + 1
        else:
            return None
        try: nxt = obj[k]
        except (KeyError, IndexError, TypeError): nxt = None
        if not isinstance(nxt, (Struct, list)):
            if isinstance(nxt, (int, float, str)) and nxt not in (0, ''): return None
            nxt = Struct()
            try: obj[k] = nxt
            except (IndexError, TypeError): return None
        obj = nxt
    return obj


def assign_lvalue(env, lhs, op, rhs):
    try:
        val = Expr(rhs, env).parse()
    except Unknown:
        val = 0
    base = env.lookup_soft(lhs[0][1])
    if not isinstance(base, (Struct, list)):
        base = Struct(); env.assign(lhs[0][1], base)
    cont, key = None, None
    obj = base; i = 1
    while i < len(lhs):
        if lhs[i][1] in ('.', '->'):
            k = lhs[i + 1][1]; i += 2
        elif lhs[i][1] == '[':
            c = find_close(lhs, i, '[', ']')
            try: k = num(Expr(lhs[i + 1:c], env).parse())
            except Unknown: k = 0
            i = c + 1
        else:
            return
        if cont is not None:
            try: cur0 = cont[key]
            except (KeyError, IndexError, TypeError): cur0 = None
            if isinstance(cur0, str):
                return   # Zeichen in einem Text (z.B. puffer[n-1] = '\0'): Text bleibt
            nxt = cont[key] if (isinstance(cont, Struct) and key in cont) or (isinstance(cont, list) and isinstance(key, int) and key < len(cont)) else None
            if not isinstance(nxt, (Struct, list)):
                nxt = Struct()
                if isinstance(cont, list):
                    if isinstance(key, int) and key < len(cont): cont[key] = nxt
                    else: return
                else:
                    cont[key] = nxt
            obj = nxt
        cont, key = obj, k
    if cont is None: return
    if op != '=':
        try: cur = cont[key]
        except (KeyError, IndexError, TypeError): cur = 0
        try:
            if op == '+=': val = fmtval(cur) + fmtval(val) if isinstance(cur, str) or isinstance(val, str) else num(cur) + num(val)
            elif op == '-=': val = num(cur) - num(val)
            elif op == '|=': val = num(cur) | num(val)
            elif op == '&=': val = num(cur) & num(val)
        except Unknown:
            val = 0
    try:
        cont[key] = val
    except (IndexError, TypeError):
        pass


def eval_init_list(env, rhs):
    c = find_close(rhs, 0, '{', '}')
    out = []
    for a in split_args(rhs[1:c]):
        if not a: continue
        out.append(eval_init_list(env, a) if a[0][1] == '{' else Expr(a, env).parse())
    return out

def split_args(t):
    out, cur, d = [], [], 0
    for x in t:
        if x[1] == ',' and d == 0:
            out.append(cur); cur = []
        else:
            if x[1] in '([{': d += 1
            if x[1] in ')]}': d -= 1
            cur.append(x)
    if cur: out.append(cur)
    return out

def cfmt(fmt, args):
    out, ai = [], 0
    i = 0
    while i < len(fmt):
        c = fmt[i]
        if c == '%' and i + 1 < len(fmt):
            m = re.match(r'%([-0 +#]*\d*(?:\.\d+)?)(hh|h|l|ll|z)?([dusxXcf%])', fmt[i:])
            if m:
                if m.group(3) == '%':
                    out.append('%')
                else:
                    a = args[ai]; ai += 1
                    conv = m.group(3)
                    if conv in 'du': conv = 'd'
                    if conv == 'c': out.append(chr(num(a)))
                    else: out.append(('%' + m.group(1) + conv) % (a if conv == 's' else num(a)))
                i += m.end(); continue
        out.append(c); i += 1
    return ''.join(out)

def make_item(env, menu, kind, args):
    def ev(a, default=None, soft=False):
        try:
            return Expr(a, env).parse()
        except Unknown as e:
            if soft: return default
            raise
    def evs(a):
        try:
            v = Expr(a, env).parse()
            if isinstance(v, (dict, FnRef)) or v is Unknown: raise Unknown('Puffer')
            return v
        except Unknown as e:
            txt = ' '.join(x[1] for x in a)
            menu.warn.append('Text zur Laufzeit: %s' % txt)
            return None
    it = {'id': num(ev(args[0])), 'parent': num(ev(args[1])), 'type': kind, 'name': evs(args[2]),
          'view': None, 'arg': 0}
    if it['name'] is None:
        it['name'] = '(' + ' '.join(x[1] for x in args[2]) + ')'
    if kind == 'info':
        it['text'] = evs(args[3])
    elif kind == 'u8':
        it['min'] = num(ev(args[3])); it['max'] = num(ev(args[4])); it['unit'] = evs(args[5]) or ''
        it['get'] = fname(args[6]); it['set'] = fname(args[7])
        if len(args) > 8: it['arg'] = num(ev(args[8]))
    elif kind == 'select':
        opts = evs(args[3])
        it['text'] = opts
        it['get'] = fname(args[4]); it['set'] = fname(args[5])
        if len(args) > 6: it['arg'] = num(ev(args[6]))
    it['ro'] = kind in ('u8', 'select') and it.get('set') in (None, 'nullptr')
    menu.items.append(it)
    return it

def fname(a):
    return a[0][1] if len(a) == 1 else ' '.join(x[1] for x in a)

def apply_chain(env, menu, item, rest):
    # ->view = fn ;  ->arg = x
    i = 0
    while i + 3 < len(rest) + 1 and i < len(rest):
        if rest[i][1] == '->' and i + 2 < len(rest) and rest[i + 2][1] == '=':
            field = rest[i + 1][1]
            val = rest[i + 3:]
            if field == 'view':
                item['view'] = val[0][1] if val else None
            elif field == 'arg':
                try: item['arg'] = num(Expr(val, env).parse())
                except Unknown: pass
            return
        i += 1

# ── Einstieg ───────────────────────────────────────────────────────────
def parse_menu(files, entry='buildLuaMenu', defines=None, overrides=None):
    texts, defs = [], dict(defines or {})
    for f in files:
        txt, d = preprocess(open(f, encoding='utf-8', errors='replace').read(), defs)
        for k, v in d.items():
            defs.setdefault(k, v)
        texts.append(txt)
    consts, funcs = collect(texts)
    # Arrays
    env = Env({}, funcs, overrides or {}, defs)
    for k, v in consts.items():
        if not (v and v[0][0] == 'arr'):
            env.consts[k] = v
    for k, v in consts.items():
        if v and v[0][0] == 'arr':
            try: env.cache[k] = eval_init_list(env, v[0][1])
            except Exception: pass
    if entry not in funcs:
        raise LuaParseError('Funktion %s nicht gefunden' % entry)
    menu = Menu()
    run_function(env, menu, entry, [], set())
    return menu, env


# ── JSON-Beispiel aus einer Funktion, die JSON zusammensetzt ───────────
def collect_structs(texts):
    """struct NAME { TYP feld = wert; ... };  ->  {NAME: {feld: tokens}}"""
    out = {}
    for src in texts:
        for m in re.finditer(r'\bstruct\s+(\w+)\s*\{([^{}]*)\}\s*;', src):
            d = {}
            for fm in re.finditer(r'(\w+)\s*=\s*([^;]+);', m.group(2)):
                d[fm.group(1)] = tokenize(fm.group(2))
            if d: out.setdefault(m.group(1), d)
    return out


def run_builder(files, fname, defines=None, overrides=None, init=None, _cache={}):
    """Fuehrt eine Funktion wie handleStatus() 'trocken' aus: unbekannte Werte
    werden 0, Schleifen ueber feste Grenzen laufen echt. Ergebnis ist der
    JSON-Text, den die Funktion zusammensetzt."""
    key = (tuple(files), tuple(sorted((defines or {}).items())))
    if key not in _cache:
        texts, defs = [], dict(defines or {})
        for f in files:
            txt, d = preprocess(open(f, encoding='utf-8', errors='replace').read(), defs)
            for k, v in d.items(): defs.setdefault(k, v)
            texts.append(txt)
        _cache[key] = (collect(texts), defs, collect_structs(texts))
    (consts, funcs), defs, structs = _cache[key]
    env = Env({}, funcs, overrides or {}, defs)
    env.structs = structs
    env.lenient = True
    for k, v in consts.items():
        if not (v and v[0][0] == 'arr'): env.consts[k] = v
    for k, v in consts.items():
        if v and v[0][0] == 'arr':
            try: env.cache[k] = eval_init_list(env, v[0][1])
            except Exception: pass
    if fname not in funcs:
        raise LuaParseError('Funktion %s nicht gefunden' % fname)
    menu = Menu()
    for fi in (init or []):
        if fi not in funcs:
            raise LuaParseError('Funktion %s nicht gefunden' % fi)
        env.push({})
        try:
            exec_block(env, menu, funcs[fi]['body_toks'], 0, len(funcs[fi]['body_toks']), set())
        except _Return:
            pass
        env.scopes.pop()
    env.strings = {}
    f = funcs[fname]
    env.push({})
    scope = env.scopes[-1]
    result = None
    try:
        exec_block(env, menu, f['body_toks'], 0, len(f['body_toks']), set())
    except _Return as r:
        if r.toks:
            try: result = Expr(r.toks, env).parse()
            except Unknown: result = None
    cands = list(env.strings.values())
    if not (isinstance(result, str) and result[:1] in '{['):
        result = max(cands, key=len) if cands else None
    env.pop()
    return result
