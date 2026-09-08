#!/usr/bin/env python3
"""
run_tests.py -- verification suite for the Shogun ARM32 guest runtime.

Covers milestones M0 (harness), M1 (loader) and M2 (shims) from the port
plan, and ends with the performance measurement that decides whether the
on-device runtime is worth building.

    python run_tests.py [path/to/libHAL.Android.so]
"""
import math, os, struct, sys, time
from unicorn import UC_HOOK_CODE
from guest import Guest, GuestError
import shims

def _find_lib():
    here = os.path.dirname(os.path.abspath(__file__))
    for p in ('lib/libHAL.Android.so', 'ext/lib/armeabi/libHAL.Android.so'):
        c = os.path.join(here, p)
        if os.path.exists(c):
            return c
    sys.exit("libHAL.Android.so not found; pass its path as an argument")

LIB = sys.argv[1] if len(sys.argv) > 1 else _find_lib()
PASS, FAIL = [], []


def check(name, cond, detail=""):
    (PASS if cond else FAIL).append(name)
    print("   [%s] %-46s %s" % ("PASS" if cond else "FAIL", name, detail))


def fresh():
    g = Guest(LIB)
    shims.install(g)
    return g


def head(t):
    print("\n" + "=" * 70 + "\n" + t + "\n" + "=" * 70)


# ------------------------------------------------------------------ M1
head("M1  loader")
g = fresh()
g.describe()
check("all relocations applied", sum(g.reloc_stats.values()) == 754,
      "%d relocations" % sum(g.reloc_stats.values()))
check("no unresolved symbols", not g.missing_syms)
check("every import trapped", len(g.imports) == 97, "%d imports" % len(g.imports))
check("symbol table loaded", len(g.sym) > 3000, "%d functions" % len(g.sym))

head("M1  known-value functions (ground truth from disassembly)")
check("Thumb call: areAllLevelsUnlocked -> 1",
      g.call('_Z20areAllLevelsUnlockedP6SHOGUN', 0) == 1)
check("ARM call:   DRM_CheckLicence -> 1",
      g.call('_Z16DRM_CheckLicencej', 0) == 1)

head("M1  real computation against external ground truth")
for a, b in [(1000000, 7), (0xFFFFFFFF, 3)]:
    check("__udivsi3(%d,%d)" % (a, b), g.call('__udivsi3', a, b) == a // b,
          "= %d" % (a // b))
s32 = lambda v: v - (1 << 32) if v >= (1 << 31) else v
check("__absvsi2(-2147483647)",
      s32(g.call('__absvsi2', (-2147483647) & 0xffffffff)) == 2147483647)
for v, lo, hi in [(15, 0, 10), ((-5) & 0xffffffff, 0, 10)]:
    exp = max(lo, min(hi, s32(v)))
    check("UE_Clamp(%d,%d,%d)" % (s32(v), lo, hi),
          s32(g.call('UE_Clamp', v, lo, hi)) == exp, "= %d" % exp)

buf = g.malloc(16)
g.write(buf, b"123456789" + b"\0" * 7)
crc = g.call('UE_GetCRC16', buf, 9) & 0xffff
check("UE_GetCRC16 matches CRC-16/CCITT-FALSE check vector", crc == 0x29B1,
      "0x%04X" % crc)

# ------------------------------------------------------------------ M2
head("M2  shim layer")
check("every import has a shim", not [n for n in g.imports if n not in g.shims],
      "97/97")
big = g.malloc(65536)
g.write(big, bytes((i * 7 + 3) & 0xff for i in range(65536)))
c1 = g.call('UE_GetCRC16', big, 65536)
c2 = g.call('UE_GetCRC16', big, 65536)
check("guest malloc + 64KB CRC is deterministic", c1 == c2, "0x%04X" % (c1 & 0xffff))

got = g.call_d('_Z9UE_ATanHPd', 0.5)
check("soft-float ABI round trip through host 'atan' shim",
      abs(got - math.atan(0.5)) < 1e-12, "%.15f" % got)
check("host math import was actually reached", g.calls.get('atan', 0) >= 1)

# ------------------------------------------------------------- benchmark
head("PERFORMANCE")
gb = fresh()
b = gb.malloc(65536)
gb.write(b, bytes((i * 7 + 3) & 0xff for i in range(65536)))
cnt = [0]
h = gb.uc.hook_add(UC_HOOK_CODE, lambda uc, a, s, u: cnt.__setitem__(0, cnt[0] + 1))
gb.call('UE_GetCRC16', b, 65536)
gb.uc.hook_del(h)
per = cnt[0]

g2 = fresh()
b2 = g2.malloc(65536)
g2.write(b2, bytes((i * 7 + 3) & 0xff for i in range(65536)))
it = 60
t0 = time.perf_counter()
for _ in range(it):
    g2.call('UE_GetCRC16', b2, 65536)
dt = time.perf_counter() - t0
mips = per * it / dt / 1e6

ORIG = 600.0
print("   instructions per call : {:,}".format(per))
print("   sustained throughput  : {:.0f} MIPS".format(mips))
print("   original hardware     : ~{:.0f} MIPS (600 MHz ARM11 class)".format(ORIG))
print("   headroom              : {:.1f}x".format(mips / ORIG))
check("emulation clears original hardware budget", mips > ORIG,
      "%.1fx" % (mips / ORIG))


# ------------------------------------------------------------------ M3
head("M3  JNI bridge + engine boot")
import jni
from guest import J
PAK = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'assets', 'data.mp3')
if not os.path.exists(PAK):
    print("   assets/data.mp3 not present -- extract res/raw/data.mp3 from the APK")
else:
    g3 = fresh()
    reads = {'n': 0}
    _fr = g3.shims['fread']
    def _counting_fread(gg):
        reads['n'] += gg.arg(1) * gg.arg(2); _fr(gg)
    g3.shims['fread'] = _counting_fread
    vm = jni.JVM(g3, verbose=False)
    check("fake JNIEnv vtable built", vm.env != 0 and vm.javavm != 0,
          "env=0x%08x vm=0x%08x" % (vm.env, vm.javavm))

    fh = open(PAK, 'rb'); g3.fd_files = {7: fh}
    size = os.path.getsize(PAK)
    def boot(step, *a):
        try:
            g3.call(step, *a); return True
        except GuestError as e:
            print("        %s" % e); return False

    check("JNI_OnLoad returns", boot('JNI_OnLoad', vm.javavm, 0))
    check("onInit returns", boot('Java_net_int13_HalActivity_onInit', vm.env,
          vm.activity, 480, 800, 0, vm.new_string('en'),
          vm.new_string('/files/'), vm.new_string('/sdcard/')))
    check("engine called back into Java (initAudio)", 'initAudio' in vm.java_calls,
          ",".join(vm.java_calls))
    check("onArchiveInit returns", boot('Java_net_int13_HalActivity_onArchiveInit',
          vm.env, vm.activity, vm.handle(('fd', 7)), J(0), J(size),
          vm.new_string('/files/')))
    check("engine opened the pack through fdopen", g3.calls.get('fdopen', 0) == 1)

    frames = 0
    for i in range(300):
        try:
            g3.call('Java_net_int13_HalActivity_onTick', vm.env, vm.activity)
            frames += 1
        except GuestError as e:
            print("        tick %d: %s" % (i + 1, e)); break
    check("game loop runs 300 frames", frames == 300, "%d/300" % frames)

    check("whole asset pack was read", reads['n'] >= size,
          "{:,} bytes for a {:,} byte pack".format(reads['n'], size))
    draws = sum(1 for n, _ in g3.gl_log if n in ('glDrawArrays', 'glDrawElements'))
    check("engine issues real draw calls", draws > 1000, "{:,} draws".format(draws))
    tex = sum(1 for n, _ in g3.gl_log if n in ('glTexImage2D', 'glTexSubImage2D'))
    check("engine uploads textures", tex > 100, "%d uploads" % tex)

    touch_ok = (boot('Java_net_int13_HalActivity_onTouchPressed', vm.env, vm.activity, 240, 400)
                and boot('Java_net_int13_HalActivity_onTouchMove', vm.env, vm.activity, 240, 500)
                and boot('Java_net_int13_HalActivity_onTouchReleased', vm.env, vm.activity, 240, 500))
    check("touch input accepted", touch_ok)
    check("no unimplemented JNI slots", not vm.unimpl, str(vm.unimpl or ''))
    check("no unimplemented imports", not g3.unimplemented, str(g3.unimplemented or ''))
    print("   NOTE: onAudioFrame is not covered -- it reaches the JNI buffer")
    print("         handover, then calls a null mixer callback because the")
    print("         engine has not issued startAudio in this harness (M5).")


# ------------------------------------------------------------------ M4
head("M4  rendering through a real GL driver")
try:
    import glfw
    from OpenGL.GL import (glReadPixels, glGetString, GL_RGB, GL_UNSIGNED_BYTE,
                           GL_VERSION, GL_PACK_ALIGNMENT, glPixelStorei)
    import gl as glbe
    have_gl = bool(glfw.init())
except Exception as _e:
    have_gl = False
    print("   skipped: %s" % _e)

if have_gl and os.path.exists(PAK):
    W, H = 480, 800
    glfw.window_hint(glfw.VISIBLE, glfw.FALSE)
    win = glfw.create_window(W, H, "verify", None, None)
    glfw.make_context_current(win)
    print("   driver: %s" % glGetString(GL_VERSION).decode())
    g4 = fresh()
    vm4 = jni.JVM(g4, verbose=False)
    be = glbe.GLBackend(g4, W, H)
    fh4 = open(PAK, 'rb'); g4.fd_files = {7: fh4}
    sz4 = os.path.getsize(PAK)
    g4.call('JNI_OnLoad', vm4.javavm, 0)
    g4.call('Java_net_int13_HalActivity_onInit', vm4.env, vm4.activity, W, H, 0,
            vm4.new_string('en'), vm4.new_string('/files/'), vm4.new_string('/sdcard/'))
    g4.call('Java_net_int13_HalActivity_onArchiveInit', vm4.env, vm4.activity,
            vm4.handle(('fd', 7)), J(0), J(sz4), vm4.new_string('/files/'))
    rendered = 0
    for i in range(300):
        try:
            g4.call('Java_net_int13_HalActivity_onTick', vm4.env, vm4.activity)
            rendered += 1
        except GuestError as e:
            print("        tick %d: %s" % (i + 1, e)); break
    check("300 frames through the real driver", rendered == 300, "%d/300" % rendered)
    check("draw calls reached the GPU", be.draws > 1000, "{:,}".format(be.draws))
    check("textures uploaded to the GPU", len(be.tex_map) >= 10,
          "%d textures" % len(be.tex_map))

    glPixelStorei(GL_PACK_ALIGNMENT, 1)
    px = bytes(glReadPixels(0, 0, W, H, GL_RGB, GL_UNSIGNED_BYTE))
    lit = sum(1 for i in range(0, len(px), 3) if px[i] or px[i+1] or px[i+2])
    total = W * H
    colours = len({px[i:i+3] for i in range(0, len(px), 3 * 97)})
    check("framebuffer is not blank", lit > total * 0.20,
          "%.0f%% of pixels lit" % (100.0 * lit / total))
    check("image has real colour variety", colours > 200, "%d distinct sampled colours" % colours)
    shot = glbe.save_png('frames/verify.png', W, H)
    print("   wrote %s" % shot)

    # ---- M4b: scaling / letterbox --------------------------------------
    be2 = glbe.GLBackend(fresh(), 480, 800, out=(1080, 2400))
    x, y, vw, vh = be2.viewport()
    check("letterbox preserves aspect ratio", abs((vw / float(vh)) - (480 / 800.0)) < 0.01,
          "%dx%d at (%d,%d), %.2fx" % (vw, vh, x, y, vw / 480.0))
    check("letterbox fills one axis exactly", vw == 1080 and y == 300,
          "width filled, %dpx bars top and bottom" % y)

    # ---- M5: audio ------------------------------------------------------
    head("M5  audio")
    print("   engine asked for %d-sample buffers at %d Hz"
          % (vm4.audio['samples'], vm4.audio['hz']))
    abuf, asz = vm4.new_audio_buffer()
    pcm = bytearray()
    audio_ok = True
    for _ in range(120):
        try:
            g4.call('Java_net_int13_HalActivity_onAudioFrame', vm4.env, vm4.activity,
                    abuf, asz)
            pcm += vm4.obj[abuf][1]
            g4.call('Java_net_int13_HalActivity_onTick', vm4.env, vm4.activity)
        except GuestError as e:
            print("        %s" % e); audio_ok = False; break
    check("onAudioFrame runs 120 buffers", audio_ok)
    if pcm:
        import statistics
        sm = struct.unpack('<%dh' % (len(pcm) // 2), bytes(pcm[:len(pcm) // 2 * 2]))
        rms = statistics.pstdev(sm)
        check("mixer produces real audio, not silence", rms > 200,
              "peak %d, RMS %.0f, %.1f s" % (max(abs(v) for v in sm), rms,
                                             len(sm) / float(vm4.audio['hz'])))

    glfw.terminate()
elif have_gl:
    print("   skipped: assets/data.mp3 not present")

head("SUMMARY")
print("   %d passed, %d failed" % (len(PASS), len(FAIL)))
if FAIL:
    print("   failures: %s" % FAIL)
sys.exit(1 if FAIL else 0)
