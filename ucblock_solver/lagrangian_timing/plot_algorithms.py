#!/usr/bin/env python3
# plot_algorithms.py
#
# Algorithm-side view of the timing study (complementary to
# plot_lagrangian_timing.py, which is generator-type oriented). It pools the
# per-horizon timing_<h>_*.csv files in results/ and draws how the FOUR solvers
# compare AS ALGORITHMS -- not by thermal-unit type:
#
#   algo_scaling.png  log-log mean/median solve time vs horizon n, one line per
#                     solver: stdDP (base DP serial), parDP (base DP, FastFlow),
#                     extDP (run-length DP), MILP (Gurobi on T+P/C, integer).
#   algo_ratios.png   the two head-to-head ratios vs horizon: parDP/stdDP (the
#                     FastFlow parallel break-even) and MILP/extDP (the
#                     general-purpose Gurobi vs the dedicated run-length DP).
#
#   usage: plot_algorithms.py [results_dir=.] [out_dir=results_dir]
#
# The DP figures are pooled from the per-seed timing_<h>_*.csv files. The MILP at
# n=2976 is taken from month_milp_probe.csv (a single-wave re-measurement at that
# horizon, where one MILP solve is on the order of seconds).
import sys, os, csv, glob
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

res = sys.argv[1] if len(sys.argv) > 1 else "."
out = sys.argv[2] if len(sys.argv) > 2 else res

HORIZONS = [("day", 96), ("week", 672), ("month", 2976)]
SOLVERS = ["stdDP", "parDP", "extDP", "MILP"]
LABEL = {"stdDP": "ThermalUnitDPSolver (base, serial)",
         "parDP": "ThermalUnitDPSolver (base, FastFlow)",
         "extDP": "ThermalUnitExtDPSolver (run-length)",
         "MILP":  "MILPSolver  (Gurobi, T+P/C integer)"}
COLOR = {"stdDP": "tab:blue", "parDP": "tab:cyan",
         "extDP": "tab:green", "MILP": "tab:red"}
MARK = {"stdDP": "o", "parDP": "s", "extDP": "^", "MILP": "D"}
# month (n=2976) MILP is read from a dedicated re-measurement, not the per-seed
# pool; the DP columns there are pooled as usual
MILP_FROM_PROBE_N = {2976}

def summarise(vals):
    v = np.array(vals)
    return (np.median(v), np.percentile(v, 25), np.percentile(v, 75)) if v.size else None

# stats[solver][n] = (median_us, p25, p75)
stats = {s: {} for s in SOLVERS}
for h, n in HORIZONS:
    vals = {s: [] for s in SOLVERS}
    for f in glob.glob(os.path.join(res, f"timing_{h}_*.csv")):
        with open(f) as fh:
            for r in csv.DictReader(fh):
                if r["solver"] in vals:
                    vals[r["solver"]].append(float(r["time_us"]))
    for s in SOLVERS:
        if s == "MILP" and n in MILP_FROM_PROBE_N:
            continue
        st_ = summarise(vals[s])
        if st_:
            stats[s][n] = st_

probe = os.path.join(res, "month_milp_probe.csv")
if os.path.exists(probe):
    mvals = [float(r["time_us"]) for r in csv.DictReader(open(probe))
             if r["solver"] == "MILP"]
    st_ = summarise(mvals)
    if st_:
        stats["MILP"][2976] = st_

ns = [n for _, n in HORIZONS]

# --- Figure A: log-log scaling, one line per solver ------------------------- #
fig, ax = plt.subplots(figsize=(8, 5.5))
for s in SOLVERS:
    xs = [n for n in ns if n in stats[s]]
    if not xs:
        continue
    med = np.array([stats[s][n][0] / 1000.0 for n in xs])   # ms
    lo = np.array([stats[s][n][1] / 1000.0 for n in xs])
    hi = np.array([stats[s][n][2] / 1000.0 for n in xs])
    ax.plot(xs, med, MARK[s] + "-", color=COLOR[s], lw=2, ms=7, label=LABEL[s])
    ax.fill_between(xs, lo, hi, color=COLOR[s], alpha=0.15)
ax.set_xscale("log"); ax.set_yscale("log")
ax.set_xticks(ns); ax.set_xticklabels([f"{n}\n({h})" for h, n in HORIZONS])
ax.set_xlabel("time horizon  n  (periods)")
ax.set_ylabel("single-unit solve time  (ms, median; band = IQR)")
ax.set_title("Single-unit solve time vs horizon, by algorithm\n"
             "base DP wins only at n=96; run-length DP overtakes from n=672")
ax.grid(True, which="both", alpha=0.3)
ax.legend(fontsize=8, loc="upper left")
fig.tight_layout(); fig.savefig(os.path.join(out, "algo_scaling.png"), dpi=130)
print("wrote", os.path.join(out, "algo_scaling.png"))

# --- Figure B: the two head-to-head ratios vs horizon ----------------------- #
def ratio(num, den):
    xs, ys = [], []
    for n in ns:
        if n in stats[num] and n in stats[den]:
            xs.append(n); ys.append(stats[num][n][0] / stats[den][n][0])
    return xs, ys

fig, (a1, a2) = plt.subplots(1, 2, figsize=(12, 5))

x, y = ratio("parDP", "stdDP")
a1.plot(x, y, "s-", color="tab:cyan", lw=2, ms=8)
for xi, yi in zip(x, y):
    a1.annotate(f"{yi:.2f}×", (xi, yi), textcoords="offset points",
                xytext=(0, 8), ha="center", fontsize=9)
a1.axhline(1.0, color="k", ls="--", lw=1, alpha=0.7)
a1.fill_between(ns, 1.0, a1.get_ylim()[1], color="tab:red", alpha=0.05)
a1.set_xscale("log"); a1.set_xticks(ns)
a1.set_xticklabels([f"{n}\n({h})" for h, n in HORIZONS])
a1.set_ylabel("parDP / stdDP  (median)")
a1.set_title("FastFlow parallel break-even\n(>1 = parallel slower; <1 = faster)")
a1.grid(True, which="both", alpha=0.3)

x, y = ratio("MILP", "extDP")
a2.plot(x, y, "D-", color="tab:red", lw=2, ms=8)
for xi, yi in zip(x, y):
    a2.annotate(f"{yi:.0f}×", (xi, yi), textcoords="offset points",
                xytext=(0, 8), ha="center", fontsize=9)
a2.set_xscale("log"); a2.set_yscale("log"); a2.set_xticks(ns)
a2.set_xticklabels([f"{n}\n({h})" for h, n in HORIZONS])
a2.set_ylabel("MILP / extDP  (median, log)")
a2.set_title("Gurobi (T+P/C) vs run-length DP\n"
             "(extDP faster by 2+ orders of magnitude at every horizon)")
a2.grid(True, which="both", alpha=0.3)

fig.tight_layout(); fig.savefig(os.path.join(out, "algo_ratios.png"), dpi=130)
print("wrote", os.path.join(out, "algo_ratios.png"))

# numeric summary
print("\nhorizon  " + "  ".join(f"{s:>10}" for s in SOLVERS))
for h, n in HORIZONS:
    row = [f"{stats[s][n][0]/1000:10.3f}" if n in stats[s] else f"{'--':>10}"
           for s in SOLVERS]
    print(f"{h:7} " + "  ".join(row) + "  (ms, median)")
