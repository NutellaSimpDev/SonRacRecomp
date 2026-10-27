#!/bin/bash
# k.sh <A|B|DOWN|UP|LEFT|RIGHT|shot> : one input or a small screenshot to out/k.png
P="C:/Users/EduPedu/Desktop/Decompilar/SonicRacingTransformed_Project"
ADB="$(cygpath -u "$LOCALAPPDATA")/Android/Sdk/platform-tools/adb.exe"
DEVICE="${SONRAC_ADB_SERIAL}"
export MSYS_NO_PATHCONV=1
D=/storage/emulated/0/Android/data/com.sonrac.game/files/user
case $1 in
  A) "$ADB" -s "$DEVICE" shell input swipe 2443 761 2443 761 200;;
  B) "$ADB" -s "$DEVICE" shell input swipe 2563 631 2563 631 200;;
  DOWN) "$ADB" -s "$DEVICE" shell input swipe 547 1089 547 1089 200;;
  UP) "$ADB" -s "$DEVICE" shell input swipe 547 853 547 853 200;;
  LEFT) "$ADB" -s "$DEVICE" shell input swipe 429 969 429 969 200;;
  RIGHT) "$ADB" -s "$DEVICE" shell input swipe 663 969 663 969 200;;
  shot) "$ADB" -s "$DEVICE" exec-out screencap -p > "$P/out/k_full.png"; py -c "from PIL import Image; Image.open(r'$P/out/k_full.png').resize((896,405)).save(r'$P/out/k.png')";;
esac
