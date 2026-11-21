#!/usr/bin/env bash
# Build the final-project submission (Team 4) as the slides require:
#   HW1: Team4_CC.cpp, Team4_LK.cpp   (CPSim/sharedObjectFiles/)
#        Team4_design.xml             (CPSim/, copied from design.xml written by Run Simulation)
#   HW2: Team4_read_write.log, Team4_schedule.log (CPSim/Log/)
#   Report: Team4_report.pdf (repo root)
# -> Team4_Final_Project.zip in the repo root.
#
# Run it after a final Run Simulation with the final design, so design.xml and the logs match.
set -euo pipefail
TEAM=Team4
ROOT="$(cd -- "$(dirname -- "$(readlink -f -- "${BASH_SOURCE[0]}")")/.." && pwd)"
CP="$ROOT/CPSim"
SO="$CP/sharedObjectFiles"
warn() { echo "[package] WARNING: $*" >&2; }

# CC.cpp/LK.cpp start with a "// Team4_CC.cpp" line: the engine extracts sim_main() starting at the
# first '{' after the first line containing the file's base name, so the copies compile as-is.
for swc in CC LK; do
	cp "$SO/$swc/$swc.cpp" "$SO/${TEAM}_${swc}.cpp"
done

[ -f "$CP/design.xml" ] || { echo "[package] CPSim/design.xml missing: press Run Simulation once" >&2; exit 1; }
cp "$CP/design.xml" "$CP/${TEAM}_design.xml"
echo "[package] design in ${TEAM}_design.xml:"
grep -o 'IP="[^"]*"[^>]*period="[0-9]*"' "$CP/design.xml" | sed -E 's/.*IP="([^"]*)".*WCET="([0-9]*)".*period="([0-9]*)"/  \1 period=\3 exec=\2/; s/BCET="[0-9]*" //' || true
grep -q "ComputerModeling/CPSim/sharedObjectFiles/CC/CC.cpp" "$CP/design.xml" || warn "design.xml does not use CPSim/sharedObjectFiles/CC/CC.cpp from this repo"

for log in read_write schedule; do
	f="$CP/Log/${TEAM}_${log}.log"
	[ -f "$f" ] || { warn "$f missing (run a simulation)"; continue; }
	[ "$(wc -l < "$f")" -gt 1 ] || warn "$f has only a header"
done
grep -q "DEADLINE MISS" "$CP/Log/${TEAM}_schedule.log" 2>/dev/null && warn "the schedule log contains DEADLINE MISS events (zero grade for HW1!)"
[ -f "$ROOT/${TEAM}_report.pdf" ] || warn "${TEAM}_report.pdf not found in $ROOT"

python3 - "$ROOT" "$TEAM" <<'PY'
import os, sys, zipfile
root, team = sys.argv[1], sys.argv[2]
files = [f"CPSim/sharedObjectFiles/{team}_CC.cpp", f"CPSim/sharedObjectFiles/{team}_LK.cpp",
         f"CPSim/{team}_design.xml", f"CPSim/Log/{team}_read_write.log", f"CPSim/Log/{team}_schedule.log",
         f"{team}_report.pdf"]
out = os.path.join(root, f"{team}_Final_Project.zip")
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for f in files:
        p = os.path.join(root, f)
        if os.path.exists(p):
            z.write(p, os.path.basename(f))
            print("  +", os.path.basename(f))
print("[package] wrote", out)
PY
