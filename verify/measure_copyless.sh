#!/bin/bash
# measure_copyless.sh <label>
# One clip per mode, same everything else, so the two can be compared:
#   copyless on  -> compare every switch, copy only the bytes that differ
#   copyless off -> copy the whole 572,953-byte registered set both ways
# At a 1-frame slice, which is the regime that needs the saving: every frame is a
# switch, so the snapshot is copied (or compared) at the frame rate.
set -u
ROOT="$HOME/Documents/recursive_doom"
LABEL="$1"
SRC="$ROOT/nested/dos_state.c"

python3 - "$SRC" "$2" <<'PY'
import sys
p, val = sys.argv[1], sys.argv[2]
s = open(p).read()
s = s.replace("#define MTX_COPYLESS 1", "#define MTX_COPYLESS " + ("1" if val == "on" else "0"))
open(p, "w").write(s)
PY
grep -n "define MTX_COPYLESS" "$SRC"
cd "$ROOT" && for p in $(pgrep dosbox); do kill -9 $p 2>/dev/null; done
MTX_WALK=1 timeout 400 ./verify/record.sh "$LABEL" 18 2>&1 | grep -E "ILLEGAL|DYNX86|switch:|clip:"
cd verify && ffmpeg -loglevel error -y -i "rec-$LABEL/$LABEL.mp4" -vf "crop=350:250:28:34,format=gray" -f rawvideo "/tmp/$LABEL.i.raw" \
  && ffmpeg -loglevel error -y -i "rec-$LABEL/$LABEL.mp4" -vf "crop=350:250:400:200,format=gray" -f rawvideo "/tmp/$LABEL.m.raw" \
  && python3 - "$LABEL" <<'PY'
import sys
lab = sys.argv[1]
W,H = 350,250; N = W*H; SPLIT = 90
for name in ("inset","main"):
    b = open("/tmp/%s.%s.raw" % (lab, "i" if name=="inset" else "m"),"rb").read()
    fr = len(b)//N
    sl = b[SPLIT*N:fr*N]; n = fr-SPLIT
    means = [sum(sl[i*N:(i+1)*N])/N for i in range(n)]
    ch = [abs(means[i]-means[i-1]) for i in range(1,n)]
    upd = sum(1 for c in ch if c > 0.5)
    best = run = 0
    for c in ch:
        run = run+1 if c <= 0.05 else 0
        best = max(best, run)
    print("  %-6s %3d updates (%.1f/s), longest freeze %.2f s" % (name, upd, upd/(n/20.0), best/20.0))
PY
