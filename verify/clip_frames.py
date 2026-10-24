#!/usr/bin/env python3
"""Numeric discriminator over a recorded clip's sampled frames.

Metadata says the file exists; only the pixels say what is in it. This measures the
INSET region (the box the engine composites into, 8,8..168,108 in the depth's
320x200 space, scaled to the captured window) on every sampled frame:

  - its mean luma, so a state change shows up as a level change;
  - the mean absolute change against the previous sampled frame, so the frame where
    the switch happens stands out from the steady states either side of it.

Calibration: the engine's own log gives the ground truth for the same run
(1973/16000 pixels matching the depth's frame with the inset OFF, 15911..16000/16000
with it ON), so a level/step change here has something to agree with. Deliberately
stdlib-only: no numpy in this environment.
"""
import glob
import os
import subprocess
import sys

CLIP_DIR = sys.argv[1] if len(sys.argv) > 1 else "."
# The emulator window is 758x569 for a 320x200 framebuffer -> scale the box.
WIN_W, WIN_H = 758, 569
SX, SY = WIN_W / 320.0, WIN_H / 200.0
CX, CY = int(12 * SX), int(12 * SY)
CW, CH = int(148 * SX), int(88 * SY)


def inset_mean_brightness(png):
    raw = subprocess.run(
        ["ffmpeg", "-loglevel", "error", "-i", png,
         "-vf", "crop=%d:%d:%d:%d,format=gray" % (CW, CH, CX, CY),
         "-f", "rawvideo", "-"],
        capture_output=True, check=True).stdout
    if not raw:
        return None
    return sum(raw) / len(raw)


def main():
    fs = sorted(glob.glob(os.path.join(CLIP_DIR, "f_*.png")),
                key=lambda p: int(os.path.basename(p)[2:-4]))
    if not fs:
        print("no sampled frames in", CLIP_DIR)
        return
    print("inset box: %dx%d at %d,%d  (%d frames)" % (CW, CH, CX, CY, len(fs)))
    prev = None
    step = 0.0
    step_at = None
    for p in fs:
        t = int(os.path.basename(p)[2:-4])
        m = inset_mean_brightness(p)
        if m is None:
            print("t=%2d  (unreadable)" % t)
            continue
        if prev is None:
            print("t=%2d  mean %6.2f" % (t, m))
        else:
            d = abs(m - prev)
            print("t=%2d  mean %6.2f   change %6.2f" % (t, m, d))
            if d > step:
                step, step_at = d, t
        prev = m
    if step_at is not None:
        print("\nlargest change: %.2f at t=%ds (the switch)" % (step, step_at))


main()
