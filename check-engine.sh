#!/usr/bin/env bash
# Verify that the built simulator engine (CPSim/build/Debug/CPSim-0.1.out) actually
# runs correctly. Useful after editing anything under CPSim/Engine/ or CPSim/sharedObjectFiles/.
#
# Runs the real binary against a throwaway sandbox (a temp dir, not the real
# CPSim/Log or CPSim/sharedObjectFiles) so it never touches real project state.
#
# What it checks, regardless of network access:
#   - the binary exists and all shared libraries resolve
#   - it compiles and loads every SWC without crashing
#   - it prints the CPU performance line
#   - it does not crash, hang past its timeout, or dump core
#
# What it additionally checks if python3 is available (optional, best-effort):
#   - spins up a fake TORCS listener on 127.0.0.1 and confirms the engine
#     connects to it and starts writing Log/scheduling.log
#
# Usage:
#   deploy/check-engine.sh                 use a local fake TORCS (needs python3)
#   deploy/check-engine.sh <TORCS_IP>       connect to a real TORCS at that address instead
#
# The engine's TORCS port is fixed at build time (13380, see CMakeLists.txt);
# only the IP can be changed here.
set -uo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "$(readlink -f -- "${BASH_SOURCE[0]}")")" &>/dev/null && pwd)"
CPSIM_DIR="$SCRIPT_DIR/CPSim"
ENGINE_BIN="$CPSIM_DIR/build/Debug/CPSim-0.1.out"
TORCS_PORT=13380
CUSTOM_TORCS_IP="${1:-}"

log() { echo "[check-engine] $*"; }
fail() { echo "[check-engine] FAIL: $*" >&2; cleanup; exit 1; }

SANDBOX=""
TORCS_PID=""
cleanup() {
	[ -n "$TORCS_PID" ] && kill "$TORCS_PID" 2>/dev/null
	[ -n "$SANDBOX" ] && rm -rf "$SANDBOX"
}
trap cleanup EXIT

# 1) (Re)build first, so this checks the binary matching the current source,
#    not a stale one left over from an earlier edit. build.sh is incremental,
#    so this is nearly instant when nothing changed.
log "Building..."
"$SCRIPT_DIR/build.sh" || fail "build.sh failed (see compiler output above)"

# 2) All shared libraries must resolve
if ldd "$ENGINE_BIN" 2>&1 | grep -q "not found"; then
	ldd "$ENGINE_BIN" | grep "not found" >&2
	fail "the engine is missing shared libraries on this system"
fi
log "Binary and libraries OK: $ENGINE_BIN"

# 3) Build an isolated sandbox with two known-good SWCs (CC, LK), both linked
#    to ECU0, so the SWC-ECU link path is exercised the same way the GUI uses it.
SANDBOX="$(mktemp -d)"
mkdir -p "$SANDBOX/Log" "$SANDBOX/sharedObjectFiles"
cp "$CPSIM_DIR/settings.txt" "$SANDBOX/"
cp "$CPSIM_DIR/sharedObjectFiles/shared.h" "$SANDBOX/sharedObjectFiles/"
for swc in CC LK; do
	[ -d "$CPSIM_DIR/sharedObjectFiles/$swc" ] || fail "expected SWC source not found: sharedObjectFiles/$swc"
	cp -r "$CPSIM_DIR/sharedObjectFiles/$swc" "$SANDBOX/sharedObjectFiles/"
done

# 4) Decide what the engine should connect to:
#    - a real TORCS address given on the command line, checked for reachability first
#    - otherwise a local fake TORCS (needs python3)
#    - otherwise just 127.0.0.1 with nothing listening (connection check is skipped,
#      but SWC compile/load is still exercised)
expect_connection=false
if [ -n "$CUSTOM_TORCS_IP" ]; then
	torcs_ip="$CUSTOM_TORCS_IP"
	log "Checking TCP reachability of $torcs_ip:$TORCS_PORT..."
	if timeout 5 bash -c "exec 3<>/dev/tcp/$torcs_ip/$TORCS_PORT" 2>/dev/null; then
		expect_connection=true
		log "Reachable. Will verify the engine connects to it."
	else
		fail "cannot reach $torcs_ip:$TORCS_PORT over TCP. Check that TORCS is running there, the IP is correct, and no firewall is blocking port $TORCS_PORT."
	fi
elif command -v python3 >/dev/null 2>&1; then
	python3 - "$TORCS_PORT" >"$SANDBOX/fake_torcs.log" 2>&1 <<'PYEOF' &
import socket, sys, time
port = int(sys.argv[1])
s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("127.0.0.1", port)); s.listen(1); s.settimeout(15)
try:
    conn, _ = s.accept()
    print("fake TORCS: engine connected", flush=True)
    time.sleep(10)
except socket.timeout:
    print("fake TORCS: no connection within timeout", flush=True)
PYEOF
	TORCS_PID=$!
	sleep 0.3
	expect_connection=true
	torcs_ip="127.0.0.1"
	log "Fake TORCS listener started on 127.0.0.1:$TORCS_PORT"
else
	torcs_ip="127.0.0.1"
	log "python3 not found and no IP given: skipping the TORCS-connection check (engine startup is still checked)"
fi

cat > "$SANDBOX/design.xml" <<EOF
<?xml version="1.0" encoding="UTF-8"?><hyundaiDesignTool>
    <ETHs>
        <ETH IP="$torcs_ip">
            <link rid="CAR" type="CAR"/>
        </ETH>
    </ETHs>
    <CAR IP="CAR">
        <link rid="$torcs_ip" type="ETH"/>
    </CAR>
    <SWCs>
        <SWC BCET="40" IP="SWC0" Virtual="1" WCET="40" consumer="" deadline="100" period="100" phase="0" readCon="1" writeCon="1">
            <impl path="$SANDBOX/sharedObjectFiles/CC/CC.cpp" type="MANUAL"/>
            <link idxCore="0" rid="ECU0" type="ECU"/>
        </SWC>
        <SWC BCET="40" IP="SWC1" Virtual="1" WCET="40" consumer="" deadline="100" period="100" phase="0" readCon="1" writeCon="1">
            <impl path="$SANDBOX/sharedObjectFiles/LK/LK.cpp" type="MANUAL"/>
            <link idxCore="0" rid="ECU0" type="ECU"/>
        </SWC>
    </SWCs>
    <ECUs>
        <ECU IP="ECU0" Virtual="1" numCores="1" schedPolicy="RM" sysClock="100" targetArch="SPC56EL70">
            <link idxCore="0" rid="SWC1" type="SWC"/>
            <link idxCore="0" rid="SWC0" type="SWC"/>
        </ECU>
    </ECUs>
</hyundaiDesignTool>
EOF

# 5) Run the engine
log "Running the engine..."
CPSIM_PATH="$SANDBOX" timeout 15 "$ENGINE_BIN" > "$SANDBOX/engine.log" 2>&1
engine_exit=$?

# 6) Check the output
# A process killed by a signal (SIGSEGV, SIGABRT, ...) exits with 128+signal and
# often prints nothing at all to stdout/stderr, so the exit code must be checked
# too; grepping the log alone is not enough. 124 is timeout's own "ran out of
# time" exit code, which is fine here (expected when nothing answers on TORCS_PORT).
crash_pattern='terminate called|core dumped|Segmentation fault|std::out_of_range|std::.*exception|Aborted'
if [ "$engine_exit" -ge 128 ] && [ "$engine_exit" -ne 124 ] || grep -qE "$crash_pattern" "$SANDBOX/engine.log"; then
	log "--- engine output (exit code $engine_exit) ---"
	cat "$SANDBOX/engine.log" >&2
	fail "the engine crashed (see output above)"
fi

loaded_count=$(grep -c "Successfully Loaded" "$SANDBOX/engine.log")
[ "$loaded_count" -eq 2 ] || { cat "$SANDBOX/engine.log" >&2; fail "expected 2 SWCs to load, got $loaded_count"; }
log "Both SWCs compiled and loaded"

grep -q "YOUR CPU MAX PERFORMANCE" "$SANDBOX/engine.log" \
	|| { cat "$SANDBOX/engine.log" >&2; fail "the engine did not report CPU performance"; }
log "CPU performance check OK"

if [ "$expect_connection" = true ]; then
	if [ -s "$SANDBOX/Log/scheduling.log" ]; then
		log "Connected to $torcs_ip:$TORCS_PORT and started writing Log/scheduling.log"
	else
		cat "$SANDBOX/engine.log" >&2
		fail "the engine did not connect to $torcs_ip:$TORCS_PORT or never wrote Log/scheduling.log"
	fi
else
	grep -q "CONNECTION ERROR" "$SANDBOX/engine.log" \
		|| log "Note: engine exited without the usual CONNECTION ERROR message; check engine.log manually if this is unexpected"
fi

log "PASS: the engine builds, loads its SWCs, and runs without crashing."
exit 0
