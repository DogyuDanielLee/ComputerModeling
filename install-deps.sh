#!/usr/bin/env bash
# Install the packages CPSim needs and check the runtime environment.
# Target: Ubuntu 20.04 / 22.04 / 24.04 (x86_64). Run once inside the package folder.
#
# Packages:
#   openjdk-17-jre   runs CPSim (Eclipse); full JRE, not headless, because the analysis windows use Swing
#   build-essential  compiles the engine (build.sh) and the SWC code during simulation
#   cmake            configures the engine build (build.sh)
#   GTK3             GUI toolkit (package is named libgtk-3-0t64 since Ubuntu 24.04)
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "$(readlink -f -- "${BASH_SOURCE[0]}")")" &>/dev/null && pwd)"
CPSIM_DIR="$SCRIPT_DIR/CPSim"
JAVA17="/usr/lib/jvm/java-17-openjdk-amd64/bin/java"   # run.sh uses the Java at this path

log() { echo "[install-deps] $*"; }
die() { echo "[install-deps] Error: $*" >&2; exit 1; }

command -v apt-get >/dev/null 2>&1 \
	|| die "This is not an apt-based Ubuntu system. Install Java 17, gcc/g++ and GTK3 manually."
[ "$(uname -m)" = "x86_64" ] || die "This package supports x86_64 only (this machine: $(uname -m))."

SUDO=""
if [ "$(id -u)" -ne 0 ]; then
	SUDO="sudo"
	log "Installing packages requires administrator rights. Enter your password if asked."
fi

# 1) Install packages (apt skips the ones already installed)
log "Updating package lists..."
$SUDO apt-get update

pkgs=(openjdk-17-jre build-essential cmake)
if apt-cache show libgtk-3-0t64 >/dev/null 2>&1; then
	pkgs+=(libgtk-3-0t64)
else
	pkgs+=(libgtk-3-0)
fi
log "Installing: ${pkgs[*]}"
$SUDO apt-get install -y "${pkgs[@]}"

# 2) Check the result
problems=0
check() {
	if eval "$2"; then
		log "  [OK]   $1"
	else
		echo "[install-deps]   [FAIL] $1 - $3" >&2
		problems=$((problems + 1))
	fi
}

log "Checking the runtime environment:"
check "Java 17 ($JAVA17)" '[ -x "$JAVA17" ]' "check that openjdk-17-jre is installed"
check "gcc / g++" 'command -v gcc >/dev/null && command -v g++ >/dev/null' "check that build-essential is installed"
check "cmake" 'command -v cmake >/dev/null' "check that cmake is installed"
check "CPSim executable" '[ -x "$SCRIPT_DIR/CPSim_GUI/CPSim" ]' "make sure the whole archive was extracted"
check "Simulator engine sources" '[ -f "$CPSIM_DIR/CMakeLists.txt" ] && [ -d "$CPSIM_DIR/Engine" ]' \
	"make sure the whole archive was extracted"
# The engine puts this path into its gcc command unquoted, so spaces break compilation
check "No spaces or '&' in the folder path ($SCRIPT_DIR)" '[[ "$SCRIPT_DIR" != *[[:space:]]* && "$SCRIPT_DIR" != *"&"* ]]' \
	"move the folder to a path without spaces (e.g. ~/ComputerModeling)"
# build.sh writes CPSim/build/; at runtime the engine writes sharedObjectFiles/ and Log/, and the GUI writes design.xml
check "Write permission on the CPSim folder" '[ -w "$CPSIM_DIR" ] && [ -w "$CPSIM_DIR/sharedObjectFiles" ] && [ -w "$CPSIM_DIR/Log" ]' \
	"extract the archive somewhere you can write to, such as your home directory"

if [ "$problems" -gt 0 ]; then
	die "${problems} check(s) failed. Fix the [FAIL] items above and run this script again."
fi
log "All set. Start CPSim with:  $SCRIPT_DIR/run.sh  (the simulator engine is built on the first run)"
