#!/usr/bin/env python3
# rules_cost.py
#
# What the operating rules of a nuclear unit cost inside a Lagrangian
# decomposition. Reads the CSVs that timing_harness writes over two dump sets
# of the same fleet, the same demand and the same regime, one whose units
# carry the whole set of the rules and one whose rules are reduced to the
# original model (modulations of a single instant), and reports, for each
# solver, the median and the worst solve, how many of them ended on the time
# limit instead of on the problem, and the factor between the two arms.
#
# Every saved subproblem counts, the first solve of each unit comprised, which
# is the convention of the table of the paper: taking the warm ones alone
# moves the medians by about a tenth and drops a fifth of the sample.
#
#   usage: rules_cost.py <rules-off.csv> <rules-on.csv> [limit_s]
#
# A solve counts as stopped by the limit when it reaches it, which is the
# convention of the table: a run that ends at 298 s ended on the problem.
#
#   csv: "unit,iter,solver,time_us,phase", the format of timing_harness
import sys
from collections import defaultdict

import numpy as np

MS, S = 1e3, 1e6


def read(path):
    """solve times per solver, in microseconds."""
    out = defaultdict(list)
    with open(path) as f:
        next(f)
        for line in f:
            parts = line.rstrip("\n").split(",")
            if len(parts) == 5:
                out[parts[2]].append(float(parts[3]))
    return {k: np.array(v) for k, v in out.items()}


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    arms = [("rules off", read(sys.argv[1])), ("rules on", read(sys.argv[2]))]
    limit = float(sys.argv[3]) if len(sys.argv) > 3 else 300.0
    censored = limit * S

    print(f"{'solver':8s} {'arm':10s} {'subpr.':>7s} {'median':>11s} "
          f"{'max':>11s} {'at limit':>10s}  factor")
    for solver in sorted(set().union(*(set(a) for _, a in arms))):
        first = None
        for tag, data in arms:
            t = data.get(solver)
            if t is None or not len(t):
                continue
            med = float(np.median(t))
            unit, scale = ("ms", MS) if med < S else ("s", S)
            factor = f"{med / first:6.2f}x" if first else ""
            print(f"{solver:8s} {tag:10s} {len(t):7d} "
                  f"{med / scale:9.1f}{unit} {t.max() / scale:9.1f}{unit} "
                  f"{int(np.sum(t >= censored)):4d}/{len(t):<5d} {factor}")
            first = first or med


if __name__ == "__main__":
    main()
