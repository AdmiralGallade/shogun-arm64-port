#!/usr/bin/env bash
# build-apk.sh -- package the Shogun arm64 runtime.
# No Gradle: aapt2 + javac + d8 + apksigner / bundletool directly.
#
#   bash android/build-apk.sh          # android/shogun-arm64.apk, to install with adb
#   bash android/build-apk.sh --aab    # android/shogun-arm64.aab, to upload to Google Play
#
# Signing. With no key configured, a local debug key (android/shogun-debug.keystore)
# signs the APK; fine for your own phone, never for Play. To sign with your upload
# key (see tools/make_upload_key.sh), set, e.g. in a file you source and never commit:
#   SHOGUN_KEYSTORE=/path/outside/the/repo/shogun-upload.jks
#   SHOGUN_KEY_ALIAS=upload
#   SHOGUN_KEYSTORE_PASS=...          (read from the environment, never from the command line)
#   SHOGUN_KEY_PASS=...               (optional; defaults to SHOGUN_KEYSTORE_PASS)
# --aab refuses to finish without one: Play remembers the first upload key forever.
set -e

MODE=apk
[ "$1" = "--aab" ] && MODE=aab

# Tool locations: override any of these from the environment.
SDK="${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}"
BT="${BUILD_TOOLS:-$SDK/build-tools/36.0.0}"
PLATFORM="${ANDROID_JAR:-$SDK/platforms/android-36/android.jar}"
JAVA_HOME="${JAVA_HOME:-/c/Program Files/Android/Android Studio/jbr}"
export JAVA_HOME
JAVA="$JAVA_HOME/bin/java"
JAVAC="$JAVA_HOME/bin/javac"
KEYTOOL="$JAVA_HOME/bin/keytool"
JARSIGNER="$JAVA_HOME/bin/jarsigner"
BUNDLETOOL_VERSION=1.18.3
BUNDLETOOL="${BUNDLETOOL:-$HOME/.cache/bundletool/bundletool-all-$BUNDLETOOL_VERSION.jar}"

ROOT="$(cd "$(dirname "$0")" && pwd)"
MAIN="$ROOT/app/src/main"
OUT="$ROOT/build"

# ---- keys ------------------------------------------------------------------
if [ -n "$SHOGUN_KEYSTORE" ]; then
  [ -f "$SHOGUN_KEYSTORE" ] || { echo "SHOGUN_KEYSTORE: no such file"; exit 1; }
  [ -n "$SHOGUN_KEY_ALIAS" ] || { echo "set SHOGUN_KEY_ALIAS"; exit 1; }
  [ -n "$SHOGUN_KEYSTORE_PASS" ] || { echo "set SHOGUN_KEYSTORE_PASS"; exit 1; }
  export SHOGUN_KEYSTORE_PASS
  export SHOGUN_KEY_PASS="${SHOGUN_KEY_PASS:-$SHOGUN_KEYSTORE_PASS}"
  KS="$SHOGUN_KEYSTORE"; KS_ALIAS="$SHOGUN_KEY_ALIAS"; KEY_KIND="upload key"
else
  [ "$MODE" = aab ] && { echo "--aab needs your upload key: set SHOGUN_KEYSTORE etc. (see the top of this file)"; exit 1; }
  KS="$ROOT/shogun-debug.keystore"; KS_ALIAS=shogun; KEY_KIND="DEBUG key (not for Play)"
  export SHOGUN_KEYSTORE_PASS=shogun SHOGUN_KEY_PASS=shogun
  if [ ! -f "$KS" ]; then
    "$KEYTOOL" -genkeypair -v -keystore "$KS" -storepass shogun -keypass shogun \
      -alias shogun -keyalg RSA -keysize 2048 -validity 10950 \
      -dname "CN=Shogun Personal Rebuild, O=Personal Archive" >/dev/null 2>&1
  fi
fi
if [ "$MODE" = aab ] && [ ! -f "$BUNDLETOOL" ]; then
  echo "bundletool $BUNDLETOOL_VERSION not found; fetching it from Google's GitHub release"
  mkdir -p "$(dirname "$BUNDLETOOL")"
  curl -fsSLo "$BUNDLETOOL" \
    "https://github.com/google/bundletool/releases/download/$BUNDLETOOL_VERSION/bundletool-all-$BUNDLETOOL_VERSION.jar"
fi

rm -rf "$OUT"; mkdir -p "$OUT/classes" "$OUT/dex" "$OUT/apk/lib/arm64-v8a"

echo "== 1. resources =="
"$BT/aapt2.exe" compile --dir "$MAIN/res" -o "$OUT/res.zip"

echo "== 2. link =="
# data.mp3 is LZMA inside already and libHAL is read whole, so storing both
# uncompressed costs nothing and keeps AssetManager.openFd() usable on them.
PROTO=""
[ "$MODE" = aab ] && PROTO="--proto-format"
"$BT/aapt2.exe" link $PROTO \
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
"$BT/d8.bat" --lib "$PLATFORM" --min-api 26 --release --output "$OUT/dex" @"$OUT/classes.txt"

echo "== 5. native libraries =="
# Shipped stripped: no debug info, and no home-directory paths (tools/scrub_paths.py
# fails the build if one is left).
STRIP="${LLVM_STRIP:-$(ls "$SDK"/ndk/*/toolchains/llvm/prebuilt/*/bin/llvm-strip* 2>/dev/null | tail -1)}"
for so in "$MAIN/jniLibs/arm64-v8a/libunicorn.so" "$MAIN/cpp/build/libshogun.so"; do
  "$STRIP" --strip-unneeded -o "$OUT/apk/lib/arm64-v8a/$(basename "$so")" "$so"
done
python "$ROOT/../tools/scrub_paths.py" "$OUT"/apk/lib/arm64-v8a/*.so

if [ "$MODE" = apk ]; then
  echo "== 6. assemble =="
  cp "$OUT/dex/classes.dex" "$OUT/apk/"
  cp "$OUT/base.apk"        "$OUT/unsigned.apk"
  python - "$OUT/unsigned.apk" "$OUT/apk" <<'PYEOF'
import os, shutil, sys, zipfile
apk, root = sys.argv[1], sys.argv[2]
# aapt2 stores .mpg uncompressed (a media type, by its defaults), but the worlds'
# level-select clips are I-frame-only MPEG-1 and deflate to about half -- ~20 MB
# off the APK, losslessly. Nothing opens them by file descriptor: the app
# stream-copies each world's folder to the files directory.
tmp = apk + ".tmp"
with zipfile.ZipFile(apk) as src, zipfile.ZipFile(tmp, 'w') as dst:
    for info in src.infolist():
        data = src.read(info)
        if info.filename.endswith('.mpg'):
            info.compress_type = zipfile.ZIP_DEFLATED
            dst.writestr(info, data, compresslevel=9)
        else:
            dst.writestr(info, data)
shutil.move(tmp, apk)
with zipfile.ZipFile(apk, 'a', zipfile.ZIP_DEFLATED) as z:
    for dirpath, _, names in os.walk(root):
        for n in names:
            full = os.path.join(dirpath, n)
            arc = os.path.relpath(full, root).replace(os.sep, '/')
            z.write(full, arc)
            print("   +", arc, os.path.getsize(full))
PYEOF

  echo "== 7. align =="
  "$BT/zipalign.exe" -p -f 4 "$OUT/unsigned.apk" "$OUT/aligned.apk"

  echo "== 8. sign ($KEY_KIND) =="
  "$BT/apksigner.bat" sign --ks "$KS" --ks-key-alias "$KS_ALIAS" \
    --ks-pass env:SHOGUN_KEYSTORE_PASS --key-pass env:SHOGUN_KEY_PASS \
    --min-sdk-version 26 --v1-signing-enabled true --v2-signing-enabled true \
    --out "$ROOT/shogun-arm64.apk" "$OUT/aligned.apk"

  "$BT/apksigner.bat" verify --print-certs "$ROOT/shogun-arm64.apk" | head -4
  ls -la "$ROOT/shogun-arm64.apk"
  echo "OK: $ROOT/shogun-arm64.apk"
  exit 0
fi

echo "== 6. base module =="
# bundletool's module layout: manifest/, dex/, res/ + resources.pb, assets/, lib/.
python - "$OUT/base.apk" "$OUT/dex/classes.dex" "$OUT/apk/lib" "$OUT/base.zip" <<'PYEOF'
import os, sys, zipfile
proto, dex, libs, out = sys.argv[1:5]
with zipfile.ZipFile(proto) as src, zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for info in src.infolist():
        n = info.filename
        if n == "AndroidManifest.xml":
            z.writestr("manifest/AndroidManifest.xml", src.read(n))
        else:                     # resources.pb, res/..., assets/...
            z.writestr(n, src.read(n))
    z.write(dex, "dex/classes.dex")
    for dirpath, _, names in os.walk(libs):
        for f in names:
            full = os.path.join(dirpath, f)
            z.write(full, "lib/" + os.path.relpath(full, libs).replace(os.sep, "/"))
print("   base module:", len(zipfile.ZipFile(out).namelist()), "entries")
PYEOF

echo "== 7. bundle =="
# Keep the pack and the engine library stored (as the APK does), and keep native
# libraries compressed and extracted at install (as the APK does): the
# configuration every device test so far has run.
cat > "$OUT/BundleConfig.json" <<'JSON'
{
  "compression": { "uncompressedGlob": ["assets/**.mp3", "assets/**.so"] },
  "optimizations": { "uncompressNativeLibraries": { "enabled": false } }
}
JSON
rm -f "$ROOT/shogun-arm64.aab"
"$JAVA" -jar "$BUNDLETOOL" build-bundle --modules="$(cygpath -w "$OUT/base.zip")" \
  --config="$(cygpath -w "$OUT/BundleConfig.json")" --output="$(cygpath -w "$OUT/unsigned.aab")"

echo "== 8. sign ($KEY_KIND) =="
"$JARSIGNER" -keystore "$(cygpath -w "$KS")" -storepass:env SHOGUN_KEYSTORE_PASS \
  -keypass:env SHOGUN_KEY_PASS -sigalg SHA256withRSA -digestalg SHA-256 \
  -signedjar "$(cygpath -w "$ROOT/shogun-arm64.aab")" "$(cygpath -w "$OUT/unsigned.aab")" "$KS_ALIAS" >/dev/null
"$JARSIGNER" -verify "$(cygpath -w "$ROOT/shogun-arm64.aab")" | tail -1
ls -la "$ROOT/shogun-arm64.aab"
echo "OK: $ROOT/shogun-arm64.aab"
