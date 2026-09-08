// gl.cpp -- forward the guest's OpenGL ES 1.x calls to the device driver.
//
// Ported from runtime/gl.py, and simpler here than it was on the desktop.
// Two of the three translations the desktop needed still apply:
//
//   * Soft-float ABI. A GLfloat argument arrives as raw IEEE-754 bits in an
//     integer register, so every float parameter is reinterpreted, not read.
//   * Client arrays live in guest memory. glVertexPointer is called ONCE for
//     the whole run and the engine rewrites that buffer between draws, so the
//     data must be copied out at every draw call.
//
// The third does not: desktop GL has no glOrthox/glLoadMatrixx/glMultMatrixx,
// so the harness converted 16.16 fixed point to float. Android's GLES 1.x has
// those entry points natively, so here they forward straight through -- this
// path is closer to what the engine was written against than the desktop was.
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <android/log.h>

#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "jni_bridge.h"
#include "shogun_runtime.h"

namespace shogun {
namespace {

constexpr uint32_t kVertexArray   = 0x8074;
constexpr uint32_t kNormalArray   = 0x8075;
constexpr uint32_t kColorArray    = 0x8076;
constexpr uint32_t kTexCoordArray = 0x8078;

// `ptr` is a guest address for a client array, or a byte offset into `vbo`
// when a buffer was bound at the time the pointer was set.
struct ArraySpec { uint32_t size, type, stride, ptr, vbo; };

struct CallStat { uint64_t n = 0; };

// One mip level of a texture, kept so it can be re-uploaded from scratch.
struct TexImage {
  uint32_t level, ifmt, w, h, border, fmt, type;
  std::vector<uint8_t> px;
};

struct GlState {
  std::unordered_map<std::string, uint64_t> calls;   // per-entry-point counts
  std::unordered_map<uint32_t, uint64_t> enables;    // glEnable(cap) counts
  std::string last_call;
  uint32_t first_error = 0;
  std::string error_from;
  // last seen client-array specs, logged when they change
  std::string arrays_desc;
  std::unordered_map<uint32_t, ArraySpec> arrays;
  std::unordered_map<uint32_t, bool> enabled;
  std::unordered_map<uint32_t, GLuint> tex;     // guest name -> real name
  std::unordered_map<uint32_t, GLuint> buf;     // guest buffer -> real buffer
  // A CPU-side copy of every buffer's contents. Needed because an indexed draw
  // whose indices live in an element buffer still has to know how many
  // vertices a *client-side* array must supply, and GLES1 cannot read a buffer
  // back. Without it those arrays were sized zero and the driver read garbage.
  std::unordered_map<uint32_t, std::vector<uint8_t>> bufdata;
  uint32_t bound_array = 0, bound_elem = 0;     // currently bound guest names
  // Names must be handed out monotonically. Sizing them from the map is wrong
  // because RealBuf/RealTex also insert on demand for names the guest invented,
  // so a later Gen could reissue a live name and two meshes would share one
  // buffer -- geometry appears for a frame, then gets overwritten.
  uint32_t next_buf = 1, next_tex = 1;
  // ---- everything needed to rebuild GL state after an EGL context loss ----
  // Android may destroy the context while the app is in the background. The
  // engine has no idea that happened: it keeps drawing with the same texture
  // and buffer names, which now refer to nothing, and the game comes back
  // untextured. It never re-uploads, so we have to be able to.
  std::unordered_map<uint32_t, std::vector<TexImage>> teximg;   // name -> levels
  std::unordered_map<uint32_t, std::unordered_map<uint32_t, int32_t>> texparam;
  std::unordered_map<uint32_t, uint32_t> buftarget;   // buffer name -> target
  std::unordered_map<uint32_t, bool> srv_enabled;     // glEnable/glDisable caps
  uint32_t bound_tex = 0;
  size_t shadow_bytes = 0;
  uint64_t context_losses = 0;
  int trace = 0;   // countdown of call-sequence lines to emit
  std::vector<uint8_t> staging[4];
  uint64_t draws = 0;
};
GlState g;

inline float AsFloat(uint32_t bits) { float f; std::memcpy(&f, &bits, 4); return f; }

size_t TypeSize(uint32_t t) {
  switch (t) {
    case GL_BYTE: case GL_UNSIGNED_BYTE: return 1;
    case GL_SHORT: case GL_UNSIGNED_SHORT: return 2;
    case GL_FLOAT: case GL_FIXED: return 4;
    default: return 4;
  }
}

size_t TexelBytes(uint32_t fmt, uint32_t type) {
  switch (type) {
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
    case GL_UNSIGNED_SHORT_5_6_5: return 2;
    default: break;
  }
  switch (fmt) {
    case GL_ALPHA: case GL_LUMINANCE: return 1;
    case GL_LUMINANCE_ALPHA: return 2;
    case GL_RGB: return 3;
    default: return 4;
  }
}

uint32_t NewBufName() {
  while (g.buf.count(g.next_buf)) g.next_buf++;
  return g.next_buf++;
}

uint32_t NewTexName() {
  while (g.tex.count(g.next_tex)) g.next_tex++;
  return g.next_tex++;
}

GLuint RealBuf(uint32_t name) {
  if (!name) return 0;
  auto it = g.buf.find(name);
  if (it != g.buf.end()) return it->second;
  GLuint b = 0;
  glGenBuffers(1, &b);
  g.buf[name] = b;
  return b;
}

GLuint RealTex(uint32_t name) {
  if (!name) return 0;
  auto it = g.tex.find(name);
  if (it != g.tex.end()) return it->second;
  GLuint t = 0;
  glGenTextures(1, &t);
  g.tex[name] = t;
  return t;
}

// Keep a copy of a full texture upload. Replacing a level replaces the copy,
// which is what glTexImage2D means.
void StoreLevel(uint32_t name, TexImage img) {
  auto& levels = g.teximg[name];
  for (auto& lv : levels) {
    if (lv.level == img.level) {
      g.shadow_bytes -= lv.px.size();
      g.shadow_bytes += img.px.size();
      lv = std::move(img);
      return;
    }
  }
  g.shadow_bytes += img.px.size();
  levels.push_back(std::move(img));
}

// Apply a partial upload to the copy, row by row -- a sub-image is a rectangle
// inside the level, not a contiguous run.
void PatchLevel(uint32_t name, uint32_t level, uint32_t xo, uint32_t yo,
                uint32_t w, uint32_t h, uint32_t fmt, uint32_t typ,
                const std::vector<uint8_t>& src) {
  auto it = g.teximg.find(name);
  if (it == g.teximg.end()) return;
  for (auto& lv : it->second) {
    if (lv.level != level) continue;
    const size_t tb = TexelBytes(fmt, typ);
    if (tb != TexelBytes(lv.fmt, lv.type)) return;   // reinterpreted; give up
    if (xo + w > lv.w || yo + h > lv.h) return;
    for (uint32_t row = 0; row < h; row++) {
      const size_t dst = ((yo + row) * static_cast<size_t>(lv.w) + xo) * tb;
      const size_t off = row * static_cast<size_t>(w) * tb;
      if (dst + w * tb > lv.px.size() || off + w * tb > src.size()) return;
      std::memcpy(lv.px.data() + dst, src.data() + off, w * tb);
    }
    return;
  }
}

// Copy each enabled client array out of guest memory for this draw.
void UploadArrays(Runtime& rt, uint32_t vertex_count) {
  int slot = 0;
  for (uint32_t cap : {kVertexArray, kTexCoordArray, kColorArray, kNormalArray}) {
    auto en = g.enabled.find(cap);
    if (en == g.enabled.end() || !en->second) continue;
    auto it = g.arrays.find(cap);
    if (it == g.arrays.end()) continue;
    const ArraySpec& a = it->second;
    // A VBO-backed array addresses its data by *offset*, and offset 0 is the
    // normal case. Treating 0 as "no pointer" skipped every mesh array and
    // left GL still pointing at the 2D sprite buffer -- which is what tore the
    // 3D geometry apart. Only a client array with a null pointer is skippable.
    if (!a.vbo && !a.ptr) continue;
    const void* base;
    if (a.vbo) {
      // Data already lives in a buffer object on the GPU; `ptr` is an offset
      // into it, so nothing is copied out of guest memory.
      glBindBuffer(GL_ARRAY_BUFFER, RealBuf(a.vbo));
      base = reinterpret_cast<const void*>(static_cast<uintptr_t>(a.ptr));
    } else {
      size_t step = a.stride ? a.stride : a.size * TypeSize(a.type);
      size_t bytes = step * vertex_count;
      if (!bytes) continue;          // nothing known to copy; leave GL untouched
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      auto& sbuf = g.staging[slot++];
      sbuf.resize(bytes);
      rt.Read(a.ptr, sbuf.data(), bytes);
      base = sbuf.data();
    }
    switch (cap) {
      case kVertexArray:   glVertexPointer(a.size, a.type, a.stride, base); break;
      case kTexCoordArray: glTexCoordPointer(a.size, a.type, a.stride, base); break;
      case kColorArray:    glColorPointer(a.size, a.type, a.stride, base); break;
      case kNormalArray:   glNormalPointer(a.type, a.stride, base); break;
      default: break;
    }
    if (slot >= 4) break;
  }
}

}  // namespace

void InstallGl(Runtime& rt) {
  auto S = [&rt](const char* n, ShimFn f) { rt.SetShim(n, std::move(f)); };
  auto F = [](Runtime& r, int i) { return AsFloat(r.Arg(i)); };

  // ---- plain state -------------------------------------------------------
  S("glClear",        [](Runtime& r) { glClear(r.Arg(0)); r.Ret(0); });
  // glClearColor takes GLfloat, not GLfixed. Feeding the raw soft-float bit
  // patterns to glClearColorx treats them as 16.16 fixed point, which clamps
  // to 1.0 and clears the screen white on every frame.
  S("glClearColor",   [F](Runtime& r) {
    glClearColor(F(r,0), F(r,1), F(r,2), F(r,3)); r.Ret(0); });
  S("glEnable",       [](Runtime& r) {
    g.srv_enabled[r.Arg(0)] = true;  glEnable(r.Arg(0));  r.Ret(0); });
  S("glDisable",      [](Runtime& r) {
    g.srv_enabled[r.Arg(0)] = false; glDisable(r.Arg(0)); r.Ret(0); });
  S("glBlendFunc",    [](Runtime& r) { glBlendFunc(r.Arg(0), r.Arg(1)); r.Ret(0); });
  S("glDepthMask",    [](Runtime& r) { glDepthMask(r.Arg(0) ? GL_TRUE : GL_FALSE); r.Ret(0); });
  S("glFrontFace",    [](Runtime& r) { glFrontFace(r.Arg(0)); r.Ret(0); });
  S("glPixelStorei",  [](Runtime& r) { glPixelStorei(r.Arg(0), r.Arg(1)); r.Ret(0); });
  S("glFinish",       [](Runtime& r) { glFinish(); r.Ret(0); });
  S("glColor4ub",     [](Runtime& r) {
    glColor4ub(r.Arg(0) & 0xff, r.Arg(1) & 0xff, r.Arg(2) & 0xff, r.Arg(3) & 0xff);
    r.Ret(0); });

  // ---- matrices: the fixed-point family forwards natively here -----------
  S("glMatrixMode",   [](Runtime& r) { glMatrixMode(r.Arg(0)); r.Ret(0); });
  S("glLoadIdentity", [](Runtime& r) { glLoadIdentity(); r.Ret(0); });
  S("glPushMatrix",   [](Runtime& r) { glPushMatrix(); r.Ret(0); });
  S("glPopMatrix",    [](Runtime& r) { glPopMatrix(); r.Ret(0); });
  S("glOrthox",       [](Runtime& r) {
    glOrthox(r.Arg(0), r.Arg(1), r.Arg(2), r.Arg(3), r.Arg(4), r.Arg(5)); r.Ret(0); });
  S("glLoadMatrixx",  [](Runtime& r) {
    GLfixed m[16]; r.Read(r.Arg(0), m, sizeof m);
    static int n = 0;
    if (n++ < 3) {
      GLint mode = 0; glGetIntegerv(GL_MATRIX_MODE, &mode);
      __android_log_print(ANDROID_LOG_INFO, "shogun",
        "glLoadMatrixx mode=0x%x [%.3f %.3f %.3f %.3f | %.3f %.3f %.3f %.3f | "
        "%.3f %.3f %.3f %.3f | %.3f %.3f %.3f %.3f]", mode,
        m[0]/65536.0, m[1]/65536.0, m[2]/65536.0, m[3]/65536.0,
        m[4]/65536.0, m[5]/65536.0, m[6]/65536.0, m[7]/65536.0,
        m[8]/65536.0, m[9]/65536.0, m[10]/65536.0, m[11]/65536.0,
        m[12]/65536.0, m[13]/65536.0, m[14]/65536.0, m[15]/65536.0);
    }
    glLoadMatrixx(m); r.Ret(0); });
  S("glMultMatrixx",  [](Runtime& r) {
    GLfixed m[16]; r.Read(r.Arg(0), m, sizeof m);
    static int n = 0;
    if (n++ < 3)
      __android_log_print(ANDROID_LOG_INFO, "shogun",
        "glMultMatrixx full [%.3f %.3f %.3f %.3f | %.3f %.3f %.3f %.3f | "
        "%.3f %.3f %.3f %.3f | %.3f %.3f %.3f %.3f]",
        m[0]/65536.0, m[1]/65536.0, m[2]/65536.0, m[3]/65536.0,
        m[4]/65536.0, m[5]/65536.0, m[6]/65536.0, m[7]/65536.0,
        m[8]/65536.0, m[9]/65536.0, m[10]/65536.0, m[11]/65536.0,
        m[12]/65536.0, m[13]/65536.0, m[14]/65536.0, m[15]/65536.0);
    glMultMatrixx(m); r.Ret(0); });
  S("glTranslatef",   [F](Runtime& r) { glTranslatef(F(r,0), F(r,1), F(r,2)); r.Ret(0); });
  S("glScalef",       [F](Runtime& r) { glScalef(F(r,0), F(r,1), F(r,2)); r.Ret(0); });
  S("glRotatef",      [F](Runtime& r) {
    glRotatef(F(r,0), F(r,1), F(r,2), F(r,3)); r.Ret(0); });
  S("glClipPlanef",   [](Runtime& r) {
    GLfloat p[4] = {0,0,0,0};
    if (r.Arg(1)) r.Read(r.Arg(1), p, sizeof p);
    static int n = 0;
    if (n++ < 4)
      __android_log_print(ANDROID_LOG_INFO, "shogun",
        "glClipPlanef plane=0x%x [%.4f %.4f %.4f %.4f] enabled=%d",
        r.Arg(0), p[0], p[1], p[2], p[3],
        glIsEnabled(GL_CLIP_PLANE0));
    glClipPlanef(r.Arg(0), p); r.Ret(0); });

  // ---- textures ----------------------------------------------------------
  S("glGenTextures",  [](Runtime& r) {
    uint32_t n = r.Arg(0), ptr = r.Arg(1);
    for (uint32_t i = 0; i < n; i++) {
      uint32_t name = NewTexName();
      GLuint t = 0; glGenTextures(1, &t);
      g.tex[name] = t;
      r.Write(ptr + i * 4, &name, 4);
    }
    r.Ret(0); });
  S("glDeleteTextures", [](Runtime& r) {
    uint32_t n = r.Arg(0), ptr = r.Arg(1);
    for (uint32_t i = 0; i < n; i++) {
      uint32_t name = 0; r.Read(ptr + i * 4, &name, 4);
      auto it = g.tex.find(name);
      if (it != g.tex.end()) { glDeleteTextures(1, &it->second); g.tex.erase(it); }
      auto im = g.teximg.find(name);
      if (im != g.teximg.end()) {
        for (const auto& lv : im->second) g.shadow_bytes -= lv.px.size();
        g.teximg.erase(im);
      }
      g.texparam.erase(name);
      if (g.bound_tex == name) g.bound_tex = 0;
    }
    r.Ret(0); });
  S("glBindTexture",  [](Runtime& r) {
    g.bound_tex = r.Arg(1);
    glBindTexture(r.Arg(0), RealTex(r.Arg(1))); r.Ret(0); });
  S("glTexParameteri", [](Runtime& r) {
    if (g.bound_tex)
      g.texparam[g.bound_tex][r.Arg(1)] = static_cast<int32_t>(r.Arg(2));
    glTexParameteri(r.Arg(0), r.Arg(1), r.Arg(2)); r.Ret(0); });
  S("glTexParameterf", [F](Runtime& r) {
    glTexParameterf(r.Arg(0), r.Arg(1), F(r,2)); r.Ret(0); });
  S("glTexImage2D",   [](Runtime& r) {
    uint32_t tgt=r.Arg(0), lvl=r.Arg(1), ifmt=r.Arg(2), w=r.Arg(3), h=r.Arg(4),
             bd=r.Arg(5), fmt=r.Arg(6), typ=r.Arg(7), ptr=r.Arg(8);
    size_t n = static_cast<size_t>(w) * h * TexelBytes(fmt, typ);
    std::vector<uint8_t> buf;
    if (ptr && n) { buf.resize(n); r.Read(ptr, buf.data(), n); }
    if (g.bound_tex) StoreLevel(g.bound_tex, {lvl, ifmt, w, h, bd, fmt, typ, buf});
    glTexImage2D(tgt, lvl, ifmt, w, h, bd, fmt, typ, buf.empty() ? nullptr : buf.data());
    r.Ret(0); });
  S("glTexSubImage2D", [](Runtime& r) {
    uint32_t tgt=r.Arg(0), lvl=r.Arg(1), xo=r.Arg(2), yo=r.Arg(3), w=r.Arg(4),
             h=r.Arg(5), fmt=r.Arg(6), typ=r.Arg(7), ptr=r.Arg(8);
    size_t n = static_cast<size_t>(w) * h * TexelBytes(fmt, typ);
    if (ptr && n) {
      std::vector<uint8_t> buf(n);
      r.Read(ptr, buf.data(), n);
      if (g.bound_tex) PatchLevel(g.bound_tex, lvl, xo, yo, w, h, fmt, typ, buf);
      glTexSubImage2D(tgt, lvl, xo, yo, w, h, fmt, typ, buf.data());
    }
    r.Ret(0); });

  // ---- client arrays -----------------------------------------------------
  S("glEnableClientState",  [](Runtime& r) {
    uint32_t c = r.Arg(0); g.enabled[c] = true;  glEnableClientState(c);  r.Ret(0); });
  S("glDisableClientState", [](Runtime& r) {
    uint32_t c = r.Arg(0); g.enabled[c] = false; glDisableClientState(c); r.Ret(0); });
  // Whatever buffer is bound *now* is the one this pointer refers to.
  S("glVertexPointer",   [](Runtime& r) {
    // Log the mesh (VBO-backed) configuration, which is the 3D path.
    if (g.trace > 0) { g.trace--;
      __android_log_print(ANDROID_LOG_INFO, "shogun",
        "seq vertexPointer size=%u type=0x%04x stride=%u ptr=0x%08x boundArray=%u",
        r.Arg(0), r.Arg(1), r.Arg(2), r.Arg(3), g.bound_array); }
    g.arrays[kVertexArray]   = {r.Arg(0), r.Arg(1), r.Arg(2), r.Arg(3),
                                g.bound_array}; r.Ret(0); });
  S("glTexCoordPointer", [](Runtime& r) {
    if (g.trace > 0) { g.trace--;
      __android_log_print(ANDROID_LOG_INFO, "shogun",
        "seq texCoordPointer size=%u type=0x%04x stride=%u ptr=0x%08x boundArray=%u",
        r.Arg(0), r.Arg(1), r.Arg(2), r.Arg(3), g.bound_array); }
    g.arrays[kTexCoordArray] = {r.Arg(0), r.Arg(1), r.Arg(2), r.Arg(3),
                                g.bound_array}; r.Ret(0); });

  S("glDrawArrays", [](Runtime& r) {
    uint32_t mode = r.Arg(0), first = r.Arg(1), count = r.Arg(2);
    if (count) {
      UploadArrays(r, first + count);
      glDrawArrays(mode, first, count);
      g.draws++;
    }
    r.Ret(0); });
  S("glDrawElements", [](Runtime& r) {
    uint32_t mode = r.Arg(0), count = r.Arg(1), type = r.Arg(2), ptr = r.Arg(3);
    if (count) {
      if (g.bound_elem) {
        // Indices live in an element buffer; `ptr` is an offset into it. Read
        // them from our shadow copy so client-side arrays still get sized.
        size_t isz = (type == GL_UNSIGNED_BYTE) ? 1 : 2;
        uint32_t maxi = 0;
        auto bd = g.bufdata.find(g.bound_elem);
        if (bd != g.bufdata.end() && ptr + count * isz <= bd->second.size()) {
          const uint8_t* p = bd->second.data() + ptr;
          for (uint32_t i = 0; i < count; i++) {
            uint32_t v = (isz == 1) ? p[i]
                                    : static_cast<uint32_t>(p[i*2] | (p[i*2+1] << 8));
            maxi = std::max(maxi, v);
          }
        }
        UploadArrays(r, maxi + 1);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, RealBuf(g.bound_elem));
        glDrawElements(mode, count, type,
                       reinterpret_cast<const void*>(static_cast<uintptr_t>(ptr)));
        g.draws++;
      } else if (ptr) {
        size_t isz = (type == GL_UNSIGNED_BYTE) ? 1 : 2;
        std::vector<uint8_t> idx(count * isz);
        r.Read(ptr, idx.data(), idx.size());
        uint32_t maxi = 0;
        for (uint32_t i = 0; i < count; i++) {
          uint32_t v = (isz == 1) ? idx[i]
                                  : static_cast<uint32_t>(idx[i*2] | (idx[i*2+1] << 8));
          maxi = std::max(maxi, v);
        }
        UploadArrays(r, maxi + 1);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glDrawElements(mode, count, type, idx.data());
        g.draws++;
      }
    }
    r.Ret(0); });

  // ---- queries -----------------------------------------------------------
  S("glGetIntegerv", [](Runtime& r) {
    GLint v[16] = {0};
    glGetIntegerv(r.Arg(0), v);
    uint32_t n = (r.Arg(0) == GL_VIEWPORT || r.Arg(0) == GL_SCISSOR_BOX) ? 4 : 1;
    for (uint32_t i = 0; i < n; i++) r.Write(r.Arg(1) + i * 4, &v[i], 4);
    r.Ret(0); });
  S("glGetFloatv", [](Runtime& r) {
    GLfloat v[16] = {0};
    glGetFloatv(r.Arg(0), v);
    uint32_t p = r.Arg(0);
    uint32_t n = (p == GL_MODELVIEW_MATRIX || p == GL_PROJECTION_MATRIX) ? 16 : 1;
    for (uint32_t i = 0; i < n; i++) r.Write(r.Arg(1) + i * 4, &v[i], 4);
    r.Ret(0); });
  S("glReadPixels", [](Runtime& r) {
    uint32_t x=r.Arg(0), y=r.Arg(1), w=r.Arg(2), h=r.Arg(3),
             fmt=r.Arg(4), typ=r.Arg(5), ptr=r.Arg(6);
    size_t n = static_cast<size_t>(w) * h * TexelBytes(fmt, typ);
    if (ptr && n) {
      std::vector<uint8_t> buf(n);
      glReadPixels(x, y, w, h, fmt, typ, buf.data());
      r.Write(ptr, buf.data(), n);
    }
    r.Ret(0); });

  // Real buffer objects. The 3D background draws its meshes out of VBOs --
  // hundreds of thousands of binds per minute -- so stubbing these out left
  // the geometry with no vertex data and the background black.
  S("glGenBuffers", [](Runtime& r) {
    uint32_t n = r.Arg(0), ptr = r.Arg(1);
    for (uint32_t i = 0; i < n; i++) {
      uint32_t name = NewBufName();
      GLuint b = 0; glGenBuffers(1, &b);
      g.buf[name] = b;
      r.Write(ptr + i * 4, &name, 4);
    }
    r.Ret(0); });
  S("glDeleteBuffers", [](Runtime& r) {
    uint32_t n = r.Arg(0), ptr = r.Arg(1);
    for (uint32_t i = 0; i < n; i++) {
      uint32_t name = 0; r.Read(ptr + i * 4, &name, 4);
      auto it = g.buf.find(name);
      if (it != g.buf.end()) { glDeleteBuffers(1, &it->second); g.buf.erase(it); }
      auto bd = g.bufdata.find(name);
      if (bd != g.bufdata.end()) { g.shadow_bytes -= bd->second.size();
                                   g.bufdata.erase(bd); }
      g.buftarget.erase(name);
    }
    r.Ret(0); });
  S("glBindBuffer", [](Runtime& r) {
    uint32_t target = r.Arg(0), name = r.Arg(1);
    if (g.trace > 0) { g.trace--;
      __android_log_print(ANDROID_LOG_INFO, "shogun",
        "seq bindBuffer target=%s name=%u",
        target == GL_ARRAY_BUFFER ? "ARRAY" : "ELEM", name); }
    if (target == GL_ARRAY_BUFFER) g.bound_array = name;
    else if (target == GL_ELEMENT_ARRAY_BUFFER) g.bound_elem = name;
    if (name) g.buftarget[name] = target;
    glBindBuffer(target, RealBuf(name));
    r.Ret(0); });
  S("glBufferData", [](Runtime& r) {
    uint32_t target = r.Arg(0), size = r.Arg(1), ptr = r.Arg(2), usage = r.Arg(3);
    std::vector<uint8_t> data;
    if (ptr && size) { data.resize(size); r.Read(ptr, data.data(), size); }
    static int n = 0;
    if (n++ < 6) {
      char head[160] = {0};
      // first 8 words, so the vertex encoding is visible directly
      for (int i = 0; i < 8 && (i + 1) * 4 <= (int)data.size(); i++) {
        int32_t w; std::memcpy(&w, data.data() + i * 4, 4);
        float f; std::memcpy(&f, data.data() + i * 4, 4);
        char one[24];
        std::snprintf(one, sizeof one, "%.3f/%d ", f, w);
        std::strncat(head, one, sizeof head - std::strlen(head) - 1);
      }
      __android_log_print(ANDROID_LOG_INFO, "shogun",
        "glBufferData target=0x%x size=%u usage=0x%x buf=%u | asFloat/asInt: %s",
        target, size, usage,
        target == GL_ARRAY_BUFFER ? g.bound_array : g.bound_elem, head);
    }
    uint32_t name = (target == GL_ARRAY_BUFFER) ? g.bound_array : g.bound_elem;
    if (name) {
      auto& shadow = g.bufdata[name];
      g.shadow_bytes -= shadow.size();
      shadow.assign(size, 0);
      g.shadow_bytes += shadow.size();
      if (!data.empty()) std::memcpy(shadow.data(), data.data(),
                                     std::min<size_t>(size, data.size()));
    }
    glBufferData(target, size, data.empty() ? nullptr : data.data(), usage);
    r.Ret(0); });

  // Wrap every GL entry point so we can see which ones the 3D path uses and
  // catch the first driver error with the call that caused it.
  for (const auto& name : rt.imports()) {
    if (name.rfind("gl", 0) != 0) continue;
    ShimFn inner = rt.ShimFor(name);
    if (!inner) continue;
    std::string nm = name;
    rt.SetShim(nm, [nm, inner](Runtime& r) {
      g.calls[nm]++;
      g.last_call = nm;
      inner(r);
      if (!g.first_error) {
        GLenum e = glGetError();
        if (e) { g.first_error = e; g.error_from = nm; }
      }
    });
  }
}

uint64_t GlDrawCount() { return g.draws; }

// Called periodically from the tick logger.
// Android is free to throw the EGL context away while the app is backgrounded.
// setPreserveEGLContextOnPause(true) asks it not to, but that is a request, not
// a guarantee, and when it is refused every name the driver gave us dies with
// it. The engine is never told: it goes on drawing with the same texture and
// buffer names, which is why the game came back untextured after a minimise.
//
// So rebuild. Names the *guest* holds stay exactly as they were -- only the
// real GL names behind them are reissued -- and the shadows recorded above are
// replayed into them. Per-material state (blend, tex env, matrices) is not
// replayed because the engine flushes its own render state every frame.
void GlContextLost() {
  g.context_losses++;
  __android_log_print(ANDROID_LOG_WARN, "shogun",
      "EGL context lost (#%llu): rebuilding %zu textures, %zu buffers, %zu KB",
      static_cast<unsigned long long>(g.context_losses),
      g.teximg.size(), g.bufdata.size(), g.shadow_bytes / 1024);

  // The old names belong to a dead context; deleting them is meaningless and
  // would be a driver error. Just forget them.
  g.tex.clear();
  g.buf.clear();

  for (const auto& kv : g.teximg) {
    GLuint t = 0;
    glGenTextures(1, &t);
    g.tex[kv.first] = t;
    glBindTexture(GL_TEXTURE_2D, t);
    for (const auto& lv : kv.second)
      glTexImage2D(GL_TEXTURE_2D, lv.level, lv.ifmt, lv.w, lv.h, lv.border,
                   lv.fmt, lv.type, lv.px.empty() ? nullptr : lv.px.data());
    auto pit = g.texparam.find(kv.first);
    if (pit != g.texparam.end())
      for (const auto& pv : pit->second)
        glTexParameteri(GL_TEXTURE_2D, pv.first, pv.second);
  }

  for (const auto& kv : g.bufdata) {
    auto tg = g.buftarget.find(kv.first);
    const GLenum target = tg == g.buftarget.end() ? GL_ARRAY_BUFFER
                                                  : tg->second;
    GLuint b = 0;
    glGenBuffers(1, &b);
    g.buf[kv.first] = b;
    glBindBuffer(target, b);
    glBufferData(target, kv.second.size(),
                 kv.second.empty() ? nullptr : kv.second.data(),
                 GL_STATIC_DRAW);
  }

  // Client and server enables are context state too, and the engine only ever
  // set them once at init.
  for (const auto& kv : g.enabled)
    kv.second ? glEnableClientState(kv.first) : glDisableClientState(kv.first);
  for (const auto& kv : g.srv_enabled)
    kv.second ? glEnable(kv.first) : glDisable(kv.first);

  // Leave the bindings where the engine believes they are.
  glBindBuffer(GL_ARRAY_BUFFER, RealBuf(g.bound_array));
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, RealBuf(g.bound_elem));
  if (g.bound_tex) glBindTexture(GL_TEXTURE_2D, RealTex(g.bound_tex));

  GLenum e = glGetError();
  __android_log_print(ANDROID_LOG_INFO, "shogun",
      "GL restore complete, glGetError=0x%x", e);
}

std::string GlDiagnostics(Runtime& rt) {
  char buf[1024];
  std::string out;
  if (g.first_error) {
    std::snprintf(buf, sizeof buf, "GL ERROR 0x%04x first raised by %s | ",
                  g.first_error, g.error_from.c_str());
    out += buf;
    g.first_error = 0;
  }
  // which of the mesh-specific entry points has the engine actually used?
  for (const char* k : {"glDrawElements", "glDrawArrays", "glLoadMatrixx",
                        "glMultMatrixx", "glOrthox", "glClipPlanef",
                        "glBindBuffer", "glBufferData", "glGetIntegerv"}) {
    std::snprintf(buf, sizeof buf, "%s=%llu ", k,
                  static_cast<unsigned long long>(g.calls[k]));
    out += buf;
  }
  // current client-array configuration
  std::string desc;
  for (auto cap : {kVertexArray, kTexCoordArray, kColorArray, kNormalArray}) {
    auto it = g.arrays.find(cap);
    bool on = g.enabled.count(cap) && g.enabled[cap];
    if (it == g.arrays.end() && !on) continue;
    std::snprintf(buf, sizeof buf, "| cap%04x %s size=%u type=0x%04x stride=%u ptr=0x%08x ",
                  cap, on ? "ON " : "off",
                  it == g.arrays.end() ? 0 : it->second.size,
                  it == g.arrays.end() ? 0 : it->second.type,
                  it == g.arrays.end() ? 0 : it->second.stride,
                  it == g.arrays.end() ? 0 : it->second.ptr);
    desc += buf;
  }
  out += desc;
  // depth/cull state actually in effect
  GLboolean dt = glIsEnabled(GL_DEPTH_TEST), cf = glIsEnabled(GL_CULL_FACE),
            tex = glIsEnabled(GL_TEXTURE_2D), bl = glIsEnabled(GL_BLEND);
  GLint depth_bits = 0;
  glGetIntegerv(GL_DEPTH_BITS, &depth_bits);
  GLboolean cp0 = glIsEnabled(GL_CLIP_PLANE0), al = glIsEnabled(GL_ALPHA_TEST),
            lt = glIsEnabled(GL_LIGHTING);
  std::snprintf(buf, sizeof buf,
                "| depthTest=%d cull=%d tex2d=%d blend=%d depthBits=%d "
                "clipPlane0=%d alphaTest=%d lighting=%d",
                dt, cf, tex, bl, depth_bits, cp0, al, lt);
  out += buf;
  return out;
}

}  // namespace shogun
