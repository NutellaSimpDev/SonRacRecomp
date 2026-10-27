#!/bin/bash
# texdump.sh <format> <width> <base_decimal|0> <outdir>: go to the menu (30 FPS), dump one texture load.
P="C:/Users/EduPedu/Desktop/Decompilar/SonicRacingTransformed_Project"
ADB="$(cygpath -u "$LOCALAPPDATA")/Android/Sdk/platform-tools/adb.exe"; export MSYS_NO_PATHCONV=1
D=/storage/emulated/0/Android/data/com.sonrac.game/files/user
"$ADB" -s ${SONRAC_ADB_SERIAL} shell "rm -f $D/texdump_*"
bash "$P/tools/phone/${NAV:-to_title_keep.sh}" 0 --guest_vblank_multiplier=1 --edram_skip_overlap_transfers=false --vulkan_dump_texture_format=$1 --vulkan_dump_texture_min_width=$2 --vulkan_dump_texture_base=$3 --vulkan_dump_texture_skip=${SKIP:--1} --vulkan_dump_texture_dir=$D ${EXTRA_ARGS} 2>&1 | tail -1
"$ADB" -s ${SONRAC_ADB_SERIAL} shell touch $D/texdump_now; sleep 4
mkdir -p "$P/out/$4"; for n in texdump_source.bin texdump_loaded.bin texdump_info.txt; do "$ADB" -s ${SONRAC_ADB_SERIAL} pull $D/$n "$P/out/$4/$n" >/dev/null 2>&1; done
"$ADB" -s ${SONRAC_ADB_SERIAL} shell am force-stop com.sonrac.game; "$ADB" -s ${SONRAC_ADB_SERIAL} shell "rm -f /sdcard/Android/data/com.sonrac.game/files/args.txt $D/texdump_now"
cat "$P/out/$4/texdump_info.txt"; echo
