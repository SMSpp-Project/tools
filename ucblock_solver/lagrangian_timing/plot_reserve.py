#!/usr/bin/env python3
# plot_reserve.py
#
# The reserve experiment: per-iteration single-unit solve time against the
# horizon, with vs without spinning reserves, for the four solvers. Reads the
# config-structured results/ (noreserve/ and reserve/ subdirs) written by
# run-lagrangian-timing. Pricing reserves is cheap for the run-length DP but
# devastating for the base DP, which is the point of the figure.
#
#   usage: plot_reserve.py [results_dir=.] [out_dir=results_dir]
import sys, os, csv, glob
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

res = sys.argv[1] if len(sys.argv) > 1 else "."
out = sys.argv[2] if len(sys.argv) > 2 else res
HZ = [("day", 96), ("week", 672), ("month", 2976)]
SOL = ["stdDP", "parDP", "extDP", "MILP"]
LAB = {"stdDP": "base DP, serial", "parDP": "base DP, FastFlow",
       "extDP": "run-length DP", "MILP": "MILP (Gurobi, T+P/C)"}
COL = {"stdDP": "tab:blue", "parDP": "tab:cyan", "extDP": "tab:green", "MILP": "tab:red"}

def median_ms(cfg, h):
    """median per-solver time (ms) for config cfg at horizon h; uses the warm
    rows, falling back to cold when a horizon has no warm row (the n=2976
    with-reserve arm, whose Lagrangian dual was too slow to dump a second
    wave)."""
    vals = {s: {"warm": [], "cold": []} for s in SOL}
    for f in glob.glob(os.path.join(res, cfg, f"timing_{h}_*.csv")):
        for r in csv.DictReader(open(f)):
            if r["solver"] in vals:
                vals[r["solver"]][r.get("phase", "warm")].append(float(r["time_us"]))
    out = {}
    for s in SOL:
        w, c = vals[s]["warm"], vals[s]["cold"]
        v = w if w else c
        if v:
            out[s] = np.median(v) / 1000.0
    return out

ns = [n for _, n in HZ]
fig, ax = plt.subplots(figsize=(8.2, 5.6))
for s in SOL:
    nr = [median_ms("noreserve", h).get(s) for h, _ in HZ]
    wr = [median_ms("reserve", h).get(s) for h, _ in HZ]
    xn = [n for n, y in zip(ns, nr) if y]
    yn = [y for y in nr if y]
    xw = [n for n, y in zip(ns, wr) if y]
    yw = [y for y in wr if y]
    ax.plot(xn, yn, "o-", color=COL[s], lw=2, label=f"{LAB[s]}  (no reserves)")
    ax.plot(xw, yw, "s--", color=COL[s], lw=2, alpha=0.8,
            label=f"{LAB[s]}  (with reserves)")
ax.set_xscale("log"); ax.set_yscale("log")
ax.set_xticks(ns); ax.set_xticklabels([f"{n}\n({h})" for h, n in HZ])
ax.set_xlabel("time horizon  n  (periods)")
ax.set_ylabel("single-unit solve time  (ms, median)")
ax.set_title("Solve time vs horizon, with and without spinning reserves\n"
             "pricing reserves barely touches the run-length DP, "
             "but devastates the base DP")
ax.grid(True, which="both", alpha=0.3)
ax.legend(fontsize=7, ncol=2)
fig.tight_layout()
fig.savefig(os.path.join(out, "reserve_scaling.png"), dpi=130)
print("wrote", os.path.join(out, "reserve_scaling.png"))

# numeric summary
print(f"\n{'solver':6} " + "  ".join(f"{h+'(n='+str(n)+')':>18}" for h, n in HZ))
for s in SOL:
    cells = []
    for h, _ in HZ:
        a = median_ms("noreserve", h).get(s)
        b = median_ms("reserve", h).get(s)
        cells.append(f"{a:7.2f}->{b:7.2f}" if a and b else f"{'--':>16}")
    print(f"{s:6} " + "  ".join(f"{c:>18}" for c in cells))
