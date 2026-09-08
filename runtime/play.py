"""play.py -- boot Shogun, dismiss the changelog, and drive the menu."""
import os, glfw
from OpenGL.GL import *
from guest import Guest, GuestError, J
import shims, jni, gl
W,H=480,800
glfw.init(); glfw.window_hint(glfw.VISIBLE, glfw.FALSE)
win=glfw.create_window(W,H,"shogun",None,None); glfw.make_context_current(win)
g=Guest('ext/lib/armeabi/libHAL.Android.so'); shims.install(g)
vm=jni.JVM(g, verbose=False); be=gl.GLBackend(g,W,H)
fh=open('assets/data.mp3','rb'); g.fd_files={7:fh}; size=os.path.getsize('assets/data.mp3')
g.call('JNI_OnLoad', vm.javavm, 0)
g.call('Java_net_int13_HalActivity_onInit', vm.env, vm.activity, W,H,0,
       vm.new_string('en'), vm.new_string('/files/'), vm.new_string('/sdcard/'))
g.call('Java_net_int13_HalActivity_onArchiveInit', vm.env, vm.activity,
       vm.handle(('fd',7)), J(0), J(size), vm.new_string('/files/'))
def ticks(n):
    for _ in range(n): g.call('Java_net_int13_HalActivity_onTick', vm.env, vm.activity)
def tap(x,y,hold=4):
    g.call('Java_net_int13_HalActivity_onTouchPressed', vm.env, vm.activity, x, y)
    ticks(hold)
    g.call('Java_net_int13_HalActivity_onTouchReleased', vm.env, vm.activity, x, y)
    ticks(hold)
os.makedirs('frames',exist_ok=True)
ticks(300)
print("tap OK on the changelog (425,740)"); tap(425,740); ticks(90)
gl.save_png('frames/menu_a.png',W,H); print("  -> frames/menu_a.png  draws",be.draws)
print("tap centre (touch the screen)");    tap(240,400); ticks(120)
gl.save_png('frames/menu_b.png',W,H); print("  -> frames/menu_b.png  draws",be.draws)
print("tap centre again");                 tap(240,400); ticks(120)
gl.save_png('frames/menu_c.png',W,H); print("  -> frames/menu_c.png  draws",be.draws)
print("tap PLAY (215,770)");               tap(215,770); ticks(150)
gl.save_png('frames/play_1.png',W,H); print("  -> frames/play_1.png  draws",be.draws)
print("tap first level slot (240,300)");   tap(240,300); ticks(180)
gl.save_png('frames/play_2.png',W,H); print("  -> frames/play_2.png  draws",be.draws)
print("tap centre to confirm");            tap(240,600); ticks(240)
gl.save_png('frames/play_3.png',W,H); print("  -> frames/play_3.png  draws",be.draws)
glfw.terminate()
