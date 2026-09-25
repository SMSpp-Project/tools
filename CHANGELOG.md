# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- `investmentblock_solver` solves an `InvestmentBlock` whose inner Block is an
  `SDDPBlock`, from a Block file as from a problem file: the UCBlock of each
  stage, which sits behind a `BendersBFunction` that no BlockConfig crosses,
  gets the `-B` "meta"-BlockConfig, and each `SDDPGreedySolver` passes the
  final state of a stage to the next; the problem file used to leave the
  `SDDPBlock` with no Solver, and the Block file refused it.
  `examples/instance-3` is such an instance, with 3 stages and 5 scenarios,
  solved with `-S BSPar.txt -B SDDPBCfg-LD.txt`; with `config/SDDPBSCfg.txt`
  in `IFCfg-SDDP.txt` the stages are solved via `LagrangianDualSolver`, and
  the components that must be hard and whose primal solution the
  `SDDPSolver` needs are written in `BSPar-LD.txt` and in the extra
  Configuration of `SDDPCfg.txt`

- `svm_solver` estimates the leave-p-out error on a sample of the subsets
  (-P), repeats an estimate with one seed per repetition (-r), writes the
  seconds and the score of each point of the grid on each split (--csv), asks
  for the bias regularised with the weights (-R), eliminates the features
  recursively with each round optionally reoptimized (-F), walks the grid
  along C keeping the Solver and reoptimizing from the previous point (-W),
  removes each fold from one model and puts it back, the Solver
  re-optimizing after each change (-u), and prints the wall-clock time of the
  whole selection and the sum of the times of its trainings; it starts no
  more workers than there are trainings to run

- `svm_solver/modelsel`, the computational study on the model selection of
  an SVM: the configuration of the Solvers compared, the download of the data
  sets, the campaign guarded by the load of the machine and the tables

- `tssb_solver/ldld`, the computational study on the nested and the
  recursive Lagrangian dual of a `TwoStageStochasticBlock`: a driver of its
  own (`ldld_bench`, which runs one `Solver` of a `BlockSolverConfig` per
  process and prints its bound, time, iterations and peak memory, with an
  optional primal recovery), the configuration of the six methods, the
  generation of the instances, the campaign guarded by the load of the
  machine and the figures and tables; it depends on nothing else in the
  tools and is built on its own, with CMake against an installation or with
  its makefile

- `-v 2` prints the parameters of the Solver attached to the Block and `-v 3`
  those of the Solver of the sub-Block as well, which is how a run says what
  it was actually asked, rather than what the configuration files seem to say

- `ucblock_solver/lagrangian_timing/rules_cost.py`, which says what the
  operating rules of a nuclear unit cost inside a Lagrangian decomposition:
  it reads the CSV that the timing harness writes over two dump sets of the
  same fleet, the same demand and the same regime, one carrying the whole set
  of the rules and one reduced to the original model, and reports, per
  Solver, the median and the worst solve, how many ended on the time limit
  rather than on the problem, and the factor between the two arms

- `tssb_solver` also solves a MultiStageStochasticBlock when the module is
  built, `-k` giving the Benders form over the leaves of its scenario tree

- `-R, --recover <file>` in `tssb_solver`: a feasible solution is recovered
  after a Solver that only gives a bound, e.g., a LagrangianDualSolver: the
  here-and-now Variable of every leaf are fixed to their mean over the
  leaves, rounded where they are integer, each leaf is solved alone with the
  BlockSolverConfig in `<file>`, and the value of the solution is printed
  with its gap to the bound; when the Solver ends without a primal solution
  (e.g., it failed) there is no mean to take, and the recovery says so
  instead of starting

- `-j, --threads <n>` in `tssb_solver`: the leaves of `-R`, independent once
  the design is fixed, are solved by `<n>` threads

- `-k, --benders` in `tssb_solver`: the Benders form of the problem is
  assembled around it [see TwoStageStochasticBlock::get_Benders_form()] and
  the BlockSolverConfig of `-S` is applied to its root, which is where a
  BendersDecompositionSolver is attached; the tool links the
  BendersDecompositionSolver when CMake finds it, the makefile does not

### Changed

- everything `investmentblock_solver` gives the `InvestmentBlock` and its
  `InvestmentFunction` comes from the configuration files, the tool only
  reading and applying them: the `OBlockConfig` of the `InvestmentBlock`
  (`config/IBOCfg.txt`, the `InvestmentBlock` entry of the "meta"-BlockConfig
  `InnerBCfg.txt`) reformulates the bounds on the investment and gives the
  `InvestmentFunction` its ComputeConfig (`config/IFCfg.txt`), with the file
  of the investment candidates and, in the extra Configuration, the
  BlockSolverConfig of the inner Block; the BlockSolverConfig of the
  `BundleSolver` no longer carries `strInnerBSC`, a parameter that was not
  one and that the tool took out before applying it, nor does the tool fall
  back to `BSCfg.txt`, set the file of the candidates, reformulate the
  bounds or build a default formulation of the inner Block in code. A Block
  file now needs a BlockConfig (`-B`, by default `InnerBCfg.txt`)

- the makefile asks for `-O3 -DNDEBUG` and nothing else, the macro of the
  patch for `boost::any` on macOS having no reason to be there since there is
  no `boost::any` left in the core

- the set of instances is named `pypsa-data`, as the folder that holds it

- the parameter that the feasibility cut of the Benders decomposition needs
  is in the configuration, commented where the Solver refuses it, so that a
  run that wants that cut is one line away instead of a search through the
  documentation

- the master of the Lagrangian dual asks Gurobi for its least numerical care
  and not for none of it, satisfies its own rows tighter than the oracle
  satisfies its own, and declares the residual zero on the scale of the
  model: the extra care costs at every one of the thousands of solves of a
  run, while what the master needs is to be solved consistently

- the configurations of the tools move to the parameter set of BundleSolver
  2.0, the Solver of the master being configured where the master is and not
  where the bundle is, and `Method` and `NumericFocus` counting among the
  integer parameters of that Solver

- a solve that the license service refuses is waited out and tried again, and
  a sweep that stops part way through says so and leaves no file that looks
  like a measurement: the units of a fleet differ from one another, so the
  fleet is timed whole or not at all

- the timing study reaches the nuclear units, and the MILP it compares
  against separates the Perspective Cuts, so that the two arms are the same
  model solved in two ways

### Removed

- the options `-l`, `-n`, `-r` and `-s` of `investmentblock_solver`, which had
  no effect: the cuts that `-l` named were never loaded, the number of
  sub-Blocks per stage of `-n` was never set, `-r` only relaxed the default
  formulation built in code, which is gone, and `-s` was read and never used;
  `-n` is again the standard option

### Fixed

- the `MPBCfg.txt` of `ucblock_solver`, `tssb_solver`, `sddp_solver` and
  `investmentblock_solver`, which every default configuration with a
  BundleSolver names, asked for Gurobi, so that these tools stopped at once
  where only HiGHS is there, e.g., in the conda packages: the master is now
  solved by HiGHS, Gurobi being one uncommented line away as the other
  :MILPSolver are

- `sddp_solver` solves an `SDDPBlock` whose scenarios come from a
  `ScenarioGenerator`, such as `examples/SDDPBlock-new.nc4`, also when the
  stages are solved by a `LagrangianDualSolver`: it set scenario 0 at every
  stage before attaching the Solver, i.e., before the pool of scenarios the
  `SDDPSolver` prepares when it is attached, and stopped with "invalid
  scenario index 0"; that was only there for `OSIMPSolver`, which
  `BundleSolver` no longer has, and it is gone

- `ucblock_solver` takes `LagrangianDualSolver` from its plain makefile, as
  the other tools do, rather than from the one that assumes the library was
  installed

- the duals of the pollutant constraints are read by pollutant and by zone
  again, a single index having mixed the zones of one pollutant with those of
  another in the output of the SDDP tool

- `print_status()` closes its parenthesis and goes to a new line whatever the
  status is, `kLowPrecision` and the ones below it having left the line open
  and run into what came next

- `ucblock_solver` and `svm_solver` set the log of their Solver as the other
  tools do, so that `intLogVerb` of a configuration is heard instead of being
  read and dropped

- the header of each file of the pollutant duals written by
  `smspp_sddp_solver` names the zones rather than repeating `Zone_0`

## [0.7.1] - 2026-09-14

### Fixed

- the link that carries the name a tool had before the prefix is made in the
  directory of the install and not in the one of the configure, so that
  `cmake --install --prefix` puts it next to the tool instead of failing on
  the directory of the machine

## [0.7.0] - 2026-09-13

### Changed

- the installed executables carry the name of the project, e.g.
  `smspp_ucblock_solver` and `smspp_chgcfg`, so that they are recognisable
  among all the others where they are installed; each of them is also
  installed under the name it had before, which is a link to it and which a
  later release will drop, and both names have their shell completions and
  their man page

## [0.6.0] - 2026-09-12

### Added

- the configurations that attach the dynamic programming Solver of the
  nuclear units inside `ucblock_solver` and inside the timing study
  (`NUBSCfg-DP.txt`), the Lagrangian chain of a unit commitment whose units
  are nuclear being otherwise that of the thermal ones

- `svm_solver`, a SVMBlock solver that trains a Support Vector Machine and
  performs the model selection around it: hold-out and k-fold
  cross-validation, both stratified, and grid search over the
  hyper-parameters

- `ml_utils.{h,cpp}`, the model-agnostic machine learning scaffolding that
  `svm_solver` uses, i.e., the splits, the scores and the grid; it depends on
  nothing but the standard library, so any tool training a model can use it

- every tool installs its configuration, its example instances and a man
  page generated from its --help; without -c, when none of its configuration
  files is found with its own prefix, it uses the installed configuration

- the --help of every tool follows the GNU layout, and describes its input
  file, how the configuration files are looked up, some examples and the
  exit status

- an empty name given to -B or -S, as in -B '', means no file at all

- `chgcfg` has --help, --version and a man page, as every other tool

- `mcfblock_solver`, `bkblock_solver`, `cflblock_solver`, `mmcfblock_solver`
  and `sfdcrblock_solver`, the solvers of MCFBlock, BinaryKnapsackBlock,
  CapacitatedFacilityLocationBlock, MMCFBlock and SingleFlowDCRBlock, which
  read their Block from an SMS++ netCDF file or from a native text format of
  it, chosen by -f

### Changed

- the default configurations of `ucblock_solver`, `tssb_solver` and
  `sddp_solver` solve with HiGHSMILPSolver, which needs no license

- the default -B of `ucblock_solver` is InnerBCfg.txt, which chooses the
  formulation of the units and of the network

- -n writes the problem on the file it is given, and -v takes its level only
  when attached, as in -v2 or --verbose=2, so that -v can precede the input
  file

- `svm_solver` takes the kernel from the command line with -K, one of linear,
  poly, gaussian, laplacian and sigmoid, which overrides the one the instance
  carries while keeping its parameters

- `svm_solver` reports the status and the bounds of the training problem in
  the format every other tool uses, and -O writes the trained model, i.e.,
  the `SVMBlockSolution`

- `svm_solver` trains the models of a model selection in parallel, the grid
  and the folds being a cartesian product of independent problems; -j says
  how many at a time

- the splits of `ml_utils` are specified down to the bit, rather than being
  left to the implementation-defined shuffle of the standard library, so that
  a seed gives the same splits everywhere and an experiment can be reproduced
  in any other language

- the PPH configuration of ucblock_solver solves the Lagrangian Dual of
  every proximal iteration to convergence, and follows the parameters of
  PrimalProximalHeur being now named after the algorithm they belong to

- the version of the module is the git tag of its repository, or the
  VERSION.txt of a release tarball, and the shared library carries it: its
  SONAME is major.minor while the major is 0, and it is installed with an
  RPATH relative to itself, so that an installed tree keeps working wherever
  it is moved

### Fixed

- `svm_solver` reads a netCDF SVMBlock again, and looks for the input file at
  the -p prefix whatever its format

- the tools report a missing input file and an unknown option on the
  standard error

- the shell completions of the tools name their tool, instead of nothing

## [0.5.4] - 2025-12-12

### Added

- support fir set\_solver\_log() in all solvers

- std::set_terminate() support in all solvers

- MPI support to the computation of InvestmentFunction

- support for  reading initial State and writing the final one

- support for dry runs

- support for reading initial Solution and writing the final one

### Changed

- all things that can be changed, and the common definitions, are
  now in makefile\_common to reduce code duplication within makefiles
  and to make adapting to one's environment quicker

- all *\_solver executables share the same baisc set of command-line
  options, then can add upon it

- updated block solver handling in CutProcessing

### Fixed

- consider UnitBlock scaling when outputting the solution

- get\_installed\_quantity in investment\_solver

## [0.5.3] - 2024-02-29

### Changed

- adapted to new CMake / makefile organisation

### Removed

- the -r option from sddp_solver

## [0.5.2] - 2023-05-17

### Added

- investment_solver keeps track of the best solution found.

- If the State file is not found, investment_solver shows a warning and
  proceeds.

- Save the best solution and the two most recent SolverState in
  investment_solver.

- Implement linear constraints in InvestmentFunction.

- InvestmentFunctionState.

### Changed

- Disable the computation of linearizations in InvestmentFunction in
  simulation mode.

### Fixed

- The initial up and downtimes of thermal units are only updated in
  investment_solver when a single scenario is being simulated.

- Linearization of InvestmentFunction.

## [0.5.1] - 2022-07-01

### Added

- The investment_solver tool to solve an InvestmentBlock.

- The chgcfg tool to change configuration files.

- Consecutive simulations to sddp_solver.

## [0.5.0] - 2021-12-08

### Added

- Multiple parameters to sddp_solver.

- MPI support to sddp_solver.

- Configuration of LagrangianDualSolver in sddp_solver.

### Fixed

- Output of UCBlock solution.

- Initial conditions for simulation in sddp_solver.

## [0.4.0] - 2021-02-05

### Added

- sddp_solver tool.

### Changed

- Block/ucblock/thermalunit solvers have now the same interface.

- Major review of project tree.

## [0.3.1] - 2020-09-28

### Fixed

- A bug in ucblock_solver that prevented configuration loading.

## [0.3.0] - 2020-09-16

### Added

- Support for new configuration framework.

## [0.2.0] - 2020-03-06

### Added

- Changelog.

### Fixed

- Minor fixes.

## [0.1.0] - 2020-01-06

### Added

- First test release.

[Unreleased]: https://gitlab.com/smspp/tools/-/compare/0.7.1...develop
[0.7.1]: https://gitlab.com/smspp/tools/-/compare/0.7.0...0.7.1
[0.7.0]: https://gitlab.com/smspp/tools/-/compare/0.6.0...0.7.0
[0.6.0]: https://gitlab.com/smspp/tools/-/compare/0.5.4...0.6.0
[0.5.4]: https://gitlab.com/smspp/tools/-/compare/0.5.3...0.5.4
[0.5.3]: https://gitlab.com/smspp/tools/-/compare/0.5.2...0.5.3
[0.5.2]: https://gitlab.com/smspp/tools/-/compare/0.5.1...0.5.2
[0.5.1]: https://gitlab.com/smspp/tools/-/compare/0.5.0...0.5.1
[0.5.0]: https://gitlab.com/smspp/tools/-/compare/0.4.0...0.5.0
[0.4.0]: https://gitlab.com/smspp/tools/-/compare/0.3.1...0.4.0
[0.3.1]: https://gitlab.com/smspp/tools/-/compare/0.3.0...0.3.1
[0.3.0]: https://gitlab.com/smspp/tools/-/compare/0.2.0...0.3.0
[0.2.0]: https://gitlab.com/smspp/tools/-/compare/0.1.0...0.2.0
[0.1.0]: https://gitlab.com/smspp/tools/-/tags/0.1.0
