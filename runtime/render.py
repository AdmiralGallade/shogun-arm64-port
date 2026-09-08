"""
render.py -- Shogun, on screen again, scaled, with audio  (M4 + M5)

    python render.py                      600 frames offscreen, capture PNGs
    python render.py 1800 --window        watch it in a 720x1200 window
    python render.py 600 --size 1080x2400 letterbox to a Pixel-sized panel
    python render.py 600 --wav out.wav    also capture the audio

The game is authored for a 480x800 portrait panel.  Rather than lie to it
about the resolution, it renders into an offscreen buffer at its native size
which is then scaled up with the aspect ratio preserved.
"""
import os, struct, sys, time
import glfw
from OpenGL.GL import *
from guest import Guest, GuestError, J
import shims, jni, gl

NATIVE_W, NATIVE_H = 480, 800
argv = sys.argv[1:]
frames_wanted = int(argv[0]) if argv and argv[0].isdigit() else 600
SHOW = '--window' in argv
WAV = argv[argv.index('--wav') + 1] if '--wav' in argv else None
if '--size' in argv:
    ow, oh = (int(v) for v in argv[argv.index('--size') + 1].lower().split('x'))
else:
    ow, oh = (720, 1200) if SHOW else (NATIVE_W, NATIVE_H)
SHOTS = {1, 30, 90, 180, 300, 450, 600}
os.makedirs('frames', exist_ok=True)

if not glfw.init():
    sys.exit("glfw init failed")
glfw.window_hint(glfw.VISIBLE, glfw.TRUE if SHOW else glfw.FALSE)
win = glfw.create_window(ow, oh, "Shogun (2012) - ARM32 HLE runtime", None, None)
glfw.make_context_current(win)
glfw.swap_interval(0)
print("GL      : %s" % glGetString(GL_VERSION).decode())
print("output  : %dx%d  (game renders at %dx%d, letterboxed)"
      % (ow, oh, NATIVE_W, NATIVE_H))

g = Guest('ext/lib/armeabi/libHAL.Android.so')
shims.install(g)
vm = jni.JVM(g, verbose=False)
scaled = (ow, oh) != (NATIVE_W, NATIVE_H)
be = gl.GLBackend(g, NATIVE_W, NATIVE_H, out=(ow, oh) if scaled else None)

pak = 'assets/data.mp3'
fh = open(pak, 'rb')
g.fd_files = {7: fh}
g.call('JNI_OnLoad', vm.javavm, 0)
g.call('Java_net_int13_HalActivity_onInit', vm.env, vm.activity, NATIVE_W, NATIVE_H,
       0, vm.new_string('en'), vm.new_string('/files/'), vm.new_string('/sdcard/'))
g.call('Java_net_int13_HalActivity_onArchiveInit', vm.env, vm.activity,
       vm.handle(('fd', 7)), J(0), J(os.path.getsize(pak)), vm.new_string('/files/'))
if scaled:
    x, y, vw, vh = be.viewport()
    print("viewport: %dx%d at (%d,%d)  scale %.2fx" % (vw, vh, x, y, vw / float(NATIVE_W)))
print("archive loaded, entering game loop\n")

abuf, asamples = vm.new_audio_buffer()
pcm = bytearray()
audio_every = max(1, int(round(60.0 / (vm.audio['hz'] / float(asamples)))))
saved, done, t0 = [], 0, time.perf_counter()

for i in range(1, frames_wanted + 1):
    be.begin_frame()
    try:
        g.call('Java_net_int13_HalActivity_onTick', vm.env, vm.activity)
    except GuestError as e:
        print("  tick %d STOPPED: %s" % (i, e)); break
    done = i
    be.end_frame(present=glfw.get_framebuffer_size(win) if SHOW else None)
    if WAV:
        for _ in range(3):                       # keep the mixer fed
            g.call('Java_net_int13_HalActivity_onAudioFrame', vm.env, vm.activity,
                   abuf, asamples)
            pcm += vm.obj[abuf][1]
    if i in SHOTS:
        p = gl.save_png('frames/frame_%04d.png' % i, ow, oh, backend=be)
        saved.append(p)
        print("  captured %s  (draws %d)" % (p, be.draws))
    if SHOW:
        glfw.swap_buffers(win); glfw.poll_events()
        if glfw.window_should_close(win):
            break

dt = time.perf_counter() - t0
print("\nframes      : %d" % done)
print("draw calls  : %d" % be.draws)
print("textures    : %d" % len(be.tex_map))
print("speed       : %.1f fps in this Python harness" % (done / dt))
if WAV and pcm:
    hz = vm.audio['hz']
    hdr = (b'RIFF' + struct.pack('<I', 36 + len(pcm)) + b'WAVEfmt '
           + struct.pack('<IHHIIHH', 16, 1, 1, hz, hz * 2, 2, 16)
           + b'data' + struct.pack('<I', len(pcm)))
    open(WAV, 'wb').write(hdr + bytes(pcm))
    print("audio       : %s  (%.1f s, %d Hz 16-bit mono)"
          % (WAV, len(pcm) / 2.0 / hz, hz))
print("captures    : %s" % ", ".join(saved))
glfw.terminate()
