#!/usr/bin/env bash
# Delete the CPSim workspace so the next run.sh starts again from the template.
# Uses the same WORKSPACE_DIR rule as run.sh.
set -euo pipefail

ACCOUNT_HOME="$(getent passwd "$(id -un)" | cut -d: -f6)"
WORKSPACE_DIR="$ACCOUNT_HOME/CPSim-workspace"

if pgrep -f "/CPSim$" >/dev/null 2>&1; then
	echo "[reset.sh] Warning: CPSim seems to be running. Close CPSim first, then try again." >&2
	exit 1
fi

if [ ! -d "$WORKSPACE_DIR" ]; then
	echo "[reset.sh] No workspace to delete: $WORKSPACE_DIR"
	exit 0
fi

read -r -p "This deletes the workspace and all unsaved work: $WORKSPACE_DIR  Continue? [y/N] " reply
case "$reply" in
	y|Y)
		rm -rf "$WORKSPACE_DIR"
		echo "[reset.sh] Deleted. The next run.sh will recreate it from the workspace template."
		;;
	*)
		echo "[reset.sh] Cancelled."
		;;
esac
