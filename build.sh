#!/usr/bin/env bash
#
# build.sh -- turn your own copy of Shogun into the 64-bit port, in one command.
#
#   ./build.sh path/to/Shogun.apk
#
# Everything the port needs but cannot ship -- the engine binary and the asset
# pack -- comes out of the APK you supply. Nothing here downloads the game.
#
# Steps, all of which this script does for you:
#   1. check the toolchain and say exactly what is missing
#   2. fetch and build Unicorn for arm64-v8a (once; cached afterwards)
#   3. extract the engine and asset pack from your APK, patch the changelog,
#      rebuild the launcher icon
#   4. build the new weapons, missions and bosses (missile/build.py)
#   5. build the native runtime
#   6. package and sign the APK
set -euo pipefail

say()  { printf '\n\033[1m==> %s\033[0m\n' "$*"; }
ok()   { printf '    \033[32mok\033[0m  %s\n' "$*"; }
warn() { printf '    \033[33m!\033[0m   %s\n' "$*"; }
die()  { printf '\n\033[31merror:\033[0m %s\n\n' "$*" >&2; exit 1; }

ROOT="$(cd "$(dirname "$0")" && pwd)"
APK_IN="${1:-}"

if [ -z "$APK_IN" ]; then
  cat >&2 <<'USAGE'
usage: ./build.sh path/to/Shogun.apk

You need your own copy of Shogun: Rise Of The Renegade. See "Getting the game"
in the README -- this project does not distribute it and cannot get it for you.
USAGE
  exit 2
fi
[ -f "$APK_IN" ] || die "no such file: $APK_IN"

# ---------------------------------------------------------------- 1. toolchain
say "Checking the toolchain"
missing=0
need() {                     # need <what> <test-command> <how-to-fix>
  if eval "$2" >/dev/null 2>&1; then ok "$1"; else warn "$1 -- $3"; missing=1; fi
}
SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}}"
NDK="${ANDROID_NDK:-${ANDROID_NDK_HOME:-}}"
# Most people have an NDK inside the SDK and no ANDROID_NDK set. Look before
# asking them for one; if several are installed, take the newest.
if [ -z "$NDK" ] || [ ! -f "$NDK/build/cmake/android.toolchain.cmake" ]; then
  for cand in $(ls -d "$SDK"/ndk/*/ 2>/dev/null | sort -V -r); do
    if [ -f "${cand}build/cmake/android.toolchain.cmake" ]; then
      NDK="${cand%/}"
      break
    fi
  done
fi
BUILD_TOOLS_VERSION="${BUILD_TOOLS_VERSION:-36.0.0}"

# The SDK ships both cmake and ninja under cmake/<version>/bin. Use them if
# the user has not put their own on PATH, so neither has to be installed
# separately. Ninja matters twice over: without a named generator CMake
# picks NMake on Windows, which the NDK toolchain cannot use.
for cand in $(ls -d "$SDK"/cmake/*/bin 2>/dev/null | sort -V -r); do
  if [ -x "$cand/cmake" ] || [ -x "$cand/cmake.exe" ]; then
    # A PATH entry must be in the shell's own form. Under Git Bash a
    # Windows-style "C:/..." entry is silently ignored, so the tool stays
    # invisible and the check reports it missing.
    if command -v cygpath >/dev/null 2>&1; then cand="$(cygpath -u "$cand")"; fi
    PATH="$cand:$PATH"; export PATH; break
  fi
done
GEN=()
command -v ninja >/dev/null 2>&1 && GEN=(-G Ninja)
COMPILE_SDK="${COMPILE_SDK:-36}"

# Pick an interpreter that actually runs: on Windows "python3" is usually the
# Microsoft Store stub, which sits on PATH and does nothing useful.
PY=""
for c in python3 python py; do
  if command -v "$c" >/dev/null 2>&1 &&
     "$c" -c "import sys; sys.exit(0 if sys.version_info >= (3, 9) else 1)" >/dev/null 2>&1; then
    PY="$c"; break
  fi
done
need "python 3.9+"    '[ -n "$PY" ]' \
     "install Python 3.9+ so that it runs as python, python3 or py"
need "Pillow"         '[ -n "$PY" ] && "$PY" -c "import PIL"' \
     "python -m pip install Pillow  (needed for the launcher icon)"
need "cmake"          "command -v cmake" \
     "install CMake 3.22+, or add the SDK's cmake/*/bin to PATH"
need "JAVA_HOME"      "[ -n \"\${JAVA_HOME:-}\" ] && [ -x \"\$JAVA_HOME/bin/javac\" ]" \
     "export JAVA_HOME=... (Android Studio ships a JDK at its jbr/ directory)"
need "Android SDK"    "[ -d \"$SDK/build-tools/$BUILD_TOOLS_VERSION\" ]" \
     "export ANDROID_HOME=... with build-tools $BUILD_TOOLS_VERSION installed"
need "platform $COMPILE_SDK" "[ -f \"$SDK/platforms/android-$COMPILE_SDK/android.jar\" ]" \
     "install platform android-$COMPILE_SDK in the SDK manager"
need "Android NDK"    "[ -n \"$NDK\" ] && [ -f \"$NDK/build/cmake/android.toolchain.cmake\" ]" \
     "install the NDK in the SDK manager, or export ANDROID_NDK=..."
[ "$missing" -eq 0 ] || die "fix the items marked ! above, then run this again"


# ------------------------------------------------------------------ 2. unicorn
UNI="$ROOT/.build/unicorn"
UNI_SO="$ROOT/android/app/src/main/jniLibs/arm64-v8a/libunicorn.so"
UNI_VER="2.1.3"
if [ -f "$UNI_SO" ] && [ -d "$UNI/include/unicorn" ]; then
  say "Unicorn already built"; ok "$(basename "$UNI_SO")"
else
    # QEMU's configure (vendored inside Unicorn) probes for `strings` and
  # `pkg-config`. Neither ships with Git Bash, and its error for the second one
  # is fatal. The NDK already carries llvm-strings, and a pkg-config that
  # reports every package as absent is exactly right here -- this build wants
  # none of the optional dependencies it would find.
  SHIM="$ROOT/.build/shim"
  mkdir -p "$SHIM"
  LLVM_STRINGS="$(ls "$NDK"/toolchains/llvm/prebuilt/*/bin/llvm-strings* 2>/dev/null | head -1 || true)"
  if [ -n "$LLVM_STRINGS" ] && ! command -v strings >/dev/null 2>&1; then
    cat > "$SHIM/strings" <<SHIMEOF
#!/bin/sh
exec "$LLVM_STRINGS" "$@"
SHIMEOF
    chmod +x "$SHIM/strings"
  fi
  if ! command -v pkg-config >/dev/null 2>&1; then
    cat > "$SHIM/pkg-config" <<'SHIMEOF'
#!/bin/sh
# Reports every package as absent, which is what this build wants.
case "$1" in --version) echo 0.29.2; exit 0;; esac
exit 1
SHIMEOF
    chmod +x "$SHIM/pkg-config"
  fi
  PATH="$SHIM:$PATH"; export PATH

say "Building Unicorn $UNI_VER for arm64-v8a (once; a few minutes)"
  mkdir -p "$ROOT/.build"
  if [ ! -d "$UNI" ]; then
    command -v curl >/dev/null || die "curl is needed to fetch Unicorn"
    curl -sSL -o "$ROOT/.build/unicorn.tar.gz" \
      "https://github.com/unicorn-engine/unicorn/archive/refs/tags/$UNI_VER.tar.gz"
    tar -xzf "$ROOT/.build/unicorn.tar.gz" -C "$ROOT/.build"
    mv "$ROOT/.build/unicorn-$UNI_VER" "$UNI"
    ok "fetched"
  fi
  cmake -S "$UNI" -B "$UNI/build" "${GEN[@]}" \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
    -DCMAKE_BUILD_TYPE=Release -DUNICORN_ARCH=arm >/dev/null
  # Only the shared library. Building everything also makes the static one,
  # whose final step is a create_symlink that fails on Windows without
  # Developer Mode ("a required privilege is not held by the client") --
  # after libunicorn.so has already linked, so it looks like a build that
  # produced its output and failed anyway.
  cmake --build "$UNI/build" --target unicorn -j >/dev/null
  mkdir -p "$(dirname "$UNI_SO")"
  cp "$UNI/build/libunicorn.so" "$UNI_SO"
  ok "built $(basename "$UNI_SO")"
fi

# ------------------------------------------------------------------- 3. assets
say "Extracting the engine and asset pack from your APK"
"$PY" "$ROOT/tools/extract_assets.py" "$APK_IN"

# ------------------------------------------------------------------ 4. missile
# The guest code for the new weapons, missions and bosses, compiled for the
# game's ARM32 and linked into libshogun.so as missile_blob.h. Its weapon
# buttons are cut from the game's own HUD sheet, so they are not in this
# repository: make them from your copy first (missile/make_icon.py, then
# missile/make_upgrades.py).
say "Building the game patches"
for t in weapon_m weapon_e_up weapon_e_down weapon_p_up weapon_p_down weapon_sh_up weapon_sh_down; do
  [ -f "$ROOT/missile/$t.tga" ] || die "missile/$t.tga is missing: run missile/make_icon.py on the game's common/hud/weapons_buttons_thin.tga, then missile/make_upgrades.py"
done
"$PY" "$ROOT/missile/build.py" >/dev/null
ok "missile_blob.h"

# ------------------------------------------------------------------ 5. runtime
say "Building the native runtime"
CPP="$ROOT/android/app/src/main/cpp"
cmake -S "$CPP" -B "$CPP/build" "${GEN[@]}" \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
  -DCMAKE_BUILD_TYPE=Release \
  -DUNICORN_INC="$UNI/include" -DUNICORN_LIB="$UNI_SO" >/dev/null
cmake --build "$CPP/build" -j >/dev/null
ok "libshogun.so"

# ------------------------------------------------------------------ 6. package
say "Packaging"
LLVM_STRIP="$(ls "$NDK"/toolchains/llvm/prebuilt/*/bin/llvm-strip* 2>/dev/null | head -1)"
export LLVM_STRIP
"$ROOT/android/build-apk.sh" >/dev/null
APK_OUT="$ROOT/android/shogun-arm64.apk"
[ -f "$APK_OUT" ] || die "build-apk.sh did not produce an APK"

# The one check worth never skipping: that the APK contains the runtime just
# built. A silently stale APK looks exactly like a fix that did not work.
# build-apk.sh ships it stripped, so compare against a stripped copy.
"$LLVM_STRIP" --strip-unneeded -o "$CPP/build/libshogun.stripped.so" "$CPP/build/libshogun.so"
"$PY" - "$APK_OUT" "$CPP/build/libshogun.stripped.so" <<'PYEOF'
import sys, zipfile
a = zipfile.ZipFile(sys.argv[1]).read('lib/arm64-v8a/libshogun.so')
b = open(sys.argv[2], 'rb').read()
if a != b:
    sys.exit("the APK does not contain the runtime just built")
PYEOF
ok "APK carries the runtime just built"

mkdir -p "$ROOT/dist"
cp "$APK_OUT" "$ROOT/dist/Shogun-64-1.2.4-arm64.apk"

say "Done"
cat <<EOF
    $ROOT/dist/Shogun-64-1.2.4-arm64.apk

    adb install -r "$ROOT/dist/Shogun-64-1.2.4-arm64.apk"

    It installs as "Shogun 64" alongside the original, not over it.
EOF
