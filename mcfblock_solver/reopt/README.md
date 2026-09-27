# The re-optimization of the Min-Cost Flow problem

This directory holds everything the computational part of the study on the
re-optimization of the Min-Cost Flow (MCF) problem needs, i.e., the driver
that runs the methods, their configuration, the generation of the
instances and the campaign, and it depends on nothing else in `tools/`
(neither `../mcfblock_solver.cpp` nor the `common_utils` of the tools are
used), so that it can be built and run on its own against any build of the
SMS++ modules it links. The results are not kept here: a campaign writes
them in the directory it is given, together with a copy of the
configuration it ran with and the description of the machine.

## The methods

`config/MCFBSCfg.txt` attaches one `Solver` per method to the `MCFBlock`,
and `reopt_bench -k` keeps the one to run:

| k  | method | what it is                                                         |
|----|--------|--------------------------------------------------------------------|
| 0  | SIMw   | `MCFSolver<MCFSimplex>`, re-optimizing                             |
| 1  | SIMc   | `MCFSolver<MCFSimplex>`, from scratch at each solve                |
| 2  | RIVw   | `MCFSolver<RelaxIV>`, re-optimizing                                |
| 3  | RIVc   | `MCFSolver<RelaxIV>`, from scratch at each solve                   |
| 4  | NSw    | the network simplex of LEMON, re-optimizing                        |
| 5  | NSc    | the network simplex of LEMON, from scratch at each solve           |
| 6  | CSw    | the cost scaling of LEMON, re-optimizing                           |
| 7  | CSc    | the cost scaling of LEMON, from scratch at each solve              |
| 8  | CAPw   | the capacity scaling of LEMON, re-optimizing                       |
| 9  | CAPc   | the capacity scaling of LEMON, from scratch at each solve          |
| 10 | CCw    | the cycle canceling of LEMON, re-optimizing                        |
| 11 | CCc    | the cycle canceling of LEMON, from scratch at each solve           |
| 12 | NSSw   | the network simplex of LEMON on `SmartDigraph`, re-optimizing      |
| 13 | NSSc   | the network simplex of LEMON on `SmartDigraph`, from scratch       |
| 14 | CPXw   | `MCFSolver<MCFCplex>`, the network simplex of CPLEX, re-optimizing |
| 15 | CPXc   | `MCFSolver<MCFCplex>`, the network simplex of CPLEX, from scratch  |

Each re-optimizing method differs from the one next to it only by `kReopt`
(`config/ReoptCfg.txt`, and `config/ScratchCfg.txt` that sets it to 0), so
that the pair measures what re-optimizing gains on the same code. The
methods on `SmartDigraph`, the static graph of LEMON, measure what the
graph that can change costs, and those of CPLEX, which are left out by a
build without it, are the reference of a commercial code. The network
simplex of LEMON re-optimizes from the basis of its previous solve, which
it makes primal feasible again after a change of the capacities or of the
deficits, provided the deficits sum to zero; its capacity scaling starts
from the flow and the potentials of the previous solve after any change,
and its cost scaling and cycle canceling from the flow of the previous
solve when it is still feasible, i.e., after a change of the costs (see the
`runWarm()` of each in `MCFLemonSolver/shim`); otherwise, and after a
change of the graph, they start from scratch.

## The instances

`instances.txt` lists them by family, size and seed. The families are those
of Kovacs (2015), with the same parameters: NETGEN, GRIDGEN and GOTO with
`m = 8n` and `m = n sqrt(n)` arcs, and GRIDGRAPH on grids of 16 columns, of
16 rows and square ones, from `n = 2^10` to `2^16` nodes (`2^14` for the
dense ones), 3 seeds each. The generators are those of the first DIMACS
Implementation Challenge, which `gen/fetch-generators` downloads and builds
in `gen/bin` (their sources are not in the repository, since NETGEN comes
with restrictions on its redistribution), and `make-instances` writes the
instances in DIMACS format, which `reopt_bench` reads directly.

## The changes

After the first solve, `reopt_bench` changes the instance and solves it
again for `-n` rounds, each change touching a fraction `-f` of the arcs (or
of the nodes) and being computed from the original data, so that the
instance does not drift away from the one of the generator: the costs
(`cost`), the capacities (`cap`), the deficits of pairs of a supply and a
demand node (`dfct`), the arcs closed at the previous round opened again
and as many others closed (`arcs`), or one of the four at random at each
round (`mix`). Finally, `lag` changes the costs of all the arcs a little at
each round, as those of the subproblem of a Lagrangian relaxation do along
a subgradient method: each cost is its original one plus a multiplier that
moves by `-f` times the largest cost over the square root of the round,
times a standard normal step, kept nonnegative, so that the costs are
fractional. The changes depend only on the instance, the kind and the
seed, hence every method solves the same sequence of instances and their
optimal values can be compared round by round.

## The measures

Each solve gives a line of `reopt_bench`, with the time of `compute()`
alone, i.e., of taking in the changes and re-optimizing (the reading of
the instance, the construction of the `Solver` and the changes made to the
`MCFBlock` are excluded). From these, following Frangioni and Manca
(2006), one has for each method the time of the first solve `T1`, the
total time of the rounds `Ttot`, the re-optimization index `RI`, i.e.,
the average time of a re-solve over that of the first solve, and the gain
`G`, i.e., the time of the re-solves from scratch over that of the
re-solves re-optimizing, which is how much re-optimizing gains on that
sequence; over the seeds they are summarized by the geometric mean.

## Building

With CMake, against an SMS++ installation (a build tree of the umbrella
project does not export its targets, so it has to be installed first,
e.g., `cmake --install <build> --prefix <dir>`):

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<dir>
    cmake --build build

Inside the umbrella project the Find modules of the external libraries are
taken from `../../../SMS++/cmake`; elsewhere they are given with
`-DCMAKE_MODULE_PATH`. With the makefiles, from this directory inside the
umbrella project, `make` builds `reopt_bench` here.

## Running

    gen/fetch-generators                   # gen/bin
    ./make-instances                       # instances/ from instances.txt
    PAR=32 ./run-campaign results/<name>   # every method on every instance
    ./make-tables results/<name>           # results/<name>/tables/*.tex
    ./make-plots results/<name>            # results/<name>/plots/*.png

`run-campaign` runs each method on each instance, kind of change, seed and
repetition in a process of its own, PAR of them at a time, each pinned to a
physical core of its own, and only while the load of the other processes of
the machine is below a threshold; a method that runs out of time on an
instance is not run on the larger ones of the same family and kind. The
variables it reads are described at its top. With `FRAC` it runs the same
campaign with another fraction of changed data, and `make-tables --sens`
and `make-plots --sens` put several such campaigns in one table and one
figure. `make-tables` needs Python alone, `make-plots` also matplotlib.
