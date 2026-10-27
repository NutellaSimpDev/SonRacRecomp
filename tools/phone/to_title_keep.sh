#!/bin/bash
# to_title_keep.sh <unused> [args.txt lines...]: launches the installed APK with the given cvars,
# skips the intro movie and stays on the title screen (PRESS START, floating islands), running.
P="C:/Users/EduPedu/Desktop/Decompilar/SonicRacingTransformed_Project"
ADB="$(cygpath -u "$LOCALAPPDATA")/Android/Sdk/platform-tools/adb.exe"
DEVICE="${SONRAC_ADB_SERIAL}"
export MSYS_NO_PATHCONV=1
shift
: > "$P/out/args.txt"; for a in "$@"; do echo "$a" >> "$P/out/args.txt"; done
A() { "$ADB" -s "$DEVICE" shell input swipe 2443 761 2443 761 200; }
"$ADB" -s "$DEVICE" shell am force-stop com.sonrac.game
"$ADB" -s "$DEVICE" push "$P/out/args.txt" /sdcard/Android/data/com.sonrac.game/files/args.txt >/dev/null
"$ADB" -s "$DEVICE" shell monkey -p com.sonrac.game -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1
sleep 5; "$ADB" -s "$DEVICE" shell input tap 508 1001
sleep 34; A            # skip the intro movie (logos ~11 s + start of the cinematic)
sleep 7
echo "in title"
