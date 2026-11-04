#!/usr/bin/env python3
"""Plot sequential work and average memory utilization at minimum space."""
import json
import os
from pathlib import Path
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
OUT=Path(os.environ.get("PEBBLE_FIGURE_OUTPUT",str(ROOT/"build/applications/complete_trees"))).resolve()
OUT.mkdir(parents=True,exist_ok=True)
data=json.loads((ROOT/"build/applications/complete_trees/results.json").read_text())
rows=data
assert [r["height"] for r in rows]==list(range(5,18))
assert all(r["budget"]==r["height"]+1 for r in rows)
heights=[r["height"] for r in rows]
plt.rcParams.update({
    "font.family":"STIXGeneral","mathtext.fontset":"stix","font.size":10,
    "axes.labelsize":10,"legend.fontsize":8,"xtick.labelsize":8,"ytick.labelsize":9,
    "axes.spines.top":False,"axes.spines.right":False,"pdf.fonttype":42})
fig,axes=plt.subplots(1,2,figsize=(7.2,3.0),layout="constrained")
fig.get_layout_engine().set(w_pad=0.10,h_pad=0.04,wspace=0.02)
colors={"kss":"#555555","ours":"#0072B2","improvement":"#009E73"}
work_lines=[]
for kind,label in [("kss","KSS"),("ours","Ours")]:
    line,=axes[0].plot(heights,[r[kind]["work"] for r in rows],
                       marker="o",markersize=2.7,linewidth=1.4,
                       label=label,color=colors[kind])
    work_lines.append(line)
    utilization=[100*r[kind]["area"]/(r["budget"]*r[kind]["depth"]) for r in rows]
    axes[1].plot(heights,utilization,marker="o",markersize=2.7,
                  linewidth=1.4,label=label,color=colors[kind])
improvement=[100*(1-r["ours"]["work"]/r["kss"]["work"]) for r in rows]
secondary=axes[0].twinx()
line,=secondary.plot(heights,improvement,color=colors["improvement"],
                     linestyle="--",marker="^",markersize=3,
                     linewidth=1.3,label="Improvement")
work_lines.append(line)
secondary.set_ylim(0,100)
secondary.set_yticks(range(0,101,20))
secondary.set_ylabel("Improvement (%)",color="black",fontsize=9,labelpad=0)
secondary.tick_params(axis="y",colors="black",labelsize=9)
secondary.spines["right"].set_visible(True)
secondary.spines["right"].set_color("black")
axes[0].set_yscale("log",base=2)
axes[0].set_ylabel("Work",labelpad=0)
axes[0].legend(handles=work_lines,frameon=False,loc="upper left")
axes[1].set_ylabel("Memory utilization (%)",labelpad=0)
axes[1].set_ylim(0,100)
axes[1].set_yticks(range(0,101,20))
axes[1].legend(frameon=False,loc="upper right")
for ax in axes:
    ax.set_xlabel(r"Height $H$")
    ax.set_xticks(heights)
    ax.set_xlim(4.6,17.4)
    ax.grid(True,alpha=.18)
axes[0].set_title("(a) Work",fontsize=10)
axes[1].set_title("(b) Memory utilization",fontsize=10)
for suffix in ("pdf","png"):
    destination=OUT/f"numerical_tradeoffs.{suffix}"
    fig.savefig(destination,dpi=220,bbox_inches="tight",pad_inches=.02)
    print(destination)
