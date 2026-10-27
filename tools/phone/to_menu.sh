#!/bin/bash
# Launches the game and navigates to the main menu with long presses; screenshot to $1.
ADB="$(cygpath -u "$LOCALAPPDATA")/Android/Sdk/platform-tools/adb.exe"
export MSYS_NO_PATHCONV=1
A() { "$ADB" -s ${SONRAC_ADB_SERIAL} shell input swipe 2443 761 2443 761 200; }
DOWN() { "$ADB" -s ${SONRAC_ADB_SERIAL} shell input swipe 547 1089 547 1089 200; }
"$ADB" -s ${SONRAC_ADB_SERIAL} shell am force-stop com.sonrac.game
"$ADB" -s ${SONRAC_ADB_SERIAL} shell monkey -p com.sonrac.game -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1
sleep 5; "$ADB" -s ${SONRAC_ADB_SERIAL} shell input tap 508 1001
sleep 32; A          # skip the intro movie
sleep 5; A           # title screen
sleep 7; A           # autosave warning
sleep 7; DOWN; sleep 1; A   # continue without loading
sleep 8; A           # confirm
sleep 8; A           # autosave disabled
sleep 6; A           # new character notice
sleep 8
"$ADB" -s ${SONRAC_ADB_SERIAL} exec-out screencap -p > "$1"
