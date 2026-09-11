#!/usr/bin/env python3
"""
Convert an SDDPBlock.nc4 instance from the legacy ScenarioSet layout
(top-level "Scenarios" variable + "SubScenarioSize"/"SizeRandomDataGroups"
at SDDPBlock level) to the new ScenarioGenerator layout (sub-group
"ScenarioGenerator" with type="DiscreteScenarioSet", carrying the
"Scenarios" variable and its dimensions; structural metadata
"SubScenarioSize"/"SizeRandomDataGroups" stay at SDDPBlock level).

The "NumberScenarios" and "ScenarioSize" dimensions of the legacy
layout shape the SDDPBlock-level "Scenarios" variable that we drop on
conversion: they have no other referent in the SDDPBlock group, so we
also drop them here rather than carrying vestigial metadata.

Usage:
    python3 convert_to_scenariogenerator.py SDDPBlock.nc4 SDDPBlock-new.nc4
"""

import sys
import shutil
from pathlib import Path

import netCDF4 as nc
import numpy as np


def copy_group(src, dst, skip_dims=None, skip_vars=None):
    """Recursively copy contents of netCDF group `src` into `dst`,
    skipping any top-level dimensions or variables listed in
    `skip_dims` / `skip_vars`. The skip lists apply only to the
    current group; sub-groups are copied entirely."""
    skip_dims = set(skip_dims or [])
    skip_vars = set(skip_vars or [])

    # copy group attributes
    for name in src.ncattrs():
        dst.setncattr(name, src.getncattr(name))

    # copy dimensions (use None for unlimited)
    for name, dim in src.dimensions.items():
        if name in skip_dims:
            continue
        dst.createDimension(name, len(dim) if not dim.isunlimited() else None)

    # copy variables (skipping requested ones at this level)
    for name, var in src.variables.items():
        if name in skip_vars:
            continue
        new_var = dst.createVariable(name, var.dtype, var.dimensions)
        for attr in var.ncattrs():
            new_var.setncattr(attr, var.getncattr(attr))
        new_var[:] = var[:]

    # recurse into sub-groups
    for name, sub in src.groups.items():
        new_sub = dst.createGroup(name)
        copy_group(sub, new_sub)


def convert(src_path: Path, dst_path: Path):
    if dst_path.exists():
        dst_path.unlink()

    with nc.Dataset(src_path, "r") as src, nc.Dataset(dst_path, "w") as dst:
        # --- root file attributes ----------------------------------------
        for name in src.ncattrs():
            dst.setncattr(name, src.getncattr(name))

        # there is no root-level variable / dim in an SMS++ block file;
        # everything lives inside the "SDDPBlock" group
        if "SDDPBlock" not in src.groups:
            raise SystemExit("error: src root is missing 'SDDPBlock' group")

        sddp_src = src.groups["SDDPBlock"]
        sddp_dst = dst.createGroup("SDDPBlock")

        # --- copy SDDPBlock contents, omitting the legacy 'Scenarios' ----
        # variable plus the now-orphaned dimensions that shaped it: in
        # the new layout the per-scenario data lives in the
        # ScenarioGenerator sub-group with its own local dimensions, and
        # nothing else in the SDDPBlock group references NumberScenarios
        # or ScenarioSize.
        copy_group(sddp_src, sddp_dst,
                   skip_dims={"NumberScenarios", "ScenarioSize"},
                   skip_vars={"Scenarios"})

        # --- now build the new ScenarioGenerator sub-group ---------------
        # DiscreteScenarioSet expects (in its own group):
        #     dims: NumberScenarios, ScenarioSize
        #     var:  Scenarios(NumberScenarios, ScenarioSize)
        # and the SDDPBlock-level 'type' attribute on the sub-group must
        # be "DiscreteScenarioSet" so the factory picks the right derived
        # class.
        sc_var = sddp_src.variables["Scenarios"]
        nb_scen, scen_size = sc_var.shape

        gen = sddp_dst.createGroup("ScenarioGenerator")
        gen.setncattr("type", "DiscreteScenarioSet")
        gen.createDimension("NumberScenarios", nb_scen)
        gen.createDimension("ScenarioSize", scen_size)

        new_sc = gen.createVariable("Scenarios", sc_var.dtype,
                                    ("NumberScenarios", "ScenarioSize"))
        for attr in sc_var.ncattrs():
            new_sc.setncattr(attr, sc_var.getncattr(attr))
        new_sc[:, :] = sc_var[:, :]

        # PoolWeights omitted on purpose: the v1 SDDPBlock path only
        # accepts uniform probabilities, and the absence of the variable
        # means uniform 1/N — exactly what the legacy ScenarioSet implied.

        print(f"wrote {dst_path}")
        print(f"  scenarios: {nb_scen} x {scen_size}")
        print(f"  SubScenarioSize     present at SDDPBlock level: "
              f"{'SubScenarioSize' in sddp_dst.dimensions}")
        print(f"  SizeRandomDataGroups present at SDDPBlock level: "
              f"{'SizeRandomDataGroups' in sddp_dst.variables}")


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    convert(Path(sys.argv[1]), Path(sys.argv[2]))


if __name__ == "__main__":
    main()
