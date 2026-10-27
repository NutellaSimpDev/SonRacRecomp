#!/bin/bash
# title_run.sh [args.txt lines...]: title screen with the given cvars, screenshot to out/title.png,
# log to out/phone-auto.log, then stops the game.
P="C:/Users/EduPedu/Desktop/Decompilar/SonicRacingTransformed_Project"
ADB="$(cygpath -u "$LOCALAPPDATA")/Android/Sdk/platform-tools/adb.exe"
DEVICE="${SONRAC_ADB_SERIAL}"
export MSYS_NO_PATHCONV=1
bash "$P/tools/phone/to_title_keep.sh" 0 "$@" | tail -1
"$ADB" -s "$DEVICE" exec-out screencap -p > "$P/out/title.png"
"$ADB" -s "$DEVICE" shell am force-stop com.sonrac.game
"$ADB" -s "$DEVICE" shell rm -f /sdcard/Android/data/com.sonrac.game/files/args.txt
L=$("$ADB" -s "$DEVICE" shell "ls -t /sdcard/Android/data/com.sonrac.game/files/user/logs/ | head -1" | tr -d '\r')
"$ADB" -s "$DEVICE" pull "/sdcard/Android/data/com.sonrac.game/files/user/logs/$L" "$P/out/phone-auto.log" >/dev/null
