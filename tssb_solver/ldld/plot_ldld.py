"""The figures and the tables of the paper out of a campaign.

    python plot_ldld.py <out-dir>

reads <out-dir>/runs.csv, written by run-campaign, and instances.txt, and
writes in <out-dir>:

- time-<axis>.pdf and rss-<axis>.pdf, one per axis of instances.txt (units,
  horizon, scenarios, buses): the running time and the peak memory of each
  method against the value of that axis, on a logarithmic scale, a run that
  did not finish being drawn at the time limit with an empty marker;
- on the tree axis the recursive form on the MultiStageStochasticBlock is
  the method LDtree, the others being run on the TwoStageStochasticBlock of
  the same leaves (<name>_2s), and the abscissa is the number of leaves;
- on the scaling axis, whose instances come in 5 seeds per size, the time of
  a method at a size is the geometric mean over the seeds it solved, and
  time-scaling-u<units>.pdf draws it against the horizon, one figure per
  number of units, while its table gives, for each size and method, that
  time, the number of seeds solved and the mean gap of the bound;
- on the cfl-size axis the instances of the same size (facilities x
  customers) are aggregated as the seeds of the scaling axis, the
  abscissa of time-cfl-size.pdf being the size;
- ratio-uc.pdf and ratio-others.pdf, the time of each method as a ratio to
  that of the MILP on the same instance, on a logarithmic scale with a
  dashed line at 1 (geometric mean over the seeds where there are several),
  one panel per axis; gap.pdf, the gap of the bound of each method to the
  value of the MILP; memory.pdf, the peak memory as a ratio to that of the
  MILP; a point where a run did not finish has an empty marker;
- tables.tex, one table per axis, whose last two columns are the speed-up
  of the recursive form (LDtree on the tree axis) over the two references,
  i.e., the time of the MILP and that of the monolithic dual LD over its
  own: for each instance and method the time,
  the relative gap of the bound to the reference value and, for the methods
  with a primal recovery, the gap of the recovered solution to the bound.

The reference value of an instance is the value of the MILP when it is
solved to optimality (status 10), and there is none otherwise. Where a
method was run more than once (REPS > 1), its time is the median of the
repetitions and the spread (max - min) / median is printed, which is the
noise any comparison between two methods is to be read against. A run whose
load at the end was above QUIET is left out, since it is not a measure.
"""

import re
import sys
from pathlib import Path

import numpy as np

import pandas as pd
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker

HERE = Path(__file__).resolve().parent
QUIET = 8.0
OK = {"10", "20"}  # kOK and kLowPrecision (11 is kStopTime, not a solve)
METHODS = ["MILP", "MILP1", "LP", "LD", "LDLD", "LDrec", "LDtree"]
STYLE = {"MILP": ("k", "s"), "MILP1": ("0.5", "s"), "LP": ("0.7", "v"),
         "LD": ("tab:blue", "o"), "LDLD": ("tab:green", "^"),
         "LDrec": ("tab:red", "D"), "LDtree": ("tab:purple", "P")}
AXIS = {"scenarios": ("s", "number of scenarios"),
        "buses": ("b", "number of buses"),
        "long": ("t", "number of periods"),
        "month": ("u", "number of units"),
        "tree": ("cd", "number of leaves"),
        "cfl-scen": ("s", "number of scenarios")}
COLS = ["instance", "method", "status", "lb", "ub", "time", "iter", "rss",
        "rub", "rtime", "gap", "rep", "l0", "l1"]


def value_of(name, key):
    # tuc_u80_t96_s3_b1 -> the number after key; "cd" is the number of
    # leaves c * d of a tree
    if key == "cd":
        return value_of(name, "c") * value_of(name, "d")
    if key == "fc":
        return value_of(name, "f") * value_of(name, "c")
    for part in name.split("_"):
        if part.startswith(key) and part[len(key):].isdigit():
            return int(part[len(key):])
    return None


def speedup(sub, name, base, rec):
    """time of base over time of rec on the instance, when both finished"""
    b = sub[(sub["instance"] == name) & (sub["method"] == base)]
    r = sub[(sub["instance"] == name) & (sub["method"] == rec)]
    if b.empty or r.empty:
        return ""
    b, r = b.iloc[0], r.iloc[0]
    if (b["status"] not in OK) or (r["status"] not in OK):
        return "--"
    return f"{b['time'] / r['time']:.1f}"


def main(out):
    out = Path(out)
    runs = pd.read_csv(out / "runs.csv", header=None, names=COLS,
                       dtype={"status": str})
    runs = runs[runs["l1"] < QUIET]
    runs["instance"] = runs["instance"].str.replace("smspp_", "", regex=False)
    # a tree: the recursive form on the MultiStageStochasticBlock is LDtree,
    # and the flat equivalent <name>_2s stands for the tree in the rest
    tree = runs["instance"].str.startswith("ttr_")
    flat = runs["instance"].str.endswith("_2s")
    runs.loc[tree & ~flat, "method"] = "LDtree"
    runs.loc[flat, "instance"] = runs.loc[flat, "instance"].str[:-3]

    inst = []
    for line in (HERE / "instances.txt").read_text().splitlines():
        if line.strip() and not line.startswith("#"):
            name, axis = line.split()[:2]
            inst.append((name, axis))
    inst = pd.DataFrame(inst, columns=["instance", "axis"])

    agg = (runs.groupby(["instance", "method"])
           .agg(time=("time", "median"), tmin=("time", "min"),
                tmax=("time", "max"), n=("time", "size"),
                status=("status", "first"), lb=("lb", "first"),
                ub=("ub", "first"), rss=("rss", "max"),
                rub=("rub", "first"), gap=("gap", "first"))
           .reset_index())
    noisy = agg[agg["n"] > 1]
    if len(noisy):
        spread = ((noisy["tmax"] - noisy["tmin"]) / noisy["time"]).max()
        print(f"largest spread over the repetitions: {100 * spread:.1f}%")

    ref = agg[(agg["method"] == "MILP") & (agg["status"] == "10")]
    ref = ref.set_index("instance")["ub"]

    tables = []
    tables.append(scaling(agg, ref, inst, out))
    tables.append(scaling(agg, ref, inst, out, "cfl-size"))
    for axis, (key, label) in AXIS.items():
        names = inst[inst["axis"] == axis]["instance"]
        sub = agg[agg["instance"].isin(names)].copy()
        if sub.empty:
            continue
        sub["x"] = sub["instance"].map(lambda n: value_of(n, key))
        for what, ylabel in (("time", "time (s)"), ("rss", "peak memory (GB)")):
            fig, ax = plt.subplots(figsize=(4.5, 3.2))
            for m in METHODS:
                d = sub[sub["method"] == m].sort_values("x")
                if d.empty:
                    continue
                c, mk = STYLE[m]
                done = d["status"].isin(OK)
                ax.plot(d["x"], d[what], color=c, lw=1)
                ax.plot(d["x"][done], d[what][done], mk, color=c, label=m)
                ax.plot(d["x"][~done], d[what][~done], mk, color=c,
                        mfc="none")
            ax.set_xscale("log")
            ax.set_yscale("log")
            ax.set_xlabel(label)
            ax.set_ylabel(ylabel)
            ax.legend(fontsize=7, frameon=False)
            fig.tight_layout()
            fig.savefig(out / f"{what}-{axis}.pdf")
            plt.close(fig)

        # the table of the axis
        rows = []
        for name in names:
            row = [name.replace("_", r"\_")]
            r = ref.get(name)
            for m in ["MILP", "LD", "LDLD", "LDrec", "LDtree"]:
                d = sub[(sub["instance"] == name) & (sub["method"] == m)]
                if d.empty:
                    row += ["", ""]
                    continue
                d = d.iloc[0]
                t = f"{d['time']:.1f}" if d["status"] in OK else "--"
                if m == "MILP":
                    g = "" if r is not None else "--"
                elif r is not None and d["status"] in OK:
                    g = f"{100 * (r - d['lb']) / abs(r):.3f}"
                else:
                    g = "--"
                row += [t, g]
            # the speed-up of the recursive form (on the tree, LDtree) over
            # the two references, the MILP and the monolithic dual LD
            rec = "LDtree" if axis == "tree" else "LDrec"
            row += [speedup(sub, name, "MILP", rec),
                    speedup(sub, name, "LD", rec)]
            rows.append(" & ".join(row) + r" \\")
        tables.append(
            "\\begin{tabular}{l" + "rr" * 5 + "rr}\n\\toprule\n"
            "instance & \\multicolumn{2}{c}{MILP} & \\multicolumn{2}{c}{LD} & "
            "\\multicolumn{2}{c}{LDLD} & \\multicolumn{2}{c}{LDrec} & "
            "\\multicolumn{2}{c}{LDtree} & "
            "\\multicolumn{2}{c}{speed-up} \\\\\n"
            " & time & & time & gap & time & gap & time & gap & time & gap "
            "& MILP & LD \\\\\n"
            "\\midrule\n" + "\n".join(rows) + "\n\\bottomrule\n\\end{tabular}"
            f"\n% axis: {axis}\n")
    (out / "tables.tex").write_text("\n".join(tables))
    figures(agg, ref, inst, out)


def scaling(agg, ref, inst, out, axis="scaling"):
    """An axis of sizes whose instances come in several seeds: units x
    horizon for the scaling axis, facilities x customers for cfl-size,
    whose seeds are the different ORLib instances of a size."""
    names = set(inst[inst["axis"] == axis]["instance"])
    sub = agg[agg["instance"].isin(names)].copy()
    if sub.empty:
        return ""
    # the size is the name without its last part, the seed
    sub["size"] = sub["instance"].str.replace(r"_[^_]+$", "", regex=True)
    a, b = ("u", "t") if axis == "scaling" else ("f", "c")
    sub["u"] = sub["size"].map(lambda n: value_of(n, a))
    sub["t"] = sub["size"].map(lambda n: value_of(n, b))
    sub["ref"] = sub["instance"].map(ref)
    sub["solved"] = sub["status"].isin(OK)
    sub["bgap"] = 100 * (sub["ref"] - sub["lb"]) / sub["ref"].abs()

    def geomean(x):
        return float(np.exp(np.log(x).mean())) if len(x) else np.nan

    rows = []
    for (size, m), d in sub.groupby(["size", "method"]):
        ok = d[d["solved"]]
        rows.append({"size": size, "u": d["u"].iloc[0], "t": d["t"].iloc[0],
                     "method": m, "time": geomean(ok["time"]),
                     "solved": len(ok), "seeds": len(d),
                     "gap": ok["bgap"].mean()})
    res = pd.DataFrame(rows)

    if axis != "scaling":  # one figure, against the size
        res["x"] = res["u"] * res["t"]
        fig, ax = plt.subplots(figsize=(4.5, 3.2))
        for m in METHODS:
            d = res[res["method"] == m].sort_values("x")
            if d.empty:
                continue
            c, mk = STYLE[m]
            ax.plot(d["x"], d["time"], mk + "-", color=c, lw=1, label=m)
        ax.set_xscale("log")
        ax.set_yscale("log")
        ax.set_xlabel("facilities x customers")
        ax.set_ylabel("time (s), geometric mean over the instances")
        ax.legend(fontsize=7, frameon=False)
        fig.tight_layout()
        fig.savefig(out / f"time-{axis}.pdf")
        plt.close(fig)

    for u, du in (res.groupby("u") if axis == "scaling" else []):
        fig, ax = plt.subplots(figsize=(4.5, 3.2))
        for m in METHODS:
            d = du[du["method"] == m].sort_values("t")
            if d.empty:
                continue
            c, mk = STYLE[m]
            ax.plot(d["t"], d["time"], mk + "-", color=c, lw=1, label=m)
        ax.set_xscale("log")
        ax.set_yscale("log")
        ax.set_xlabel("number of periods")
        ax.set_ylabel("time (s), geometric mean over the seeds")
        ax.set_title(f"{u} units", fontsize=9)
        ax.legend(fontsize=7, frameon=False)
        fig.tight_layout()
        fig.savefig(out / f"time-scaling-u{u}.pdf")
        plt.close(fig)

    cols = ["MILP", "MILP1", "LD", "LDLD", "LDrec"]
    lines = []
    for (u, t), d in res.groupby(["u", "t"]):
        row = [str(u), str(t)]
        for m in cols:
            e = d[d["method"] == m]
            if e.empty:
                row += ["", ""]
                continue
            e = e.iloc[0]
            row += ["--" if np.isnan(e["time"]) else f"{e['time']:.1f}",
                    f"{e['solved']}/{e['seeds']}"]
        # the speed-up of LDrec over the MILP and over LD, as the ratio of
        # their geometric means
        tm = dict(zip(d["method"], d["time"]))
        for base in ("MILP", "LD"):
            a, b = tm.get(base, np.nan), tm.get("LDrec", np.nan)
            row.append("--" if np.isnan(a) or np.isnan(b) else f"{a / b:.1f}")
        lines.append(" & ".join(row) + r" \\")
    head = " & ".join(rf"\multicolumn{{2}}{{c}}{{{m}}}" for m in cols)
    h1, h2 = ("$u$", "$n$") if axis == "scaling" else ("$f$", "$c$")
    return ("\\begin{tabular}{rr" + "rr" * len(cols) + "rr}\n\\toprule\n"
            f"{h1} & {h2} & {head} & \\multicolumn{{2}}{{c}}{{speed-up}} "
            "\\\\\n"
            " & " + " & time & solved" * len(cols) + " & MILP & LD "
            "\\\\\n\\midrule\n"
            + "\n".join(lines) + "\n\\bottomrule\n\\end{tabular}\n"
            f"% axis: {axis}\n")


# the figures of the paper -------------------------------------------------
# Each method is drawn as the ratio of its time (or memory) to that of the
# MILP on the same instance, on a logarithmic scale with a dashed line at 1,
# so that differences of orders of magnitude read at a glance whatever the
# absolute times; a point where the method (or the MILP) did not finish is
# drawn with an empty marker, its ratio being a bound and not a value.

RATIO = ["MILP1", "LP", "LD", "LDLD", "LDrec", "LDtree"]
BOUND = ["LP", "LD", "LDLD", "LDrec", "LDtree"]


def geomean(x):
    x = np.asarray(x, dtype=float)
    x = x[np.isfinite(x) & (x > 0)]
    return float(np.exp(np.log(x).mean())) if len(x) else np.nan


def panel_data(agg, ref, names, key):
    """for each value of the axis and method: the time and the memory as
    ratios to those of the MILP (geometric means over the instances with
    that value, i.e., the seeds), whether all the runs finished, and the
    mean gap of the bound to the value of the MILP (%)"""
    sub = agg[agg["instance"].isin(names)].copy()
    if sub.empty:
        return pd.DataFrame()
    sub["x"] = sub["instance"].map(lambda n: value_of(n, key))
    # the label of a value: the value itself, or facilities x customers
    lab = {}
    for n in sub["instance"].unique():
        x = value_of(n, key)
        lab[x] = (f"{value_of(n, 'f')}x{value_of(n, 'c')}" if key == "fc"
                  else f"{x}")
    milp = sub[sub["method"] == "MILP"].set_index("instance")
    rows = []
    for (x, m), d in sub.groupby(["x", "method"]):
        t, r, ok, g = [], [], True, []
        for _, e in d.iterrows():
            if e["instance"] not in milp.index:
                continue
            b = milp.loc[e["instance"]]
            t.append(e["time"] / b["time"])
            r.append(e["rss"] / b["rss"] if b["rss"] > 0 else np.nan)
            ok &= (e["status"] in OK) and (b["status"] in OK)
            v = ref.get(e["instance"])
            if v is not None and e["status"] in OK:
                g.append(100 * (v - e["lb"]) / abs(v))
        rows.append({"x": x, "lab": lab[x], "method": m,
                     "time": geomean(t),
                     "rss": geomean(r), "ok": ok,
                     "gap": np.mean(g) if g else np.nan})
    return pd.DataFrame(rows)


# LDLD and LDrec give the same bound (Theorem 1): LDLD is drawn larger and
# below, so that both stay visible where they coincide
SIZE = {"LDLD": 7}


def draw(ax, res, what, methods, ylabel, title, xlabel, ref_line=True):
    for m in methods:
        d = res[res["method"] == m].sort_values("x")
        d = d[np.isfinite(d[what]) & (d[what] > 0)]
        if d.empty:
            continue
        c, mk = STYLE[m]
        ax.plot(d["x"], d[what], color=c, lw=1)
        ms = SIZE.get(m, 4)
        ax.plot(d["x"][d["ok"]], d[what][d["ok"]], mk, color=c, ms=ms,
                label=m)
        ax.plot(d["x"][~d["ok"]], d[what][~d["ok"]], mk, color=c, ms=ms,
                mfc="none")
    if ref_line:
        ax.axhline(1, color="k", ls="--", lw=0.8)
    ax.set_yscale("log")
    ax.set_title(title, fontsize=9)
    ax.set_xlabel(xlabel, fontsize=8)
    ax.set_ylabel(ylabel, fontsize=8)
    ax.tick_params(labelsize=7)
    ax.grid(True, which="major", alpha=0.3)


def multi(panels, what, methods, ylabel, fname, out, ref_line=True):
    """one figure with a panel per (title, data, xlabel, logx)"""
    panels = [p for p in panels if not p[1].empty]
    if not panels:
        return
    n = len(panels)
    cols = min(n, 3)
    rows = (n + cols - 1) // cols
    fig, axes = plt.subplots(rows, cols, figsize=(3.3 * cols, 2.7 * rows),
                             squeeze=False)
    for ax, (title, res, xlabel, logx) in zip(axes.flat, panels):
        draw(ax, res, what, methods, ylabel, title, xlabel, ref_line)
        if logx:
            # the values of the axis as the ticks, written in full
            ax.set_xscale("log")
            ticks = res.drop_duplicates("x").sort_values("x")
            ax.set_xticks(ticks["x"])
            long = ticks["lab"].str.len().max() > 4
            ax.set_xticklabels(ticks["lab"], fontsize=7,
                               rotation=30 if long else 0)
            ax.xaxis.set_minor_formatter(matplotlib.ticker.NullFormatter())
    for ax in list(axes.flat)[n:]:
        ax.axis("off")
    h, lab = [], []
    for ax in axes.flat:
        for hh, ll in zip(*ax.get_legend_handles_labels()):
            if ll not in lab:
                h.append(hh)
                lab.append(ll)
    fig.legend(h, lab, loc="lower center", ncol=len(lab), fontsize=7,
               frameon=False)
    fig.tight_layout(rect=(0, 0.06, 1, 1))
    fig.savefig(out / fname)
    plt.close(fig)


def figures(agg, ref, inst, out):
    """ratio-uc.pdf, ratio-others.pdf, gap.pdf and memory.pdf"""
    def names(axis, pred=None):
        n = inst[inst["axis"] == axis]["instance"]
        return set(n if pred is None else [x for x in n if pred(x)])

    uc = []
    for t, label in ((24, "a day"), (168, "a week")):
        uc.append((f"scaling, {label}",
                   panel_data(agg, ref, names(
                       "scaling", lambda x: value_of(x, "t") == t), "u"),
                   "number of units", True))
    uc.append(("80 units", panel_data(agg, ref, names("long"), "t"),
               "number of periods", True))
    uc.append(("40 units, 96 periods",
               panel_data(agg, ref, names("scenarios"), "s"),
               "number of scenarios", True))
    uc.append(("80 units, 96 periods",
               panel_data(agg, ref, names("buses"), "b"),
               "number of buses", True))
    others = [("three-stage trees", panel_data(agg, ref, names("tree"), "cd"),
               "number of leaves", True),
              ("facility location",
               panel_data(agg, ref, names("cfl-size"), "fc"),
               "facilities x customers", True),
              ("facility location, 50 x 50",
               panel_data(agg, ref, names("cfl-scen"), "s"),
               "number of scenarios", True)]
    ylab = "time / time of the MILP"
    multi(uc, "time", RATIO, ylab, "ratio-uc.pdf", out)
    multi(others, "time", RATIO, ylab, "ratio-others.pdf", out)
    multi(uc + others, "gap", BOUND, "gap of the bound (%)", "gap.pdf", out,
          ref_line=False)
    multi([p for p in uc if p[0] in ("scaling, a week", "80 units")],
          "rss", RATIO, "memory / memory of the MILP", "memory.pdf", out)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
