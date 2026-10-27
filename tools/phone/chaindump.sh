#!/bin/bash
# chaindump.sh <outdir> [cvars]: title screen at 30 FPS, dumps the depth bound at the first draw
# matching sonrac_depthdump_base/ctl and the color source of the last native color -> 4x depth
# transfer before it.
P="C:/Users/EduPedu/Desktop/Decompilar/SonicRacingTransformed_Project"
ADB="$(cygpath -u "$LOCALAPPDATA")/Android/Sdk/platform-tools/adb.exe"; export MSYS_NO_PATHCONV=1
D=/storage/emulated/0/Android/data/com.sonrac.game/files/user
OUT=$1; shift
"$ADB" -s ${SONRAC_ADB_SERIAL} shell "rm -f $D/depthimg* $D/rtimg* $D/teximg*"
bash "$P/tools/phone/${NAV:-to_title_keep.sh}" 0 --guest_vblank_multiplier=1 --vulkan_dump_texture_dir=$D "$@" 2>&1 | tail -1
"$ADB" -s ${SONRAC_ADB_SERIAL} shell touch $D/depthimg_now $D/rtimg_now $D/teximg_now; sleep 4
mkdir -p "$P/out/$OUT"; for n in depthimg.bin depthimg.txt rtimg_0.bin rtimg_0.txt teximg.bin teximg.txt; do "$ADB" -s ${SONRAC_ADB_SERIAL} pull $D/$n "$P/out/$OUT/$n" >/dev/null 2>&1; done
"$ADB" -s ${SONRAC_ADB_SERIAL} exec-out screencap -p > "$P/out/$OUT/screen.png"
"$ADB" -s ${SONRAC_ADB_SERIAL} shell am force-stop com.sonrac.game; "$ADB" -s ${SONRAC_ADB_SERIAL} shell "rm -f /sdcard/Android/data/com.sonrac.game/files/args.txt $D/depthimg_now $D/rtimg_now $D/teximg_now"
