# Long-dual timing campaign

Warm single-unit re-solve times behind Table `tab:timing` of the TUDPS paper.

The Lagrangian costs come from dumps taken along a 2000-iteration run of the
Lagrangian dual on the `sb_50u_*_seed1` single-bus instances, so the multipliers
are far from zero and the single-unit problems are as hard as they get inside a
decomposition. Short dual runs give reserve columns several times cheaper, which
is why these numbers do not match earlier campaigns.

One CSV per dump set, `<horizon>_<reserve>_<reactive>.csv`, with one row per
(unit, iteration, solver): `time_us` is the measured time and `phase` tells the
first, cold solve from the warm re-solves that follow. Only the warm rows belong
in the table.

`campaign.log` records, for every set, the repetition count, the dump-wave
stride and the machine load at the start and at the end of the measurement. The
machine is shared: a set measured while somebody else floods it is worthless,
so the runner refused to start below a load threshold and discarded any result
whose load had blown up by the end.

To reproduce, point `timing_harness` at a directory of `TUB-<unit>-<iter>.nc4`
dumps holding a `config/` link, e.g.

    timing_harness <dumpdir> <dp_reps> <stride> <milp_reps>

The MILP runs under the 300 s limit of `BSCfg.txt` and on one core, as the DPs
do; at the monthly horizon with reserves priced a third of its solves reach that
limit without proving optimality, so those times are lower bounds.
