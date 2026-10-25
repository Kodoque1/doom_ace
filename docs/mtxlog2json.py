#!/usr/bin/env python3
"""Turn one MTX.LOG into the JSON the article's measured diagrams are drawn from.

    python3 mtxlog2json.py path/to/MTX.LOG > mtx-run.json

Only lines the host and the instances actually wrote are used. Nothing is
estimated here: a field the log does not carry is left out, and the page says
so instead of drawing it.
"""
import json
import re
import sys


def ints(s):
    return [int(x) for x in re.findall(r"-?\d+", s)]


def main(path):
    out = {"source": path.split("/")[-2] + "/MTX.LOG", "depths": {}, "regions": [],
           "schedule": [], "copyless": [], "suspended": None}
    d = out["depths"]

    def dep(n):
        return d.setdefault(str(n), {})

    # I_GetTime (35 Hz tics) at the root's frame lines: the start of each slice,
    # so the gap to the next slice's start is how long that slice took.
    last_t = None
    for raw in open(path, errors="replace"):
        line = raw.strip()
        if not line.startswith("[MTX]"):
            continue
        t = line[6:]
        m = re.match(r"host up: depths (\d+) snap (\d+) img (\d+)", t)
        if m:
            out["depth_count"], out["snapshot_bytes"], out["image_bytes"] = map(int, m.groups())
            continue
        m = re.match(r"stack pool (\d+) B at (\d+), (\d+) B per depth", t)
        if m:
            total, base, per = map(int, m.groups())
            out["stack_pool"] = {"base": base, "bytes": total, "per_depth": per}
            # The slots are carved in order from the pool's base; only depth 1's is
            # logged on its own line, so the others are derived from this one.
            for k in range(total // per):
                dep(k).setdefault("stack", [base + k * per, base + (k + 1) * per])
                dep(k)["stack_derived"] = True
            continue
        m = re.match(r"raw pool (\d+) B/depth \(child 1 at (\d+)\)", t)
        if m:
            size, at = map(int, m.groups())
            dep(1)["raw"] = [at, at + size]
            continue
        m = re.match(r"isolation: (\d+) regions, (\d+) patches, snapshot (\d+) B", t)
        if m:
            out["region_count"], out["patches"], out["snapshot_bytes"] = map(int, m.groups())
            continue
        m = re.match(r"bss \[(\d+), (\d+)\) size (\d+) \| mtx_hs \[(\d+), (\d+)\) size (\d+)", t)
        if m:
            b0, b1, _, h0, h1, hs = map(int, m.groups())
            out["bss"] = [b0, b1]
            out["mtx_hs"] = [h0, h1]
            continue
        m = re.match(r"region (\d+): \[(\d+), (\d+)\) size (\d+) \| esp (\d+)", t)
        if m:
            k, lo, hi, size, esp = map(int, m.groups())
            out["regions"].append({"index": k, "lo": lo, "hi": hi, "size": size})
            out["root_esp_at_boot"] = esp
            continue
        m = re.match(r"root: ace_seg (\d+) code_seg (\d+) data_seg (\d+)", t)
        if m:
            out["ace_seg"], out["code_seg"], out["data_seg"] = map(int, m.groups())
            continue
        m = re.match(r"probe site (\d+), byte there now (\d+)", t)
        if m:
            out["probe"] = {"addr": int(m.group(1)), "byte": int(m.group(2))}
            continue
        m = re.match(r"hook isolation: depth1 wrote (\d+) at (\d+) \(read back (\d+)\); root now (\d+), was (\d+)", t)
        if m:
            w, a, rb, now, was = map(int, m.groups())
            out["probe"] = {"addr": a, "byte": was, "depth1_wrote": w, "depth1_read": rb, "root_after": now}
            continue
        m = re.match(r"doom screens\[0\] at (\d+) = (\d+)", t)
        if m:
            out["doom_screens0_var"] = int(m.group(1))
            dep(0)["screen"] = int(m.group(2))
            continue
        m = re.match(r"depth(\d+) stack slot \[(\d+), (\d+)\)", t)
        if m:
            n, lo, hi = map(int, m.groups())
            dep(n)["stack"] = [lo, hi]
            dep(n)["stack_derived"] = False
            continue
        m = re.match(r"depth(\d+): engine image (\d+) B at (\d+) entry (\d+)", t)
        if m:
            n, size, at, entry = map(int, m.groups())
            dep(n)["image"] = [at, at + size]
            dep(n)["entry"] = entry
            continue
        m = re.match(r"depth(\d+) params at (\d+): depth \d+ zone (\d+) screen (\d+) raw (\d+)", t)
        if m:
            n, p, z, s, r = map(int, m.groups())
            dep(n).update({"params": p, "screen": s, "raw_pool_ctl": r})
            dep(n).setdefault("zone", [z, None])
            continue
        m = re.match(r"depth(\d+): entering, ECX=(\d+) ESI=(\d+) EDI=(\d+) EBP=(\d+)", t)
        if m:
            n, ecx, esi, edi, ebp = map(int, m.groups())
            dep(n)["handoff"] = {"ECX": ecx, "ESI": esi, "EDI": edi, "EBP": ebp}
            continue
        m = re.match(r"d(\d+) entered on stack (\d+)", t)
        if m:
            dep(int(m.group(1)))["seeded_esp"] = int(m.group(2))
            continue
        m = re.match(r"depth(\d+) zone (\d+) B at (\d+) build (\d+) alloc (\d+) probe (\d+)", t)
        if m:
            n, size, at, build, alloc, probe = map(int, m.groups())
            dep(n)["zone"] = [at, at + size]
            dep(n)["zone_check"] = {"build": build, "first_alloc": alloc, "probe": probe}
            continue
        m = re.match(r"depth stack \(ctx\[(\d+)\]\.esp\) from the root at (\d+): (.*)", t)
        if m and out["suspended"] is None:
            words = [int(w, 16) for w in m.group(3).split()]
            out["suspended"] = {"depth": int(m.group(1)), "esp": int(m.group(2)), "words": words}
            continue
        m = re.match(r"instance live esp at (\d+):", t)
        if m:
            esp = int(m.group(1))
            for n, v in d.items():
                if "stack" in v and v["stack"][0] <= esp < v["stack"][1]:
                    v.setdefault("live_esp", esp)
                    if out["schedule"] and out["schedule"][-1].get("pending"):
                        out["schedule"][-1] = {"depth": int(n), "frames": 4, "t": out["schedule"][-1]["t"]}
            continue
        m = re.match(r"instance resumed, frame (\d+)", t)
        if m:
            out["schedule"].append({"pending": True, "t": last_t})
            continue
        m = re.match(r"frame (\d+) child=\d+ t=(\d+)", t)
        if m:
            last_t = int(m.group(2))
            s = out["schedule"]
            if s and s[-1].get("depth") == 0:
                s[-1]["frames"] += 1
            else:
                s.append({"depth": 0, "frames": 1, "t": last_t})
            continue
        m = re.match(r"copyless: last save copied (\d+) B, last load (\d+) B, of a (\d+) B snapshot", t)
        if m:
            sv, ld, tot = map(int, m.groups())
            out["copyless"].append({"save": sv, "load": ld, "of": tot})
            continue
    out["schedule"] = [s for s in out["schedule"] if "depth" in s]
    sch = out["schedule"]
    for i, s in enumerate(sch):
        if i + 1 < len(sch) and s.get("t") is not None and sch[i + 1].get("t") is not None:
            s["tics"] = sch[i + 1]["t"] - s["t"]
    json.dump(out, sys.stdout, indent=1)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main(sys.argv[1])
