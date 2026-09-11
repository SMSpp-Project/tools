# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added 

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
- Consecutive simulations to sddp_solver.
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

[Unreleased]: https://gitlab.com/smspp/tools/-/compare/0.5.4...develop
[0.5.4]: https://gitlab.com/smspp/tools/-/compare/0.5.3...0.5.3
[0.5.3]: https://gitlab.com/smspp/tools/-/compare/0.5.2...0.5.3
[0.5.2]: https://gitlab.com/smspp/tools/-/compare/0.5.1...0.5.2
[0.5.1]: https://gitlab.com/smspp/tools/-/compare/0.5.0...0.5.1
[0.5.0]: https://gitlab.com/smspp/tools/-/compare/0.4.0...0.5.0
[0.4.0]: https://gitlab.com/smspp/tools/-/compare/0.3.1...0.4.0
[0.3.1]: https://gitlab.com/smspp/tools/-/compare/0.3.0...0.3.1
[0.3.0]: https://gitlab.com/smspp/tools/-/compare/0.2.0...0.3.0
[0.2.0]: https://gitlab.com/smspp/tools/-/compare/0.1.0...0.2.0
[0.1.0]: https://gitlab.com/smspp/tools/-/tags/0.1.0
