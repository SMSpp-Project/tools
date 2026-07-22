# Lagrangian-iteration timing study

How fast is the **single-unit** solve as a Lagrangian dual uses it, how do the
two exact dynamic-programming solvers compare with a general-purpose MILP solver
on the *same* integer problem, how does pricing spinning reserves and reactive
power change the picture, and what does the whole Lagrangian scheme buy against
the monolithic T+Perspective-Cuts formulation?

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
     (`config/TUBCfg-tpc.txt`, `wf = 9`), solved as an integer MILP
     (`intRelaxIntVars 0`) so it is the same problem the exact DPs solve. The
     back-end is chosen in `config/TUBSCfg-MILP.txt` (currently Gurobi).

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

The parallel DP is forced on every horizon from `config/TUBSCfg-parDP.txt`
(`intParMinN 0`, a run-time `ThermalUnitDPSolver` parameter), so no special
build is needed. The full `~10⁴`-iteration LD solve is the wall-clock
bottleneck, while the per-unit solve time is flat along the iteration, so a
handful of dump waves suffices; `LD_MAXITER` caps the LD and `LD_EVERYK` sets
the dump stride (how many waves, hence warm samples, a unit gets), both via a
per-run copy of the config so the shared `../config/LDCfg.txt` is untouched:

    LD_EVERYK=50 LD_MAXITER=300 HORIZONS="day week" SEEDS="1 2 3 4 5" \
      PYTHON=/usr/bin/python3.14 ./run-lagrangian-timing

**Pricing arms.** The factorial single-bus instances
(`UCBlock/data/nc4/UC_singlebus/sb_<n>u_<h>_{nores,res}_{noreac,reac}_seed<s>.nc4`,
from `UCBlock/tools/json2netCDF`) provide each arm natively: what the instance
carries (reserve demand, reactive data) is what the dual prices and the dumps
record. `LD_NCDIR` points the driver at the arm's copies and `LD_JSONDIR` at
the matching JSONs for the `gen_type` map:

    LD_NCDIR=/tmp/exp/res_noreac HORIZONS=day SEEDS="1 2 3 4 5" ... ./run-lagrangian-timing

For an instance set without energy-only variants,
`./make-noreserve <in.nc4> <out.nc4>` strips the reserve demand (it needs only
netCDF's `ncdump`/`ncgen`, not the Julia generator).

**Lagrangian dual against the monolithic formulation.** `run-ld-vs-tpc` solves
each instance three ways, the Lagrangian dual with a graceful time-budgeted
stop (`config/UCBSCfg-LD-bound.txt`), the T+Perspective-Cuts LP relaxation and
the integer T+P/C MILP (`config/InnerBCfg-tpc.txt`, with the shared
`../config/BSCfg.txt` and `config/BSCfg-int.txt` respectively), and tabulates
bound and wall-clock of each into one CSV.

See the headers of `run-lagrangian-timing` and `run-ld-vs-tpc` for every knob
and the reproducible recipe, including running the binaries from a copy outside
a synced folder.

**Build pitfalls.** Three traps when building the study binaries from scratch:

1. the makefile compiles `BundleSolver.o` with the default `WHICH_OSI_QP=2`
   (Gurobi) while a `libNDO.a` built by CMake wants OsiCpx: the LD then dies
   with `OSIMPSolver::SetOsi: not an OsiCpx`. Either recompile `BundleSolver.o`
   with `-DWHICH_OSI_QP=1` or link the CMake-built objects throughout;
2. `../../common_utils.cpp` includes the generated `SMS++Config.h`, which lives
   in the CMake build tree: `make CMMNUINC="-I../.. -I<cmake-build>/SMS++"`;
3. the dump producer must carry `-DSAVE_TUB=1` (this makefile sets it; a CMake
   `ucblock_solver` needs the flag added when compiling `ucblock_solver.cpp`).

A binary whose `ThermalUnitBlock`-chain objects predate the current sources
silently measures the old code: check the link line, not just the mtime of the
executable.

## Profiling a single re-solve

To profile one priced 1UC solve (no Lagrangian run, no harness), use any
`TUB-<unit>-<iteration>.nc4` dump: it is a stand-alone `ThermalUnitBlock`
carrying the true Lagrangian energy and reserve prices of that iteration
(`LinearTerm`, `Primary/SecondarySpinningReserveCost`, and
`ReactiveLinearTerm` when the instance prices reactive power). Then:

1. set `TUDPS_PROFILE` to `1` at the top of
   `UCBlock/src/ThermalUnitDPSolver.cpp` and rebuild: `compute()` prints
   per-phase wall-clock (`build_graph` / `compute_EDPs` / `min_path` /
   `compute_solutions`) on every call;
2. run the standalone checker on the dump with the base DP as second solver
   (in `tests/ThermalUnitBlock_Solver`, a `BSCfg.txt` copy with
   `ThermalUnitDPSolver` in place of `ThermalUnitExtDPSolver`):

       ./TUDPS_test -S BSCfg-base.txt TUB-<u>-<i>.nc4

   The reserve-priced path is active whenever the dump carries a negative
   reserve cost; on an instance without one, the env hooks `TUDPS_RESCOST=<c>`
   / `TUDPS_QCOST=<c>` in `test.cpp` inject a constant reserve / reactive
   price. The reserve overhead of the base DP lives in `compute_EDPs`
   (`augment_with_g`, the multi-piece per-period cost in the sweep).

## Findings

The per-unit solve time is **flat along the Lagrangian iteration** (within a few
percent), so the per-iteration cost is well summarised by a median. It is
governed by the unit *type* and scales very differently with the horizon `n`
(`day` 96, `week` 672, `month` 2976), and pricing spinning reserves changes the
ranking entirely. Median warm time per solve (ms), **energy-only, with spinning
reserves priced (+r), with reserves and reactive power priced (+r+q)**:

| solver |     day n=96     |   week n=672    |    month n=2976    |
|--------|-----------------:|----------------:|-------------------:|
| stdDP  | 0.40 , 8.8 , 9.4 | 23 , 487 , 637  |  629 , 8850 , 9900 |
| parDP  | 0.93 , 4.1 , 4.7 | 11 , 167 , 220  |  235 , 3010 , 2920 |
| extDP  | 0.57 , 1.8 , 1.8 | 5.9 , 12 , 19   |    22 , 45 , 54    |
| MILP   |  77 , 102 , 111  | 433 , 459 , 559 | 2520 , 1830 , 1960 |

(median, day/week over 5/2 seeds, month over 1 seed, on the factorial
`sb_<n>u_<h>_{nores,res}_{noreac,reac}` instances; per-arm CSVs and plots in
`results/config_c/`.)

- **Energy-only, the base DP wins only at short horizons.** At `n=96` `stdDP`
  (0.40 ms) beats the run-length `extDP` (0.57 ms); from `n=672` up `extDP`
  overtakes it (5.9 ms vs 23 ms) and pulls away (`O(n^3)` vs near-linear).
- **Pricing reserves flips the comparison.** It is nearly free for the
  run-length DP (extDP grows only 2 to 3 times) but devastating for the base
  DP, whose single-parabola inner loop must generalise to a multi-piece cost:
  `stdDP` grows 22 times at `n=96`, 21 times at `n=672`, 14 times at `n=2976`,
  to the point of becoming **slower than the MILP** at `n=672` and reaching
  `~8.9 s` at `n=2976`. So with reserves the run-length **extDP wins at every
  horizon**, by up to two orders of magnitude.
- **Pricing reactive power on top adds a modest overhead.** ~10-30% on the DP
  solvers (up to ~1.6x for `extDP` at `n=672`), without changing the ranking.
- **FastFlow parallelism pays once the solve is heavy enough.** Energy-only at
  `n=96` it does not (the thread-dispatch overhead is a one-off `cold` cost,
  amortised over the dual, leaving the warm `parDP` near `stdDP`), but it helps
  from `n=672` up, and with reserves it already pays at `n=96` (`parDP` 4.1 ms
  vs `stdDP` 8.8 ms), the priced dispatch being heavy enough to parallelise.
- **The MILP cost is per-solve build and presolve, not branch-and-bound, and is
  nearly reserve-insensitive.** A single `n=96` solve is a linear MILP found
  optimal at the root in one node, with most of the time in model build and
  Gurobi presolve (`~0.1 s` at `n=96`, `~2 s` at `n=2976`). The dual solves the
  single-unit problem on the order of `10⁴` times and Gurobi pays this fixed
  cost each time, so the MILP stays one to two orders of magnitude behind
  `extDP` throughout, and reserves barely move it (the reserve rows add little
  to the presolve).
- **Warm-started, the Lagrangian dual certifies near-LP bounds in near-LP
  time.** `run-warm-ld` solves each reserve instance once: LP relaxation ->
  its duals initialise the multipliers -> bundle with extDP inner solvers
  (budgeted by iterations: 200/100/50 at day/week/month) -> primal recovery
  (`results/config_c/ldtpc/warm_ld_all.csv`). The bound lands within 0.4-1%
  of the LP bound at `day` in ~70-100 s, within 0.6-3.3% at `week`, within
  1.1-6% at `month`, where the LP warm start itself dominates the time; from
  a warm point a master iteration costs ~1.4 s even at 50 units x month,
  while cold-started the master pays minutes per iteration before yielding
  any meaningful bound (`run-ld-vs-tpc`, `ld_vs_tpc_all.csv`). The reactive
  arm consistently trails: the LP is blind to the reactive fields, so those
  multipliers cannot be warm-started. At `day` the recovery returns a
  feasible schedule in the same run (+1.1-1.3% above the LP bound, a ~1.5%
  certified gap); at larger scales its restricted problems inherit the
  monolithic intractability. The one monolithic computation the scheme
  cannot avoid, the seeding LP, sets its time scale.
- **No drift along the dual, a real spread across unit types.** The warm time
  is flat along the Lagrangian iterations and governed by the unit type. The
  flatness is in `ld_timing_day_vs_iter.png` (median per-unit coefficient of
  variation within a few percent, clearest at `day` where the dual runs long
  enough for many dump waves), the per-type spread in `ld_timing_<h>_bytype.png`:
  the run-length `extDP` ranges close to an order of magnitude from peaker to
  old coal, the reserve-saturated base `stdDP` only `~2x`.
