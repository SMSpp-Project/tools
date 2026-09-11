# SMS++ Tools

A set of tools and examples that use SMS++ library and other modules.
At the moment we provide:

- `block_solver`: a generic Block solver with some example input files

- `ucblock_solver`: a UCBlock solver

- `sddp_solver`: an SDDPBlock solver

- `tssb_solver`: a TwoStageStochasticBlock solver

- `mssb_solver`: a MultiStageStochasticBlock solver

- `investmentblock_solver`: an InvestmentBlock solver

- `svm_solver`: a SVMBlock solver, i.e., a Support Vector Machine trainer,
  together with the model selection that surrounds the training: hold-out and
  k-fold cross-validation, both stratified, and grid search over the
  hyper-parameters

- `chgcfg`: a small utility to change some parameters in a configuration
  file while leaving all the rest unchanged


## Getting started

These instructions will let you build SMS++ Tools on your system.

### Requirements

- [SMS++ core library](https://gitlab.com/smspp/smspp)

- [InvestmentBlock](https://gitlab.com/smspp/investmentblock)

- [MILPSolver](https://gitlab.com/smspp/milpsolver)

- [SDDPBlock](https://gitlab.com/smspp/sddpblock) and its
  dependencies

- [TwoStageStochasticBlock](https://gitlab.com/smspp/twostagestochasticblock)

- [MultiStageStochasticBlock](https://gitlab.com/smspp/multistagestochasticblock)

- [SVMBlock](https://gitlab.com/smspp/svmblock)

- [UCBlock](https://gitlab.com/smspp/ucblock)

### Build and install with CMake

Configure and build with:

```sh
mkdir build
cd build
cmake ..
cmake --build .
```

Optionally, install with:

```sh
make install
```

### Build and install with makefile

Some (but not all) the modules have a hand-made makefile that can be
manually edited and then used with just

```sh
cmake --build .
```


## Usage

All the `*_solver` share a large part of the command-line interface:

The Block solver (`block_solver`) and the Unit Commitment solver (`ucblock_solver`)
share the same interface:

```sh
Usage: <*_solver> [options] <file>
  or:  <*_solver> -h | --help
  or:  <*_solver> -V | --version

Options:
  -h, --help                      print this help
  -V, --version                   print the SMS++ tools version and exit
  -a, --save-state <file>         save State of the Solver
  -B, --blockcfg <file>           Block Configuration
  -b, --load-state <file>         load State for the Solver
  -p, --prefix <path>             the prefix for all Block filenames
  -S, --solvercfg <file>          Solver Configuration
  -c, --configdir <path>          the prefix for all Config filenames
  -I, --inputsol <file>           input Solution
  -O, --outputsol <file>          output Solution
  -C, --outsolcfg <file>          output Solution Configuration
  -o, --output-solution           output the solutions
  -n, --nc4problem <file>         write nc4 problem on file
  -D, --dryrun                    skip the compute() call
  -v, --verbose[=N]               verbose output (0 = silent, 1 = basic, 2 = debug)
```

See the [`examples`](ucblock_solver/examples) directory for sample
input files and configurations.

When the `-B` and/or `-S` options are not given on the command line, each
solver falls back to its own conventional default Configuration files, looked
up relative to the `-c` prefix. For `sddp_solver`, `tssb_solver` and
`mssb_solver` the `-c` prefix itself defaults to `config/`, so those need no
`-c` at all when run from the tool directory. The per-tool defaults are:

| tool                     | default `-B`                          | default `-S`   |
|--------------------------|---------------------------------------|----------------|
| `sddp_solver`            | `SDDPBCfg.txt` (or `SDDPBCfg-LD.txt`) | `SDDPSCfg.txt` |
| `tssb_solver`            | `TSSBCfg.txt`                         | `TSSBSCfg.txt` |
| `mssb_solver`            | `InnerBCfg.txt`                       | `MSSBSCfg.txt` |
| `investmentblock_solver` | `InnerBCfg.txt`                       | `BSPar.txt`    |
| `ucblock_solver`         | `InnerBCfg.txt`                       | `BSCfg.txt`    |
| `block_solver`           | `BCfg.txt`                            | `BSCfg.txt`    |

For `sddp_solver` the default `-B` is applied through the inner-Block "meta"
BlockConfig (`SDDPBCfg.txt`, or `SDDPBCfg-LD.txt` when the SDDPSolver drives a
LagrangianDualSolver), which linearises the `PolyhedralFunctionBlock` and
shapes the network/units; passing a generic `BCfg.txt` instead is wrong and
makes the MILPSolver throw "Unknown type of Objective Function". The fallback
applies only when the file is actually reachable, so a missing default is
silently ignored, and explicit `-B`/`-S` always take precedence; an empty
name, as in `-B ''`, means no file at all.

The `-c` prefix is applied to *every* Configuration filename, not only the
top-level `-B`/`-S` files but also every filename referenced from inside a
config file: the `*filename` include mechanism and the `strInnerBSC` /
`str_LagBF_BSCfg` meta-config chains. A run from any working directory therefore
resolves every config relative to `-c`: point `-c` at the directory holding the
`.txt` files and the tool needs no particular current directory (this is what
lets an external driver such as pySMSpp invoke the tool from an arbitrary
location). Consequently, filenames referenced from inside a config file are
written *bare* (e.g. `*PFBCfg.txt`, `strInnerBSC UCBSCfg.txt`), without a
`config/` component, since `-c` supplies the base directory.

Every tool with a `config/` directory installs it, under
`share/SMS++_tools/<tool>/config`, and uses it when `-c` is not given and
none of its Configuration files is found with its own prefix: the whole
configuration then comes from the installed directory, which the tool finds
relative to its executable, so an installed tool runs from anywhere with no
option at all. A file in the current directory, or an explicit `-c`, always
takes precedence; `--help` prints the installed directory, and `-v` which
files are in use. `--help` also lists the exit status: 0 when the run
completes, whatever the status of the Solvers, nonzero on errors. Next to its
configuration each tool installs its example instances, under
`share/SMS++_tools/<tool>/examples`, and a man page generated from its
`--help` when help2man is available.

### Quick start (run the bundled example)

From each tool's own directory, a plain run on the bundled example is:

```sh
cd sddp_solver            && sddp_solver SDDPBlock.nc4 -p examples/
cd tssb_solver            && tssb_solver examples/toy_tssb.nc4
cd mssb_solver            && mssb_solver examples/big_baked_L8.nc4
cd ucblock_solver         && ucblock_solver examples/Bus_Test.nc4 -c config/
cd investmentblock_solver && investmentblock_solver InvestmentBlockBus.nc4 -c config/ -p examples/
```

`block_solver` is generic: it needs an explicit `-B`/`-S` matching the Block in
the file, e.g. `block_solver <file>.nc4 -B <BlockConfig> -S <BlockSolverConfig>`.

### Block solver

The input netCDF file can be a problem file or a Block file:

- a problem file already contains a Block configuration and a Solver
  configuration, so if you provide them by command line they will be ignored;

- a Block file is solved with the given Solver configuration (or the default
  `BSCfg.txt`); the Block configuration is optional, the Block being solved as
  deserialized when none is given.

See the [`examples`](ucblock_solver/examples) directory for sample input
files and configurations.

### InvestmentBlock Solver

`investmentblock_solver` adds the following command-line options to the basic
ones:

```sh
  -l, --load-cuts <file>          load cuts from a file
  -n, --num-blocks <number>       number of sub-Blocks per stage
  -r, --relax                     relax integer variables
  -s, --simulate                  simulate the given investment
  -x, --initial-investment <file> initial investment
```

The input netCDF file can be a problem file or a block file:

- a problem file already contains a Block configuration and a Solver
  configuration; any Block or Solver configuration provided by command line
  will be ignored;

- for a block file, if a Block configuration or a Solver configuration is not
  provided, a default configuration will be used.

The `-c` option specifies the prefix to the paths to all configuration
files. This means that if PATH is the value passed to the `-c` option, then
the name (or path) to each configuration file will be prepended by
PATH. The `-p` option specifies the prefix to the paths to all files
specified by the attribute "filename" in the input netCDF file.

It is possible to provide an initial point (initial solution or initial
investment) through the `-x` option. This option must be followed by a file
containing the initial point. If there are N assets subject to investment,
then this file must contain N numbers, where the i-th number is the initial
value for the investment in the i-th asset. If this option is not used,
then the initial value x_i for the investment in the i-th asset is
determined as follows. If the lower bound l_i on the i-th investment is
finite, then x_i = l_i. Otherwise, if the upper bound u_i on the i-th
investment is finite, then x_i = u_i. Otherwise, if both bounds are not
finite, then x_i = 0.

To simulate a given investment, i.e., to compute the investment function at a
given point, the `-s` option must be used. The investment to be simulated is
given by the initial point as described above: a given point provided by the
`-x` option or the default initial point.

If the `-o` option is used, then part of the primal and dual solutions of
every UCBlock for each scenario is output while the investment function is
computed. Typically, one may want the solutions to be output in simulation
mode (i.e., when the `-s` option is used).

The `-n` option specifies the number of sub-Blocks of SDDPBlock that must be
constructed for each stage. By default, SDDPBlock contains a single
sub-Blocks for each stage. This option must be provided in order to solve
multiple scenarios in parallel. In this case, the number of scenarios that
are solved in parallel is n (assuming n is not larger than the number of
scenarios).

The `-B` and `-S` options are only considered if the given netCDF file is a
BlockFile. The `-B` option specifies a BlockConfig file to be applied to every
InvestmentBlock; while the `-S` option specifies a BlockSolverConfig file for
every InvestmentBlock. If the `-B` option is not provided when the given
netCDF file is a BlockFile, then a default configuration is considered.

Initial cuts can be provided by using the `-l` option. This option must be
followed by the path to the file containing the initial cuts. This file
must have the following format. The first line contains a header and its
content is ignored. Each of the following lines represent a cut and has the
following format:

    t, a_0, a_1, ..., a_k, b

where 't' is a stage (an integer between 0 and time horizon minus 1), 'a_0',
..., 'a_k' are the coefficients of the cut, and 'b' is the constant term of
the cut.

The `-r` option relaxes the integrality of every integer variable in the
inner Block (typically the binary commitment variables of every
ThermalUnitBlock), so that the investment function is evaluated over the
LP relaxation of the operational problem.

There are a few ways to specify the initial state for the first stage
subproblem. This can be done by setting the initial state variable of
SDDPBlock or by setting the initial state parameter of SDDPGreedySolver.

### SDDPBlock Solver

`sddp_solver` adds the following command-line options to the basic ones:

```sh
  -d, --output-dir                directory where solutions are written
  -e, --eliminate-redundant-cuts  eliminate given redundant cuts
  -l, --load-cuts <file>          load cuts from a file
  -n, --num-blocks <number>       number of sub-Blocks per stage
  -i, --scenario <index>          the index of the scenario
  -m, --num-simulations <number>  number of simulations to be performed
  -s, --simulation                simulation mode
  -t, --stage <stage>             stage from which initial state is taken
```

The input netCDF file can be a problem file or a block file:

- a problem file already contains a Block configuration and a Solver
  configuration; any Block or Solver configuration provided by command line
  will be ignored;

- for a block file, if a Block configuration or a Solver configuration is not
  provided, a default configuration will be used.

The `-c` option specifies the prefix to the paths to all configuration
files. This means that if PATH is the value passed to the `-c` option, then
the name (or path) to each configuration file will be prepended by PATH. The
`-p` option specifies the prefix to the paths to all files specified by the
attribute "filename" in the input netCDF file.

The `-s` option indicates whether a simulation must be performed. If this
option is used, then the SDDPBlock is solved using the
SDDPGreedySolver. Otherwise, the SDDPBlock is solved by the SDDPSolver.

In simulation mode (i.e., when the `-s` option is used), the `-i` option
specifies the index of the scenario for which the problem must be solved. The
index must be a number between 0 and n-1, where n is the number of scenarios
in the SDDPBlock. If this index is not provided, then the problem is solved
for the first scenario. Also in simulation mode, the `-m` option indicates
that consecutive simulations must be performed. Consecutive simulations are
simulations which are launched in sequence, one after the other, and which are
linked by the storage levels. The final state of some stage of a simulation is
used as the initial state for the next simulation. See the comments below for
more details. If the value NUMBER provided by this option is greater than 1,
then NUMBER consecutive simulations are performed.

The `-n` option specifies the number of sub-Blocks of SDDPBlock that must be
constructed for each stage. By default, SDDPBlock contains a single sub-Blocks
for each stage. This option must be provided in order to solve multiple
scenarios in parallel. In this case, the number of scenarios that are solved
in parallel is n (assuming n is not larger than the number of scenarios).

The `-B` and `-S` options are only considered if the given netCDF file is a
BlockFile. The `-B` option specifies a BlockConfig file to be applied to every
SDDPBlock; while the `-S` option specifies a BlockSolverConfig file for every
SDDPBlock. If each of these options is not provided when the given netCDF file
is a BlockFile, then default configurations are considered.

Initial cuts can be provided by using the `-l` option. This option must be
followed by the path to the file containing the initial cuts. This file must
have the following format. The first line contains a header and its content is
ignored. Each of the following lines represent a cut and has the following
format:

```
t, a_0, a_1, ..., a_k, b
```

where `t` is a stage (an integer between 0 and time horizon minus 1), `a_0`,
..., `a_k` are the coefficients of the cut, and `b` is the constant term of
the cut.

As a preprocessing, given redundant cuts can be removed by using the `-e`
option. Notice that all cuts will be subject to being removed, whether they
are provided in a netCDF file or by the `-l` option.

There are a few ways to specify the initial state for the first stage
subproblem. This can be done by setting the initial state variable of
SDDPBlock or by setting the initial state parameter of SDDPSolver or
SDDPGreedySolver. When running multiple simulations (when both the `-s` and
`-m` options are used), there is an additional way to specify the initial
state. The (final) state of some stage from a simulation can be used as the
initial state for the first stage of the next simulation. The stage at which
the state can be taken to serve as the initial state for the next simulation
can be specified by the `-t` option. This option must be followed by an
integer number STAGE. If STAGE is between 0 and T-1, where T is the time
horizon of the problem, then the solution (final state) of the subproblem
associated with stage STAGE of a simulation will serve as the initial state
for the first stage subproblem of the next simulation. If STAGE does not
belong to that interval (that is, if it is negative or greater than or equal
to T) or if the `-t` option is not used, then no changes are made to the way
the initial state is specified.

### Unit Commitment solver

`ucblock_solver` does not add any tool-specific command-line option to the
basic ones. The input netCDF file may be either a Block file or a problem
file, of which only the Block is used, the configuration always coming from
`-B` and `-S`. The default configuration solves the UCBlock with
HiGHSMILPSolver, and its `InnerBCfg.txt` chooses the formulation of the units
and of the network, which is needed, e.g., by the instances whose objective
contains a PolyhedralFunction; `-B ''` keeps the formulation of the file.

### TwoStageStochasticBlock solver

`tssb_solver` does not add any command-line options to the basic ones.

### MultiStageStochasticBlock solver

`mssb_solver` does not add any command-line options to the basic ones.

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
`in-cfg`, and any existing content in the file is deleted.

Then, an arbitrary number of `par-i val-i` pairs is allowed: each `par-i`
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
`chgcfg.cpp`; if activated, the produced configuration file will be
stripped by all non-necessary comments and comment lines.


## Getting help

If you need support, you want to submit bugs or propose a new feature, you can
[open a new issue](https://gitlab.com/smspp/tools/-/issues/new).


## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md) for details on our code of
conduct, and the process for submitting merge requests to us.


## Authors

### Current Lead Authors

- **Antonio Frangioni**  
  Dipartimento di Informatica  
  Università di Pisa

- **Ali Ghezelsoflu**  
  Dipartimento di Informatica  
  Università di Pisa

- **Niccolò Iardella**  
  Dipartimento di Informatica  
  Università di Pisa

- **Rafael Durbano Lobato**  
  Dipartimento di Informatica  
  Università di Pisa

### Contributors


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
