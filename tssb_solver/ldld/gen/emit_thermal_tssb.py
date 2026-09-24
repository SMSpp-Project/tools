"""Turn a network written by gen_thermal_tssb.py into a TwoStageStochasticBlock.

    python emit_thermal_tssb.py tuc_u20_t48_s3_b1

The conversion is the ordinary two-stage one of pypsa2smspp, with the thermal
units switched on (`enable_thermal_units=True`) and only the two renewable
carriers declared intermittent, so that every unit of the fleet becomes a
ThermalUnitBlock with its own commitment variables. It reads
../instances/<name>_flat.nc and writes ../instances/smspp_<name>.nc4, the
Block file that ldld_bench reads.
"""

import sys
import warnings
from pathlib import Path

warnings.simplefilter("ignore")

import pypsa
from pypsa2smspp.transformation import Transformation

HERE = Path(__file__).resolve().parent
DATA = HERE.parent / "instances"
CONFIG = HERE.parent / "config" / "TSSBSCfg.txt"

name = sys.argv[1] if len(sys.argv) > 1 else "tuc_u20_t48_s3_b1"
WORK = DATA / "work"
WORK.mkdir(parents=True, exist_ok=True)

n = pypsa.Network(str(DATA / f"{name}_flat.nc"))

t = Transformation(
    name=name,
    configfile=str(CONFIG),
    enable_thermal_units=True,
    intermittent_carriers=["solar", "wind"],
    workdir=str(WORK),
    stochastic_parameters={"stochastic_type": "tssb",
                           "parameters": ["demand", "renewable_maxpower"]},
    overwrite=True,
    fp_temp="smspp_{name}_temp.nc",
)
t.create_model(n, verbose=False)
t.sms_network.to_netcdf(str(DATA / f"smspp_{name}.nc4"), force=True)
print("[written]", DATA / f"smspp_{name}.nc4")
