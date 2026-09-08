"""
shims.py -- host implementations of the 99 imports (milestone M2)

Every function libHAL.Android.so can reach outside itself lands here.  The
list is closed: the library has no dlopen and issues no syscalls, so nothing
can appear at runtime that is not registered below.
"""
import math, os, struct, time, errno as _errno
from guest import Guest, GuestError

REG = {}
def shim(*names):
    def deco(fn):
        for n in names:
            REG[n] = fn
        return fn
    return deco


def install(g, root=None, log=False):
    g.log_shims = log
    g.root = root or os.path.abspath("guestfs")
    os.makedirs(g.root, exist_ok=True)
    g.files = {}                 # guest FILE* -> python handle
    g.gl_log = []                # recorded GL calls (no GL on the desktop harness)
    for n, fn in REG.items():
        g.shims[n] = fn
    for n in g.imports:
        if n.startswith('gl') and n not in g.shims:
            g.shims[n] = _gl_record(n)
    return g


# ------------------------------------------------------------------ memory
@shim('malloc')
def _malloc(g):    g.ret(g.malloc(g.arg(0)))

@shim('free')
def _free(g):      g.free(g.arg(0)); g.ret(0)

@shim('realloc')
def _realloc(g):
    p, n = g.arg(0), g.arg(1)
    if not p:
        return g.ret(g.malloc(n))
    old = g.blocks.get(p, [0, False])[0]
    new = g.malloc(n)
    if old:
        g.write(new, g.read(p, min(old, n)))
    g.free(p)
    g.ret(new)

@shim('memcpy', 'memmove')
def _memcpy(g):
    d, s, n = g.arg(0), g.arg(1), g.arg(2)
    if n:
        g.write(d, g.read(s, n))
    g.ret(d)

@shim('memset')
def _memset(g):
    d, c, n = g.arg(0), g.arg(1) & 0xff, g.arg(2)
    if n:
        g.write(d, bytes([c]) * n)
    g.ret(d)


# -------------------------------------------------------------------- math
def _d1(fn):
    def f(g): g.retd(fn(g.argd(0)))
    return f

def _d2(fn):
    def f(g): g.retd(fn(g.argd(0), g.argd(2)))
    return f

def _safe(fn, default=0.0):
    def w(*a):
        try:    return fn(*a)
        except (ValueError, OverflowError, ZeroDivisionError): return default
    return w

for _n, _f in [('sin', math.sin), ('cos', math.cos), ('tan', math.tan),
               ('asin', math.asin), ('acos', math.acos), ('atan', math.atan),
               ('sqrt', math.sqrt), ('log', math.log)]:
    REG[_n] = _d1(_safe(_f))
REG['atan2'] = _d2(_safe(math.atan2))
REG['pow']   = _d2(_safe(math.pow))


# -------------------------------------------------------------------- time
@shim('clock_gettime')
def _clock_gettime(g):
    ts = g.arg(1)
    t = time.perf_counter()
    g.write(ts, struct.pack('<II', int(t), int((t % 1) * 1e9)))
    g.ret(0)

@shim('time')
def _time(g):
    t = int(time.time())
    if g.arg(0):
        g.write(g.arg(0), struct.pack('<I', t))
    g.ret(t)

@shim('localtime')
def _localtime(g):
    tp = g.arg(0)
    secs = struct.unpack('<I', g.read(tp, 4))[0] if tp else int(time.time())
    lt = time.localtime(secs)
    buf = getattr(g, '_tm', None) or g.alloc_scratch(44)
    g._tm = buf
    g.write(buf, struct.pack('<9i', lt.tm_sec, lt.tm_min, lt.tm_hour, lt.tm_mday,
                             lt.tm_mon - 1, lt.tm_year - 1900, lt.tm_wday,
                             lt.tm_yday, lt.tm_isdst))
    g.ret(buf)

@shim('usleep')
def _usleep(g):
    # real sleeping would stall the harness; the engine only uses this to yield
    g.ret(0)


# ---------------------------------------------------------------- file I/O
def _hostpath(g, p):
    p = (p or '').replace('\\', '/').lstrip('/')
    return os.path.join(g.root, p)

@shim('fopen')
def _fopen(g):
    path, mode = g.cstr(g.arg(0)), (g.cstr(g.arg(1)) or 'rb')
    m = mode.replace('b', '') + 'b'
    try:
        fh = open(_hostpath(g, path), m)
    except OSError:
        return g.ret(0)
    fp = g.malloc(8)
    g.files[fp] = fh
    if getattr(g, 'log_shims', False):
        print("      fopen(%s,%s) -> 0x%x" % (path, mode, fp))
    g.ret(fp)

@shim('fdopen')
def _fdopen(g):
    """The engine receives its asset pack as a file descriptor from
    Java (openRawResourceFd) and wraps it here."""
    fd, mode = g.arg(0), g.cstr(g.arg(1)) or 'rb'
    fh = getattr(g, 'fd_files', {}).get(fd)
    if fh is None:
        return g.ret(0)
    fp = g.malloc(8)
    g.files[fp] = fh
    if getattr(g, 'log_shims', False):
        print("      fdopen(fd=%d,%s) -> FILE 0x%x" % (fd, mode, fp))
    g.ret(fp)

@shim('fread')
def _fread(g):
    buf, sz, n, fp = g.arg(0), g.arg(1), g.arg(2), g.arg(3)
    fh = g.files.get(fp)
    if not fh or sz * n == 0:
        return g.ret(0)
    data = fh.read(sz * n)
    g.write(buf, data)
    g.ret(len(data) // sz if sz else 0)

@shim('fwrite')
def _fwrite(g):
    buf, sz, n, fp = g.arg(0), g.arg(1), g.arg(2), g.arg(3)
    fh = g.files.get(fp)
    if not fh or sz * n == 0:
        return g.ret(0)
    fh.write(g.read(buf, sz * n))
    g.ret(n)

@shim('fseek')
def _fseek(g):
    fh = g.files.get(g.arg(0))
    if not fh:
        return g.ret(-1 & 0xffffffff)
    off = g.arg(1)
    fh.seek(off - (1 << 32) if off >= (1 << 31) else off, g.arg(2))
    g.ret(0)

@shim('ftell')
def _ftell(g):
    fh = g.files.get(g.arg(0))
    g.ret(fh.tell() if fh else -1 & 0xffffffff)

@shim('fclose')
def _fclose(g):
    fh = g.files.pop(g.arg(0), None)
    if fh:
        fh.close()
    g.ret(0)

@shim('fflush')
def _fflush(g):
    fh = g.files.get(g.arg(0))
    if fh:
        fh.flush()
    g.ret(0)

@shim('fputc')
def _fputc(g):  g.ret(g.arg(0))

@shim('stat')
def _stat(g):
    try:
        os.stat(_hostpath(g, g.cstr(g.arg(0))))
        g.ret(0)
    except OSError:
        g.ret(-1 & 0xffffffff)

@shim('mkdir')
def _mkdir(g):
    try:
        os.makedirs(_hostpath(g, g.cstr(g.arg(0))), exist_ok=True)
        g.ret(0)
    except OSError:
        g.ret(-1 & 0xffffffff)

@shim('remove')
def _remove(g):
    try:
        os.remove(_hostpath(g, g.cstr(g.arg(0))))
        g.ret(0)
    except OSError:
        g.ret(-1 & 0xffffffff)

@shim('chdir')
def _chdir(g):  g.ret(0)

@shim('dup')
def _dup(g):    g.ret(g.arg(0))          # same descriptor is fine here

@shim('close', 'fcntl')
def _fdops(g):  g.ret(0)


# ------------------------------------------------------------- varargs I/O
def _fmt(g, fmt, argi):
    """Render a C format string using ARM32 varargs starting at arg index."""
    out, i, n = [], 0, argi
    while i < len(fmt):
        c = fmt[i]
        if c != '%':
            out.append(c); i += 1; continue
        j = i + 1
        while j < len(fmt) and fmt[j] in '-+ #0123456789.*lhqLzjt':
            j += 1
        if j >= len(fmt):
            break
        conv, spec = fmt[j], fmt[i:j + 1]
        longlong = 'll' in spec or 'q' in spec
        spec = spec.replace('ll', '').replace('l', '').replace('h', '').replace('q', '')
        try:
            if conv == '%':
                out.append('%')
            elif conv in 'di':
                if longlong:
                    if n % 2: n += 1
                    v = g.arg(n) | (g.arg(n + 1) << 32); n += 2
                else:
                    v = g.arg(n); n += 1
                    v = v - (1 << 32) if v >= (1 << 31) else v
                out.append(spec % v)
            elif conv in 'uxXo':
                v = g.arg(n); n += 1
                out.append(spec % v)
            elif conv in 'fFeEgG':
                if n % 2: n += 1
                out.append(spec % g.argd(n)); n += 2
            elif conv == 'c':
                out.append(chr(g.arg(n) & 0xff)); n += 1
            elif conv == 's':
                out.append(g.cstr(g.arg(n)) or "(null)"); n += 1
            elif conv == 'p':
                out.append("0x%x" % g.arg(n)); n += 1
            else:
                out.append(spec)
        except Exception:
            out.append(spec)
        i = j + 1
    return ''.join(out)

@shim('__android_log_print')
def _logprint(g):
    tag, fmt = g.cstr(g.arg(1)), g.cstr(g.arg(2)) or ''
    msg = _fmt(g, fmt, 3)
    print("      [logcat/%s] %s" % (tag, msg.rstrip()))
    g.ret(0)

@shim('fprintf')
def _fprintf(g):
    fmt = g.cstr(g.arg(1)) or ''
    msg = _fmt(g, fmt, 2)
    fh = g.files.get(g.arg(0))
    if fh:
        fh.write(msg.encode('utf-8', 'replace'))
    else:
        print("      [stderr] %s" % msg.rstrip())
    g.ret(len(msg))


# ----------------------------------------------------------------- runtime
@shim('__errno')
def _errno_(g):  g.ret(g.errno_addr)

@shim('abort')
def _abort(g):   raise GuestError("guest called abort()")

@shim('__stack_chk_fail')
def _sschk(g):   raise GuestError("stack smashing detected in guest")

@shim('__gnu_Unwind_Find_exidx')
def _find_exidx(g):
    base, count = g.exidx
    if g.arg(1):
        g.write(g.arg(1), struct.pack('<I', count))
    g.ret(base)

@shim('__cxa_begin_cleanup', '__cxa_type_match', '__cxa_call_unexpected')
def _cxa(g):     g.ret(1)


# ----------------------------------------------------------------- sockets
# Failure conventions differ and it matters: the syscall-style calls report
# -1, but gethostbyname returns a POINTER and must report NULL.  Returning
# -1 there makes the engine treat 0xffffffff as a valid struct hostent* and
# dereference it -- which is exactly how UE_ResolveHostAddress crashed.
def _fail_minus1(g):  g.ret(0xffffffff)
def _fail_null(g):    g.ret(0)

for _n in ('socket', 'bind', 'connect', 'send', 'recv', 'sendto', 'recvfrom',
           'sendmsg', 'setsockopt', 'gethostname'):
    REG[_n] = _fail_minus1
REG['gethostbyname'] = _fail_null


# ---------------------------------------------------------------- graphics
def _gl_record(name):
    """Desktop harness has no GL context; record the call and continue.
    On device these become direct forwards to the real driver."""
    def f(g):
        g.gl_log.append((name, [g.arg(i) for i in range(10)]))
        if name in ('glGenTextures', 'glGenBuffers'):
            cnt, ptr = g.arg(0), g.arg(1)
            for i in range(cnt):
                g._glid = getattr(g, '_glid', 0) + 1
                g.write(ptr + i * 4, struct.pack('<I', g._glid))
        g.ret(0)
    return f
