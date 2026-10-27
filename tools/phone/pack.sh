#!/bin/bash
# pack.sh: strip the jniLibs, build the debug APK and install it on the phone.
P="C:/Users/EduPedu/Desktop/Decompilar/SonicRacingTransformed_Project"
cd "$P/android"
STRIPB="$(cygpath -u "$LOCALAPPDATA")/Android/Sdk/ndk/29.0.14206865/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-strip.exe"
for f in app/src/main/jniLibs/arm64-v8a/*.so; do "$STRIPB" --strip-debug "$f"; done
G=$(ls -d ~/.gradle/wrapper/dists/gradle-9.6.0-bin/*/gradle-9.6.0/bin/gradle.bat | head -1)
export JAVA_HOME="C:/Users/EduPedu/.gradle/jdks/eclipse_adoptium-21-amd64-windows.2"
APK=app/build/outputs/apk/debug/app-debug.apk
BEFORE=$(stat -c %Y "$APK" 2>/dev/null || echo 0)
cmd //c "$(cygpath -w "$G")" assembleDebug --no-daemon > "$P/out/gradle.log" 2>&1
grep -E "BUILD|e: " "$P/out/gradle.log"
if [ "$(stat -c %Y "$APK" 2>/dev/null || echo 0)" = "$BEFORE" ]; then
  echo "pack: the APK was not rebuilt, see out/gradle.log"; tail -20 "$P/out/gradle.log"; exit 1
fi
ADB="$(cygpath -u "$LOCALAPPDATA")/Android/Sdk/platform-tools/adb.exe"
MSYS_NO_PATHCONV=1 "$ADB" -s "${SONRAC_ADB_SERIAL:-${SONRAC_ADB_SERIAL}}" install -r app/build/outputs/apk/debug/app-debug.apk 2>&1 | tail -1
