"""
boot.py -- drive the real JNI entry points with the real asset pack (M3).

Brings the engine up the way HalActivity does on a device: JNI_OnLoad, then
onInit, then onArchiveInit with a file descriptor onto res/raw/data.mp3, then
runs the game loop.
"""
import collections, os, sys
from guest import Guest, GuestError, J
import shims, jni

LIB = 'ext/lib/armeabi/libHAL.Android.so'
PAK = 'assets/data.mp3'

g = Guest(LIB)
shims.install(g, log=True)
vm = jni.JVM(g, verbose=True)
print("JNIEnv* = 0x%08x   JavaVM* = 0x%08x   %d JNIEnv trap slots"
      % (vm.env, vm.javavm, jni.NSLOTS))

fh = open(PAK, 'rb')
FD = 7
g.fd_files = {FD: fh}
size = os.path.getsize(PAK)
ok = {}


def step(title, fn):
    print("\n=== %s ===" % title)
    try:
        fn()
        print("    -> returned cleanly")
        ok[title] = True
    except GuestError as e:
        print("    -> STOPPED: %s" % e)
        ok[title] = False
    if vm.java_calls:
        print("    java callbacks:", vm.java_calls)
        vm.java_calls.clear()


step("JNI_OnLoad", lambda: g.call('JNI_OnLoad', vm.javavm, 0))

step("onInit(480,800,0,'en',filesdir,sdcard)",
     lambda: g.call('Java_net_int13_HalActivity_onInit', vm.env, vm.activity,
                    480, 800, 0, vm.new_string('en'),
                    vm.new_string('/data/data/net.int13.shogun/files/'),
                    vm.new_string('/sdcard/')))

step("onArchiveInit(fd=%d, offset=0, length=%d)" % (FD, size),
     lambda: g.call('Java_net_int13_HalActivity_onArchiveInit', vm.env,
                    vm.activity, vm.handle(('fd', FD)), J(0), J(size),
                    vm.new_string('/data/data/net.int13.shogun/files/')))

# ---------------------------------------------------------------- game loop
print("\n=== game loop: onTick x 8 ===")
g.log_shims = False
vm.verbose = False
frames = 0
for i in range(8):
    before_gl, before_rd = len(g.gl_log), g.calls.get('fread', 0)
    try:
        g.call('Java_net_int13_HalActivity_onTick', vm.env, vm.activity)
        frames += 1
        print("    tick %d ok    GL calls %-5d fread %d"
              % (i + 1, len(g.gl_log) - before_gl, g.calls.get('fread', 0) - before_rd))
    except GuestError as e:
        print("    tick %d STOPPED: %s" % (i + 1, e))
        break

print("\n--- GL calls issued (top 12) ---")
for n, c in collections.Counter(n for n, _ in g.gl_log).most_common(12):
    print("      %-26s %d" % (n, c))
if not g.gl_log:
    print("      none")

print("\n--- JNI slots used ---")
seen = []
for t in vm.trace:
    k = t.split(':')[0]
    if k not in seen:
        seen.append(k)
print("   ", ", ".join(seen) or "none")
print("--- unimplemented JNI ---\n   ", vm.unimpl or "none")
print("--- host imports used ---\n   ",
      {k: v for k, v in sorted(g.calls.items()) if not k.startswith('gl')})
print("--- unimplemented imports ---\n   ", g.unimplemented or "none")
print("--- guest heap ---\n    peak %d KB in %d blocks"
      % (g.peak // 1024, len(g.blocks)))
print("\nframes completed: %d/8   entry points clean: %d/%d"
      % (frames, sum(1 for v in ok.values() if v), len(ok)))
