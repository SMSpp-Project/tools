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
- tables.tex, one table per axis: for each instance and method the time,
  the relative gap of the bound to the reference value and, for the methods
  with a primal recovery, the gap of the recovered solution to the bound.

The reference value of an instance is the value of the MILP when it is
solved to optimality (status 10), and there is none otherwise. Where a
method was run more than once (REPS > 1), its time is the median of the
repetitions and the spread (max - min) / median is printed, which is the
noise any comparison between two methods is to be read against. A run whose
load at the end was above QUIET is left out, since it is not a measure.
"""

import sys
from pathlib import Path

import pandas as pd
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = Path(__file__).resolve().parent
QUIET = 8.0
OK = {"10", "11"}  # kOK and kLowPrecision
METHODS = ["MILP", "MILP1", "LP", "LD", "LDLD", "LDrec", "LDtree"]
STYLE = {"MILP": ("k", "s"), "MILP1": ("0.5", "s"), "LP": ("0.7", "v"),
         "LD": ("tab:blue", "o"), "LDLD": ("tab:green", "^"),
         "LDrec": ("tab:red", "D"), "LDtree": ("tab:purple", "P")}
AXIS = {"units": ("u", "number of units"),
        "horizon": ("t", "number of periods"),
        "scenarios": ("s", "number of scenarios"),
        "buses": ("b", "number of buses"),
        "tree": ("cd", "number of leaves")}
COLS = ["instance", "method", "status", "lb", "ub", "time", "iter", "rss",
        "rub", "rtime", "gap", "rep", "l0", "l1"]


def value_of(name, key):
    # tuc_u80_t96_s3_b1 -> the number after key; "cd" is the number of
    # leaves c * d of a tree
    if key == "cd":
        return value_of(name, "c") * value_of(name, "d")
    for part in name.split("_"):
        if part.startswith(key) and part[len(key):].isdigit():
            return int(part[len(key):])
    return None


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
            rows.append(" & ".join(row) + r" \\")
        tables.append(
            "\\begin{tabular}{l" + "rr" * 5 + "}\n\\toprule\n"
            "instance & \\multicolumn{2}{c}{MILP} & \\multicolumn{2}{c}{LD} & "
            "\\multicolumn{2}{c}{LDLD} & \\multicolumn{2}{c}{LDrec} & "
            "\\multicolumn{2}{c}{LDtree} \\\\\n"
            " & time & & time & gap & time & gap & time & gap & time & gap "
            "\\\\\n"
            "\\midrule\n" + "\n".join(rows) + "\n\\bottomrule\n\\end{tabular}"
            f"\n% axis: {axis}\n")
    (out / "tables.tex").write_text("\n".join(tables))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
