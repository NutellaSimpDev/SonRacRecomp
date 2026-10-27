#!/bin/bash
# depthdump.sh <outdir> [extra cvars]: menu at 30 FPS, dump the depth the first sky draw sees.
P="C:/Users/EduPedu/Desktop/Decompilar/SonicRacingTransformed_Project"
ADB="$(cygpath -u "$LOCALAPPDATA")/Android/Sdk/platform-tools/adb.exe"; export MSYS_NO_PATHCONV=1
D=/storage/emulated/0/Android/data/com.sonrac.game/files/user
OUT=$1; shift
"$ADB" -s ${SONRAC_ADB_SERIAL} shell "rm -f $D/depthimg*"
bash "$P/tools/phone/${NAV:-to_title_keep.sh}" 0 --guest_vblank_multiplier=1 --vulkan_dump_texture_dir=$D "$@" 2>&1 | tail -1
"$ADB" -s ${SONRAC_ADB_SERIAL} shell touch $D/depthimg_now; sleep 4
mkdir -p "$P/out/$OUT"; for n in depthimg.bin depthimg.txt; do "$ADB" -s ${SONRAC_ADB_SERIAL} pull $D/$n "$P/out/$OUT/$n" >/dev/null 2>&1; done
"$ADB" -s ${SONRAC_ADB_SERIAL} shell am force-stop com.sonrac.game; "$ADB" -s ${SONRAC_ADB_SERIAL} shell "rm -f /sdcard/Android/data/com.sonrac.game/files/args.txt $D/depthimg_now"
py - "$P/out/$OUT" <<'PY'
import numpy as np, sys
D=sys.argv[1]
info=open(D+'/depthimg.txt').read(); print(info)
w=int(info.split('width=')[1].split()[0]); h=int(info.split('height=')[1].split()[0])
raw=np.fromfile(D+'/depthimg.bin',np.uint32).reshape(h,w)
a=raw.view(np.float32)[:544,:1152]
print('min/mean/max', a.min(), a.mean(), a.max(), 'nonzero', np.count_nonzero(a))
PY
