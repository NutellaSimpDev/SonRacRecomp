#!/bin/bash
# Regenerate the recompiled code from sonicracingtransformed_clean.toml (adding
# tail-call targets until the analysis is clean), patch the dispatch macro for
# the 0.9.0 runtime, rebuild the native libraries and the APK, and install it.
#
# usage: android_native/regen_and_build.sh [adb serial]
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
REXGLUE="/c/Users/EduPedu/Tools/rexglue/win-amd64/bin/rexglue.exe"
SERIAL="${1:-9125332100CB}"

prev=""
for i in $(seq 1 15); do
  [ -n "$prev" ] && python add_tail_targets.py "$prev"
  log="out/codegen_regen_$i.log"
  "$REXGLUE" codegen sonicracingtransformed_clean.toml > "$log" 2>&1
  rc=$?
  echo "codegen pass $i exit=$rc $(grep -oE 'Total: [0-9]+ errors' "$log")"
  [ $rc = 0 ] && break
  prev="$log"
done
[ $rc = 0 ] || { echo "!! codegen did not converge"; exit 1; }

python patch_init_h.py || exit 1
bash android_native/build_android.sh > out/build_android.log 2>&1 || {
  echo "!! native build failed"; grep -E "error:|FAILED" out/build_android.log | head; exit 1; }

cd android
STRIPB="$(cygpath -u "$LOCALAPPDATA")/Android/Sdk/ndk/29.0.14206865/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-strip.exe"
for f in app/src/main/jniLibs/arm64-v8a/*.so; do "$STRIPB" --strip-debug "$f"; done
G=$(ls -d ~/.gradle/wrapper/dists/gradle-9.6.0-bin/*/gradle-9.6.0/bin/gradle.bat | head -1)
export JAVA_HOME="C:/Users/EduPedu/.gradle/jdks/eclipse_adoptium-21-amd64-windows.2"
cmd //c "$(cygpath -w "$G")" assembleDebug 2>&1 | grep -E "BUILD|e: "
"$(cygpath -u "$LOCALAPPDATA")/Android/Sdk/platform-tools/adb.exe" -s "$SERIAL" install -r app/build/outputs/apk/debug/app-debug.apk
echo "== done"
