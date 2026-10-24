#!/bin/bash
# fps_probe.sh <label> [secs]
#
# WHY THIS EXISTS. "The video's framerate is low" is two different claims that look
# identical in a player, and they need opposite fixes:
#   (a) the file has few frames            -> a capture problem
#   (b) the file has plenty of frames that REPEAT the same picture -> the game
#       itself is updating the screen slowly, and every faithful recording of it
#       will look choppy no matter how it is encoded
# (a) is answered by ffprobe. (b) needs the GAME's frame clock against the host's
# wall clock, which is what this measures: the engine logs "[MTX] frame N" (the
# root's own counter, incremented once per game frame in mtx_engine_policy) every
# 50 frames, and every transport line carries the frame it happened on. Polling that
# number with a timestamp gives the game's true frame rate, live.
#
# THE CAPTURE IS OFF BY DEFAULT, on purpose: DOSBox's ZMBV recorder is one of the
# suspects (it compresses every presented frame, 70 a second, on the same host that
# is already emulating three nested Dooms). MTX_CAP=1 runs the same probe with the
# recorder on, and the two rates are the measurement of what the recorder costs.
#
# It reuses record.sh for the launch/menu/level path - the frame rate depends on
# what the machine is doing, so the probe has to run the same thing the clip runs.
set -u
LABEL="${1:-fpsprobe}"; SECS="${2:-55}"
ROOT="$HOME/Documents/recursive_doom"
D="$ROOT/verify/rec-$LABEL"

# No switch presses: this measures the frame clock, not the input path.
export MTX_SWITCHES="${MTX_SWITCHES:-0}"
if [ -z "${MTX_CAP:-}" ]; then export MTX_NOCAP=1; else unset MTX_NOCAP; fi

# The highest frame number the engine has logged so far. Three line shapes carry a
# HOST frame number; the per-depth "depth 1 frame 400" lines carry a DIFFERENT
# counter (that depth's own), so they are deliberately not matched.
poll_frame() {
  python3 - "$D/MTX.LOG" <<'PY'
import re, sys
try:
    t = open(sys.argv[1], errors='ignore').read()
except Exception:
    print(0); raise SystemExit
ns = [int(m) for m in re.findall(r'\[MTX\] frame (\d+)', t)]
ns += [int(m) for m in re.findall(r'transport depth \d+ -> \d+ at frame (\d+)', t)]
ns += [int(m) for m in re.findall(r'instance hand-back at frame (\d+)', t)]
print(max(ns) if ns else 0)
PY
}

# The report goes to a FILE, and it records its OWN conditions first: a rate that only
# ever appeared in a terminal scrollback is not evidence, and a report that cannot
# contradict a claim about how it was produced is worse than no report. (Measured: an
# "fps-on" report was quoted as the capture-on run while the capture was OFF, because
# the state line went to the terminal and the file kept only rates.)
REPORT="$ROOT/verify/$LABEL.fps.txt"
: > "$REPORT"
say() { echo "$@" | tee -a "$REPORT"; }
if [ -z "${MTX_CAP:-}" ]; then CAPSTATE="OFF (MTX_NOCAP=1)"; else CAPSTATE="ON (MTX_CAP=1)"; fi
say "fps_probe $LABEL: ${SECS}s, capture $CAPSTATE"

( "$ROOT/verify/record.sh" "$LABEL" "$((SECS + 20))" --nobuild >"$D.probe.out" 2>&1 ) &
RP=$!

t0=$(date +%s)
prev_f=0; prev_t=0
say "  t(s)  frame   fps(window)"
while :; do
  t=$(( $(date +%s) - t0 ))
  [ "$t" -ge "$SECS" ] && break
  f=$(poll_frame)
  if [ "$f" -gt "$prev_f" ] && [ "$prev_t" -gt 0 ]; then
    dt=$(( t - prev_t )); df=$(( f - prev_f ))
    # integer arithmetic, one decimal
    printf "  %4d  %6d   %d.%d\n" "$t" "$f" "$(( df / dt ))" "$(( (df * 10 / dt) % 10 ))" | tee -a "$REPORT"
    prev_f=$f; prev_t=$t
  elif [ "$f" -gt "$prev_f" ] && [ "$prev_t" -eq 0 ]; then
    prev_f=$f; prev_t=$t
  fi
  sleep 1
done

say "--- the capture window, from DOSBox's own log (emulated time vs wall time) ---"
grep -aE "CAPTURE: (Capturing|Stopped)" "$D/db.log" 2>/dev/null | sed 's/^/  /' | tee -a "$REPORT"
AVI=$(ls -t "$D"/cap/*.avi 2>/dev/null | head -1)
if [ -n "$AVI" ]; then
  ffprobe -v error -select_streams v:0 -show_entries stream=r_frame_rate,nb_frames,duration \
          -of default=noprint_wrappers=1 "$AVI" | sed 's/^/  avi: /' | tee -a "$REPORT"
elif [ -n "${MTX_CAP:-}" ]; then
  # The flag is an intent; the AVI is the fact. Never let a capture-on report exist
  # without the recorder's own output in it.
  say "CAPTURE WAS ASKED FOR AND DID NOT RUN: no AVI under $D/cap - this is NOT a capture-on measurement"
fi
wait $RP 2>/dev/null
say "report: $REPORT"
