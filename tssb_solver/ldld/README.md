# The nested and the recursive Lagrangian dual

This directory holds everything the computational part of the paper on the
nested and the recursive Lagrangian dual of a `TwoStageStochasticBlock` needs,
i.e., the driver that runs the methods, their configurations, the generation
of the instances, the campaign and the figures, and it depends on nothing else
in `tools/` (neither `../tssb_solver.cpp` nor the `common_utils` of the tools
are used), so that it can be built and run on its own against any build of
the SMS++ modules it links. The results are not kept here: a campaign writes
them in the directory it is given, together with a copy of the configuration
it ran with and the description of the machine.

## The methods

`config/TSSBSCfg.txt` attaches one `Solver` per method to the
`TwoStageStochasticBlock`, and `ldld_bench -k` keeps the one to run:

| k | method | what it solves                                                    |
|---|--------|-------------------------------------------------------------------|
| 0 | MILP   | the deterministic equivalent, integer, to 1e-6 and 3600 s at most |
| 1 | LP     | its continuous relaxation                                         |
| 2 | LD     | the dual over the scenarios, each scenario solved as a MILP       |
| 3 | LDLD   | the nested form: each scenario solved by an inner dual            |
| 4 | LDrec  | the recursive form: one dual over the units of all the scenarios  |
| 5 | MILP1  | the MILP of 0 on a single thread, as the duals run                |

The nested and the recursive form give the same bound, which is weaker than
that of LD (whose scenarios are solved as integer problems) and stronger than
LP. In the configuration the recursive form is the nested one with
`intRecursive 1` and the units as the components (`LDCfg-LDLD.txt` and the
last lines of `TSSBSCfg.txt`), and all the duals share `LDCfg.txt`. With
`-R ScnBSCfg-IP.txt`, after the dual the here-and-now variables are fixed to
their mean over the scenarios and each scenario is solved as a MILP, which
gives a feasible solution and so a gap for the methods that give a bound
alone.

## Building

With CMake, against an SMS++ installation (a build tree of the umbrella
project does not export its targets, so it has to be installed first, e.g.,
`cmake --install <build> --prefix <dir>`):

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<dir>
    cmake --build build

Inside the umbrella project the Find modules of the external libraries are
taken from `../../../SMS++/cmake`; elsewhere they are given with
`-DCMAKE_MODULE_PATH`. With the makefiles, from this directory inside the
umbrella project, `make` builds `ldld_bench` here.

## Running

    ./make-instances                      # instances/ from instances.txt
    ./run-campaign results/<name>         # every method on every instance
    python plot_ldld.py results/<name>    # figures and tables.tex

`instances.txt` lists the instances with the arguments of the generator. They
are PyPSA networks, written by the generators in `gen/` and converted by
pypsa2smspp. The scaling axis grows the thermal family in units (10, 20, 50,
100) and horizon (a day and a week at hourly resolution), with 5 seeds at
each size, whose spread the figures and tables report; the month axis keeps
the smallest and the largest fleet over a month, one seed each, as an
example of the horizon where no method closes within the time limit; the
long axis takes 80 units from two days to two weeks; the other axes sweep
one parameter at a time (scenarios, buses) around a common point, so that
each figure reads one axis. The last
axis is that of the three-stage trees (`ttr_` instances, by
`gen/gen_thermal_tree.py`): the outer stage is the climate year, which scales
the availability of the renewables, the inner one the demand, drawn
conditional on the climate, and the fleet is that of the two-stage family.
Such an instance is a `MultiStageStochasticBlock`, whose sub-Blocks are one
`TwoStageStochasticBlock` per climate, and on it the recursive form is the
only Lagrangian decomposition that applies, since the dual over the climates
would have a `TwoStageStochasticBlock` as the Block of each `LagBFunction`,
which requires an `FRealObjective` that a `TwoStageStochasticBlock` has not;
the recursive form goes through the three levels down to the units. The same
leaves as a single `TwoStageStochasticBlock` (`<name>_2s`) have the same
optimum and the same bound, and all the other methods run on it; in the
figures and tables the recursive form on the tree is `LDtree`.

The last two axes are a second problem, the stochastic capacitated facility
location (`cfl_` instances, by `gen/cfl_tssb_gen`, out of the ORLib instances
of the `CapacitatedFacilityLocationBlock` module): the openings of the
facilities are decided before the demands of the customers are known, and
each scenario is a `CapacitatedFacilityLocationBlock`. In its knapsack
formulation each facility is a `BinaryKnapsackBlock`, solved by dynamic
programming, whose last item is the opening; the nested form relaxes the
non-anticipativity of the openings over the scenarios and, in each scenario,
the demand constraints, and the recursive form relaxes both at once, its
components being the knapsacks of all the scenarios. The Lagrangian methods
run on the knapsack formulation (`InnerBCfg.txt`, whose
`CFLBCfg-KF.txt` makes the knapsacks the structure of the Block, so that the
recursive form sees them), and so do the MILP and its relaxation, which add
the strong constraints x_ij <= y_i (`InnerBCfg-MILP.txt`), as `run-campaign`
chooses by itself; the demands of the scenarios are integer, as the dynamic
programming of the knapsacks wants them. `cfl-size` takes 4 instances per size, from 16 x 50 to
100 x 1000 facilities x customers, with 10 scenarios each, and `cfl-scen`
one instance of 50 x 50 with 5 to 100 scenarios. The
generation needs a Python with `pypsa` and `pypsa2smspp`, and its seed is
fixed, so that the files are rebuilt identical from the list.

`run-campaign` runs one method on one instance per process, one at a time,
and starts a run only when the load of the machine is below `QUIET`; it
writes one line per run in `runs.csv`, with the load at the start and the
average number of cores the other processes kept busy during the run (the
CPU time of the machine minus that of the run, over its wall-clock time),
and a run is skipped if it is already there, so that an interrupted
campaign is resumed by launching it again. The times of the paper are taken
on an otherwise idle machine: a run during which the other processes kept
more than `QUIET` cores busy is not a measure, it is run again when the
campaign is launched again, and `plot_ldld.py` leaves it out. Before two methods are
compared, `REPS=2` measures the noise, i.e., each method against itself.

A single run is

    build/ldld_bench -c config/ -k 4 -m LDrec -R ScnBSCfg-IP.txt \
                     instances/smspp_tuc_u80_t96_s5_b1.nc4

which prints `instance,method,status,lb,ub,time,iter,rss,rub,rtime,gap`: the
time is that of `compute()` alone, the reading of the instance and the
construction of the `Solver` being excluded, `iter` is the number of
iterations of the outer bundle and `rss` the peak memory of the process in
GB. The MILP uses all the threads Gurobi takes, the duals one thread, and
MILP1 is there to compare the two at the same resources.
