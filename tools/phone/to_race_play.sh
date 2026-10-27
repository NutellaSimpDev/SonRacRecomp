#!/bin/bash
# to_race_keep with the extra A to skip the intro; texdump passes "0" first.
if [ "$1" = "0" ]; then shift; fi
if [ $# -eq 0 ]; then
  set -- "--guest_vblank_multiplier=1" "--sonrac_cri_audio_poll_us=100" "--audio_pace_callback=true" 
fi
P="C:/Users/EduPedu/Desktop/Decompilar/SonicRacingTransformed_Project"
bash "$P/tools/phone/to_race_keep.sh" "$@" | tail -1
bash "$P/tools/phone/k.sh" A; timeout 8 tail -f /dev/null
echo "in race"
