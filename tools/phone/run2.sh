#!/bin/bash
# usage: run2.sh <pack|nopack> <seconds> [args.txt lines...]
# pack: strip + gradle + install the current jniLibs. Then runs the game for <seconds>
# from INICIAR, with the given extra cvars, and pulls the log to out/.
# Set SONRAC_ADB_SERIAL to select a phone other than the previous default device.
P="C:/Users/EduPedu/Desktop/Decompilar/SonicRacingTransformed_Project"
S="$P/out"
ADB="$(cygpath -u "$LOCALAPPDATA")/Android/Sdk/platform-tools/adb.exe"
DEVICE="${SONRAC_ADB_SERIAL}"
MODE=$1; SECS=$2; shift 2
if [ "$MODE" == "pack" ]; then
  cd "$P/android"
  STRIPB="$(cygpath -u "$LOCALAPPDATA")/Android/Sdk/ndk/29.0.14206865/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-strip.exe"
  for f in app/src/main/jniLibs/arm64-v8a/*.so; do "$STRIPB" --strip-debug "$f"; done
  G=$(ls -d ~/.gradle/wrapper/dists/gradle-9.6.0-bin/*/gradle-9.6.0/bin/gradle.bat | head -1)
  export JAVA_HOME="C:/Users/EduPedu/.gradle/jdks/eclipse_adoptium-21-amd64-windows.2"
  cmd //c "$(cygpath -w "$G")" assembleDebug --no-daemon 2>&1 | grep -E "BUILD FAILED|e: "
  MSYS_NO_PATHCONV=1 "$ADB" -s "$DEVICE" install -r app/build/outputs/apk/debug/app-debug.apk 2>&1 | tail -1
fi
: > "$S/args.txt"
for a in "$@"; do echo "$a" >> "$S/args.txt"; done
export MSYS_NO_PATHCONV=1
T=$("$ADB" -s "$DEVICE" shell dumpsys thermalservice | grep "Temperature{" | grep -E "GPU0|skin" | sed -E 's/.*mValue=([0-9.]+).*mName=([A-Za-z0-9]+).*/\2=\1/' | tr '\n' ' ')
echo "temps before: $T"
"$ADB" -s "$DEVICE" shell am force-stop com.sonrac.game
if [ -s "$S/args.txt" ]; then
  "$ADB" -s "$DEVICE" push "$S/args.txt" /sdcard/Android/data/com.sonrac.game/files/args.txt >/dev/null
else
  "$ADB" -s "$DEVICE" shell rm -f /sdcard/Android/data/com.sonrac.game/files/args.txt
fi
"$ADB" -s "$DEVICE" shell monkey -p com.sonrac.game -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1
sleep 5; "$ADB" -s "$DEVICE" shell input tap 508 1001
sleep $SECS
"$ADB" -s "$DEVICE" shell am force-stop com.sonrac.game
"$ADB" -s "$DEVICE" shell rm -f /sdcard/Android/data/com.sonrac.game/files/args.txt
L=$("$ADB" -s "$DEVICE" shell "ls -t /sdcard/Android/data/com.sonrac.game/files/user/logs/ | head -1" | tr -d '\r')
"$ADB" -s "$DEVICE" pull "/sdcard/Android/data/com.sonrac.game/files/user/logs/$L" "$P/out/phone-auto.log" >/dev/null
T=$("$ADB" -s "$DEVICE" shell dumpsys thermalservice | grep "Temperature{" | grep -E "GPU0|skin" | sed -E 's/.*mValue=([0-9.]+).*mName=([A-Za-z0-9]+).*/\2=\1/' | tr '\n' ' ')
echo "temps after: $T"
