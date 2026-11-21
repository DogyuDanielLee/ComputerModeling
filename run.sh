#!/usr/bin/env bash
# Launch script for the ComputerModeling student package.
#
# Layout (this script lives in the package root):
#   CPSim/       simulator engine sources, SWC code, Log/, settings  (= CPSIM_PATH)
#   CPSim_GUI/   CPSim Eclipse application and workspace-template/
#
# The workspace location is fixed by "-data @user.home/CPSim-workspace" in
# CPSim_GUI/CPSim.ini. WORKSPACE_DIR below must match it exactly; changing only
# one of them makes the template land somewhere CPSim does not open.
set -euo pipefail

# Resolve paths from the real location of this script (symlinks resolved),
# not from the caller's working directory.
SCRIPT_DIR="$(cd -- "$(dirname -- "$(readlink -f -- "${BASH_SOURCE[0]}")")" &>/dev/null && pwd)"
export CPSIM_PATH="$SCRIPT_DIR/CPSim"

GUI_DIR="$SCRIPT_DIR/CPSim_GUI"
APP_BIN="$GUI_DIR/CPSim"
TEMPLATE_DIR="$GUI_DIR/workspace-template"
ENGINE_BIN="$CPSIM_PATH/build/Debug/CPSim-0.1.out"
# Java's user.home follows the account (passwd) home, not $HOME, so resolve it the same way
ACCOUNT_HOME="$(getent passwd "$(id -un)" | cut -d: -f6)"
WORKSPACE_DIR="$ACCOUNT_HOME/CPSim-workspace"   # must match -data in CPSim.ini

echo "[run.sh] CPSIM_PATH = $CPSIM_PATH"

if [ ! -x "$APP_BIN" ]; then
	echo "[run.sh] Error: CPSim executable not found: $APP_BIN" >&2
	echo "[run.sh] Make sure the whole ComputerModeling archive was extracted." >&2
	exit 1
fi

# 1) Build the simulator engine if it has not been built yet
if [ ! -x "$ENGINE_BIN" ]; then
	echo "[run.sh] The simulator engine is not built yet. Building it now..."
	"$SCRIPT_DIR/build.sh"
fi

# 2) On the first run, copy the template into the home directory
if [ ! -d "$WORKSPACE_DIR" ]; then
	if [ ! -d "$TEMPLATE_DIR" ]; then
		echo "[run.sh] Error: workspace template not found: $TEMPLATE_DIR" >&2
		exit 1
	fi
	echo "[run.sh] First run detected. Copying the workspace template:"
	echo "[run.sh]   $TEMPLATE_DIR -> $WORKSPACE_DIR"
	tmp_dir="$(mktemp -d "${WORKSPACE_DIR}.XXXXXX")"
	cp -r "$TEMPLATE_DIR/." "$tmp_dir/"
	# SWC source paths are stored as @CPSIM_PATH@ in the template (the engine compiles those sources)
	escaped_path="$(printf '%s' "$CPSIM_PATH" | sed 's/[&|\\]/\\&/g')"
	find "$tmp_dir" -name '*.diagram' -exec sed -i "s|@CPSIM_PATH@|${escaped_path}|g" {} +
	mv "$tmp_dir" "$WORKSPACE_DIR"
fi

# 3) Launch. The native launcher resolves the relative paths in CPSim.ini
#    against the executable's directory, so no cd is needed.
#    The default java may not be 17, so use the Java 17 installed by install-deps.sh when present.
JAVA17="/usr/lib/jvm/java-17-openjdk-amd64/bin/java"
vm_args=()
if [ -x "$JAVA17" ]; then
	vm_args=(-vm "$JAVA17")
fi
exec "$APP_BIN" ${vm_args[@]+"${vm_args[@]}"} "$@"
