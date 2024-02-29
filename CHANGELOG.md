# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added 

### Changed 

### Fixed 

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

- The initial up and down times of thermal units are only updated in
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

[Unreleased]: https://gitlab.com/smspp/tools/-/compare/0.5.3...develop
[0.5.3]: https://gitlab.com/smspp/tools/-/compare/0.5.2...0.5.3
[0.5.2]: https://gitlab.com/smspp/tools/-/compare/0.5.1...0.5.2
[0.5.1]: https://gitlab.com/smspp/tools/-/compare/0.5.0...0.5.1
[0.5.0]: https://gitlab.com/smspp/tools/-/compare/0.4.0...0.5.0
[0.4.0]: https://gitlab.com/smspp/tools/-/compare/0.3.1...0.4.0
[0.3.1]: https://gitlab.com/smspp/tools/-/compare/0.3.0...0.3.1
[0.3.0]: https://gitlab.com/smspp/tools/-/compare/0.2.0...0.3.0
[0.2.0]: https://gitlab.com/smspp/tools/-/compare/0.1.0...0.2.0
[0.1.0]: https://gitlab.com/smspp/tools/-/tags/0.1.0
