#!/usr/bin/env python3
"""
Convert an SDDPBlock-new.nc4 instance (which uses a single-stage
DiscreteScenarioSet ScenarioGenerator whose scenarios span all stages
concatenated) into a semantically equivalent SDDPBlock-new-multi.nc4
where the ScenarioGenerator is an IndependentMultiStageScenarioGenerator
with one DiscreteScenarioSet inner per stage. The per-stage data of
stage t is the t-th sub_scenario_size-length slice of each original
global scenario, in the same order. Per-stage PoolWeights are omitted
(defaulting to uniform 1/N), matching the source instance.

This produces a third .nc4 (alongside SDDPBlock.nc4 [legacy] and
SDDPBlock-new.nc4 [single-stage gen]) that demonstrates the multi-stage
ScenarioGenerator layout. SDDP runs against the three instances must
produce bit-identical Backward/Forward values when the SDDPSolver
parameters are at their defaults.

Usage:
    python3 convert_to_multistage.py SDDPBlock-new.nc4 SDDPBlock-new-multi.nc4
"""

import sys
from pathlib import Path

import netCDF4 as nc


def copy_group(src, dst, skip_dims=None, skip_vars=None, skip_groups=None):
    """Recursively copy contents of netCDF group `src` into `dst`,
    skipping any top-level dimensions, variables or sub-groups listed."""
    skip_dims = set(skip_dims or [])
    skip_vars = set(skip_vars or [])
    skip_groups = set(skip_groups or [])

    for name in src.ncattrs():
        dst.setncattr(name, src.getncattr(name))

    for name, dim in src.dimensions.items():
        if name in skip_dims:
            continue
        dst.createDimension(name, len(dim) if not dim.isunlimited() else None)

    for name, var in src.variables.items():
        if name in skip_vars:
            continue
        new_var = dst.createVariable(name, var.dtype, var.dimensions)
        for attr in var.ncattrs():
            new_var.setncattr(attr, var.getncattr(attr))
        new_var[:] = var[:]

    for name, sub in src.groups.items():
        if name in skip_groups:
            continue
        new_sub = dst.createGroup(name)
        copy_group(sub, new_sub)


def convert(src_path: Path, dst_path: Path):
    if dst_path.exists():
        dst_path.unlink()

    with nc.Dataset(src_path, "r") as src, nc.Dataset(dst_path, "w") as dst:
        for name in src.ncattrs():
            dst.setncattr(name, src.getncattr(name))

        if "SDDPBlock" not in src.groups:
            raise SystemExit("error: src root is missing 'SDDPBlock' group")

        sddp_src = src.groups["SDDPBlock"]
        sddp_dst = dst.createGroup("SDDPBlock")

        # Copy everything in SDDPBlock except the ScenarioGenerator
        # subgroup and the SubScenarioSize dimension: on the multi-stage
        # path SDDPBlock::deserialize derives the per-stage sizes from
        # the generator walk and never reads the SubScenarioSize dim, so
        # carrying it here would be misleading vestigial metadata.
        copy_group(sddp_src, sddp_dst,
                   skip_dims={"SubScenarioSize"},
                   skip_groups={"ScenarioGenerator"})

        # Extract original single-stage ScenarioGenerator data
        if "ScenarioGenerator" not in sddp_src.groups:
            raise SystemExit(
                "error: src is missing 'ScenarioGenerator' subgroup")
        gen_src = sddp_src.groups["ScenarioGenerator"]
        gen_type = gen_src.getncattr("type")
        if gen_type != "DiscreteScenarioSet":
            raise SystemExit(
                f"error: src ScenarioGenerator type is '{gen_type}', "
                "expected 'DiscreteScenarioSet'")

        nb_scen = gen_src.dimensions["NumberScenarios"].size
        scen_size = gen_src.dimensions["ScenarioSize"].size
        scenarios = gen_src.variables["Scenarios"][:, :]

        # Derive per-stage sizes from SDDPBlock-level metadata
        time_horizon = sddp_src.dimensions["TimeHorizon"].size
        # SubScenarioSize is a *dimension* in this layout, carrying the
        # uniform per-stage size (all stages share it)
        sub_size = sddp_src.dimensions["SubScenarioSize"].size
        if sub_size * time_horizon != scen_size:
            raise SystemExit(
                f"error: SubScenarioSize ({sub_size}) * TimeHorizon "
                f"({time_horizon}) != ScenarioSize ({scen_size}); this "
                "converter currently only supports uniform per-stage sizes")

        # Build the IndependentMultiStageScenarioGenerator subgroup
        gen = sddp_dst.createGroup("ScenarioGenerator")
        gen.setncattr("type", "IndependentMultiStageScenarioGenerator")
        gen.createDimension("NumberStages", time_horizon)

        for t in range(time_horizon):
            stage = gen.createGroup(f"Stage_{t}")
            stage.setncattr("type", "DiscreteScenarioSet")
            stage.createDimension("NumberScenarios", nb_scen)
            stage.createDimension("ScenarioSize", sub_size)
            sc = stage.createVariable("Scenarios", scenarios.dtype,
                                      ("NumberScenarios", "ScenarioSize"))
            sc[:, :] = scenarios[:, t * sub_size: (t + 1) * sub_size]
            # PoolWeights omitted on purpose: defaults to uniform 1/N,
            # matching the source instance (which also omits PoolWeights).

        print(f"wrote {dst_path}")
        print(f"  stages: {time_horizon}, scenarios per stage: {nb_scen}, "
              f"sub-scenario size: {sub_size}")


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    convert(Path(sys.argv[1]), Path(sys.argv[2]))


if __name__ == "__main__":
    main()
