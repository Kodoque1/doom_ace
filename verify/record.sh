#!/bin/bash
# record.sh <label> [secs] [--nobuild]
# Record the "doom inception" switch as a VIDEO, headless.
#
# WHAT CHANGED, AND WHY (the tearing). The clips used to be captured with ffmpeg's
# x11grab on the DOSBox window. The emulated screen is written by a plain `rep
# movsl` into video memory (engine/asm.S, I_FinishUpdate) and DOSBox presents the
# window from that memory at its own frame boundary, so an X11 grab can read the
# window while it is being updated: that is the horizontal tear the clips showed,
# and it is a CAPTURE artifact rather than a display bug. DOSBox Staging can record
# the EMULATED FRAMEBUFFER at frame boundaries instead (Ctrl+F7, lossless
# AVI/ZMBV), which cannot contain a half-updated screen. So the capture is DOSBox's
# own, and the AVI is converted to MP4 here.
#
# The order still matters: the capture must already be running when the effect can
# happen, so: launch, walk into a level (Return presses, same as run.sh), THEN start
# the capture, then press the switch key HELD (a tap can fall between two frames and
# be invisible), and keep recording well past it.
#
# The switch now TRANSPORTS the viewer instead of toggling an overlay, so a press
# changes the whole picture and the ring needs several presses to show the loop:
# MTX_SWITCHES (default 1) says how many, each held for a second and spaced.
#
# Verification is a separate step on purpose (see the skill's video reference): this
# writes the clip and the log, then samples frames; the frames are what gets looked
# at, because metadata proves the file exists and nothing else.
set -u
LABEL="${1:-clip}"; SECS="${2:-20}"; NOBUILD="${3:-}"
SWITCHES="${MTX_SWITCHES:-1}"
ROOT="${MTX_ROOT:-$HOME/Documents/recursive_doom}"
REF="${MTX_REF:-$ROOT/verify/after}"   # MTX_REF picks a variant game image
D="$ROOT/verify/rec-$LABEL"
DB="$HOME/Downloads/dosbox-staging-linux-x86_64-0.83.0-7b400/dosbox"
DISP=":$((RANDOM%40+150))"

if [ "$NOBUILD" != "--nobuild" ]; then
  ( cd "$ROOT/doom_ace" && make -C engine >/dev/null 2>&1 && make -C loader >/dev/null 2>&1 ) \
    || { echo "BUILD FAILED"; exit 1; }
  # AFTER the build, never before: the level WADs carry a COPY of the exploit
  # lumps, so a rebuild makes them stale and they would silently test the
  # previous engine.
  ( cd "$ROOT" && python3 verify/mkwadlevels.py --iwad "$REF/DOOM2.WAD" \
      --loader doom_ace/loader/loader.wad --out verify/levels ${MTX_LEVELARGS:-} >/dev/null ) \
    || { echo "LEVEL WAD BUILD FAILED"; exit 1; }
fi

mkdir -p "$D" "$D/cap"
rm -f "$D"/*.png "$D"/*.mp4 "$D/MTX.LOG" "$D/LOG.TXT" "$D"/cap/*.avi
ln -sf "$REF/DOOM2.EXE" "$D/DOOM2.EXE"
ln -sf "$REF/DOOM2.WAD" "$D/DOOM2.WAD"
# All three level WADs are COPIED into the mounted directory either way - what
# varies is which of them are on the COMMAND LINE, and that is the whole
# difference between the two options:
#   option A (default): all three, so the ROOT loads them once and each depth
#     picks its own map by number; their maps have distinct names (MAP02, MAP03)
#     so no file overrides another - see mkwadlevels.py.
#   option B (MTX_WADS="level0.wad"): only the WAD that bootstraps ACE is on the
#     command line, and each depth reaches its level by opening its own file at
#     its first level load (mtx_level_wad_add) - so level1.wad/level2.wad have to
#     be present for the DEPTHS to find, which is why they are copied regardless.
# MTX_LEVELS picks which level WADs are staged AT ALL. Staging only level0.wad is how
# a SINGLE instance gets measured: each depth finds its level by opening level<d>.wad
# (mtx_level_wad_add), so with those files absent the ring has nothing to push and the
# root runs alone - the only way to see the engine's OWN frame rate without the ring
# dividing it by three. Default keeps the shipped behaviour exactly.
LEVELS="${MTX_LEVELS:-0 1 2}"
for i in $LEVELS; do cp "$ROOT/verify/levels/level$i.wad" "$D/level$i.wad"; done
if [ -n "${MTX_WADS:-}" ]; then
  WADS="$MTX_WADS"
else
  WADS=""; for i in $LEVELS; do WADS="$WADS level$i.wad"; done
fi

# SOUND IS OPT-IN (MTX_SOUND=1), and this is the measurement behind that default.
#
# The switch's teleport sound is the engine's own call (S_StartSound with Doom's own
# teleport sound, resolved by name - see mtx_fx_start), and the engine plays it. What
# does not survive is the emulated SOUND DEVICE in a three-instance run: with the
# device enabled, every attempt died before the children booted, with
# "NetUpdate: netbuffer->numtics > BACKUPTICS" - Doom's tic buffer overflowing, which
# is what a frame loop that cannot keep up looks like from inside the game. Measured
# on four runs (with and without the video capture, and with the nested instances'
# own sound device already zeroed in the engine), so it is the ROOT's device that is
# enough. The likely mechanism is that a Sound Blaster is IRQ-driven: its DMA
# completion handler runs at interrupt time in WHICHEVER depth's context is loaded,
# and a per-depth context switch under an interrupt handler is exactly what this
# engine has not isolated. That is a roadmap item, not a harness flag.
#
# So: sound OFF for a recording (reliable), sound ON with MTX_SOUND=1 to reproduce
# the failure. Doom's own config is what reaches `snd_sfxdevice`: ACE's config only
# fills the value in when Doom's is zero, and a WAD-carried value was measured not to
# survive. 3 = Sound Blaster; the port/irq/dma match the [sblaster] section below.
if [ -n "${MTX_SOUND:-}" ]; then
  cat > "$D/default.cfg" <<'CFG'
snd_sfxdevice 3
snd_musicdevice 0
snd_sbport 544
snd_sbirq 7
snd_sbdma 1
snd_SfxVolume 8
snd_MusicVolume 0
CFG
else
  rm -f "$D/default.cfg"
fi

# sbtype: a Sound Blaster, so the switch's teleport sound is actually audible - and
# therefore audible in the capture. Doom detects it from BLASTER, which DOSBox sets.
cat > "$D/run.conf" <<EOF
[sdl]
output=texture
fullscreen=false
[dosbox]
machine=svga_s3
memsize=63
[cpu]
cpu_cycles=max
[sblaster]
sbtype=$([ -n "${MTX_SOUND:-}" ] && echo sb16 || echo none)
[capture]
capture_dir=$D/cap
# Extra config, verbatim, from the environment - how a DOSBox setting is TESTED
# against the baseline without touching this script: MTX_CONF_EXTRA=$'[cpu]\ncore=normal'
# The block is inserted before [autoexec] and DOSBox lets a later section override an
# earlier one, so a variant can restate a whole section.
${MTX_CONF_EXTRA:-}
[autoexec]
mount c "$D"
c:
doom2 -file $WADS -warp 1 ${MTX_DOOMARGS:-} > LOG.TXT
EOF

pkill -f "Xvfb $DISP" 2>/dev/null
Xvfb "$DISP" -screen 0 1024x768x24 >/dev/null 2>&1 & XP=$!
export DISPLAY="$DISP"
sleep 2

( cd "$D" && timeout $((SECS + 90)) "$DB" -conf run.conf --noprimaryconf > db.log 2>&1 ) & DP=$!
sleep 4

# Reach a level: the first press clears the startup notice, the rest walk the menu
# (title -> New Game -> skill). Once in a level Return is just "use".
#
# BOUNDED BY THE ENGINE'S OWN EVIDENCE, not by a count: the run is ready when the
# engine's log says a level is up, and with the sound device enabled the boot got
# long enough that a fixed number of presses landed too early - measured, a run that
# pressed ten times and logged zero switches because it was still in the menu when
# the capture started. The log is written by the engine, so this is the real
# readiness signal rather than a longer guess.
# The signal is the DEEPEST level being up, not the root's: the root's level is up at
# frame 1 (it loads before the host boots), so waiting for "level UP" alone started the
# capture and the presses while the game was still on the title - measured, a switch at
# frame 1. The children's levels come up once the ring is really running, which is also
# when a press means something.
for _k in $(seq 1 25); do
  grep -qa "depth 2 level UP" "$D/MTX.LOG" 2>/dev/null && break
  xdotool key --clearmodifiers Return 2>/dev/null
  sleep 2
done
if ! grep -qa "depth 2 level UP" "$D/MTX.LOG" 2>/dev/null; then
  echo "NO LEVEL: the deepest level never came up"
fi
sleep 2

WID=""
for i in $(seq 1 24); do
  WID=$(xdotool search --name '.' 2>/dev/null | head -1)
  [ -n "$WID" ] && break
  sleep 0.5
done
if [ -z "$WID" ]; then echo "NO WINDOW"; kill $DP $XP 2>/dev/null; exit 1; fi
eval "$(xdotool getwindowgeometry --shell "$WID")"   # WIDTH HEIGHT X Y
echo "window $WID ${WIDTH}x${HEIGHT} at $X,$Y"

# GIVE THE WINDOW THE INPUT FOCUS, explicitly. Under Xvfb there is no window manager,
# so nothing assigns the focus: SDL may take it a few seconds after mapping the window,
# and every key sent before that goes nowhere - measured, whole runs with the ring
# running normally (102 hand-backs, three levels up) and ZERO key sightings, i.e. the
# game never saw a single press. Early presses failed and later ones worked, which is
# what a focus race looks like from the outside.
xdotool windowfocus --sync "$WID" 2>/dev/null || xdotool windowactivate "$WID" 2>/dev/null
sleep 1

sleep 2
# DOSBox's OWN recorder, started before the effect can happen (Ctrl+F7 toggles it).
# MTX_NOCAP=1 skips it, which is how the capture's own effect on the run is told
# apart from the engine's: a run without it is still a valid run of the ENGINE.
if [ -z "${MTX_NOCAP:-}" ]; then
  echo "starting the DOSBox capture (Ctrl+F7)"
  xdotool key --clearmodifiers ctrl+F7 2>/dev/null
  # Ctrl is also Doom's FIRE key, and the game sees the hotkey's Ctrl go down. Tap it
  # so it ends released - with one keyboard for the whole ring (mtx_keys_route) a
  # stuck Ctrl would fire in every depth for the rest of the clip.
  sleep 0.5
  xdotool key Control_L 2>/dev/null
fi
sleep 3

# The switch key, HELD, one press per switch. Movement is optional but makes the
# transported view visibly RUNNING rather than a still of a still.
# MTX_WALKKEYS picks the held keys (default: run in a circle); "Up" walks straight at
# the screen-wall each level plants ahead of the start.
WALKKEYS="${MTX_WALKKEYS:-Up Right}"
if [ -n "${MTX_WALK:-}" ]; then
  for k in $WALKKEYS; do xdotool keydown --clearmodifiers "$k" 2>/dev/null; done
fi

# One switch press, HELD, with the RELEASE VERIFIED against the engine's own log.
#
# Why the verification: a lost keyup leaves the game's key array saying "still down",
# so the next press is not a leading edge and the switch silently does nothing -
# measured, a three-press recording that switched once, with the depth key sightings
# running continuously from the first press to the end of the clip. The engine's
# sightings ARE the release signal (they only happen while the key is down), so the
# loop below waits for them to stop and re-sends the release if they do not. That
# turns "hope the keyup lands" into a check with a retry.
# grep -c prints the count AND exits 1 when it is zero, so `|| echo 0` would make the
# substitution two lines and every arithmetic test on it an error. Take the count only.
count_of() { grep -ac "$1" "$D/MTX.LOG" 2>/dev/null | head -1 | tr -d '[:space:]'; }
sightings() { local n; n=$(count_of "SEES the key"); echo "${n:-0}"; }
switches()  { local n; n=$(count_of "transport depth"); echo "${n:-0}"; }

# Each switch is RETRIED until the ENGINE says it happened, because the key delivery
# through X is not reliable here: measured, three presses producing one switch, with
# the depth sightings running continuously from the first press to the end of the clip
# (the release never arrived, so the next press was not a leading edge). A full
# press/release cycle is sent before each retry - that re-syncs the state that a lost
# release leaves stuck, which repeated bare releases did not.
# MTX_STEPS="Up:1.2 Up:1 Down:1.5 Up:0": the demo choreography, one KEY:SECONDS per
# stay (before each switch, and after the last). On arriving in a depth, hold KEY for
# SECONDS, release, then hold still for MTX_LOOK seconds (default 3) so the
# screen-wall in view - the next depth's picture - can be read before the next
# switch. Per depth because the maps differ: MAP03 starts 36 units from one of its
# screens, so that stay backs off instead of walking in.
STAY=0
step() {
  [ -z "${MTX_STEPS:-}" ] && return
  local s; s=$(echo $MTX_STEPS | cut -d' ' -f$((STAY + 1)))
  STAY=$((STAY + 1))
  [ -z "$s" ] && return
  local k=${s%%:*} t=${s##*:}
  xdotool keydown --clearmodifiers "$k" 2>/dev/null; sleep "$t"
  xdotool keyup --clearmodifiers "$k" 2>/dev/null; sleep 0.2
  xdotool keyup --clearmodifiers "$k" 2>/dev/null   # a lost release walks the ring into the wall
  sleep "${MTX_LOOK:-3}"
}
for _s in $(seq 1 "$SWITCHES"); do
  step
  want=$(( $(switches) + 1 ))
  echo "pressing the switch key (switch $_s of $SWITCHES, held)"

  for _try in $(seq 1 4); do
    xdotool keydown --clearmodifiers y 2>/dev/null
    sleep 1
    xdotool keyup --clearmodifiers y 2>/dev/null

    # wait for the release to be seen: the sightings only happen while the key is down
    for _w in $(seq 1 8); do
      a=$(sightings); sleep 1; b=$(sightings)
      [ "$a" = "$b" ] && break
      xdotool keyup --clearmodifiers y 2>/dev/null
    done

    [ "$(switches)" -ge "$want" ] && break
    echo "  (switch $_s did not register: retrying)"
    # a full cycle, then make sure it ends released
    xdotool key --clearmodifiers y 2>/dev/null
    xdotool keyup --clearmodifiers y 2>/dev/null
    sleep 2
  done
  sleep "${MTX_SWITCH_GAP:-2}"   # how long the clip stays in each depth
done

# The harness checks its own input: how many switches actually happened is in the
# engine's log, and a clip that was asked for three and shows one must say so rather
# than look like a working run.
step
GOT=$(switches)
echo "switches asked: $SWITCHES, switches the engine logged: $GOT"
[ "$GOT" -lt "$SWITCHES" ] && echo "WARNING: fewer switches than asked for - a key release was lost"

# Let the last transported view run for a while, so the clip ends on a readable
# state rather than on the transition.
sleep 4
if [ -z "${MTX_NOCAP:-}" ]; then
  echo "stopping the DOSBox capture (Ctrl+F7)"
  xdotool key --clearmodifiers ctrl+F7 2>/dev/null
fi
sleep 2

kill $DP 2>/dev/null; wait $DP 2>/dev/null
kill $XP 2>/dev/null

# Convert what DOSBox wrote. AVI/ZMBV is lossless, so this is a re-encode for
# size and for a format everything can play - not a quality decision.
AVI=$(ls -t "$D"/cap/*.avi 2>/dev/null | head -1)
if [ -z "$AVI" ]; then
  echo "NO CAPTURE FILE: DOSBox's recorder did not start (is Ctrl+F7 reaching it?)"
else
  ffmpeg -loglevel error -y -i "$AVI" -c:v libx264 -preset veryfast -crf 20 \
         -pix_fmt yuv420p -c:a aac -b:a 128k "$D/$LABEL.mp4" || echo "CONVERT FAILED"
fi

echo "=== clip ==="
ffprobe -v error -show_entries format=duration,size -of csv=p=0 "$D/$LABEL.mp4" 2>/dev/null
echo "=== audio streams (the teleport sound) ==="
ffprobe -v error -select_streams a -show_entries stream=codec_name,channels,duration -of csv=p=0 "$D/$LABEL.mp4" 2>/dev/null
echo "=== engine ==="
echo "ILLEGAL: $(grep -c Illegal "$D/db.log" 2>/dev/null || echo 0)  DYNX86: $(grep -c DYNX86 "$D/db.log" 2>/dev/null || echo 0)"
grep -n "transport depth" "$D/MTX.LOG" 2>/dev/null
echo "=== per-depth levels ==="
grep -E "level UP|level: marker|screen texture" "$D/MTX.LOG" 2>/dev/null | head -20
echo "=== what the screen showed, per check ==="
grep -a "screen check" "$D/MTX.LOG" 2>/dev/null | tail -6

# Sample frames: a clip can be perfect by ffprobe and contain nothing.
DUR=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$D/$LABEL.mp4" 2>/dev/null | cut -d. -f1)
for t in $(seq 0 "${DUR:-0}"); do
  ffmpeg -loglevel error -y -ss "$t" -i "$D/$LABEL.mp4" -frames:v 1 "$D/f_$t.png" 2>/dev/null
done
echo "=== frames ==="; ls "$D"/f_*.png 2>/dev/null | wc -l
echo "clip: $D/$LABEL.mp4"
