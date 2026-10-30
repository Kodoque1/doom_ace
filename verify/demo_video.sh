#!/bin/bash
# demo_video.sh [out.mp4] - record the ring demo and cut it into the presentation video.
#
# The clip: record.sh with the demo WADs (mkwadlevels.py --demo), no monsters, and the
# choreography - a small turn (the keyboard is shared, so every depth moves; walking
# measured into the screens until the tunnel collapsed, strafing out of their view, and
# ',' '.' are ACE's inventory keys), hold still, switch, round the ring.
# The cut: a title card, the clip with one caption per depth, and a closing ring diagram
# built from the clip's own frames (demo_cards.py, Doom's own font and flats).
#
# The clip times of the three switches are READ from the clip: the depth badge's digit
# (drawn by the engine) is sampled ten times a second, and the first three changes of
# depth are the cuts. The ring diagram takes one frame from the middle of each of the
# first three stays. --cut re-cuts an existing recording without re-recording it.
set -u
ROOT="${MTX_ROOT:-$HOME/Documents/recursive_doom}"; V="$ROOT/verify"
OUT="${1:-$V/matryoshka-ring.mp4}"; W="$V/rec-demo/cut"
if [ "${2:-}" != "--cut" ]; then
  MTX_LEVELARGS=--demo MTX_DOOMARGS=-nomonsters MTX_STEPS="Left:0.12 Right:0.24 Left:0.24 Right:0.12" \
  MTX_LOOK=3.5 MTX_SWITCHES=3 MTX_SWITCH_GAP=0 "$V/record.sh" demo 50
  pkill -9 -x dosbox 2>/dev/null; pkill -9 -x Xvfb 2>/dev/null
fi
AVI="$V/rec-demo/cap/video0001.avi"
read C1 C2 C3 R0 R1 R2 <<< "$(python3 - "$AVI" <<'PY'
import subprocess, sys
W, H = 320, 200
b = subprocess.run(["ffmpeg", "-v", "error", "-i", sys.argv[1], "-vf", "fps=10,scale=320:200",
                    "-pix_fmt", "gray", "-f", "rawvideo", "-"], capture_output=True).stdout
n = len(b) // (W * H)
sigs = []
for i in range(n):
    f = b[i * W * H:(i + 1) * W * H]
    sigs.append(tuple(f[(10 + r * 2) * W + 293 + c * 2] > 128 for r in range(7) for c in range(5)))
# a depth is a signature that holds for at least a second (the teleport sweep passes
# over the badge for a few frames)
stays, cur, start = [], None, 0
for i, s in enumerate(sigs + [None]):
    if s != cur:
        if cur is not None and i - start >= 10:
            if not stays or stays[-1][0] != cur:
                stays.append([cur, start, i])
            else:
                stays[-1][2] = i
        cur, start = s, i
cuts = [st[1] / 10 for st in stays[1:4]]
mids = [(st[1] + st[2]) / 20 for st in stays[:3]]
sys.stderr.write("stays (s): %s\n" % [(st[1] / 10, st[2] / 10) for st in stays])
print(" ".join("%.1f" % t for t in cuts + mids))
PY
)"
echo "cuts: $C1 $C2 $C3   ring frames: $R0 $R1 $R2"
export MTX_RING_T="$R0 $R1 $R2"
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
