"""
gl.py -- forward the guest's OpenGL ES 1.x calls to a real GL driver (M4)

Three things have to be translated at this boundary:

  * Soft-float ABI.  A GLfloat argument arrives as raw IEEE-754 bits sitting
    in an integer register, so every float parameter must be reinterpreted
    rather than read.
  * Fixed point.  The engine was written for FPU-less phones and uses the
    GLES `x` entry points (glOrthox, glLoadMatrixx, glMultMatrixx), where
    values are 16.16 fixed point.  Desktop GL has no such calls.
  * Pointers.  Vertex and texture data live in *guest* memory, so client
    arrays have to be copied out at draw time -- the engine rewrites that
    buffer between draws from a single glVertexPointer set up once.
"""
import ctypes, struct, zlib
from OpenGL.GL import *
from OpenGL import GL

GL_VERTEX_ARRAY_        = 0x8074
GL_NORMAL_ARRAY_        = 0x8075
GL_COLOR_ARRAY_         = 0x8076
GL_TEXTURE_COORD_ARRAY_ = 0x8078
FIXED = 1.0 / 65536.0

_COMPONENTS = {0x1906: 1, 0x1907: 3, 0x1908: 4, 0x1909: 1, 0x190A: 2}   # A,RGB,RGBA,LUM,LUM_A
_TYPESIZE = {0x1401: 1, 0x1400: 1, 0x1402: 2, 0x1403: 2, 0x1406: 4, 0x140C: 4}
_PACKED = {0x8033: 2, 0x8034: 2, 0x8363: 2}                             # 4444, 5551, 565
_GETN = {0x0BA2: 4, 0x0C10: 4, 0x0BA6: 16, 0x0BA7: 16, 0x0D33: 1}


def _texel_bytes(fmt, typ):
    if typ in _PACKED:
        return _PACKED[typ]
    return _COMPONENTS.get(fmt, 4) * _TYPESIZE.get(typ, 1)


class GLBackend:
    def __init__(self, g, width=480, height=800, out=None):
        self.g = g
        self.w, self.h = width, height
        self.out = out                       # (win_w, win_h) enables scaling
        self.fbo = None
        self.arrays = {}          # cap -> (size, type, stride, guest_ptr)
        self.enabled = set()
        self.tex_map = {}         # guest texture name -> real name
        self.buf_map = {}
        self.draws = 0
        self._install()
        if out:
            self._make_target()

    # ------------------------------------------------------- scaled output
    def _make_target(self):
        """The game is authored for a 480x800 portrait panel.  Rather than lie
        to it about the resolution, render into an offscreen buffer at its
        native size and scale that up, preserving aspect ratio."""
        self.fbo = glGenFramebuffers(1)
        self.colour = glGenTextures(1)
        glBindTexture(GL_TEXTURE_2D, self.colour)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, self.w, self.h, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, None)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
        self.depth = glGenRenderbuffers(1)
        glBindRenderbuffer(GL_RENDERBUFFER, self.depth)
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, self.w, self.h)
        glBindFramebuffer(GL_FRAMEBUFFER, self.fbo)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, self.colour, 0)
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, self.depth)
        if glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE:
            raise RuntimeError("offscreen target incomplete")
        # A second target holds the final composite.  We cannot letterbox into
        # the window's own framebuffer: the window manager clamps a window to
        # the physical display, so asking for a 1080x2400 phone panel on a
        # smaller monitor silently gives a smaller buffer and the capture is
        # cropped.  Compositing offscreen makes the output size exact.
        ow, oh = self.out
        self.out_fbo = glGenFramebuffers(1)
        self.out_colour = glGenTextures(1)
        glBindTexture(GL_TEXTURE_2D, self.out_colour)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ow, oh, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, None)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
        glBindFramebuffer(GL_FRAMEBUFFER, self.out_fbo)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, self.out_colour, 0)
        if glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE:
            raise RuntimeError("output target incomplete")
        glBindFramebuffer(GL_FRAMEBUFFER, 0)

    def viewport(self):
        """Letterbox rect for the game inside the output window."""
        ow, oh = self.out
        scale = min(ow / float(self.w), oh / float(self.h))
        vw, vh = int(self.w * scale), int(self.h * scale)
        return (ow - vw) // 2, (oh - vh) // 2, vw, vh

    def begin_frame(self):
        if self.fbo:
            glBindFramebuffer(GL_FRAMEBUFFER, self.fbo)
            glViewport(0, 0, self.w, self.h)

    def end_frame(self, present=None):
        """Letterbox the game into the output target, then optionally show it."""
        if not self.fbo:
            return
        ow, oh = self.out
        x, y, vw, vh = self.viewport()
        glBindFramebuffer(GL_READ_FRAMEBUFFER, self.fbo)
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, self.out_fbo)
        glViewport(0, 0, ow, oh)
        glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT)
        glBlitFramebuffer(0, 0, self.w, self.h, x, y, x + vw, y + vh,
                          GL_COLOR_BUFFER_BIT, GL_LINEAR)
        if present:
            pw, ph = present
            sc = min(pw / float(ow), oh and ph / float(oh))
            bw, bh = int(ow * sc), int(oh * sc)
            bx, by = (pw - bw) // 2, (ph - bh) // 2
            glBindFramebuffer(GL_READ_FRAMEBUFFER, self.out_fbo)
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0)
            glViewport(0, 0, pw, ph)
            glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT)
            glBlitFramebuffer(0, 0, ow, oh, bx, by, bx + bw, by + bh,
                              GL_COLOR_BUFFER_BIT, GL_LINEAR)
        glBindFramebuffer(GL_FRAMEBUFFER, 0)

    def bind_capture(self):
        """Point glReadPixels at whichever buffer holds the finished image."""
        glBindFramebuffer(GL_READ_FRAMEBUFFER, self.out_fbo if self.fbo else 0)

    # ------------------------------------------------------------- helpers
    def f(self, i):
        """Argument i is a float carried as raw bits (soft-float ABI)."""
        return struct.unpack('<f', struct.pack('<I', self.g.arg(i)))[0]

    def x(self, i):
        """Argument i is 16.16 fixed point."""
        v = self.g.arg(i)
        return (v - (1 << 32) if v >= (1 << 31) else v) * FIXED

    def _install(self):
        for name in [n for n in self.g.imports if n.startswith('gl')]:
            fn = getattr(self, 'c_' + name, None)
            self.g.shims[name] = (lambda gg, f=fn: f()) if fn else (lambda gg: gg.ret(0))

    def tex(self, name):
        if name == 0:
            return 0
        if name not in self.tex_map:
            self.tex_map[name] = int(glGenTextures(1))
        return self.tex_map[name]

    # ------------------------------------------------------------ state
    def c_glClear(self):          glClear(self.g.arg(0)); self.g.ret(0)
    def c_glClearColor(self):
        glClearColor(self.f(0), self.f(1), self.f(2), self.f(3)); self.g.ret(0)
    def c_glEnable(self):         glEnable(self.g.arg(0)); self.g.ret(0)
    def c_glDisable(self):        glDisable(self.g.arg(0)); self.g.ret(0)
    def c_glBlendFunc(self):      glBlendFunc(self.g.arg(0), self.g.arg(1)); self.g.ret(0)
    def c_glDepthMask(self):      glDepthMask(bool(self.g.arg(0))); self.g.ret(0)
    def c_glFrontFace(self):      glFrontFace(self.g.arg(0)); self.g.ret(0)
    def c_glPixelStorei(self):    glPixelStorei(self.g.arg(0), self.g.arg(1)); self.g.ret(0)
    def c_glFinish(self):         glFinish(); self.g.ret(0)
    def c_glColor4ub(self):
        glColor4ub(self.g.arg(0) & 0xff, self.g.arg(1) & 0xff,
                   self.g.arg(2) & 0xff, self.g.arg(3) & 0xff)
        self.g.ret(0)

    # ------------------------------------------------------------ matrices
    def c_glMatrixMode(self):     glMatrixMode(self.g.arg(0)); self.g.ret(0)
    def c_glLoadIdentity(self):   glLoadIdentity(); self.g.ret(0)
    def c_glPushMatrix(self):     glPushMatrix(); self.g.ret(0)
    def c_glPopMatrix(self):      glPopMatrix(); self.g.ret(0)
    def c_glTranslatef(self):     glTranslatef(self.f(0), self.f(1), self.f(2)); self.g.ret(0)
    def c_glScalef(self):         glScalef(self.f(0), self.f(1), self.f(2)); self.g.ret(0)
    def c_glRotatef(self):
        glRotatef(self.f(0), self.f(1), self.f(2), self.f(3)); self.g.ret(0)

    def c_glOrthox(self):
        glOrtho(self.x(0), self.x(1), self.x(2), self.x(3), self.x(4), self.x(5))
        self.g.ret(0)

    def _matrix_from_guest(self, ptr):
        raw = self.g.read(ptr, 64)
        vals = struct.unpack('<16i', raw)
        return [v * FIXED for v in vals]

    def c_glLoadMatrixx(self):
        glLoadMatrixf(self._matrix_from_guest(self.g.arg(0))); self.g.ret(0)

    def c_glMultMatrixx(self):
        glMultMatrixf(self._matrix_from_guest(self.g.arg(0))); self.g.ret(0)

    def c_glClipPlanef(self):
        p = self.g.arg(1)
        vals = struct.unpack('<4f', self.g.read(p, 16)) if p else (0, 0, 0, 0)
        glClipPlane(self.g.arg(0), [float(v) for v in vals]); self.g.ret(0)

    # ------------------------------------------------------------ textures
    def c_glGenTextures(self):
        n, ptr = self.g.arg(0), self.g.arg(1)
        for i in range(n):
            name = len(self.tex_map) + 1 + i
            self.tex_map[name] = int(glGenTextures(1))
            self.g.write(ptr + i * 4, struct.pack('<I', name))
        self.g.ret(0)

    def c_glDeleteTextures(self):
        n, ptr = self.g.arg(0), self.g.arg(1)
        for i in range(n):
            name = struct.unpack('<I', self.g.read(ptr + i * 4, 4))[0]
            if name in self.tex_map:
                glDeleteTextures([self.tex_map.pop(name)])
        self.g.ret(0)

    def c_glBindTexture(self):
        glBindTexture(self.g.arg(0), self.tex(self.g.arg(1))); self.g.ret(0)

    def c_glTexParameteri(self):
        glTexParameteri(self.g.arg(0), self.g.arg(1), self.g.arg(2)); self.g.ret(0)

    def c_glTexParameterf(self):
        glTexParameterf(self.g.arg(0), self.g.arg(1), self.f(2)); self.g.ret(0)

    def c_glTexImage2D(self):
        a = [self.g.arg(i) for i in range(9)]
        tgt, lvl, ifmt, w, h, border, fmt, typ, ptr = a
        n = w * h * _texel_bytes(fmt, typ)
        data = self.g.read(ptr, n) if ptr and n else None
        glTexImage2D(tgt, lvl, _COMPONENTS.get(fmt, 4), w, h, border, fmt, typ, data)
        self.g.ret(0)

    def c_glTexSubImage2D(self):
        a = [self.g.arg(i) for i in range(9)]
        tgt, lvl, xo, yo, w, h, fmt, typ, ptr = a
        n = w * h * _texel_bytes(fmt, typ)
        if ptr and n:
            glTexSubImage2D(tgt, lvl, xo, yo, w, h, fmt, typ, self.g.read(ptr, n))
        self.g.ret(0)

    # -------------------------------------------------------- client arrays
    def c_glEnableClientState(self):
        cap = self.g.arg(0); self.enabled.add(cap); glEnableClientState(cap); self.g.ret(0)

    def c_glDisableClientState(self):
        cap = self.g.arg(0); self.enabled.discard(cap); glDisableClientState(cap); self.g.ret(0)

    def c_glVertexPointer(self):
        self.arrays[GL_VERTEX_ARRAY_] = tuple(self.g.arg(i) for i in range(4)); self.g.ret(0)

    def c_glTexCoordPointer(self):
        self.arrays[GL_TEXTURE_COORD_ARRAY_] = tuple(self.g.arg(i) for i in range(4)); self.g.ret(0)

    def _upload(self, count):
        """Copy each enabled client array out of guest memory for this draw."""
        keep = []
        for cap in (GL_VERTEX_ARRAY_, GL_TEXTURE_COORD_ARRAY_):
            if cap not in self.enabled or cap not in self.arrays:
                continue
            size, typ, stride, ptr = self.arrays[cap]
            if not ptr:
                continue
            step = stride or size * _TYPESIZE.get(typ, 4)
            nbytes = step * count
            buf = ctypes.create_string_buffer(self.g.read(ptr, nbytes), nbytes)
            keep.append(buf)
            if typ == 0x140C:                      # GL_FIXED has no desktop equivalent
                ints = struct.unpack('<%di' % (nbytes // 4), bytes(buf)[:nbytes // 4 * 4])
                flt = struct.pack('<%df' % len(ints), *[v * FIXED for v in ints])
                buf = ctypes.create_string_buffer(flt, len(flt))
                keep.append(buf); typ = 0x1406
            if cap == GL_VERTEX_ARRAY_:
                glVertexPointer(size, typ, stride, buf)
            else:
                glTexCoordPointer(size, typ, stride, buf)
        return keep

    def c_glDrawArrays(self):
        mode, first, count = self.g.arg(0), self.g.arg(1), self.g.arg(2)
        if count > 0:
            keep = self._upload(first + count)          # noqa: F841 (must outlive the call)
            glDrawArrays(mode, first, count)
            self.draws += 1
        self.g.ret(0)

    def c_glDrawElements(self):
        mode, count, typ, ptr = (self.g.arg(i) for i in range(4))
        if count > 0 and ptr:
            isz = 1 if typ == 0x1401 else 2
            idx = self.g.read(ptr, count * isz)
            mx = max(struct.unpack('<%d%s' % (count, 'B' if isz == 1 else 'H'), idx)) + 1
            keep = self._upload(mx)                     # noqa: F841
            glDrawElements(mode, count, typ, idx)
            self.draws += 1
        self.g.ret(0)

    # ------------------------------------------------------------- queries
    def c_glGetIntegerv(self):
        pname, ptr = self.g.arg(0), self.g.arg(1)
        try:
            vals = glGetIntegerv(pname)
        except Exception:
            vals = 0
        vals = [int(v) for v in (vals if hasattr(vals, '__len__') else [vals])]
        for i, v in enumerate(vals[:_GETN.get(pname, 1)]):
            self.g.write(ptr + i * 4, struct.pack('<i', int(v)))
        self.g.ret(0)

    def c_glGetFloatv(self):
        pname, ptr = self.g.arg(0), self.g.arg(1)
        try:
            vals = glGetFloatv(pname)
        except Exception:
            vals = 0.0
        flat = []
        if hasattr(vals, 'flatten'):
            flat = [float(v) for v in vals.flatten()]
        elif hasattr(vals, '__len__'):
            flat = [float(v) for v in vals]
        else:
            flat = [float(vals)]
        for i, v in enumerate(flat[:_GETN.get(pname, 1)]):
            self.g.write(ptr + i * 4, struct.pack('<f', v))
        self.g.ret(0)

    def c_glReadPixels(self):
        x, y, w, h, fmt, typ, ptr = (self.g.arg(i) for i in range(7))
        data = glReadPixels(x, y, w, h, fmt, typ)
        if ptr and data is not None:
            self.g.write(ptr, bytes(data)[:w * h * _texel_bytes(fmt, typ)])
        self.g.ret(0)

    # buffers: the engine never binds one, but keep the entry points honest
    def c_glGenBuffers(self):
        n, ptr = self.g.arg(0), self.g.arg(1)
        for i in range(n):
            self.g.write(ptr + i * 4, struct.pack('<I', len(self.buf_map) + 1 + i))
        self.g.ret(0)
    def c_glDeleteBuffers(self):  self.g.ret(0)
    def c_glBindBuffer(self):     self.g.ret(0)
    def c_glBufferData(self):     self.g.ret(0)


# ------------------------------------------------------------------ capture
def save_png(path, width, height, backend=None):
    """Grab the framebuffer and write a PNG (flipped: GL origin is bottom-left)."""
    if backend is not None:
        backend.bind_capture()
    glPixelStorei(GL_PACK_ALIGNMENT, 1)
    raw = glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE)
    raw = bytes(raw)
    rows = [raw[y * width * 3:(y + 1) * width * 3] for y in range(height)][::-1]
    body = b''.join(b'\x00' + r for r in rows)

    def chunk(tag, data):
        c = struct.pack('>I', len(data)) + tag + data
        return c + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)

    png = (b'\x89PNG\r\n\x1a\n'
           + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
           + chunk(b'IDAT', zlib.compress(body, 9))
           + chunk(b'IEND', b''))
    open(path, 'wb').write(png)
    if backend is not None:
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0)
    return path
