#!/bin/bash
# demo_video.sh [out.mp4] - record the ring demo and cut it into the presentation video.
#
# The clip: record.sh with the demo WADs (mkwadlevels.py --demo), no monsters, and the
# per-depth choreography - walk up to the screen, hold still, switch, round the ring.
# The cut: a title card, the clip with one caption per depth, and a closing ring diagram
# built from the clip's own frames (demo_cards.py, Doom's own font and flats).
#
# MTX_CUTS="6.2 20 25.5": the clip times of the three switches, used to time captions.
# They are printed by this script from the depth badge (the digit the engine draws),
# so after a new recording check them and re-run with --cut to re-cut only.
set -u
ROOT="${MTX_ROOT:-$HOME/Documents/recursive_doom}"; V="$ROOT/verify"
OUT="${1:-$V/matryoshka-ring.mp4}"; W="$V/rec-demo/cut"
if [ "${2:-}" != "--cut" ]; then
  MTX_LEVELARGS=--demo MTX_DOOMARGS=-nomonsters MTX_STEPS="Up:0.8 Up:0.25 Up:0.25 Down:1.0" \
  MTX_LOOK=3.5 MTX_SWITCHES=3 MTX_SWITCH_GAP=0 "$V/record.sh" demo 50
  pkill -9 -x dosbox 2>/dev/null; pkill -9 -x Xvfb 2>/dev/null
fi
AVI="$V/rec-demo/cap/video0001.avi"
python3 - "$AVI" <<'PY'
import subprocess, sys
W, H = 320, 200
b = subprocess.run(["ffmpeg", "-v", "error", "-i", sys.argv[1], "-vf", "fps=10,scale=320:200",
                    "-pix_fmt", "gray", "-f", "rawvideo", "-"], capture_output=True).stdout
prev = None
for i in range(len(b) // (W * H)):
    f = b[i * W * H:(i + 1) * W * H]
    sig = tuple(f[(10 + r * 2) * W + 293 + c * 2] > 128 for r in range(7) for c in range(5))
    if sig != prev:
        print("badge changes at %.1f s" % (i / 10)); prev = sig
PY
read C1 C2 C3 <<< "${MTX_CUTS:-6.2 20 25.5}"
mkdir -p "$W"
python3 "$V/demo_cards.py" "$AVI" "$W" || exit 1
ffmpeg -v error -y -loop 1 -t 5 -r 35 -i "$W/title.png" -vf format=yuv420p -c:v libx264 -crf 18 "$W/1.mp4"
ffmpeg -v error -y -i "$AVI" -i "$W/cap0.png" -i "$W/cap1.png" -i "$W/cap2.png" -i "$W/cap3.png" -an \
  -filter_complex "[0:v]fps=35,scale=1280:960:flags=neighbor,pad=1280:1080:0:0:black[b];\
[b][1:v]overlay=0:960:enable='lt(t,$C1)'[b1];[b1][2:v]overlay=0:960:enable='between(t,$C1,$C2)'[b2];\
[b2][3:v]overlay=0:960:enable='between(t,$C2,$C3)'[b3];[b3][4:v]overlay=0:960:enable='gte(t,$C3)',format=yuv420p[v]" \
  -map "[v]" -c:v libx264 -preset medium -crf 18 "$W/2.mp4"
ffmpeg -v error -y -loop 1 -t 8 -r 35 -i "$W/end.png" -vf format=yuv420p -c:v libx264 -crf 18 "$W/3.mp4"
printf "file '%s'\n" "$W/1.mp4" "$W/2.mp4" "$W/3.mp4" > "$W/list.txt"
ffmpeg -v error -y -f concat -safe 0 -i "$W/list.txt" -c copy "$OUT" && echo "video: $OUT"
