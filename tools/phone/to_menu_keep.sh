#!/bin/bash
# menu_run.sh <seconds_in_menu> [args.txt lines...]: launches the installed APK with the given
# cvars, navigates to the main menu, stays there, pulls the log to out/phone-auto.log and a
# screenshot to out/menu.png.
P="C:/Users/EduPedu/Desktop/Decompilar/SonicRacingTransformed_Project"
ADB="$(cygpath -u "$LOCALAPPDATA")/Android/Sdk/platform-tools/adb.exe"
DEVICE="${SONRAC_ADB_SERIAL}"
export MSYS_NO_PATHCONV=1
SECS=$1; shift
if [ $# -eq 0 ]; then
  set -- "--guest_vblank_multiplier=1" "--sonrac_cri_audio_poll_us=100" "--audio_pace_callback=true" 
fi
: > "$P/out/args.txt"; for a in "$@"; do echo "$a" >> "$P/out/args.txt"; done
A() { "$ADB" -s "$DEVICE" shell input swipe 2443 761 2443 761 200; }
DOWN() { "$ADB" -s "$DEVICE" shell input swipe 547 1089 547 1089 200; }
"$ADB" -s "$DEVICE" shell am force-stop com.sonrac.game
"$ADB" -s "$DEVICE" push "$P/out/args.txt" /sdcard/Android/data/com.sonrac.game/files/args.txt >/dev/null
"$ADB" -s "$DEVICE" shell monkey -p com.sonrac.game -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1
sleep 5; "$ADB" -s "$DEVICE" shell input tap 508 1001
sleep 34; A            # skip the intro movie (logos ~11 s + start of the cinematic)
sleep 8; A             # title screen
sleep 8; A             # autosave warning
sleep 9; DOWN; sleep 1.5; A   # load error -> continue without loading
sleep 8; A             # confirm
sleep 8; A             # autosave disabled
sleep 8; A             # new character notice
echo "in menu"
