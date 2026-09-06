#!/usr/bin/env python3
# plot_by_type.py
#
# What the MILP finds hard, by unit technology. The dumps carry no technology
# label, so the units are classified from their own data: the ratio pmin/pmax
# separates every type but the three mid CCGTs, which the minimum up time then
# splits. The classification is checked against the declared per-size mix, and
# the script refuses to plot if it does not come out exactly, since a silent
# misclassification would put the times under the wrong technology.
#
# The MILP is the only solver that is selectively hard: the dynamic programs
# pay the shape of the cost function, which every unit has, while the MILP pays
# the combinatorics of the commitment, which only the units flexible enough to
# have many feasible schedules, and constrained enough for the choice to
# matter, actually have. Bars at the time limit are hatched: those medians are
# lower bounds, the solver having been stopped rather than having finished.
#
#   usage: plot_by_type.py <dump_dir> <csv_dir> [out_dir=csv_dir] [arm=res_noreac]
import sys, os, csv, re, subprocess
from collections import defaultdict, Counter
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

dumps = sys.argv[1]
src = sys.argv[2]
out = sys.argv[3] if len(sys.argv) > 3 else src
arm = sys.argv[4] if len(sys.argv) > 4 else "res_noreac"

# order and mix as declared for the 50-unit instances
TYPES = ["oil peaker", "OCGT peaker", "CCGT small", "CCGT medium",
         "CCGT efficient", "CCGT large", "old coal"]
MIX = {"CCGT efficient": 6, "CCGT large": 8, "CCGT medium": 10,
       "CCGT small": 8, "OCGT peaker": 8, "old coal": 6, "oil peaker": 4}
HZ = [("day", 96), ("week", 672), ("month", 2976)]
LIMIT_US = 300e6
CENSORED = 0.99 * LIMIT_US
PERIODS_PER_HOUR = 4          # the instances are quarter-hourly at every horizon

def nc(f, var):
    o = subprocess.run(["ncdump", "-v", var, f], capture_output=True,
                       text=True).stdout
    return [float(x) for x in re.findall(r"-?\d+\.?\d*(?:e[+-]?\d+)?",
                                         o.split(f"{var} =")[1].split(";")[0])]

def classify(d):
    """unit -> technology, read from the dumps themselves"""
    one = {}
    for f in sorted(os.listdir(d)):
        m = re.match(r"TUB-(\d+)-(\d+)\.nc4$", f)
        if m and int(m.group(1)) not in one:
            one[int(m.group(1))] = os.path.join(d, f)
    lab = {}
    for u, f in one.items():
        phi = nc(f, "MinPower")[0] / nc(f, "MaxPower")[0]
        up = nc(f, "MinUpTime")[0] / PERIODS_PER_HOUR
        lab[u] = ("oil peaker" if phi < 0.225 else
                  "OCGT peaker" if phi < 0.33 else
                  "old coal" if phi < 0.45 else
                  "CCGT efficient" if phi < 0.525 else
                  "CCGT large" if up >= 6.5 else
                  "CCGT medium" if up >= 2.5 else "CCGT small")
    got = dict(Counter(lab.values()))
    if got != MIX:
        sys.exit(f"plot_by_type: the classification gives {got}, not {MIX}")
    print(f"units classified from {len(lab)} dumps, mix as declared")
    return lab

def milp_by_type(path, lab):
    """per technology: median warm MILP time in ms, and the share at the limit"""
    t = defaultdict(list)
    for r in csv.DictReader(open(path)):
        if r["solver"] == "MILP" and r["phase"] == "warm":
            t[lab[int(r["unit"])]].append(float(r["time_us"]))
    return ({k: np.median(v) / 1e3 for k, v in t.items()},
            {k: 100 * np.mean(np.array(v) >= CENSORED) for k, v in t.items()})

lab = classify(dumps)
fig, ax = plt.subplots(figsize=(11, 4.6))
w = 0.27
for j, (h, n) in enumerate(HZ):
    p = os.path.join(src, f"{h}_{arm}.csv")
    if not os.path.exists(p):
        continue
    med, cens = milp_by_type(p, lab)
    x = np.arange(len(TYPES)) + (j - 1) * w
    v = [med.get(k, np.nan) for k in TYPES]
    c = [cens.get(k, 0) for k in TYPES]
    ax.bar(x, v, w, label=f"$n = {n}$",
           hatch=["//" if s > 0 else "" for s in c], edgecolor="white")
    print(f"\n{h} (n={n}) <- {os.path.basename(p)}")
    for k, mv, cv in zip(TYPES, v, c):
        print(f"  {k:15s} {mv:10.1f} ms" + (f"   {cv:.0f}% al limite" if cv else ""))

ax.set_xticks(range(len(TYPES)))
ax.set_xticklabels(TYPES, rotation=20, ha="right")
ax.set_yscale("log")
ax.set_ylabel("median warm MILP time (ms)")
ax.set_title("What the MILP finds hard, by technology (hatched: stopped at the "
             "$300$ s limit)")
ax.grid(alpha=0.3, axis="y")
ax.legend()
fig.tight_layout()
dst = os.path.join(out, "milp_by_type.png")
fig.savefig(dst, dpi=150)
print(f"\n{dst}")
