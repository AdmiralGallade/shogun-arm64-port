#!/usr/bin/env python3
"""
device-test.py -- everything we can verify on the Pixel with adb alone.

    python device-test.py            # read-only checks
    python device-test.py --install  # also install the patched APK and report

No NDK needed for these. The trap benchmark in bench/ is separate and does
need one, because unicorn has to be built for arm64 Android first.
"""
import os, subprocess, sys, glob

def find_adb():
    try:
        import adbutils
        c = os.path.join(os.path.dirname(adbutils.__file__), 'binaries', 'adb.exe')
        if os.path.exists(c):
            return c
    except Exception:
        pass
    for p in (os.environ.get('LOCALAPPDATA', ''), os.environ.get('USERPROFILE', '')):
        for c in glob.glob(os.path.join(p, '**', 'platform-tools', 'adb*'), recursive=True):
            return c
    return 'adb'

ADB = find_adb()
def sh(*a, timeout=60):
    try:
        r = subprocess.run([ADB] + list(a), capture_output=True, text=True, timeout=timeout)
        return (r.stdout + r.stderr).strip()
    except Exception as e:
        return "ERROR: %s" % e

def prop(k):
    return sh('shell', 'getprop', k)

print("adb:", ADB)
devs = [l for l in sh('devices').splitlines()[1:] if l.strip()]
if not devs:
    print("\nNO DEVICE ATTACHED.\n")
    print("On the phone:  Settings > About phone > tap 'Build number' 7 times,")
    print("               then Settings > System > Developer options > USB debugging ON.")
    print("Plug in with a DATA cable, set USB mode to 'File transfer', and accept")
    print("the 'Allow USB debugging?' prompt when it appears.")
    print("\nWireless instead (Android 11+, often easier):")
    print("  Developer options > Wireless debugging > Pair device with pairing code")
    print("  %s pair <ip>:<pair-port>" % os.path.basename(ADB))
    print("  %s connect <ip>:<port>" % os.path.basename(ADB))
    sys.exit(1)
print("device:", devs[0])

print("\n=== identity ===")
for label, key in [("model", "ro.product.model"), ("device", "ro.product.device"),
                   ("SoC", "ro.soc.model"), ("manufacturer", "ro.soc.manufacturer"),
                   ("Android", "ro.build.version.release"),
                   ("SDK", "ro.build.version.sdk"),
                   ("build", "ro.build.display.id")]:
    print("   %-14s %s" % (label, prop(key)))

print("\n=== the premise: is there ANY 32-bit ARM support? ===")
abilist = prop('ro.product.cpu.abilist')
ab32 = prop('ro.product.cpu.abilist32')
ab64 = prop('ro.product.cpu.abilist64')
print("   abilist    : %s" % abilist)
print("   abilist32  : %r" % ab32)
print("   abilist64  : %s" % ab64)
has32 = bool(ab32.strip()) or 'armeabi' in abilist
print("   -> 32-bit ARM: %s" % ("PRESENT (the whole premise is wrong!)" if has32
                                else "ABSENT -- confirms the teardown"))
print("   CPU:", (sh('shell', 'cat', '/proc/cpuinfo') or '').splitlines()[:1])

print("\n=== graphics: the one residual risk (ANGLE vs native GLES1) ===")
sf = sh('shell', 'dumpsys', 'SurfaceFlinger')
for line in sf.splitlines():
    if any(k in line for k in ('GLES:', 'OpenGL ES', 'EGL implementation', 'Vendor')):
        print("   " + line.strip()[:150])
print("   ANGLE default:", prop('ro.gfx.angle.supported'), "|",
      prop('persist.graphics.egl') or '(egl driver unset)')

print("\n=== does the device have the GLES1 library the engine needs? ===")
for lib in ('/system/lib64/libGLESv1_CM.so', '/vendor/lib64/libGLESv1_CM.so'):
    print("   %-38s %s" % (lib, sh('shell', 'ls', '-l', lib)))

if '--install' in sys.argv:
    apk = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'build',
                       'Shogun-1.2.4-patched-armeabi-v7a-signed.apk')
    print("\n=== installing the patched APK (expected to FAIL: 32-bit) ===")
    print("   ", apk)
    out = sh('install', '-r', '--bypass-low-target-sdk-block', apk, timeout=300)
    print("   " + out.replace("\n", "\n   "))
    if 'Success' in out:
        print("   -> installed. Launching:")
        print("   " + sh('shell', 'monkey', '-p', 'net.int13.shogun', '-c',
                         'android.intent.category.LAUNCHER', '1'))
        print("   (if it crashes, that is the 32-bit library failing to load)")
