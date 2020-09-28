# SMS++ Tools

A set of tools and examples that use SMS++ library and other modules.
At the moment we provide:

- a generic Block Solver with some example input files
- a single Thermal Unit solver
- a UCBlock solver
- an SDDPBlock greedy (simulation) solver

## Getting started

These instructions will let you build SMS++ Tools on your system.

### Requirements

- SMS++
- MILPSolver
- SDDPBlock
- UCBlock

### Build and install

Configure and build with:

```sh
mkdir build
cd build
cmake ..
make
```

Optionally, install with:

```sh
make install
```

## Usage

### Block solver

```sh
Usage: block_solver [options] <nc4-file>

Options:
  -b <file>, --blockcfg <file>   Block configuration.
  -s <file>, --solvercfg <file>  Solver configuration.
  -h, --help                     Print this help.
```

The input netCDF file can be a problem file or a block file:
- a problem file already contains a Block configuration and a Solver configuration,
  so if you provide them by command line they will be ignored;
- a block file needs a Block configuration and a Solver configuration to be solved.

See the [`examples`](examples) directory for sample input files.

### SDDPBlock Greedy Solver

```sh
Usage: sddp_greedy_solver [options] <nc4-file>

Options:
  -i <index>, --scenario <index> The index of the scenario.
  -b <file>, --blockcfg <file>   Block configuration.
  -s <file>, --solvercfg <file>  Solver configuration.
  -h, --help                     Print this help.
```

The input netCDF file can be a problem file or a block file:
- a problem file already contains a Block configuration and a Solver
  configuration; any Block or Solver configuration provided by command line
  will be ignored;
- for a block file, if a Block configuration or a Solver configuration is nor
  provided, a default configuration will be used.

The -i option specifies the index of the scenario for which the problem must
be solved. The index must be a number between 0 and n-1, where n is the number
of scenarios in the SDDPBlock. If this index is not provided, then the problem
is solved for the first scenario.

### Thermal Unit solver

```sh
Usage: thermalunit_solver [options] <nc4-file>

Options:
  -s <solver>, --solver <solver>  Choose solver.
                                  Available solvers are: cplex, dp.
  -w <file>, --writelp <file>     Write LP problem on file.
  -n <file>, --nc4problem <file>  Write nc4 problem on file.
  -h, --help                      Print this help.
```

The input netCDF file must be a block file.
At the moment, DP solver support is limited.

### Unit Commitment Block solver

```sh
Usage: ucblock_solver [options] <nc4-file>

Options:
  -B <file>, --blockcfg <file>    Block configuration.
  -S <file>, --solvercfg <file>   Solver configuration.
  -s <solver>, --solver <solver>  Choose solver.
                                  Available solvers are: cplex, dp.
  -w <file>, --writelp <file>     Write LP problem on file.
  -v, --verbose                   Make the solver verbose.
  -h, --help                      Print this help.
```

The input netCDF file must be a block file.
At the moment, DP solver support is limited.

## Authors

- **Antonio Frangioni**  
  *Operations Research Group*  
  Dipartimento di Informatica  
  Università di Pisa

- **Ali Ghezelsoflu**  
  *Operations Research Group*  
  Dipartimento di Informatica  
  Università di Pisa

- **Niccolò Iardella**  
  *Operations Research Group*  
  Dipartimento di Informatica  
  Università di Pisa

- **Rafael Durbano Lobato**  
  *Operations Research Group*  
  Dipartimento di Informatica  
  Università di Pisa

## License

See SMS++ library for details.

## Disclaimer

The code is currently provided free of charge for academic purposes only.
As such, it is provided "*as is*", without any explicit or implicit warranty
that it will properly behave or it will suit your needs. The Authors of
the code cannot be considered liable, either directly or indirectly, for
any damage or loss that anybody could suffer for having used it. More
details about the non-warranty attached to this code are available in the
license description file.
