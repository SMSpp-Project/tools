#!/usr/bin/env python3
# plot_lagrangian_timing.py
#
# Plot the per-iteration single-unit solve time, as a function of the
# (normalized) Lagrangian iteration, for the FOUR solvers timed by
# timing_harness (stdDP = ThermalUnitDPSolver serial, parDP = the same with all
# cores as FastFlow workers, extDP = ThermalUnitExtDPSolver, MILP = a
# :MILPSolver on the T+P/C formulation), grouped by generator type and
# pooling several instances. Driven by run-lagrangian-timing.
#
#   usage: plot_lagrangian_timing.py <out_prefix> <timing1.csv> <gentype1.csv>
#                                                  [<timing2.csv> <gentype2.csv> ...]
#
#   timing*.csv : "unit,iter,solver,time_us,phase"  (from timing_harness; only
#                 the per-iteration "warm" re-solves are plotted, the one-off
#                 "cold" first solve per unit is dropped)
#   gentype*.csv: "unit,gen_type"             (from the source JSON instance)
#
# Different instances run a different number of Lagrangian iterations, so the
# iteration index is normalized to [0,1] per instance before pooling.
#
# Produces:
#   <out_prefix>_vs_iter.png    one panel per solver (own y-scale): mean +/-1s
#                               band across units, per gen_type, vs norm. iter.
#   <out_prefix>_normalized.png THE question plot: each unit's series divided by
#                               its own mean, pooled across all units -> shows
#                               whether the time varies ALONG the iteration,
#                               independently of the (very different) scales.
#   <out_prefix>_bytype.png     boxplot of time by gen_type, one panel per solver.
import sys, csv
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

out_prefix = sys.argv[1]
pairs = sys.argv[2:]
assert len(pairs) % 2 == 0 and pairs, "need <out_prefix> then (timing,gentype) pairs"

# rows: (instance, unit, gen_type, solver, x_normalized_iter, time_us)
rows = []
for k in range(0, len(pairs), 2):
    tcsv, gcsv, inst = pairs[k], pairs[k + 1], k // 2
    g = {}
    with open(gcsv) as f:
        for r in csv.DictReader(f):
            g[int(r["unit"])] = r["gen_type"]
    # iteration normalization is per instance, computed over ALL solvers/units.
    # only the "warm" rows are kept: those are the per-Lagrangian-iteration
    # re-solve times the dual actually pays (the single "cold" row per unit is
    # the one-off model build + first solve, not a cost incurred along the
    # iterations). Files without a "phase" column are treated as all-warm.
    recs = []
    with open(tcsv) as f:
        for r in csv.DictReader(f):
            if r.get("phase", "warm") != "warm":
                continue
            u = int(r["unit"])
            if u in g:
                recs.append((u, int(r["iter"]), r["solver"], float(r["time_us"])))
    if not recs:
        continue
    imax = max(it for _, it, _, _ in recs)
    for u, it, slv, t in recs:
        xn = it / imax if imax > 0 else 0.0
        rows.append((inst, u, g[u], slv, xn, t))

inst = np.array([r[0] for r in rows]); unit = np.array([r[1] for r in rows])
gt = np.array([r[2] for r in rows]); solver = np.array([r[3] for r in rows])
x = np.array([r[4] for r in rows]); T = np.array([r[5] for r in rows])
n_inst = len(set(inst.tolist()))

# solvers in a fixed, meaningful order (only those actually present)
SLV_ORDER = ["stdDP", "parDP", "extDP", "MILP"]
SLV_LABEL = {"stdDP": "ThermalUnitDPSolver",
             "parDP": "ThermalUnitDPSolver (parallel)",
             "extDP": "ThermalUnitExtDPSolver",
             "MILP": "MILPSolver (T+P/C, integer)"}
solvers = [s for s in SLV_ORDER if s in set(solver.tolist())]

nbins = 30
edges = np.linspace(0, 1, nbins + 1); centers = 0.5 * (edges[:-1] + edges[1:])

def _bin_iter(xb):
    """yield (bin, boolean selector) over the normalized-iteration bins."""
    for b in range(nbins):
        hi = (xb <= edges[b + 1]) if b == nbins - 1 else (xb < edges[b + 1])
        yield b, (xb >= edges[b]) & hi

def band(mask, normalize=False, across="units"):
    """per-bin mean/std of time over the rows in `mask`. The series are first
    aggregated per (instance,unit); if normalize, each (instance,unit) series is
    divided by its own mean first (so the band measures variation ALONG the
    iteration regardless of absolute scale). The center/band are then taken:
      across="units" : mean/std ACROSS the per-unit values -> unit-to-unit
                       spread (dominated by single-timing noise once normalized);
      across="types" : per-unit values are first averaged within each gen_type,
                       then mean/std ACROSS the per-type means -> the INTER-TYPE
                       spread, with per-unit measurement noise averaged out."""
    m = np.full(nbins, np.nan); s = np.full(nbins, np.nan)
    xb, tb = x[mask], T[mask]
    key = inst[mask] * 100000 + unit[mask]
    gtb = gt[mask]
    ukey = {}                      # (instance,unit) -> its gen_type
    tb = tb.astype(float).copy()
    if normalize:
        for k in np.unique(key):
            sel = key == k
            mu = tb[sel].mean()
            if mu > 0:
                tb[sel] = tb[sel] / mu
    for b, sel in _bin_iter(xb):
        if sel.sum() == 0:
            continue
        kk, tt, gg = key[sel], tb[sel], gtb[sel]
        ks = np.unique(kk)
        per = np.array([tt[kk == k].mean() for k in ks])
        if across == "types":
            # gen_type of each unit-key, then average per type, std over types
            ktype = {k: gg[kk == k][0] for k in ks}
            byt = {}
            for k, v in zip(ks, per):
                byt.setdefault(ktype[k], []).append(v)
            per = np.array([np.mean(v) for v in byt.values()])
        m[b] = np.mean(per); s[b] = np.std(per)
    return m, s

def n_units(mask):
    return len(set(zip(inst[mask].tolist(), unit[mask].tolist())))

types = sorted(set(gt.tolist()))
cmap = plt.get_cmap("tab10")

# --- per-solver: time vs normalized iteration, per gen_type -----------------
ns = len(solvers)
fig, axes = plt.subplots(ns, 1, figsize=(11, 4.2 * ns), squeeze=False)
for si, slv in enumerate(solvers):
    ax = axes[si][0]
    for i, t in enumerate(types):
        mask = (gt == t) & (solver == slv)
        if mask.sum() == 0:
            continue
        m, s = band(mask); c = cmap(i % 10); ok = ~np.isnan(m)
        if not ok.any():
            continue
        ax.plot(centers[ok], m[ok], color=c, lw=2,
                label=f"{t} ({n_units(mask)}u)")
        ax.fill_between(centers[ok], (m - s)[ok], (m + s)[ok], color=c, alpha=0.18)
    ax.set_title(f"{SLV_LABEL[slv]}: solve time vs normalized Lagrangian "
                 f"iteration, by gen_type ({n_inst} instances)")
    ax.set_xlabel("normalized Lagrangian iteration [0,1]")
    ax.set_ylabel("solve time (us, median of reps)\nband = +/-1s across units")
    ax.legend(fontsize=7, ncol=2); ax.grid(alpha=0.3)
plt.tight_layout(); plt.savefig(out_prefix + "_vs_iter.png", dpi=110)
print("wrote", out_prefix + "_vs_iter.png")

# --- THE question plot: relative time (per-unit normalized) vs norm. iter ----
# band = INTER-TYPE spread (per-unit measurement noise is averaged out within
# each gen_type), so a tight band means no gen_type behaves differently along
# the iteration; a flat center means the time does not vary along the iteration.
fig, ax = plt.subplots(figsize=(11, 6.5))
for si, slv in enumerate(solvers):
    mask = solver == slv
    m, s = band(mask, normalize=True, across="types"); ok = ~np.isnan(m)
    if not ok.any():
        continue
    c = cmap(si % 10)
    spread = 100 * np.nanstd(m[ok])  # how much the mean line itself moves (%)
    ax.plot(centers[ok], m[ok], color=c, lw=2,
            label=f"{SLV_LABEL[slv]} (line moves {spread:.1f}%)")
    ax.fill_between(centers[ok], (m - s)[ok], (m + s)[ok], color=c, alpha=0.15)
ax.axhline(1.0, color="k", lw=0.8, ls="--", alpha=0.6)
ax.set_title(f"Relative solve time (each unit / its own mean) vs normalized "
             f"Lagrangian iteration ({n_inst} instances)")
ax.set_xlabel("normalized Lagrangian iteration [0,1]")
ax.set_ylabel("relative solve time (1.0 = unit's own mean)\n"
              "band = +/-1s across gen_types")
ax.legend(fontsize=8); ax.grid(alpha=0.3)
plt.tight_layout(); plt.savefig(out_prefix + "_normalized.png", dpi=110)
print("wrote", out_prefix + "_normalized.png")

# numeric summary per solver
for slv in solvers:
    mask = solver == slv
    m, s = band(mask); ok = ~np.isnan(m)
    mn, sn = band(mask, normalize=True); okn = ~np.isnan(mn)
    print(f"  [{slv}] {n_units(mask)} unit-instances, {mask.sum()} points; "
          f"mean {np.nanmean(m):.1f}us; "
          f"variation ALONG iter (abs) = {np.nanstd(m[ok]):.1f}us "
          f"({100*np.nanstd(m[ok])/np.nanmean(m):.1f}%); "
          f"(per-unit normalized line moves {100*np.nanstd(mn[okn]):.1f}%)")

# --- boxplot by gen_type, one panel per solver ------------------------------
fig, axes = plt.subplots(ns, 1, figsize=(11, 4.0 * ns), squeeze=False)
for si, slv in enumerate(solvers):
    ax = axes[si][0]
    by = {}
    for t, sv, tt in zip(gt, solver, T):
        if sv == slv:
            by.setdefault(t, []).append(tt)
    order = sorted(by, key=lambda t: np.median(by[t]))
    if not order:
        continue
    bp = ax.boxplot([by[t] for t in order], tick_labels=order, showfliers=False,
                    patch_artist=True)
    for i, b in enumerate(bp["boxes"]):
        b.set_facecolor(cmap(i % 10)); b.set_alpha(0.5)
    ax.set_title(f"{SLV_LABEL[slv]}: solve time by gen_type "
                 f"({n_inst} instances pooled)")
    ax.set_ylabel("solve time (us)"); ax.grid(alpha=0.3, axis="y")
    ax.tick_params(axis="x", rotation=15, labelsize=8)
plt.tight_layout(); plt.savefig(out_prefix + "_bytype.png", dpi=110)
print("wrote", out_prefix + "_bytype.png")
