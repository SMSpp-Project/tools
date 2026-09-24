"""Three-stage instances: the thermal fleet of gen_thermal_tssb.py under a
two-level scenario tree.

    python gen_thermal_tree.py --units 80 --snapshots 96 --climates 3 --demands 3

The outer stage is the climate year, which scales the availability of the
renewables, and the inner stage is the demand, whose realizations depend on
the outer branch both in value and in conditional probability: the drier the
year, the higher the demand and the more it leans towards its high
realizations. The here-and-now decision is the solar capacity, the fleet and
the rest of the data are those of gen_thermal_tssb.py with the same arguments
and seed.

It writes, in ../instances,
  - <name>_flat.nc, the equivalent flat network, one scenario per leaf with
    the joint probability P(c) P(d|c): as long as the only here-and-now
    decisions are the design ones, its extensive form is that of the tree,
    so that it is both an exact reference and the TwoStageStochasticBlock the
    tree is compared with (emit_thermal_tssb.py);
  - <name>_tree.json, the tree (groups of leaves and their conditional
    probabilities), which emit_thermal_mssb.py turns, together with the flat
    network, into a MultiStageStochasticBlock.
"""

import argparse
import json
from pathlib import Path

import numpy as np

import gen_thermal_tssb as base

HERE = Path(__file__).resolve().parent
DATA = HERE.parent / "instances"


def climate_axis(number):
    """The outer stage: availability multipliers, the dry year first."""
    if number == 1:
        return np.array([1.0]), np.array([1.0])
    availability = np.linspace(1.20, 0.75, number)
    weights = 1.0 + np.cos(np.linspace(-np.pi, np.pi, number + 2)[1:-1])
    return availability, weights / weights.sum()


def demand_axis(number, climate, number_climates):
    """The inner stage, conditional on the climate."""
    # the shift grows along the climate axis, from the wet year to the dry one
    shift = 0.05 * (2.0 * climate / max(number_climates - 1, 1) - 1.0)
    if number == 1:
        return np.array([1.0 + shift]), np.array([1.0])
    demand = np.linspace(0.92, 1.10, number) + shift
    # a skew that changes sign along the climate axis, so that no two outer
    # branches share the same conditional distribution
    skew = np.linspace(-1.0, 1.0, number) * shift * 10.0
    weights = np.clip(1.0 + skew, 0.05, None)
    return demand, weights / weights.sum()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--units", type=int, default=20)
    parser.add_argument("--snapshots", type=int, default=48)
    parser.add_argument("--climates", type=int, default=3)
    parser.add_argument("--demands", type=int, default=3)
    parser.add_argument("--buses", type=int, default=1)
    parser.add_argument("--peak-load", type=float, default=10000.0)
    parser.add_argument("--margin", type=float, default=1.25)
    parser.add_argument("--link-capacity", type=float, default=0.10)
    parser.add_argument("--seed", type=int, default=20260912)
    parser.add_argument("--name", default=None)
    args = parser.parse_args()
    args.battery = False
    args.no_design = False
    args.scenarios = args.climates * args.demands

    name = args.name or (
        f"ttr_u{args.units}_t{args.snapshots}_c{args.climates}"
        f"_d{args.demands}_b{args.buses}")

    names, demand, availability, joint = [], [], [], []
    groups = {}
    climate_availability, climate_probability = climate_axis(args.climates)
    for c, (factor, pc) in enumerate(zip(climate_availability,
                                         climate_probability)):
        climate = f"c{c}"
        groups[climate] = {"probability": float(pc), "scenarios": {}}
        multipliers, conditional = demand_axis(args.demands, c, args.climates)
        for d, (multiplier, pd_c) in enumerate(zip(multipliers, conditional)):
            leaf = f"{climate}_d{d}"
            groups[climate]["scenarios"][leaf] = float(pd_c)
            names.append(leaf)
            demand.append(multiplier)
            availability.append(factor)
            joint.append(pc * pd_c)
    joint = np.array(joint)
    assert abs(joint.sum() - 1.0) < 1e-9, joint.sum()

    DATA.mkdir(parents=True, exist_ok=True)
    n, _, _ = base.build(args, (names, np.array(demand), np.array(availability),
                                joint))
    n.export_to_netcdf(str(DATA / f"{name}_flat.nc"))
    with open(DATA / f"{name}_tree.json", "w") as f:
        json.dump({"name": name, "stages": 3, "groups": groups}, f, indent=1)

    print(f"{name}: {args.climates} outer x {args.demands} inner = "
          f"{len(names)} leaves, {args.units} units, {args.snapshots} "
          f"snapshots, {args.buses} buses, joint probability {joint.sum():.12f}")


if __name__ == "__main__":
    main()
