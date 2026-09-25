# The model selection of an SVM

This directory holds the computational study on the operations of model
selection of a Support Vector Machine, i.e., on what a training costs when it
is one of the many that an estimate of the error, a search over the
hyper-parameters or an elimination of the features requires, and on how much
of that cost the re-optimization of `SMOSolver` saves with respect to the
Solvers that restart from scratch. It uses the `svm_solver` tool and nothing
else of the tools.

## What is compared

Each operation is done in each of the ways the Solvers allow:

| operation | ways |
|---|---|
| one training | `SMOSolver`, `LIBSVMSolver`, `LIBLINEARSolver` (linear kernel, regularised bias), `GRBMILPSolver` on the Wolfe dual (small data sets) |
| k-fold cross-validation, leave-one-out | each fold trained from scratch, removed from one model and put back with the Solver re-optimizing (`-u`), or unlearnt out of one model along the exact solution path (`-i`) |
| leave-p-out | the same, on a sample of the subsets (`-P`, `-r`) |
| grid search | each point from scratch or re-optimizing along C (`-W`), each fold trained, removed and put back (`-u`) or unlearnt (`-i`, small data sets only) |
| elimination of the features | each round from scratch, or re-optimized from the previous one (`-F`, `-i`) |
| parallelism | the grid search on 1, 8 and 64 cores (`-j`) |

All the Solvers are given the same tolerance, 1e-3 on the optimality
conditions of the dual, and LIBSVM a kernel cache of the size the `SVMBlock`
gives its own (`dblLSVMCache 0`), so that the memory is the same: see
`config/`.

## How to run it

    ./get-data ~/svmms/data
    DATA=~/svmms/data ./run-campaign ~/svmms/runs [section ...]
    python3 tables.py ~/svmms/runs/raw.tsv

The sections are `single`, `cv`, `lpo`, `grid`, `rfe` and `par`. The
campaign waits for the machine to be idle before each run, records the load
before and after it, repeats each run on the small and medium data sets so
that the noise can be measured by comparing a variant with itself, and skips
the runs already in `raw.tsv`, so that it can be stopped and resumed. The
time of a run with one worker is the sum of its trainings and walks, which
leaves out the reading of the data set and the start of the thread pool; with
more workers it is the wall clock of the whole selection.

`datasets.txt` lists the data sets of the LIBSVM repository used, with the
tier that decides which operations are affordable on each of them.
