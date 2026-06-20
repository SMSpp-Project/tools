# Lagrangian-iteration timing study

How fast is the **single-unit** solve as a Lagrangian dual uses it, how do the
two exact dynamic-programming solvers compare with a general-purpose MILP solver
on the *same* integer problem, and how does pricing spinning reserves change the
picture?

The study lives entirely under this directory so the base `ucblock_solver`
never carries the experimental per-iteration dump (see `makefile`): a normal
build leaves the `SAVE_TUB` macro at `0`.

## Pipeline

1. **`ucblock_solver` built with `-DSAVE_TUB=1`** solves a single-bus `UCBlock`
   with a `LagrangianDualSolver` and, every `intEverykIt` Lagrangian iterations,
   serializes each inner `ThermalUnitBlock`, carrying its *current Lagrangian
   costs* (the active-power linear term and, when the unit prices reserve, the
   primary/secondary spinning-reserve costs), to `TUB-<unit>-<iter>.nc4`.
2. **`timing_harness <dir>`** times four solvers and prints
   `unit,iter,solver,time_us,phase`:
   - `stdDP`, `ThermalUnitDPSolver`, serial (the base `O(n^3)` scheme);
   - `parDP`, `ThermalUnitDPSolver`, FastFlow parallel (`intMaxThread 0`,
     `intParMinN 0`);
   - `extDP`, `ThermalUnitExtDPSolver` (the run-length variant);
   - `MILP`, a `:MILPSolver` on the **T + Perspective-Cuts** formulation
     (`../config/TUBCfg-tpc.txt`, `wf = 9`), solved as an integer MILP
     (`intRelaxIntVars 0`) so it is the same problem the exact DPs solve. The
     back-end is chosen in `../config/TUBSCfg-MILP.txt` (currently Gurobi).

   The timing mirrors how a `LagrangianDualSolver` actually uses the solver: the
   unit is **loaded once** and the Solver is **attached once**, then from one
   iteration to the next only the Lagrangian costs change, pushed into the same
   attached Block via `set_linear_term()` / `set_*_spinning_reserve_cost()`
   followed by a `compute()`, never a detach, re-attach or model rebuild. Each
   unit therefore yields one **`cold`** row (the one-off first solve, model
   build plus solve) and one **`warm`** row per later dump (the re-optimization
   the dual pays at every iteration: Gurobi keeps its presolved model and
   warm-starts, `stdDP` keeps its graph, `extDP` rebuilds its value functions).
   The per-iteration cost is the warm one.
3. **`plot_lagrangian_timing.py`** (per gen_type, time vs normalized iteration),
   **`plot_algorithms.py`** (per solver, scaling and head-to-head ratios) and
   **`plot_reserve.py`** (the with/without-reserve comparison) read the warm
   rows.

## Running

Build the two experiment binaries (recompiles `../ucblock_solver.cpp` with
`SAVE_TUB`, in place):

    make

The parallel DP is forced on every horizon from `../config/TUBSCfg-parDP.txt`
(`intParMinN 0`, a run-time `ThermalUnitDPSolver` parameter), so no special
build is needed. The full `~10⁴`-iteration LD solve is the wall-clock
bottleneck, while the per-unit solve time is flat along the iteration, so a
handful of dump waves suffices; `LD_MAXITER` caps the LD and `LD_EVERYK` sets
the dump stride (how many waves, hence warm samples, a unit gets), both via a
per-run copy of the config so the shared `../config/LDCfg.txt` is untouched:

    LD_EVERYK=50 LD_MAXITER=300 HORIZONS="day week" SEEDS="1 2 3 4 5" \
      PYTHON=/usr/bin/python3.14 ./run-lagrangian-timing

**Reserve experiment.** The single-bus instances carry a reserve demand, so the
dual prices reserve and the dumps carry it. For the energy-only arm,
`./make-noreserve <in.nc4> <out.nc4>` strips the reserve demand (it needs only
netCDF's `ncdump`/`ncgen`, not the Julia generator), and `LD_NCDIR` points the
driver at the stripped copies:

    LD_NCDIR=/tmp/nc_nores HORIZONS=day SEEDS="1 2 3 4 5" ... ./run-lagrangian-timing

See the header of `run-lagrangian-timing` for every knob and the reproducible
recipe, including running the binaries from a copy outside a synced folder.

## Findings

The per-unit solve time is **flat along the Lagrangian iteration** (within a few
percent), so the per-iteration cost is well summarised by a median. It is
governed by the unit *type* and scales very differently with the horizon `n`
(`day` 96, `week` 672, `month` 2976), and pricing spinning reserves changes the
ranking entirely. Median warm time per solve, **no reserves, with reserves**:

| solver |    day n=96    |   week n=672   |   month n=2976  |
|--------|---------------:|---------------:|----------------:|
| stdDP  | 0.23 , 6.52 ms |  17.8 , 396 ms |  478 , 8714 ms  |
| parDP  | 1.00 , 2.90 ms |  7.0 , 111 ms  |  160 , 2757 ms  |
| extDP  | 0.34 , 1.18 ms |  3.9 , 9.6 ms  |   20 , 38 ms    |
| MILP   |   54 , 76 ms   |  416 , 390 ms  |  2007 , 1872 ms |

(median, day/week over 5/2 seeds; month over 1 seed; the `month` with-reserve
row is the `cold` solve, the dual at `n=2976` being too slow to dump a second
wave. See `results/reserve_scaling.png`.)

- **Energy-only, the base DP wins only at short horizons.** At `n=96` `stdDP`
  (0.23 ms) beats the run-length `extDP` (0.34 ms); from `n=672` up `extDP`
  overtakes it (3.9 ms vs 17.8 ms) and pulls away (`O(n^3)` vs near-linear).
- **Pricing reserves flips the comparison.** It is nearly free for the
  run-length DP (extDP grows only 2 to 3.4 times) but devastating for the base
  DP, whose single-parabola inner loop must generalise to a multi-piece cost:
  `stdDP` grows 28 times at `n=96`, 22 times at `n=672`, 18 times at `n=2976`,
  to the point of becoming **slower than the MILP** at `n=672` and reaching
  `~8.7 s` at `n=2976`. So with reserves the run-length **extDP wins at every
  horizon**, by up to two orders of magnitude.
- **FastFlow parallelism pays once the solve is heavy enough.** Energy-only at
  `n=96` it does not (the thread-dispatch overhead is a one-off `cold` cost,
  amortised over the dual, leaving the warm `parDP` near `stdDP`), but it helps
  from `n=672` up, and with reserves it already pays at `n=96` (`parDP` 2.9 ms
  vs `stdDP` 6.5 ms), the priced dispatch being heavy enough to parallelise.
- **The MILP cost is per-solve build and presolve, not branch-and-bound, and is
  nearly reserve-insensitive.** A single `n=96` solve is a linear MILP found
  optimal at the root in one node, with most of the time in model build and
  Gurobi presolve (`~0.1 s` at `n=96`, `~2 s` at `n=2976`). The dual solves the
  single-unit problem on the order of `10⁴` times and Gurobi pays this fixed
  cost each time, so the MILP stays one to two orders of magnitude behind
  `extDP` throughout, and reserves barely move it (the reserve rows add little
  to the presolve).
- **No drift along the dual, a real spread across unit types.** The warm time
  is flat over the Lagrangian iterations: the per-unit coefficient of variation
  across iterations is a median of `5` to `9` percent at `day` and `1` to `5`
  percent at `week` for every solver (the larger `day` figure is timing noise on
  sub-millisecond solves), so one median per unit summarises the iteration well
  (see `ld_timing_<h>_vs_iter.png`). The spread across unit *types* is instead
  real and solver-dependent: with reserves at `day` the run-length `extDP` runs
  from `~0.35 ms` on a peaker to `~3.4 ms` on the old coal unit, a `~10x` range
  that tracks how many cost pieces the unit carries, while the base `stdDP`,
  already saturated by the multi-piece reserve cost, flattens to `~1.7x` (see
  `ld_timing_<h>_bytype.png`).
