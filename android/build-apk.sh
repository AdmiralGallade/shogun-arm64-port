#!/usr/bin/env bash
# build-apk.sh -- package the Shogun arm64 runtime into an installable APK.
# No Gradle: aapt2 + javac + d8 + apksigner directly.
set -e

# Point these at your own SDK; ANDROID_HOME and JAVA_HOME are used when set.
SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}}"
BUILD_TOOLS_VERSION="${BUILD_TOOLS_VERSION:-36.0.0}"
COMPILE_SDK="${COMPILE_SDK:-36}"
BT="$SDK/build-tools/$BUILD_TOOLS_VERSION"
PLATFORM="$SDK/platforms/android-$COMPILE_SDK/android.jar"
: "${JAVA_HOME:?set JAVA_HOME to a JDK 17+ -- Android Studio ships one at jbr/}"
export JAVA_HOME

for p in "$BT" "$PLATFORM" "$JAVA_HOME"; do
  [ -e "$p" ] || { echo "not found: $p" >&2; exit 1; }
done
JAVAC="$JAVA_HOME/bin/javac"
KEYTOOL="$JAVA_HOME/bin/keytool"

ROOT="$(cd "$(dirname "$0")" && pwd)"
MAIN="$ROOT/app/src/main"
OUT="$ROOT/build"
rm -rf "$OUT"; mkdir -p "$OUT/classes" "$OUT/dex" "$OUT/apk/lib/arm64-v8a"

echo "== 1. resources =="
"$BT/aapt2.exe" compile --dir "$MAIN/res" -o "$OUT/res.zip"

echo "== 2. link (assets stored uncompressed so openFd() works) =="
# The engine gets its pack as a file descriptor + offset; AssetManager.openFd()
# fails outright on a compressed asset, so .mp3 and .so must be stored.
"$BT/aapt2.exe" link \
  -o "$OUT/base.apk" \
  -I "$PLATFORM" \
  --manifest "$MAIN/AndroidManifest.xml" \
  -A "$MAIN/assets" \
  -0 mp3 -0 so \
  --min-sdk-version 26 --target-sdk-version 36 \
  "$OUT/res.zip"

echo "== 3. compile java =="
find "$MAIN/java" -name '*.java' -exec cygpath -w {} \; > "$OUT/sources.txt"
"$JAVAC" -source 17 -target 17 -nowarn \
  -classpath "$PLATFORM" -d "$OUT/classes" @"$OUT/sources.txt"

echo "== 4. dex =="
find "$OUT/classes" -name '*.class' -exec cygpath -w {} \; > "$OUT/classes.txt"
"$BT/d8.bat" --lib "$PLATFORM" --min-api 26 --output "$OUT/dex" @"$OUT/classes.txt"

echo "== 5. assemble =="
cp "$MAIN/jniLibs/arm64-v8a/libunicorn.so" "$OUT/apk/lib/arm64-v8a/"
cp "$MAIN/cpp/build/libshogun.so"          "$OUT/apk/lib/arm64-v8a/"
cp "$OUT/dex/classes.dex"                  "$OUT/apk/"
cp "$OUT/base.apk"                         "$OUT/unsigned.apk"
python - "$OUT/unsigned.apk" "$OUT/apk" <<'PYEOF'
import os, sys, zipfile
apk, root = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(apk, 'a', zipfile.ZIP_DEFLATED) as z:
    for dirpath, _, names in os.walk(root):
        for n in names:
            full = os.path.join(dirpath, n)
            arc = os.path.relpath(full, root).replace(os.sep, '/')
            z.write(full, arc)
            print("   +", arc, os.path.getsize(full))
PYEOF

echo "== 6. align =="
"$BT/zipalign.exe" -p -f 4 "$OUT/unsigned.apk" "$OUT/aligned.apk"

echo "== 7. sign =="
KS="$ROOT/shogun-debug.keystore"
if [ ! -f "$KS" ]; then
  "$KEYTOOL" -genkeypair -v -keystore "$KS" -storepass shogun -keypass shogun \
    -alias shogun -keyalg RSA -keysize 2048 -validity 10950 \
    -dname "CN=Shogun Personal Rebuild, O=Personal Archive" >/dev/null 2>&1
fi
"$BT/apksigner.bat" sign --ks "$KS" --ks-pass pass:shogun --key-pass pass:shogun \
  --min-sdk-version 26 --v1-signing-enabled true --v2-signing-enabled true \
  --out "$ROOT/shogun-arm64.apk" "$OUT/aligned.apk"

"$BT/apksigner.bat" verify --print-certs "$ROOT/shogun-arm64.apk" | head -4
ls -la "$ROOT/shogun-arm64.apk"
echo "OK: $ROOT/shogun-arm64.apk"
