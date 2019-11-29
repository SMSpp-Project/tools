# SMS++ Tools

A set of tools and examples that use SMS++ library.
At the moment we provide only a generic block solver with some example input files.

## Getting started

These instructions will let you build SMS++ Tools on your system.

### Requirements

- SMS++

At the moment, the block solver also requires:

- UCBlock
- MILPSolver

### Build and install

Configure and build with:
```sh
mkdir build
cd build
cmake ..
make
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
## Authors

- **Antonio Frangioni**  
  *Operations Research Group*  
  Dipartimento di Informatica  
  Università di Pisa

- **Niccolò Iardella**  
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
