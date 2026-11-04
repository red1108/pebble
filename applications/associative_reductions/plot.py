#!/usr/bin/env python3
"""Compare associative-reduction depth at the measured KMRV space budgets."""
import json
import os
from pathlib import Path
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
OUT=Path(os.environ.get("PEBBLE_FIGURE_OUTPUT",str(ROOT/"build/applications/associative_reductions"))).resolve()
OUT.mkdir(parents=True,exist_ok=True)
rows=json.loads((ROOT/"build/applications/associative_reductions/results.json").read_text())
heights=[r["height"] for r in rows]
plt.rcParams.update({
    "font.family":"STIXGeneral","mathtext.fontset":"stix","font.size":10,
    "axes.labelsize":10,"legend.fontsize":8,"xtick.labelsize":8,"ytick.labelsize":9,
    "axes.spines.top":False,"axes.spines.right":False,"pdf.fonttype":42})
fig,ax=plt.subplots(figsize=(4.0,2.8),layout="constrained")
fig.get_layout_engine().set(w_pad=0.06,h_pad=0.04)
lines=[]
for values,label,color in [
    ([r["kmrv"]["depth"] for r in rows],"KMRV","#555555"),
    ([r["ours"]["depth"] for r in rows],"Ours","#0072B2"),
]:
    line,=ax.plot(heights,values,color=color,marker="o",markersize=2.7,
                  linewidth=1.4,label=label)
    lines.append(line)
ax.set_yscale("log",base=2)
ax.set_xlabel(r"Height $H$")
ax.set_ylabel("Depth",labelpad=6)
ax.set_xticks(heights)
ax.set_xlim(4.6,17.4)
ax.grid(True,alpha=.18)
secondary=ax.twinx()
line,=secondary.plot(heights,[100*(1-r["ours"]["depth"]/r["kmrv"]["depth"]) for r in rows],
                     color="#009E73",linestyle="--",marker="^",
                     markersize=3,linewidth=1.3,label="Improvement")
lines.append(line)
secondary.set_ylabel("Improvement (%)",color="black",fontsize=9,labelpad=0)
secondary.set_ylim(-20,100)
secondary.set_yticks(range(-20,101,20))
secondary.axhline(0,color="#bbbbbb",linestyle=":",linewidth=.6)
secondary.tick_params(axis="y",colors="black")
secondary.spines["right"].set_visible(True)
secondary.spines["right"].set_color("black")
ax.legend(handles=lines,frameon=False,loc="upper left")
for suffix in ("pdf","png"):
    path=OUT/f"associative_reduction_depth.{suffix}"
    fig.savefig(path,dpi=220,bbox_inches="tight",pad_inches=.02)
    print(path)
