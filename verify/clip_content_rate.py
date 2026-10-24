#!/usr/bin/env python3
# clip_content_rate.py <clip.mp4 | capture.avi>
#
# WHY THIS EXISTS. "The video's framerate is low" is two different claims that look
# identical in a player, and they need opposite fixes:
#   (a) the file has few frames                                    -> a capture problem
#   (b) the file has plenty of frames that REPEAT the same picture -> the game is
#       updating the screen slowly, and every faithful recording of it looks choppy
#       however it is encoded
# ffprobe answers (a) and CANNOT answer (b): a container's frame rate is a stamp, not a
# measurement (DOSBox stamps the emulated mode's refresh - 70 Hz for VGA mode 13h -
# whatever the game's real rate is). This counts how often the PICTURE changes, which
# is the number a viewer perceives.
#
# TWO NUMBERS, ON PURPOSE, and the difference is the whole point of the tool:
#   * EXACT    - frames differing in ANY sampled byte. Valid only for a LOSSLESS
#                source: the AVI DOSBox writes is lossless ZMBV, so an unchanged
#                picture is bit-identical and this is the truth.
#   * TOLERANT - frames whose mean absolute difference is above TOLERANCE. This is the
#                one to read on an H.264/mp4 re-encode, where compression noise makes
#                every "identical" frame differ in a few low bits - measured: the same
#                clip scores 8.47/s exact on the AVI and 28.45/s exact on its mp4, so
#                quoting the exact number off a lossy file is a false measurement.
# Always measure the AVI when the question is "how fast does the game update".
import os, subprocess, sys, collections

# A change is judged by AMPLITUDE, not by a mean: a mean over the whole frame hides a
# real change confined to a small part of it (measured: the status-bar digits moving
# give a mean of a few hundredths of a grey level, and a mean-based threshold called
# that "no change" and scored the lossless AVI at 2.77/s against its true 8.47/s).
# Compression noise is small-amplitude and sparse; a rendered change is not.
BIG = 8                  # per-pixel difference (grey levels) that is a real change
PCT_PERMILLE = 2         # ... in at least this many per mille of sampled pixels
W, H = 320, 200          # native mode 13h
STRIDE = 7               # subsample pixels: enough to catch any real change, cheap

def main():
    src = sys.argv[1] if len(sys.argv) > 1 else \
        os.path.join(os.environ.get("MTX_ROOT", os.path.expanduser("~/Documents/recursive_doom")), "verify/rec-ship-final/cap/video0001.avi")
    p = subprocess.run(["ffmpeg", "-v", "error", "-i", src,
                        "-pix_fmt", "gray", "-f", "rawvideo", "-"], capture_output=True)
    buf = p.stdout
    fsz = W * H
    n = len(buf) // fsz
    if n < 2:
        print("no frames decoded from %s" % src)
        return 1
    print("source: %s" % src)
    print("decoded frames: %d" % n)

    prev = None
    runs_exact, runs_tol = [], []
    cur_e = cur_t = 1
    changes_exact = changes_tol = 0
    for i in range(n):
        f = buf[i*fsz:(i+1)*fsz:STRIDE]
        if prev is None:
            prev = f
            continue
        ndiff = 0
        big = 0
        for a, b in zip(f, prev):
            d = a - b
            if d:
                ndiff += 1
                if d < 0:
                    d = -d
                if d > BIG:
                    big += 1
        if ndiff == 0:
            cur_e += 1
        else:
            runs_exact.append(cur_e); cur_e = 1; changes_exact += 1
        if big * 1000 < len(f) * PCT_PERMILLE:
            cur_t += 1
        else:
            runs_tol.append(cur_t); cur_t = 1; changes_tol += 1
        prev = f
    runs_exact.append(cur_e); runs_tol.append(cur_t)

    fps = 2190197 / 31250.0      # the emulated mode's refresh, as DOSBox stamps it
    dur = n / fps
    print("container stamp: %.2f fps -> file duration %.2f s" % (fps, dur))
    print()
    print("EXACT    (lossless source only): %4d distinct pictures, %6.2f pictures/s, "
          "mean run %.2f" % (changes_exact, changes_exact / dur,
                             sum(runs_exact) / len(runs_exact)))
    print("TOLERANT (>%d/%d px differ by >%d): %4d distinct pictures, %6.2f pictures/s, "
          "mean run %.2f" % (PCT_PERMILLE, 1000, BIG, changes_tol, changes_tol / dur,
                             sum(runs_tol) / len(runs_tol)))
    if changes_exact > changes_tol * 1.5:
        print()
        print("NOTE: the two disagree by more than half, which means this source is")
        print("      LOSSY (an mp4 re-encode): the exact count is measuring compression")
        print("      noise, not the picture. Read the tolerant number, or measure the")
        print("      AVI DOSBox wrote.")
    print()
    print("identical-run histogram, tolerant (run:count):")
    for k, v in sorted(collections.Counter(runs_tol).items()):
        print("   %3d : %5d" % (k, v))
    return 0

if __name__ == "__main__":
    sys.exit(main())
