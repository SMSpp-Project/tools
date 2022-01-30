# SMS++ Tools

A set of tools and examples that use SMS++ library and other modules.
At the moment we provide:

- a generic Block Solver with some example input files
- a single Thermal Unit solver
- a UCBlock solver
- an SDDPBlock solver
- a small utility to change some parameters in a configuration
  file while leaving all the rest unchanged


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

### Build and install with makefile

Some (but not all) the modules have a hand-made makefile that can be
manually edited and then used with just

```sh
make
```


## Usage

The Block solver (`block_solver`), the Thermal Unit solver
(`thermalunit_solver`) and the Unit Commitment solver (`ucblock_solver`)
share the same interface:

```sh
Usage: <solver-name> [options] <nc4-file>

  -B <file>, --blockcfg <file>    Block configuration.
  -S <file>, --solvercfg <file>   Solver configuration.
  -n <file>, --nc4problem <file>  Write nc4 problem on file.
  -v, --verbose                   Make the solver verbose.
  -h, --help                      Print this help.
```

See the [`examples`](thermalunit_solver/examples) directory for sample
input files and configurations.

### Block solver

The input netCDF file can be a problem file or a Block file:

- a problem file already contains a Block configuration and a Solver
  configuration, so if you provide them by command line they will be ignored;

- a Block file needs a Block configuration and a Solver configuration to be
  solved.

See the [`examples`](thermalunit_solver/examples) directory for sample input
files and configurations.

### SDDPBlock Solver

```sh
Usage: sddp_solver [options] <nc4-file>

Options:
  -B <file>, --blockcfg <file>       Block configuration.
  -c <path>, --configdir <path>      The prefix for all config filenames.
  -e, --eliminate-redundant-cuts     Eliminate given redundant cuts.
  -h, --help                         Print this help.
  -i <index>, --scenario <index>     The index of the scenario.
  -l <file>, --load-cuts <file>      Load cuts from a file.
  -n <number>, --num-blocks <number> Number of sub-Blocks per stage.
  -p <path>, --prefix <path>         The prefix for all Block filenames.
  -r, --relax                        Relax integer variables.
  -s, --simulation                   Simulation mode.
  -S <file>, --solvercfg <file>      Solver configuration.
```

The input netCDF file can be a problem file or a Block file:

- a problem file already contains a Block configuration and a Solver
  configuration; any Block or Solver configuration provided by command line
  will be ignored;

- for a Block file, if a Block configuration or a Solver configuration is not
  provided, a default configuration will be used.

The `-c` option specifies the prefix to the paths to all configuration
files. The `-p` option specifies the prefix to the paths to all files
specified by the attribute `filename` in the input netCDF file.

The `-s` option indicates whether a simulation should be performed. If this
option is used, then the SDDPBlock is solved using the SDDPGreedySolver.
Otherwise, the SDDPBlock is solved by the SDDPSolver.

The `-n` option specifies the number of sub-Blocks of SDDPBlock that must be
constructed for each stage.

In simulation mode (i.e., when the `-s` option is used), the `-i` option
specifies the index of the scenario for which the problem must be solved. The
index must be a number between 0 and n-1, where n is the number of scenarios
in the SDDPBlock. If this index is not provided, then the problem is solved
for the first scenario. Also in simulation mode, the `-r` option indicates
that the integrality constraints over the variables must be relaxed.

Initial cuts can be provided by using the `-l` option. This option must be
followed by the path to the file containing the initial cuts. This file must
have the following format. The first line contains a header and its content is
ignored. Each of the following lines represent a cut and has the following
format:

t, a_0, a_1, ..., a_k, b

where t is a stage (an integer between 0 and time horizon - 1), a_0, ...,
a_k are the coefficients of the cut, and b is the constant term of the cut.

As a preprocessing, given redundant cuts can be removed by using the `-e`
option. Notice that all cuts will be subject to being removed, whether they
are provided in a netCDF file or by the `-l` option.

### Thermal Unit solver / Unit Commitment solver

The input netCDF file must be a Block file. If you don't provide Block
or Solver configurations, default configurations will be used.

### The `chgcfg` utility

```sh
Usage: chgcfg in-cfg out-cfg [ par1 val1 [ par2 val2 [ ... ] ] ]
```

`in-cfg` is the input configuration file. The assumptions are:

- `#` is the comment character, and everything from it to the end of the
  line is comment

- each pair < parameter , value > in the file is at the beginning of a
  separate line, possibly with trailing whitespaces and followed by comment

`out-cfg` is the output configuration file; it must be different from
`in_cfg`, and any existing content in the file is deleted.

Then, an aritrary number of `par-i val-i` pairs is allowed: each `par-i`
is checked against the existing parameters in `in-cfg`, and if it is found
the value `val-i` is put in `out-cfg` following `par-i`, replacing whatever
is there in `in-file`. Anything that does not contain any of the `par-i` is
copied over unchanged (save possibly for the comments, see below). Note that
each `par-i` *is only replaced once*. That is, the first time (scanning the
file from the beginning to the end) that the parameter is found it is put in
the output file with the replaced value, but from then on that `par-i` is
ignored. This allows to replace parameters that occur multiple times in the
original file by specifying them multiple times in the command line: the
first command-line copy modifies the first occurrence in the file and so on.

The module has a compile-time option, commanded by the macro BAREBONES in
`chgcfg.cpp`; if activatwd, the produced configuration file will be
stripped by all non-necessary comments and comment lines.



## Getting help

If you need support, you want to submit bugs or propose a new feature, you can
[open a new issue](https://gitlab.com/smspp/tools/-/issues/new).


## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md) for details on our code of
conduct, and the process for submitting merge requests to us.


## Authors

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Universita' di Pisa

- **Ali Ghezelsoflu**  
  Dipartimento di Informatica  
  Universita' di Pisa

- **Niccolo' Iardella**  
  Dipartimento di Informatica  
  Universitaè di Pisa

- **Rafael Durbano Lobato**  
  Dipartimento di Informatica  
  Universita' di Pisa


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
