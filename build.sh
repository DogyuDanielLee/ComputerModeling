#!/usr/bin/env bash
# Build the CPSim simulator engine from the sources in CPSim/ (CMake).
# Run it again after editing anything under CPSim/Engine/.
# Output: CPSim/build/Debug/CPSim-0.1.out (the path start.sh runs)
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "$(readlink -f -- "${BASH_SOURCE[0]}")")" &>/dev/null && pwd)"
CPSIM_DIR="$SCRIPT_DIR/CPSim"
BUILD_DIR="$CPSIM_DIR/build"
ENGINE_BIN="$BUILD_DIR/Debug/CPSim-0.1.out"

log() { echo "[build.sh] $*"; }
die() { echo "[build.sh] Error: $*" >&2; exit 1; }

for tool in cmake g++; do
	command -v "$tool" >/dev/null 2>&1 || die "$tool not found. Run install-deps.sh first."
done
[ -f "$CPSIM_DIR/CMakeLists.txt" ] || die "CMakeLists.txt not found in $CPSIM_DIR"

# CMake refuses to reuse a cache created at another path, e.g. after the folder was moved
cache="$BUILD_DIR/CMakeCache.txt"
if [ -f "$cache" ] && ! grep -qxF "CMAKE_HOME_DIRECTORY:INTERNAL=$CPSIM_DIR" "$cache"; then
	log "The folder was moved since the last build. Starting a clean build."
	rm -rf "$BUILD_DIR"
fi

log "Configuring..."
cmake -S "$CPSIM_DIR" -B "$BUILD_DIR" > /dev/null
log "Compiling..."
cmake --build "$BUILD_DIR" -- -j"$(nproc)" > /dev/null

[ -x "$ENGINE_BIN" ] || die "Build finished but the engine was not found: $ENGINE_BIN"
log "Engine built: $ENGINE_BIN"
