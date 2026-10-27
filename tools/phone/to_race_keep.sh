#!/bin/bash
# to_race_keep.sh [args.txt lines...]: main menu -> Career (goes straight to the next race event),
# (World Tour, first event, medium), skips the track intro and stays in the race, running.
if [ $# -eq 0 ]; then
  set -- "--guest_vblank_multiplier=1" "--sonrac_cri_audio_poll_us=100" "--audio_pace_callback=true" 
fi
P="C:/Users/EduPedu/Desktop/Decompilar/SonicRacingTransformed_Project"
bash "$P/tools/phone/to_menu_keep.sh" 0 "$@" | tail -1
K() { bash "$P/tools/phone/k.sh" "$1"; }
K LEFT; K LEFT; K LEFT; K A        # Career
timeout 5 tail -f /dev/null; K A   # World Tour: Sunshine Coast
timeout 5 tail -f /dev/null; K A   # first event (race)
timeout 5 tail -f /dev/null; K A   # difficulty: medium
timeout 5 tail -f /dev/null; K A   # character: Sonic
timeout 45 tail -f /dev/null       # loading
K A; timeout 6 tail -f /dev/null; K A   # skip the track intro (and the countdown screen)
timeout 10 tail -f /dev/null
echo "in race"
