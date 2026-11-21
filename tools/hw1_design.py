#!/usr/bin/env python3
"""HW1 design helper (Team 4).

  python3 tools/hw1_design.py show   [preset]   print a preset, check the HW1 rules and RM deadlines
  python3 tools/hw1_design.py apply  [preset]   write it into the CPSim GUI design (close CPSim first)
  python3 tools/hw1_design.py list              list presets

The GUI design lives in ~/CPSim-workspace/cm-project/src/diagrams/cm-project.diagram; CPSim
turns it into CPSim/design.xml when you press Run Simulation. 'apply' backs the diagram up,
sets period/deadline/WCET/BCET/phase of all six SWCs, and points CC/LK at the code in this repo.
"""
import math
import os
import re
import shutil
import subprocess
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIAGRAM = os.path.join(os.path.expanduser("~"), "CPSim-workspace/cm-project/src/diagrams/cm-project.diagram")
SWC_DIR = os.path.join(REPO, "CPSim/sharedObjectFiles")
DUMMIES = ["SENSING", "BRAKE", "POWER", "LOCAL"]

# name: {swc: (period, exec_time)}; deadline = period, BCET = WCET = exec_time, phase 0, one ECU
PRESETS = {
    # LK and CC both every 110 ms. U = 0.945; RM response-time analysis: every task meets its
    # deadline with >= 50 ms to spare. Found by exhaustive search over all legal designs: no legal
    # one-ECU design runs LK faster than 100 ms, and LK 100 forces U = 1.00 exactly (preset lk100).
    "balanced": {"LK": (110, 30), "CC": (110, 30), **{d: (500, 50) for d in DUMMIES}},
    # LK every 100 ms, CC every 500 ms. U = 1.00 exactly, harmonic: schedulable but zero slack.
    "lk100":    {"LK": (100, 50), "CC": (500, 50), **{d: (500, 50) for d in DUMMIES}},
    # The course baseline (for comparison only).
    "baseline": {"LK": (500, 50), "CC": (500, 50), **{d: (500, 100) for d in DUMMIES}},
}
DEFAULT = "balanced"


def check_rules(design):
    """HW1 constraints from the project slides."""
    problems = []
    for name, (period, c) in design.items():
        if not 20 <= period <= 500:
            problems.append(f"{name}: period {period} outside [20, 500]")
        if c < 20:
            problems.append(f"{name}: execution time {c} < 20")
        if period % 10 or c % 10:
            problems.append(f"{name}: values must be multiples of 10 (unit stride)")
        u = c / period
        need = 0.1 if period > 250 else 0.2 if period > 100 else 0.5
        if u + 1e-9 < need:
            problems.append(f"{name}: utilization {u:.3f} < {need} required for period {period}")
    return problems


def rm_response_times(design):
    """Exact response-time analysis for fixed-priority RM on one core (ties: LK, CC, dummies)."""
    order = sorted(design.items(), key=lambda kv: (kv[1][0], ["LK", "CC"].index(kv[0]) if kv[0] in ("LK", "CC") else 2))
    result = {}
    for i, (name, (period, c)) in enumerate(order):
        r = c
        while True:
            nxt = c + sum(math.ceil(r / p) * cj for _, (p, cj) in order[:i])
            if nxt == r or nxt > period:
                r = nxt
                break
            r = nxt
        result[name] = r
    return result


def show(name):
    design = PRESETS[name]
    print(f"preset '{name}'  (one ECU, RM, deadline = period, BCET = WCET, phase 0)")
    print(f"  {'SWC':8} {'period':>6} {'exec':>5} {'util':>6} {'resp':>5}  read/write")
    resp = rm_response_times(design)
    for swc, (p, c) in design.items():
        rw = "yes" if swc in ("LK", "CC") else "no"
        flag = "" if resp[swc] <= p else "  <-- DEADLINE MISS"
        print(f"  {swc:8} {p:>6} {c:>5} {c / p:>6.3f} {resp[swc]:>5}  {rw}{flag}")
    total = sum(c / p for p, c in design.values())
    print(f"  total utilization {total:.3f}")
    problems = check_rules(design)
    misses = [s for s, (p, _) in design.items() if resp[s] > p]
    for msg in problems:
        print("  RULE VIOLATION:", msg)
    if misses:
        print("  DEADLINE MISS:", ", ".join(misses))
    lk_const = re.search(r"LK_PERIOD_MS\s*=\s*([0-9.]+)", open(os.path.join(SWC_DIR, "LK/LK.cpp")).read())
    if lk_const and float(lk_const.group(1)) != design["LK"][0]:
        print(f"  NOTE: LK.cpp has LK_PERIOD_MS = {lk_const.group(1)}, design LK period is {design['LK'][0]}; "
              "update LK.cpp so the derivative gain matches")
    ok = not problems and not misses
    print("  OK" if ok else "  NOT OK")
    return ok


def apply(name):
    if not show(name):
        sys.exit("refusing to apply a design that breaks the rules")
    if subprocess.run(["pgrep", "-f", "CPSim_GUI/CPSim"], capture_output=True).returncode == 0 or \
       subprocess.run(["pgrep", "-x", "CPSim"], capture_output=True).returncode == 0:
        sys.exit("CPSim GUI is running: close it first, otherwise it overwrites the diagram")
    if not os.path.exists(DIAGRAM):
        sys.exit(f"diagram not found: {DIAGRAM} (run ./run.sh once to create the workspace)")
    text = open(DIAGRAM).read()
    backup = DIAGRAM + time.strftime(".bak-%Y%m%d-%H%M%S")
    shutil.copy2(DIAGRAM, backup)

    def fix(match):
        tag = match.group(0)
        swc = re.search(r'id="([^;"]+)', tag).group(1)
        if swc not in PRESETS[name]:
            return tag
        period, c = PRESETS[name][swc]
        for attr, value in (("period", period), ("deadline", period), ("wcet", c), ("bcet", c), ("phase", 0)):
            tag = re.sub(rf'{attr}="[^"]*"', f'{attr}="{value}"', tag)
        if swc in ("LK", "CC"):
            tag = re.sub(r'id="[^"]*"', f'id="{swc};C;{SWC_DIR}/{swc}/{swc}.cpp"', tag)
            for attr in ("readCon", "writeCon"):
                tag = re.sub(rf'{attr}="[^"]*"', f'{attr}="1"', tag)
        return tag

    new_text, count = re.subn(r"<swcRefs [^>]*/>", fix, text)
    open(DIAGRAM, "w").write(new_text)
    print(f"updated {count} SWC entries in {DIAGRAM}")
    print(f"backup: {backup}")
    print("Now start ./run.sh and press Run Simulation; CPSim/design.xml is regenerated from this design.")


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "show"
    preset = sys.argv[2] if len(sys.argv) > 2 else DEFAULT
    if cmd == "list":
        for p in PRESETS:
            print(p + (" (default)" if p == DEFAULT else ""))
    elif cmd == "show":
        sys.exit(0 if show(preset) else 1)
    elif cmd == "apply":
        apply(preset)
    else:
        sys.exit(__doc__)
