"""Turn a tree written by gen_thermal_tree.py into a MultiStageStochasticBlock.

    python emit_thermal_mssb.py ttr_u20_t48_c3_d3_b1

It reads ../instances/<name>_flat.nc and ../instances/<name>_tree.json and
writes ../instances/smspp_<name>.nc4, a MultiStageStochasticBlock with one
TwoStageStochasticBlock per climate, whose leaves are the demands of that
climate, the thermal units being ThermalUnitBlock as in emit_thermal_tssb.py.
The same leaves as a single TwoStageStochasticBlock are written by

    python emit_thermal_tssb.py ttr_u20_t48_c3_d3_b1 smspp_ttr_u20_t48_c3_d3_b1_2s
"""

import json
import sys
import warnings
from pathlib import Path

warnings.simplefilter("ignore")

import pypsa
from pypsa2smspp.transformation import Transformation

HERE = Path(__file__).resolve().parent
DATA = HERE.parent / "instances"
CONFIG = HERE.parent / "config" / "TSSBSCfg.txt"

name = sys.argv[1] if len(sys.argv) > 1 else "ttr_u20_t48_c3_d3_b1"
WORK = DATA / "work"
WORK.mkdir(parents=True, exist_ok=True)

n = pypsa.Network(str(DATA / f"{name}_flat.nc"))
tree = json.load(open(DATA / f"{name}_tree.json"))

t = Transformation(
    name=name,
    configfile=str(CONFIG),
    enable_thermal_units=True,
    intermittent_carriers=["solar", "wind"],
    workdir=str(WORK),
    stochastic_parameters={"stochastic_type": "mssb",
                           "parameters": ["demand", "renewable_maxpower"],
                           "tree": {"groups": tree["groups"]}},
    overwrite=True,
    fp_temp="smspp_{name}_temp.nc",
)
t.create_model(n, verbose=False)
t.sms_network.to_netcdf(str(DATA / f"smspp_{name}.nc4"), force=True)
print("[written]", DATA / f"smspp_{name}.nc4")
