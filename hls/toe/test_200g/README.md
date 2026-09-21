# TOE 1024-bit verification

This standalone flow targets **1024 bits at 250 MHz (4 ns)** on
`xcu55c-fsvh2892-2L-e`. It does not change the system CMake, MAC, NVMe, or
memory-controller configuration. Width is specified in **bytes**: 64 or 128.
MSS=8960, RX_DDR_BYPASS=1, TCP_NODELAY=1, WINDOW_SCALE=1, MAX_SESSIONS=1000.

## Running

From this directory, with Vitis HLS and Vivado 2024.2 installed:

```sh
./run.sh 64 csim
./run.sh 128 csim
./run.sh 64 synth
./run.sh 128 synth
./run.sh 64 xsim functional
./run.sh 128 xsim functional
./run.sh 128 xsim rx
./run.sh 128 xsim tx
./run.sh 128 xsim duplex
./run.sh 128 xsim flow
./run.sh 128 xsim full
TOE_SESSIONS=1 ./run.sh 128 xsim duplex
```

Override `TOE_HLS_BIN` and `TOE_VIVADO_BIN` to use another installation.
Run C-sim again after source changes; a failed C-sim removes its pass marker.
Synthesis requires a passing C-sim marker. No `cosim_design` is called.
The SV testbench instantiates the actual HLS-generated `toe` RTL, with its
actual port widths. `generate_xsim.py` only generates declarations, connections,
AXIS queue plumbing and assertions; behavior is in `tb_xsim.svh`.
Generated RAM initialization `.dat` files are copied into the simulation directory.
XSim needs its local simulator IPC facilities; a restrictive sandbox may prevent
startup even when compilation/elaboration succeeds.

Build products and logs are ignored under `work_64`, `work_128`, and `xsim_*`.
Each mode/session-count pair has a separate log (`duplex_8sessions.log`, etc.). A PASS marker is required, since XSim
can exit successfully after an SV `$fatal`. Do not run two modes concurrently
in the same simulation directory.

## Checks

C-sim calls the current AXI `toe` top. The old `toe_tb.cpp` is left unchanged.
Independent byte-based packet and TCP checksum checks cover odd and full beat
lengths, 64/128-byte boundaries, MSS +/- 1, SYN MSS/window-scale options,
RX/TX payload equality, TX ring wrap, and FIN. The actual memory packet pusher
is additionally exercised at every beat offset; length/mask/alignment helpers
are checked exhaustively for the selected width.

XSim uses a TCP peer, session lookup/update model, byte-addressed TX memory with
command/status checking, an 8192-entry RX bypass FIFO, and application drivers.
TX data is supplied only after a successful request response; insufficient
window responses are retried. The application checks byte sequence and payload;
the peer checks TCP checksum and transmitted payload, including retransmissions.
The TOE's IPv4 checksum is intentionally zero: the downstream MAC encoder
supplies it. Invalid bytes outside TKEEP are ignored.

Functional testing includes randomized output backpressure, zero-window and
reopen, duplicate ACKs inducing retransmission, small lengths, MSS boundaries,
and more than 256 KiB of traffic to cross the TX session ring boundary.
AXIS stability assertions cover data and metadata interfaces.

Performance modes use native-width traffic, eight sessions, 10 us (2500 clocks)
feedback delay in each tested TCP direction, 160 us of traffic warmup, then
250000 clocks / 1 ms of measurement. Count only delivered RX bytes and unique
TX payload bytes; TCP/IP headers, ACKs and retransmissions are not payload.
`rx`, `tx`, and `duplex` each require >= 25,000,000 bytes per active direction
in the measured millisecond (200 Gbit/s). `TOE_SESSIONS=1` records a separate
single-session benchmark without imposing the eight-session throughput gate. The behavioral memory is deliberately
not a model of any particular HBM/DDR controller.

The `flow` mode pauses the receiving application to exercise occupancy-derived
zero-window advertisement and reopening after drain. The `full` stress mode
ignores the advertised window to reach the existing 375-entry RX safety guard,
then retransmits dropped data and checks complete recovery and FIFO drain.
The guard prevents reaching physical FIFO full; it is deliberately preserved.
Neither stress mode imposes the 200 Gbit/s gate during the pause.

HLS clock estimates, XSim's 4 ns clock, and physical post-route timing are
separate results. No physical place-and-route is performed by this flow.
Production timer constants and congestion/window policy are unchanged; the
legacy timer clock assumption is not certified by these tests.

개별 추가 TCP/backpressure TB의 파일 목록과 실행 방법은 [CASES.md](CASES.md),
추가 검증 결과는 [EXTENDED_RESULTS.md](EXTENDED_RESULTS.md)에 정리한다.
