#!/usr/bin/env python3
"""Watch the CPSim <-> TORCS traffic on the Ethernet link and decode it.

Usage:  sudo python3 tools/torcs_monitor.py [interface] [seconds] [csv_out]
        defaults: enp56s0, run until Ctrl-C, no CSV

TORCS -> CPSim packets are 6 little-endian int32 (see Engine/EthernetClient.cpp):
  [0] CC_TRIGGER  [1] LK read2  [2] SPEED  [3] LK read1  [4] TARGET_SPEED  [5] ACCEL_VALUE
CPSim -> TORCS packets are 4 int32 (see Engine/Task.cpp Task::run):
  [0] ACCEL  [1] BRAKE  [2] LK write4  [3] LK write3
TCP may merge or split packets, so payloads are reassembled per direction.
"""
import struct
import subprocess
import sys
import time

PORT = 13380
iface = sys.argv[1] if len(sys.argv) > 1 else "enp56s0"
duration = float(sys.argv[2]) if len(sys.argv) > 2 else 0
csv_path = sys.argv[3] if len(sys.argv) > 3 else None

proc = subprocess.Popen(
    ["tcpdump", "-i", iface, "-U", "-w", "-", "-s", "0", f"tcp port {PORT}"],
    stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
f = proc.stdout


def read(n):
    data = b""
    while len(data) < n:
        chunk = f.read(n - len(data))
        if not chunk:
            raise EOFError
        data += chunk
    return data


read(24)  # pcap global header
buffers = {"in": b"", "out": b""}
last_print = 0.0
start = time.time()
csv = open(csv_path, "w") if csv_path else None
if csv:
    csv.write("t,dir,v0,v1,v2,v3,v4,v5\n")
latest = {"in": None, "out": None}
counts = {"in": 0, "out": 0}

try:
    while True:
        ts_sec, ts_usec, incl, _ = struct.unpack("<IIII", read(16))
        pkt = read(incl)
        if duration and time.time() - start > duration:
            break
        # Ethernet(14) + IPv4 + TCP
        if len(pkt) < 34 or pkt[12:14] != b"\x08\x00":
            continue
        ihl = (pkt[14] & 0x0F) * 4
        tcp = 14 + ihl
        sport, dport = struct.unpack(">HH", pkt[tcp:tcp + 4])
        doff = (pkt[tcp + 12] >> 4) * 4
        payload = pkt[tcp + doff:]
        if not payload:
            continue
        direction = "in" if sport == PORT else "out"
        size = 24 if direction == "in" else 16
        buffers[direction] += payload
        t = ts_sec + ts_usec / 1e6
        while len(buffers[direction]) >= size:
            chunk, buffers[direction] = buffers[direction][:size], buffers[direction][size:]
            values = struct.unpack("<%di" % (size // 4), chunk)
            latest[direction] = values
            counts[direction] += 1
            if csv:
                csv.write("%.3f,%s,%s\n" % (t, direction, ",".join(map(str, values))))
        now = time.time()
        if now - last_print >= 0.5:
            last_print = now
            i, o = latest["in"], latest["out"]
            line = "[%6.1fs] in=%d out=%d" % (now - start, counts["in"], counts["out"])
            if i:
                line += " | CC_TRIG=%d SPEED=%d TARGET=%d ACCEL_VAL=%d LK1=%d LK2=%d" % (i[0], i[2], i[4], i[5], i[3], i[1])
            if o:
                line += " | ->ACCEL=%d BRAKE=%d W4=%d W3=%d" % o
            print(line, flush=True)
except (EOFError, KeyboardInterrupt):
    pass
finally:
    proc.terminate()
    if csv:
        csv.close()
