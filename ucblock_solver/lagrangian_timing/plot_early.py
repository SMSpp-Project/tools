#!/usr/bin/env python3
# plot_early.py
#
# The early-iteration experiment: how the warm single-unit re-solve time moves
# along the first few hundred iterations of the Lagrangian dual, one panel per
# horizon, every curve divided by its own first sample so that the four solvers
# are compared on how they DEGRADE and not on how fast they are. Reads the CSVs
# written by run-timing-guarded over a dump set produced with -DSAVE_TUB=1,
# reserves priced. The dynamic programs are flat, the MILP is not: the multi-
# pliers move away from zero, the single-unit problem stops being trivial for a
# general-purpose solver, and a decomposition pays that at every iteration.
#
# Also prints the figures the caption quotes: the last/first ratio of each
# solver and the share of MILP solves that stop at the time limit, at the two
# ends of the span.
#
# The arm is the one the paper calls +r, the instances with the spinning
# reserves priced and no reactive power (res_noreac); pass res_reac for the
# +r+q companion.
#
#   usage: plot_early.py [csv_dir=.] [out_dir=csv_dir] [arm=res_noreac]
import sys, os, csv
from collections import defaultdict
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

src = sys.argv[1] if len(sys.argv) > 1 else "."
out = sys.argv[2] if len(sys.argv) > 2 else src
arm = sys.argv[3] if len(sys.argv) > 3 else "res_noreac"
TAG = {"res_noreac": "+r", "res_reac": "+r+q"}[arm]

HZ = [("day", 96), ("week", 672), ("month", 2976)]
SOL = ["stdDP", "parDP", "extDP", "MILP"]
COL = {"stdDP": "tab:blue", "parDP": "tab:green", "extDP": "tab:red",
       "MILP": "tab:gray"}
LIMIT_US = 300e6      # the MILP time limit; a solve within 1% of it is censored
CENSORED = 0.99 * LIMIT_US

# The set is measured over a day and the guard discards a run the machine
# spoiled, so prefer the promoted CSV and fall back to whatever partial or
# quarantined file is there, saying which one is being read.
def pick(h):
    for suffix in ("", ".try2-underload", ".SNAPSHOT"):
        p = os.path.join(src, f"{h}_{arm}{suffix}.csv")
        if os.path.exists(p) and os.path.getsize(p) > 1000:
            return p
    return None

def read(path):
    """warm times per (solver, iteration), in microseconds"""
    t = defaultdict(lambda: defaultdict(list))
    with open(path) as f:
        for r in csv.DictReader(f):
            if r["phase"] != "warm":
                continue
            t[r["solver"]][int(r["iter"])].append(float(r["time_us"]))
    return t

fig, axes = plt.subplots(1, 3, figsize=(15, 4.2))
fig.suptitle("The first few hundred dual iterations: only the MILP gets harder")

for ax, (h, n) in zip(axes, HZ):
    path = pick(h)
    if not path:
        ax.set_title(f"$n = {n}$, {TAG} (no data)")
        continue
    t = read(path)
    print(f"\n{h} (n={n}) <- {os.path.basename(path)}")
    for s in SOL:
        if s not in t:
            continue
        its = sorted(t[s])
        med = np.array([np.median(t[s][i]) for i in its])
        ax.plot(its, med / med[0], "o-", ms=3.5, lw=1.2, color=COL[s], label=s)
        print(f"  {s:6s} {len(its):3d} points, {its[0]}->{its[-1]}, "
              f"last/first {med[-1] / med[0]:5.2f}, max/first "
              f"{med.max() / med[0]:5.2f}")
        if s == "MILP":
            for i in (its[0], its[-1]):
                v = np.array(t[s][i])
                print(f"    at the limit, iteration {i:3d}: "
                      f"{100 * (v >= CENSORED).mean():4.0f}%")
    ax.axhline(1.0, ls="--", lw=1, color="black")
    ax.set_yscale("log")
    ax.set_title(f"$n = {n}$, {TAG}")
    ax.set_xlabel("Lagrangian iteration")
    ax.grid(alpha=0.3)

axes[0].set_ylabel("time, relative to the first sample")
axes[0].legend()
fig.tight_layout()
dst = os.path.join(out, "early_iterations.png")
fig.savefig(dst, dpi=150)
print(f"\n{dst}")
