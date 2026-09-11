#!/usr/bin/env python3
# plot_dual_range.py
#
# The companion of plot_early.py over the WHOLE dual rather than its first
# iterations: the long productions dump every 100 or 200 iterations up to the
# end of the run, so the same warm re-solve can be followed to multipliers that
# are nowhere near zero. Two figures:
#
#   dense_week_reac.png : median warm time at the weekly horizon, +r+q, along
#                         the whole dual, in milliseconds, so that the four
#                         solvers are read against each other and not only
#                         against their own first sample;
#   ratios_vs_extdp.png : how many times slower than the run-length solver the
#                         other three are, at the weekly and monthly horizons,
#                         along the same range. The MILP curve is the argument
#                         of the paper: the gap does not settle, it widens with
#                         the dual.
#
#   usage: plot_dual_range.py [csv_dir=.] [out_dir=csv_dir]
import sys, os, csv
from collections import defaultdict
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

src = sys.argv[1] if len(sys.argv) > 1 else "."
out = sys.argv[2] if len(sys.argv) > 2 else src

SOL = ["stdDP", "parDP", "extDP", "MILP"]
COL = {"stdDP": "tab:blue", "parDP": "tab:green", "extDP": "tab:red",
       "MILP": "tab:gray"}

def medians(path):
    """median warm time per solver over the units, in ms, and the iterations"""
    t = defaultdict(lambda: defaultdict(list))
    for r in csv.DictReader(open(path)):
        if r["phase"] == "warm":
            t[r["solver"]][int(r["iter"])].append(float(r["time_us"]) / 1e3)
    return {s: (sorted(v), np.array([np.median(v[i]) for i in sorted(v)]))
            for s, v in t.items()}

# the weekly +r+q set, densely sampled over the whole dual
path = os.path.join(src, "week_res_reac.csv")
m = medians(path)
fig, ax = plt.subplots(figsize=(7, 4.4))
for s in SOL:
    if s in m:
        its, med = m[s]
        ax.plot(its, med, "o-", ms=4, lw=1.3, color=COL[s], label=s)
        print(f"week +r+q {s:6s} {med[0]:9.1f} -> {med[-1]:9.1f} ms  "
              f"({med[-1] / med[0]:5.2f}x over iterations {its[0]}-{its[-1]})")
ax.set_yscale("log")
ax.set_xlabel("Lagrangian iteration")
ax.set_ylabel("median warm re-solve time (ms)")
ax.set_title("$n = 672$, +r+q, along the whole dual")
ax.grid(alpha=0.3)
ax.legend()
fig.tight_layout()
fig.savefig(os.path.join(out, "dense_week_reac.png"), dpi=150)

# how far the other three sit from the run-length solver, both horizons
fig, axes = plt.subplots(1, 2, figsize=(11, 4.4))
fig.suptitle("Times the run-length solver, along the dual")
for ax, (h, n) in zip(axes, [("week", 672), ("month", 2976)]):
    m = medians(os.path.join(src, f"{h}_res_noreac.csv"))
    base = dict(zip(*m["extDP"]))
    print(f"\n{h} (n={n}), +r")
    for s in SOL:
        if s == "extDP" or s not in m:
            continue
        its, med = m[s]
        r = np.array([t / base[i] for i, t in zip(its, med)])
        ax.plot(its, r, "o-", ms=4, lw=1.3, color=COL[s], label=s)
        print(f"  {s:6s} {r[0]:8.1f}x -> {r[-1]:8.1f}x")
    ax.axhline(1.0, ls="--", lw=1, color="black")
    ax.set_yscale("log")
    ax.set_title(f"$n = {n}$, +r")
    ax.set_xlabel("Lagrangian iteration")
    ax.grid(alpha=0.3)
axes[0].set_ylabel("median time / extDP median time")
axes[0].legend()
fig.tight_layout()
fig.savefig(os.path.join(out, "ratios_vs_extdp.png"), dpi=150)
print(f"\n{out}/dense_week_reac.png\n{out}/ratios_vs_extdp.png")
