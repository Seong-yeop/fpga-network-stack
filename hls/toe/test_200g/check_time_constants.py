#!/usr/bin/env python3
import argparse
import os
import re
import subprocess
import sys
import tempfile
from fractions import Fraction
from pathlib import Path

HERE = Path(__file__).resolve().parent
UNITS = {"us": 1, "ms": 1000, "s": 1000000}
DEFAULT_CONFIGS = ["256:0.004", "256:0.004:40", "256:0.004:1000", "1000:0.0064"]


def find_include():
    roots = [os.environ.get("XILINX_HLS", ""), os.environ.get("XILINX_VITIS", ""),
             "/opt/xilinx/Vitis/2024.2", "/opt/Xilinx/Vitis/2024.2",
             "/opt/xilinx/Vitis_HLS/2024.2", "/opt/Xilinx/Vitis_HLS/2024.2"]
    for root in roots:
        if root and (Path(root) / "include" / "ap_int.h").is_file():
            return str(Path(root) / "include")
    raise SystemExit("ap_int.h not found; give --include")


def synthesis_branch(text):
    lines = text.splitlines()
    start = next((i for i, l in enumerate(lines) if l.strip() == "#ifndef __SYNTHESIS__"), None)
    if start is None:
        raise SystemExit("#ifndef __SYNTHESIS__ not found")
    depth = 0
    other = None
    for i in range(start, len(lines)):
        s = lines[i].strip()
        if s.startswith("#if"):
            depth += 1
        elif s.startswith("#endif"):
            depth -= 1
            if depth == 0:
                if other is None:
                    raise SystemExit("synthesis branch has no #else")
                return "\n".join(lines[other + 1:i])
        elif s.startswith("#else") and depth == 1:
            other = i
    raise SystemExit("unterminated #ifndef __SYNTHESIS__")


def duration_us(name, rto_us):
    if name == "TIME_RTO_BASE":
        return Fraction(rto_us) if rto_us else Fraction(1000000)
    if name == "TIME_RTO_MAX" or (name == "TIME_60s" and rto_us):
        return Fraction(rto_us) * 60 if rto_us else Fraction(60000000)
    m = re.fullmatch(r"TIME_(\d+)(us|ms|s)", name)
    if not m:
        return None
    return Fraction(int(m.group(1)) * UNITS[m.group(2)])


def evaluate(block, names, sessions, period, rto_us, include, cxx, work):
    src = ["#include <ap_int.h>", "#include <cstdint>", "#include <cstdio>",
           f"static const uint16_t MAX_SESSIONS = {sessions};",
           f"#define TOE_CLOCK_PERIOD_US {period}"]
    if rto_us:
        src.append(f"#define V80_RTO_TEST_US {rto_us}")
    src.append(block)
    src.append("int main() {")
    src += [f' printf("{n} %llu\\n", (unsigned long long){n}.to_uint64());' for n in names]
    src += [" return 0;", "}"]
    cpp = Path(work) / "time_constants.cpp"
    exe = Path(work) / "time_constants"
    cpp.write_text("\n".join(src) + "\n")
    subprocess.run([cxx, "-std=c++14", "-I", include, "-o", str(exe), str(cpp)], check=True)
    out = subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout
    return {k: int(v) for k, v in (line.split() for line in out.splitlines())}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--header", default=str(HERE.parent / "toe_internals.hpp"))
    ap.add_argument("--include", default=None)
    ap.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    ap.add_argument("--config", action="append", help="sessions:period_us[:rto_test_us]")
    args = ap.parse_args()
    include = args.include or find_include()
    block = synthesis_branch(Path(args.header).read_text())
    names = list(dict.fromkeys(re.findall(r"static\s+const\s+ap_uint<\d+>\s+(TIME_\w+)", block)))
    failed = 0
    with tempfile.TemporaryDirectory() as work:
        for config in args.config or DEFAULT_CONFIGS:
            parts = config.split(":")
            sessions, period = int(parts[0]), parts[1]
            rto_us = int(parts[2]) if len(parts) > 2 else None
            values = evaluate(block, names, sessions, period, rto_us, include, args.cxx, work)
            for name in names:
                us = duration_us(name, rto_us)
                if us is None:
                    continue
                exact = us / Fraction(period) / sessions
                lo = exact.numerator // exact.denominator
                hi = lo + 1
                v = values[name]
                ok = lo <= v <= hi
                failed += not ok
                print(f"{'PASS' if ok else 'FAIL'} {config} {name} value={v} (0x{v:08X}) "
                      f"expected={lo}..{hi} seconds={float(v * sessions * Fraction(period)) / 1e6:.6g} "
                      f"expected_seconds={float(us) / 1e6:.6g}")
    print(f"check_time_constants: {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
