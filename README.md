# SMS++ Tools

A set of tools and examples that use SMS++ library and other modules.
At the moment we provide:

- a generic Block Solver with some example input files
- a single Thermal Unit solver
- a UCBlock solver
- an SDDPBlock solver

## Getting started

These instructions will let you build SMS++ Tools on your system.

### Requirements

- [SMS++ core library](https://gitlab.com/smspp/smspp)
- [MILPSolver](https://gitlab.com/smspp/milpsolver)
- [SDDPBlock](https://gitlab.com/smspp/sddpblock)
- [UCBlock](https://gitlab.com/smspp/ucblock)

### Build and install with CMake

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

The Block solver (`block_solver`),
the Thermal Unit solver (`thermalunit_solver`) and 
the Unit Commitment solver (`ucblock_solver`) share the same interface:

```sh
Usage: <solver-name> [options] <nc4-file>

  -B <file>, --blockcfg <file>    Block configuration.
  -S <file>, --solvercfg <file>   Solver configuration.
  -n <file>, --nc4problem <file>  Write nc4 problem on file.
  -v, --verbose                   Make the solver verbose.
  -h, --help                      Print this help.
```

See the [`examples`](examples) directory for sample input files and configurations.

### Block solver

The input netCDF file can be a problem file or a block file:
- a problem file already contains a Block configuration and a Solver configuration,
  so if you provide them by command line they will be ignored;
- a block file needs a Block configuration and a Solver configuration to be solved.

See the [`examples`](examples) directory for sample input files and configurations.

### SDDPBlock Solver

```sh
Usage: sddp_solver [options] <nc4-file>

Options:
  -B <file>, --blockcfg <file>   Block configuration.
  -c <path>, --configdir <path>  The prefix for all config filenames.
  -h, --help                     Print this help.
  -i <index>, --scenario <index> The index of the scenario.
  -p <path>, --prefix <path>     The prefix for all Block filenames.
  -r, --relax                    Relax integer variables.
  -s, --simulation               Simulation mode.
  -S <file>, --solvercfg <file>  Solver configuration.
```

The input netCDF file can be a problem file or a block file:
- a problem file already contains a Block configuration and a Solver
  configuration; any Block or Solver configuration provided by command line
  will be ignored;
- for a block file, if a Block configuration or a Solver configuration is not
  provided, a default configuration will be used.

The `-c` option specifies the prefix to the paths to all configuration
files. The `-p` option specifies the prefix to the paths to all files
specified by the attribute `filename` in the input netCDF file.

The `-s` option indicates whether a simulation should be performed. If this
option is used, then the SDDPBlock is solved using the
SDDPGreedySolver. Otherwise, the SDDPBlock is solved by the SDDPSolver.

In simulation mode (i.e., when the `-s` option is used), the `-i` option
specifies the index of the scenario for which the problem must be solved. The
index must be a number between 0 and n-1, where n is the number of scenarios
in the SDDPBlock. If this index is not provided, then the problem is solved
for the first scenario. Also in simulation mode, the `-r` option indicates
that the integrality constraints over the variables must be relaxed.

### Thermal Unit solver / Unit Commitment solver

The input netCDF file must be a block file. If you don't provide Block
or Solver configurations, default configurations will be used.

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

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.

## Disclaimer

The code is currently provided free of charge under an open-source license.
As such, it is provided "*as is*", without any explicit or implicit warranty
that it will properly behave or it will suit your needs. The Authors of
the code cannot be considered liable, either directly or indirectly, for
any damage or loss that anybody could suffer for having used it. More
details about the non-warranty attached to this code are available in the
license description file.
